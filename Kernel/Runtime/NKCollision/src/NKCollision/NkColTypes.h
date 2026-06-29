#pragma once
// =============================================================================
// NkColTypes.h — Types fondamentaux de NKCollision (2D + 3D), ZÉRO STL.
// Tout repose sur NKMath (vecteurs) + NKCore (types primitifs). Aucune dépendance
// std:: — les conteneurs viennent de NKContainers, la mémoire de NKMemory.
// =============================================================================
#include "NKMath/NKMath.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace collision {

        using nkentseu::float32;
        using nkentseu::int32;
        using nkentseu::uint8;
        using nkentseu::uint32;
        using nkentseu::math::NkVec2f;
        using nkentseu::math::NkVec3f;

        // ── Types de forme (2D + 3D) ─────────────────────────────────────────
        enum class NkShapeType : uint8 {
            // 2D
            NK_CIRCLE2D = 0,
            NK_BOX2D,            // OBB : centre + demi-extents + rotation (radians)
            NK_CAPSULE2D,        // segment a-b + rayon
            NK_POLYGON2D,        // convexe, sommets CCW
            // 3D
            NK_SPHERE,
            NK_BOX3D,            // OBB : centre + demi-extents + base orthonormée
            NK_CAPSULE3D,        // segment a-b + rayon
            NK_CONVEX3D,         // enveloppe convexe (sommets)
            NK_COUNT
        };

        NK_FORCE_INLINE bool NkShapeIs2D(NkShapeType t) noexcept {
            return t == NkShapeType::NK_CIRCLE2D || t == NkShapeType::NK_BOX2D
                || t == NkShapeType::NK_CAPSULE2D || t == NkShapeType::NK_POLYGON2D;
        }

        // ── Boîtes englobantes alignées (broadphase) ─────────────────────────
        struct NkAABB2D {
            NkVec2f min{ 0.f, 0.f };
            NkVec2f max{ 0.f, 0.f };

            NK_FORCE_INLINE NkVec2f Center() const noexcept { return (min + max) * 0.5f; }
            NK_FORCE_INLINE NkVec2f Extents() const noexcept { return (max - min) * 0.5f; }
            NK_FORCE_INLINE bool Overlaps(const NkAABB2D& o) const noexcept {
                return min.x <= o.max.x && max.x >= o.min.x
                    && min.y <= o.max.y && max.y >= o.min.y;
            }
            NK_FORCE_INLINE bool Contains(const NkVec2f& p) const noexcept {
                return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
            }
            NK_FORCE_INLINE void Expand(const NkVec2f& p) noexcept {
                if (p.x < min.x) min.x = p.x; if (p.y < min.y) min.y = p.y;
                if (p.x > max.x) max.x = p.x; if (p.y > max.y) max.y = p.y;
            }
            NK_FORCE_INLINE NkAABB2D Merged(const NkAABB2D& o) const noexcept {
                NkAABB2D r;
                r.min = { math::NkMin(min.x, o.min.x), math::NkMin(min.y, o.min.y) };
                r.max = { math::NkMax(max.x, o.max.x), math::NkMax(max.y, o.max.y) };
                return r;
            }
            NK_FORCE_INLINE void Grow(float32 margin) noexcept {
                min.x -= margin; min.y -= margin; max.x += margin; max.y += margin;
            }
        };

        struct NkAABB3D {
            NkVec3f min{ 0.f, 0.f, 0.f };
            NkVec3f max{ 0.f, 0.f, 0.f };

            NK_FORCE_INLINE NkVec3f Center() const noexcept { return (min + max) * 0.5f; }
            NK_FORCE_INLINE NkVec3f Extents() const noexcept { return (max - min) * 0.5f; }
            NK_FORCE_INLINE bool Overlaps(const NkAABB3D& o) const noexcept {
                return min.x <= o.max.x && max.x >= o.min.x
                    && min.y <= o.max.y && max.y >= o.min.y
                    && min.z <= o.max.z && max.z >= o.min.z;
            }
            NK_FORCE_INLINE bool Contains(const NkVec3f& p) const noexcept {
                return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y
                    && p.z >= min.z && p.z <= max.z;
            }
            NK_FORCE_INLINE void Expand(const NkVec3f& p) noexcept {
                if (p.x < min.x) min.x = p.x; if (p.y < min.y) min.y = p.y; if (p.z < min.z) min.z = p.z;
                if (p.x > max.x) max.x = p.x; if (p.y > max.y) max.y = p.y; if (p.z > max.z) max.z = p.z;
            }
            NK_FORCE_INLINE NkAABB3D Merged(const NkAABB3D& o) const noexcept {
                NkAABB3D r;
                r.min = { math::NkMin(min.x, o.min.x), math::NkMin(min.y, o.min.y), math::NkMin(min.z, o.min.z) };
                r.max = { math::NkMax(max.x, o.max.x), math::NkMax(max.y, o.max.y), math::NkMax(max.z, o.max.z) };
                return r;
            }
            NK_FORCE_INLINE void Grow(float32 m) noexcept {
                min.x -= m; min.y -= m; min.z -= m; max.x += m; max.y += m; max.z += m;
            }
        };

        // ── Manifold de contact (résultat narrowphase) ───────────────────────
        // Normale orientée de A vers B ; depth = profondeur de pénétration (>0 si
        // chevauchement). Jusqu'à 2 points en 2D, 4 en 3D (faces).
        struct NkContactPoint2D { NkVec2f point{}; float32 depth = 0.f; };
        struct NkManifold2D {
            NkVec2f          normal{};        // A -> B, normalisée
            NkContactPoint2D points[2];
            int32            count = 0;       // 0 = pas de contact
            NK_FORCE_INLINE bool Hit() const noexcept { return count > 0; }
        };

        struct NkContactPoint3D { NkVec3f point{}; float32 depth = 0.f; };
        struct NkManifold3D {
            NkVec3f          normal{};        // A -> B, normalisée
            NkContactPoint3D points[4];
            int32            count = 0;
            NK_FORCE_INLINE bool Hit() const noexcept { return count > 0; }
        };

        // ── Lancer de rayon ──────────────────────────────────────────────────
        struct NkRay2D  { NkVec2f origin{}; NkVec2f dir{ 1.f, 0.f }; float32 maxT = 1e30f; };
        struct NkRay3D  { NkVec3f origin{}; NkVec3f dir{ 0.f, 0.f, 1.f }; float32 maxT = 1e30f; };
        struct NkRayHit2D { bool hit = false; float32 t = 0.f; NkVec2f point{}; NkVec2f normal{}; };
        struct NkRayHit3D { bool hit = false; float32 t = 0.f; NkVec3f point{}; NkVec3f normal{}; };

    } // namespace collision
} // namespace nkentseu
