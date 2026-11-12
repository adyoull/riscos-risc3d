#!/usr/bin/env python3
# mkkit.py ZIP FOLDER NAME,TYPE...: a zip for RISC OS, each file with its
# file type (the ",xxx" ending, which is dropped from its name) in the
# Acorn extra field, as SparkFS and InfoZip on RISC OS read it.
# Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).
import os, struct, sys, time, zipfile

out, folder = sys.argv[1], sys.argv[2]
now = time.time()
# RISC OS time: centiseconds since 1900
cs = int((now + 2208988800) * 100)
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for path in sys.argv[3:]:
        name = os.path.basename(path)
        ftype = 0xfff
        if len(name) > 4 and name[-4] == ',':
            ftype = int(name[-3:], 16)
            name = name[:-4]
        load = 0xfff00000 | (ftype << 8) | ((cs >> 32) & 0xff)
        exe = cs & 0xffffffff
        attr = 0x13				# owner read and write, public read
        info = zipfile.ZipInfo(folder + '/' + name, time.localtime(now)[:6])
        info.compress_type = zipfile.ZIP_DEFLATED
        info.extra = struct.pack('<HH4sIIII', 0x4341, 20, b'ARC0', load, exe, attr, 0)
        info.external_attr = 0o644 << 16
        z.writestr(info, open(path, 'rb').read())
