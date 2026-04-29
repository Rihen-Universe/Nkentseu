// =============================================================================
// NkShaderLibrary.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkShaderLibrary.h"
#include <cstdio>
#include <sys/stat.h>
#include <cstring>

namespace nkentseu {
namespace renderer {

    NkShaderLibrary::~NkShaderLibrary() { Shutdown(); }

    bool NkShaderLibrary::Init(NkIDevice* device, NkGraphicsApi api, bool useNkSL) {
        mDevice  = device;
        mBackend = NkCreateShaderBackend(api, useNkSL);
        return mBackend != nullptr;
    }

    void NkShaderLibrary::Shutdown() {
        ReleaseAll();
        delete mBackend; mBackend = nullptr;
    }

    // ── Lecture fichier ───────────────────────────────────────────────────────
    NkString NkShaderLibrary::ReadFile(const NkString& path) {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return "";
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        NkString s; s.Resize((uint32)sz);
        fread(s.Data(), 1, (size_t)sz, f);
        fclose(f);
        return s;
    }

    uint64 NkShaderLibrary::GetFileMtime(const NkString& path) {
#if defined(_WIN32)
        struct _stat64 st;
        if (_stat64(path.c_str(), &st) != 0) return 0;
        return (uint64)st.st_mtime;
#else
        struct stat st;
        if (stat(path.c_str(), &st) != 0) return 0;
        return (uint64)st.st_mtime;
#endif
    }

    // ── Compilation ───────────────────────────────────────────────────────────
    bool NkShaderLibrary::Recompile(NkShaderProgram& prog) {
        NkString vertSrc = prog.vertPath.Empty() ? "" : ReadFile(prog.vertPath);
        NkString fragSrc = prog.fragPath.Empty() ? "" : ReadFile(prog.fragPath);

        if (vertSrc.Empty() && fragSrc.Empty()) return false;

        NkShaderCompileOptions opts;
        opts.optimize = true;

        bool ok = true;
        if (!vertSrc.Empty()) {
            auto res = mBackend->Compile(vertSrc, NkShaderStage::NK_VERTEX, opts);
            if (!res.success) {
                fprintf(stderr, "[NkShader] VERT compile error (%s):\n%s\n",
                        prog.name.c_str(), res.errors.c_str());
                ok = false;
            } else {
                prog.vertBytecode = res.bytecode;
            }
        }
        if (!fragSrc.Empty()) {
            auto res = mBackend->Compile(fragSrc, NkShaderStage::NK_FRAGMENT, opts);
            if (!res.success) {
                fprintf(stderr, "[NkShader] FRAG compile error (%s):\n%s\n",
                        prog.name.c_str(), res.errors.c_str());
                ok = false;
            } else {
                prog.fragBytecode = res.bytecode;
            }
        }

        if (ok) {
            // Créer / recréer le programme RHI
            NkShaderProgramDesc spd;
            spd.name          = prog.name;
            spd.vertBytecode  = prog.vertBytecode.Data();
            spd.vertSize      = prog.vertBytecode.Size();
            spd.fragBytecode  = prog.fragBytecode.Data();
            spd.fragSize      = prog.fragBytecode.Size();
            // Si un programme RHI existait, le détruire d'abord
            if (prog.valid)
                mDevice->DestroyShaderProgram(prog.handle);

            NkIShaderProgram* rhi = mDevice->CreateShaderProgram(spd);
            prog.valid = (rhi != nullptr);
            if (rhi) {
                // Stocker le handle RHI dans le handle NkShader
                prog.handle.id = (uint64)(uintptr_t)rhi;
            }
        }
        return ok;
    }

    // ── Chargement ───────────────────────────────────────────────────────────
    NkShaderHandle NkShaderLibrary::LoadVF(const NkString& vPath,
                                             const NkString& fPath,
                                             const NkString& name) {
        NkShaderProgram prog;
        prog.name     = name.Empty() ? (vPath + "+" + fPath) : name;
        prog.vertPath = vPath;
        prog.fragPath = fPath;
        prog.vertMtime= GetFileMtime(vPath);
        prog.fragMtime= GetFileMtime(fPath);

        if (!Recompile(prog)) return NkShaderHandle::Null();
        return Alloc(prog);
    }

    NkShaderHandle NkShaderLibrary::LoadVGF(const NkString& v,
                                              const NkString& g,
                                              const NkString& f,
                                              const NkString& name) {
        NkShaderProgram prog;
        prog.name     = name.Empty() ? (v+"+"+g+"+"+f) : name;
        prog.vertPath = v;
        prog.geomPath = g;
        prog.fragPath = f;
        if (!Recompile(prog)) return NkShaderHandle::Null();
        return Alloc(prog);
    }

    NkShaderHandle NkShaderLibrary::LoadCompute(const NkString& cPath,
                                                  const NkString& name) {
        NkShaderProgram prog;
        prog.name     = name.Empty() ? cPath : name;
        prog.compPath = cPath;
        NkString src  = ReadFile(cPath);
        if (src.Empty()) return NkShaderHandle::Null();
        auto res = mBackend->Compile(src, NkShaderStage::NK_COMPUTE);
        if (!res.success) {
            fprintf(stderr, "[NkShader] COMPUTE error (%s):\n%s\n",
                    prog.name.c_str(), res.errors.c_str());
            return NkShaderHandle::Null();
        }
        prog.vertBytecode = res.bytecode;
        prog.valid = true;
        return Alloc(prog);
    }

    NkShaderHandle NkShaderLibrary::CompileVF(const NkString& vSrc,
                                               const NkString& fSrc,
                                               const NkString& name) {
        NkShaderProgram prog;
        prog.name = name.Empty() ? "inline_shader" : name;
        NkShaderCompileOptions opts; opts.optimize = false;

        auto vr = mBackend->Compile(vSrc, NkShaderStage::NK_VERTEX,   opts);
        auto fr = mBackend->Compile(fSrc, NkShaderStage::NK_FRAGMENT,  opts);
        if (!vr.success || !fr.success) {
            fprintf(stderr, "[NkShader] Inline compile error:\n  V:%s\n  F:%s\n",
                    vr.errors.c_str(), fr.errors.c_str());
            return NkShaderHandle::Null();
        }
        prog.vertBytecode = vr.bytecode;
        prog.fragBytecode = fr.bytecode;

        NkShaderProgramDesc spd;
        spd.name         = prog.name;
        spd.vertBytecode = prog.vertBytecode.Data();
        spd.vertSize     = prog.vertBytecode.Size();
        spd.fragBytecode = prog.fragBytecode.Data();
        spd.fragSize     = prog.fragBytecode.Size();
        NkIShaderProgram* rhi = mDevice->CreateShaderProgram(spd);
        prog.valid = (rhi != nullptr);
        if (rhi) prog.handle.id = (uint64)(uintptr_t)rhi;
        return Alloc(prog);
    }

    // ── Hot-reload ───────────────────────────────────────────────────────────
    void NkShaderLibrary::PollHotReload() {
        if (!mBackend->SupportsHotReload()) return;
        mPendingReload = false;
        for (auto& [id, prog] : mPrograms) {
            bool needsReload = false;
            if (!prog.vertPath.Empty()) {
                uint64 mt = GetFileMtime(prog.vertPath);
                if (mt != prog.vertMtime) { prog.vertMtime=mt; needsReload=true; }
            }
            if (!prog.fragPath.Empty()) {
                uint64 mt = GetFileMtime(prog.fragPath);
                if (mt != prog.fragMtime) { prog.fragMtime=mt; needsReload=true; }
            }
            if (needsReload) {
                printf("[NkShader] Hot-reloading '%s'...\n", prog.name.c_str());
                Recompile(prog);
                mPendingReload = true;
            }
        }
    }

    // ── Accès ────────────────────────────────────────────────────────────────
    NkShaderHandle NkShaderLibrary::Find(const NkString& name) const {
        auto* h = mByName.Find(name);
        return h ? *h : NkShaderHandle::Null();
    }

    const NkShaderProgram* NkShaderLibrary::Get(NkShaderHandle h) const {
        return mPrograms.Find(h.id);
    }

    NkIShaderProgram* NkShaderLibrary::GetRHI(NkShaderHandle h) const {
        auto* p = mPrograms.Find(h.id);
        if (!p || !p->valid) return nullptr;
        return (NkIShaderProgram*)(uintptr_t)p->handle.id;
    }

    NkShaderHandle NkShaderLibrary::Alloc(NkShaderProgram& prog) {
        NkShaderHandle h{mNextId++};
        prog.handle = h;
        mPrograms.Insert(h.id, prog);
        if (!prog.name.Empty()) mByName.Insert(prog.name, h);
        return h;
    }

    void NkShaderLibrary::Release(NkShaderHandle& h) {
        auto* p = mPrograms.Find(h.id);
        if (!p) return;
        if (p->valid) mDevice->DestroyShaderProgram(p->handle);
        if (!p->name.Empty()) mByName.Remove(p->name);
        mPrograms.Remove(h.id);
        h = NkShaderHandle::Null();
    }

    void NkShaderLibrary::ReleaseAll() {
        for (auto& [id, prog] : mPrograms)
            if (prog.valid) mDevice->DestroyShaderProgram(prog.handle);
        mPrograms.Clear();
        mByName.Clear();
    }

} // namespace renderer
} // namespace nkentseu
