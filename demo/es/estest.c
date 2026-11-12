/* estest: GLES 1.1 and GLES 2's extensions through RISC3D (RISC3D 0.4).
   Every function is found by name, as riscos-mesa's EGL finds RISC3D's
   (r3d_gl_functions[], with its other names), or Mesa's own with -DDIRECT
   (eglGetProcAddress). Both builds print the same lines (bar the first two
   of each part) and the same pictures' sums, if RISC3D carries them right.

     estest [es1|es2]      both parts, or one

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "r3d_gl.h"

#define W 128
#define H 96
static unsigned char frame[W * H * 4];

typedef void (*fnp) (void);
static fnp find (const char *name);
static int missing;

/* the functions used, by name (the extension's names where it has them) */
#define FUNCS(X) \
  X (glGetString) X (glGetError) X (glViewport) X (glClear) X (glClearColor) X (glEnable) X (glDisable) \
  X (glGetIntegerv) X (glGetFloatv) X (glFinish) X (glReadPixels) X (glPixelStorei) \
  X (glMatrixMode) X (glLoadIdentity) X (glOrthof) X (glFrustumf) X (glRotatex) X (glTranslatex) \
  X (glScalex) X (glLoadMatrixx) X (glMultMatrixx) X (glPushMatrix) X (glPopMatrix) \
  X (glGetFixedv) X (glQueryMatrixxOES) X (glClearColorx) X (glColor4x) X (glNormal3x) \
  X (glLightxv) X (glLightModelx) X (glMaterialxv) X (glPointSizex) X (glLineWidthx) \
  X (glTexEnvx) X (glTexParameterx) X (glGetTexEnvxv) X (glGetLightxv) X (glClipPlanef) \
  X (glGetClipPlanef) X (glFogx) X (glFogxv) X (glAlphaFuncx) X (glDepthRangef) \
  X (glEnableClientState) X (glDisableClientState) X (glVertexPointer) X (glColorPointer) \
  X (glNormalPointer) X (glTexCoordPointer) X (glPointSizePointerOES) X (glDrawArrays) \
  X (glDrawElements) X (glGenTextures) X (glBindTexture) X (glTexImage2D) X (glGenBuffers) \
  X (glBindBuffer) X (glBufferData) X (glMapBufferOES) X (glUnmapBufferOES) \
  X (glGenVertexArraysOES) X (glBindVertexArrayOES) X (glDeleteVertexArraysOES) X (glIsVertexArrayOES) \
  X (glGenFramebuffersOES) X (glIsFramebufferOES) \
  X (glCreateShader) X (glShaderSource) X (glCompileShader) X (glCreateProgram) X (glAttachShader) \
  X (glBindAttribLocation) X (glLinkProgram) X (glUseProgram) X (glGetUniformLocation) X (glUniform4f) \
  X (glVertexAttribPointer) X (glEnableVertexAttribArray) X (glDisableVertexAttribArray) \
  X (glGetProgramiv) X (glDrawArraysInstancedEXT) X (glDrawElementsInstancedEXT) \
  X (glDrawElementsBaseVertexEXT) X (glDrawRangeElementsBaseVertexEXT) X (glMapBufferRangeEXT) \
  X (glFlushMappedBufferRangeEXT) X (glDebugMessageCallbackKHR) X (glDebugMessageInsertKHR) \
  X (glDebugMessageControlKHR) X (glGetDebugMessageLogKHR) X (glPushDebugGroupKHR) X (glPopDebugGroupKHR) \
  X (glObjectLabelKHR) X (glGetObjectLabelKHR) X (glTexImage3DOES) X (glDiscardFramebufferEXT) \
  X (glCreateShaderProgramvEXT) X (glGenProgramPipelinesEXT) X (glBindProgramPipelineEXT) \
  X (glUseProgramStagesEXT) X (glProgramUniform4fEXT) X (glGetProgramPipelineInfoLogEXT) \
  X (glValidateProgramPipelineEXT) X (glGetProgramPipelineivEXT) X (glTexParameterIivEXT) \
  X (glGetTexParameterIivEXT) X (glGetProgramBinaryOES) X (glGenQueriesEXT) X (glBeginQueryEXT) \
  X (glEndQueryEXT) X (glGetQueryObjectuivEXT) X (glCompressedTexImage2D) X (glTexParameteri) X (glTexCoordPointer)

#define DECL(n) static __typeof__ (n) *p_##n;
FUNCS (DECL)
#define LOAD(n) p_##n = (__typeof__ (n) *) find (#n);
static void
load_all (void)
{
  FUNCS (LOAD)
}
static void nothing (void) {}
static fnp chk (fnp f) { if (f) return f; missing++; return nothing; }
#define GL(n) ((__typeof__ (n) *) chk ((fnp) p_##n))

#if defined OSMESA
/* --- riscos-mesa's renderer (Mesa 20.3.5's OSMesa, built for the host) --- */
typedef struct osmesa_context *OSMesaContext;
OSMesaContext OSMesaCreateContextAttribs (const int *, OSMesaContext);
GLboolean OSMesaMakeCurrent (OSMesaContext, void *, GLenum, GLsizei, GLsizei);
void OSMesaDestroyContext (OSMesaContext);
fnp OSMesaGetProcAddress (const char *);
static OSMesaContext om;
static unsigned char osbuf[W * H * 4];
static fnp find (const char *name) { return OSMesaGetProcAddress (name); }

static const char *
open_api (int es1)
{
  const int att[] = { 0x22 /* FORMAT */, GL_RGBA, 0x30 /* DEPTH_BITS */, 24, 0x33 /* PROFILE */, es1 ? 0x1001 : 0x1002, 0 };
  om = OSMesaCreateContextAttribs (att, 0);
  if (!om || !OSMesaMakeCurrent (om, osbuf, GL_UNSIGNED_BYTE, W, H))
    return "no OSMesa context";
  load_all ();
  return 0;
}

static void close_api (void) { OSMesaDestroyContext (om); }

static void
grab (void)
{
  GL (glFinish) ();
  for (int y = 0; y < H; y++)
    memcpy (frame + (size_t) y * W * 4, osbuf + (size_t) (H - 1 - y) * W * 4, W * 4);
}
#define WHO "riscos-mesa (OSMesa)"
#elif defined DIRECT
/* --- Mesa directly: EGL, a pbuffer ------------------------------------ */
typedef void *EGLDisplay, *EGLConfig, *EGLContext, *EGLSurface;
typedef int EGLint;
EGLDisplay eglGetPlatformDisplay (unsigned, void *, const intptr_t *);
unsigned eglInitialize (EGLDisplay, EGLint *, EGLint *);
unsigned eglBindAPI (unsigned);
unsigned eglChooseConfig (EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
EGLContext eglCreateContext (EGLDisplay, EGLConfig, EGLContext, const EGLint *);
EGLSurface eglCreatePbufferSurface (EGLDisplay, EGLConfig, const EGLint *);
unsigned eglMakeCurrent (EGLDisplay, EGLSurface, EGLSurface, EGLContext);
unsigned eglDestroyContext (EGLDisplay, EGLContext);
fnp eglGetProcAddress (const char *);
static EGLDisplay dpy;
static EGLContext ctx;
static fnp find (const char *name) { return eglGetProcAddress (name); }

static const char *
open_api (int es1)
{
  const EGLint want[] = { 0x3040, es1 ? 1 : 4, 0x3033, 1, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3025, 24, 0x3038 };
  const EGLint ver[] = { 0x3098, es1 ? 1 : 2, 0x3038 };
  const EGLint pb[] = { 0x3057, W, 0x3056, H, 0x3038 };
  EGLConfig cfg;
  EGLint n = 0;
  if (!dpy)
    {
      dpy = eglGetPlatformDisplay (0x31DD, 0, 0);
      if (!dpy || !eglInitialize (dpy, 0, 0))
	return "no EGL";
    }
  eglBindAPI (0x30A0);
  if (!eglChooseConfig (dpy, want, &cfg, 1, &n) || n < 1)
    return "no config";
  ctx = eglCreateContext (dpy, cfg, 0, ver);
  EGLSurface s = eglCreatePbufferSurface (dpy, cfg, pb);
  if (!ctx || !s || !eglMakeCurrent (dpy, s, s, ctx))
    return "no context";
  load_all ();
  return 0;
}

static void close_api (void) { eglMakeCurrent (dpy, 0, 0, 0); eglDestroyContext (dpy, ctx); }

static void
grab (void)
{
  static unsigned char t[W * H * 4];
  GL (glPixelStorei) (GL_PACK_ALIGNMENT, 4);
  GL (glReadPixels) (0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, t);
  for (int y = 0; y < H; y++)
    memcpy (frame + (size_t) y * W * 4, t + (size_t) (H - 1 - y) * W * 4, W * 4);
}
#define WHO "Mesa directly"
#else
/* --- through RISC3D: the names riscos-mesa's EGL would look up ---------- */
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
open_api (int es1)
{
  rc = es1 ? r3d_create_es1 () : r3d_create ();
  if (!rc)
    return r3d_error ();
  r3d_make_current (rc);
  r3d_surface (W, H);
  load_all ();
  return 0;
}

static void close_api (void) { r3d_destroy (rc); rc = 0; }
static void grab (void) { r3d_swap (frame, sizeof frame); }
#define WHO "RISC3D"
#endif

static const char *picture_file;
static void
save_frame (const char *part)
{
  if (!picture_file)
    return;
  char name[256];
#ifdef __riscos__
  snprintf (name, sizeof name, "%s%s", picture_file, part);	/* (ESes1: no dots) */
#else
  snprintf (name, sizeof name, "%s-%s.ppm", picture_file, part);
#endif
  FILE *f = fopen (name, "wb");
  if (!f)
    return;
  fprintf (f, "P6 %d %d 255\n", W, H);
  for (size_t i = 0; i < sizeof frame; i += 4)
    fwrite (frame + i, 1, 3, f);
  fclose (f);
}

static uint32_t
sum_frame (void)
{
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < sizeof frame; i++)
    h = (h ^ frame[i]) * 16777619u;
  return h;
}

static void
show_x (const char *what, const GLfixed *v, int n)
{
  printf ("%s:", what);
  for (int i = 0; i < n; i++)
    printf (" %d", (int) v[i]);
  printf ("\n");
}

static void
show_f (const char *what, const GLfloat *v, int n)
{
  printf ("%s:", what);
  for (int i = 0; i < n; i++)
    printf (" %.5g", (double) v[i] + 0.0);
  printf ("\n");
}

static int
count_words (const char *s)
{
  int n = 0;
  for (const char *p = s; p && *p; p++)
    n += (p == s || p[-1] == ' ') && *p != ' ';
  return n;
}

#define X(v) ((GLfixed) ((v) * 65536))

/* --- GLES 1.1 ---------------------------------------------------------- */
static void
part_es1 (void)
{
  const char *e = open_api (1);
  if (e)
    {
      printf ("es1: no context (%s)\n", e);
      return;
    }
  printf ("es1 (%s)\n", WHO);
  printf ("version: %s; %d extensions\n", (const char *) GL (glGetString) (GL_VERSION),
	  count_words ((const char *) GL (glGetString) (GL_EXTENSIONS)));
  GL (glViewport) (0, 0, W, H);
  GL (glClearColorx) (X (0.1), X (0.12), X (0.3), X (1));
  GL (glClear) (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  GL (glMatrixMode) (GL_PROJECTION);
  GL (glLoadIdentity) ();
  GL (glFrustumf) (-1, 1, -0.75f, 0.75f, 1, 10);
  GL (glMatrixMode) (GL_MODELVIEW);
  GL (glLoadIdentity) ();
  GL (glTranslatex) (0, 0, X (-3));
  GL (glRotatex) (X (30), X (0.3), X (1), 0);
  GL (glScalex) (X (1.2), X (1.2), X (1.2));
  GLfixed mx[16];
  GLfloat mf[16];
  GL (glGetFixedv) (GL_MODELVIEW_MATRIX, mx);
  show_x ("modelview (fixed)", mx, 16);
  GL (glGetFloatv) (GL_PROJECTION_MATRIX, mf);
  show_f ("projection", mf, 16);
  GLfixed mant[16];
  GLint ex[16];
  GLbitfield st = GL (glQueryMatrixxOES) (mant, ex);
  printf ("query matrix: status %u, first %d e%d\n", (unsigned) st, (int) mant[0], (int) ex[0]);
  GL (glPushMatrix) ();
  static const GLfixed m2[16] = { X (1), 0, 0, 0, 0, X (1), 0, 0, 0, 0, X (1), 0, X (0.25), 0, 0, X (1) };
  GL (glMultMatrixx) (m2);
  GL (glGetFixedv) (GL_MODELVIEW_MATRIX, mx);
  show_x ("after glMultMatrixx", mx, 16);
  GL (glPopMatrix) ();
  GLfixed depth[1];
  GL (glGetFixedv) (GL_MODELVIEW_STACK_DEPTH, depth);
  printf ("stack depth (fixed): %d\n", (int) depth[0]);

  /* lit, from fixed-point arrays in the program's memory */
  GL (glEnable) (GL_DEPTH_TEST);
  GL (glEnable) (GL_LIGHTING);
  GL (glEnable) (GL_LIGHT0);
  GL (glEnable) (GL_NORMALIZE);	/* (riscos-mesa lights in object space when it can: unscaled normals) */
  const GLfixed lp[4] = { X (0.5), X (1), X (1), 0 }, amb[4] = { X (0.2), X (0.2), X (0.2), X (1) };
  GL (glLightxv) (GL_LIGHT0, GL_POSITION, lp);
  GL (glLightxv) (GL_LIGHT0, GL_AMBIENT, amb);
  GLfixed got[4];
  GL (glGetLightxv) (GL_LIGHT0, GL_AMBIENT, got);
  show_x ("light ambient", got, 4);
  const GLfixed mat[4] = { X (0.9), X (0.5), X (0.2), X (1) };
  GL (glMaterialxv) (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, mat);
  static const GLfixed tri[] = { X (-1), X (-1), 0, X (1), X (-1), 0, X (0), X (1), X (0.3),
				 X (-1), X (0.8), X (-0.5), X (1), X (0.9), X (-0.5), 0, X (-0.6), X (-0.5) };
  static const GLfixed nrm[] = { 0, 0, X (1), 0, 0, X (1), 0, X (0.3), X (0.95),
				 0, X (0.6), X (0.8), 0, X (0.6), X (0.8), 0, X (0.6), X (0.8) };
  GL (glEnableClientState) (GL_VERTEX_ARRAY);
  GL (glEnableClientState) (GL_NORMAL_ARRAY);
  GL (glVertexPointer) (3, GL_FIXED, 0, tri);
  GL (glNormalPointer) (GL_FIXED, 0, nrm);
  GL (glDrawArrays) (GL_TRIANGLES, 0, 6);
  GL (glDisableClientState) (GL_NORMAL_ARRAY);
  GL (glDisable) (GL_LIGHTING);

  /* points with sizes from an array (OES_point_size_array), colours as bytes */
  static const GLfloat pv[] = { -0.8f, -0.6f, 0.5f, -0.3f, -0.6f, 0.5f, 0.2f, -0.6f, 0.5f, 0.7f, -0.6f, 0.5f };
  static const GLubyte pc[] = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255 };
  static const GLfloat ps[] = { 2, 4, 6, 8 };
  GL (glEnableClientState) (GL_COLOR_ARRAY);
  GL (glEnableClientState) (0x8B9C /* GL_POINT_SIZE_ARRAY_OES */);
  GL (glVertexPointer) (3, GL_FLOAT, 0, pv);
  GL (glColorPointer) (4, GL_UNSIGNED_BYTE, 0, pc);
  GL (glPointSizePointerOES) (GL_FLOAT, 0, ps);
  GL (glDrawArrays) (GL_POINTS, 0, 4);
  GL (glDisableClientState) (0x8B9C);
  GL (glDisableClientState) (GL_COLOR_ARRAY);

  /* a vertex array object (OES_vertex_array_object) and a mapped buffer */
  GLuint vao = 0, vbo = 0;
  GL (glGenVertexArraysOES) (1, &vao);
  GL (glBindVertexArrayOES) (vao);
  GL (glGenBuffers) (1, &vbo);
  GL (glBindBuffer) (GL_ARRAY_BUFFER, vbo);
  GL (glBufferData) (GL_ARRAY_BUFFER, 6 * sizeof (GLfloat), 0, GL_STATIC_DRAW);
  GLfloat *mp = GL (glMapBufferOES) (GL_ARRAY_BUFFER, 0x88B9 /* GL_WRITE_ONLY_OES */);
  if (mp)
    {
      static const GLfloat q[] = { 0.4f, 0.4f, 0.9f, 0.4f, 0.65f, 0.8f };
      memcpy (mp, q, sizeof q);
    }
  printf ("mapped: %s, unmap %d\n", mp ? "yes" : "no", (int) GL (glUnmapBufferOES) (GL_ARRAY_BUFFER));
  GL (glEnableClientState) (GL_VERTEX_ARRAY);
  GL (glVertexPointer) (2, GL_FLOAT, 0, 0);
  GL (glBindBuffer) (GL_ARRAY_BUFFER, 0);
  GL (glBindVertexArrayOES) (0);
  /* object 0's arrays meanwhile: a strip from the program's memory */
  static const GLfloat strip[] = { -0.9f, 0.5f, -0.6f, 0.5f, -0.9f, 0.9f, -0.6f, 0.9f };
  GL (glVertexPointer) (2, GL_FLOAT, 0, strip);
  GL (glColor4x) (X (1), X (1), X (1), X (1));
  GL (glDisable) (GL_DEPTH_TEST);
  GL (glMatrixMode) (GL_MODELVIEW);
  GL (glLoadIdentity) ();
  GL (glMatrixMode) (GL_PROJECTION);
  GL (glLoadIdentity) ();
  GL (glOrthof) (-1, 1, -1, 1, -1, 1);
  GL (glDrawArrays) (GL_TRIANGLE_STRIP, 0, 4);
  GL (glBindVertexArrayOES) (vao);
  GL (glColor4x) (X (0.2), X (1), X (0.4), X (1));
  GL (glDrawArrays) (GL_TRIANGLES, 0, 3);
  GL (glBindVertexArrayOES) (0);
  printf ("vertex array object: %d (is %d)\n", vao != 0, (int) GL (glIsVertexArrayOES) (vao));
  GLfixed te = 0;
  GL (glTexEnvx) (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  GL (glGetTexEnvxv) (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &te);
  printf ("tex env mode: &%X\n", (unsigned) te);
  const GLfloat cp[4] = { 1, 0, 0, 0.5f };
  GLfloat cpg[4];
  GL (glClipPlanef) (GL_CLIP_PLANE0, cp);
  GL (glGetClipPlanef) (GL_CLIP_PLANE0, cpg);
  show_f ("clip plane (eye space)", cpg, 4);
  /* an ETC1 texture (riscos-mesa decodes it; so does RISC3D, for GLES 1.1) */
  static unsigned char etc[16 * 16 / 2];
  uint32_t seed = 12345;
  for (size_t i = 0; i < sizeof etc; i++)
    etc[i] = (unsigned char) ((seed = seed * 1103515245u + 12345u) >> 16);
  GLuint et;
  GL (glGenTextures) (1, &et);
  GL (glBindTexture) (GL_TEXTURE_2D, et);
  GL (glTexParameteri) (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  GL (glTexParameteri) (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  GL (glCompressedTexImage2D) (GL_TEXTURE_2D, 0, 0x8D64 /* GL_ETC1_RGB8_OES */, 16, 16, 0, sizeof etc, etc);
  printf ("ETC1 texture: error &%X\n", GL (glGetError) ());
  static const GLfloat tq[] = { 0.2f, -0.95f, 0.95f, -0.95f, 0.2f, -0.2f, 0.95f, -0.2f };
  static const GLfloat tc[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
  GL (glEnable) (GL_TEXTURE_2D);
  GL (glColor4x) (X (1), X (1), X (1), X (1));
  GL (glEnableClientState) (GL_TEXTURE_COORD_ARRAY);
  GL (glVertexPointer) (2, GL_FLOAT, 0, tq);
  GL (glTexCoordPointer) (2, GL_FLOAT, 0, tc);
  GL (glDrawArrays) (GL_TRIANGLE_STRIP, 0, 4);
  GL (glDisableClientState) (GL_TEXTURE_COORD_ARRAY);
  GL (glDisable) (GL_TEXTURE_2D);
  GLuint fbo = 0;
  GL (glGenFramebuffersOES) (1, &fbo);
  printf ("framebuffer object name: %s\n", fbo ? "yes" : "no");
  printf ("error: &%X\n", GL (glGetError) ());
  grab ();
  save_frame ("es1");
  printf ("picture: %08x\n", sum_frame ());
  close_api ();
}

/* --- GLES 2's extensions ------------------------------------------------ */
static GLuint
program (const char *vs, const char *fs)
{
  GLuint v = GL (glCreateShader) (GL_VERTEX_SHADER), f = GL (glCreateShader) (GL_FRAGMENT_SHADER), p = GL (glCreateProgram) ();
  GL (glShaderSource) (v, 1, &vs, 0);
  GL (glShaderSource) (f, 1, &fs, 0);
  GL (glCompileShader) (v);
  GL (glCompileShader) (f);
  GL (glAttachShader) (p, v);
  GL (glAttachShader) (p, f);
  GL (glBindAttribLocation) (p, 0, "pos");
  GL (glLinkProgram) (p);
  GLint ok = 0;
  GL (glGetProgramiv) (p, GL_LINK_STATUS, &ok);
  printf ("program linked: %d\n", ok);
  return p;
}

static int debug_calls;
static char debug_last[128];
static void
on_debug (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user)
{
  (void) source; (void) type; (void) severity; (void) user;
  if (id == 77)
    {
      debug_calls++;
      snprintf (debug_last, sizeof debug_last, "%.*s", (int) length, message);
    }
}

static void
part_es2 (void)
{
  const char *e = open_api (0);
  if (e)
    {
      printf ("es2: no context (%s)\n", e);
      return;
    }
  printf ("es2 (%s)\n", WHO);
  printf ("version: %s\n", (const char *) GL (glGetString) (GL_VERSION));
  GL (glViewport) (0, 0, W, H);
  GL (glClearColor) (0.05f, 0.1f, 0.2f, 1);
  GL (glClear) (GL_COLOR_BUFFER_BIT);
  GLuint p = program ("attribute vec2 pos; uniform vec4 off; void main () { gl_Position = vec4 (pos * 0.3 + off.xy, 0.0, 1.0); }",
		      "precision mediump float; uniform vec4 col; void main () { gl_FragColor = col; }");
  GL (glUseProgram) (p);
  GLint off = GL (glGetUniformLocation) (p, "off"), col = GL (glGetUniformLocation) (p, "col");

  /* base vertex, indices and vertices in the program's memory */
  static const GLfloat v[] = { 9, 9, 9, 9, -1, -1, 1, -1, 0, 1, -1, 1, 1, 1 };
  static const GLushort ix[] = { 0, 1, 2 };
  GL (glEnableVertexAttribArray) (0);
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, v);
  GL (glUniform4f) (off, -0.6f, -0.5f, 0, 0);
  GL (glUniform4f) (col, 1, 0.3f, 0.2f, 1);
  GL (glDrawElementsBaseVertexEXT) (GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, ix, 2);
  GL (glUniform4f) (off, 0, -0.5f, 0, 0);
  GL (glUniform4f) (col, 0.3f, 1, 0.3f, 1);
  GL (glDrawRangeElementsBaseVertexEXT) (GL_TRIANGLES, 0, 2, 3, GL_UNSIGNED_SHORT, ix, 3);

  /* a vertex array object with a buffer filled through a mapped range */
  GLuint vao = 0, vbo = 0, ibo = 0;
  GL (glGenVertexArraysOES) (1, &vao);
  GL (glBindVertexArrayOES) (vao);
  GL (glGenBuffers) (1, &vbo);
  GL (glGenBuffers) (1, &ibo);
  GL (glBindBuffer) (GL_ARRAY_BUFFER, vbo);
  GL (glBufferData) (GL_ARRAY_BUFFER, 16 * sizeof (GLfloat), 0, GL_DYNAMIC_DRAW);
  GLfloat *m = GL (glMapBufferRangeEXT) (GL_ARRAY_BUFFER, 0, 16 * sizeof (GLfloat), GL_MAP_WRITE_BIT_EXT | GL_MAP_FLUSH_EXPLICIT_BIT_EXT);
  if (m)
    {
      static const GLfloat q[] = { -1, -1, 1, -1, -1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1 };
      memcpy (m, q, sizeof q);
      GL (glFlushMappedBufferRangeEXT) (GL_ARRAY_BUFFER, 0, 16 * sizeof (GLfloat));
    }
  printf ("mapped range: %s, unmap %d\n", m ? "yes" : "no", (int) GL (glUnmapBufferOES) (GL_ARRAY_BUFFER));
  m = GL (glMapBufferRangeEXT) (GL_ARRAY_BUFFER, 8 * sizeof (GLfloat), 4 * sizeof (GLfloat), GL_MAP_READ_BIT_EXT);
  if (m)
    show_f ("read back through a mapped range", m, 4);
  GL (glUnmapBufferOES) (GL_ARRAY_BUFFER);
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  GL (glEnableVertexAttribArray) (0);
  static const GLushort q6[] = { 0, 1, 2, 2, 1, 3 };
  GL (glBindBuffer) (GL_ELEMENT_ARRAY_BUFFER, ibo);
  GL (glBufferData) (GL_ELEMENT_ARRAY_BUFFER, sizeof q6, q6, GL_STATIC_DRAW);
  GL (glBindVertexArrayOES) (0);
  /* object 0 meanwhile: its attribute array is the program's memory one */
  GL (glUniform4f) (off, 0.6f, -0.5f, 0, 0);
  GL (glUniform4f) (col, 0.3f, 0.4f, 1, 1);
  GL (glDrawArrays) (GL_TRIANGLES, 4, 3);
  GL (glBindVertexArrayOES) (vao);
  GL (glUniform4f) (off, -0.6f, 0.4f, 0, 0);
  GL (glUniform4f) (col, 1, 1, 0.3f, 1);
  GL (glDrawElements) (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
  GL (glUniform4f) (off, 0, 0.4f, 0, 0);
  GL (glUniform4f) (col, 1, 0.5f, 1, 1);
  GL (glDrawElementsBaseVertexEXT) (GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (const void *) 0, 4);
  GL (glBindVertexArrayOES) (0);

  /* instances (EXT_draw_instanced) */
  GLuint pi = program ("#extension GL_EXT_draw_instanced : require\n"
		       "attribute vec2 pos; void main () { gl_Position = vec4 (pos * 0.12 + vec2 (0.4 + float (gl_InstanceIDEXT) * 0.25, 0.4), 0.0, 1.0); }",
		       "precision mediump float; void main () { gl_FragColor = vec4 (0.9, 0.9, 0.9, 1.0); }");
  GL (glUseProgram) (pi);
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, v + 4);
  GL (glDrawArraysInstancedEXT) (GL_TRIANGLES, 0, 3, 2);
  GL (glDrawElementsInstancedEXT) (GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, ix, 1);

  /* separate programs (EXT_separate_shader_objects) */
  const char *vs = "#extension GL_EXT_separate_shader_objects : enable\n"
		   "attribute vec2 pos; void main () { gl_Position = vec4 (pos * 0.15 + vec2 (-0.8, 0.8), 0.0, 1.0); }";
  const char *fs = "#extension GL_EXT_separate_shader_objects : enable\n"
		   "precision mediump float; uniform vec4 c; void main () { gl_FragColor = c; }";
  GLuint sv = GL (glCreateShaderProgramvEXT) (GL_VERTEX_SHADER, 1, &vs);
  GLuint sf = GL (glCreateShaderProgramvEXT) (GL_FRAGMENT_SHADER, 1, &fs);
  GLuint pipe = 0;
  GL (glGenProgramPipelinesEXT) (1, &pipe);
  GL (glUseProgramStagesEXT) (pipe, GL_VERTEX_SHADER_BIT_EXT, sv);
  GL (glUseProgramStagesEXT) (pipe, GL_FRAGMENT_SHADER_BIT_EXT, sf);
  GL (glProgramUniform4fEXT) (sf, GL (glGetUniformLocation) (sf, "c"), 0.2f, 0.9f, 0.9f, 1);
  GL (glUseProgram) (0);
  GL (glBindProgramPipelineEXT) (pipe);
  GL (glValidateProgramPipelineEXT) (pipe);
  GLint valid = -1;
  GL (glGetProgramPipelineivEXT) (pipe, GL_VALIDATE_STATUS, &valid);
  char log[256];
  GLsizei ll = -1;
  GL (glGetProgramPipelineInfoLogEXT) (pipe, sizeof log, &ll, log);
  printf ("separate programs: %d %d, pipeline valid %d, log %s\n", sv != 0, sf != 0, valid, ll >= 0 ? "read" : "not read");
  GL (glDrawArrays) (GL_TRIANGLES, 4, 3);
  GL (glBindProgramPipelineEXT) (0);

  /* debug output (KHR_debug): the callback and labels */
  GL (glEnable) (0x92E0 /* GL_DEBUG_OUTPUT_KHR */);
  GL (glDebugMessageCallbackKHR) (on_debug, 0);
  GL (glDebugMessageInsertKHR) (GL_DEBUG_SOURCE_APPLICATION_KHR, GL_DEBUG_TYPE_MARKER_KHR, 77,
				GL_DEBUG_SEVERITY_NOTIFICATION_KHR, -1, "hello from the program");
  GL (glPushDebugGroupKHR) (GL_DEBUG_SOURCE_APPLICATION_KHR, 5, -1, "group");
  GL (glPopDebugGroupKHR) ();
  GL (glObjectLabelKHR) (GL_PROGRAM_KHR, p, -1, "the program");
  char label[64] = "";
  GLsizei lab = 0;
  GL (glGetObjectLabelKHR) (GL_PROGRAM_KHR, p, sizeof label, &lab, label);
  GL (glGetError) ();			/* (a wait: the callback's messages come) */
  printf ("debug: label \"%s\" (%d), callback %d \"%s\"\n", label, (int) lab, debug_calls, debug_last);
  GL (glDebugMessageCallbackKHR) (0, 0);
  GL (glDebugMessageInsertKHR) (GL_DEBUG_SOURCE_APPLICATION_KHR, GL_DEBUG_TYPE_MARKER_KHR, 78,
				GL_DEBUG_SEVERITY_NOTIFICATION_KHR, 5, "kept!");
  GLenum src[4], typ[4], sev[4];
  GLuint id[4];
  GLsizei len[4];
  char text[256];
  GLuint got = GL (glGetDebugMessageLogKHR) (4, sizeof text, src, typ, id, sev, len, text);
  GLuint i78 = 0;
  for (GLuint i = 0; i < got; i++)
    if (id[i] == 78)
      i78 = 1;
  printf ("debug log: %s\n", i78 ? "has the message" : "hasn't the message");
  GL (glDisable) (0x92E0);

  /* the rest: 3D textures, border colours, a query, discarding, binaries */
  GLuint t3;
  GL (glGenTextures) (1, &t3);
  GL (glBindTexture) (0x806F /* GL_TEXTURE_3D_OES */, t3);
  static unsigned char vox[2 * 2 * 2 * 4];
  GL (glTexImage3DOES) (0x806F, 0, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, vox);
  printf ("3D texture: error &%X\n", GL (glGetError) ());
  GLuint t2;
  GL (glGenTextures) (1, &t2);
  GL (glBindTexture) (GL_TEXTURE_2D, t2);
  const GLint border[4] = { 1, 2, 3, 4 };
  GLint bg[4] = { 0, 0, 0, 0 };
  GL (glTexParameterIivEXT) (GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR_EXT, border);
  GL (glGetTexParameterIivEXT) (GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR_EXT, bg);
  printf ("border colour: %d %d %d %d\n", bg[0], bg[1], bg[2], bg[3]);
  GLuint q = 0, qr = 7;
  GL (glGenQueriesEXT) (1, &q);
  GL (glBeginQueryEXT) (GL_ANY_SAMPLES_PASSED_EXT, q);
  GL (glUseProgram) (p);
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, v);
  GL (glDrawArrays) (GL_TRIANGLES, 4, 3);
  GL (glEndQueryEXT) (GL_ANY_SAMPLES_PASSED_EXT);
  GL (glGetQueryObjectuivEXT) (q, GL_QUERY_RESULT_EXT, &qr);
  printf ("any samples passed: %u\n", qr);
  GLint nbin = -1;
  GL (glGetIntegerv) (GL_NUM_PROGRAM_BINARY_FORMATS_OES, &nbin);
  GLsizei bl = -1;
  GLenum bf = 0;
  static char bin[65536];
  GL (glGetProgramBinaryOES) (p, sizeof bin, &bl, &bf, bin);
  printf ("program binary: formats %s, read %s\n", nbin > 0 ? "some" : "none", bl > 0 || nbin <= 0 ? "ok" : "no");
  printf ("error: &%X\n", GL (glGetError) ());
  grab ();
  save_frame ("es2");
  printf ("picture: %08x\n", sum_frame ());
  const GLenum disc[1] = { GL_COLOR_EXT };
  GL (glDiscardFramebufferEXT) (GL_FRAMEBUFFER, 1, disc);
  close_api ();
}

int
main (int argc, char **argv)
{
  int which = argc > 1 && strcmp (argv[1], "both") ? (!strcmp (argv[1], "es1") ? 1 : 2) : 3;
  picture_file = argc > 2 ? argv[2] : 0;
  if (which & 1)
    part_es1 ();
  if (which & 2)
    part_es2 ();
  if (missing)
    printf ("functions not found: %d\n", missing);
  return 0;
}
