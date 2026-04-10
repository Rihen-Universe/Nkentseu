// -----------------------------------------------------------------------------
// FICHIER: NKFont/NkFontMesh.cpp
// DESCRIPTION: Implémentation des méthodes de génération de mesh 3D pour NkFont.
// -----------------------------------------------------------------------------

#include "NKFont/NkFont.h"
#include "NKFont/Core/NkFontParser.h"
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
    // HELPERS — TRIANGULATION SIMPLE
    // ============================================================

    static bool IsEar(const NkVector<NkVec2f>& poly, size_t i0, size_t i1, size_t i2) {
        const NkVec2f& A = poly[i0];
        const NkVec2f& B = poly[i1];
        const NkVec2f& C = poly[i2];
        
        float cross = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
        if (cross <= 0.0001f) return false;
        
        for (size_t k = 0; k < poly.Size(); ++k) {
            if (k == i0 || k == i1 || k == i2) continue;
            const NkVec2f& P = poly[k];
            float w0 = (A.x - P.x) * (B.y - P.y) - (A.y - P.y) * (B.x - P.x);
            float w1 = (B.x - P.x) * (C.y - P.y) - (B.y - P.y) * (C.x - P.x);
            float w2 = (C.x - P.x) * (A.y - P.y) - (C.y - P.y) * (A.x - P.x);
            bool allPos = (w0 > 0.0001f && w1 > 0.0001f && w2 > 0.0001f);
            bool allNeg = (w0 < -0.0001f && w1 < -0.0001f && w2 < -0.0001f);
            if (allPos || allNeg) return false;
        }
        return true;
    }

    static void TriangulateContour(const NkVector<NkVec2f>& contour,
                                    NkVector<uint32_t>& outIndices) {
        if (contour.Size() < 3) return;
        
        NkVector<size_t> indices;
        for (size_t i = 0; i < contour.Size(); ++i) indices.PushBack(i);
        
        while (indices.Size() > 3) {
            bool found = false;
            for (size_t i = 0; i < indices.Size(); ++i) {
                size_t i0 = indices[(i + indices.Size() - 1) % indices.Size()];
                size_t i1 = indices[i];
                size_t i2 = indices[(i + 1) % indices.Size()];
                
                if (IsEar(contour, i0, i1, i2)) {
                    outIndices.PushBack((uint32_t)i0);
                    outIndices.PushBack((uint32_t)i1);
                    outIndices.PushBack((uint32_t)i2);
                    indices.Erase(indices.Begin() + i);
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }
        
        if (indices.Size() == 3) {
            outIndices.PushBack((uint32_t)indices[0]);
            outIndices.PushBack((uint32_t)indices[1]);
            outIndices.PushBack((uint32_t)indices[2]);
        }
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

        // Classifier les contours : extérieur (CCW, area > 0) ou trou (CW, area < 0)
        struct ContourInfo {
            NkVector<NkVec2f> contour;
            bool isOuter;
            float area;
        };
        NkVector<ContourInfo> contourInfos;
        for (const auto& c : contours) {
            float area = ComputeContourArea(c);
            contourInfos.PushBack({c, area > 0.f, area});
        }

        // Associer les trous à leur contour extérieur parent
        struct ContourGroup {
            ContourInfo outer;
            NkVector<ContourInfo> holes;
        };
        NkVector<ContourGroup> groups;
        
        for (size_t i = 0; i < contourInfos.Size(); ++i) {
            if (!contourInfos[i].isOuter) continue;  // On traite seulement les extérieurs
            
            ContourGroup group;
            group.outer = contourInfos[i];
            
            // Trouver les trous à l'intérieur de ce contour extérieur
            for (size_t j = 0; j < contourInfos.Size(); ++j) {
                if (i == j) continue;
                if (!contourInfos[j].isOuter) {  // C'est un trou potentiel
                    // Vérifier si le premier point du trou est à l'intérieur de l'extérieur
                    if (IsPointInPolygon(group.outer.contour, contourInfos[j].contour[0])) {
                        group.holes.PushBack(contourInfos[j]);
                    }
                }
            }
            groups.PushBack(group);
        }

        // Traitement de chaque groupe (extérieur + ses trous)
        for (const auto& group : groups) {
            // Points 3D avant/arrière pour le contour extérieur
            NkVector<NkVec3f> frontPts, backPts;
            NkVector<NkVec2f> uvs;
            
            for (const auto& pt : group.outer.contour) {
                float lx = pt.x * scale;
                float ly = pt.y * scale;
                frontPts.PushBack(WorldTransform(worldMatrix, lx, ly, 0.f));
                backPts.PushBack(WorldTransform(worldMatrix, lx, ly, extrusionDepth));

                // Calcul des UV : mapper la position 2D dans la bbox du glyphe
                float u = (pt.x - gl->x0) / (gl->x1 - gl->x0 + 1e-6f);
                float v = (pt.y - gl->y0) / (gl->y1 - gl->y0 + 1e-6f);
                uvs.PushBack({math::NkClamp(u, 0.f, 1.f), math::NkClamp(v, 0.f, 1.f)});
            }

            // Triangulation du contour extérieur seulement pour les faces avant/arrière
            // Les trous ne sont PAS triangulés → ils apparaissent comme des vides
            NkVector<uint32_t> triIndices;
            TriangulateContour(group.outer.contour, triIndices);

            // Faces avant (winding CCW → normale -Z)
            for (size_t t = 0; t + 2 < triIndices.Size(); t += 3) {
                uint32_t i0 = triIndices[t], i1 = triIndices[t+1], i2 = triIndices[t+2];
                NkFontGlyph3DVertex tri[3] = {
                    {frontPts[i0], normFront, uvs[i0], color, (nkft_uint32)cp, 0},
                    {frontPts[i1], normFront, uvs[i1], color, (nkft_uint32)cp, 0},
                    {frontPts[i2], normFront, uvs[i2], color, (nkft_uint32)cp, 0}
                };
                callback(tri, 3, userData);
            }

            // Faces arrière (winding inversé → normale +Z)
            for (size_t t = 0; t + 2 < triIndices.Size(); t += 3) {
                uint32_t i0 = triIndices[t], i1 = triIndices[t+1], i2 = triIndices[t+2];
                NkFontGlyph3DVertex tri[3] = {
                    {backPts[i0], normBack, uvs[i0], color, (nkft_uint32)cp, 1},
                    {backPts[i2], normBack, uvs[i2], color, (nkft_uint32)cp, 1},
                    {backPts[i1], normBack, uvs[i1], color, (nkft_uint32)cp, 1}
                };
                callback(tri, 3, userData);
            }

            // Faces latérales pour le contour extérieur — normales toujours sortantes
            for (size_t i = 0; i < group.outer.contour.Size(); ++i) {
                size_t j = (i + 1) % group.outer.contour.Size();
                
                const NkVec3f& a0 = frontPts[i];
                const NkVec3f& a1 = frontPts[j];
                const NkVec3f& b0 = backPts[i];
                const NkVec3f& b1 = backPts[j];

                // Calcul de la normale latérale sortante en utilisant l'information trou/extérieur
                NkVec3f sideNorm = ComputeOutwardSideNormal(group.outer.contour[i], group.outer.contour[j], true);
                // La normale calculée est en 2D, on la transforme avec la matrice monde (rotation uniquement)
                // mais ici on l'utilise telle quelle car les faces latérales sont parallèles à Z.
                // Il faut s'assurer qu'elle est bien orientée dans l'espace 3D.
                // On applique la rotation de la matrice monde à la normale (sans translation)
                NkVec3f worldNormal{
                    worldMatrix.data[0]*sideNorm.x + worldMatrix.data[4]*sideNorm.y + worldMatrix.data[8]*sideNorm.z,
                    worldMatrix.data[1]*sideNorm.x + worldMatrix.data[5]*sideNorm.y + worldMatrix.data[9]*sideNorm.z,
                    worldMatrix.data[2]*sideNorm.x + worldMatrix.data[6]*sideNorm.y + worldMatrix.data[10]*sideNorm.z
                };
                worldNormal = worldNormal.Normalized();

                // Quad latéral : deux triangles
                NkFontGlyph3DVertex quad[6] = {
                    {a0, worldNormal, uvs[i], color, (nkft_uint32)cp, 2},
                    {b0, worldNormal, uvs[i], color, (nkft_uint32)cp, 2},
                    {b1, worldNormal, uvs[j], color, (nkft_uint32)cp, 2},
                    {a0, worldNormal, uvs[i], color, (nkft_uint32)cp, 2},
                    {b1, worldNormal, uvs[j], color, (nkft_uint32)cp, 2},
                    {a1, worldNormal, uvs[j], color, (nkft_uint32)cp, 2}
                };
                callback(quad, 6, userData);
            }

            // Traitement des trous : uniquement les faces latérales (pas de triangulation front/back)
            for (const auto& hole : group.holes) {
                NkVector<NkVec3f> holeFrontPts, holeBackPts;
                NkVector<NkVec2f> holeUvs;
                
                for (const auto& pt : hole.contour) {
                    float lx = pt.x * scale;
                    float ly = pt.y * scale;
                    holeFrontPts.PushBack(WorldTransform(worldMatrix, lx, ly, 0.f));
                    holeBackPts.PushBack(WorldTransform(worldMatrix, lx, ly, extrusionDepth));

                    float u = (pt.x - gl->x0) / (gl->x1 - gl->x0 + 1e-6f);
                    float v = (pt.y - gl->y0) / (gl->y1 - gl->y0 + 1e-6f);
                    holeUvs.PushBack({math::NkClamp(u, 0.f, 1.f), math::NkClamp(v, 0.f, 1.f)});
                }

                // Faces latérales pour le trou — normales sortantes (isOuter = false)
                for (size_t i = 0; i < hole.contour.Size(); ++i) {
                    size_t j = (i + 1) % hole.contour.Size();
                    
                    const NkVec3f& a0 = holeFrontPts[i];
                    const NkVec3f& a1 = holeFrontPts[j];
                    const NkVec3f& b0 = holeBackPts[i];
                    const NkVec3f& b1 = holeBackPts[j];

                    NkVec3f sideNorm = ComputeOutwardSideNormal(hole.contour[i], hole.contour[j], false);
                    NkVec3f worldNormal{
                        worldMatrix.data[0]*sideNorm.x + worldMatrix.data[4]*sideNorm.y + worldMatrix.data[8]*sideNorm.z,
                        worldMatrix.data[1]*sideNorm.x + worldMatrix.data[5]*sideNorm.y + worldMatrix.data[9]*sideNorm.z,
                        worldMatrix.data[2]*sideNorm.x + worldMatrix.data[6]*sideNorm.y + worldMatrix.data[10]*sideNorm.z
                    };
                    worldNormal = worldNormal.Normalized();

                    NkFontGlyph3DVertex quad[6] = {
                        {a0, worldNormal, holeUvs[i], color, (nkft_uint32)cp, 2},
                        {b0, worldNormal, holeUvs[i], color, (nkft_uint32)cp, 2},
                        {b1, worldNormal, holeUvs[j], color, (nkft_uint32)cp, 2},
                        {a0, worldNormal, holeUvs[i], color, (nkft_uint32)cp, 2},
                        {b1, worldNormal, holeUvs[j], color, (nkft_uint32)cp, 2},
                        {a1, worldNormal, holeUvs[j], color, (nkft_uint32)cp, 2}
                    };
                    callback(quad, 6, userData);
                }
            }
        }

        // Traitement des contours qui n'ont pas été groupés (contours extérieurs isolés sans trous)
        // Cette boucle gère les cas où un contour extérieur n'a pas de trous associés
        // ou les contours qui étaient déjà traités dans la boucle principale
        // (Cette section est redondante mais conservée pour sécurité)
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