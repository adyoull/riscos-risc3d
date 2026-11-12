# Changelog

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
