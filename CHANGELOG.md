# Changelog

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
