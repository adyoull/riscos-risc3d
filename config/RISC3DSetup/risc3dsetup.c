/* RISC3DSetup: Configure's plug-in for RISC3D. Shows which 3D driver
   RISC3D loaded, whether it draws on a GPU, and what it offers: the
   renderer, the OpenGL and OpenGL ES versions, the largest texture, the
   extensions, shared contexts, the contexts open, and what RISC3D found
   in its drivers folder.

   For a command-stream driver (Linux3D) the versions come from asking:
   it opens an OpenGL, an OpenGL ES and an OpenGL ES 1.1 context for a
   moment, as a program would. A DRM device (V3DRender) is drawn with
   Mesa's own driver inside each program, so there it says that.

   Started by Configure with "-openat x y" (the window's top left, OS
   units), else centred. Message_OpenConfigWindow brings it to the front.
   Plain Wimp calls, no Toolbox. Copyright (c) 2026 Andrew Youll. MIT
   licence (see LICENSE). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <swis.h>
#include "r3d_gl.h"
#define R3D_DIRECT
#include "r3d.h"

#define VERSION "1.00 (20 Nov 2026)"
#define RISC3D_Info 0x5AF80
#define Message_Quit 0
#define Message_HelpRequest 0x502
#define Message_HelpReply 0x503
#define Message_OpenConfigWindow 0x50D83

/* --- what is shown --------------------------------------------------- */

enum { R_DRIVER, R_DRAWS, R_RENDERER, R_GL, R_GLES, R_TEXTURE, R_EXTS, R_SHARE, R_CONTEXTS, R_RISC3D, R_FOLDER, ROWS };
static const char *const label[ROWS] = {
  "Driver", "Draws", "Renderer", "OpenGL", "OpenGL ES", "Largest texture", "Extensions", "Shared contexts",
  "Contexts open", "RISC3D", "Drivers folder"
};
static const char *const help[ROWS] = {
  "The 3D driver RISC3D loaded from its drivers folder: the first one there that starts on this machine.",
  "Whether the driver draws on the machine's GPU, or in software. With no driver, Mesa draws in software in each program, as before.",
  "The GPU and the driver that draw for programs.",
  "The newest OpenGL a program can have (compatibility profile: OpenGL 1.x calls still work), and its shading language.",
  "The newest OpenGL ES a program can have, and whether OpenGL ES 1.1 is there too.",
  "The largest texture the GPU takes.",
  "How many OpenGL and OpenGL ES extensions the driver offers.",
  "Whether a program can have contexts that share textures and buffers, one per thread (as web browsers draw).",
  "How many 3D contexts programs have open now.",
  "The version of the RISC3D module.",
  "Where RISC3D looks for drivers (!Boot.Resources.!RISC3D.GPU), and what each one there did. A driver refuses to start on machines it isn't for."
};
#define MAXDRV 6
static char value[ROWS][200];
static char drv_line[MAXDRV][160];
static int ndrv;

/* --- the window --------------------------------------------------------- */

#define LABEL_W 300
#define VALUE_W 1180
#define ROW_H 56
#define TOP 24
#define MARGIN 24
#define WIN_W (MARGIN + LABEL_W + 16 + VALUE_W + MARGIN)

typedef struct { int x0, y0, x1, y1; unsigned flags; const char *text; const char *valid; int len; } icon;

#define ICONS (2 * ROWS + MAXDRV + 2)
static icon icons[ICONS];
static int nicons, i_refresh, i_close;
static int win = -1, win_h;
static char title[] = "RISC3D";

static void
add_icon (int x0, int y0, int x1, int y1, unsigned flags, const char *text, const char *valid)
{
  icon *i = &icons[nicons++];
  i->x0 = x0, i->y0 = y0, i->x1 = x1, i->y1 = y1;
  i->flags = flags;
  i->text = text;
  i->valid = valid;
  i->len = (int) strlen (text) + 1;
}

/* text, vertically centred, indirected; colours fg 7 bg 1 */
#define F_TEXT 0x1u
#define F_BORDER 0x4u
#define F_HCENTRE 0x8u
#define F_VCENTRE 0x10u
#define F_FILLED 0x20u
#define F_RIGHT 0x200u
#define F_INDIR 0x100u
#define F_CLICK (3u << 12)
#define F_COLOURS ((7u << 24) | (1u << 28))

static void
build (void)
{
  int y = -TOP;
  nicons = 0;
  for (int r = 0; r < ROWS; r++)
    {
      add_icon (MARGIN, y - 48, MARGIN + LABEL_W, y, F_TEXT | F_VCENTRE | F_RIGHT | F_INDIR | F_COLOURS, label[r], "");
      add_icon (MARGIN + LABEL_W + 16, y - 48, MARGIN + LABEL_W + 16 + VALUE_W, y,
		F_TEXT | F_BORDER | F_VCENTRE | F_FILLED | F_INDIR | F_COLOURS, value[r], "R2");
      y -= ROW_H;
    }
  for (int d = 0; d < ndrv; d++)
    {
      add_icon (MARGIN + LABEL_W + 16, y - 44, MARGIN + LABEL_W + 16 + VALUE_W, y,
		F_TEXT | F_VCENTRE | F_INDIR | F_COLOURS, drv_line[d], "");
      y -= 48;
    }
  y -= 24;
  i_refresh = nicons;
  add_icon (WIN_W - MARGIN - 2 * 200 - 24, y - 68, WIN_W - MARGIN - 200 - 24, y,
	    F_TEXT | F_BORDER | F_HCENTRE | F_VCENTRE | F_FILLED | F_INDIR | F_CLICK | F_COLOURS, "Refresh", "R5,3");
  i_close = nicons;
  add_icon (WIN_W - MARGIN - 200, y - 68, WIN_W - MARGIN, y,
	    F_TEXT | F_BORDER | F_HCENTRE | F_VCENTRE | F_FILLED | F_INDIR | F_CLICK | F_COLOURS, "Close", "R6,3");
  win_h = -(y - 68 - MARGIN);
}

static int
create_window (void)
{
  static int blk[23 + ICONS * 8];
  memset (blk, 0, sizeof blk);
  blk[0] = 0, blk[1] = -win_h, blk[2] = WIN_W, blk[3] = 0;
  blk[6] = -1;
  blk[7] = (int) (0x80000002u | 0x10u | (1u << 25) | (1u << 26));	/* moveable, auto-redraw, close, title */
  unsigned char *col = (unsigned char *) &blk[8];
  col[0] = 7, col[1] = 2, col[2] = 7, col[3] = 1, col[4] = 3, col[5] = 1, col[6] = 12, col[7] = 0;
  blk[10] = 0, blk[11] = -win_h, blk[12] = WIN_W, blk[13] = 0;
  blk[14] = 0x119;				/* title: text, centred, indirected */
  blk[15] = 0;					/* work area: never */
  blk[16] = 1;					/* sprites: the Wimp's */
  blk[17] = 0;
  blk[18] = (int) title, blk[19] = -1, blk[20] = (int) sizeof title;
  blk[21] = nicons;
  for (int i = 0; i < nicons; i++)
    {
      int *b = &blk[22 + i * 8];
      b[0] = icons[i].x0, b[1] = icons[i].y0, b[2] = icons[i].x1, b[3] = icons[i].y1;
      b[4] = (int) icons[i].flags;
      b[5] = (int) icons[i].text, b[6] = (int) icons[i].valid, b[7] = icons[i].len;
    }
  int h = -1;
  if (_swix (Wimp_CreateWindow, _IN (1) | _OUT (0), blk, &h))
    return -1;
  return h;
}

static void
open_at (int x, int y)
{
  int b[8] = { win, x, y - win_h, x + WIN_W, y, 0, 0, -1 };
  _swix (Wimp_OpenWindow, _IN (1), b);
}

static void
open_front (void)
{
  int b[16] = { win };
  _swix (Wimp_GetWindowState, _IN (1), b);
  b[7] = -1;
  _swix (Wimp_OpenWindow, _IN (1), b);
}

/* --- finding out ------------------------------------------------------ */

static void
trim_to (char *dst, size_t n, const char *s)
{
  snprintf (dst, n, "%s", s ? s : "");
  size_t l = strlen (dst);
  while (l && (dst[l - 1] == ' ' || dst[l - 1] == '\r' || dst[l - 1] == '\n'))
    dst[--l] = 0;
}

static int
count_words (const char *s)
{
  int n = 0;
  for (; s && *s; s++)
    if (*s != ' ' && (s[1] == ' ' || !s[1]))
      n++;
  return n;
}

/* one context of an API for a moment: its version string, shading
   language, largest texture and extension count. 0 if none */
static int
ask_gl (int api, char *ver, size_t vn, char *sl, size_t sn, int *tex, int *exts, char *renderer, size_t rn)
{
  struct r3d_context *c = api == R3D_API_GL ? r3d_create_gl () : api == R3D_API_GLES1 ? r3d_create_es1 () : r3d_create ();
  if (!c)
    {
      trim_to (ver, vn, r3d_error ());
      return 0;
    }
  r3d_make_current (c);
  trim_to (ver, vn, (const char *) glGetString (GL_VERSION));
  if (sl)
    trim_to (sl, sn, (const char *) glGetString (GL_SHADING_LANGUAGE_VERSION));
  if (renderer)
    trim_to (renderer, rn, (const char *) glGetString (GL_RENDERER));
  if (tex)
    glGetIntegerv (GL_MAX_TEXTURE_SIZE, tex);
  if (exts)
    {
      const char *e = (const char *) glGetString (GL_EXTENSIONS);
      *exts = count_words (e);
    }
  r3d_make_current (0);
  r3d_destroy (c);
  return 1;
}

/* "OpenGL ES 3.1 RISC3D (Mesa x)" -> "3.1"; "3.1 RISC3D (Mesa x)" -> "3.1" */
static const char *
version_number (const char *v)
{
  static char out[32];
  const char *p = strstr (v, "OpenGL ES ");
  p = p ? p + 10 : v;
  size_t i = 0;
  while (i < sizeof out - 1 && ((p[i] >= '0' && p[i] <= '9') || p[i] == '.'))
    {
      out[i] = p[i];
      i++;
    }
  out[i] = 0;
  return i ? out : v;
}

/* *RISC3D's own account, for the drivers folder and what was tried */
static void
read_star_risc3d (void)
{
  ndrv = 0;
  if (_swix (OS_CLI, _IN (0), "RISC3D { > <Wimp$ScrapDir>.RISC3DSet }"))
    return;
  int type = 0, len = 0;
  if (_swix (OS_File, _INR (0, 1) | _OUT (0) | _OUT (4), 17, "<Wimp$ScrapDir>.RISC3DSet", &type, &len) || type != 1)
    return;
  char *buf = malloc ((size_t) len + 1);
  if (!buf)
    return;
  if (_swix (OS_File, _INR (0, 3), 16, "<Wimp$ScrapDir>.RISC3DSet", buf, 0))
    len = 0;
  buf[len] = 0;
  _swix (OS_File, _INR (0, 1), 6, "<Wimp$ScrapDir>.RISC3DSet");
  for (char *line = strtok (buf, "\r\n"); line; line = strtok (0, "\r\n"))	/* (OS_NewLine: LF CR) */
    {
      char t[200];
      trim_to (t, sizeof t, line);
      if (!strncmp (t, "Drivers folder: ", 16))
	trim_to (value[R_FOLDER], sizeof value[R_FOLDER], t + 16);
      else if (t[0] == ' ' && t[1] == ' ' && ndrv < MAXDRV)
	snprintf (drv_line[ndrv++], sizeof drv_line[0], "%s", t + 2);
    }
  free (buf);
}

static void
hourglass (int on)
{
  _swix (on ? Hourglass_On : Hourglass_Off, 0);
}

static void
find_out (void)
{
  for (int r = 0; r < ROWS; r++)
    value[r][0] = 0;
  for (int d = 0; d < MAXDRV; d++)
    drv_line[d][0] = 0;
  int version = 0, ndrivers = 0, nctx = 0;
  const char *name = 0, *drm = 0;
  unsigned flags = 0, ioctl = 0;
  if (_swix (RISC3D_Info, _OUTR (0, 6), &version, &ndrivers, &nctx, &name, &flags, &ioctl, &drm))
    {
      snprintf (value[R_DRIVER], sizeof value[0], "None: RISC3D isn't loaded");
      snprintf (value[R_DRAWS], sizeof value[0], "In software (Mesa, in each program)");
      snprintf (value[R_RISC3D], sizeof value[0], "Not loaded (!Boot.Resources.!RISC3D loads it at start-up)");
      return;
    }
  snprintf (value[R_RISC3D], sizeof value[0], "%d.%02d", version / 100, version % 100);	/* (version x 100) */
  snprintf (value[R_CONTEXTS], sizeof value[0], "%d", nctx);
  read_star_risc3d ();
  if (!name)
    {
      snprintf (value[R_DRIVER], sizeof value[0], "None: no driver in the folder starts on this machine");
      snprintf (value[R_DRAWS], sizeof value[0], "In software (Mesa, in each program)");
      snprintf (value[R_SHARE], sizeof value[0], "Yes, in software");
      return;
    }
  trim_to (value[R_DRIVER], sizeof value[0], name);
  snprintf (value[R_SHARE], sizeof value[0], "%s",
	    flags & 4 ? "Yes" : flags & 1 ? "Through Mesa's own driver in each program"
	    : "No (RISC OS on Linux: RISC3D 0.06, Linux3D 0.06 and riscos-risc3d 0.23 are needed)");
  if (flags & 1)
    {
      /* a DRM device: Mesa's own driver draws, in each program */
      snprintf (value[R_DRAWS], sizeof value[0], "On a GPU, with Mesa's %s driver in each program (libMesaGPU)", drm ? drm : "own");
      snprintf (value[R_RENDERER], sizeof value[0], "Mesa's %s driver, through %s's SWI &%X", drm ? drm : "", value[R_DRIVER], ioctl);
      snprintf (value[R_GL], sizeof value[0], "As Mesa's %s driver gives (programs linked with libMesaGPU)", drm ? drm : "");
      snprintf (value[R_GLES], sizeof value[0], "As Mesa's %s driver gives (programs linked with libMesaGPU)", drm ? drm : "");
      snprintf (value[R_TEXTURE], sizeof value[0], "As Mesa's driver gives");
      snprintf (value[R_EXTS], sizeof value[0], "As Mesa's driver gives");
      return;
    }
  snprintf (value[R_DRAWS], sizeof value[0], "%s", flags & 2 ? "In software (Linux has no GPU driver for this machine)" : "On a GPU");

  /* a command stream: ask, with a context of each kind */
  char ver[160], sl[80], ren[160];
  int tex = 0, ext_gl = -1, ext_es = -1;
  hourglass (1);
  if (ask_gl (R3D_API_GL, ver, sizeof ver, sl, sizeof sl, &tex, &ext_gl, ren, sizeof ren))
    {
      snprintf (value[R_GL], sizeof value[0], "%s (compatibility profile), GLSL %s", version_number (ver), sl);
      trim_to (value[R_RENDERER], sizeof value[0], ren);
      if (tex > 0)
	snprintf (value[R_TEXTURE], sizeof value[0], "%d x %d", tex, tex);
    }
  else
    snprintf (value[R_GL], sizeof value[0], "None (%s)", ver);
  if (ask_gl (R3D_API_GLES2, ver, sizeof ver, sl, sizeof sl, value[R_TEXTURE][0] ? 0 : &tex, &ext_es,
	      value[R_RENDERER][0] ? 0 : ren, sizeof ren))
    {
      char es1[160];
      int one = ask_gl (R3D_API_GLES1, es1, sizeof es1, 0, 0, 0, 0, 0, 0);
      const char *slv = strstr (sl, "GLSL ES ");
      char v1[32];
      snprintf (v1, sizeof v1, "%s", version_number (ver));	/* (one static buffer) */
      snprintf (value[R_GLES], sizeof value[0], "%s%s, GLSL ES %s", v1, one ? " and 1.1" : "",
		slv ? version_number (slv + 8) : sl);
      if (!value[R_RENDERER][0])
	trim_to (value[R_RENDERER], sizeof value[0], ren);
      if (!value[R_TEXTURE][0] && tex > 0)
	snprintf (value[R_TEXTURE], sizeof value[0], "%d x %d", tex, tex);
    }
  else
    snprintf (value[R_GLES], sizeof value[0], "None (%s)", ver);
  hourglass (0);
  if (ext_gl >= 0 && ext_es >= 0)
    snprintf (value[R_EXTS], sizeof value[0], "%d in OpenGL, %d in OpenGL ES", ext_gl, ext_es);
  else if (ext_gl >= 0 || ext_es >= 0)
    snprintf (value[R_EXTS], sizeof value[0], "%d", ext_gl >= 0 ? ext_gl : ext_es);
}

/* --- interactive help -------------------------------------------------- */

static void
help_reply (int *m)
{
  int icn = m[9];			/* (+32 window, +36 icon) */
  const char *h = 0;
  if (icn >= 0 && icn < 2 * ROWS)
    h = help[icn / 2];
  else if (icn >= 2 * ROWS && icn < 2 * ROWS + ndrv)
    h = help[R_FOLDER];
  else if (icn == i_refresh)
    h = "Click SELECT to look again (for a driver loaded or changed since).";
  else if (icn == i_close)
    h = "Click SELECT to close this window.";
  else
    h = "This window shows which 3D driver RISC3D loaded and what it offers programs.";
  int r[64];
  memset (r, 0, sizeof r);
  snprintf ((char *) &r[5], 236, "%s", h);
  r[0] = (int) ((20 + strlen ((char *) &r[5]) + 1 + 3) & ~3u);
  r[3] = m[2];
  r[4] = Message_HelpReply;
  _swix (Wimp_SendMessage, _INR (0, 2), 17, r, m[1]);
}

/* --- main ------------------------------------------------------------ */

int
main (int argc, char **argv)
{
  static const int msgs[] = { Message_HelpRequest, Message_OpenConfigWindow, Message_Quit };
  int task = 0;
  if (_swix (Wimp_Initialise, _INR (0, 3) | _OUT (1), 380, 0x4B534154, "RISC3D", msgs, &task))
    return 1;
  int at_x = -1, at_y = -1;
  for (int i = 1; i + 2 < argc; i++)
    if (!strcmp (argv[i], "-openat"))
      {
	at_x = atoi (argv[i + 1]);
	at_y = atoi (argv[i + 2]);
	break;
      }
  find_out ();
  build ();
  win = create_window ();
  if (win < 0)
    {
      _swix (Wimp_CloseDown, _INR (0, 1), task, 0x4B534154);
      return 1;
    }
  if (at_x < 0)
    {
      int xe = 1, ye = 1, xl = 0, yl = 0;
      _swix (OS_ReadModeVariable, _INR (0, 1) | _OUT (2), -1, 4, &xe);
      _swix (OS_ReadModeVariable, _INR (0, 1) | _OUT (2), -1, 5, &ye);
      _swix (OS_ReadModeVariable, _INR (0, 1) | _OUT (2), -1, 11, &xl);
      _swix (OS_ReadModeVariable, _INR (0, 1) | _OUT (2), -1, 12, &yl);
      at_x = (((xl + 1) << xe) - WIN_W) / 2;
      at_y = (((yl + 1) << ye) + win_h) / 2;
    }
  open_at (at_x, at_y);

  for (;;)
    {
      int b[64], ev = 0;
      _swix (Wimp_Poll, _INR (0, 1) | _OUT (0), 1, b, &ev);	/* (no null events) */
      switch (ev)
	{
	case 2:
	  _swix (Wimp_OpenWindow, _IN (1), b);
	  break;
	case 3:
	  goto done;
	case 6:
	  if (b[3] == win && b[4] == i_close)
	    goto done;
	  if (b[3] == win && b[4] == i_refresh)
	    {
	      /* again, with as many driver lines as there now are */
	      int st[9] = { win };
	      _swix (Wimp_GetWindowState, _IN (1), st);
	      _swix (Wimp_DeleteWindow, _IN (1), &win);
	      find_out ();
	      build ();
	      win = create_window ();
	      if (win < 0)
		goto done;
	      open_at (st[1], st[4]);
	    }
	  break;
	case 8:
	  if (b[6] == 0x1B)			/* Escape */
	    goto done;
	  _swix (Wimp_ProcessKey, _IN (0), b[6]);
	  break;
	case 17:
	case 18:
	  if (b[4] == Message_Quit)
	    goto done;
	  if (b[4] == Message_OpenConfigWindow)
	    open_front ();
	  if (b[4] == Message_HelpRequest && b[8] == win)
	    help_reply (b);
	  break;
	}
    }
done:
  _swix (Wimp_CloseDown, _INR (0, 1), task, 0x4B534154);
  return 0;
}
