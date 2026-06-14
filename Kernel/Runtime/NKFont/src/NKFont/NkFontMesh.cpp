// -----------------------------------------------------------------------------
// FICHIER: NKFont/NkFontMesh.cpp
// DESCRIPTION: Implémentation des méthodes de génération de mesh 3D pour NkFont.
// -----------------------------------------------------------------------------

#include "NKFont/NkFont.h"
#include "NKFont/Core/NkFontParser.h"
#include "NKFont/NkEarcut.h"
#include <math.h>

#include "NKLogger/NkLog.h"

namespace nkentseu {
    using namespace math;

    // ============================================================
    // HELPERS POUR LA GESTION DES TROUS
    // ============================================================

    // Calcule l'aire signée d'un contour 2D
    // > 0 = CCW (contour extérieur), < 0 = CW (trou)
    static float ComputeContourArea(const NkVector<NkVec2f>& contour) {
        float area = 0.f;
        size_t n = contour.Size();
        for (size_t i = 0; i < n; ++i) {
            const NkVec2f& p0 = contour[i];
            const NkVec2f& p1 = contour[(i + 1) % n];
            area += p0.x * p1.y - p0.y * p1.x;
        }
        return area * 0.5f;
    }

    // Test si un point est à l'intérieur d'un polygone (ray casting)
    static bool IsPointInPolygon(const NkVector<NkVec2f>& poly, const NkVec2f& point) {
        bool inside = false;
        size_t n = poly.Size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            const NkVec2f& pi = poly[i];
            const NkVec2f& pj = poly[j];
            
            if (((pi.y > point.y) != (pj.y > point.y)) &&
                (point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y + 1e-6f) + pi.x)) {
                inside = !inside;
            }
        }
        return inside;
    }

    // ============================================================
    // HELPERS — VERSION COLUMN-MAJOR (standard NkMat4f)
    // ============================================================

    static NkVec3f WorldTransform(const NkMat4f& m, float x, float y, float z) {
        // NkMat4f est column-major : data[col*4 + row]
        // Translation dans data[12], data[13], data[14] (col3, row0/1/2)
        return NkVec3f{
            m.data[0]*x + m.data[4]*y + m.data[8]*z + m.data[12],   // row 0
            m.data[1]*x + m.data[5]*y + m.data[9]*z + m.data[13],   // row 1
            m.data[2]*x + m.data[6]*y + m.data[10]*z + m.data[14]   // row 2
        };
    }

    // Calcule une normale 2D sortante pour une arête donnée
    static NkVec3f ComputeOutwardSideNormal(const NkVec2f& edgeStart, 
                                            const NkVec2f& edgeEnd, 
                                            bool isOuter) {
        // Direction de l'arête en 2D
        NkVec2f edge = NkVec2f(edgeEnd.x - edgeStart.x, edgeEnd.y - edgeStart.y).Normalized();
        
        // Normale 2D sortante : rotation de 90°
        // Pour CCW (extérieur) : normale = (edge.y, -edge.x) → pointe vers l'extérieur
        // Pour CW (trou) : normale = (-edge.y, edge.x) → pointe aussi vers l'extérieur du trou
        NkVec2f normal2D = isOuter 
            ? NkVec2f( edge.y, -edge.x) 
            : NkVec2f(-edge.y,  edge.x);
        
        return NkVec3f(normal2D.x, normal2D.y, 0.f).Normalized();
    }

    // ============================================================
    // GÉNÉRATION CORE — IMPLEMENTATION INTERNE
    // ============================================================

    static void GenerateGlyphMesh3DInternal(
        const NkFont* font,
        NkFontCodepoint cp,
        float scale,
        float extrusionDepth,
        const NkMat4f& worldMatrix,
        const NkVec4f& color,
        NkFont3DVertexCallback callback,
        void* userData)
    {
        if (!font || !callback) return;

        const NkFontGlyph* gl = font->FindGlyph(cp);
        if (!gl || !gl->visible) return;

        // Récupération des contours vectoriels du glyphe
        NkVector<NkFontOutlineVertex> outline;
        if (!font->GetGlyphOutlinePoints(cp, outline) || outline.Size() < 3) {
            // Fallback : aucun contour vectoriel, on génère quand même un simple quad
            // (cela ne devrait pas arriver pour une police TrueType standard)
            float x0 = gl->x0 * scale;
            float y0 = gl->y0 * scale;
            float x1 = gl->x1 * scale;
            float y1 = gl->y1 * scale;

            NkVec3f normFront = NkVec3f(0.f, 0.f, -1.f);
            NkVec3f normBack  = NkVec3f(0.f, 0.f,  1.f);

            NkVec3f f00 = WorldTransform(worldMatrix, x0, y0, 0.f);
            NkVec3f f10 = WorldTransform(worldMatrix, x1, y0, 0.f);
            NkVec3f f01 = WorldTransform(worldMatrix, x0, y1, 0.f);
            NkVec3f f11 = WorldTransform(worldMatrix, x1, y1, 0.f);
            
            NkVec3f b00 = WorldTransform(worldMatrix, x0, y0, extrusionDepth);
            NkVec3f b10 = WorldTransform(worldMatrix, x1, y0, extrusionDepth);
            NkVec3f b01 = WorldTransform(worldMatrix, x0, y1, extrusionDepth);
            NkVec3f b11 = WorldTransform(worldMatrix, x1, y1, extrusionDepth);

            // Face avant
            NkFontGlyph3DVertex front[6] = {
                {f00, normFront, {gl->u0, gl->v0}, color, (nkft_uint32)cp, 0},
                {f10, normFront, {gl->u1, gl->v0}, color, (nkft_uint32)cp, 0},
                {f11, normFront, {gl->u1, gl->v1}, color, (nkft_uint32)cp, 0},
                {f00, normFront, {gl->u0, gl->v0}, color, (nkft_uint32)cp, 0},
                {f11, normFront, {gl->u1, gl->v1}, color, (nkft_uint32)cp, 0},
                {f01, normFront, {gl->u0, gl->v1}, color, (nkft_uint32)cp, 0}
            };
            for (int i = 0; i < 6; ++i) callback(&front[i], 6, userData);

            // Face arrière
            NkFontGlyph3DVertex back[6] = {
                {b00, normBack, {gl->u0, gl->v0}, color, (nkft_uint32)cp, 1},
                {b11, normBack, {gl->u1, gl->v1}, color, (nkft_uint32)cp, 1},
                {b10, normBack, {gl->u1, gl->v0}, color, (nkft_uint32)cp, 1},
                {b00, normBack, {gl->u0, gl->v0}, color, (nkft_uint32)cp, 1},
                {b01, normBack, {gl->u0, gl->v1}, color, (nkft_uint32)cp, 1},
                {b11, normBack, {gl->u1, gl->v1}, color, (nkft_uint32)cp, 1}
            };
            for (int i = 0; i < 6; ++i) callback(&back[i], 6, userData);
            return;
        }

        // Découpage en contours individuels
        NkVector<NkVector<NkVec2f>> contours;
        NkVector<NkVec2f> current;
        for (size_t i = 0; i < outline.Size(); ++i) {
            current.PushBack({outline[i].x, outline[i].y});
            if (outline[i].isEndOfContour) {
                if (current.Size() >= 3) contours.PushBack(current);
                current.Clear();
            }
        }
        if (current.Size() >= 3) contours.PushBack(current);

        // Normales fixes pour faces avant/arrière
        NkVec3f normFront = NkVec3f(0.f, 0.f, -1.f);
        NkVec3f normBack  = NkVec3f(0.f, 0.f,  1.f);

        // Classifier les contours par profondeur d'imbrication.
        // profondeur paire (0, 2, …) = extérieur ; profondeur impaire (1, 3, …) = trou.
        // Cette approche est robuste : elle ne dépend pas du signe du winding TrueType.
        struct ContourInfo {
            NkVector<NkVec2f> contour;
            bool isOuter;
            float area;
        };
        NkVector<ContourInfo> contourInfos;
        for (size_t i = 0; i < contours.Size(); ++i) {
            int depth = 0;
            for (size_t j = 0; j < contours.Size(); ++j) {
                if (i != j && contours[j].Size() > 0 &&
                    IsPointInPolygon(contours[j], contours[i][0]))
                    ++depth;
            }
            float area = ComputeContourArea(contours[i]);
            contourInfos.PushBack({contours[i], (depth % 2 == 0), area});
        }

        // Normaliser le winding pour NkEarcut :
        //   extérieur → CCW (area > 0)   trou → CW (area < 0)
        for (size_t i = 0; i < contourInfos.Size(); ++i) {
            ContourInfo& ci = contourInfos[i];
            bool wantCCW = ci.isOuter;
            bool isCCW   = (ci.area > 0.f);
            if (isCCW != wantCCW) {
                NkVector<NkVec2f>& c = ci.contour;
                size_t n = c.Size();
                for (size_t l = 0, r = n - 1; l < r; ++l, --r) {
                    NkVec2f tmp = c[l]; c[l] = c[r]; c[r] = tmp;
                }
                ci.area = -ci.area;
            }
        }

        // Associer chaque trou à son contour extérieur parent
        struct ContourGroup {
            ContourInfo outer;
            NkVector<ContourInfo> holes;
        };
        NkVector<ContourGroup> groups;
        for (size_t i = 0; i < contourInfos.Size(); ++i) {
            if (!contourInfos[i].isOuter) continue;
            ContourGroup group;
            group.outer = contourInfos[i];
            for (size_t j = 0; j < contourInfos.Size(); ++j) {
                if (i == j || contourInfos[j].isOuter) continue;
                if (IsPointInPolygon(group.outer.contour, contourInfos[j].contour[0]))
                    group.holes.PushBack(contourInfos[j]);
            }
            groups.PushBack(group);
        }

        // Pour chaque groupe : tableau plat outer+trous → NkEarcut → faces avant/arrière/latérales
        for (size_t g = 0; g < groups.Size(); ++g) {
            const ContourGroup& group = groups[g];
            size_t outerN = group.outer.contour.Size();

            // Tableau plat : outer d'abord, puis chaque trou dans l'ordre
            NkVector<NkVec3f> frontPts, backPts;
            NkVector<NkVec2f> uvs;

            for (size_t i = 0; i < outerN; ++i) {
                const NkVec2f& pt = group.outer.contour[i];
                float lx = pt.x * scale, ly = pt.y * scale;
                frontPts.PushBack(WorldTransform(worldMatrix, lx, ly, 0.f));
                backPts.PushBack(WorldTransform(worldMatrix, lx, ly, extrusionDepth));
                float u = (pt.x - gl->x0) / (gl->x1 - gl->x0 + 1e-6f);
                float v = (pt.y - gl->y0) / (gl->y1 - gl->y0 + 1e-6f);
                uvs.PushBack({math::NkClamp(u, 0.f, 1.f), math::NkClamp(v, 0.f, 1.f)});
            }

            NkVector<size_t> holeOffsets;
            size_t flatOffset = outerN;
            for (size_t h = 0; h < group.holes.Size(); ++h) {
                holeOffsets.PushBack(flatOffset);
                size_t hn = group.holes[h].contour.Size();
                for (size_t i = 0; i < hn; ++i) {
                    const NkVec2f& pt = group.holes[h].contour[i];
                    float lx = pt.x * scale, ly = pt.y * scale;
                    frontPts.PushBack(WorldTransform(worldMatrix, lx, ly, 0.f));
                    backPts.PushBack(WorldTransform(worldMatrix, lx, ly, extrusionDepth));
                    float u = (pt.x - gl->x0) / (gl->x1 - gl->x0 + 1e-6f);
                    float v = (pt.y - gl->y0) / (gl->y1 - gl->y0 + 1e-6f);
                    uvs.PushBack({math::NkClamp(u, 0.f, 1.f), math::NkClamp(v, 0.f, 1.f)});
                }
                flatOffset += hn;
            }

            // Triangulation avec trous via NkEarcut (indices globaux dans le tableau plat)
            NkVector<NkVector<NkVec2f>> polygon;
            polygon.PushBack(group.outer.contour);
            for (size_t h = 0; h < group.holes.Size(); ++h)
                polygon.PushBack(group.holes[h].contour);

            NkVector<std::size_t> triIdx = NkEarcut<float>(polygon);

            // Faces avant (winding CCW → normale -Z)
            for (size_t t = 0; t + 2 < triIdx.Size(); t += 3) {
                size_t i0 = triIdx[t], i1 = triIdx[t+1], i2 = triIdx[t+2];
                NkFontGlyph3DVertex tri[3] = {
                    {frontPts[i0], normFront, uvs[i0], color, (nkft_uint32)cp, 0},
                    {frontPts[i1], normFront, uvs[i1], color, (nkft_uint32)cp, 0},
                    {frontPts[i2], normFront, uvs[i2], color, (nkft_uint32)cp, 0}
                };
                callback(tri, 3, userData);
            }

            // Faces arrière (winding inversé → normale +Z)
            for (size_t t = 0; t + 2 < triIdx.Size(); t += 3) {
                size_t i0 = triIdx[t], i1 = triIdx[t+1], i2 = triIdx[t+2];
                NkFontGlyph3DVertex tri[3] = {
                    {backPts[i0], normBack, uvs[i0], color, (nkft_uint32)cp, 1},
                    {backPts[i2], normBack, uvs[i2], color, (nkft_uint32)cp, 1},
                    {backPts[i1], normBack, uvs[i1], color, (nkft_uint32)cp, 1}
                };
                callback(tri, 3, userData);
            }

            // Faces latérales du contour extérieur (CCW → normale droite = sortante)
            for (size_t i = 0; i < outerN; ++i) {
                size_t j = (i + 1) % outerN;
                NkVec3f sn = ComputeOutwardSideNormal(
                    group.outer.contour[i], group.outer.contour[j], true);
                NkVec3f wn{
                    worldMatrix.data[0]*sn.x + worldMatrix.data[4]*sn.y + worldMatrix.data[8]*sn.z,
                    worldMatrix.data[1]*sn.x + worldMatrix.data[5]*sn.y + worldMatrix.data[9]*sn.z,
                    worldMatrix.data[2]*sn.x + worldMatrix.data[6]*sn.y + worldMatrix.data[10]*sn.z
                };
                wn = wn.Normalized();
                NkFontGlyph3DVertex quad[6] = {
                    {frontPts[i], wn, uvs[i], color, (nkft_uint32)cp, 2},
                    {backPts[i],  wn, uvs[i], color, (nkft_uint32)cp, 2},
                    {backPts[j],  wn, uvs[j], color, (nkft_uint32)cp, 2},
                    {frontPts[i], wn, uvs[i], color, (nkft_uint32)cp, 2},
                    {backPts[j],  wn, uvs[j], color, (nkft_uint32)cp, 2},
                    {frontPts[j], wn, uvs[j], color, (nkft_uint32)cp, 2}
                };
                callback(quad, 6, userData);
            }

            // Faces latérales des trous (CW → normale droite pointe vers centre du trou)
            for (size_t h = 0; h < group.holes.Size(); ++h) {
                size_t ho = holeOffsets[h];
                size_t hn = group.holes[h].contour.Size();
                for (size_t i = 0; i < hn; ++i) {
                    size_t j  = (i + 1) % hn;
                    size_t gi = ho + i;
                    size_t gj = ho + j;
                    NkVec3f sn = ComputeOutwardSideNormal(
                        group.holes[h].contour[i], group.holes[h].contour[j], true);
                    NkVec3f wn{
                        worldMatrix.data[0]*sn.x + worldMatrix.data[4]*sn.y + worldMatrix.data[8]*sn.z,
                        worldMatrix.data[1]*sn.x + worldMatrix.data[5]*sn.y + worldMatrix.data[9]*sn.z,
                        worldMatrix.data[2]*sn.x + worldMatrix.data[6]*sn.y + worldMatrix.data[10]*sn.z
                    };
                    wn = wn.Normalized();
                    NkFontGlyph3DVertex quad[6] = {
                        {frontPts[gi], wn, uvs[gi], color, (nkft_uint32)cp, 2},
                        {backPts[gi],  wn, uvs[gi], color, (nkft_uint32)cp, 2},
                        {backPts[gj],  wn, uvs[gj], color, (nkft_uint32)cp, 2},
                        {frontPts[gi], wn, uvs[gi], color, (nkft_uint32)cp, 2},
                        {backPts[gj],  wn, uvs[gj], color, (nkft_uint32)cp, 2},
                        {frontPts[gj], wn, uvs[gj], color, (nkft_uint32)cp, 2}
                    };
                    callback(quad, 6, userData);
                }
            }
        }
    }

    // ============================================================
    // API PUBLIQUE — IMPLEMENTATION
    // ============================================================

    NkFontGlyphMesh3D NkFont::GenerateGlyphMesh3D(
        NkFontCodepoint cp,
        float scale,
        float extrusionDepth,
        const NkMat4f& worldMatrix,
        const NkVec4f& color) const
    {
        NkFontGlyphMesh3D mesh;
        mesh.advanceX = GetCharAdvance(cp) * scale;
        mesh.ascent = ascent;
        mesh.descent = descent;

        auto collector = [](const NkFontGlyph3DVertex* v, usize count, void* userData) {
            auto* m = static_cast<NkFontGlyphMesh3D*>(userData);
            for (usize i = 0; i < count; ++i) {
                m->vertices.PushBack(v[i]);
            }
        };

        GenerateGlyphMesh3DInternal(this, cp, scale, extrusionDepth, worldMatrix, color, collector, &mesh);
        return mesh;
    }

    NkFontGlyphMesh3D NkFont::GenerateTextMesh3D(
        const char* text,
        float scale,
        float extrusionDepth,
        const NkMat4f& worldMatrix,
        const NkVec4f& color) const
    {
        NkFontGlyphMesh3D mesh;
        float curX = 0.f;
        const char* p = text;

        while (*p) {
            NkFontCodepoint cp = NkFont::DecodeUTF8(&p, nullptr);
            if (cp == '\n') { curX = 0.f; continue; }

            const NkFontGlyph* gl = FindGlyph(cp);
            if (!gl || !gl->visible) { curX += GetCharAdvance(cp) * scale; continue; }

            // Matrice monde translatée pour ce glyphe
            NkMat4f glyphWorld = worldMatrix * NkMat4f::Translation(NkVec3f(curX, 0.f, 0.f));
            
            // Générer et concaténer le mesh du glyphe
            NkFontGlyphMesh3D glyphMesh = GenerateGlyphMesh3D(cp, scale, extrusionDepth, glyphWorld, color);
            for (const auto& v : glyphMesh.vertices) mesh.vertices.PushBack(v);
            
            curX += glyphMesh.advanceX;
        }
        return mesh;
    }

    void NkFont::ForEachGlyph3DVertex(
        NkFontCodepoint cp,
        float scale,
        float extrusionDepth,
        const NkMat4f& worldMatrix,
        const NkVec4f& color,
        NkFont3DVertexCallback callback,
        void* userData) const
    {
        GenerateGlyphMesh3DInternal(this, cp, scale, extrusionDepth, worldMatrix, color, callback, userData);
    }

    void NkFont::ForEachText3DVertex(
        const char* text,
        float scale,
        float extrusionDepth,
        const NkMat4f& worldMatrix,
        const NkVec4f& color,
        NkFont3DVertexCallback callback,
        void* userData) const
    {
        float curX = 0.f;
        const char* p = text;

        while (*p) {
            NkFontCodepoint cp = NkFont::DecodeUTF8(&p, nullptr);
            if (cp == '\n') { curX = 0.f; continue; }

            const NkFontGlyph* gl = FindGlyph(cp);
            if (!gl || !gl->visible) { curX += GetCharAdvance(cp) * scale; continue; }

            NkMat4f glyphWorld = worldMatrix * NkMat4f::Translation(NkVec3f(curX, 0.f, 0.f));
            GenerateGlyphMesh3DInternal(this, cp, scale, extrusionDepth, glyphWorld, color, callback, userData);
            
            curX += GetCharAdvance(cp) * scale;
        }
    }

} // namespace nkentseu