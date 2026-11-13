/* gl3test: OpenGL ES 3.0/3.1 and OpenGL 3.0/3.1 through RISC3D (RISC3D
   0.5). Every function is found by name, as riscos-mesa's EGL finds
   RISC3D's (r3d_gl_functions[]), or Mesa's own with -DDIRECT
   (eglGetProcAddress). Both builds print the same lines (bar the version
   lines) and the same pictures' sums, if RISC3D carries them right.

     gl3test [es3|gl3|both] [picture]

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

#define FUNCS(X) \
  X (glGetString) X (glGetStringi) X (glGetError) X (glViewport) X (glClear) X (glClearColor) X (glEnable) \
  X (glDisable) X (glGetIntegerv) X (glGetInteger64v) X (glGetIntegeri_v) X (glGetInteger64i_v) \
  X (glGetBooleani_v) X (glFinish) X (glFlush) X (glReadPixels) X (glPixelStorei) \
  X (glCreateShader) X (glShaderSource) X (glCompileShader) X (glGetShaderiv) X (glGetShaderInfoLog) \
  X (glCreateProgram) X (glAttachShader) X (glLinkProgram) X (glGetProgramiv) X (glGetProgramInfoLog) \
  X (glUseProgram) X (glGetUniformLocation) X (glUniform4f) X (glUniform1i) X (glUniform1ui) \
  X (glGetUniformuiv) X (glBindAttribLocation) \
  X (glGenBuffers) X (glBindBuffer) X (glBufferData) X (glBufferSubData) X (glMapBufferRange) \
  X (glUnmapBuffer) X (glCopyBufferSubData) X (glBindBufferBase) X (glBindBufferRange) \
  X (glGenVertexArrays) X (glBindVertexArray) X (glDeleteVertexArrays) \
  X (glVertexAttribPointer) X (glVertexAttribIPointer) X (glEnableVertexAttribArray) \
  X (glDisableVertexAttribArray) X (glVertexAttribDivisor) X (glVertexAttribI4i) X (glGetVertexAttribIiv) \
  X (glDrawArrays) X (glDrawElements) X (glDrawArraysInstanced) X (glDrawElementsInstanced) \
  X (glDrawArraysIndirect) X (glDrawElementsIndirect) X (glDrawRangeElements) \
  X (glGetUniformBlockIndex) X (glUniformBlockBinding) X (glGetActiveUniformBlockiv) \
  X (glGetActiveUniformBlockName) X (glGetUniformIndices) X (glGetActiveUniformsiv) \
  X (glGetActiveUniformName) \
  X (glTransformFeedbackVaryings) X (glGetTransformFeedbackVarying) X (glBeginTransformFeedback) \
  X (glEndTransformFeedback) X (glGenTransformFeedbacks) X (glBindTransformFeedback) \
  X (glGenQueries) X (glBeginQuery) X (glEndQuery) X (glGetQueryObjectuiv) \
  X (glFenceSync) X (glClientWaitSync) X (glWaitSync) X (glGetSynciv) X (glIsSync) X (glDeleteSync) \
  X (glObjectPtrLabel) X (glGetObjectPtrLabel) \
  X (glGenSamplers) X (glBindSampler) X (glSamplerParameteri) X (glGetSamplerParameteriv) X (glDeleteSamplers) \
  X (glClearBufferfv) X (glClearBufferfi) X (glClearBufferiv) \
  X (glGenTextures) X (glBindTexture) X (glTexStorage2D) X (glTexSubImage2D) X (glTexImage2D) \
  X (glTexParameteri) X (glActiveTexture) X (glTexBuffer) \
  X (glGenFramebuffers) X (glBindFramebuffer) X (glFramebufferTexture2D) X (glCheckFramebufferStatus) \
  X (glBlitFramebuffer) X (glInvalidateFramebuffer) X (glReadBuffer) X (glDrawBuffers) \
  X (glGenRenderbuffers) X (glBindRenderbuffer) X (glRenderbufferStorage) X (glFramebufferRenderbuffer) \
  X (glGetInternalformativ) \
  X (glDispatchCompute) X (glMemoryBarrier) X (glGetProgramResourceIndex) X (glGetProgramResourceName) \
  X (glGetProgramResourceiv) X (glGetProgramInterfaceiv) X (glGetProgramResourceLocation) \
  X (glBindFragDataLocation) X (glGetFragDataLocation) X (glPrimitiveRestartIndex) \
  X (glColorMaski) X (glEnablei) X (glDisablei) X (glIsEnabledi) X (glBeginConditionalRender) X (glEndConditionalRender) \
  X (glMatrixMode) X (glLoadIdentity) X (glColor4f) X (glBegin) X (glEnd) X (glVertex2f)

#define DECL(n) static __typeof__ (n) *p_##n;
FUNCS (DECL)
#define LOAD(n) p_##n = (__typeof__ (n) *) find (#n);
static void load_all (void) { FUNCS (LOAD) }
static void nothing (void) {}
static fnp chk (fnp f) { if (f) return f; missing++; return nothing; }
#define GL(n) ((__typeof__ (n) *) chk ((fnp) p_##n))

#if defined DIRECT
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
open_api (int desk)
{
  const EGLint want[] = { 0x3040, desk ? 8 : 0x40, 0x3033, 1, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3025, 24, 0x3026, 8, 0x3038 };
  const EGLint ver_es[] = { 0x3098, 3, 0x30FB, 1, 0x3038 };
  const EGLint pb[] = { 0x3057, W, 0x3056, H, 0x3038 };
  EGLConfig cfg;
  EGLint n = 0;
  if (!dpy)
    {
      dpy = eglGetPlatformDisplay (0x31DD, 0, 0);
      if (!dpy || !eglInitialize (dpy, 0, 0))
	return "no EGL";
    }
  eglBindAPI (desk ? 0x30A2 : 0x30A0);
  if (!eglChooseConfig (dpy, want, &cfg, 1, &n) || n < 1)
    return "no config";
  ctx = eglCreateContext (dpy, cfg, 0, desk ? 0 : ver_es);
  EGLSurface s = eglCreatePbufferSurface (dpy, cfg, pb);
  if (!ctx || !s || !eglMakeCurrent (dpy, s, s, ctx))
    return "no context";
  load_all ();
  /* (as rol-3d: OpenGL 3.1 without the compatibility profile if that is
     older, as Mesa's v3d's) */
  const char *v = (const char *) GL (glGetString) (GL_VERSION);
  if (desk && v && strncmp (v, "3.1", 3) < 0)
    {
      const EGLint v31[] = { 0x3098, 3, 0x30FB, 1, 0x3038 };
      eglMakeCurrent (dpy, 0, 0, 0);
      eglDestroyContext (dpy, ctx);
      ctx = eglCreateContext (dpy, cfg, 0, v31);
      if (!ctx || !eglMakeCurrent (dpy, s, s, ctx))
	return "no OpenGL 3.1 context";
      load_all ();
    }
  return 0;
}

static void close_api (void) { eglMakeCurrent (dpy, 0, 0, 0); eglDestroyContext (dpy, ctx); }

static void
grab (void)
{
  static unsigned char t[W * H * 4];
  GL (glBindFramebuffer) (GL_FRAMEBUFFER, 0);
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
open_api (int desk)
{
  rc = r3d_create_version (desk ? R3D_API_GL : R3D_API_GLES2, 3, desk ? 1 : 1);
  if (!rc)
    return r3d_error ();
  r3d_make_current (rc);
  r3d_surface (W, H);
  load_all ();
  return 0;
}

static void close_api (void) { r3d_destroy (rc); rc = 0; }
static void grab (void) { GL (glBindFramebuffer) (GL_FRAMEBUFFER, 0); r3d_swap (frame, sizeof frame); }
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
  snprintf (name, sizeof name, "%s%s", picture_file, part);
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

static int
count_words (const char *s)
{
  int n = 0;
  for (const char *p = s; p && *p; p++)
    n += (p == s || p[-1] == ' ') && *p != ' ';
  return n;
}

static GLuint
shader (GLenum type, const char *src)
{
  GLuint s = GL (glCreateShader) (type);
  GL (glShaderSource) (s, 1, &src, 0);
  GL (glCompileShader) (s);
  GLint ok = 0;
  GL (glGetShaderiv) (s, GL_COMPILE_STATUS, &ok);
  if (!ok)
    {
      char log[1024] = "";
      GL (glGetShaderInfoLog) (s, sizeof log, 0, log);
      printf ("shader: %s\n", log);
    }
  return s;
}

/* a program; varying: a transform feedback output to capture, or 0 */
static GLuint
program2 (const char *vs, const char *fs, const char *varying)
{
  GLuint p = GL (glCreateProgram) ();
  GL (glAttachShader) (p, shader (GL_VERTEX_SHADER, vs));
  if (fs)
    GL (glAttachShader) (p, shader (GL_FRAGMENT_SHADER, fs));
  if (varying)
    GL (glTransformFeedbackVaryings) (p, 1, &varying, GL_INTERLEAVED_ATTRIBS);
  GL (glBindAttribLocation) (p, 0, "pos");
  GL (glLinkProgram) (p);
  GLint ok = 0;
  GL (glGetProgramiv) (p, GL_LINK_STATUS, &ok);
  if (!ok)
    {
      char log[1024] = "";
      GL (glGetProgramInfoLog) (p, sizeof log, 0, log);
      printf ("link: %s\n", log);
    }
  return p;
}

static int compat;			/* GL_ARB_compatibility */

static void
versions (void)
{
  GLint ma = -1, mi = -1, ne = -1;
  GL (glGetIntegerv) (GL_MAJOR_VERSION, &ma);
  GL (glGetIntegerv) (GL_MINOR_VERSION, &mi);
  GL (glGetIntegerv) (GL_NUM_EXTENSIONS, &ne);
  printf ("version: %s; GLSL %s\n", (const char *) GL (glGetString) (GL_VERSION),
	  (const char *) GL (glGetString) (GL_SHADING_LANGUAGE_VERSION));
  printf ("version numbers: %d.%d\n", ma, mi);
  /* the extensions one at a time are the same as the string */
  const char *all = (const char *) GL (glGetString) (GL_EXTENSIONS);
  int same = ne == count_words (all);
  for (GLint i = 0; same && i < ne; i++)
    {
      const char *e = (const char *) GL (glGetStringi) (GL_EXTENSIONS, (GLuint) i);
      size_t l = e ? strlen (e) : 0;
      same = 0;
      for (const char *at = all; e && at && (at = strstr (at, e)) != 0; at += l)
	if ((at == all || at[-1] == ' ') && (at[l] == ' ' || at[l] == 0))
	  same = 1;
    }
  printf ("extensions one at a time: %s\n", !all ? "only those (no string)" : same ? "the same" : "different");
  compat = 0;
  for (GLint i = 0; i < ne; i++)
    {
      const char *x = (const char *) GL (glGetStringi) (GL_EXTENSIONS, (GLuint) i);
      if (x && !strcmp (x, "GL_ARB_compatibility"))
	compat = 1;
    }
  GL (glGetError) ();			/* (GL_EXTENSIONS's, without the compatibility profile) */
}

static const GLfloat tri[] = { -1, -1, 1, -1, -1, 1, 1, 1 };

/* --- OpenGL ES 3.0 and 3.1 --------------------------------------------- */
static void
part_es3 (void)
{
  const char *e = open_api (0);
  if (e)
    {
      printf ("es3: no context (%s)\n", e);
      return;
    }
  printf ("es3 (%s)\n", WHO);
  versions ();
  GL (glViewport) (0, 0, W, H);
  GL (glClearColor) (0.1f, 0.2f, 0.3f, 1);
  GL (glClear) (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  const GLfloat cc[4] = { 0.2f, 0.1f, 0.05f, 1 };
  GL (glClearBufferfv) (GL_COLOR, 0, cc);
  GL (glClearBufferfi) (GL_DEPTH_STENCIL, 0, 1.0f, 0);

  /* a uniform block */
  GLuint pu = program2 ("#version 300 es\nlayout(location = 0) in vec2 pos;\n"
			"uniform Block { vec4 col; vec4 off; };\n"
			"void main () { gl_Position = vec4 (pos * 0.2 + off.xy, 0.0, 1.0); }",
			"#version 300 es\nprecision mediump float;\n"
			"uniform Block { vec4 col; vec4 off; };\nout vec4 o;\nvoid main () { o = col; }", 0);
  GLuint bi = GL (glGetUniformBlockIndex) (pu, "Block");
  GLint bsize = -1, bact = -1, bidx[4] = { -1, -1, -1, -1 };
  GL (glGetActiveUniformBlockiv) (pu, bi, GL_UNIFORM_BLOCK_DATA_SIZE, &bsize);
  GL (glGetActiveUniformBlockiv) (pu, bi, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &bact);
  GL (glGetActiveUniformBlockiv) (pu, bi, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, bidx);
  char bname[32] = "";
  GLsizei bl = -1;
  GL (glGetActiveUniformBlockName) (pu, bi, sizeof bname, &bl, bname);
  printf ("uniform block: index %u, %d bytes, %d uniforms, name \"%s\" (%d)\n", bi, bsize, bact, bname, (int) bl);
  const GLchar *names[2] = { "col", "off" };
  GLuint ui[2] = { 99, 99 };
  GL (glGetUniformIndices) (pu, 2, names, ui);
  GLint offs[2] = { -1, -1 };
  GL (glGetActiveUniformsiv) (pu, 2, ui, GL_UNIFORM_OFFSET, offs);
  printf ("its uniforms: offsets %d %d, indices listed %s\n", offs[0], offs[1],
	  (bidx[0] == (GLint) ui[0] || bidx[0] == (GLint) ui[1]) && bidx[2] == -1 ? "yes" : "no");
  GLuint ub;
  GL (glGenBuffers) (1, &ub);
  GL (glBindBuffer) (GL_UNIFORM_BUFFER, ub);
  const GLfloat ubd[8] = { 0.9f, 0.6f, 0.1f, 1, -0.6f, 0.5f, 0, 0 };
  GL (glBufferData) (GL_UNIFORM_BUFFER, sizeof ubd, ubd, GL_STATIC_DRAW);
  GL (glUniformBlockBinding) (pu, bi, 1);
  GL (glBindBufferRange) (GL_UNIFORM_BUFFER, 1, ub, 0, sizeof ubd);
  GLint bound = -1;
  GLint64 bsz = -1;
  GL (glGetIntegeri_v) (GL_UNIFORM_BUFFER_BINDING, 1, &bound);
  GL (glGetInteger64i_v) (GL_UNIFORM_BUFFER_SIZE, 1, &bsz);
  printf ("bound at 1: %s, size %d\n", bound == (GLint) ub ? "yes" : "no", (int) bsz);
  GL (glUseProgram) (pu);
  GL (glBindBuffer) (GL_ARRAY_BUFFER, 0);
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, tri);
  GL (glEnableVertexAttribArray) (0);
  GL (glDrawArrays) (GL_TRIANGLE_STRIP, 0, 4);

  /* transform feedback: captured, read back */
  GLuint pt = program2 ("#version 300 es\nlayout(location = 0) in vec2 pos;\nout vec4 tf;\n"
			"void main () { tf = vec4 (pos * 2.0, float (gl_VertexID), 1.0); gl_Position = vec4 (pos, 0.0, 1.0); }",
			"#version 300 es\nprecision mediump float;\nout vec4 o;\nvoid main () { o = vec4 (1.0); }", "tf");
  char vn[16] = "";
  GLsizei vl = -1, vs = -1;
  GLenum vt = 0;
  GL (glGetTransformFeedbackVarying) (pt, 0, sizeof vn, &vl, &vs, &vt, vn);
  printf ("captured: \"%s\" (%d), size %d, type &%X\n", vn, (int) vl, (int) vs, vt);
  GLuint tb, tfo, q;
  GL (glGenBuffers) (1, &tb);
  GL (glBindBuffer) (GL_TRANSFORM_FEEDBACK_BUFFER, tb);
  GL (glBufferData) (GL_TRANSFORM_FEEDBACK_BUFFER, 3 * 16, 0, GL_STATIC_READ);
  GL (glGenTransformFeedbacks) (1, &tfo);
  GL (glBindTransformFeedback) (GL_TRANSFORM_FEEDBACK, tfo);
  GL (glBindBufferBase) (GL_TRANSFORM_FEEDBACK_BUFFER, 0, tb);
  GL (glGenQueries) (1, &q);
  GL (glUseProgram) (pt);
  GL (glEnable) (GL_RASTERIZER_DISCARD);
  GL (glBeginQuery) (GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, q);
  GL (glBeginTransformFeedback) (GL_TRIANGLES);
  GL (glDrawArrays) (GL_TRIANGLES, 0, 3);
  GL (glEndTransformFeedback) ();
  GL (glEndQuery) (GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
  GL (glDisable) (GL_RASTERIZER_DISCARD);
  GL (glBindTransformFeedback) (GL_TRANSFORM_FEEDBACK, 0);
  GLuint prims = 99;
  GL (glGetQueryObjectuiv) (q, GL_QUERY_RESULT, &prims);
  const GLfloat *cap = GL (glMapBufferRange) (GL_TRANSFORM_FEEDBACK_BUFFER, 0, 3 * 16, GL_MAP_READ_BIT);
  printf ("feedback: %u primitive(s):", prims);
  for (int i = 0; cap && i < 12; i++)
    printf (" %g", (double) cap[i]);
  printf ("\n");
  GL (glUnmapBuffer) (GL_TRANSFORM_FEEDBACK_BUFFER);

  /* instances, with an array of the program's per instance (a divisor),
     and integer attributes from the program's memory */
  GLuint pi = program2 ("#version 300 es\nlayout(location = 0) in vec2 pos;\nlayout(location = 1) in vec2 at;\n"
			"layout(location = 2) in ivec2 ic;\nflat out ivec2 c;\n"
			"void main () { c = ic; gl_Position = vec4 (pos * 0.08 + at, 0.0, 1.0); }",
			"#version 300 es\nprecision mediump float;\nflat in ivec2 c;\nout vec4 o;\n"
			"void main () { o = vec4 (float (c.x) / 255.0, float (c.y) / 255.0, 0.5, 1.0); }", 0);
  static const GLfloat at[8] = { -0.8f, -0.7f, -0.5f, -0.7f, -0.2f, -0.7f, 0.1f, -0.7f };
  static const GLint ic[8] = { 255, 0, 200, 50, 100, 150, 30, 250 };
  GL (glUseProgram) (pi);
  GL (glVertexAttribPointer) (1, 2, GL_FLOAT, GL_FALSE, 0, at);
  GL (glVertexAttribDivisor) (1, 1);
  GL (glEnableVertexAttribArray) (1);
  GL (glVertexAttribIPointer) (2, 2, GL_INT, 0, ic);
  GL (glVertexAttribDivisor) (2, 1);
  GL (glEnableVertexAttribArray) (2);
  GL (glDrawArraysInstanced) (GL_TRIANGLE_STRIP, 0, 4, 4);
  GLint isint = -1;
  GL (glGetVertexAttribIiv) (2, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &isint);
  printf ("integer array: %d\n", isint);
  GL (glDisableVertexAttribArray) (1);
  GL (glDisableVertexAttribArray) (2);
  GL (glVertexAttribDivisor) (1, 0);
  GL (glVertexAttribDivisor) (2, 0);
  GL (glVertexAttribI4i) (2, 40, 220, 0, 0);
  static const GLfloat at1[2] = { 0.4f, -0.7f };
  GL (glVertexAttribPointer) (1, 2, GL_FLOAT, GL_FALSE, 0, at1);
  GL (glVertexAttribDivisor) (1, 1);
  GL (glEnableVertexAttribArray) (1);
  GL (glDrawArraysInstanced) (GL_TRIANGLE_STRIP, 0, 4, 1);
  GL (glDisableVertexAttribArray) (1);
  GL (glVertexAttribDivisor) (1, 0);

  /* primitive restart (the fixed index), indices in the program's memory */
  GL (glUseProgram) (pu);
  GL (glEnable) (GL_PRIMITIVE_RESTART_FIXED_INDEX);
  static const GLfloat strips[16] = { 0.3f, 0.3f, 0.5f, 0.3f, 0.3f, 0.5f, 0.5f, 0.5f,
				      0.6f, 0.3f, 0.8f, 0.3f, 0.6f, 0.5f, 0.8f, 0.5f };
  static const GLushort six[9] = { 0, 1, 2, 3, 0xFFFF, 4, 5, 6, 7 };
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, strips);
  const GLfloat ubd2[8] = { 0.2f, 0.9f, 0.4f, 1, -0.3f, 0.3f, 0, 0 };
  GL (glBufferSubData) (GL_UNIFORM_BUFFER, 0, sizeof ubd2, ubd2);
  GL (glDrawElements) (GL_TRIANGLE_STRIP, 9, GL_UNSIGNED_SHORT, six);
  GL (glDisable) (GL_PRIMITIVE_RESTART_FIXED_INDEX);

  /* sync objects */
  GLsync s = GL (glFenceSync) (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  GL (glObjectPtrLabel) (s, -1, "a fence");
  char sl[32] = "";
  GLsizei sll = -1;
  GL (glGetObjectPtrLabel) (s, sizeof sl, &sll, sl);
  GLenum w = GL (glClientWaitSync) (s, GL_SYNC_FLUSH_COMMANDS_BIT, 2000000000ull);
  GLint st = -1;
  GLsizei stl = -1;
  GL (glGetSynciv) (s, GL_SYNC_STATUS, 1, &stl, &st);
  GLboolean was = GL (glIsSync) (s);
  GL (glWaitSync) (s, 0, GL_TIMEOUT_IGNORED);
  GL (glDeleteSync) (s);
  printf ("sync: waited %s, status %s (%d), was %d, label \"%s\" (%d)\n",
	  w == GL_ALREADY_SIGNALED || w == GL_CONDITION_SATISFIED ? "ok" : "no",
	  st == GL_SIGNALED ? "signalled" : "not", (int) stl, was, sl, (int) sll);

  /* a sampler object and an RG texture from the program's memory */
  GLuint tex, smp;
  GL (glGenTextures) (1, &tex);
  GL (glActiveTexture) (GL_TEXTURE0);
  GL (glBindTexture) (GL_TEXTURE_2D, tex);
  GL (glTexStorage2D) (GL_TEXTURE_2D, 1, GL_RG8, 3, 2);
  static const GLubyte rg[2][3][2] = { { { 255, 0 }, { 0, 255 }, { 128, 128 } }, { { 30, 60 }, { 90, 120 }, { 200, 10 } } };
  GL (glPixelStorei) (GL_UNPACK_ALIGNMENT, 1);
  GL (glTexSubImage2D) (GL_TEXTURE_2D, 0, 0, 0, 3, 2, GL_RG, GL_UNSIGNED_BYTE, rg);
  GL (glPixelStorei) (GL_UNPACK_ALIGNMENT, 4);
  GL (glGenSamplers) (1, &smp);
  GL (glSamplerParameteri) (smp, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  GL (glSamplerParameteri) (smp, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  GL (glBindSampler) (0, smp);
  GLint sf = 0;
  GL (glGetSamplerParameteriv) (smp, GL_TEXTURE_MAG_FILTER, &sf);
  printf ("sampler: mag filter &%X\n", sf);
  GLuint ptx = program2 ("#version 300 es\nlayout(location = 0) in vec2 pos;\nout vec2 uv;\n"
			 "void main () { uv = pos * 0.5 + 0.5; gl_Position = vec4 (pos * 0.25 + vec2 (0.65, -0.1), 0.0, 1.0); }",
			 "#version 300 es\nprecision mediump float;\nuniform sampler2D t;\nin vec2 uv;\nout vec4 o;\n"
			 "void main () { o = vec4 (texture (t, uv).rg, 0.2, 1.0); }", 0);
  GL (glUseProgram) (ptx);
  GL (glUniform1i) (GL (glGetUniformLocation) (ptx, "t"), 0);
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, tri);
  GL (glDrawArrays) (GL_TRIANGLE_STRIP, 0, 4);
  GL (glBindSampler) (0, 0);

  /* indirect draws (GLES 3.1): from a buffer, with arrays in a buffer and
     in the program's memory */
  GLuint vb, ib, eb;
  GL (glGenBuffers) (1, &vb);
  GL (glGenBuffers) (1, &ib);
  GL (glGenBuffers) (1, &eb);
  GLuint vao;
  GL (glGenVertexArrays) (1, &vao);
  GL (glBindVertexArray) (vao);
  static const GLfloat quad[8] = { -0.95f, 0.6f, -0.75f, 0.6f, -0.95f, 0.9f, -0.75f, 0.9f };
  GL (glBindBuffer) (GL_ARRAY_BUFFER, vb);
  GL (glBufferData) (GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  GL (glEnableVertexAttribArray) (0);
  static const GLuint cmds[9] = { 4, 1, 0, 0, /* elements: */ 3, 1, 1, 0, 0 };
  GL (glBindBuffer) (GL_DRAW_INDIRECT_BUFFER, ib);
  GL (glBufferData) (GL_DRAW_INDIRECT_BUFFER, sizeof cmds, cmds, GL_STATIC_DRAW);
  static const GLushort el[4] = { 0, 1, 2, 3 };
  GL (glBindBuffer) (GL_ELEMENT_ARRAY_BUFFER, eb);
  GL (glBufferData) (GL_ELEMENT_ARRAY_BUFFER, sizeof el, el, GL_STATIC_DRAW);
  GL (glUseProgram) (pu);
  const GLfloat ubd3[8] = { 0.7f, 0.7f, 0.9f, 1, 0, 0, 0, 0 };
  GL (glBufferSubData) (GL_UNIFORM_BUFFER, 0, sizeof ubd3, ubd3);
  GL (glDrawArraysIndirect) (GL_TRIANGLE_STRIP, 0);
  const GLfloat ubd4[8] = { 0.9f, 0.3f, 0.7f, 1, 0.25f, 0, 0, 0 };
  GL (glBufferSubData) (GL_UNIFORM_BUFFER, 0, sizeof ubd4, ubd4);
  GL (glDrawElementsIndirect) (GL_TRIANGLES, GL_UNSIGNED_SHORT, (const void *) 16);
  GL (glBindVertexArray) (0);
  GL (glBindBuffer) (GL_ARRAY_BUFFER, 0);
  GLint drawn_ib = -1;
  GL (glGetIntegerv) (GL_DRAW_INDIRECT_BUFFER_BINDING, &drawn_ib);
  printf ("indirect buffer bound: %s, error &%X\n", drawn_ib == (GLint) ib ? "yes" : "no", GL (glGetError) ());

  /* a compute shader (GLES 3.1), a storage buffer, its resources */
  GLuint cs = shader (GL_COMPUTE_SHADER, "#version 310 es\nlayout(local_size_x = 4) in;\n"
		      "layout(std430, binding = 0) buffer Out { uint v[]; };\n"
		      "void main () { v[gl_GlobalInvocationID.x] = gl_GlobalInvocationID.x * 3u + 1u; }");
  GLuint pc = GL (glCreateProgram) ();
  GL (glAttachShader) (pc, cs);
  GL (glLinkProgram) (pc);
  GLuint sb;
  GL (glGenBuffers) (1, &sb);
  GL (glBindBuffer) (GL_SHADER_STORAGE_BUFFER, sb);
  GL (glBufferData) (GL_SHADER_STORAGE_BUFFER, 8 * 4, 0, GL_DYNAMIC_READ);
  GL (glBindBufferBase) (GL_SHADER_STORAGE_BUFFER, 0, sb);
  GL (glUseProgram) (pc);
  GL (glDispatchCompute) (2, 1, 1);
  GL (glMemoryBarrier) (GL_BUFFER_UPDATE_BARRIER_BIT);
  const GLuint *cv = GL (glMapBufferRange) (GL_SHADER_STORAGE_BUFFER, 0, 8 * 4, GL_MAP_READ_BIT);
  printf ("compute:");
  for (int i = 0; cv && i < 8; i++)
    printf (" %u", cv[i]);
  printf ("\n");
  GL (glUnmapBuffer) (GL_SHADER_STORAGE_BUFFER);
  GLuint ri = GL (glGetProgramResourceIndex) (pc, GL_SHADER_STORAGE_BLOCK, "Out");
  char rn[16] = "";
  GLsizei rl = -1;
  GL (glGetProgramResourceName) (pc, GL_SHADER_STORAGE_BLOCK, ri, sizeof rn, &rl, rn);
  const GLenum props[2] = { GL_BUFFER_BINDING, GL_NUM_ACTIVE_VARIABLES };
  GLint pv[2] = { -1, -1 };
  GLsizei pl = -1;
  GL (glGetProgramResourceiv) (pc, GL_SHADER_STORAGE_BLOCK, ri, 2, props, 2, &pl, pv);
  GLint nres = -1;
  GL (glGetProgramInterfaceiv) (pc, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &nres);
  printf ("storage block: \"%s\" (%d), binding %d, variables %d (%d values), %d block(s)\n", rn, (int) rl, pv[0], pv[1],
	  (int) pl, nres);

  /* an object's framebuffer, copied to the program's */
  GLuint fb, rb;
  GL (glGenFramebuffers) (1, &fb);
  GL (glGenRenderbuffers) (1, &rb);
  GL (glBindRenderbuffer) (GL_RENDERBUFFER, rb);
  GL (glRenderbufferStorage) (GL_RENDERBUFFER, GL_RGBA8, 16, 16);
  GL (glBindFramebuffer) (GL_FRAMEBUFFER, fb);
  GL (glFramebufferRenderbuffer) (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
  const GLfloat green[4] = { 0, 1, 0.5f, 1 };
  GL (glClearBufferfv) (GL_COLOR, 0, green);
  GL (glBindFramebuffer) (GL_DRAW_FRAMEBUFFER, 0);
  GL (glBlitFramebuffer) (0, 0, 16, 16, W - 20, H - 20, W - 4, H - 4, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  GL (glBindFramebuffer) (GL_FRAMEBUFFER, 0);
  const GLenum inv[1] = { GL_DEPTH };
  GL (glInvalidateFramebuffer) (GL_FRAMEBUFFER, 1, inv);
  GLint nsamp = -1;
  GL (glGetInternalformativ) (GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 1, &nsamp);
  GLint64 maxub = -1;
  GL (glGetInteger64v) (GL_MAX_UNIFORM_BLOCK_SIZE, &maxub);
  printf ("sample counts: %s; largest uniform block: %s\n", nsamp >= 0 ? "some" : "none", maxub >= 16384 ? "big enough" : "small");
  printf ("error: &%X\n", GL (glGetError) ());
  grab ();
  save_frame ("es3");
  printf ("picture: %08x\n", sum_frame ());
  close_api ();
}

/* --- OpenGL 3.0 and 3.1 (the compatibility profile) -------------------- */
static void
part_gl3 (void)
{
  const char *e = open_api (1);
  if (e)
    {
      printf ("gl3: no context (%s)\n", e);
      return;
    }
  printf ("gl3 (%s)\n", WHO);
  versions ();
  GL (glViewport) (0, 0, W, H);
  GL (glClearColor) (0.3f, 0.3f, 0.3f, 1);
  GL (glClear) (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* fixed function still there, with the compatibility profile (Mesa's
     v3d gives OpenGL 3.1 without it) */
  printf ("compatibility profile: %s\n", compat ? "yes" : "no");
  if (compat)
    {
      GL (glMatrixMode) (GL_PROJECTION);
      GL (glLoadIdentity) ();
      GL (glMatrixMode) (GL_MODELVIEW);
      GL (glLoadIdentity) ();
      GL (glColor4f) (0.9f, 0.5f, 0.1f, 1);
      GL (glBegin) (GL_TRIANGLES);
      GL (glVertex2f) (-0.9f, -0.9f);
      GL (glVertex2f) (-0.5f, -0.9f);
      GL (glVertex2f) (-0.9f, -0.5f);
      GL (glEnd) ();
    }

  /* GLSL 1.40: an output's location, a texture buffer, integers */
  GLuint p = GL (glCreateProgram) ();
  GL (glAttachShader) (p, shader (GL_VERTEX_SHADER, "#version 140\nin vec2 pos;\nin ivec2 ic;\nflat out ivec2 c;\n"
				  "void main () { c = ic; gl_Position = vec4 (pos * 0.3 + vec2 (0.2, 0.2), 0.0, 1.0); }"));
  GL (glAttachShader) (p, shader (GL_FRAGMENT_SHADER, "#version 140\nuniform samplerBuffer tb;\nuniform uint k;\n"
				  "flat in ivec2 c;\nout vec4 colour;\n"
				  "void main () { colour = vec4 (texelFetch (tb, c.x).rg, float (k) / 10.0, 1.0); }"));
  GL (glBindAttribLocation) (p, 0, "pos");
  GL (glBindAttribLocation) (p, 1, "ic");
  GL (glBindFragDataLocation) (p, 0, "colour");
  GL (glLinkProgram) (p);
  GLint ok = 0;
  GL (glGetProgramiv) (p, GL_LINK_STATUS, &ok);
  printf ("program linked: %d, output at %d\n", ok, GL (glGetFragDataLocation) (p, "colour"));
  GLuint tbuf, tbt;
  GL (glGenBuffers) (1, &tbuf);
  GL (glBindBuffer) (GL_TEXTURE_BUFFER, tbuf);
  static const GLfloat texels[8] = { 1, 0, 0, 1, 0.2f, 0.8f, 0, 0 };
  GL (glBufferData) (GL_TEXTURE_BUFFER, sizeof texels, texels, GL_STATIC_DRAW);
  GL (glGenTextures) (1, &tbt);
  GL (glBindTexture) (GL_TEXTURE_BUFFER, tbt);
  GL (glTexBuffer) (GL_TEXTURE_BUFFER, GL_RG32F, tbuf);
  GL (glUseProgram) (p);
  GL (glUniform1i) (GL (glGetUniformLocation) (p, "tb"), 0);
  GL (glUniform1ui) (GL (glGetUniformLocation) (p, "k"), 7);
  GLuint kv = 0;
  GL (glGetUniformuiv) (p, GL (glGetUniformLocation) (p, "k"), &kv);
  char un[16] = "";
  GLsizei unl = -1;
  GL (glGetActiveUniformName) (p, 0, sizeof un, &unl, un);
  printf ("unsigned uniform: %u; uniform 0 \"%s\" (%d)\n", kv, un, (int) unl);
  static const GLint icv[4][2] = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 } };
  static const GLuint idx[7] = { 0, 1, 2, 9, 1, 2, 3 };
  /* the program's memory with the compatibility profile; without it, a
     vertex array object and buffers */
  const void *pt = tri, *pi = icv, *px = idx;
  GLuint vao = 0, bufs[3];
  if (!compat)
    {
      GL (glGenVertexArrays) (1, &vao);
      GL (glBindVertexArray) (vao);
      GL (glGenBuffers) (3, bufs);
      GL (glBindBuffer) (GL_ELEMENT_ARRAY_BUFFER, bufs[2]);
      GL (glBufferData) (GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
      pt = pi = px = 0;
    }
  if (vao)
    {
      GL (glBindBuffer) (GL_ARRAY_BUFFER, bufs[0]);
      GL (glBufferData) (GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    }
  else
    GL (glBindBuffer) (GL_ARRAY_BUFFER, 0);
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, pt);
  GL (glEnableVertexAttribArray) (0);
  if (vao)
    {
      GL (glBindBuffer) (GL_ARRAY_BUFFER, bufs[1]);
      GL (glBufferData) (GL_ARRAY_BUFFER, sizeof icv, icv, GL_STATIC_DRAW);
    }
  GL (glVertexAttribIPointer) (1, 2, GL_INT, 0, pi);
  GL (glEnableVertexAttribArray) (1);
  /* primitive restart with an index of the program's */
  GL (glEnable) (GL_PRIMITIVE_RESTART);
  GL (glPrimitiveRestartIndex) (9);
  GL (glDrawElements) (GL_TRIANGLE_STRIP, 7, GL_UNSIGNED_INT, px);
  GLint ri = -1;
  GL (glGetIntegerv) (GL_PRIMITIVE_RESTART_INDEX, &ri);
  GL (glDisable) (GL_PRIMITIVE_RESTART);
  printf ("restart index: %d\n", ri);
  GL (glDisableVertexAttribArray) (1);
  GL (glDisableVertexAttribArray) (0);
  GL (glUseProgram) (0);

  /* per-buffer state, conditional rendering */
  GL (glColorMaski) (0, GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
  GLboolean cm[4] = { 9, 9, 9, 9 };
  GL (glGetBooleani_v) (GL_COLOR_WRITEMASK, 0, cm);
  GL (glEnablei) (GL_BLEND, 0);
  printf ("colour mask 0: %d %d %d %d, blend 0 %d\n", cm[0], cm[1], cm[2], cm[3], GL (glIsEnabledi) (GL_BLEND, 0));
  GL (glDisablei) (GL_BLEND, 0);
  GLuint q;
  GL (glGenQueries) (1, &q);
  GLuint pf = GL (glCreateProgram) ();
  GL (glAttachShader) (pf, shader (GL_VERTEX_SHADER, "#version 140\nin vec2 pos;\nuniform vec4 at;\n"
				   "void main () { gl_Position = vec4 (pos * at.z + at.xy, 0.0, 1.0); }"));
  GL (glAttachShader) (pf, shader (GL_FRAGMENT_SHADER, "#version 140\nuniform vec4 c;\nout vec4 o;\nvoid main () { o = c; }"));
  GL (glBindAttribLocation) (pf, 0, "pos");
  GL (glLinkProgram) (pf);
  GL (glUseProgram) (pf);
  GLint lat = GL (glGetUniformLocation) (pf, "at"), lc = GL (glGetUniformLocation) (pf, "c");
  if (vao)
    GL (glBindBuffer) (GL_ARRAY_BUFFER, bufs[0]);
  GL (glVertexAttribPointer) (0, 2, GL_FLOAT, GL_FALSE, 0, pt);
  GL (glEnableVertexAttribArray) (0);
  GL (glBeginQuery) (GL_SAMPLES_PASSED, q);
  GL (glUniform4f) (lat, 0.7f, -0.7f, 0.15f, 0);
  GL (glUniform4f) (lc, 1, 1, 1, 1);
  GL (glDrawArrays) (GL_TRIANGLES, 0, 3);
  GL (glEndQuery) (GL_SAMPLES_PASSED);
  GL (glColorMaski) (0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  GL (glBeginConditionalRender) (q, GL_QUERY_WAIT);
  GL (glUniform4f) (lat, 0.7f, 0.7f, 0.15f, 0);
  GL (glUniform4f) (lc, 0.1f, 0.1f, 0.9f, 1);
  GL (glDrawArrays) (GL_TRIANGLES, 0, 3);
  GL (glEndConditionalRender) ();
  GL (glDisableVertexAttribArray) (0);
  GL (glUseProgram) (0);
  GLsync s = GL (glFenceSync) (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  GLenum w = GL (glClientWaitSync) (s, GL_SYNC_FLUSH_COMMANDS_BIT, 2000000000ull);
  GL (glDeleteSync) (s);
  printf ("sync: %s\n", w == GL_ALREADY_SIGNALED || w == GL_CONDITION_SATISFIED ? "ok" : "no");
  printf ("error: &%X\n", GL (glGetError) ());
  grab ();
  save_frame ("gl3");
  printf ("picture: %08x\n", sum_frame ());
  close_api ();
}

int
main (int argc, char **argv)
{
  int which = argc > 1 && strcmp (argv[1], "both") ? (!strcmp (argv[1], "es3") ? 1 : 2) : 3;
  picture_file = argc > 2 ? argv[2] : 0;
  if (which & 1)
    part_es3 ();
  if (which & 2)
    part_gl3 ();
  if (missing)
    printf ("functions not found: %d\n", missing);
  return 0;
}
