// =============================================================================
// NkSLCodeGenMSL_SpirvCross.cpp  — v3.0
//
// Backend MSL via SPIRV-Cross embarqué.
// Ce fichier implémente NkSLCodeGenMSLSpirvCross qui prend du SPIR-V binaire
// et produit du MSL via la bibliothèque SPIRV-Cross (embarquée en sous-module).
//
// Avantages vs le générateur MSL natif :
//   - Gère tous les cas edge (geometry shaders, tessellation, ray tracing)
//   - Testé extensivement par la communauté Khronos
//   - Gestion correcte de l'espace d'adressage Metal
//   - Meilleure gestion des samplers shadow et tableaux de textures
//
// Activation : NKSL_HAS_SPIRV_CROSS, pose par NKSL.jenga sous macOS quand
// NKGLSlang et NKSPIRVCross sont construits (2026-09-30). Avant, aucun jenga ne
// le definissait : tout NkSL -> MSL tombait sur le generateur natif, dont les
// samplers, les push constants et les shaders plein ecran sans attribut ne
// correspondaient pas a ce que NkMetalDevice lie.
//
// CHAINE : AST -> GLSL VULKAN (le meme que pour Vulkan : set/binding, push
// constants) -> SPIR-V (glslang) -> MSL par NkShaderConverter::SpirvToMsl, qui
// porte LA convention de liaison (NkMslConventions.h). Un seul convertisseur
// SPIR-V -> MSL dans le moteur : celui-ci et le chemin .vk.glsl ne peuvent plus
// diverger.
// =============================================================================
#include "NKSL/CodeGen/NkSLCodeGen.h"
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKLogger/NkLog.h"

#define NKSL_MSL_LOG(...) logger_src.Infof("[NkSL/MSL-SC] " __VA_ARGS__)
#define NKSL_MSL_ERR(...) logger_src.Errorf("[NkSL/MSL-SC] " __VA_ARGS__)


namespace nkentseu {

	// =============================================================================
	// Generate — chemin complet : AST → GLSL → SPIR-V → MSL
	// =============================================================================
	NkSLCompileResult NkSLCodeGenMSLSpirvCross::Generate(NkSLProgramNode *ast, NkSLStage stage,
														 const NkSLCompileOptions &opts) {
		NkSLCompileResult res;
		res.target = NkSLTarget::NK_MSL_SPIRV_CROSS;
		res.stage = stage;

#ifndef NKSL_HAS_SPIRV_CROSS
		// SPIRV-Cross non disponible : fallback vers le générateur MSL natif
		NKSL_MSL_LOG("SPIRV-Cross not available, falling back to native MSL generator\n");
		NkSLCodeGen_MSL nativeMSL;
		auto nativeResult = nativeMSL.Generate(ast, stage, opts);
		nativeResult.target = NkSLTarget::NK_MSL_SPIRV_CROSS; // tag pour indiquer le fallback
		return nativeResult;
#else
		// Étape 1 : GLSL VULKAN depuis l'AST (set/binding et push constants
		// explicites, comme la cible NK_SPIRV). Le GLSL OpenGL d'avant perdait
		// les ensembles et les push constants : ils n'arrivaient pas au SPIR-V.
		NkSLCodeGenGLSLVulkan glslGen;
		NkSLCompileResult glslRes = glslGen.Generate(ast, stage, opts);
		if (!glslRes.success) {
			res.errors = glslRes.errors;
			res.warnings = glslRes.warnings;
			NKSL_MSL_ERR("GLSL generation failed before SPIR-V conversion\n");
			return res;
		}

		// Étape 2 : GLSL → SPIR-V via glslang embarqué
		NkSLCompiler tempCompiler;
		NkSLCompileResult spirvRes = tempCompiler.CompileToSPIRV(glslRes.source, stage, opts);
		if (!spirvRes.success) {
			// Si pas de compilateur SPIR-V dispo, fallback MSL natif
			NKSL_MSL_LOG("No SPIR-V compiler, falling back to native MSL generator\n");
			NkSLCodeGen_MSL nativeMSL;
			auto nativeResult = nativeMSL.Generate(ast, stage, opts);
			nativeResult.target = NkSLTarget::NK_MSL_SPIRV_CROSS;
			// Propager les warnings GLSL
			for (auto &w : glslRes.warnings)
				nativeResult.warnings.PushBack(w);
			return nativeResult;
		}

		// Étape 3 : SPIR-V → MSL via SPIRV-Cross
		// Convertir le bytecode uint8 en uint32
		NkVector<uint32> spirvWords;
		const uint8 *data = spirvRes.bytecode.Data();
		uint32 wordCount = (uint32)spirvRes.bytecode.Size() / sizeof(uint32);
		spirvWords.Reserve(wordCount);
		for (uint32 i = 0; i < wordCount; i++) {
			uint32 w;
			w = (uint32)data[i * 4 + 0];
			w |= (uint32)data[i * 4 + 1] << 8;
			w |= (uint32)data[i * 4 + 2] << 16;
			w |= (uint32)data[i * 4 + 3] << 24;
			spirvWords.PushBack(w);
		}

		res = SPIRVToMSL(spirvWords, stage, opts);

		// Propager les warnings des étapes précédentes
		for (auto &w : glslRes.warnings)
			res.warnings.PushBack(w);
		for (auto &w : spirvRes.warnings)
			res.warnings.PushBack(w);

		return res;
#endif
	}

	// =============================================================================
	// GenerateFromSPIRV — chemin direct depuis le SPIR-V binaire
	// =============================================================================
	NkSLCompileResult NkSLCodeGenMSLSpirvCross::GenerateFromSPIRV(const NkVector<uint32> &spirvWords, NkSLStage stage,
																  const NkSLCompileOptions &opts) {
		NkSLCompileResult res;
		res.target = NkSLTarget::NK_MSL_SPIRV_CROSS;
		res.stage = stage;

#ifndef NKSL_HAS_SPIRV_CROSS
		res.success = false;
		res.AddError(0, "SPIRV-Cross not available. Build with NKSL_HAS_SPIRV_CROSS.");
		return res;
#else
		return SPIRVToMSL(spirvWords, stage, opts);
#endif
	}

	// =============================================================================
	// SPIRVToMSL — conversion SPIR-V → MSL via SPIRV-Cross
	// =============================================================================
	NkSLCompileResult NkSLCodeGenMSLSpirvCross::SPIRVToMSL(const NkVector<uint32> &spirvWords, NkSLStage stage,
														   const NkSLCompileOptions &opts) {
		NkSLCompileResult res;
		res.target = NkSLTarget::NK_MSL_SPIRV_CROSS;
		res.stage = stage;

#ifdef NKSL_HAS_SPIRV_CROSS
		// La conversion et SA CONVENTION DE LIAISON vivent dans NkShaderConverter
		// (options MSL, push constants en buffer(30), samplers constexpr au-dela
		// de 16) : voir NkMslConventions.h. opts.mslVersion / flipUVY ne sont plus
		// lus ici -- le MSL suit les conventions Vulkan du moteur (NDC, profondeur
		// [0,1]), que NkMetalCommandBuffer reproduit (viewport).
		(void)opts;
		NkShaderConvertResult conv =
			NkShaderConverter::SpirvToMsl(spirvWords.Data(), (uint32)spirvWords.Size(), stage);
		if (!conv.success || conv.source.Empty()) {
			res.success = false;
			res.AddError(0, NkString("SPIRV-Cross error: ") + conv.errors);
			NKSL_MSL_ERR("SPIRV-Cross : %s\n", conv.errors.CStr());
			return res;
		}
		res.source = conv.source;
		res.success = true;

		// Copier le source dans le bytecode (pour la cohérence avec les autres backends)
		res.bytecode.Reserve((uint32)res.source.Size());
		for (uint32 i = 0; i < (uint32)res.source.Size(); i++)
			res.bytecode.PushBack((uint8)res.source[i]);
#else
		res.success = false;
		res.AddError(0, "SPIRV-Cross not compiled in. Define NKSL_HAS_SPIRV_CROSS.");
#endif

		return res;
	}

} // namespace nkentseu
