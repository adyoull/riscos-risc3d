/* A stand-in for RISC OS's swis.h on the build machine: RISC3D's three
   SWIs done straight to the Linux helper, to test the library and helper
   without RISC OS. Copyright (c) 2026 Andrew Youll. MIT licence. */
#include <stdarg.h>
#include <stddef.h>
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
static int host_sock;
static _kernel_oserror host_err;
static _kernel_oserror *
_swix (int swi, int flags, ...)
{
  va_list ap;
  (void) flags;
  va_start (ap, flags);
  if (swi == 0x5AF82)
    {
      (void) va_arg (ap, int);
      int *h = va_arg (ap, int *);
      void **area = va_arg (ap, void **);
      unsigned *bytes = va_arg (ap, unsigned *);
      struct sockaddr_un a = { .sun_family = AF_UNIX };
      memcpy (a.sun_path + 1, "rol-3d", 6);
      host_sock = socket (AF_UNIX, SOCK_STREAM, 0);
      strcpy (host_err.errmess, "no helper");
      if (connect (host_sock, (struct sockaddr *) &a, offsetof (struct sockaddr_un, sun_path) + 7))
	return &host_err;
      char rep[240];
      union { char buf[CMSG_SPACE (sizeof (int))]; struct cmsghdr align; } ctl;
      struct iovec iov = { rep, sizeof rep };
      struct msghdr m = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = ctl.buf, .msg_controllen = sizeof ctl.buf };
      if (recvmsg (host_sock, &m, 0) < 3 || rep[0] != 'O')
	{
	  strcpy (host_err.errmess, rep);
	  return &host_err;
	}
      int fd;
      memcpy (&fd, CMSG_DATA (CMSG_FIRSTHDR (&m)), sizeof fd);
      *bytes = (unsigned) atoi (rep + 3);
      *area = mmap (0, *bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      *h = 1;
    }
  else if (swi == 0x5AF83)
    close (host_sock);
  else if (swi == 0x23)
    {
      /* OS_ReadVarVal, R2 < 0: does the variable exist (R2 0 if not) */
      const char *name = va_arg (ap, const char *);
      (void) va_arg (ap, void *); (void) va_arg (ap, int); (void) va_arg (ap, int); (void) va_arg (ap, int);
      int *len = va_arg (ap, int *);
      const char *v = getenv (name);
      *len = v ? ~(int) strlen (v) : 0;
    }
  else if (swi == 0x5AF80)
    {
      /* RISC3D_Info, as RISC3D 0.03 with Linux3D 0.02: r3, r4 (software
	 from the helper's "rol-3d-info") */
      const char **drv = va_arg (ap, const char **);
      unsigned *fl = va_arg (ap, unsigned *);
      *drv = "Linux3D (host)";
      *fl = 0;
      struct sockaddr_un a = { .sun_family = AF_UNIX };
      memcpy (a.sun_path + 1, "rol-3d-info", 11);
      int q = socket (AF_UNIX, SOCK_STREAM, 0);
      char c = 0;
      if (q >= 0 && !connect (q, (struct sockaddr *) &a, offsetof (struct sockaddr_un, sun_path) + 12) && read (q, &c, 1) == 1
	  && c == 's')
	*fl = 2;
      if (q >= 0)
	close (q);
    }
  else if (swi == 0x5AF84)
    {
      (void) va_arg (ap, int);
      int wait = va_arg (ap, int);
      char c = wait ? 'S' : 'W';
      if (write (host_sock, &c, 1) != 1 || (wait && read (host_sock, &c, 1) != 1))
	{
	  strcpy (host_err.errmess, "the helper went away");
	  return &host_err;
	}
    }
  va_end (ap);
  return 0;
}
