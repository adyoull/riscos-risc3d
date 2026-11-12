# Test scaffolding

For testing under QEMU, where there is no GPU and (from the build
container) no Debian mirror: a 64-bit ARM Mesa with a software renderer,
from conda-forge, placed in the test image at `/opt/riscos/gl`, which
`rol-3d` looks in first. Not for shipping (about 200 MB).

1. Fetch each file named in `conda-mesa-aarch64.txt` from
   `https://conda.anaconda.org/conda-forge/linux-aarch64/`.
2. `python3 unconda.py root *.conda` unpacks them (needs the `zstandard`
   Python module).
3. Keep `root/lib` (the shared libraries `libEGL.so.1`, `libGLESv2.so.2`,
   `libgbm.so.1` and what they need, `dri/`, `gbm/`) and
   `root/share/glvnd`, `root/share/drirc.d`; strip the libraries.
   `lib/dri/swrast_dri.so` and `kms_swrast_dri.so` (links to
   `libdril_dri.so`, from the mesa-llvmpipe package) must be there or EGL
   offers no configurations.
4. Write the tree into the image as `/opt/riscos/gl` (mode 755).

This build of Mesa has desktop OpenGL only (no GLES): `rol-3d` then makes
a desktop OpenGL context. `rol-3d --check` says what would draw;
`ROL3D_DEBUG=1` lists EGL's configurations when none fits.
