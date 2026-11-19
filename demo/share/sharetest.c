/* sharetest: contexts that share objects, drawing in two threads at once
   (RISC3D 0.12: r3d_create_shared, a current context per thread), as
   WebKit's Skia does. Prints each check and "sharetest ok" or
   "sharetest FAILED".

   1. r3d_can_share.
   2. Context A (GLES 2) in the main thread; context B sharing it, made in a
      second thread and current there.
   3. Each thread's current context is its own: A's viewport and B's
      differ, read back here in each thread (kept by the library).
   4. Names: both threads take 2000 texture names at once; none is given
      twice (one set of names for the group).
   5. A texture made and filled in B (then glFinish) is drawn in A.
   6. A fence made in B (glFlush, not glFinish) is waited for in A
      (glClientWaitSync and glWaitSync) before A draws B's next picture.
   7. Both threads draw 200 frames at once, each its own colour, each
      checking its own frame.
   8. A is destroyed while B lives: B still draws with the texture; a new
      context C sharing B's objects (the first context gone) sees it too.
   9. An unshared context D has names of its own; a context of another
      API can't share with B (refused).
   10. OpenGL (compatibility): a display list made in one context is
      called in another and moves the matrix the library keeps.

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "r3d_gl.h"
#define R3D_DIRECT
#include "r3d.h"

#define S 64
static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf ("  FAILED: " __VA_ARGS__); printf ("\n"); } } while (0)

static const char *vs_src =
  "attribute vec2 p;\n"
  "varying vec2 t;\n"
  "void main () { t = p * 0.5 + 0.5; gl_Position = vec4 (p, 0.0, 1.0); }\n";
static const char *fs_src =
  "precision mediump float;\n"
  "varying vec2 t;\n"
  "uniform sampler2D tex;\n"
  "void main () { gl_FragColor = texture2D (tex, t); }\n";

static GLuint
program (void)
{
  GLuint v = glCreateShader (GL_VERTEX_SHADER), f = glCreateShader (GL_FRAGMENT_SHADER), p = glCreateProgram ();
  glShaderSource (v, 1, &vs_src, 0);
  glShaderSource (f, 1, &fs_src, 0);
  glCompileShader (v);
  glCompileShader (f);
  glAttachShader (p, v);
  glAttachShader (p, f);
  glBindAttribLocation (p, 0, "p");
  glLinkProgram (p);
  GLint ok = 0;
  glGetProgramiv (p, GL_LINK_STATUS, &ok);
  CHECK (ok, "the program didn't link");
  return p;
}

/* the texture drawn over the whole surface; the middle pixel back */
static void
draw_texture (GLuint prog, GLuint tex, unsigned char px[4])
{
  static const GLfloat quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
  glViewport (0, 0, S, S);
  glClearColor (0, 0, 0, 1);
  glClear (GL_COLOR_BUFFER_BIT);
  glUseProgram (prog);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, tex);
  glUniform1i (glGetUniformLocation (prog, "tex"), 0);
  glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 0, quad);
  glEnableVertexAttribArray (0);
  glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
  glReadPixels (S / 2, S / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
}

static void
fill (GLuint tex, unsigned char r, unsigned char g, unsigned char b)
{
  unsigned char t[16 * 16 * 4];
  for (int i = 0; i < 16 * 16; i++)
    {
      t[i * 4] = r;
      t[i * 4 + 1] = g;
      t[i * 4 + 2] = b;
      t[i * 4 + 3] = 255;
    }
  glBindTexture (GL_TEXTURE_2D, tex);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, t);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static int
near (const unsigned char *p, int r, int g, int b)
{
  return abs (p[0] - r) <= 2 && abs (p[1] - g) <= 2 && abs (p[2] - b) <= 2;
}

/* --- the second thread: steps the main thread asks for ------------------- */

static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int step, done_step;
static struct r3d_context *ctx_a, *ctx_b;
static GLuint shared_tex, tex2;
static GLsync fence;
static GLuint names_b[2000];
static int frames_bad_b;

static void
wait_step (int s)
{
  pthread_mutex_lock (&mx);
  while (step < s)
    pthread_cond_wait (&cv, &mx);
  pthread_mutex_unlock (&mx);
}

static void
did (int s)
{
  pthread_mutex_lock (&mx);
  done_step = s;
  pthread_cond_broadcast (&cv);
  pthread_mutex_unlock (&mx);
}

static void
ask (int s)
{
  pthread_mutex_lock (&mx);
  step = s;
  pthread_cond_broadcast (&cv);
  while (done_step < s)
    pthread_cond_wait (&cv, &mx);
  pthread_mutex_unlock (&mx);
}

static void
ask_nowait (int s)
{
  pthread_mutex_lock (&mx);
  step = s;
  pthread_cond_broadcast (&cv);
  pthread_mutex_unlock (&mx);
}

static void
wait_done (int s)
{
  pthread_mutex_lock (&mx);
  while (done_step < s)
    pthread_cond_wait (&cv, &mx);
  pthread_mutex_unlock (&mx);
}

static GLint vp_b[4];

static void *
thread_b (void *arg)
{
  (void) arg;
  wait_step (1);
  ctx_b = r3d_create_shared (R3D_API_GLES2, 2, 0, ctx_a);
  if (ctx_b)
    {
      r3d_make_current (ctx_b);
      r3d_surface (S, S);
      glViewport (1, 2, 30, 40);
      glGetIntegerv (GL_VIEWPORT, vp_b);
    }
  did (1);
  if (!ctx_b)
    return 0;

  wait_step (2);			/* names, at the same time as A */
  glGenTextures (2000, names_b);
  did (2);

  wait_step (3);			/* a texture for A */
  glGenTextures (1, &shared_tex);
  fill (shared_tex, 200, 40, 90);
  glFinish ();
  did (3);

  wait_step (4);			/* another, with a fence (no glFinish) */
  glGenTextures (1, &tex2);
  fill (tex2, 30, 180, 220);
  {
    /* (work ahead of the fence, so that A's wait may reach the renderer
       before B's fence has: it must still wait for it) */
    static unsigned char big[1024 * 1024 * 4];
    GLuint t;
    glGenTextures (1, &t);
    for (int i = 0; i < 4; i++)
      {
	glBindTexture (GL_TEXTURE_2D, t);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_BYTE, big);
      }
    glBindTexture (GL_TEXTURE_2D, tex2);
  }
  fence = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush ();
  did (4);

  wait_step (5);			/* 200 frames, at the same time as A */
  GLuint prog = program ();
  GLuint own;
  glGenTextures (1, &own);
  fill (own, 10, 250, 10);
  for (int i = 0; i < 200; i++)
    {
      unsigned char px[4];
      draw_texture (prog, own, px);
      if (!near (px, 10, 250, 10))
	frames_bad_b++;
    }
  did (5);

  wait_step (6);			/* A has gone: B still draws the texture */
  {
    unsigned char px[4];
    draw_texture (prog, shared_tex, px);
    CHECK (near (px, 200, 40, 90), "after A was destroyed, B drew %d,%d,%d", px[0], px[1], px[2]);
    printf ("  B after A was destroyed: %d,%d,%d\n", px[0], px[1], px[2]);
  }
  did (6);

  wait_step (7);
  r3d_destroy (ctx_b);
  did (7);
  return 0;
}

int
main (void)
{
  printf ("sharetest: contexts sharing objects, in two threads\n");
  int can = r3d_can_share ();
  printf ("1. r3d_can_share: %d\n", can);
  if (!can)
    {
      printf ("sharetest: contexts can't share here (RISC3D 0.06, Linux3D 0.06 and rol-3d 0.20 are needed)\nsharetest FAILED\n");
      return 1;
    }

  ctx_a = r3d_create_version (R3D_API_GLES2, 2, 0);
  if (!ctx_a)
    {
      printf ("no context: %s\nsharetest FAILED\n", r3d_error ());
      return 1;
    }
  r3d_make_current (ctx_a);
  r3d_surface (S, S);
  printf ("   Renderer: %s\n", (const char *) glGetString (GL_RENDERER));
  GLuint prog = program ();

  pthread_t t;
  if (pthread_create (&t, 0, thread_b, 0))
    {
      printf ("no thread\nsharetest FAILED\n");
      return 1;
    }
  ask (1);
  printf ("2. B, sharing A's objects, made in a second thread: %s\n", ctx_b ? "yes" : r3d_error ());
  CHECK (ctx_b, "no shared context");
  if (!ctx_b)
    {
      printf ("sharetest FAILED\n");
      return 1;
    }

  glViewport (5, 6, 50, 60);
  GLint vp_a[4];
  glGetIntegerv (GL_VIEWPORT, vp_a);
  printf ("3. viewports: A %d,%d,%d,%d; B %d,%d,%d,%d\n", vp_a[0], vp_a[1], vp_a[2], vp_a[3], vp_b[0], vp_b[1], vp_b[2], vp_b[3]);
  CHECK (vp_a[2] == 50 && vp_b[2] == 30, "each thread's current context isn't its own");

  static GLuint names_a[2000];
  pthread_mutex_lock (&mx);
  step = 2;
  pthread_cond_broadcast (&cv);
  pthread_mutex_unlock (&mx);
  glGenTextures (2000, names_a);
  wait_done (2);
  int twice = 0;
  for (int i = 0; i < 2000; i++)
    for (int j = 0; j < 2000; j++)
      twice += names_a[i] == names_b[j];
  printf ("4. 4000 texture names taken in two threads at once: %d given twice\n", twice);
  CHECK (!twice, "names given twice");

  ask (3);
  unsigned char px[4];
  draw_texture (prog, shared_tex, px);
  printf ("5. B's texture %u drawn in A: %d,%d,%d (200,40,90 expected)\n", shared_tex, px[0], px[1], px[2]);
  CHECK (near (px, 200, 40, 90), "B's texture didn't reach A");

  ask (4);
  GLenum w = glClientWaitSync (fence, GL_SYNC_FLUSH_COMMANDS_BIT, 2000000000ull);
  glWaitSync (fence, 0, GL_TIMEOUT_IGNORED);
  draw_texture (prog, tex2, px);
  printf ("6. B's fence waited for in A: &%X; its texture: %d,%d,%d (30,180,220 expected)\n", w, px[0], px[1], px[2]);
  CHECK (w == GL_ALREADY_SIGNALED || w == GL_CONDITION_SATISFIED, "the fence wasn't seen");
  CHECK (near (px, 30, 180, 220), "B's second texture didn't reach A");
  glDeleteSync (fence);

  GLuint own;
  glGenTextures (1, &own);
  fill (own, 250, 250, 0);
  ask_nowait (5);
  int frames_bad_a = 0;
  for (int i = 0; i < 200; i++)
    {
      draw_texture (prog, own, px);
      if (!near (px, 250, 250, 0))
	frames_bad_a++;
    }
  wait_done (5);
  printf ("7. 200 frames each, both threads at once: %d wrong in A, %d in B\n", frames_bad_a, frames_bad_b);
  CHECK (!frames_bad_a && !frames_bad_b, "frames drawn at once were wrong");

  r3d_destroy (ctx_a);
  printf ("8. A destroyed\n");
  ask (6);
  struct r3d_context *c = r3d_create_shared (R3D_API_GLES2, 2, 0, ctx_b);
  CHECK (c, "no context C sharing B (%s)", r3d_error ());
  if (c)
    {
      r3d_make_current (c);
      r3d_surface (S, S);
      GLuint p2 = program ();
      draw_texture (p2, shared_tex, px);
      printf ("   C, sharing B's objects: %d,%d,%d\n", px[0], px[1], px[2]);
      CHECK (near (px, 200, 40, 90), "C didn't see the texture");
      r3d_destroy (c);
    }

  struct r3d_context *d = r3d_create_version (R3D_API_GLES2, 2, 0);
  CHECK (d, "no unshared context D");
  if (d)
    {
      r3d_make_current (d);
      GLuint n1;
      glGenTextures (1, &n1);
      printf ("9. unshared D's first texture name: %u\n", n1);
      CHECK (n1 == 1, "an unshared context's names should start at 1");
      r3d_destroy (d);
    }
  struct r3d_context *x = r3d_create_shared (R3D_API_GL, 2, 1, ctx_b);
  printf ("   OpenGL sharing with GLES: %s\n", x ? "made (wrong)" : r3d_error ());
  CHECK (!x, "a context of another API shared");
  if (x)
    r3d_destroy (x);
  ask (7);
  pthread_join (t, 0);

  /* 10: OpenGL, a display list made in one context, called in another */
  struct r3d_context *g1 = r3d_create_version (R3D_API_GL, 2, 1);
  struct r3d_context *g2 = g1 ? r3d_create_shared (R3D_API_GL, 2, 1, g1) : 0;
  CHECK (g1 && g2, "no OpenGL contexts (%s)", r3d_error ());
  if (g1 && g2)
    {
      r3d_make_current (g1);
      GLuint l = glGenLists (1);
      glNewList (l, GL_COMPILE);
      glTranslatef (3, 4, 5);
      glEndList ();
      glFinish ();
      r3d_make_current (g2);
      GLuint l2 = glGenLists (1);
      glMatrixMode (GL_MODELVIEW);
      glLoadIdentity ();
      glCallList (l);
      GLfloat m[16];
      glGetFloatv (GL_MODELVIEW_MATRIX, m);
      printf ("10. OpenGL: list %u made in one context, called in another: translation %g,%g,%g; next list %u\n", l,
	      m[12], m[13], m[14], l2);
      CHECK (m[12] == 3 && m[13] == 4 && m[14] == 5, "the list's matrix change wasn't seen");
      CHECK (l2 != l, "list names given twice");
    }
  if (g2)
    r3d_destroy (g2);
  if (g1)
    r3d_destroy (g1);
  printf (bad ? "sharetest FAILED\n" : "sharetest ok\n");
  return bad != 0;
}
