/**
 * SPDX-License-Identifier: (WTFPL OR CC0-1.0) AND Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glad/gles1.h>

#ifndef GLAD_IMPL_UTIL_C_
#define GLAD_IMPL_UTIL_C_

#ifdef _MSC_VER
#define GLAD_IMPL_UTIL_SSCANF sscanf_s
#else
#define GLAD_IMPL_UTIL_SSCANF sscanf
#endif

#endif /* GLAD_IMPL_UTIL_C_ */

#ifdef __cplusplus
extern "C" {
#endif



int GLAD_GL_VERSION_ES_CM_1_0 = 0;
int GLAD_GL_AMD_compressed_3DC_texture = 0;
int GLAD_GL_AMD_compressed_ATC_texture = 0;
int GLAD_GL_APPLE_copy_texture_levels = 0;
int GLAD_GL_APPLE_framebuffer_multisample = 0;
int GLAD_GL_APPLE_sync = 0;
int GLAD_GL_APPLE_texture_2D_limited_npot = 0;
int GLAD_GL_APPLE_texture_format_BGRA8888 = 0;
int GLAD_GL_APPLE_texture_max_level = 0;
int GLAD_GL_ARM_rgba8 = 0;
int GLAD_GL_EXT_blend_minmax = 0;
int GLAD_GL_EXT_debug_marker = 0;
int GLAD_GL_EXT_discard_framebuffer = 0;
int GLAD_GL_EXT_map_buffer_range = 0;
int GLAD_GL_EXT_multi_draw_arrays = 0;
int GLAD_GL_EXT_multisampled_render_to_texture = 0;
int GLAD_GL_EXT_read_format_bgra = 0;
int GLAD_GL_EXT_robustness = 0;
int GLAD_GL_EXT_sRGB = 0;
int GLAD_GL_EXT_texture_compression_dxt1 = 0;
int GLAD_GL_EXT_texture_filter_anisotropic = 0;
int GLAD_GL_EXT_texture_format_BGRA8888 = 0;
int GLAD_GL_EXT_texture_lod_bias = 0;
int GLAD_GL_EXT_texture_storage = 0;
int GLAD_GL_IMG_multisampled_render_to_texture = 0;
int GLAD_GL_IMG_read_format = 0;
int GLAD_GL_IMG_texture_compression_pvrtc = 0;
int GLAD_GL_IMG_texture_env_enhanced_fixed_function = 0;
int GLAD_GL_IMG_user_clip_plane = 0;
int GLAD_GL_KHR_debug = 0;
int GLAD_GL_NV_fence = 0;
int GLAD_GL_OES_EGL_image = 0;
int GLAD_GL_OES_EGL_image_external = 0;
int GLAD_GL_OES_blend_equation_separate = 0;
int GLAD_GL_OES_blend_func_separate = 0;
int GLAD_GL_OES_blend_subtract = 0;
int GLAD_GL_OES_byte_coordinates = 0;
int GLAD_GL_OES_compressed_ETC1_RGB8_sub_texture = 0;
int GLAD_GL_OES_compressed_ETC1_RGB8_texture = 0;
int GLAD_GL_OES_compressed_paletted_texture = 0;
int GLAD_GL_OES_depth24 = 0;
int GLAD_GL_OES_depth32 = 0;
int GLAD_GL_OES_draw_texture = 0;
int GLAD_GL_OES_element_index_uint = 0;
int GLAD_GL_OES_extended_matrix_palette = 0;
int GLAD_GL_OES_fbo_render_mipmap = 0;
int GLAD_GL_OES_fixed_point = 0;
int GLAD_GL_OES_framebuffer_object = 0;
int GLAD_GL_OES_mapbuffer = 0;
int GLAD_GL_OES_matrix_get = 0;
int GLAD_GL_OES_matrix_palette = 0;
int GLAD_GL_OES_packed_depth_stencil = 0;
int GLAD_GL_OES_point_size_array = 0;
int GLAD_GL_OES_point_sprite = 0;
int GLAD_GL_OES_query_matrix = 0;
int GLAD_GL_OES_read_format = 0;
int GLAD_GL_OES_required_internalformat = 0;
int GLAD_GL_OES_rgb8_rgba8 = 0;
int GLAD_GL_OES_single_precision = 0;
int GLAD_GL_OES_stencil1 = 0;
int GLAD_GL_OES_stencil4 = 0;
int GLAD_GL_OES_stencil8 = 0;
int GLAD_GL_OES_stencil_wrap = 0;
int GLAD_GL_OES_surfaceless_context = 0;
int GLAD_GL_OES_texture_cube_map = 0;
int GLAD_GL_OES_texture_env_crossbar = 0;
int GLAD_GL_OES_texture_mirrored_repeat = 0;
int GLAD_GL_OES_texture_npot = 0;
int GLAD_GL_OES_vertex_array_object = 0;
int GLAD_GL_QCOM_driver_control = 0;
int GLAD_GL_QCOM_extended_get = 0;
int GLAD_GL_QCOM_extended_get2 = 0;
int GLAD_GL_QCOM_perfmon_global_mode = 0;
int GLAD_GL_QCOM_tiled_rendering = 0;
int GLAD_GL_QCOM_writeonly_rendering = 0;


static void _pre_call_gles1_callback_default(const char *name, GLADapiproc apiproc, int len_args, ...) {
    GLAD_UNUSED(len_args);

    if (apiproc == NULL) {
        fprintf(stderr, "GLAD: ERROR %s is NULL!\n", name);
        return;
    }
    if (glad_glGetError == NULL) {
        fprintf(stderr, "GLAD: ERROR glGetError is NULL!\n");
        return;
    }

    (void) glad_glGetError();
}
static void _post_call_gles1_callback_default(void *ret, const char *name, GLADapiproc apiproc, int len_args, ...) {
    GLenum error_code;

    GLAD_UNUSED(ret);
    GLAD_UNUSED(apiproc);
    GLAD_UNUSED(len_args);

    error_code = glad_glGetError();

    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "GLAD: ERROR %d in %s!\n", error_code, name);
    }
}

static GLADprecallback _pre_call_gles1_callback = _pre_call_gles1_callback_default;
void gladSetGLES1PreCallback(GLADprecallback cb) {
    _pre_call_gles1_callback = cb;
}
static GLADpostcallback _post_call_gles1_callback = _post_call_gles1_callback_default;
void gladSetGLES1PostCallback(GLADpostcallback cb) {
    _post_call_gles1_callback = cb;
}

PFNGLACTIVETEXTUREPROC glad_glActiveTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glActiveTexture(GLenum texture) {
    _pre_call_gles1_callback("glActiveTexture", (GLADapiproc) glad_glActiveTexture, 1, texture);
    glad_glActiveTexture(texture);
    _post_call_gles1_callback(NULL, "glActiveTexture", (GLADapiproc) glad_glActiveTexture, 1, texture);
    
}
PFNGLACTIVETEXTUREPROC glad_debug_glActiveTexture = glad_debug_impl_glActiveTexture;
PFNGLALPHAFUNCPROC glad_glAlphaFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glAlphaFunc(GLenum func, GLfloat ref) {
    _pre_call_gles1_callback("glAlphaFunc", (GLADapiproc) glad_glAlphaFunc, 2, func, ref);
    glad_glAlphaFunc(func, ref);
    _post_call_gles1_callback(NULL, "glAlphaFunc", (GLADapiproc) glad_glAlphaFunc, 2, func, ref);
    
}
PFNGLALPHAFUNCPROC glad_debug_glAlphaFunc = glad_debug_impl_glAlphaFunc;
PFNGLALPHAFUNCXPROC glad_glAlphaFuncx = NULL;
static void GLAD_API_PTR glad_debug_impl_glAlphaFuncx(GLenum func, GLfixed ref) {
    _pre_call_gles1_callback("glAlphaFuncx", (GLADapiproc) glad_glAlphaFuncx, 2, func, ref);
    glad_glAlphaFuncx(func, ref);
    _post_call_gles1_callback(NULL, "glAlphaFuncx", (GLADapiproc) glad_glAlphaFuncx, 2, func, ref);
    
}
PFNGLALPHAFUNCXPROC glad_debug_glAlphaFuncx = glad_debug_impl_glAlphaFuncx;
PFNGLALPHAFUNCXOESPROC glad_glAlphaFuncxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glAlphaFuncxOES(GLenum func, GLfixed ref) {
    _pre_call_gles1_callback("glAlphaFuncxOES", (GLADapiproc) glad_glAlphaFuncxOES, 2, func, ref);
    glad_glAlphaFuncxOES(func, ref);
    _post_call_gles1_callback(NULL, "glAlphaFuncxOES", (GLADapiproc) glad_glAlphaFuncxOES, 2, func, ref);
    
}
PFNGLALPHAFUNCXOESPROC glad_debug_glAlphaFuncxOES = glad_debug_impl_glAlphaFuncxOES;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBuffer(GLenum target, GLuint buffer) {
    _pre_call_gles1_callback("glBindBuffer", (GLADapiproc) glad_glBindBuffer, 2, target, buffer);
    glad_glBindBuffer(target, buffer);
    _post_call_gles1_callback(NULL, "glBindBuffer", (GLADapiproc) glad_glBindBuffer, 2, target, buffer);
    
}
PFNGLBINDBUFFERPROC glad_debug_glBindBuffer = glad_debug_impl_glBindBuffer;
PFNGLBINDFRAMEBUFFEROESPROC glad_glBindFramebufferOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindFramebufferOES(GLenum target, GLuint framebuffer) {
    _pre_call_gles1_callback("glBindFramebufferOES", (GLADapiproc) glad_glBindFramebufferOES, 2, target, framebuffer);
    glad_glBindFramebufferOES(target, framebuffer);
    _post_call_gles1_callback(NULL, "glBindFramebufferOES", (GLADapiproc) glad_glBindFramebufferOES, 2, target, framebuffer);
    
}
PFNGLBINDFRAMEBUFFEROESPROC glad_debug_glBindFramebufferOES = glad_debug_impl_glBindFramebufferOES;
PFNGLBINDRENDERBUFFEROESPROC glad_glBindRenderbufferOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindRenderbufferOES(GLenum target, GLuint renderbuffer) {
    _pre_call_gles1_callback("glBindRenderbufferOES", (GLADapiproc) glad_glBindRenderbufferOES, 2, target, renderbuffer);
    glad_glBindRenderbufferOES(target, renderbuffer);
    _post_call_gles1_callback(NULL, "glBindRenderbufferOES", (GLADapiproc) glad_glBindRenderbufferOES, 2, target, renderbuffer);
    
}
PFNGLBINDRENDERBUFFEROESPROC glad_debug_glBindRenderbufferOES = glad_debug_impl_glBindRenderbufferOES;
PFNGLBINDTEXTUREPROC glad_glBindTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindTexture(GLenum target, GLuint texture) {
    _pre_call_gles1_callback("glBindTexture", (GLADapiproc) glad_glBindTexture, 2, target, texture);
    glad_glBindTexture(target, texture);
    _post_call_gles1_callback(NULL, "glBindTexture", (GLADapiproc) glad_glBindTexture, 2, target, texture);
    
}
PFNGLBINDTEXTUREPROC glad_debug_glBindTexture = glad_debug_impl_glBindTexture;
PFNGLBINDVERTEXARRAYOESPROC glad_glBindVertexArrayOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindVertexArrayOES(GLuint array) {
    _pre_call_gles1_callback("glBindVertexArrayOES", (GLADapiproc) glad_glBindVertexArrayOES, 1, array);
    glad_glBindVertexArrayOES(array);
    _post_call_gles1_callback(NULL, "glBindVertexArrayOES", (GLADapiproc) glad_glBindVertexArrayOES, 1, array);
    
}
PFNGLBINDVERTEXARRAYOESPROC glad_debug_glBindVertexArrayOES = glad_debug_impl_glBindVertexArrayOES;
PFNGLBLENDEQUATIONOESPROC glad_glBlendEquationOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationOES(GLenum mode) {
    _pre_call_gles1_callback("glBlendEquationOES", (GLADapiproc) glad_glBlendEquationOES, 1, mode);
    glad_glBlendEquationOES(mode);
    _post_call_gles1_callback(NULL, "glBlendEquationOES", (GLADapiproc) glad_glBlendEquationOES, 1, mode);
    
}
PFNGLBLENDEQUATIONOESPROC glad_debug_glBlendEquationOES = glad_debug_impl_glBlendEquationOES;
PFNGLBLENDEQUATIONSEPARATEOESPROC glad_glBlendEquationSeparateOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationSeparateOES(GLenum modeRGB, GLenum modeAlpha) {
    _pre_call_gles1_callback("glBlendEquationSeparateOES", (GLADapiproc) glad_glBlendEquationSeparateOES, 2, modeRGB, modeAlpha);
    glad_glBlendEquationSeparateOES(modeRGB, modeAlpha);
    _post_call_gles1_callback(NULL, "glBlendEquationSeparateOES", (GLADapiproc) glad_glBlendEquationSeparateOES, 2, modeRGB, modeAlpha);
    
}
PFNGLBLENDEQUATIONSEPARATEOESPROC glad_debug_glBlendEquationSeparateOES = glad_debug_impl_glBlendEquationSeparateOES;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFunc(GLenum sfactor, GLenum dfactor) {
    _pre_call_gles1_callback("glBlendFunc", (GLADapiproc) glad_glBlendFunc, 2, sfactor, dfactor);
    glad_glBlendFunc(sfactor, dfactor);
    _post_call_gles1_callback(NULL, "glBlendFunc", (GLADapiproc) glad_glBlendFunc, 2, sfactor, dfactor);
    
}
PFNGLBLENDFUNCPROC glad_debug_glBlendFunc = glad_debug_impl_glBlendFunc;
PFNGLBLENDFUNCSEPARATEOESPROC glad_glBlendFuncSeparateOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFuncSeparateOES(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    _pre_call_gles1_callback("glBlendFuncSeparateOES", (GLADapiproc) glad_glBlendFuncSeparateOES, 4, srcRGB, dstRGB, srcAlpha, dstAlpha);
    glad_glBlendFuncSeparateOES(srcRGB, dstRGB, srcAlpha, dstAlpha);
    _post_call_gles1_callback(NULL, "glBlendFuncSeparateOES", (GLADapiproc) glad_glBlendFuncSeparateOES, 4, srcRGB, dstRGB, srcAlpha, dstAlpha);
    
}
PFNGLBLENDFUNCSEPARATEOESPROC glad_debug_glBlendFuncSeparateOES = glad_debug_impl_glBlendFuncSeparateOES;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferData(GLenum target, GLsizeiptr size, const void * data, GLenum usage) {
    _pre_call_gles1_callback("glBufferData", (GLADapiproc) glad_glBufferData, 4, target, size, data, usage);
    glad_glBufferData(target, size, data, usage);
    _post_call_gles1_callback(NULL, "glBufferData", (GLADapiproc) glad_glBufferData, 4, target, size, data, usage);
    
}
PFNGLBUFFERDATAPROC glad_debug_glBufferData = glad_debug_impl_glBufferData;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void * data) {
    _pre_call_gles1_callback("glBufferSubData", (GLADapiproc) glad_glBufferSubData, 4, target, offset, size, data);
    glad_glBufferSubData(target, offset, size, data);
    _post_call_gles1_callback(NULL, "glBufferSubData", (GLADapiproc) glad_glBufferSubData, 4, target, offset, size, data);
    
}
PFNGLBUFFERSUBDATAPROC glad_debug_glBufferSubData = glad_debug_impl_glBufferSubData;
PFNGLCHECKFRAMEBUFFERSTATUSOESPROC glad_glCheckFramebufferStatusOES = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glCheckFramebufferStatusOES(GLenum target) {
    GLenum ret;
    _pre_call_gles1_callback("glCheckFramebufferStatusOES", (GLADapiproc) glad_glCheckFramebufferStatusOES, 1, target);
    ret = glad_glCheckFramebufferStatusOES(target);
    _post_call_gles1_callback((void*) &ret, "glCheckFramebufferStatusOES", (GLADapiproc) glad_glCheckFramebufferStatusOES, 1, target);
    return ret;
}
PFNGLCHECKFRAMEBUFFERSTATUSOESPROC glad_debug_glCheckFramebufferStatusOES = glad_debug_impl_glCheckFramebufferStatusOES;
PFNGLCLEARPROC glad_glClear = NULL;
static void GLAD_API_PTR glad_debug_impl_glClear(GLbitfield mask) {
    _pre_call_gles1_callback("glClear", (GLADapiproc) glad_glClear, 1, mask);
    glad_glClear(mask);
    _post_call_gles1_callback(NULL, "glClear", (GLADapiproc) glad_glClear, 1, mask);
    
}
PFNGLCLEARPROC glad_debug_glClear = glad_debug_impl_glClear;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    _pre_call_gles1_callback("glClearColor", (GLADapiproc) glad_glClearColor, 4, red, green, blue, alpha);
    glad_glClearColor(red, green, blue, alpha);
    _post_call_gles1_callback(NULL, "glClearColor", (GLADapiproc) glad_glClearColor, 4, red, green, blue, alpha);
    
}
PFNGLCLEARCOLORPROC glad_debug_glClearColor = glad_debug_impl_glClearColor;
PFNGLCLEARCOLORXPROC glad_glClearColorx = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearColorx(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha) {
    _pre_call_gles1_callback("glClearColorx", (GLADapiproc) glad_glClearColorx, 4, red, green, blue, alpha);
    glad_glClearColorx(red, green, blue, alpha);
    _post_call_gles1_callback(NULL, "glClearColorx", (GLADapiproc) glad_glClearColorx, 4, red, green, blue, alpha);
    
}
PFNGLCLEARCOLORXPROC glad_debug_glClearColorx = glad_debug_impl_glClearColorx;
PFNGLCLEARCOLORXOESPROC glad_glClearColorxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearColorxOES(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha) {
    _pre_call_gles1_callback("glClearColorxOES", (GLADapiproc) glad_glClearColorxOES, 4, red, green, blue, alpha);
    glad_glClearColorxOES(red, green, blue, alpha);
    _post_call_gles1_callback(NULL, "glClearColorxOES", (GLADapiproc) glad_glClearColorxOES, 4, red, green, blue, alpha);
    
}
PFNGLCLEARCOLORXOESPROC glad_debug_glClearColorxOES = glad_debug_impl_glClearColorxOES;
PFNGLCLEARDEPTHFPROC glad_glClearDepthf = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearDepthf(GLfloat d) {
    _pre_call_gles1_callback("glClearDepthf", (GLADapiproc) glad_glClearDepthf, 1, d);
    glad_glClearDepthf(d);
    _post_call_gles1_callback(NULL, "glClearDepthf", (GLADapiproc) glad_glClearDepthf, 1, d);
    
}
PFNGLCLEARDEPTHFPROC glad_debug_glClearDepthf = glad_debug_impl_glClearDepthf;
PFNGLCLEARDEPTHFOESPROC glad_glClearDepthfOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearDepthfOES(GLclampf depth) {
    _pre_call_gles1_callback("glClearDepthfOES", (GLADapiproc) glad_glClearDepthfOES, 1, depth);
    glad_glClearDepthfOES(depth);
    _post_call_gles1_callback(NULL, "glClearDepthfOES", (GLADapiproc) glad_glClearDepthfOES, 1, depth);
    
}
PFNGLCLEARDEPTHFOESPROC glad_debug_glClearDepthfOES = glad_debug_impl_glClearDepthfOES;
PFNGLCLEARDEPTHXPROC glad_glClearDepthx = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearDepthx(GLfixed depth) {
    _pre_call_gles1_callback("glClearDepthx", (GLADapiproc) glad_glClearDepthx, 1, depth);
    glad_glClearDepthx(depth);
    _post_call_gles1_callback(NULL, "glClearDepthx", (GLADapiproc) glad_glClearDepthx, 1, depth);
    
}
PFNGLCLEARDEPTHXPROC glad_debug_glClearDepthx = glad_debug_impl_glClearDepthx;
PFNGLCLEARDEPTHXOESPROC glad_glClearDepthxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearDepthxOES(GLfixed depth) {
    _pre_call_gles1_callback("glClearDepthxOES", (GLADapiproc) glad_glClearDepthxOES, 1, depth);
    glad_glClearDepthxOES(depth);
    _post_call_gles1_callback(NULL, "glClearDepthxOES", (GLADapiproc) glad_glClearDepthxOES, 1, depth);
    
}
PFNGLCLEARDEPTHXOESPROC glad_debug_glClearDepthxOES = glad_debug_impl_glClearDepthxOES;
PFNGLCLEARSTENCILPROC glad_glClearStencil = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearStencil(GLint s) {
    _pre_call_gles1_callback("glClearStencil", (GLADapiproc) glad_glClearStencil, 1, s);
    glad_glClearStencil(s);
    _post_call_gles1_callback(NULL, "glClearStencil", (GLADapiproc) glad_glClearStencil, 1, s);
    
}
PFNGLCLEARSTENCILPROC glad_debug_glClearStencil = glad_debug_impl_glClearStencil;
PFNGLCLIENTACTIVETEXTUREPROC glad_glClientActiveTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glClientActiveTexture(GLenum texture) {
    _pre_call_gles1_callback("glClientActiveTexture", (GLADapiproc) glad_glClientActiveTexture, 1, texture);
    glad_glClientActiveTexture(texture);
    _post_call_gles1_callback(NULL, "glClientActiveTexture", (GLADapiproc) glad_glClientActiveTexture, 1, texture);
    
}
PFNGLCLIENTACTIVETEXTUREPROC glad_debug_glClientActiveTexture = glad_debug_impl_glClientActiveTexture;
PFNGLCLIENTWAITSYNCAPPLEPROC glad_glClientWaitSyncAPPLE = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glClientWaitSyncAPPLE(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    GLenum ret;
    _pre_call_gles1_callback("glClientWaitSyncAPPLE", (GLADapiproc) glad_glClientWaitSyncAPPLE, 3, sync, flags, timeout);
    ret = glad_glClientWaitSyncAPPLE(sync, flags, timeout);
    _post_call_gles1_callback((void*) &ret, "glClientWaitSyncAPPLE", (GLADapiproc) glad_glClientWaitSyncAPPLE, 3, sync, flags, timeout);
    return ret;
}
PFNGLCLIENTWAITSYNCAPPLEPROC glad_debug_glClientWaitSyncAPPLE = glad_debug_impl_glClientWaitSyncAPPLE;
PFNGLCLIPPLANEFPROC glad_glClipPlanef = NULL;
static void GLAD_API_PTR glad_debug_impl_glClipPlanef(GLenum p, const GLfloat * eqn) {
    _pre_call_gles1_callback("glClipPlanef", (GLADapiproc) glad_glClipPlanef, 2, p, eqn);
    glad_glClipPlanef(p, eqn);
    _post_call_gles1_callback(NULL, "glClipPlanef", (GLADapiproc) glad_glClipPlanef, 2, p, eqn);
    
}
PFNGLCLIPPLANEFPROC glad_debug_glClipPlanef = glad_debug_impl_glClipPlanef;
PFNGLCLIPPLANEFIMGPROC glad_glClipPlanefIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glClipPlanefIMG(GLenum p, const GLfloat * eqn) {
    _pre_call_gles1_callback("glClipPlanefIMG", (GLADapiproc) glad_glClipPlanefIMG, 2, p, eqn);
    glad_glClipPlanefIMG(p, eqn);
    _post_call_gles1_callback(NULL, "glClipPlanefIMG", (GLADapiproc) glad_glClipPlanefIMG, 2, p, eqn);
    
}
PFNGLCLIPPLANEFIMGPROC glad_debug_glClipPlanefIMG = glad_debug_impl_glClipPlanefIMG;
PFNGLCLIPPLANEFOESPROC glad_glClipPlanefOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glClipPlanefOES(GLenum plane, const GLfloat * equation) {
    _pre_call_gles1_callback("glClipPlanefOES", (GLADapiproc) glad_glClipPlanefOES, 2, plane, equation);
    glad_glClipPlanefOES(plane, equation);
    _post_call_gles1_callback(NULL, "glClipPlanefOES", (GLADapiproc) glad_glClipPlanefOES, 2, plane, equation);
    
}
PFNGLCLIPPLANEFOESPROC glad_debug_glClipPlanefOES = glad_debug_impl_glClipPlanefOES;
PFNGLCLIPPLANEXPROC glad_glClipPlanex = NULL;
static void GLAD_API_PTR glad_debug_impl_glClipPlanex(GLenum plane, const GLfixed * equation) {
    _pre_call_gles1_callback("glClipPlanex", (GLADapiproc) glad_glClipPlanex, 2, plane, equation);
    glad_glClipPlanex(plane, equation);
    _post_call_gles1_callback(NULL, "glClipPlanex", (GLADapiproc) glad_glClipPlanex, 2, plane, equation);
    
}
PFNGLCLIPPLANEXPROC glad_debug_glClipPlanex = glad_debug_impl_glClipPlanex;
PFNGLCLIPPLANEXIMGPROC glad_glClipPlanexIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glClipPlanexIMG(GLenum p, const GLfixed * eqn) {
    _pre_call_gles1_callback("glClipPlanexIMG", (GLADapiproc) glad_glClipPlanexIMG, 2, p, eqn);
    glad_glClipPlanexIMG(p, eqn);
    _post_call_gles1_callback(NULL, "glClipPlanexIMG", (GLADapiproc) glad_glClipPlanexIMG, 2, p, eqn);
    
}
PFNGLCLIPPLANEXIMGPROC glad_debug_glClipPlanexIMG = glad_debug_impl_glClipPlanexIMG;
PFNGLCLIPPLANEXOESPROC glad_glClipPlanexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glClipPlanexOES(GLenum plane, const GLfixed * equation) {
    _pre_call_gles1_callback("glClipPlanexOES", (GLADapiproc) glad_glClipPlanexOES, 2, plane, equation);
    glad_glClipPlanexOES(plane, equation);
    _post_call_gles1_callback(NULL, "glClipPlanexOES", (GLADapiproc) glad_glClipPlanexOES, 2, plane, equation);
    
}
PFNGLCLIPPLANEXOESPROC glad_debug_glClipPlanexOES = glad_debug_impl_glClipPlanexOES;
PFNGLCOLOR4FPROC glad_glColor4f = NULL;
static void GLAD_API_PTR glad_debug_impl_glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    _pre_call_gles1_callback("glColor4f", (GLADapiproc) glad_glColor4f, 4, red, green, blue, alpha);
    glad_glColor4f(red, green, blue, alpha);
    _post_call_gles1_callback(NULL, "glColor4f", (GLADapiproc) glad_glColor4f, 4, red, green, blue, alpha);
    
}
PFNGLCOLOR4FPROC glad_debug_glColor4f = glad_debug_impl_glColor4f;
PFNGLCOLOR4UBPROC glad_glColor4ub = NULL;
static void GLAD_API_PTR glad_debug_impl_glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
    _pre_call_gles1_callback("glColor4ub", (GLADapiproc) glad_glColor4ub, 4, red, green, blue, alpha);
    glad_glColor4ub(red, green, blue, alpha);
    _post_call_gles1_callback(NULL, "glColor4ub", (GLADapiproc) glad_glColor4ub, 4, red, green, blue, alpha);
    
}
PFNGLCOLOR4UBPROC glad_debug_glColor4ub = glad_debug_impl_glColor4ub;
PFNGLCOLOR4XPROC glad_glColor4x = NULL;
static void GLAD_API_PTR glad_debug_impl_glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha) {
    _pre_call_gles1_callback("glColor4x", (GLADapiproc) glad_glColor4x, 4, red, green, blue, alpha);
    glad_glColor4x(red, green, blue, alpha);
    _post_call_gles1_callback(NULL, "glColor4x", (GLADapiproc) glad_glColor4x, 4, red, green, blue, alpha);
    
}
PFNGLCOLOR4XPROC glad_debug_glColor4x = glad_debug_impl_glColor4x;
PFNGLCOLOR4XOESPROC glad_glColor4xOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glColor4xOES(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha) {
    _pre_call_gles1_callback("glColor4xOES", (GLADapiproc) glad_glColor4xOES, 4, red, green, blue, alpha);
    glad_glColor4xOES(red, green, blue, alpha);
    _post_call_gles1_callback(NULL, "glColor4xOES", (GLADapiproc) glad_glColor4xOES, 4, red, green, blue, alpha);
    
}
PFNGLCOLOR4XOESPROC glad_debug_glColor4xOES = glad_debug_impl_glColor4xOES;
PFNGLCOLORMASKPROC glad_glColorMask = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    _pre_call_gles1_callback("glColorMask", (GLADapiproc) glad_glColorMask, 4, red, green, blue, alpha);
    glad_glColorMask(red, green, blue, alpha);
    _post_call_gles1_callback(NULL, "glColorMask", (GLADapiproc) glad_glColorMask, 4, red, green, blue, alpha);
    
}
PFNGLCOLORMASKPROC glad_debug_glColorMask = glad_debug_impl_glColorMask;
PFNGLCOLORPOINTERPROC glad_glColorPointer = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorPointer(GLint size, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gles1_callback("glColorPointer", (GLADapiproc) glad_glColorPointer, 4, size, type, stride, pointer);
    glad_glColorPointer(size, type, stride, pointer);
    _post_call_gles1_callback(NULL, "glColorPointer", (GLADapiproc) glad_glColorPointer, 4, size, type, stride, pointer);
    
}
PFNGLCOLORPOINTERPROC glad_debug_glColorPointer = glad_debug_impl_glColorPointer;
PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_glCompressedTexImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gles1_callback("glCompressedTexImage2D", (GLADapiproc) glad_glCompressedTexImage2D, 8, target, level, internalformat, width, height, border, imageSize, data);
    glad_glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
    _post_call_gles1_callback(NULL, "glCompressedTexImage2D", (GLADapiproc) glad_glCompressedTexImage2D, 8, target, level, internalformat, width, height, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_debug_glCompressedTexImage2D = glad_debug_impl_glCompressedTexImage2D;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_glCompressedTexSubImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gles1_callback("glCompressedTexSubImage2D", (GLADapiproc) glad_glCompressedTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, imageSize, data);
    glad_glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    _post_call_gles1_callback(NULL, "glCompressedTexSubImage2D", (GLADapiproc) glad_glCompressedTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_debug_glCompressedTexSubImage2D = glad_debug_impl_glCompressedTexSubImage2D;
PFNGLCOPYTEXIMAGE2DPROC glad_glCopyTexImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    _pre_call_gles1_callback("glCopyTexImage2D", (GLADapiproc) glad_glCopyTexImage2D, 8, target, level, internalformat, x, y, width, height, border);
    glad_glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    _post_call_gles1_callback(NULL, "glCopyTexImage2D", (GLADapiproc) glad_glCopyTexImage2D, 8, target, level, internalformat, x, y, width, height, border);
    
}
PFNGLCOPYTEXIMAGE2DPROC glad_debug_glCopyTexImage2D = glad_debug_impl_glCopyTexImage2D;
PFNGLCOPYTEXSUBIMAGE2DPROC glad_glCopyTexSubImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles1_callback("glCopyTexSubImage2D", (GLADapiproc) glad_glCopyTexSubImage2D, 8, target, level, xoffset, yoffset, x, y, width, height);
    glad_glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    _post_call_gles1_callback(NULL, "glCopyTexSubImage2D", (GLADapiproc) glad_glCopyTexSubImage2D, 8, target, level, xoffset, yoffset, x, y, width, height);
    
}
PFNGLCOPYTEXSUBIMAGE2DPROC glad_debug_glCopyTexSubImage2D = glad_debug_impl_glCopyTexSubImage2D;
PFNGLCOPYTEXTURELEVELSAPPLEPROC glad_glCopyTextureLevelsAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTextureLevelsAPPLE(GLuint destinationTexture, GLuint sourceTexture, GLint sourceBaseLevel, GLsizei sourceLevelCount) {
    _pre_call_gles1_callback("glCopyTextureLevelsAPPLE", (GLADapiproc) glad_glCopyTextureLevelsAPPLE, 4, destinationTexture, sourceTexture, sourceBaseLevel, sourceLevelCount);
    glad_glCopyTextureLevelsAPPLE(destinationTexture, sourceTexture, sourceBaseLevel, sourceLevelCount);
    _post_call_gles1_callback(NULL, "glCopyTextureLevelsAPPLE", (GLADapiproc) glad_glCopyTextureLevelsAPPLE, 4, destinationTexture, sourceTexture, sourceBaseLevel, sourceLevelCount);
    
}
PFNGLCOPYTEXTURELEVELSAPPLEPROC glad_debug_glCopyTextureLevelsAPPLE = glad_debug_impl_glCopyTextureLevelsAPPLE;
PFNGLCULLFACEPROC glad_glCullFace = NULL;
static void GLAD_API_PTR glad_debug_impl_glCullFace(GLenum mode) {
    _pre_call_gles1_callback("glCullFace", (GLADapiproc) glad_glCullFace, 1, mode);
    glad_glCullFace(mode);
    _post_call_gles1_callback(NULL, "glCullFace", (GLADapiproc) glad_glCullFace, 1, mode);
    
}
PFNGLCULLFACEPROC glad_debug_glCullFace = glad_debug_impl_glCullFace;
PFNGLCURRENTPALETTEMATRIXOESPROC glad_glCurrentPaletteMatrixOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glCurrentPaletteMatrixOES(GLuint matrixpaletteindex) {
    _pre_call_gles1_callback("glCurrentPaletteMatrixOES", (GLADapiproc) glad_glCurrentPaletteMatrixOES, 1, matrixpaletteindex);
    glad_glCurrentPaletteMatrixOES(matrixpaletteindex);
    _post_call_gles1_callback(NULL, "glCurrentPaletteMatrixOES", (GLADapiproc) glad_glCurrentPaletteMatrixOES, 1, matrixpaletteindex);
    
}
PFNGLCURRENTPALETTEMATRIXOESPROC glad_debug_glCurrentPaletteMatrixOES = glad_debug_impl_glCurrentPaletteMatrixOES;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteBuffers(GLsizei n, const GLuint * buffers) {
    _pre_call_gles1_callback("glDeleteBuffers", (GLADapiproc) glad_glDeleteBuffers, 2, n, buffers);
    glad_glDeleteBuffers(n, buffers);
    _post_call_gles1_callback(NULL, "glDeleteBuffers", (GLADapiproc) glad_glDeleteBuffers, 2, n, buffers);
    
}
PFNGLDELETEBUFFERSPROC glad_debug_glDeleteBuffers = glad_debug_impl_glDeleteBuffers;
PFNGLDELETEFENCESNVPROC glad_glDeleteFencesNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteFencesNV(GLsizei n, const GLuint * fences) {
    _pre_call_gles1_callback("glDeleteFencesNV", (GLADapiproc) glad_glDeleteFencesNV, 2, n, fences);
    glad_glDeleteFencesNV(n, fences);
    _post_call_gles1_callback(NULL, "glDeleteFencesNV", (GLADapiproc) glad_glDeleteFencesNV, 2, n, fences);
    
}
PFNGLDELETEFENCESNVPROC glad_debug_glDeleteFencesNV = glad_debug_impl_glDeleteFencesNV;
PFNGLDELETEFRAMEBUFFERSOESPROC glad_glDeleteFramebuffersOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteFramebuffersOES(GLsizei n, const GLuint * framebuffers) {
    _pre_call_gles1_callback("glDeleteFramebuffersOES", (GLADapiproc) glad_glDeleteFramebuffersOES, 2, n, framebuffers);
    glad_glDeleteFramebuffersOES(n, framebuffers);
    _post_call_gles1_callback(NULL, "glDeleteFramebuffersOES", (GLADapiproc) glad_glDeleteFramebuffersOES, 2, n, framebuffers);
    
}
PFNGLDELETEFRAMEBUFFERSOESPROC glad_debug_glDeleteFramebuffersOES = glad_debug_impl_glDeleteFramebuffersOES;
PFNGLDELETERENDERBUFFERSOESPROC glad_glDeleteRenderbuffersOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteRenderbuffersOES(GLsizei n, const GLuint * renderbuffers) {
    _pre_call_gles1_callback("glDeleteRenderbuffersOES", (GLADapiproc) glad_glDeleteRenderbuffersOES, 2, n, renderbuffers);
    glad_glDeleteRenderbuffersOES(n, renderbuffers);
    _post_call_gles1_callback(NULL, "glDeleteRenderbuffersOES", (GLADapiproc) glad_glDeleteRenderbuffersOES, 2, n, renderbuffers);
    
}
PFNGLDELETERENDERBUFFERSOESPROC glad_debug_glDeleteRenderbuffersOES = glad_debug_impl_glDeleteRenderbuffersOES;
PFNGLDELETESYNCAPPLEPROC glad_glDeleteSyncAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteSyncAPPLE(GLsync sync) {
    _pre_call_gles1_callback("glDeleteSyncAPPLE", (GLADapiproc) glad_glDeleteSyncAPPLE, 1, sync);
    glad_glDeleteSyncAPPLE(sync);
    _post_call_gles1_callback(NULL, "glDeleteSyncAPPLE", (GLADapiproc) glad_glDeleteSyncAPPLE, 1, sync);
    
}
PFNGLDELETESYNCAPPLEPROC glad_debug_glDeleteSyncAPPLE = glad_debug_impl_glDeleteSyncAPPLE;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteTextures(GLsizei n, const GLuint * textures) {
    _pre_call_gles1_callback("glDeleteTextures", (GLADapiproc) glad_glDeleteTextures, 2, n, textures);
    glad_glDeleteTextures(n, textures);
    _post_call_gles1_callback(NULL, "glDeleteTextures", (GLADapiproc) glad_glDeleteTextures, 2, n, textures);
    
}
PFNGLDELETETEXTURESPROC glad_debug_glDeleteTextures = glad_debug_impl_glDeleteTextures;
PFNGLDELETEVERTEXARRAYSOESPROC glad_glDeleteVertexArraysOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteVertexArraysOES(GLsizei n, const GLuint * arrays) {
    _pre_call_gles1_callback("glDeleteVertexArraysOES", (GLADapiproc) glad_glDeleteVertexArraysOES, 2, n, arrays);
    glad_glDeleteVertexArraysOES(n, arrays);
    _post_call_gles1_callback(NULL, "glDeleteVertexArraysOES", (GLADapiproc) glad_glDeleteVertexArraysOES, 2, n, arrays);
    
}
PFNGLDELETEVERTEXARRAYSOESPROC glad_debug_glDeleteVertexArraysOES = glad_debug_impl_glDeleteVertexArraysOES;
PFNGLDEPTHFUNCPROC glad_glDepthFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthFunc(GLenum func) {
    _pre_call_gles1_callback("glDepthFunc", (GLADapiproc) glad_glDepthFunc, 1, func);
    glad_glDepthFunc(func);
    _post_call_gles1_callback(NULL, "glDepthFunc", (GLADapiproc) glad_glDepthFunc, 1, func);
    
}
PFNGLDEPTHFUNCPROC glad_debug_glDepthFunc = glad_debug_impl_glDepthFunc;
PFNGLDEPTHMASKPROC glad_glDepthMask = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthMask(GLboolean flag) {
    _pre_call_gles1_callback("glDepthMask", (GLADapiproc) glad_glDepthMask, 1, flag);
    glad_glDepthMask(flag);
    _post_call_gles1_callback(NULL, "glDepthMask", (GLADapiproc) glad_glDepthMask, 1, flag);
    
}
PFNGLDEPTHMASKPROC glad_debug_glDepthMask = glad_debug_impl_glDepthMask;
PFNGLDEPTHRANGEFPROC glad_glDepthRangef = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRangef(GLfloat n, GLfloat f) {
    _pre_call_gles1_callback("glDepthRangef", (GLADapiproc) glad_glDepthRangef, 2, n, f);
    glad_glDepthRangef(n, f);
    _post_call_gles1_callback(NULL, "glDepthRangef", (GLADapiproc) glad_glDepthRangef, 2, n, f);
    
}
PFNGLDEPTHRANGEFPROC glad_debug_glDepthRangef = glad_debug_impl_glDepthRangef;
PFNGLDEPTHRANGEFOESPROC glad_glDepthRangefOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRangefOES(GLclampf n, GLclampf f) {
    _pre_call_gles1_callback("glDepthRangefOES", (GLADapiproc) glad_glDepthRangefOES, 2, n, f);
    glad_glDepthRangefOES(n, f);
    _post_call_gles1_callback(NULL, "glDepthRangefOES", (GLADapiproc) glad_glDepthRangefOES, 2, n, f);
    
}
PFNGLDEPTHRANGEFOESPROC glad_debug_glDepthRangefOES = glad_debug_impl_glDepthRangefOES;
PFNGLDEPTHRANGEXPROC glad_glDepthRangex = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRangex(GLfixed n, GLfixed f) {
    _pre_call_gles1_callback("glDepthRangex", (GLADapiproc) glad_glDepthRangex, 2, n, f);
    glad_glDepthRangex(n, f);
    _post_call_gles1_callback(NULL, "glDepthRangex", (GLADapiproc) glad_glDepthRangex, 2, n, f);
    
}
PFNGLDEPTHRANGEXPROC glad_debug_glDepthRangex = glad_debug_impl_glDepthRangex;
PFNGLDEPTHRANGEXOESPROC glad_glDepthRangexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRangexOES(GLfixed n, GLfixed f) {
    _pre_call_gles1_callback("glDepthRangexOES", (GLADapiproc) glad_glDepthRangexOES, 2, n, f);
    glad_glDepthRangexOES(n, f);
    _post_call_gles1_callback(NULL, "glDepthRangexOES", (GLADapiproc) glad_glDepthRangexOES, 2, n, f);
    
}
PFNGLDEPTHRANGEXOESPROC glad_debug_glDepthRangexOES = glad_debug_impl_glDepthRangexOES;
PFNGLDISABLEPROC glad_glDisable = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisable(GLenum cap) {
    _pre_call_gles1_callback("glDisable", (GLADapiproc) glad_glDisable, 1, cap);
    glad_glDisable(cap);
    _post_call_gles1_callback(NULL, "glDisable", (GLADapiproc) glad_glDisable, 1, cap);
    
}
PFNGLDISABLEPROC glad_debug_glDisable = glad_debug_impl_glDisable;
PFNGLDISABLECLIENTSTATEPROC glad_glDisableClientState = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableClientState(GLenum array) {
    _pre_call_gles1_callback("glDisableClientState", (GLADapiproc) glad_glDisableClientState, 1, array);
    glad_glDisableClientState(array);
    _post_call_gles1_callback(NULL, "glDisableClientState", (GLADapiproc) glad_glDisableClientState, 1, array);
    
}
PFNGLDISABLECLIENTSTATEPROC glad_debug_glDisableClientState = glad_debug_impl_glDisableClientState;
PFNGLDISABLEDRIVERCONTROLQCOMPROC glad_glDisableDriverControlQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableDriverControlQCOM(GLuint driverControl) {
    _pre_call_gles1_callback("glDisableDriverControlQCOM", (GLADapiproc) glad_glDisableDriverControlQCOM, 1, driverControl);
    glad_glDisableDriverControlQCOM(driverControl);
    _post_call_gles1_callback(NULL, "glDisableDriverControlQCOM", (GLADapiproc) glad_glDisableDriverControlQCOM, 1, driverControl);
    
}
PFNGLDISABLEDRIVERCONTROLQCOMPROC glad_debug_glDisableDriverControlQCOM = glad_debug_impl_glDisableDriverControlQCOM;
PFNGLDISCARDFRAMEBUFFEREXTPROC glad_glDiscardFramebufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDiscardFramebufferEXT(GLenum target, GLsizei numAttachments, const GLenum * attachments) {
    _pre_call_gles1_callback("glDiscardFramebufferEXT", (GLADapiproc) glad_glDiscardFramebufferEXT, 3, target, numAttachments, attachments);
    glad_glDiscardFramebufferEXT(target, numAttachments, attachments);
    _post_call_gles1_callback(NULL, "glDiscardFramebufferEXT", (GLADapiproc) glad_glDiscardFramebufferEXT, 3, target, numAttachments, attachments);
    
}
PFNGLDISCARDFRAMEBUFFEREXTPROC glad_debug_glDiscardFramebufferEXT = glad_debug_impl_glDiscardFramebufferEXT;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    _pre_call_gles1_callback("glDrawArrays", (GLADapiproc) glad_glDrawArrays, 3, mode, first, count);
    glad_glDrawArrays(mode, first, count);
    _post_call_gles1_callback(NULL, "glDrawArrays", (GLADapiproc) glad_glDrawArrays, 3, mode, first, count);
    
}
PFNGLDRAWARRAYSPROC glad_debug_glDrawArrays = glad_debug_impl_glDrawArrays;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void * indices) {
    _pre_call_gles1_callback("glDrawElements", (GLADapiproc) glad_glDrawElements, 4, mode, count, type, indices);
    glad_glDrawElements(mode, count, type, indices);
    _post_call_gles1_callback(NULL, "glDrawElements", (GLADapiproc) glad_glDrawElements, 4, mode, count, type, indices);
    
}
PFNGLDRAWELEMENTSPROC glad_debug_glDrawElements = glad_debug_impl_glDrawElements;
PFNGLDRAWTEXFOESPROC glad_glDrawTexfOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTexfOES(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height) {
    _pre_call_gles1_callback("glDrawTexfOES", (GLADapiproc) glad_glDrawTexfOES, 5, x, y, z, width, height);
    glad_glDrawTexfOES(x, y, z, width, height);
    _post_call_gles1_callback(NULL, "glDrawTexfOES", (GLADapiproc) glad_glDrawTexfOES, 5, x, y, z, width, height);
    
}
PFNGLDRAWTEXFOESPROC glad_debug_glDrawTexfOES = glad_debug_impl_glDrawTexfOES;
PFNGLDRAWTEXFVOESPROC glad_glDrawTexfvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTexfvOES(const GLfloat * coords) {
    _pre_call_gles1_callback("glDrawTexfvOES", (GLADapiproc) glad_glDrawTexfvOES, 1, coords);
    glad_glDrawTexfvOES(coords);
    _post_call_gles1_callback(NULL, "glDrawTexfvOES", (GLADapiproc) glad_glDrawTexfvOES, 1, coords);
    
}
PFNGLDRAWTEXFVOESPROC glad_debug_glDrawTexfvOES = glad_debug_impl_glDrawTexfvOES;
PFNGLDRAWTEXIOESPROC glad_glDrawTexiOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTexiOES(GLint x, GLint y, GLint z, GLint width, GLint height) {
    _pre_call_gles1_callback("glDrawTexiOES", (GLADapiproc) glad_glDrawTexiOES, 5, x, y, z, width, height);
    glad_glDrawTexiOES(x, y, z, width, height);
    _post_call_gles1_callback(NULL, "glDrawTexiOES", (GLADapiproc) glad_glDrawTexiOES, 5, x, y, z, width, height);
    
}
PFNGLDRAWTEXIOESPROC glad_debug_glDrawTexiOES = glad_debug_impl_glDrawTexiOES;
PFNGLDRAWTEXIVOESPROC glad_glDrawTexivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTexivOES(const GLint * coords) {
    _pre_call_gles1_callback("glDrawTexivOES", (GLADapiproc) glad_glDrawTexivOES, 1, coords);
    glad_glDrawTexivOES(coords);
    _post_call_gles1_callback(NULL, "glDrawTexivOES", (GLADapiproc) glad_glDrawTexivOES, 1, coords);
    
}
PFNGLDRAWTEXIVOESPROC glad_debug_glDrawTexivOES = glad_debug_impl_glDrawTexivOES;
PFNGLDRAWTEXSOESPROC glad_glDrawTexsOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTexsOES(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height) {
    _pre_call_gles1_callback("glDrawTexsOES", (GLADapiproc) glad_glDrawTexsOES, 5, x, y, z, width, height);
    glad_glDrawTexsOES(x, y, z, width, height);
    _post_call_gles1_callback(NULL, "glDrawTexsOES", (GLADapiproc) glad_glDrawTexsOES, 5, x, y, z, width, height);
    
}
PFNGLDRAWTEXSOESPROC glad_debug_glDrawTexsOES = glad_debug_impl_glDrawTexsOES;
PFNGLDRAWTEXSVOESPROC glad_glDrawTexsvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTexsvOES(const GLshort * coords) {
    _pre_call_gles1_callback("glDrawTexsvOES", (GLADapiproc) glad_glDrawTexsvOES, 1, coords);
    glad_glDrawTexsvOES(coords);
    _post_call_gles1_callback(NULL, "glDrawTexsvOES", (GLADapiproc) glad_glDrawTexsvOES, 1, coords);
    
}
PFNGLDRAWTEXSVOESPROC glad_debug_glDrawTexsvOES = glad_debug_impl_glDrawTexsvOES;
PFNGLDRAWTEXXOESPROC glad_glDrawTexxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTexxOES(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height) {
    _pre_call_gles1_callback("glDrawTexxOES", (GLADapiproc) glad_glDrawTexxOES, 5, x, y, z, width, height);
    glad_glDrawTexxOES(x, y, z, width, height);
    _post_call_gles1_callback(NULL, "glDrawTexxOES", (GLADapiproc) glad_glDrawTexxOES, 5, x, y, z, width, height);
    
}
PFNGLDRAWTEXXOESPROC glad_debug_glDrawTexxOES = glad_debug_impl_glDrawTexxOES;
PFNGLDRAWTEXXVOESPROC glad_glDrawTexxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTexxvOES(const GLfixed * coords) {
    _pre_call_gles1_callback("glDrawTexxvOES", (GLADapiproc) glad_glDrawTexxvOES, 1, coords);
    glad_glDrawTexxvOES(coords);
    _post_call_gles1_callback(NULL, "glDrawTexxvOES", (GLADapiproc) glad_glDrawTexxvOES, 1, coords);
    
}
PFNGLDRAWTEXXVOESPROC glad_debug_glDrawTexxvOES = glad_debug_impl_glDrawTexxvOES;
PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glad_glEGLImageTargetRenderbufferStorageOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glEGLImageTargetRenderbufferStorageOES(GLenum target, GLeglImageOES image) {
    _pre_call_gles1_callback("glEGLImageTargetRenderbufferStorageOES", (GLADapiproc) glad_glEGLImageTargetRenderbufferStorageOES, 2, target, image);
    glad_glEGLImageTargetRenderbufferStorageOES(target, image);
    _post_call_gles1_callback(NULL, "glEGLImageTargetRenderbufferStorageOES", (GLADapiproc) glad_glEGLImageTargetRenderbufferStorageOES, 2, target, image);
    
}
PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glad_debug_glEGLImageTargetRenderbufferStorageOES = glad_debug_impl_glEGLImageTargetRenderbufferStorageOES;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glad_glEGLImageTargetTexture2DOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image) {
    _pre_call_gles1_callback("glEGLImageTargetTexture2DOES", (GLADapiproc) glad_glEGLImageTargetTexture2DOES, 2, target, image);
    glad_glEGLImageTargetTexture2DOES(target, image);
    _post_call_gles1_callback(NULL, "glEGLImageTargetTexture2DOES", (GLADapiproc) glad_glEGLImageTargetTexture2DOES, 2, target, image);
    
}
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glad_debug_glEGLImageTargetTexture2DOES = glad_debug_impl_glEGLImageTargetTexture2DOES;
PFNGLENABLEPROC glad_glEnable = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnable(GLenum cap) {
    _pre_call_gles1_callback("glEnable", (GLADapiproc) glad_glEnable, 1, cap);
    glad_glEnable(cap);
    _post_call_gles1_callback(NULL, "glEnable", (GLADapiproc) glad_glEnable, 1, cap);
    
}
PFNGLENABLEPROC glad_debug_glEnable = glad_debug_impl_glEnable;
PFNGLENABLECLIENTSTATEPROC glad_glEnableClientState = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableClientState(GLenum array) {
    _pre_call_gles1_callback("glEnableClientState", (GLADapiproc) glad_glEnableClientState, 1, array);
    glad_glEnableClientState(array);
    _post_call_gles1_callback(NULL, "glEnableClientState", (GLADapiproc) glad_glEnableClientState, 1, array);
    
}
PFNGLENABLECLIENTSTATEPROC glad_debug_glEnableClientState = glad_debug_impl_glEnableClientState;
PFNGLENABLEDRIVERCONTROLQCOMPROC glad_glEnableDriverControlQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableDriverControlQCOM(GLuint driverControl) {
    _pre_call_gles1_callback("glEnableDriverControlQCOM", (GLADapiproc) glad_glEnableDriverControlQCOM, 1, driverControl);
    glad_glEnableDriverControlQCOM(driverControl);
    _post_call_gles1_callback(NULL, "glEnableDriverControlQCOM", (GLADapiproc) glad_glEnableDriverControlQCOM, 1, driverControl);
    
}
PFNGLENABLEDRIVERCONTROLQCOMPROC glad_debug_glEnableDriverControlQCOM = glad_debug_impl_glEnableDriverControlQCOM;
PFNGLENDTILINGQCOMPROC glad_glEndTilingQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndTilingQCOM(GLbitfield preserveMask) {
    _pre_call_gles1_callback("glEndTilingQCOM", (GLADapiproc) glad_glEndTilingQCOM, 1, preserveMask);
    glad_glEndTilingQCOM(preserveMask);
    _post_call_gles1_callback(NULL, "glEndTilingQCOM", (GLADapiproc) glad_glEndTilingQCOM, 1, preserveMask);
    
}
PFNGLENDTILINGQCOMPROC glad_debug_glEndTilingQCOM = glad_debug_impl_glEndTilingQCOM;
PFNGLEXTGETBUFFERPOINTERVQCOMPROC glad_glExtGetBufferPointervQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetBufferPointervQCOM(GLenum target, void ** params) {
    _pre_call_gles1_callback("glExtGetBufferPointervQCOM", (GLADapiproc) glad_glExtGetBufferPointervQCOM, 2, target, params);
    glad_glExtGetBufferPointervQCOM(target, params);
    _post_call_gles1_callback(NULL, "glExtGetBufferPointervQCOM", (GLADapiproc) glad_glExtGetBufferPointervQCOM, 2, target, params);
    
}
PFNGLEXTGETBUFFERPOINTERVQCOMPROC glad_debug_glExtGetBufferPointervQCOM = glad_debug_impl_glExtGetBufferPointervQCOM;
PFNGLEXTGETBUFFERSQCOMPROC glad_glExtGetBuffersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetBuffersQCOM(GLuint * buffers, GLint maxBuffers, GLint * numBuffers) {
    _pre_call_gles1_callback("glExtGetBuffersQCOM", (GLADapiproc) glad_glExtGetBuffersQCOM, 3, buffers, maxBuffers, numBuffers);
    glad_glExtGetBuffersQCOM(buffers, maxBuffers, numBuffers);
    _post_call_gles1_callback(NULL, "glExtGetBuffersQCOM", (GLADapiproc) glad_glExtGetBuffersQCOM, 3, buffers, maxBuffers, numBuffers);
    
}
PFNGLEXTGETBUFFERSQCOMPROC glad_debug_glExtGetBuffersQCOM = glad_debug_impl_glExtGetBuffersQCOM;
PFNGLEXTGETFRAMEBUFFERSQCOMPROC glad_glExtGetFramebuffersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetFramebuffersQCOM(GLuint * framebuffers, GLint maxFramebuffers, GLint * numFramebuffers) {
    _pre_call_gles1_callback("glExtGetFramebuffersQCOM", (GLADapiproc) glad_glExtGetFramebuffersQCOM, 3, framebuffers, maxFramebuffers, numFramebuffers);
    glad_glExtGetFramebuffersQCOM(framebuffers, maxFramebuffers, numFramebuffers);
    _post_call_gles1_callback(NULL, "glExtGetFramebuffersQCOM", (GLADapiproc) glad_glExtGetFramebuffersQCOM, 3, framebuffers, maxFramebuffers, numFramebuffers);
    
}
PFNGLEXTGETFRAMEBUFFERSQCOMPROC glad_debug_glExtGetFramebuffersQCOM = glad_debug_impl_glExtGetFramebuffersQCOM;
PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC glad_glExtGetProgramBinarySourceQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetProgramBinarySourceQCOM(GLuint program, GLenum shadertype, GLchar * source, GLint * length) {
    _pre_call_gles1_callback("glExtGetProgramBinarySourceQCOM", (GLADapiproc) glad_glExtGetProgramBinarySourceQCOM, 4, program, shadertype, source, length);
    glad_glExtGetProgramBinarySourceQCOM(program, shadertype, source, length);
    _post_call_gles1_callback(NULL, "glExtGetProgramBinarySourceQCOM", (GLADapiproc) glad_glExtGetProgramBinarySourceQCOM, 4, program, shadertype, source, length);
    
}
PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC glad_debug_glExtGetProgramBinarySourceQCOM = glad_debug_impl_glExtGetProgramBinarySourceQCOM;
PFNGLEXTGETPROGRAMSQCOMPROC glad_glExtGetProgramsQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetProgramsQCOM(GLuint * programs, GLint maxPrograms, GLint * numPrograms) {
    _pre_call_gles1_callback("glExtGetProgramsQCOM", (GLADapiproc) glad_glExtGetProgramsQCOM, 3, programs, maxPrograms, numPrograms);
    glad_glExtGetProgramsQCOM(programs, maxPrograms, numPrograms);
    _post_call_gles1_callback(NULL, "glExtGetProgramsQCOM", (GLADapiproc) glad_glExtGetProgramsQCOM, 3, programs, maxPrograms, numPrograms);
    
}
PFNGLEXTGETPROGRAMSQCOMPROC glad_debug_glExtGetProgramsQCOM = glad_debug_impl_glExtGetProgramsQCOM;
PFNGLEXTGETRENDERBUFFERSQCOMPROC glad_glExtGetRenderbuffersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetRenderbuffersQCOM(GLuint * renderbuffers, GLint maxRenderbuffers, GLint * numRenderbuffers) {
    _pre_call_gles1_callback("glExtGetRenderbuffersQCOM", (GLADapiproc) glad_glExtGetRenderbuffersQCOM, 3, renderbuffers, maxRenderbuffers, numRenderbuffers);
    glad_glExtGetRenderbuffersQCOM(renderbuffers, maxRenderbuffers, numRenderbuffers);
    _post_call_gles1_callback(NULL, "glExtGetRenderbuffersQCOM", (GLADapiproc) glad_glExtGetRenderbuffersQCOM, 3, renderbuffers, maxRenderbuffers, numRenderbuffers);
    
}
PFNGLEXTGETRENDERBUFFERSQCOMPROC glad_debug_glExtGetRenderbuffersQCOM = glad_debug_impl_glExtGetRenderbuffersQCOM;
PFNGLEXTGETSHADERSQCOMPROC glad_glExtGetShadersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetShadersQCOM(GLuint * shaders, GLint maxShaders, GLint * numShaders) {
    _pre_call_gles1_callback("glExtGetShadersQCOM", (GLADapiproc) glad_glExtGetShadersQCOM, 3, shaders, maxShaders, numShaders);
    glad_glExtGetShadersQCOM(shaders, maxShaders, numShaders);
    _post_call_gles1_callback(NULL, "glExtGetShadersQCOM", (GLADapiproc) glad_glExtGetShadersQCOM, 3, shaders, maxShaders, numShaders);
    
}
PFNGLEXTGETSHADERSQCOMPROC glad_debug_glExtGetShadersQCOM = glad_debug_impl_glExtGetShadersQCOM;
PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC glad_glExtGetTexLevelParameterivQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetTexLevelParameterivQCOM(GLuint texture, GLenum face, GLint level, GLenum pname, GLint * params) {
    _pre_call_gles1_callback("glExtGetTexLevelParameterivQCOM", (GLADapiproc) glad_glExtGetTexLevelParameterivQCOM, 5, texture, face, level, pname, params);
    glad_glExtGetTexLevelParameterivQCOM(texture, face, level, pname, params);
    _post_call_gles1_callback(NULL, "glExtGetTexLevelParameterivQCOM", (GLADapiproc) glad_glExtGetTexLevelParameterivQCOM, 5, texture, face, level, pname, params);
    
}
PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC glad_debug_glExtGetTexLevelParameterivQCOM = glad_debug_impl_glExtGetTexLevelParameterivQCOM;
PFNGLEXTGETTEXSUBIMAGEQCOMPROC glad_glExtGetTexSubImageQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetTexSubImageQCOM(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void * texels) {
    _pre_call_gles1_callback("glExtGetTexSubImageQCOM", (GLADapiproc) glad_glExtGetTexSubImageQCOM, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, texels);
    glad_glExtGetTexSubImageQCOM(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, texels);
    _post_call_gles1_callback(NULL, "glExtGetTexSubImageQCOM", (GLADapiproc) glad_glExtGetTexSubImageQCOM, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, texels);
    
}
PFNGLEXTGETTEXSUBIMAGEQCOMPROC glad_debug_glExtGetTexSubImageQCOM = glad_debug_impl_glExtGetTexSubImageQCOM;
PFNGLEXTGETTEXTURESQCOMPROC glad_glExtGetTexturesQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetTexturesQCOM(GLuint * textures, GLint maxTextures, GLint * numTextures) {
    _pre_call_gles1_callback("glExtGetTexturesQCOM", (GLADapiproc) glad_glExtGetTexturesQCOM, 3, textures, maxTextures, numTextures);
    glad_glExtGetTexturesQCOM(textures, maxTextures, numTextures);
    _post_call_gles1_callback(NULL, "glExtGetTexturesQCOM", (GLADapiproc) glad_glExtGetTexturesQCOM, 3, textures, maxTextures, numTextures);
    
}
PFNGLEXTGETTEXTURESQCOMPROC glad_debug_glExtGetTexturesQCOM = glad_debug_impl_glExtGetTexturesQCOM;
PFNGLEXTISPROGRAMBINARYQCOMPROC glad_glExtIsProgramBinaryQCOM = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glExtIsProgramBinaryQCOM(GLuint program) {
    GLboolean ret;
    _pre_call_gles1_callback("glExtIsProgramBinaryQCOM", (GLADapiproc) glad_glExtIsProgramBinaryQCOM, 1, program);
    ret = glad_glExtIsProgramBinaryQCOM(program);
    _post_call_gles1_callback((void*) &ret, "glExtIsProgramBinaryQCOM", (GLADapiproc) glad_glExtIsProgramBinaryQCOM, 1, program);
    return ret;
}
PFNGLEXTISPROGRAMBINARYQCOMPROC glad_debug_glExtIsProgramBinaryQCOM = glad_debug_impl_glExtIsProgramBinaryQCOM;
PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC glad_glExtTexObjectStateOverrideiQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtTexObjectStateOverrideiQCOM(GLenum target, GLenum pname, GLint param) {
    _pre_call_gles1_callback("glExtTexObjectStateOverrideiQCOM", (GLADapiproc) glad_glExtTexObjectStateOverrideiQCOM, 3, target, pname, param);
    glad_glExtTexObjectStateOverrideiQCOM(target, pname, param);
    _post_call_gles1_callback(NULL, "glExtTexObjectStateOverrideiQCOM", (GLADapiproc) glad_glExtTexObjectStateOverrideiQCOM, 3, target, pname, param);
    
}
PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC glad_debug_glExtTexObjectStateOverrideiQCOM = glad_debug_impl_glExtTexObjectStateOverrideiQCOM;
PFNGLFENCESYNCAPPLEPROC glad_glFenceSyncAPPLE = NULL;
static GLsync GLAD_API_PTR glad_debug_impl_glFenceSyncAPPLE(GLenum condition, GLbitfield flags) {
    GLsync ret;
    _pre_call_gles1_callback("glFenceSyncAPPLE", (GLADapiproc) glad_glFenceSyncAPPLE, 2, condition, flags);
    ret = glad_glFenceSyncAPPLE(condition, flags);
    _post_call_gles1_callback((void*) &ret, "glFenceSyncAPPLE", (GLADapiproc) glad_glFenceSyncAPPLE, 2, condition, flags);
    return ret;
}
PFNGLFENCESYNCAPPLEPROC glad_debug_glFenceSyncAPPLE = glad_debug_impl_glFenceSyncAPPLE;
PFNGLFINISHPROC glad_glFinish = NULL;
static void GLAD_API_PTR glad_debug_impl_glFinish(void) {
    _pre_call_gles1_callback("glFinish", (GLADapiproc) glad_glFinish, 0);
    glad_glFinish();
    _post_call_gles1_callback(NULL, "glFinish", (GLADapiproc) glad_glFinish, 0);
    
}
PFNGLFINISHPROC glad_debug_glFinish = glad_debug_impl_glFinish;
PFNGLFINISHFENCENVPROC glad_glFinishFenceNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glFinishFenceNV(GLuint fence) {
    _pre_call_gles1_callback("glFinishFenceNV", (GLADapiproc) glad_glFinishFenceNV, 1, fence);
    glad_glFinishFenceNV(fence);
    _post_call_gles1_callback(NULL, "glFinishFenceNV", (GLADapiproc) glad_glFinishFenceNV, 1, fence);
    
}
PFNGLFINISHFENCENVPROC glad_debug_glFinishFenceNV = glad_debug_impl_glFinishFenceNV;
PFNGLFLUSHPROC glad_glFlush = NULL;
static void GLAD_API_PTR glad_debug_impl_glFlush(void) {
    _pre_call_gles1_callback("glFlush", (GLADapiproc) glad_glFlush, 0);
    glad_glFlush();
    _post_call_gles1_callback(NULL, "glFlush", (GLADapiproc) glad_glFlush, 0);
    
}
PFNGLFLUSHPROC glad_debug_glFlush = glad_debug_impl_glFlush;
PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC glad_glFlushMappedBufferRangeEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFlushMappedBufferRangeEXT(GLenum target, GLintptr offset, GLsizeiptr length) {
    _pre_call_gles1_callback("glFlushMappedBufferRangeEXT", (GLADapiproc) glad_glFlushMappedBufferRangeEXT, 3, target, offset, length);
    glad_glFlushMappedBufferRangeEXT(target, offset, length);
    _post_call_gles1_callback(NULL, "glFlushMappedBufferRangeEXT", (GLADapiproc) glad_glFlushMappedBufferRangeEXT, 3, target, offset, length);
    
}
PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC glad_debug_glFlushMappedBufferRangeEXT = glad_debug_impl_glFlushMappedBufferRangeEXT;
PFNGLFOGFPROC glad_glFogf = NULL;
static void GLAD_API_PTR glad_debug_impl_glFogf(GLenum pname, GLfloat param) {
    _pre_call_gles1_callback("glFogf", (GLADapiproc) glad_glFogf, 2, pname, param);
    glad_glFogf(pname, param);
    _post_call_gles1_callback(NULL, "glFogf", (GLADapiproc) glad_glFogf, 2, pname, param);
    
}
PFNGLFOGFPROC glad_debug_glFogf = glad_debug_impl_glFogf;
PFNGLFOGFVPROC glad_glFogfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glFogfv(GLenum pname, const GLfloat * params) {
    _pre_call_gles1_callback("glFogfv", (GLADapiproc) glad_glFogfv, 2, pname, params);
    glad_glFogfv(pname, params);
    _post_call_gles1_callback(NULL, "glFogfv", (GLADapiproc) glad_glFogfv, 2, pname, params);
    
}
PFNGLFOGFVPROC glad_debug_glFogfv = glad_debug_impl_glFogfv;
PFNGLFOGXPROC glad_glFogx = NULL;
static void GLAD_API_PTR glad_debug_impl_glFogx(GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glFogx", (GLADapiproc) glad_glFogx, 2, pname, param);
    glad_glFogx(pname, param);
    _post_call_gles1_callback(NULL, "glFogx", (GLADapiproc) glad_glFogx, 2, pname, param);
    
}
PFNGLFOGXPROC glad_debug_glFogx = glad_debug_impl_glFogx;
PFNGLFOGXOESPROC glad_glFogxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glFogxOES(GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glFogxOES", (GLADapiproc) glad_glFogxOES, 2, pname, param);
    glad_glFogxOES(pname, param);
    _post_call_gles1_callback(NULL, "glFogxOES", (GLADapiproc) glad_glFogxOES, 2, pname, param);
    
}
PFNGLFOGXOESPROC glad_debug_glFogxOES = glad_debug_impl_glFogxOES;
PFNGLFOGXVPROC glad_glFogxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glFogxv(GLenum pname, const GLfixed * param) {
    _pre_call_gles1_callback("glFogxv", (GLADapiproc) glad_glFogxv, 2, pname, param);
    glad_glFogxv(pname, param);
    _post_call_gles1_callback(NULL, "glFogxv", (GLADapiproc) glad_glFogxv, 2, pname, param);
    
}
PFNGLFOGXVPROC glad_debug_glFogxv = glad_debug_impl_glFogxv;
PFNGLFOGXVOESPROC glad_glFogxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glFogxvOES(GLenum pname, const GLfixed * param) {
    _pre_call_gles1_callback("glFogxvOES", (GLADapiproc) glad_glFogxvOES, 2, pname, param);
    glad_glFogxvOES(pname, param);
    _post_call_gles1_callback(NULL, "glFogxvOES", (GLADapiproc) glad_glFogxvOES, 2, pname, param);
    
}
PFNGLFOGXVOESPROC glad_debug_glFogxvOES = glad_debug_impl_glFogxvOES;
PFNGLFRAMEBUFFERRENDERBUFFEROESPROC glad_glFramebufferRenderbufferOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferRenderbufferOES(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    _pre_call_gles1_callback("glFramebufferRenderbufferOES", (GLADapiproc) glad_glFramebufferRenderbufferOES, 4, target, attachment, renderbuffertarget, renderbuffer);
    glad_glFramebufferRenderbufferOES(target, attachment, renderbuffertarget, renderbuffer);
    _post_call_gles1_callback(NULL, "glFramebufferRenderbufferOES", (GLADapiproc) glad_glFramebufferRenderbufferOES, 4, target, attachment, renderbuffertarget, renderbuffer);
    
}
PFNGLFRAMEBUFFERRENDERBUFFEROESPROC glad_debug_glFramebufferRenderbufferOES = glad_debug_impl_glFramebufferRenderbufferOES;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glad_glFramebufferTexture2DMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture2DMultisampleEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples) {
    _pre_call_gles1_callback("glFramebufferTexture2DMultisampleEXT", (GLADapiproc) glad_glFramebufferTexture2DMultisampleEXT, 6, target, attachment, textarget, texture, level, samples);
    glad_glFramebufferTexture2DMultisampleEXT(target, attachment, textarget, texture, level, samples);
    _post_call_gles1_callback(NULL, "glFramebufferTexture2DMultisampleEXT", (GLADapiproc) glad_glFramebufferTexture2DMultisampleEXT, 6, target, attachment, textarget, texture, level, samples);
    
}
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glad_debug_glFramebufferTexture2DMultisampleEXT = glad_debug_impl_glFramebufferTexture2DMultisampleEXT;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC glad_glFramebufferTexture2DMultisampleIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture2DMultisampleIMG(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples) {
    _pre_call_gles1_callback("glFramebufferTexture2DMultisampleIMG", (GLADapiproc) glad_glFramebufferTexture2DMultisampleIMG, 6, target, attachment, textarget, texture, level, samples);
    glad_glFramebufferTexture2DMultisampleIMG(target, attachment, textarget, texture, level, samples);
    _post_call_gles1_callback(NULL, "glFramebufferTexture2DMultisampleIMG", (GLADapiproc) glad_glFramebufferTexture2DMultisampleIMG, 6, target, attachment, textarget, texture, level, samples);
    
}
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC glad_debug_glFramebufferTexture2DMultisampleIMG = glad_debug_impl_glFramebufferTexture2DMultisampleIMG;
PFNGLFRAMEBUFFERTEXTURE2DOESPROC glad_glFramebufferTexture2DOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture2DOES(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    _pre_call_gles1_callback("glFramebufferTexture2DOES", (GLADapiproc) glad_glFramebufferTexture2DOES, 5, target, attachment, textarget, texture, level);
    glad_glFramebufferTexture2DOES(target, attachment, textarget, texture, level);
    _post_call_gles1_callback(NULL, "glFramebufferTexture2DOES", (GLADapiproc) glad_glFramebufferTexture2DOES, 5, target, attachment, textarget, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTURE2DOESPROC glad_debug_glFramebufferTexture2DOES = glad_debug_impl_glFramebufferTexture2DOES;
PFNGLFRONTFACEPROC glad_glFrontFace = NULL;
static void GLAD_API_PTR glad_debug_impl_glFrontFace(GLenum mode) {
    _pre_call_gles1_callback("glFrontFace", (GLADapiproc) glad_glFrontFace, 1, mode);
    glad_glFrontFace(mode);
    _post_call_gles1_callback(NULL, "glFrontFace", (GLADapiproc) glad_glFrontFace, 1, mode);
    
}
PFNGLFRONTFACEPROC glad_debug_glFrontFace = glad_debug_impl_glFrontFace;
PFNGLFRUSTUMFPROC glad_glFrustumf = NULL;
static void GLAD_API_PTR glad_debug_impl_glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    _pre_call_gles1_callback("glFrustumf", (GLADapiproc) glad_glFrustumf, 6, l, r, b, t, n, f);
    glad_glFrustumf(l, r, b, t, n, f);
    _post_call_gles1_callback(NULL, "glFrustumf", (GLADapiproc) glad_glFrustumf, 6, l, r, b, t, n, f);
    
}
PFNGLFRUSTUMFPROC glad_debug_glFrustumf = glad_debug_impl_glFrustumf;
PFNGLFRUSTUMFOESPROC glad_glFrustumfOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glFrustumfOES(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    _pre_call_gles1_callback("glFrustumfOES", (GLADapiproc) glad_glFrustumfOES, 6, l, r, b, t, n, f);
    glad_glFrustumfOES(l, r, b, t, n, f);
    _post_call_gles1_callback(NULL, "glFrustumfOES", (GLADapiproc) glad_glFrustumfOES, 6, l, r, b, t, n, f);
    
}
PFNGLFRUSTUMFOESPROC glad_debug_glFrustumfOES = glad_debug_impl_glFrustumfOES;
PFNGLFRUSTUMXPROC glad_glFrustumx = NULL;
static void GLAD_API_PTR glad_debug_impl_glFrustumx(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f) {
    _pre_call_gles1_callback("glFrustumx", (GLADapiproc) glad_glFrustumx, 6, l, r, b, t, n, f);
    glad_glFrustumx(l, r, b, t, n, f);
    _post_call_gles1_callback(NULL, "glFrustumx", (GLADapiproc) glad_glFrustumx, 6, l, r, b, t, n, f);
    
}
PFNGLFRUSTUMXPROC glad_debug_glFrustumx = glad_debug_impl_glFrustumx;
PFNGLFRUSTUMXOESPROC glad_glFrustumxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glFrustumxOES(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f) {
    _pre_call_gles1_callback("glFrustumxOES", (GLADapiproc) glad_glFrustumxOES, 6, l, r, b, t, n, f);
    glad_glFrustumxOES(l, r, b, t, n, f);
    _post_call_gles1_callback(NULL, "glFrustumxOES", (GLADapiproc) glad_glFrustumxOES, 6, l, r, b, t, n, f);
    
}
PFNGLFRUSTUMXOESPROC glad_debug_glFrustumxOES = glad_debug_impl_glFrustumxOES;
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenBuffers(GLsizei n, GLuint * buffers) {
    _pre_call_gles1_callback("glGenBuffers", (GLADapiproc) glad_glGenBuffers, 2, n, buffers);
    glad_glGenBuffers(n, buffers);
    _post_call_gles1_callback(NULL, "glGenBuffers", (GLADapiproc) glad_glGenBuffers, 2, n, buffers);
    
}
PFNGLGENBUFFERSPROC glad_debug_glGenBuffers = glad_debug_impl_glGenBuffers;
PFNGLGENFENCESNVPROC glad_glGenFencesNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenFencesNV(GLsizei n, GLuint * fences) {
    _pre_call_gles1_callback("glGenFencesNV", (GLADapiproc) glad_glGenFencesNV, 2, n, fences);
    glad_glGenFencesNV(n, fences);
    _post_call_gles1_callback(NULL, "glGenFencesNV", (GLADapiproc) glad_glGenFencesNV, 2, n, fences);
    
}
PFNGLGENFENCESNVPROC glad_debug_glGenFencesNV = glad_debug_impl_glGenFencesNV;
PFNGLGENFRAMEBUFFERSOESPROC glad_glGenFramebuffersOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenFramebuffersOES(GLsizei n, GLuint * framebuffers) {
    _pre_call_gles1_callback("glGenFramebuffersOES", (GLADapiproc) glad_glGenFramebuffersOES, 2, n, framebuffers);
    glad_glGenFramebuffersOES(n, framebuffers);
    _post_call_gles1_callback(NULL, "glGenFramebuffersOES", (GLADapiproc) glad_glGenFramebuffersOES, 2, n, framebuffers);
    
}
PFNGLGENFRAMEBUFFERSOESPROC glad_debug_glGenFramebuffersOES = glad_debug_impl_glGenFramebuffersOES;
PFNGLGENRENDERBUFFERSOESPROC glad_glGenRenderbuffersOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenRenderbuffersOES(GLsizei n, GLuint * renderbuffers) {
    _pre_call_gles1_callback("glGenRenderbuffersOES", (GLADapiproc) glad_glGenRenderbuffersOES, 2, n, renderbuffers);
    glad_glGenRenderbuffersOES(n, renderbuffers);
    _post_call_gles1_callback(NULL, "glGenRenderbuffersOES", (GLADapiproc) glad_glGenRenderbuffersOES, 2, n, renderbuffers);
    
}
PFNGLGENRENDERBUFFERSOESPROC glad_debug_glGenRenderbuffersOES = glad_debug_impl_glGenRenderbuffersOES;
PFNGLGENTEXTURESPROC glad_glGenTextures = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenTextures(GLsizei n, GLuint * textures) {
    _pre_call_gles1_callback("glGenTextures", (GLADapiproc) glad_glGenTextures, 2, n, textures);
    glad_glGenTextures(n, textures);
    _post_call_gles1_callback(NULL, "glGenTextures", (GLADapiproc) glad_glGenTextures, 2, n, textures);
    
}
PFNGLGENTEXTURESPROC glad_debug_glGenTextures = glad_debug_impl_glGenTextures;
PFNGLGENVERTEXARRAYSOESPROC glad_glGenVertexArraysOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenVertexArraysOES(GLsizei n, GLuint * arrays) {
    _pre_call_gles1_callback("glGenVertexArraysOES", (GLADapiproc) glad_glGenVertexArraysOES, 2, n, arrays);
    glad_glGenVertexArraysOES(n, arrays);
    _post_call_gles1_callback(NULL, "glGenVertexArraysOES", (GLADapiproc) glad_glGenVertexArraysOES, 2, n, arrays);
    
}
PFNGLGENVERTEXARRAYSOESPROC glad_debug_glGenVertexArraysOES = glad_debug_impl_glGenVertexArraysOES;
PFNGLGENERATEMIPMAPOESPROC glad_glGenerateMipmapOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenerateMipmapOES(GLenum target) {
    _pre_call_gles1_callback("glGenerateMipmapOES", (GLADapiproc) glad_glGenerateMipmapOES, 1, target);
    glad_glGenerateMipmapOES(target);
    _post_call_gles1_callback(NULL, "glGenerateMipmapOES", (GLADapiproc) glad_glGenerateMipmapOES, 1, target);
    
}
PFNGLGENERATEMIPMAPOESPROC glad_debug_glGenerateMipmapOES = glad_debug_impl_glGenerateMipmapOES;
PFNGLGETBOOLEANVPROC glad_glGetBooleanv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBooleanv(GLenum pname, GLboolean * data) {
    _pre_call_gles1_callback("glGetBooleanv", (GLADapiproc) glad_glGetBooleanv, 2, pname, data);
    glad_glGetBooleanv(pname, data);
    _post_call_gles1_callback(NULL, "glGetBooleanv", (GLADapiproc) glad_glGetBooleanv, 2, pname, data);
    
}
PFNGLGETBOOLEANVPROC glad_debug_glGetBooleanv = glad_debug_impl_glGetBooleanv;
PFNGLGETBUFFERPARAMETERIVPROC glad_glGetBufferParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles1_callback("glGetBufferParameteriv", (GLADapiproc) glad_glGetBufferParameteriv, 3, target, pname, params);
    glad_glGetBufferParameteriv(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetBufferParameteriv", (GLADapiproc) glad_glGetBufferParameteriv, 3, target, pname, params);
    
}
PFNGLGETBUFFERPARAMETERIVPROC glad_debug_glGetBufferParameteriv = glad_debug_impl_glGetBufferParameteriv;
PFNGLGETBUFFERPOINTERVOESPROC glad_glGetBufferPointervOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferPointervOES(GLenum target, GLenum pname, void ** params) {
    _pre_call_gles1_callback("glGetBufferPointervOES", (GLADapiproc) glad_glGetBufferPointervOES, 3, target, pname, params);
    glad_glGetBufferPointervOES(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetBufferPointervOES", (GLADapiproc) glad_glGetBufferPointervOES, 3, target, pname, params);
    
}
PFNGLGETBUFFERPOINTERVOESPROC glad_debug_glGetBufferPointervOES = glad_debug_impl_glGetBufferPointervOES;
PFNGLGETCLIPPLANEFPROC glad_glGetClipPlanef = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetClipPlanef(GLenum plane, GLfloat * equation) {
    _pre_call_gles1_callback("glGetClipPlanef", (GLADapiproc) glad_glGetClipPlanef, 2, plane, equation);
    glad_glGetClipPlanef(plane, equation);
    _post_call_gles1_callback(NULL, "glGetClipPlanef", (GLADapiproc) glad_glGetClipPlanef, 2, plane, equation);
    
}
PFNGLGETCLIPPLANEFPROC glad_debug_glGetClipPlanef = glad_debug_impl_glGetClipPlanef;
PFNGLGETCLIPPLANEFOESPROC glad_glGetClipPlanefOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetClipPlanefOES(GLenum plane, GLfloat * equation) {
    _pre_call_gles1_callback("glGetClipPlanefOES", (GLADapiproc) glad_glGetClipPlanefOES, 2, plane, equation);
    glad_glGetClipPlanefOES(plane, equation);
    _post_call_gles1_callback(NULL, "glGetClipPlanefOES", (GLADapiproc) glad_glGetClipPlanefOES, 2, plane, equation);
    
}
PFNGLGETCLIPPLANEFOESPROC glad_debug_glGetClipPlanefOES = glad_debug_impl_glGetClipPlanefOES;
PFNGLGETCLIPPLANEXPROC glad_glGetClipPlanex = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetClipPlanex(GLenum plane, GLfixed * equation) {
    _pre_call_gles1_callback("glGetClipPlanex", (GLADapiproc) glad_glGetClipPlanex, 2, plane, equation);
    glad_glGetClipPlanex(plane, equation);
    _post_call_gles1_callback(NULL, "glGetClipPlanex", (GLADapiproc) glad_glGetClipPlanex, 2, plane, equation);
    
}
PFNGLGETCLIPPLANEXPROC glad_debug_glGetClipPlanex = glad_debug_impl_glGetClipPlanex;
PFNGLGETCLIPPLANEXOESPROC glad_glGetClipPlanexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetClipPlanexOES(GLenum plane, GLfixed * equation) {
    _pre_call_gles1_callback("glGetClipPlanexOES", (GLADapiproc) glad_glGetClipPlanexOES, 2, plane, equation);
    glad_glGetClipPlanexOES(plane, equation);
    _post_call_gles1_callback(NULL, "glGetClipPlanexOES", (GLADapiproc) glad_glGetClipPlanexOES, 2, plane, equation);
    
}
PFNGLGETCLIPPLANEXOESPROC glad_debug_glGetClipPlanexOES = glad_debug_impl_glGetClipPlanexOES;
PFNGLGETDRIVERCONTROLSTRINGQCOMPROC glad_glGetDriverControlStringQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetDriverControlStringQCOM(GLuint driverControl, GLsizei bufSize, GLsizei * length, GLchar * driverControlString) {
    _pre_call_gles1_callback("glGetDriverControlStringQCOM", (GLADapiproc) glad_glGetDriverControlStringQCOM, 4, driverControl, bufSize, length, driverControlString);
    glad_glGetDriverControlStringQCOM(driverControl, bufSize, length, driverControlString);
    _post_call_gles1_callback(NULL, "glGetDriverControlStringQCOM", (GLADapiproc) glad_glGetDriverControlStringQCOM, 4, driverControl, bufSize, length, driverControlString);
    
}
PFNGLGETDRIVERCONTROLSTRINGQCOMPROC glad_debug_glGetDriverControlStringQCOM = glad_debug_impl_glGetDriverControlStringQCOM;
PFNGLGETDRIVERCONTROLSQCOMPROC glad_glGetDriverControlsQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetDriverControlsQCOM(GLint * num, GLsizei size, GLuint * driverControls) {
    _pre_call_gles1_callback("glGetDriverControlsQCOM", (GLADapiproc) glad_glGetDriverControlsQCOM, 3, num, size, driverControls);
    glad_glGetDriverControlsQCOM(num, size, driverControls);
    _post_call_gles1_callback(NULL, "glGetDriverControlsQCOM", (GLADapiproc) glad_glGetDriverControlsQCOM, 3, num, size, driverControls);
    
}
PFNGLGETDRIVERCONTROLSQCOMPROC glad_debug_glGetDriverControlsQCOM = glad_debug_impl_glGetDriverControlsQCOM;
PFNGLGETERRORPROC glad_glGetError = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glGetError(void) {
    GLenum ret;
    _pre_call_gles1_callback("glGetError", (GLADapiproc) glad_glGetError, 0);
    ret = glad_glGetError();
    _post_call_gles1_callback((void*) &ret, "glGetError", (GLADapiproc) glad_glGetError, 0);
    return ret;
}
PFNGLGETERRORPROC glad_debug_glGetError = glad_debug_impl_glGetError;
PFNGLGETFENCEIVNVPROC glad_glGetFenceivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFenceivNV(GLuint fence, GLenum pname, GLint * params) {
    _pre_call_gles1_callback("glGetFenceivNV", (GLADapiproc) glad_glGetFenceivNV, 3, fence, pname, params);
    glad_glGetFenceivNV(fence, pname, params);
    _post_call_gles1_callback(NULL, "glGetFenceivNV", (GLADapiproc) glad_glGetFenceivNV, 3, fence, pname, params);
    
}
PFNGLGETFENCEIVNVPROC glad_debug_glGetFenceivNV = glad_debug_impl_glGetFenceivNV;
PFNGLGETFIXEDVPROC glad_glGetFixedv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFixedv(GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetFixedv", (GLADapiproc) glad_glGetFixedv, 2, pname, params);
    glad_glGetFixedv(pname, params);
    _post_call_gles1_callback(NULL, "glGetFixedv", (GLADapiproc) glad_glGetFixedv, 2, pname, params);
    
}
PFNGLGETFIXEDVPROC glad_debug_glGetFixedv = glad_debug_impl_glGetFixedv;
PFNGLGETFIXEDVOESPROC glad_glGetFixedvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFixedvOES(GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetFixedvOES", (GLADapiproc) glad_glGetFixedvOES, 2, pname, params);
    glad_glGetFixedvOES(pname, params);
    _post_call_gles1_callback(NULL, "glGetFixedvOES", (GLADapiproc) glad_glGetFixedvOES, 2, pname, params);
    
}
PFNGLGETFIXEDVOESPROC glad_debug_glGetFixedvOES = glad_debug_impl_glGetFixedvOES;
PFNGLGETFLOATVPROC glad_glGetFloatv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFloatv(GLenum pname, GLfloat * data) {
    _pre_call_gles1_callback("glGetFloatv", (GLADapiproc) glad_glGetFloatv, 2, pname, data);
    glad_glGetFloatv(pname, data);
    _post_call_gles1_callback(NULL, "glGetFloatv", (GLADapiproc) glad_glGetFloatv, 2, pname, data);
    
}
PFNGLGETFLOATVPROC glad_debug_glGetFloatv = glad_debug_impl_glGetFloatv;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVOESPROC glad_glGetFramebufferAttachmentParameterivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFramebufferAttachmentParameterivOES(GLenum target, GLenum attachment, GLenum pname, GLint * params) {
    _pre_call_gles1_callback("glGetFramebufferAttachmentParameterivOES", (GLADapiproc) glad_glGetFramebufferAttachmentParameterivOES, 4, target, attachment, pname, params);
    glad_glGetFramebufferAttachmentParameterivOES(target, attachment, pname, params);
    _post_call_gles1_callback(NULL, "glGetFramebufferAttachmentParameterivOES", (GLADapiproc) glad_glGetFramebufferAttachmentParameterivOES, 4, target, attachment, pname, params);
    
}
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVOESPROC glad_debug_glGetFramebufferAttachmentParameterivOES = glad_debug_impl_glGetFramebufferAttachmentParameterivOES;
PFNGLGETGRAPHICSRESETSTATUSEXTPROC glad_glGetGraphicsResetStatusEXT = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glGetGraphicsResetStatusEXT(void) {
    GLenum ret;
    _pre_call_gles1_callback("glGetGraphicsResetStatusEXT", (GLADapiproc) glad_glGetGraphicsResetStatusEXT, 0);
    ret = glad_glGetGraphicsResetStatusEXT();
    _post_call_gles1_callback((void*) &ret, "glGetGraphicsResetStatusEXT", (GLADapiproc) glad_glGetGraphicsResetStatusEXT, 0);
    return ret;
}
PFNGLGETGRAPHICSRESETSTATUSEXTPROC glad_debug_glGetGraphicsResetStatusEXT = glad_debug_impl_glGetGraphicsResetStatusEXT;
PFNGLGETINTEGER64VAPPLEPROC glad_glGetInteger64vAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInteger64vAPPLE(GLenum pname, GLint64 * params) {
    _pre_call_gles1_callback("glGetInteger64vAPPLE", (GLADapiproc) glad_glGetInteger64vAPPLE, 2, pname, params);
    glad_glGetInteger64vAPPLE(pname, params);
    _post_call_gles1_callback(NULL, "glGetInteger64vAPPLE", (GLADapiproc) glad_glGetInteger64vAPPLE, 2, pname, params);
    
}
PFNGLGETINTEGER64VAPPLEPROC glad_debug_glGetInteger64vAPPLE = glad_debug_impl_glGetInteger64vAPPLE;
PFNGLGETINTEGERVPROC glad_glGetIntegerv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetIntegerv(GLenum pname, GLint * data) {
    _pre_call_gles1_callback("glGetIntegerv", (GLADapiproc) glad_glGetIntegerv, 2, pname, data);
    glad_glGetIntegerv(pname, data);
    _post_call_gles1_callback(NULL, "glGetIntegerv", (GLADapiproc) glad_glGetIntegerv, 2, pname, data);
    
}
PFNGLGETINTEGERVPROC glad_debug_glGetIntegerv = glad_debug_impl_glGetIntegerv;
PFNGLGETLIGHTFVPROC glad_glGetLightfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetLightfv(GLenum light, GLenum pname, GLfloat * params) {
    _pre_call_gles1_callback("glGetLightfv", (GLADapiproc) glad_glGetLightfv, 3, light, pname, params);
    glad_glGetLightfv(light, pname, params);
    _post_call_gles1_callback(NULL, "glGetLightfv", (GLADapiproc) glad_glGetLightfv, 3, light, pname, params);
    
}
PFNGLGETLIGHTFVPROC glad_debug_glGetLightfv = glad_debug_impl_glGetLightfv;
PFNGLGETLIGHTXVPROC glad_glGetLightxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetLightxv(GLenum light, GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetLightxv", (GLADapiproc) glad_glGetLightxv, 3, light, pname, params);
    glad_glGetLightxv(light, pname, params);
    _post_call_gles1_callback(NULL, "glGetLightxv", (GLADapiproc) glad_glGetLightxv, 3, light, pname, params);
    
}
PFNGLGETLIGHTXVPROC glad_debug_glGetLightxv = glad_debug_impl_glGetLightxv;
PFNGLGETLIGHTXVOESPROC glad_glGetLightxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetLightxvOES(GLenum light, GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetLightxvOES", (GLADapiproc) glad_glGetLightxvOES, 3, light, pname, params);
    glad_glGetLightxvOES(light, pname, params);
    _post_call_gles1_callback(NULL, "glGetLightxvOES", (GLADapiproc) glad_glGetLightxvOES, 3, light, pname, params);
    
}
PFNGLGETLIGHTXVOESPROC glad_debug_glGetLightxvOES = glad_debug_impl_glGetLightxvOES;
PFNGLGETMATERIALFVPROC glad_glGetMaterialfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMaterialfv(GLenum face, GLenum pname, GLfloat * params) {
    _pre_call_gles1_callback("glGetMaterialfv", (GLADapiproc) glad_glGetMaterialfv, 3, face, pname, params);
    glad_glGetMaterialfv(face, pname, params);
    _post_call_gles1_callback(NULL, "glGetMaterialfv", (GLADapiproc) glad_glGetMaterialfv, 3, face, pname, params);
    
}
PFNGLGETMATERIALFVPROC glad_debug_glGetMaterialfv = glad_debug_impl_glGetMaterialfv;
PFNGLGETMATERIALXVPROC glad_glGetMaterialxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMaterialxv(GLenum face, GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetMaterialxv", (GLADapiproc) glad_glGetMaterialxv, 3, face, pname, params);
    glad_glGetMaterialxv(face, pname, params);
    _post_call_gles1_callback(NULL, "glGetMaterialxv", (GLADapiproc) glad_glGetMaterialxv, 3, face, pname, params);
    
}
PFNGLGETMATERIALXVPROC glad_debug_glGetMaterialxv = glad_debug_impl_glGetMaterialxv;
PFNGLGETMATERIALXVOESPROC glad_glGetMaterialxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMaterialxvOES(GLenum face, GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetMaterialxvOES", (GLADapiproc) glad_glGetMaterialxvOES, 3, face, pname, params);
    glad_glGetMaterialxvOES(face, pname, params);
    _post_call_gles1_callback(NULL, "glGetMaterialxvOES", (GLADapiproc) glad_glGetMaterialxvOES, 3, face, pname, params);
    
}
PFNGLGETMATERIALXVOESPROC glad_debug_glGetMaterialxvOES = glad_debug_impl_glGetMaterialxvOES;
PFNGLGETPOINTERVPROC glad_glGetPointerv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPointerv(GLenum pname, void ** params) {
    _pre_call_gles1_callback("glGetPointerv", (GLADapiproc) glad_glGetPointerv, 2, pname, params);
    glad_glGetPointerv(pname, params);
    _post_call_gles1_callback(NULL, "glGetPointerv", (GLADapiproc) glad_glGetPointerv, 2, pname, params);
    
}
PFNGLGETPOINTERVPROC glad_debug_glGetPointerv = glad_debug_impl_glGetPointerv;
PFNGLGETRENDERBUFFERPARAMETERIVOESPROC glad_glGetRenderbufferParameterivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetRenderbufferParameterivOES(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles1_callback("glGetRenderbufferParameterivOES", (GLADapiproc) glad_glGetRenderbufferParameterivOES, 3, target, pname, params);
    glad_glGetRenderbufferParameterivOES(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetRenderbufferParameterivOES", (GLADapiproc) glad_glGetRenderbufferParameterivOES, 3, target, pname, params);
    
}
PFNGLGETRENDERBUFFERPARAMETERIVOESPROC glad_debug_glGetRenderbufferParameterivOES = glad_debug_impl_glGetRenderbufferParameterivOES;
PFNGLGETSTRINGPROC glad_glGetString = NULL;
static const GLubyte * GLAD_API_PTR glad_debug_impl_glGetString(GLenum name) {
    const GLubyte * ret;
    _pre_call_gles1_callback("glGetString", (GLADapiproc) glad_glGetString, 1, name);
    ret = glad_glGetString(name);
    _post_call_gles1_callback((void*) &ret, "glGetString", (GLADapiproc) glad_glGetString, 1, name);
    return ret;
}
PFNGLGETSTRINGPROC glad_debug_glGetString = glad_debug_impl_glGetString;
PFNGLGETSYNCIVAPPLEPROC glad_glGetSyncivAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSyncivAPPLE(GLsync sync, GLenum pname, GLsizei count, GLsizei * length, GLint * values) {
    _pre_call_gles1_callback("glGetSyncivAPPLE", (GLADapiproc) glad_glGetSyncivAPPLE, 5, sync, pname, count, length, values);
    glad_glGetSyncivAPPLE(sync, pname, count, length, values);
    _post_call_gles1_callback(NULL, "glGetSyncivAPPLE", (GLADapiproc) glad_glGetSyncivAPPLE, 5, sync, pname, count, length, values);
    
}
PFNGLGETSYNCIVAPPLEPROC glad_debug_glGetSyncivAPPLE = glad_debug_impl_glGetSyncivAPPLE;
PFNGLGETTEXENVFVPROC glad_glGetTexEnvfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexEnvfv(GLenum target, GLenum pname, GLfloat * params) {
    _pre_call_gles1_callback("glGetTexEnvfv", (GLADapiproc) glad_glGetTexEnvfv, 3, target, pname, params);
    glad_glGetTexEnvfv(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexEnvfv", (GLADapiproc) glad_glGetTexEnvfv, 3, target, pname, params);
    
}
PFNGLGETTEXENVFVPROC glad_debug_glGetTexEnvfv = glad_debug_impl_glGetTexEnvfv;
PFNGLGETTEXENVIVPROC glad_glGetTexEnviv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexEnviv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles1_callback("glGetTexEnviv", (GLADapiproc) glad_glGetTexEnviv, 3, target, pname, params);
    glad_glGetTexEnviv(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexEnviv", (GLADapiproc) glad_glGetTexEnviv, 3, target, pname, params);
    
}
PFNGLGETTEXENVIVPROC glad_debug_glGetTexEnviv = glad_debug_impl_glGetTexEnviv;
PFNGLGETTEXENVXVPROC glad_glGetTexEnvxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexEnvxv(GLenum target, GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetTexEnvxv", (GLADapiproc) glad_glGetTexEnvxv, 3, target, pname, params);
    glad_glGetTexEnvxv(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexEnvxv", (GLADapiproc) glad_glGetTexEnvxv, 3, target, pname, params);
    
}
PFNGLGETTEXENVXVPROC glad_debug_glGetTexEnvxv = glad_debug_impl_glGetTexEnvxv;
PFNGLGETTEXENVXVOESPROC glad_glGetTexEnvxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexEnvxvOES(GLenum target, GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetTexEnvxvOES", (GLADapiproc) glad_glGetTexEnvxvOES, 3, target, pname, params);
    glad_glGetTexEnvxvOES(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexEnvxvOES", (GLADapiproc) glad_glGetTexEnvxvOES, 3, target, pname, params);
    
}
PFNGLGETTEXENVXVOESPROC glad_debug_glGetTexEnvxvOES = glad_debug_impl_glGetTexEnvxvOES;
PFNGLGETTEXGENFVOESPROC glad_glGetTexGenfvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexGenfvOES(GLenum coord, GLenum pname, GLfloat * params) {
    _pre_call_gles1_callback("glGetTexGenfvOES", (GLADapiproc) glad_glGetTexGenfvOES, 3, coord, pname, params);
    glad_glGetTexGenfvOES(coord, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexGenfvOES", (GLADapiproc) glad_glGetTexGenfvOES, 3, coord, pname, params);
    
}
PFNGLGETTEXGENFVOESPROC glad_debug_glGetTexGenfvOES = glad_debug_impl_glGetTexGenfvOES;
PFNGLGETTEXGENIVOESPROC glad_glGetTexGenivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexGenivOES(GLenum coord, GLenum pname, GLint * params) {
    _pre_call_gles1_callback("glGetTexGenivOES", (GLADapiproc) glad_glGetTexGenivOES, 3, coord, pname, params);
    glad_glGetTexGenivOES(coord, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexGenivOES", (GLADapiproc) glad_glGetTexGenivOES, 3, coord, pname, params);
    
}
PFNGLGETTEXGENIVOESPROC glad_debug_glGetTexGenivOES = glad_debug_impl_glGetTexGenivOES;
PFNGLGETTEXGENXVOESPROC glad_glGetTexGenxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexGenxvOES(GLenum coord, GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetTexGenxvOES", (GLADapiproc) glad_glGetTexGenxvOES, 3, coord, pname, params);
    glad_glGetTexGenxvOES(coord, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexGenxvOES", (GLADapiproc) glad_glGetTexGenxvOES, 3, coord, pname, params);
    
}
PFNGLGETTEXGENXVOESPROC glad_debug_glGetTexGenxvOES = glad_debug_impl_glGetTexGenxvOES;
PFNGLGETTEXPARAMETERFVPROC glad_glGetTexParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat * params) {
    _pre_call_gles1_callback("glGetTexParameterfv", (GLADapiproc) glad_glGetTexParameterfv, 3, target, pname, params);
    glad_glGetTexParameterfv(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexParameterfv", (GLADapiproc) glad_glGetTexParameterfv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERFVPROC glad_debug_glGetTexParameterfv = glad_debug_impl_glGetTexParameterfv;
PFNGLGETTEXPARAMETERIVPROC glad_glGetTexParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameteriv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles1_callback("glGetTexParameteriv", (GLADapiproc) glad_glGetTexParameteriv, 3, target, pname, params);
    glad_glGetTexParameteriv(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexParameteriv", (GLADapiproc) glad_glGetTexParameteriv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIVPROC glad_debug_glGetTexParameteriv = glad_debug_impl_glGetTexParameteriv;
PFNGLGETTEXPARAMETERXVPROC glad_glGetTexParameterxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterxv(GLenum target, GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetTexParameterxv", (GLADapiproc) glad_glGetTexParameterxv, 3, target, pname, params);
    glad_glGetTexParameterxv(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexParameterxv", (GLADapiproc) glad_glGetTexParameterxv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERXVPROC glad_debug_glGetTexParameterxv = glad_debug_impl_glGetTexParameterxv;
PFNGLGETTEXPARAMETERXVOESPROC glad_glGetTexParameterxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterxvOES(GLenum target, GLenum pname, GLfixed * params) {
    _pre_call_gles1_callback("glGetTexParameterxvOES", (GLADapiproc) glad_glGetTexParameterxvOES, 3, target, pname, params);
    glad_glGetTexParameterxvOES(target, pname, params);
    _post_call_gles1_callback(NULL, "glGetTexParameterxvOES", (GLADapiproc) glad_glGetTexParameterxvOES, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERXVOESPROC glad_debug_glGetTexParameterxvOES = glad_debug_impl_glGetTexParameterxvOES;
PFNGLGETNUNIFORMFVEXTPROC glad_glGetnUniformfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformfvEXT(GLuint program, GLint location, GLsizei bufSize, GLfloat * params) {
    _pre_call_gles1_callback("glGetnUniformfvEXT", (GLADapiproc) glad_glGetnUniformfvEXT, 4, program, location, bufSize, params);
    glad_glGetnUniformfvEXT(program, location, bufSize, params);
    _post_call_gles1_callback(NULL, "glGetnUniformfvEXT", (GLADapiproc) glad_glGetnUniformfvEXT, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMFVEXTPROC glad_debug_glGetnUniformfvEXT = glad_debug_impl_glGetnUniformfvEXT;
PFNGLGETNUNIFORMIVEXTPROC glad_glGetnUniformivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformivEXT(GLuint program, GLint location, GLsizei bufSize, GLint * params) {
    _pre_call_gles1_callback("glGetnUniformivEXT", (GLADapiproc) glad_glGetnUniformivEXT, 4, program, location, bufSize, params);
    glad_glGetnUniformivEXT(program, location, bufSize, params);
    _post_call_gles1_callback(NULL, "glGetnUniformivEXT", (GLADapiproc) glad_glGetnUniformivEXT, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMIVEXTPROC glad_debug_glGetnUniformivEXT = glad_debug_impl_glGetnUniformivEXT;
PFNGLHINTPROC glad_glHint = NULL;
static void GLAD_API_PTR glad_debug_impl_glHint(GLenum target, GLenum mode) {
    _pre_call_gles1_callback("glHint", (GLADapiproc) glad_glHint, 2, target, mode);
    glad_glHint(target, mode);
    _post_call_gles1_callback(NULL, "glHint", (GLADapiproc) glad_glHint, 2, target, mode);
    
}
PFNGLHINTPROC glad_debug_glHint = glad_debug_impl_glHint;
PFNGLINSERTEVENTMARKEREXTPROC glad_glInsertEventMarkerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glInsertEventMarkerEXT(GLsizei length, const GLchar * marker) {
    _pre_call_gles1_callback("glInsertEventMarkerEXT", (GLADapiproc) glad_glInsertEventMarkerEXT, 2, length, marker);
    glad_glInsertEventMarkerEXT(length, marker);
    _post_call_gles1_callback(NULL, "glInsertEventMarkerEXT", (GLADapiproc) glad_glInsertEventMarkerEXT, 2, length, marker);
    
}
PFNGLINSERTEVENTMARKEREXTPROC glad_debug_glInsertEventMarkerEXT = glad_debug_impl_glInsertEventMarkerEXT;
PFNGLISBUFFERPROC glad_glIsBuffer = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsBuffer(GLuint buffer) {
    GLboolean ret;
    _pre_call_gles1_callback("glIsBuffer", (GLADapiproc) glad_glIsBuffer, 1, buffer);
    ret = glad_glIsBuffer(buffer);
    _post_call_gles1_callback((void*) &ret, "glIsBuffer", (GLADapiproc) glad_glIsBuffer, 1, buffer);
    return ret;
}
PFNGLISBUFFERPROC glad_debug_glIsBuffer = glad_debug_impl_glIsBuffer;
PFNGLISENABLEDPROC glad_glIsEnabled = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsEnabled(GLenum cap) {
    GLboolean ret;
    _pre_call_gles1_callback("glIsEnabled", (GLADapiproc) glad_glIsEnabled, 1, cap);
    ret = glad_glIsEnabled(cap);
    _post_call_gles1_callback((void*) &ret, "glIsEnabled", (GLADapiproc) glad_glIsEnabled, 1, cap);
    return ret;
}
PFNGLISENABLEDPROC glad_debug_glIsEnabled = glad_debug_impl_glIsEnabled;
PFNGLISFENCENVPROC glad_glIsFenceNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsFenceNV(GLuint fence) {
    GLboolean ret;
    _pre_call_gles1_callback("glIsFenceNV", (GLADapiproc) glad_glIsFenceNV, 1, fence);
    ret = glad_glIsFenceNV(fence);
    _post_call_gles1_callback((void*) &ret, "glIsFenceNV", (GLADapiproc) glad_glIsFenceNV, 1, fence);
    return ret;
}
PFNGLISFENCENVPROC glad_debug_glIsFenceNV = glad_debug_impl_glIsFenceNV;
PFNGLISFRAMEBUFFEROESPROC glad_glIsFramebufferOES = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsFramebufferOES(GLuint framebuffer) {
    GLboolean ret;
    _pre_call_gles1_callback("glIsFramebufferOES", (GLADapiproc) glad_glIsFramebufferOES, 1, framebuffer);
    ret = glad_glIsFramebufferOES(framebuffer);
    _post_call_gles1_callback((void*) &ret, "glIsFramebufferOES", (GLADapiproc) glad_glIsFramebufferOES, 1, framebuffer);
    return ret;
}
PFNGLISFRAMEBUFFEROESPROC glad_debug_glIsFramebufferOES = glad_debug_impl_glIsFramebufferOES;
PFNGLISRENDERBUFFEROESPROC glad_glIsRenderbufferOES = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsRenderbufferOES(GLuint renderbuffer) {
    GLboolean ret;
    _pre_call_gles1_callback("glIsRenderbufferOES", (GLADapiproc) glad_glIsRenderbufferOES, 1, renderbuffer);
    ret = glad_glIsRenderbufferOES(renderbuffer);
    _post_call_gles1_callback((void*) &ret, "glIsRenderbufferOES", (GLADapiproc) glad_glIsRenderbufferOES, 1, renderbuffer);
    return ret;
}
PFNGLISRENDERBUFFEROESPROC glad_debug_glIsRenderbufferOES = glad_debug_impl_glIsRenderbufferOES;
PFNGLISSYNCAPPLEPROC glad_glIsSyncAPPLE = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsSyncAPPLE(GLsync sync) {
    GLboolean ret;
    _pre_call_gles1_callback("glIsSyncAPPLE", (GLADapiproc) glad_glIsSyncAPPLE, 1, sync);
    ret = glad_glIsSyncAPPLE(sync);
    _post_call_gles1_callback((void*) &ret, "glIsSyncAPPLE", (GLADapiproc) glad_glIsSyncAPPLE, 1, sync);
    return ret;
}
PFNGLISSYNCAPPLEPROC glad_debug_glIsSyncAPPLE = glad_debug_impl_glIsSyncAPPLE;
PFNGLISTEXTUREPROC glad_glIsTexture = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsTexture(GLuint texture) {
    GLboolean ret;
    _pre_call_gles1_callback("glIsTexture", (GLADapiproc) glad_glIsTexture, 1, texture);
    ret = glad_glIsTexture(texture);
    _post_call_gles1_callback((void*) &ret, "glIsTexture", (GLADapiproc) glad_glIsTexture, 1, texture);
    return ret;
}
PFNGLISTEXTUREPROC glad_debug_glIsTexture = glad_debug_impl_glIsTexture;
PFNGLISVERTEXARRAYOESPROC glad_glIsVertexArrayOES = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsVertexArrayOES(GLuint array) {
    GLboolean ret;
    _pre_call_gles1_callback("glIsVertexArrayOES", (GLADapiproc) glad_glIsVertexArrayOES, 1, array);
    ret = glad_glIsVertexArrayOES(array);
    _post_call_gles1_callback((void*) &ret, "glIsVertexArrayOES", (GLADapiproc) glad_glIsVertexArrayOES, 1, array);
    return ret;
}
PFNGLISVERTEXARRAYOESPROC glad_debug_glIsVertexArrayOES = glad_debug_impl_glIsVertexArrayOES;
PFNGLLIGHTMODELFPROC glad_glLightModelf = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightModelf(GLenum pname, GLfloat param) {
    _pre_call_gles1_callback("glLightModelf", (GLADapiproc) glad_glLightModelf, 2, pname, param);
    glad_glLightModelf(pname, param);
    _post_call_gles1_callback(NULL, "glLightModelf", (GLADapiproc) glad_glLightModelf, 2, pname, param);
    
}
PFNGLLIGHTMODELFPROC glad_debug_glLightModelf = glad_debug_impl_glLightModelf;
PFNGLLIGHTMODELFVPROC glad_glLightModelfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightModelfv(GLenum pname, const GLfloat * params) {
    _pre_call_gles1_callback("glLightModelfv", (GLADapiproc) glad_glLightModelfv, 2, pname, params);
    glad_glLightModelfv(pname, params);
    _post_call_gles1_callback(NULL, "glLightModelfv", (GLADapiproc) glad_glLightModelfv, 2, pname, params);
    
}
PFNGLLIGHTMODELFVPROC glad_debug_glLightModelfv = glad_debug_impl_glLightModelfv;
PFNGLLIGHTMODELXPROC glad_glLightModelx = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightModelx(GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glLightModelx", (GLADapiproc) glad_glLightModelx, 2, pname, param);
    glad_glLightModelx(pname, param);
    _post_call_gles1_callback(NULL, "glLightModelx", (GLADapiproc) glad_glLightModelx, 2, pname, param);
    
}
PFNGLLIGHTMODELXPROC glad_debug_glLightModelx = glad_debug_impl_glLightModelx;
PFNGLLIGHTMODELXOESPROC glad_glLightModelxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightModelxOES(GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glLightModelxOES", (GLADapiproc) glad_glLightModelxOES, 2, pname, param);
    glad_glLightModelxOES(pname, param);
    _post_call_gles1_callback(NULL, "glLightModelxOES", (GLADapiproc) glad_glLightModelxOES, 2, pname, param);
    
}
PFNGLLIGHTMODELXOESPROC glad_debug_glLightModelxOES = glad_debug_impl_glLightModelxOES;
PFNGLLIGHTMODELXVPROC glad_glLightModelxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightModelxv(GLenum pname, const GLfixed * param) {
    _pre_call_gles1_callback("glLightModelxv", (GLADapiproc) glad_glLightModelxv, 2, pname, param);
    glad_glLightModelxv(pname, param);
    _post_call_gles1_callback(NULL, "glLightModelxv", (GLADapiproc) glad_glLightModelxv, 2, pname, param);
    
}
PFNGLLIGHTMODELXVPROC glad_debug_glLightModelxv = glad_debug_impl_glLightModelxv;
PFNGLLIGHTMODELXVOESPROC glad_glLightModelxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightModelxvOES(GLenum pname, const GLfixed * param) {
    _pre_call_gles1_callback("glLightModelxvOES", (GLADapiproc) glad_glLightModelxvOES, 2, pname, param);
    glad_glLightModelxvOES(pname, param);
    _post_call_gles1_callback(NULL, "glLightModelxvOES", (GLADapiproc) glad_glLightModelxvOES, 2, pname, param);
    
}
PFNGLLIGHTMODELXVOESPROC glad_debug_glLightModelxvOES = glad_debug_impl_glLightModelxvOES;
PFNGLLIGHTFPROC glad_glLightf = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightf(GLenum light, GLenum pname, GLfloat param) {
    _pre_call_gles1_callback("glLightf", (GLADapiproc) glad_glLightf, 3, light, pname, param);
    glad_glLightf(light, pname, param);
    _post_call_gles1_callback(NULL, "glLightf", (GLADapiproc) glad_glLightf, 3, light, pname, param);
    
}
PFNGLLIGHTFPROC glad_debug_glLightf = glad_debug_impl_glLightf;
PFNGLLIGHTFVPROC glad_glLightfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightfv(GLenum light, GLenum pname, const GLfloat * params) {
    _pre_call_gles1_callback("glLightfv", (GLADapiproc) glad_glLightfv, 3, light, pname, params);
    glad_glLightfv(light, pname, params);
    _post_call_gles1_callback(NULL, "glLightfv", (GLADapiproc) glad_glLightfv, 3, light, pname, params);
    
}
PFNGLLIGHTFVPROC glad_debug_glLightfv = glad_debug_impl_glLightfv;
PFNGLLIGHTXPROC glad_glLightx = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightx(GLenum light, GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glLightx", (GLADapiproc) glad_glLightx, 3, light, pname, param);
    glad_glLightx(light, pname, param);
    _post_call_gles1_callback(NULL, "glLightx", (GLADapiproc) glad_glLightx, 3, light, pname, param);
    
}
PFNGLLIGHTXPROC glad_debug_glLightx = glad_debug_impl_glLightx;
PFNGLLIGHTXOESPROC glad_glLightxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightxOES(GLenum light, GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glLightxOES", (GLADapiproc) glad_glLightxOES, 3, light, pname, param);
    glad_glLightxOES(light, pname, param);
    _post_call_gles1_callback(NULL, "glLightxOES", (GLADapiproc) glad_glLightxOES, 3, light, pname, param);
    
}
PFNGLLIGHTXOESPROC glad_debug_glLightxOES = glad_debug_impl_glLightxOES;
PFNGLLIGHTXVPROC glad_glLightxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightxv(GLenum light, GLenum pname, const GLfixed * params) {
    _pre_call_gles1_callback("glLightxv", (GLADapiproc) glad_glLightxv, 3, light, pname, params);
    glad_glLightxv(light, pname, params);
    _post_call_gles1_callback(NULL, "glLightxv", (GLADapiproc) glad_glLightxv, 3, light, pname, params);
    
}
PFNGLLIGHTXVPROC glad_debug_glLightxv = glad_debug_impl_glLightxv;
PFNGLLIGHTXVOESPROC glad_glLightxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glLightxvOES(GLenum light, GLenum pname, const GLfixed * params) {
    _pre_call_gles1_callback("glLightxvOES", (GLADapiproc) glad_glLightxvOES, 3, light, pname, params);
    glad_glLightxvOES(light, pname, params);
    _post_call_gles1_callback(NULL, "glLightxvOES", (GLADapiproc) glad_glLightxvOES, 3, light, pname, params);
    
}
PFNGLLIGHTXVOESPROC glad_debug_glLightxvOES = glad_debug_impl_glLightxvOES;
PFNGLLINEWIDTHPROC glad_glLineWidth = NULL;
static void GLAD_API_PTR glad_debug_impl_glLineWidth(GLfloat width) {
    _pre_call_gles1_callback("glLineWidth", (GLADapiproc) glad_glLineWidth, 1, width);
    glad_glLineWidth(width);
    _post_call_gles1_callback(NULL, "glLineWidth", (GLADapiproc) glad_glLineWidth, 1, width);
    
}
PFNGLLINEWIDTHPROC glad_debug_glLineWidth = glad_debug_impl_glLineWidth;
PFNGLLINEWIDTHXPROC glad_glLineWidthx = NULL;
static void GLAD_API_PTR glad_debug_impl_glLineWidthx(GLfixed width) {
    _pre_call_gles1_callback("glLineWidthx", (GLADapiproc) glad_glLineWidthx, 1, width);
    glad_glLineWidthx(width);
    _post_call_gles1_callback(NULL, "glLineWidthx", (GLADapiproc) glad_glLineWidthx, 1, width);
    
}
PFNGLLINEWIDTHXPROC glad_debug_glLineWidthx = glad_debug_impl_glLineWidthx;
PFNGLLINEWIDTHXOESPROC glad_glLineWidthxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glLineWidthxOES(GLfixed width) {
    _pre_call_gles1_callback("glLineWidthxOES", (GLADapiproc) glad_glLineWidthxOES, 1, width);
    glad_glLineWidthxOES(width);
    _post_call_gles1_callback(NULL, "glLineWidthxOES", (GLADapiproc) glad_glLineWidthxOES, 1, width);
    
}
PFNGLLINEWIDTHXOESPROC glad_debug_glLineWidthxOES = glad_debug_impl_glLineWidthxOES;
PFNGLLOADIDENTITYPROC glad_glLoadIdentity = NULL;
static void GLAD_API_PTR glad_debug_impl_glLoadIdentity(void) {
    _pre_call_gles1_callback("glLoadIdentity", (GLADapiproc) glad_glLoadIdentity, 0);
    glad_glLoadIdentity();
    _post_call_gles1_callback(NULL, "glLoadIdentity", (GLADapiproc) glad_glLoadIdentity, 0);
    
}
PFNGLLOADIDENTITYPROC glad_debug_glLoadIdentity = glad_debug_impl_glLoadIdentity;
PFNGLLOADMATRIXFPROC glad_glLoadMatrixf = NULL;
static void GLAD_API_PTR glad_debug_impl_glLoadMatrixf(const GLfloat * m) {
    _pre_call_gles1_callback("glLoadMatrixf", (GLADapiproc) glad_glLoadMatrixf, 1, m);
    glad_glLoadMatrixf(m);
    _post_call_gles1_callback(NULL, "glLoadMatrixf", (GLADapiproc) glad_glLoadMatrixf, 1, m);
    
}
PFNGLLOADMATRIXFPROC glad_debug_glLoadMatrixf = glad_debug_impl_glLoadMatrixf;
PFNGLLOADMATRIXXPROC glad_glLoadMatrixx = NULL;
static void GLAD_API_PTR glad_debug_impl_glLoadMatrixx(const GLfixed * m) {
    _pre_call_gles1_callback("glLoadMatrixx", (GLADapiproc) glad_glLoadMatrixx, 1, m);
    glad_glLoadMatrixx(m);
    _post_call_gles1_callback(NULL, "glLoadMatrixx", (GLADapiproc) glad_glLoadMatrixx, 1, m);
    
}
PFNGLLOADMATRIXXPROC glad_debug_glLoadMatrixx = glad_debug_impl_glLoadMatrixx;
PFNGLLOADMATRIXXOESPROC glad_glLoadMatrixxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glLoadMatrixxOES(const GLfixed * m) {
    _pre_call_gles1_callback("glLoadMatrixxOES", (GLADapiproc) glad_glLoadMatrixxOES, 1, m);
    glad_glLoadMatrixxOES(m);
    _post_call_gles1_callback(NULL, "glLoadMatrixxOES", (GLADapiproc) glad_glLoadMatrixxOES, 1, m);
    
}
PFNGLLOADMATRIXXOESPROC glad_debug_glLoadMatrixxOES = glad_debug_impl_glLoadMatrixxOES;
PFNGLLOADPALETTEFROMMODELVIEWMATRIXOESPROC glad_glLoadPaletteFromModelViewMatrixOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glLoadPaletteFromModelViewMatrixOES(void) {
    _pre_call_gles1_callback("glLoadPaletteFromModelViewMatrixOES", (GLADapiproc) glad_glLoadPaletteFromModelViewMatrixOES, 0);
    glad_glLoadPaletteFromModelViewMatrixOES();
    _post_call_gles1_callback(NULL, "glLoadPaletteFromModelViewMatrixOES", (GLADapiproc) glad_glLoadPaletteFromModelViewMatrixOES, 0);
    
}
PFNGLLOADPALETTEFROMMODELVIEWMATRIXOESPROC glad_debug_glLoadPaletteFromModelViewMatrixOES = glad_debug_impl_glLoadPaletteFromModelViewMatrixOES;
PFNGLLOGICOPPROC glad_glLogicOp = NULL;
static void GLAD_API_PTR glad_debug_impl_glLogicOp(GLenum opcode) {
    _pre_call_gles1_callback("glLogicOp", (GLADapiproc) glad_glLogicOp, 1, opcode);
    glad_glLogicOp(opcode);
    _post_call_gles1_callback(NULL, "glLogicOp", (GLADapiproc) glad_glLogicOp, 1, opcode);
    
}
PFNGLLOGICOPPROC glad_debug_glLogicOp = glad_debug_impl_glLogicOp;
PFNGLMAPBUFFEROESPROC glad_glMapBufferOES = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapBufferOES(GLenum target, GLenum access) {
    void * ret;
    _pre_call_gles1_callback("glMapBufferOES", (GLADapiproc) glad_glMapBufferOES, 2, target, access);
    ret = glad_glMapBufferOES(target, access);
    _post_call_gles1_callback((void*) &ret, "glMapBufferOES", (GLADapiproc) glad_glMapBufferOES, 2, target, access);
    return ret;
}
PFNGLMAPBUFFEROESPROC glad_debug_glMapBufferOES = glad_debug_impl_glMapBufferOES;
PFNGLMAPBUFFERRANGEEXTPROC glad_glMapBufferRangeEXT = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapBufferRangeEXT(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    void * ret;
    _pre_call_gles1_callback("glMapBufferRangeEXT", (GLADapiproc) glad_glMapBufferRangeEXT, 4, target, offset, length, access);
    ret = glad_glMapBufferRangeEXT(target, offset, length, access);
    _post_call_gles1_callback((void*) &ret, "glMapBufferRangeEXT", (GLADapiproc) glad_glMapBufferRangeEXT, 4, target, offset, length, access);
    return ret;
}
PFNGLMAPBUFFERRANGEEXTPROC glad_debug_glMapBufferRangeEXT = glad_debug_impl_glMapBufferRangeEXT;
PFNGLMATERIALFPROC glad_glMaterialf = NULL;
static void GLAD_API_PTR glad_debug_impl_glMaterialf(GLenum face, GLenum pname, GLfloat param) {
    _pre_call_gles1_callback("glMaterialf", (GLADapiproc) glad_glMaterialf, 3, face, pname, param);
    glad_glMaterialf(face, pname, param);
    _post_call_gles1_callback(NULL, "glMaterialf", (GLADapiproc) glad_glMaterialf, 3, face, pname, param);
    
}
PFNGLMATERIALFPROC glad_debug_glMaterialf = glad_debug_impl_glMaterialf;
PFNGLMATERIALFVPROC glad_glMaterialfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glMaterialfv(GLenum face, GLenum pname, const GLfloat * params) {
    _pre_call_gles1_callback("glMaterialfv", (GLADapiproc) glad_glMaterialfv, 3, face, pname, params);
    glad_glMaterialfv(face, pname, params);
    _post_call_gles1_callback(NULL, "glMaterialfv", (GLADapiproc) glad_glMaterialfv, 3, face, pname, params);
    
}
PFNGLMATERIALFVPROC glad_debug_glMaterialfv = glad_debug_impl_glMaterialfv;
PFNGLMATERIALXPROC glad_glMaterialx = NULL;
static void GLAD_API_PTR glad_debug_impl_glMaterialx(GLenum face, GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glMaterialx", (GLADapiproc) glad_glMaterialx, 3, face, pname, param);
    glad_glMaterialx(face, pname, param);
    _post_call_gles1_callback(NULL, "glMaterialx", (GLADapiproc) glad_glMaterialx, 3, face, pname, param);
    
}
PFNGLMATERIALXPROC glad_debug_glMaterialx = glad_debug_impl_glMaterialx;
PFNGLMATERIALXOESPROC glad_glMaterialxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glMaterialxOES(GLenum face, GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glMaterialxOES", (GLADapiproc) glad_glMaterialxOES, 3, face, pname, param);
    glad_glMaterialxOES(face, pname, param);
    _post_call_gles1_callback(NULL, "glMaterialxOES", (GLADapiproc) glad_glMaterialxOES, 3, face, pname, param);
    
}
PFNGLMATERIALXOESPROC glad_debug_glMaterialxOES = glad_debug_impl_glMaterialxOES;
PFNGLMATERIALXVPROC glad_glMaterialxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glMaterialxv(GLenum face, GLenum pname, const GLfixed * param) {
    _pre_call_gles1_callback("glMaterialxv", (GLADapiproc) glad_glMaterialxv, 3, face, pname, param);
    glad_glMaterialxv(face, pname, param);
    _post_call_gles1_callback(NULL, "glMaterialxv", (GLADapiproc) glad_glMaterialxv, 3, face, pname, param);
    
}
PFNGLMATERIALXVPROC glad_debug_glMaterialxv = glad_debug_impl_glMaterialxv;
PFNGLMATERIALXVOESPROC glad_glMaterialxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glMaterialxvOES(GLenum face, GLenum pname, const GLfixed * param) {
    _pre_call_gles1_callback("glMaterialxvOES", (GLADapiproc) glad_glMaterialxvOES, 3, face, pname, param);
    glad_glMaterialxvOES(face, pname, param);
    _post_call_gles1_callback(NULL, "glMaterialxvOES", (GLADapiproc) glad_glMaterialxvOES, 3, face, pname, param);
    
}
PFNGLMATERIALXVOESPROC glad_debug_glMaterialxvOES = glad_debug_impl_glMaterialxvOES;
PFNGLMATRIXINDEXPOINTEROESPROC glad_glMatrixIndexPointerOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixIndexPointerOES(GLint size, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gles1_callback("glMatrixIndexPointerOES", (GLADapiproc) glad_glMatrixIndexPointerOES, 4, size, type, stride, pointer);
    glad_glMatrixIndexPointerOES(size, type, stride, pointer);
    _post_call_gles1_callback(NULL, "glMatrixIndexPointerOES", (GLADapiproc) glad_glMatrixIndexPointerOES, 4, size, type, stride, pointer);
    
}
PFNGLMATRIXINDEXPOINTEROESPROC glad_debug_glMatrixIndexPointerOES = glad_debug_impl_glMatrixIndexPointerOES;
PFNGLMATRIXMODEPROC glad_glMatrixMode = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMode(GLenum mode) {
    _pre_call_gles1_callback("glMatrixMode", (GLADapiproc) glad_glMatrixMode, 1, mode);
    glad_glMatrixMode(mode);
    _post_call_gles1_callback(NULL, "glMatrixMode", (GLADapiproc) glad_glMatrixMode, 1, mode);
    
}
PFNGLMATRIXMODEPROC glad_debug_glMatrixMode = glad_debug_impl_glMatrixMode;
PFNGLMULTMATRIXFPROC glad_glMultMatrixf = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultMatrixf(const GLfloat * m) {
    _pre_call_gles1_callback("glMultMatrixf", (GLADapiproc) glad_glMultMatrixf, 1, m);
    glad_glMultMatrixf(m);
    _post_call_gles1_callback(NULL, "glMultMatrixf", (GLADapiproc) glad_glMultMatrixf, 1, m);
    
}
PFNGLMULTMATRIXFPROC glad_debug_glMultMatrixf = glad_debug_impl_glMultMatrixf;
PFNGLMULTMATRIXXPROC glad_glMultMatrixx = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultMatrixx(const GLfixed * m) {
    _pre_call_gles1_callback("glMultMatrixx", (GLADapiproc) glad_glMultMatrixx, 1, m);
    glad_glMultMatrixx(m);
    _post_call_gles1_callback(NULL, "glMultMatrixx", (GLADapiproc) glad_glMultMatrixx, 1, m);
    
}
PFNGLMULTMATRIXXPROC glad_debug_glMultMatrixx = glad_debug_impl_glMultMatrixx;
PFNGLMULTMATRIXXOESPROC glad_glMultMatrixxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultMatrixxOES(const GLfixed * m) {
    _pre_call_gles1_callback("glMultMatrixxOES", (GLADapiproc) glad_glMultMatrixxOES, 1, m);
    glad_glMultMatrixxOES(m);
    _post_call_gles1_callback(NULL, "glMultMatrixxOES", (GLADapiproc) glad_glMultMatrixxOES, 1, m);
    
}
PFNGLMULTMATRIXXOESPROC glad_debug_glMultMatrixxOES = glad_debug_impl_glMultMatrixxOES;
PFNGLMULTIDRAWARRAYSEXTPROC glad_glMultiDrawArraysEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawArraysEXT(GLenum mode, const GLint * first, const GLsizei * count, GLsizei primcount) {
    _pre_call_gles1_callback("glMultiDrawArraysEXT", (GLADapiproc) glad_glMultiDrawArraysEXT, 4, mode, first, count, primcount);
    glad_glMultiDrawArraysEXT(mode, first, count, primcount);
    _post_call_gles1_callback(NULL, "glMultiDrawArraysEXT", (GLADapiproc) glad_glMultiDrawArraysEXT, 4, mode, first, count, primcount);
    
}
PFNGLMULTIDRAWARRAYSEXTPROC glad_debug_glMultiDrawArraysEXT = glad_debug_impl_glMultiDrawArraysEXT;
PFNGLMULTIDRAWELEMENTSEXTPROC glad_glMultiDrawElementsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawElementsEXT(GLenum mode, const GLsizei * count, GLenum type, const void *const* indices, GLsizei primcount) {
    _pre_call_gles1_callback("glMultiDrawElementsEXT", (GLADapiproc) glad_glMultiDrawElementsEXT, 5, mode, count, type, indices, primcount);
    glad_glMultiDrawElementsEXT(mode, count, type, indices, primcount);
    _post_call_gles1_callback(NULL, "glMultiDrawElementsEXT", (GLADapiproc) glad_glMultiDrawElementsEXT, 5, mode, count, type, indices, primcount);
    
}
PFNGLMULTIDRAWELEMENTSEXTPROC glad_debug_glMultiDrawElementsEXT = glad_debug_impl_glMultiDrawElementsEXT;
PFNGLMULTITEXCOORD4FPROC glad_glMultiTexCoord4f = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
    _pre_call_gles1_callback("glMultiTexCoord4f", (GLADapiproc) glad_glMultiTexCoord4f, 5, target, s, t, r, q);
    glad_glMultiTexCoord4f(target, s, t, r, q);
    _post_call_gles1_callback(NULL, "glMultiTexCoord4f", (GLADapiproc) glad_glMultiTexCoord4f, 5, target, s, t, r, q);
    
}
PFNGLMULTITEXCOORD4FPROC glad_debug_glMultiTexCoord4f = glad_debug_impl_glMultiTexCoord4f;
PFNGLMULTITEXCOORD4XPROC glad_glMultiTexCoord4x = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4x(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q) {
    _pre_call_gles1_callback("glMultiTexCoord4x", (GLADapiproc) glad_glMultiTexCoord4x, 5, texture, s, t, r, q);
    glad_glMultiTexCoord4x(texture, s, t, r, q);
    _post_call_gles1_callback(NULL, "glMultiTexCoord4x", (GLADapiproc) glad_glMultiTexCoord4x, 5, texture, s, t, r, q);
    
}
PFNGLMULTITEXCOORD4XPROC glad_debug_glMultiTexCoord4x = glad_debug_impl_glMultiTexCoord4x;
PFNGLMULTITEXCOORD4XOESPROC glad_glMultiTexCoord4xOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiTexCoord4xOES(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q) {
    _pre_call_gles1_callback("glMultiTexCoord4xOES", (GLADapiproc) glad_glMultiTexCoord4xOES, 5, texture, s, t, r, q);
    glad_glMultiTexCoord4xOES(texture, s, t, r, q);
    _post_call_gles1_callback(NULL, "glMultiTexCoord4xOES", (GLADapiproc) glad_glMultiTexCoord4xOES, 5, texture, s, t, r, q);
    
}
PFNGLMULTITEXCOORD4XOESPROC glad_debug_glMultiTexCoord4xOES = glad_debug_impl_glMultiTexCoord4xOES;
PFNGLNORMAL3FPROC glad_glNormal3f = NULL;
static void GLAD_API_PTR glad_debug_impl_glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    _pre_call_gles1_callback("glNormal3f", (GLADapiproc) glad_glNormal3f, 3, nx, ny, nz);
    glad_glNormal3f(nx, ny, nz);
    _post_call_gles1_callback(NULL, "glNormal3f", (GLADapiproc) glad_glNormal3f, 3, nx, ny, nz);
    
}
PFNGLNORMAL3FPROC glad_debug_glNormal3f = glad_debug_impl_glNormal3f;
PFNGLNORMAL3XPROC glad_glNormal3x = NULL;
static void GLAD_API_PTR glad_debug_impl_glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz) {
    _pre_call_gles1_callback("glNormal3x", (GLADapiproc) glad_glNormal3x, 3, nx, ny, nz);
    glad_glNormal3x(nx, ny, nz);
    _post_call_gles1_callback(NULL, "glNormal3x", (GLADapiproc) glad_glNormal3x, 3, nx, ny, nz);
    
}
PFNGLNORMAL3XPROC glad_debug_glNormal3x = glad_debug_impl_glNormal3x;
PFNGLNORMAL3XOESPROC glad_glNormal3xOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glNormal3xOES(GLfixed nx, GLfixed ny, GLfixed nz) {
    _pre_call_gles1_callback("glNormal3xOES", (GLADapiproc) glad_glNormal3xOES, 3, nx, ny, nz);
    glad_glNormal3xOES(nx, ny, nz);
    _post_call_gles1_callback(NULL, "glNormal3xOES", (GLADapiproc) glad_glNormal3xOES, 3, nx, ny, nz);
    
}
PFNGLNORMAL3XOESPROC glad_debug_glNormal3xOES = glad_debug_impl_glNormal3xOES;
PFNGLNORMALPOINTERPROC glad_glNormalPointer = NULL;
static void GLAD_API_PTR glad_debug_impl_glNormalPointer(GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gles1_callback("glNormalPointer", (GLADapiproc) glad_glNormalPointer, 3, type, stride, pointer);
    glad_glNormalPointer(type, stride, pointer);
    _post_call_gles1_callback(NULL, "glNormalPointer", (GLADapiproc) glad_glNormalPointer, 3, type, stride, pointer);
    
}
PFNGLNORMALPOINTERPROC glad_debug_glNormalPointer = glad_debug_impl_glNormalPointer;
PFNGLORTHOFPROC glad_glOrthof = NULL;
static void GLAD_API_PTR glad_debug_impl_glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    _pre_call_gles1_callback("glOrthof", (GLADapiproc) glad_glOrthof, 6, l, r, b, t, n, f);
    glad_glOrthof(l, r, b, t, n, f);
    _post_call_gles1_callback(NULL, "glOrthof", (GLADapiproc) glad_glOrthof, 6, l, r, b, t, n, f);
    
}
PFNGLORTHOFPROC glad_debug_glOrthof = glad_debug_impl_glOrthof;
PFNGLORTHOFOESPROC glad_glOrthofOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glOrthofOES(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    _pre_call_gles1_callback("glOrthofOES", (GLADapiproc) glad_glOrthofOES, 6, l, r, b, t, n, f);
    glad_glOrthofOES(l, r, b, t, n, f);
    _post_call_gles1_callback(NULL, "glOrthofOES", (GLADapiproc) glad_glOrthofOES, 6, l, r, b, t, n, f);
    
}
PFNGLORTHOFOESPROC glad_debug_glOrthofOES = glad_debug_impl_glOrthofOES;
PFNGLORTHOXPROC glad_glOrthox = NULL;
static void GLAD_API_PTR glad_debug_impl_glOrthox(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f) {
    _pre_call_gles1_callback("glOrthox", (GLADapiproc) glad_glOrthox, 6, l, r, b, t, n, f);
    glad_glOrthox(l, r, b, t, n, f);
    _post_call_gles1_callback(NULL, "glOrthox", (GLADapiproc) glad_glOrthox, 6, l, r, b, t, n, f);
    
}
PFNGLORTHOXPROC glad_debug_glOrthox = glad_debug_impl_glOrthox;
PFNGLORTHOXOESPROC glad_glOrthoxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glOrthoxOES(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f) {
    _pre_call_gles1_callback("glOrthoxOES", (GLADapiproc) glad_glOrthoxOES, 6, l, r, b, t, n, f);
    glad_glOrthoxOES(l, r, b, t, n, f);
    _post_call_gles1_callback(NULL, "glOrthoxOES", (GLADapiproc) glad_glOrthoxOES, 6, l, r, b, t, n, f);
    
}
PFNGLORTHOXOESPROC glad_debug_glOrthoxOES = glad_debug_impl_glOrthoxOES;
PFNGLPIXELSTOREIPROC glad_glPixelStorei = NULL;
static void GLAD_API_PTR glad_debug_impl_glPixelStorei(GLenum pname, GLint param) {
    _pre_call_gles1_callback("glPixelStorei", (GLADapiproc) glad_glPixelStorei, 2, pname, param);
    glad_glPixelStorei(pname, param);
    _post_call_gles1_callback(NULL, "glPixelStorei", (GLADapiproc) glad_glPixelStorei, 2, pname, param);
    
}
PFNGLPIXELSTOREIPROC glad_debug_glPixelStorei = glad_debug_impl_glPixelStorei;
PFNGLPOINTPARAMETERFPROC glad_glPointParameterf = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterf(GLenum pname, GLfloat param) {
    _pre_call_gles1_callback("glPointParameterf", (GLADapiproc) glad_glPointParameterf, 2, pname, param);
    glad_glPointParameterf(pname, param);
    _post_call_gles1_callback(NULL, "glPointParameterf", (GLADapiproc) glad_glPointParameterf, 2, pname, param);
    
}
PFNGLPOINTPARAMETERFPROC glad_debug_glPointParameterf = glad_debug_impl_glPointParameterf;
PFNGLPOINTPARAMETERFVPROC glad_glPointParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterfv(GLenum pname, const GLfloat * params) {
    _pre_call_gles1_callback("glPointParameterfv", (GLADapiproc) glad_glPointParameterfv, 2, pname, params);
    glad_glPointParameterfv(pname, params);
    _post_call_gles1_callback(NULL, "glPointParameterfv", (GLADapiproc) glad_glPointParameterfv, 2, pname, params);
    
}
PFNGLPOINTPARAMETERFVPROC glad_debug_glPointParameterfv = glad_debug_impl_glPointParameterfv;
PFNGLPOINTPARAMETERXPROC glad_glPointParameterx = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterx(GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glPointParameterx", (GLADapiproc) glad_glPointParameterx, 2, pname, param);
    glad_glPointParameterx(pname, param);
    _post_call_gles1_callback(NULL, "glPointParameterx", (GLADapiproc) glad_glPointParameterx, 2, pname, param);
    
}
PFNGLPOINTPARAMETERXPROC glad_debug_glPointParameterx = glad_debug_impl_glPointParameterx;
PFNGLPOINTPARAMETERXOESPROC glad_glPointParameterxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterxOES(GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glPointParameterxOES", (GLADapiproc) glad_glPointParameterxOES, 2, pname, param);
    glad_glPointParameterxOES(pname, param);
    _post_call_gles1_callback(NULL, "glPointParameterxOES", (GLADapiproc) glad_glPointParameterxOES, 2, pname, param);
    
}
PFNGLPOINTPARAMETERXOESPROC glad_debug_glPointParameterxOES = glad_debug_impl_glPointParameterxOES;
PFNGLPOINTPARAMETERXVPROC glad_glPointParameterxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterxv(GLenum pname, const GLfixed * params) {
    _pre_call_gles1_callback("glPointParameterxv", (GLADapiproc) glad_glPointParameterxv, 2, pname, params);
    glad_glPointParameterxv(pname, params);
    _post_call_gles1_callback(NULL, "glPointParameterxv", (GLADapiproc) glad_glPointParameterxv, 2, pname, params);
    
}
PFNGLPOINTPARAMETERXVPROC glad_debug_glPointParameterxv = glad_debug_impl_glPointParameterxv;
PFNGLPOINTPARAMETERXVOESPROC glad_glPointParameterxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointParameterxvOES(GLenum pname, const GLfixed * params) {
    _pre_call_gles1_callback("glPointParameterxvOES", (GLADapiproc) glad_glPointParameterxvOES, 2, pname, params);
    glad_glPointParameterxvOES(pname, params);
    _post_call_gles1_callback(NULL, "glPointParameterxvOES", (GLADapiproc) glad_glPointParameterxvOES, 2, pname, params);
    
}
PFNGLPOINTPARAMETERXVOESPROC glad_debug_glPointParameterxvOES = glad_debug_impl_glPointParameterxvOES;
PFNGLPOINTSIZEPROC glad_glPointSize = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointSize(GLfloat size) {
    _pre_call_gles1_callback("glPointSize", (GLADapiproc) glad_glPointSize, 1, size);
    glad_glPointSize(size);
    _post_call_gles1_callback(NULL, "glPointSize", (GLADapiproc) glad_glPointSize, 1, size);
    
}
PFNGLPOINTSIZEPROC glad_debug_glPointSize = glad_debug_impl_glPointSize;
PFNGLPOINTSIZEPOINTEROESPROC glad_glPointSizePointerOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointSizePointerOES(GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gles1_callback("glPointSizePointerOES", (GLADapiproc) glad_glPointSizePointerOES, 3, type, stride, pointer);
    glad_glPointSizePointerOES(type, stride, pointer);
    _post_call_gles1_callback(NULL, "glPointSizePointerOES", (GLADapiproc) glad_glPointSizePointerOES, 3, type, stride, pointer);
    
}
PFNGLPOINTSIZEPOINTEROESPROC glad_debug_glPointSizePointerOES = glad_debug_impl_glPointSizePointerOES;
PFNGLPOINTSIZEXPROC glad_glPointSizex = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointSizex(GLfixed size) {
    _pre_call_gles1_callback("glPointSizex", (GLADapiproc) glad_glPointSizex, 1, size);
    glad_glPointSizex(size);
    _post_call_gles1_callback(NULL, "glPointSizex", (GLADapiproc) glad_glPointSizex, 1, size);
    
}
PFNGLPOINTSIZEXPROC glad_debug_glPointSizex = glad_debug_impl_glPointSizex;
PFNGLPOINTSIZEXOESPROC glad_glPointSizexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glPointSizexOES(GLfixed size) {
    _pre_call_gles1_callback("glPointSizexOES", (GLADapiproc) glad_glPointSizexOES, 1, size);
    glad_glPointSizexOES(size);
    _post_call_gles1_callback(NULL, "glPointSizexOES", (GLADapiproc) glad_glPointSizexOES, 1, size);
    
}
PFNGLPOINTSIZEXOESPROC glad_debug_glPointSizexOES = glad_debug_impl_glPointSizexOES;
PFNGLPOLYGONOFFSETPROC glad_glPolygonOffset = NULL;
static void GLAD_API_PTR glad_debug_impl_glPolygonOffset(GLfloat factor, GLfloat units) {
    _pre_call_gles1_callback("glPolygonOffset", (GLADapiproc) glad_glPolygonOffset, 2, factor, units);
    glad_glPolygonOffset(factor, units);
    _post_call_gles1_callback(NULL, "glPolygonOffset", (GLADapiproc) glad_glPolygonOffset, 2, factor, units);
    
}
PFNGLPOLYGONOFFSETPROC glad_debug_glPolygonOffset = glad_debug_impl_glPolygonOffset;
PFNGLPOLYGONOFFSETXPROC glad_glPolygonOffsetx = NULL;
static void GLAD_API_PTR glad_debug_impl_glPolygonOffsetx(GLfixed factor, GLfixed units) {
    _pre_call_gles1_callback("glPolygonOffsetx", (GLADapiproc) glad_glPolygonOffsetx, 2, factor, units);
    glad_glPolygonOffsetx(factor, units);
    _post_call_gles1_callback(NULL, "glPolygonOffsetx", (GLADapiproc) glad_glPolygonOffsetx, 2, factor, units);
    
}
PFNGLPOLYGONOFFSETXPROC glad_debug_glPolygonOffsetx = glad_debug_impl_glPolygonOffsetx;
PFNGLPOLYGONOFFSETXOESPROC glad_glPolygonOffsetxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glPolygonOffsetxOES(GLfixed factor, GLfixed units) {
    _pre_call_gles1_callback("glPolygonOffsetxOES", (GLADapiproc) glad_glPolygonOffsetxOES, 2, factor, units);
    glad_glPolygonOffsetxOES(factor, units);
    _post_call_gles1_callback(NULL, "glPolygonOffsetxOES", (GLADapiproc) glad_glPolygonOffsetxOES, 2, factor, units);
    
}
PFNGLPOLYGONOFFSETXOESPROC glad_debug_glPolygonOffsetxOES = glad_debug_impl_glPolygonOffsetxOES;
PFNGLPOPGROUPMARKEREXTPROC glad_glPopGroupMarkerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPopGroupMarkerEXT(void) {
    _pre_call_gles1_callback("glPopGroupMarkerEXT", (GLADapiproc) glad_glPopGroupMarkerEXT, 0);
    glad_glPopGroupMarkerEXT();
    _post_call_gles1_callback(NULL, "glPopGroupMarkerEXT", (GLADapiproc) glad_glPopGroupMarkerEXT, 0);
    
}
PFNGLPOPGROUPMARKEREXTPROC glad_debug_glPopGroupMarkerEXT = glad_debug_impl_glPopGroupMarkerEXT;
PFNGLPOPMATRIXPROC glad_glPopMatrix = NULL;
static void GLAD_API_PTR glad_debug_impl_glPopMatrix(void) {
    _pre_call_gles1_callback("glPopMatrix", (GLADapiproc) glad_glPopMatrix, 0);
    glad_glPopMatrix();
    _post_call_gles1_callback(NULL, "glPopMatrix", (GLADapiproc) glad_glPopMatrix, 0);
    
}
PFNGLPOPMATRIXPROC glad_debug_glPopMatrix = glad_debug_impl_glPopMatrix;
PFNGLPUSHGROUPMARKEREXTPROC glad_glPushGroupMarkerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPushGroupMarkerEXT(GLsizei length, const GLchar * marker) {
    _pre_call_gles1_callback("glPushGroupMarkerEXT", (GLADapiproc) glad_glPushGroupMarkerEXT, 2, length, marker);
    glad_glPushGroupMarkerEXT(length, marker);
    _post_call_gles1_callback(NULL, "glPushGroupMarkerEXT", (GLADapiproc) glad_glPushGroupMarkerEXT, 2, length, marker);
    
}
PFNGLPUSHGROUPMARKEREXTPROC glad_debug_glPushGroupMarkerEXT = glad_debug_impl_glPushGroupMarkerEXT;
PFNGLPUSHMATRIXPROC glad_glPushMatrix = NULL;
static void GLAD_API_PTR glad_debug_impl_glPushMatrix(void) {
    _pre_call_gles1_callback("glPushMatrix", (GLADapiproc) glad_glPushMatrix, 0);
    glad_glPushMatrix();
    _post_call_gles1_callback(NULL, "glPushMatrix", (GLADapiproc) glad_glPushMatrix, 0);
    
}
PFNGLPUSHMATRIXPROC glad_debug_glPushMatrix = glad_debug_impl_glPushMatrix;
PFNGLQUERYMATRIXXOESPROC glad_glQueryMatrixxOES = NULL;
static GLbitfield GLAD_API_PTR glad_debug_impl_glQueryMatrixxOES(GLfixed * mantissa, GLint * exponent) {
    GLbitfield ret;
    _pre_call_gles1_callback("glQueryMatrixxOES", (GLADapiproc) glad_glQueryMatrixxOES, 2, mantissa, exponent);
    ret = glad_glQueryMatrixxOES(mantissa, exponent);
    _post_call_gles1_callback((void*) &ret, "glQueryMatrixxOES", (GLADapiproc) glad_glQueryMatrixxOES, 2, mantissa, exponent);
    return ret;
}
PFNGLQUERYMATRIXXOESPROC glad_debug_glQueryMatrixxOES = glad_debug_impl_glQueryMatrixxOES;
PFNGLREADPIXELSPROC glad_glReadPixels = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void * pixels) {
    _pre_call_gles1_callback("glReadPixels", (GLADapiproc) glad_glReadPixels, 7, x, y, width, height, format, type, pixels);
    glad_glReadPixels(x, y, width, height, format, type, pixels);
    _post_call_gles1_callback(NULL, "glReadPixels", (GLADapiproc) glad_glReadPixels, 7, x, y, width, height, format, type, pixels);
    
}
PFNGLREADPIXELSPROC glad_debug_glReadPixels = glad_debug_impl_glReadPixels;
PFNGLREADNPIXELSEXTPROC glad_glReadnPixelsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadnPixelsEXT(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void * data) {
    _pre_call_gles1_callback("glReadnPixelsEXT", (GLADapiproc) glad_glReadnPixelsEXT, 8, x, y, width, height, format, type, bufSize, data);
    glad_glReadnPixelsEXT(x, y, width, height, format, type, bufSize, data);
    _post_call_gles1_callback(NULL, "glReadnPixelsEXT", (GLADapiproc) glad_glReadnPixelsEXT, 8, x, y, width, height, format, type, bufSize, data);
    
}
PFNGLREADNPIXELSEXTPROC glad_debug_glReadnPixelsEXT = glad_debug_impl_glReadnPixelsEXT;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC glad_glRenderbufferStorageMultisampleAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleAPPLE(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles1_callback("glRenderbufferStorageMultisampleAPPLE", (GLADapiproc) glad_glRenderbufferStorageMultisampleAPPLE, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleAPPLE(target, samples, internalformat, width, height);
    _post_call_gles1_callback(NULL, "glRenderbufferStorageMultisampleAPPLE", (GLADapiproc) glad_glRenderbufferStorageMultisampleAPPLE, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC glad_debug_glRenderbufferStorageMultisampleAPPLE = glad_debug_impl_glRenderbufferStorageMultisampleAPPLE;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glad_glRenderbufferStorageMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles1_callback("glRenderbufferStorageMultisampleEXT", (GLADapiproc) glad_glRenderbufferStorageMultisampleEXT, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width, height);
    _post_call_gles1_callback(NULL, "glRenderbufferStorageMultisampleEXT", (GLADapiproc) glad_glRenderbufferStorageMultisampleEXT, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glad_debug_glRenderbufferStorageMultisampleEXT = glad_debug_impl_glRenderbufferStorageMultisampleEXT;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC glad_glRenderbufferStorageMultisampleIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleIMG(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles1_callback("glRenderbufferStorageMultisampleIMG", (GLADapiproc) glad_glRenderbufferStorageMultisampleIMG, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleIMG(target, samples, internalformat, width, height);
    _post_call_gles1_callback(NULL, "glRenderbufferStorageMultisampleIMG", (GLADapiproc) glad_glRenderbufferStorageMultisampleIMG, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC glad_debug_glRenderbufferStorageMultisampleIMG = glad_debug_impl_glRenderbufferStorageMultisampleIMG;
PFNGLRENDERBUFFERSTORAGEOESPROC glad_glRenderbufferStorageOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageOES(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles1_callback("glRenderbufferStorageOES", (GLADapiproc) glad_glRenderbufferStorageOES, 4, target, internalformat, width, height);
    glad_glRenderbufferStorageOES(target, internalformat, width, height);
    _post_call_gles1_callback(NULL, "glRenderbufferStorageOES", (GLADapiproc) glad_glRenderbufferStorageOES, 4, target, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEOESPROC glad_debug_glRenderbufferStorageOES = glad_debug_impl_glRenderbufferStorageOES;
PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC glad_glResolveMultisampleFramebufferAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glResolveMultisampleFramebufferAPPLE(void) {
    _pre_call_gles1_callback("glResolveMultisampleFramebufferAPPLE", (GLADapiproc) glad_glResolveMultisampleFramebufferAPPLE, 0);
    glad_glResolveMultisampleFramebufferAPPLE();
    _post_call_gles1_callback(NULL, "glResolveMultisampleFramebufferAPPLE", (GLADapiproc) glad_glResolveMultisampleFramebufferAPPLE, 0);
    
}
PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC glad_debug_glResolveMultisampleFramebufferAPPLE = glad_debug_impl_glResolveMultisampleFramebufferAPPLE;
PFNGLROTATEFPROC glad_glRotatef = NULL;
static void GLAD_API_PTR glad_debug_impl_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gles1_callback("glRotatef", (GLADapiproc) glad_glRotatef, 4, angle, x, y, z);
    glad_glRotatef(angle, x, y, z);
    _post_call_gles1_callback(NULL, "glRotatef", (GLADapiproc) glad_glRotatef, 4, angle, x, y, z);
    
}
PFNGLROTATEFPROC glad_debug_glRotatef = glad_debug_impl_glRotatef;
PFNGLROTATEXPROC glad_glRotatex = NULL;
static void GLAD_API_PTR glad_debug_impl_glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z) {
    _pre_call_gles1_callback("glRotatex", (GLADapiproc) glad_glRotatex, 4, angle, x, y, z);
    glad_glRotatex(angle, x, y, z);
    _post_call_gles1_callback(NULL, "glRotatex", (GLADapiproc) glad_glRotatex, 4, angle, x, y, z);
    
}
PFNGLROTATEXPROC glad_debug_glRotatex = glad_debug_impl_glRotatex;
PFNGLROTATEXOESPROC glad_glRotatexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glRotatexOES(GLfixed angle, GLfixed x, GLfixed y, GLfixed z) {
    _pre_call_gles1_callback("glRotatexOES", (GLADapiproc) glad_glRotatexOES, 4, angle, x, y, z);
    glad_glRotatexOES(angle, x, y, z);
    _post_call_gles1_callback(NULL, "glRotatexOES", (GLADapiproc) glad_glRotatexOES, 4, angle, x, y, z);
    
}
PFNGLROTATEXOESPROC glad_debug_glRotatexOES = glad_debug_impl_glRotatexOES;
PFNGLSAMPLECOVERAGEPROC glad_glSampleCoverage = NULL;
static void GLAD_API_PTR glad_debug_impl_glSampleCoverage(GLfloat value, GLboolean invert) {
    _pre_call_gles1_callback("glSampleCoverage", (GLADapiproc) glad_glSampleCoverage, 2, value, invert);
    glad_glSampleCoverage(value, invert);
    _post_call_gles1_callback(NULL, "glSampleCoverage", (GLADapiproc) glad_glSampleCoverage, 2, value, invert);
    
}
PFNGLSAMPLECOVERAGEPROC glad_debug_glSampleCoverage = glad_debug_impl_glSampleCoverage;
PFNGLSAMPLECOVERAGEXPROC glad_glSampleCoveragex = NULL;
static void GLAD_API_PTR glad_debug_impl_glSampleCoveragex(GLclampx value, GLboolean invert) {
    _pre_call_gles1_callback("glSampleCoveragex", (GLADapiproc) glad_glSampleCoveragex, 2, value, invert);
    glad_glSampleCoveragex(value, invert);
    _post_call_gles1_callback(NULL, "glSampleCoveragex", (GLADapiproc) glad_glSampleCoveragex, 2, value, invert);
    
}
PFNGLSAMPLECOVERAGEXPROC glad_debug_glSampleCoveragex = glad_debug_impl_glSampleCoveragex;
PFNGLSAMPLECOVERAGEXOESPROC glad_glSampleCoveragexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glSampleCoveragexOES(GLclampx value, GLboolean invert) {
    _pre_call_gles1_callback("glSampleCoveragexOES", (GLADapiproc) glad_glSampleCoveragexOES, 2, value, invert);
    glad_glSampleCoveragexOES(value, invert);
    _post_call_gles1_callback(NULL, "glSampleCoveragexOES", (GLADapiproc) glad_glSampleCoveragexOES, 2, value, invert);
    
}
PFNGLSAMPLECOVERAGEXOESPROC glad_debug_glSampleCoveragexOES = glad_debug_impl_glSampleCoveragexOES;
PFNGLSCALEFPROC glad_glScalef = NULL;
static void GLAD_API_PTR glad_debug_impl_glScalef(GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gles1_callback("glScalef", (GLADapiproc) glad_glScalef, 3, x, y, z);
    glad_glScalef(x, y, z);
    _post_call_gles1_callback(NULL, "glScalef", (GLADapiproc) glad_glScalef, 3, x, y, z);
    
}
PFNGLSCALEFPROC glad_debug_glScalef = glad_debug_impl_glScalef;
PFNGLSCALEXPROC glad_glScalex = NULL;
static void GLAD_API_PTR glad_debug_impl_glScalex(GLfixed x, GLfixed y, GLfixed z) {
    _pre_call_gles1_callback("glScalex", (GLADapiproc) glad_glScalex, 3, x, y, z);
    glad_glScalex(x, y, z);
    _post_call_gles1_callback(NULL, "glScalex", (GLADapiproc) glad_glScalex, 3, x, y, z);
    
}
PFNGLSCALEXPROC glad_debug_glScalex = glad_debug_impl_glScalex;
PFNGLSCALEXOESPROC glad_glScalexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glScalexOES(GLfixed x, GLfixed y, GLfixed z) {
    _pre_call_gles1_callback("glScalexOES", (GLADapiproc) glad_glScalexOES, 3, x, y, z);
    glad_glScalexOES(x, y, z);
    _post_call_gles1_callback(NULL, "glScalexOES", (GLADapiproc) glad_glScalexOES, 3, x, y, z);
    
}
PFNGLSCALEXOESPROC glad_debug_glScalexOES = glad_debug_impl_glScalexOES;
PFNGLSCISSORPROC glad_glScissor = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles1_callback("glScissor", (GLADapiproc) glad_glScissor, 4, x, y, width, height);
    glad_glScissor(x, y, width, height);
    _post_call_gles1_callback(NULL, "glScissor", (GLADapiproc) glad_glScissor, 4, x, y, width, height);
    
}
PFNGLSCISSORPROC glad_debug_glScissor = glad_debug_impl_glScissor;
PFNGLSETFENCENVPROC glad_glSetFenceNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glSetFenceNV(GLuint fence, GLenum condition) {
    _pre_call_gles1_callback("glSetFenceNV", (GLADapiproc) glad_glSetFenceNV, 2, fence, condition);
    glad_glSetFenceNV(fence, condition);
    _post_call_gles1_callback(NULL, "glSetFenceNV", (GLADapiproc) glad_glSetFenceNV, 2, fence, condition);
    
}
PFNGLSETFENCENVPROC glad_debug_glSetFenceNV = glad_debug_impl_glSetFenceNV;
PFNGLSHADEMODELPROC glad_glShadeModel = NULL;
static void GLAD_API_PTR glad_debug_impl_glShadeModel(GLenum mode) {
    _pre_call_gles1_callback("glShadeModel", (GLADapiproc) glad_glShadeModel, 1, mode);
    glad_glShadeModel(mode);
    _post_call_gles1_callback(NULL, "glShadeModel", (GLADapiproc) glad_glShadeModel, 1, mode);
    
}
PFNGLSHADEMODELPROC glad_debug_glShadeModel = glad_debug_impl_glShadeModel;
PFNGLSTARTTILINGQCOMPROC glad_glStartTilingQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glStartTilingQCOM(GLuint x, GLuint y, GLuint width, GLuint height, GLbitfield preserveMask) {
    _pre_call_gles1_callback("glStartTilingQCOM", (GLADapiproc) glad_glStartTilingQCOM, 5, x, y, width, height, preserveMask);
    glad_glStartTilingQCOM(x, y, width, height, preserveMask);
    _post_call_gles1_callback(NULL, "glStartTilingQCOM", (GLADapiproc) glad_glStartTilingQCOM, 5, x, y, width, height, preserveMask);
    
}
PFNGLSTARTTILINGQCOMPROC glad_debug_glStartTilingQCOM = glad_debug_impl_glStartTilingQCOM;
PFNGLSTENCILFUNCPROC glad_glStencilFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    _pre_call_gles1_callback("glStencilFunc", (GLADapiproc) glad_glStencilFunc, 3, func, ref, mask);
    glad_glStencilFunc(func, ref, mask);
    _post_call_gles1_callback(NULL, "glStencilFunc", (GLADapiproc) glad_glStencilFunc, 3, func, ref, mask);
    
}
PFNGLSTENCILFUNCPROC glad_debug_glStencilFunc = glad_debug_impl_glStencilFunc;
PFNGLSTENCILMASKPROC glad_glStencilMask = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilMask(GLuint mask) {
    _pre_call_gles1_callback("glStencilMask", (GLADapiproc) glad_glStencilMask, 1, mask);
    glad_glStencilMask(mask);
    _post_call_gles1_callback(NULL, "glStencilMask", (GLADapiproc) glad_glStencilMask, 1, mask);
    
}
PFNGLSTENCILMASKPROC glad_debug_glStencilMask = glad_debug_impl_glStencilMask;
PFNGLSTENCILOPPROC glad_glStencilOp = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    _pre_call_gles1_callback("glStencilOp", (GLADapiproc) glad_glStencilOp, 3, fail, zfail, zpass);
    glad_glStencilOp(fail, zfail, zpass);
    _post_call_gles1_callback(NULL, "glStencilOp", (GLADapiproc) glad_glStencilOp, 3, fail, zfail, zpass);
    
}
PFNGLSTENCILOPPROC glad_debug_glStencilOp = glad_debug_impl_glStencilOp;
PFNGLTESTFENCENVPROC glad_glTestFenceNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glTestFenceNV(GLuint fence) {
    GLboolean ret;
    _pre_call_gles1_callback("glTestFenceNV", (GLADapiproc) glad_glTestFenceNV, 1, fence);
    ret = glad_glTestFenceNV(fence);
    _post_call_gles1_callback((void*) &ret, "glTestFenceNV", (GLADapiproc) glad_glTestFenceNV, 1, fence);
    return ret;
}
PFNGLTESTFENCENVPROC glad_debug_glTestFenceNV = glad_debug_impl_glTestFenceNV;
PFNGLTEXCOORDPOINTERPROC glad_glTexCoordPointer = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gles1_callback("glTexCoordPointer", (GLADapiproc) glad_glTexCoordPointer, 4, size, type, stride, pointer);
    glad_glTexCoordPointer(size, type, stride, pointer);
    _post_call_gles1_callback(NULL, "glTexCoordPointer", (GLADapiproc) glad_glTexCoordPointer, 4, size, type, stride, pointer);
    
}
PFNGLTEXCOORDPOINTERPROC glad_debug_glTexCoordPointer = glad_debug_impl_glTexCoordPointer;
PFNGLTEXENVFPROC glad_glTexEnvf = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    _pre_call_gles1_callback("glTexEnvf", (GLADapiproc) glad_glTexEnvf, 3, target, pname, param);
    glad_glTexEnvf(target, pname, param);
    _post_call_gles1_callback(NULL, "glTexEnvf", (GLADapiproc) glad_glTexEnvf, 3, target, pname, param);
    
}
PFNGLTEXENVFPROC glad_debug_glTexEnvf = glad_debug_impl_glTexEnvf;
PFNGLTEXENVFVPROC glad_glTexEnvfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEnvfv(GLenum target, GLenum pname, const GLfloat * params) {
    _pre_call_gles1_callback("glTexEnvfv", (GLADapiproc) glad_glTexEnvfv, 3, target, pname, params);
    glad_glTexEnvfv(target, pname, params);
    _post_call_gles1_callback(NULL, "glTexEnvfv", (GLADapiproc) glad_glTexEnvfv, 3, target, pname, params);
    
}
PFNGLTEXENVFVPROC glad_debug_glTexEnvfv = glad_debug_impl_glTexEnvfv;
PFNGLTEXENVIPROC glad_glTexEnvi = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEnvi(GLenum target, GLenum pname, GLint param) {
    _pre_call_gles1_callback("glTexEnvi", (GLADapiproc) glad_glTexEnvi, 3, target, pname, param);
    glad_glTexEnvi(target, pname, param);
    _post_call_gles1_callback(NULL, "glTexEnvi", (GLADapiproc) glad_glTexEnvi, 3, target, pname, param);
    
}
PFNGLTEXENVIPROC glad_debug_glTexEnvi = glad_debug_impl_glTexEnvi;
PFNGLTEXENVIVPROC glad_glTexEnviv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEnviv(GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gles1_callback("glTexEnviv", (GLADapiproc) glad_glTexEnviv, 3, target, pname, params);
    glad_glTexEnviv(target, pname, params);
    _post_call_gles1_callback(NULL, "glTexEnviv", (GLADapiproc) glad_glTexEnviv, 3, target, pname, params);
    
}
PFNGLTEXENVIVPROC glad_debug_glTexEnviv = glad_debug_impl_glTexEnviv;
PFNGLTEXENVXPROC glad_glTexEnvx = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEnvx(GLenum target, GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glTexEnvx", (GLADapiproc) glad_glTexEnvx, 3, target, pname, param);
    glad_glTexEnvx(target, pname, param);
    _post_call_gles1_callback(NULL, "glTexEnvx", (GLADapiproc) glad_glTexEnvx, 3, target, pname, param);
    
}
PFNGLTEXENVXPROC glad_debug_glTexEnvx = glad_debug_impl_glTexEnvx;
PFNGLTEXENVXOESPROC glad_glTexEnvxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEnvxOES(GLenum target, GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glTexEnvxOES", (GLADapiproc) glad_glTexEnvxOES, 3, target, pname, param);
    glad_glTexEnvxOES(target, pname, param);
    _post_call_gles1_callback(NULL, "glTexEnvxOES", (GLADapiproc) glad_glTexEnvxOES, 3, target, pname, param);
    
}
PFNGLTEXENVXOESPROC glad_debug_glTexEnvxOES = glad_debug_impl_glTexEnvxOES;
PFNGLTEXENVXVPROC glad_glTexEnvxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEnvxv(GLenum target, GLenum pname, const GLfixed * params) {
    _pre_call_gles1_callback("glTexEnvxv", (GLADapiproc) glad_glTexEnvxv, 3, target, pname, params);
    glad_glTexEnvxv(target, pname, params);
    _post_call_gles1_callback(NULL, "glTexEnvxv", (GLADapiproc) glad_glTexEnvxv, 3, target, pname, params);
    
}
PFNGLTEXENVXVPROC glad_debug_glTexEnvxv = glad_debug_impl_glTexEnvxv;
PFNGLTEXENVXVOESPROC glad_glTexEnvxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEnvxvOES(GLenum target, GLenum pname, const GLfixed * params) {
    _pre_call_gles1_callback("glTexEnvxvOES", (GLADapiproc) glad_glTexEnvxvOES, 3, target, pname, params);
    glad_glTexEnvxvOES(target, pname, params);
    _post_call_gles1_callback(NULL, "glTexEnvxvOES", (GLADapiproc) glad_glTexEnvxvOES, 3, target, pname, params);
    
}
PFNGLTEXENVXVOESPROC glad_debug_glTexEnvxvOES = glad_debug_impl_glTexEnvxvOES;
PFNGLTEXGENFOESPROC glad_glTexGenfOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexGenfOES(GLenum coord, GLenum pname, GLfloat param) {
    _pre_call_gles1_callback("glTexGenfOES", (GLADapiproc) glad_glTexGenfOES, 3, coord, pname, param);
    glad_glTexGenfOES(coord, pname, param);
    _post_call_gles1_callback(NULL, "glTexGenfOES", (GLADapiproc) glad_glTexGenfOES, 3, coord, pname, param);
    
}
PFNGLTEXGENFOESPROC glad_debug_glTexGenfOES = glad_debug_impl_glTexGenfOES;
PFNGLTEXGENFVOESPROC glad_glTexGenfvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexGenfvOES(GLenum coord, GLenum pname, const GLfloat * params) {
    _pre_call_gles1_callback("glTexGenfvOES", (GLADapiproc) glad_glTexGenfvOES, 3, coord, pname, params);
    glad_glTexGenfvOES(coord, pname, params);
    _post_call_gles1_callback(NULL, "glTexGenfvOES", (GLADapiproc) glad_glTexGenfvOES, 3, coord, pname, params);
    
}
PFNGLTEXGENFVOESPROC glad_debug_glTexGenfvOES = glad_debug_impl_glTexGenfvOES;
PFNGLTEXGENIOESPROC glad_glTexGeniOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexGeniOES(GLenum coord, GLenum pname, GLint param) {
    _pre_call_gles1_callback("glTexGeniOES", (GLADapiproc) glad_glTexGeniOES, 3, coord, pname, param);
    glad_glTexGeniOES(coord, pname, param);
    _post_call_gles1_callback(NULL, "glTexGeniOES", (GLADapiproc) glad_glTexGeniOES, 3, coord, pname, param);
    
}
PFNGLTEXGENIOESPROC glad_debug_glTexGeniOES = glad_debug_impl_glTexGeniOES;
PFNGLTEXGENIVOESPROC glad_glTexGenivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexGenivOES(GLenum coord, GLenum pname, const GLint * params) {
    _pre_call_gles1_callback("glTexGenivOES", (GLADapiproc) glad_glTexGenivOES, 3, coord, pname, params);
    glad_glTexGenivOES(coord, pname, params);
    _post_call_gles1_callback(NULL, "glTexGenivOES", (GLADapiproc) glad_glTexGenivOES, 3, coord, pname, params);
    
}
PFNGLTEXGENIVOESPROC glad_debug_glTexGenivOES = glad_debug_impl_glTexGenivOES;
PFNGLTEXGENXOESPROC glad_glTexGenxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexGenxOES(GLenum coord, GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glTexGenxOES", (GLADapiproc) glad_glTexGenxOES, 3, coord, pname, param);
    glad_glTexGenxOES(coord, pname, param);
    _post_call_gles1_callback(NULL, "glTexGenxOES", (GLADapiproc) glad_glTexGenxOES, 3, coord, pname, param);
    
}
PFNGLTEXGENXOESPROC glad_debug_glTexGenxOES = glad_debug_impl_glTexGenxOES;
PFNGLTEXGENXVOESPROC glad_glTexGenxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexGenxvOES(GLenum coord, GLenum pname, const GLfixed * params) {
    _pre_call_gles1_callback("glTexGenxvOES", (GLADapiproc) glad_glTexGenxvOES, 3, coord, pname, params);
    glad_glTexGenxvOES(coord, pname, params);
    _post_call_gles1_callback(NULL, "glTexGenxvOES", (GLADapiproc) glad_glTexGenxvOES, 3, coord, pname, params);
    
}
PFNGLTEXGENXVOESPROC glad_debug_glTexGenxvOES = glad_debug_impl_glTexGenxvOES;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gles1_callback("glTexImage2D", (GLADapiproc) glad_glTexImage2D, 9, target, level, internalformat, width, height, border, format, type, pixels);
    glad_glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    _post_call_gles1_callback(NULL, "glTexImage2D", (GLADapiproc) glad_glTexImage2D, 9, target, level, internalformat, width, height, border, format, type, pixels);
    
}
PFNGLTEXIMAGE2DPROC glad_debug_glTexImage2D = glad_debug_impl_glTexImage2D;
PFNGLTEXPARAMETERFPROC glad_glTexParameterf = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    _pre_call_gles1_callback("glTexParameterf", (GLADapiproc) glad_glTexParameterf, 3, target, pname, param);
    glad_glTexParameterf(target, pname, param);
    _post_call_gles1_callback(NULL, "glTexParameterf", (GLADapiproc) glad_glTexParameterf, 3, target, pname, param);
    
}
PFNGLTEXPARAMETERFPROC glad_debug_glTexParameterf = glad_debug_impl_glTexParameterf;
PFNGLTEXPARAMETERFVPROC glad_glTexParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterfv(GLenum target, GLenum pname, const GLfloat * params) {
    _pre_call_gles1_callback("glTexParameterfv", (GLADapiproc) glad_glTexParameterfv, 3, target, pname, params);
    glad_glTexParameterfv(target, pname, params);
    _post_call_gles1_callback(NULL, "glTexParameterfv", (GLADapiproc) glad_glTexParameterfv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERFVPROC glad_debug_glTexParameterfv = glad_debug_impl_glTexParameterfv;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    _pre_call_gles1_callback("glTexParameteri", (GLADapiproc) glad_glTexParameteri, 3, target, pname, param);
    glad_glTexParameteri(target, pname, param);
    _post_call_gles1_callback(NULL, "glTexParameteri", (GLADapiproc) glad_glTexParameteri, 3, target, pname, param);
    
}
PFNGLTEXPARAMETERIPROC glad_debug_glTexParameteri = glad_debug_impl_glTexParameteri;
PFNGLTEXPARAMETERIVPROC glad_glTexParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameteriv(GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gles1_callback("glTexParameteriv", (GLADapiproc) glad_glTexParameteriv, 3, target, pname, params);
    glad_glTexParameteriv(target, pname, params);
    _post_call_gles1_callback(NULL, "glTexParameteriv", (GLADapiproc) glad_glTexParameteriv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIVPROC glad_debug_glTexParameteriv = glad_debug_impl_glTexParameteriv;
PFNGLTEXPARAMETERXPROC glad_glTexParameterx = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterx(GLenum target, GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glTexParameterx", (GLADapiproc) glad_glTexParameterx, 3, target, pname, param);
    glad_glTexParameterx(target, pname, param);
    _post_call_gles1_callback(NULL, "glTexParameterx", (GLADapiproc) glad_glTexParameterx, 3, target, pname, param);
    
}
PFNGLTEXPARAMETERXPROC glad_debug_glTexParameterx = glad_debug_impl_glTexParameterx;
PFNGLTEXPARAMETERXOESPROC glad_glTexParameterxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterxOES(GLenum target, GLenum pname, GLfixed param) {
    _pre_call_gles1_callback("glTexParameterxOES", (GLADapiproc) glad_glTexParameterxOES, 3, target, pname, param);
    glad_glTexParameterxOES(target, pname, param);
    _post_call_gles1_callback(NULL, "glTexParameterxOES", (GLADapiproc) glad_glTexParameterxOES, 3, target, pname, param);
    
}
PFNGLTEXPARAMETERXOESPROC glad_debug_glTexParameterxOES = glad_debug_impl_glTexParameterxOES;
PFNGLTEXPARAMETERXVPROC glad_glTexParameterxv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterxv(GLenum target, GLenum pname, const GLfixed * params) {
    _pre_call_gles1_callback("glTexParameterxv", (GLADapiproc) glad_glTexParameterxv, 3, target, pname, params);
    glad_glTexParameterxv(target, pname, params);
    _post_call_gles1_callback(NULL, "glTexParameterxv", (GLADapiproc) glad_glTexParameterxv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERXVPROC glad_debug_glTexParameterxv = glad_debug_impl_glTexParameterxv;
PFNGLTEXPARAMETERXVOESPROC glad_glTexParameterxvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterxvOES(GLenum target, GLenum pname, const GLfixed * params) {
    _pre_call_gles1_callback("glTexParameterxvOES", (GLADapiproc) glad_glTexParameterxvOES, 3, target, pname, params);
    glad_glTexParameterxvOES(target, pname, params);
    _post_call_gles1_callback(NULL, "glTexParameterxvOES", (GLADapiproc) glad_glTexParameterxvOES, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERXVOESPROC glad_debug_glTexParameterxvOES = glad_debug_impl_glTexParameterxvOES;
PFNGLTEXSTORAGE1DEXTPROC glad_glTexStorage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage1DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    _pre_call_gles1_callback("glTexStorage1DEXT", (GLADapiproc) glad_glTexStorage1DEXT, 4, target, levels, internalformat, width);
    glad_glTexStorage1DEXT(target, levels, internalformat, width);
    _post_call_gles1_callback(NULL, "glTexStorage1DEXT", (GLADapiproc) glad_glTexStorage1DEXT, 4, target, levels, internalformat, width);
    
}
PFNGLTEXSTORAGE1DEXTPROC glad_debug_glTexStorage1DEXT = glad_debug_impl_glTexStorage1DEXT;
PFNGLTEXSTORAGE2DEXTPROC glad_glTexStorage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage2DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles1_callback("glTexStorage2DEXT", (GLADapiproc) glad_glTexStorage2DEXT, 5, target, levels, internalformat, width, height);
    glad_glTexStorage2DEXT(target, levels, internalformat, width, height);
    _post_call_gles1_callback(NULL, "glTexStorage2DEXT", (GLADapiproc) glad_glTexStorage2DEXT, 5, target, levels, internalformat, width, height);
    
}
PFNGLTEXSTORAGE2DEXTPROC glad_debug_glTexStorage2DEXT = glad_debug_impl_glTexStorage2DEXT;
PFNGLTEXSTORAGE3DEXTPROC glad_glTexStorage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage3DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    _pre_call_gles1_callback("glTexStorage3DEXT", (GLADapiproc) glad_glTexStorage3DEXT, 6, target, levels, internalformat, width, height, depth);
    glad_glTexStorage3DEXT(target, levels, internalformat, width, height, depth);
    _post_call_gles1_callback(NULL, "glTexStorage3DEXT", (GLADapiproc) glad_glTexStorage3DEXT, 6, target, levels, internalformat, width, height, depth);
    
}
PFNGLTEXSTORAGE3DEXTPROC glad_debug_glTexStorage3DEXT = glad_debug_impl_glTexStorage3DEXT;
PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gles1_callback("glTexSubImage2D", (GLADapiproc) glad_glTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, type, pixels);
    glad_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    _post_call_gles1_callback(NULL, "glTexSubImage2D", (GLADapiproc) glad_glTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE2DPROC glad_debug_glTexSubImage2D = glad_debug_impl_glTexSubImage2D;
PFNGLTEXTURESTORAGE1DEXTPROC glad_glTextureStorage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    _pre_call_gles1_callback("glTextureStorage1DEXT", (GLADapiproc) glad_glTextureStorage1DEXT, 5, texture, target, levels, internalformat, width);
    glad_glTextureStorage1DEXT(texture, target, levels, internalformat, width);
    _post_call_gles1_callback(NULL, "glTextureStorage1DEXT", (GLADapiproc) glad_glTextureStorage1DEXT, 5, texture, target, levels, internalformat, width);
    
}
PFNGLTEXTURESTORAGE1DEXTPROC glad_debug_glTextureStorage1DEXT = glad_debug_impl_glTextureStorage1DEXT;
PFNGLTEXTURESTORAGE2DEXTPROC glad_glTextureStorage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles1_callback("glTextureStorage2DEXT", (GLADapiproc) glad_glTextureStorage2DEXT, 6, texture, target, levels, internalformat, width, height);
    glad_glTextureStorage2DEXT(texture, target, levels, internalformat, width, height);
    _post_call_gles1_callback(NULL, "glTextureStorage2DEXT", (GLADapiproc) glad_glTextureStorage2DEXT, 6, texture, target, levels, internalformat, width, height);
    
}
PFNGLTEXTURESTORAGE2DEXTPROC glad_debug_glTextureStorage2DEXT = glad_debug_impl_glTextureStorage2DEXT;
PFNGLTEXTURESTORAGE3DEXTPROC glad_glTextureStorage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    _pre_call_gles1_callback("glTextureStorage3DEXT", (GLADapiproc) glad_glTextureStorage3DEXT, 7, texture, target, levels, internalformat, width, height, depth);
    glad_glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);
    _post_call_gles1_callback(NULL, "glTextureStorage3DEXT", (GLADapiproc) glad_glTextureStorage3DEXT, 7, texture, target, levels, internalformat, width, height, depth);
    
}
PFNGLTEXTURESTORAGE3DEXTPROC glad_debug_glTextureStorage3DEXT = glad_debug_impl_glTextureStorage3DEXT;
PFNGLTRANSLATEFPROC glad_glTranslatef = NULL;
static void GLAD_API_PTR glad_debug_impl_glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gles1_callback("glTranslatef", (GLADapiproc) glad_glTranslatef, 3, x, y, z);
    glad_glTranslatef(x, y, z);
    _post_call_gles1_callback(NULL, "glTranslatef", (GLADapiproc) glad_glTranslatef, 3, x, y, z);
    
}
PFNGLTRANSLATEFPROC glad_debug_glTranslatef = glad_debug_impl_glTranslatef;
PFNGLTRANSLATEXPROC glad_glTranslatex = NULL;
static void GLAD_API_PTR glad_debug_impl_glTranslatex(GLfixed x, GLfixed y, GLfixed z) {
    _pre_call_gles1_callback("glTranslatex", (GLADapiproc) glad_glTranslatex, 3, x, y, z);
    glad_glTranslatex(x, y, z);
    _post_call_gles1_callback(NULL, "glTranslatex", (GLADapiproc) glad_glTranslatex, 3, x, y, z);
    
}
PFNGLTRANSLATEXPROC glad_debug_glTranslatex = glad_debug_impl_glTranslatex;
PFNGLTRANSLATEXOESPROC glad_glTranslatexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTranslatexOES(GLfixed x, GLfixed y, GLfixed z) {
    _pre_call_gles1_callback("glTranslatexOES", (GLADapiproc) glad_glTranslatexOES, 3, x, y, z);
    glad_glTranslatexOES(x, y, z);
    _post_call_gles1_callback(NULL, "glTranslatexOES", (GLADapiproc) glad_glTranslatexOES, 3, x, y, z);
    
}
PFNGLTRANSLATEXOESPROC glad_debug_glTranslatexOES = glad_debug_impl_glTranslatexOES;
PFNGLUNMAPBUFFEROESPROC glad_glUnmapBufferOES = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glUnmapBufferOES(GLenum target) {
    GLboolean ret;
    _pre_call_gles1_callback("glUnmapBufferOES", (GLADapiproc) glad_glUnmapBufferOES, 1, target);
    ret = glad_glUnmapBufferOES(target);
    _post_call_gles1_callback((void*) &ret, "glUnmapBufferOES", (GLADapiproc) glad_glUnmapBufferOES, 1, target);
    return ret;
}
PFNGLUNMAPBUFFEROESPROC glad_debug_glUnmapBufferOES = glad_debug_impl_glUnmapBufferOES;
PFNGLVERTEXPOINTERPROC glad_glVertexPointer = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexPointer(GLint size, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gles1_callback("glVertexPointer", (GLADapiproc) glad_glVertexPointer, 4, size, type, stride, pointer);
    glad_glVertexPointer(size, type, stride, pointer);
    _post_call_gles1_callback(NULL, "glVertexPointer", (GLADapiproc) glad_glVertexPointer, 4, size, type, stride, pointer);
    
}
PFNGLVERTEXPOINTERPROC glad_debug_glVertexPointer = glad_debug_impl_glVertexPointer;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles1_callback("glViewport", (GLADapiproc) glad_glViewport, 4, x, y, width, height);
    glad_glViewport(x, y, width, height);
    _post_call_gles1_callback(NULL, "glViewport", (GLADapiproc) glad_glViewport, 4, x, y, width, height);
    
}
PFNGLVIEWPORTPROC glad_debug_glViewport = glad_debug_impl_glViewport;
PFNGLWAITSYNCAPPLEPROC glad_glWaitSyncAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glWaitSyncAPPLE(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    _pre_call_gles1_callback("glWaitSyncAPPLE", (GLADapiproc) glad_glWaitSyncAPPLE, 3, sync, flags, timeout);
    glad_glWaitSyncAPPLE(sync, flags, timeout);
    _post_call_gles1_callback(NULL, "glWaitSyncAPPLE", (GLADapiproc) glad_glWaitSyncAPPLE, 3, sync, flags, timeout);
    
}
PFNGLWAITSYNCAPPLEPROC glad_debug_glWaitSyncAPPLE = glad_debug_impl_glWaitSyncAPPLE;
PFNGLWEIGHTPOINTEROESPROC glad_glWeightPointerOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glWeightPointerOES(GLint size, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gles1_callback("glWeightPointerOES", (GLADapiproc) glad_glWeightPointerOES, 4, size, type, stride, pointer);
    glad_glWeightPointerOES(size, type, stride, pointer);
    _post_call_gles1_callback(NULL, "glWeightPointerOES", (GLADapiproc) glad_glWeightPointerOES, 4, size, type, stride, pointer);
    
}
PFNGLWEIGHTPOINTEROESPROC glad_debug_glWeightPointerOES = glad_debug_impl_glWeightPointerOES;


static void glad_gl_load_GL_VERSION_ES_CM_1_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_VERSION_ES_CM_1_0) return;
    glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC) load(userptr, "glActiveTexture");
    glad_glAlphaFunc = (PFNGLALPHAFUNCPROC) load(userptr, "glAlphaFunc");
    glad_glAlphaFuncx = (PFNGLALPHAFUNCXPROC) load(userptr, "glAlphaFuncx");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC) load(userptr, "glBindBuffer");
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC) load(userptr, "glBindTexture");
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC) load(userptr, "glBlendFunc");
    glad_glBufferData = (PFNGLBUFFERDATAPROC) load(userptr, "glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC) load(userptr, "glBufferSubData");
    glad_glClear = (PFNGLCLEARPROC) load(userptr, "glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC) load(userptr, "glClearColor");
    glad_glClearColorx = (PFNGLCLEARCOLORXPROC) load(userptr, "glClearColorx");
    glad_glClearDepthf = (PFNGLCLEARDEPTHFPROC) load(userptr, "glClearDepthf");
    glad_glClearDepthx = (PFNGLCLEARDEPTHXPROC) load(userptr, "glClearDepthx");
    glad_glClearStencil = (PFNGLCLEARSTENCILPROC) load(userptr, "glClearStencil");
    glad_glClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC) load(userptr, "glClientActiveTexture");
    glad_glClipPlanef = (PFNGLCLIPPLANEFPROC) load(userptr, "glClipPlanef");
    glad_glClipPlanex = (PFNGLCLIPPLANEXPROC) load(userptr, "glClipPlanex");
    glad_glColor4f = (PFNGLCOLOR4FPROC) load(userptr, "glColor4f");
    glad_glColor4ub = (PFNGLCOLOR4UBPROC) load(userptr, "glColor4ub");
    glad_glColor4x = (PFNGLCOLOR4XPROC) load(userptr, "glColor4x");
    glad_glColorMask = (PFNGLCOLORMASKPROC) load(userptr, "glColorMask");
    glad_glColorPointer = (PFNGLCOLORPOINTERPROC) load(userptr, "glColorPointer");
    glad_glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC) load(userptr, "glCompressedTexImage2D");
    glad_glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC) load(userptr, "glCompressedTexSubImage2D");
    glad_glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC) load(userptr, "glCopyTexImage2D");
    glad_glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC) load(userptr, "glCopyTexSubImage2D");
    glad_glCullFace = (PFNGLCULLFACEPROC) load(userptr, "glCullFace");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC) load(userptr, "glDeleteBuffers");
    glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC) load(userptr, "glDeleteTextures");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC) load(userptr, "glDepthFunc");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC) load(userptr, "glDepthMask");
    glad_glDepthRangef = (PFNGLDEPTHRANGEFPROC) load(userptr, "glDepthRangef");
    glad_glDepthRangex = (PFNGLDEPTHRANGEXPROC) load(userptr, "glDepthRangex");
    glad_glDisable = (PFNGLDISABLEPROC) load(userptr, "glDisable");
    glad_glDisableClientState = (PFNGLDISABLECLIENTSTATEPROC) load(userptr, "glDisableClientState");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC) load(userptr, "glDrawArrays");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC) load(userptr, "glDrawElements");
    glad_glEnable = (PFNGLENABLEPROC) load(userptr, "glEnable");
    glad_glEnableClientState = (PFNGLENABLECLIENTSTATEPROC) load(userptr, "glEnableClientState");
    glad_glFinish = (PFNGLFINISHPROC) load(userptr, "glFinish");
    glad_glFlush = (PFNGLFLUSHPROC) load(userptr, "glFlush");
    glad_glFogf = (PFNGLFOGFPROC) load(userptr, "glFogf");
    glad_glFogfv = (PFNGLFOGFVPROC) load(userptr, "glFogfv");
    glad_glFogx = (PFNGLFOGXPROC) load(userptr, "glFogx");
    glad_glFogxv = (PFNGLFOGXVPROC) load(userptr, "glFogxv");
    glad_glFrontFace = (PFNGLFRONTFACEPROC) load(userptr, "glFrontFace");
    glad_glFrustumf = (PFNGLFRUSTUMFPROC) load(userptr, "glFrustumf");
    glad_glFrustumx = (PFNGLFRUSTUMXPROC) load(userptr, "glFrustumx");
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC) load(userptr, "glGenBuffers");
    glad_glGenTextures = (PFNGLGENTEXTURESPROC) load(userptr, "glGenTextures");
    glad_glGetBooleanv = (PFNGLGETBOOLEANVPROC) load(userptr, "glGetBooleanv");
    glad_glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC) load(userptr, "glGetBufferParameteriv");
    glad_glGetClipPlanef = (PFNGLGETCLIPPLANEFPROC) load(userptr, "glGetClipPlanef");
    glad_glGetClipPlanex = (PFNGLGETCLIPPLANEXPROC) load(userptr, "glGetClipPlanex");
    glad_glGetError = (PFNGLGETERRORPROC) load(userptr, "glGetError");
    glad_glGetFixedv = (PFNGLGETFIXEDVPROC) load(userptr, "glGetFixedv");
    glad_glGetFloatv = (PFNGLGETFLOATVPROC) load(userptr, "glGetFloatv");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC) load(userptr, "glGetIntegerv");
    glad_glGetLightfv = (PFNGLGETLIGHTFVPROC) load(userptr, "glGetLightfv");
    glad_glGetLightxv = (PFNGLGETLIGHTXVPROC) load(userptr, "glGetLightxv");
    glad_glGetMaterialfv = (PFNGLGETMATERIALFVPROC) load(userptr, "glGetMaterialfv");
    glad_glGetMaterialxv = (PFNGLGETMATERIALXVPROC) load(userptr, "glGetMaterialxv");
    glad_glGetPointerv = (PFNGLGETPOINTERVPROC) load(userptr, "glGetPointerv");
    glad_glGetString = (PFNGLGETSTRINGPROC) load(userptr, "glGetString");
    glad_glGetTexEnvfv = (PFNGLGETTEXENVFVPROC) load(userptr, "glGetTexEnvfv");
    glad_glGetTexEnviv = (PFNGLGETTEXENVIVPROC) load(userptr, "glGetTexEnviv");
    glad_glGetTexEnvxv = (PFNGLGETTEXENVXVPROC) load(userptr, "glGetTexEnvxv");
    glad_glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC) load(userptr, "glGetTexParameterfv");
    glad_glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC) load(userptr, "glGetTexParameteriv");
    glad_glGetTexParameterxv = (PFNGLGETTEXPARAMETERXVPROC) load(userptr, "glGetTexParameterxv");
    glad_glHint = (PFNGLHINTPROC) load(userptr, "glHint");
    glad_glIsBuffer = (PFNGLISBUFFERPROC) load(userptr, "glIsBuffer");
    glad_glIsEnabled = (PFNGLISENABLEDPROC) load(userptr, "glIsEnabled");
    glad_glIsTexture = (PFNGLISTEXTUREPROC) load(userptr, "glIsTexture");
    glad_glLightModelf = (PFNGLLIGHTMODELFPROC) load(userptr, "glLightModelf");
    glad_glLightModelfv = (PFNGLLIGHTMODELFVPROC) load(userptr, "glLightModelfv");
    glad_glLightModelx = (PFNGLLIGHTMODELXPROC) load(userptr, "glLightModelx");
    glad_glLightModelxv = (PFNGLLIGHTMODELXVPROC) load(userptr, "glLightModelxv");
    glad_glLightf = (PFNGLLIGHTFPROC) load(userptr, "glLightf");
    glad_glLightfv = (PFNGLLIGHTFVPROC) load(userptr, "glLightfv");
    glad_glLightx = (PFNGLLIGHTXPROC) load(userptr, "glLightx");
    glad_glLightxv = (PFNGLLIGHTXVPROC) load(userptr, "glLightxv");
    glad_glLineWidth = (PFNGLLINEWIDTHPROC) load(userptr, "glLineWidth");
    glad_glLineWidthx = (PFNGLLINEWIDTHXPROC) load(userptr, "glLineWidthx");
    glad_glLoadIdentity = (PFNGLLOADIDENTITYPROC) load(userptr, "glLoadIdentity");
    glad_glLoadMatrixf = (PFNGLLOADMATRIXFPROC) load(userptr, "glLoadMatrixf");
    glad_glLoadMatrixx = (PFNGLLOADMATRIXXPROC) load(userptr, "glLoadMatrixx");
    glad_glLogicOp = (PFNGLLOGICOPPROC) load(userptr, "glLogicOp");
    glad_glMaterialf = (PFNGLMATERIALFPROC) load(userptr, "glMaterialf");
    glad_glMaterialfv = (PFNGLMATERIALFVPROC) load(userptr, "glMaterialfv");
    glad_glMaterialx = (PFNGLMATERIALXPROC) load(userptr, "glMaterialx");
    glad_glMaterialxv = (PFNGLMATERIALXVPROC) load(userptr, "glMaterialxv");
    glad_glMatrixMode = (PFNGLMATRIXMODEPROC) load(userptr, "glMatrixMode");
    glad_glMultMatrixf = (PFNGLMULTMATRIXFPROC) load(userptr, "glMultMatrixf");
    glad_glMultMatrixx = (PFNGLMULTMATRIXXPROC) load(userptr, "glMultMatrixx");
    glad_glMultiTexCoord4f = (PFNGLMULTITEXCOORD4FPROC) load(userptr, "glMultiTexCoord4f");
    glad_glMultiTexCoord4x = (PFNGLMULTITEXCOORD4XPROC) load(userptr, "glMultiTexCoord4x");
    glad_glNormal3f = (PFNGLNORMAL3FPROC) load(userptr, "glNormal3f");
    glad_glNormal3x = (PFNGLNORMAL3XPROC) load(userptr, "glNormal3x");
    glad_glNormalPointer = (PFNGLNORMALPOINTERPROC) load(userptr, "glNormalPointer");
    glad_glOrthof = (PFNGLORTHOFPROC) load(userptr, "glOrthof");
    glad_glOrthox = (PFNGLORTHOXPROC) load(userptr, "glOrthox");
    glad_glPixelStorei = (PFNGLPIXELSTOREIPROC) load(userptr, "glPixelStorei");
    glad_glPointParameterf = (PFNGLPOINTPARAMETERFPROC) load(userptr, "glPointParameterf");
    glad_glPointParameterfv = (PFNGLPOINTPARAMETERFVPROC) load(userptr, "glPointParameterfv");
    glad_glPointParameterx = (PFNGLPOINTPARAMETERXPROC) load(userptr, "glPointParameterx");
    glad_glPointParameterxv = (PFNGLPOINTPARAMETERXVPROC) load(userptr, "glPointParameterxv");
    glad_glPointSize = (PFNGLPOINTSIZEPROC) load(userptr, "glPointSize");
    glad_glPointSizex = (PFNGLPOINTSIZEXPROC) load(userptr, "glPointSizex");
    glad_glPolygonOffset = (PFNGLPOLYGONOFFSETPROC) load(userptr, "glPolygonOffset");
    glad_glPolygonOffsetx = (PFNGLPOLYGONOFFSETXPROC) load(userptr, "glPolygonOffsetx");
    glad_glPopMatrix = (PFNGLPOPMATRIXPROC) load(userptr, "glPopMatrix");
    glad_glPushMatrix = (PFNGLPUSHMATRIXPROC) load(userptr, "glPushMatrix");
    glad_glReadPixels = (PFNGLREADPIXELSPROC) load(userptr, "glReadPixels");
    glad_glRotatef = (PFNGLROTATEFPROC) load(userptr, "glRotatef");
    glad_glRotatex = (PFNGLROTATEXPROC) load(userptr, "glRotatex");
    glad_glSampleCoverage = (PFNGLSAMPLECOVERAGEPROC) load(userptr, "glSampleCoverage");
    glad_glSampleCoveragex = (PFNGLSAMPLECOVERAGEXPROC) load(userptr, "glSampleCoveragex");
    glad_glScalef = (PFNGLSCALEFPROC) load(userptr, "glScalef");
    glad_glScalex = (PFNGLSCALEXPROC) load(userptr, "glScalex");
    glad_glScissor = (PFNGLSCISSORPROC) load(userptr, "glScissor");
    glad_glShadeModel = (PFNGLSHADEMODELPROC) load(userptr, "glShadeModel");
    glad_glStencilFunc = (PFNGLSTENCILFUNCPROC) load(userptr, "glStencilFunc");
    glad_glStencilMask = (PFNGLSTENCILMASKPROC) load(userptr, "glStencilMask");
    glad_glStencilOp = (PFNGLSTENCILOPPROC) load(userptr, "glStencilOp");
    glad_glTexCoordPointer = (PFNGLTEXCOORDPOINTERPROC) load(userptr, "glTexCoordPointer");
    glad_glTexEnvf = (PFNGLTEXENVFPROC) load(userptr, "glTexEnvf");
    glad_glTexEnvfv = (PFNGLTEXENVFVPROC) load(userptr, "glTexEnvfv");
    glad_glTexEnvi = (PFNGLTEXENVIPROC) load(userptr, "glTexEnvi");
    glad_glTexEnviv = (PFNGLTEXENVIVPROC) load(userptr, "glTexEnviv");
    glad_glTexEnvx = (PFNGLTEXENVXPROC) load(userptr, "glTexEnvx");
    glad_glTexEnvxv = (PFNGLTEXENVXVPROC) load(userptr, "glTexEnvxv");
    glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC) load(userptr, "glTexImage2D");
    glad_glTexParameterf = (PFNGLTEXPARAMETERFPROC) load(userptr, "glTexParameterf");
    glad_glTexParameterfv = (PFNGLTEXPARAMETERFVPROC) load(userptr, "glTexParameterfv");
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC) load(userptr, "glTexParameteri");
    glad_glTexParameteriv = (PFNGLTEXPARAMETERIVPROC) load(userptr, "glTexParameteriv");
    glad_glTexParameterx = (PFNGLTEXPARAMETERXPROC) load(userptr, "glTexParameterx");
    glad_glTexParameterxv = (PFNGLTEXPARAMETERXVPROC) load(userptr, "glTexParameterxv");
    glad_glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC) load(userptr, "glTexSubImage2D");
    glad_glTranslatef = (PFNGLTRANSLATEFPROC) load(userptr, "glTranslatef");
    glad_glTranslatex = (PFNGLTRANSLATEXPROC) load(userptr, "glTranslatex");
    glad_glVertexPointer = (PFNGLVERTEXPOINTERPROC) load(userptr, "glVertexPointer");
    glad_glViewport = (PFNGLVIEWPORTPROC) load(userptr, "glViewport");
}
static void glad_gl_load_GL_APPLE_copy_texture_levels( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_APPLE_copy_texture_levels) return;
    glad_glCopyTextureLevelsAPPLE = (PFNGLCOPYTEXTURELEVELSAPPLEPROC) load(userptr, "glCopyTextureLevelsAPPLE");
}
static void glad_gl_load_GL_APPLE_framebuffer_multisample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_APPLE_framebuffer_multisample) return;
    glad_glRenderbufferStorageMultisampleAPPLE = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC) load(userptr, "glRenderbufferStorageMultisampleAPPLE");
    glad_glResolveMultisampleFramebufferAPPLE = (PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC) load(userptr, "glResolveMultisampleFramebufferAPPLE");
}
static void glad_gl_load_GL_APPLE_sync( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_APPLE_sync) return;
    glad_glClientWaitSyncAPPLE = (PFNGLCLIENTWAITSYNCAPPLEPROC) load(userptr, "glClientWaitSyncAPPLE");
    glad_glDeleteSyncAPPLE = (PFNGLDELETESYNCAPPLEPROC) load(userptr, "glDeleteSyncAPPLE");
    glad_glFenceSyncAPPLE = (PFNGLFENCESYNCAPPLEPROC) load(userptr, "glFenceSyncAPPLE");
    glad_glGetInteger64vAPPLE = (PFNGLGETINTEGER64VAPPLEPROC) load(userptr, "glGetInteger64vAPPLE");
    glad_glGetSyncivAPPLE = (PFNGLGETSYNCIVAPPLEPROC) load(userptr, "glGetSyncivAPPLE");
    glad_glIsSyncAPPLE = (PFNGLISSYNCAPPLEPROC) load(userptr, "glIsSyncAPPLE");
    glad_glWaitSyncAPPLE = (PFNGLWAITSYNCAPPLEPROC) load(userptr, "glWaitSyncAPPLE");
}
static void glad_gl_load_GL_EXT_debug_marker( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_debug_marker) return;
    glad_glInsertEventMarkerEXT = (PFNGLINSERTEVENTMARKEREXTPROC) load(userptr, "glInsertEventMarkerEXT");
    glad_glPopGroupMarkerEXT = (PFNGLPOPGROUPMARKEREXTPROC) load(userptr, "glPopGroupMarkerEXT");
    glad_glPushGroupMarkerEXT = (PFNGLPUSHGROUPMARKEREXTPROC) load(userptr, "glPushGroupMarkerEXT");
}
static void glad_gl_load_GL_EXT_discard_framebuffer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_discard_framebuffer) return;
    glad_glDiscardFramebufferEXT = (PFNGLDISCARDFRAMEBUFFEREXTPROC) load(userptr, "glDiscardFramebufferEXT");
}
static void glad_gl_load_GL_EXT_map_buffer_range( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_map_buffer_range) return;
    glad_glFlushMappedBufferRangeEXT = (PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC) load(userptr, "glFlushMappedBufferRangeEXT");
    glad_glMapBufferRangeEXT = (PFNGLMAPBUFFERRANGEEXTPROC) load(userptr, "glMapBufferRangeEXT");
}
static void glad_gl_load_GL_EXT_multi_draw_arrays( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_multi_draw_arrays) return;
    glad_glMultiDrawArraysEXT = (PFNGLMULTIDRAWARRAYSEXTPROC) load(userptr, "glMultiDrawArraysEXT");
    glad_glMultiDrawElementsEXT = (PFNGLMULTIDRAWELEMENTSEXTPROC) load(userptr, "glMultiDrawElementsEXT");
}
static void glad_gl_load_GL_EXT_multisampled_render_to_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_multisampled_render_to_texture) return;
    glad_glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) load(userptr, "glFramebufferTexture2DMultisampleEXT");
    glad_glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) load(userptr, "glRenderbufferStorageMultisampleEXT");
}
static void glad_gl_load_GL_EXT_robustness( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_robustness) return;
    glad_glGetGraphicsResetStatusEXT = (PFNGLGETGRAPHICSRESETSTATUSEXTPROC) load(userptr, "glGetGraphicsResetStatusEXT");
    glad_glGetnUniformfvEXT = (PFNGLGETNUNIFORMFVEXTPROC) load(userptr, "glGetnUniformfvEXT");
    glad_glGetnUniformivEXT = (PFNGLGETNUNIFORMIVEXTPROC) load(userptr, "glGetnUniformivEXT");
    glad_glReadnPixelsEXT = (PFNGLREADNPIXELSEXTPROC) load(userptr, "glReadnPixelsEXT");
}
static void glad_gl_load_GL_EXT_texture_storage( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture_storage) return;
    glad_glTexStorage1DEXT = (PFNGLTEXSTORAGE1DEXTPROC) load(userptr, "glTexStorage1DEXT");
    glad_glTexStorage2DEXT = (PFNGLTEXSTORAGE2DEXTPROC) load(userptr, "glTexStorage2DEXT");
    glad_glTexStorage3DEXT = (PFNGLTEXSTORAGE3DEXTPROC) load(userptr, "glTexStorage3DEXT");
    glad_glTextureStorage1DEXT = (PFNGLTEXTURESTORAGE1DEXTPROC) load(userptr, "glTextureStorage1DEXT");
    glad_glTextureStorage2DEXT = (PFNGLTEXTURESTORAGE2DEXTPROC) load(userptr, "glTextureStorage2DEXT");
    glad_glTextureStorage3DEXT = (PFNGLTEXTURESTORAGE3DEXTPROC) load(userptr, "glTextureStorage3DEXT");
}
static void glad_gl_load_GL_IMG_multisampled_render_to_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_IMG_multisampled_render_to_texture) return;
    glad_glFramebufferTexture2DMultisampleIMG = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC) load(userptr, "glFramebufferTexture2DMultisampleIMG");
    glad_glRenderbufferStorageMultisampleIMG = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC) load(userptr, "glRenderbufferStorageMultisampleIMG");
}
static void glad_gl_load_GL_IMG_user_clip_plane( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_IMG_user_clip_plane) return;
    glad_glClipPlanefIMG = (PFNGLCLIPPLANEFIMGPROC) load(userptr, "glClipPlanefIMG");
    glad_glClipPlanexIMG = (PFNGLCLIPPLANEXIMGPROC) load(userptr, "glClipPlanexIMG");
}
static void glad_gl_load_GL_NV_fence( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_fence) return;
    glad_glDeleteFencesNV = (PFNGLDELETEFENCESNVPROC) load(userptr, "glDeleteFencesNV");
    glad_glFinishFenceNV = (PFNGLFINISHFENCENVPROC) load(userptr, "glFinishFenceNV");
    glad_glGenFencesNV = (PFNGLGENFENCESNVPROC) load(userptr, "glGenFencesNV");
    glad_glGetFenceivNV = (PFNGLGETFENCEIVNVPROC) load(userptr, "glGetFenceivNV");
    glad_glIsFenceNV = (PFNGLISFENCENVPROC) load(userptr, "glIsFenceNV");
    glad_glSetFenceNV = (PFNGLSETFENCENVPROC) load(userptr, "glSetFenceNV");
    glad_glTestFenceNV = (PFNGLTESTFENCENVPROC) load(userptr, "glTestFenceNV");
}
static void glad_gl_load_GL_OES_EGL_image( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_EGL_image) return;
    glad_glEGLImageTargetRenderbufferStorageOES = (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) load(userptr, "glEGLImageTargetRenderbufferStorageOES");
    glad_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) load(userptr, "glEGLImageTargetTexture2DOES");
}
static void glad_gl_load_GL_OES_EGL_image_external( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_EGL_image_external) return;
    glad_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) load(userptr, "glEGLImageTargetTexture2DOES");
}
static void glad_gl_load_GL_OES_blend_equation_separate( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_blend_equation_separate) return;
    glad_glBlendEquationSeparateOES = (PFNGLBLENDEQUATIONSEPARATEOESPROC) load(userptr, "glBlendEquationSeparateOES");
}
static void glad_gl_load_GL_OES_blend_func_separate( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_blend_func_separate) return;
    glad_glBlendFuncSeparateOES = (PFNGLBLENDFUNCSEPARATEOESPROC) load(userptr, "glBlendFuncSeparateOES");
}
static void glad_gl_load_GL_OES_blend_subtract( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_blend_subtract) return;
    glad_glBlendEquationOES = (PFNGLBLENDEQUATIONOESPROC) load(userptr, "glBlendEquationOES");
}
static void glad_gl_load_GL_OES_draw_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_draw_texture) return;
    glad_glDrawTexfOES = (PFNGLDRAWTEXFOESPROC) load(userptr, "glDrawTexfOES");
    glad_glDrawTexfvOES = (PFNGLDRAWTEXFVOESPROC) load(userptr, "glDrawTexfvOES");
    glad_glDrawTexiOES = (PFNGLDRAWTEXIOESPROC) load(userptr, "glDrawTexiOES");
    glad_glDrawTexivOES = (PFNGLDRAWTEXIVOESPROC) load(userptr, "glDrawTexivOES");
    glad_glDrawTexsOES = (PFNGLDRAWTEXSOESPROC) load(userptr, "glDrawTexsOES");
    glad_glDrawTexsvOES = (PFNGLDRAWTEXSVOESPROC) load(userptr, "glDrawTexsvOES");
    glad_glDrawTexxOES = (PFNGLDRAWTEXXOESPROC) load(userptr, "glDrawTexxOES");
    glad_glDrawTexxvOES = (PFNGLDRAWTEXXVOESPROC) load(userptr, "glDrawTexxvOES");
}
static void glad_gl_load_GL_OES_fixed_point( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_fixed_point) return;
    glad_glAlphaFuncxOES = (PFNGLALPHAFUNCXOESPROC) load(userptr, "glAlphaFuncxOES");
    glad_glClearColorxOES = (PFNGLCLEARCOLORXOESPROC) load(userptr, "glClearColorxOES");
    glad_glClearDepthxOES = (PFNGLCLEARDEPTHXOESPROC) load(userptr, "glClearDepthxOES");
    glad_glClipPlanexOES = (PFNGLCLIPPLANEXOESPROC) load(userptr, "glClipPlanexOES");
    glad_glColor4xOES = (PFNGLCOLOR4XOESPROC) load(userptr, "glColor4xOES");
    glad_glDepthRangexOES = (PFNGLDEPTHRANGEXOESPROC) load(userptr, "glDepthRangexOES");
    glad_glFogxOES = (PFNGLFOGXOESPROC) load(userptr, "glFogxOES");
    glad_glFogxvOES = (PFNGLFOGXVOESPROC) load(userptr, "glFogxvOES");
    glad_glFrustumxOES = (PFNGLFRUSTUMXOESPROC) load(userptr, "glFrustumxOES");
    glad_glGetClipPlanexOES = (PFNGLGETCLIPPLANEXOESPROC) load(userptr, "glGetClipPlanexOES");
    glad_glGetFixedvOES = (PFNGLGETFIXEDVOESPROC) load(userptr, "glGetFixedvOES");
    glad_glGetLightxvOES = (PFNGLGETLIGHTXVOESPROC) load(userptr, "glGetLightxvOES");
    glad_glGetMaterialxvOES = (PFNGLGETMATERIALXVOESPROC) load(userptr, "glGetMaterialxvOES");
    glad_glGetTexEnvxvOES = (PFNGLGETTEXENVXVOESPROC) load(userptr, "glGetTexEnvxvOES");
    glad_glGetTexParameterxvOES = (PFNGLGETTEXPARAMETERXVOESPROC) load(userptr, "glGetTexParameterxvOES");
    glad_glLightModelxOES = (PFNGLLIGHTMODELXOESPROC) load(userptr, "glLightModelxOES");
    glad_glLightModelxvOES = (PFNGLLIGHTMODELXVOESPROC) load(userptr, "glLightModelxvOES");
    glad_glLightxOES = (PFNGLLIGHTXOESPROC) load(userptr, "glLightxOES");
    glad_glLightxvOES = (PFNGLLIGHTXVOESPROC) load(userptr, "glLightxvOES");
    glad_glLineWidthxOES = (PFNGLLINEWIDTHXOESPROC) load(userptr, "glLineWidthxOES");
    glad_glLoadMatrixxOES = (PFNGLLOADMATRIXXOESPROC) load(userptr, "glLoadMatrixxOES");
    glad_glMaterialxOES = (PFNGLMATERIALXOESPROC) load(userptr, "glMaterialxOES");
    glad_glMaterialxvOES = (PFNGLMATERIALXVOESPROC) load(userptr, "glMaterialxvOES");
    glad_glMultMatrixxOES = (PFNGLMULTMATRIXXOESPROC) load(userptr, "glMultMatrixxOES");
    glad_glMultiTexCoord4xOES = (PFNGLMULTITEXCOORD4XOESPROC) load(userptr, "glMultiTexCoord4xOES");
    glad_glNormal3xOES = (PFNGLNORMAL3XOESPROC) load(userptr, "glNormal3xOES");
    glad_glOrthoxOES = (PFNGLORTHOXOESPROC) load(userptr, "glOrthoxOES");
    glad_glPointParameterxOES = (PFNGLPOINTPARAMETERXOESPROC) load(userptr, "glPointParameterxOES");
    glad_glPointParameterxvOES = (PFNGLPOINTPARAMETERXVOESPROC) load(userptr, "glPointParameterxvOES");
    glad_glPointSizexOES = (PFNGLPOINTSIZEXOESPROC) load(userptr, "glPointSizexOES");
    glad_glPolygonOffsetxOES = (PFNGLPOLYGONOFFSETXOESPROC) load(userptr, "glPolygonOffsetxOES");
    glad_glRotatexOES = (PFNGLROTATEXOESPROC) load(userptr, "glRotatexOES");
    glad_glSampleCoveragexOES = (PFNGLSAMPLECOVERAGEXOESPROC) load(userptr, "glSampleCoveragexOES");
    glad_glScalexOES = (PFNGLSCALEXOESPROC) load(userptr, "glScalexOES");
    glad_glTexEnvxOES = (PFNGLTEXENVXOESPROC) load(userptr, "glTexEnvxOES");
    glad_glTexEnvxvOES = (PFNGLTEXENVXVOESPROC) load(userptr, "glTexEnvxvOES");
    glad_glTexParameterxOES = (PFNGLTEXPARAMETERXOESPROC) load(userptr, "glTexParameterxOES");
    glad_glTexParameterxvOES = (PFNGLTEXPARAMETERXVOESPROC) load(userptr, "glTexParameterxvOES");
    glad_glTranslatexOES = (PFNGLTRANSLATEXOESPROC) load(userptr, "glTranslatexOES");
}
static void glad_gl_load_GL_OES_framebuffer_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_framebuffer_object) return;
    glad_glBindFramebufferOES = (PFNGLBINDFRAMEBUFFEROESPROC) load(userptr, "glBindFramebufferOES");
    glad_glBindRenderbufferOES = (PFNGLBINDRENDERBUFFEROESPROC) load(userptr, "glBindRenderbufferOES");
    glad_glCheckFramebufferStatusOES = (PFNGLCHECKFRAMEBUFFERSTATUSOESPROC) load(userptr, "glCheckFramebufferStatusOES");
    glad_glDeleteFramebuffersOES = (PFNGLDELETEFRAMEBUFFERSOESPROC) load(userptr, "glDeleteFramebuffersOES");
    glad_glDeleteRenderbuffersOES = (PFNGLDELETERENDERBUFFERSOESPROC) load(userptr, "glDeleteRenderbuffersOES");
    glad_glFramebufferRenderbufferOES = (PFNGLFRAMEBUFFERRENDERBUFFEROESPROC) load(userptr, "glFramebufferRenderbufferOES");
    glad_glFramebufferTexture2DOES = (PFNGLFRAMEBUFFERTEXTURE2DOESPROC) load(userptr, "glFramebufferTexture2DOES");
    glad_glGenFramebuffersOES = (PFNGLGENFRAMEBUFFERSOESPROC) load(userptr, "glGenFramebuffersOES");
    glad_glGenRenderbuffersOES = (PFNGLGENRENDERBUFFERSOESPROC) load(userptr, "glGenRenderbuffersOES");
    glad_glGenerateMipmapOES = (PFNGLGENERATEMIPMAPOESPROC) load(userptr, "glGenerateMipmapOES");
    glad_glGetFramebufferAttachmentParameterivOES = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVOESPROC) load(userptr, "glGetFramebufferAttachmentParameterivOES");
    glad_glGetRenderbufferParameterivOES = (PFNGLGETRENDERBUFFERPARAMETERIVOESPROC) load(userptr, "glGetRenderbufferParameterivOES");
    glad_glIsFramebufferOES = (PFNGLISFRAMEBUFFEROESPROC) load(userptr, "glIsFramebufferOES");
    glad_glIsRenderbufferOES = (PFNGLISRENDERBUFFEROESPROC) load(userptr, "glIsRenderbufferOES");
    glad_glRenderbufferStorageOES = (PFNGLRENDERBUFFERSTORAGEOESPROC) load(userptr, "glRenderbufferStorageOES");
}
static void glad_gl_load_GL_OES_mapbuffer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_mapbuffer) return;
    glad_glGetBufferPointervOES = (PFNGLGETBUFFERPOINTERVOESPROC) load(userptr, "glGetBufferPointervOES");
    glad_glMapBufferOES = (PFNGLMAPBUFFEROESPROC) load(userptr, "glMapBufferOES");
    glad_glUnmapBufferOES = (PFNGLUNMAPBUFFEROESPROC) load(userptr, "glUnmapBufferOES");
}
static void glad_gl_load_GL_OES_matrix_palette( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_matrix_palette) return;
    glad_glCurrentPaletteMatrixOES = (PFNGLCURRENTPALETTEMATRIXOESPROC) load(userptr, "glCurrentPaletteMatrixOES");
    glad_glLoadPaletteFromModelViewMatrixOES = (PFNGLLOADPALETTEFROMMODELVIEWMATRIXOESPROC) load(userptr, "glLoadPaletteFromModelViewMatrixOES");
    glad_glMatrixIndexPointerOES = (PFNGLMATRIXINDEXPOINTEROESPROC) load(userptr, "glMatrixIndexPointerOES");
    glad_glWeightPointerOES = (PFNGLWEIGHTPOINTEROESPROC) load(userptr, "glWeightPointerOES");
}
static void glad_gl_load_GL_OES_point_size_array( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_point_size_array) return;
    glad_glPointSizePointerOES = (PFNGLPOINTSIZEPOINTEROESPROC) load(userptr, "glPointSizePointerOES");
}
static void glad_gl_load_GL_OES_query_matrix( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_query_matrix) return;
    glad_glQueryMatrixxOES = (PFNGLQUERYMATRIXXOESPROC) load(userptr, "glQueryMatrixxOES");
}
static void glad_gl_load_GL_OES_single_precision( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_single_precision) return;
    glad_glClearDepthfOES = (PFNGLCLEARDEPTHFOESPROC) load(userptr, "glClearDepthfOES");
    glad_glClipPlanefOES = (PFNGLCLIPPLANEFOESPROC) load(userptr, "glClipPlanefOES");
    glad_glDepthRangefOES = (PFNGLDEPTHRANGEFOESPROC) load(userptr, "glDepthRangefOES");
    glad_glFrustumfOES = (PFNGLFRUSTUMFOESPROC) load(userptr, "glFrustumfOES");
    glad_glGetClipPlanefOES = (PFNGLGETCLIPPLANEFOESPROC) load(userptr, "glGetClipPlanefOES");
    glad_glOrthofOES = (PFNGLORTHOFOESPROC) load(userptr, "glOrthofOES");
}
static void glad_gl_load_GL_OES_texture_cube_map( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_texture_cube_map) return;
    glad_glGetTexGenfvOES = (PFNGLGETTEXGENFVOESPROC) load(userptr, "glGetTexGenfvOES");
    glad_glGetTexGenivOES = (PFNGLGETTEXGENIVOESPROC) load(userptr, "glGetTexGenivOES");
    glad_glGetTexGenxvOES = (PFNGLGETTEXGENXVOESPROC) load(userptr, "glGetTexGenxvOES");
    glad_glTexGenfOES = (PFNGLTEXGENFOESPROC) load(userptr, "glTexGenfOES");
    glad_glTexGenfvOES = (PFNGLTEXGENFVOESPROC) load(userptr, "glTexGenfvOES");
    glad_glTexGeniOES = (PFNGLTEXGENIOESPROC) load(userptr, "glTexGeniOES");
    glad_glTexGenivOES = (PFNGLTEXGENIVOESPROC) load(userptr, "glTexGenivOES");
    glad_glTexGenxOES = (PFNGLTEXGENXOESPROC) load(userptr, "glTexGenxOES");
    glad_glTexGenxvOES = (PFNGLTEXGENXVOESPROC) load(userptr, "glTexGenxvOES");
}
static void glad_gl_load_GL_OES_vertex_array_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_vertex_array_object) return;
    glad_glBindVertexArrayOES = (PFNGLBINDVERTEXARRAYOESPROC) load(userptr, "glBindVertexArrayOES");
    glad_glDeleteVertexArraysOES = (PFNGLDELETEVERTEXARRAYSOESPROC) load(userptr, "glDeleteVertexArraysOES");
    glad_glGenVertexArraysOES = (PFNGLGENVERTEXARRAYSOESPROC) load(userptr, "glGenVertexArraysOES");
    glad_glIsVertexArrayOES = (PFNGLISVERTEXARRAYOESPROC) load(userptr, "glIsVertexArrayOES");
}
static void glad_gl_load_GL_QCOM_driver_control( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_driver_control) return;
    glad_glDisableDriverControlQCOM = (PFNGLDISABLEDRIVERCONTROLQCOMPROC) load(userptr, "glDisableDriverControlQCOM");
    glad_glEnableDriverControlQCOM = (PFNGLENABLEDRIVERCONTROLQCOMPROC) load(userptr, "glEnableDriverControlQCOM");
    glad_glGetDriverControlStringQCOM = (PFNGLGETDRIVERCONTROLSTRINGQCOMPROC) load(userptr, "glGetDriverControlStringQCOM");
    glad_glGetDriverControlsQCOM = (PFNGLGETDRIVERCONTROLSQCOMPROC) load(userptr, "glGetDriverControlsQCOM");
}
static void glad_gl_load_GL_QCOM_extended_get( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_extended_get) return;
    glad_glExtGetBufferPointervQCOM = (PFNGLEXTGETBUFFERPOINTERVQCOMPROC) load(userptr, "glExtGetBufferPointervQCOM");
    glad_glExtGetBuffersQCOM = (PFNGLEXTGETBUFFERSQCOMPROC) load(userptr, "glExtGetBuffersQCOM");
    glad_glExtGetFramebuffersQCOM = (PFNGLEXTGETFRAMEBUFFERSQCOMPROC) load(userptr, "glExtGetFramebuffersQCOM");
    glad_glExtGetRenderbuffersQCOM = (PFNGLEXTGETRENDERBUFFERSQCOMPROC) load(userptr, "glExtGetRenderbuffersQCOM");
    glad_glExtGetTexLevelParameterivQCOM = (PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC) load(userptr, "glExtGetTexLevelParameterivQCOM");
    glad_glExtGetTexSubImageQCOM = (PFNGLEXTGETTEXSUBIMAGEQCOMPROC) load(userptr, "glExtGetTexSubImageQCOM");
    glad_glExtGetTexturesQCOM = (PFNGLEXTGETTEXTURESQCOMPROC) load(userptr, "glExtGetTexturesQCOM");
    glad_glExtTexObjectStateOverrideiQCOM = (PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC) load(userptr, "glExtTexObjectStateOverrideiQCOM");
}
static void glad_gl_load_GL_QCOM_extended_get2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_extended_get2) return;
    glad_glExtGetProgramBinarySourceQCOM = (PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC) load(userptr, "glExtGetProgramBinarySourceQCOM");
    glad_glExtGetProgramsQCOM = (PFNGLEXTGETPROGRAMSQCOMPROC) load(userptr, "glExtGetProgramsQCOM");
    glad_glExtGetShadersQCOM = (PFNGLEXTGETSHADERSQCOMPROC) load(userptr, "glExtGetShadersQCOM");
    glad_glExtIsProgramBinaryQCOM = (PFNGLEXTISPROGRAMBINARYQCOMPROC) load(userptr, "glExtIsProgramBinaryQCOM");
}
static void glad_gl_load_GL_QCOM_tiled_rendering( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_tiled_rendering) return;
    glad_glEndTilingQCOM = (PFNGLENDTILINGQCOMPROC) load(userptr, "glEndTilingQCOM");
    glad_glStartTilingQCOM = (PFNGLSTARTTILINGQCOMPROC) load(userptr, "glStartTilingQCOM");
}



static void glad_gl_free_extensions(char **exts_i) {
    if (exts_i != NULL) {
        unsigned int index;
        for(index = 0; exts_i[index]; index++) {
            free((void *) (exts_i[index]));
        }
        free((void *)exts_i);
        exts_i = NULL;
    }
}
static int glad_gl_get_extensions( const char **out_exts, char ***out_exts_i) {
#if defined(GL_ES_VERSION_3_0) || defined(GL_VERSION_3_0)
    if (glad_glGetStringi != NULL && glad_glGetIntegerv != NULL) {
        unsigned int index = 0;
        unsigned int num_exts_i = 0;
        char **exts_i = NULL;
        glad_glGetIntegerv(GL_NUM_EXTENSIONS, (int*) &num_exts_i);
        exts_i = (char **) malloc((num_exts_i + 1) * (sizeof *exts_i));
        if (exts_i == NULL) {
            return 0;
        }
        for(index = 0; index < num_exts_i; index++) {
            const char *gl_str_tmp = (const char*) glad_glGetStringi(GL_EXTENSIONS, index);
            size_t len = strlen(gl_str_tmp) + 1;

            char *local_str = (char*) malloc(len * sizeof(char));
            if(local_str == NULL) {
                exts_i[index] = NULL;
                glad_gl_free_extensions(exts_i);
                return 0;
            }

            memcpy(local_str, gl_str_tmp, len * sizeof(char));
            exts_i[index] = local_str;
        }
        exts_i[index] = NULL;

        *out_exts_i = exts_i;

        return 1;
    }
#else
    GLAD_UNUSED(out_exts_i);
#endif
    if (glad_glGetString == NULL) {
        return 0;
    }
    *out_exts = (const char *)glad_glGetString(GL_EXTENSIONS);
    return 1;
}
static int glad_gl_has_extension(const char *exts, char **exts_i, const char *ext) {
    if(exts_i) {
        unsigned int index;
        for(index = 0; exts_i[index]; index++) {
            const char *e = exts_i[index];
            if(strcmp(e, ext) == 0) {
                return 1;
            }
        }
    } else {
        const char *extensions;
        const char *loc;
        const char *terminator;
        extensions = exts;
        if(extensions == NULL || ext == NULL) {
            return 0;
        }
        while(1) {
            loc = strstr(extensions, ext);
            if(loc == NULL) {
                return 0;
            }
            terminator = loc + strlen(ext);
            if((loc == extensions || *(loc - 1) == ' ') &&
                (*terminator == ' ' || *terminator == '\0')) {
                return 1;
            }
            extensions = terminator;
        }
    }
    return 0;
}

static GLADapiproc glad_gl_get_proc_from_userptr(void *userptr, const char* name) {
    return (GLAD_GNUC_EXTENSION (GLADapiproc (*)(const char *name)) userptr)(name);
}

static int glad_gl_find_extensions_gles1(void) {
    const char *exts = NULL;
    char **exts_i = NULL;
    if (!glad_gl_get_extensions(&exts, &exts_i)) return 0;

    GLAD_GL_AMD_compressed_3DC_texture = glad_gl_has_extension(exts, exts_i, "GL_AMD_compressed_3DC_texture");
    GLAD_GL_AMD_compressed_ATC_texture = glad_gl_has_extension(exts, exts_i, "GL_AMD_compressed_ATC_texture");
    GLAD_GL_APPLE_copy_texture_levels = glad_gl_has_extension(exts, exts_i, "GL_APPLE_copy_texture_levels");
    GLAD_GL_APPLE_framebuffer_multisample = glad_gl_has_extension(exts, exts_i, "GL_APPLE_framebuffer_multisample");
    GLAD_GL_APPLE_sync = glad_gl_has_extension(exts, exts_i, "GL_APPLE_sync");
    GLAD_GL_APPLE_texture_2D_limited_npot = glad_gl_has_extension(exts, exts_i, "GL_APPLE_texture_2D_limited_npot");
    GLAD_GL_APPLE_texture_format_BGRA8888 = glad_gl_has_extension(exts, exts_i, "GL_APPLE_texture_format_BGRA8888");
    GLAD_GL_APPLE_texture_max_level = glad_gl_has_extension(exts, exts_i, "GL_APPLE_texture_max_level");
    GLAD_GL_ARM_rgba8 = glad_gl_has_extension(exts, exts_i, "GL_ARM_rgba8");
    GLAD_GL_EXT_blend_minmax = glad_gl_has_extension(exts, exts_i, "GL_EXT_blend_minmax");
    GLAD_GL_EXT_debug_marker = glad_gl_has_extension(exts, exts_i, "GL_EXT_debug_marker");
    GLAD_GL_EXT_discard_framebuffer = glad_gl_has_extension(exts, exts_i, "GL_EXT_discard_framebuffer");
    GLAD_GL_EXT_map_buffer_range = glad_gl_has_extension(exts, exts_i, "GL_EXT_map_buffer_range");
    GLAD_GL_EXT_multi_draw_arrays = glad_gl_has_extension(exts, exts_i, "GL_EXT_multi_draw_arrays");
    GLAD_GL_EXT_multisampled_render_to_texture = glad_gl_has_extension(exts, exts_i, "GL_EXT_multisampled_render_to_texture");
    GLAD_GL_EXT_read_format_bgra = glad_gl_has_extension(exts, exts_i, "GL_EXT_read_format_bgra");
    GLAD_GL_EXT_robustness = glad_gl_has_extension(exts, exts_i, "GL_EXT_robustness");
    GLAD_GL_EXT_sRGB = glad_gl_has_extension(exts, exts_i, "GL_EXT_sRGB");
    GLAD_GL_EXT_texture_compression_dxt1 = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_compression_dxt1");
    GLAD_GL_EXT_texture_filter_anisotropic = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_filter_anisotropic");
    GLAD_GL_EXT_texture_format_BGRA8888 = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_format_BGRA8888");
    GLAD_GL_EXT_texture_lod_bias = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_lod_bias");
    GLAD_GL_EXT_texture_storage = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_storage");
    GLAD_GL_IMG_multisampled_render_to_texture = glad_gl_has_extension(exts, exts_i, "GL_IMG_multisampled_render_to_texture");
    GLAD_GL_IMG_read_format = glad_gl_has_extension(exts, exts_i, "GL_IMG_read_format");
    GLAD_GL_IMG_texture_compression_pvrtc = glad_gl_has_extension(exts, exts_i, "GL_IMG_texture_compression_pvrtc");
    GLAD_GL_IMG_texture_env_enhanced_fixed_function = glad_gl_has_extension(exts, exts_i, "GL_IMG_texture_env_enhanced_fixed_function");
    GLAD_GL_IMG_user_clip_plane = glad_gl_has_extension(exts, exts_i, "GL_IMG_user_clip_plane");
    GLAD_GL_KHR_debug = glad_gl_has_extension(exts, exts_i, "GL_KHR_debug");
    GLAD_GL_NV_fence = glad_gl_has_extension(exts, exts_i, "GL_NV_fence");
    GLAD_GL_OES_EGL_image = glad_gl_has_extension(exts, exts_i, "GL_OES_EGL_image");
    GLAD_GL_OES_EGL_image_external = glad_gl_has_extension(exts, exts_i, "GL_OES_EGL_image_external");
    GLAD_GL_OES_blend_equation_separate = glad_gl_has_extension(exts, exts_i, "GL_OES_blend_equation_separate");
    GLAD_GL_OES_blend_func_separate = glad_gl_has_extension(exts, exts_i, "GL_OES_blend_func_separate");
    GLAD_GL_OES_blend_subtract = glad_gl_has_extension(exts, exts_i, "GL_OES_blend_subtract");
    GLAD_GL_OES_byte_coordinates = glad_gl_has_extension(exts, exts_i, "GL_OES_byte_coordinates");
    GLAD_GL_OES_compressed_ETC1_RGB8_sub_texture = glad_gl_has_extension(exts, exts_i, "GL_OES_compressed_ETC1_RGB8_sub_texture");
    GLAD_GL_OES_compressed_ETC1_RGB8_texture = glad_gl_has_extension(exts, exts_i, "GL_OES_compressed_ETC1_RGB8_texture");
    GLAD_GL_OES_compressed_paletted_texture = glad_gl_has_extension(exts, exts_i, "GL_OES_compressed_paletted_texture");
    GLAD_GL_OES_depth24 = glad_gl_has_extension(exts, exts_i, "GL_OES_depth24");
    GLAD_GL_OES_depth32 = glad_gl_has_extension(exts, exts_i, "GL_OES_depth32");
    GLAD_GL_OES_draw_texture = glad_gl_has_extension(exts, exts_i, "GL_OES_draw_texture");
    GLAD_GL_OES_element_index_uint = glad_gl_has_extension(exts, exts_i, "GL_OES_element_index_uint");
    GLAD_GL_OES_extended_matrix_palette = glad_gl_has_extension(exts, exts_i, "GL_OES_extended_matrix_palette");
    GLAD_GL_OES_fbo_render_mipmap = glad_gl_has_extension(exts, exts_i, "GL_OES_fbo_render_mipmap");
    GLAD_GL_OES_fixed_point = glad_gl_has_extension(exts, exts_i, "GL_OES_fixed_point");
    GLAD_GL_OES_framebuffer_object = glad_gl_has_extension(exts, exts_i, "GL_OES_framebuffer_object");
    GLAD_GL_OES_mapbuffer = glad_gl_has_extension(exts, exts_i, "GL_OES_mapbuffer");
    GLAD_GL_OES_matrix_get = glad_gl_has_extension(exts, exts_i, "GL_OES_matrix_get");
    GLAD_GL_OES_matrix_palette = glad_gl_has_extension(exts, exts_i, "GL_OES_matrix_palette");
    GLAD_GL_OES_packed_depth_stencil = glad_gl_has_extension(exts, exts_i, "GL_OES_packed_depth_stencil");
    GLAD_GL_OES_point_size_array = glad_gl_has_extension(exts, exts_i, "GL_OES_point_size_array");
    GLAD_GL_OES_point_sprite = glad_gl_has_extension(exts, exts_i, "GL_OES_point_sprite");
    GLAD_GL_OES_query_matrix = glad_gl_has_extension(exts, exts_i, "GL_OES_query_matrix");
    GLAD_GL_OES_read_format = glad_gl_has_extension(exts, exts_i, "GL_OES_read_format");
    GLAD_GL_OES_required_internalformat = glad_gl_has_extension(exts, exts_i, "GL_OES_required_internalformat");
    GLAD_GL_OES_rgb8_rgba8 = glad_gl_has_extension(exts, exts_i, "GL_OES_rgb8_rgba8");
    GLAD_GL_OES_single_precision = glad_gl_has_extension(exts, exts_i, "GL_OES_single_precision");
    GLAD_GL_OES_stencil1 = glad_gl_has_extension(exts, exts_i, "GL_OES_stencil1");
    GLAD_GL_OES_stencil4 = glad_gl_has_extension(exts, exts_i, "GL_OES_stencil4");
    GLAD_GL_OES_stencil8 = glad_gl_has_extension(exts, exts_i, "GL_OES_stencil8");
    GLAD_GL_OES_stencil_wrap = glad_gl_has_extension(exts, exts_i, "GL_OES_stencil_wrap");
    GLAD_GL_OES_surfaceless_context = glad_gl_has_extension(exts, exts_i, "GL_OES_surfaceless_context");
    GLAD_GL_OES_texture_cube_map = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_cube_map");
    GLAD_GL_OES_texture_env_crossbar = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_env_crossbar");
    GLAD_GL_OES_texture_mirrored_repeat = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_mirrored_repeat");
    GLAD_GL_OES_texture_npot = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_npot");
    GLAD_GL_OES_vertex_array_object = glad_gl_has_extension(exts, exts_i, "GL_OES_vertex_array_object");
    GLAD_GL_QCOM_driver_control = glad_gl_has_extension(exts, exts_i, "GL_QCOM_driver_control");
    GLAD_GL_QCOM_extended_get = glad_gl_has_extension(exts, exts_i, "GL_QCOM_extended_get");
    GLAD_GL_QCOM_extended_get2 = glad_gl_has_extension(exts, exts_i, "GL_QCOM_extended_get2");
    GLAD_GL_QCOM_perfmon_global_mode = glad_gl_has_extension(exts, exts_i, "GL_QCOM_perfmon_global_mode");
    GLAD_GL_QCOM_tiled_rendering = glad_gl_has_extension(exts, exts_i, "GL_QCOM_tiled_rendering");
    GLAD_GL_QCOM_writeonly_rendering = glad_gl_has_extension(exts, exts_i, "GL_QCOM_writeonly_rendering");

    glad_gl_free_extensions(exts_i);

    return 1;
}

static int glad_gl_find_core_gles1(void) {
    int i;
    const char* version;
    const char* prefixes[] = {
        "OpenGL ES-CM ",
        "OpenGL ES-CL ",
        "OpenGL ES ",
        "OpenGL SC ",
        NULL
    };
    int major = 0;
    int minor = 0;
    version = (const char*) glad_glGetString(GL_VERSION);
    if (!version) return 0;
    for (i = 0;  prefixes[i];  i++) {
        const size_t length = strlen(prefixes[i]);
        if (strncmp(version, prefixes[i], length) == 0) {
            version += length;
            break;
        }
    }

    GLAD_IMPL_UTIL_SSCANF(version, "%d.%d", &major, &minor);

    GLAD_GL_VERSION_ES_CM_1_0 = (major == 1 && minor >= 0) || major > 1;

    return GLAD_MAKE_VERSION(major, minor);
}

int gladLoadGLES1UserPtr( GLADuserptrloadfunc load, void *userptr) {
    int version;

    glad_glGetString = (PFNGLGETSTRINGPROC) load(userptr, "glGetString");
    if(glad_glGetString == NULL) return 0;
    version = glad_gl_find_core_gles1();

    glad_gl_load_GL_VERSION_ES_CM_1_0(load, userptr);

    if (!glad_gl_find_extensions_gles1()) return 0;
    glad_gl_load_GL_APPLE_copy_texture_levels(load, userptr);
    glad_gl_load_GL_APPLE_framebuffer_multisample(load, userptr);
    glad_gl_load_GL_APPLE_sync(load, userptr);
    glad_gl_load_GL_EXT_debug_marker(load, userptr);
    glad_gl_load_GL_EXT_discard_framebuffer(load, userptr);
    glad_gl_load_GL_EXT_map_buffer_range(load, userptr);
    glad_gl_load_GL_EXT_multi_draw_arrays(load, userptr);
    glad_gl_load_GL_EXT_multisampled_render_to_texture(load, userptr);
    glad_gl_load_GL_EXT_robustness(load, userptr);
    glad_gl_load_GL_EXT_texture_storage(load, userptr);
    glad_gl_load_GL_IMG_multisampled_render_to_texture(load, userptr);
    glad_gl_load_GL_IMG_user_clip_plane(load, userptr);
    glad_gl_load_GL_NV_fence(load, userptr);
    glad_gl_load_GL_OES_EGL_image(load, userptr);
    glad_gl_load_GL_OES_EGL_image_external(load, userptr);
    glad_gl_load_GL_OES_blend_equation_separate(load, userptr);
    glad_gl_load_GL_OES_blend_func_separate(load, userptr);
    glad_gl_load_GL_OES_blend_subtract(load, userptr);
    glad_gl_load_GL_OES_draw_texture(load, userptr);
    glad_gl_load_GL_OES_fixed_point(load, userptr);
    glad_gl_load_GL_OES_framebuffer_object(load, userptr);
    glad_gl_load_GL_OES_mapbuffer(load, userptr);
    glad_gl_load_GL_OES_matrix_palette(load, userptr);
    glad_gl_load_GL_OES_point_size_array(load, userptr);
    glad_gl_load_GL_OES_query_matrix(load, userptr);
    glad_gl_load_GL_OES_single_precision(load, userptr);
    glad_gl_load_GL_OES_texture_cube_map(load, userptr);
    glad_gl_load_GL_OES_vertex_array_object(load, userptr);
    glad_gl_load_GL_QCOM_driver_control(load, userptr);
    glad_gl_load_GL_QCOM_extended_get(load, userptr);
    glad_gl_load_GL_QCOM_extended_get2(load, userptr);
    glad_gl_load_GL_QCOM_tiled_rendering(load, userptr);



    return version;
}


int gladLoadGLES1( GLADloadfunc load) {
    return gladLoadGLES1UserPtr( glad_gl_get_proc_from_userptr, GLAD_GNUC_EXTENSION (void*) load);
}



 
void gladInstallGLES1Debug(void) {
    glad_debug_glActiveTexture = glad_debug_impl_glActiveTexture;
    glad_debug_glAlphaFunc = glad_debug_impl_glAlphaFunc;
    glad_debug_glAlphaFuncx = glad_debug_impl_glAlphaFuncx;
    glad_debug_glAlphaFuncxOES = glad_debug_impl_glAlphaFuncxOES;
    glad_debug_glBindBuffer = glad_debug_impl_glBindBuffer;
    glad_debug_glBindFramebufferOES = glad_debug_impl_glBindFramebufferOES;
    glad_debug_glBindRenderbufferOES = glad_debug_impl_glBindRenderbufferOES;
    glad_debug_glBindTexture = glad_debug_impl_glBindTexture;
    glad_debug_glBindVertexArrayOES = glad_debug_impl_glBindVertexArrayOES;
    glad_debug_glBlendEquationOES = glad_debug_impl_glBlendEquationOES;
    glad_debug_glBlendEquationSeparateOES = glad_debug_impl_glBlendEquationSeparateOES;
    glad_debug_glBlendFunc = glad_debug_impl_glBlendFunc;
    glad_debug_glBlendFuncSeparateOES = glad_debug_impl_glBlendFuncSeparateOES;
    glad_debug_glBufferData = glad_debug_impl_glBufferData;
    glad_debug_glBufferSubData = glad_debug_impl_glBufferSubData;
    glad_debug_glCheckFramebufferStatusOES = glad_debug_impl_glCheckFramebufferStatusOES;
    glad_debug_glClear = glad_debug_impl_glClear;
    glad_debug_glClearColor = glad_debug_impl_glClearColor;
    glad_debug_glClearColorx = glad_debug_impl_glClearColorx;
    glad_debug_glClearColorxOES = glad_debug_impl_glClearColorxOES;
    glad_debug_glClearDepthf = glad_debug_impl_glClearDepthf;
    glad_debug_glClearDepthfOES = glad_debug_impl_glClearDepthfOES;
    glad_debug_glClearDepthx = glad_debug_impl_glClearDepthx;
    glad_debug_glClearDepthxOES = glad_debug_impl_glClearDepthxOES;
    glad_debug_glClearStencil = glad_debug_impl_glClearStencil;
    glad_debug_glClientActiveTexture = glad_debug_impl_glClientActiveTexture;
    glad_debug_glClientWaitSyncAPPLE = glad_debug_impl_glClientWaitSyncAPPLE;
    glad_debug_glClipPlanef = glad_debug_impl_glClipPlanef;
    glad_debug_glClipPlanefIMG = glad_debug_impl_glClipPlanefIMG;
    glad_debug_glClipPlanefOES = glad_debug_impl_glClipPlanefOES;
    glad_debug_glClipPlanex = glad_debug_impl_glClipPlanex;
    glad_debug_glClipPlanexIMG = glad_debug_impl_glClipPlanexIMG;
    glad_debug_glClipPlanexOES = glad_debug_impl_glClipPlanexOES;
    glad_debug_glColor4f = glad_debug_impl_glColor4f;
    glad_debug_glColor4ub = glad_debug_impl_glColor4ub;
    glad_debug_glColor4x = glad_debug_impl_glColor4x;
    glad_debug_glColor4xOES = glad_debug_impl_glColor4xOES;
    glad_debug_glColorMask = glad_debug_impl_glColorMask;
    glad_debug_glColorPointer = glad_debug_impl_glColorPointer;
    glad_debug_glCompressedTexImage2D = glad_debug_impl_glCompressedTexImage2D;
    glad_debug_glCompressedTexSubImage2D = glad_debug_impl_glCompressedTexSubImage2D;
    glad_debug_glCopyTexImage2D = glad_debug_impl_glCopyTexImage2D;
    glad_debug_glCopyTexSubImage2D = glad_debug_impl_glCopyTexSubImage2D;
    glad_debug_glCopyTextureLevelsAPPLE = glad_debug_impl_glCopyTextureLevelsAPPLE;
    glad_debug_glCullFace = glad_debug_impl_glCullFace;
    glad_debug_glCurrentPaletteMatrixOES = glad_debug_impl_glCurrentPaletteMatrixOES;
    glad_debug_glDeleteBuffers = glad_debug_impl_glDeleteBuffers;
    glad_debug_glDeleteFencesNV = glad_debug_impl_glDeleteFencesNV;
    glad_debug_glDeleteFramebuffersOES = glad_debug_impl_glDeleteFramebuffersOES;
    glad_debug_glDeleteRenderbuffersOES = glad_debug_impl_glDeleteRenderbuffersOES;
    glad_debug_glDeleteSyncAPPLE = glad_debug_impl_glDeleteSyncAPPLE;
    glad_debug_glDeleteTextures = glad_debug_impl_glDeleteTextures;
    glad_debug_glDeleteVertexArraysOES = glad_debug_impl_glDeleteVertexArraysOES;
    glad_debug_glDepthFunc = glad_debug_impl_glDepthFunc;
    glad_debug_glDepthMask = glad_debug_impl_glDepthMask;
    glad_debug_glDepthRangef = glad_debug_impl_glDepthRangef;
    glad_debug_glDepthRangefOES = glad_debug_impl_glDepthRangefOES;
    glad_debug_glDepthRangex = glad_debug_impl_glDepthRangex;
    glad_debug_glDepthRangexOES = glad_debug_impl_glDepthRangexOES;
    glad_debug_glDisable = glad_debug_impl_glDisable;
    glad_debug_glDisableClientState = glad_debug_impl_glDisableClientState;
    glad_debug_glDisableDriverControlQCOM = glad_debug_impl_glDisableDriverControlQCOM;
    glad_debug_glDiscardFramebufferEXT = glad_debug_impl_glDiscardFramebufferEXT;
    glad_debug_glDrawArrays = glad_debug_impl_glDrawArrays;
    glad_debug_glDrawElements = glad_debug_impl_glDrawElements;
    glad_debug_glDrawTexfOES = glad_debug_impl_glDrawTexfOES;
    glad_debug_glDrawTexfvOES = glad_debug_impl_glDrawTexfvOES;
    glad_debug_glDrawTexiOES = glad_debug_impl_glDrawTexiOES;
    glad_debug_glDrawTexivOES = glad_debug_impl_glDrawTexivOES;
    glad_debug_glDrawTexsOES = glad_debug_impl_glDrawTexsOES;
    glad_debug_glDrawTexsvOES = glad_debug_impl_glDrawTexsvOES;
    glad_debug_glDrawTexxOES = glad_debug_impl_glDrawTexxOES;
    glad_debug_glDrawTexxvOES = glad_debug_impl_glDrawTexxvOES;
    glad_debug_glEGLImageTargetRenderbufferStorageOES = glad_debug_impl_glEGLImageTargetRenderbufferStorageOES;
    glad_debug_glEGLImageTargetTexture2DOES = glad_debug_impl_glEGLImageTargetTexture2DOES;
    glad_debug_glEnable = glad_debug_impl_glEnable;
    glad_debug_glEnableClientState = glad_debug_impl_glEnableClientState;
    glad_debug_glEnableDriverControlQCOM = glad_debug_impl_glEnableDriverControlQCOM;
    glad_debug_glEndTilingQCOM = glad_debug_impl_glEndTilingQCOM;
    glad_debug_glExtGetBufferPointervQCOM = glad_debug_impl_glExtGetBufferPointervQCOM;
    glad_debug_glExtGetBuffersQCOM = glad_debug_impl_glExtGetBuffersQCOM;
    glad_debug_glExtGetFramebuffersQCOM = glad_debug_impl_glExtGetFramebuffersQCOM;
    glad_debug_glExtGetProgramBinarySourceQCOM = glad_debug_impl_glExtGetProgramBinarySourceQCOM;
    glad_debug_glExtGetProgramsQCOM = glad_debug_impl_glExtGetProgramsQCOM;
    glad_debug_glExtGetRenderbuffersQCOM = glad_debug_impl_glExtGetRenderbuffersQCOM;
    glad_debug_glExtGetShadersQCOM = glad_debug_impl_glExtGetShadersQCOM;
    glad_debug_glExtGetTexLevelParameterivQCOM = glad_debug_impl_glExtGetTexLevelParameterivQCOM;
    glad_debug_glExtGetTexSubImageQCOM = glad_debug_impl_glExtGetTexSubImageQCOM;
    glad_debug_glExtGetTexturesQCOM = glad_debug_impl_glExtGetTexturesQCOM;
    glad_debug_glExtIsProgramBinaryQCOM = glad_debug_impl_glExtIsProgramBinaryQCOM;
    glad_debug_glExtTexObjectStateOverrideiQCOM = glad_debug_impl_glExtTexObjectStateOverrideiQCOM;
    glad_debug_glFenceSyncAPPLE = glad_debug_impl_glFenceSyncAPPLE;
    glad_debug_glFinish = glad_debug_impl_glFinish;
    glad_debug_glFinishFenceNV = glad_debug_impl_glFinishFenceNV;
    glad_debug_glFlush = glad_debug_impl_glFlush;
    glad_debug_glFlushMappedBufferRangeEXT = glad_debug_impl_glFlushMappedBufferRangeEXT;
    glad_debug_glFogf = glad_debug_impl_glFogf;
    glad_debug_glFogfv = glad_debug_impl_glFogfv;
    glad_debug_glFogx = glad_debug_impl_glFogx;
    glad_debug_glFogxOES = glad_debug_impl_glFogxOES;
    glad_debug_glFogxv = glad_debug_impl_glFogxv;
    glad_debug_glFogxvOES = glad_debug_impl_glFogxvOES;
    glad_debug_glFramebufferRenderbufferOES = glad_debug_impl_glFramebufferRenderbufferOES;
    glad_debug_glFramebufferTexture2DMultisampleEXT = glad_debug_impl_glFramebufferTexture2DMultisampleEXT;
    glad_debug_glFramebufferTexture2DMultisampleIMG = glad_debug_impl_glFramebufferTexture2DMultisampleIMG;
    glad_debug_glFramebufferTexture2DOES = glad_debug_impl_glFramebufferTexture2DOES;
    glad_debug_glFrontFace = glad_debug_impl_glFrontFace;
    glad_debug_glFrustumf = glad_debug_impl_glFrustumf;
    glad_debug_glFrustumfOES = glad_debug_impl_glFrustumfOES;
    glad_debug_glFrustumx = glad_debug_impl_glFrustumx;
    glad_debug_glFrustumxOES = glad_debug_impl_glFrustumxOES;
    glad_debug_glGenBuffers = glad_debug_impl_glGenBuffers;
    glad_debug_glGenFencesNV = glad_debug_impl_glGenFencesNV;
    glad_debug_glGenFramebuffersOES = glad_debug_impl_glGenFramebuffersOES;
    glad_debug_glGenRenderbuffersOES = glad_debug_impl_glGenRenderbuffersOES;
    glad_debug_glGenTextures = glad_debug_impl_glGenTextures;
    glad_debug_glGenVertexArraysOES = glad_debug_impl_glGenVertexArraysOES;
    glad_debug_glGenerateMipmapOES = glad_debug_impl_glGenerateMipmapOES;
    glad_debug_glGetBooleanv = glad_debug_impl_glGetBooleanv;
    glad_debug_glGetBufferParameteriv = glad_debug_impl_glGetBufferParameteriv;
    glad_debug_glGetBufferPointervOES = glad_debug_impl_glGetBufferPointervOES;
    glad_debug_glGetClipPlanef = glad_debug_impl_glGetClipPlanef;
    glad_debug_glGetClipPlanefOES = glad_debug_impl_glGetClipPlanefOES;
    glad_debug_glGetClipPlanex = glad_debug_impl_glGetClipPlanex;
    glad_debug_glGetClipPlanexOES = glad_debug_impl_glGetClipPlanexOES;
    glad_debug_glGetDriverControlStringQCOM = glad_debug_impl_glGetDriverControlStringQCOM;
    glad_debug_glGetDriverControlsQCOM = glad_debug_impl_glGetDriverControlsQCOM;
    glad_debug_glGetError = glad_debug_impl_glGetError;
    glad_debug_glGetFenceivNV = glad_debug_impl_glGetFenceivNV;
    glad_debug_glGetFixedv = glad_debug_impl_glGetFixedv;
    glad_debug_glGetFixedvOES = glad_debug_impl_glGetFixedvOES;
    glad_debug_glGetFloatv = glad_debug_impl_glGetFloatv;
    glad_debug_glGetFramebufferAttachmentParameterivOES = glad_debug_impl_glGetFramebufferAttachmentParameterivOES;
    glad_debug_glGetGraphicsResetStatusEXT = glad_debug_impl_glGetGraphicsResetStatusEXT;
    glad_debug_glGetInteger64vAPPLE = glad_debug_impl_glGetInteger64vAPPLE;
    glad_debug_glGetIntegerv = glad_debug_impl_glGetIntegerv;
    glad_debug_glGetLightfv = glad_debug_impl_glGetLightfv;
    glad_debug_glGetLightxv = glad_debug_impl_glGetLightxv;
    glad_debug_glGetLightxvOES = glad_debug_impl_glGetLightxvOES;
    glad_debug_glGetMaterialfv = glad_debug_impl_glGetMaterialfv;
    glad_debug_glGetMaterialxv = glad_debug_impl_glGetMaterialxv;
    glad_debug_glGetMaterialxvOES = glad_debug_impl_glGetMaterialxvOES;
    glad_debug_glGetPointerv = glad_debug_impl_glGetPointerv;
    glad_debug_glGetRenderbufferParameterivOES = glad_debug_impl_glGetRenderbufferParameterivOES;
    glad_debug_glGetString = glad_debug_impl_glGetString;
    glad_debug_glGetSyncivAPPLE = glad_debug_impl_glGetSyncivAPPLE;
    glad_debug_glGetTexEnvfv = glad_debug_impl_glGetTexEnvfv;
    glad_debug_glGetTexEnviv = glad_debug_impl_glGetTexEnviv;
    glad_debug_glGetTexEnvxv = glad_debug_impl_glGetTexEnvxv;
    glad_debug_glGetTexEnvxvOES = glad_debug_impl_glGetTexEnvxvOES;
    glad_debug_glGetTexGenfvOES = glad_debug_impl_glGetTexGenfvOES;
    glad_debug_glGetTexGenivOES = glad_debug_impl_glGetTexGenivOES;
    glad_debug_glGetTexGenxvOES = glad_debug_impl_glGetTexGenxvOES;
    glad_debug_glGetTexParameterfv = glad_debug_impl_glGetTexParameterfv;
    glad_debug_glGetTexParameteriv = glad_debug_impl_glGetTexParameteriv;
    glad_debug_glGetTexParameterxv = glad_debug_impl_glGetTexParameterxv;
    glad_debug_glGetTexParameterxvOES = glad_debug_impl_glGetTexParameterxvOES;
    glad_debug_glGetnUniformfvEXT = glad_debug_impl_glGetnUniformfvEXT;
    glad_debug_glGetnUniformivEXT = glad_debug_impl_glGetnUniformivEXT;
    glad_debug_glHint = glad_debug_impl_glHint;
    glad_debug_glInsertEventMarkerEXT = glad_debug_impl_glInsertEventMarkerEXT;
    glad_debug_glIsBuffer = glad_debug_impl_glIsBuffer;
    glad_debug_glIsEnabled = glad_debug_impl_glIsEnabled;
    glad_debug_glIsFenceNV = glad_debug_impl_glIsFenceNV;
    glad_debug_glIsFramebufferOES = glad_debug_impl_glIsFramebufferOES;
    glad_debug_glIsRenderbufferOES = glad_debug_impl_glIsRenderbufferOES;
    glad_debug_glIsSyncAPPLE = glad_debug_impl_glIsSyncAPPLE;
    glad_debug_glIsTexture = glad_debug_impl_glIsTexture;
    glad_debug_glIsVertexArrayOES = glad_debug_impl_glIsVertexArrayOES;
    glad_debug_glLightModelf = glad_debug_impl_glLightModelf;
    glad_debug_glLightModelfv = glad_debug_impl_glLightModelfv;
    glad_debug_glLightModelx = glad_debug_impl_glLightModelx;
    glad_debug_glLightModelxOES = glad_debug_impl_glLightModelxOES;
    glad_debug_glLightModelxv = glad_debug_impl_glLightModelxv;
    glad_debug_glLightModelxvOES = glad_debug_impl_glLightModelxvOES;
    glad_debug_glLightf = glad_debug_impl_glLightf;
    glad_debug_glLightfv = glad_debug_impl_glLightfv;
    glad_debug_glLightx = glad_debug_impl_glLightx;
    glad_debug_glLightxOES = glad_debug_impl_glLightxOES;
    glad_debug_glLightxv = glad_debug_impl_glLightxv;
    glad_debug_glLightxvOES = glad_debug_impl_glLightxvOES;
    glad_debug_glLineWidth = glad_debug_impl_glLineWidth;
    glad_debug_glLineWidthx = glad_debug_impl_glLineWidthx;
    glad_debug_glLineWidthxOES = glad_debug_impl_glLineWidthxOES;
    glad_debug_glLoadIdentity = glad_debug_impl_glLoadIdentity;
    glad_debug_glLoadMatrixf = glad_debug_impl_glLoadMatrixf;
    glad_debug_glLoadMatrixx = glad_debug_impl_glLoadMatrixx;
    glad_debug_glLoadMatrixxOES = glad_debug_impl_glLoadMatrixxOES;
    glad_debug_glLoadPaletteFromModelViewMatrixOES = glad_debug_impl_glLoadPaletteFromModelViewMatrixOES;
    glad_debug_glLogicOp = glad_debug_impl_glLogicOp;
    glad_debug_glMapBufferOES = glad_debug_impl_glMapBufferOES;
    glad_debug_glMapBufferRangeEXT = glad_debug_impl_glMapBufferRangeEXT;
    glad_debug_glMaterialf = glad_debug_impl_glMaterialf;
    glad_debug_glMaterialfv = glad_debug_impl_glMaterialfv;
    glad_debug_glMaterialx = glad_debug_impl_glMaterialx;
    glad_debug_glMaterialxOES = glad_debug_impl_glMaterialxOES;
    glad_debug_glMaterialxv = glad_debug_impl_glMaterialxv;
    glad_debug_glMaterialxvOES = glad_debug_impl_glMaterialxvOES;
    glad_debug_glMatrixIndexPointerOES = glad_debug_impl_glMatrixIndexPointerOES;
    glad_debug_glMatrixMode = glad_debug_impl_glMatrixMode;
    glad_debug_glMultMatrixf = glad_debug_impl_glMultMatrixf;
    glad_debug_glMultMatrixx = glad_debug_impl_glMultMatrixx;
    glad_debug_glMultMatrixxOES = glad_debug_impl_glMultMatrixxOES;
    glad_debug_glMultiDrawArraysEXT = glad_debug_impl_glMultiDrawArraysEXT;
    glad_debug_glMultiDrawElementsEXT = glad_debug_impl_glMultiDrawElementsEXT;
    glad_debug_glMultiTexCoord4f = glad_debug_impl_glMultiTexCoord4f;
    glad_debug_glMultiTexCoord4x = glad_debug_impl_glMultiTexCoord4x;
    glad_debug_glMultiTexCoord4xOES = glad_debug_impl_glMultiTexCoord4xOES;
    glad_debug_glNormal3f = glad_debug_impl_glNormal3f;
    glad_debug_glNormal3x = glad_debug_impl_glNormal3x;
    glad_debug_glNormal3xOES = glad_debug_impl_glNormal3xOES;
    glad_debug_glNormalPointer = glad_debug_impl_glNormalPointer;
    glad_debug_glOrthof = glad_debug_impl_glOrthof;
    glad_debug_glOrthofOES = glad_debug_impl_glOrthofOES;
    glad_debug_glOrthox = glad_debug_impl_glOrthox;
    glad_debug_glOrthoxOES = glad_debug_impl_glOrthoxOES;
    glad_debug_glPixelStorei = glad_debug_impl_glPixelStorei;
    glad_debug_glPointParameterf = glad_debug_impl_glPointParameterf;
    glad_debug_glPointParameterfv = glad_debug_impl_glPointParameterfv;
    glad_debug_glPointParameterx = glad_debug_impl_glPointParameterx;
    glad_debug_glPointParameterxOES = glad_debug_impl_glPointParameterxOES;
    glad_debug_glPointParameterxv = glad_debug_impl_glPointParameterxv;
    glad_debug_glPointParameterxvOES = glad_debug_impl_glPointParameterxvOES;
    glad_debug_glPointSize = glad_debug_impl_glPointSize;
    glad_debug_glPointSizePointerOES = glad_debug_impl_glPointSizePointerOES;
    glad_debug_glPointSizex = glad_debug_impl_glPointSizex;
    glad_debug_glPointSizexOES = glad_debug_impl_glPointSizexOES;
    glad_debug_glPolygonOffset = glad_debug_impl_glPolygonOffset;
    glad_debug_glPolygonOffsetx = glad_debug_impl_glPolygonOffsetx;
    glad_debug_glPolygonOffsetxOES = glad_debug_impl_glPolygonOffsetxOES;
    glad_debug_glPopGroupMarkerEXT = glad_debug_impl_glPopGroupMarkerEXT;
    glad_debug_glPopMatrix = glad_debug_impl_glPopMatrix;
    glad_debug_glPushGroupMarkerEXT = glad_debug_impl_glPushGroupMarkerEXT;
    glad_debug_glPushMatrix = glad_debug_impl_glPushMatrix;
    glad_debug_glQueryMatrixxOES = glad_debug_impl_glQueryMatrixxOES;
    glad_debug_glReadPixels = glad_debug_impl_glReadPixels;
    glad_debug_glReadnPixelsEXT = glad_debug_impl_glReadnPixelsEXT;
    glad_debug_glRenderbufferStorageMultisampleAPPLE = glad_debug_impl_glRenderbufferStorageMultisampleAPPLE;
    glad_debug_glRenderbufferStorageMultisampleEXT = glad_debug_impl_glRenderbufferStorageMultisampleEXT;
    glad_debug_glRenderbufferStorageMultisampleIMG = glad_debug_impl_glRenderbufferStorageMultisampleIMG;
    glad_debug_glRenderbufferStorageOES = glad_debug_impl_glRenderbufferStorageOES;
    glad_debug_glResolveMultisampleFramebufferAPPLE = glad_debug_impl_glResolveMultisampleFramebufferAPPLE;
    glad_debug_glRotatef = glad_debug_impl_glRotatef;
    glad_debug_glRotatex = glad_debug_impl_glRotatex;
    glad_debug_glRotatexOES = glad_debug_impl_glRotatexOES;
    glad_debug_glSampleCoverage = glad_debug_impl_glSampleCoverage;
    glad_debug_glSampleCoveragex = glad_debug_impl_glSampleCoveragex;
    glad_debug_glSampleCoveragexOES = glad_debug_impl_glSampleCoveragexOES;
    glad_debug_glScalef = glad_debug_impl_glScalef;
    glad_debug_glScalex = glad_debug_impl_glScalex;
    glad_debug_glScalexOES = glad_debug_impl_glScalexOES;
    glad_debug_glScissor = glad_debug_impl_glScissor;
    glad_debug_glSetFenceNV = glad_debug_impl_glSetFenceNV;
    glad_debug_glShadeModel = glad_debug_impl_glShadeModel;
    glad_debug_glStartTilingQCOM = glad_debug_impl_glStartTilingQCOM;
    glad_debug_glStencilFunc = glad_debug_impl_glStencilFunc;
    glad_debug_glStencilMask = glad_debug_impl_glStencilMask;
    glad_debug_glStencilOp = glad_debug_impl_glStencilOp;
    glad_debug_glTestFenceNV = glad_debug_impl_glTestFenceNV;
    glad_debug_glTexCoordPointer = glad_debug_impl_glTexCoordPointer;
    glad_debug_glTexEnvf = glad_debug_impl_glTexEnvf;
    glad_debug_glTexEnvfv = glad_debug_impl_glTexEnvfv;
    glad_debug_glTexEnvi = glad_debug_impl_glTexEnvi;
    glad_debug_glTexEnviv = glad_debug_impl_glTexEnviv;
    glad_debug_glTexEnvx = glad_debug_impl_glTexEnvx;
    glad_debug_glTexEnvxOES = glad_debug_impl_glTexEnvxOES;
    glad_debug_glTexEnvxv = glad_debug_impl_glTexEnvxv;
    glad_debug_glTexEnvxvOES = glad_debug_impl_glTexEnvxvOES;
    glad_debug_glTexGenfOES = glad_debug_impl_glTexGenfOES;
    glad_debug_glTexGenfvOES = glad_debug_impl_glTexGenfvOES;
    glad_debug_glTexGeniOES = glad_debug_impl_glTexGeniOES;
    glad_debug_glTexGenivOES = glad_debug_impl_glTexGenivOES;
    glad_debug_glTexGenxOES = glad_debug_impl_glTexGenxOES;
    glad_debug_glTexGenxvOES = glad_debug_impl_glTexGenxvOES;
    glad_debug_glTexImage2D = glad_debug_impl_glTexImage2D;
    glad_debug_glTexParameterf = glad_debug_impl_glTexParameterf;
    glad_debug_glTexParameterfv = glad_debug_impl_glTexParameterfv;
    glad_debug_glTexParameteri = glad_debug_impl_glTexParameteri;
    glad_debug_glTexParameteriv = glad_debug_impl_glTexParameteriv;
    glad_debug_glTexParameterx = glad_debug_impl_glTexParameterx;
    glad_debug_glTexParameterxOES = glad_debug_impl_glTexParameterxOES;
    glad_debug_glTexParameterxv = glad_debug_impl_glTexParameterxv;
    glad_debug_glTexParameterxvOES = glad_debug_impl_glTexParameterxvOES;
    glad_debug_glTexStorage1DEXT = glad_debug_impl_glTexStorage1DEXT;
    glad_debug_glTexStorage2DEXT = glad_debug_impl_glTexStorage2DEXT;
    glad_debug_glTexStorage3DEXT = glad_debug_impl_glTexStorage3DEXT;
    glad_debug_glTexSubImage2D = glad_debug_impl_glTexSubImage2D;
    glad_debug_glTextureStorage1DEXT = glad_debug_impl_glTextureStorage1DEXT;
    glad_debug_glTextureStorage2DEXT = glad_debug_impl_glTextureStorage2DEXT;
    glad_debug_glTextureStorage3DEXT = glad_debug_impl_glTextureStorage3DEXT;
    glad_debug_glTranslatef = glad_debug_impl_glTranslatef;
    glad_debug_glTranslatex = glad_debug_impl_glTranslatex;
    glad_debug_glTranslatexOES = glad_debug_impl_glTranslatexOES;
    glad_debug_glUnmapBufferOES = glad_debug_impl_glUnmapBufferOES;
    glad_debug_glVertexPointer = glad_debug_impl_glVertexPointer;
    glad_debug_glViewport = glad_debug_impl_glViewport;
    glad_debug_glWaitSyncAPPLE = glad_debug_impl_glWaitSyncAPPLE;
    glad_debug_glWeightPointerOES = glad_debug_impl_glWeightPointerOES;
}

void gladUninstallGLES1Debug(void) {
    glad_debug_glActiveTexture = glad_glActiveTexture;
    glad_debug_glAlphaFunc = glad_glAlphaFunc;
    glad_debug_glAlphaFuncx = glad_glAlphaFuncx;
    glad_debug_glAlphaFuncxOES = glad_glAlphaFuncxOES;
    glad_debug_glBindBuffer = glad_glBindBuffer;
    glad_debug_glBindFramebufferOES = glad_glBindFramebufferOES;
    glad_debug_glBindRenderbufferOES = glad_glBindRenderbufferOES;
    glad_debug_glBindTexture = glad_glBindTexture;
    glad_debug_glBindVertexArrayOES = glad_glBindVertexArrayOES;
    glad_debug_glBlendEquationOES = glad_glBlendEquationOES;
    glad_debug_glBlendEquationSeparateOES = glad_glBlendEquationSeparateOES;
    glad_debug_glBlendFunc = glad_glBlendFunc;
    glad_debug_glBlendFuncSeparateOES = glad_glBlendFuncSeparateOES;
    glad_debug_glBufferData = glad_glBufferData;
    glad_debug_glBufferSubData = glad_glBufferSubData;
    glad_debug_glCheckFramebufferStatusOES = glad_glCheckFramebufferStatusOES;
    glad_debug_glClear = glad_glClear;
    glad_debug_glClearColor = glad_glClearColor;
    glad_debug_glClearColorx = glad_glClearColorx;
    glad_debug_glClearColorxOES = glad_glClearColorxOES;
    glad_debug_glClearDepthf = glad_glClearDepthf;
    glad_debug_glClearDepthfOES = glad_glClearDepthfOES;
    glad_debug_glClearDepthx = glad_glClearDepthx;
    glad_debug_glClearDepthxOES = glad_glClearDepthxOES;
    glad_debug_glClearStencil = glad_glClearStencil;
    glad_debug_glClientActiveTexture = glad_glClientActiveTexture;
    glad_debug_glClientWaitSyncAPPLE = glad_glClientWaitSyncAPPLE;
    glad_debug_glClipPlanef = glad_glClipPlanef;
    glad_debug_glClipPlanefIMG = glad_glClipPlanefIMG;
    glad_debug_glClipPlanefOES = glad_glClipPlanefOES;
    glad_debug_glClipPlanex = glad_glClipPlanex;
    glad_debug_glClipPlanexIMG = glad_glClipPlanexIMG;
    glad_debug_glClipPlanexOES = glad_glClipPlanexOES;
    glad_debug_glColor4f = glad_glColor4f;
    glad_debug_glColor4ub = glad_glColor4ub;
    glad_debug_glColor4x = glad_glColor4x;
    glad_debug_glColor4xOES = glad_glColor4xOES;
    glad_debug_glColorMask = glad_glColorMask;
    glad_debug_glColorPointer = glad_glColorPointer;
    glad_debug_glCompressedTexImage2D = glad_glCompressedTexImage2D;
    glad_debug_glCompressedTexSubImage2D = glad_glCompressedTexSubImage2D;
    glad_debug_glCopyTexImage2D = glad_glCopyTexImage2D;
    glad_debug_glCopyTexSubImage2D = glad_glCopyTexSubImage2D;
    glad_debug_glCopyTextureLevelsAPPLE = glad_glCopyTextureLevelsAPPLE;
    glad_debug_glCullFace = glad_glCullFace;
    glad_debug_glCurrentPaletteMatrixOES = glad_glCurrentPaletteMatrixOES;
    glad_debug_glDeleteBuffers = glad_glDeleteBuffers;
    glad_debug_glDeleteFencesNV = glad_glDeleteFencesNV;
    glad_debug_glDeleteFramebuffersOES = glad_glDeleteFramebuffersOES;
    glad_debug_glDeleteRenderbuffersOES = glad_glDeleteRenderbuffersOES;
    glad_debug_glDeleteSyncAPPLE = glad_glDeleteSyncAPPLE;
    glad_debug_glDeleteTextures = glad_glDeleteTextures;
    glad_debug_glDeleteVertexArraysOES = glad_glDeleteVertexArraysOES;
    glad_debug_glDepthFunc = glad_glDepthFunc;
    glad_debug_glDepthMask = glad_glDepthMask;
    glad_debug_glDepthRangef = glad_glDepthRangef;
    glad_debug_glDepthRangefOES = glad_glDepthRangefOES;
    glad_debug_glDepthRangex = glad_glDepthRangex;
    glad_debug_glDepthRangexOES = glad_glDepthRangexOES;
    glad_debug_glDisable = glad_glDisable;
    glad_debug_glDisableClientState = glad_glDisableClientState;
    glad_debug_glDisableDriverControlQCOM = glad_glDisableDriverControlQCOM;
    glad_debug_glDiscardFramebufferEXT = glad_glDiscardFramebufferEXT;
    glad_debug_glDrawArrays = glad_glDrawArrays;
    glad_debug_glDrawElements = glad_glDrawElements;
    glad_debug_glDrawTexfOES = glad_glDrawTexfOES;
    glad_debug_glDrawTexfvOES = glad_glDrawTexfvOES;
    glad_debug_glDrawTexiOES = glad_glDrawTexiOES;
    glad_debug_glDrawTexivOES = glad_glDrawTexivOES;
    glad_debug_glDrawTexsOES = glad_glDrawTexsOES;
    glad_debug_glDrawTexsvOES = glad_glDrawTexsvOES;
    glad_debug_glDrawTexxOES = glad_glDrawTexxOES;
    glad_debug_glDrawTexxvOES = glad_glDrawTexxvOES;
    glad_debug_glEGLImageTargetRenderbufferStorageOES = glad_glEGLImageTargetRenderbufferStorageOES;
    glad_debug_glEGLImageTargetTexture2DOES = glad_glEGLImageTargetTexture2DOES;
    glad_debug_glEnable = glad_glEnable;
    glad_debug_glEnableClientState = glad_glEnableClientState;
    glad_debug_glEnableDriverControlQCOM = glad_glEnableDriverControlQCOM;
    glad_debug_glEndTilingQCOM = glad_glEndTilingQCOM;
    glad_debug_glExtGetBufferPointervQCOM = glad_glExtGetBufferPointervQCOM;
    glad_debug_glExtGetBuffersQCOM = glad_glExtGetBuffersQCOM;
    glad_debug_glExtGetFramebuffersQCOM = glad_glExtGetFramebuffersQCOM;
    glad_debug_glExtGetProgramBinarySourceQCOM = glad_glExtGetProgramBinarySourceQCOM;
    glad_debug_glExtGetProgramsQCOM = glad_glExtGetProgramsQCOM;
    glad_debug_glExtGetRenderbuffersQCOM = glad_glExtGetRenderbuffersQCOM;
    glad_debug_glExtGetShadersQCOM = glad_glExtGetShadersQCOM;
    glad_debug_glExtGetTexLevelParameterivQCOM = glad_glExtGetTexLevelParameterivQCOM;
    glad_debug_glExtGetTexSubImageQCOM = glad_glExtGetTexSubImageQCOM;
    glad_debug_glExtGetTexturesQCOM = glad_glExtGetTexturesQCOM;
    glad_debug_glExtIsProgramBinaryQCOM = glad_glExtIsProgramBinaryQCOM;
    glad_debug_glExtTexObjectStateOverrideiQCOM = glad_glExtTexObjectStateOverrideiQCOM;
    glad_debug_glFenceSyncAPPLE = glad_glFenceSyncAPPLE;
    glad_debug_glFinish = glad_glFinish;
    glad_debug_glFinishFenceNV = glad_glFinishFenceNV;
    glad_debug_glFlush = glad_glFlush;
    glad_debug_glFlushMappedBufferRangeEXT = glad_glFlushMappedBufferRangeEXT;
    glad_debug_glFogf = glad_glFogf;
    glad_debug_glFogfv = glad_glFogfv;
    glad_debug_glFogx = glad_glFogx;
    glad_debug_glFogxOES = glad_glFogxOES;
    glad_debug_glFogxv = glad_glFogxv;
    glad_debug_glFogxvOES = glad_glFogxvOES;
    glad_debug_glFramebufferRenderbufferOES = glad_glFramebufferRenderbufferOES;
    glad_debug_glFramebufferTexture2DMultisampleEXT = glad_glFramebufferTexture2DMultisampleEXT;
    glad_debug_glFramebufferTexture2DMultisampleIMG = glad_glFramebufferTexture2DMultisampleIMG;
    glad_debug_glFramebufferTexture2DOES = glad_glFramebufferTexture2DOES;
    glad_debug_glFrontFace = glad_glFrontFace;
    glad_debug_glFrustumf = glad_glFrustumf;
    glad_debug_glFrustumfOES = glad_glFrustumfOES;
    glad_debug_glFrustumx = glad_glFrustumx;
    glad_debug_glFrustumxOES = glad_glFrustumxOES;
    glad_debug_glGenBuffers = glad_glGenBuffers;
    glad_debug_glGenFencesNV = glad_glGenFencesNV;
    glad_debug_glGenFramebuffersOES = glad_glGenFramebuffersOES;
    glad_debug_glGenRenderbuffersOES = glad_glGenRenderbuffersOES;
    glad_debug_glGenTextures = glad_glGenTextures;
    glad_debug_glGenVertexArraysOES = glad_glGenVertexArraysOES;
    glad_debug_glGenerateMipmapOES = glad_glGenerateMipmapOES;
    glad_debug_glGetBooleanv = glad_glGetBooleanv;
    glad_debug_glGetBufferParameteriv = glad_glGetBufferParameteriv;
    glad_debug_glGetBufferPointervOES = glad_glGetBufferPointervOES;
    glad_debug_glGetClipPlanef = glad_glGetClipPlanef;
    glad_debug_glGetClipPlanefOES = glad_glGetClipPlanefOES;
    glad_debug_glGetClipPlanex = glad_glGetClipPlanex;
    glad_debug_glGetClipPlanexOES = glad_glGetClipPlanexOES;
    glad_debug_glGetDriverControlStringQCOM = glad_glGetDriverControlStringQCOM;
    glad_debug_glGetDriverControlsQCOM = glad_glGetDriverControlsQCOM;
    glad_debug_glGetError = glad_glGetError;
    glad_debug_glGetFenceivNV = glad_glGetFenceivNV;
    glad_debug_glGetFixedv = glad_glGetFixedv;
    glad_debug_glGetFixedvOES = glad_glGetFixedvOES;
    glad_debug_glGetFloatv = glad_glGetFloatv;
    glad_debug_glGetFramebufferAttachmentParameterivOES = glad_glGetFramebufferAttachmentParameterivOES;
    glad_debug_glGetGraphicsResetStatusEXT = glad_glGetGraphicsResetStatusEXT;
    glad_debug_glGetInteger64vAPPLE = glad_glGetInteger64vAPPLE;
    glad_debug_glGetIntegerv = glad_glGetIntegerv;
    glad_debug_glGetLightfv = glad_glGetLightfv;
    glad_debug_glGetLightxv = glad_glGetLightxv;
    glad_debug_glGetLightxvOES = glad_glGetLightxvOES;
    glad_debug_glGetMaterialfv = glad_glGetMaterialfv;
    glad_debug_glGetMaterialxv = glad_glGetMaterialxv;
    glad_debug_glGetMaterialxvOES = glad_glGetMaterialxvOES;
    glad_debug_glGetPointerv = glad_glGetPointerv;
    glad_debug_glGetRenderbufferParameterivOES = glad_glGetRenderbufferParameterivOES;
    glad_debug_glGetString = glad_glGetString;
    glad_debug_glGetSyncivAPPLE = glad_glGetSyncivAPPLE;
    glad_debug_glGetTexEnvfv = glad_glGetTexEnvfv;
    glad_debug_glGetTexEnviv = glad_glGetTexEnviv;
    glad_debug_glGetTexEnvxv = glad_glGetTexEnvxv;
    glad_debug_glGetTexEnvxvOES = glad_glGetTexEnvxvOES;
    glad_debug_glGetTexGenfvOES = glad_glGetTexGenfvOES;
    glad_debug_glGetTexGenivOES = glad_glGetTexGenivOES;
    glad_debug_glGetTexGenxvOES = glad_glGetTexGenxvOES;
    glad_debug_glGetTexParameterfv = glad_glGetTexParameterfv;
    glad_debug_glGetTexParameteriv = glad_glGetTexParameteriv;
    glad_debug_glGetTexParameterxv = glad_glGetTexParameterxv;
    glad_debug_glGetTexParameterxvOES = glad_glGetTexParameterxvOES;
    glad_debug_glGetnUniformfvEXT = glad_glGetnUniformfvEXT;
    glad_debug_glGetnUniformivEXT = glad_glGetnUniformivEXT;
    glad_debug_glHint = glad_glHint;
    glad_debug_glInsertEventMarkerEXT = glad_glInsertEventMarkerEXT;
    glad_debug_glIsBuffer = glad_glIsBuffer;
    glad_debug_glIsEnabled = glad_glIsEnabled;
    glad_debug_glIsFenceNV = glad_glIsFenceNV;
    glad_debug_glIsFramebufferOES = glad_glIsFramebufferOES;
    glad_debug_glIsRenderbufferOES = glad_glIsRenderbufferOES;
    glad_debug_glIsSyncAPPLE = glad_glIsSyncAPPLE;
    glad_debug_glIsTexture = glad_glIsTexture;
    glad_debug_glIsVertexArrayOES = glad_glIsVertexArrayOES;
    glad_debug_glLightModelf = glad_glLightModelf;
    glad_debug_glLightModelfv = glad_glLightModelfv;
    glad_debug_glLightModelx = glad_glLightModelx;
    glad_debug_glLightModelxOES = glad_glLightModelxOES;
    glad_debug_glLightModelxv = glad_glLightModelxv;
    glad_debug_glLightModelxvOES = glad_glLightModelxvOES;
    glad_debug_glLightf = glad_glLightf;
    glad_debug_glLightfv = glad_glLightfv;
    glad_debug_glLightx = glad_glLightx;
    glad_debug_glLightxOES = glad_glLightxOES;
    glad_debug_glLightxv = glad_glLightxv;
    glad_debug_glLightxvOES = glad_glLightxvOES;
    glad_debug_glLineWidth = glad_glLineWidth;
    glad_debug_glLineWidthx = glad_glLineWidthx;
    glad_debug_glLineWidthxOES = glad_glLineWidthxOES;
    glad_debug_glLoadIdentity = glad_glLoadIdentity;
    glad_debug_glLoadMatrixf = glad_glLoadMatrixf;
    glad_debug_glLoadMatrixx = glad_glLoadMatrixx;
    glad_debug_glLoadMatrixxOES = glad_glLoadMatrixxOES;
    glad_debug_glLoadPaletteFromModelViewMatrixOES = glad_glLoadPaletteFromModelViewMatrixOES;
    glad_debug_glLogicOp = glad_glLogicOp;
    glad_debug_glMapBufferOES = glad_glMapBufferOES;
    glad_debug_glMapBufferRangeEXT = glad_glMapBufferRangeEXT;
    glad_debug_glMaterialf = glad_glMaterialf;
    glad_debug_glMaterialfv = glad_glMaterialfv;
    glad_debug_glMaterialx = glad_glMaterialx;
    glad_debug_glMaterialxOES = glad_glMaterialxOES;
    glad_debug_glMaterialxv = glad_glMaterialxv;
    glad_debug_glMaterialxvOES = glad_glMaterialxvOES;
    glad_debug_glMatrixIndexPointerOES = glad_glMatrixIndexPointerOES;
    glad_debug_glMatrixMode = glad_glMatrixMode;
    glad_debug_glMultMatrixf = glad_glMultMatrixf;
    glad_debug_glMultMatrixx = glad_glMultMatrixx;
    glad_debug_glMultMatrixxOES = glad_glMultMatrixxOES;
    glad_debug_glMultiDrawArraysEXT = glad_glMultiDrawArraysEXT;
    glad_debug_glMultiDrawElementsEXT = glad_glMultiDrawElementsEXT;
    glad_debug_glMultiTexCoord4f = glad_glMultiTexCoord4f;
    glad_debug_glMultiTexCoord4x = glad_glMultiTexCoord4x;
    glad_debug_glMultiTexCoord4xOES = glad_glMultiTexCoord4xOES;
    glad_debug_glNormal3f = glad_glNormal3f;
    glad_debug_glNormal3x = glad_glNormal3x;
    glad_debug_glNormal3xOES = glad_glNormal3xOES;
    glad_debug_glNormalPointer = glad_glNormalPointer;
    glad_debug_glOrthof = glad_glOrthof;
    glad_debug_glOrthofOES = glad_glOrthofOES;
    glad_debug_glOrthox = glad_glOrthox;
    glad_debug_glOrthoxOES = glad_glOrthoxOES;
    glad_debug_glPixelStorei = glad_glPixelStorei;
    glad_debug_glPointParameterf = glad_glPointParameterf;
    glad_debug_glPointParameterfv = glad_glPointParameterfv;
    glad_debug_glPointParameterx = glad_glPointParameterx;
    glad_debug_glPointParameterxOES = glad_glPointParameterxOES;
    glad_debug_glPointParameterxv = glad_glPointParameterxv;
    glad_debug_glPointParameterxvOES = glad_glPointParameterxvOES;
    glad_debug_glPointSize = glad_glPointSize;
    glad_debug_glPointSizePointerOES = glad_glPointSizePointerOES;
    glad_debug_glPointSizex = glad_glPointSizex;
    glad_debug_glPointSizexOES = glad_glPointSizexOES;
    glad_debug_glPolygonOffset = glad_glPolygonOffset;
    glad_debug_glPolygonOffsetx = glad_glPolygonOffsetx;
    glad_debug_glPolygonOffsetxOES = glad_glPolygonOffsetxOES;
    glad_debug_glPopGroupMarkerEXT = glad_glPopGroupMarkerEXT;
    glad_debug_glPopMatrix = glad_glPopMatrix;
    glad_debug_glPushGroupMarkerEXT = glad_glPushGroupMarkerEXT;
    glad_debug_glPushMatrix = glad_glPushMatrix;
    glad_debug_glQueryMatrixxOES = glad_glQueryMatrixxOES;
    glad_debug_glReadPixels = glad_glReadPixels;
    glad_debug_glReadnPixelsEXT = glad_glReadnPixelsEXT;
    glad_debug_glRenderbufferStorageMultisampleAPPLE = glad_glRenderbufferStorageMultisampleAPPLE;
    glad_debug_glRenderbufferStorageMultisampleEXT = glad_glRenderbufferStorageMultisampleEXT;
    glad_debug_glRenderbufferStorageMultisampleIMG = glad_glRenderbufferStorageMultisampleIMG;
    glad_debug_glRenderbufferStorageOES = glad_glRenderbufferStorageOES;
    glad_debug_glResolveMultisampleFramebufferAPPLE = glad_glResolveMultisampleFramebufferAPPLE;
    glad_debug_glRotatef = glad_glRotatef;
    glad_debug_glRotatex = glad_glRotatex;
    glad_debug_glRotatexOES = glad_glRotatexOES;
    glad_debug_glSampleCoverage = glad_glSampleCoverage;
    glad_debug_glSampleCoveragex = glad_glSampleCoveragex;
    glad_debug_glSampleCoveragexOES = glad_glSampleCoveragexOES;
    glad_debug_glScalef = glad_glScalef;
    glad_debug_glScalex = glad_glScalex;
    glad_debug_glScalexOES = glad_glScalexOES;
    glad_debug_glScissor = glad_glScissor;
    glad_debug_glSetFenceNV = glad_glSetFenceNV;
    glad_debug_glShadeModel = glad_glShadeModel;
    glad_debug_glStartTilingQCOM = glad_glStartTilingQCOM;
    glad_debug_glStencilFunc = glad_glStencilFunc;
    glad_debug_glStencilMask = glad_glStencilMask;
    glad_debug_glStencilOp = glad_glStencilOp;
    glad_debug_glTestFenceNV = glad_glTestFenceNV;
    glad_debug_glTexCoordPointer = glad_glTexCoordPointer;
    glad_debug_glTexEnvf = glad_glTexEnvf;
    glad_debug_glTexEnvfv = glad_glTexEnvfv;
    glad_debug_glTexEnvi = glad_glTexEnvi;
    glad_debug_glTexEnviv = glad_glTexEnviv;
    glad_debug_glTexEnvx = glad_glTexEnvx;
    glad_debug_glTexEnvxOES = glad_glTexEnvxOES;
    glad_debug_glTexEnvxv = glad_glTexEnvxv;
    glad_debug_glTexEnvxvOES = glad_glTexEnvxvOES;
    glad_debug_glTexGenfOES = glad_glTexGenfOES;
    glad_debug_glTexGenfvOES = glad_glTexGenfvOES;
    glad_debug_glTexGeniOES = glad_glTexGeniOES;
    glad_debug_glTexGenivOES = glad_glTexGenivOES;
    glad_debug_glTexGenxOES = glad_glTexGenxOES;
    glad_debug_glTexGenxvOES = glad_glTexGenxvOES;
    glad_debug_glTexImage2D = glad_glTexImage2D;
    glad_debug_glTexParameterf = glad_glTexParameterf;
    glad_debug_glTexParameterfv = glad_glTexParameterfv;
    glad_debug_glTexParameteri = glad_glTexParameteri;
    glad_debug_glTexParameteriv = glad_glTexParameteriv;
    glad_debug_glTexParameterx = glad_glTexParameterx;
    glad_debug_glTexParameterxOES = glad_glTexParameterxOES;
    glad_debug_glTexParameterxv = glad_glTexParameterxv;
    glad_debug_glTexParameterxvOES = glad_glTexParameterxvOES;
    glad_debug_glTexStorage1DEXT = glad_glTexStorage1DEXT;
    glad_debug_glTexStorage2DEXT = glad_glTexStorage2DEXT;
    glad_debug_glTexStorage3DEXT = glad_glTexStorage3DEXT;
    glad_debug_glTexSubImage2D = glad_glTexSubImage2D;
    glad_debug_glTextureStorage1DEXT = glad_glTextureStorage1DEXT;
    glad_debug_glTextureStorage2DEXT = glad_glTextureStorage2DEXT;
    glad_debug_glTextureStorage3DEXT = glad_glTextureStorage3DEXT;
    glad_debug_glTranslatef = glad_glTranslatef;
    glad_debug_glTranslatex = glad_glTranslatex;
    glad_debug_glTranslatexOES = glad_glTranslatexOES;
    glad_debug_glUnmapBufferOES = glad_glUnmapBufferOES;
    glad_debug_glVertexPointer = glad_glVertexPointer;
    glad_debug_glViewport = glad_glViewport;
    glad_debug_glWaitSyncAPPLE = glad_glWaitSyncAPPLE;
    glad_debug_glWeightPointerOES = glad_glWeightPointerOES;
}

#ifdef GLAD_GLES1

#ifndef GLAD_LOADER_LIBRARY_C_
#define GLAD_LOADER_LIBRARY_C_

#include <stddef.h>
#include <stdlib.h>

#if GLAD_PLATFORM_WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif


static void* glad_get_dlopen_handle(const char *lib_names[], int length) {
    void *handle = NULL;
    int i;

    for (i = 0; i < length; ++i) {
#if GLAD_PLATFORM_WIN32
  #if GLAD_PLATFORM_UWP
        size_t buffer_size = (strlen(lib_names[i]) + 1) * sizeof(WCHAR);
        LPWSTR buffer = (LPWSTR) malloc(buffer_size);
        if (buffer != NULL) {
            int ret = MultiByteToWideChar(CP_ACP, 0, lib_names[i], -1, buffer, buffer_size);
            if (ret != 0) {
                handle = (void*) LoadPackagedLibrary(buffer, 0);
            }
            free((void*) buffer);
        }
  #else
        handle = (void*) LoadLibraryA(lib_names[i]);
  #endif
#else
        handle = dlopen(lib_names[i], RTLD_LAZY | RTLD_LOCAL);
#endif
        if (handle != NULL) {
            return handle;
        }
    }

    return NULL;
}

static void glad_close_dlopen_handle(void* handle) {
    if (handle != NULL) {
#if GLAD_PLATFORM_WIN32
        FreeLibrary((HMODULE) handle);
#else
        dlclose(handle);
#endif
    }
}

static GLADapiproc glad_dlsym_handle(void* handle, const char *name) {
    if (handle == NULL) {
        return NULL;
    }

#if GLAD_PLATFORM_WIN32
    return (GLADapiproc) GetProcAddress((HMODULE) handle, name);
#else
    return GLAD_GNUC_EXTENSION (GLADapiproc) dlsym(handle, name);
#endif
}

#endif /* GLAD_LOADER_LIBRARY_C_ */

#include <glad/egl.h>

struct _glad_gles1_userptr {
    void *handle;
    PFNEGLGETPROCADDRESSPROC get_proc_address_ptr;
};


static GLADapiproc glad_gles1_get_proc(void *vuserptr, const char* name) {
    struct _glad_gles1_userptr userptr = *(struct _glad_gles1_userptr*) vuserptr;
    GLADapiproc result = NULL;

    result = glad_dlsym_handle(userptr.handle, name);
    if (result == NULL) {
        result = userptr.get_proc_address_ptr(name);
    }

    return result;
}

static void* _glad_GLES1_loader_handle = NULL;

static void* glad_gles1_dlopen_handle(void) {
#if GLAD_PLATFORM_APPLE
    static const char *NAMES[] = {"libGLESv1_CM.dylib"};
#elif GLAD_PLATFORM_WIN32
    static const char *NAMES[] = {"GLESv1_CM.dll", "libGLESv1_CM", "libGLES_CM.dll"};
#else
    static const char *NAMES[] = {"libGLESv1_CM.so.1", "libGLESv1_CM.so", "libGLES_CM.so.1"};
#endif

    if (_glad_GLES1_loader_handle == NULL) {
        _glad_GLES1_loader_handle = glad_get_dlopen_handle(NAMES, sizeof(NAMES) / sizeof(NAMES[0]));
    }

    return _glad_GLES1_loader_handle;
}

static struct _glad_gles1_userptr glad_gles1_build_userptr(void *handle) {
    struct _glad_gles1_userptr userptr;
    userptr.handle = handle;
    userptr.get_proc_address_ptr = eglGetProcAddress;
    return userptr;
}

int gladLoaderLoadGLES1(void) {
    int version = 0;
    void *handle = NULL;
    int did_load = 0;
    struct _glad_gles1_userptr userptr;

    if (eglGetProcAddress == NULL) {
        return 0;
    }

    did_load = _glad_GLES1_loader_handle == NULL;
    handle = glad_gles1_dlopen_handle();
    if (handle != NULL) {
        userptr = glad_gles1_build_userptr(handle);

        version = gladLoadGLES1UserPtr(glad_gles1_get_proc, &userptr);

        if (!version && did_load) {
            gladLoaderUnloadGLES1();
        }
    }

    return version;
}



void gladLoaderUnloadGLES1(void) {
    if (_glad_GLES1_loader_handle != NULL) {
        glad_close_dlopen_handle(_glad_GLES1_loader_handle);
        _glad_GLES1_loader_handle = NULL;
    }
}

#endif /* GLAD_GLES1 */

#ifdef __cplusplus
}
#endif
