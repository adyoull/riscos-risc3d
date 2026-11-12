/* R3DCube: the RISC3D test cube in a desktop window. Each frame is drawn
   by RISC3D's driver and plotted as a 32 bits a pixel sprite; the title
   bar says what is drawing and how fast.
   PROTOTYPE: a real program would use Mesa's EGL, not r3d.h.
   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <stdint.h>
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

static void
plot (const int *b)
{
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
main (void)
{
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
	  if (r3d_swap_overlapped (pixels, W * H * 4) != W * H * 4)
	    {
	      quit = 1;
	      break;
	    }
	  redraw (1);
	  frames++;
	  if (clock () - t0 >= CLOCKS_PER_SEC || !shown)
	    {
	      double s = (double) (clock () - t0) / CLOCKS_PER_SEC;
	      snprintf (title, sizeof title, "RISC3D: %s, %.1f frames a second", renderer, s > 0 ? frames / s : 0.0);
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
	  break;
	case 3:
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
