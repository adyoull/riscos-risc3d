# RISC OS sprite files: 32bpp new-format sprites with 8-bit alpha masks
# (RISC OS 5.21+ "wide mask": mode word bit 31).
import struct, sys
from PIL import Image
def sprite(name, png, dpi):
    im=Image.open(png).convert('RGBA'); w,h=im.size
    px=im.load()
    img=b''.join(struct.pack('<I', px[x,y][0] | px[x,y][1]<<8 | px[x,y][2]<<16) for y in range(h) for x in range(w))
    mrow=(w+3)&~3
    mask=b''.join(bytes(px[x,y][3] for x in range(w))+b'\0'*(mrow-w) for y in range(h))
    mode=(1<<31)|(6<<27)|(dpi<<14)|(dpi<<1)|1
    hdr=44
    size=hdr+len(img)+len(mask)
    nm=name.lower().encode().ljust(12,b'\0')
    return struct.pack('<I12sIIIIIII', size, nm, w-1, h-1, 0, 31, hdr, hdr+len(img), mode)+img+mask
def area(sprites, out):
    body=b''.join(sprites)
    # file = sprite area without its first word: count, first (16), free
    open(out,'wb').write(struct.pack('<III', len(sprites), 16, 16+len(body))+body)
area([sprite('!risc3d','icon34.png',90), sprite('sm!risc3d','icon18.png',90)], '!Sprites,ff9')
area([sprite('!risc3d','icon68.png',180), sprite('sm!risc3d','icon36.png',180)], '!Sprites11,ff9')
