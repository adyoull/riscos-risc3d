/* scene.h: the test picture, in plain GLES 2: a textured, lit, spinning
   cube. scene_init returns 0 or what went wrong; scene_draw draws one
   frame at an angle. Shared by r3dtest and the desktop demo.
   Copyright (c) 2026 Andrew Youll. MIT licence (see LICENSE). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "r3d_gl.h"

static GLuint scene_prog, scene_tex, scene_vbo;
static GLint scene_mvp, scene_rot;
static char scene_log[512];

static const char scene_vs[] =
  "attribute vec3 pos; attribute vec3 nrm; attribute vec2 uv;\n"
  "uniform mat4 mvp; uniform mat4 rot; varying vec2 t; varying float l;\n"
  "void main () { gl_Position = mvp * vec4 (pos, 1.0); t = uv;\n"
  "  vec3 n = (rot * vec4 (nrm, 0.0)).xyz; l = 0.35 + 0.65 * max (dot (n, normalize (vec3 (0.4, 0.6, 1.0))), 0.0); }\n";
static const char scene_fs[] =
  "precision mediump float; uniform sampler2D tex; varying vec2 t; varying float l;\n"
  "void main () { gl_FragColor = vec4 (texture2D (tex, t).rgb * l, 1.0); }\n";

static GLuint
scene_shader (GLenum type, const char *src)
{
  GLuint s = glCreateShader (type);
  GLint ok = 0;
  glShaderSource (s, 1, &src, 0);
  glCompileShader (s);
  glGetShaderiv (s, GL_COMPILE_STATUS, &ok);
  if (!ok)
    {
      glGetShaderInfoLog (s, sizeof scene_log, 0, scene_log);
      return 0;
    }
  return s;
}

/* 24 vertices: position, normal, texture coordinate */
static GLfloat scene_v[24 * 8];
static GLushort scene_i[36];

static void
scene_mul (float *o, const float *a, const float *b)
{
  float r[16];
  for (int c = 0; c < 4; c++)
    for (int w = 0; w < 4; w++)
      r[c * 4 + w] = a[0 * 4 + w] * b[c * 4 + 0] + a[1 * 4 + w] * b[c * 4 + 1]
		     + a[2 * 4 + w] * b[c * 4 + 2] + a[3 * 4 + w] * b[c * 4 + 3];
  memcpy (o, r, sizeof r);
}

static const char *
scene_init (int use_vbo)
{
  static const float f[6][3][3] = {	/* normal, then two edges */
    { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } }, { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } },
    { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } }, { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
    { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } }, { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } } };
  for (int s = 0; s < 6; s++)
    {
      for (int k = 0; k < 4; k++)
	{
	  float a = (k == 1 || k == 2) ? 1 : -1, b = k >= 2 ? 1 : -1;
	  GLfloat *v = scene_v + (s * 4 + k) * 8;
	  for (int c = 0; c < 3; c++)
	    {
	      v[c] = f[s][0][c] + a * f[s][1][c] + b * f[s][2][c];
	      v[3 + c] = f[s][0][c];
	    }
	  v[6] = (a + 1) / 2;
	  v[7] = (b + 1) / 2;
	}
      static const int q[6] = { 0, 1, 2, 0, 2, 3 };
      for (int k = 0; k < 6; k++)
	scene_i[s * 6 + k] = (GLushort) (s * 4 + q[k]);
    }
  GLuint vs = scene_shader (GL_VERTEX_SHADER, scene_vs), fs = scene_shader (GL_FRAGMENT_SHADER, scene_fs);
  if (!vs || !fs)
    return scene_log[0] ? scene_log : "a shader would not compile";
  scene_prog = glCreateProgram ();
  glAttachShader (scene_prog, vs);
  glAttachShader (scene_prog, fs);
  glBindAttribLocation (scene_prog, 0, "pos");
  glBindAttribLocation (scene_prog, 1, "nrm");
  glBindAttribLocation (scene_prog, 2, "uv");
  glLinkProgram (scene_prog);
  GLint ok = 0;
  glGetProgramiv (scene_prog, GL_LINK_STATUS, &ok);
  if (!ok)
    {
      glGetProgramInfoLog (scene_prog, sizeof scene_log, 0, scene_log);
      return scene_log;
    }
  glUseProgram (scene_prog);
  scene_mvp = glGetUniformLocation (scene_prog, "mvp");
  scene_rot = glGetUniformLocation (scene_prog, "rot");
  /* a 64 by 64 chequered texture, each square its own colour */
  static unsigned char px[64 * 64 * 4];
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 64; x++)
      {
	unsigned char *p = px + (y * 64 + x) * 4;
	int on = ((x >> 3) ^ (y >> 3)) & 1;
	p[0] = on ? 255 : (unsigned char) (40 + x * 3);
	p[1] = on ? (unsigned char) (255 - y * 2) : 60;
	p[2] = on ? 80 : (unsigned char) (120 + y * 2);
	p[3] = 255;
      }
  glGenTextures (1, &scene_tex);
  glBindTexture (GL_TEXTURE_2D, scene_tex);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  glGenerateMipmap (GL_TEXTURE_2D);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glUniform1i (glGetUniformLocation (scene_prog, "tex"), 0);
  const GLfloat *base = scene_v;
  if (use_vbo)
    {
      glGenBuffers (1, &scene_vbo);
      glBindBuffer (GL_ARRAY_BUFFER, scene_vbo);
      glBufferData (GL_ARRAY_BUFFER, sizeof scene_v, scene_v, GL_STATIC_DRAW);
      base = 0;
    }
  glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, 32, base);
  glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, 32, base + 3);
  glVertexAttribPointer (2, 2, GL_FLOAT, GL_FALSE, 32, base + 6);
  glEnableVertexAttribArray (0);
  glEnableVertexAttribArray (1);
  glEnableVertexAttribArray (2);
  glEnable (GL_DEPTH_TEST);
  glEnable (GL_CULL_FACE);
  return glGetError () == GL_NO_ERROR ? 0 : "a GL error while setting up";
}

static void
scene_draw (int w, int h, float angle)
{
  float ca = cosf (angle), sa = sinf (angle), cb = cosf (angle * 0.7f), sb = sinf (angle * 0.7f);
  float ry[16] = { ca, 0, -sa, 0, 0, 1, 0, 0, sa, 0, ca, 0, 0, 0, 0, 1 };
  float rx[16] = { 1, 0, 0, 0, 0, cb, sb, 0, 0, -sb, cb, 0, 0, 0, 0, 1 };
  float rot[16], mv[16], mvp[16];
  scene_mul (rot, rx, ry);
  float tr[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -5, 1 };
  scene_mul (mv, tr, rot);
  float fz = 1.0f / tanf (0.4f), asp = (float) w / (float) h, n = 1, fr = 20;
  float pr[16] = { fz / asp, 0, 0, 0, 0, fz, 0, 0, 0, 0, (fr + n) / (n - fr), -1, 0, 0, 2 * fr * n / (n - fr), 0 };
  scene_mul (mvp, pr, mv);
  glViewport (0, 0, w, h);
  glClearColor (0.10f, 0.12f, 0.30f, 1.0f);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUniformMatrix4fv (scene_mvp, 1, GL_FALSE, mvp);
  glUniformMatrix4fv (scene_rot, 1, GL_FALSE, rot);
  glDrawElements (GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, scene_i);
}
