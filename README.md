# RISC3D (prototype)

A 3D driver interface for RISC OS: Mesa's front end (EGL, GLES, OpenGL)
above it, 3D drivers below it. Programs use Mesa as they do now; where a
driver is registered the drawing is done by the machine's GPU driver, and
where none is, Mesa draws in software as before.

**Prototype, 12 November 2026.** The SWI chunk (&C3D00) and error numbers
are provisional, the wire format will change, and nothing here has run on
real hardware yet.

## The parts

| Part | What it is |
|---|---|
| `modules/RISC3D` | The interface module. Drivers register; a program's GL library opens a context and gets memory shared with the driver. RISC3D never looks at the commands: it keeps the drivers and contexts, closes a task's contexts when the task goes, and passes on "wake" and "wait". |
| `modules/Linux3D` | The driver for RISC OS on Linux. Each context is a connection to the `rol-3d` helper; it maps the shared memory into RISC OS. |
| `linux/rol-3d.c` | The Linux helper: one process per context, with an EGL context on Linux's graphics driver (the GPU through its render node; Mesa's software renderer otherwise). GLES 2 where Linux's Mesa has it, desktop OpenGL otherwise. |
| `proto/` | The command stream. `gen.py` writes both ends from the Khronos header (`include/GLES2/gl2.h`: 142 functions, 28 of them by hand). `r3d_shared.h` is the shared memory's layout. |
| `lib/` | The GLES 2 functions for RISC OS programs, as commands: `r3d_glXxx`, listed by GL name in `r3d_gl_functions[]` so that Mesa's EGL (riscos-mesa) can build a dispatch table from them. Contexts: `r3d_create`, `r3d_make_current`, `r3d_destroy`; `r3d_swap` finishes a frame into a buffer. `r3d_open`/`r3d_close` and `R3D_DIRECT` are for the test programs. |
| `demo/test`, `demo/cube` | `r3dtest` (command line, checks the pixels) and `r3dcube` (the same cube in a desktop window). |
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
| `RISC3D_Info` (&C3D00) | | r0 = version * 100, r1 = drivers, r2 = contexts, r3 -> the name of the driver a new context gets (0: none) |
| `RISC3D_Register` (&C3D01) | r0 = 0 add / 1 remove, r1 = entry, r2 = its r12, r3 -> name | |
| `RISC3D_Open` (&C3D02) | r0 = flags (0) | r0 = context, r1 -> shared memory, r2 = size |
| `RISC3D_Close` (&C3D03) | r0 = context | |
| `RISC3D_Sync` (&C3D04) | r0 = context, r1 = 0 wake / 1 wait | |

A driver's entry is called with r0 = 0 open, 1 close, 2 sync (see
`modules/RISC3D/risc3d.c`).

## Building and testing

`RISCOS_TOOLCHAIN=.../toolchain/env ./build.sh` builds everything into
`out/`. On the build machine: `out/rol-3d-host &` then
`out/r3dtest-host 100`. On RISC OS on Linux: `/opt/riscos/rol-3d` (init
starts it), `RMLoad RISC3D`, `RMLoad Linux3D`, then `r3dtest` or
`r3dcube`.

## Not done

- Mesa's EGL as the front end (riscos-mesa): the hardware path behind the
  standard API, software where there is no driver.
- Desktop OpenGL's fixed-function calls in the command stream (generate
  from Khronos's `gl.xml`).
- Zero-copy presentation; window moves and resizes; vsync.
- Client-side object names and cached queries (fewer waits).
- Real hardware: a Raspberry Pi 4 or 5 with Mesa's v3d driver.
- A native RISC OS driver. Allocated SWI and error numbers.

Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). The Khronos
headers in `include/` carry their own licences.
