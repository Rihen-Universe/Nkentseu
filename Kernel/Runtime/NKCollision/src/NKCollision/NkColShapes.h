#pragma once
// =============================================================================
// NkColShapes.h — Formes de collision 2D + 3D (ZÉRO STL) + calcul d'AABB.
// v1 : primitives analytiques (cercle/sphère, boîte, capsule). Polygone/convexe
// (GJK/EPA) + OBB pleine en phase suivante.
// =============================================================================
#include "NKCollision/NkColTypes.h"

namespace nkentseu {
    namespace collision {

        // Forme runtime compacte (triviale à copier). Les champs p0/p1 sont réutilisés
        // selon le type pour éviter une union verbeuse.
        struct NkShape {
            NkShapeType type = NkShapeType::NK_SPHERE;
            NkVec3f     p0{};            // cercle/sphère: centre · boîte: centre · capsule: A
            NkVec3f     p1{};            // boîte: demi-extents · capsule: B
            float32     radius   = 0.5f; // cercle/sphère/capsule
            float32     rotation = 0.f;  // boîte 2D (radians), réservé OBB

            // ── Fabriques 2D (z ignoré) ──────────────────────────────────────
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
                case NkShapeType::NK_CAPSULE2D: {
                    NkVec2f a{ s.p0.x, s.p0.y }, b{ s.p1.x, s.p1.y }, r{ s.radius, s.radius };
                    NkVec2f mn{ math::NkMin(a.x, b.x), math::NkMin(a.y, b.y) };
                    NkVec2f mx{ math::NkMax(a.x, b.x), math::NkMax(a.y, b.y) };
                    box.min = mn - r; box.max = mx + r; break;
                }
                default: { box.min = c; box.max = c; break; }
            }
            return box;
        }

    } // namespace collision
} // namespace nkentseu
