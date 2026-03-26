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

#if !defined(__MINGW32__) && !defined(__MINGW64__)
    // Compile une source GLSL en SPIR-V via glslang.
    // Disponible uniquement avec un toolchain non-MinGW (MSVC / clang-cl / GCC).
    NkGLSLCompileResult NkGLSLToSPIRV(NkShaderStage stage,
                                       const char*   glslSrc,
                                       const char*   entry = "main");
    void NkGLSLCompilerInit();
    void NkGLSLCompilerShutdown();
#else
    // Stubs MinGW : glslang.lib (Vulkan SDK) est compilé avec MSVC — ABI incompatible.
    // La compilation GLSL→SPIR-V n'est pas disponible avec ce toolchain.
    inline NkGLSLCompileResult NkGLSLToSPIRV(NkShaderStage /*stage*/,
                                              const char*   /*glslSrc*/,
                                              const char*   /*entry*/ = "main") {
        NkGLSLCompileResult r;
        r.errorLog = "NkGLSLToSPIRV: non disponible avec MinGW (ABI MSVC incompatible)";
        return r;
    }
    inline void NkGLSLCompilerInit()     {}
    inline void NkGLSLCompilerShutdown() {}
#endif // !MinGW

#endif // NK_RHI_VK_ENABLED

} // namespace nkentseu
