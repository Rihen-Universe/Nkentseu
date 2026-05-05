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
// Activation : définir NKSL_HAS_SPIRV_CROSS dans votre CMakeLists.txt
//   find_package ou add_subdirectory(third_party/SPIRV-Cross)
//   target_link_libraries(NkSL PRIVATE spirv-cross-msl spirv-cross-core)
//   target_compile_definitions(NkSL PRIVATE NKSL_HAS_SPIRV_CROSS)
//
// =============================================================================
#include "NkSLCodeGen.h"
#include "NkSLCompiler.h"
#include "NKLogger/NkLog.h"

#define NKSL_MSL_LOG(...) logger_src.Infof("[NkSL/MSL-SC] " __VA_ARGS__)
#define NKSL_MSL_ERR(...) logger_src.Errorf("[NkSL/MSL-SC] " __VA_ARGS__)

#ifdef NKSL_HAS_SPIRV_CROSS
// SPIRV-Cross headers — disponibles via sous-module git ou package manager
// Chemin attendu : third_party/SPIRV-Cross/
#include <spirv_cross/spirv_msl.hpp>
#include <spirv_cross/spirv_parser.hpp>
#endif

namespace nkentseu {

// =============================================================================
// Generate — chemin complet : AST → GLSL → SPIR-V → MSL
// =============================================================================
NkSLCompileResult NkSLCodeGenMSLSpirvCross::Generate(
    NkSLProgramNode*          ast,
    NkSLStage                 stage,
    const NkSLCompileOptions& opts)
{
    NkSLCompileResult res;
    res.target = NkSLTarget::NK_MSL_SPIRV_CROSS;
    res.stage  = stage;

#ifndef NKSL_HAS_SPIRV_CROSS
    // SPIRV-Cross non disponible : fallback vers le générateur MSL natif
    NKSL_MSL_LOG("SPIRV-Cross not available, falling back to native MSL generator\n");
    NkSLCodeGen_MSL nativeMSL;
    auto nativeResult = nativeMSL.Generate(ast, stage, opts);
    nativeResult.target = NkSLTarget::NK_MSL_SPIRV_CROSS; // tag pour indiquer le fallback
    return nativeResult;
#else
    // Étape 1 : générer le GLSL intermédiaire depuis l'AST
    NkSLCodeGenGLSL glslGen;
    NkSLCompileOptions glslOpts = opts;
    glslOpts.glslVersion = 450;
    NkSLCompileResult glslRes = glslGen.Generate(ast, stage, glslOpts);
    if (!glslRes.success) {
        res.errors   = glslRes.errors;
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
        for (auto& w : glslRes.warnings) nativeResult.warnings.PushBack(w);
        return nativeResult;
    }

    // Étape 3 : SPIR-V → MSL via SPIRV-Cross
    // Convertir le bytecode uint8 en uint32
    NkVector<uint32> spirvWords;
    const uint8* data = spirvRes.bytecode.Data();
    uint32 wordCount = (uint32)spirvRes.bytecode.Size() / sizeof(uint32);
    spirvWords.Reserve(wordCount);
    for (uint32 i = 0; i < wordCount; i++) {
        uint32 w;
        w  = (uint32)data[i*4+0];
        w |= (uint32)data[i*4+1] << 8;
        w |= (uint32)data[i*4+2] << 16;
        w |= (uint32)data[i*4+3] << 24;
        spirvWords.PushBack(w);
    }

    res = SPIRVToMSL(spirvWords, stage, opts);

    // Propager les warnings des étapes précédentes
    for (auto& w : glslRes.warnings)  res.warnings.PushBack(w);
    for (auto& w : spirvRes.warnings) res.warnings.PushBack(w);

    return res;
#endif
}

// =============================================================================
// GenerateFromSPIRV — chemin direct depuis le SPIR-V binaire
// =============================================================================
NkSLCompileResult NkSLCodeGenMSLSpirvCross::GenerateFromSPIRV(
    const NkVector<uint32>&   spirvWords,
    NkSLStage                 stage,
    const NkSLCompileOptions& opts)
{
    NkSLCompileResult res;
    res.target = NkSLTarget::NK_MSL_SPIRV_CROSS;
    res.stage  = stage;

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
NkSLCompileResult NkSLCodeGenMSLSpirvCross::SPIRVToMSL(
    const NkVector<uint32>&   spirvWords,
    NkSLStage                 stage,
    const NkSLCompileOptions& opts)
{
    NkSLCompileResult res;
    res.target = NkSLTarget::NK_MSL_SPIRV_CROSS;
    res.stage  = stage;

#ifdef NKSL_HAS_SPIRV_CROSS
    try {
        // Construire le vecteur STL requis par SPIRV-Cross
        std::vector<uint32_t> spvData(spirvWords.Data(),
                                       spirvWords.Data() + spirvWords.Size());

        // Créer le compilateur MSL
        spirv_cross::CompilerMSL compiler(std::move(spvData));

        // Options MSL
        spirv_cross::CompilerMSL::Options mslOptions;

        // Version MSL selon les options
        uint32 mslVer = opts.mslVersion;
        if (mslVer >= 300) {
            mslOptions.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(3, 0);
        } else if (mslVer >= 210) {
            mslOptions.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 1);
        } else {
            mslOptions.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 0);
        }

        // Platform (iOS vs macOS)
        // On utilise macOS par défaut, iOS si demandé via les options
        mslOptions.platform = spirv_cross::CompilerMSL::Options::macOS;

        // Flip viewport Y si demandé (utile pour compatibilité OpenGL)
        mslOptions.flip_vert_y = opts.flipUVY;

        // Activer les argument buffers pour le bindless (Metal 3+)
        // mslOptions.argument_buffers = (mslVer >= 300);

        compiler.set_msl_options(mslOptions);

        // Options SPIRV-Cross générales
        spirv_cross::CompilerGLSL::Options commonOptions;
        commonOptions.emit_line_directives = opts.debugInfo;
        compiler.set_common_options(commonOptions);

        // Compilation
        std::string mslSource = compiler.compile();

        if (mslSource.empty()) {
            res.success = false;
            res.AddError(0, "SPIRV-Cross produced empty MSL output");
            NKSL_MSL_ERR("SPIRV-Cross produced empty MSL output\n");
            return res;
        }

        res.source  = NkString(mslSource.c_str());
        res.success = true;

        // Copier le source dans le bytecode (pour la cohérence avec les autres backends)
        res.bytecode.Reserve((uint32)res.source.Size());
        for (uint32 i = 0; i < (uint32)res.source.Size(); i++)
            res.bytecode.PushBack((uint8)res.source[i]);

        NKSL_MSL_LOG("SPIRV-Cross MSL generation succeeded (%u bytes)\n",
                     (uint32)mslSource.size());
    }
    catch (const spirv_cross::CompilerError& e) {
        res.success = false;
        res.AddError(0, NkString("SPIRV-Cross error: ") + NkString(e.what()));
        NKSL_MSL_ERR("SPIRV-Cross exception: %s\n", e.what());
    }
    catch (...) {
        res.success = false;
        res.AddError(0, "SPIRV-Cross unknown exception");
        NKSL_MSL_ERR("SPIRV-Cross unknown exception\n");
    }
#else
    res.success = false;
    res.AddError(0, "SPIRV-Cross not compiled in. Define NKSL_HAS_SPIRV_CROSS.");
#endif

    return res;
}

} // namespace nkentseu
