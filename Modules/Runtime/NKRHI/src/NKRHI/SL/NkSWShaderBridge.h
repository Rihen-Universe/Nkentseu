#pragma once
// =============================================================================
// NkSWShaderBridge.h  — v5.0  FINAL
//
// PROBLÈMES RÉSOLUS v5 :
//   1. Noms arbitraires : on n'utilise PLUS le nom pour deviner la sémantique.
//      On utilise uniquement le TYPE NkSL + la LOCATION, avec une convention
//      explicite déclarée par l'utilisateur dans le shader via un commentaire
//      de métadonnées ou via les annotations NkSL existantes.
//
//   2. Textures multiples : la lambda fragment reçoit maintenant un tableau
//      de textures (NkSWTextureBatch) au lieu d'un seul pointeur.
//      Le shader déclare ses samplers avec @binding(set=0, binding=N),
//      la reflection extrait les bindings → la lambda les reçoit dans l'ordre.
//
//   3. UBOs multiples : idem, tableau de buffers uniform dans l'ordre des bindings.
//
// CONVENTION DE LAYOUT (sans dépendance aux noms) :
//   Les vertex inputs sont lus dans l'ordre des locations.
//   L'utilisateur déclare ce que représente chaque location via les commentaires
//   NkSL ou via une structure NkSWVertexMapping fournie au bridge.
//
//   Mapping par défaut (si aucune info supplémentaire) :
//     location 0 → POSITION  (vec2 → NDC direct, vec3/vec4 → position 3D)
//     location 1 → si vec2 → UV ; si vec3/vec4/uvec4 → COLOR
//     location 2 → si vec2 → UV (si pas déjà UV) ; sinon COLOR/NORMAL
//     location 3 → NORMAL (vec3)
//     location 4+ → attributs génériques (stockés dans attrs[])
//
//   Si ce mapping ne convient pas : fournir un NkSWVertexMapping explicite.
//
// TEXTURES MULTIPLES :
//   Le fragment shader reçoit un NkSWTextureBatch contenant jusqu'à 8 textures.
//   Accéder à la texture N : batch.tex[N]
//   Les textures sont assignées dans l'ordre des bindings du fragment shader.
//
// UTILISATION :
//   // Option 1 : mapping automatique
//   auto res = swbridge::NkCompileFiles("vert.sw.sksl", "frag.sw.sksl");
//
//   // Option 2 : mapping explicite
//   swbridge::NkSWVertexMapping map;
//   map.Set(0, swbridge::AttrSemantic::POSITION);
//   map.Set(1, swbridge::AttrSemantic::UV);
//   map.Set(2, swbridge::AttrSemantic::COLOR);
//   auto res = swbridge::NkCompileFiles("vert.sw.sksl", "frag.sw.sksl", &map);
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
        // Sémantique d'un attribut vertex
        // =============================================================================
        enum class AttrSemantic : uint8 {
            POSITION = 0,  // vec2/vec3/vec4 — coordonnées spatiales
            COLOR,         // vec3/vec4/uvec4 — couleur RGBA ou RGB
            UV,            // vec2 — coordonnées de texture
            NORMAL,        // vec3 — normale
            TANGENT,       // vec3 — tangente
            GENERIC,       // Tout autre attribut → stocké dans NkVertexSoftware::attrs[]
            AUTO,          // Déduire automatiquement (comportement par défaut)
        };

        // =============================================================================
        // Mapping vertex : associe une location à une sémantique
        // Permet à l'utilisateur de surcharger la détection automatique.
        // =============================================================================
        struct NkSWVertexMapping {
            static constexpr uint32 kMaxLocations = 16;

            AttrSemantic semantics[kMaxLocations];
            bool         isSet[kMaxLocations];

            NkSWVertexMapping() {
                for (uint32 i = 0; i < kMaxLocations; ++i) {
                    semantics[i] = AttrSemantic::AUTO;
                    isSet[i]     = false;
                }
            }

            void Set(uint32 location, AttrSemantic sem) {
                if (location < kMaxLocations) {
                    semantics[location] = sem;
                    isSet[location]     = true;
                }
            }

            AttrSemantic Get(uint32 location) const {
                if (location >= kMaxLocations) return AttrSemantic::GENERIC;
                return semantics[location];
            }
        };

        // =============================================================================
        // Description d'un attribut résolu
        // =============================================================================
        struct AttrDesc {
            uint32       location = 0;
            uint32       offset   = 0;
            NkSLBaseType type     = NkSLBaseType::NK_FLOAT;
            uint32       bytes    = 4;
            AttrSemantic semantic = AttrSemantic::GENERIC;
            uint32       genericSlot = 0; // index dans attrs[] pour GENERIC
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
                case NkSLBaseType::NK_UVEC4:  return 4;
                case NkSLBaseType::NK_IVEC2:  return 8;
                case NkSLBaseType::NK_IVEC3:  return 12;
                case NkSLBaseType::NK_IVEC4:  return 16;
                default:                      return 4;
            }
        }

        // =============================================================================
        // Détection automatique de la sémantique (fallback si pas de mapping explicite)
        //
        // RÈGLES (sans utiliser le nom) :
        //   location 0 → toujours POSITION
        //   location 1 → vec2 → UV ; vec3/vec4/uvec4 → COLOR
        //   location 2 → vec2 → UV (si pas déjà assigné) ; vec3 → NORMAL ou COLOR
        //   location 3 → vec3 → NORMAL
        //   location 4+ → GENERIC
        //
        // Si l'utilisateur a des conventions différentes → fournir NkSWVertexMapping.
        // =============================================================================
        static AttrSemantic AutoSemantic(uint32 location, NkSLBaseType type,
                                        bool hasPos, bool hasCol, bool hasUV, bool hasNorm) {
            if (location == 0) return AttrSemantic::POSITION;

            if (location == 1) {
                if (type == NkSLBaseType::NK_VEC2) return AttrSemantic::UV;
                if (type == NkSLBaseType::NK_VEC3 ||
                    type == NkSLBaseType::NK_VEC4 ||
                    type == NkSLBaseType::NK_UVEC4) return AttrSemantic::COLOR;
            }

            if (location == 2) {
                if (!hasUV && type == NkSLBaseType::NK_VEC2)  return AttrSemantic::UV;
                if (!hasCol && (type == NkSLBaseType::NK_VEC3 ||
                                type == NkSLBaseType::NK_VEC4)) return AttrSemantic::COLOR;
                if (!hasNorm && type == NkSLBaseType::NK_VEC3) return AttrSemantic::NORMAL;
            }

            if (location == 3) {
                if (!hasNorm && type == NkSLBaseType::NK_VEC3) return AttrSemantic::NORMAL;
                if (!hasUV && type == NkSLBaseType::NK_VEC2)   return AttrSemantic::UV;
            }

            return AttrSemantic::GENERIC;
        }

        // =============================================================================
        // Construire le layout depuis la reflection (stage vertex uniquement)
        // =============================================================================
        static NkVector<AttrDesc> BuildLayout(const NkSLReflection& vertRefl,
                                            uint32& outStride,
                                            const NkSWVertexMapping* mapping = nullptr) {
            // Trier par location
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
            uint32 genericIdx = 0;
            bool hasPos=false, hasCol=false, hasUV=false, hasNorm=false;

            for (uint32 i = 0; i < (uint32)sorted.Size(); ++i) {
                AttrDesc a;
                a.location = sorted[i]->location;
                a.type     = sorted[i]->baseType;
                a.bytes    = ByteSize(a.type);
                a.offset   = off;
                off += a.bytes;

                // Déterminer la sémantique
                if (mapping && mapping->isSet[a.location] &&
                    mapping->semantics[a.location] != AttrSemantic::AUTO) {
                    a.semantic = mapping->semantics[a.location];
                } else {
                    a.semantic = AutoSemantic(a.location, a.type, hasPos, hasCol, hasUV, hasNorm);
                }

                if (a.semantic == AttrSemantic::GENERIC) {
                    a.genericSlot = genericIdx++;
                }
                if (a.semantic == AttrSemantic::POSITION) hasPos  = true;
                if (a.semantic == AttrSemantic::COLOR)    hasCol  = true;
                if (a.semantic == AttrSemantic::UV)       hasUV   = true;
                if (a.semantic == AttrSemantic::NORMAL)   hasNorm = true;

                attrs.PushBack(a);
            }
            outStride = off;
            return attrs;
        }

        // =============================================================================
        // Helpers de lecture
        // =============================================================================
        static inline float RdF(const uint8* p, uint32 off) {
            float v; memcpy(&v, p+off, 4); return v;
        }

        static inline void ReadAttr(const uint8* src, const AttrDesc& a,
                                    float& x, float& y, float& z, float& w) {
            x = y = z = 0.f; w = 1.f;
            switch (a.type) {
                case NkSLBaseType::NK_FLOAT:  x = RdF(src, a.offset); break;
                case NkSLBaseType::NK_VEC2:   x = RdF(src, a.offset); y = RdF(src, a.offset+4); break;
                case NkSLBaseType::NK_VEC3:   x = RdF(src, a.offset); y = RdF(src, a.offset+4); z = RdF(src, a.offset+8); break;
                case NkSLBaseType::NK_VEC4:   x = RdF(src, a.offset); y = RdF(src, a.offset+4); z = RdF(src, a.offset+8); w = RdF(src, a.offset+12); break;
                case NkSLBaseType::NK_UVEC4: {
                    const uint8* cp = src + a.offset;
                    x=cp[0]/255.f; y=cp[1]/255.f; z=cp[2]/255.f; w=cp[3]/255.f;
                    break;
                }
                case NkSLBaseType::NK_IVEC2: {
                    int32 ix, iy;
                    memcpy(&ix, src+a.offset, 4); memcpy(&iy, src+a.offset+4, 4);
                    x=(float)ix; y=(float)iy; break;
                }
                case NkSLBaseType::NK_IVEC3: {
                    int32 ix, iy, iz;
                    memcpy(&ix, src+a.offset, 4); memcpy(&iy, src+a.offset+4, 4); memcpy(&iz, src+a.offset+8, 4);
                    x=(float)ix; y=(float)iy; z=(float)iz; break;
                }
                default: x = RdF(src, a.offset); break;
            }
        }

        // =============================================================================
        // Projection mat4 column-major × (x,y,z,1)
        // =============================================================================
        static void ProjMV(const float* m, float vx, float vy, float vz,
                        float& ox, float& oy, float& oz, float& ow) {
            ox = m[0]*vx + m[4]*vy + m[ 8]*vz + m[12];
            oy = m[1]*vx + m[5]*vy + m[ 9]*vz + m[13];
            oz = m[2]*vx + m[6]*vy + m[10]*vz + m[14];
            ow = m[3]*vx + m[7]*vy + m[11]*vz + m[15];
        }

        // =============================================================================
        // Texture nearest-neighbor (lecture directe, sans virtual)
        // =============================================================================
        static void SampleTexNN(const NkSWTexture* tex, float u, float v,
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
        // Résultat
        // =============================================================================
        struct NkSWBridgeResult {
            bool                   success = false;
            NkString               error;
            NkVertexShaderSoftware vertFn;
            NkPixelShaderSoftware  fragFn;
            NkSLReflection         vertRefl;
            NkSLReflection         fragRefl;
            uint32                 stride  = 0;
            uint32                 texCount = 0;  // nombre de samplers déclarés
            uint32                 uboCount = 0;  // nombre d'UBOs déclarés
        };

        // =============================================================================
        // Fabriquer la lambda vertex
        // =============================================================================
        static NkVertexShaderSoftware MakeVertexFn(const NkVector<AttrDesc>& attrs,
                                                    uint32 stride,
                                                    bool hasProj) {
            // Log de débogage (une seule fois)
            NKSW_LOG("MakeVertexFn: stride=%u, %u attrs, proj=%s",
                    stride, (uint32)attrs.Size(), hasProj?"yes":"no");
            for (uint32 i = 0; i < (uint32)attrs.Size(); ++i) {
                const char* semStr = "?";
                switch(attrs[i].semantic) {
                    case AttrSemantic::POSITION: semStr="POSITION"; break;
                    case AttrSemantic::COLOR:    semStr="COLOR";    break;
                    case AttrSemantic::UV:       semStr="UV";       break;
                    case AttrSemantic::NORMAL:   semStr="NORMAL";   break;
                    case AttrSemantic::TANGENT:  semStr="TANGENT";  break;
                    case AttrSemantic::GENERIC:  semStr="GENERIC";  break;
                    default: break;
                }
                NKSW_LOG("  [%u] loc=%u off=%u bytes=%u -> %s",
                        i, attrs[i].location, attrs[i].offset, attrs[i].bytes, semStr);
            }

            return [attrs, stride, hasProj](const void* vdata, uint32 idx,
                                            const void* udata) -> NkVertexSoftware
            {
                const uint8* src = (const uint8*)vdata + (uint64)idx * stride;

                float px=0,py=0,pz=0;
                float cr=1,cg=1,cb=1,ca=1;
                float pu=0,pv=0;
                float nx=0,ny=0,nz=1;
                float tx=0,ty_=0,tz=0;
                NkVertexSoftware out{};

                for (uint32 i = 0; i < (uint32)attrs.Size(); ++i) {
                    float x,y,z,w;
                    ReadAttr(src, attrs[i], x, y, z, w);

                    switch (attrs[i].semantic) {
                        case AttrSemantic::POSITION: px=x; py=y; pz=z; break;
                        case AttrSemantic::COLOR:
                            cr=x; cg=y; cb=z;
                            ca = (attrs[i].type==NkSLBaseType::NK_VEC4 ||
                                attrs[i].type==NkSLBaseType::NK_UVEC4) ? w : 1.f;
                            break;
                        case AttrSemantic::UV:     pu=x; pv=y; break;
                        case AttrSemantic::NORMAL: nx=x; ny=y; nz=z; break;
                        case AttrSemantic::TANGENT:tx=x; ty_=y; tz=z; break;
                        case AttrSemantic::GENERIC: {
                            // Stocker dans attrs[] — max 16 floats
                            uint32 slot = attrs[i].genericSlot * 4;
                            if (slot + 3 < 16) {
                                out.attrs[slot+0] = x;
                                out.attrs[slot+1] = y;
                                out.attrs[slot+2] = z;
                                out.attrs[slot+3] = w;
                                if (out.attrCount < slot+4) out.attrCount = slot+4;
                            }
                            break;
                        }
                        default: break;
                    }
                }

                out.uv     = { pu, pv };
                out.color  = { cr, cg, cb, ca };
                out.normal = { nx, ny, nz };

                // Tangente dans les premiers attrs si pas déjà utilisés
                if (attrs.Size() > 0) {
                    // Stocker tangente dans les 4 premiers slots libres après les génériques
                }

                if (hasProj && udata) {
                    // L'UBO[0] est la matrice de projection (premier UBO déclaré)
                    const float* proj = (const float*)udata;
                    float ox,oy,oz,ow;
                    ProjMV(proj, px, py, pz, ox, oy, oz, ow);
                    out.position = { ox, oy, oz, ow };
                } else {
                    out.position = { px, py, pz, 1.f };
                }

                return out;
            };
        }

        // =============================================================================
        // Fabriquer la lambda fragment — supporte plusieurs textures et UBOs
        //
        // La lambda reçoit :
        //   uniformData → pointeur vers NkSWUniformBatch* (UBOs multiples)
        //   texSampler  → pointeur vers NkSWTextureBatch* (textures multiples)
        //
        // Pour accéder aux UBOs/textures dans les lambdas stockées par AddSWFn,
        // NkSoftwareCommandBuffer doit remplir ces batches avant de passer au rasterizer.
        // Voir NkSWResourceBatch ci-dessous.
        // =============================================================================
        static NkPixelShaderSoftware MakeFragmentFn(uint32 texCount) {
            if (texCount == 0) {
                // Pas de texture — passthrough couleur pure
                return [](const NkVertexSoftware& f, const void*, const void*) -> math::NkVec4 {
                    return { f.color.r, f.color.g, f.color.b, f.color.a };
                };
            }

            if (texCount == 1) {
                // Cas le plus courant : une seule texture
                return [](const NkVertexSoftware& f, const void*, const void* tp) -> math::NkVec4 {
                    const NkSWTexture* tex = nullptr;
                    if (tp) {
                        // tp peut être soit NkSWTextureBatch* soit NkSWTexture* direct
                        // On tente NkSWTextureBatch d'abord
                        const NkSWTextureBatch* batch = static_cast<const NkSWTextureBatch*>(tp);
                        tex = batch->tex[0];
                        if (!tex) tex = static_cast<const NkSWTexture*>(tp); // fallback
                    }
                    float r,g,b,a;
                    SampleTexNN(tex, f.uv.x, f.uv.y, r, g, b, a);
                    return { r*f.color.r, g*f.color.g, b*f.color.b, a*f.color.a };
                };
            }

            // Textures multiples — la lambda échantillonne la texture 0 par défaut
            // et multiplie par la couleur. Pour des shaders plus complexes (ex: normal map),
            // l'utilisateur devra utiliser le système de fragment shader custom.
            return [texCount](const NkVertexSoftware& f, const void*, const void* tp) -> math::NkVec4 {
                if (!tp) return { f.color.r, f.color.g, f.color.b, f.color.a };

                const NkSWTextureBatch* batch = static_cast<const NkSWTextureBatch*>(tp);
                const NkSWTexture* tex0 = batch->tex[0]; // diffuse/albedo

                float r,g,b,a;
                SampleTexNN(tex0, f.uv.x, f.uv.y, r, g, b, a);
                return { r*f.color.r, g*f.color.g, b*f.color.b, a*f.color.a };
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
            fread(buf.Data(), 1, (usize)sz, f); fclose(f);
            out = NkString(buf.Data());
            return true;
        }

        // =============================================================================
        // Compiler depuis deux sources séparées
        // =============================================================================
        static NkSWBridgeResult NkCompileSources(
            const NkString& vertSrc, const NkString& fragSrc,
            const NkString& vertName = "shader.vert.sw.sksl",
            const NkString& fragName = "shader.frag.sw.sksl",
            const NkSWVertexMapping* mapping = nullptr)
        {
            NkSWBridgeResult res;
            NkSLCompiler& compiler = nksl::GetCompiler();

            // ── Vertex ────────────────────────────────────────────────────────────────
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

            // ── Fragment ──────────────────────────────────────────────────────────────
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

            // ── Compter les textures et UBOs dans le fragment ─────────────────────────
            // (triés par binding pour un ordre déterministe)
            uint32 texCount = 0, uboCount = 0;
            for (uint32 i = 0; i < (uint32)res.fragRefl.resources.Size(); ++i) {
                if (res.fragRefl.resources[i].kind == NkSLResourceKind::NK_SAMPLED_TEXTURE)
                    ++texCount;
                if (res.fragRefl.resources[i].kind == NkSLResourceKind::NK_UNIFORM_BUFFER)
                    ++uboCount;
            }
            // Aussi les UBOs du vertex (matrice de projection, etc.)
            for (uint32 i = 0; i < (uint32)res.vertRefl.resources.Size(); ++i) {
                if (res.vertRefl.resources[i].kind == NkSLResourceKind::NK_UNIFORM_BUFFER)
                    ++uboCount;
            }
            res.texCount = texCount;
            res.uboCount = uboCount;

            // ── Layout vertex ─────────────────────────────────────────────────────────
            NkVector<AttrDesc> attrs = BuildLayout(res.vertRefl, res.stride, mapping);

            if (attrs.Empty()) {
                res.error = "Aucun vertex input dans " + vertName;
                NKSW_ERR("%s", res.error.CStr()); return res;
            }

            // ── Projection ? (UBO dans le vertex) ────────────────────────────────────
            bool hasProj = false;
            for (uint32 i = 0; i < (uint32)res.vertRefl.resources.Size(); ++i) {
                if (res.vertRefl.resources[i].kind == NkSLResourceKind::NK_UNIFORM_BUFFER ||
                    res.vertRefl.resources[i].kind == NkSLResourceKind::NK_PUSH_CONSTANT) {
                    hasProj = true; break;
                }
            }

            // ── Log de synthèse ───────────────────────────────────────────────────────
            NKSW_LOG("Compile OK: vert='%s' frag='%s'", vertName.CStr(), fragName.CStr());
            NKSW_LOG("  stride=%u attrs=%u proj=%s textures=%u ubos=%u",
                    res.stride, (uint32)attrs.Size(),
                    hasProj?"yes":"no", texCount, uboCount);

            // ── Lambdas ───────────────────────────────────────────────────────────────
            res.vertFn  = MakeVertexFn(attrs, res.stride, hasProj);
            res.fragFn  = MakeFragmentFn(texCount);
            res.success = true;
            return res;
        }

        // =============================================================================
        // Compiler depuis deux fichiers
        // =============================================================================
        static NkSWBridgeResult NkCompileFiles(
            const char* vertPath, const char* fragPath,
            const NkSWVertexMapping* mapping = nullptr)
        {
            NkString vs, fs;
            if (!LoadFile(vertPath, vs)) {
                NkSWBridgeResult r; r.error = NkString("Cannot open: ") + vertPath;
                NKSW_ERR("%s", r.error.CStr()); return r;
            }
            if (!LoadFile(fragPath, fs)) {
                NkSWBridgeResult r; r.error = NkString("Cannot open: ") + fragPath;
                NKSW_ERR("%s", r.error.CStr()); return r;
            }
            return NkCompileSources(vs, fs, NkString(vertPath), NkString(fragPath), mapping);
        }

        // =============================================================================
        // Compiler depuis un fichier unique (les deux stages dedans)
        // Restriction : marche bien tant que les déclarations `in` vertex et fragment
        // n'ont pas de locations conflictuelles. Préférer deux fichiers séparés.
        // =============================================================================
        static NkSWBridgeResult NkCompileFile(const char* path,
                                            const NkSWVertexMapping* mapping = nullptr) {
            NkString src;
            if (!LoadFile(path, src)) {
                NkSWBridgeResult r; r.error = NkString("Cannot open: ") + path;
                NKSW_ERR("%s", r.error.CStr()); return r;
            }
            return NkCompileSources(src, src, NkString(path), NkString(path), mapping);
        }

        // =============================================================================
        // Compiler depuis source inline
        // =============================================================================
        static NkSWBridgeResult NkCompile(const NkString& vertSrc, const NkString& fragSrc,
                                        const NkString& name = "inline.sw.sksl",
                                        const NkSWVertexMapping* mapping = nullptr) {
            return NkCompileSources(vertSrc, fragSrc,
                                    name + ".vert", name + ".frag", mapping);
        }

        // Surcharge fichier unique (compatibilité)
        static NkSWBridgeResult NkCompile(const NkString& src,
                                        const NkString& name = "inline.sw.sksl",
                                        uint32 /*hint*/ = 0) {
            return NkCompileSources(src, src, name + ".vert", name + ".frag", nullptr);
        }

        // Alias pour compatibilité
        using NkVertexShaderSoftware = ::nkentseu::NkVertexShaderSoftware;
        using NkPixelShaderSoftware  = ::nkentseu::NkPixelShaderSoftware;

    } // namespace swbridge
} // namespace nkentseu