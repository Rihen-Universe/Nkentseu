// =============================================================================
// NkShaderLibrary.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkShaderLibrary.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <sys/stat.h>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        NkShaderLibrary::~NkShaderLibrary() { Shutdown(); }

        bool NkShaderLibrary::Init(NkIDevice* device, NkGraphicsApi api, bool useNkSL) {
            mDevice  = device;
            mApi     = api;
            mBackend = NkCreateShaderBackend(api, useNkSL);
            return mBackend != nullptr;
        }

        void NkShaderLibrary::Shutdown() {
            ReleaseAll();
            delete mBackend; mBackend = nullptr;
        }

        // ── Lecture fichier ───────────────────────────────────────────────────────
        NkString NkShaderLibrary::ReadFile(const NkString& path) {
            FILE* f = fopen(path.CStr(), "rb");
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
            if (_stat64(path.CStr(), &st) != 0) return 0;
            return (uint64)st.st_mtime;
    #else
            struct stat st;
            if (stat(path.CStr(), &st) != 0) return 0;
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
                            prog.name.CStr(), res.errors.CStr());
                    ok = false;
                } else {
                    prog.vertBytecode = res.bytecode;
                }
            }
            if (!fragSrc.Empty()) {
                auto res = mBackend->Compile(fragSrc, NkShaderStage::NK_FRAGMENT, opts);
                if (!res.success) {
                    fprintf(stderr, "[NkShader] FRAG compile error (%s):\n%s\n",
                            prog.name.CStr(), res.errors.CStr());
                    ok = false;
                } else {
                    prog.fragBytecode = res.bytecode;
                }
            }

            if (ok) {
                // Detruire l'ancien programme RHI s'il existait (hot-reload)
                if (prog.valid && prog.rhiHandle.IsValid())
                    mDevice->DestroyShader(prog.rhiHandle);

                // Cf CompileVF : une SEULE entree par stage, GLSL + SPIRV combines.
                NkShaderDesc desc;
                if (!vertSrc.Empty()) {
                    ::nkentseu::NkShaderStageDesc vs{};
                    vs.stage      = ::nkentseu::NkShaderStage::NK_VERTEX;
                    vs.glslSource = vertSrc.CStr();
                    vs.entryPoint = "main";
                    if (!prog.vertBytecode.Empty()) {
                        vs.spirvBinary.Resize((uint32)prog.vertBytecode.Size());
                        memcpy(vs.spirvBinary.Data(),
                               prog.vertBytecode.Data(),
                               (size_t)prog.vertBytecode.Size());
                    }
                    desc.AddStage(vs);
                }
                if (!fragSrc.Empty()) {
                    ::nkentseu::NkShaderStageDesc fs{};
                    fs.stage      = ::nkentseu::NkShaderStage::NK_FRAGMENT;
                    fs.glslSource = fragSrc.CStr();
                    fs.entryPoint = "main";
                    if (!prog.fragBytecode.Empty()) {
                        fs.spirvBinary.Resize((uint32)prog.fragBytecode.Size());
                        memcpy(fs.spirvBinary.Data(),
                               prog.fragBytecode.Data(),
                               (size_t)prog.fragBytecode.Size());
                    }
                    desc.AddStage(fs);
                }
                desc.debugName = prog.name.CStr();

                NkShaderHandle rhi = mDevice->CreateShader(desc);
                prog.valid     = rhi.IsValid();
                prog.rhiHandle = rhi;
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
                        prog.name.CStr(), res.errors.CStr());
                return NkShaderHandle::Null();
            }
            prog.vertBytecode = res.bytecode;
            prog.valid = true;
            return Alloc(prog);
        }

        NkShaderHandle NkShaderLibrary::CompileVF(const NkString& vSrc, const NkString& fSrc, const NkString& name) {
            NkShaderProgram prog;
            prog.name = name.Empty() ? "inline_shader" : name;
            NkShaderCompileOptions opts; opts.optimize = false;

            // glslang -> SPIR-V (utile pour Vulkan, optionnel pour OpenGL).
            auto vr = mBackend->Compile(vSrc, NkShaderStage::NK_VERTEX,   opts);
            auto fr = mBackend->Compile(fSrc, NkShaderStage::NK_FRAGMENT,  opts);
            // On ne fail PAS si glslang echoue : OpenGL peut compiler le GLSL directement.
            if (vr.success) prog.vertBytecode = vr.bytecode;
            if (fr.success) prog.fragBytecode = fr.bytecode;

            // Une SEULE entree NkShaderStageDesc par stage : sinon le backend
            // Vulkan recoit 2 stages NK_VERTEX (un pour GLSL, un pour SPIRV) et
            // produit 2 VkShaderModule -> validation error VUID-...stage-06897.
            // On porte les deux infos (GLSL pour OpenGL, SPIRV pour Vulkan/DX12)
            // dans la meme entree, le device choisira ce qui lui convient.
            NkShaderDesc desc;
            {
                ::nkentseu::NkShaderStageDesc vs{};
                vs.stage      = ::nkentseu::NkShaderStage::NK_VERTEX;
                vs.glslSource = vSrc.CStr();
                vs.entryPoint = "main";
                if (!prog.vertBytecode.Empty()) {
                    vs.spirvBinary.Resize((uint32)prog.vertBytecode.Size());
                    memcpy(vs.spirvBinary.Data(),
                           prog.vertBytecode.Data(),
                           (size_t)prog.vertBytecode.Size());
                }
                desc.AddStage(vs);

                ::nkentseu::NkShaderStageDesc fs{};
                fs.stage      = ::nkentseu::NkShaderStage::NK_FRAGMENT;
                fs.glslSource = fSrc.CStr();
                fs.entryPoint = "main";
                if (!prog.fragBytecode.Empty()) {
                    fs.spirvBinary.Resize((uint32)prog.fragBytecode.Size());
                    memcpy(fs.spirvBinary.Data(),
                           prog.fragBytecode.Data(),
                           (size_t)prog.fragBytecode.Size());
                }
                desc.AddStage(fs);
            }
            desc.debugName = prog.name.CStr();

            NkShaderHandle rhi = mDevice->CreateShader(desc);
            prog.valid     = rhi.IsValid();
            prog.rhiHandle = rhi;
            if (!prog.valid) {
                fprintf(stderr, "[NkShader] CreateShader fail '%s' (glslang : V:%d F:%d)\n",
                        prog.name.CStr(), (int)vr.success, (int)fr.success);
            }
            return Alloc(prog);
        }

        // ── User-override / fallback embedded ─────────────────────────────────────
        // Convention : Resources/NKRenderer/Shaders/<MaterialName>/<Backend>/
        //                                   <materialname>.<stage>.<backend>.<ext>
        // (cf Resources/NKRenderer/README.md). Si l'un des deux fichiers est present,
        // on l'utilise au lieu de la source embedded -> permet a l'utilisateur de
        // surcharger un shader stock sans recompiler le moteur.
        // Le path est relatif au CWD (= dossier du binaire ou de l'app launchee).
        NkShaderHandle NkShaderLibrary::LoadOrCompileVF(const NkString& materialName,
                                                         const NkString& fallbackVS,
                                                         const NkString& fallbackFS) {
            logger.Info("[NkShaderLibrary] LoadOrCompileVF '{0}' (api={1})\n",
                        materialName, (int)mApi);
            // Backend + extension selectionne d'apres l'API courante. Convention :
            //   Resources/NKRenderer/Shaders/<MaterialName>/<BackendDir>/
            //                              <materialname>.<stage>.<ext>
            // Chaque backend a son langage natif :
            //   GL    -> GLSL OpenGL natif (uniforme blocks, pas de set=)
            //   VK    -> GLSL Vulkan (set=N, layout(push_constant), etc.)
            //   DX11  -> HLSL SM5 (cbuffer, register(b/t/s), SV_*)
            //   DX12  -> HLSL SM6 (idem + bindless support)
            //   MSL   -> Metal Shading Language (struct + [[buffer(N)]])
            //   NkSL  -> NkSL (langage interne, transpile vers tous les autres)
            const char* backendDir = "GL";
            const char* extVS      = "vert.gl.glsl";
            const char* extFS      = "frag.gl.glsl";
            switch (mApi) {
                case NkGraphicsApi::NK_GFX_API_VULKAN:
                    backendDir = "VK";
                    extVS = "vert.vk.glsl"; extFS = "frag.vk.glsl"; break;
                case NkGraphicsApi::NK_GFX_API_DX11:
                    backendDir = "DX11";
                    extVS = "vert.dx11.hlsl"; extFS = "frag.dx11.hlsl"; break;
                case NkGraphicsApi::NK_GFX_API_DX12:
                    backendDir = "DX12";
                    extVS = "vert.dx12.hlsl"; extFS = "frag.dx12.hlsl"; break;
                case NkGraphicsApi::NK_GFX_API_METAL:
                    backendDir = "MSL";
                    extVS = "vert.metal.msl"; extFS = "frag.metal.msl"; break;
                default: break;   // OpenGL / OpenGLES / Software gardent la convention GL
            }

            // Lower-case le material name pour l'extension fichier (convention
            // POSIX-friendly : pbr.vert.gl.glsl, pas PBR.vert.gl.glsl).
            NkString matLower = materialName;
            for (uint32 i = 0; i < (uint32)matLower.Size(); i++) {
                char c = matLower[i];
                if (c >= 'A' && c <= 'Z') matLower[i] = (char)(c + 32);
            }

            NkString basePath = "Resources/NKRenderer/Shaders/";
            basePath += materialName;
            basePath += "/";
            basePath += backendDir;
            basePath += "/";

            NkString vsPath = basePath + matLower + "." + extVS;
            NkString fsPath = basePath + matLower + "." + extFS;

            NkString vSrc = fallbackVS;
            NkString fSrc = fallbackFS;
            bool overrideVS = NkFile::Exists(vsPath.CStr());
            bool overrideFS = NkFile::Exists(fsPath.CStr());
            if (overrideVS) vSrc = NkFile::ReadAllText(vsPath.CStr());
            if (overrideFS) fSrc = NkFile::ReadAllText(fsPath.CStr());

            if (overrideVS || overrideFS) {
                logger.Info("[NkShader] LoadOrCompileVF '{0}' override : VS={1} FS={2}\n",
                            materialName, overrideVS ? "file" : "embedded",
                            overrideFS ? "file" : "embedded");
            }

            NkShaderHandle h = CompileVF(vSrc, fSrc, materialName);

            // Hot-reload n'est active que si VS ET FS sont overrides en meme temps.
            // (Recompile() lit chaque stage depuis son path : si une seule des deux
            // sources est sur disque, on ne peut pas reconstruire le programme.)
            if (h.IsValid() && overrideVS && overrideFS) {
                NkShaderProgram* prog = mPrograms.Find(h.id);
                if (prog) {
                    prog->vertPath = vsPath;
                    prog->vertMtime= GetFileMtime(vsPath);
                    prog->fragPath = fsPath;
                    prog->fragMtime= GetFileMtime(fsPath);
                }
            }
            return h;
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
                    printf("[NkShader] Hot-reloading '%s'...\n", prog.name.CStr());
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

        NkShaderHandle NkShaderLibrary::GetRHIHandle(NkShaderHandle h) const {
            auto* p = mPrograms.Find(h.id);
            if (!p || !p->valid) return NkShaderHandle::Null();
            return p->rhiHandle;          // ← retourne le vrai RHI handle
        }

        NkShaderHandle NkShaderLibrary::Alloc(NkShaderProgram& prog) {
            NkShaderHandle h{mNextId++};
            prog.handle = h;              // ← seulement le renderer-side ID, NE PLUS toucher rhiHandle
            mPrograms.Insert(h.id, prog);
            if (!prog.name.Empty()) mByName.Insert(prog.name, h);
            return h;
        }

        void NkShaderLibrary::Release(NkShaderHandle& h) {
            auto* p = mPrograms.Find(h.id);
            if (!p) return;
            if (p->valid && p->rhiHandle.IsValid()) mDevice->DestroyShader(p->rhiHandle);
            if (!p->name.Empty()) mByName.Remove(p->name);
            mPrograms.Remove(h.id);
            h = NkShaderHandle::Null();
        }

        void NkShaderLibrary::ReleaseAll() {
            for (auto& [id, prog] : mPrograms)
                if (prog.valid && prog.rhiHandle.IsValid())
                    mDevice->DestroyShader(prog.rhiHandle);
            mPrograms.Clear();
            mByName.Clear();
        }

    } // namespace renderer
} // namespace nkentseu
