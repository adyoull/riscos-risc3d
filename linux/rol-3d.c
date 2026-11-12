/* rol-3d: the renderer behind RISC OS on Linux's Linux3D driver (RISC3D).
 *
 * RISC OS programs draw with GLES 2 through Mesa's front end on RISC OS,
 * which writes commands into memory shared with this program
 * (proto/r3d_shared.h). Each RISC3D context is one connection on the
 * abstract socket "rol-3d" and one child process here, with its own EGL
 * context on Linux's graphics driver: the GPU's (through its DRM render
 * node) where there is one, Mesa's software renderer otherwise. Several
 * RISC OS programs are several clients of the driver, as on any Linux
 * desktop.
 *
 * The child reads commands as they appear, with no system call; when
 * there are none it sleeps on the socket until a byte wakes it ('W'), or
 * asks it to finish everything and answer ('S': a query, or the end of a
 * frame). Answers and finished frames go back in the shared reply area.
 *
 * PROTOTYPE: frames are read back and copied (no zero-copy to the screen
 * yet), and the wire format will change.
 *
 * Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <GLES2/gl2.h>
#include "r3d_ops.h"
#include "r3d_shared.h"

#if defined __aarch64__ || defined __arm__
#define R3D_PAUSE() __asm__ volatile ("yield")
#else
#define R3D_PAUSE() __builtin_ia32_pause ()
#endif
#define RO_UID 1000
#define RO_GID 1000
#define GLDIR "/opt/riscos/gl"		/* a private Mesa, if the image has one */

static struct r3d_area *area;
static uint32_t *ring;
static char *reply_area;
static int sock = -1;

static void
logmsg (const char *fmt, ...)
{
  char b[400];
  va_list ap;
  va_start (ap, fmt);
  int n = vsnprintf (b, sizeof b, fmt, ap);
  va_end (ap);
  int k = open ("/dev/kmsg", O_WRONLY | O_CLOEXEC);
  if (k >= 0)
    {
      if (write (k, b, (size_t) (n < (int) sizeof b ? n : (int) sizeof b - 1)) < 0) { }
      close (k);
    }
  if (write (2, b, strlen (b)) < 0) { }
}

static void
reply (const void *data, size_t n)
{
  if (n > area->reply_bytes)
    n = area->reply_bytes;
  memcpy (reply_area, data, n);
  area->reply_len = (uint32_t) n;
}

static void *
scratch (size_t n)
{
  static void *p;
  static size_t have;
  if (n > have)
    {
      free (p);
      p = malloc (have = n + 4096);
    }
  memset (p, 0, n);
  return p;
}

static float r3d_u2f (uint32_t u) { union { uint32_t u; float f; } v = { u }; return v.f; }

#include "server_gen.c"

/* --- EGL, loaded at run time -------------------------------------------- */

typedef void *EGLDisplay, *EGLConfig, *EGLContext, *EGLSurface;
typedef int EGLint;
typedef unsigned EGLBoolean, EGLenum;
#define EGL_NONE 0x3038
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES2_BIT 4
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_PLATFORM_GBM_KHR 0x31D7
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD

static struct
{
  EGLDisplay (*GetPlatformDisplay) (EGLenum, void *, const intptr_t *);
  EGLBoolean (*Initialize) (EGLDisplay, EGLint *, EGLint *);
  EGLBoolean (*BindAPI) (EGLenum);
  EGLBoolean (*ChooseConfig) (EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
  EGLContext (*CreateContext) (EGLDisplay, EGLConfig, EGLContext, const EGLint *);
  EGLBoolean (*MakeCurrent) (EGLDisplay, EGLSurface, EGLSurface, EGLContext);
  EGLint (*GetError) (void);
} egl;

static void *
lib (const char *name)
{
  char p[256];
  snprintf (p, sizeof p, GLDIR "/lib/%s", name);
  void *h = dlopen (p, RTLD_NOW | RTLD_GLOBAL);
  return h ? h : dlopen (name, RTLD_NOW | RTLD_GLOBAL);
}

static char gl_error[200];
static int hardware;

/* A context on display d, made current: 1, or 0 with gl_error. GLES 2
   where Linux's Mesa has it; otherwise desktop OpenGL, which has the same
   functions (ARB_ES2_compatibility) and takes GLES shaders marked
   "#version 100" (shader_source adds the mark). */
static int desktop_gl;

static int
context_on (EGLDisplay d)
{
  static const EGLint want_es[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, 0x3033 /* EGL_SURFACE_TYPE: any */, 0, EGL_NONE };
  static const EGLint want_gl[] = { EGL_RENDERABLE_TYPE, 8 /* EGL_OPENGL_BIT */, 0x3033, 0, EGL_NONE };
  static const EGLint ver[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  EGLConfig cfg = 0;
  EGLint n = 0;
  if (!d || !egl.Initialize (d, 0, 0))
    {
      snprintf (gl_error, sizeof gl_error, "EGL would not start (error &%X)", egl.GetError ());
      return 0;
    }
  EGLContext c = 0;
  egl.BindAPI (EGL_OPENGL_ES_API);
  if (egl.ChooseConfig (d, want_es, &cfg, 1, &n) && n >= 1)
    c = egl.CreateContext (d, cfg, 0, ver);
  if (!c)
    {
      n = 0;
      egl.BindAPI (0x30A2 /* EGL_OPENGL_API */);
      if (egl.ChooseConfig (d, want_gl, &cfg, 1, &n) && n >= 1)
	c = egl.CreateContext (d, cfg, 0, 0);
      desktop_gl = c != 0;
    }
  if (!c || !egl.MakeCurrent (d, 0, 0, c))
    {
      snprintf (gl_error, sizeof gl_error, "no GLES 2 or OpenGL context (EGL error &%X)", egl.GetError ());
      return 0;
    }
  return 1;
}

static int
gl_start (void)
{
  if (access (GLDIR "/lib", F_OK) == 0)
    {
      setenv ("__EGL_VENDOR_LIBRARY_DIRS", GLDIR "/share/glvnd/egl_vendor.d", 0);
      setenv ("LIBGL_DRIVERS_PATH", GLDIR "/lib/dri", 0);
      setenv ("GBM_BACKENDS_PATH", GLDIR "/lib/gbm", 0);
    }
  void *e = lib ("libEGL.so.1"), *g = lib ("libGLESv2.so.2");
  if (!e || !g)
    {
      snprintf (gl_error, sizeof gl_error, "Linux has no EGL or GLES library (%s)", dlerror ());
      return 0;
    }
#define E(n) if (!(egl.n = dlsym (e, "egl" #n))) { snprintf (gl_error, sizeof gl_error, "egl" #n " missing"); return 0; }
  E (GetPlatformDisplay) E (Initialize) E (BindAPI) E (ChooseConfig) E (CreateContext) E (MakeCurrent) E (GetError)
#undef E
  void **fp = (void **) &gl;
  for (int i = 0; gl_names[i]; i++)
    if (!(fp[i] = dlsym (g, gl_names[i])))
      {
	snprintf (gl_error, sizeof gl_error, "%s missing from the GLES library", gl_names[i]);
	return 0;
      }
  /* the GPU, through its render node; then Mesa's software renderer (also
     where the node is a display with no 3D, as virtio's can be) */
  void *gbm = lib ("libgbm.so.1");
  void *(*gbm_create_device) (int) = gbm ? dlsym (gbm, "gbm_create_device") : 0;
  for (int n = 128; n < 132 && gbm_create_device && !getenv ("ROL3D_SOFTWARE"); n++)
    {
      char node[40];
      snprintf (node, sizeof node, "/dev/dri/renderD%d", n);
      int fd = open (node, O_RDWR | O_CLOEXEC);
      if (fd < 0)
	continue;
      void *dev = gbm_create_device (fd);
      if (dev && context_on (egl.GetPlatformDisplay (EGL_PLATFORM_GBM_KHR, dev, 0)))
	{
	  hardware = 1;
	  return 1;
	}
      close (fd);
    }
  return context_on (egl.GetPlatformDisplay (EGL_PLATFORM_SURFACELESS_MESA, 0, 0));
}

/* --- the surface: a framebuffer object of ours that is the program's
   framebuffer 0 ---------------------------------------------------------- */

static GLuint fbo, fb_tex, fb_depth, app_fb;
static int fb_w, fb_h;

static void
surface (int w, int h)
{
  GLint tex = 0, rb = 0;
  if (w < 1 || h < 1 || w > 8192 || h > 8192)
    return;
  gl.GetIntegerv (GL_TEXTURE_BINDING_2D, &tex);
  gl.GetIntegerv (GL_RENDERBUFFER_BINDING, &rb);
  if (!fbo)
    {
      gl.GenFramebuffers (1, &fbo);
      gl.GenTextures (1, &fb_tex);
      gl.GenRenderbuffers (1, &fb_depth);
    }
  gl.BindTexture (GL_TEXTURE_2D, fb_tex);
  gl.TexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  gl.TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl.TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl.BindRenderbuffer (GL_RENDERBUFFER, fb_depth);
  const char *ext = (const char *) gl.GetString (GL_EXTENSIONS);
  int packed = ext && strstr (ext, "GL_OES_packed_depth_stencil");
  gl.RenderbufferStorage (GL_RENDERBUFFER, packed ? 0x88F0 /* DEPTH24_STENCIL8_OES */ : GL_DEPTH_COMPONENT16, w, h);
  gl.BindFramebuffer (GL_FRAMEBUFFER, fbo);
  gl.FramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb_tex, 0);
  gl.FramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb_depth);
  if (packed)
    gl.FramebufferRenderbuffer (GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb_depth);
  gl.BindTexture (GL_TEXTURE_2D, (GLuint) tex);
  gl.BindRenderbuffer (GL_RENDERBUFFER, (GLuint) rb);
  if (!fb_w)
    {
      gl.Viewport (0, 0, w, h);
      gl.Scissor (0, 0, w, h);
    }
  fb_w = w;
  fb_h = h;
  gl.BindFramebuffer (GL_FRAMEBUFFER, app_fb ? app_fb : fbo);
}

/* the finished frame, top row first, into the reply area */
static void
present (void)
{
  size_t row = (size_t) fb_w * 4, n = row * (size_t) fb_h;
  if (!fbo || n > area->reply_bytes)
    {
      area->reply_len = 0;
      return;
    }
  GLint pack = 4;
  gl.GetIntegerv (GL_PACK_ALIGNMENT, &pack);
  gl.PixelStorei (GL_PACK_ALIGNMENT, 4);
  gl.BindFramebuffer (GL_FRAMEBUFFER, fbo);
  char *t = scratch (n);
  gl.ReadPixels (0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, t);
  for (int y = 0; y < fb_h; y++)
    memcpy (reply_area + row * (size_t) y, t + row * (size_t) (fb_h - 1 - y), row);
  area->reply_len = (uint32_t) n;
  gl.PixelStorei (GL_PACK_ALIGNMENT, pack);
  gl.BindFramebuffer (GL_FRAMEBUFFER, app_fb ? app_fb : fbo);
}

/* --- the commands written by hand at both ends (lib/r3d_gl.c) ----------- */

static GLuint array_vbo[16], element_vbo;

static void
text_reply (void (*get) (GLuint, GLsizei, GLsizei *, GLchar *), GLuint obj, uint32_t max)
{
  if (max > 65536)
    max = 65536;
  uint32_t *r = scratch (max + 8);
  GLsizei l = 0;
  get (obj, (GLsizei) max, &l, (GLchar *) (r + 1));
  r[0] = (uint32_t) l;
  reply (r, (size_t) l + 4);
}

static void
dispatch (uint32_t op, const uint32_t *a, uint32_t n)
{
  if (dispatch_gen (op, a, n))
    return;
  switch (op)
    {
    case OP_SURFACE:
      surface ((int) a[0], (int) a[1]);
      break;
    case OP_PRESENT:
      present ();
      break;
    case OP_SYNC:
      area->reply_len = 0;
      break;
    case OP_glBindFramebuffer:
      app_fb = a[1];
      gl.BindFramebuffer (a[0], a[1] ? a[1] : fbo);
      break;
    case OP_glBindAttribLocation:
      {
	char *s = scratch (a[2] + 1);
	memcpy (s, a + 3, a[2]);
	gl.BindAttribLocation (a[0], a[1], s);
	break;
      }
    case OP_glGetAttribLocation:
    case OP_glGetUniformLocation:
      {
	char *s = scratch (a[1] + 1);
	memcpy (s, a + 2, a[1]);
	int32_t r = op == OP_glGetAttribLocation ? gl.GetAttribLocation (a[0], s) : gl.GetUniformLocation (a[0], s);
	reply (&r, 4);
	break;
      }
    case OP_glShaderSource:
      {
	const GLchar *s[2] = { "#version 100\n", (const GLchar *) (a + 2) };
	GLint l[2] = { 13, (GLint) a[1] };
	int mark = desktop_gl && !memmem (s[1], (size_t) l[1], "#version", 8);
	gl.ShaderSource (a[0], mark ? 2 : 1, s + !mark, l + !mark);
	break;
      }
    case OP_glGetShaderInfoLog:
      text_reply (gl.GetShaderInfoLog, a[0], a[1]);
      break;
    case OP_glGetProgramInfoLog:
      text_reply (gl.GetProgramInfoLog, a[0], a[1]);
      break;
    case OP_glGetShaderSource:
      text_reply (gl.GetShaderSource, a[0], a[1]);
      break;
    case OP_glGetActiveAttrib:
    case OP_glGetActiveUniform:
      {
	uint32_t r[3 + 64];
	GLsizei l = 0;
	GLint size = 0;
	GLenum type = 0;
	memset (r, 0, sizeof r);
	(op == OP_glGetActiveAttrib ? gl.GetActiveAttrib : gl.GetActiveUniform)
	  (a[0], a[1], 255, &l, &size, &type, (GLchar *) (r + 3));
	r[0] = (uint32_t) l;
	r[1] = (uint32_t) size;
	r[2] = type;
	reply (r, sizeof r);
	break;
      }
    case OP_glGetAttachedShaders:
      {
	uint32_t r[1 + 32];
	GLsizei c = 0;
	memset (r, 0, sizeof r);
	gl.GetAttachedShaders (a[0], a[1] > 32 ? 32 : (GLsizei) a[1], &c, r + 1);
	r[0] = (uint32_t) c;
	reply (r, sizeof r);
	break;
      }
    case OP_glGetShaderPrecisionFormat:
      {
	GLint r[3] = { 0, 0, 0 };
	gl.GetShaderPrecisionFormat (a[0], a[1], r, r + 2);
	reply (r, sizeof r);
	break;
      }
    case OP_glGetString:
      {
	const char *s = (const char *) gl.GetString (a[0]);
	char b[2048];
	if (a[0] == GL_RENDERER && s)
	  {
	    /* (a render node can be a software renderer's too: virtio's) */
	    if (strstr (s, "llvmpipe") || strstr (s, "softpipe"))
	      hardware = 0;
	    /* say how it is being drawn */
	    snprintf (b, sizeof b, "%s (RISC3D, Linux %s%s)", s, hardware ? "GPU" : "software", desktop_gl ? ", desktop OpenGL" : "");
	    s = b;
	  }
	reply (s ? s : "", s ? strlen (s) + 1 : 1);
	break;
      }
    case OP_glGetIntegerv:
    case OP_glGetFloatv:
      {
	uint32_t r[16];
	memset (r, 0, sizeof r);
	if (op == OP_glGetIntegerv)
	  {
	    gl.GetIntegerv (a[0], (GLint *) r);
	    if (a[0] == GL_FRAMEBUFFER_BINDING && r[0] == fbo)
	      r[0] = 0;
	  }
	else
	  gl.GetFloatv (a[0], (GLfloat *) r);
	reply (r, sizeof r);
	break;
      }
    case OP_glBufferData:
      gl.BufferData (a[0], (GLsizeiptr) a[1], a[3] ? a + 4 : 0, a[2]);
      break;
    case OP_glBufferSubData:
      gl.BufferSubData (a[0], (GLintptr) a[1], (GLsizeiptr) a[2], a + 3);
      break;
    case OP_glTexImage2D:
      gl.TexImage2D (a[0], (GLint) a[1], (GLint) a[2], (GLsizei) a[3], (GLsizei) a[4], (GLint) a[5], a[6], a[7], a[8] ? a + 9 : 0);
      break;
    case OP_glTexSubImage2D:
      gl.TexSubImage2D (a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLsizei) a[4], (GLsizei) a[5], a[6], a[7], a + 9);
      break;
    case OP_glCompressedTexImage2D:
      gl.CompressedTexImage2D (a[0], (GLint) a[1], a[2], (GLsizei) a[3], (GLsizei) a[4], (GLint) a[5], (GLsizei) a[6], a + 7);
      break;
    case OP_glCompressedTexSubImage2D:
      gl.CompressedTexSubImage2D (a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLsizei) a[4], (GLsizei) a[5], a[6], (GLsizei) a[7], a + 8);
      break;
    case OP_glReadPixels:
      {
	uint32_t bytes = a[6] > area->reply_bytes ? area->reply_bytes : a[6];
	gl.ReadPixels ((GLint) a[0], (GLint) a[1], (GLsizei) a[2], (GLsizei) a[3], a[4], a[5], reply_area);
	area->reply_len = bytes;
	break;
      }
    case OP_glVertexAttribPointer:
      gl.VertexAttribPointer (a[0], (GLint) a[1], a[2], (GLboolean) a[3], (GLsizei) a[4], (const void *) (uintptr_t) a[5]);
      break;
    case OP_CLIENT_ARRAY:
      {
	/* an array from the program's memory: into a buffer of ours */
	GLint was = 0;
	if (a[0] >= 16)
	  break;
	gl.GetIntegerv (GL_ARRAY_BUFFER_BINDING, &was);
	if (!array_vbo[a[0]])
	  gl.GenBuffers (1, &array_vbo[a[0]]);
	gl.BindBuffer (GL_ARRAY_BUFFER, array_vbo[a[0]]);
	gl.BufferData (GL_ARRAY_BUFFER, (GLsizeiptr) a[5], a + 6, GL_STREAM_DRAW);
	gl.VertexAttribPointer (a[0], (GLint) a[1], a[2], (GLboolean) a[3], (GLsizei) a[4], 0);
	gl.BindBuffer (GL_ARRAY_BUFFER, (GLuint) was);
	break;
      }
    case OP_glDrawArrays:
      gl.DrawArrays (a[0], (GLint) a[1], (GLsizei) a[2]);
      break;
    case OP_glDrawElements:
      gl.DrawElements (a[0], (GLsizei) a[1], a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_DRAW_ELEMENTS_INLINE:
      if (!element_vbo)
	gl.GenBuffers (1, &element_vbo);
      gl.BindBuffer (GL_ELEMENT_ARRAY_BUFFER, element_vbo);
      gl.BufferData (GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) a[3], a + 4, GL_STREAM_DRAW);
      gl.DrawElements (a[0], (GLsizei) a[1], a[2], 0);
      gl.BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
      break;
    default:
      break;
    }
}

/* --- the ring ----------------------------------------------------------- */

static char *blob;
static size_t blob_len, blob_cap;

static void
drain (void)
{
  uint32_t mask = area->ring_words - 1;
  for (;;)
    {
      uint32_t tail = area->tail;
      if (tail == __atomic_load_n (&area->head, __ATOMIC_SEQ_CST))
	return;
      uint32_t at = tail & mask;
      uint32_t op = ring[at];
      if (op == OP_WRAP)
	{
	  __atomic_store_n (&area->tail, tail + (area->ring_words - at), __ATOMIC_SEQ_CST);
	  continue;
	}
      uint32_t n = ring[at + 1];
      if (n < 2 || at + n > area->ring_words)
	{
	  logmsg ("rol-3d: a bad command (&%X, %u words): the context ends\n", op, n);
	  _exit (1);
	}
      const uint32_t *a = ring + at + 2;
      if (op == OP_BLOB)
	{
	  if (blob_len + a[0] > blob_cap)
	    blob = realloc (blob, blob_cap = (blob_len + a[0]) * 2 + 65536);
	  memcpy (blob + blob_len, a + 1, a[0]);
	  blob_len += a[0];
	}
      else if (op & R3D_WITH_BLOB)
	{
	  /* the fixed words, then the data that came ahead */
	  size_t fixed = (size_t) (n - 2) * 4;
	  uint32_t *all = malloc (fixed + blob_len + 8);
	  memcpy (all, a, fixed);
	  memcpy ((char *) all + fixed, blob, blob_len);
	  memset ((char *) all + fixed + blob_len, 0, 8);
	  dispatch (op & ~R3D_WITH_BLOB, all, (uint32_t) ((fixed + blob_len + 3) / 4));
	  free (all);
	  blob_len = 0;
	}
      else
	dispatch (op, a, n - 2);
      __atomic_store_n (&area->tail, tail + n, __ATOMIC_SEQ_CST);
    }
}

static void
serve (int c)
{
  sock = c;
  int fd = memfd_create ("risc3d", MFD_CLOEXEC);
  if (fd < 0 || ftruncate (fd, R3D_AREA_BYTES))
    _exit (1);
  area = mmap (0, R3D_AREA_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (area == MAP_FAILED)
    _exit (1);
  area->magic = R3D_MAGIC;
  area->ring_words = R3D_RING_WORDS;
  area->ring_offset = 4096;
  area->reply_offset = 4096 + R3D_RING_WORDS * 4;
  area->reply_bytes = R3D_REPLY_BYTES;
  ring = (uint32_t *) ((char *) area + area->ring_offset);
  reply_area = (char *) area + area->reply_offset;

  char msg[240];
  if (!gl_start ())
    {
      snprintf (msg, sizeof msg, "ERR %s", gl_error);
      logmsg ("rol-3d: %s\n", gl_error);
      if (send (c, msg, strlen (msg) + 1, MSG_NOSIGNAL) < 0) { }
      _exit (1);
    }
  snprintf (msg, sizeof msg, "OK %u", R3D_AREA_BYTES);
  union { char buf[CMSG_SPACE (sizeof (int))]; struct cmsghdr align; } ctl;
  struct iovec iov = { msg, strlen (msg) + 1 };
  struct msghdr m = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = ctl.buf, .msg_controllen = sizeof ctl.buf };
  struct cmsghdr *cm = CMSG_FIRSTHDR (&m);
  cm->cmsg_level = SOL_SOCKET;
  cm->cmsg_type = SCM_RIGHTS;
  cm->cmsg_len = CMSG_LEN (sizeof (int));
  memcpy (CMSG_DATA (cm), &fd, sizeof (int));
  if (sendmsg (c, &m, MSG_NOSIGNAL) < 0)
    _exit (1);
  close (fd);
  logmsg ("rol-3d: a context: %s, %s\n", (const char *) gl.GetString (GL_RENDERER), (const char *) gl.GetString (GL_VERSION));

  for (;;)
    {
      drain ();
      /* a little longer awake: the next command is often on its way */
      for (int i = 0; i < 2000 && area->tail == __atomic_load_n (&area->head, __ATOMIC_SEQ_CST); i++)
	R3D_PAUSE ();
      if (area->tail != area->head)
	continue;
      __atomic_store_n (&area->sleeping, 1, __ATOMIC_SEQ_CST);
      if (area->tail != __atomic_load_n (&area->head, __ATOMIC_SEQ_CST))
	{
	  __atomic_store_n (&area->sleeping, 0, __ATOMIC_SEQ_CST);
	  continue;
	}
      char b[64];
      ssize_t n = read (c, b, sizeof b);
      __atomic_store_n (&area->sleeping, 0, __ATOMIC_SEQ_CST);
      if (n == 0 || (n < 0 && errno != EINTR))
	_exit (0);			/* the context was closed */
      for (ssize_t i = 0; i < n; i++)
	if (b[i] == R3D_WAIT)
	  {
	    drain ();
	    char ack = 'A';
	    if (send (c, &ack, 1, MSG_NOSIGNAL) != 1)
	      _exit (0);
	  }
    }
}

int
main (int argc, char **argv)
{
  if (argc > 1 && !strcmp (argv[1], "--check"))
    {
      /* what would draw, or why nothing can */
      if (!gl_start ())
	{
	  printf ("rol-3d: %s\n", gl_error);
	  return 1;
	}
      printf ("rol-3d: %s; %s; %s\n", hardware ? "GPU" : "software", (const char *) gl.GetString (GL_RENDERER),
	      (const char *) gl.GetString (GL_VERSION));
      return 0;
    }
  int l = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  struct sockaddr_un a = { .sun_family = AF_UNIX };
  static const char nm[] = "rol-3d";
  memcpy (a.sun_path + 1, nm, sizeof nm - 1);	/* abstract */
  if (l < 0 || bind (l, (struct sockaddr *) &a, offsetof (struct sockaddr_un, sun_path) + sizeof nm)
      || listen (l, 8))
    return 1;
  /* As RISC OS's own Linux user, with the group that reaches the GPU. The
     images have no udev, so the render nodes are root's until now: they
     go to Linux's render group (or video), which only this helper joins.
     Render nodes can draw; they can't set modes or take the display. */
  if (getuid () == 0)
    {
      struct group *gr = getgrnam ("render");
      if (!gr)
	gr = getgrnam ("video");
      gid_t g[2] = { RO_GID, gr ? gr->gr_gid : RO_GID };
      for (int n = 128; n < 132; n++)
	{
	  char node[40];
	  snprintf (node, sizeof node, "/dev/dri/renderD%d", n);
	  if (chown (node, 0, g[1]) == 0)
	    chmod (node, 0660);
	}
      if (setgroups (2, g) || setgid (RO_GID) || setuid (RO_UID))
	return 1;
      /* HOME was root's, so Mesa couldn't make its shader cache and
	 compiled every shader again for each program */
      struct passwd *pw = getpwuid (RO_UID);
      setenv ("HOME", pw && pw->pw_dir && access (pw->pw_dir, W_OK) == 0 ? pw->pw_dir : "/tmp", 1);
    }
  if (!getenv ("MESA_SHADER_CACHE_DIR") && access (getenv ("HOME") ? getenv ("HOME") : "/", W_OK) != 0)
    {
      mkdir ("/tmp/rol-3d-cache", 0700);
      setenv ("MESA_SHADER_CACHE_DIR", "/tmp/rol-3d-cache", 1);
    }
  signal (SIGCHLD, SIG_IGN);
  signal (SIGPIPE, SIG_IGN);
  for (;;)
    {
      int c = accept4 (l, 0, 0, SOCK_CLOEXEC);
      if (c < 0)
	continue;
      if (fork () == 0)
	{
	  close (l);
	  serve (c);
	  _exit (0);
	}
      close (c);
    }
}
