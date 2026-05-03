// =============================================================================
// NkShaderBackend.cpp  — NKRenderer v4.0
// Compilation shaders par backend + transpiler NkSL → GLSL/HLSL/MSL.
// =============================================================================
#include "NkShaderBackend.h"

// Inclure les headers des compilateurs si disponibles
#if defined(NK_BACKEND_GL)
#  include <GL/glew.h>
#endif
#if defined(NK_BACKEND_VK)
#  include <glslang/Public/ShaderLang.h>
#  include <SPIRV/GlslangToSpv.h>
#endif
#if defined(NK_BACKEND_DX11)
#  include <d3dcompiler.h>
#endif
#if defined(NK_BACKEND_DX12)
   // DXC via COM
#endif

#include <cstring>
#include <cstdio>

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // GLSL OpenGL
    // =========================================================================
    NkShaderCompileResult NkShaderBackendGL::Compile(const NkString&              src,
                                                       NkShaderStage                stage,
                                                       const NkShaderCompileOptions& opts) {
        NkShaderCompileResult res;

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
        const char* code = src.CStr();
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
        // Store shader ID as "bytecode" (4 bytes = GLuint)
        res.bytecode.Resize(sizeof(GLuint));
        memcpy(res.bytecode.Data(), &shader, sizeof(GLuint));
        res.success = true;
#else
        // Pas de backend GL : validation syntaxique minimale
        res.success = !src.Empty();
        if (!res.success) res.errors = "Empty shader source";
        // Stocker la source telle quelle
        res.bytecode.Resize(src.Size()+1);
        memcpy(res.bytecode.Data(), src.CStr(), src.Size()+1);
#endif
        return res;
    }

    // =========================================================================
    // GLSL Vulkan → SPIR-V (via glslang)
    // =========================================================================
    NkShaderCompileResult NkShaderBackendVK::Compile(const NkString&              src,
                                                       NkShaderStage                stage,
                                                       const NkShaderCompileOptions& opts) {
        NkShaderCompileResult res;

#if defined(NK_BACKEND_VK) && defined(GLSLANG_AVAILABLE)
        glslang::InitializeProcess();

        EShLanguage lang;
        switch (stage) {
            case NkShaderStage::NK_VERTEX:   lang = EShLangVertex;   break;
            case NkShaderStage::NK_FRAGMENT: lang = EShLangFragment; break;
            case NkShaderStage::NK_GEOMETRY: lang = EShLangGeometry; break;
            case NkShaderStage::NK_COMPUTE:  lang = EShLangCompute;  break;
            default:                         lang = EShLangVertex;   break;
        }

        glslang::TShader shader(lang);
        const char* code = src.CStr();
        shader.setStrings(&code, 1);
        shader.setEnvInput(glslang::EShSourceGlsl, lang,
                            glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        shader.setEnvTarget(glslang::EShTargetSpv,    glslang::EShTargetSpv_1_5);

        EShMessages msgs = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
        if (!shader.parse(&glslang::DefaultTBuiltInResource, 460, false, msgs)) {
            res.errors  = shader.getInfoLog();
            res.success = false;
            glslang::FinalizeProcess();
            return res;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(msgs)) {
            res.errors  = program.getInfoLog();
            res.success = false;
            glslang::FinalizeProcess();
            return res;
        }

        std::vector<uint32_t> spirv;
        glslang::GlslangToSpv(*program.getIntermediate(lang), spirv);
        res.bytecode.Resize((uint32)spirv.size()*4);
        memcpy(res.bytecode.Data(), spirv.data(), res.bytecode.Size());
        res.success = true;
        glslang::FinalizeProcess();
#else
        // Sans glslang : conserver la source GLSL Vulkan brute
        res.success = !src.Empty();
        if (!res.success) { res.errors = "Empty shader source"; return res; }
        res.bytecode.Resize(src.Size()+1);
        memcpy(res.bytecode.Data(), src.CStr(), src.Size()+1);
#endif
        return res;
    }

    // =========================================================================
    // HLSL DX11 (D3DCompile)
    // =========================================================================
    NkShaderCompileResult NkShaderBackendDX11::Compile(const NkString&              src,
                                                         NkShaderStage                stage,
                                                         const NkShaderCompileOptions& opts) {
        NkShaderCompileResult res;

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
        HRESULT hr = D3DCompile(src.CStr(), src.Size(), nullptr, nullptr, nullptr,
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
        res.success = !src.Empty();
        if (!res.success) { res.errors = "Empty HLSL source"; return res; }
        res.bytecode.Resize(src.Size()+1);
        memcpy(res.bytecode.Data(), src.CStr(), src.Size()+1);
#endif
        return res;
    }

    // =========================================================================
    // HLSL DX12 (DXC)
    // =========================================================================
    NkShaderCompileResult NkShaderBackendDX12::Compile(const NkString&              src,
                                                         NkShaderStage                stage,
                                                         const NkShaderCompileOptions& opts) {
        NkShaderCompileResult res;
        // DXC COM API — similaire à DX11 mais avec IDxcCompiler3
        // Stub : même comportement sans backend
        res.success = !src.Empty();
        if (!res.success) { res.errors = "Empty HLSL SM6 source"; return res; }
        res.bytecode.Resize(src.Size()+1);
        memcpy(res.bytecode.Data(), src.CStr(), src.Size()+1);
        return res;
    }

    // =========================================================================
    // MSL (Metal Shading Language)
    // =========================================================================
    NkShaderCompileResult NkShaderBackendMSL::Compile(const NkString&              src,
                                                        NkShaderStage                stage,
                                                        const NkShaderCompileOptions& opts) {
        NkShaderCompileResult res;
        // Sur macOS : MTLLibrary compilation via Objective-C Metal API
        // Retourner la source MSL telle quelle (compilée au runtime par Metal)
        res.success = !src.Empty();
        if (!res.success) { res.errors = "Empty MSL source"; return res; }
        res.bytecode.Resize(src.Size()+1);
        memcpy(res.bytecode.Data(), src.CStr(), src.Size()+1);
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
            case NkGraphicsApi::NK_OPENGL:
            case NkGraphicsApi::NK_OPENGLES: return TranspileToGL(nksl);
            case NkGraphicsApi::NK_VULKAN:   return TranspileToVK(nksl);
            case NkGraphicsApi::NK_DX11:     return TranspileToDX11(nksl);
            case NkGraphicsApi::NK_DX12:     return TranspileToDX12(nksl);
            case NkGraphicsApi::NK_METAL:    return TranspileToMSL(nksl);
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
            case NkGraphicsApi::NK_OPENGL:
            case NkGraphicsApi::NK_OPENGLES:  return new NkShaderBackendGL();
            case NkGraphicsApi::NK_VULKAN:    return new NkShaderBackendVK();
            case NkGraphicsApi::NK_DX11:      return new NkShaderBackendDX11();
            case NkGraphicsApi::NK_DX12:      return new NkShaderBackendDX12();
            case NkGraphicsApi::NK_METAL:     return new NkShaderBackendMSL();
            default:                           return new NkShaderBackendGL();
        }
    }

} // namespace renderer
} // namespace nkentseu
