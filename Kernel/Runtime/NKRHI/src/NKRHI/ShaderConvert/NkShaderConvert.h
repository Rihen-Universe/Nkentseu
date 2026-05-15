#pragma once
// =============================================================================
// NkShaderConvert.h  — Conversion et résolution de fichiers shaders.
//
// CONVENTION D'EXTENSIONS :
//   shader.vert.glsl   — vertex GLSL source
//   shader.frag.glsl   — fragment GLSL source
//   shader.comp.glsl   — compute GLSL source
//   shader.vert.spirv  — vertex SPIR-V binaire
//   shader.frag.spirv  — fragment SPIR-V binaire
//   shader.vert.hlsl   — vertex HLSL source
//   shader.frag.hlsl   — fragment HLSL source
//   shader.vert.msl    — vertex MSL source
//   shader.frag.msl    — fragment MSL source
//
// CAPACITÉS CONDITIONNELLES :
//   NK_RHI_GLSLANG_ENABLED    → GLSL → SPIRV (via glslang du Vulkan SDK)
//   NK_RHI_SPIRVCROSS_ENABLED → SPIRV → GLSL/HLSL/MSL (via SPIRV-Cross)
//
// La résolution de fichiers (NkShaderFileResolver) est toujours disponible.
// =============================================================================
#include "NKRHI/SL/NkSLTypes.h"

namespace nkentseu {

    // =============================================================================
    // NkShaderConvertResult
    // =============================================================================
    // struct NkShaderConvertResult {
    //     bool            success = false;
    //     NkString        source;       // Source texte (GLSL / HLSL / MSL)
    //     NkVector<uint8> binary;       // Binaire (SPIR-V : mots uint32 emballés en bytes)
    //     NkString        errors;

    //     // Accès au SPIR-V comme tableau de uint32
    //     const uint32* SpirvWords() const {
    //         return reinterpret_cast<const uint32*>(binary.Data());
    //     }
    //     uint32 SpirvWordCount() const {
    //         return (uint32)(binary.Size() / sizeof(uint32));
    //     }
    // };
    struct NkShaderConvertResult {
        bool            success = false;
        NkString        source;       // Source texte (GLSL / HLSL / MSL)
        NkVector<uint8> binary;       // Binaire (SPIR-V : mots uint32 emballés en bytes)
        NkString        errors;

        // Accès au SPIR-V comme tableau de uint32 avec vérification d'alignement
        const uint32* SpirvWords() const {
            // Vérifier que les données sont alignées sur 4 octets
            if (binary.IsEmpty()) return nullptr;
            // Pour Windows/x64, uint8* est aligné sur 1, mais le compilateur 
            // peut aligner le buffer sur 8 octets. C'est généralement OK.
            return reinterpret_cast<const uint32*>(binary.Data());
        }
        
        uint32 SpirvWordCount() const {
            return (uint32)(binary.Size() / sizeof(uint32));
        }
        
        // NOUVEAU : retourner une copie alignée si nécessaire
        NkVector<uint32> GetSpirvWordsCopy() const {
            NkVector<uint32> words;
            if (binary.IsEmpty()) return words;
            words.Resize(SpirvWordCount());
            memcpy(words.Data(), binary.Data(), binary.Size());
            return words;
        }
    };

    // =============================================================================
    // NkShaderFileResolver
    //
    // Résolution par convention de nommage :
    //   BasePath    ("a/b/shader.vert.glsl") → "a/b/shader.vert"
    //   FormatExt   ("a/b/shader.vert.glsl") → "glsl"
    //   StageExt    ("a/b/shader.vert.glsl") → "vert"
    //   StageFrom   ("a/b/shader.vert.glsl") → NkSLStage::NK_VERTEX
    //   ResolveVariant("shader.vert.glsl", "spirv") → "shader.vert.spirv"
    //   FindVariants("shader.vert.glsl")   → liste de variantes existant sur disque
    // =============================================================================
    class NkShaderFileResolver {
        public:
            // Retire la dernière extension de format
            // "path/shader.vert.glsl" → "path/shader.vert"
            static NkString BasePath(const NkString& path);

            // Extension de format (dernière extension)
            // "shader.vert.glsl" → "glsl"
            static NkString FormatExt(const NkString& path);

            // Extension de stage (avant-dernière extension)
            // "shader.vert.glsl" → "vert"
            static NkString StageExt(const NkString& path);

            // Déduit le stage depuis l'extension de chemin
            // "shader.vert.glsl" → NK_VERTEX
            static NkSLStage StageFrom(const NkString& path);

            // Remplace l'extension de format tout en gardant le stage
            // ("shader.vert.glsl", "spirv") → "shader.vert.spirv"
            static NkString ResolveVariant(const NkString& path, const NkString& targetFmtExt);

            // Retourne toutes les variantes existant sur disque
            // Vérifie : .glsl, .spirv, .spv, .hlsl, .msl
            static NkVector<NkString> FindVariants(const NkString& path);

            // Teste si un fichier existe
            static bool FileExists(const NkString& path);
    };

    // =============================================================================
    // NkShaderConverter
    // =============================================================================
    class NkShaderConverter {
        public:
            // ── Capacités (compilées selon les backends) ──────────────────────────────
            static bool CanGlslToSpirv();   // NK_RHI_GLSLANG_ENABLED
            static bool CanSpirvToGlsl();   // NK_RHI_SPIRVCROSS_ENABLED
            static bool CanSpirvToHlsl();   // NK_RHI_SPIRVCROSS_ENABLED
            static bool CanSpirvToMsl();    // NK_RHI_SPIRVCROSS_ENABLED

            // ── GLSL source → SPIR-V binary ───────────────────────────────────────────
            // Requiert NK_RHI_GLSLANG_ENABLED.
            // glslSource : source GLSL 4.30+ sans annotation NkSL
            // stage      : stage cible (vertex, fragment, compute…)
            // debugName  : nom pour les messages d'erreur
            static NkShaderConvertResult GlslToSpirv(
                const NkString& glslSource,
                NkSLStage       stage,
                const NkString& debugName = "shader");

            // ── SPIR-V → texte ────────────────────────────────────────────────────────
            // Requiert NK_RHI_SPIRVCROSS_ENABLED.
            static NkShaderConvertResult SpirvToGlsl(
                const uint32* spirvWords, uint32 wordCount, NkSLStage stage);
            static NkShaderConvertResult SpirvToHlsl(
                const uint32* spirvWords, uint32 wordCount, NkSLStage stage,
                uint32 hlslShaderModel = 50);
            static NkShaderConvertResult SpirvToMsl(
                const uint32* spirvWords, uint32 wordCount, NkSLStage stage);

            // ── Helpers SPIR-V via NkShaderConvertResult ──────────────────────────────
            static NkShaderConvertResult SpirvToGlsl(const NkShaderConvertResult& spirv, NkSLStage s) {
                return SpirvToGlsl(spirv.SpirvWords(), spirv.SpirvWordCount(), s);
            }
            static NkShaderConvertResult SpirvToHlsl(const NkShaderConvertResult& spirv, NkSLStage s,
                                                    uint32 sm = 50) {
                return SpirvToHlsl(spirv.SpirvWords(), spirv.SpirvWordCount(), s, sm);
            }
            static NkShaderConvertResult SpirvToMsl(const NkShaderConvertResult& spirv, NkSLStage s) {
                return SpirvToMsl(spirv.SpirvWords(), spirv.SpirvWordCount(), s);
            }

            // ── Chargement de fichier ─────────────────────────────────────────────────
            // Charge un fichier shader et auto-détecte le format via l'extension.
            // .spirv / .spv → binary (uint32 mots en bytes)
            // .glsl / .hlsl / .msl  → source (texte)
            static NkShaderConvertResult LoadFile(const NkString& path);

            // Charge et retourne du SPIR-V :
            //   .spirv / .spv → charge directement
            //   .glsl         → compile via GlslToSpirv si disponible
            //   .hlsl / .msl  → erreur (pas de compilation vers SPIR-V dans ce module)
            static NkShaderConvertResult LoadAsSpirv(const NkString& path);

            // ── Raccourci : fichier GLSL → spirv → cible ─────────────────────────────
            // Equivalent de : LoadFile(glslPath) | GlslToSpirv | SpirvToHlsl/Glsl/Msl
            static NkShaderConvertResult GlslFileToHlsl(const NkString& glslPath, uint32 sm = 50);
            static NkShaderConvertResult GlslFileToMsl (const NkString& glslPath);
            static NkShaderConvertResult GlslFileToGlsl(const NkString& glslPath);

            // ── Conversion GLSL Vulkan-style → cible (chaine GlslToSpirv + SpirvToXxx) ─
            // Source canonique = GLSL Vulkan (avec layout(set=,binding=), push_constant,
            // etc.). Sortie selon le backend cible. Necessite NK_RHI_GLSLANG_ENABLED +
            // NK_RHI_SPIRVCROSS_ENABLED.
            //   GlslToHlsl : pour DX11/DX12 (HLSL SM5/SM6)
            //   GlslToMsl  : pour Metal
            //   GlslToGlsl : pour OpenGL classique (transpilation cross-version)
            // Le Reflector/Pipeline-state du backend cible appellera ces helpers a la
            // demande via NkShaderCache (cache binaire pour eviter les recompilations).
            static NkShaderConvertResult GlslToHlsl(const NkString& glslSource, NkSLStage stage,
                                                    uint32 hlslShaderModel = 50,
                                                    const NkString& debugName = "shader");
            static NkShaderConvertResult GlslToMsl (const NkString& glslSource, NkSLStage stage,
                                                    const NkString& debugName = "shader");
            static NkShaderConvertResult GlslToGlsl(const NkString& glslSource, NkSLStage stage,
                                                    const NkString& debugName = "shader");
    };

    // =============================================================================
    // NkShaderCache
    //
    // Cache binaire sur disque pour les shaders compilés.
    // Clé = FNV-1a 64-bit sur (source + stage + format-cible).
    //
    // Format de fichier cache (.nksc) :
    //   [4B magic='NKSC'] [8B key] [4B size] [size bytes data]
    //
    // Usage :
    //   NkShaderCache::Global().SetCacheDir("Build/ShaderCache");
    //   auto res = NkShaderCache::Global().Load(key);
    //   if (!res.success) {
    //       res = NkShaderConverter::GlslToSpirv(src, stage);
    //       NkShaderCache::Global().Save(key, res);
    //   }
    // =============================================================================
    class NkShaderCache {
        public:
            // Répertoire de stockage (créé automatiquement si absent)
            void SetCacheDir(const NkString& dir) noexcept;
            const NkString& CacheDir() const noexcept { return mCacheDir; }

            // Calcul de clé FNV-1a 64-bit
            static uint64 ComputeKey(const NkString& source,
                                     NkSLStage       stage,
                                     const NkString& targetFormat = "spirv") noexcept;

            // Charge depuis le cache. result.success == false si absent.
            NkShaderConvertResult Load(uint64 key) const noexcept;

            // Sauvegarde dans le cache (remplace si déjà présent).
            bool Save(uint64 key, const NkShaderConvertResult& result) noexcept;

            // Invalide une entrée.
            void Invalidate(uint64 key) noexcept;

            // Vide tout le cache (supprime les fichiers .nksc).
            void Clear() noexcept;

            // ── Garbage collection ────────────────────────────────────────────
            // Les fichiers .nksc accumulent dans le cache au fil des sessions :
            // un changement de source produit un nouveau hash -> nouveau .nksc,
            // mais l'ancien reste sur disque indefiniment. Les helpers ci-dessous
            // permettent une purge selective.

            // Supprime tous les .nksc dont la cle n'est PAS dans livingKeys.
            // Retourne le nombre de fichiers supprimes.
            uint32 PurgeUnused(const NkVector<uint64>& livingKeys) noexcept;

            // Supprime les .nksc non touches (ni Load ni Save) durant cette
            // session. Appel typique au Shutdown : NkShaderCache::Global()
            // .PurgeUnusedThisSession(). Tres aggressif : NE PAS l'appeler en
            // mode dev partiel (modifier 1 shader vide le cache des autres).
            uint32 PurgeUnusedThisSession() noexcept;

            // Supprime les .nksc dont mtime est plus vieux que maxAgeSeconds.
            // Plus conservateur que PurgeUnusedThisSession. Pratique en CI :
            // PurgeOlderThan(30*24*3600) au startup -> garbe 30 jours.
            uint32 PurgeOlderThan(uint64 maxAgeSeconds) noexcept;

            // Singleton global (optionnel).
            static NkShaderCache& Global() noexcept;

        private:
            NkString mCacheDir;
            NkString KeyToPath(uint64 key) const noexcept;

            // Tracking des keys touchees (Load ou Save) durant la session.
            // Utilise par PurgeUnusedThisSession.
            mutable NkVector<uint64> mTouchedKeys;
            mutable bool             mTouchedSorted = false;

            void   MarkTouched(uint64 key) const noexcept;
            bool   IsTouched(uint64 key)  const noexcept;
            uint32 PurgeImpl(const NkVector<uint64>& keepKeys,
                             bool ageCheck, uint64 maxAgeSeconds) noexcept;
    };

} // namespace nkentseu