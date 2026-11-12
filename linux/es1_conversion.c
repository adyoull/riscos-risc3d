/* linux/es1_conversion.c: OpenGL ES 1.1's own functions on a desktop
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

static void
es1_AlphaFuncx(GLenum func, GLclampx ref)
{
   gl.AlphaFunc(func, (GLclampf) (ref / 65536.0f));
}

static void
es1_ClearColorx(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha)
{
   gl.ClearColor((GLclampf) (red / 65536.0f),
                    (GLclampf) (green / 65536.0f),
                    (GLclampf) (blue / 65536.0f),
                    (GLclampf) (alpha / 65536.0f));
}

static void
es1_ClearDepthx(GLclampx depth)
{
   es1_clear_depth((GLclampf) (depth / 65536.0f));
}

static void
es1_ClipPlanef(GLenum plane, const GLfloat *equation)
{
   unsigned int i;
   GLdouble converted_equation[4];

   for (i = 0; i < ARRAY_SIZE(converted_equation); i++) {
      converted_equation[i] = (GLdouble) (equation[i]);
   }

   gl.ClipPlane(plane, converted_equation);
}

static void
es1_ClipPlanex(GLenum plane, const GLfixed *equation)
{
   unsigned int i;
   GLdouble converted_equation[4];

   for (i = 0; i < ARRAY_SIZE(converted_equation); i++) {
      converted_equation[i] = (GLdouble) (equation[i] / 65536.0);
   }

   gl.ClipPlane(plane, converted_equation);
}


static void
es1_Color4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
{
    gl.Color4f((GLfloat) (red / 65536.0f),
                (GLfloat) (green / 65536.0f),
                (GLfloat) (blue / 65536.0f),
                (GLfloat) (alpha / 65536.0f));
}

static void
es1_DepthRangex(GLclampx zNear, GLclampx zFar)
{
    es1_depth_range((GLclampf) (zNear / 65536.0f),
                      (GLclampf) (zFar / 65536.0f));
}



static void
es1_Fogx(GLenum pname, GLfixed param)
{
   if (pname != GL_FOG_MODE) {
      gl.Fogf(pname, (GLfloat) (param / 65536.0f));
   } else {
      gl.Fogf(pname, (GLfloat) param);
   }

}

static void
es1_Fogxv(GLenum pname, const GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 4;
   GLfloat converted_params[4];
   bool convert_params_value = true;

   switch(pname) {
   case GL_FOG_MODE:
      convert_params_value = false;
      n_params = 1;
      break;
   case GL_FOG_COLOR:
      n_params = 4;
      break;
   case GL_FOG_DENSITY:
   case GL_FOG_START:
   case GL_FOG_END:
      n_params = 1;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glFogxv(pname=0x%x)", pname);
      return;
   }

   if (convert_params_value) {
      for (i = 0; i < n_params; i++) {
         converted_params[i] = (GLfloat) (params[i] / 65536.0f);
      }
   } else {
      for (i = 0; i < n_params; i++) {
         converted_params[i] = (GLfloat) params[i];
      }
   }

   gl.Fogfv(pname, converted_params);
}

static void
es1_Frustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
             GLfloat zNear, GLfloat zFar)
{
   gl.Frustum((GLdouble) (left),
                 (GLdouble) (right),
                 (GLdouble) (bottom),
                 (GLdouble) (top),
                 (GLdouble) (zNear),
                 (GLdouble) (zFar));
}

static void
es1_Frustumx(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top,
             GLfixed zNear, GLfixed zFar)
{
   gl.Frustum((GLdouble) (left / 65536.0),
                 (GLdouble) (right / 65536.0),
                 (GLdouble) (bottom / 65536.0),
                 (GLdouble) (top / 65536.0),
                 (GLdouble) (zNear / 65536.0),
                 (GLdouble) (zFar / 65536.0));
}

static void
es1_GetClipPlanef(GLenum plane, GLfloat *equation)
{
   unsigned int i;
   GLdouble converted_equation[4];

   gl.GetClipPlane(plane, converted_equation);
   for (i = 0; i < ARRAY_SIZE(converted_equation); i++) {
      equation[i] = (GLfloat) (converted_equation[i]);
   }
}

static void
es1_GetClipPlanex(GLenum plane, GLfixed *equation)
{
   unsigned int i;
   GLdouble converted_equation[4];

   gl.GetClipPlane(plane, converted_equation);
   for (i = 0; i < ARRAY_SIZE(converted_equation); i++) {
      equation[i] = (GLfixed) (converted_equation[i] * 65536);
   }
}

static void
es1_GetLightxv(GLenum light, GLenum pname, GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 4;
   GLfloat converted_params[4];

   if (light < GL_LIGHT0 || light > GL_LIGHT7) {
      ES1_ERROR(GL_INVALID_ENUM,
                  "glGetLightxv(light=0x%x)", light);
      return;
   }
   switch(pname) {
   case GL_AMBIENT:
   case GL_DIFFUSE:
   case GL_SPECULAR:
   case GL_POSITION:
      n_params = 4;
      break;
   case GL_SPOT_DIRECTION:
      n_params = 3;
      break;
   case GL_SPOT_EXPONENT:
   case GL_SPOT_CUTOFF:
   case GL_CONSTANT_ATTENUATION:
   case GL_LINEAR_ATTENUATION:
   case GL_QUADRATIC_ATTENUATION:
      n_params = 1;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glGetLightxv(pname=0x%x)", pname);
      return;
   }

   gl.GetLightfv(light, pname, converted_params);
   for (i = 0; i < n_params; i++) {
      params[i] = (GLint) (converted_params[i] * 65536);
   }
}

static void
es1_GetMaterialxv(GLenum face, GLenum pname, GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 4;
   GLfloat converted_params[4];

   switch(face) {
   case GL_FRONT:
   case GL_BACK:
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glGetMaterialxv(face=0x%x)", face);
      return;
   }
   switch(pname) {
   case GL_SHININESS:
      n_params = 1;
      break;
   case GL_AMBIENT:
   case GL_DIFFUSE:
   case GL_SPECULAR:
   case GL_EMISSION:
      n_params = 4;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glGetMaterialxv(pname=0x%x)", pname);
      return;
   }

   gl.GetMaterialfv(face, pname, converted_params);
   for (i = 0; i < n_params; i++) {
      params[i] = (GLint) (converted_params[i] * 65536);
   }
}

static void
es1_GetTexEnvxv(GLenum target, GLenum pname, GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 4;
   GLfloat converted_params[4];
   bool convert_params_value = true;

   switch(target) {
   case GL_POINT_SPRITE:
      if (pname != GL_COORD_REPLACE) {
         ES1_ERROR(GL_INVALID_ENUM,
                     "glGetTexEnvxv(target=0x%x)", target);
         return;
      }
      break;
   case GL_TEXTURE_FILTER_CONTROL_EXT:
      if (pname != GL_TEXTURE_LOD_BIAS_EXT) {
         ES1_ERROR(GL_INVALID_ENUM,
                     "glGetTexEnvxv(target=0x%x)", target);
         return;
      }
      break;
   case GL_TEXTURE_ENV:
      if (pname != GL_TEXTURE_ENV_COLOR &&
          pname != GL_RGB_SCALE &&
          pname != GL_ALPHA_SCALE &&
          pname != GL_TEXTURE_ENV_MODE &&
          pname != GL_COMBINE_RGB &&
          pname != GL_COMBINE_ALPHA &&
          pname != GL_SRC0_RGB &&
          pname != GL_SRC1_RGB &&
          pname != GL_SRC2_RGB &&
          pname != GL_SRC0_ALPHA &&
          pname != GL_SRC1_ALPHA &&
          pname != GL_SRC2_ALPHA &&
          pname != GL_OPERAND0_RGB &&
          pname != GL_OPERAND1_RGB &&
          pname != GL_OPERAND2_RGB &&
          pname != GL_OPERAND0_ALPHA &&
          pname != GL_OPERAND1_ALPHA &&
          pname != GL_OPERAND2_ALPHA) {
         ES1_ERROR(GL_INVALID_ENUM,
                     "glGetTexEnvxv(target=0x%x)", target);
         return;
      }
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glGetTexEnvxv(target=0x%x)", target);
      return;
   }
   switch(pname) {
   case GL_COORD_REPLACE:
      convert_params_value = false;
      n_params = 1;
      break;
   case GL_TEXTURE_LOD_BIAS_EXT:
      n_params = 1;
      break;
   case GL_TEXTURE_ENV_COLOR:
      n_params = 4;
      break;
   case GL_RGB_SCALE:
   case GL_ALPHA_SCALE:
      n_params = 1;
      break;
   case GL_TEXTURE_ENV_MODE:
   case GL_COMBINE_RGB:
   case GL_COMBINE_ALPHA:
   case GL_SRC0_RGB:
   case GL_SRC1_RGB:
   case GL_SRC2_RGB:
   case GL_SRC0_ALPHA:
   case GL_SRC1_ALPHA:
   case GL_SRC2_ALPHA:
   case GL_OPERAND0_RGB:
   case GL_OPERAND1_RGB:
   case GL_OPERAND2_RGB:
   case GL_OPERAND0_ALPHA:
   case GL_OPERAND1_ALPHA:
   case GL_OPERAND2_ALPHA:
      convert_params_value = false;
      n_params = 1;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glGetTexEnvxv(pname=0x%x)", pname);
      return;
   }

   gl.GetTexEnvfv(target, pname, converted_params);
   if (convert_params_value) {
      for (i = 0; i < n_params; i++) {
         params[i] = (GLint) (converted_params[i] * 65536);
      }
   } else {
      for (i = 0; i < n_params; i++) {
         params[i] = (GLfixed) converted_params[i];
      }
   }
}

static __attribute__ ((unused)) void
es1__check_GetTexGenivOES(GLenum coord, GLenum pname, GLint *params)
{
   gl.GetTexGeniv(coord, pname, params);
}

static void
es1_GetTexGenxvOES(GLenum coord, GLenum pname, GLfixed *params)
{
   gl.GetTexGeniv(coord, pname, (GLint *) params);
}

static void
es1_GetTexParameterxv(GLenum target, GLenum pname, GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 4;
   GLfloat converted_params[4];
   bool convert_params_value = true;

   switch(target) {
   case GL_TEXTURE_2D:
   case GL_TEXTURE_CUBE_MAP:
   case GL_TEXTURE_EXTERNAL_OES:
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glGetTexParameterxv(target=0x%x)", target);
      return;
   }
   switch(pname) {
   case GL_TEXTURE_WRAP_S:
   case GL_TEXTURE_WRAP_T:
   case GL_TEXTURE_MIN_FILTER:
   case GL_TEXTURE_MAG_FILTER:
   case GL_GENERATE_MIPMAP:
      convert_params_value = false;
      n_params = 1;
      break;
   case GL_TEXTURE_CROP_RECT_OES:
      n_params = 4;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glGetTexParameterxv(pname=0x%x)", pname);
      return;
   }

   gl.GetTexParameterfv(target, pname, converted_params);
   if (convert_params_value) {
      for (i = 0; i < n_params; i++) {
         params[i] = (GLint) (converted_params[i] * 65536);
      }
   } else {
      for (i = 0; i < n_params; i++) {
         params[i] = (GLfixed) converted_params[i];
      }
   }
}

static void
es1_LightModelx(GLenum pname, GLfixed param)
{
   gl.LightModelf(pname, (GLfloat) param);
}

static void
es1_LightModelxv(GLenum pname, const GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 4;
   GLfloat converted_params[4];
   bool convert_params_value = true;

   switch(pname) {
   case GL_LIGHT_MODEL_AMBIENT:
      n_params = 4;
      break;
   case GL_LIGHT_MODEL_TWO_SIDE:
      convert_params_value = false;
      n_params = 1;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glLightModelxv(pname=0x%x)", pname);
      return;
   }

   if (convert_params_value) {
      for (i = 0; i < n_params; i++) {
         converted_params[i] = (GLfloat) (params[i] / 65536.0f);
      }
   } else {
      for (i = 0; i < n_params; i++) {
         converted_params[i] = (GLfloat) params[i];
      }
   }

   gl.LightModelfv(pname, converted_params);
}

static void
es1_Lightx(GLenum light, GLenum pname, GLfixed param)
{
   gl.Lightf(light, pname, (GLfloat) (param / 65536.0f));
}

static void
es1_Lightxv(GLenum light, GLenum pname, const GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 4;
   GLfloat converted_params[4];

   if (light < GL_LIGHT0 || light > GL_LIGHT7) {
      ES1_ERROR(GL_INVALID_ENUM,
                  "glLightxv(light=0x%x)", light);
      return;
   }
   switch(pname) {
   case GL_AMBIENT:
   case GL_DIFFUSE:
   case GL_SPECULAR:
   case GL_POSITION:
      n_params = 4;
      break;
   case GL_SPOT_DIRECTION:
      n_params = 3;
      break;
   case GL_SPOT_EXPONENT:
   case GL_SPOT_CUTOFF:
   case GL_CONSTANT_ATTENUATION:
   case GL_LINEAR_ATTENUATION:
   case GL_QUADRATIC_ATTENUATION:
      n_params = 1;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glLightxv(pname=0x%x)", pname);
      return;
   }

   for (i = 0; i < n_params; i++) {
      converted_params[i] = (GLfloat) (params[i] / 65536.0f);
   }

   gl.Lightfv(light, pname, converted_params);
}

static void
es1_LineWidthx(GLfixed width)
{
   gl.LineWidth((GLfloat) (width / 65536.0f));
}

static void
es1_LoadMatrixx(const GLfixed *m)
{
   unsigned int i;
   GLfloat converted_m[16];

   for (i = 0; i < ARRAY_SIZE(converted_m); i++) {
      converted_m[i] = (GLfloat) (m[i] / 65536.0f);
   }

   gl.LoadMatrixf(converted_m);
}

static void
es1_Materialx(GLenum face, GLenum pname, GLfixed param)
{
   if (face != GL_FRONT_AND_BACK) {
      ES1_ERROR(GL_INVALID_ENUM,
                  "glMaterialx(face=0x%x)", face);
      return;
   }

   if (pname != GL_SHININESS) {
      ES1_ERROR(GL_INVALID_ENUM,
                  "glMaterialx(pname=0x%x)", pname);
      return;
   }

   gl.Materialf(face, pname, (GLfloat) (param / 65536.0f));
}

static void
es1_Materialxv(GLenum face, GLenum pname, const GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 4;
   GLfloat converted_params[4];

   if (face != GL_FRONT_AND_BACK) {
      ES1_ERROR(GL_INVALID_ENUM,
                  "glMaterialxv(face=0x%x)", face);
      return;
   }

   switch(pname) {
   case GL_AMBIENT:
   case GL_DIFFUSE:
   case GL_AMBIENT_AND_DIFFUSE:
   case GL_SPECULAR:
   case GL_EMISSION:
      n_params = 4;
      break;
   case GL_SHININESS:
      n_params = 1;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glMaterialxv(pname=0x%x)", pname);
      return;
   }

   for (i = 0; i < n_params; i++) {
      converted_params[i] = (GLfloat) (params[i] / 65536.0f);
   }

   gl.Materialfv(face, pname, converted_params);
}

static void
es1_MultMatrixx(const GLfixed *m)
{
   unsigned int i;
   GLfloat converted_m[16];

   for (i = 0; i < ARRAY_SIZE(converted_m); i++) {
      converted_m[i] = (GLfloat) (m[i] / 65536.0f);
   }

   gl.MultMatrixf(converted_m);
}

static void
es1_MultiTexCoord4x(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
   gl.MultiTexCoord4f(texture,
                       (GLfloat) (s / 65536.0f),
                       (GLfloat) (t / 65536.0f),
                       (GLfloat) (r / 65536.0f),
                       (GLfloat) (q / 65536.0f));
}

static void
es1_Normal3x(GLfixed nx, GLfixed ny, GLfixed nz)
{
   gl.Normal3f((GLfloat) (nx / 65536.0f),
                (GLfloat) (ny / 65536.0f),
                (GLfloat) (nz / 65536.0f));
}

static void
es1_Orthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
           GLfloat zNear, GLfloat zFar)
{
   gl.Ortho((GLdouble) (left),
               (GLdouble) (right),
               (GLdouble) (bottom),
               (GLdouble) (top),
               (GLdouble) (zNear),
               (GLdouble) (zFar));
}

static void
es1_Orthox(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top,
           GLfixed zNear, GLfixed zFar)
{
   gl.Ortho((GLdouble) (left / 65536.0),
               (GLdouble) (right / 65536.0),
               (GLdouble) (bottom / 65536.0),
               (GLdouble) (top / 65536.0),
               (GLdouble) (zNear / 65536.0),
               (GLdouble) (zFar / 65536.0));
}

static void
es1_PointParameterx(GLenum pname, GLfixed param)
{
   gl.PointParameterf(pname, (GLfloat) (param / 65536.0f));
}

static void
es1_PointParameterxv(GLenum pname, const GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 3;
   GLfloat converted_params[3];

   switch(pname) {
   case GL_POINT_SIZE_MIN:
   case GL_POINT_SIZE_MAX:
   case GL_POINT_FADE_THRESHOLD_SIZE:
      n_params = 1;
      break;
   case GL_POINT_DISTANCE_ATTENUATION:
      n_params = 3;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glPointParameterxv(pname=0x%x)", pname);
      return;
   }

   for (i = 0; i < n_params; i++) {
      converted_params[i] = (GLfloat) (params[i] / 65536.0f);
   }

   gl.PointParameterfv(pname, converted_params);
}

static void
es1_PointSizex(GLfixed size)
{
   gl.PointSize((GLfloat) (size / 65536.0f));
}

static void
es1_PolygonOffsetx(GLfixed factor, GLfixed units)
{
   gl.PolygonOffset((GLfloat) (factor / 65536.0f),
                       (GLfloat) (units / 65536.0f));
}

static void
es1_Rotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
   gl.Rotatef((GLfloat) (angle / 65536.0f),
                 (GLfloat) (x / 65536.0f),
                 (GLfloat) (y / 65536.0f),
                 (GLfloat) (z / 65536.0f));
}

static void
es1_SampleCoveragex(GLclampx value, GLboolean invert)
{
   gl.SampleCoverage((GLclampf) (value / 65536.0f),
                           invert);
}

static void
es1_Scalex(GLfixed x, GLfixed y, GLfixed z)
{
   gl.Scalef((GLfloat) (x / 65536.0f),
                (GLfloat) (y / 65536.0f),
                (GLfloat) (z / 65536.0f));
}

static void
es1_TexEnvx(GLenum target, GLenum pname, GLfixed param)
{
   switch(target) {
   case GL_POINT_SPRITE:
   case GL_TEXTURE_FILTER_CONTROL_EXT:
   case GL_TEXTURE_ENV:
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glTexEnvx(target=0x%x)", target);
      return;
   }

   switch(pname) {
   case GL_COORD_REPLACE:
   case GL_TEXTURE_ENV_MODE:
   case GL_COMBINE_RGB:
   case GL_COMBINE_ALPHA:
   case GL_SRC0_RGB:
   case GL_SRC1_RGB:
   case GL_SRC2_RGB:
   case GL_SRC0_ALPHA:
   case GL_SRC1_ALPHA:
   case GL_SRC2_ALPHA:
   case GL_OPERAND0_RGB:
   case GL_OPERAND1_RGB:
   case GL_OPERAND2_RGB:
   case GL_OPERAND0_ALPHA:
   case GL_OPERAND1_ALPHA:
   case GL_OPERAND2_ALPHA:
      gl.TexEnvf(target, pname, (GLfloat) param);
      break;
   case GL_TEXTURE_LOD_BIAS_EXT:
   case GL_RGB_SCALE:
   case GL_ALPHA_SCALE:
      gl.TexEnvf(target, pname, (GLfloat) (param / 65536.0f));
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glTexEnvx(pname=0x%x)", pname);
      return;
   }
}

static void
es1_TexEnvxv(GLenum target, GLenum pname, const GLfixed *params)
{
   switch(target) {
   case GL_POINT_SPRITE:
   case GL_TEXTURE_FILTER_CONTROL_EXT:
   case GL_TEXTURE_ENV:
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glTexEnvxv(target=0x%x)", target);
      return;
   }

   switch(pname) {
   case GL_COORD_REPLACE:
   case GL_TEXTURE_ENV_MODE:
   case GL_COMBINE_RGB:
   case GL_COMBINE_ALPHA:
   case GL_SRC0_RGB:
   case GL_SRC1_RGB:
   case GL_SRC2_RGB:
   case GL_SRC0_ALPHA:
   case GL_SRC1_ALPHA:
   case GL_SRC2_ALPHA:
   case GL_OPERAND0_RGB:
   case GL_OPERAND1_RGB:
   case GL_OPERAND2_RGB:
   case GL_OPERAND0_ALPHA:
   case GL_OPERAND1_ALPHA:
   case GL_OPERAND2_ALPHA:
      gl.TexEnvf(target, pname, (GLfloat) params[0]);
      break;
   case GL_TEXTURE_LOD_BIAS_EXT:
   case GL_RGB_SCALE:
   case GL_ALPHA_SCALE:
      gl.TexEnvf(target, pname, (GLfloat) (params[0] / 65536.0f));
      break;
   case GL_TEXTURE_ENV_COLOR: {
      unsigned int i;
      GLfloat converted_params[4];

      for (i = 0; i < ARRAY_SIZE(converted_params); i++) {
         converted_params[i] = (GLfloat) (params[i] / 65536.0f);
      }

      gl.TexEnvfv(target, pname, converted_params);
      break;
   }
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glTexEnvxv(pname=0x%x)", pname);
      return;
   }
}

static __attribute__ ((unused)) void
es1__check_TexGeniOES(GLenum coord, GLenum pname, GLint param)
{
   gl.TexGenf(coord, pname, (GLfloat) param);
}

static __attribute__ ((unused)) void
es1__check_TexGenivOES(GLenum coord, GLenum pname, const GLint *params)
{
   gl.TexGenf(coord, pname, (GLfloat) params[0]);
}

static void
es1_TexGenxOES(GLenum coord, GLenum pname, GLfixed param)
{
   gl.TexGenf(coord, pname, (GLfloat) param);
}

static void
es1_TexGenxvOES(GLenum coord, GLenum pname, const GLfixed *params)
{
   gl.TexGenf(coord, pname, (GLfloat) params[0]);
}

static void
es1_TexParameterx(GLenum target, GLenum pname, GLfixed param)
{
   if (pname == GL_TEXTURE_MAX_ANISOTROPY_EXT) {
      gl.TexParameterf(target, pname, (GLfloat) (param / 65536.0f));
   } else {
      gl.TexParameterf(target, pname, (GLfloat) param);
   }
}

static void
es1_TexParameterxv(GLenum target, GLenum pname, const GLfixed *params)
{
   unsigned int i;
   unsigned int n_params = 4;
   GLfloat converted_params[4];
   bool convert_params_value = true;

   switch(target) {
   case GL_TEXTURE_2D:
   case GL_TEXTURE_CUBE_MAP:
   case GL_TEXTURE_EXTERNAL_OES:
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glTexParameterxv(target=0x%x)", target);
      return;
   }
   switch(pname) {
   case GL_TEXTURE_WRAP_S:
   case GL_TEXTURE_WRAP_T:
      convert_params_value = false;
      n_params = 1;
      break;
   case GL_TEXTURE_MIN_FILTER:
   case GL_TEXTURE_MAG_FILTER:
   case GL_GENERATE_MIPMAP:
      convert_params_value = false;
      n_params = 1;
      break;
   case GL_TEXTURE_MAX_ANISOTROPY_EXT:
      n_params = 1;
      break;
   case GL_TEXTURE_CROP_RECT_OES:
      n_params = 4;
      break;
   default:
      ES1_ERROR(GL_INVALID_ENUM,
                  "glTexParameterxv(pname=0x%x)", pname);
      return;
   }

   if (convert_params_value) {
      for (i = 0; i < n_params; i++) {
         converted_params[i] = (GLfloat) (params[i] / 65536.0f);
      }
   } else {
      for (i = 0; i < n_params; i++) {
         converted_params[i] = (GLfloat) params[i];
      }
   }

   gl.TexParameterfv(target, pname, converted_params);
}

static void
es1_Translatex(GLfixed x, GLfixed y, GLfixed z)
{
    gl.Translatef((GLfloat) (x / 65536.0f),
                     (GLfloat) (y / 65536.0f),
                     (GLfloat) (z / 65536.0f));
}

/* the functions above, in place of the driver's of the same names */
#define ES1_FUNCTIONS(X) \
  X (AlphaFuncx) \
  X (ClearColorx) \
  X (ClearDepthx) \
  X (ClipPlanef) \
  X (ClipPlanex) \
  X (Color4x) \
  X (DepthRangex) \
  X (Fogx) \
  X (Fogxv) \
  X (Frustumf) \
  X (Frustumx) \
  X (GetClipPlanef) \
  X (GetClipPlanex) \
  X (GetLightxv) \
  X (GetMaterialxv) \
  X (GetTexEnvxv) \
  X (GetTexGenxvOES) \
  X (GetTexParameterxv) \
  X (LightModelx) \
  X (LightModelxv) \
  X (Lightx) \
  X (Lightxv) \
  X (LineWidthx) \
  X (LoadMatrixx) \
  X (Materialx) \
  X (Materialxv) \
  X (MultMatrixx) \
  X (MultiTexCoord4x) \
  X (Normal3x) \
  X (Orthof) \
  X (Orthox) \
  X (PointParameterx) \
  X (PointParameterxv) \
  X (PointSizex) \
  X (PolygonOffsetx) \
  X (Rotatex) \
  X (SampleCoveragex) \
  X (Scalex) \
  X (TexEnvx) \
  X (TexEnvxv) \
  X (TexGenxOES) \
  X (TexGenxvOES) \
  X (TexParameterx) \
  X (TexParameterxv) \
  X (Translatex)
