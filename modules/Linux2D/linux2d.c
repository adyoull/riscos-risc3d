/* Linux2D (prototype): RISC OS on Linux's desktop rectangle copies and
   fills, done by native Linux code.

   The kernel offers its display driver the screen's rectangle copies
   (scrolls, window moves) and fills as GraphicsV 13 (Render), ops 1 and
   2; ROL's driver, SocketKVM, doesn't take them, so the kernel's own code
   does them, which under QEMU (no 32-bit ARM on the host) is emulated.
   Linux2D claims GraphicsV and sends the large ones to the rol-2d helper
   (linux/rol-2d.c): the screen is dynamic area 2, which ROL keeps in a
   file (OS_DynamicArea 29 gives its descriptor), and rol-2d maps the same
   file and does the work there, natively, while Linux2D waits. Small ones
   are left to the kernel (a round trip to rol-2d costs more).

   Only the current driver, head 0; 8, 16 and 32 bits a pixel; solid fills
   (each ECF line's OR all ones and EOR the same). Anything else is passed
   on as before. With no rol-2d, everything is passed on (and Linux2D
   looks again every 5 seconds).

   *Linux2D [on | off | limits <copy bytes> <fill bytes>]

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */

#include "cmodule.h"

#define IXSupport_LinuxSyscall	0x59EC0
#define OS_Claim		0x1F
#define OS_Release		0x20
#define OS_ReadVduVariables	0x31
#define OS_ReadMonotonicTime	0x42
#define GRAPHICSV		42
#define NR_close	6
#define NR_getpid	20
#define NR_socket	281
#define NR_connect	283
#define NR_send		289
#define NR_recv		291
#define NR_sendmsg	296
#define O_CLOEXEC	02000000
#define SOCK_SEQPACKET	5
#define MAGIC		0x31443252u	/* 'R2D1' */
#define DA_ALTER_FILE	29		/* ROL's OS_DynamicArea reason: the area's file */

void gfxv_entry (void);

ERROR_BLOCK (err_notrol, 0x823C08, "Linux2D is for RISC OS on Linux only");
ERROR_BLOCK (err_syntax, 0x823C09, "Syntax: *Linux2D [on | off | limits <copy bytes> <fill bytes>]");

static void *pw;
static int claimed, busy;
static int sock = -1;			/* rol-2d, or -1 */
static int screen_sent;			/* rol-2d has the current screen */
static unsigned retry_at;		/* (no rol-2d: when to look again, cs) */

/* the screen, read again after a mode or bank change */
static struct
{
  int known, why;			/* why: 0 usable, else the reason not */
  unsigned log2bpp, stride, xlim, ylim, off, size;
  int fd;
} scr;

static unsigned lim_copy = 65536, lim_fill = 65536;
static unsigned n_copy, n_fill, n_small, n_other, n_fail, n_pattern, n_offered;
static int last_err;

static int
lsys (int nr, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e, unsigned f)
{
  regs r = {{ a, b, c, d, e, f, 0, (unsigned) nr }};
  if (swix_regs (IXSupport_LinuxSyscall, &r))
    return -38;
  return (int) r.r[0];
}

static unsigned
now_cs (void)
{
  regs r = {{ 0 }};
  swix_regs (OS_ReadMonotonicTime, &r);
  return r.r[0];
}

static void
drop_helper (void)
{
  if (sock >= 0)
    lsys (NR_close, (unsigned) sock, 0, 0, 0, 0, 0);
  sock = -1;
  screen_sent = 0;
  retry_at = now_cs () + 500;
}

/* rol-2d: connected, or -1 (and not tried again for 5 s) */
static int
helper (void)
{
  if (sock >= 0)
    return sock;
  if ((int) (now_cs () - retry_at) < 0)
    return -1;
  static const char addr[] = "\1\0\0rol-2d";
  int s = lsys (NR_socket, 1 /* AF_UNIX */, SOCK_SEQPACKET | O_CLOEXEC, 0, 0, 0, 0);
  if (s < 0 || lsys (NR_connect, (unsigned) s, (unsigned) addr, 2 + 7, 0, 0, 0) != 0)
    {
      if (s >= 0)
	lsys (NR_close, (unsigned) s, 0, 0, 0, 0, 0);
      retry_at = now_cs () + 500;
      return -1;
    }
  sock = s;
  screen_sent = 0;
  return s;
}

/* a message of 8 words, perhaps with a descriptor; rol-2d's answer */
static int
ask (const unsigned *m, int fd)
{
  struct { unsigned base, len; } iov = { (unsigned) m, 32 };
  struct { unsigned len; int level, type, fd; } cm = { 16, 1 /* SOL_SOCKET */, 1 /* SCM_RIGHTS */, fd };
  struct { unsigned name, namelen, iov, iovlen, control, controllen, flags; } h = {
    0, 0, (unsigned) &iov, 1, fd >= 0 ? (unsigned) &cm : 0, fd >= 0 ? 16 : 0, 0 };
  int n;
  do
    n = lsys (NR_sendmsg, (unsigned) sock, (unsigned) &h, 0x4000 /* MSG_NOSIGNAL */, 0, 0, 0);
  while (n == -4);
  if (n != 32)
    return n < 0 ? n : -5;
  int ret = 0;
  do
    n = lsys (NR_recv, (unsigned) sock, (unsigned) &ret, 4, 0, 0, 0);
  while (n == -4 || n == -11);
  if (n != 4)
    return n < 0 ? n : -5;
  return ret;
}

/* --- the screen ------------------------------------------------------ */

enum { WHY_OK, WHY_VDU, WHY_DEPTH, WHY_NOFILE, WHY_GEOMETRY };

static void
forget (void)
{
  scr.known = 0;
  screen_sent = 0;
}

static int
screen (void)
{
  if (scr.known)
    return scr.why;
  scr.known = 1;
  static const int vars[] = { 9, 6, 11, 12, 148, -1 };
  int v[5];
  regs r = {{ (unsigned) vars, (unsigned) v }};
  if (swix_regs (OS_ReadVduVariables, &r))
    return scr.why = WHY_VDU;
  scr.log2bpp = (unsigned) v[0];
  scr.stride = (unsigned) v[1];
  scr.xlim = (unsigned) v[2];
  scr.ylim = (unsigned) v[3];
  if (scr.log2bpp < 3 || scr.log2bpp > 5)
    return scr.why = WHY_DEPTH;
  /* dynamic area 2: its base and size, and its file (ROL) */
  regs d = {{ 2, 2 }};
  if (swix_regs (OS_DynamicArea, &d))
    return scr.why = WHY_NOFILE;
  unsigned base = d.r[3], size = d.r[2];
  regs f = {{ DA_ALTER_FILE, 2, (unsigned) -1 }};
  if (swix_regs (OS_DynamicArea, &f) || (int) f.r[3] < 0)
    return scr.why = WHY_NOFILE;
  scr.fd = (int) f.r[3];
  /* where the VDU draws (ScreenStart), as an offset into the file; the
     area is doubly mapped, so it may be the copy below the base */
  unsigned start = (unsigned) v[4];
  unsigned off = start >= base ? start - base : start + size - base;
  unsigned need = (scr.ylim + 1) * scr.stride;
  if (((scr.xlim + 1) << (scr.log2bpp - 3)) > scr.stride || off > size || need > size - off)
    return scr.why = WHY_GEOMETRY;
  scr.off = off;
  scr.size = size;
  screen_sent = 0;
  return scr.why = WHY_OK;
}

/* rol-2d ready with this screen: 1, else 0 */
static int
ready (void)
{
  if (screen ())
    return 0;
  if (helper () < 0)
    return 0;
  if (!screen_sent)
    {
      unsigned m[8] = { MAGIC, 0, scr.size, 0, 0, 0, 0, 0 };
      int ret = ask (m, scr.fd);
      if (ret)
	{
	  last_err = ret;
	  n_fail++;
	  drop_helper ();
	  return 0;
	}
      screen_sent = 1;
    }
  return 1;
}

/* --- GraphicsV 13 ---------------------------------------------------- */

static int
op_done (const unsigned *m)
{
  int ret = ask (m, -1);
  if (ret == 0)
    return 1;
  last_err = ret;
  n_fail++;
  if (ret != -22)			/* (EINVAL: this one; anything else: rol-2d) */
    drop_helper ();
  return 0;
}

/* op 1: source x, y, destination x, y (bottom left, y up), width - 1,
   height - 1 */
static int
copy (const unsigned *p)
{
  unsigned sx = p[0], sy = p[1], dx = p[2], dy = p[3], w = p[4] + 1, h = p[5] + 1;
  if (!w || !h || w > 0x10000 || h > 0x10000)
    return 0;
  /* the size first, before anything else is read: most are small */
  unsigned bpp_shift = scr.known && !scr.why ? scr.log2bpp - 3 : 2;
  if ((w << bpp_shift) * h < lim_copy)
    {
      n_small++;
      return 0;
    }
  if (!ready ())
    return 0;
  if (sx > scr.xlim || w - 1 > scr.xlim - sx || dx > scr.xlim || w - 1 > scr.xlim - dx
      || sy > scr.ylim || h - 1 > scr.ylim - sy || dy > scr.ylim || h - 1 > scr.ylim - dy)
    return 0;
  unsigned bs = scr.log2bpp - 3;
  if ((w << bs) * h < lim_copy)
    {
      n_small++;
      return 0;
    }
  unsigned st = scr.ylim - (sy + h - 1), dt = scr.ylim - (dy + h - 1);	/* top rows, in memory */
  unsigned m[8] = { 1, scr.off + st * scr.stride + (sx << bs), scr.off + dt * scr.stride + (dx << bs),
		    w << bs, h, scr.stride, 0, 0 };
  if (!op_done (m))
    return 0;
  n_copy++;
  return 1;
}

/* op 2: left, top, right, bottom (inclusive, y up), the ECF: 8 lines of
   OR, EOR words. Solid when every OR is all ones and every EOR the same:
   each pixel becomes NOT EOR */
static int
fill (const unsigned *p)
{
  unsigned left = p[0], top = p[1], right = p[2], bottom = p[3];
  const unsigned *ecf = (const unsigned *) p[4];
  if (right < left || top < bottom || right - left > 0xFFFF || top - bottom > 0xFFFF)
    return 0;
  unsigned w = right - left + 1, h = top - bottom + 1;
  unsigned bpp_shift = scr.known && !scr.why ? scr.log2bpp - 3 : 2;
  if ((w << bpp_shift) * h < lim_fill)
    {
      n_small++;
      return 0;
    }
  unsigned e = ecf[1];
  for (int i = 0; i < 8; i++)
    if (ecf[2 * i] != 0xFFFFFFFFu || ecf[2 * i + 1] != e)
      {
	n_pattern++;
	return 0;
      }
  if (!ready ())
    return 0;
  if (right > scr.xlim || top > scr.ylim)
    return 0;
  unsigned bs = scr.log2bpp - 3;
  unsigned pat = ~e;
  /* every pixel of the word the same (the ECF's words are the pixel
     repeated; a pattern across pixels isn't solid) */
  if ((bs == 1 && (pat >> 16) != (pat & 0xFFFF))
      || (bs == 0 && ((pat ^ (pat >> 8)) & 0x00FFFFFF)))
    {
      n_pattern++;
      return 0;
    }
  if ((w << bs) * h < lim_fill)
    {
      n_small++;
      return 0;
    }
  unsigned t = scr.ylim - top;
  unsigned m[8] = { 2, 0, scr.off + t * scr.stride + (left << bs), w << bs, h, scr.stride, pat, 0 };
  if (!op_done (m))
    return 0;
  n_fill++;
  return 1;
}

/* r[0..9]: the vector's registers. 1: claimed (r[4] = 0). */
int
gfxv_c (unsigned *r)
{
  if ((r[4] >> 16) & 0xFF)		/* head 0 only */
    return 0;
  unsigned reason = r[4] & 0xFFFF;
  if (reason == 2 || reason == 6)	/* SetMode, SetDMAAddress: look again */
    {
      forget ();
      return 0;
    }
  if (reason != 13 || busy)
    return 0;
  busy = 1;
  int done = 0;
  unsigned op = r[1];
  n_offered++;
  if (op == 1)
    done = copy ((const unsigned *) r[2]);
  else if (op == 2)
    done = fill ((const unsigned *) r[2]);
  else
    n_other++;
  busy = 0;
  if (done)
    r[4] = 0;
  return done;
}

/* --- the module ------------------------------------------------------ */

static oserror *
set (int on)
{
  if (on && !claimed)
    {
      regs r = {{ GRAPHICSV, (unsigned) gfxv_entry, (unsigned) pw }};
      oserror *e = swix_regs (OS_Claim, &r);
      if (e)
	return e;
      claimed = 1;
      forget ();
    }
  if (!on && claimed)
    {
      regs r = {{ GRAPHICSV, (unsigned) gfxv_entry, (unsigned) pw }};
      swix_regs (OS_Release, &r);
      claimed = 0;
    }
  return 0;
}

oserror *
mod_init (const char *tail, int instance, void *p)
{
  (void) tail; (void) instance;
  pw = p;
  if (lsys (NR_getpid, 0, 0, 0, 0, 0, 0) <= 0)
    return ERR (err_notrol);
  sock = -1;
  retry_at = now_cs ();
  return set (1);
}

oserror *
mod_final (int fatal)
{
  (void) fatal;
  set (0);
  if (sock >= 0)
    lsys (NR_close, (unsigned) sock, 0, 0, 0, 0, 0);
  sock = -1;
  return 0;
}

oserror *mod_swi (unsigned number, regs *r) { (void) number; (void) r; return 0; }
void mod_service (regs *r) { (void) r; }

static const char *
word (const char *s, char *out, int n)
{
  while (*s == ' ')
    s++;
  int i = 0;
  while (*s > ' ' && i < n - 1)
    out[i++] = *s++;
  out[i] = 0;
  return s;
}

static unsigned
number (const char *s)
{
  unsigned v = 0;
  while (*s >= '0' && *s <= '9')
    v = v * 10 + (unsigned) (*s++ - '0');
  return v;
}

static int
same (const char *a, const char *b)
{
  for (; *a && *b; a++, b++)
    if ((*a | 32) != (*b | 32))
      return 0;
  return !*a && !*b;
}

static void
line (const char *s, unsigned v)
{
  write0 (s);
  write_dec (v);
  newline ();
}

oserror *
mod_command (int index, const char *tail, int argc)
{
  (void) index;
  char w[16], a[16], b[16];
  const char *t = word (tail, w, sizeof w);
  if (argc >= 1 && same (w, "on"))
    return set (1);
  if (argc >= 1 && same (w, "off"))
    return set (0);
  if (argc >= 1 && same (w, "limits"))
    {
      t = word (t, a, sizeof a);
      word (t, b, sizeof b);
      if (argc != 3)
	return ERR (err_syntax);
      lim_copy = number (a);
      lim_fill = number (b);
      return 0;
    }
  if (argc)
    return ERR (err_syntax);
  write0 (claimed ? "Linux2D: on" : "Linux2D: off");
  write0 (sock >= 0 ? ", rol-2d connected" : ", rol-2d not connected");
  newline ();
  line ("  Copies done by rol-2d: ", n_copy);
  line ("  Fills done by rol-2d: ", n_fill);
  line ("  Left to RISC OS, small: ", n_small);
  line ("  Left to RISC OS, patterned fills: ", n_pattern);
  line ("  Left to RISC OS, other operations: ", n_other);
  line ("  Failed: ", n_fail);
  line ("  Offered in all: ", n_offered);
  line ("  Smallest copy sent (bytes): ", lim_copy);
  line ("  Smallest fill sent (bytes): ", lim_fill);
  if (last_err)
    line ("  Last error (Linux errno): ", (unsigned) -last_err);
  if (scr.known)
    line ("  Screen: reason not usable (0 = usable): ", (unsigned) scr.why);
  return 0;
}
