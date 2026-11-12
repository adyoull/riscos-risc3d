#!/bin/sh
# tools/mkdeb.sh: the riscos-risc3d package for RISC OS on Linux, from
# build.sh's out/ and packaging/riscos-risc3d/, into out/.
# Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).
set -e
cd "$(dirname "$0")/.."
s=packaging/riscos-risc3d
v=$(sed -n 's/^Version: //p' $s/control)
P=out/pkg/riscos-risc3d
rm -rf out/pkg
mkdir -p $P/DEBIAN $P/opt/riscos $P/riscos/HardDisc4/TestBed/RISC3D $P/usr/share/doc/riscos-risc3d
aarch64-linux-gnu-strip -o $P/opt/riscos/rol-3d out/rol-3d
chmod 755 $P/opt/riscos/rol-3d
cp "out/modules/RISC3D,ffa" "out/modules/Linux3D,ffa" $P/riscos/HardDisc4/TestBed/RISC3D/
cp $s/README $s/copyright $P/usr/share/doc/riscos-risc3d/
gzip -9n -c $s/changelog > $P/usr/share/doc/riscos-risc3d/changelog.gz
{ sed -n '1,/^Architecture/p' $s/control
  echo "Installed-Size: $(du -sk --exclude=DEBIAN $P | cut -f1)"
  sed -n '/^Architecture/,$p' $s/control | tail -n +2; } > $P/DEBIAN/control
install -m 755 $s/postinst $s/prerm $s/postrm $P/DEBIAN/
dpkg-deb --root-owner-group -Zxz --build $P "out/riscos-risc3d_${v}_arm64.deb"
