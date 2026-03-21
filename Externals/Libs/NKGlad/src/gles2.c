/**
 * SPDX-License-Identifier: (WTFPL OR CC0-1.0) AND Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glad/gles2.h>

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



int GLAD_GL_ES_VERSION_2_0 = 0;
int GLAD_GL_ES_VERSION_3_0 = 0;
int GLAD_GL_ES_VERSION_3_1 = 0;
int GLAD_GL_ES_VERSION_3_2 = 0;
int GLAD_GL_AMD_compressed_3DC_texture = 0;
int GLAD_GL_AMD_compressed_ATC_texture = 0;
int GLAD_GL_AMD_framebuffer_multisample_advanced = 0;
int GLAD_GL_AMD_performance_monitor = 0;
int GLAD_GL_AMD_program_binary_Z400 = 0;
int GLAD_GL_ANDROID_extension_pack_es31a = 0;
int GLAD_GL_ANGLE_depth_texture = 0;
int GLAD_GL_ANGLE_framebuffer_blit = 0;
int GLAD_GL_ANGLE_framebuffer_multisample = 0;
int GLAD_GL_ANGLE_instanced_arrays = 0;
int GLAD_GL_ANGLE_pack_reverse_row_order = 0;
int GLAD_GL_ANGLE_program_binary = 0;
int GLAD_GL_ANGLE_texture_compression_dxt3 = 0;
int GLAD_GL_ANGLE_texture_compression_dxt5 = 0;
int GLAD_GL_ANGLE_texture_usage = 0;
int GLAD_GL_ANGLE_translated_shader_source = 0;
int GLAD_GL_APPLE_clip_distance = 0;
int GLAD_GL_APPLE_color_buffer_packed_float = 0;
int GLAD_GL_APPLE_copy_texture_levels = 0;
int GLAD_GL_APPLE_framebuffer_multisample = 0;
int GLAD_GL_APPLE_rgb_422 = 0;
int GLAD_GL_APPLE_sync = 0;
int GLAD_GL_APPLE_texture_format_BGRA8888 = 0;
int GLAD_GL_APPLE_texture_max_level = 0;
int GLAD_GL_APPLE_texture_packed_float = 0;
int GLAD_GL_ARM_mali_program_binary = 0;
int GLAD_GL_ARM_mali_shader_binary = 0;
int GLAD_GL_ARM_rgba8 = 0;
int GLAD_GL_ARM_shader_core_properties = 0;
int GLAD_GL_ARM_shader_framebuffer_fetch = 0;
int GLAD_GL_ARM_shader_framebuffer_fetch_depth_stencil = 0;
int GLAD_GL_ARM_texture_unnormalized_coordinates = 0;
int GLAD_GL_DMP_program_binary = 0;
int GLAD_GL_DMP_shader_binary = 0;
int GLAD_GL_EXT_EGL_image_array = 0;
int GLAD_GL_EXT_EGL_image_storage = 0;
int GLAD_GL_EXT_EGL_image_storage_compression = 0;
int GLAD_GL_EXT_YUV_target = 0;
int GLAD_GL_EXT_base_instance = 0;
int GLAD_GL_EXT_blend_func_extended = 0;
int GLAD_GL_EXT_blend_minmax = 0;
int GLAD_GL_EXT_buffer_storage = 0;
int GLAD_GL_EXT_clear_texture = 0;
int GLAD_GL_EXT_clip_control = 0;
int GLAD_GL_EXT_clip_cull_distance = 0;
int GLAD_GL_EXT_color_buffer_float = 0;
int GLAD_GL_EXT_color_buffer_half_float = 0;
int GLAD_GL_EXT_conservative_depth = 0;
int GLAD_GL_EXT_copy_image = 0;
int GLAD_GL_EXT_debug_label = 0;
int GLAD_GL_EXT_debug_marker = 0;
int GLAD_GL_EXT_depth_clamp = 0;
int GLAD_GL_EXT_discard_framebuffer = 0;
int GLAD_GL_EXT_disjoint_timer_query = 0;
int GLAD_GL_EXT_draw_buffers = 0;
int GLAD_GL_EXT_draw_buffers_indexed = 0;
int GLAD_GL_EXT_draw_elements_base_vertex = 0;
int GLAD_GL_EXT_draw_instanced = 0;
int GLAD_GL_EXT_draw_transform_feedback = 0;
int GLAD_GL_EXT_external_buffer = 0;
int GLAD_GL_EXT_float_blend = 0;
int GLAD_GL_EXT_fragment_shading_rate = 0;
int GLAD_GL_EXT_framebuffer_blit_layers = 0;
int GLAD_GL_EXT_geometry_point_size = 0;
int GLAD_GL_EXT_geometry_shader = 0;
int GLAD_GL_EXT_gpu_shader5 = 0;
int GLAD_GL_EXT_instanced_arrays = 0;
int GLAD_GL_EXT_map_buffer_range = 0;
int GLAD_GL_EXT_memory_object = 0;
int GLAD_GL_EXT_memory_object_fd = 0;
int GLAD_GL_EXT_memory_object_win32 = 0;
int GLAD_GL_EXT_mesh_shader = 0;
int GLAD_GL_EXT_multi_draw_arrays = 0;
int GLAD_GL_EXT_multi_draw_indirect = 0;
int GLAD_GL_EXT_multisampled_compatibility = 0;
int GLAD_GL_EXT_multisampled_render_to_texture = 0;
int GLAD_GL_EXT_multisampled_render_to_texture2 = 0;
int GLAD_GL_EXT_multiview_draw_buffers = 0;
int GLAD_GL_EXT_multiview_tessellation_geometry_shader = 0;
int GLAD_GL_EXT_multiview_texture_multisample = 0;
int GLAD_GL_EXT_multiview_timer_query = 0;
int GLAD_GL_EXT_occlusion_query_boolean = 0;
int GLAD_GL_EXT_polygon_offset_clamp = 0;
int GLAD_GL_EXT_post_depth_coverage = 0;
int GLAD_GL_EXT_primitive_bounding_box = 0;
int GLAD_GL_EXT_protected_textures = 0;
int GLAD_GL_EXT_pvrtc_sRGB = 0;
int GLAD_GL_EXT_raster_multisample = 0;
int GLAD_GL_EXT_read_format_bgra = 0;
int GLAD_GL_EXT_render_snorm = 0;
int GLAD_GL_EXT_robustness = 0;
int GLAD_GL_EXT_sRGB = 0;
int GLAD_GL_EXT_sRGB_write_control = 0;
int GLAD_GL_EXT_semaphore = 0;
int GLAD_GL_EXT_semaphore_fd = 0;
int GLAD_GL_EXT_semaphore_win32 = 0;
int GLAD_GL_EXT_separate_depth_stencil = 0;
int GLAD_GL_EXT_separate_shader_objects = 0;
int GLAD_GL_EXT_shader_framebuffer_fetch = 0;
int GLAD_GL_EXT_shader_framebuffer_fetch_non_coherent = 0;
int GLAD_GL_EXT_shader_group_vote = 0;
int GLAD_GL_EXT_shader_implicit_conversions = 0;
int GLAD_GL_EXT_shader_integer_mix = 0;
int GLAD_GL_EXT_shader_io_blocks = 0;
int GLAD_GL_EXT_shader_non_constant_global_initializers = 0;
int GLAD_GL_EXT_shader_pixel_local_storage = 0;
int GLAD_GL_EXT_shader_pixel_local_storage2 = 0;
int GLAD_GL_EXT_shader_samples_identical = 0;
int GLAD_GL_EXT_shader_texture_lod = 0;
int GLAD_GL_EXT_shader_texture_samples = 0;
int GLAD_GL_EXT_shadow_samplers = 0;
int GLAD_GL_EXT_sparse_texture = 0;
int GLAD_GL_EXT_sparse_texture2 = 0;
int GLAD_GL_EXT_tessellation_point_size = 0;
int GLAD_GL_EXT_tessellation_shader = 0;
int GLAD_GL_EXT_texture_border_clamp = 0;
int GLAD_GL_EXT_texture_buffer = 0;
int GLAD_GL_EXT_texture_compression_astc_decode_mode = 0;
int GLAD_GL_EXT_texture_compression_bptc = 0;
int GLAD_GL_EXT_texture_compression_dxt1 = 0;
int GLAD_GL_EXT_texture_compression_rgtc = 0;
int GLAD_GL_EXT_texture_compression_s3tc = 0;
int GLAD_GL_EXT_texture_compression_s3tc_srgb = 0;
int GLAD_GL_EXT_texture_cube_map_array = 0;
int GLAD_GL_EXT_texture_filter_anisotropic = 0;
int GLAD_GL_EXT_texture_filter_minmax = 0;
int GLAD_GL_EXT_texture_format_BGRA8888 = 0;
int GLAD_GL_EXT_texture_format_sRGB_override = 0;
int GLAD_GL_EXT_texture_mirror_clamp_to_edge = 0;
int GLAD_GL_EXT_texture_norm16 = 0;
int GLAD_GL_EXT_texture_query_lod = 0;
int GLAD_GL_EXT_texture_rg = 0;
int GLAD_GL_EXT_texture_sRGB_R8 = 0;
int GLAD_GL_EXT_texture_sRGB_RG8 = 0;
int GLAD_GL_EXT_texture_sRGB_decode = 0;
int GLAD_GL_EXT_texture_shadow_lod = 0;
int GLAD_GL_EXT_texture_storage = 0;
int GLAD_GL_EXT_texture_storage_compression = 0;
int GLAD_GL_EXT_texture_type_2_10_10_10_REV = 0;
int GLAD_GL_EXT_texture_view = 0;
int GLAD_GL_EXT_unpack_subimage = 0;
int GLAD_GL_EXT_win32_keyed_mutex = 0;
int GLAD_GL_EXT_window_rectangles = 0;
int GLAD_GL_FJ_shader_binary_GCCSO = 0;
int GLAD_GL_HUAWEI_program_binary = 0;
int GLAD_GL_HUAWEI_shader_binary = 0;
int GLAD_GL_IMG_bindless_texture = 0;
int GLAD_GL_IMG_framebuffer_downsample = 0;
int GLAD_GL_IMG_multisampled_render_to_texture = 0;
int GLAD_GL_IMG_program_binary = 0;
int GLAD_GL_IMG_read_format = 0;
int GLAD_GL_IMG_shader_binary = 0;
int GLAD_GL_IMG_texture_compression_pvrtc = 0;
int GLAD_GL_IMG_texture_compression_pvrtc2 = 0;
int GLAD_GL_IMG_texture_filter_cubic = 0;
int GLAD_GL_INTEL_blackhole_render = 0;
int GLAD_GL_INTEL_conservative_rasterization = 0;
int GLAD_GL_INTEL_framebuffer_CMAA = 0;
int GLAD_GL_INTEL_performance_query = 0;
int GLAD_GL_KHR_blend_equation_advanced = 0;
int GLAD_GL_KHR_blend_equation_advanced_coherent = 0;
int GLAD_GL_KHR_context_flush_control = 0;
int GLAD_GL_KHR_debug = 0;
int GLAD_GL_KHR_no_error = 0;
int GLAD_GL_KHR_parallel_shader_compile = 0;
int GLAD_GL_KHR_robust_buffer_access_behavior = 0;
int GLAD_GL_KHR_robustness = 0;
int GLAD_GL_KHR_shader_subgroup = 0;
int GLAD_GL_KHR_texture_compression_astc_hdr = 0;
int GLAD_GL_KHR_texture_compression_astc_ldr = 0;
int GLAD_GL_KHR_texture_compression_astc_sliced_3d = 0;
int GLAD_GL_MESA_bgra = 0;
int GLAD_GL_MESA_framebuffer_flip_x = 0;
int GLAD_GL_MESA_framebuffer_flip_y = 0;
int GLAD_GL_MESA_framebuffer_swap_xy = 0;
int GLAD_GL_MESA_program_binary_formats = 0;
int GLAD_GL_MESA_sampler_objects = 0;
int GLAD_GL_MESA_shader_integer_functions = 0;
int GLAD_GL_MESA_texture_const_bandwidth = 0;
int GLAD_GL_NVX_blend_equation_advanced_multi_draw_buffers = 0;
int GLAD_GL_NV_bindless_texture = 0;
int GLAD_GL_NV_blend_equation_advanced = 0;
int GLAD_GL_NV_blend_equation_advanced_coherent = 0;
int GLAD_GL_NV_blend_minmax_factor = 0;
int GLAD_GL_NV_clip_space_w_scaling = 0;
int GLAD_GL_NV_compute_shader_derivatives = 0;
int GLAD_GL_NV_conditional_render = 0;
int GLAD_GL_NV_conservative_raster = 0;
int GLAD_GL_NV_conservative_raster_pre_snap = 0;
int GLAD_GL_NV_conservative_raster_pre_snap_triangles = 0;
int GLAD_GL_NV_copy_buffer = 0;
int GLAD_GL_NV_coverage_sample = 0;
int GLAD_GL_NV_depth_nonlinear = 0;
int GLAD_GL_NV_draw_buffers = 0;
int GLAD_GL_NV_draw_instanced = 0;
int GLAD_GL_NV_draw_vulkan_image = 0;
int GLAD_GL_NV_explicit_attrib_location = 0;
int GLAD_GL_NV_fbo_color_attachments = 0;
int GLAD_GL_NV_fence = 0;
int GLAD_GL_NV_fill_rectangle = 0;
int GLAD_GL_NV_fragment_coverage_to_color = 0;
int GLAD_GL_NV_fragment_shader_barycentric = 0;
int GLAD_GL_NV_fragment_shader_interlock = 0;
int GLAD_GL_NV_framebuffer_blit = 0;
int GLAD_GL_NV_framebuffer_mixed_samples = 0;
int GLAD_GL_NV_framebuffer_multisample = 0;
int GLAD_GL_NV_generate_mipmap_sRGB = 0;
int GLAD_GL_NV_geometry_shader_passthrough = 0;
int GLAD_GL_NV_gpu_shader5 = 0;
int GLAD_GL_NV_image_formats = 0;
int GLAD_GL_NV_instanced_arrays = 0;
int GLAD_GL_NV_internalformat_sample_query = 0;
int GLAD_GL_NV_memory_attachment = 0;
int GLAD_GL_NV_memory_object_sparse = 0;
int GLAD_GL_NV_mesh_shader = 0;
int GLAD_GL_NV_non_square_matrices = 0;
int GLAD_GL_NV_pack_subimage = 0;
int GLAD_GL_NV_path_rendering = 0;
int GLAD_GL_NV_path_rendering_shared_edge = 0;
int GLAD_GL_NV_pixel_buffer_object = 0;
int GLAD_GL_NV_polygon_mode = 0;
int GLAD_GL_NV_primitive_shading_rate = 0;
int GLAD_GL_NV_read_buffer = 0;
int GLAD_GL_NV_read_buffer_front = 0;
int GLAD_GL_NV_read_depth = 0;
int GLAD_GL_NV_read_depth_stencil = 0;
int GLAD_GL_NV_read_stencil = 0;
int GLAD_GL_NV_representative_fragment_test = 0;
int GLAD_GL_NV_sRGB_formats = 0;
int GLAD_GL_NV_sample_locations = 0;
int GLAD_GL_NV_sample_mask_override_coverage = 0;
int GLAD_GL_NV_scissor_exclusive = 0;
int GLAD_GL_NV_shader_atomic_fp16_vector = 0;
int GLAD_GL_NV_shader_noperspective_interpolation = 0;
int GLAD_GL_NV_shader_subgroup_partitioned = 0;
int GLAD_GL_NV_shader_texture_footprint = 0;
int GLAD_GL_NV_shading_rate_image = 0;
int GLAD_GL_NV_shadow_samplers_array = 0;
int GLAD_GL_NV_shadow_samplers_cube = 0;
int GLAD_GL_NV_stereo_view_rendering = 0;
int GLAD_GL_NV_texture_barrier = 0;
int GLAD_GL_NV_texture_border_clamp = 0;
int GLAD_GL_NV_texture_compression_s3tc_update = 0;
int GLAD_GL_NV_texture_npot_2D_mipmap = 0;
int GLAD_GL_NV_timeline_semaphore = 0;
int GLAD_GL_NV_viewport_array = 0;
int GLAD_GL_NV_viewport_array2 = 0;
int GLAD_GL_NV_viewport_swizzle = 0;
int GLAD_GL_OES_EGL_image = 0;
int GLAD_GL_OES_EGL_image_external = 0;
int GLAD_GL_OES_EGL_image_external_essl3 = 0;
int GLAD_GL_OES_compressed_ETC1_RGB8_sub_texture = 0;
int GLAD_GL_OES_compressed_ETC1_RGB8_texture = 0;
int GLAD_GL_OES_compressed_paletted_texture = 0;
int GLAD_GL_OES_copy_image = 0;
int GLAD_GL_OES_depth24 = 0;
int GLAD_GL_OES_depth32 = 0;
int GLAD_GL_OES_depth_texture = 0;
int GLAD_GL_OES_draw_buffers_indexed = 0;
int GLAD_GL_OES_draw_elements_base_vertex = 0;
int GLAD_GL_OES_element_index_uint = 0;
int GLAD_GL_OES_fbo_render_mipmap = 0;
int GLAD_GL_OES_fragment_precision_high = 0;
int GLAD_GL_OES_geometry_point_size = 0;
int GLAD_GL_OES_geometry_shader = 0;
int GLAD_GL_OES_get_program_binary = 0;
int GLAD_GL_OES_gpu_shader5 = 0;
int GLAD_GL_OES_mapbuffer = 0;
int GLAD_GL_OES_packed_depth_stencil = 0;
int GLAD_GL_OES_primitive_bounding_box = 0;
int GLAD_GL_OES_required_internalformat = 0;
int GLAD_GL_OES_rgb8_rgba8 = 0;
int GLAD_GL_OES_sample_shading = 0;
int GLAD_GL_OES_sample_variables = 0;
int GLAD_GL_OES_shader_image_atomic = 0;
int GLAD_GL_OES_shader_io_blocks = 0;
int GLAD_GL_OES_shader_multisample_interpolation = 0;
int GLAD_GL_OES_standard_derivatives = 0;
int GLAD_GL_OES_stencil1 = 0;
int GLAD_GL_OES_stencil4 = 0;
int GLAD_GL_OES_surfaceless_context = 0;
int GLAD_GL_OES_tessellation_point_size = 0;
int GLAD_GL_OES_tessellation_shader = 0;
int GLAD_GL_OES_texture_3D = 0;
int GLAD_GL_OES_texture_border_clamp = 0;
int GLAD_GL_OES_texture_buffer = 0;
int GLAD_GL_OES_texture_compression_astc = 0;
int GLAD_GL_OES_texture_cube_map_array = 0;
int GLAD_GL_OES_texture_float = 0;
int GLAD_GL_OES_texture_float_linear = 0;
int GLAD_GL_OES_texture_half_float = 0;
int GLAD_GL_OES_texture_half_float_linear = 0;
int GLAD_GL_OES_texture_npot = 0;
int GLAD_GL_OES_texture_stencil8 = 0;
int GLAD_GL_OES_texture_storage_multisample_2d_array = 0;
int GLAD_GL_OES_texture_view = 0;
int GLAD_GL_OES_vertex_array_object = 0;
int GLAD_GL_OES_vertex_half_float = 0;
int GLAD_GL_OES_vertex_type_10_10_10_2 = 0;
int GLAD_GL_OES_viewport_array = 0;
int GLAD_GL_OVR_multiview = 0;
int GLAD_GL_OVR_multiview2 = 0;
int GLAD_GL_OVR_multiview_multisampled_render_to_texture = 0;
int GLAD_GL_QCOM_YUV_texture_gather = 0;
int GLAD_GL_QCOM_alpha_test = 0;
int GLAD_GL_QCOM_binning_control = 0;
int GLAD_GL_QCOM_driver_control = 0;
int GLAD_GL_QCOM_extended_get = 0;
int GLAD_GL_QCOM_extended_get2 = 0;
int GLAD_GL_QCOM_frame_extrapolation = 0;
int GLAD_GL_QCOM_framebuffer_foveated = 0;
int GLAD_GL_QCOM_motion_estimation = 0;
int GLAD_GL_QCOM_perfmon_global_mode = 0;
int GLAD_GL_QCOM_render_sRGB_R8_RG8 = 0;
int GLAD_GL_QCOM_render_shared_exponent = 0;
int GLAD_GL_QCOM_shader_framebuffer_fetch_noncoherent = 0;
int GLAD_GL_QCOM_shader_framebuffer_fetch_rate = 0;
int GLAD_GL_QCOM_shading_rate = 0;
int GLAD_GL_QCOM_texture_foveated = 0;
int GLAD_GL_QCOM_texture_foveated2 = 0;
int GLAD_GL_QCOM_texture_foveated_subsampled_layout = 0;
int GLAD_GL_QCOM_texture_lod_bias = 0;
int GLAD_GL_QCOM_tiled_rendering = 0;
int GLAD_GL_QCOM_writeonly_rendering = 0;
int GLAD_GL_QCOM_ycbcr_degamma = 0;
int GLAD_GL_VIV_shader_binary = 0;


static void _pre_call_gles2_callback_default(const char *name, GLADapiproc apiproc, int len_args, ...) {
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
static void _post_call_gles2_callback_default(void *ret, const char *name, GLADapiproc apiproc, int len_args, ...) {
    GLenum error_code;

    GLAD_UNUSED(ret);
    GLAD_UNUSED(apiproc);
    GLAD_UNUSED(len_args);

    error_code = glad_glGetError();

    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "GLAD: ERROR %d in %s!\n", error_code, name);
    }
}

static GLADprecallback _pre_call_gles2_callback = _pre_call_gles2_callback_default;
void gladSetGLES2PreCallback(GLADprecallback cb) {
    _pre_call_gles2_callback = cb;
}
static GLADpostcallback _post_call_gles2_callback = _post_call_gles2_callback_default;
void gladSetGLES2PostCallback(GLADpostcallback cb) {
    _post_call_gles2_callback = cb;
}

PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC glad_glAcquireKeyedMutexWin32EXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glAcquireKeyedMutexWin32EXT(GLuint memory, GLuint64 key, GLuint timeout) {
    GLboolean ret;
    _pre_call_gles2_callback("glAcquireKeyedMutexWin32EXT", (GLADapiproc) glad_glAcquireKeyedMutexWin32EXT, 3, memory, key, timeout);
    ret = glad_glAcquireKeyedMutexWin32EXT(memory, key, timeout);
    _post_call_gles2_callback((void*) &ret, "glAcquireKeyedMutexWin32EXT", (GLADapiproc) glad_glAcquireKeyedMutexWin32EXT, 3, memory, key, timeout);
    return ret;
}
PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC glad_debug_glAcquireKeyedMutexWin32EXT = glad_debug_impl_glAcquireKeyedMutexWin32EXT;
PFNGLACTIVESHADERPROGRAMPROC glad_glActiveShaderProgram = NULL;
static void GLAD_API_PTR glad_debug_impl_glActiveShaderProgram(GLuint pipeline, GLuint program) {
    _pre_call_gles2_callback("glActiveShaderProgram", (GLADapiproc) glad_glActiveShaderProgram, 2, pipeline, program);
    glad_glActiveShaderProgram(pipeline, program);
    _post_call_gles2_callback(NULL, "glActiveShaderProgram", (GLADapiproc) glad_glActiveShaderProgram, 2, pipeline, program);
    
}
PFNGLACTIVESHADERPROGRAMPROC glad_debug_glActiveShaderProgram = glad_debug_impl_glActiveShaderProgram;
PFNGLACTIVESHADERPROGRAMEXTPROC glad_glActiveShaderProgramEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glActiveShaderProgramEXT(GLuint pipeline, GLuint program) {
    _pre_call_gles2_callback("glActiveShaderProgramEXT", (GLADapiproc) glad_glActiveShaderProgramEXT, 2, pipeline, program);
    glad_glActiveShaderProgramEXT(pipeline, program);
    _post_call_gles2_callback(NULL, "glActiveShaderProgramEXT", (GLADapiproc) glad_glActiveShaderProgramEXT, 2, pipeline, program);
    
}
PFNGLACTIVESHADERPROGRAMEXTPROC glad_debug_glActiveShaderProgramEXT = glad_debug_impl_glActiveShaderProgramEXT;
PFNGLACTIVETEXTUREPROC glad_glActiveTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glActiveTexture(GLenum texture) {
    _pre_call_gles2_callback("glActiveTexture", (GLADapiproc) glad_glActiveTexture, 1, texture);
    glad_glActiveTexture(texture);
    _post_call_gles2_callback(NULL, "glActiveTexture", (GLADapiproc) glad_glActiveTexture, 1, texture);
    
}
PFNGLACTIVETEXTUREPROC glad_debug_glActiveTexture = glad_debug_impl_glActiveTexture;
PFNGLALPHAFUNCQCOMPROC glad_glAlphaFuncQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glAlphaFuncQCOM(GLenum func, GLclampf ref) {
    _pre_call_gles2_callback("glAlphaFuncQCOM", (GLADapiproc) glad_glAlphaFuncQCOM, 2, func, ref);
    glad_glAlphaFuncQCOM(func, ref);
    _post_call_gles2_callback(NULL, "glAlphaFuncQCOM", (GLADapiproc) glad_glAlphaFuncQCOM, 2, func, ref);
    
}
PFNGLALPHAFUNCQCOMPROC glad_debug_glAlphaFuncQCOM = glad_debug_impl_glAlphaFuncQCOM;
PFNGLAPPLYFRAMEBUFFERATTACHMENTCMAAINTELPROC glad_glApplyFramebufferAttachmentCMAAINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glApplyFramebufferAttachmentCMAAINTEL(void) {
    _pre_call_gles2_callback("glApplyFramebufferAttachmentCMAAINTEL", (GLADapiproc) glad_glApplyFramebufferAttachmentCMAAINTEL, 0);
    glad_glApplyFramebufferAttachmentCMAAINTEL();
    _post_call_gles2_callback(NULL, "glApplyFramebufferAttachmentCMAAINTEL", (GLADapiproc) glad_glApplyFramebufferAttachmentCMAAINTEL, 0);
    
}
PFNGLAPPLYFRAMEBUFFERATTACHMENTCMAAINTELPROC glad_debug_glApplyFramebufferAttachmentCMAAINTEL = glad_debug_impl_glApplyFramebufferAttachmentCMAAINTEL;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
static void GLAD_API_PTR glad_debug_impl_glAttachShader(GLuint program, GLuint shader) {
    _pre_call_gles2_callback("glAttachShader", (GLADapiproc) glad_glAttachShader, 2, program, shader);
    glad_glAttachShader(program, shader);
    _post_call_gles2_callback(NULL, "glAttachShader", (GLADapiproc) glad_glAttachShader, 2, program, shader);
    
}
PFNGLATTACHSHADERPROC glad_debug_glAttachShader = glad_debug_impl_glAttachShader;
PFNGLBEGINCONDITIONALRENDERNVPROC glad_glBeginConditionalRenderNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginConditionalRenderNV(GLuint id, GLenum mode) {
    _pre_call_gles2_callback("glBeginConditionalRenderNV", (GLADapiproc) glad_glBeginConditionalRenderNV, 2, id, mode);
    glad_glBeginConditionalRenderNV(id, mode);
    _post_call_gles2_callback(NULL, "glBeginConditionalRenderNV", (GLADapiproc) glad_glBeginConditionalRenderNV, 2, id, mode);
    
}
PFNGLBEGINCONDITIONALRENDERNVPROC glad_debug_glBeginConditionalRenderNV = glad_debug_impl_glBeginConditionalRenderNV;
PFNGLBEGINPERFMONITORAMDPROC glad_glBeginPerfMonitorAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginPerfMonitorAMD(GLuint monitor) {
    _pre_call_gles2_callback("glBeginPerfMonitorAMD", (GLADapiproc) glad_glBeginPerfMonitorAMD, 1, monitor);
    glad_glBeginPerfMonitorAMD(monitor);
    _post_call_gles2_callback(NULL, "glBeginPerfMonitorAMD", (GLADapiproc) glad_glBeginPerfMonitorAMD, 1, monitor);
    
}
PFNGLBEGINPERFMONITORAMDPROC glad_debug_glBeginPerfMonitorAMD = glad_debug_impl_glBeginPerfMonitorAMD;
PFNGLBEGINPERFQUERYINTELPROC glad_glBeginPerfQueryINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginPerfQueryINTEL(GLuint queryHandle) {
    _pre_call_gles2_callback("glBeginPerfQueryINTEL", (GLADapiproc) glad_glBeginPerfQueryINTEL, 1, queryHandle);
    glad_glBeginPerfQueryINTEL(queryHandle);
    _post_call_gles2_callback(NULL, "glBeginPerfQueryINTEL", (GLADapiproc) glad_glBeginPerfQueryINTEL, 1, queryHandle);
    
}
PFNGLBEGINPERFQUERYINTELPROC glad_debug_glBeginPerfQueryINTEL = glad_debug_impl_glBeginPerfQueryINTEL;
PFNGLBEGINQUERYPROC glad_glBeginQuery = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginQuery(GLenum target, GLuint id) {
    _pre_call_gles2_callback("glBeginQuery", (GLADapiproc) glad_glBeginQuery, 2, target, id);
    glad_glBeginQuery(target, id);
    _post_call_gles2_callback(NULL, "glBeginQuery", (GLADapiproc) glad_glBeginQuery, 2, target, id);
    
}
PFNGLBEGINQUERYPROC glad_debug_glBeginQuery = glad_debug_impl_glBeginQuery;
PFNGLBEGINQUERYEXTPROC glad_glBeginQueryEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginQueryEXT(GLenum target, GLuint id) {
    _pre_call_gles2_callback("glBeginQueryEXT", (GLADapiproc) glad_glBeginQueryEXT, 2, target, id);
    glad_glBeginQueryEXT(target, id);
    _post_call_gles2_callback(NULL, "glBeginQueryEXT", (GLADapiproc) glad_glBeginQueryEXT, 2, target, id);
    
}
PFNGLBEGINQUERYEXTPROC glad_debug_glBeginQueryEXT = glad_debug_impl_glBeginQueryEXT;
PFNGLBEGINTRANSFORMFEEDBACKPROC glad_glBeginTransformFeedback = NULL;
static void GLAD_API_PTR glad_debug_impl_glBeginTransformFeedback(GLenum primitiveMode) {
    _pre_call_gles2_callback("glBeginTransformFeedback", (GLADapiproc) glad_glBeginTransformFeedback, 1, primitiveMode);
    glad_glBeginTransformFeedback(primitiveMode);
    _post_call_gles2_callback(NULL, "glBeginTransformFeedback", (GLADapiproc) glad_glBeginTransformFeedback, 1, primitiveMode);
    
}
PFNGLBEGINTRANSFORMFEEDBACKPROC glad_debug_glBeginTransformFeedback = glad_debug_impl_glBeginTransformFeedback;
PFNGLBINDATTRIBLOCATIONPROC glad_glBindAttribLocation = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindAttribLocation(GLuint program, GLuint index, const GLchar * name) {
    _pre_call_gles2_callback("glBindAttribLocation", (GLADapiproc) glad_glBindAttribLocation, 3, program, index, name);
    glad_glBindAttribLocation(program, index, name);
    _post_call_gles2_callback(NULL, "glBindAttribLocation", (GLADapiproc) glad_glBindAttribLocation, 3, program, index, name);
    
}
PFNGLBINDATTRIBLOCATIONPROC glad_debug_glBindAttribLocation = glad_debug_impl_glBindAttribLocation;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBuffer(GLenum target, GLuint buffer) {
    _pre_call_gles2_callback("glBindBuffer", (GLADapiproc) glad_glBindBuffer, 2, target, buffer);
    glad_glBindBuffer(target, buffer);
    _post_call_gles2_callback(NULL, "glBindBuffer", (GLADapiproc) glad_glBindBuffer, 2, target, buffer);
    
}
PFNGLBINDBUFFERPROC glad_debug_glBindBuffer = glad_debug_impl_glBindBuffer;
PFNGLBINDBUFFERBASEPROC glad_glBindBufferBase = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    _pre_call_gles2_callback("glBindBufferBase", (GLADapiproc) glad_glBindBufferBase, 3, target, index, buffer);
    glad_glBindBufferBase(target, index, buffer);
    _post_call_gles2_callback(NULL, "glBindBufferBase", (GLADapiproc) glad_glBindBufferBase, 3, target, index, buffer);
    
}
PFNGLBINDBUFFERBASEPROC glad_debug_glBindBufferBase = glad_debug_impl_glBindBufferBase;
PFNGLBINDBUFFERRANGEPROC glad_glBindBufferRange = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    _pre_call_gles2_callback("glBindBufferRange", (GLADapiproc) glad_glBindBufferRange, 5, target, index, buffer, offset, size);
    glad_glBindBufferRange(target, index, buffer, offset, size);
    _post_call_gles2_callback(NULL, "glBindBufferRange", (GLADapiproc) glad_glBindBufferRange, 5, target, index, buffer, offset, size);
    
}
PFNGLBINDBUFFERRANGEPROC glad_debug_glBindBufferRange = glad_debug_impl_glBindBufferRange;
PFNGLBINDFRAGDATALOCATIONEXTPROC glad_glBindFragDataLocationEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindFragDataLocationEXT(GLuint program, GLuint color, const GLchar * name) {
    _pre_call_gles2_callback("glBindFragDataLocationEXT", (GLADapiproc) glad_glBindFragDataLocationEXT, 3, program, color, name);
    glad_glBindFragDataLocationEXT(program, color, name);
    _post_call_gles2_callback(NULL, "glBindFragDataLocationEXT", (GLADapiproc) glad_glBindFragDataLocationEXT, 3, program, color, name);
    
}
PFNGLBINDFRAGDATALOCATIONEXTPROC glad_debug_glBindFragDataLocationEXT = glad_debug_impl_glBindFragDataLocationEXT;
PFNGLBINDFRAGDATALOCATIONINDEXEDEXTPROC glad_glBindFragDataLocationIndexedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindFragDataLocationIndexedEXT(GLuint program, GLuint colorNumber, GLuint index, const GLchar * name) {
    _pre_call_gles2_callback("glBindFragDataLocationIndexedEXT", (GLADapiproc) glad_glBindFragDataLocationIndexedEXT, 4, program, colorNumber, index, name);
    glad_glBindFragDataLocationIndexedEXT(program, colorNumber, index, name);
    _post_call_gles2_callback(NULL, "glBindFragDataLocationIndexedEXT", (GLADapiproc) glad_glBindFragDataLocationIndexedEXT, 4, program, colorNumber, index, name);
    
}
PFNGLBINDFRAGDATALOCATIONINDEXEDEXTPROC glad_debug_glBindFragDataLocationIndexedEXT = glad_debug_impl_glBindFragDataLocationIndexedEXT;
PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindFramebuffer(GLenum target, GLuint framebuffer) {
    _pre_call_gles2_callback("glBindFramebuffer", (GLADapiproc) glad_glBindFramebuffer, 2, target, framebuffer);
    glad_glBindFramebuffer(target, framebuffer);
    _post_call_gles2_callback(NULL, "glBindFramebuffer", (GLADapiproc) glad_glBindFramebuffer, 2, target, framebuffer);
    
}
PFNGLBINDFRAMEBUFFERPROC glad_debug_glBindFramebuffer = glad_debug_impl_glBindFramebuffer;
PFNGLBINDIMAGETEXTUREPROC glad_glBindImageTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format) {
    _pre_call_gles2_callback("glBindImageTexture", (GLADapiproc) glad_glBindImageTexture, 7, unit, texture, level, layered, layer, access, format);
    glad_glBindImageTexture(unit, texture, level, layered, layer, access, format);
    _post_call_gles2_callback(NULL, "glBindImageTexture", (GLADapiproc) glad_glBindImageTexture, 7, unit, texture, level, layered, layer, access, format);
    
}
PFNGLBINDIMAGETEXTUREPROC glad_debug_glBindImageTexture = glad_debug_impl_glBindImageTexture;
PFNGLBINDPROGRAMPIPELINEPROC glad_glBindProgramPipeline = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindProgramPipeline(GLuint pipeline) {
    _pre_call_gles2_callback("glBindProgramPipeline", (GLADapiproc) glad_glBindProgramPipeline, 1, pipeline);
    glad_glBindProgramPipeline(pipeline);
    _post_call_gles2_callback(NULL, "glBindProgramPipeline", (GLADapiproc) glad_glBindProgramPipeline, 1, pipeline);
    
}
PFNGLBINDPROGRAMPIPELINEPROC glad_debug_glBindProgramPipeline = glad_debug_impl_glBindProgramPipeline;
PFNGLBINDPROGRAMPIPELINEEXTPROC glad_glBindProgramPipelineEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindProgramPipelineEXT(GLuint pipeline) {
    _pre_call_gles2_callback("glBindProgramPipelineEXT", (GLADapiproc) glad_glBindProgramPipelineEXT, 1, pipeline);
    glad_glBindProgramPipelineEXT(pipeline);
    _post_call_gles2_callback(NULL, "glBindProgramPipelineEXT", (GLADapiproc) glad_glBindProgramPipelineEXT, 1, pipeline);
    
}
PFNGLBINDPROGRAMPIPELINEEXTPROC glad_debug_glBindProgramPipelineEXT = glad_debug_impl_glBindProgramPipelineEXT;
PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    _pre_call_gles2_callback("glBindRenderbuffer", (GLADapiproc) glad_glBindRenderbuffer, 2, target, renderbuffer);
    glad_glBindRenderbuffer(target, renderbuffer);
    _post_call_gles2_callback(NULL, "glBindRenderbuffer", (GLADapiproc) glad_glBindRenderbuffer, 2, target, renderbuffer);
    
}
PFNGLBINDRENDERBUFFERPROC glad_debug_glBindRenderbuffer = glad_debug_impl_glBindRenderbuffer;
PFNGLBINDSAMPLERPROC glad_glBindSampler = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindSampler(GLuint unit, GLuint sampler) {
    _pre_call_gles2_callback("glBindSampler", (GLADapiproc) glad_glBindSampler, 2, unit, sampler);
    glad_glBindSampler(unit, sampler);
    _post_call_gles2_callback(NULL, "glBindSampler", (GLADapiproc) glad_glBindSampler, 2, unit, sampler);
    
}
PFNGLBINDSAMPLERPROC glad_debug_glBindSampler = glad_debug_impl_glBindSampler;
PFNGLBINDSHADINGRATEIMAGENVPROC glad_glBindShadingRateImageNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindShadingRateImageNV(GLuint texture) {
    _pre_call_gles2_callback("glBindShadingRateImageNV", (GLADapiproc) glad_glBindShadingRateImageNV, 1, texture);
    glad_glBindShadingRateImageNV(texture);
    _post_call_gles2_callback(NULL, "glBindShadingRateImageNV", (GLADapiproc) glad_glBindShadingRateImageNV, 1, texture);
    
}
PFNGLBINDSHADINGRATEIMAGENVPROC glad_debug_glBindShadingRateImageNV = glad_debug_impl_glBindShadingRateImageNV;
PFNGLBINDTEXTUREPROC glad_glBindTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindTexture(GLenum target, GLuint texture) {
    _pre_call_gles2_callback("glBindTexture", (GLADapiproc) glad_glBindTexture, 2, target, texture);
    glad_glBindTexture(target, texture);
    _post_call_gles2_callback(NULL, "glBindTexture", (GLADapiproc) glad_glBindTexture, 2, target, texture);
    
}
PFNGLBINDTEXTUREPROC glad_debug_glBindTexture = glad_debug_impl_glBindTexture;
PFNGLBINDTRANSFORMFEEDBACKPROC glad_glBindTransformFeedback = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindTransformFeedback(GLenum target, GLuint id) {
    _pre_call_gles2_callback("glBindTransformFeedback", (GLADapiproc) glad_glBindTransformFeedback, 2, target, id);
    glad_glBindTransformFeedback(target, id);
    _post_call_gles2_callback(NULL, "glBindTransformFeedback", (GLADapiproc) glad_glBindTransformFeedback, 2, target, id);
    
}
PFNGLBINDTRANSFORMFEEDBACKPROC glad_debug_glBindTransformFeedback = glad_debug_impl_glBindTransformFeedback;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindVertexArray(GLuint array) {
    _pre_call_gles2_callback("glBindVertexArray", (GLADapiproc) glad_glBindVertexArray, 1, array);
    glad_glBindVertexArray(array);
    _post_call_gles2_callback(NULL, "glBindVertexArray", (GLADapiproc) glad_glBindVertexArray, 1, array);
    
}
PFNGLBINDVERTEXARRAYPROC glad_debug_glBindVertexArray = glad_debug_impl_glBindVertexArray;
PFNGLBINDVERTEXARRAYOESPROC glad_glBindVertexArrayOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindVertexArrayOES(GLuint array) {
    _pre_call_gles2_callback("glBindVertexArrayOES", (GLADapiproc) glad_glBindVertexArrayOES, 1, array);
    glad_glBindVertexArrayOES(array);
    _post_call_gles2_callback(NULL, "glBindVertexArrayOES", (GLADapiproc) glad_glBindVertexArrayOES, 1, array);
    
}
PFNGLBINDVERTEXARRAYOESPROC glad_debug_glBindVertexArrayOES = glad_debug_impl_glBindVertexArrayOES;
PFNGLBINDVERTEXBUFFERPROC glad_glBindVertexBuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBindVertexBuffer(GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    _pre_call_gles2_callback("glBindVertexBuffer", (GLADapiproc) glad_glBindVertexBuffer, 4, bindingindex, buffer, offset, stride);
    glad_glBindVertexBuffer(bindingindex, buffer, offset, stride);
    _post_call_gles2_callback(NULL, "glBindVertexBuffer", (GLADapiproc) glad_glBindVertexBuffer, 4, bindingindex, buffer, offset, stride);
    
}
PFNGLBINDVERTEXBUFFERPROC glad_debug_glBindVertexBuffer = glad_debug_impl_glBindVertexBuffer;
PFNGLBLENDBARRIERPROC glad_glBlendBarrier = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendBarrier(void) {
    _pre_call_gles2_callback("glBlendBarrier", (GLADapiproc) glad_glBlendBarrier, 0);
    glad_glBlendBarrier();
    _post_call_gles2_callback(NULL, "glBlendBarrier", (GLADapiproc) glad_glBlendBarrier, 0);
    
}
PFNGLBLENDBARRIERPROC glad_debug_glBlendBarrier = glad_debug_impl_glBlendBarrier;
PFNGLBLENDBARRIERKHRPROC glad_glBlendBarrierKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendBarrierKHR(void) {
    _pre_call_gles2_callback("glBlendBarrierKHR", (GLADapiproc) glad_glBlendBarrierKHR, 0);
    glad_glBlendBarrierKHR();
    _post_call_gles2_callback(NULL, "glBlendBarrierKHR", (GLADapiproc) glad_glBlendBarrierKHR, 0);
    
}
PFNGLBLENDBARRIERKHRPROC glad_debug_glBlendBarrierKHR = glad_debug_impl_glBlendBarrierKHR;
PFNGLBLENDBARRIERNVPROC glad_glBlendBarrierNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendBarrierNV(void) {
    _pre_call_gles2_callback("glBlendBarrierNV", (GLADapiproc) glad_glBlendBarrierNV, 0);
    glad_glBlendBarrierNV();
    _post_call_gles2_callback(NULL, "glBlendBarrierNV", (GLADapiproc) glad_glBlendBarrierNV, 0);
    
}
PFNGLBLENDBARRIERNVPROC glad_debug_glBlendBarrierNV = glad_debug_impl_glBlendBarrierNV;
PFNGLBLENDCOLORPROC glad_glBlendColor = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    _pre_call_gles2_callback("glBlendColor", (GLADapiproc) glad_glBlendColor, 4, red, green, blue, alpha);
    glad_glBlendColor(red, green, blue, alpha);
    _post_call_gles2_callback(NULL, "glBlendColor", (GLADapiproc) glad_glBlendColor, 4, red, green, blue, alpha);
    
}
PFNGLBLENDCOLORPROC glad_debug_glBlendColor = glad_debug_impl_glBlendColor;
PFNGLBLENDEQUATIONPROC glad_glBlendEquation = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquation(GLenum mode) {
    _pre_call_gles2_callback("glBlendEquation", (GLADapiproc) glad_glBlendEquation, 1, mode);
    glad_glBlendEquation(mode);
    _post_call_gles2_callback(NULL, "glBlendEquation", (GLADapiproc) glad_glBlendEquation, 1, mode);
    
}
PFNGLBLENDEQUATIONPROC glad_debug_glBlendEquation = glad_debug_impl_glBlendEquation;
PFNGLBLENDEQUATIONSEPARATEPROC glad_glBlendEquationSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    _pre_call_gles2_callback("glBlendEquationSeparate", (GLADapiproc) glad_glBlendEquationSeparate, 2, modeRGB, modeAlpha);
    glad_glBlendEquationSeparate(modeRGB, modeAlpha);
    _post_call_gles2_callback(NULL, "glBlendEquationSeparate", (GLADapiproc) glad_glBlendEquationSeparate, 2, modeRGB, modeAlpha);
    
}
PFNGLBLENDEQUATIONSEPARATEPROC glad_debug_glBlendEquationSeparate = glad_debug_impl_glBlendEquationSeparate;
PFNGLBLENDEQUATIONSEPARATEIPROC glad_glBlendEquationSeparatei = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationSeparatei(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    _pre_call_gles2_callback("glBlendEquationSeparatei", (GLADapiproc) glad_glBlendEquationSeparatei, 3, buf, modeRGB, modeAlpha);
    glad_glBlendEquationSeparatei(buf, modeRGB, modeAlpha);
    _post_call_gles2_callback(NULL, "glBlendEquationSeparatei", (GLADapiproc) glad_glBlendEquationSeparatei, 3, buf, modeRGB, modeAlpha);
    
}
PFNGLBLENDEQUATIONSEPARATEIPROC glad_debug_glBlendEquationSeparatei = glad_debug_impl_glBlendEquationSeparatei;
PFNGLBLENDEQUATIONSEPARATEIEXTPROC glad_glBlendEquationSeparateiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationSeparateiEXT(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    _pre_call_gles2_callback("glBlendEquationSeparateiEXT", (GLADapiproc) glad_glBlendEquationSeparateiEXT, 3, buf, modeRGB, modeAlpha);
    glad_glBlendEquationSeparateiEXT(buf, modeRGB, modeAlpha);
    _post_call_gles2_callback(NULL, "glBlendEquationSeparateiEXT", (GLADapiproc) glad_glBlendEquationSeparateiEXT, 3, buf, modeRGB, modeAlpha);
    
}
PFNGLBLENDEQUATIONSEPARATEIEXTPROC glad_debug_glBlendEquationSeparateiEXT = glad_debug_impl_glBlendEquationSeparateiEXT;
PFNGLBLENDEQUATIONSEPARATEIOESPROC glad_glBlendEquationSeparateiOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationSeparateiOES(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
    _pre_call_gles2_callback("glBlendEquationSeparateiOES", (GLADapiproc) glad_glBlendEquationSeparateiOES, 3, buf, modeRGB, modeAlpha);
    glad_glBlendEquationSeparateiOES(buf, modeRGB, modeAlpha);
    _post_call_gles2_callback(NULL, "glBlendEquationSeparateiOES", (GLADapiproc) glad_glBlendEquationSeparateiOES, 3, buf, modeRGB, modeAlpha);
    
}
PFNGLBLENDEQUATIONSEPARATEIOESPROC glad_debug_glBlendEquationSeparateiOES = glad_debug_impl_glBlendEquationSeparateiOES;
PFNGLBLENDEQUATIONIPROC glad_glBlendEquationi = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationi(GLuint buf, GLenum mode) {
    _pre_call_gles2_callback("glBlendEquationi", (GLADapiproc) glad_glBlendEquationi, 2, buf, mode);
    glad_glBlendEquationi(buf, mode);
    _post_call_gles2_callback(NULL, "glBlendEquationi", (GLADapiproc) glad_glBlendEquationi, 2, buf, mode);
    
}
PFNGLBLENDEQUATIONIPROC glad_debug_glBlendEquationi = glad_debug_impl_glBlendEquationi;
PFNGLBLENDEQUATIONIEXTPROC glad_glBlendEquationiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationiEXT(GLuint buf, GLenum mode) {
    _pre_call_gles2_callback("glBlendEquationiEXT", (GLADapiproc) glad_glBlendEquationiEXT, 2, buf, mode);
    glad_glBlendEquationiEXT(buf, mode);
    _post_call_gles2_callback(NULL, "glBlendEquationiEXT", (GLADapiproc) glad_glBlendEquationiEXT, 2, buf, mode);
    
}
PFNGLBLENDEQUATIONIEXTPROC glad_debug_glBlendEquationiEXT = glad_debug_impl_glBlendEquationiEXT;
PFNGLBLENDEQUATIONIOESPROC glad_glBlendEquationiOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendEquationiOES(GLuint buf, GLenum mode) {
    _pre_call_gles2_callback("glBlendEquationiOES", (GLADapiproc) glad_glBlendEquationiOES, 2, buf, mode);
    glad_glBlendEquationiOES(buf, mode);
    _post_call_gles2_callback(NULL, "glBlendEquationiOES", (GLADapiproc) glad_glBlendEquationiOES, 2, buf, mode);
    
}
PFNGLBLENDEQUATIONIOESPROC glad_debug_glBlendEquationiOES = glad_debug_impl_glBlendEquationiOES;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFunc(GLenum sfactor, GLenum dfactor) {
    _pre_call_gles2_callback("glBlendFunc", (GLADapiproc) glad_glBlendFunc, 2, sfactor, dfactor);
    glad_glBlendFunc(sfactor, dfactor);
    _post_call_gles2_callback(NULL, "glBlendFunc", (GLADapiproc) glad_glBlendFunc, 2, sfactor, dfactor);
    
}
PFNGLBLENDFUNCPROC glad_debug_glBlendFunc = glad_debug_impl_glBlendFunc;
PFNGLBLENDFUNCSEPARATEPROC glad_glBlendFuncSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    _pre_call_gles2_callback("glBlendFuncSeparate", (GLADapiproc) glad_glBlendFuncSeparate, 4, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    glad_glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    _post_call_gles2_callback(NULL, "glBlendFuncSeparate", (GLADapiproc) glad_glBlendFuncSeparate, 4, sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
    
}
PFNGLBLENDFUNCSEPARATEPROC glad_debug_glBlendFuncSeparate = glad_debug_impl_glBlendFuncSeparate;
PFNGLBLENDFUNCSEPARATEIPROC glad_glBlendFuncSeparatei = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFuncSeparatei(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    _pre_call_gles2_callback("glBlendFuncSeparatei", (GLADapiproc) glad_glBlendFuncSeparatei, 5, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
    glad_glBlendFuncSeparatei(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
    _post_call_gles2_callback(NULL, "glBlendFuncSeparatei", (GLADapiproc) glad_glBlendFuncSeparatei, 5, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
    
}
PFNGLBLENDFUNCSEPARATEIPROC glad_debug_glBlendFuncSeparatei = glad_debug_impl_glBlendFuncSeparatei;
PFNGLBLENDFUNCSEPARATEIEXTPROC glad_glBlendFuncSeparateiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFuncSeparateiEXT(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    _pre_call_gles2_callback("glBlendFuncSeparateiEXT", (GLADapiproc) glad_glBlendFuncSeparateiEXT, 5, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
    glad_glBlendFuncSeparateiEXT(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
    _post_call_gles2_callback(NULL, "glBlendFuncSeparateiEXT", (GLADapiproc) glad_glBlendFuncSeparateiEXT, 5, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
    
}
PFNGLBLENDFUNCSEPARATEIEXTPROC glad_debug_glBlendFuncSeparateiEXT = glad_debug_impl_glBlendFuncSeparateiEXT;
PFNGLBLENDFUNCSEPARATEIOESPROC glad_glBlendFuncSeparateiOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFuncSeparateiOES(GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    _pre_call_gles2_callback("glBlendFuncSeparateiOES", (GLADapiproc) glad_glBlendFuncSeparateiOES, 5, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
    glad_glBlendFuncSeparateiOES(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
    _post_call_gles2_callback(NULL, "glBlendFuncSeparateiOES", (GLADapiproc) glad_glBlendFuncSeparateiOES, 5, buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
    
}
PFNGLBLENDFUNCSEPARATEIOESPROC glad_debug_glBlendFuncSeparateiOES = glad_debug_impl_glBlendFuncSeparateiOES;
PFNGLBLENDFUNCIPROC glad_glBlendFunci = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFunci(GLuint buf, GLenum src, GLenum dst) {
    _pre_call_gles2_callback("glBlendFunci", (GLADapiproc) glad_glBlendFunci, 3, buf, src, dst);
    glad_glBlendFunci(buf, src, dst);
    _post_call_gles2_callback(NULL, "glBlendFunci", (GLADapiproc) glad_glBlendFunci, 3, buf, src, dst);
    
}
PFNGLBLENDFUNCIPROC glad_debug_glBlendFunci = glad_debug_impl_glBlendFunci;
PFNGLBLENDFUNCIEXTPROC glad_glBlendFunciEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFunciEXT(GLuint buf, GLenum src, GLenum dst) {
    _pre_call_gles2_callback("glBlendFunciEXT", (GLADapiproc) glad_glBlendFunciEXT, 3, buf, src, dst);
    glad_glBlendFunciEXT(buf, src, dst);
    _post_call_gles2_callback(NULL, "glBlendFunciEXT", (GLADapiproc) glad_glBlendFunciEXT, 3, buf, src, dst);
    
}
PFNGLBLENDFUNCIEXTPROC glad_debug_glBlendFunciEXT = glad_debug_impl_glBlendFunciEXT;
PFNGLBLENDFUNCIOESPROC glad_glBlendFunciOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendFunciOES(GLuint buf, GLenum src, GLenum dst) {
    _pre_call_gles2_callback("glBlendFunciOES", (GLADapiproc) glad_glBlendFunciOES, 3, buf, src, dst);
    glad_glBlendFunciOES(buf, src, dst);
    _post_call_gles2_callback(NULL, "glBlendFunciOES", (GLADapiproc) glad_glBlendFunciOES, 3, buf, src, dst);
    
}
PFNGLBLENDFUNCIOESPROC glad_debug_glBlendFunciOES = glad_debug_impl_glBlendFunciOES;
PFNGLBLENDPARAMETERINVPROC glad_glBlendParameteriNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlendParameteriNV(GLenum pname, GLint value) {
    _pre_call_gles2_callback("glBlendParameteriNV", (GLADapiproc) glad_glBlendParameteriNV, 2, pname, value);
    glad_glBlendParameteriNV(pname, value);
    _post_call_gles2_callback(NULL, "glBlendParameteriNV", (GLADapiproc) glad_glBlendParameteriNV, 2, pname, value);
    
}
PFNGLBLENDPARAMETERINVPROC glad_debug_glBlendParameteriNV = glad_debug_impl_glBlendParameteriNV;
PFNGLBLITFRAMEBUFFERPROC glad_glBlitFramebuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    _pre_call_gles2_callback("glBlitFramebuffer", (GLADapiproc) glad_glBlitFramebuffer, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    glad_glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    _post_call_gles2_callback(NULL, "glBlitFramebuffer", (GLADapiproc) glad_glBlitFramebuffer, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    
}
PFNGLBLITFRAMEBUFFERPROC glad_debug_glBlitFramebuffer = glad_debug_impl_glBlitFramebuffer;
PFNGLBLITFRAMEBUFFERANGLEPROC glad_glBlitFramebufferANGLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlitFramebufferANGLE(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    _pre_call_gles2_callback("glBlitFramebufferANGLE", (GLADapiproc) glad_glBlitFramebufferANGLE, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    glad_glBlitFramebufferANGLE(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    _post_call_gles2_callback(NULL, "glBlitFramebufferANGLE", (GLADapiproc) glad_glBlitFramebufferANGLE, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    
}
PFNGLBLITFRAMEBUFFERANGLEPROC glad_debug_glBlitFramebufferANGLE = glad_debug_impl_glBlitFramebufferANGLE;
PFNGLBLITFRAMEBUFFERLAYEREXTPROC glad_glBlitFramebufferLayerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlitFramebufferLayerEXT(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint srcLayer, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLint dstLayer, GLbitfield mask, GLenum filter) {
    _pre_call_gles2_callback("glBlitFramebufferLayerEXT", (GLADapiproc) glad_glBlitFramebufferLayerEXT, 12, srcX0, srcY0, srcX1, srcY1, srcLayer, dstX0, dstY0, dstX1, dstY1, dstLayer, mask, filter);
    glad_glBlitFramebufferLayerEXT(srcX0, srcY0, srcX1, srcY1, srcLayer, dstX0, dstY0, dstX1, dstY1, dstLayer, mask, filter);
    _post_call_gles2_callback(NULL, "glBlitFramebufferLayerEXT", (GLADapiproc) glad_glBlitFramebufferLayerEXT, 12, srcX0, srcY0, srcX1, srcY1, srcLayer, dstX0, dstY0, dstX1, dstY1, dstLayer, mask, filter);
    
}
PFNGLBLITFRAMEBUFFERLAYEREXTPROC glad_debug_glBlitFramebufferLayerEXT = glad_debug_impl_glBlitFramebufferLayerEXT;
PFNGLBLITFRAMEBUFFERLAYERSEXTPROC glad_glBlitFramebufferLayersEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlitFramebufferLayersEXT(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    _pre_call_gles2_callback("glBlitFramebufferLayersEXT", (GLADapiproc) glad_glBlitFramebufferLayersEXT, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    glad_glBlitFramebufferLayersEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    _post_call_gles2_callback(NULL, "glBlitFramebufferLayersEXT", (GLADapiproc) glad_glBlitFramebufferLayersEXT, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    
}
PFNGLBLITFRAMEBUFFERLAYERSEXTPROC glad_debug_glBlitFramebufferLayersEXT = glad_debug_impl_glBlitFramebufferLayersEXT;
PFNGLBLITFRAMEBUFFERNVPROC glad_glBlitFramebufferNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBlitFramebufferNV(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    _pre_call_gles2_callback("glBlitFramebufferNV", (GLADapiproc) glad_glBlitFramebufferNV, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    glad_glBlitFramebufferNV(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    _post_call_gles2_callback(NULL, "glBlitFramebufferNV", (GLADapiproc) glad_glBlitFramebufferNV, 10, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    
}
PFNGLBLITFRAMEBUFFERNVPROC glad_debug_glBlitFramebufferNV = glad_debug_impl_glBlitFramebufferNV;
PFNGLBUFFERATTACHMEMORYNVPROC glad_glBufferAttachMemoryNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferAttachMemoryNV(GLenum target, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glBufferAttachMemoryNV", (GLADapiproc) glad_glBufferAttachMemoryNV, 3, target, memory, offset);
    glad_glBufferAttachMemoryNV(target, memory, offset);
    _post_call_gles2_callback(NULL, "glBufferAttachMemoryNV", (GLADapiproc) glad_glBufferAttachMemoryNV, 3, target, memory, offset);
    
}
PFNGLBUFFERATTACHMEMORYNVPROC glad_debug_glBufferAttachMemoryNV = glad_debug_impl_glBufferAttachMemoryNV;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferData(GLenum target, GLsizeiptr size, const void * data, GLenum usage) {
    _pre_call_gles2_callback("glBufferData", (GLADapiproc) glad_glBufferData, 4, target, size, data, usage);
    glad_glBufferData(target, size, data, usage);
    _post_call_gles2_callback(NULL, "glBufferData", (GLADapiproc) glad_glBufferData, 4, target, size, data, usage);
    
}
PFNGLBUFFERDATAPROC glad_debug_glBufferData = glad_debug_impl_glBufferData;
PFNGLBUFFERPAGECOMMITMENTMEMNVPROC glad_glBufferPageCommitmentMemNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferPageCommitmentMemNV(GLenum target, GLintptr offset, GLsizeiptr size, GLuint memory, GLuint64 memOffset, GLboolean commit) {
    _pre_call_gles2_callback("glBufferPageCommitmentMemNV", (GLADapiproc) glad_glBufferPageCommitmentMemNV, 6, target, offset, size, memory, memOffset, commit);
    glad_glBufferPageCommitmentMemNV(target, offset, size, memory, memOffset, commit);
    _post_call_gles2_callback(NULL, "glBufferPageCommitmentMemNV", (GLADapiproc) glad_glBufferPageCommitmentMemNV, 6, target, offset, size, memory, memOffset, commit);
    
}
PFNGLBUFFERPAGECOMMITMENTMEMNVPROC glad_debug_glBufferPageCommitmentMemNV = glad_debug_impl_glBufferPageCommitmentMemNV;
PFNGLBUFFERSTORAGEEXTPROC glad_glBufferStorageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferStorageEXT(GLenum target, GLsizeiptr size, const void * data, GLbitfield flags) {
    _pre_call_gles2_callback("glBufferStorageEXT", (GLADapiproc) glad_glBufferStorageEXT, 4, target, size, data, flags);
    glad_glBufferStorageEXT(target, size, data, flags);
    _post_call_gles2_callback(NULL, "glBufferStorageEXT", (GLADapiproc) glad_glBufferStorageEXT, 4, target, size, data, flags);
    
}
PFNGLBUFFERSTORAGEEXTPROC glad_debug_glBufferStorageEXT = glad_debug_impl_glBufferStorageEXT;
PFNGLBUFFERSTORAGEEXTERNALEXTPROC glad_glBufferStorageExternalEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferStorageExternalEXT(GLenum target, GLintptr offset, GLsizeiptr size, GLeglClientBufferEXT clientBuffer, GLbitfield flags) {
    _pre_call_gles2_callback("glBufferStorageExternalEXT", (GLADapiproc) glad_glBufferStorageExternalEXT, 5, target, offset, size, clientBuffer, flags);
    glad_glBufferStorageExternalEXT(target, offset, size, clientBuffer, flags);
    _post_call_gles2_callback(NULL, "glBufferStorageExternalEXT", (GLADapiproc) glad_glBufferStorageExternalEXT, 5, target, offset, size, clientBuffer, flags);
    
}
PFNGLBUFFERSTORAGEEXTERNALEXTPROC glad_debug_glBufferStorageExternalEXT = glad_debug_impl_glBufferStorageExternalEXT;
PFNGLBUFFERSTORAGEMEMEXTPROC glad_glBufferStorageMemEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferStorageMemEXT(GLenum target, GLsizeiptr size, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glBufferStorageMemEXT", (GLADapiproc) glad_glBufferStorageMemEXT, 4, target, size, memory, offset);
    glad_glBufferStorageMemEXT(target, size, memory, offset);
    _post_call_gles2_callback(NULL, "glBufferStorageMemEXT", (GLADapiproc) glad_glBufferStorageMemEXT, 4, target, size, memory, offset);
    
}
PFNGLBUFFERSTORAGEMEMEXTPROC glad_debug_glBufferStorageMemEXT = glad_debug_impl_glBufferStorageMemEXT;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData = NULL;
static void GLAD_API_PTR glad_debug_impl_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void * data) {
    _pre_call_gles2_callback("glBufferSubData", (GLADapiproc) glad_glBufferSubData, 4, target, offset, size, data);
    glad_glBufferSubData(target, offset, size, data);
    _post_call_gles2_callback(NULL, "glBufferSubData", (GLADapiproc) glad_glBufferSubData, 4, target, offset, size, data);
    
}
PFNGLBUFFERSUBDATAPROC glad_debug_glBufferSubData = glad_debug_impl_glBufferSubData;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glCheckFramebufferStatus(GLenum target) {
    GLenum ret;
    _pre_call_gles2_callback("glCheckFramebufferStatus", (GLADapiproc) glad_glCheckFramebufferStatus, 1, target);
    ret = glad_glCheckFramebufferStatus(target);
    _post_call_gles2_callback((void*) &ret, "glCheckFramebufferStatus", (GLADapiproc) glad_glCheckFramebufferStatus, 1, target);
    return ret;
}
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_debug_glCheckFramebufferStatus = glad_debug_impl_glCheckFramebufferStatus;
PFNGLCLEARPROC glad_glClear = NULL;
static void GLAD_API_PTR glad_debug_impl_glClear(GLbitfield mask) {
    _pre_call_gles2_callback("glClear", (GLADapiproc) glad_glClear, 1, mask);
    glad_glClear(mask);
    _post_call_gles2_callback(NULL, "glClear", (GLADapiproc) glad_glClear, 1, mask);
    
}
PFNGLCLEARPROC glad_debug_glClear = glad_debug_impl_glClear;
PFNGLCLEARBUFFERFIPROC glad_glClearBufferfi = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    _pre_call_gles2_callback("glClearBufferfi", (GLADapiproc) glad_glClearBufferfi, 4, buffer, drawbuffer, depth, stencil);
    glad_glClearBufferfi(buffer, drawbuffer, depth, stencil);
    _post_call_gles2_callback(NULL, "glClearBufferfi", (GLADapiproc) glad_glClearBufferfi, 4, buffer, drawbuffer, depth, stencil);
    
}
PFNGLCLEARBUFFERFIPROC glad_debug_glClearBufferfi = glad_debug_impl_glClearBufferfi;
PFNGLCLEARBUFFERFVPROC glad_glClearBufferfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat * value) {
    _pre_call_gles2_callback("glClearBufferfv", (GLADapiproc) glad_glClearBufferfv, 3, buffer, drawbuffer, value);
    glad_glClearBufferfv(buffer, drawbuffer, value);
    _post_call_gles2_callback(NULL, "glClearBufferfv", (GLADapiproc) glad_glClearBufferfv, 3, buffer, drawbuffer, value);
    
}
PFNGLCLEARBUFFERFVPROC glad_debug_glClearBufferfv = glad_debug_impl_glClearBufferfv;
PFNGLCLEARBUFFERIVPROC glad_glClearBufferiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint * value) {
    _pre_call_gles2_callback("glClearBufferiv", (GLADapiproc) glad_glClearBufferiv, 3, buffer, drawbuffer, value);
    glad_glClearBufferiv(buffer, drawbuffer, value);
    _post_call_gles2_callback(NULL, "glClearBufferiv", (GLADapiproc) glad_glClearBufferiv, 3, buffer, drawbuffer, value);
    
}
PFNGLCLEARBUFFERIVPROC glad_debug_glClearBufferiv = glad_debug_impl_glClearBufferiv;
PFNGLCLEARBUFFERUIVPROC glad_glClearBufferuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint * value) {
    _pre_call_gles2_callback("glClearBufferuiv", (GLADapiproc) glad_glClearBufferuiv, 3, buffer, drawbuffer, value);
    glad_glClearBufferuiv(buffer, drawbuffer, value);
    _post_call_gles2_callback(NULL, "glClearBufferuiv", (GLADapiproc) glad_glClearBufferuiv, 3, buffer, drawbuffer, value);
    
}
PFNGLCLEARBUFFERUIVPROC glad_debug_glClearBufferuiv = glad_debug_impl_glClearBufferuiv;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    _pre_call_gles2_callback("glClearColor", (GLADapiproc) glad_glClearColor, 4, red, green, blue, alpha);
    glad_glClearColor(red, green, blue, alpha);
    _post_call_gles2_callback(NULL, "glClearColor", (GLADapiproc) glad_glClearColor, 4, red, green, blue, alpha);
    
}
PFNGLCLEARCOLORPROC glad_debug_glClearColor = glad_debug_impl_glClearColor;
PFNGLCLEARDEPTHFPROC glad_glClearDepthf = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearDepthf(GLfloat d) {
    _pre_call_gles2_callback("glClearDepthf", (GLADapiproc) glad_glClearDepthf, 1, d);
    glad_glClearDepthf(d);
    _post_call_gles2_callback(NULL, "glClearDepthf", (GLADapiproc) glad_glClearDepthf, 1, d);
    
}
PFNGLCLEARDEPTHFPROC glad_debug_glClearDepthf = glad_debug_impl_glClearDepthf;
PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC glad_glClearPixelLocalStorageuiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearPixelLocalStorageuiEXT(GLsizei offset, GLsizei n, const GLuint * values) {
    _pre_call_gles2_callback("glClearPixelLocalStorageuiEXT", (GLADapiproc) glad_glClearPixelLocalStorageuiEXT, 3, offset, n, values);
    glad_glClearPixelLocalStorageuiEXT(offset, n, values);
    _post_call_gles2_callback(NULL, "glClearPixelLocalStorageuiEXT", (GLADapiproc) glad_glClearPixelLocalStorageuiEXT, 3, offset, n, values);
    
}
PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC glad_debug_glClearPixelLocalStorageuiEXT = glad_debug_impl_glClearPixelLocalStorageuiEXT;
PFNGLCLEARSTENCILPROC glad_glClearStencil = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearStencil(GLint s) {
    _pre_call_gles2_callback("glClearStencil", (GLADapiproc) glad_glClearStencil, 1, s);
    glad_glClearStencil(s);
    _post_call_gles2_callback(NULL, "glClearStencil", (GLADapiproc) glad_glClearStencil, 1, s);
    
}
PFNGLCLEARSTENCILPROC glad_debug_glClearStencil = glad_debug_impl_glClearStencil;
PFNGLCLEARTEXIMAGEEXTPROC glad_glClearTexImageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearTexImageEXT(GLuint texture, GLint level, GLenum format, GLenum type, const void * data) {
    _pre_call_gles2_callback("glClearTexImageEXT", (GLADapiproc) glad_glClearTexImageEXT, 5, texture, level, format, type, data);
    glad_glClearTexImageEXT(texture, level, format, type, data);
    _post_call_gles2_callback(NULL, "glClearTexImageEXT", (GLADapiproc) glad_glClearTexImageEXT, 5, texture, level, format, type, data);
    
}
PFNGLCLEARTEXIMAGEEXTPROC glad_debug_glClearTexImageEXT = glad_debug_impl_glClearTexImageEXT;
PFNGLCLEARTEXSUBIMAGEEXTPROC glad_glClearTexSubImageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glClearTexSubImageEXT(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void * data) {
    _pre_call_gles2_callback("glClearTexSubImageEXT", (GLADapiproc) glad_glClearTexSubImageEXT, 11, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
    glad_glClearTexSubImageEXT(texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
    _post_call_gles2_callback(NULL, "glClearTexSubImageEXT", (GLADapiproc) glad_glClearTexSubImageEXT, 11, texture, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
    
}
PFNGLCLEARTEXSUBIMAGEEXTPROC glad_debug_glClearTexSubImageEXT = glad_debug_impl_glClearTexSubImageEXT;
PFNGLCLIENTWAITSYNCPROC glad_glClientWaitSync = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    GLenum ret;
    _pre_call_gles2_callback("glClientWaitSync", (GLADapiproc) glad_glClientWaitSync, 3, sync, flags, timeout);
    ret = glad_glClientWaitSync(sync, flags, timeout);
    _post_call_gles2_callback((void*) &ret, "glClientWaitSync", (GLADapiproc) glad_glClientWaitSync, 3, sync, flags, timeout);
    return ret;
}
PFNGLCLIENTWAITSYNCPROC glad_debug_glClientWaitSync = glad_debug_impl_glClientWaitSync;
PFNGLCLIENTWAITSYNCAPPLEPROC glad_glClientWaitSyncAPPLE = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glClientWaitSyncAPPLE(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    GLenum ret;
    _pre_call_gles2_callback("glClientWaitSyncAPPLE", (GLADapiproc) glad_glClientWaitSyncAPPLE, 3, sync, flags, timeout);
    ret = glad_glClientWaitSyncAPPLE(sync, flags, timeout);
    _post_call_gles2_callback((void*) &ret, "glClientWaitSyncAPPLE", (GLADapiproc) glad_glClientWaitSyncAPPLE, 3, sync, flags, timeout);
    return ret;
}
PFNGLCLIENTWAITSYNCAPPLEPROC glad_debug_glClientWaitSyncAPPLE = glad_debug_impl_glClientWaitSyncAPPLE;
PFNGLCLIPCONTROLEXTPROC glad_glClipControlEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glClipControlEXT(GLenum origin, GLenum depth) {
    _pre_call_gles2_callback("glClipControlEXT", (GLADapiproc) glad_glClipControlEXT, 2, origin, depth);
    glad_glClipControlEXT(origin, depth);
    _post_call_gles2_callback(NULL, "glClipControlEXT", (GLADapiproc) glad_glClipControlEXT, 2, origin, depth);
    
}
PFNGLCLIPCONTROLEXTPROC glad_debug_glClipControlEXT = glad_debug_impl_glClipControlEXT;
PFNGLCOLORMASKPROC glad_glColorMask = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    _pre_call_gles2_callback("glColorMask", (GLADapiproc) glad_glColorMask, 4, red, green, blue, alpha);
    glad_glColorMask(red, green, blue, alpha);
    _post_call_gles2_callback(NULL, "glColorMask", (GLADapiproc) glad_glColorMask, 4, red, green, blue, alpha);
    
}
PFNGLCOLORMASKPROC glad_debug_glColorMask = glad_debug_impl_glColorMask;
PFNGLCOLORMASKIPROC glad_glColorMaski = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorMaski(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    _pre_call_gles2_callback("glColorMaski", (GLADapiproc) glad_glColorMaski, 5, index, r, g, b, a);
    glad_glColorMaski(index, r, g, b, a);
    _post_call_gles2_callback(NULL, "glColorMaski", (GLADapiproc) glad_glColorMaski, 5, index, r, g, b, a);
    
}
PFNGLCOLORMASKIPROC glad_debug_glColorMaski = glad_debug_impl_glColorMaski;
PFNGLCOLORMASKIEXTPROC glad_glColorMaskiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorMaskiEXT(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    _pre_call_gles2_callback("glColorMaskiEXT", (GLADapiproc) glad_glColorMaskiEXT, 5, index, r, g, b, a);
    glad_glColorMaskiEXT(index, r, g, b, a);
    _post_call_gles2_callback(NULL, "glColorMaskiEXT", (GLADapiproc) glad_glColorMaskiEXT, 5, index, r, g, b, a);
    
}
PFNGLCOLORMASKIEXTPROC glad_debug_glColorMaskiEXT = glad_debug_impl_glColorMaskiEXT;
PFNGLCOLORMASKIOESPROC glad_glColorMaskiOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glColorMaskiOES(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    _pre_call_gles2_callback("glColorMaskiOES", (GLADapiproc) glad_glColorMaskiOES, 5, index, r, g, b, a);
    glad_glColorMaskiOES(index, r, g, b, a);
    _post_call_gles2_callback(NULL, "glColorMaskiOES", (GLADapiproc) glad_glColorMaskiOES, 5, index, r, g, b, a);
    
}
PFNGLCOLORMASKIOESPROC glad_debug_glColorMaskiOES = glad_debug_impl_glColorMaskiOES;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompileShader(GLuint shader) {
    _pre_call_gles2_callback("glCompileShader", (GLADapiproc) glad_glCompileShader, 1, shader);
    glad_glCompileShader(shader);
    _post_call_gles2_callback(NULL, "glCompileShader", (GLADapiproc) glad_glCompileShader, 1, shader);
    
}
PFNGLCOMPILESHADERPROC glad_debug_glCompileShader = glad_debug_impl_glCompileShader;
PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_glCompressedTexImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gles2_callback("glCompressedTexImage2D", (GLADapiproc) glad_glCompressedTexImage2D, 8, target, level, internalformat, width, height, border, imageSize, data);
    glad_glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
    _post_call_gles2_callback(NULL, "glCompressedTexImage2D", (GLADapiproc) glad_glCompressedTexImage2D, 8, target, level, internalformat, width, height, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE2DPROC glad_debug_glCompressedTexImage2D = glad_debug_impl_glCompressedTexImage2D;
PFNGLCOMPRESSEDTEXIMAGE3DPROC glad_glCompressedTexImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gles2_callback("glCompressedTexImage3D", (GLADapiproc) glad_glCompressedTexImage3D, 9, target, level, internalformat, width, height, depth, border, imageSize, data);
    glad_glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
    _post_call_gles2_callback(NULL, "glCompressedTexImage3D", (GLADapiproc) glad_glCompressedTexImage3D, 9, target, level, internalformat, width, height, depth, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE3DPROC glad_debug_glCompressedTexImage3D = glad_debug_impl_glCompressedTexImage3D;
PFNGLCOMPRESSEDTEXIMAGE3DOESPROC glad_glCompressedTexImage3DOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexImage3DOES(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void * data) {
    _pre_call_gles2_callback("glCompressedTexImage3DOES", (GLADapiproc) glad_glCompressedTexImage3DOES, 9, target, level, internalformat, width, height, depth, border, imageSize, data);
    glad_glCompressedTexImage3DOES(target, level, internalformat, width, height, depth, border, imageSize, data);
    _post_call_gles2_callback(NULL, "glCompressedTexImage3DOES", (GLADapiproc) glad_glCompressedTexImage3DOES, 9, target, level, internalformat, width, height, depth, border, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXIMAGE3DOESPROC glad_debug_glCompressedTexImage3DOES = glad_debug_impl_glCompressedTexImage3DOES;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_glCompressedTexSubImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gles2_callback("glCompressedTexSubImage2D", (GLADapiproc) glad_glCompressedTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, imageSize, data);
    glad_glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    _post_call_gles2_callback(NULL, "glCompressedTexSubImage2D", (GLADapiproc) glad_glCompressedTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glad_debug_glCompressedTexSubImage2D = glad_debug_impl_glCompressedTexSubImage2D;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glad_glCompressedTexSubImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gles2_callback("glCompressedTexSubImage3D", (GLADapiproc) glad_glCompressedTexSubImage3D, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    glad_glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    _post_call_gles2_callback(NULL, "glCompressedTexSubImage3D", (GLADapiproc) glad_glCompressedTexSubImage3D, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glad_debug_glCompressedTexSubImage3D = glad_debug_impl_glCompressedTexSubImage3D;
PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC glad_glCompressedTexSubImage3DOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glCompressedTexSubImage3DOES(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void * data) {
    _pre_call_gles2_callback("glCompressedTexSubImage3DOES", (GLADapiproc) glad_glCompressedTexSubImage3DOES, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    glad_glCompressedTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    _post_call_gles2_callback(NULL, "glCompressedTexSubImage3DOES", (GLADapiproc) glad_glCompressedTexSubImage3DOES, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
    
}
PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC glad_debug_glCompressedTexSubImage3DOES = glad_debug_impl_glCompressedTexSubImage3DOES;
PFNGLCONSERVATIVERASTERPARAMETERINVPROC glad_glConservativeRasterParameteriNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glConservativeRasterParameteriNV(GLenum pname, GLint param) {
    _pre_call_gles2_callback("glConservativeRasterParameteriNV", (GLADapiproc) glad_glConservativeRasterParameteriNV, 2, pname, param);
    glad_glConservativeRasterParameteriNV(pname, param);
    _post_call_gles2_callback(NULL, "glConservativeRasterParameteriNV", (GLADapiproc) glad_glConservativeRasterParameteriNV, 2, pname, param);
    
}
PFNGLCONSERVATIVERASTERPARAMETERINVPROC glad_debug_glConservativeRasterParameteriNV = glad_debug_impl_glConservativeRasterParameteriNV;
PFNGLCOPYBUFFERSUBDATAPROC glad_glCopyBufferSubData = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    _pre_call_gles2_callback("glCopyBufferSubData", (GLADapiproc) glad_glCopyBufferSubData, 5, readTarget, writeTarget, readOffset, writeOffset, size);
    glad_glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
    _post_call_gles2_callback(NULL, "glCopyBufferSubData", (GLADapiproc) glad_glCopyBufferSubData, 5, readTarget, writeTarget, readOffset, writeOffset, size);
    
}
PFNGLCOPYBUFFERSUBDATAPROC glad_debug_glCopyBufferSubData = glad_debug_impl_glCopyBufferSubData;
PFNGLCOPYBUFFERSUBDATANVPROC glad_glCopyBufferSubDataNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyBufferSubDataNV(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    _pre_call_gles2_callback("glCopyBufferSubDataNV", (GLADapiproc) glad_glCopyBufferSubDataNV, 5, readTarget, writeTarget, readOffset, writeOffset, size);
    glad_glCopyBufferSubDataNV(readTarget, writeTarget, readOffset, writeOffset, size);
    _post_call_gles2_callback(NULL, "glCopyBufferSubDataNV", (GLADapiproc) glad_glCopyBufferSubDataNV, 5, readTarget, writeTarget, readOffset, writeOffset, size);
    
}
PFNGLCOPYBUFFERSUBDATANVPROC glad_debug_glCopyBufferSubDataNV = glad_debug_impl_glCopyBufferSubDataNV;
PFNGLCOPYIMAGESUBDATAPROC glad_glCopyImageSubData = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyImageSubData(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth) {
    _pre_call_gles2_callback("glCopyImageSubData", (GLADapiproc) glad_glCopyImageSubData, 15, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    glad_glCopyImageSubData(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    _post_call_gles2_callback(NULL, "glCopyImageSubData", (GLADapiproc) glad_glCopyImageSubData, 15, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    
}
PFNGLCOPYIMAGESUBDATAPROC glad_debug_glCopyImageSubData = glad_debug_impl_glCopyImageSubData;
PFNGLCOPYIMAGESUBDATAEXTPROC glad_glCopyImageSubDataEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyImageSubDataEXT(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth) {
    _pre_call_gles2_callback("glCopyImageSubDataEXT", (GLADapiproc) glad_glCopyImageSubDataEXT, 15, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    glad_glCopyImageSubDataEXT(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    _post_call_gles2_callback(NULL, "glCopyImageSubDataEXT", (GLADapiproc) glad_glCopyImageSubDataEXT, 15, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    
}
PFNGLCOPYIMAGESUBDATAEXTPROC glad_debug_glCopyImageSubDataEXT = glad_debug_impl_glCopyImageSubDataEXT;
PFNGLCOPYIMAGESUBDATAOESPROC glad_glCopyImageSubDataOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyImageSubDataOES(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth) {
    _pre_call_gles2_callback("glCopyImageSubDataOES", (GLADapiproc) glad_glCopyImageSubDataOES, 15, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    glad_glCopyImageSubDataOES(srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    _post_call_gles2_callback(NULL, "glCopyImageSubDataOES", (GLADapiproc) glad_glCopyImageSubDataOES, 15, srcName, srcTarget, srcLevel, srcX, srcY, srcZ, dstName, dstTarget, dstLevel, dstX, dstY, dstZ, srcWidth, srcHeight, srcDepth);
    
}
PFNGLCOPYIMAGESUBDATAOESPROC glad_debug_glCopyImageSubDataOES = glad_debug_impl_glCopyImageSubDataOES;
PFNGLCOPYPATHNVPROC glad_glCopyPathNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyPathNV(GLuint resultPath, GLuint srcPath) {
    _pre_call_gles2_callback("glCopyPathNV", (GLADapiproc) glad_glCopyPathNV, 2, resultPath, srcPath);
    glad_glCopyPathNV(resultPath, srcPath);
    _post_call_gles2_callback(NULL, "glCopyPathNV", (GLADapiproc) glad_glCopyPathNV, 2, resultPath, srcPath);
    
}
PFNGLCOPYPATHNVPROC glad_debug_glCopyPathNV = glad_debug_impl_glCopyPathNV;
PFNGLCOPYTEXIMAGE2DPROC glad_glCopyTexImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) {
    _pre_call_gles2_callback("glCopyTexImage2D", (GLADapiproc) glad_glCopyTexImage2D, 8, target, level, internalformat, x, y, width, height, border);
    glad_glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    _post_call_gles2_callback(NULL, "glCopyTexImage2D", (GLADapiproc) glad_glCopyTexImage2D, 8, target, level, internalformat, x, y, width, height, border);
    
}
PFNGLCOPYTEXIMAGE2DPROC glad_debug_glCopyTexImage2D = glad_debug_impl_glCopyTexImage2D;
PFNGLCOPYTEXSUBIMAGE2DPROC glad_glCopyTexSubImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glCopyTexSubImage2D", (GLADapiproc) glad_glCopyTexSubImage2D, 8, target, level, xoffset, yoffset, x, y, width, height);
    glad_glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    _post_call_gles2_callback(NULL, "glCopyTexSubImage2D", (GLADapiproc) glad_glCopyTexSubImage2D, 8, target, level, xoffset, yoffset, x, y, width, height);
    
}
PFNGLCOPYTEXSUBIMAGE2DPROC glad_debug_glCopyTexSubImage2D = glad_debug_impl_glCopyTexSubImage2D;
PFNGLCOPYTEXSUBIMAGE3DPROC glad_glCopyTexSubImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glCopyTexSubImage3D", (GLADapiproc) glad_glCopyTexSubImage3D, 9, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    glad_glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    _post_call_gles2_callback(NULL, "glCopyTexSubImage3D", (GLADapiproc) glad_glCopyTexSubImage3D, 9, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    
}
PFNGLCOPYTEXSUBIMAGE3DPROC glad_debug_glCopyTexSubImage3D = glad_debug_impl_glCopyTexSubImage3D;
PFNGLCOPYTEXSUBIMAGE3DOESPROC glad_glCopyTexSubImage3DOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTexSubImage3DOES(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glCopyTexSubImage3DOES", (GLADapiproc) glad_glCopyTexSubImage3DOES, 9, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    glad_glCopyTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    _post_call_gles2_callback(NULL, "glCopyTexSubImage3DOES", (GLADapiproc) glad_glCopyTexSubImage3DOES, 9, target, level, xoffset, yoffset, zoffset, x, y, width, height);
    
}
PFNGLCOPYTEXSUBIMAGE3DOESPROC glad_debug_glCopyTexSubImage3DOES = glad_debug_impl_glCopyTexSubImage3DOES;
PFNGLCOPYTEXTURELEVELSAPPLEPROC glad_glCopyTextureLevelsAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glCopyTextureLevelsAPPLE(GLuint destinationTexture, GLuint sourceTexture, GLint sourceBaseLevel, GLsizei sourceLevelCount) {
    _pre_call_gles2_callback("glCopyTextureLevelsAPPLE", (GLADapiproc) glad_glCopyTextureLevelsAPPLE, 4, destinationTexture, sourceTexture, sourceBaseLevel, sourceLevelCount);
    glad_glCopyTextureLevelsAPPLE(destinationTexture, sourceTexture, sourceBaseLevel, sourceLevelCount);
    _post_call_gles2_callback(NULL, "glCopyTextureLevelsAPPLE", (GLADapiproc) glad_glCopyTextureLevelsAPPLE, 4, destinationTexture, sourceTexture, sourceBaseLevel, sourceLevelCount);
    
}
PFNGLCOPYTEXTURELEVELSAPPLEPROC glad_debug_glCopyTextureLevelsAPPLE = glad_debug_impl_glCopyTextureLevelsAPPLE;
PFNGLCOVERFILLPATHINSTANCEDNVPROC glad_glCoverFillPathInstancedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCoverFillPathInstancedNV(GLsizei numPaths, GLenum pathNameType, const void * paths, GLuint pathBase, GLenum coverMode, GLenum transformType, const GLfloat * transformValues) {
    _pre_call_gles2_callback("glCoverFillPathInstancedNV", (GLADapiproc) glad_glCoverFillPathInstancedNV, 7, numPaths, pathNameType, paths, pathBase, coverMode, transformType, transformValues);
    glad_glCoverFillPathInstancedNV(numPaths, pathNameType, paths, pathBase, coverMode, transformType, transformValues);
    _post_call_gles2_callback(NULL, "glCoverFillPathInstancedNV", (GLADapiproc) glad_glCoverFillPathInstancedNV, 7, numPaths, pathNameType, paths, pathBase, coverMode, transformType, transformValues);
    
}
PFNGLCOVERFILLPATHINSTANCEDNVPROC glad_debug_glCoverFillPathInstancedNV = glad_debug_impl_glCoverFillPathInstancedNV;
PFNGLCOVERFILLPATHNVPROC glad_glCoverFillPathNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCoverFillPathNV(GLuint path, GLenum coverMode) {
    _pre_call_gles2_callback("glCoverFillPathNV", (GLADapiproc) glad_glCoverFillPathNV, 2, path, coverMode);
    glad_glCoverFillPathNV(path, coverMode);
    _post_call_gles2_callback(NULL, "glCoverFillPathNV", (GLADapiproc) glad_glCoverFillPathNV, 2, path, coverMode);
    
}
PFNGLCOVERFILLPATHNVPROC glad_debug_glCoverFillPathNV = glad_debug_impl_glCoverFillPathNV;
PFNGLCOVERSTROKEPATHINSTANCEDNVPROC glad_glCoverStrokePathInstancedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCoverStrokePathInstancedNV(GLsizei numPaths, GLenum pathNameType, const void * paths, GLuint pathBase, GLenum coverMode, GLenum transformType, const GLfloat * transformValues) {
    _pre_call_gles2_callback("glCoverStrokePathInstancedNV", (GLADapiproc) glad_glCoverStrokePathInstancedNV, 7, numPaths, pathNameType, paths, pathBase, coverMode, transformType, transformValues);
    glad_glCoverStrokePathInstancedNV(numPaths, pathNameType, paths, pathBase, coverMode, transformType, transformValues);
    _post_call_gles2_callback(NULL, "glCoverStrokePathInstancedNV", (GLADapiproc) glad_glCoverStrokePathInstancedNV, 7, numPaths, pathNameType, paths, pathBase, coverMode, transformType, transformValues);
    
}
PFNGLCOVERSTROKEPATHINSTANCEDNVPROC glad_debug_glCoverStrokePathInstancedNV = glad_debug_impl_glCoverStrokePathInstancedNV;
PFNGLCOVERSTROKEPATHNVPROC glad_glCoverStrokePathNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCoverStrokePathNV(GLuint path, GLenum coverMode) {
    _pre_call_gles2_callback("glCoverStrokePathNV", (GLADapiproc) glad_glCoverStrokePathNV, 2, path, coverMode);
    glad_glCoverStrokePathNV(path, coverMode);
    _post_call_gles2_callback(NULL, "glCoverStrokePathNV", (GLADapiproc) glad_glCoverStrokePathNV, 2, path, coverMode);
    
}
PFNGLCOVERSTROKEPATHNVPROC glad_debug_glCoverStrokePathNV = glad_debug_impl_glCoverStrokePathNV;
PFNGLCOVERAGEMASKNVPROC glad_glCoverageMaskNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCoverageMaskNV(GLboolean mask) {
    _pre_call_gles2_callback("glCoverageMaskNV", (GLADapiproc) glad_glCoverageMaskNV, 1, mask);
    glad_glCoverageMaskNV(mask);
    _post_call_gles2_callback(NULL, "glCoverageMaskNV", (GLADapiproc) glad_glCoverageMaskNV, 1, mask);
    
}
PFNGLCOVERAGEMASKNVPROC glad_debug_glCoverageMaskNV = glad_debug_impl_glCoverageMaskNV;
PFNGLCOVERAGEMODULATIONNVPROC glad_glCoverageModulationNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCoverageModulationNV(GLenum components) {
    _pre_call_gles2_callback("glCoverageModulationNV", (GLADapiproc) glad_glCoverageModulationNV, 1, components);
    glad_glCoverageModulationNV(components);
    _post_call_gles2_callback(NULL, "glCoverageModulationNV", (GLADapiproc) glad_glCoverageModulationNV, 1, components);
    
}
PFNGLCOVERAGEMODULATIONNVPROC glad_debug_glCoverageModulationNV = glad_debug_impl_glCoverageModulationNV;
PFNGLCOVERAGEMODULATIONTABLENVPROC glad_glCoverageModulationTableNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCoverageModulationTableNV(GLsizei n, const GLfloat * v) {
    _pre_call_gles2_callback("glCoverageModulationTableNV", (GLADapiproc) glad_glCoverageModulationTableNV, 2, n, v);
    glad_glCoverageModulationTableNV(n, v);
    _post_call_gles2_callback(NULL, "glCoverageModulationTableNV", (GLADapiproc) glad_glCoverageModulationTableNV, 2, n, v);
    
}
PFNGLCOVERAGEMODULATIONTABLENVPROC glad_debug_glCoverageModulationTableNV = glad_debug_impl_glCoverageModulationTableNV;
PFNGLCOVERAGEOPERATIONNVPROC glad_glCoverageOperationNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCoverageOperationNV(GLenum operation) {
    _pre_call_gles2_callback("glCoverageOperationNV", (GLADapiproc) glad_glCoverageOperationNV, 1, operation);
    glad_glCoverageOperationNV(operation);
    _post_call_gles2_callback(NULL, "glCoverageOperationNV", (GLADapiproc) glad_glCoverageOperationNV, 1, operation);
    
}
PFNGLCOVERAGEOPERATIONNVPROC glad_debug_glCoverageOperationNV = glad_debug_impl_glCoverageOperationNV;
PFNGLCREATEMEMORYOBJECTSEXTPROC glad_glCreateMemoryObjectsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glCreateMemoryObjectsEXT(GLsizei n, GLuint * memoryObjects) {
    _pre_call_gles2_callback("glCreateMemoryObjectsEXT", (GLADapiproc) glad_glCreateMemoryObjectsEXT, 2, n, memoryObjects);
    glad_glCreateMemoryObjectsEXT(n, memoryObjects);
    _post_call_gles2_callback(NULL, "glCreateMemoryObjectsEXT", (GLADapiproc) glad_glCreateMemoryObjectsEXT, 2, n, memoryObjects);
    
}
PFNGLCREATEMEMORYOBJECTSEXTPROC glad_debug_glCreateMemoryObjectsEXT = glad_debug_impl_glCreateMemoryObjectsEXT;
PFNGLCREATEPERFQUERYINTELPROC glad_glCreatePerfQueryINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glCreatePerfQueryINTEL(GLuint queryId, GLuint * queryHandle) {
    _pre_call_gles2_callback("glCreatePerfQueryINTEL", (GLADapiproc) glad_glCreatePerfQueryINTEL, 2, queryId, queryHandle);
    glad_glCreatePerfQueryINTEL(queryId, queryHandle);
    _post_call_gles2_callback(NULL, "glCreatePerfQueryINTEL", (GLADapiproc) glad_glCreatePerfQueryINTEL, 2, queryId, queryHandle);
    
}
PFNGLCREATEPERFQUERYINTELPROC glad_debug_glCreatePerfQueryINTEL = glad_debug_impl_glCreatePerfQueryINTEL;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glCreateProgram(void) {
    GLuint ret;
    _pre_call_gles2_callback("glCreateProgram", (GLADapiproc) glad_glCreateProgram, 0);
    ret = glad_glCreateProgram();
    _post_call_gles2_callback((void*) &ret, "glCreateProgram", (GLADapiproc) glad_glCreateProgram, 0);
    return ret;
}
PFNGLCREATEPROGRAMPROC glad_debug_glCreateProgram = glad_debug_impl_glCreateProgram;
PFNGLCREATESEMAPHORESNVPROC glad_glCreateSemaphoresNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glCreateSemaphoresNV(GLsizei n, GLuint * semaphores) {
    _pre_call_gles2_callback("glCreateSemaphoresNV", (GLADapiproc) glad_glCreateSemaphoresNV, 2, n, semaphores);
    glad_glCreateSemaphoresNV(n, semaphores);
    _post_call_gles2_callback(NULL, "glCreateSemaphoresNV", (GLADapiproc) glad_glCreateSemaphoresNV, 2, n, semaphores);
    
}
PFNGLCREATESEMAPHORESNVPROC glad_debug_glCreateSemaphoresNV = glad_debug_impl_glCreateSemaphoresNV;
PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glCreateShader(GLenum type) {
    GLuint ret;
    _pre_call_gles2_callback("glCreateShader", (GLADapiproc) glad_glCreateShader, 1, type);
    ret = glad_glCreateShader(type);
    _post_call_gles2_callback((void*) &ret, "glCreateShader", (GLADapiproc) glad_glCreateShader, 1, type);
    return ret;
}
PFNGLCREATESHADERPROC glad_debug_glCreateShader = glad_debug_impl_glCreateShader;
PFNGLCREATESHADERPROGRAMVPROC glad_glCreateShaderProgramv = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glCreateShaderProgramv(GLenum type, GLsizei count, const GLchar *const* strings) {
    GLuint ret;
    _pre_call_gles2_callback("glCreateShaderProgramv", (GLADapiproc) glad_glCreateShaderProgramv, 3, type, count, strings);
    ret = glad_glCreateShaderProgramv(type, count, strings);
    _post_call_gles2_callback((void*) &ret, "glCreateShaderProgramv", (GLADapiproc) glad_glCreateShaderProgramv, 3, type, count, strings);
    return ret;
}
PFNGLCREATESHADERPROGRAMVPROC glad_debug_glCreateShaderProgramv = glad_debug_impl_glCreateShaderProgramv;
PFNGLCREATESHADERPROGRAMVEXTPROC glad_glCreateShaderProgramvEXT = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glCreateShaderProgramvEXT(GLenum type, GLsizei count, const GLchar *const* strings) {
    GLuint ret;
    _pre_call_gles2_callback("glCreateShaderProgramvEXT", (GLADapiproc) glad_glCreateShaderProgramvEXT, 3, type, count, strings);
    ret = glad_glCreateShaderProgramvEXT(type, count, strings);
    _post_call_gles2_callback((void*) &ret, "glCreateShaderProgramvEXT", (GLADapiproc) glad_glCreateShaderProgramvEXT, 3, type, count, strings);
    return ret;
}
PFNGLCREATESHADERPROGRAMVEXTPROC glad_debug_glCreateShaderProgramvEXT = glad_debug_impl_glCreateShaderProgramvEXT;
PFNGLCULLFACEPROC glad_glCullFace = NULL;
static void GLAD_API_PTR glad_debug_impl_glCullFace(GLenum mode) {
    _pre_call_gles2_callback("glCullFace", (GLADapiproc) glad_glCullFace, 1, mode);
    glad_glCullFace(mode);
    _post_call_gles2_callback(NULL, "glCullFace", (GLADapiproc) glad_glCullFace, 1, mode);
    
}
PFNGLCULLFACEPROC glad_debug_glCullFace = glad_debug_impl_glCullFace;
PFNGLDEBUGMESSAGECALLBACKPROC glad_glDebugMessageCallback = NULL;
static void GLAD_API_PTR glad_debug_impl_glDebugMessageCallback(GLDEBUGPROC callback, const void * userParam) {
    _pre_call_gles2_callback("glDebugMessageCallback", (GLADapiproc) glad_glDebugMessageCallback, 2, callback, userParam);
    glad_glDebugMessageCallback(callback, userParam);
    _post_call_gles2_callback(NULL, "glDebugMessageCallback", (GLADapiproc) glad_glDebugMessageCallback, 2, callback, userParam);
    
}
PFNGLDEBUGMESSAGECALLBACKPROC glad_debug_glDebugMessageCallback = glad_debug_impl_glDebugMessageCallback;
PFNGLDEBUGMESSAGECALLBACKKHRPROC glad_glDebugMessageCallbackKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glDebugMessageCallbackKHR(GLDEBUGPROCKHR callback, const void * userParam) {
    _pre_call_gles2_callback("glDebugMessageCallbackKHR", (GLADapiproc) glad_glDebugMessageCallbackKHR, 2, callback, userParam);
    glad_glDebugMessageCallbackKHR(callback, userParam);
    _post_call_gles2_callback(NULL, "glDebugMessageCallbackKHR", (GLADapiproc) glad_glDebugMessageCallbackKHR, 2, callback, userParam);
    
}
PFNGLDEBUGMESSAGECALLBACKKHRPROC glad_debug_glDebugMessageCallbackKHR = glad_debug_impl_glDebugMessageCallbackKHR;
PFNGLDEBUGMESSAGECONTROLPROC glad_glDebugMessageControl = NULL;
static void GLAD_API_PTR glad_debug_impl_glDebugMessageControl(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint * ids, GLboolean enabled) {
    _pre_call_gles2_callback("glDebugMessageControl", (GLADapiproc) glad_glDebugMessageControl, 6, source, type, severity, count, ids, enabled);
    glad_glDebugMessageControl(source, type, severity, count, ids, enabled);
    _post_call_gles2_callback(NULL, "glDebugMessageControl", (GLADapiproc) glad_glDebugMessageControl, 6, source, type, severity, count, ids, enabled);
    
}
PFNGLDEBUGMESSAGECONTROLPROC glad_debug_glDebugMessageControl = glad_debug_impl_glDebugMessageControl;
PFNGLDEBUGMESSAGECONTROLKHRPROC glad_glDebugMessageControlKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glDebugMessageControlKHR(GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint * ids, GLboolean enabled) {
    _pre_call_gles2_callback("glDebugMessageControlKHR", (GLADapiproc) glad_glDebugMessageControlKHR, 6, source, type, severity, count, ids, enabled);
    glad_glDebugMessageControlKHR(source, type, severity, count, ids, enabled);
    _post_call_gles2_callback(NULL, "glDebugMessageControlKHR", (GLADapiproc) glad_glDebugMessageControlKHR, 6, source, type, severity, count, ids, enabled);
    
}
PFNGLDEBUGMESSAGECONTROLKHRPROC glad_debug_glDebugMessageControlKHR = glad_debug_impl_glDebugMessageControlKHR;
PFNGLDEBUGMESSAGEINSERTPROC glad_glDebugMessageInsert = NULL;
static void GLAD_API_PTR glad_debug_impl_glDebugMessageInsert(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * buf) {
    _pre_call_gles2_callback("glDebugMessageInsert", (GLADapiproc) glad_glDebugMessageInsert, 6, source, type, id, severity, length, buf);
    glad_glDebugMessageInsert(source, type, id, severity, length, buf);
    _post_call_gles2_callback(NULL, "glDebugMessageInsert", (GLADapiproc) glad_glDebugMessageInsert, 6, source, type, id, severity, length, buf);
    
}
PFNGLDEBUGMESSAGEINSERTPROC glad_debug_glDebugMessageInsert = glad_debug_impl_glDebugMessageInsert;
PFNGLDEBUGMESSAGEINSERTKHRPROC glad_glDebugMessageInsertKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glDebugMessageInsertKHR(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar * buf) {
    _pre_call_gles2_callback("glDebugMessageInsertKHR", (GLADapiproc) glad_glDebugMessageInsertKHR, 6, source, type, id, severity, length, buf);
    glad_glDebugMessageInsertKHR(source, type, id, severity, length, buf);
    _post_call_gles2_callback(NULL, "glDebugMessageInsertKHR", (GLADapiproc) glad_glDebugMessageInsertKHR, 6, source, type, id, severity, length, buf);
    
}
PFNGLDEBUGMESSAGEINSERTKHRPROC glad_debug_glDebugMessageInsertKHR = glad_debug_impl_glDebugMessageInsertKHR;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteBuffers(GLsizei n, const GLuint * buffers) {
    _pre_call_gles2_callback("glDeleteBuffers", (GLADapiproc) glad_glDeleteBuffers, 2, n, buffers);
    glad_glDeleteBuffers(n, buffers);
    _post_call_gles2_callback(NULL, "glDeleteBuffers", (GLADapiproc) glad_glDeleteBuffers, 2, n, buffers);
    
}
PFNGLDELETEBUFFERSPROC glad_debug_glDeleteBuffers = glad_debug_impl_glDeleteBuffers;
PFNGLDELETEFENCESNVPROC glad_glDeleteFencesNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteFencesNV(GLsizei n, const GLuint * fences) {
    _pre_call_gles2_callback("glDeleteFencesNV", (GLADapiproc) glad_glDeleteFencesNV, 2, n, fences);
    glad_glDeleteFencesNV(n, fences);
    _post_call_gles2_callback(NULL, "glDeleteFencesNV", (GLADapiproc) glad_glDeleteFencesNV, 2, n, fences);
    
}
PFNGLDELETEFENCESNVPROC glad_debug_glDeleteFencesNV = glad_debug_impl_glDeleteFencesNV;
PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteFramebuffers(GLsizei n, const GLuint * framebuffers) {
    _pre_call_gles2_callback("glDeleteFramebuffers", (GLADapiproc) glad_glDeleteFramebuffers, 2, n, framebuffers);
    glad_glDeleteFramebuffers(n, framebuffers);
    _post_call_gles2_callback(NULL, "glDeleteFramebuffers", (GLADapiproc) glad_glDeleteFramebuffers, 2, n, framebuffers);
    
}
PFNGLDELETEFRAMEBUFFERSPROC glad_debug_glDeleteFramebuffers = glad_debug_impl_glDeleteFramebuffers;
PFNGLDELETEMEMORYOBJECTSEXTPROC glad_glDeleteMemoryObjectsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteMemoryObjectsEXT(GLsizei n, const GLuint * memoryObjects) {
    _pre_call_gles2_callback("glDeleteMemoryObjectsEXT", (GLADapiproc) glad_glDeleteMemoryObjectsEXT, 2, n, memoryObjects);
    glad_glDeleteMemoryObjectsEXT(n, memoryObjects);
    _post_call_gles2_callback(NULL, "glDeleteMemoryObjectsEXT", (GLADapiproc) glad_glDeleteMemoryObjectsEXT, 2, n, memoryObjects);
    
}
PFNGLDELETEMEMORYOBJECTSEXTPROC glad_debug_glDeleteMemoryObjectsEXT = glad_debug_impl_glDeleteMemoryObjectsEXT;
PFNGLDELETEPATHSNVPROC glad_glDeletePathsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeletePathsNV(GLuint path, GLsizei range) {
    _pre_call_gles2_callback("glDeletePathsNV", (GLADapiproc) glad_glDeletePathsNV, 2, path, range);
    glad_glDeletePathsNV(path, range);
    _post_call_gles2_callback(NULL, "glDeletePathsNV", (GLADapiproc) glad_glDeletePathsNV, 2, path, range);
    
}
PFNGLDELETEPATHSNVPROC glad_debug_glDeletePathsNV = glad_debug_impl_glDeletePathsNV;
PFNGLDELETEPERFMONITORSAMDPROC glad_glDeletePerfMonitorsAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeletePerfMonitorsAMD(GLsizei n, GLuint * monitors) {
    _pre_call_gles2_callback("glDeletePerfMonitorsAMD", (GLADapiproc) glad_glDeletePerfMonitorsAMD, 2, n, monitors);
    glad_glDeletePerfMonitorsAMD(n, monitors);
    _post_call_gles2_callback(NULL, "glDeletePerfMonitorsAMD", (GLADapiproc) glad_glDeletePerfMonitorsAMD, 2, n, monitors);
    
}
PFNGLDELETEPERFMONITORSAMDPROC glad_debug_glDeletePerfMonitorsAMD = glad_debug_impl_glDeletePerfMonitorsAMD;
PFNGLDELETEPERFQUERYINTELPROC glad_glDeletePerfQueryINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeletePerfQueryINTEL(GLuint queryHandle) {
    _pre_call_gles2_callback("glDeletePerfQueryINTEL", (GLADapiproc) glad_glDeletePerfQueryINTEL, 1, queryHandle);
    glad_glDeletePerfQueryINTEL(queryHandle);
    _post_call_gles2_callback(NULL, "glDeletePerfQueryINTEL", (GLADapiproc) glad_glDeletePerfQueryINTEL, 1, queryHandle);
    
}
PFNGLDELETEPERFQUERYINTELPROC glad_debug_glDeletePerfQueryINTEL = glad_debug_impl_glDeletePerfQueryINTEL;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteProgram(GLuint program) {
    _pre_call_gles2_callback("glDeleteProgram", (GLADapiproc) glad_glDeleteProgram, 1, program);
    glad_glDeleteProgram(program);
    _post_call_gles2_callback(NULL, "glDeleteProgram", (GLADapiproc) glad_glDeleteProgram, 1, program);
    
}
PFNGLDELETEPROGRAMPROC glad_debug_glDeleteProgram = glad_debug_impl_glDeleteProgram;
PFNGLDELETEPROGRAMPIPELINESPROC glad_glDeleteProgramPipelines = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteProgramPipelines(GLsizei n, const GLuint * pipelines) {
    _pre_call_gles2_callback("glDeleteProgramPipelines", (GLADapiproc) glad_glDeleteProgramPipelines, 2, n, pipelines);
    glad_glDeleteProgramPipelines(n, pipelines);
    _post_call_gles2_callback(NULL, "glDeleteProgramPipelines", (GLADapiproc) glad_glDeleteProgramPipelines, 2, n, pipelines);
    
}
PFNGLDELETEPROGRAMPIPELINESPROC glad_debug_glDeleteProgramPipelines = glad_debug_impl_glDeleteProgramPipelines;
PFNGLDELETEPROGRAMPIPELINESEXTPROC glad_glDeleteProgramPipelinesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteProgramPipelinesEXT(GLsizei n, const GLuint * pipelines) {
    _pre_call_gles2_callback("glDeleteProgramPipelinesEXT", (GLADapiproc) glad_glDeleteProgramPipelinesEXT, 2, n, pipelines);
    glad_glDeleteProgramPipelinesEXT(n, pipelines);
    _post_call_gles2_callback(NULL, "glDeleteProgramPipelinesEXT", (GLADapiproc) glad_glDeleteProgramPipelinesEXT, 2, n, pipelines);
    
}
PFNGLDELETEPROGRAMPIPELINESEXTPROC glad_debug_glDeleteProgramPipelinesEXT = glad_debug_impl_glDeleteProgramPipelinesEXT;
PFNGLDELETEQUERIESPROC glad_glDeleteQueries = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteQueries(GLsizei n, const GLuint * ids) {
    _pre_call_gles2_callback("glDeleteQueries", (GLADapiproc) glad_glDeleteQueries, 2, n, ids);
    glad_glDeleteQueries(n, ids);
    _post_call_gles2_callback(NULL, "glDeleteQueries", (GLADapiproc) glad_glDeleteQueries, 2, n, ids);
    
}
PFNGLDELETEQUERIESPROC glad_debug_glDeleteQueries = glad_debug_impl_glDeleteQueries;
PFNGLDELETEQUERIESEXTPROC glad_glDeleteQueriesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteQueriesEXT(GLsizei n, const GLuint * ids) {
    _pre_call_gles2_callback("glDeleteQueriesEXT", (GLADapiproc) glad_glDeleteQueriesEXT, 2, n, ids);
    glad_glDeleteQueriesEXT(n, ids);
    _post_call_gles2_callback(NULL, "glDeleteQueriesEXT", (GLADapiproc) glad_glDeleteQueriesEXT, 2, n, ids);
    
}
PFNGLDELETEQUERIESEXTPROC glad_debug_glDeleteQueriesEXT = glad_debug_impl_glDeleteQueriesEXT;
PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteRenderbuffers(GLsizei n, const GLuint * renderbuffers) {
    _pre_call_gles2_callback("glDeleteRenderbuffers", (GLADapiproc) glad_glDeleteRenderbuffers, 2, n, renderbuffers);
    glad_glDeleteRenderbuffers(n, renderbuffers);
    _post_call_gles2_callback(NULL, "glDeleteRenderbuffers", (GLADapiproc) glad_glDeleteRenderbuffers, 2, n, renderbuffers);
    
}
PFNGLDELETERENDERBUFFERSPROC glad_debug_glDeleteRenderbuffers = glad_debug_impl_glDeleteRenderbuffers;
PFNGLDELETESAMPLERSPROC glad_glDeleteSamplers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteSamplers(GLsizei count, const GLuint * samplers) {
    _pre_call_gles2_callback("glDeleteSamplers", (GLADapiproc) glad_glDeleteSamplers, 2, count, samplers);
    glad_glDeleteSamplers(count, samplers);
    _post_call_gles2_callback(NULL, "glDeleteSamplers", (GLADapiproc) glad_glDeleteSamplers, 2, count, samplers);
    
}
PFNGLDELETESAMPLERSPROC glad_debug_glDeleteSamplers = glad_debug_impl_glDeleteSamplers;
PFNGLDELETESEMAPHORESEXTPROC glad_glDeleteSemaphoresEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteSemaphoresEXT(GLsizei n, const GLuint * semaphores) {
    _pre_call_gles2_callback("glDeleteSemaphoresEXT", (GLADapiproc) glad_glDeleteSemaphoresEXT, 2, n, semaphores);
    glad_glDeleteSemaphoresEXT(n, semaphores);
    _post_call_gles2_callback(NULL, "glDeleteSemaphoresEXT", (GLADapiproc) glad_glDeleteSemaphoresEXT, 2, n, semaphores);
    
}
PFNGLDELETESEMAPHORESEXTPROC glad_debug_glDeleteSemaphoresEXT = glad_debug_impl_glDeleteSemaphoresEXT;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteShader(GLuint shader) {
    _pre_call_gles2_callback("glDeleteShader", (GLADapiproc) glad_glDeleteShader, 1, shader);
    glad_glDeleteShader(shader);
    _post_call_gles2_callback(NULL, "glDeleteShader", (GLADapiproc) glad_glDeleteShader, 1, shader);
    
}
PFNGLDELETESHADERPROC glad_debug_glDeleteShader = glad_debug_impl_glDeleteShader;
PFNGLDELETESYNCPROC glad_glDeleteSync = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteSync(GLsync sync) {
    _pre_call_gles2_callback("glDeleteSync", (GLADapiproc) glad_glDeleteSync, 1, sync);
    glad_glDeleteSync(sync);
    _post_call_gles2_callback(NULL, "glDeleteSync", (GLADapiproc) glad_glDeleteSync, 1, sync);
    
}
PFNGLDELETESYNCPROC glad_debug_glDeleteSync = glad_debug_impl_glDeleteSync;
PFNGLDELETESYNCAPPLEPROC glad_glDeleteSyncAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteSyncAPPLE(GLsync sync) {
    _pre_call_gles2_callback("glDeleteSyncAPPLE", (GLADapiproc) glad_glDeleteSyncAPPLE, 1, sync);
    glad_glDeleteSyncAPPLE(sync);
    _post_call_gles2_callback(NULL, "glDeleteSyncAPPLE", (GLADapiproc) glad_glDeleteSyncAPPLE, 1, sync);
    
}
PFNGLDELETESYNCAPPLEPROC glad_debug_glDeleteSyncAPPLE = glad_debug_impl_glDeleteSyncAPPLE;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteTextures(GLsizei n, const GLuint * textures) {
    _pre_call_gles2_callback("glDeleteTextures", (GLADapiproc) glad_glDeleteTextures, 2, n, textures);
    glad_glDeleteTextures(n, textures);
    _post_call_gles2_callback(NULL, "glDeleteTextures", (GLADapiproc) glad_glDeleteTextures, 2, n, textures);
    
}
PFNGLDELETETEXTURESPROC glad_debug_glDeleteTextures = glad_debug_impl_glDeleteTextures;
PFNGLDELETETRANSFORMFEEDBACKSPROC glad_glDeleteTransformFeedbacks = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteTransformFeedbacks(GLsizei n, const GLuint * ids) {
    _pre_call_gles2_callback("glDeleteTransformFeedbacks", (GLADapiproc) glad_glDeleteTransformFeedbacks, 2, n, ids);
    glad_glDeleteTransformFeedbacks(n, ids);
    _post_call_gles2_callback(NULL, "glDeleteTransformFeedbacks", (GLADapiproc) glad_glDeleteTransformFeedbacks, 2, n, ids);
    
}
PFNGLDELETETRANSFORMFEEDBACKSPROC glad_debug_glDeleteTransformFeedbacks = glad_debug_impl_glDeleteTransformFeedbacks;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteVertexArrays(GLsizei n, const GLuint * arrays) {
    _pre_call_gles2_callback("glDeleteVertexArrays", (GLADapiproc) glad_glDeleteVertexArrays, 2, n, arrays);
    glad_glDeleteVertexArrays(n, arrays);
    _post_call_gles2_callback(NULL, "glDeleteVertexArrays", (GLADapiproc) glad_glDeleteVertexArrays, 2, n, arrays);
    
}
PFNGLDELETEVERTEXARRAYSPROC glad_debug_glDeleteVertexArrays = glad_debug_impl_glDeleteVertexArrays;
PFNGLDELETEVERTEXARRAYSOESPROC glad_glDeleteVertexArraysOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDeleteVertexArraysOES(GLsizei n, const GLuint * arrays) {
    _pre_call_gles2_callback("glDeleteVertexArraysOES", (GLADapiproc) glad_glDeleteVertexArraysOES, 2, n, arrays);
    glad_glDeleteVertexArraysOES(n, arrays);
    _post_call_gles2_callback(NULL, "glDeleteVertexArraysOES", (GLADapiproc) glad_glDeleteVertexArraysOES, 2, n, arrays);
    
}
PFNGLDELETEVERTEXARRAYSOESPROC glad_debug_glDeleteVertexArraysOES = glad_debug_impl_glDeleteVertexArraysOES;
PFNGLDEPTHFUNCPROC glad_glDepthFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthFunc(GLenum func) {
    _pre_call_gles2_callback("glDepthFunc", (GLADapiproc) glad_glDepthFunc, 1, func);
    glad_glDepthFunc(func);
    _post_call_gles2_callback(NULL, "glDepthFunc", (GLADapiproc) glad_glDepthFunc, 1, func);
    
}
PFNGLDEPTHFUNCPROC glad_debug_glDepthFunc = glad_debug_impl_glDepthFunc;
PFNGLDEPTHMASKPROC glad_glDepthMask = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthMask(GLboolean flag) {
    _pre_call_gles2_callback("glDepthMask", (GLADapiproc) glad_glDepthMask, 1, flag);
    glad_glDepthMask(flag);
    _post_call_gles2_callback(NULL, "glDepthMask", (GLADapiproc) glad_glDepthMask, 1, flag);
    
}
PFNGLDEPTHMASKPROC glad_debug_glDepthMask = glad_debug_impl_glDepthMask;
PFNGLDEPTHRANGEARRAYFVNVPROC glad_glDepthRangeArrayfvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRangeArrayfvNV(GLuint first, GLsizei count, const GLfloat * v) {
    _pre_call_gles2_callback("glDepthRangeArrayfvNV", (GLADapiproc) glad_glDepthRangeArrayfvNV, 3, first, count, v);
    glad_glDepthRangeArrayfvNV(first, count, v);
    _post_call_gles2_callback(NULL, "glDepthRangeArrayfvNV", (GLADapiproc) glad_glDepthRangeArrayfvNV, 3, first, count, v);
    
}
PFNGLDEPTHRANGEARRAYFVNVPROC glad_debug_glDepthRangeArrayfvNV = glad_debug_impl_glDepthRangeArrayfvNV;
PFNGLDEPTHRANGEARRAYFVOESPROC glad_glDepthRangeArrayfvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRangeArrayfvOES(GLuint first, GLsizei count, const GLfloat * v) {
    _pre_call_gles2_callback("glDepthRangeArrayfvOES", (GLADapiproc) glad_glDepthRangeArrayfvOES, 3, first, count, v);
    glad_glDepthRangeArrayfvOES(first, count, v);
    _post_call_gles2_callback(NULL, "glDepthRangeArrayfvOES", (GLADapiproc) glad_glDepthRangeArrayfvOES, 3, first, count, v);
    
}
PFNGLDEPTHRANGEARRAYFVOESPROC glad_debug_glDepthRangeArrayfvOES = glad_debug_impl_glDepthRangeArrayfvOES;
PFNGLDEPTHRANGEINDEXEDFNVPROC glad_glDepthRangeIndexedfNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRangeIndexedfNV(GLuint index, GLfloat n, GLfloat f) {
    _pre_call_gles2_callback("glDepthRangeIndexedfNV", (GLADapiproc) glad_glDepthRangeIndexedfNV, 3, index, n, f);
    glad_glDepthRangeIndexedfNV(index, n, f);
    _post_call_gles2_callback(NULL, "glDepthRangeIndexedfNV", (GLADapiproc) glad_glDepthRangeIndexedfNV, 3, index, n, f);
    
}
PFNGLDEPTHRANGEINDEXEDFNVPROC glad_debug_glDepthRangeIndexedfNV = glad_debug_impl_glDepthRangeIndexedfNV;
PFNGLDEPTHRANGEINDEXEDFOESPROC glad_glDepthRangeIndexedfOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRangeIndexedfOES(GLuint index, GLfloat n, GLfloat f) {
    _pre_call_gles2_callback("glDepthRangeIndexedfOES", (GLADapiproc) glad_glDepthRangeIndexedfOES, 3, index, n, f);
    glad_glDepthRangeIndexedfOES(index, n, f);
    _post_call_gles2_callback(NULL, "glDepthRangeIndexedfOES", (GLADapiproc) glad_glDepthRangeIndexedfOES, 3, index, n, f);
    
}
PFNGLDEPTHRANGEINDEXEDFOESPROC glad_debug_glDepthRangeIndexedfOES = glad_debug_impl_glDepthRangeIndexedfOES;
PFNGLDEPTHRANGEFPROC glad_glDepthRangef = NULL;
static void GLAD_API_PTR glad_debug_impl_glDepthRangef(GLfloat n, GLfloat f) {
    _pre_call_gles2_callback("glDepthRangef", (GLADapiproc) glad_glDepthRangef, 2, n, f);
    glad_glDepthRangef(n, f);
    _post_call_gles2_callback(NULL, "glDepthRangef", (GLADapiproc) glad_glDepthRangef, 2, n, f);
    
}
PFNGLDEPTHRANGEFPROC glad_debug_glDepthRangef = glad_debug_impl_glDepthRangef;
PFNGLDETACHSHADERPROC glad_glDetachShader = NULL;
static void GLAD_API_PTR glad_debug_impl_glDetachShader(GLuint program, GLuint shader) {
    _pre_call_gles2_callback("glDetachShader", (GLADapiproc) glad_glDetachShader, 2, program, shader);
    glad_glDetachShader(program, shader);
    _post_call_gles2_callback(NULL, "glDetachShader", (GLADapiproc) glad_glDetachShader, 2, program, shader);
    
}
PFNGLDETACHSHADERPROC glad_debug_glDetachShader = glad_debug_impl_glDetachShader;
PFNGLDISABLEPROC glad_glDisable = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisable(GLenum cap) {
    _pre_call_gles2_callback("glDisable", (GLADapiproc) glad_glDisable, 1, cap);
    glad_glDisable(cap);
    _post_call_gles2_callback(NULL, "glDisable", (GLADapiproc) glad_glDisable, 1, cap);
    
}
PFNGLDISABLEPROC glad_debug_glDisable = glad_debug_impl_glDisable;
PFNGLDISABLEDRIVERCONTROLQCOMPROC glad_glDisableDriverControlQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableDriverControlQCOM(GLuint driverControl) {
    _pre_call_gles2_callback("glDisableDriverControlQCOM", (GLADapiproc) glad_glDisableDriverControlQCOM, 1, driverControl);
    glad_glDisableDriverControlQCOM(driverControl);
    _post_call_gles2_callback(NULL, "glDisableDriverControlQCOM", (GLADapiproc) glad_glDisableDriverControlQCOM, 1, driverControl);
    
}
PFNGLDISABLEDRIVERCONTROLQCOMPROC glad_debug_glDisableDriverControlQCOM = glad_debug_impl_glDisableDriverControlQCOM;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableVertexAttribArray(GLuint index) {
    _pre_call_gles2_callback("glDisableVertexAttribArray", (GLADapiproc) glad_glDisableVertexAttribArray, 1, index);
    glad_glDisableVertexAttribArray(index);
    _post_call_gles2_callback(NULL, "glDisableVertexAttribArray", (GLADapiproc) glad_glDisableVertexAttribArray, 1, index);
    
}
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_debug_glDisableVertexAttribArray = glad_debug_impl_glDisableVertexAttribArray;
PFNGLDISABLEIPROC glad_glDisablei = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisablei(GLenum target, GLuint index) {
    _pre_call_gles2_callback("glDisablei", (GLADapiproc) glad_glDisablei, 2, target, index);
    glad_glDisablei(target, index);
    _post_call_gles2_callback(NULL, "glDisablei", (GLADapiproc) glad_glDisablei, 2, target, index);
    
}
PFNGLDISABLEIPROC glad_debug_glDisablei = glad_debug_impl_glDisablei;
PFNGLDISABLEIEXTPROC glad_glDisableiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableiEXT(GLenum target, GLuint index) {
    _pre_call_gles2_callback("glDisableiEXT", (GLADapiproc) glad_glDisableiEXT, 2, target, index);
    glad_glDisableiEXT(target, index);
    _post_call_gles2_callback(NULL, "glDisableiEXT", (GLADapiproc) glad_glDisableiEXT, 2, target, index);
    
}
PFNGLDISABLEIEXTPROC glad_debug_glDisableiEXT = glad_debug_impl_glDisableiEXT;
PFNGLDISABLEINVPROC glad_glDisableiNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableiNV(GLenum target, GLuint index) {
    _pre_call_gles2_callback("glDisableiNV", (GLADapiproc) glad_glDisableiNV, 2, target, index);
    glad_glDisableiNV(target, index);
    _post_call_gles2_callback(NULL, "glDisableiNV", (GLADapiproc) glad_glDisableiNV, 2, target, index);
    
}
PFNGLDISABLEINVPROC glad_debug_glDisableiNV = glad_debug_impl_glDisableiNV;
PFNGLDISABLEIOESPROC glad_glDisableiOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDisableiOES(GLenum target, GLuint index) {
    _pre_call_gles2_callback("glDisableiOES", (GLADapiproc) glad_glDisableiOES, 2, target, index);
    glad_glDisableiOES(target, index);
    _post_call_gles2_callback(NULL, "glDisableiOES", (GLADapiproc) glad_glDisableiOES, 2, target, index);
    
}
PFNGLDISABLEIOESPROC glad_debug_glDisableiOES = glad_debug_impl_glDisableiOES;
PFNGLDISCARDFRAMEBUFFEREXTPROC glad_glDiscardFramebufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDiscardFramebufferEXT(GLenum target, GLsizei numAttachments, const GLenum * attachments) {
    _pre_call_gles2_callback("glDiscardFramebufferEXT", (GLADapiproc) glad_glDiscardFramebufferEXT, 3, target, numAttachments, attachments);
    glad_glDiscardFramebufferEXT(target, numAttachments, attachments);
    _post_call_gles2_callback(NULL, "glDiscardFramebufferEXT", (GLADapiproc) glad_glDiscardFramebufferEXT, 3, target, numAttachments, attachments);
    
}
PFNGLDISCARDFRAMEBUFFEREXTPROC glad_debug_glDiscardFramebufferEXT = glad_debug_impl_glDiscardFramebufferEXT;
PFNGLDISPATCHCOMPUTEPROC glad_glDispatchCompute = NULL;
static void GLAD_API_PTR glad_debug_impl_glDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) {
    _pre_call_gles2_callback("glDispatchCompute", (GLADapiproc) glad_glDispatchCompute, 3, num_groups_x, num_groups_y, num_groups_z);
    glad_glDispatchCompute(num_groups_x, num_groups_y, num_groups_z);
    _post_call_gles2_callback(NULL, "glDispatchCompute", (GLADapiproc) glad_glDispatchCompute, 3, num_groups_x, num_groups_y, num_groups_z);
    
}
PFNGLDISPATCHCOMPUTEPROC glad_debug_glDispatchCompute = glad_debug_impl_glDispatchCompute;
PFNGLDISPATCHCOMPUTEINDIRECTPROC glad_glDispatchComputeIndirect = NULL;
static void GLAD_API_PTR glad_debug_impl_glDispatchComputeIndirect(GLintptr indirect) {
    _pre_call_gles2_callback("glDispatchComputeIndirect", (GLADapiproc) glad_glDispatchComputeIndirect, 1, indirect);
    glad_glDispatchComputeIndirect(indirect);
    _post_call_gles2_callback(NULL, "glDispatchComputeIndirect", (GLADapiproc) glad_glDispatchComputeIndirect, 1, indirect);
    
}
PFNGLDISPATCHCOMPUTEINDIRECTPROC glad_debug_glDispatchComputeIndirect = glad_debug_impl_glDispatchComputeIndirect;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    _pre_call_gles2_callback("glDrawArrays", (GLADapiproc) glad_glDrawArrays, 3, mode, first, count);
    glad_glDrawArrays(mode, first, count);
    _post_call_gles2_callback(NULL, "glDrawArrays", (GLADapiproc) glad_glDrawArrays, 3, mode, first, count);
    
}
PFNGLDRAWARRAYSPROC glad_debug_glDrawArrays = glad_debug_impl_glDrawArrays;
PFNGLDRAWARRAYSINDIRECTPROC glad_glDrawArraysIndirect = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysIndirect(GLenum mode, const void * indirect) {
    _pre_call_gles2_callback("glDrawArraysIndirect", (GLADapiproc) glad_glDrawArraysIndirect, 2, mode, indirect);
    glad_glDrawArraysIndirect(mode, indirect);
    _post_call_gles2_callback(NULL, "glDrawArraysIndirect", (GLADapiproc) glad_glDrawArraysIndirect, 2, mode, indirect);
    
}
PFNGLDRAWARRAYSINDIRECTPROC glad_debug_glDrawArraysIndirect = glad_debug_impl_glDrawArraysIndirect;
PFNGLDRAWARRAYSINSTANCEDPROC glad_glDrawArraysInstanced = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount) {
    _pre_call_gles2_callback("glDrawArraysInstanced", (GLADapiproc) glad_glDrawArraysInstanced, 4, mode, first, count, instancecount);
    glad_glDrawArraysInstanced(mode, first, count, instancecount);
    _post_call_gles2_callback(NULL, "glDrawArraysInstanced", (GLADapiproc) glad_glDrawArraysInstanced, 4, mode, first, count, instancecount);
    
}
PFNGLDRAWARRAYSINSTANCEDPROC glad_debug_glDrawArraysInstanced = glad_debug_impl_glDrawArraysInstanced;
PFNGLDRAWARRAYSINSTANCEDANGLEPROC glad_glDrawArraysInstancedANGLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysInstancedANGLE(GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
    _pre_call_gles2_callback("glDrawArraysInstancedANGLE", (GLADapiproc) glad_glDrawArraysInstancedANGLE, 4, mode, first, count, primcount);
    glad_glDrawArraysInstancedANGLE(mode, first, count, primcount);
    _post_call_gles2_callback(NULL, "glDrawArraysInstancedANGLE", (GLADapiproc) glad_glDrawArraysInstancedANGLE, 4, mode, first, count, primcount);
    
}
PFNGLDRAWARRAYSINSTANCEDANGLEPROC glad_debug_glDrawArraysInstancedANGLE = glad_debug_impl_glDrawArraysInstancedANGLE;
PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC glad_glDrawArraysInstancedBaseInstanceEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysInstancedBaseInstanceEXT(GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance) {
    _pre_call_gles2_callback("glDrawArraysInstancedBaseInstanceEXT", (GLADapiproc) glad_glDrawArraysInstancedBaseInstanceEXT, 5, mode, first, count, instancecount, baseinstance);
    glad_glDrawArraysInstancedBaseInstanceEXT(mode, first, count, instancecount, baseinstance);
    _post_call_gles2_callback(NULL, "glDrawArraysInstancedBaseInstanceEXT", (GLADapiproc) glad_glDrawArraysInstancedBaseInstanceEXT, 5, mode, first, count, instancecount, baseinstance);
    
}
PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC glad_debug_glDrawArraysInstancedBaseInstanceEXT = glad_debug_impl_glDrawArraysInstancedBaseInstanceEXT;
PFNGLDRAWARRAYSINSTANCEDEXTPROC glad_glDrawArraysInstancedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysInstancedEXT(GLenum mode, GLint start, GLsizei count, GLsizei primcount) {
    _pre_call_gles2_callback("glDrawArraysInstancedEXT", (GLADapiproc) glad_glDrawArraysInstancedEXT, 4, mode, start, count, primcount);
    glad_glDrawArraysInstancedEXT(mode, start, count, primcount);
    _post_call_gles2_callback(NULL, "glDrawArraysInstancedEXT", (GLADapiproc) glad_glDrawArraysInstancedEXT, 4, mode, start, count, primcount);
    
}
PFNGLDRAWARRAYSINSTANCEDEXTPROC glad_debug_glDrawArraysInstancedEXT = glad_debug_impl_glDrawArraysInstancedEXT;
PFNGLDRAWARRAYSINSTANCEDNVPROC glad_glDrawArraysInstancedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawArraysInstancedNV(GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
    _pre_call_gles2_callback("glDrawArraysInstancedNV", (GLADapiproc) glad_glDrawArraysInstancedNV, 4, mode, first, count, primcount);
    glad_glDrawArraysInstancedNV(mode, first, count, primcount);
    _post_call_gles2_callback(NULL, "glDrawArraysInstancedNV", (GLADapiproc) glad_glDrawArraysInstancedNV, 4, mode, first, count, primcount);
    
}
PFNGLDRAWARRAYSINSTANCEDNVPROC glad_debug_glDrawArraysInstancedNV = glad_debug_impl_glDrawArraysInstancedNV;
PFNGLDRAWBUFFERSPROC glad_glDrawBuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawBuffers(GLsizei n, const GLenum * bufs) {
    _pre_call_gles2_callback("glDrawBuffers", (GLADapiproc) glad_glDrawBuffers, 2, n, bufs);
    glad_glDrawBuffers(n, bufs);
    _post_call_gles2_callback(NULL, "glDrawBuffers", (GLADapiproc) glad_glDrawBuffers, 2, n, bufs);
    
}
PFNGLDRAWBUFFERSPROC glad_debug_glDrawBuffers = glad_debug_impl_glDrawBuffers;
PFNGLDRAWBUFFERSEXTPROC glad_glDrawBuffersEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawBuffersEXT(GLsizei n, const GLenum * bufs) {
    _pre_call_gles2_callback("glDrawBuffersEXT", (GLADapiproc) glad_glDrawBuffersEXT, 2, n, bufs);
    glad_glDrawBuffersEXT(n, bufs);
    _post_call_gles2_callback(NULL, "glDrawBuffersEXT", (GLADapiproc) glad_glDrawBuffersEXT, 2, n, bufs);
    
}
PFNGLDRAWBUFFERSEXTPROC glad_debug_glDrawBuffersEXT = glad_debug_impl_glDrawBuffersEXT;
PFNGLDRAWBUFFERSINDEXEDEXTPROC glad_glDrawBuffersIndexedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawBuffersIndexedEXT(GLint n, const GLenum * location, const GLint * indices) {
    _pre_call_gles2_callback("glDrawBuffersIndexedEXT", (GLADapiproc) glad_glDrawBuffersIndexedEXT, 3, n, location, indices);
    glad_glDrawBuffersIndexedEXT(n, location, indices);
    _post_call_gles2_callback(NULL, "glDrawBuffersIndexedEXT", (GLADapiproc) glad_glDrawBuffersIndexedEXT, 3, n, location, indices);
    
}
PFNGLDRAWBUFFERSINDEXEDEXTPROC glad_debug_glDrawBuffersIndexedEXT = glad_debug_impl_glDrawBuffersIndexedEXT;
PFNGLDRAWBUFFERSNVPROC glad_glDrawBuffersNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawBuffersNV(GLsizei n, const GLenum * bufs) {
    _pre_call_gles2_callback("glDrawBuffersNV", (GLADapiproc) glad_glDrawBuffersNV, 2, n, bufs);
    glad_glDrawBuffersNV(n, bufs);
    _post_call_gles2_callback(NULL, "glDrawBuffersNV", (GLADapiproc) glad_glDrawBuffersNV, 2, n, bufs);
    
}
PFNGLDRAWBUFFERSNVPROC glad_debug_glDrawBuffersNV = glad_debug_impl_glDrawBuffersNV;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void * indices) {
    _pre_call_gles2_callback("glDrawElements", (GLADapiproc) glad_glDrawElements, 4, mode, count, type, indices);
    glad_glDrawElements(mode, count, type, indices);
    _post_call_gles2_callback(NULL, "glDrawElements", (GLADapiproc) glad_glDrawElements, 4, mode, count, type, indices);
    
}
PFNGLDRAWELEMENTSPROC glad_debug_glDrawElements = glad_debug_impl_glDrawElements;
PFNGLDRAWELEMENTSBASEVERTEXPROC glad_glDrawElementsBaseVertex = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void * indices, GLint basevertex) {
    _pre_call_gles2_callback("glDrawElementsBaseVertex", (GLADapiproc) glad_glDrawElementsBaseVertex, 5, mode, count, type, indices, basevertex);
    glad_glDrawElementsBaseVertex(mode, count, type, indices, basevertex);
    _post_call_gles2_callback(NULL, "glDrawElementsBaseVertex", (GLADapiproc) glad_glDrawElementsBaseVertex, 5, mode, count, type, indices, basevertex);
    
}
PFNGLDRAWELEMENTSBASEVERTEXPROC glad_debug_glDrawElementsBaseVertex = glad_debug_impl_glDrawElementsBaseVertex;
PFNGLDRAWELEMENTSBASEVERTEXEXTPROC glad_glDrawElementsBaseVertexEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsBaseVertexEXT(GLenum mode, GLsizei count, GLenum type, const void * indices, GLint basevertex) {
    _pre_call_gles2_callback("glDrawElementsBaseVertexEXT", (GLADapiproc) glad_glDrawElementsBaseVertexEXT, 5, mode, count, type, indices, basevertex);
    glad_glDrawElementsBaseVertexEXT(mode, count, type, indices, basevertex);
    _post_call_gles2_callback(NULL, "glDrawElementsBaseVertexEXT", (GLADapiproc) glad_glDrawElementsBaseVertexEXT, 5, mode, count, type, indices, basevertex);
    
}
PFNGLDRAWELEMENTSBASEVERTEXEXTPROC glad_debug_glDrawElementsBaseVertexEXT = glad_debug_impl_glDrawElementsBaseVertexEXT;
PFNGLDRAWELEMENTSBASEVERTEXOESPROC glad_glDrawElementsBaseVertexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsBaseVertexOES(GLenum mode, GLsizei count, GLenum type, const void * indices, GLint basevertex) {
    _pre_call_gles2_callback("glDrawElementsBaseVertexOES", (GLADapiproc) glad_glDrawElementsBaseVertexOES, 5, mode, count, type, indices, basevertex);
    glad_glDrawElementsBaseVertexOES(mode, count, type, indices, basevertex);
    _post_call_gles2_callback(NULL, "glDrawElementsBaseVertexOES", (GLADapiproc) glad_glDrawElementsBaseVertexOES, 5, mode, count, type, indices, basevertex);
    
}
PFNGLDRAWELEMENTSBASEVERTEXOESPROC glad_debug_glDrawElementsBaseVertexOES = glad_debug_impl_glDrawElementsBaseVertexOES;
PFNGLDRAWELEMENTSINDIRECTPROC glad_glDrawElementsIndirect = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsIndirect(GLenum mode, GLenum type, const void * indirect) {
    _pre_call_gles2_callback("glDrawElementsIndirect", (GLADapiproc) glad_glDrawElementsIndirect, 3, mode, type, indirect);
    glad_glDrawElementsIndirect(mode, type, indirect);
    _post_call_gles2_callback(NULL, "glDrawElementsIndirect", (GLADapiproc) glad_glDrawElementsIndirect, 3, mode, type, indirect);
    
}
PFNGLDRAWELEMENTSINDIRECTPROC glad_debug_glDrawElementsIndirect = glad_debug_impl_glDrawElementsIndirect;
PFNGLDRAWELEMENTSINSTANCEDPROC glad_glDrawElementsInstanced = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount) {
    _pre_call_gles2_callback("glDrawElementsInstanced", (GLADapiproc) glad_glDrawElementsInstanced, 5, mode, count, type, indices, instancecount);
    glad_glDrawElementsInstanced(mode, count, type, indices, instancecount);
    _post_call_gles2_callback(NULL, "glDrawElementsInstanced", (GLADapiproc) glad_glDrawElementsInstanced, 5, mode, count, type, indices, instancecount);
    
}
PFNGLDRAWELEMENTSINSTANCEDPROC glad_debug_glDrawElementsInstanced = glad_debug_impl_glDrawElementsInstanced;
PFNGLDRAWELEMENTSINSTANCEDANGLEPROC glad_glDrawElementsInstancedANGLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedANGLE(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei primcount) {
    _pre_call_gles2_callback("glDrawElementsInstancedANGLE", (GLADapiproc) glad_glDrawElementsInstancedANGLE, 5, mode, count, type, indices, primcount);
    glad_glDrawElementsInstancedANGLE(mode, count, type, indices, primcount);
    _post_call_gles2_callback(NULL, "glDrawElementsInstancedANGLE", (GLADapiproc) glad_glDrawElementsInstancedANGLE, 5, mode, count, type, indices, primcount);
    
}
PFNGLDRAWELEMENTSINSTANCEDANGLEPROC glad_debug_glDrawElementsInstancedANGLE = glad_debug_impl_glDrawElementsInstancedANGLE;
PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC glad_glDrawElementsInstancedBaseInstanceEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedBaseInstanceEXT(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLuint baseinstance) {
    _pre_call_gles2_callback("glDrawElementsInstancedBaseInstanceEXT", (GLADapiproc) glad_glDrawElementsInstancedBaseInstanceEXT, 6, mode, count, type, indices, instancecount, baseinstance);
    glad_glDrawElementsInstancedBaseInstanceEXT(mode, count, type, indices, instancecount, baseinstance);
    _post_call_gles2_callback(NULL, "glDrawElementsInstancedBaseInstanceEXT", (GLADapiproc) glad_glDrawElementsInstancedBaseInstanceEXT, 6, mode, count, type, indices, instancecount, baseinstance);
    
}
PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC glad_debug_glDrawElementsInstancedBaseInstanceEXT = glad_debug_impl_glDrawElementsInstancedBaseInstanceEXT;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glad_glDrawElementsInstancedBaseVertex = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLint basevertex) {
    _pre_call_gles2_callback("glDrawElementsInstancedBaseVertex", (GLADapiproc) glad_glDrawElementsInstancedBaseVertex, 6, mode, count, type, indices, instancecount, basevertex);
    glad_glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);
    _post_call_gles2_callback(NULL, "glDrawElementsInstancedBaseVertex", (GLADapiproc) glad_glDrawElementsInstancedBaseVertex, 6, mode, count, type, indices, instancecount, basevertex);
    
}
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glad_debug_glDrawElementsInstancedBaseVertex = glad_debug_impl_glDrawElementsInstancedBaseVertex;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC glad_glDrawElementsInstancedBaseVertexBaseInstanceEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedBaseVertexBaseInstanceEXT(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance) {
    _pre_call_gles2_callback("glDrawElementsInstancedBaseVertexBaseInstanceEXT", (GLADapiproc) glad_glDrawElementsInstancedBaseVertexBaseInstanceEXT, 7, mode, count, type, indices, instancecount, basevertex, baseinstance);
    glad_glDrawElementsInstancedBaseVertexBaseInstanceEXT(mode, count, type, indices, instancecount, basevertex, baseinstance);
    _post_call_gles2_callback(NULL, "glDrawElementsInstancedBaseVertexBaseInstanceEXT", (GLADapiproc) glad_glDrawElementsInstancedBaseVertexBaseInstanceEXT, 7, mode, count, type, indices, instancecount, basevertex, baseinstance);
    
}
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC glad_debug_glDrawElementsInstancedBaseVertexBaseInstanceEXT = glad_debug_impl_glDrawElementsInstancedBaseVertexBaseInstanceEXT;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXEXTPROC glad_glDrawElementsInstancedBaseVertexEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedBaseVertexEXT(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLint basevertex) {
    _pre_call_gles2_callback("glDrawElementsInstancedBaseVertexEXT", (GLADapiproc) glad_glDrawElementsInstancedBaseVertexEXT, 6, mode, count, type, indices, instancecount, basevertex);
    glad_glDrawElementsInstancedBaseVertexEXT(mode, count, type, indices, instancecount, basevertex);
    _post_call_gles2_callback(NULL, "glDrawElementsInstancedBaseVertexEXT", (GLADapiproc) glad_glDrawElementsInstancedBaseVertexEXT, 6, mode, count, type, indices, instancecount, basevertex);
    
}
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXEXTPROC glad_debug_glDrawElementsInstancedBaseVertexEXT = glad_debug_impl_glDrawElementsInstancedBaseVertexEXT;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC glad_glDrawElementsInstancedBaseVertexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedBaseVertexOES(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei instancecount, GLint basevertex) {
    _pre_call_gles2_callback("glDrawElementsInstancedBaseVertexOES", (GLADapiproc) glad_glDrawElementsInstancedBaseVertexOES, 6, mode, count, type, indices, instancecount, basevertex);
    glad_glDrawElementsInstancedBaseVertexOES(mode, count, type, indices, instancecount, basevertex);
    _post_call_gles2_callback(NULL, "glDrawElementsInstancedBaseVertexOES", (GLADapiproc) glad_glDrawElementsInstancedBaseVertexOES, 6, mode, count, type, indices, instancecount, basevertex);
    
}
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC glad_debug_glDrawElementsInstancedBaseVertexOES = glad_debug_impl_glDrawElementsInstancedBaseVertexOES;
PFNGLDRAWELEMENTSINSTANCEDEXTPROC glad_glDrawElementsInstancedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedEXT(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei primcount) {
    _pre_call_gles2_callback("glDrawElementsInstancedEXT", (GLADapiproc) glad_glDrawElementsInstancedEXT, 5, mode, count, type, indices, primcount);
    glad_glDrawElementsInstancedEXT(mode, count, type, indices, primcount);
    _post_call_gles2_callback(NULL, "glDrawElementsInstancedEXT", (GLADapiproc) glad_glDrawElementsInstancedEXT, 5, mode, count, type, indices, primcount);
    
}
PFNGLDRAWELEMENTSINSTANCEDEXTPROC glad_debug_glDrawElementsInstancedEXT = glad_debug_impl_glDrawElementsInstancedEXT;
PFNGLDRAWELEMENTSINSTANCEDNVPROC glad_glDrawElementsInstancedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawElementsInstancedNV(GLenum mode, GLsizei count, GLenum type, const void * indices, GLsizei primcount) {
    _pre_call_gles2_callback("glDrawElementsInstancedNV", (GLADapiproc) glad_glDrawElementsInstancedNV, 5, mode, count, type, indices, primcount);
    glad_glDrawElementsInstancedNV(mode, count, type, indices, primcount);
    _post_call_gles2_callback(NULL, "glDrawElementsInstancedNV", (GLADapiproc) glad_glDrawElementsInstancedNV, 5, mode, count, type, indices, primcount);
    
}
PFNGLDRAWELEMENTSINSTANCEDNVPROC glad_debug_glDrawElementsInstancedNV = glad_debug_impl_glDrawElementsInstancedNV;
PFNGLDRAWMESHTASKSEXTPROC glad_glDrawMeshTasksEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawMeshTasksEXT(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) {
    _pre_call_gles2_callback("glDrawMeshTasksEXT", (GLADapiproc) glad_glDrawMeshTasksEXT, 3, num_groups_x, num_groups_y, num_groups_z);
    glad_glDrawMeshTasksEXT(num_groups_x, num_groups_y, num_groups_z);
    _post_call_gles2_callback(NULL, "glDrawMeshTasksEXT", (GLADapiproc) glad_glDrawMeshTasksEXT, 3, num_groups_x, num_groups_y, num_groups_z);
    
}
PFNGLDRAWMESHTASKSEXTPROC glad_debug_glDrawMeshTasksEXT = glad_debug_impl_glDrawMeshTasksEXT;
PFNGLDRAWMESHTASKSINDIRECTEXTPROC glad_glDrawMeshTasksIndirectEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawMeshTasksIndirectEXT(GLintptr indirect) {
    _pre_call_gles2_callback("glDrawMeshTasksIndirectEXT", (GLADapiproc) glad_glDrawMeshTasksIndirectEXT, 1, indirect);
    glad_glDrawMeshTasksIndirectEXT(indirect);
    _post_call_gles2_callback(NULL, "glDrawMeshTasksIndirectEXT", (GLADapiproc) glad_glDrawMeshTasksIndirectEXT, 1, indirect);
    
}
PFNGLDRAWMESHTASKSINDIRECTEXTPROC glad_debug_glDrawMeshTasksIndirectEXT = glad_debug_impl_glDrawMeshTasksIndirectEXT;
PFNGLDRAWMESHTASKSINDIRECTNVPROC glad_glDrawMeshTasksIndirectNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawMeshTasksIndirectNV(GLintptr indirect) {
    _pre_call_gles2_callback("glDrawMeshTasksIndirectNV", (GLADapiproc) glad_glDrawMeshTasksIndirectNV, 1, indirect);
    glad_glDrawMeshTasksIndirectNV(indirect);
    _post_call_gles2_callback(NULL, "glDrawMeshTasksIndirectNV", (GLADapiproc) glad_glDrawMeshTasksIndirectNV, 1, indirect);
    
}
PFNGLDRAWMESHTASKSINDIRECTNVPROC glad_debug_glDrawMeshTasksIndirectNV = glad_debug_impl_glDrawMeshTasksIndirectNV;
PFNGLDRAWMESHTASKSNVPROC glad_glDrawMeshTasksNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawMeshTasksNV(GLuint first, GLuint count) {
    _pre_call_gles2_callback("glDrawMeshTasksNV", (GLADapiproc) glad_glDrawMeshTasksNV, 2, first, count);
    glad_glDrawMeshTasksNV(first, count);
    _post_call_gles2_callback(NULL, "glDrawMeshTasksNV", (GLADapiproc) glad_glDrawMeshTasksNV, 2, first, count);
    
}
PFNGLDRAWMESHTASKSNVPROC glad_debug_glDrawMeshTasksNV = glad_debug_impl_glDrawMeshTasksNV;
PFNGLDRAWRANGEELEMENTSPROC glad_glDrawRangeElements = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void * indices) {
    _pre_call_gles2_callback("glDrawRangeElements", (GLADapiproc) glad_glDrawRangeElements, 6, mode, start, end, count, type, indices);
    glad_glDrawRangeElements(mode, start, end, count, type, indices);
    _post_call_gles2_callback(NULL, "glDrawRangeElements", (GLADapiproc) glad_glDrawRangeElements, 6, mode, start, end, count, type, indices);
    
}
PFNGLDRAWRANGEELEMENTSPROC glad_debug_glDrawRangeElements = glad_debug_impl_glDrawRangeElements;
PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glad_glDrawRangeElementsBaseVertex = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void * indices, GLint basevertex) {
    _pre_call_gles2_callback("glDrawRangeElementsBaseVertex", (GLADapiproc) glad_glDrawRangeElementsBaseVertex, 7, mode, start, end, count, type, indices, basevertex);
    glad_glDrawRangeElementsBaseVertex(mode, start, end, count, type, indices, basevertex);
    _post_call_gles2_callback(NULL, "glDrawRangeElementsBaseVertex", (GLADapiproc) glad_glDrawRangeElementsBaseVertex, 7, mode, start, end, count, type, indices, basevertex);
    
}
PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC glad_debug_glDrawRangeElementsBaseVertex = glad_debug_impl_glDrawRangeElementsBaseVertex;
PFNGLDRAWRANGEELEMENTSBASEVERTEXEXTPROC glad_glDrawRangeElementsBaseVertexEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawRangeElementsBaseVertexEXT(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void * indices, GLint basevertex) {
    _pre_call_gles2_callback("glDrawRangeElementsBaseVertexEXT", (GLADapiproc) glad_glDrawRangeElementsBaseVertexEXT, 7, mode, start, end, count, type, indices, basevertex);
    glad_glDrawRangeElementsBaseVertexEXT(mode, start, end, count, type, indices, basevertex);
    _post_call_gles2_callback(NULL, "glDrawRangeElementsBaseVertexEXT", (GLADapiproc) glad_glDrawRangeElementsBaseVertexEXT, 7, mode, start, end, count, type, indices, basevertex);
    
}
PFNGLDRAWRANGEELEMENTSBASEVERTEXEXTPROC glad_debug_glDrawRangeElementsBaseVertexEXT = glad_debug_impl_glDrawRangeElementsBaseVertexEXT;
PFNGLDRAWRANGEELEMENTSBASEVERTEXOESPROC glad_glDrawRangeElementsBaseVertexOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawRangeElementsBaseVertexOES(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void * indices, GLint basevertex) {
    _pre_call_gles2_callback("glDrawRangeElementsBaseVertexOES", (GLADapiproc) glad_glDrawRangeElementsBaseVertexOES, 7, mode, start, end, count, type, indices, basevertex);
    glad_glDrawRangeElementsBaseVertexOES(mode, start, end, count, type, indices, basevertex);
    _post_call_gles2_callback(NULL, "glDrawRangeElementsBaseVertexOES", (GLADapiproc) glad_glDrawRangeElementsBaseVertexOES, 7, mode, start, end, count, type, indices, basevertex);
    
}
PFNGLDRAWRANGEELEMENTSBASEVERTEXOESPROC glad_debug_glDrawRangeElementsBaseVertexOES = glad_debug_impl_glDrawRangeElementsBaseVertexOES;
PFNGLDRAWTRANSFORMFEEDBACKEXTPROC glad_glDrawTransformFeedbackEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTransformFeedbackEXT(GLenum mode, GLuint id) {
    _pre_call_gles2_callback("glDrawTransformFeedbackEXT", (GLADapiproc) glad_glDrawTransformFeedbackEXT, 2, mode, id);
    glad_glDrawTransformFeedbackEXT(mode, id);
    _post_call_gles2_callback(NULL, "glDrawTransformFeedbackEXT", (GLADapiproc) glad_glDrawTransformFeedbackEXT, 2, mode, id);
    
}
PFNGLDRAWTRANSFORMFEEDBACKEXTPROC glad_debug_glDrawTransformFeedbackEXT = glad_debug_impl_glDrawTransformFeedbackEXT;
PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDEXTPROC glad_glDrawTransformFeedbackInstancedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawTransformFeedbackInstancedEXT(GLenum mode, GLuint id, GLsizei instancecount) {
    _pre_call_gles2_callback("glDrawTransformFeedbackInstancedEXT", (GLADapiproc) glad_glDrawTransformFeedbackInstancedEXT, 3, mode, id, instancecount);
    glad_glDrawTransformFeedbackInstancedEXT(mode, id, instancecount);
    _post_call_gles2_callback(NULL, "glDrawTransformFeedbackInstancedEXT", (GLADapiproc) glad_glDrawTransformFeedbackInstancedEXT, 3, mode, id, instancecount);
    
}
PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDEXTPROC glad_debug_glDrawTransformFeedbackInstancedEXT = glad_debug_impl_glDrawTransformFeedbackInstancedEXT;
PFNGLDRAWVKIMAGENVPROC glad_glDrawVkImageNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glDrawVkImageNV(GLuint64 vkImage, GLuint sampler, GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1, GLfloat z, GLfloat s0, GLfloat t0, GLfloat s1, GLfloat t1) {
    _pre_call_gles2_callback("glDrawVkImageNV", (GLADapiproc) glad_glDrawVkImageNV, 11, vkImage, sampler, x0, y0, x1, y1, z, s0, t0, s1, t1);
    glad_glDrawVkImageNV(vkImage, sampler, x0, y0, x1, y1, z, s0, t0, s1, t1);
    _post_call_gles2_callback(NULL, "glDrawVkImageNV", (GLADapiproc) glad_glDrawVkImageNV, 11, vkImage, sampler, x0, y0, x1, y1, z, s0, t0, s1, t1);
    
}
PFNGLDRAWVKIMAGENVPROC glad_debug_glDrawVkImageNV = glad_debug_impl_glDrawVkImageNV;
PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glad_glEGLImageTargetRenderbufferStorageOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glEGLImageTargetRenderbufferStorageOES(GLenum target, GLeglImageOES image) {
    _pre_call_gles2_callback("glEGLImageTargetRenderbufferStorageOES", (GLADapiproc) glad_glEGLImageTargetRenderbufferStorageOES, 2, target, image);
    glad_glEGLImageTargetRenderbufferStorageOES(target, image);
    _post_call_gles2_callback(NULL, "glEGLImageTargetRenderbufferStorageOES", (GLADapiproc) glad_glEGLImageTargetRenderbufferStorageOES, 2, target, image);
    
}
PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glad_debug_glEGLImageTargetRenderbufferStorageOES = glad_debug_impl_glEGLImageTargetRenderbufferStorageOES;
PFNGLEGLIMAGETARGETTEXSTORAGEEXTPROC glad_glEGLImageTargetTexStorageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEGLImageTargetTexStorageEXT(GLenum target, GLeglImageOES image, const GLint * attrib_list) {
    _pre_call_gles2_callback("glEGLImageTargetTexStorageEXT", (GLADapiproc) glad_glEGLImageTargetTexStorageEXT, 3, target, image, attrib_list);
    glad_glEGLImageTargetTexStorageEXT(target, image, attrib_list);
    _post_call_gles2_callback(NULL, "glEGLImageTargetTexStorageEXT", (GLADapiproc) glad_glEGLImageTargetTexStorageEXT, 3, target, image, attrib_list);
    
}
PFNGLEGLIMAGETARGETTEXSTORAGEEXTPROC glad_debug_glEGLImageTargetTexStorageEXT = glad_debug_impl_glEGLImageTargetTexStorageEXT;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glad_glEGLImageTargetTexture2DOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image) {
    _pre_call_gles2_callback("glEGLImageTargetTexture2DOES", (GLADapiproc) glad_glEGLImageTargetTexture2DOES, 2, target, image);
    glad_glEGLImageTargetTexture2DOES(target, image);
    _post_call_gles2_callback(NULL, "glEGLImageTargetTexture2DOES", (GLADapiproc) glad_glEGLImageTargetTexture2DOES, 2, target, image);
    
}
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glad_debug_glEGLImageTargetTexture2DOES = glad_debug_impl_glEGLImageTargetTexture2DOES;
PFNGLEGLIMAGETARGETTEXTURESTORAGEEXTPROC glad_glEGLImageTargetTextureStorageEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEGLImageTargetTextureStorageEXT(GLuint texture, GLeglImageOES image, const GLint * attrib_list) {
    _pre_call_gles2_callback("glEGLImageTargetTextureStorageEXT", (GLADapiproc) glad_glEGLImageTargetTextureStorageEXT, 3, texture, image, attrib_list);
    glad_glEGLImageTargetTextureStorageEXT(texture, image, attrib_list);
    _post_call_gles2_callback(NULL, "glEGLImageTargetTextureStorageEXT", (GLADapiproc) glad_glEGLImageTargetTextureStorageEXT, 3, texture, image, attrib_list);
    
}
PFNGLEGLIMAGETARGETTEXTURESTORAGEEXTPROC glad_debug_glEGLImageTargetTextureStorageEXT = glad_debug_impl_glEGLImageTargetTextureStorageEXT;
PFNGLENABLEPROC glad_glEnable = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnable(GLenum cap) {
    _pre_call_gles2_callback("glEnable", (GLADapiproc) glad_glEnable, 1, cap);
    glad_glEnable(cap);
    _post_call_gles2_callback(NULL, "glEnable", (GLADapiproc) glad_glEnable, 1, cap);
    
}
PFNGLENABLEPROC glad_debug_glEnable = glad_debug_impl_glEnable;
PFNGLENABLEDRIVERCONTROLQCOMPROC glad_glEnableDriverControlQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableDriverControlQCOM(GLuint driverControl) {
    _pre_call_gles2_callback("glEnableDriverControlQCOM", (GLADapiproc) glad_glEnableDriverControlQCOM, 1, driverControl);
    glad_glEnableDriverControlQCOM(driverControl);
    _post_call_gles2_callback(NULL, "glEnableDriverControlQCOM", (GLADapiproc) glad_glEnableDriverControlQCOM, 1, driverControl);
    
}
PFNGLENABLEDRIVERCONTROLQCOMPROC glad_debug_glEnableDriverControlQCOM = glad_debug_impl_glEnableDriverControlQCOM;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableVertexAttribArray(GLuint index) {
    _pre_call_gles2_callback("glEnableVertexAttribArray", (GLADapiproc) glad_glEnableVertexAttribArray, 1, index);
    glad_glEnableVertexAttribArray(index);
    _post_call_gles2_callback(NULL, "glEnableVertexAttribArray", (GLADapiproc) glad_glEnableVertexAttribArray, 1, index);
    
}
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_debug_glEnableVertexAttribArray = glad_debug_impl_glEnableVertexAttribArray;
PFNGLENABLEIPROC glad_glEnablei = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnablei(GLenum target, GLuint index) {
    _pre_call_gles2_callback("glEnablei", (GLADapiproc) glad_glEnablei, 2, target, index);
    glad_glEnablei(target, index);
    _post_call_gles2_callback(NULL, "glEnablei", (GLADapiproc) glad_glEnablei, 2, target, index);
    
}
PFNGLENABLEIPROC glad_debug_glEnablei = glad_debug_impl_glEnablei;
PFNGLENABLEIEXTPROC glad_glEnableiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableiEXT(GLenum target, GLuint index) {
    _pre_call_gles2_callback("glEnableiEXT", (GLADapiproc) glad_glEnableiEXT, 2, target, index);
    glad_glEnableiEXT(target, index);
    _post_call_gles2_callback(NULL, "glEnableiEXT", (GLADapiproc) glad_glEnableiEXT, 2, target, index);
    
}
PFNGLENABLEIEXTPROC glad_debug_glEnableiEXT = glad_debug_impl_glEnableiEXT;
PFNGLENABLEINVPROC glad_glEnableiNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableiNV(GLenum target, GLuint index) {
    _pre_call_gles2_callback("glEnableiNV", (GLADapiproc) glad_glEnableiNV, 2, target, index);
    glad_glEnableiNV(target, index);
    _post_call_gles2_callback(NULL, "glEnableiNV", (GLADapiproc) glad_glEnableiNV, 2, target, index);
    
}
PFNGLENABLEINVPROC glad_debug_glEnableiNV = glad_debug_impl_glEnableiNV;
PFNGLENABLEIOESPROC glad_glEnableiOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glEnableiOES(GLenum target, GLuint index) {
    _pre_call_gles2_callback("glEnableiOES", (GLADapiproc) glad_glEnableiOES, 2, target, index);
    glad_glEnableiOES(target, index);
    _post_call_gles2_callback(NULL, "glEnableiOES", (GLADapiproc) glad_glEnableiOES, 2, target, index);
    
}
PFNGLENABLEIOESPROC glad_debug_glEnableiOES = glad_debug_impl_glEnableiOES;
PFNGLENDCONDITIONALRENDERNVPROC glad_glEndConditionalRenderNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndConditionalRenderNV(void) {
    _pre_call_gles2_callback("glEndConditionalRenderNV", (GLADapiproc) glad_glEndConditionalRenderNV, 0);
    glad_glEndConditionalRenderNV();
    _post_call_gles2_callback(NULL, "glEndConditionalRenderNV", (GLADapiproc) glad_glEndConditionalRenderNV, 0);
    
}
PFNGLENDCONDITIONALRENDERNVPROC glad_debug_glEndConditionalRenderNV = glad_debug_impl_glEndConditionalRenderNV;
PFNGLENDPERFMONITORAMDPROC glad_glEndPerfMonitorAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndPerfMonitorAMD(GLuint monitor) {
    _pre_call_gles2_callback("glEndPerfMonitorAMD", (GLADapiproc) glad_glEndPerfMonitorAMD, 1, monitor);
    glad_glEndPerfMonitorAMD(monitor);
    _post_call_gles2_callback(NULL, "glEndPerfMonitorAMD", (GLADapiproc) glad_glEndPerfMonitorAMD, 1, monitor);
    
}
PFNGLENDPERFMONITORAMDPROC glad_debug_glEndPerfMonitorAMD = glad_debug_impl_glEndPerfMonitorAMD;
PFNGLENDPERFQUERYINTELPROC glad_glEndPerfQueryINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndPerfQueryINTEL(GLuint queryHandle) {
    _pre_call_gles2_callback("glEndPerfQueryINTEL", (GLADapiproc) glad_glEndPerfQueryINTEL, 1, queryHandle);
    glad_glEndPerfQueryINTEL(queryHandle);
    _post_call_gles2_callback(NULL, "glEndPerfQueryINTEL", (GLADapiproc) glad_glEndPerfQueryINTEL, 1, queryHandle);
    
}
PFNGLENDPERFQUERYINTELPROC glad_debug_glEndPerfQueryINTEL = glad_debug_impl_glEndPerfQueryINTEL;
PFNGLENDQUERYPROC glad_glEndQuery = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndQuery(GLenum target) {
    _pre_call_gles2_callback("glEndQuery", (GLADapiproc) glad_glEndQuery, 1, target);
    glad_glEndQuery(target);
    _post_call_gles2_callback(NULL, "glEndQuery", (GLADapiproc) glad_glEndQuery, 1, target);
    
}
PFNGLENDQUERYPROC glad_debug_glEndQuery = glad_debug_impl_glEndQuery;
PFNGLENDQUERYEXTPROC glad_glEndQueryEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndQueryEXT(GLenum target) {
    _pre_call_gles2_callback("glEndQueryEXT", (GLADapiproc) glad_glEndQueryEXT, 1, target);
    glad_glEndQueryEXT(target);
    _post_call_gles2_callback(NULL, "glEndQueryEXT", (GLADapiproc) glad_glEndQueryEXT, 1, target);
    
}
PFNGLENDQUERYEXTPROC glad_debug_glEndQueryEXT = glad_debug_impl_glEndQueryEXT;
PFNGLENDTILINGQCOMPROC glad_glEndTilingQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndTilingQCOM(GLbitfield preserveMask) {
    _pre_call_gles2_callback("glEndTilingQCOM", (GLADapiproc) glad_glEndTilingQCOM, 1, preserveMask);
    glad_glEndTilingQCOM(preserveMask);
    _post_call_gles2_callback(NULL, "glEndTilingQCOM", (GLADapiproc) glad_glEndTilingQCOM, 1, preserveMask);
    
}
PFNGLENDTILINGQCOMPROC glad_debug_glEndTilingQCOM = glad_debug_impl_glEndTilingQCOM;
PFNGLENDTRANSFORMFEEDBACKPROC glad_glEndTransformFeedback = NULL;
static void GLAD_API_PTR glad_debug_impl_glEndTransformFeedback(void) {
    _pre_call_gles2_callback("glEndTransformFeedback", (GLADapiproc) glad_glEndTransformFeedback, 0);
    glad_glEndTransformFeedback();
    _post_call_gles2_callback(NULL, "glEndTransformFeedback", (GLADapiproc) glad_glEndTransformFeedback, 0);
    
}
PFNGLENDTRANSFORMFEEDBACKPROC glad_debug_glEndTransformFeedback = glad_debug_impl_glEndTransformFeedback;
PFNGLEXTGETBUFFERPOINTERVQCOMPROC glad_glExtGetBufferPointervQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetBufferPointervQCOM(GLenum target, void ** params) {
    _pre_call_gles2_callback("glExtGetBufferPointervQCOM", (GLADapiproc) glad_glExtGetBufferPointervQCOM, 2, target, params);
    glad_glExtGetBufferPointervQCOM(target, params);
    _post_call_gles2_callback(NULL, "glExtGetBufferPointervQCOM", (GLADapiproc) glad_glExtGetBufferPointervQCOM, 2, target, params);
    
}
PFNGLEXTGETBUFFERPOINTERVQCOMPROC glad_debug_glExtGetBufferPointervQCOM = glad_debug_impl_glExtGetBufferPointervQCOM;
PFNGLEXTGETBUFFERSQCOMPROC glad_glExtGetBuffersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetBuffersQCOM(GLuint * buffers, GLint maxBuffers, GLint * numBuffers) {
    _pre_call_gles2_callback("glExtGetBuffersQCOM", (GLADapiproc) glad_glExtGetBuffersQCOM, 3, buffers, maxBuffers, numBuffers);
    glad_glExtGetBuffersQCOM(buffers, maxBuffers, numBuffers);
    _post_call_gles2_callback(NULL, "glExtGetBuffersQCOM", (GLADapiproc) glad_glExtGetBuffersQCOM, 3, buffers, maxBuffers, numBuffers);
    
}
PFNGLEXTGETBUFFERSQCOMPROC glad_debug_glExtGetBuffersQCOM = glad_debug_impl_glExtGetBuffersQCOM;
PFNGLEXTGETFRAMEBUFFERSQCOMPROC glad_glExtGetFramebuffersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetFramebuffersQCOM(GLuint * framebuffers, GLint maxFramebuffers, GLint * numFramebuffers) {
    _pre_call_gles2_callback("glExtGetFramebuffersQCOM", (GLADapiproc) glad_glExtGetFramebuffersQCOM, 3, framebuffers, maxFramebuffers, numFramebuffers);
    glad_glExtGetFramebuffersQCOM(framebuffers, maxFramebuffers, numFramebuffers);
    _post_call_gles2_callback(NULL, "glExtGetFramebuffersQCOM", (GLADapiproc) glad_glExtGetFramebuffersQCOM, 3, framebuffers, maxFramebuffers, numFramebuffers);
    
}
PFNGLEXTGETFRAMEBUFFERSQCOMPROC glad_debug_glExtGetFramebuffersQCOM = glad_debug_impl_glExtGetFramebuffersQCOM;
PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC glad_glExtGetProgramBinarySourceQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetProgramBinarySourceQCOM(GLuint program, GLenum shadertype, GLchar * source, GLint * length) {
    _pre_call_gles2_callback("glExtGetProgramBinarySourceQCOM", (GLADapiproc) glad_glExtGetProgramBinarySourceQCOM, 4, program, shadertype, source, length);
    glad_glExtGetProgramBinarySourceQCOM(program, shadertype, source, length);
    _post_call_gles2_callback(NULL, "glExtGetProgramBinarySourceQCOM", (GLADapiproc) glad_glExtGetProgramBinarySourceQCOM, 4, program, shadertype, source, length);
    
}
PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC glad_debug_glExtGetProgramBinarySourceQCOM = glad_debug_impl_glExtGetProgramBinarySourceQCOM;
PFNGLEXTGETPROGRAMSQCOMPROC glad_glExtGetProgramsQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetProgramsQCOM(GLuint * programs, GLint maxPrograms, GLint * numPrograms) {
    _pre_call_gles2_callback("glExtGetProgramsQCOM", (GLADapiproc) glad_glExtGetProgramsQCOM, 3, programs, maxPrograms, numPrograms);
    glad_glExtGetProgramsQCOM(programs, maxPrograms, numPrograms);
    _post_call_gles2_callback(NULL, "glExtGetProgramsQCOM", (GLADapiproc) glad_glExtGetProgramsQCOM, 3, programs, maxPrograms, numPrograms);
    
}
PFNGLEXTGETPROGRAMSQCOMPROC glad_debug_glExtGetProgramsQCOM = glad_debug_impl_glExtGetProgramsQCOM;
PFNGLEXTGETRENDERBUFFERSQCOMPROC glad_glExtGetRenderbuffersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetRenderbuffersQCOM(GLuint * renderbuffers, GLint maxRenderbuffers, GLint * numRenderbuffers) {
    _pre_call_gles2_callback("glExtGetRenderbuffersQCOM", (GLADapiproc) glad_glExtGetRenderbuffersQCOM, 3, renderbuffers, maxRenderbuffers, numRenderbuffers);
    glad_glExtGetRenderbuffersQCOM(renderbuffers, maxRenderbuffers, numRenderbuffers);
    _post_call_gles2_callback(NULL, "glExtGetRenderbuffersQCOM", (GLADapiproc) glad_glExtGetRenderbuffersQCOM, 3, renderbuffers, maxRenderbuffers, numRenderbuffers);
    
}
PFNGLEXTGETRENDERBUFFERSQCOMPROC glad_debug_glExtGetRenderbuffersQCOM = glad_debug_impl_glExtGetRenderbuffersQCOM;
PFNGLEXTGETSHADERSQCOMPROC glad_glExtGetShadersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetShadersQCOM(GLuint * shaders, GLint maxShaders, GLint * numShaders) {
    _pre_call_gles2_callback("glExtGetShadersQCOM", (GLADapiproc) glad_glExtGetShadersQCOM, 3, shaders, maxShaders, numShaders);
    glad_glExtGetShadersQCOM(shaders, maxShaders, numShaders);
    _post_call_gles2_callback(NULL, "glExtGetShadersQCOM", (GLADapiproc) glad_glExtGetShadersQCOM, 3, shaders, maxShaders, numShaders);
    
}
PFNGLEXTGETSHADERSQCOMPROC glad_debug_glExtGetShadersQCOM = glad_debug_impl_glExtGetShadersQCOM;
PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC glad_glExtGetTexLevelParameterivQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetTexLevelParameterivQCOM(GLuint texture, GLenum face, GLint level, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glExtGetTexLevelParameterivQCOM", (GLADapiproc) glad_glExtGetTexLevelParameterivQCOM, 5, texture, face, level, pname, params);
    glad_glExtGetTexLevelParameterivQCOM(texture, face, level, pname, params);
    _post_call_gles2_callback(NULL, "glExtGetTexLevelParameterivQCOM", (GLADapiproc) glad_glExtGetTexLevelParameterivQCOM, 5, texture, face, level, pname, params);
    
}
PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC glad_debug_glExtGetTexLevelParameterivQCOM = glad_debug_impl_glExtGetTexLevelParameterivQCOM;
PFNGLEXTGETTEXSUBIMAGEQCOMPROC glad_glExtGetTexSubImageQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetTexSubImageQCOM(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void * texels) {
    _pre_call_gles2_callback("glExtGetTexSubImageQCOM", (GLADapiproc) glad_glExtGetTexSubImageQCOM, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, texels);
    glad_glExtGetTexSubImageQCOM(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, texels);
    _post_call_gles2_callback(NULL, "glExtGetTexSubImageQCOM", (GLADapiproc) glad_glExtGetTexSubImageQCOM, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, texels);
    
}
PFNGLEXTGETTEXSUBIMAGEQCOMPROC glad_debug_glExtGetTexSubImageQCOM = glad_debug_impl_glExtGetTexSubImageQCOM;
PFNGLEXTGETTEXTURESQCOMPROC glad_glExtGetTexturesQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtGetTexturesQCOM(GLuint * textures, GLint maxTextures, GLint * numTextures) {
    _pre_call_gles2_callback("glExtGetTexturesQCOM", (GLADapiproc) glad_glExtGetTexturesQCOM, 3, textures, maxTextures, numTextures);
    glad_glExtGetTexturesQCOM(textures, maxTextures, numTextures);
    _post_call_gles2_callback(NULL, "glExtGetTexturesQCOM", (GLADapiproc) glad_glExtGetTexturesQCOM, 3, textures, maxTextures, numTextures);
    
}
PFNGLEXTGETTEXTURESQCOMPROC glad_debug_glExtGetTexturesQCOM = glad_debug_impl_glExtGetTexturesQCOM;
PFNGLEXTISPROGRAMBINARYQCOMPROC glad_glExtIsProgramBinaryQCOM = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glExtIsProgramBinaryQCOM(GLuint program) {
    GLboolean ret;
    _pre_call_gles2_callback("glExtIsProgramBinaryQCOM", (GLADapiproc) glad_glExtIsProgramBinaryQCOM, 1, program);
    ret = glad_glExtIsProgramBinaryQCOM(program);
    _post_call_gles2_callback((void*) &ret, "glExtIsProgramBinaryQCOM", (GLADapiproc) glad_glExtIsProgramBinaryQCOM, 1, program);
    return ret;
}
PFNGLEXTISPROGRAMBINARYQCOMPROC glad_debug_glExtIsProgramBinaryQCOM = glad_debug_impl_glExtIsProgramBinaryQCOM;
PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC glad_glExtTexObjectStateOverrideiQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtTexObjectStateOverrideiQCOM(GLenum target, GLenum pname, GLint param) {
    _pre_call_gles2_callback("glExtTexObjectStateOverrideiQCOM", (GLADapiproc) glad_glExtTexObjectStateOverrideiQCOM, 3, target, pname, param);
    glad_glExtTexObjectStateOverrideiQCOM(target, pname, param);
    _post_call_gles2_callback(NULL, "glExtTexObjectStateOverrideiQCOM", (GLADapiproc) glad_glExtTexObjectStateOverrideiQCOM, 3, target, pname, param);
    
}
PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC glad_debug_glExtTexObjectStateOverrideiQCOM = glad_debug_impl_glExtTexObjectStateOverrideiQCOM;
PFNGLEXTRAPOLATETEX2DQCOMPROC glad_glExtrapolateTex2DQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glExtrapolateTex2DQCOM(GLuint src1, GLuint src2, GLuint output, GLfloat scaleFactor) {
    _pre_call_gles2_callback("glExtrapolateTex2DQCOM", (GLADapiproc) glad_glExtrapolateTex2DQCOM, 4, src1, src2, output, scaleFactor);
    glad_glExtrapolateTex2DQCOM(src1, src2, output, scaleFactor);
    _post_call_gles2_callback(NULL, "glExtrapolateTex2DQCOM", (GLADapiproc) glad_glExtrapolateTex2DQCOM, 4, src1, src2, output, scaleFactor);
    
}
PFNGLEXTRAPOLATETEX2DQCOMPROC glad_debug_glExtrapolateTex2DQCOM = glad_debug_impl_glExtrapolateTex2DQCOM;
PFNGLFENCESYNCPROC glad_glFenceSync = NULL;
static GLsync GLAD_API_PTR glad_debug_impl_glFenceSync(GLenum condition, GLbitfield flags) {
    GLsync ret;
    _pre_call_gles2_callback("glFenceSync", (GLADapiproc) glad_glFenceSync, 2, condition, flags);
    ret = glad_glFenceSync(condition, flags);
    _post_call_gles2_callback((void*) &ret, "glFenceSync", (GLADapiproc) glad_glFenceSync, 2, condition, flags);
    return ret;
}
PFNGLFENCESYNCPROC glad_debug_glFenceSync = glad_debug_impl_glFenceSync;
PFNGLFENCESYNCAPPLEPROC glad_glFenceSyncAPPLE = NULL;
static GLsync GLAD_API_PTR glad_debug_impl_glFenceSyncAPPLE(GLenum condition, GLbitfield flags) {
    GLsync ret;
    _pre_call_gles2_callback("glFenceSyncAPPLE", (GLADapiproc) glad_glFenceSyncAPPLE, 2, condition, flags);
    ret = glad_glFenceSyncAPPLE(condition, flags);
    _post_call_gles2_callback((void*) &ret, "glFenceSyncAPPLE", (GLADapiproc) glad_glFenceSyncAPPLE, 2, condition, flags);
    return ret;
}
PFNGLFENCESYNCAPPLEPROC glad_debug_glFenceSyncAPPLE = glad_debug_impl_glFenceSyncAPPLE;
PFNGLFINISHPROC glad_glFinish = NULL;
static void GLAD_API_PTR glad_debug_impl_glFinish(void) {
    _pre_call_gles2_callback("glFinish", (GLADapiproc) glad_glFinish, 0);
    glad_glFinish();
    _post_call_gles2_callback(NULL, "glFinish", (GLADapiproc) glad_glFinish, 0);
    
}
PFNGLFINISHPROC glad_debug_glFinish = glad_debug_impl_glFinish;
PFNGLFINISHFENCENVPROC glad_glFinishFenceNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glFinishFenceNV(GLuint fence) {
    _pre_call_gles2_callback("glFinishFenceNV", (GLADapiproc) glad_glFinishFenceNV, 1, fence);
    glad_glFinishFenceNV(fence);
    _post_call_gles2_callback(NULL, "glFinishFenceNV", (GLADapiproc) glad_glFinishFenceNV, 1, fence);
    
}
PFNGLFINISHFENCENVPROC glad_debug_glFinishFenceNV = glad_debug_impl_glFinishFenceNV;
PFNGLFLUSHPROC glad_glFlush = NULL;
static void GLAD_API_PTR glad_debug_impl_glFlush(void) {
    _pre_call_gles2_callback("glFlush", (GLADapiproc) glad_glFlush, 0);
    glad_glFlush();
    _post_call_gles2_callback(NULL, "glFlush", (GLADapiproc) glad_glFlush, 0);
    
}
PFNGLFLUSHPROC glad_debug_glFlush = glad_debug_impl_glFlush;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glad_glFlushMappedBufferRange = NULL;
static void GLAD_API_PTR glad_debug_impl_glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    _pre_call_gles2_callback("glFlushMappedBufferRange", (GLADapiproc) glad_glFlushMappedBufferRange, 3, target, offset, length);
    glad_glFlushMappedBufferRange(target, offset, length);
    _post_call_gles2_callback(NULL, "glFlushMappedBufferRange", (GLADapiproc) glad_glFlushMappedBufferRange, 3, target, offset, length);
    
}
PFNGLFLUSHMAPPEDBUFFERRANGEPROC glad_debug_glFlushMappedBufferRange = glad_debug_impl_glFlushMappedBufferRange;
PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC glad_glFlushMappedBufferRangeEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFlushMappedBufferRangeEXT(GLenum target, GLintptr offset, GLsizeiptr length) {
    _pre_call_gles2_callback("glFlushMappedBufferRangeEXT", (GLADapiproc) glad_glFlushMappedBufferRangeEXT, 3, target, offset, length);
    glad_glFlushMappedBufferRangeEXT(target, offset, length);
    _post_call_gles2_callback(NULL, "glFlushMappedBufferRangeEXT", (GLADapiproc) glad_glFlushMappedBufferRangeEXT, 3, target, offset, length);
    
}
PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC glad_debug_glFlushMappedBufferRangeEXT = glad_debug_impl_glFlushMappedBufferRangeEXT;
PFNGLFRAGMENTCOVERAGECOLORNVPROC glad_glFragmentCoverageColorNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glFragmentCoverageColorNV(GLuint color) {
    _pre_call_gles2_callback("glFragmentCoverageColorNV", (GLADapiproc) glad_glFragmentCoverageColorNV, 1, color);
    glad_glFragmentCoverageColorNV(color);
    _post_call_gles2_callback(NULL, "glFragmentCoverageColorNV", (GLADapiproc) glad_glFragmentCoverageColorNV, 1, color);
    
}
PFNGLFRAGMENTCOVERAGECOLORNVPROC glad_debug_glFragmentCoverageColorNV = glad_debug_impl_glFragmentCoverageColorNV;
PFNGLFRAMEBUFFERFETCHBARRIEREXTPROC glad_glFramebufferFetchBarrierEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferFetchBarrierEXT(void) {
    _pre_call_gles2_callback("glFramebufferFetchBarrierEXT", (GLADapiproc) glad_glFramebufferFetchBarrierEXT, 0);
    glad_glFramebufferFetchBarrierEXT();
    _post_call_gles2_callback(NULL, "glFramebufferFetchBarrierEXT", (GLADapiproc) glad_glFramebufferFetchBarrierEXT, 0);
    
}
PFNGLFRAMEBUFFERFETCHBARRIEREXTPROC glad_debug_glFramebufferFetchBarrierEXT = glad_debug_impl_glFramebufferFetchBarrierEXT;
PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC glad_glFramebufferFetchBarrierQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferFetchBarrierQCOM(void) {
    _pre_call_gles2_callback("glFramebufferFetchBarrierQCOM", (GLADapiproc) glad_glFramebufferFetchBarrierQCOM, 0);
    glad_glFramebufferFetchBarrierQCOM();
    _post_call_gles2_callback(NULL, "glFramebufferFetchBarrierQCOM", (GLADapiproc) glad_glFramebufferFetchBarrierQCOM, 0);
    
}
PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC glad_debug_glFramebufferFetchBarrierQCOM = glad_debug_impl_glFramebufferFetchBarrierQCOM;
PFNGLFRAMEBUFFERFOVEATIONCONFIGQCOMPROC glad_glFramebufferFoveationConfigQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferFoveationConfigQCOM(GLuint framebuffer, GLuint numLayers, GLuint focalPointsPerLayer, GLuint requestedFeatures, GLuint * providedFeatures) {
    _pre_call_gles2_callback("glFramebufferFoveationConfigQCOM", (GLADapiproc) glad_glFramebufferFoveationConfigQCOM, 5, framebuffer, numLayers, focalPointsPerLayer, requestedFeatures, providedFeatures);
    glad_glFramebufferFoveationConfigQCOM(framebuffer, numLayers, focalPointsPerLayer, requestedFeatures, providedFeatures);
    _post_call_gles2_callback(NULL, "glFramebufferFoveationConfigQCOM", (GLADapiproc) glad_glFramebufferFoveationConfigQCOM, 5, framebuffer, numLayers, focalPointsPerLayer, requestedFeatures, providedFeatures);
    
}
PFNGLFRAMEBUFFERFOVEATIONCONFIGQCOMPROC glad_debug_glFramebufferFoveationConfigQCOM = glad_debug_impl_glFramebufferFoveationConfigQCOM;
PFNGLFRAMEBUFFERFOVEATIONPARAMETERSQCOMPROC glad_glFramebufferFoveationParametersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferFoveationParametersQCOM(GLuint framebuffer, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea) {
    _pre_call_gles2_callback("glFramebufferFoveationParametersQCOM", (GLADapiproc) glad_glFramebufferFoveationParametersQCOM, 8, framebuffer, layer, focalPoint, focalX, focalY, gainX, gainY, foveaArea);
    glad_glFramebufferFoveationParametersQCOM(framebuffer, layer, focalPoint, focalX, focalY, gainX, gainY, foveaArea);
    _post_call_gles2_callback(NULL, "glFramebufferFoveationParametersQCOM", (GLADapiproc) glad_glFramebufferFoveationParametersQCOM, 8, framebuffer, layer, focalPoint, focalX, focalY, gainX, gainY, foveaArea);
    
}
PFNGLFRAMEBUFFERFOVEATIONPARAMETERSQCOMPROC glad_debug_glFramebufferFoveationParametersQCOM = glad_debug_impl_glFramebufferFoveationParametersQCOM;
PFNGLFRAMEBUFFERPARAMETERIPROC glad_glFramebufferParameteri = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferParameteri(GLenum target, GLenum pname, GLint param) {
    _pre_call_gles2_callback("glFramebufferParameteri", (GLADapiproc) glad_glFramebufferParameteri, 3, target, pname, param);
    glad_glFramebufferParameteri(target, pname, param);
    _post_call_gles2_callback(NULL, "glFramebufferParameteri", (GLADapiproc) glad_glFramebufferParameteri, 3, target, pname, param);
    
}
PFNGLFRAMEBUFFERPARAMETERIPROC glad_debug_glFramebufferParameteri = glad_debug_impl_glFramebufferParameteri;
PFNGLFRAMEBUFFERPARAMETERIMESAPROC glad_glFramebufferParameteriMESA = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferParameteriMESA(GLenum target, GLenum pname, GLint param) {
    _pre_call_gles2_callback("glFramebufferParameteriMESA", (GLADapiproc) glad_glFramebufferParameteriMESA, 3, target, pname, param);
    glad_glFramebufferParameteriMESA(target, pname, param);
    _post_call_gles2_callback(NULL, "glFramebufferParameteriMESA", (GLADapiproc) glad_glFramebufferParameteriMESA, 3, target, pname, param);
    
}
PFNGLFRAMEBUFFERPARAMETERIMESAPROC glad_debug_glFramebufferParameteriMESA = glad_debug_impl_glFramebufferParameteriMESA;
PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC glad_glFramebufferPixelLocalStorageSizeEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferPixelLocalStorageSizeEXT(GLuint target, GLsizei size) {
    _pre_call_gles2_callback("glFramebufferPixelLocalStorageSizeEXT", (GLADapiproc) glad_glFramebufferPixelLocalStorageSizeEXT, 2, target, size);
    glad_glFramebufferPixelLocalStorageSizeEXT(target, size);
    _post_call_gles2_callback(NULL, "glFramebufferPixelLocalStorageSizeEXT", (GLADapiproc) glad_glFramebufferPixelLocalStorageSizeEXT, 2, target, size);
    
}
PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC glad_debug_glFramebufferPixelLocalStorageSizeEXT = glad_debug_impl_glFramebufferPixelLocalStorageSizeEXT;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    _pre_call_gles2_callback("glFramebufferRenderbuffer", (GLADapiproc) glad_glFramebufferRenderbuffer, 4, target, attachment, renderbuffertarget, renderbuffer);
    glad_glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
    _post_call_gles2_callback(NULL, "glFramebufferRenderbuffer", (GLADapiproc) glad_glFramebufferRenderbuffer, 4, target, attachment, renderbuffertarget, renderbuffer);
    
}
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_debug_glFramebufferRenderbuffer = glad_debug_impl_glFramebufferRenderbuffer;
PFNGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC glad_glFramebufferSampleLocationsfvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferSampleLocationsfvNV(GLenum target, GLuint start, GLsizei count, const GLfloat * v) {
    _pre_call_gles2_callback("glFramebufferSampleLocationsfvNV", (GLADapiproc) glad_glFramebufferSampleLocationsfvNV, 4, target, start, count, v);
    glad_glFramebufferSampleLocationsfvNV(target, start, count, v);
    _post_call_gles2_callback(NULL, "glFramebufferSampleLocationsfvNV", (GLADapiproc) glad_glFramebufferSampleLocationsfvNV, 4, target, start, count, v);
    
}
PFNGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC glad_debug_glFramebufferSampleLocationsfvNV = glad_debug_impl_glFramebufferSampleLocationsfvNV;
PFNGLFRAMEBUFFERSHADINGRATEEXTPROC glad_glFramebufferShadingRateEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferShadingRateEXT(GLenum target, GLenum attachment, GLuint texture, GLint baseLayer, GLsizei numLayers, GLsizei texelWidth, GLsizei texelHeight) {
    _pre_call_gles2_callback("glFramebufferShadingRateEXT", (GLADapiproc) glad_glFramebufferShadingRateEXT, 7, target, attachment, texture, baseLayer, numLayers, texelWidth, texelHeight);
    glad_glFramebufferShadingRateEXT(target, attachment, texture, baseLayer, numLayers, texelWidth, texelHeight);
    _post_call_gles2_callback(NULL, "glFramebufferShadingRateEXT", (GLADapiproc) glad_glFramebufferShadingRateEXT, 7, target, attachment, texture, baseLayer, numLayers, texelWidth, texelHeight);
    
}
PFNGLFRAMEBUFFERSHADINGRATEEXTPROC glad_debug_glFramebufferShadingRateEXT = glad_debug_impl_glFramebufferShadingRateEXT;
PFNGLFRAMEBUFFERTEXTUREPROC glad_glFramebufferTexture = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    _pre_call_gles2_callback("glFramebufferTexture", (GLADapiproc) glad_glFramebufferTexture, 4, target, attachment, texture, level);
    glad_glFramebufferTexture(target, attachment, texture, level);
    _post_call_gles2_callback(NULL, "glFramebufferTexture", (GLADapiproc) glad_glFramebufferTexture, 4, target, attachment, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTUREPROC glad_debug_glFramebufferTexture = glad_debug_impl_glFramebufferTexture;
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    _pre_call_gles2_callback("glFramebufferTexture2D", (GLADapiproc) glad_glFramebufferTexture2D, 5, target, attachment, textarget, texture, level);
    glad_glFramebufferTexture2D(target, attachment, textarget, texture, level);
    _post_call_gles2_callback(NULL, "glFramebufferTexture2D", (GLADapiproc) glad_glFramebufferTexture2D, 5, target, attachment, textarget, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_debug_glFramebufferTexture2D = glad_debug_impl_glFramebufferTexture2D;
PFNGLFRAMEBUFFERTEXTURE2DDOWNSAMPLEIMGPROC glad_glFramebufferTexture2DDownsampleIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture2DDownsampleIMG(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint xscale, GLint yscale) {
    _pre_call_gles2_callback("glFramebufferTexture2DDownsampleIMG", (GLADapiproc) glad_glFramebufferTexture2DDownsampleIMG, 7, target, attachment, textarget, texture, level, xscale, yscale);
    glad_glFramebufferTexture2DDownsampleIMG(target, attachment, textarget, texture, level, xscale, yscale);
    _post_call_gles2_callback(NULL, "glFramebufferTexture2DDownsampleIMG", (GLADapiproc) glad_glFramebufferTexture2DDownsampleIMG, 7, target, attachment, textarget, texture, level, xscale, yscale);
    
}
PFNGLFRAMEBUFFERTEXTURE2DDOWNSAMPLEIMGPROC glad_debug_glFramebufferTexture2DDownsampleIMG = glad_debug_impl_glFramebufferTexture2DDownsampleIMG;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glad_glFramebufferTexture2DMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture2DMultisampleEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples) {
    _pre_call_gles2_callback("glFramebufferTexture2DMultisampleEXT", (GLADapiproc) glad_glFramebufferTexture2DMultisampleEXT, 6, target, attachment, textarget, texture, level, samples);
    glad_glFramebufferTexture2DMultisampleEXT(target, attachment, textarget, texture, level, samples);
    _post_call_gles2_callback(NULL, "glFramebufferTexture2DMultisampleEXT", (GLADapiproc) glad_glFramebufferTexture2DMultisampleEXT, 6, target, attachment, textarget, texture, level, samples);
    
}
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glad_debug_glFramebufferTexture2DMultisampleEXT = glad_debug_impl_glFramebufferTexture2DMultisampleEXT;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC glad_glFramebufferTexture2DMultisampleIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture2DMultisampleIMG(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples) {
    _pre_call_gles2_callback("glFramebufferTexture2DMultisampleIMG", (GLADapiproc) glad_glFramebufferTexture2DMultisampleIMG, 6, target, attachment, textarget, texture, level, samples);
    glad_glFramebufferTexture2DMultisampleIMG(target, attachment, textarget, texture, level, samples);
    _post_call_gles2_callback(NULL, "glFramebufferTexture2DMultisampleIMG", (GLADapiproc) glad_glFramebufferTexture2DMultisampleIMG, 6, target, attachment, textarget, texture, level, samples);
    
}
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC glad_debug_glFramebufferTexture2DMultisampleIMG = glad_debug_impl_glFramebufferTexture2DMultisampleIMG;
PFNGLFRAMEBUFFERTEXTURE3DOESPROC glad_glFramebufferTexture3DOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTexture3DOES(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {
    _pre_call_gles2_callback("glFramebufferTexture3DOES", (GLADapiproc) glad_glFramebufferTexture3DOES, 6, target, attachment, textarget, texture, level, zoffset);
    glad_glFramebufferTexture3DOES(target, attachment, textarget, texture, level, zoffset);
    _post_call_gles2_callback(NULL, "glFramebufferTexture3DOES", (GLADapiproc) glad_glFramebufferTexture3DOES, 6, target, attachment, textarget, texture, level, zoffset);
    
}
PFNGLFRAMEBUFFERTEXTURE3DOESPROC glad_debug_glFramebufferTexture3DOES = glad_debug_impl_glFramebufferTexture3DOES;
PFNGLFRAMEBUFFERTEXTUREEXTPROC glad_glFramebufferTextureEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureEXT(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    _pre_call_gles2_callback("glFramebufferTextureEXT", (GLADapiproc) glad_glFramebufferTextureEXT, 4, target, attachment, texture, level);
    glad_glFramebufferTextureEXT(target, attachment, texture, level);
    _post_call_gles2_callback(NULL, "glFramebufferTextureEXT", (GLADapiproc) glad_glFramebufferTextureEXT, 4, target, attachment, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTUREEXTPROC glad_debug_glFramebufferTextureEXT = glad_debug_impl_glFramebufferTextureEXT;
PFNGLFRAMEBUFFERTEXTURELAYERPROC glad_glFramebufferTextureLayer = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    _pre_call_gles2_callback("glFramebufferTextureLayer", (GLADapiproc) glad_glFramebufferTextureLayer, 5, target, attachment, texture, level, layer);
    glad_glFramebufferTextureLayer(target, attachment, texture, level, layer);
    _post_call_gles2_callback(NULL, "glFramebufferTextureLayer", (GLADapiproc) glad_glFramebufferTextureLayer, 5, target, attachment, texture, level, layer);
    
}
PFNGLFRAMEBUFFERTEXTURELAYERPROC glad_debug_glFramebufferTextureLayer = glad_debug_impl_glFramebufferTextureLayer;
PFNGLFRAMEBUFFERTEXTURELAYERDOWNSAMPLEIMGPROC glad_glFramebufferTextureLayerDownsampleIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureLayerDownsampleIMG(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer, GLint xscale, GLint yscale) {
    _pre_call_gles2_callback("glFramebufferTextureLayerDownsampleIMG", (GLADapiproc) glad_glFramebufferTextureLayerDownsampleIMG, 7, target, attachment, texture, level, layer, xscale, yscale);
    glad_glFramebufferTextureLayerDownsampleIMG(target, attachment, texture, level, layer, xscale, yscale);
    _post_call_gles2_callback(NULL, "glFramebufferTextureLayerDownsampleIMG", (GLADapiproc) glad_glFramebufferTextureLayerDownsampleIMG, 7, target, attachment, texture, level, layer, xscale, yscale);
    
}
PFNGLFRAMEBUFFERTEXTURELAYERDOWNSAMPLEIMGPROC glad_debug_glFramebufferTextureLayerDownsampleIMG = glad_debug_impl_glFramebufferTextureLayerDownsampleIMG;
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glad_glFramebufferTextureMultisampleMultiviewOVR = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureMultisampleMultiviewOVR(GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews) {
    _pre_call_gles2_callback("glFramebufferTextureMultisampleMultiviewOVR", (GLADapiproc) glad_glFramebufferTextureMultisampleMultiviewOVR, 7, target, attachment, texture, level, samples, baseViewIndex, numViews);
    glad_glFramebufferTextureMultisampleMultiviewOVR(target, attachment, texture, level, samples, baseViewIndex, numViews);
    _post_call_gles2_callback(NULL, "glFramebufferTextureMultisampleMultiviewOVR", (GLADapiproc) glad_glFramebufferTextureMultisampleMultiviewOVR, 7, target, attachment, texture, level, samples, baseViewIndex, numViews);
    
}
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glad_debug_glFramebufferTextureMultisampleMultiviewOVR = glad_debug_impl_glFramebufferTextureMultisampleMultiviewOVR;
PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glad_glFramebufferTextureMultiviewOVR = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureMultiviewOVR(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews) {
    _pre_call_gles2_callback("glFramebufferTextureMultiviewOVR", (GLADapiproc) glad_glFramebufferTextureMultiviewOVR, 6, target, attachment, texture, level, baseViewIndex, numViews);
    glad_glFramebufferTextureMultiviewOVR(target, attachment, texture, level, baseViewIndex, numViews);
    _post_call_gles2_callback(NULL, "glFramebufferTextureMultiviewOVR", (GLADapiproc) glad_glFramebufferTextureMultiviewOVR, 6, target, attachment, texture, level, baseViewIndex, numViews);
    
}
PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glad_debug_glFramebufferTextureMultiviewOVR = glad_debug_impl_glFramebufferTextureMultiviewOVR;
PFNGLFRAMEBUFFERTEXTUREOESPROC glad_glFramebufferTextureOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glFramebufferTextureOES(GLenum target, GLenum attachment, GLuint texture, GLint level) {
    _pre_call_gles2_callback("glFramebufferTextureOES", (GLADapiproc) glad_glFramebufferTextureOES, 4, target, attachment, texture, level);
    glad_glFramebufferTextureOES(target, attachment, texture, level);
    _post_call_gles2_callback(NULL, "glFramebufferTextureOES", (GLADapiproc) glad_glFramebufferTextureOES, 4, target, attachment, texture, level);
    
}
PFNGLFRAMEBUFFERTEXTUREOESPROC glad_debug_glFramebufferTextureOES = glad_debug_impl_glFramebufferTextureOES;
PFNGLFRONTFACEPROC glad_glFrontFace = NULL;
static void GLAD_API_PTR glad_debug_impl_glFrontFace(GLenum mode) {
    _pre_call_gles2_callback("glFrontFace", (GLADapiproc) glad_glFrontFace, 1, mode);
    glad_glFrontFace(mode);
    _post_call_gles2_callback(NULL, "glFrontFace", (GLADapiproc) glad_glFrontFace, 1, mode);
    
}
PFNGLFRONTFACEPROC glad_debug_glFrontFace = glad_debug_impl_glFrontFace;
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenBuffers(GLsizei n, GLuint * buffers) {
    _pre_call_gles2_callback("glGenBuffers", (GLADapiproc) glad_glGenBuffers, 2, n, buffers);
    glad_glGenBuffers(n, buffers);
    _post_call_gles2_callback(NULL, "glGenBuffers", (GLADapiproc) glad_glGenBuffers, 2, n, buffers);
    
}
PFNGLGENBUFFERSPROC glad_debug_glGenBuffers = glad_debug_impl_glGenBuffers;
PFNGLGENFENCESNVPROC glad_glGenFencesNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenFencesNV(GLsizei n, GLuint * fences) {
    _pre_call_gles2_callback("glGenFencesNV", (GLADapiproc) glad_glGenFencesNV, 2, n, fences);
    glad_glGenFencesNV(n, fences);
    _post_call_gles2_callback(NULL, "glGenFencesNV", (GLADapiproc) glad_glGenFencesNV, 2, n, fences);
    
}
PFNGLGENFENCESNVPROC glad_debug_glGenFencesNV = glad_debug_impl_glGenFencesNV;
PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenFramebuffers(GLsizei n, GLuint * framebuffers) {
    _pre_call_gles2_callback("glGenFramebuffers", (GLADapiproc) glad_glGenFramebuffers, 2, n, framebuffers);
    glad_glGenFramebuffers(n, framebuffers);
    _post_call_gles2_callback(NULL, "glGenFramebuffers", (GLADapiproc) glad_glGenFramebuffers, 2, n, framebuffers);
    
}
PFNGLGENFRAMEBUFFERSPROC glad_debug_glGenFramebuffers = glad_debug_impl_glGenFramebuffers;
PFNGLGENPATHSNVPROC glad_glGenPathsNV = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glGenPathsNV(GLsizei range) {
    GLuint ret;
    _pre_call_gles2_callback("glGenPathsNV", (GLADapiproc) glad_glGenPathsNV, 1, range);
    ret = glad_glGenPathsNV(range);
    _post_call_gles2_callback((void*) &ret, "glGenPathsNV", (GLADapiproc) glad_glGenPathsNV, 1, range);
    return ret;
}
PFNGLGENPATHSNVPROC glad_debug_glGenPathsNV = glad_debug_impl_glGenPathsNV;
PFNGLGENPERFMONITORSAMDPROC glad_glGenPerfMonitorsAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenPerfMonitorsAMD(GLsizei n, GLuint * monitors) {
    _pre_call_gles2_callback("glGenPerfMonitorsAMD", (GLADapiproc) glad_glGenPerfMonitorsAMD, 2, n, monitors);
    glad_glGenPerfMonitorsAMD(n, monitors);
    _post_call_gles2_callback(NULL, "glGenPerfMonitorsAMD", (GLADapiproc) glad_glGenPerfMonitorsAMD, 2, n, monitors);
    
}
PFNGLGENPERFMONITORSAMDPROC glad_debug_glGenPerfMonitorsAMD = glad_debug_impl_glGenPerfMonitorsAMD;
PFNGLGENPROGRAMPIPELINESPROC glad_glGenProgramPipelines = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenProgramPipelines(GLsizei n, GLuint * pipelines) {
    _pre_call_gles2_callback("glGenProgramPipelines", (GLADapiproc) glad_glGenProgramPipelines, 2, n, pipelines);
    glad_glGenProgramPipelines(n, pipelines);
    _post_call_gles2_callback(NULL, "glGenProgramPipelines", (GLADapiproc) glad_glGenProgramPipelines, 2, n, pipelines);
    
}
PFNGLGENPROGRAMPIPELINESPROC glad_debug_glGenProgramPipelines = glad_debug_impl_glGenProgramPipelines;
PFNGLGENPROGRAMPIPELINESEXTPROC glad_glGenProgramPipelinesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenProgramPipelinesEXT(GLsizei n, GLuint * pipelines) {
    _pre_call_gles2_callback("glGenProgramPipelinesEXT", (GLADapiproc) glad_glGenProgramPipelinesEXT, 2, n, pipelines);
    glad_glGenProgramPipelinesEXT(n, pipelines);
    _post_call_gles2_callback(NULL, "glGenProgramPipelinesEXT", (GLADapiproc) glad_glGenProgramPipelinesEXT, 2, n, pipelines);
    
}
PFNGLGENPROGRAMPIPELINESEXTPROC glad_debug_glGenProgramPipelinesEXT = glad_debug_impl_glGenProgramPipelinesEXT;
PFNGLGENQUERIESPROC glad_glGenQueries = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenQueries(GLsizei n, GLuint * ids) {
    _pre_call_gles2_callback("glGenQueries", (GLADapiproc) glad_glGenQueries, 2, n, ids);
    glad_glGenQueries(n, ids);
    _post_call_gles2_callback(NULL, "glGenQueries", (GLADapiproc) glad_glGenQueries, 2, n, ids);
    
}
PFNGLGENQUERIESPROC glad_debug_glGenQueries = glad_debug_impl_glGenQueries;
PFNGLGENQUERIESEXTPROC glad_glGenQueriesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenQueriesEXT(GLsizei n, GLuint * ids) {
    _pre_call_gles2_callback("glGenQueriesEXT", (GLADapiproc) glad_glGenQueriesEXT, 2, n, ids);
    glad_glGenQueriesEXT(n, ids);
    _post_call_gles2_callback(NULL, "glGenQueriesEXT", (GLADapiproc) glad_glGenQueriesEXT, 2, n, ids);
    
}
PFNGLGENQUERIESEXTPROC glad_debug_glGenQueriesEXT = glad_debug_impl_glGenQueriesEXT;
PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenRenderbuffers(GLsizei n, GLuint * renderbuffers) {
    _pre_call_gles2_callback("glGenRenderbuffers", (GLADapiproc) glad_glGenRenderbuffers, 2, n, renderbuffers);
    glad_glGenRenderbuffers(n, renderbuffers);
    _post_call_gles2_callback(NULL, "glGenRenderbuffers", (GLADapiproc) glad_glGenRenderbuffers, 2, n, renderbuffers);
    
}
PFNGLGENRENDERBUFFERSPROC glad_debug_glGenRenderbuffers = glad_debug_impl_glGenRenderbuffers;
PFNGLGENSAMPLERSPROC glad_glGenSamplers = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenSamplers(GLsizei count, GLuint * samplers) {
    _pre_call_gles2_callback("glGenSamplers", (GLADapiproc) glad_glGenSamplers, 2, count, samplers);
    glad_glGenSamplers(count, samplers);
    _post_call_gles2_callback(NULL, "glGenSamplers", (GLADapiproc) glad_glGenSamplers, 2, count, samplers);
    
}
PFNGLGENSAMPLERSPROC glad_debug_glGenSamplers = glad_debug_impl_glGenSamplers;
PFNGLGENSEMAPHORESEXTPROC glad_glGenSemaphoresEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenSemaphoresEXT(GLsizei n, GLuint * semaphores) {
    _pre_call_gles2_callback("glGenSemaphoresEXT", (GLADapiproc) glad_glGenSemaphoresEXT, 2, n, semaphores);
    glad_glGenSemaphoresEXT(n, semaphores);
    _post_call_gles2_callback(NULL, "glGenSemaphoresEXT", (GLADapiproc) glad_glGenSemaphoresEXT, 2, n, semaphores);
    
}
PFNGLGENSEMAPHORESEXTPROC glad_debug_glGenSemaphoresEXT = glad_debug_impl_glGenSemaphoresEXT;
PFNGLGENTEXTURESPROC glad_glGenTextures = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenTextures(GLsizei n, GLuint * textures) {
    _pre_call_gles2_callback("glGenTextures", (GLADapiproc) glad_glGenTextures, 2, n, textures);
    glad_glGenTextures(n, textures);
    _post_call_gles2_callback(NULL, "glGenTextures", (GLADapiproc) glad_glGenTextures, 2, n, textures);
    
}
PFNGLGENTEXTURESPROC glad_debug_glGenTextures = glad_debug_impl_glGenTextures;
PFNGLGENTRANSFORMFEEDBACKSPROC glad_glGenTransformFeedbacks = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenTransformFeedbacks(GLsizei n, GLuint * ids) {
    _pre_call_gles2_callback("glGenTransformFeedbacks", (GLADapiproc) glad_glGenTransformFeedbacks, 2, n, ids);
    glad_glGenTransformFeedbacks(n, ids);
    _post_call_gles2_callback(NULL, "glGenTransformFeedbacks", (GLADapiproc) glad_glGenTransformFeedbacks, 2, n, ids);
    
}
PFNGLGENTRANSFORMFEEDBACKSPROC glad_debug_glGenTransformFeedbacks = glad_debug_impl_glGenTransformFeedbacks;
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenVertexArrays(GLsizei n, GLuint * arrays) {
    _pre_call_gles2_callback("glGenVertexArrays", (GLADapiproc) glad_glGenVertexArrays, 2, n, arrays);
    glad_glGenVertexArrays(n, arrays);
    _post_call_gles2_callback(NULL, "glGenVertexArrays", (GLADapiproc) glad_glGenVertexArrays, 2, n, arrays);
    
}
PFNGLGENVERTEXARRAYSPROC glad_debug_glGenVertexArrays = glad_debug_impl_glGenVertexArrays;
PFNGLGENVERTEXARRAYSOESPROC glad_glGenVertexArraysOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenVertexArraysOES(GLsizei n, GLuint * arrays) {
    _pre_call_gles2_callback("glGenVertexArraysOES", (GLADapiproc) glad_glGenVertexArraysOES, 2, n, arrays);
    glad_glGenVertexArraysOES(n, arrays);
    _post_call_gles2_callback(NULL, "glGenVertexArraysOES", (GLADapiproc) glad_glGenVertexArraysOES, 2, n, arrays);
    
}
PFNGLGENVERTEXARRAYSOESPROC glad_debug_glGenVertexArraysOES = glad_debug_impl_glGenVertexArraysOES;
PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap = NULL;
static void GLAD_API_PTR glad_debug_impl_glGenerateMipmap(GLenum target) {
    _pre_call_gles2_callback("glGenerateMipmap", (GLADapiproc) glad_glGenerateMipmap, 1, target);
    glad_glGenerateMipmap(target);
    _post_call_gles2_callback(NULL, "glGenerateMipmap", (GLADapiproc) glad_glGenerateMipmap, 1, target);
    
}
PFNGLGENERATEMIPMAPPROC glad_debug_glGenerateMipmap = glad_debug_impl_glGenerateMipmap;
PFNGLGETACTIVEATTRIBPROC glad_glGetActiveAttrib = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name) {
    _pre_call_gles2_callback("glGetActiveAttrib", (GLADapiproc) glad_glGetActiveAttrib, 7, program, index, bufSize, length, size, type, name);
    glad_glGetActiveAttrib(program, index, bufSize, length, size, type, name);
    _post_call_gles2_callback(NULL, "glGetActiveAttrib", (GLADapiproc) glad_glGetActiveAttrib, 7, program, index, bufSize, length, size, type, name);
    
}
PFNGLGETACTIVEATTRIBPROC glad_debug_glGetActiveAttrib = glad_debug_impl_glGetActiveAttrib;
PFNGLGETACTIVEUNIFORMPROC glad_glGetActiveUniform = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, GLchar * name) {
    _pre_call_gles2_callback("glGetActiveUniform", (GLADapiproc) glad_glGetActiveUniform, 7, program, index, bufSize, length, size, type, name);
    glad_glGetActiveUniform(program, index, bufSize, length, size, type, name);
    _post_call_gles2_callback(NULL, "glGetActiveUniform", (GLADapiproc) glad_glGetActiveUniform, 7, program, index, bufSize, length, size, type, name);
    
}
PFNGLGETACTIVEUNIFORMPROC glad_debug_glGetActiveUniform = glad_debug_impl_glGetActiveUniform;
PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glad_glGetActiveUniformBlockName = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei * length, GLchar * uniformBlockName) {
    _pre_call_gles2_callback("glGetActiveUniformBlockName", (GLADapiproc) glad_glGetActiveUniformBlockName, 5, program, uniformBlockIndex, bufSize, length, uniformBlockName);
    glad_glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
    _post_call_gles2_callback(NULL, "glGetActiveUniformBlockName", (GLADapiproc) glad_glGetActiveUniformBlockName, 5, program, uniformBlockIndex, bufSize, length, uniformBlockName);
    
}
PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC glad_debug_glGetActiveUniformBlockName = glad_debug_impl_glGetActiveUniformBlockName;
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glad_glGetActiveUniformBlockiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetActiveUniformBlockiv", (GLADapiproc) glad_glGetActiveUniformBlockiv, 4, program, uniformBlockIndex, pname, params);
    glad_glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
    _post_call_gles2_callback(NULL, "glGetActiveUniformBlockiv", (GLADapiproc) glad_glGetActiveUniformBlockiv, 4, program, uniformBlockIndex, pname, params);
    
}
PFNGLGETACTIVEUNIFORMBLOCKIVPROC glad_debug_glGetActiveUniformBlockiv = glad_debug_impl_glGetActiveUniformBlockiv;
PFNGLGETACTIVEUNIFORMSIVPROC glad_glGetActiveUniformsiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint * uniformIndices, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetActiveUniformsiv", (GLADapiproc) glad_glGetActiveUniformsiv, 5, program, uniformCount, uniformIndices, pname, params);
    glad_glGetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
    _post_call_gles2_callback(NULL, "glGetActiveUniformsiv", (GLADapiproc) glad_glGetActiveUniformsiv, 5, program, uniformCount, uniformIndices, pname, params);
    
}
PFNGLGETACTIVEUNIFORMSIVPROC glad_debug_glGetActiveUniformsiv = glad_debug_impl_glGetActiveUniformsiv;
PFNGLGETATTACHEDSHADERSPROC glad_glGetAttachedShaders = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei * count, GLuint * shaders) {
    _pre_call_gles2_callback("glGetAttachedShaders", (GLADapiproc) glad_glGetAttachedShaders, 4, program, maxCount, count, shaders);
    glad_glGetAttachedShaders(program, maxCount, count, shaders);
    _post_call_gles2_callback(NULL, "glGetAttachedShaders", (GLADapiproc) glad_glGetAttachedShaders, 4, program, maxCount, count, shaders);
    
}
PFNGLGETATTACHEDSHADERSPROC glad_debug_glGetAttachedShaders = glad_debug_impl_glGetAttachedShaders;
PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetAttribLocation(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gles2_callback("glGetAttribLocation", (GLADapiproc) glad_glGetAttribLocation, 2, program, name);
    ret = glad_glGetAttribLocation(program, name);
    _post_call_gles2_callback((void*) &ret, "glGetAttribLocation", (GLADapiproc) glad_glGetAttribLocation, 2, program, name);
    return ret;
}
PFNGLGETATTRIBLOCATIONPROC glad_debug_glGetAttribLocation = glad_debug_impl_glGetAttribLocation;
PFNGLGETBOOLEANI_VPROC glad_glGetBooleani_v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBooleani_v(GLenum target, GLuint index, GLboolean * data) {
    _pre_call_gles2_callback("glGetBooleani_v", (GLADapiproc) glad_glGetBooleani_v, 3, target, index, data);
    glad_glGetBooleani_v(target, index, data);
    _post_call_gles2_callback(NULL, "glGetBooleani_v", (GLADapiproc) glad_glGetBooleani_v, 3, target, index, data);
    
}
PFNGLGETBOOLEANI_VPROC glad_debug_glGetBooleani_v = glad_debug_impl_glGetBooleani_v;
PFNGLGETBOOLEANVPROC glad_glGetBooleanv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBooleanv(GLenum pname, GLboolean * data) {
    _pre_call_gles2_callback("glGetBooleanv", (GLADapiproc) glad_glGetBooleanv, 2, pname, data);
    glad_glGetBooleanv(pname, data);
    _post_call_gles2_callback(NULL, "glGetBooleanv", (GLADapiproc) glad_glGetBooleanv, 2, pname, data);
    
}
PFNGLGETBOOLEANVPROC glad_debug_glGetBooleanv = glad_debug_impl_glGetBooleanv;
PFNGLGETBUFFERPARAMETERI64VPROC glad_glGetBufferParameteri64v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 * params) {
    _pre_call_gles2_callback("glGetBufferParameteri64v", (GLADapiproc) glad_glGetBufferParameteri64v, 3, target, pname, params);
    glad_glGetBufferParameteri64v(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetBufferParameteri64v", (GLADapiproc) glad_glGetBufferParameteri64v, 3, target, pname, params);
    
}
PFNGLGETBUFFERPARAMETERI64VPROC glad_debug_glGetBufferParameteri64v = glad_debug_impl_glGetBufferParameteri64v;
PFNGLGETBUFFERPARAMETERIVPROC glad_glGetBufferParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetBufferParameteriv", (GLADapiproc) glad_glGetBufferParameteriv, 3, target, pname, params);
    glad_glGetBufferParameteriv(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetBufferParameteriv", (GLADapiproc) glad_glGetBufferParameteriv, 3, target, pname, params);
    
}
PFNGLGETBUFFERPARAMETERIVPROC glad_debug_glGetBufferParameteriv = glad_debug_impl_glGetBufferParameteriv;
PFNGLGETBUFFERPOINTERVPROC glad_glGetBufferPointerv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferPointerv(GLenum target, GLenum pname, void ** params) {
    _pre_call_gles2_callback("glGetBufferPointerv", (GLADapiproc) glad_glGetBufferPointerv, 3, target, pname, params);
    glad_glGetBufferPointerv(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetBufferPointerv", (GLADapiproc) glad_glGetBufferPointerv, 3, target, pname, params);
    
}
PFNGLGETBUFFERPOINTERVPROC glad_debug_glGetBufferPointerv = glad_debug_impl_glGetBufferPointerv;
PFNGLGETBUFFERPOINTERVOESPROC glad_glGetBufferPointervOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetBufferPointervOES(GLenum target, GLenum pname, void ** params) {
    _pre_call_gles2_callback("glGetBufferPointervOES", (GLADapiproc) glad_glGetBufferPointervOES, 3, target, pname, params);
    glad_glGetBufferPointervOES(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetBufferPointervOES", (GLADapiproc) glad_glGetBufferPointervOES, 3, target, pname, params);
    
}
PFNGLGETBUFFERPOINTERVOESPROC glad_debug_glGetBufferPointervOES = glad_debug_impl_glGetBufferPointervOES;
PFNGLGETCOVERAGEMODULATIONTABLENVPROC glad_glGetCoverageModulationTableNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetCoverageModulationTableNV(GLsizei bufSize, GLfloat * v) {
    _pre_call_gles2_callback("glGetCoverageModulationTableNV", (GLADapiproc) glad_glGetCoverageModulationTableNV, 2, bufSize, v);
    glad_glGetCoverageModulationTableNV(bufSize, v);
    _post_call_gles2_callback(NULL, "glGetCoverageModulationTableNV", (GLADapiproc) glad_glGetCoverageModulationTableNV, 2, bufSize, v);
    
}
PFNGLGETCOVERAGEMODULATIONTABLENVPROC glad_debug_glGetCoverageModulationTableNV = glad_debug_impl_glGetCoverageModulationTableNV;
PFNGLGETDEBUGMESSAGELOGPROC glad_glGetDebugMessageLog = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glGetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum * sources, GLenum * types, GLuint * ids, GLenum * severities, GLsizei * lengths, GLchar * messageLog) {
    GLuint ret;
    _pre_call_gles2_callback("glGetDebugMessageLog", (GLADapiproc) glad_glGetDebugMessageLog, 8, count, bufSize, sources, types, ids, severities, lengths, messageLog);
    ret = glad_glGetDebugMessageLog(count, bufSize, sources, types, ids, severities, lengths, messageLog);
    _post_call_gles2_callback((void*) &ret, "glGetDebugMessageLog", (GLADapiproc) glad_glGetDebugMessageLog, 8, count, bufSize, sources, types, ids, severities, lengths, messageLog);
    return ret;
}
PFNGLGETDEBUGMESSAGELOGPROC glad_debug_glGetDebugMessageLog = glad_debug_impl_glGetDebugMessageLog;
PFNGLGETDEBUGMESSAGELOGKHRPROC glad_glGetDebugMessageLogKHR = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glGetDebugMessageLogKHR(GLuint count, GLsizei bufSize, GLenum * sources, GLenum * types, GLuint * ids, GLenum * severities, GLsizei * lengths, GLchar * messageLog) {
    GLuint ret;
    _pre_call_gles2_callback("glGetDebugMessageLogKHR", (GLADapiproc) glad_glGetDebugMessageLogKHR, 8, count, bufSize, sources, types, ids, severities, lengths, messageLog);
    ret = glad_glGetDebugMessageLogKHR(count, bufSize, sources, types, ids, severities, lengths, messageLog);
    _post_call_gles2_callback((void*) &ret, "glGetDebugMessageLogKHR", (GLADapiproc) glad_glGetDebugMessageLogKHR, 8, count, bufSize, sources, types, ids, severities, lengths, messageLog);
    return ret;
}
PFNGLGETDEBUGMESSAGELOGKHRPROC glad_debug_glGetDebugMessageLogKHR = glad_debug_impl_glGetDebugMessageLogKHR;
PFNGLGETDRIVERCONTROLSTRINGQCOMPROC glad_glGetDriverControlStringQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetDriverControlStringQCOM(GLuint driverControl, GLsizei bufSize, GLsizei * length, GLchar * driverControlString) {
    _pre_call_gles2_callback("glGetDriverControlStringQCOM", (GLADapiproc) glad_glGetDriverControlStringQCOM, 4, driverControl, bufSize, length, driverControlString);
    glad_glGetDriverControlStringQCOM(driverControl, bufSize, length, driverControlString);
    _post_call_gles2_callback(NULL, "glGetDriverControlStringQCOM", (GLADapiproc) glad_glGetDriverControlStringQCOM, 4, driverControl, bufSize, length, driverControlString);
    
}
PFNGLGETDRIVERCONTROLSTRINGQCOMPROC glad_debug_glGetDriverControlStringQCOM = glad_debug_impl_glGetDriverControlStringQCOM;
PFNGLGETDRIVERCONTROLSQCOMPROC glad_glGetDriverControlsQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetDriverControlsQCOM(GLint * num, GLsizei size, GLuint * driverControls) {
    _pre_call_gles2_callback("glGetDriverControlsQCOM", (GLADapiproc) glad_glGetDriverControlsQCOM, 3, num, size, driverControls);
    glad_glGetDriverControlsQCOM(num, size, driverControls);
    _post_call_gles2_callback(NULL, "glGetDriverControlsQCOM", (GLADapiproc) glad_glGetDriverControlsQCOM, 3, num, size, driverControls);
    
}
PFNGLGETDRIVERCONTROLSQCOMPROC glad_debug_glGetDriverControlsQCOM = glad_debug_impl_glGetDriverControlsQCOM;
PFNGLGETERRORPROC glad_glGetError = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glGetError(void) {
    GLenum ret;
    _pre_call_gles2_callback("glGetError", (GLADapiproc) glad_glGetError, 0);
    ret = glad_glGetError();
    _post_call_gles2_callback((void*) &ret, "glGetError", (GLADapiproc) glad_glGetError, 0);
    return ret;
}
PFNGLGETERRORPROC glad_debug_glGetError = glad_debug_impl_glGetError;
PFNGLGETFENCEIVNVPROC glad_glGetFenceivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFenceivNV(GLuint fence, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetFenceivNV", (GLADapiproc) glad_glGetFenceivNV, 3, fence, pname, params);
    glad_glGetFenceivNV(fence, pname, params);
    _post_call_gles2_callback(NULL, "glGetFenceivNV", (GLADapiproc) glad_glGetFenceivNV, 3, fence, pname, params);
    
}
PFNGLGETFENCEIVNVPROC glad_debug_glGetFenceivNV = glad_debug_impl_glGetFenceivNV;
PFNGLGETFIRSTPERFQUERYIDINTELPROC glad_glGetFirstPerfQueryIdINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFirstPerfQueryIdINTEL(GLuint * queryId) {
    _pre_call_gles2_callback("glGetFirstPerfQueryIdINTEL", (GLADapiproc) glad_glGetFirstPerfQueryIdINTEL, 1, queryId);
    glad_glGetFirstPerfQueryIdINTEL(queryId);
    _post_call_gles2_callback(NULL, "glGetFirstPerfQueryIdINTEL", (GLADapiproc) glad_glGetFirstPerfQueryIdINTEL, 1, queryId);
    
}
PFNGLGETFIRSTPERFQUERYIDINTELPROC glad_debug_glGetFirstPerfQueryIdINTEL = glad_debug_impl_glGetFirstPerfQueryIdINTEL;
PFNGLGETFLOATI_VNVPROC glad_glGetFloati_vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFloati_vNV(GLenum target, GLuint index, GLfloat * data) {
    _pre_call_gles2_callback("glGetFloati_vNV", (GLADapiproc) glad_glGetFloati_vNV, 3, target, index, data);
    glad_glGetFloati_vNV(target, index, data);
    _post_call_gles2_callback(NULL, "glGetFloati_vNV", (GLADapiproc) glad_glGetFloati_vNV, 3, target, index, data);
    
}
PFNGLGETFLOATI_VNVPROC glad_debug_glGetFloati_vNV = glad_debug_impl_glGetFloati_vNV;
PFNGLGETFLOATI_VOESPROC glad_glGetFloati_vOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFloati_vOES(GLenum target, GLuint index, GLfloat * data) {
    _pre_call_gles2_callback("glGetFloati_vOES", (GLADapiproc) glad_glGetFloati_vOES, 3, target, index, data);
    glad_glGetFloati_vOES(target, index, data);
    _post_call_gles2_callback(NULL, "glGetFloati_vOES", (GLADapiproc) glad_glGetFloati_vOES, 3, target, index, data);
    
}
PFNGLGETFLOATI_VOESPROC glad_debug_glGetFloati_vOES = glad_debug_impl_glGetFloati_vOES;
PFNGLGETFLOATVPROC glad_glGetFloatv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFloatv(GLenum pname, GLfloat * data) {
    _pre_call_gles2_callback("glGetFloatv", (GLADapiproc) glad_glGetFloatv, 2, pname, data);
    glad_glGetFloatv(pname, data);
    _post_call_gles2_callback(NULL, "glGetFloatv", (GLADapiproc) glad_glGetFloatv, 2, pname, data);
    
}
PFNGLGETFLOATVPROC glad_debug_glGetFloatv = glad_debug_impl_glGetFloatv;
PFNGLGETFRAGDATAINDEXEXTPROC glad_glGetFragDataIndexEXT = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetFragDataIndexEXT(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gles2_callback("glGetFragDataIndexEXT", (GLADapiproc) glad_glGetFragDataIndexEXT, 2, program, name);
    ret = glad_glGetFragDataIndexEXT(program, name);
    _post_call_gles2_callback((void*) &ret, "glGetFragDataIndexEXT", (GLADapiproc) glad_glGetFragDataIndexEXT, 2, program, name);
    return ret;
}
PFNGLGETFRAGDATAINDEXEXTPROC glad_debug_glGetFragDataIndexEXT = glad_debug_impl_glGetFragDataIndexEXT;
PFNGLGETFRAGDATALOCATIONPROC glad_glGetFragDataLocation = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetFragDataLocation(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gles2_callback("glGetFragDataLocation", (GLADapiproc) glad_glGetFragDataLocation, 2, program, name);
    ret = glad_glGetFragDataLocation(program, name);
    _post_call_gles2_callback((void*) &ret, "glGetFragDataLocation", (GLADapiproc) glad_glGetFragDataLocation, 2, program, name);
    return ret;
}
PFNGLGETFRAGDATALOCATIONPROC glad_debug_glGetFragDataLocation = glad_debug_impl_glGetFragDataLocation;
PFNGLGETFRAGMENTSHADINGRATESEXTPROC glad_glGetFragmentShadingRatesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFragmentShadingRatesEXT(GLsizei samples, GLsizei maxCount, GLsizei * count, GLenum * shadingRates) {
    _pre_call_gles2_callback("glGetFragmentShadingRatesEXT", (GLADapiproc) glad_glGetFragmentShadingRatesEXT, 4, samples, maxCount, count, shadingRates);
    glad_glGetFragmentShadingRatesEXT(samples, maxCount, count, shadingRates);
    _post_call_gles2_callback(NULL, "glGetFragmentShadingRatesEXT", (GLADapiproc) glad_glGetFragmentShadingRatesEXT, 4, samples, maxCount, count, shadingRates);
    
}
PFNGLGETFRAGMENTSHADINGRATESEXTPROC glad_debug_glGetFragmentShadingRatesEXT = glad_debug_impl_glGetFragmentShadingRatesEXT;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_glGetFramebufferAttachmentParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetFramebufferAttachmentParameteriv", (GLADapiproc) glad_glGetFramebufferAttachmentParameteriv, 4, target, attachment, pname, params);
    glad_glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
    _post_call_gles2_callback(NULL, "glGetFramebufferAttachmentParameteriv", (GLADapiproc) glad_glGetFramebufferAttachmentParameteriv, 4, target, attachment, pname, params);
    
}
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glad_debug_glGetFramebufferAttachmentParameteriv = glad_debug_impl_glGetFramebufferAttachmentParameteriv;
PFNGLGETFRAMEBUFFERPARAMETERIVPROC glad_glGetFramebufferParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFramebufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetFramebufferParameteriv", (GLADapiproc) glad_glGetFramebufferParameteriv, 3, target, pname, params);
    glad_glGetFramebufferParameteriv(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetFramebufferParameteriv", (GLADapiproc) glad_glGetFramebufferParameteriv, 3, target, pname, params);
    
}
PFNGLGETFRAMEBUFFERPARAMETERIVPROC glad_debug_glGetFramebufferParameteriv = glad_debug_impl_glGetFramebufferParameteriv;
PFNGLGETFRAMEBUFFERPARAMETERIVMESAPROC glad_glGetFramebufferParameterivMESA = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetFramebufferParameterivMESA(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetFramebufferParameterivMESA", (GLADapiproc) glad_glGetFramebufferParameterivMESA, 3, target, pname, params);
    glad_glGetFramebufferParameterivMESA(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetFramebufferParameterivMESA", (GLADapiproc) glad_glGetFramebufferParameterivMESA, 3, target, pname, params);
    
}
PFNGLGETFRAMEBUFFERPARAMETERIVMESAPROC glad_debug_glGetFramebufferParameterivMESA = glad_debug_impl_glGetFramebufferParameterivMESA;
PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC glad_glGetFramebufferPixelLocalStorageSizeEXT = NULL;
static GLsizei GLAD_API_PTR glad_debug_impl_glGetFramebufferPixelLocalStorageSizeEXT(GLuint target) {
    GLsizei ret;
    _pre_call_gles2_callback("glGetFramebufferPixelLocalStorageSizeEXT", (GLADapiproc) glad_glGetFramebufferPixelLocalStorageSizeEXT, 1, target);
    ret = glad_glGetFramebufferPixelLocalStorageSizeEXT(target);
    _post_call_gles2_callback((void*) &ret, "glGetFramebufferPixelLocalStorageSizeEXT", (GLADapiproc) glad_glGetFramebufferPixelLocalStorageSizeEXT, 1, target);
    return ret;
}
PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC glad_debug_glGetFramebufferPixelLocalStorageSizeEXT = glad_debug_impl_glGetFramebufferPixelLocalStorageSizeEXT;
PFNGLGETGRAPHICSRESETSTATUSPROC glad_glGetGraphicsResetStatus = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glGetGraphicsResetStatus(void) {
    GLenum ret;
    _pre_call_gles2_callback("glGetGraphicsResetStatus", (GLADapiproc) glad_glGetGraphicsResetStatus, 0);
    ret = glad_glGetGraphicsResetStatus();
    _post_call_gles2_callback((void*) &ret, "glGetGraphicsResetStatus", (GLADapiproc) glad_glGetGraphicsResetStatus, 0);
    return ret;
}
PFNGLGETGRAPHICSRESETSTATUSPROC glad_debug_glGetGraphicsResetStatus = glad_debug_impl_glGetGraphicsResetStatus;
PFNGLGETGRAPHICSRESETSTATUSEXTPROC glad_glGetGraphicsResetStatusEXT = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glGetGraphicsResetStatusEXT(void) {
    GLenum ret;
    _pre_call_gles2_callback("glGetGraphicsResetStatusEXT", (GLADapiproc) glad_glGetGraphicsResetStatusEXT, 0);
    ret = glad_glGetGraphicsResetStatusEXT();
    _post_call_gles2_callback((void*) &ret, "glGetGraphicsResetStatusEXT", (GLADapiproc) glad_glGetGraphicsResetStatusEXT, 0);
    return ret;
}
PFNGLGETGRAPHICSRESETSTATUSEXTPROC glad_debug_glGetGraphicsResetStatusEXT = glad_debug_impl_glGetGraphicsResetStatusEXT;
PFNGLGETGRAPHICSRESETSTATUSKHRPROC glad_glGetGraphicsResetStatusKHR = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glGetGraphicsResetStatusKHR(void) {
    GLenum ret;
    _pre_call_gles2_callback("glGetGraphicsResetStatusKHR", (GLADapiproc) glad_glGetGraphicsResetStatusKHR, 0);
    ret = glad_glGetGraphicsResetStatusKHR();
    _post_call_gles2_callback((void*) &ret, "glGetGraphicsResetStatusKHR", (GLADapiproc) glad_glGetGraphicsResetStatusKHR, 0);
    return ret;
}
PFNGLGETGRAPHICSRESETSTATUSKHRPROC glad_debug_glGetGraphicsResetStatusKHR = glad_debug_impl_glGetGraphicsResetStatusKHR;
PFNGLGETIMAGEHANDLENVPROC glad_glGetImageHandleNV = NULL;
static GLuint64 GLAD_API_PTR glad_debug_impl_glGetImageHandleNV(GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format) {
    GLuint64 ret;
    _pre_call_gles2_callback("glGetImageHandleNV", (GLADapiproc) glad_glGetImageHandleNV, 5, texture, level, layered, layer, format);
    ret = glad_glGetImageHandleNV(texture, level, layered, layer, format);
    _post_call_gles2_callback((void*) &ret, "glGetImageHandleNV", (GLADapiproc) glad_glGetImageHandleNV, 5, texture, level, layered, layer, format);
    return ret;
}
PFNGLGETIMAGEHANDLENVPROC glad_debug_glGetImageHandleNV = glad_debug_impl_glGetImageHandleNV;
PFNGLGETINTEGER64I_VPROC glad_glGetInteger64i_v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInteger64i_v(GLenum target, GLuint index, GLint64 * data) {
    _pre_call_gles2_callback("glGetInteger64i_v", (GLADapiproc) glad_glGetInteger64i_v, 3, target, index, data);
    glad_glGetInteger64i_v(target, index, data);
    _post_call_gles2_callback(NULL, "glGetInteger64i_v", (GLADapiproc) glad_glGetInteger64i_v, 3, target, index, data);
    
}
PFNGLGETINTEGER64I_VPROC glad_debug_glGetInteger64i_v = glad_debug_impl_glGetInteger64i_v;
PFNGLGETINTEGER64VPROC glad_glGetInteger64v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInteger64v(GLenum pname, GLint64 * data) {
    _pre_call_gles2_callback("glGetInteger64v", (GLADapiproc) glad_glGetInteger64v, 2, pname, data);
    glad_glGetInteger64v(pname, data);
    _post_call_gles2_callback(NULL, "glGetInteger64v", (GLADapiproc) glad_glGetInteger64v, 2, pname, data);
    
}
PFNGLGETINTEGER64VPROC glad_debug_glGetInteger64v = glad_debug_impl_glGetInteger64v;
PFNGLGETINTEGER64VAPPLEPROC glad_glGetInteger64vAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInteger64vAPPLE(GLenum pname, GLint64 * params) {
    _pre_call_gles2_callback("glGetInteger64vAPPLE", (GLADapiproc) glad_glGetInteger64vAPPLE, 2, pname, params);
    glad_glGetInteger64vAPPLE(pname, params);
    _post_call_gles2_callback(NULL, "glGetInteger64vAPPLE", (GLADapiproc) glad_glGetInteger64vAPPLE, 2, pname, params);
    
}
PFNGLGETINTEGER64VAPPLEPROC glad_debug_glGetInteger64vAPPLE = glad_debug_impl_glGetInteger64vAPPLE;
PFNGLGETINTEGER64VEXTPROC glad_glGetInteger64vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInteger64vEXT(GLenum pname, GLint64 * data) {
    _pre_call_gles2_callback("glGetInteger64vEXT", (GLADapiproc) glad_glGetInteger64vEXT, 2, pname, data);
    glad_glGetInteger64vEXT(pname, data);
    _post_call_gles2_callback(NULL, "glGetInteger64vEXT", (GLADapiproc) glad_glGetInteger64vEXT, 2, pname, data);
    
}
PFNGLGETINTEGER64VEXTPROC glad_debug_glGetInteger64vEXT = glad_debug_impl_glGetInteger64vEXT;
PFNGLGETINTEGERI_VPROC glad_glGetIntegeri_v = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetIntegeri_v(GLenum target, GLuint index, GLint * data) {
    _pre_call_gles2_callback("glGetIntegeri_v", (GLADapiproc) glad_glGetIntegeri_v, 3, target, index, data);
    glad_glGetIntegeri_v(target, index, data);
    _post_call_gles2_callback(NULL, "glGetIntegeri_v", (GLADapiproc) glad_glGetIntegeri_v, 3, target, index, data);
    
}
PFNGLGETINTEGERI_VPROC glad_debug_glGetIntegeri_v = glad_debug_impl_glGetIntegeri_v;
PFNGLGETINTEGERI_VEXTPROC glad_glGetIntegeri_vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetIntegeri_vEXT(GLenum target, GLuint index, GLint * data) {
    _pre_call_gles2_callback("glGetIntegeri_vEXT", (GLADapiproc) glad_glGetIntegeri_vEXT, 3, target, index, data);
    glad_glGetIntegeri_vEXT(target, index, data);
    _post_call_gles2_callback(NULL, "glGetIntegeri_vEXT", (GLADapiproc) glad_glGetIntegeri_vEXT, 3, target, index, data);
    
}
PFNGLGETINTEGERI_VEXTPROC glad_debug_glGetIntegeri_vEXT = glad_debug_impl_glGetIntegeri_vEXT;
PFNGLGETINTEGERVPROC glad_glGetIntegerv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetIntegerv(GLenum pname, GLint * data) {
    _pre_call_gles2_callback("glGetIntegerv", (GLADapiproc) glad_glGetIntegerv, 2, pname, data);
    glad_glGetIntegerv(pname, data);
    _post_call_gles2_callback(NULL, "glGetIntegerv", (GLADapiproc) glad_glGetIntegerv, 2, pname, data);
    
}
PFNGLGETINTEGERVPROC glad_debug_glGetIntegerv = glad_debug_impl_glGetIntegerv;
PFNGLGETINTERNALFORMATSAMPLEIVNVPROC glad_glGetInternalformatSampleivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInternalformatSampleivNV(GLenum target, GLenum internalformat, GLsizei samples, GLenum pname, GLsizei count, GLint * params) {
    _pre_call_gles2_callback("glGetInternalformatSampleivNV", (GLADapiproc) glad_glGetInternalformatSampleivNV, 6, target, internalformat, samples, pname, count, params);
    glad_glGetInternalformatSampleivNV(target, internalformat, samples, pname, count, params);
    _post_call_gles2_callback(NULL, "glGetInternalformatSampleivNV", (GLADapiproc) glad_glGetInternalformatSampleivNV, 6, target, internalformat, samples, pname, count, params);
    
}
PFNGLGETINTERNALFORMATSAMPLEIVNVPROC glad_debug_glGetInternalformatSampleivNV = glad_debug_impl_glGetInternalformatSampleivNV;
PFNGLGETINTERNALFORMATIVPROC glad_glGetInternalformativ = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint * params) {
    _pre_call_gles2_callback("glGetInternalformativ", (GLADapiproc) glad_glGetInternalformativ, 5, target, internalformat, pname, count, params);
    glad_glGetInternalformativ(target, internalformat, pname, count, params);
    _post_call_gles2_callback(NULL, "glGetInternalformativ", (GLADapiproc) glad_glGetInternalformativ, 5, target, internalformat, pname, count, params);
    
}
PFNGLGETINTERNALFORMATIVPROC glad_debug_glGetInternalformativ = glad_debug_impl_glGetInternalformativ;
PFNGLGETMEMORYOBJECTDETACHEDRESOURCESUIVNVPROC glad_glGetMemoryObjectDetachedResourcesuivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMemoryObjectDetachedResourcesuivNV(GLuint memory, GLenum pname, GLint first, GLsizei count, GLuint * params) {
    _pre_call_gles2_callback("glGetMemoryObjectDetachedResourcesuivNV", (GLADapiproc) glad_glGetMemoryObjectDetachedResourcesuivNV, 5, memory, pname, first, count, params);
    glad_glGetMemoryObjectDetachedResourcesuivNV(memory, pname, first, count, params);
    _post_call_gles2_callback(NULL, "glGetMemoryObjectDetachedResourcesuivNV", (GLADapiproc) glad_glGetMemoryObjectDetachedResourcesuivNV, 5, memory, pname, first, count, params);
    
}
PFNGLGETMEMORYOBJECTDETACHEDRESOURCESUIVNVPROC glad_debug_glGetMemoryObjectDetachedResourcesuivNV = glad_debug_impl_glGetMemoryObjectDetachedResourcesuivNV;
PFNGLGETMEMORYOBJECTPARAMETERIVEXTPROC glad_glGetMemoryObjectParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMemoryObjectParameterivEXT(GLuint memoryObject, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetMemoryObjectParameterivEXT", (GLADapiproc) glad_glGetMemoryObjectParameterivEXT, 3, memoryObject, pname, params);
    glad_glGetMemoryObjectParameterivEXT(memoryObject, pname, params);
    _post_call_gles2_callback(NULL, "glGetMemoryObjectParameterivEXT", (GLADapiproc) glad_glGetMemoryObjectParameterivEXT, 3, memoryObject, pname, params);
    
}
PFNGLGETMEMORYOBJECTPARAMETERIVEXTPROC glad_debug_glGetMemoryObjectParameterivEXT = glad_debug_impl_glGetMemoryObjectParameterivEXT;
PFNGLGETMULTISAMPLEFVPROC glad_glGetMultisamplefv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetMultisamplefv(GLenum pname, GLuint index, GLfloat * val) {
    _pre_call_gles2_callback("glGetMultisamplefv", (GLADapiproc) glad_glGetMultisamplefv, 3, pname, index, val);
    glad_glGetMultisamplefv(pname, index, val);
    _post_call_gles2_callback(NULL, "glGetMultisamplefv", (GLADapiproc) glad_glGetMultisamplefv, 3, pname, index, val);
    
}
PFNGLGETMULTISAMPLEFVPROC glad_debug_glGetMultisamplefv = glad_debug_impl_glGetMultisamplefv;
PFNGLGETNEXTPERFQUERYIDINTELPROC glad_glGetNextPerfQueryIdINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetNextPerfQueryIdINTEL(GLuint queryId, GLuint * nextQueryId) {
    _pre_call_gles2_callback("glGetNextPerfQueryIdINTEL", (GLADapiproc) glad_glGetNextPerfQueryIdINTEL, 2, queryId, nextQueryId);
    glad_glGetNextPerfQueryIdINTEL(queryId, nextQueryId);
    _post_call_gles2_callback(NULL, "glGetNextPerfQueryIdINTEL", (GLADapiproc) glad_glGetNextPerfQueryIdINTEL, 2, queryId, nextQueryId);
    
}
PFNGLGETNEXTPERFQUERYIDINTELPROC glad_debug_glGetNextPerfQueryIdINTEL = glad_debug_impl_glGetNextPerfQueryIdINTEL;
PFNGLGETOBJECTLABELPROC glad_glGetObjectLabel = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetObjectLabel(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei * length, GLchar * label) {
    _pre_call_gles2_callback("glGetObjectLabel", (GLADapiproc) glad_glGetObjectLabel, 5, identifier, name, bufSize, length, label);
    glad_glGetObjectLabel(identifier, name, bufSize, length, label);
    _post_call_gles2_callback(NULL, "glGetObjectLabel", (GLADapiproc) glad_glGetObjectLabel, 5, identifier, name, bufSize, length, label);
    
}
PFNGLGETOBJECTLABELPROC glad_debug_glGetObjectLabel = glad_debug_impl_glGetObjectLabel;
PFNGLGETOBJECTLABELEXTPROC glad_glGetObjectLabelEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetObjectLabelEXT(GLenum type, GLuint object, GLsizei bufSize, GLsizei * length, GLchar * label) {
    _pre_call_gles2_callback("glGetObjectLabelEXT", (GLADapiproc) glad_glGetObjectLabelEXT, 5, type, object, bufSize, length, label);
    glad_glGetObjectLabelEXT(type, object, bufSize, length, label);
    _post_call_gles2_callback(NULL, "glGetObjectLabelEXT", (GLADapiproc) glad_glGetObjectLabelEXT, 5, type, object, bufSize, length, label);
    
}
PFNGLGETOBJECTLABELEXTPROC glad_debug_glGetObjectLabelEXT = glad_debug_impl_glGetObjectLabelEXT;
PFNGLGETOBJECTLABELKHRPROC glad_glGetObjectLabelKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetObjectLabelKHR(GLenum identifier, GLuint name, GLsizei bufSize, GLsizei * length, GLchar * label) {
    _pre_call_gles2_callback("glGetObjectLabelKHR", (GLADapiproc) glad_glGetObjectLabelKHR, 5, identifier, name, bufSize, length, label);
    glad_glGetObjectLabelKHR(identifier, name, bufSize, length, label);
    _post_call_gles2_callback(NULL, "glGetObjectLabelKHR", (GLADapiproc) glad_glGetObjectLabelKHR, 5, identifier, name, bufSize, length, label);
    
}
PFNGLGETOBJECTLABELKHRPROC glad_debug_glGetObjectLabelKHR = glad_debug_impl_glGetObjectLabelKHR;
PFNGLGETOBJECTPTRLABELPROC glad_glGetObjectPtrLabel = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetObjectPtrLabel(const void * ptr, GLsizei bufSize, GLsizei * length, GLchar * label) {
    _pre_call_gles2_callback("glGetObjectPtrLabel", (GLADapiproc) glad_glGetObjectPtrLabel, 4, ptr, bufSize, length, label);
    glad_glGetObjectPtrLabel(ptr, bufSize, length, label);
    _post_call_gles2_callback(NULL, "glGetObjectPtrLabel", (GLADapiproc) glad_glGetObjectPtrLabel, 4, ptr, bufSize, length, label);
    
}
PFNGLGETOBJECTPTRLABELPROC glad_debug_glGetObjectPtrLabel = glad_debug_impl_glGetObjectPtrLabel;
PFNGLGETOBJECTPTRLABELKHRPROC glad_glGetObjectPtrLabelKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetObjectPtrLabelKHR(const void * ptr, GLsizei bufSize, GLsizei * length, GLchar * label) {
    _pre_call_gles2_callback("glGetObjectPtrLabelKHR", (GLADapiproc) glad_glGetObjectPtrLabelKHR, 4, ptr, bufSize, length, label);
    glad_glGetObjectPtrLabelKHR(ptr, bufSize, length, label);
    _post_call_gles2_callback(NULL, "glGetObjectPtrLabelKHR", (GLADapiproc) glad_glGetObjectPtrLabelKHR, 4, ptr, bufSize, length, label);
    
}
PFNGLGETOBJECTPTRLABELKHRPROC glad_debug_glGetObjectPtrLabelKHR = glad_debug_impl_glGetObjectPtrLabelKHR;
PFNGLGETPATHCOMMANDSNVPROC glad_glGetPathCommandsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPathCommandsNV(GLuint path, GLubyte * commands) {
    _pre_call_gles2_callback("glGetPathCommandsNV", (GLADapiproc) glad_glGetPathCommandsNV, 2, path, commands);
    glad_glGetPathCommandsNV(path, commands);
    _post_call_gles2_callback(NULL, "glGetPathCommandsNV", (GLADapiproc) glad_glGetPathCommandsNV, 2, path, commands);
    
}
PFNGLGETPATHCOMMANDSNVPROC glad_debug_glGetPathCommandsNV = glad_debug_impl_glGetPathCommandsNV;
PFNGLGETPATHCOORDSNVPROC glad_glGetPathCoordsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPathCoordsNV(GLuint path, GLfloat * coords) {
    _pre_call_gles2_callback("glGetPathCoordsNV", (GLADapiproc) glad_glGetPathCoordsNV, 2, path, coords);
    glad_glGetPathCoordsNV(path, coords);
    _post_call_gles2_callback(NULL, "glGetPathCoordsNV", (GLADapiproc) glad_glGetPathCoordsNV, 2, path, coords);
    
}
PFNGLGETPATHCOORDSNVPROC glad_debug_glGetPathCoordsNV = glad_debug_impl_glGetPathCoordsNV;
PFNGLGETPATHDASHARRAYNVPROC glad_glGetPathDashArrayNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPathDashArrayNV(GLuint path, GLfloat * dashArray) {
    _pre_call_gles2_callback("glGetPathDashArrayNV", (GLADapiproc) glad_glGetPathDashArrayNV, 2, path, dashArray);
    glad_glGetPathDashArrayNV(path, dashArray);
    _post_call_gles2_callback(NULL, "glGetPathDashArrayNV", (GLADapiproc) glad_glGetPathDashArrayNV, 2, path, dashArray);
    
}
PFNGLGETPATHDASHARRAYNVPROC glad_debug_glGetPathDashArrayNV = glad_debug_impl_glGetPathDashArrayNV;
PFNGLGETPATHLENGTHNVPROC glad_glGetPathLengthNV = NULL;
static GLfloat GLAD_API_PTR glad_debug_impl_glGetPathLengthNV(GLuint path, GLsizei startSegment, GLsizei numSegments) {
    GLfloat ret;
    _pre_call_gles2_callback("glGetPathLengthNV", (GLADapiproc) glad_glGetPathLengthNV, 3, path, startSegment, numSegments);
    ret = glad_glGetPathLengthNV(path, startSegment, numSegments);
    _post_call_gles2_callback((void*) &ret, "glGetPathLengthNV", (GLADapiproc) glad_glGetPathLengthNV, 3, path, startSegment, numSegments);
    return ret;
}
PFNGLGETPATHLENGTHNVPROC glad_debug_glGetPathLengthNV = glad_debug_impl_glGetPathLengthNV;
PFNGLGETPATHMETRICRANGENVPROC glad_glGetPathMetricRangeNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPathMetricRangeNV(GLbitfield metricQueryMask, GLuint firstPathName, GLsizei numPaths, GLsizei stride, GLfloat * metrics) {
    _pre_call_gles2_callback("glGetPathMetricRangeNV", (GLADapiproc) glad_glGetPathMetricRangeNV, 5, metricQueryMask, firstPathName, numPaths, stride, metrics);
    glad_glGetPathMetricRangeNV(metricQueryMask, firstPathName, numPaths, stride, metrics);
    _post_call_gles2_callback(NULL, "glGetPathMetricRangeNV", (GLADapiproc) glad_glGetPathMetricRangeNV, 5, metricQueryMask, firstPathName, numPaths, stride, metrics);
    
}
PFNGLGETPATHMETRICRANGENVPROC glad_debug_glGetPathMetricRangeNV = glad_debug_impl_glGetPathMetricRangeNV;
PFNGLGETPATHMETRICSNVPROC glad_glGetPathMetricsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPathMetricsNV(GLbitfield metricQueryMask, GLsizei numPaths, GLenum pathNameType, const void * paths, GLuint pathBase, GLsizei stride, GLfloat * metrics) {
    _pre_call_gles2_callback("glGetPathMetricsNV", (GLADapiproc) glad_glGetPathMetricsNV, 7, metricQueryMask, numPaths, pathNameType, paths, pathBase, stride, metrics);
    glad_glGetPathMetricsNV(metricQueryMask, numPaths, pathNameType, paths, pathBase, stride, metrics);
    _post_call_gles2_callback(NULL, "glGetPathMetricsNV", (GLADapiproc) glad_glGetPathMetricsNV, 7, metricQueryMask, numPaths, pathNameType, paths, pathBase, stride, metrics);
    
}
PFNGLGETPATHMETRICSNVPROC glad_debug_glGetPathMetricsNV = glad_debug_impl_glGetPathMetricsNV;
PFNGLGETPATHPARAMETERFVNVPROC glad_glGetPathParameterfvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPathParameterfvNV(GLuint path, GLenum pname, GLfloat * value) {
    _pre_call_gles2_callback("glGetPathParameterfvNV", (GLADapiproc) glad_glGetPathParameterfvNV, 3, path, pname, value);
    glad_glGetPathParameterfvNV(path, pname, value);
    _post_call_gles2_callback(NULL, "glGetPathParameterfvNV", (GLADapiproc) glad_glGetPathParameterfvNV, 3, path, pname, value);
    
}
PFNGLGETPATHPARAMETERFVNVPROC glad_debug_glGetPathParameterfvNV = glad_debug_impl_glGetPathParameterfvNV;
PFNGLGETPATHPARAMETERIVNVPROC glad_glGetPathParameterivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPathParameterivNV(GLuint path, GLenum pname, GLint * value) {
    _pre_call_gles2_callback("glGetPathParameterivNV", (GLADapiproc) glad_glGetPathParameterivNV, 3, path, pname, value);
    glad_glGetPathParameterivNV(path, pname, value);
    _post_call_gles2_callback(NULL, "glGetPathParameterivNV", (GLADapiproc) glad_glGetPathParameterivNV, 3, path, pname, value);
    
}
PFNGLGETPATHPARAMETERIVNVPROC glad_debug_glGetPathParameterivNV = glad_debug_impl_glGetPathParameterivNV;
PFNGLGETPATHSPACINGNVPROC glad_glGetPathSpacingNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPathSpacingNV(GLenum pathListMode, GLsizei numPaths, GLenum pathNameType, const void * paths, GLuint pathBase, GLfloat advanceScale, GLfloat kerningScale, GLenum transformType, GLfloat * returnedSpacing) {
    _pre_call_gles2_callback("glGetPathSpacingNV", (GLADapiproc) glad_glGetPathSpacingNV, 9, pathListMode, numPaths, pathNameType, paths, pathBase, advanceScale, kerningScale, transformType, returnedSpacing);
    glad_glGetPathSpacingNV(pathListMode, numPaths, pathNameType, paths, pathBase, advanceScale, kerningScale, transformType, returnedSpacing);
    _post_call_gles2_callback(NULL, "glGetPathSpacingNV", (GLADapiproc) glad_glGetPathSpacingNV, 9, pathListMode, numPaths, pathNameType, paths, pathBase, advanceScale, kerningScale, transformType, returnedSpacing);
    
}
PFNGLGETPATHSPACINGNVPROC glad_debug_glGetPathSpacingNV = glad_debug_impl_glGetPathSpacingNV;
PFNGLGETPERFCOUNTERINFOINTELPROC glad_glGetPerfCounterInfoINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfCounterInfoINTEL(GLuint queryId, GLuint counterId, GLuint counterNameLength, GLchar * counterName, GLuint counterDescLength, GLchar * counterDesc, GLuint * counterOffset, GLuint * counterDataSize, GLuint * counterTypeEnum, GLuint * counterDataTypeEnum, GLuint64 * rawCounterMaxValue) {
    _pre_call_gles2_callback("glGetPerfCounterInfoINTEL", (GLADapiproc) glad_glGetPerfCounterInfoINTEL, 11, queryId, counterId, counterNameLength, counterName, counterDescLength, counterDesc, counterOffset, counterDataSize, counterTypeEnum, counterDataTypeEnum, rawCounterMaxValue);
    glad_glGetPerfCounterInfoINTEL(queryId, counterId, counterNameLength, counterName, counterDescLength, counterDesc, counterOffset, counterDataSize, counterTypeEnum, counterDataTypeEnum, rawCounterMaxValue);
    _post_call_gles2_callback(NULL, "glGetPerfCounterInfoINTEL", (GLADapiproc) glad_glGetPerfCounterInfoINTEL, 11, queryId, counterId, counterNameLength, counterName, counterDescLength, counterDesc, counterOffset, counterDataSize, counterTypeEnum, counterDataTypeEnum, rawCounterMaxValue);
    
}
PFNGLGETPERFCOUNTERINFOINTELPROC glad_debug_glGetPerfCounterInfoINTEL = glad_debug_impl_glGetPerfCounterInfoINTEL;
PFNGLGETPERFMONITORCOUNTERDATAAMDPROC glad_glGetPerfMonitorCounterDataAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfMonitorCounterDataAMD(GLuint monitor, GLenum pname, GLsizei dataSize, GLuint * data, GLint * bytesWritten) {
    _pre_call_gles2_callback("glGetPerfMonitorCounterDataAMD", (GLADapiproc) glad_glGetPerfMonitorCounterDataAMD, 5, monitor, pname, dataSize, data, bytesWritten);
    glad_glGetPerfMonitorCounterDataAMD(monitor, pname, dataSize, data, bytesWritten);
    _post_call_gles2_callback(NULL, "glGetPerfMonitorCounterDataAMD", (GLADapiproc) glad_glGetPerfMonitorCounterDataAMD, 5, monitor, pname, dataSize, data, bytesWritten);
    
}
PFNGLGETPERFMONITORCOUNTERDATAAMDPROC glad_debug_glGetPerfMonitorCounterDataAMD = glad_debug_impl_glGetPerfMonitorCounterDataAMD;
PFNGLGETPERFMONITORCOUNTERINFOAMDPROC glad_glGetPerfMonitorCounterInfoAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfMonitorCounterInfoAMD(GLuint group, GLuint counter, GLenum pname, void * data) {
    _pre_call_gles2_callback("glGetPerfMonitorCounterInfoAMD", (GLADapiproc) glad_glGetPerfMonitorCounterInfoAMD, 4, group, counter, pname, data);
    glad_glGetPerfMonitorCounterInfoAMD(group, counter, pname, data);
    _post_call_gles2_callback(NULL, "glGetPerfMonitorCounterInfoAMD", (GLADapiproc) glad_glGetPerfMonitorCounterInfoAMD, 4, group, counter, pname, data);
    
}
PFNGLGETPERFMONITORCOUNTERINFOAMDPROC glad_debug_glGetPerfMonitorCounterInfoAMD = glad_debug_impl_glGetPerfMonitorCounterInfoAMD;
PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC glad_glGetPerfMonitorCounterStringAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfMonitorCounterStringAMD(GLuint group, GLuint counter, GLsizei bufSize, GLsizei * length, GLchar * counterString) {
    _pre_call_gles2_callback("glGetPerfMonitorCounterStringAMD", (GLADapiproc) glad_glGetPerfMonitorCounterStringAMD, 5, group, counter, bufSize, length, counterString);
    glad_glGetPerfMonitorCounterStringAMD(group, counter, bufSize, length, counterString);
    _post_call_gles2_callback(NULL, "glGetPerfMonitorCounterStringAMD", (GLADapiproc) glad_glGetPerfMonitorCounterStringAMD, 5, group, counter, bufSize, length, counterString);
    
}
PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC glad_debug_glGetPerfMonitorCounterStringAMD = glad_debug_impl_glGetPerfMonitorCounterStringAMD;
PFNGLGETPERFMONITORCOUNTERSAMDPROC glad_glGetPerfMonitorCountersAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfMonitorCountersAMD(GLuint group, GLint * numCounters, GLint * maxActiveCounters, GLsizei counterSize, GLuint * counters) {
    _pre_call_gles2_callback("glGetPerfMonitorCountersAMD", (GLADapiproc) glad_glGetPerfMonitorCountersAMD, 5, group, numCounters, maxActiveCounters, counterSize, counters);
    glad_glGetPerfMonitorCountersAMD(group, numCounters, maxActiveCounters, counterSize, counters);
    _post_call_gles2_callback(NULL, "glGetPerfMonitorCountersAMD", (GLADapiproc) glad_glGetPerfMonitorCountersAMD, 5, group, numCounters, maxActiveCounters, counterSize, counters);
    
}
PFNGLGETPERFMONITORCOUNTERSAMDPROC glad_debug_glGetPerfMonitorCountersAMD = glad_debug_impl_glGetPerfMonitorCountersAMD;
PFNGLGETPERFMONITORGROUPSTRINGAMDPROC glad_glGetPerfMonitorGroupStringAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfMonitorGroupStringAMD(GLuint group, GLsizei bufSize, GLsizei * length, GLchar * groupString) {
    _pre_call_gles2_callback("glGetPerfMonitorGroupStringAMD", (GLADapiproc) glad_glGetPerfMonitorGroupStringAMD, 4, group, bufSize, length, groupString);
    glad_glGetPerfMonitorGroupStringAMD(group, bufSize, length, groupString);
    _post_call_gles2_callback(NULL, "glGetPerfMonitorGroupStringAMD", (GLADapiproc) glad_glGetPerfMonitorGroupStringAMD, 4, group, bufSize, length, groupString);
    
}
PFNGLGETPERFMONITORGROUPSTRINGAMDPROC glad_debug_glGetPerfMonitorGroupStringAMD = glad_debug_impl_glGetPerfMonitorGroupStringAMD;
PFNGLGETPERFMONITORGROUPSAMDPROC glad_glGetPerfMonitorGroupsAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfMonitorGroupsAMD(GLint * numGroups, GLsizei groupsSize, GLuint * groups) {
    _pre_call_gles2_callback("glGetPerfMonitorGroupsAMD", (GLADapiproc) glad_glGetPerfMonitorGroupsAMD, 3, numGroups, groupsSize, groups);
    glad_glGetPerfMonitorGroupsAMD(numGroups, groupsSize, groups);
    _post_call_gles2_callback(NULL, "glGetPerfMonitorGroupsAMD", (GLADapiproc) glad_glGetPerfMonitorGroupsAMD, 3, numGroups, groupsSize, groups);
    
}
PFNGLGETPERFMONITORGROUPSAMDPROC glad_debug_glGetPerfMonitorGroupsAMD = glad_debug_impl_glGetPerfMonitorGroupsAMD;
PFNGLGETPERFQUERYDATAINTELPROC glad_glGetPerfQueryDataINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfQueryDataINTEL(GLuint queryHandle, GLuint flags, GLsizei dataSize, void * data, GLuint * bytesWritten) {
    _pre_call_gles2_callback("glGetPerfQueryDataINTEL", (GLADapiproc) glad_glGetPerfQueryDataINTEL, 5, queryHandle, flags, dataSize, data, bytesWritten);
    glad_glGetPerfQueryDataINTEL(queryHandle, flags, dataSize, data, bytesWritten);
    _post_call_gles2_callback(NULL, "glGetPerfQueryDataINTEL", (GLADapiproc) glad_glGetPerfQueryDataINTEL, 5, queryHandle, flags, dataSize, data, bytesWritten);
    
}
PFNGLGETPERFQUERYDATAINTELPROC glad_debug_glGetPerfQueryDataINTEL = glad_debug_impl_glGetPerfQueryDataINTEL;
PFNGLGETPERFQUERYIDBYNAMEINTELPROC glad_glGetPerfQueryIdByNameINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfQueryIdByNameINTEL(GLchar * queryName, GLuint * queryId) {
    _pre_call_gles2_callback("glGetPerfQueryIdByNameINTEL", (GLADapiproc) glad_glGetPerfQueryIdByNameINTEL, 2, queryName, queryId);
    glad_glGetPerfQueryIdByNameINTEL(queryName, queryId);
    _post_call_gles2_callback(NULL, "glGetPerfQueryIdByNameINTEL", (GLADapiproc) glad_glGetPerfQueryIdByNameINTEL, 2, queryName, queryId);
    
}
PFNGLGETPERFQUERYIDBYNAMEINTELPROC glad_debug_glGetPerfQueryIdByNameINTEL = glad_debug_impl_glGetPerfQueryIdByNameINTEL;
PFNGLGETPERFQUERYINFOINTELPROC glad_glGetPerfQueryInfoINTEL = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPerfQueryInfoINTEL(GLuint queryId, GLuint queryNameLength, GLchar * queryName, GLuint * dataSize, GLuint * noCounters, GLuint * noInstances, GLuint * capsMask) {
    _pre_call_gles2_callback("glGetPerfQueryInfoINTEL", (GLADapiproc) glad_glGetPerfQueryInfoINTEL, 7, queryId, queryNameLength, queryName, dataSize, noCounters, noInstances, capsMask);
    glad_glGetPerfQueryInfoINTEL(queryId, queryNameLength, queryName, dataSize, noCounters, noInstances, capsMask);
    _post_call_gles2_callback(NULL, "glGetPerfQueryInfoINTEL", (GLADapiproc) glad_glGetPerfQueryInfoINTEL, 7, queryId, queryNameLength, queryName, dataSize, noCounters, noInstances, capsMask);
    
}
PFNGLGETPERFQUERYINFOINTELPROC glad_debug_glGetPerfQueryInfoINTEL = glad_debug_impl_glGetPerfQueryInfoINTEL;
PFNGLGETPOINTERVPROC glad_glGetPointerv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPointerv(GLenum pname, void ** params) {
    _pre_call_gles2_callback("glGetPointerv", (GLADapiproc) glad_glGetPointerv, 2, pname, params);
    glad_glGetPointerv(pname, params);
    _post_call_gles2_callback(NULL, "glGetPointerv", (GLADapiproc) glad_glGetPointerv, 2, pname, params);
    
}
PFNGLGETPOINTERVPROC glad_debug_glGetPointerv = glad_debug_impl_glGetPointerv;
PFNGLGETPOINTERVKHRPROC glad_glGetPointervKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetPointervKHR(GLenum pname, void ** params) {
    _pre_call_gles2_callback("glGetPointervKHR", (GLADapiproc) glad_glGetPointervKHR, 2, pname, params);
    glad_glGetPointervKHR(pname, params);
    _post_call_gles2_callback(NULL, "glGetPointervKHR", (GLADapiproc) glad_glGetPointervKHR, 2, pname, params);
    
}
PFNGLGETPOINTERVKHRPROC glad_debug_glGetPointervKHR = glad_debug_impl_glGetPointervKHR;
PFNGLGETPROGRAMBINARYPROC glad_glGetProgramBinary = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, void * binary) {
    _pre_call_gles2_callback("glGetProgramBinary", (GLADapiproc) glad_glGetProgramBinary, 5, program, bufSize, length, binaryFormat, binary);
    glad_glGetProgramBinary(program, bufSize, length, binaryFormat, binary);
    _post_call_gles2_callback(NULL, "glGetProgramBinary", (GLADapiproc) glad_glGetProgramBinary, 5, program, bufSize, length, binaryFormat, binary);
    
}
PFNGLGETPROGRAMBINARYPROC glad_debug_glGetProgramBinary = glad_debug_impl_glGetProgramBinary;
PFNGLGETPROGRAMBINARYOESPROC glad_glGetProgramBinaryOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramBinaryOES(GLuint program, GLsizei bufSize, GLsizei * length, GLenum * binaryFormat, void * binary) {
    _pre_call_gles2_callback("glGetProgramBinaryOES", (GLADapiproc) glad_glGetProgramBinaryOES, 5, program, bufSize, length, binaryFormat, binary);
    glad_glGetProgramBinaryOES(program, bufSize, length, binaryFormat, binary);
    _post_call_gles2_callback(NULL, "glGetProgramBinaryOES", (GLADapiproc) glad_glGetProgramBinaryOES, 5, program, bufSize, length, binaryFormat, binary);
    
}
PFNGLGETPROGRAMBINARYOESPROC glad_debug_glGetProgramBinaryOES = glad_debug_impl_glGetProgramBinaryOES;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    _pre_call_gles2_callback("glGetProgramInfoLog", (GLADapiproc) glad_glGetProgramInfoLog, 4, program, bufSize, length, infoLog);
    glad_glGetProgramInfoLog(program, bufSize, length, infoLog);
    _post_call_gles2_callback(NULL, "glGetProgramInfoLog", (GLADapiproc) glad_glGetProgramInfoLog, 4, program, bufSize, length, infoLog);
    
}
PFNGLGETPROGRAMINFOLOGPROC glad_debug_glGetProgramInfoLog = glad_debug_impl_glGetProgramInfoLog;
PFNGLGETPROGRAMINTERFACEIVPROC glad_glGetProgramInterfaceiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetProgramInterfaceiv", (GLADapiproc) glad_glGetProgramInterfaceiv, 4, program, programInterface, pname, params);
    glad_glGetProgramInterfaceiv(program, programInterface, pname, params);
    _post_call_gles2_callback(NULL, "glGetProgramInterfaceiv", (GLADapiproc) glad_glGetProgramInterfaceiv, 4, program, programInterface, pname, params);
    
}
PFNGLGETPROGRAMINTERFACEIVPROC glad_debug_glGetProgramInterfaceiv = glad_debug_impl_glGetProgramInterfaceiv;
PFNGLGETPROGRAMPIPELINEINFOLOGPROC glad_glGetProgramPipelineInfoLog = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramPipelineInfoLog(GLuint pipeline, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    _pre_call_gles2_callback("glGetProgramPipelineInfoLog", (GLADapiproc) glad_glGetProgramPipelineInfoLog, 4, pipeline, bufSize, length, infoLog);
    glad_glGetProgramPipelineInfoLog(pipeline, bufSize, length, infoLog);
    _post_call_gles2_callback(NULL, "glGetProgramPipelineInfoLog", (GLADapiproc) glad_glGetProgramPipelineInfoLog, 4, pipeline, bufSize, length, infoLog);
    
}
PFNGLGETPROGRAMPIPELINEINFOLOGPROC glad_debug_glGetProgramPipelineInfoLog = glad_debug_impl_glGetProgramPipelineInfoLog;
PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC glad_glGetProgramPipelineInfoLogEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramPipelineInfoLogEXT(GLuint pipeline, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    _pre_call_gles2_callback("glGetProgramPipelineInfoLogEXT", (GLADapiproc) glad_glGetProgramPipelineInfoLogEXT, 4, pipeline, bufSize, length, infoLog);
    glad_glGetProgramPipelineInfoLogEXT(pipeline, bufSize, length, infoLog);
    _post_call_gles2_callback(NULL, "glGetProgramPipelineInfoLogEXT", (GLADapiproc) glad_glGetProgramPipelineInfoLogEXT, 4, pipeline, bufSize, length, infoLog);
    
}
PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC glad_debug_glGetProgramPipelineInfoLogEXT = glad_debug_impl_glGetProgramPipelineInfoLogEXT;
PFNGLGETPROGRAMPIPELINEIVPROC glad_glGetProgramPipelineiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramPipelineiv(GLuint pipeline, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetProgramPipelineiv", (GLADapiproc) glad_glGetProgramPipelineiv, 3, pipeline, pname, params);
    glad_glGetProgramPipelineiv(pipeline, pname, params);
    _post_call_gles2_callback(NULL, "glGetProgramPipelineiv", (GLADapiproc) glad_glGetProgramPipelineiv, 3, pipeline, pname, params);
    
}
PFNGLGETPROGRAMPIPELINEIVPROC glad_debug_glGetProgramPipelineiv = glad_debug_impl_glGetProgramPipelineiv;
PFNGLGETPROGRAMPIPELINEIVEXTPROC glad_glGetProgramPipelineivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramPipelineivEXT(GLuint pipeline, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetProgramPipelineivEXT", (GLADapiproc) glad_glGetProgramPipelineivEXT, 3, pipeline, pname, params);
    glad_glGetProgramPipelineivEXT(pipeline, pname, params);
    _post_call_gles2_callback(NULL, "glGetProgramPipelineivEXT", (GLADapiproc) glad_glGetProgramPipelineivEXT, 3, pipeline, pname, params);
    
}
PFNGLGETPROGRAMPIPELINEIVEXTPROC glad_debug_glGetProgramPipelineivEXT = glad_debug_impl_glGetProgramPipelineivEXT;
PFNGLGETPROGRAMRESOURCEINDEXPROC glad_glGetProgramResourceIndex = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glGetProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar * name) {
    GLuint ret;
    _pre_call_gles2_callback("glGetProgramResourceIndex", (GLADapiproc) glad_glGetProgramResourceIndex, 3, program, programInterface, name);
    ret = glad_glGetProgramResourceIndex(program, programInterface, name);
    _post_call_gles2_callback((void*) &ret, "glGetProgramResourceIndex", (GLADapiproc) glad_glGetProgramResourceIndex, 3, program, programInterface, name);
    return ret;
}
PFNGLGETPROGRAMRESOURCEINDEXPROC glad_debug_glGetProgramResourceIndex = glad_debug_impl_glGetProgramResourceIndex;
PFNGLGETPROGRAMRESOURCELOCATIONPROC glad_glGetProgramResourceLocation = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetProgramResourceLocation(GLuint program, GLenum programInterface, const GLchar * name) {
    GLint ret;
    _pre_call_gles2_callback("glGetProgramResourceLocation", (GLADapiproc) glad_glGetProgramResourceLocation, 3, program, programInterface, name);
    ret = glad_glGetProgramResourceLocation(program, programInterface, name);
    _post_call_gles2_callback((void*) &ret, "glGetProgramResourceLocation", (GLADapiproc) glad_glGetProgramResourceLocation, 3, program, programInterface, name);
    return ret;
}
PFNGLGETPROGRAMRESOURCELOCATIONPROC glad_debug_glGetProgramResourceLocation = glad_debug_impl_glGetProgramResourceLocation;
PFNGLGETPROGRAMRESOURCELOCATIONINDEXEXTPROC glad_glGetProgramResourceLocationIndexEXT = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetProgramResourceLocationIndexEXT(GLuint program, GLenum programInterface, const GLchar * name) {
    GLint ret;
    _pre_call_gles2_callback("glGetProgramResourceLocationIndexEXT", (GLADapiproc) glad_glGetProgramResourceLocationIndexEXT, 3, program, programInterface, name);
    ret = glad_glGetProgramResourceLocationIndexEXT(program, programInterface, name);
    _post_call_gles2_callback((void*) &ret, "glGetProgramResourceLocationIndexEXT", (GLADapiproc) glad_glGetProgramResourceLocationIndexEXT, 3, program, programInterface, name);
    return ret;
}
PFNGLGETPROGRAMRESOURCELOCATIONINDEXEXTPROC glad_debug_glGetProgramResourceLocationIndexEXT = glad_debug_impl_glGetProgramResourceLocationIndexEXT;
PFNGLGETPROGRAMRESOURCENAMEPROC glad_glGetProgramResourceName = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramResourceName(GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei * length, GLchar * name) {
    _pre_call_gles2_callback("glGetProgramResourceName", (GLADapiproc) glad_glGetProgramResourceName, 6, program, programInterface, index, bufSize, length, name);
    glad_glGetProgramResourceName(program, programInterface, index, bufSize, length, name);
    _post_call_gles2_callback(NULL, "glGetProgramResourceName", (GLADapiproc) glad_glGetProgramResourceName, 6, program, programInterface, index, bufSize, length, name);
    
}
PFNGLGETPROGRAMRESOURCENAMEPROC glad_debug_glGetProgramResourceName = glad_debug_impl_glGetProgramResourceName;
PFNGLGETPROGRAMRESOURCEFVNVPROC glad_glGetProgramResourcefvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramResourcefvNV(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum * props, GLsizei count, GLsizei * length, GLfloat * params) {
    _pre_call_gles2_callback("glGetProgramResourcefvNV", (GLADapiproc) glad_glGetProgramResourcefvNV, 8, program, programInterface, index, propCount, props, count, length, params);
    glad_glGetProgramResourcefvNV(program, programInterface, index, propCount, props, count, length, params);
    _post_call_gles2_callback(NULL, "glGetProgramResourcefvNV", (GLADapiproc) glad_glGetProgramResourcefvNV, 8, program, programInterface, index, propCount, props, count, length, params);
    
}
PFNGLGETPROGRAMRESOURCEFVNVPROC glad_debug_glGetProgramResourcefvNV = glad_debug_impl_glGetProgramResourcefvNV;
PFNGLGETPROGRAMRESOURCEIVPROC glad_glGetProgramResourceiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramResourceiv(GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum * props, GLsizei count, GLsizei * length, GLint * params) {
    _pre_call_gles2_callback("glGetProgramResourceiv", (GLADapiproc) glad_glGetProgramResourceiv, 8, program, programInterface, index, propCount, props, count, length, params);
    glad_glGetProgramResourceiv(program, programInterface, index, propCount, props, count, length, params);
    _post_call_gles2_callback(NULL, "glGetProgramResourceiv", (GLADapiproc) glad_glGetProgramResourceiv, 8, program, programInterface, index, propCount, props, count, length, params);
    
}
PFNGLGETPROGRAMRESOURCEIVPROC glad_debug_glGetProgramResourceiv = glad_debug_impl_glGetProgramResourceiv;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetProgramiv(GLuint program, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetProgramiv", (GLADapiproc) glad_glGetProgramiv, 3, program, pname, params);
    glad_glGetProgramiv(program, pname, params);
    _post_call_gles2_callback(NULL, "glGetProgramiv", (GLADapiproc) glad_glGetProgramiv, 3, program, pname, params);
    
}
PFNGLGETPROGRAMIVPROC glad_debug_glGetProgramiv = glad_debug_impl_glGetProgramiv;
PFNGLGETQUERYOBJECTI64VEXTPROC glad_glGetQueryObjecti64vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjecti64vEXT(GLuint id, GLenum pname, GLint64 * params) {
    _pre_call_gles2_callback("glGetQueryObjecti64vEXT", (GLADapiproc) glad_glGetQueryObjecti64vEXT, 3, id, pname, params);
    glad_glGetQueryObjecti64vEXT(id, pname, params);
    _post_call_gles2_callback(NULL, "glGetQueryObjecti64vEXT", (GLADapiproc) glad_glGetQueryObjecti64vEXT, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTI64VEXTPROC glad_debug_glGetQueryObjecti64vEXT = glad_debug_impl_glGetQueryObjecti64vEXT;
PFNGLGETQUERYOBJECTIVEXTPROC glad_glGetQueryObjectivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectivEXT(GLuint id, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetQueryObjectivEXT", (GLADapiproc) glad_glGetQueryObjectivEXT, 3, id, pname, params);
    glad_glGetQueryObjectivEXT(id, pname, params);
    _post_call_gles2_callback(NULL, "glGetQueryObjectivEXT", (GLADapiproc) glad_glGetQueryObjectivEXT, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTIVEXTPROC glad_debug_glGetQueryObjectivEXT = glad_debug_impl_glGetQueryObjectivEXT;
PFNGLGETQUERYOBJECTUI64VEXTPROC glad_glGetQueryObjectui64vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectui64vEXT(GLuint id, GLenum pname, GLuint64 * params) {
    _pre_call_gles2_callback("glGetQueryObjectui64vEXT", (GLADapiproc) glad_glGetQueryObjectui64vEXT, 3, id, pname, params);
    glad_glGetQueryObjectui64vEXT(id, pname, params);
    _post_call_gles2_callback(NULL, "glGetQueryObjectui64vEXT", (GLADapiproc) glad_glGetQueryObjectui64vEXT, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTUI64VEXTPROC glad_debug_glGetQueryObjectui64vEXT = glad_debug_impl_glGetQueryObjectui64vEXT;
PFNGLGETQUERYOBJECTUIVPROC glad_glGetQueryObjectuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint * params) {
    _pre_call_gles2_callback("glGetQueryObjectuiv", (GLADapiproc) glad_glGetQueryObjectuiv, 3, id, pname, params);
    glad_glGetQueryObjectuiv(id, pname, params);
    _post_call_gles2_callback(NULL, "glGetQueryObjectuiv", (GLADapiproc) glad_glGetQueryObjectuiv, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTUIVPROC glad_debug_glGetQueryObjectuiv = glad_debug_impl_glGetQueryObjectuiv;
PFNGLGETQUERYOBJECTUIVEXTPROC glad_glGetQueryObjectuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint * params) {
    _pre_call_gles2_callback("glGetQueryObjectuivEXT", (GLADapiproc) glad_glGetQueryObjectuivEXT, 3, id, pname, params);
    glad_glGetQueryObjectuivEXT(id, pname, params);
    _post_call_gles2_callback(NULL, "glGetQueryObjectuivEXT", (GLADapiproc) glad_glGetQueryObjectuivEXT, 3, id, pname, params);
    
}
PFNGLGETQUERYOBJECTUIVEXTPROC glad_debug_glGetQueryObjectuivEXT = glad_debug_impl_glGetQueryObjectuivEXT;
PFNGLGETQUERYIVPROC glad_glGetQueryiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryiv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetQueryiv", (GLADapiproc) glad_glGetQueryiv, 3, target, pname, params);
    glad_glGetQueryiv(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetQueryiv", (GLADapiproc) glad_glGetQueryiv, 3, target, pname, params);
    
}
PFNGLGETQUERYIVPROC glad_debug_glGetQueryiv = glad_debug_impl_glGetQueryiv;
PFNGLGETQUERYIVEXTPROC glad_glGetQueryivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetQueryivEXT(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetQueryivEXT", (GLADapiproc) glad_glGetQueryivEXT, 3, target, pname, params);
    glad_glGetQueryivEXT(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetQueryivEXT", (GLADapiproc) glad_glGetQueryivEXT, 3, target, pname, params);
    
}
PFNGLGETQUERYIVEXTPROC glad_debug_glGetQueryivEXT = glad_debug_impl_glGetQueryivEXT;
PFNGLGETRENDERBUFFERPARAMETERIVPROC glad_glGetRenderbufferParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetRenderbufferParameteriv", (GLADapiproc) glad_glGetRenderbufferParameteriv, 3, target, pname, params);
    glad_glGetRenderbufferParameteriv(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetRenderbufferParameteriv", (GLADapiproc) glad_glGetRenderbufferParameteriv, 3, target, pname, params);
    
}
PFNGLGETRENDERBUFFERPARAMETERIVPROC glad_debug_glGetRenderbufferParameteriv = glad_debug_impl_glGetRenderbufferParameteriv;
PFNGLGETSAMPLERPARAMETERIIVPROC glad_glGetSamplerParameterIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterIiv(GLuint sampler, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetSamplerParameterIiv", (GLADapiproc) glad_glGetSamplerParameterIiv, 3, sampler, pname, params);
    glad_glGetSamplerParameterIiv(sampler, pname, params);
    _post_call_gles2_callback(NULL, "glGetSamplerParameterIiv", (GLADapiproc) glad_glGetSamplerParameterIiv, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIIVPROC glad_debug_glGetSamplerParameterIiv = glad_debug_impl_glGetSamplerParameterIiv;
PFNGLGETSAMPLERPARAMETERIIVEXTPROC glad_glGetSamplerParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterIivEXT(GLuint sampler, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetSamplerParameterIivEXT", (GLADapiproc) glad_glGetSamplerParameterIivEXT, 3, sampler, pname, params);
    glad_glGetSamplerParameterIivEXT(sampler, pname, params);
    _post_call_gles2_callback(NULL, "glGetSamplerParameterIivEXT", (GLADapiproc) glad_glGetSamplerParameterIivEXT, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIIVEXTPROC glad_debug_glGetSamplerParameterIivEXT = glad_debug_impl_glGetSamplerParameterIivEXT;
PFNGLGETSAMPLERPARAMETERIIVOESPROC glad_glGetSamplerParameterIivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterIivOES(GLuint sampler, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetSamplerParameterIivOES", (GLADapiproc) glad_glGetSamplerParameterIivOES, 3, sampler, pname, params);
    glad_glGetSamplerParameterIivOES(sampler, pname, params);
    _post_call_gles2_callback(NULL, "glGetSamplerParameterIivOES", (GLADapiproc) glad_glGetSamplerParameterIivOES, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIIVOESPROC glad_debug_glGetSamplerParameterIivOES = glad_debug_impl_glGetSamplerParameterIivOES;
PFNGLGETSAMPLERPARAMETERIUIVPROC glad_glGetSamplerParameterIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterIuiv(GLuint sampler, GLenum pname, GLuint * params) {
    _pre_call_gles2_callback("glGetSamplerParameterIuiv", (GLADapiproc) glad_glGetSamplerParameterIuiv, 3, sampler, pname, params);
    glad_glGetSamplerParameterIuiv(sampler, pname, params);
    _post_call_gles2_callback(NULL, "glGetSamplerParameterIuiv", (GLADapiproc) glad_glGetSamplerParameterIuiv, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIUIVPROC glad_debug_glGetSamplerParameterIuiv = glad_debug_impl_glGetSamplerParameterIuiv;
PFNGLGETSAMPLERPARAMETERIUIVEXTPROC glad_glGetSamplerParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterIuivEXT(GLuint sampler, GLenum pname, GLuint * params) {
    _pre_call_gles2_callback("glGetSamplerParameterIuivEXT", (GLADapiproc) glad_glGetSamplerParameterIuivEXT, 3, sampler, pname, params);
    glad_glGetSamplerParameterIuivEXT(sampler, pname, params);
    _post_call_gles2_callback(NULL, "glGetSamplerParameterIuivEXT", (GLADapiproc) glad_glGetSamplerParameterIuivEXT, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIUIVEXTPROC glad_debug_glGetSamplerParameterIuivEXT = glad_debug_impl_glGetSamplerParameterIuivEXT;
PFNGLGETSAMPLERPARAMETERIUIVOESPROC glad_glGetSamplerParameterIuivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterIuivOES(GLuint sampler, GLenum pname, GLuint * params) {
    _pre_call_gles2_callback("glGetSamplerParameterIuivOES", (GLADapiproc) glad_glGetSamplerParameterIuivOES, 3, sampler, pname, params);
    glad_glGetSamplerParameterIuivOES(sampler, pname, params);
    _post_call_gles2_callback(NULL, "glGetSamplerParameterIuivOES", (GLADapiproc) glad_glGetSamplerParameterIuivOES, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIUIVOESPROC glad_debug_glGetSamplerParameterIuivOES = glad_debug_impl_glGetSamplerParameterIuivOES;
PFNGLGETSAMPLERPARAMETERFVPROC glad_glGetSamplerParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat * params) {
    _pre_call_gles2_callback("glGetSamplerParameterfv", (GLADapiproc) glad_glGetSamplerParameterfv, 3, sampler, pname, params);
    glad_glGetSamplerParameterfv(sampler, pname, params);
    _post_call_gles2_callback(NULL, "glGetSamplerParameterfv", (GLADapiproc) glad_glGetSamplerParameterfv, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERFVPROC glad_debug_glGetSamplerParameterfv = glad_debug_impl_glGetSamplerParameterfv;
PFNGLGETSAMPLERPARAMETERIVPROC glad_glGetSamplerParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetSamplerParameteriv", (GLADapiproc) glad_glGetSamplerParameteriv, 3, sampler, pname, params);
    glad_glGetSamplerParameteriv(sampler, pname, params);
    _post_call_gles2_callback(NULL, "glGetSamplerParameteriv", (GLADapiproc) glad_glGetSamplerParameteriv, 3, sampler, pname, params);
    
}
PFNGLGETSAMPLERPARAMETERIVPROC glad_debug_glGetSamplerParameteriv = glad_debug_impl_glGetSamplerParameteriv;
PFNGLGETSEMAPHOREPARAMETERIVNVPROC glad_glGetSemaphoreParameterivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSemaphoreParameterivNV(GLuint semaphore, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetSemaphoreParameterivNV", (GLADapiproc) glad_glGetSemaphoreParameterivNV, 3, semaphore, pname, params);
    glad_glGetSemaphoreParameterivNV(semaphore, pname, params);
    _post_call_gles2_callback(NULL, "glGetSemaphoreParameterivNV", (GLADapiproc) glad_glGetSemaphoreParameterivNV, 3, semaphore, pname, params);
    
}
PFNGLGETSEMAPHOREPARAMETERIVNVPROC glad_debug_glGetSemaphoreParameterivNV = glad_debug_impl_glGetSemaphoreParameterivNV;
PFNGLGETSEMAPHOREPARAMETERUI64VEXTPROC glad_glGetSemaphoreParameterui64vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSemaphoreParameterui64vEXT(GLuint semaphore, GLenum pname, GLuint64 * params) {
    _pre_call_gles2_callback("glGetSemaphoreParameterui64vEXT", (GLADapiproc) glad_glGetSemaphoreParameterui64vEXT, 3, semaphore, pname, params);
    glad_glGetSemaphoreParameterui64vEXT(semaphore, pname, params);
    _post_call_gles2_callback(NULL, "glGetSemaphoreParameterui64vEXT", (GLADapiproc) glad_glGetSemaphoreParameterui64vEXT, 3, semaphore, pname, params);
    
}
PFNGLGETSEMAPHOREPARAMETERUI64VEXTPROC glad_debug_glGetSemaphoreParameterui64vEXT = glad_debug_impl_glGetSemaphoreParameterui64vEXT;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog) {
    _pre_call_gles2_callback("glGetShaderInfoLog", (GLADapiproc) glad_glGetShaderInfoLog, 4, shader, bufSize, length, infoLog);
    glad_glGetShaderInfoLog(shader, bufSize, length, infoLog);
    _post_call_gles2_callback(NULL, "glGetShaderInfoLog", (GLADapiproc) glad_glGetShaderInfoLog, 4, shader, bufSize, length, infoLog);
    
}
PFNGLGETSHADERINFOLOGPROC glad_debug_glGetShaderInfoLog = glad_debug_impl_glGetShaderInfoLog;
PFNGLGETSHADERPRECISIONFORMATPROC glad_glGetShaderPrecisionFormat = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint * range, GLint * precision) {
    _pre_call_gles2_callback("glGetShaderPrecisionFormat", (GLADapiproc) glad_glGetShaderPrecisionFormat, 4, shadertype, precisiontype, range, precision);
    glad_glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
    _post_call_gles2_callback(NULL, "glGetShaderPrecisionFormat", (GLADapiproc) glad_glGetShaderPrecisionFormat, 4, shadertype, precisiontype, range, precision);
    
}
PFNGLGETSHADERPRECISIONFORMATPROC glad_debug_glGetShaderPrecisionFormat = glad_debug_impl_glGetShaderPrecisionFormat;
PFNGLGETSHADERSOURCEPROC glad_glGetShaderSource = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * source) {
    _pre_call_gles2_callback("glGetShaderSource", (GLADapiproc) glad_glGetShaderSource, 4, shader, bufSize, length, source);
    glad_glGetShaderSource(shader, bufSize, length, source);
    _post_call_gles2_callback(NULL, "glGetShaderSource", (GLADapiproc) glad_glGetShaderSource, 4, shader, bufSize, length, source);
    
}
PFNGLGETSHADERSOURCEPROC glad_debug_glGetShaderSource = glad_debug_impl_glGetShaderSource;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShaderiv(GLuint shader, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetShaderiv", (GLADapiproc) glad_glGetShaderiv, 3, shader, pname, params);
    glad_glGetShaderiv(shader, pname, params);
    _post_call_gles2_callback(NULL, "glGetShaderiv", (GLADapiproc) glad_glGetShaderiv, 3, shader, pname, params);
    
}
PFNGLGETSHADERIVPROC glad_debug_glGetShaderiv = glad_debug_impl_glGetShaderiv;
PFNGLGETSHADINGRATEIMAGEPALETTENVPROC glad_glGetShadingRateImagePaletteNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShadingRateImagePaletteNV(GLuint viewport, GLuint entry, GLenum * rate) {
    _pre_call_gles2_callback("glGetShadingRateImagePaletteNV", (GLADapiproc) glad_glGetShadingRateImagePaletteNV, 3, viewport, entry, rate);
    glad_glGetShadingRateImagePaletteNV(viewport, entry, rate);
    _post_call_gles2_callback(NULL, "glGetShadingRateImagePaletteNV", (GLADapiproc) glad_glGetShadingRateImagePaletteNV, 3, viewport, entry, rate);
    
}
PFNGLGETSHADINGRATEIMAGEPALETTENVPROC glad_debug_glGetShadingRateImagePaletteNV = glad_debug_impl_glGetShadingRateImagePaletteNV;
PFNGLGETSHADINGRATESAMPLELOCATIONIVNVPROC glad_glGetShadingRateSampleLocationivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetShadingRateSampleLocationivNV(GLenum rate, GLuint samples, GLuint index, GLint * location) {
    _pre_call_gles2_callback("glGetShadingRateSampleLocationivNV", (GLADapiproc) glad_glGetShadingRateSampleLocationivNV, 4, rate, samples, index, location);
    glad_glGetShadingRateSampleLocationivNV(rate, samples, index, location);
    _post_call_gles2_callback(NULL, "glGetShadingRateSampleLocationivNV", (GLADapiproc) glad_glGetShadingRateSampleLocationivNV, 4, rate, samples, index, location);
    
}
PFNGLGETSHADINGRATESAMPLELOCATIONIVNVPROC glad_debug_glGetShadingRateSampleLocationivNV = glad_debug_impl_glGetShadingRateSampleLocationivNV;
PFNGLGETSTRINGPROC glad_glGetString = NULL;
static const GLubyte * GLAD_API_PTR glad_debug_impl_glGetString(GLenum name) {
    const GLubyte * ret;
    _pre_call_gles2_callback("glGetString", (GLADapiproc) glad_glGetString, 1, name);
    ret = glad_glGetString(name);
    _post_call_gles2_callback((void*) &ret, "glGetString", (GLADapiproc) glad_glGetString, 1, name);
    return ret;
}
PFNGLGETSTRINGPROC glad_debug_glGetString = glad_debug_impl_glGetString;
PFNGLGETSTRINGIPROC glad_glGetStringi = NULL;
static const GLubyte * GLAD_API_PTR glad_debug_impl_glGetStringi(GLenum name, GLuint index) {
    const GLubyte * ret;
    _pre_call_gles2_callback("glGetStringi", (GLADapiproc) glad_glGetStringi, 2, name, index);
    ret = glad_glGetStringi(name, index);
    _post_call_gles2_callback((void*) &ret, "glGetStringi", (GLADapiproc) glad_glGetStringi, 2, name, index);
    return ret;
}
PFNGLGETSTRINGIPROC glad_debug_glGetStringi = glad_debug_impl_glGetStringi;
PFNGLGETSYNCIVPROC glad_glGetSynciv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSynciv(GLsync sync, GLenum pname, GLsizei count, GLsizei * length, GLint * values) {
    _pre_call_gles2_callback("glGetSynciv", (GLADapiproc) glad_glGetSynciv, 5, sync, pname, count, length, values);
    glad_glGetSynciv(sync, pname, count, length, values);
    _post_call_gles2_callback(NULL, "glGetSynciv", (GLADapiproc) glad_glGetSynciv, 5, sync, pname, count, length, values);
    
}
PFNGLGETSYNCIVPROC glad_debug_glGetSynciv = glad_debug_impl_glGetSynciv;
PFNGLGETSYNCIVAPPLEPROC glad_glGetSyncivAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetSyncivAPPLE(GLsync sync, GLenum pname, GLsizei count, GLsizei * length, GLint * values) {
    _pre_call_gles2_callback("glGetSyncivAPPLE", (GLADapiproc) glad_glGetSyncivAPPLE, 5, sync, pname, count, length, values);
    glad_glGetSyncivAPPLE(sync, pname, count, length, values);
    _post_call_gles2_callback(NULL, "glGetSyncivAPPLE", (GLADapiproc) glad_glGetSyncivAPPLE, 5, sync, pname, count, length, values);
    
}
PFNGLGETSYNCIVAPPLEPROC glad_debug_glGetSyncivAPPLE = glad_debug_impl_glGetSyncivAPPLE;
PFNGLGETTEXLEVELPARAMETERFVPROC glad_glGetTexLevelParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexLevelParameterfv(GLenum target, GLint level, GLenum pname, GLfloat * params) {
    _pre_call_gles2_callback("glGetTexLevelParameterfv", (GLADapiproc) glad_glGetTexLevelParameterfv, 4, target, level, pname, params);
    glad_glGetTexLevelParameterfv(target, level, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexLevelParameterfv", (GLADapiproc) glad_glGetTexLevelParameterfv, 4, target, level, pname, params);
    
}
PFNGLGETTEXLEVELPARAMETERFVPROC glad_debug_glGetTexLevelParameterfv = glad_debug_impl_glGetTexLevelParameterfv;
PFNGLGETTEXLEVELPARAMETERIVPROC glad_glGetTexLevelParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetTexLevelParameteriv", (GLADapiproc) glad_glGetTexLevelParameteriv, 4, target, level, pname, params);
    glad_glGetTexLevelParameteriv(target, level, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexLevelParameteriv", (GLADapiproc) glad_glGetTexLevelParameteriv, 4, target, level, pname, params);
    
}
PFNGLGETTEXLEVELPARAMETERIVPROC glad_debug_glGetTexLevelParameteriv = glad_debug_impl_glGetTexLevelParameteriv;
PFNGLGETTEXPARAMETERIIVPROC glad_glGetTexParameterIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIiv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetTexParameterIiv", (GLADapiproc) glad_glGetTexParameterIiv, 3, target, pname, params);
    glad_glGetTexParameterIiv(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexParameterIiv", (GLADapiproc) glad_glGetTexParameterIiv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIIVPROC glad_debug_glGetTexParameterIiv = glad_debug_impl_glGetTexParameterIiv;
PFNGLGETTEXPARAMETERIIVEXTPROC glad_glGetTexParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIivEXT(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetTexParameterIivEXT", (GLADapiproc) glad_glGetTexParameterIivEXT, 3, target, pname, params);
    glad_glGetTexParameterIivEXT(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexParameterIivEXT", (GLADapiproc) glad_glGetTexParameterIivEXT, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIIVEXTPROC glad_debug_glGetTexParameterIivEXT = glad_debug_impl_glGetTexParameterIivEXT;
PFNGLGETTEXPARAMETERIIVOESPROC glad_glGetTexParameterIivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIivOES(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetTexParameterIivOES", (GLADapiproc) glad_glGetTexParameterIivOES, 3, target, pname, params);
    glad_glGetTexParameterIivOES(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexParameterIivOES", (GLADapiproc) glad_glGetTexParameterIivOES, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIIVOESPROC glad_debug_glGetTexParameterIivOES = glad_debug_impl_glGetTexParameterIivOES;
PFNGLGETTEXPARAMETERIUIVPROC glad_glGetTexParameterIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIuiv(GLenum target, GLenum pname, GLuint * params) {
    _pre_call_gles2_callback("glGetTexParameterIuiv", (GLADapiproc) glad_glGetTexParameterIuiv, 3, target, pname, params);
    glad_glGetTexParameterIuiv(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexParameterIuiv", (GLADapiproc) glad_glGetTexParameterIuiv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIUIVPROC glad_debug_glGetTexParameterIuiv = glad_debug_impl_glGetTexParameterIuiv;
PFNGLGETTEXPARAMETERIUIVEXTPROC glad_glGetTexParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIuivEXT(GLenum target, GLenum pname, GLuint * params) {
    _pre_call_gles2_callback("glGetTexParameterIuivEXT", (GLADapiproc) glad_glGetTexParameterIuivEXT, 3, target, pname, params);
    glad_glGetTexParameterIuivEXT(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexParameterIuivEXT", (GLADapiproc) glad_glGetTexParameterIuivEXT, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIUIVEXTPROC glad_debug_glGetTexParameterIuivEXT = glad_debug_impl_glGetTexParameterIuivEXT;
PFNGLGETTEXPARAMETERIUIVOESPROC glad_glGetTexParameterIuivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterIuivOES(GLenum target, GLenum pname, GLuint * params) {
    _pre_call_gles2_callback("glGetTexParameterIuivOES", (GLADapiproc) glad_glGetTexParameterIuivOES, 3, target, pname, params);
    glad_glGetTexParameterIuivOES(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexParameterIuivOES", (GLADapiproc) glad_glGetTexParameterIuivOES, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIUIVOESPROC glad_debug_glGetTexParameterIuivOES = glad_debug_impl_glGetTexParameterIuivOES;
PFNGLGETTEXPARAMETERFVPROC glad_glGetTexParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat * params) {
    _pre_call_gles2_callback("glGetTexParameterfv", (GLADapiproc) glad_glGetTexParameterfv, 3, target, pname, params);
    glad_glGetTexParameterfv(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexParameterfv", (GLADapiproc) glad_glGetTexParameterfv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERFVPROC glad_debug_glGetTexParameterfv = glad_debug_impl_glGetTexParameterfv;
PFNGLGETTEXPARAMETERIVPROC glad_glGetTexParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTexParameteriv(GLenum target, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetTexParameteriv", (GLADapiproc) glad_glGetTexParameteriv, 3, target, pname, params);
    glad_glGetTexParameteriv(target, pname, params);
    _post_call_gles2_callback(NULL, "glGetTexParameteriv", (GLADapiproc) glad_glGetTexParameteriv, 3, target, pname, params);
    
}
PFNGLGETTEXPARAMETERIVPROC glad_debug_glGetTexParameteriv = glad_debug_impl_glGetTexParameteriv;
PFNGLGETTEXTUREHANDLEIMGPROC glad_glGetTextureHandleIMG = NULL;
static GLuint64 GLAD_API_PTR glad_debug_impl_glGetTextureHandleIMG(GLuint texture) {
    GLuint64 ret;
    _pre_call_gles2_callback("glGetTextureHandleIMG", (GLADapiproc) glad_glGetTextureHandleIMG, 1, texture);
    ret = glad_glGetTextureHandleIMG(texture);
    _post_call_gles2_callback((void*) &ret, "glGetTextureHandleIMG", (GLADapiproc) glad_glGetTextureHandleIMG, 1, texture);
    return ret;
}
PFNGLGETTEXTUREHANDLEIMGPROC glad_debug_glGetTextureHandleIMG = glad_debug_impl_glGetTextureHandleIMG;
PFNGLGETTEXTUREHANDLENVPROC glad_glGetTextureHandleNV = NULL;
static GLuint64 GLAD_API_PTR glad_debug_impl_glGetTextureHandleNV(GLuint texture) {
    GLuint64 ret;
    _pre_call_gles2_callback("glGetTextureHandleNV", (GLADapiproc) glad_glGetTextureHandleNV, 1, texture);
    ret = glad_glGetTextureHandleNV(texture);
    _post_call_gles2_callback((void*) &ret, "glGetTextureHandleNV", (GLADapiproc) glad_glGetTextureHandleNV, 1, texture);
    return ret;
}
PFNGLGETTEXTUREHANDLENVPROC glad_debug_glGetTextureHandleNV = glad_debug_impl_glGetTextureHandleNV;
PFNGLGETTEXTURESAMPLERHANDLEIMGPROC glad_glGetTextureSamplerHandleIMG = NULL;
static GLuint64 GLAD_API_PTR glad_debug_impl_glGetTextureSamplerHandleIMG(GLuint texture, GLuint sampler) {
    GLuint64 ret;
    _pre_call_gles2_callback("glGetTextureSamplerHandleIMG", (GLADapiproc) glad_glGetTextureSamplerHandleIMG, 2, texture, sampler);
    ret = glad_glGetTextureSamplerHandleIMG(texture, sampler);
    _post_call_gles2_callback((void*) &ret, "glGetTextureSamplerHandleIMG", (GLADapiproc) glad_glGetTextureSamplerHandleIMG, 2, texture, sampler);
    return ret;
}
PFNGLGETTEXTURESAMPLERHANDLEIMGPROC glad_debug_glGetTextureSamplerHandleIMG = glad_debug_impl_glGetTextureSamplerHandleIMG;
PFNGLGETTEXTURESAMPLERHANDLENVPROC glad_glGetTextureSamplerHandleNV = NULL;
static GLuint64 GLAD_API_PTR glad_debug_impl_glGetTextureSamplerHandleNV(GLuint texture, GLuint sampler) {
    GLuint64 ret;
    _pre_call_gles2_callback("glGetTextureSamplerHandleNV", (GLADapiproc) glad_glGetTextureSamplerHandleNV, 2, texture, sampler);
    ret = glad_glGetTextureSamplerHandleNV(texture, sampler);
    _post_call_gles2_callback((void*) &ret, "glGetTextureSamplerHandleNV", (GLADapiproc) glad_glGetTextureSamplerHandleNV, 2, texture, sampler);
    return ret;
}
PFNGLGETTEXTURESAMPLERHANDLENVPROC glad_debug_glGetTextureSamplerHandleNV = glad_debug_impl_glGetTextureSamplerHandleNV;
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glad_glGetTransformFeedbackVarying = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei * length, GLsizei * size, GLenum * type, GLchar * name) {
    _pre_call_gles2_callback("glGetTransformFeedbackVarying", (GLADapiproc) glad_glGetTransformFeedbackVarying, 7, program, index, bufSize, length, size, type, name);
    glad_glGetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
    _post_call_gles2_callback(NULL, "glGetTransformFeedbackVarying", (GLADapiproc) glad_glGetTransformFeedbackVarying, 7, program, index, bufSize, length, size, type, name);
    
}
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC glad_debug_glGetTransformFeedbackVarying = glad_debug_impl_glGetTransformFeedbackVarying;
PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC glad_glGetTranslatedShaderSourceANGLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetTranslatedShaderSourceANGLE(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * source) {
    _pre_call_gles2_callback("glGetTranslatedShaderSourceANGLE", (GLADapiproc) glad_glGetTranslatedShaderSourceANGLE, 4, shader, bufSize, length, source);
    glad_glGetTranslatedShaderSourceANGLE(shader, bufSize, length, source);
    _post_call_gles2_callback(NULL, "glGetTranslatedShaderSourceANGLE", (GLADapiproc) glad_glGetTranslatedShaderSourceANGLE, 4, shader, bufSize, length, source);
    
}
PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC glad_debug_glGetTranslatedShaderSourceANGLE = glad_debug_impl_glGetTranslatedShaderSourceANGLE;
PFNGLGETUNIFORMBLOCKINDEXPROC glad_glGetUniformBlockIndex = NULL;
static GLuint GLAD_API_PTR glad_debug_impl_glGetUniformBlockIndex(GLuint program, const GLchar * uniformBlockName) {
    GLuint ret;
    _pre_call_gles2_callback("glGetUniformBlockIndex", (GLADapiproc) glad_glGetUniformBlockIndex, 2, program, uniformBlockName);
    ret = glad_glGetUniformBlockIndex(program, uniformBlockName);
    _post_call_gles2_callback((void*) &ret, "glGetUniformBlockIndex", (GLADapiproc) glad_glGetUniformBlockIndex, 2, program, uniformBlockName);
    return ret;
}
PFNGLGETUNIFORMBLOCKINDEXPROC glad_debug_glGetUniformBlockIndex = glad_debug_impl_glGetUniformBlockIndex;
PFNGLGETUNIFORMINDICESPROC glad_glGetUniformIndices = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const* uniformNames, GLuint * uniformIndices) {
    _pre_call_gles2_callback("glGetUniformIndices", (GLADapiproc) glad_glGetUniformIndices, 4, program, uniformCount, uniformNames, uniformIndices);
    glad_glGetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
    _post_call_gles2_callback(NULL, "glGetUniformIndices", (GLADapiproc) glad_glGetUniformIndices, 4, program, uniformCount, uniformNames, uniformIndices);
    
}
PFNGLGETUNIFORMINDICESPROC glad_debug_glGetUniformIndices = glad_debug_impl_glGetUniformIndices;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = NULL;
static GLint GLAD_API_PTR glad_debug_impl_glGetUniformLocation(GLuint program, const GLchar * name) {
    GLint ret;
    _pre_call_gles2_callback("glGetUniformLocation", (GLADapiproc) glad_glGetUniformLocation, 2, program, name);
    ret = glad_glGetUniformLocation(program, name);
    _post_call_gles2_callback((void*) &ret, "glGetUniformLocation", (GLADapiproc) glad_glGetUniformLocation, 2, program, name);
    return ret;
}
PFNGLGETUNIFORMLOCATIONPROC glad_debug_glGetUniformLocation = glad_debug_impl_glGetUniformLocation;
PFNGLGETUNIFORMFVPROC glad_glGetUniformfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformfv(GLuint program, GLint location, GLfloat * params) {
    _pre_call_gles2_callback("glGetUniformfv", (GLADapiproc) glad_glGetUniformfv, 3, program, location, params);
    glad_glGetUniformfv(program, location, params);
    _post_call_gles2_callback(NULL, "glGetUniformfv", (GLADapiproc) glad_glGetUniformfv, 3, program, location, params);
    
}
PFNGLGETUNIFORMFVPROC glad_debug_glGetUniformfv = glad_debug_impl_glGetUniformfv;
PFNGLGETUNIFORMI64VNVPROC glad_glGetUniformi64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformi64vNV(GLuint program, GLint location, GLint64EXT * params) {
    _pre_call_gles2_callback("glGetUniformi64vNV", (GLADapiproc) glad_glGetUniformi64vNV, 3, program, location, params);
    glad_glGetUniformi64vNV(program, location, params);
    _post_call_gles2_callback(NULL, "glGetUniformi64vNV", (GLADapiproc) glad_glGetUniformi64vNV, 3, program, location, params);
    
}
PFNGLGETUNIFORMI64VNVPROC glad_debug_glGetUniformi64vNV = glad_debug_impl_glGetUniformi64vNV;
PFNGLGETUNIFORMIVPROC glad_glGetUniformiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformiv(GLuint program, GLint location, GLint * params) {
    _pre_call_gles2_callback("glGetUniformiv", (GLADapiproc) glad_glGetUniformiv, 3, program, location, params);
    glad_glGetUniformiv(program, location, params);
    _post_call_gles2_callback(NULL, "glGetUniformiv", (GLADapiproc) glad_glGetUniformiv, 3, program, location, params);
    
}
PFNGLGETUNIFORMIVPROC glad_debug_glGetUniformiv = glad_debug_impl_glGetUniformiv;
PFNGLGETUNIFORMUIVPROC glad_glGetUniformuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUniformuiv(GLuint program, GLint location, GLuint * params) {
    _pre_call_gles2_callback("glGetUniformuiv", (GLADapiproc) glad_glGetUniformuiv, 3, program, location, params);
    glad_glGetUniformuiv(program, location, params);
    _post_call_gles2_callback(NULL, "glGetUniformuiv", (GLADapiproc) glad_glGetUniformuiv, 3, program, location, params);
    
}
PFNGLGETUNIFORMUIVPROC glad_debug_glGetUniformuiv = glad_debug_impl_glGetUniformuiv;
PFNGLGETUNSIGNEDBYTEI_VEXTPROC glad_glGetUnsignedBytei_vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUnsignedBytei_vEXT(GLenum target, GLuint index, GLubyte * data) {
    _pre_call_gles2_callback("glGetUnsignedBytei_vEXT", (GLADapiproc) glad_glGetUnsignedBytei_vEXT, 3, target, index, data);
    glad_glGetUnsignedBytei_vEXT(target, index, data);
    _post_call_gles2_callback(NULL, "glGetUnsignedBytei_vEXT", (GLADapiproc) glad_glGetUnsignedBytei_vEXT, 3, target, index, data);
    
}
PFNGLGETUNSIGNEDBYTEI_VEXTPROC glad_debug_glGetUnsignedBytei_vEXT = glad_debug_impl_glGetUnsignedBytei_vEXT;
PFNGLGETUNSIGNEDBYTEVEXTPROC glad_glGetUnsignedBytevEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetUnsignedBytevEXT(GLenum pname, GLubyte * data) {
    _pre_call_gles2_callback("glGetUnsignedBytevEXT", (GLADapiproc) glad_glGetUnsignedBytevEXT, 2, pname, data);
    glad_glGetUnsignedBytevEXT(pname, data);
    _post_call_gles2_callback(NULL, "glGetUnsignedBytevEXT", (GLADapiproc) glad_glGetUnsignedBytevEXT, 2, pname, data);
    
}
PFNGLGETUNSIGNEDBYTEVEXTPROC glad_debug_glGetUnsignedBytevEXT = glad_debug_impl_glGetUnsignedBytevEXT;
PFNGLGETVERTEXATTRIBIIVPROC glad_glGetVertexAttribIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribIiv(GLuint index, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetVertexAttribIiv", (GLADapiproc) glad_glGetVertexAttribIiv, 3, index, pname, params);
    glad_glGetVertexAttribIiv(index, pname, params);
    _post_call_gles2_callback(NULL, "glGetVertexAttribIiv", (GLADapiproc) glad_glGetVertexAttribIiv, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIIVPROC glad_debug_glGetVertexAttribIiv = glad_debug_impl_glGetVertexAttribIiv;
PFNGLGETVERTEXATTRIBIUIVPROC glad_glGetVertexAttribIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint * params) {
    _pre_call_gles2_callback("glGetVertexAttribIuiv", (GLADapiproc) glad_glGetVertexAttribIuiv, 3, index, pname, params);
    glad_glGetVertexAttribIuiv(index, pname, params);
    _post_call_gles2_callback(NULL, "glGetVertexAttribIuiv", (GLADapiproc) glad_glGetVertexAttribIuiv, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIUIVPROC glad_debug_glGetVertexAttribIuiv = glad_debug_impl_glGetVertexAttribIuiv;
PFNGLGETVERTEXATTRIBPOINTERVPROC glad_glGetVertexAttribPointerv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribPointerv(GLuint index, GLenum pname, void ** pointer) {
    _pre_call_gles2_callback("glGetVertexAttribPointerv", (GLADapiproc) glad_glGetVertexAttribPointerv, 3, index, pname, pointer);
    glad_glGetVertexAttribPointerv(index, pname, pointer);
    _post_call_gles2_callback(NULL, "glGetVertexAttribPointerv", (GLADapiproc) glad_glGetVertexAttribPointerv, 3, index, pname, pointer);
    
}
PFNGLGETVERTEXATTRIBPOINTERVPROC glad_debug_glGetVertexAttribPointerv = glad_debug_impl_glGetVertexAttribPointerv;
PFNGLGETVERTEXATTRIBFVPROC glad_glGetVertexAttribfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat * params) {
    _pre_call_gles2_callback("glGetVertexAttribfv", (GLADapiproc) glad_glGetVertexAttribfv, 3, index, pname, params);
    glad_glGetVertexAttribfv(index, pname, params);
    _post_call_gles2_callback(NULL, "glGetVertexAttribfv", (GLADapiproc) glad_glGetVertexAttribfv, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBFVPROC glad_debug_glGetVertexAttribfv = glad_debug_impl_glGetVertexAttribfv;
PFNGLGETVERTEXATTRIBIVPROC glad_glGetVertexAttribiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetVertexAttribiv(GLuint index, GLenum pname, GLint * params) {
    _pre_call_gles2_callback("glGetVertexAttribiv", (GLADapiproc) glad_glGetVertexAttribiv, 3, index, pname, params);
    glad_glGetVertexAttribiv(index, pname, params);
    _post_call_gles2_callback(NULL, "glGetVertexAttribiv", (GLADapiproc) glad_glGetVertexAttribiv, 3, index, pname, params);
    
}
PFNGLGETVERTEXATTRIBIVPROC glad_debug_glGetVertexAttribiv = glad_debug_impl_glGetVertexAttribiv;
PFNGLGETVKPROCADDRNVPROC glad_glGetVkProcAddrNV = NULL;
static GLVULKANPROCNV GLAD_API_PTR glad_debug_impl_glGetVkProcAddrNV(const GLchar * name) {
    GLVULKANPROCNV ret;
    _pre_call_gles2_callback("glGetVkProcAddrNV", (GLADapiproc) glad_glGetVkProcAddrNV, 1, name);
    ret = glad_glGetVkProcAddrNV(name);
    _post_call_gles2_callback((void*) &ret, "glGetVkProcAddrNV", (GLADapiproc) glad_glGetVkProcAddrNV, 1, name);
    return ret;
}
PFNGLGETVKPROCADDRNVPROC glad_debug_glGetVkProcAddrNV = glad_debug_impl_glGetVkProcAddrNV;
PFNGLGETNUNIFORMFVPROC glad_glGetnUniformfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat * params) {
    _pre_call_gles2_callback("glGetnUniformfv", (GLADapiproc) glad_glGetnUniformfv, 4, program, location, bufSize, params);
    glad_glGetnUniformfv(program, location, bufSize, params);
    _post_call_gles2_callback(NULL, "glGetnUniformfv", (GLADapiproc) glad_glGetnUniformfv, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMFVPROC glad_debug_glGetnUniformfv = glad_debug_impl_glGetnUniformfv;
PFNGLGETNUNIFORMFVEXTPROC glad_glGetnUniformfvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformfvEXT(GLuint program, GLint location, GLsizei bufSize, GLfloat * params) {
    _pre_call_gles2_callback("glGetnUniformfvEXT", (GLADapiproc) glad_glGetnUniformfvEXT, 4, program, location, bufSize, params);
    glad_glGetnUniformfvEXT(program, location, bufSize, params);
    _post_call_gles2_callback(NULL, "glGetnUniformfvEXT", (GLADapiproc) glad_glGetnUniformfvEXT, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMFVEXTPROC glad_debug_glGetnUniformfvEXT = glad_debug_impl_glGetnUniformfvEXT;
PFNGLGETNUNIFORMFVKHRPROC glad_glGetnUniformfvKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformfvKHR(GLuint program, GLint location, GLsizei bufSize, GLfloat * params) {
    _pre_call_gles2_callback("glGetnUniformfvKHR", (GLADapiproc) glad_glGetnUniformfvKHR, 4, program, location, bufSize, params);
    glad_glGetnUniformfvKHR(program, location, bufSize, params);
    _post_call_gles2_callback(NULL, "glGetnUniformfvKHR", (GLADapiproc) glad_glGetnUniformfvKHR, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMFVKHRPROC glad_debug_glGetnUniformfvKHR = glad_debug_impl_glGetnUniformfvKHR;
PFNGLGETNUNIFORMIVPROC glad_glGetnUniformiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint * params) {
    _pre_call_gles2_callback("glGetnUniformiv", (GLADapiproc) glad_glGetnUniformiv, 4, program, location, bufSize, params);
    glad_glGetnUniformiv(program, location, bufSize, params);
    _post_call_gles2_callback(NULL, "glGetnUniformiv", (GLADapiproc) glad_glGetnUniformiv, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMIVPROC glad_debug_glGetnUniformiv = glad_debug_impl_glGetnUniformiv;
PFNGLGETNUNIFORMIVEXTPROC glad_glGetnUniformivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformivEXT(GLuint program, GLint location, GLsizei bufSize, GLint * params) {
    _pre_call_gles2_callback("glGetnUniformivEXT", (GLADapiproc) glad_glGetnUniformivEXT, 4, program, location, bufSize, params);
    glad_glGetnUniformivEXT(program, location, bufSize, params);
    _post_call_gles2_callback(NULL, "glGetnUniformivEXT", (GLADapiproc) glad_glGetnUniformivEXT, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMIVEXTPROC glad_debug_glGetnUniformivEXT = glad_debug_impl_glGetnUniformivEXT;
PFNGLGETNUNIFORMIVKHRPROC glad_glGetnUniformivKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformivKHR(GLuint program, GLint location, GLsizei bufSize, GLint * params) {
    _pre_call_gles2_callback("glGetnUniformivKHR", (GLADapiproc) glad_glGetnUniformivKHR, 4, program, location, bufSize, params);
    glad_glGetnUniformivKHR(program, location, bufSize, params);
    _post_call_gles2_callback(NULL, "glGetnUniformivKHR", (GLADapiproc) glad_glGetnUniformivKHR, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMIVKHRPROC glad_debug_glGetnUniformivKHR = glad_debug_impl_glGetnUniformivKHR;
PFNGLGETNUNIFORMUIVPROC glad_glGetnUniformuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformuiv(GLuint program, GLint location, GLsizei bufSize, GLuint * params) {
    _pre_call_gles2_callback("glGetnUniformuiv", (GLADapiproc) glad_glGetnUniformuiv, 4, program, location, bufSize, params);
    glad_glGetnUniformuiv(program, location, bufSize, params);
    _post_call_gles2_callback(NULL, "glGetnUniformuiv", (GLADapiproc) glad_glGetnUniformuiv, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMUIVPROC glad_debug_glGetnUniformuiv = glad_debug_impl_glGetnUniformuiv;
PFNGLGETNUNIFORMUIVKHRPROC glad_glGetnUniformuivKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glGetnUniformuivKHR(GLuint program, GLint location, GLsizei bufSize, GLuint * params) {
    _pre_call_gles2_callback("glGetnUniformuivKHR", (GLADapiproc) glad_glGetnUniformuivKHR, 4, program, location, bufSize, params);
    glad_glGetnUniformuivKHR(program, location, bufSize, params);
    _post_call_gles2_callback(NULL, "glGetnUniformuivKHR", (GLADapiproc) glad_glGetnUniformuivKHR, 4, program, location, bufSize, params);
    
}
PFNGLGETNUNIFORMUIVKHRPROC glad_debug_glGetnUniformuivKHR = glad_debug_impl_glGetnUniformuivKHR;
PFNGLHINTPROC glad_glHint = NULL;
static void GLAD_API_PTR glad_debug_impl_glHint(GLenum target, GLenum mode) {
    _pre_call_gles2_callback("glHint", (GLADapiproc) glad_glHint, 2, target, mode);
    glad_glHint(target, mode);
    _post_call_gles2_callback(NULL, "glHint", (GLADapiproc) glad_glHint, 2, target, mode);
    
}
PFNGLHINTPROC glad_debug_glHint = glad_debug_impl_glHint;
PFNGLIMPORTMEMORYFDEXTPROC glad_glImportMemoryFdEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glImportMemoryFdEXT(GLuint memory, GLuint64 size, GLenum handleType, GLint fd) {
    _pre_call_gles2_callback("glImportMemoryFdEXT", (GLADapiproc) glad_glImportMemoryFdEXT, 4, memory, size, handleType, fd);
    glad_glImportMemoryFdEXT(memory, size, handleType, fd);
    _post_call_gles2_callback(NULL, "glImportMemoryFdEXT", (GLADapiproc) glad_glImportMemoryFdEXT, 4, memory, size, handleType, fd);
    
}
PFNGLIMPORTMEMORYFDEXTPROC glad_debug_glImportMemoryFdEXT = glad_debug_impl_glImportMemoryFdEXT;
PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC glad_glImportMemoryWin32HandleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glImportMemoryWin32HandleEXT(GLuint memory, GLuint64 size, GLenum handleType, void * handle) {
    _pre_call_gles2_callback("glImportMemoryWin32HandleEXT", (GLADapiproc) glad_glImportMemoryWin32HandleEXT, 4, memory, size, handleType, handle);
    glad_glImportMemoryWin32HandleEXT(memory, size, handleType, handle);
    _post_call_gles2_callback(NULL, "glImportMemoryWin32HandleEXT", (GLADapiproc) glad_glImportMemoryWin32HandleEXT, 4, memory, size, handleType, handle);
    
}
PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC glad_debug_glImportMemoryWin32HandleEXT = glad_debug_impl_glImportMemoryWin32HandleEXT;
PFNGLIMPORTMEMORYWIN32NAMEEXTPROC glad_glImportMemoryWin32NameEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glImportMemoryWin32NameEXT(GLuint memory, GLuint64 size, GLenum handleType, const void * name) {
    _pre_call_gles2_callback("glImportMemoryWin32NameEXT", (GLADapiproc) glad_glImportMemoryWin32NameEXT, 4, memory, size, handleType, name);
    glad_glImportMemoryWin32NameEXT(memory, size, handleType, name);
    _post_call_gles2_callback(NULL, "glImportMemoryWin32NameEXT", (GLADapiproc) glad_glImportMemoryWin32NameEXT, 4, memory, size, handleType, name);
    
}
PFNGLIMPORTMEMORYWIN32NAMEEXTPROC glad_debug_glImportMemoryWin32NameEXT = glad_debug_impl_glImportMemoryWin32NameEXT;
PFNGLIMPORTSEMAPHOREFDEXTPROC glad_glImportSemaphoreFdEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glImportSemaphoreFdEXT(GLuint semaphore, GLenum handleType, GLint fd) {
    _pre_call_gles2_callback("glImportSemaphoreFdEXT", (GLADapiproc) glad_glImportSemaphoreFdEXT, 3, semaphore, handleType, fd);
    glad_glImportSemaphoreFdEXT(semaphore, handleType, fd);
    _post_call_gles2_callback(NULL, "glImportSemaphoreFdEXT", (GLADapiproc) glad_glImportSemaphoreFdEXT, 3, semaphore, handleType, fd);
    
}
PFNGLIMPORTSEMAPHOREFDEXTPROC glad_debug_glImportSemaphoreFdEXT = glad_debug_impl_glImportSemaphoreFdEXT;
PFNGLIMPORTSEMAPHOREWIN32HANDLEEXTPROC glad_glImportSemaphoreWin32HandleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glImportSemaphoreWin32HandleEXT(GLuint semaphore, GLenum handleType, void * handle) {
    _pre_call_gles2_callback("glImportSemaphoreWin32HandleEXT", (GLADapiproc) glad_glImportSemaphoreWin32HandleEXT, 3, semaphore, handleType, handle);
    glad_glImportSemaphoreWin32HandleEXT(semaphore, handleType, handle);
    _post_call_gles2_callback(NULL, "glImportSemaphoreWin32HandleEXT", (GLADapiproc) glad_glImportSemaphoreWin32HandleEXT, 3, semaphore, handleType, handle);
    
}
PFNGLIMPORTSEMAPHOREWIN32HANDLEEXTPROC glad_debug_glImportSemaphoreWin32HandleEXT = glad_debug_impl_glImportSemaphoreWin32HandleEXT;
PFNGLIMPORTSEMAPHOREWIN32NAMEEXTPROC glad_glImportSemaphoreWin32NameEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glImportSemaphoreWin32NameEXT(GLuint semaphore, GLenum handleType, const void * name) {
    _pre_call_gles2_callback("glImportSemaphoreWin32NameEXT", (GLADapiproc) glad_glImportSemaphoreWin32NameEXT, 3, semaphore, handleType, name);
    glad_glImportSemaphoreWin32NameEXT(semaphore, handleType, name);
    _post_call_gles2_callback(NULL, "glImportSemaphoreWin32NameEXT", (GLADapiproc) glad_glImportSemaphoreWin32NameEXT, 3, semaphore, handleType, name);
    
}
PFNGLIMPORTSEMAPHOREWIN32NAMEEXTPROC glad_debug_glImportSemaphoreWin32NameEXT = glad_debug_impl_glImportSemaphoreWin32NameEXT;
PFNGLINSERTEVENTMARKEREXTPROC glad_glInsertEventMarkerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glInsertEventMarkerEXT(GLsizei length, const GLchar * marker) {
    _pre_call_gles2_callback("glInsertEventMarkerEXT", (GLADapiproc) glad_glInsertEventMarkerEXT, 2, length, marker);
    glad_glInsertEventMarkerEXT(length, marker);
    _post_call_gles2_callback(NULL, "glInsertEventMarkerEXT", (GLADapiproc) glad_glInsertEventMarkerEXT, 2, length, marker);
    
}
PFNGLINSERTEVENTMARKEREXTPROC glad_debug_glInsertEventMarkerEXT = glad_debug_impl_glInsertEventMarkerEXT;
PFNGLINTERPOLATEPATHSNVPROC glad_glInterpolatePathsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glInterpolatePathsNV(GLuint resultPath, GLuint pathA, GLuint pathB, GLfloat weight) {
    _pre_call_gles2_callback("glInterpolatePathsNV", (GLADapiproc) glad_glInterpolatePathsNV, 4, resultPath, pathA, pathB, weight);
    glad_glInterpolatePathsNV(resultPath, pathA, pathB, weight);
    _post_call_gles2_callback(NULL, "glInterpolatePathsNV", (GLADapiproc) glad_glInterpolatePathsNV, 4, resultPath, pathA, pathB, weight);
    
}
PFNGLINTERPOLATEPATHSNVPROC glad_debug_glInterpolatePathsNV = glad_debug_impl_glInterpolatePathsNV;
PFNGLINVALIDATEFRAMEBUFFERPROC glad_glInvalidateFramebuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum * attachments) {
    _pre_call_gles2_callback("glInvalidateFramebuffer", (GLADapiproc) glad_glInvalidateFramebuffer, 3, target, numAttachments, attachments);
    glad_glInvalidateFramebuffer(target, numAttachments, attachments);
    _post_call_gles2_callback(NULL, "glInvalidateFramebuffer", (GLADapiproc) glad_glInvalidateFramebuffer, 3, target, numAttachments, attachments);
    
}
PFNGLINVALIDATEFRAMEBUFFERPROC glad_debug_glInvalidateFramebuffer = glad_debug_impl_glInvalidateFramebuffer;
PFNGLINVALIDATESUBFRAMEBUFFERPROC glad_glInvalidateSubFramebuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum * attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glInvalidateSubFramebuffer", (GLADapiproc) glad_glInvalidateSubFramebuffer, 7, target, numAttachments, attachments, x, y, width, height);
    glad_glInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
    _post_call_gles2_callback(NULL, "glInvalidateSubFramebuffer", (GLADapiproc) glad_glInvalidateSubFramebuffer, 7, target, numAttachments, attachments, x, y, width, height);
    
}
PFNGLINVALIDATESUBFRAMEBUFFERPROC glad_debug_glInvalidateSubFramebuffer = glad_debug_impl_glInvalidateSubFramebuffer;
PFNGLISBUFFERPROC glad_glIsBuffer = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsBuffer(GLuint buffer) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsBuffer", (GLADapiproc) glad_glIsBuffer, 1, buffer);
    ret = glad_glIsBuffer(buffer);
    _post_call_gles2_callback((void*) &ret, "glIsBuffer", (GLADapiproc) glad_glIsBuffer, 1, buffer);
    return ret;
}
PFNGLISBUFFERPROC glad_debug_glIsBuffer = glad_debug_impl_glIsBuffer;
PFNGLISENABLEDPROC glad_glIsEnabled = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsEnabled(GLenum cap) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsEnabled", (GLADapiproc) glad_glIsEnabled, 1, cap);
    ret = glad_glIsEnabled(cap);
    _post_call_gles2_callback((void*) &ret, "glIsEnabled", (GLADapiproc) glad_glIsEnabled, 1, cap);
    return ret;
}
PFNGLISENABLEDPROC glad_debug_glIsEnabled = glad_debug_impl_glIsEnabled;
PFNGLISENABLEDIPROC glad_glIsEnabledi = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsEnabledi(GLenum target, GLuint index) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsEnabledi", (GLADapiproc) glad_glIsEnabledi, 2, target, index);
    ret = glad_glIsEnabledi(target, index);
    _post_call_gles2_callback((void*) &ret, "glIsEnabledi", (GLADapiproc) glad_glIsEnabledi, 2, target, index);
    return ret;
}
PFNGLISENABLEDIPROC glad_debug_glIsEnabledi = glad_debug_impl_glIsEnabledi;
PFNGLISENABLEDIEXTPROC glad_glIsEnablediEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsEnablediEXT(GLenum target, GLuint index) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsEnablediEXT", (GLADapiproc) glad_glIsEnablediEXT, 2, target, index);
    ret = glad_glIsEnablediEXT(target, index);
    _post_call_gles2_callback((void*) &ret, "glIsEnablediEXT", (GLADapiproc) glad_glIsEnablediEXT, 2, target, index);
    return ret;
}
PFNGLISENABLEDIEXTPROC glad_debug_glIsEnablediEXT = glad_debug_impl_glIsEnablediEXT;
PFNGLISENABLEDINVPROC glad_glIsEnablediNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsEnablediNV(GLenum target, GLuint index) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsEnablediNV", (GLADapiproc) glad_glIsEnablediNV, 2, target, index);
    ret = glad_glIsEnablediNV(target, index);
    _post_call_gles2_callback((void*) &ret, "glIsEnablediNV", (GLADapiproc) glad_glIsEnablediNV, 2, target, index);
    return ret;
}
PFNGLISENABLEDINVPROC glad_debug_glIsEnablediNV = glad_debug_impl_glIsEnablediNV;
PFNGLISENABLEDIOESPROC glad_glIsEnablediOES = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsEnablediOES(GLenum target, GLuint index) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsEnablediOES", (GLADapiproc) glad_glIsEnablediOES, 2, target, index);
    ret = glad_glIsEnablediOES(target, index);
    _post_call_gles2_callback((void*) &ret, "glIsEnablediOES", (GLADapiproc) glad_glIsEnablediOES, 2, target, index);
    return ret;
}
PFNGLISENABLEDIOESPROC glad_debug_glIsEnablediOES = glad_debug_impl_glIsEnablediOES;
PFNGLISFENCENVPROC glad_glIsFenceNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsFenceNV(GLuint fence) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsFenceNV", (GLADapiproc) glad_glIsFenceNV, 1, fence);
    ret = glad_glIsFenceNV(fence);
    _post_call_gles2_callback((void*) &ret, "glIsFenceNV", (GLADapiproc) glad_glIsFenceNV, 1, fence);
    return ret;
}
PFNGLISFENCENVPROC glad_debug_glIsFenceNV = glad_debug_impl_glIsFenceNV;
PFNGLISFRAMEBUFFERPROC glad_glIsFramebuffer = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsFramebuffer(GLuint framebuffer) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsFramebuffer", (GLADapiproc) glad_glIsFramebuffer, 1, framebuffer);
    ret = glad_glIsFramebuffer(framebuffer);
    _post_call_gles2_callback((void*) &ret, "glIsFramebuffer", (GLADapiproc) glad_glIsFramebuffer, 1, framebuffer);
    return ret;
}
PFNGLISFRAMEBUFFERPROC glad_debug_glIsFramebuffer = glad_debug_impl_glIsFramebuffer;
PFNGLISIMAGEHANDLERESIDENTNVPROC glad_glIsImageHandleResidentNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsImageHandleResidentNV(GLuint64 handle) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsImageHandleResidentNV", (GLADapiproc) glad_glIsImageHandleResidentNV, 1, handle);
    ret = glad_glIsImageHandleResidentNV(handle);
    _post_call_gles2_callback((void*) &ret, "glIsImageHandleResidentNV", (GLADapiproc) glad_glIsImageHandleResidentNV, 1, handle);
    return ret;
}
PFNGLISIMAGEHANDLERESIDENTNVPROC glad_debug_glIsImageHandleResidentNV = glad_debug_impl_glIsImageHandleResidentNV;
PFNGLISMEMORYOBJECTEXTPROC glad_glIsMemoryObjectEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsMemoryObjectEXT(GLuint memoryObject) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsMemoryObjectEXT", (GLADapiproc) glad_glIsMemoryObjectEXT, 1, memoryObject);
    ret = glad_glIsMemoryObjectEXT(memoryObject);
    _post_call_gles2_callback((void*) &ret, "glIsMemoryObjectEXT", (GLADapiproc) glad_glIsMemoryObjectEXT, 1, memoryObject);
    return ret;
}
PFNGLISMEMORYOBJECTEXTPROC glad_debug_glIsMemoryObjectEXT = glad_debug_impl_glIsMemoryObjectEXT;
PFNGLISPATHNVPROC glad_glIsPathNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsPathNV(GLuint path) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsPathNV", (GLADapiproc) glad_glIsPathNV, 1, path);
    ret = glad_glIsPathNV(path);
    _post_call_gles2_callback((void*) &ret, "glIsPathNV", (GLADapiproc) glad_glIsPathNV, 1, path);
    return ret;
}
PFNGLISPATHNVPROC glad_debug_glIsPathNV = glad_debug_impl_glIsPathNV;
PFNGLISPOINTINFILLPATHNVPROC glad_glIsPointInFillPathNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsPointInFillPathNV(GLuint path, GLuint mask, GLfloat x, GLfloat y) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsPointInFillPathNV", (GLADapiproc) glad_glIsPointInFillPathNV, 4, path, mask, x, y);
    ret = glad_glIsPointInFillPathNV(path, mask, x, y);
    _post_call_gles2_callback((void*) &ret, "glIsPointInFillPathNV", (GLADapiproc) glad_glIsPointInFillPathNV, 4, path, mask, x, y);
    return ret;
}
PFNGLISPOINTINFILLPATHNVPROC glad_debug_glIsPointInFillPathNV = glad_debug_impl_glIsPointInFillPathNV;
PFNGLISPOINTINSTROKEPATHNVPROC glad_glIsPointInStrokePathNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsPointInStrokePathNV(GLuint path, GLfloat x, GLfloat y) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsPointInStrokePathNV", (GLADapiproc) glad_glIsPointInStrokePathNV, 3, path, x, y);
    ret = glad_glIsPointInStrokePathNV(path, x, y);
    _post_call_gles2_callback((void*) &ret, "glIsPointInStrokePathNV", (GLADapiproc) glad_glIsPointInStrokePathNV, 3, path, x, y);
    return ret;
}
PFNGLISPOINTINSTROKEPATHNVPROC glad_debug_glIsPointInStrokePathNV = glad_debug_impl_glIsPointInStrokePathNV;
PFNGLISPROGRAMPROC glad_glIsProgram = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsProgram(GLuint program) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsProgram", (GLADapiproc) glad_glIsProgram, 1, program);
    ret = glad_glIsProgram(program);
    _post_call_gles2_callback((void*) &ret, "glIsProgram", (GLADapiproc) glad_glIsProgram, 1, program);
    return ret;
}
PFNGLISPROGRAMPROC glad_debug_glIsProgram = glad_debug_impl_glIsProgram;
PFNGLISPROGRAMPIPELINEPROC glad_glIsProgramPipeline = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsProgramPipeline(GLuint pipeline) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsProgramPipeline", (GLADapiproc) glad_glIsProgramPipeline, 1, pipeline);
    ret = glad_glIsProgramPipeline(pipeline);
    _post_call_gles2_callback((void*) &ret, "glIsProgramPipeline", (GLADapiproc) glad_glIsProgramPipeline, 1, pipeline);
    return ret;
}
PFNGLISPROGRAMPIPELINEPROC glad_debug_glIsProgramPipeline = glad_debug_impl_glIsProgramPipeline;
PFNGLISPROGRAMPIPELINEEXTPROC glad_glIsProgramPipelineEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsProgramPipelineEXT(GLuint pipeline) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsProgramPipelineEXT", (GLADapiproc) glad_glIsProgramPipelineEXT, 1, pipeline);
    ret = glad_glIsProgramPipelineEXT(pipeline);
    _post_call_gles2_callback((void*) &ret, "glIsProgramPipelineEXT", (GLADapiproc) glad_glIsProgramPipelineEXT, 1, pipeline);
    return ret;
}
PFNGLISPROGRAMPIPELINEEXTPROC glad_debug_glIsProgramPipelineEXT = glad_debug_impl_glIsProgramPipelineEXT;
PFNGLISQUERYPROC glad_glIsQuery = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsQuery(GLuint id) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsQuery", (GLADapiproc) glad_glIsQuery, 1, id);
    ret = glad_glIsQuery(id);
    _post_call_gles2_callback((void*) &ret, "glIsQuery", (GLADapiproc) glad_glIsQuery, 1, id);
    return ret;
}
PFNGLISQUERYPROC glad_debug_glIsQuery = glad_debug_impl_glIsQuery;
PFNGLISQUERYEXTPROC glad_glIsQueryEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsQueryEXT(GLuint id) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsQueryEXT", (GLADapiproc) glad_glIsQueryEXT, 1, id);
    ret = glad_glIsQueryEXT(id);
    _post_call_gles2_callback((void*) &ret, "glIsQueryEXT", (GLADapiproc) glad_glIsQueryEXT, 1, id);
    return ret;
}
PFNGLISQUERYEXTPROC glad_debug_glIsQueryEXT = glad_debug_impl_glIsQueryEXT;
PFNGLISRENDERBUFFERPROC glad_glIsRenderbuffer = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsRenderbuffer(GLuint renderbuffer) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsRenderbuffer", (GLADapiproc) glad_glIsRenderbuffer, 1, renderbuffer);
    ret = glad_glIsRenderbuffer(renderbuffer);
    _post_call_gles2_callback((void*) &ret, "glIsRenderbuffer", (GLADapiproc) glad_glIsRenderbuffer, 1, renderbuffer);
    return ret;
}
PFNGLISRENDERBUFFERPROC glad_debug_glIsRenderbuffer = glad_debug_impl_glIsRenderbuffer;
PFNGLISSAMPLERPROC glad_glIsSampler = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsSampler(GLuint sampler) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsSampler", (GLADapiproc) glad_glIsSampler, 1, sampler);
    ret = glad_glIsSampler(sampler);
    _post_call_gles2_callback((void*) &ret, "glIsSampler", (GLADapiproc) glad_glIsSampler, 1, sampler);
    return ret;
}
PFNGLISSAMPLERPROC glad_debug_glIsSampler = glad_debug_impl_glIsSampler;
PFNGLISSEMAPHOREEXTPROC glad_glIsSemaphoreEXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsSemaphoreEXT(GLuint semaphore) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsSemaphoreEXT", (GLADapiproc) glad_glIsSemaphoreEXT, 1, semaphore);
    ret = glad_glIsSemaphoreEXT(semaphore);
    _post_call_gles2_callback((void*) &ret, "glIsSemaphoreEXT", (GLADapiproc) glad_glIsSemaphoreEXT, 1, semaphore);
    return ret;
}
PFNGLISSEMAPHOREEXTPROC glad_debug_glIsSemaphoreEXT = glad_debug_impl_glIsSemaphoreEXT;
PFNGLISSHADERPROC glad_glIsShader = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsShader(GLuint shader) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsShader", (GLADapiproc) glad_glIsShader, 1, shader);
    ret = glad_glIsShader(shader);
    _post_call_gles2_callback((void*) &ret, "glIsShader", (GLADapiproc) glad_glIsShader, 1, shader);
    return ret;
}
PFNGLISSHADERPROC glad_debug_glIsShader = glad_debug_impl_glIsShader;
PFNGLISSYNCPROC glad_glIsSync = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsSync(GLsync sync) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsSync", (GLADapiproc) glad_glIsSync, 1, sync);
    ret = glad_glIsSync(sync);
    _post_call_gles2_callback((void*) &ret, "glIsSync", (GLADapiproc) glad_glIsSync, 1, sync);
    return ret;
}
PFNGLISSYNCPROC glad_debug_glIsSync = glad_debug_impl_glIsSync;
PFNGLISSYNCAPPLEPROC glad_glIsSyncAPPLE = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsSyncAPPLE(GLsync sync) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsSyncAPPLE", (GLADapiproc) glad_glIsSyncAPPLE, 1, sync);
    ret = glad_glIsSyncAPPLE(sync);
    _post_call_gles2_callback((void*) &ret, "glIsSyncAPPLE", (GLADapiproc) glad_glIsSyncAPPLE, 1, sync);
    return ret;
}
PFNGLISSYNCAPPLEPROC glad_debug_glIsSyncAPPLE = glad_debug_impl_glIsSyncAPPLE;
PFNGLISTEXTUREPROC glad_glIsTexture = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsTexture(GLuint texture) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsTexture", (GLADapiproc) glad_glIsTexture, 1, texture);
    ret = glad_glIsTexture(texture);
    _post_call_gles2_callback((void*) &ret, "glIsTexture", (GLADapiproc) glad_glIsTexture, 1, texture);
    return ret;
}
PFNGLISTEXTUREPROC glad_debug_glIsTexture = glad_debug_impl_glIsTexture;
PFNGLISTEXTUREHANDLERESIDENTNVPROC glad_glIsTextureHandleResidentNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsTextureHandleResidentNV(GLuint64 handle) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsTextureHandleResidentNV", (GLADapiproc) glad_glIsTextureHandleResidentNV, 1, handle);
    ret = glad_glIsTextureHandleResidentNV(handle);
    _post_call_gles2_callback((void*) &ret, "glIsTextureHandleResidentNV", (GLADapiproc) glad_glIsTextureHandleResidentNV, 1, handle);
    return ret;
}
PFNGLISTEXTUREHANDLERESIDENTNVPROC glad_debug_glIsTextureHandleResidentNV = glad_debug_impl_glIsTextureHandleResidentNV;
PFNGLISTRANSFORMFEEDBACKPROC glad_glIsTransformFeedback = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsTransformFeedback(GLuint id) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsTransformFeedback", (GLADapiproc) glad_glIsTransformFeedback, 1, id);
    ret = glad_glIsTransformFeedback(id);
    _post_call_gles2_callback((void*) &ret, "glIsTransformFeedback", (GLADapiproc) glad_glIsTransformFeedback, 1, id);
    return ret;
}
PFNGLISTRANSFORMFEEDBACKPROC glad_debug_glIsTransformFeedback = glad_debug_impl_glIsTransformFeedback;
PFNGLISVERTEXARRAYPROC glad_glIsVertexArray = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsVertexArray(GLuint array) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsVertexArray", (GLADapiproc) glad_glIsVertexArray, 1, array);
    ret = glad_glIsVertexArray(array);
    _post_call_gles2_callback((void*) &ret, "glIsVertexArray", (GLADapiproc) glad_glIsVertexArray, 1, array);
    return ret;
}
PFNGLISVERTEXARRAYPROC glad_debug_glIsVertexArray = glad_debug_impl_glIsVertexArray;
PFNGLISVERTEXARRAYOESPROC glad_glIsVertexArrayOES = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glIsVertexArrayOES(GLuint array) {
    GLboolean ret;
    _pre_call_gles2_callback("glIsVertexArrayOES", (GLADapiproc) glad_glIsVertexArrayOES, 1, array);
    ret = glad_glIsVertexArrayOES(array);
    _post_call_gles2_callback((void*) &ret, "glIsVertexArrayOES", (GLADapiproc) glad_glIsVertexArrayOES, 1, array);
    return ret;
}
PFNGLISVERTEXARRAYOESPROC glad_debug_glIsVertexArrayOES = glad_debug_impl_glIsVertexArrayOES;
PFNGLLABELOBJECTEXTPROC glad_glLabelObjectEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glLabelObjectEXT(GLenum type, GLuint object, GLsizei length, const GLchar * label) {
    _pre_call_gles2_callback("glLabelObjectEXT", (GLADapiproc) glad_glLabelObjectEXT, 4, type, object, length, label);
    glad_glLabelObjectEXT(type, object, length, label);
    _post_call_gles2_callback(NULL, "glLabelObjectEXT", (GLADapiproc) glad_glLabelObjectEXT, 4, type, object, length, label);
    
}
PFNGLLABELOBJECTEXTPROC glad_debug_glLabelObjectEXT = glad_debug_impl_glLabelObjectEXT;
PFNGLLINEWIDTHPROC glad_glLineWidth = NULL;
static void GLAD_API_PTR glad_debug_impl_glLineWidth(GLfloat width) {
    _pre_call_gles2_callback("glLineWidth", (GLADapiproc) glad_glLineWidth, 1, width);
    glad_glLineWidth(width);
    _post_call_gles2_callback(NULL, "glLineWidth", (GLADapiproc) glad_glLineWidth, 1, width);
    
}
PFNGLLINEWIDTHPROC glad_debug_glLineWidth = glad_debug_impl_glLineWidth;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
static void GLAD_API_PTR glad_debug_impl_glLinkProgram(GLuint program) {
    _pre_call_gles2_callback("glLinkProgram", (GLADapiproc) glad_glLinkProgram, 1, program);
    glad_glLinkProgram(program);
    _post_call_gles2_callback(NULL, "glLinkProgram", (GLADapiproc) glad_glLinkProgram, 1, program);
    
}
PFNGLLINKPROGRAMPROC glad_debug_glLinkProgram = glad_debug_impl_glLinkProgram;
PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC glad_glMakeImageHandleNonResidentNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMakeImageHandleNonResidentNV(GLuint64 handle) {
    _pre_call_gles2_callback("glMakeImageHandleNonResidentNV", (GLADapiproc) glad_glMakeImageHandleNonResidentNV, 1, handle);
    glad_glMakeImageHandleNonResidentNV(handle);
    _post_call_gles2_callback(NULL, "glMakeImageHandleNonResidentNV", (GLADapiproc) glad_glMakeImageHandleNonResidentNV, 1, handle);
    
}
PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC glad_debug_glMakeImageHandleNonResidentNV = glad_debug_impl_glMakeImageHandleNonResidentNV;
PFNGLMAKEIMAGEHANDLERESIDENTNVPROC glad_glMakeImageHandleResidentNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMakeImageHandleResidentNV(GLuint64 handle, GLenum access) {
    _pre_call_gles2_callback("glMakeImageHandleResidentNV", (GLADapiproc) glad_glMakeImageHandleResidentNV, 2, handle, access);
    glad_glMakeImageHandleResidentNV(handle, access);
    _post_call_gles2_callback(NULL, "glMakeImageHandleResidentNV", (GLADapiproc) glad_glMakeImageHandleResidentNV, 2, handle, access);
    
}
PFNGLMAKEIMAGEHANDLERESIDENTNVPROC glad_debug_glMakeImageHandleResidentNV = glad_debug_impl_glMakeImageHandleResidentNV;
PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC glad_glMakeTextureHandleNonResidentNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMakeTextureHandleNonResidentNV(GLuint64 handle) {
    _pre_call_gles2_callback("glMakeTextureHandleNonResidentNV", (GLADapiproc) glad_glMakeTextureHandleNonResidentNV, 1, handle);
    glad_glMakeTextureHandleNonResidentNV(handle);
    _post_call_gles2_callback(NULL, "glMakeTextureHandleNonResidentNV", (GLADapiproc) glad_glMakeTextureHandleNonResidentNV, 1, handle);
    
}
PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC glad_debug_glMakeTextureHandleNonResidentNV = glad_debug_impl_glMakeTextureHandleNonResidentNV;
PFNGLMAKETEXTUREHANDLERESIDENTNVPROC glad_glMakeTextureHandleResidentNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMakeTextureHandleResidentNV(GLuint64 handle) {
    _pre_call_gles2_callback("glMakeTextureHandleResidentNV", (GLADapiproc) glad_glMakeTextureHandleResidentNV, 1, handle);
    glad_glMakeTextureHandleResidentNV(handle);
    _post_call_gles2_callback(NULL, "glMakeTextureHandleResidentNV", (GLADapiproc) glad_glMakeTextureHandleResidentNV, 1, handle);
    
}
PFNGLMAKETEXTUREHANDLERESIDENTNVPROC glad_debug_glMakeTextureHandleResidentNV = glad_debug_impl_glMakeTextureHandleResidentNV;
PFNGLMAPBUFFEROESPROC glad_glMapBufferOES = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapBufferOES(GLenum target, GLenum access) {
    void * ret;
    _pre_call_gles2_callback("glMapBufferOES", (GLADapiproc) glad_glMapBufferOES, 2, target, access);
    ret = glad_glMapBufferOES(target, access);
    _post_call_gles2_callback((void*) &ret, "glMapBufferOES", (GLADapiproc) glad_glMapBufferOES, 2, target, access);
    return ret;
}
PFNGLMAPBUFFEROESPROC glad_debug_glMapBufferOES = glad_debug_impl_glMapBufferOES;
PFNGLMAPBUFFERRANGEPROC glad_glMapBufferRange = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    void * ret;
    _pre_call_gles2_callback("glMapBufferRange", (GLADapiproc) glad_glMapBufferRange, 4, target, offset, length, access);
    ret = glad_glMapBufferRange(target, offset, length, access);
    _post_call_gles2_callback((void*) &ret, "glMapBufferRange", (GLADapiproc) glad_glMapBufferRange, 4, target, offset, length, access);
    return ret;
}
PFNGLMAPBUFFERRANGEPROC glad_debug_glMapBufferRange = glad_debug_impl_glMapBufferRange;
PFNGLMAPBUFFERRANGEEXTPROC glad_glMapBufferRangeEXT = NULL;
static void * GLAD_API_PTR glad_debug_impl_glMapBufferRangeEXT(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    void * ret;
    _pre_call_gles2_callback("glMapBufferRangeEXT", (GLADapiproc) glad_glMapBufferRangeEXT, 4, target, offset, length, access);
    ret = glad_glMapBufferRangeEXT(target, offset, length, access);
    _post_call_gles2_callback((void*) &ret, "glMapBufferRangeEXT", (GLADapiproc) glad_glMapBufferRangeEXT, 4, target, offset, length, access);
    return ret;
}
PFNGLMAPBUFFERRANGEEXTPROC glad_debug_glMapBufferRangeEXT = glad_debug_impl_glMapBufferRangeEXT;
PFNGLMATRIXFRUSTUMEXTPROC glad_glMatrixFrustumEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixFrustumEXT(GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    _pre_call_gles2_callback("glMatrixFrustumEXT", (GLADapiproc) glad_glMatrixFrustumEXT, 7, mode, left, right, bottom, top, zNear, zFar);
    glad_glMatrixFrustumEXT(mode, left, right, bottom, top, zNear, zFar);
    _post_call_gles2_callback(NULL, "glMatrixFrustumEXT", (GLADapiproc) glad_glMatrixFrustumEXT, 7, mode, left, right, bottom, top, zNear, zFar);
    
}
PFNGLMATRIXFRUSTUMEXTPROC glad_debug_glMatrixFrustumEXT = glad_debug_impl_glMatrixFrustumEXT;
PFNGLMATRIXLOAD3X2FNVPROC glad_glMatrixLoad3x2fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoad3x2fNV(GLenum matrixMode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixLoad3x2fNV", (GLADapiproc) glad_glMatrixLoad3x2fNV, 2, matrixMode, m);
    glad_glMatrixLoad3x2fNV(matrixMode, m);
    _post_call_gles2_callback(NULL, "glMatrixLoad3x2fNV", (GLADapiproc) glad_glMatrixLoad3x2fNV, 2, matrixMode, m);
    
}
PFNGLMATRIXLOAD3X2FNVPROC glad_debug_glMatrixLoad3x2fNV = glad_debug_impl_glMatrixLoad3x2fNV;
PFNGLMATRIXLOAD3X3FNVPROC glad_glMatrixLoad3x3fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoad3x3fNV(GLenum matrixMode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixLoad3x3fNV", (GLADapiproc) glad_glMatrixLoad3x3fNV, 2, matrixMode, m);
    glad_glMatrixLoad3x3fNV(matrixMode, m);
    _post_call_gles2_callback(NULL, "glMatrixLoad3x3fNV", (GLADapiproc) glad_glMatrixLoad3x3fNV, 2, matrixMode, m);
    
}
PFNGLMATRIXLOAD3X3FNVPROC glad_debug_glMatrixLoad3x3fNV = glad_debug_impl_glMatrixLoad3x3fNV;
PFNGLMATRIXLOADIDENTITYEXTPROC glad_glMatrixLoadIdentityEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoadIdentityEXT(GLenum mode) {
    _pre_call_gles2_callback("glMatrixLoadIdentityEXT", (GLADapiproc) glad_glMatrixLoadIdentityEXT, 1, mode);
    glad_glMatrixLoadIdentityEXT(mode);
    _post_call_gles2_callback(NULL, "glMatrixLoadIdentityEXT", (GLADapiproc) glad_glMatrixLoadIdentityEXT, 1, mode);
    
}
PFNGLMATRIXLOADIDENTITYEXTPROC glad_debug_glMatrixLoadIdentityEXT = glad_debug_impl_glMatrixLoadIdentityEXT;
PFNGLMATRIXLOADTRANSPOSE3X3FNVPROC glad_glMatrixLoadTranspose3x3fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoadTranspose3x3fNV(GLenum matrixMode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixLoadTranspose3x3fNV", (GLADapiproc) glad_glMatrixLoadTranspose3x3fNV, 2, matrixMode, m);
    glad_glMatrixLoadTranspose3x3fNV(matrixMode, m);
    _post_call_gles2_callback(NULL, "glMatrixLoadTranspose3x3fNV", (GLADapiproc) glad_glMatrixLoadTranspose3x3fNV, 2, matrixMode, m);
    
}
PFNGLMATRIXLOADTRANSPOSE3X3FNVPROC glad_debug_glMatrixLoadTranspose3x3fNV = glad_debug_impl_glMatrixLoadTranspose3x3fNV;
PFNGLMATRIXLOADTRANSPOSEDEXTPROC glad_glMatrixLoadTransposedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoadTransposedEXT(GLenum mode, const GLdouble * m) {
    _pre_call_gles2_callback("glMatrixLoadTransposedEXT", (GLADapiproc) glad_glMatrixLoadTransposedEXT, 2, mode, m);
    glad_glMatrixLoadTransposedEXT(mode, m);
    _post_call_gles2_callback(NULL, "glMatrixLoadTransposedEXT", (GLADapiproc) glad_glMatrixLoadTransposedEXT, 2, mode, m);
    
}
PFNGLMATRIXLOADTRANSPOSEDEXTPROC glad_debug_glMatrixLoadTransposedEXT = glad_debug_impl_glMatrixLoadTransposedEXT;
PFNGLMATRIXLOADTRANSPOSEFEXTPROC glad_glMatrixLoadTransposefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoadTransposefEXT(GLenum mode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixLoadTransposefEXT", (GLADapiproc) glad_glMatrixLoadTransposefEXT, 2, mode, m);
    glad_glMatrixLoadTransposefEXT(mode, m);
    _post_call_gles2_callback(NULL, "glMatrixLoadTransposefEXT", (GLADapiproc) glad_glMatrixLoadTransposefEXT, 2, mode, m);
    
}
PFNGLMATRIXLOADTRANSPOSEFEXTPROC glad_debug_glMatrixLoadTransposefEXT = glad_debug_impl_glMatrixLoadTransposefEXT;
PFNGLMATRIXLOADDEXTPROC glad_glMatrixLoaddEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoaddEXT(GLenum mode, const GLdouble * m) {
    _pre_call_gles2_callback("glMatrixLoaddEXT", (GLADapiproc) glad_glMatrixLoaddEXT, 2, mode, m);
    glad_glMatrixLoaddEXT(mode, m);
    _post_call_gles2_callback(NULL, "glMatrixLoaddEXT", (GLADapiproc) glad_glMatrixLoaddEXT, 2, mode, m);
    
}
PFNGLMATRIXLOADDEXTPROC glad_debug_glMatrixLoaddEXT = glad_debug_impl_glMatrixLoaddEXT;
PFNGLMATRIXLOADFEXTPROC glad_glMatrixLoadfEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixLoadfEXT(GLenum mode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixLoadfEXT", (GLADapiproc) glad_glMatrixLoadfEXT, 2, mode, m);
    glad_glMatrixLoadfEXT(mode, m);
    _post_call_gles2_callback(NULL, "glMatrixLoadfEXT", (GLADapiproc) glad_glMatrixLoadfEXT, 2, mode, m);
    
}
PFNGLMATRIXLOADFEXTPROC glad_debug_glMatrixLoadfEXT = glad_debug_impl_glMatrixLoadfEXT;
PFNGLMATRIXMULT3X2FNVPROC glad_glMatrixMult3x2fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMult3x2fNV(GLenum matrixMode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixMult3x2fNV", (GLADapiproc) glad_glMatrixMult3x2fNV, 2, matrixMode, m);
    glad_glMatrixMult3x2fNV(matrixMode, m);
    _post_call_gles2_callback(NULL, "glMatrixMult3x2fNV", (GLADapiproc) glad_glMatrixMult3x2fNV, 2, matrixMode, m);
    
}
PFNGLMATRIXMULT3X2FNVPROC glad_debug_glMatrixMult3x2fNV = glad_debug_impl_glMatrixMult3x2fNV;
PFNGLMATRIXMULT3X3FNVPROC glad_glMatrixMult3x3fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMult3x3fNV(GLenum matrixMode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixMult3x3fNV", (GLADapiproc) glad_glMatrixMult3x3fNV, 2, matrixMode, m);
    glad_glMatrixMult3x3fNV(matrixMode, m);
    _post_call_gles2_callback(NULL, "glMatrixMult3x3fNV", (GLADapiproc) glad_glMatrixMult3x3fNV, 2, matrixMode, m);
    
}
PFNGLMATRIXMULT3X3FNVPROC glad_debug_glMatrixMult3x3fNV = glad_debug_impl_glMatrixMult3x3fNV;
PFNGLMATRIXMULTTRANSPOSE3X3FNVPROC glad_glMatrixMultTranspose3x3fNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMultTranspose3x3fNV(GLenum matrixMode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixMultTranspose3x3fNV", (GLADapiproc) glad_glMatrixMultTranspose3x3fNV, 2, matrixMode, m);
    glad_glMatrixMultTranspose3x3fNV(matrixMode, m);
    _post_call_gles2_callback(NULL, "glMatrixMultTranspose3x3fNV", (GLADapiproc) glad_glMatrixMultTranspose3x3fNV, 2, matrixMode, m);
    
}
PFNGLMATRIXMULTTRANSPOSE3X3FNVPROC glad_debug_glMatrixMultTranspose3x3fNV = glad_debug_impl_glMatrixMultTranspose3x3fNV;
PFNGLMATRIXMULTTRANSPOSEDEXTPROC glad_glMatrixMultTransposedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMultTransposedEXT(GLenum mode, const GLdouble * m) {
    _pre_call_gles2_callback("glMatrixMultTransposedEXT", (GLADapiproc) glad_glMatrixMultTransposedEXT, 2, mode, m);
    glad_glMatrixMultTransposedEXT(mode, m);
    _post_call_gles2_callback(NULL, "glMatrixMultTransposedEXT", (GLADapiproc) glad_glMatrixMultTransposedEXT, 2, mode, m);
    
}
PFNGLMATRIXMULTTRANSPOSEDEXTPROC glad_debug_glMatrixMultTransposedEXT = glad_debug_impl_glMatrixMultTransposedEXT;
PFNGLMATRIXMULTTRANSPOSEFEXTPROC glad_glMatrixMultTransposefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMultTransposefEXT(GLenum mode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixMultTransposefEXT", (GLADapiproc) glad_glMatrixMultTransposefEXT, 2, mode, m);
    glad_glMatrixMultTransposefEXT(mode, m);
    _post_call_gles2_callback(NULL, "glMatrixMultTransposefEXT", (GLADapiproc) glad_glMatrixMultTransposefEXT, 2, mode, m);
    
}
PFNGLMATRIXMULTTRANSPOSEFEXTPROC glad_debug_glMatrixMultTransposefEXT = glad_debug_impl_glMatrixMultTransposefEXT;
PFNGLMATRIXMULTDEXTPROC glad_glMatrixMultdEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMultdEXT(GLenum mode, const GLdouble * m) {
    _pre_call_gles2_callback("glMatrixMultdEXT", (GLADapiproc) glad_glMatrixMultdEXT, 2, mode, m);
    glad_glMatrixMultdEXT(mode, m);
    _post_call_gles2_callback(NULL, "glMatrixMultdEXT", (GLADapiproc) glad_glMatrixMultdEXT, 2, mode, m);
    
}
PFNGLMATRIXMULTDEXTPROC glad_debug_glMatrixMultdEXT = glad_debug_impl_glMatrixMultdEXT;
PFNGLMATRIXMULTFEXTPROC glad_glMatrixMultfEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixMultfEXT(GLenum mode, const GLfloat * m) {
    _pre_call_gles2_callback("glMatrixMultfEXT", (GLADapiproc) glad_glMatrixMultfEXT, 2, mode, m);
    glad_glMatrixMultfEXT(mode, m);
    _post_call_gles2_callback(NULL, "glMatrixMultfEXT", (GLADapiproc) glad_glMatrixMultfEXT, 2, mode, m);
    
}
PFNGLMATRIXMULTFEXTPROC glad_debug_glMatrixMultfEXT = glad_debug_impl_glMatrixMultfEXT;
PFNGLMATRIXORTHOEXTPROC glad_glMatrixOrthoEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixOrthoEXT(GLenum mode, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) {
    _pre_call_gles2_callback("glMatrixOrthoEXT", (GLADapiproc) glad_glMatrixOrthoEXT, 7, mode, left, right, bottom, top, zNear, zFar);
    glad_glMatrixOrthoEXT(mode, left, right, bottom, top, zNear, zFar);
    _post_call_gles2_callback(NULL, "glMatrixOrthoEXT", (GLADapiproc) glad_glMatrixOrthoEXT, 7, mode, left, right, bottom, top, zNear, zFar);
    
}
PFNGLMATRIXORTHOEXTPROC glad_debug_glMatrixOrthoEXT = glad_debug_impl_glMatrixOrthoEXT;
PFNGLMATRIXPOPEXTPROC glad_glMatrixPopEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixPopEXT(GLenum mode) {
    _pre_call_gles2_callback("glMatrixPopEXT", (GLADapiproc) glad_glMatrixPopEXT, 1, mode);
    glad_glMatrixPopEXT(mode);
    _post_call_gles2_callback(NULL, "glMatrixPopEXT", (GLADapiproc) glad_glMatrixPopEXT, 1, mode);
    
}
PFNGLMATRIXPOPEXTPROC glad_debug_glMatrixPopEXT = glad_debug_impl_glMatrixPopEXT;
PFNGLMATRIXPUSHEXTPROC glad_glMatrixPushEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixPushEXT(GLenum mode) {
    _pre_call_gles2_callback("glMatrixPushEXT", (GLADapiproc) glad_glMatrixPushEXT, 1, mode);
    glad_glMatrixPushEXT(mode);
    _post_call_gles2_callback(NULL, "glMatrixPushEXT", (GLADapiproc) glad_glMatrixPushEXT, 1, mode);
    
}
PFNGLMATRIXPUSHEXTPROC glad_debug_glMatrixPushEXT = glad_debug_impl_glMatrixPushEXT;
PFNGLMATRIXROTATEDEXTPROC glad_glMatrixRotatedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixRotatedEXT(GLenum mode, GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gles2_callback("glMatrixRotatedEXT", (GLADapiproc) glad_glMatrixRotatedEXT, 5, mode, angle, x, y, z);
    glad_glMatrixRotatedEXT(mode, angle, x, y, z);
    _post_call_gles2_callback(NULL, "glMatrixRotatedEXT", (GLADapiproc) glad_glMatrixRotatedEXT, 5, mode, angle, x, y, z);
    
}
PFNGLMATRIXROTATEDEXTPROC glad_debug_glMatrixRotatedEXT = glad_debug_impl_glMatrixRotatedEXT;
PFNGLMATRIXROTATEFEXTPROC glad_glMatrixRotatefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixRotatefEXT(GLenum mode, GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gles2_callback("glMatrixRotatefEXT", (GLADapiproc) glad_glMatrixRotatefEXT, 5, mode, angle, x, y, z);
    glad_glMatrixRotatefEXT(mode, angle, x, y, z);
    _post_call_gles2_callback(NULL, "glMatrixRotatefEXT", (GLADapiproc) glad_glMatrixRotatefEXT, 5, mode, angle, x, y, z);
    
}
PFNGLMATRIXROTATEFEXTPROC glad_debug_glMatrixRotatefEXT = glad_debug_impl_glMatrixRotatefEXT;
PFNGLMATRIXSCALEDEXTPROC glad_glMatrixScaledEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixScaledEXT(GLenum mode, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gles2_callback("glMatrixScaledEXT", (GLADapiproc) glad_glMatrixScaledEXT, 4, mode, x, y, z);
    glad_glMatrixScaledEXT(mode, x, y, z);
    _post_call_gles2_callback(NULL, "glMatrixScaledEXT", (GLADapiproc) glad_glMatrixScaledEXT, 4, mode, x, y, z);
    
}
PFNGLMATRIXSCALEDEXTPROC glad_debug_glMatrixScaledEXT = glad_debug_impl_glMatrixScaledEXT;
PFNGLMATRIXSCALEFEXTPROC glad_glMatrixScalefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixScalefEXT(GLenum mode, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gles2_callback("glMatrixScalefEXT", (GLADapiproc) glad_glMatrixScalefEXT, 4, mode, x, y, z);
    glad_glMatrixScalefEXT(mode, x, y, z);
    _post_call_gles2_callback(NULL, "glMatrixScalefEXT", (GLADapiproc) glad_glMatrixScalefEXT, 4, mode, x, y, z);
    
}
PFNGLMATRIXSCALEFEXTPROC glad_debug_glMatrixScalefEXT = glad_debug_impl_glMatrixScalefEXT;
PFNGLMATRIXTRANSLATEDEXTPROC glad_glMatrixTranslatedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixTranslatedEXT(GLenum mode, GLdouble x, GLdouble y, GLdouble z) {
    _pre_call_gles2_callback("glMatrixTranslatedEXT", (GLADapiproc) glad_glMatrixTranslatedEXT, 4, mode, x, y, z);
    glad_glMatrixTranslatedEXT(mode, x, y, z);
    _post_call_gles2_callback(NULL, "glMatrixTranslatedEXT", (GLADapiproc) glad_glMatrixTranslatedEXT, 4, mode, x, y, z);
    
}
PFNGLMATRIXTRANSLATEDEXTPROC glad_debug_glMatrixTranslatedEXT = glad_debug_impl_glMatrixTranslatedEXT;
PFNGLMATRIXTRANSLATEFEXTPROC glad_glMatrixTranslatefEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMatrixTranslatefEXT(GLenum mode, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gles2_callback("glMatrixTranslatefEXT", (GLADapiproc) glad_glMatrixTranslatefEXT, 4, mode, x, y, z);
    glad_glMatrixTranslatefEXT(mode, x, y, z);
    _post_call_gles2_callback(NULL, "glMatrixTranslatefEXT", (GLADapiproc) glad_glMatrixTranslatefEXT, 4, mode, x, y, z);
    
}
PFNGLMATRIXTRANSLATEFEXTPROC glad_debug_glMatrixTranslatefEXT = glad_debug_impl_glMatrixTranslatefEXT;
PFNGLMAXACTIVESHADERCORESARMPROC glad_glMaxActiveShaderCoresARM = NULL;
static void GLAD_API_PTR glad_debug_impl_glMaxActiveShaderCoresARM(GLuint count) {
    _pre_call_gles2_callback("glMaxActiveShaderCoresARM", (GLADapiproc) glad_glMaxActiveShaderCoresARM, 1, count);
    glad_glMaxActiveShaderCoresARM(count);
    _post_call_gles2_callback(NULL, "glMaxActiveShaderCoresARM", (GLADapiproc) glad_glMaxActiveShaderCoresARM, 1, count);
    
}
PFNGLMAXACTIVESHADERCORESARMPROC glad_debug_glMaxActiveShaderCoresARM = glad_debug_impl_glMaxActiveShaderCoresARM;
PFNGLMAXSHADERCOMPILERTHREADSKHRPROC glad_glMaxShaderCompilerThreadsKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glMaxShaderCompilerThreadsKHR(GLuint count) {
    _pre_call_gles2_callback("glMaxShaderCompilerThreadsKHR", (GLADapiproc) glad_glMaxShaderCompilerThreadsKHR, 1, count);
    glad_glMaxShaderCompilerThreadsKHR(count);
    _post_call_gles2_callback(NULL, "glMaxShaderCompilerThreadsKHR", (GLADapiproc) glad_glMaxShaderCompilerThreadsKHR, 1, count);
    
}
PFNGLMAXSHADERCOMPILERTHREADSKHRPROC glad_debug_glMaxShaderCompilerThreadsKHR = glad_debug_impl_glMaxShaderCompilerThreadsKHR;
PFNGLMEMORYBARRIERPROC glad_glMemoryBarrier = NULL;
static void GLAD_API_PTR glad_debug_impl_glMemoryBarrier(GLbitfield barriers) {
    _pre_call_gles2_callback("glMemoryBarrier", (GLADapiproc) glad_glMemoryBarrier, 1, barriers);
    glad_glMemoryBarrier(barriers);
    _post_call_gles2_callback(NULL, "glMemoryBarrier", (GLADapiproc) glad_glMemoryBarrier, 1, barriers);
    
}
PFNGLMEMORYBARRIERPROC glad_debug_glMemoryBarrier = glad_debug_impl_glMemoryBarrier;
PFNGLMEMORYBARRIERBYREGIONPROC glad_glMemoryBarrierByRegion = NULL;
static void GLAD_API_PTR glad_debug_impl_glMemoryBarrierByRegion(GLbitfield barriers) {
    _pre_call_gles2_callback("glMemoryBarrierByRegion", (GLADapiproc) glad_glMemoryBarrierByRegion, 1, barriers);
    glad_glMemoryBarrierByRegion(barriers);
    _post_call_gles2_callback(NULL, "glMemoryBarrierByRegion", (GLADapiproc) glad_glMemoryBarrierByRegion, 1, barriers);
    
}
PFNGLMEMORYBARRIERBYREGIONPROC glad_debug_glMemoryBarrierByRegion = glad_debug_impl_glMemoryBarrierByRegion;
PFNGLMEMORYOBJECTPARAMETERIVEXTPROC glad_glMemoryObjectParameterivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMemoryObjectParameterivEXT(GLuint memoryObject, GLenum pname, const GLint * params) {
    _pre_call_gles2_callback("glMemoryObjectParameterivEXT", (GLADapiproc) glad_glMemoryObjectParameterivEXT, 3, memoryObject, pname, params);
    glad_glMemoryObjectParameterivEXT(memoryObject, pname, params);
    _post_call_gles2_callback(NULL, "glMemoryObjectParameterivEXT", (GLADapiproc) glad_glMemoryObjectParameterivEXT, 3, memoryObject, pname, params);
    
}
PFNGLMEMORYOBJECTPARAMETERIVEXTPROC glad_debug_glMemoryObjectParameterivEXT = glad_debug_impl_glMemoryObjectParameterivEXT;
PFNGLMINSAMPLESHADINGPROC glad_glMinSampleShading = NULL;
static void GLAD_API_PTR glad_debug_impl_glMinSampleShading(GLfloat value) {
    _pre_call_gles2_callback("glMinSampleShading", (GLADapiproc) glad_glMinSampleShading, 1, value);
    glad_glMinSampleShading(value);
    _post_call_gles2_callback(NULL, "glMinSampleShading", (GLADapiproc) glad_glMinSampleShading, 1, value);
    
}
PFNGLMINSAMPLESHADINGPROC glad_debug_glMinSampleShading = glad_debug_impl_glMinSampleShading;
PFNGLMINSAMPLESHADINGOESPROC glad_glMinSampleShadingOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glMinSampleShadingOES(GLfloat value) {
    _pre_call_gles2_callback("glMinSampleShadingOES", (GLADapiproc) glad_glMinSampleShadingOES, 1, value);
    glad_glMinSampleShadingOES(value);
    _post_call_gles2_callback(NULL, "glMinSampleShadingOES", (GLADapiproc) glad_glMinSampleShadingOES, 1, value);
    
}
PFNGLMINSAMPLESHADINGOESPROC glad_debug_glMinSampleShadingOES = glad_debug_impl_glMinSampleShadingOES;
PFNGLMULTIDRAWARRAYSEXTPROC glad_glMultiDrawArraysEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawArraysEXT(GLenum mode, const GLint * first, const GLsizei * count, GLsizei primcount) {
    _pre_call_gles2_callback("glMultiDrawArraysEXT", (GLADapiproc) glad_glMultiDrawArraysEXT, 4, mode, first, count, primcount);
    glad_glMultiDrawArraysEXT(mode, first, count, primcount);
    _post_call_gles2_callback(NULL, "glMultiDrawArraysEXT", (GLADapiproc) glad_glMultiDrawArraysEXT, 4, mode, first, count, primcount);
    
}
PFNGLMULTIDRAWARRAYSEXTPROC glad_debug_glMultiDrawArraysEXT = glad_debug_impl_glMultiDrawArraysEXT;
PFNGLMULTIDRAWARRAYSINDIRECTEXTPROC glad_glMultiDrawArraysIndirectEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawArraysIndirectEXT(GLenum mode, const void * indirect, GLsizei drawcount, GLsizei stride) {
    _pre_call_gles2_callback("glMultiDrawArraysIndirectEXT", (GLADapiproc) glad_glMultiDrawArraysIndirectEXT, 4, mode, indirect, drawcount, stride);
    glad_glMultiDrawArraysIndirectEXT(mode, indirect, drawcount, stride);
    _post_call_gles2_callback(NULL, "glMultiDrawArraysIndirectEXT", (GLADapiproc) glad_glMultiDrawArraysIndirectEXT, 4, mode, indirect, drawcount, stride);
    
}
PFNGLMULTIDRAWARRAYSINDIRECTEXTPROC glad_debug_glMultiDrawArraysIndirectEXT = glad_debug_impl_glMultiDrawArraysIndirectEXT;
PFNGLMULTIDRAWELEMENTSBASEVERTEXEXTPROC glad_glMultiDrawElementsBaseVertexEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawElementsBaseVertexEXT(GLenum mode, const GLsizei * count, GLenum type, const void *const* indices, GLsizei drawcount, const GLint * basevertex) {
    _pre_call_gles2_callback("glMultiDrawElementsBaseVertexEXT", (GLADapiproc) glad_glMultiDrawElementsBaseVertexEXT, 6, mode, count, type, indices, drawcount, basevertex);
    glad_glMultiDrawElementsBaseVertexEXT(mode, count, type, indices, drawcount, basevertex);
    _post_call_gles2_callback(NULL, "glMultiDrawElementsBaseVertexEXT", (GLADapiproc) glad_glMultiDrawElementsBaseVertexEXT, 6, mode, count, type, indices, drawcount, basevertex);
    
}
PFNGLMULTIDRAWELEMENTSBASEVERTEXEXTPROC glad_debug_glMultiDrawElementsBaseVertexEXT = glad_debug_impl_glMultiDrawElementsBaseVertexEXT;
PFNGLMULTIDRAWELEMENTSEXTPROC glad_glMultiDrawElementsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawElementsEXT(GLenum mode, const GLsizei * count, GLenum type, const void *const* indices, GLsizei primcount) {
    _pre_call_gles2_callback("glMultiDrawElementsEXT", (GLADapiproc) glad_glMultiDrawElementsEXT, 5, mode, count, type, indices, primcount);
    glad_glMultiDrawElementsEXT(mode, count, type, indices, primcount);
    _post_call_gles2_callback(NULL, "glMultiDrawElementsEXT", (GLADapiproc) glad_glMultiDrawElementsEXT, 5, mode, count, type, indices, primcount);
    
}
PFNGLMULTIDRAWELEMENTSEXTPROC glad_debug_glMultiDrawElementsEXT = glad_debug_impl_glMultiDrawElementsEXT;
PFNGLMULTIDRAWELEMENTSINDIRECTEXTPROC glad_glMultiDrawElementsIndirectEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawElementsIndirectEXT(GLenum mode, GLenum type, const void * indirect, GLsizei drawcount, GLsizei stride) {
    _pre_call_gles2_callback("glMultiDrawElementsIndirectEXT", (GLADapiproc) glad_glMultiDrawElementsIndirectEXT, 5, mode, type, indirect, drawcount, stride);
    glad_glMultiDrawElementsIndirectEXT(mode, type, indirect, drawcount, stride);
    _post_call_gles2_callback(NULL, "glMultiDrawElementsIndirectEXT", (GLADapiproc) glad_glMultiDrawElementsIndirectEXT, 5, mode, type, indirect, drawcount, stride);
    
}
PFNGLMULTIDRAWELEMENTSINDIRECTEXTPROC glad_debug_glMultiDrawElementsIndirectEXT = glad_debug_impl_glMultiDrawElementsIndirectEXT;
PFNGLMULTIDRAWMESHTASKSINDIRECTCOUNTEXTPROC glad_glMultiDrawMeshTasksIndirectCountEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawMeshTasksIndirectCountEXT(GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride) {
    _pre_call_gles2_callback("glMultiDrawMeshTasksIndirectCountEXT", (GLADapiproc) glad_glMultiDrawMeshTasksIndirectCountEXT, 4, indirect, drawcount, maxdrawcount, stride);
    glad_glMultiDrawMeshTasksIndirectCountEXT(indirect, drawcount, maxdrawcount, stride);
    _post_call_gles2_callback(NULL, "glMultiDrawMeshTasksIndirectCountEXT", (GLADapiproc) glad_glMultiDrawMeshTasksIndirectCountEXT, 4, indirect, drawcount, maxdrawcount, stride);
    
}
PFNGLMULTIDRAWMESHTASKSINDIRECTCOUNTEXTPROC glad_debug_glMultiDrawMeshTasksIndirectCountEXT = glad_debug_impl_glMultiDrawMeshTasksIndirectCountEXT;
PFNGLMULTIDRAWMESHTASKSINDIRECTCOUNTNVPROC glad_glMultiDrawMeshTasksIndirectCountNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawMeshTasksIndirectCountNV(GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride) {
    _pre_call_gles2_callback("glMultiDrawMeshTasksIndirectCountNV", (GLADapiproc) glad_glMultiDrawMeshTasksIndirectCountNV, 4, indirect, drawcount, maxdrawcount, stride);
    glad_glMultiDrawMeshTasksIndirectCountNV(indirect, drawcount, maxdrawcount, stride);
    _post_call_gles2_callback(NULL, "glMultiDrawMeshTasksIndirectCountNV", (GLADapiproc) glad_glMultiDrawMeshTasksIndirectCountNV, 4, indirect, drawcount, maxdrawcount, stride);
    
}
PFNGLMULTIDRAWMESHTASKSINDIRECTCOUNTNVPROC glad_debug_glMultiDrawMeshTasksIndirectCountNV = glad_debug_impl_glMultiDrawMeshTasksIndirectCountNV;
PFNGLMULTIDRAWMESHTASKSINDIRECTEXTPROC glad_glMultiDrawMeshTasksIndirectEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawMeshTasksIndirectEXT(GLintptr indirect, GLsizei drawcount, GLsizei stride) {
    _pre_call_gles2_callback("glMultiDrawMeshTasksIndirectEXT", (GLADapiproc) glad_glMultiDrawMeshTasksIndirectEXT, 3, indirect, drawcount, stride);
    glad_glMultiDrawMeshTasksIndirectEXT(indirect, drawcount, stride);
    _post_call_gles2_callback(NULL, "glMultiDrawMeshTasksIndirectEXT", (GLADapiproc) glad_glMultiDrawMeshTasksIndirectEXT, 3, indirect, drawcount, stride);
    
}
PFNGLMULTIDRAWMESHTASKSINDIRECTEXTPROC glad_debug_glMultiDrawMeshTasksIndirectEXT = glad_debug_impl_glMultiDrawMeshTasksIndirectEXT;
PFNGLMULTIDRAWMESHTASKSINDIRECTNVPROC glad_glMultiDrawMeshTasksIndirectNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glMultiDrawMeshTasksIndirectNV(GLintptr indirect, GLsizei drawcount, GLsizei stride) {
    _pre_call_gles2_callback("glMultiDrawMeshTasksIndirectNV", (GLADapiproc) glad_glMultiDrawMeshTasksIndirectNV, 3, indirect, drawcount, stride);
    glad_glMultiDrawMeshTasksIndirectNV(indirect, drawcount, stride);
    _post_call_gles2_callback(NULL, "glMultiDrawMeshTasksIndirectNV", (GLADapiproc) glad_glMultiDrawMeshTasksIndirectNV, 3, indirect, drawcount, stride);
    
}
PFNGLMULTIDRAWMESHTASKSINDIRECTNVPROC glad_debug_glMultiDrawMeshTasksIndirectNV = glad_debug_impl_glMultiDrawMeshTasksIndirectNV;
PFNGLNAMEDBUFFERATTACHMEMORYNVPROC glad_glNamedBufferAttachMemoryNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedBufferAttachMemoryNV(GLuint buffer, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glNamedBufferAttachMemoryNV", (GLADapiproc) glad_glNamedBufferAttachMemoryNV, 3, buffer, memory, offset);
    glad_glNamedBufferAttachMemoryNV(buffer, memory, offset);
    _post_call_gles2_callback(NULL, "glNamedBufferAttachMemoryNV", (GLADapiproc) glad_glNamedBufferAttachMemoryNV, 3, buffer, memory, offset);
    
}
PFNGLNAMEDBUFFERATTACHMEMORYNVPROC glad_debug_glNamedBufferAttachMemoryNV = glad_debug_impl_glNamedBufferAttachMemoryNV;
PFNGLNAMEDBUFFERPAGECOMMITMENTMEMNVPROC glad_glNamedBufferPageCommitmentMemNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedBufferPageCommitmentMemNV(GLuint buffer, GLintptr offset, GLsizeiptr size, GLuint memory, GLuint64 memOffset, GLboolean commit) {
    _pre_call_gles2_callback("glNamedBufferPageCommitmentMemNV", (GLADapiproc) glad_glNamedBufferPageCommitmentMemNV, 6, buffer, offset, size, memory, memOffset, commit);
    glad_glNamedBufferPageCommitmentMemNV(buffer, offset, size, memory, memOffset, commit);
    _post_call_gles2_callback(NULL, "glNamedBufferPageCommitmentMemNV", (GLADapiproc) glad_glNamedBufferPageCommitmentMemNV, 6, buffer, offset, size, memory, memOffset, commit);
    
}
PFNGLNAMEDBUFFERPAGECOMMITMENTMEMNVPROC glad_debug_glNamedBufferPageCommitmentMemNV = glad_debug_impl_glNamedBufferPageCommitmentMemNV;
PFNGLNAMEDBUFFERSTORAGEEXTERNALEXTPROC glad_glNamedBufferStorageExternalEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedBufferStorageExternalEXT(GLuint buffer, GLintptr offset, GLsizeiptr size, GLeglClientBufferEXT clientBuffer, GLbitfield flags) {
    _pre_call_gles2_callback("glNamedBufferStorageExternalEXT", (GLADapiproc) glad_glNamedBufferStorageExternalEXT, 5, buffer, offset, size, clientBuffer, flags);
    glad_glNamedBufferStorageExternalEXT(buffer, offset, size, clientBuffer, flags);
    _post_call_gles2_callback(NULL, "glNamedBufferStorageExternalEXT", (GLADapiproc) glad_glNamedBufferStorageExternalEXT, 5, buffer, offset, size, clientBuffer, flags);
    
}
PFNGLNAMEDBUFFERSTORAGEEXTERNALEXTPROC glad_debug_glNamedBufferStorageExternalEXT = glad_debug_impl_glNamedBufferStorageExternalEXT;
PFNGLNAMEDBUFFERSTORAGEMEMEXTPROC glad_glNamedBufferStorageMemEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedBufferStorageMemEXT(GLuint buffer, GLsizeiptr size, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glNamedBufferStorageMemEXT", (GLADapiproc) glad_glNamedBufferStorageMemEXT, 4, buffer, size, memory, offset);
    glad_glNamedBufferStorageMemEXT(buffer, size, memory, offset);
    _post_call_gles2_callback(NULL, "glNamedBufferStorageMemEXT", (GLADapiproc) glad_glNamedBufferStorageMemEXT, 4, buffer, size, memory, offset);
    
}
PFNGLNAMEDBUFFERSTORAGEMEMEXTPROC glad_debug_glNamedBufferStorageMemEXT = glad_debug_impl_glNamedBufferStorageMemEXT;
PFNGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC glad_glNamedFramebufferSampleLocationsfvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferSampleLocationsfvNV(GLuint framebuffer, GLuint start, GLsizei count, const GLfloat * v) {
    _pre_call_gles2_callback("glNamedFramebufferSampleLocationsfvNV", (GLADapiproc) glad_glNamedFramebufferSampleLocationsfvNV, 4, framebuffer, start, count, v);
    glad_glNamedFramebufferSampleLocationsfvNV(framebuffer, start, count, v);
    _post_call_gles2_callback(NULL, "glNamedFramebufferSampleLocationsfvNV", (GLADapiproc) glad_glNamedFramebufferSampleLocationsfvNV, 4, framebuffer, start, count, v);
    
}
PFNGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC glad_debug_glNamedFramebufferSampleLocationsfvNV = glad_debug_impl_glNamedFramebufferSampleLocationsfvNV;
PFNGLNAMEDFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glad_glNamedFramebufferTextureMultiviewOVR = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedFramebufferTextureMultiviewOVR(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews) {
    _pre_call_gles2_callback("glNamedFramebufferTextureMultiviewOVR", (GLADapiproc) glad_glNamedFramebufferTextureMultiviewOVR, 6, framebuffer, attachment, texture, level, baseViewIndex, numViews);
    glad_glNamedFramebufferTextureMultiviewOVR(framebuffer, attachment, texture, level, baseViewIndex, numViews);
    _post_call_gles2_callback(NULL, "glNamedFramebufferTextureMultiviewOVR", (GLADapiproc) glad_glNamedFramebufferTextureMultiviewOVR, 6, framebuffer, attachment, texture, level, baseViewIndex, numViews);
    
}
PFNGLNAMEDFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glad_debug_glNamedFramebufferTextureMultiviewOVR = glad_debug_impl_glNamedFramebufferTextureMultiviewOVR;
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEADVANCEDAMDPROC glad_glNamedRenderbufferStorageMultisampleAdvancedAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glNamedRenderbufferStorageMultisampleAdvancedAMD(GLuint renderbuffer, GLsizei samples, GLsizei storageSamples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glNamedRenderbufferStorageMultisampleAdvancedAMD", (GLADapiproc) glad_glNamedRenderbufferStorageMultisampleAdvancedAMD, 6, renderbuffer, samples, storageSamples, internalformat, width, height);
    glad_glNamedRenderbufferStorageMultisampleAdvancedAMD(renderbuffer, samples, storageSamples, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glNamedRenderbufferStorageMultisampleAdvancedAMD", (GLADapiproc) glad_glNamedRenderbufferStorageMultisampleAdvancedAMD, 6, renderbuffer, samples, storageSamples, internalformat, width, height);
    
}
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEADVANCEDAMDPROC glad_debug_glNamedRenderbufferStorageMultisampleAdvancedAMD = glad_debug_impl_glNamedRenderbufferStorageMultisampleAdvancedAMD;
PFNGLOBJECTLABELPROC glad_glObjectLabel = NULL;
static void GLAD_API_PTR glad_debug_impl_glObjectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar * label) {
    _pre_call_gles2_callback("glObjectLabel", (GLADapiproc) glad_glObjectLabel, 4, identifier, name, length, label);
    glad_glObjectLabel(identifier, name, length, label);
    _post_call_gles2_callback(NULL, "glObjectLabel", (GLADapiproc) glad_glObjectLabel, 4, identifier, name, length, label);
    
}
PFNGLOBJECTLABELPROC glad_debug_glObjectLabel = glad_debug_impl_glObjectLabel;
PFNGLOBJECTLABELKHRPROC glad_glObjectLabelKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glObjectLabelKHR(GLenum identifier, GLuint name, GLsizei length, const GLchar * label) {
    _pre_call_gles2_callback("glObjectLabelKHR", (GLADapiproc) glad_glObjectLabelKHR, 4, identifier, name, length, label);
    glad_glObjectLabelKHR(identifier, name, length, label);
    _post_call_gles2_callback(NULL, "glObjectLabelKHR", (GLADapiproc) glad_glObjectLabelKHR, 4, identifier, name, length, label);
    
}
PFNGLOBJECTLABELKHRPROC glad_debug_glObjectLabelKHR = glad_debug_impl_glObjectLabelKHR;
PFNGLOBJECTPTRLABELPROC glad_glObjectPtrLabel = NULL;
static void GLAD_API_PTR glad_debug_impl_glObjectPtrLabel(const void * ptr, GLsizei length, const GLchar * label) {
    _pre_call_gles2_callback("glObjectPtrLabel", (GLADapiproc) glad_glObjectPtrLabel, 3, ptr, length, label);
    glad_glObjectPtrLabel(ptr, length, label);
    _post_call_gles2_callback(NULL, "glObjectPtrLabel", (GLADapiproc) glad_glObjectPtrLabel, 3, ptr, length, label);
    
}
PFNGLOBJECTPTRLABELPROC glad_debug_glObjectPtrLabel = glad_debug_impl_glObjectPtrLabel;
PFNGLOBJECTPTRLABELKHRPROC glad_glObjectPtrLabelKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glObjectPtrLabelKHR(const void * ptr, GLsizei length, const GLchar * label) {
    _pre_call_gles2_callback("glObjectPtrLabelKHR", (GLADapiproc) glad_glObjectPtrLabelKHR, 3, ptr, length, label);
    glad_glObjectPtrLabelKHR(ptr, length, label);
    _post_call_gles2_callback(NULL, "glObjectPtrLabelKHR", (GLADapiproc) glad_glObjectPtrLabelKHR, 3, ptr, length, label);
    
}
PFNGLOBJECTPTRLABELKHRPROC glad_debug_glObjectPtrLabelKHR = glad_debug_impl_glObjectPtrLabelKHR;
PFNGLPATCHPARAMETERIPROC glad_glPatchParameteri = NULL;
static void GLAD_API_PTR glad_debug_impl_glPatchParameteri(GLenum pname, GLint value) {
    _pre_call_gles2_callback("glPatchParameteri", (GLADapiproc) glad_glPatchParameteri, 2, pname, value);
    glad_glPatchParameteri(pname, value);
    _post_call_gles2_callback(NULL, "glPatchParameteri", (GLADapiproc) glad_glPatchParameteri, 2, pname, value);
    
}
PFNGLPATCHPARAMETERIPROC glad_debug_glPatchParameteri = glad_debug_impl_glPatchParameteri;
PFNGLPATCHPARAMETERIEXTPROC glad_glPatchParameteriEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPatchParameteriEXT(GLenum pname, GLint value) {
    _pre_call_gles2_callback("glPatchParameteriEXT", (GLADapiproc) glad_glPatchParameteriEXT, 2, pname, value);
    glad_glPatchParameteriEXT(pname, value);
    _post_call_gles2_callback(NULL, "glPatchParameteriEXT", (GLADapiproc) glad_glPatchParameteriEXT, 2, pname, value);
    
}
PFNGLPATCHPARAMETERIEXTPROC glad_debug_glPatchParameteriEXT = glad_debug_impl_glPatchParameteriEXT;
PFNGLPATCHPARAMETERIOESPROC glad_glPatchParameteriOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glPatchParameteriOES(GLenum pname, GLint value) {
    _pre_call_gles2_callback("glPatchParameteriOES", (GLADapiproc) glad_glPatchParameteriOES, 2, pname, value);
    glad_glPatchParameteriOES(pname, value);
    _post_call_gles2_callback(NULL, "glPatchParameteriOES", (GLADapiproc) glad_glPatchParameteriOES, 2, pname, value);
    
}
PFNGLPATCHPARAMETERIOESPROC glad_debug_glPatchParameteriOES = glad_debug_impl_glPatchParameteriOES;
PFNGLPATHCOMMANDSNVPROC glad_glPathCommandsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathCommandsNV(GLuint path, GLsizei numCommands, const GLubyte * commands, GLsizei numCoords, GLenum coordType, const void * coords) {
    _pre_call_gles2_callback("glPathCommandsNV", (GLADapiproc) glad_glPathCommandsNV, 6, path, numCommands, commands, numCoords, coordType, coords);
    glad_glPathCommandsNV(path, numCommands, commands, numCoords, coordType, coords);
    _post_call_gles2_callback(NULL, "glPathCommandsNV", (GLADapiproc) glad_glPathCommandsNV, 6, path, numCommands, commands, numCoords, coordType, coords);
    
}
PFNGLPATHCOMMANDSNVPROC glad_debug_glPathCommandsNV = glad_debug_impl_glPathCommandsNV;
PFNGLPATHCOORDSNVPROC glad_glPathCoordsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathCoordsNV(GLuint path, GLsizei numCoords, GLenum coordType, const void * coords) {
    _pre_call_gles2_callback("glPathCoordsNV", (GLADapiproc) glad_glPathCoordsNV, 4, path, numCoords, coordType, coords);
    glad_glPathCoordsNV(path, numCoords, coordType, coords);
    _post_call_gles2_callback(NULL, "glPathCoordsNV", (GLADapiproc) glad_glPathCoordsNV, 4, path, numCoords, coordType, coords);
    
}
PFNGLPATHCOORDSNVPROC glad_debug_glPathCoordsNV = glad_debug_impl_glPathCoordsNV;
PFNGLPATHCOVERDEPTHFUNCNVPROC glad_glPathCoverDepthFuncNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathCoverDepthFuncNV(GLenum func) {
    _pre_call_gles2_callback("glPathCoverDepthFuncNV", (GLADapiproc) glad_glPathCoverDepthFuncNV, 1, func);
    glad_glPathCoverDepthFuncNV(func);
    _post_call_gles2_callback(NULL, "glPathCoverDepthFuncNV", (GLADapiproc) glad_glPathCoverDepthFuncNV, 1, func);
    
}
PFNGLPATHCOVERDEPTHFUNCNVPROC glad_debug_glPathCoverDepthFuncNV = glad_debug_impl_glPathCoverDepthFuncNV;
PFNGLPATHDASHARRAYNVPROC glad_glPathDashArrayNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathDashArrayNV(GLuint path, GLsizei dashCount, const GLfloat * dashArray) {
    _pre_call_gles2_callback("glPathDashArrayNV", (GLADapiproc) glad_glPathDashArrayNV, 3, path, dashCount, dashArray);
    glad_glPathDashArrayNV(path, dashCount, dashArray);
    _post_call_gles2_callback(NULL, "glPathDashArrayNV", (GLADapiproc) glad_glPathDashArrayNV, 3, path, dashCount, dashArray);
    
}
PFNGLPATHDASHARRAYNVPROC glad_debug_glPathDashArrayNV = glad_debug_impl_glPathDashArrayNV;
PFNGLPATHGLYPHINDEXARRAYNVPROC glad_glPathGlyphIndexArrayNV = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glPathGlyphIndexArrayNV(GLuint firstPathName, GLenum fontTarget, const void * fontName, GLbitfield fontStyle, GLuint firstGlyphIndex, GLsizei numGlyphs, GLuint pathParameterTemplate, GLfloat emScale) {
    GLenum ret;
    _pre_call_gles2_callback("glPathGlyphIndexArrayNV", (GLADapiproc) glad_glPathGlyphIndexArrayNV, 8, firstPathName, fontTarget, fontName, fontStyle, firstGlyphIndex, numGlyphs, pathParameterTemplate, emScale);
    ret = glad_glPathGlyphIndexArrayNV(firstPathName, fontTarget, fontName, fontStyle, firstGlyphIndex, numGlyphs, pathParameterTemplate, emScale);
    _post_call_gles2_callback((void*) &ret, "glPathGlyphIndexArrayNV", (GLADapiproc) glad_glPathGlyphIndexArrayNV, 8, firstPathName, fontTarget, fontName, fontStyle, firstGlyphIndex, numGlyphs, pathParameterTemplate, emScale);
    return ret;
}
PFNGLPATHGLYPHINDEXARRAYNVPROC glad_debug_glPathGlyphIndexArrayNV = glad_debug_impl_glPathGlyphIndexArrayNV;
PFNGLPATHGLYPHINDEXRANGENVPROC glad_glPathGlyphIndexRangeNV = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glPathGlyphIndexRangeNV(GLenum fontTarget, const void * fontName, GLbitfield fontStyle, GLuint pathParameterTemplate, GLfloat emScale, GLuint * baseAndCount) {
    GLenum ret;
    _pre_call_gles2_callback("glPathGlyphIndexRangeNV", (GLADapiproc) glad_glPathGlyphIndexRangeNV, 6, fontTarget, fontName, fontStyle, pathParameterTemplate, emScale, baseAndCount);
    ret = glad_glPathGlyphIndexRangeNV(fontTarget, fontName, fontStyle, pathParameterTemplate, emScale, baseAndCount);
    _post_call_gles2_callback((void*) &ret, "glPathGlyphIndexRangeNV", (GLADapiproc) glad_glPathGlyphIndexRangeNV, 6, fontTarget, fontName, fontStyle, pathParameterTemplate, emScale, baseAndCount);
    return ret;
}
PFNGLPATHGLYPHINDEXRANGENVPROC glad_debug_glPathGlyphIndexRangeNV = glad_debug_impl_glPathGlyphIndexRangeNV;
PFNGLPATHGLYPHRANGENVPROC glad_glPathGlyphRangeNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathGlyphRangeNV(GLuint firstPathName, GLenum fontTarget, const void * fontName, GLbitfield fontStyle, GLuint firstGlyph, GLsizei numGlyphs, GLenum handleMissingGlyphs, GLuint pathParameterTemplate, GLfloat emScale) {
    _pre_call_gles2_callback("glPathGlyphRangeNV", (GLADapiproc) glad_glPathGlyphRangeNV, 9, firstPathName, fontTarget, fontName, fontStyle, firstGlyph, numGlyphs, handleMissingGlyphs, pathParameterTemplate, emScale);
    glad_glPathGlyphRangeNV(firstPathName, fontTarget, fontName, fontStyle, firstGlyph, numGlyphs, handleMissingGlyphs, pathParameterTemplate, emScale);
    _post_call_gles2_callback(NULL, "glPathGlyphRangeNV", (GLADapiproc) glad_glPathGlyphRangeNV, 9, firstPathName, fontTarget, fontName, fontStyle, firstGlyph, numGlyphs, handleMissingGlyphs, pathParameterTemplate, emScale);
    
}
PFNGLPATHGLYPHRANGENVPROC glad_debug_glPathGlyphRangeNV = glad_debug_impl_glPathGlyphRangeNV;
PFNGLPATHGLYPHSNVPROC glad_glPathGlyphsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathGlyphsNV(GLuint firstPathName, GLenum fontTarget, const void * fontName, GLbitfield fontStyle, GLsizei numGlyphs, GLenum type, const void * charcodes, GLenum handleMissingGlyphs, GLuint pathParameterTemplate, GLfloat emScale) {
    _pre_call_gles2_callback("glPathGlyphsNV", (GLADapiproc) glad_glPathGlyphsNV, 10, firstPathName, fontTarget, fontName, fontStyle, numGlyphs, type, charcodes, handleMissingGlyphs, pathParameterTemplate, emScale);
    glad_glPathGlyphsNV(firstPathName, fontTarget, fontName, fontStyle, numGlyphs, type, charcodes, handleMissingGlyphs, pathParameterTemplate, emScale);
    _post_call_gles2_callback(NULL, "glPathGlyphsNV", (GLADapiproc) glad_glPathGlyphsNV, 10, firstPathName, fontTarget, fontName, fontStyle, numGlyphs, type, charcodes, handleMissingGlyphs, pathParameterTemplate, emScale);
    
}
PFNGLPATHGLYPHSNVPROC glad_debug_glPathGlyphsNV = glad_debug_impl_glPathGlyphsNV;
PFNGLPATHMEMORYGLYPHINDEXARRAYNVPROC glad_glPathMemoryGlyphIndexArrayNV = NULL;
static GLenum GLAD_API_PTR glad_debug_impl_glPathMemoryGlyphIndexArrayNV(GLuint firstPathName, GLenum fontTarget, GLsizeiptr fontSize, const void * fontData, GLsizei faceIndex, GLuint firstGlyphIndex, GLsizei numGlyphs, GLuint pathParameterTemplate, GLfloat emScale) {
    GLenum ret;
    _pre_call_gles2_callback("glPathMemoryGlyphIndexArrayNV", (GLADapiproc) glad_glPathMemoryGlyphIndexArrayNV, 9, firstPathName, fontTarget, fontSize, fontData, faceIndex, firstGlyphIndex, numGlyphs, pathParameterTemplate, emScale);
    ret = glad_glPathMemoryGlyphIndexArrayNV(firstPathName, fontTarget, fontSize, fontData, faceIndex, firstGlyphIndex, numGlyphs, pathParameterTemplate, emScale);
    _post_call_gles2_callback((void*) &ret, "glPathMemoryGlyphIndexArrayNV", (GLADapiproc) glad_glPathMemoryGlyphIndexArrayNV, 9, firstPathName, fontTarget, fontSize, fontData, faceIndex, firstGlyphIndex, numGlyphs, pathParameterTemplate, emScale);
    return ret;
}
PFNGLPATHMEMORYGLYPHINDEXARRAYNVPROC glad_debug_glPathMemoryGlyphIndexArrayNV = glad_debug_impl_glPathMemoryGlyphIndexArrayNV;
PFNGLPATHPARAMETERFNVPROC glad_glPathParameterfNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathParameterfNV(GLuint path, GLenum pname, GLfloat value) {
    _pre_call_gles2_callback("glPathParameterfNV", (GLADapiproc) glad_glPathParameterfNV, 3, path, pname, value);
    glad_glPathParameterfNV(path, pname, value);
    _post_call_gles2_callback(NULL, "glPathParameterfNV", (GLADapiproc) glad_glPathParameterfNV, 3, path, pname, value);
    
}
PFNGLPATHPARAMETERFNVPROC glad_debug_glPathParameterfNV = glad_debug_impl_glPathParameterfNV;
PFNGLPATHPARAMETERFVNVPROC glad_glPathParameterfvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathParameterfvNV(GLuint path, GLenum pname, const GLfloat * value) {
    _pre_call_gles2_callback("glPathParameterfvNV", (GLADapiproc) glad_glPathParameterfvNV, 3, path, pname, value);
    glad_glPathParameterfvNV(path, pname, value);
    _post_call_gles2_callback(NULL, "glPathParameterfvNV", (GLADapiproc) glad_glPathParameterfvNV, 3, path, pname, value);
    
}
PFNGLPATHPARAMETERFVNVPROC glad_debug_glPathParameterfvNV = glad_debug_impl_glPathParameterfvNV;
PFNGLPATHPARAMETERINVPROC glad_glPathParameteriNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathParameteriNV(GLuint path, GLenum pname, GLint value) {
    _pre_call_gles2_callback("glPathParameteriNV", (GLADapiproc) glad_glPathParameteriNV, 3, path, pname, value);
    glad_glPathParameteriNV(path, pname, value);
    _post_call_gles2_callback(NULL, "glPathParameteriNV", (GLADapiproc) glad_glPathParameteriNV, 3, path, pname, value);
    
}
PFNGLPATHPARAMETERINVPROC glad_debug_glPathParameteriNV = glad_debug_impl_glPathParameteriNV;
PFNGLPATHPARAMETERIVNVPROC glad_glPathParameterivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathParameterivNV(GLuint path, GLenum pname, const GLint * value) {
    _pre_call_gles2_callback("glPathParameterivNV", (GLADapiproc) glad_glPathParameterivNV, 3, path, pname, value);
    glad_glPathParameterivNV(path, pname, value);
    _post_call_gles2_callback(NULL, "glPathParameterivNV", (GLADapiproc) glad_glPathParameterivNV, 3, path, pname, value);
    
}
PFNGLPATHPARAMETERIVNVPROC glad_debug_glPathParameterivNV = glad_debug_impl_glPathParameterivNV;
PFNGLPATHSTENCILDEPTHOFFSETNVPROC glad_glPathStencilDepthOffsetNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathStencilDepthOffsetNV(GLfloat factor, GLfloat units) {
    _pre_call_gles2_callback("glPathStencilDepthOffsetNV", (GLADapiproc) glad_glPathStencilDepthOffsetNV, 2, factor, units);
    glad_glPathStencilDepthOffsetNV(factor, units);
    _post_call_gles2_callback(NULL, "glPathStencilDepthOffsetNV", (GLADapiproc) glad_glPathStencilDepthOffsetNV, 2, factor, units);
    
}
PFNGLPATHSTENCILDEPTHOFFSETNVPROC glad_debug_glPathStencilDepthOffsetNV = glad_debug_impl_glPathStencilDepthOffsetNV;
PFNGLPATHSTENCILFUNCNVPROC glad_glPathStencilFuncNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathStencilFuncNV(GLenum func, GLint ref, GLuint mask) {
    _pre_call_gles2_callback("glPathStencilFuncNV", (GLADapiproc) glad_glPathStencilFuncNV, 3, func, ref, mask);
    glad_glPathStencilFuncNV(func, ref, mask);
    _post_call_gles2_callback(NULL, "glPathStencilFuncNV", (GLADapiproc) glad_glPathStencilFuncNV, 3, func, ref, mask);
    
}
PFNGLPATHSTENCILFUNCNVPROC glad_debug_glPathStencilFuncNV = glad_debug_impl_glPathStencilFuncNV;
PFNGLPATHSTRINGNVPROC glad_glPathStringNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathStringNV(GLuint path, GLenum format, GLsizei length, const void * pathString) {
    _pre_call_gles2_callback("glPathStringNV", (GLADapiproc) glad_glPathStringNV, 4, path, format, length, pathString);
    glad_glPathStringNV(path, format, length, pathString);
    _post_call_gles2_callback(NULL, "glPathStringNV", (GLADapiproc) glad_glPathStringNV, 4, path, format, length, pathString);
    
}
PFNGLPATHSTRINGNVPROC glad_debug_glPathStringNV = glad_debug_impl_glPathStringNV;
PFNGLPATHSUBCOMMANDSNVPROC glad_glPathSubCommandsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathSubCommandsNV(GLuint path, GLsizei commandStart, GLsizei commandsToDelete, GLsizei numCommands, const GLubyte * commands, GLsizei numCoords, GLenum coordType, const void * coords) {
    _pre_call_gles2_callback("glPathSubCommandsNV", (GLADapiproc) glad_glPathSubCommandsNV, 8, path, commandStart, commandsToDelete, numCommands, commands, numCoords, coordType, coords);
    glad_glPathSubCommandsNV(path, commandStart, commandsToDelete, numCommands, commands, numCoords, coordType, coords);
    _post_call_gles2_callback(NULL, "glPathSubCommandsNV", (GLADapiproc) glad_glPathSubCommandsNV, 8, path, commandStart, commandsToDelete, numCommands, commands, numCoords, coordType, coords);
    
}
PFNGLPATHSUBCOMMANDSNVPROC glad_debug_glPathSubCommandsNV = glad_debug_impl_glPathSubCommandsNV;
PFNGLPATHSUBCOORDSNVPROC glad_glPathSubCoordsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPathSubCoordsNV(GLuint path, GLsizei coordStart, GLsizei numCoords, GLenum coordType, const void * coords) {
    _pre_call_gles2_callback("glPathSubCoordsNV", (GLADapiproc) glad_glPathSubCoordsNV, 5, path, coordStart, numCoords, coordType, coords);
    glad_glPathSubCoordsNV(path, coordStart, numCoords, coordType, coords);
    _post_call_gles2_callback(NULL, "glPathSubCoordsNV", (GLADapiproc) glad_glPathSubCoordsNV, 5, path, coordStart, numCoords, coordType, coords);
    
}
PFNGLPATHSUBCOORDSNVPROC glad_debug_glPathSubCoordsNV = glad_debug_impl_glPathSubCoordsNV;
PFNGLPAUSETRANSFORMFEEDBACKPROC glad_glPauseTransformFeedback = NULL;
static void GLAD_API_PTR glad_debug_impl_glPauseTransformFeedback(void) {
    _pre_call_gles2_callback("glPauseTransformFeedback", (GLADapiproc) glad_glPauseTransformFeedback, 0);
    glad_glPauseTransformFeedback();
    _post_call_gles2_callback(NULL, "glPauseTransformFeedback", (GLADapiproc) glad_glPauseTransformFeedback, 0);
    
}
PFNGLPAUSETRANSFORMFEEDBACKPROC glad_debug_glPauseTransformFeedback = glad_debug_impl_glPauseTransformFeedback;
PFNGLPIXELSTOREIPROC glad_glPixelStorei = NULL;
static void GLAD_API_PTR glad_debug_impl_glPixelStorei(GLenum pname, GLint param) {
    _pre_call_gles2_callback("glPixelStorei", (GLADapiproc) glad_glPixelStorei, 2, pname, param);
    glad_glPixelStorei(pname, param);
    _post_call_gles2_callback(NULL, "glPixelStorei", (GLADapiproc) glad_glPixelStorei, 2, pname, param);
    
}
PFNGLPIXELSTOREIPROC glad_debug_glPixelStorei = glad_debug_impl_glPixelStorei;
PFNGLPOINTALONGPATHNVPROC glad_glPointAlongPathNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glPointAlongPathNV(GLuint path, GLsizei startSegment, GLsizei numSegments, GLfloat distance, GLfloat * x, GLfloat * y, GLfloat * tangentX, GLfloat * tangentY) {
    GLboolean ret;
    _pre_call_gles2_callback("glPointAlongPathNV", (GLADapiproc) glad_glPointAlongPathNV, 8, path, startSegment, numSegments, distance, x, y, tangentX, tangentY);
    ret = glad_glPointAlongPathNV(path, startSegment, numSegments, distance, x, y, tangentX, tangentY);
    _post_call_gles2_callback((void*) &ret, "glPointAlongPathNV", (GLADapiproc) glad_glPointAlongPathNV, 8, path, startSegment, numSegments, distance, x, y, tangentX, tangentY);
    return ret;
}
PFNGLPOINTALONGPATHNVPROC glad_debug_glPointAlongPathNV = glad_debug_impl_glPointAlongPathNV;
PFNGLPOLYGONMODENVPROC glad_glPolygonModeNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glPolygonModeNV(GLenum face, GLenum mode) {
    _pre_call_gles2_callback("glPolygonModeNV", (GLADapiproc) glad_glPolygonModeNV, 2, face, mode);
    glad_glPolygonModeNV(face, mode);
    _post_call_gles2_callback(NULL, "glPolygonModeNV", (GLADapiproc) glad_glPolygonModeNV, 2, face, mode);
    
}
PFNGLPOLYGONMODENVPROC glad_debug_glPolygonModeNV = glad_debug_impl_glPolygonModeNV;
PFNGLPOLYGONOFFSETPROC glad_glPolygonOffset = NULL;
static void GLAD_API_PTR glad_debug_impl_glPolygonOffset(GLfloat factor, GLfloat units) {
    _pre_call_gles2_callback("glPolygonOffset", (GLADapiproc) glad_glPolygonOffset, 2, factor, units);
    glad_glPolygonOffset(factor, units);
    _post_call_gles2_callback(NULL, "glPolygonOffset", (GLADapiproc) glad_glPolygonOffset, 2, factor, units);
    
}
PFNGLPOLYGONOFFSETPROC glad_debug_glPolygonOffset = glad_debug_impl_glPolygonOffset;
PFNGLPOLYGONOFFSETCLAMPEXTPROC glad_glPolygonOffsetClampEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPolygonOffsetClampEXT(GLfloat factor, GLfloat units, GLfloat clamp) {
    _pre_call_gles2_callback("glPolygonOffsetClampEXT", (GLADapiproc) glad_glPolygonOffsetClampEXT, 3, factor, units, clamp);
    glad_glPolygonOffsetClampEXT(factor, units, clamp);
    _post_call_gles2_callback(NULL, "glPolygonOffsetClampEXT", (GLADapiproc) glad_glPolygonOffsetClampEXT, 3, factor, units, clamp);
    
}
PFNGLPOLYGONOFFSETCLAMPEXTPROC glad_debug_glPolygonOffsetClampEXT = glad_debug_impl_glPolygonOffsetClampEXT;
PFNGLPOPDEBUGGROUPPROC glad_glPopDebugGroup = NULL;
static void GLAD_API_PTR glad_debug_impl_glPopDebugGroup(void) {
    _pre_call_gles2_callback("glPopDebugGroup", (GLADapiproc) glad_glPopDebugGroup, 0);
    glad_glPopDebugGroup();
    _post_call_gles2_callback(NULL, "glPopDebugGroup", (GLADapiproc) glad_glPopDebugGroup, 0);
    
}
PFNGLPOPDEBUGGROUPPROC glad_debug_glPopDebugGroup = glad_debug_impl_glPopDebugGroup;
PFNGLPOPDEBUGGROUPKHRPROC glad_glPopDebugGroupKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glPopDebugGroupKHR(void) {
    _pre_call_gles2_callback("glPopDebugGroupKHR", (GLADapiproc) glad_glPopDebugGroupKHR, 0);
    glad_glPopDebugGroupKHR();
    _post_call_gles2_callback(NULL, "glPopDebugGroupKHR", (GLADapiproc) glad_glPopDebugGroupKHR, 0);
    
}
PFNGLPOPDEBUGGROUPKHRPROC glad_debug_glPopDebugGroupKHR = glad_debug_impl_glPopDebugGroupKHR;
PFNGLPOPGROUPMARKEREXTPROC glad_glPopGroupMarkerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPopGroupMarkerEXT(void) {
    _pre_call_gles2_callback("glPopGroupMarkerEXT", (GLADapiproc) glad_glPopGroupMarkerEXT, 0);
    glad_glPopGroupMarkerEXT();
    _post_call_gles2_callback(NULL, "glPopGroupMarkerEXT", (GLADapiproc) glad_glPopGroupMarkerEXT, 0);
    
}
PFNGLPOPGROUPMARKEREXTPROC glad_debug_glPopGroupMarkerEXT = glad_debug_impl_glPopGroupMarkerEXT;
PFNGLPRIMITIVEBOUNDINGBOXPROC glad_glPrimitiveBoundingBox = NULL;
static void GLAD_API_PTR glad_debug_impl_glPrimitiveBoundingBox(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    _pre_call_gles2_callback("glPrimitiveBoundingBox", (GLADapiproc) glad_glPrimitiveBoundingBox, 8, minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    glad_glPrimitiveBoundingBox(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    _post_call_gles2_callback(NULL, "glPrimitiveBoundingBox", (GLADapiproc) glad_glPrimitiveBoundingBox, 8, minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    
}
PFNGLPRIMITIVEBOUNDINGBOXPROC glad_debug_glPrimitiveBoundingBox = glad_debug_impl_glPrimitiveBoundingBox;
PFNGLPRIMITIVEBOUNDINGBOXEXTPROC glad_glPrimitiveBoundingBoxEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPrimitiveBoundingBoxEXT(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    _pre_call_gles2_callback("glPrimitiveBoundingBoxEXT", (GLADapiproc) glad_glPrimitiveBoundingBoxEXT, 8, minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    glad_glPrimitiveBoundingBoxEXT(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    _post_call_gles2_callback(NULL, "glPrimitiveBoundingBoxEXT", (GLADapiproc) glad_glPrimitiveBoundingBoxEXT, 8, minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    
}
PFNGLPRIMITIVEBOUNDINGBOXEXTPROC glad_debug_glPrimitiveBoundingBoxEXT = glad_debug_impl_glPrimitiveBoundingBoxEXT;
PFNGLPRIMITIVEBOUNDINGBOXOESPROC glad_glPrimitiveBoundingBoxOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glPrimitiveBoundingBoxOES(GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW) {
    _pre_call_gles2_callback("glPrimitiveBoundingBoxOES", (GLADapiproc) glad_glPrimitiveBoundingBoxOES, 8, minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    glad_glPrimitiveBoundingBoxOES(minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    _post_call_gles2_callback(NULL, "glPrimitiveBoundingBoxOES", (GLADapiproc) glad_glPrimitiveBoundingBoxOES, 8, minX, minY, minZ, minW, maxX, maxY, maxZ, maxW);
    
}
PFNGLPRIMITIVEBOUNDINGBOXOESPROC glad_debug_glPrimitiveBoundingBoxOES = glad_debug_impl_glPrimitiveBoundingBoxOES;
PFNGLPROGRAMBINARYPROC glad_glProgramBinary = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramBinary(GLuint program, GLenum binaryFormat, const void * binary, GLsizei length) {
    _pre_call_gles2_callback("glProgramBinary", (GLADapiproc) glad_glProgramBinary, 4, program, binaryFormat, binary, length);
    glad_glProgramBinary(program, binaryFormat, binary, length);
    _post_call_gles2_callback(NULL, "glProgramBinary", (GLADapiproc) glad_glProgramBinary, 4, program, binaryFormat, binary, length);
    
}
PFNGLPROGRAMBINARYPROC glad_debug_glProgramBinary = glad_debug_impl_glProgramBinary;
PFNGLPROGRAMBINARYOESPROC glad_glProgramBinaryOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramBinaryOES(GLuint program, GLenum binaryFormat, const void * binary, GLint length) {
    _pre_call_gles2_callback("glProgramBinaryOES", (GLADapiproc) glad_glProgramBinaryOES, 4, program, binaryFormat, binary, length);
    glad_glProgramBinaryOES(program, binaryFormat, binary, length);
    _post_call_gles2_callback(NULL, "glProgramBinaryOES", (GLADapiproc) glad_glProgramBinaryOES, 4, program, binaryFormat, binary, length);
    
}
PFNGLPROGRAMBINARYOESPROC glad_debug_glProgramBinaryOES = glad_debug_impl_glProgramBinaryOES;
PFNGLPROGRAMPARAMETERIPROC glad_glProgramParameteri = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramParameteri(GLuint program, GLenum pname, GLint value) {
    _pre_call_gles2_callback("glProgramParameteri", (GLADapiproc) glad_glProgramParameteri, 3, program, pname, value);
    glad_glProgramParameteri(program, pname, value);
    _post_call_gles2_callback(NULL, "glProgramParameteri", (GLADapiproc) glad_glProgramParameteri, 3, program, pname, value);
    
}
PFNGLPROGRAMPARAMETERIPROC glad_debug_glProgramParameteri = glad_debug_impl_glProgramParameteri;
PFNGLPROGRAMPARAMETERIEXTPROC glad_glProgramParameteriEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramParameteriEXT(GLuint program, GLenum pname, GLint value) {
    _pre_call_gles2_callback("glProgramParameteriEXT", (GLADapiproc) glad_glProgramParameteriEXT, 3, program, pname, value);
    glad_glProgramParameteriEXT(program, pname, value);
    _post_call_gles2_callback(NULL, "glProgramParameteriEXT", (GLADapiproc) glad_glProgramParameteriEXT, 3, program, pname, value);
    
}
PFNGLPROGRAMPARAMETERIEXTPROC glad_debug_glProgramParameteriEXT = glad_debug_impl_glProgramParameteriEXT;
PFNGLPROGRAMPATHFRAGMENTINPUTGENNVPROC glad_glProgramPathFragmentInputGenNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramPathFragmentInputGenNV(GLuint program, GLint location, GLenum genMode, GLint components, const GLfloat * coeffs) {
    _pre_call_gles2_callback("glProgramPathFragmentInputGenNV", (GLADapiproc) glad_glProgramPathFragmentInputGenNV, 5, program, location, genMode, components, coeffs);
    glad_glProgramPathFragmentInputGenNV(program, location, genMode, components, coeffs);
    _post_call_gles2_callback(NULL, "glProgramPathFragmentInputGenNV", (GLADapiproc) glad_glProgramPathFragmentInputGenNV, 5, program, location, genMode, components, coeffs);
    
}
PFNGLPROGRAMPATHFRAGMENTINPUTGENNVPROC glad_debug_glProgramPathFragmentInputGenNV = glad_debug_impl_glProgramPathFragmentInputGenNV;
PFNGLPROGRAMUNIFORM1FPROC glad_glProgramUniform1f = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1f(GLuint program, GLint location, GLfloat v0) {
    _pre_call_gles2_callback("glProgramUniform1f", (GLADapiproc) glad_glProgramUniform1f, 3, program, location, v0);
    glad_glProgramUniform1f(program, location, v0);
    _post_call_gles2_callback(NULL, "glProgramUniform1f", (GLADapiproc) glad_glProgramUniform1f, 3, program, location, v0);
    
}
PFNGLPROGRAMUNIFORM1FPROC glad_debug_glProgramUniform1f = glad_debug_impl_glProgramUniform1f;
PFNGLPROGRAMUNIFORM1FEXTPROC glad_glProgramUniform1fEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1fEXT(GLuint program, GLint location, GLfloat v0) {
    _pre_call_gles2_callback("glProgramUniform1fEXT", (GLADapiproc) glad_glProgramUniform1fEXT, 3, program, location, v0);
    glad_glProgramUniform1fEXT(program, location, v0);
    _post_call_gles2_callback(NULL, "glProgramUniform1fEXT", (GLADapiproc) glad_glProgramUniform1fEXT, 3, program, location, v0);
    
}
PFNGLPROGRAMUNIFORM1FEXTPROC glad_debug_glProgramUniform1fEXT = glad_debug_impl_glProgramUniform1fEXT;
PFNGLPROGRAMUNIFORM1FVPROC glad_glProgramUniform1fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniform1fv", (GLADapiproc) glad_glProgramUniform1fv, 4, program, location, count, value);
    glad_glProgramUniform1fv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform1fv", (GLADapiproc) glad_glProgramUniform1fv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1FVPROC glad_debug_glProgramUniform1fv = glad_debug_impl_glProgramUniform1fv;
PFNGLPROGRAMUNIFORM1FVEXTPROC glad_glProgramUniform1fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniform1fvEXT", (GLADapiproc) glad_glProgramUniform1fvEXT, 4, program, location, count, value);
    glad_glProgramUniform1fvEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform1fvEXT", (GLADapiproc) glad_glProgramUniform1fvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1FVEXTPROC glad_debug_glProgramUniform1fvEXT = glad_debug_impl_glProgramUniform1fvEXT;
PFNGLPROGRAMUNIFORM1IPROC glad_glProgramUniform1i = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1i(GLuint program, GLint location, GLint v0) {
    _pre_call_gles2_callback("glProgramUniform1i", (GLADapiproc) glad_glProgramUniform1i, 3, program, location, v0);
    glad_glProgramUniform1i(program, location, v0);
    _post_call_gles2_callback(NULL, "glProgramUniform1i", (GLADapiproc) glad_glProgramUniform1i, 3, program, location, v0);
    
}
PFNGLPROGRAMUNIFORM1IPROC glad_debug_glProgramUniform1i = glad_debug_impl_glProgramUniform1i;
PFNGLPROGRAMUNIFORM1I64NVPROC glad_glProgramUniform1i64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1i64NV(GLuint program, GLint location, GLint64EXT x) {
    _pre_call_gles2_callback("glProgramUniform1i64NV", (GLADapiproc) glad_glProgramUniform1i64NV, 3, program, location, x);
    glad_glProgramUniform1i64NV(program, location, x);
    _post_call_gles2_callback(NULL, "glProgramUniform1i64NV", (GLADapiproc) glad_glProgramUniform1i64NV, 3, program, location, x);
    
}
PFNGLPROGRAMUNIFORM1I64NVPROC glad_debug_glProgramUniform1i64NV = glad_debug_impl_glProgramUniform1i64NV;
PFNGLPROGRAMUNIFORM1I64VNVPROC glad_glProgramUniform1i64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT * value) {
    _pre_call_gles2_callback("glProgramUniform1i64vNV", (GLADapiproc) glad_glProgramUniform1i64vNV, 4, program, location, count, value);
    glad_glProgramUniform1i64vNV(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform1i64vNV", (GLADapiproc) glad_glProgramUniform1i64vNV, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1I64VNVPROC glad_debug_glProgramUniform1i64vNV = glad_debug_impl_glProgramUniform1i64vNV;
PFNGLPROGRAMUNIFORM1IEXTPROC glad_glProgramUniform1iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1iEXT(GLuint program, GLint location, GLint v0) {
    _pre_call_gles2_callback("glProgramUniform1iEXT", (GLADapiproc) glad_glProgramUniform1iEXT, 3, program, location, v0);
    glad_glProgramUniform1iEXT(program, location, v0);
    _post_call_gles2_callback(NULL, "glProgramUniform1iEXT", (GLADapiproc) glad_glProgramUniform1iEXT, 3, program, location, v0);
    
}
PFNGLPROGRAMUNIFORM1IEXTPROC glad_debug_glProgramUniform1iEXT = glad_debug_impl_glProgramUniform1iEXT;
PFNGLPROGRAMUNIFORM1IVPROC glad_glProgramUniform1iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1iv(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glProgramUniform1iv", (GLADapiproc) glad_glProgramUniform1iv, 4, program, location, count, value);
    glad_glProgramUniform1iv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform1iv", (GLADapiproc) glad_glProgramUniform1iv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1IVPROC glad_debug_glProgramUniform1iv = glad_debug_impl_glProgramUniform1iv;
PFNGLPROGRAMUNIFORM1IVEXTPROC glad_glProgramUniform1ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1ivEXT(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glProgramUniform1ivEXT", (GLADapiproc) glad_glProgramUniform1ivEXT, 4, program, location, count, value);
    glad_glProgramUniform1ivEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform1ivEXT", (GLADapiproc) glad_glProgramUniform1ivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1IVEXTPROC glad_debug_glProgramUniform1ivEXT = glad_debug_impl_glProgramUniform1ivEXT;
PFNGLPROGRAMUNIFORM1UIPROC glad_glProgramUniform1ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1ui(GLuint program, GLint location, GLuint v0) {
    _pre_call_gles2_callback("glProgramUniform1ui", (GLADapiproc) glad_glProgramUniform1ui, 3, program, location, v0);
    glad_glProgramUniform1ui(program, location, v0);
    _post_call_gles2_callback(NULL, "glProgramUniform1ui", (GLADapiproc) glad_glProgramUniform1ui, 3, program, location, v0);
    
}
PFNGLPROGRAMUNIFORM1UIPROC glad_debug_glProgramUniform1ui = glad_debug_impl_glProgramUniform1ui;
PFNGLPROGRAMUNIFORM1UI64NVPROC glad_glProgramUniform1ui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1ui64NV(GLuint program, GLint location, GLuint64EXT x) {
    _pre_call_gles2_callback("glProgramUniform1ui64NV", (GLADapiproc) glad_glProgramUniform1ui64NV, 3, program, location, x);
    glad_glProgramUniform1ui64NV(program, location, x);
    _post_call_gles2_callback(NULL, "glProgramUniform1ui64NV", (GLADapiproc) glad_glProgramUniform1ui64NV, 3, program, location, x);
    
}
PFNGLPROGRAMUNIFORM1UI64NVPROC glad_debug_glProgramUniform1ui64NV = glad_debug_impl_glProgramUniform1ui64NV;
PFNGLPROGRAMUNIFORM1UI64VNVPROC glad_glProgramUniform1ui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT * value) {
    _pre_call_gles2_callback("glProgramUniform1ui64vNV", (GLADapiproc) glad_glProgramUniform1ui64vNV, 4, program, location, count, value);
    glad_glProgramUniform1ui64vNV(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform1ui64vNV", (GLADapiproc) glad_glProgramUniform1ui64vNV, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1UI64VNVPROC glad_debug_glProgramUniform1ui64vNV = glad_debug_impl_glProgramUniform1ui64vNV;
PFNGLPROGRAMUNIFORM1UIEXTPROC glad_glProgramUniform1uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1uiEXT(GLuint program, GLint location, GLuint v0) {
    _pre_call_gles2_callback("glProgramUniform1uiEXT", (GLADapiproc) glad_glProgramUniform1uiEXT, 3, program, location, v0);
    glad_glProgramUniform1uiEXT(program, location, v0);
    _post_call_gles2_callback(NULL, "glProgramUniform1uiEXT", (GLADapiproc) glad_glProgramUniform1uiEXT, 3, program, location, v0);
    
}
PFNGLPROGRAMUNIFORM1UIEXTPROC glad_debug_glProgramUniform1uiEXT = glad_debug_impl_glProgramUniform1uiEXT;
PFNGLPROGRAMUNIFORM1UIVPROC glad_glProgramUniform1uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glProgramUniform1uiv", (GLADapiproc) glad_glProgramUniform1uiv, 4, program, location, count, value);
    glad_glProgramUniform1uiv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform1uiv", (GLADapiproc) glad_glProgramUniform1uiv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1UIVPROC glad_debug_glProgramUniform1uiv = glad_debug_impl_glProgramUniform1uiv;
PFNGLPROGRAMUNIFORM1UIVEXTPROC glad_glProgramUniform1uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform1uivEXT(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glProgramUniform1uivEXT", (GLADapiproc) glad_glProgramUniform1uivEXT, 4, program, location, count, value);
    glad_glProgramUniform1uivEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform1uivEXT", (GLADapiproc) glad_glProgramUniform1uivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM1UIVEXTPROC glad_debug_glProgramUniform1uivEXT = glad_debug_impl_glProgramUniform1uivEXT;
PFNGLPROGRAMUNIFORM2FPROC glad_glProgramUniform2f = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1) {
    _pre_call_gles2_callback("glProgramUniform2f", (GLADapiproc) glad_glProgramUniform2f, 4, program, location, v0, v1);
    glad_glProgramUniform2f(program, location, v0, v1);
    _post_call_gles2_callback(NULL, "glProgramUniform2f", (GLADapiproc) glad_glProgramUniform2f, 4, program, location, v0, v1);
    
}
PFNGLPROGRAMUNIFORM2FPROC glad_debug_glProgramUniform2f = glad_debug_impl_glProgramUniform2f;
PFNGLPROGRAMUNIFORM2FEXTPROC glad_glProgramUniform2fEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1) {
    _pre_call_gles2_callback("glProgramUniform2fEXT", (GLADapiproc) glad_glProgramUniform2fEXT, 4, program, location, v0, v1);
    glad_glProgramUniform2fEXT(program, location, v0, v1);
    _post_call_gles2_callback(NULL, "glProgramUniform2fEXT", (GLADapiproc) glad_glProgramUniform2fEXT, 4, program, location, v0, v1);
    
}
PFNGLPROGRAMUNIFORM2FEXTPROC glad_debug_glProgramUniform2fEXT = glad_debug_impl_glProgramUniform2fEXT;
PFNGLPROGRAMUNIFORM2FVPROC glad_glProgramUniform2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniform2fv", (GLADapiproc) glad_glProgramUniform2fv, 4, program, location, count, value);
    glad_glProgramUniform2fv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform2fv", (GLADapiproc) glad_glProgramUniform2fv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2FVPROC glad_debug_glProgramUniform2fv = glad_debug_impl_glProgramUniform2fv;
PFNGLPROGRAMUNIFORM2FVEXTPROC glad_glProgramUniform2fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniform2fvEXT", (GLADapiproc) glad_glProgramUniform2fvEXT, 4, program, location, count, value);
    glad_glProgramUniform2fvEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform2fvEXT", (GLADapiproc) glad_glProgramUniform2fvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2FVEXTPROC glad_debug_glProgramUniform2fvEXT = glad_debug_impl_glProgramUniform2fvEXT;
PFNGLPROGRAMUNIFORM2IPROC glad_glProgramUniform2i = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2i(GLuint program, GLint location, GLint v0, GLint v1) {
    _pre_call_gles2_callback("glProgramUniform2i", (GLADapiproc) glad_glProgramUniform2i, 4, program, location, v0, v1);
    glad_glProgramUniform2i(program, location, v0, v1);
    _post_call_gles2_callback(NULL, "glProgramUniform2i", (GLADapiproc) glad_glProgramUniform2i, 4, program, location, v0, v1);
    
}
PFNGLPROGRAMUNIFORM2IPROC glad_debug_glProgramUniform2i = glad_debug_impl_glProgramUniform2i;
PFNGLPROGRAMUNIFORM2I64NVPROC glad_glProgramUniform2i64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2i64NV(GLuint program, GLint location, GLint64EXT x, GLint64EXT y) {
    _pre_call_gles2_callback("glProgramUniform2i64NV", (GLADapiproc) glad_glProgramUniform2i64NV, 4, program, location, x, y);
    glad_glProgramUniform2i64NV(program, location, x, y);
    _post_call_gles2_callback(NULL, "glProgramUniform2i64NV", (GLADapiproc) glad_glProgramUniform2i64NV, 4, program, location, x, y);
    
}
PFNGLPROGRAMUNIFORM2I64NVPROC glad_debug_glProgramUniform2i64NV = glad_debug_impl_glProgramUniform2i64NV;
PFNGLPROGRAMUNIFORM2I64VNVPROC glad_glProgramUniform2i64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT * value) {
    _pre_call_gles2_callback("glProgramUniform2i64vNV", (GLADapiproc) glad_glProgramUniform2i64vNV, 4, program, location, count, value);
    glad_glProgramUniform2i64vNV(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform2i64vNV", (GLADapiproc) glad_glProgramUniform2i64vNV, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2I64VNVPROC glad_debug_glProgramUniform2i64vNV = glad_debug_impl_glProgramUniform2i64vNV;
PFNGLPROGRAMUNIFORM2IEXTPROC glad_glProgramUniform2iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2iEXT(GLuint program, GLint location, GLint v0, GLint v1) {
    _pre_call_gles2_callback("glProgramUniform2iEXT", (GLADapiproc) glad_glProgramUniform2iEXT, 4, program, location, v0, v1);
    glad_glProgramUniform2iEXT(program, location, v0, v1);
    _post_call_gles2_callback(NULL, "glProgramUniform2iEXT", (GLADapiproc) glad_glProgramUniform2iEXT, 4, program, location, v0, v1);
    
}
PFNGLPROGRAMUNIFORM2IEXTPROC glad_debug_glProgramUniform2iEXT = glad_debug_impl_glProgramUniform2iEXT;
PFNGLPROGRAMUNIFORM2IVPROC glad_glProgramUniform2iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2iv(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glProgramUniform2iv", (GLADapiproc) glad_glProgramUniform2iv, 4, program, location, count, value);
    glad_glProgramUniform2iv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform2iv", (GLADapiproc) glad_glProgramUniform2iv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2IVPROC glad_debug_glProgramUniform2iv = glad_debug_impl_glProgramUniform2iv;
PFNGLPROGRAMUNIFORM2IVEXTPROC glad_glProgramUniform2ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2ivEXT(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glProgramUniform2ivEXT", (GLADapiproc) glad_glProgramUniform2ivEXT, 4, program, location, count, value);
    glad_glProgramUniform2ivEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform2ivEXT", (GLADapiproc) glad_glProgramUniform2ivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2IVEXTPROC glad_debug_glProgramUniform2ivEXT = glad_debug_impl_glProgramUniform2ivEXT;
PFNGLPROGRAMUNIFORM2UIPROC glad_glProgramUniform2ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1) {
    _pre_call_gles2_callback("glProgramUniform2ui", (GLADapiproc) glad_glProgramUniform2ui, 4, program, location, v0, v1);
    glad_glProgramUniform2ui(program, location, v0, v1);
    _post_call_gles2_callback(NULL, "glProgramUniform2ui", (GLADapiproc) glad_glProgramUniform2ui, 4, program, location, v0, v1);
    
}
PFNGLPROGRAMUNIFORM2UIPROC glad_debug_glProgramUniform2ui = glad_debug_impl_glProgramUniform2ui;
PFNGLPROGRAMUNIFORM2UI64NVPROC glad_glProgramUniform2ui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2ui64NV(GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y) {
    _pre_call_gles2_callback("glProgramUniform2ui64NV", (GLADapiproc) glad_glProgramUniform2ui64NV, 4, program, location, x, y);
    glad_glProgramUniform2ui64NV(program, location, x, y);
    _post_call_gles2_callback(NULL, "glProgramUniform2ui64NV", (GLADapiproc) glad_glProgramUniform2ui64NV, 4, program, location, x, y);
    
}
PFNGLPROGRAMUNIFORM2UI64NVPROC glad_debug_glProgramUniform2ui64NV = glad_debug_impl_glProgramUniform2ui64NV;
PFNGLPROGRAMUNIFORM2UI64VNVPROC glad_glProgramUniform2ui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT * value) {
    _pre_call_gles2_callback("glProgramUniform2ui64vNV", (GLADapiproc) glad_glProgramUniform2ui64vNV, 4, program, location, count, value);
    glad_glProgramUniform2ui64vNV(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform2ui64vNV", (GLADapiproc) glad_glProgramUniform2ui64vNV, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2UI64VNVPROC glad_debug_glProgramUniform2ui64vNV = glad_debug_impl_glProgramUniform2ui64vNV;
PFNGLPROGRAMUNIFORM2UIEXTPROC glad_glProgramUniform2uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1) {
    _pre_call_gles2_callback("glProgramUniform2uiEXT", (GLADapiproc) glad_glProgramUniform2uiEXT, 4, program, location, v0, v1);
    glad_glProgramUniform2uiEXT(program, location, v0, v1);
    _post_call_gles2_callback(NULL, "glProgramUniform2uiEXT", (GLADapiproc) glad_glProgramUniform2uiEXT, 4, program, location, v0, v1);
    
}
PFNGLPROGRAMUNIFORM2UIEXTPROC glad_debug_glProgramUniform2uiEXT = glad_debug_impl_glProgramUniform2uiEXT;
PFNGLPROGRAMUNIFORM2UIVPROC glad_glProgramUniform2uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glProgramUniform2uiv", (GLADapiproc) glad_glProgramUniform2uiv, 4, program, location, count, value);
    glad_glProgramUniform2uiv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform2uiv", (GLADapiproc) glad_glProgramUniform2uiv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2UIVPROC glad_debug_glProgramUniform2uiv = glad_debug_impl_glProgramUniform2uiv;
PFNGLPROGRAMUNIFORM2UIVEXTPROC glad_glProgramUniform2uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform2uivEXT(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glProgramUniform2uivEXT", (GLADapiproc) glad_glProgramUniform2uivEXT, 4, program, location, count, value);
    glad_glProgramUniform2uivEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform2uivEXT", (GLADapiproc) glad_glProgramUniform2uivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM2UIVEXTPROC glad_debug_glProgramUniform2uivEXT = glad_debug_impl_glProgramUniform2uivEXT;
PFNGLPROGRAMUNIFORM3FPROC glad_glProgramUniform3f = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    _pre_call_gles2_callback("glProgramUniform3f", (GLADapiproc) glad_glProgramUniform3f, 5, program, location, v0, v1, v2);
    glad_glProgramUniform3f(program, location, v0, v1, v2);
    _post_call_gles2_callback(NULL, "glProgramUniform3f", (GLADapiproc) glad_glProgramUniform3f, 5, program, location, v0, v1, v2);
    
}
PFNGLPROGRAMUNIFORM3FPROC glad_debug_glProgramUniform3f = glad_debug_impl_glProgramUniform3f;
PFNGLPROGRAMUNIFORM3FEXTPROC glad_glProgramUniform3fEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    _pre_call_gles2_callback("glProgramUniform3fEXT", (GLADapiproc) glad_glProgramUniform3fEXT, 5, program, location, v0, v1, v2);
    glad_glProgramUniform3fEXT(program, location, v0, v1, v2);
    _post_call_gles2_callback(NULL, "glProgramUniform3fEXT", (GLADapiproc) glad_glProgramUniform3fEXT, 5, program, location, v0, v1, v2);
    
}
PFNGLPROGRAMUNIFORM3FEXTPROC glad_debug_glProgramUniform3fEXT = glad_debug_impl_glProgramUniform3fEXT;
PFNGLPROGRAMUNIFORM3FVPROC glad_glProgramUniform3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniform3fv", (GLADapiproc) glad_glProgramUniform3fv, 4, program, location, count, value);
    glad_glProgramUniform3fv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform3fv", (GLADapiproc) glad_glProgramUniform3fv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3FVPROC glad_debug_glProgramUniform3fv = glad_debug_impl_glProgramUniform3fv;
PFNGLPROGRAMUNIFORM3FVEXTPROC glad_glProgramUniform3fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniform3fvEXT", (GLADapiproc) glad_glProgramUniform3fvEXT, 4, program, location, count, value);
    glad_glProgramUniform3fvEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform3fvEXT", (GLADapiproc) glad_glProgramUniform3fvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3FVEXTPROC glad_debug_glProgramUniform3fvEXT = glad_debug_impl_glProgramUniform3fvEXT;
PFNGLPROGRAMUNIFORM3IPROC glad_glProgramUniform3i = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2) {
    _pre_call_gles2_callback("glProgramUniform3i", (GLADapiproc) glad_glProgramUniform3i, 5, program, location, v0, v1, v2);
    glad_glProgramUniform3i(program, location, v0, v1, v2);
    _post_call_gles2_callback(NULL, "glProgramUniform3i", (GLADapiproc) glad_glProgramUniform3i, 5, program, location, v0, v1, v2);
    
}
PFNGLPROGRAMUNIFORM3IPROC glad_debug_glProgramUniform3i = glad_debug_impl_glProgramUniform3i;
PFNGLPROGRAMUNIFORM3I64NVPROC glad_glProgramUniform3i64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3i64NV(GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z) {
    _pre_call_gles2_callback("glProgramUniform3i64NV", (GLADapiproc) glad_glProgramUniform3i64NV, 5, program, location, x, y, z);
    glad_glProgramUniform3i64NV(program, location, x, y, z);
    _post_call_gles2_callback(NULL, "glProgramUniform3i64NV", (GLADapiproc) glad_glProgramUniform3i64NV, 5, program, location, x, y, z);
    
}
PFNGLPROGRAMUNIFORM3I64NVPROC glad_debug_glProgramUniform3i64NV = glad_debug_impl_glProgramUniform3i64NV;
PFNGLPROGRAMUNIFORM3I64VNVPROC glad_glProgramUniform3i64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT * value) {
    _pre_call_gles2_callback("glProgramUniform3i64vNV", (GLADapiproc) glad_glProgramUniform3i64vNV, 4, program, location, count, value);
    glad_glProgramUniform3i64vNV(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform3i64vNV", (GLADapiproc) glad_glProgramUniform3i64vNV, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3I64VNVPROC glad_debug_glProgramUniform3i64vNV = glad_debug_impl_glProgramUniform3i64vNV;
PFNGLPROGRAMUNIFORM3IEXTPROC glad_glProgramUniform3iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2) {
    _pre_call_gles2_callback("glProgramUniform3iEXT", (GLADapiproc) glad_glProgramUniform3iEXT, 5, program, location, v0, v1, v2);
    glad_glProgramUniform3iEXT(program, location, v0, v1, v2);
    _post_call_gles2_callback(NULL, "glProgramUniform3iEXT", (GLADapiproc) glad_glProgramUniform3iEXT, 5, program, location, v0, v1, v2);
    
}
PFNGLPROGRAMUNIFORM3IEXTPROC glad_debug_glProgramUniform3iEXT = glad_debug_impl_glProgramUniform3iEXT;
PFNGLPROGRAMUNIFORM3IVPROC glad_glProgramUniform3iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3iv(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glProgramUniform3iv", (GLADapiproc) glad_glProgramUniform3iv, 4, program, location, count, value);
    glad_glProgramUniform3iv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform3iv", (GLADapiproc) glad_glProgramUniform3iv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3IVPROC glad_debug_glProgramUniform3iv = glad_debug_impl_glProgramUniform3iv;
PFNGLPROGRAMUNIFORM3IVEXTPROC glad_glProgramUniform3ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3ivEXT(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glProgramUniform3ivEXT", (GLADapiproc) glad_glProgramUniform3ivEXT, 4, program, location, count, value);
    glad_glProgramUniform3ivEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform3ivEXT", (GLADapiproc) glad_glProgramUniform3ivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3IVEXTPROC glad_debug_glProgramUniform3ivEXT = glad_debug_impl_glProgramUniform3ivEXT;
PFNGLPROGRAMUNIFORM3UIPROC glad_glProgramUniform3ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2) {
    _pre_call_gles2_callback("glProgramUniform3ui", (GLADapiproc) glad_glProgramUniform3ui, 5, program, location, v0, v1, v2);
    glad_glProgramUniform3ui(program, location, v0, v1, v2);
    _post_call_gles2_callback(NULL, "glProgramUniform3ui", (GLADapiproc) glad_glProgramUniform3ui, 5, program, location, v0, v1, v2);
    
}
PFNGLPROGRAMUNIFORM3UIPROC glad_debug_glProgramUniform3ui = glad_debug_impl_glProgramUniform3ui;
PFNGLPROGRAMUNIFORM3UI64NVPROC glad_glProgramUniform3ui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3ui64NV(GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z) {
    _pre_call_gles2_callback("glProgramUniform3ui64NV", (GLADapiproc) glad_glProgramUniform3ui64NV, 5, program, location, x, y, z);
    glad_glProgramUniform3ui64NV(program, location, x, y, z);
    _post_call_gles2_callback(NULL, "glProgramUniform3ui64NV", (GLADapiproc) glad_glProgramUniform3ui64NV, 5, program, location, x, y, z);
    
}
PFNGLPROGRAMUNIFORM3UI64NVPROC glad_debug_glProgramUniform3ui64NV = glad_debug_impl_glProgramUniform3ui64NV;
PFNGLPROGRAMUNIFORM3UI64VNVPROC glad_glProgramUniform3ui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT * value) {
    _pre_call_gles2_callback("glProgramUniform3ui64vNV", (GLADapiproc) glad_glProgramUniform3ui64vNV, 4, program, location, count, value);
    glad_glProgramUniform3ui64vNV(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform3ui64vNV", (GLADapiproc) glad_glProgramUniform3ui64vNV, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3UI64VNVPROC glad_debug_glProgramUniform3ui64vNV = glad_debug_impl_glProgramUniform3ui64vNV;
PFNGLPROGRAMUNIFORM3UIEXTPROC glad_glProgramUniform3uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2) {
    _pre_call_gles2_callback("glProgramUniform3uiEXT", (GLADapiproc) glad_glProgramUniform3uiEXT, 5, program, location, v0, v1, v2);
    glad_glProgramUniform3uiEXT(program, location, v0, v1, v2);
    _post_call_gles2_callback(NULL, "glProgramUniform3uiEXT", (GLADapiproc) glad_glProgramUniform3uiEXT, 5, program, location, v0, v1, v2);
    
}
PFNGLPROGRAMUNIFORM3UIEXTPROC glad_debug_glProgramUniform3uiEXT = glad_debug_impl_glProgramUniform3uiEXT;
PFNGLPROGRAMUNIFORM3UIVPROC glad_glProgramUniform3uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glProgramUniform3uiv", (GLADapiproc) glad_glProgramUniform3uiv, 4, program, location, count, value);
    glad_glProgramUniform3uiv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform3uiv", (GLADapiproc) glad_glProgramUniform3uiv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3UIVPROC glad_debug_glProgramUniform3uiv = glad_debug_impl_glProgramUniform3uiv;
PFNGLPROGRAMUNIFORM3UIVEXTPROC glad_glProgramUniform3uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform3uivEXT(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glProgramUniform3uivEXT", (GLADapiproc) glad_glProgramUniform3uivEXT, 4, program, location, count, value);
    glad_glProgramUniform3uivEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform3uivEXT", (GLADapiproc) glad_glProgramUniform3uivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM3UIVEXTPROC glad_debug_glProgramUniform3uivEXT = glad_debug_impl_glProgramUniform3uivEXT;
PFNGLPROGRAMUNIFORM4FPROC glad_glProgramUniform4f = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    _pre_call_gles2_callback("glProgramUniform4f", (GLADapiproc) glad_glProgramUniform4f, 6, program, location, v0, v1, v2, v3);
    glad_glProgramUniform4f(program, location, v0, v1, v2, v3);
    _post_call_gles2_callback(NULL, "glProgramUniform4f", (GLADapiproc) glad_glProgramUniform4f, 6, program, location, v0, v1, v2, v3);
    
}
PFNGLPROGRAMUNIFORM4FPROC glad_debug_glProgramUniform4f = glad_debug_impl_glProgramUniform4f;
PFNGLPROGRAMUNIFORM4FEXTPROC glad_glProgramUniform4fEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4fEXT(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    _pre_call_gles2_callback("glProgramUniform4fEXT", (GLADapiproc) glad_glProgramUniform4fEXT, 6, program, location, v0, v1, v2, v3);
    glad_glProgramUniform4fEXT(program, location, v0, v1, v2, v3);
    _post_call_gles2_callback(NULL, "glProgramUniform4fEXT", (GLADapiproc) glad_glProgramUniform4fEXT, 6, program, location, v0, v1, v2, v3);
    
}
PFNGLPROGRAMUNIFORM4FEXTPROC glad_debug_glProgramUniform4fEXT = glad_debug_impl_glProgramUniform4fEXT;
PFNGLPROGRAMUNIFORM4FVPROC glad_glProgramUniform4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniform4fv", (GLADapiproc) glad_glProgramUniform4fv, 4, program, location, count, value);
    glad_glProgramUniform4fv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform4fv", (GLADapiproc) glad_glProgramUniform4fv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4FVPROC glad_debug_glProgramUniform4fv = glad_debug_impl_glProgramUniform4fv;
PFNGLPROGRAMUNIFORM4FVEXTPROC glad_glProgramUniform4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4fvEXT(GLuint program, GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniform4fvEXT", (GLADapiproc) glad_glProgramUniform4fvEXT, 4, program, location, count, value);
    glad_glProgramUniform4fvEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform4fvEXT", (GLADapiproc) glad_glProgramUniform4fvEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4FVEXTPROC glad_debug_glProgramUniform4fvEXT = glad_debug_impl_glProgramUniform4fvEXT;
PFNGLPROGRAMUNIFORM4IPROC glad_glProgramUniform4i = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    _pre_call_gles2_callback("glProgramUniform4i", (GLADapiproc) glad_glProgramUniform4i, 6, program, location, v0, v1, v2, v3);
    glad_glProgramUniform4i(program, location, v0, v1, v2, v3);
    _post_call_gles2_callback(NULL, "glProgramUniform4i", (GLADapiproc) glad_glProgramUniform4i, 6, program, location, v0, v1, v2, v3);
    
}
PFNGLPROGRAMUNIFORM4IPROC glad_debug_glProgramUniform4i = glad_debug_impl_glProgramUniform4i;
PFNGLPROGRAMUNIFORM4I64NVPROC glad_glProgramUniform4i64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4i64NV(GLuint program, GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w) {
    _pre_call_gles2_callback("glProgramUniform4i64NV", (GLADapiproc) glad_glProgramUniform4i64NV, 6, program, location, x, y, z, w);
    glad_glProgramUniform4i64NV(program, location, x, y, z, w);
    _post_call_gles2_callback(NULL, "glProgramUniform4i64NV", (GLADapiproc) glad_glProgramUniform4i64NV, 6, program, location, x, y, z, w);
    
}
PFNGLPROGRAMUNIFORM4I64NVPROC glad_debug_glProgramUniform4i64NV = glad_debug_impl_glProgramUniform4i64NV;
PFNGLPROGRAMUNIFORM4I64VNVPROC glad_glProgramUniform4i64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4i64vNV(GLuint program, GLint location, GLsizei count, const GLint64EXT * value) {
    _pre_call_gles2_callback("glProgramUniform4i64vNV", (GLADapiproc) glad_glProgramUniform4i64vNV, 4, program, location, count, value);
    glad_glProgramUniform4i64vNV(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform4i64vNV", (GLADapiproc) glad_glProgramUniform4i64vNV, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4I64VNVPROC glad_debug_glProgramUniform4i64vNV = glad_debug_impl_glProgramUniform4i64vNV;
PFNGLPROGRAMUNIFORM4IEXTPROC glad_glProgramUniform4iEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4iEXT(GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    _pre_call_gles2_callback("glProgramUniform4iEXT", (GLADapiproc) glad_glProgramUniform4iEXT, 6, program, location, v0, v1, v2, v3);
    glad_glProgramUniform4iEXT(program, location, v0, v1, v2, v3);
    _post_call_gles2_callback(NULL, "glProgramUniform4iEXT", (GLADapiproc) glad_glProgramUniform4iEXT, 6, program, location, v0, v1, v2, v3);
    
}
PFNGLPROGRAMUNIFORM4IEXTPROC glad_debug_glProgramUniform4iEXT = glad_debug_impl_glProgramUniform4iEXT;
PFNGLPROGRAMUNIFORM4IVPROC glad_glProgramUniform4iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4iv(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glProgramUniform4iv", (GLADapiproc) glad_glProgramUniform4iv, 4, program, location, count, value);
    glad_glProgramUniform4iv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform4iv", (GLADapiproc) glad_glProgramUniform4iv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4IVPROC glad_debug_glProgramUniform4iv = glad_debug_impl_glProgramUniform4iv;
PFNGLPROGRAMUNIFORM4IVEXTPROC glad_glProgramUniform4ivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4ivEXT(GLuint program, GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glProgramUniform4ivEXT", (GLADapiproc) glad_glProgramUniform4ivEXT, 4, program, location, count, value);
    glad_glProgramUniform4ivEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform4ivEXT", (GLADapiproc) glad_glProgramUniform4ivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4IVEXTPROC glad_debug_glProgramUniform4ivEXT = glad_debug_impl_glProgramUniform4ivEXT;
PFNGLPROGRAMUNIFORM4UIPROC glad_glProgramUniform4ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    _pre_call_gles2_callback("glProgramUniform4ui", (GLADapiproc) glad_glProgramUniform4ui, 6, program, location, v0, v1, v2, v3);
    glad_glProgramUniform4ui(program, location, v0, v1, v2, v3);
    _post_call_gles2_callback(NULL, "glProgramUniform4ui", (GLADapiproc) glad_glProgramUniform4ui, 6, program, location, v0, v1, v2, v3);
    
}
PFNGLPROGRAMUNIFORM4UIPROC glad_debug_glProgramUniform4ui = glad_debug_impl_glProgramUniform4ui;
PFNGLPROGRAMUNIFORM4UI64NVPROC glad_glProgramUniform4ui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4ui64NV(GLuint program, GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w) {
    _pre_call_gles2_callback("glProgramUniform4ui64NV", (GLADapiproc) glad_glProgramUniform4ui64NV, 6, program, location, x, y, z, w);
    glad_glProgramUniform4ui64NV(program, location, x, y, z, w);
    _post_call_gles2_callback(NULL, "glProgramUniform4ui64NV", (GLADapiproc) glad_glProgramUniform4ui64NV, 6, program, location, x, y, z, w);
    
}
PFNGLPROGRAMUNIFORM4UI64NVPROC glad_debug_glProgramUniform4ui64NV = glad_debug_impl_glProgramUniform4ui64NV;
PFNGLPROGRAMUNIFORM4UI64VNVPROC glad_glProgramUniform4ui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4ui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64EXT * value) {
    _pre_call_gles2_callback("glProgramUniform4ui64vNV", (GLADapiproc) glad_glProgramUniform4ui64vNV, 4, program, location, count, value);
    glad_glProgramUniform4ui64vNV(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform4ui64vNV", (GLADapiproc) glad_glProgramUniform4ui64vNV, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4UI64VNVPROC glad_debug_glProgramUniform4ui64vNV = glad_debug_impl_glProgramUniform4ui64vNV;
PFNGLPROGRAMUNIFORM4UIEXTPROC glad_glProgramUniform4uiEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4uiEXT(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    _pre_call_gles2_callback("glProgramUniform4uiEXT", (GLADapiproc) glad_glProgramUniform4uiEXT, 6, program, location, v0, v1, v2, v3);
    glad_glProgramUniform4uiEXT(program, location, v0, v1, v2, v3);
    _post_call_gles2_callback(NULL, "glProgramUniform4uiEXT", (GLADapiproc) glad_glProgramUniform4uiEXT, 6, program, location, v0, v1, v2, v3);
    
}
PFNGLPROGRAMUNIFORM4UIEXTPROC glad_debug_glProgramUniform4uiEXT = glad_debug_impl_glProgramUniform4uiEXT;
PFNGLPROGRAMUNIFORM4UIVPROC glad_glProgramUniform4uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glProgramUniform4uiv", (GLADapiproc) glad_glProgramUniform4uiv, 4, program, location, count, value);
    glad_glProgramUniform4uiv(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform4uiv", (GLADapiproc) glad_glProgramUniform4uiv, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4UIVPROC glad_debug_glProgramUniform4uiv = glad_debug_impl_glProgramUniform4uiv;
PFNGLPROGRAMUNIFORM4UIVEXTPROC glad_glProgramUniform4uivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniform4uivEXT(GLuint program, GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glProgramUniform4uivEXT", (GLADapiproc) glad_glProgramUniform4uivEXT, 4, program, location, count, value);
    glad_glProgramUniform4uivEXT(program, location, count, value);
    _post_call_gles2_callback(NULL, "glProgramUniform4uivEXT", (GLADapiproc) glad_glProgramUniform4uivEXT, 4, program, location, count, value);
    
}
PFNGLPROGRAMUNIFORM4UIVEXTPROC glad_debug_glProgramUniform4uivEXT = glad_debug_impl_glProgramUniform4uivEXT;
PFNGLPROGRAMUNIFORMHANDLEUI64IMGPROC glad_glProgramUniformHandleui64IMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformHandleui64IMG(GLuint program, GLint location, GLuint64 value) {
    _pre_call_gles2_callback("glProgramUniformHandleui64IMG", (GLADapiproc) glad_glProgramUniformHandleui64IMG, 3, program, location, value);
    glad_glProgramUniformHandleui64IMG(program, location, value);
    _post_call_gles2_callback(NULL, "glProgramUniformHandleui64IMG", (GLADapiproc) glad_glProgramUniformHandleui64IMG, 3, program, location, value);
    
}
PFNGLPROGRAMUNIFORMHANDLEUI64IMGPROC glad_debug_glProgramUniformHandleui64IMG = glad_debug_impl_glProgramUniformHandleui64IMG;
PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC glad_glProgramUniformHandleui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformHandleui64NV(GLuint program, GLint location, GLuint64 value) {
    _pre_call_gles2_callback("glProgramUniformHandleui64NV", (GLADapiproc) glad_glProgramUniformHandleui64NV, 3, program, location, value);
    glad_glProgramUniformHandleui64NV(program, location, value);
    _post_call_gles2_callback(NULL, "glProgramUniformHandleui64NV", (GLADapiproc) glad_glProgramUniformHandleui64NV, 3, program, location, value);
    
}
PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC glad_debug_glProgramUniformHandleui64NV = glad_debug_impl_glProgramUniformHandleui64NV;
PFNGLPROGRAMUNIFORMHANDLEUI64VIMGPROC glad_glProgramUniformHandleui64vIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformHandleui64vIMG(GLuint program, GLint location, GLsizei count, const GLuint64 * values) {
    _pre_call_gles2_callback("glProgramUniformHandleui64vIMG", (GLADapiproc) glad_glProgramUniformHandleui64vIMG, 4, program, location, count, values);
    glad_glProgramUniformHandleui64vIMG(program, location, count, values);
    _post_call_gles2_callback(NULL, "glProgramUniformHandleui64vIMG", (GLADapiproc) glad_glProgramUniformHandleui64vIMG, 4, program, location, count, values);
    
}
PFNGLPROGRAMUNIFORMHANDLEUI64VIMGPROC glad_debug_glProgramUniformHandleui64vIMG = glad_debug_impl_glProgramUniformHandleui64vIMG;
PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC glad_glProgramUniformHandleui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformHandleui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64 * values) {
    _pre_call_gles2_callback("glProgramUniformHandleui64vNV", (GLADapiproc) glad_glProgramUniformHandleui64vNV, 4, program, location, count, values);
    glad_glProgramUniformHandleui64vNV(program, location, count, values);
    _post_call_gles2_callback(NULL, "glProgramUniformHandleui64vNV", (GLADapiproc) glad_glProgramUniformHandleui64vNV, 4, program, location, count, values);
    
}
PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC glad_debug_glProgramUniformHandleui64vNV = glad_debug_impl_glProgramUniformHandleui64vNV;
PFNGLPROGRAMUNIFORMMATRIX2FVPROC glad_glProgramUniformMatrix2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix2fv", (GLADapiproc) glad_glProgramUniformMatrix2fv, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2fv(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix2fv", (GLADapiproc) glad_glProgramUniformMatrix2fv, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2FVPROC glad_debug_glProgramUniformMatrix2fv = glad_debug_impl_glProgramUniformMatrix2fv;
PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC glad_glProgramUniformMatrix2fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2fvEXT(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC glad_debug_glProgramUniformMatrix2fvEXT = glad_debug_impl_glProgramUniformMatrix2fvEXT;
PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC glad_glProgramUniformMatrix2x3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix2x3fv", (GLADapiproc) glad_glProgramUniformMatrix2x3fv, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2x3fv(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix2x3fv", (GLADapiproc) glad_glProgramUniformMatrix2x3fv, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC glad_debug_glProgramUniformMatrix2x3fv = glad_debug_impl_glProgramUniformMatrix2x3fv;
PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC glad_glProgramUniformMatrix2x3fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix2x3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x3fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2x3fvEXT(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix2x3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x3fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC glad_debug_glProgramUniformMatrix2x3fvEXT = glad_debug_impl_glProgramUniformMatrix2x3fvEXT;
PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC glad_glProgramUniformMatrix2x4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix2x4fv", (GLADapiproc) glad_glProgramUniformMatrix2x4fv, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2x4fv(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix2x4fv", (GLADapiproc) glad_glProgramUniformMatrix2x4fv, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC glad_debug_glProgramUniformMatrix2x4fv = glad_debug_impl_glProgramUniformMatrix2x4fv;
PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC glad_glProgramUniformMatrix2x4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix2x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix2x4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x4fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix2x4fvEXT(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix2x4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix2x4fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC glad_debug_glProgramUniformMatrix2x4fvEXT = glad_debug_impl_glProgramUniformMatrix2x4fvEXT;
PFNGLPROGRAMUNIFORMMATRIX3FVPROC glad_glProgramUniformMatrix3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix3fv", (GLADapiproc) glad_glProgramUniformMatrix3fv, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3fv(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix3fv", (GLADapiproc) glad_glProgramUniformMatrix3fv, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3FVPROC glad_debug_glProgramUniformMatrix3fv = glad_debug_impl_glProgramUniformMatrix3fv;
PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC glad_glProgramUniformMatrix3fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3fvEXT(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC glad_debug_glProgramUniformMatrix3fvEXT = glad_debug_impl_glProgramUniformMatrix3fvEXT;
PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC glad_glProgramUniformMatrix3x2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix3x2fv", (GLADapiproc) glad_glProgramUniformMatrix3x2fv, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3x2fv(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix3x2fv", (GLADapiproc) glad_glProgramUniformMatrix3x2fv, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC glad_debug_glProgramUniformMatrix3x2fv = glad_debug_impl_glProgramUniformMatrix3x2fv;
PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC glad_glProgramUniformMatrix3x2fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix3x2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x2fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3x2fvEXT(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix3x2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x2fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC glad_debug_glProgramUniformMatrix3x2fvEXT = glad_debug_impl_glProgramUniformMatrix3x2fvEXT;
PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC glad_glProgramUniformMatrix3x4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3x4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix3x4fv", (GLADapiproc) glad_glProgramUniformMatrix3x4fv, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3x4fv(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix3x4fv", (GLADapiproc) glad_glProgramUniformMatrix3x4fv, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC glad_debug_glProgramUniformMatrix3x4fv = glad_debug_impl_glProgramUniformMatrix3x4fv;
PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC glad_glProgramUniformMatrix3x4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix3x4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix3x4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x4fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix3x4fvEXT(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix3x4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix3x4fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC glad_debug_glProgramUniformMatrix3x4fvEXT = glad_debug_impl_glProgramUniformMatrix3x4fvEXT;
PFNGLPROGRAMUNIFORMMATRIX4FVPROC glad_glProgramUniformMatrix4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix4fv", (GLADapiproc) glad_glProgramUniformMatrix4fv, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4fv(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix4fv", (GLADapiproc) glad_glProgramUniformMatrix4fv, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4FVPROC glad_debug_glProgramUniformMatrix4fv = glad_debug_impl_glProgramUniformMatrix4fv;
PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC glad_glProgramUniformMatrix4fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4fvEXT(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix4fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC glad_debug_glProgramUniformMatrix4fvEXT = glad_debug_impl_glProgramUniformMatrix4fvEXT;
PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC glad_glProgramUniformMatrix4x2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4x2fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix4x2fv", (GLADapiproc) glad_glProgramUniformMatrix4x2fv, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4x2fv(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix4x2fv", (GLADapiproc) glad_glProgramUniformMatrix4x2fv, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC glad_debug_glProgramUniformMatrix4x2fv = glad_debug_impl_glProgramUniformMatrix4x2fv;
PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC glad_glProgramUniformMatrix4x2fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4x2fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix4x2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x2fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4x2fvEXT(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix4x2fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x2fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC glad_debug_glProgramUniformMatrix4x2fvEXT = glad_debug_impl_glProgramUniformMatrix4x2fvEXT;
PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC glad_glProgramUniformMatrix4x3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4x3fv(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix4x3fv", (GLADapiproc) glad_glProgramUniformMatrix4x3fv, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4x3fv(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix4x3fv", (GLADapiproc) glad_glProgramUniformMatrix4x3fv, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC glad_debug_glProgramUniformMatrix4x3fv = glad_debug_impl_glProgramUniformMatrix4x3fv;
PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC glad_glProgramUniformMatrix4x3fvEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glProgramUniformMatrix4x3fvEXT(GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glProgramUniformMatrix4x3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x3fvEXT, 5, program, location, count, transpose, value);
    glad_glProgramUniformMatrix4x3fvEXT(program, location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glProgramUniformMatrix4x3fvEXT", (GLADapiproc) glad_glProgramUniformMatrix4x3fvEXT, 5, program, location, count, transpose, value);
    
}
PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC glad_debug_glProgramUniformMatrix4x3fvEXT = glad_debug_impl_glProgramUniformMatrix4x3fvEXT;
PFNGLPUSHDEBUGGROUPPROC glad_glPushDebugGroup = NULL;
static void GLAD_API_PTR glad_debug_impl_glPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar * message) {
    _pre_call_gles2_callback("glPushDebugGroup", (GLADapiproc) glad_glPushDebugGroup, 4, source, id, length, message);
    glad_glPushDebugGroup(source, id, length, message);
    _post_call_gles2_callback(NULL, "glPushDebugGroup", (GLADapiproc) glad_glPushDebugGroup, 4, source, id, length, message);
    
}
PFNGLPUSHDEBUGGROUPPROC glad_debug_glPushDebugGroup = glad_debug_impl_glPushDebugGroup;
PFNGLPUSHDEBUGGROUPKHRPROC glad_glPushDebugGroupKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glPushDebugGroupKHR(GLenum source, GLuint id, GLsizei length, const GLchar * message) {
    _pre_call_gles2_callback("glPushDebugGroupKHR", (GLADapiproc) glad_glPushDebugGroupKHR, 4, source, id, length, message);
    glad_glPushDebugGroupKHR(source, id, length, message);
    _post_call_gles2_callback(NULL, "glPushDebugGroupKHR", (GLADapiproc) glad_glPushDebugGroupKHR, 4, source, id, length, message);
    
}
PFNGLPUSHDEBUGGROUPKHRPROC glad_debug_glPushDebugGroupKHR = glad_debug_impl_glPushDebugGroupKHR;
PFNGLPUSHGROUPMARKEREXTPROC glad_glPushGroupMarkerEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glPushGroupMarkerEXT(GLsizei length, const GLchar * marker) {
    _pre_call_gles2_callback("glPushGroupMarkerEXT", (GLADapiproc) glad_glPushGroupMarkerEXT, 2, length, marker);
    glad_glPushGroupMarkerEXT(length, marker);
    _post_call_gles2_callback(NULL, "glPushGroupMarkerEXT", (GLADapiproc) glad_glPushGroupMarkerEXT, 2, length, marker);
    
}
PFNGLPUSHGROUPMARKEREXTPROC glad_debug_glPushGroupMarkerEXT = glad_debug_impl_glPushGroupMarkerEXT;
PFNGLQUERYCOUNTEREXTPROC glad_glQueryCounterEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glQueryCounterEXT(GLuint id, GLenum target) {
    _pre_call_gles2_callback("glQueryCounterEXT", (GLADapiproc) glad_glQueryCounterEXT, 2, id, target);
    glad_glQueryCounterEXT(id, target);
    _post_call_gles2_callback(NULL, "glQueryCounterEXT", (GLADapiproc) glad_glQueryCounterEXT, 2, id, target);
    
}
PFNGLQUERYCOUNTEREXTPROC glad_debug_glQueryCounterEXT = glad_debug_impl_glQueryCounterEXT;
PFNGLRASTERSAMPLESEXTPROC glad_glRasterSamplesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glRasterSamplesEXT(GLuint samples, GLboolean fixedsamplelocations) {
    _pre_call_gles2_callback("glRasterSamplesEXT", (GLADapiproc) glad_glRasterSamplesEXT, 2, samples, fixedsamplelocations);
    glad_glRasterSamplesEXT(samples, fixedsamplelocations);
    _post_call_gles2_callback(NULL, "glRasterSamplesEXT", (GLADapiproc) glad_glRasterSamplesEXT, 2, samples, fixedsamplelocations);
    
}
PFNGLRASTERSAMPLESEXTPROC glad_debug_glRasterSamplesEXT = glad_debug_impl_glRasterSamplesEXT;
PFNGLREADBUFFERPROC glad_glReadBuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadBuffer(GLenum src) {
    _pre_call_gles2_callback("glReadBuffer", (GLADapiproc) glad_glReadBuffer, 1, src);
    glad_glReadBuffer(src);
    _post_call_gles2_callback(NULL, "glReadBuffer", (GLADapiproc) glad_glReadBuffer, 1, src);
    
}
PFNGLREADBUFFERPROC glad_debug_glReadBuffer = glad_debug_impl_glReadBuffer;
PFNGLREADBUFFERINDEXEDEXTPROC glad_glReadBufferIndexedEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadBufferIndexedEXT(GLenum src, GLint index) {
    _pre_call_gles2_callback("glReadBufferIndexedEXT", (GLADapiproc) glad_glReadBufferIndexedEXT, 2, src, index);
    glad_glReadBufferIndexedEXT(src, index);
    _post_call_gles2_callback(NULL, "glReadBufferIndexedEXT", (GLADapiproc) glad_glReadBufferIndexedEXT, 2, src, index);
    
}
PFNGLREADBUFFERINDEXEDEXTPROC glad_debug_glReadBufferIndexedEXT = glad_debug_impl_glReadBufferIndexedEXT;
PFNGLREADBUFFERNVPROC glad_glReadBufferNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadBufferNV(GLenum mode) {
    _pre_call_gles2_callback("glReadBufferNV", (GLADapiproc) glad_glReadBufferNV, 1, mode);
    glad_glReadBufferNV(mode);
    _post_call_gles2_callback(NULL, "glReadBufferNV", (GLADapiproc) glad_glReadBufferNV, 1, mode);
    
}
PFNGLREADBUFFERNVPROC glad_debug_glReadBufferNV = glad_debug_impl_glReadBufferNV;
PFNGLREADPIXELSPROC glad_glReadPixels = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void * pixels) {
    _pre_call_gles2_callback("glReadPixels", (GLADapiproc) glad_glReadPixels, 7, x, y, width, height, format, type, pixels);
    glad_glReadPixels(x, y, width, height, format, type, pixels);
    _post_call_gles2_callback(NULL, "glReadPixels", (GLADapiproc) glad_glReadPixels, 7, x, y, width, height, format, type, pixels);
    
}
PFNGLREADPIXELSPROC glad_debug_glReadPixels = glad_debug_impl_glReadPixels;
PFNGLREADNPIXELSPROC glad_glReadnPixels = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void * data) {
    _pre_call_gles2_callback("glReadnPixels", (GLADapiproc) glad_glReadnPixels, 8, x, y, width, height, format, type, bufSize, data);
    glad_glReadnPixels(x, y, width, height, format, type, bufSize, data);
    _post_call_gles2_callback(NULL, "glReadnPixels", (GLADapiproc) glad_glReadnPixels, 8, x, y, width, height, format, type, bufSize, data);
    
}
PFNGLREADNPIXELSPROC glad_debug_glReadnPixels = glad_debug_impl_glReadnPixels;
PFNGLREADNPIXELSEXTPROC glad_glReadnPixelsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadnPixelsEXT(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void * data) {
    _pre_call_gles2_callback("glReadnPixelsEXT", (GLADapiproc) glad_glReadnPixelsEXT, 8, x, y, width, height, format, type, bufSize, data);
    glad_glReadnPixelsEXT(x, y, width, height, format, type, bufSize, data);
    _post_call_gles2_callback(NULL, "glReadnPixelsEXT", (GLADapiproc) glad_glReadnPixelsEXT, 8, x, y, width, height, format, type, bufSize, data);
    
}
PFNGLREADNPIXELSEXTPROC glad_debug_glReadnPixelsEXT = glad_debug_impl_glReadnPixelsEXT;
PFNGLREADNPIXELSKHRPROC glad_glReadnPixelsKHR = NULL;
static void GLAD_API_PTR glad_debug_impl_glReadnPixelsKHR(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void * data) {
    _pre_call_gles2_callback("glReadnPixelsKHR", (GLADapiproc) glad_glReadnPixelsKHR, 8, x, y, width, height, format, type, bufSize, data);
    glad_glReadnPixelsKHR(x, y, width, height, format, type, bufSize, data);
    _post_call_gles2_callback(NULL, "glReadnPixelsKHR", (GLADapiproc) glad_glReadnPixelsKHR, 8, x, y, width, height, format, type, bufSize, data);
    
}
PFNGLREADNPIXELSKHRPROC glad_debug_glReadnPixelsKHR = glad_debug_impl_glReadnPixelsKHR;
PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC glad_glReleaseKeyedMutexWin32EXT = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glReleaseKeyedMutexWin32EXT(GLuint memory, GLuint64 key) {
    GLboolean ret;
    _pre_call_gles2_callback("glReleaseKeyedMutexWin32EXT", (GLADapiproc) glad_glReleaseKeyedMutexWin32EXT, 2, memory, key);
    ret = glad_glReleaseKeyedMutexWin32EXT(memory, key);
    _post_call_gles2_callback((void*) &ret, "glReleaseKeyedMutexWin32EXT", (GLADapiproc) glad_glReleaseKeyedMutexWin32EXT, 2, memory, key);
    return ret;
}
PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC glad_debug_glReleaseKeyedMutexWin32EXT = glad_debug_impl_glReleaseKeyedMutexWin32EXT;
PFNGLRELEASESHADERCOMPILERPROC glad_glReleaseShaderCompiler = NULL;
static void GLAD_API_PTR glad_debug_impl_glReleaseShaderCompiler(void) {
    _pre_call_gles2_callback("glReleaseShaderCompiler", (GLADapiproc) glad_glReleaseShaderCompiler, 0);
    glad_glReleaseShaderCompiler();
    _post_call_gles2_callback(NULL, "glReleaseShaderCompiler", (GLADapiproc) glad_glReleaseShaderCompiler, 0);
    
}
PFNGLRELEASESHADERCOMPILERPROC glad_debug_glReleaseShaderCompiler = glad_debug_impl_glReleaseShaderCompiler;
PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glRenderbufferStorage", (GLADapiproc) glad_glRenderbufferStorage, 4, target, internalformat, width, height);
    glad_glRenderbufferStorage(target, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glRenderbufferStorage", (GLADapiproc) glad_glRenderbufferStorage, 4, target, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEPROC glad_debug_glRenderbufferStorage = glad_debug_impl_glRenderbufferStorage;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_glRenderbufferStorageMultisample = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glRenderbufferStorageMultisample", (GLADapiproc) glad_glRenderbufferStorageMultisample, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glRenderbufferStorageMultisample", (GLADapiproc) glad_glRenderbufferStorageMultisample, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_debug_glRenderbufferStorageMultisample = glad_debug_impl_glRenderbufferStorageMultisample;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC glad_glRenderbufferStorageMultisampleANGLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleANGLE(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glRenderbufferStorageMultisampleANGLE", (GLADapiproc) glad_glRenderbufferStorageMultisampleANGLE, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleANGLE(target, samples, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glRenderbufferStorageMultisampleANGLE", (GLADapiproc) glad_glRenderbufferStorageMultisampleANGLE, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC glad_debug_glRenderbufferStorageMultisampleANGLE = glad_debug_impl_glRenderbufferStorageMultisampleANGLE;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC glad_glRenderbufferStorageMultisampleAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleAPPLE(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glRenderbufferStorageMultisampleAPPLE", (GLADapiproc) glad_glRenderbufferStorageMultisampleAPPLE, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleAPPLE(target, samples, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glRenderbufferStorageMultisampleAPPLE", (GLADapiproc) glad_glRenderbufferStorageMultisampleAPPLE, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC glad_debug_glRenderbufferStorageMultisampleAPPLE = glad_debug_impl_glRenderbufferStorageMultisampleAPPLE;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEADVANCEDAMDPROC glad_glRenderbufferStorageMultisampleAdvancedAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleAdvancedAMD(GLenum target, GLsizei samples, GLsizei storageSamples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glRenderbufferStorageMultisampleAdvancedAMD", (GLADapiproc) glad_glRenderbufferStorageMultisampleAdvancedAMD, 6, target, samples, storageSamples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleAdvancedAMD(target, samples, storageSamples, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glRenderbufferStorageMultisampleAdvancedAMD", (GLADapiproc) glad_glRenderbufferStorageMultisampleAdvancedAMD, 6, target, samples, storageSamples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEADVANCEDAMDPROC glad_debug_glRenderbufferStorageMultisampleAdvancedAMD = glad_debug_impl_glRenderbufferStorageMultisampleAdvancedAMD;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glad_glRenderbufferStorageMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glRenderbufferStorageMultisampleEXT", (GLADapiproc) glad_glRenderbufferStorageMultisampleEXT, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleEXT(target, samples, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glRenderbufferStorageMultisampleEXT", (GLADapiproc) glad_glRenderbufferStorageMultisampleEXT, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glad_debug_glRenderbufferStorageMultisampleEXT = glad_debug_impl_glRenderbufferStorageMultisampleEXT;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC glad_glRenderbufferStorageMultisampleIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleIMG(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glRenderbufferStorageMultisampleIMG", (GLADapiproc) glad_glRenderbufferStorageMultisampleIMG, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleIMG(target, samples, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glRenderbufferStorageMultisampleIMG", (GLADapiproc) glad_glRenderbufferStorageMultisampleIMG, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC glad_debug_glRenderbufferStorageMultisampleIMG = glad_debug_impl_glRenderbufferStorageMultisampleIMG;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC glad_glRenderbufferStorageMultisampleNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glRenderbufferStorageMultisampleNV(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glRenderbufferStorageMultisampleNV", (GLADapiproc) glad_glRenderbufferStorageMultisampleNV, 5, target, samples, internalformat, width, height);
    glad_glRenderbufferStorageMultisampleNV(target, samples, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glRenderbufferStorageMultisampleNV", (GLADapiproc) glad_glRenderbufferStorageMultisampleNV, 5, target, samples, internalformat, width, height);
    
}
PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC glad_debug_glRenderbufferStorageMultisampleNV = glad_debug_impl_glRenderbufferStorageMultisampleNV;
PFNGLRESETMEMORYOBJECTPARAMETERNVPROC glad_glResetMemoryObjectParameterNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glResetMemoryObjectParameterNV(GLuint memory, GLenum pname) {
    _pre_call_gles2_callback("glResetMemoryObjectParameterNV", (GLADapiproc) glad_glResetMemoryObjectParameterNV, 2, memory, pname);
    glad_glResetMemoryObjectParameterNV(memory, pname);
    _post_call_gles2_callback(NULL, "glResetMemoryObjectParameterNV", (GLADapiproc) glad_glResetMemoryObjectParameterNV, 2, memory, pname);
    
}
PFNGLRESETMEMORYOBJECTPARAMETERNVPROC glad_debug_glResetMemoryObjectParameterNV = glad_debug_impl_glResetMemoryObjectParameterNV;
PFNGLRESOLVEDEPTHVALUESNVPROC glad_glResolveDepthValuesNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glResolveDepthValuesNV(void) {
    _pre_call_gles2_callback("glResolveDepthValuesNV", (GLADapiproc) glad_glResolveDepthValuesNV, 0);
    glad_glResolveDepthValuesNV();
    _post_call_gles2_callback(NULL, "glResolveDepthValuesNV", (GLADapiproc) glad_glResolveDepthValuesNV, 0);
    
}
PFNGLRESOLVEDEPTHVALUESNVPROC glad_debug_glResolveDepthValuesNV = glad_debug_impl_glResolveDepthValuesNV;
PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC glad_glResolveMultisampleFramebufferAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glResolveMultisampleFramebufferAPPLE(void) {
    _pre_call_gles2_callback("glResolveMultisampleFramebufferAPPLE", (GLADapiproc) glad_glResolveMultisampleFramebufferAPPLE, 0);
    glad_glResolveMultisampleFramebufferAPPLE();
    _post_call_gles2_callback(NULL, "glResolveMultisampleFramebufferAPPLE", (GLADapiproc) glad_glResolveMultisampleFramebufferAPPLE, 0);
    
}
PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC glad_debug_glResolveMultisampleFramebufferAPPLE = glad_debug_impl_glResolveMultisampleFramebufferAPPLE;
PFNGLRESUMETRANSFORMFEEDBACKPROC glad_glResumeTransformFeedback = NULL;
static void GLAD_API_PTR glad_debug_impl_glResumeTransformFeedback(void) {
    _pre_call_gles2_callback("glResumeTransformFeedback", (GLADapiproc) glad_glResumeTransformFeedback, 0);
    glad_glResumeTransformFeedback();
    _post_call_gles2_callback(NULL, "glResumeTransformFeedback", (GLADapiproc) glad_glResumeTransformFeedback, 0);
    
}
PFNGLRESUMETRANSFORMFEEDBACKPROC glad_debug_glResumeTransformFeedback = glad_debug_impl_glResumeTransformFeedback;
PFNGLSAMPLECOVERAGEPROC glad_glSampleCoverage = NULL;
static void GLAD_API_PTR glad_debug_impl_glSampleCoverage(GLfloat value, GLboolean invert) {
    _pre_call_gles2_callback("glSampleCoverage", (GLADapiproc) glad_glSampleCoverage, 2, value, invert);
    glad_glSampleCoverage(value, invert);
    _post_call_gles2_callback(NULL, "glSampleCoverage", (GLADapiproc) glad_glSampleCoverage, 2, value, invert);
    
}
PFNGLSAMPLECOVERAGEPROC glad_debug_glSampleCoverage = glad_debug_impl_glSampleCoverage;
PFNGLSAMPLEMASKIPROC glad_glSampleMaski = NULL;
static void GLAD_API_PTR glad_debug_impl_glSampleMaski(GLuint maskNumber, GLbitfield mask) {
    _pre_call_gles2_callback("glSampleMaski", (GLADapiproc) glad_glSampleMaski, 2, maskNumber, mask);
    glad_glSampleMaski(maskNumber, mask);
    _post_call_gles2_callback(NULL, "glSampleMaski", (GLADapiproc) glad_glSampleMaski, 2, maskNumber, mask);
    
}
PFNGLSAMPLEMASKIPROC glad_debug_glSampleMaski = glad_debug_impl_glSampleMaski;
PFNGLSAMPLERPARAMETERIIVPROC glad_glSamplerParameterIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint * param) {
    _pre_call_gles2_callback("glSamplerParameterIiv", (GLADapiproc) glad_glSamplerParameterIiv, 3, sampler, pname, param);
    glad_glSamplerParameterIiv(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameterIiv", (GLADapiproc) glad_glSamplerParameterIiv, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIIVPROC glad_debug_glSamplerParameterIiv = glad_debug_impl_glSamplerParameterIiv;
PFNGLSAMPLERPARAMETERIIVEXTPROC glad_glSamplerParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterIivEXT(GLuint sampler, GLenum pname, const GLint * param) {
    _pre_call_gles2_callback("glSamplerParameterIivEXT", (GLADapiproc) glad_glSamplerParameterIivEXT, 3, sampler, pname, param);
    glad_glSamplerParameterIivEXT(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameterIivEXT", (GLADapiproc) glad_glSamplerParameterIivEXT, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIIVEXTPROC glad_debug_glSamplerParameterIivEXT = glad_debug_impl_glSamplerParameterIivEXT;
PFNGLSAMPLERPARAMETERIIVOESPROC glad_glSamplerParameterIivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterIivOES(GLuint sampler, GLenum pname, const GLint * param) {
    _pre_call_gles2_callback("glSamplerParameterIivOES", (GLADapiproc) glad_glSamplerParameterIivOES, 3, sampler, pname, param);
    glad_glSamplerParameterIivOES(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameterIivOES", (GLADapiproc) glad_glSamplerParameterIivOES, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIIVOESPROC glad_debug_glSamplerParameterIivOES = glad_debug_impl_glSamplerParameterIivOES;
PFNGLSAMPLERPARAMETERIUIVPROC glad_glSamplerParameterIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint * param) {
    _pre_call_gles2_callback("glSamplerParameterIuiv", (GLADapiproc) glad_glSamplerParameterIuiv, 3, sampler, pname, param);
    glad_glSamplerParameterIuiv(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameterIuiv", (GLADapiproc) glad_glSamplerParameterIuiv, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIUIVPROC glad_debug_glSamplerParameterIuiv = glad_debug_impl_glSamplerParameterIuiv;
PFNGLSAMPLERPARAMETERIUIVEXTPROC glad_glSamplerParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterIuivEXT(GLuint sampler, GLenum pname, const GLuint * param) {
    _pre_call_gles2_callback("glSamplerParameterIuivEXT", (GLADapiproc) glad_glSamplerParameterIuivEXT, 3, sampler, pname, param);
    glad_glSamplerParameterIuivEXT(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameterIuivEXT", (GLADapiproc) glad_glSamplerParameterIuivEXT, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIUIVEXTPROC glad_debug_glSamplerParameterIuivEXT = glad_debug_impl_glSamplerParameterIuivEXT;
PFNGLSAMPLERPARAMETERIUIVOESPROC glad_glSamplerParameterIuivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterIuivOES(GLuint sampler, GLenum pname, const GLuint * param) {
    _pre_call_gles2_callback("glSamplerParameterIuivOES", (GLADapiproc) glad_glSamplerParameterIuivOES, 3, sampler, pname, param);
    glad_glSamplerParameterIuivOES(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameterIuivOES", (GLADapiproc) glad_glSamplerParameterIuivOES, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIUIVOESPROC glad_debug_glSamplerParameterIuivOES = glad_debug_impl_glSamplerParameterIuivOES;
PFNGLSAMPLERPARAMETERFPROC glad_glSamplerParameterf = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    _pre_call_gles2_callback("glSamplerParameterf", (GLADapiproc) glad_glSamplerParameterf, 3, sampler, pname, param);
    glad_glSamplerParameterf(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameterf", (GLADapiproc) glad_glSamplerParameterf, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERFPROC glad_debug_glSamplerParameterf = glad_debug_impl_glSamplerParameterf;
PFNGLSAMPLERPARAMETERFVPROC glad_glSamplerParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat * param) {
    _pre_call_gles2_callback("glSamplerParameterfv", (GLADapiproc) glad_glSamplerParameterfv, 3, sampler, pname, param);
    glad_glSamplerParameterfv(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameterfv", (GLADapiproc) glad_glSamplerParameterfv, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERFVPROC glad_debug_glSamplerParameterfv = glad_debug_impl_glSamplerParameterfv;
PFNGLSAMPLERPARAMETERIPROC glad_glSamplerParameteri = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    _pre_call_gles2_callback("glSamplerParameteri", (GLADapiproc) glad_glSamplerParameteri, 3, sampler, pname, param);
    glad_glSamplerParameteri(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameteri", (GLADapiproc) glad_glSamplerParameteri, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIPROC glad_debug_glSamplerParameteri = glad_debug_impl_glSamplerParameteri;
PFNGLSAMPLERPARAMETERIVPROC glad_glSamplerParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint * param) {
    _pre_call_gles2_callback("glSamplerParameteriv", (GLADapiproc) glad_glSamplerParameteriv, 3, sampler, pname, param);
    glad_glSamplerParameteriv(sampler, pname, param);
    _post_call_gles2_callback(NULL, "glSamplerParameteriv", (GLADapiproc) glad_glSamplerParameteriv, 3, sampler, pname, param);
    
}
PFNGLSAMPLERPARAMETERIVPROC glad_debug_glSamplerParameteriv = glad_debug_impl_glSamplerParameteriv;
PFNGLSCISSORPROC glad_glScissor = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glScissor", (GLADapiproc) glad_glScissor, 4, x, y, width, height);
    glad_glScissor(x, y, width, height);
    _post_call_gles2_callback(NULL, "glScissor", (GLADapiproc) glad_glScissor, 4, x, y, width, height);
    
}
PFNGLSCISSORPROC glad_debug_glScissor = glad_debug_impl_glScissor;
PFNGLSCISSORARRAYVNVPROC glad_glScissorArrayvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissorArrayvNV(GLuint first, GLsizei count, const GLint * v) {
    _pre_call_gles2_callback("glScissorArrayvNV", (GLADapiproc) glad_glScissorArrayvNV, 3, first, count, v);
    glad_glScissorArrayvNV(first, count, v);
    _post_call_gles2_callback(NULL, "glScissorArrayvNV", (GLADapiproc) glad_glScissorArrayvNV, 3, first, count, v);
    
}
PFNGLSCISSORARRAYVNVPROC glad_debug_glScissorArrayvNV = glad_debug_impl_glScissorArrayvNV;
PFNGLSCISSORARRAYVOESPROC glad_glScissorArrayvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissorArrayvOES(GLuint first, GLsizei count, const GLint * v) {
    _pre_call_gles2_callback("glScissorArrayvOES", (GLADapiproc) glad_glScissorArrayvOES, 3, first, count, v);
    glad_glScissorArrayvOES(first, count, v);
    _post_call_gles2_callback(NULL, "glScissorArrayvOES", (GLADapiproc) glad_glScissorArrayvOES, 3, first, count, v);
    
}
PFNGLSCISSORARRAYVOESPROC glad_debug_glScissorArrayvOES = glad_debug_impl_glScissorArrayvOES;
PFNGLSCISSOREXCLUSIVEARRAYVNVPROC glad_glScissorExclusiveArrayvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissorExclusiveArrayvNV(GLuint first, GLsizei count, const GLint * v) {
    _pre_call_gles2_callback("glScissorExclusiveArrayvNV", (GLADapiproc) glad_glScissorExclusiveArrayvNV, 3, first, count, v);
    glad_glScissorExclusiveArrayvNV(first, count, v);
    _post_call_gles2_callback(NULL, "glScissorExclusiveArrayvNV", (GLADapiproc) glad_glScissorExclusiveArrayvNV, 3, first, count, v);
    
}
PFNGLSCISSOREXCLUSIVEARRAYVNVPROC glad_debug_glScissorExclusiveArrayvNV = glad_debug_impl_glScissorExclusiveArrayvNV;
PFNGLSCISSOREXCLUSIVENVPROC glad_glScissorExclusiveNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissorExclusiveNV(GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glScissorExclusiveNV", (GLADapiproc) glad_glScissorExclusiveNV, 4, x, y, width, height);
    glad_glScissorExclusiveNV(x, y, width, height);
    _post_call_gles2_callback(NULL, "glScissorExclusiveNV", (GLADapiproc) glad_glScissorExclusiveNV, 4, x, y, width, height);
    
}
PFNGLSCISSOREXCLUSIVENVPROC glad_debug_glScissorExclusiveNV = glad_debug_impl_glScissorExclusiveNV;
PFNGLSCISSORINDEXEDNVPROC glad_glScissorIndexedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissorIndexedNV(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glScissorIndexedNV", (GLADapiproc) glad_glScissorIndexedNV, 5, index, left, bottom, width, height);
    glad_glScissorIndexedNV(index, left, bottom, width, height);
    _post_call_gles2_callback(NULL, "glScissorIndexedNV", (GLADapiproc) glad_glScissorIndexedNV, 5, index, left, bottom, width, height);
    
}
PFNGLSCISSORINDEXEDNVPROC glad_debug_glScissorIndexedNV = glad_debug_impl_glScissorIndexedNV;
PFNGLSCISSORINDEXEDOESPROC glad_glScissorIndexedOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissorIndexedOES(GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glScissorIndexedOES", (GLADapiproc) glad_glScissorIndexedOES, 5, index, left, bottom, width, height);
    glad_glScissorIndexedOES(index, left, bottom, width, height);
    _post_call_gles2_callback(NULL, "glScissorIndexedOES", (GLADapiproc) glad_glScissorIndexedOES, 5, index, left, bottom, width, height);
    
}
PFNGLSCISSORINDEXEDOESPROC glad_debug_glScissorIndexedOES = glad_debug_impl_glScissorIndexedOES;
PFNGLSCISSORINDEXEDVNVPROC glad_glScissorIndexedvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissorIndexedvNV(GLuint index, const GLint * v) {
    _pre_call_gles2_callback("glScissorIndexedvNV", (GLADapiproc) glad_glScissorIndexedvNV, 2, index, v);
    glad_glScissorIndexedvNV(index, v);
    _post_call_gles2_callback(NULL, "glScissorIndexedvNV", (GLADapiproc) glad_glScissorIndexedvNV, 2, index, v);
    
}
PFNGLSCISSORINDEXEDVNVPROC glad_debug_glScissorIndexedvNV = glad_debug_impl_glScissorIndexedvNV;
PFNGLSCISSORINDEXEDVOESPROC glad_glScissorIndexedvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glScissorIndexedvOES(GLuint index, const GLint * v) {
    _pre_call_gles2_callback("glScissorIndexedvOES", (GLADapiproc) glad_glScissorIndexedvOES, 2, index, v);
    glad_glScissorIndexedvOES(index, v);
    _post_call_gles2_callback(NULL, "glScissorIndexedvOES", (GLADapiproc) glad_glScissorIndexedvOES, 2, index, v);
    
}
PFNGLSCISSORINDEXEDVOESPROC glad_debug_glScissorIndexedvOES = glad_debug_impl_glScissorIndexedvOES;
PFNGLSELECTPERFMONITORCOUNTERSAMDPROC glad_glSelectPerfMonitorCountersAMD = NULL;
static void GLAD_API_PTR glad_debug_impl_glSelectPerfMonitorCountersAMD(GLuint monitor, GLboolean enable, GLuint group, GLint numCounters, GLuint * counterList) {
    _pre_call_gles2_callback("glSelectPerfMonitorCountersAMD", (GLADapiproc) glad_glSelectPerfMonitorCountersAMD, 5, monitor, enable, group, numCounters, counterList);
    glad_glSelectPerfMonitorCountersAMD(monitor, enable, group, numCounters, counterList);
    _post_call_gles2_callback(NULL, "glSelectPerfMonitorCountersAMD", (GLADapiproc) glad_glSelectPerfMonitorCountersAMD, 5, monitor, enable, group, numCounters, counterList);
    
}
PFNGLSELECTPERFMONITORCOUNTERSAMDPROC glad_debug_glSelectPerfMonitorCountersAMD = glad_debug_impl_glSelectPerfMonitorCountersAMD;
PFNGLSEMAPHOREPARAMETERIVNVPROC glad_glSemaphoreParameterivNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glSemaphoreParameterivNV(GLuint semaphore, GLenum pname, const GLint * params) {
    _pre_call_gles2_callback("glSemaphoreParameterivNV", (GLADapiproc) glad_glSemaphoreParameterivNV, 3, semaphore, pname, params);
    glad_glSemaphoreParameterivNV(semaphore, pname, params);
    _post_call_gles2_callback(NULL, "glSemaphoreParameterivNV", (GLADapiproc) glad_glSemaphoreParameterivNV, 3, semaphore, pname, params);
    
}
PFNGLSEMAPHOREPARAMETERIVNVPROC glad_debug_glSemaphoreParameterivNV = glad_debug_impl_glSemaphoreParameterivNV;
PFNGLSEMAPHOREPARAMETERUI64VEXTPROC glad_glSemaphoreParameterui64vEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glSemaphoreParameterui64vEXT(GLuint semaphore, GLenum pname, const GLuint64 * params) {
    _pre_call_gles2_callback("glSemaphoreParameterui64vEXT", (GLADapiproc) glad_glSemaphoreParameterui64vEXT, 3, semaphore, pname, params);
    glad_glSemaphoreParameterui64vEXT(semaphore, pname, params);
    _post_call_gles2_callback(NULL, "glSemaphoreParameterui64vEXT", (GLADapiproc) glad_glSemaphoreParameterui64vEXT, 3, semaphore, pname, params);
    
}
PFNGLSEMAPHOREPARAMETERUI64VEXTPROC glad_debug_glSemaphoreParameterui64vEXT = glad_debug_impl_glSemaphoreParameterui64vEXT;
PFNGLSETFENCENVPROC glad_glSetFenceNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glSetFenceNV(GLuint fence, GLenum condition) {
    _pre_call_gles2_callback("glSetFenceNV", (GLADapiproc) glad_glSetFenceNV, 2, fence, condition);
    glad_glSetFenceNV(fence, condition);
    _post_call_gles2_callback(NULL, "glSetFenceNV", (GLADapiproc) glad_glSetFenceNV, 2, fence, condition);
    
}
PFNGLSETFENCENVPROC glad_debug_glSetFenceNV = glad_debug_impl_glSetFenceNV;
PFNGLSHADERBINARYPROC glad_glShaderBinary = NULL;
static void GLAD_API_PTR glad_debug_impl_glShaderBinary(GLsizei count, const GLuint * shaders, GLenum binaryFormat, const void * binary, GLsizei length) {
    _pre_call_gles2_callback("glShaderBinary", (GLADapiproc) glad_glShaderBinary, 5, count, shaders, binaryFormat, binary, length);
    glad_glShaderBinary(count, shaders, binaryFormat, binary, length);
    _post_call_gles2_callback(NULL, "glShaderBinary", (GLADapiproc) glad_glShaderBinary, 5, count, shaders, binaryFormat, binary, length);
    
}
PFNGLSHADERBINARYPROC glad_debug_glShaderBinary = glad_debug_impl_glShaderBinary;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
static void GLAD_API_PTR glad_debug_impl_glShaderSource(GLuint shader, GLsizei count, const GLchar *const* string, const GLint * length) {
    _pre_call_gles2_callback("glShaderSource", (GLADapiproc) glad_glShaderSource, 4, shader, count, string, length);
    glad_glShaderSource(shader, count, string, length);
    _post_call_gles2_callback(NULL, "glShaderSource", (GLADapiproc) glad_glShaderSource, 4, shader, count, string, length);
    
}
PFNGLSHADERSOURCEPROC glad_debug_glShaderSource = glad_debug_impl_glShaderSource;
PFNGLSHADINGRATECOMBINEROPSEXTPROC glad_glShadingRateCombinerOpsEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glShadingRateCombinerOpsEXT(GLenum combinerOp0, GLenum combinerOp1) {
    _pre_call_gles2_callback("glShadingRateCombinerOpsEXT", (GLADapiproc) glad_glShadingRateCombinerOpsEXT, 2, combinerOp0, combinerOp1);
    glad_glShadingRateCombinerOpsEXT(combinerOp0, combinerOp1);
    _post_call_gles2_callback(NULL, "glShadingRateCombinerOpsEXT", (GLADapiproc) glad_glShadingRateCombinerOpsEXT, 2, combinerOp0, combinerOp1);
    
}
PFNGLSHADINGRATECOMBINEROPSEXTPROC glad_debug_glShadingRateCombinerOpsEXT = glad_debug_impl_glShadingRateCombinerOpsEXT;
PFNGLSHADINGRATEEXTPROC glad_glShadingRateEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glShadingRateEXT(GLenum rate) {
    _pre_call_gles2_callback("glShadingRateEXT", (GLADapiproc) glad_glShadingRateEXT, 1, rate);
    glad_glShadingRateEXT(rate);
    _post_call_gles2_callback(NULL, "glShadingRateEXT", (GLADapiproc) glad_glShadingRateEXT, 1, rate);
    
}
PFNGLSHADINGRATEEXTPROC glad_debug_glShadingRateEXT = glad_debug_impl_glShadingRateEXT;
PFNGLSHADINGRATEIMAGEBARRIERNVPROC glad_glShadingRateImageBarrierNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glShadingRateImageBarrierNV(GLboolean synchronize) {
    _pre_call_gles2_callback("glShadingRateImageBarrierNV", (GLADapiproc) glad_glShadingRateImageBarrierNV, 1, synchronize);
    glad_glShadingRateImageBarrierNV(synchronize);
    _post_call_gles2_callback(NULL, "glShadingRateImageBarrierNV", (GLADapiproc) glad_glShadingRateImageBarrierNV, 1, synchronize);
    
}
PFNGLSHADINGRATEIMAGEBARRIERNVPROC glad_debug_glShadingRateImageBarrierNV = glad_debug_impl_glShadingRateImageBarrierNV;
PFNGLSHADINGRATEIMAGEPALETTENVPROC glad_glShadingRateImagePaletteNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glShadingRateImagePaletteNV(GLuint viewport, GLuint first, GLsizei count, const GLenum * rates) {
    _pre_call_gles2_callback("glShadingRateImagePaletteNV", (GLADapiproc) glad_glShadingRateImagePaletteNV, 4, viewport, first, count, rates);
    glad_glShadingRateImagePaletteNV(viewport, first, count, rates);
    _post_call_gles2_callback(NULL, "glShadingRateImagePaletteNV", (GLADapiproc) glad_glShadingRateImagePaletteNV, 4, viewport, first, count, rates);
    
}
PFNGLSHADINGRATEIMAGEPALETTENVPROC glad_debug_glShadingRateImagePaletteNV = glad_debug_impl_glShadingRateImagePaletteNV;
PFNGLSHADINGRATEQCOMPROC glad_glShadingRateQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glShadingRateQCOM(GLenum rate) {
    _pre_call_gles2_callback("glShadingRateQCOM", (GLADapiproc) glad_glShadingRateQCOM, 1, rate);
    glad_glShadingRateQCOM(rate);
    _post_call_gles2_callback(NULL, "glShadingRateQCOM", (GLADapiproc) glad_glShadingRateQCOM, 1, rate);
    
}
PFNGLSHADINGRATEQCOMPROC glad_debug_glShadingRateQCOM = glad_debug_impl_glShadingRateQCOM;
PFNGLSHADINGRATESAMPLEORDERCUSTOMNVPROC glad_glShadingRateSampleOrderCustomNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glShadingRateSampleOrderCustomNV(GLenum rate, GLuint samples, const GLint * locations) {
    _pre_call_gles2_callback("glShadingRateSampleOrderCustomNV", (GLADapiproc) glad_glShadingRateSampleOrderCustomNV, 3, rate, samples, locations);
    glad_glShadingRateSampleOrderCustomNV(rate, samples, locations);
    _post_call_gles2_callback(NULL, "glShadingRateSampleOrderCustomNV", (GLADapiproc) glad_glShadingRateSampleOrderCustomNV, 3, rate, samples, locations);
    
}
PFNGLSHADINGRATESAMPLEORDERCUSTOMNVPROC glad_debug_glShadingRateSampleOrderCustomNV = glad_debug_impl_glShadingRateSampleOrderCustomNV;
PFNGLSHADINGRATESAMPLEORDERNVPROC glad_glShadingRateSampleOrderNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glShadingRateSampleOrderNV(GLenum order) {
    _pre_call_gles2_callback("glShadingRateSampleOrderNV", (GLADapiproc) glad_glShadingRateSampleOrderNV, 1, order);
    glad_glShadingRateSampleOrderNV(order);
    _post_call_gles2_callback(NULL, "glShadingRateSampleOrderNV", (GLADapiproc) glad_glShadingRateSampleOrderNV, 1, order);
    
}
PFNGLSHADINGRATESAMPLEORDERNVPROC glad_debug_glShadingRateSampleOrderNV = glad_debug_impl_glShadingRateSampleOrderNV;
PFNGLSIGNALSEMAPHOREEXTPROC glad_glSignalSemaphoreEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glSignalSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers, const GLuint * buffers, GLuint numTextureBarriers, const GLuint * textures, const GLenum * dstLayouts) {
    _pre_call_gles2_callback("glSignalSemaphoreEXT", (GLADapiproc) glad_glSignalSemaphoreEXT, 6, semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, dstLayouts);
    glad_glSignalSemaphoreEXT(semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, dstLayouts);
    _post_call_gles2_callback(NULL, "glSignalSemaphoreEXT", (GLADapiproc) glad_glSignalSemaphoreEXT, 6, semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, dstLayouts);
    
}
PFNGLSIGNALSEMAPHOREEXTPROC glad_debug_glSignalSemaphoreEXT = glad_debug_impl_glSignalSemaphoreEXT;
PFNGLSIGNALVKFENCENVPROC glad_glSignalVkFenceNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glSignalVkFenceNV(GLuint64 vkFence) {
    _pre_call_gles2_callback("glSignalVkFenceNV", (GLADapiproc) glad_glSignalVkFenceNV, 1, vkFence);
    glad_glSignalVkFenceNV(vkFence);
    _post_call_gles2_callback(NULL, "glSignalVkFenceNV", (GLADapiproc) glad_glSignalVkFenceNV, 1, vkFence);
    
}
PFNGLSIGNALVKFENCENVPROC glad_debug_glSignalVkFenceNV = glad_debug_impl_glSignalVkFenceNV;
PFNGLSIGNALVKSEMAPHORENVPROC glad_glSignalVkSemaphoreNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glSignalVkSemaphoreNV(GLuint64 vkSemaphore) {
    _pre_call_gles2_callback("glSignalVkSemaphoreNV", (GLADapiproc) glad_glSignalVkSemaphoreNV, 1, vkSemaphore);
    glad_glSignalVkSemaphoreNV(vkSemaphore);
    _post_call_gles2_callback(NULL, "glSignalVkSemaphoreNV", (GLADapiproc) glad_glSignalVkSemaphoreNV, 1, vkSemaphore);
    
}
PFNGLSIGNALVKSEMAPHORENVPROC glad_debug_glSignalVkSemaphoreNV = glad_debug_impl_glSignalVkSemaphoreNV;
PFNGLSTARTTILINGQCOMPROC glad_glStartTilingQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glStartTilingQCOM(GLuint x, GLuint y, GLuint width, GLuint height, GLbitfield preserveMask) {
    _pre_call_gles2_callback("glStartTilingQCOM", (GLADapiproc) glad_glStartTilingQCOM, 5, x, y, width, height, preserveMask);
    glad_glStartTilingQCOM(x, y, width, height, preserveMask);
    _post_call_gles2_callback(NULL, "glStartTilingQCOM", (GLADapiproc) glad_glStartTilingQCOM, 5, x, y, width, height, preserveMask);
    
}
PFNGLSTARTTILINGQCOMPROC glad_debug_glStartTilingQCOM = glad_debug_impl_glStartTilingQCOM;
PFNGLSTENCILFILLPATHINSTANCEDNVPROC glad_glStencilFillPathInstancedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilFillPathInstancedNV(GLsizei numPaths, GLenum pathNameType, const void * paths, GLuint pathBase, GLenum fillMode, GLuint mask, GLenum transformType, const GLfloat * transformValues) {
    _pre_call_gles2_callback("glStencilFillPathInstancedNV", (GLADapiproc) glad_glStencilFillPathInstancedNV, 8, numPaths, pathNameType, paths, pathBase, fillMode, mask, transformType, transformValues);
    glad_glStencilFillPathInstancedNV(numPaths, pathNameType, paths, pathBase, fillMode, mask, transformType, transformValues);
    _post_call_gles2_callback(NULL, "glStencilFillPathInstancedNV", (GLADapiproc) glad_glStencilFillPathInstancedNV, 8, numPaths, pathNameType, paths, pathBase, fillMode, mask, transformType, transformValues);
    
}
PFNGLSTENCILFILLPATHINSTANCEDNVPROC glad_debug_glStencilFillPathInstancedNV = glad_debug_impl_glStencilFillPathInstancedNV;
PFNGLSTENCILFILLPATHNVPROC glad_glStencilFillPathNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilFillPathNV(GLuint path, GLenum fillMode, GLuint mask) {
    _pre_call_gles2_callback("glStencilFillPathNV", (GLADapiproc) glad_glStencilFillPathNV, 3, path, fillMode, mask);
    glad_glStencilFillPathNV(path, fillMode, mask);
    _post_call_gles2_callback(NULL, "glStencilFillPathNV", (GLADapiproc) glad_glStencilFillPathNV, 3, path, fillMode, mask);
    
}
PFNGLSTENCILFILLPATHNVPROC glad_debug_glStencilFillPathNV = glad_debug_impl_glStencilFillPathNV;
PFNGLSTENCILFUNCPROC glad_glStencilFunc = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    _pre_call_gles2_callback("glStencilFunc", (GLADapiproc) glad_glStencilFunc, 3, func, ref, mask);
    glad_glStencilFunc(func, ref, mask);
    _post_call_gles2_callback(NULL, "glStencilFunc", (GLADapiproc) glad_glStencilFunc, 3, func, ref, mask);
    
}
PFNGLSTENCILFUNCPROC glad_debug_glStencilFunc = glad_debug_impl_glStencilFunc;
PFNGLSTENCILFUNCSEPARATEPROC glad_glStencilFuncSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    _pre_call_gles2_callback("glStencilFuncSeparate", (GLADapiproc) glad_glStencilFuncSeparate, 4, face, func, ref, mask);
    glad_glStencilFuncSeparate(face, func, ref, mask);
    _post_call_gles2_callback(NULL, "glStencilFuncSeparate", (GLADapiproc) glad_glStencilFuncSeparate, 4, face, func, ref, mask);
    
}
PFNGLSTENCILFUNCSEPARATEPROC glad_debug_glStencilFuncSeparate = glad_debug_impl_glStencilFuncSeparate;
PFNGLSTENCILMASKPROC glad_glStencilMask = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilMask(GLuint mask) {
    _pre_call_gles2_callback("glStencilMask", (GLADapiproc) glad_glStencilMask, 1, mask);
    glad_glStencilMask(mask);
    _post_call_gles2_callback(NULL, "glStencilMask", (GLADapiproc) glad_glStencilMask, 1, mask);
    
}
PFNGLSTENCILMASKPROC glad_debug_glStencilMask = glad_debug_impl_glStencilMask;
PFNGLSTENCILMASKSEPARATEPROC glad_glStencilMaskSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilMaskSeparate(GLenum face, GLuint mask) {
    _pre_call_gles2_callback("glStencilMaskSeparate", (GLADapiproc) glad_glStencilMaskSeparate, 2, face, mask);
    glad_glStencilMaskSeparate(face, mask);
    _post_call_gles2_callback(NULL, "glStencilMaskSeparate", (GLADapiproc) glad_glStencilMaskSeparate, 2, face, mask);
    
}
PFNGLSTENCILMASKSEPARATEPROC glad_debug_glStencilMaskSeparate = glad_debug_impl_glStencilMaskSeparate;
PFNGLSTENCILOPPROC glad_glStencilOp = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    _pre_call_gles2_callback("glStencilOp", (GLADapiproc) glad_glStencilOp, 3, fail, zfail, zpass);
    glad_glStencilOp(fail, zfail, zpass);
    _post_call_gles2_callback(NULL, "glStencilOp", (GLADapiproc) glad_glStencilOp, 3, fail, zfail, zpass);
    
}
PFNGLSTENCILOPPROC glad_debug_glStencilOp = glad_debug_impl_glStencilOp;
PFNGLSTENCILOPSEPARATEPROC glad_glStencilOpSeparate = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    _pre_call_gles2_callback("glStencilOpSeparate", (GLADapiproc) glad_glStencilOpSeparate, 4, face, sfail, dpfail, dppass);
    glad_glStencilOpSeparate(face, sfail, dpfail, dppass);
    _post_call_gles2_callback(NULL, "glStencilOpSeparate", (GLADapiproc) glad_glStencilOpSeparate, 4, face, sfail, dpfail, dppass);
    
}
PFNGLSTENCILOPSEPARATEPROC glad_debug_glStencilOpSeparate = glad_debug_impl_glStencilOpSeparate;
PFNGLSTENCILSTROKEPATHINSTANCEDNVPROC glad_glStencilStrokePathInstancedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilStrokePathInstancedNV(GLsizei numPaths, GLenum pathNameType, const void * paths, GLuint pathBase, GLint reference, GLuint mask, GLenum transformType, const GLfloat * transformValues) {
    _pre_call_gles2_callback("glStencilStrokePathInstancedNV", (GLADapiproc) glad_glStencilStrokePathInstancedNV, 8, numPaths, pathNameType, paths, pathBase, reference, mask, transformType, transformValues);
    glad_glStencilStrokePathInstancedNV(numPaths, pathNameType, paths, pathBase, reference, mask, transformType, transformValues);
    _post_call_gles2_callback(NULL, "glStencilStrokePathInstancedNV", (GLADapiproc) glad_glStencilStrokePathInstancedNV, 8, numPaths, pathNameType, paths, pathBase, reference, mask, transformType, transformValues);
    
}
PFNGLSTENCILSTROKEPATHINSTANCEDNVPROC glad_debug_glStencilStrokePathInstancedNV = glad_debug_impl_glStencilStrokePathInstancedNV;
PFNGLSTENCILSTROKEPATHNVPROC glad_glStencilStrokePathNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilStrokePathNV(GLuint path, GLint reference, GLuint mask) {
    _pre_call_gles2_callback("glStencilStrokePathNV", (GLADapiproc) glad_glStencilStrokePathNV, 3, path, reference, mask);
    glad_glStencilStrokePathNV(path, reference, mask);
    _post_call_gles2_callback(NULL, "glStencilStrokePathNV", (GLADapiproc) glad_glStencilStrokePathNV, 3, path, reference, mask);
    
}
PFNGLSTENCILSTROKEPATHNVPROC glad_debug_glStencilStrokePathNV = glad_debug_impl_glStencilStrokePathNV;
PFNGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC glad_glStencilThenCoverFillPathInstancedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilThenCoverFillPathInstancedNV(GLsizei numPaths, GLenum pathNameType, const void * paths, GLuint pathBase, GLenum fillMode, GLuint mask, GLenum coverMode, GLenum transformType, const GLfloat * transformValues) {
    _pre_call_gles2_callback("glStencilThenCoverFillPathInstancedNV", (GLADapiproc) glad_glStencilThenCoverFillPathInstancedNV, 9, numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode, transformType, transformValues);
    glad_glStencilThenCoverFillPathInstancedNV(numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode, transformType, transformValues);
    _post_call_gles2_callback(NULL, "glStencilThenCoverFillPathInstancedNV", (GLADapiproc) glad_glStencilThenCoverFillPathInstancedNV, 9, numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode, transformType, transformValues);
    
}
PFNGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC glad_debug_glStencilThenCoverFillPathInstancedNV = glad_debug_impl_glStencilThenCoverFillPathInstancedNV;
PFNGLSTENCILTHENCOVERFILLPATHNVPROC glad_glStencilThenCoverFillPathNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilThenCoverFillPathNV(GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode) {
    _pre_call_gles2_callback("glStencilThenCoverFillPathNV", (GLADapiproc) glad_glStencilThenCoverFillPathNV, 4, path, fillMode, mask, coverMode);
    glad_glStencilThenCoverFillPathNV(path, fillMode, mask, coverMode);
    _post_call_gles2_callback(NULL, "glStencilThenCoverFillPathNV", (GLADapiproc) glad_glStencilThenCoverFillPathNV, 4, path, fillMode, mask, coverMode);
    
}
PFNGLSTENCILTHENCOVERFILLPATHNVPROC glad_debug_glStencilThenCoverFillPathNV = glad_debug_impl_glStencilThenCoverFillPathNV;
PFNGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC glad_glStencilThenCoverStrokePathInstancedNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilThenCoverStrokePathInstancedNV(GLsizei numPaths, GLenum pathNameType, const void * paths, GLuint pathBase, GLint reference, GLuint mask, GLenum coverMode, GLenum transformType, const GLfloat * transformValues) {
    _pre_call_gles2_callback("glStencilThenCoverStrokePathInstancedNV", (GLADapiproc) glad_glStencilThenCoverStrokePathInstancedNV, 9, numPaths, pathNameType, paths, pathBase, reference, mask, coverMode, transformType, transformValues);
    glad_glStencilThenCoverStrokePathInstancedNV(numPaths, pathNameType, paths, pathBase, reference, mask, coverMode, transformType, transformValues);
    _post_call_gles2_callback(NULL, "glStencilThenCoverStrokePathInstancedNV", (GLADapiproc) glad_glStencilThenCoverStrokePathInstancedNV, 9, numPaths, pathNameType, paths, pathBase, reference, mask, coverMode, transformType, transformValues);
    
}
PFNGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC glad_debug_glStencilThenCoverStrokePathInstancedNV = glad_debug_impl_glStencilThenCoverStrokePathInstancedNV;
PFNGLSTENCILTHENCOVERSTROKEPATHNVPROC glad_glStencilThenCoverStrokePathNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glStencilThenCoverStrokePathNV(GLuint path, GLint reference, GLuint mask, GLenum coverMode) {
    _pre_call_gles2_callback("glStencilThenCoverStrokePathNV", (GLADapiproc) glad_glStencilThenCoverStrokePathNV, 4, path, reference, mask, coverMode);
    glad_glStencilThenCoverStrokePathNV(path, reference, mask, coverMode);
    _post_call_gles2_callback(NULL, "glStencilThenCoverStrokePathNV", (GLADapiproc) glad_glStencilThenCoverStrokePathNV, 4, path, reference, mask, coverMode);
    
}
PFNGLSTENCILTHENCOVERSTROKEPATHNVPROC glad_debug_glStencilThenCoverStrokePathNV = glad_debug_impl_glStencilThenCoverStrokePathNV;
PFNGLSUBPIXELPRECISIONBIASNVPROC glad_glSubpixelPrecisionBiasNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glSubpixelPrecisionBiasNV(GLuint xbits, GLuint ybits) {
    _pre_call_gles2_callback("glSubpixelPrecisionBiasNV", (GLADapiproc) glad_glSubpixelPrecisionBiasNV, 2, xbits, ybits);
    glad_glSubpixelPrecisionBiasNV(xbits, ybits);
    _post_call_gles2_callback(NULL, "glSubpixelPrecisionBiasNV", (GLADapiproc) glad_glSubpixelPrecisionBiasNV, 2, xbits, ybits);
    
}
PFNGLSUBPIXELPRECISIONBIASNVPROC glad_debug_glSubpixelPrecisionBiasNV = glad_debug_impl_glSubpixelPrecisionBiasNV;
PFNGLTESTFENCENVPROC glad_glTestFenceNV = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glTestFenceNV(GLuint fence) {
    GLboolean ret;
    _pre_call_gles2_callback("glTestFenceNV", (GLADapiproc) glad_glTestFenceNV, 1, fence);
    ret = glad_glTestFenceNV(fence);
    _post_call_gles2_callback((void*) &ret, "glTestFenceNV", (GLADapiproc) glad_glTestFenceNV, 1, fence);
    return ret;
}
PFNGLTESTFENCENVPROC glad_debug_glTestFenceNV = glad_debug_impl_glTestFenceNV;
PFNGLTEXATTACHMEMORYNVPROC glad_glTexAttachMemoryNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexAttachMemoryNV(GLenum target, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTexAttachMemoryNV", (GLADapiproc) glad_glTexAttachMemoryNV, 3, target, memory, offset);
    glad_glTexAttachMemoryNV(target, memory, offset);
    _post_call_gles2_callback(NULL, "glTexAttachMemoryNV", (GLADapiproc) glad_glTexAttachMemoryNV, 3, target, memory, offset);
    
}
PFNGLTEXATTACHMEMORYNVPROC glad_debug_glTexAttachMemoryNV = glad_debug_impl_glTexAttachMemoryNV;
PFNGLTEXBUFFERPROC glad_glTexBuffer = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexBuffer(GLenum target, GLenum internalformat, GLuint buffer) {
    _pre_call_gles2_callback("glTexBuffer", (GLADapiproc) glad_glTexBuffer, 3, target, internalformat, buffer);
    glad_glTexBuffer(target, internalformat, buffer);
    _post_call_gles2_callback(NULL, "glTexBuffer", (GLADapiproc) glad_glTexBuffer, 3, target, internalformat, buffer);
    
}
PFNGLTEXBUFFERPROC glad_debug_glTexBuffer = glad_debug_impl_glTexBuffer;
PFNGLTEXBUFFEREXTPROC glad_glTexBufferEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexBufferEXT(GLenum target, GLenum internalformat, GLuint buffer) {
    _pre_call_gles2_callback("glTexBufferEXT", (GLADapiproc) glad_glTexBufferEXT, 3, target, internalformat, buffer);
    glad_glTexBufferEXT(target, internalformat, buffer);
    _post_call_gles2_callback(NULL, "glTexBufferEXT", (GLADapiproc) glad_glTexBufferEXT, 3, target, internalformat, buffer);
    
}
PFNGLTEXBUFFEREXTPROC glad_debug_glTexBufferEXT = glad_debug_impl_glTexBufferEXT;
PFNGLTEXBUFFEROESPROC glad_glTexBufferOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexBufferOES(GLenum target, GLenum internalformat, GLuint buffer) {
    _pre_call_gles2_callback("glTexBufferOES", (GLADapiproc) glad_glTexBufferOES, 3, target, internalformat, buffer);
    glad_glTexBufferOES(target, internalformat, buffer);
    _post_call_gles2_callback(NULL, "glTexBufferOES", (GLADapiproc) glad_glTexBufferOES, 3, target, internalformat, buffer);
    
}
PFNGLTEXBUFFEROESPROC glad_debug_glTexBufferOES = glad_debug_impl_glTexBufferOES;
PFNGLTEXBUFFERRANGEPROC glad_glTexBufferRange = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexBufferRange(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    _pre_call_gles2_callback("glTexBufferRange", (GLADapiproc) glad_glTexBufferRange, 5, target, internalformat, buffer, offset, size);
    glad_glTexBufferRange(target, internalformat, buffer, offset, size);
    _post_call_gles2_callback(NULL, "glTexBufferRange", (GLADapiproc) glad_glTexBufferRange, 5, target, internalformat, buffer, offset, size);
    
}
PFNGLTEXBUFFERRANGEPROC glad_debug_glTexBufferRange = glad_debug_impl_glTexBufferRange;
PFNGLTEXBUFFERRANGEEXTPROC glad_glTexBufferRangeEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexBufferRangeEXT(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    _pre_call_gles2_callback("glTexBufferRangeEXT", (GLADapiproc) glad_glTexBufferRangeEXT, 5, target, internalformat, buffer, offset, size);
    glad_glTexBufferRangeEXT(target, internalformat, buffer, offset, size);
    _post_call_gles2_callback(NULL, "glTexBufferRangeEXT", (GLADapiproc) glad_glTexBufferRangeEXT, 5, target, internalformat, buffer, offset, size);
    
}
PFNGLTEXBUFFERRANGEEXTPROC glad_debug_glTexBufferRangeEXT = glad_debug_impl_glTexBufferRangeEXT;
PFNGLTEXBUFFERRANGEOESPROC glad_glTexBufferRangeOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexBufferRangeOES(GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    _pre_call_gles2_callback("glTexBufferRangeOES", (GLADapiproc) glad_glTexBufferRangeOES, 5, target, internalformat, buffer, offset, size);
    glad_glTexBufferRangeOES(target, internalformat, buffer, offset, size);
    _post_call_gles2_callback(NULL, "glTexBufferRangeOES", (GLADapiproc) glad_glTexBufferRangeOES, 5, target, internalformat, buffer, offset, size);
    
}
PFNGLTEXBUFFERRANGEOESPROC glad_debug_glTexBufferRangeOES = glad_debug_impl_glTexBufferRangeOES;
PFNGLTEXESTIMATEMOTIONQCOMPROC glad_glTexEstimateMotionQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEstimateMotionQCOM(GLuint ref, GLuint target, GLuint output) {
    _pre_call_gles2_callback("glTexEstimateMotionQCOM", (GLADapiproc) glad_glTexEstimateMotionQCOM, 3, ref, target, output);
    glad_glTexEstimateMotionQCOM(ref, target, output);
    _post_call_gles2_callback(NULL, "glTexEstimateMotionQCOM", (GLADapiproc) glad_glTexEstimateMotionQCOM, 3, ref, target, output);
    
}
PFNGLTEXESTIMATEMOTIONQCOMPROC glad_debug_glTexEstimateMotionQCOM = glad_debug_impl_glTexEstimateMotionQCOM;
PFNGLTEXESTIMATEMOTIONREGIONSQCOMPROC glad_glTexEstimateMotionRegionsQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexEstimateMotionRegionsQCOM(GLuint ref, GLuint target, GLuint output, GLuint mask) {
    _pre_call_gles2_callback("glTexEstimateMotionRegionsQCOM", (GLADapiproc) glad_glTexEstimateMotionRegionsQCOM, 4, ref, target, output, mask);
    glad_glTexEstimateMotionRegionsQCOM(ref, target, output, mask);
    _post_call_gles2_callback(NULL, "glTexEstimateMotionRegionsQCOM", (GLADapiproc) glad_glTexEstimateMotionRegionsQCOM, 4, ref, target, output, mask);
    
}
PFNGLTEXESTIMATEMOTIONREGIONSQCOMPROC glad_debug_glTexEstimateMotionRegionsQCOM = glad_debug_impl_glTexEstimateMotionRegionsQCOM;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gles2_callback("glTexImage2D", (GLADapiproc) glad_glTexImage2D, 9, target, level, internalformat, width, height, border, format, type, pixels);
    glad_glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    _post_call_gles2_callback(NULL, "glTexImage2D", (GLADapiproc) glad_glTexImage2D, 9, target, level, internalformat, width, height, border, format, type, pixels);
    
}
PFNGLTEXIMAGE2DPROC glad_debug_glTexImage2D = glad_debug_impl_glTexImage2D;
PFNGLTEXIMAGE3DPROC glad_glTexImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gles2_callback("glTexImage3D", (GLADapiproc) glad_glTexImage3D, 10, target, level, internalformat, width, height, depth, border, format, type, pixels);
    glad_glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
    _post_call_gles2_callback(NULL, "glTexImage3D", (GLADapiproc) glad_glTexImage3D, 10, target, level, internalformat, width, height, depth, border, format, type, pixels);
    
}
PFNGLTEXIMAGE3DPROC glad_debug_glTexImage3D = glad_debug_impl_glTexImage3D;
PFNGLTEXIMAGE3DOESPROC glad_glTexImage3DOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexImage3DOES(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gles2_callback("glTexImage3DOES", (GLADapiproc) glad_glTexImage3DOES, 10, target, level, internalformat, width, height, depth, border, format, type, pixels);
    glad_glTexImage3DOES(target, level, internalformat, width, height, depth, border, format, type, pixels);
    _post_call_gles2_callback(NULL, "glTexImage3DOES", (GLADapiproc) glad_glTexImage3DOES, 10, target, level, internalformat, width, height, depth, border, format, type, pixels);
    
}
PFNGLTEXIMAGE3DOESPROC glad_debug_glTexImage3DOES = glad_debug_impl_glTexImage3DOES;
PFNGLTEXPAGECOMMITMENTEXTPROC glad_glTexPageCommitmentEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexPageCommitmentEXT(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit) {
    _pre_call_gles2_callback("glTexPageCommitmentEXT", (GLADapiproc) glad_glTexPageCommitmentEXT, 9, target, level, xoffset, yoffset, zoffset, width, height, depth, commit);
    glad_glTexPageCommitmentEXT(target, level, xoffset, yoffset, zoffset, width, height, depth, commit);
    _post_call_gles2_callback(NULL, "glTexPageCommitmentEXT", (GLADapiproc) glad_glTexPageCommitmentEXT, 9, target, level, xoffset, yoffset, zoffset, width, height, depth, commit);
    
}
PFNGLTEXPAGECOMMITMENTEXTPROC glad_debug_glTexPageCommitmentEXT = glad_debug_impl_glTexPageCommitmentEXT;
PFNGLTEXPAGECOMMITMENTMEMNVPROC glad_glTexPageCommitmentMemNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexPageCommitmentMemNV(GLenum target, GLint layer, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset, GLboolean commit) {
    _pre_call_gles2_callback("glTexPageCommitmentMemNV", (GLADapiproc) glad_glTexPageCommitmentMemNV, 12, target, layer, level, xoffset, yoffset, zoffset, width, height, depth, memory, offset, commit);
    glad_glTexPageCommitmentMemNV(target, layer, level, xoffset, yoffset, zoffset, width, height, depth, memory, offset, commit);
    _post_call_gles2_callback(NULL, "glTexPageCommitmentMemNV", (GLADapiproc) glad_glTexPageCommitmentMemNV, 12, target, layer, level, xoffset, yoffset, zoffset, width, height, depth, memory, offset, commit);
    
}
PFNGLTEXPAGECOMMITMENTMEMNVPROC glad_debug_glTexPageCommitmentMemNV = glad_debug_impl_glTexPageCommitmentMemNV;
PFNGLTEXPARAMETERIIVPROC glad_glTexParameterIiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIiv(GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gles2_callback("glTexParameterIiv", (GLADapiproc) glad_glTexParameterIiv, 3, target, pname, params);
    glad_glTexParameterIiv(target, pname, params);
    _post_call_gles2_callback(NULL, "glTexParameterIiv", (GLADapiproc) glad_glTexParameterIiv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIIVPROC glad_debug_glTexParameterIiv = glad_debug_impl_glTexParameterIiv;
PFNGLTEXPARAMETERIIVEXTPROC glad_glTexParameterIivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIivEXT(GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gles2_callback("glTexParameterIivEXT", (GLADapiproc) glad_glTexParameterIivEXT, 3, target, pname, params);
    glad_glTexParameterIivEXT(target, pname, params);
    _post_call_gles2_callback(NULL, "glTexParameterIivEXT", (GLADapiproc) glad_glTexParameterIivEXT, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIIVEXTPROC glad_debug_glTexParameterIivEXT = glad_debug_impl_glTexParameterIivEXT;
PFNGLTEXPARAMETERIIVOESPROC glad_glTexParameterIivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIivOES(GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gles2_callback("glTexParameterIivOES", (GLADapiproc) glad_glTexParameterIivOES, 3, target, pname, params);
    glad_glTexParameterIivOES(target, pname, params);
    _post_call_gles2_callback(NULL, "glTexParameterIivOES", (GLADapiproc) glad_glTexParameterIivOES, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIIVOESPROC glad_debug_glTexParameterIivOES = glad_debug_impl_glTexParameterIivOES;
PFNGLTEXPARAMETERIUIVPROC glad_glTexParameterIuiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIuiv(GLenum target, GLenum pname, const GLuint * params) {
    _pre_call_gles2_callback("glTexParameterIuiv", (GLADapiproc) glad_glTexParameterIuiv, 3, target, pname, params);
    glad_glTexParameterIuiv(target, pname, params);
    _post_call_gles2_callback(NULL, "glTexParameterIuiv", (GLADapiproc) glad_glTexParameterIuiv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIUIVPROC glad_debug_glTexParameterIuiv = glad_debug_impl_glTexParameterIuiv;
PFNGLTEXPARAMETERIUIVEXTPROC glad_glTexParameterIuivEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIuivEXT(GLenum target, GLenum pname, const GLuint * params) {
    _pre_call_gles2_callback("glTexParameterIuivEXT", (GLADapiproc) glad_glTexParameterIuivEXT, 3, target, pname, params);
    glad_glTexParameterIuivEXT(target, pname, params);
    _post_call_gles2_callback(NULL, "glTexParameterIuivEXT", (GLADapiproc) glad_glTexParameterIuivEXT, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIUIVEXTPROC glad_debug_glTexParameterIuivEXT = glad_debug_impl_glTexParameterIuivEXT;
PFNGLTEXPARAMETERIUIVOESPROC glad_glTexParameterIuivOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterIuivOES(GLenum target, GLenum pname, const GLuint * params) {
    _pre_call_gles2_callback("glTexParameterIuivOES", (GLADapiproc) glad_glTexParameterIuivOES, 3, target, pname, params);
    glad_glTexParameterIuivOES(target, pname, params);
    _post_call_gles2_callback(NULL, "glTexParameterIuivOES", (GLADapiproc) glad_glTexParameterIuivOES, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIUIVOESPROC glad_debug_glTexParameterIuivOES = glad_debug_impl_glTexParameterIuivOES;
PFNGLTEXPARAMETERFPROC glad_glTexParameterf = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    _pre_call_gles2_callback("glTexParameterf", (GLADapiproc) glad_glTexParameterf, 3, target, pname, param);
    glad_glTexParameterf(target, pname, param);
    _post_call_gles2_callback(NULL, "glTexParameterf", (GLADapiproc) glad_glTexParameterf, 3, target, pname, param);
    
}
PFNGLTEXPARAMETERFPROC glad_debug_glTexParameterf = glad_debug_impl_glTexParameterf;
PFNGLTEXPARAMETERFVPROC glad_glTexParameterfv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameterfv(GLenum target, GLenum pname, const GLfloat * params) {
    _pre_call_gles2_callback("glTexParameterfv", (GLADapiproc) glad_glTexParameterfv, 3, target, pname, params);
    glad_glTexParameterfv(target, pname, params);
    _post_call_gles2_callback(NULL, "glTexParameterfv", (GLADapiproc) glad_glTexParameterfv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERFVPROC glad_debug_glTexParameterfv = glad_debug_impl_glTexParameterfv;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    _pre_call_gles2_callback("glTexParameteri", (GLADapiproc) glad_glTexParameteri, 3, target, pname, param);
    glad_glTexParameteri(target, pname, param);
    _post_call_gles2_callback(NULL, "glTexParameteri", (GLADapiproc) glad_glTexParameteri, 3, target, pname, param);
    
}
PFNGLTEXPARAMETERIPROC glad_debug_glTexParameteri = glad_debug_impl_glTexParameteri;
PFNGLTEXPARAMETERIVPROC glad_glTexParameteriv = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexParameteriv(GLenum target, GLenum pname, const GLint * params) {
    _pre_call_gles2_callback("glTexParameteriv", (GLADapiproc) glad_glTexParameteriv, 3, target, pname, params);
    glad_glTexParameteriv(target, pname, params);
    _post_call_gles2_callback(NULL, "glTexParameteriv", (GLADapiproc) glad_glTexParameteriv, 3, target, pname, params);
    
}
PFNGLTEXPARAMETERIVPROC glad_debug_glTexParameteriv = glad_debug_impl_glTexParameteriv;
PFNGLTEXSTORAGE1DEXTPROC glad_glTexStorage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage1DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    _pre_call_gles2_callback("glTexStorage1DEXT", (GLADapiproc) glad_glTexStorage1DEXT, 4, target, levels, internalformat, width);
    glad_glTexStorage1DEXT(target, levels, internalformat, width);
    _post_call_gles2_callback(NULL, "glTexStorage1DEXT", (GLADapiproc) glad_glTexStorage1DEXT, 4, target, levels, internalformat, width);
    
}
PFNGLTEXSTORAGE1DEXTPROC glad_debug_glTexStorage1DEXT = glad_debug_impl_glTexStorage1DEXT;
PFNGLTEXSTORAGE2DPROC glad_glTexStorage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glTexStorage2D", (GLADapiproc) glad_glTexStorage2D, 5, target, levels, internalformat, width, height);
    glad_glTexStorage2D(target, levels, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glTexStorage2D", (GLADapiproc) glad_glTexStorage2D, 5, target, levels, internalformat, width, height);
    
}
PFNGLTEXSTORAGE2DPROC glad_debug_glTexStorage2D = glad_debug_impl_glTexStorage2D;
PFNGLTEXSTORAGE2DEXTPROC glad_glTexStorage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage2DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glTexStorage2DEXT", (GLADapiproc) glad_glTexStorage2DEXT, 5, target, levels, internalformat, width, height);
    glad_glTexStorage2DEXT(target, levels, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glTexStorage2DEXT", (GLADapiproc) glad_glTexStorage2DEXT, 5, target, levels, internalformat, width, height);
    
}
PFNGLTEXSTORAGE2DEXTPROC glad_debug_glTexStorage2DEXT = glad_debug_impl_glTexStorage2DEXT;
PFNGLTEXSTORAGE2DMULTISAMPLEPROC glad_glTexStorage2DMultisample = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations) {
    _pre_call_gles2_callback("glTexStorage2DMultisample", (GLADapiproc) glad_glTexStorage2DMultisample, 6, target, samples, internalformat, width, height, fixedsamplelocations);
    glad_glTexStorage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
    _post_call_gles2_callback(NULL, "glTexStorage2DMultisample", (GLADapiproc) glad_glTexStorage2DMultisample, 6, target, samples, internalformat, width, height, fixedsamplelocations);
    
}
PFNGLTEXSTORAGE2DMULTISAMPLEPROC glad_debug_glTexStorage2DMultisample = glad_debug_impl_glTexStorage2DMultisample;
PFNGLTEXSTORAGE3DPROC glad_glTexStorage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    _pre_call_gles2_callback("glTexStorage3D", (GLADapiproc) glad_glTexStorage3D, 6, target, levels, internalformat, width, height, depth);
    glad_glTexStorage3D(target, levels, internalformat, width, height, depth);
    _post_call_gles2_callback(NULL, "glTexStorage3D", (GLADapiproc) glad_glTexStorage3D, 6, target, levels, internalformat, width, height, depth);
    
}
PFNGLTEXSTORAGE3DPROC glad_debug_glTexStorage3D = glad_debug_impl_glTexStorage3D;
PFNGLTEXSTORAGE3DEXTPROC glad_glTexStorage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage3DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    _pre_call_gles2_callback("glTexStorage3DEXT", (GLADapiproc) glad_glTexStorage3DEXT, 6, target, levels, internalformat, width, height, depth);
    glad_glTexStorage3DEXT(target, levels, internalformat, width, height, depth);
    _post_call_gles2_callback(NULL, "glTexStorage3DEXT", (GLADapiproc) glad_glTexStorage3DEXT, 6, target, levels, internalformat, width, height, depth);
    
}
PFNGLTEXSTORAGE3DEXTPROC glad_debug_glTexStorage3DEXT = glad_debug_impl_glTexStorage3DEXT;
PFNGLTEXSTORAGE3DMULTISAMPLEPROC glad_glTexStorage3DMultisample = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage3DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    _pre_call_gles2_callback("glTexStorage3DMultisample", (GLADapiproc) glad_glTexStorage3DMultisample, 7, target, samples, internalformat, width, height, depth, fixedsamplelocations);
    glad_glTexStorage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations);
    _post_call_gles2_callback(NULL, "glTexStorage3DMultisample", (GLADapiproc) glad_glTexStorage3DMultisample, 7, target, samples, internalformat, width, height, depth, fixedsamplelocations);
    
}
PFNGLTEXSTORAGE3DMULTISAMPLEPROC glad_debug_glTexStorage3DMultisample = glad_debug_impl_glTexStorage3DMultisample;
PFNGLTEXSTORAGE3DMULTISAMPLEOESPROC glad_glTexStorage3DMultisampleOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorage3DMultisampleOES(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations) {
    _pre_call_gles2_callback("glTexStorage3DMultisampleOES", (GLADapiproc) glad_glTexStorage3DMultisampleOES, 7, target, samples, internalformat, width, height, depth, fixedsamplelocations);
    glad_glTexStorage3DMultisampleOES(target, samples, internalformat, width, height, depth, fixedsamplelocations);
    _post_call_gles2_callback(NULL, "glTexStorage3DMultisampleOES", (GLADapiproc) glad_glTexStorage3DMultisampleOES, 7, target, samples, internalformat, width, height, depth, fixedsamplelocations);
    
}
PFNGLTEXSTORAGE3DMULTISAMPLEOESPROC glad_debug_glTexStorage3DMultisampleOES = glad_debug_impl_glTexStorage3DMultisampleOES;
PFNGLTEXSTORAGEATTRIBS2DEXTPROC glad_glTexStorageAttribs2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorageAttribs2DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, const GLint * attrib_list) {
    _pre_call_gles2_callback("glTexStorageAttribs2DEXT", (GLADapiproc) glad_glTexStorageAttribs2DEXT, 6, target, levels, internalformat, width, height, attrib_list);
    glad_glTexStorageAttribs2DEXT(target, levels, internalformat, width, height, attrib_list);
    _post_call_gles2_callback(NULL, "glTexStorageAttribs2DEXT", (GLADapiproc) glad_glTexStorageAttribs2DEXT, 6, target, levels, internalformat, width, height, attrib_list);
    
}
PFNGLTEXSTORAGEATTRIBS2DEXTPROC glad_debug_glTexStorageAttribs2DEXT = glad_debug_impl_glTexStorageAttribs2DEXT;
PFNGLTEXSTORAGEATTRIBS3DEXTPROC glad_glTexStorageAttribs3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorageAttribs3DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, const GLint * attrib_list) {
    _pre_call_gles2_callback("glTexStorageAttribs3DEXT", (GLADapiproc) glad_glTexStorageAttribs3DEXT, 7, target, levels, internalformat, width, height, depth, attrib_list);
    glad_glTexStorageAttribs3DEXT(target, levels, internalformat, width, height, depth, attrib_list);
    _post_call_gles2_callback(NULL, "glTexStorageAttribs3DEXT", (GLADapiproc) glad_glTexStorageAttribs3DEXT, 7, target, levels, internalformat, width, height, depth, attrib_list);
    
}
PFNGLTEXSTORAGEATTRIBS3DEXTPROC glad_debug_glTexStorageAttribs3DEXT = glad_debug_impl_glTexStorageAttribs3DEXT;
PFNGLTEXSTORAGEMEM2DEXTPROC glad_glTexStorageMem2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorageMem2DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTexStorageMem2DEXT", (GLADapiproc) glad_glTexStorageMem2DEXT, 7, target, levels, internalFormat, width, height, memory, offset);
    glad_glTexStorageMem2DEXT(target, levels, internalFormat, width, height, memory, offset);
    _post_call_gles2_callback(NULL, "glTexStorageMem2DEXT", (GLADapiproc) glad_glTexStorageMem2DEXT, 7, target, levels, internalFormat, width, height, memory, offset);
    
}
PFNGLTEXSTORAGEMEM2DEXTPROC glad_debug_glTexStorageMem2DEXT = glad_debug_impl_glTexStorageMem2DEXT;
PFNGLTEXSTORAGEMEM2DMULTISAMPLEEXTPROC glad_glTexStorageMem2DMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorageMem2DMultisampleEXT(GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTexStorageMem2DMultisampleEXT", (GLADapiproc) glad_glTexStorageMem2DMultisampleEXT, 8, target, samples, internalFormat, width, height, fixedSampleLocations, memory, offset);
    glad_glTexStorageMem2DMultisampleEXT(target, samples, internalFormat, width, height, fixedSampleLocations, memory, offset);
    _post_call_gles2_callback(NULL, "glTexStorageMem2DMultisampleEXT", (GLADapiproc) glad_glTexStorageMem2DMultisampleEXT, 8, target, samples, internalFormat, width, height, fixedSampleLocations, memory, offset);
    
}
PFNGLTEXSTORAGEMEM2DMULTISAMPLEEXTPROC glad_debug_glTexStorageMem2DMultisampleEXT = glad_debug_impl_glTexStorageMem2DMultisampleEXT;
PFNGLTEXSTORAGEMEM3DEXTPROC glad_glTexStorageMem3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorageMem3DEXT(GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTexStorageMem3DEXT", (GLADapiproc) glad_glTexStorageMem3DEXT, 8, target, levels, internalFormat, width, height, depth, memory, offset);
    glad_glTexStorageMem3DEXT(target, levels, internalFormat, width, height, depth, memory, offset);
    _post_call_gles2_callback(NULL, "glTexStorageMem3DEXT", (GLADapiproc) glad_glTexStorageMem3DEXT, 8, target, levels, internalFormat, width, height, depth, memory, offset);
    
}
PFNGLTEXSTORAGEMEM3DEXTPROC glad_debug_glTexStorageMem3DEXT = glad_debug_impl_glTexStorageMem3DEXT;
PFNGLTEXSTORAGEMEM3DMULTISAMPLEEXTPROC glad_glTexStorageMem3DMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexStorageMem3DMultisampleEXT(GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTexStorageMem3DMultisampleEXT", (GLADapiproc) glad_glTexStorageMem3DMultisampleEXT, 9, target, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset);
    glad_glTexStorageMem3DMultisampleEXT(target, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset);
    _post_call_gles2_callback(NULL, "glTexStorageMem3DMultisampleEXT", (GLADapiproc) glad_glTexStorageMem3DMultisampleEXT, 9, target, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset);
    
}
PFNGLTEXSTORAGEMEM3DMULTISAMPLEEXTPROC glad_debug_glTexStorageMem3DMultisampleEXT = glad_debug_impl_glTexStorageMem3DMultisampleEXT;
PFNGLTEXSUBIMAGE2DPROC glad_glTexSubImage2D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gles2_callback("glTexSubImage2D", (GLADapiproc) glad_glTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, type, pixels);
    glad_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
    _post_call_gles2_callback(NULL, "glTexSubImage2D", (GLADapiproc) glad_glTexSubImage2D, 9, target, level, xoffset, yoffset, width, height, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE2DPROC glad_debug_glTexSubImage2D = glad_debug_impl_glTexSubImage2D;
PFNGLTEXSUBIMAGE3DPROC glad_glTexSubImage3D = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gles2_callback("glTexSubImage3D", (GLADapiproc) glad_glTexSubImage3D, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    glad_glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    _post_call_gles2_callback(NULL, "glTexSubImage3D", (GLADapiproc) glad_glTexSubImage3D, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE3DPROC glad_debug_glTexSubImage3D = glad_debug_impl_glTexSubImage3D;
PFNGLTEXSUBIMAGE3DOESPROC glad_glTexSubImage3DOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexSubImage3DOES(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void * pixels) {
    _pre_call_gles2_callback("glTexSubImage3DOES", (GLADapiproc) glad_glTexSubImage3DOES, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    glad_glTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    _post_call_gles2_callback(NULL, "glTexSubImage3DOES", (GLADapiproc) glad_glTexSubImage3DOES, 11, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
    
}
PFNGLTEXSUBIMAGE3DOESPROC glad_debug_glTexSubImage3DOES = glad_debug_impl_glTexSubImage3DOES;
PFNGLTEXTUREATTACHMEMORYNVPROC glad_glTextureAttachMemoryNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureAttachMemoryNV(GLuint texture, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTextureAttachMemoryNV", (GLADapiproc) glad_glTextureAttachMemoryNV, 3, texture, memory, offset);
    glad_glTextureAttachMemoryNV(texture, memory, offset);
    _post_call_gles2_callback(NULL, "glTextureAttachMemoryNV", (GLADapiproc) glad_glTextureAttachMemoryNV, 3, texture, memory, offset);
    
}
PFNGLTEXTUREATTACHMEMORYNVPROC glad_debug_glTextureAttachMemoryNV = glad_debug_impl_glTextureAttachMemoryNV;
PFNGLTEXTUREBARRIERNVPROC glad_glTextureBarrierNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureBarrierNV(void) {
    _pre_call_gles2_callback("glTextureBarrierNV", (GLADapiproc) glad_glTextureBarrierNV, 0);
    glad_glTextureBarrierNV();
    _post_call_gles2_callback(NULL, "glTextureBarrierNV", (GLADapiproc) glad_glTextureBarrierNV, 0);
    
}
PFNGLTEXTUREBARRIERNVPROC glad_debug_glTextureBarrierNV = glad_debug_impl_glTextureBarrierNV;
PFNGLTEXTUREFOVEATIONPARAMETERSQCOMPROC glad_glTextureFoveationParametersQCOM = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureFoveationParametersQCOM(GLuint texture, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY, GLfloat gainX, GLfloat gainY, GLfloat foveaArea) {
    _pre_call_gles2_callback("glTextureFoveationParametersQCOM", (GLADapiproc) glad_glTextureFoveationParametersQCOM, 8, texture, layer, focalPoint, focalX, focalY, gainX, gainY, foveaArea);
    glad_glTextureFoveationParametersQCOM(texture, layer, focalPoint, focalX, focalY, gainX, gainY, foveaArea);
    _post_call_gles2_callback(NULL, "glTextureFoveationParametersQCOM", (GLADapiproc) glad_glTextureFoveationParametersQCOM, 8, texture, layer, focalPoint, focalX, focalY, gainX, gainY, foveaArea);
    
}
PFNGLTEXTUREFOVEATIONPARAMETERSQCOMPROC glad_debug_glTextureFoveationParametersQCOM = glad_debug_impl_glTextureFoveationParametersQCOM;
PFNGLTEXTUREPAGECOMMITMENTMEMNVPROC glad_glTexturePageCommitmentMemNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTexturePageCommitmentMemNV(GLuint texture, GLint layer, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset, GLboolean commit) {
    _pre_call_gles2_callback("glTexturePageCommitmentMemNV", (GLADapiproc) glad_glTexturePageCommitmentMemNV, 12, texture, layer, level, xoffset, yoffset, zoffset, width, height, depth, memory, offset, commit);
    glad_glTexturePageCommitmentMemNV(texture, layer, level, xoffset, yoffset, zoffset, width, height, depth, memory, offset, commit);
    _post_call_gles2_callback(NULL, "glTexturePageCommitmentMemNV", (GLADapiproc) glad_glTexturePageCommitmentMemNV, 12, texture, layer, level, xoffset, yoffset, zoffset, width, height, depth, memory, offset, commit);
    
}
PFNGLTEXTUREPAGECOMMITMENTMEMNVPROC glad_debug_glTexturePageCommitmentMemNV = glad_debug_impl_glTexturePageCommitmentMemNV;
PFNGLTEXTURESTORAGE1DEXTPROC glad_glTextureStorage1DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage1DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
    _pre_call_gles2_callback("glTextureStorage1DEXT", (GLADapiproc) glad_glTextureStorage1DEXT, 5, texture, target, levels, internalformat, width);
    glad_glTextureStorage1DEXT(texture, target, levels, internalformat, width);
    _post_call_gles2_callback(NULL, "glTextureStorage1DEXT", (GLADapiproc) glad_glTextureStorage1DEXT, 5, texture, target, levels, internalformat, width);
    
}
PFNGLTEXTURESTORAGE1DEXTPROC glad_debug_glTextureStorage1DEXT = glad_debug_impl_glTextureStorage1DEXT;
PFNGLTEXTURESTORAGE2DEXTPROC glad_glTextureStorage2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage2DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glTextureStorage2DEXT", (GLADapiproc) glad_glTextureStorage2DEXT, 6, texture, target, levels, internalformat, width, height);
    glad_glTextureStorage2DEXT(texture, target, levels, internalformat, width, height);
    _post_call_gles2_callback(NULL, "glTextureStorage2DEXT", (GLADapiproc) glad_glTextureStorage2DEXT, 6, texture, target, levels, internalformat, width, height);
    
}
PFNGLTEXTURESTORAGE2DEXTPROC glad_debug_glTextureStorage2DEXT = glad_debug_impl_glTextureStorage2DEXT;
PFNGLTEXTURESTORAGE3DEXTPROC glad_glTextureStorage3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorage3DEXT(GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    _pre_call_gles2_callback("glTextureStorage3DEXT", (GLADapiproc) glad_glTextureStorage3DEXT, 7, texture, target, levels, internalformat, width, height, depth);
    glad_glTextureStorage3DEXT(texture, target, levels, internalformat, width, height, depth);
    _post_call_gles2_callback(NULL, "glTextureStorage3DEXT", (GLADapiproc) glad_glTextureStorage3DEXT, 7, texture, target, levels, internalformat, width, height, depth);
    
}
PFNGLTEXTURESTORAGE3DEXTPROC glad_debug_glTextureStorage3DEXT = glad_debug_impl_glTextureStorage3DEXT;
PFNGLTEXTURESTORAGEMEM2DEXTPROC glad_glTextureStorageMem2DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorageMem2DEXT(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTextureStorageMem2DEXT", (GLADapiproc) glad_glTextureStorageMem2DEXT, 7, texture, levels, internalFormat, width, height, memory, offset);
    glad_glTextureStorageMem2DEXT(texture, levels, internalFormat, width, height, memory, offset);
    _post_call_gles2_callback(NULL, "glTextureStorageMem2DEXT", (GLADapiproc) glad_glTextureStorageMem2DEXT, 7, texture, levels, internalFormat, width, height, memory, offset);
    
}
PFNGLTEXTURESTORAGEMEM2DEXTPROC glad_debug_glTextureStorageMem2DEXT = glad_debug_impl_glTextureStorageMem2DEXT;
PFNGLTEXTURESTORAGEMEM2DMULTISAMPLEEXTPROC glad_glTextureStorageMem2DMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorageMem2DMultisampleEXT(GLuint texture, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTextureStorageMem2DMultisampleEXT", (GLADapiproc) glad_glTextureStorageMem2DMultisampleEXT, 8, texture, samples, internalFormat, width, height, fixedSampleLocations, memory, offset);
    glad_glTextureStorageMem2DMultisampleEXT(texture, samples, internalFormat, width, height, fixedSampleLocations, memory, offset);
    _post_call_gles2_callback(NULL, "glTextureStorageMem2DMultisampleEXT", (GLADapiproc) glad_glTextureStorageMem2DMultisampleEXT, 8, texture, samples, internalFormat, width, height, fixedSampleLocations, memory, offset);
    
}
PFNGLTEXTURESTORAGEMEM2DMULTISAMPLEEXTPROC glad_debug_glTextureStorageMem2DMultisampleEXT = glad_debug_impl_glTextureStorageMem2DMultisampleEXT;
PFNGLTEXTURESTORAGEMEM3DEXTPROC glad_glTextureStorageMem3DEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorageMem3DEXT(GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTextureStorageMem3DEXT", (GLADapiproc) glad_glTextureStorageMem3DEXT, 8, texture, levels, internalFormat, width, height, depth, memory, offset);
    glad_glTextureStorageMem3DEXT(texture, levels, internalFormat, width, height, depth, memory, offset);
    _post_call_gles2_callback(NULL, "glTextureStorageMem3DEXT", (GLADapiproc) glad_glTextureStorageMem3DEXT, 8, texture, levels, internalFormat, width, height, depth, memory, offset);
    
}
PFNGLTEXTURESTORAGEMEM3DEXTPROC glad_debug_glTextureStorageMem3DEXT = glad_debug_impl_glTextureStorageMem3DEXT;
PFNGLTEXTURESTORAGEMEM3DMULTISAMPLEEXTPROC glad_glTextureStorageMem3DMultisampleEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureStorageMem3DMultisampleEXT(GLuint texture, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedSampleLocations, GLuint memory, GLuint64 offset) {
    _pre_call_gles2_callback("glTextureStorageMem3DMultisampleEXT", (GLADapiproc) glad_glTextureStorageMem3DMultisampleEXT, 9, texture, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset);
    glad_glTextureStorageMem3DMultisampleEXT(texture, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset);
    _post_call_gles2_callback(NULL, "glTextureStorageMem3DMultisampleEXT", (GLADapiproc) glad_glTextureStorageMem3DMultisampleEXT, 9, texture, samples, internalFormat, width, height, depth, fixedSampleLocations, memory, offset);
    
}
PFNGLTEXTURESTORAGEMEM3DMULTISAMPLEEXTPROC glad_debug_glTextureStorageMem3DMultisampleEXT = glad_debug_impl_glTextureStorageMem3DMultisampleEXT;
PFNGLTEXTUREVIEWEXTPROC glad_glTextureViewEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureViewEXT(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers) {
    _pre_call_gles2_callback("glTextureViewEXT", (GLADapiproc) glad_glTextureViewEXT, 8, texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
    glad_glTextureViewEXT(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
    _post_call_gles2_callback(NULL, "glTextureViewEXT", (GLADapiproc) glad_glTextureViewEXT, 8, texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
    
}
PFNGLTEXTUREVIEWEXTPROC glad_debug_glTextureViewEXT = glad_debug_impl_glTextureViewEXT;
PFNGLTEXTUREVIEWOESPROC glad_glTextureViewOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glTextureViewOES(GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers) {
    _pre_call_gles2_callback("glTextureViewOES", (GLADapiproc) glad_glTextureViewOES, 8, texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
    glad_glTextureViewOES(texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
    _post_call_gles2_callback(NULL, "glTextureViewOES", (GLADapiproc) glad_glTextureViewOES, 8, texture, target, origtexture, internalformat, minlevel, numlevels, minlayer, numlayers);
    
}
PFNGLTEXTUREVIEWOESPROC glad_debug_glTextureViewOES = glad_debug_impl_glTextureViewOES;
PFNGLTRANSFORMFEEDBACKVARYINGSPROC glad_glTransformFeedbackVaryings = NULL;
static void GLAD_API_PTR glad_debug_impl_glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const* varyings, GLenum bufferMode) {
    _pre_call_gles2_callback("glTransformFeedbackVaryings", (GLADapiproc) glad_glTransformFeedbackVaryings, 4, program, count, varyings, bufferMode);
    glad_glTransformFeedbackVaryings(program, count, varyings, bufferMode);
    _post_call_gles2_callback(NULL, "glTransformFeedbackVaryings", (GLADapiproc) glad_glTransformFeedbackVaryings, 4, program, count, varyings, bufferMode);
    
}
PFNGLTRANSFORMFEEDBACKVARYINGSPROC glad_debug_glTransformFeedbackVaryings = glad_debug_impl_glTransformFeedbackVaryings;
PFNGLTRANSFORMPATHNVPROC glad_glTransformPathNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glTransformPathNV(GLuint resultPath, GLuint srcPath, GLenum transformType, const GLfloat * transformValues) {
    _pre_call_gles2_callback("glTransformPathNV", (GLADapiproc) glad_glTransformPathNV, 4, resultPath, srcPath, transformType, transformValues);
    glad_glTransformPathNV(resultPath, srcPath, transformType, transformValues);
    _post_call_gles2_callback(NULL, "glTransformPathNV", (GLADapiproc) glad_glTransformPathNV, 4, resultPath, srcPath, transformType, transformValues);
    
}
PFNGLTRANSFORMPATHNVPROC glad_debug_glTransformPathNV = glad_debug_impl_glTransformPathNV;
PFNGLUNIFORM1FPROC glad_glUniform1f = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1f(GLint location, GLfloat v0) {
    _pre_call_gles2_callback("glUniform1f", (GLADapiproc) glad_glUniform1f, 2, location, v0);
    glad_glUniform1f(location, v0);
    _post_call_gles2_callback(NULL, "glUniform1f", (GLADapiproc) glad_glUniform1f, 2, location, v0);
    
}
PFNGLUNIFORM1FPROC glad_debug_glUniform1f = glad_debug_impl_glUniform1f;
PFNGLUNIFORM1FVPROC glad_glUniform1fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1fv(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glUniform1fv", (GLADapiproc) glad_glUniform1fv, 3, location, count, value);
    glad_glUniform1fv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform1fv", (GLADapiproc) glad_glUniform1fv, 3, location, count, value);
    
}
PFNGLUNIFORM1FVPROC glad_debug_glUniform1fv = glad_debug_impl_glUniform1fv;
PFNGLUNIFORM1IPROC glad_glUniform1i = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1i(GLint location, GLint v0) {
    _pre_call_gles2_callback("glUniform1i", (GLADapiproc) glad_glUniform1i, 2, location, v0);
    glad_glUniform1i(location, v0);
    _post_call_gles2_callback(NULL, "glUniform1i", (GLADapiproc) glad_glUniform1i, 2, location, v0);
    
}
PFNGLUNIFORM1IPROC glad_debug_glUniform1i = glad_debug_impl_glUniform1i;
PFNGLUNIFORM1I64NVPROC glad_glUniform1i64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1i64NV(GLint location, GLint64EXT x) {
    _pre_call_gles2_callback("glUniform1i64NV", (GLADapiproc) glad_glUniform1i64NV, 2, location, x);
    glad_glUniform1i64NV(location, x);
    _post_call_gles2_callback(NULL, "glUniform1i64NV", (GLADapiproc) glad_glUniform1i64NV, 2, location, x);
    
}
PFNGLUNIFORM1I64NVPROC glad_debug_glUniform1i64NV = glad_debug_impl_glUniform1i64NV;
PFNGLUNIFORM1I64VNVPROC glad_glUniform1i64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1i64vNV(GLint location, GLsizei count, const GLint64EXT * value) {
    _pre_call_gles2_callback("glUniform1i64vNV", (GLADapiproc) glad_glUniform1i64vNV, 3, location, count, value);
    glad_glUniform1i64vNV(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform1i64vNV", (GLADapiproc) glad_glUniform1i64vNV, 3, location, count, value);
    
}
PFNGLUNIFORM1I64VNVPROC glad_debug_glUniform1i64vNV = glad_debug_impl_glUniform1i64vNV;
PFNGLUNIFORM1IVPROC glad_glUniform1iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1iv(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glUniform1iv", (GLADapiproc) glad_glUniform1iv, 3, location, count, value);
    glad_glUniform1iv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform1iv", (GLADapiproc) glad_glUniform1iv, 3, location, count, value);
    
}
PFNGLUNIFORM1IVPROC glad_debug_glUniform1iv = glad_debug_impl_glUniform1iv;
PFNGLUNIFORM1UIPROC glad_glUniform1ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1ui(GLint location, GLuint v0) {
    _pre_call_gles2_callback("glUniform1ui", (GLADapiproc) glad_glUniform1ui, 2, location, v0);
    glad_glUniform1ui(location, v0);
    _post_call_gles2_callback(NULL, "glUniform1ui", (GLADapiproc) glad_glUniform1ui, 2, location, v0);
    
}
PFNGLUNIFORM1UIPROC glad_debug_glUniform1ui = glad_debug_impl_glUniform1ui;
PFNGLUNIFORM1UI64NVPROC glad_glUniform1ui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1ui64NV(GLint location, GLuint64EXT x) {
    _pre_call_gles2_callback("glUniform1ui64NV", (GLADapiproc) glad_glUniform1ui64NV, 2, location, x);
    glad_glUniform1ui64NV(location, x);
    _post_call_gles2_callback(NULL, "glUniform1ui64NV", (GLADapiproc) glad_glUniform1ui64NV, 2, location, x);
    
}
PFNGLUNIFORM1UI64NVPROC glad_debug_glUniform1ui64NV = glad_debug_impl_glUniform1ui64NV;
PFNGLUNIFORM1UI64VNVPROC glad_glUniform1ui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1ui64vNV(GLint location, GLsizei count, const GLuint64EXT * value) {
    _pre_call_gles2_callback("glUniform1ui64vNV", (GLADapiproc) glad_glUniform1ui64vNV, 3, location, count, value);
    glad_glUniform1ui64vNV(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform1ui64vNV", (GLADapiproc) glad_glUniform1ui64vNV, 3, location, count, value);
    
}
PFNGLUNIFORM1UI64VNVPROC glad_debug_glUniform1ui64vNV = glad_debug_impl_glUniform1ui64vNV;
PFNGLUNIFORM1UIVPROC glad_glUniform1uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform1uiv(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glUniform1uiv", (GLADapiproc) glad_glUniform1uiv, 3, location, count, value);
    glad_glUniform1uiv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform1uiv", (GLADapiproc) glad_glUniform1uiv, 3, location, count, value);
    
}
PFNGLUNIFORM1UIVPROC glad_debug_glUniform1uiv = glad_debug_impl_glUniform1uiv;
PFNGLUNIFORM2FPROC glad_glUniform2f = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    _pre_call_gles2_callback("glUniform2f", (GLADapiproc) glad_glUniform2f, 3, location, v0, v1);
    glad_glUniform2f(location, v0, v1);
    _post_call_gles2_callback(NULL, "glUniform2f", (GLADapiproc) glad_glUniform2f, 3, location, v0, v1);
    
}
PFNGLUNIFORM2FPROC glad_debug_glUniform2f = glad_debug_impl_glUniform2f;
PFNGLUNIFORM2FVPROC glad_glUniform2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2fv(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glUniform2fv", (GLADapiproc) glad_glUniform2fv, 3, location, count, value);
    glad_glUniform2fv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform2fv", (GLADapiproc) glad_glUniform2fv, 3, location, count, value);
    
}
PFNGLUNIFORM2FVPROC glad_debug_glUniform2fv = glad_debug_impl_glUniform2fv;
PFNGLUNIFORM2IPROC glad_glUniform2i = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2i(GLint location, GLint v0, GLint v1) {
    _pre_call_gles2_callback("glUniform2i", (GLADapiproc) glad_glUniform2i, 3, location, v0, v1);
    glad_glUniform2i(location, v0, v1);
    _post_call_gles2_callback(NULL, "glUniform2i", (GLADapiproc) glad_glUniform2i, 3, location, v0, v1);
    
}
PFNGLUNIFORM2IPROC glad_debug_glUniform2i = glad_debug_impl_glUniform2i;
PFNGLUNIFORM2I64NVPROC glad_glUniform2i64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2i64NV(GLint location, GLint64EXT x, GLint64EXT y) {
    _pre_call_gles2_callback("glUniform2i64NV", (GLADapiproc) glad_glUniform2i64NV, 3, location, x, y);
    glad_glUniform2i64NV(location, x, y);
    _post_call_gles2_callback(NULL, "glUniform2i64NV", (GLADapiproc) glad_glUniform2i64NV, 3, location, x, y);
    
}
PFNGLUNIFORM2I64NVPROC glad_debug_glUniform2i64NV = glad_debug_impl_glUniform2i64NV;
PFNGLUNIFORM2I64VNVPROC glad_glUniform2i64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2i64vNV(GLint location, GLsizei count, const GLint64EXT * value) {
    _pre_call_gles2_callback("glUniform2i64vNV", (GLADapiproc) glad_glUniform2i64vNV, 3, location, count, value);
    glad_glUniform2i64vNV(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform2i64vNV", (GLADapiproc) glad_glUniform2i64vNV, 3, location, count, value);
    
}
PFNGLUNIFORM2I64VNVPROC glad_debug_glUniform2i64vNV = glad_debug_impl_glUniform2i64vNV;
PFNGLUNIFORM2IVPROC glad_glUniform2iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2iv(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glUniform2iv", (GLADapiproc) glad_glUniform2iv, 3, location, count, value);
    glad_glUniform2iv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform2iv", (GLADapiproc) glad_glUniform2iv, 3, location, count, value);
    
}
PFNGLUNIFORM2IVPROC glad_debug_glUniform2iv = glad_debug_impl_glUniform2iv;
PFNGLUNIFORM2UIPROC glad_glUniform2ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2ui(GLint location, GLuint v0, GLuint v1) {
    _pre_call_gles2_callback("glUniform2ui", (GLADapiproc) glad_glUniform2ui, 3, location, v0, v1);
    glad_glUniform2ui(location, v0, v1);
    _post_call_gles2_callback(NULL, "glUniform2ui", (GLADapiproc) glad_glUniform2ui, 3, location, v0, v1);
    
}
PFNGLUNIFORM2UIPROC glad_debug_glUniform2ui = glad_debug_impl_glUniform2ui;
PFNGLUNIFORM2UI64NVPROC glad_glUniform2ui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2ui64NV(GLint location, GLuint64EXT x, GLuint64EXT y) {
    _pre_call_gles2_callback("glUniform2ui64NV", (GLADapiproc) glad_glUniform2ui64NV, 3, location, x, y);
    glad_glUniform2ui64NV(location, x, y);
    _post_call_gles2_callback(NULL, "glUniform2ui64NV", (GLADapiproc) glad_glUniform2ui64NV, 3, location, x, y);
    
}
PFNGLUNIFORM2UI64NVPROC glad_debug_glUniform2ui64NV = glad_debug_impl_glUniform2ui64NV;
PFNGLUNIFORM2UI64VNVPROC glad_glUniform2ui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2ui64vNV(GLint location, GLsizei count, const GLuint64EXT * value) {
    _pre_call_gles2_callback("glUniform2ui64vNV", (GLADapiproc) glad_glUniform2ui64vNV, 3, location, count, value);
    glad_glUniform2ui64vNV(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform2ui64vNV", (GLADapiproc) glad_glUniform2ui64vNV, 3, location, count, value);
    
}
PFNGLUNIFORM2UI64VNVPROC glad_debug_glUniform2ui64vNV = glad_debug_impl_glUniform2ui64vNV;
PFNGLUNIFORM2UIVPROC glad_glUniform2uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform2uiv(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glUniform2uiv", (GLADapiproc) glad_glUniform2uiv, 3, location, count, value);
    glad_glUniform2uiv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform2uiv", (GLADapiproc) glad_glUniform2uiv, 3, location, count, value);
    
}
PFNGLUNIFORM2UIVPROC glad_debug_glUniform2uiv = glad_debug_impl_glUniform2uiv;
PFNGLUNIFORM3FPROC glad_glUniform3f = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    _pre_call_gles2_callback("glUniform3f", (GLADapiproc) glad_glUniform3f, 4, location, v0, v1, v2);
    glad_glUniform3f(location, v0, v1, v2);
    _post_call_gles2_callback(NULL, "glUniform3f", (GLADapiproc) glad_glUniform3f, 4, location, v0, v1, v2);
    
}
PFNGLUNIFORM3FPROC glad_debug_glUniform3f = glad_debug_impl_glUniform3f;
PFNGLUNIFORM3FVPROC glad_glUniform3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3fv(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glUniform3fv", (GLADapiproc) glad_glUniform3fv, 3, location, count, value);
    glad_glUniform3fv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform3fv", (GLADapiproc) glad_glUniform3fv, 3, location, count, value);
    
}
PFNGLUNIFORM3FVPROC glad_debug_glUniform3fv = glad_debug_impl_glUniform3fv;
PFNGLUNIFORM3IPROC glad_glUniform3i = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    _pre_call_gles2_callback("glUniform3i", (GLADapiproc) glad_glUniform3i, 4, location, v0, v1, v2);
    glad_glUniform3i(location, v0, v1, v2);
    _post_call_gles2_callback(NULL, "glUniform3i", (GLADapiproc) glad_glUniform3i, 4, location, v0, v1, v2);
    
}
PFNGLUNIFORM3IPROC glad_debug_glUniform3i = glad_debug_impl_glUniform3i;
PFNGLUNIFORM3I64NVPROC glad_glUniform3i64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3i64NV(GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z) {
    _pre_call_gles2_callback("glUniform3i64NV", (GLADapiproc) glad_glUniform3i64NV, 4, location, x, y, z);
    glad_glUniform3i64NV(location, x, y, z);
    _post_call_gles2_callback(NULL, "glUniform3i64NV", (GLADapiproc) glad_glUniform3i64NV, 4, location, x, y, z);
    
}
PFNGLUNIFORM3I64NVPROC glad_debug_glUniform3i64NV = glad_debug_impl_glUniform3i64NV;
PFNGLUNIFORM3I64VNVPROC glad_glUniform3i64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3i64vNV(GLint location, GLsizei count, const GLint64EXT * value) {
    _pre_call_gles2_callback("glUniform3i64vNV", (GLADapiproc) glad_glUniform3i64vNV, 3, location, count, value);
    glad_glUniform3i64vNV(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform3i64vNV", (GLADapiproc) glad_glUniform3i64vNV, 3, location, count, value);
    
}
PFNGLUNIFORM3I64VNVPROC glad_debug_glUniform3i64vNV = glad_debug_impl_glUniform3i64vNV;
PFNGLUNIFORM3IVPROC glad_glUniform3iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3iv(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glUniform3iv", (GLADapiproc) glad_glUniform3iv, 3, location, count, value);
    glad_glUniform3iv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform3iv", (GLADapiproc) glad_glUniform3iv, 3, location, count, value);
    
}
PFNGLUNIFORM3IVPROC glad_debug_glUniform3iv = glad_debug_impl_glUniform3iv;
PFNGLUNIFORM3UIPROC glad_glUniform3ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    _pre_call_gles2_callback("glUniform3ui", (GLADapiproc) glad_glUniform3ui, 4, location, v0, v1, v2);
    glad_glUniform3ui(location, v0, v1, v2);
    _post_call_gles2_callback(NULL, "glUniform3ui", (GLADapiproc) glad_glUniform3ui, 4, location, v0, v1, v2);
    
}
PFNGLUNIFORM3UIPROC glad_debug_glUniform3ui = glad_debug_impl_glUniform3ui;
PFNGLUNIFORM3UI64NVPROC glad_glUniform3ui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3ui64NV(GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z) {
    _pre_call_gles2_callback("glUniform3ui64NV", (GLADapiproc) glad_glUniform3ui64NV, 4, location, x, y, z);
    glad_glUniform3ui64NV(location, x, y, z);
    _post_call_gles2_callback(NULL, "glUniform3ui64NV", (GLADapiproc) glad_glUniform3ui64NV, 4, location, x, y, z);
    
}
PFNGLUNIFORM3UI64NVPROC glad_debug_glUniform3ui64NV = glad_debug_impl_glUniform3ui64NV;
PFNGLUNIFORM3UI64VNVPROC glad_glUniform3ui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3ui64vNV(GLint location, GLsizei count, const GLuint64EXT * value) {
    _pre_call_gles2_callback("glUniform3ui64vNV", (GLADapiproc) glad_glUniform3ui64vNV, 3, location, count, value);
    glad_glUniform3ui64vNV(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform3ui64vNV", (GLADapiproc) glad_glUniform3ui64vNV, 3, location, count, value);
    
}
PFNGLUNIFORM3UI64VNVPROC glad_debug_glUniform3ui64vNV = glad_debug_impl_glUniform3ui64vNV;
PFNGLUNIFORM3UIVPROC glad_glUniform3uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform3uiv(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glUniform3uiv", (GLADapiproc) glad_glUniform3uiv, 3, location, count, value);
    glad_glUniform3uiv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform3uiv", (GLADapiproc) glad_glUniform3uiv, 3, location, count, value);
    
}
PFNGLUNIFORM3UIVPROC glad_debug_glUniform3uiv = glad_debug_impl_glUniform3uiv;
PFNGLUNIFORM4FPROC glad_glUniform4f = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    _pre_call_gles2_callback("glUniform4f", (GLADapiproc) glad_glUniform4f, 5, location, v0, v1, v2, v3);
    glad_glUniform4f(location, v0, v1, v2, v3);
    _post_call_gles2_callback(NULL, "glUniform4f", (GLADapiproc) glad_glUniform4f, 5, location, v0, v1, v2, v3);
    
}
PFNGLUNIFORM4FPROC glad_debug_glUniform4f = glad_debug_impl_glUniform4f;
PFNGLUNIFORM4FVPROC glad_glUniform4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4fv(GLint location, GLsizei count, const GLfloat * value) {
    _pre_call_gles2_callback("glUniform4fv", (GLADapiproc) glad_glUniform4fv, 3, location, count, value);
    glad_glUniform4fv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform4fv", (GLADapiproc) glad_glUniform4fv, 3, location, count, value);
    
}
PFNGLUNIFORM4FVPROC glad_debug_glUniform4fv = glad_debug_impl_glUniform4fv;
PFNGLUNIFORM4IPROC glad_glUniform4i = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    _pre_call_gles2_callback("glUniform4i", (GLADapiproc) glad_glUniform4i, 5, location, v0, v1, v2, v3);
    glad_glUniform4i(location, v0, v1, v2, v3);
    _post_call_gles2_callback(NULL, "glUniform4i", (GLADapiproc) glad_glUniform4i, 5, location, v0, v1, v2, v3);
    
}
PFNGLUNIFORM4IPROC glad_debug_glUniform4i = glad_debug_impl_glUniform4i;
PFNGLUNIFORM4I64NVPROC glad_glUniform4i64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4i64NV(GLint location, GLint64EXT x, GLint64EXT y, GLint64EXT z, GLint64EXT w) {
    _pre_call_gles2_callback("glUniform4i64NV", (GLADapiproc) glad_glUniform4i64NV, 5, location, x, y, z, w);
    glad_glUniform4i64NV(location, x, y, z, w);
    _post_call_gles2_callback(NULL, "glUniform4i64NV", (GLADapiproc) glad_glUniform4i64NV, 5, location, x, y, z, w);
    
}
PFNGLUNIFORM4I64NVPROC glad_debug_glUniform4i64NV = glad_debug_impl_glUniform4i64NV;
PFNGLUNIFORM4I64VNVPROC glad_glUniform4i64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4i64vNV(GLint location, GLsizei count, const GLint64EXT * value) {
    _pre_call_gles2_callback("glUniform4i64vNV", (GLADapiproc) glad_glUniform4i64vNV, 3, location, count, value);
    glad_glUniform4i64vNV(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform4i64vNV", (GLADapiproc) glad_glUniform4i64vNV, 3, location, count, value);
    
}
PFNGLUNIFORM4I64VNVPROC glad_debug_glUniform4i64vNV = glad_debug_impl_glUniform4i64vNV;
PFNGLUNIFORM4IVPROC glad_glUniform4iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4iv(GLint location, GLsizei count, const GLint * value) {
    _pre_call_gles2_callback("glUniform4iv", (GLADapiproc) glad_glUniform4iv, 3, location, count, value);
    glad_glUniform4iv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform4iv", (GLADapiproc) glad_glUniform4iv, 3, location, count, value);
    
}
PFNGLUNIFORM4IVPROC glad_debug_glUniform4iv = glad_debug_impl_glUniform4iv;
PFNGLUNIFORM4UIPROC glad_glUniform4ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    _pre_call_gles2_callback("glUniform4ui", (GLADapiproc) glad_glUniform4ui, 5, location, v0, v1, v2, v3);
    glad_glUniform4ui(location, v0, v1, v2, v3);
    _post_call_gles2_callback(NULL, "glUniform4ui", (GLADapiproc) glad_glUniform4ui, 5, location, v0, v1, v2, v3);
    
}
PFNGLUNIFORM4UIPROC glad_debug_glUniform4ui = glad_debug_impl_glUniform4ui;
PFNGLUNIFORM4UI64NVPROC glad_glUniform4ui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4ui64NV(GLint location, GLuint64EXT x, GLuint64EXT y, GLuint64EXT z, GLuint64EXT w) {
    _pre_call_gles2_callback("glUniform4ui64NV", (GLADapiproc) glad_glUniform4ui64NV, 5, location, x, y, z, w);
    glad_glUniform4ui64NV(location, x, y, z, w);
    _post_call_gles2_callback(NULL, "glUniform4ui64NV", (GLADapiproc) glad_glUniform4ui64NV, 5, location, x, y, z, w);
    
}
PFNGLUNIFORM4UI64NVPROC glad_debug_glUniform4ui64NV = glad_debug_impl_glUniform4ui64NV;
PFNGLUNIFORM4UI64VNVPROC glad_glUniform4ui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4ui64vNV(GLint location, GLsizei count, const GLuint64EXT * value) {
    _pre_call_gles2_callback("glUniform4ui64vNV", (GLADapiproc) glad_glUniform4ui64vNV, 3, location, count, value);
    glad_glUniform4ui64vNV(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform4ui64vNV", (GLADapiproc) glad_glUniform4ui64vNV, 3, location, count, value);
    
}
PFNGLUNIFORM4UI64VNVPROC glad_debug_glUniform4ui64vNV = glad_debug_impl_glUniform4ui64vNV;
PFNGLUNIFORM4UIVPROC glad_glUniform4uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniform4uiv(GLint location, GLsizei count, const GLuint * value) {
    _pre_call_gles2_callback("glUniform4uiv", (GLADapiproc) glad_glUniform4uiv, 3, location, count, value);
    glad_glUniform4uiv(location, count, value);
    _post_call_gles2_callback(NULL, "glUniform4uiv", (GLADapiproc) glad_glUniform4uiv, 3, location, count, value);
    
}
PFNGLUNIFORM4UIVPROC glad_debug_glUniform4uiv = glad_debug_impl_glUniform4uiv;
PFNGLUNIFORMBLOCKBINDINGPROC glad_glUniformBlockBinding = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
    _pre_call_gles2_callback("glUniformBlockBinding", (GLADapiproc) glad_glUniformBlockBinding, 3, program, uniformBlockIndex, uniformBlockBinding);
    glad_glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
    _post_call_gles2_callback(NULL, "glUniformBlockBinding", (GLADapiproc) glad_glUniformBlockBinding, 3, program, uniformBlockIndex, uniformBlockBinding);
    
}
PFNGLUNIFORMBLOCKBINDINGPROC glad_debug_glUniformBlockBinding = glad_debug_impl_glUniformBlockBinding;
PFNGLUNIFORMHANDLEUI64IMGPROC glad_glUniformHandleui64IMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformHandleui64IMG(GLint location, GLuint64 value) {
    _pre_call_gles2_callback("glUniformHandleui64IMG", (GLADapiproc) glad_glUniformHandleui64IMG, 2, location, value);
    glad_glUniformHandleui64IMG(location, value);
    _post_call_gles2_callback(NULL, "glUniformHandleui64IMG", (GLADapiproc) glad_glUniformHandleui64IMG, 2, location, value);
    
}
PFNGLUNIFORMHANDLEUI64IMGPROC glad_debug_glUniformHandleui64IMG = glad_debug_impl_glUniformHandleui64IMG;
PFNGLUNIFORMHANDLEUI64NVPROC glad_glUniformHandleui64NV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformHandleui64NV(GLint location, GLuint64 value) {
    _pre_call_gles2_callback("glUniformHandleui64NV", (GLADapiproc) glad_glUniformHandleui64NV, 2, location, value);
    glad_glUniformHandleui64NV(location, value);
    _post_call_gles2_callback(NULL, "glUniformHandleui64NV", (GLADapiproc) glad_glUniformHandleui64NV, 2, location, value);
    
}
PFNGLUNIFORMHANDLEUI64NVPROC glad_debug_glUniformHandleui64NV = glad_debug_impl_glUniformHandleui64NV;
PFNGLUNIFORMHANDLEUI64VIMGPROC glad_glUniformHandleui64vIMG = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformHandleui64vIMG(GLint location, GLsizei count, const GLuint64 * value) {
    _pre_call_gles2_callback("glUniformHandleui64vIMG", (GLADapiproc) glad_glUniformHandleui64vIMG, 3, location, count, value);
    glad_glUniformHandleui64vIMG(location, count, value);
    _post_call_gles2_callback(NULL, "glUniformHandleui64vIMG", (GLADapiproc) glad_glUniformHandleui64vIMG, 3, location, count, value);
    
}
PFNGLUNIFORMHANDLEUI64VIMGPROC glad_debug_glUniformHandleui64vIMG = glad_debug_impl_glUniformHandleui64vIMG;
PFNGLUNIFORMHANDLEUI64VNVPROC glad_glUniformHandleui64vNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformHandleui64vNV(GLint location, GLsizei count, const GLuint64 * value) {
    _pre_call_gles2_callback("glUniformHandleui64vNV", (GLADapiproc) glad_glUniformHandleui64vNV, 3, location, count, value);
    glad_glUniformHandleui64vNV(location, count, value);
    _post_call_gles2_callback(NULL, "glUniformHandleui64vNV", (GLADapiproc) glad_glUniformHandleui64vNV, 3, location, count, value);
    
}
PFNGLUNIFORMHANDLEUI64VNVPROC glad_debug_glUniformHandleui64vNV = glad_debug_impl_glUniformHandleui64vNV;
PFNGLUNIFORMMATRIX2FVPROC glad_glUniformMatrix2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix2fv", (GLADapiproc) glad_glUniformMatrix2fv, 4, location, count, transpose, value);
    glad_glUniformMatrix2fv(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix2fv", (GLADapiproc) glad_glUniformMatrix2fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX2FVPROC glad_debug_glUniformMatrix2fv = glad_debug_impl_glUniformMatrix2fv;
PFNGLUNIFORMMATRIX2X3FVPROC glad_glUniformMatrix2x3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix2x3fv", (GLADapiproc) glad_glUniformMatrix2x3fv, 4, location, count, transpose, value);
    glad_glUniformMatrix2x3fv(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix2x3fv", (GLADapiproc) glad_glUniformMatrix2x3fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX2X3FVPROC glad_debug_glUniformMatrix2x3fv = glad_debug_impl_glUniformMatrix2x3fv;
PFNGLUNIFORMMATRIX2X3FVNVPROC glad_glUniformMatrix2x3fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix2x3fvNV(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix2x3fvNV", (GLADapiproc) glad_glUniformMatrix2x3fvNV, 4, location, count, transpose, value);
    glad_glUniformMatrix2x3fvNV(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix2x3fvNV", (GLADapiproc) glad_glUniformMatrix2x3fvNV, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX2X3FVNVPROC glad_debug_glUniformMatrix2x3fvNV = glad_debug_impl_glUniformMatrix2x3fvNV;
PFNGLUNIFORMMATRIX2X4FVPROC glad_glUniformMatrix2x4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix2x4fv", (GLADapiproc) glad_glUniformMatrix2x4fv, 4, location, count, transpose, value);
    glad_glUniformMatrix2x4fv(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix2x4fv", (GLADapiproc) glad_glUniformMatrix2x4fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX2X4FVPROC glad_debug_glUniformMatrix2x4fv = glad_debug_impl_glUniformMatrix2x4fv;
PFNGLUNIFORMMATRIX2X4FVNVPROC glad_glUniformMatrix2x4fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix2x4fvNV(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix2x4fvNV", (GLADapiproc) glad_glUniformMatrix2x4fvNV, 4, location, count, transpose, value);
    glad_glUniformMatrix2x4fvNV(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix2x4fvNV", (GLADapiproc) glad_glUniformMatrix2x4fvNV, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX2X4FVNVPROC glad_debug_glUniformMatrix2x4fvNV = glad_debug_impl_glUniformMatrix2x4fvNV;
PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix3fv", (GLADapiproc) glad_glUniformMatrix3fv, 4, location, count, transpose, value);
    glad_glUniformMatrix3fv(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix3fv", (GLADapiproc) glad_glUniformMatrix3fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX3FVPROC glad_debug_glUniformMatrix3fv = glad_debug_impl_glUniformMatrix3fv;
PFNGLUNIFORMMATRIX3X2FVPROC glad_glUniformMatrix3x2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix3x2fv", (GLADapiproc) glad_glUniformMatrix3x2fv, 4, location, count, transpose, value);
    glad_glUniformMatrix3x2fv(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix3x2fv", (GLADapiproc) glad_glUniformMatrix3x2fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX3X2FVPROC glad_debug_glUniformMatrix3x2fv = glad_debug_impl_glUniformMatrix3x2fv;
PFNGLUNIFORMMATRIX3X2FVNVPROC glad_glUniformMatrix3x2fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix3x2fvNV(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix3x2fvNV", (GLADapiproc) glad_glUniformMatrix3x2fvNV, 4, location, count, transpose, value);
    glad_glUniformMatrix3x2fvNV(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix3x2fvNV", (GLADapiproc) glad_glUniformMatrix3x2fvNV, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX3X2FVNVPROC glad_debug_glUniformMatrix3x2fvNV = glad_debug_impl_glUniformMatrix3x2fvNV;
PFNGLUNIFORMMATRIX3X4FVPROC glad_glUniformMatrix3x4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix3x4fv", (GLADapiproc) glad_glUniformMatrix3x4fv, 4, location, count, transpose, value);
    glad_glUniformMatrix3x4fv(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix3x4fv", (GLADapiproc) glad_glUniformMatrix3x4fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX3X4FVPROC glad_debug_glUniformMatrix3x4fv = glad_debug_impl_glUniformMatrix3x4fv;
PFNGLUNIFORMMATRIX3X4FVNVPROC glad_glUniformMatrix3x4fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix3x4fvNV(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix3x4fvNV", (GLADapiproc) glad_glUniformMatrix3x4fvNV, 4, location, count, transpose, value);
    glad_glUniformMatrix3x4fvNV(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix3x4fvNV", (GLADapiproc) glad_glUniformMatrix3x4fvNV, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX3X4FVNVPROC glad_debug_glUniformMatrix3x4fvNV = glad_debug_impl_glUniformMatrix3x4fvNV;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix4fv", (GLADapiproc) glad_glUniformMatrix4fv, 4, location, count, transpose, value);
    glad_glUniformMatrix4fv(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix4fv", (GLADapiproc) glad_glUniformMatrix4fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX4FVPROC glad_debug_glUniformMatrix4fv = glad_debug_impl_glUniformMatrix4fv;
PFNGLUNIFORMMATRIX4X2FVPROC glad_glUniformMatrix4x2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix4x2fv", (GLADapiproc) glad_glUniformMatrix4x2fv, 4, location, count, transpose, value);
    glad_glUniformMatrix4x2fv(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix4x2fv", (GLADapiproc) glad_glUniformMatrix4x2fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX4X2FVPROC glad_debug_glUniformMatrix4x2fv = glad_debug_impl_glUniformMatrix4x2fv;
PFNGLUNIFORMMATRIX4X2FVNVPROC glad_glUniformMatrix4x2fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix4x2fvNV(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix4x2fvNV", (GLADapiproc) glad_glUniformMatrix4x2fvNV, 4, location, count, transpose, value);
    glad_glUniformMatrix4x2fvNV(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix4x2fvNV", (GLADapiproc) glad_glUniformMatrix4x2fvNV, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX4X2FVNVPROC glad_debug_glUniformMatrix4x2fvNV = glad_debug_impl_glUniformMatrix4x2fvNV;
PFNGLUNIFORMMATRIX4X3FVPROC glad_glUniformMatrix4x3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix4x3fv", (GLADapiproc) glad_glUniformMatrix4x3fv, 4, location, count, transpose, value);
    glad_glUniformMatrix4x3fv(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix4x3fv", (GLADapiproc) glad_glUniformMatrix4x3fv, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX4X3FVPROC glad_debug_glUniformMatrix4x3fv = glad_debug_impl_glUniformMatrix4x3fv;
PFNGLUNIFORMMATRIX4X3FVNVPROC glad_glUniformMatrix4x3fvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glUniformMatrix4x3fvNV(GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) {
    _pre_call_gles2_callback("glUniformMatrix4x3fvNV", (GLADapiproc) glad_glUniformMatrix4x3fvNV, 4, location, count, transpose, value);
    glad_glUniformMatrix4x3fvNV(location, count, transpose, value);
    _post_call_gles2_callback(NULL, "glUniformMatrix4x3fvNV", (GLADapiproc) glad_glUniformMatrix4x3fvNV, 4, location, count, transpose, value);
    
}
PFNGLUNIFORMMATRIX4X3FVNVPROC glad_debug_glUniformMatrix4x3fvNV = glad_debug_impl_glUniformMatrix4x3fvNV;
PFNGLUNMAPBUFFERPROC glad_glUnmapBuffer = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glUnmapBuffer(GLenum target) {
    GLboolean ret;
    _pre_call_gles2_callback("glUnmapBuffer", (GLADapiproc) glad_glUnmapBuffer, 1, target);
    ret = glad_glUnmapBuffer(target);
    _post_call_gles2_callback((void*) &ret, "glUnmapBuffer", (GLADapiproc) glad_glUnmapBuffer, 1, target);
    return ret;
}
PFNGLUNMAPBUFFERPROC glad_debug_glUnmapBuffer = glad_debug_impl_glUnmapBuffer;
PFNGLUNMAPBUFFEROESPROC glad_glUnmapBufferOES = NULL;
static GLboolean GLAD_API_PTR glad_debug_impl_glUnmapBufferOES(GLenum target) {
    GLboolean ret;
    _pre_call_gles2_callback("glUnmapBufferOES", (GLADapiproc) glad_glUnmapBufferOES, 1, target);
    ret = glad_glUnmapBufferOES(target);
    _post_call_gles2_callback((void*) &ret, "glUnmapBufferOES", (GLADapiproc) glad_glUnmapBufferOES, 1, target);
    return ret;
}
PFNGLUNMAPBUFFEROESPROC glad_debug_glUnmapBufferOES = glad_debug_impl_glUnmapBufferOES;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
static void GLAD_API_PTR glad_debug_impl_glUseProgram(GLuint program) {
    _pre_call_gles2_callback("glUseProgram", (GLADapiproc) glad_glUseProgram, 1, program);
    glad_glUseProgram(program);
    _post_call_gles2_callback(NULL, "glUseProgram", (GLADapiproc) glad_glUseProgram, 1, program);
    
}
PFNGLUSEPROGRAMPROC glad_debug_glUseProgram = glad_debug_impl_glUseProgram;
PFNGLUSEPROGRAMSTAGESPROC glad_glUseProgramStages = NULL;
static void GLAD_API_PTR glad_debug_impl_glUseProgramStages(GLuint pipeline, GLbitfield stages, GLuint program) {
    _pre_call_gles2_callback("glUseProgramStages", (GLADapiproc) glad_glUseProgramStages, 3, pipeline, stages, program);
    glad_glUseProgramStages(pipeline, stages, program);
    _post_call_gles2_callback(NULL, "glUseProgramStages", (GLADapiproc) glad_glUseProgramStages, 3, pipeline, stages, program);
    
}
PFNGLUSEPROGRAMSTAGESPROC glad_debug_glUseProgramStages = glad_debug_impl_glUseProgramStages;
PFNGLUSEPROGRAMSTAGESEXTPROC glad_glUseProgramStagesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glUseProgramStagesEXT(GLuint pipeline, GLbitfield stages, GLuint program) {
    _pre_call_gles2_callback("glUseProgramStagesEXT", (GLADapiproc) glad_glUseProgramStagesEXT, 3, pipeline, stages, program);
    glad_glUseProgramStagesEXT(pipeline, stages, program);
    _post_call_gles2_callback(NULL, "glUseProgramStagesEXT", (GLADapiproc) glad_glUseProgramStagesEXT, 3, pipeline, stages, program);
    
}
PFNGLUSEPROGRAMSTAGESEXTPROC glad_debug_glUseProgramStagesEXT = glad_debug_impl_glUseProgramStagesEXT;
PFNGLVALIDATEPROGRAMPROC glad_glValidateProgram = NULL;
static void GLAD_API_PTR glad_debug_impl_glValidateProgram(GLuint program) {
    _pre_call_gles2_callback("glValidateProgram", (GLADapiproc) glad_glValidateProgram, 1, program);
    glad_glValidateProgram(program);
    _post_call_gles2_callback(NULL, "glValidateProgram", (GLADapiproc) glad_glValidateProgram, 1, program);
    
}
PFNGLVALIDATEPROGRAMPROC glad_debug_glValidateProgram = glad_debug_impl_glValidateProgram;
PFNGLVALIDATEPROGRAMPIPELINEPROC glad_glValidateProgramPipeline = NULL;
static void GLAD_API_PTR glad_debug_impl_glValidateProgramPipeline(GLuint pipeline) {
    _pre_call_gles2_callback("glValidateProgramPipeline", (GLADapiproc) glad_glValidateProgramPipeline, 1, pipeline);
    glad_glValidateProgramPipeline(pipeline);
    _post_call_gles2_callback(NULL, "glValidateProgramPipeline", (GLADapiproc) glad_glValidateProgramPipeline, 1, pipeline);
    
}
PFNGLVALIDATEPROGRAMPIPELINEPROC glad_debug_glValidateProgramPipeline = glad_debug_impl_glValidateProgramPipeline;
PFNGLVALIDATEPROGRAMPIPELINEEXTPROC glad_glValidateProgramPipelineEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glValidateProgramPipelineEXT(GLuint pipeline) {
    _pre_call_gles2_callback("glValidateProgramPipelineEXT", (GLADapiproc) glad_glValidateProgramPipelineEXT, 1, pipeline);
    glad_glValidateProgramPipelineEXT(pipeline);
    _post_call_gles2_callback(NULL, "glValidateProgramPipelineEXT", (GLADapiproc) glad_glValidateProgramPipelineEXT, 1, pipeline);
    
}
PFNGLVALIDATEPROGRAMPIPELINEEXTPROC glad_debug_glValidateProgramPipelineEXT = glad_debug_impl_glValidateProgramPipelineEXT;
PFNGLVERTEXATTRIB1FPROC glad_glVertexAttrib1f = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1f(GLuint index, GLfloat x) {
    _pre_call_gles2_callback("glVertexAttrib1f", (GLADapiproc) glad_glVertexAttrib1f, 2, index, x);
    glad_glVertexAttrib1f(index, x);
    _post_call_gles2_callback(NULL, "glVertexAttrib1f", (GLADapiproc) glad_glVertexAttrib1f, 2, index, x);
    
}
PFNGLVERTEXATTRIB1FPROC glad_debug_glVertexAttrib1f = glad_debug_impl_glVertexAttrib1f;
PFNGLVERTEXATTRIB1FVPROC glad_glVertexAttrib1fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib1fv(GLuint index, const GLfloat * v) {
    _pre_call_gles2_callback("glVertexAttrib1fv", (GLADapiproc) glad_glVertexAttrib1fv, 2, index, v);
    glad_glVertexAttrib1fv(index, v);
    _post_call_gles2_callback(NULL, "glVertexAttrib1fv", (GLADapiproc) glad_glVertexAttrib1fv, 2, index, v);
    
}
PFNGLVERTEXATTRIB1FVPROC glad_debug_glVertexAttrib1fv = glad_debug_impl_glVertexAttrib1fv;
PFNGLVERTEXATTRIB2FPROC glad_glVertexAttrib2f = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    _pre_call_gles2_callback("glVertexAttrib2f", (GLADapiproc) glad_glVertexAttrib2f, 3, index, x, y);
    glad_glVertexAttrib2f(index, x, y);
    _post_call_gles2_callback(NULL, "glVertexAttrib2f", (GLADapiproc) glad_glVertexAttrib2f, 3, index, x, y);
    
}
PFNGLVERTEXATTRIB2FPROC glad_debug_glVertexAttrib2f = glad_debug_impl_glVertexAttrib2f;
PFNGLVERTEXATTRIB2FVPROC glad_glVertexAttrib2fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib2fv(GLuint index, const GLfloat * v) {
    _pre_call_gles2_callback("glVertexAttrib2fv", (GLADapiproc) glad_glVertexAttrib2fv, 2, index, v);
    glad_glVertexAttrib2fv(index, v);
    _post_call_gles2_callback(NULL, "glVertexAttrib2fv", (GLADapiproc) glad_glVertexAttrib2fv, 2, index, v);
    
}
PFNGLVERTEXATTRIB2FVPROC glad_debug_glVertexAttrib2fv = glad_debug_impl_glVertexAttrib2fv;
PFNGLVERTEXATTRIB3FPROC glad_glVertexAttrib3f = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    _pre_call_gles2_callback("glVertexAttrib3f", (GLADapiproc) glad_glVertexAttrib3f, 4, index, x, y, z);
    glad_glVertexAttrib3f(index, x, y, z);
    _post_call_gles2_callback(NULL, "glVertexAttrib3f", (GLADapiproc) glad_glVertexAttrib3f, 4, index, x, y, z);
    
}
PFNGLVERTEXATTRIB3FPROC glad_debug_glVertexAttrib3f = glad_debug_impl_glVertexAttrib3f;
PFNGLVERTEXATTRIB3FVPROC glad_glVertexAttrib3fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib3fv(GLuint index, const GLfloat * v) {
    _pre_call_gles2_callback("glVertexAttrib3fv", (GLADapiproc) glad_glVertexAttrib3fv, 2, index, v);
    glad_glVertexAttrib3fv(index, v);
    _post_call_gles2_callback(NULL, "glVertexAttrib3fv", (GLADapiproc) glad_glVertexAttrib3fv, 2, index, v);
    
}
PFNGLVERTEXATTRIB3FVPROC glad_debug_glVertexAttrib3fv = glad_debug_impl_glVertexAttrib3fv;
PFNGLVERTEXATTRIB4FPROC glad_glVertexAttrib4f = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    _pre_call_gles2_callback("glVertexAttrib4f", (GLADapiproc) glad_glVertexAttrib4f, 5, index, x, y, z, w);
    glad_glVertexAttrib4f(index, x, y, z, w);
    _post_call_gles2_callback(NULL, "glVertexAttrib4f", (GLADapiproc) glad_glVertexAttrib4f, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIB4FPROC glad_debug_glVertexAttrib4f = glad_debug_impl_glVertexAttrib4f;
PFNGLVERTEXATTRIB4FVPROC glad_glVertexAttrib4fv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttrib4fv(GLuint index, const GLfloat * v) {
    _pre_call_gles2_callback("glVertexAttrib4fv", (GLADapiproc) glad_glVertexAttrib4fv, 2, index, v);
    glad_glVertexAttrib4fv(index, v);
    _post_call_gles2_callback(NULL, "glVertexAttrib4fv", (GLADapiproc) glad_glVertexAttrib4fv, 2, index, v);
    
}
PFNGLVERTEXATTRIB4FVPROC glad_debug_glVertexAttrib4fv = glad_debug_impl_glVertexAttrib4fv;
PFNGLVERTEXATTRIBBINDINGPROC glad_glVertexAttribBinding = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribBinding(GLuint attribindex, GLuint bindingindex) {
    _pre_call_gles2_callback("glVertexAttribBinding", (GLADapiproc) glad_glVertexAttribBinding, 2, attribindex, bindingindex);
    glad_glVertexAttribBinding(attribindex, bindingindex);
    _post_call_gles2_callback(NULL, "glVertexAttribBinding", (GLADapiproc) glad_glVertexAttribBinding, 2, attribindex, bindingindex);
    
}
PFNGLVERTEXATTRIBBINDINGPROC glad_debug_glVertexAttribBinding = glad_debug_impl_glVertexAttribBinding;
PFNGLVERTEXATTRIBDIVISORPROC glad_glVertexAttribDivisor = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribDivisor(GLuint index, GLuint divisor) {
    _pre_call_gles2_callback("glVertexAttribDivisor", (GLADapiproc) glad_glVertexAttribDivisor, 2, index, divisor);
    glad_glVertexAttribDivisor(index, divisor);
    _post_call_gles2_callback(NULL, "glVertexAttribDivisor", (GLADapiproc) glad_glVertexAttribDivisor, 2, index, divisor);
    
}
PFNGLVERTEXATTRIBDIVISORPROC glad_debug_glVertexAttribDivisor = glad_debug_impl_glVertexAttribDivisor;
PFNGLVERTEXATTRIBDIVISORANGLEPROC glad_glVertexAttribDivisorANGLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
    _pre_call_gles2_callback("glVertexAttribDivisorANGLE", (GLADapiproc) glad_glVertexAttribDivisorANGLE, 2, index, divisor);
    glad_glVertexAttribDivisorANGLE(index, divisor);
    _post_call_gles2_callback(NULL, "glVertexAttribDivisorANGLE", (GLADapiproc) glad_glVertexAttribDivisorANGLE, 2, index, divisor);
    
}
PFNGLVERTEXATTRIBDIVISORANGLEPROC glad_debug_glVertexAttribDivisorANGLE = glad_debug_impl_glVertexAttribDivisorANGLE;
PFNGLVERTEXATTRIBDIVISOREXTPROC glad_glVertexAttribDivisorEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribDivisorEXT(GLuint index, GLuint divisor) {
    _pre_call_gles2_callback("glVertexAttribDivisorEXT", (GLADapiproc) glad_glVertexAttribDivisorEXT, 2, index, divisor);
    glad_glVertexAttribDivisorEXT(index, divisor);
    _post_call_gles2_callback(NULL, "glVertexAttribDivisorEXT", (GLADapiproc) glad_glVertexAttribDivisorEXT, 2, index, divisor);
    
}
PFNGLVERTEXATTRIBDIVISOREXTPROC glad_debug_glVertexAttribDivisorEXT = glad_debug_impl_glVertexAttribDivisorEXT;
PFNGLVERTEXATTRIBDIVISORNVPROC glad_glVertexAttribDivisorNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribDivisorNV(GLuint index, GLuint divisor) {
    _pre_call_gles2_callback("glVertexAttribDivisorNV", (GLADapiproc) glad_glVertexAttribDivisorNV, 2, index, divisor);
    glad_glVertexAttribDivisorNV(index, divisor);
    _post_call_gles2_callback(NULL, "glVertexAttribDivisorNV", (GLADapiproc) glad_glVertexAttribDivisorNV, 2, index, divisor);
    
}
PFNGLVERTEXATTRIBDIVISORNVPROC glad_debug_glVertexAttribDivisorNV = glad_debug_impl_glVertexAttribDivisorNV;
PFNGLVERTEXATTRIBFORMATPROC glad_glVertexAttribFormat = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribFormat(GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    _pre_call_gles2_callback("glVertexAttribFormat", (GLADapiproc) glad_glVertexAttribFormat, 5, attribindex, size, type, normalized, relativeoffset);
    glad_glVertexAttribFormat(attribindex, size, type, normalized, relativeoffset);
    _post_call_gles2_callback(NULL, "glVertexAttribFormat", (GLADapiproc) glad_glVertexAttribFormat, 5, attribindex, size, type, normalized, relativeoffset);
    
}
PFNGLVERTEXATTRIBFORMATPROC glad_debug_glVertexAttribFormat = glad_debug_impl_glVertexAttribFormat;
PFNGLVERTEXATTRIBI4IPROC glad_glVertexAttribI4i = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    _pre_call_gles2_callback("glVertexAttribI4i", (GLADapiproc) glad_glVertexAttribI4i, 5, index, x, y, z, w);
    glad_glVertexAttribI4i(index, x, y, z, w);
    _post_call_gles2_callback(NULL, "glVertexAttribI4i", (GLADapiproc) glad_glVertexAttribI4i, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIBI4IPROC glad_debug_glVertexAttribI4i = glad_debug_impl_glVertexAttribI4i;
PFNGLVERTEXATTRIBI4IVPROC glad_glVertexAttribI4iv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4iv(GLuint index, const GLint * v) {
    _pre_call_gles2_callback("glVertexAttribI4iv", (GLADapiproc) glad_glVertexAttribI4iv, 2, index, v);
    glad_glVertexAttribI4iv(index, v);
    _post_call_gles2_callback(NULL, "glVertexAttribI4iv", (GLADapiproc) glad_glVertexAttribI4iv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4IVPROC glad_debug_glVertexAttribI4iv = glad_debug_impl_glVertexAttribI4iv;
PFNGLVERTEXATTRIBI4UIPROC glad_glVertexAttribI4ui = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    _pre_call_gles2_callback("glVertexAttribI4ui", (GLADapiproc) glad_glVertexAttribI4ui, 5, index, x, y, z, w);
    glad_glVertexAttribI4ui(index, x, y, z, w);
    _post_call_gles2_callback(NULL, "glVertexAttribI4ui", (GLADapiproc) glad_glVertexAttribI4ui, 5, index, x, y, z, w);
    
}
PFNGLVERTEXATTRIBI4UIPROC glad_debug_glVertexAttribI4ui = glad_debug_impl_glVertexAttribI4ui;
PFNGLVERTEXATTRIBI4UIVPROC glad_glVertexAttribI4uiv = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribI4uiv(GLuint index, const GLuint * v) {
    _pre_call_gles2_callback("glVertexAttribI4uiv", (GLADapiproc) glad_glVertexAttribI4uiv, 2, index, v);
    glad_glVertexAttribI4uiv(index, v);
    _post_call_gles2_callback(NULL, "glVertexAttribI4uiv", (GLADapiproc) glad_glVertexAttribI4uiv, 2, index, v);
    
}
PFNGLVERTEXATTRIBI4UIVPROC glad_debug_glVertexAttribI4uiv = glad_debug_impl_glVertexAttribI4uiv;
PFNGLVERTEXATTRIBIFORMATPROC glad_glVertexAttribIFormat = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribIFormat(GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) {
    _pre_call_gles2_callback("glVertexAttribIFormat", (GLADapiproc) glad_glVertexAttribIFormat, 4, attribindex, size, type, relativeoffset);
    glad_glVertexAttribIFormat(attribindex, size, type, relativeoffset);
    _post_call_gles2_callback(NULL, "glVertexAttribIFormat", (GLADapiproc) glad_glVertexAttribIFormat, 4, attribindex, size, type, relativeoffset);
    
}
PFNGLVERTEXATTRIBIFORMATPROC glad_debug_glVertexAttribIFormat = glad_debug_impl_glVertexAttribIFormat;
PFNGLVERTEXATTRIBIPOINTERPROC glad_glVertexAttribIPointer = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void * pointer) {
    _pre_call_gles2_callback("glVertexAttribIPointer", (GLADapiproc) glad_glVertexAttribIPointer, 5, index, size, type, stride, pointer);
    glad_glVertexAttribIPointer(index, size, type, stride, pointer);
    _post_call_gles2_callback(NULL, "glVertexAttribIPointer", (GLADapiproc) glad_glVertexAttribIPointer, 5, index, size, type, stride, pointer);
    
}
PFNGLVERTEXATTRIBIPOINTERPROC glad_debug_glVertexAttribIPointer = glad_debug_impl_glVertexAttribIPointer;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer) {
    _pre_call_gles2_callback("glVertexAttribPointer", (GLADapiproc) glad_glVertexAttribPointer, 6, index, size, type, normalized, stride, pointer);
    glad_glVertexAttribPointer(index, size, type, normalized, stride, pointer);
    _post_call_gles2_callback(NULL, "glVertexAttribPointer", (GLADapiproc) glad_glVertexAttribPointer, 6, index, size, type, normalized, stride, pointer);
    
}
PFNGLVERTEXATTRIBPOINTERPROC glad_debug_glVertexAttribPointer = glad_debug_impl_glVertexAttribPointer;
PFNGLVERTEXBINDINGDIVISORPROC glad_glVertexBindingDivisor = NULL;
static void GLAD_API_PTR glad_debug_impl_glVertexBindingDivisor(GLuint bindingindex, GLuint divisor) {
    _pre_call_gles2_callback("glVertexBindingDivisor", (GLADapiproc) glad_glVertexBindingDivisor, 2, bindingindex, divisor);
    glad_glVertexBindingDivisor(bindingindex, divisor);
    _post_call_gles2_callback(NULL, "glVertexBindingDivisor", (GLADapiproc) glad_glVertexBindingDivisor, 2, bindingindex, divisor);
    
}
PFNGLVERTEXBINDINGDIVISORPROC glad_debug_glVertexBindingDivisor = glad_debug_impl_glVertexBindingDivisor;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    _pre_call_gles2_callback("glViewport", (GLADapiproc) glad_glViewport, 4, x, y, width, height);
    glad_glViewport(x, y, width, height);
    _post_call_gles2_callback(NULL, "glViewport", (GLADapiproc) glad_glViewport, 4, x, y, width, height);
    
}
PFNGLVIEWPORTPROC glad_debug_glViewport = glad_debug_impl_glViewport;
PFNGLVIEWPORTARRAYVNVPROC glad_glViewportArrayvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewportArrayvNV(GLuint first, GLsizei count, const GLfloat * v) {
    _pre_call_gles2_callback("glViewportArrayvNV", (GLADapiproc) glad_glViewportArrayvNV, 3, first, count, v);
    glad_glViewportArrayvNV(first, count, v);
    _post_call_gles2_callback(NULL, "glViewportArrayvNV", (GLADapiproc) glad_glViewportArrayvNV, 3, first, count, v);
    
}
PFNGLVIEWPORTARRAYVNVPROC glad_debug_glViewportArrayvNV = glad_debug_impl_glViewportArrayvNV;
PFNGLVIEWPORTARRAYVOESPROC glad_glViewportArrayvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewportArrayvOES(GLuint first, GLsizei count, const GLfloat * v) {
    _pre_call_gles2_callback("glViewportArrayvOES", (GLADapiproc) glad_glViewportArrayvOES, 3, first, count, v);
    glad_glViewportArrayvOES(first, count, v);
    _post_call_gles2_callback(NULL, "glViewportArrayvOES", (GLADapiproc) glad_glViewportArrayvOES, 3, first, count, v);
    
}
PFNGLVIEWPORTARRAYVOESPROC glad_debug_glViewportArrayvOES = glad_debug_impl_glViewportArrayvOES;
PFNGLVIEWPORTINDEXEDFNVPROC glad_glViewportIndexedfNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewportIndexedfNV(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
    _pre_call_gles2_callback("glViewportIndexedfNV", (GLADapiproc) glad_glViewportIndexedfNV, 5, index, x, y, w, h);
    glad_glViewportIndexedfNV(index, x, y, w, h);
    _post_call_gles2_callback(NULL, "glViewportIndexedfNV", (GLADapiproc) glad_glViewportIndexedfNV, 5, index, x, y, w, h);
    
}
PFNGLVIEWPORTINDEXEDFNVPROC glad_debug_glViewportIndexedfNV = glad_debug_impl_glViewportIndexedfNV;
PFNGLVIEWPORTINDEXEDFOESPROC glad_glViewportIndexedfOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewportIndexedfOES(GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
    _pre_call_gles2_callback("glViewportIndexedfOES", (GLADapiproc) glad_glViewportIndexedfOES, 5, index, x, y, w, h);
    glad_glViewportIndexedfOES(index, x, y, w, h);
    _post_call_gles2_callback(NULL, "glViewportIndexedfOES", (GLADapiproc) glad_glViewportIndexedfOES, 5, index, x, y, w, h);
    
}
PFNGLVIEWPORTINDEXEDFOESPROC glad_debug_glViewportIndexedfOES = glad_debug_impl_glViewportIndexedfOES;
PFNGLVIEWPORTINDEXEDFVNVPROC glad_glViewportIndexedfvNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewportIndexedfvNV(GLuint index, const GLfloat * v) {
    _pre_call_gles2_callback("glViewportIndexedfvNV", (GLADapiproc) glad_glViewportIndexedfvNV, 2, index, v);
    glad_glViewportIndexedfvNV(index, v);
    _post_call_gles2_callback(NULL, "glViewportIndexedfvNV", (GLADapiproc) glad_glViewportIndexedfvNV, 2, index, v);
    
}
PFNGLVIEWPORTINDEXEDFVNVPROC glad_debug_glViewportIndexedfvNV = glad_debug_impl_glViewportIndexedfvNV;
PFNGLVIEWPORTINDEXEDFVOESPROC glad_glViewportIndexedfvOES = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewportIndexedfvOES(GLuint index, const GLfloat * v) {
    _pre_call_gles2_callback("glViewportIndexedfvOES", (GLADapiproc) glad_glViewportIndexedfvOES, 2, index, v);
    glad_glViewportIndexedfvOES(index, v);
    _post_call_gles2_callback(NULL, "glViewportIndexedfvOES", (GLADapiproc) glad_glViewportIndexedfvOES, 2, index, v);
    
}
PFNGLVIEWPORTINDEXEDFVOESPROC glad_debug_glViewportIndexedfvOES = glad_debug_impl_glViewportIndexedfvOES;
PFNGLVIEWPORTPOSITIONWSCALENVPROC glad_glViewportPositionWScaleNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewportPositionWScaleNV(GLuint index, GLfloat xcoeff, GLfloat ycoeff) {
    _pre_call_gles2_callback("glViewportPositionWScaleNV", (GLADapiproc) glad_glViewportPositionWScaleNV, 3, index, xcoeff, ycoeff);
    glad_glViewportPositionWScaleNV(index, xcoeff, ycoeff);
    _post_call_gles2_callback(NULL, "glViewportPositionWScaleNV", (GLADapiproc) glad_glViewportPositionWScaleNV, 3, index, xcoeff, ycoeff);
    
}
PFNGLVIEWPORTPOSITIONWSCALENVPROC glad_debug_glViewportPositionWScaleNV = glad_debug_impl_glViewportPositionWScaleNV;
PFNGLVIEWPORTSWIZZLENVPROC glad_glViewportSwizzleNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glViewportSwizzleNV(GLuint index, GLenum swizzlex, GLenum swizzley, GLenum swizzlez, GLenum swizzlew) {
    _pre_call_gles2_callback("glViewportSwizzleNV", (GLADapiproc) glad_glViewportSwizzleNV, 5, index, swizzlex, swizzley, swizzlez, swizzlew);
    glad_glViewportSwizzleNV(index, swizzlex, swizzley, swizzlez, swizzlew);
    _post_call_gles2_callback(NULL, "glViewportSwizzleNV", (GLADapiproc) glad_glViewportSwizzleNV, 5, index, swizzlex, swizzley, swizzlez, swizzlew);
    
}
PFNGLVIEWPORTSWIZZLENVPROC glad_debug_glViewportSwizzleNV = glad_debug_impl_glViewportSwizzleNV;
PFNGLWAITSEMAPHOREEXTPROC glad_glWaitSemaphoreEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glWaitSemaphoreEXT(GLuint semaphore, GLuint numBufferBarriers, const GLuint * buffers, GLuint numTextureBarriers, const GLuint * textures, const GLenum * srcLayouts) {
    _pre_call_gles2_callback("glWaitSemaphoreEXT", (GLADapiproc) glad_glWaitSemaphoreEXT, 6, semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, srcLayouts);
    glad_glWaitSemaphoreEXT(semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, srcLayouts);
    _post_call_gles2_callback(NULL, "glWaitSemaphoreEXT", (GLADapiproc) glad_glWaitSemaphoreEXT, 6, semaphore, numBufferBarriers, buffers, numTextureBarriers, textures, srcLayouts);
    
}
PFNGLWAITSEMAPHOREEXTPROC glad_debug_glWaitSemaphoreEXT = glad_debug_impl_glWaitSemaphoreEXT;
PFNGLWAITSYNCPROC glad_glWaitSync = NULL;
static void GLAD_API_PTR glad_debug_impl_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    _pre_call_gles2_callback("glWaitSync", (GLADapiproc) glad_glWaitSync, 3, sync, flags, timeout);
    glad_glWaitSync(sync, flags, timeout);
    _post_call_gles2_callback(NULL, "glWaitSync", (GLADapiproc) glad_glWaitSync, 3, sync, flags, timeout);
    
}
PFNGLWAITSYNCPROC glad_debug_glWaitSync = glad_debug_impl_glWaitSync;
PFNGLWAITSYNCAPPLEPROC glad_glWaitSyncAPPLE = NULL;
static void GLAD_API_PTR glad_debug_impl_glWaitSyncAPPLE(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    _pre_call_gles2_callback("glWaitSyncAPPLE", (GLADapiproc) glad_glWaitSyncAPPLE, 3, sync, flags, timeout);
    glad_glWaitSyncAPPLE(sync, flags, timeout);
    _post_call_gles2_callback(NULL, "glWaitSyncAPPLE", (GLADapiproc) glad_glWaitSyncAPPLE, 3, sync, flags, timeout);
    
}
PFNGLWAITSYNCAPPLEPROC glad_debug_glWaitSyncAPPLE = glad_debug_impl_glWaitSyncAPPLE;
PFNGLWAITVKSEMAPHORENVPROC glad_glWaitVkSemaphoreNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glWaitVkSemaphoreNV(GLuint64 vkSemaphore) {
    _pre_call_gles2_callback("glWaitVkSemaphoreNV", (GLADapiproc) glad_glWaitVkSemaphoreNV, 1, vkSemaphore);
    glad_glWaitVkSemaphoreNV(vkSemaphore);
    _post_call_gles2_callback(NULL, "glWaitVkSemaphoreNV", (GLADapiproc) glad_glWaitVkSemaphoreNV, 1, vkSemaphore);
    
}
PFNGLWAITVKSEMAPHORENVPROC glad_debug_glWaitVkSemaphoreNV = glad_debug_impl_glWaitVkSemaphoreNV;
PFNGLWEIGHTPATHSNVPROC glad_glWeightPathsNV = NULL;
static void GLAD_API_PTR glad_debug_impl_glWeightPathsNV(GLuint resultPath, GLsizei numPaths, const GLuint * paths, const GLfloat * weights) {
    _pre_call_gles2_callback("glWeightPathsNV", (GLADapiproc) glad_glWeightPathsNV, 4, resultPath, numPaths, paths, weights);
    glad_glWeightPathsNV(resultPath, numPaths, paths, weights);
    _post_call_gles2_callback(NULL, "glWeightPathsNV", (GLADapiproc) glad_glWeightPathsNV, 4, resultPath, numPaths, paths, weights);
    
}
PFNGLWEIGHTPATHSNVPROC glad_debug_glWeightPathsNV = glad_debug_impl_glWeightPathsNV;
PFNGLWINDOWRECTANGLESEXTPROC glad_glWindowRectanglesEXT = NULL;
static void GLAD_API_PTR glad_debug_impl_glWindowRectanglesEXT(GLenum mode, GLsizei count, const GLint * box) {
    _pre_call_gles2_callback("glWindowRectanglesEXT", (GLADapiproc) glad_glWindowRectanglesEXT, 3, mode, count, box);
    glad_glWindowRectanglesEXT(mode, count, box);
    _post_call_gles2_callback(NULL, "glWindowRectanglesEXT", (GLADapiproc) glad_glWindowRectanglesEXT, 3, mode, count, box);
    
}
PFNGLWINDOWRECTANGLESEXTPROC glad_debug_glWindowRectanglesEXT = glad_debug_impl_glWindowRectanglesEXT;


static void glad_gl_load_GL_ES_VERSION_2_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ES_VERSION_2_0) return;
    glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC) load(userptr, "glActiveTexture");
    glad_glAttachShader = (PFNGLATTACHSHADERPROC) load(userptr, "glAttachShader");
    glad_glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC) load(userptr, "glBindAttribLocation");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC) load(userptr, "glBindBuffer");
    glad_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) load(userptr, "glBindFramebuffer");
    glad_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) load(userptr, "glBindRenderbuffer");
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC) load(userptr, "glBindTexture");
    glad_glBlendColor = (PFNGLBLENDCOLORPROC) load(userptr, "glBlendColor");
    glad_glBlendEquation = (PFNGLBLENDEQUATIONPROC) load(userptr, "glBlendEquation");
    glad_glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC) load(userptr, "glBlendEquationSeparate");
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC) load(userptr, "glBlendFunc");
    glad_glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC) load(userptr, "glBlendFuncSeparate");
    glad_glBufferData = (PFNGLBUFFERDATAPROC) load(userptr, "glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC) load(userptr, "glBufferSubData");
    glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) load(userptr, "glCheckFramebufferStatus");
    glad_glClear = (PFNGLCLEARPROC) load(userptr, "glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC) load(userptr, "glClearColor");
    glad_glClearDepthf = (PFNGLCLEARDEPTHFPROC) load(userptr, "glClearDepthf");
    glad_glClearStencil = (PFNGLCLEARSTENCILPROC) load(userptr, "glClearStencil");
    glad_glColorMask = (PFNGLCOLORMASKPROC) load(userptr, "glColorMask");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC) load(userptr, "glCompileShader");
    glad_glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC) load(userptr, "glCompressedTexImage2D");
    glad_glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC) load(userptr, "glCompressedTexSubImage2D");
    glad_glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC) load(userptr, "glCopyTexImage2D");
    glad_glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC) load(userptr, "glCopyTexSubImage2D");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC) load(userptr, "glCreateProgram");
    glad_glCreateShader = (PFNGLCREATESHADERPROC) load(userptr, "glCreateShader");
    glad_glCullFace = (PFNGLCULLFACEPROC) load(userptr, "glCullFace");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC) load(userptr, "glDeleteBuffers");
    glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) load(userptr, "glDeleteFramebuffers");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC) load(userptr, "glDeleteProgram");
    glad_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) load(userptr, "glDeleteRenderbuffers");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC) load(userptr, "glDeleteShader");
    glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC) load(userptr, "glDeleteTextures");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC) load(userptr, "glDepthFunc");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC) load(userptr, "glDepthMask");
    glad_glDepthRangef = (PFNGLDEPTHRANGEFPROC) load(userptr, "glDepthRangef");
    glad_glDetachShader = (PFNGLDETACHSHADERPROC) load(userptr, "glDetachShader");
    glad_glDisable = (PFNGLDISABLEPROC) load(userptr, "glDisable");
    glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC) load(userptr, "glDisableVertexAttribArray");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC) load(userptr, "glDrawArrays");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC) load(userptr, "glDrawElements");
    glad_glEnable = (PFNGLENABLEPROC) load(userptr, "glEnable");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) load(userptr, "glEnableVertexAttribArray");
    glad_glFinish = (PFNGLFINISHPROC) load(userptr, "glFinish");
    glad_glFlush = (PFNGLFLUSHPROC) load(userptr, "glFlush");
    glad_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) load(userptr, "glFramebufferRenderbuffer");
    glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) load(userptr, "glFramebufferTexture2D");
    glad_glFrontFace = (PFNGLFRONTFACEPROC) load(userptr, "glFrontFace");
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC) load(userptr, "glGenBuffers");
    glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) load(userptr, "glGenFramebuffers");
    glad_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) load(userptr, "glGenRenderbuffers");
    glad_glGenTextures = (PFNGLGENTEXTURESPROC) load(userptr, "glGenTextures");
    glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) load(userptr, "glGenerateMipmap");
    glad_glGetActiveAttrib = (PFNGLGETACTIVEATTRIBPROC) load(userptr, "glGetActiveAttrib");
    glad_glGetActiveUniform = (PFNGLGETACTIVEUNIFORMPROC) load(userptr, "glGetActiveUniform");
    glad_glGetAttachedShaders = (PFNGLGETATTACHEDSHADERSPROC) load(userptr, "glGetAttachedShaders");
    glad_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC) load(userptr, "glGetAttribLocation");
    glad_glGetBooleanv = (PFNGLGETBOOLEANVPROC) load(userptr, "glGetBooleanv");
    glad_glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC) load(userptr, "glGetBufferParameteriv");
    glad_glGetError = (PFNGLGETERRORPROC) load(userptr, "glGetError");
    glad_glGetFloatv = (PFNGLGETFLOATVPROC) load(userptr, "glGetFloatv");
    glad_glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) load(userptr, "glGetFramebufferAttachmentParameteriv");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC) load(userptr, "glGetIntegerv");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC) load(userptr, "glGetProgramInfoLog");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC) load(userptr, "glGetProgramiv");
    glad_glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) load(userptr, "glGetRenderbufferParameteriv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) load(userptr, "glGetShaderInfoLog");
    glad_glGetShaderPrecisionFormat = (PFNGLGETSHADERPRECISIONFORMATPROC) load(userptr, "glGetShaderPrecisionFormat");
    glad_glGetShaderSource = (PFNGLGETSHADERSOURCEPROC) load(userptr, "glGetShaderSource");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC) load(userptr, "glGetShaderiv");
    glad_glGetString = (PFNGLGETSTRINGPROC) load(userptr, "glGetString");
    glad_glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC) load(userptr, "glGetTexParameterfv");
    glad_glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC) load(userptr, "glGetTexParameteriv");
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) load(userptr, "glGetUniformLocation");
    glad_glGetUniformfv = (PFNGLGETUNIFORMFVPROC) load(userptr, "glGetUniformfv");
    glad_glGetUniformiv = (PFNGLGETUNIFORMIVPROC) load(userptr, "glGetUniformiv");
    glad_glGetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC) load(userptr, "glGetVertexAttribPointerv");
    glad_glGetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC) load(userptr, "glGetVertexAttribfv");
    glad_glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC) load(userptr, "glGetVertexAttribiv");
    glad_glHint = (PFNGLHINTPROC) load(userptr, "glHint");
    glad_glIsBuffer = (PFNGLISBUFFERPROC) load(userptr, "glIsBuffer");
    glad_glIsEnabled = (PFNGLISENABLEDPROC) load(userptr, "glIsEnabled");
    glad_glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC) load(userptr, "glIsFramebuffer");
    glad_glIsProgram = (PFNGLISPROGRAMPROC) load(userptr, "glIsProgram");
    glad_glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC) load(userptr, "glIsRenderbuffer");
    glad_glIsShader = (PFNGLISSHADERPROC) load(userptr, "glIsShader");
    glad_glIsTexture = (PFNGLISTEXTUREPROC) load(userptr, "glIsTexture");
    glad_glLineWidth = (PFNGLLINEWIDTHPROC) load(userptr, "glLineWidth");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC) load(userptr, "glLinkProgram");
    glad_glPixelStorei = (PFNGLPIXELSTOREIPROC) load(userptr, "glPixelStorei");
    glad_glPolygonOffset = (PFNGLPOLYGONOFFSETPROC) load(userptr, "glPolygonOffset");
    glad_glReadPixels = (PFNGLREADPIXELSPROC) load(userptr, "glReadPixels");
    glad_glReleaseShaderCompiler = (PFNGLRELEASESHADERCOMPILERPROC) load(userptr, "glReleaseShaderCompiler");
    glad_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) load(userptr, "glRenderbufferStorage");
    glad_glSampleCoverage = (PFNGLSAMPLECOVERAGEPROC) load(userptr, "glSampleCoverage");
    glad_glScissor = (PFNGLSCISSORPROC) load(userptr, "glScissor");
    glad_glShaderBinary = (PFNGLSHADERBINARYPROC) load(userptr, "glShaderBinary");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC) load(userptr, "glShaderSource");
    glad_glStencilFunc = (PFNGLSTENCILFUNCPROC) load(userptr, "glStencilFunc");
    glad_glStencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC) load(userptr, "glStencilFuncSeparate");
    glad_glStencilMask = (PFNGLSTENCILMASKPROC) load(userptr, "glStencilMask");
    glad_glStencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC) load(userptr, "glStencilMaskSeparate");
    glad_glStencilOp = (PFNGLSTENCILOPPROC) load(userptr, "glStencilOp");
    glad_glStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC) load(userptr, "glStencilOpSeparate");
    glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC) load(userptr, "glTexImage2D");
    glad_glTexParameterf = (PFNGLTEXPARAMETERFPROC) load(userptr, "glTexParameterf");
    glad_glTexParameterfv = (PFNGLTEXPARAMETERFVPROC) load(userptr, "glTexParameterfv");
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC) load(userptr, "glTexParameteri");
    glad_glTexParameteriv = (PFNGLTEXPARAMETERIVPROC) load(userptr, "glTexParameteriv");
    glad_glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC) load(userptr, "glTexSubImage2D");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC) load(userptr, "glUniform1f");
    glad_glUniform1fv = (PFNGLUNIFORM1FVPROC) load(userptr, "glUniform1fv");
    glad_glUniform1i = (PFNGLUNIFORM1IPROC) load(userptr, "glUniform1i");
    glad_glUniform1iv = (PFNGLUNIFORM1IVPROC) load(userptr, "glUniform1iv");
    glad_glUniform2f = (PFNGLUNIFORM2FPROC) load(userptr, "glUniform2f");
    glad_glUniform2fv = (PFNGLUNIFORM2FVPROC) load(userptr, "glUniform2fv");
    glad_glUniform2i = (PFNGLUNIFORM2IPROC) load(userptr, "glUniform2i");
    glad_glUniform2iv = (PFNGLUNIFORM2IVPROC) load(userptr, "glUniform2iv");
    glad_glUniform3f = (PFNGLUNIFORM3FPROC) load(userptr, "glUniform3f");
    glad_glUniform3fv = (PFNGLUNIFORM3FVPROC) load(userptr, "glUniform3fv");
    glad_glUniform3i = (PFNGLUNIFORM3IPROC) load(userptr, "glUniform3i");
    glad_glUniform3iv = (PFNGLUNIFORM3IVPROC) load(userptr, "glUniform3iv");
    glad_glUniform4f = (PFNGLUNIFORM4FPROC) load(userptr, "glUniform4f");
    glad_glUniform4fv = (PFNGLUNIFORM4FVPROC) load(userptr, "glUniform4fv");
    glad_glUniform4i = (PFNGLUNIFORM4IPROC) load(userptr, "glUniform4i");
    glad_glUniform4iv = (PFNGLUNIFORM4IVPROC) load(userptr, "glUniform4iv");
    glad_glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC) load(userptr, "glUniformMatrix2fv");
    glad_glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC) load(userptr, "glUniformMatrix3fv");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) load(userptr, "glUniformMatrix4fv");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC) load(userptr, "glUseProgram");
    glad_glValidateProgram = (PFNGLVALIDATEPROGRAMPROC) load(userptr, "glValidateProgram");
    glad_glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC) load(userptr, "glVertexAttrib1f");
    glad_glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC) load(userptr, "glVertexAttrib1fv");
    glad_glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC) load(userptr, "glVertexAttrib2f");
    glad_glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC) load(userptr, "glVertexAttrib2fv");
    glad_glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC) load(userptr, "glVertexAttrib3f");
    glad_glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC) load(userptr, "glVertexAttrib3fv");
    glad_glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC) load(userptr, "glVertexAttrib4f");
    glad_glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC) load(userptr, "glVertexAttrib4fv");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) load(userptr, "glVertexAttribPointer");
    glad_glViewport = (PFNGLVIEWPORTPROC) load(userptr, "glViewport");
}
static void glad_gl_load_GL_ES_VERSION_3_0( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ES_VERSION_3_0) return;
    glad_glBeginQuery = (PFNGLBEGINQUERYPROC) load(userptr, "glBeginQuery");
    glad_glBeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC) load(userptr, "glBeginTransformFeedback");
    glad_glBindBufferBase = (PFNGLBINDBUFFERBASEPROC) load(userptr, "glBindBufferBase");
    glad_glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC) load(userptr, "glBindBufferRange");
    glad_glBindSampler = (PFNGLBINDSAMPLERPROC) load(userptr, "glBindSampler");
    glad_glBindTransformFeedback = (PFNGLBINDTRANSFORMFEEDBACKPROC) load(userptr, "glBindTransformFeedback");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) load(userptr, "glBindVertexArray");
    glad_glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC) load(userptr, "glBlitFramebuffer");
    glad_glClearBufferfi = (PFNGLCLEARBUFFERFIPROC) load(userptr, "glClearBufferfi");
    glad_glClearBufferfv = (PFNGLCLEARBUFFERFVPROC) load(userptr, "glClearBufferfv");
    glad_glClearBufferiv = (PFNGLCLEARBUFFERIVPROC) load(userptr, "glClearBufferiv");
    glad_glClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC) load(userptr, "glClearBufferuiv");
    glad_glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC) load(userptr, "glClientWaitSync");
    glad_glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC) load(userptr, "glCompressedTexImage3D");
    glad_glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC) load(userptr, "glCompressedTexSubImage3D");
    glad_glCopyBufferSubData = (PFNGLCOPYBUFFERSUBDATAPROC) load(userptr, "glCopyBufferSubData");
    glad_glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC) load(userptr, "glCopyTexSubImage3D");
    glad_glDeleteQueries = (PFNGLDELETEQUERIESPROC) load(userptr, "glDeleteQueries");
    glad_glDeleteSamplers = (PFNGLDELETESAMPLERSPROC) load(userptr, "glDeleteSamplers");
    glad_glDeleteSync = (PFNGLDELETESYNCPROC) load(userptr, "glDeleteSync");
    glad_glDeleteTransformFeedbacks = (PFNGLDELETETRANSFORMFEEDBACKSPROC) load(userptr, "glDeleteTransformFeedbacks");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) load(userptr, "glDeleteVertexArrays");
    glad_glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC) load(userptr, "glDrawArraysInstanced");
    glad_glDrawBuffers = (PFNGLDRAWBUFFERSPROC) load(userptr, "glDrawBuffers");
    glad_glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC) load(userptr, "glDrawElementsInstanced");
    glad_glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC) load(userptr, "glDrawRangeElements");
    glad_glEndQuery = (PFNGLENDQUERYPROC) load(userptr, "glEndQuery");
    glad_glEndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC) load(userptr, "glEndTransformFeedback");
    glad_glFenceSync = (PFNGLFENCESYNCPROC) load(userptr, "glFenceSync");
    glad_glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC) load(userptr, "glFlushMappedBufferRange");
    glad_glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC) load(userptr, "glFramebufferTextureLayer");
    glad_glGenQueries = (PFNGLGENQUERIESPROC) load(userptr, "glGenQueries");
    glad_glGenSamplers = (PFNGLGENSAMPLERSPROC) load(userptr, "glGenSamplers");
    glad_glGenTransformFeedbacks = (PFNGLGENTRANSFORMFEEDBACKSPROC) load(userptr, "glGenTransformFeedbacks");
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) load(userptr, "glGenVertexArrays");
    glad_glGetActiveUniformBlockName = (PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC) load(userptr, "glGetActiveUniformBlockName");
    glad_glGetActiveUniformBlockiv = (PFNGLGETACTIVEUNIFORMBLOCKIVPROC) load(userptr, "glGetActiveUniformBlockiv");
    glad_glGetActiveUniformsiv = (PFNGLGETACTIVEUNIFORMSIVPROC) load(userptr, "glGetActiveUniformsiv");
    glad_glGetBufferParameteri64v = (PFNGLGETBUFFERPARAMETERI64VPROC) load(userptr, "glGetBufferParameteri64v");
    glad_glGetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC) load(userptr, "glGetBufferPointerv");
    glad_glGetFragDataLocation = (PFNGLGETFRAGDATALOCATIONPROC) load(userptr, "glGetFragDataLocation");
    glad_glGetInteger64i_v = (PFNGLGETINTEGER64I_VPROC) load(userptr, "glGetInteger64i_v");
    glad_glGetInteger64v = (PFNGLGETINTEGER64VPROC) load(userptr, "glGetInteger64v");
    glad_glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC) load(userptr, "glGetIntegeri_v");
    glad_glGetInternalformativ = (PFNGLGETINTERNALFORMATIVPROC) load(userptr, "glGetInternalformativ");
    glad_glGetProgramBinary = (PFNGLGETPROGRAMBINARYPROC) load(userptr, "glGetProgramBinary");
    glad_glGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC) load(userptr, "glGetQueryObjectuiv");
    glad_glGetQueryiv = (PFNGLGETQUERYIVPROC) load(userptr, "glGetQueryiv");
    glad_glGetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC) load(userptr, "glGetSamplerParameterfv");
    glad_glGetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC) load(userptr, "glGetSamplerParameteriv");
    glad_glGetStringi = (PFNGLGETSTRINGIPROC) load(userptr, "glGetStringi");
    glad_glGetSynciv = (PFNGLGETSYNCIVPROC) load(userptr, "glGetSynciv");
    glad_glGetTransformFeedbackVarying = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC) load(userptr, "glGetTransformFeedbackVarying");
    glad_glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC) load(userptr, "glGetUniformBlockIndex");
    glad_glGetUniformIndices = (PFNGLGETUNIFORMINDICESPROC) load(userptr, "glGetUniformIndices");
    glad_glGetUniformuiv = (PFNGLGETUNIFORMUIVPROC) load(userptr, "glGetUniformuiv");
    glad_glGetVertexAttribIiv = (PFNGLGETVERTEXATTRIBIIVPROC) load(userptr, "glGetVertexAttribIiv");
    glad_glGetVertexAttribIuiv = (PFNGLGETVERTEXATTRIBIUIVPROC) load(userptr, "glGetVertexAttribIuiv");
    glad_glInvalidateFramebuffer = (PFNGLINVALIDATEFRAMEBUFFERPROC) load(userptr, "glInvalidateFramebuffer");
    glad_glInvalidateSubFramebuffer = (PFNGLINVALIDATESUBFRAMEBUFFERPROC) load(userptr, "glInvalidateSubFramebuffer");
    glad_glIsQuery = (PFNGLISQUERYPROC) load(userptr, "glIsQuery");
    glad_glIsSampler = (PFNGLISSAMPLERPROC) load(userptr, "glIsSampler");
    glad_glIsSync = (PFNGLISSYNCPROC) load(userptr, "glIsSync");
    glad_glIsTransformFeedback = (PFNGLISTRANSFORMFEEDBACKPROC) load(userptr, "glIsTransformFeedback");
    glad_glIsVertexArray = (PFNGLISVERTEXARRAYPROC) load(userptr, "glIsVertexArray");
    glad_glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC) load(userptr, "glMapBufferRange");
    glad_glPauseTransformFeedback = (PFNGLPAUSETRANSFORMFEEDBACKPROC) load(userptr, "glPauseTransformFeedback");
    glad_glProgramBinary = (PFNGLPROGRAMBINARYPROC) load(userptr, "glProgramBinary");
    glad_glProgramParameteri = (PFNGLPROGRAMPARAMETERIPROC) load(userptr, "glProgramParameteri");
    glad_glReadBuffer = (PFNGLREADBUFFERPROC) load(userptr, "glReadBuffer");
    glad_glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) load(userptr, "glRenderbufferStorageMultisample");
    glad_glResumeTransformFeedback = (PFNGLRESUMETRANSFORMFEEDBACKPROC) load(userptr, "glResumeTransformFeedback");
    glad_glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC) load(userptr, "glSamplerParameterf");
    glad_glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC) load(userptr, "glSamplerParameterfv");
    glad_glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC) load(userptr, "glSamplerParameteri");
    glad_glSamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC) load(userptr, "glSamplerParameteriv");
    glad_glTexImage3D = (PFNGLTEXIMAGE3DPROC) load(userptr, "glTexImage3D");
    glad_glTexStorage2D = (PFNGLTEXSTORAGE2DPROC) load(userptr, "glTexStorage2D");
    glad_glTexStorage3D = (PFNGLTEXSTORAGE3DPROC) load(userptr, "glTexStorage3D");
    glad_glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC) load(userptr, "glTexSubImage3D");
    glad_glTransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC) load(userptr, "glTransformFeedbackVaryings");
    glad_glUniform1ui = (PFNGLUNIFORM1UIPROC) load(userptr, "glUniform1ui");
    glad_glUniform1uiv = (PFNGLUNIFORM1UIVPROC) load(userptr, "glUniform1uiv");
    glad_glUniform2ui = (PFNGLUNIFORM2UIPROC) load(userptr, "glUniform2ui");
    glad_glUniform2uiv = (PFNGLUNIFORM2UIVPROC) load(userptr, "glUniform2uiv");
    glad_glUniform3ui = (PFNGLUNIFORM3UIPROC) load(userptr, "glUniform3ui");
    glad_glUniform3uiv = (PFNGLUNIFORM3UIVPROC) load(userptr, "glUniform3uiv");
    glad_glUniform4ui = (PFNGLUNIFORM4UIPROC) load(userptr, "glUniform4ui");
    glad_glUniform4uiv = (PFNGLUNIFORM4UIVPROC) load(userptr, "glUniform4uiv");
    glad_glUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC) load(userptr, "glUniformBlockBinding");
    glad_glUniformMatrix2x3fv = (PFNGLUNIFORMMATRIX2X3FVPROC) load(userptr, "glUniformMatrix2x3fv");
    glad_glUniformMatrix2x4fv = (PFNGLUNIFORMMATRIX2X4FVPROC) load(userptr, "glUniformMatrix2x4fv");
    glad_glUniformMatrix3x2fv = (PFNGLUNIFORMMATRIX3X2FVPROC) load(userptr, "glUniformMatrix3x2fv");
    glad_glUniformMatrix3x4fv = (PFNGLUNIFORMMATRIX3X4FVPROC) load(userptr, "glUniformMatrix3x4fv");
    glad_glUniformMatrix4x2fv = (PFNGLUNIFORMMATRIX4X2FVPROC) load(userptr, "glUniformMatrix4x2fv");
    glad_glUniformMatrix4x3fv = (PFNGLUNIFORMMATRIX4X3FVPROC) load(userptr, "glUniformMatrix4x3fv");
    glad_glUnmapBuffer = (PFNGLUNMAPBUFFERPROC) load(userptr, "glUnmapBuffer");
    glad_glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC) load(userptr, "glVertexAttribDivisor");
    glad_glVertexAttribI4i = (PFNGLVERTEXATTRIBI4IPROC) load(userptr, "glVertexAttribI4i");
    glad_glVertexAttribI4iv = (PFNGLVERTEXATTRIBI4IVPROC) load(userptr, "glVertexAttribI4iv");
    glad_glVertexAttribI4ui = (PFNGLVERTEXATTRIBI4UIPROC) load(userptr, "glVertexAttribI4ui");
    glad_glVertexAttribI4uiv = (PFNGLVERTEXATTRIBI4UIVPROC) load(userptr, "glVertexAttribI4uiv");
    glad_glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC) load(userptr, "glVertexAttribIPointer");
    glad_glWaitSync = (PFNGLWAITSYNCPROC) load(userptr, "glWaitSync");
}
static void glad_gl_load_GL_ES_VERSION_3_1( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ES_VERSION_3_1) return;
    glad_glActiveShaderProgram = (PFNGLACTIVESHADERPROGRAMPROC) load(userptr, "glActiveShaderProgram");
    glad_glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC) load(userptr, "glBindImageTexture");
    glad_glBindProgramPipeline = (PFNGLBINDPROGRAMPIPELINEPROC) load(userptr, "glBindProgramPipeline");
    glad_glBindVertexBuffer = (PFNGLBINDVERTEXBUFFERPROC) load(userptr, "glBindVertexBuffer");
    glad_glCreateShaderProgramv = (PFNGLCREATESHADERPROGRAMVPROC) load(userptr, "glCreateShaderProgramv");
    glad_glDeleteProgramPipelines = (PFNGLDELETEPROGRAMPIPELINESPROC) load(userptr, "glDeleteProgramPipelines");
    glad_glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC) load(userptr, "glDispatchCompute");
    glad_glDispatchComputeIndirect = (PFNGLDISPATCHCOMPUTEINDIRECTPROC) load(userptr, "glDispatchComputeIndirect");
    glad_glDrawArraysIndirect = (PFNGLDRAWARRAYSINDIRECTPROC) load(userptr, "glDrawArraysIndirect");
    glad_glDrawElementsIndirect = (PFNGLDRAWELEMENTSINDIRECTPROC) load(userptr, "glDrawElementsIndirect");
    glad_glFramebufferParameteri = (PFNGLFRAMEBUFFERPARAMETERIPROC) load(userptr, "glFramebufferParameteri");
    glad_glGenProgramPipelines = (PFNGLGENPROGRAMPIPELINESPROC) load(userptr, "glGenProgramPipelines");
    glad_glGetBooleani_v = (PFNGLGETBOOLEANI_VPROC) load(userptr, "glGetBooleani_v");
    glad_glGetFramebufferParameteriv = (PFNGLGETFRAMEBUFFERPARAMETERIVPROC) load(userptr, "glGetFramebufferParameteriv");
    glad_glGetMultisamplefv = (PFNGLGETMULTISAMPLEFVPROC) load(userptr, "glGetMultisamplefv");
    glad_glGetProgramInterfaceiv = (PFNGLGETPROGRAMINTERFACEIVPROC) load(userptr, "glGetProgramInterfaceiv");
    glad_glGetProgramPipelineInfoLog = (PFNGLGETPROGRAMPIPELINEINFOLOGPROC) load(userptr, "glGetProgramPipelineInfoLog");
    glad_glGetProgramPipelineiv = (PFNGLGETPROGRAMPIPELINEIVPROC) load(userptr, "glGetProgramPipelineiv");
    glad_glGetProgramResourceIndex = (PFNGLGETPROGRAMRESOURCEINDEXPROC) load(userptr, "glGetProgramResourceIndex");
    glad_glGetProgramResourceLocation = (PFNGLGETPROGRAMRESOURCELOCATIONPROC) load(userptr, "glGetProgramResourceLocation");
    glad_glGetProgramResourceName = (PFNGLGETPROGRAMRESOURCENAMEPROC) load(userptr, "glGetProgramResourceName");
    glad_glGetProgramResourceiv = (PFNGLGETPROGRAMRESOURCEIVPROC) load(userptr, "glGetProgramResourceiv");
    glad_glGetTexLevelParameterfv = (PFNGLGETTEXLEVELPARAMETERFVPROC) load(userptr, "glGetTexLevelParameterfv");
    glad_glGetTexLevelParameteriv = (PFNGLGETTEXLEVELPARAMETERIVPROC) load(userptr, "glGetTexLevelParameteriv");
    glad_glIsProgramPipeline = (PFNGLISPROGRAMPIPELINEPROC) load(userptr, "glIsProgramPipeline");
    glad_glMemoryBarrier = (PFNGLMEMORYBARRIERPROC) load(userptr, "glMemoryBarrier");
    glad_glMemoryBarrierByRegion = (PFNGLMEMORYBARRIERBYREGIONPROC) load(userptr, "glMemoryBarrierByRegion");
    glad_glProgramUniform1f = (PFNGLPROGRAMUNIFORM1FPROC) load(userptr, "glProgramUniform1f");
    glad_glProgramUniform1fv = (PFNGLPROGRAMUNIFORM1FVPROC) load(userptr, "glProgramUniform1fv");
    glad_glProgramUniform1i = (PFNGLPROGRAMUNIFORM1IPROC) load(userptr, "glProgramUniform1i");
    glad_glProgramUniform1iv = (PFNGLPROGRAMUNIFORM1IVPROC) load(userptr, "glProgramUniform1iv");
    glad_glProgramUniform1ui = (PFNGLPROGRAMUNIFORM1UIPROC) load(userptr, "glProgramUniform1ui");
    glad_glProgramUniform1uiv = (PFNGLPROGRAMUNIFORM1UIVPROC) load(userptr, "glProgramUniform1uiv");
    glad_glProgramUniform2f = (PFNGLPROGRAMUNIFORM2FPROC) load(userptr, "glProgramUniform2f");
    glad_glProgramUniform2fv = (PFNGLPROGRAMUNIFORM2FVPROC) load(userptr, "glProgramUniform2fv");
    glad_glProgramUniform2i = (PFNGLPROGRAMUNIFORM2IPROC) load(userptr, "glProgramUniform2i");
    glad_glProgramUniform2iv = (PFNGLPROGRAMUNIFORM2IVPROC) load(userptr, "glProgramUniform2iv");
    glad_glProgramUniform2ui = (PFNGLPROGRAMUNIFORM2UIPROC) load(userptr, "glProgramUniform2ui");
    glad_glProgramUniform2uiv = (PFNGLPROGRAMUNIFORM2UIVPROC) load(userptr, "glProgramUniform2uiv");
    glad_glProgramUniform3f = (PFNGLPROGRAMUNIFORM3FPROC) load(userptr, "glProgramUniform3f");
    glad_glProgramUniform3fv = (PFNGLPROGRAMUNIFORM3FVPROC) load(userptr, "glProgramUniform3fv");
    glad_glProgramUniform3i = (PFNGLPROGRAMUNIFORM3IPROC) load(userptr, "glProgramUniform3i");
    glad_glProgramUniform3iv = (PFNGLPROGRAMUNIFORM3IVPROC) load(userptr, "glProgramUniform3iv");
    glad_glProgramUniform3ui = (PFNGLPROGRAMUNIFORM3UIPROC) load(userptr, "glProgramUniform3ui");
    glad_glProgramUniform3uiv = (PFNGLPROGRAMUNIFORM3UIVPROC) load(userptr, "glProgramUniform3uiv");
    glad_glProgramUniform4f = (PFNGLPROGRAMUNIFORM4FPROC) load(userptr, "glProgramUniform4f");
    glad_glProgramUniform4fv = (PFNGLPROGRAMUNIFORM4FVPROC) load(userptr, "glProgramUniform4fv");
    glad_glProgramUniform4i = (PFNGLPROGRAMUNIFORM4IPROC) load(userptr, "glProgramUniform4i");
    glad_glProgramUniform4iv = (PFNGLPROGRAMUNIFORM4IVPROC) load(userptr, "glProgramUniform4iv");
    glad_glProgramUniform4ui = (PFNGLPROGRAMUNIFORM4UIPROC) load(userptr, "glProgramUniform4ui");
    glad_glProgramUniform4uiv = (PFNGLPROGRAMUNIFORM4UIVPROC) load(userptr, "glProgramUniform4uiv");
    glad_glProgramUniformMatrix2fv = (PFNGLPROGRAMUNIFORMMATRIX2FVPROC) load(userptr, "glProgramUniformMatrix2fv");
    glad_glProgramUniformMatrix2x3fv = (PFNGLPROGRAMUNIFORMMATRIX2X3FVPROC) load(userptr, "glProgramUniformMatrix2x3fv");
    glad_glProgramUniformMatrix2x4fv = (PFNGLPROGRAMUNIFORMMATRIX2X4FVPROC) load(userptr, "glProgramUniformMatrix2x4fv");
    glad_glProgramUniformMatrix3fv = (PFNGLPROGRAMUNIFORMMATRIX3FVPROC) load(userptr, "glProgramUniformMatrix3fv");
    glad_glProgramUniformMatrix3x2fv = (PFNGLPROGRAMUNIFORMMATRIX3X2FVPROC) load(userptr, "glProgramUniformMatrix3x2fv");
    glad_glProgramUniformMatrix3x4fv = (PFNGLPROGRAMUNIFORMMATRIX3X4FVPROC) load(userptr, "glProgramUniformMatrix3x4fv");
    glad_glProgramUniformMatrix4fv = (PFNGLPROGRAMUNIFORMMATRIX4FVPROC) load(userptr, "glProgramUniformMatrix4fv");
    glad_glProgramUniformMatrix4x2fv = (PFNGLPROGRAMUNIFORMMATRIX4X2FVPROC) load(userptr, "glProgramUniformMatrix4x2fv");
    glad_glProgramUniformMatrix4x3fv = (PFNGLPROGRAMUNIFORMMATRIX4X3FVPROC) load(userptr, "glProgramUniformMatrix4x3fv");
    glad_glSampleMaski = (PFNGLSAMPLEMASKIPROC) load(userptr, "glSampleMaski");
    glad_glTexStorage2DMultisample = (PFNGLTEXSTORAGE2DMULTISAMPLEPROC) load(userptr, "glTexStorage2DMultisample");
    glad_glUseProgramStages = (PFNGLUSEPROGRAMSTAGESPROC) load(userptr, "glUseProgramStages");
    glad_glValidateProgramPipeline = (PFNGLVALIDATEPROGRAMPIPELINEPROC) load(userptr, "glValidateProgramPipeline");
    glad_glVertexAttribBinding = (PFNGLVERTEXATTRIBBINDINGPROC) load(userptr, "glVertexAttribBinding");
    glad_glVertexAttribFormat = (PFNGLVERTEXATTRIBFORMATPROC) load(userptr, "glVertexAttribFormat");
    glad_glVertexAttribIFormat = (PFNGLVERTEXATTRIBIFORMATPROC) load(userptr, "glVertexAttribIFormat");
    glad_glVertexBindingDivisor = (PFNGLVERTEXBINDINGDIVISORPROC) load(userptr, "glVertexBindingDivisor");
}
static void glad_gl_load_GL_ES_VERSION_3_2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ES_VERSION_3_2) return;
    glad_glBlendBarrier = (PFNGLBLENDBARRIERPROC) load(userptr, "glBlendBarrier");
    glad_glBlendEquationSeparatei = (PFNGLBLENDEQUATIONSEPARATEIPROC) load(userptr, "glBlendEquationSeparatei");
    glad_glBlendEquationi = (PFNGLBLENDEQUATIONIPROC) load(userptr, "glBlendEquationi");
    glad_glBlendFuncSeparatei = (PFNGLBLENDFUNCSEPARATEIPROC) load(userptr, "glBlendFuncSeparatei");
    glad_glBlendFunci = (PFNGLBLENDFUNCIPROC) load(userptr, "glBlendFunci");
    glad_glColorMaski = (PFNGLCOLORMASKIPROC) load(userptr, "glColorMaski");
    glad_glCopyImageSubData = (PFNGLCOPYIMAGESUBDATAPROC) load(userptr, "glCopyImageSubData");
    glad_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC) load(userptr, "glDebugMessageCallback");
    glad_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC) load(userptr, "glDebugMessageControl");
    glad_glDebugMessageInsert = (PFNGLDEBUGMESSAGEINSERTPROC) load(userptr, "glDebugMessageInsert");
    glad_glDisablei = (PFNGLDISABLEIPROC) load(userptr, "glDisablei");
    glad_glDrawElementsBaseVertex = (PFNGLDRAWELEMENTSBASEVERTEXPROC) load(userptr, "glDrawElementsBaseVertex");
    glad_glDrawElementsInstancedBaseVertex = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC) load(userptr, "glDrawElementsInstancedBaseVertex");
    glad_glDrawRangeElementsBaseVertex = (PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC) load(userptr, "glDrawRangeElementsBaseVertex");
    glad_glEnablei = (PFNGLENABLEIPROC) load(userptr, "glEnablei");
    glad_glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC) load(userptr, "glFramebufferTexture");
    glad_glGetDebugMessageLog = (PFNGLGETDEBUGMESSAGELOGPROC) load(userptr, "glGetDebugMessageLog");
    glad_glGetGraphicsResetStatus = (PFNGLGETGRAPHICSRESETSTATUSPROC) load(userptr, "glGetGraphicsResetStatus");
    glad_glGetObjectLabel = (PFNGLGETOBJECTLABELPROC) load(userptr, "glGetObjectLabel");
    glad_glGetObjectPtrLabel = (PFNGLGETOBJECTPTRLABELPROC) load(userptr, "glGetObjectPtrLabel");
    glad_glGetPointerv = (PFNGLGETPOINTERVPROC) load(userptr, "glGetPointerv");
    glad_glGetSamplerParameterIiv = (PFNGLGETSAMPLERPARAMETERIIVPROC) load(userptr, "glGetSamplerParameterIiv");
    glad_glGetSamplerParameterIuiv = (PFNGLGETSAMPLERPARAMETERIUIVPROC) load(userptr, "glGetSamplerParameterIuiv");
    glad_glGetTexParameterIiv = (PFNGLGETTEXPARAMETERIIVPROC) load(userptr, "glGetTexParameterIiv");
    glad_glGetTexParameterIuiv = (PFNGLGETTEXPARAMETERIUIVPROC) load(userptr, "glGetTexParameterIuiv");
    glad_glGetnUniformfv = (PFNGLGETNUNIFORMFVPROC) load(userptr, "glGetnUniformfv");
    glad_glGetnUniformiv = (PFNGLGETNUNIFORMIVPROC) load(userptr, "glGetnUniformiv");
    glad_glGetnUniformuiv = (PFNGLGETNUNIFORMUIVPROC) load(userptr, "glGetnUniformuiv");
    glad_glIsEnabledi = (PFNGLISENABLEDIPROC) load(userptr, "glIsEnabledi");
    glad_glMinSampleShading = (PFNGLMINSAMPLESHADINGPROC) load(userptr, "glMinSampleShading");
    glad_glObjectLabel = (PFNGLOBJECTLABELPROC) load(userptr, "glObjectLabel");
    glad_glObjectPtrLabel = (PFNGLOBJECTPTRLABELPROC) load(userptr, "glObjectPtrLabel");
    glad_glPatchParameteri = (PFNGLPATCHPARAMETERIPROC) load(userptr, "glPatchParameteri");
    glad_glPopDebugGroup = (PFNGLPOPDEBUGGROUPPROC) load(userptr, "glPopDebugGroup");
    glad_glPrimitiveBoundingBox = (PFNGLPRIMITIVEBOUNDINGBOXPROC) load(userptr, "glPrimitiveBoundingBox");
    glad_glPushDebugGroup = (PFNGLPUSHDEBUGGROUPPROC) load(userptr, "glPushDebugGroup");
    glad_glReadnPixels = (PFNGLREADNPIXELSPROC) load(userptr, "glReadnPixels");
    glad_glSamplerParameterIiv = (PFNGLSAMPLERPARAMETERIIVPROC) load(userptr, "glSamplerParameterIiv");
    glad_glSamplerParameterIuiv = (PFNGLSAMPLERPARAMETERIUIVPROC) load(userptr, "glSamplerParameterIuiv");
    glad_glTexBuffer = (PFNGLTEXBUFFERPROC) load(userptr, "glTexBuffer");
    glad_glTexBufferRange = (PFNGLTEXBUFFERRANGEPROC) load(userptr, "glTexBufferRange");
    glad_glTexParameterIiv = (PFNGLTEXPARAMETERIIVPROC) load(userptr, "glTexParameterIiv");
    glad_glTexParameterIuiv = (PFNGLTEXPARAMETERIUIVPROC) load(userptr, "glTexParameterIuiv");
    glad_glTexStorage3DMultisample = (PFNGLTEXSTORAGE3DMULTISAMPLEPROC) load(userptr, "glTexStorage3DMultisample");
}
static void glad_gl_load_GL_AMD_framebuffer_multisample_advanced( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_AMD_framebuffer_multisample_advanced) return;
    glad_glNamedRenderbufferStorageMultisampleAdvancedAMD = (PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEADVANCEDAMDPROC) load(userptr, "glNamedRenderbufferStorageMultisampleAdvancedAMD");
    glad_glRenderbufferStorageMultisampleAdvancedAMD = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEADVANCEDAMDPROC) load(userptr, "glRenderbufferStorageMultisampleAdvancedAMD");
}
static void glad_gl_load_GL_AMD_performance_monitor( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_AMD_performance_monitor) return;
    glad_glBeginPerfMonitorAMD = (PFNGLBEGINPERFMONITORAMDPROC) load(userptr, "glBeginPerfMonitorAMD");
    glad_glDeletePerfMonitorsAMD = (PFNGLDELETEPERFMONITORSAMDPROC) load(userptr, "glDeletePerfMonitorsAMD");
    glad_glEndPerfMonitorAMD = (PFNGLENDPERFMONITORAMDPROC) load(userptr, "glEndPerfMonitorAMD");
    glad_glGenPerfMonitorsAMD = (PFNGLGENPERFMONITORSAMDPROC) load(userptr, "glGenPerfMonitorsAMD");
    glad_glGetPerfMonitorCounterDataAMD = (PFNGLGETPERFMONITORCOUNTERDATAAMDPROC) load(userptr, "glGetPerfMonitorCounterDataAMD");
    glad_glGetPerfMonitorCounterInfoAMD = (PFNGLGETPERFMONITORCOUNTERINFOAMDPROC) load(userptr, "glGetPerfMonitorCounterInfoAMD");
    glad_glGetPerfMonitorCounterStringAMD = (PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC) load(userptr, "glGetPerfMonitorCounterStringAMD");
    glad_glGetPerfMonitorCountersAMD = (PFNGLGETPERFMONITORCOUNTERSAMDPROC) load(userptr, "glGetPerfMonitorCountersAMD");
    glad_glGetPerfMonitorGroupStringAMD = (PFNGLGETPERFMONITORGROUPSTRINGAMDPROC) load(userptr, "glGetPerfMonitorGroupStringAMD");
    glad_glGetPerfMonitorGroupsAMD = (PFNGLGETPERFMONITORGROUPSAMDPROC) load(userptr, "glGetPerfMonitorGroupsAMD");
    glad_glSelectPerfMonitorCountersAMD = (PFNGLSELECTPERFMONITORCOUNTERSAMDPROC) load(userptr, "glSelectPerfMonitorCountersAMD");
}
static void glad_gl_load_GL_ANGLE_framebuffer_blit( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ANGLE_framebuffer_blit) return;
    glad_glBlitFramebufferANGLE = (PFNGLBLITFRAMEBUFFERANGLEPROC) load(userptr, "glBlitFramebufferANGLE");
}
static void glad_gl_load_GL_ANGLE_framebuffer_multisample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ANGLE_framebuffer_multisample) return;
    glad_glRenderbufferStorageMultisampleANGLE = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC) load(userptr, "glRenderbufferStorageMultisampleANGLE");
}
static void glad_gl_load_GL_ANGLE_instanced_arrays( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ANGLE_instanced_arrays) return;
    glad_glDrawArraysInstancedANGLE = (PFNGLDRAWARRAYSINSTANCEDANGLEPROC) load(userptr, "glDrawArraysInstancedANGLE");
    glad_glDrawElementsInstancedANGLE = (PFNGLDRAWELEMENTSINSTANCEDANGLEPROC) load(userptr, "glDrawElementsInstancedANGLE");
    glad_glVertexAttribDivisorANGLE = (PFNGLVERTEXATTRIBDIVISORANGLEPROC) load(userptr, "glVertexAttribDivisorANGLE");
}
static void glad_gl_load_GL_ANGLE_translated_shader_source( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ANGLE_translated_shader_source) return;
    glad_glGetTranslatedShaderSourceANGLE = (PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC) load(userptr, "glGetTranslatedShaderSourceANGLE");
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
static void glad_gl_load_GL_ARM_shader_core_properties( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_ARM_shader_core_properties) return;
    glad_glMaxActiveShaderCoresARM = (PFNGLMAXACTIVESHADERCORESARMPROC) load(userptr, "glMaxActiveShaderCoresARM");
}
static void glad_gl_load_GL_EXT_EGL_image_storage( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_EGL_image_storage) return;
    glad_glEGLImageTargetTexStorageEXT = (PFNGLEGLIMAGETARGETTEXSTORAGEEXTPROC) load(userptr, "glEGLImageTargetTexStorageEXT");
    glad_glEGLImageTargetTextureStorageEXT = (PFNGLEGLIMAGETARGETTEXTURESTORAGEEXTPROC) load(userptr, "glEGLImageTargetTextureStorageEXT");
}
static void glad_gl_load_GL_EXT_base_instance( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_base_instance) return;
    glad_glDrawArraysInstancedBaseInstanceEXT = (PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC) load(userptr, "glDrawArraysInstancedBaseInstanceEXT");
    glad_glDrawElementsInstancedBaseInstanceEXT = (PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC) load(userptr, "glDrawElementsInstancedBaseInstanceEXT");
    glad_glDrawElementsInstancedBaseVertexBaseInstanceEXT = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC) load(userptr, "glDrawElementsInstancedBaseVertexBaseInstanceEXT");
}
static void glad_gl_load_GL_EXT_blend_func_extended( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_blend_func_extended) return;
    glad_glBindFragDataLocationEXT = (PFNGLBINDFRAGDATALOCATIONEXTPROC) load(userptr, "glBindFragDataLocationEXT");
    glad_glBindFragDataLocationIndexedEXT = (PFNGLBINDFRAGDATALOCATIONINDEXEDEXTPROC) load(userptr, "glBindFragDataLocationIndexedEXT");
    glad_glGetFragDataIndexEXT = (PFNGLGETFRAGDATAINDEXEXTPROC) load(userptr, "glGetFragDataIndexEXT");
    glad_glGetProgramResourceLocationIndexEXT = (PFNGLGETPROGRAMRESOURCELOCATIONINDEXEXTPROC) load(userptr, "glGetProgramResourceLocationIndexEXT");
}
static void glad_gl_load_GL_EXT_buffer_storage( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_buffer_storage) return;
    glad_glBufferStorageEXT = (PFNGLBUFFERSTORAGEEXTPROC) load(userptr, "glBufferStorageEXT");
}
static void glad_gl_load_GL_EXT_clear_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_clear_texture) return;
    glad_glClearTexImageEXT = (PFNGLCLEARTEXIMAGEEXTPROC) load(userptr, "glClearTexImageEXT");
    glad_glClearTexSubImageEXT = (PFNGLCLEARTEXSUBIMAGEEXTPROC) load(userptr, "glClearTexSubImageEXT");
}
static void glad_gl_load_GL_EXT_clip_control( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_clip_control) return;
    glad_glClipControlEXT = (PFNGLCLIPCONTROLEXTPROC) load(userptr, "glClipControlEXT");
}
static void glad_gl_load_GL_EXT_copy_image( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_copy_image) return;
    glad_glCopyImageSubDataEXT = (PFNGLCOPYIMAGESUBDATAEXTPROC) load(userptr, "glCopyImageSubDataEXT");
}
static void glad_gl_load_GL_EXT_debug_label( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_debug_label) return;
    glad_glGetObjectLabelEXT = (PFNGLGETOBJECTLABELEXTPROC) load(userptr, "glGetObjectLabelEXT");
    glad_glLabelObjectEXT = (PFNGLLABELOBJECTEXTPROC) load(userptr, "glLabelObjectEXT");
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
static void glad_gl_load_GL_EXT_disjoint_timer_query( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_disjoint_timer_query) return;
    glad_glBeginQueryEXT = (PFNGLBEGINQUERYEXTPROC) load(userptr, "glBeginQueryEXT");
    glad_glDeleteQueriesEXT = (PFNGLDELETEQUERIESEXTPROC) load(userptr, "glDeleteQueriesEXT");
    glad_glEndQueryEXT = (PFNGLENDQUERYEXTPROC) load(userptr, "glEndQueryEXT");
    glad_glGenQueriesEXT = (PFNGLGENQUERIESEXTPROC) load(userptr, "glGenQueriesEXT");
    glad_glGetInteger64vEXT = (PFNGLGETINTEGER64VEXTPROC) load(userptr, "glGetInteger64vEXT");
    glad_glGetQueryObjecti64vEXT = (PFNGLGETQUERYOBJECTI64VEXTPROC) load(userptr, "glGetQueryObjecti64vEXT");
    glad_glGetQueryObjectivEXT = (PFNGLGETQUERYOBJECTIVEXTPROC) load(userptr, "glGetQueryObjectivEXT");
    glad_glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC) load(userptr, "glGetQueryObjectui64vEXT");
    glad_glGetQueryObjectuivEXT = (PFNGLGETQUERYOBJECTUIVEXTPROC) load(userptr, "glGetQueryObjectuivEXT");
    glad_glGetQueryivEXT = (PFNGLGETQUERYIVEXTPROC) load(userptr, "glGetQueryivEXT");
    glad_glIsQueryEXT = (PFNGLISQUERYEXTPROC) load(userptr, "glIsQueryEXT");
    glad_glQueryCounterEXT = (PFNGLQUERYCOUNTEREXTPROC) load(userptr, "glQueryCounterEXT");
}
static void glad_gl_load_GL_EXT_draw_buffers( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_draw_buffers) return;
    glad_glDrawBuffersEXT = (PFNGLDRAWBUFFERSEXTPROC) load(userptr, "glDrawBuffersEXT");
}
static void glad_gl_load_GL_EXT_draw_buffers_indexed( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_draw_buffers_indexed) return;
    glad_glBlendEquationSeparateiEXT = (PFNGLBLENDEQUATIONSEPARATEIEXTPROC) load(userptr, "glBlendEquationSeparateiEXT");
    glad_glBlendEquationiEXT = (PFNGLBLENDEQUATIONIEXTPROC) load(userptr, "glBlendEquationiEXT");
    glad_glBlendFuncSeparateiEXT = (PFNGLBLENDFUNCSEPARATEIEXTPROC) load(userptr, "glBlendFuncSeparateiEXT");
    glad_glBlendFunciEXT = (PFNGLBLENDFUNCIEXTPROC) load(userptr, "glBlendFunciEXT");
    glad_glColorMaskiEXT = (PFNGLCOLORMASKIEXTPROC) load(userptr, "glColorMaskiEXT");
    glad_glDisableiEXT = (PFNGLDISABLEIEXTPROC) load(userptr, "glDisableiEXT");
    glad_glEnableiEXT = (PFNGLENABLEIEXTPROC) load(userptr, "glEnableiEXT");
    glad_glIsEnablediEXT = (PFNGLISENABLEDIEXTPROC) load(userptr, "glIsEnablediEXT");
}
static void glad_gl_load_GL_EXT_draw_elements_base_vertex( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_draw_elements_base_vertex) return;
    glad_glDrawElementsBaseVertexEXT = (PFNGLDRAWELEMENTSBASEVERTEXEXTPROC) load(userptr, "glDrawElementsBaseVertexEXT");
    glad_glDrawElementsInstancedBaseVertexEXT = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXEXTPROC) load(userptr, "glDrawElementsInstancedBaseVertexEXT");
    glad_glDrawRangeElementsBaseVertexEXT = (PFNGLDRAWRANGEELEMENTSBASEVERTEXEXTPROC) load(userptr, "glDrawRangeElementsBaseVertexEXT");
    glad_glMultiDrawElementsBaseVertexEXT = (PFNGLMULTIDRAWELEMENTSBASEVERTEXEXTPROC) load(userptr, "glMultiDrawElementsBaseVertexEXT");
}
static void glad_gl_load_GL_EXT_draw_instanced( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_draw_instanced) return;
    glad_glDrawArraysInstancedEXT = (PFNGLDRAWARRAYSINSTANCEDEXTPROC) load(userptr, "glDrawArraysInstancedEXT");
    glad_glDrawElementsInstancedEXT = (PFNGLDRAWELEMENTSINSTANCEDEXTPROC) load(userptr, "glDrawElementsInstancedEXT");
}
static void glad_gl_load_GL_EXT_draw_transform_feedback( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_draw_transform_feedback) return;
    glad_glDrawTransformFeedbackEXT = (PFNGLDRAWTRANSFORMFEEDBACKEXTPROC) load(userptr, "glDrawTransformFeedbackEXT");
    glad_glDrawTransformFeedbackInstancedEXT = (PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDEXTPROC) load(userptr, "glDrawTransformFeedbackInstancedEXT");
}
static void glad_gl_load_GL_EXT_external_buffer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_external_buffer) return;
    glad_glBufferStorageExternalEXT = (PFNGLBUFFERSTORAGEEXTERNALEXTPROC) load(userptr, "glBufferStorageExternalEXT");
    glad_glNamedBufferStorageExternalEXT = (PFNGLNAMEDBUFFERSTORAGEEXTERNALEXTPROC) load(userptr, "glNamedBufferStorageExternalEXT");
}
static void glad_gl_load_GL_EXT_fragment_shading_rate( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_fragment_shading_rate) return;
    glad_glFramebufferShadingRateEXT = (PFNGLFRAMEBUFFERSHADINGRATEEXTPROC) load(userptr, "glFramebufferShadingRateEXT");
    glad_glGetFragmentShadingRatesEXT = (PFNGLGETFRAGMENTSHADINGRATESEXTPROC) load(userptr, "glGetFragmentShadingRatesEXT");
    glad_glShadingRateCombinerOpsEXT = (PFNGLSHADINGRATECOMBINEROPSEXTPROC) load(userptr, "glShadingRateCombinerOpsEXT");
    glad_glShadingRateEXT = (PFNGLSHADINGRATEEXTPROC) load(userptr, "glShadingRateEXT");
}
static void glad_gl_load_GL_EXT_framebuffer_blit_layers( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_framebuffer_blit_layers) return;
    glad_glBlitFramebufferLayerEXT = (PFNGLBLITFRAMEBUFFERLAYEREXTPROC) load(userptr, "glBlitFramebufferLayerEXT");
    glad_glBlitFramebufferLayersEXT = (PFNGLBLITFRAMEBUFFERLAYERSEXTPROC) load(userptr, "glBlitFramebufferLayersEXT");
}
static void glad_gl_load_GL_EXT_geometry_shader( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_geometry_shader) return;
    glad_glFramebufferTextureEXT = (PFNGLFRAMEBUFFERTEXTUREEXTPROC) load(userptr, "glFramebufferTextureEXT");
}
static void glad_gl_load_GL_EXT_instanced_arrays( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_instanced_arrays) return;
    glad_glDrawArraysInstancedEXT = (PFNGLDRAWARRAYSINSTANCEDEXTPROC) load(userptr, "glDrawArraysInstancedEXT");
    glad_glDrawElementsInstancedEXT = (PFNGLDRAWELEMENTSINSTANCEDEXTPROC) load(userptr, "glDrawElementsInstancedEXT");
    glad_glVertexAttribDivisorEXT = (PFNGLVERTEXATTRIBDIVISOREXTPROC) load(userptr, "glVertexAttribDivisorEXT");
}
static void glad_gl_load_GL_EXT_map_buffer_range( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_map_buffer_range) return;
    glad_glFlushMappedBufferRangeEXT = (PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC) load(userptr, "glFlushMappedBufferRangeEXT");
    glad_glMapBufferRangeEXT = (PFNGLMAPBUFFERRANGEEXTPROC) load(userptr, "glMapBufferRangeEXT");
}
static void glad_gl_load_GL_EXT_memory_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_memory_object) return;
    glad_glBufferStorageMemEXT = (PFNGLBUFFERSTORAGEMEMEXTPROC) load(userptr, "glBufferStorageMemEXT");
    glad_glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC) load(userptr, "glCreateMemoryObjectsEXT");
    glad_glDeleteMemoryObjectsEXT = (PFNGLDELETEMEMORYOBJECTSEXTPROC) load(userptr, "glDeleteMemoryObjectsEXT");
    glad_glGetMemoryObjectParameterivEXT = (PFNGLGETMEMORYOBJECTPARAMETERIVEXTPROC) load(userptr, "glGetMemoryObjectParameterivEXT");
    glad_glGetUnsignedBytei_vEXT = (PFNGLGETUNSIGNEDBYTEI_VEXTPROC) load(userptr, "glGetUnsignedBytei_vEXT");
    glad_glGetUnsignedBytevEXT = (PFNGLGETUNSIGNEDBYTEVEXTPROC) load(userptr, "glGetUnsignedBytevEXT");
    glad_glIsMemoryObjectEXT = (PFNGLISMEMORYOBJECTEXTPROC) load(userptr, "glIsMemoryObjectEXT");
    glad_glMemoryObjectParameterivEXT = (PFNGLMEMORYOBJECTPARAMETERIVEXTPROC) load(userptr, "glMemoryObjectParameterivEXT");
    glad_glNamedBufferStorageMemEXT = (PFNGLNAMEDBUFFERSTORAGEMEMEXTPROC) load(userptr, "glNamedBufferStorageMemEXT");
    glad_glTexStorageMem2DEXT = (PFNGLTEXSTORAGEMEM2DEXTPROC) load(userptr, "glTexStorageMem2DEXT");
    glad_glTexStorageMem2DMultisampleEXT = (PFNGLTEXSTORAGEMEM2DMULTISAMPLEEXTPROC) load(userptr, "glTexStorageMem2DMultisampleEXT");
    glad_glTexStorageMem3DEXT = (PFNGLTEXSTORAGEMEM3DEXTPROC) load(userptr, "glTexStorageMem3DEXT");
    glad_glTexStorageMem3DMultisampleEXT = (PFNGLTEXSTORAGEMEM3DMULTISAMPLEEXTPROC) load(userptr, "glTexStorageMem3DMultisampleEXT");
    glad_glTextureStorageMem2DEXT = (PFNGLTEXTURESTORAGEMEM2DEXTPROC) load(userptr, "glTextureStorageMem2DEXT");
    glad_glTextureStorageMem2DMultisampleEXT = (PFNGLTEXTURESTORAGEMEM2DMULTISAMPLEEXTPROC) load(userptr, "glTextureStorageMem2DMultisampleEXT");
    glad_glTextureStorageMem3DEXT = (PFNGLTEXTURESTORAGEMEM3DEXTPROC) load(userptr, "glTextureStorageMem3DEXT");
    glad_glTextureStorageMem3DMultisampleEXT = (PFNGLTEXTURESTORAGEMEM3DMULTISAMPLEEXTPROC) load(userptr, "glTextureStorageMem3DMultisampleEXT");
}
static void glad_gl_load_GL_EXT_memory_object_fd( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_memory_object_fd) return;
    glad_glImportMemoryFdEXT = (PFNGLIMPORTMEMORYFDEXTPROC) load(userptr, "glImportMemoryFdEXT");
}
static void glad_gl_load_GL_EXT_memory_object_win32( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_memory_object_win32) return;
    glad_glImportMemoryWin32HandleEXT = (PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC) load(userptr, "glImportMemoryWin32HandleEXT");
    glad_glImportMemoryWin32NameEXT = (PFNGLIMPORTMEMORYWIN32NAMEEXTPROC) load(userptr, "glImportMemoryWin32NameEXT");
}
static void glad_gl_load_GL_EXT_mesh_shader( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_mesh_shader) return;
    glad_glDrawMeshTasksEXT = (PFNGLDRAWMESHTASKSEXTPROC) load(userptr, "glDrawMeshTasksEXT");
    glad_glDrawMeshTasksIndirectEXT = (PFNGLDRAWMESHTASKSINDIRECTEXTPROC) load(userptr, "glDrawMeshTasksIndirectEXT");
    glad_glMultiDrawMeshTasksIndirectCountEXT = (PFNGLMULTIDRAWMESHTASKSINDIRECTCOUNTEXTPROC) load(userptr, "glMultiDrawMeshTasksIndirectCountEXT");
    glad_glMultiDrawMeshTasksIndirectEXT = (PFNGLMULTIDRAWMESHTASKSINDIRECTEXTPROC) load(userptr, "glMultiDrawMeshTasksIndirectEXT");
}
static void glad_gl_load_GL_EXT_multi_draw_arrays( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_multi_draw_arrays) return;
    glad_glMultiDrawArraysEXT = (PFNGLMULTIDRAWARRAYSEXTPROC) load(userptr, "glMultiDrawArraysEXT");
    glad_glMultiDrawElementsEXT = (PFNGLMULTIDRAWELEMENTSEXTPROC) load(userptr, "glMultiDrawElementsEXT");
}
static void glad_gl_load_GL_EXT_multi_draw_indirect( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_multi_draw_indirect) return;
    glad_glMultiDrawArraysIndirectEXT = (PFNGLMULTIDRAWARRAYSINDIRECTEXTPROC) load(userptr, "glMultiDrawArraysIndirectEXT");
    glad_glMultiDrawElementsIndirectEXT = (PFNGLMULTIDRAWELEMENTSINDIRECTEXTPROC) load(userptr, "glMultiDrawElementsIndirectEXT");
}
static void glad_gl_load_GL_EXT_multisampled_render_to_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_multisampled_render_to_texture) return;
    glad_glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) load(userptr, "glFramebufferTexture2DMultisampleEXT");
    glad_glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) load(userptr, "glRenderbufferStorageMultisampleEXT");
}
static void glad_gl_load_GL_EXT_multiview_draw_buffers( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_multiview_draw_buffers) return;
    glad_glDrawBuffersIndexedEXT = (PFNGLDRAWBUFFERSINDEXEDEXTPROC) load(userptr, "glDrawBuffersIndexedEXT");
    glad_glGetIntegeri_vEXT = (PFNGLGETINTEGERI_VEXTPROC) load(userptr, "glGetIntegeri_vEXT");
    glad_glReadBufferIndexedEXT = (PFNGLREADBUFFERINDEXEDEXTPROC) load(userptr, "glReadBufferIndexedEXT");
}
static void glad_gl_load_GL_EXT_occlusion_query_boolean( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_occlusion_query_boolean) return;
    glad_glBeginQueryEXT = (PFNGLBEGINQUERYEXTPROC) load(userptr, "glBeginQueryEXT");
    glad_glDeleteQueriesEXT = (PFNGLDELETEQUERIESEXTPROC) load(userptr, "glDeleteQueriesEXT");
    glad_glEndQueryEXT = (PFNGLENDQUERYEXTPROC) load(userptr, "glEndQueryEXT");
    glad_glGenQueriesEXT = (PFNGLGENQUERIESEXTPROC) load(userptr, "glGenQueriesEXT");
    glad_glGetQueryObjectuivEXT = (PFNGLGETQUERYOBJECTUIVEXTPROC) load(userptr, "glGetQueryObjectuivEXT");
    glad_glGetQueryivEXT = (PFNGLGETQUERYIVEXTPROC) load(userptr, "glGetQueryivEXT");
    glad_glIsQueryEXT = (PFNGLISQUERYEXTPROC) load(userptr, "glIsQueryEXT");
}
static void glad_gl_load_GL_EXT_polygon_offset_clamp( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_polygon_offset_clamp) return;
    glad_glPolygonOffsetClampEXT = (PFNGLPOLYGONOFFSETCLAMPEXTPROC) load(userptr, "glPolygonOffsetClampEXT");
}
static void glad_gl_load_GL_EXT_primitive_bounding_box( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_primitive_bounding_box) return;
    glad_glPrimitiveBoundingBoxEXT = (PFNGLPRIMITIVEBOUNDINGBOXEXTPROC) load(userptr, "glPrimitiveBoundingBoxEXT");
}
static void glad_gl_load_GL_EXT_raster_multisample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_raster_multisample) return;
    glad_glRasterSamplesEXT = (PFNGLRASTERSAMPLESEXTPROC) load(userptr, "glRasterSamplesEXT");
}
static void glad_gl_load_GL_EXT_robustness( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_robustness) return;
    glad_glGetGraphicsResetStatusEXT = (PFNGLGETGRAPHICSRESETSTATUSEXTPROC) load(userptr, "glGetGraphicsResetStatusEXT");
    glad_glGetnUniformfvEXT = (PFNGLGETNUNIFORMFVEXTPROC) load(userptr, "glGetnUniformfvEXT");
    glad_glGetnUniformivEXT = (PFNGLGETNUNIFORMIVEXTPROC) load(userptr, "glGetnUniformivEXT");
    glad_glReadnPixelsEXT = (PFNGLREADNPIXELSEXTPROC) load(userptr, "glReadnPixelsEXT");
}
static void glad_gl_load_GL_EXT_semaphore( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_semaphore) return;
    glad_glDeleteSemaphoresEXT = (PFNGLDELETESEMAPHORESEXTPROC) load(userptr, "glDeleteSemaphoresEXT");
    glad_glGenSemaphoresEXT = (PFNGLGENSEMAPHORESEXTPROC) load(userptr, "glGenSemaphoresEXT");
    glad_glGetSemaphoreParameterui64vEXT = (PFNGLGETSEMAPHOREPARAMETERUI64VEXTPROC) load(userptr, "glGetSemaphoreParameterui64vEXT");
    glad_glGetUnsignedBytei_vEXT = (PFNGLGETUNSIGNEDBYTEI_VEXTPROC) load(userptr, "glGetUnsignedBytei_vEXT");
    glad_glGetUnsignedBytevEXT = (PFNGLGETUNSIGNEDBYTEVEXTPROC) load(userptr, "glGetUnsignedBytevEXT");
    glad_glIsSemaphoreEXT = (PFNGLISSEMAPHOREEXTPROC) load(userptr, "glIsSemaphoreEXT");
    glad_glSemaphoreParameterui64vEXT = (PFNGLSEMAPHOREPARAMETERUI64VEXTPROC) load(userptr, "glSemaphoreParameterui64vEXT");
    glad_glSignalSemaphoreEXT = (PFNGLSIGNALSEMAPHOREEXTPROC) load(userptr, "glSignalSemaphoreEXT");
    glad_glWaitSemaphoreEXT = (PFNGLWAITSEMAPHOREEXTPROC) load(userptr, "glWaitSemaphoreEXT");
}
static void glad_gl_load_GL_EXT_semaphore_fd( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_semaphore_fd) return;
    glad_glImportSemaphoreFdEXT = (PFNGLIMPORTSEMAPHOREFDEXTPROC) load(userptr, "glImportSemaphoreFdEXT");
}
static void glad_gl_load_GL_EXT_semaphore_win32( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_semaphore_win32) return;
    glad_glImportSemaphoreWin32HandleEXT = (PFNGLIMPORTSEMAPHOREWIN32HANDLEEXTPROC) load(userptr, "glImportSemaphoreWin32HandleEXT");
    glad_glImportSemaphoreWin32NameEXT = (PFNGLIMPORTSEMAPHOREWIN32NAMEEXTPROC) load(userptr, "glImportSemaphoreWin32NameEXT");
}
static void glad_gl_load_GL_EXT_separate_shader_objects( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_separate_shader_objects) return;
    glad_glActiveShaderProgramEXT = (PFNGLACTIVESHADERPROGRAMEXTPROC) load(userptr, "glActiveShaderProgramEXT");
    glad_glBindProgramPipelineEXT = (PFNGLBINDPROGRAMPIPELINEEXTPROC) load(userptr, "glBindProgramPipelineEXT");
    glad_glCreateShaderProgramvEXT = (PFNGLCREATESHADERPROGRAMVEXTPROC) load(userptr, "glCreateShaderProgramvEXT");
    glad_glDeleteProgramPipelinesEXT = (PFNGLDELETEPROGRAMPIPELINESEXTPROC) load(userptr, "glDeleteProgramPipelinesEXT");
    glad_glGenProgramPipelinesEXT = (PFNGLGENPROGRAMPIPELINESEXTPROC) load(userptr, "glGenProgramPipelinesEXT");
    glad_glGetProgramPipelineInfoLogEXT = (PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC) load(userptr, "glGetProgramPipelineInfoLogEXT");
    glad_glGetProgramPipelineivEXT = (PFNGLGETPROGRAMPIPELINEIVEXTPROC) load(userptr, "glGetProgramPipelineivEXT");
    glad_glIsProgramPipelineEXT = (PFNGLISPROGRAMPIPELINEEXTPROC) load(userptr, "glIsProgramPipelineEXT");
    glad_glProgramParameteriEXT = (PFNGLPROGRAMPARAMETERIEXTPROC) load(userptr, "glProgramParameteriEXT");
    glad_glProgramUniform1fEXT = (PFNGLPROGRAMUNIFORM1FEXTPROC) load(userptr, "glProgramUniform1fEXT");
    glad_glProgramUniform1fvEXT = (PFNGLPROGRAMUNIFORM1FVEXTPROC) load(userptr, "glProgramUniform1fvEXT");
    glad_glProgramUniform1iEXT = (PFNGLPROGRAMUNIFORM1IEXTPROC) load(userptr, "glProgramUniform1iEXT");
    glad_glProgramUniform1ivEXT = (PFNGLPROGRAMUNIFORM1IVEXTPROC) load(userptr, "glProgramUniform1ivEXT");
    glad_glProgramUniform1uiEXT = (PFNGLPROGRAMUNIFORM1UIEXTPROC) load(userptr, "glProgramUniform1uiEXT");
    glad_glProgramUniform1uivEXT = (PFNGLPROGRAMUNIFORM1UIVEXTPROC) load(userptr, "glProgramUniform1uivEXT");
    glad_glProgramUniform2fEXT = (PFNGLPROGRAMUNIFORM2FEXTPROC) load(userptr, "glProgramUniform2fEXT");
    glad_glProgramUniform2fvEXT = (PFNGLPROGRAMUNIFORM2FVEXTPROC) load(userptr, "glProgramUniform2fvEXT");
    glad_glProgramUniform2iEXT = (PFNGLPROGRAMUNIFORM2IEXTPROC) load(userptr, "glProgramUniform2iEXT");
    glad_glProgramUniform2ivEXT = (PFNGLPROGRAMUNIFORM2IVEXTPROC) load(userptr, "glProgramUniform2ivEXT");
    glad_glProgramUniform2uiEXT = (PFNGLPROGRAMUNIFORM2UIEXTPROC) load(userptr, "glProgramUniform2uiEXT");
    glad_glProgramUniform2uivEXT = (PFNGLPROGRAMUNIFORM2UIVEXTPROC) load(userptr, "glProgramUniform2uivEXT");
    glad_glProgramUniform3fEXT = (PFNGLPROGRAMUNIFORM3FEXTPROC) load(userptr, "glProgramUniform3fEXT");
    glad_glProgramUniform3fvEXT = (PFNGLPROGRAMUNIFORM3FVEXTPROC) load(userptr, "glProgramUniform3fvEXT");
    glad_glProgramUniform3iEXT = (PFNGLPROGRAMUNIFORM3IEXTPROC) load(userptr, "glProgramUniform3iEXT");
    glad_glProgramUniform3ivEXT = (PFNGLPROGRAMUNIFORM3IVEXTPROC) load(userptr, "glProgramUniform3ivEXT");
    glad_glProgramUniform3uiEXT = (PFNGLPROGRAMUNIFORM3UIEXTPROC) load(userptr, "glProgramUniform3uiEXT");
    glad_glProgramUniform3uivEXT = (PFNGLPROGRAMUNIFORM3UIVEXTPROC) load(userptr, "glProgramUniform3uivEXT");
    glad_glProgramUniform4fEXT = (PFNGLPROGRAMUNIFORM4FEXTPROC) load(userptr, "glProgramUniform4fEXT");
    glad_glProgramUniform4fvEXT = (PFNGLPROGRAMUNIFORM4FVEXTPROC) load(userptr, "glProgramUniform4fvEXT");
    glad_glProgramUniform4iEXT = (PFNGLPROGRAMUNIFORM4IEXTPROC) load(userptr, "glProgramUniform4iEXT");
    glad_glProgramUniform4ivEXT = (PFNGLPROGRAMUNIFORM4IVEXTPROC) load(userptr, "glProgramUniform4ivEXT");
    glad_glProgramUniform4uiEXT = (PFNGLPROGRAMUNIFORM4UIEXTPROC) load(userptr, "glProgramUniform4uiEXT");
    glad_glProgramUniform4uivEXT = (PFNGLPROGRAMUNIFORM4UIVEXTPROC) load(userptr, "glProgramUniform4uivEXT");
    glad_glProgramUniformMatrix2fvEXT = (PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC) load(userptr, "glProgramUniformMatrix2fvEXT");
    glad_glProgramUniformMatrix2x3fvEXT = (PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC) load(userptr, "glProgramUniformMatrix2x3fvEXT");
    glad_glProgramUniformMatrix2x4fvEXT = (PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC) load(userptr, "glProgramUniformMatrix2x4fvEXT");
    glad_glProgramUniformMatrix3fvEXT = (PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC) load(userptr, "glProgramUniformMatrix3fvEXT");
    glad_glProgramUniformMatrix3x2fvEXT = (PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC) load(userptr, "glProgramUniformMatrix3x2fvEXT");
    glad_glProgramUniformMatrix3x4fvEXT = (PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC) load(userptr, "glProgramUniformMatrix3x4fvEXT");
    glad_glProgramUniformMatrix4fvEXT = (PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC) load(userptr, "glProgramUniformMatrix4fvEXT");
    glad_glProgramUniformMatrix4x2fvEXT = (PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC) load(userptr, "glProgramUniformMatrix4x2fvEXT");
    glad_glProgramUniformMatrix4x3fvEXT = (PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC) load(userptr, "glProgramUniformMatrix4x3fvEXT");
    glad_glUseProgramStagesEXT = (PFNGLUSEPROGRAMSTAGESEXTPROC) load(userptr, "glUseProgramStagesEXT");
    glad_glValidateProgramPipelineEXT = (PFNGLVALIDATEPROGRAMPIPELINEEXTPROC) load(userptr, "glValidateProgramPipelineEXT");
}
static void glad_gl_load_GL_EXT_shader_framebuffer_fetch_non_coherent( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_shader_framebuffer_fetch_non_coherent) return;
    glad_glFramebufferFetchBarrierEXT = (PFNGLFRAMEBUFFERFETCHBARRIEREXTPROC) load(userptr, "glFramebufferFetchBarrierEXT");
}
static void glad_gl_load_GL_EXT_shader_pixel_local_storage2( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_shader_pixel_local_storage2) return;
    glad_glClearPixelLocalStorageuiEXT = (PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC) load(userptr, "glClearPixelLocalStorageuiEXT");
    glad_glFramebufferPixelLocalStorageSizeEXT = (PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC) load(userptr, "glFramebufferPixelLocalStorageSizeEXT");
    glad_glGetFramebufferPixelLocalStorageSizeEXT = (PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC) load(userptr, "glGetFramebufferPixelLocalStorageSizeEXT");
}
static void glad_gl_load_GL_EXT_sparse_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_sparse_texture) return;
    glad_glTexPageCommitmentEXT = (PFNGLTEXPAGECOMMITMENTEXTPROC) load(userptr, "glTexPageCommitmentEXT");
}
static void glad_gl_load_GL_EXT_tessellation_shader( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_tessellation_shader) return;
    glad_glPatchParameteriEXT = (PFNGLPATCHPARAMETERIEXTPROC) load(userptr, "glPatchParameteriEXT");
}
static void glad_gl_load_GL_EXT_texture_border_clamp( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture_border_clamp) return;
    glad_glGetSamplerParameterIivEXT = (PFNGLGETSAMPLERPARAMETERIIVEXTPROC) load(userptr, "glGetSamplerParameterIivEXT");
    glad_glGetSamplerParameterIuivEXT = (PFNGLGETSAMPLERPARAMETERIUIVEXTPROC) load(userptr, "glGetSamplerParameterIuivEXT");
    glad_glGetTexParameterIivEXT = (PFNGLGETTEXPARAMETERIIVEXTPROC) load(userptr, "glGetTexParameterIivEXT");
    glad_glGetTexParameterIuivEXT = (PFNGLGETTEXPARAMETERIUIVEXTPROC) load(userptr, "glGetTexParameterIuivEXT");
    glad_glSamplerParameterIivEXT = (PFNGLSAMPLERPARAMETERIIVEXTPROC) load(userptr, "glSamplerParameterIivEXT");
    glad_glSamplerParameterIuivEXT = (PFNGLSAMPLERPARAMETERIUIVEXTPROC) load(userptr, "glSamplerParameterIuivEXT");
    glad_glTexParameterIivEXT = (PFNGLTEXPARAMETERIIVEXTPROC) load(userptr, "glTexParameterIivEXT");
    glad_glTexParameterIuivEXT = (PFNGLTEXPARAMETERIUIVEXTPROC) load(userptr, "glTexParameterIuivEXT");
}
static void glad_gl_load_GL_EXT_texture_buffer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture_buffer) return;
    glad_glTexBufferEXT = (PFNGLTEXBUFFEREXTPROC) load(userptr, "glTexBufferEXT");
    glad_glTexBufferRangeEXT = (PFNGLTEXBUFFERRANGEEXTPROC) load(userptr, "glTexBufferRangeEXT");
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
static void glad_gl_load_GL_EXT_texture_storage_compression( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture_storage_compression) return;
    glad_glTexStorageAttribs2DEXT = (PFNGLTEXSTORAGEATTRIBS2DEXTPROC) load(userptr, "glTexStorageAttribs2DEXT");
    glad_glTexStorageAttribs3DEXT = (PFNGLTEXSTORAGEATTRIBS3DEXTPROC) load(userptr, "glTexStorageAttribs3DEXT");
}
static void glad_gl_load_GL_EXT_texture_view( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_texture_view) return;
    glad_glTextureViewEXT = (PFNGLTEXTUREVIEWEXTPROC) load(userptr, "glTextureViewEXT");
}
static void glad_gl_load_GL_EXT_win32_keyed_mutex( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_win32_keyed_mutex) return;
    glad_glAcquireKeyedMutexWin32EXT = (PFNGLACQUIREKEYEDMUTEXWIN32EXTPROC) load(userptr, "glAcquireKeyedMutexWin32EXT");
    glad_glReleaseKeyedMutexWin32EXT = (PFNGLRELEASEKEYEDMUTEXWIN32EXTPROC) load(userptr, "glReleaseKeyedMutexWin32EXT");
}
static void glad_gl_load_GL_EXT_window_rectangles( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_EXT_window_rectangles) return;
    glad_glWindowRectanglesEXT = (PFNGLWINDOWRECTANGLESEXTPROC) load(userptr, "glWindowRectanglesEXT");
}
static void glad_gl_load_GL_IMG_bindless_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_IMG_bindless_texture) return;
    glad_glGetTextureHandleIMG = (PFNGLGETTEXTUREHANDLEIMGPROC) load(userptr, "glGetTextureHandleIMG");
    glad_glGetTextureSamplerHandleIMG = (PFNGLGETTEXTURESAMPLERHANDLEIMGPROC) load(userptr, "glGetTextureSamplerHandleIMG");
    glad_glProgramUniformHandleui64IMG = (PFNGLPROGRAMUNIFORMHANDLEUI64IMGPROC) load(userptr, "glProgramUniformHandleui64IMG");
    glad_glProgramUniformHandleui64vIMG = (PFNGLPROGRAMUNIFORMHANDLEUI64VIMGPROC) load(userptr, "glProgramUniformHandleui64vIMG");
    glad_glUniformHandleui64IMG = (PFNGLUNIFORMHANDLEUI64IMGPROC) load(userptr, "glUniformHandleui64IMG");
    glad_glUniformHandleui64vIMG = (PFNGLUNIFORMHANDLEUI64VIMGPROC) load(userptr, "glUniformHandleui64vIMG");
}
static void glad_gl_load_GL_IMG_framebuffer_downsample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_IMG_framebuffer_downsample) return;
    glad_glFramebufferTexture2DDownsampleIMG = (PFNGLFRAMEBUFFERTEXTURE2DDOWNSAMPLEIMGPROC) load(userptr, "glFramebufferTexture2DDownsampleIMG");
    glad_glFramebufferTextureLayerDownsampleIMG = (PFNGLFRAMEBUFFERTEXTURELAYERDOWNSAMPLEIMGPROC) load(userptr, "glFramebufferTextureLayerDownsampleIMG");
}
static void glad_gl_load_GL_IMG_multisampled_render_to_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_IMG_multisampled_render_to_texture) return;
    glad_glFramebufferTexture2DMultisampleIMG = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC) load(userptr, "glFramebufferTexture2DMultisampleIMG");
    glad_glRenderbufferStorageMultisampleIMG = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC) load(userptr, "glRenderbufferStorageMultisampleIMG");
}
static void glad_gl_load_GL_INTEL_framebuffer_CMAA( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_INTEL_framebuffer_CMAA) return;
    glad_glApplyFramebufferAttachmentCMAAINTEL = (PFNGLAPPLYFRAMEBUFFERATTACHMENTCMAAINTELPROC) load(userptr, "glApplyFramebufferAttachmentCMAAINTEL");
}
static void glad_gl_load_GL_INTEL_performance_query( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_INTEL_performance_query) return;
    glad_glBeginPerfQueryINTEL = (PFNGLBEGINPERFQUERYINTELPROC) load(userptr, "glBeginPerfQueryINTEL");
    glad_glCreatePerfQueryINTEL = (PFNGLCREATEPERFQUERYINTELPROC) load(userptr, "glCreatePerfQueryINTEL");
    glad_glDeletePerfQueryINTEL = (PFNGLDELETEPERFQUERYINTELPROC) load(userptr, "glDeletePerfQueryINTEL");
    glad_glEndPerfQueryINTEL = (PFNGLENDPERFQUERYINTELPROC) load(userptr, "glEndPerfQueryINTEL");
    glad_glGetFirstPerfQueryIdINTEL = (PFNGLGETFIRSTPERFQUERYIDINTELPROC) load(userptr, "glGetFirstPerfQueryIdINTEL");
    glad_glGetNextPerfQueryIdINTEL = (PFNGLGETNEXTPERFQUERYIDINTELPROC) load(userptr, "glGetNextPerfQueryIdINTEL");
    glad_glGetPerfCounterInfoINTEL = (PFNGLGETPERFCOUNTERINFOINTELPROC) load(userptr, "glGetPerfCounterInfoINTEL");
    glad_glGetPerfQueryDataINTEL = (PFNGLGETPERFQUERYDATAINTELPROC) load(userptr, "glGetPerfQueryDataINTEL");
    glad_glGetPerfQueryIdByNameINTEL = (PFNGLGETPERFQUERYIDBYNAMEINTELPROC) load(userptr, "glGetPerfQueryIdByNameINTEL");
    glad_glGetPerfQueryInfoINTEL = (PFNGLGETPERFQUERYINFOINTELPROC) load(userptr, "glGetPerfQueryInfoINTEL");
}
static void glad_gl_load_GL_KHR_blend_equation_advanced( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_KHR_blend_equation_advanced) return;
    glad_glBlendBarrierKHR = (PFNGLBLENDBARRIERKHRPROC) load(userptr, "glBlendBarrierKHR");
}
static void glad_gl_load_GL_KHR_debug( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_KHR_debug) return;
    glad_glDebugMessageCallbackKHR = (PFNGLDEBUGMESSAGECALLBACKKHRPROC) load(userptr, "glDebugMessageCallbackKHR");
    glad_glDebugMessageControlKHR = (PFNGLDEBUGMESSAGECONTROLKHRPROC) load(userptr, "glDebugMessageControlKHR");
    glad_glDebugMessageInsertKHR = (PFNGLDEBUGMESSAGEINSERTKHRPROC) load(userptr, "glDebugMessageInsertKHR");
    glad_glGetDebugMessageLogKHR = (PFNGLGETDEBUGMESSAGELOGKHRPROC) load(userptr, "glGetDebugMessageLogKHR");
    glad_glGetObjectLabelKHR = (PFNGLGETOBJECTLABELKHRPROC) load(userptr, "glGetObjectLabelKHR");
    glad_glGetObjectPtrLabelKHR = (PFNGLGETOBJECTPTRLABELKHRPROC) load(userptr, "glGetObjectPtrLabelKHR");
    glad_glGetPointervKHR = (PFNGLGETPOINTERVKHRPROC) load(userptr, "glGetPointervKHR");
    glad_glObjectLabelKHR = (PFNGLOBJECTLABELKHRPROC) load(userptr, "glObjectLabelKHR");
    glad_glObjectPtrLabelKHR = (PFNGLOBJECTPTRLABELKHRPROC) load(userptr, "glObjectPtrLabelKHR");
    glad_glPopDebugGroupKHR = (PFNGLPOPDEBUGGROUPKHRPROC) load(userptr, "glPopDebugGroupKHR");
    glad_glPushDebugGroupKHR = (PFNGLPUSHDEBUGGROUPKHRPROC) load(userptr, "glPushDebugGroupKHR");
}
static void glad_gl_load_GL_KHR_parallel_shader_compile( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_KHR_parallel_shader_compile) return;
    glad_glMaxShaderCompilerThreadsKHR = (PFNGLMAXSHADERCOMPILERTHREADSKHRPROC) load(userptr, "glMaxShaderCompilerThreadsKHR");
}
static void glad_gl_load_GL_KHR_robustness( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_KHR_robustness) return;
    glad_glGetGraphicsResetStatusKHR = (PFNGLGETGRAPHICSRESETSTATUSKHRPROC) load(userptr, "glGetGraphicsResetStatusKHR");
    glad_glGetnUniformfvKHR = (PFNGLGETNUNIFORMFVKHRPROC) load(userptr, "glGetnUniformfvKHR");
    glad_glGetnUniformivKHR = (PFNGLGETNUNIFORMIVKHRPROC) load(userptr, "glGetnUniformivKHR");
    glad_glGetnUniformuivKHR = (PFNGLGETNUNIFORMUIVKHRPROC) load(userptr, "glGetnUniformuivKHR");
    glad_glReadnPixelsKHR = (PFNGLREADNPIXELSKHRPROC) load(userptr, "glReadnPixelsKHR");
}
static void glad_gl_load_GL_MESA_framebuffer_flip_y( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_MESA_framebuffer_flip_y) return;
    glad_glFramebufferParameteriMESA = (PFNGLFRAMEBUFFERPARAMETERIMESAPROC) load(userptr, "glFramebufferParameteriMESA");
    glad_glGetFramebufferParameterivMESA = (PFNGLGETFRAMEBUFFERPARAMETERIVMESAPROC) load(userptr, "glGetFramebufferParameterivMESA");
}
static void glad_gl_load_GL_MESA_sampler_objects( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_MESA_sampler_objects) return;
    glad_glBindSampler = (PFNGLBINDSAMPLERPROC) load(userptr, "glBindSampler");
    glad_glDeleteSamplers = (PFNGLDELETESAMPLERSPROC) load(userptr, "glDeleteSamplers");
    glad_glGenSamplers = (PFNGLGENSAMPLERSPROC) load(userptr, "glGenSamplers");
    glad_glGetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC) load(userptr, "glGetSamplerParameterfv");
    glad_glGetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC) load(userptr, "glGetSamplerParameteriv");
    glad_glIsSampler = (PFNGLISSAMPLERPROC) load(userptr, "glIsSampler");
    glad_glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC) load(userptr, "glSamplerParameterf");
    glad_glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC) load(userptr, "glSamplerParameterfv");
    glad_glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC) load(userptr, "glSamplerParameteri");
    glad_glSamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC) load(userptr, "glSamplerParameteriv");
}
static void glad_gl_load_GL_NV_bindless_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_bindless_texture) return;
    glad_glGetImageHandleNV = (PFNGLGETIMAGEHANDLENVPROC) load(userptr, "glGetImageHandleNV");
    glad_glGetTextureHandleNV = (PFNGLGETTEXTUREHANDLENVPROC) load(userptr, "glGetTextureHandleNV");
    glad_glGetTextureSamplerHandleNV = (PFNGLGETTEXTURESAMPLERHANDLENVPROC) load(userptr, "glGetTextureSamplerHandleNV");
    glad_glIsImageHandleResidentNV = (PFNGLISIMAGEHANDLERESIDENTNVPROC) load(userptr, "glIsImageHandleResidentNV");
    glad_glIsTextureHandleResidentNV = (PFNGLISTEXTUREHANDLERESIDENTNVPROC) load(userptr, "glIsTextureHandleResidentNV");
    glad_glMakeImageHandleNonResidentNV = (PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC) load(userptr, "glMakeImageHandleNonResidentNV");
    glad_glMakeImageHandleResidentNV = (PFNGLMAKEIMAGEHANDLERESIDENTNVPROC) load(userptr, "glMakeImageHandleResidentNV");
    glad_glMakeTextureHandleNonResidentNV = (PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC) load(userptr, "glMakeTextureHandleNonResidentNV");
    glad_glMakeTextureHandleResidentNV = (PFNGLMAKETEXTUREHANDLERESIDENTNVPROC) load(userptr, "glMakeTextureHandleResidentNV");
    glad_glProgramUniformHandleui64NV = (PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC) load(userptr, "glProgramUniformHandleui64NV");
    glad_glProgramUniformHandleui64vNV = (PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC) load(userptr, "glProgramUniformHandleui64vNV");
    glad_glUniformHandleui64NV = (PFNGLUNIFORMHANDLEUI64NVPROC) load(userptr, "glUniformHandleui64NV");
    glad_glUniformHandleui64vNV = (PFNGLUNIFORMHANDLEUI64VNVPROC) load(userptr, "glUniformHandleui64vNV");
}
static void glad_gl_load_GL_NV_blend_equation_advanced( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_blend_equation_advanced) return;
    glad_glBlendBarrierNV = (PFNGLBLENDBARRIERNVPROC) load(userptr, "glBlendBarrierNV");
    glad_glBlendParameteriNV = (PFNGLBLENDPARAMETERINVPROC) load(userptr, "glBlendParameteriNV");
}
static void glad_gl_load_GL_NV_clip_space_w_scaling( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_clip_space_w_scaling) return;
    glad_glViewportPositionWScaleNV = (PFNGLVIEWPORTPOSITIONWSCALENVPROC) load(userptr, "glViewportPositionWScaleNV");
}
static void glad_gl_load_GL_NV_conditional_render( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_conditional_render) return;
    glad_glBeginConditionalRenderNV = (PFNGLBEGINCONDITIONALRENDERNVPROC) load(userptr, "glBeginConditionalRenderNV");
    glad_glEndConditionalRenderNV = (PFNGLENDCONDITIONALRENDERNVPROC) load(userptr, "glEndConditionalRenderNV");
}
static void glad_gl_load_GL_NV_conservative_raster( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_conservative_raster) return;
    glad_glSubpixelPrecisionBiasNV = (PFNGLSUBPIXELPRECISIONBIASNVPROC) load(userptr, "glSubpixelPrecisionBiasNV");
}
static void glad_gl_load_GL_NV_conservative_raster_pre_snap_triangles( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_conservative_raster_pre_snap_triangles) return;
    glad_glConservativeRasterParameteriNV = (PFNGLCONSERVATIVERASTERPARAMETERINVPROC) load(userptr, "glConservativeRasterParameteriNV");
}
static void glad_gl_load_GL_NV_copy_buffer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_copy_buffer) return;
    glad_glCopyBufferSubDataNV = (PFNGLCOPYBUFFERSUBDATANVPROC) load(userptr, "glCopyBufferSubDataNV");
}
static void glad_gl_load_GL_NV_coverage_sample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_coverage_sample) return;
    glad_glCoverageMaskNV = (PFNGLCOVERAGEMASKNVPROC) load(userptr, "glCoverageMaskNV");
    glad_glCoverageOperationNV = (PFNGLCOVERAGEOPERATIONNVPROC) load(userptr, "glCoverageOperationNV");
}
static void glad_gl_load_GL_NV_draw_buffers( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_draw_buffers) return;
    glad_glDrawBuffersNV = (PFNGLDRAWBUFFERSNVPROC) load(userptr, "glDrawBuffersNV");
}
static void glad_gl_load_GL_NV_draw_instanced( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_draw_instanced) return;
    glad_glDrawArraysInstancedNV = (PFNGLDRAWARRAYSINSTANCEDNVPROC) load(userptr, "glDrawArraysInstancedNV");
    glad_glDrawElementsInstancedNV = (PFNGLDRAWELEMENTSINSTANCEDNVPROC) load(userptr, "glDrawElementsInstancedNV");
}
static void glad_gl_load_GL_NV_draw_vulkan_image( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_draw_vulkan_image) return;
    glad_glDrawVkImageNV = (PFNGLDRAWVKIMAGENVPROC) load(userptr, "glDrawVkImageNV");
    glad_glGetVkProcAddrNV = (PFNGLGETVKPROCADDRNVPROC) load(userptr, "glGetVkProcAddrNV");
    glad_glSignalVkFenceNV = (PFNGLSIGNALVKFENCENVPROC) load(userptr, "glSignalVkFenceNV");
    glad_glSignalVkSemaphoreNV = (PFNGLSIGNALVKSEMAPHORENVPROC) load(userptr, "glSignalVkSemaphoreNV");
    glad_glWaitVkSemaphoreNV = (PFNGLWAITVKSEMAPHORENVPROC) load(userptr, "glWaitVkSemaphoreNV");
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
static void glad_gl_load_GL_NV_fragment_coverage_to_color( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_fragment_coverage_to_color) return;
    glad_glFragmentCoverageColorNV = (PFNGLFRAGMENTCOVERAGECOLORNVPROC) load(userptr, "glFragmentCoverageColorNV");
}
static void glad_gl_load_GL_NV_framebuffer_blit( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_framebuffer_blit) return;
    glad_glBlitFramebufferNV = (PFNGLBLITFRAMEBUFFERNVPROC) load(userptr, "glBlitFramebufferNV");
}
static void glad_gl_load_GL_NV_framebuffer_mixed_samples( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_framebuffer_mixed_samples) return;
    glad_glCoverageModulationNV = (PFNGLCOVERAGEMODULATIONNVPROC) load(userptr, "glCoverageModulationNV");
    glad_glCoverageModulationTableNV = (PFNGLCOVERAGEMODULATIONTABLENVPROC) load(userptr, "glCoverageModulationTableNV");
    glad_glGetCoverageModulationTableNV = (PFNGLGETCOVERAGEMODULATIONTABLENVPROC) load(userptr, "glGetCoverageModulationTableNV");
    glad_glRasterSamplesEXT = (PFNGLRASTERSAMPLESEXTPROC) load(userptr, "glRasterSamplesEXT");
}
static void glad_gl_load_GL_NV_framebuffer_multisample( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_framebuffer_multisample) return;
    glad_glRenderbufferStorageMultisampleNV = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC) load(userptr, "glRenderbufferStorageMultisampleNV");
}
static void glad_gl_load_GL_NV_gpu_shader5( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_gpu_shader5) return;
    glad_glGetUniformi64vNV = (PFNGLGETUNIFORMI64VNVPROC) load(userptr, "glGetUniformi64vNV");
    glad_glProgramUniform1i64NV = (PFNGLPROGRAMUNIFORM1I64NVPROC) load(userptr, "glProgramUniform1i64NV");
    glad_glProgramUniform1i64vNV = (PFNGLPROGRAMUNIFORM1I64VNVPROC) load(userptr, "glProgramUniform1i64vNV");
    glad_glProgramUniform1ui64NV = (PFNGLPROGRAMUNIFORM1UI64NVPROC) load(userptr, "glProgramUniform1ui64NV");
    glad_glProgramUniform1ui64vNV = (PFNGLPROGRAMUNIFORM1UI64VNVPROC) load(userptr, "glProgramUniform1ui64vNV");
    glad_glProgramUniform2i64NV = (PFNGLPROGRAMUNIFORM2I64NVPROC) load(userptr, "glProgramUniform2i64NV");
    glad_glProgramUniform2i64vNV = (PFNGLPROGRAMUNIFORM2I64VNVPROC) load(userptr, "glProgramUniform2i64vNV");
    glad_glProgramUniform2ui64NV = (PFNGLPROGRAMUNIFORM2UI64NVPROC) load(userptr, "glProgramUniform2ui64NV");
    glad_glProgramUniform2ui64vNV = (PFNGLPROGRAMUNIFORM2UI64VNVPROC) load(userptr, "glProgramUniform2ui64vNV");
    glad_glProgramUniform3i64NV = (PFNGLPROGRAMUNIFORM3I64NVPROC) load(userptr, "glProgramUniform3i64NV");
    glad_glProgramUniform3i64vNV = (PFNGLPROGRAMUNIFORM3I64VNVPROC) load(userptr, "glProgramUniform3i64vNV");
    glad_glProgramUniform3ui64NV = (PFNGLPROGRAMUNIFORM3UI64NVPROC) load(userptr, "glProgramUniform3ui64NV");
    glad_glProgramUniform3ui64vNV = (PFNGLPROGRAMUNIFORM3UI64VNVPROC) load(userptr, "glProgramUniform3ui64vNV");
    glad_glProgramUniform4i64NV = (PFNGLPROGRAMUNIFORM4I64NVPROC) load(userptr, "glProgramUniform4i64NV");
    glad_glProgramUniform4i64vNV = (PFNGLPROGRAMUNIFORM4I64VNVPROC) load(userptr, "glProgramUniform4i64vNV");
    glad_glProgramUniform4ui64NV = (PFNGLPROGRAMUNIFORM4UI64NVPROC) load(userptr, "glProgramUniform4ui64NV");
    glad_glProgramUniform4ui64vNV = (PFNGLPROGRAMUNIFORM4UI64VNVPROC) load(userptr, "glProgramUniform4ui64vNV");
    glad_glUniform1i64NV = (PFNGLUNIFORM1I64NVPROC) load(userptr, "glUniform1i64NV");
    glad_glUniform1i64vNV = (PFNGLUNIFORM1I64VNVPROC) load(userptr, "glUniform1i64vNV");
    glad_glUniform1ui64NV = (PFNGLUNIFORM1UI64NVPROC) load(userptr, "glUniform1ui64NV");
    glad_glUniform1ui64vNV = (PFNGLUNIFORM1UI64VNVPROC) load(userptr, "glUniform1ui64vNV");
    glad_glUniform2i64NV = (PFNGLUNIFORM2I64NVPROC) load(userptr, "glUniform2i64NV");
    glad_glUniform2i64vNV = (PFNGLUNIFORM2I64VNVPROC) load(userptr, "glUniform2i64vNV");
    glad_glUniform2ui64NV = (PFNGLUNIFORM2UI64NVPROC) load(userptr, "glUniform2ui64NV");
    glad_glUniform2ui64vNV = (PFNGLUNIFORM2UI64VNVPROC) load(userptr, "glUniform2ui64vNV");
    glad_glUniform3i64NV = (PFNGLUNIFORM3I64NVPROC) load(userptr, "glUniform3i64NV");
    glad_glUniform3i64vNV = (PFNGLUNIFORM3I64VNVPROC) load(userptr, "glUniform3i64vNV");
    glad_glUniform3ui64NV = (PFNGLUNIFORM3UI64NVPROC) load(userptr, "glUniform3ui64NV");
    glad_glUniform3ui64vNV = (PFNGLUNIFORM3UI64VNVPROC) load(userptr, "glUniform3ui64vNV");
    glad_glUniform4i64NV = (PFNGLUNIFORM4I64NVPROC) load(userptr, "glUniform4i64NV");
    glad_glUniform4i64vNV = (PFNGLUNIFORM4I64VNVPROC) load(userptr, "glUniform4i64vNV");
    glad_glUniform4ui64NV = (PFNGLUNIFORM4UI64NVPROC) load(userptr, "glUniform4ui64NV");
    glad_glUniform4ui64vNV = (PFNGLUNIFORM4UI64VNVPROC) load(userptr, "glUniform4ui64vNV");
}
static void glad_gl_load_GL_NV_instanced_arrays( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_instanced_arrays) return;
    glad_glVertexAttribDivisorNV = (PFNGLVERTEXATTRIBDIVISORNVPROC) load(userptr, "glVertexAttribDivisorNV");
}
static void glad_gl_load_GL_NV_internalformat_sample_query( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_internalformat_sample_query) return;
    glad_glGetInternalformatSampleivNV = (PFNGLGETINTERNALFORMATSAMPLEIVNVPROC) load(userptr, "glGetInternalformatSampleivNV");
}
static void glad_gl_load_GL_NV_memory_attachment( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_memory_attachment) return;
    glad_glBufferAttachMemoryNV = (PFNGLBUFFERATTACHMEMORYNVPROC) load(userptr, "glBufferAttachMemoryNV");
    glad_glGetMemoryObjectDetachedResourcesuivNV = (PFNGLGETMEMORYOBJECTDETACHEDRESOURCESUIVNVPROC) load(userptr, "glGetMemoryObjectDetachedResourcesuivNV");
    glad_glNamedBufferAttachMemoryNV = (PFNGLNAMEDBUFFERATTACHMEMORYNVPROC) load(userptr, "glNamedBufferAttachMemoryNV");
    glad_glResetMemoryObjectParameterNV = (PFNGLRESETMEMORYOBJECTPARAMETERNVPROC) load(userptr, "glResetMemoryObjectParameterNV");
    glad_glTexAttachMemoryNV = (PFNGLTEXATTACHMEMORYNVPROC) load(userptr, "glTexAttachMemoryNV");
    glad_glTextureAttachMemoryNV = (PFNGLTEXTUREATTACHMEMORYNVPROC) load(userptr, "glTextureAttachMemoryNV");
}
static void glad_gl_load_GL_NV_memory_object_sparse( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_memory_object_sparse) return;
    glad_glBufferPageCommitmentMemNV = (PFNGLBUFFERPAGECOMMITMENTMEMNVPROC) load(userptr, "glBufferPageCommitmentMemNV");
    glad_glNamedBufferPageCommitmentMemNV = (PFNGLNAMEDBUFFERPAGECOMMITMENTMEMNVPROC) load(userptr, "glNamedBufferPageCommitmentMemNV");
    glad_glTexPageCommitmentMemNV = (PFNGLTEXPAGECOMMITMENTMEMNVPROC) load(userptr, "glTexPageCommitmentMemNV");
    glad_glTexturePageCommitmentMemNV = (PFNGLTEXTUREPAGECOMMITMENTMEMNVPROC) load(userptr, "glTexturePageCommitmentMemNV");
}
static void glad_gl_load_GL_NV_mesh_shader( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_mesh_shader) return;
    glad_glDrawMeshTasksIndirectNV = (PFNGLDRAWMESHTASKSINDIRECTNVPROC) load(userptr, "glDrawMeshTasksIndirectNV");
    glad_glDrawMeshTasksNV = (PFNGLDRAWMESHTASKSNVPROC) load(userptr, "glDrawMeshTasksNV");
    glad_glMultiDrawMeshTasksIndirectCountNV = (PFNGLMULTIDRAWMESHTASKSINDIRECTCOUNTNVPROC) load(userptr, "glMultiDrawMeshTasksIndirectCountNV");
    glad_glMultiDrawMeshTasksIndirectNV = (PFNGLMULTIDRAWMESHTASKSINDIRECTNVPROC) load(userptr, "glMultiDrawMeshTasksIndirectNV");
}
static void glad_gl_load_GL_NV_non_square_matrices( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_non_square_matrices) return;
    glad_glUniformMatrix2x3fvNV = (PFNGLUNIFORMMATRIX2X3FVNVPROC) load(userptr, "glUniformMatrix2x3fvNV");
    glad_glUniformMatrix2x4fvNV = (PFNGLUNIFORMMATRIX2X4FVNVPROC) load(userptr, "glUniformMatrix2x4fvNV");
    glad_glUniformMatrix3x2fvNV = (PFNGLUNIFORMMATRIX3X2FVNVPROC) load(userptr, "glUniformMatrix3x2fvNV");
    glad_glUniformMatrix3x4fvNV = (PFNGLUNIFORMMATRIX3X4FVNVPROC) load(userptr, "glUniformMatrix3x4fvNV");
    glad_glUniformMatrix4x2fvNV = (PFNGLUNIFORMMATRIX4X2FVNVPROC) load(userptr, "glUniformMatrix4x2fvNV");
    glad_glUniformMatrix4x3fvNV = (PFNGLUNIFORMMATRIX4X3FVNVPROC) load(userptr, "glUniformMatrix4x3fvNV");
}
static void glad_gl_load_GL_NV_path_rendering( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_path_rendering) return;
    glad_glCopyPathNV = (PFNGLCOPYPATHNVPROC) load(userptr, "glCopyPathNV");
    glad_glCoverFillPathInstancedNV = (PFNGLCOVERFILLPATHINSTANCEDNVPROC) load(userptr, "glCoverFillPathInstancedNV");
    glad_glCoverFillPathNV = (PFNGLCOVERFILLPATHNVPROC) load(userptr, "glCoverFillPathNV");
    glad_glCoverStrokePathInstancedNV = (PFNGLCOVERSTROKEPATHINSTANCEDNVPROC) load(userptr, "glCoverStrokePathInstancedNV");
    glad_glCoverStrokePathNV = (PFNGLCOVERSTROKEPATHNVPROC) load(userptr, "glCoverStrokePathNV");
    glad_glDeletePathsNV = (PFNGLDELETEPATHSNVPROC) load(userptr, "glDeletePathsNV");
    glad_glGenPathsNV = (PFNGLGENPATHSNVPROC) load(userptr, "glGenPathsNV");
    glad_glGetPathCommandsNV = (PFNGLGETPATHCOMMANDSNVPROC) load(userptr, "glGetPathCommandsNV");
    glad_glGetPathCoordsNV = (PFNGLGETPATHCOORDSNVPROC) load(userptr, "glGetPathCoordsNV");
    glad_glGetPathDashArrayNV = (PFNGLGETPATHDASHARRAYNVPROC) load(userptr, "glGetPathDashArrayNV");
    glad_glGetPathLengthNV = (PFNGLGETPATHLENGTHNVPROC) load(userptr, "glGetPathLengthNV");
    glad_glGetPathMetricRangeNV = (PFNGLGETPATHMETRICRANGENVPROC) load(userptr, "glGetPathMetricRangeNV");
    glad_glGetPathMetricsNV = (PFNGLGETPATHMETRICSNVPROC) load(userptr, "glGetPathMetricsNV");
    glad_glGetPathParameterfvNV = (PFNGLGETPATHPARAMETERFVNVPROC) load(userptr, "glGetPathParameterfvNV");
    glad_glGetPathParameterivNV = (PFNGLGETPATHPARAMETERIVNVPROC) load(userptr, "glGetPathParameterivNV");
    glad_glGetPathSpacingNV = (PFNGLGETPATHSPACINGNVPROC) load(userptr, "glGetPathSpacingNV");
    glad_glGetProgramResourcefvNV = (PFNGLGETPROGRAMRESOURCEFVNVPROC) load(userptr, "glGetProgramResourcefvNV");
    glad_glInterpolatePathsNV = (PFNGLINTERPOLATEPATHSNVPROC) load(userptr, "glInterpolatePathsNV");
    glad_glIsPathNV = (PFNGLISPATHNVPROC) load(userptr, "glIsPathNV");
    glad_glIsPointInFillPathNV = (PFNGLISPOINTINFILLPATHNVPROC) load(userptr, "glIsPointInFillPathNV");
    glad_glIsPointInStrokePathNV = (PFNGLISPOINTINSTROKEPATHNVPROC) load(userptr, "glIsPointInStrokePathNV");
    glad_glMatrixFrustumEXT = (PFNGLMATRIXFRUSTUMEXTPROC) load(userptr, "glMatrixFrustumEXT");
    glad_glMatrixLoad3x2fNV = (PFNGLMATRIXLOAD3X2FNVPROC) load(userptr, "glMatrixLoad3x2fNV");
    glad_glMatrixLoad3x3fNV = (PFNGLMATRIXLOAD3X3FNVPROC) load(userptr, "glMatrixLoad3x3fNV");
    glad_glMatrixLoadIdentityEXT = (PFNGLMATRIXLOADIDENTITYEXTPROC) load(userptr, "glMatrixLoadIdentityEXT");
    glad_glMatrixLoadTranspose3x3fNV = (PFNGLMATRIXLOADTRANSPOSE3X3FNVPROC) load(userptr, "glMatrixLoadTranspose3x3fNV");
    glad_glMatrixLoadTransposedEXT = (PFNGLMATRIXLOADTRANSPOSEDEXTPROC) load(userptr, "glMatrixLoadTransposedEXT");
    glad_glMatrixLoadTransposefEXT = (PFNGLMATRIXLOADTRANSPOSEFEXTPROC) load(userptr, "glMatrixLoadTransposefEXT");
    glad_glMatrixLoaddEXT = (PFNGLMATRIXLOADDEXTPROC) load(userptr, "glMatrixLoaddEXT");
    glad_glMatrixLoadfEXT = (PFNGLMATRIXLOADFEXTPROC) load(userptr, "glMatrixLoadfEXT");
    glad_glMatrixMult3x2fNV = (PFNGLMATRIXMULT3X2FNVPROC) load(userptr, "glMatrixMult3x2fNV");
    glad_glMatrixMult3x3fNV = (PFNGLMATRIXMULT3X3FNVPROC) load(userptr, "glMatrixMult3x3fNV");
    glad_glMatrixMultTranspose3x3fNV = (PFNGLMATRIXMULTTRANSPOSE3X3FNVPROC) load(userptr, "glMatrixMultTranspose3x3fNV");
    glad_glMatrixMultTransposedEXT = (PFNGLMATRIXMULTTRANSPOSEDEXTPROC) load(userptr, "glMatrixMultTransposedEXT");
    glad_glMatrixMultTransposefEXT = (PFNGLMATRIXMULTTRANSPOSEFEXTPROC) load(userptr, "glMatrixMultTransposefEXT");
    glad_glMatrixMultdEXT = (PFNGLMATRIXMULTDEXTPROC) load(userptr, "glMatrixMultdEXT");
    glad_glMatrixMultfEXT = (PFNGLMATRIXMULTFEXTPROC) load(userptr, "glMatrixMultfEXT");
    glad_glMatrixOrthoEXT = (PFNGLMATRIXORTHOEXTPROC) load(userptr, "glMatrixOrthoEXT");
    glad_glMatrixPopEXT = (PFNGLMATRIXPOPEXTPROC) load(userptr, "glMatrixPopEXT");
    glad_glMatrixPushEXT = (PFNGLMATRIXPUSHEXTPROC) load(userptr, "glMatrixPushEXT");
    glad_glMatrixRotatedEXT = (PFNGLMATRIXROTATEDEXTPROC) load(userptr, "glMatrixRotatedEXT");
    glad_glMatrixRotatefEXT = (PFNGLMATRIXROTATEFEXTPROC) load(userptr, "glMatrixRotatefEXT");
    glad_glMatrixScaledEXT = (PFNGLMATRIXSCALEDEXTPROC) load(userptr, "glMatrixScaledEXT");
    glad_glMatrixScalefEXT = (PFNGLMATRIXSCALEFEXTPROC) load(userptr, "glMatrixScalefEXT");
    glad_glMatrixTranslatedEXT = (PFNGLMATRIXTRANSLATEDEXTPROC) load(userptr, "glMatrixTranslatedEXT");
    glad_glMatrixTranslatefEXT = (PFNGLMATRIXTRANSLATEFEXTPROC) load(userptr, "glMatrixTranslatefEXT");
    glad_glPathCommandsNV = (PFNGLPATHCOMMANDSNVPROC) load(userptr, "glPathCommandsNV");
    glad_glPathCoordsNV = (PFNGLPATHCOORDSNVPROC) load(userptr, "glPathCoordsNV");
    glad_glPathCoverDepthFuncNV = (PFNGLPATHCOVERDEPTHFUNCNVPROC) load(userptr, "glPathCoverDepthFuncNV");
    glad_glPathDashArrayNV = (PFNGLPATHDASHARRAYNVPROC) load(userptr, "glPathDashArrayNV");
    glad_glPathGlyphIndexArrayNV = (PFNGLPATHGLYPHINDEXARRAYNVPROC) load(userptr, "glPathGlyphIndexArrayNV");
    glad_glPathGlyphIndexRangeNV = (PFNGLPATHGLYPHINDEXRANGENVPROC) load(userptr, "glPathGlyphIndexRangeNV");
    glad_glPathGlyphRangeNV = (PFNGLPATHGLYPHRANGENVPROC) load(userptr, "glPathGlyphRangeNV");
    glad_glPathGlyphsNV = (PFNGLPATHGLYPHSNVPROC) load(userptr, "glPathGlyphsNV");
    glad_glPathMemoryGlyphIndexArrayNV = (PFNGLPATHMEMORYGLYPHINDEXARRAYNVPROC) load(userptr, "glPathMemoryGlyphIndexArrayNV");
    glad_glPathParameterfNV = (PFNGLPATHPARAMETERFNVPROC) load(userptr, "glPathParameterfNV");
    glad_glPathParameterfvNV = (PFNGLPATHPARAMETERFVNVPROC) load(userptr, "glPathParameterfvNV");
    glad_glPathParameteriNV = (PFNGLPATHPARAMETERINVPROC) load(userptr, "glPathParameteriNV");
    glad_glPathParameterivNV = (PFNGLPATHPARAMETERIVNVPROC) load(userptr, "glPathParameterivNV");
    glad_glPathStencilDepthOffsetNV = (PFNGLPATHSTENCILDEPTHOFFSETNVPROC) load(userptr, "glPathStencilDepthOffsetNV");
    glad_glPathStencilFuncNV = (PFNGLPATHSTENCILFUNCNVPROC) load(userptr, "glPathStencilFuncNV");
    glad_glPathStringNV = (PFNGLPATHSTRINGNVPROC) load(userptr, "glPathStringNV");
    glad_glPathSubCommandsNV = (PFNGLPATHSUBCOMMANDSNVPROC) load(userptr, "glPathSubCommandsNV");
    glad_glPathSubCoordsNV = (PFNGLPATHSUBCOORDSNVPROC) load(userptr, "glPathSubCoordsNV");
    glad_glPointAlongPathNV = (PFNGLPOINTALONGPATHNVPROC) load(userptr, "glPointAlongPathNV");
    glad_glProgramPathFragmentInputGenNV = (PFNGLPROGRAMPATHFRAGMENTINPUTGENNVPROC) load(userptr, "glProgramPathFragmentInputGenNV");
    glad_glStencilFillPathInstancedNV = (PFNGLSTENCILFILLPATHINSTANCEDNVPROC) load(userptr, "glStencilFillPathInstancedNV");
    glad_glStencilFillPathNV = (PFNGLSTENCILFILLPATHNVPROC) load(userptr, "glStencilFillPathNV");
    glad_glStencilStrokePathInstancedNV = (PFNGLSTENCILSTROKEPATHINSTANCEDNVPROC) load(userptr, "glStencilStrokePathInstancedNV");
    glad_glStencilStrokePathNV = (PFNGLSTENCILSTROKEPATHNVPROC) load(userptr, "glStencilStrokePathNV");
    glad_glStencilThenCoverFillPathInstancedNV = (PFNGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC) load(userptr, "glStencilThenCoverFillPathInstancedNV");
    glad_glStencilThenCoverFillPathNV = (PFNGLSTENCILTHENCOVERFILLPATHNVPROC) load(userptr, "glStencilThenCoverFillPathNV");
    glad_glStencilThenCoverStrokePathInstancedNV = (PFNGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC) load(userptr, "glStencilThenCoverStrokePathInstancedNV");
    glad_glStencilThenCoverStrokePathNV = (PFNGLSTENCILTHENCOVERSTROKEPATHNVPROC) load(userptr, "glStencilThenCoverStrokePathNV");
    glad_glTransformPathNV = (PFNGLTRANSFORMPATHNVPROC) load(userptr, "glTransformPathNV");
    glad_glWeightPathsNV = (PFNGLWEIGHTPATHSNVPROC) load(userptr, "glWeightPathsNV");
}
static void glad_gl_load_GL_NV_polygon_mode( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_polygon_mode) return;
    glad_glPolygonModeNV = (PFNGLPOLYGONMODENVPROC) load(userptr, "glPolygonModeNV");
}
static void glad_gl_load_GL_NV_read_buffer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_read_buffer) return;
    glad_glReadBufferNV = (PFNGLREADBUFFERNVPROC) load(userptr, "glReadBufferNV");
}
static void glad_gl_load_GL_NV_sample_locations( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_sample_locations) return;
    glad_glFramebufferSampleLocationsfvNV = (PFNGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC) load(userptr, "glFramebufferSampleLocationsfvNV");
    glad_glNamedFramebufferSampleLocationsfvNV = (PFNGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC) load(userptr, "glNamedFramebufferSampleLocationsfvNV");
    glad_glResolveDepthValuesNV = (PFNGLRESOLVEDEPTHVALUESNVPROC) load(userptr, "glResolveDepthValuesNV");
}
static void glad_gl_load_GL_NV_scissor_exclusive( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_scissor_exclusive) return;
    glad_glScissorExclusiveArrayvNV = (PFNGLSCISSOREXCLUSIVEARRAYVNVPROC) load(userptr, "glScissorExclusiveArrayvNV");
    glad_glScissorExclusiveNV = (PFNGLSCISSOREXCLUSIVENVPROC) load(userptr, "glScissorExclusiveNV");
}
static void glad_gl_load_GL_NV_shading_rate_image( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_shading_rate_image) return;
    glad_glBindShadingRateImageNV = (PFNGLBINDSHADINGRATEIMAGENVPROC) load(userptr, "glBindShadingRateImageNV");
    glad_glGetShadingRateImagePaletteNV = (PFNGLGETSHADINGRATEIMAGEPALETTENVPROC) load(userptr, "glGetShadingRateImagePaletteNV");
    glad_glGetShadingRateSampleLocationivNV = (PFNGLGETSHADINGRATESAMPLELOCATIONIVNVPROC) load(userptr, "glGetShadingRateSampleLocationivNV");
    glad_glShadingRateImageBarrierNV = (PFNGLSHADINGRATEIMAGEBARRIERNVPROC) load(userptr, "glShadingRateImageBarrierNV");
    glad_glShadingRateImagePaletteNV = (PFNGLSHADINGRATEIMAGEPALETTENVPROC) load(userptr, "glShadingRateImagePaletteNV");
    glad_glShadingRateSampleOrderCustomNV = (PFNGLSHADINGRATESAMPLEORDERCUSTOMNVPROC) load(userptr, "glShadingRateSampleOrderCustomNV");
    glad_glShadingRateSampleOrderNV = (PFNGLSHADINGRATESAMPLEORDERNVPROC) load(userptr, "glShadingRateSampleOrderNV");
}
static void glad_gl_load_GL_NV_texture_barrier( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_texture_barrier) return;
    glad_glTextureBarrierNV = (PFNGLTEXTUREBARRIERNVPROC) load(userptr, "glTextureBarrierNV");
}
static void glad_gl_load_GL_NV_timeline_semaphore( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_timeline_semaphore) return;
    glad_glCreateSemaphoresNV = (PFNGLCREATESEMAPHORESNVPROC) load(userptr, "glCreateSemaphoresNV");
    glad_glGetSemaphoreParameterivNV = (PFNGLGETSEMAPHOREPARAMETERIVNVPROC) load(userptr, "glGetSemaphoreParameterivNV");
    glad_glSemaphoreParameterivNV = (PFNGLSEMAPHOREPARAMETERIVNVPROC) load(userptr, "glSemaphoreParameterivNV");
}
static void glad_gl_load_GL_NV_viewport_array( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_viewport_array) return;
    glad_glDepthRangeArrayfvNV = (PFNGLDEPTHRANGEARRAYFVNVPROC) load(userptr, "glDepthRangeArrayfvNV");
    glad_glDepthRangeIndexedfNV = (PFNGLDEPTHRANGEINDEXEDFNVPROC) load(userptr, "glDepthRangeIndexedfNV");
    glad_glDisableiNV = (PFNGLDISABLEINVPROC) load(userptr, "glDisableiNV");
    glad_glEnableiNV = (PFNGLENABLEINVPROC) load(userptr, "glEnableiNV");
    glad_glGetFloati_vNV = (PFNGLGETFLOATI_VNVPROC) load(userptr, "glGetFloati_vNV");
    glad_glIsEnablediNV = (PFNGLISENABLEDINVPROC) load(userptr, "glIsEnablediNV");
    glad_glScissorArrayvNV = (PFNGLSCISSORARRAYVNVPROC) load(userptr, "glScissorArrayvNV");
    glad_glScissorIndexedNV = (PFNGLSCISSORINDEXEDNVPROC) load(userptr, "glScissorIndexedNV");
    glad_glScissorIndexedvNV = (PFNGLSCISSORINDEXEDVNVPROC) load(userptr, "glScissorIndexedvNV");
    glad_glViewportArrayvNV = (PFNGLVIEWPORTARRAYVNVPROC) load(userptr, "glViewportArrayvNV");
    glad_glViewportIndexedfNV = (PFNGLVIEWPORTINDEXEDFNVPROC) load(userptr, "glViewportIndexedfNV");
    glad_glViewportIndexedfvNV = (PFNGLVIEWPORTINDEXEDFVNVPROC) load(userptr, "glViewportIndexedfvNV");
}
static void glad_gl_load_GL_NV_viewport_swizzle( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_NV_viewport_swizzle) return;
    glad_glViewportSwizzleNV = (PFNGLVIEWPORTSWIZZLENVPROC) load(userptr, "glViewportSwizzleNV");
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
static void glad_gl_load_GL_OES_copy_image( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_copy_image) return;
    glad_glCopyImageSubDataOES = (PFNGLCOPYIMAGESUBDATAOESPROC) load(userptr, "glCopyImageSubDataOES");
}
static void glad_gl_load_GL_OES_draw_buffers_indexed( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_draw_buffers_indexed) return;
    glad_glBlendEquationSeparateiOES = (PFNGLBLENDEQUATIONSEPARATEIOESPROC) load(userptr, "glBlendEquationSeparateiOES");
    glad_glBlendEquationiOES = (PFNGLBLENDEQUATIONIOESPROC) load(userptr, "glBlendEquationiOES");
    glad_glBlendFuncSeparateiOES = (PFNGLBLENDFUNCSEPARATEIOESPROC) load(userptr, "glBlendFuncSeparateiOES");
    glad_glBlendFunciOES = (PFNGLBLENDFUNCIOESPROC) load(userptr, "glBlendFunciOES");
    glad_glColorMaskiOES = (PFNGLCOLORMASKIOESPROC) load(userptr, "glColorMaskiOES");
    glad_glDisableiOES = (PFNGLDISABLEIOESPROC) load(userptr, "glDisableiOES");
    glad_glEnableiOES = (PFNGLENABLEIOESPROC) load(userptr, "glEnableiOES");
    glad_glIsEnablediOES = (PFNGLISENABLEDIOESPROC) load(userptr, "glIsEnablediOES");
}
static void glad_gl_load_GL_OES_draw_elements_base_vertex( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_draw_elements_base_vertex) return;
    glad_glDrawElementsBaseVertexOES = (PFNGLDRAWELEMENTSBASEVERTEXOESPROC) load(userptr, "glDrawElementsBaseVertexOES");
    glad_glDrawElementsInstancedBaseVertexOES = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC) load(userptr, "glDrawElementsInstancedBaseVertexOES");
    glad_glDrawRangeElementsBaseVertexOES = (PFNGLDRAWRANGEELEMENTSBASEVERTEXOESPROC) load(userptr, "glDrawRangeElementsBaseVertexOES");
    glad_glMultiDrawElementsBaseVertexEXT = (PFNGLMULTIDRAWELEMENTSBASEVERTEXEXTPROC) load(userptr, "glMultiDrawElementsBaseVertexEXT");
}
static void glad_gl_load_GL_OES_geometry_shader( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_geometry_shader) return;
    glad_glFramebufferTextureOES = (PFNGLFRAMEBUFFERTEXTUREOESPROC) load(userptr, "glFramebufferTextureOES");
}
static void glad_gl_load_GL_OES_get_program_binary( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_get_program_binary) return;
    glad_glGetProgramBinaryOES = (PFNGLGETPROGRAMBINARYOESPROC) load(userptr, "glGetProgramBinaryOES");
    glad_glProgramBinaryOES = (PFNGLPROGRAMBINARYOESPROC) load(userptr, "glProgramBinaryOES");
}
static void glad_gl_load_GL_OES_mapbuffer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_mapbuffer) return;
    glad_glGetBufferPointervOES = (PFNGLGETBUFFERPOINTERVOESPROC) load(userptr, "glGetBufferPointervOES");
    glad_glMapBufferOES = (PFNGLMAPBUFFEROESPROC) load(userptr, "glMapBufferOES");
    glad_glUnmapBufferOES = (PFNGLUNMAPBUFFEROESPROC) load(userptr, "glUnmapBufferOES");
}
static void glad_gl_load_GL_OES_primitive_bounding_box( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_primitive_bounding_box) return;
    glad_glPrimitiveBoundingBoxOES = (PFNGLPRIMITIVEBOUNDINGBOXOESPROC) load(userptr, "glPrimitiveBoundingBoxOES");
}
static void glad_gl_load_GL_OES_sample_shading( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_sample_shading) return;
    glad_glMinSampleShadingOES = (PFNGLMINSAMPLESHADINGOESPROC) load(userptr, "glMinSampleShadingOES");
}
static void glad_gl_load_GL_OES_tessellation_shader( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_tessellation_shader) return;
    glad_glPatchParameteriOES = (PFNGLPATCHPARAMETERIOESPROC) load(userptr, "glPatchParameteriOES");
}
static void glad_gl_load_GL_OES_texture_3D( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_texture_3D) return;
    glad_glCompressedTexImage3DOES = (PFNGLCOMPRESSEDTEXIMAGE3DOESPROC) load(userptr, "glCompressedTexImage3DOES");
    glad_glCompressedTexSubImage3DOES = (PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC) load(userptr, "glCompressedTexSubImage3DOES");
    glad_glCopyTexSubImage3DOES = (PFNGLCOPYTEXSUBIMAGE3DOESPROC) load(userptr, "glCopyTexSubImage3DOES");
    glad_glFramebufferTexture3DOES = (PFNGLFRAMEBUFFERTEXTURE3DOESPROC) load(userptr, "glFramebufferTexture3DOES");
    glad_glTexImage3DOES = (PFNGLTEXIMAGE3DOESPROC) load(userptr, "glTexImage3DOES");
    glad_glTexSubImage3DOES = (PFNGLTEXSUBIMAGE3DOESPROC) load(userptr, "glTexSubImage3DOES");
}
static void glad_gl_load_GL_OES_texture_border_clamp( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_texture_border_clamp) return;
    glad_glGetSamplerParameterIivOES = (PFNGLGETSAMPLERPARAMETERIIVOESPROC) load(userptr, "glGetSamplerParameterIivOES");
    glad_glGetSamplerParameterIuivOES = (PFNGLGETSAMPLERPARAMETERIUIVOESPROC) load(userptr, "glGetSamplerParameterIuivOES");
    glad_glGetTexParameterIivOES = (PFNGLGETTEXPARAMETERIIVOESPROC) load(userptr, "glGetTexParameterIivOES");
    glad_glGetTexParameterIuivOES = (PFNGLGETTEXPARAMETERIUIVOESPROC) load(userptr, "glGetTexParameterIuivOES");
    glad_glSamplerParameterIivOES = (PFNGLSAMPLERPARAMETERIIVOESPROC) load(userptr, "glSamplerParameterIivOES");
    glad_glSamplerParameterIuivOES = (PFNGLSAMPLERPARAMETERIUIVOESPROC) load(userptr, "glSamplerParameterIuivOES");
    glad_glTexParameterIivOES = (PFNGLTEXPARAMETERIIVOESPROC) load(userptr, "glTexParameterIivOES");
    glad_glTexParameterIuivOES = (PFNGLTEXPARAMETERIUIVOESPROC) load(userptr, "glTexParameterIuivOES");
}
static void glad_gl_load_GL_OES_texture_buffer( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_texture_buffer) return;
    glad_glTexBufferOES = (PFNGLTEXBUFFEROESPROC) load(userptr, "glTexBufferOES");
    glad_glTexBufferRangeOES = (PFNGLTEXBUFFERRANGEOESPROC) load(userptr, "glTexBufferRangeOES");
}
static void glad_gl_load_GL_OES_texture_storage_multisample_2d_array( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_texture_storage_multisample_2d_array) return;
    glad_glTexStorage3DMultisampleOES = (PFNGLTEXSTORAGE3DMULTISAMPLEOESPROC) load(userptr, "glTexStorage3DMultisampleOES");
}
static void glad_gl_load_GL_OES_texture_view( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_texture_view) return;
    glad_glTextureViewOES = (PFNGLTEXTUREVIEWOESPROC) load(userptr, "glTextureViewOES");
}
static void glad_gl_load_GL_OES_vertex_array_object( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_vertex_array_object) return;
    glad_glBindVertexArrayOES = (PFNGLBINDVERTEXARRAYOESPROC) load(userptr, "glBindVertexArrayOES");
    glad_glDeleteVertexArraysOES = (PFNGLDELETEVERTEXARRAYSOESPROC) load(userptr, "glDeleteVertexArraysOES");
    glad_glGenVertexArraysOES = (PFNGLGENVERTEXARRAYSOESPROC) load(userptr, "glGenVertexArraysOES");
    glad_glIsVertexArrayOES = (PFNGLISVERTEXARRAYOESPROC) load(userptr, "glIsVertexArrayOES");
}
static void glad_gl_load_GL_OES_viewport_array( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OES_viewport_array) return;
    glad_glDepthRangeArrayfvOES = (PFNGLDEPTHRANGEARRAYFVOESPROC) load(userptr, "glDepthRangeArrayfvOES");
    glad_glDepthRangeIndexedfOES = (PFNGLDEPTHRANGEINDEXEDFOESPROC) load(userptr, "glDepthRangeIndexedfOES");
    glad_glDisableiOES = (PFNGLDISABLEIOESPROC) load(userptr, "glDisableiOES");
    glad_glEnableiOES = (PFNGLENABLEIOESPROC) load(userptr, "glEnableiOES");
    glad_glGetFloati_vOES = (PFNGLGETFLOATI_VOESPROC) load(userptr, "glGetFloati_vOES");
    glad_glIsEnablediOES = (PFNGLISENABLEDIOESPROC) load(userptr, "glIsEnablediOES");
    glad_glScissorArrayvOES = (PFNGLSCISSORARRAYVOESPROC) load(userptr, "glScissorArrayvOES");
    glad_glScissorIndexedOES = (PFNGLSCISSORINDEXEDOESPROC) load(userptr, "glScissorIndexedOES");
    glad_glScissorIndexedvOES = (PFNGLSCISSORINDEXEDVOESPROC) load(userptr, "glScissorIndexedvOES");
    glad_glViewportArrayvOES = (PFNGLVIEWPORTARRAYVOESPROC) load(userptr, "glViewportArrayvOES");
    glad_glViewportIndexedfOES = (PFNGLVIEWPORTINDEXEDFOESPROC) load(userptr, "glViewportIndexedfOES");
    glad_glViewportIndexedfvOES = (PFNGLVIEWPORTINDEXEDFVOESPROC) load(userptr, "glViewportIndexedfvOES");
}
static void glad_gl_load_GL_OVR_multiview( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OVR_multiview) return;
    glad_glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) load(userptr, "glFramebufferTextureMultiviewOVR");
    glad_glNamedFramebufferTextureMultiviewOVR = (PFNGLNAMEDFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) load(userptr, "glNamedFramebufferTextureMultiviewOVR");
}
static void glad_gl_load_GL_OVR_multiview_multisampled_render_to_texture( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_OVR_multiview_multisampled_render_to_texture) return;
    glad_glFramebufferTextureMultisampleMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC) load(userptr, "glFramebufferTextureMultisampleMultiviewOVR");
}
static void glad_gl_load_GL_QCOM_alpha_test( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_alpha_test) return;
    glad_glAlphaFuncQCOM = (PFNGLALPHAFUNCQCOMPROC) load(userptr, "glAlphaFuncQCOM");
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
static void glad_gl_load_GL_QCOM_frame_extrapolation( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_frame_extrapolation) return;
    glad_glExtrapolateTex2DQCOM = (PFNGLEXTRAPOLATETEX2DQCOMPROC) load(userptr, "glExtrapolateTex2DQCOM");
}
static void glad_gl_load_GL_QCOM_framebuffer_foveated( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_framebuffer_foveated) return;
    glad_glFramebufferFoveationConfigQCOM = (PFNGLFRAMEBUFFERFOVEATIONCONFIGQCOMPROC) load(userptr, "glFramebufferFoveationConfigQCOM");
    glad_glFramebufferFoveationParametersQCOM = (PFNGLFRAMEBUFFERFOVEATIONPARAMETERSQCOMPROC) load(userptr, "glFramebufferFoveationParametersQCOM");
}
static void glad_gl_load_GL_QCOM_motion_estimation( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_motion_estimation) return;
    glad_glTexEstimateMotionQCOM = (PFNGLTEXESTIMATEMOTIONQCOMPROC) load(userptr, "glTexEstimateMotionQCOM");
    glad_glTexEstimateMotionRegionsQCOM = (PFNGLTEXESTIMATEMOTIONREGIONSQCOMPROC) load(userptr, "glTexEstimateMotionRegionsQCOM");
}
static void glad_gl_load_GL_QCOM_shader_framebuffer_fetch_noncoherent( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_shader_framebuffer_fetch_noncoherent) return;
    glad_glFramebufferFetchBarrierQCOM = (PFNGLFRAMEBUFFERFETCHBARRIERQCOMPROC) load(userptr, "glFramebufferFetchBarrierQCOM");
}
static void glad_gl_load_GL_QCOM_shading_rate( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_shading_rate) return;
    glad_glShadingRateQCOM = (PFNGLSHADINGRATEQCOMPROC) load(userptr, "glShadingRateQCOM");
}
static void glad_gl_load_GL_QCOM_texture_foveated( GLADuserptrloadfunc load, void* userptr) {
    if(!GLAD_GL_QCOM_texture_foveated) return;
    glad_glTextureFoveationParametersQCOM = (PFNGLTEXTUREFOVEATIONPARAMETERSQCOMPROC) load(userptr, "glTextureFoveationParametersQCOM");
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

static int glad_gl_find_extensions_gles2(void) {
    const char *exts = NULL;
    char **exts_i = NULL;
    if (!glad_gl_get_extensions(&exts, &exts_i)) return 0;

    GLAD_GL_AMD_compressed_3DC_texture = glad_gl_has_extension(exts, exts_i, "GL_AMD_compressed_3DC_texture");
    GLAD_GL_AMD_compressed_ATC_texture = glad_gl_has_extension(exts, exts_i, "GL_AMD_compressed_ATC_texture");
    GLAD_GL_AMD_framebuffer_multisample_advanced = glad_gl_has_extension(exts, exts_i, "GL_AMD_framebuffer_multisample_advanced");
    GLAD_GL_AMD_performance_monitor = glad_gl_has_extension(exts, exts_i, "GL_AMD_performance_monitor");
    GLAD_GL_AMD_program_binary_Z400 = glad_gl_has_extension(exts, exts_i, "GL_AMD_program_binary_Z400");
    GLAD_GL_ANDROID_extension_pack_es31a = glad_gl_has_extension(exts, exts_i, "GL_ANDROID_extension_pack_es31a");
    GLAD_GL_ANGLE_depth_texture = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_depth_texture");
    GLAD_GL_ANGLE_framebuffer_blit = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_framebuffer_blit");
    GLAD_GL_ANGLE_framebuffer_multisample = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_framebuffer_multisample");
    GLAD_GL_ANGLE_instanced_arrays = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_instanced_arrays");
    GLAD_GL_ANGLE_pack_reverse_row_order = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_pack_reverse_row_order");
    GLAD_GL_ANGLE_program_binary = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_program_binary");
    GLAD_GL_ANGLE_texture_compression_dxt3 = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_texture_compression_dxt3");
    GLAD_GL_ANGLE_texture_compression_dxt5 = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_texture_compression_dxt5");
    GLAD_GL_ANGLE_texture_usage = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_texture_usage");
    GLAD_GL_ANGLE_translated_shader_source = glad_gl_has_extension(exts, exts_i, "GL_ANGLE_translated_shader_source");
    GLAD_GL_APPLE_clip_distance = glad_gl_has_extension(exts, exts_i, "GL_APPLE_clip_distance");
    GLAD_GL_APPLE_color_buffer_packed_float = glad_gl_has_extension(exts, exts_i, "GL_APPLE_color_buffer_packed_float");
    GLAD_GL_APPLE_copy_texture_levels = glad_gl_has_extension(exts, exts_i, "GL_APPLE_copy_texture_levels");
    GLAD_GL_APPLE_framebuffer_multisample = glad_gl_has_extension(exts, exts_i, "GL_APPLE_framebuffer_multisample");
    GLAD_GL_APPLE_rgb_422 = glad_gl_has_extension(exts, exts_i, "GL_APPLE_rgb_422");
    GLAD_GL_APPLE_sync = glad_gl_has_extension(exts, exts_i, "GL_APPLE_sync");
    GLAD_GL_APPLE_texture_format_BGRA8888 = glad_gl_has_extension(exts, exts_i, "GL_APPLE_texture_format_BGRA8888");
    GLAD_GL_APPLE_texture_max_level = glad_gl_has_extension(exts, exts_i, "GL_APPLE_texture_max_level");
    GLAD_GL_APPLE_texture_packed_float = glad_gl_has_extension(exts, exts_i, "GL_APPLE_texture_packed_float");
    GLAD_GL_ARM_mali_program_binary = glad_gl_has_extension(exts, exts_i, "GL_ARM_mali_program_binary");
    GLAD_GL_ARM_mali_shader_binary = glad_gl_has_extension(exts, exts_i, "GL_ARM_mali_shader_binary");
    GLAD_GL_ARM_rgba8 = glad_gl_has_extension(exts, exts_i, "GL_ARM_rgba8");
    GLAD_GL_ARM_shader_core_properties = glad_gl_has_extension(exts, exts_i, "GL_ARM_shader_core_properties");
    GLAD_GL_ARM_shader_framebuffer_fetch = glad_gl_has_extension(exts, exts_i, "GL_ARM_shader_framebuffer_fetch");
    GLAD_GL_ARM_shader_framebuffer_fetch_depth_stencil = glad_gl_has_extension(exts, exts_i, "GL_ARM_shader_framebuffer_fetch_depth_stencil");
    GLAD_GL_ARM_texture_unnormalized_coordinates = glad_gl_has_extension(exts, exts_i, "GL_ARM_texture_unnormalized_coordinates");
    GLAD_GL_DMP_program_binary = glad_gl_has_extension(exts, exts_i, "GL_DMP_program_binary");
    GLAD_GL_DMP_shader_binary = glad_gl_has_extension(exts, exts_i, "GL_DMP_shader_binary");
    GLAD_GL_EXT_EGL_image_array = glad_gl_has_extension(exts, exts_i, "GL_EXT_EGL_image_array");
    GLAD_GL_EXT_EGL_image_storage = glad_gl_has_extension(exts, exts_i, "GL_EXT_EGL_image_storage");
    GLAD_GL_EXT_EGL_image_storage_compression = glad_gl_has_extension(exts, exts_i, "GL_EXT_EGL_image_storage_compression");
    GLAD_GL_EXT_YUV_target = glad_gl_has_extension(exts, exts_i, "GL_EXT_YUV_target");
    GLAD_GL_EXT_base_instance = glad_gl_has_extension(exts, exts_i, "GL_EXT_base_instance");
    GLAD_GL_EXT_blend_func_extended = glad_gl_has_extension(exts, exts_i, "GL_EXT_blend_func_extended");
    GLAD_GL_EXT_blend_minmax = glad_gl_has_extension(exts, exts_i, "GL_EXT_blend_minmax");
    GLAD_GL_EXT_buffer_storage = glad_gl_has_extension(exts, exts_i, "GL_EXT_buffer_storage");
    GLAD_GL_EXT_clear_texture = glad_gl_has_extension(exts, exts_i, "GL_EXT_clear_texture");
    GLAD_GL_EXT_clip_control = glad_gl_has_extension(exts, exts_i, "GL_EXT_clip_control");
    GLAD_GL_EXT_clip_cull_distance = glad_gl_has_extension(exts, exts_i, "GL_EXT_clip_cull_distance");
    GLAD_GL_EXT_color_buffer_float = glad_gl_has_extension(exts, exts_i, "GL_EXT_color_buffer_float");
    GLAD_GL_EXT_color_buffer_half_float = glad_gl_has_extension(exts, exts_i, "GL_EXT_color_buffer_half_float");
    GLAD_GL_EXT_conservative_depth = glad_gl_has_extension(exts, exts_i, "GL_EXT_conservative_depth");
    GLAD_GL_EXT_copy_image = glad_gl_has_extension(exts, exts_i, "GL_EXT_copy_image");
    GLAD_GL_EXT_debug_label = glad_gl_has_extension(exts, exts_i, "GL_EXT_debug_label");
    GLAD_GL_EXT_debug_marker = glad_gl_has_extension(exts, exts_i, "GL_EXT_debug_marker");
    GLAD_GL_EXT_depth_clamp = glad_gl_has_extension(exts, exts_i, "GL_EXT_depth_clamp");
    GLAD_GL_EXT_discard_framebuffer = glad_gl_has_extension(exts, exts_i, "GL_EXT_discard_framebuffer");
    GLAD_GL_EXT_disjoint_timer_query = glad_gl_has_extension(exts, exts_i, "GL_EXT_disjoint_timer_query");
    GLAD_GL_EXT_draw_buffers = glad_gl_has_extension(exts, exts_i, "GL_EXT_draw_buffers");
    GLAD_GL_EXT_draw_buffers_indexed = glad_gl_has_extension(exts, exts_i, "GL_EXT_draw_buffers_indexed");
    GLAD_GL_EXT_draw_elements_base_vertex = glad_gl_has_extension(exts, exts_i, "GL_EXT_draw_elements_base_vertex");
    GLAD_GL_EXT_draw_instanced = glad_gl_has_extension(exts, exts_i, "GL_EXT_draw_instanced");
    GLAD_GL_EXT_draw_transform_feedback = glad_gl_has_extension(exts, exts_i, "GL_EXT_draw_transform_feedback");
    GLAD_GL_EXT_external_buffer = glad_gl_has_extension(exts, exts_i, "GL_EXT_external_buffer");
    GLAD_GL_EXT_float_blend = glad_gl_has_extension(exts, exts_i, "GL_EXT_float_blend");
    GLAD_GL_EXT_fragment_shading_rate = glad_gl_has_extension(exts, exts_i, "GL_EXT_fragment_shading_rate");
    GLAD_GL_EXT_framebuffer_blit_layers = glad_gl_has_extension(exts, exts_i, "GL_EXT_framebuffer_blit_layers");
    GLAD_GL_EXT_geometry_point_size = glad_gl_has_extension(exts, exts_i, "GL_EXT_geometry_point_size");
    GLAD_GL_EXT_geometry_shader = glad_gl_has_extension(exts, exts_i, "GL_EXT_geometry_shader");
    GLAD_GL_EXT_gpu_shader5 = glad_gl_has_extension(exts, exts_i, "GL_EXT_gpu_shader5");
    GLAD_GL_EXT_instanced_arrays = glad_gl_has_extension(exts, exts_i, "GL_EXT_instanced_arrays");
    GLAD_GL_EXT_map_buffer_range = glad_gl_has_extension(exts, exts_i, "GL_EXT_map_buffer_range");
    GLAD_GL_EXT_memory_object = glad_gl_has_extension(exts, exts_i, "GL_EXT_memory_object");
    GLAD_GL_EXT_memory_object_fd = glad_gl_has_extension(exts, exts_i, "GL_EXT_memory_object_fd");
    GLAD_GL_EXT_memory_object_win32 = glad_gl_has_extension(exts, exts_i, "GL_EXT_memory_object_win32");
    GLAD_GL_EXT_mesh_shader = glad_gl_has_extension(exts, exts_i, "GL_EXT_mesh_shader");
    GLAD_GL_EXT_multi_draw_arrays = glad_gl_has_extension(exts, exts_i, "GL_EXT_multi_draw_arrays");
    GLAD_GL_EXT_multi_draw_indirect = glad_gl_has_extension(exts, exts_i, "GL_EXT_multi_draw_indirect");
    GLAD_GL_EXT_multisampled_compatibility = glad_gl_has_extension(exts, exts_i, "GL_EXT_multisampled_compatibility");
    GLAD_GL_EXT_multisampled_render_to_texture = glad_gl_has_extension(exts, exts_i, "GL_EXT_multisampled_render_to_texture");
    GLAD_GL_EXT_multisampled_render_to_texture2 = glad_gl_has_extension(exts, exts_i, "GL_EXT_multisampled_render_to_texture2");
    GLAD_GL_EXT_multiview_draw_buffers = glad_gl_has_extension(exts, exts_i, "GL_EXT_multiview_draw_buffers");
    GLAD_GL_EXT_multiview_tessellation_geometry_shader = glad_gl_has_extension(exts, exts_i, "GL_EXT_multiview_tessellation_geometry_shader");
    GLAD_GL_EXT_multiview_texture_multisample = glad_gl_has_extension(exts, exts_i, "GL_EXT_multiview_texture_multisample");
    GLAD_GL_EXT_multiview_timer_query = glad_gl_has_extension(exts, exts_i, "GL_EXT_multiview_timer_query");
    GLAD_GL_EXT_occlusion_query_boolean = glad_gl_has_extension(exts, exts_i, "GL_EXT_occlusion_query_boolean");
    GLAD_GL_EXT_polygon_offset_clamp = glad_gl_has_extension(exts, exts_i, "GL_EXT_polygon_offset_clamp");
    GLAD_GL_EXT_post_depth_coverage = glad_gl_has_extension(exts, exts_i, "GL_EXT_post_depth_coverage");
    GLAD_GL_EXT_primitive_bounding_box = glad_gl_has_extension(exts, exts_i, "GL_EXT_primitive_bounding_box");
    GLAD_GL_EXT_protected_textures = glad_gl_has_extension(exts, exts_i, "GL_EXT_protected_textures");
    GLAD_GL_EXT_pvrtc_sRGB = glad_gl_has_extension(exts, exts_i, "GL_EXT_pvrtc_sRGB");
    GLAD_GL_EXT_raster_multisample = glad_gl_has_extension(exts, exts_i, "GL_EXT_raster_multisample");
    GLAD_GL_EXT_read_format_bgra = glad_gl_has_extension(exts, exts_i, "GL_EXT_read_format_bgra");
    GLAD_GL_EXT_render_snorm = glad_gl_has_extension(exts, exts_i, "GL_EXT_render_snorm");
    GLAD_GL_EXT_robustness = glad_gl_has_extension(exts, exts_i, "GL_EXT_robustness");
    GLAD_GL_EXT_sRGB = glad_gl_has_extension(exts, exts_i, "GL_EXT_sRGB");
    GLAD_GL_EXT_sRGB_write_control = glad_gl_has_extension(exts, exts_i, "GL_EXT_sRGB_write_control");
    GLAD_GL_EXT_semaphore = glad_gl_has_extension(exts, exts_i, "GL_EXT_semaphore");
    GLAD_GL_EXT_semaphore_fd = glad_gl_has_extension(exts, exts_i, "GL_EXT_semaphore_fd");
    GLAD_GL_EXT_semaphore_win32 = glad_gl_has_extension(exts, exts_i, "GL_EXT_semaphore_win32");
    GLAD_GL_EXT_separate_depth_stencil = glad_gl_has_extension(exts, exts_i, "GL_EXT_separate_depth_stencil");
    GLAD_GL_EXT_separate_shader_objects = glad_gl_has_extension(exts, exts_i, "GL_EXT_separate_shader_objects");
    GLAD_GL_EXT_shader_framebuffer_fetch = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_framebuffer_fetch");
    GLAD_GL_EXT_shader_framebuffer_fetch_non_coherent = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_framebuffer_fetch_non_coherent");
    GLAD_GL_EXT_shader_group_vote = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_group_vote");
    GLAD_GL_EXT_shader_implicit_conversions = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_implicit_conversions");
    GLAD_GL_EXT_shader_integer_mix = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_integer_mix");
    GLAD_GL_EXT_shader_io_blocks = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_io_blocks");
    GLAD_GL_EXT_shader_non_constant_global_initializers = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_non_constant_global_initializers");
    GLAD_GL_EXT_shader_pixel_local_storage = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_pixel_local_storage");
    GLAD_GL_EXT_shader_pixel_local_storage2 = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_pixel_local_storage2");
    GLAD_GL_EXT_shader_samples_identical = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_samples_identical");
    GLAD_GL_EXT_shader_texture_lod = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_texture_lod");
    GLAD_GL_EXT_shader_texture_samples = glad_gl_has_extension(exts, exts_i, "GL_EXT_shader_texture_samples");
    GLAD_GL_EXT_shadow_samplers = glad_gl_has_extension(exts, exts_i, "GL_EXT_shadow_samplers");
    GLAD_GL_EXT_sparse_texture = glad_gl_has_extension(exts, exts_i, "GL_EXT_sparse_texture");
    GLAD_GL_EXT_sparse_texture2 = glad_gl_has_extension(exts, exts_i, "GL_EXT_sparse_texture2");
    GLAD_GL_EXT_tessellation_point_size = glad_gl_has_extension(exts, exts_i, "GL_EXT_tessellation_point_size");
    GLAD_GL_EXT_tessellation_shader = glad_gl_has_extension(exts, exts_i, "GL_EXT_tessellation_shader");
    GLAD_GL_EXT_texture_border_clamp = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_border_clamp");
    GLAD_GL_EXT_texture_buffer = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_buffer");
    GLAD_GL_EXT_texture_compression_astc_decode_mode = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_compression_astc_decode_mode");
    GLAD_GL_EXT_texture_compression_bptc = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_compression_bptc");
    GLAD_GL_EXT_texture_compression_dxt1 = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_compression_dxt1");
    GLAD_GL_EXT_texture_compression_rgtc = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_compression_rgtc");
    GLAD_GL_EXT_texture_compression_s3tc = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_compression_s3tc");
    GLAD_GL_EXT_texture_compression_s3tc_srgb = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_compression_s3tc_srgb");
    GLAD_GL_EXT_texture_cube_map_array = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_cube_map_array");
    GLAD_GL_EXT_texture_filter_anisotropic = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_filter_anisotropic");
    GLAD_GL_EXT_texture_filter_minmax = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_filter_minmax");
    GLAD_GL_EXT_texture_format_BGRA8888 = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_format_BGRA8888");
    GLAD_GL_EXT_texture_format_sRGB_override = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_format_sRGB_override");
    GLAD_GL_EXT_texture_mirror_clamp_to_edge = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_mirror_clamp_to_edge");
    GLAD_GL_EXT_texture_norm16 = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_norm16");
    GLAD_GL_EXT_texture_query_lod = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_query_lod");
    GLAD_GL_EXT_texture_rg = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_rg");
    GLAD_GL_EXT_texture_sRGB_R8 = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_sRGB_R8");
    GLAD_GL_EXT_texture_sRGB_RG8 = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_sRGB_RG8");
    GLAD_GL_EXT_texture_sRGB_decode = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_sRGB_decode");
    GLAD_GL_EXT_texture_shadow_lod = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_shadow_lod");
    GLAD_GL_EXT_texture_storage = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_storage");
    GLAD_GL_EXT_texture_storage_compression = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_storage_compression");
    GLAD_GL_EXT_texture_type_2_10_10_10_REV = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_type_2_10_10_10_REV");
    GLAD_GL_EXT_texture_view = glad_gl_has_extension(exts, exts_i, "GL_EXT_texture_view");
    GLAD_GL_EXT_unpack_subimage = glad_gl_has_extension(exts, exts_i, "GL_EXT_unpack_subimage");
    GLAD_GL_EXT_win32_keyed_mutex = glad_gl_has_extension(exts, exts_i, "GL_EXT_win32_keyed_mutex");
    GLAD_GL_EXT_window_rectangles = glad_gl_has_extension(exts, exts_i, "GL_EXT_window_rectangles");
    GLAD_GL_FJ_shader_binary_GCCSO = glad_gl_has_extension(exts, exts_i, "GL_FJ_shader_binary_GCCSO");
    GLAD_GL_HUAWEI_program_binary = glad_gl_has_extension(exts, exts_i, "GL_HUAWEI_program_binary");
    GLAD_GL_HUAWEI_shader_binary = glad_gl_has_extension(exts, exts_i, "GL_HUAWEI_shader_binary");
    GLAD_GL_IMG_bindless_texture = glad_gl_has_extension(exts, exts_i, "GL_IMG_bindless_texture");
    GLAD_GL_IMG_framebuffer_downsample = glad_gl_has_extension(exts, exts_i, "GL_IMG_framebuffer_downsample");
    GLAD_GL_IMG_multisampled_render_to_texture = glad_gl_has_extension(exts, exts_i, "GL_IMG_multisampled_render_to_texture");
    GLAD_GL_IMG_program_binary = glad_gl_has_extension(exts, exts_i, "GL_IMG_program_binary");
    GLAD_GL_IMG_read_format = glad_gl_has_extension(exts, exts_i, "GL_IMG_read_format");
    GLAD_GL_IMG_shader_binary = glad_gl_has_extension(exts, exts_i, "GL_IMG_shader_binary");
    GLAD_GL_IMG_texture_compression_pvrtc = glad_gl_has_extension(exts, exts_i, "GL_IMG_texture_compression_pvrtc");
    GLAD_GL_IMG_texture_compression_pvrtc2 = glad_gl_has_extension(exts, exts_i, "GL_IMG_texture_compression_pvrtc2");
    GLAD_GL_IMG_texture_filter_cubic = glad_gl_has_extension(exts, exts_i, "GL_IMG_texture_filter_cubic");
    GLAD_GL_INTEL_blackhole_render = glad_gl_has_extension(exts, exts_i, "GL_INTEL_blackhole_render");
    GLAD_GL_INTEL_conservative_rasterization = glad_gl_has_extension(exts, exts_i, "GL_INTEL_conservative_rasterization");
    GLAD_GL_INTEL_framebuffer_CMAA = glad_gl_has_extension(exts, exts_i, "GL_INTEL_framebuffer_CMAA");
    GLAD_GL_INTEL_performance_query = glad_gl_has_extension(exts, exts_i, "GL_INTEL_performance_query");
    GLAD_GL_KHR_blend_equation_advanced = glad_gl_has_extension(exts, exts_i, "GL_KHR_blend_equation_advanced");
    GLAD_GL_KHR_blend_equation_advanced_coherent = glad_gl_has_extension(exts, exts_i, "GL_KHR_blend_equation_advanced_coherent");
    GLAD_GL_KHR_context_flush_control = glad_gl_has_extension(exts, exts_i, "GL_KHR_context_flush_control");
    GLAD_GL_KHR_debug = glad_gl_has_extension(exts, exts_i, "GL_KHR_debug");
    GLAD_GL_KHR_no_error = glad_gl_has_extension(exts, exts_i, "GL_KHR_no_error");
    GLAD_GL_KHR_parallel_shader_compile = glad_gl_has_extension(exts, exts_i, "GL_KHR_parallel_shader_compile");
    GLAD_GL_KHR_robust_buffer_access_behavior = glad_gl_has_extension(exts, exts_i, "GL_KHR_robust_buffer_access_behavior");
    GLAD_GL_KHR_robustness = glad_gl_has_extension(exts, exts_i, "GL_KHR_robustness");
    GLAD_GL_KHR_shader_subgroup = glad_gl_has_extension(exts, exts_i, "GL_KHR_shader_subgroup");
    GLAD_GL_KHR_texture_compression_astc_hdr = glad_gl_has_extension(exts, exts_i, "GL_KHR_texture_compression_astc_hdr");
    GLAD_GL_KHR_texture_compression_astc_ldr = glad_gl_has_extension(exts, exts_i, "GL_KHR_texture_compression_astc_ldr");
    GLAD_GL_KHR_texture_compression_astc_sliced_3d = glad_gl_has_extension(exts, exts_i, "GL_KHR_texture_compression_astc_sliced_3d");
    GLAD_GL_MESA_bgra = glad_gl_has_extension(exts, exts_i, "GL_MESA_bgra");
    GLAD_GL_MESA_framebuffer_flip_x = glad_gl_has_extension(exts, exts_i, "GL_MESA_framebuffer_flip_x");
    GLAD_GL_MESA_framebuffer_flip_y = glad_gl_has_extension(exts, exts_i, "GL_MESA_framebuffer_flip_y");
    GLAD_GL_MESA_framebuffer_swap_xy = glad_gl_has_extension(exts, exts_i, "GL_MESA_framebuffer_swap_xy");
    GLAD_GL_MESA_program_binary_formats = glad_gl_has_extension(exts, exts_i, "GL_MESA_program_binary_formats");
    GLAD_GL_MESA_sampler_objects = glad_gl_has_extension(exts, exts_i, "GL_MESA_sampler_objects");
    GLAD_GL_MESA_shader_integer_functions = glad_gl_has_extension(exts, exts_i, "GL_MESA_shader_integer_functions");
    GLAD_GL_MESA_texture_const_bandwidth = glad_gl_has_extension(exts, exts_i, "GL_MESA_texture_const_bandwidth");
    GLAD_GL_NVX_blend_equation_advanced_multi_draw_buffers = glad_gl_has_extension(exts, exts_i, "GL_NVX_blend_equation_advanced_multi_draw_buffers");
    GLAD_GL_NV_bindless_texture = glad_gl_has_extension(exts, exts_i, "GL_NV_bindless_texture");
    GLAD_GL_NV_blend_equation_advanced = glad_gl_has_extension(exts, exts_i, "GL_NV_blend_equation_advanced");
    GLAD_GL_NV_blend_equation_advanced_coherent = glad_gl_has_extension(exts, exts_i, "GL_NV_blend_equation_advanced_coherent");
    GLAD_GL_NV_blend_minmax_factor = glad_gl_has_extension(exts, exts_i, "GL_NV_blend_minmax_factor");
    GLAD_GL_NV_clip_space_w_scaling = glad_gl_has_extension(exts, exts_i, "GL_NV_clip_space_w_scaling");
    GLAD_GL_NV_compute_shader_derivatives = glad_gl_has_extension(exts, exts_i, "GL_NV_compute_shader_derivatives");
    GLAD_GL_NV_conditional_render = glad_gl_has_extension(exts, exts_i, "GL_NV_conditional_render");
    GLAD_GL_NV_conservative_raster = glad_gl_has_extension(exts, exts_i, "GL_NV_conservative_raster");
    GLAD_GL_NV_conservative_raster_pre_snap = glad_gl_has_extension(exts, exts_i, "GL_NV_conservative_raster_pre_snap");
    GLAD_GL_NV_conservative_raster_pre_snap_triangles = glad_gl_has_extension(exts, exts_i, "GL_NV_conservative_raster_pre_snap_triangles");
    GLAD_GL_NV_copy_buffer = glad_gl_has_extension(exts, exts_i, "GL_NV_copy_buffer");
    GLAD_GL_NV_coverage_sample = glad_gl_has_extension(exts, exts_i, "GL_NV_coverage_sample");
    GLAD_GL_NV_depth_nonlinear = glad_gl_has_extension(exts, exts_i, "GL_NV_depth_nonlinear");
    GLAD_GL_NV_draw_buffers = glad_gl_has_extension(exts, exts_i, "GL_NV_draw_buffers");
    GLAD_GL_NV_draw_instanced = glad_gl_has_extension(exts, exts_i, "GL_NV_draw_instanced");
    GLAD_GL_NV_draw_vulkan_image = glad_gl_has_extension(exts, exts_i, "GL_NV_draw_vulkan_image");
    GLAD_GL_NV_explicit_attrib_location = glad_gl_has_extension(exts, exts_i, "GL_NV_explicit_attrib_location");
    GLAD_GL_NV_fbo_color_attachments = glad_gl_has_extension(exts, exts_i, "GL_NV_fbo_color_attachments");
    GLAD_GL_NV_fence = glad_gl_has_extension(exts, exts_i, "GL_NV_fence");
    GLAD_GL_NV_fill_rectangle = glad_gl_has_extension(exts, exts_i, "GL_NV_fill_rectangle");
    GLAD_GL_NV_fragment_coverage_to_color = glad_gl_has_extension(exts, exts_i, "GL_NV_fragment_coverage_to_color");
    GLAD_GL_NV_fragment_shader_barycentric = glad_gl_has_extension(exts, exts_i, "GL_NV_fragment_shader_barycentric");
    GLAD_GL_NV_fragment_shader_interlock = glad_gl_has_extension(exts, exts_i, "GL_NV_fragment_shader_interlock");
    GLAD_GL_NV_framebuffer_blit = glad_gl_has_extension(exts, exts_i, "GL_NV_framebuffer_blit");
    GLAD_GL_NV_framebuffer_mixed_samples = glad_gl_has_extension(exts, exts_i, "GL_NV_framebuffer_mixed_samples");
    GLAD_GL_NV_framebuffer_multisample = glad_gl_has_extension(exts, exts_i, "GL_NV_framebuffer_multisample");
    GLAD_GL_NV_generate_mipmap_sRGB = glad_gl_has_extension(exts, exts_i, "GL_NV_generate_mipmap_sRGB");
    GLAD_GL_NV_geometry_shader_passthrough = glad_gl_has_extension(exts, exts_i, "GL_NV_geometry_shader_passthrough");
    GLAD_GL_NV_gpu_shader5 = glad_gl_has_extension(exts, exts_i, "GL_NV_gpu_shader5");
    GLAD_GL_NV_image_formats = glad_gl_has_extension(exts, exts_i, "GL_NV_image_formats");
    GLAD_GL_NV_instanced_arrays = glad_gl_has_extension(exts, exts_i, "GL_NV_instanced_arrays");
    GLAD_GL_NV_internalformat_sample_query = glad_gl_has_extension(exts, exts_i, "GL_NV_internalformat_sample_query");
    GLAD_GL_NV_memory_attachment = glad_gl_has_extension(exts, exts_i, "GL_NV_memory_attachment");
    GLAD_GL_NV_memory_object_sparse = glad_gl_has_extension(exts, exts_i, "GL_NV_memory_object_sparse");
    GLAD_GL_NV_mesh_shader = glad_gl_has_extension(exts, exts_i, "GL_NV_mesh_shader");
    GLAD_GL_NV_non_square_matrices = glad_gl_has_extension(exts, exts_i, "GL_NV_non_square_matrices");
    GLAD_GL_NV_pack_subimage = glad_gl_has_extension(exts, exts_i, "GL_NV_pack_subimage");
    GLAD_GL_NV_path_rendering = glad_gl_has_extension(exts, exts_i, "GL_NV_path_rendering");
    GLAD_GL_NV_path_rendering_shared_edge = glad_gl_has_extension(exts, exts_i, "GL_NV_path_rendering_shared_edge");
    GLAD_GL_NV_pixel_buffer_object = glad_gl_has_extension(exts, exts_i, "GL_NV_pixel_buffer_object");
    GLAD_GL_NV_polygon_mode = glad_gl_has_extension(exts, exts_i, "GL_NV_polygon_mode");
    GLAD_GL_NV_primitive_shading_rate = glad_gl_has_extension(exts, exts_i, "GL_NV_primitive_shading_rate");
    GLAD_GL_NV_read_buffer = glad_gl_has_extension(exts, exts_i, "GL_NV_read_buffer");
    GLAD_GL_NV_read_buffer_front = glad_gl_has_extension(exts, exts_i, "GL_NV_read_buffer_front");
    GLAD_GL_NV_read_depth = glad_gl_has_extension(exts, exts_i, "GL_NV_read_depth");
    GLAD_GL_NV_read_depth_stencil = glad_gl_has_extension(exts, exts_i, "GL_NV_read_depth_stencil");
    GLAD_GL_NV_read_stencil = glad_gl_has_extension(exts, exts_i, "GL_NV_read_stencil");
    GLAD_GL_NV_representative_fragment_test = glad_gl_has_extension(exts, exts_i, "GL_NV_representative_fragment_test");
    GLAD_GL_NV_sRGB_formats = glad_gl_has_extension(exts, exts_i, "GL_NV_sRGB_formats");
    GLAD_GL_NV_sample_locations = glad_gl_has_extension(exts, exts_i, "GL_NV_sample_locations");
    GLAD_GL_NV_sample_mask_override_coverage = glad_gl_has_extension(exts, exts_i, "GL_NV_sample_mask_override_coverage");
    GLAD_GL_NV_scissor_exclusive = glad_gl_has_extension(exts, exts_i, "GL_NV_scissor_exclusive");
    GLAD_GL_NV_shader_atomic_fp16_vector = glad_gl_has_extension(exts, exts_i, "GL_NV_shader_atomic_fp16_vector");
    GLAD_GL_NV_shader_noperspective_interpolation = glad_gl_has_extension(exts, exts_i, "GL_NV_shader_noperspective_interpolation");
    GLAD_GL_NV_shader_subgroup_partitioned = glad_gl_has_extension(exts, exts_i, "GL_NV_shader_subgroup_partitioned");
    GLAD_GL_NV_shader_texture_footprint = glad_gl_has_extension(exts, exts_i, "GL_NV_shader_texture_footprint");
    GLAD_GL_NV_shading_rate_image = glad_gl_has_extension(exts, exts_i, "GL_NV_shading_rate_image");
    GLAD_GL_NV_shadow_samplers_array = glad_gl_has_extension(exts, exts_i, "GL_NV_shadow_samplers_array");
    GLAD_GL_NV_shadow_samplers_cube = glad_gl_has_extension(exts, exts_i, "GL_NV_shadow_samplers_cube");
    GLAD_GL_NV_stereo_view_rendering = glad_gl_has_extension(exts, exts_i, "GL_NV_stereo_view_rendering");
    GLAD_GL_NV_texture_barrier = glad_gl_has_extension(exts, exts_i, "GL_NV_texture_barrier");
    GLAD_GL_NV_texture_border_clamp = glad_gl_has_extension(exts, exts_i, "GL_NV_texture_border_clamp");
    GLAD_GL_NV_texture_compression_s3tc_update = glad_gl_has_extension(exts, exts_i, "GL_NV_texture_compression_s3tc_update");
    GLAD_GL_NV_texture_npot_2D_mipmap = glad_gl_has_extension(exts, exts_i, "GL_NV_texture_npot_2D_mipmap");
    GLAD_GL_NV_timeline_semaphore = glad_gl_has_extension(exts, exts_i, "GL_NV_timeline_semaphore");
    GLAD_GL_NV_viewport_array = glad_gl_has_extension(exts, exts_i, "GL_NV_viewport_array");
    GLAD_GL_NV_viewport_array2 = glad_gl_has_extension(exts, exts_i, "GL_NV_viewport_array2");
    GLAD_GL_NV_viewport_swizzle = glad_gl_has_extension(exts, exts_i, "GL_NV_viewport_swizzle");
    GLAD_GL_OES_EGL_image = glad_gl_has_extension(exts, exts_i, "GL_OES_EGL_image");
    GLAD_GL_OES_EGL_image_external = glad_gl_has_extension(exts, exts_i, "GL_OES_EGL_image_external");
    GLAD_GL_OES_EGL_image_external_essl3 = glad_gl_has_extension(exts, exts_i, "GL_OES_EGL_image_external_essl3");
    GLAD_GL_OES_compressed_ETC1_RGB8_sub_texture = glad_gl_has_extension(exts, exts_i, "GL_OES_compressed_ETC1_RGB8_sub_texture");
    GLAD_GL_OES_compressed_ETC1_RGB8_texture = glad_gl_has_extension(exts, exts_i, "GL_OES_compressed_ETC1_RGB8_texture");
    GLAD_GL_OES_compressed_paletted_texture = glad_gl_has_extension(exts, exts_i, "GL_OES_compressed_paletted_texture");
    GLAD_GL_OES_copy_image = glad_gl_has_extension(exts, exts_i, "GL_OES_copy_image");
    GLAD_GL_OES_depth24 = glad_gl_has_extension(exts, exts_i, "GL_OES_depth24");
    GLAD_GL_OES_depth32 = glad_gl_has_extension(exts, exts_i, "GL_OES_depth32");
    GLAD_GL_OES_depth_texture = glad_gl_has_extension(exts, exts_i, "GL_OES_depth_texture");
    GLAD_GL_OES_draw_buffers_indexed = glad_gl_has_extension(exts, exts_i, "GL_OES_draw_buffers_indexed");
    GLAD_GL_OES_draw_elements_base_vertex = glad_gl_has_extension(exts, exts_i, "GL_OES_draw_elements_base_vertex");
    GLAD_GL_OES_element_index_uint = glad_gl_has_extension(exts, exts_i, "GL_OES_element_index_uint");
    GLAD_GL_OES_fbo_render_mipmap = glad_gl_has_extension(exts, exts_i, "GL_OES_fbo_render_mipmap");
    GLAD_GL_OES_fragment_precision_high = glad_gl_has_extension(exts, exts_i, "GL_OES_fragment_precision_high");
    GLAD_GL_OES_geometry_point_size = glad_gl_has_extension(exts, exts_i, "GL_OES_geometry_point_size");
    GLAD_GL_OES_geometry_shader = glad_gl_has_extension(exts, exts_i, "GL_OES_geometry_shader");
    GLAD_GL_OES_get_program_binary = glad_gl_has_extension(exts, exts_i, "GL_OES_get_program_binary");
    GLAD_GL_OES_gpu_shader5 = glad_gl_has_extension(exts, exts_i, "GL_OES_gpu_shader5");
    GLAD_GL_OES_mapbuffer = glad_gl_has_extension(exts, exts_i, "GL_OES_mapbuffer");
    GLAD_GL_OES_packed_depth_stencil = glad_gl_has_extension(exts, exts_i, "GL_OES_packed_depth_stencil");
    GLAD_GL_OES_primitive_bounding_box = glad_gl_has_extension(exts, exts_i, "GL_OES_primitive_bounding_box");
    GLAD_GL_OES_required_internalformat = glad_gl_has_extension(exts, exts_i, "GL_OES_required_internalformat");
    GLAD_GL_OES_rgb8_rgba8 = glad_gl_has_extension(exts, exts_i, "GL_OES_rgb8_rgba8");
    GLAD_GL_OES_sample_shading = glad_gl_has_extension(exts, exts_i, "GL_OES_sample_shading");
    GLAD_GL_OES_sample_variables = glad_gl_has_extension(exts, exts_i, "GL_OES_sample_variables");
    GLAD_GL_OES_shader_image_atomic = glad_gl_has_extension(exts, exts_i, "GL_OES_shader_image_atomic");
    GLAD_GL_OES_shader_io_blocks = glad_gl_has_extension(exts, exts_i, "GL_OES_shader_io_blocks");
    GLAD_GL_OES_shader_multisample_interpolation = glad_gl_has_extension(exts, exts_i, "GL_OES_shader_multisample_interpolation");
    GLAD_GL_OES_standard_derivatives = glad_gl_has_extension(exts, exts_i, "GL_OES_standard_derivatives");
    GLAD_GL_OES_stencil1 = glad_gl_has_extension(exts, exts_i, "GL_OES_stencil1");
    GLAD_GL_OES_stencil4 = glad_gl_has_extension(exts, exts_i, "GL_OES_stencil4");
    GLAD_GL_OES_surfaceless_context = glad_gl_has_extension(exts, exts_i, "GL_OES_surfaceless_context");
    GLAD_GL_OES_tessellation_point_size = glad_gl_has_extension(exts, exts_i, "GL_OES_tessellation_point_size");
    GLAD_GL_OES_tessellation_shader = glad_gl_has_extension(exts, exts_i, "GL_OES_tessellation_shader");
    GLAD_GL_OES_texture_3D = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_3D");
    GLAD_GL_OES_texture_border_clamp = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_border_clamp");
    GLAD_GL_OES_texture_buffer = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_buffer");
    GLAD_GL_OES_texture_compression_astc = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_compression_astc");
    GLAD_GL_OES_texture_cube_map_array = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_cube_map_array");
    GLAD_GL_OES_texture_float = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_float");
    GLAD_GL_OES_texture_float_linear = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_float_linear");
    GLAD_GL_OES_texture_half_float = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_half_float");
    GLAD_GL_OES_texture_half_float_linear = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_half_float_linear");
    GLAD_GL_OES_texture_npot = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_npot");
    GLAD_GL_OES_texture_stencil8 = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_stencil8");
    GLAD_GL_OES_texture_storage_multisample_2d_array = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_storage_multisample_2d_array");
    GLAD_GL_OES_texture_view = glad_gl_has_extension(exts, exts_i, "GL_OES_texture_view");
    GLAD_GL_OES_vertex_array_object = glad_gl_has_extension(exts, exts_i, "GL_OES_vertex_array_object");
    GLAD_GL_OES_vertex_half_float = glad_gl_has_extension(exts, exts_i, "GL_OES_vertex_half_float");
    GLAD_GL_OES_vertex_type_10_10_10_2 = glad_gl_has_extension(exts, exts_i, "GL_OES_vertex_type_10_10_10_2");
    GLAD_GL_OES_viewport_array = glad_gl_has_extension(exts, exts_i, "GL_OES_viewport_array");
    GLAD_GL_OVR_multiview = glad_gl_has_extension(exts, exts_i, "GL_OVR_multiview");
    GLAD_GL_OVR_multiview2 = glad_gl_has_extension(exts, exts_i, "GL_OVR_multiview2");
    GLAD_GL_OVR_multiview_multisampled_render_to_texture = glad_gl_has_extension(exts, exts_i, "GL_OVR_multiview_multisampled_render_to_texture");
    GLAD_GL_QCOM_YUV_texture_gather = glad_gl_has_extension(exts, exts_i, "GL_QCOM_YUV_texture_gather");
    GLAD_GL_QCOM_alpha_test = glad_gl_has_extension(exts, exts_i, "GL_QCOM_alpha_test");
    GLAD_GL_QCOM_binning_control = glad_gl_has_extension(exts, exts_i, "GL_QCOM_binning_control");
    GLAD_GL_QCOM_driver_control = glad_gl_has_extension(exts, exts_i, "GL_QCOM_driver_control");
    GLAD_GL_QCOM_extended_get = glad_gl_has_extension(exts, exts_i, "GL_QCOM_extended_get");
    GLAD_GL_QCOM_extended_get2 = glad_gl_has_extension(exts, exts_i, "GL_QCOM_extended_get2");
    GLAD_GL_QCOM_frame_extrapolation = glad_gl_has_extension(exts, exts_i, "GL_QCOM_frame_extrapolation");
    GLAD_GL_QCOM_framebuffer_foveated = glad_gl_has_extension(exts, exts_i, "GL_QCOM_framebuffer_foveated");
    GLAD_GL_QCOM_motion_estimation = glad_gl_has_extension(exts, exts_i, "GL_QCOM_motion_estimation");
    GLAD_GL_QCOM_perfmon_global_mode = glad_gl_has_extension(exts, exts_i, "GL_QCOM_perfmon_global_mode");
    GLAD_GL_QCOM_render_sRGB_R8_RG8 = glad_gl_has_extension(exts, exts_i, "GL_QCOM_render_sRGB_R8_RG8");
    GLAD_GL_QCOM_render_shared_exponent = glad_gl_has_extension(exts, exts_i, "GL_QCOM_render_shared_exponent");
    GLAD_GL_QCOM_shader_framebuffer_fetch_noncoherent = glad_gl_has_extension(exts, exts_i, "GL_QCOM_shader_framebuffer_fetch_noncoherent");
    GLAD_GL_QCOM_shader_framebuffer_fetch_rate = glad_gl_has_extension(exts, exts_i, "GL_QCOM_shader_framebuffer_fetch_rate");
    GLAD_GL_QCOM_shading_rate = glad_gl_has_extension(exts, exts_i, "GL_QCOM_shading_rate");
    GLAD_GL_QCOM_texture_foveated = glad_gl_has_extension(exts, exts_i, "GL_QCOM_texture_foveated");
    GLAD_GL_QCOM_texture_foveated2 = glad_gl_has_extension(exts, exts_i, "GL_QCOM_texture_foveated2");
    GLAD_GL_QCOM_texture_foveated_subsampled_layout = glad_gl_has_extension(exts, exts_i, "GL_QCOM_texture_foveated_subsampled_layout");
    GLAD_GL_QCOM_texture_lod_bias = glad_gl_has_extension(exts, exts_i, "GL_QCOM_texture_lod_bias");
    GLAD_GL_QCOM_tiled_rendering = glad_gl_has_extension(exts, exts_i, "GL_QCOM_tiled_rendering");
    GLAD_GL_QCOM_writeonly_rendering = glad_gl_has_extension(exts, exts_i, "GL_QCOM_writeonly_rendering");
    GLAD_GL_QCOM_ycbcr_degamma = glad_gl_has_extension(exts, exts_i, "GL_QCOM_ycbcr_degamma");
    GLAD_GL_VIV_shader_binary = glad_gl_has_extension(exts, exts_i, "GL_VIV_shader_binary");

    glad_gl_free_extensions(exts_i);

    return 1;
}

static int glad_gl_find_core_gles2(void) {
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

    GLAD_GL_ES_VERSION_2_0 = (major == 2 && minor >= 0) || major > 2;
    GLAD_GL_ES_VERSION_3_0 = (major == 3 && minor >= 0) || major > 3;
    GLAD_GL_ES_VERSION_3_1 = (major == 3 && minor >= 1) || major > 3;
    GLAD_GL_ES_VERSION_3_2 = (major == 3 && minor >= 2) || major > 3;

    return GLAD_MAKE_VERSION(major, minor);
}

int gladLoadGLES2UserPtr( GLADuserptrloadfunc load, void *userptr) {
    int version;

    glad_glGetString = (PFNGLGETSTRINGPROC) load(userptr, "glGetString");
    if(glad_glGetString == NULL) return 0;
    version = glad_gl_find_core_gles2();

    glad_gl_load_GL_ES_VERSION_2_0(load, userptr);
    glad_gl_load_GL_ES_VERSION_3_0(load, userptr);
    glad_gl_load_GL_ES_VERSION_3_1(load, userptr);
    glad_gl_load_GL_ES_VERSION_3_2(load, userptr);

    if (!glad_gl_find_extensions_gles2()) return 0;
    glad_gl_load_GL_AMD_framebuffer_multisample_advanced(load, userptr);
    glad_gl_load_GL_AMD_performance_monitor(load, userptr);
    glad_gl_load_GL_ANGLE_framebuffer_blit(load, userptr);
    glad_gl_load_GL_ANGLE_framebuffer_multisample(load, userptr);
    glad_gl_load_GL_ANGLE_instanced_arrays(load, userptr);
    glad_gl_load_GL_ANGLE_translated_shader_source(load, userptr);
    glad_gl_load_GL_APPLE_copy_texture_levels(load, userptr);
    glad_gl_load_GL_APPLE_framebuffer_multisample(load, userptr);
    glad_gl_load_GL_APPLE_sync(load, userptr);
    glad_gl_load_GL_ARM_shader_core_properties(load, userptr);
    glad_gl_load_GL_EXT_EGL_image_storage(load, userptr);
    glad_gl_load_GL_EXT_base_instance(load, userptr);
    glad_gl_load_GL_EXT_blend_func_extended(load, userptr);
    glad_gl_load_GL_EXT_buffer_storage(load, userptr);
    glad_gl_load_GL_EXT_clear_texture(load, userptr);
    glad_gl_load_GL_EXT_clip_control(load, userptr);
    glad_gl_load_GL_EXT_copy_image(load, userptr);
    glad_gl_load_GL_EXT_debug_label(load, userptr);
    glad_gl_load_GL_EXT_debug_marker(load, userptr);
    glad_gl_load_GL_EXT_discard_framebuffer(load, userptr);
    glad_gl_load_GL_EXT_disjoint_timer_query(load, userptr);
    glad_gl_load_GL_EXT_draw_buffers(load, userptr);
    glad_gl_load_GL_EXT_draw_buffers_indexed(load, userptr);
    glad_gl_load_GL_EXT_draw_elements_base_vertex(load, userptr);
    glad_gl_load_GL_EXT_draw_instanced(load, userptr);
    glad_gl_load_GL_EXT_draw_transform_feedback(load, userptr);
    glad_gl_load_GL_EXT_external_buffer(load, userptr);
    glad_gl_load_GL_EXT_fragment_shading_rate(load, userptr);
    glad_gl_load_GL_EXT_framebuffer_blit_layers(load, userptr);
    glad_gl_load_GL_EXT_geometry_shader(load, userptr);
    glad_gl_load_GL_EXT_instanced_arrays(load, userptr);
    glad_gl_load_GL_EXT_map_buffer_range(load, userptr);
    glad_gl_load_GL_EXT_memory_object(load, userptr);
    glad_gl_load_GL_EXT_memory_object_fd(load, userptr);
    glad_gl_load_GL_EXT_memory_object_win32(load, userptr);
    glad_gl_load_GL_EXT_mesh_shader(load, userptr);
    glad_gl_load_GL_EXT_multi_draw_arrays(load, userptr);
    glad_gl_load_GL_EXT_multi_draw_indirect(load, userptr);
    glad_gl_load_GL_EXT_multisampled_render_to_texture(load, userptr);
    glad_gl_load_GL_EXT_multiview_draw_buffers(load, userptr);
    glad_gl_load_GL_EXT_occlusion_query_boolean(load, userptr);
    glad_gl_load_GL_EXT_polygon_offset_clamp(load, userptr);
    glad_gl_load_GL_EXT_primitive_bounding_box(load, userptr);
    glad_gl_load_GL_EXT_raster_multisample(load, userptr);
    glad_gl_load_GL_EXT_robustness(load, userptr);
    glad_gl_load_GL_EXT_semaphore(load, userptr);
    glad_gl_load_GL_EXT_semaphore_fd(load, userptr);
    glad_gl_load_GL_EXT_semaphore_win32(load, userptr);
    glad_gl_load_GL_EXT_separate_shader_objects(load, userptr);
    glad_gl_load_GL_EXT_shader_framebuffer_fetch_non_coherent(load, userptr);
    glad_gl_load_GL_EXT_shader_pixel_local_storage2(load, userptr);
    glad_gl_load_GL_EXT_sparse_texture(load, userptr);
    glad_gl_load_GL_EXT_tessellation_shader(load, userptr);
    glad_gl_load_GL_EXT_texture_border_clamp(load, userptr);
    glad_gl_load_GL_EXT_texture_buffer(load, userptr);
    glad_gl_load_GL_EXT_texture_storage(load, userptr);
    glad_gl_load_GL_EXT_texture_storage_compression(load, userptr);
    glad_gl_load_GL_EXT_texture_view(load, userptr);
    glad_gl_load_GL_EXT_win32_keyed_mutex(load, userptr);
    glad_gl_load_GL_EXT_window_rectangles(load, userptr);
    glad_gl_load_GL_IMG_bindless_texture(load, userptr);
    glad_gl_load_GL_IMG_framebuffer_downsample(load, userptr);
    glad_gl_load_GL_IMG_multisampled_render_to_texture(load, userptr);
    glad_gl_load_GL_INTEL_framebuffer_CMAA(load, userptr);
    glad_gl_load_GL_INTEL_performance_query(load, userptr);
    glad_gl_load_GL_KHR_blend_equation_advanced(load, userptr);
    glad_gl_load_GL_KHR_debug(load, userptr);
    glad_gl_load_GL_KHR_parallel_shader_compile(load, userptr);
    glad_gl_load_GL_KHR_robustness(load, userptr);
    glad_gl_load_GL_MESA_framebuffer_flip_y(load, userptr);
    glad_gl_load_GL_MESA_sampler_objects(load, userptr);
    glad_gl_load_GL_NV_bindless_texture(load, userptr);
    glad_gl_load_GL_NV_blend_equation_advanced(load, userptr);
    glad_gl_load_GL_NV_clip_space_w_scaling(load, userptr);
    glad_gl_load_GL_NV_conditional_render(load, userptr);
    glad_gl_load_GL_NV_conservative_raster(load, userptr);
    glad_gl_load_GL_NV_conservative_raster_pre_snap_triangles(load, userptr);
    glad_gl_load_GL_NV_copy_buffer(load, userptr);
    glad_gl_load_GL_NV_coverage_sample(load, userptr);
    glad_gl_load_GL_NV_draw_buffers(load, userptr);
    glad_gl_load_GL_NV_draw_instanced(load, userptr);
    glad_gl_load_GL_NV_draw_vulkan_image(load, userptr);
    glad_gl_load_GL_NV_fence(load, userptr);
    glad_gl_load_GL_NV_fragment_coverage_to_color(load, userptr);
    glad_gl_load_GL_NV_framebuffer_blit(load, userptr);
    glad_gl_load_GL_NV_framebuffer_mixed_samples(load, userptr);
    glad_gl_load_GL_NV_framebuffer_multisample(load, userptr);
    glad_gl_load_GL_NV_gpu_shader5(load, userptr);
    glad_gl_load_GL_NV_instanced_arrays(load, userptr);
    glad_gl_load_GL_NV_internalformat_sample_query(load, userptr);
    glad_gl_load_GL_NV_memory_attachment(load, userptr);
    glad_gl_load_GL_NV_memory_object_sparse(load, userptr);
    glad_gl_load_GL_NV_mesh_shader(load, userptr);
    glad_gl_load_GL_NV_non_square_matrices(load, userptr);
    glad_gl_load_GL_NV_path_rendering(load, userptr);
    glad_gl_load_GL_NV_polygon_mode(load, userptr);
    glad_gl_load_GL_NV_read_buffer(load, userptr);
    glad_gl_load_GL_NV_sample_locations(load, userptr);
    glad_gl_load_GL_NV_scissor_exclusive(load, userptr);
    glad_gl_load_GL_NV_shading_rate_image(load, userptr);
    glad_gl_load_GL_NV_texture_barrier(load, userptr);
    glad_gl_load_GL_NV_timeline_semaphore(load, userptr);
    glad_gl_load_GL_NV_viewport_array(load, userptr);
    glad_gl_load_GL_NV_viewport_swizzle(load, userptr);
    glad_gl_load_GL_OES_EGL_image(load, userptr);
    glad_gl_load_GL_OES_EGL_image_external(load, userptr);
    glad_gl_load_GL_OES_copy_image(load, userptr);
    glad_gl_load_GL_OES_draw_buffers_indexed(load, userptr);
    glad_gl_load_GL_OES_draw_elements_base_vertex(load, userptr);
    glad_gl_load_GL_OES_geometry_shader(load, userptr);
    glad_gl_load_GL_OES_get_program_binary(load, userptr);
    glad_gl_load_GL_OES_mapbuffer(load, userptr);
    glad_gl_load_GL_OES_primitive_bounding_box(load, userptr);
    glad_gl_load_GL_OES_sample_shading(load, userptr);
    glad_gl_load_GL_OES_tessellation_shader(load, userptr);
    glad_gl_load_GL_OES_texture_3D(load, userptr);
    glad_gl_load_GL_OES_texture_border_clamp(load, userptr);
    glad_gl_load_GL_OES_texture_buffer(load, userptr);
    glad_gl_load_GL_OES_texture_storage_multisample_2d_array(load, userptr);
    glad_gl_load_GL_OES_texture_view(load, userptr);
    glad_gl_load_GL_OES_vertex_array_object(load, userptr);
    glad_gl_load_GL_OES_viewport_array(load, userptr);
    glad_gl_load_GL_OVR_multiview(load, userptr);
    glad_gl_load_GL_OVR_multiview_multisampled_render_to_texture(load, userptr);
    glad_gl_load_GL_QCOM_alpha_test(load, userptr);
    glad_gl_load_GL_QCOM_driver_control(load, userptr);
    glad_gl_load_GL_QCOM_extended_get(load, userptr);
    glad_gl_load_GL_QCOM_extended_get2(load, userptr);
    glad_gl_load_GL_QCOM_frame_extrapolation(load, userptr);
    glad_gl_load_GL_QCOM_framebuffer_foveated(load, userptr);
    glad_gl_load_GL_QCOM_motion_estimation(load, userptr);
    glad_gl_load_GL_QCOM_shader_framebuffer_fetch_noncoherent(load, userptr);
    glad_gl_load_GL_QCOM_shading_rate(load, userptr);
    glad_gl_load_GL_QCOM_texture_foveated(load, userptr);
    glad_gl_load_GL_QCOM_tiled_rendering(load, userptr);



    return version;
}


int gladLoadGLES2( GLADloadfunc load) {
    return gladLoadGLES2UserPtr( glad_gl_get_proc_from_userptr, GLAD_GNUC_EXTENSION (void*) load);
}



 
void gladInstallGLES2Debug(void) {
    glad_debug_glAcquireKeyedMutexWin32EXT = glad_debug_impl_glAcquireKeyedMutexWin32EXT;
    glad_debug_glActiveShaderProgram = glad_debug_impl_glActiveShaderProgram;
    glad_debug_glActiveShaderProgramEXT = glad_debug_impl_glActiveShaderProgramEXT;
    glad_debug_glActiveTexture = glad_debug_impl_glActiveTexture;
    glad_debug_glAlphaFuncQCOM = glad_debug_impl_glAlphaFuncQCOM;
    glad_debug_glApplyFramebufferAttachmentCMAAINTEL = glad_debug_impl_glApplyFramebufferAttachmentCMAAINTEL;
    glad_debug_glAttachShader = glad_debug_impl_glAttachShader;
    glad_debug_glBeginConditionalRenderNV = glad_debug_impl_glBeginConditionalRenderNV;
    glad_debug_glBeginPerfMonitorAMD = glad_debug_impl_glBeginPerfMonitorAMD;
    glad_debug_glBeginPerfQueryINTEL = glad_debug_impl_glBeginPerfQueryINTEL;
    glad_debug_glBeginQuery = glad_debug_impl_glBeginQuery;
    glad_debug_glBeginQueryEXT = glad_debug_impl_glBeginQueryEXT;
    glad_debug_glBeginTransformFeedback = glad_debug_impl_glBeginTransformFeedback;
    glad_debug_glBindAttribLocation = glad_debug_impl_glBindAttribLocation;
    glad_debug_glBindBuffer = glad_debug_impl_glBindBuffer;
    glad_debug_glBindBufferBase = glad_debug_impl_glBindBufferBase;
    glad_debug_glBindBufferRange = glad_debug_impl_glBindBufferRange;
    glad_debug_glBindFragDataLocationEXT = glad_debug_impl_glBindFragDataLocationEXT;
    glad_debug_glBindFragDataLocationIndexedEXT = glad_debug_impl_glBindFragDataLocationIndexedEXT;
    glad_debug_glBindFramebuffer = glad_debug_impl_glBindFramebuffer;
    glad_debug_glBindImageTexture = glad_debug_impl_glBindImageTexture;
    glad_debug_glBindProgramPipeline = glad_debug_impl_glBindProgramPipeline;
    glad_debug_glBindProgramPipelineEXT = glad_debug_impl_glBindProgramPipelineEXT;
    glad_debug_glBindRenderbuffer = glad_debug_impl_glBindRenderbuffer;
    glad_debug_glBindSampler = glad_debug_impl_glBindSampler;
    glad_debug_glBindShadingRateImageNV = glad_debug_impl_glBindShadingRateImageNV;
    glad_debug_glBindTexture = glad_debug_impl_glBindTexture;
    glad_debug_glBindTransformFeedback = glad_debug_impl_glBindTransformFeedback;
    glad_debug_glBindVertexArray = glad_debug_impl_glBindVertexArray;
    glad_debug_glBindVertexArrayOES = glad_debug_impl_glBindVertexArrayOES;
    glad_debug_glBindVertexBuffer = glad_debug_impl_glBindVertexBuffer;
    glad_debug_glBlendBarrier = glad_debug_impl_glBlendBarrier;
    glad_debug_glBlendBarrierKHR = glad_debug_impl_glBlendBarrierKHR;
    glad_debug_glBlendBarrierNV = glad_debug_impl_glBlendBarrierNV;
    glad_debug_glBlendColor = glad_debug_impl_glBlendColor;
    glad_debug_glBlendEquation = glad_debug_impl_glBlendEquation;
    glad_debug_glBlendEquationSeparate = glad_debug_impl_glBlendEquationSeparate;
    glad_debug_glBlendEquationSeparatei = glad_debug_impl_glBlendEquationSeparatei;
    glad_debug_glBlendEquationSeparateiEXT = glad_debug_impl_glBlendEquationSeparateiEXT;
    glad_debug_glBlendEquationSeparateiOES = glad_debug_impl_glBlendEquationSeparateiOES;
    glad_debug_glBlendEquationi = glad_debug_impl_glBlendEquationi;
    glad_debug_glBlendEquationiEXT = glad_debug_impl_glBlendEquationiEXT;
    glad_debug_glBlendEquationiOES = glad_debug_impl_glBlendEquationiOES;
    glad_debug_glBlendFunc = glad_debug_impl_glBlendFunc;
    glad_debug_glBlendFuncSeparate = glad_debug_impl_glBlendFuncSeparate;
    glad_debug_glBlendFuncSeparatei = glad_debug_impl_glBlendFuncSeparatei;
    glad_debug_glBlendFuncSeparateiEXT = glad_debug_impl_glBlendFuncSeparateiEXT;
    glad_debug_glBlendFuncSeparateiOES = glad_debug_impl_glBlendFuncSeparateiOES;
    glad_debug_glBlendFunci = glad_debug_impl_glBlendFunci;
    glad_debug_glBlendFunciEXT = glad_debug_impl_glBlendFunciEXT;
    glad_debug_glBlendFunciOES = glad_debug_impl_glBlendFunciOES;
    glad_debug_glBlendParameteriNV = glad_debug_impl_glBlendParameteriNV;
    glad_debug_glBlitFramebuffer = glad_debug_impl_glBlitFramebuffer;
    glad_debug_glBlitFramebufferANGLE = glad_debug_impl_glBlitFramebufferANGLE;
    glad_debug_glBlitFramebufferLayerEXT = glad_debug_impl_glBlitFramebufferLayerEXT;
    glad_debug_glBlitFramebufferLayersEXT = glad_debug_impl_glBlitFramebufferLayersEXT;
    glad_debug_glBlitFramebufferNV = glad_debug_impl_glBlitFramebufferNV;
    glad_debug_glBufferAttachMemoryNV = glad_debug_impl_glBufferAttachMemoryNV;
    glad_debug_glBufferData = glad_debug_impl_glBufferData;
    glad_debug_glBufferPageCommitmentMemNV = glad_debug_impl_glBufferPageCommitmentMemNV;
    glad_debug_glBufferStorageEXT = glad_debug_impl_glBufferStorageEXT;
    glad_debug_glBufferStorageExternalEXT = glad_debug_impl_glBufferStorageExternalEXT;
    glad_debug_glBufferStorageMemEXT = glad_debug_impl_glBufferStorageMemEXT;
    glad_debug_glBufferSubData = glad_debug_impl_glBufferSubData;
    glad_debug_glCheckFramebufferStatus = glad_debug_impl_glCheckFramebufferStatus;
    glad_debug_glClear = glad_debug_impl_glClear;
    glad_debug_glClearBufferfi = glad_debug_impl_glClearBufferfi;
    glad_debug_glClearBufferfv = glad_debug_impl_glClearBufferfv;
    glad_debug_glClearBufferiv = glad_debug_impl_glClearBufferiv;
    glad_debug_glClearBufferuiv = glad_debug_impl_glClearBufferuiv;
    glad_debug_glClearColor = glad_debug_impl_glClearColor;
    glad_debug_glClearDepthf = glad_debug_impl_glClearDepthf;
    glad_debug_glClearPixelLocalStorageuiEXT = glad_debug_impl_glClearPixelLocalStorageuiEXT;
    glad_debug_glClearStencil = glad_debug_impl_glClearStencil;
    glad_debug_glClearTexImageEXT = glad_debug_impl_glClearTexImageEXT;
    glad_debug_glClearTexSubImageEXT = glad_debug_impl_glClearTexSubImageEXT;
    glad_debug_glClientWaitSync = glad_debug_impl_glClientWaitSync;
    glad_debug_glClientWaitSyncAPPLE = glad_debug_impl_glClientWaitSyncAPPLE;
    glad_debug_glClipControlEXT = glad_debug_impl_glClipControlEXT;
    glad_debug_glColorMask = glad_debug_impl_glColorMask;
    glad_debug_glColorMaski = glad_debug_impl_glColorMaski;
    glad_debug_glColorMaskiEXT = glad_debug_impl_glColorMaskiEXT;
    glad_debug_glColorMaskiOES = glad_debug_impl_glColorMaskiOES;
    glad_debug_glCompileShader = glad_debug_impl_glCompileShader;
    glad_debug_glCompressedTexImage2D = glad_debug_impl_glCompressedTexImage2D;
    glad_debug_glCompressedTexImage3D = glad_debug_impl_glCompressedTexImage3D;
    glad_debug_glCompressedTexImage3DOES = glad_debug_impl_glCompressedTexImage3DOES;
    glad_debug_glCompressedTexSubImage2D = glad_debug_impl_glCompressedTexSubImage2D;
    glad_debug_glCompressedTexSubImage3D = glad_debug_impl_glCompressedTexSubImage3D;
    glad_debug_glCompressedTexSubImage3DOES = glad_debug_impl_glCompressedTexSubImage3DOES;
    glad_debug_glConservativeRasterParameteriNV = glad_debug_impl_glConservativeRasterParameteriNV;
    glad_debug_glCopyBufferSubData = glad_debug_impl_glCopyBufferSubData;
    glad_debug_glCopyBufferSubDataNV = glad_debug_impl_glCopyBufferSubDataNV;
    glad_debug_glCopyImageSubData = glad_debug_impl_glCopyImageSubData;
    glad_debug_glCopyImageSubDataEXT = glad_debug_impl_glCopyImageSubDataEXT;
    glad_debug_glCopyImageSubDataOES = glad_debug_impl_glCopyImageSubDataOES;
    glad_debug_glCopyPathNV = glad_debug_impl_glCopyPathNV;
    glad_debug_glCopyTexImage2D = glad_debug_impl_glCopyTexImage2D;
    glad_debug_glCopyTexSubImage2D = glad_debug_impl_glCopyTexSubImage2D;
    glad_debug_glCopyTexSubImage3D = glad_debug_impl_glCopyTexSubImage3D;
    glad_debug_glCopyTexSubImage3DOES = glad_debug_impl_glCopyTexSubImage3DOES;
    glad_debug_glCopyTextureLevelsAPPLE = glad_debug_impl_glCopyTextureLevelsAPPLE;
    glad_debug_glCoverFillPathInstancedNV = glad_debug_impl_glCoverFillPathInstancedNV;
    glad_debug_glCoverFillPathNV = glad_debug_impl_glCoverFillPathNV;
    glad_debug_glCoverStrokePathInstancedNV = glad_debug_impl_glCoverStrokePathInstancedNV;
    glad_debug_glCoverStrokePathNV = glad_debug_impl_glCoverStrokePathNV;
    glad_debug_glCoverageMaskNV = glad_debug_impl_glCoverageMaskNV;
    glad_debug_glCoverageModulationNV = glad_debug_impl_glCoverageModulationNV;
    glad_debug_glCoverageModulationTableNV = glad_debug_impl_glCoverageModulationTableNV;
    glad_debug_glCoverageOperationNV = glad_debug_impl_glCoverageOperationNV;
    glad_debug_glCreateMemoryObjectsEXT = glad_debug_impl_glCreateMemoryObjectsEXT;
    glad_debug_glCreatePerfQueryINTEL = glad_debug_impl_glCreatePerfQueryINTEL;
    glad_debug_glCreateProgram = glad_debug_impl_glCreateProgram;
    glad_debug_glCreateSemaphoresNV = glad_debug_impl_glCreateSemaphoresNV;
    glad_debug_glCreateShader = glad_debug_impl_glCreateShader;
    glad_debug_glCreateShaderProgramv = glad_debug_impl_glCreateShaderProgramv;
    glad_debug_glCreateShaderProgramvEXT = glad_debug_impl_glCreateShaderProgramvEXT;
    glad_debug_glCullFace = glad_debug_impl_glCullFace;
    glad_debug_glDebugMessageCallback = glad_debug_impl_glDebugMessageCallback;
    glad_debug_glDebugMessageCallbackKHR = glad_debug_impl_glDebugMessageCallbackKHR;
    glad_debug_glDebugMessageControl = glad_debug_impl_glDebugMessageControl;
    glad_debug_glDebugMessageControlKHR = glad_debug_impl_glDebugMessageControlKHR;
    glad_debug_glDebugMessageInsert = glad_debug_impl_glDebugMessageInsert;
    glad_debug_glDebugMessageInsertKHR = glad_debug_impl_glDebugMessageInsertKHR;
    glad_debug_glDeleteBuffers = glad_debug_impl_glDeleteBuffers;
    glad_debug_glDeleteFencesNV = glad_debug_impl_glDeleteFencesNV;
    glad_debug_glDeleteFramebuffers = glad_debug_impl_glDeleteFramebuffers;
    glad_debug_glDeleteMemoryObjectsEXT = glad_debug_impl_glDeleteMemoryObjectsEXT;
    glad_debug_glDeletePathsNV = glad_debug_impl_glDeletePathsNV;
    glad_debug_glDeletePerfMonitorsAMD = glad_debug_impl_glDeletePerfMonitorsAMD;
    glad_debug_glDeletePerfQueryINTEL = glad_debug_impl_glDeletePerfQueryINTEL;
    glad_debug_glDeleteProgram = glad_debug_impl_glDeleteProgram;
    glad_debug_glDeleteProgramPipelines = glad_debug_impl_glDeleteProgramPipelines;
    glad_debug_glDeleteProgramPipelinesEXT = glad_debug_impl_glDeleteProgramPipelinesEXT;
    glad_debug_glDeleteQueries = glad_debug_impl_glDeleteQueries;
    glad_debug_glDeleteQueriesEXT = glad_debug_impl_glDeleteQueriesEXT;
    glad_debug_glDeleteRenderbuffers = glad_debug_impl_glDeleteRenderbuffers;
    glad_debug_glDeleteSamplers = glad_debug_impl_glDeleteSamplers;
    glad_debug_glDeleteSemaphoresEXT = glad_debug_impl_glDeleteSemaphoresEXT;
    glad_debug_glDeleteShader = glad_debug_impl_glDeleteShader;
    glad_debug_glDeleteSync = glad_debug_impl_glDeleteSync;
    glad_debug_glDeleteSyncAPPLE = glad_debug_impl_glDeleteSyncAPPLE;
    glad_debug_glDeleteTextures = glad_debug_impl_glDeleteTextures;
    glad_debug_glDeleteTransformFeedbacks = glad_debug_impl_glDeleteTransformFeedbacks;
    glad_debug_glDeleteVertexArrays = glad_debug_impl_glDeleteVertexArrays;
    glad_debug_glDeleteVertexArraysOES = glad_debug_impl_glDeleteVertexArraysOES;
    glad_debug_glDepthFunc = glad_debug_impl_glDepthFunc;
    glad_debug_glDepthMask = glad_debug_impl_glDepthMask;
    glad_debug_glDepthRangeArrayfvNV = glad_debug_impl_glDepthRangeArrayfvNV;
    glad_debug_glDepthRangeArrayfvOES = glad_debug_impl_glDepthRangeArrayfvOES;
    glad_debug_glDepthRangeIndexedfNV = glad_debug_impl_glDepthRangeIndexedfNV;
    glad_debug_glDepthRangeIndexedfOES = glad_debug_impl_glDepthRangeIndexedfOES;
    glad_debug_glDepthRangef = glad_debug_impl_glDepthRangef;
    glad_debug_glDetachShader = glad_debug_impl_glDetachShader;
    glad_debug_glDisable = glad_debug_impl_glDisable;
    glad_debug_glDisableDriverControlQCOM = glad_debug_impl_glDisableDriverControlQCOM;
    glad_debug_glDisableVertexAttribArray = glad_debug_impl_glDisableVertexAttribArray;
    glad_debug_glDisablei = glad_debug_impl_glDisablei;
    glad_debug_glDisableiEXT = glad_debug_impl_glDisableiEXT;
    glad_debug_glDisableiNV = glad_debug_impl_glDisableiNV;
    glad_debug_glDisableiOES = glad_debug_impl_glDisableiOES;
    glad_debug_glDiscardFramebufferEXT = glad_debug_impl_glDiscardFramebufferEXT;
    glad_debug_glDispatchCompute = glad_debug_impl_glDispatchCompute;
    glad_debug_glDispatchComputeIndirect = glad_debug_impl_glDispatchComputeIndirect;
    glad_debug_glDrawArrays = glad_debug_impl_glDrawArrays;
    glad_debug_glDrawArraysIndirect = glad_debug_impl_glDrawArraysIndirect;
    glad_debug_glDrawArraysInstanced = glad_debug_impl_glDrawArraysInstanced;
    glad_debug_glDrawArraysInstancedANGLE = glad_debug_impl_glDrawArraysInstancedANGLE;
    glad_debug_glDrawArraysInstancedBaseInstanceEXT = glad_debug_impl_glDrawArraysInstancedBaseInstanceEXT;
    glad_debug_glDrawArraysInstancedEXT = glad_debug_impl_glDrawArraysInstancedEXT;
    glad_debug_glDrawArraysInstancedNV = glad_debug_impl_glDrawArraysInstancedNV;
    glad_debug_glDrawBuffers = glad_debug_impl_glDrawBuffers;
    glad_debug_glDrawBuffersEXT = glad_debug_impl_glDrawBuffersEXT;
    glad_debug_glDrawBuffersIndexedEXT = glad_debug_impl_glDrawBuffersIndexedEXT;
    glad_debug_glDrawBuffersNV = glad_debug_impl_glDrawBuffersNV;
    glad_debug_glDrawElements = glad_debug_impl_glDrawElements;
    glad_debug_glDrawElementsBaseVertex = glad_debug_impl_glDrawElementsBaseVertex;
    glad_debug_glDrawElementsBaseVertexEXT = glad_debug_impl_glDrawElementsBaseVertexEXT;
    glad_debug_glDrawElementsBaseVertexOES = glad_debug_impl_glDrawElementsBaseVertexOES;
    glad_debug_glDrawElementsIndirect = glad_debug_impl_glDrawElementsIndirect;
    glad_debug_glDrawElementsInstanced = glad_debug_impl_glDrawElementsInstanced;
    glad_debug_glDrawElementsInstancedANGLE = glad_debug_impl_glDrawElementsInstancedANGLE;
    glad_debug_glDrawElementsInstancedBaseInstanceEXT = glad_debug_impl_glDrawElementsInstancedBaseInstanceEXT;
    glad_debug_glDrawElementsInstancedBaseVertex = glad_debug_impl_glDrawElementsInstancedBaseVertex;
    glad_debug_glDrawElementsInstancedBaseVertexBaseInstanceEXT = glad_debug_impl_glDrawElementsInstancedBaseVertexBaseInstanceEXT;
    glad_debug_glDrawElementsInstancedBaseVertexEXT = glad_debug_impl_glDrawElementsInstancedBaseVertexEXT;
    glad_debug_glDrawElementsInstancedBaseVertexOES = glad_debug_impl_glDrawElementsInstancedBaseVertexOES;
    glad_debug_glDrawElementsInstancedEXT = glad_debug_impl_glDrawElementsInstancedEXT;
    glad_debug_glDrawElementsInstancedNV = glad_debug_impl_glDrawElementsInstancedNV;
    glad_debug_glDrawMeshTasksEXT = glad_debug_impl_glDrawMeshTasksEXT;
    glad_debug_glDrawMeshTasksIndirectEXT = glad_debug_impl_glDrawMeshTasksIndirectEXT;
    glad_debug_glDrawMeshTasksIndirectNV = glad_debug_impl_glDrawMeshTasksIndirectNV;
    glad_debug_glDrawMeshTasksNV = glad_debug_impl_glDrawMeshTasksNV;
    glad_debug_glDrawRangeElements = glad_debug_impl_glDrawRangeElements;
    glad_debug_glDrawRangeElementsBaseVertex = glad_debug_impl_glDrawRangeElementsBaseVertex;
    glad_debug_glDrawRangeElementsBaseVertexEXT = glad_debug_impl_glDrawRangeElementsBaseVertexEXT;
    glad_debug_glDrawRangeElementsBaseVertexOES = glad_debug_impl_glDrawRangeElementsBaseVertexOES;
    glad_debug_glDrawTransformFeedbackEXT = glad_debug_impl_glDrawTransformFeedbackEXT;
    glad_debug_glDrawTransformFeedbackInstancedEXT = glad_debug_impl_glDrawTransformFeedbackInstancedEXT;
    glad_debug_glDrawVkImageNV = glad_debug_impl_glDrawVkImageNV;
    glad_debug_glEGLImageTargetRenderbufferStorageOES = glad_debug_impl_glEGLImageTargetRenderbufferStorageOES;
    glad_debug_glEGLImageTargetTexStorageEXT = glad_debug_impl_glEGLImageTargetTexStorageEXT;
    glad_debug_glEGLImageTargetTexture2DOES = glad_debug_impl_glEGLImageTargetTexture2DOES;
    glad_debug_glEGLImageTargetTextureStorageEXT = glad_debug_impl_glEGLImageTargetTextureStorageEXT;
    glad_debug_glEnable = glad_debug_impl_glEnable;
    glad_debug_glEnableDriverControlQCOM = glad_debug_impl_glEnableDriverControlQCOM;
    glad_debug_glEnableVertexAttribArray = glad_debug_impl_glEnableVertexAttribArray;
    glad_debug_glEnablei = glad_debug_impl_glEnablei;
    glad_debug_glEnableiEXT = glad_debug_impl_glEnableiEXT;
    glad_debug_glEnableiNV = glad_debug_impl_glEnableiNV;
    glad_debug_glEnableiOES = glad_debug_impl_glEnableiOES;
    glad_debug_glEndConditionalRenderNV = glad_debug_impl_glEndConditionalRenderNV;
    glad_debug_glEndPerfMonitorAMD = glad_debug_impl_glEndPerfMonitorAMD;
    glad_debug_glEndPerfQueryINTEL = glad_debug_impl_glEndPerfQueryINTEL;
    glad_debug_glEndQuery = glad_debug_impl_glEndQuery;
    glad_debug_glEndQueryEXT = glad_debug_impl_glEndQueryEXT;
    glad_debug_glEndTilingQCOM = glad_debug_impl_glEndTilingQCOM;
    glad_debug_glEndTransformFeedback = glad_debug_impl_glEndTransformFeedback;
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
    glad_debug_glExtrapolateTex2DQCOM = glad_debug_impl_glExtrapolateTex2DQCOM;
    glad_debug_glFenceSync = glad_debug_impl_glFenceSync;
    glad_debug_glFenceSyncAPPLE = glad_debug_impl_glFenceSyncAPPLE;
    glad_debug_glFinish = glad_debug_impl_glFinish;
    glad_debug_glFinishFenceNV = glad_debug_impl_glFinishFenceNV;
    glad_debug_glFlush = glad_debug_impl_glFlush;
    glad_debug_glFlushMappedBufferRange = glad_debug_impl_glFlushMappedBufferRange;
    glad_debug_glFlushMappedBufferRangeEXT = glad_debug_impl_glFlushMappedBufferRangeEXT;
    glad_debug_glFragmentCoverageColorNV = glad_debug_impl_glFragmentCoverageColorNV;
    glad_debug_glFramebufferFetchBarrierEXT = glad_debug_impl_glFramebufferFetchBarrierEXT;
    glad_debug_glFramebufferFetchBarrierQCOM = glad_debug_impl_glFramebufferFetchBarrierQCOM;
    glad_debug_glFramebufferFoveationConfigQCOM = glad_debug_impl_glFramebufferFoveationConfigQCOM;
    glad_debug_glFramebufferFoveationParametersQCOM = glad_debug_impl_glFramebufferFoveationParametersQCOM;
    glad_debug_glFramebufferParameteri = glad_debug_impl_glFramebufferParameteri;
    glad_debug_glFramebufferParameteriMESA = glad_debug_impl_glFramebufferParameteriMESA;
    glad_debug_glFramebufferPixelLocalStorageSizeEXT = glad_debug_impl_glFramebufferPixelLocalStorageSizeEXT;
    glad_debug_glFramebufferRenderbuffer = glad_debug_impl_glFramebufferRenderbuffer;
    glad_debug_glFramebufferSampleLocationsfvNV = glad_debug_impl_glFramebufferSampleLocationsfvNV;
    glad_debug_glFramebufferShadingRateEXT = glad_debug_impl_glFramebufferShadingRateEXT;
    glad_debug_glFramebufferTexture = glad_debug_impl_glFramebufferTexture;
    glad_debug_glFramebufferTexture2D = glad_debug_impl_glFramebufferTexture2D;
    glad_debug_glFramebufferTexture2DDownsampleIMG = glad_debug_impl_glFramebufferTexture2DDownsampleIMG;
    glad_debug_glFramebufferTexture2DMultisampleEXT = glad_debug_impl_glFramebufferTexture2DMultisampleEXT;
    glad_debug_glFramebufferTexture2DMultisampleIMG = glad_debug_impl_glFramebufferTexture2DMultisampleIMG;
    glad_debug_glFramebufferTexture3DOES = glad_debug_impl_glFramebufferTexture3DOES;
    glad_debug_glFramebufferTextureEXT = glad_debug_impl_glFramebufferTextureEXT;
    glad_debug_glFramebufferTextureLayer = glad_debug_impl_glFramebufferTextureLayer;
    glad_debug_glFramebufferTextureLayerDownsampleIMG = glad_debug_impl_glFramebufferTextureLayerDownsampleIMG;
    glad_debug_glFramebufferTextureMultisampleMultiviewOVR = glad_debug_impl_glFramebufferTextureMultisampleMultiviewOVR;
    glad_debug_glFramebufferTextureMultiviewOVR = glad_debug_impl_glFramebufferTextureMultiviewOVR;
    glad_debug_glFramebufferTextureOES = glad_debug_impl_glFramebufferTextureOES;
    glad_debug_glFrontFace = glad_debug_impl_glFrontFace;
    glad_debug_glGenBuffers = glad_debug_impl_glGenBuffers;
    glad_debug_glGenFencesNV = glad_debug_impl_glGenFencesNV;
    glad_debug_glGenFramebuffers = glad_debug_impl_glGenFramebuffers;
    glad_debug_glGenPathsNV = glad_debug_impl_glGenPathsNV;
    glad_debug_glGenPerfMonitorsAMD = glad_debug_impl_glGenPerfMonitorsAMD;
    glad_debug_glGenProgramPipelines = glad_debug_impl_glGenProgramPipelines;
    glad_debug_glGenProgramPipelinesEXT = glad_debug_impl_glGenProgramPipelinesEXT;
    glad_debug_glGenQueries = glad_debug_impl_glGenQueries;
    glad_debug_glGenQueriesEXT = glad_debug_impl_glGenQueriesEXT;
    glad_debug_glGenRenderbuffers = glad_debug_impl_glGenRenderbuffers;
    glad_debug_glGenSamplers = glad_debug_impl_glGenSamplers;
    glad_debug_glGenSemaphoresEXT = glad_debug_impl_glGenSemaphoresEXT;
    glad_debug_glGenTextures = glad_debug_impl_glGenTextures;
    glad_debug_glGenTransformFeedbacks = glad_debug_impl_glGenTransformFeedbacks;
    glad_debug_glGenVertexArrays = glad_debug_impl_glGenVertexArrays;
    glad_debug_glGenVertexArraysOES = glad_debug_impl_glGenVertexArraysOES;
    glad_debug_glGenerateMipmap = glad_debug_impl_glGenerateMipmap;
    glad_debug_glGetActiveAttrib = glad_debug_impl_glGetActiveAttrib;
    glad_debug_glGetActiveUniform = glad_debug_impl_glGetActiveUniform;
    glad_debug_glGetActiveUniformBlockName = glad_debug_impl_glGetActiveUniformBlockName;
    glad_debug_glGetActiveUniformBlockiv = glad_debug_impl_glGetActiveUniformBlockiv;
    glad_debug_glGetActiveUniformsiv = glad_debug_impl_glGetActiveUniformsiv;
    glad_debug_glGetAttachedShaders = glad_debug_impl_glGetAttachedShaders;
    glad_debug_glGetAttribLocation = glad_debug_impl_glGetAttribLocation;
    glad_debug_glGetBooleani_v = glad_debug_impl_glGetBooleani_v;
    glad_debug_glGetBooleanv = glad_debug_impl_glGetBooleanv;
    glad_debug_glGetBufferParameteri64v = glad_debug_impl_glGetBufferParameteri64v;
    glad_debug_glGetBufferParameteriv = glad_debug_impl_glGetBufferParameteriv;
    glad_debug_glGetBufferPointerv = glad_debug_impl_glGetBufferPointerv;
    glad_debug_glGetBufferPointervOES = glad_debug_impl_glGetBufferPointervOES;
    glad_debug_glGetCoverageModulationTableNV = glad_debug_impl_glGetCoverageModulationTableNV;
    glad_debug_glGetDebugMessageLog = glad_debug_impl_glGetDebugMessageLog;
    glad_debug_glGetDebugMessageLogKHR = glad_debug_impl_glGetDebugMessageLogKHR;
    glad_debug_glGetDriverControlStringQCOM = glad_debug_impl_glGetDriverControlStringQCOM;
    glad_debug_glGetDriverControlsQCOM = glad_debug_impl_glGetDriverControlsQCOM;
    glad_debug_glGetError = glad_debug_impl_glGetError;
    glad_debug_glGetFenceivNV = glad_debug_impl_glGetFenceivNV;
    glad_debug_glGetFirstPerfQueryIdINTEL = glad_debug_impl_glGetFirstPerfQueryIdINTEL;
    glad_debug_glGetFloati_vNV = glad_debug_impl_glGetFloati_vNV;
    glad_debug_glGetFloati_vOES = glad_debug_impl_glGetFloati_vOES;
    glad_debug_glGetFloatv = glad_debug_impl_glGetFloatv;
    glad_debug_glGetFragDataIndexEXT = glad_debug_impl_glGetFragDataIndexEXT;
    glad_debug_glGetFragDataLocation = glad_debug_impl_glGetFragDataLocation;
    glad_debug_glGetFragmentShadingRatesEXT = glad_debug_impl_glGetFragmentShadingRatesEXT;
    glad_debug_glGetFramebufferAttachmentParameteriv = glad_debug_impl_glGetFramebufferAttachmentParameteriv;
    glad_debug_glGetFramebufferParameteriv = glad_debug_impl_glGetFramebufferParameteriv;
    glad_debug_glGetFramebufferParameterivMESA = glad_debug_impl_glGetFramebufferParameterivMESA;
    glad_debug_glGetFramebufferPixelLocalStorageSizeEXT = glad_debug_impl_glGetFramebufferPixelLocalStorageSizeEXT;
    glad_debug_glGetGraphicsResetStatus = glad_debug_impl_glGetGraphicsResetStatus;
    glad_debug_glGetGraphicsResetStatusEXT = glad_debug_impl_glGetGraphicsResetStatusEXT;
    glad_debug_glGetGraphicsResetStatusKHR = glad_debug_impl_glGetGraphicsResetStatusKHR;
    glad_debug_glGetImageHandleNV = glad_debug_impl_glGetImageHandleNV;
    glad_debug_glGetInteger64i_v = glad_debug_impl_glGetInteger64i_v;
    glad_debug_glGetInteger64v = glad_debug_impl_glGetInteger64v;
    glad_debug_glGetInteger64vAPPLE = glad_debug_impl_glGetInteger64vAPPLE;
    glad_debug_glGetInteger64vEXT = glad_debug_impl_glGetInteger64vEXT;
    glad_debug_glGetIntegeri_v = glad_debug_impl_glGetIntegeri_v;
    glad_debug_glGetIntegeri_vEXT = glad_debug_impl_glGetIntegeri_vEXT;
    glad_debug_glGetIntegerv = glad_debug_impl_glGetIntegerv;
    glad_debug_glGetInternalformatSampleivNV = glad_debug_impl_glGetInternalformatSampleivNV;
    glad_debug_glGetInternalformativ = glad_debug_impl_glGetInternalformativ;
    glad_debug_glGetMemoryObjectDetachedResourcesuivNV = glad_debug_impl_glGetMemoryObjectDetachedResourcesuivNV;
    glad_debug_glGetMemoryObjectParameterivEXT = glad_debug_impl_glGetMemoryObjectParameterivEXT;
    glad_debug_glGetMultisamplefv = glad_debug_impl_glGetMultisamplefv;
    glad_debug_glGetNextPerfQueryIdINTEL = glad_debug_impl_glGetNextPerfQueryIdINTEL;
    glad_debug_glGetObjectLabel = glad_debug_impl_glGetObjectLabel;
    glad_debug_glGetObjectLabelEXT = glad_debug_impl_glGetObjectLabelEXT;
    glad_debug_glGetObjectLabelKHR = glad_debug_impl_glGetObjectLabelKHR;
    glad_debug_glGetObjectPtrLabel = glad_debug_impl_glGetObjectPtrLabel;
    glad_debug_glGetObjectPtrLabelKHR = glad_debug_impl_glGetObjectPtrLabelKHR;
    glad_debug_glGetPathCommandsNV = glad_debug_impl_glGetPathCommandsNV;
    glad_debug_glGetPathCoordsNV = glad_debug_impl_glGetPathCoordsNV;
    glad_debug_glGetPathDashArrayNV = glad_debug_impl_glGetPathDashArrayNV;
    glad_debug_glGetPathLengthNV = glad_debug_impl_glGetPathLengthNV;
    glad_debug_glGetPathMetricRangeNV = glad_debug_impl_glGetPathMetricRangeNV;
    glad_debug_glGetPathMetricsNV = glad_debug_impl_glGetPathMetricsNV;
    glad_debug_glGetPathParameterfvNV = glad_debug_impl_glGetPathParameterfvNV;
    glad_debug_glGetPathParameterivNV = glad_debug_impl_glGetPathParameterivNV;
    glad_debug_glGetPathSpacingNV = glad_debug_impl_glGetPathSpacingNV;
    glad_debug_glGetPerfCounterInfoINTEL = glad_debug_impl_glGetPerfCounterInfoINTEL;
    glad_debug_glGetPerfMonitorCounterDataAMD = glad_debug_impl_glGetPerfMonitorCounterDataAMD;
    glad_debug_glGetPerfMonitorCounterInfoAMD = glad_debug_impl_glGetPerfMonitorCounterInfoAMD;
    glad_debug_glGetPerfMonitorCounterStringAMD = glad_debug_impl_glGetPerfMonitorCounterStringAMD;
    glad_debug_glGetPerfMonitorCountersAMD = glad_debug_impl_glGetPerfMonitorCountersAMD;
    glad_debug_glGetPerfMonitorGroupStringAMD = glad_debug_impl_glGetPerfMonitorGroupStringAMD;
    glad_debug_glGetPerfMonitorGroupsAMD = glad_debug_impl_glGetPerfMonitorGroupsAMD;
    glad_debug_glGetPerfQueryDataINTEL = glad_debug_impl_glGetPerfQueryDataINTEL;
    glad_debug_glGetPerfQueryIdByNameINTEL = glad_debug_impl_glGetPerfQueryIdByNameINTEL;
    glad_debug_glGetPerfQueryInfoINTEL = glad_debug_impl_glGetPerfQueryInfoINTEL;
    glad_debug_glGetPointerv = glad_debug_impl_glGetPointerv;
    glad_debug_glGetPointervKHR = glad_debug_impl_glGetPointervKHR;
    glad_debug_glGetProgramBinary = glad_debug_impl_glGetProgramBinary;
    glad_debug_glGetProgramBinaryOES = glad_debug_impl_glGetProgramBinaryOES;
    glad_debug_glGetProgramInfoLog = glad_debug_impl_glGetProgramInfoLog;
    glad_debug_glGetProgramInterfaceiv = glad_debug_impl_glGetProgramInterfaceiv;
    glad_debug_glGetProgramPipelineInfoLog = glad_debug_impl_glGetProgramPipelineInfoLog;
    glad_debug_glGetProgramPipelineInfoLogEXT = glad_debug_impl_glGetProgramPipelineInfoLogEXT;
    glad_debug_glGetProgramPipelineiv = glad_debug_impl_glGetProgramPipelineiv;
    glad_debug_glGetProgramPipelineivEXT = glad_debug_impl_glGetProgramPipelineivEXT;
    glad_debug_glGetProgramResourceIndex = glad_debug_impl_glGetProgramResourceIndex;
    glad_debug_glGetProgramResourceLocation = glad_debug_impl_glGetProgramResourceLocation;
    glad_debug_glGetProgramResourceLocationIndexEXT = glad_debug_impl_glGetProgramResourceLocationIndexEXT;
    glad_debug_glGetProgramResourceName = glad_debug_impl_glGetProgramResourceName;
    glad_debug_glGetProgramResourcefvNV = glad_debug_impl_glGetProgramResourcefvNV;
    glad_debug_glGetProgramResourceiv = glad_debug_impl_glGetProgramResourceiv;
    glad_debug_glGetProgramiv = glad_debug_impl_glGetProgramiv;
    glad_debug_glGetQueryObjecti64vEXT = glad_debug_impl_glGetQueryObjecti64vEXT;
    glad_debug_glGetQueryObjectivEXT = glad_debug_impl_glGetQueryObjectivEXT;
    glad_debug_glGetQueryObjectui64vEXT = glad_debug_impl_glGetQueryObjectui64vEXT;
    glad_debug_glGetQueryObjectuiv = glad_debug_impl_glGetQueryObjectuiv;
    glad_debug_glGetQueryObjectuivEXT = glad_debug_impl_glGetQueryObjectuivEXT;
    glad_debug_glGetQueryiv = glad_debug_impl_glGetQueryiv;
    glad_debug_glGetQueryivEXT = glad_debug_impl_glGetQueryivEXT;
    glad_debug_glGetRenderbufferParameteriv = glad_debug_impl_glGetRenderbufferParameteriv;
    glad_debug_glGetSamplerParameterIiv = glad_debug_impl_glGetSamplerParameterIiv;
    glad_debug_glGetSamplerParameterIivEXT = glad_debug_impl_glGetSamplerParameterIivEXT;
    glad_debug_glGetSamplerParameterIivOES = glad_debug_impl_glGetSamplerParameterIivOES;
    glad_debug_glGetSamplerParameterIuiv = glad_debug_impl_glGetSamplerParameterIuiv;
    glad_debug_glGetSamplerParameterIuivEXT = glad_debug_impl_glGetSamplerParameterIuivEXT;
    glad_debug_glGetSamplerParameterIuivOES = glad_debug_impl_glGetSamplerParameterIuivOES;
    glad_debug_glGetSamplerParameterfv = glad_debug_impl_glGetSamplerParameterfv;
    glad_debug_glGetSamplerParameteriv = glad_debug_impl_glGetSamplerParameteriv;
    glad_debug_glGetSemaphoreParameterivNV = glad_debug_impl_glGetSemaphoreParameterivNV;
    glad_debug_glGetSemaphoreParameterui64vEXT = glad_debug_impl_glGetSemaphoreParameterui64vEXT;
    glad_debug_glGetShaderInfoLog = glad_debug_impl_glGetShaderInfoLog;
    glad_debug_glGetShaderPrecisionFormat = glad_debug_impl_glGetShaderPrecisionFormat;
    glad_debug_glGetShaderSource = glad_debug_impl_glGetShaderSource;
    glad_debug_glGetShaderiv = glad_debug_impl_glGetShaderiv;
    glad_debug_glGetShadingRateImagePaletteNV = glad_debug_impl_glGetShadingRateImagePaletteNV;
    glad_debug_glGetShadingRateSampleLocationivNV = glad_debug_impl_glGetShadingRateSampleLocationivNV;
    glad_debug_glGetString = glad_debug_impl_glGetString;
    glad_debug_glGetStringi = glad_debug_impl_glGetStringi;
    glad_debug_glGetSynciv = glad_debug_impl_glGetSynciv;
    glad_debug_glGetSyncivAPPLE = glad_debug_impl_glGetSyncivAPPLE;
    glad_debug_glGetTexLevelParameterfv = glad_debug_impl_glGetTexLevelParameterfv;
    glad_debug_glGetTexLevelParameteriv = glad_debug_impl_glGetTexLevelParameteriv;
    glad_debug_glGetTexParameterIiv = glad_debug_impl_glGetTexParameterIiv;
    glad_debug_glGetTexParameterIivEXT = glad_debug_impl_glGetTexParameterIivEXT;
    glad_debug_glGetTexParameterIivOES = glad_debug_impl_glGetTexParameterIivOES;
    glad_debug_glGetTexParameterIuiv = glad_debug_impl_glGetTexParameterIuiv;
    glad_debug_glGetTexParameterIuivEXT = glad_debug_impl_glGetTexParameterIuivEXT;
    glad_debug_glGetTexParameterIuivOES = glad_debug_impl_glGetTexParameterIuivOES;
    glad_debug_glGetTexParameterfv = glad_debug_impl_glGetTexParameterfv;
    glad_debug_glGetTexParameteriv = glad_debug_impl_glGetTexParameteriv;
    glad_debug_glGetTextureHandleIMG = glad_debug_impl_glGetTextureHandleIMG;
    glad_debug_glGetTextureHandleNV = glad_debug_impl_glGetTextureHandleNV;
    glad_debug_glGetTextureSamplerHandleIMG = glad_debug_impl_glGetTextureSamplerHandleIMG;
    glad_debug_glGetTextureSamplerHandleNV = glad_debug_impl_glGetTextureSamplerHandleNV;
    glad_debug_glGetTransformFeedbackVarying = glad_debug_impl_glGetTransformFeedbackVarying;
    glad_debug_glGetTranslatedShaderSourceANGLE = glad_debug_impl_glGetTranslatedShaderSourceANGLE;
    glad_debug_glGetUniformBlockIndex = glad_debug_impl_glGetUniformBlockIndex;
    glad_debug_glGetUniformIndices = glad_debug_impl_glGetUniformIndices;
    glad_debug_glGetUniformLocation = glad_debug_impl_glGetUniformLocation;
    glad_debug_glGetUniformfv = glad_debug_impl_glGetUniformfv;
    glad_debug_glGetUniformi64vNV = glad_debug_impl_glGetUniformi64vNV;
    glad_debug_glGetUniformiv = glad_debug_impl_glGetUniformiv;
    glad_debug_glGetUniformuiv = glad_debug_impl_glGetUniformuiv;
    glad_debug_glGetUnsignedBytei_vEXT = glad_debug_impl_glGetUnsignedBytei_vEXT;
    glad_debug_glGetUnsignedBytevEXT = glad_debug_impl_glGetUnsignedBytevEXT;
    glad_debug_glGetVertexAttribIiv = glad_debug_impl_glGetVertexAttribIiv;
    glad_debug_glGetVertexAttribIuiv = glad_debug_impl_glGetVertexAttribIuiv;
    glad_debug_glGetVertexAttribPointerv = glad_debug_impl_glGetVertexAttribPointerv;
    glad_debug_glGetVertexAttribfv = glad_debug_impl_glGetVertexAttribfv;
    glad_debug_glGetVertexAttribiv = glad_debug_impl_glGetVertexAttribiv;
    glad_debug_glGetVkProcAddrNV = glad_debug_impl_glGetVkProcAddrNV;
    glad_debug_glGetnUniformfv = glad_debug_impl_glGetnUniformfv;
    glad_debug_glGetnUniformfvEXT = glad_debug_impl_glGetnUniformfvEXT;
    glad_debug_glGetnUniformfvKHR = glad_debug_impl_glGetnUniformfvKHR;
    glad_debug_glGetnUniformiv = glad_debug_impl_glGetnUniformiv;
    glad_debug_glGetnUniformivEXT = glad_debug_impl_glGetnUniformivEXT;
    glad_debug_glGetnUniformivKHR = glad_debug_impl_glGetnUniformivKHR;
    glad_debug_glGetnUniformuiv = glad_debug_impl_glGetnUniformuiv;
    glad_debug_glGetnUniformuivKHR = glad_debug_impl_glGetnUniformuivKHR;
    glad_debug_glHint = glad_debug_impl_glHint;
    glad_debug_glImportMemoryFdEXT = glad_debug_impl_glImportMemoryFdEXT;
    glad_debug_glImportMemoryWin32HandleEXT = glad_debug_impl_glImportMemoryWin32HandleEXT;
    glad_debug_glImportMemoryWin32NameEXT = glad_debug_impl_glImportMemoryWin32NameEXT;
    glad_debug_glImportSemaphoreFdEXT = glad_debug_impl_glImportSemaphoreFdEXT;
    glad_debug_glImportSemaphoreWin32HandleEXT = glad_debug_impl_glImportSemaphoreWin32HandleEXT;
    glad_debug_glImportSemaphoreWin32NameEXT = glad_debug_impl_glImportSemaphoreWin32NameEXT;
    glad_debug_glInsertEventMarkerEXT = glad_debug_impl_glInsertEventMarkerEXT;
    glad_debug_glInterpolatePathsNV = glad_debug_impl_glInterpolatePathsNV;
    glad_debug_glInvalidateFramebuffer = glad_debug_impl_glInvalidateFramebuffer;
    glad_debug_glInvalidateSubFramebuffer = glad_debug_impl_glInvalidateSubFramebuffer;
    glad_debug_glIsBuffer = glad_debug_impl_glIsBuffer;
    glad_debug_glIsEnabled = glad_debug_impl_glIsEnabled;
    glad_debug_glIsEnabledi = glad_debug_impl_glIsEnabledi;
    glad_debug_glIsEnablediEXT = glad_debug_impl_glIsEnablediEXT;
    glad_debug_glIsEnablediNV = glad_debug_impl_glIsEnablediNV;
    glad_debug_glIsEnablediOES = glad_debug_impl_glIsEnablediOES;
    glad_debug_glIsFenceNV = glad_debug_impl_glIsFenceNV;
    glad_debug_glIsFramebuffer = glad_debug_impl_glIsFramebuffer;
    glad_debug_glIsImageHandleResidentNV = glad_debug_impl_glIsImageHandleResidentNV;
    glad_debug_glIsMemoryObjectEXT = glad_debug_impl_glIsMemoryObjectEXT;
    glad_debug_glIsPathNV = glad_debug_impl_glIsPathNV;
    glad_debug_glIsPointInFillPathNV = glad_debug_impl_glIsPointInFillPathNV;
    glad_debug_glIsPointInStrokePathNV = glad_debug_impl_glIsPointInStrokePathNV;
    glad_debug_glIsProgram = glad_debug_impl_glIsProgram;
    glad_debug_glIsProgramPipeline = glad_debug_impl_glIsProgramPipeline;
    glad_debug_glIsProgramPipelineEXT = glad_debug_impl_glIsProgramPipelineEXT;
    glad_debug_glIsQuery = glad_debug_impl_glIsQuery;
    glad_debug_glIsQueryEXT = glad_debug_impl_glIsQueryEXT;
    glad_debug_glIsRenderbuffer = glad_debug_impl_glIsRenderbuffer;
    glad_debug_glIsSampler = glad_debug_impl_glIsSampler;
    glad_debug_glIsSemaphoreEXT = glad_debug_impl_glIsSemaphoreEXT;
    glad_debug_glIsShader = glad_debug_impl_glIsShader;
    glad_debug_glIsSync = glad_debug_impl_glIsSync;
    glad_debug_glIsSyncAPPLE = glad_debug_impl_glIsSyncAPPLE;
    glad_debug_glIsTexture = glad_debug_impl_glIsTexture;
    glad_debug_glIsTextureHandleResidentNV = glad_debug_impl_glIsTextureHandleResidentNV;
    glad_debug_glIsTransformFeedback = glad_debug_impl_glIsTransformFeedback;
    glad_debug_glIsVertexArray = glad_debug_impl_glIsVertexArray;
    glad_debug_glIsVertexArrayOES = glad_debug_impl_glIsVertexArrayOES;
    glad_debug_glLabelObjectEXT = glad_debug_impl_glLabelObjectEXT;
    glad_debug_glLineWidth = glad_debug_impl_glLineWidth;
    glad_debug_glLinkProgram = glad_debug_impl_glLinkProgram;
    glad_debug_glMakeImageHandleNonResidentNV = glad_debug_impl_glMakeImageHandleNonResidentNV;
    glad_debug_glMakeImageHandleResidentNV = glad_debug_impl_glMakeImageHandleResidentNV;
    glad_debug_glMakeTextureHandleNonResidentNV = glad_debug_impl_glMakeTextureHandleNonResidentNV;
    glad_debug_glMakeTextureHandleResidentNV = glad_debug_impl_glMakeTextureHandleResidentNV;
    glad_debug_glMapBufferOES = glad_debug_impl_glMapBufferOES;
    glad_debug_glMapBufferRange = glad_debug_impl_glMapBufferRange;
    glad_debug_glMapBufferRangeEXT = glad_debug_impl_glMapBufferRangeEXT;
    glad_debug_glMatrixFrustumEXT = glad_debug_impl_glMatrixFrustumEXT;
    glad_debug_glMatrixLoad3x2fNV = glad_debug_impl_glMatrixLoad3x2fNV;
    glad_debug_glMatrixLoad3x3fNV = glad_debug_impl_glMatrixLoad3x3fNV;
    glad_debug_glMatrixLoadIdentityEXT = glad_debug_impl_glMatrixLoadIdentityEXT;
    glad_debug_glMatrixLoadTranspose3x3fNV = glad_debug_impl_glMatrixLoadTranspose3x3fNV;
    glad_debug_glMatrixLoadTransposedEXT = glad_debug_impl_glMatrixLoadTransposedEXT;
    glad_debug_glMatrixLoadTransposefEXT = glad_debug_impl_glMatrixLoadTransposefEXT;
    glad_debug_glMatrixLoaddEXT = glad_debug_impl_glMatrixLoaddEXT;
    glad_debug_glMatrixLoadfEXT = glad_debug_impl_glMatrixLoadfEXT;
    glad_debug_glMatrixMult3x2fNV = glad_debug_impl_glMatrixMult3x2fNV;
    glad_debug_glMatrixMult3x3fNV = glad_debug_impl_glMatrixMult3x3fNV;
    glad_debug_glMatrixMultTranspose3x3fNV = glad_debug_impl_glMatrixMultTranspose3x3fNV;
    glad_debug_glMatrixMultTransposedEXT = glad_debug_impl_glMatrixMultTransposedEXT;
    glad_debug_glMatrixMultTransposefEXT = glad_debug_impl_glMatrixMultTransposefEXT;
    glad_debug_glMatrixMultdEXT = glad_debug_impl_glMatrixMultdEXT;
    glad_debug_glMatrixMultfEXT = glad_debug_impl_glMatrixMultfEXT;
    glad_debug_glMatrixOrthoEXT = glad_debug_impl_glMatrixOrthoEXT;
    glad_debug_glMatrixPopEXT = glad_debug_impl_glMatrixPopEXT;
    glad_debug_glMatrixPushEXT = glad_debug_impl_glMatrixPushEXT;
    glad_debug_glMatrixRotatedEXT = glad_debug_impl_glMatrixRotatedEXT;
    glad_debug_glMatrixRotatefEXT = glad_debug_impl_glMatrixRotatefEXT;
    glad_debug_glMatrixScaledEXT = glad_debug_impl_glMatrixScaledEXT;
    glad_debug_glMatrixScalefEXT = glad_debug_impl_glMatrixScalefEXT;
    glad_debug_glMatrixTranslatedEXT = glad_debug_impl_glMatrixTranslatedEXT;
    glad_debug_glMatrixTranslatefEXT = glad_debug_impl_glMatrixTranslatefEXT;
    glad_debug_glMaxActiveShaderCoresARM = glad_debug_impl_glMaxActiveShaderCoresARM;
    glad_debug_glMaxShaderCompilerThreadsKHR = glad_debug_impl_glMaxShaderCompilerThreadsKHR;
    glad_debug_glMemoryBarrier = glad_debug_impl_glMemoryBarrier;
    glad_debug_glMemoryBarrierByRegion = glad_debug_impl_glMemoryBarrierByRegion;
    glad_debug_glMemoryObjectParameterivEXT = glad_debug_impl_glMemoryObjectParameterivEXT;
    glad_debug_glMinSampleShading = glad_debug_impl_glMinSampleShading;
    glad_debug_glMinSampleShadingOES = glad_debug_impl_glMinSampleShadingOES;
    glad_debug_glMultiDrawArraysEXT = glad_debug_impl_glMultiDrawArraysEXT;
    glad_debug_glMultiDrawArraysIndirectEXT = glad_debug_impl_glMultiDrawArraysIndirectEXT;
    glad_debug_glMultiDrawElementsBaseVertexEXT = glad_debug_impl_glMultiDrawElementsBaseVertexEXT;
    glad_debug_glMultiDrawElementsEXT = glad_debug_impl_glMultiDrawElementsEXT;
    glad_debug_glMultiDrawElementsIndirectEXT = glad_debug_impl_glMultiDrawElementsIndirectEXT;
    glad_debug_glMultiDrawMeshTasksIndirectCountEXT = glad_debug_impl_glMultiDrawMeshTasksIndirectCountEXT;
    glad_debug_glMultiDrawMeshTasksIndirectCountNV = glad_debug_impl_glMultiDrawMeshTasksIndirectCountNV;
    glad_debug_glMultiDrawMeshTasksIndirectEXT = glad_debug_impl_glMultiDrawMeshTasksIndirectEXT;
    glad_debug_glMultiDrawMeshTasksIndirectNV = glad_debug_impl_glMultiDrawMeshTasksIndirectNV;
    glad_debug_glNamedBufferAttachMemoryNV = glad_debug_impl_glNamedBufferAttachMemoryNV;
    glad_debug_glNamedBufferPageCommitmentMemNV = glad_debug_impl_glNamedBufferPageCommitmentMemNV;
    glad_debug_glNamedBufferStorageExternalEXT = glad_debug_impl_glNamedBufferStorageExternalEXT;
    glad_debug_glNamedBufferStorageMemEXT = glad_debug_impl_glNamedBufferStorageMemEXT;
    glad_debug_glNamedFramebufferSampleLocationsfvNV = glad_debug_impl_glNamedFramebufferSampleLocationsfvNV;
    glad_debug_glNamedFramebufferTextureMultiviewOVR = glad_debug_impl_glNamedFramebufferTextureMultiviewOVR;
    glad_debug_glNamedRenderbufferStorageMultisampleAdvancedAMD = glad_debug_impl_glNamedRenderbufferStorageMultisampleAdvancedAMD;
    glad_debug_glObjectLabel = glad_debug_impl_glObjectLabel;
    glad_debug_glObjectLabelKHR = glad_debug_impl_glObjectLabelKHR;
    glad_debug_glObjectPtrLabel = glad_debug_impl_glObjectPtrLabel;
    glad_debug_glObjectPtrLabelKHR = glad_debug_impl_glObjectPtrLabelKHR;
    glad_debug_glPatchParameteri = glad_debug_impl_glPatchParameteri;
    glad_debug_glPatchParameteriEXT = glad_debug_impl_glPatchParameteriEXT;
    glad_debug_glPatchParameteriOES = glad_debug_impl_glPatchParameteriOES;
    glad_debug_glPathCommandsNV = glad_debug_impl_glPathCommandsNV;
    glad_debug_glPathCoordsNV = glad_debug_impl_glPathCoordsNV;
    glad_debug_glPathCoverDepthFuncNV = glad_debug_impl_glPathCoverDepthFuncNV;
    glad_debug_glPathDashArrayNV = glad_debug_impl_glPathDashArrayNV;
    glad_debug_glPathGlyphIndexArrayNV = glad_debug_impl_glPathGlyphIndexArrayNV;
    glad_debug_glPathGlyphIndexRangeNV = glad_debug_impl_glPathGlyphIndexRangeNV;
    glad_debug_glPathGlyphRangeNV = glad_debug_impl_glPathGlyphRangeNV;
    glad_debug_glPathGlyphsNV = glad_debug_impl_glPathGlyphsNV;
    glad_debug_glPathMemoryGlyphIndexArrayNV = glad_debug_impl_glPathMemoryGlyphIndexArrayNV;
    glad_debug_glPathParameterfNV = glad_debug_impl_glPathParameterfNV;
    glad_debug_glPathParameterfvNV = glad_debug_impl_glPathParameterfvNV;
    glad_debug_glPathParameteriNV = glad_debug_impl_glPathParameteriNV;
    glad_debug_glPathParameterivNV = glad_debug_impl_glPathParameterivNV;
    glad_debug_glPathStencilDepthOffsetNV = glad_debug_impl_glPathStencilDepthOffsetNV;
    glad_debug_glPathStencilFuncNV = glad_debug_impl_glPathStencilFuncNV;
    glad_debug_glPathStringNV = glad_debug_impl_glPathStringNV;
    glad_debug_glPathSubCommandsNV = glad_debug_impl_glPathSubCommandsNV;
    glad_debug_glPathSubCoordsNV = glad_debug_impl_glPathSubCoordsNV;
    glad_debug_glPauseTransformFeedback = glad_debug_impl_glPauseTransformFeedback;
    glad_debug_glPixelStorei = glad_debug_impl_glPixelStorei;
    glad_debug_glPointAlongPathNV = glad_debug_impl_glPointAlongPathNV;
    glad_debug_glPolygonModeNV = glad_debug_impl_glPolygonModeNV;
    glad_debug_glPolygonOffset = glad_debug_impl_glPolygonOffset;
    glad_debug_glPolygonOffsetClampEXT = glad_debug_impl_glPolygonOffsetClampEXT;
    glad_debug_glPopDebugGroup = glad_debug_impl_glPopDebugGroup;
    glad_debug_glPopDebugGroupKHR = glad_debug_impl_glPopDebugGroupKHR;
    glad_debug_glPopGroupMarkerEXT = glad_debug_impl_glPopGroupMarkerEXT;
    glad_debug_glPrimitiveBoundingBox = glad_debug_impl_glPrimitiveBoundingBox;
    glad_debug_glPrimitiveBoundingBoxEXT = glad_debug_impl_glPrimitiveBoundingBoxEXT;
    glad_debug_glPrimitiveBoundingBoxOES = glad_debug_impl_glPrimitiveBoundingBoxOES;
    glad_debug_glProgramBinary = glad_debug_impl_glProgramBinary;
    glad_debug_glProgramBinaryOES = glad_debug_impl_glProgramBinaryOES;
    glad_debug_glProgramParameteri = glad_debug_impl_glProgramParameteri;
    glad_debug_glProgramParameteriEXT = glad_debug_impl_glProgramParameteriEXT;
    glad_debug_glProgramPathFragmentInputGenNV = glad_debug_impl_glProgramPathFragmentInputGenNV;
    glad_debug_glProgramUniform1f = glad_debug_impl_glProgramUniform1f;
    glad_debug_glProgramUniform1fEXT = glad_debug_impl_glProgramUniform1fEXT;
    glad_debug_glProgramUniform1fv = glad_debug_impl_glProgramUniform1fv;
    glad_debug_glProgramUniform1fvEXT = glad_debug_impl_glProgramUniform1fvEXT;
    glad_debug_glProgramUniform1i = glad_debug_impl_glProgramUniform1i;
    glad_debug_glProgramUniform1i64NV = glad_debug_impl_glProgramUniform1i64NV;
    glad_debug_glProgramUniform1i64vNV = glad_debug_impl_glProgramUniform1i64vNV;
    glad_debug_glProgramUniform1iEXT = glad_debug_impl_glProgramUniform1iEXT;
    glad_debug_glProgramUniform1iv = glad_debug_impl_glProgramUniform1iv;
    glad_debug_glProgramUniform1ivEXT = glad_debug_impl_glProgramUniform1ivEXT;
    glad_debug_glProgramUniform1ui = glad_debug_impl_glProgramUniform1ui;
    glad_debug_glProgramUniform1ui64NV = glad_debug_impl_glProgramUniform1ui64NV;
    glad_debug_glProgramUniform1ui64vNV = glad_debug_impl_glProgramUniform1ui64vNV;
    glad_debug_glProgramUniform1uiEXT = glad_debug_impl_glProgramUniform1uiEXT;
    glad_debug_glProgramUniform1uiv = glad_debug_impl_glProgramUniform1uiv;
    glad_debug_glProgramUniform1uivEXT = glad_debug_impl_glProgramUniform1uivEXT;
    glad_debug_glProgramUniform2f = glad_debug_impl_glProgramUniform2f;
    glad_debug_glProgramUniform2fEXT = glad_debug_impl_glProgramUniform2fEXT;
    glad_debug_glProgramUniform2fv = glad_debug_impl_glProgramUniform2fv;
    glad_debug_glProgramUniform2fvEXT = glad_debug_impl_glProgramUniform2fvEXT;
    glad_debug_glProgramUniform2i = glad_debug_impl_glProgramUniform2i;
    glad_debug_glProgramUniform2i64NV = glad_debug_impl_glProgramUniform2i64NV;
    glad_debug_glProgramUniform2i64vNV = glad_debug_impl_glProgramUniform2i64vNV;
    glad_debug_glProgramUniform2iEXT = glad_debug_impl_glProgramUniform2iEXT;
    glad_debug_glProgramUniform2iv = glad_debug_impl_glProgramUniform2iv;
    glad_debug_glProgramUniform2ivEXT = glad_debug_impl_glProgramUniform2ivEXT;
    glad_debug_glProgramUniform2ui = glad_debug_impl_glProgramUniform2ui;
    glad_debug_glProgramUniform2ui64NV = glad_debug_impl_glProgramUniform2ui64NV;
    glad_debug_glProgramUniform2ui64vNV = glad_debug_impl_glProgramUniform2ui64vNV;
    glad_debug_glProgramUniform2uiEXT = glad_debug_impl_glProgramUniform2uiEXT;
    glad_debug_glProgramUniform2uiv = glad_debug_impl_glProgramUniform2uiv;
    glad_debug_glProgramUniform2uivEXT = glad_debug_impl_glProgramUniform2uivEXT;
    glad_debug_glProgramUniform3f = glad_debug_impl_glProgramUniform3f;
    glad_debug_glProgramUniform3fEXT = glad_debug_impl_glProgramUniform3fEXT;
    glad_debug_glProgramUniform3fv = glad_debug_impl_glProgramUniform3fv;
    glad_debug_glProgramUniform3fvEXT = glad_debug_impl_glProgramUniform3fvEXT;
    glad_debug_glProgramUniform3i = glad_debug_impl_glProgramUniform3i;
    glad_debug_glProgramUniform3i64NV = glad_debug_impl_glProgramUniform3i64NV;
    glad_debug_glProgramUniform3i64vNV = glad_debug_impl_glProgramUniform3i64vNV;
    glad_debug_glProgramUniform3iEXT = glad_debug_impl_glProgramUniform3iEXT;
    glad_debug_glProgramUniform3iv = glad_debug_impl_glProgramUniform3iv;
    glad_debug_glProgramUniform3ivEXT = glad_debug_impl_glProgramUniform3ivEXT;
    glad_debug_glProgramUniform3ui = glad_debug_impl_glProgramUniform3ui;
    glad_debug_glProgramUniform3ui64NV = glad_debug_impl_glProgramUniform3ui64NV;
    glad_debug_glProgramUniform3ui64vNV = glad_debug_impl_glProgramUniform3ui64vNV;
    glad_debug_glProgramUniform3uiEXT = glad_debug_impl_glProgramUniform3uiEXT;
    glad_debug_glProgramUniform3uiv = glad_debug_impl_glProgramUniform3uiv;
    glad_debug_glProgramUniform3uivEXT = glad_debug_impl_glProgramUniform3uivEXT;
    glad_debug_glProgramUniform4f = glad_debug_impl_glProgramUniform4f;
    glad_debug_glProgramUniform4fEXT = glad_debug_impl_glProgramUniform4fEXT;
    glad_debug_glProgramUniform4fv = glad_debug_impl_glProgramUniform4fv;
    glad_debug_glProgramUniform4fvEXT = glad_debug_impl_glProgramUniform4fvEXT;
    glad_debug_glProgramUniform4i = glad_debug_impl_glProgramUniform4i;
    glad_debug_glProgramUniform4i64NV = glad_debug_impl_glProgramUniform4i64NV;
    glad_debug_glProgramUniform4i64vNV = glad_debug_impl_glProgramUniform4i64vNV;
    glad_debug_glProgramUniform4iEXT = glad_debug_impl_glProgramUniform4iEXT;
    glad_debug_glProgramUniform4iv = glad_debug_impl_glProgramUniform4iv;
    glad_debug_glProgramUniform4ivEXT = glad_debug_impl_glProgramUniform4ivEXT;
    glad_debug_glProgramUniform4ui = glad_debug_impl_glProgramUniform4ui;
    glad_debug_glProgramUniform4ui64NV = glad_debug_impl_glProgramUniform4ui64NV;
    glad_debug_glProgramUniform4ui64vNV = glad_debug_impl_glProgramUniform4ui64vNV;
    glad_debug_glProgramUniform4uiEXT = glad_debug_impl_glProgramUniform4uiEXT;
    glad_debug_glProgramUniform4uiv = glad_debug_impl_glProgramUniform4uiv;
    glad_debug_glProgramUniform4uivEXT = glad_debug_impl_glProgramUniform4uivEXT;
    glad_debug_glProgramUniformHandleui64IMG = glad_debug_impl_glProgramUniformHandleui64IMG;
    glad_debug_glProgramUniformHandleui64NV = glad_debug_impl_glProgramUniformHandleui64NV;
    glad_debug_glProgramUniformHandleui64vIMG = glad_debug_impl_glProgramUniformHandleui64vIMG;
    glad_debug_glProgramUniformHandleui64vNV = glad_debug_impl_glProgramUniformHandleui64vNV;
    glad_debug_glProgramUniformMatrix2fv = glad_debug_impl_glProgramUniformMatrix2fv;
    glad_debug_glProgramUniformMatrix2fvEXT = glad_debug_impl_glProgramUniformMatrix2fvEXT;
    glad_debug_glProgramUniformMatrix2x3fv = glad_debug_impl_glProgramUniformMatrix2x3fv;
    glad_debug_glProgramUniformMatrix2x3fvEXT = glad_debug_impl_glProgramUniformMatrix2x3fvEXT;
    glad_debug_glProgramUniformMatrix2x4fv = glad_debug_impl_glProgramUniformMatrix2x4fv;
    glad_debug_glProgramUniformMatrix2x4fvEXT = glad_debug_impl_glProgramUniformMatrix2x4fvEXT;
    glad_debug_glProgramUniformMatrix3fv = glad_debug_impl_glProgramUniformMatrix3fv;
    glad_debug_glProgramUniformMatrix3fvEXT = glad_debug_impl_glProgramUniformMatrix3fvEXT;
    glad_debug_glProgramUniformMatrix3x2fv = glad_debug_impl_glProgramUniformMatrix3x2fv;
    glad_debug_glProgramUniformMatrix3x2fvEXT = glad_debug_impl_glProgramUniformMatrix3x2fvEXT;
    glad_debug_glProgramUniformMatrix3x4fv = glad_debug_impl_glProgramUniformMatrix3x4fv;
    glad_debug_glProgramUniformMatrix3x4fvEXT = glad_debug_impl_glProgramUniformMatrix3x4fvEXT;
    glad_debug_glProgramUniformMatrix4fv = glad_debug_impl_glProgramUniformMatrix4fv;
    glad_debug_glProgramUniformMatrix4fvEXT = glad_debug_impl_glProgramUniformMatrix4fvEXT;
    glad_debug_glProgramUniformMatrix4x2fv = glad_debug_impl_glProgramUniformMatrix4x2fv;
    glad_debug_glProgramUniformMatrix4x2fvEXT = glad_debug_impl_glProgramUniformMatrix4x2fvEXT;
    glad_debug_glProgramUniformMatrix4x3fv = glad_debug_impl_glProgramUniformMatrix4x3fv;
    glad_debug_glProgramUniformMatrix4x3fvEXT = glad_debug_impl_glProgramUniformMatrix4x3fvEXT;
    glad_debug_glPushDebugGroup = glad_debug_impl_glPushDebugGroup;
    glad_debug_glPushDebugGroupKHR = glad_debug_impl_glPushDebugGroupKHR;
    glad_debug_glPushGroupMarkerEXT = glad_debug_impl_glPushGroupMarkerEXT;
    glad_debug_glQueryCounterEXT = glad_debug_impl_glQueryCounterEXT;
    glad_debug_glRasterSamplesEXT = glad_debug_impl_glRasterSamplesEXT;
    glad_debug_glReadBuffer = glad_debug_impl_glReadBuffer;
    glad_debug_glReadBufferIndexedEXT = glad_debug_impl_glReadBufferIndexedEXT;
    glad_debug_glReadBufferNV = glad_debug_impl_glReadBufferNV;
    glad_debug_glReadPixels = glad_debug_impl_glReadPixels;
    glad_debug_glReadnPixels = glad_debug_impl_glReadnPixels;
    glad_debug_glReadnPixelsEXT = glad_debug_impl_glReadnPixelsEXT;
    glad_debug_glReadnPixelsKHR = glad_debug_impl_glReadnPixelsKHR;
    glad_debug_glReleaseKeyedMutexWin32EXT = glad_debug_impl_glReleaseKeyedMutexWin32EXT;
    glad_debug_glReleaseShaderCompiler = glad_debug_impl_glReleaseShaderCompiler;
    glad_debug_glRenderbufferStorage = glad_debug_impl_glRenderbufferStorage;
    glad_debug_glRenderbufferStorageMultisample = glad_debug_impl_glRenderbufferStorageMultisample;
    glad_debug_glRenderbufferStorageMultisampleANGLE = glad_debug_impl_glRenderbufferStorageMultisampleANGLE;
    glad_debug_glRenderbufferStorageMultisampleAPPLE = glad_debug_impl_glRenderbufferStorageMultisampleAPPLE;
    glad_debug_glRenderbufferStorageMultisampleAdvancedAMD = glad_debug_impl_glRenderbufferStorageMultisampleAdvancedAMD;
    glad_debug_glRenderbufferStorageMultisampleEXT = glad_debug_impl_glRenderbufferStorageMultisampleEXT;
    glad_debug_glRenderbufferStorageMultisampleIMG = glad_debug_impl_glRenderbufferStorageMultisampleIMG;
    glad_debug_glRenderbufferStorageMultisampleNV = glad_debug_impl_glRenderbufferStorageMultisampleNV;
    glad_debug_glResetMemoryObjectParameterNV = glad_debug_impl_glResetMemoryObjectParameterNV;
    glad_debug_glResolveDepthValuesNV = glad_debug_impl_glResolveDepthValuesNV;
    glad_debug_glResolveMultisampleFramebufferAPPLE = glad_debug_impl_glResolveMultisampleFramebufferAPPLE;
    glad_debug_glResumeTransformFeedback = glad_debug_impl_glResumeTransformFeedback;
    glad_debug_glSampleCoverage = glad_debug_impl_glSampleCoverage;
    glad_debug_glSampleMaski = glad_debug_impl_glSampleMaski;
    glad_debug_glSamplerParameterIiv = glad_debug_impl_glSamplerParameterIiv;
    glad_debug_glSamplerParameterIivEXT = glad_debug_impl_glSamplerParameterIivEXT;
    glad_debug_glSamplerParameterIivOES = glad_debug_impl_glSamplerParameterIivOES;
    glad_debug_glSamplerParameterIuiv = glad_debug_impl_glSamplerParameterIuiv;
    glad_debug_glSamplerParameterIuivEXT = glad_debug_impl_glSamplerParameterIuivEXT;
    glad_debug_glSamplerParameterIuivOES = glad_debug_impl_glSamplerParameterIuivOES;
    glad_debug_glSamplerParameterf = glad_debug_impl_glSamplerParameterf;
    glad_debug_glSamplerParameterfv = glad_debug_impl_glSamplerParameterfv;
    glad_debug_glSamplerParameteri = glad_debug_impl_glSamplerParameteri;
    glad_debug_glSamplerParameteriv = glad_debug_impl_glSamplerParameteriv;
    glad_debug_glScissor = glad_debug_impl_glScissor;
    glad_debug_glScissorArrayvNV = glad_debug_impl_glScissorArrayvNV;
    glad_debug_glScissorArrayvOES = glad_debug_impl_glScissorArrayvOES;
    glad_debug_glScissorExclusiveArrayvNV = glad_debug_impl_glScissorExclusiveArrayvNV;
    glad_debug_glScissorExclusiveNV = glad_debug_impl_glScissorExclusiveNV;
    glad_debug_glScissorIndexedNV = glad_debug_impl_glScissorIndexedNV;
    glad_debug_glScissorIndexedOES = glad_debug_impl_glScissorIndexedOES;
    glad_debug_glScissorIndexedvNV = glad_debug_impl_glScissorIndexedvNV;
    glad_debug_glScissorIndexedvOES = glad_debug_impl_glScissorIndexedvOES;
    glad_debug_glSelectPerfMonitorCountersAMD = glad_debug_impl_glSelectPerfMonitorCountersAMD;
    glad_debug_glSemaphoreParameterivNV = glad_debug_impl_glSemaphoreParameterivNV;
    glad_debug_glSemaphoreParameterui64vEXT = glad_debug_impl_glSemaphoreParameterui64vEXT;
    glad_debug_glSetFenceNV = glad_debug_impl_glSetFenceNV;
    glad_debug_glShaderBinary = glad_debug_impl_glShaderBinary;
    glad_debug_glShaderSource = glad_debug_impl_glShaderSource;
    glad_debug_glShadingRateCombinerOpsEXT = glad_debug_impl_glShadingRateCombinerOpsEXT;
    glad_debug_glShadingRateEXT = glad_debug_impl_glShadingRateEXT;
    glad_debug_glShadingRateImageBarrierNV = glad_debug_impl_glShadingRateImageBarrierNV;
    glad_debug_glShadingRateImagePaletteNV = glad_debug_impl_glShadingRateImagePaletteNV;
    glad_debug_glShadingRateQCOM = glad_debug_impl_glShadingRateQCOM;
    glad_debug_glShadingRateSampleOrderCustomNV = glad_debug_impl_glShadingRateSampleOrderCustomNV;
    glad_debug_glShadingRateSampleOrderNV = glad_debug_impl_glShadingRateSampleOrderNV;
    glad_debug_glSignalSemaphoreEXT = glad_debug_impl_glSignalSemaphoreEXT;
    glad_debug_glSignalVkFenceNV = glad_debug_impl_glSignalVkFenceNV;
    glad_debug_glSignalVkSemaphoreNV = glad_debug_impl_glSignalVkSemaphoreNV;
    glad_debug_glStartTilingQCOM = glad_debug_impl_glStartTilingQCOM;
    glad_debug_glStencilFillPathInstancedNV = glad_debug_impl_glStencilFillPathInstancedNV;
    glad_debug_glStencilFillPathNV = glad_debug_impl_glStencilFillPathNV;
    glad_debug_glStencilFunc = glad_debug_impl_glStencilFunc;
    glad_debug_glStencilFuncSeparate = glad_debug_impl_glStencilFuncSeparate;
    glad_debug_glStencilMask = glad_debug_impl_glStencilMask;
    glad_debug_glStencilMaskSeparate = glad_debug_impl_glStencilMaskSeparate;
    glad_debug_glStencilOp = glad_debug_impl_glStencilOp;
    glad_debug_glStencilOpSeparate = glad_debug_impl_glStencilOpSeparate;
    glad_debug_glStencilStrokePathInstancedNV = glad_debug_impl_glStencilStrokePathInstancedNV;
    glad_debug_glStencilStrokePathNV = glad_debug_impl_glStencilStrokePathNV;
    glad_debug_glStencilThenCoverFillPathInstancedNV = glad_debug_impl_glStencilThenCoverFillPathInstancedNV;
    glad_debug_glStencilThenCoverFillPathNV = glad_debug_impl_glStencilThenCoverFillPathNV;
    glad_debug_glStencilThenCoverStrokePathInstancedNV = glad_debug_impl_glStencilThenCoverStrokePathInstancedNV;
    glad_debug_glStencilThenCoverStrokePathNV = glad_debug_impl_glStencilThenCoverStrokePathNV;
    glad_debug_glSubpixelPrecisionBiasNV = glad_debug_impl_glSubpixelPrecisionBiasNV;
    glad_debug_glTestFenceNV = glad_debug_impl_glTestFenceNV;
    glad_debug_glTexAttachMemoryNV = glad_debug_impl_glTexAttachMemoryNV;
    glad_debug_glTexBuffer = glad_debug_impl_glTexBuffer;
    glad_debug_glTexBufferEXT = glad_debug_impl_glTexBufferEXT;
    glad_debug_glTexBufferOES = glad_debug_impl_glTexBufferOES;
    glad_debug_glTexBufferRange = glad_debug_impl_glTexBufferRange;
    glad_debug_glTexBufferRangeEXT = glad_debug_impl_glTexBufferRangeEXT;
    glad_debug_glTexBufferRangeOES = glad_debug_impl_glTexBufferRangeOES;
    glad_debug_glTexEstimateMotionQCOM = glad_debug_impl_glTexEstimateMotionQCOM;
    glad_debug_glTexEstimateMotionRegionsQCOM = glad_debug_impl_glTexEstimateMotionRegionsQCOM;
    glad_debug_glTexImage2D = glad_debug_impl_glTexImage2D;
    glad_debug_glTexImage3D = glad_debug_impl_glTexImage3D;
    glad_debug_glTexImage3DOES = glad_debug_impl_glTexImage3DOES;
    glad_debug_glTexPageCommitmentEXT = glad_debug_impl_glTexPageCommitmentEXT;
    glad_debug_glTexPageCommitmentMemNV = glad_debug_impl_glTexPageCommitmentMemNV;
    glad_debug_glTexParameterIiv = glad_debug_impl_glTexParameterIiv;
    glad_debug_glTexParameterIivEXT = glad_debug_impl_glTexParameterIivEXT;
    glad_debug_glTexParameterIivOES = glad_debug_impl_glTexParameterIivOES;
    glad_debug_glTexParameterIuiv = glad_debug_impl_glTexParameterIuiv;
    glad_debug_glTexParameterIuivEXT = glad_debug_impl_glTexParameterIuivEXT;
    glad_debug_glTexParameterIuivOES = glad_debug_impl_glTexParameterIuivOES;
    glad_debug_glTexParameterf = glad_debug_impl_glTexParameterf;
    glad_debug_glTexParameterfv = glad_debug_impl_glTexParameterfv;
    glad_debug_glTexParameteri = glad_debug_impl_glTexParameteri;
    glad_debug_glTexParameteriv = glad_debug_impl_glTexParameteriv;
    glad_debug_glTexStorage1DEXT = glad_debug_impl_glTexStorage1DEXT;
    glad_debug_glTexStorage2D = glad_debug_impl_glTexStorage2D;
    glad_debug_glTexStorage2DEXT = glad_debug_impl_glTexStorage2DEXT;
    glad_debug_glTexStorage2DMultisample = glad_debug_impl_glTexStorage2DMultisample;
    glad_debug_glTexStorage3D = glad_debug_impl_glTexStorage3D;
    glad_debug_glTexStorage3DEXT = glad_debug_impl_glTexStorage3DEXT;
    glad_debug_glTexStorage3DMultisample = glad_debug_impl_glTexStorage3DMultisample;
    glad_debug_glTexStorage3DMultisampleOES = glad_debug_impl_glTexStorage3DMultisampleOES;
    glad_debug_glTexStorageAttribs2DEXT = glad_debug_impl_glTexStorageAttribs2DEXT;
    glad_debug_glTexStorageAttribs3DEXT = glad_debug_impl_glTexStorageAttribs3DEXT;
    glad_debug_glTexStorageMem2DEXT = glad_debug_impl_glTexStorageMem2DEXT;
    glad_debug_glTexStorageMem2DMultisampleEXT = glad_debug_impl_glTexStorageMem2DMultisampleEXT;
    glad_debug_glTexStorageMem3DEXT = glad_debug_impl_glTexStorageMem3DEXT;
    glad_debug_glTexStorageMem3DMultisampleEXT = glad_debug_impl_glTexStorageMem3DMultisampleEXT;
    glad_debug_glTexSubImage2D = glad_debug_impl_glTexSubImage2D;
    glad_debug_glTexSubImage3D = glad_debug_impl_glTexSubImage3D;
    glad_debug_glTexSubImage3DOES = glad_debug_impl_glTexSubImage3DOES;
    glad_debug_glTextureAttachMemoryNV = glad_debug_impl_glTextureAttachMemoryNV;
    glad_debug_glTextureBarrierNV = glad_debug_impl_glTextureBarrierNV;
    glad_debug_glTextureFoveationParametersQCOM = glad_debug_impl_glTextureFoveationParametersQCOM;
    glad_debug_glTexturePageCommitmentMemNV = glad_debug_impl_glTexturePageCommitmentMemNV;
    glad_debug_glTextureStorage1DEXT = glad_debug_impl_glTextureStorage1DEXT;
    glad_debug_glTextureStorage2DEXT = glad_debug_impl_glTextureStorage2DEXT;
    glad_debug_glTextureStorage3DEXT = glad_debug_impl_glTextureStorage3DEXT;
    glad_debug_glTextureStorageMem2DEXT = glad_debug_impl_glTextureStorageMem2DEXT;
    glad_debug_glTextureStorageMem2DMultisampleEXT = glad_debug_impl_glTextureStorageMem2DMultisampleEXT;
    glad_debug_glTextureStorageMem3DEXT = glad_debug_impl_glTextureStorageMem3DEXT;
    glad_debug_glTextureStorageMem3DMultisampleEXT = glad_debug_impl_glTextureStorageMem3DMultisampleEXT;
    glad_debug_glTextureViewEXT = glad_debug_impl_glTextureViewEXT;
    glad_debug_glTextureViewOES = glad_debug_impl_glTextureViewOES;
    glad_debug_glTransformFeedbackVaryings = glad_debug_impl_glTransformFeedbackVaryings;
    glad_debug_glTransformPathNV = glad_debug_impl_glTransformPathNV;
    glad_debug_glUniform1f = glad_debug_impl_glUniform1f;
    glad_debug_glUniform1fv = glad_debug_impl_glUniform1fv;
    glad_debug_glUniform1i = glad_debug_impl_glUniform1i;
    glad_debug_glUniform1i64NV = glad_debug_impl_glUniform1i64NV;
    glad_debug_glUniform1i64vNV = glad_debug_impl_glUniform1i64vNV;
    glad_debug_glUniform1iv = glad_debug_impl_glUniform1iv;
    glad_debug_glUniform1ui = glad_debug_impl_glUniform1ui;
    glad_debug_glUniform1ui64NV = glad_debug_impl_glUniform1ui64NV;
    glad_debug_glUniform1ui64vNV = glad_debug_impl_glUniform1ui64vNV;
    glad_debug_glUniform1uiv = glad_debug_impl_glUniform1uiv;
    glad_debug_glUniform2f = glad_debug_impl_glUniform2f;
    glad_debug_glUniform2fv = glad_debug_impl_glUniform2fv;
    glad_debug_glUniform2i = glad_debug_impl_glUniform2i;
    glad_debug_glUniform2i64NV = glad_debug_impl_glUniform2i64NV;
    glad_debug_glUniform2i64vNV = glad_debug_impl_glUniform2i64vNV;
    glad_debug_glUniform2iv = glad_debug_impl_glUniform2iv;
    glad_debug_glUniform2ui = glad_debug_impl_glUniform2ui;
    glad_debug_glUniform2ui64NV = glad_debug_impl_glUniform2ui64NV;
    glad_debug_glUniform2ui64vNV = glad_debug_impl_glUniform2ui64vNV;
    glad_debug_glUniform2uiv = glad_debug_impl_glUniform2uiv;
    glad_debug_glUniform3f = glad_debug_impl_glUniform3f;
    glad_debug_glUniform3fv = glad_debug_impl_glUniform3fv;
    glad_debug_glUniform3i = glad_debug_impl_glUniform3i;
    glad_debug_glUniform3i64NV = glad_debug_impl_glUniform3i64NV;
    glad_debug_glUniform3i64vNV = glad_debug_impl_glUniform3i64vNV;
    glad_debug_glUniform3iv = glad_debug_impl_glUniform3iv;
    glad_debug_glUniform3ui = glad_debug_impl_glUniform3ui;
    glad_debug_glUniform3ui64NV = glad_debug_impl_glUniform3ui64NV;
    glad_debug_glUniform3ui64vNV = glad_debug_impl_glUniform3ui64vNV;
    glad_debug_glUniform3uiv = glad_debug_impl_glUniform3uiv;
    glad_debug_glUniform4f = glad_debug_impl_glUniform4f;
    glad_debug_glUniform4fv = glad_debug_impl_glUniform4fv;
    glad_debug_glUniform4i = glad_debug_impl_glUniform4i;
    glad_debug_glUniform4i64NV = glad_debug_impl_glUniform4i64NV;
    glad_debug_glUniform4i64vNV = glad_debug_impl_glUniform4i64vNV;
    glad_debug_glUniform4iv = glad_debug_impl_glUniform4iv;
    glad_debug_glUniform4ui = glad_debug_impl_glUniform4ui;
    glad_debug_glUniform4ui64NV = glad_debug_impl_glUniform4ui64NV;
    glad_debug_glUniform4ui64vNV = glad_debug_impl_glUniform4ui64vNV;
    glad_debug_glUniform4uiv = glad_debug_impl_glUniform4uiv;
    glad_debug_glUniformBlockBinding = glad_debug_impl_glUniformBlockBinding;
    glad_debug_glUniformHandleui64IMG = glad_debug_impl_glUniformHandleui64IMG;
    glad_debug_glUniformHandleui64NV = glad_debug_impl_glUniformHandleui64NV;
    glad_debug_glUniformHandleui64vIMG = glad_debug_impl_glUniformHandleui64vIMG;
    glad_debug_glUniformHandleui64vNV = glad_debug_impl_glUniformHandleui64vNV;
    glad_debug_glUniformMatrix2fv = glad_debug_impl_glUniformMatrix2fv;
    glad_debug_glUniformMatrix2x3fv = glad_debug_impl_glUniformMatrix2x3fv;
    glad_debug_glUniformMatrix2x3fvNV = glad_debug_impl_glUniformMatrix2x3fvNV;
    glad_debug_glUniformMatrix2x4fv = glad_debug_impl_glUniformMatrix2x4fv;
    glad_debug_glUniformMatrix2x4fvNV = glad_debug_impl_glUniformMatrix2x4fvNV;
    glad_debug_glUniformMatrix3fv = glad_debug_impl_glUniformMatrix3fv;
    glad_debug_glUniformMatrix3x2fv = glad_debug_impl_glUniformMatrix3x2fv;
    glad_debug_glUniformMatrix3x2fvNV = glad_debug_impl_glUniformMatrix3x2fvNV;
    glad_debug_glUniformMatrix3x4fv = glad_debug_impl_glUniformMatrix3x4fv;
    glad_debug_glUniformMatrix3x4fvNV = glad_debug_impl_glUniformMatrix3x4fvNV;
    glad_debug_glUniformMatrix4fv = glad_debug_impl_glUniformMatrix4fv;
    glad_debug_glUniformMatrix4x2fv = glad_debug_impl_glUniformMatrix4x2fv;
    glad_debug_glUniformMatrix4x2fvNV = glad_debug_impl_glUniformMatrix4x2fvNV;
    glad_debug_glUniformMatrix4x3fv = glad_debug_impl_glUniformMatrix4x3fv;
    glad_debug_glUniformMatrix4x3fvNV = glad_debug_impl_glUniformMatrix4x3fvNV;
    glad_debug_glUnmapBuffer = glad_debug_impl_glUnmapBuffer;
    glad_debug_glUnmapBufferOES = glad_debug_impl_glUnmapBufferOES;
    glad_debug_glUseProgram = glad_debug_impl_glUseProgram;
    glad_debug_glUseProgramStages = glad_debug_impl_glUseProgramStages;
    glad_debug_glUseProgramStagesEXT = glad_debug_impl_glUseProgramStagesEXT;
    glad_debug_glValidateProgram = glad_debug_impl_glValidateProgram;
    glad_debug_glValidateProgramPipeline = glad_debug_impl_glValidateProgramPipeline;
    glad_debug_glValidateProgramPipelineEXT = glad_debug_impl_glValidateProgramPipelineEXT;
    glad_debug_glVertexAttrib1f = glad_debug_impl_glVertexAttrib1f;
    glad_debug_glVertexAttrib1fv = glad_debug_impl_glVertexAttrib1fv;
    glad_debug_glVertexAttrib2f = glad_debug_impl_glVertexAttrib2f;
    glad_debug_glVertexAttrib2fv = glad_debug_impl_glVertexAttrib2fv;
    glad_debug_glVertexAttrib3f = glad_debug_impl_glVertexAttrib3f;
    glad_debug_glVertexAttrib3fv = glad_debug_impl_glVertexAttrib3fv;
    glad_debug_glVertexAttrib4f = glad_debug_impl_glVertexAttrib4f;
    glad_debug_glVertexAttrib4fv = glad_debug_impl_glVertexAttrib4fv;
    glad_debug_glVertexAttribBinding = glad_debug_impl_glVertexAttribBinding;
    glad_debug_glVertexAttribDivisor = glad_debug_impl_glVertexAttribDivisor;
    glad_debug_glVertexAttribDivisorANGLE = glad_debug_impl_glVertexAttribDivisorANGLE;
    glad_debug_glVertexAttribDivisorEXT = glad_debug_impl_glVertexAttribDivisorEXT;
    glad_debug_glVertexAttribDivisorNV = glad_debug_impl_glVertexAttribDivisorNV;
    glad_debug_glVertexAttribFormat = glad_debug_impl_glVertexAttribFormat;
    glad_debug_glVertexAttribI4i = glad_debug_impl_glVertexAttribI4i;
    glad_debug_glVertexAttribI4iv = glad_debug_impl_glVertexAttribI4iv;
    glad_debug_glVertexAttribI4ui = glad_debug_impl_glVertexAttribI4ui;
    glad_debug_glVertexAttribI4uiv = glad_debug_impl_glVertexAttribI4uiv;
    glad_debug_glVertexAttribIFormat = glad_debug_impl_glVertexAttribIFormat;
    glad_debug_glVertexAttribIPointer = glad_debug_impl_glVertexAttribIPointer;
    glad_debug_glVertexAttribPointer = glad_debug_impl_glVertexAttribPointer;
    glad_debug_glVertexBindingDivisor = glad_debug_impl_glVertexBindingDivisor;
    glad_debug_glViewport = glad_debug_impl_glViewport;
    glad_debug_glViewportArrayvNV = glad_debug_impl_glViewportArrayvNV;
    glad_debug_glViewportArrayvOES = glad_debug_impl_glViewportArrayvOES;
    glad_debug_glViewportIndexedfNV = glad_debug_impl_glViewportIndexedfNV;
    glad_debug_glViewportIndexedfOES = glad_debug_impl_glViewportIndexedfOES;
    glad_debug_glViewportIndexedfvNV = glad_debug_impl_glViewportIndexedfvNV;
    glad_debug_glViewportIndexedfvOES = glad_debug_impl_glViewportIndexedfvOES;
    glad_debug_glViewportPositionWScaleNV = glad_debug_impl_glViewportPositionWScaleNV;
    glad_debug_glViewportSwizzleNV = glad_debug_impl_glViewportSwizzleNV;
    glad_debug_glWaitSemaphoreEXT = glad_debug_impl_glWaitSemaphoreEXT;
    glad_debug_glWaitSync = glad_debug_impl_glWaitSync;
    glad_debug_glWaitSyncAPPLE = glad_debug_impl_glWaitSyncAPPLE;
    glad_debug_glWaitVkSemaphoreNV = glad_debug_impl_glWaitVkSemaphoreNV;
    glad_debug_glWeightPathsNV = glad_debug_impl_glWeightPathsNV;
    glad_debug_glWindowRectanglesEXT = glad_debug_impl_glWindowRectanglesEXT;
}

void gladUninstallGLES2Debug(void) {
    glad_debug_glAcquireKeyedMutexWin32EXT = glad_glAcquireKeyedMutexWin32EXT;
    glad_debug_glActiveShaderProgram = glad_glActiveShaderProgram;
    glad_debug_glActiveShaderProgramEXT = glad_glActiveShaderProgramEXT;
    glad_debug_glActiveTexture = glad_glActiveTexture;
    glad_debug_glAlphaFuncQCOM = glad_glAlphaFuncQCOM;
    glad_debug_glApplyFramebufferAttachmentCMAAINTEL = glad_glApplyFramebufferAttachmentCMAAINTEL;
    glad_debug_glAttachShader = glad_glAttachShader;
    glad_debug_glBeginConditionalRenderNV = glad_glBeginConditionalRenderNV;
    glad_debug_glBeginPerfMonitorAMD = glad_glBeginPerfMonitorAMD;
    glad_debug_glBeginPerfQueryINTEL = glad_glBeginPerfQueryINTEL;
    glad_debug_glBeginQuery = glad_glBeginQuery;
    glad_debug_glBeginQueryEXT = glad_glBeginQueryEXT;
    glad_debug_glBeginTransformFeedback = glad_glBeginTransformFeedback;
    glad_debug_glBindAttribLocation = glad_glBindAttribLocation;
    glad_debug_glBindBuffer = glad_glBindBuffer;
    glad_debug_glBindBufferBase = glad_glBindBufferBase;
    glad_debug_glBindBufferRange = glad_glBindBufferRange;
    glad_debug_glBindFragDataLocationEXT = glad_glBindFragDataLocationEXT;
    glad_debug_glBindFragDataLocationIndexedEXT = glad_glBindFragDataLocationIndexedEXT;
    glad_debug_glBindFramebuffer = glad_glBindFramebuffer;
    glad_debug_glBindImageTexture = glad_glBindImageTexture;
    glad_debug_glBindProgramPipeline = glad_glBindProgramPipeline;
    glad_debug_glBindProgramPipelineEXT = glad_glBindProgramPipelineEXT;
    glad_debug_glBindRenderbuffer = glad_glBindRenderbuffer;
    glad_debug_glBindSampler = glad_glBindSampler;
    glad_debug_glBindShadingRateImageNV = glad_glBindShadingRateImageNV;
    glad_debug_glBindTexture = glad_glBindTexture;
    glad_debug_glBindTransformFeedback = glad_glBindTransformFeedback;
    glad_debug_glBindVertexArray = glad_glBindVertexArray;
    glad_debug_glBindVertexArrayOES = glad_glBindVertexArrayOES;
    glad_debug_glBindVertexBuffer = glad_glBindVertexBuffer;
    glad_debug_glBlendBarrier = glad_glBlendBarrier;
    glad_debug_glBlendBarrierKHR = glad_glBlendBarrierKHR;
    glad_debug_glBlendBarrierNV = glad_glBlendBarrierNV;
    glad_debug_glBlendColor = glad_glBlendColor;
    glad_debug_glBlendEquation = glad_glBlendEquation;
    glad_debug_glBlendEquationSeparate = glad_glBlendEquationSeparate;
    glad_debug_glBlendEquationSeparatei = glad_glBlendEquationSeparatei;
    glad_debug_glBlendEquationSeparateiEXT = glad_glBlendEquationSeparateiEXT;
    glad_debug_glBlendEquationSeparateiOES = glad_glBlendEquationSeparateiOES;
    glad_debug_glBlendEquationi = glad_glBlendEquationi;
    glad_debug_glBlendEquationiEXT = glad_glBlendEquationiEXT;
    glad_debug_glBlendEquationiOES = glad_glBlendEquationiOES;
    glad_debug_glBlendFunc = glad_glBlendFunc;
    glad_debug_glBlendFuncSeparate = glad_glBlendFuncSeparate;
    glad_debug_glBlendFuncSeparatei = glad_glBlendFuncSeparatei;
    glad_debug_glBlendFuncSeparateiEXT = glad_glBlendFuncSeparateiEXT;
    glad_debug_glBlendFuncSeparateiOES = glad_glBlendFuncSeparateiOES;
    glad_debug_glBlendFunci = glad_glBlendFunci;
    glad_debug_glBlendFunciEXT = glad_glBlendFunciEXT;
    glad_debug_glBlendFunciOES = glad_glBlendFunciOES;
    glad_debug_glBlendParameteriNV = glad_glBlendParameteriNV;
    glad_debug_glBlitFramebuffer = glad_glBlitFramebuffer;
    glad_debug_glBlitFramebufferANGLE = glad_glBlitFramebufferANGLE;
    glad_debug_glBlitFramebufferLayerEXT = glad_glBlitFramebufferLayerEXT;
    glad_debug_glBlitFramebufferLayersEXT = glad_glBlitFramebufferLayersEXT;
    glad_debug_glBlitFramebufferNV = glad_glBlitFramebufferNV;
    glad_debug_glBufferAttachMemoryNV = glad_glBufferAttachMemoryNV;
    glad_debug_glBufferData = glad_glBufferData;
    glad_debug_glBufferPageCommitmentMemNV = glad_glBufferPageCommitmentMemNV;
    glad_debug_glBufferStorageEXT = glad_glBufferStorageEXT;
    glad_debug_glBufferStorageExternalEXT = glad_glBufferStorageExternalEXT;
    glad_debug_glBufferStorageMemEXT = glad_glBufferStorageMemEXT;
    glad_debug_glBufferSubData = glad_glBufferSubData;
    glad_debug_glCheckFramebufferStatus = glad_glCheckFramebufferStatus;
    glad_debug_glClear = glad_glClear;
    glad_debug_glClearBufferfi = glad_glClearBufferfi;
    glad_debug_glClearBufferfv = glad_glClearBufferfv;
    glad_debug_glClearBufferiv = glad_glClearBufferiv;
    glad_debug_glClearBufferuiv = glad_glClearBufferuiv;
    glad_debug_glClearColor = glad_glClearColor;
    glad_debug_glClearDepthf = glad_glClearDepthf;
    glad_debug_glClearPixelLocalStorageuiEXT = glad_glClearPixelLocalStorageuiEXT;
    glad_debug_glClearStencil = glad_glClearStencil;
    glad_debug_glClearTexImageEXT = glad_glClearTexImageEXT;
    glad_debug_glClearTexSubImageEXT = glad_glClearTexSubImageEXT;
    glad_debug_glClientWaitSync = glad_glClientWaitSync;
    glad_debug_glClientWaitSyncAPPLE = glad_glClientWaitSyncAPPLE;
    glad_debug_glClipControlEXT = glad_glClipControlEXT;
    glad_debug_glColorMask = glad_glColorMask;
    glad_debug_glColorMaski = glad_glColorMaski;
    glad_debug_glColorMaskiEXT = glad_glColorMaskiEXT;
    glad_debug_glColorMaskiOES = glad_glColorMaskiOES;
    glad_debug_glCompileShader = glad_glCompileShader;
    glad_debug_glCompressedTexImage2D = glad_glCompressedTexImage2D;
    glad_debug_glCompressedTexImage3D = glad_glCompressedTexImage3D;
    glad_debug_glCompressedTexImage3DOES = glad_glCompressedTexImage3DOES;
    glad_debug_glCompressedTexSubImage2D = glad_glCompressedTexSubImage2D;
    glad_debug_glCompressedTexSubImage3D = glad_glCompressedTexSubImage3D;
    glad_debug_glCompressedTexSubImage3DOES = glad_glCompressedTexSubImage3DOES;
    glad_debug_glConservativeRasterParameteriNV = glad_glConservativeRasterParameteriNV;
    glad_debug_glCopyBufferSubData = glad_glCopyBufferSubData;
    glad_debug_glCopyBufferSubDataNV = glad_glCopyBufferSubDataNV;
    glad_debug_glCopyImageSubData = glad_glCopyImageSubData;
    glad_debug_glCopyImageSubDataEXT = glad_glCopyImageSubDataEXT;
    glad_debug_glCopyImageSubDataOES = glad_glCopyImageSubDataOES;
    glad_debug_glCopyPathNV = glad_glCopyPathNV;
    glad_debug_glCopyTexImage2D = glad_glCopyTexImage2D;
    glad_debug_glCopyTexSubImage2D = glad_glCopyTexSubImage2D;
    glad_debug_glCopyTexSubImage3D = glad_glCopyTexSubImage3D;
    glad_debug_glCopyTexSubImage3DOES = glad_glCopyTexSubImage3DOES;
    glad_debug_glCopyTextureLevelsAPPLE = glad_glCopyTextureLevelsAPPLE;
    glad_debug_glCoverFillPathInstancedNV = glad_glCoverFillPathInstancedNV;
    glad_debug_glCoverFillPathNV = glad_glCoverFillPathNV;
    glad_debug_glCoverStrokePathInstancedNV = glad_glCoverStrokePathInstancedNV;
    glad_debug_glCoverStrokePathNV = glad_glCoverStrokePathNV;
    glad_debug_glCoverageMaskNV = glad_glCoverageMaskNV;
    glad_debug_glCoverageModulationNV = glad_glCoverageModulationNV;
    glad_debug_glCoverageModulationTableNV = glad_glCoverageModulationTableNV;
    glad_debug_glCoverageOperationNV = glad_glCoverageOperationNV;
    glad_debug_glCreateMemoryObjectsEXT = glad_glCreateMemoryObjectsEXT;
    glad_debug_glCreatePerfQueryINTEL = glad_glCreatePerfQueryINTEL;
    glad_debug_glCreateProgram = glad_glCreateProgram;
    glad_debug_glCreateSemaphoresNV = glad_glCreateSemaphoresNV;
    glad_debug_glCreateShader = glad_glCreateShader;
    glad_debug_glCreateShaderProgramv = glad_glCreateShaderProgramv;
    glad_debug_glCreateShaderProgramvEXT = glad_glCreateShaderProgramvEXT;
    glad_debug_glCullFace = glad_glCullFace;
    glad_debug_glDebugMessageCallback = glad_glDebugMessageCallback;
    glad_debug_glDebugMessageCallbackKHR = glad_glDebugMessageCallbackKHR;
    glad_debug_glDebugMessageControl = glad_glDebugMessageControl;
    glad_debug_glDebugMessageControlKHR = glad_glDebugMessageControlKHR;
    glad_debug_glDebugMessageInsert = glad_glDebugMessageInsert;
    glad_debug_glDebugMessageInsertKHR = glad_glDebugMessageInsertKHR;
    glad_debug_glDeleteBuffers = glad_glDeleteBuffers;
    glad_debug_glDeleteFencesNV = glad_glDeleteFencesNV;
    glad_debug_glDeleteFramebuffers = glad_glDeleteFramebuffers;
    glad_debug_glDeleteMemoryObjectsEXT = glad_glDeleteMemoryObjectsEXT;
    glad_debug_glDeletePathsNV = glad_glDeletePathsNV;
    glad_debug_glDeletePerfMonitorsAMD = glad_glDeletePerfMonitorsAMD;
    glad_debug_glDeletePerfQueryINTEL = glad_glDeletePerfQueryINTEL;
    glad_debug_glDeleteProgram = glad_glDeleteProgram;
    glad_debug_glDeleteProgramPipelines = glad_glDeleteProgramPipelines;
    glad_debug_glDeleteProgramPipelinesEXT = glad_glDeleteProgramPipelinesEXT;
    glad_debug_glDeleteQueries = glad_glDeleteQueries;
    glad_debug_glDeleteQueriesEXT = glad_glDeleteQueriesEXT;
    glad_debug_glDeleteRenderbuffers = glad_glDeleteRenderbuffers;
    glad_debug_glDeleteSamplers = glad_glDeleteSamplers;
    glad_debug_glDeleteSemaphoresEXT = glad_glDeleteSemaphoresEXT;
    glad_debug_glDeleteShader = glad_glDeleteShader;
    glad_debug_glDeleteSync = glad_glDeleteSync;
    glad_debug_glDeleteSyncAPPLE = glad_glDeleteSyncAPPLE;
    glad_debug_glDeleteTextures = glad_glDeleteTextures;
    glad_debug_glDeleteTransformFeedbacks = glad_glDeleteTransformFeedbacks;
    glad_debug_glDeleteVertexArrays = glad_glDeleteVertexArrays;
    glad_debug_glDeleteVertexArraysOES = glad_glDeleteVertexArraysOES;
    glad_debug_glDepthFunc = glad_glDepthFunc;
    glad_debug_glDepthMask = glad_glDepthMask;
    glad_debug_glDepthRangeArrayfvNV = glad_glDepthRangeArrayfvNV;
    glad_debug_glDepthRangeArrayfvOES = glad_glDepthRangeArrayfvOES;
    glad_debug_glDepthRangeIndexedfNV = glad_glDepthRangeIndexedfNV;
    glad_debug_glDepthRangeIndexedfOES = glad_glDepthRangeIndexedfOES;
    glad_debug_glDepthRangef = glad_glDepthRangef;
    glad_debug_glDetachShader = glad_glDetachShader;
    glad_debug_glDisable = glad_glDisable;
    glad_debug_glDisableDriverControlQCOM = glad_glDisableDriverControlQCOM;
    glad_debug_glDisableVertexAttribArray = glad_glDisableVertexAttribArray;
    glad_debug_glDisablei = glad_glDisablei;
    glad_debug_glDisableiEXT = glad_glDisableiEXT;
    glad_debug_glDisableiNV = glad_glDisableiNV;
    glad_debug_glDisableiOES = glad_glDisableiOES;
    glad_debug_glDiscardFramebufferEXT = glad_glDiscardFramebufferEXT;
    glad_debug_glDispatchCompute = glad_glDispatchCompute;
    glad_debug_glDispatchComputeIndirect = glad_glDispatchComputeIndirect;
    glad_debug_glDrawArrays = glad_glDrawArrays;
    glad_debug_glDrawArraysIndirect = glad_glDrawArraysIndirect;
    glad_debug_glDrawArraysInstanced = glad_glDrawArraysInstanced;
    glad_debug_glDrawArraysInstancedANGLE = glad_glDrawArraysInstancedANGLE;
    glad_debug_glDrawArraysInstancedBaseInstanceEXT = glad_glDrawArraysInstancedBaseInstanceEXT;
    glad_debug_glDrawArraysInstancedEXT = glad_glDrawArraysInstancedEXT;
    glad_debug_glDrawArraysInstancedNV = glad_glDrawArraysInstancedNV;
    glad_debug_glDrawBuffers = glad_glDrawBuffers;
    glad_debug_glDrawBuffersEXT = glad_glDrawBuffersEXT;
    glad_debug_glDrawBuffersIndexedEXT = glad_glDrawBuffersIndexedEXT;
    glad_debug_glDrawBuffersNV = glad_glDrawBuffersNV;
    glad_debug_glDrawElements = glad_glDrawElements;
    glad_debug_glDrawElementsBaseVertex = glad_glDrawElementsBaseVertex;
    glad_debug_glDrawElementsBaseVertexEXT = glad_glDrawElementsBaseVertexEXT;
    glad_debug_glDrawElementsBaseVertexOES = glad_glDrawElementsBaseVertexOES;
    glad_debug_glDrawElementsIndirect = glad_glDrawElementsIndirect;
    glad_debug_glDrawElementsInstanced = glad_glDrawElementsInstanced;
    glad_debug_glDrawElementsInstancedANGLE = glad_glDrawElementsInstancedANGLE;
    glad_debug_glDrawElementsInstancedBaseInstanceEXT = glad_glDrawElementsInstancedBaseInstanceEXT;
    glad_debug_glDrawElementsInstancedBaseVertex = glad_glDrawElementsInstancedBaseVertex;
    glad_debug_glDrawElementsInstancedBaseVertexBaseInstanceEXT = glad_glDrawElementsInstancedBaseVertexBaseInstanceEXT;
    glad_debug_glDrawElementsInstancedBaseVertexEXT = glad_glDrawElementsInstancedBaseVertexEXT;
    glad_debug_glDrawElementsInstancedBaseVertexOES = glad_glDrawElementsInstancedBaseVertexOES;
    glad_debug_glDrawElementsInstancedEXT = glad_glDrawElementsInstancedEXT;
    glad_debug_glDrawElementsInstancedNV = glad_glDrawElementsInstancedNV;
    glad_debug_glDrawMeshTasksEXT = glad_glDrawMeshTasksEXT;
    glad_debug_glDrawMeshTasksIndirectEXT = glad_glDrawMeshTasksIndirectEXT;
    glad_debug_glDrawMeshTasksIndirectNV = glad_glDrawMeshTasksIndirectNV;
    glad_debug_glDrawMeshTasksNV = glad_glDrawMeshTasksNV;
    glad_debug_glDrawRangeElements = glad_glDrawRangeElements;
    glad_debug_glDrawRangeElementsBaseVertex = glad_glDrawRangeElementsBaseVertex;
    glad_debug_glDrawRangeElementsBaseVertexEXT = glad_glDrawRangeElementsBaseVertexEXT;
    glad_debug_glDrawRangeElementsBaseVertexOES = glad_glDrawRangeElementsBaseVertexOES;
    glad_debug_glDrawTransformFeedbackEXT = glad_glDrawTransformFeedbackEXT;
    glad_debug_glDrawTransformFeedbackInstancedEXT = glad_glDrawTransformFeedbackInstancedEXT;
    glad_debug_glDrawVkImageNV = glad_glDrawVkImageNV;
    glad_debug_glEGLImageTargetRenderbufferStorageOES = glad_glEGLImageTargetRenderbufferStorageOES;
    glad_debug_glEGLImageTargetTexStorageEXT = glad_glEGLImageTargetTexStorageEXT;
    glad_debug_glEGLImageTargetTexture2DOES = glad_glEGLImageTargetTexture2DOES;
    glad_debug_glEGLImageTargetTextureStorageEXT = glad_glEGLImageTargetTextureStorageEXT;
    glad_debug_glEnable = glad_glEnable;
    glad_debug_glEnableDriverControlQCOM = glad_glEnableDriverControlQCOM;
    glad_debug_glEnableVertexAttribArray = glad_glEnableVertexAttribArray;
    glad_debug_glEnablei = glad_glEnablei;
    glad_debug_glEnableiEXT = glad_glEnableiEXT;
    glad_debug_glEnableiNV = glad_glEnableiNV;
    glad_debug_glEnableiOES = glad_glEnableiOES;
    glad_debug_glEndConditionalRenderNV = glad_glEndConditionalRenderNV;
    glad_debug_glEndPerfMonitorAMD = glad_glEndPerfMonitorAMD;
    glad_debug_glEndPerfQueryINTEL = glad_glEndPerfQueryINTEL;
    glad_debug_glEndQuery = glad_glEndQuery;
    glad_debug_glEndQueryEXT = glad_glEndQueryEXT;
    glad_debug_glEndTilingQCOM = glad_glEndTilingQCOM;
    glad_debug_glEndTransformFeedback = glad_glEndTransformFeedback;
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
    glad_debug_glExtrapolateTex2DQCOM = glad_glExtrapolateTex2DQCOM;
    glad_debug_glFenceSync = glad_glFenceSync;
    glad_debug_glFenceSyncAPPLE = glad_glFenceSyncAPPLE;
    glad_debug_glFinish = glad_glFinish;
    glad_debug_glFinishFenceNV = glad_glFinishFenceNV;
    glad_debug_glFlush = glad_glFlush;
    glad_debug_glFlushMappedBufferRange = glad_glFlushMappedBufferRange;
    glad_debug_glFlushMappedBufferRangeEXT = glad_glFlushMappedBufferRangeEXT;
    glad_debug_glFragmentCoverageColorNV = glad_glFragmentCoverageColorNV;
    glad_debug_glFramebufferFetchBarrierEXT = glad_glFramebufferFetchBarrierEXT;
    glad_debug_glFramebufferFetchBarrierQCOM = glad_glFramebufferFetchBarrierQCOM;
    glad_debug_glFramebufferFoveationConfigQCOM = glad_glFramebufferFoveationConfigQCOM;
    glad_debug_glFramebufferFoveationParametersQCOM = glad_glFramebufferFoveationParametersQCOM;
    glad_debug_glFramebufferParameteri = glad_glFramebufferParameteri;
    glad_debug_glFramebufferParameteriMESA = glad_glFramebufferParameteriMESA;
    glad_debug_glFramebufferPixelLocalStorageSizeEXT = glad_glFramebufferPixelLocalStorageSizeEXT;
    glad_debug_glFramebufferRenderbuffer = glad_glFramebufferRenderbuffer;
    glad_debug_glFramebufferSampleLocationsfvNV = glad_glFramebufferSampleLocationsfvNV;
    glad_debug_glFramebufferShadingRateEXT = glad_glFramebufferShadingRateEXT;
    glad_debug_glFramebufferTexture = glad_glFramebufferTexture;
    glad_debug_glFramebufferTexture2D = glad_glFramebufferTexture2D;
    glad_debug_glFramebufferTexture2DDownsampleIMG = glad_glFramebufferTexture2DDownsampleIMG;
    glad_debug_glFramebufferTexture2DMultisampleEXT = glad_glFramebufferTexture2DMultisampleEXT;
    glad_debug_glFramebufferTexture2DMultisampleIMG = glad_glFramebufferTexture2DMultisampleIMG;
    glad_debug_glFramebufferTexture3DOES = glad_glFramebufferTexture3DOES;
    glad_debug_glFramebufferTextureEXT = glad_glFramebufferTextureEXT;
    glad_debug_glFramebufferTextureLayer = glad_glFramebufferTextureLayer;
    glad_debug_glFramebufferTextureLayerDownsampleIMG = glad_glFramebufferTextureLayerDownsampleIMG;
    glad_debug_glFramebufferTextureMultisampleMultiviewOVR = glad_glFramebufferTextureMultisampleMultiviewOVR;
    glad_debug_glFramebufferTextureMultiviewOVR = glad_glFramebufferTextureMultiviewOVR;
    glad_debug_glFramebufferTextureOES = glad_glFramebufferTextureOES;
    glad_debug_glFrontFace = glad_glFrontFace;
    glad_debug_glGenBuffers = glad_glGenBuffers;
    glad_debug_glGenFencesNV = glad_glGenFencesNV;
    glad_debug_glGenFramebuffers = glad_glGenFramebuffers;
    glad_debug_glGenPathsNV = glad_glGenPathsNV;
    glad_debug_glGenPerfMonitorsAMD = glad_glGenPerfMonitorsAMD;
    glad_debug_glGenProgramPipelines = glad_glGenProgramPipelines;
    glad_debug_glGenProgramPipelinesEXT = glad_glGenProgramPipelinesEXT;
    glad_debug_glGenQueries = glad_glGenQueries;
    glad_debug_glGenQueriesEXT = glad_glGenQueriesEXT;
    glad_debug_glGenRenderbuffers = glad_glGenRenderbuffers;
    glad_debug_glGenSamplers = glad_glGenSamplers;
    glad_debug_glGenSemaphoresEXT = glad_glGenSemaphoresEXT;
    glad_debug_glGenTextures = glad_glGenTextures;
    glad_debug_glGenTransformFeedbacks = glad_glGenTransformFeedbacks;
    glad_debug_glGenVertexArrays = glad_glGenVertexArrays;
    glad_debug_glGenVertexArraysOES = glad_glGenVertexArraysOES;
    glad_debug_glGenerateMipmap = glad_glGenerateMipmap;
    glad_debug_glGetActiveAttrib = glad_glGetActiveAttrib;
    glad_debug_glGetActiveUniform = glad_glGetActiveUniform;
    glad_debug_glGetActiveUniformBlockName = glad_glGetActiveUniformBlockName;
    glad_debug_glGetActiveUniformBlockiv = glad_glGetActiveUniformBlockiv;
    glad_debug_glGetActiveUniformsiv = glad_glGetActiveUniformsiv;
    glad_debug_glGetAttachedShaders = glad_glGetAttachedShaders;
    glad_debug_glGetAttribLocation = glad_glGetAttribLocation;
    glad_debug_glGetBooleani_v = glad_glGetBooleani_v;
    glad_debug_glGetBooleanv = glad_glGetBooleanv;
    glad_debug_glGetBufferParameteri64v = glad_glGetBufferParameteri64v;
    glad_debug_glGetBufferParameteriv = glad_glGetBufferParameteriv;
    glad_debug_glGetBufferPointerv = glad_glGetBufferPointerv;
    glad_debug_glGetBufferPointervOES = glad_glGetBufferPointervOES;
    glad_debug_glGetCoverageModulationTableNV = glad_glGetCoverageModulationTableNV;
    glad_debug_glGetDebugMessageLog = glad_glGetDebugMessageLog;
    glad_debug_glGetDebugMessageLogKHR = glad_glGetDebugMessageLogKHR;
    glad_debug_glGetDriverControlStringQCOM = glad_glGetDriverControlStringQCOM;
    glad_debug_glGetDriverControlsQCOM = glad_glGetDriverControlsQCOM;
    glad_debug_glGetError = glad_glGetError;
    glad_debug_glGetFenceivNV = glad_glGetFenceivNV;
    glad_debug_glGetFirstPerfQueryIdINTEL = glad_glGetFirstPerfQueryIdINTEL;
    glad_debug_glGetFloati_vNV = glad_glGetFloati_vNV;
    glad_debug_glGetFloati_vOES = glad_glGetFloati_vOES;
    glad_debug_glGetFloatv = glad_glGetFloatv;
    glad_debug_glGetFragDataIndexEXT = glad_glGetFragDataIndexEXT;
    glad_debug_glGetFragDataLocation = glad_glGetFragDataLocation;
    glad_debug_glGetFragmentShadingRatesEXT = glad_glGetFragmentShadingRatesEXT;
    glad_debug_glGetFramebufferAttachmentParameteriv = glad_glGetFramebufferAttachmentParameteriv;
    glad_debug_glGetFramebufferParameteriv = glad_glGetFramebufferParameteriv;
    glad_debug_glGetFramebufferParameterivMESA = glad_glGetFramebufferParameterivMESA;
    glad_debug_glGetFramebufferPixelLocalStorageSizeEXT = glad_glGetFramebufferPixelLocalStorageSizeEXT;
    glad_debug_glGetGraphicsResetStatus = glad_glGetGraphicsResetStatus;
    glad_debug_glGetGraphicsResetStatusEXT = glad_glGetGraphicsResetStatusEXT;
    glad_debug_glGetGraphicsResetStatusKHR = glad_glGetGraphicsResetStatusKHR;
    glad_debug_glGetImageHandleNV = glad_glGetImageHandleNV;
    glad_debug_glGetInteger64i_v = glad_glGetInteger64i_v;
    glad_debug_glGetInteger64v = glad_glGetInteger64v;
    glad_debug_glGetInteger64vAPPLE = glad_glGetInteger64vAPPLE;
    glad_debug_glGetInteger64vEXT = glad_glGetInteger64vEXT;
    glad_debug_glGetIntegeri_v = glad_glGetIntegeri_v;
    glad_debug_glGetIntegeri_vEXT = glad_glGetIntegeri_vEXT;
    glad_debug_glGetIntegerv = glad_glGetIntegerv;
    glad_debug_glGetInternalformatSampleivNV = glad_glGetInternalformatSampleivNV;
    glad_debug_glGetInternalformativ = glad_glGetInternalformativ;
    glad_debug_glGetMemoryObjectDetachedResourcesuivNV = glad_glGetMemoryObjectDetachedResourcesuivNV;
    glad_debug_glGetMemoryObjectParameterivEXT = glad_glGetMemoryObjectParameterivEXT;
    glad_debug_glGetMultisamplefv = glad_glGetMultisamplefv;
    glad_debug_glGetNextPerfQueryIdINTEL = glad_glGetNextPerfQueryIdINTEL;
    glad_debug_glGetObjectLabel = glad_glGetObjectLabel;
    glad_debug_glGetObjectLabelEXT = glad_glGetObjectLabelEXT;
    glad_debug_glGetObjectLabelKHR = glad_glGetObjectLabelKHR;
    glad_debug_glGetObjectPtrLabel = glad_glGetObjectPtrLabel;
    glad_debug_glGetObjectPtrLabelKHR = glad_glGetObjectPtrLabelKHR;
    glad_debug_glGetPathCommandsNV = glad_glGetPathCommandsNV;
    glad_debug_glGetPathCoordsNV = glad_glGetPathCoordsNV;
    glad_debug_glGetPathDashArrayNV = glad_glGetPathDashArrayNV;
    glad_debug_glGetPathLengthNV = glad_glGetPathLengthNV;
    glad_debug_glGetPathMetricRangeNV = glad_glGetPathMetricRangeNV;
    glad_debug_glGetPathMetricsNV = glad_glGetPathMetricsNV;
    glad_debug_glGetPathParameterfvNV = glad_glGetPathParameterfvNV;
    glad_debug_glGetPathParameterivNV = glad_glGetPathParameterivNV;
    glad_debug_glGetPathSpacingNV = glad_glGetPathSpacingNV;
    glad_debug_glGetPerfCounterInfoINTEL = glad_glGetPerfCounterInfoINTEL;
    glad_debug_glGetPerfMonitorCounterDataAMD = glad_glGetPerfMonitorCounterDataAMD;
    glad_debug_glGetPerfMonitorCounterInfoAMD = glad_glGetPerfMonitorCounterInfoAMD;
    glad_debug_glGetPerfMonitorCounterStringAMD = glad_glGetPerfMonitorCounterStringAMD;
    glad_debug_glGetPerfMonitorCountersAMD = glad_glGetPerfMonitorCountersAMD;
    glad_debug_glGetPerfMonitorGroupStringAMD = glad_glGetPerfMonitorGroupStringAMD;
    glad_debug_glGetPerfMonitorGroupsAMD = glad_glGetPerfMonitorGroupsAMD;
    glad_debug_glGetPerfQueryDataINTEL = glad_glGetPerfQueryDataINTEL;
    glad_debug_glGetPerfQueryIdByNameINTEL = glad_glGetPerfQueryIdByNameINTEL;
    glad_debug_glGetPerfQueryInfoINTEL = glad_glGetPerfQueryInfoINTEL;
    glad_debug_glGetPointerv = glad_glGetPointerv;
    glad_debug_glGetPointervKHR = glad_glGetPointervKHR;
    glad_debug_glGetProgramBinary = glad_glGetProgramBinary;
    glad_debug_glGetProgramBinaryOES = glad_glGetProgramBinaryOES;
    glad_debug_glGetProgramInfoLog = glad_glGetProgramInfoLog;
    glad_debug_glGetProgramInterfaceiv = glad_glGetProgramInterfaceiv;
    glad_debug_glGetProgramPipelineInfoLog = glad_glGetProgramPipelineInfoLog;
    glad_debug_glGetProgramPipelineInfoLogEXT = glad_glGetProgramPipelineInfoLogEXT;
    glad_debug_glGetProgramPipelineiv = glad_glGetProgramPipelineiv;
    glad_debug_glGetProgramPipelineivEXT = glad_glGetProgramPipelineivEXT;
    glad_debug_glGetProgramResourceIndex = glad_glGetProgramResourceIndex;
    glad_debug_glGetProgramResourceLocation = glad_glGetProgramResourceLocation;
    glad_debug_glGetProgramResourceLocationIndexEXT = glad_glGetProgramResourceLocationIndexEXT;
    glad_debug_glGetProgramResourceName = glad_glGetProgramResourceName;
    glad_debug_glGetProgramResourcefvNV = glad_glGetProgramResourcefvNV;
    glad_debug_glGetProgramResourceiv = glad_glGetProgramResourceiv;
    glad_debug_glGetProgramiv = glad_glGetProgramiv;
    glad_debug_glGetQueryObjecti64vEXT = glad_glGetQueryObjecti64vEXT;
    glad_debug_glGetQueryObjectivEXT = glad_glGetQueryObjectivEXT;
    glad_debug_glGetQueryObjectui64vEXT = glad_glGetQueryObjectui64vEXT;
    glad_debug_glGetQueryObjectuiv = glad_glGetQueryObjectuiv;
    glad_debug_glGetQueryObjectuivEXT = glad_glGetQueryObjectuivEXT;
    glad_debug_glGetQueryiv = glad_glGetQueryiv;
    glad_debug_glGetQueryivEXT = glad_glGetQueryivEXT;
    glad_debug_glGetRenderbufferParameteriv = glad_glGetRenderbufferParameteriv;
    glad_debug_glGetSamplerParameterIiv = glad_glGetSamplerParameterIiv;
    glad_debug_glGetSamplerParameterIivEXT = glad_glGetSamplerParameterIivEXT;
    glad_debug_glGetSamplerParameterIivOES = glad_glGetSamplerParameterIivOES;
    glad_debug_glGetSamplerParameterIuiv = glad_glGetSamplerParameterIuiv;
    glad_debug_glGetSamplerParameterIuivEXT = glad_glGetSamplerParameterIuivEXT;
    glad_debug_glGetSamplerParameterIuivOES = glad_glGetSamplerParameterIuivOES;
    glad_debug_glGetSamplerParameterfv = glad_glGetSamplerParameterfv;
    glad_debug_glGetSamplerParameteriv = glad_glGetSamplerParameteriv;
    glad_debug_glGetSemaphoreParameterivNV = glad_glGetSemaphoreParameterivNV;
    glad_debug_glGetSemaphoreParameterui64vEXT = glad_glGetSemaphoreParameterui64vEXT;
    glad_debug_glGetShaderInfoLog = glad_glGetShaderInfoLog;
    glad_debug_glGetShaderPrecisionFormat = glad_glGetShaderPrecisionFormat;
    glad_debug_glGetShaderSource = glad_glGetShaderSource;
    glad_debug_glGetShaderiv = glad_glGetShaderiv;
    glad_debug_glGetShadingRateImagePaletteNV = glad_glGetShadingRateImagePaletteNV;
    glad_debug_glGetShadingRateSampleLocationivNV = glad_glGetShadingRateSampleLocationivNV;
    glad_debug_glGetString = glad_glGetString;
    glad_debug_glGetStringi = glad_glGetStringi;
    glad_debug_glGetSynciv = glad_glGetSynciv;
    glad_debug_glGetSyncivAPPLE = glad_glGetSyncivAPPLE;
    glad_debug_glGetTexLevelParameterfv = glad_glGetTexLevelParameterfv;
    glad_debug_glGetTexLevelParameteriv = glad_glGetTexLevelParameteriv;
    glad_debug_glGetTexParameterIiv = glad_glGetTexParameterIiv;
    glad_debug_glGetTexParameterIivEXT = glad_glGetTexParameterIivEXT;
    glad_debug_glGetTexParameterIivOES = glad_glGetTexParameterIivOES;
    glad_debug_glGetTexParameterIuiv = glad_glGetTexParameterIuiv;
    glad_debug_glGetTexParameterIuivEXT = glad_glGetTexParameterIuivEXT;
    glad_debug_glGetTexParameterIuivOES = glad_glGetTexParameterIuivOES;
    glad_debug_glGetTexParameterfv = glad_glGetTexParameterfv;
    glad_debug_glGetTexParameteriv = glad_glGetTexParameteriv;
    glad_debug_glGetTextureHandleIMG = glad_glGetTextureHandleIMG;
    glad_debug_glGetTextureHandleNV = glad_glGetTextureHandleNV;
    glad_debug_glGetTextureSamplerHandleIMG = glad_glGetTextureSamplerHandleIMG;
    glad_debug_glGetTextureSamplerHandleNV = glad_glGetTextureSamplerHandleNV;
    glad_debug_glGetTransformFeedbackVarying = glad_glGetTransformFeedbackVarying;
    glad_debug_glGetTranslatedShaderSourceANGLE = glad_glGetTranslatedShaderSourceANGLE;
    glad_debug_glGetUniformBlockIndex = glad_glGetUniformBlockIndex;
    glad_debug_glGetUniformIndices = glad_glGetUniformIndices;
    glad_debug_glGetUniformLocation = glad_glGetUniformLocation;
    glad_debug_glGetUniformfv = glad_glGetUniformfv;
    glad_debug_glGetUniformi64vNV = glad_glGetUniformi64vNV;
    glad_debug_glGetUniformiv = glad_glGetUniformiv;
    glad_debug_glGetUniformuiv = glad_glGetUniformuiv;
    glad_debug_glGetUnsignedBytei_vEXT = glad_glGetUnsignedBytei_vEXT;
    glad_debug_glGetUnsignedBytevEXT = glad_glGetUnsignedBytevEXT;
    glad_debug_glGetVertexAttribIiv = glad_glGetVertexAttribIiv;
    glad_debug_glGetVertexAttribIuiv = glad_glGetVertexAttribIuiv;
    glad_debug_glGetVertexAttribPointerv = glad_glGetVertexAttribPointerv;
    glad_debug_glGetVertexAttribfv = glad_glGetVertexAttribfv;
    glad_debug_glGetVertexAttribiv = glad_glGetVertexAttribiv;
    glad_debug_glGetVkProcAddrNV = glad_glGetVkProcAddrNV;
    glad_debug_glGetnUniformfv = glad_glGetnUniformfv;
    glad_debug_glGetnUniformfvEXT = glad_glGetnUniformfvEXT;
    glad_debug_glGetnUniformfvKHR = glad_glGetnUniformfvKHR;
    glad_debug_glGetnUniformiv = glad_glGetnUniformiv;
    glad_debug_glGetnUniformivEXT = glad_glGetnUniformivEXT;
    glad_debug_glGetnUniformivKHR = glad_glGetnUniformivKHR;
    glad_debug_glGetnUniformuiv = glad_glGetnUniformuiv;
    glad_debug_glGetnUniformuivKHR = glad_glGetnUniformuivKHR;
    glad_debug_glHint = glad_glHint;
    glad_debug_glImportMemoryFdEXT = glad_glImportMemoryFdEXT;
    glad_debug_glImportMemoryWin32HandleEXT = glad_glImportMemoryWin32HandleEXT;
    glad_debug_glImportMemoryWin32NameEXT = glad_glImportMemoryWin32NameEXT;
    glad_debug_glImportSemaphoreFdEXT = glad_glImportSemaphoreFdEXT;
    glad_debug_glImportSemaphoreWin32HandleEXT = glad_glImportSemaphoreWin32HandleEXT;
    glad_debug_glImportSemaphoreWin32NameEXT = glad_glImportSemaphoreWin32NameEXT;
    glad_debug_glInsertEventMarkerEXT = glad_glInsertEventMarkerEXT;
    glad_debug_glInterpolatePathsNV = glad_glInterpolatePathsNV;
    glad_debug_glInvalidateFramebuffer = glad_glInvalidateFramebuffer;
    glad_debug_glInvalidateSubFramebuffer = glad_glInvalidateSubFramebuffer;
    glad_debug_glIsBuffer = glad_glIsBuffer;
    glad_debug_glIsEnabled = glad_glIsEnabled;
    glad_debug_glIsEnabledi = glad_glIsEnabledi;
    glad_debug_glIsEnablediEXT = glad_glIsEnablediEXT;
    glad_debug_glIsEnablediNV = glad_glIsEnablediNV;
    glad_debug_glIsEnablediOES = glad_glIsEnablediOES;
    glad_debug_glIsFenceNV = glad_glIsFenceNV;
    glad_debug_glIsFramebuffer = glad_glIsFramebuffer;
    glad_debug_glIsImageHandleResidentNV = glad_glIsImageHandleResidentNV;
    glad_debug_glIsMemoryObjectEXT = glad_glIsMemoryObjectEXT;
    glad_debug_glIsPathNV = glad_glIsPathNV;
    glad_debug_glIsPointInFillPathNV = glad_glIsPointInFillPathNV;
    glad_debug_glIsPointInStrokePathNV = glad_glIsPointInStrokePathNV;
    glad_debug_glIsProgram = glad_glIsProgram;
    glad_debug_glIsProgramPipeline = glad_glIsProgramPipeline;
    glad_debug_glIsProgramPipelineEXT = glad_glIsProgramPipelineEXT;
    glad_debug_glIsQuery = glad_glIsQuery;
    glad_debug_glIsQueryEXT = glad_glIsQueryEXT;
    glad_debug_glIsRenderbuffer = glad_glIsRenderbuffer;
    glad_debug_glIsSampler = glad_glIsSampler;
    glad_debug_glIsSemaphoreEXT = glad_glIsSemaphoreEXT;
    glad_debug_glIsShader = glad_glIsShader;
    glad_debug_glIsSync = glad_glIsSync;
    glad_debug_glIsSyncAPPLE = glad_glIsSyncAPPLE;
    glad_debug_glIsTexture = glad_glIsTexture;
    glad_debug_glIsTextureHandleResidentNV = glad_glIsTextureHandleResidentNV;
    glad_debug_glIsTransformFeedback = glad_glIsTransformFeedback;
    glad_debug_glIsVertexArray = glad_glIsVertexArray;
    glad_debug_glIsVertexArrayOES = glad_glIsVertexArrayOES;
    glad_debug_glLabelObjectEXT = glad_glLabelObjectEXT;
    glad_debug_glLineWidth = glad_glLineWidth;
    glad_debug_glLinkProgram = glad_glLinkProgram;
    glad_debug_glMakeImageHandleNonResidentNV = glad_glMakeImageHandleNonResidentNV;
    glad_debug_glMakeImageHandleResidentNV = glad_glMakeImageHandleResidentNV;
    glad_debug_glMakeTextureHandleNonResidentNV = glad_glMakeTextureHandleNonResidentNV;
    glad_debug_glMakeTextureHandleResidentNV = glad_glMakeTextureHandleResidentNV;
    glad_debug_glMapBufferOES = glad_glMapBufferOES;
    glad_debug_glMapBufferRange = glad_glMapBufferRange;
    glad_debug_glMapBufferRangeEXT = glad_glMapBufferRangeEXT;
    glad_debug_glMatrixFrustumEXT = glad_glMatrixFrustumEXT;
    glad_debug_glMatrixLoad3x2fNV = glad_glMatrixLoad3x2fNV;
    glad_debug_glMatrixLoad3x3fNV = glad_glMatrixLoad3x3fNV;
    glad_debug_glMatrixLoadIdentityEXT = glad_glMatrixLoadIdentityEXT;
    glad_debug_glMatrixLoadTranspose3x3fNV = glad_glMatrixLoadTranspose3x3fNV;
    glad_debug_glMatrixLoadTransposedEXT = glad_glMatrixLoadTransposedEXT;
    glad_debug_glMatrixLoadTransposefEXT = glad_glMatrixLoadTransposefEXT;
    glad_debug_glMatrixLoaddEXT = glad_glMatrixLoaddEXT;
    glad_debug_glMatrixLoadfEXT = glad_glMatrixLoadfEXT;
    glad_debug_glMatrixMult3x2fNV = glad_glMatrixMult3x2fNV;
    glad_debug_glMatrixMult3x3fNV = glad_glMatrixMult3x3fNV;
    glad_debug_glMatrixMultTranspose3x3fNV = glad_glMatrixMultTranspose3x3fNV;
    glad_debug_glMatrixMultTransposedEXT = glad_glMatrixMultTransposedEXT;
    glad_debug_glMatrixMultTransposefEXT = glad_glMatrixMultTransposefEXT;
    glad_debug_glMatrixMultdEXT = glad_glMatrixMultdEXT;
    glad_debug_glMatrixMultfEXT = glad_glMatrixMultfEXT;
    glad_debug_glMatrixOrthoEXT = glad_glMatrixOrthoEXT;
    glad_debug_glMatrixPopEXT = glad_glMatrixPopEXT;
    glad_debug_glMatrixPushEXT = glad_glMatrixPushEXT;
    glad_debug_glMatrixRotatedEXT = glad_glMatrixRotatedEXT;
    glad_debug_glMatrixRotatefEXT = glad_glMatrixRotatefEXT;
    glad_debug_glMatrixScaledEXT = glad_glMatrixScaledEXT;
    glad_debug_glMatrixScalefEXT = glad_glMatrixScalefEXT;
    glad_debug_glMatrixTranslatedEXT = glad_glMatrixTranslatedEXT;
    glad_debug_glMatrixTranslatefEXT = glad_glMatrixTranslatefEXT;
    glad_debug_glMaxActiveShaderCoresARM = glad_glMaxActiveShaderCoresARM;
    glad_debug_glMaxShaderCompilerThreadsKHR = glad_glMaxShaderCompilerThreadsKHR;
    glad_debug_glMemoryBarrier = glad_glMemoryBarrier;
    glad_debug_glMemoryBarrierByRegion = glad_glMemoryBarrierByRegion;
    glad_debug_glMemoryObjectParameterivEXT = glad_glMemoryObjectParameterivEXT;
    glad_debug_glMinSampleShading = glad_glMinSampleShading;
    glad_debug_glMinSampleShadingOES = glad_glMinSampleShadingOES;
    glad_debug_glMultiDrawArraysEXT = glad_glMultiDrawArraysEXT;
    glad_debug_glMultiDrawArraysIndirectEXT = glad_glMultiDrawArraysIndirectEXT;
    glad_debug_glMultiDrawElementsBaseVertexEXT = glad_glMultiDrawElementsBaseVertexEXT;
    glad_debug_glMultiDrawElementsEXT = glad_glMultiDrawElementsEXT;
    glad_debug_glMultiDrawElementsIndirectEXT = glad_glMultiDrawElementsIndirectEXT;
    glad_debug_glMultiDrawMeshTasksIndirectCountEXT = glad_glMultiDrawMeshTasksIndirectCountEXT;
    glad_debug_glMultiDrawMeshTasksIndirectCountNV = glad_glMultiDrawMeshTasksIndirectCountNV;
    glad_debug_glMultiDrawMeshTasksIndirectEXT = glad_glMultiDrawMeshTasksIndirectEXT;
    glad_debug_glMultiDrawMeshTasksIndirectNV = glad_glMultiDrawMeshTasksIndirectNV;
    glad_debug_glNamedBufferAttachMemoryNV = glad_glNamedBufferAttachMemoryNV;
    glad_debug_glNamedBufferPageCommitmentMemNV = glad_glNamedBufferPageCommitmentMemNV;
    glad_debug_glNamedBufferStorageExternalEXT = glad_glNamedBufferStorageExternalEXT;
    glad_debug_glNamedBufferStorageMemEXT = glad_glNamedBufferStorageMemEXT;
    glad_debug_glNamedFramebufferSampleLocationsfvNV = glad_glNamedFramebufferSampleLocationsfvNV;
    glad_debug_glNamedFramebufferTextureMultiviewOVR = glad_glNamedFramebufferTextureMultiviewOVR;
    glad_debug_glNamedRenderbufferStorageMultisampleAdvancedAMD = glad_glNamedRenderbufferStorageMultisampleAdvancedAMD;
    glad_debug_glObjectLabel = glad_glObjectLabel;
    glad_debug_glObjectLabelKHR = glad_glObjectLabelKHR;
    glad_debug_glObjectPtrLabel = glad_glObjectPtrLabel;
    glad_debug_glObjectPtrLabelKHR = glad_glObjectPtrLabelKHR;
    glad_debug_glPatchParameteri = glad_glPatchParameteri;
    glad_debug_glPatchParameteriEXT = glad_glPatchParameteriEXT;
    glad_debug_glPatchParameteriOES = glad_glPatchParameteriOES;
    glad_debug_glPathCommandsNV = glad_glPathCommandsNV;
    glad_debug_glPathCoordsNV = glad_glPathCoordsNV;
    glad_debug_glPathCoverDepthFuncNV = glad_glPathCoverDepthFuncNV;
    glad_debug_glPathDashArrayNV = glad_glPathDashArrayNV;
    glad_debug_glPathGlyphIndexArrayNV = glad_glPathGlyphIndexArrayNV;
    glad_debug_glPathGlyphIndexRangeNV = glad_glPathGlyphIndexRangeNV;
    glad_debug_glPathGlyphRangeNV = glad_glPathGlyphRangeNV;
    glad_debug_glPathGlyphsNV = glad_glPathGlyphsNV;
    glad_debug_glPathMemoryGlyphIndexArrayNV = glad_glPathMemoryGlyphIndexArrayNV;
    glad_debug_glPathParameterfNV = glad_glPathParameterfNV;
    glad_debug_glPathParameterfvNV = glad_glPathParameterfvNV;
    glad_debug_glPathParameteriNV = glad_glPathParameteriNV;
    glad_debug_glPathParameterivNV = glad_glPathParameterivNV;
    glad_debug_glPathStencilDepthOffsetNV = glad_glPathStencilDepthOffsetNV;
    glad_debug_glPathStencilFuncNV = glad_glPathStencilFuncNV;
    glad_debug_glPathStringNV = glad_glPathStringNV;
    glad_debug_glPathSubCommandsNV = glad_glPathSubCommandsNV;
    glad_debug_glPathSubCoordsNV = glad_glPathSubCoordsNV;
    glad_debug_glPauseTransformFeedback = glad_glPauseTransformFeedback;
    glad_debug_glPixelStorei = glad_glPixelStorei;
    glad_debug_glPointAlongPathNV = glad_glPointAlongPathNV;
    glad_debug_glPolygonModeNV = glad_glPolygonModeNV;
    glad_debug_glPolygonOffset = glad_glPolygonOffset;
    glad_debug_glPolygonOffsetClampEXT = glad_glPolygonOffsetClampEXT;
    glad_debug_glPopDebugGroup = glad_glPopDebugGroup;
    glad_debug_glPopDebugGroupKHR = glad_glPopDebugGroupKHR;
    glad_debug_glPopGroupMarkerEXT = glad_glPopGroupMarkerEXT;
    glad_debug_glPrimitiveBoundingBox = glad_glPrimitiveBoundingBox;
    glad_debug_glPrimitiveBoundingBoxEXT = glad_glPrimitiveBoundingBoxEXT;
    glad_debug_glPrimitiveBoundingBoxOES = glad_glPrimitiveBoundingBoxOES;
    glad_debug_glProgramBinary = glad_glProgramBinary;
    glad_debug_glProgramBinaryOES = glad_glProgramBinaryOES;
    glad_debug_glProgramParameteri = glad_glProgramParameteri;
    glad_debug_glProgramParameteriEXT = glad_glProgramParameteriEXT;
    glad_debug_glProgramPathFragmentInputGenNV = glad_glProgramPathFragmentInputGenNV;
    glad_debug_glProgramUniform1f = glad_glProgramUniform1f;
    glad_debug_glProgramUniform1fEXT = glad_glProgramUniform1fEXT;
    glad_debug_glProgramUniform1fv = glad_glProgramUniform1fv;
    glad_debug_glProgramUniform1fvEXT = glad_glProgramUniform1fvEXT;
    glad_debug_glProgramUniform1i = glad_glProgramUniform1i;
    glad_debug_glProgramUniform1i64NV = glad_glProgramUniform1i64NV;
    glad_debug_glProgramUniform1i64vNV = glad_glProgramUniform1i64vNV;
    glad_debug_glProgramUniform1iEXT = glad_glProgramUniform1iEXT;
    glad_debug_glProgramUniform1iv = glad_glProgramUniform1iv;
    glad_debug_glProgramUniform1ivEXT = glad_glProgramUniform1ivEXT;
    glad_debug_glProgramUniform1ui = glad_glProgramUniform1ui;
    glad_debug_glProgramUniform1ui64NV = glad_glProgramUniform1ui64NV;
    glad_debug_glProgramUniform1ui64vNV = glad_glProgramUniform1ui64vNV;
    glad_debug_glProgramUniform1uiEXT = glad_glProgramUniform1uiEXT;
    glad_debug_glProgramUniform1uiv = glad_glProgramUniform1uiv;
    glad_debug_glProgramUniform1uivEXT = glad_glProgramUniform1uivEXT;
    glad_debug_glProgramUniform2f = glad_glProgramUniform2f;
    glad_debug_glProgramUniform2fEXT = glad_glProgramUniform2fEXT;
    glad_debug_glProgramUniform2fv = glad_glProgramUniform2fv;
    glad_debug_glProgramUniform2fvEXT = glad_glProgramUniform2fvEXT;
    glad_debug_glProgramUniform2i = glad_glProgramUniform2i;
    glad_debug_glProgramUniform2i64NV = glad_glProgramUniform2i64NV;
    glad_debug_glProgramUniform2i64vNV = glad_glProgramUniform2i64vNV;
    glad_debug_glProgramUniform2iEXT = glad_glProgramUniform2iEXT;
    glad_debug_glProgramUniform2iv = glad_glProgramUniform2iv;
    glad_debug_glProgramUniform2ivEXT = glad_glProgramUniform2ivEXT;
    glad_debug_glProgramUniform2ui = glad_glProgramUniform2ui;
    glad_debug_glProgramUniform2ui64NV = glad_glProgramUniform2ui64NV;
    glad_debug_glProgramUniform2ui64vNV = glad_glProgramUniform2ui64vNV;
    glad_debug_glProgramUniform2uiEXT = glad_glProgramUniform2uiEXT;
    glad_debug_glProgramUniform2uiv = glad_glProgramUniform2uiv;
    glad_debug_glProgramUniform2uivEXT = glad_glProgramUniform2uivEXT;
    glad_debug_glProgramUniform3f = glad_glProgramUniform3f;
    glad_debug_glProgramUniform3fEXT = glad_glProgramUniform3fEXT;
    glad_debug_glProgramUniform3fv = glad_glProgramUniform3fv;
    glad_debug_glProgramUniform3fvEXT = glad_glProgramUniform3fvEXT;
    glad_debug_glProgramUniform3i = glad_glProgramUniform3i;
    glad_debug_glProgramUniform3i64NV = glad_glProgramUniform3i64NV;
    glad_debug_glProgramUniform3i64vNV = glad_glProgramUniform3i64vNV;
    glad_debug_glProgramUniform3iEXT = glad_glProgramUniform3iEXT;
    glad_debug_glProgramUniform3iv = glad_glProgramUniform3iv;
    glad_debug_glProgramUniform3ivEXT = glad_glProgramUniform3ivEXT;
    glad_debug_glProgramUniform3ui = glad_glProgramUniform3ui;
    glad_debug_glProgramUniform3ui64NV = glad_glProgramUniform3ui64NV;
    glad_debug_glProgramUniform3ui64vNV = glad_glProgramUniform3ui64vNV;
    glad_debug_glProgramUniform3uiEXT = glad_glProgramUniform3uiEXT;
    glad_debug_glProgramUniform3uiv = glad_glProgramUniform3uiv;
    glad_debug_glProgramUniform3uivEXT = glad_glProgramUniform3uivEXT;
    glad_debug_glProgramUniform4f = glad_glProgramUniform4f;
    glad_debug_glProgramUniform4fEXT = glad_glProgramUniform4fEXT;
    glad_debug_glProgramUniform4fv = glad_glProgramUniform4fv;
    glad_debug_glProgramUniform4fvEXT = glad_glProgramUniform4fvEXT;
    glad_debug_glProgramUniform4i = glad_glProgramUniform4i;
    glad_debug_glProgramUniform4i64NV = glad_glProgramUniform4i64NV;
    glad_debug_glProgramUniform4i64vNV = glad_glProgramUniform4i64vNV;
    glad_debug_glProgramUniform4iEXT = glad_glProgramUniform4iEXT;
    glad_debug_glProgramUniform4iv = glad_glProgramUniform4iv;
    glad_debug_glProgramUniform4ivEXT = glad_glProgramUniform4ivEXT;
    glad_debug_glProgramUniform4ui = glad_glProgramUniform4ui;
    glad_debug_glProgramUniform4ui64NV = glad_glProgramUniform4ui64NV;
    glad_debug_glProgramUniform4ui64vNV = glad_glProgramUniform4ui64vNV;
    glad_debug_glProgramUniform4uiEXT = glad_glProgramUniform4uiEXT;
    glad_debug_glProgramUniform4uiv = glad_glProgramUniform4uiv;
    glad_debug_glProgramUniform4uivEXT = glad_glProgramUniform4uivEXT;
    glad_debug_glProgramUniformHandleui64IMG = glad_glProgramUniformHandleui64IMG;
    glad_debug_glProgramUniformHandleui64NV = glad_glProgramUniformHandleui64NV;
    glad_debug_glProgramUniformHandleui64vIMG = glad_glProgramUniformHandleui64vIMG;
    glad_debug_glProgramUniformHandleui64vNV = glad_glProgramUniformHandleui64vNV;
    glad_debug_glProgramUniformMatrix2fv = glad_glProgramUniformMatrix2fv;
    glad_debug_glProgramUniformMatrix2fvEXT = glad_glProgramUniformMatrix2fvEXT;
    glad_debug_glProgramUniformMatrix2x3fv = glad_glProgramUniformMatrix2x3fv;
    glad_debug_glProgramUniformMatrix2x3fvEXT = glad_glProgramUniformMatrix2x3fvEXT;
    glad_debug_glProgramUniformMatrix2x4fv = glad_glProgramUniformMatrix2x4fv;
    glad_debug_glProgramUniformMatrix2x4fvEXT = glad_glProgramUniformMatrix2x4fvEXT;
    glad_debug_glProgramUniformMatrix3fv = glad_glProgramUniformMatrix3fv;
    glad_debug_glProgramUniformMatrix3fvEXT = glad_glProgramUniformMatrix3fvEXT;
    glad_debug_glProgramUniformMatrix3x2fv = glad_glProgramUniformMatrix3x2fv;
    glad_debug_glProgramUniformMatrix3x2fvEXT = glad_glProgramUniformMatrix3x2fvEXT;
    glad_debug_glProgramUniformMatrix3x4fv = glad_glProgramUniformMatrix3x4fv;
    glad_debug_glProgramUniformMatrix3x4fvEXT = glad_glProgramUniformMatrix3x4fvEXT;
    glad_debug_glProgramUniformMatrix4fv = glad_glProgramUniformMatrix4fv;
    glad_debug_glProgramUniformMatrix4fvEXT = glad_glProgramUniformMatrix4fvEXT;
    glad_debug_glProgramUniformMatrix4x2fv = glad_glProgramUniformMatrix4x2fv;
    glad_debug_glProgramUniformMatrix4x2fvEXT = glad_glProgramUniformMatrix4x2fvEXT;
    glad_debug_glProgramUniformMatrix4x3fv = glad_glProgramUniformMatrix4x3fv;
    glad_debug_glProgramUniformMatrix4x3fvEXT = glad_glProgramUniformMatrix4x3fvEXT;
    glad_debug_glPushDebugGroup = glad_glPushDebugGroup;
    glad_debug_glPushDebugGroupKHR = glad_glPushDebugGroupKHR;
    glad_debug_glPushGroupMarkerEXT = glad_glPushGroupMarkerEXT;
    glad_debug_glQueryCounterEXT = glad_glQueryCounterEXT;
    glad_debug_glRasterSamplesEXT = glad_glRasterSamplesEXT;
    glad_debug_glReadBuffer = glad_glReadBuffer;
    glad_debug_glReadBufferIndexedEXT = glad_glReadBufferIndexedEXT;
    glad_debug_glReadBufferNV = glad_glReadBufferNV;
    glad_debug_glReadPixels = glad_glReadPixels;
    glad_debug_glReadnPixels = glad_glReadnPixels;
    glad_debug_glReadnPixelsEXT = glad_glReadnPixelsEXT;
    glad_debug_glReadnPixelsKHR = glad_glReadnPixelsKHR;
    glad_debug_glReleaseKeyedMutexWin32EXT = glad_glReleaseKeyedMutexWin32EXT;
    glad_debug_glReleaseShaderCompiler = glad_glReleaseShaderCompiler;
    glad_debug_glRenderbufferStorage = glad_glRenderbufferStorage;
    glad_debug_glRenderbufferStorageMultisample = glad_glRenderbufferStorageMultisample;
    glad_debug_glRenderbufferStorageMultisampleANGLE = glad_glRenderbufferStorageMultisampleANGLE;
    glad_debug_glRenderbufferStorageMultisampleAPPLE = glad_glRenderbufferStorageMultisampleAPPLE;
    glad_debug_glRenderbufferStorageMultisampleAdvancedAMD = glad_glRenderbufferStorageMultisampleAdvancedAMD;
    glad_debug_glRenderbufferStorageMultisampleEXT = glad_glRenderbufferStorageMultisampleEXT;
    glad_debug_glRenderbufferStorageMultisampleIMG = glad_glRenderbufferStorageMultisampleIMG;
    glad_debug_glRenderbufferStorageMultisampleNV = glad_glRenderbufferStorageMultisampleNV;
    glad_debug_glResetMemoryObjectParameterNV = glad_glResetMemoryObjectParameterNV;
    glad_debug_glResolveDepthValuesNV = glad_glResolveDepthValuesNV;
    glad_debug_glResolveMultisampleFramebufferAPPLE = glad_glResolveMultisampleFramebufferAPPLE;
    glad_debug_glResumeTransformFeedback = glad_glResumeTransformFeedback;
    glad_debug_glSampleCoverage = glad_glSampleCoverage;
    glad_debug_glSampleMaski = glad_glSampleMaski;
    glad_debug_glSamplerParameterIiv = glad_glSamplerParameterIiv;
    glad_debug_glSamplerParameterIivEXT = glad_glSamplerParameterIivEXT;
    glad_debug_glSamplerParameterIivOES = glad_glSamplerParameterIivOES;
    glad_debug_glSamplerParameterIuiv = glad_glSamplerParameterIuiv;
    glad_debug_glSamplerParameterIuivEXT = glad_glSamplerParameterIuivEXT;
    glad_debug_glSamplerParameterIuivOES = glad_glSamplerParameterIuivOES;
    glad_debug_glSamplerParameterf = glad_glSamplerParameterf;
    glad_debug_glSamplerParameterfv = glad_glSamplerParameterfv;
    glad_debug_glSamplerParameteri = glad_glSamplerParameteri;
    glad_debug_glSamplerParameteriv = glad_glSamplerParameteriv;
    glad_debug_glScissor = glad_glScissor;
    glad_debug_glScissorArrayvNV = glad_glScissorArrayvNV;
    glad_debug_glScissorArrayvOES = glad_glScissorArrayvOES;
    glad_debug_glScissorExclusiveArrayvNV = glad_glScissorExclusiveArrayvNV;
    glad_debug_glScissorExclusiveNV = glad_glScissorExclusiveNV;
    glad_debug_glScissorIndexedNV = glad_glScissorIndexedNV;
    glad_debug_glScissorIndexedOES = glad_glScissorIndexedOES;
    glad_debug_glScissorIndexedvNV = glad_glScissorIndexedvNV;
    glad_debug_glScissorIndexedvOES = glad_glScissorIndexedvOES;
    glad_debug_glSelectPerfMonitorCountersAMD = glad_glSelectPerfMonitorCountersAMD;
    glad_debug_glSemaphoreParameterivNV = glad_glSemaphoreParameterivNV;
    glad_debug_glSemaphoreParameterui64vEXT = glad_glSemaphoreParameterui64vEXT;
    glad_debug_glSetFenceNV = glad_glSetFenceNV;
    glad_debug_glShaderBinary = glad_glShaderBinary;
    glad_debug_glShaderSource = glad_glShaderSource;
    glad_debug_glShadingRateCombinerOpsEXT = glad_glShadingRateCombinerOpsEXT;
    glad_debug_glShadingRateEXT = glad_glShadingRateEXT;
    glad_debug_glShadingRateImageBarrierNV = glad_glShadingRateImageBarrierNV;
    glad_debug_glShadingRateImagePaletteNV = glad_glShadingRateImagePaletteNV;
    glad_debug_glShadingRateQCOM = glad_glShadingRateQCOM;
    glad_debug_glShadingRateSampleOrderCustomNV = glad_glShadingRateSampleOrderCustomNV;
    glad_debug_glShadingRateSampleOrderNV = glad_glShadingRateSampleOrderNV;
    glad_debug_glSignalSemaphoreEXT = glad_glSignalSemaphoreEXT;
    glad_debug_glSignalVkFenceNV = glad_glSignalVkFenceNV;
    glad_debug_glSignalVkSemaphoreNV = glad_glSignalVkSemaphoreNV;
    glad_debug_glStartTilingQCOM = glad_glStartTilingQCOM;
    glad_debug_glStencilFillPathInstancedNV = glad_glStencilFillPathInstancedNV;
    glad_debug_glStencilFillPathNV = glad_glStencilFillPathNV;
    glad_debug_glStencilFunc = glad_glStencilFunc;
    glad_debug_glStencilFuncSeparate = glad_glStencilFuncSeparate;
    glad_debug_glStencilMask = glad_glStencilMask;
    glad_debug_glStencilMaskSeparate = glad_glStencilMaskSeparate;
    glad_debug_glStencilOp = glad_glStencilOp;
    glad_debug_glStencilOpSeparate = glad_glStencilOpSeparate;
    glad_debug_glStencilStrokePathInstancedNV = glad_glStencilStrokePathInstancedNV;
    glad_debug_glStencilStrokePathNV = glad_glStencilStrokePathNV;
    glad_debug_glStencilThenCoverFillPathInstancedNV = glad_glStencilThenCoverFillPathInstancedNV;
    glad_debug_glStencilThenCoverFillPathNV = glad_glStencilThenCoverFillPathNV;
    glad_debug_glStencilThenCoverStrokePathInstancedNV = glad_glStencilThenCoverStrokePathInstancedNV;
    glad_debug_glStencilThenCoverStrokePathNV = glad_glStencilThenCoverStrokePathNV;
    glad_debug_glSubpixelPrecisionBiasNV = glad_glSubpixelPrecisionBiasNV;
    glad_debug_glTestFenceNV = glad_glTestFenceNV;
    glad_debug_glTexAttachMemoryNV = glad_glTexAttachMemoryNV;
    glad_debug_glTexBuffer = glad_glTexBuffer;
    glad_debug_glTexBufferEXT = glad_glTexBufferEXT;
    glad_debug_glTexBufferOES = glad_glTexBufferOES;
    glad_debug_glTexBufferRange = glad_glTexBufferRange;
    glad_debug_glTexBufferRangeEXT = glad_glTexBufferRangeEXT;
    glad_debug_glTexBufferRangeOES = glad_glTexBufferRangeOES;
    glad_debug_glTexEstimateMotionQCOM = glad_glTexEstimateMotionQCOM;
    glad_debug_glTexEstimateMotionRegionsQCOM = glad_glTexEstimateMotionRegionsQCOM;
    glad_debug_glTexImage2D = glad_glTexImage2D;
    glad_debug_glTexImage3D = glad_glTexImage3D;
    glad_debug_glTexImage3DOES = glad_glTexImage3DOES;
    glad_debug_glTexPageCommitmentEXT = glad_glTexPageCommitmentEXT;
    glad_debug_glTexPageCommitmentMemNV = glad_glTexPageCommitmentMemNV;
    glad_debug_glTexParameterIiv = glad_glTexParameterIiv;
    glad_debug_glTexParameterIivEXT = glad_glTexParameterIivEXT;
    glad_debug_glTexParameterIivOES = glad_glTexParameterIivOES;
    glad_debug_glTexParameterIuiv = glad_glTexParameterIuiv;
    glad_debug_glTexParameterIuivEXT = glad_glTexParameterIuivEXT;
    glad_debug_glTexParameterIuivOES = glad_glTexParameterIuivOES;
    glad_debug_glTexParameterf = glad_glTexParameterf;
    glad_debug_glTexParameterfv = glad_glTexParameterfv;
    glad_debug_glTexParameteri = glad_glTexParameteri;
    glad_debug_glTexParameteriv = glad_glTexParameteriv;
    glad_debug_glTexStorage1DEXT = glad_glTexStorage1DEXT;
    glad_debug_glTexStorage2D = glad_glTexStorage2D;
    glad_debug_glTexStorage2DEXT = glad_glTexStorage2DEXT;
    glad_debug_glTexStorage2DMultisample = glad_glTexStorage2DMultisample;
    glad_debug_glTexStorage3D = glad_glTexStorage3D;
    glad_debug_glTexStorage3DEXT = glad_glTexStorage3DEXT;
    glad_debug_glTexStorage3DMultisample = glad_glTexStorage3DMultisample;
    glad_debug_glTexStorage3DMultisampleOES = glad_glTexStorage3DMultisampleOES;
    glad_debug_glTexStorageAttribs2DEXT = glad_glTexStorageAttribs2DEXT;
    glad_debug_glTexStorageAttribs3DEXT = glad_glTexStorageAttribs3DEXT;
    glad_debug_glTexStorageMem2DEXT = glad_glTexStorageMem2DEXT;
    glad_debug_glTexStorageMem2DMultisampleEXT = glad_glTexStorageMem2DMultisampleEXT;
    glad_debug_glTexStorageMem3DEXT = glad_glTexStorageMem3DEXT;
    glad_debug_glTexStorageMem3DMultisampleEXT = glad_glTexStorageMem3DMultisampleEXT;
    glad_debug_glTexSubImage2D = glad_glTexSubImage2D;
    glad_debug_glTexSubImage3D = glad_glTexSubImage3D;
    glad_debug_glTexSubImage3DOES = glad_glTexSubImage3DOES;
    glad_debug_glTextureAttachMemoryNV = glad_glTextureAttachMemoryNV;
    glad_debug_glTextureBarrierNV = glad_glTextureBarrierNV;
    glad_debug_glTextureFoveationParametersQCOM = glad_glTextureFoveationParametersQCOM;
    glad_debug_glTexturePageCommitmentMemNV = glad_glTexturePageCommitmentMemNV;
    glad_debug_glTextureStorage1DEXT = glad_glTextureStorage1DEXT;
    glad_debug_glTextureStorage2DEXT = glad_glTextureStorage2DEXT;
    glad_debug_glTextureStorage3DEXT = glad_glTextureStorage3DEXT;
    glad_debug_glTextureStorageMem2DEXT = glad_glTextureStorageMem2DEXT;
    glad_debug_glTextureStorageMem2DMultisampleEXT = glad_glTextureStorageMem2DMultisampleEXT;
    glad_debug_glTextureStorageMem3DEXT = glad_glTextureStorageMem3DEXT;
    glad_debug_glTextureStorageMem3DMultisampleEXT = glad_glTextureStorageMem3DMultisampleEXT;
    glad_debug_glTextureViewEXT = glad_glTextureViewEXT;
    glad_debug_glTextureViewOES = glad_glTextureViewOES;
    glad_debug_glTransformFeedbackVaryings = glad_glTransformFeedbackVaryings;
    glad_debug_glTransformPathNV = glad_glTransformPathNV;
    glad_debug_glUniform1f = glad_glUniform1f;
    glad_debug_glUniform1fv = glad_glUniform1fv;
    glad_debug_glUniform1i = glad_glUniform1i;
    glad_debug_glUniform1i64NV = glad_glUniform1i64NV;
    glad_debug_glUniform1i64vNV = glad_glUniform1i64vNV;
    glad_debug_glUniform1iv = glad_glUniform1iv;
    glad_debug_glUniform1ui = glad_glUniform1ui;
    glad_debug_glUniform1ui64NV = glad_glUniform1ui64NV;
    glad_debug_glUniform1ui64vNV = glad_glUniform1ui64vNV;
    glad_debug_glUniform1uiv = glad_glUniform1uiv;
    glad_debug_glUniform2f = glad_glUniform2f;
    glad_debug_glUniform2fv = glad_glUniform2fv;
    glad_debug_glUniform2i = glad_glUniform2i;
    glad_debug_glUniform2i64NV = glad_glUniform2i64NV;
    glad_debug_glUniform2i64vNV = glad_glUniform2i64vNV;
    glad_debug_glUniform2iv = glad_glUniform2iv;
    glad_debug_glUniform2ui = glad_glUniform2ui;
    glad_debug_glUniform2ui64NV = glad_glUniform2ui64NV;
    glad_debug_glUniform2ui64vNV = glad_glUniform2ui64vNV;
    glad_debug_glUniform2uiv = glad_glUniform2uiv;
    glad_debug_glUniform3f = glad_glUniform3f;
    glad_debug_glUniform3fv = glad_glUniform3fv;
    glad_debug_glUniform3i = glad_glUniform3i;
    glad_debug_glUniform3i64NV = glad_glUniform3i64NV;
    glad_debug_glUniform3i64vNV = glad_glUniform3i64vNV;
    glad_debug_glUniform3iv = glad_glUniform3iv;
    glad_debug_glUniform3ui = glad_glUniform3ui;
    glad_debug_glUniform3ui64NV = glad_glUniform3ui64NV;
    glad_debug_glUniform3ui64vNV = glad_glUniform3ui64vNV;
    glad_debug_glUniform3uiv = glad_glUniform3uiv;
    glad_debug_glUniform4f = glad_glUniform4f;
    glad_debug_glUniform4fv = glad_glUniform4fv;
    glad_debug_glUniform4i = glad_glUniform4i;
    glad_debug_glUniform4i64NV = glad_glUniform4i64NV;
    glad_debug_glUniform4i64vNV = glad_glUniform4i64vNV;
    glad_debug_glUniform4iv = glad_glUniform4iv;
    glad_debug_glUniform4ui = glad_glUniform4ui;
    glad_debug_glUniform4ui64NV = glad_glUniform4ui64NV;
    glad_debug_glUniform4ui64vNV = glad_glUniform4ui64vNV;
    glad_debug_glUniform4uiv = glad_glUniform4uiv;
    glad_debug_glUniformBlockBinding = glad_glUniformBlockBinding;
    glad_debug_glUniformHandleui64IMG = glad_glUniformHandleui64IMG;
    glad_debug_glUniformHandleui64NV = glad_glUniformHandleui64NV;
    glad_debug_glUniformHandleui64vIMG = glad_glUniformHandleui64vIMG;
    glad_debug_glUniformHandleui64vNV = glad_glUniformHandleui64vNV;
    glad_debug_glUniformMatrix2fv = glad_glUniformMatrix2fv;
    glad_debug_glUniformMatrix2x3fv = glad_glUniformMatrix2x3fv;
    glad_debug_glUniformMatrix2x3fvNV = glad_glUniformMatrix2x3fvNV;
    glad_debug_glUniformMatrix2x4fv = glad_glUniformMatrix2x4fv;
    glad_debug_glUniformMatrix2x4fvNV = glad_glUniformMatrix2x4fvNV;
    glad_debug_glUniformMatrix3fv = glad_glUniformMatrix3fv;
    glad_debug_glUniformMatrix3x2fv = glad_glUniformMatrix3x2fv;
    glad_debug_glUniformMatrix3x2fvNV = glad_glUniformMatrix3x2fvNV;
    glad_debug_glUniformMatrix3x4fv = glad_glUniformMatrix3x4fv;
    glad_debug_glUniformMatrix3x4fvNV = glad_glUniformMatrix3x4fvNV;
    glad_debug_glUniformMatrix4fv = glad_glUniformMatrix4fv;
    glad_debug_glUniformMatrix4x2fv = glad_glUniformMatrix4x2fv;
    glad_debug_glUniformMatrix4x2fvNV = glad_glUniformMatrix4x2fvNV;
    glad_debug_glUniformMatrix4x3fv = glad_glUniformMatrix4x3fv;
    glad_debug_glUniformMatrix4x3fvNV = glad_glUniformMatrix4x3fvNV;
    glad_debug_glUnmapBuffer = glad_glUnmapBuffer;
    glad_debug_glUnmapBufferOES = glad_glUnmapBufferOES;
    glad_debug_glUseProgram = glad_glUseProgram;
    glad_debug_glUseProgramStages = glad_glUseProgramStages;
    glad_debug_glUseProgramStagesEXT = glad_glUseProgramStagesEXT;
    glad_debug_glValidateProgram = glad_glValidateProgram;
    glad_debug_glValidateProgramPipeline = glad_glValidateProgramPipeline;
    glad_debug_glValidateProgramPipelineEXT = glad_glValidateProgramPipelineEXT;
    glad_debug_glVertexAttrib1f = glad_glVertexAttrib1f;
    glad_debug_glVertexAttrib1fv = glad_glVertexAttrib1fv;
    glad_debug_glVertexAttrib2f = glad_glVertexAttrib2f;
    glad_debug_glVertexAttrib2fv = glad_glVertexAttrib2fv;
    glad_debug_glVertexAttrib3f = glad_glVertexAttrib3f;
    glad_debug_glVertexAttrib3fv = glad_glVertexAttrib3fv;
    glad_debug_glVertexAttrib4f = glad_glVertexAttrib4f;
    glad_debug_glVertexAttrib4fv = glad_glVertexAttrib4fv;
    glad_debug_glVertexAttribBinding = glad_glVertexAttribBinding;
    glad_debug_glVertexAttribDivisor = glad_glVertexAttribDivisor;
    glad_debug_glVertexAttribDivisorANGLE = glad_glVertexAttribDivisorANGLE;
    glad_debug_glVertexAttribDivisorEXT = glad_glVertexAttribDivisorEXT;
    glad_debug_glVertexAttribDivisorNV = glad_glVertexAttribDivisorNV;
    glad_debug_glVertexAttribFormat = glad_glVertexAttribFormat;
    glad_debug_glVertexAttribI4i = glad_glVertexAttribI4i;
    glad_debug_glVertexAttribI4iv = glad_glVertexAttribI4iv;
    glad_debug_glVertexAttribI4ui = glad_glVertexAttribI4ui;
    glad_debug_glVertexAttribI4uiv = glad_glVertexAttribI4uiv;
    glad_debug_glVertexAttribIFormat = glad_glVertexAttribIFormat;
    glad_debug_glVertexAttribIPointer = glad_glVertexAttribIPointer;
    glad_debug_glVertexAttribPointer = glad_glVertexAttribPointer;
    glad_debug_glVertexBindingDivisor = glad_glVertexBindingDivisor;
    glad_debug_glViewport = glad_glViewport;
    glad_debug_glViewportArrayvNV = glad_glViewportArrayvNV;
    glad_debug_glViewportArrayvOES = glad_glViewportArrayvOES;
    glad_debug_glViewportIndexedfNV = glad_glViewportIndexedfNV;
    glad_debug_glViewportIndexedfOES = glad_glViewportIndexedfOES;
    glad_debug_glViewportIndexedfvNV = glad_glViewportIndexedfvNV;
    glad_debug_glViewportIndexedfvOES = glad_glViewportIndexedfvOES;
    glad_debug_glViewportPositionWScaleNV = glad_glViewportPositionWScaleNV;
    glad_debug_glViewportSwizzleNV = glad_glViewportSwizzleNV;
    glad_debug_glWaitSemaphoreEXT = glad_glWaitSemaphoreEXT;
    glad_debug_glWaitSync = glad_glWaitSync;
    glad_debug_glWaitSyncAPPLE = glad_glWaitSyncAPPLE;
    glad_debug_glWaitVkSemaphoreNV = glad_glWaitVkSemaphoreNV;
    glad_debug_glWeightPathsNV = glad_glWeightPathsNV;
    glad_debug_glWindowRectanglesEXT = glad_glWindowRectanglesEXT;
}

#ifdef GLAD_GLES2

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

#if GLAD_PLATFORM_EMSCRIPTEN
#ifndef GLAD_EGL_H_
  typedef void (*__eglMustCastToProperFunctionPointerType)(void);
  typedef __eglMustCastToProperFunctionPointerType (GLAD_API_PTR *PFNEGLGETPROCADDRESSPROC)(const char *name);
#endif
  extern __eglMustCastToProperFunctionPointerType emscripten_GetProcAddress(const char *name);
#elif defined(GLAD_GLES2_USE_SYSTEM_EGL)
  #include <EGL/egl.h>
  typedef __eglMustCastToProperFunctionPointerType (GLAD_API_PTR *PFNEGLGETPROCADDRESSPROC)(const char *name);
#else
  #include <glad/egl.h>
#endif


struct _glad_gles2_userptr {
    void *handle;
    PFNEGLGETPROCADDRESSPROC get_proc_address_ptr;
};


static GLADapiproc glad_gles2_get_proc(void *vuserptr, const char* name) {
    struct _glad_gles2_userptr userptr = *(struct _glad_gles2_userptr*) vuserptr;
    GLADapiproc result = NULL;

#if GLAD_PLATFORM_EMSCRIPTEN
    GLAD_UNUSED(&glad_dlsym_handle);
#else
    result = glad_dlsym_handle(userptr.handle, name);
#endif
    if (result == NULL) {
        result = userptr.get_proc_address_ptr(name);
    }

    return result;
}

static void* _glad_GLES2_loader_handle = NULL;

static void* glad_gles2_dlopen_handle(void) {
#if GLAD_PLATFORM_EMSCRIPTEN
#elif GLAD_PLATFORM_APPLE
    static const char *NAMES[] = {"libGLESv2.dylib"};
#elif GLAD_PLATFORM_WIN32
    static const char *NAMES[] = {"GLESv2.dll", "libGLESv2.dll"};
#else
    static const char *NAMES[] = {"libGLESv2.so.2", "libGLESv2.so"};
#endif

#if GLAD_PLATFORM_EMSCRIPTEN
    GLAD_UNUSED(&glad_get_dlopen_handle);
    return NULL;
#else
    if (_glad_GLES2_loader_handle == NULL) {
        _glad_GLES2_loader_handle = glad_get_dlopen_handle(NAMES, sizeof(NAMES) / sizeof(NAMES[0]));
    }

    return _glad_GLES2_loader_handle;
#endif
}

static struct _glad_gles2_userptr glad_gles2_build_userptr(void *handle) {
    struct _glad_gles2_userptr userptr;
#if GLAD_PLATFORM_EMSCRIPTEN
    GLAD_UNUSED(handle);
    userptr.get_proc_address_ptr = emscripten_GetProcAddress;
#else
    userptr.handle = handle;
    userptr.get_proc_address_ptr = eglGetProcAddress;
#endif
    return userptr;
}

int gladLoaderLoadGLES2(void) {
    int version = 0;
    void *handle = NULL;
    int did_load = 0;
    struct _glad_gles2_userptr userptr;

#if GLAD_PLATFORM_EMSCRIPTEN
    GLAD_UNUSED(handle);
    GLAD_UNUSED(did_load);
    GLAD_UNUSED(&glad_gles2_dlopen_handle);
    GLAD_UNUSED(&glad_gles2_build_userptr);
    userptr.get_proc_address_ptr = emscripten_GetProcAddress;
    version = gladLoadGLES2UserPtr(glad_gles2_get_proc, &userptr);
#else
#ifndef GLAD_GLES2_USE_SYSTEM_EGL
    if (eglGetProcAddress == NULL) {
        return 0;
    }
#endif
    did_load = _glad_GLES2_loader_handle == NULL;
    handle = glad_gles2_dlopen_handle();
    if (handle != NULL) {
        userptr = glad_gles2_build_userptr(handle);

        version = gladLoadGLES2UserPtr(glad_gles2_get_proc, &userptr);

        if (!version && did_load) {
            gladLoaderUnloadGLES2();
        }
    }
#endif

    return version;
}



void gladLoaderUnloadGLES2(void) {
    if (_glad_GLES2_loader_handle != NULL) {
        glad_close_dlopen_handle(_glad_GLES2_loader_handle);
        _glad_GLES2_loader_handle = NULL;
    }
}

#endif /* GLAD_GLES2 */

#ifdef __cplusplus
}
#endif
