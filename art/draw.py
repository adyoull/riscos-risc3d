import math, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

TOP=((126,226,255),(64,180,240))     # light cyan -> sky
LEFT=((40,120,220),(22,74,170))      # blue
RIGHT=((24,56,140),(14,30,90))       # deep navy
OUT=(10,18,48)
MESH_L=(120,190,255); MESH_R=(70,110,200); MESH_T=(220,248,255)

def lerp(a,b,t): return tuple(int(a[i]+(b[i]-a[i])*t) for i in range(3))

def grad_poly(img, poly, c0, c1, vertical=True):
    W,H=img.size
    m=Image.new('L',(W,H),0); ImageDraw.Draw(m).polygon(poly,fill=255)
    ys=[p[1] for p in poly]; xs=[p[0] for p in poly]
    g=Image.new('RGB',(W,H))
    d=ImageDraw.Draw(g)
    if vertical:
        y0,y1=min(ys),max(ys)
        for y in range(int(y0),int(y1)+1):
            d.line([(0,y),(W,y)],fill=lerp(c0,c1,(y-y0)/max(1,y1-y0)))
    else:
        x0,x1=min(xs),max(xs)
        for x in range(int(x0),int(x1)+1):
            d.line([(x,0),(x,H)],fill=lerp(c0,c1,(x-x0)/max(1,x1-x0)))
    img.paste(g,(0,0),m)

def cube(img, cx, cy, s, line, mesh=True, n=3):
    k=0.866
    T=(cx,cy-s); UL=(cx-k*s,cy-s/2); UR=(cx+k*s,cy-s/2); C=(cx,cy)
    LL=(cx-k*s,cy+s/2); LR=(cx+k*s,cy+s/2); B=(cx,cy+s)
    d=ImageDraw.Draw(img)
    # soft shadow
    sh=Image.new('L',img.size,0)
    ImageDraw.Draw(sh).ellipse([cx-k*s*1.05,cy+s*0.78,cx+k*s*1.05,cy+s*1.12],fill=110)
    sh=sh.filter(ImageFilter.GaussianBlur(s*0.06))
    img.paste((0,0,0),(0,0),sh)
    grad_poly(img,[T,UR,C,UL],*TOP)
    grad_poly(img,[UL,C,B,LL],*LEFT)
    grad_poly(img,[C,UR,LR,B],*RIGHT)
    if mesh:
        w=max(1,int(line*0.45))
        def P(a,b,c,u,v): # point on face a + u*(b-a) + v*(c-a)
            return (a[0]+u*(b[0]-a[0])+v*(c[0]-a[0]), a[1]+u*(b[1]-a[1])+v*(c[1]-a[1]))
        for face,col in (((UL,C,LL),MESH_L),((UR,C,LR),MESH_R),((UL,T,C),MESH_T)):
            a,b,c=face
            for i in range(1,n):
                t=i/n
                d.line([P(a,b,c,t,0),P(a,b,c,t,1)],fill=col,width=w)
                d.line([P(a,b,c,0,t),P(a,b,c,1,t)],fill=col,width=w)
            for i in range(1,2*n):
                t=i/n
                p0=P(a,b,c,min(t,1),max(0,t-1)); p1=P(a,b,c,max(0,t-1),min(t,1))
                d.line([p0,p1],fill=col,width=w)
    # highlight edges and outline
    d.line([UL,C,UR],fill=(235,252,255),width=max(1,int(line*0.8)))
    d.line([C,B],fill=(150,200,255),width=max(1,int(line*0.6)))
    d.polygon([T,UR,LR,B,LL,UL],outline=OUT,width=line)
    d.line([T,UR,LR,B,LL,UL,T,UR],fill=OUT,width=line,joint='curve')

def icon(size, ss=8):
    S=size*ss
    img=Image.new('RGBA',(S,S),(0,0,0,0))
    base=Image.new('RGB',(S,S),(0,0,0))
    alpha=Image.new('L',(S,S),0)
    # draw onto RGB then derive alpha from a separate silhouette pass
    cube(base,S/2,S*0.47,S*0.42,line=max(ss,int(S*0.032)),mesh=size>=24, n=3 if size>=48 else 2)
    k=0.866;s=S*0.42;cx=S/2;cy=S*0.47
    ad=ImageDraw.Draw(alpha)
    hexp=[(cx,cy-s),(cx+k*s,cy-s/2),(cx+k*s,cy+s/2),(cx,cy+s),(cx-k*s,cy+s/2),(cx-k*s,cy-s/2)]
    ad.polygon(hexp,fill=255)
    ad.line(hexp+[hexp[0],hexp[1]],fill=255,width=max(ss,int(S*0.032)),joint='curve')
    sh=Image.new('L',(S,S),0)
    ImageDraw.Draw(sh).ellipse([cx-k*s*1.05,cy+s*0.78,cx+k*s*1.05,cy+s*1.12],fill=110)
    sh=sh.filter(ImageFilter.GaussianBlur(s*0.06))
    from PIL import ImageChops
    alpha=ImageChops.lighter(alpha,sh)
    img=base.convert('RGBA'); img.putalpha(alpha)
    return img.resize((size,size),Image.LANCZOS)

def logo(W=1200,H=400):
    ss=2
    img=Image.new('RGBA',(W*ss,H*ss),(255,255,255,0))
    base=Image.new('RGB',(W*ss,H*ss),(255,255,255))
    cube(base,H*ss*0.5,H*ss*0.47,H*ss*0.40,line=int(H*ss*0.022))
    d=ImageDraw.Draw(base)
    f=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',int(H*ss*0.42))
    x=H*ss*0.98; y=H*ss*0.25
    d.text((x,y),'RISC',font=f,fill=OUT)
    x+=d.textlength('RISC',font=f)
    d.text((x,y),'3D',font=f,fill=(30,110,215))
    fs=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',int(H*ss*0.085))
    d.text((H*ss*1.0,H*ss*0.72),'GPU drawing for RISC OS',font=fs,fill=(70,80,110))
    return base.resize((W,H),Image.LANCZOS)

if __name__=='__main__':
    for n in (34,18,68,36):
        icon(n).save(f'icon{n}.png')
    logo().save('RISC3D-logo.png')
    icon(512,ss=2).save('RISC3D-icon-512.png')
    # preview sheet: icons at 4x nearest on desktop grey
    sheet=Image.new('RGB',(560,320),(221,221,221))
    xs=10
    for n in (34,18,68,36):
        im=Image.open(f'icon{n}.png').resize((n*4,n*4),Image.NEAREST)
        sheet.paste(im,(xs,10),im); xs+=n*4+10
    xs=10
    for n in (34,18,68,36):
        im=Image.open(f'icon{n}.png')
        sheet.paste(im,(xs,290-n),im); xs+=n+20
    sheet.save('preview.png')
