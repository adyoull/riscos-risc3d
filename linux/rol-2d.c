/* rol-2d: Linux2D's helper (prototype). RISC OS's desktop asks its display
   driver to copy and fill rectangles of the screen (GraphicsV 13); the
   Linux2D module sends the large ones here, where they are done by native
   code on the same memory: RISC OS on Linux keeps its screen (dynamic
   area 2) in a file, so Linux2D passes that file's descriptor and this
   maps it. Under QEMU (no 32-bit ARM on the host: Apple Silicon, the Pi 5
   without AArch32...) RISC OS's own copy is emulated, and a native one is
   several times quicker.

   Protocol, on the abstract SOCK_SEQPACKET socket "rol-2d", one client at
   a time:
     SCREEN  { 'R2D1', 0, size, 0... } + the screen's file (SCM_RIGHTS)
	     -> int32 0, or -errno
     COPY    { 1, src, dst, row_bytes, rows, stride, 0, 0 }
     FILL    { 2, 0, dst, row_bytes, rows, stride, pattern, 0 }
	     -> int32 0, or -errno (offsets in bytes into the file; the
	     rows go down the screen from the offset, stride bytes apart; a
	     copy may overlap, as a scroll does)

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#define _GNU_SOURCE
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define ROL2D_VERSION "0.01"
#define MAGIC 0x31443252u		/* 'R2D1' */

static uint8_t *screen;
static size_t screen_size;

static int
map_screen (int fd, size_t size)
{
  struct stat st;
  if (fstat (fd, &st))
    return -errno;
  if ((size_t) st.st_size < size)
    size = (size_t) st.st_size;	/* (the area's current size) */
  void *p = mmap (0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED)
    return -errno;
  if (screen)
    munmap (screen, screen_size);
  screen = p;
  screen_size = size;
  return 0;
}

/* within the mapping: rows of row_bytes from off, stride apart */
static int
fits (uint32_t off, uint32_t row_bytes, uint32_t rows, uint32_t stride)
{
  if (!screen || !rows || !row_bytes || row_bytes > stride)
    return 0;
  uint64_t end = (uint64_t) off + (uint64_t) (rows - 1) * stride + row_bytes;
  return end <= screen_size;
}

static int
do_copy (const uint32_t *m)
{
  uint32_t src = m[1], dst = m[2], rb = m[3], rows = m[4], stride = m[5];
  if (!fits (src, rb, rows, stride) || !fits (dst, rb, rows, stride))
    return -EINVAL;
  /* rows in the order that never reads a row already written: moving
     down the screen (dst after src), from the bottom row up */
  if (dst > src)
    for (uint32_t r = rows; r-- > 0;)
      memmove (screen + dst + (size_t) r * stride, screen + src + (size_t) r * stride, rb);
  else
    for (uint32_t r = 0; r < rows; r++)
      memmove (screen + dst + (size_t) r * stride, screen + src + (size_t) r * stride, rb);
  return 0;
}

/* the pattern is a word as the screen holds it at a word boundary (the
   pixel repeated: 8, 16 or 32 bits); a row may start inside a word */
static int
do_fill (const uint32_t *m)
{
  uint32_t dst = m[2], rb = m[3], rows = m[4], stride = m[5], pat = m[6];
  if (!fits (dst, rb, rows, stride))
    return -EINVAL;
  uint8_t b[4];
  memcpy (b, &pat, 4);
  int bytes_same = b[0] == b[1] && b[1] == b[2] && b[2] == b[3];
  uint64_t pat64 = (uint64_t) pat << 32 | pat;
  for (uint32_t r = 0; r < rows; r++)
    {
      uint8_t *p = screen + dst + (size_t) r * stride;
      if (bytes_same)
	{
	  memset (p, b[0], rb);
	  continue;
	}
      uint32_t n = rb;
      while (n && ((uintptr_t) p & 7))
	{
	  *p = b[(uintptr_t) p & 3];
	  p++, n--;
	}
      uint64_t *q = (uint64_t *) p;
      for (; n >= 8; n -= 8)
	*q++ = pat64;
      p = (uint8_t *) q;
      while (n)
	{
	  *p = b[(uintptr_t) p & 3];
	  p++, n--;
	}
    }
  return 0;
}

static void
serve (int c)
{
  for (;;)
    {
      uint32_t m[8];
      union { char buf[CMSG_SPACE (sizeof (int))]; struct cmsghdr align; } ctl;
      struct iovec iov = { m, sizeof m };
      struct msghdr h = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = ctl.buf, .msg_controllen = sizeof ctl.buf };
      ssize_t n = recvmsg (c, &h, MSG_CMSG_CLOEXEC);
      if (n <= 0)
	{
	  if (n < 0 && errno == EINTR)
	    continue;
	  return;
	}
      int32_t ret;
      if (n != sizeof m)
	ret = -EINVAL;
      else if (m[0] == MAGIC)
	{
	  struct cmsghdr *cm = CMSG_FIRSTHDR (&h);
	  int fd = -1;
	  if (cm && cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS)
	    memcpy (&fd, CMSG_DATA (cm), sizeof fd);
	  ret = fd < 0 ? -EBADF : map_screen (fd, m[2]);
	  if (fd >= 0)
	    close (fd);
	  fprintf (stderr, "rol-2d: screen of %zu bytes: %s\n", screen_size, ret ? strerror (-ret) : "mapped");
	}
      else if (m[0] == 1)
	ret = do_copy (m);
      else if (m[0] == 2)
	ret = do_fill (m);
      else
	ret = -EINVAL;
      if (send (c, &ret, sizeof ret, MSG_NOSIGNAL) != sizeof ret)
	return;
    }
}

int
main (int argc, char **argv)
{
  if (argc > 1 && !strcmp (argv[1], "--version"))
    {
      printf ("rol-2d %s\n", ROL2D_VERSION);
      return 0;
    }
  int l = socket (AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  struct sockaddr_un a = { .sun_family = AF_UNIX };
  memcpy (a.sun_path + 1, "rol-2d", 6);
  if (l < 0 || bind (l, (struct sockaddr *) &a, offsetof (struct sockaddr_un, sun_path) + 7) || listen (l, 2))
    {
      perror ("rol-2d: socket");
      return 1;
    }
  fprintf (stderr, "rol-2d %s: waiting for Linux2D\n", ROL2D_VERSION);
  for (;;)
    {
      int c = accept4 (l, 0, 0, SOCK_CLOEXEC);
      if (c < 0)
	continue;
      serve (c);
      close (c);
      if (screen)
	munmap (screen, screen_size);
      screen = 0;
      screen_size = 0;
    }
}
