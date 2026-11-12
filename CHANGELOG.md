# Changelog

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
