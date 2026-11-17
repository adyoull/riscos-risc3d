<img src="art/RISC3D-logo.png" alt="RISC3D" width="480">

# RISC3D (prototype)

A 3D driver interface for RISC OS: Mesa's front end (EGL, GLES, OpenGL)
above it, 3D drivers below it. Programs use Mesa as they do now; where a
driver is registered the drawing is done by the machine's GPU driver, and
where none is, Mesa draws in software as before.

**Release 0.10, 16 November 2026.** It draws on the GPU on native RISC OS
on a Raspberry Pi 4 (through V3DRender) and on RISC OS on Linux (through
Linux3D). ClassiCube, Warzone 2100, YSFlight and TORCS have run on the Pi 4's
GPU. It covers OpenGL 1.x to 3.1 and OpenGL ES 1.1 to 3.1, with
riscos-mesa's extensions. The SWI chunk (&C3D00) and error numbers are still
provisional.

## Installing

RISC3D is the PackMan package **RISC3D** on every system: `!RISC3D` in
`!Boot.Resources`, holding the RISC3D module and a folder `GPU` with the
drivers (each starts only on the machine it is for: Linux3D on RISC OS on
Linux; V3DRender on the Raspberry Pi 4, 400 and CM4). Its `!Boot` loads RISC3D
at start-up and RISC3D loads the first driver in `GPU` that starts, so
programs need nothing in their `!Run`. `*RISC3D` says which driver is in
use. On RISC OS on Linux, Linux's side is the Debian package
`riscos-risc3d` (`rol-3d` and Mesa 26.1.6 from trixie-backports), which
ROL's riscos-support (1.0.0068 on) brings in through Configure's Updates;
riscos-harddisc (1.0.0064 on) installs the PackMan package.

### For programs

Programs reach RISC3D through [riscos-mesa](https://github.com/adyoull/riscos-mesa)'s
EGL and OSMesa (!MesaLibs). They don't choose between the GPU and software:
RISC3D does, by the driver it has loaded, and with none Mesa draws in
software.

- **!MesaLibs 20.3.5-20 or later** (20.3.5-21 also carries the so/2 loader)
  has both routes: the command stream for RISC OS on Linux, and
  `libMesaGPU` (Mesa 26.1.6's v3d driver) for native RISC OS on a Pi 4.
- **On native RISC OS, a program draws on the GPU only if it is linked
  with libMesaGPU:**
  - mixed (shared) builds: `-lMesaGPU` after `-lOSMesa`;
  - static builds: `-Wl,-u,riscos_mesa_gpu -lMesaGPU` (about 15 MB more).

  Older programs, and programs without it, still work but draw in software
  there.
- **On RISC OS on Linux,** any program built against a RISC3D-aware
  riscos-mesa (20.3.5-19 on) uses the GPU. No libMesaGPU is needed.
- **In a program's !Run:** nothing is needed, since !RISC3D loads RISC3D at
  start-up. A program that must have it can check with
  `RMEnsure RISC3D 0.04 Error This program needs !RISC3D`. Never name a
  driver.

### How riscos-mesa uses it

These are riscos-mesa's parts; its EGL-GUIDE has the details.

- **EGL:** `eglCreateContext` gives OpenGL (up to 3.1, compatibility
  profile) and OpenGL ES 1.1 to 3.1 contexts to RISC3D where it has a
  driver. OpenGL 3 and ES 3 contexts exist only through RISC3D.
  - `EGL_VERSION` says so: "1.4 riscos-mesa (OSMesa; RISC3D for OpenGL and
    OpenGL ES)".
  - `GL_VERSION` says "... RISC3D (Mesa x)" on RISC OS on Linux.
  - A program can ask for software with the context attribute
    `EGL_CONTEXT_SOFTWARE_RISCOS` (extension `EGL_RISCOS_renderer`).
- **Window frames:**
  - **On RISC OS on Linux:** frames can go straight onto the screen
    through one of rol-fb's display planes (zero copy), in 16 million
    colour modes, with the window painted in a key colour. Otherwise they
    are copied back into the window.
  - **On native RISC OS:** the GPU draws straight into the window's
    sprite or into screen memory.
  - `EGL$SwapInterval` (0–4) overrides a program's vsync setting.
- **OSMesa:** programs that call OSMesa directly are routed to RISC3D too,
  drawing into their own buffer (`OSMESA_SOFTWARE_RISCOS` keeps one in
  software).
- **SDL:** SDL's OpenGL windows use EGL, and so RISC3D, where RISC3D has a
  driver. The hint `SDL_RISCOS_GL_EGL` set to 0 keeps SDL's own sprite path
  (software).
- **Variables:**
  - `RISC3D$Disable`: software for programs started afterwards.
  - `LIBGL_ALWAYS_SOFTWARE`: the same, Mesa's usual way.
  - `RISC3D$Force`: use the driver even where it draws in software (such
    as Linux's llvmpipe).
  - `V3DM$Stats 1`: on native RISC OS, the GPU work every 5 seconds.

Writing a driver for another GPU: see [docs/DRIVER-GUIDE.md](docs/DRIVER-GUIDE.md).

RISC3D's client library (`lib/`, `proto/`) is released as
`RISC3D-client-<version>.tgz` for riscos-mesa to build in.

### Where it runs

| System | Driver | GPU | Tested |
|---|---|---|---|
| Native RISC OS, Raspberry Pi 4 / 400 / CM4 | V3DRender | V3D 4.2 (Mesa's v3d in riscos-mesa) | Yes |
| RISC OS on Linux, Raspberry Pi 4 | Linux3D | V3D 4.2 (Linux's v3d, Mesa 26.1) | Yes |
| RISC OS on Linux, Pinebook Pro | Linux3D | Mali-T860 (Linux's panfrost) | No: drivers are in ROL's kernel and Debian's Mesa |
| RISC OS on Linux, Pinebook | Linux3D | Mali-400 (Linux's lima) | No: as above; an OpenGL ES 2.0-class GPU |
| RISC OS on Linux, Windows Dev Kit 2023 | Linux3D | Adreno 690 (Linux's msm, Mesa's freedreno) | Yes (Chris Gransden: Quakespasm's timedemo 5 fps at 640x480 in software, 98 fps at 1920x1080 on the GPU). The GPU's firmware comes with ROL's riscos-support 1.0.0069 and kernel 7.2.0-7.1 |
| Anything else | none | Mesa draws in software, as before | |

On RISC OS on Linux, Linux3D hands the drawing to Linux, which uses whatever
GPU it has, so any machine whose GPU Linux and Mesa drive should work.

## The parts

| Part | What it is |
|---|---|
| `modules/RISC3D` | The interface module. Drivers register; a program's GL library opens a context and gets memory shared with the driver. RISC3D never looks at the commands: it keeps the drivers and contexts, closes a task's contexts when the task goes, and passes on "wake" and "wait". |
| `modules/Linux3D` | The driver for RISC OS on Linux. Each context is a connection to the `rol-3d` helper; it maps the shared memory into RISC OS. |
| `linux/rol-3d.c` | The Linux helper: one process per context, with an EGL context on Linux's graphics driver (the GPU through its render node; Mesa's software renderer otherwise): OpenGL's compatibility profile (or, where that is older than the program asks for, the version asked for without it), GLES 2/3, or GLES 1.1 on desktop OpenGL (Debian's Mesa has none; `linux/es1*.c`). |
| `proto/` | The command stream. `gen.py` writes both ends from Khronos's registry (`khronos/gl.xml`: OpenGL 1.0 to 3.1, GLES 1.1 to 3.1, and every extension riscos-mesa has; 1189 functions, 218 of them by hand, 484 other names), and `r3d_gl.h`. `r3d_shared.h` is the shared memory's layout. |
| `lib/` | The GL functions for RISC OS programs, as commands: `r3d_glXxx`, listed by GL name (and the usual ARB/EXT names) in `r3d_gl_functions[]` so that Mesa's EGL (riscos-mesa) can build a dispatch table from them. Contexts: `r3d_create` (GLES 2 and on), `r3d_create_gl` (OpenGL), `r3d_create_es1` (GLES 1.1), `r3d_create_version` (a version at least), `r3d_target` (the program's own buffer as framebuffer 0, as OSMesa's), `r3d_driver_flags` (R3D_DRIVER_SOFTWARE: Linux draws in software), `r3d_make_current`, `r3d_destroy`; `r3d_swap` finishes a frame into a buffer. It keeps the matrix stacks and other state queried often, so those queries need no wait. `r3d_open`/`r3d_open_gl`/`r3d_close` and `R3D_DIRECT` are for the test programs. |
| `demo/test`, `demo/cube`, `demo/gl1`, `demo/es`, `demo/gl3` | `r3dtest` (GLES 2, command line, checks the pixels), `r3dcube` (the same cube in a desktop window), `gl1test` (OpenGL 1.x/2.1; built with DIRECT, the same drawn by Linux's Mesa directly, to compare; `gl1test bench`), `estest` (GLES 1.1 and GLES 2's extensions), `gl3test` (GLES 3.0/3.1, OpenGL 3.0/3.1 and EXT_direct_state_access), `targettest` (drawing into the program's buffer in OSMesa's formats; -DOSMESA: riscos-mesa's OSMesa itself). |
| `test/host` | Stand-ins so that the library and helper can be tested on the build machine without RISC OS. |

## How a frame goes

1. The GL library writes each call as a few words into a ring in the
   shared memory. No SWI, no system call.
2. The renderer (another process, usually another core) reads them. It is
   shown them in batches, and only needs waking if it has gone to sleep
   (`RISC3D_Sync` 0: one byte down a socket).
3. When the program needs an answer (a query, the end of the frame) it
   calls `RISC3D_Sync` 1 and waits; the answer is in the shared reply area.
4. PROTOTYPE: the finished frame is read back and copied into the
   program's sprite. Zero-copy presentation (a hardware plane through
   rol-fb, as video already does) is not done.

## SWIs (provisional)

| SWI | In | Out |
|---|---|---|
| `RISC3D_Info` (&C3D00) | | r0 = version * 100, r1 = drivers, r2 = contexts, r3 -> the name of the driver a new context gets (0: none), r4 = its flags, r5 = its ioctl SWI and r6 -> its DRM driver name (DRM devices) |
| `RISC3D_Register` (&C3D01) | r0 = 0 add / 1 remove, r1 = entry, r2 = its r12, r3 -> name, r4 = flags (bit 0: a DRM device), r5 = ioctl SWI, r6 -> DRM driver name | |
| `RISC3D_Open` (&C3D02) | r0 = flags (0) | r0 = context, r1 -> shared memory, r2 = size (a DRM device: r1 = the driver's file, r2 = 0) |
| `RISC3D_Close` (&C3D03) | r0 = context | |
| `RISC3D_Sync` (&C3D04) | r0 = context, r1 = 0 wake / 1 wait | |

A driver's entry is called with r0 = 0 open, 1 close, 2 sync (see
`modules/RISC3D/risc3d.c`).

Two kinds of driver (RISC3D 0.02): a command-stream driver (Linux3D, on
RISC OS on Linux) reads the commands `lib/` writes into the shared
memory; a DRM device (flag bit 0: V3DRender, the native driver for the
Raspberry Pi 4's GPU, in its own repository) takes no command stream.
For a DRM device the program's Mesa runs its own GPU driver (Mesa's v3d)
and sends that driver's requests to the driver's ioctl SWI, with the
file RISC3D_Open returned, as Mesa does through a DRM device on Linux.
`r3d_available` is for the command stream, so it is 0 there.

## Building and testing

`RISCOS_TOOLCHAIN=.../toolchain/env ./build.sh` builds everything into
`out/`. On the build machine: `out/rol-3d-host &` then
`out/r3dtest-host 100`. On RISC OS on Linux: `/opt/riscos/rol-3d` (init
starts it) and !RISC3D in `!Boot.Resources` (it loads RISC3D, which loads
Linux3D), then `r3dtest` or `r3dcube`.

## Not done

- Window moves and resizes, and vsync, for RISC OS on Linux's zero-copy
  frames.
- More of GL's state kept here (enables, bound textures), and immediate
  mode packed tighter in the ring.
- glDrawElements with indices in a buffer object and arrays in the
  program's memory reads the indices back (one wait).
- Arrays in the program's memory with indirect draws are drawn as the
  direct call (the command read back); a pixel pack buffer isn't
  followed by glGetTextureSubImage, glGetCompressedTextureSubImage and
  the imaging gets (glGetnColorTable and the like).
- Allocated SWI and error numbers. (The native driver, V3DRender, is
  in its own repository.)

Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). The Khronos
headers in `include/` carry their own licences.

## Licence

MIT (see LICENSE), Copyright (c) 2026 Andrew Youll. Included from others:
Khronos's registry `proto/khronos/gl.xml` (Apache-2.0) and the Khronos
headers in `include/` (MIT-style, as in each file); `linux/es1_conversion.c`
is ported from Mesa 20.3.5 (MIT, the Mesa authors).
