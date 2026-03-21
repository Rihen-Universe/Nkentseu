#pragma once
// =============================================================================
// NkSLFeatures.h
// Déclarations des features avancées NkSL :
//   - Bindless resources (HLSL ResourceDescriptorHeap, MSL argument buffers)
//   - Mesh + Task shaders (Vulkan mesh shading, DX12 mesh shader, Metal 3)
//   - Ray tracing (Vulkan KHR_ray_tracing, DXR)
//   - Tessellation complète (HLSL hull/domain, MSL post-tessellation)
//   - StructuredBuffer / AppendStructuredBuffer / ConsumeStructuredBuffer
// =============================================================================
#include "NkSLTypes.h"

namespace nkentseu {

// =============================================================================
// Stages avancés
// =============================================================================
// (Ajouts à NkSLStage — ces valeurs doivent correspondre à l'enum étendu)
constexpr uint32 NK_SL_STAGE_TASK      = 6;  // Mesh shader task/amplification
constexpr uint32 NK_SL_STAGE_MESH      = 7;  // Mesh shader
constexpr uint32 NK_SL_STAGE_RAY_GEN   = 8;  // Ray generation
constexpr uint32 NK_SL_STAGE_ANY_HIT   = 9;  // Any hit
constexpr uint32 NK_SL_STAGE_CLOSEST   = 10; // Closest hit
constexpr uint32 NK_SL_STAGE_MISS      = 11; // Miss
constexpr uint32 NK_SL_STAGE_INTERSECT = 12; // Intersection
constexpr uint32 NK_SL_STAGE_CALLABLE  = 13; // Callable

// =============================================================================
// Types avancés (à intégrer dans NkSLBaseType)
// =============================================================================
// Pour les ray tracing et mesh shaders, des types spéciaux sont nécessaires.
// Ces constantes sont utilisées dans le codegen pour la détection de types avancés.
constexpr uint32 NK_SL_TYPE_ACCELERATION_STRUCTURE = 100;
constexpr uint32 NK_SL_TYPE_RAY_QUERY              = 101;
constexpr uint32 NK_SL_TYPE_MESH_VERTEX_OUTPUT     = 102;
constexpr uint32 NK_SL_TYPE_MESH_PRIMITIVE_OUTPUT  = 103;

// =============================================================================
// Options de feature par API
// =============================================================================
struct NkSLFeatureCaps {
    bool bindless           = false; // ResourceDescriptorHeap / argument buffers
    bool meshShaders        = false; // Mesh + Task shaders
    bool rayTracing         = false; // DXR / VK_KHR_ray_tracing
    bool tessellation       = true;  // Hull/domain shaders
    bool geometryShaders    = true;  // Geometry shaders
    bool computeAtomics     = true;  // Atomic image/buffer ops
    bool waveIntrinsics     = false; // Wave/subgroup operations
    bool int64Ops           = false; // 64-bit integers
    bool fp16Ops            = false; // 16-bit floats
    bool variableRateShading= false; // VRS

    static NkSLFeatureCaps ForTarget(NkSLTarget t) {
        NkSLFeatureCaps caps;
        switch (t) {
            case NkSLTarget::GLSL:
                caps.tessellation    = true;
                caps.geometryShaders = true;
                caps.computeAtomics  = true;
                caps.bindless        = false;
                caps.meshShaders     = false;
                caps.rayTracing      = false;
                break;
            case NkSLTarget::SPIRV:
                caps.tessellation    = true;
                caps.geometryShaders = true;
                caps.computeAtomics  = true;
                caps.meshShaders     = true;  // Vulkan 1.2+ avec extension
                caps.rayTracing      = true;  // KHR_ray_tracing
                caps.bindless        = true;  // VK_EXT_descriptor_indexing
                caps.waveIntrinsics  = true;
                caps.int64Ops        = true;
                caps.fp16Ops         = true;
                break;
            case NkSLTarget::HLSL:
                caps.tessellation    = true;
                caps.geometryShaders = true;
                caps.computeAtomics  = true;
                caps.meshShaders     = true;  // DX12 SM6.5+
                caps.rayTracing      = true;  // DXR SM6.3+
                caps.bindless        = true;  // SM6.6 ResourceDescriptorHeap
                caps.waveIntrinsics  = true;  // SM6.0+
                caps.int64Ops        = true;
                caps.fp16Ops         = true;
                break;
            case NkSLTarget::MSL:
                caps.tessellation    = false; // Pas de tessellation native Metal < 3
                caps.geometryShaders = false; // Pas de geometry shaders en Metal
                caps.computeAtomics  = true;
                caps.meshShaders     = true;  // Metal 3 mesh shaders
                caps.rayTracing      = true;  // Metal ray tracing (A14+/M1+)
                caps.bindless        = true;  // Argument buffers Tier 2
                caps.waveIntrinsics  = true;  // SIMD-group operations
                caps.fp16Ops         = true;
                break;
            default:
                break;
        }
        return caps;
    }
};

// =============================================================================
// Génération HLSL — Mesh shader
// =============================================================================
struct NkSLMeshShaderLayout {
    uint32 maxVertices   = 128;
    uint32 maxPrimitives = 128;
    // topologie : "line", "triangle"
    NkString topology    = "triangle";
    // thread group size
    uint32 groupSizeX    = 128;
    uint32 groupSizeY    = 1;
    uint32 groupSizeZ    = 1;
};

NkString GenerateMeshShaderHLSL(
    const NkString&           meshEntrySource,
    const NkSLMeshShaderLayout& layout,
    const NkSLCompileOptions&  opts);

NkString GenerateMeshShaderMSL(
    const NkString&           meshEntrySource,
    const NkSLMeshShaderLayout& layout,
    const NkSLCompileOptions&  opts);

// =============================================================================
// Génération HLSL — Tessellation (hull + domain)
// =============================================================================
struct NkSLTessLayout {
    // Topologie du patch : "tri", "quad", "isoline"
    NkString domain           = "tri";
    // Partitioning : "integer", "fractional_even", "fractional_odd", "pow2"
    NkString partitioning     = "fractional_even";
    // Topologie de sortie : "point", "line", "triangle_cw", "triangle_ccw"
    NkString outputTopology   = "triangle_cw";
    // Nombre de control points
    uint32   outputControlPoints = 3;
    // Fonction de constante de patch
    NkString patchConstantFunc   = "PatchConstantFunc";
    // Tess factors
    float    defaultTessFactor   = 4.0f;
};

NkString GenerateTessControlHLSL(
    const NkString&         source,
    const NkSLTessLayout&   layout,
    const NkSLCompileOptions& opts);

NkString GenerateTessEvalHLSL(
    const NkString&         source,
    const NkSLTessLayout&   layout,
    const NkSLCompileOptions& opts);

// =============================================================================
// Génération HLSL — Ray tracing (DXR)
// =============================================================================
struct NkSLRayTracingConfig {
    uint32 maxRecursionDepth = 1;
    bool   useInlineRT       = false; // ray query (inline) vs pipeline RT
};

NkString GenerateRayGenHLSL   (const NkString& src, const NkSLCompileOptions& opts);
NkString GenerateClosestHitHLSL(const NkString& src, const NkSLCompileOptions& opts);
NkString GenerateMissHLSL     (const NkString& src, const NkSLCompileOptions& opts);
NkString GenerateAnyHitHLSL   (const NkString& src, const NkSLCompileOptions& opts);

// =============================================================================
// Génération — Bindless resources
// =============================================================================
struct NkSLBindlessConfig {
    // HLSL SM6.6 : ResourceDescriptorHeap / SamplerDescriptorHeap
    // MSL : argument buffers tier 2
    bool useSM66Heap        = true;  // HLSL only
    bool useArgumentBuffers = true;  // MSL only
    uint32 maxDescriptors   = 1000000;
};

NkString GenerateBindlessHLSL(const NkSLBindlessConfig& cfg);
NkString GenerateBindlessMSL (const NkSLBindlessConfig& cfg);

} // namespace nkentseu
