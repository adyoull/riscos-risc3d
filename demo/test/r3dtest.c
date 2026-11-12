/* r3dtest [frames] [vbo]: RISC3D from the command line. Opens a context,
   says what is drawing, draws the test cube for some frames at 640 by 480
   and checks what comes back: the corner is the clear colour, the middle
   is the cube, and two angles differ. Prints the frame rate.
   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <stdlib.h>
#include <time.h>
#include "r3d_gl.h"
#define R3D_DIRECT
#include "r3d.h"
#include "scene.h"

#define W 640
#define H 480
static unsigned char frame[W * H * 4], first[W * H * 4];

int
main (int argc, char **argv)
{
  int frames = argc > 1 ? atoi (argv[1]) : 30, vbo = argc > 2;
  const char *e = r3d_open ();
  if (e)
    {
      printf ("RISC3D: %s\n", e);
      return 1;
    }
  r3d_surface (W, H);
  printf ("Renderer: %s\nVersion:  %s\nGLSL:     %s\n", (const char *) glGetString (GL_RENDERER),
	  (const char *) glGetString (GL_VERSION), (const char *) glGetString (GL_SHADING_LANGUAGE_VERSION));
  e = scene_init (vbo);
  if (e)
    {
      printf ("scene: %s\n", e);
      return 1;
    }
  int bad = 0;
  clock_t t0 = clock ();
  for (int i = 0; i < frames; i++)
    {
      scene_draw (W, H, 0.6f + (float) i * 0.05f);
      int n = r3d_swap (frame, sizeof frame);
      if (n != (int) sizeof frame)
	{
	  printf ("frame %d: %d bytes came back (%s)\n", i, n, r3d_error () ? r3d_error () : "no error");
	  return 1;
	}
      if (i == 0)
	memcpy (first, frame, sizeof frame);
    }
  double secs = (double) (clock () - t0) / CLOCKS_PER_SEC;
  const unsigned char *c = frame, *m = frame + ((H / 2) * W + W / 2) * 4;
  printf ("corner %d,%d,%d (the clear colour is 26,31,77); middle %d,%d,%d\n", c[0], c[1], c[2], m[0], m[1], m[2]);
  if (abs (c[0] - 26) > 2 || abs (c[1] - 31) > 2 || abs (c[2] - 77) > 2)
    bad++, printf ("the corner is wrong\n");
  if (abs (m[0] - 26) <= 2 && abs (m[1] - 31) <= 2 && abs (m[2] - 77) <= 2)
    bad++, printf ("the middle is empty\n");
  if (frames > 1 && !memcmp (first, frame, sizeof frame))
    bad++, printf ("the cube did not turn\n");
  GLint vp[4] = { 0, 0, 0, 0 };
  glGetIntegerv (GL_VIEWPORT, vp);
  if (vp[2] != W || vp[3] != H || glGetError () != GL_NO_ERROR)
    bad++, printf ("a query or GL error is wrong (viewport %d by %d)\n", vp[2], vp[3]);
  if (secs > 0)
    printf ("%d frames in %.2f s of this program's time: %.1f a second (%s)\n", frames, secs, frames / secs,
	    vbo ? "arrays in a buffer object" : "arrays from the program's memory");
#ifdef R3D_HOST
  FILE *f = fopen ("frame.ppm", "wb");
  fprintf (f, "P6 %d %d 255\n", W, H);
  for (int i = 0; i < W * H; i++)
    fwrite (frame + i * 4, 1, 3, f);
  fclose (f);
#endif
  r3d_close ();
  printf (bad ? "r3dtest FAILED\n" : "r3dtest ok\n");
  return bad != 0;
}
