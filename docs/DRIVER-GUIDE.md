# Writing a GPU driver for RISC3D

This guide is for anyone adding a new GPU to RISC3D. RISC3D is the
interface between Mesa (riscos-mesa's !MesaLibs) and a machine's GPU driver.
It says what a driver is, how RISC3D finds and calls it, and what it has to do.

The interface is as in RISC3D module 0.05 (release 0.11). Its SWI chunk
(&5AF80) and error blocks are allocated by RISC OS Allocations.

There are two drivers to learn from:

| Driver | Kind | Machine | Source |
|---|---|---|---|
| **V3DRender** | DRM device | native RISC OS, Raspberry Pi 4/400/CM4 | [riscos-v3drender](https://github.com/adyoull/riscos-v3drender) (`docs/INTERFACE.md`) |
| **Linux3D** | command stream | RISC OS on Linux | this repository (`modules/Linux3D`, `linux/rol-3d.c`) |
| **EtnaRender** | DRM device | native RISC OS, i.MX6 (Vivante GC) | its own project (not yet released or tested on hardware) |

## 1. The two kinds of driver

**A DRM device (recommended for a real GPU).** The program runs Mesa's
own driver for the GPU, the same code Linux uses, inside riscos-mesa's GPU
library. Your module does what Linux's kernel driver does for that GPU:
- the GPU's memory and its MMU;
- job queues and interrupts;
- resets.

Mesa reaches the module through a single "ioctl" SWI, which takes Linux's
request numbers and structures. Most of the work has already been done
twice over: by Linux's kernel driver, which you follow, and by Mesa's
driver, which you don't have to write. V3DRender is this kind.

**A command stream.** The program writes GL calls as commands into memory
it shares with your driver. Your driver plays them back somewhere else.
This suits a GPU that something outside RISC OS drives: Linux3D hands the
commands to Linux, where `rol-3d` replays them with Linux's Mesa. The
commands come from Khronos's registry (`proto/gen.py`), and the player has
to implement every one. So this kind only makes sense where a full OpenGL
already exists on the other side.

The rest of this guide marks what applies to only one kind.

## 2. Where a driver lives and how it's loaded

RISC3D is installed as `!RISC3D` in `!Boot.Resources`:

```
!RISC3D
  !Boot, !Run     set RISC3D$Dir and RISC3D$Path, load RISC3D
  RISC3D          the module
  GPU             the drivers: Linux3D, V3DRender, ... yours
```

At start-up, !RISC3D's !Boot loads RISC3D. Just after it starts, RISC3D
looks for a driver. If that hasn't happened yet, it looks at the first
`RISC3D_Info`, `RISC3D_Open` or `*RISC3D` instead:

1. **A driver already registered?** If so, nothing more is loaded. This
   covers one loaded by hand, or at start-up some other way.
2. **The folder:** `<RISC3D$Drivers>` if that's set (for testing), else
   `<RISC3D$Dir>.GPU`.
3. **Try each module:** RISC3D loads each file of type Module (&FFA) there
   with `OS_Module 1`, in the order the filing system lists them. It stops
   at the first that registers.

So **one !RISC3D, with every driver in GPU, serves every machine**. RISC3D
has no list of machines. Instead:

> **Your module must refuse to start on a machine it isn't for:** return
> an error from its initialisation. RISC OS then doesn't keep it, and RISC3D
> tries the next.

- **Check before you touch any hardware.** V3DRender asks BCMSupport for
  the board's revision, and reads the GPU's identity registers only on a
  BCM2711. Linux3D checks for `IXSupport_LinuxSyscall` and for Linux's
  helper.
- **Make the error say why.** `*RISC3D` lists each module it tried and its
  error, which is what a user sends you.
- **Ship it in GPU.** RISC3D is one PackMan package for every system, with
  every driver in `!RISC3D.GPU`. To ship yours, offer it for that package:
  an issue or pull request on this repository, with the module's version,
  its checksum, its licence and where its source is. Don't make a package
  of your own.

Programs never name a driver and never load one; neither should you
ask them to.

## 3. Registering

Register at the end of a successful initialisation, and remove yourself in
your finalisation.

**`RISC3D_Register` (&5AF81)**, to add a driver:

| In | |
|---|---|
| r0 | 0 (add) |
| r1 | your entry point (section 4) |
| r2 | the value for r12 when it's called (your private word, or 0) |
| r3 | → your name, shown by `*RISC3D` (up to 47 characters) |
| r4 | flags: bit 0 = a DRM device; bit 1 = you may draw in software (section 6); bit 2 = your contexts can share objects (section 6, RISC3D 0.06 on); bit 3 = you describe your hardware (reason 4, RISC3D 0.09 on) |
| r5 | DRM device: your ioctl SWI's number |
| r6 | DRM device: → the name of Mesa's driver for your GPU ("v3d", "etnaviv", ...) |

To remove one: r0 = 1, with the same r1 and r2. RISC3D first closes every
context that's open on you, through your entry. Its programs get errors
from then on.

Errors: "Too many 3D contexts or drivers" (&823D01) if four drivers are
already registered. If RISC3D isn't loaded, the SWI doesn't exist.

- **If RISC3D isn't there:** either refuse to start, as Linux3D does, or
  stay loaded and register when it arrives.
- **Re-registering:** V3DRender watches for `Service_ModulePostInit` (&DA)
  with RISC3D's title and registers again then. That covers RISC3D being
  reloaded. (A reloaded RISC3D also reloads its drivers from GPU, which
  restarts yours.)

**`RISC3D_Info` (&5AF80)** is what Mesa asks. It's useful for checking your
registration:

| Out | |
|---|---|
| r0 | RISC3D's version × 100 (9 for 0.09) |
| r1 | drivers registered |
| r2 | contexts open |
| r3 | → the name of the driver a new context gets (the last registered), or 0 |
| r4 | its flags: bit 0 a DRM device; bit 1 drawing in software now; bit 2 contexts can share objects now (0.06 on) |
| r5, r6 | a DRM device's ioctl SWI and Mesa driver name, else 0 |

## 4. Your entry point

RISC3D calls your entry from inside its SWIs, so the call comes in SVC mode
and from whatever task called it. The registers are:

- **r0:** the reason;
- **r1 to r9:** its arguments;
- **r12:** the value you registered;
- **lr:** where to return.

Return with V clear and the results in r0 to r9. For an error, return with
V set and r0 pointing to an error block. RISC3D passes it on to the
program. You may change r0 to r9. Keep r10, r11 and sp as they were.

| r0 | Reason | In | Out |
|---|---|---|---|
| 0 | **open** a context | r1 = flags: bit 0 = share the objects of the context whose handle is r2 (flags bit 2 only; RISC3D 0.06 on) | command stream: r0 = your handle, r1 → the shared memory, r2 = its size. DRM device: r0 = the file, r1 = r2 = 0 |
| 1 | **close** it | r1 = your handle (or file) | |
| 2 | **sync** (command stream only) | r1 = handle, r2 = 0 wake / 1 wait | |
| 3 | **describe** (flags bit 1 only) | | r0 = 0 on a GPU, 1 in software now, 2 not known yet; r1 bit 0 = contexts can share objects now (flags bit 2 too) |
| 4 | **hardware** (flags bit 3 only; RISC3D 0.09 on) | r1 → a buffer, r2 = its size | r2 = the size your text needs, with its terminating 0 |

- **Open:** made for each `RISC3D_Open`. RISC3D keeps your handle and gives
  the program its own context number.
  - For a DRM device, the "file" is what the program's libdrm uses as its
    fd: a client of yours, with its own buffers and sync objects.
  - Refuse with an error if you can't; the program then draws in
    software.
- **Close:** called for `RISC3D_Close`, and also:
  - when the program's Wimp task ends (`Service_WimpCloseDown`);
  - when RISC3D or your driver is removed.

  Free everything the context held. A command-line program that dies
  without closing still holds it until a restart. riscos-mesa closes its
  contexts at exit, and from its signal handlers, to keep that rare.
- **Sync:** command stream only; see section 5.
- **Describe:** only if you registered with flags bit 1; see section 6.
- **Hardware:** only if you registered with flags bit 3. Write what you know
  about the GPU itself as lines of `label` TAB `value`, each ended by a
  line feed, then a 0. For example: `GPU` TAB `V3D 4.2, revision 14`,
  `Clock` TAB `500 MHz`, `GPU memory` TAB `2048 KB in use`. Keep labels short (up to
  about 24 characters) and values to one line (up to about 90). Configure's
  RISC3D window shows up to 8 of them under **Hardware**.
  - Fill as much as fits in r2 bytes, always ending with a 0 when r2 > 0, and
    return the full size in r2.
  - Give hardware facts only. What OpenGL and OpenGL ES offer on your GPU
    comes from Mesa: see the probe in section 5.
  - Programs reach it through `RISC3D_Describe` (&5AF85): r0 = 0, r1 →
    buffer, r2 = size → r2 = size needed (1 if you say nothing), r3 → your
    name.

RISC3D allows up to 32 contexts at once, across all drivers.

## 5. A DRM device

Mesa finds and opens you through RISC3D, never by your own SWIs:

1. `RISC3D_Info`: r4 bit 0 says a DRM device. r5 is the SWI every request
   goes to, and r6 names Mesa's driver.
2. `RISC3D_Open` with r0 = 0: r1 is the file (your open reason's r0).
3. **Requests:** `drmIoctl (fd, request, arg)` becomes your SWI with
   r0 = the file, r1 = Linux's request number, r2 → Linux's structure.
   Return r0 = 0, or a negated Linux errno. Don't raise a RISC OS error
   for those. riscos-mesa turns the Linux errno into UnixLib's.
4. `RISC3D_Close` at the end.

What to implement is the request set Mesa's driver for your GPU uses on
Linux. Follow Linux's kernel driver for it. V3DRender's `docs/INTERFACE.md`
lists what Mesa's v3d driver needs, with the few places RISC OS differs:

- **Mapping:** `MMAP_BO`'s "offset" is the buffer's address in the
  program's memory, already mapped, and `munmap` does nothing. RISC OS has
  no mmap of a device.
- **Timeouts:** absolute ones count on a clock you define (V3DRender:
  `OS_ReadMonotonicTime` × 10,000,000 ns).
- **Sync files:** these are your own numbers, not UnixLib file
  descriptors. Add requests to wait on and close them.
- **The CPU's view:** buffers the CPU reads back want to be cacheable,
  with cache maintenance at submit and after the job. Screen memory and a
  window's sprite can be imported, so the GPU draws into them directly.
  V3DRender's `DRM_RISCOS_BO_CPU_CACHED` and
  `DRM_IOCTL_RISCOS_IMPORT_MEMORY` are the models. Without them, frames
  are copied through uncached memory, and that is slow.
- **Memory other parts of the machine need:** keep out of it. On the Pi
  4, V3DRender takes its memory above the first 1 GB, which the VideoCore
  needs for video.

**On the Mesa side**, riscos-mesa's GPU library has to carry Mesa's driver
for your GPU. It picks the driver by the name in r6, from a table that
holds only "v3d" today. Adding yours means:
- a row in that table;
- your Mesa driver built into libMesaGPU.

Ask riscos-mesa for it, with your driver's name and Mesa's driver. Mesa
25.2 or later is used, since older v3d drivers mis-draw fixed-function
lighting.

### What Configure's window shows for a DRM device: GPUProbe

For a DRM device, the OpenGL and OpenGL ES that programs get come from the
Mesa linked into each program, so neither RISC3D nor your driver can say what
they are. Configure's RISC3D window (!RISC3DSetup 1.05 on) runs riscos-mesa's
**GPUProbe** instead: `<MesaLibs$Dir>.GPUProbe -o <file>`, from !MesaLibs
20.3.5-23 on, started as a task of its own.

- It's linked with the same Mesa as every program and goes through RISC3D in
  the same way. It makes an OpenGL, an OpenGL ES and an OpenGL ES 1.1
  context, closes them, and writes `key: value` lines (format 1, in
  !MesaLibs' !Help).
- There's one block per API (`api:`, `route:`, `version:`, `renderer:`,
  `shading:`, `max_texture_size:`, `extensions:`, `error:`), each ended by
  `end`.
- Your driver needs nothing for it, beyond opening contexts as usual.
- With an older !MesaLibs, the window says the figures need !MesaLibs
  20.3.5-23.

## 6. A command stream

Your open reason gives the program a block of memory you share, laid out
as in `proto/r3d_shared.h`:
- a header;
- a ring of commands, 4 MB;
- a reply area, 32 MB, which holds answers and finished frames.

**The program's side.** It writes commands into the ring and moves `head`,
with no SWI. A command is words: its opcode, its length in words (counting
those two), then its arguments. A command is never split at the end of the
ring: `OP_WRAP` (0) sends the reader back to the start.

**Your side.** You read the commands and move `tail`. Set `sleeping` when
you stop to wait for more. Answers go into the reply area, with their
length in `reply_len`.

**Sync** (reason 2) is all the program asks of you through RISC3D:

- **r2 = 0, wake:** there are new commands, so wake if you're sleeping.
  The program calls it only when it sees `sleeping`, so it's rare.
- **r2 = 1, wait:** return only when everything written so far has been
  done, and any answer is in the reply area. Queries, `glFinish` and the
  end of each frame use it.

The commands are made from Khronos's registry by `proto/gen.py`. That
writes the opcodes (`r3d_ops.h`), the program's side (`client_gen.c`,
built into `lib/r3d_gl.c`) and the player's side (`server_gen.c`). It also
adds a few of RISC3D's own commands (`OP_CONTEXT`, `OP_IMMEDIATE`, client
arrays, frames).
`linux/rol-3d.c` is the reference player.

**Flags bit 1** is for a driver whose other side may itself be drawing in
software. Linux3D uses it when Linux has no GPU driver, so Mesa there falls
back to llvmpipe. RISC3D then asks reason 3 at each `RISC3D_Info`.
- **0 or 2:** Mesa sends programs to you.
- **1:** Mesa keeps OpenGL up to 2.1 and OpenGL ES up to 2.0 in its own
  software renderer, which beats a software renderer's frames copied back.
  `RISC3D$Force` overrides this.

**Shared contexts (flags bit 2, RISC3D 0.06 on).** A program can ask for
a context that shares another's objects (textures, buffers, renderbuffers,
programs, shaders, samplers, sync objects, display lists), as EGL's
`share_context` does: `RISC3D_Open` with r0 bit 0 and r1 = the program's
context. WebKit's Skia draws that way, a context per thread. RISC3D sends
it to the other context's driver, with open's r1 bit 0 set and r2 = your
handle for that context; without flags bit 2 it refuses with &823D03. Your
side must then draw both contexts with one set of objects:
- **Names.** The program's library gives texture, buffer and list names,
  and sync object numbers, from one set for the group. Yours must not
  clash: rol-3d gives each context of a group its own range.
- **Ordering.** The program calls `glFinish` in one context before another
  uses what it made, or makes a fence, flushes, and waits for it in the
  other. A wait can reach you before the fence's own context has run the
  command that makes it, so wait for the fence to exist (rol-3d waits up
  to 2 s).
- **Closing.** The group's objects live while any of its contexts does.

Linux3D does this by opening a shared context on the first context's own
Linux process (`rol-3d-<pid>`), where it is a thread with an EGL context in
the first one's share group. With flags bit 1 too, say whether you can now
in reason 3's r1 (Linux3D: whether rol-3d is new enough).

## 7. Errors and numbers

Allocated by RISC OS Allocations (18 November 2026):

| | SWI chunk | Error block |
|---|---|---|
| RISC3D | &5AF80 | &823D00 |
| Linux3D | (none) | &823C00 |
| V3DRender | &5AFC0 | &823E00 |

**A new driver needs its own SWI chunk and error block.** Ask RISC OS
Allocations for them before you release, and register your module's name
and *commands there too. Until they're allocated, use provisional numbers
that clash with nothing allocated, and say so.

RISC3D's own errors are:
- &823D00: there is no 3D driver;
- &823D01: too many contexts or drivers;
- &823D02: not a RISC3D context;
- &823D03: this driver can't share objects between contexts (0.06 on).

Linux3D's include &823C07: Linux's rol-3d is too old to share objects
between contexts (Linux3D 0.06 on).

## 8. Checklist

- [ ] Refuses to start, with a clear error, on any machine it isn't for, before touching hardware.
- [ ] Registers at the end of initialisation. Removes itself in finalisation. Registers again on `Service_ModulePostInit` for RISC3D.
- [ ] Entry: reasons 0 and 1, plus 2 for a command stream and 3 if flags bit 1. V and r0 for errors; r10, r11 and sp kept.
- [ ] Flags bit 2 only if contexts opened with open's r1 bit 0 really share objects, with names that don't clash and fences waited for across contexts.
- [ ] Frees a context's resources on close, which may come from task closedown.
- [ ] DRM device: Linux's requests and structures, 0 or a negated errno, the file from open. The few RISC OS differences above. Mesa's driver named in r6, and riscos-mesa asked to carry it.
- [ ] Command stream: the `r3d_shared.h` layout, wake and wait, every command played back.
- [ ] A `*` command showing its state helps people report problems (`*V3DRender`, `*Linux3D`).
- [ ] Tested by putting the module in `!RISC3D.GPU` and restarting. `*RISC3D` should list it as the driver. On other machines it should be listed as refused, with your error.
- [ ] Offered for `!RISC3D.GPU` with its version, checksum, licence and source.
