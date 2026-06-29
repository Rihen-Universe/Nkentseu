#pragma once
// =============================================================================
// NkColShapes.h — Formes de collision 2D + 3D (ZÉRO STL) + calcul d'AABB.
// v1 : primitives analytiques (cercle/sphère, boîte, capsule). Polygone/convexe
// (GJK/EPA) + OBB pleine en phase suivante.
// =============================================================================
#include "NKCollision/NkColTypes.h"

namespace nkentseu {
    namespace collision {

        // Forme runtime compacte (triviale à copier). Les champs p0/p1/radius/height
        // sont réutilisés selon le type pour éviter une union verbeuse. Les sommets
        // (triangle/polygone/convexe) sont NON-OWNANTS : la durée de vie du buffer
        // `verts` est gérée par l'appelant (typiquement le corps/asset).
        struct NkShape {
            NkShapeType    type = NkShapeType::NK_SPHERE;
            NkVec3f        p0{};            // centre · A (capsule/seg) · base (cone) · point plan
            NkVec3f        p1{};            // demi-extents · B (capsule/seg) · axe unite (cyl/cone) · normale (plan)
            float32        radius   = 0.5f; // cercle/sphère/capsule/cylindre/cône
            float32        rotation = 0.f;  // boîte 2D (radians)
            float32        height   = 0.f;  // cylindre (demi-hauteur) · cône (hauteur base->apex)
            const NkVec3f* verts    = nullptr; // triangle/polygone/convexe (z=0 en 2D) · sommets trimesh · sommets chain2D
            uint32         vertCount = 0;
            // ── Concaves / composite (non-ownants) ───────────────────────────
            const uint32*  indices   = nullptr; // trimesh : triplets de sommets
            uint32         indexCount = 0;       // = 3 * nbTriangles
            const float32* heights   = nullptr; // heightfield : grille rows*cols de hauteurs (Y)
            uint32         rows = 0, cols = 0;   // heightfield : dimensions de la grille
            const NkShape* children   = nullptr; // compound : sous-formes (positionnées dans le repère local)
            uint32         childCount = 0;

            // ── Fabriques 2D (z ignoré) ──────────────────────────────────────
            static NkShape Point2D(const NkVec2f& p) noexcept {
                NkShape s; s.type = NkShapeType::NK_POINT2D; s.p0 = { p.x, p.y, 0.f }; s.radius = 0.f; return s;
            }
            static NkShape Segment2D(const NkVec2f& a, const NkVec2f& b) noexcept {
                NkShape s; s.type = NkShapeType::NK_SEGMENT2D;
                s.p0 = { a.x, a.y, 0.f }; s.p1 = { b.x, b.y, 0.f }; s.radius = 0.f; return s;
            }
            static NkShape Circle2D(const NkVec2f& c, float32 r) noexcept {
                NkShape s; s.type = NkShapeType::NK_CIRCLE2D;
                s.p0 = { c.x, c.y, 0.f }; s.radius = r; return s;
            }
            static NkShape Box2D(const NkVec2f& center, const NkVec2f& halfExtents, float32 rot = 0.f) noexcept {
                NkShape s; s.type = NkShapeType::NK_BOX2D;
                s.p0 = { center.x, center.y, 0.f }; s.p1 = { halfExtents.x, halfExtents.y, 0.f };
                s.rotation = rot; return s;
            }
            static NkShape Capsule2D(const NkVec2f& a, const NkVec2f& b, float32 r) noexcept {
                NkShape s; s.type = NkShapeType::NK_CAPSULE2D;
                s.p0 = { a.x, a.y, 0.f }; s.p1 = { b.x, b.y, 0.f }; s.radius = r; return s;
            }
            // verts3 -> 3 sommets (z ignoré). Buffer non-ownant.
            static NkShape Triangle2D(const NkVec3f* verts3) noexcept {
                NkShape s; s.type = NkShapeType::NK_TRIANGLE2D; s.verts = verts3; s.vertCount = 3; s.radius = 0.f; return s;
            }
            // Polygone convexe CCW (z ignoré). Buffer non-ownant.
            static NkShape Polygon2D(const NkVec3f* verts, uint32 n) noexcept {
                NkShape s; s.type = NkShapeType::NK_POLYGON2D; s.verts = verts; s.vertCount = n; s.radius = 0.f; return s;
            }

            // ── Fabriques 3D ─────────────────────────────────────────────────
            static NkShape Sphere(const NkVec3f& c, float32 r) noexcept {
                NkShape s; s.type = NkShapeType::NK_SPHERE; s.p0 = c; s.radius = r; return s;
            }
            static NkShape Box3D(const NkVec3f& center, const NkVec3f& halfExtents) noexcept {
                NkShape s; s.type = NkShapeType::NK_BOX3D; s.p0 = center; s.p1 = halfExtents; return s;
            }
            static NkShape Capsule3D(const NkVec3f& a, const NkVec3f& b, float32 r) noexcept {
                NkShape s; s.type = NkShapeType::NK_CAPSULE3D; s.p0 = a; s.p1 = b; s.radius = r; return s;
            }
            // Cylindre : centre + axe unite + demi-hauteur + rayon.
            static NkShape Cylinder3D(const NkVec3f& center, const NkVec3f& axisUnit, float32 halfHeight, float32 r) noexcept {
                NkShape s; s.type = NkShapeType::NK_CYLINDER3D; s.p0 = center; s.p1 = axisUnit;
                s.height = halfHeight; s.radius = r; return s;
            }
            // Cône : centre de base + axe unite (base->apex) + hauteur + rayon de base.
            static NkShape Cone3D(const NkVec3f& baseCenter, const NkVec3f& axisUnit, float32 fullHeight, float32 baseRadius) noexcept {
                NkShape s; s.type = NkShapeType::NK_CONE3D; s.p0 = baseCenter; s.p1 = axisUnit;
                s.height = fullHeight; s.radius = baseRadius; return s;
            }
            // Triangle 3D : 3 sommets (non-ownant).
            static NkShape Triangle3D(const NkVec3f* verts3) noexcept {
                NkShape s; s.type = NkShapeType::NK_TRIANGLE3D; s.verts = verts3; s.vertCount = 3; s.radius = 0.f; return s;
            }
            // Enveloppe convexe : sommets (non-ownant).
            static NkShape Convex3D(const NkVec3f* verts, uint32 n) noexcept {
                NkShape s; s.type = NkShapeType::NK_CONVEX3D; s.verts = verts; s.vertCount = n; s.radius = 0.f; return s;
            }
            // Plan / half-space infini : un point + une normale unite (le solide est du
            // côté opposé à la normale, i.e. dot(n, x - point) <= 0).
            static NkShape Plane3D(const NkVec3f& point, const NkVec3f& normalUnit) noexcept {
                NkShape s; s.type = NkShapeType::NK_PLANE3D; s.p0 = point; s.p1 = normalUnit; s.radius = 0.f; return s;
            }

            // ── Concaves / composite (statiques sauf compound) ───────────────
            // Triangle mesh : sommets + indices (triplets). Buffers non-ownants.
            static NkShape Trimesh3D(const NkVec3f* verts, uint32 vertN, const uint32* indices, uint32 indexN) noexcept {
                NkShape s; s.type = NkShapeType::NK_TRIMESH3D; s.verts = verts; s.vertCount = vertN;
                s.indices = indices; s.indexCount = indexN; s.radius = 0.f; return s;
            }
            // Heightfield : grille rows*cols de hauteurs Y. p0 = coin (x,_,z) min, p1 = (dx,_,dz) pas de cellule.
            static NkShape Heightfield3D(const NkVec3f& origin, float32 cellX, float32 cellZ,
                                         const float32* heights, uint32 rows, uint32 cols) noexcept {
                NkShape s; s.type = NkShapeType::NK_HEIGHTFIELD3D; s.p0 = origin;
                s.p1 = { cellX, 0.f, cellZ }; s.heights = heights; s.rows = rows; s.cols = cols; s.radius = 0.f; return s;
            }
            // Chain 2D : polyligne concave (sol/terrain). Segments = sommets consécutifs.
            static NkShape Chain2D(const NkVec3f* verts, uint32 n) noexcept {
                NkShape s; s.type = NkShapeType::NK_CHAIN2D; s.verts = verts; s.vertCount = n; s.radius = 0.f; return s;
            }
            // Compound : sous-formes positionnées dans le repère local (offset = p0). Non-ownant.
            static NkShape Compound(const NkShape* children, uint32 n, const NkVec3f& offset = {}) noexcept {
                NkShape s; s.type = NkShapeType::NK_COMPOUND; s.children = children; s.childCount = n; s.p0 = offset; s.radius = 0.f; return s;
            }
        };

        // ── AABB d'une forme (broadphase) ────────────────────────────────────
        NK_FORCE_INLINE NkAABB3D NkComputeAABB3D(const NkShape& s) noexcept {
            NkAABB3D box;
            switch (s.type) {
                case NkShapeType::NK_SPHERE: {
                    NkVec3f r{ s.radius, s.radius, s.radius };
                    box.min = s.p0 - r; box.max = s.p0 + r; break;
                }
                case NkShapeType::NK_BOX3D: {
                    box.min = s.p0 - s.p1; box.max = s.p0 + s.p1; break;
                }
                case NkShapeType::NK_CAPSULE3D: {
                    NkVec3f r{ s.radius, s.radius, s.radius };
                    NkVec3f mn{ math::NkMin(s.p0.x, s.p1.x), math::NkMin(s.p0.y, s.p1.y), math::NkMin(s.p0.z, s.p1.z) };
                    NkVec3f mx{ math::NkMax(s.p0.x, s.p1.x), math::NkMax(s.p0.y, s.p1.y), math::NkMax(s.p0.z, s.p1.z) };
                    box.min = mn - r; box.max = mx + r; break;
                }
                case NkShapeType::NK_CYLINDER3D: {
                    // ext_i = demiHauteur*|axe_i| + rayon*sqrt(1 - axe_i^2)  (enveloppe exacte du cylindre)
                    const NkVec3f& a = s.p1;
                    NkVec3f e{
                        s.height * math::NkAbs(a.x) + s.radius * math::NkSqrt(math::NkMax(0.f, 1.f - a.x * a.x)),
                        s.height * math::NkAbs(a.y) + s.radius * math::NkSqrt(math::NkMax(0.f, 1.f - a.y * a.y)),
                        s.height * math::NkAbs(a.z) + s.radius * math::NkSqrt(math::NkMax(0.f, 1.f - a.z * a.z)) };
                    box.min = s.p0 - e; box.max = s.p0 + e; break;
                }
                case NkShapeType::NK_CONE3D: {
                    // disque de base (centre p0, rayon) etendu par l'apex.
                    const NkVec3f& a = s.p1;
                    NkVec3f er{
                        s.radius * math::NkSqrt(math::NkMax(0.f, 1.f - a.x * a.x)),
                        s.radius * math::NkSqrt(math::NkMax(0.f, 1.f - a.y * a.y)),
                        s.radius * math::NkSqrt(math::NkMax(0.f, 1.f - a.z * a.z)) };
                    NkVec3f apex = s.p0 + a * s.height;
                    box.min = { math::NkMin(s.p0.x - er.x, apex.x), math::NkMin(s.p0.y - er.y, apex.y), math::NkMin(s.p0.z - er.z, apex.z) };
                    box.max = { math::NkMax(s.p0.x + er.x, apex.x), math::NkMax(s.p0.y + er.y, apex.y), math::NkMax(s.p0.z + er.z, apex.z) };
                    break;
                }
                case NkShapeType::NK_TRIANGLE3D:
                case NkShapeType::NK_CONVEX3D: {
                    if (s.verts && s.vertCount) {
                        box.min = box.max = s.verts[0];
                        for (uint32 i = 1; i < s.vertCount; ++i) box.Expand(s.verts[i]);
                    } else { box.min = s.p0; box.max = s.p0; }
                    break;
                }
                case NkShapeType::NK_PLANE3D: {
                    const float32 big = 1e18f;
                    box.min = { -big, -big, -big }; box.max = { big, big, big }; break;
                }
                case NkShapeType::NK_TRIMESH3D: {
                    if (s.verts && s.vertCount) {
                        box.min = box.max = s.verts[0];
                        for (uint32 i = 1; i < s.vertCount; ++i) box.Expand(s.verts[i]);
                    } else { box.min = s.p0; box.max = s.p0; }
                    break;
                }
                case NkShapeType::NK_HEIGHTFIELD3D: {
                    const float32 w = (s.cols > 0 ? (float32)(s.cols - 1) : 0.f) * s.p1.x;
                    const float32 d = (s.rows > 0 ? (float32)(s.rows - 1) : 0.f) * s.p1.z;
                    float32 ymin = 0.f, ymax = 0.f;
                    if (s.heights && s.rows * s.cols) { ymin = ymax = s.heights[0]; for (uint32 i = 1; i < s.rows * s.cols; ++i) { ymin = math::NkMin(ymin, s.heights[i]); ymax = math::NkMax(ymax, s.heights[i]); } }
                    box.min = { s.p0.x, s.p0.y + ymin, s.p0.z };
                    box.max = { s.p0.x + w, s.p0.y + ymax, s.p0.z + d };
                    break;
                }
                case NkShapeType::NK_COMPOUND: {
                    if (s.children && s.childCount) {
                        NkAABB3D c0 = NkComputeAABB3D(s.children[0]);
                        box.min = c0.min + s.p0; box.max = c0.max + s.p0;
                        for (uint32 i = 1; i < s.childCount; ++i) {
                            NkAABB3D ci = NkComputeAABB3D(s.children[i]);
                            box.Expand(ci.min + s.p0); box.Expand(ci.max + s.p0);
                        }
                    } else { box.min = s.p0; box.max = s.p0; }
                    break;
                }
                default: { box.min = s.p0; box.max = s.p0; break; }
            }
            return box;
        }

        NK_FORCE_INLINE NkAABB2D NkComputeAABB2D(const NkShape& s) noexcept {
            NkAABB2D box;
            const NkVec2f c{ s.p0.x, s.p0.y };
            switch (s.type) {
                case NkShapeType::NK_CIRCLE2D: {
                    NkVec2f r{ s.radius, s.radius }; box.min = c - r; box.max = c + r; break;
                }
                case NkShapeType::NK_BOX2D: {
                    // demi-extents étendus par la rotation (enveloppe alignée de l'OBB)
                    const float32 ca = math::NkAbs(math::NkCos(s.rotation));
                    const float32 sa = math::NkAbs(math::NkSin(s.rotation));
                    NkVec2f e{ s.p1.x * ca + s.p1.y * sa, s.p1.x * sa + s.p1.y * ca };
                    box.min = c - e; box.max = c + e; break;
                }
                case NkShapeType::NK_CAPSULE2D:
                case NkShapeType::NK_SEGMENT2D: {
                    NkVec2f a{ s.p0.x, s.p0.y }, b{ s.p1.x, s.p1.y }, r{ s.radius, s.radius };
                    NkVec2f mn{ math::NkMin(a.x, b.x), math::NkMin(a.y, b.y) };
                    NkVec2f mx{ math::NkMax(a.x, b.x), math::NkMax(a.y, b.y) };
                    box.min = mn - r; box.max = mx + r; break;
                }
                case NkShapeType::NK_TRIANGLE2D:
                case NkShapeType::NK_POLYGON2D:
                case NkShapeType::NK_CHAIN2D: {
                    if (s.verts && s.vertCount) {
                        NkVec2f v0{ s.verts[0].x, s.verts[0].y };
                        box.min = box.max = v0;
                        for (uint32 i = 1; i < s.vertCount; ++i) box.Expand({ s.verts[i].x, s.verts[i].y });
                    } else { box.min = c; box.max = c; }
                    break;
                }
                default: { box.min = c; box.max = c; break; }
            }
            return box;
        }

    } // namespace collision
} // namespace nkentseu
