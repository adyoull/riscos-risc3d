/* RISC3D: RISC OS's 3D driver interface.

   Programs draw with Mesa's EGL and GLES. Mesa's front end opens a
   context here and gets memory it shares with a 3D driver; it writes its
   commands there itself, with no SWI, and calls RISC3D_Sync only to wake
   a driver that has gone idle or to wait for an answer. RISC3D doesn't
   look at the commands: it keeps the drivers that have registered and the
   contexts that are open, and closes a task's contexts when the task
   goes.

   Drivers: on RISC OS on Linux, Linux3D (Linux's graphics driver draws).
   On other machines, a driver for the machine's GPU. With none, Mesa
   draws in software as before.

   SWIs (chunk &C3D00, PROVISIONAL):
     RISC3D_Info      -> r0 = version * 100, r1 = drivers registered,
			 r2 = contexts open, r3 -> the name of the driver a
			 new context would get (0: none), r4 = its flags,
			 r5 = its ioctl SWI and r6 -> its DRM driver name
			 (DRM devices; else 0)
     RISC3D_Register  r0 = 0 add / 1 remove, r1 = entry, r2 = its r12,
			 r3 -> name (add), r4 = flags (add), r5 = ioctl SWI
			 and r6 -> DRM driver name (DRM devices)
     RISC3D_Open      r0 = flags (0) -> r0 = context, r1 -> shared memory,
			 r2 = its size; or, for a DRM device, r1 = the
			 driver's file and r2 = 0

   Driver flags (0.03 on: bit 1 too):
     bit 1 = the driver may draw in software (Linux3D: as Linux's Mesa
     does where there is no GPU driver): RISC3D asks it (reason 3) at each
     RISC3D_Info, and Info's r4 bit 1 is set if it does now. Mesa can then
     keep its own software renderer, which is faster than a software
     renderer's frames copied back.
   Bit 0 = a DRM device. Such a driver (V3DRender, for the
   Pi 4's GPU) takes no command stream: the program's own Mesa drives the
   GPU through the driver's ioctl SWI (r5), with the file from
   RISC3D_Open, as Mesa's DRM drivers do on Linux. RISC3D_Sync isn't used
   with it. Without bit 0 the driver is a command-stream driver
   (Linux3D) as before.
     RISC3D_Close     r0 = context
     RISC3D_Sync      r0 = context, r1 = 0 wake the driver / 1 wait until
			 it has done everything written so far

   A driver's entry (r12 as registered; V set and r0 -> error to fail):
     r0 = 0 open           -> r0 = its handle, r1 -> memory, r2 = size
			      (a DRM device: r0 = its file, r1 = r2 = 0)
     r0 = 1 close          r1 = its handle
     r0 = 2 sync           r1 = its handle, r2 = 0 wake / 1 wait
     r0 = 3 describe       -> r0 = 1 if it draws in software now, 0 on a
			      GPU, 2 not known (drivers registered with
			      flags bit 1 only)

   Finding the driver (0.04 on). RISC3D is installed as !RISC3D in
   !Boot.Resources: its !Boot sets RISC3D$Dir and RISC3D$Path, and it
   holds this module and a folder GPU with the driver module for that
   machine (Linux3D on RISC OS on Linux; V3DRender on a Pi 4; ...). A
   program's !Run then only needs
     RMEnsure RISC3D 0.04 RMLoad RISC3D:RISC3D
   and RISC3D loads the driver itself, just after it starts (a callback),
   or at the first Info, Open or *RISC3D if that comes first. It tries each
   module in GPU, in the order the filing system gives them, until one
   registers; a driver whose hardware isn't there refuses to start, so
   RISC OS doesn't keep it and the next is tried. Nothing is loaded if a
   driver has already registered (loaded by hand or at start). The folder
   is <RISC3D$Drivers> if that is set (for testing), else
   <RISC3D$Dir>.GPU.

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */

#include "cmodule.h"

#define OS_GBPB			0x0C
#define OS_ReadVarVal		0x23
#define OS_AddCallBack		0x54
#define OS_RemoveCallBack	0x5F
#define MAXTRIED		6

void drivers_callback (void);		/* header.s: saves everything, calls find_driver */
static void *private_word;
static int callback_pending, looked;
static char folder[200];
static struct { char name[32]; char why[80]; } tried[MAXTRIED];
static int ntried;

#define Wimp_ReadSysInfo 0x400F2
#define MAXDRV 4
#define MAXCTX 32

oserror *call_driver (unsigned entry, unsigned r12, regs *r);

static struct { unsigned entry, r12, flags, ioctl, drm_name; char name[48]; } drv[MAXDRV];
static struct { int used, driver; unsigned handle, task, area, bytes, syncs; } ctx[MAXCTX];

ERROR_BLOCK (err_swi, 0x1E6, "SWI value out of range for module RISC3D");
ERROR_BLOCK (err_nodriver, 0x81D300, "There is no 3D driver (RISC3D has none registered)");
ERROR_BLOCK (err_full, 0x81D301, "Too many 3D contexts or drivers");
ERROR_BLOCK (err_handle, 0x81D302, "Not a RISC3D context");

static unsigned
this_task (void)
{
  regs r = {{ 5 }};
  return swix_regs (Wimp_ReadSysInfo, &r) ? 0 : r.r[0];
}

static void
ctx_close (int i)
{
  regs r = {{ 1, ctx[i].handle }};
  if (drv[ctx[i].driver].entry)
    call_driver (drv[ctx[i].driver].entry, drv[ctx[i].driver].r12, &r);
  ctx[i].used = 0;
}

static void
copy (char *d, const char *s, size_t n)
{
  size_t l = 0;
  while (s && s[l] >= ' ' && l < n - 1)
    {
      d[l] = s[l];
      l++;
    }
  d[l] = 0;
}

static int best_driver (void);

/* Loads the first module in the drivers folder that starts (see the top). */
void
find_driver (void)
{
  if (looked)
    return;
  looked = 1;
  ntried = 0;
  folder[0] = 0;
  if (best_driver () >= 0)
    return;
  regs v = {{ (unsigned) "RISC3D$Drivers", (unsigned) folder, sizeof folder - 1, 0, 0 }};
  int set = swix_regs (OS_ReadVarVal, &v) == 0 && v.r[2] > 0;
  folder[set ? v.r[2] : 0] = 0;
  if (!set)
    {
      regs d = {{ (unsigned) "RISC3D$Dir", (unsigned) folder, sizeof folder - 5, 0, 0 }};
      if (swix_regs (OS_ReadVarVal, &d) || d.r[2] == 0)
	{
	  folder[0] = 0;
	  return;			/* !RISC3D hasn't been seen */
	}
      memcpy (folder + d.r[2], ".GPU", 5);
    }
  /* OS_GBPB 10: name, type and so on for each object in the folder */
  static unsigned buf[64];
  int next = 0;
  while (next != -1 && best_driver () < 0)
    {
      regs g = {{ 10, (unsigned) folder, (unsigned) buf, 1, (unsigned) next, sizeof buf, 0 }};
      if (swix_regs (OS_GBPB, &g))
	return;				/* no such folder: no driver */
      next = (int) g.r[4];
      if (g.r[3] == 0)
	continue;
      /* load, exec, length, attributes, object type, then the name */
      unsigned load = buf[0], type = buf[4];
      const char *name = (const char *) &buf[5];
      if (type != 1 || (load & 0xFFF00000) != 0xFFF00000 || ((load >> 8) & 0xFFF) != 0xFFA)
	continue;			/* only files of type Module */
      char path[260];
      size_t a = strlen (folder);
      memcpy (path, folder, a);
      path[a] = '.';
      copy (path + a + 1, name, sizeof path - a - 1);
      regs m = {{ 1, (unsigned) path }};
      oserror *e = swix_regs (OS_Module, &m);
      if (ntried < MAXTRIED)
	{
	  copy (tried[ntried].name, name, sizeof tried[ntried].name);
	  copy (tried[ntried].why, e ? e->errmess : best_driver () >= 0 ? "registered" : "started, but didn't register",
		sizeof tried[ntried].why);
	  ntried++;
	}
    }
}

static void
look_now (void)
{
  if (!looked)
    {
      if (callback_pending)
	{
	  regs r = {{ (unsigned) drivers_callback, (unsigned) private_word }};
	  swix_regs (OS_RemoveCallBack, &r);
	  callback_pending = 0;
	}
      find_driver ();
    }
}

/* header.s's callback veneer calls this */
void
drivers_callback_c (void)
{
  callback_pending = 0;
  find_driver ();
}

static int
best_driver (void)
{
  for (int i = MAXDRV - 1; i >= 0; i--)
    if (drv[i].entry)
      return i;
  return -1;
}

oserror *
mod_swi (unsigned number, regs *r)
{
  switch (number)
    {
    case 0:
      {
	look_now ();
	int n = 0, c = 0, b = best_driver ();
	for (int i = 0; i < MAXDRV; i++)
	  n += drv[i].entry != 0;
	for (int i = 0; i < MAXCTX; i++)
	  c += ctx[i].used;
	r->r[0] = 4;
	r->r[1] = (unsigned) n;
	r->r[2] = (unsigned) c;
	r->r[3] = b < 0 ? 0 : (unsigned) drv[b].name;
	r->r[4] = b < 0 ? 0 : drv[b].flags & 1;
	if (b >= 0 && (drv[b].flags & 2))
	  {
	    regs q = {{ 3 }};
	    if (!call_driver (drv[b].entry, drv[b].r12, &q) && q.r[0] == 1)
	      r->r[4] |= 2;
	  }
	r->r[5] = b < 0 ? 0 : drv[b].ioctl;
	r->r[6] = b < 0 ? 0 : drv[b].drm_name;
	return 0;
      }
    case 1:
      if (r->r[0] == 0)
	{
	  for (int i = 0; i < MAXDRV; i++)
	    if (!drv[i].entry)
	      {
		const char *s = (const char *) r->r[3];
		size_t l = 0;
		while (s && s[l] >= ' ' && l < sizeof drv[i].name - 1)
		  l++;
		memset (drv[i].name, 0, sizeof drv[i].name);
		memcpy (drv[i].name, s, l);
		drv[i].r12 = r->r[2];
		drv[i].flags = r->r[4] & 3;
		drv[i].ioctl = drv[i].flags & 1 ? r->r[5] : 0;
		drv[i].drm_name = drv[i].flags & 1 ? r->r[6] : 0;
		drv[i].entry = r->r[1];
		return 0;
	      }
	  return ERR (err_full);
	}
      for (int i = 0; i < MAXDRV; i++)
	if (drv[i].entry == r->r[1] && drv[i].r12 == r->r[2])
	  {
	    /* its contexts go first (their programs get errors from now) */
	    for (int k = 0; k < MAXCTX; k++)
	      if (ctx[k].used && ctx[k].driver == i)
		ctx_close (k);
	    drv[i].entry = 0;
	  }
      return 0;
    case 2:
      {
	look_now ();
	int d = best_driver (), i;
	if (d < 0)
	  return ERR (err_nodriver);
	for (i = 0; i < MAXCTX && ctx[i].used; i++)
	  ;
	if (i == MAXCTX)
	  return ERR (err_full);
	regs c = {{ 0 }};
	oserror *e = call_driver (drv[d].entry, drv[d].r12, &c);
	if (e)
	  return e;
	ctx[i].used = 1;
	ctx[i].driver = d;
	ctx[i].handle = c.r[0];
	ctx[i].area = c.r[1];
	ctx[i].bytes = c.r[2];
	ctx[i].task = this_task ();
	ctx[i].syncs = 0;
	r->r[0] = (unsigned) i + 1;
	r->r[1] = drv[d].flags & 1 ? c.r[0] : c.r[1];
	r->r[2] = drv[d].flags & 1 ? 0 : c.r[2];
	return 0;
      }
    case 3:
    case 4:
      {
	unsigned i = r->r[0] - 1;
	if (i >= MAXCTX || !ctx[i].used)
	  return ERR (err_handle);
	if (number == 3)
	  {
	    ctx_close ((int) i);
	    return 0;
	  }
	regs c = {{ 2, ctx[i].handle, r->r[1] }};
	ctx[i].syncs++;
	return call_driver (drv[ctx[i].driver].entry, drv[ctx[i].driver].r12, &c);
      }
    }
  return ERR (err_swi);
}

/* Service_WimpCloseDown: r2 = the task */
void
mod_service (regs *r)
{
  for (int i = 0; i < MAXCTX; i++)
    if (ctx[i].used && ctx[i].task && ctx[i].task == r->r[2])
      ctx_close (i);
}

oserror *
mod_init (const char *tail, int instance, void *pw)
{
  (void) tail; (void) instance;
  private_word = pw;
  /* find the driver once RISC OS has finished starting this module */
  regs r = {{ (unsigned) drivers_callback, (unsigned) pw }};
  if (!swix_regs (OS_AddCallBack, &r))
    callback_pending = 1;
  return 0;
}

oserror *
mod_final (int fatal)
{
  (void) fatal;
  if (callback_pending)
    {
      regs r = {{ (unsigned) drivers_callback, (unsigned) private_word }};
      swix_regs (OS_RemoveCallBack, &r);
      callback_pending = 0;
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
  look_now ();
  int n = 0;
  for (int i = 0; i < MAXDRV; i++)
    if (drv[i].entry)
      {
	write0 ("Driver: ");
	write0 (drv[i].name);
	if (drv[i].flags & 2)
	  {
	    /* (0.03: one that may draw in software says which now) */
	    regs q = {{ 3 }};
	    oserror *e = call_driver (drv[i].entry, drv[i].r12, &q);
	    write0 (e || q.r[0] > 1 ? ", not known yet whether on a GPU" : q.r[0] ? ", drawing in software" : ", drawing on a GPU");
	  }
	newline ();
	n++;
      }
  if (!n)
    {
      write0 ("No 3D driver is registered");
      newline ();
    }
  if (folder[0])
    {
      write0 ("Drivers folder: ");
      write0 (folder);
      if (!ntried)
	write0 (" (no driver modules tried)");
      newline ();
      for (int i = 0; i < ntried; i++)
	{
	  write0 ("  ");
	  write0 (tried[i].name);
	  write0 (": ");
	  write0 (tried[i].why);
	  newline ();
	}
    }
  for (int i = 0; i < MAXCTX; i++)
    if (ctx[i].used)
      {
	write0 ("Context ");
	write_dec ((unsigned) i + 1);
	write0 (": ");
	write0 (drv[ctx[i].driver].name);
	write0 (", task &");
	write_hex8 (ctx[i].task);
	if (drv[ctx[i].driver].flags & 1)
	  {
	    write0 (", the driver's file ");
	    write_dec (ctx[i].handle);
	  }
	else
	  {
	    write0 (", shared memory at &");
	    write_hex8 (ctx[i].area);
	  }
	write0 (", ");
	write_dec (ctx[i].syncs);
	write0 (" calls to the driver");
	newline ();
      }
  return 0;
}
