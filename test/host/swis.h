/* A stand-in for RISC OS's swis.h on the build machine: RISC3D's SWIs done
   straight to the Linux helper (as RISC3D 0.06 with Linux3D 0.06 would),
   to test the library and helper without RISC OS. Several contexts, and
   (0.12) contexts sharing objects: opened on the first one's process's
   own socket, "rol-3d-<pid>".
   Copyright (c) 2026 Andrew Youll. MIT licence. */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
typedef struct { int errnum; char errmess[252]; } _kernel_oserror;
#define _IN(n) 0
#define _INR(a, b) 0
#define _OUT(n) 0
#define _OUTR(a, b) 0
#define HOST_MAX 32
static struct { int sock, pid; } host_ctx[HOST_MAX + 1];	/* by handle, from 1 */
static __thread _kernel_oserror host_err;

static _kernel_oserror *
host_fail (const char *why)
{
  snprintf (host_err.errmess, sizeof host_err.errmess, "%s", why);
  return &host_err;
}

static _kernel_oserror *
_swix (int swi, int flags, ...)
{
  va_list ap;
  (void) flags;
  va_start (ap, flags);
  _kernel_oserror *e = 0;
  if (swi == 0x5AF82)
    {
      /* RISC3D_Open: r0 flags (bit 0: share with r1) */
      int fl = va_arg (ap, int);
      int share = fl & 1 ? va_arg (ap, int) : 0;
      int *h = va_arg (ap, int *);
      void **area = va_arg (ap, void **);
      unsigned *bytes = va_arg (ap, unsigned *);
      int i = 1;
      while (i <= HOST_MAX && host_ctx[i].sock > 0)
	i++;
      char name[32] = "rol-3d";
      if (fl & 1)
	{
	  if (share < 1 || share > HOST_MAX || host_ctx[share].sock <= 0)
	    {
	      va_end (ap);
	      return host_fail ("Not a RISC3D context");
	    }
	  if (!host_ctx[share].pid)
	    {
	      va_end (ap);
	      return host_fail ("Linux's rol-3d is too old to share objects between contexts");
	    }
	  snprintf (name, sizeof name, "rol-3d-%d", host_ctx[share].pid);
	}
      if (i > HOST_MAX)
	{
	  va_end (ap);
	  return host_fail ("Too many 3D contexts");
	}
      struct sockaddr_un a = { .sun_family = AF_UNIX };
      size_t len = strlen (name);
      memcpy (a.sun_path + 1, name, len);
      int s = socket (AF_UNIX, SOCK_STREAM, 0);
      if (connect (s, (struct sockaddr *) &a, (socklen_t) (offsetof (struct sockaddr_un, sun_path) + 1 + len)))
	{
	  close (s);
	  va_end (ap);
	  return host_fail ("no helper");
	}
      char rep[240] = "";
      union { char buf[CMSG_SPACE (sizeof (int))]; struct cmsghdr align; } ctl;
      struct iovec iov = { rep, sizeof rep - 1 };
      struct msghdr m = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = ctl.buf, .msg_controllen = sizeof ctl.buf };
      if (recvmsg (s, &m, 0) < 3 || rep[0] != 'O' || !CMSG_FIRSTHDR (&m))
	{
	  close (s);
	  va_end (ap);
	  return host_fail (rep[0] == 'E' ? rep + 4 : "the helper gave nothing");
	}
      int fd;
      memcpy (&fd, CMSG_DATA (CMSG_FIRSTHDR (&m)), sizeof fd);
      unsigned b = 0;
      int pid = 0;
      sscanf (rep + 3, "%u %d", &b, &pid);
      *bytes = b;
      *area = mmap (0, b, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      close (fd);
      host_ctx[i].sock = s;
      host_ctx[i].pid = pid;
      *h = i;
    }
  else if (swi == 0x5AF83)
    {
      int h = va_arg (ap, int);
      if (h >= 1 && h <= HOST_MAX && host_ctx[h].sock > 0)
	{
	  close (host_ctx[h].sock);
	  host_ctx[h].sock = 0;
	}
    }
  else if (swi == 0x23)
    {
      /* OS_ReadVarVal, as RISC OS has it (0.13: the old stand-in gave no
	 error for R2 < 0, so the client's mistake there went unseen):
	 not found: error &124, R2 = 0; R2 < 0 and found: error (buffer
	 overflow), R2 = NOT length; a buffer: the value, R2 = its length,
	 or error &1E4 if it doesn't fit */
      static _kernel_oserror notfound = { 0x124, "System variable not found" };
      static _kernel_oserror overflow = { 0x1E4, "Buffer overflow" };
      const char *name = va_arg (ap, const char *);
      char *buf = va_arg (ap, char *);
      int size = va_arg (ap, int);
      (void) va_arg (ap, int); (void) va_arg (ap, int);
      int *len = va_arg (ap, int *);
      const char *v = getenv (name);
      if (!v)
	{
	  *len = 0;
	  e = &notfound;
	}
      else if (size < 0 || (int) strlen (v) > size)
	{
	  *len = ~(int) strlen (v);
	  e = &overflow;
	}
      else
	{
	  memcpy (buf, v, strlen (v));
	  *len = (int) strlen (v);
	}
    }
  else if (swi == 0x5AF80)
    {
      /* RISC3D_Info, as RISC3D 0.06 with Linux3D 0.06: r3, r4 (software,
	 sharing: from the helper's "rol-3d-info") */
      const char **drv = va_arg (ap, const char **);
      unsigned *fl = va_arg (ap, unsigned *);
      *drv = "Linux3D (host)";
      *fl = 0;
      struct sockaddr_un a = { .sun_family = AF_UNIX };
      memcpy (a.sun_path + 1, "rol-3d-info", 11);
      int q = socket (AF_UNIX, SOCK_STREAM, 0);
      char b[256] = "";
      size_t got = 0;
      if (q >= 0 && !connect (q, (struct sockaddr *) &a, offsetof (struct sockaddr_un, sun_path) + 12))
	{
	  ssize_t n;
	  while (got < sizeof b - 1 && (n = read (q, b + got, sizeof b - 1 - got)) > 0)
	    got += (size_t) n;
	}
      b[got] = 0;
      if (b[0] == 's')
	*fl |= 2;
      if (strstr (b, ";share"))
	*fl |= 4;
      if (q >= 0)
	close (q);
    }
  else if (swi == 0x5AF84)
    {
      int h = va_arg (ap, int);
      int wait = va_arg (ap, int);
      char c = wait ? 'S' : 'W';
      int s = h >= 1 && h <= HOST_MAX ? host_ctx[h].sock : 0;
      if (s <= 0 || write (s, &c, 1) != 1 || (wait && read (s, &c, 1) != 1))
	e = host_fail ("the helper went away");
    }
  va_end (ap);
  return e;
}
