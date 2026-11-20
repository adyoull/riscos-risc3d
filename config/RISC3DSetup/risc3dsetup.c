/* RISC3DSetup: Configure's plug-in for RISC3D. Shows which 3D driver
   RISC3D loaded, whether it draws on a GPU, and what it offers: the
   renderer, the OpenGL and OpenGL ES versions, the largest texture, the
   extensions, shared contexts, the contexts open, and what each driver in
   RISC3D's drivers folder did.

   For a command-stream driver (Linux3D) the versions come from asking:
   it opens an OpenGL, an OpenGL ES and an OpenGL ES 1.1 context for a
   moment, as a program would. A DRM device (V3DRender) is drawn with
   Mesa's own driver inside each program, so there it says that.

   Started by Configure with "-openat x y" (the window's top left, OS
   units), else centred. Message_OpenConfigWindow brings it to the front.
   Plain Wimp calls, no Toolbox.

   1.04: the Drivers group's last row is what draws now: the driver in use
   (Linux3D on RISC OS on Linux, V3DRender on a Pi 4 natively, ...), or
   Software3D when none is (turned off, or none starts here): Mesa in each
   program. Not *RISC3D's list of every driver tried.

   1.03: Set applies the choice and the window stays, showing the result.
   Only the choice has a tick box (at the top, in its own group); what is
   only shown is in display fields ("Yes", "No"), as the other values.

   1.02: named RISC3D in Configure; "Load a 3D driver" turns RISC3D's
   driver loading on or off (RISC3D 0.08's RISC3D$Off), kept in
   Choices:RISC3D.Settings for the next start and applied at once when no
   program has a 3D context open. Set and Cancel, as Configure's windows.

   1.01: laid out in groups, with the driver's name large at the top, the
   RISC3D icon, and ticks (the Wimp's option sprites) for features and for
   each driver in the folder.

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <swis.h>
#include "r3d_gl.h"
#define R3D_DIRECT
#include "r3d.h"

#define RISC3D_Info 0x5AF80
#define Message_Quit 0
#define Message_HelpRequest 0x502
#define Message_HelpReply 0x503
#define Message_OpenConfigWindow 0x50D83

/* --- what is shown --------------------------------------------------- */

#define MAXDRV 6
static struct
{
  char headline[120];			/* the driver, or "No 3D driver" */
  char subline[160];			/* GPU or software, in a sentence */
  char renderer[120], gl[100], gles[100], texture[40], exts[60];
  int on_gpu, es1, share;
  int off;				/* RISC3D$Off set: loading a driver turned off */
  int nctx, have_driver;
  char driver_module[40];		/* the driver's first word: its module */
  char folder[200];
  int ndrv;
  char drv_name[MAXDRV][40], drv_state[MAXDRV][160];
  int drv_ok[MAXDRV];
  char version_text[40], contexts_text[16];
} info;

/* --- the window --------------------------------------------------------- */

#define WIN_W 1240
#define MARGIN 32
#define LABEL_X 56
#define LABEL_W 250
#define VALUE_X (LABEL_X + LABEL_W + 16)
#define VALUE_R (WIN_W - 56)
#define ROW 56

typedef struct
{
  int x0, y0, x1, y1;
  unsigned flags;
  const char *text, *valid;
  int len;
  const char *help;
} icon;

#define ICONS 64
static icon icons[ICONS];
static int nicons, i_refresh, i_cancel, i_set, i_load;
static int want_on;			/* the choice shown */
static int win = -1, win_h;
static int font = -1;			/* the headline's, or -1: the desktop font */
static char title[] = "RISC3D";

#define F_TEXT 0x1u
#define F_SPRITE 0x2u
#define F_BORDER 0x4u
#define F_HCENTRE 0x8u
#define F_VCENTRE 0x10u
#define F_FILLED 0x20u
#define F_FONT 0x40u
#define F_INDIR 0x100u
#define F_RIGHT 0x200u
#define F_CLICK (3u << 12)
#define F_COLOURS(fg, bg) (((unsigned) (fg) << 24) | ((unsigned) (bg) << 28))

static int
add_icon (int x0, int y0, int x1, int y1, unsigned flags, const char *text, const char *valid, const char *help)
{
  if (nicons == ICONS)
    return -1;
  icon *i = &icons[nicons];
  i->x0 = x0, i->y0 = y0, i->x1 = x1, i->y1 = y1;
  i->flags = flags;
  i->text = text;
  i->valid = valid;
  i->len = (int) strlen (text) + 1;
  i->help = help;
  return nicons++;
}

static void
label_at (int x0, int x1, int y, const char *text, const char *help)
{
  add_icon (x0, y - 48, x1, y, F_TEXT | F_VCENTRE | F_RIGHT | F_INDIR | F_COLOURS (7, 1), text, "", help);
}

static void
value_at (int x0, int x1, int y, const char *text, const char *help)
{
  add_icon (x0, y - 48, x1, y, F_TEXT | F_BORDER | F_VCENTRE | F_FILLED | F_INDIR | F_COLOURS (7, 0), text, "R2", help);
}

static void
row (int *y, const char *lab, const char *val, const char *help)
{
  label_at (LABEL_X, LABEL_X + LABEL_W, *y, lab, help);
  value_at (VALUE_X, VALUE_R, *y, val, help);
  *y -= ROW;
}

/* a label and a short display field saying Yes or No: shown, not clicked */
static void
yes_no_at (int x0, int x1, int y, int on, const char *text, const char *help)
{
  label_at (x0, x1 - 16 - 96, y, text, help);
  value_at (x1 - 96, x1, y, on ? "Yes" : "No", help);
}

/* a clickable option: the choice to load a driver */
static int
option_at (int x0, int x1, int y, int on, const char *text, const char *help)
{
  return add_icon (x0, y - 44, x1, y,
		   F_TEXT | F_SPRITE | F_VCENTRE | F_INDIR | F_CLICK | F_COLOURS (7, 1) | (on ? 1u << 21 : 0), text,
		   "Soptoff,opton", help);
}

/* a group: a channel border, its name over the top edge. Returns the y
   of the first row inside; close it with group_end */
static int group_icon;
static int
group (int y, const char *name)
{
  group_icon = add_icon (MARGIN, y - 40, WIN_W - MARGIN, y - 16, F_BORDER | F_COLOURS (7, 1), "", "R4", 0);
  add_icon (MARGIN + 24, y - 32, MARGIN + 24 + 16 * (int) strlen (name) + 24, y,
	    F_TEXT | F_VCENTRE | F_FILLED | F_INDIR | F_HCENTRE | F_COLOURS (7, 1), name, "", 0);
  return y - 52;
}

static int
group_end (int y)
{
  icons[group_icon].y0 = y - 12;
  return y - 12 - 20;
}

static const char
  h_driver[] = "The 3D driver RISC3D loaded from its drivers folder (!Boot.Resources.!RISC3D.GPU): the first one there that starts on this machine.",
  h_renderer[] = "The GPU and the driver that draw for programs.",
  h_gl[] = "The newest OpenGL a program can have (compatibility profile: OpenGL 1.x calls still work), and its shading language.",
  h_gles[] = "The newest OpenGL ES a program can have, and its shading language.",
  h_texture[] = "The largest texture the GPU takes.",
  h_exts[] = "How many OpenGL and OpenGL ES extensions the driver offers.",
  h_gpu[] = "Yes: programs' 3D is drawn on the machine's GPU. No: in software (with no driver, Mesa draws in each program, as before).",
  h_es1[] = "Yes: programs can have OpenGL ES 1.1 contexts too.",
  h_share[] = "Yes: a program can have contexts that share textures and buffers, one per thread, as web browsers draw. On RISC OS on Linux this needs RISC3D 0.06, Linux3D 0.06 and riscos-risc3d 0.23 or later.",
  h_contexts[] = "How many 3D contexts programs have open now.",
  h_folder[] = "Where RISC3D looks for drivers. Each one there is tried in turn until one starts; a driver refuses to start on machines it isn't for.",
  h_drv[] = "What draws for programs now: the 3D driver RISC3D loaded (Linux3D on RISC OS on Linux, V3DRender on a Raspberry Pi 4), or Software3D: Mesa drawing in software in each program, as without RISC3D.",
  h_version[] = "The version of the RISC3D module.",
  h_load[] = "Click to tick or untick. Ticked: RISC3D loads a 3D driver, so programs can draw on the GPU. Not ticked: no driver is loaded and programs draw in software, as without RISC3D. Then click Set.";

static void
build (void)
{
  nicons = 0;
  int y = -24;

  /* the RISC3D icon, the driver large, and what it draws with */
  /* (an indirected sprite-only icon: name, sprite area (1: the Wimp's),
     length) */
  int sp = add_icon (MARGIN, y - 80, MARGIN + 88, y, F_SPRITE | F_HCENTRE | F_VCENTRE | F_INDIR | F_COLOURS (7, 1),
		     "co_risc3d", (const char *) 1, h_driver);
  icons[sp].len = (int) sizeof "co_risc3d";
  if (font >= 0)
    add_icon (MARGIN + 104, y - 48, WIN_W - MARGIN, y, F_TEXT | F_VCENTRE | F_FONT | F_INDIR | ((unsigned) font << 24),
	      info.headline, "", h_driver);
  else
    add_icon (MARGIN + 104, y - 44, WIN_W - MARGIN, y, F_TEXT | F_VCENTRE | F_INDIR | F_COLOURS (7, 1), info.headline, "",
	      h_driver);
  add_icon (MARGIN + 104, y - 88, WIN_W - MARGIN, y - 48, F_TEXT | F_VCENTRE | F_INDIR | F_COLOURS (7, 1),
	    info.subline, "", h_gpu);
  y -= 112;

  /* the one choice */
  y = group (y, "Use");
  i_load = option_at (LABEL_X, VALUE_R, y, want_on, "Load a 3D driver, so programs can draw on the GPU", h_load);
  y -= ROW;
  y = group_end (y + 8);

  /* what programs get */
  y = group (y, "Graphics");
  row (&y, "Renderer", info.renderer, h_renderer);
  row (&y, "OpenGL", info.gl, h_gl);
  row (&y, "OpenGL ES", info.gles, h_gles);
  label_at (LABEL_X, LABEL_X + LABEL_W, y, "Largest texture", h_texture);
  value_at (VALUE_X, VALUE_X + 240, y, info.texture, h_texture);
  label_at (VALUE_X + 240, VALUE_X + 240 + 180, y, "Extensions", h_exts);
  value_at (VALUE_X + 436, VALUE_R, y, info.exts, h_exts);
  y -= ROW;
  y = group_end (y + 8);

  y = group (y, "Features");
  int third = (VALUE_R - LABEL_X) / 3;
  yes_no_at (LABEL_X, LABEL_X + third, y, info.on_gpu, "On the GPU", h_gpu);
  yes_no_at (LABEL_X + third, LABEL_X + 2 * third, y, info.es1, "OpenGL ES 1.1", h_es1);
  yes_no_at (LABEL_X + 2 * third, VALUE_R, y, info.share, "Shared contexts", h_share);
  y -= ROW;
  label_at (LABEL_X, LABEL_X + LABEL_W, y, "Contexts open", h_contexts);
  value_at (VALUE_X, VALUE_X + 120, y, info.contexts_text, h_contexts);
  y -= ROW;
  y = group_end (y + 8);

  y = group (y, "Drivers");
  row (&y, "Folder", info.folder[0] ? info.folder : "(none: !RISC3D hasn't been seen)", h_folder);
  static char in_use[160];
  if (info.have_driver)
    snprintf (in_use, sizeof in_use, "In use: draws for programs");
  else
    snprintf (in_use, sizeof in_use, "%s",
	      info.off ? "In use: RISC3D is turned off; Mesa draws in software"
		       : "In use: no driver starts here; Mesa draws in software");
  row (&y, info.have_driver ? info.driver_module : "Software3D", in_use, h_drv);
  y = group_end (y + 8);

  /* RISC3D's version, and the buttons */
  y -= 8;
  add_icon (MARGIN, y - 68, MARGIN + 400, y, F_TEXT | F_VCENTRE | F_INDIR | F_COLOURS (5, 1), info.version_text, "",
	    h_version);
  i_refresh = add_icon (WIN_W - MARGIN - 3 * 200 - 2 * 24, y - 68, WIN_W - MARGIN - 2 * 200 - 2 * 24, y,
			F_TEXT | F_BORDER | F_HCENTRE | F_VCENTRE | F_FILLED | F_INDIR | F_CLICK | F_COLOURS (7, 1),
			"Refresh", "R5,3", "Click SELECT to look again (for a driver loaded or changed since).");
  i_cancel = add_icon (WIN_W - MARGIN - 2 * 200 - 24, y - 68, WIN_W - MARGIN - 200 - 24, y,
		       F_TEXT | F_BORDER | F_HCENTRE | F_VCENTRE | F_FILLED | F_INDIR | F_CLICK | F_COLOURS (7, 1),
		       "Close", "R5,3", "Click SELECT to close this window (a choice not Set is left as it was).");
  i_set = add_icon (WIN_W - MARGIN - 200, y - 68, WIN_W - MARGIN, y,
		    F_TEXT | F_BORDER | F_HCENTRE | F_VCENTRE | F_FILLED | F_INDIR | F_CLICK | F_COLOURS (7, 1),
		    "Set", "R6,3", "Click SELECT to use the choice now (when no program has a 3D context open) and keep it for the next start. The window stays open and shows the result.");
  win_h = -(y - 68 - 24);
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
    *exts = count_words ((const char *) glGetString (GL_EXTENSIONS));
  r3d_make_current (0);
  r3d_destroy (c);
  return 1;
}

/* "OpenGL ES 3.1 RISC3D (Mesa x)" -> "3.1"; "1.40" -> "1.40" */
static void
version_number (char *out, size_t n, const char *v)
{
  const char *p = v;
  while (*p && !(*p >= '0' && *p <= '9'))
    p++;
  size_t i = 0;
  while (i < n - 1 && ((p[i] >= '0' && p[i] <= '9') || p[i] == '.'))
    {
      out[i] = p[i];
      i++;
    }
  out[i] = 0;
  if (!i)
    snprintf (out, n, "%s", v);
}

/* " (RISC3D, Linux GPU)" off the renderer: said elsewhere */
static void
plain_renderer (char *r)
{
  char *p = strstr (r, " (RISC3D");
  if (p)
    *p = 0;
}

/* *RISC3D's own account, for the drivers folder and what was tried */
static void
read_star_risc3d (void)
{
  info.ndrv = 0;
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
	trim_to (info.folder, sizeof info.folder, t + 16);
      else if (t[0] == ' ' && t[1] == ' ' && info.ndrv < MAXDRV)
	{
	  /* "  Linux3D: registered" or "  V3DRender: <why not>" */
	  char *colon = strchr (t + 2, ':');
	  int d = info.ndrv++;
	  if (colon)
	    {
	      *colon = 0;
	      trim_to (info.drv_name[d], sizeof info.drv_name[0], t + 2);
	      trim_to (info.drv_state[d], sizeof info.drv_state[0], colon[1] == ' ' ? colon + 2 : colon + 1);
	    }
	  else
	    trim_to (info.drv_name[d], sizeof info.drv_name[0], t + 2);
	  info.drv_ok[d] = !strncmp (info.drv_state[d], "registered", 10);
	  if (info.drv_ok[d])
	    snprintf (info.drv_state[d], sizeof info.drv_state[0], "Started: draws for programs");
	}
    }
  free (buf);
}

static void
find_out (void)
{
  memset (&info, 0, sizeof info);
  snprintf (info.renderer, sizeof info.renderer, "\x97");
  snprintf (info.gl, sizeof info.gl, "\x97");
  snprintf (info.gles, sizeof info.gles, "\x97");
  snprintf (info.texture, sizeof info.texture, "\x97");
  snprintf (info.exts, sizeof info.exts, "\x97");
  int version = 0, ndrivers = 0, nctx = 0;
  const char *name = 0, *drm = 0;
  unsigned flags = 0, ioctl = 0;
  if (_swix (RISC3D_Info, _OUTR (0, 6), &version, &ndrivers, &nctx, &name, &flags, &ioctl, &drm))
    {
      snprintf (info.headline, sizeof info.headline, "RISC3D isn't loaded");
      snprintf (info.subline, sizeof info.subline, "Programs draw in software. !Boot.Resources.!RISC3D loads RISC3D at start-up.");
      snprintf (info.version_text, sizeof info.version_text, "RISC3D: not loaded");
      snprintf (info.contexts_text, sizeof info.contexts_text, "0");
      return;
    }
  snprintf (info.version_text, sizeof info.version_text, "RISC3D %d.%02d", version / 100, version % 100);
  snprintf (info.contexts_text, sizeof info.contexts_text, "%d", nctx);
  info.nctx = nctx;
  {
    char o[8];
    int n = 0;
    info.off = !_swix (OS_ReadVarVal, _INR (0, 4) | _OUT (2), "RISC3D$Off", o, sizeof o - 1, 0, 0, &n) && n > 0;
  }
  read_star_risc3d ();
  /* (*RISC3D's list is of what was tried when it last looked: a driver
     since removed, as when turned off, isn't drawing now) */
  for (int d = 0; d < info.ndrv && !name; d++)
    if (info.drv_ok[d])
      {
	info.drv_ok[d] = 0;
	snprintf (info.drv_state[d], sizeof info.drv_state[0], "%s", info.off ? "Not loaded: RISC3D is turned off" : "Not loaded");
      }
  if (!name && info.off)
    {
      snprintf (info.headline, sizeof info.headline, "RISC3D is turned off");
      snprintf (info.subline, sizeof info.subline, "No 3D driver is loaded: programs draw in software.");
      return;
    }
  if (!name)
    {
      snprintf (info.headline, sizeof info.headline, "No 3D driver");
      snprintf (info.subline, sizeof info.subline, "No driver in the folder starts on this machine: programs draw in software.");
      return;
    }
  trim_to (info.headline, sizeof info.headline, name);
  info.have_driver = 1;
  {
    size_t k = 0;
    while (name[k] > ' ' && k < sizeof info.driver_module - 1)
      {
	info.driver_module[k] = name[k];
	k++;
      }
    info.driver_module[k] = 0;
  }
  info.share = (flags & 4) != 0;
  if (flags & 1)
    {
      /* a DRM device: Mesa's own driver draws, in each program */
      info.on_gpu = 1;
      info.share = 1;
      snprintf (info.subline, sizeof info.subline, "Drawing on the GPU, with Mesa's %s driver in each program", drm ? drm : "own");
      snprintf (info.renderer, sizeof info.renderer, "Mesa's %s driver (programs linked with libMesaGPU)", drm ? drm : "");
      snprintf (info.gl, sizeof info.gl, "As Mesa's %s driver gives", drm ? drm : "");
      snprintf (info.gles, sizeof info.gles, "As Mesa's %s driver gives", drm ? drm : "");
      return;
    }
  info.on_gpu = !(flags & 2);

  /* a command stream: ask, with a context of each kind */
  char ver[160], sl[80], ren[160] = "", v[32], s[32];
  int tex = 0, ext_gl = -1, ext_es = -1;
  _swix (Hourglass_On, 0);
  if (ask_gl (R3D_API_GL, ver, sizeof ver, sl, sizeof sl, &tex, &ext_gl, ren, sizeof ren))
    {
      version_number (v, sizeof v, ver);
      version_number (s, sizeof s, sl);
      snprintf (info.gl, sizeof info.gl, "%s, compatibility profile \x97 GLSL %s", v, s);
    }
  else
    snprintf (info.gl, sizeof info.gl, "None (%s)", ver);
  if (ask_gl (R3D_API_GLES2, ver, sizeof ver, sl, sizeof sl, tex ? 0 : &tex, &ext_es, ren[0] ? 0 : ren, sizeof ren))
    {
      version_number (v, sizeof v, ver);
      const char *g = strstr (sl, "GLSL ES ");
      version_number (s, sizeof s, g ? g + 8 : sl);
      snprintf (info.gles, sizeof info.gles, "%s \x97 GLSL ES %s", v, s);
      char es1[160];
      info.es1 = ask_gl (R3D_API_GLES1, es1, sizeof es1, 0, 0, 0, 0, 0, 0);
    }
  else
    snprintf (info.gles, sizeof info.gles, "None (%s)", ver);
  _swix (Hourglass_Off, 0);
  if (ren[0])
    {
      plain_renderer (ren);
      trim_to (info.renderer, sizeof info.renderer, ren);
    }
  if (tex > 0)
    snprintf (info.texture, sizeof info.texture, "%d \xD7 %d", tex, tex);
  if (ext_gl >= 0 && ext_es >= 0)
    snprintf (info.exts, sizeof info.exts, "%d OpenGL, %d OpenGL ES", ext_gl, ext_es);
  else if (ext_gl >= 0 || ext_es >= 0)
    snprintf (info.exts, sizeof info.exts, "%d", ext_gl >= 0 ? ext_gl : ext_es);
  if (info.on_gpu)
    snprintf (info.subline, sizeof info.subline, "Drawing on the GPU: %s", info.renderer);
  else
    snprintf (info.subline, sizeof info.subline, "Drawing in software: Linux has no GPU driver for this machine");
}

/* --- the choice ------------------------------------------------------- */

static void
report (const char *text)
{
  int e[64];
  e[0] = 0;
  snprintf ((char *) &e[1], 240, "%s", text);
  _swix (Wimp_ReportError, _INR (0, 2), e, 1 | 16, "RISC3D");	/* OK, no "Error from" */
}

/* keep it for the next start: Choices:RISC3D.Settings, which !RISC3D's
   !Boot runs before loading RISC3D */
static int
save_choice (int on)
{
  const char *text = on ? "| RISC3D: written by Configure's RISC3D window\nUnset RISC3D$Off\n"
		       : "| RISC3D: written by Configure's RISC3D window\nSet RISC3D$Off 1\n";
  _swix (OS_File, _INR (0, 1) | _IN (4), 8, "<Choices$Write>.RISC3D", 0);
  return !_swix (OS_File, _INR (0, 2) | _INR (4, 5), 10, "<Choices$Write>.RISC3D.Settings", 0xFEB, text,
		 text + strlen (text));
}

/* now, where nothing is drawing: off, the driver module goes; on, RISC3D
   starts again and loads one */
static void
apply_choice (int on)
{
  char cmd[80];
  if (on)
    _swix (OS_SetVarVal, _INR (0, 4), "RISC3D$Off", 0, -1, 0, 0);
  else
    _swix (OS_SetVarVal, _INR (0, 4), "RISC3D$Off", "1", 1, 0, 0);
  find_out ();
  if (info.nctx > 0)
    {
      report (on ? "RISC3D will load a 3D driver when the machine next starts: programs have 3D contexts open now."
		 : "RISC3D won't load a 3D driver when the machine next starts. Programs have 3D contexts open now, so the driver stays until then.");
      return;
    }
  if (!on && info.have_driver && info.driver_module[0])
    {
      snprintf (cmd, sizeof cmd, "RMKill %s", info.driver_module);
      _swix (OS_CLI, _IN (0), cmd);
    }
  if (on && !info.have_driver)
    _swix (OS_CLI, _IN (0), "RMReInit RISC3D");
}

/* --- interactive help -------------------------------------------------- */

static void
help_reply (int *m)
{
  int icn = m[9];			/* (+32 window, +36 icon) */
  const char *h = icn >= 0 && icn < nicons ? icons[icn].help : 0;
  if (!h)
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

static void
make (void)
{
  find_out ();
  want_on = !info.off;
  build ();
  win = create_window ();
}

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
  /* the headline in Homerton Bold, 14 point (the desktop font if none) */
  if (_swix (Font_FindFont, _INR (1, 5) | _OUT (0), "Homerton.Bold", 14 * 16, 14 * 16, 0, 0, &font))
    font = -1;
  make ();
  if (win < 0)
    goto done;
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
	  if (b[3] == win && b[4] == i_cancel)
	    goto done;
	  if (b[3] == win && b[4] == i_load)
	    {
	      want_on = !want_on;
	      int blk[4] = { win, i_load, want_on ? 1 << 21 : 0, 1 << 21 };	/* (EOR, clear: selected) */
	      _swix (Wimp_SetIconState, _IN (1), blk);
	    }
	  if (b[3] == win && b[4] == i_set)
	    {
	      if (want_on == info.off)	/* (changed) */
		{
		  if (!save_choice (want_on))
		    report ("RISC3D couldn't keep the choice in Choices:RISC3D.Settings: it lasts until the machine next starts.");
		  apply_choice (want_on);
		}
	      /* (and stay, showing what it did) */
	    }
	  if (b[3] == win && (b[4] == i_refresh || b[4] == i_set))
	    {
	      /* again, with as many driver lines as there now are */
	      int st[9] = { win };
	      _swix (Wimp_GetWindowState, _IN (1), st);
	      _swix (Wimp_DeleteWindow, _IN (1), &win);
	      make ();
	      if (win < 0)
		goto done;
	      open_at (st[1], st[4]);
	    }
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
  if (font >= 0)
    _swix (Font_LoseFont, _IN (0), font);
  _swix (Wimp_CloseDown, _INR (0, 1), task, 0x4B534154);
  return 0;
}
