from PIL import Image, ImageDraw, ImageFont
S=2; W,H=1400*S,1060*S
im=Image.new('RGB',(W,H),'white'); d=ImageDraw.Draw(im)
B='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'; R='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
fb=ImageFont.truetype(B,26*S); fr=ImageFont.truetype(R,19*S); fs=ImageFont.truetype(R,17*S); ft=ImageFont.truetype(B,34*S)
NAVY=(10,18,48); BLUE=(30,110,215); LBLUE=(225,238,255); CYAN=(214,246,255); GREY=(242,242,245); GREEN=(228,246,232); AMB=(255,243,222)
def box(x,y,w,h,title,lines,fill,edge=NAVY):
    x,y,w,h=[v*S for v in (x,y,w,h)]
    d.rounded_rectangle([x,y,x+w,y+h],radius=14*S,fill=fill,outline=edge,width=3*S)
    tw=d.textlength(title,font=fb); d.text((x+(w-tw)/2,y+12*S),title,font=fb,fill=NAVY)
    yy=y+50*S
    for l in lines:
        lw=d.textlength(l,font=fr); d.text((x+(w-lw)/2,yy),l,font=fr,fill=(40,48,70)); yy+=26*S
def arrow(x0,y0,x1,y1,label=None,col=BLUE,lx=0,ly=0):
    x0,y0,x1,y1=[v*S for v in (x0,y0,x1,y1)]
    d.line([x0,y0,x1,y1],fill=col,width=5*S)
    import math
    a=math.atan2(y1-y0,x1-x0); L=18*S
    p=[(x1,y1),(x1-L*math.cos(a-0.45),y1-L*math.sin(a-0.45)),(x1-L*math.cos(a+0.45),y1-L*math.sin(a+0.45))]
    d.polygon(p,fill=col)
    if label:
        d.text(((x0+x1)/2+lx*S,(y0+y1)/2+ly*S),label,font=fs,fill=(70,80,110))
t='How RISC3D works'; d.text(((W-d.textlength(t,font=ft))/2,20*S),t,font=ft,fill=NAVY)
box(400,75,600,105,'A program',['ClassiCube, TORCS, YSCE, Warzone 2100 ...','drawing with OpenGL or OpenGL ES'],GREY)
box(400,215,600,105,'riscos-mesa (!MesaLibs)',['EGL and OSMesa: the GPU where RISC3D','has a driver, else Mesa in software'],LBLUE)
box(400,355,600,130,'RISC3D module',['in !Boot.Resources.!RISC3D, loaded at start-up;','loads the first driver in !RISC3D.GPU that starts;','keeps drivers and contexts'],CYAN)
arrow(700,180,700,213); arrow(700,320,700,353)
# software fallback
box(1060,215,300,105,'No GPU driver?',['Mesa draws in software,','as before'],GREY,edge=(150,150,160))
arrow(1000,268,1058,268,col=(150,150,160))
# left: ROL
d.text((70*S,522*S),'RISC OS on Linux',font=fb,fill=BLUE)
box(60,570,560,105,'Linux3D (driver)',['GL calls written as commands into memory','shared with Linux: no SWI per call'],GREEN)
box(60,710,560,105,'rol-3d (Linux)',["replays the commands with Linux's Mesa","(Debian's 26.1); frames straight to the screen"],GREEN)
box(60,850,560,130,"Linux's GPU driver",['Pi 4: v3d. Untested: Pinebook Pro (panfrost),','Pinebook (lima), Windows Dev Kit 2023','(Adreno, freedreno). Else software.'],GREEN)
d.line([700*S,487*S,700*S,505*S],fill=BLUE,width=5*S); d.line([340*S,505*S,1060*S,505*S],fill=BLUE,width=5*S); arrow(340,505,340,568); arrow(340,675,340,708); arrow(340,815,340,848)
# right: native
d.text((680*S,522*S),'Native RISC OS on a Pi 4',font=fb,fill=BLUE)
box(780,570,560,105,'Mesa v3d driver (in the program)',["Mesa's own Pi 4 GPU driver, in riscos-mesa's","GPU library; finds V3DRender through RISC3D"],AMB)
box(780,710,560,105,'V3DRender (driver)',["after Linux's v3d: GPU memory and MMU,","job queues, interrupts, resets"],AMB)
box(780,850,560,105,'V3D 4.2 GPU',['Raspberry Pi 4, 400, CM4'],AMB)
arrow(1060,505,1060,568); arrow(1060,675,1060,708,'Linux-style requests',lx=12,ly=-14); arrow(1060,815,1060,848)
foot='Programs need nothing in their !Run: !RISC3D loads RISC3D, and RISC3D loads the driver for the machine.'
d.text(((W-d.textlength(foot,font=fr))/2,1012*S),foot,font=fr,fill=(70,80,110))
im=im.resize((W//S,H//S),Image.LANCZOS); im.save('docs/images/risc3d-how-it-works.png')
