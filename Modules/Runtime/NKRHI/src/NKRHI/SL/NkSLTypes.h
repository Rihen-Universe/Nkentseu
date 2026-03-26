#pragma once
// =============================================================================
// NkSLTypes.h
// Types fondamentaux du langage de shader NkSL.
//
// NkSL est un langage de shader cross-platform qui se compile vers :
//   GLSL 4.30+  (OpenGL)
//   SPIR-V 1.0+ (Vulkan)  via glslang embarqué (standalone, sans Vulkan SDK)
//   HLSL SM5+   (DX11/DX12)
//   MSL 2.0+    (Metal)   via SPIRV-Cross embarqué (chemin alternatif)
//   C++ lambdas (Software rasterizer)
//
// Dépendances embarquées (sous-modules git, AUCUNE dépendance système) :
//   - glslang  : https://github.com/KhronosGroup/glslang
//   - SPIRV-Cross : https://github.com/KhronosGroup/SPIRV-Cross
//
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

// =============================================================================
// Cible de compilation
// =============================================================================
enum class NkSLTarget : uint32 {
    NK_GLSL,        // OpenGL 4.30+
    NK_SPIRV,       // Vulkan 1.0+ (bytecode binaire)
    NK_HLSL,        // DirectX 11 / 12 (SM 5.0+)
    NK_MSL,         // Metal 2.0+
    NK_MSL_SPIRV_CROSS, // Metal via SPIRV-Cross (chemin alternatif plus robuste)
    NK_CPLUSPLUS,   // Software rasterizer (C++ lambda)
    NK_COUNT
};

inline const char* NkSLTargetName(NkSLTarget t) {
    switch (t) {
        case NkSLTarget::NK_GLSL:           return "GLSL";
        case NkSLTarget::NK_SPIRV:          return "SPIR-V";
        case NkSLTarget::NK_HLSL:           return "HLSL";
        case NkSLTarget::NK_MSL:            return "MSL";
        case NkSLTarget::NK_MSL_SPIRV_CROSS:return "MSL(SPIRV-Cross)";
        case NkSLTarget::NK_CPLUSPLUS:      return "C++";
        default:                         return "Unknown";
    }
}

// =============================================================================
// Stage de shader
// =============================================================================
enum class NkSLStage : uint32 {
    NK_VERTEX,
    NK_FRAGMENT,
    NK_GEOMETRY,
    NK_TESS_CONTROL,
    NK_TESS_EVAL,
    NK_COMPUTE,
};

inline const char* NkSLStageName(NkSLStage s) {
    switch (s) {
        case NkSLStage::NK_VERTEX:       return "vertex";
        case NkSLStage::NK_FRAGMENT:     return "fragment";
        case NkSLStage::NK_GEOMETRY:     return "geometry";
        case NkSLStage::NK_TESS_CONTROL: return "tess_control";
        case NkSLStage::NK_TESS_EVAL:    return "tess_eval";
        case NkSLStage::NK_COMPUTE:      return "compute";
        default:                      return "unknown";
    }
}

// =============================================================================
// Types NkSL
// =============================================================================
enum class NkSLBaseType : uint32 {
    NK_VOID,
    NK_BOOL,
    NK_INT, NK_IVEC2, NK_IVEC3, NK_IVEC4,
    NK_UINT, NK_UVEC2, NK_UVEC3, NK_UVEC4,
    NK_FLOAT, NK_VEC2, NK_VEC3, NK_VEC4,
    NK_DOUBLE, NK_DVEC2, NK_DVEC3, NK_DVEC4,
    NK_MAT2, NK_MAT3, NK_MAT4,
    NK_MAT2X3, NK_MAT2X4, NK_MAT3X2, NK_MAT3X4, NK_MAT4X2, NK_MAT4X3,
    NK_DMAT2, NK_DMAT3, NK_DMAT4,
    // Samplers
    NK_SAMPLER2D, NK_SAMPLER2D_SHADOW,
    NK_SAMPLER2D_ARRAY, NK_SAMPLER2D_ARRAY_SHADOW,
    NK_SAMPLER_CUBE, NK_SAMPLER_CUBE_SHADOW,
    NK_SAMPLER3D,
    NK_ISAMPLER2D, NK_USAMPLER2D,
    // Storage images
    NK_IMAGE2D, NK_IIMAGE2D, NK_UIMAGE2D,
    // Struct / Array
    NK_STRUCT, NK_ARRAY,
    NK_UNKNOWN
};

inline bool NkSLTypeIsSampler(NkSLBaseType t) {
    return t >= NkSLBaseType::NK_SAMPLER2D && t <= NkSLBaseType::NK_USAMPLER2D;
}

inline bool NkSLTypeIsImage(NkSLBaseType t) {
    return t >= NkSLBaseType::NK_IMAGE2D && t <= NkSLBaseType::NK_UIMAGE2D;
}

inline bool NkSLTypeIsMatrix(NkSLBaseType t) {
    return t == NkSLBaseType::NK_MAT2   || t == NkSLBaseType::NK_MAT3   ||
           t == NkSLBaseType::NK_MAT4   || t == NkSLBaseType::NK_MAT2X3 ||
           t == NkSLBaseType::NK_MAT2X4 || t == NkSLBaseType::NK_MAT3X2 ||
           t == NkSLBaseType::NK_MAT3X4 || t == NkSLBaseType::NK_MAT4X2 ||
           t == NkSLBaseType::NK_MAT4X3;
}

// =============================================================================
// Qualificateurs
// =============================================================================
enum class NkSLStorageQual : uint32 {
    NK_NONE,
    NK_IN, NK_OUT, NK_INOUT,
    NK_UNIFORM, NK_BUFFER,
    NK_PUSH_CONSTANT,
    NK_SHARED,
    NK_WORKGROUP,
};

enum class NkSLInterpolation : uint32 {
    NK_SMOOTH,
    NK_FLAT,
    NK_NOPERSPECTIVE,
};

enum class NkSLPrecision : uint32 {
    NK_NONE,
    NK_LOWP, NK_MEDIUMP, NK_HIGHP
};

// =============================================================================
// Métadonnées de binding
// =============================================================================
struct NkSLBinding {
    int32  set      = 0;
    int32  binding  = -1;
    int32  location = -1;
    int32  offset   = -1;

    bool HasBinding()  const { return binding  >= 0; }
    bool HasLocation() const { return location >= 0; }
    bool HasSet()      const { return set      >= 0; }
};

// =============================================================================
// Résultat de compilation
// =============================================================================
struct NkSLCompileError {
    uint32    line    = 0;
    uint32    column  = 0;
    NkString  file;
    NkString  message;
    bool      isFatal = true;
};

struct NkSLCompileResult {
    bool                       success = false;
    NkVector<uint8>            bytecode;
    NkString                   source;
    NkVector<NkSLCompileError> errors;
    NkVector<NkSLCompileError> warnings;
    NkSLTarget                 target = NkSLTarget::NK_GLSL;
    NkSLStage                  stage  = NkSLStage::NK_VERTEX;

    bool IsText() const {
        return target != NkSLTarget::NK_SPIRV;
    }

    const char* GetSource() const {
        return source.CStr();
    }

    void AddError(uint32 line, const NkString& msg, bool fatal=true) {
        errors.PushBack({line, 0, "", msg, fatal});
        if (fatal) success = false;
    }
    void AddWarning(uint32 line, const NkString& msg) {
        warnings.PushBack({line, 0, "", msg, false});
    }
};

// =============================================================================
// Options de compilation
// =============================================================================
struct NkSLCompileOptions {
    uint32 vulkanVersion   = 100;
    uint32 glslVersion     = 460;
    bool   glslEs          = false;
    uint32 hlslShaderModel = 50;
    uint32 mslVersion      = 200;
    bool   debugInfo       = false;
    bool   optimize        = true;
    bool   strictMode      = true;
    bool   flipUVY         = false;
    bool   invertDepth     = false;
    bool   flattenGLSLBindings = true;
    // Préférer SPIRV-Cross pour la génération MSL (plus robuste)
    bool   preferSpirvCrossForMSL = true;
    NkString entryPoint = "main";
};

// =============================================================================
// Reflection — descripteurs extraits automatiquement de l'AST
// =============================================================================

enum class NkSLResourceKind : uint32 {
    NK_UNIFORM_BUFFER,    // cbuffer / UBO
    NK_STORAGE_BUFFER,    // SSBO / RWStructuredBuffer
    NK_PUSH_CONSTANT,     // Vulkan push constants
    NK_SAMPLED_TEXTURE,   // sampler2D etc.
    NK_STORAGE_IMAGE,     // image2D etc.
    NK_SAMPLER,           // SamplerState séparé (HLSL/MSL)
    NK_INPUT_ATTACHMENT,  // Vulkan subpass input
};

struct NkSLResourceBinding {
    NkString         name;
    NkSLResourceKind kind     = NkSLResourceKind::NK_UNIFORM_BUFFER;
    uint32           set      = 0;
    uint32           binding  = 0;
    uint32           location = 0;    // pour in/out
    NkSLStage        stages   = NkSLStage::NK_VERTEX; // bitfield si nécessaire
    NkSLBaseType     baseType = NkSLBaseType::NK_UNKNOWN;
    NkString         typeName;        // nom du struct pour les UBO
    uint32           arraySize = 0;   // 0 = pas de tableau
    uint32           sizeBytes = 0;   // taille en octets (pour les UBO)
};

struct NkSLVertexInput {
    NkString     name;
    uint32       location = 0;
    NkSLBaseType baseType = NkSLBaseType::NK_FLOAT;
    uint32       components = 1; // 1=scalar, 2=vec2, etc.
};

struct NkSLStageOutput {
    NkString     name;
    uint32       location = 0;
    NkSLBaseType baseType = NkSLBaseType::NK_FLOAT;
};

// Résultat complet de la reflection d'un shader
struct NkSLReflection {
    NkVector<NkSLResourceBinding> resources;
    NkVector<NkSLVertexInput>     vertexInputs;
    NkVector<NkSLStageOutput>     stageOutputs;

    // Compute shader
    uint32 localSizeX = 1;
    uint32 localSizeY = 1;
    uint32 localSizeZ = 1;

    // Utilitaires de recherche
    const NkSLResourceBinding* FindResource(const NkString& name) const {
        for (auto& r : resources)
            if (r.name == name) return &r;
        return nullptr;
    }

    const NkSLResourceBinding* FindBinding(uint32 set, uint32 binding) const {
        for (auto& r : resources)
            if (r.set == set && r.binding == (uint32)binding) return &r;
        return nullptr;
    }

    // Taille totale d'un UBO (somme des membres)
    uint32 GetUBOSize(const NkString& name) const {
        const NkSLResourceBinding* r = FindResource(name);
        return r ? r->sizeBytes : 0;
    }
};

} // namespace nkentseu
