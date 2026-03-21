#pragma once
// =============================================================================
// NkSLTypes.h
// Types fondamentaux du langage de shader NkSL.
//
// NkSL est un langage de shader cross-platform qui se compile vers :
//   GLSL 4.30+  (OpenGL)
//   SPIR-V 1.0+ (Vulkan)  via glslang
//   HLSL SM5+   (DX11/DX12)
//   MSL 2.0+    (Metal)
//   C++ lambdas (Software rasterizer)
//
// Syntaxe de base : GLSL-like avec extensions pour les resource bindings.
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
    GLSL,        // OpenGL 4.30+
    SPIRV,       // Vulkan 1.0+ (bytecode binaire)
    HLSL,        // DirectX 11 / 12 (SM 5.0+)
    MSL,         // Metal 2.0+
    CPLUSPLUS,   // Software rasterizer (C++ lambda)
    COUNT
};

inline const char* NkSLTargetName(NkSLTarget t) {
    switch (t) {
        case NkSLTarget::GLSL:      return "GLSL";
        case NkSLTarget::SPIRV:     return "SPIR-V";
        case NkSLTarget::HLSL:      return "HLSL";
        case NkSLTarget::MSL:       return "MSL";
        case NkSLTarget::CPLUSPLUS: return "C++";
        default:                    return "Unknown";
    }
}

// =============================================================================
// Stage de shader
// =============================================================================
enum class NkSLStage : uint32 {
    VERTEX,
    FRAGMENT,
    GEOMETRY,
    TESS_CONTROL,
    TESS_EVAL,
    COMPUTE,
};

inline const char* NkSLStageName(NkSLStage s) {
    switch (s) {
        case NkSLStage::VERTEX:       return "vertex";
        case NkSLStage::FRAGMENT:     return "fragment";
        case NkSLStage::GEOMETRY:     return "geometry";
        case NkSLStage::TESS_CONTROL: return "tess_control";
        case NkSLStage::TESS_EVAL:    return "tess_eval";
        case NkSLStage::COMPUTE:      return "compute";
        default:                      return "unknown";
    }
}

// =============================================================================
// Types NkSL (sous-ensemble)
// =============================================================================
enum class NkSLBaseType : uint32 {
    VOID,
    BOOL,
    INT, IVEC2, IVEC3, IVEC4,
    UINT, UVEC2, UVEC3, UVEC4,
    FLOAT, VEC2, VEC3, VEC4,
    DOUBLE, DVEC2, DVEC3, DVEC4,
    MAT2, MAT3, MAT4,
    MAT2X3, MAT2X4, MAT3X2, MAT3X4, MAT4X2, MAT4X3,
    DMAT2, DMAT3, DMAT4,
    // Samplers
    SAMPLER2D, SAMPLER2D_SHADOW,
    SAMPLER2D_ARRAY, SAMPLER2D_ARRAY_SHADOW,
    SAMPLER_CUBE, SAMPLER_CUBE_SHADOW,
    SAMPLER3D,
    ISAMPLER2D, USAMPLER2D,
    // Storage images
    IMAGE2D, IIMAGE2D, UIMAGE2D,
    // Struct / Array
    STRUCT, ARRAY,
    UNKNOWN
};

inline bool NkSLTypeIsSampler(NkSLBaseType t) {
    return t >= NkSLBaseType::SAMPLER2D && t <= NkSLBaseType::USAMPLER2D;
}

inline bool NkSLTypeIsImage(NkSLBaseType t) {
    return t >= NkSLBaseType::IMAGE2D && t <= NkSLBaseType::UIMAGE2D;
}

inline bool NkSLTypeIsMatrix(NkSLBaseType t) {
    return t >= NkSLBaseType::MAT2 && t <= NkSLBaseType::DMAT4;
}

// =============================================================================
// Qualificateurs
// =============================================================================
enum class NkSLStorageQual : uint32 {
    NONE,
    IN, OUT, INOUT,
    UNIFORM, BUFFER,        // uniform block, storage buffer
    PUSH_CONSTANT,          // Vulkan push constants
    SHARED,                 // compute shared memory
    WORKGROUP,              // alias
};

enum class NkSLInterpolation : uint32 {
    SMOOTH,     // défaut
    FLAT,
    NOPERSPECTIVE,
};

enum class NkSLPrecision : uint32 {
    NONE,
    LOWP, MEDIUMP, HIGHP   // ES/Metal
};

// =============================================================================
// Métadonnées de binding
// =============================================================================
struct NkSLBinding {
    int32  set      = 0;     // Vulkan descriptor set (HLSL: space, GLSL: ignoré)
    int32  binding  = -1;    // binding point (-1 = auto)
    int32  location = -1;    // in/out location (-1 = auto)
    int32  offset   = -1;    // explicit offset dans un block

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
    NkVector<uint8>            bytecode;  // SPIR-V binary or text source
    NkString                   source;    // GLSL / HLSL / MSL text output
    NkVector<NkSLCompileError> errors;
    NkVector<NkSLCompileError> warnings;
    NkSLTarget                 target = NkSLTarget::GLSL;
    NkSLStage                  stage  = NkSLStage::VERTEX;

    bool IsText() const {
        return target != NkSLTarget::SPIRV;
    }

    const char* GetSource() const {
        return source.CStr();
    }

    void AddError(uint32 line, const NkString& msg, bool fatal=true) {
        errors.PushBack({line, 0, "", msg, fatal});
    }
    void AddWarning(uint32 line, const NkString& msg) {
        warnings.PushBack({line, 0, "", msg, false});
    }
};

// =============================================================================
// Options de compilation
// =============================================================================
struct NkSLCompileOptions {
    // Vulkan
    uint32 vulkanVersion   = 100; // 100=1.0, 110=1.1, 120=1.2

    // OpenGL
    uint32 glslVersion     = 460; // 430, 450, 460
    bool   glslEs          = false;

    // HLSL
    uint32 hlslShaderModel = 50;  // 50=SM5.0, 51=SM5.1, 60=SM6.0

    // MSL
    uint32 mslVersion      = 200; // 200=2.0, 210=2.1, 300=3.0

    // General
    bool   debugInfo       = false;
    bool   optimize        = true;
    bool   strictMode      = true;
    bool   flipUVY         = false; // flip Y pour les UV (OpenGL vs DX)
    bool   invertDepth     = false; // reverse-Z

    // Remapping de bindings automatique pour GLSL
    // (GLSL 4.30 ne supporte pas les sets Vulkan nativement)
    bool   flattenGLSLBindings = true;

    // Entry point override
    NkString entryPoint = "main";
};

} // namespace nkentseu
