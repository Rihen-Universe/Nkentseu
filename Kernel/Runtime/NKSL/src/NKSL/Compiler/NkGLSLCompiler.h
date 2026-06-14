#pragma once
// =============================================================================
// NkGLSLCompiler.h
// Compilation GLSL → SPIR-V via libglslang (NKGLSlang in-tree).
// Ne dépend QUE de glslang, PAS de Vulkan : la déclaration est toujours visible
// (module NKSL). L'implémentation réelle est fournie quand glslang est présent
// (NK_RHI_GLSLANG_ENABLED) ; sinon le .cpp fournit un stub. Les consommateurs
// n'ont donc pas à connaître l'état de glslang.
// =============================================================================
#include "NKSL/Core/NkSLTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    // Résultat d'une compilation GLSL → SPIR-V
    struct NkGLSLCompileResult {
        bool             success  = false;
        NkVector<uint32> spirv;          // mots SPIR-V (vides si échec)
        const char*      errorLog = nullptr; // pointeur vers buffer statique interne
    };

    NkGLSLCompileResult NkGLSLToSPIRV(NkSLStage stage,
                                       const char*   glslSrc,
                                       const char*   entry = "main");
    void NkGLSLCompilerInit();
    void NkGLSLCompilerShutdown();

} // namespace nkentseu
