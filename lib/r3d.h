/* r3d.h: RISC3D for programs. GLES 2 (include <GLES2/gl2.h>) drawn by
   whatever backend the RISC3D module has: on RISC OS on Linux, Linux's
   graphics driver.

   A program opens one context, gives it a surface size, draws with the gl
   functions, and takes each finished frame as pixels (r3d_swap): 32 bits a
   pixel in a RISC OS sprite's order (&BBGGRR words), top row first.

   PROTOTYPE: the SWI numbers are provisional and the interface will change.

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#ifndef R3D_H
#define R3D_H
#include <stddef.h>

#define RISC3D_Info	0xC3D00		/* provisional chunk */
#define RISC3D_Register	0xC3D01
#define RISC3D_Open	0xC3D02
#define RISC3D_Close	0xC3D03
#define RISC3D_Sync	0xC3D04

/* 0, or a RISC OS error's text (no module, no backend, no Linux helper) */
const char *r3d_open (void);
void r3d_close (void);
/* the size of what is drawn on, in pixels */
void r3d_surface (int width, int height);
/* finishes the frame and copies it out: width * height * 4 bytes.
   Returns the bytes copied, or -1 */
int r3d_swap (void *pixels, size_t bytes);
/* lets the renderer see everything so far (nothing comes back) */
void r3d_flush (void);
/* the last RISC OS error from the module, or 0 */
const char *r3d_error (void);

#endif
