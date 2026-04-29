#pragma once
// =============================================================================
// NkShaderLibrary.h  — NKRenderer v4.0  (Shader/)
// Cache de shaders compilés, hot-reload en développement.
// =============================================================================
#include "NkShaderBackend.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
namespace renderer {

    struct NkShaderProgram {
        NkShaderHandle  handle;
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

        // ── Chargement depuis fichier ────────────────────────────────────────
        NkShaderHandle LoadVF(const NkString& vertPath, const NkString& fragPath,
                               const NkString& name = "");
        NkShaderHandle LoadVGF(const NkString& vert, const NkString& geom,
                                const NkString& frag, const NkString& name = "");
        NkShaderHandle LoadCompute(const NkString& compPath, const NkString& name = "");

        // ── Chargement depuis source ─────────────────────────────────────────
        NkShaderHandle CompileVF(const NkString& vertSrc, const NkString& fragSrc,
                                  const NkString& name = "");

        // ── Hot-reload (polling en développement) ────────────────────────────
        void PollHotReload();     // appeler chaque frame en mode debug
        bool HasPendingReloads() const { return mPendingReload; }

        // ── Accès ────────────────────────────────────────────────────────────
        NkShaderHandle Find(const NkString& name) const;
        const NkShaderProgram* Get(NkShaderHandle h) const;
        NkIShaderProgram* GetRHI(NkShaderHandle h) const;

        // ── Libération ───────────────────────────────────────────────────────
        void Release(NkShaderHandle& h);
        void ReleaseAll();

    private:
        NkIDevice*                          mDevice    = nullptr;
        NkShaderBackend*                    mBackend   = nullptr;
        NkHashMap<uint64, NkShaderProgram>  mPrograms;
        NkHashMap<NkString, NkShaderHandle> mByName;
        NkHashMap<NkString, uint64>         mFileMtime;
        uint64                              mNextId    = 1;
        bool                                mPendingReload = false;

        NkShaderHandle Alloc(NkShaderProgram& prog);
        bool           Recompile(NkShaderProgram& prog);
        uint64         GetFileMtime(const NkString& path);
        NkString       ReadFile(const NkString& path);
    };

} // namespace renderer
} // namespace nkentseu
