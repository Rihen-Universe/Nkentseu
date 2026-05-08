#pragma once
// =============================================================================
// NkShaderLibrary.h  — NKRenderer v4.0  (Shader/)
// Cache de shaders compilés, hot-reload en développement.
// =============================================================================
#include "NkShaderBackend.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
    namespace renderer {

        struct NkShaderProgram {
            // ATTENTION : `NkShaderHandle` ici est resolu selon l'ordre d'include
            // dans le .cpp consommateur (peut etre soit le RHI, soit le renderer-side
            // wrapper). Pour eviter ce piege, on force explicitement les types
            // qualifies (::nkentseu::NkShaderHandle = RHI) :
            ::nkentseu::NkShaderHandle handle;        // Renderer-side ID (cache lookup, hot-reload key)
            ::nkentseu::NkShaderHandle rhiHandle;     // RHI shader handle (a passer aux pipelines)
            NkString        name;
            NkString        vertPath, fragPath, geomPath, compPath;
            NkVector<uint8> vertBytecode, fragBytecode;
            uint64          vertMtime = 0, fragMtime = 0;
            bool            valid     = false;
        };

        class NkShaderLibrary {
            public:
                NkShaderLibrary() = default;
                ~NkShaderLibrary();

                bool Init(NkIDevice* device, NkGraphicsApi api, bool useNkSL = false);
                void Shutdown();

                // NB : tous les handles renvoyes/recus sont les handles RHI
                // (::nkentseu::NkShaderHandle). On les passe directement a
                // NkGraphicsPipelineDesc::shader.

                // ── Chargement depuis fichier ────────────────────────────────────────
                ::nkentseu::NkShaderHandle LoadVF(const NkString& vertPath, const NkString& fragPath, const NkString& name = "");
                ::nkentseu::NkShaderHandle LoadVGF(const NkString& vert, const NkString& geom, const NkString& frag, const NkString& name = "");
                ::nkentseu::NkShaderHandle LoadCompute(const NkString& compPath, const NkString& name = "");

                // ── Chargement depuis source ─────────────────────────────────────────
                ::nkentseu::NkShaderHandle CompileVF(const NkString& vertSrc, const NkString& fragSrc, const NkString& name = "");

                // ── Hot-reload (polling en développement) ────────────────────────────
                void PollHotReload();
                bool HasPendingReloads() const { return mPendingReload; }

                // ── Accès ────────────────────────────────────────────────────────────
                ::nkentseu::NkShaderHandle Find(const NkString& name) const;
                const NkShaderProgram* Get(::nkentseu::NkShaderHandle h) const;

                // Retourne le handle RHI shader stocke par Recompile/CompileVF.
                // C'est ce handle qu'on passe aux NkGraphicsPipelineDesc::shader.
                // Si le program est invalide, renvoie un handle null.
                ::nkentseu::NkShaderHandle GetRHIHandle(::nkentseu::NkShaderHandle h) const;

                // ── Libération ───────────────────────────────────────────────────────
                void Release(::nkentseu::NkShaderHandle& h);
                void ReleaseAll();

            private:
                NkIDevice*                          mDevice    = nullptr;
                NkShaderBackend*                    mBackend   = nullptr;
                NkHashMap<uint64, NkShaderProgram>              mPrograms;
                NkHashMap<NkString, ::nkentseu::NkShaderHandle> mByName;
                NkHashMap<NkString, uint64>                     mFileMtime;
                uint64                              mNextId    = 1;
                bool                                mPendingReload = false;

                ::nkentseu::NkShaderHandle Alloc(NkShaderProgram& prog);
                bool                       Recompile(NkShaderProgram& prog);
                uint64         GetFileMtime(const NkString& path);
                NkString       ReadFile(const NkString& path);
        };

    } // namespace renderer
} // namespace nkentseu
