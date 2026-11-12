/* R3DCube: the RISC3D test cube in a desktop window. Each frame is drawn
   by RISC3D's driver and plotted as a 32 bits a pixel sprite; the title
   bar says what is drawing and how fast.
   "r3dcube -overlay": zero-copy frames where the renderer can show them
   on a display plane (r3d_overlay): the window's work area is painted in
   the key colour and the frames show through it, never read back.
   PROTOTYPE: a real program would use Mesa's EGL, not r3d.h.
   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <swis.h>
#include <kernel.h>
#include "r3d_gl.h"
#define R3D_DIRECT
#include "r3d.h"
#include "scene.h"

#define W 480
#define H 360

static uint32_t *area;			/* sprite area: 16 + 44 bytes of headers, then pixels */
static char title[160] = "RISC3D";
static int window;
static int overlay;			/* frames go to the screen */
static unsigned key;

/* the window's visible area in RISC OS pixels from the screen's top left,
   to r3d_overlay (width 0 if the window isn't open) */
static int
place (void)
{
  int st[9] = { window }, xe = 1, ye = 1, ywl = 0;
  _swix (Wimp_GetWindowState, _IN (1), st);
  _swix (OS_ReadModeVariable, _INR (0, 1) | _OUT (2), -1, 4, &xe);
  _swix (OS_ReadModeVariable, _INR (0, 1) | _OUT (2), -1, 5, &ye);
  _swix (OS_ReadModeVariable, _INR (0, 1) | _OUT (2), -1, 12, &ywl);
  if (!(st[8] & (1 << 16)))
    return r3d_overlay (0, 0, 0, 0, &key);
  return r3d_overlay (st[1] >> xe, ywl + 1 - (st[4] >> ye), (st[3] - st[1]) >> xe, (st[4] - st[2]) >> ye, &key);
}

static void
plot (const int *b)
{
  if (overlay)
    {
      /* the key colour (&BBGGRR) as a palette entry, &BBGGRR00 */
      _swix (ColourTrans_SetGCOL, _IN (0) | _INR (3, 4), key << 8, 0, 0);
      _swix (OS_Plot, _INR (0, 2), 4, b[7], b[8]);
      _swix (OS_Plot, _INR (0, 2), 101, b[9], b[10]);
      return;
    }
  int ox = b[1] - b[5], oy = b[4] - b[6];
  _swix (OS_SpriteOp, _INR (0, 7), 0x234, area, (char *) area + 16, ox, oy - H * 2, 0, 0, 0);
}

static void
redraw (int update)
{
  int b[16], more = 0;
  b[0] = window;
  b[1] = 0; b[2] = -H * 2; b[3] = W * 2; b[4] = 0;
  _swix (update ? Wimp_UpdateWindow : Wimp_RedrawWindow, _IN (1) | _OUT (0), b, &more);
  while (more)
    {
      plot (b);
      _swix (Wimp_GetRectangle, _IN (1) | _OUT (0), b, &more);
    }
}

int
main (int argc, char **argv)
{
  int want_overlay = argc > 1 && !strcmp (argv[1], "-overlay");
  static int msgs[] = { 0 };
  int task = 0;
  _swix (Wimp_Initialise, _INR (0, 3) | _OUT (1), 310, 0x4B534154, "RISC3D cube", msgs, &task);
  const char *e = r3d_open ();
  if (!e)
    {
      r3d_surface (W, H);
      e = scene_init (0);
    }
  if (e)
    {
      _kernel_oserror err = { 0, "" };
      snprintf (err.errmess, sizeof err.errmess, "RISC3D cube: %.200s", e);
      _swix (Wimp_ReportError, _INR (0, 2), &err, 1, "RISC3D cube");
      _swix (Wimp_CloseDown, _INR (0, 1), task, 0x4B534154);
      return 1;
    }
  char renderer[100];
  snprintf (renderer, sizeof renderer, "%s", (const char *) glGetString (GL_RENDERER));

  size_t bytes = 16 + 44 + W * H * 4;
  area = calloc (bytes, 1);
  area[0] = (uint32_t) bytes; area[1] = 1; area[2] = 16; area[3] = (uint32_t) bytes;
  uint32_t *sp = area + 4;
  sp[0] = 44 + W * H * 4;
  memcpy (sp + 1, "frame\0\0\0\0\0\0\0", 12);
  sp[4] = W - 1; sp[5] = H - 1; sp[6] = 0; sp[7] = 31; sp[8] = 44; sp[9] = 44;
  sp[10] = (6u << 27) | (90u << 14) | (90u << 1) | 1;	/* 32 bits a pixel, 90 dpi */
  unsigned char *pixels = (unsigned char *) (sp + 11);

  int wb[23] = { 200, 300, 200 + W * 2, 300 + H * 2, 0, 0, -1, (int) 0x87000002,
		 0xFF000207 /* title fg 7, bg 2, work fg 0, bg transparent */, 0x000C0103,
		 0, -H * 2, W * 2, 0, 0x119, 0, 1, 0, (int) title, -1, sizeof title, 0, 0 };
  _swix (Wimp_CreateWindow, _IN (1) | _OUT (0), wb, &window);
  int ob[8] = { window, 200, 300, 200 + W * 2, 300 + H * 2, 0, 0, -1 };
  _swix (Wimp_OpenWindow, _IN (1), ob);
  const char *how = "read back";
  if (want_overlay)
    {
      int bpp = 0;
      _swix (OS_ReadModeVariable, _INR (0, 1) | _OUT (2), -1, 9, &bpp);
      if (bpp != 5)
	how = "read back: zero-copy needs 16M colours";
      else if (place () == 1)
	{
	  overlay = 1;
	  how = "zero-copy";
	  _swix (Wimp_ForceRedraw, _INR (0, 4), window, 0, -H * 2, W * 2, 0);
	}
      else
	how = "read back: no overlay";
    }

  int frames = 0, shown = 0, quit = 0;
  clock_t t0 = clock ();
  float angle = 0.6f;
  while (!quit)
    {
      int b[64], ev = 0;
      _swix (Wimp_Poll, _INR (0, 1) | _OUT (0), 0, b, &ev);
      switch (ev)
	{
	case 0:
	  scene_draw (W, H, angle);
	  angle += 0.05f;
	  if (overlay)
	    {
	      if (r3d_swap_overlay () < 0)
		{
		  /* (a mode change, say): back to reading frames */
		  overlay = 0;
		  how = "read back: the overlay stopped";
		  _swix (Wimp_ForceRedraw, _INR (0, 4), window, 0, -H * 2, W * 2, 0);
		}
	    }
	  else if (r3d_swap_overlapped (pixels, W * H * 4) != W * H * 4)
	    {
	      quit = 1;
	      break;
	    }
	  else
	    redraw (1);
	  frames++;
	  if (clock () - t0 >= CLOCKS_PER_SEC || !shown)
	    {
	      double s = (double) (clock () - t0) / CLOCKS_PER_SEC;
	      snprintf (title, sizeof title, "RISC3D: %s, %s, %.1f frames a second", renderer, how, s > 0 ? frames / s : 0.0);
	      _swix (Wimp_ForceRedraw, _INR (0, 2), window, 0x4B534154, 3);
	      if (shown)
		frames = 0, t0 = clock ();
	      shown = 1;
	    }
	  break;
	case 1:
	  b[0] = window;
	  redraw (0);
	  break;
	case 2:
	  _swix (Wimp_OpenWindow, _IN (1), b);
	  if (overlay)
	    place ();
	  break;
	case 3:
	  if (overlay)
	    r3d_overlay (0, 0, 0, 0, 0);
	  quit = 1;
	  break;
	case 17: case 18:
	  if (b[4] == 0)
	    quit = 1;
	  break;
	}
    }
  r3d_close ();
  _swix (Wimp_CloseDown, _INR (0, 1), task, 0x4B534154);
  return 0;
}
