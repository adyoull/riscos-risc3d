#!/usr/bin/env python3
# gen.py OUTDIR: writes both ends of the RISC3D command stream from
# Khronos's registry (proto/khronos/gl.xml):
#   r3d_gl.h       types, enums and glXxx prototypes for the functions below
#   r3d_ops.h      the opcodes (one per function)
#   client_gen.c   the library's functions that need no hand-written code
#   server_gen.c   the Linux helper's table of the driver's functions and
#                  its dispatch for the same commands
#   r3d_gl_names.h glXxx -> r3d_glXxx, for test programs (R3D_DIRECT)
#
# The functions: OpenGL 1.0 to 2.1 (the compatibility profile, which is
# what riscos-mesa's ports use), OpenGL ES 2.0, and GL_ARB_framebuffer_object.
# The usual ARB and EXT names of the same functions are listed too, for
# Mesa's EGL to fill its table by name (r3d_gl_functions[]).
#
# A command is words: opcode, length in words (with these two), arguments.
# A double is two words. Arrays the function reads go after the other
# arguments: one word of length in bytes for each, then the data, each
# padded to a word. gl.xml's len= attributes give the lengths:
# COMPSIZE(pname) is r3d_count (pname) in lib/r3d_gl.c. Functions that fit
# no pattern here are written by hand at both ends (lib/r3d_gl.c,
# linux/rol-3d.c): HAND below.
#
# Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE).
# gl.xml is Copyright The Khronos Group Inc., Apache 2.0 (in the file).
import re, sys, os
import xml.etree.ElementTree as ET

here = os.path.dirname(os.path.abspath(__file__))
out = sys.argv[1]
reg = ET.parse(os.path.join(here, 'khronos', 'gl.xml')).getroot()

# --- what gl.xml says ---------------------------------------------------

commands = {}
for c in reg.find('commands'):
    p = c.find('proto')
    name = p.find('name').text
    ret = ''.join(p.itertext())
    ret = ret[:ret.rfind(name)].strip()
    params = []
    for q in c.findall('param'):
        n = q.find('name').text
        t = ''.join(q.itertext())
        t = t[:t.rfind(n)].strip()
        params.append((t, n, q.get('len')))
    al = c.find('alias')
    commands[name] = (ret, params, al.get('name') if al is not None else None)

want, enums_wanted = [], []
seen, seen_e = set(), set()

def take(req):
    for c in req.findall('command'):
        if c.get('name') not in seen:
            seen.add(c.get('name'))
            want.append(c.get('name'))
    for e in req.findall('enum'):
        if e.get('name') not in seen_e:
            seen_e.add(e.get('name'))
            enums_wanted.append(e.get('name'))

for f in reg.findall('feature'):
    api, num = f.get('api'), float(f.get('number'))
    if (api == 'gl' and num <= 2.1) or (api == 'gles2' and f.get('name') == 'GL_ES_VERSION_2_0'):
        for req in f.findall('require'):
            take(req)
ext = {e.get('name'): e for e in reg.find('extensions')}
for x in ('GL_ARB_framebuffer_object',):
    for req in ext[x].findall('require'):
        take(req)

# other names for the same functions (Mesa's EGL fills its table by name)
ALIAS_EXTS = [
    'GL_ARB_multitexture', 'GL_ARB_vertex_buffer_object', 'GL_ARB_transpose_matrix',
    'GL_ARB_point_parameters', 'GL_ARB_window_pos', 'GL_EXT_framebuffer_object',
    'GL_ARB_texture_compression', 'GL_ARB_occlusion_query', 'GL_EXT_blend_func_separate',
    'GL_EXT_blend_color', 'GL_EXT_blend_minmax', 'GL_EXT_texture3D',
    'GL_EXT_draw_range_elements', 'GL_EXT_multi_draw_arrays', 'GL_EXT_secondary_color',
    'GL_EXT_fog_coord', 'GL_EXT_texture_object', 'GL_ARB_shader_objects',
    'GL_ARB_vertex_shader', 'GL_EXT_blend_equation_separate', 'GL_ARB_draw_buffers',
    'GL_EXT_framebuffer_blit', 'GL_EXT_framebuffer_multisample', 'GL_EXT_copy_texture',
    'GL_EXT_subtexture',
]
def norm(params):
    m = {'GLhandleARB': 'GLuint', 'GLcharARB': 'GLchar', 'GLintptrARB': 'GLintptr',
         'GLsizeiptrARB': 'GLsizeiptr'}
    out = []
    for t, n, l in params:
        for k, v in m.items():
            t = re.sub(r'\b%s\b' % k, v, t)
        out.append(t.replace(' ', ''))
    return out
aliases = []
for x in ALIAS_EXTS:
    for req in ext[x].findall('require'):
        for c in req.findall('command'):
            n = c.get('name')
            target = commands[n][2]
            if target is None:
                # the EXT names that gl.xml doesn't call aliases: the same
                # function in a compatibility context
                m = re.match(r'(gl\w+?)(EXT)$', n)
                if m and m.group(1) in seen and x in ('GL_EXT_framebuffer_object', 'GL_EXT_texture_object'):
                    target = m.group(1)
            if target in seen and n not in seen and norm(commands[n][1]) == norm(commands[target][1]):
                aliases.append((n, target))

# --- how each function travels -------------------------------------------

HAND = {
    # strings, names and answers of their own shape
    'glBindAttribLocation', 'glGetAttribLocation', 'glGetUniformLocation',
    'glShaderSource', 'glGetShaderInfoLog', 'glGetProgramInfoLog',
    'glGetShaderSource', 'glGetActiveAttrib', 'glGetActiveUniform',
    'glGetAttachedShaders', 'glGetShaderPrecisionFormat', 'glGetString',
    'glShaderBinary', 'glAreTexturesResident',
    # state that is kept, or answered, on the RISC OS side
    'glGetBooleanv', 'glGetFloatv', 'glGetIntegerv', 'glGetDoublev',
    'glGetPointerv', 'glGetVertexAttribPointerv', 'glGetBufferPointerv',
    'glGenTextures', 'glGenBuffers', 'glGenLists', 'glBindFramebuffer',
    'glPushClientAttrib', 'glPopClientAttrib',
    # data
    'glBufferData', 'glBufferSubData', 'glGetBufferSubData', 'glMapBuffer', 'glUnmapBuffer',
    'glTexImage1D', 'glTexImage2D', 'glTexImage3D',
    'glTexSubImage1D', 'glTexSubImage2D', 'glTexSubImage3D',
    'glCompressedTexImage1D', 'glCompressedTexImage2D', 'glCompressedTexImage3D',
    'glCompressedTexSubImage1D', 'glCompressedTexSubImage2D', 'glCompressedTexSubImage3D',
    'glGetTexImage', 'glGetCompressedTexImage', 'glReadPixels', 'glDrawPixels',
    'glBitmap', 'glPolygonStipple', 'glGetPolygonStipple',
    'glMap1d', 'glMap1f', 'glMap2d', 'glMap2f', 'glCallLists',
    'glFeedbackBuffer', 'glSelectBuffer', 'glRenderMode',
    # vertex arrays (an array in the program's memory is sent with each draw)
    'glVertexAttribPointer', 'glVertexPointer', 'glColorPointer', 'glTexCoordPointer',
    'glNormalPointer', 'glIndexPointer', 'glEdgeFlagPointer', 'glFogCoordPointer',
    'glSecondaryColorPointer', 'glInterleavedArrays', 'glEnableClientState',
    'glDisableClientState', 'glClientActiveTexture', 'glArrayElement',
    'glDrawArrays', 'glDrawElements', 'glDrawRangeElements',
    'glMultiDrawArrays', 'glMultiDrawElements',
}
# the library notes these before sending them (lib/r3d_gl.c: r3d_track_*)
TRACK = {
    'glBindBuffer', 'glEnableVertexAttribArray', 'glDisableVertexAttribArray',
    'glPixelStorei', 'glPixelStoref', 'glDeleteBuffers',
    'glMatrixMode', 'glLoadIdentity', 'glLoadMatrixf', 'glLoadMatrixd', 'glMultMatrixf',
    'glMultMatrixd', 'glLoadTransposeMatrixf', 'glLoadTransposeMatrixd',
    'glMultTransposeMatrixf', 'glMultTransposeMatrixd', 'glPushMatrix', 'glPopMatrix',
    'glTranslatef', 'glTranslated', 'glRotatef', 'glRotated', 'glScalef', 'glScaled',
    'glOrtho', 'glFrustum', 'glActiveTexture', 'glViewport', 'glListBase',
    'glNewList', 'glEndList', 'glCallList', 'glPushAttrib', 'glPopAttrib',
    'glDeleteLists', 'glBindTexture', 'glBegin', 'glEnd',
}
# answers whose length only the driver knows: it marks the end
SENTINEL = {'glGetUniformfv', 'glGetUniformiv', 'glGetMapdv', 'glGetMapfv', 'glGetMapiv'}
PIXMAP = {'glGetPixelMapfv', 'glGetPixelMapuiv', 'glGetPixelMapusv'}

for n in HAND | TRACK | SENTINEL | PIXMAP:
    if n not in seen:
        raise SystemExit('gen.py: %s is not in the set' % n)

SIZES = {'GLdouble': 8, 'GLclampd': 8, 'GLfloat': 4, 'GLclampf': 4, 'GLint': 4, 'GLuint': 4,
         'GLenum': 4, 'GLsizei': 4, 'GLbitfield': 4, 'GLshort': 2, 'GLushort': 2,
         'GLbyte': 1, 'GLubyte': 1, 'GLboolean': 1, 'GLchar': 1, 'void': 1}

def is_ptr(t): return '*' in t
def is_const_ptr(t): return is_ptr(t) and t.startswith('const')
def base(t): return t.replace('const', '').replace('*', '').strip()
def is_d(t): return t in ('GLdouble', 'GLclampd')
def is_f(t): return t in ('GLfloat', 'GLclampf')
def words(t): return 2 if is_d(t) else 1

def len_expr(l):
    if l is None:
        raise ValueError
    if re.match(r'^\d+$', l):
        return l
    m = re.match(r'COMPSIZE\((\w+)\)$', l)
    if m and m.group(1) == 'pname':
        return 'r3d_count (pname)'
    if 'COMPSIZE' in l:
        raise ValueError
    return '(%s)' % l

def kind(name):
    ret, al, _ = commands[name]
    if name in HAND:
        return 'hand'
    if name in SENTINEL:
        return 'sentinel'
    if name in PIXMAP:
        return 'pixmap'
    ptrs = [p for p in al if is_ptr(p[0])]
    if not ptrs:
        return 'auto'
    if all(is_const_ptr(t) for t, _, _ in ptrs):
        for t, n, l in ptrs:
            len_expr(l)
        return 'in'
    if len(ptrs) == 1 and al[-1] == ptrs[0] and not is_ptr(ret):
        len_expr(ptrs[0][2])
        return 'out'
    raise SystemExit('gen.py: no rule for ' + name)

kinds = {n: kind(n) for n in want}

FIRST = 16
ops = [(n, FIRST + i) for i, n in enumerate(want)]

def cargs(al): return ', '.join('%s %s' % (t, n) for t, n, _ in al) or 'void'

# --- r3d_gl.h --------------------------------------------------------------

enum_values = {}
for es in reg.findall('enums'):
    for e in es.findall('enum'):
        if e.get('api') in (None, 'gl') and e.get('name') not in enum_values:
            enum_values[e.get('name')] = e.get('value')

with open(os.path.join(out, 'r3d_gl.h'), 'w') as f:
    f.write('/* Generated by proto/gen.py from Khronos\'s gl.xml: do not edit.\n'
            '   OpenGL 1.0 to 2.1 (compatibility), OpenGL ES 2.0 and\n'
            '   ARB_framebuffer_object: the functions RISC3D carries. */\n')
    f.write('#ifndef R3D_GL_H\n#define R3D_GL_H\n#include <KHR/khrplatform.h>\n')
    f.write('#define __gl_h_ 1\n#define __gl2_h_ 1\n#define __glext_h_ 1\n#define __gl2ext_h_ 1\n')
    f.write('''typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef khronos_int8_t GLbyte;
typedef khronos_uint8_t GLubyte;
typedef khronos_int16_t GLshort;
typedef khronos_uint16_t GLushort;
typedef int GLint;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef khronos_float_t GLfloat;
typedef khronos_float_t GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef char GLchar;
typedef khronos_int32_t GLfixed;
typedef khronos_intptr_t GLintptr;
typedef khronos_ssize_t GLsizeiptr;
''')
    for e in enums_wanted:
        if e in enum_values:
            f.write('#define %s %s\n' % (e, enum_values[e]))
    for n in want:
        ret, al, _ = commands[n]
        f.write('%s %s (%s);\n' % (ret, n, cargs(al)))
    f.write('#endif\n')

with open(os.path.join(out, 'r3d_ops.h'), 'w') as f:
    f.write('/* Generated by proto/gen.py from gl.xml: do not edit. */\n')
    f.write('#ifndef R3D_OPS_H\n#define R3D_OPS_H\n')
    f.write('#define R3D_PROTOCOL 2\n')
    f.write('enum {\n  OP_SURFACE = 1, OP_PRESENT, OP_CLIENT_ARRAY, OP_DRAW_ELEMENTS_INLINE,\n'
            '  OP_SYNC, OP_INFO, OP_CONTEXT, OP_FIXED_ARRAY, OP_LIMITS, OP_STATS,\n')
    for n, o in ops:
        f.write('  OP_%s = %d,\n' % (n, o))
    f.write('  OP_END\n};\n#endif\n')

# --- both ends ------------------------------------------------------------

def put_scalar(w, t, n):
    if is_d(t):
        return '  r3d_d2w (a + %d, %s);\n' % (w, n)
    if is_f(t):
        return '  a[%d] = r3d_f2u (%s);\n' % (w, n)
    return '  a[%d] = (uint32_t) %s;\n' % (w, n)

def get_scalar(w, t):
    if is_d(t):
        return 'r3d_w2d (a + %d)' % w
    if is_f(t):
        return 'r3d_u2f (a[%d])' % w
    if t in ('GLint', 'GLsizei', 'GLshort', 'GLbyte', 'GLintptr', 'GLsizeiptr', 'GLfixed'):
        return '(%s) (int32_t) a[%d]' % (t, w)
    return '(%s) a[%d]' % (t, w)

def layout(al):
    """word positions of the arguments that aren't arrays"""
    pos, w = {}, 0
    for t, n, _ in al:
        if not is_ptr(t):
            pos[n] = w
            w += words(t)
    return pos, w

c = open(os.path.join(out, 'client_gen.c'), 'w')
s = open(os.path.join(out, 'server_gen.c'), 'w')
c.write('/* Generated by proto/gen.py from gl.xml: do not edit. */\n')
s.write('/* Generated by proto/gen.py from gl.xml: do not edit. */\n')
s.write('static struct gl_funcs {\n')
for n in want:
    ret, al, _ = commands[n]
    s.write('  %s (*%s) (%s);\n' % (ret, n[2:], cargs(al)))
s.write('} gl;\n')
s.write('static const char *const gl_names[] = {\n')
for n in want:
    s.write('  "%s",\n' % n)
s.write('  0 };\n\n')
s.write('/* 1: done; 0: not one of these (hand-written) */\n')
s.write('static int\ndispatch_gen (uint32_t op, const uint32_t *a, uint32_t n)\n{\n  (void) n;\n  switch (op)\n    {\n')

def ret_fail(ret): return '' if ret == 'void' else ' 0'

for name in want:
    k = kinds[name]
    if k == 'hand':
        continue
    ret, al, _ = commands[name]
    op = 'OP_' + name
    track = '  r3d_track_%s (%s);\n' % (name, ', '.join(n for _, n, _ in al)) if name in TRACK else ''
    pos, nw = layout(al)
    call_args = []
    c.write('\n%s\n%s (%s)\n{\n' % (ret, name, cargs(al)))
    c.write(track)
    if k == 'auto':
        if nw:
            c.write('  uint32_t *a = r3d_put (%s, %d);\n' % (op, nw))
            c.write('  if (!a)\n    return%s;\n' % ret_fail(ret))
        else:
            c.write('  if (!r3d_put (%s, 0))\n    return%s;\n' % (op, ret_fail(ret)))
        for t, n, _ in al:
            c.write(put_scalar(pos[n], t, n))
        call = 'gl.%s (%s)' % (name[2:], ', '.join(get_scalar(pos[n], t) for t, n, _ in al))
        if ret == 'void':
            s.write('    case %s:\n      if (gl.%s)\n\t%s;\n      return 1;\n' % (op, name[2:], call))
        else:
            c.write('  uint32_t r = 0;\n  r3d_sync (&r, 4);\n  return (%s) r;\n' % ret)
            s.write('    case %s:\n      {\n\tuint32_t r = gl.%s ? (uint32_t) %s : 0;\n\treply (&r, 4);\n\treturn 1;\n      }\n' % (op, name[2:], call))
    elif k == 'in':
        ptrs = [(t, n, l) for t, n, l in al if is_ptr(t)]
        np_ = len(ptrs)
        consts = all(re.match(r'^\d+$', l) for _, _, l in ptrs)
        if consts:
            # small arrays of fixed length (glVertex3fv, glLoadMatrixf):
            # straight into the ring
            dw = sum((int(l) * SIZES[base(t)] + 3) // 4 for t, _, l in ptrs)
            c.write('  uint32_t *a = r3d_put (%s, %d);\n  if (!a)\n    return%s;\n' % (op, nw + np_ + dw, ret_fail(ret)))
            for t, n, _ in al:
                if not is_ptr(t):
                    c.write(put_scalar(pos[n], t, n))
            w = nw + np_
            for i, (t, n, l) in enumerate(ptrs):
                b = int(l) * SIZES[base(t)]
                c.write('  a[%d] = %d;\n' % (nw + i, b))
                if b % 4:
                    c.write('  a[%d] = 0;\n' % (w + (b + 3) // 4 - 1))
                c.write('  memcpy (a + %d, %s, %d);\n' % (w, n, b))
                w += (b + 3) // 4
        else:
            c.write('  uint32_t f[%d];\n' % (nw + np_))
            c.write('  uint32_t *a = f;\n')
            for t, n, _ in al:
                if not is_ptr(t):
                    c.write(put_scalar(pos[n], t, n))
            for i, (t, n, l) in enumerate(ptrs):
                c.write('  size_t b%d = %s ? (size_t) r3d_nonneg (%s) * %d : 0;\n' % (i, n, len_expr(l), SIZES[base(t)]))
                c.write('  f[%d] = (uint32_t) b%d;\n' % (nw + i, i))
            if np_ == 1:
                c.write('  r3d_put_data (%s, f, %d, %s, b0);\n' % (op, nw + np_, ptrs[0][1]))
            else:
                c.write('  size_t at = 0, all = %s;\n' % ' + '.join('(b%d + 3) / 4 * 4' % i for i in range(np_)))
                c.write('  char *g = malloc (all + 4);\n  if (!g)\n    return%s;\n' % ret_fail(ret))
                for i, (t, n, l) in enumerate(ptrs):
                    c.write('  memset (g + at, 0, (b%d + 3) / 4 * 4);\n  if (b%d)\n    memcpy (g + at, %s, b%d);\n  at += (b%d + 3) / 4 * 4;\n' % (i, i, n, i, i))
                c.write('  r3d_put_data (%s, f, %d, g, all);\n  free (g);\n' % (op, nw + np_))
        # the other end: the arrays follow the counts
        s.write('    case %s:\n      {\n' % op)
        s.write('\tconst char *d = (const char *) (a + %d);\n' % (nw + np_))
        for i, (t, n, l) in enumerate(ptrs):
            s.write('\tconst void *p%d = a[%d] ? d : 0;\n\td += (a[%d] + 3) / 4 * 4;\n' % (i, nw + i, nw + i))
        s.write('\t(void) d;\n')
        args = []
        pi = 0
        for t, n, _ in al:
            if is_ptr(t):
                args.append('(%s) p%d' % (t, pi))
                pi += 1
            else:
                args.append(get_scalar(pos[n], t))
        call = 'gl.%s (%s)' % (name[2:], ', '.join(args))
        if ret == 'void':
            s.write('\tif (gl.%s)\n\t  %s;\n\treturn 1;\n      }\n' % (name[2:], call))
        else:
            c.write('  uint32_t r = 0;\n  r3d_sync (&r, 4);\n  return (%s) r;\n' % ret)
            s.write('\tuint32_t r = gl.%s ? (uint32_t) %s : 0;\n\treply (&r, 4);\n\treturn 1;\n      }\n' % (name[2:], call))
    elif k in ('out', 'sentinel', 'pixmap'):
        pt, pn, pl = al[-1]
        es = SIZES[base(pt)]
        sc = al[:-1]
        if nw:
            c.write('  uint32_t *a = r3d_put (%s, %d);\n  if (!a)\n    return;\n' % (op, nw))
        else:
            c.write('  if (!r3d_put (%s, 0))\n    return;\n' % op)
        for t, n, _ in sc:
            c.write(put_scalar(pos[n], t, n))
        sargs = ', '.join([get_scalar(pos[n], t) for t, n, _ in sc] + ['(%s) r' % pt])
        if k == 'out':
            c.write('  uint32_t r[64];\n  memset (r, 0, sizeof r);\n  r3d_sync (r, sizeof r);\n')
            c.write('  size_t cnt = (size_t) r3d_nonneg (%s);\n' % len_expr(pl))
            c.write('  if (cnt * %d > sizeof r)\n    cnt = sizeof r / %d;\n' % (es, es))
            c.write('  if (%s)\n    memcpy (%s, r, cnt * %d);\n' % (pn, pn, es))
            s.write('    case %s:\n      {\n\tuint32_t r[64];\n\tmemset (r, 0, sizeof r);\n\tif (gl.%s)\n\t  gl.%s (%s);\n\treply (r, sizeof r);\n\treturn 1;\n      }\n' % (
                op, name[2:], name[2:], sargs))
        else:
            # the answer: a count, then that many values
            c.write('  uint32_t *r = malloc (4 + 4096 * %d);\n  if (!r)\n    return;\n  r[0] = 0;\n' % es)
            c.write('  r3d_sync (r, 4 + 4096 * %d);\n' % es)
            c.write('  if (%s && r[0] <= 4096)\n    memcpy (%s, r + 1, r[0] * %d);\n  free (r);\n' % (pn, pn, es))
            s.write('    case %s:\n      {\n' % op)
            if k == 'sentinel':
                s.write('\tuint32_t *r = scratch (4 + 4096 * %d);\n\tsentinel_fill (r + 1, 4096, %d);\n' % (es, es))
                s.write('\tif (gl.%s)\n\t  gl.%s (%s);\n' % (name[2:], name[2:], sargs.replace('(%s) r' % pt, '(%s) (r + 1)' % pt)))
                s.write('\tr[0] = sentinel_count (r + 1, 4096, %d);\n' % es)
            else:
                s.write('\tGLint size = 0;\n\tgl.GetIntegerv (a[0] + 0x40, &size);\n')
                s.write('\tif (size < 0 || size > 4096)\n\t  size = 0;\n')
                s.write('\tuint32_t *r = scratch (4 + 4096 * %d);\n' % es)
                s.write('\tif (gl.%s)\n\t  gl.%s (%s);\n' % (name[2:], name[2:], sargs.replace('(%s) r' % pt, '(%s) (r + 1)' % pt)))
                s.write('\tr[0] = (uint32_t) size;\n')
            s.write('\treply (r, 4 + r[0] * %d);\n\treturn 1;\n      }\n' % es)
    c.write('}\n')

s.write('    }\n  return 0;\n}\n')
# every forwarding function is r3d_glXxx (a program that also links Mesa
# has Mesa's own glXxx); Mesa's EGL builds a dispatch table from this list
c.write('\nconst struct r3d_gl_function r3d_gl_functions[] = {\n')
for n in want:
    c.write('  { "%s", (void (*) (void)) %s },\n' % (n, n))
for n, t in aliases:
    c.write('  { "%s", (void (*) (void)) %s },\n' % (n, t))
c.write('  { 0, 0 } };\n')
with open(os.path.join(out, 'r3d_gl_names.h'), 'w') as f:
    f.write('/* Generated by proto/gen.py from gl.xml: do not edit.\n')
    f.write('   Include after r3d_gl.h (or GL/gl.h): glXxx becomes RISC3D\'s r3d_glXxx. */\n')
    f.write('#ifndef R3D_GL_NAMES_H\n#define R3D_GL_NAMES_H\n')
    for n in want:
        ret, al, _ = commands[n]
        f.write('%s r3d_%s (%s);\n#define %s r3d_%s\n' % (ret, n, cargs(al), n, n))
    f.write('#endif\n')
c.close()
s.close()
print('%d functions (%d by hand), %d other names for them' % (len(want), len(HAND), len(aliases)))
