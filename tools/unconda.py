import sys,zipfile,tarfile,io,zstandard,os
dest=sys.argv[1]
for f in sys.argv[2:]:
    z=zipfile.ZipFile(f)
    for n in z.namelist():
        if n.startswith('pkg-') and n.endswith('.tar.zst'):
            data=zstandard.ZstdDecompressor().stream_reader(io.BytesIO(z.read(n)))
            t=tarfile.open(fileobj=data,mode='r|')
            t.extractall(dest)
