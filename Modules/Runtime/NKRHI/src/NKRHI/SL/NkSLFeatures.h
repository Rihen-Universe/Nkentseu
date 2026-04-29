#pragma once
// =============================================================================
// NkSLFeatures.h  — v4.0
//
// NOUVEAUTÉS :
//   - NkSLFeatureCaps::ForTarget() étendu pour NK_GLSL_VULKAN, NK_HLSL_DX11,
//     NK_HLSL_DX12 avec les bonnes capacités par API/SM
//   - NkSLFeatureCaps::ShaderModelDX12() : retourne les caps selon le SM DX12
// =============================================================================
#include "NkSLTypes.h"

namespace nkentseu {

// =============================================================================
// Stages avancés (extensions de NkSLStage)
// =============================================================================
constexpr uint32 NK_SL_STAGE_TASK      = 6;
constexpr uint32 NK_SL_STAGE_MESH      = 7;
constexpr uint32 NK_SL_STAGE_RAY_GEN   = 8;
constexpr uint32 NK_SL_STAGE_ANY_HIT   = 9;
constexpr uint32 NK_SL_STAGE_CLOSEST   = 10;
constexpr uint32 NK_SL_STAGE_MISS      = 11;
constexpr uint32 NK_SL_STAGE_INTERSECT = 12;
constexpr uint32 NK_SL_STAGE_CALLABLE  = 13;

// =============================================================================
// Capacités par cible de compilation
// =============================================================================
struct NkSLFeatureCaps {
    bool bindless            = false; // Bindless resources
    bool meshShaders         = false; // Mesh + Task shaders
    bool rayTracing          = false; // Ray tracing
    bool tessellation        = true;  // Hull/domain shaders
    bool geometryShaders     = true;  // Geometry shaders
    bool computeAtomics      = true;  // Atomic image/buffer ops
    bool waveIntrinsics      = false; // Wave/subgroup operations (SM6.0+ / SPV)
    bool int64Ops            = false; // 64-bit integers
    bool fp16Ops             = false; // 16-bit floats
    bool variableRateShading = false; // VRS
    bool subpassInput        = false; // Vulkan input attachments (subpassLoad)
    bool pushConstants       = false; // Vulkan push constants
    bool drawParams          = false; // gl_BaseVertex/Instance (Vulkan/ARB)
    bool specializationConst = false; // Vulkan specialization constants
    bool rootSignature       = false; // DX12 inline root signature
    bool bindlessHeapSM66    = false; // DX12 SM6.6 ResourceDescriptorHeap

    // ==========================================================================
    // Capacités par target
    // ==========================================================================
    static NkSLFeatureCaps ForTarget(NkSLTarget t,
                                      uint32 hlslSM = 50) {
        NkSLFeatureCaps caps;
        switch (t) {

            // GLSL OpenGL 4.30+ — capacités classiques, pas d'extensions Vulkan
            case NkSLTarget::NK_GLSL:
                caps.tessellation    = true;
                caps.geometryShaders = true;
                caps.computeAtomics  = true;
                caps.bindless        = false;  // pas de bindless standard en GL (sauf ARB_bindless_texture)
                caps.meshShaders     = false;
                caps.rayTracing      = false;
                caps.subpassInput    = false;
                caps.pushConstants   = false;
                caps.waveIntrinsics  = false;
                caps.drawParams      = false;
                break;

            // GLSL Vulkan 4.50+ — extensions Vulkan activées
            case NkSLTarget::NK_GLSL_VULKAN:
                caps.tessellation       = true;
                caps.geometryShaders    = true;
                caps.computeAtomics     = true;
                caps.subpassInput       = true;  // layout(input_attachment_index=N)
                caps.pushConstants      = true;  // layout(push_constant)
                caps.drawParams         = true;  // GL_ARB_shader_draw_parameters
                caps.specializationConst= true;  // layout(constant_id=N) const
                caps.waveIntrinsics     = false; // via GL_KHR_shader_subgroup (optionnel)
                caps.bindless           = false; // via VK_EXT_descriptor_indexing (hors scope)
                caps.meshShaders        = false; // via NV_mesh_shader (hors scope)
                caps.rayTracing         = false; // via KHR_ray_tracing (hors scope)
                break;

            // SPIR-V — capacités Vulkan complètes
            case NkSLTarget::NK_SPIRV:
                caps.tessellation       = true;
                caps.geometryShaders    = true;
                caps.computeAtomics     = true;
                caps.meshShaders        = true;  // Vulkan 1.2+ avec extension
                caps.rayTracing         = true;  // KHR_ray_tracing
                caps.bindless           = true;  // VK_EXT_descriptor_indexing
                caps.waveIntrinsics     = true;  // KHR_shader_subgroup
                caps.int64Ops           = true;
                caps.fp16Ops            = true;
                caps.subpassInput       = true;
                caps.pushConstants      = true;
                caps.drawParams         = true;
                caps.specializationConst= true;
                break;

            // HLSL DX11 — SM5, fxc, pas de wave/mesh/RT
            case NkSLTarget::NK_HLSL_DX11:
                caps.tessellation       = true;
                caps.geometryShaders    = true;
                caps.computeAtomics     = true;
                caps.meshShaders        = false; // SM6.5+ seulement
                caps.rayTracing         = false; // SM6.3+ (DXR) seulement
                caps.bindless           = false; // SM6.6 seulement
                caps.waveIntrinsics     = false; // SM6.0+ seulement
                caps.int64Ops           = false;
                caps.fp16Ops            = false;
                caps.rootSignature      = false;
                caps.bindlessHeapSM66   = false;
                break;

            // HLSL DX12 — SM6+, capacités selon la version du SM
            case NkSLTarget::NK_HLSL_DX12:
                caps.tessellation       = true;
                caps.geometryShaders    = true;
                caps.computeAtomics     = true;
                caps.waveIntrinsics     = (hlslSM >= 60); // SM6.0+
                caps.rayTracing         = (hlslSM >= 63); // SM6.3+ (DXR)
                caps.meshShaders        = (hlslSM >= 65); // SM6.5+
                caps.bindless           = (hlslSM >= 66); // SM6.6+
                caps.bindlessHeapSM66   = (hlslSM >= 66);
                caps.int64Ops           = (hlslSM >= 60);
                caps.fp16Ops            = (hlslSM >= 62); // SM6.2+
                caps.rootSignature      = true;
                caps.variableRateShading= (hlslSM >= 64); // SM6.4+ (VRS)
                break;

            // MSL — Metal
            case NkSLTarget::NK_MSL:
            case NkSLTarget::NK_MSL_SPIRV_CROSS:
                caps.tessellation       = false; // pas de tessellation native < Metal 3
                caps.geometryShaders    = false; // pas de geometry shaders Metal
                caps.computeAtomics     = true;
                caps.meshShaders        = true;  // Metal 3 mesh shaders
                caps.rayTracing         = true;  // Metal ray tracing (A14+/M1+)
                caps.bindless           = true;  // Argument buffers Tier 2
                caps.waveIntrinsics     = true;  // SIMD-group operations
                caps.fp16Ops            = true;
                break;

            default:
                break;
        }
        return caps;
    }

    // Alias pour DX12 selon le SM exact
    static NkSLFeatureCaps ForHLSL_DX12(uint32 shaderModel) {
        return ForTarget(NkSLTarget::NK_HLSL_DX12, shaderModel);
    }
};

// =============================================================================
// Mesh shader
// =============================================================================
struct NkSLMeshShaderLayout {
    uint32   maxVertices   = 128;
    uint32   maxPrimitives = 128;
    NkString topology      = "triangle";
    uint32   groupSizeX    = 128;
    uint32   groupSizeY    = 1;
    uint32   groupSizeZ    = 1;
};

NkString GenerateMeshShaderHLSL(const NkString& meshEntrySource,
                                  const NkSLMeshShaderLayout& layout,
                                  const NkSLCompileOptions& opts);

NkString GenerateMeshShaderMSL (const NkString& meshEntrySource,
                                  const NkSLMeshShaderLayout& layout,
                                  const NkSLCompileOptions& opts);

// =============================================================================
// Tessellation (DX12 SM6 utilise le même mécanisme que DX11)
// =============================================================================
struct NkSLTessLayout {
    NkString domain              = "tri";
    NkString partitioning        = "fractional_even";
    NkString outputTopology      = "triangle_cw";
    uint32   outputControlPoints = 3;
    NkString patchConstantFunc   = "PatchConstantFunc";
    float    defaultTessFactor   = 4.0f;
};

NkString GenerateTessControlHLSL(const NkString& source,
                                   const NkSLTessLayout& layout,
                                   const NkSLCompileOptions& opts);

NkString GenerateTessEvalHLSL   (const NkString& source,
                                   const NkSLTessLayout& layout,
                                   const NkSLCompileOptions& opts);

// =============================================================================
// Ray tracing
// =============================================================================
struct NkSLRayTracingConfig {
    uint32 maxRecursionDepth = 1;
    bool   useInlineRT       = false;
};

NkString GenerateRayGenHLSL    (const NkString& src, const NkSLCompileOptions& opts);
NkString GenerateClosestHitHLSL(const NkString& src, const NkSLCompileOptions& opts);
NkString GenerateMissHLSL      (const NkString& src, const NkSLCompileOptions& opts);
NkString GenerateAnyHitHLSL    (const NkString& src, const NkSLCompileOptions& opts);

// =============================================================================
// Bindless
// =============================================================================
struct NkSLBindlessConfig {
    bool   useSM66Heap        = true;
    bool   useArgumentBuffers = true;
    uint32 maxDescriptors     = 1000000;
};

NkString GenerateBindlessHLSL(const NkSLBindlessConfig& cfg);
NkString GenerateBindlessMSL (const NkSLBindlessConfig& cfg);

} // namespace nkentseu
