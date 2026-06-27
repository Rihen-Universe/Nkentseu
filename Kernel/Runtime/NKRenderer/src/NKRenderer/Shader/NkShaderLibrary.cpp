// =============================================================================
// NkShaderLibrary.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkShaderLibrary.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKLogger/NkLog.h"
#include "NKSL/NKSL.h"
#include "NkShaderIncludeResolver.h"
#include "NKMemory/NkAllocator.h"
#include <cstdio>
#include <sys/stat.h>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        NkShaderLibrary::~NkShaderLibrary() { Shutdown(); }

        // ─────────────────────────────────────────────────────────────────────────
        // Test au demarrage : valider que NkShaderConverter peut convertir un
        // VRAI shader GLSL Vulkan (le PBR FS complexe = ~16KB avec samplerCube,
        // sampler2DShadow, push_constant, uniform blocks, arrays) vers tous les
        // backends. Ecrit les sorties dans Build/cross_api_output/ pour
        // inspection manuelle (verifier que la conversion produit du code
        // coherent dans chaque langage).
        // ─────────────────────────────────────────────────────────────────────────
        static bool ReadFileToString(const char* path, NkString& out) {
            FILE* f = fopen(path, "rb");
            if (!f) return false;
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            out.Resize((uint32)sz);
            fread(out.Data(), 1, (size_t)sz, f);
            fclose(f);
            return true;
        }

        static void WriteStringToFile(const char* path, const NkString& s) {
            FILE* f = fopen(path, "wb");
            if (!f) {
                logger.Errorf("[CrossAPITest] Impossible d'ecrire dans %s\n", path);
                return;
            }
            fwrite(s.CStr(), 1, (size_t)s.Size(), f);
            fclose(f);
        }

        static void TestCrossApiConversion() {
            // Tente de charger un VRAI shader pour stress-test. Si fichier introuvable,
            // fallback sur un shader trivial inline.
            const char* kPbrPath = "Resources/NKRenderer/Shaders/PBR/VK/pbr.frag.vk.glsl";
            NkString src;
            NkString name;
            if (ReadFileToString(kPbrPath, src) && !src.Empty()) {
                name = "pbr.frag";
                logger.Info("[CrossAPITest] Chargement {0} ({1} bytes)\n",
                            NkString(kPbrPath), (uint32)src.Size());
            } else {
                logger.Info("[CrossAPITest] Fallback shader trivial (pbr.frag.vk.glsl introuvable)\n");
                src = NkString(
                    "#version 460 core\n"
                    "layout(location=0) in  vec2 vUV;\n"
                    "layout(location=0) out vec4 oColor;\n"
                    "layout(set=0, binding=0) uniform sampler2D uTex;\n"
                    "layout(set=0, binding=1, std140) uniform Block {\n"
                    "    vec4  tint;\n"
                    "    float exposure;\n"
                    "} u;\n"
                    "layout(push_constant) uniform PC { vec2 offset; } pc;\n"
                    "void main() {\n"
                    "    vec4 c = texture(uTex, vUV + pc.offset) * u.tint;\n"
                    "    oColor = vec4(c.rgb * u.exposure, c.a);\n"
                    "}\n"
                );
                name = "test_fs";
            }
            const NkSLStage st = NkSLStage::NK_FRAGMENT;
            const char* kOutDir = "Build/cross_api_output";

            auto reportAndSave = [&](const char* target, const char* outName,
                                     const NkShaderConvertResult& r) {
                if (r.success) {
                    logger.Info("[CrossAPITest] {0}: OK ({1} chars) -> {2}\n",
                                NkString(target), (uint32)r.source.Size(),
                                NkString(outName));
                    NkString outPath(kOutDir);
                    outPath += "/";
                    outPath += outName;
                    WriteStringToFile(outPath.CStr(), r.source);
                } else {
                    logger.Errorf("[CrossAPITest] %s: FAIL -- %s\n",
                                  target, r.errors.CStr());
                }
            };

            logger.Info("[CrossAPITest] === Test conversion {0} -> {{GL, HLSL SM5, HLSL SM6, MSL}} ===\n", name);

            // Cree le dossier de sortie (mkdir-like). Win32 + POSIX gerees via NK_MKDIR
            // inferable, mais ici on tente direct l'ecriture (fopen wb echouera si
            // le dossier n'existe pas).
        #ifdef _WIN32
            system("if not exist \"Build\\cross_api_output\" mkdir \"Build\\cross_api_output\"");
        #else
            system("mkdir -p Build/cross_api_output");
        #endif

            reportAndSave("VK->GL",       (name + ".gl.glsl"     ).CStr(),
                          NkShaderConverter::GlslToGlsl(src, st, name));
            reportAndSave("VK->HLSL SM5", (name + ".sm5.hlsl"    ).CStr(),
                          NkShaderConverter::GlslToHlsl(src, st, 50, name));
            reportAndSave("VK->HLSL SM6", (name + ".sm6.hlsl"    ).CStr(),
                          NkShaderConverter::GlslToHlsl(src, st, 60, name));
            reportAndSave("VK->MSL",      (name + ".msl"         ).CStr(),
                          NkShaderConverter::GlslToMsl (src, st, name));
            logger.Info("[CrossAPITest] === End -> outputs dans {0}/ ===\n", NkString(kOutDir));
        }

        bool NkShaderLibrary::Init(NkIDevice* device, NkGraphicsApi api, bool useNkSL) {
            mDevice  = device;
            mApi     = api;
            mBackend = NkCreateShaderBackend(api, useNkSL);
            // Backend NkSL secondaire (vrai NkSLCompiler) pour l'opt-in par-shader :
            // LoadOrCompileVF l'utilise quand un .nksl (vrai dialecte) existe.
            mNkSLBackend = NkCreateShaderBackend(api, /*useNkSL=*/true);

            // Cache shader sur disque : cache/shaders/<hash16>.nksc
            // Cle = FNV-1a(source + stage + format). Invalide automatiquement
            // si le contenu du fichier source change.
            NkPath shaderCacheDir = NkPath::GetExecutableDirectory() / "cache" / "shaders";
            NkShaderCache::Global().SetCacheDir(shaderCacheDir.ToString());

            // Test cross-API: desactive par defaut (coute ~8s au demarrage en debug).
            // Definir NK_ENABLE_CROSS_API_TEST pour activer (dev seulement).
#ifdef NK_ENABLE_CROSS_API_TEST
            static bool sTestDone = false;
            if (!sTestDone) { TestCrossApiConversion(); sTestDone = true; }
#endif
            return mBackend != nullptr;
        }

        void NkShaderLibrary::Shutdown() {
            ReleaseAll();
            memory::NkGetDefaultAllocator().Delete(mBackend); mBackend = nullptr;
            memory::NkGetDefaultAllocator().Delete(mNkSLBackend); mNkSLBackend = nullptr;
        }

        // ── Lecture fichier ───────────────────────────────────────────────────────
        // Strip automatiquement les annotations semantiques @xxx au chargement,
        // pour que tous les consommateurs (backend GL/VK/DX/MSL, glslang,
        // SPIRV-Cross) recoivent du GLSL Vulkan natif sans aucun marker `@xxx`
        // (qui ferait planter le compilateur). La metadata extraite n'est pas
        // conservee ici (sera re-extraite par l'UI editeur de materiaux a la
        // demande, sur demande explicite via NkShaderAnnotationParser::Parse).
        NkString NkShaderLibrary::ReadFile(const NkString& path) {
            FILE* f = fopen(path.CStr(), "rb");
            if (!f) return "";
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            NkString raw; raw.Resize((uint32)sz);
            fread(raw.Data(), 1, (size_t)sz, f);
            fclose(f);
            // M.5 Material Functions : resoud les #include "Include/foo.glsli"
            // AVANT le strip d'annotations (les .glsli ne sont pas censes contenir
            // d'annotations, mais on prefere les stripper sur le tout au cas ou).
            NkString withIncludes = NkShaderIncludeResolver::Resolve(raw, path);
            // Strip annotations -> shader prêt à passer aux compilateurs/backends.
            return NkShaderAnnotationParser::StripAnnotations(withIncludes);
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

        // ── Helpers cache ─────────────────────────────────────────────────────────
        // Sauvegarde le resultat d'une compilation dans NkShaderCache.
        // Pour OpenGL : stocke la source GL GLSL convertie (preprocessed) en bytes.
        // Pour Vulkan : stocke le binaire SPIR-V.
        // Tag de cache PAR API : la clé de cache doit distinguer chaque backend, sinon
        // le résultat compilé pour une API (ex. SPIR-V Vulkan, ou HLSL DX11) est rechargé
        // pour une autre (DX12, Software…) → pollution. Avant : tout sauf OpenGL = "spirv"
        // → VK/DX11/DX12/SW collisionnaient (il fallait purger le cache à chaque switch).
        static const char* ApiCacheTag(NkGraphicsApi api) {
            switch (api) {
                case NkGraphicsApi::NK_GFX_API_OPENGL:   return "glsl";
                case NkGraphicsApi::NK_GFX_API_OPENGLES: return "glsles";
                case NkGraphicsApi::NK_GFX_API_WEBGL:    return "webgl";
                case NkGraphicsApi::NK_GFX_API_VULKAN:   return "spirv";
                case NkGraphicsApi::NK_GFX_API_DX11:     return "hlsl11";
                case NkGraphicsApi::NK_GFX_API_DX12:     return "hlsl12";
                case NkGraphicsApi::NK_GFX_API_METAL:    return "msl";
                case NkGraphicsApi::NK_GFX_API_SOFTWARE: return "swvm";
                default:                                 return "raw";
            }
        }

        // Cible TEXTE (le résultat compilé est une SOURCE texte, pas du binaire) :
        // OpenGL/GLES/WebGL → GLSL, DX11/DX12 → HLSL, Metal → MSL. Ces cibles stockent
        // et restaurent le cache via la STRING (outGlsl/preprocessed → glslSource/hlslSource).
        // Vulkan (SPIR-V) et Software (VM bytecode) stockent du binaire (outBytecode).
        // CRUCIAL : sans ça, au 2e run (cache hit) DX rechargeait outBytecode mais
        // fsGlslStr restait = NkSL BRUT → D3DCompile reçoit `@...` → X3000 → blanc + crash.
        static bool IsTextTarget(NkGraphicsApi api) {
            return api == NkGraphicsApi::NK_GFX_API_OPENGL ||
                   api == NkGraphicsApi::NK_GFX_API_OPENGLES ||
                   api == NkGraphicsApi::NK_GFX_API_WEBGL ||
                   api == NkGraphicsApi::NK_GFX_API_DX11 ||
                   api == NkGraphicsApi::NK_GFX_API_DX12 ||
                   api == NkGraphicsApi::NK_GFX_API_METAL;
        }

        static void SaveToShaderCache(uint64 key, NkGraphicsApi api,
                                       const NkShaderCompileResult& res,
                                       const NkString& fallbackSrc) {
            NkShaderConvertResult toCache;
            toCache.success = true;
            // Cible texte avec source disponible → stocker le TEXTE (GLSL/HLSL/MSL).
            if (IsTextTarget(api) && !res.preprocessed.Empty()) {
                const NkString& src = res.preprocessed;
                toCache.binary.Resize((uint32)src.Size());
                memcpy(toCache.binary.Data(), src.CStr(), src.Size());
            } else if (api == NkGraphicsApi::NK_GFX_API_OPENGL) {
                const NkString& src = res.preprocessed.Empty() ? fallbackSrc : res.preprocessed;
                toCache.binary.Resize((uint32)src.Size());
                memcpy(toCache.binary.Data(), src.CStr(), src.Size());
            } else {
                toCache.binary = res.bytecode;
            }
            NkShaderCache::Global().Save(key, toCache);
        }

        // Restaure depuis le cache. Retourne false si absent.
        // Remplit outBytecode (SPIR-V/VM) et/ou outGlsl (source texte GLSL/HLSL/MSL).
        static bool LoadFromShaderCache(uint64 key, NkGraphicsApi api,
                                         NkVector<uint8>& outBytecode,
                                         NkString& outGlsl) {
            auto cached = NkShaderCache::Global().Load(key);
            if (!cached.success || cached.binary.IsEmpty()) return false;
            if (IsTextTarget(api)) {
                outGlsl.Resize((uint32)cached.binary.Size());
                memcpy(outGlsl.Data(), cached.binary.Data(), cached.binary.Size());
            } else {
                outBytecode = cached.binary;
            }
            return true;
        }

        // ── Compilation ───────────────────────────────────────────────────────────
        bool NkShaderLibrary::Recompile(NkShaderProgram& prog) {
            NkString vertSrc = prog.vertPath.Empty() ? "" : ReadFile(prog.vertPath);
            NkString fragSrc = prog.fragPath.Empty() ? "" : ReadFile(prog.fragPath);

            if (vertSrc.Empty() && fragSrc.Empty()) return false;

            NkShaderCompileOptions opts;
            opts.optimize = true;

            // Backend GL stocke le GLSL converti dans preprocessed (VK -> GL).
            // On conserve ces sources converties pour les passer a glslSource.
            NkString vertGlsl = vertSrc;
            NkString fragGlsl = fragSrc;

            // Format cible pour la cle de cache : "glsl" pour OpenGL, "spirv" pour Vulkan.
            const NkString fmtKey = ApiCacheTag(mApi); // clé distincte par backend (anti-pollution)

            bool ok = true;
            if (!vertSrc.Empty()) {
                auto key = NkShaderCache::Global().ComputeKey(vertSrc, NkSLStage::NK_VERTEX, fmtKey);
                if (!LoadFromShaderCache(key, mApi, prog.vertBytecode, vertGlsl)) {
                    auto res = mBackend->Compile(vertSrc, NkShaderStage::NK_VERTEX, opts);
                    if (!res.success) {
                        fprintf(stderr, "[NkShader] VERT compile error (%s):\n%s\n",
                                prog.name.CStr(), res.errors.CStr());
                        ok = false;
                    } else {
                        prog.vertBytecode = res.bytecode;
                        if (!res.preprocessed.Empty()) vertGlsl = res.preprocessed;
                        SaveToShaderCache(key, mApi, res, vertSrc);
                    }
                }
            }
            if (!fragSrc.Empty()) {
                auto key = NkShaderCache::Global().ComputeKey(fragSrc, NkSLStage::NK_FRAGMENT, fmtKey);
                if (!LoadFromShaderCache(key, mApi, prog.fragBytecode, fragGlsl)) {
                    auto res = mBackend->Compile(fragSrc, NkShaderStage::NK_FRAGMENT, opts);
                    if (!res.success) {
                        fprintf(stderr, "[NkShader] FRAG compile error (%s):\n%s\n",
                                prog.name.CStr(), res.errors.CStr());
                        ok = false;
                    } else {
                        prog.fragBytecode = res.bytecode;
                        if (!res.preprocessed.Empty()) fragGlsl = res.preprocessed;
                        SaveToShaderCache(key, mApi, res, fragSrc);
                    }
                }
            }

            NkString geomSrc  = prog.geomPath.Empty() ? "" : ReadFile(prog.geomPath);
            NkString geomGlsl = geomSrc;
            if (!geomSrc.Empty()) {
                auto key = NkShaderCache::Global().ComputeKey(geomSrc, NkSLStage::NK_GEOMETRY, fmtKey);
                if (!LoadFromShaderCache(key, mApi, prog.geomBytecode, geomGlsl)) {
                    auto res = mBackend->Compile(geomSrc, NkShaderStage::NK_GEOMETRY, opts);
                    if (!res.success) {
                        fprintf(stderr, "[NkShader] GEOM compile error (%s):\n%s\n",
                                prog.name.CStr(), res.errors.CStr());
                        ok = false;
                    } else {
                        prog.geomBytecode = res.bytecode;
                        if (!res.preprocessed.Empty()) geomGlsl = res.preprocessed;
                        SaveToShaderCache(key, mApi, res, geomSrc);
                    }
                }
            }

            if (ok) {
                // Detruire l'ancien programme RHI s'il existait (hot-reload)
                if (prog.valid && prog.rhiHandle.IsValid())
                    mDevice->DestroyShader(prog.rhiHandle);

                // Cf CompileVF : une SEULE entree par stage, GLSL + SPIRV combines.
                // glslSource = source convertie pour le backend cible (GL GLSL si GL,
                // VK GLSL sinon). Le device choisit entre glslSource et spirvBinary.
                NkShaderDesc desc;
                if (!vertSrc.Empty()) {
                    ::nkentseu::NkShaderStageDesc vs{};
                    vs.stage      = ::nkentseu::NkShaderStage::NK_VERTEX;
                    vs.glslSource = vertGlsl.CStr();
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
                    fs.glslSource = fragGlsl.CStr();
                    fs.entryPoint = "main";
                    if (!prog.fragBytecode.Empty()) {
                        fs.spirvBinary.Resize((uint32)prog.fragBytecode.Size());
                        memcpy(fs.spirvBinary.Data(),
                               prog.fragBytecode.Data(),
                               (size_t)prog.fragBytecode.Size());
                    }
                    desc.AddStage(fs);
                }
                if (!geomSrc.Empty()) {
                    ::nkentseu::NkShaderStageDesc gs{};
                    gs.stage      = ::nkentseu::NkShaderStage::NK_GEOMETRY;
                    gs.glslSource = geomGlsl.CStr();
                    gs.entryPoint = "main";
                    if (!prog.geomBytecode.Empty()) {
                        gs.spirvBinary.Resize((uint32)prog.geomBytecode.Size());
                        memcpy(gs.spirvBinary.Data(),
                               prog.geomBytecode.Data(),
                               (size_t)prog.geomBytecode.Size());
                    }
                    desc.AddStage(gs);
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

            const NkString fmtKey = ApiCacheTag(mApi); // clé distincte par backend (anti-pollution)
            auto key = NkShaderCache::Global().ComputeKey(src, NkSLStage::NK_COMPUTE, fmtKey);

            NkString glslStr = src;
            if (!LoadFromShaderCache(key, mApi, prog.vertBytecode, glslStr)) {
                auto res = mBackend->Compile(src, NkShaderStage::NK_COMPUTE);
                if (!res.success) {
                    fprintf(stderr, "[NkShader] COMPUTE error (%s):\n%s\n",
                            prog.name.CStr(), res.errors.CStr());
                    return NkShaderHandle::Null();
                }
                prog.vertBytecode = res.bytecode;
                if (!res.preprocessed.Empty()) glslStr = res.preprocessed;
                SaveToShaderCache(key, mApi, res, src);
            }
            prog.valid = true;
            return Alloc(prog);
        }

        NkShaderHandle NkShaderLibrary::CompileVF(const NkString& vSrc, const NkString& fSrc, const NkString& name,
                                                  NkShaderBackend* backendOverride) {
            NkShaderProgram prog;
            prog.name = name.Empty() ? "inline_shader" : name;
            NkShaderCompileOptions opts; opts.optimize = false;
            NkShaderBackend* be = backendOverride ? backendOverride : mBackend;

            const NkString fmtKey = ApiCacheTag(mApi); // clé distincte par backend (anti-pollution)

            // Vertex : cache ou compilation
            NkString vsGlslStr = vSrc;
            bool vsOk = true;
            {
                auto key = NkShaderCache::Global().ComputeKey(vSrc, NkSLStage::NK_VERTEX, fmtKey);
                if (!LoadFromShaderCache(key, mApi, prog.vertBytecode, vsGlslStr)) {
                    auto vr = be->Compile(vSrc, NkShaderStage::NK_VERTEX, opts);
                    vsOk = vr.success;
                    if (vr.success) {
                        prog.vertBytecode = vr.bytecode;
                        if (!vr.preprocessed.Empty()) vsGlslStr = vr.preprocessed;
                        SaveToShaderCache(key, mApi, vr, vSrc);
                    }
                }
            }

            // Fragment : cache ou compilation
            NkString fsGlslStr = fSrc;
            bool fsOk = true;
            {
                auto key = NkShaderCache::Global().ComputeKey(fSrc, NkSLStage::NK_FRAGMENT, fmtKey);
                if (!LoadFromShaderCache(key, mApi, prog.fragBytecode, fsGlslStr)) {
                    auto fr = be->Compile(fSrc, NkShaderStage::NK_FRAGMENT, opts);
                    fsOk = fr.success;
                    if (fr.success) {
                        prog.fragBytecode = fr.bytecode;
                        if (!fr.preprocessed.Empty()) fsGlslStr = fr.preprocessed;
                        SaveToShaderCache(key, mApi, fr, fSrc);
                    }
                }
            }

            const char* vsGlsl = vsGlslStr.CStr();
            const char* fsGlsl = fsGlslStr.CStr();

            // ── Cross-compilation SPIR-V → HLSL pour les backends DirectX ─────────
            // DX11/DX12 ne consomment ni GLSL ni SPIR-V : ils compilent du HLSL via
            // D3DCompile (DX11 vs_5_0/ps_5_0, DX12 vs_5_1/ps_5_1). On convertit le
            // SPIR-V déjà produit par glslang en HLSL via SPIRV-Cross. Pour DX on NE
            // remplit PAS spirvBinary : le device DX12 prendrait le SPIR-V pour du
            // DXBC. L'entry point reste "main" (SPIRV-Cross conserve le nom du SPIR-V).
            const bool isDX = (mApi == NkGraphicsApi::NK_GFX_API_DX11 ||
                               mApi == NkGraphicsApi::NK_GFX_API_DX12);
            // Chemin NkSL : le backend a DÉJÀ généré du HLSL natif (NkSL→HLSL direct),
            // stocké dans res.preprocessed -> vsGlslStr/fsGlslStr. Donc PAS de
            // SPIRV-Cross (qui échouerait : bytecode = texte HLSL, pas du SPIR-V).
            const bool usingNkSL = (be == mNkSLBackend);
            NkString vsHlslStr, fsHlslStr;
            if (isDX && usingNkSL) {
                vsHlslStr = vsGlslStr;   // HLSL généré par NkSL (NK_HLSL_DX11/DX12)
                fsHlslStr = fsGlslStr;
            } else if (isDX) {
                // Backend DX standard (NkShaderBackendDX11/DX12) : sa methode Compile a
                // DEJA produit du HLSL (GlslToHlsl SM5 pour DX11, SM6 pour DX12) et l'a
                // expose via res.preprocessed -> vsGlslStr/fsGlslStr (cache-miss ET, via
                // le cache disque IsTextTarget, cache-hit). On le consomme DIRECTEMENT.
                //
                // NE PAS re-appeler SpirvToHlsl ici : prog.vert/fragBytecode contient du
                // HLSL (texte), pas du SPIR-V -> SPIRV-Cross levait "Invalid SPIRV format."
                // (avale dans le catch), vsHlslStr restait vide et le pipeline DX echouait.
                // C'etait le bug du shader Skin (SSBO bones) sur DX12 : Compile generait
                // bien le StructuredBuffer (SM6, 4824 chars) puis cette double conversion
                // l'effacait. PBR/Shadow/Skybox passent par le chemin NkSL (usingNkSL) et
                // n'etaient donc pas touches.
                vsHlslStr = vsGlslStr;
                fsHlslStr = fsGlslStr;
                if (vsHlslStr.Empty() && fsHlslStr.Empty()) {
                    logger.Info("[CompileVF] '{0}' : backend DX n'a pas produit de HLSL (DX KO)\n", prog.name);
                }
            }

            logger.Info("[CompileVF] '{0}' vsGlsl={1} fsGlsl={2} hlsl(vs={3},fs={4})\n",
                        prog.name,
                        (uint32)(vsGlsl ? strlen(vsGlsl) : 0),
                        (uint32)(fsGlsl ? strlen(fsGlsl) : 0),
                        (uint32)vsHlslStr.Size(), (uint32)fsHlslStr.Size());

            // DIAG temporaire : dump du HLSL genere (opt-in NK_DUMP_HLSL=<name>).
            if (isDX) {
                const char* dn = getenv("NK_DUMP_HLSL");
                if (dn && prog.name == dn) {
                    system("if not exist \"Build\\hlsl_dump\" mkdir \"Build\\hlsl_dump\"");
                    if (!vsHlslStr.Empty()) {
                        FILE* f = fopen("Build/hlsl_dump/skin_vs.hlsl", "wb");
                        if (f) { fwrite(vsHlslStr.CStr(),1,vsHlslStr.Size(),f); fclose(f); }
                    }
                    if (!fsHlslStr.Empty()) {
                        FILE* f = fopen("Build/hlsl_dump/skin_fs.hlsl", "wb");
                        if (f) { fwrite(fsHlslStr.CStr(),1,fsHlslStr.Size(),f); fclose(f); }
                    }
                }
            }
            // DIAG temporaire : dump du GLSL genere (opt-in NK_DUMP_GLSL=<name>).
            {
                const char* dn = getenv("NK_DUMP_GLSL");
                if (dn && prog.name == dn && vsGlsl && vsGlsl[0]) {
                    system("if not exist \"Build\\hlsl_dump\" mkdir \"Build\\hlsl_dump\"");
                    FILE* f = fopen("Build/hlsl_dump/dump_vs.glsl", "wb");
                    if (f) { fwrite(vsGlsl,1,strlen(vsGlsl),f); fclose(f); }
                }
            }

            // Une SEULE entree NkShaderStageDesc par stage : sinon le backend
            // Vulkan recoit 2 stages NK_VERTEX (un pour GLSL, un pour SPIRV) et
            // produit 2 VkShaderModule -> validation error VUID-...stage-06897.
            // Selon l'API : GLSL pour OpenGL, SPIRV pour Vulkan, HLSL pour DX11/DX12.
            NkShaderDesc desc;
            {
                ::nkentseu::NkShaderStageDesc vs{};
                vs.stage      = ::nkentseu::NkShaderStage::NK_VERTEX;
                vs.glslSource = vsGlsl;
                vs.entryPoint = "main";
                if (isDX) {
                    if (!vsHlslStr.Empty()) vs.hlslSource = vsHlslStr.CStr();
                } else if (!prog.vertBytecode.Empty()) {
                    vs.spirvBinary.Resize((uint32)prog.vertBytecode.Size());
                    memcpy(vs.spirvBinary.Data(),
                           prog.vertBytecode.Data(),
                           (size_t)prog.vertBytecode.Size());
                }
                desc.AddStage(vs);

                ::nkentseu::NkShaderStageDesc fs{};
                fs.stage      = ::nkentseu::NkShaderStage::NK_FRAGMENT;
                fs.glslSource = fsGlsl;
                fs.entryPoint = "main";
                if (isDX) {
                    if (!fsHlslStr.Empty()) fs.hlslSource = fsHlslStr.CStr();
                } else if (!prog.fragBytecode.Empty()) {
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
                        prog.name.CStr(), (int)vsOk, (int)fsOk);
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
            logger.Info("[NkShaderLibrary] LoadOrCompileVF '{0}' (api={1})\n", materialName, (int)mApi);

            // Si ce shader a deja ete compile sous ce nom (ex : NkRender3D a compile
            // "PBR" avant NkMaterialSystem), on retourne le handle cache directement.
            {
                auto cached = Find(materialName);
                if (cached.IsValid()) {
                    logger.Info("[NkShaderLibrary] LoadOrCompileVF '{0}' — cache hit\n", materialName);
                    return cached;
                }
            }

            // Lower-case le material name (convention POSIX-friendly).
            NkString matLower = materialName;
            for (uint32 i = 0; i < (uint32)matLower.Size(); i++) {
                char c = matLower[i];
                if (c >= 'A' && c <= 'Z') matLower[i] = (char)(c + 32);
            }

            NkString basePath = "Resources/NKRenderer/Shaders/";
            basePath += materialName;
            basePath += "/";

            // ── Opt-in NkSL par-shader ────────────────────────────────────────
            // Si <Mat>/NkSL/<mat>.{vert,frag}.nksl existent (VRAI dialecte NkSL),
            // compiler via le backend NkSL natif (générateurs maison) au lieu du
            // chemin .vk.glsl. Sans flipper le backend global → migration douce,
            // shader par shader. Lecture BRUTE (ReadFile strippe les @xxx, or
            // @binding/@location/@stage/@entry SONT la syntaxe NkSL).
            if (mNkSLBackend) {
                NkString nkVS = basePath + "NkSL/" + matLower + ".vert.nksl";
                NkString nkFS = basePath + "NkSL/" + matLower + ".frag.nksl";
                if (NkFile::Exists(nkVS.CStr()) && NkFile::Exists(nkFS.CStr())) {
                    auto readRaw = [](const NkString& p) -> NkString {
                        FILE* f = fopen(p.CStr(), "rb"); if (!f) return NkString("");
                        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                        NkString s; s.Resize((uint32)sz);
                        fread(s.Data(), 1, (size_t)sz, f); fclose(f);
                        // Résout les #include "Include/*.glsli" (BRDF/shadow/voxel partagés)
                        // SANS stripper les annotations @ : le compilateur NkSL en a besoin.
                        return NkShaderIncludeResolver::Resolve(s, p);
                    };
                    NkString vNksl = readRaw(nkVS), fNksl = readRaw(nkFS);
                    logger.Info("[NkShaderLibrary] '{0}' -> chemin NkSL (vrai dialecte) : {1}\n",
                                materialName, nkVS);
                    NkShaderHandle h = CompileVF(vNksl, fNksl, materialName, mNkSLBackend);
                    if (h.IsValid()) {
                        NkShaderProgram* prog = mPrograms.Find(h.id);
                        if (prog) { prog->vertPath = nkVS; prog->vertMtime = GetFileMtime(nkVS);
                                    prog->fragPath = nkFS; prog->fragMtime = GetFileMtime(nkFS); }
                        return h;
                    }
                    logger.Errorf("[NkShaderLibrary] '{0}' NkSL echec -> fallback .vk.glsl\n", materialName);
                }
            }

            // ── Source VK canonique + conversion automatique ──────────────────
            // Tous les backends chargent le .vk.glsl :
            //   - VK   : compile direct via glslang.
            //   - GL   : backend convertit VK -> GL via SPIRV-Cross.
            //   - DX11 : backend convertit VK -> HLSL SM5.
            //   - DX12 : backend convertit VK -> HLSL SM6.
            //   - MSL  : backend convertit VK -> MSL.
            // Strip des annotations @xxx fait dans ReadFile.
            NkString vkPath = basePath + "VK/";
            NkString vsPath = vkPath + matLower + ".vert.vk.glsl";
            NkString fsPath = vkPath + matLower + ".frag.vk.glsl";

            NkString vSrc = fallbackVS;
            NkString fSrc = fallbackFS;
            bool overrideVS = NkFile::Exists(vsPath.CStr());
            bool overrideFS = NkFile::Exists(fsPath.CStr());
            if (overrideVS) vSrc = ReadFile(vsPath);
            if (overrideFS) fSrc = ReadFile(fsPath);

            if (overrideVS || overrideFS) {
                logger.Info("[NkShader] LoadOrCompileVF '{0}' override : VS={1} FS={2}\n",
                            materialName, overrideVS ? "file" : "embedded",
                            overrideFS ? "file" : "embedded");
            }

            NkShaderHandle h = CompileVF(vSrc, fSrc, materialName);

            if (h.IsValid() && overrideVS && overrideFS) {
                NkShaderProgram* prog = mPrograms.Find(h.id);
                if (prog) {
                    prog->vertPath  = vsPath;
                    prog->vertMtime = GetFileMtime(vsPath);
                    prog->fragPath  = fsPath;
                    prog->fragMtime = GetFileMtime(fsPath);
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
                if (!prog.geomPath.Empty()) {
                    uint64 mt = GetFileMtime(prog.geomPath);
                    if (mt != prog.geomMtime) { prog.geomMtime=mt; needsReload=true; }
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
