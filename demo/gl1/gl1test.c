/* gl1test [out.ppm]: OpenGL 1.x and 2.1 (the compatibility profile)
   through RISC3D: immediate mode, lighting, a texture under unusual pixel
   store settings, display lists (one with a transform, a "font" called
   with glCallLists and glListBase), vertex arrays in the program's memory
   and in a buffer object, glInterleavedArrays, glArrayElement, glBitmap,
   glDrawPixels, selection (glRenderMode), glMapBuffer, and the queries
   programs make (the matrices, the viewport, the list base).

   It prints every answer and a checksum of the picture, and writes the
   picture. Built with DIRECT, the same program draws with Linux's Mesa
   directly (EGL, no RISC3D): the two outputs should match.

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "r3d_gl.h"

#define W 320
#define H 240
static unsigned char frame[W * H * 4];

#ifdef DIRECT
/* --- Mesa directly: EGL with no surface, a framebuffer object ---------- */
typedef void *EGLDisplay, *EGLConfig, *EGLContext;
typedef int EGLint;
EGLDisplay eglGetPlatformDisplay (unsigned, void *, const intptr_t *);
unsigned eglInitialize (EGLDisplay, EGLint *, EGLint *);
unsigned eglBindAPI (unsigned);
unsigned eglChooseConfig (EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
EGLContext eglCreateContext (EGLDisplay, EGLConfig, EGLContext, const EGLint *);
unsigned eglMakeCurrent (EGLDisplay, void *, void *, EGLContext);

static const char *
open_gl (void)
{
  static const EGLint want[] = { 0x3040, 8, 0x3033, 0, 0x3038 };
  EGLConfig cfg;
  EGLint n = 0;
  EGLDisplay d = eglGetPlatformDisplay (0x31DD, 0, 0);
  if (!d || !eglInitialize (d, 0, 0))
    return "no EGL";
  eglBindAPI (0x30A2);
  if (!eglChooseConfig (d, want, &cfg, 1, &n) || n < 1)
    return "no config";
  EGLContext c = eglCreateContext (d, cfg, 0, 0);
  if (!c || !eglMakeCurrent (d, 0, 0, c))
    return "no context";
  GLuint fb, tex = 0x7fff0000u, rb;	/* (as RISC3D's helper: test names start at 1) */
  glGenFramebuffers (1, &fb);
  glGenRenderbuffers (1, &rb);
  glBindTexture (GL_TEXTURE_2D, tex);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glBindRenderbuffer (GL_RENDERBUFFER, rb);
  glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, W, H);
  glBindFramebuffer (GL_FRAMEBUFFER, fb);
  glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
  glDrawBuffer (GL_COLOR_ATTACHMENT0);
  glReadBuffer (GL_COLOR_ATTACHMENT0);
  glBindTexture (GL_TEXTURE_2D, 0);
  glViewport (0, 0, W, H);
  glScissor (0, 0, W, H);
  /* (names the test makes start where RISC3D's do) */
  return 0;
}

static void
finish_frame (void)
{
  static unsigned char t[W * H * 4];
  glPixelStorei (GL_PACK_ALIGNMENT, 4);
  glReadPixels (0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, t);
  for (int y = 0; y < H; y++)
    memcpy (frame + (size_t) y * W * 4, t + (size_t) (H - 1 - y) * W * 4, W * 4);
}
#define WHO "Mesa directly"
#else
/* --- through RISC3D ---------------------------------------------------- */
#define R3D_DIRECT
#include "r3d.h"

static const char *
open_gl (void)
{
  const char *e = r3d_open_gl ();
  if (!e)
    r3d_surface (W, H);
  return e;
}

static int overlapped;

static void
finish_frame (void)
{
  if (overlapped)
    r3d_swap_overlapped (frame, sizeof frame);
  else
    r3d_swap (frame, sizeof frame);
}
#define WHO "RISC3D"
#endif

volatile float sink[64];		/* (for the processor measure) */

static void
show_f (const char *what, const GLfloat *v, int n)
{
  printf ("%s:", what);
  for (int i = 0; i < n; i++)
    printf (" %.5g", (double) v[i] + 0.0);
  printf ("\n");
}

static void
show_matrix (const char *what, GLenum pname)
{
  GLfloat m[16];
  memset (m, 0, sizeof m);
  glGetFloatv (pname, m);
  show_f (what, m, 16);
}

static void
quad (float x, float y, float s)
{
  glBegin (GL_QUADS);
  glNormal3f (0, 0, 1);
  glTexCoord2f (0, 0); glVertex3f (x, y, 0);
  glTexCoord2f (1, 0); glVertex3f (x + s, y, 0);
  glTexCoord2f (1, 1); glVertex3f (x + s, y + s, 0);
  glTexCoord2f (0, 1); glVertex3f (x, y + s, 0);
  glEnd ();
}

/* gl1test bench: immediate mode as YSFlight draws, a call per vertex:
   frames of 20000 vertices, each with a colour and a normal */
static double
now (void)
{
#ifdef __riscos__
  return (double) clock () / CLOCKS_PER_SEC;	/* (RISC OS: real time) */
#else
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return (double) t.tv_sec + t.tv_nsec / 1e9;
#endif
}

static void
bench (void)
{
  enum { FRAMES = 60, QUADS = 5000 };
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (-1, 1, -1, 1, -1, 1);
  glMatrixMode (GL_MODELVIEW);
  GLfloat m[16];
#ifndef DIRECT
  unsigned long long st0[4];
  r3d_stats (st0);			/* (from here) */
#endif
  double t0 = now ();
  for (int f = 0; f < FRAMES; f++)
    {
      glClear (GL_COLOR_BUFFER_BIT);
      glLoadIdentity ();
      glRotatef ((float) f, 0, 0, 1);
      glBegin (GL_QUADS);
      for (int i = 0; i < QUADS; i++)
	{
	  float x = (float) (i % 100) / 50.0f - 1.0f, y = (float) (i / 100) / 25.0f - 1.0f;
	  glColor3f (x * 0.5f + 0.5f, y * 0.5f + 0.5f, 0.5f);
	  glNormal3f (0, 0, 1);
	  glVertex2f (x, y);
	  glVertex2f (x + 0.015f, y);
	  glVertex2f (x + 0.015f, y + 0.03f);
	  glVertex2f (x, y + 0.03f);
	}
      glEnd ();
      /* and the reads a frame makes (Warzone, YSFlight: per object) */
      for (int i = 0; i < 100; i++)
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
      finish_frame ();
    }
  double s = now () - t0;
#ifndef DIRECT
  unsigned long long st[4];
  int have = r3d_stats (st) == 0;
#endif
  printf ("bench: %d frames of %d immediate-mode vertices in %.2f s: "
	  "%.1f frames a second, %.1f million calls a second\n", FRAMES, QUADS * 4, s, FRAMES / s,
	  FRAMES * (double) QUADS * 6 / s / 1e6);
#ifndef DIRECT
  if (have)
    printf ("  of which the renderer worked %.2f s (reading frames back %.2f s), slept %llu times, %llu commands\n",
	    st[0] / 1e9, st[1] / 1e9, st[2], st[3]);
#endif
  /* frames with nothing in them: what a frame costs on its own */
  t0 = now ();
  for (int f = 0; f < FRAMES; f++)
    {
      glClear (GL_COLOR_BUFFER_BIT);
      finish_frame ();
    }
  s = now () - t0;
  printf ("empty frames: %.1f a second (%.2f ms each)\n", FRAMES / s, s * 1000 / FRAMES);
  /* the calls alone: the same 60 frames' calls, one frame at the end */
  t0 = now ();
  for (int f = 0; f < FRAMES; f++)
    {
      glBegin (GL_QUADS);
      for (int i = 0; i < QUADS; i++)
	{
	  float x = (float) (i % 100) / 50.0f - 1.0f, y = (float) (i / 100) / 25.0f - 1.0f;
	  glColor3f (x * 0.5f + 0.5f, y * 0.5f + 0.5f, 0.5f);
	  glNormal3f (0, 0, 1);
	  glVertex2f (x, y);
	  glVertex2f (x + 0.015f, y);
	  glVertex2f (x + 0.015f, y + 0.03f);
	  glVertex2f (x, y + 0.03f);
	}
      glEnd ();
    }
  double issued = now () - t0;
  finish_frame ();
  s = now () - t0;
  printf ("the calls alone: %.1f million calls a second (issued in %.2f s, done in %.2f s)\n",
	  FRAMES * (double) QUADS * 6 / s / 1e6, issued, s);
#ifndef DIRECT
  if (r3d_stats (st) == 0)
    printf ("  of which the renderer worked %.2f s, slept %llu times, %llu commands\n", st[0] / 1e9, st[2], st[3]);
#endif
}

/* gl1test track: drawn as TORCS (plib's ssg) draws a track: objects of
   a few thousand vertices in the program's memory, each drawn as many
   triangle strips with glDrawElements (indices in memory), arrays set up
   again for each object under glPushClientAttrib; then the loader's
   texture pattern (a proxy query, every mipmap level, the wrap
   modes read back) timed in groups of 50 */
enum { T_OBJS = 60, T_ROWS = 40, T_COLS = 60, T_FRAMES = 20, T_TEX = 200 };
static GLfloat t_v[T_OBJS][T_ROWS * T_COLS][3], t_n[T_OBJS][T_ROWS * T_COLS][3], t_t[T_OBJS][T_ROWS * T_COLS][2];
static GLushort t_i[T_ROWS - 1][T_COLS * 2];

static int t_parts = 7;			/* 1 strips, 2 textured, 4 lines */

static unsigned long
frame_hash (void)
{
  unsigned long h = 5381;
  for (size_t i = 0; i < sizeof frame; i += 4)
    h = h * 33 + frame[i] + frame[i + 1] * 7u + frame[i + 2] * 13u;
  return h & 0xffffffffUL;
}

static void
save_ppm (const char *name)
{
  FILE *f = name ? fopen (name, "wb") : 0;
  if (!f)
    return;
  fprintf (f, "P6 %d %d 255\n", W, H);
  for (int i = 0; i < W * H; i++)
    fwrite (frame + i * 4, 1, 3, f);
  fclose (f);
}

static void
track_frame (int f)
{
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  glRotatef ((float) f * 3, 0, 0, 1);
  for (int o = 0; o < T_OBJS; o++)
    {
      glColor4f (1, 1, 1, 1);
      glPushClientAttrib (GL_CLIENT_VERTEX_ARRAY_BIT);
      glEnableClientState (GL_NORMAL_ARRAY);
      glNormalPointer (GL_FLOAT, 0, t_n[o]);
      glEnableClientState (GL_TEXTURE_COORD_ARRAY);
      glTexCoordPointer (2, GL_FLOAT, 0, t_t[o]);
      glEnableClientState (GL_VERTEX_ARRAY);
      glVertexPointer (3, GL_FLOAT, 0, t_v[o]);
      if (t_parts & 2)
	glEnable (GL_TEXTURE_2D);
      else
	glDisable (GL_TEXTURE_2D);
      if (t_parts & 1)
	for (int r = 0; r < T_ROWS - 1; r++)
	  glDrawElements (GL_TRIANGLE_STRIP, T_COLS * 2, GL_UNSIGNED_SHORT, t_i[r]);
      /* (and a row as a line, drawn from a first vertex) */
      glDisable (GL_TEXTURE_2D);
      glColor4f (1, 1, 0, 1);
      if (t_parts & 4)
	glDrawArrays (GL_LINE_STRIP, (o % 7 + 3) * T_COLS, T_COLS);
      glPopClientAttrib ();
    }
  finish_frame ();
}

static void
track (const char *pic)
{
  for (int o = 0; o < T_OBJS; o++)
    for (int r = 0; r < T_ROWS; r++)
      for (int c = 0; c < T_COLS; c++)
	{
	  int k = r * T_COLS + c;
	  float x = (float) (o % 10) * 0.2f - 1 + (float) c * 0.18f / T_COLS;
	  float y = (float) (o / 10) * 0.33f - 1 + (float) r * 0.3f / T_ROWS;
	  t_v[o][k][0] = x; t_v[o][k][1] = y; t_v[o][k][2] = 0;
	  t_n[o][k][0] = 0; t_n[o][k][1] = 0; t_n[o][k][2] = 1;
	  t_t[o][k][0] = (float) c / 8; t_t[o][k][1] = (float) r / 8;
	}
  for (int r = 0; r < T_ROWS - 1; r++)
    for (int c = 0; c < T_COLS; c++)
      {
	t_i[r][c * 2] = (GLushort) ((r + 1) * T_COLS + c);
	t_i[r][c * 2 + 1] = (GLushort) (r * T_COLS + c);
      }
  static unsigned char img[64 * 64 * 3];
  for (int i = 0; i < 64 * 64; i++)
    {
      img[i * 3] = (unsigned char) (i * 7);
      img[i * 3 + 1] = (unsigned char) (((i % 64) / 8 ^ (i / 512)) & 1 ? 230 : 60);
      img[i * 3 + 2] = (unsigned char) (i / 16);
    }
  GLuint tex;
  glGenTextures (1, &tex);
  glBindTexture (GL_TEXTURE_2D, tex);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glEnable (GL_TEXTURE_2D);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glOrtho (-1.2, 1.2, -1.2, 1.2, -1, 1);
  double t0;
#if !defined DIRECT && !defined R3D_08
  /* (RISC3D: the renderer's two ways with arrays from memory: 0.8's
     first, then the default) */
  for (int way = 0; way < 2; way++)
#else
  int way = 1;
  (void) way;
#endif
    {
#ifndef DIRECT
#ifndef R3D_08
      r3d_uploads (way);
#endif
      unsigned long long st0[4], st[4];
      r3d_stats (st0);
#endif
      t0 = now ();
      for (int f = 0; f < T_FRAMES; f++)
	track_frame (f);
      double s = now () - t0;
      printf ("track: %d frames of %d objects, %d strips (%d vertices) each, in %.2f s: %.1f frames a second\n",
	      T_FRAMES, T_OBJS, T_ROWS - 1, T_ROWS * T_COLS, s, T_FRAMES / s);
#ifndef DIRECT
#ifdef R3D_08
      printf ("  (RISC3D 0.8's library: every strip sent all the arrays up to its last vertex)\n");
#else
      printf ("  (arrays %s)\n", way ? "given to Mesa as memory: the default" : "each in a buffer of its own, as 0.8");
#endif
      if (r3d_stats (st) == 0)
	printf ("  of which the renderer worked %.2f s, slept %llu times, %llu commands\n",
		(st[0] - st0[0]) / 1e9, st[2] - st0[2], st[3] - st0[3]);
#endif
      printf ("track picture: %08lx\n", frame_hash ());
      /* (the parts on their own, to say which differs) */
      static const int parts[] = { 1, 3, 4 };
      static const char *const pname[] = { "strips untextured", "strips textured", "lines from a first vertex" };
      for (int k = 0; k < 3; k++)
	{
	  t_parts = parts[k];
	  track_frame (T_FRAMES - 1);
	  printf ("  %s: %08lx\n", pname[k], frame_hash ());
	}
      t_parts = 7;
      track_frame (T_FRAMES - 1);
      if (pic)
	{
	  char name[256];
#ifdef DIRECT
	  snprintf (name, sizeof name, "%s", pic);
#elif defined R3D_08
	  snprintf (name, sizeof name, "%s8", pic);
#else
	  snprintf (name, sizeof name, "%s%s", pic, way ? "M" : "B");
#endif
	  save_ppm (name);
	}
    }

  /* the loader's textures (TORCS's grtexture.cpp, with the RISC OS port's
     wrap check) */
  static unsigned char lv[256 * 256 * 4];
  for (size_t i = 0; i < sizeof lv; i++)
    lv[i] = (unsigned char) (i * 13);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  t0 = now ();
  double g0 = t0;
  for (int k = 0; k < T_TEX; k++)
    {
      int size = 16 << (k % 5);		/* 16 .. 256 */
      GLuint t;
      glGenTextures (1, &t);
      glBindTexture (GL_TEXTURE_2D, t);
      GLint ww = 0, wrap = 0;
      glTexImage2D (GL_PROXY_TEXTURE_2D, 0, 4, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
      glGetTexLevelParameteriv (GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &ww);
      if (ww != size)
	printf ("  proxy %d: width %d\n", size, ww);
      for (int l = 0, w = size; w >= 1; l++, w >>= 1)
	glTexImage2D (GL_TEXTURE_2D, l, 4, w, w, 0, GL_RGBA, GL_UNSIGNED_BYTE, lv);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glGetTexParameteriv (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
      glGetTexParameteriv (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrap);
      if ((k + 1) % 50 == 0)
	{
	  double n = now ();
	  printf ("textures %d-%d: %.3f s\n", k - 48, k + 1, n - g0);
	  g0 = n;
	}
    }
  printf ("textures: %d in %.2f s\n", T_TEX, now () - t0);
}

/* gl1test multi: TORCS's multitexture drawing (grvtxtable.cpp): a car
   in immediate mode with glMultiTexCoord2fv for units 0, 2 and 1 and a
   texture matrix on units 1 and 2; then the track's way, arrays for
   units 0-2 under glPushClientAttrib and strips with glDrawElements */
static GLuint
checker_tex (int a, int b)
{
  static unsigned char im[32 * 32 * 3];
  for (int i = 0; i < 32 * 32; i++)
    {
      int on = ((i % 32) / 4 + (i / 32) / 4) & 1;
      im[i * 3] = (unsigned char) (on ? a : 255 - a);
      im[i * 3 + 1] = (unsigned char) (on ? b : 40);
      im[i * 3 + 2] = (unsigned char) (on ? 30 : b);
    }
  GLuint t;
  glGenTextures (1, &t);
  glBindTexture (GL_TEXTURE_2D, t);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, im);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  return t;
}

static void
multi_units (GLuint t0, GLuint t1, GLuint t2, float shift)
{
  static const float rot[16] = { 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
  glActiveTexture (GL_TEXTURE2);
  glBindTexture (GL_TEXTURE_2D, t2);
  glEnable (GL_TEXTURE_2D);
  glMatrixMode (GL_TEXTURE);
  glLoadIdentity ();
  glMultMatrixf (rot);
  glMatrixMode (GL_MODELVIEW);
  glActiveTexture (GL_TEXTURE1);
  glBindTexture (GL_TEXTURE_2D, t1);
  glEnable (GL_TEXTURE_2D);
  glMatrixMode (GL_TEXTURE);
  glLoadIdentity ();
  glTranslatef (shift, 0, 0);
  glMatrixMode (GL_MODELVIEW);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, t0);
  glEnable (GL_TEXTURE_2D);
}

static void
multi_off (void)
{
  glActiveTexture (GL_TEXTURE1);
  glDisable (GL_TEXTURE_2D);
  glMatrixMode (GL_TEXTURE);
  glLoadIdentity ();
  glMatrixMode (GL_MODELVIEW);
  glActiveTexture (GL_TEXTURE2);
  glMatrixMode (GL_TEXTURE);
  glLoadIdentity ();
  glMatrixMode (GL_MODELVIEW);
  glDisable (GL_TEXTURE_2D);
  glActiveTexture (GL_TEXTURE0);
}

static void
multi (void)
{
  enum { N = 24 };
  static GLfloat vx[N][3], nm[N][3], tx[N][2], tx1[N][2], tx2[N][2];
  static GLushort ix[N];
  for (int i = 0; i < N; i++)
    {
      vx[i][0] = (float) (i / 2) / (N / 2) - 0.5f;
      vx[i][1] = (i & 1) ? 0.4f : -0.4f;
      vx[i][2] = 0;
      nm[i][0] = 0; nm[i][1] = 0; nm[i][2] = 1;
      tx[i][0] = (float) (i / 2) / 4; tx[i][1] = (float) (i & 1);
      tx1[i][0] = (float) (i / 2) / 7; tx1[i][1] = (float) (i & 1) * 0.5f;
      tx2[i][0] = (float) (i & 1); tx2[i][1] = (float) (i / 2) / 5;
      ix[i] = (GLushort) i;
    }
  GLuint t0 = checker_tex (250, 200), t1 = checker_tex (120, 230), t2 = checker_tex (60, 90);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glMatrixMode (GL_MODELVIEW);
  const char *what[] = { "car (immediate, units 0, 2, 1)", "track (arrays, units 0-2)", "both, unit 0 alone after" };
  for (int part = 0; part < 3; part++)
    {
      glClearColor (0.1f, 0.2f, 0.3f, 1);
      glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glLoadIdentity ();
      if (part != 1)
	{
	  glTranslatef (0, 0.45f, 0);
	  multi_units (t0, t1, t2, 0.25f);
	  glBegin (GL_TRIANGLE_STRIP);
	  glColor4f (1, 1, 1, 1);
	  for (int i = 0; i < N; i++)
	    {
	      glNormal3fv (nm[i]);
	      glMultiTexCoord2fv (GL_TEXTURE0, tx[i]);
	      glMultiTexCoord2fv (GL_TEXTURE2, tx2[i]);
	      glMultiTexCoord2fv (GL_TEXTURE1, tx1[i]);
	      glVertex3fv (vx[i]);
	    }
	  glEnd ();
	  multi_off ();
	  glLoadIdentity ();
	}
      if (part != 0)
	{
	  glTranslatef (0, -0.45f, 0);
	  multi_units (t0, t1, t2, 0.5f);
	  glColor4f (1, 1, 1, 1);
	  glPushClientAttrib (GL_CLIENT_VERTEX_ARRAY_BIT);
	  glEnableClientState (GL_NORMAL_ARRAY);
	  glNormalPointer (GL_FLOAT, 0, nm);
	  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	  glTexCoordPointer (2, GL_FLOAT, 0, tx);
	  glClientActiveTexture (GL_TEXTURE1);
	  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	  glTexCoordPointer (2, GL_FLOAT, 0, tx1);
	  glClientActiveTexture (GL_TEXTURE2);
	  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	  glTexCoordPointer (2, GL_FLOAT, 0, tx2);
	  glEnableClientState (GL_VERTEX_ARRAY);
	  glVertexPointer (3, GL_FLOAT, 0, vx);
	  glClientActiveTexture (GL_TEXTURE0);
	  glEnableClientState (GL_TEXTURE_COORD_ARRAY);
	  for (int k = 0; k < N - 2; k += 8)
	    glDrawElements (GL_TRIANGLE_STRIP, 10 < N - k ? 10 : N - k, GL_UNSIGNED_SHORT, ix + k);
	  glPopClientAttrib ();
	  multi_off ();
	  glLoadIdentity ();
	}
      if (part == 2)
	{
	  /* unit 0 alone, as the HUD would draw */
	  glTranslatef (0.6f, 0, 0);
	  glBegin (GL_QUADS);
	  glTexCoord2f (0, 0); glVertex2f (-0.2f, -0.1f);
	  glTexCoord2f (1, 0); glVertex2f (0.2f, -0.1f);
	  glTexCoord2f (1, 1); glVertex2f (0.2f, 0.1f);
	  glTexCoord2f (0, 1); glVertex2f (-0.2f, 0.1f);
	  glEnd ();
	}
      finish_frame ();
      printf ("multi %s: %08lx\n", what[part], frame_hash ());
    }
}

/* gl1test bigtex: large textures with every mipmap level given (as
   TORCS's loader does), RGB and RGBA, drawn at sizes that use several
   levels */
static void
bigtex (void)
{
  static const int sizes[] = { 1024, 512, 256 };
  static unsigned char lv[1024 * 1024 * 4];
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  for (int k = 0; k < 6; k++)
    {
      int size = sizes[k % 3], comp = k < 3 ? 4 : 3;
      GLuint t;
      glGenTextures (1, &t);
      glBindTexture (GL_TEXTURE_2D, t);
      glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
      for (int l = 0, w = size; w >= 1; l++, w >>= 1)
	{
	  /* each level its own colours, so the level used shows */
	  for (int i = 0; i < w * w; i++)
	    for (int c = 0; c < comp; c++)
	      lv[i * comp + c] = (unsigned char) (c == 3 ? 255 : ((i % w) / (w > 8 ? w / 8 : 1) * 37 + (i / w) / (w > 8 ? w / 8 : 1) * 11 + l * 50 + c * 80));
	  glTexImage2D (GL_TEXTURE_2D, l, comp, w, w, 0, comp == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, lv);
	}
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glEnable (GL_TEXTURE_2D);
      glClearColor (0, 0, 0, 1);
      glClear (GL_COLOR_BUFFER_BIT);
      float q[] = { 0.9f, 0.4f, 0.15f, 0.05f };
      for (int j = 0; j < 4; j++)
	{
	  float x = -0.95f + (float) j * 0.5f;
	  glBegin (GL_QUADS);
	  glTexCoord2f (0, 0); glVertex2f (x, -q[j]);
	  glTexCoord2f (1, 0); glVertex2f (x + q[j], -q[j]);
	  glTexCoord2f (1, 1); glVertex2f (x + q[j], q[j]);
	  glTexCoord2f (0, 1); glVertex2f (x, q[j]);
	  glEnd ();
	}
      finish_frame ();
      printf ("bigtex %dx%d %s: %08lx; error %x\n", size, size, comp == 4 ? "RGBA" : "RGB", frame_hash (), glGetError ());
      glDeleteTextures (1, &t);
    }
}

int
main (int argc, char **argv)
{
  /* (a line at a time: a stop part way still shows how far it got) */
  setvbuf (stdout, 0, _IOLBF, 0);
  const char *e = open_gl ();
  if (e)
    {
      printf ("gl1test (%s): %s\n", WHO, e);
      return 1;
    }
  printf ("gl1test (%s)\n", WHO);
  {
    /* (these differ from Linux's own, as they should: RISC3D says 2.1) */
    const char *x = (const char *) glGetString (GL_EXTENSIONS);
    int nx = 0;
    for (const char *p = x; p && *p; p++)
      nx += (p == x || p[-1] == ' ') && *p != ' ';
    printf ("version: %s; GLSL %s; %d extensions\n", (const char *) glGetString (GL_VERSION),
	    (const char *) glGetString (GL_SHADING_LANGUAGE_VERSION), nx);
  }
  if (argc > 1 && !strcmp (argv[1], "bigtex"))
    {
      bigtex ();
      if (argc > 2)
	save_ppm (argv[2]);
#ifndef DIRECT
      r3d_close ();
#endif
      return 0;
    }
  if (argc > 1 && !strcmp (argv[1], "multi"))
    {
      multi ();
      if (argc > 2)
	save_ppm (argv[2]);
#ifndef DIRECT
      r3d_close ();
#endif
      return 0;
    }
  if (argc > 1 && !strcmp (argv[1], "track"))
    {
      track (argc > 2 ? argv[2] : 0);
#ifndef DIRECT
      r3d_close ();
#endif
      return 0;
    }
  if (argc > 1 && !strcmp (argv[1], "bench"))
    {
      /* the processor alone, as a measure of this side: the same loop
	 in the RISC OS and Linux builds */
      float acc = 0;
      double c0 = now ();
      for (int i = 0; i < 100000000; i++)
	{
	  acc = acc * 0.999f + (float) (i & 255);
	  sink[i & 63] = acc;
	}
      printf ("processor: 100 million steps in %.2f s\n", now () - c0);
      bench ();
#ifndef DIRECT
      /* again, with the frames overlapped (r3d_swap_overlapped): first a
	 check that what comes back is the frame before (the first time,
	 this one) */
      overlapped = 1;
      static const GLfloat col[4][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 0 } };
      static const int expect[4] = { 0, 0, 1, 2 };
      int ok = 1;
      for (int i = 0; i < 4; i++)
	{
	  printf ("  overlapped frame %d\n", i + 1);
	  glClearColor (col[i][0], col[i][1], col[i][2], 1);
	  glClear (GL_COLOR_BUFFER_BIT);
	  finish_frame ();
	  const GLfloat *c = col[expect[i]];
	  if (frame[0] != (c[0] > 0 ? 255 : 0) || frame[1] != (c[1] > 0 ? 255 : 0) || frame[2] != (c[2] > 0 ? 255 : 0))
	    ok = 0;
	}
      glClearColor (0.1f, 0.12f, 0.3f, 1);
      printf ("frames overlapped: each one back a frame later: %s\n", ok ? "ok" : "WRONG");
      printf ("with frames overlapped:\n");
      bench ();
#endif
#ifndef DIRECT
      r3d_close ();
#endif
      return 0;
    }

  /* --- a texture, from a bigger image with skips and odd alignment ----- */
  enum { TW = 16, TH = 16, ROW = 21 };
  static unsigned char img[(ROW * (TH + 2)) * 3 + 8];
  for (int y = 0; y < TH + 2; y++)
    for (int x = 0; x < ROW; x++)
      {
	unsigned char *p = img + (y * ROW + x) * 3;
	p[0] = (unsigned char) (x * 12);
	p[1] = (unsigned char) (((x / 4) ^ (y / 4)) & 1 ? 255 : 40);
	p[2] = (unsigned char) (y * 14);
      }
  GLuint tex;
  glGenTextures (1, &tex);
  glBindTexture (GL_TEXTURE_2D, tex);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, ROW);
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, 3);
  glPixelStorei (GL_UNPACK_SKIP_ROWS, 2);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, TW, TH, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei (GL_UNPACK_SKIP_ROWS, 0);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  printf ("texture name: %u\n", tex);

  /* --- display lists: a transform, and a four-letter "font" ------------ */
  GLuint spin = glGenLists (1), font = glGenLists (4);
  printf ("list names: %u %u\n", spin, font);
  glNewList (spin, GL_COMPILE);
  glTranslatef (0.5f, 0.25f, 0);
  glRotatef (30, 0, 0, 1);
  quad (-0.2f, -0.2f, 0.4f);
  glEndList ();
  for (int i = 0; i < 4; i++)
    {
      glNewList (font + i, GL_COMPILE);
      glColor3f (i & 1 ? 1.0f : 0.3f, i & 2 ? 1.0f : 0.3f, 0.5f);
      quad (0, 0, 0.12f + 0.02f * i);
      glTranslatef (0.2f, 0, 0);
      glEndList ();
    }

  /* --- a buffer object, filled through glMapBuffer --------------------- */
  GLuint vbo;
  glGenBuffers (1, &vbo);
  glBindBuffer (GL_ARRAY_BUFFER, vbo);
  glBufferData (GL_ARRAY_BUFFER, 6 * sizeof (GLfloat) * 3, 0, GL_STATIC_DRAW);
  GLfloat *m = glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
  if (m)
    {
      static const GLfloat tri[18] = { -0.9f, -0.9f, 0, -0.6f, -0.9f, 0, -0.75f, -0.6f, 0,
				       -0.9f, 0.6f, 0, -0.6f, 0.6f, 0, -0.75f, 0.9f, 0 };
      memcpy (m, tri, sizeof tri);
    }
  printf ("glUnmapBuffer: %d (mapped: %s)\n", glUnmapBuffer (GL_ARRAY_BUFFER), m ? "yes" : "no");
  glBindBuffer (GL_ARRAY_BUFFER, 0);
  printf ("buffer name: %u\n", vbo);

  /* --- the frame ------------------------------------------------------- */
  glClearColor (0.1f, 0.12f, 0.3f, 1);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glFrustum (-1.333, 1.333, -1, 1, 1.5, 20);
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();
  glTranslated (0, 0, -3);
  glRotatef (20, 1, 0.5f, 0);
  show_matrix ("projection", GL_PROJECTION_MATRIX);
  show_matrix ("modelview", GL_MODELVIEW_MATRIX);

  /* lit, textured quads */
  glEnable (GL_DEPTH_TEST);
  glEnable (GL_LIGHTING);
  glEnable (GL_LIGHT0);
  static const GLfloat pos[4] = { 1, 2, 3, 0 }, dif[4] = { 1, 0.9f, 0.8f, 1 }, amb[4] = { 0.2f, 0.2f, 0.25f, 1 };
  glLightfv (GL_LIGHT0, GL_POSITION, pos);
  glLightfv (GL_LIGHT0, GL_DIFFUSE, dif);
  glLightModelfv (GL_LIGHT_MODEL_AMBIENT, amb);
  glEnable (GL_COLOR_MATERIAL);
  glColorMaterial (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glEnable (GL_TEXTURE_2D);
  glColor3ub (255, 200, 120);
  quad (-1.2f, -0.2f, 0.7f);
  glPushMatrix ();
  glCallList (spin);
  show_matrix ("after glCallList", GL_MODELVIEW_MATRIX);
  glPopMatrix ();
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_LIGHTING);

  /* the "font" */
  glPushMatrix ();
  glTranslatef (-0.9f, 0.7f, 0);
  glListBase (font);
  static const GLubyte text[6] = { 0, 1, 2, 3, 1, 0 };
  glCallLists (6, GL_UNSIGNED_BYTE, text);
  show_matrix ("after glCallLists", GL_MODELVIEW_MATRIX);
  GLint base = -1, depth = -1;
  glGetIntegerv (GL_LIST_BASE, &base);
  glGetIntegerv (GL_MODELVIEW_STACK_DEPTH, &depth);
  printf ("list base: %d (font %u), stack depth: %d\n", base, font, depth);
  glPopMatrix ();
  glListBase (0);

  /* vertex arrays in the program's memory: colours and indices */
  static const GLfloat va[] = { 0.3f, -0.9f, 0.5f, 0.9f, -0.9f, 0.5f, 0.9f, -0.4f, 0.5f, 0.3f, -0.4f, 0.5f };
  static const GLubyte vc[] = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255 };
  static const GLushort vi[] = { 0, 1, 2, 0, 2, 3 };
  glEnableClientState (GL_VERTEX_ARRAY);
  glEnableClientState (GL_COLOR_ARRAY);
  glVertexPointer (3, GL_FLOAT, 0, va);
  glColorPointer (4, GL_UNSIGNED_BYTE, 0, vc);
  glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, vi);
  glDisableClientState (GL_COLOR_ARRAY);

  /* interleaved */
  static const struct { GLubyte c[4]; GLfloat v[3]; } il[3] = {
    { { 255, 255, 255, 255 }, { -0.3f, -0.9f, 0.6f } },
    { { 0, 255, 128, 255 }, { 0.1f, -0.9f, 0.6f } },
    { { 255, 0, 255, 255 }, { -0.1f, -0.5f, 0.6f } } };
  glInterleavedArrays (GL_C4UB_V3F, 0, il);
  glDrawArrays (GL_TRIANGLES, 0, 3);
  glDisableClientState (GL_COLOR_ARRAY);

  /* glArrayElement, from the same arrays */
  glColor3f (1, 0.5f, 0);
  glVertexPointer (3, GL_FLOAT, 0, va);
  glPushMatrix ();
  glTranslatef (0, 1.0f, 0);
  glBegin (GL_LINE_LOOP);
  for (int i = 0; i < 4; i++)
    glArrayElement (i);
  glEnd ();
  glPopMatrix ();

  /* the buffer object */
  glBindBuffer (GL_ARRAY_BUFFER, vbo);
  glVertexPointer (3, GL_FLOAT, 0, 0);
  glBindBuffer (GL_ARRAY_BUFFER, 0);
  glColor3f (0.2f, 1, 0.3f);
  glDrawArrays (GL_TRIANGLES, 0, 6);
  glDisableClientState (GL_VERTEX_ARRAY);

  /* immediate mode collected by the library: a long strip, then the
     current colour it leaves; one that falls back (a normal first given
     after the first vertex); one with glMaterial inside (falls back) */
  glPushMatrix ();
  glTranslatef (0.9f, 0.6f, 0.2f);
  glScalef (0.3f, 0.3f, 1);
  glBegin (GL_TRIANGLE_STRIP);
  for (int i = 0; i < 24; i++)
    {
      glColor4ub ((GLubyte) (i * 10), (GLubyte) (255 - i * 10), 128, 255);
      glVertex2f ((float) (i / 2) * 0.1f - 0.6f, (float) (i % 2) * 0.3f);
    }
  glEnd ();
  GLfloat cc[4];
  /* long ones: triangles cut into draws (3000 vertices), and a strip
     gathered whole (3000 vertices) */
  glBegin (GL_TRIANGLES);
  for (int i = 0; i < 3000; i++)
    {
      float t = (float) i / 3000.0f;
      glColor3f (t, 1 - t, 0.5f);
      glVertex2f (-1.0f + 2.0f * t + (i % 3 == 1 ? 0.004f : 0), -1.4f + (i % 3 == 2 ? 0.05f : 0));
    }
  glEnd ();
  glBegin (GL_TRIANGLE_STRIP);
  for (int i = 0; i < 3000; i++)
    {
      float t = (float) (i / 2) / 1500.0f;
      glColor3f (0.5f, t, 1 - t);
      glVertex2f (-1.0f + 2.0f * t, 1.3f + (float) (i % 2) * 0.04f);
    }
  glEnd ();
  glGetFloatv (GL_CURRENT_COLOR, cc);
  printf ("current colour after a collected strip: %.4f %.4f %.4f %.4f\n", cc[0], cc[1], cc[2], cc[3]);
  glEnable (GL_LIGHTING);
  glBegin (GL_QUADS);
  for (int i = 0; i < 12; i++)
    {
      if (i == 5)
	glNormal3f (0.3f, 0.3f, 0.9f);	/* first given after a vertex */
      glColor3f (1, (float) i / 12, 0.2f);
      glVertex3f ((float) (i / 4) * 0.4f - 0.6f + (i % 4 == 1 || i % 4 == 2 ? 0.3f : 0),
		  -0.5f + (i % 4 >= 2 ? 0.3f : 0), 0);
    }
  glEnd ();
  glBegin (GL_TRIANGLES);
  for (int i = 0; i < 9; i++)
    {
      if (i == 3)
	{
	  static const GLfloat em[4] = { 0.2f, 0.0f, 0.4f, 1 };
	  glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, em);
	}
      glNormal3f (0, 0, 1);
      glColor3f (0.2f, 0.4f, (float) i / 9);
      glVertex2f ((float) (i / 3) * 0.5f - 0.7f + (i % 3 == 1 ? 0.4f : 0), -1.2f + (i % 3 == 2 ? 0.4f : 0));
    }
  glEnd ();
  static const GLfloat noem[4] = { 0, 0, 0, 1 };
  glMaterialfv (GL_FRONT_AND_BACK, GL_EMISSION, noem);
  glDisable (GL_LIGHTING);
  glGetFloatv (GL_CURRENT_NORMAL, cc);
  printf ("current normal after a fallen-back primitive: %.4f %.4f %.4f\n", cc[0], cc[1], cc[2]);
  glPopMatrix ();

  /* bitmaps and pixels, in window coordinates */
  glDisable (GL_DEPTH_TEST);
  static const GLubyte bm[16] = { 0xff, 0x00, 0x81, 0x00, 0x81, 0x00, 0xff, 0x00,
				  0x18, 0x00, 0x3c, 0x00, 0x7e, 0x00, 0xff, 0x00 };
  glColor3f (1, 1, 0);
  glWindowPos2i (10, 10);
  glPixelStorei (GL_UNPACK_ALIGNMENT, 2);
  glBitmap (8, 8, 0, 0, 10, 0, bm);
  glBitmap (8, 8, 0, 0, 10, 0, bm);
  static GLubyte px[12 * 6 * 4];
  for (int i = 0; i < 12 * 6; i++)
    {
      px[i * 4] = (GLubyte) (i * 3);
      px[i * 4 + 1] = 200;
      px[i * 4 + 2] = (GLubyte) (255 - i * 3);
      px[i * 4 + 3] = 255;
    }
  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
  glWindowPos2i (W - 20, 10);
  glDrawPixels (12, 6, GL_RGBA, GL_UNSIGNED_BYTE, px);

  /* state that programs read back */
  glPushAttrib (GL_TRANSFORM_BIT | GL_VIEWPORT_BIT);
  glMatrixMode (GL_PROJECTION);
  glViewport (5, 6, 70, 80);
  GLint mode = 0, vp[4];
  glGetIntegerv (GL_MATRIX_MODE, &mode);
  glGetIntegerv (GL_VIEWPORT, vp);
  printf ("inside glPushAttrib: mode &%X, viewport %d %d %d %d\n", (unsigned) mode, vp[0], vp[1], vp[2], vp[3]);
  glPopAttrib ();
  glGetIntegerv (GL_MATRIX_MODE, &mode);
  glGetIntegerv (GL_VIEWPORT, vp);
  printf ("after glPopAttrib: mode &%X, viewport %d %d %d %d\n", (unsigned) mode, vp[0], vp[1], vp[2], vp[3]);
  GLdouble pd[16];
  glGetDoublev (GL_TRANSPOSE_PROJECTION_MATRIX, pd);
  printf ("transpose projection (doubles): %.6g %.6g %.6g %.6g\n", pd[0], pd[2], pd[11], pd[14]);
  GLint im[16];
  glGetIntegerv (GL_MODELVIEW_MATRIX, im);
  printf ("modelview as integers: %d %d %d %d %d\n", im[0], im[5], im[10], im[14], im[15]);

  /* selection: which of two quads is under the middle */
  static GLuint sel[64];
  glSelectBuffer (64, sel);
  glRenderMode (GL_SELECT);
  glInitNames ();
  glPushName (7);
  quad (-0.2f, -0.2f, 0.4f);
  glLoadName (9);
  quad (2.0f, 2.0f, 0.4f);
  GLint hits = glRenderMode (GL_RENDER);
  printf ("selection: %d hit(s), first name %u\n", hits, hits > 0 ? sel[3] : 0);

  /* a read with pack settings */
  static GLubyte rd[40 * 4 + 64];
  memset (rd, 0xEE, sizeof rd);
  glPixelStorei (GL_PACK_ALIGNMENT, 1);
  glPixelStorei (GL_PACK_ROW_LENGTH, 20);
  glPixelStorei (GL_PACK_SKIP_PIXELS, 2);
  glReadPixels (W / 2 - 4, H / 2 - 1, 4, 2, GL_RGB, GL_UNSIGNED_BYTE, rd);
  glPixelStorei (GL_PACK_ROW_LENGTH, 0);
  glPixelStorei (GL_PACK_SKIP_PIXELS, 0);
  glPixelStorei (GL_PACK_ALIGNMENT, 4);
  unsigned sum = 0;
  for (size_t i = 0; i < sizeof rd; i++)
    sum = sum * 31 + rd[i];
  printf ("glReadPixels (row length 20, skip 2): first %d,%d,%d gap %d, sum %08x\n", rd[6], rd[7], rd[8], rd[0], sum);

  GLenum err = glGetError ();
  printf ("glGetError: &%X\n", err);
  finish_frame ();

  unsigned long h = 5381;
  for (size_t i = 0; i < sizeof frame; i += 4)
    h = h * 33 + frame[i] + frame[i + 1] * 7u + frame[i + 2] * 13u;
  printf ("picture: %08lx; middle %d,%d,%d\n", h & 0xffffffffUL, frame[((H / 2) * W + W / 2) * 4],
	  frame[((H / 2) * W + W / 2) * 4 + 1], frame[((H / 2) * W + W / 2) * 4 + 2]);
  if (argc > 1)
    {
      FILE *f = fopen (argv[1], "wb");
      if (f)
	{
	  fprintf (f, "P6 %d %d 255\n", W, H);
	  for (int i = 0; i < W * H; i++)
	    fwrite (frame + i * 4, 1, 3, f);
	  fclose (f);
	}
    }
#ifndef DIRECT
  r3d_close ();
#endif
  return 0;
}
