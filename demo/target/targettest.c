/* targettest: a context drawing into the program's own buffer, as OSMesa's
   do (RISC3D 0.7: r3d_target). The same scene in each of OSMesa's formats,
   with rows top first and bottom first and a row length; for each, a sum of
   the program's buffer after glFinish, and whether it changed with no
   glFinish/glFlush yet (it shouldn't, for RISC3D; OSMesa draws at once, so
   that line is skipped there).

   Built with -DOSMESA it draws with riscos-mesa's OSMesa itself (Mesa
   20.3.5, its patches): the sums should be the same, the scene being flat
   colours on pixel centres.

     targettest

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "r3d_gl.h"

#define W 64
#define H 48
#define ROW 72				/* a row length longer than the width */

#define OSMESA_RGBA GL_RGBA
#define OSMESA_BGRA 0x1
#define OSMESA_ARGB 0x2
#define OSMESA_RGB GL_RGB
#define OSMESA_BGR 0x4
#define OSMESA_RGB_565 0x5
#define OSMESA_ROW_LENGTH 0x10
#define OSMESA_Y_UP 0x11

typedef void (*fnp) (void);
#define FUNCS(X) \
  X (glViewport) X (glClearColor) X (glClear) X (glMatrixMode) X (glLoadIdentity) X (glOrtho) \
  X (glColor3ub) X (glBegin) X (glEnd) X (glVertex2f) X (glFinish) X (glFlush) X (glGetString) X (glGetError)
#define DECL(n) static __typeof__ (n) *p_##n;
FUNCS (DECL)
#define GL(n) (*p_##n)

#ifdef OSMESA
typedef struct osmesa_context *OSMesaContext;
OSMesaContext OSMesaCreateContextExt (GLenum, GLint, GLint, GLint, OSMesaContext);
GLboolean OSMesaMakeCurrent (OSMesaContext, void *, GLenum, GLsizei, GLsizei);
void OSMesaPixelStore (GLint, GLint);
void OSMesaDestroyContext (OSMesaContext);
fnp OSMesaGetProcAddress (const char *);
static OSMesaContext om;
static int fmt_now;
static fnp find (const char *name) { return OSMesaGetProcAddress (name); }
static const char *
open_ctx (int format)
{
  if (om && format != fmt_now)
    {
      OSMesaDestroyContext (om);
      om = 0;
    }
  if (!om)
    om = OSMesaCreateContextExt (format, 0, 0, 0, 0);
  fmt_now = format;
  return om ? 0 : "no OSMesa context";
}
static const char *
bind (void *buf, int format, int row, int up)
{
  if (!OSMesaMakeCurrent (om, buf, format == OSMESA_RGB_565 ? GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE, W, H))
    return "OSMesaMakeCurrent failed";
  OSMesaPixelStore (OSMESA_ROW_LENGTH, row);
  OSMesaPixelStore (OSMESA_Y_UP, up);
  return 0;
}
#define WHO "OSMesa"
#define DRAWS_AT_ONCE 1
#else
#include "r3d.h"
static struct r3d_context *rc;
static fnp
find (const char *name)
{
  for (int i = 0; r3d_gl_functions[i].name; i++)
    if (!strcmp (r3d_gl_functions[i].name, name))
      return r3d_gl_functions[i].fn;
  return 0;
}
static const char *
open_ctx (int format)
{
  (void) format;
  if (!rc)
    rc = r3d_create_gl ();
  if (!rc)
    return r3d_error ();
  r3d_make_current (rc);
  return 0;
}
static const char *bind (void *buf, int format, int row, int up) { return r3d_target (rc, buf, W, H, format, row, up); }
#define WHO "RISC3D"
#define DRAWS_AT_ONCE 0
#endif

#define LOAD(n) p_##n = (__typeof__ (n) *) find (#n);

static uint32_t
sum (const unsigned char *p, size_t n)
{
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}

/* flat colours, edges on pixel boundaries */
static void
scene (int step)
{
  GL (glViewport) (0, 0, W, H);
  GL (glMatrixMode) (GL_PROJECTION);
  GL (glLoadIdentity) ();
  GL (glOrtho) (0, W, 0, H, -1, 1);
  GL (glMatrixMode) (GL_MODELVIEW);
  GL (glLoadIdentity) ();
  GL (glClearColor) (0.0f, 0.0f, 0.0f, 1.0f);
  GL (glClear) (GL_COLOR_BUFFER_BIT);
  static const unsigned char col[4][3] = { { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 }, { 200, 120, 40 } };
  for (int i = 0; i < 4; i++)
    {
      float x = (float) (4 + i * 14 + step), y = (float) (4 + i * 9);
      GL (glColor3ub) (col[i][0], col[i][1], col[i][2]);
      GL (glBegin) (GL_QUADS);
      GL (glVertex2f) (x, y);
      GL (glVertex2f) (x + 12, y);
      GL (glVertex2f) (x + 12, y + 20);
      GL (glVertex2f) (x, y + 20);
      GL (glEnd) ();
    }
}

int
main (void)
{
  static const struct { int format, bpp; const char *name; } fmts[] = {
    { OSMESA_RGBA, 4, "RGBA" }, { OSMESA_BGRA, 4, "BGRA" }, { OSMESA_ARGB, 4, "ARGB" },
    { OSMESA_RGB, 3, "RGB" }, { OSMESA_BGR, 3, "BGR" }, { OSMESA_RGB_565, 2, "RGB_565" },
  };
  printf ("target (%s)\n", WHO);
  for (size_t f = 0; f < sizeof fmts / sizeof *fmts; f++)
    for (int up = 0; up < 2; up++)
      for (int rowed = 0; rowed < 2; rowed++)
	{
	  int row = rowed ? ROW : W;
	  size_t bytes = (size_t) row * H * (size_t) fmts[f].bpp;
	  unsigned char *buf = calloc (1, bytes);
	  const char *e = open_ctx (fmts[f].format);
	  if (!e)
	    {
	      FUNCS (LOAD)
	      e = bind (buf, fmts[f].format, rowed ? ROW : 0, up);
	    }
	  if (e)
	    {
	      printf ("%s: %s\n", fmts[f].name, e);
	      free (buf);
	      continue;
	    }
	  memset (buf, 0x5a, bytes);
	  scene (0);
	  uint32_t before = sum (buf, bytes);
	  GL (glFinish) ();
	  uint32_t s1 = sum (buf, bytes);
	  /* drawn again, moved: glFlush brings it */
	  scene (2);
	  GL (glFlush) ();
	  uint32_t s2 = sum (buf, bytes);
	  /* a glFinish with nothing new changes nothing */
	  memset (buf, 0x5a, bytes);
	  GL (glFinish) ();
	  int kept = sum (buf, bytes) == sum (buf, bytes) && buf[0] == 0x5a;
	  if (DRAWS_AT_ONCE)
	    printf ("%-7s %s %s: %08x %08x\n", fmts[f].name, up ? "up  " : "down", rowed ? "row 72" : "row 64", s1, s2);
	  else
	    printf ("%-7s %s %s: %08x %08x%s%s\n", fmts[f].name, up ? "up  " : "down", rowed ? "row 72" : "row 64", s1, s2,
		    before == s1 ? " (not copied at glFinish!)" : "", kept ? "" : " (copied again!)");
	  free (buf);
	}
  printf ("error: &%X\n", GL (glGetError) ());
  return 0;
}
