// =============================================================================
// NkShaderBackend.cpp  — NKRenderer v4.0
// Compilation shaders par backend + transpiler NkSL → GLSL/HLSL/MSL.
// =============================================================================
#include "NkShaderBackend.h"
#include "NKLogger/NkLog.h"

// Inclure les headers des compilateurs si disponibles
#if defined(NK_BACKEND_GL)
#  include <GL/glew.h>
#endif
// Vulkan : on delegue la compilation GLSL -> SPIR-V au compilateur partage
// (NKRHI/SL/NkGLSLCompiler) qui s'appuie sur NKGLSlang in-tree (meme toolchain
// que NKRenderer => ABI consistant). Si NK_RHI_GLSLANG_ENABLED n'est pas defini,
// la fonction renvoie un stub d'erreur et le device Vulkan failera proprement.
#include "NKRHI/SL/NkGLSLCompiler.h"
#include "NKRHI/ShaderConvert/NkShaderAnnotations.h"
#include "NKRHI/ShaderConvert/NkShaderConvert.h"
#if defined(NK_BACKEND_DX11)
#  include <d3dcompiler.h>
#endif
#if defined(NK_BACKEND_DX12)
   // DXC via COM
#endif

#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace nkentseu {
    namespace renderer {

        // =========================================================================
        // GLSL OpenGL
        //
        // Convention cross-API : la source canonique est en GLSL Vulkan (avec
        // annotations semantiques + `layout(set=,binding=)` + `push_constant`).
        // Le backend GL fait le pipeline complet :
        //   1. Strip annotations @xxx (NkShaderAnnotationParser).
        //   2. GLSL VK -> SPIR-V (glslang) -> GLSL OpenGL (SPIRV-Cross).
        //   3. Compile GL via glCreateShader/glCompileShader.
        // Si la source est deja en GLSL OpenGL (legacy : pas de `layout(set=`),
        // on bypass la conversion (compile direct).
        // =========================================================================

        // Transforme la sortie SPIRV-Cross pour un push_constant :
        //   "struct PC { float m0; float m1; }; uniform PC _PushConstants;"
        //   → "uniform vec4 _PushConstants[N];"
        // et patche les accès membres "_PushConstants.m0" → "_PushConstants[0].x".
        // Compatible avec NkOpenGLCommandBuffer::PushConstants qui cherche _PushConstants[0].
        static NkString PatchPushConstantsForGL(const NkString& nkSrc) {
            std::string s(nkSrc.CStr(), nkSrc.Size());
            const std::string kVar = "_PushConstants";

            // 1. Trouver "uniform TYPENAME _PushConstants;" — extraire TYPENAME
            std::string typeName;
            {
                const std::string pfx = "uniform ";
                size_t pos = 0;
                while ((pos = s.find(pfx, pos)) != std::string::npos) {
                    size_t a = pos + pfx.size();
                    while (a < s.size() && (s[a]==' '||s[a]=='\t')) ++a;
                    size_t ts = a;
                    while (a < s.size() && s[a]!=' '&&s[a]!='\t'&&s[a]!='\n'&&s[a]!=';') ++a;
                    std::string tn = s.substr(ts, a - ts);
                    while (a < s.size() && (s[a]==' '||s[a]=='\t')) ++a;
                    if (s.substr(a, kVar.size()) == kVar) { typeName = tn; break; }
                    ++pos;
                }
            }
            // Pas de push_constant struct, ou deja au bon format vec4
            if (typeName.empty() || typeName == "vec4") return nkSrc;

            // 2. Parser "struct TYPENAME { TYPE m0; ... };"
            // fc = nombre de floats (vec4 slots * 4) occupés par le membre
            struct Member { std::string name, type; int floats; };
            std::vector<Member> members;
            {
                std::string pat = "struct " + typeName;
                size_t pos = s.find(pat);
                if (pos == std::string::npos) return nkSrc;
                size_t bo = s.find('{', pos + pat.size());
                if (bo == std::string::npos) return nkSrc;
                int depth = 1; size_t p = bo + 1;
                while (p < s.size() && depth > 0) {
                    if (s[p]=='{') ++depth;
                    else if (s[p]=='}') --depth;
                    ++p;
                }
                size_t bc = p - 1;
                size_t bp = bo + 1;
                while (bp < bc) {
                    while (bp < bc && (s[bp]==' '||s[bp]=='\t'||s[bp]=='\r'||s[bp]=='\n')) ++bp;
                    if (bp >= bc) break;
                    size_t ts = bp;
                    while (bp < bc && s[bp]!=' '&&s[bp]!='\t') ++bp;
                    std::string mtype = s.substr(ts, bp - ts);
                    while (bp < bc && (s[bp]==' '||s[bp]=='\t')) ++bp;
                    size_t ns = bp;
                    while (bp < bc && s[bp]!=';'&&s[bp]!='\n'&&s[bp]!='\r') ++bp;
                    std::string mname = s.substr(ns, bp - ns);
                    while (!mname.empty() && (mname.back()==' '||mname.back()=='\t')) mname.pop_back();
                    if (bp < bc) ++bp;
                    if (!mname.empty() && !mtype.empty()) {
                        int fc = 1;
                        if      (mtype=="vec2") fc = 2;
                        else if (mtype=="vec3") fc = 3;
                        else if (mtype=="vec4") fc = 4;
                        else if (mtype=="mat2") fc = 4;  // 2 colonnes × vec2 (std430: 2 vec4 padded)
                        else if (mtype=="mat3") fc = 12; // 3 colonnes × vec4 (std430 padding)
                        else if (mtype=="mat4") fc = 16; // 4 colonnes × vec4
                        members.push_back({mname, mtype, fc});
                    }
                }
            }
            if (members.empty()) return nkSrc;

            // 3. Nombre de vec4 nécessaires
            int totalFloats = 0;
            for (auto& m : members) totalFloats += m.floats;
            int numVec4 = (totalFloats + 3) / 4;
            if (numVec4 == 0) numVec4 = 1;

            // 4. Remplacer les accès "_PushConstants.member" → expression GLSL correcte
            static const char* kComp[] = {"x","y","z","w"};
            int floatOff = 0;
            for (auto& m : members) {
                std::string oldRef = kVar + "." + m.name;
                int slot = floatOff / 4, comp = floatOff % 4;
                std::string newRef;
                if (m.type == "mat4") {
                    newRef = "mat4("
                           + kVar + "[" + std::to_string(slot)   + "],"
                           + kVar + "[" + std::to_string(slot+1) + "],"
                           + kVar + "[" + std::to_string(slot+2) + "],"
                           + kVar + "[" + std::to_string(slot+3) + "])";
                } else if (m.type == "mat3") {
                    newRef = "mat3(vec3("
                           + kVar + "[" + std::to_string(slot)   + "]),vec3("
                           + kVar + "[" + std::to_string(slot+1) + "]),vec3("
                           + kVar + "[" + std::to_string(slot+2) + "]))";
                } else if (m.type == "mat2") {
                    newRef = "mat2("
                           + kVar + "[" + std::to_string(slot) + "].xy,"
                           + kVar + "[" + std::to_string(slot) + "].zw)";
                } else if (m.floats == 1) {
                    newRef = kVar + "[" + std::to_string(slot) + "]." + kComp[comp];
                } else {
                    std::string sw;
                    for (int j = 0; j < m.floats; ++j) sw += kComp[(comp + j) % 4];
                    newRef = kVar + "[" + std::to_string(slot) + "]." + sw;
                }
                size_t pos = 0;
                while ((pos = s.find(oldRef, pos)) != std::string::npos) {
                    s.replace(pos, oldRef.size(), newRef);
                    pos += newRef.size();
                }
                floatOff += m.floats;
            }

            // 5. Remplacer "uniform TYPENAME _PushConstants;" → "uniform vec4 _PushConstants[N];"
            {
                std::string oldDecl = "uniform " + typeName + " " + kVar + ";";
                std::string newDecl = "uniform vec4 " + kVar + "[" + std::to_string(numVec4) + "];";
                size_t pos = s.find(oldDecl);
                if (pos != std::string::npos) s.replace(pos, oldDecl.size(), newDecl);
            }

            // 6. Supprimer la définition "struct TYPENAME { ... };"
            {
                std::string pat = "struct " + typeName;
                size_t pos = s.find(pat);
                if (pos != std::string::npos) {
                    size_t bo = s.find('{', pos + pat.size());
                    if (bo != std::string::npos) {
                        int depth = 1; size_t p = bo + 1;
                        while (p < s.size() && depth > 0) {
                            if (s[p]=='{') ++depth;
                            else if (s[p]=='}') --depth;
                            ++p;
                        }
                        size_t bc = p - 1;
                        size_t semi = s.find(';', bc);
                        size_t end = (semi != std::string::npos) ? semi + 1 : bc + 1;
                        if (end < s.size() && s[end]=='\n') ++end;
                        s.erase(pos, end - pos);
                    }
                }
            }

            return NkString(s.c_str());
        }

        static bool LooksLikeVulkanGlsl(const NkString& src) {
            // Detecte GLSL Vulkan-style : layout avec 'set=' (dans n'importe quel
            // ordre de qualificateurs : "layout(set=", "layout(std140, set=", etc.)
            // ou 'push_constant'. Le token 'set =' peut avoir des espaces autour.
            const char* s = src.CStr();
            return strstr(s, "layout(set=")    != nullptr
                || strstr(s, "layout(set =")   != nullptr
                || strstr(s, ", set=")          != nullptr  // layout(std140, set=0, ...)
                || strstr(s, ", set =")         != nullptr
                || strstr(s, ",set=")           != nullptr
                || strstr(s, "push_constant")   != nullptr;
        }

        static ::nkentseu::NkSLStage ToNkSLStage(NkShaderStage s) {
            switch (s) {
                case NkShaderStage::NK_VERTEX:    return ::nkentseu::NkSLStage::NK_VERTEX;
                case NkShaderStage::NK_FRAGMENT:  return ::nkentseu::NkSLStage::NK_FRAGMENT;
                case NkShaderStage::NK_GEOMETRY:  return ::nkentseu::NkSLStage::NK_GEOMETRY;
                case NkShaderStage::NK_COMPUTE:   return ::nkentseu::NkSLStage::NK_COMPUTE;
                case NkShaderStage::NK_TESS_CTRL: return ::nkentseu::NkSLStage::NK_TESS_CONTROL;
                case NkShaderStage::NK_TESS_EVAL: return ::nkentseu::NkSLStage::NK_TESS_EVAL;
                default:                          return ::nkentseu::NkSLStage::NK_VERTEX;
            }
        }

        NkShaderCompileResult NkShaderBackendGL::Compile(const NkString&              src,
                                                        NkShaderStage                stage,
                                                        const NkShaderCompileOptions& opts) {
            NkShaderCompileResult res;
            if (src.Empty()) { res.errors = "Empty shader source"; return res; }

            // Strip annotations centralise dans NkShaderLibrary au moment du
            // chargement -- la source recue ici est deja propre.
            // Phase : si GLSL Vulkan, convertir en GLSL OpenGL via SPIRV-Cross.
            // Source canonique = VK. Mais on tolere les sources legacy en GL natif
            // (heuristique : pas de layout(set=,binding=) ni push_constant).
            NkString glslGL;
            const char* stageName = (stage == NkShaderStage::NK_VERTEX) ? "VS" : "FS";
            if (LooksLikeVulkanGlsl(src)) {
                ::nkentseu::NkShaderConvertResult conv =
                    ::nkentseu::NkShaderConverter::GlslToGlsl(src, ToNkSLStage(stage), "shader");
                if (!conv.success) {
                    res.success = false;
                    res.errors  = NkString("VK->GL conversion failed: ") + conv.errors;
                    logger.Errorf("[NkShaderBackendGL] %s VK->GL FAIL: %s\n",
                                  stageName, res.errors.CStr());
                    return res;
                }
                glslGL = PatchPushConstantsForGL(conv.source);
                logger.Info("[NkShaderBackendGL] {0} VK->GL OK: {1} chars\n",
                            NkString(stageName), (uint32)glslGL.Size());
            } else {
                glslGL = src;
                logger.Info("[NkShaderBackendGL] {0} native GL (pas VK): {1} chars\n",
                            NkString(stageName), (uint32)glslGL.Size());
            }

            // Ecrit le shader GL converti dans Build/debug_gl/ pour inspection.
            // Utile pour verifier que la conversion VK->GL produit du code valide.
            {
                static int sDbgIdx = 0;
                char dbgPath[256];
    #ifdef _WIN32
                system("if not exist \"Build\\debug_gl\" mkdir \"Build\\debug_gl\"");
    #else
                system("mkdir -p Build/debug_gl");
    #endif
                snprintf(dbgPath, sizeof(dbgPath), "Build/debug_gl/%03d_%s.gl.glsl",
                         sDbgIdx++, stageName);
                FILE* df = fopen(dbgPath, "wb");
                if (df) { fwrite(glslGL.CStr(), 1, glslGL.Size(), df); fclose(df); }
            }

            // glslGL contient le source GLSL OpenGL converti (ou le source natif GL).
            // On le stocke TOUJOURS dans res.preprocessed pour que CompileVF puisse
            // l'utiliser comme glslSource dans NkShaderStageDesc — sinon le device
            // OpenGL recevrait le source VK GLSL original et rejetterait layout(set=...).
            res.preprocessed = glslGL;

            // ── Compilation GL (si runtime GL disponible dans ce module) ──────────
    #if defined(NK_BACKEND_GL)
            GLenum glStage;
            switch (stage) {
                case NkShaderStage::NK_VERTEX:    glStage = GL_VERTEX_SHADER;   break;
                case NkShaderStage::NK_FRAGMENT:  glStage = GL_FRAGMENT_SHADER; break;
                case NkShaderStage::NK_GEOMETRY:  glStage = GL_GEOMETRY_SHADER; break;
                case NkShaderStage::NK_COMPUTE:   glStage = GL_COMPUTE_SHADER;  break;
                default:                          glStage = GL_VERTEX_SHADER;   break;
            }

            GLuint shader = glCreateShader(glStage);
            const char* code = glslGL.CStr();
            glShaderSource(shader, 1, &code, nullptr);
            glCompileShader(shader);

            GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                GLint len = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
                res.errors.Resize(len);
                glGetShaderInfoLog(shader, len, nullptr, res.errors.Data());
                glDeleteShader(shader);
                res.success = false;
                return res;
            }
            // Store pre-compiled shader ID as "bytecode" (4 bytes = GLuint).
            // Le device GL utilisera ce GLuint directement lors du link.
            res.bytecode.Resize(sizeof(GLuint));
            memcpy(res.bytecode.Data(), &shader, sizeof(GLuint));
            res.success = true;
    #else
            // Pas de runtime GL dans ce module : on passe le source GL converti
            // via preprocessed (deja fait ci-dessus). CompileVF l'utilisera comme
            // glslSource, et le device OpenGL le compilera lui-meme via glCreateShader.
            res.success = !glslGL.Empty();
            if (!res.success) res.errors = "Empty converted GL source";
            // bytecode = source GL texte (le device peut l'utiliser comme source)
            res.bytecode.Resize(glslGL.Size() + 1);
            memcpy(res.bytecode.Data(), glslGL.CStr(), glslGL.Size() + 1);
    #endif
            return res;
        }

        // =========================================================================
        // GLSL Vulkan → SPIR-V (delegue a NkGLSLToSPIRV qui utilise NKGLSlang in-tree)
        //
        // Conversion d'enum : renderer::NkShaderStage est un index 0..7 dense, alors
        // que ::nkentseu::NkShaderStage (RHI) est un bitfield (1<<n). On mappe.
        // =========================================================================
        static ::nkentseu::NkShaderStage ToRHIStage(NkShaderStage s) {
            switch (s) {
                case NkShaderStage::NK_VERTEX:    return ::nkentseu::NkShaderStage::NK_VERTEX;
                case NkShaderStage::NK_FRAGMENT:  return ::nkentseu::NkShaderStage::NK_FRAGMENT;
                case NkShaderStage::NK_GEOMETRY:  return ::nkentseu::NkShaderStage::NK_GEOMETRY;
                case NkShaderStage::NK_COMPUTE:   return ::nkentseu::NkShaderStage::NK_COMPUTE;
                case NkShaderStage::NK_TESS_CTRL: return ::nkentseu::NkShaderStage::NK_TESS_CTRL;
                case NkShaderStage::NK_TESS_EVAL: return ::nkentseu::NkShaderStage::NK_TESS_EVAL;
                case NkShaderStage::NK_MESH:      return ::nkentseu::NkShaderStage::NK_MESH;
                case NkShaderStage::NK_TASK:      return ::nkentseu::NkShaderStage::NK_TASK;
            }
            return ::nkentseu::NkShaderStage::NK_VERTEX;
        }

        NkShaderCompileResult NkShaderBackendVK::Compile(const NkString&              src,
                                                        NkShaderStage                stage,
                                                        const NkShaderCompileOptions& opts) {
            NkShaderCompileResult res;
            if (src.Empty()) { res.errors = "Empty shader source"; return res; }

            const char* stageName =
                stage == NkShaderStage::NK_VERTEX   ? "VS" :
                stage == NkShaderStage::NK_FRAGMENT ? "FS" :
                stage == NkShaderStage::NK_GEOMETRY ? "GS" :
                stage == NkShaderStage::NK_COMPUTE  ? "CS" : "??";
            logger.Info("[NkShaderBackendVK] Compile {0} start (src={1} bytes)\n",
                        stageName, (uint32)src.Size());

            // Strip annotations centralise dans NkShaderLibrary au moment du
            // chargement -- la source recue ici est deja propre.
            const char* entry = opts.entryPoint.Empty() ? "main" : opts.entryPoint.CStr();
            ::nkentseu::NkGLSLCompileResult c = ::nkentseu::NkGLSLToSPIRV(
                ToRHIStage(stage), src.CStr(), entry);

            if (!c.success) {
                res.success = false;
                res.errors  = c.errorLog ? c.errorLog : "NkGLSLToSPIRV: erreur inconnue";
                logger.Errorf("[NkShaderBackendVK] Compile %s FAIL: %s\n",
                              stageName, res.errors.CStr());
                return res;
            }

            // SPIR-V words -> octets
            const uint32 wordCount = c.spirv.Size();
            res.bytecode.Resize(wordCount * 4);
            memcpy(res.bytecode.Data(), c.spirv.Data(), wordCount * 4);
            res.success = true;
            logger.Info("[NkShaderBackendVK] Compile {0} OK ({1} SPIR-V words)\n",
                        stageName, wordCount);
            return res;
        }

        // =========================================================================
        // HLSL DX11 (D3DCompile)
        //
        // Source canonique = GLSL Vulkan. Si la source recue est du GLSL VK
        // (detecte via LooksLikeVulkanGlsl), on fait la conversion GLSL->HLSL SM5
        // via SPIRV-Cross avant de passer a D3DCompile. Sinon on suppose que
        // c'est deja du HLSL (cas custom via NkMaterialTemplateDesc.vertSrcDX11).
        // =========================================================================
        NkShaderCompileResult NkShaderBackendDX11::Compile(const NkString&              src,
                                                            NkShaderStage                stage,
                                                            const NkShaderCompileOptions& opts) {
            NkShaderCompileResult res;

            // Auto-convert GLSL VK → HLSL SM5
            NkString hlslSrc = src;
            if (LooksLikeVulkanGlsl(src)) {
                auto conv = ::nkentseu::NkShaderConverter::GlslToHlsl(
                    src, ToNkSLStage(stage), 50, "dx11_shader");
                if (!conv.success) {
                    res.errors = NkString("[DX11] GLSL->HLSL SM5 failed: ") + conv.errors;
                    return res;
                }
                hlslSrc = conv.source;
            }

    #if defined(NK_BACKEND_DX11)
            const char* target;
            switch (stage) {
                case NkShaderStage::NK_VERTEX:   target = "vs_5_0"; break;
                case NkShaderStage::NK_FRAGMENT: target = "ps_5_0"; break;
                case NkShaderStage::NK_GEOMETRY: target = "gs_5_0"; break;
                case NkShaderStage::NK_COMPUTE:  target = "cs_5_0"; break;
                default:                         target = "vs_5_0"; break;
            }
            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
            if (opts.debug)    flags |= D3DCOMPILE_DEBUG;
            if (opts.optimize) flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;

            ID3DBlob *codeBlob=nullptr, *errBlob=nullptr;
            HRESULT hr = D3DCompile(hlslSrc.CStr(), hlslSrc.Size(), nullptr, nullptr, nullptr,
                                    opts.entryPoint.CStr(), target, flags, 0,
                                    &codeBlob, &errBlob);
            if (FAILED(hr)) {
                if (errBlob) {
                    res.errors = (const char*)errBlob->GetBufferPointer();
                    errBlob->Release();
                }
                res.success = false;
                return res;
            }
            res.bytecode.Resize((uint32)codeBlob->GetBufferSize());
            memcpy(res.bytecode.Data(), codeBlob->GetBufferPointer(), res.bytecode.Size());
            if (codeBlob) codeBlob->Release();
            res.success = true;
    #else
            // Hors DX11 : stocker le HLSL converti (utilisable pour inspection/debug)
            res.success = !hlslSrc.Empty();
            if (!res.success) { res.errors = "Empty HLSL source"; return res; }
            res.bytecode.Resize(hlslSrc.Size()+1);
            memcpy(res.bytecode.Data(), hlslSrc.CStr(), hlslSrc.Size()+1);
    #endif
            return res;
        }

        // =========================================================================
        // HLSL DX12 (DXC)
        //
        // Meme convention : source canonique = GLSL VK, conversion GLSL->HLSL SM6
        // via SPIRV-Cross avant compilation DXC. Le stub DXC sera remplace en F.B.3
        // par la vraie chaine IDxcCompiler3, mais la conversion GLSL->HLSL est deja
        // fonctionnelle et peut etre validee independamment.
        // =========================================================================
        NkShaderCompileResult NkShaderBackendDX12::Compile(const NkString&              src,
                                                            NkShaderStage                stage,
                                                            const NkShaderCompileOptions& opts) {
            NkShaderCompileResult res;

            // Auto-convert GLSL VK → HLSL SM6
            NkString hlslSrc = src;
            if (LooksLikeVulkanGlsl(src)) {
                auto conv = ::nkentseu::NkShaderConverter::GlslToHlsl(
                    src, ToNkSLStage(stage), 60, "dx12_shader");
                if (!conv.success) {
                    res.errors = NkString("[DX12] GLSL->HLSL SM6 failed: ") + conv.errors;
                    return res;
                }
                hlslSrc = conv.source;
            }

            // Stub DXC (F.B.3) : stocker le HLSL SM6 converti comme bytecode.
            // Le NkDirectX12Device::CreateShader lira ce source et l'envoiera a DXC.
            res.success = !hlslSrc.Empty();
            if (!res.success) { res.errors = "Empty HLSL SM6 source"; return res; }
            res.bytecode.Resize(hlslSrc.Size()+1);
            memcpy(res.bytecode.Data(), hlslSrc.CStr(), hlslSrc.Size()+1);
            return res;
        }

        // =========================================================================
        // MSL (Metal Shading Language)
        //
        // Source canonique = GLSL VK, conversion GLSL->MSL via SPIRV-Cross.
        // Le MTLLibrary est compile au runtime par Metal (pas besoin de bytecode
        // intermediaire — on retourne le source MSL comme bytecode).
        // =========================================================================
        NkShaderCompileResult NkShaderBackendMSL::Compile(const NkString&              src,
                                                            NkShaderStage                stage,
                                                            const NkShaderCompileOptions& opts) {
            NkShaderCompileResult res;

            // Auto-convert GLSL VK → MSL
            NkString mslSrc = src;
            if (LooksLikeVulkanGlsl(src)) {
                auto conv = ::nkentseu::NkShaderConverter::GlslToMsl(
                    src, ToNkSLStage(stage), "msl_shader");
                if (!conv.success) {
                    res.errors = NkString("[MSL] GLSL->MSL failed: ") + conv.errors;
                    return res;
                }
                mslSrc = conv.source;
            }

            // Source MSL retournee telle quelle (Metal compile au runtime via MTLLibrary)
            res.success = !mslSrc.Empty();
            if (!res.success) { res.errors = "Empty MSL source"; return res; }
            res.bytecode.Resize(mslSrc.Size()+1);
            memcpy(res.bytecode.Data(), mslSrc.CStr(), mslSrc.Size()+1);
            return res;
        }

        // =========================================================================
        // NkSL — transpiler vers le backend cible
        // =========================================================================
        NkShaderBackendNkSL::NkShaderBackendNkSL(NkGraphicsApi targetApi)
            : mTarget(targetApi) {
            mDelegate = NkCreateShaderBackend(targetApi, false);
        }

        NkShaderCompileResult NkShaderBackendNkSL::Compile(const NkString&              src,
                                                            NkShaderStage                stage,
                                                            const NkShaderCompileOptions& opts) {
            // 1. Transpiler NkSL vers le langage cible
            NkString transpiled = Transpile(src, mTarget);

            // 2. Compiler via le backend délégué
            if (mDelegate)
                return mDelegate->Compile(transpiled, stage, opts);

            NkShaderCompileResult res;
            res.success        = true;
            res.preprocessed   = transpiled;
            res.bytecode.Resize(transpiled.Size()+1);
            memcpy(res.bytecode.Data(), transpiled.CStr(), transpiled.Size()+1);
            return res;
        }

        NkString NkShaderBackendNkSL::Transpile(const NkString& nksl,
                                                NkGraphicsApi   target) const {
            switch (target) {
                case NkGraphicsApi::NK_GFX_API_OPENGL:
                case NkGraphicsApi::NK_GFX_API_OPENGLES: return TranspileToGL(nksl);
                case NkGraphicsApi::NK_GFX_API_VULKAN:   return TranspileToVK(nksl);
                case NkGraphicsApi::NK_GFX_API_DX11:     return TranspileToDX11(nksl);
                case NkGraphicsApi::NK_GFX_API_DX12:     return TranspileToDX12(nksl);
                case NkGraphicsApi::NK_GFX_API_METAL:    return TranspileToMSL(nksl);
                default:                         return TranspileToGL(nksl);
            }
        }

        // ─────────────────────────────────────────────────────────────────────────
        // Transpiler NkSL → GLSL OpenGL
        // Transformations clés :
        //   @uniform N { } → layout(binding=N,std140) uniform
        //   @push { }      → layout(push_constant) uniform (ignoré pour GL, → binding=0)
        //   @texture(N)    → layout(binding=N) uniform sampler2D
        //   @in / @out     → in / out avec layout(location)
        //   @vertex { }    → code du vertex shader
        //   @fragment { }  → code du fragment shader
        //   @target GL { } → inclus, autres @target { } → ignorés
        //   mat4 → mat4, vec4 → vec4 (même syntaxe)
        //   mul(A,B) → A*B  (si présent, DX style)
        // ─────────────────────────────────────────────────────────────────────────
        static NkString StripTargetBlocks(const NkString& src, const char* keep) {
            // Supprimer les blocs @target XXX { ... } sauf celui nommé `keep`
            // Implémentation simplifiée — parser ligne par ligne
            NkString out;
            const char* p   = src.CStr();
            const char* end = p + src.Size();

            while (p < end) {
                // Chercher @target
                const char* atgt = strstr(p, "@target");
                if (!atgt || atgt >= end) {
                    // Plus de @target → copier le reste
                    out += NkString(p, (uint32)(end - p));
                    break;
                }
                // Copier jusqu'au @target
                out += NkString(p, (uint32)(atgt - p));
                p = atgt + 7; // après "@target"
                // Lire le nom du backend
                while (p < end && (*p==' '||*p=='\t')) p++;
                const char* nameStart = p;
                while (p < end && *p!='{' && *p!=' ' && *p!='\t') p++;
                NkString bkName(nameStart, (uint32)(p - nameStart));
                // Trouver le '{' ouvrant
                while (p < end && *p!='{') p++;
                if (p >= end) break;
                p++; // sauter '{'
                // Trouver le '}' fermant (depth tracking)
                int depth = 1;
                const char* bodyStart = p;
                while (p < end && depth > 0) {
                    if (*p=='{') depth++;
                    else if (*p=='}') depth--;
                    p++;
                }
                // Si c'est le backend qu'on garde, inclure le corps
                if (bkName == keep) {
                    out += NkString(bodyStart, (uint32)(p - bodyStart - 1));
                }
                // sinon : ignorer le bloc
            }
            return out;
        }

        NkString NkShaderBackendNkSL::TranspileToGL(const NkString& src) const {
            NkString s = StripTargetBlocks(src, "GL");

            // Remplacements simples par parsing séquentiel
            // @vertex { } → extrait le contenu dans un fichier séparé
            // Pour cette impl : retourner source GLSL 460 avec header

            NkString out = "#version 460 core\n\n";

            // @uniform N { } → layout(binding=N,std140) uniform Block_N {
            NkString work = s;
            // Remplacement @uniform N → layout(binding=N, std140) uniform
            // @in type name : LOC → layout(location=LOC) in type name
            // @out type name : LOC → layout(location=LOC) out type name
            // @texture(N) name → layout(binding=N) uniform sampler2D name
            // @push { } → layout(binding=99,std140) uniform PushBlock { (approximation GL)

            // Transformations textuelles basiques
            auto Replace = [](NkString& str, const char* from, const char* to) {
                NkString result;
                const char* p   = str.CStr();
                const char* end = p + str.Size();
                size_t flen     = strlen(from);
                while (p < end) {
                    if (strncmp(p, from, flen) == 0) {
                        result += to;
                        p += flen;
                    } else {
                        result += NkString(p, 1);
                        p++;
                    }
                }
                str = result;
            };

            // @vertex / @fragment block markers → retirer (pour un fichier par étape)
            Replace(work, "@vertex {",   "// === VERTEX ===");
            Replace(work, "@fragment {", "// === FRAGMENT ===");
            // @in → in (avec location automatique — simplification)
            Replace(work, "@in ",  "in ");
            Replace(work, "@out ", "out ");
            // @texture → uniform sampler2D
            Replace(work, "@texture", "// texture binding ");
            // @push → uniform PushBlock
            Replace(work, "@push {", "layout(std140) uniform PushBlock {");
            // mul() DX style → *
            Replace(work, "mul(", "(/* mul */ ");

            out += work;
            return out;
        }

        NkString NkShaderBackendNkSL::TranspileToVK(const NkString& src) const {
            NkString s = StripTargetBlocks(src, "VK");
            NkString out = "#version 460 core\n"
                            "#extension GL_ARB_separate_shader_objects : enable\n\n";
            // Idem GL avec push_constant + Y-flip
            out += s;
            out += "\n// VK: gl_Position.y = -gl_Position.y;\n";
            return out;
        }

        NkString NkShaderBackendNkSL::TranspileToDX11(const NkString& src) const {
            NkString s   = StripTargetBlocks(src, "DX11");
            NkString out = "// HLSL SM5.0\n\n";
            // @uniform N → cbuffer Block_N : register(bN)
            // @texture(N) → Texture2D tName : register(tN)
            // vec4 → float4, mat4 → float4x4, etc.
            auto Replace = [](NkString& str, const char* from, const char* to) {
                NkString r; const char* p=str.CStr(),*e=p+str.Size(); size_t fl=strlen(from);
                while(p<e){if(strncmp(p,from,fl)==0){r+=to;p+=fl;}else{r+=NkString(p,1);p++;}}
                str=r;
            };
            Replace(s, "vec2 ",  "float2 ");
            Replace(s, "vec3 ",  "float3 ");
            Replace(s, "vec4 ",  "float4 ");
            Replace(s, "mat3 ",  "float3x3 ");
            Replace(s, "mat4 ",  "float4x4 ");
            Replace(s, "mix(",   "lerp(");
            Replace(s, "fract(", "frac(");
            Replace(s, "mod(",   "fmod(");
            out += s;
            return out;
        }

        NkString NkShaderBackendNkSL::TranspileToDX12(const NkString& src) const {
            NkString s = TranspileToDX11(src); // SM6 est compatible SM5 au niveau syntaxe
            // Ajouter support bindless + wave ops
            return NkString("// HLSL SM6 (DXC)\n") + s;
        }

        NkString NkShaderBackendNkSL::TranspileToMSL(const NkString& src) const {
            NkString s   = StripTargetBlocks(src, "MSL");
            NkString out = "#include <metal_stdlib>\nusing namespace metal;\n\n";
            // vec4 → float4, mat4 → float4x4, texture2d etc.
            auto Replace = [](NkString& str, const char* from, const char* to) {
                NkString r; const char* p=str.CStr(),*e=p+str.Size(); size_t fl=strlen(from);
                while(p<e){if(strncmp(p,from,fl)==0){r+=to;p+=fl;}else{r+=NkString(p,1);p++;}}
                str=r;
            };
            Replace(s, "vec2 ", "float2 ");
            Replace(s, "vec3 ", "float3 ");
            Replace(s, "vec4 ", "float4 ");
            Replace(s, "mat4 ", "float4x4 ");
            Replace(s, "mix(",  "mix(");  // MSL a mix()
            out += s;
            return out;
        }

        // =========================================================================
        // Fabrique
        // =========================================================================
        NkShaderBackend* NkCreateShaderBackend(NkGraphicsApi api, bool useNkSL) {
            if (useNkSL) return new NkShaderBackendNkSL(api);
            switch (api) {
                case NkGraphicsApi::NK_GFX_API_OPENGL:
                case NkGraphicsApi::NK_GFX_API_OPENGLES:  return new NkShaderBackendGL();
                case NkGraphicsApi::NK_GFX_API_VULKAN:    return new NkShaderBackendVK();
                case NkGraphicsApi::NK_GFX_API_DX11:      return new NkShaderBackendDX11();
                case NkGraphicsApi::NK_GFX_API_DX12:      return new NkShaderBackendDX12();
                case NkGraphicsApi::NK_GFX_API_METAL:     return new NkShaderBackendMSL();
                default:                           return new NkShaderBackendGL();
            }
        }

    } // namespace renderer
} // namespace nkentseu