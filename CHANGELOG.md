# Changelog

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
