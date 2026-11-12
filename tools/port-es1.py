#!/usr/bin/env python3
# tools/port-es1.py MESA_SRC: writes linux/es1_conversion.c from Mesa
# 20.3.5's src/mesa/main/es1_conversion.c (GLES 1.1's fixed-point and float
# functions in terms of desktop OpenGL's), as calls on rol-3d's gl.Xxx.
# Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).
import re, sys, os
src = open(os.path.join(sys.argv[1], 'src/mesa/main/es1_conversion.c')).read()
body = src[src.index('void GL_APIENTRY'):]
def drop(fn, text):
    i = text.index('void GL_APIENTRY\n%s(' % fn)
    j = text.index('\n}\n', i) + 3
    return text[:i] + text[j:]
# (DrawTex isn't one riscos-mesa offers; Color4ub is OpenGL's own)
for fn in ('_mesa_DrawTexxOES', '_mesa_DrawTexxvOES', '_es_Color4ub'):
    body = drop(fn, body)
defined = re.findall(r'(?:void GL_APIENTRY|static \w+)\n(\w+)\(', body)
def new(d):
    return 'es1_' + re.sub(r'^_(mesa|es)_', '', d)
for d in sorted(defined, key=len, reverse=True):
    body = re.sub(r'\b%s\b' % d, new(d), body)
body = body.replace('_mesa_error(_mesa_get_current_context(), ', 'ES1_ERROR(')
body = body.replace('_mesa_DepthRangef', 'es1_depth_range').replace('_mesa_ClearDepthf', 'es1_clear_depth')
body = re.sub(r'\b_(mesa|es)_(\w+)\(', r'gl.\2(', body)
body = body.replace('void GL_APIENTRY\n', 'static void\n')
body = re.sub(r'static (\w+)\n(es1__check)', r'static __attribute__ ((unused)) \1\n\2', body)
names = [new(d)[4:] for d in defined if not d.startswith('_check')]
hdr = '''/* linux/es1_conversion.c: OpenGL ES 1.1's own functions on a desktop
   OpenGL (compatibility profile) context, for rol-3d (#included).

   Debian's Mesa is built without GLES 1 (-Dgles1=disabled, and since Mesa
   23.1 that leaves no GLES 1 contexts at all), so GLES 1.1 runs on the
   desktop compatibility profile, which has everything GLES 1.1 draws with.
   These are Mesa 20.3.5's own conversions (src/mesa/main/es1_conversion.c,
   MIT licence, Copyright the Mesa authors), the same that riscos-mesa's
   software renderer uses, as calls on the driver's functions (gl.Xxx).
   Made by tools/port-es1.py: don't edit by hand. */
#include <stdbool.h>
#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif
#ifndef GL_TEXTURE_CROP_RECT_OES
#define GL_TEXTURE_CROP_RECT_OES 0x8B9D
#endif
#define ES1_ERROR(...) ((void) 0)
static void es1_depth_range (GLclampf n, GLclampf f) { gl.DepthRange (n, f); }
static void es1_clear_depth (GLclampf d) { gl.ClearDepth (d); }

'''
tail = '\n/* the functions above, in place of the driver\'s of the same names */\n#define ES1_FUNCTIONS(X) \\\n' + \
       ' \\\n'.join('  X (%s)' % n for n in names) + '\n'
open(os.path.join(os.path.dirname(__file__), '..', 'linux', 'es1_conversion.c'), 'w').write(hdr + body + tail)
print(len(names), 'functions')
