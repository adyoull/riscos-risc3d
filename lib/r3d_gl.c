/* r3d_gl.c: the GLES 2 functions for RISC OS programs, as commands for the
   RISC3D module (which hands them to its backend). Functions that are only
   numbers in and out are generated (out/gen/client_gen.c, from gl2.h);
   the ones with strings, pixels and arrays are here.

   Commands are written straight into memory shared with the renderer
   (proto/r3d_shared.h): no SWI and no system call for a command. The
   module is called only to wake a renderer that has gone to sleep and to
   wait for an answer (a query, the end of a frame).

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>
#include <kernel.h>
#include <GLES2/gl2.h>
#include "r3d.h"
#include "r3d_ops.h"
#include "r3d_shared.h"

#define MAX_ATTRIBS 16

static struct r3d_area *area;
static uint32_t *ring;
static uint32_t ring_words;
static int handle = -1;
static const char *last_error;
static char error_text[256];

static struct attrib
{
  int enabled, client;			/* client: its pointer is the program's memory */
  GLint size;
  GLenum type;
  GLboolean normalized;
  GLsizei stride;
  const void *pointer;
} attribs[MAX_ATTRIBS];
static GLuint array_buffer, element_buffer;
static int unpack_align = 4, pack_align = 4;

static uint32_t r3d_f2u (float f) { union { float f; uint32_t u; } v = { f }; return v.u; }

/* the module: 0 wakes the renderer, 1 waits until it has done everything */
static int
kick (int wait)
{
  _kernel_oserror *e = _swix (RISC3D_Sync, _INR (0, 1), handle, wait);
  if (e)
    {
      strncpy (error_text, e->errmess, sizeof error_text - 1);
      last_error = error_text;
      return -1;
    }
  return 0;
}

/* Commands are written at local_head and shown to the renderer (the
   shared head moved) in batches: when a sixteenth of the ring is waiting,
   at glFlush, at r3d_flush and before any wait. It is woken only if it
   has gone to sleep. */
static uint32_t local_head;

static void
publish (void)
{
  if (area->head == local_head)
    return;
  __atomic_store_n (&area->head, local_head, __ATOMIC_SEQ_CST);
  if (__atomic_load_n (&area->sleeping, __ATOMIC_SEQ_CST))
    kick (0);
}

void
r3d_flush (void)
{
  if (handle >= 0)
    publish ();
}

/* room for a command of n argument words: its arguments, or 0 */
static uint32_t *
r3d_put (uint32_t op, size_t n)
{
  if (handle < 0 || n + 2 > ring_words / 2)
    return 0;
  /* the renderer is shown commands a batch at a time (and always before a
     wait): shown one by one, a renderer faster than the program goes to
     sleep between them and has to be woken for each */
  if (local_head - area->head > ring_words / 16 || op == OP_glFlush || op == OP_glFinish)
    publish ();
  uint32_t need = (uint32_t) n + 2;
  uint32_t at = local_head & (ring_words - 1);
  uint32_t skip = at + need > ring_words ? ring_words - at : 0;
  if (ring_words - (local_head - __atomic_load_n (&area->tail, __ATOMIC_SEQ_CST)) < need + skip)
    if (kick (1) < 0)			/* full: let the renderer catch up */
      return 0;
  if (skip)
    {
      ring[at] = OP_WRAP;
      local_head += skip;
      at = 0;
    }
  uint32_t *a = ring + at;
  a[0] = op;
  a[1] = need;
  local_head += need;
  return a + 2;
}

/* a command with fixed words and then data of any size (padded to words).
   Data too big for one command goes ahead of it in pieces (OP_BLOB), and
   the renderer puts it after the fixed words. */
static void
r3d_put_data (uint32_t op, const uint32_t *fixed, size_t nfixed, const void *data, size_t bytes)
{
  size_t dw = data ? (bytes + 3) / 4 : 0;
  size_t piece = ring_words / 8;
  if (handle < 0)
    return;
  if (nfixed + dw <= piece)
    {
      uint32_t *a = r3d_put (op, nfixed + dw);
      if (!a)
	return;
      memcpy (a, fixed, nfixed * 4);
      if (dw)
	{
	  a[nfixed + dw - 1] = 0;
	  memcpy (a + nfixed, data, bytes);
	}
      return;
    }
  for (size_t at = 0; at < bytes; )
    {
      size_t n = bytes - at < piece * 4 ? bytes - at : piece * 4;
      uint32_t *a = r3d_put (OP_BLOB, 1 + (n + 3) / 4);
      if (!a)
	return;
      a[0] = (uint32_t) n;
      a[(n + 3) / 4] = 0;
      memcpy (a + 1, (const char *) data + at, n);
      at += n;
    }
  uint32_t *a = r3d_put (op | R3D_WITH_BLOB, nfixed);
  if (a)
    memcpy (a, fixed, nfixed * 4);
}

/* waits for everything to be done, and takes the last command's answer */
static int
r3d_sync (void *reply, size_t max)
{
  if (handle < 0)
    return -1;
  area->reply_len = 0;
  if (area->head != local_head)
    __atomic_store_n (&area->head, local_head, __ATOMIC_SEQ_CST);
  if (kick (1) < 0)
    return -1;
  uint32_t n = area->reply_len;
  if (reply && n)
    memcpy (reply, (const char *) area + area->reply_offset, n < max ? n : max);
  return (int) n;
}

const char *
r3d_error (void)
{
  return last_error;
}

const char *
r3d_open (void)
{
  if (handle >= 0)
    return 0;
  unsigned bytes = 0;
  _kernel_oserror *e = _swix (RISC3D_Open, _IN (0) | _OUTR (0, 2), 0, &handle, &area, &bytes);
  if (e)
    {
      handle = -1;
      strncpy (error_text, e->errmess, sizeof error_text - 1);
      return last_error = error_text;
    }
  if (!area || area->magic != R3D_MAGIC)
    {
      _swix (RISC3D_Close, _IN (0), handle);
      handle = -1;
      return last_error = "RISC3D's backend gave no shared memory";
    }
  ring = (uint32_t *) ((char *) area + area->ring_offset);
  ring_words = area->ring_words;
  local_head = area->head;
  memset (attribs, 0, sizeof attribs);
  array_buffer = element_buffer = 0;
  unpack_align = pack_align = 4;
  return 0;
}

void
r3d_close (void)
{
  if (handle < 0)
    return;
  publish ();
  _swix (RISC3D_Close, _IN (0), handle);
  handle = -1;
  area = 0;
}

void
r3d_surface (int width, int height)
{
  uint32_t *a = r3d_put (OP_SURFACE, 2);
  if (a)
    {
      a[0] = (uint32_t) width;
      a[1] = (uint32_t) height;
    }
}

int
r3d_swap (void *pixels, size_t bytes)
{
  if (!r3d_put (OP_PRESENT, 0))
    return -1;
  return r3d_sync (pixels, bytes);
}

/* --- what the library has to remember ---------------------------------- */

static void
r3d_track_glBindBuffer (GLenum target, GLuint buffer)
{
  if (target == GL_ARRAY_BUFFER)
    array_buffer = buffer;
  else if (target == GL_ELEMENT_ARRAY_BUFFER)
    element_buffer = buffer;
}

static void
r3d_track_glDeleteBuffers (GLsizei n, const GLuint *buffers)
{
  for (GLsizei i = 0; i < n; i++)
    {
      if (buffers[i] == array_buffer)
	array_buffer = 0;
      if (buffers[i] == element_buffer)
	element_buffer = 0;
    }
}

static void
r3d_track_glEnableVertexAttribArray (GLuint index)
{
  if (index < MAX_ATTRIBS)
    attribs[index].enabled = 1;
}

static void
r3d_track_glDisableVertexAttribArray (GLuint index)
{
  if (index < MAX_ATTRIBS)
    attribs[index].enabled = 0;
}

static void
r3d_track_glPixelStorei (GLenum pname, GLint param)
{
  if (pname == GL_UNPACK_ALIGNMENT)
    unpack_align = param;
  else if (pname == GL_PACK_ALIGNMENT)
    pack_align = param;
}

/* how many values a glGet...v with this name gives */
static size_t
r3d_values (GLenum pname)
{
  switch (pname)
    {
    case GL_VIEWPORT: case GL_SCISSOR_BOX: case GL_COLOR_CLEAR_VALUE:
    case GL_COLOR_WRITEMASK: case GL_BLEND_COLOR: case GL_CURRENT_VERTEX_ATTRIB:
      return 4;
    case GL_DEPTH_RANGE: case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_ALIASED_LINE_WIDTH_RANGE: case GL_MAX_VIEWPORT_DIMS:
      return 2;
    }
  return 1;
}

static size_t
type_bytes (GLenum type)
{
  switch (type)
    {
    case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
    case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
    }
  return 4;
}

static size_t
image_bytes (GLsizei w, GLsizei h, GLenum format, GLenum type, int align)
{
  size_t bpp;
  if (type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_4_4_4_4
      || type == GL_UNSIGNED_SHORT_5_5_5_1)
    bpp = 2;
  else
    {
      size_t comps = format == GL_RGBA ? 4 : format == GL_RGB ? 3
		     : format == GL_LUMINANCE_ALPHA ? 2 : 1;
      bpp = comps * type_bytes (type);
    }
  if (w <= 0 || h <= 0)
    return 0;
  size_t row = (size_t) w * bpp;
  size_t padded = (row + (size_t) align - 1) / (size_t) align * (size_t) align;
  return padded * (size_t) (h - 1) + row;
}

/* --- strings ----------------------------------------------------------- */

static void
put_named (uint32_t op, GLuint a0, GLuint a1, int two, const GLchar *name)
{
  uint32_t f[3];
  size_t n = 0, l = strlen (name);
  f[n++] = a0;
  if (two)
    f[n++] = a1;
  f[n++] = (uint32_t) l;
  r3d_put_data (op, f, n, name, l);
}

void
glBindAttribLocation (GLuint program, GLuint index, const GLchar *name)
{
  put_named (OP_glBindAttribLocation, program, index, 1, name);
}

GLint
glGetAttribLocation (GLuint program, const GLchar *name)
{
  int32_t r = -1;
  put_named (OP_glGetAttribLocation, program, 0, 0, name);
  r3d_sync (&r, 4);
  return r;
}

GLint
glGetUniformLocation (GLuint program, const GLchar *name)
{
  int32_t r = -1;
  put_named (OP_glGetUniformLocation, program, 0, 0, name);
  r3d_sync (&r, 4);
  return r;
}

void
glShaderSource (GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)
{
  size_t total = 0;
  for (GLsizei i = 0; i < count; i++)
    total += length && length[i] >= 0 ? (size_t) length[i] : strlen (string[i]);
  char *all = malloc (total + 1);
  if (!all)
    return;
  size_t at = 0;
  for (GLsizei i = 0; i < count; i++)
    {
      size_t l = length && length[i] >= 0 ? (size_t) length[i] : strlen (string[i]);
      memcpy (all + at, string[i], l);
      at += l;
    }
  uint32_t f[2] = { shader, (uint32_t) total };
  r3d_put_data (OP_glShaderSource, f, 2, all, total);
  free (all);
}

/* an answer of a length word and then text */
static void
get_text (uint32_t op, GLuint obj, GLsizei bufSize, GLsizei *length, GLchar *text)
{
  if (length)
    *length = 0;
  if (bufSize <= 0)
    return;
  uint32_t *a = r3d_put (op, 2);
  uint32_t *r = malloc ((size_t) bufSize + 8);
  if (!a || !r)
    {
      free (r);
      return;
    }
  a[0] = obj;
  a[1] = (uint32_t) bufSize;
  r[0] = 0;
  r3d_sync (r, (size_t) bufSize + 4);
  size_t l = r[0] < (uint32_t) bufSize ? r[0] : (uint32_t) bufSize - 1;
  if (text)
    {
      memcpy (text, r + 1, l);
      text[l] = 0;
    }
  if (length)
    *length = (GLsizei) l;
  free (r);
}

void
glGetShaderInfoLog (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
  get_text (OP_glGetShaderInfoLog, shader, bufSize, length, infoLog);
}

void
glGetProgramInfoLog (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
  get_text (OP_glGetProgramInfoLog, program, bufSize, length, infoLog);
}

void
glGetShaderSource (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)
{
  get_text (OP_glGetShaderSource, shader, bufSize, length, source);
}

static void
get_active (uint32_t op, GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
	    GLint *size, GLenum *type, GLchar *name)
{
  uint32_t r[3 + 64];
  uint32_t *a = r3d_put (op, 3);
  if (!a)
    return;
  memset (r, 0, sizeof r);
  a[0] = program;
  a[1] = index;
  a[2] = 255;
  r3d_sync (r, sizeof r);
  size_t l = r[0] < 255 ? r[0] : 255;
  if (bufSize > 0 && l >= (size_t) bufSize)
    l = (size_t) bufSize - 1;
  if (size)
    *size = (GLint) r[1];
  if (type)
    *type = r[2];
  if (name && bufSize > 0)
    {
      memcpy (name, r + 3, l);
      name[l] = 0;
    }
  if (length)
    *length = (GLsizei) l;
}

void
glGetActiveAttrib (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
  get_active (OP_glGetActiveAttrib, program, index, bufSize, length, size, type, name);
}

void
glGetActiveUniform (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
  get_active (OP_glGetActiveUniform, program, index, bufSize, length, size, type, name);
}

void
glGetAttachedShaders (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders)
{
  uint32_t r[1 + 32];
  uint32_t *a = r3d_put (OP_glGetAttachedShaders, 2);
  if (!a)
    return;
  memset (r, 0, sizeof r);
  a[0] = program;
  a[1] = maxCount > 32 ? 32 : (uint32_t) maxCount;
  r3d_sync (r, sizeof r);
  if (count)
    *count = (GLsizei) r[0];
  if (shaders)
    memcpy (shaders, r + 1, r[0] * 4);
}

void
glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision)
{
  uint32_t r[3] = { 0, 0, 0 };
  uint32_t *a = r3d_put (OP_glGetShaderPrecisionFormat, 2);
  if (!a)
    return;
  a[0] = shadertype;
  a[1] = precisiontype;
  r3d_sync (r, sizeof r);
  range[0] = (GLint) r[0];
  range[1] = (GLint) r[1];
  *precision = (GLint) r[2];
}

const GLubyte *
glGetString (GLenum name)
{
  static char text[5][2048];
  int slot = name == GL_VENDOR ? 0 : name == GL_RENDERER ? 1 : name == GL_VERSION ? 2
	     : name == GL_SHADING_LANGUAGE_VERSION ? 3 : 4;
  uint32_t *a = r3d_put (OP_glGetString, 1);
  if (!a)
    return 0;
  a[0] = name;
  memset (text[slot], 0, sizeof text[slot]);
  if (r3d_sync (text[slot], sizeof text[slot] - 1) <= 0)
    return 0;
  return (const GLubyte *) text[slot];
}

static void
get_values (uint32_t op, GLenum pname, uint32_t r[16])
{
  uint32_t *a = r3d_put (op, 1);
  memset (r, 0, 16 * 4);
  if (!a)
    return;
  a[0] = pname;
  r3d_sync (r, 16 * 4);
}

void
glGetIntegerv (GLenum pname, GLint *data)
{
  uint32_t r[16];
  get_values (OP_glGetIntegerv, pname, r);
  memcpy (data, r, 4 * r3d_values (pname));
}

void
glGetFloatv (GLenum pname, GLfloat *data)
{
  uint32_t r[16];
  get_values (OP_glGetFloatv, pname, r);
  memcpy (data, r, 4 * r3d_values (pname));
}

void
glGetBooleanv (GLenum pname, GLboolean *data)
{
  uint32_t r[16];
  get_values (OP_glGetIntegerv, pname, r);
  for (size_t i = 0; i < r3d_values (pname); i++)
    data[i] = r[i] != 0;
}

/* --- data -------------------------------------------------------------- */

void
glBufferData (GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
  uint32_t f[4] = { target, (uint32_t) size, usage, data != 0 };
  r3d_put_data (OP_glBufferData, f, 4, data, data ? (size_t) size : 0);
}

void
glBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
  uint32_t f[3] = { target, (uint32_t) offset, (uint32_t) size };
  r3d_put_data (OP_glBufferSubData, f, 3, data, (size_t) size);
}

void
glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
  size_t n = pixels ? image_bytes (width, height, format, type, unpack_align) : 0;
  uint32_t f[9] = { target, (uint32_t) level, (uint32_t) internalformat, (uint32_t) width,
		    (uint32_t) height, (uint32_t) border, format, type, (uint32_t) n };
  r3d_put_data (OP_glTexImage2D, f, 9, pixels, n);
}

void
glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
  size_t n = image_bytes (width, height, format, type, unpack_align);
  uint32_t f[9] = { target, (uint32_t) level, (uint32_t) xoffset, (uint32_t) yoffset,
		    (uint32_t) width, (uint32_t) height, format, type, (uint32_t) n };
  r3d_put_data (OP_glTexSubImage2D, f, 9, pixels, n);
}

void
glCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data)
{
  uint32_t f[7] = { target, (uint32_t) level, internalformat, (uint32_t) width,
		    (uint32_t) height, (uint32_t) border, (uint32_t) imageSize };
  r3d_put_data (OP_glCompressedTexImage2D, f, 7, data, (size_t) imageSize);
}

void
glCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
  uint32_t f[8] = { target, (uint32_t) level, (uint32_t) xoffset, (uint32_t) yoffset,
		    (uint32_t) width, (uint32_t) height, format, (uint32_t) imageSize };
  r3d_put_data (OP_glCompressedTexSubImage2D, f, 8, data, (size_t) imageSize);
}

void
glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
  size_t n = image_bytes (width, height, format, type, pack_align);
  uint32_t *a = r3d_put (OP_glReadPixels, 7);
  if (!a)
    return;
  a[0] = (uint32_t) x;
  a[1] = (uint32_t) y;
  a[2] = (uint32_t) width;
  a[3] = (uint32_t) height;
  a[4] = format;
  a[5] = type;
  a[6] = (uint32_t) n;
  r3d_sync (pixels, n);
}

void
glShaderBinary (GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length)
{
  (void) count; (void) shaders; (void) binaryFormat; (void) binary; (void) length;
}

void
glBindFramebuffer (GLenum target, GLuint framebuffer)
{
  uint32_t *a = r3d_put (OP_glBindFramebuffer, 2);
  if (!a)
    return;
  a[0] = target;
  a[1] = framebuffer;
}

/* --- vertex arrays ------------------------------------------------------

   An array in a buffer object is the backend's already. One in the
   program's own memory is sent before each draw that uses it: the vertices
   from 0 to the last one the draw needs. */

void
glVertexAttribPointer (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
  if (index >= MAX_ATTRIBS)
    return;
  struct attrib *t = &attribs[index];
  t->size = size;
  t->type = type;
  t->normalized = normalized;
  t->stride = stride;
  t->pointer = pointer;
  t->client = array_buffer == 0;
  if (t->client)
    return;
  uint32_t *a = r3d_put (OP_glVertexAttribPointer, 6);
  if (!a)
    return;
  a[0] = index;
  a[1] = (uint32_t) size;
  a[2] = type;
  a[3] = normalized;
  a[4] = (uint32_t) stride;
  a[5] = (uint32_t) (uintptr_t) pointer;
}

void
glGetVertexAttribPointerv (GLuint index, GLenum pname, void **pointer)
{
  (void) pname;
  *pointer = index < MAX_ATTRIBS ? (void *) attribs[index].pointer : 0;
}

static void
send_client_arrays (size_t vertices)
{
  for (GLuint i = 0; i < MAX_ATTRIBS; i++)
    {
      struct attrib *t = &attribs[i];
      if (!t->enabled || !t->client || !t->pointer || !vertices)
	continue;
      size_t elem = (size_t) t->size * type_bytes (t->type);
      size_t stride = t->stride ? (size_t) t->stride : elem;
      size_t bytes = stride * (vertices - 1) + elem;
      uint32_t f[6] = { i, (uint32_t) t->size, t->type, t->normalized, (uint32_t) stride, (uint32_t) bytes };
      r3d_put_data (OP_CLIENT_ARRAY, f, 6, t->pointer, bytes);
    }
}

void
glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
  if (count <= 0 || first < 0)
    return;
  send_client_arrays ((size_t) first + (size_t) count);
  uint32_t *a = r3d_put (OP_glDrawArrays, 3);
  if (!a)
    return;
  a[0] = mode;
  a[1] = (uint32_t) first;
  a[2] = (uint32_t) count;
}

void
glDrawElements (GLenum mode, GLsizei count, GLenum type, const void *indices)
{
  if (count <= 0)
    return;
  if (element_buffer)
    {
      /* indices in a buffer object: arrays in the program's memory can't
	 be sized from here (PROTOTYPE: such a draw sends none) */
      uint32_t *a = r3d_put (OP_glDrawElements, 4);
      if (!a)
	return;
      a[0] = mode;
      a[1] = (uint32_t) count;
      a[2] = type;
      a[3] = (uint32_t) (uintptr_t) indices;
      return;
    }
  size_t max = 0, eb = type_bytes (type);
  for (GLsizei i = 0; i < count; i++)
    {
      size_t v = type == GL_UNSIGNED_BYTE ? ((const uint8_t *) indices)[i]
		 : type == GL_UNSIGNED_SHORT ? ((const uint16_t *) indices)[i]
		 : ((const uint32_t *) indices)[i];
      if (v > max)
	max = v;
    }
  send_client_arrays (max + 1);
  uint32_t f[4] = { mode, (uint32_t) count, type, (uint32_t) ((size_t) count * eb) };
  r3d_put_data (OP_DRAW_ELEMENTS_INLINE, f, 4, indices, (size_t) count * eb);
}

#include "client_gen.c"
