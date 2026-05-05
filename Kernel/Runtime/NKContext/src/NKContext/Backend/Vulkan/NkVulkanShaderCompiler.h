// =============================================================================
// NkVulkanShaderCompiler.h — Runtime GLSL → SPIR-V via glslang C++ API
//
// Inclure ce header dans NkVulkanRenderer2D.cpp AVANT NkRenderer2DVkSpv.inl.
// Il fournit NkCompileGLSLToSPIRV() qui retourne un NkVector<uint32>.
//
// Link flags (à ajouter dans NKContext.jenga pour Vulkan) :
//   Windows/Linux : -lglslang -lSPIRV -lglslang-default-resource-limits
//   OU:  utiliser shaderc (recommandé) : -lshaderc_combined
//
// Si ni glslang ni shaderc ne sont disponibles, la fonction retourne {} et
// CreatePipelines() logge une erreur claire.
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Essaie dans l'ordre :
        //   1. shaderc (libshaderc — disponible avec Vulkan SDK)
        //   2. glslang C++ API (header-only + libs)
        //   3. Aucun compilateur → retourne {} (les shaders pré-compilés sont requis)
        // =============================================================================

        #if defined(NK_VK2D_USE_SHADERC)
            // ── shaderc ──────────────────────────────────────────────────────────────────
            #include <shaderc/shaderc.hpp>

            inline NkVector<uint32> NkCompileGLSLToSPIRV(
                const char* glslSource,
                shaderc_shader_kind kind,
                const char* sourceName = "shader")
            {
                shaderc::Compiler       compiler;
                shaderc::CompileOptions opts;
                opts.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
                opts.SetOptimizationLevel(shaderc_optimization_level_performance);

                auto result = compiler.CompileGlslToSpv(
                    glslSource, strlen(glslSource), kind, sourceName, opts);

                if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
                    logger.Errorf("[NkVk2D] shaderc: %s", result.GetErrorMessage().c_str());
                    return {};
                }

                NkVector<uint32> spv;
                spv.Reserve((NkVector<uint32>::SizeType)(result.end() - result.begin()));
                for (auto w : result) spv.PushBack(w);
                return spv;
            }

            inline NkVector<uint32> NkCompileVertGLSL(const char* src) {
                return NkCompileGLSLToSPIRV(src, shaderc_glsl_vertex_shader, "nkrenderer2d.vert");
            }
            inline NkVector<uint32> NkCompileFragGLSL(const char* src) {
                return NkCompileGLSLToSPIRV(src, shaderc_glsl_fragment_shader, "nkrenderer2d.frag");
            }

        #elif defined(NK_VK2D_USE_GLSLANG)
            // ── glslang C++ API ──────────────────────────────────────────────────────────
            #include <glslang/Public/ShaderLang.h>
            #include <glslang/SPIRV/GlslangToSpv.h>
            #include <glslang/Public/ResourceLimits.h>

            inline NkVector<uint32> NkCompileGLSLToSPIRV_Impl(
                const char* glslSource,
                EShLanguage stage)
            {
                static bool sInitialized = false;
                if (!sInitialized) {
                    glslang::InitializeProcess();
                    sInitialized = true;
                }

                glslang::TShader shader(stage);
                shader.setStrings(&glslSource, 1);
                shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
                shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
                shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

                const TBuiltInResource* resources = GetDefaultResources();
                if (!shader.parse(resources, 100, false, EShMsgDefault)) {
                    logger.Errorf("[NkVk2D] glslang parse: %s", shader.getInfoLog());
                    return {};
                }

                glslang::TProgram program;
                program.addShader(&shader);
                if (!program.link(EShMsgDefault)) {
                    logger.Errorf("[NkVk2D] glslang link: %s", program.getInfoLog());
                    return {};
                }

                std::vector<uint32_t> spvStd;
                glslang::GlslangToSpv(*program.getIntermediate(stage), spvStd);

                NkVector<uint32> spv;
                spv.Reserve((NkVector<uint32>::SizeType)spvStd.size());
                for (auto w : spvStd) spv.PushBack(w);
                return spv;
            }

            inline NkVector<uint32> NkCompileVertGLSL(const char* src) {
                return NkCompileGLSLToSPIRV_Impl(src, EShLangVertex);
            }
            inline NkVector<uint32> NkCompileFragGLSL(const char* src) {
                return NkCompileGLSLToSPIRV_Impl(src, EShLangFragment);
            }

        #else
            // ── Pas de compilateur runtime : retourne {} ─────────────────────────────────
            // Les shaders pré-compilés (NkRenderer2DVkSpv.inl) sont obligatoires.
            inline NkVector<uint32> NkCompileVertGLSL(const char*) { return {}; }
            inline NkVector<uint32> NkCompileFragGLSL(const char*) { return {}; }
        #endif

    } // namespace renderer
} // namespace nkentseu