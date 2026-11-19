#!/bin/sh
# tools/mkclient.sh VERSION: the client release for riscos-mesa,
# out/RISC3D-client-VERSION.tgz, from build.sh's out/ (libRISC3D.a, gen/)
# and the sources. Kept if it already exists (a delivered version is never
# rebuilt: bump it).
# Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).
set -e
cd "$(dirname "$0")/.."
v=${1:?version}
o=out/RISC3D-client-$v.tgz
[ -e "$o" ] && { echo "kept $o"; exit 0; }
d=out/client/RISC3D-client-$v
rm -rf out/client && mkdir -p $d/lib $d/gen $d/proto $d/include/KHR
cp lib/r3d.h lib/r3d_gl.c lib/r3d_dsa.c $d/lib/
cp out/gen/r3d_gl.h out/gen/r3d_ops.h out/gen/r3d_exts.h out/gen/r3d_gl_names.h out/gen/client_gen.c $d/gen/
cp proto/r3d_shared.h $d/proto/
cp include/KHR/khrplatform.h $d/include/KHR/
cp out/libRISC3D.a LICENSE $d/
cp packaging/client/README.txt $d/
tar -C out/client --owner=0 --group=0 --sort=name -czf "$o" "RISC3D-client-$v"
echo "$o"
