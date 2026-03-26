// =============================================================================
// NkGLSLCompiler.cpp
// Compilation GLSL → SPIR-V via libglslang (Vulkan SDK).
// =============================================================================
#include "NkGLSLCompiler.h"

#if defined(NK_RHI_VK_ENABLED) && !defined(__MINGW32__) && !defined(__MINGW64__)

// ─── glslang headers ─────────────────────────────────────────────────────────
// Le SDK Vulkan fournit glslang dans <VULKAN_SDK>/Include/glslang/
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include "NKLogger/NkLog.h"

#define NKENTSEU_ERRORF(...) logger_src.Errorf("[GLSL COMPILER] " __VA_ARGS__)
#define NKENTSEU_WARNF(...) logger_src.Warnf("[GLSL COMPILER] " __VA_ARGS__)

namespace nkentseu {

    // ── helpers ───────────────────────────────────────────────────────────────

    static EShLanguage ToEShLanguage(NkShaderStage stage) {
        switch (stage) {
            case NkShaderStage::NK_VERTEX:    return EShLangVertex;
            case NkShaderStage::NK_FRAGMENT:  return EShLangFragment;
            case NkShaderStage::NK_GEOMETRY:  return EShLangGeometry;
            case NkShaderStage::NK_TESS_CTRL: return EShLangTessControl;
            case NkShaderStage::NK_TESS_EVAL: return EShLangTessEvaluation;
            case NkShaderStage::NK_COMPUTE:   return EShLangCompute;
            default:                          return EShLangVertex;
        }
    }

    // Buffer statique pour les messages d'erreur (évite l'allocation dynamique)
    static char s_errorBuf[4096];

    // ── API publique ──────────────────────────────────────────────────────────

    void NkGLSLCompilerInit() {
        glslang::InitializeProcess();
    }

    void NkGLSLCompilerShutdown() {
        glslang::FinalizeProcess();
    }

    NkGLSLCompileResult NkGLSLToSPIRV(NkShaderStage stage,
                                       const char*   glslSrc,
                                       const char*   entry) {
        NkGLSLCompileResult result;

        if (!glslSrc || !*glslSrc) {
            result.errorLog = "NkGLSLToSPIRV: source GLSL vide";
            return result;
        }

        EShLanguage lang = ToEShLanguage(stage);
        glslang::TShader shader(lang);

        // Configuration Vulkan 1.0 / GLSL 450
        shader.setEnvInput(glslang::EShSourceGlsl, lang,
                           glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan,
                            glslang::EShTargetVulkan_1_0);
        shader.setEnvTarget(glslang::EShTargetSpv,
                            glslang::EShTargetSpv_1_0);
        shader.setEntryPoint(entry ? entry : "main");

        const char* srcs[] = { glslSrc };
        shader.setStrings(srcs, 1);

        // Limites par défaut fournies par glslang-default-resource-limits
        const TBuiltInResource* limits = GetDefaultResources();

        EShMessages msgs = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
        if (!shader.parse(limits, 450, false, msgs)) {
            const char* log = shader.getInfoLog();
            int n = (int)(sizeof(s_errorBuf) - 1);
            int len = 0;
            while (log && log[len] && len < n) len++;
            for (int i = 0; i < len; i++) s_errorBuf[i] = log[i];
            s_errorBuf[len] = '\0';
            result.errorLog = s_errorBuf;
            NKENTSEU_ERRORF("NkGLSLToSPIRV parse error:\n%s\n", s_errorBuf);
            return result;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(msgs)) {
            const char* log = program.getInfoLog();
            int n = (int)(sizeof(s_errorBuf) - 1);
            int len = 0;
            while (log && log[len] && len < n) len++;
            for (int i = 0; i < len; i++) s_errorBuf[i] = log[i];
            s_errorBuf[len] = '\0';
            result.errorLog = s_errorBuf;
            NKENTSEU_ERRORF("NkGLSLToSPIRV link error:\n%s\n", s_errorBuf);
            return result;
        }

        // Génération du SPIR-V dans un std::vector temporaire puis copie
        std::vector<uint32_t> spirvStd;
        spv::SpvBuildLogger spvLog;
        glslang::SpvOptions spvOpts;
        spvOpts.disableOptimizer = true;
        spvOpts.validate         = false;

        glslang::GlslangToSpv(*program.getIntermediate(lang),
                              spirvStd, &spvLog, &spvOpts);

        const std::string& spvMsg = spvLog.getAllMessages();
        if (!spvMsg.empty()) {
            NKENTSEU_WARNF("NkGLSLToSPIRV spv log: %s\n", spvMsg.c_str());
        }

        if (spirvStd.empty()) {
            result.errorLog = "NkGLSLToSPIRV: GlslangToSpv a produit 0 mots";
            NKENTSEU_ERRORF("%s\n", result.errorLog);
            return result;
        }

        // Copie dans notre NkVector<uint32> (sans STL en dehors de ce .cpp)
        result.spirv.Reserve((uint32)spirvStd.size());
        for (uint32_t w : spirvStd)
            result.spirv.PushBack(w);

        result.success = true;
        return result;
    }

} // namespace nkentseu

#endif // NK_RHI_VK_ENABLED && !MinGW
