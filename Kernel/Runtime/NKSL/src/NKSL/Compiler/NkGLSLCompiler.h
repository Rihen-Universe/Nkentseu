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
			bool success = false;
			NkVector<uint32> spirv;			// mots SPIR-V (vides si échec)
			const char *errorLog = nullptr; // pointeur vers buffer statique interne
	};

	/// (10/11/2026, plan RTX etape 0) `spirvVersion` : la version SPIR-V du NUANCEUR (100 = 1.0, le
	/// defaut d'avant ; 103 / 104 / 105 / 106), le client Vulkan suit (NkSLCompileOptions.spirvVersion).
	NkGLSLCompileResult NkGLSLToSPIRV(NkSLStage stage, const char *glslSrc, const char *entry = "main", uint32 spirvVersion = 100u);
	void NkGLSLCompilerInit();
	void NkGLSLCompilerShutdown();

} // namespace nkentseu
