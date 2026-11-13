#!/bin/bash
# build.sh: builds everything into out/.
#   out/gen/            both ends of the command stream (proto/gen.py)
#   out/modules/        RISC3D,ffa and Linux3D,ffa
#   out/libRISC3D.a     the GLES 2 front end for RISC OS programs
#   out/r3dtest,ff8     the command-line test
#   out/r3dcube,ff8     the same cube in a desktop window
#   out/gl1test,ff8     OpenGL 1.x/2.1 through RISC3D (prints, writes a picture)
#   out/gl1test-linux   the same drawn by Linux's Mesa directly (64-bit ARM),
#                       to compare with
#   out/rol-3d          the Linux helper (64-bit ARM)
#   out/rol-3d-host, out/r3dtest-host   the same two for the build machine
# RISCOS_TOOLCHAIN: riscos-crossdev's toolchain/env.
#
# Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).
set -euo pipefail
cd "$(dirname "$0")"
T=${RISCOS_TOOLCHAIN:?set RISCOS_TOOLCHAIN}/bin/arm-riscos-gnueabihf-
mkdir -p out/gen out/modules out/obj
python3 proto/gen.py out/gen
I="-Iinclude -Ilib -Iout/gen -Iproto"

MC="-O2 -marm -march=armv7-a -mno-unaligned-access -fpie -fvisibility=hidden -mgeneral-regs-only -ffreestanding -fno-common -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -nostdinc -isystem $(${T}gcc -print-file-name=include) -Wall -Wextra -Werror -Icommon"
ML="-nostdlib -static -Wl,--no-dynamic-linker -Wl,-T,common/module.ld -Wl,--build-id=none -Wl,-z,norelro"
${T}gcc $MC -c -o out/obj/cmodule.o common/cmodule.c
for m in RISC3D:risc3d Linux3D:linux3d; do
  n=${m%%:*}; c=${m##*:}
  ${T}as -I common -o out/obj/$n-header.o modules/$n/header.s
  ${T}gcc $MC -c -o out/obj/$n.o modules/$n/$c.c
  objs="out/obj/$n-header.o out/obj/$n.o out/obj/cmodule.o"
  ${T}gcc $MC $ML -o out/obj/$n.elf $objs -lgcc
  ${T}gcc $MC $ML -Wl,--defsym=__link_base=0x10000 -o out/obj/$n-64k.elf $objs -lgcc
  ${T}objcopy -O binary out/obj/$n.elf out/obj/$n-0.bin
  ${T}objcopy -O binary out/obj/$n-64k.elf out/obj/$n-1.bin
  python3 -B common/check-pic.py out/obj/$n-0.bin out/obj/$n-1.bin 0x10000 \
    0x$(${T}nm out/obj/$n.elf | awk '$3=="__got_start"{print $1}') \
    0x$(${T}nm out/obj/$n.elf | awk '$3=="__got_end"{print $1}')
  cp out/obj/$n-0.bin "out/modules/$n,ffa"
done

AC="-O2 -mfpu=vfpv3 -mfloat-abi=hard -Wall -Wextra -Werror"
${T}gcc $AC $I -c -o out/obj/r3d_gl.o lib/r3d_gl.c
rm -f out/libRISC3D.a; ${T}ar rcs out/libRISC3D.a out/obj/r3d_gl.o
${T}gcc $AC $I -Idemo/test -static -o out/obj/r3dtest.elf demo/test/r3dtest.c out/libRISC3D.a -lm
${T}strip out/obj/r3dtest.elf
"${RISCOS_TOOLCHAIN}/bin/elf2aif" -e out/obj/r3dtest.elf "out/r3dtest,ff8" >/dev/null
${T}gcc $AC $I -Idemo/test -static -o out/obj/cube.elf demo/cube/cube.c out/libRISC3D.a -lm
${T}strip out/obj/cube.elf
"${RISCOS_TOOLCHAIN}/bin/elf2aif" -e out/obj/cube.elf "out/r3dcube,ff8" >/dev/null
${T}gcc $AC $I -static -o out/obj/gl1test.elf demo/gl1/gl1test.c out/libRISC3D.a -lm
${T}strip out/obj/gl1test.elf
"${RISCOS_TOOLCHAIN}/bin/elf2aif" -e out/obj/gl1test.elf "out/gl1test,ff8" >/dev/null
${T}gcc $AC $I -static -o out/obj/estest.elf demo/es/estest.c out/libRISC3D.a -lm
${T}strip out/obj/estest.elf
"${RISCOS_TOOLCHAIN}/bin/elf2aif" -e out/obj/estest.elf "out/estest,ff8" >/dev/null
${T}gcc $AC $I -static -o out/obj/gl3test.elf demo/gl3/gl3test.c out/libRISC3D.a -lm
${T}strip out/obj/gl3test.elf
"${RISCOS_TOOLCHAIN}/bin/elf2aif" -e out/obj/gl3test.elf "out/gl3test,ff8" >/dev/null

aarch64-linux-gnu-gcc -O2 -Wall -Wextra $I -o out/rol-3d linux/rol-3d.c -ldl
gcc -O2 -Wall -Wextra $I -o out/rol-3d-host linux/rol-3d.c -ldl
gcc -O2 -Wall -DR3D_HOST -Itest/host $I -Idemo/test -o out/r3dtest-host demo/test/r3dtest.c lib/r3d_gl.c -lm
gcc -O2 -Wall -DR3D_HOST -Itest/host $I -o out/gl1test-host demo/gl1/gl1test.c lib/r3d_gl.c -lm
gcc -O2 -Wall -DR3D_HOST -Itest/host $I -o out/estest-host demo/es/estest.c lib/r3d_gl.c -lm
gcc -O2 -Wall -DR3D_HOST -Itest/host $I -o out/gl3test-host demo/gl3/gl3test.c lib/r3d_gl.c -lm
# the direct builds: Linux's libGL and libEGL, linked through stand-ins
# with their names (no -dev packages, nor 64-bit ARM libraries, needed)
direct () {				# compiler, output
  local cc=$1 o=$2 d=out/obj/stub-$(basename "$2")
  mkdir -p "$d"
  $cc -O2 -Wall -DDIRECT $I -c -o "$d/gl1test.o" demo/gl1/gl1test.c
  for lib in GL EGL; do
    $cc -shared -fPIC -Wl,-soname,lib$lib.so.1 -o "$d/lib$lib.so" -x c - <<<"$(nm -u "$d/gl1test.o" | awk -v p="${lib,,}" '$2 ~ "^"p"[A-Z]" {print "void " $2 " (void) {}"}')"
  done
  $cc -o "$o" "$d/gl1test.o" -L"$d" -lGL -lEGL
}
direct gcc out/gl1test-direct
direct aarch64-linux-gnu-gcc out/gl1test-linux
# estest and gl3test find Mesa's functions through eglGetProcAddress: only EGL
direct_es () {				# compiler, output, source
  local cc=$1 o=$2 src=${3:-demo/es/estest.c} d=out/obj/stub-$(basename "$2")
  mkdir -p "$d"
  $cc -O2 -Wall -DDIRECT $I -c -o "$d/test.o" "$src"
  $cc -shared -fPIC -Wl,-soname,libEGL.so.1 -o "$d/libEGL.so" -x c - <<<"$(nm -u "$d/test.o" | awk '$2 ~ "^egl[A-Z]" {print "void " $2 " (void) {}"}')"
  $cc -o "$o" "$d/test.o" -L"$d" -lEGL
}
direct_es gcc out/estest-direct
direct_es aarch64-linux-gnu-gcc out/estest-linux
direct_es gcc out/gl3test-direct demo/gl3/gl3test.c
direct_es aarch64-linux-gnu-gcc out/gl3test-linux demo/gl3/gl3test.c
ls -la out/modules out/libRISC3D.a "out/r3dtest,ff8" "out/gl1test,ff8" out/rol-3d

# the package (the kit carries it)
tools/mkdeb.sh >/dev/null

# the Pi test kit: a zip for RISC OS, with file types
python3 tools/mkkit.py out/RISC3DTest-14.zip RISC3DTest "kit/!ReadMe,fff" kit/Cube,feb kit/Test,feb \
  kit/GL,feb kit/ES,feb kit/GL3,feb kit/Overlay,feb kit/linux-tests,fff "out/r3dcube,ff8" \
  "out/r3dtest,ff8" "out/gl1test,ff8" "out/estest,ff8" "out/gl3test,ff8" out/gl1test-linux \
  out/estest-linux out/gl3test-linux "out/riscos-risc3d_0.5-1_arm64.deb"

