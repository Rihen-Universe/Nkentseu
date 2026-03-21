#pragma once
// =============================================================================
// NkSLCompiler.h
// Point d'entrée unique du compilateur NkSL.
//
// Usage :
//   NkSLCompiler compiler;
//   auto result = compiler.Compile(source, NkSLStage::VERTEX, NkSLTarget::GLSL);
//   if (result.success) { ... result.source ... }
//
// Pour Vulkan, la compilation GLSL→SPIR-V est faite via glslang si disponible,
// sinon le GLSL est retourné directement (le driver peut compiler à la volée).
// =============================================================================
#include "NkSLTypes.h"
#include "NkSLSemantic.h"
#include "NkSLCodeGen.h"
#include "NkSLCodeGen.h"

#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"
#include "NKLogger/NkSync.h"
#include "NKRenderer/RHI/Core/NkRhiTypes.h"
#include "NKRenderer/RHI/Core/NkRhiDescs.h"

namespace nkentseu {

// =============================================================================
// Cache de shaders compilés (on-disk, keyed par hash source+target+stage)
// =============================================================================
struct NkSLCacheEntry {
    uint64  sourceHash = 0;
    NkSLTarget target  = NkSLTarget::GLSL;
    NkSLStage  stage   = NkSLStage::VERTEX;
    NkVector<uint8> bytecode;
    NkString        source;
    uint64  timestamp  = 0;
    bool    isSpirv    = false;
};

class NkSLCache {
public:
    void SetDirectory(const NkString& dir) {
        loggersync::NkScopedLock lock(mMutex);
        mCacheDir = dir;
    }
public:
    explicit NkSLCache(const NkString& cacheDir = "");

    bool        Has(uint64 hash, NkSLTarget t, NkSLStage s) const;
    bool        Get(uint64 hash, NkSLTarget t, NkSLStage s, NkSLCacheEntry& out) const;
    void        Put(uint64 hash, NkSLTarget t, NkSLStage s, const NkSLCompileResult& r);
    void        Clear();
    void        Flush();   // écriture sur disque
    void        Load();    // lecture depuis disque

    uint32      Size() const { return (uint32)mEntries.Size(); }

private:
    uint64 MakeKey(uint64 hash, NkSLTarget t, NkSLStage s) const;

    NkString                  mCacheDir;
    NkVector<NkSLCacheEntry>  mEntries;
    mutable threading::NkMutex           mMutex;
};

// =============================================================================
// NkSLCompiler — compilateur principal
// =============================================================================
class NkSLCompiler {
public:
    explicit NkSLCompiler(const NkString& cacheDir = "");
    ~NkSLCompiler() = default;

    // ── Compilation d'un seul stage ──────────────────────────────────────────
    NkSLCompileResult Compile(
        const NkString&          source,
        NkSLStage                stage,
        NkSLTarget               target,
        const NkSLCompileOptions& opts   = {},
        const NkString&          filename= "shader"
    );

    // ── Compilation de tous les targets depuis une source unique ──────────────
    // Pratique pour produire le NkShaderDesc complet en une seule passe
    struct MultiTargetResult {
        NkVector<NkSLCompileResult> results;  // un par target demandé
        bool allSucceeded() const {
            for (auto& r : results) if (!r.success) return false;
            return true;
        }
    };

    MultiTargetResult CompileAllTargets(
        const NkString&           source,
        NkSLStage                 stage,
        const NkVector<NkSLTarget>& targets,
        const NkSLCompileOptions&  opts    = {},
        const NkString&            filename= "shader"
    );

    // ── Compilation GLSL→SPIR-V via glslang ──────────────────────────────────
    // Retourne un bytecode SPIR-V binaire si glslang est disponible.
    // Sinon retourne le GLSL texte (Vulkan peut le compiler à la volée avec
    // VK_NV_glsl_shader, mais ce n'est pas recommandé en production).
    NkSLCompileResult CompileToSPIRV(
        const NkString&          glslSource,
        NkSLStage                stage,
        const NkSLCompileOptions& opts = {}
    );

    // ── Production d'un NkShaderDesc prêt pour NkIDevice ─────────────────────
    // Compile le shader NkSL et remplit automatiquement le NkShaderDesc
    // avec le bytecode approprié selon l'API cible.
    bool FillShaderDesc(
        const NkString&          nkslSource,
        NkSLStage                stage,
        NkSLTarget               targetApi,
        NkShaderDesc&            outDesc,
        const NkSLCompileOptions& opts     = {},
        const NkString&          filename  = "shader"
    );

    // ── Configuration ─────────────────────────────────────────────────────────
    void SetCacheDir(const NkString& dir);
    void SetGlslangPath(const NkString& path) { mGlslangPath = path; }
    void EnableCache(bool v)  { mCacheEnabled = v; }
    void EnableDebug(bool v)  { mDebugMode    = v; }

    // ── Accès au cache ────────────────────────────────────────────────────────
    NkSLCache& GetCache() { return mCache; }

    // ── Compilation avec analyse sémantique complète ─────────────────────────
    // Identique à Compile() mais passe par NkSLSemantic avant la génération.
    // Détecte les erreurs de type, les variables non déclarées, les swizzles
    // invalides, les conflits de binding, etc.
    NkSLCompileResult CompileWithSemantic(
        const NkString&          source,
        NkSLStage                stage,
        NkSLTarget               target,
        const NkSLCompileOptions& opts    = {},
        const NkString&          filename = "shader"
    );

    // ── Validation ────────────────────────────────────────────────────────────
    // Vérifie la syntaxe sans générer de code cible
    NkVector<NkSLCompileError> Validate(const NkString& source,
                                         const NkString& filename = "shader");

    // ── Prétraitement : résoudre les #include ─────────────────────────────────
    NkString Preprocess(const NkString& source,
                         const NkString& baseDir = "",
                         NkVector<NkSLCompileError>* errors = nullptr);

private:
    uint64  HashSource(const NkString& src, NkSLStage stage, NkSLTarget target,
                        const NkSLCompileOptions& opts) const;

    // Compilation GLSL→SPIR-V via sous-processus glslangValidator
    // ou via l'API glslang si linkée
    bool    GLSLtoSPIRV_glslang(const NkString& glsl, NkSLStage stage,
                                 const NkSLCompileOptions& opts,
                                 NkVector<uint8>& spirvOut,
                                 NkVector<NkSLCompileError>& errors);

    // Compilation GLSL→SPIR-V via spirv-cross / shaderc si disponible
    bool    GLSLtoSPIRV_shaderc(const NkString& glsl, NkSLStage stage,
                                 const NkSLCompileOptions& opts,
                                 NkVector<uint8>& spirvOut,
                                 NkVector<NkSLCompileError>& errors);

    NkSLCache  mCache;
    NkString   mGlslangPath;
    bool       mCacheEnabled = true;
    bool       mDebugMode    = false;
};

// =============================================================================
// NkSLShaderLibrary — gestion de bibliothèques de shaders NkSL
// Charge des fichiers .nksl, les compile à la demande, les garde en cache.
// =============================================================================
class NkSLShaderLibrary {
public:
    struct ShaderEntry {
        NkString                  name;
        NkString                  sourcePath;
        NkString                  source;
        NkVector<NkSLStage>       stages;
        NkVector<NkSLCompileResult> compiled; // indexé par stage
        bool                      dirty = true;
    };

    explicit NkSLShaderLibrary(NkSLCompiler* compiler,
                                const NkString& baseDir = "");

    // Enregistrer un shader par nom
    bool   Register(const NkString& name,
                     const NkString& filePath,
                     const NkVector<NkSLStage>& stages);

    bool   RegisterInline(const NkString& name,
                           const NkString& source,
                           const NkVector<NkSLStage>& stages);

    // Compiler tous les shaders enregistrés pour une cible API
    bool   CompileAll(NkSLTarget target,
                       const NkSLCompileOptions& opts = {});

    // Obtenir le résultat pour un shader/stage/target
    const NkSLCompileResult* Get(const NkString& name,
                                  NkSLStage stage,
                                  NkSLTarget target) const;

    // Remplir un NkShaderDesc complet
    bool   FillShaderDesc(const NkString& name,
                           NkSLTarget target,
                           NkShaderDesc& outDesc,
                           const NkSLCompileOptions& opts = {}) const;

    // Hot-reload : recompile les shaders dont le fichier source a changé
    uint32 HotReload(NkSLTarget target, const NkSLCompileOptions& opts = {});

    uint32 Count() const { return (uint32)mEntries.Size(); }

private:
    NkSLCompiler*            mCompiler = nullptr;
    NkString                 mBaseDir;
    NkVector<ShaderEntry>    mEntries;
    // Cache des résultats indexé par (name, stage, target)
    mutable NkUnorderedMap<uint64, NkSLCompileResult> mResultCache;
};

} // namespace nkentseu