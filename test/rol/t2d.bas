REM Linux2D test: big copies and fills in three depths; screen checksum
REM and times. Run once without Linux2D and once with; the sums must match.
REM (The text cursor is turned off: it flashes on a timer, which would
REM change the sum.) With "*Linux2D limits 1 1" first, every operation
REM goes to rol-2d.
ON ERROR PRINT REPORT$;" at ";ERL:SYS "IXSupport_LinuxSyscall",1,,,,,,,248
DIM code% 256, vb% 64
FOR pass%=0 TO 2 STEP 2
P%=code%
[OPT pass%
.sum            ; r0 = address, r1 = words -> r0 = checksum
 MOV R2,#0
.sl
 LDR R3,[R0],#4
 ADD R2,R3,R2,ROR #27
 SUBS R1,R1,#1
 BNE sl
 MOV R0,R2
 MOV PC,R14
]
NEXT
FOR m%=0 TO 2
READ md$
MODE md$:VDU 23,1,0;0;0;0;
SYS "OS_ReadModeVariable",-1,9 TO ,,l2%
SYS "OS_ReadModeVariable",-1,6 TO ,,ll%
SYS "OS_ReadModeVariable",-1,12 TO ,,yl%
REM a pattern of small rectangles (the kernel's) to move about
FOR y%=0 TO 2100 STEP 60:FOR x%=0 TO 3800 STEP 90
GCOL (x%*7+y%*13) MOD 256 TINT 0
RECTANGLE FILL x%+2,y%+2,40+(y% MOD 70),30+(x% MOD 23)
NEXT:NEXT
t%=TIME
REM copies in every direction, overlapping (scrolls, window drags)
RECTANGLE 0,8,3800,2000 TO 0,0
RECTANGLE 0,0,3800,2000 TO 0,12
RECTANGLE 4,0,3000,1500 TO 100,30
RECTANGLE 300,300,3000,1500 TO 6,2
RECTANGLE 10,10,1000,800 TO 2000,900
REM odd sizes and positions
RECTANGLE 6,6,1777,931 TO 1003,513
REM fills, solid, various colours and odd edges
GCOL 3 TINT 0:RECTANGLE FILL 2,2,3001,1203
GCOL 200 TINT 0:RECTANGLE FILL 1006,404,1233,1415
GCOL 45 TINT 0:RECTANGLE FILL 0,0,3838,2158
GCOL 7:RECTANGLE FILL 14,22,999,333
FOR y%=0 TO 2100 STEP 60:FOR x%=0 TO 3800 STEP 90
GCOL (x%*5+y%*3) MOD 256 TINT 0
RECTANGLE FILL x%+2,y%+2,40+(y% MOD 70),30+(x% MOD 23)
NEXT:NEXT
RECTANGLE 0,8,3800,2000 TO 0,0
RECTANGLE 600,600,2000,1000 TO 602,598
t%=TIME-t%
!vb%=148:vb%!4=-1:SYS "OS_ReadVduVariables",vb%,vb%+16
A%=vb%!16:B%=ll%*(yl%+1) DIV 4
s%=USR sum
REM timing: big ones only
N%=100
t1%=TIME:FOR i%=1 TO N%:RECTANGLE FILL 0,0,3800,2100:NEXT:t1%=TIME-t1%
t2%=TIME:FOR i%=1 TO N%:RECTANGLE 0,8,3800,2000 TO 0,0:NEXT:t2%=TIME-t2%
OSCLI "Echo"
PRINT "RES: ";md$;" log2bpp ";l2%;" sum &";~s%;" mixed ";t%;" cs, fill ";t1%*10/N%;" ms, copy ";t2%*10/N%;" ms"
NEXT
OSCLI "RMEnsure Linux2D 0 Error none"
VDU 4:OSCLI "Linux2D"
SYS "IXSupport_LinuxSyscall",0,,,,,,,248
DATA X1920 Y1080 C16M, X1920 Y1080 C64K, X1920 Y1080 C256
