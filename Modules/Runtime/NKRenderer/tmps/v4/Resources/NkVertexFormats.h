#pragma once
// =============================================================================
// NkVertexFormats.h — Formats de vertex 2D / 3D pour NKRenderer
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Format d'attribut vertex
        // =============================================================================
        enum class NkAttribFmt : uint32 {
            NK_FLOAT,  NK_FLOAT2, NK_FLOAT3, NK_FLOAT4,
            NK_UINT,   NK_UINT2,  NK_UINT4,
            NK_UBYTE4N,    // RGBA [0-255] → [0-1]  (couleur vertex packée)
            NK_SHORT2N,    // 2 shorts normalisés
            NK_HALF2,  NK_HALF4,
            NK_SINT,   NK_SINT2,  NK_SINT4,
        };

        inline uint32 NkAttribFmtSize(NkAttribFmt f) {
            switch (f) {
                case NkAttribFmt::NK_FLOAT:   return 4;
                case NkAttribFmt::NK_FLOAT2:  return 8;
                case NkAttribFmt::NK_FLOAT3:  return 12;
                case NkAttribFmt::NK_FLOAT4:  return 16;
                case NkAttribFmt::NK_UINT:    return 4;
                case NkAttribFmt::NK_UINT2:   return 8;
                case NkAttribFmt::NK_UINT4:   return 16;
                case NkAttribFmt::NK_UBYTE4N: return 4;
                case NkAttribFmt::NK_SHORT2N: return 4;
                case NkAttribFmt::NK_HALF2:   return 4;
                case NkAttribFmt::NK_HALF4:   return 8;
                case NkAttribFmt::NK_SINT:    return 4;
                case NkAttribFmt::NK_SINT2:   return 8;
                case NkAttribFmt::NK_SINT4:   return 16;
                default:                      return 0;
            }
        }

        // =============================================================================
        // Descripteurs de layout vertex
        // =============================================================================
        struct NkVertexAttrib {
            uint32      location     = 0;
            uint32      binding      = 0;
            NkAttribFmt format       = NkAttribFmt::NK_FLOAT3;
            uint32      offset       = 0;
            const char* semanticName = nullptr;  // DX: "POSITION", "NORMAL"...
            uint32      semanticIdx  = 0;
        };

        struct NkVertexBinding {
            uint32 binding     = 0;
            uint32 stride      = 0;
            bool   perInstance = false;
        };

        struct NkVertexLayout {
            NkVector<NkVertexAttrib>  attribs;
            NkVector<NkVertexBinding> bindings;

            NkVertexLayout& Attrib(uint32 loc, uint32 bind, NkAttribFmt fmt,
                                    uint32 off, const char* sem = nullptr,
                                    uint32 semIdx = 0) {
                attribs.PushBack({loc, bind, fmt, off, sem, semIdx});
                return *this;
            }
            NkVertexLayout& Bind(uint32 bind, uint32 stride, bool instanced = false) {
                bindings.PushBack({bind, stride, instanced});
                return *this;
            }
            bool IsEmpty() const { return attribs.IsEmpty(); }
        };

        // =============================================================================
        // Helpers pour packer/unpacker les couleurs vertex
        // =============================================================================
        inline uint32 NkPackRGBA(const NkColorF& c) {
            uint32 r = (uint32)(c.r * 255.f + .5f) & 0xFF;
            uint32 g = (uint32)(c.g * 255.f + .5f) & 0xFF;
            uint32 b = (uint32)(c.b * 255.f + .5f) & 0xFF;
            uint32 a = (uint32)(c.a * 255.f + .5f) & 0xFF;
            return (a << 24) | (b << 16) | (g << 8) | r;
        }
        inline NkColorF NkUnpackRGBA(uint32 c) {
            return {
                float((c >> 0 ) & 0xFF) / 255.f,
                float((c >> 8 ) & 0xFF) / 255.f,
                float((c >> 16) & 0xFF) / 255.f,
                float((c >> 24) & 0xFF) / 255.f,
            };
        }

        // =============================================================================
        // NkVertex2D — sprite / shape / UI
        // pos(2f) + uv(2f) + color(uint) = 20 bytes
        // =============================================================================
        struct alignas(4) NkVertex2D {
            float32 x = 0, y = 0;
            float32 u = -1.f, v = -1.f;   // uv < 0 → pas de texture
            uint32  color = 0xFFFFFFFF;

            NkVertex2D() = default;
            NkVertex2D(float32 x, float32 y, NkColorF c = NkColorF::White())
                : x(x), y(y), u(-1.f), v(-1.f), color(NkPackRGBA(c)) {}
            NkVertex2D(float32 x, float32 y, float32 u, float32 v,
                        NkColorF c = NkColorF::White())
                : x(x), y(y), u(u), v(v), color(NkPackRGBA(c)) {}

            static NkVertexLayout Layout() {
                NkVertexLayout l;
                l.Attrib(0, 0, NkAttribFmt::NK_FLOAT2,  0,  "POSITION", 0)
                 .Attrib(1, 0, NkAttribFmt::NK_FLOAT2,  8,  "TEXCOORD", 0)
                 .Attrib(2, 0, NkAttribFmt::NK_UBYTE4N, 16, "COLOR",    0)
                 .Bind(0, sizeof(NkVertex2D));
                return l;
            }
        };
        static_assert(sizeof(NkVertex2D) == 20, "NkVertex2D size mismatch");

        // =============================================================================
        // NkVertex3D — rendu 3D opaque/transparent
        // pos(3f)+normal(3f)+tangent(3f)+uv(2f)+color(uint) = 48 bytes
        // =============================================================================
        struct alignas(4) NkVertex3D {
            float32 px = 0, py = 0, pz = 0;
            float32 nx = 0, ny = 1, nz = 0;
            float32 tx = 1, ty = 0, tz = 0;
            float32 u  = 0, v  = 0;
            uint32  color = 0xFFFFFFFF;

            NkVertex3D() = default;
            NkVertex3D(float32 px, float32 py, float32 pz,
                        float32 nx, float32 ny, float32 nz,
                        float32 u, float32 v,
                        NkColorF c = NkColorF::White())
                : px(px), py(py), pz(pz)
                , nx(nx), ny(ny), nz(nz)
                , tx(1), ty(0), tz(0)
                , u(u), v(v)
                , color(NkPackRGBA(c)) {}

            NkVec3f Pos()     const { return {px, py, pz}; }
            NkVec3f Normal()  const { return {nx, ny, nz}; }
            NkVec3f Tangent() const { return {tx, ty, tz}; }
            NkVec2f UV()      const { return {u, v}; }

            static NkVertexLayout Layout() {
                NkVertexLayout l;
                l.Attrib(0, 0, NkAttribFmt::NK_FLOAT3,  0,  "POSITION", 0)
                 .Attrib(1, 0, NkAttribFmt::NK_FLOAT3,  12, "NORMAL",   0)
                 .Attrib(2, 0, NkAttribFmt::NK_FLOAT3,  24, "TANGENT",  0)
                 .Attrib(3, 0, NkAttribFmt::NK_FLOAT2,  36, "TEXCOORD", 0)
                 .Attrib(4, 0, NkAttribFmt::NK_UBYTE4N, 44, "COLOR",    0)
                 .Bind(0, sizeof(NkVertex3D));
                return l;
            }
        };
        static_assert(sizeof(NkVertex3D) == 48, "NkVertex3D size mismatch");

        // =============================================================================
        // NkVertexSkinned — animation squelettique
        // pos(3f)+normal(3f)+tangent(3f)+uv(2f)+color(uint)+boneIdx(4u8)+boneW(4f)
        // = 68 bytes
        // =============================================================================
        struct alignas(4) NkVertexSkinned {
            float32 px = 0, py = 0, pz = 0;
            float32 nx = 0, ny = 1, nz = 0;
            float32 tx = 1, ty = 0, tz = 0;
            float32 u  = 0, v  = 0;
            uint32  color    = 0xFFFFFFFF;
            uint8   boneIdx[4]    = {0, 0, 0, 0};
            float32 boneWeight[4] = {1, 0, 0, 0};

            static NkVertexLayout Layout() {
                NkVertexLayout l;
                l.Attrib(0, 0, NkAttribFmt::NK_FLOAT3,  0,  "POSITION",     0)
                 .Attrib(1, 0, NkAttribFmt::NK_FLOAT3,  12, "NORMAL",       0)
                 .Attrib(2, 0, NkAttribFmt::NK_FLOAT3,  24, "TANGENT",      0)
                 .Attrib(3, 0, NkAttribFmt::NK_FLOAT2,  36, "TEXCOORD",     0)
                 .Attrib(4, 0, NkAttribFmt::NK_UBYTE4N, 44, "COLOR",        0)
                 .Attrib(5, 0, NkAttribFmt::NK_UINT4,   48, "BLENDINDICES", 0)
                 .Attrib(6, 0, NkAttribFmt::NK_FLOAT4,  52, "BLENDWEIGHTS", 0)
                 .Bind(0, sizeof(NkVertexSkinned));
                return l;
            }
        };
        static_assert(sizeof(NkVertexSkinned) == 68, "NkVertexSkinned size mismatch");

        // =============================================================================
        // NkVertexPosOnly — depth/shadow pass (12 bytes)
        // =============================================================================
        struct alignas(4) NkVertexPosOnly {
            float32 px = 0, py = 0, pz = 0;

            NkVertexPosOnly() = default;
            NkVertexPosOnly(float32 x, float32 y, float32 z) : px(x), py(y), pz(z) {}
            explicit NkVertexPosOnly(NkVec3f p) : px(p.x), py(p.y), pz(p.z) {}

            static NkVertexLayout Layout() {
                NkVertexLayout l;
                l.Attrib(0, 0, NkAttribFmt::NK_FLOAT3, 0, "POSITION", 0)
                 .Bind(0, 12);
                return l;
            }
        };
        static_assert(sizeof(NkVertexPosOnly) == 12, "NkVertexPosOnly size mismatch");

        // =============================================================================
        // NkVertexDebug — lignes/axes/wireframe (16 bytes)
        // =============================================================================
        struct alignas(4) NkVertexDebug {
            float32 px = 0, py = 0, pz = 0;
            uint32  color = 0xFFFFFFFF;

            NkVertexDebug() = default;
            NkVertexDebug(float32 x, float32 y, float32 z,
                           NkColorF c = NkColorF::White())
                : px(x), py(y), pz(z), color(NkPackRGBA(c)) {}
            NkVertexDebug(NkVec3f p, NkColorF c = NkColorF::White())
                : px(p.x), py(p.y), pz(p.z), color(NkPackRGBA(c)) {}

            static NkVertexLayout Layout() {
                NkVertexLayout l;
                l.Attrib(0, 0, NkAttribFmt::NK_FLOAT3,  0,  "POSITION", 0)
                 .Attrib(1, 0, NkAttribFmt::NK_UBYTE4N, 12, "COLOR",    0)
                 .Bind(0, 16);
                return l;
            }
        };
        static_assert(sizeof(NkVertexDebug) == 16, "NkVertexDebug size mismatch");

        // =============================================================================
        // NkVertexParticle — billboard (32 bytes)
        // =============================================================================
        struct alignas(4) NkVertexParticle {
            float32 px = 0, py = 0, pz = 0;
            float32 u  = 0, v  = 0;
            uint32  color    = 0xFFFFFFFF;
            float32 size     = 1.f;
            float32 rotation = 0.f;

            static NkVertexLayout Layout() {
                NkVertexLayout l;
                l.Attrib(0, 0, NkAttribFmt::NK_FLOAT3,  0,  "POSITION", 0)
                 .Attrib(1, 0, NkAttribFmt::NK_FLOAT2,  12, "TEXCOORD", 0)
                 .Attrib(2, 0, NkAttribFmt::NK_UBYTE4N, 20, "COLOR",    0)
                 .Attrib(3, 0, NkAttribFmt::NK_FLOAT2,  24, "TEXCOORD", 1) // size+rot
                 .Bind(0, sizeof(NkVertexParticle));
                return l;
            }
        };
        static_assert(sizeof(NkVertexParticle) == 32, "NkVertexParticle size mismatch");

    } // namespace renderer
} // namespace nkentseu
