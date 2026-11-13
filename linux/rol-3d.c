/* rol-3d: the renderer behind RISC OS on Linux's Linux3D driver (RISC3D).
 *
 * RISC OS programs draw with OpenGL (1.x to 2.1, the compatibility
 * profile) or OpenGL ES 2 through Mesa's front end on RISC OS, which
 * writes commands into memory shared with this program
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
 * Frames are read back into the reply area, or (zero-copy) copied by the
 * GPU into buffers that rol-fb shows on a display plane under the
 * desktop. The main process stays root to open the display's card for
 * each connection; each context's process runs as RISC OS's Linux user.
 *
 * PROTOTYPE: the wire format will change.
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
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <drm/drm.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_mode.h>
#include "r3d_gl.h"
#include "r3d_ops.h"
#include "r3d_shared.h"

#if defined __aarch64__ || defined __arm__
#define R3D_PAUSE() __asm__ volatile ("yield")
#else
#define R3D_PAUSE() __builtin_ia32_pause ()
#endif
#define RO_UID 1000
#define RO_GID 1000
#define ROL3D_VERSION "0.6"
/* what the library may send (OP_CONTEXT's reply, word 3): 6 = 0.6's
   commands (GL_EXT_direct_state_access, OP_TEMP_BIND); none: 0.5 */
#define ROL3D_LEVEL 6
#define GLDIR "/opt/riscos/gl"		/* a private Mesa, if the image has one */

/* The helper's own textures and buffers have names the program's never
   reach: the library gives the program names from 1 up (lib/r3d_gl.c). */
#define PRIV 0x7fff0000u
#define PRIV_FB_TEX priv_name (0)		/* (a texture; the rest are buffers) */
#define PRIV_ARRAY(i) priv_name (16 + (i))	/* generic attributes 0..15 */
#define PRIV_ELEMENTS priv_name (40)
#define PRIV_FIXED(i) priv_name (48 + (i))	/* fixed-function arrays 0..15 */
#define PRIV_IMMEDIATE priv_name (64)
static GLuint priv_name (unsigned k);

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

/* where the time goes (OP_STATS): working through commands, of that the
   frames' read-back, times asleep, commands */
static uint64_t st_busy, st_present, st_sleeps, st_commands;

static uint64_t
now_ns (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return (uint64_t) t.tv_sec * 1000000000u + (uint64_t) t.tv_nsec;
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
static double r3d_w2d (const uint32_t *w) { double d; memcpy (&d, w, 8); return d; }
static uint64_t r3d_w2_64 (const uint32_t *w) { uint64_t v; memcpy (&v, w, 8); return v; }

/* strings from the stream (lib/r3d_gl.c: r3d_strs): [count], then each
   one's length and bytes; an array of copies ending in a null */
static char **
unpack_strs (const char *d)
{
  uint32_t n;
  memcpy (&n, d, 4);
  if (n > 65536)
    return 0;
  char **v = calloc (n + 1, sizeof *v);
  const char *p = d + 4;
  for (uint32_t i = 0; v && i < n; i++)
    {
      uint32_t l;
      memcpy (&l, p, 4);
      v[i] = malloc (l + 1);
      if (v[i])
	{
	  memcpy (v[i], p + 4, l);
	  v[i][l] = 0;
	}
      p += 4 + (l + 3) / 4 * 4;
    }
  return v;
}

static void
free_strs (char **v)
{
  for (size_t i = 0; v && v[i]; i++)
    free (v[i]);
  free (v);
}

/* answers whose length only the driver knows: fill with a mark first,
   and count what it wrote over */
#define SENTINEL 0x7fbadbadu
static void
sentinel_fill (uint32_t *r, size_t n, int size)
{
  for (size_t i = 0; i < n * (size_t) size / 4; i++)
    r[i] = SENTINEL;
}

static uint32_t
sentinel_count (const uint32_t *r, size_t n, int size)
{
  size_t per = (size_t) size / 4, k = n;
  while (k > 0)
    {
      size_t i;
      for (i = 0; i < per; i++)
	if (r[(k - 1) * per + i] != SENTINEL)
	  break;
      if (i < per)
	break;
      k--;
    }
  return (uint32_t) k;
}

#include "server_gen.c"

/* OpenGL 3.1 without the compatibility profile: names it gave only (ours
   from it too, and the program's: lib/r3d_gl.c, gen_core) */
static int strict_ctx;
static GLuint
priv_name (unsigned k)
{
  static GLuint have[128];
  if (!strict_ctx || k >= 128)
    return PRIV + k;
  if (!have[k])
    {
      if (k == 0)
	gl.GenTextures (1, &have[k]);
      else
	gl.GenBuffers (1, &have[k]);
    }
  return have[k];
}
#include "es1_conversion.c"
#include "es1.c"

/* --- EGL, loaded at run time -------------------------------------------- */

typedef void *EGLDisplay, *EGLConfig, *EGLContext, *EGLSurface;
typedef int EGLint;
typedef unsigned EGLBoolean, EGLenum;
#define EGL_NONE 0x3038
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_SURFACE_TYPE 0x3033
#define EGL_OPENGL_ES2_BIT 4
#define EGL_OPENGL_BIT 8
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_OPENGL_API 0x30A2
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_PLATFORM_GBM_KHR 0x31D7
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_LINUX_DMA_BUF_EXT 0x3270
#define EGL_LINUX_DRM_FOURCC_EXT 0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT 0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT 0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT 0x3274
typedef void *EGLImage;

static struct
{
  EGLDisplay (*GetPlatformDisplay) (EGLenum, void *, const intptr_t *);
  EGLBoolean (*Initialize) (EGLDisplay, EGLint *, EGLint *);
  EGLBoolean (*BindAPI) (EGLenum);
  EGLBoolean (*ChooseConfig) (EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
  EGLContext (*CreateContext) (EGLDisplay, EGLConfig, EGLContext, const EGLint *);
  EGLBoolean (*MakeCurrent) (EGLDisplay, EGLSurface, EGLSurface, EGLContext);
  EGLBoolean (*DestroyContext) (EGLDisplay, EGLContext);
  EGLint (*GetError) (void);
  void (*(*GetProcAddress) (const char *)) (void);
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
static EGLDisplay display;

/* The context's API. The program asks for one (OP_CONTEXT; GLES 2 if it
   doesn't): GLES 2 where Linux's Mesa has it, otherwise desktop OpenGL,
   which has the same functions (ARB_ES2_compatibility) and takes GLES
   shaders marked "#version 100" (OP_glShaderSource adds the mark); or
   desktop OpenGL's compatibility profile, for OpenGL 1.x and 2.x. */
static int have_context, desktop_gl, es_on_desktop, pack_ext, es1;

/* Linux's EGL, and a display on the GPU's render node, or Mesa's
   software renderer: 1, or 0 with gl_error */
static int
gl_start (void)
{
  if (access (GLDIR "/lib", F_OK) == 0)
    {
      setenv ("__EGL_VENDOR_LIBRARY_DIRS", GLDIR "/share/glvnd/egl_vendor.d", 0);
      setenv ("LIBGL_DRIVERS_PATH", GLDIR "/lib/dri", 0);
      setenv ("GBM_BACKENDS_PATH", GLDIR "/lib/gbm", 0);
    }
  void *e = lib ("libEGL.so.1");
  if (!e)
    {
      snprintf (gl_error, sizeof gl_error, "Linux has no EGL library (%s)", dlerror ());
      return 0;
    }
#define E(n) if (!(egl.n = dlsym (e, "egl" #n))) { snprintf (gl_error, sizeof gl_error, "egl" #n " missing"); return 0; }
  E (GetPlatformDisplay) E (Initialize) E (BindAPI) E (ChooseConfig) E (CreateContext)
  E (MakeCurrent) E (DestroyContext) E (GetError) E (GetProcAddress)
#undef E
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
      EGLDisplay d = dev ? egl.GetPlatformDisplay (EGL_PLATFORM_GBM_KHR, dev, 0) : 0;
      if (d && egl.Initialize (d, 0, 0))
	{
	  display = d;
	  hardware = 1;
	  return 1;
	}
      close (fd);
    }
  return 0;
}

static int
software_display (void)
{
  EGLDisplay d = egl.GetPlatformDisplay (EGL_PLATFORM_SURFACELESS_MESA, 0, 0);
  if (!d || !egl.Initialize (d, 0, 0))
    {
      snprintf (gl_error, sizeof gl_error, "EGL would not start (error &%X)", egl.GetError ());
      return 0;
    }
  display = d;
  hardware = 0;
  return 1;
}

/* OpenGL above its compatibility profile's version: (0.5 on) a context
   of that version, for a program that asks for it */
static int want_version;

static EGLContext
make_on (EGLDisplay d, int desktop)
{
  static const EGLint want_es[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, 0, EGL_NONE };
  static const EGLint want_gl[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_SURFACE_TYPE, 0, EGL_NONE };
  static const EGLint ver[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  EGLConfig cfg = 0;
  EGLint n = 0;
  EGLContext c = 0;
  egl.BindAPI (desktop ? EGL_OPENGL_API : EGL_OPENGL_ES_API);
  if (egl.ChooseConfig (d, desktop ? want_gl : want_es, &cfg, 1, &n) && n >= 1)
    {
      if (desktop && want_version > 21)
	{
	  /* EGL_CONTEXT_MAJOR_VERSION, EGL_CONTEXT_MINOR_VERSION: tried
	     after the compatibility profile, if that is too old */
	  c = egl.CreateContext (d, cfg, 0, 0);
	  if (c && egl.MakeCurrent (d, 0, 0, c))
	    {
	      const char *(*gs) (GLenum) = (const char *(*) (GLenum)) egl.GetProcAddress ("glGetString");
	      const char *v = gs ? gs (GL_VERSION) : 0;
	      int ma = 0, mi = 0;
	      if (v && sscanf (v, "%d.%d", &ma, &mi) == 2 && ma * 10 + mi >= want_version)
		return c;
	      egl.MakeCurrent (d, 0, 0, 0);
	    }
	  if (c)
	    egl.DestroyContext (d, c);
	  const EGLint vv[] = { 0x3098, want_version / 10, 0x30FB, want_version % 10, EGL_NONE };
	  c = egl.CreateContext (d, cfg, 0, vv);
	}
      else
	c = egl.CreateContext (d, cfg, 0, desktop ? 0 : ver);
    }
  if (c && !egl.MakeCurrent (d, 0, 0, c))
    {
      egl.DestroyContext (d, c);
      c = 0;
    }
  return c;
}

/* the driver's functions, by name: from the API's library, else from EGL */
static int
load_table (int desktop)
{
  void *g = desktop ? lib ("libOpenGL.so.0") : lib ("libGLESv2.so.2");
  if (!g && desktop)
    g = lib ("libGL.so.1");
  void **fp = (void **) &gl;
  for (int i = 0; gl_names[i]; i++)
    {
      fp[i] = g ? dlsym (g, gl_names[i]) : 0;
      if (!fp[i])
	fp[i] = (void *) egl.GetProcAddress (gl_names[i]);
    }
  /* what the helper itself needs */
#define NEED(n) if (!gl.n) { snprintf (gl_error, sizeof gl_error, "gl" #n " missing from Linux's GL"); return 0; }
  NEED (GetIntegerv) NEED (GetString) NEED (BindTexture) NEED (TexImage2D) NEED (TexParameteri)
  NEED (GenFramebuffers) NEED (GenRenderbuffers) NEED (BindFramebuffer) NEED (BindRenderbuffer)
  NEED (RenderbufferStorage) NEED (FramebufferTexture2D) NEED (FramebufferRenderbuffer)
  NEED (Viewport) NEED (Scissor) NEED (PixelStorei) NEED (ReadPixels) NEED (BindBuffer)
  NEED (BufferData) NEED (VertexAttribPointer) NEED (DrawElements)
#undef NEED
  return 1;
}

/* a context of the API asked for (desktop: OpenGL's compatibility
   profile), current: 1, or 0 with gl_error */
static int
make_context (int desktop)
{
  EGLContext c = 0;
  int es_desk = 0;
  /* GLES 1.1: on desktop OpenGL (Debian's Mesa has no GLES 1: es1.c) */
  es1 = desktop == 2;
  if (es1)
    desktop = 1;
  for (int tries = 0; tries < 2 && !c; tries++)
    {
      if (tries && (!hardware || !software_display ()))
	break;
      c = make_on (display, desktop);
      if (!c && !desktop)
	{
	  c = make_on (display, 1);
	  es_desk = c != 0;
	}
    }
  if (!c)
    {
      snprintf (gl_error, sizeof gl_error, "no %s context (EGL error &%X)",
		es1 ? "OpenGL (for GLES 1.1)" : desktop ? "OpenGL" : "GLES 2 or OpenGL", egl.GetError ());
      return 0;
    }
  desktop_gl = desktop || es_desk;
  es_on_desktop = es_desk;
  if (!load_table (desktop_gl))
    return 0;
  if (es1)
    es1_install ();
  strict_ctx = desktop_gl && !gl.GetString (GL_EXTENSIONS);
  if (strict_ctx)
    gl.GetError ();
  /* the pack and unpack settings past alignment: OpenGL, and GLES 3 */
  const char *v = (const char *) gl.GetString (GL_VERSION);
  pack_ext = desktop_gl || (v && strstr (v, "OpenGL ES ") && v[10] >= '3');
  have_context = 1;
  return 1;
}

/* --- the surface: a framebuffer object of ours that is the program's
   framebuffer 0 ---------------------------------------------------------- */

static GLuint fbo, fb_depth, app_fb;
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
      gl.GenRenderbuffers (1, &fb_depth);
    }
  gl.BindTexture (GL_TEXTURE_2D, PRIV_FB_TEX);
  gl.TexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  gl.TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl.TexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl.BindRenderbuffer (GL_RENDERBUFFER, fb_depth);
  const char *ext = (const char *) gl.GetString (GL_EXTENSIONS);
  int packed = desktop_gl || (ext && strstr (ext, "GL_OES_packed_depth_stencil"));
  gl.RenderbufferStorage (GL_RENDERBUFFER, packed ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT16, w, h);
  gl.BindFramebuffer (GL_FRAMEBUFFER, fbo);
  gl.FramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, PRIV_FB_TEX, 0);
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
  if (desktop_gl && gl.DrawBuffer && !app_fb)
    {
      gl.DrawBuffer (GL_COLOR_ATTACHMENT0);
      gl.ReadBuffer (GL_COLOR_ATTACHMENT0);
    }
}

/* pack settings: tight (alignment 1, no row length or skips) while the
   helper reads pixels for the program, which lays them out itself */
static GLint saved_pack[4];
static const GLenum pack_names[4] = { GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_PIXELS, GL_PACK_SKIP_ROWS };

static void
pack_tight (void)
{
  for (int i = 0; i < (pack_ext ? 4 : 1); i++)
    {
      gl.GetIntegerv (pack_names[i], &saved_pack[i]);
      gl.PixelStorei (pack_names[i], i == 0 ? 1 : 0);
    }
}

static void
pack_restore (void)
{
  for (int i = 0; i < (pack_ext ? 4 : 1); i++)
    gl.PixelStorei (pack_names[i], saved_pack[i]);
}

/* the finished frame, top row first, into the reply area */
static size_t present_frame (char *dst, size_t room);

static void
present (void)
{
  uint64_t t = now_ns ();
  area->reply_len = (uint32_t) present_frame (reply_area, area->reply_bytes);
  st_present += now_ns () - t;
}

/* overlapped frames: frame n into its half of the reply area, while the
   program goes on with the next; the program waits only for frame n - 1 */
static void
present_slot (uint32_t n)
{
  uint64_t t = now_ns ();
  size_t half = area->reply_bytes / 2;
  present_frame (reply_area + (n & 1) * half, half);
  __atomic_store_n (&area->frame_done, n, __ATOMIC_RELEASE);
  st_present += now_ns () - t;
}

/* the finished frame, top row first, at dst: its size, or 0 */
static size_t
present_frame (char *dst, size_t room)
{
  size_t row = (size_t) fb_w * 4, n = row * (size_t) fb_h;
  if (!fbo || n > room)
    return 0;
  GLint pbo = 0;
  if (pack_ext)
    {
      gl.GetIntegerv (GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
      if (pbo)
	gl.BindBuffer (GL_PIXEL_PACK_BUFFER, 0);
    }
  pack_tight ();
  gl.BindFramebuffer (GL_FRAMEBUFFER, fbo);
  char *t = scratch (n);
  gl.ReadPixels (0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, t);
  for (int y = 0; y < fb_h; y++)
    memcpy (dst + row * (size_t) y, t + row * (size_t) (fb_h - 1 - y), row);
  pack_restore ();
  if (pbo)
    gl.BindBuffer (GL_PIXEL_PACK_BUFFER, (GLuint) pbo);
  gl.BindFramebuffer (GL_FRAMEBUFFER, app_fb ? app_fb : fbo);
  return n;
}


/* --- zero-copy: the frame on a display plane (rol-fb's overlays) --------

   With the program's window placed (OP_OVERLAY), each frame is copied by
   the GPU into one of two buffers that the display itself shows, on a
   plane under the desktop, through rol-fb's socket "rol-fb-overlay"
   (images/fe/rol-fb.c in riscos-linux-testbed). Nothing is read back. The
   picture shows where the desktop has the key colour (0x080008) inside the
   window's rectangle: the program paints its work area so, and windows over
   it clip it for nothing.

   The buffers are dumb buffers of the display's card (contiguous memory the
   display can scan out), shared with the GPU as dma-bufs. The main process
   (root) opens the card for each connection: card_fd. Any failure stops
   the overlay (area->overlay_error); the program goes back to read-back. */

static int card_fd = -1;
static int ov_sock = -1, ov_on, ov_failed, ov_formats, ov_under, ov_shown = -1;
static int ov_x, ov_y, ov_w, ov_h, ov_visible;
static struct ovbuf
{
  int fd;
  uint32_t handle, pitch;
  int w, h;
  EGLImage image;
  GLuint rb, fb;
} ov_buf[2];
static EGLImage (*CreateImageKHR) (EGLDisplay, EGLContext, EGLenum, void *, const EGLint *);
static EGLBoolean (*DestroyImageKHR) (EGLDisplay, EGLImage);
static void (*ImageTargetRenderbuffer) (GLenum, void *);

/* one message (with fd if >= 0) and its reply: the reply's [0] */
static int
ov_call (const int32_t *m, int fd, int32_t *r)
{
  union { char buf[CMSG_SPACE (sizeof (int))]; struct cmsghdr align; } ctl;
  struct iovec iov = { (void *) m, 16 * sizeof (int32_t) };
  struct msghdr msg = { .msg_iov = &iov, .msg_iovlen = 1 };
  if (fd >= 0)
    {
      msg.msg_control = ctl.buf;
      msg.msg_controllen = sizeof ctl.buf;
      struct cmsghdr *c = CMSG_FIRSTHDR (&msg);
      c->cmsg_level = SOL_SOCKET;
      c->cmsg_type = SCM_RIGHTS;
      c->cmsg_len = CMSG_LEN (sizeof (int));
      memcpy (CMSG_DATA (c), &fd, sizeof fd);
    }
  memset (r, 0, 16 * sizeof (int32_t));
  if (sendmsg (ov_sock, &msg, MSG_NOSIGNAL) < 0)
    return -errno;
  ssize_t n = recv (ov_sock, r, 16 * sizeof (int32_t), 0);
  return n <= 0 ? -EPIPE : r[0];
}

static void
ov_free_bufs (void)
{
  for (int i = 0; i < 2; i++)
    {
      struct ovbuf *b = &ov_buf[i];
      if (b->fb)
	gl.DeleteFramebuffers (1, &b->fb);
      if (b->rb)
	gl.DeleteRenderbuffers (1, &b->rb);
      if (b->image)
	DestroyImageKHR (display, b->image);
      if (b->fd >= 0)
	close (b->fd);
      if (b->handle)
	{
	  struct drm_mode_destroy_dumb dd = { .handle = b->handle };
	  ioctl (card_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
	}
      memset (b, 0, sizeof *b);
      b->fd = -1;
    }
  ov_shown = -1;
}

/* the overlay stops: the program goes back to reading frames */
static void
ov_fail (const char *what, int e)
{
  if (!ov_failed)
    logmsg ("rol-3d: overlay: %s (%s); frames are read back\n", what, e < 0 ? strerror (-e) : "failed");
  ov_failed = 1;
  ov_on = 0;
  __atomic_store_n (&area->overlay_error, e < 0 ? (uint32_t) -e : 1u, __ATOMIC_RELEASE);
  if (have_context && gl.DeleteFramebuffers)
    ov_free_bufs ();
  if (ov_sock >= 0)
    close (ov_sock);
  ov_sock = -1;
}

/* connects and opens an RGB overlay: 0, or -errno */
static int
ov_open (void)
{
  if (card_fd < 0)
    return -ENODEV;
  if (!hardware)
    return -ENODEV;		/* (the software renderer can't share buffers) */
  CreateImageKHR = (void *) egl.GetProcAddress ("eglCreateImageKHR");
  DestroyImageKHR = (void *) egl.GetProcAddress ("eglDestroyImageKHR");
  ImageTargetRenderbuffer = (void *) egl.GetProcAddress ("glEGLImageTargetRenderbufferStorageOES");
  if (!CreateImageKHR || !DestroyImageKHR || !ImageTargetRenderbuffer || !gl.BlitFramebuffer
      || !gl.DeleteFramebuffers || !gl.DeleteRenderbuffers || !gl.Finish)
    return -ENOSYS;
  for (int i = 0; i < 2; i++)
    ov_buf[i].fd = -1;
  ov_sock = socket (AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  struct sockaddr_un a = { .sun_family = AF_UNIX };
  memcpy (a.sun_path, "\0rol-fb-overlay", 15);
  if (ov_sock < 0 || connect (ov_sock, (struct sockaddr *) &a, offsetof (struct sockaddr_un, sun_path) + 15))
    {
      int e = -errno;
      if (ov_sock >= 0)
	close (ov_sock);
      ov_sock = -1;
      return e;
    }
  int32_t m[16] = { 1, 1, 1 }, r[16];
  int e = ov_call (m, -1, r);
  if (e)
    {
      close (ov_sock);
      ov_sock = -1;
      return e;
    }
  ov_under = r[2];
  ov_formats = r[4];
  if (!(ov_formats & 5))
    {
      close (ov_sock);
      ov_sock = -1;
      return -EINVAL;
    }
  ov_on = 1;
  logmsg ("rol-3d: overlay open (formats %d, under the desktop %d, screen %dx%d)\n", r[4], r[2], r[5], r[6]);
  return 0;
}

/* two buffers the surface's size: 0, or -errno */
static int
ov_bufs (void)
{
  if (ov_buf[0].fb && ov_buf[0].w == fb_w && ov_buf[0].h == fb_h)
    return 0;
  if (ov_buf[0].fb)
    {
      int32_t m[16] = { 5 }, r[16];	/* FORGET */
      ov_call (m, -1, r);
      ov_free_bufs ();
    }
  uint32_t fourcc = ov_formats & 4 ? DRM_FORMAT_XBGR8888 : DRM_FORMAT_XRGB8888;
  GLint rb = 0;
  gl.GetIntegerv (GL_RENDERBUFFER_BINDING, &rb);
  for (int i = 0; i < 2; i++)
    {
      struct ovbuf *b = &ov_buf[i];
      struct drm_mode_create_dumb cd = { .width = (uint32_t) fb_w, .height = (uint32_t) fb_h, .bpp = 32 };
      if (ioctl (card_fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd))
	return -errno;
      b->handle = cd.handle;
      b->pitch = cd.pitch;
      b->w = fb_w;
      b->h = fb_h;
      struct drm_prime_handle ph = { .handle = cd.handle, .flags = DRM_CLOEXEC | DRM_RDWR, .fd = -1 };
      if (ioctl (card_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &ph))
	return -errno;
      b->fd = ph.fd;
      const EGLint at[] = { EGL_WIDTH, fb_w, EGL_HEIGHT, fb_h, EGL_LINUX_DRM_FOURCC_EXT, (EGLint) fourcc,
			    EGL_DMA_BUF_PLANE0_FD_EXT, b->fd, EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
			    EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint) b->pitch, EGL_NONE };
      b->image = CreateImageKHR (display, 0, EGL_LINUX_DMA_BUF_EXT, 0, at);
      if (!b->image)
	return -EINVAL;
      gl.GenRenderbuffers (1, &b->rb);
      gl.BindRenderbuffer (GL_RENDERBUFFER, b->rb);
      ImageTargetRenderbuffer (GL_RENDERBUFFER, b->image);
      gl.GenFramebuffers (1, &b->fb);
      gl.BindFramebuffer (GL_FRAMEBUFFER, b->fb);
      gl.FramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, b->rb);
      GLenum st = gl.CheckFramebufferStatus ? gl.CheckFramebufferStatus (GL_FRAMEBUFFER) : GL_FRAMEBUFFER_COMPLETE;
      gl.BindFramebuffer (GL_FRAMEBUFFER, app_fb ? app_fb : fbo);
      if (st != GL_FRAMEBUFFER_COMPLETE)
	return -EINVAL;
      int32_t m[16] = { 2, i, fb_w, fb_h, (int32_t) fourcc, (int32_t) b->pitch, 0 }, r[16];
      int e = ov_call (m, b->fd, r);
      if (e)
	return e;
    }
  gl.BindRenderbuffer (GL_RENDERBUFFER, (GLuint) rb);
  return 0;
}

/* SHOW buffer i at the window's place: 0, or -errno */
static int
ov_show (int i)
{
  int32_t m[16] = { 3, i, ov_x, ov_y, ov_w, ov_h, 0, 0, ov_buf[i].w, ov_buf[i].h, 0 }, r[16];
  int e = ov_call (m, -1, r);
  if (!e)
    ov_shown = i;
  return e;
}

/* OP_OVERLAY: the window's place on the screen, in RISC OS pixels from
   the top left (w 0: hidden). Answers [0] 1 (frames go to the overlay),
   0 (they can't: [1] the error number), [1] the key colour, [2] 1 if the
   overlay is under the desktop */
static void
overlay_place (const uint32_t *a)
{
  int32_t r[3] = { 0, 0, 0 };
  if (ov_failed)
    {
      /* (asked again after a failure: try once more, e.g. after a mode change) */
      ov_failed = 0;
      __atomic_store_n (&area->overlay_error, 0, __ATOMIC_RELEASE);
    }
  int e = ov_on ? 0 : ov_open ();
  if (e)
    {
      r[1] = -e;
      reply (r, sizeof r);
      return;
    }
  ov_x = (int32_t) a[0];
  ov_y = (int32_t) a[1];
  ov_w = (int32_t) a[2];
  ov_h = (int32_t) a[3];
  ov_visible = ov_w > 0 && ov_h > 0;
  if (!ov_visible && ov_shown >= 0)
    {
      int32_t m[16] = { 4 }, rr[16];	/* HIDE */
      ov_call (m, -1, rr);
      ov_shown = -1;
    }
  else if (ov_visible && ov_shown >= 0 && (e = ov_show (ov_shown)))
    {
      ov_fail ("SHOW at the window's new place", e);
      r[1] = -e;
      reply (r, sizeof r);
      return;
    }
  r[0] = 1;
  r[1] = 0x080008;
  r[2] = ov_under;
  reply (r, sizeof r);
}

/* OP_PRESENT_OVERLAY: frame n, upside down (GL's rows are bottom first)
   into the buffer not on the screen, then shown; SHOW's answer comes at
   the display's next frame, so this paces the program at the display's
   rate, a frame behind (the program waits only for frame n - 1) */
static void
present_overlay (uint32_t n)
{
  uint64_t t = now_ns ();
  int e = 0;
  if (!ov_on || !fbo)
    e = -ENODEV;
  else if ((e = ov_bufs ()))
    ov_fail ("the buffers (dumb buffers shared with the GPU)", e);
  else if (ov_visible)
    {
      int i = ov_shown == 0 ? 1 : 0;
      GLboolean scissor = gl.IsEnabled (GL_SCISSOR_TEST);
      if (scissor)
	gl.Disable (GL_SCISSOR_TEST);
      gl.BindFramebuffer (GL_READ_FRAMEBUFFER, fbo);
      gl.BindFramebuffer (GL_DRAW_FRAMEBUFFER, ov_buf[i].fb);
      gl.BlitFramebuffer (0, 0, fb_w, fb_h, 0, fb_h, fb_w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
      gl.BindFramebuffer (GL_FRAMEBUFFER, app_fb ? app_fb : fbo);
      if (scissor)
	gl.Enable (GL_SCISSOR_TEST);
      gl.Finish ();
      if ((e = ov_show (i)))
	ov_fail ("SHOW (a mode change?)", e);
    }
  if (e && !ov_failed)
    __atomic_store_n (&area->overlay_error, (uint32_t) -e, __ATOMIC_RELEASE);
  __atomic_store_n (&area->frame_done, n, __ATOMIC_RELEASE);
  st_present += now_ns () - t;
}

/* --- the commands written by hand at both ends (lib/r3d_gl.c) ----------- */

static void
text_reply (void (*get) (GLuint, GLsizei, GLsizei *, GLchar *), GLuint obj, uint32_t max)
{
  if (max > 65536)
    max = 65536;
  uint32_t *r = scratch (max + 8);
  GLsizei l = 0;
  if (get)
    get (obj, (GLsizei) max, &l, (GLchar *) (r + 1));
  r[0] = (uint32_t) l;
  reply (r, (size_t) l + 4);
}

/* an image's pixels: from a pixel unpack buffer (an offset), from the
   command, or none. f: the flag, offset and byte count words */
static const void *
pixels_at (const uint32_t *f)
{
  if (f[0])
    return (const void *) (uintptr_t) f[1];
  return f[2] ? (const void *) (f + 3) : 0;
}

static GLfloat *feedback_buf, *select_buf;	/* (select's are GLuint) */
static GLsizei feedback_size, select_size;

/* a fixed-function array from the program's memory: into a buffer of
   ours. Slots: vertex 0, normal 1, colour 2, index 3, edge flag 4, fog
   coordinate 5, secondary colour 6, texture coordinates 8 + unit */
static void
fixed_array (const uint32_t *a)
{
  uint32_t which = a[0], unit = a[1], slot = which < 8 ? which : 8 + (unit & 7);
  GLint size = (GLint) a[2], stride = (GLint) a[4];
  GLenum type = a[3];
  GLint was = 0;
  gl.GetIntegerv (GL_ARRAY_BUFFER_BINDING, &was);
  gl.BindBuffer (GL_ARRAY_BUFFER, PRIV_FIXED (slot));
  gl.BufferData (GL_ARRAY_BUFFER, (GLsizeiptr) a[5], a + 6, GL_STREAM_DRAW);
  switch (which)
    {
    case 0: if (gl.VertexPointer) gl.VertexPointer (size, type, stride, 0); break;
    case 1: if (gl.NormalPointer) gl.NormalPointer (type, stride, 0); break;
    case 2: if (gl.ColorPointer) gl.ColorPointer (size, type, stride, 0); break;
    case 3: if (gl.IndexPointer) gl.IndexPointer (type, stride, 0); break;
    case 4: if (gl.EdgeFlagPointer) gl.EdgeFlagPointer (stride, 0); break;
    case 5: if (gl.FogCoordPointer) gl.FogCoordPointer (type, stride, 0); break;
    case 6: if (gl.SecondaryColorPointer) gl.SecondaryColorPointer (size, type, stride, 0); break;
    case 7: if (gl.PointSizePointerOES) gl.PointSizePointerOES (type, stride, 0); break;
    default:
      if (gl.TexCoordPointer && gl.ClientActiveTexture)
	{
	  GLint cat = GL_TEXTURE0;
	  gl.GetIntegerv (GL_CLIENT_ACTIVE_TEXTURE, &cat);
	  gl.ClientActiveTexture (GL_TEXTURE0 + unit);
	  gl.TexCoordPointer (size, type, stride, 0);
	  gl.ClientActiveTexture ((GLenum) cat);
	}
      break;
    }
  gl.BindBuffer (GL_ARRAY_BUFFER, (GLuint) was);
}

/* immediate mode the library collected: mode, count, layout (colour 1,
   normal 2, texture coordinates 4 << unit), whether the program's own
   arrays are all off (else they are saved and restored around ours), then
   the vertices: position, colour, normal, texture coordinates (floats) */
static void
immediate (const uint32_t *a)
{
  uint32_t layout = a[2], stride = 4, fast = a[3];
  GLsizei count = (GLsizei) a[1];
  if (layout & 1) stride += 4;
  if (layout & 2) stride += 3;
  for (int u = 0; u < 4; u++)
    if (layout & (4u << u))
      stride += 4;
  if (!gl.VertexPointer || !gl.EnableClientState || count <= 0)
    return;
  GLint was = 0, cat = GL_TEXTURE0;
  gl.GetIntegerv (GL_ARRAY_BUFFER_BINDING, &was);
  gl.GetIntegerv (GL_CLIENT_ACTIVE_TEXTURE, &cat);
  if (!fast)
    {
      static GLint attribs;
      if (!attribs)
	gl.GetIntegerv (GL_MAX_VERTEX_ATTRIBS, &attribs);
      gl.PushClientAttrib (GL_CLIENT_VERTEX_ARRAY_BIT);
      static const GLenum off[] = { GL_NORMAL_ARRAY, GL_COLOR_ARRAY, GL_INDEX_ARRAY, GL_EDGE_FLAG_ARRAY,
				    GL_FOG_COORD_ARRAY, GL_SECONDARY_COLOR_ARRAY };
      for (unsigned i = 0; i < sizeof off / sizeof off[0]; i++)
	gl.DisableClientState (off[i]);
      for (int u = 0; u < 8; u++)
	{
	  gl.ClientActiveTexture (GL_TEXTURE0 + (GLenum) u);
	  gl.DisableClientState (GL_TEXTURE_COORD_ARRAY);
	}
      for (GLint i = 0; i < attribs && i < 16; i++)
	gl.DisableVertexAttribArray ((GLuint) i);
    }
  GLsizei bytes = (GLsizei) (stride * 4);
  gl.BindBuffer (GL_ARRAY_BUFFER, PRIV_IMMEDIATE);
  gl.BufferData (GL_ARRAY_BUFFER, (GLsizeiptr) count * bytes, a + 4, GL_STREAM_DRAW);
  uintptr_t at = 16;
  gl.VertexPointer (4, GL_FLOAT, bytes, 0);
  gl.EnableClientState (GL_VERTEX_ARRAY);
  if (layout & 1)
    {
      gl.ColorPointer (4, GL_FLOAT, bytes, (const void *) at);
      gl.EnableClientState (GL_COLOR_ARRAY);
      at += 16;
    }
  if (layout & 2)
    {
      gl.NormalPointer (GL_FLOAT, bytes, (const void *) at);
      gl.EnableClientState (GL_NORMAL_ARRAY);
      at += 12;
    }
  for (int u = 0; u < 4; u++)
    if (layout & (4u << u))
      {
	gl.ClientActiveTexture (GL_TEXTURE0 + (GLenum) u);
	gl.TexCoordPointer (4, GL_FLOAT, bytes, (const void *) at);
	gl.EnableClientState (GL_TEXTURE_COORD_ARRAY);
	at += 16;
      }
  gl.DrawArrays (a[0], 0, count);
  if (fast)
    {
      gl.DisableClientState (GL_VERTEX_ARRAY);
      if (layout & 1)
	gl.DisableClientState (GL_COLOR_ARRAY);
      if (layout & 2)
	gl.DisableClientState (GL_NORMAL_ARRAY);
      for (int u = 0; u < 4; u++)
	if (layout & (4u << u))
	  {
	    gl.ClientActiveTexture (GL_TEXTURE0 + (GLenum) u);
	    gl.DisableClientState (GL_TEXTURE_COORD_ARRAY);
	  }
    }
  else
    gl.PopClientAttrib ();
  gl.ClientActiveTexture ((GLenum) cat);
  gl.BindBuffer (GL_ARRAY_BUFFER, (GLuint) was);
}

/* framebuffer 0 is ours: what the program says of its back and front
   buffers is said of our framebuffer's colour */
static GLenum
own_buffer (GLenum b)
{
  if (app_fb || !fbo)
    return b;
  switch (b)
    {
    case GL_BACK: case GL_FRONT: case GL_FRONT_AND_BACK: case GL_LEFT:
    case GL_BACK_LEFT: case GL_FRONT_LEFT:
      return GL_COLOR_ATTACHMENT0;
    }
  return b;
}

/* the draws with instances or a base vertex (lib/r3d_gl.c:
   draw_elements_ex): how (1 instanced, 2 base vertex, 4 range; 0x100: the
   indices follow), mode, count, type, instances, base vertex, start, end,
   then the offset in the element buffer, or the indices' bytes and them */
static void
draw_elements_ex (const uint32_t *a)
{
  uint32_t how = a[0];
  GLenum mode = a[1], type = a[3];
  GLsizei count = (GLsizei) a[2], inst = (GLsizei) a[4];
  GLint base = (GLint) a[5];
  const void *at = (const void *) (uintptr_t) a[8];
  if (how & 0x100)
    {
      gl.BindBuffer (GL_ELEMENT_ARRAY_BUFFER, PRIV_ELEMENTS);
      gl.BufferData (GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) a[8], a + 9, GL_STREAM_DRAW);
      at = 0;
    }
  switch (how & 7)
    {
    case 1:
      if (gl.DrawElementsInstanced)
	gl.DrawElementsInstanced (mode, count, type, at, inst);
      break;
    case 2:
      if (gl.DrawElementsBaseVertex)
	gl.DrawElementsBaseVertex (mode, count, type, at, base);
      break;
    case 3:
      if (gl.DrawElementsInstancedBaseVertex)
	gl.DrawElementsInstancedBaseVertex (mode, count, type, at, inst, base);
      break;
    case 6:
      if (gl.DrawRangeElementsBaseVertex)
	gl.DrawRangeElementsBaseVertex (mode, a[6], a[7], count, type, at, base);
      break;
    }
  if (how & 0x100)
    gl.BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

/* debug messages kept for the program's callback (it can't be called from
   here): the driver calls ours, which keeps them until the library asks */
#define DEBUG_KEEP 256
static struct dmsg { GLenum source, type, severity; GLuint id; char *text; } dq[DEBUG_KEEP];
static int dq_head, dq_n, debug_kept;

static void
#ifdef __GNUC__
__attribute__ ((unused))
#endif
on_debug (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user)
{
  (void) user;
  if (dq_n == DEBUG_KEEP)
    {
      free (dq[dq_head].text);
      dq_head = (dq_head + 1) % DEBUG_KEEP;
      dq_n--;
    }
  struct dmsg *m = &dq[(dq_head + dq_n) % DEBUG_KEEP];
  size_t l = length >= 0 ? (size_t) length : strlen (message);
  m->text = malloc (l + 1);
  if (!m->text)
    return;
  memcpy (m->text, message, l);
  m->text[l] = 0;
  m->source = source;
  m->type = type;
  m->id = id;
  m->severity = severity;
  dq_n++;
  __atomic_store_n (&area->debug_pending, (uint32_t) dq_n, __ATOMIC_RELEASE);
}

static void
debug_callback (uint32_t on)
{
  if (!gl.DebugMessageCallback)
    return;
  debug_kept = (int) on;
  gl.DebugMessageCallback (on ? (GLDEBUGPROC) on_debug : 0, 0);
}

/* glGetDebugMessageLog's answer: a count, then for each source, type, id,
   severity, length (with the 0), the text padded to a word */
static void
debug_log (uint32_t count, uint32_t bufsize)
{
  uint32_t *r = (uint32_t *) reply_area, *w = r + 1;
  size_t room = area->reply_bytes / 4, used = 1, n = 0, text = 0;
  if (debug_kept)
    {
      while (n < count && dq_n)
	{
	  struct dmsg *m = &dq[dq_head];
	  size_t l = strlen (m->text) + 1, need = 5 + (l + 3) / 4;
	  if (used + need > room || text + l > bufsize)
	    break;
	  w[0] = m->source; w[1] = m->type; w[2] = m->id; w[3] = m->severity; w[4] = (uint32_t) l;
	  memcpy (w + 5, m->text, l);
	  w += need;
	  used += need;
	  text += l;
	  n++;
	  free (m->text);
	  dq_head = (dq_head + 1) % DEBUG_KEEP;
	  dq_n--;
	}
      __atomic_store_n (&area->debug_pending, (uint32_t) dq_n, __ATOMIC_RELEASE);
    }
  else if (gl.GetDebugMessageLog)
    {
      /* the driver's own log, one message at a time */
      static GLchar t[65536];
      while (n < count)
	{
	  GLenum src, type, sev;
	  GLuint id;
	  GLsizei l = 0;
	  if (!gl.GetDebugMessageLog (1, (GLsizei) sizeof t, &src, &type, &id, &sev, &l, t) || l <= 0)
	    break;
	  size_t need = 5 + ((size_t) l + 3) / 4;
	  if (used + need > room || text + (size_t) l > bufsize)
	    break;			/* (that one is lost) */
	  w[0] = src; w[1] = type; w[2] = id; w[3] = sev; w[4] = (uint32_t) l;
	  memcpy (w + 5, t, (size_t) l);
	  w += need;
	  used += need;
	  text += (size_t) l;
	  n++;
	}
    }
  r[0] = (uint32_t) n;
  area->reply_len = (uint32_t) used * 4;
}

/* GL_EXT_direct_state_access's framebuffer functions (a framebuffer first) */
static int
named_fb (uint32_t op)
{
  switch (op)
    {
    case OP_glCheckNamedFramebufferStatusEXT: case OP_glNamedFramebufferTexture1DEXT:
    case OP_glNamedFramebufferTexture2DEXT: case OP_glNamedFramebufferTexture3DEXT:
    case OP_glNamedFramebufferRenderbufferEXT: case OP_glGetNamedFramebufferAttachmentParameterivEXT:
    case OP_glFramebufferDrawBufferEXT: case OP_glFramebufferDrawBuffersEXT:
    case OP_glFramebufferReadBufferEXT: case OP_glGetFramebufferParameterivEXT:
    case OP_glNamedFramebufferTextureEXT: case OP_glNamedFramebufferTextureLayerEXT:
    case OP_glNamedFramebufferTextureFaceEXT: case OP_glNamedFramebufferParameteriEXT:
    case OP_glGetNamedFramebufferParameterivEXT:
      return 1;
    }
  return 0;
}

/* the program's buffer names for framebuffer 0, as ours */
static GLenum
fb0_buffer (GLenum b)
{
  switch (b)
    {
    case GL_BACK: case GL_FRONT: case GL_FRONT_AND_BACK: case GL_LEFT:
    case GL_BACK_LEFT: case GL_FRONT_LEFT:
      return GL_COLOR_ATTACHMENT0;
    }
  return b;
}

/* GL_EXT_direct_state_access done as the bound object's functions: an
   object bound for the commands between OP_TEMP_BIND and OP_TEMP_END */
static struct { int on; uint32_t kind; GLenum target; GLint was; } temp;

static GLenum
binding_of (uint32_t kind, GLenum target)
{
  if (kind == 1)
    switch (target)
      {
      case GL_COPY_WRITE_BUFFER: return GL_COPY_WRITE_BUFFER_BINDING;
      case GL_ARRAY_BUFFER: return GL_ARRAY_BUFFER_BINDING;
      }
  switch (target)
    {
    case GL_TEXTURE_1D: return GL_TEXTURE_BINDING_1D;
    case GL_TEXTURE_2D: return GL_TEXTURE_BINDING_2D;
    case GL_TEXTURE_3D: return GL_TEXTURE_BINDING_3D;
    case GL_TEXTURE_1D_ARRAY: return GL_TEXTURE_BINDING_1D_ARRAY;
    case GL_TEXTURE_2D_ARRAY: return GL_TEXTURE_BINDING_2D_ARRAY;
    case GL_TEXTURE_RECTANGLE: return GL_TEXTURE_BINDING_RECTANGLE;
    case GL_TEXTURE_BUFFER: return GL_TEXTURE_BINDING_BUFFER;
    case GL_TEXTURE_CUBE_MAP: return GL_TEXTURE_BINDING_CUBE_MAP;
    case GL_TEXTURE_2D_MULTISAMPLE: return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
    case 0x9102: return 0x9105;		/* GL_TEXTURE_2D_MULTISAMPLE_ARRAY */
    }
  return 0;
}

static void
temp_bind (const uint32_t *a)
{
  GLenum target = a[1];
  if (a[0] == 0 && target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
    target = GL_TEXTURE_CUBE_MAP;
  GLenum b = binding_of (a[0], target);
  if (temp.on || !b)
    return;
  temp.on = 1;
  temp.kind = a[0];
  temp.target = target;
  temp.was = 0;
  gl.GetIntegerv (b, &temp.was);
  if (a[0] == 0)
    gl.BindTexture (target, a[2]);
  else
    gl.BindBuffer (target, a[2]);
}

static void
temp_end (void)
{
  if (!temp.on)
    return;
  temp.on = 0;
  if (temp.kind == 0)
    gl.BindTexture (temp.target, (GLuint) temp.was);
  else
    gl.BindBuffer (temp.target, (GLuint) temp.was);
}

/* the context's version: major * 10 + minor */
static uint32_t
context_version (void)
{
  const char *v = (const char *) gl.GetString (GL_VERSION);
  int major = 0, minor = 0;
  if (!v)
    return 0;
  if (!strncmp (v, "OpenGL ES ", 10))
    v += 10;
  if (sscanf (v, "%d.%d", &major, &minor) != 2)
    return 0;
  return (uint32_t) (major * 10 + minor);
}

/* sync objects: the program's numbers for Linux's pointers */
static GLsync *syncs;
static uint32_t nsyncs;

static GLsync
sync_of (uint32_t id)
{
  return id < nsyncs ? syncs[id] : 0;
}

static void
sync_ops (uint32_t op, const uint32_t *a)
{
  uint32_t r = 0;
  switch (op)
    {
    case OP_glFenceSync:
      if (a[0] >= nsyncs)
	{
	  uint32_t n = a[0] + 64;
	  GLsync *s = realloc (syncs, n * sizeof *s);
	  if (!s)
	    return;
	  memset (s + nsyncs, 0, (n - nsyncs) * sizeof *s);
	  syncs = s;
	  nsyncs = n;
	}
      if (gl.FenceSync)
	syncs[a[0]] = gl.FenceSync (a[1], a[2]);
      return;
    case OP_glDeleteSync:
      if (sync_of (a[0]) && gl.DeleteSync)
	gl.DeleteSync (syncs[a[0]]);
      if (a[0] < nsyncs)
	syncs[a[0]] = 0;
      return;
    case OP_glWaitSync:
      if (sync_of (a[0]) && gl.WaitSync)
	gl.WaitSync (syncs[a[0]], a[1], r3d_w2_64 (a + 2));
      return;
    case OP_glIsSync:
      r = sync_of (a[0]) && gl.IsSync ? gl.IsSync (syncs[a[0]]) : 0;
      break;
    case OP_glClientWaitSync:
      r = sync_of (a[0]) && gl.ClientWaitSync ? gl.ClientWaitSync (syncs[a[0]], a[1], r3d_w2_64 (a + 2)) : GL_WAIT_FAILED;
      break;
    case OP_glGetSynciv:
      {
	uint32_t w[65];
	GLsizei l = 0;
	memset (w, 0, sizeof w);
	if (sync_of (a[0]) && gl.GetSynciv)
	  gl.GetSynciv (syncs[a[0]], a[1], (GLsizei) (a[2] > 64 ? 64 : a[2]), &l, (GLint *) (w + 1));
	w[0] = (uint32_t) l;
	reply (w, sizeof w);
	return;
      }
    }
  reply (&r, 4);
}

static void
dispatch (uint32_t op, const uint32_t *a, uint32_t n)
{
  if (__builtin_expect (!have_context, 0))
    {
      int desktop = op == OP_CONTEXT ? (int) a[0] : 0;
      /* (0.5 on: the version asked for, major * 10 + minor) */
      want_version = op == OP_CONTEXT && n >= 2 ? (int) a[1] : 0;
      int ok = make_context (desktop);
      if (op == OP_CONTEXT)
	{
	  if (ok)
	    {
	      /* (0.5 on: and the version, major * 10 + minor) */
	      uint32_t w[4] = { 1, (uint32_t) desktop_gl, context_version (), ROL3D_LEVEL };
	      reply (w, 16);
	      return;
	    }
	  char r[8 + sizeof gl_error];
	  uint32_t w[2] = { 0, (uint32_t) desktop_gl };
	  memcpy (r, w, 8);
	  strcpy (r + 8, gl_error);
	  reply (r, 8 + strlen (r + 8) + 1);
	  _exit (1);
	}
      if (!ok)
	{
	  logmsg ("rol-3d: %s\n", gl_error);
	  _exit (1);
	}
      logmsg ("rol-3d: a context: %s, %s\n", (const char *) gl.GetString (GL_RENDERER), (const char *) gl.GetString (GL_VERSION));
    }
  else if (op == OP_CONTEXT)
    {
      uint32_t w[4] = { 1, (uint32_t) desktop_gl, context_version (), ROL3D_LEVEL };
      reply (w, 16);
      return;
    }
  /* GL_EXT_direct_state_access on framebuffer 0 (the program's, ours) */
  if (__builtin_expect (op >= OP_glCheckNamedFramebufferStatusEXT && named_fb (op), 0) && a[0] == 0 && fbo)
    {
      uint32_t b[64];
      if (n > 64)
	return;
      memcpy (b, a, n * 4);
      b[0] = fbo;
      if (op == OP_glFramebufferDrawBufferEXT || op == OP_glFramebufferReadBufferEXT)
	b[1] = fb0_buffer (b[1]);
      if (op == OP_glFramebufferDrawBuffersEXT)
	for (uint32_t i = 3; i < n && i < 3 + b[1]; i++)
	  b[i] = fb0_buffer (b[i]);
      if (op == OP_glGetFramebufferParameterivEXT)
	{
	  GLint r[64];
	  memset (r, 0, sizeof r);
	  if (gl.GetFramebufferParameterivEXT)
	    gl.GetFramebufferParameterivEXT (fbo, b[1], r);
	  if (r[0] == GL_COLOR_ATTACHMENT0 && (b[1] == GL_READ_BUFFER || (b[1] >= GL_DRAW_BUFFER0 && b[1] <= GL_DRAW_BUFFER15)
					       || b[1] == GL_DRAW_BUFFER))
	    r[0] = GL_BACK;
	  reply (r, sizeof r);
	  return;
	}
      dispatch_gen (op, b, n);
      return;
    }
  /* GLES 3's hints on our framebuffer (the program's framebuffer 0): not
     passed on, as its attachments have other names */
  if (__builtin_expect (op == OP_glInvalidateFramebuffer || op == OP_glInvalidateSubFramebuffer, 0) && !app_fb)
    return;
  /* the generated ones that need a word from us first */
  if (__builtin_expect (op == OP_glDrawBuffer || op == OP_glReadBuffer, 0))
    {
      void (*f) (GLenum) = op == OP_glDrawBuffer ? gl.DrawBuffer : gl.ReadBuffer;
      if (f)
	f (own_buffer (a[0]));
      return;
    }
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
    case OP_PRESENT_SLOT:
      present_slot (a[0]);
      break;
    case OP_OVERLAY:
      overlay_place (a);
      break;
    case OP_PRESENT_OVERLAY:
      present_overlay (a[0]);
      break;
    case OP_SYNC:
      area->reply_len = 0;
      break;
    case OP_STATS:
      {
	uint64_t r[4] = { st_busy, st_present, st_sleeps, st_commands };
	reply (r, sizeof r);
	st_busy = st_present = st_sleeps = st_commands = 0;
	break;
      }
    case OP_LIMITS:
      {
	/* what the library keeps on its side: matrix stack depths, texture
	   units */
	static const GLenum q[] = { GL_MAX_MODELVIEW_STACK_DEPTH, GL_MAX_PROJECTION_STACK_DEPTH,
				    GL_MAX_TEXTURE_STACK_DEPTH, GL_MAX_TEXTURE_COORDS,
				    GL_MAX_TEXTURE_IMAGE_UNITS, GL_MAX_VERTEX_ATTRIBS };
	GLint r[6] = { 0, 0, 0, 0, 0, 0 };
	for (int i = 0; i < 6; i++)
	  if (desktop_gl || i >= 4)
	    gl.GetIntegerv (q[i], &r[i]);
	reply (r, sizeof r);
	break;
      }
    case OP_glBindFramebuffer:
      if (a[0] != GL_READ_FRAMEBUFFER)
	app_fb = a[1];
      gl.BindFramebuffer (a[0], a[1] ? a[1] : fbo);
      if (!a[1] && desktop_gl && gl.DrawBuffer)
	{
	  gl.DrawBuffer (GL_COLOR_ATTACHMENT0);
	  gl.ReadBuffer (GL_COLOR_ATTACHMENT0);
	}
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
	int mark = es_on_desktop && !memmem (s[1], (size_t) l[1], "#version", 8);
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
	if (gl.GetShaderPrecisionFormat)
	  gl.GetShaderPrecisionFormat (a[0], a[1], r, r + 2);
	reply (r, sizeof r);
	break;
      }
    case OP_glGetString:
      {
	const char *s = (const char *) gl.GetString (a[0]);
	char b[2048];
	if (!s && a[0] == GL_EXTENSIONS && gl.GetStringi)
	  {
	    /* (OpenGL 3.1 and on without the compatibility profile: one at
	       a time, joined) */
	    GLint ne = 0;
	    gl.GetIntegerv (GL_NUM_EXTENSIONS, &ne);
	    size_t at = 0;
	    char *j = scratch (65536);
	    for (GLint i = 0; i < ne; i++)
	      {
		const char *x = (const char *) gl.GetStringi (GL_EXTENSIONS, (GLuint) i);
		size_t l = x ? strlen (x) : 0;
		if (!l || at + l + 2 > 65536)
		  continue;
		if (at)
		  j[at++] = ' ';
		memcpy (j + at, x, l);
		at += l;
	      }
	    j[at] = 0;
	    gl.GetError ();
	    reply (j, at + 1);
	    break;
	  }
	if (a[0] == GL_RENDERER && s)
	  {
	    /* (a render node can be a software renderer's too: virtio's) */
	    if (strstr (s, "llvmpipe") || strstr (s, "softpipe"))
	      hardware = 0;
	    /* say how it is being drawn */
	    snprintf (b, sizeof b, "%s (RISC3D, Linux %s%s)", s, hardware ? "GPU" : "software",
		      es_on_desktop ? ", desktop OpenGL" : "");
	    s = b;
	  }
	reply (s ? s : "", s ? strlen (s) + 1 : 1);
	break;
      }
    case OP_glGetIntegerv:
    case OP_glGetFloatv:
    case OP_glGetDoublev:
    case OP_glGetBooleanv:
      {
	uint32_t r[64];
	memset (r, 0, sizeof r);
	if (op == OP_glGetIntegerv)
	  {
	    gl.GetIntegerv (a[0], (GLint *) r);
	    if ((a[0] == GL_FRAMEBUFFER_BINDING || a[0] == GL_READ_FRAMEBUFFER_BINDING) && r[0] == fbo)
	      r[0] = 0;
	    if ((a[0] == GL_DRAW_BUFFER || a[0] == GL_READ_BUFFER) && !app_fb && r[0] == GL_COLOR_ATTACHMENT0)
	      r[0] = GL_BACK;
	  }
	else if (op == OP_glGetFloatv)
	  gl.GetFloatv (a[0], (GLfloat *) r);
	else if (op == OP_glGetDoublev && gl.GetDoublev)
	  gl.GetDoublev (a[0], (GLdouble *) r);
	else if (op == OP_glGetBooleanv)
	  gl.GetBooleanv (a[0], (GLboolean *) r);
	reply (r, sizeof r);
	break;
      }
    case OP_glAreTexturesResident:
      break;
    case OP_glBufferData:
      gl.BufferData (a[0], (GLsizeiptr) a[1], a[3] ? a + 4 : 0, a[2]);
      break;
    case OP_glBufferSubData:
      gl.BufferSubData (a[0], (GLintptr) a[1], (GLsizeiptr) a[2], a + 3);
      break;
    case OP_glGetBufferSubData:
      {
	uint32_t bytes = a[2] > area->reply_bytes ? area->reply_bytes : a[2];
	if (gl.GetBufferSubData)
	  gl.GetBufferSubData (a[0], (GLintptr) a[1], (GLsizeiptr) bytes, reply_area);
	area->reply_len = bytes;
	break;
      }
    case OP_glTexImage1D:
      if (gl.TexImage1D)
	gl.TexImage1D (a[0], (GLint) a[1], (GLint) a[2], (GLsizei) a[3], (GLint) a[4], a[5], a[6], pixels_at (a + 7));
      break;
    case OP_glTexImage2D:
      gl.TexImage2D (a[0], (GLint) a[1], (GLint) a[2], (GLsizei) a[3], (GLsizei) a[4], (GLint) a[5], a[6], a[7], pixels_at (a + 8));
      break;
    case OP_glTexImage3D:
      if (gl.TexImage3D)
	gl.TexImage3D (a[0], (GLint) a[1], (GLint) a[2], (GLsizei) a[3], (GLsizei) a[4], (GLsizei) a[5], (GLint) a[6], a[7], a[8], pixels_at (a + 9));
      break;
    case OP_glTexSubImage1D:
      if (gl.TexSubImage1D)
	gl.TexSubImage1D (a[0], (GLint) a[1], (GLint) a[2], (GLsizei) a[3], a[4], a[5], pixels_at (a + 6));
      break;
    case OP_glTexSubImage2D:
      gl.TexSubImage2D (a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLsizei) a[4], (GLsizei) a[5], a[6], a[7], pixels_at (a + 8));
      break;
    case OP_glTexSubImage3D:
      if (gl.TexSubImage3D)
	gl.TexSubImage3D (a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLint) a[4], (GLsizei) a[5], (GLsizei) a[6], (GLsizei) a[7], a[8], a[9], pixels_at (a + 10));
      break;
    case OP_glCompressedTexImage1D:
      if (gl.CompressedTexImage1D)
	gl.CompressedTexImage1D (a[0], (GLint) a[1], a[2], (GLsizei) a[3], (GLint) a[4], (GLsizei) a[5], pixels_at (a + 6));
      break;
    case OP_glCompressedTexImage2D:
      gl.CompressedTexImage2D (a[0], (GLint) a[1], a[2], (GLsizei) a[3], (GLsizei) a[4], (GLint) a[5], (GLsizei) a[6], pixels_at (a + 7));
      break;
    case OP_glCompressedTexImage3D:
      if (gl.CompressedTexImage3D)
	gl.CompressedTexImage3D (a[0], (GLint) a[1], a[2], (GLsizei) a[3], (GLsizei) a[4], (GLsizei) a[5], (GLint) a[6], (GLsizei) a[7], pixels_at (a + 8));
      break;
    case OP_glCompressedTexSubImage1D:
      if (gl.CompressedTexSubImage1D)
	gl.CompressedTexSubImage1D (a[0], (GLint) a[1], (GLint) a[2], (GLsizei) a[3], a[4], (GLsizei) a[5], pixels_at (a + 6));
      break;
    case OP_glCompressedTexSubImage2D:
      gl.CompressedTexSubImage2D (a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLsizei) a[4], (GLsizei) a[5], a[6], (GLsizei) a[7], pixels_at (a + 8));
      break;
    case OP_glCompressedTexSubImage3D:
      if (gl.CompressedTexSubImage3D)
	gl.CompressedTexSubImage3D (a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLint) a[4], (GLsizei) a[5], (GLsizei) a[6], (GLsizei) a[7], a[8], (GLsizei) a[9], pixels_at (a + 10));
      break;
    case OP_glDrawPixels:
      if (gl.DrawPixels)
	gl.DrawPixels ((GLsizei) a[0], (GLsizei) a[1], a[2], a[3], pixels_at (a + 4));
      break;
    case OP_glBitmap:
      if (gl.Bitmap)
	gl.Bitmap ((GLsizei) a[0], (GLsizei) a[1], r3d_u2f (a[2]), r3d_u2f (a[3]), r3d_u2f (a[4]), r3d_u2f (a[5]),
		   (const GLubyte *) pixels_at (a + 6));
      break;
    case OP_glPolygonStipple:
      if (gl.PolygonStipple)
	gl.PolygonStipple ((const GLubyte *) pixels_at (a));
      break;
    case OP_glReadPixels:
      /* x y w h format type flag offset; with no pack buffer, the pixels
	 tight, laid out by the library */
      if (a[6])
	gl.ReadPixels ((GLint) a[0], (GLint) a[1], (GLsizei) a[2], (GLsizei) a[3], a[4], a[5], (void *) (uintptr_t) a[7]);
      else
	{
	  pack_tight ();
	  gl.ReadPixels ((GLint) a[0], (GLint) a[1], (GLsizei) a[2], (GLsizei) a[3], a[4], a[5], reply_area);
	  pack_restore ();
	  area->reply_len = area->reply_bytes;
	}
      break;
    case OP_glGetTexImage:
      /* target level format type flag offset: the size, then the pixels */
      if (!gl.GetTexImage)
	break;
      if (a[4])
	gl.GetTexImage (a[0], (GLint) a[1], a[2], a[3], (void *) (uintptr_t) a[5]);
      else
	{
	  GLint wh[3] = { 0, 1, 1 };
	  gl.GetTexLevelParameteriv (a[0], (GLint) a[1], GL_TEXTURE_WIDTH, &wh[0]);
	  gl.GetTexLevelParameteriv (a[0], (GLint) a[1], GL_TEXTURE_HEIGHT, &wh[1]);
	  if (a[0] == GL_TEXTURE_3D)
	    gl.GetTexLevelParameteriv (a[0], (GLint) a[1], GL_TEXTURE_DEPTH, &wh[2]);
	  memcpy (reply_area, wh, 12);
	  pack_tight ();
	  gl.GetTexImage (a[0], (GLint) a[1], a[2], a[3], reply_area + 12);
	  pack_restore ();
	  area->reply_len = area->reply_bytes;
	}
      break;
    case OP_glGetCompressedTexImage:
      if (!gl.GetCompressedTexImage)
	break;
      if (a[2])
	gl.GetCompressedTexImage (a[0], (GLint) a[1], (void *) (uintptr_t) a[3]);
      else
	{
	  GLint size = 0;
	  gl.GetTexLevelParameteriv (a[0], (GLint) a[1], GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
	  if (size < 0 || (uint32_t) size + 4 > area->reply_bytes)
	    size = 0;
	  memcpy (reply_area, &size, 4);
	  gl.GetCompressedTexImage (a[0], (GLint) a[1], reply_area + 4);
	  area->reply_len = (uint32_t) size + 4;
	}
      break;
    case OP_glGetPolygonStipple:
      if (a[0])
	gl.GetPolygonStipple ((GLubyte *) (uintptr_t) a[1]);
      else
	{
	  GLubyte m[128];
	  memset (m, 0, sizeof m);
	  pack_tight ();
	  if (gl.GetPolygonStipple)
	    gl.GetPolygonStipple (m);
	  pack_restore ();
	  reply (m, sizeof m);
	}
      break;
    case OP_glMap1d:
      if (gl.Map1d)
	gl.Map1d (a[0], r3d_w2d (a + 1), r3d_w2d (a + 3), (GLint) a[5], (GLint) a[6], (const GLdouble *) (a + 8));
      break;
    case OP_glMap1f:
      if (gl.Map1f)
	gl.Map1f (a[0], r3d_u2f (a[1]), r3d_u2f (a[2]), (GLint) a[3], (GLint) a[4], (const GLfloat *) (a + 6));
      break;
    case OP_glMap2d:
      if (gl.Map2d)
	gl.Map2d (a[0], r3d_w2d (a + 1), r3d_w2d (a + 3), (GLint) a[5], (GLint) a[6],
		  r3d_w2d (a + 7), r3d_w2d (a + 9), (GLint) a[11], (GLint) a[12], (const GLdouble *) (a + 14));
      break;
    case OP_glMap2f:
      if (gl.Map2f)
	gl.Map2f (a[0], r3d_u2f (a[1]), r3d_u2f (a[2]), (GLint) a[3], (GLint) a[4],
		  r3d_u2f (a[5]), r3d_u2f (a[6]), (GLint) a[7], (GLint) a[8], (const GLfloat *) (a + 10));
      break;
    case OP_glCallLists:
      if (gl.CallLists)
	gl.CallLists ((GLsizei) a[0], a[1], a + 3);
      break;
    case OP_glFeedbackBuffer:
      free (feedback_buf);
      feedback_size = (GLsizei) a[0];
      feedback_buf = calloc ((size_t) feedback_size + 1, 4);
      if (gl.FeedbackBuffer && feedback_buf)
	gl.FeedbackBuffer (feedback_size, a[1], feedback_buf);
      break;
    case OP_glSelectBuffer:
      free (select_buf);
      select_size = (GLsizei) a[0];
      select_buf = calloc ((size_t) select_size + 1, 4);
      if (gl.SelectBuffer && select_buf)
	gl.SelectBuffer (select_size, (GLuint *) select_buf);
      break;
    case OP_glRenderMode:
      {
	/* the result, the mode left, then that mode's buffer */
	GLint prev = GL_RENDER;
	gl.GetIntegerv (GL_RENDER_MODE, &prev);
	uint32_t *r = (uint32_t *) reply_area;
	r[0] = gl.RenderMode ? (uint32_t) gl.RenderMode (a[0]) : 0;
	r[1] = (uint32_t) prev;
	r[2] = 0;
	const GLfloat *b = prev == GL_FEEDBACK ? feedback_buf : prev == GL_SELECT ? select_buf : 0;
	GLsizei size = prev == GL_FEEDBACK ? feedback_size : select_size;
	if (b && (size_t) size * 4 + 12 <= area->reply_bytes)
	  {
	    r[2] = (uint32_t) size;
	    memcpy (r + 3, b, (size_t) size * 4);
	  }
	area->reply_len = 12 + r[2] * 4;
	break;
      }
    case OP_glVertexAttribPointer:
      gl.VertexAttribPointer (a[0], (GLint) a[1], a[2], (GLboolean) a[3], (GLsizei) a[4], (const void *) (uintptr_t) a[5]);
      break;
    case OP_CLIENT_ARRAY:
      {
	/* an attribute array from the program's memory: into a buffer of ours */
	GLint was = 0;
	if (a[0] >= 16)
	  break;
	gl.GetIntegerv (GL_ARRAY_BUFFER_BINDING, &was);
	gl.BindBuffer (GL_ARRAY_BUFFER, PRIV_ARRAY (a[0]));
	gl.BufferData (GL_ARRAY_BUFFER, (GLsizeiptr) a[5], a + 6, GL_STREAM_DRAW);
	if ((a[3] & 2) && gl.VertexAttribIPointer)
	  gl.VertexAttribIPointer (a[0], (GLint) a[1], a[2], (GLsizei) a[4], 0);
	else
	  gl.VertexAttribPointer (a[0], (GLint) a[1], a[2], (GLboolean) (a[3] & 1), (GLsizei) a[4], 0);
	gl.BindBuffer (GL_ARRAY_BUFFER, (GLuint) was);
	break;
      }
    case OP_FIXED_ARRAY:
      fixed_array (a);
      break;
    case OP_IMMEDIATE:
      immediate (a);
      break;
    /* fixed-function arrays in buffer objects: size type stride offset */
    case OP_glVertexPointer:
      if (gl.VertexPointer)
	gl.VertexPointer ((GLint) a[0], a[1], (GLsizei) a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glColorPointer:
      if (gl.ColorPointer)
	gl.ColorPointer ((GLint) a[0], a[1], (GLsizei) a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glSecondaryColorPointer:
      if (gl.SecondaryColorPointer)
	gl.SecondaryColorPointer ((GLint) a[0], a[1], (GLsizei) a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glTexCoordPointer:
      if (gl.TexCoordPointer)
	gl.TexCoordPointer ((GLint) a[0], a[1], (GLsizei) a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glNormalPointer:
      if (gl.NormalPointer)
	gl.NormalPointer (a[1], (GLsizei) a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glIndexPointer:
      if (gl.IndexPointer)
	gl.IndexPointer (a[1], (GLsizei) a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glFogCoordPointer:
      if (gl.FogCoordPointer)
	gl.FogCoordPointer (a[1], (GLsizei) a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glEdgeFlagPointer:
      if (gl.EdgeFlagPointer)
	gl.EdgeFlagPointer ((GLsizei) a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glEnableClientState:
      if (gl.EnableClientState)
	gl.EnableClientState (a[0]);
      break;
    case OP_glDisableClientState:
      if (gl.DisableClientState)
	gl.DisableClientState (a[0]);
      break;
    case OP_glClientActiveTexture:
      if (gl.ClientActiveTexture)
	gl.ClientActiveTexture (a[0]);
      break;
    case OP_glPushClientAttrib:
      if (gl.PushClientAttrib)
	gl.PushClientAttrib (a[0]);
      break;
    case OP_glPopClientAttrib:
      if (gl.PopClientAttrib)
	gl.PopClientAttrib ();
      break;
    case OP_glArrayElement:
      if (gl.ArrayElement)
	gl.ArrayElement ((GLint) a[0]);
      break;
    case OP_glDrawArrays:
      gl.DrawArrays (a[0], (GLint) a[1], (GLsizei) a[2]);
      break;
    case OP_glDrawElements:
      gl.DrawElements (a[0], (GLsizei) a[1], a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glDrawRangeElements:
      if (gl.DrawRangeElements)
	gl.DrawRangeElements (a[0], a[1], a[2], (GLsizei) a[3], a[4], (const void *) (uintptr_t) a[5]);
      break;
    case OP_glPointSizePointerOES:
      if (gl.PointSizePointerOES)
	gl.PointSizePointerOES (a[1], (GLsizei) a[2], (const void *) (uintptr_t) a[3]);
      break;
    case OP_glQueryMatrixxOES:
      {
	uint32_t r[33];
	memset (r, 0, sizeof r);
	r[0] = gl.QueryMatrixxOES ? gl.QueryMatrixxOES ((GLfixed *) (r + 1), (GLint *) (r + 17)) : 0xFFFF;
	reply (r, sizeof r);
	break;
      }
    case OP_READ_BUFFER_RANGE:
      {
	/* glMapBufferRange with GL_MAP_READ_BIT: the range's bytes */
	uint32_t bytes = a[2] > area->reply_bytes ? area->reply_bytes : a[2];
	if (gl.GetBufferSubData && desktop_gl)
	  gl.GetBufferSubData (a[0], (GLintptr) a[1], (GLsizeiptr) bytes, reply_area);
	else if (gl.MapBufferRange && gl.UnmapBuffer)
	  {
	    const void *p = gl.MapBufferRange (a[0], (GLintptr) a[1], (GLsizeiptr) bytes, 0x0001);
	    if (p)
	      memcpy (reply_area, p, bytes);
	    gl.UnmapBuffer (a[0]);
	  }
	area->reply_len = bytes;
	break;
      }
    case OP_glDrawArraysInstanced:
      if (gl.DrawArraysInstanced)
	gl.DrawArraysInstanced (a[0], (GLint) a[1], (GLsizei) a[2], (GLsizei) a[3]);
      break;
    case OP_DRAW_ELEMENTS_EX:
      draw_elements_ex (a);
      break;
    case OP_glCreateShaderProgramv:
      {
	const GLchar *src[2] = { "#version 100\n", 0 };
	char *t = scratch (a[1] + 1);
	memcpy (t, a + 2, a[1]);
	t[a[1]] = 0;
	src[1] = t;
	int mark = es_on_desktop && !strstr (t, "#version");
	uint32_t r = gl.CreateShaderProgramv ? gl.CreateShaderProgramv (a[0], mark ? 2 : 1, src + !mark) : 0;
	reply (&r, 4);
	break;
      }
    case OP_glGetProgramPipelineInfoLog:
      text_reply (gl.GetProgramPipelineInfoLog, a[0], a[1]);
      break;
    case OP_glGetProgramBinary:
      {
	uint32_t max = a[1] > area->reply_bytes - 8 ? area->reply_bytes - 8 : a[1];
	uint32_t *r = (uint32_t *) reply_area;
	GLsizei l = 0;
	GLenum fmt = 0;
	if (gl.GetProgramBinary)
	  gl.GetProgramBinary (a[0], (GLsizei) max, &l, &fmt, r + 2);
	r[0] = (uint32_t) l;
	r[1] = fmt;
	area->reply_len = 8 + (uint32_t) l;
	break;
      }
    case OP_glDebugMessageInsert:
      if (gl.DebugMessageInsert)
	gl.DebugMessageInsert (a[0], a[1], a[2], a[3], (GLsizei) a[4], (const GLchar *) (a + 5));
      break;
    case OP_glPushDebugGroup:
      if (gl.PushDebugGroup)
	gl.PushDebugGroup (a[0], a[1], (GLsizei) a[2], (const GLchar *) (a + 3));
      break;
    case OP_glObjectLabel:
      if (a[0] == R3D_SYNC_LABEL)
	{
	  if (sync_of (a[1]) && gl.ObjectPtrLabel)
	    gl.ObjectPtrLabel (syncs[a[1]], a[2] == 0xFFFFFFFF ? 0 : (GLsizei) a[2], a[2] == 0xFFFFFFFF ? 0 : (const GLchar *) (a + 3));
	  break;
	}
      if (gl.ObjectLabel)
	gl.ObjectLabel (a[0], a[1], a[2] == 0xFFFFFFFF ? 0 : (GLsizei) a[2], a[2] == 0xFFFFFFFF ? 0 : (const GLchar *) (a + 3));
      break;
    case OP_glGetObjectLabel:
      {
	uint32_t max = a[2] > 65536 ? 65536 : a[2];
	uint32_t *r = scratch (max + 8);
	GLsizei l = 0;
	if (a[0] == R3D_SYNC_LABEL)
	  {
	    if (sync_of (a[1]) && gl.GetObjectPtrLabel)
	      gl.GetObjectPtrLabel (syncs[a[1]], (GLsizei) max, &l, (GLchar *) (r + 1));
	  }
	else if (gl.GetObjectLabel)
	  gl.GetObjectLabel (a[0], a[1], (GLsizei) max, &l, (GLchar *) (r + 1));
	r[0] = (uint32_t) l;
	reply (r, (size_t) l + 4);
	break;
      }
    case OP_glGetDebugMessageLog:
      debug_log (a[0], a[1]);
      break;
    case OP_glDebugMessageCallback:
      debug_callback (a[0]);
      break;
    case OP_TEMP_BIND:
      temp_bind (a);
      break;
    case OP_TEMP_END:
      temp_end ();
      break;
    case OP_glGetNamedProgramStringEXT:
      {
	uint32_t bytes = a[3] > area->reply_bytes ? area->reply_bytes : a[3];
	void *r = scratch (bytes);
	if (gl.GetNamedProgramStringEXT)
	  gl.GetNamedProgramStringEXT (a[0], a[1], a[2], r);
	reply (r, bytes);
	break;
      }
    /* OpenGL 3 / GLES 3 */
    case OP_glGenBuffers:
    case OP_glGenTextures:
      {
	/* (OpenGL 3.1 without the compatibility profile: names of its own) */
	uint32_t cnt = a[0] > 65536 ? 65536 : a[0];
	GLuint *r = scratch (cnt * 4);
	if (op == OP_glGenBuffers && gl.GenBuffers)
	  gl.GenBuffers ((GLsizei) cnt, r);
	else if (op == OP_glGenTextures && gl.GenTextures)
	  gl.GenTextures ((GLsizei) cnt, r);
	reply (r, cnt * 4);
	break;
      }
    case OP_glFenceSync: case OP_glDeleteSync: case OP_glWaitSync:
    case OP_glIsSync: case OP_glClientWaitSync: case OP_glGetSynciv:
      sync_ops (op, a);
      break;
    case OP_glGetInteger64v:
      {
	GLint64 r[64];
	memset (r, 0, sizeof r);
	if (gl.GetInteger64v)
	  gl.GetInteger64v (a[0], r);
	reply (r, sizeof r);
	break;
      }
    case OP_glVertexAttribIPointer:
      if (gl.VertexAttribIPointer)
	gl.VertexAttribIPointer (a[0], (GLint) a[1], a[2], (GLsizei) a[3], (const void *) (uintptr_t) a[4]);
      break;
    case OP_glDrawArraysIndirect:
      if (gl.DrawArraysIndirect)
	gl.DrawArraysIndirect (a[0], (const void *) (uintptr_t) a[1]);
      break;
    case OP_glDrawElementsIndirect:
      if (gl.DrawElementsIndirect)
	gl.DrawElementsIndirect (a[0], a[1], (const void *) (uintptr_t) a[2]);
      break;
    case OP_glGetProgramStringARB:
      {
	uint32_t bytes = a[2] > area->reply_bytes ? area->reply_bytes : a[2];
	void *r = scratch (bytes);
	if (gl.GetProgramStringARB)
	  gl.GetProgramStringARB (a[0], a[1], r);
	reply (r, bytes);
	break;
      }
    case OP_DRAW_ELEMENTS_INLINE:
      gl.BindBuffer (GL_ELEMENT_ARRAY_BUFFER, PRIV_ELEMENTS);
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

static void drain_ring (void);

static void
drain (void)
{
  if (area->tail == __atomic_load_n (&area->head, __ATOMIC_SEQ_CST))
    return;
  uint64_t t = now_ns ();
  drain_ring ();
  st_busy += now_ns () - t;
}

/* Commands up to the head as last read; the head is read again only when
   they are done, and the tail (how far the program may write) is moved
   every 16K words and at the end, not after each command: each is a
   memory barrier on ARM, and immediate mode is a command a vertex. */
static void
drain_ring (void)
{
  uint32_t words = area->ring_words, mask = words - 1;
  uint32_t tail = area->tail, shown = tail;
  uint32_t head = __atomic_load_n (&area->head, __ATOMIC_ACQUIRE);
  for (;;)
    {
      if (tail == head)
	{
	  head = __atomic_load_n (&area->head, __ATOMIC_ACQUIRE);
	  if (tail == head)
	    break;
	}
      uint32_t at = tail & mask;
      uint32_t op = ring[at];
      if (op == OP_WRAP)
	{
	  tail += words - at;
	  continue;
	}
      uint32_t n = ring[at + 1];
      if (n < 2 || at + n > words)
	{
	  logmsg ("rol-3d: a bad command (&%X, %u words): the context ends\n", op, n);
	  _exit (1);
	}
      const uint32_t *a = ring + at + 2;
      if (__builtin_expect (op < OP_BLOB, 1))
	dispatch (op, a, n - 2);
      else if (op == OP_BLOB)
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
      st_commands++;
      tail += n;
      if (tail - shown >= 16384)
	{
	  __atomic_store_n (&area->tail, tail, __ATOMIC_RELEASE);
	  shown = tail;
	}
    }
  __atomic_store_n (&area->tail, tail, __ATOMIC_SEQ_CST);
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
  if (!gl_start () && !software_display ())
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

  for (;;)
    {
      drain ();
      /* a little longer awake (0.2 ms): the next batch is often on its way,
	 and waking a sleeping renderer costs the program a call into Linux */
      uint64_t until = now_ns () + 200000;
      for (int i = 1; area->tail == __atomic_load_n (&area->head, __ATOMIC_ACQUIRE) && !area->asking; i++)
	{
	  R3D_PAUSE ();
	  if (!(i & 63) && now_ns () > until)
	    break;
	}
      if (area->tail != area->head)
	continue;
      __atomic_store_n (&area->sleeping, 1, __ATOMIC_SEQ_CST);
      if (area->tail != __atomic_load_n (&area->head, __ATOMIC_SEQ_CST))
	{
	  __atomic_store_n (&area->sleeping, 0, __ATOMIC_SEQ_CST);
	  continue;
	}
      char b[64];
      st_sleeps++;
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

/* the display's card: the one that makes dumb buffers (needs root), into
   name (32 bytes), else "" */
/* a card, never kept as its master: a file that opens a card with no
   master becomes it, and rol-fb (started after this helper) then couldn't
   set a mode. -1 if it won't open. */
static int
open_card (const char *name)
{
  int fd = open (name, O_RDWR | O_CLOEXEC);
  if (fd >= 0)
    ioctl (fd, DRM_IOCTL_DROP_MASTER, 0);	/* (an error if it wasn't: fine) */
  return fd;
}

static void
find_card (char *card)
{
  card[0] = 0;
  for (int i = 0; i < 4 && !card[0]; i++)
    {
      char name[32];
      snprintf (name, sizeof name, "/dev/dri/card%d", i);
      int fd = open_card (name);
      struct drm_mode_create_dumb cd = { .width = 16, .height = 16, .bpp = 32 };
      if (fd >= 0 && ioctl (fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd) == 0)
	{
	  struct drm_mode_destroy_dumb dd = { .handle = cd.handle };
	  ioctl (fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
	  strcpy (card, name);
	}
      if (fd >= 0)
	close (fd);
    }
}

/* HOME was root's, so Mesa couldn't make its shader cache and compiled
   every shader again for each program */
static void
home (void)
{
  struct passwd *pw = getpwuid (getuid ());
  if (getuid () == RO_UID)
    setenv ("HOME", pw && pw->pw_dir && access (pw->pw_dir, W_OK) == 0 ? pw->pw_dir : "/tmp", 1);
  if (!getenv ("MESA_SHADER_CACHE_DIR") && access (getenv ("HOME") ? getenv ("HOME") : "/", W_OK) != 0)
    {
      mkdir ("/tmp/rol-3d-cache", 0700);
      setenv ("MESA_SHADER_CACHE_DIR", "/tmp/rol-3d-cache", 1);
    }
}

int
main (int argc, char **argv)
{
  if (argc > 1 && !strcmp (argv[1], "--check"))
    {
      /* what would draw, or why nothing can: GLES first, then OpenGL */
      if ((!gl_start () && !software_display ()) || !make_context (0))
	{
	  printf ("rol-3d: %s\n", gl_error);
	  return 1;
	}
      printf ("rol-3d %s: %s; %s; %s", ROL3D_VERSION, hardware ? "GPU" : "software", (const char *) gl.GetString (GL_RENDERER),
	      (const char *) gl.GetString (GL_VERSION));
      if (!es_on_desktop)
	{
	  have_context = 0;
	  if (make_context (1))
	    printf ("; %s", (const char *) gl.GetString (GL_VERSION));
	  else
	    printf ("; no desktop OpenGL (%s)", gl_error);
	}
      char card[32];
      find_card (card);
      printf ("\nrol-3d: zero-copy frames: %s\n", card[0] ? card : "no display card makes buffers (read back only)");
      return 0;
    }
  /* Started by init, output goes nowhere (/dev/null): then it goes to
     /riscos/3d/log.txt (the one before kept as log.prev.txt), as rol-snd's
     and rol-netd's do. Started from a shell, it stays where it was sent. */
  struct stat es, ns;
  if (fstat (2, &es) == 0 && stat ("/dev/null", &ns) == 0 && S_ISCHR (es.st_mode) && es.st_rdev == ns.st_rdev)
    {
      mkdir ("/riscos/3d", 0755);
      rename ("/riscos/3d/log.txt", "/riscos/3d/log.prev.txt");
      int f = open ("/riscos/3d/log.txt", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
      if (f >= 0)
	{
	  dup2 (f, 1);
	  dup2 (f, 2);
	  close (f);
	}
    }
  logmsg ("rol-3d %s: started\n", ROL3D_VERSION);
  int l = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  struct sockaddr_un a = { .sun_family = AF_UNIX };
  static const char nm[] = "rol-3d";
  memcpy (a.sun_path + 1, nm, sizeof nm - 1);	/* abstract */
  if (l < 0 || bind (l, (struct sockaddr *) &a, offsetof (struct sockaddr_un, sun_path) + sizeof nm)
      || listen (l, 8))
    return 1;
  /* The display's card (for zero-copy frames: dumb buffers the display
     can show): the one that makes dumb buffers. Opened again as root for
     each connection, so that each context's buffers are its own and go
     when it does. */
  /* The display's card is looked for at the first connection, not here:
     init starts this helper before rol-fb, and RISC OS (so any program)
     after both. */
  char card[32] = "";
  int card_looked = 0;
  /* The images have no udev, so the render nodes are root's until now:
     they go to Linux's render group (or video), which only this helper
     joins. Render nodes can draw; they can't set modes or take the
     display. */
  gid_t g[2] = { RO_GID, RO_GID };
  if (getuid () == 0)
    {
      struct group *gr = getgrnam ("render");
      if (!gr)
	gr = getgrnam ("video");
      g[1] = gr ? gr->gr_gid : RO_GID;
      for (int n = 128; n < 132; n++)
	{
	  char node[40];
	  snprintf (node, sizeof node, "/dev/dri/renderD%d", n);
	  if (chown (node, 0, g[1]) == 0)
	    chmod (node, 0660);
	}
    }
  signal (SIGCHLD, SIG_IGN);
  signal (SIGPIPE, SIG_IGN);
  for (;;)
    {
      int c = accept4 (l, 0, 0, SOCK_CLOEXEC);
      if (c < 0)
	continue;
      if (!card_looked && getuid () == 0)
	{
	  find_card (card);
	  card_looked = 1;
	  logmsg ("rol-3d: zero-copy frames: %s\n", card[0] ? card : "no display card makes buffers (read back only)");
	}
      int cf = card[0] ? open_card (card) : -1;
      if (fork () == 0)
	{
	  close (l);
	  card_fd = cf;
	  /* each context runs as RISC OS's own Linux user, with the group
	     that reaches the GPU */
	  if (getuid () == 0 && (setgroups (2, g) || setgid (RO_GID) || setuid (RO_UID)))
	    _exit (1);
	  home ();
	  serve (c);
	  _exit (0);
	}
      if (cf >= 0)
	close (cf);
      close (c);
    }
}
