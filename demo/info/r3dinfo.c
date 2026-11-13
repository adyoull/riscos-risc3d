/* r3dinfo: the 3D driver a new context would get, and whether it draws in
   software (RISC3D 0.8: r3d_driver_flags; RISC3D module 0.03, Linux3D
   0.02, rol-3d 0.7). Copyright (c) 2026 Andrew Youll. MIT licence. */
#include <stdio.h>
#include "r3d.h"

int
main (void)
{
  int f = r3d_driver_flags ();
  if (f < 0)
    printf ("driver: none\n");
  else
    printf ("driver flags: %d (%s%s)\n", f, f & R3D_DRIVER_DRM ? "a DRM device, " : "",
	    f & R3D_DRIVER_SOFTWARE ? "draws in software" : "draws on a GPU, or not known");
  printf ("r3d_available: %d\n", r3d_available ());
  return 0;
}
