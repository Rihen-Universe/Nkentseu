#pragma once
// =============================================================================
// NkSLCompiler.h  — v3.0
//
// Nouveautés v3.0 :
//   - CompileWithReflection() : compile + extrait la reflection en une passe
//   - Support MSL_SPIRV_CROSS comme target
//   - glslang embarqué (standalone, sans Vulkan SDK)
//   - SPIRV-Cross embarqué (sous-module git)
//   - Cache thread-safe amélioré
// =============================================================================
#include "NkSLTypes.h"
#include "NkSLSemantic.h"
#include "NkSLCodeGen.h"

#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"
#include "NKLogger/NkSync.h"
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkDescs.h"

namespace nkentseu {
    // =============================================================================
    // Cache de shaders compilés
    // =============================================================================
    struct NkSLCacheEntry {
        uint64  sourceHash = 0;
        NkSLTarget target  = NkSLTarget::NK_GLSL;
        NkSLStage  stage   = NkSLStage::NK_VERTEX;
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
        void        Flush();
        void        Load();
        uint32      Size() const { return (uint32)mEntries.Size(); }

    private:
        uint64 MakeKey(uint64 hash, NkSLTarget t, NkSLStage s) const;
        NkString                  mCacheDir;
        NkVector<NkSLCacheEntry>  mEntries;
        mutable threading::NkMutex mMutex;
    };

    // =============================================================================
    // Résultat de compilation avec reflection
    // =============================================================================
    struct NkSLCompileResultWithReflection {
        NkSLCompileResult result;
        NkSLReflection    reflection;
        bool              hasReflection = false;
    };

    // =============================================================================
    // NkSLCompiler — compilateur principal v3.0
    // =============================================================================
    class NkSLCompiler {
    public:
        explicit NkSLCompiler(const NkString& cacheDir = "");
        ~NkSLCompiler() = default;

        // ── Compilation simple ────────────────────────────────────────────────────
        NkSLCompileResult Compile(
            const NkString&          source,
            NkSLStage                stage,
            NkSLTarget               target,
            const NkSLCompileOptions& opts    = {},
            const NkString&          filename = "shader"
        );

        // ── Compilation avec reflection automatique ───────────────────────────────
        // Compile ET extrait tous les bindings/vertex inputs depuis l'AST.
        // C'est la fonction recommandée pour une utilisation production.
        NkSLCompileResultWithReflection CompileWithReflection(
            const NkString&          source,
            NkSLStage                stage,
            NkSLTarget               target,
            const NkSLCompileOptions& opts    = {},
            const NkString&          filename = "shader"
        );

        // ── Reflection seule (sans compilation) ──────────────────────────────────
        // Extrait les informations de binding depuis le source NkSL
        // sans générer de code cible. Utile pour le tooling.
        NkSLReflection Reflect(
            const NkString& source,
            NkSLStage       stage,
            const NkString& filename = "shader"
        );

        // ── Compilation multi-targets ─────────────────────────────────────────────
        struct MultiTargetResult {
            NkVector<NkSLCompileResult> results;
            NkSLReflection              reflection; // partagée entre tous les targets
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

        // ── SPIR-V ────────────────────────────────────────────────────────────────
        // Compilation GLSL→SPIR-V via glslang embarqué (standalone, sans Vulkan SDK)
        NkSLCompileResult CompileToSPIRV(
            const NkString&          glslSource,
            NkSLStage                stage,
            const NkSLCompileOptions& opts = {}
        );

        // ── MSL via SPIRV-Cross ───────────────────────────────────────────────────
        // Chemin GLSL→SPIR-V→MSL, plus robuste pour les cas edge
        NkSLCompileResult CompileToMSL_SpirvCross(
            const NkString&          source,
            NkSLStage                stage,
            const NkSLCompileOptions& opts    = {},
            const NkString&          filename = "shader"
        );

        // ── Production d'un NkShaderDesc ─────────────────────────────────────────
        bool FillShaderDesc(
            const NkString&          nkslSource,
            NkSLStage                stage,
            NkSLTarget               targetApi,
            NkShaderDesc&            outDesc,
            const NkSLCompileOptions& opts     = {},
            const NkString&          filename  = "shader"
        );

        // ── Avec reflection automatique du layout ─────────────────────────────────
        // Remplit le NkShaderDesc ET retourne la reflection
        bool FillShaderDescWithReflection(
            const NkString&          nkslSource,
            NkSLStage                stage,
            NkSLTarget               targetApi,
            NkShaderDesc&            outDesc,
            NkSLReflection&          outReflection,
            const NkSLCompileOptions& opts     = {},
            const NkString&          filename  = "shader"
        );

        // ── Validation ────────────────────────────────────────────────────────────
        NkVector<NkSLCompileError> Validate(const NkString& source,
                                            const NkString& filename = "shader");

        // ── Prétraitement ─────────────────────────────────────────────────────────
        NkString Preprocess(const NkString& source,
                            const NkString& baseDir = "",
                            NkVector<NkSLCompileError>* errors = nullptr);

        // ── Analyse sémantique complète ───────────────────────────────────────────
        NkSLCompileResult CompileWithSemantic(
            const NkString&          source,
            NkSLStage                stage,
            NkSLTarget               target,
            const NkSLCompileOptions& opts    = {},
            const NkString&          filename = "shader"
        );

        // ── Configuration ─────────────────────────────────────────────────────────
        void SetCacheDir(const NkString& dir);
        void SetGlslangPath(const NkString& path) { mGlslangPath = path; }
        void EnableCache(bool v)  { mCacheEnabled = v; }
        void EnableDebug(bool v)  { mDebugMode    = v; }

        NkSLCache& GetCache() { return mCache; }

    private:
        uint64 HashSource(const NkString& src, NkSLStage stage, NkSLTarget target,
                        const NkSLCompileOptions& opts) const;

        // Compilation SPIR-V via glslang embarqué (standalone)
        bool GLSLtoSPIRV_glslang(const NkString& glsl, NkSLStage stage,
                                const NkSLCompileOptions& opts,
                                NkVector<uint8>& spirvOut,
                                NkVector<NkSLCompileError>& errors);

        // Compilation SPIR-V via shaderc (Vulkan SDK, optionnel)
        bool GLSLtoSPIRV_shaderc(const NkString& glsl, NkSLStage stage,
                                const NkSLCompileOptions& opts,
                                NkVector<uint8>& spirvOut,
                                NkVector<NkSLCompileError>& errors);

        NkSLCache  mCache;
        NkString   mGlslangPath;
        bool       mCacheEnabled = true;
        bool       mDebugMode    = false;
    };

    // =============================================================================
    // NkSLShaderLibrary — bibliothèque de shaders avec hot-reload
    // =============================================================================
    class NkSLShaderLibrary {
    public:
        struct ShaderEntry {
            NkString                  name;
            NkString                  sourcePath;
            NkString                  source;
            NkVector<NkSLStage>       stages;
            NkVector<NkSLCompileResult> compiled;
            NkSLReflection            reflection;  // ← NOUVEAU: reflection intégrée
            bool                      dirty = true;
        };

        explicit NkSLShaderLibrary(NkSLCompiler* compiler,
                                    const NkString& baseDir = "");

        bool   Register(const NkString& name,
                        const NkString& filePath,
                        const NkVector<NkSLStage>& stages);

        bool   RegisterInline(const NkString& name,
                            const NkString& source,
                            const NkVector<NkSLStage>& stages);

        bool   CompileAll(NkSLTarget target,
                        const NkSLCompileOptions& opts = {});

        const NkSLCompileResult* Get(const NkString& name,
                                    NkSLStage stage,
                                    NkSLTarget target) const;

        // NOUVEAU: accès à la reflection d'un shader
        const NkSLReflection* GetReflection(const NkString& name) const;

        bool   FillShaderDesc(const NkString& name,
                            NkSLTarget target,
                            NkShaderDesc& outDesc,
                            const NkSLCompileOptions& opts = {}) const;

        // NOUVEAU: FillShaderDesc + reflection en une passe
        bool   FillShaderDescWithReflection(const NkString& name,
                                            NkSLTarget target,
                                            NkShaderDesc& outDesc,
                                            NkSLReflection& outReflection,
                                            const NkSLCompileOptions& opts = {}) const;

        uint32 HotReload(NkSLTarget target, const NkSLCompileOptions& opts = {});
        uint32 Count() const { return (uint32)mEntries.Size(); }

    private:
        NkSLCompiler*         mCompiler = nullptr;
        NkString              mBaseDir;
        NkVector<ShaderEntry> mEntries;
        mutable NkUnorderedMap<uint64, NkSLCompileResult> mResultCache;
    };

} // namespace nkentseu
