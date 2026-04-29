#pragma once
// =============================================================================
// NkSLCompiler.h  — v4.0
//
// CORRECTIONS :
//   - NkSLCache : NkUnorderedMap<uint64, NkSLCacheEntry> → O(1)
//   - glslang init : std::call_once (thread-safe)
//   - Preprocess() : signature étendue avec garde d'inclusion
//   - NkSLShaderLibrary : NkSLMutexLock sur toutes les méthodes publiques
//   - FillShaderDesc : compatible NK_GLSL_VULKAN, NK_HLSL_DX11, NK_HLSL_DX12
// =============================================================================
#include "NkSLTypes.h"
#include "NkSLSemantic.h"
#include "NkSLCodeGen.h"

#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"

#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkDescs.h"

// Alias local pour le lock — évite l'ambiguïté NkScopedLock/NkScopedMutex
using NkSLMutexLock = NkScopedLock;

namespace nkentseu {

    // =============================================================================
    // Entrée de cache
    // =============================================================================
    struct NkSLCacheEntry {
        uint64          sourceHash = 0;
        NkSLTarget      target     = NkSLTarget::NK_GLSL;
        NkSLStage       stage      = NkSLStage::NK_VERTEX;
        NkVector<uint8> bytecode;
        NkString        source;
        uint64          timestamp  = 0;
        bool            isSpirv    = false;
    };

    // =============================================================================
    // NkSLCache — O(1) via NkUnorderedMap
    // =============================================================================
    class NkSLCache {
    public:
        explicit NkSLCache(const NkString& cacheDir = "");

        void SetDirectory(const NkString& dir) {
            NkSLMutexLock lock(mMutex);
            mCacheDir = dir;
        }

        bool   Has(uint64 hash, NkSLTarget t, NkSLStage s) const;
        bool   Get(uint64 hash, NkSLTarget t, NkSLStage s, NkSLCacheEntry& out) const;
        void   Put(uint64 hash, NkSLTarget t, NkSLStage s, const NkSLCompileResult& r);
        void   Clear();
        void   Flush();
        void   Load();
        uint32 Size() const {
            NkSLMutexLock lock(mMutex);
            return (uint32)mEntries.Size();
        }

    private:
        uint64 MakeKey(uint64 hash, NkSLTarget t, NkSLStage s) const;

        NkString                                   mCacheDir;
        NkUnorderedMap<uint64, NkSLCacheEntry>     mEntries; // O(1) lookup
        mutable threading::NkMutex                 mMutex;
    };

    // =============================================================================
    // Résultat avec reflection
    // =============================================================================
    struct NkSLCompileResultWithReflection {
        NkSLCompileResult result;
        NkSLReflection    reflection;
        bool              hasReflection = false;
    };

    // =============================================================================
    // NkSLCompiler — v4.0
    // =============================================================================
    class NkSLCompiler {
    public:
        explicit NkSLCompiler(const NkString& cacheDir = "");
        ~NkSLCompiler() = default;

        // ── Compilation simple ────────────────────────────────────────────────────
        NkSLCompileResult Compile(
            const NkString&           source,
            NkSLStage                 stage,
            NkSLTarget                target,
            const NkSLCompileOptions& opts     = {},
            const NkString&           filename = "shader"
        );

        // ── Compilation + reflection en une passe ─────────────────────────────────
        NkSLCompileResultWithReflection CompileWithReflection(
            const NkString&           source,
            NkSLStage                 stage,
            NkSLTarget                target,
            const NkSLCompileOptions& opts     = {},
            const NkString&           filename = "shader"
        );

        // ── Reflection seule (sans compilation) ──────────────────────────────────
        NkSLReflection Reflect(
            const NkString& source,
            NkSLStage       stage,
            const NkString& filename = "shader"
        );

        // ── Compilation multi-targets ─────────────────────────────────────────────
        struct MultiTargetResult {
            NkVector<NkSLCompileResult> results;
            NkSLReflection              reflection;
            bool allSucceeded() const {
                for (auto& r : results) if (!r.success) return false;
                return true;
            }
        };

        MultiTargetResult CompileAllTargets(
            const NkString&            source,
            NkSLStage                  stage,
            const NkVector<NkSLTarget>& targets,
            const NkSLCompileOptions&  opts     = {},
            const NkString&            filename = "shader"
        );

        // ── SPIR-V ────────────────────────────────────────────────────────────────
        NkSLCompileResult CompileToSPIRV(
            const NkString&           glslSource,
            NkSLStage                 stage,
            const NkSLCompileOptions& opts = {}
        );

        // ── MSL via SPIRV-Cross ───────────────────────────────────────────────────
        NkSLCompileResult CompileToMSL_SpirvCross(
            const NkString&           source,
            NkSLStage                 stage,
            const NkSLCompileOptions& opts     = {},
            const NkString&           filename = "shader"
        );

        // ── NkShaderDesc ──────────────────────────────────────────────────────────
        bool FillShaderDesc(
            const NkString&           nkslSource,
            NkSLStage                 stage,
            NkSLTarget                targetApi,
            NkShaderDesc&             outDesc,
            const NkSLCompileOptions& opts     = {},
            const NkString&           filename = "shader"
        );

        bool FillShaderDescWithReflection(
            const NkString&           nkslSource,
            NkSLStage                 stage,
            NkSLTarget                targetApi,
            NkShaderDesc&             outDesc,
            NkSLReflection&           outReflection,
            const NkSLCompileOptions& opts     = {},
            const NkString&           filename = "shader"
        );

        // ── Validation ────────────────────────────────────────────────────────────
        NkVector<NkSLCompileError> Validate(const NkString& source,
                                             const NkString& filename = "shader");

        // ── Prétraitement (avec garde #pragma once) ───────────────────────────────
        NkString Preprocess(const NkString& source,
                            const NkString& baseDir  = "",
                            NkVector<NkSLCompileError>* errors = nullptr,
                            NkVector<NkString>* includedFiles  = nullptr);

        // ── Compilation avec analyse sémantique ──────────────────────────────────
        NkSLCompileResult CompileWithSemantic(
            const NkString&           source,
            NkSLStage                 stage,
            NkSLTarget                target,
            const NkSLCompileOptions& opts     = {},
            const NkString&           filename = "shader"
        );

        // ── Configuration ─────────────────────────────────────────────────────────
        void SetCacheDir    (const NkString& dir);
        void SetGlslangPath (const NkString& path) { mGlslangPath = path; }
        void EnableCache    (bool v)  { mCacheEnabled = v; }
        void EnableDebug    (bool v)  { mDebugMode    = v; }

        NkSLCache& GetCache() { return mCache; }

    private:
        uint64 HashSource(const NkString& src, NkSLStage stage, NkSLTarget target,
                          const NkSLCompileOptions& opts) const;

        bool GLSLtoSPIRV_glslang(const NkString& glsl, NkSLStage stage,
                                  const NkSLCompileOptions& opts,
                                  NkVector<uint8>& spirvOut,
                                  NkVector<NkSLCompileError>& errors);

        bool GLSLtoSPIRV_shaderc(const NkString& glsl, NkSLStage stage,
                                  const NkSLCompileOptions& opts,
                                  NkVector<uint8>& spirvOut,
                                  NkVector<NkSLCompileError>& errors);

        NkSLCache  mCache;
        NkString   mGlslangPath;
        NkString   filename;   // utilisé dans Preprocess pour les directives #line
        bool       mCacheEnabled = true;
        bool       mDebugMode    = false;
    };

    // =============================================================================
    // NkSLShaderLibrary — bibliothèque avec hot-reload, thread-safe
    // =============================================================================
    class NkSLShaderLibrary {
    public:
        struct ShaderEntry {
            NkString                    name;
            NkString                    sourcePath;
            NkString                    source;
            NkVector<NkSLStage>         stages;
            NkVector<NkSLCompileResult> compiled;
            NkSLReflection              reflection;
            bool                        dirty = true;
        };

        explicit NkSLShaderLibrary(NkSLCompiler* compiler,
                                    const NkString& baseDir = "");

        bool Register(const NkString& name, const NkString& filePath,
                      const NkVector<NkSLStage>& stages);

        bool RegisterInline(const NkString& name, const NkString& source,
                            const NkVector<NkSLStage>& stages);

        bool   CompileAll(NkSLTarget target, const NkSLCompileOptions& opts = {});

        // Accès thread-safe (mutex interne)
        const NkSLCompileResult* Get(const NkString& name, NkSLStage stage,
                                     NkSLTarget target) const;
        const NkSLReflection*    GetReflection(const NkString& name) const;

        bool FillShaderDesc(const NkString& name, NkSLTarget target,
                            NkShaderDesc& outDesc,
                            const NkSLCompileOptions& opts = {}) const;

        bool FillShaderDescWithReflection(const NkString& name, NkSLTarget target,
                                          NkShaderDesc& outDesc,
                                          NkSLReflection& outReflection,
                                          const NkSLCompileOptions& opts = {}) const;

        uint32 HotReload(NkSLTarget target, const NkSLCompileOptions& opts = {});
        uint32 Count() const {
            NkSLMutexLock lock(mMutex);
            return (uint32)mEntries.Size();
        }

    private:
        NkSLCompiler*         mCompiler = nullptr;
        NkString              mBaseDir;
        NkVector<ShaderEntry> mEntries;
        mutable threading::NkMutex mMutex; // protège mEntries
    };

} // namespace nkentseu
