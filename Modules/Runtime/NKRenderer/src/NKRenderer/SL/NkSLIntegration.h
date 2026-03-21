#pragma once
// =============================================================================
// NkSLIntegration.h
// Helpers d'intégration NkSL ↔ NkIDevice.
//
// Remplace complètement la gestion manuelle des shaders par backend.
// Usage :
//
//   // Shader NkSL unique, compile vers la bonne API automatiquement :
//   const char* kPhongShader = R"NKSL(
//       @stage(vertex)
//       @entry
//       void main() { ... }
//   )NKSL";
//
//   NkShaderHandle hShader = NkSL::CreateShaderFromSource(device, kPhongShader,
//       {NkSLStage::VERTEX, NkSLStage::FRAGMENT});
//
// =============================================================================
#include "NkSLCompiler.h"
#include "NKRenderer/RHI/Core/NkIDevice.h"

namespace nkentseu {
namespace NkSL {

// =============================================================================
// Singleton du compilateur NkSL (partagé par toute l'application)
// =============================================================================
NkSLCompiler& GetCompiler();
void          InitCompiler(const NkString& cacheDir = "");
void          ShutdownCompiler();

// =============================================================================
// Conversion NkGraphicsApi → NkSLTarget
// =============================================================================
inline NkSLTarget ApiToTarget(NkGraphicsApi api) {
    switch (api) {
        case NkGraphicsApi::NK_API_OPENGL:    return NkSLTarget::GLSL;
        case NkGraphicsApi::NK_API_VULKAN:    return NkSLTarget::SPIRV;
        case NkGraphicsApi::NK_API_DIRECTX11:
        case NkGraphicsApi::NK_API_DIRECTX12: return NkSLTarget::HLSL;
        case NkGraphicsApi::NK_API_METAL:     return NkSLTarget::MSL;
        case NkGraphicsApi::NK_API_SOFTWARE:  return NkSLTarget::CPLUSPLUS;
        default:                              return NkSLTarget::GLSL;
    }
}

// =============================================================================
// Compiler un shader NkSL et créer le NkShaderHandle
// source    : code source NkSL complet (vertex + fragment dans le même fichier)
// stages    : stages à compiler (VERTEX, FRAGMENT, etc.)
// name      : nom debug
// =============================================================================
NkShaderHandle CreateShaderFromSource(
    NkIDevice*               device,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    const NkString&          name      = "shader",
    const NkSLCompileOptions& opts     = {}
);

// =============================================================================
// Compiler depuis un fichier .nksl
// =============================================================================
NkShaderHandle CreateShaderFromFile(
    NkIDevice*               device,
    const NkString&          filePath,
    const NkVector<NkSLStage>& stages,
    const NkSLCompileOptions& opts     = {}
);

// =============================================================================
// Construire un NkShaderDesc depuis source NkSL (sans créer le shader)
// Utile pour pré-compiler et inspecter le résultat avant de créer le device.
// =============================================================================
bool BuildShaderDesc(
    NkGraphicsApi            api,
    const NkString&          source,
    const NkVector<NkSLStage>& stages,
    NkShaderDesc&            outDesc,
    const NkString&          name      = "shader",
    const NkSLCompileOptions& opts     = {}
);

// =============================================================================
// Valider un shader NkSL sans le compiler
// Retourne les erreurs syntaxiques et sémantiques
// =============================================================================
NkVector<NkSLCompileError> Validate(const NkString& source,
                                     const NkString& filename = "shader");

// =============================================================================
// Obtenir le code source cible généré (pour debug / inspection)
// =============================================================================
NkString GetGeneratedSource(
    NkGraphicsApi            api,
    const NkString&          source,
    NkSLStage                stage,
    const NkSLCompileOptions& opts = {}
);

} // namespace NkSL
} // namespace nkentseu
