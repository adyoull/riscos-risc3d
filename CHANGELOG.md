# Changelog

## 0.14: turn RISC3D off from Configure; the window laid out afresh (20 Nov 2026)

- **!RISC3D 0.08-1** (`out/RISC3D-0.08-1.zip`): RISC3D 0.08, Linux3D 0.06,
  V3DRender 0.16, and !RISC3DSetup 1.02.
- **RISC3D module 0.08:**
  - **`RISC3D$Off`:** while it is set, RISC3D loads no driver from its
    folder, so programs draw in software. A driver that registers itself is
    still taken. `*RISC3D` says when loading is turned off. Info r0 = 8.
  - **`*RMReInit RISC3D` starts afresh.** Its initialisation now clears what
    it kept (drivers, contexts, the folder, "looked"), because RMReInit runs
    it again in the same copy of the module. Before this, a reinitialised
    RISC3D never loaded a driver again.
- **!RISC3D's !Boot and !Run** run `Choices:RISC3D.Settings` (if it exists)
  before loading RISC3D, and RMEnsure 0.08.
- **!RISC3DSetup 1.02**, Configure's **RISC3D** window (the owner: "the
  plugin should be RISC3D its name"; "make the UI better looking"; "turn it
  off"):
  - Configure's icon and the window's title are now "RISC3D".
  - The window is laid out in groups: Graphics, Features and Drivers.
    - At the top are the RISC3D icon, the driver's name in Homerton Bold
      14pt, and a line saying what draws.
    - Features (GPU, OpenGL ES 1.1, shared contexts) and each driver in the
      folder have ticks, from the Wimp's option sprites. A driver that isn't
      drawing is in grey, with why.
    - The renderer is shown without " (RISC3D, Linux GPU)", and the largest
      texture reads "4096 × 4096".
    - Each icon has its own interactive help.
  - **Load a 3D driver**, with Set and Cancel:
    - Set keeps the choice in `Choices:RISC3D.Settings` (`Set` or `Unset`
      `RISC3D$Off`).
    - With no 3D context open, it also takes effect at once. Off kills the
      driver's module; on runs `*RMReInit RISC3D`, which loads the driver
      again. Otherwise it says the change waits for the next start.
  - Fixed during this work:
    - An indirected sprite-only icon takes the sprite's name, its area (1:
      the Wimp's) and the length, not validation. With validation there, the
      first build crashed at start with an EMT trap.
    - `!Run` loads CoSprite too.
- **Tested on the Pi 4 under ROL, through TestAgent:**
  - The window was right with the driver loaded and when turned off.
  - Turning off removed Linux3D, wrote the Settings file and showed "RISC3D
    is turned off". Turning on again reloaded Linux3D 0.06, sharing
    included.
  - `*RMReInit RISC3D` now reloads the driver.
  - Not yet opened from inside Configure, and not yet tested at start-up
    with the Settings file.
- !RISC3D 0.07-1 (release 0.13, delivered earlier today) is superseded.

## 0.13: Configure's 3D plug-in; RISC3D 0.07 (20 Nov 2026)

- **!RISC3DSetup 1.00**, Configure's plug-in (for `!Boot.Resources.Configure`;
  Configure shows it as "3D"). It shows:
  - the driver RISC3D loaded, and whether it draws on a GPU or in software;
  - the renderer;
  - the newest OpenGL (compatibility profile) and OpenGL ES, with their
    shading languages, and whether OpenGL ES 1.1 is there;
  - the largest texture and the extension counts;
  - whether contexts can share objects, and the contexts open;
  - RISC3D's version, and the drivers folder with what each driver there did.

  On a command-stream driver (Linux3D) it opens an OpenGL, an OpenGL ES and
  an OpenGL ES 1.1 context for a moment to ask. On a DRM device (V3DRender)
  it says Mesa's own driver draws, in each program. It has Refresh,
  interactive help, `-openat x y` and `Message_OpenConfigWindow` (&50D83).
  It is plain C with Wimp calls, linked with the client library
  (`config/RISC3DSetup/`).
- **RISC3D module 0.07:** the drivers folder from `RISC3D$Drivers` ends at its
  first space. A value set with a space after it used to be taken whole, so
  no driver was found (seen through TestAgent's `*Set`). Info r0 = 7.
- **!RISC3D 0.07-1** (`out/RISC3D-0.07-1.zip`): RISC3D 0.07, Linux3D 0.06,
  V3DRender 0.16, and `!RISC3DSetup` beside `!RISC3D` at the top of the zip.
  !Boot and !Run RMEnsure 0.07.
- `tools/mkkit.py`: `SRC=/TOP/NAME` puts a file beside the zip's folder.
- **Tested on the Pi 4 under ROL, through TestAgent (20 Nov):**
  - !RISC3DSetup run from `HardDisc4.Test`: every row right on V3D, with
    Mesa 26.1.6. It shows OpenGL 3.1 compatibility (GLSL 1.40), OpenGL ES
    3.1 and 1.1 (GLSL ES 3.10), textures up to 4096, and 186 and 94
    extensions. Refresh works. Screenshot: `docs/images/risc3dsetup.png`.
    Not yet opened from Configure itself.
  - RISC3D 0.07 found the drivers in a folder set with a trailing space.
  - **Kit 38 passed on the GPU** with riscos-risc3d 0.23-1: `sharetest ok`,
    with every check right. The fence made in one thread and waited for in
    the other worked; 200 frames each at once came out with none wrong; the
    first context was destroyed and a new one shared with the second. r3dtest
    ran at 140.2 and 147.8 fps, unchanged.

## 0.12: contexts that share objects, a current context per thread (19 Nov 2026)

For WebKit 2.52 (riscos-mesa's handoff of 19 November): Skia draws with
OpenGL ES in several threads at once, a context each, all sharing one set
of textures and buffers. riscos-mesa does that natively through its own
GPU driver; on RISC OS on Linux it needs RISC3D.

- **!RISC3D 0.06-1** (`out/RISC3D-0.06-1.zip`, md5 b393a9e6…): RISC3D 0.06,
  Linux3D 0.06; V3DRender 0.16 unchanged. !Boot and !Run RMEnsure 0.06.
- **RISC3D module 0.06:**
  - `RISC3D_Open` r0 bit 0: share the objects of context r1. Sent to that
    context's driver (open's r1 bit 0, r2 = its handle), which must have
    registered with flags bit 2; else error &823D03.
  - Register flags bit 2: the driver can share. Info's r4 bit 2: it can
    now (for drivers with bit 1 too, reason 3's r1 bit 0 says). Info r0 = 6.
  - `*RISC3D` says "contexts can share objects", and which contexts share.
- **Linux3D 0.06:** registers with flags 2 | 4. Takes the pid from rol-3d's
  "OK bytes pid" and opens a sharing context on "rol-3d-<pid>". Reason 3
  reads the whole "rol-3d-info" answer (";share" = r1 bit 0). New error
  &823C07 (rol-3d too old). `*Linux3D` shows each context's Linux process
  and what it shares.
- **rol-3d 0.20** (riscos-risc3d 0.23-1, md5 17867493…):
  - Everything a context keeps is thread-local; what a share group has in
    common (EGL, the function table, sync objects) is not.
  - Each context process listens on "rol-3d-<pid>"; a context opened there
    is a thread with an EGL context in the group's share group, made as the
    first one was (its API, config and version). A different API is refused.
  - The helper's own textures and buffers: 128 names per context of a
    group (by slot).
  - Sync objects: one table per group, under a lock. A wait for a fence not
    yet made (its context's thread hasn't reached it) waits up to 2 s.
  - A context of a group that others outlive deletes its own objects and
    EGL context and ends its thread; the process ends with its last.
  - "rol-3d-info" answers end ";share". ROL3D_LEVEL 8.
- **Client 0.12** (`RISC3D-client-0.12.tgz`, md5 3a6ec013…, from
  `tools/mkclient.sh`):
  - `r3d_create_shared (api, major, minor, share)` and `r3d_can_share ()`;
    `R3D_DRIVER_SHARE` (4).
  - A current context per thread: a plain variable while one thread has
    made contexts current (no cost to one thread), a pthread key for every
    other thread once a second one does.
  - Names (textures, buffers, lists) and sync numbers are one set for a
    share group, taken atomically; display lists' records are the group's,
    under its lock.
  - New C library imports: UnixLib's pthread functions (nine).
- **Tests:** `demo/share/sharetest.c` (sharetest, sharetest-host): two
  threads, viewports per thread, 4000 names at once, a texture and a fence
  from one context used in the other (the fence wait path hit every run),
  200 frames each at once, the first context destroyed while the second
  draws, a third sharing the second, an unshared context, an API mismatch
  refused, an OpenGL display list across contexts. Passed on the build
  machine (llvmpipe) ten times; every earlier host test gives the same
  output and pictures as 0.11, with old and new helpers mixed both ways.
  Not yet run on the Pi: kit 38 (`RISC3DTest-38.zip`, md5 35e2ba56…).
- **build.sh** keeps every delivered zip, kit, package and client tarball
  instead of making them again (it used to remake older kits with the
  newest modules).

## 0.11: the allocated SWI chunk and error blocks (18 Nov 2026)

- **Released as !RISC3D 0.05-1** (`out/RISC3D-0.05-1.zip`): RISC3D 0.05,
  Linux3D 0.05, and V3DRender 0.16 (md5 1fb093ae…, at &5AFC0/&823E00; it
  registers by looking up RISC3D_Register by name), with GPU.Licences naming
  0.16. riscos-mesa pinned 0.11 in 20.3.5-22. Notes: docs/releases/0.11.md.

RISC OS Allocations allocated (18 November 2026):
- **RISC3D:** SWI chunk &5AF80, error block &823D00.
- **Linux3D:** error block &823C00.
- **V3DRender:** SWI chunk &5AFC0, error block &823E00.
- **Names:** RISC3D, Linux3D and V3DRender (module and *command), the
  application !RISC3D, and V3DRender's *V3DClock, *V3DDump, *V3DPower and
  *V3DReset.

The owner chose a clean break: no answering the old numbers. Only riscos-mesa's
!MesaLibs needs rebuilding (pin 0.11).

- **RISC3D module 0.05:**
  - RISC3D_Info &5AF80, _Register &5AF81, _Open &5AF82, _Close &5AF83,
    _Sync &5AF84 (were &C3D00-&C3D04);
  - errors &823D00-&823D02 (were &81D300-&81D302);
  - Info r0 = 5.
- **Linux3D 0.05:** registers at &5AF81; errors &823C00-&823C06 (were
  &81D310-&81D316, in the same order).
- **Client library 0.11** (`RISC3D-client-0.11.tgz`): lib/r3d.h's SWI
  numbers. Nothing else changed.
- **!RISC3D's !Boot and !Run** RMEnsure RISC3D 0.05. A program that checks
  should use `RMEnsure RISC3D 0.05`.
- **Docs:** README, the driver guide (section 7 lists the allocations) and
  !Help.
- **V3DRender** must move to &5AFC0 / &823E00 and register at &5AF81
  before the next !RISC3D package (asked of V3DRender).

## Release 0.10 (16 Nov 2026): !RISC3D 0.04-7 with V3DRender 0.15

- The owner: V3DRender should be the latest: 0.15 (md5 229b10e6…; cache
  work that no longer costs YSFlight its frames) replaces 0.13 in GPU.
  `out/RISC3D-0.04-7.zip`. Release 0.10 is 0.04-7 (0.04-6 never published).

## !RISC3D 0.04-6 with V3DRender 0.13

- V3DRender 0.13 (passed on the owner's Pi 4; refuses to start anywhere but
  a Pi 4/400/CM4, before touching hardware) joins Linux3D in !RISC3D.GPU,
  from V3DRender's release (md5 0d13cc21…), kept in `third/V3DRender`. Its
  licence (GPL-2.0-or-later) and notice are in GPU.Licences and GPU.GPL-2;
  !Help says so. `out/RISC3D-0.04-6.zip`: the one package for every system.
- (Superseded before publishing by 0.04-7.) !RISC3D 0.04-6 (RISC3D module 0.04, Linux3D
  0.04, V3DRender 0.13), riscos-risc3d 0.22-1 (rol-3d 0.19), client library
  0.9 (unchanged; RISC3D-client-0.9.tgz).

## !RISC3D 0.04-5: its icon shown (16 Nov 2026)

- The owner: no sprite was given to the Filer. !Boot and !Run now do
  `IconSprites <RISC3D$Dir>.!Sprites` (the Wimp picks !Sprites11 itself on
  high-resolution screens). `out/RISC3D-0.04-5.zip`.

## RISC OS's side by PackMan on ROL too (riscos-risc3d 0.22-1, Linux3D 0.04, !RISC3D 0.04-4; 16 Nov 2026)

- ROL staged RISC3D 0.04-3 in riscos-harddisc 1.0.0058-1 (installed as
  PackMan would, with an adopt list for 0.19-1..0.21-1's unrecorded copy).
- **riscos-risc3d 0.22-1:** Linux's side only (rol-3d, the Mesa pin);
  Depends riscos-harddisc (>= 1.0.0058-1). Its postinst puts back any of
  !RISC3D's files that dpkg removed on the upgrade from 0.21-1 (same paths
  as PackMan's copy), from the staged copy, when PackMan's Status has
  RISC3D installed; never over a file there under either name. Removes the
  PreDesk.RISC3D file it made (Resources boot loads !RISC3D).
- **Linux3D 0.04:** refuses to start where /opt/riscos/rol-3d isn't
  installed (&81D316), as ROL asked: every ROL image now has !RISC3D, and
  without riscos-risc3d programs would be sent to a helper that never comes
  instead of drawing in software.
- **`out/RISC3D-0.04-4.zip`**: !RISC3D with Linux3D 0.04.
- V3DRender 0.12 is NOT added yet: it must first refuse to start without
  V3D (other machines would load it otherwise).

## One RISC3D package for every system (!RISC3D 0.04-3, Linux3D 0.03; 16 Nov 2026)

- The owner: RISC3D managed by PackMan everywhere, ROL included; ROL's
  distribution packages carry it for updates, and the user can update it
  with PackMan; one package for all systems.
- **Linux3D 0.03** refuses to start where IXSupport_LinuxSyscall isn't
  there (error &81D315 "Linux3D is the 3D driver for RISC OS on Linux
  only"), so RISC3D moves on to the next module in GPU.
- **`out/RISC3D-0.04-3.zip`**: !RISC3D with the icon and GPU.Linux3D; the
  same on every system. V3DRender to be added to GPU (asked of V3DRender).
  !Help and GPU.ReadMe say so.

## !RISC3D's icon and the RISC3D logo (16 Nov 2026)

- `art/`: draw.py draws the icon (an isometric cube with a triangle mesh,
  blues) and the logo; mksprites.py writes RISC OS sprite files: 32bpp
  sprites with 8-bit alpha masks (RISC OS 5.21 on). `dist/!RISC3D/!Sprites`
  (!risc3d 34x34 and sm!risc3d 18x18 at 90 dpi) and `!Sprites11` (68x68 and
  36x36 at 180 dpi). Logo: art/RISC3D-logo.png (1200x400), icon at 512 px.
  Not yet in a release: the next !RISC3D zip and riscos-risc3d carry them.

## !RISC3D loads RISC3D at start-up (riscos-risc3d 0.21-1, !RISC3D 0.04-2, kit 37; 16 Nov 2026)

- Kit 36 worked. The owner asked for programs not to need a path to load
  RISC3D: !RISC3D's !Boot (and !Run) now also do
  `RMEnsure RISC3D 0.04 RMLoad <RISC3D$Dir>.RISC3D`, so RISC3D (and through
  it the GPU driver) is there from start-up and programs need nothing in
  their !Run. Module unchanged (0.04).

## !RISC3D: RISC3D finds its own driver (module 0.04, riscos-risc3d 0.20-1, kit 36; 15 Nov 2026)

- The owner: programs' !Run files shouldn't have to name the driver. RISC3D
  is now installed as **!RISC3D in !Boot.Resources**: !Boot/!Run set
  RISC3D$Dir and RISC3D$Path, and it holds the RISC3D module and a folder
  **GPU** with the driver for that machine (each GPU's own package puts it
  there; PackMan allows several packages' files in one folder). A program
  needs only `RMEnsure RISC3D 0.04 RMLoad RISC3D:RISC3D`.
- **RISC3D module 0.04:** after starting (OS_AddCallBack), or at the first
  Info/Open/*RISC3D if that comes first, it loads driver modules from
  `<RISC3D$Dir>.GPU` (or `<RISC3D$Drivers>`, for testing), one at a time
  in directory order, until one registers. A driver whose GPU isn't there
  refuses to start, so RISC3D needs no table of machines. Nothing is loaded
  if a driver has already registered. *RISC3D lists the folder and what
  was tried. Info r0 = 4.
- Reloading RISC3D now reloads Linux3D from GPU, which fixes "Linux3D left
  unregistered after RISC3D is reloaded".
- **riscos-risc3d 0.20-1:** !RISC3D in HardDisc4.!Boot.Resources with
  GPU.Linux3D (TestBed.RISC3D goes); PreDesk.RISC3D runs !RISC3D's !Boot
  and loads RISC3D:RISC3D only (rewritten if it is 0.19-1's or older).
  rol-3d unchanged (0.19).
- `dist/!RISC3D` (the app without a driver) and `out/RISC3D-0.04.zip`, the
  release for a PackMan package. tools/mkkit.py takes SRC=SUB/NAME for
  nested folders.
- Kit 36: Auto (start-up, reload with RMLoad RISC3D:RISC3D only, a draw)
  and a bare !RISC3D for native RISC OS with V3DRender.

## Kit 35: for the Windows Dev Kit 2023 (15 Nov 2026)

- ROL built a Dev Kit test image with riscos-risc3d 0.19-1 and Mesa
  26.1.6 installed (Adreno 690 through freedreno). Kit 35 has kit 34's
  programs with a Dev Kit !ReadMe (nothing to install) and a `devkit`
  script: machine, DRM devices and drivers, GPU/display kernel messages,
  Mesa packages, `rol-3d --check`, linux-tests, lit and litx2 on the GPU
  and in software, and rol-3d's log, into DevKitLog and DevKit-*.png.
  No Pi-only scripts (newmesa, verify, litx). No code changed.

## 0.9: released (15 Nov 2026)

- **Kit 34 on the Pi:** riscos-risc3d 0.19-1 installed, then apt update
  and full-upgrade. apt-cache policy shows libgl1-mesa-dri and
  mesa-libgallium 26.1.6-1~bpo13+1 installed (pin 500; trixie's
  25.0.7 is also 500, and the higher version wins). With Mesa 26.1.6:
  - lit: all six parts right, including the normal not in an array;
  - litx2: all twelve cells right;
  - the TORCS replay: right (livery, cockpit, trees, grass), with no
    texture-matrix line, as 0.19 leaves it off.
- **Earlier in the day (still Mesa 25.0.7, client 0.8 in !MesaLibs):**
  TORCS ran about 24-30 fps by its trace, with 6,000-8,000 GL calls and
  2-4 waits a frame. It then aborted at &691D5CEC (in the !MesaLibs
  libraries) on going back to the menus after the race. The likely
  cause is the closed-context crash fixed in this client (287b6cc).
  It's confirmed once !MesaLibs carries 0.9.
- **Client 0.9 (RISC3D-client-0.9.tgz, tag v0.9):**
  - Arrays from the program's memory are sent from the lowest index
    used (TORCS's strips; with rol-3d 0.8 on, given to Mesa as memory).
  - Closed contexts drop later calls (the exit abort).
  - New r3d_uploads().
  - C library imports are the same as 0.8's.
- **Package:** riscos-risc3d 0.19-1, carrying rol-3d 0.19 and the Mesa
  pin.

## 0.9, continued: Mesa from trixie-backports, pinned; rol-3d 0.19; kit 34 (15 Nov 2026)

- **ROL's answer (RISC3D/handoffs/2026-11-15-rol-mesa-from-backports-pinned.md):**
  - Debian's Mesa from trixie-backports, pinned, carried by
    riscos-risc3d; ROL's images and packages are unchanged.
  - Backports has Mesa 26.1.6-1~bpo13+1 now, not 25.2, so kit 33's
    newmesa fetched 26.1.6 this morning. Its dependencies are all in
    trixie main.
  - riscos-support 1.0.0055-1's /var/lib/riscos/apt-again hook makes
    Updates run apt update and full-upgrade a second time.
- **riscos-risc3d 0.19-1:**
  - Conffiles /etc/apt/sources.list.d/debian-backports.sources and
    /etc/apt/preferences.d/riscos-risc3d-mesa (src:mesa,
    n=trixie-backports, 500).
  - postinst touches apt-again once (marker
    /var/lib/riscos/risc3d-backports) and says how to upgrade by hand.
  - postrm removes both files on remove or purge, and the marker on
    purge. Mesa stays Recommends without a version (apt can't see
    backports at first install).
- **rol-3d 0.19:** mesa_version() reads GL_VERSION. The V3D
  texture-matrix workaround now applies only when V3D runs Mesa below
  25.2, and in that case it logs how to upgrade. Host: the TORCS replay
  is unchanged.
- **Kit 34:**
  - Install, apt update, full-upgrade, then remove /opt/riscos/gl-test.
  - The `verify` script (`check` is kit 21's GL-error logging) prints apt-cache policy and draws lit, litx2 and
    the TORCS replay with the system Mesa. Then TORCS.

## 0.9, continued: Mesa 25.2 draws everything right (kit 33, 15 Nov 2026)

- **Kit 33 on the Pi:** with Debian's Mesa 25.2 from trixie-backports,
  unpacked in /opt/riscos/gl-test (Linux directly):
  - lit: all six parts right, including the normal not in an array.
  - litx and litx2: all cells right.
  - The TORCS replay is right with rol-3d's V3D fix off and with it on.

  So both V3D faults (lit texture coordinates with an identity texture
  matrix or with fog, and the lit colour with a lone normal) are fixed
  between 25.0.7 and 25.2. The way forward is a newer Mesa, not more
  workarounds; how to ship it is the owner's choice.

## 0.9, continued: fog is V3D's second trigger; trying Mesa 25.2 (kit 33, 15 Nov 2026)

- **Kit 32 on the Pi:**
  - ReplayNoFog (0.18's fix, fog dropped) is RIGHT: livery, cockpit,
    trees, grass.
  - litx2 on the GPU is right for: nudged alone; fog from GL_FOG_COORD;
    a colour array; fog with GL_LIGHT0 off; unlit with fog.
  - It's wrong for every lit, nudged FRAGMENT_DEPTH fog (linear,
    NICEST, EXP) and for not nudged with fog.
  - Normal not in an array: plain colour with MODULATE but right with
    REPLACE, so the lit colour is wrong there, not the texture
    coordinates. Software: all right.
- **Mesa release notes:**
  - 25.0.0 switched the fixed-function vertex program to IO intrinsics.
  - 25.1.0: "v3d/compiler: Fixup output types for all 8 outputs".
  - 25.2.3: fog scale clamp.
  - 25.3.0: "st/glsl: fix packed uniform handling in
    st_nir_lower_fog()".

  Debian has mesa 25.2.x in trixie-backports.
- **Kit 33:** `newmesa` script (no package):
  - Downloads mesa-libgallium, libegl-mesa0, libgbm1 and
    libgl1-mesa-dri from trixie-backports, plus any dependency the Pi
    lacks (LLVM), without installing them.
  - Unpacks them into /opt/riscos/gl-test and removes the backports
    source again.
  - Runs gl1test-linux lit/litx/litx2 and the TORCS replay (with
    ROL3D_TEXMAT=0, then default) with LD_LIBRARY_PATH,
    __EGL_VENDOR_LIBRARY_DIRS, LIBGL_DRIVERS_PATH and GBM_BACKENDS_PATH
    pointing at it.
  - Writes NewMesa-*.png and NewMesaLog.

  If 25.2 is right, rol-3d can carry it in /opt/riscos/gl (its private
  Mesa path).

## 0.9, continued: 0.18 fixes the lit test but not TORCS; gl1test litx2; kit 32 (15 Nov 2026)

- **Kit 31 on the Pi (rol-3d 0.18, "texture matrices kept off the
  identity ..., 8 units"):** lit parts 1-5 now draw right through RISC3D.
  Part 6 (normal not in an array) is still one plain colour. The TORCS
  replay and TORCS itself are unchanged.
- **The TORCS recording's draws:** 500,401 glDrawElements, lit and
  textured, with normal, texture and vertex arrays (lit part 2's case);
  the rest unlit, or glDrawArrays with T V. Current colour from
  glColor4fv (colour material). Fog linear is on, which lit/litx don't
  have.
- **gl1test litx2 (new):** twelve lit cells with the 0.18 nudge applied
  by the test itself (the direct build doesn't go through rol-3d):
  - nudged, then with linear fog, fog with GL_NICEST, GL_EXP fog, and
    fog from GL_FOG_COORD;
  - normal not in an array (also with REPLACE), a colour array, and fog
    with normal not in an array;
  - not nudged with fog, nudged with fog and GL_LIGHT0 off, unlit with
    fog.

  Host: through RISC3D = direct (8befd9f3).
- **Kit 32:**
  - `litx2` script: gl1test-linux litx2 on the GPU and in software.
  - `replay`: the TORCS recording with 0.18, as normal and with
    ROL3D_REPLAY=nofog.
  - No package (0.18-1 stays).

## 0.9, continued: V3D's lit-texture bug worked round in rol-3d 0.18; kit 31 (15 Nov 2026)

- **Kit 30 on the Pi** (Linux's Mesa directly, 3.1 compat; the same
  with MESA_GL_VERSION_OVERRIDE=2.1):
  - Right on the GPU: unlit, GL_LIGHT0 off, a texture matrix scaled by
    1.0001, and GL_OBJECT_LINEAR texgen.
  - Wrong: glBegin/glEnd, VBO, 4-component texture coordinates, local
    viewer, two-sided and flat shading. Separate specular is worse
    still: one plain colour.
  - Software: all right.

  So Mesa 25.0.7 on V3D loses fixed-function texture coordinates when
  lighting is on, a light is enabled, and the texture matrix is the
  identity (the pass-through case).
- **rol-3d 0.18 (riscos-risc3d 0.18-1):** on V3D only (GL_RENDERER
  contains "V3D"; `ROL3D_TEXMAT=0` off, `=1` on anywhere), on desktop
  compat contexts (and ES1, which runs on one), every texture matrix left
  at the identity goes to Mesa as diag(1, 1, 1.00000012, 1). For 1D/2D
  textures the coordinates are the same; for 3D and cube ones, r
  changes by 1 part in 8 million.
  - Set for every unit (GL_MAX_TEXTURE_COORDS) when the context is
    made.
  - Checked again after texture-mode matrix ops (load, mult, translate,
    rotate, scale, frustum, ortho, pop).
  - All units checked again after glPopAttrib, glCallList(s) and the
    EXT_dsa glMatrix*EXT ops.

  The library keeps the program's own matrices, so reads still give the
  identity. A wrapper around dispatch (dispatch_main) does it; the
  replay uses it too.
- **Host, with ROL3D_TEXMAT=1 forced:** gl1test scene and answers, lit
  (3ab4b9dc), litx (68955b26), multi (8e153fe7), estest all, gl3test
  all, targettest and the TORCS replay are all identical to off.
- **Kit 31:** install 0.18-1. `Lit` (through RISC3D) and `replay` (the
  TORCS recording on the GPU, as normal) should both now be right; then
  TORCS itself.

## 0.9, continued: a bug in the Pi's Mesa; gl1test litx; kit 30 (15 Nov 2026)

- **Kit 29 on the Pi:** LitLinuxPicture (Linux's own Mesa, no RISC OS)
  = LitPicture (through RISC3D). Unlit is right; all five lit parts lose
  their texture coordinates: horizontal stripes, and a plain colour
  with the normal not in an array. ReplayNoSpec and ReplayNoEmis are
  still wrong. So it's a bug in Debian's Mesa 25.0.7 on V3D with
  GL_LIGHTING, not RISC3D's. No matching report or fix found in Mesa's
  release notes.
- **gl1test litx (new):** twelve variations of lit part 2, to find one
  V3D gets right:
  - as lit part 2, and unlit;
  - glBegin/glEnd, VBO arrays, 4-component texture coordinates;
  - GL_LIGHT0 off, local viewer, two-sided, separate specular;
  - texture matrix scaled by 1.0001, GL_OBJECT_LINEAR texgen, flat
    shading.

  Host: through RISC3D = Mesa directly (68955b26).
- **Kit 30:** `litx` script runs gl1test-linux litx on the GPU, on the
  GPU with MESA_GL_VERSION_OVERRIDE=2.1, and in software. No new package
  (still 0.17-1, not carried).

## 0.9, continued: it's lighting; rol-3d 0.17 and gl1test lit narrow it down; kit 29 (14 Nov 2026)

- **Kit 28 on the Pi:** ReplayNoLight and ReplayNoFogLight are right (car
  livery, cockpit, trees). ReplayNoFog and ReplayNoColMat are as wrong as
  before. So the fault needs GL_LIGHTING on: V3D (Mesa 25.0.7) draws
  TORCS's textures wrongly when lit.
- **TORCS's lighting (from the recording):**
  - GL_LIGHT0 directional at (0, -15000, 10000, 0), set before the
    camera; ambient 0.2, diffuse 1, specular 0.1.
  - Colour material front-and-back, ambient-and-diffuse.
  - Car material: ambient 0.8, specular 0.5, shininess 50, emission
    0.4/0.2. Another material: specular 1, shininess 0, emission 0.
  - Two-sided off, single colour; normals in client arrays.
- **gl1test lit (new):** draws a plane as TORCS draws it (client arrays,
  Push/PopClientAttrib, strips, a 128x128 mipmapped texture with every
  level, MODULATE), with TORCS's light. Six parts: unlit, car material,
  specular 1 with shininess 0, no specular, no colour material, normal
  not in an array. Writes a PNG (gl1test's own writer). Host: through
  RISC3D = Mesa directly, byte for byte (3ab4b9dc).
- **rol-3d 0.17 (riscos-risc3d 0.17-1):** ROL3D_REPLAY nospec and noemis
  (glMaterialfv/glLightfv GL_SPECULAR or GL_EMISSION given as black).
  Host: the unchanged replay is identical; both variants differ.
- **Kit 29:**
  - `Lit` obey file (RISC3D).
  - `litlinux` script (Linux's Mesa directly). It's not called `lit`,
    because RISC OS names ignore case and it would clash with `Lit`.
  - `replay` writes ReplayNoSpec and ReplayNoEmis.

## 0.9, continued: textures right on the GPU; rol-3d 0.16 tries fog and lighting; kit 28 (14 Nov 2026)

- **Kit 27 on the Pi (recording ctx-477, 3,359,686 commands):** texcheck
  on V3D: 909 levels read back the same and 9 differ, all texture 156,
  just as in software and on the host. Both GPU test cards (mipmapped
  and level 0) match the software ones. So V3D stores and samples every
  texture right in a plain state. The fault comes from state that TORCS
  has on while drawing: the cards had lighting, fog, alpha, blend,
  culling, colour material and so on off, REPLACE, and identity
  matrices.
- **What TORCS has on:** linear fog (start 300, end 600, glFogi mode
  GL_LINEAR, colour), lighting with GL_LIGHT0 and GL_COLOR_MATERIAL
  (front-and-back, ambient-and-diffuse), single colour, smooth shading.
  The prime suspect is fog: a fog value per vertex next to the texture
  coordinates, and the stripes look like t read from something that
  barely changes. gl1test's Track (right on the Pi) has no fog.
- **rol-3d 0.16 (riscos-risc3d 0.16-1):** ROL3D_REPLAY nofog, nolight and
  nocolmat (that glEnable dropped). Host: the unchanged replay is
  identical to 0.15's; the variants differ as expected.
- **Kit 28:** `replay` writes ReplayNoFog, ReplayNoLight, ReplayNoColMat
  and ReplayNoFogLight (GPU).

## 0.9, continued: rol-3d 0.15 checks textures on the GPU; kit 27 (14 Nov 2026)

- **Kit 26 on the Pi:** only ReplaySoft is right. Imm, NoPush, Clamp,
  Sized, NoAlpha and All are all as wrong as ReplayGPU, so vertex
  arrays, Push/PopClientAttrib, GL_CLAMP, unsized formats and alpha test
  are ruled out. (The machine rebooted once around the run; cause
  unknown.)
- **The pattern:** each texture looks like one row stretched across the
  surface (trees in vertical stripes, grass in streaks, the car the
  colour of one yellow row). The HUD text and dials are right in every
  GPU picture, and they're textures without mipmaps. The car's texture
  (171 in the build machine's recording) is an ordinary 512x512 RGBA
  with levels 0-9, LINEAR_MIPMAP_LINEAR and REPEAT, and every upload
  carries exactly the bytes its size needs.
- **rol-3d 0.15 (riscos-risc3d 0.15-1):** `ROL3D_REPLAY=texcheck` keeps
  every 2D level uploaded. At the first frame written, it reads each
  level back (glGetTexImage, at the upload's alignment) and logs any
  difference or size mismatch. It then writes `-cardmip` (every texture
  in a 48-pixel grid, sampled as the program set it, with lighting, fog
  and so on off) and `-cardbase` (level 0 only).

  Host: 909 levels the same, 9 differ (all texture 156, TORCS's
  GL_COMPRESSED_RGBA test texture in GL_BYTE, expected). Card right. The
  frame picture is unchanged.
- **Kit 27:** `replay` runs texcheck on the GPU and in software.

## 0.9, continued: rol-3d 0.14 replays with one thing changed at a time; kit 26 (14 Nov 2026)

- **Kit 25 withdrawn** (never run). It replayed with arrays in buffers,
  but the owner had already run TORCS that way in kits 20 and 21, and it
  was just as wrong. So the memory path isn't the cause.
- **rol-3d 0.14 (riscos-risc3d 0.14-1):** `--replay` writes PNG
  (uncompressed, no zlib needed). The owner's PPM uploads had every
  byte above 127 replaced, so the PNGs avoid that.
  `ROL3D_REPLAY=` takes a list of changes:
  - `imm`: glDrawElements from arrays in memory drawn as glBegin/glEnd
    (unit 0, float arrays).
  - `nopush`: glPush/PopClientAttrib's array enables kept by rol-3d.
  - `clamp`: GL_CLAMP as GL_CLAMP_TO_EDGE.
  - `sized`: internal formats 1-4 and GL_RGB(A)/GL_COMPRESSED_RGB(A) as
    sized formats.
  - `noalpha`: alpha test never enabled.
  - `nomip`, `nohint`: as 0.13.

  Host (llvmpipe): the unchanged replay's PNG is identical to 0.13's
  picture. `imm` is identical too (500,401 draws done as glBegin/glEnd,
  none left). `all` differs only where alpha test matters.
- **Kit 26:** `replay` writes ReplayGPU, Imm, NoPush, Clamp, Sized,
  NoAlpha, All and Soft.

## 0.9, continued: kit 25 tests arrays in buffers on the GPU (14 Nov 2026; withdrawn, see above)

- **Kit 24 on the Pi:** ReplayNoHint-240 is identical to ReplayGPU-240
  byte for byte, so TORCS's GL_FASTEST hint isn't the cause.
  ReplayNoMip-240 has sharper scenery, but the car is still plain yellow
  and the trees are still striped, so mipmaps aren't the main cause either.
- **What the recording holds:** TORCS draws with client arrays (vertex
  3 floats, normal 3 floats, texture coordinates 2 floats, stride 8),
  about 500,000 glDrawElements calls each inside glPushClientAttrib /
  glPopClientAttrib. The picture looks like texture coordinates read
  wrongly on V3D. Since 0.9, rol-3d gives these arrays to Mesa as memory.
  llvmpipe takes them directly, but V3D has Mesa copy them into buffers
  (u_vbuf), so this is where the two drivers differ.
- **Kit 25:** riscos-risc3d 0.13-1 unchanged. `replay` writes ReplayGPU,
  ReplayBuffers (/riscos/3d/uploads = buffers, as 0.8),
  ReplayBufNoMip and ReplaySoft.

## 0.9, continued: TORCS drawn wrongly only by the Pi's GPU; kit 24 (14 Nov 2026)

- **TORCS recorded on the Pi (ctx-477.r3d, 3,462,636 commands, 250
  frames, 335 MB):** replayed on the build machine (llvmpipe), frame 240
  is right: the car's livery, number and cockpit. Replayed on the Pi in
  software (ROL3D_SOFTWARE + LIBGL_ALWAYS_SOFTWARE): the same picture (22
  pixels differ). Replayed on the Pi's GPU (the owner's PNG of it): the plain yellow car
  and smeared scenery, as TORCS showed. (The PPMs as uploaded here were
  altered in transit, so looked garbled; the owner's own PNG is right.)
  So the commands
  RISC3D passes on are right, and the Pi's GPU driver (V3D, Mesa 25.0.7)
  draws something in them differently.
- **rol-3d 0.13:** replay reads back every frame (as the program's
  frames were; before, only the saved ones, so V3D could gather many
  frames into one job). It can also
  leave out mipmaps (`ROL3D_REPLAY_NOMIP`: mipmapped min filters become
  GL_LINEAR) or glHint (`ROL3D_REPLAY_NOHINT`: TORCS sets
  GL_PERSPECTIVE_CORRECTION_HINT to GL_FASTEST), to find what V3D gets
  wrong. Host: replay unchanged byte for byte.
- **Kit 24:** riscos-risc3d 0.13-1; `replay` writes ReplayGPU, ReplayNoMip,
  ReplayNoHint and ReplaySoft.

## 0.9, continued: kit 23 records TORCS (14 Nov 2026)

- **Kit 22 on the Pi (14 Nov):** riscos-mesa's texture-load and
  multitexture tests pass on the GPU (all builds), and TORCS's own trace
  shows every texture uploaded with all levels and no error. rol-3d's
  log: 208 textures in all contexts, every mipmapped one complete, all on
  unit 0. So the yellow car is not textures arriving incomplete.
- **rol-3d 0.12 (riscos-risc3d 0.12-1):** `/riscos/3d/rec/` (a directory
  anyone can write to) makes each new context record every command it
  gets (opcode, count, argument words; data that came ahead included) to
  `ctx-<pid>.r3d`, up to `/riscos/3d/rec/frames` frames (default 200) or
  1 GB; flushed at each frame and glFinish. `rol-3d --replay file prefix
  [frames|/n]` draws a recording again (a context in memory, no display)
  and writes frames as PPM. Host: gl1test multi recorded and replayed =
  its own picture byte for byte.
- **Kit 23:** `record on|off|list` and `replay` scripts (the GPU, then
  LIBGL_ALWAYS_SOFTWARE).

## 0.9, continued: exit fix, kit 22 logs TORCS's textures (14 Nov 2026)

- **Kit 21 on the Pi (14 Nov):** TexResults = TexLinuxResults exactly.
  TORCS's check log (60 frames): no GL errors, and no glActiveTexture or
  glClientActiveTexture. I told riscos-mesa those calls were being lost;
  they checked (`MESA/handoffs/2026-11-14-mesa-risc3d-arb-names-ok-exit-abort.md`)
  and the ARB names do reach RISC3D (their multitex test under QEMU).
  TORCS on RISC OS runs with one texture unit unless -M, so the missing
  calls are probably just TORCS not making them. Not the cause, then.
- **Fixed: the abort at exit** riscos-mesa found. `close_all` (the
  client's atexit handler) closed each context but left its handle and
  `cur`; freeglut's own handler, run after it, released its context and
  flushed into the unmapped ring (r3d_put_slow -> publish). Now a closed
  context's handle is -1 and `cur` goes back to none, so later calls are
  dropped; kick, publish and the debug delivery also check. This is
  probably TORCS's abort when leaving the race too. Host: a program
  whose earlier atexit handler draws after close_all ends cleanly.
- **rol-3d 0.11 (riscos-risc3d 0.11-1):** with `/riscos/3d/check`, each
  glTexImage2D (unit, texture, level, internal format, size, format,
  type) and min filter / base / max level / generate-mipmap parameter is
  logged (at most 4,000 lines), to see which textures are incomplete.
- **Kit 22:** `RISC3DTest-22.zip` with riscos-risc3d 0.11-1.

## 0.9, continued: kit 21, rol-3d checks what TORCS sends (14 Nov 2026)

- **Kit 20 on the Pi (14 Nov):** Track's parts through RISC3D = Linux's
  own for untextured strips (90365d90) and lines (f4f9c53d); textured
  strips differ (520c3f2d vs 3dee8687), but only in 18 of 76,800 pixels
  by up to 42 (texture filtering at edges): the same in all three ways
  (0.8's library, buffers, memory). Track is drawn right. Memory way:
  18.9 fps (Linux 31.4). So TORCS's missing details and textures come
  from something Track doesn't do.
- **rol-3d 0.10 (riscos-risc3d 0.10-1):** `/riscos/3d/check` makes each
  new context log GL's errors after each command (not between glBegin
  and glEnd; at most 400), the functions Linux's GL hasn't got, and how
  many of each command came (at frame 60, then every 600), to the log
  only. The program's glGetError then sees no errors.
- **The owner's screenshots (14 Nov):** through Linux3D the car is plain
  yellow (no livery, no cockpit) and the scenery's textures are smeared;
  in software all is right. That looks like textures not applied
  (incomplete, or the wrong texture coordinates).
- **gl1test multi:** TORCS's grVtxTable multitexture drawing (the car in
  immediate mode with glMultiTexCoord2fv for units 0, 2, 1 and texture
  matrices on units 1 and 2; the track's arrays for units 0-2 under
  glPushClientAttrib with glDrawElements strips; unit 0 alone after).
  **gl1test bigtex:** 1024/512/256 RGBA and RGB textures with every
  mipmap level given, drawn at four sizes. Both = Mesa directly on the
  build machine.
- **Kit 21:** `RISC3DTest-21.zip`, with the `check` script and a Textures
  test (TexResults vs TexLinuxResults).

## 0.9, continued: kit 20 to find what TORCS draws wrongly (14 Nov 2026)

- **Kit 19 on the Pi (14 Nov):** Track 2.4 fps with 0.7's way (buffers),
  18.9 with 0.8's (memory), Linux directly 26.4; 0.8's library 8.6.
  TORCS 10-13 fps through RISC3D (software 7-10), but drawn wrongly:
  details missing and textures not applied (riscos-mesa's software draws
  it right). Track's picture also differs from Linux's own (5f2b9680
  against 4b0dddda) in all three ways, so not only the new path.
- **rol-3d 0.9 (riscos-risc3d 0.9-1):** `/riscos/3d/uploads` saying
  "buffers" goes back to 0.7's way, read by each new context, so a real
  program can be compared both ways without a restart. An update's
  postinst says not to reload the modules (unchanged); reloading RISC3D
  alone had left Linux3D unregistered on the Pi.
- **gl1test track** writes a hash for each part (strips untextured,
  strips textured, lines from a first vertex) and saves the picture
  (TrackPic8/B/M, TrackLinuxPic).
- **Kit 20:** `RISC3DTest-20.zip` with riscos-risc3d 0.9-1, the `uploads`
  script; `watch` dropped (the loading was UnixLib's).

## 0.9: arrays from a program's memory sent once, not once a strip; kit 19 (13 Nov 2026, in testing)

The ROL conversation reported (`RISC3D/handoffs/2026-11-13-rol-torcs-slow-through-risc3d-pi4.md`)
that TORCS on the owner's Pi 4 ran at 7.9 fps through RISC3D, at 100% of
the processor and slower than in software, and took 460 s to load the
track, slower and slower as it went.

- **The race: every strip sent its object's arrays again.** TORCS (plib's
  ssg, `grVtxTable::draw_geometry_array`) draws each object as many
  triangle strips with glDrawElements, indices in its memory, all from
  one set of arrays in its memory. The library sent each draw all the
  arrays from vertex 0 up to its highest index, so an object of S strips
  sent about S/2 times its arrays. With gl1test's track (60 objects of 39
  strips of 2400 vertices), that's 94 MB a frame.
  - **The library (0.9)** now sends only the vertices from the lowest to
    the highest index. The indices are moved down to match (in the copy
    it sends). glDrawArrays and glDrawArraysInstanced from a first vertex
    are moved to vertex 0 the same way.
  - It does this only when every enabled array is in the program's
    memory (none in a buffer object), no shader is in use (gl_VertexID
    would change: glUseProgram and glBindProgramPipeline are now
    tracked), and no primitive restart at a chosen index. Arrays with a
    divisor are sent as before.
  - The build machine (llvmpipe): track 16.0 fps before, 30.7 after
    (Mesa directly 21.9). Pictures identical.
- **rol-3d 0.8 (riscos-risc3d 0.8-1):** arrays and indices from the
  program's memory are given to Linux's Mesa as memory, as a Linux
  program gives them. Mesa streams them into one buffer of its own. 0.7
  made a fresh buffer object (glBufferData) for every array of every
  draw.
  - It doesn't do this with a vertex array object bound, in a context
    without the compatibility profile, or for GLES 1.1's point size array
    (es1.c reads that from a buffer).
  - New own op OP_UPLOADS (0x7F04) and `r3d_uploads(0|1)` choose the way,
    for comparing. ROL3D_LEVEL 7.
  - On llvmpipe the two ways are about equal; the Pi's driver is what
    matters (kit 19 measures both).
- **The proxy query** in the report ("proxy width 0" for 2048x512) is
  TORCS's own: grtexture.cpp sets the width to 0 without asking when a
  size is over its own limit (512). Not RISC3D's.
- **Loading:** TORCS's loader makes few GL calls (a proxy query, a
  glTexImage2D a mipmap level, the RISC OS port's two
  glGetTexParameteriv). gl1test track's texture part does the same 200
  times: no slowing on the build machine. Kit 19's `watch` script logs
  Linux's memory, swap and busiest processes every 5 s while TORCS loads.
- **gl1test track:** the TORCS pattern, plus a row of each object drawn
  with glDrawArrays from a first vertex. It runs the frames with 0.8's
  library (`gl1test08`, built from tag v0.8), then 0.9's both ways. On
  the build machine every combination of library 0.8/0.9 and helper
  0.7/0.8 gives the same answers and pictures in estest, gl3test,
  targettest and gl1test.
- **Kit 19:** `RISC3DTest-19.zip` with riscos-risc3d 0.8-1. Adds Track,
  `watch`, and the track in linux-tests (TrackLinuxResults).

## 0.8: RISC3D_Info says when Linux draws in software; kit 18 (13 Nov 2026)

- **Kit 18 on the owner's Pi 4 (13 Nov):** InfoResults says "driver flags:
  0", r3d_available 1, and *RISC3D "Driver: Linux3D (Linux's graphics
  driver), drawing on a GPU". A first run without the ", drawing ..." was
  before the new modules were loaded. Released: tag v0.8,
  `RISC3D-client-0.8.tgz`, riscos-risc3d 0.7-1 (the kit's).


riscos-mesa asked (`MESA/handoffs/2026-11-13-mesa-risc3d-0.7-osmesa-routed.md`).
In QEMU, and on any Linux machine without a GPU driver, RISC3D's
renderer is Linux's Mesa in software. Through RISC3D that is far slower
than riscos-mesa's own renderer, because every frame is copied back:
osmesatest's lit cube ran at 23.8 fps against 500.

- **rol-3d 0.7:** a second abstract socket, "rol-3d-info", answers with
  "GPU <renderer>" or "software <renderer>".
  - It's found once per helper by a child that makes a context as a
    renderer would (as RISC OS's user).
  - It counts as software with no render node, or with llvmpipe,
    softpipe or swrast as the renderer.
  - It's logged as "draws with: ...". The first answer takes about
    0.3 s on the build machine; later ones are immediate.
- **Linux3D 0.02** registers with flags bit 1, "may draw in software:
  ask". Entry reason 3, "describe", reads rol-3d-info and returns 1 for
  software, 0 for GPU, or 2 if it's not known (no helper, or rol-3d older
  than 0.7).
- **RISC3D module 0.03:**
  - RISC3D_Info's r4 bit 1 is set when the driver a new context would
    get says software now. It asks at each Info; r0 is 3.
  - *RISC3D says "drawing in software", "drawing on a GPU" or "not
    known yet".
  - A driver without bit 1 is never asked, so V3DRender needs no change.
- **The library:** `r3d_driver_flags ()` returns RISC3D_Info's flags for
  the driver a new context would get, or -1 if there is none.
  `R3D_DRIVER_SOFTWARE` is 2 and `R3D_DRIVER_DRM` is 1. `r3d_available`
  is unchanged: whether to keep software is riscos-mesa's decision. No
  new C library functions.
- **r3dinfo** (`demo/info`): prints the flags. On the build machine,
  against llvmpipe, it says "draws in software".
- Package riscos-risc3d 0.7-1 (rol-3d 0.7, the two modules); kit 18
  (`RISC3DTest-18.zip`, Info obey).

## 0.7: drawing into the program's own buffer (OSMesa's way); kit 17 (13 Nov 2026)

- **Kit 17 on the owner's Pi 4 (13 Nov):** TargetResults = TargetExpected
  (riscos-mesa's OSMesa) in all 24 lines, with no copy warnings. estest's
  es2 picture is 379eda74 through RISC3D and from Linux's Mesa directly.
  GL3 unchanged from kit 16. Released: tag v0.7, `RISC3D-client-0.7.tgz`
  (riscos-risc3d stays 0.6-1).


The owner: programs should use RISC3D automatically wherever it is
there, programs calling OSMesa directly too. That routing is riscos-mesa's
(its libOSMesa); this is what it needs from RISC3D.

- **`r3d_target (ctx, buffer, width, height, format, row_length, y_up)`**
  makes the program's buffer the context's framebuffer 0, as
  OSMesaMakeCurrent does. The frame is copied into it at glFinish and
  glFlush when anything was sent since the last copy, and at
  `r3d_target_copy`. The renderer keeps its own framebuffer, so nothing is
  lost between copies, as with OSMesa's single buffer.
  - It takes OSMesa's formats and its values: RGBA (no conversion when
    top row first and no row length), BGRA, ARGB, RGB, BGR and RGB_565,
    with a row length and either row order.
  - RGB and BGR follow riscos-mesa's OSMesa (Mesa 20.3.5) as it is:
    OSMESA_RGB is blue, green, red in memory, and OSMESA_BGR is red
    first. 565 is rounded as Mesa rounds.
  - glFinish and glFlush are noted (TRACK) for it. A glFinish with nothing
    new sent copies nothing.
- **targettest** (`demo/target`): the same scene in all six formats, both
  row orders, with and without a row length (24 cases), first after
  glFinish, then redrawn and after glFlush. Built with -DOSMESA it runs
  riscos-mesa's own OSMesa (20.3.5 and its patches, built on the host).
  On the build machine every sum is the same as OSMesa's. Kit 17 carries
  OSMesa's answers as TargetExpected.
- **No new C library functions in the library** (riscos-mesa's request:
  a mixed program exports only what it was linked with). The split for
  glGetStringi no longer uses strtok, and glGetInteger64v no longer needs
  libgcc's __fixdfdi. `nm -u` of the library is 0.4's list again.
- **estest:** GL_ARRAY_BUFFER is unbound before the instancing part's
  arrays in the program's memory. Without that, `v + 4` was an offset
  into the earlier buffer, out of range (found by riscos-mesa). The es2
  picture changes on both sides.
- rol-3d unchanged (0.6), so the package stays riscos-risc3d 0.6-1. Kit 17
  (`RISC3DTest-17.zip`) carries kit 16's copy.

## 0.6: GL_EXT_direct_state_access; kit 16 (13 Nov 2026)

- **Kit 16 on the owner's Pi 4 (13 Nov):** gl3test all through RISC3D =
  Linux's Mesa directly, every line after the versions, all three parts:
  es3 79116525, gl3 320569a2, dsa 310251cb. The Pi's Mesa has
  GL_EXT_direct_state_access with OpenGL 3.1 compatibility. Released:
  tag v0.6, `RISC3D-client-0.6.tgz`, riscos-risc3d 0.6-1 (the kit's).


Step 3 of 3 of full coverage: with it RISC3D carries every function
riscos-mesa's Mesa 20.3.5 offers (`docs/surface/missing-now.txt`: none
missing in GL 2.1, GLES 1.1 or GLES 2.0).

- **gen.py:** GL_EXT_direct_state_access's 255 functions. Those gl.xml or
  the suffix makes another name for a function carried already travel as
  it (glProgramUniform4fEXT as glProgramUniform4f, glEnableIndexedEXT as
  glEnablei...); the rest travel as themselves, not as OpenGL 4.5's
  (glNamedBufferData and the like differ, and a 3.1 renderer may not have
  them). 1189 functions (218 by hand), 484 other names.
- **Sent as they are** (rol-3d calls Mesa's own, compatibility profile):
  texture and unit parameters, environments and generation, storage,
  copies, buffers' parameters and copies, programs (ARB and GLSL),
  renderbuffers, framebuffers, the vertex attribute formats and bindings.
- **Done here as the bound object's function, the object bound for the
  call**, where state is kept here or the program's memory is read:
  - images by texture name (glTextureImage2DEXT...) and their gets:
    OP_TEMP_BIND (texture or buffer, target, name) .. OP_TEMP_END, new
    commands: rol-3d binds it and puts the old binding back after. So
    the unpack and pack settings and pixel buffers work as for
    glTexImage2D.
  - a unit's images and the like (glMultiTexImage2DEXT...): that unit
    made active and back.
  - matrices by stack (glMatrixLoadfEXT...): the kept stacks, through
    glMatrixMode and back (GL_TEXTUREi: that unit's).
  - buffers by name (data, sub data, gets, mapping, flushing, clearing):
    bound as GL_COPY_WRITE_BUFFER for the call; a mapping is kept under
    the buffer's own key.
  - vertex array objects' arrays by name (glVertexArrayVertexOffsetEXT,
    enabling, divisors, pointer gets): the object bound, and the buffer.
  - the client's arrays by unit (glMultiTexCoordPointerEXT,
    glEnableClientStateIndexedEXT...), glClientAttribDefaultEXT (the kept
    state reset too) and glPushClientAttribDefaultEXT.
- **Framebuffer 0 by name** (glFramebufferDrawBufferEXT (0, GL_BACK),
  glGetFramebufferParameterivEXT (0, ...), the attachments): rol-3d's own
  framebuffer, with its buffer names.
- **Arrays' state kept here answered here:** GL_VERTEX_ARRAY_SIZE and the
  other fixed-function arrays' sizes, types and strides (an array in the
  program's memory reaches the renderer only with each draw, so the
  renderer's answer was the last one drawn). The arrays start with
  OpenGL's defaults.
- **Levels:** OP_CONTEXT's reply says what rol-3d takes (word 3: 6 from
  0.6). With rol-3d 0.5 the library offers 0.5's extensions
  (`r3d_exts_gl_05`), with 0.4 0.4's.
- **gl3test dsa** (and `all`, which the kit runs): matrices by name with
  another stack current, a texture by name from the program's memory with
  a row length and read back, a unit's texture and environment, texture
  coordinates in the program's memory by unit, buffers by name with a
  mapped range flushed, a vertex array object's arrays by name and drawn,
  client defaults pushed and popped, framebuffers by name and
  framebuffer 0's draw buffer. On the build machine: every line and the
  picture as Mesa directly. 0.6's library with rol-3d 0.5 says "direct
  state access: no" and the rest unchanged; 0.5's library with rol-3d
  0.6, and 0.6's with 0.4, as before.
- rol-3d 0.6. Kit 16 (`RISC3DTest-16.zip`) with riscos-risc3d 0.6-1.

## 0.5: OpenGL 3.0/3.1 and GLES 3.0/3.1; kit 15 (13 Nov 2026)

- **Kit 15 on the owner's Pi 4 (13 Nov):** gl3test through RISC3D = Linux's
  Mesa directly, every line after the versions: es3 picture 79116525, gl3
  picture 320569a2 both ways. The Pi gives OpenGL ES 3.1 and OpenGL 3.1
  with the compatibility profile (Mesa 25.0.7, V3D 4.2.14.0); compute,
  transform feedback, sync objects, indirect draws on the GPU. estest's
  GLES 2 part on Linux unchanged (f6971244); ES, Cube, Test, GL and
  Overlay on RISC OS as with kit 13 (the owner). Released: tag v0.5,
  `RISC3D-client-0.5.tgz`, riscos-risc3d 0.5-1 (the kit's).


Step 2 of 3 of full coverage (`claude/risc3d-full-coverage-plan.md`).

- **gen.py:** OpenGL 3.0 and 3.1's functions, GLES 3.0 and 3.1's, and
  those of every extension in riscos-mesa's GL 2.1 list but
  EXT_direct_state_access (step 3), after the earlier ones (opcodes
  unchanged). 989 functions (134 by hand), 467 other names.
  - A new way to travel, "mixed": arrays in and out together, answers
    of any length (names, logs, resource queries). Strings arrays go as
    a count then each string (`r3d_strs`, `unpack_strs`); GLintptr and
    GLsizeiptr arrays as words, widened at the other end; 64-bit
    arguments as two words. Lengths gl.xml gives as COMPSIZE of
    something other than pname are in its `LEN` table; a GLchar name's
    is its length.
  - Parameters named like the generated code's own variables (`a`,
    `r`...) get a `_`.
  - r3d_exts.h also has 0.4's lists (`r3d_exts_gl_04`,
    `r3d_exts_gles2_04`): the library offers those when rol-3d is older
    than 0.5, whose functions stop there.
- **Versions.** OP_CONTEXT carries the version asked for (word 1);
  rol-3d answers with its context's (word 2). The library gives the lower
  of that and what the stream carries: OpenGL up to 3.1, GLES up to 3.1
  (2.0 where GLES runs on desktop OpenGL), GLES 1.1. GL_VERSION, the GLSL
  version (1.40, "OpenGL ES GLSL ES 3.10"...), GL_MAJOR_VERSION and
  GL_MINOR_VERSION follow it. `r3d_create_version (api, major, minor)`
  fails, with the renderer's version in r3d_error, if that is lower.
- **OpenGL 3.1 without the compatibility profile,** for drivers whose
  compatibility profile is older than what is asked for. rol-3d makes the compatibility context first,
  and a 3.1 one (EGL_CONTEXT_MAJOR/MINOR_VERSION) if that is too old for
  what was asked. In such a context:
  - glGenBuffers and glGenTextures ask the renderer for its names (one
    wait), as it takes no others; rol-3d's own buffers and its frame's
    texture are names it was given too (`priv_name`).
  - glGetString (GL_EXTENSIONS) gives none (as Mesa); rol-3d joins
    glGetStringi's for the library's own use.
- **glGetStringi, GL_NUM_EXTENSIONS:** the library's own list (the
  carried ones), split.
- **Sync objects:** a number in the program, rol-3d's GLsync in a table
  (a RISC OS address can't hold Linux's pointer). glFenceSync needs no
  wait. glObjectPtrLabel/glGetObjectPtrLabel label them (glObjectLabel's
  command, identifier R3D_SYNC_LABEL).
- **Arrays in the program's memory:** glVertexAttribIPointer (integer
  attributes: OP_CLIENT_ARRAY's word 3 bit 1), glVertexAttribDivisor
  (an instanced array has one value per that many instances: instanced
  draws send that many), primitive restart (its index isn't counted when
  finding the last vertex: glEnable/glDisable and glPrimitiveRestartIndex
  are noted). Indices in a buffer object are read back on GLES too
  (OP_READ_BUFFER_RANGE), so arrays in the program's memory are sent
  there as well.
- **Indirect draws (GLES 3.1):** from a buffer object, by rol-3d; with
  arrays in the program's memory, the command is read (back) and drawn
  as the direct call. GL_DRAW_INDIRECT_BUFFER_BINDING kept.
- **glGetInteger64v:** the kept state as glGetIntegerv's, the rest asked.
  Indexed gets (glGetIntegeri_v...) generated.
- **Pixel formats:** GL_RG, the _INTEGER formats, 10F_11F_11F, 5_9_9_9,
  FLOAT_32_UNSIGNED_INT_24_8_REV.
- **glInvalidateFramebuffer/glInvalidateSubFramebuffer** on the
  program's framebuffer 0 (ours, with other attachment names): not
  passed on (a hint).
- **In the library only:** the robustness extension's sized gets
  (glReadnPixels... as the unsized ones), EXT_vertex_array's pointer
  functions, IBM_multimode_draw_arrays, glGetProgramStringARB (its
  length asked first).
- **gl3test** (`demo/gl3`): GLES 3.0/3.1 (uniform block queries and
  drawing, transform feedback captured and read back, instanced arrays and
  integer attributes from the program's memory, primitive restart, sync
  objects with a label, a sampler object and an RG texture, indirect
  draws, a compute shader with a storage buffer and its resource queries,
  a blit) and OpenGL 3.0/3.1 (fixed function where there is the
  compatibility profile, GLSL 1.40 with a texture buffer, an output's
  location, an unsigned uniform, primitive restart with the program's
  index, per-buffer masks, conditional rendering, a fence). On the build
  machine: every line as Mesa directly, both pictures; also with rol-3d
  forced to 3.1 without the compatibility profile (as the Pi), against Mesa
  directly with a 3.1 context.
- **Old and new together (build machine):** 0.4's library with rol-3d
  0.5, and 0.5's library with rol-3d 0.4: r3dtest, gl1test and estest as
  0.4's, pictures included (0.5's library with 0.4's helper offers 0.4's
  extensions and versions).
- rol-3d 0.5. Kit 15 (`RISC3DTest-15.zip`) with riscos-risc3d 0.5-1:
  GL3 (gl3test both) and linux-tests running gl3test-linux. (Kit 14 was
  the same with a wrong line in its !ReadMe: that the Pi's Mesa gives
  3.1 only without the compatibility profile. Kit 2 showed 3.1 with it.)

## 0.4: OpenGL ES 1.1, and GLES 2's extensions; kit 13 (12-13 Nov 2026)

- **Kit 13 on the owner's Pi 4 (13 Nov):** GLES 2 part through RISC3D =
  Linux's Mesa directly, every line, picture f6971244 both. GLES 1.1: every
  number as riscos-mesa's renderer gives on the host (matrices in fixed
  point, glQueryMatrixxOES, light, texture environment, clip plane), the
  ETC1 texture, the point size array and vertex array objects without
  error; 40 extensions. The debug callback brings the program's message
  on the Pi. Released: tag v0.4, `RISC3D-client-0.4.tgz`, riscos-risc3d
  0.4-1 (the kit's).


The owner: everything riscos-mesa supports, and all the hardware can do.
Step 1 of 3 (`claude/risc3d-full-coverage-plan.md` in the project).

- **What riscos-mesa supports, measured:** its Mesa 20.3.5 with all 31 of
  its patches, built on the host: GL 2.1 (165 extensions), GLES 1.1 (44),
  GLES 2.0 (56). The lists are in `docs/surface/`; `tools/surface.py` says
  what RISC3D lacks.
- **gen.py:** GLES 1.1's functions, and those of every extension in
  riscos-mesa's GLES 1.1 and 2.0 lists, after the earlier ones (their
  opcodes unchanged). A function an extension promoted travels as the
  core one, the extension's name another name for it (gl.xml's alias, or
  the name without the vendor's suffix and the same parameters). 704
  functions, 400 other names. r3d_gl.h has the other names' prototypes
  and the types they need (GLfixed, GLsync, GLDEBUGPROC...).
  GL_EXTENSIONS for GLES 1.1: riscos-mesa's own list, those carried.
- **GLES 1.1** (`r3d_create_es1`, `r3d_open_es1`; OP_CONTEXT 2). Debian's
  Mesa has no GLES 1 contexts (`-Dgles1=disabled`; since Mesa 23.1 that
  leaves none), so rol-3d makes a desktop compatibility context and:
  - `linux/es1_conversion.c`: Mesa 20.3.5's own es1_conversion.c (the
    fixed-point and float functions in desktop terms, riscos-mesa's exact
    rules), made by `tools/port-es1.py`, in place of the driver's.
  - `linux/es1.c`: GL_FIXED arrays in buffer objects made floats before
    each draw; the point size array (OES_point_size_array) drawn a point
    at a time; ETC1 textures decoded (Mesa's own decoder's rules: the
    same pixels as riscos-mesa, even for odd blocks).
  - The library keeps GLES 1.1's matrices as OpenGL's (glOrthof,
    glFrustumf, the x functions), answers glGetFixedv (Mesa's
    FLOAT_TO_FIXED, cut) and glQueryMatrixxOES itself, and sends GL_FIXED
    arrays from the program's memory as floats.
- **Extensions' hand-written functions:**
  - vertex array objects: the library keeps each one's arrays (its half
    of their state) and switches them at glBindVertexArray;
  - glMapBufferRange and glFlushMappedBufferRange (read through
    OP_READ_BUFFER_RANGE; written at flush or unmap);
  - draws with instances or a base vertex, arrays and indices in the
    program's memory or in buffers (OP_DRAW_ELEMENTS_EX);
  - glCreateShaderProgramv, glGetProgramPipelineInfoLog,
    glGetProgramBinary;
  - KHR_debug: labels, groups, inserted messages, the log, and the
    callback: rol-3d keeps the driver's messages and the library gives
    them to the program's function at its next wait (`debug_pending` in
    the shared area). Labels on sync objects wait for sync objects (step 2).
- `r3d_available` reads RISC3D$Disable with OS_ReadVarVal, not getenv
  (riscos-mesa's request: UnixLib's getenv of a $ name remakes a buffer).
- **Host tests:** demo/es/estest finds every function by name, as
  riscos-mesa's EGL does. GLES 2 part: through RISC3D = Mesa directly,
  every line and the picture. GLES 1.1 part against riscos-mesa's own
  renderer (OSMesa 20.3.5 with its patches, built here): every number the
  same (matrices in fixed point, light, texture environment, clip plane),
  the lit colour the same with GL_NORMALIZE (riscos-mesa lights in
  object space when it can), the ETC1 texture the same pixel for pixel.
  Two things riscos-mesa does wrong there, not RISC3D: its GLES 1.1
  vertex array objects give GL_INVALID_OPERATION, and drawing with a
  program pipeline (EXT_separate_shader_objects) crashes it (run_vp).
  r3dtest and gl1test unchanged.
- rol-3d 0.4. **riscos-risc3d 0.4-1** (the kit carries it). Kit 13: ES,
  linux-tests; the kits' obey files expect the package's modules.

## 0.3: a package for RISC OS on Linux (12 Nov 2026)

- **riscos-risc3d 0.3-1** (`tools/mkdeb.sh`, `packaging/riscos-risc3d/`,
  into out/): `/opt/riscos/rol-3d`, the modules in
  `HardDisc4.TestBed.RISC3D`, and a start-up file
  `!Boot.Choices.Boot.PreDesk.RISC3D` made once (each line guarded with
  IfThere, so removing the package leaves no boot error). Depends on
  riscos-support and riscos-harddisc; Debian's Mesa recommended (as agreed
  with ROL). postinst starts the new rol-3d at once; init starts it at
  start-up from riscos-support 1.0.0049-1, and postinst says so if the
  installed init doesn't. Purge removes the start-up file and the log.
- rol-3d 0.3, for being started by init (before rol-fb, output to
  /dev/null), as ROL asked:
  - drops master on any display card it opens (else rol-fb, started
    after it, couldn't set a mode);
  - looks for the display card at the first connection, not at start-up;
  - with its output on /dev/null, logs to /riscos/3d/log.txt (the one
    before kept as log.prev.txt);
  - says its version ("rol-3d 0.3: started"; `--check`).
- Client library and modules unchanged (0.2's; RISC3D 0.02, Linux3D 0.01).

## 0.2 (12 Nov 2026)

- Release of the client library for riscos-mesa (`RISC3D-client-0.2.tgz`):
  zero-copy frames (`r3d_overlay`, `r3d_swap_overlay`), RISC3D's own
  version strings, GL_EXTENSIONS trimmed to the stream (new generated
  file `gen/r3d_exts.h`). Needs the rol-3d of this tag for zero-copy; an
  older one answers "no overlay" and frames are read back.
- Kit 12 on the Pi 4, the rest: a menu over an overlay, dragging the
  window, and a mode change while running all behaved (the owner: "works
  fine").
- Modules unchanged: RISC3D 0.02, Linux3D 0.01.

## Kit 12 on the Pi 4 (12 Nov 2026)

- **Zero-copy works:** Overlay at 59.0 frames a second (the display's 60
  Hz, 1920 x 1200, 16M colours), the right way up, the right colours, in
  place. ROL's PlaneTest rol-fb; dumb buffers from card0 (vc4) imported
  by v3d. helper-log: "overlay open (formats 15, under the desktop 1,
  screen 1920x1200)" for each, no failures.
- Three Overlays at once, all zero-copy: 19.2, 19.2, 19.4 frames a
  second: rol-fb sets one plane per display frame, so they share the 60.
  Asked of ROL: all planes in one atomic commit per frame.
- Read-back for comparison: Cube 196 frames a second alone, 117 each for
  two.
- Version strings on the Pi: "OpenGL ES 2.0 RISC3D (Mesa
  25.0.7-2+deb13u1)" / GLSL ES 1.00; "2.1 RISC3D (...)" / GLSL 1.20, 130
  extensions. Test ok (145.6 / 147.1); GL's picture 04ecca57 as kit 11,
  bench 240 / 250 as kit 11.

## Zero-copy frames and RISC3D's own version strings; kit 12 (12 Nov 2026)

- **Version strings (riscos-mesa's request):** `glGetString` answers for
  what the stream carries, not what Linux made. OpenGL: "2.1 RISC3D
  (Mesa x.y.z)", GLSL "1.20"; GLES 2: "OpenGL ES 2.0 RISC3D (Mesa
  x.y.z)", "OpenGL ES GLSL ES 1.00". The renderer's own version stays in
  the brackets.
- **GL_EXTENSIONS trimmed** to the extensions whose functions the stream
  carries (by name or another name), from gl.xml (`r3d_exts.h`, made by
  gen.py). Extensions with no functions (formats, tokens, shader
  features) are the renderer's to say. GL_OES_EGL_image(_external) is
  kept, as riscos-mesa's EGL gives its function. On the build machine:
  181 of llvmpipe's 310.
- **Zero-copy frames, through rol-fb's overlays** (RISC OS on Linux's
  test build: several overlays, RGB buffers, SHOW answered at the
  display's frame):
  - `r3d_overlay (x, y, w, h, &key)` places the frame on the screen (the
    window's visible area, RISC OS pixels from the top left; width 0
    hides it). 1: frames go to the screen, and the program paints its
    work area in the key colour (&080008); 0: stay with read-back.
  - `r3d_swap_overlay ()` ends a frame. rol-3d copies it with the GPU
    (upside down: GL's rows are bottom first) into one of two dumb
    buffers of the display's card, shared with the GPU as dma-bufs, and
    shows it. The program waits only for the frame before (the
    display's rate). -1: the overlay stopped (a mode change): back to
    read-back.
  - New commands OP_OVERLAY (13) and OP_PRESENT_OVERLAY (14);
    `overlay_error` in the shared area (from the spare words). An older
    rol-3d answers nothing to OP_OVERLAY: no overlay.
  - rol-3d's main process now stays root, to open the display's card for
    each connection (so each context's buffers go with it); each
    context's process still runs as uid 1000 with the render group.
    `rol-3d --check` says which card zero-copy uses.
  - Not tried on a Pi yet: rol-fb's plane side itself is unproven
    (ROL's PlaneTest kit).
- r3dcube `-overlay` (the kit's Overlay) uses it, and says in its title
  which way frames go. gl1test prints the version, GLSL and the number
  of extensions.
- Kit 12.

## Kit 11 on the Pi 4 (12 Nov 2026)

- The leaked contexts were the cause: after a restart, kit 11's GL ran to
  the end. Picture identical to Linux's (04ecca57, the files byte for
  byte), every answer the same.
- Overlapped frames: each frame came back a frame later ("ok"). The
  bench 250.0 frames a second overlapped against 240.0 with r3d_swap
  (Linux directly 341.1); the calls alone 16.4 million a second
  overlapped, more than Linux's own 11.1.
- What limits a frame now is the renderer reading it back (about 2 ms a
  frame on the Pi: the GPU finishing, the read from the GPU's memory,
  the copies): overlapping hides the program's wait but not that work.
  Zero-copy presentation (handed to the RISC OS on Linux conversation)
  is what removes it.

## Contexts closed when a program ends (12 Nov 2026)

- Kits 9 and 10 on the Pi 4: GL stopped after its first bench, at the
  overlapped-frames check, and `*RISC3D` showed 8 contexts still open,
  task &00000000, each with 36 MB mapped (from &F547C000 down to
  &E5875000) and a Linux helper process. `gl1test bench` ended without
  closing its context, and a program run from an Obey file isn't a Wimp
  task, so RISC3D's Service_WimpCloseDown never closed them. Each run
  left one more, mapped lower; the stop is probably the address space,
  not the overlapped frames themselves.
- The library now keeps a list of its open contexts and closes them
  with atexit, so they go however a program ends. gl1test closes its
  context after the bench, and its output is a line at a time (with a
  line per overlapped frame), so a stop part way shows where.
- Kit 11 (`out/RISC3DTest-11.zip`). Modules unchanged from kit 10.

## 0.02: drivers of a second kind, for the native GPU driver (12 Nov 2026)

- RISC3D 0.02. `RISC3D_Register` takes flags in r4: bit 0 = a DRM
  device, with its ioctl SWI in r5 and its DRM driver name in r6 ("v3d").
  `RISC3D_Info` returns the chosen driver's r4-r6; `RISC3D_Open` returns
  a DRM device's file in r1 (r2 = 0). Command-stream drivers (Linux3D)
  pass r4 = 0 and work as before.
- Why: on native RISC OS (the Pi 4) the driver is V3DRender, which
  follows Linux's v3d kernel driver. The program's own Mesa runs Mesa's
  v3d GPU driver and sends its requests straight to V3DRender, so there
  is no command stream and no second process to switch to.
- `r3d_available` is 0 when the driver is a DRM device (the command
  stream is for command-stream drivers).
- From the V3DRender conversation's patch (its handoff
  `handoffs/2026-11-12-v3drender-drm-device-kind.md`), made on c3181c0
  and applied on 134b17b (v0.1); only this changelog needed merging.
- Kit 10 (`out/RISC3DTest-10.zip`): kit 9 with the RISC3D module 0.02.

## Overlapped frames (12 Nov 2026)

- `r3d_swap_overlapped (pixels, bytes)`: the renderer finishes and reads
  back this frame into one half of the reply area (OP_PRESENT_SLOT, and
  `frame_done` in the shared area, from a spare word) while the program
  goes on with the next; what comes back is the frame before (the first
  time, this one). The program no longer waits for the GPU at the end of
  a frame; a frame of latency. Frames up to half the reply area (16 MB,
  2048 x 2048); larger ones fall back to r3d_swap. Not to be mixed with
  r3d_swap in one context.
- r3dcube uses it. `gl1test bench` checks that each frame comes back a
  frame later, then runs the bench both ways.
- Build machine: the bench 466 frames a second overlapped, 316 with
  r3d_swap, 276 for Mesa directly.
- Kit 9 (`out/RISC3DTest-9.zip`). Modules unchanged.

## Kit 8 on the Pi 4 (12 Nov 2026)

- Picture identical to Linux's (04ecca57, the files byte for byte), every
  answer the same, including the long triangle list and strip.
- Immediate-mode bench 230.8 frames a second (115 with kit 2, 171 with
  kit 7), against 345.3 for Linux's Mesa directly: two thirds of it. The
  renderer worked 0.23 s of the 0.26, 0.12 s of that reading frames
  back: what is left is a frame's end (waiting for the GPU, the copy
  back, the copy into the program and the round trip: an empty frame
  1.50 ms against 1.01 directly), which zero-copy presentation and
  overlapping frames address, not the calls.
- "The calls alone" (60 frames' calls, no frame between) is no longer a
  fair measure: with nothing presented the GPU's queue grows and the
  renderer waits on it (4.0 million calls a second; issued in 0.40 s).

## Collected straight into the ring (12 Nov 2026)

- Kit 7 on the Pi 4: picture and answers the same as Linux's (files byte
  for byte). The processor loop: 0.56 s as a RISC OS program, 0.39 s as
  a Linux one (1.44 times: 32-bit code, VFP and no NEON; not a fault).
  The bench 171.4 frames a second (from 117.6), the renderer working
  0.21 s of 0.35 and woken 998 times; the calls alone took 0.33 s to
  issue, about 180 ns a call: the library copied every collected vertex
  twice (into its own buffer, then into the ring; 3.6 MB a frame).
- Now the vertices are written straight into the ring: the first vertex
  makes room for a draw (OP_IMMEDIATE) of up to 1024 vertices (whole
  primitives), filled in place and finished when full or at glEnd. A
  long strip, fan, loop or polygon (which can't be cut) moves to the
  library's own memory when the room is full. The usual case of each
  collected call is inlined into it (kind, unit and size known there).
- The renderer stays awake 0.2 ms (by the clock) after the last batch
  before sleeping, so the program seldom has to wake it; and the program
  marks in the shared memory (`asking`, from a spare word: the layout is
  unchanged) when it is about to wait for an answer, so an awake
  renderer goes to the socket at once.
- gl1test: a 3000-vertex triangle list (cut into draws) and a 3000-vertex
  strip (gathered whole). Build machine: picture identical to Mesa
  directly, answers the same; bench 347-448 frames a second through
  RISC3D against 330 directly; empty frames 0.19-0.23 ms.
- Kit 8 (`out/RISC3DTest-8.zip`). Modules unchanged.

## Collected immediate mode in pieces; the processor measured (12 Nov 2026)

- Kit 6 on the Pi 4: picture identical to Linux's (7cc6caa0 both, the
  files byte for byte), every answer the same, including the current
  colour and normal after collected and fallen-back primitives. But the
  bench fell to 117.6 frames a second: the renderer now idle half the
  time (worked 0.27 s of 0.51). Collecting a whole 20000-vertex
  glBegin/glEnd before sending it took away the overlap: the program
  called (about 4.3 ms a frame), then the renderer drew (2.7 ms), then
  read the frame back (1.8 ms), one after another.
- Now a long primitive of points, lines, triangles or quads goes as a
  draw every 1024 vertices (whole primitives), published at once, so
  the renderer draws while the program is still calling. Build machine:
  the bench 470 frames a second through RISC3D against 314 for Mesa
  directly (the two sides on two cores); pictures and answers the same.
- The RISC OS side took about 140 ns a call on the Pi in kits 5 and 6,
  slow for the code (the usual case is a few dozen instructions). `gl1test
  bench` first times a plain processor loop, the same in the RISC OS and
  Linux builds, to tell whether RISC OS programs simply run slower there.
- Kit 7 (`out/RISC3DTest-7.zip`). Modules unchanged.

## Immediate mode collected (12 Nov 2026)

- Kit 5 on the Pi 4: immediate mode 181.8 frames a second (5.5 million
  calls a second), from 142.9; the calls alone 7.2 million against
  Linux's 11.7; the renderer still busy throughout, about 128 ns a
  command against 85 ns a call directly. Pictures identical.
- So immediate mode no longer goes a command a call: between glBegin and
  glEnd the library collects glVertex, glColor (float and unsigned byte),
  glNormal and glTexCoord/glMultiTexCoord (units 0-3) into an array, and
  glEnd sends it as one draw (OP_IMMEDIATE), as Mesa does itself; then
  the current colour, normal and texture coordinates are set to the last
  given. The renderer draws it from a buffer of its own, saving and
  restoring the program's arrays only if any are on or in a buffer
  object. Anything else between glBegin and glEnd (glMaterial, an edge
  flag, other types), an attribute first given after the first vertex,
  a display list being made, or fewer than 8 vertices: what was collected
  goes as the calls themselves, as before. Very long primitives are cut
  into draws where whole primitives allow it.
- gl1test has three more cases: a collected strip and the current
  colour it leaves, a primitive that falls back (a normal first given
  after a vertex), one with glMaterial inside. Build machine: picture
  identical to Mesa directly, every answer the same. The bench's 1.8
  million commands are now 541.
- Kit 6 (`out/RISC3DTest-6.zip`). Modules unchanged.

## The renderer's side cheaper (12 Nov 2026)

- Kit 4 on the Pi 4 said where the time went: the renderer was busy 0.38
  s of the bench's 0.43 (reading frames back 0.11); the calls alone, 5.1
  million a second against Linux's own 11.5, with the renderer busy
  throughout: about 180 ns a command, against 87 ns a call for Mesa
  directly. Empty frames 1.33 ms against 0.91 (the copy-back and the
  round trip: zero-copy presentation's job). Pictures still identical.
- The cause, the same as on the RISC OS side before: the renderer moved
  the shared tail after every command with a full barrier, and read the
  head with one before each. Now it reads the head once per batch
  (acquire) and moves the tail every 16K words and at the end (release);
  the common command goes straight to dispatch, the two buffer
  translations (glDrawBuffer/glReadBuffer) cost a compare.
- Build machine: immediate mode through RISC3D now level with Mesa
  directly (310-340 frames a second against 331; the calls alone 11.0-
  11.7 million a second against 10.2: two cores). Same pictures and
  answers; r3dtest ok both ways.
- Kit 5 (`out/RISC3DTest-5.zip`). Modules unchanged.

## Where the time goes (12 Nov 2026)

- Kit 3 on the Pi 4: immediate mode 142.9 frames a second (4.3 million
  calls a second), from 115; Linux's Mesa directly 342.5 (10.3). The
  cube 183. Pictures still identical to Linux's (GLPicture =
  LinuxPicture), answers unchanged.
- The renderer counts its time (OP_STATS; `r3d_stats ()` in r3d.h, for
  measuring): working through commands, of that reading frames back,
  times it slept, commands. `gl1test bench` prints it, and two more
  lines: empty frames (what a frame costs on its own) and the calls
  alone (60 frames' calls, one frame at the end).
- Build machine: the renderer is busy for the whole run (server-bound),
  about a third more time a call than Mesa directly; empty frames 0.23 ms
  through RISC3D against 0.12 directly.
- Kit 4 (`out/RISC3DTest-4.zip`): that bench. Modules unchanged.

## Kit 2 on the Pi 4, and cheaper calls (12 Nov 2026)

- Kit 2 on the owner's Pi 4 (V3D 4.2, Debian Mesa 25.0.7; desktop
  OpenGL there is 3.1, compatibility profile): gl1test through RISC3D drew
  **the same picture, pixel for pixel,** as Linux's Mesa drawing directly
  (GLPicture = LinuxPicture), and gave the same answers, bar list names
  and one matrix entry outside [-1, 1] read as an integer (C leaves that
  conversion undefined: Mesa itself gives -2147483648 on x86 and
  -2147483645 on the Pi). r3dtest ok at 147.1 and 148.5; r3dcube 179.
- But immediate mode was 2.7 times slower than Linux's own (bench: 115
  frames a second, 3.5 million calls a second, against 306 and 9.2):
  each call read the shared head and tail with full barriers (cheap on
  x86, where the two were level, not on ARM). Now the library keeps the
  head it has published and the tail it last saw, reads the tail only
  when the ring looks full and publishes a batch at a time; the usual
  case of each call is inlined (glVertex2f: about 25 instructions, no
  barrier, no call). Build machine unchanged (same pictures and answers;
  bench 288 frames a second).
- Kit 3 (`out/RISC3DTest-3.zip`): the library and tests rebuilt with it;
  the modules are unchanged (byte for byte).

## Desktop OpenGL (12 Nov 2026)

- **OpenGL 1.0 to 2.1 (the compatibility profile) in the command stream,**
  as well as GLES 2: what riscos-mesa's ports use (riscos-mesa's survey).
  `proto/gen.py` now reads Khronos's registry (`proto/khronos/gl.xml`):
  576 functions (GL 1.0-2.1, GLES 2.0, ARB_framebuffer_object), 78 of
  them by hand at both ends, and 222 ARB/EXT names of the same functions
  in `r3d_gl_functions[]` (glActiveTextureARB, glBindFramebufferEXT and
  so on) for Mesa's EGL to fill its table by name. Arrays a function reads
  travel from gl.xml's `len=` attributes; small fixed ones (glVertex3fv,
  glLoadMatrixf) go straight into the ring. It also writes `r3d_gl.h`
  (types, enums, prototypes). Protocol 2.
- **Contexts say which API:** `r3d_create_gl ()` (and `r3d_open_gl ()`)
  for OpenGL's compatibility profile; `r3d_create ()` stays GLES 2. The
  renderer makes the context when it is created (OP_CONTEXT), so a
  failure is seen at once (for Mesa's EGL to fall back to software).
- **Kept on the RISC OS side, so these queries need no wait:** the
  matrix stacks (modelview, projection, texture per unit; computed as
  Mesa does, in floats), the matrix mode, the viewport, the list base,
  the active texture unit, buffer bindings, pixel store settings and
  stack depths. glGetFloatv/Doublev/Integerv/Booleanv answer them at
  once (integers from a matrix as Mesa gives them). Display lists record
  the state they change and calling one replays that record here, so
  glCallList and a "font" drawn with glListBase and glCallLists keep the
  matrices known (riscos-mesa: Warzone and YSFlight read
  GL_MODELVIEW_MATRIX per object, YSFlight GL_LIST_BASE per string).
- **Object names given here:** glGenTextures, glGenBuffers and glGenLists
  answer at once (names above any the program has used itself); the
  renderer binds them (compatibility profile and GLES both allow it). The
  helper's own textures and buffers moved to names the program can't
  reach.
- **By hand:** fixed-function vertex arrays in the program's memory
  (sent with each draw, as attribute arrays were), glInterleavedArrays,
  glArrayElement (as immediate-mode calls), glDrawRangeElements,
  glMultiDraw*; 1D, 2D and 3D images and glDrawPixels, glBitmap and
  glPolygonStipple under all the unpack settings, and pixel unpack and
  pack buffers; glReadPixels and glGetTexImage laid out under the pack
  settings; glCallLists; evaluators (glMap1/2); feedback and selection
  (glRenderMode copies the renderer's buffer back); glMapBuffer (a copy
  here); glPushClientAttrib; answers whose length only the driver knows
  (glGetUniform*, glGetMap*, glGetPixelMap*).
- rol-3d: the context is made when the program says which API; desktop
  functions from Linux's libOpenGL (or libGL), else eglGetProcAddress;
  a missing function is skipped, not fatal. Framebuffer 0 is still our
  framebuffer object, so glDrawBuffer/glReadBuffer(GL_BACK) mean its
  colour. `rol-3d --check` also says what desktop OpenGL there is.
- New test: `demo/gl1/gl1test` (immediate mode, lighting, a texture
  under unusual unpack settings, display lists, vertex arrays in memory
  and in a buffer object, glInterleavedArrays, glArrayElement, glBitmap,
  glDrawPixels, selection, glMapBuffer, glPushAttrib, and the queries).
  Built with DIRECT it draws with Linux's Mesa directly, to compare.
  On the build machine (Mesa 25.2 llvmpipe): the picture is byte for
  byte the same through RISC3D as drawn directly, and every answer is
  the same (bar list names, which are the implementation's to choose).
  `gl1test bench` (20000 immediate-mode vertices a frame, and 100 reads
  of the modelview matrix): 276 frames a second through RISC3D, 279
  directly. r3dtest still passes both ways (GLES 3.2 context, names given
  here).
- Kit 2 (`out/RISC3DTest.zip`, made by `tools/mkkit.py` with file types):
  adds GL (gl1test, and its bench, into GLResults and GLPicture) and
  gl1test-linux, which the install step runs to draw the same with
  Linux's Mesa directly (LinuxResults); installs libgl1 and libopengl0
  too. The install command fix and Test's redirection are in it.

## The Pi 4 test (12 Nov 2026)

- The kit's ReadMe and install script said to run `install,fff`. Copied
  by RISC OS onto HardDisc4 the file is plain `install` to Linux (the
  file type isn't part of its Linux name), so the command is
  `sh /riscos/HardDisc4/RISC3DTest/install` (found by the owner).
- On the owner's Pi 4 with riscos-kernel-pi 6.6.78-9 and Debian's Mesa
  25.0.7, `rol-3d --check` says: GPU; V3D 4.2.14.0; OpenGL ES 3.1.
- First run on a real GPU (the owner's Pi 4, 16 million colours, Debian
  Mesa 25.0.7, V3D 4.2): r3dcube 179 frames a second; r3dtest ok both
  ways, 140.8 a second with arrays from the program's memory and 149.3
  with buffer objects (300 frames each). With r3dcube running beside it:
  r3dtest 145.6 and 147.8, the cube 169, and the cube's context showed
  636 calls, 636 waits, 0 wakes (one wait a frame, as designed).
- The kit's Test wrote Results with *Spool, which also caught the
  screen's control codes (the command window being opened), so Results
  began with a few lines of unreadable bytes. It now redirects each
  command's output into Results ({ > } then { >> }) instead.
- rol-3d: Mesa couldn't make its shader cache ("Failed to create
  /root/.cache", twice per context in the Pi 4's helper-log): after
  giving up root the helper still had root's HOME, so every shader was
  compiled again for every program. HOME is now RISC OS's Linux user's
  home (or /tmp if it has none it can write to).

## After riscos-mesa's reply (12 Nov 2026)

- The forwarding functions are `r3d_glXxx`, listed by GL name in
  `r3d_gl_functions[]` for Mesa's EGL to build a dispatch table from; the
  library defines no `gl*` symbol (a program that links Mesa has Mesa's).
- Contexts: `r3d_create`, `r3d_make_current`, `r3d_destroy`.
  `r3d_available` (a driver is registered and `RISC3D$Disable` isn't
  set). Test programs define `R3D_DIRECT`.
- Tested: build machine, and the kit's Test under QEMU (both runs ok).

## Prototype (12 Nov 2026)

- The owner's design: RISC3D is a driver interface, a pass-through for
  Mesa, with drivers below it (Linux3D on RISC OS on Linux; a native GPU
  driver elsewhere), and as few context switches as possible.
- The command stream and its generator (GLES 2, 142 functions from
  gl2.h), the shared-memory ring, the RISC3D and Linux3D modules, the
  Linux helper rol-3d, the GLES 2 library, r3dtest and r3dcube.
- Tested on the build machine (x86, Mesa 25.2 llvmpipe): r3dtest passes
  with arrays from the program's memory and in buffer objects.
- Tested under QEMU (RISC OS 5.31-28 on Linux, emulated Cortex-A76; Mesa
  26.3 llvmpipe from conda-forge in /opt/riscos/gl as a stand-in for a
  GPU driver, desktop OpenGL as that build has no GLES): r3dtest passes;
  r3dcube shows the cube in a desktop window at about 10 frames a second
  (480 by 360, everything emulated); two copies run at once, each with
  its own context and shared memory.
- Commands are shown to the renderer in batches: shown one at a time, the
  renderer slept between them and was woken about 8 times a frame
  (3,033 wakes in 388 frames). Batched: 688 frames, 688 waits, no wakes:
  one call into RISC3D a frame. r3dcube alone then ran at about 13
  frames a second.
- Not tested: any real GPU.
- **Test kit for a Raspberry Pi 4** (`kit/`: !ReadMe, Cube, Test, and
  `install`, which installs Debian's Mesa on the Linux side, places and
  starts rol-3d and says what will draw). Its Test file ran under QEMU:
  both r3dtest runs ok. The install script's apt-get step could not be
  run here (no Debian mirror). The Pi's GPU needs riscos-kernel-pi
  6.6.78-9 (Linux's v3d driver was not in the kernel before).
