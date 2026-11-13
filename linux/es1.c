/* linux/es1.c: what else OpenGL ES 1.1 needs on a desktop OpenGL context,
   for rol-3d (#included after es1_conversion.c).

   - Arrays of fixed-point numbers in buffer objects (desktop OpenGL's
     arrays don't take GL_FIXED): made floats in a buffer of ours before
     each draw. (Those from the program's memory come as floats already:
     lib/r3d_gl.c converts them.)
   - The point size array (OES_point_size_array, which desktop OpenGL
     hasn't): points with sizes from it are drawn one at a time.
   - Its enable, query and pointer.

   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */

#define PRIV_ES1(i) priv_name (80 + (i))		/* fixed-point arrays as floats */
#define GL_POINT_SIZE_ARRAY_OES 0x8B9C
#define GL_POINT_SIZE_ARRAY_TYPE_OES 0x898A
#define GL_POINT_SIZE_ARRAY_STRIDE_OES 0x898B
#define GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES 0x8B9F

static struct es1_array
{
  int fixed;				/* GL_FIXED in a buffer object */
  GLint size;
  GLsizei stride;
  GLuint buffer;
  uintptr_t offset;
} es1_arr[16];				/* vertex 0, normal 1, colour 2, texture coordinates 8 + unit */

static struct
{
  int on;
  GLenum type;
  GLsizei stride;
  GLuint buffer;
  uintptr_t offset;
} ps = { 0, GL_FLOAT, 0, 0, 0 };

static int es1_unit;			/* the client's active texture unit */

static struct
{
  void (*VertexPointer) (GLint, GLenum, GLsizei, const void *);
  void (*NormalPointer) (GLenum, GLsizei, const void *);
  void (*ColorPointer) (GLint, GLenum, GLsizei, const void *);
  void (*TexCoordPointer) (GLint, GLenum, GLsizei, const void *);
  void (*ClientActiveTexture) (GLenum);
  void (*EnableClientState) (GLenum);
  void (*DisableClientState) (GLenum);
  GLboolean (*IsEnabled) (GLenum);
  void (*GetIntegerv) (GLenum, GLint *);
  void (*DrawArrays) (GLenum, GLint, GLsizei);
  void (*DrawElements) (GLenum, GLsizei, GLenum, const void *);
  void (*DrawRangeElements) (GLenum, GLuint, GLuint, GLsizei, GLenum, const void *);
} real;

static GLuint
bound_array_buffer (void)
{
  GLint b = 0;
  real.GetIntegerv (GL_ARRAY_BUFFER_BINDING, &b);
  return (GLuint) b;
}

static void
note (int slot, GLint size, GLenum type, GLsizei stride, const void *p)
{
  struct es1_array *t = &es1_arr[slot];
  t->fixed = type == GL_FIXED;
  t->size = size;
  t->stride = stride ? stride : size * 4;
  t->buffer = bound_array_buffer ();
  t->offset = (uintptr_t) p;
}

static void
es1_VertexPointer (GLint size, GLenum type, GLsizei stride, const void *p)
{
  note (0, size, type, stride, p);
  if (type != GL_FIXED)
    real.VertexPointer (size, type, stride, p);
}

static void
es1_NormalPointer (GLenum type, GLsizei stride, const void *p)
{
  note (1, 3, type, stride, p);
  if (type != GL_FIXED)
    real.NormalPointer (type, stride, p);
}

static void
es1_ColorPointer (GLint size, GLenum type, GLsizei stride, const void *p)
{
  note (2, size, type, stride, p);
  if (type != GL_FIXED)
    real.ColorPointer (size, type, stride, p);
}

static void
es1_TexCoordPointer (GLint size, GLenum type, GLsizei stride, const void *p)
{
  note (8 + (es1_unit & 7), size, type, stride, p);
  if (type != GL_FIXED)
    real.TexCoordPointer (size, type, stride, p);
}

static void
es1_ClientActiveTexture (GLenum t)
{
  es1_unit = (int) (t - GL_TEXTURE0);
  real.ClientActiveTexture (t);
}

static void
es1_PointSizePointerOES (GLenum type, GLsizei stride, const void *p)
{
  ps.type = type;
  ps.stride = stride ? stride : 4;
  ps.buffer = bound_array_buffer ();
  ps.offset = (uintptr_t) p;
}

static void
es1_EnableClientState (GLenum cap)
{
  if (cap == GL_POINT_SIZE_ARRAY_OES)
    ps.on = 1;
  else
    real.EnableClientState (cap);
}

static void
es1_DisableClientState (GLenum cap)
{
  if (cap == GL_POINT_SIZE_ARRAY_OES)
    ps.on = 0;
  else
    real.DisableClientState (cap);
}

static GLboolean
es1_IsEnabled (GLenum cap)
{
  return cap == GL_POINT_SIZE_ARRAY_OES ? (GLboolean) ps.on : real.IsEnabled (cap);
}

static void
es1_GetIntegerv (GLenum pname, GLint *v)
{
  switch (pname)
    {
    case GL_POINT_SIZE_ARRAY_TYPE_OES: *v = (GLint) ps.type; return;
    case GL_POINT_SIZE_ARRAY_STRIDE_OES: *v = ps.stride; return;
    case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES: *v = (GLint) ps.buffer; return;
    }
  real.GetIntegerv (pname, v);
}

/* bytes of a buffer from an offset to its end (0: none) */
static void *
buffer_tail (GLuint buffer, uintptr_t offset, size_t *bytes)
{
  GLint size = 0, was = (GLint) bound_array_buffer ();
  void *p = 0;
  *bytes = 0;
  gl.BindBuffer (GL_ARRAY_BUFFER, buffer);
  gl.GetBufferParameteriv (GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
  if (size > 0 && (uintptr_t) size > offset && (p = malloc ((size_t) size - offset)))
    {
      *bytes = (size_t) size - offset;
      gl.GetBufferSubData (GL_ARRAY_BUFFER, (GLintptr) offset, (GLsizeiptr) *bytes, p);
    }
  gl.BindBuffer (GL_ARRAY_BUFFER, (GLuint) was);
  return p;
}

/* before a draw: fixed-point arrays in buffer objects, as floats */
static void
es1_prepare (void)
{
  for (int s = 0; s < 16; s++)
    {
      struct es1_array *t = &es1_arr[s];
      if (!t->fixed || !t->buffer)
	continue;
      size_t bytes;
      char *src = buffer_tail (t->buffer, t->offset, &bytes);
      size_t elem = (size_t) t->size * 4, n = bytes >= elem ? (bytes - elem) / (size_t) t->stride + 1 : 0;
      GLfloat *f = n ? malloc (n * elem) : 0;
      for (size_t v = 0; f && v < n; v++)
	for (int k = 0; k < t->size; k++)
	  {
	    GLfixed x;
	    memcpy (&x, src + v * (size_t) t->stride + (size_t) k * 4, 4);
	    f[v * (size_t) t->size + (size_t) k] = (GLfloat) x / 65536.0F;
	  }
      free (src);
      if (!f)
	continue;
      GLint was = (GLint) bound_array_buffer ();
      gl.BindBuffer (GL_ARRAY_BUFFER, PRIV_ES1 (s));
      gl.BufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (n * elem), f, GL_STREAM_DRAW);
      free (f);
      switch (s)
	{
	case 0: real.VertexPointer (t->size, GL_FLOAT, 0, 0); break;
	case 1: real.NormalPointer (GL_FLOAT, 0, 0); break;
	case 2: real.ColorPointer (t->size, GL_FLOAT, 0, 0); break;
	default:
	  real.ClientActiveTexture (GL_TEXTURE0 + (GLenum) (s - 8));
	  real.TexCoordPointer (t->size, GL_FLOAT, 0, 0);
	  real.ClientActiveTexture (GL_TEXTURE0 + (GLenum) es1_unit);
	  break;
	}
      gl.BindBuffer (GL_ARRAY_BUFFER, (GLuint) was);
    }
}

/* points sized from the point size array: one draw a point */
static void
points_sized (const GLuint *vertices, size_t n)
{
  size_t bytes;
  char *sizes = ps.buffer ? buffer_tail (ps.buffer, ps.offset, &bytes) : 0;
  GLfloat was = 1;
  if (!sizes)
    return;
  gl.GetFloatv (GL_POINT_SIZE, &was);
  for (size_t i = 0; i < n; i++)
    {
      size_t at = (size_t) vertices[i] * (size_t) ps.stride;
      if (at + 4 > bytes)
	continue;
      GLfloat s;
      if (ps.type == GL_FIXED)
	{
	  GLfixed x;
	  memcpy (&x, sizes + at, 4);
	  s = (GLfloat) x / 65536.0F;
	}
      else
	memcpy (&s, sizes + at, 4);
      gl.PointSize (s > 0 ? s : 1);
      real.DrawArrays (GL_POINTS, (GLint) vertices[i], 1);
    }
  gl.PointSize (was);
  free (sizes);
}

static void
es1_DrawArrays (GLenum mode, GLint first, GLsizei count)
{
  es1_prepare ();
  if (mode != GL_POINTS || !ps.on || count <= 0)
    {
      real.DrawArrays (mode, first, count);
      return;
    }
  GLuint *v = malloc ((size_t) count * sizeof *v);
  if (!v)
    return;
  for (GLsizei i = 0; i < count; i++)
    v[i] = (GLuint) (first + i);
  points_sized (v, (size_t) count);
  free (v);
}

static void
elements_sized (GLsizei count, GLenum type, const void *indices)
{
  GLint eb = 0;
  size_t es = type == GL_UNSIGNED_BYTE ? 1 : type == GL_UNSIGNED_SHORT ? 2 : 4;
  gl.GetIntegerv (GL_ELEMENT_ARRAY_BUFFER_BINDING, &eb);
  char *ix = malloc ((size_t) count * es);
  GLuint *v = malloc ((size_t) count * sizeof *v);
  if (ix && v && eb)
    {
      gl.GetBufferSubData (GL_ELEMENT_ARRAY_BUFFER, (GLintptr) (uintptr_t) indices, (GLsizeiptr) ((size_t) count * es), ix);
      for (GLsizei i = 0; i < count; i++)
	v[i] = es == 1 ? ((uint8_t *) ix)[i] : es == 2 ? ((uint16_t *) ix)[i] : ((uint32_t *) ix)[i];
      points_sized (v, (size_t) count);
    }
  free (ix);
  free (v);
}

static void
es1_DrawElements (GLenum mode, GLsizei count, GLenum type, const void *indices)
{
  es1_prepare ();
  if (mode == GL_POINTS && ps.on)
    elements_sized (count, type, indices);
  else
    real.DrawElements (mode, count, type, indices);
}

static void
es1_DrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
  es1_prepare ();
  if (mode == GL_POINTS && ps.on)
    elements_sized (count, type, indices);
  else
    real.DrawRangeElements (mode, start, end, count, type, indices);
}

/* ETC1 textures (OES_compressed_ETC1_RGB8_texture, the Pi's GPU's format):
   desktop OpenGL doesn't take them, so they are decoded here into RGB, as
   riscos-mesa does itself. A 4x4 block is 8 bytes (big-endian). */
#define GL_ETC1_RGB8_OES 0x8D64
static const int etc1_mod[8][4] = {
  { 2, 8, -2, -8 }, { 5, 17, -5, -17 }, { 9, 29, -9, -29 }, { 13, 42, -13, -42 },
  { 18, 60, -18, -60 }, { 24, 80, -24, -80 }, { 33, 106, -33, -106 }, { 47, 183, -47, -183 } };

static unsigned char
clamp8 (int v)
{
  return (unsigned char) (v < 0 ? 0 : v > 255 ? 255 : v);
}

/* one 4x4 block into RGB, as Mesa 20.3.5's texcompress_etc_tmp.h decodes
   it (riscos-mesa's renderer), even for blocks no encoder makes */
static void
etc1_block (const unsigned char *b, unsigned char *out, int stride)
{
  static const int lookup[8] = { 0, 1, 2, 3, -4, -3, -2, -1 };
  unsigned char base[2][3];
  for (int c = 0; c < 3; c++)
    {
      unsigned char in = b[c];
      if (b[3] & 2)
	{
	  unsigned char lo = (unsigned char) ((in >> 3) + lookup[in & 7]);
	  base[0][c] = (unsigned char) ((in & 0xf8) | (in >> 5));
	  base[1][c] = (unsigned char) ((lo << 3) | (lo >> 2));
	}
      else
	{
	  base[0][c] = (unsigned char) ((in & 0xf0) | ((in & 0xf0) >> 4));
	  base[1][c] = (unsigned char) (((in & 0xf) << 4) | (in & 0xf));
	}
    }
  const int *mod[2] = { etc1_mod[(b[3] >> 5) & 7], etc1_mod[(b[3] >> 2) & 7] };
  int flip = b[3] & 1;
  uint32_t pi = (uint32_t) b[4] << 24 | (uint32_t) b[5] << 16 | (uint32_t) b[6] << 8 | b[7];
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      {
	int bit = y + x * 4;
	int idx = (int) (((pi >> (15 + bit)) & 2) | ((pi >> bit) & 1));
	int blk = flip ? y >= 2 : x >= 2;
	unsigned char *px = out + (size_t) y * (size_t) stride + (size_t) x * 3;
	for (int c = 0; c < 3; c++)
	  px[c] = clamp8 (base[blk][c] + mod[blk][idx]);
      }
}

/* an ETC1 image as RGB (w x h, rows of w * 3 bytes), or 0 */
static unsigned char *
etc1_decode (const unsigned char *data, GLsizei size, int w, int h)
{
  int bw = (w + 3) / 4, bh = (h + 3) / 4;
  if (w <= 0 || h <= 0 || size < bw * bh * 8)
    return 0;
  unsigned char *rgb = malloc ((size_t) bw * 4 * (size_t) bh * 4 * 3), *out = malloc ((size_t) w * (size_t) h * 3);
  if (!rgb || !out)
    {
      free (rgb);
      free (out);
      return 0;
    }
  int stride = bw * 4 * 3;
  for (int by = 0; by < bh; by++)
    for (int bx = 0; bx < bw; bx++)
      etc1_block (data + (size_t) (by * bw + bx) * 8, rgb + (size_t) by * 4 * (size_t) stride + (size_t) bx * 12, stride);
  for (int y = 0; y < h; y++)
    memcpy (out + (size_t) y * (size_t) w * 3, rgb + (size_t) y * (size_t) stride, (size_t) w * 3);
  free (rgb);
  return out;
}

static void (*real_CompressedTexImage2D) (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *);
static void (*real_CompressedTexSubImage2D) (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void *);

static void
unpacked_tight (GLint *was)
{
  gl.GetIntegerv (GL_UNPACK_ALIGNMENT, was);
  gl.PixelStorei (GL_UNPACK_ALIGNMENT, 1);
}

static void
es1_CompressedTexImage2D (GLenum target, GLint level, GLenum fmt, GLsizei w, GLsizei h, GLint border, GLsizei size, const void *data)
{
  if (fmt != GL_ETC1_RGB8_OES)
    {
      real_CompressedTexImage2D (target, level, fmt, w, h, border, size, data);
      return;
    }
  unsigned char *rgb = data ? etc1_decode (data, size, w, h) : 0;
  GLint was;
  unpacked_tight (&was);
  gl.TexImage2D (target, level, GL_RGB, w, h, border, GL_RGB, GL_UNSIGNED_BYTE, rgb);
  gl.PixelStorei (GL_UNPACK_ALIGNMENT, was);
  free (rgb);
}

static void
es1_CompressedTexSubImage2D (GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLsizei size, const void *data)
{
  if (fmt != GL_ETC1_RGB8_OES)
    {
      real_CompressedTexSubImage2D (target, level, x, y, w, h, fmt, size, data);
      return;
    }
  unsigned char *rgb = data ? etc1_decode (data, size, w, h) : 0;
  if (!rgb)
    return;
  GLint was;
  unpacked_tight (&was);
  gl.TexSubImage2D (target, level, x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb);
  gl.PixelStorei (GL_UNPACK_ALIGNMENT, was);
  free (rgb);
}

/* the driver's functions for GLES 1.1 on desktop OpenGL */
static void
es1_install (void)
{
#define KEEP(n) real.n = (__typeof__ (real.n)) gl.n; gl.n = (__typeof__ (gl.n)) es1_##n;
  KEEP (VertexPointer) KEEP (NormalPointer) KEEP (ColorPointer) KEEP (TexCoordPointer)
  KEEP (ClientActiveTexture) KEEP (EnableClientState) KEEP (DisableClientState) KEEP (IsEnabled)
  KEEP (GetIntegerv) KEEP (DrawArrays) KEEP (DrawElements) KEEP (DrawRangeElements)
#undef KEEP
  gl.PointSizePointerOES = es1_PointSizePointerOES;
  real_CompressedTexImage2D = gl.CompressedTexImage2D;
  gl.CompressedTexImage2D = es1_CompressedTexImage2D;
  real_CompressedTexSubImage2D = gl.CompressedTexSubImage2D;
  gl.CompressedTexSubImage2D = es1_CompressedTexSubImage2D;
#define CONV(n) gl.n = (__typeof__ (gl.n)) es1_##n;
  ES1_FUNCTIONS (CONV)
#undef CONV
}
