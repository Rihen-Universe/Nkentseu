#pragma once
// =============================================================================
// NkSLIntegration.h  — v4.0
//
// CORRECTIONS :
//   - GetCompiler() : singleton protégé par std::call_once (thread-safe)
//   - ApiToTarget() : distingue DX11 vs DX12, Vulkan GLSL vs SPIR-V
//
// NOUVEAUTÉS :
//   - ApiToGLSLVulkanTarget() : retourne NK_GLSL_VULKAN pour Vulkan
//   - ApiToHLSLTarget()       : retourne NK_HLSL_DX11 ou NK_HLSL_DX12 selon l'API
// =============================================================================
#include "NkSLCompiler.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace nksl {

        // =============================================================================
        // Singleton du compilateur NkSL (thread-safe via std::call_once)
        // =============================================================================
        NkSLCompiler& GetCompiler();
        void          InitCompiler  (const NkString& cacheDir = "");
        void          ShutdownCompiler();

        // =============================================================================
        // Conversion NkGraphicsApi → NkSLTarget
        //
        // ApiToTarget() — comportement par défaut (SPIR-V pour Vulkan, HLSL SM5 pour DX)
        //   NK_GFX_API_OPENGL    → NK_GLSL
        //   NK_GFX_API_VULKAN    → NK_SPIRV     (SPIR-V binaire — chemin recommandé)
        //   NK_GFX_API_D3D11 → NK_HLSL_DX11 (SM5, fxc)
        //   NK_GFX_API_D3D12 → NK_HLSL_DX12 (SM6, dxc)
        //   NK_GFX_API_METAL     → NK_MSL        (SPIRV-Cross auto si dispo)
        //   NK_GFX_API_SOFTWARE  → NK_CPLUSPLUS
        //
        // ApiToGLSLTarget() — retourne toujours du GLSL texte (pas de SPIR-V)
        //   NK_GFX_API_VULKAN    → NK_GLSL_VULKAN (GLSL 4.50 avec set/binding Vulkan)
        //   NK_GFX_API_OPENGL    → NK_GLSL
        //
        // ApiToHLSLTarget() — retourne le HLSL approprié selon la version DX
        //   NK_GFX_API_D3D12 → NK_HLSL_DX12
        //   NK_GFX_API_D3D11 → NK_HLSL_DX11
        // =============================================================================
        inline NkSLTarget ApiToTarget(NkGraphicsApi api) {
            switch (api) {
                case NkGraphicsApi::NK_GFX_API_OPENGL:    return NkSLTarget::NK_GLSL;
                case NkGraphicsApi::NK_GFX_API_VULKAN:    return NkSLTarget::NK_SPIRV;
                case NkGraphicsApi::NK_GFX_API_D3D11: return NkSLTarget::NK_HLSL_DX11;
                case NkGraphicsApi::NK_GFX_API_D3D12: return NkSLTarget::NK_HLSL_DX12;
                case NkGraphicsApi::NK_GFX_API_METAL:     return NkSLTarget::NK_MSL;
                case NkGraphicsApi::NK_GFX_API_SOFTWARE:  return NkSLTarget::NK_CPLUSPLUS;
                default:                              return NkSLTarget::NK_GLSL;
            }
        }

        // Retourne du GLSL texte (jamais SPIR-V binaire)
        inline NkSLTarget ApiToGLSLTarget(NkGraphicsApi api) {
            switch (api) {
                case NkGraphicsApi::NK_GFX_API_VULKAN: return NkSLTarget::NK_GLSL_VULKAN;
                default:                           return NkSLTarget::NK_GLSL;
            }
        }

        // Retourne le HLSL approprié selon la version DX
        inline NkSLTarget ApiToHLSLTarget(NkGraphicsApi api) {
            if (api == NkGraphicsApi::NK_GFX_API_D3D12) return NkSLTarget::NK_HLSL_DX12;
            return NkSLTarget::NK_HLSL_DX11;
        }

        // =============================================================================
        // Créer un shader depuis source NkSL
        // =============================================================================
        NkShaderHandle CreateShaderFromSource(
            NkIDevice*                device,
            const NkString&           source,
            const NkVector<NkSLStage>& stages,
            const NkString&           name  = "shader",
            const NkSLCompileOptions& opts  = {}
        );

        struct ShaderWithReflection {
            NkShaderHandle handle;
            NkSLReflection reflection;
            bool           success = false;
        };

        ShaderWithReflection CreateShaderWithReflection(
            NkIDevice*                device,
            const NkString&           source,
            const NkVector<NkSLStage>& stages,
            const NkString&           name  = "shader",
            const NkSLCompileOptions& opts  = {}
        );

        // =============================================================================
        // Créer depuis un fichier .nksl
        // =============================================================================
        NkShaderHandle CreateShaderFromFile(
            NkIDevice*                device,
            const NkString&           filePath,
            const NkVector<NkSLStage>& stages,
            const NkSLCompileOptions& opts = {}
        );

        ShaderWithReflection CreateShaderFromFileWithReflection(
            NkIDevice*                device,
            const NkString&           filePath,
            const NkVector<NkSLStage>& stages,
            const NkSLCompileOptions& opts = {}
        );

        // =============================================================================
        // Reflection seule
        // =============================================================================
        NkSLReflection GetReflection(
            const NkString& source,
            NkSLStage       stage,
            const NkString& filename = "shader"
        );

        // =============================================================================
        // NkShaderDesc
        // =============================================================================
        bool BuildShaderDesc(
            NkGraphicsApi             api,
            const NkString&           source,
            const NkVector<NkSLStage>& stages,
            NkShaderDesc&             outDesc,
            const NkString&           name  = "shader",
            const NkSLCompileOptions& opts  = {}
        );

        bool BuildShaderDescWithReflection(
            NkGraphicsApi             api,
            const NkString&           source,
            const NkVector<NkSLStage>& stages,
            NkShaderDesc&             outDesc,
            NkSLReflection&           outReflection,
            const NkString&           name  = "shader",
            const NkSLCompileOptions& opts  = {}
        );

        // =============================================================================
        // Validation
        // =============================================================================
        NkVector<NkSLCompileError> Validate(const NkString& source,
                                             const NkString& filename = "shader");

        // =============================================================================
        // Debug — code généré
        // =============================================================================
        NkString GetGeneratedSource(
            NkGraphicsApi             api,
            const NkString&           source,
            NkSLStage                 stage,
            const NkSLCompileOptions& opts = {}
        );

        // =============================================================================
        // Génération automatique du layout
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
