#pragma once
// =============================================================================
// NkSWShaderBridge.h  — v4.0
//
// PROBLÈME RÉSOLU :
//   Le fichier .sw.sksl unique mélangeait les déclarations `in` du vertex et
//   du fragment dans le même AST. Le NkSLReflector les confondait toutes comme
//   des vertex inputs → stride=32 au lieu de 20.
//
// SOLUTION :
//   Compiler vertex et fragment depuis des FICHIERS SÉPARÉS (ou sources séparées).
//   Ainsi la reflection du vertex ne voit QUE les `in` du vertex.
//
// FONCTIONNEMENT :
//   1. Compiler triangle.vert.sw.sksl → reflection vertex (location, type, nom)
//   2. Compiler triangle.frag.sw.sksl → reflection fragment (samplers, uniforms)
//   3. Déduire stride + offsets depuis la reflection vertex uniquement
//   4. Lire chaque attribut selon son type exact (vec2=8b, vec3=12b, vec4=16b…)
//   5. Pour déterminer ce que représente chaque attribut : utiliser le NOM
//      déclaré dans le shader (@location(0) in vec2 aPosition → "aPosition")
//      et des heuristiques par nom + type (pas par location fixe)
//
// USAGE :
//   auto res = swbridge::NkCompileFiles(
//       "Resources/Shaders/Model/triangle.vert.sw.sksl",
//       "Resources/Shaders/Model/triangle.frag.sw.sksl");
//   if (res.success) {
//       auto* vs = new NkVertexShaderSoftware(std::move(res.vertFn));
//       auto* ps = new NkPixelShaderSoftware (std::move(res.fragFn));
//       shaderDesc.AddSWFn(NkShaderStage::NK_VERTEX,   (const void*)vs);
//       shaderDesc.AddSWFn(NkShaderStage::NK_FRAGMENT, (const void*)ps);
//   }
// =============================================================================

#include "NKRHI/SL/NkSLTypes.h"
#include "NKRHI/SL/NkSLCompiler.h"
#include "NKRHI/SL/NkSLIntegration.h"
#include "NKRHI/Software/NkSoftwareDevice.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"
#include <cstring>
#include <cmath>

#define NKSW_LOG(...) logger.Infof ("[NkSWBridge] " __VA_ARGS__)
#define NKSW_ERR(...) logger.Errorf("[NkSWBridge] " __VA_ARGS__)

namespace nkentseu {
    namespace swbridge {

        // =============================================================================
        // Résultat
        // =============================================================================
        struct NkSWBridgeResult {
            bool                   success = false;
            NkString               error;
            NkVertexShaderSoftware vertFn;
            NkPixelShaderSoftware  fragFn;
            NkSLReflection         vertRefl;  // reflection vertex (inputs)
            NkSLReflection         fragRefl;  // reflection fragment (samplers, uniforms)
        };

        // =============================================================================
        // Description d'un attribut vertex résolu
        // =============================================================================
        struct AttrDesc {
            uint32       location = 0;
            uint32       offset   = 0;
            NkSLBaseType type     = NkSLBaseType::NK_FLOAT;
            uint32       bytes    = 4;
            NkString     name;          // nom original : "aPosition", "aColor", "aUV", …
        };

        // =============================================================================
        // Taille en bytes d'un type NkSL vertex
        // =============================================================================
        static inline uint32 ByteSize(NkSLBaseType t) {
            switch (t) {
                case NkSLBaseType::NK_FLOAT:  return 4;
                case NkSLBaseType::NK_VEC2:   return 8;
                case NkSLBaseType::NK_VEC3:   return 12;
                case NkSLBaseType::NK_VEC4:   return 16;
                case NkSLBaseType::NK_UVEC4:  return 4;   // 4×uint8 packed RGBA
                case NkSLBaseType::NK_IVEC2:  return 8;
                case NkSLBaseType::NK_IVEC3:  return 12;
                case NkSLBaseType::NK_IVEC4:  return 16;
                default:                      return 4;
            }
        }

        // =============================================================================
        // Construire la liste des attributs triés par location, avec offsets calculés
        // Depuis la reflection du stage VERTEX uniquement.
        // =============================================================================
        static NkVector<AttrDesc> BuildLayout(const NkSLReflection& vertRefl, uint32& outStride) {
            // Trier les vertex inputs par location croissante
            NkVector<const NkSLVertexInput*> sorted;
            for (uint32 i = 0; i < (uint32)vertRefl.vertexInputs.Size(); ++i)
                sorted.PushBack(&vertRefl.vertexInputs[i]);

            for (uint32 i = 0; i < (uint32)sorted.Size(); ++i)
                for (uint32 j = i+1; j < (uint32)sorted.Size(); ++j)
                    if (sorted[j]->location < sorted[i]->location) {
                        auto* t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t;
                    }

            NkVector<AttrDesc> attrs;
            uint32 off = 0;
            for (uint32 i = 0; i < (uint32)sorted.Size(); ++i) {
                AttrDesc a;
                a.location = sorted[i]->location;
                a.type     = sorted[i]->baseType;
                a.bytes    = ByteSize(a.type);
                a.offset   = off;
                a.name     = sorted[i]->name;
                off += a.bytes;
                attrs.PushBack(a);
            }
            outStride = off;
            return attrs;
        }

        // =============================================================================
        // Sémantique d'un attribut depuis son NOM et son TYPE
        // (pas depuis sa location — la location est arbitraire côté utilisateur)
        //
        // Règles (par ordre de priorité) :
        //   1. Nom contient "pos"  → POSITION
        //   2. Nom contient "col" ou "color" ou "colour" → COLOR
        //   3. Nom contient "uv" ou "tex" ou "coord"     → UV
        //   4. Nom contient "norm"                        → NORMAL
        //   5. Fallback par type :
        //      vec2 sans contexte → UV si une position existe déjà, sinon POSITION
        //      vec3/vec4/uvec4    → COLOR si pas encore vu
        // =============================================================================
        enum class AttrSemantic { POSITION, COLOR, UV, NORMAL, UNKNOWN };

        static NkString ToLower(const NkString& s) {
            NkString r;
            for (uint32 i = 0; i < (uint32)s.Size(); ++i) {
                char c = s[i];
                if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
                r += c;
            }
            return r;
        }

        static bool Contains(const NkString& s, const char* sub) {
            return s.Contains(sub);
        }

        static AttrSemantic GuessSemantic(const AttrDesc& a,
                                        bool hasPos, bool hasCol, bool hasUV) {
            NkString low = ToLower(a.name);

            // ── Règles par nom (priorité maximale) ───────────────────────────────────
            if (Contains(low, "pos"))                    return AttrSemantic::POSITION;
            if (Contains(low, "color") ||
                Contains(low, "colour") ||
                Contains(low, "col"))                    return AttrSemantic::COLOR;
            if (Contains(low, "uv")   ||
                Contains(low, "tex")  ||
                Contains(low, "coord"))                  return AttrSemantic::UV;
            if (Contains(low, "norm"))                   return AttrSemantic::NORMAL;

            // ── Fallback par type ─────────────────────────────────────────────────────
            if (!hasPos &&
                (a.type == NkSLBaseType::NK_VEC2 ||
                a.type == NkSLBaseType::NK_VEC3 ||
                a.type == NkSLBaseType::NK_VEC4))       return AttrSemantic::POSITION;

            if (!hasCol &&
                (a.type == NkSLBaseType::NK_VEC3 ||
                a.type == NkSLBaseType::NK_VEC4 ||
                a.type == NkSLBaseType::NK_UVEC4))      return AttrSemantic::COLOR;

            if (!hasUV &&
                a.type == NkSLBaseType::NK_VEC2)        return AttrSemantic::UV;

            return AttrSemantic::UNKNOWN;
        }

        // =============================================================================
        // Lecture d'un attribut depuis le vertex buffer
        // =============================================================================
        static inline float RdF(const uint8* p, uint32 off) {
            float v; memcpy(&v, p + off, 4); return v;
        }

        static inline void ReadAttr(const uint8* src, const AttrDesc& a,
                                    float& x, float& y, float& z, float& w) {
            x = y = z = 0.f; w = 1.f;
            switch (a.type) {
                case NkSLBaseType::NK_FLOAT:
                    x = RdF(src, a.offset);
                    break;
                case NkSLBaseType::NK_VEC2:
                    x = RdF(src, a.offset); y = RdF(src, a.offset+4);
                    break;
                case NkSLBaseType::NK_VEC3:
                    x = RdF(src, a.offset); y = RdF(src, a.offset+4); z = RdF(src, a.offset+8);
                    break;
                case NkSLBaseType::NK_VEC4:
                    x = RdF(src, a.offset); y = RdF(src, a.offset+4);
                    z = RdF(src, a.offset+8); w = RdF(src, a.offset+12);
                    break;
                case NkSLBaseType::NK_UVEC4: {
                    // 4 uint8 packed → normaliser en [0,1]
                    const uint8* cp = src + a.offset;
                    x=cp[0]/255.f; y=cp[1]/255.f; z=cp[2]/255.f; w=cp[3]/255.f;
                    break;
                }
                case NkSLBaseType::NK_IVEC2: {
                    int32 ix, iy;
                    memcpy(&ix, src+a.offset,   4);
                    memcpy(&iy, src+a.offset+4, 4);
                    x=(float)ix; y=(float)iy;
                    break;
                }
                case NkSLBaseType::NK_IVEC3: {
                    int32 ix, iy, iz;
                    memcpy(&ix, src+a.offset,   4);
                    memcpy(&iy, src+a.offset+4, 4);
                    memcpy(&iz, src+a.offset+8, 4);
                    x=(float)ix; y=(float)iy; z=(float)iz;
                    break;
                }
                default:
                    x = RdF(src, a.offset);
                    break;
            }
        }

        // =============================================================================
        // Projection mat4 column-major × vec4
        // =============================================================================
        static void ProjMV(const float* m,
                        float vx, float vy, float vz, float vw,
                        float& ox, float& oy, float& oz, float& ow) {
            ox = m[0]*vx + m[4]*vy + m[ 8]*vz + m[12]*vw;
            oy = m[1]*vx + m[5]*vy + m[ 9]*vz + m[13]*vw;
            oz = m[2]*vx + m[6]*vy + m[10]*vz + m[14]*vw;
            ow = m[3]*vx + m[7]*vy + m[11]*vz + m[15]*vw;
        }

        // =============================================================================
        // Texture nearest-neighbor
        // =============================================================================
        static void SampleTex(const NkSWTexture* tex, float u, float v,
                            float& r, float& g, float& b, float& a) {
            r = g = b = a = 1.f;
            if (!tex || tex->mips.Empty()) return;
            const uint32 w = tex->Width(0), h = tex->Height(0);
            if (!w || !h) return;
            float uf = u - floorf(u);
            float vf = v - floorf(v);
            const int tx = (int)(uf*w); const int ty = (int)(vf*h);
            const int cx = tx<0?0:(tx>=(int)w?(int)w-1:tx);
            const int cy = ty<0?0:(ty>=(int)h?(int)h-1:ty);
            const uint32 bpp = tex->Bpp();
            const uint8* p = tex->mips[0].Data() + ((uint32)cy*w+(uint32)cx)*bpp;
            if (bpp>=4) { r=p[0]/255.f; g=p[1]/255.f; b=p[2]/255.f; a=p[3]/255.f; }
            else if(bpp==3) { r=p[0]/255.f; g=p[1]/255.f; b=p[2]/255.f; a=1.f; }
            else { r=g=b=p[0]/255.f; a=1.f; }
        }

        // =============================================================================
        // Fabriquer la lambda vertex
        // =============================================================================
        static NkVertexShaderSoftware MakeVertexFn(const NkVector<AttrDesc>& attrs,
                                                    uint32 stride,
                                                    bool hasProj) {
            // Pré-calculer la sémantique de chaque attribut (fait UNE seule fois ici)
            // et la capturer dans la lambda — pas de lookup à chaque vertex.

            struct ResolvedAttr {
                AttrDesc     desc;
                AttrSemantic semantic;
                bool         colHasAlpha; // vec4/uvec4 → true, vec3 → false
            };

            NkVector<ResolvedAttr> resolved;
            bool hasPos=false, hasCol=false, hasUV=false;

            for (uint32 i = 0; i < (uint32)attrs.Size(); ++i) {
                AttrSemantic sem = GuessSemantic(attrs[i], hasPos, hasCol, hasUV);
                ResolvedAttr ra;
                ra.desc     = attrs[i];
                ra.semantic = sem;
                ra.colHasAlpha = (attrs[i].type == NkSLBaseType::NK_VEC4 ||
                                attrs[i].type == NkSLBaseType::NK_UVEC4);
                resolved.PushBack(ra);

                if (sem == AttrSemantic::POSITION) hasPos = true;
                if (sem == AttrSemantic::COLOR)    hasCol = true;
                if (sem == AttrSemantic::UV)       hasUV  = true;
            }

            // Log de débogage (une seule fois à la création)
            NKSW_LOG("MakeVertexFn: stride=%u, %u attrs:", stride, (uint32)resolved.Size());
            for (uint32 i = 0; i < (uint32)resolved.Size(); ++i) {
                const char* semStr = "?";
                switch(resolved[i].semantic) {
                    case AttrSemantic::POSITION: semStr = "POSITION"; break;
                    case AttrSemantic::COLOR:    semStr = "COLOR";    break;
                    case AttrSemantic::UV:       semStr = "UV";       break;
                    case AttrSemantic::NORMAL:   semStr = "NORMAL";   break;
                    default: break;
                }
                NKSW_LOG("  [%u] loc=%u name='%s' off=%u bytes=%u -> %s",
                        i, resolved[i].desc.location, resolved[i].desc.name.CStr(),
                        resolved[i].desc.offset, resolved[i].desc.bytes, semStr);
            }

            return [resolved, stride, hasProj](const void* vdata, uint32 idx,
                                                const void* udata) -> NkVertexSoftware
            {
                const uint8* src = (const uint8*)vdata + (uint64)idx * stride;

                float px=0,py=0,pz=0;
                float cr=1,cg=1,cb=1,ca=1;
                float pu=0,pv=0;
                float nx=0,ny=0,nz=1;

                for (uint32 i = 0; i < (uint32)resolved.Size(); ++i) {
                    float x,y,z,w;
                    ReadAttr(src, resolved[i].desc, x, y, z, w);

                    switch (resolved[i].semantic) {
                        case AttrSemantic::POSITION: px=x; py=y; pz=z; break;
                        case AttrSemantic::COLOR:
                            cr=x; cg=y; cb=z;
                            ca = resolved[i].colHasAlpha ? w : 1.f;
                            break;
                        case AttrSemantic::UV:     pu=x; pv=y; break;
                        case AttrSemantic::NORMAL: nx=x; ny=y; nz=z; break;
                        default: break;
                    }
                }

                NkVertexSoftware out{};
                out.uv     = { pu, pv };
                out.color  = { cr, cg, cb, ca };
                out.normal = { nx, ny, nz };

                if (hasProj && udata) {
                    const float* proj = (const float*)udata;
                    float ox,oy,oz,ow;
                    ProjMV(proj, px, py, pz, 1.f, ox, oy, oz, ow);
                    out.position = { ox, oy, oz, ow };
                } else {
                    out.position = { px, py, pz, 1.f };
                }

                return out;
            };
        }

        // =============================================================================
        // Fabriquer la lambda fragment
        // =============================================================================
        static NkPixelShaderSoftware MakeFragmentFn(bool hasTex) {
            if (hasTex) {
                return [](const NkVertexSoftware& f, const void*,
                        const void* tp) -> math::NkVec4 {
                    float r,g,b,a;
                    SampleTex((const NkSWTexture*)tp, f.uv.x, f.uv.y, r, g, b, a);
                    return { r*f.color.r, g*f.color.g, b*f.color.b, a*f.color.a };
                };
            }
            return [](const NkVertexSoftware& f, const void*, const void*) -> math::NkVec4 {
                return { f.color.r, f.color.g, f.color.b, f.color.a };
            };
        }

        // =============================================================================
        // Charger le texte d'un fichier
        // =============================================================================
        static bool LoadFile(const char* path, NkString& out) {
            FILE* f = fopen(path, "rb");
            if (!f) return false;
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            if (sz <= 0) { fclose(f); return false; }
            NkVector<char> buf; buf.Resize((usize)sz + 1, '\0');
            fread(buf.Data(), 1, (usize)sz, f);
            fclose(f);
            out = NkString(buf.Data());
            return true;
        }

        // =============================================================================
        // Compiler depuis deux sources séparées (vertex / fragment)
        // =============================================================================
        static NkSWBridgeResult NkCompileSources(
            const NkString& vertSrc, const NkString& fragSrc,
            const NkString& vertName = "shader.vert.sw.sksl",
            const NkString& fragName = "shader.frag.sw.sksl")
        {
            NkSWBridgeResult res;
            NkSLCompiler& compiler = nksl::GetCompiler();

            // ── Stage VERTEX ──────────────────────────────────────────────────────────
            NkSLCompileOptions vo; vo.entryPoint = "vert_main";
            auto vr = compiler.CompileWithReflection(
                vertSrc, NkSLStage::NK_VERTEX, NkSLTarget::NK_CPLUSPLUS, vo, vertName);

            if (!vr.result.success) {
                res.error = "Vertex: ";
                for (uint32 i = 0; i < (uint32)vr.result.errors.Size(); ++i)
                    res.error += vr.result.errors[i].message + " | ";
                NKSW_ERR("Vertex fail [%s]: %s", vertName.CStr(), res.error.CStr());
                return res;
            }
            res.vertRefl = vr.reflection;

            // ── Stage FRAGMENT ────────────────────────────────────────────────────────
            NkSLCompileOptions fo; fo.entryPoint = "frag_main";
            auto fr = compiler.CompileWithReflection(
                fragSrc, NkSLStage::NK_FRAGMENT, NkSLTarget::NK_CPLUSPLUS, fo, fragName);

            if (!fr.result.success) {
                res.error = "Fragment: ";
                for (uint32 i = 0; i < (uint32)fr.result.errors.Size(); ++i)
                    res.error += fr.result.errors[i].message + " | ";
                NKSW_ERR("Fragment fail [%s]: %s", fragName.CStr(), res.error.CStr());
                return res;
            }
            res.fragRefl = fr.reflection;

            // ── Layout vertex depuis reflection VERTEX uniquement ─────────────────────
            uint32 stride = 0;
            NkVector<AttrDesc> attrs = BuildLayout(res.vertRefl, stride);

            if (attrs.Empty()) {
                res.error = "Aucun vertex input dans " + vertName;
                NKSW_ERR("%s", res.error.CStr());
                return res;
            }

            // ── Détecter UBO (projection) et textures ─────────────────────────────────
            bool hasProj = false, hasTex = false;
            // Chercher dans les deux reflections
            auto checkResources = [&](const NkSLReflection& refl) {
                for (uint32 i = 0; i < (uint32)refl.resources.Size(); ++i) {
                    if (refl.resources[i].kind == NkSLResourceKind::NK_UNIFORM_BUFFER)
                        hasProj = true;
                    if (refl.resources[i].kind == NkSLResourceKind::NK_SAMPLED_TEXTURE)
                        hasTex = true;
                }
            };
            checkResources(res.vertRefl);
            checkResources(res.fragRefl);

            // ── Log ───────────────────────────────────────────────────────────────────
            NKSW_LOG("Compile OK: vert='%s' frag='%s' stride=%u inputs=%u proj=%s tex=%s",
                    vertName.CStr(), fragName.CStr(), stride, (uint32)attrs.Size(),
                    hasProj?"yes":"no", hasTex?"yes":"no");

            // ── Générer les lambdas ───────────────────────────────────────────────────
            res.vertFn  = MakeVertexFn(attrs, stride, hasProj);
            res.fragFn  = MakeFragmentFn(hasTex);
            res.success = true;
            return res;
        }

        // =============================================================================
        // Compiler depuis deux fichiers séparés (API principale)
        // =============================================================================
        static NkSWBridgeResult NkCompileFiles(const char* vertPath, const char* fragPath) {
            NkString vs, fs;
            if (!LoadFile(vertPath, vs)) {
                NkSWBridgeResult r; r.error = NkString("Cannot open: ") + vertPath;
                NKSW_ERR("%s", r.error.CStr()); return r;
            }
            if (!LoadFile(fragPath, fs)) {
                NkSWBridgeResult r; r.error = NkString("Cannot open: ") + fragPath;
                NKSW_ERR("%s", r.error.CStr()); return r;
            }
            return NkCompileSources(vs, fs, NkString(vertPath), NkString(fragPath));
        }

        // =============================================================================
        // Compatibilité : compiler depuis un fichier unique (ancienne API)
        // Le fichier doit contenir les deux stages mais on les compile séparément.
        // La reflection sera incorrecte si les `in` fragment sont déclarés globalement,
        // mais on utilise le filtrage par @stage pour extraire seulement les vertex inputs.
        //
        // NOTE : préférer NkCompileFiles avec deux fichiers séparés.
        // =============================================================================
        static NkSWBridgeResult NkCompileFile(const char* path, uint32 /*hintStride*/ = 0) {
            NkString src;
            if (!LoadFile(path, src)) {
                NkSWBridgeResult r; r.error = NkString("Cannot open: ") + path;
                NKSW_ERR("%s", r.error.CStr()); return r;
            }
            return NkCompileSources(src, src, NkString(path), NkString(path));
        }

        // =============================================================================
        // Compatibilité : compiler depuis un source inline
        // =============================================================================
        static NkSWBridgeResult NkCompile(const NkString& src,
                                        const NkString& filename = "inline.sw.sksl",
                                        uint32 /*hint*/ = 0) {
            return NkCompileSources(src, src, filename, filename);
        }

    } // namespace swbridge
} // namespace nkentseu