#pragma once
// =============================================================================
// NkMeshLoaderUtil.h — NKRenderer (Mesh/)
//
// Helpers partages par les loaders de maillage from-scratch (OBJ/STL/PLY/FBX).
// Zero-STL : parsing de nombres a la main sur curseur const char* (PAS d'atof/
// strtod), packing couleur, calcul d'englobant et de normales lisses.
//
// Format CPU commun : NkGLTFMeshData (cf. NkGLTFLoader.h) sert d'echange pour
// TOUS les formats (vertices interleaves NkVertex3D + indices + sous-meshes +
// materiaux/images). Les champs glTF-specifiques (skinning/nodes/animations)
// restent vides pour les formats statiques.
//
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include <cmath>

namespace nkentseu {
    namespace renderer {
        namespace meshutil {

            // RGBA8 packe (a<<24|b<<16|g<<8|r) — meme convention que NkMeshSystem.
            inline uint32 PackRGBA(uint8 r, uint8 g, uint8 b, uint8 a = 255) {
                return ((uint32)a << 24) | ((uint32)b << 16) | ((uint32)g << 8) | (uint32)r;
            }

            // ── Parsing curseur (zero-STL) ───────────────────────────────────
            inline bool IsWS(char c)   { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
            inline bool IsHSpace(char c){ return c == ' ' || c == '\t' || c == '\r'; }

            inline void SkipWS(const char*& p, const char* end) {
                while (p < end && IsWS(*p)) ++p;
            }
            inline void SkipHSpace(const char*& p, const char* end) {
                while (p < end && IsHSpace(*p)) ++p;
            }
            inline void SkipToEOL(const char*& p, const char* end) {
                while (p < end && *p != '\n') ++p;
            }

            // Parse un float (signe, partie entiere, fraction, exposant e/E).
            // Saute les espaces horizontaux en tete. Avance p. 0 si rien.
            inline float64 ParseDouble(const char*& p, const char* end) {
                SkipHSpace(p, end);
                bool neg = false;
                if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); ++p; }
                float64 val = 0.0;
                while (p < end && *p >= '0' && *p <= '9') { val = val * 10.0 + (float64)(*p - '0'); ++p; }
                if (p < end && *p == '.') {
                    ++p; float64 f = 0.1;
                    while (p < end && *p >= '0' && *p <= '9') { val += (float64)(*p - '0') * f; f *= 0.1; ++p; }
                }
                if (p < end && (*p == 'e' || *p == 'E')) {
                    ++p; bool eneg = false;
                    if (p < end && (*p == '+' || *p == '-')) { eneg = (*p == '-'); ++p; }
                    int32 e = 0;
                    while (p < end && *p >= '0' && *p <= '9') { e = e * 10 + (int32)(*p - '0'); ++p; }
                    float64 m = 1.0; for (int32 i = 0; i < e; ++i) m *= 10.0;
                    if (eneg) val /= m; else val *= m;
                }
                return neg ? -val : val;
            }
            inline float32 ParseFloat(const char*& p, const char* end) {
                return (float32)ParseDouble(p, end);
            }
            inline int64 ParseInt64(const char*& p, const char* end) {
                SkipHSpace(p, end);
                bool neg = false;
                if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); ++p; }
                int64 val = 0;
                while (p < end && *p >= '0' && *p <= '9') { val = val * 10 + (int64)(*p - '0'); ++p; }
                return neg ? -val : val;
            }
            inline int32 ParseInt(const char*& p, const char* end) {
                return (int32)ParseInt64(p, end);
            }

            // Variantes "free-form" : sautent TOUT blanc (y compris '\n') avant la
            // valeur. Pour les formats ou les nombres traversent les lignes (PLY
            // ascii). NE PAS utiliser pour OBJ (oriente ligne, faces sur 1 ligne).
            inline float64 ParseDoubleWS(const char*& p, const char* end) {
                SkipWS(p, end); return ParseDouble(p, end);
            }
            inline int64 ParseInt64WS(const char*& p, const char* end) {
                SkipWS(p, end); return ParseInt64(p, end);
            }

            // Lit un "mot" (jusqu'au prochain whitespace) dans out. Avance p.
            inline void ReadToken(const char*& p, const char* end, NkString& out) {
                SkipHSpace(p, end);
                const char* s = p;
                while (p < end && !IsWS(*p)) ++p;
                out.Clear();
                if (p > s) out.Append(s, (NkString::SizeType)(p - s));
            }

            // ── Lecture binaire little-endian ────────────────────────────────
            inline uint16 ReadU16LE(const uint8* d) { return (uint16)(d[0] | (d[1] << 8)); }
            inline uint32 ReadU32LE(const uint8* d) {
                return (uint32)(d[0] | (d[1] << 8) | (d[2] << 16) | ((uint32)d[3] << 24));
            }
            inline float32 ReadF32LE(const uint8* d) {
                union { uint32 u; float32 f; } c; c.u = ReadU32LE(d); return c.f;
            }
            inline uint64 ReadU64LE(const uint8* d) {
                return (uint64)ReadU32LE(d) | ((uint64)ReadU32LE(d + 4) << 32);
            }
            inline float64 ReadF64LE(const uint8* d) {
                union { uint64 u; float64 f; } c; c.u = ReadU64LE(d); return c.f;
            }

            // ── Englobant global + par sous-mesh ─────────────────────────────
            inline void ComputeBounds(NkGLTFMeshData& m) {
                if (m.vertices.Empty()) return;
                NkVec3f mn = m.vertices[0].pos, mx = m.vertices[0].pos;
                for (uint32 i = 1; i < (uint32)m.vertices.Size(); ++i) {
                    const NkVec3f& p = m.vertices[i].pos;
                    if (p.x < mn.x) mn.x = p.x; if (p.y < mn.y) mn.y = p.y; if (p.z < mn.z) mn.z = p.z;
                    if (p.x > mx.x) mx.x = p.x; if (p.y > mx.y) mx.y = p.y; if (p.z > mx.z) mx.z = p.z;
                }
                m.bounds.min = mn; m.bounds.max = mx;

                for (uint32 s = 0; s < (uint32)m.subMeshes.Size(); ++s) {
                    NkSubMesh& sm = m.subMeshes[s];
                    if (sm.indexCount == 0) { sm.bounds = m.bounds; continue; }
                    bool first = true; NkVec3f smn{}, smx{};
                    for (uint32 ii = 0; ii < sm.indexCount; ++ii) {
                        uint32 vi = m.indices[sm.firstIndex + ii] + sm.baseVertex;
                        if (vi >= (uint32)m.vertices.Size()) continue;
                        const NkVec3f& p = m.vertices[vi].pos;
                        if (first) { smn = smx = p; first = false; continue; }
                        if (p.x < smn.x) smn.x = p.x; if (p.y < smn.y) smn.y = p.y; if (p.z < smn.z) smn.z = p.z;
                        if (p.x > smx.x) smx.x = p.x; if (p.y > smx.y) smx.y = p.y; if (p.z > smx.z) smx.z = p.z;
                    }
                    sm.bounds.min = smn; sm.bounds.max = smx;
                }
            }

            // Recalcule des normales LISSES par accumulation de normales de face
            // (utilise quand le format ne fournit pas de normales). Triangles
            // uniquement (indices par 3). Remet les normales a 0 avant accumulation.
            inline void GenerateSmoothNormals(NkGLTFMeshData& m) {
                for (uint32 i = 0; i < (uint32)m.vertices.Size(); ++i)
                    m.vertices[i].normal = {0.f, 0.f, 0.f};
                for (uint32 i = 0; i + 2 < (uint32)m.indices.Size(); i += 3) {
                    uint32 i0 = m.indices[i + 0], i1 = m.indices[i + 1], i2 = m.indices[i + 2];
                    if (i0 >= (uint32)m.vertices.Size() || i1 >= (uint32)m.vertices.Size()
                        || i2 >= (uint32)m.vertices.Size()) continue;
                    NkVec3f p0 = m.vertices[i0].pos, p1 = m.vertices[i1].pos, p2 = m.vertices[i2].pos;
                    NkVec3f e1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
                    NkVec3f e2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
                    NkVec3f n  = {e1.y * e2.z - e1.z * e2.y,
                                  e1.z * e2.x - e1.x * e2.z,
                                  e1.x * e2.y - e1.y * e2.x};
                    m.vertices[i0].normal = {m.vertices[i0].normal.x + n.x, m.vertices[i0].normal.y + n.y, m.vertices[i0].normal.z + n.z};
                    m.vertices[i1].normal = {m.vertices[i1].normal.x + n.x, m.vertices[i1].normal.y + n.y, m.vertices[i1].normal.z + n.z};
                    m.vertices[i2].normal = {m.vertices[i2].normal.x + n.x, m.vertices[i2].normal.y + n.y, m.vertices[i2].normal.z + n.z};
                }
                for (uint32 i = 0; i < (uint32)m.vertices.Size(); ++i) {
                    NkVec3f& n = m.vertices[i].normal;
                    float32 len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                    if (len > 1e-8f) { n.x /= len; n.y /= len; n.z /= len; }
                    else             { n = {0.f, 1.f, 0.f}; }
                }
            }

        } // namespace meshutil
    } // namespace renderer
} // namespace nkentseu
