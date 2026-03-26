#pragma once
// =============================================================================
// NkSLIntegration.h  — v3.0
//
// Nouveautés v3.0 :
//   - CreateShaderWithReflection() : crée le shader ET retourne la reflection
//   - GetReflection() : reflection seule sans compilation
//   - Support MSL_SPIRV_CROSS transparent (choisi automatiquement pour Metal)
// =============================================================================
#include "NkSLCompiler.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
namespace nksl {

// =============================================================================
// Singleton du compilateur NkSL
// =============================================================================
NkSLCompiler& GetCompiler();
void          InitCompiler(const NkString& cacheDir = "");
void          ShutdownCompiler();

// =============================================================================
// Conversion NkGraphicsApi → NkSLTarget
// =============================================================================
inline NkSLTarget ApiToTarget(NkGraphicsApi api) {
    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:    return NkSLTarget::NK_GLSL;
        case NkGraphicsApi::NK_API_VULKAN:    return NkSLTarget::NK_SPIRV;
        case NkGraphicsApi::NK_API_DIRECTX11:
        case NkGraphicsApi::NK_API_DIRECTX12: return NkSLTarget::NK_HLSL;
        case NkGraphicsApi::NK_API_METAL:     return NkSLTarget::NK_MSL; // SPIRV-Cross auto si dispo
        case NkGraphicsApi::NK_API_SOFTWARE:  return NkSLTarget::NK_CPLUSPLUS;
        default:                              return NkSLTarget::NK_GLSL;
    }
}

// =============================================================================
// Créer un shader depuis le source NkSL
// =============================================================================
NkShaderHandle CreateShaderFromSource(
    NkIDevice*               device,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    const NkString&          name      = "shader",
    const NkSLCompileOptions& opts     = {}
);

// =============================================================================
// Créer un shader ET récupérer la reflection (bindings, vertex layout, etc.)
// Recommandé pour créer automatiquement les descriptor set layouts.
// =============================================================================
struct ShaderWithReflection {
    NkShaderHandle   handle;
    NkSLReflection   reflection;
    bool             success = false;
};

ShaderWithReflection CreateShaderWithReflection(
    NkIDevice*               device,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    const NkString&          name      = "shader",
    const NkSLCompileOptions& opts     = {}
);

// =============================================================================
// Créer depuis un fichier .nksl
// =============================================================================
NkShaderHandle CreateShaderFromFile(
    NkIDevice*               device,
    const NkString&          filePath,
    const NkVector<NkSLStage>& stages,
    const NkSLCompileOptions& opts     = {}
);

ShaderWithReflection CreateShaderFromFileWithReflection(
    NkIDevice*               device,
    const NkString&          filePath,
    const NkVector<NkSLStage>& stages,
    const NkSLCompileOptions& opts     = {}
);

// =============================================================================
// Reflection seule (sans créer le shader)
// Utile pour pré-inspecter les bindings avant la création du device.
// =============================================================================
NkSLReflection GetReflection(
    const NkString& source,
    NkSLStage       stage,
    const NkString& filename = "shader"
);

// =============================================================================
// Construire un NkShaderDesc depuis source NkSL
// =============================================================================
bool BuildShaderDesc(
    NkGraphicsApi            api,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    NkShaderDesc&            outDesc,
    const NkString&          name      = "shader",
    const NkSLCompileOptions& opts     = {}
);

bool BuildShaderDescWithReflection(
    NkGraphicsApi            api,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    NkShaderDesc&            outDesc,
    NkSLReflection&          outReflection,
    const NkString&          name      = "shader",
    const NkSLCompileOptions& opts     = {}
);

// =============================================================================
// Validation
// =============================================================================
NkVector<NkSLCompileError> Validate(const NkString& source,
                                     const NkString& filename = "shader");

// =============================================================================
// Inspection du code généré (debug)
// =============================================================================
NkString GetGeneratedSource(
    NkGraphicsApi            api,
    const NkString&          source,
    NkSLStage                stage,
    const NkSLCompileOptions& opts = {}
);

// =============================================================================
// Utilitaire : générer le layout C++ depuis le source NkSL
// Permet de créer automatiquement le NkDescriptorSetLayoutDesc
// =============================================================================
NkString GenerateLayoutCPP(
    const NkString& source,
    NkSLStage       stage,
    const NkString& varName  = "layout",
    const NkString& filename = "shader"
);

NkString GenerateLayoutJSON(
    const NkString& source,
    NkSLStage       stage,
    const NkString& filename = "shader"
);

} // namespace nksl
} // namespace nkentseu
