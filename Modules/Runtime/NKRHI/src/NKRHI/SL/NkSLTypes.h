#pragma once
// =============================================================================
// NkSLTypes.h  — v4.0
//
// NkSL est un langage de shader cross-platform qui se compile vers :
//   GLSL 4.30+         (OpenGL)
//   GLSL Vulkan 4.50+  (Vulkan — layout(set=N) obligatoire, extensions VK)
//   SPIR-V 1.0+        (Vulkan)  via glslang embarqué (standalone)
//   HLSL SM5           (DX11 — register classiques, fxc compatible)
//   HLSL SM6           (DX12 — space N, wave ops, bindless SM6.6, dxc)
//   MSL 2.0+           (Metal)   via SPIRV-Cross embarqué (chemin alternatif)
//   C++ lambdas        (Software rasterizer)
//
// Nouveautés v4.0 :
//   - NK_GLSL_VULKAN  : GLSL 4.50 + extensions VK (layout set+binding, subpassInput…)
//   - NK_HLSL_DX11    : HLSL SM5  — inchangé, renommé pour clarté
//   - NK_HLSL_DX12    : HLSL SM6+ — space N, RootSignature, wave ops, bindless heap
//   - NK_HLSL alias   : pointe sur NK_HLSL_DX11 (rétrocompatibilité sans casser l'existant)
//
// Dépendances embarquées (sous-modules git, AUCUNE dépendance système) :
//   - glslang    : https://github.com/KhronosGroup/glslang
//   - SPIRV-Cross: https://github.com/KhronosGroup/SPIRV-Cross
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKRHI/Core/NkDescs.h"

namespace nkentseu {

    // =============================================================================
    // Cible de compilation
    //
    // GLSL vs GLSL_VULKAN :
    //   NK_GLSL        → OpenGL 4.30+. Bindings aplatis (pas de set=).
    //                    Émet #version 430 core.
    //   NK_GLSL_VULKAN → Vulkan GLSL 4.50+. layout(set=N, binding=M) obligatoire.
    //                    Émet #version 450 + #extension GL_KHR_vulkan_glsl : require.
    //                    Destiné à être compilé en SPIR-V via glslang.
    //                    Supporte gl_BaseVertex/Instance, subpassInput, etc.
    //
    // HLSL_DX11 vs HLSL_DX12 :
    //   NK_HLSL_DX11   → Shader Model 5.0. register(bN/tN/sN/uN). cbuffer/tbuffer.
    //                    Compatible fxc.exe et d3dcompiler_47.dll.
    //   NK_HLSL_DX12   → Shader Model 6.0+. register(bN, spaceM). Nouveau pragma
    //                    RootSignature inline. Wave Intrinsics (SM6.0+).
    //                    Mesh/Task shaders (SM6.5). Raytracing (SM6.3).
    //                    ResourceDescriptorHeap/SamplerDescriptorHeap (SM6.6).
    //                    Compilé avec dxc.exe (DirectXShaderCompiler).
    // =============================================================================
    enum class NkSLTarget : uint32 {
        NK_GLSL,              // OpenGL 4.30+      (bindings aplatis)
        NK_GLSL_VULKAN,       // Vulkan GLSL 4.50+ (layout set+binding)    ← NOUVEAU
        NK_SPIRV,             // Vulkan SPIR-V binaire
        NK_HLSL_DX11,         // DirectX 11  SM5   (fxc, register classiques)
        NK_HLSL_DX12,         // DirectX 12  SM6+  (dxc, space N, wave ops)  ← NOUVEAU
        NK_MSL,               // Metal 2.0+        (génération native AST)
        NK_MSL_SPIRV_CROSS,   // Metal via SPIRV-Cross
        NK_CPLUSPLUS,         // Software rasterizer (C++)
        NK_COUNT,

        // Alias de rétrocompatibilité (ne pas utiliser dans le nouveau code)
        NK_HLSL = NK_HLSL_DX11,
    };

    inline const char* NkSLTargetName(NkSLTarget t) {
        switch (t) {
            case NkSLTarget::NK_GLSL:           return "GLSL-OpenGL";
            case NkSLTarget::NK_GLSL_VULKAN:    return "GLSL-Vulkan";
            case NkSLTarget::NK_SPIRV:          return "SPIR-V";
            case NkSLTarget::NK_HLSL_DX11:      return "HLSL-DX11(SM5)";
            case NkSLTarget::NK_HLSL_DX12:      return "HLSL-DX12(SM6)";
            case NkSLTarget::NK_MSL:            return "MSL";
            case NkSLTarget::NK_MSL_SPIRV_CROSS:return "MSL(SPIRV-Cross)";
            case NkSLTarget::NK_CPLUSPLUS:      return "C++";
            default:                            return "Unknown";
        }
    }

    // Helpers de classification
    inline bool NkSLTargetIsGLSL(NkSLTarget t) {
        return t == NkSLTarget::NK_GLSL || t == NkSLTarget::NK_GLSL_VULKAN;
    }
    inline bool NkSLTargetIsHLSL(NkSLTarget t) {
        return t == NkSLTarget::NK_HLSL_DX11 || t == NkSLTarget::NK_HLSL_DX12;
    }
    inline bool NkSLTargetIsMSL(NkSLTarget t) {
        return t == NkSLTarget::NK_MSL || t == NkSLTarget::NK_MSL_SPIRV_CROSS;
    }
    inline bool NkSLTargetIsVulkan(NkSLTarget t) {
        return t == NkSLTarget::NK_GLSL_VULKAN || t == NkSLTarget::NK_SPIRV;
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
            default:                         return "unknown";
        }
    }

    // =============================================================================
    // Types NkSL (identiques à GLSL standard)
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
        // Vulkan specifics
        NK_SUBPASS_INPUT,    // subpassInput (Vulkan input attachment)
        NK_SUBPASS_INPUT_MS, // subpassInputMS (multisampled)
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
        NK_INPUT_ATTACHMENT, // Vulkan subpassInput
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
        int32  set              = 0;
        int32  binding          = -1;
        int32  location         = -1;
        int32  offset           = -1;
        int32  inputAttachment  = -1; // subpass input index (Vulkan)

        bool HasBinding()         const { return binding         >= 0; }
        bool HasLocation()        const { return location        >= 0; }
        bool HasSet()             const { return set             >= 0; }
        bool HasInputAttachment() const { return inputAttachment >= 0; }
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

        bool IsText() const { return target != NkSLTarget::NK_SPIRV; }
        const char* GetSource() const { return source.CStr(); }

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
        // Versions
        uint32 glslVersion          = 430;  // OpenGL : #version 430 core
        uint32 glslVulkanVersion    = 450;  // Vulkan : #version 450
        uint32 vulkanVersion        = 100;  // spv 1.0
        bool   glslEs               = false;

        // HLSL shader models (valeur ×10 : 50=SM5.0, 60=SM6.0, 66=SM6.6)
        uint32 hlslShaderModelDX11  = 50;
        uint32 hlslShaderModelDX12  = 60;

        // MSL version (valeur ×100 : 200=MSL 2.0, 300=MSL 3.0)
        uint32 mslVersion           = 200;

        // Comportement
        bool   debugInfo            = false;
        bool   optimize             = true;
        bool   strictMode           = true;
        bool   flipUVY              = false;
        bool   invertDepth          = false;
        // NK_GLSL : ne pas émettre layout(set=) — aplatir vers layout(binding=)
        bool   flattenGLSLBindings  = true;
        bool   preferSpirvCrossForMSL = true;

        // DX12 spécifique
        bool   dx12InlineRootSignature = false; // émet [RootSignature(...)]
        uint32 dx12DefaultSpace        = 0;      // numéro de space par défaut
        bool   dx12BindlessHeap        = false;  // SM6.6 ResourceDescriptorHeap

        // Vulkan GLSL spécifique
        bool   vkDrawParams         = false; // émet gl_BaseVertex/Instance
        bool   vkDebugPrintf        = false; // GL_EXT_debug_printf

        NkString entryPoint = "main";

        // Compat : hlslShaderModel → DX11 par défaut
        uint32 hlslShaderModel = 50;
    };

    // =============================================================================
    // Reflection — descripteurs extraits automatiquement de l'AST
    // =============================================================================
    enum class NkSLResourceKind : uint32 {
        NK_UNIFORM_BUFFER,
        NK_STORAGE_BUFFER,
        NK_PUSH_CONSTANT,
        NK_SAMPLED_TEXTURE,
        NK_STORAGE_IMAGE,
        NK_SAMPLER,
        NK_INPUT_ATTACHMENT,
    };

    struct NkSLResourceBinding {
        NkString         name;
        NkSLResourceKind kind      = NkSLResourceKind::NK_UNIFORM_BUFFER;
        uint32           set       = 0;
        uint32           binding   = 0;
        uint32           location  = 0;
        NkSLStage        stages    = NkSLStage::NK_VERTEX;
        NkSLBaseType     baseType  = NkSLBaseType::NK_UNKNOWN;
        NkString         typeName;
        uint32           arraySize = 0;
        uint32           sizeBytes = 0;
    };

    struct NkSLVertexInput {
        NkString     name;
        uint32       location   = 0;
        NkSLBaseType baseType   = NkSLBaseType::NK_FLOAT;
        uint32       components = 1;
    };

    struct NkSLStageOutput {
        NkString     name;
        uint32       location = 0;
        NkSLBaseType baseType = NkSLBaseType::NK_FLOAT;
    };

    struct NkSLReflection {
        NkVector<NkSLResourceBinding> resources;
        NkVector<NkSLVertexInput>     vertexInputs;
        NkVector<NkSLStageOutput>     stageOutputs;
        uint32 localSizeX = 1;
        uint32 localSizeY = 1;
        uint32 localSizeZ = 1;

        const NkSLResourceBinding* FindResource(const NkString& name) const {
            for (auto& r : resources) if (r.name == name) return &r;
            return nullptr;
        }
        const NkSLResourceBinding* FindBinding(uint32 set, uint32 binding) const {
            for (auto& r : resources)
                if (r.set == set && r.binding == (uint32)binding) return &r;
            return nullptr;
        }
        uint32 GetUBOSize(const NkString& name) const {
            const NkSLResourceBinding* r = FindResource(name);
            return r ? r->sizeBytes : 0;
        }
    };

} // namespace nkentseu
