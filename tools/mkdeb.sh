#!/bin/sh
# tools/mkdeb.sh: the riscos-risc3d package for RISC OS on Linux, from
# build.sh's out/ and packaging/riscos-risc3d/, into out/.
# Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).
set -e
cd "$(dirname "$0")/.."
s=packaging/riscos-risc3d
v=$(sed -n 's/^Version: //p' $s/control)
# (a delivered version is kept, never built again: bump it)
if [ -e "out/riscos-risc3d_${v}_arm64.deb" ]; then echo "kept out/riscos-risc3d_${v}_arm64.deb"; exit 0; fi
P=out/pkg/riscos-risc3d
rm -rf out/pkg
mkdir -p $P/DEBIAN $P/opt/riscos $P/usr/share/doc/riscos-risc3d
aarch64-linux-gnu-strip -o $P/opt/riscos/rol-3d out/rol-3d
chmod 755 $P/opt/riscos/rol-3d
# (0.22-1: no RISC OS files: they are the PackMan package RISC3D)
cp $s/README $s/copyright $P/usr/share/doc/riscos-risc3d/
gzip -9n -c $s/changelog > $P/usr/share/doc/riscos-risc3d/changelog.gz
{ sed -n '1,/^Architecture/p' $s/control
  echo "Installed-Size: $(du -sk --exclude=DEBIAN $P | cut -f1)"
  sed -n '/^Architecture/,$p' $s/control | tail -n +2; } > $P/DEBIAN/control
# (0.19: Mesa from trixie-backports, pinned: ROL's choice, 15 Nov 2026)
mkdir -p $P/etc/apt/sources.list.d $P/etc/apt/preferences.d
install -m 644 $s/apt/debian-backports.sources $P/etc/apt/sources.list.d/
install -m 644 $s/apt/riscos-risc3d-mesa $P/etc/apt/preferences.d/
printf '%s\n' /etc/apt/sources.list.d/debian-backports.sources /etc/apt/preferences.d/riscos-risc3d-mesa > $P/DEBIAN/conffiles
install -m 755 $s/postinst $s/prerm $s/postrm $P/DEBIAN/
dpkg-deb --root-owner-group -Zxz --build $P "out/riscos-risc3d_${v}_arm64.deb"
