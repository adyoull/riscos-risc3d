/* Linux3D: the RISC3D driver for RISC OS on Linux.

   Each RISC3D context is a connection to the image's rol-3d helper (the
   abstract socket "rol-3d"), which makes a renderer for it on Linux's
   graphics driver and sends back the memory the two share: Linux3D maps
   it into RISC OS, and Mesa's front end writes its commands there. After
   that this module is only called to wake the renderer (one byte down the
   socket) or to wait for it (a byte down, a byte back).

   0.02: registered as a driver that may draw in software (flags bit 1),
   RISC3D asks it which (entry reason 3): rol-3d 0.7 on answers on the
   abstract socket "rol-3d-info" with "GPU ..." or "software ...". An
   older rol-3d has no such socket: then it says GPU, as before.

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */

#include "cmodule.h"

#define IXSupport_LinuxSyscall	0x59EC0
#define RISC3D_Register		0xC3D01		/* provisional */
#define NR_read		3
#define NR_write	4
#define NR_close	6
#define NR_access	33
#define NR_munmap	91
#define NR_mmap2	192
#define NR_socket	281
#define NR_connect	283
#define NR_recvmsg	297
#define O_CLOEXEC	02000000
#define MSG_CMSG_CLOEXEC 0x40000000
#define MAXCTX 32

void driver_entry (void);

static struct { int used, sock; unsigned area, bytes, wakes, waits; } ctx[MAXCTX];
static int registered;

ERROR_BLOCK (err_nohelper, 0x81D310, "Linux's 3D helper isn't running (rol-3d)");
ERROR_BLOCK (err_gone, 0x81D311, "Linux's 3D renderer for this context has gone");
ERROR_BLOCK (err_full, 0x81D312, "Too many Linux3D contexts");
ERROR_BLOCK (err_map, 0x81D313, "Linux3D couldn't map the renderer's memory");
ERROR_BLOCK (err_notrol, 0x81D315, "Linux3D is the 3D driver for RISC OS on Linux only");
ERROR_BLOCK (err_norol3d, 0x81D316, "Linux3D needs Linux's riscos-risc3d package (rol-3d), which isn't installed");
static struct { int errnum; char errmess[240]; } err_helper = { 0x81D314, "" };

static int
lsys (int nr, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e, unsigned f)
{
  regs r = {{ a, b, c, d, e, f, 0, (unsigned) nr }};
  if (swix_regs (IXSupport_LinuxSyscall, &r))
    return -38;
  return (int) r.r[0];
}

/* one byte each way; interrupted calls (RISC OS's timer) go again */
static int
put_byte (int s, char c)
{
  for (;;)
    {
      int n = lsys (NR_write, (unsigned) s, (unsigned) &c, 1, 0, 0, 0);
      if (n == 1)
	return 1;
      if (n != -4 /* EINTR */ && n != -11 /* EAGAIN */)
	return 0;
    }
}

static int
get_byte (int s)
{
  char c;
  for (;;)
    {
      int n = lsys (NR_read, (unsigned) s, (unsigned) &c, 1, 0, 0, 0);
      if (n == 1)
	return 1;
      if (n != -4 && n != -11)
	return 0;
    }
}

static oserror *
ctx_open (regs *r)
{
  int i;
  for (i = 0; i < MAXCTX && ctx[i].used; i++)
    ;
  if (i == MAXCTX)
    return ERR (err_full);
  int s = lsys (NR_socket, 1 /* AF_UNIX */, 1 /* SOCK_STREAM */ | O_CLOEXEC, 0, 0, 0, 0);
  static const char addr[] = "\1\0\0rol-3d";		/* abstract name */
  if (s < 0 || lsys (NR_connect, (unsigned) s, (unsigned) addr, 2 + 7, 0, 0, 0) != 0)
    {
      if (s >= 0)
	lsys (NR_close, (unsigned) s, 0, 0, 0, 0, 0);
      return ERR (err_nohelper);
    }
  /* "OK bytes" with the memory's file, or "ERR why" */
  char rep[240];
  struct { unsigned len; int level, type; int fd; } cm = { 16, 0, 0, -1 };
  struct { void *base; unsigned len; } iov = { rep, sizeof rep - 1 };
  struct { void *name; unsigned namelen; void *iov; unsigned iovlen; void *control; unsigned controllen; int flags; }
    mh = { 0, 0, &iov, 1, &cm, 16, 0 };
  memset (rep, 0, sizeof rep);
  int n;
  do
    n = lsys (NR_recvmsg, (unsigned) s, (unsigned) &mh, MSG_CMSG_CLOEXEC, 0, 0, 0);
  while (n == -4);
  int fd = mh.controllen >= 16 && cm.level == 1 && cm.type == 1 ? cm.fd : -1;
  if (n < 3 || rep[0] != 'O' || fd < 0)
    {
      lsys (NR_close, (unsigned) s, 0, 0, 0, 0, 0);
      if (fd >= 0)
	lsys (NR_close, (unsigned) fd, 0, 0, 0, 0, 0);
      if (n > 4 && rep[0] == 'E')
	{
	  memcpy (err_helper.errmess, rep + 4, sizeof err_helper.errmess - 1);
	  return (oserror *) &err_helper;
	}
      return ERR (err_gone);
    }
  unsigned bytes = 0;
  for (const char *p = rep + 3; *p >= '0' && *p <= '9'; p++)
    bytes = bytes * 10 + (unsigned) (*p - '0');
  int a = lsys (NR_mmap2, 0, bytes, 3 /* read, write */, 1 /* MAP_SHARED */, (unsigned) fd, 0);
  lsys (NR_close, (unsigned) fd, 0, 0, 0, 0, 0);
  if ((unsigned) a >= 0xfffff000u || !bytes)
    {
      lsys (NR_close, (unsigned) s, 0, 0, 0, 0, 0);
      return ERR (err_map);
    }
  ctx[i].used = 1;
  ctx[i].sock = s;
  ctx[i].area = (unsigned) a;
  ctx[i].bytes = bytes;
  ctx[i].wakes = ctx[i].waits = 0;
  r->r[0] = (unsigned) i + 1;
  r->r[1] = (unsigned) a;
  r->r[2] = bytes;
  return 0;
}

static void
ctx_close (int i)
{
  lsys (NR_munmap, ctx[i].area, ctx[i].bytes, 0, 0, 0, 0);
  lsys (NR_close, (unsigned) ctx[i].sock, 0, 0, 0, 0, 0);	/* the renderer ends */
  ctx[i].used = 0;
}

/* reason 3: does Linux draw in software? r0 = 1 if so, 0 on a GPU, 2 not
   known (no helper, or one older than 0.7) */
static oserror *
describe (regs *r)
{
  r->r[0] = 2;
  int s = lsys (NR_socket, 1 /* AF_UNIX */, 1 /* SOCK_STREAM */ | O_CLOEXEC, 0, 0, 0, 0);
  static const char addr[] = "\1\0\0rol-3d-info";	/* abstract name */
  if (s < 0)
    return 0;
  if (lsys (NR_connect, (unsigned) s, (unsigned) addr, 2 + 12, 0, 0, 0) == 0)
    {
      char c = 0;
      int n;
      do
	n = lsys (NR_read, (unsigned) s, (unsigned) &c, 1, 0, 0, 0);
      while (n == -4 || n == -11);
      r->r[0] = n != 1 ? 2 : c == 's' ? 1 : c == 'G' ? 0 : 2;
    }
  lsys (NR_close, (unsigned) s, 0, 0, 0, 0, 0);
  return 0;
}

oserror *driver_call (regs *r);
oserror *
driver_call (regs *r)
{
  if (r->r[0] == 0)
    return ctx_open (r);
  if (r->r[0] == 3)
    return describe (r);
  unsigned i = r->r[1] - 1;
  if (i >= MAXCTX || !ctx[i].used)
    return ERR (err_gone);
  if (r->r[0] == 1)
    ctx_close ((int) i);
  else if (r->r[0] == 2)
    {
      if (r->r[2] & 1)
	{
	  ctx[i].waits++;
	  if (!put_byte (ctx[i].sock, 'S') || !get_byte (ctx[i].sock))
	    return ERR (err_gone);
	}
      else
	{
	  ctx[i].wakes++;
	  if (!put_byte (ctx[i].sock, 'W'))
	    return ERR (err_gone);
	}
    }
  return 0;
}

oserror *
mod_init (const char *tail, int instance, void *pw)
{
  (void) tail; (void) instance; (void) pw;
  /* 0.03: refuse to start where this isn't RISC OS on Linux, so that
     RISC3D (which tries each module in !RISC3D.GPU) moves on */
  regs x = {{ 0, (unsigned) "IXSupport_LinuxSyscall" }};
  if (swix_regs (0x39, &x))		/* OS_SWINumberFromString */
    return ERR (err_notrol);
  /* 0.04: and only where Linux's side is installed (riscos-risc3d), else
     programs would be sent to a helper that never comes and lose their
     software drawing (only "not there" refuses; anything else may be a
     call the gateway doesn't allow, so carry on) */
  if (lsys (NR_access, (unsigned) "/opt/riscos/rol-3d", 1 /* X_OK */, 0, 0, 0, 0) == -2)
    return ERR (err_norol3d);
  /* (flags bit 1: may draw in software, ask: reason 3) */
  regs r = {{ 0, (unsigned) driver_entry, 0, (unsigned) "Linux3D (Linux's graphics driver)", 2 }};
  oserror *e = swix_regs (RISC3D_Register, &r);
  if (e)
    return e;				/* RISC3D isn't loaded */
  registered = 1;
  return 0;
}

oserror *
mod_final (int fatal)
{
  (void) fatal;
  if (registered)
    {
      regs r = {{ 1, (unsigned) driver_entry, 0 }};
      swix_regs (RISC3D_Register, &r);		/* (RISC3D closes our contexts) */
    }
  for (int i = 0; i < MAXCTX; i++)
    if (ctx[i].used)
      ctx_close (i);
  return 0;
}

oserror *
mod_command (int index, const char *tail, int argc)
{
  (void) index; (void) tail; (void) argc;
  int n = 0;
  for (int i = 0; i < MAXCTX; i++)
    if (ctx[i].used)
      {
	write0 ("Context ");
	write_dec ((unsigned) i + 1);
	write0 (": memory at &");
	write_hex8 (ctx[i].area);
	write0 (", ");
	write_dec (ctx[i].waits);
	write0 (" waits, ");
	write_dec (ctx[i].wakes);
	write0 (" wakes");
	newline ();
	n++;
      }
  if (!n)
    {
      write0 ("No 3D contexts are open");
      newline ();
    }
  return 0;
}

/* (the module framework's entries this module doesn't use) */
oserror *mod_swi (unsigned number, regs *r) { (void) number; (void) r; return 0; }
void mod_service (regs *r) { (void) r; }
