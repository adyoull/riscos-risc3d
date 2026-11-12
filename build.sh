#!/bin/bash
# build.sh: builds everything into out/.
#   out/gen/            both ends of the command stream (proto/gen.py)
#   out/modules/        RISC3D,ffa and Linux3D,ffa
#   out/libRISC3D.a     the GLES 2 front end for RISC OS programs
#   out/r3dtest,ff8     the command-line test
#   out/r3dcube,ff8     the same cube in a desktop window
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

aarch64-linux-gnu-gcc -O2 -Wall -Wextra $I -o out/rol-3d linux/rol-3d.c -ldl
gcc -O2 -Wall -Wextra $I -o out/rol-3d-host linux/rol-3d.c -ldl
gcc -O2 -Wall -DR3D_HOST -Itest/host $I -Idemo/test -o out/r3dtest-host demo/test/r3dtest.c lib/r3d_gl.c -lm
ls -la out/modules out/libRISC3D.a "out/r3dtest,ff8" out/rol-3d
