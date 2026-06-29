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

        // ── Types de forme (2D + 3D) — taxonomie « type PhysX » ──────────────
        // Trois familles selon le chemin de narrowphase :
        //   • CONVEXE  -> fonction de support unique => GJK/EPA générique.
        //   • ANALYTIQUE special (plan/half-space infini) -> test dédié.
        //   • CONCAVE/COMPOSITE -> décomposition en convexes (vague suivante).
        // L'ordre groupe 2D puis 3D ; NkShapeIs2D/IsConvex/IsConcave classent.
        enum class NkShapeType : uint8 {
            // ── 2D convexes ──────────────────────────────────────────────────
            NK_POINT2D = 0,      // point (degenere, utile pour requetes)
            NK_SEGMENT2D,        // segment a-b (epaisseur nulle)
            NK_CIRCLE2D,         // centre + rayon
            NK_CAPSULE2D,        // segment a-b + rayon
            NK_TRIANGLE2D,       // 3 sommets (verts)
            NK_BOX2D,            // OBB : centre + demi-extents + rotation (radians)
            NK_POLYGON2D,        // convexe, sommets CCW (verts)
            // ── 2D concave / statique ────────────────────────────────────────
            NK_CHAIN2D,          // polyligne concave (sol/terrain 2D, statique)
            // ── 3D convexes ──────────────────────────────────────────────────
            NK_SPHERE,           // centre + rayon
            NK_CAPSULE3D,        // segment a-b + rayon
            NK_TRIANGLE3D,       // 3 sommets (verts)
            NK_BOX3D,            // boite alignee : centre + demi-extents
            NK_CYLINDER3D,       // centre + axe (p1, unite) + demi-hauteur + rayon
            NK_CONE3D,           // base (p0) + axe (p1, unite) + hauteur + rayon base
            NK_CONVEX3D,         // enveloppe convexe (verts)
            // ── 3D analytique special ────────────────────────────────────────
            NK_PLANE3D,          // half-space infini : point (p0) + normale (p1)
            // ── 3D concave / composite (statique) ────────────────────────────
            NK_HEIGHTFIELD3D,    // terrain regulier
            NK_TRIMESH3D,        // soupe de triangles (BVH), statique
            NK_COMPOUND,         // plusieurs sous-formes + transform local (2D ou 3D)
            NK_COUNT
        };

        // 2D = tout ce qui precede NK_SPHERE.
        NK_FORCE_INLINE bool NkShapeIs2D(NkShapeType t) noexcept {
            return (uint8)t <= (uint8)NkShapeType::NK_CHAIN2D;
        }
        // Famille convexe -> support function -> GJK/EPA.
        NK_FORCE_INLINE bool NkShapeIsConvex(NkShapeType t) noexcept {
            switch (t) {
                case NkShapeType::NK_POINT2D:   case NkShapeType::NK_SEGMENT2D:
                case NkShapeType::NK_CIRCLE2D:  case NkShapeType::NK_CAPSULE2D:
                case NkShapeType::NK_TRIANGLE2D:case NkShapeType::NK_BOX2D:
                case NkShapeType::NK_POLYGON2D:
                case NkShapeType::NK_SPHERE:    case NkShapeType::NK_CAPSULE3D:
                case NkShapeType::NK_TRIANGLE3D:case NkShapeType::NK_BOX3D:
                case NkShapeType::NK_CYLINDER3D:case NkShapeType::NK_CONE3D:
                case NkShapeType::NK_CONVEX3D:  return true;
                default:                        return false;
            }
        }
        // Concave/composite -> decomposition (CHAIN2D, HEIGHTFIELD3D, TRIMESH3D, COMPOUND).
        NK_FORCE_INLINE bool NkShapeIsConcave(NkShapeType t) noexcept {
            return t == NkShapeType::NK_CHAIN2D || t == NkShapeType::NK_HEIGHTFIELD3D
                || t == NkShapeType::NK_TRIMESH3D || t == NkShapeType::NK_COMPOUND;
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
        // `id` = identifiant de feature (arête/sommet source) STABLE entre frames ->
        // permet de retrouver le même point d'une frame à l'autre (warm-starting solveur).
        struct NkContactPoint2D { NkVec2f point{}; float32 depth = 0.f; uint32 id = 0; };
        struct NkManifold2D {
            NkVec2f          normal{};        // A -> B, normalisée
            NkContactPoint2D points[2];
            int32            count = 0;       // 0 = pas de contact
            NK_FORCE_INLINE bool Hit() const noexcept { return count > 0; }
        };

        struct NkContactPoint3D { NkVec3f point{}; float32 depth = 0.f; uint32 id = 0; };
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
