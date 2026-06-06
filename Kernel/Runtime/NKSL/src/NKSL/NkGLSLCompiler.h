#pragma once
// =============================================================================
// NkGLSLCompiler.h
// Compilation GLSL → SPIR-V via libglslang (Vulkan SDK).
// Disponible uniquement quand NK_RHI_VK_ENABLED est défini.
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

#if defined(NK_RHI_VK_ENABLED)

    // Résultat d'une compilation GLSL → SPIR-V
    struct NkGLSLCompileResult {
        bool             success  = false;
        NkVector<uint32> spirv;          // mots SPIR-V (vides si échec)
        const char*      errorLog = nullptr; // pointeur vers buffer statique interne
    };

    // Compile une source GLSL en SPIR-V via glslang.
    // L'implementation reelle est fournie quand NKRHI est compile avec
    // NK_RHI_GLSLANG_ENABLED (NKGLSlang in-tree). Sinon, le .cpp fournit un
    // stub qui retourne errorLog non-null. Les consommateurs (NKRenderer, etc)
    // n'ont donc PAS a connaitre NK_RHI_GLSLANG_ENABLED.
    NkGLSLCompileResult NkGLSLToSPIRV(NkShaderStage stage,
                                       const char*   glslSrc,
                                       const char*   entry = "main");
    void NkGLSLCompilerInit();
    void NkGLSLCompilerShutdown();

#endif // NK_RHI_VK_ENABLED

} // namespace nkentseu
