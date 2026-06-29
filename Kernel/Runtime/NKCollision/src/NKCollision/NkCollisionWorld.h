#pragma once
// =============================================================================
// NkCollisionWorld.h — Monde de collision (ZÉRO STL). Corps + broadphase +
// narrowphase + requêtes (raycast, overlap). 2D et 3D (un corps est l'un OU
// l'autre selon sa forme ; les paires mixtes 2D/3D sont ignorées).
// Conteneurs = NKContainers (NkVector). v1 broadphase = balayage O(n²) avec marge ;
// SAP/grille/BVH = phase suivante (architecture prête).
// =============================================================================
#include "NKCollision/NkColTests.h"
#include "NKCollision/NkDbvh.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace collision {

        struct NkBody {
            NkShape shape;
            uint32  id      = 0;
            uint32  layer   = 0x1u;         // bitmask d'appartenance
            uint32  mask    = 0xFFFFFFFFu;  // bitmask de collision (avec quels layers)
            void*   user    = nullptr;      // donnée applicative (entité, etc.)
            bool    active  = true;
            bool    trigger = false;        // zone de détection (la physique au-dessus ne résout pas)
            int32   proxy   = -1;           // index dans le DBVH (broadphase persistante)
        };

        // Paire en contact. Manifold universel 3D (les contacts 2D ont z=0).
        struct NkCollisionPair {
            uint32       a = 0;
            uint32       b = 0;
            NkManifold3D manifold;
            bool         warm = false;     // true = la paire existait déjà la frame précédente
        };

        // Événement de collision entre deux corps (par id). Émis par Step().
        struct NkCollisionEvent { uint32 a = 0; uint32 b = 0; };

        class NkWorld {
            public:
                NkWorld() = default;

                uint32 AddBody(const NkShape& shape, uint32 layer = 0x1u,
                               uint32 mask = 0xFFFFFFFFu, void* user = nullptr);
                void   RemoveBody(uint32 id);
                NkBody* GetBody(uint32 id);
                const NkBody* GetBody(uint32 id) const;
                void   SetShape(uint32 id, const NkShape& s);
                void   Clear();

                // Broadphase + narrowphase -> remplit la liste des paires en contact.
                void   Step();
                const NkVector<NkCollisionPair>& Pairs() const noexcept { return mPairs; }

                // Événements calculés par Step() (comparaison avec la frame précédente).
                const NkVector<NkCollisionEvent>& EnterEvents() const noexcept { return mEnter; }
                const NkVector<NkCollisionEvent>& StayEvents()  const noexcept { return mStay; }
                const NkVector<NkCollisionEvent>& ExitEvents()  const noexcept { return mExit; }

                // Manifold de la paire (a,b) à la frame PRÉCÉDENTE (pour warm-starting le
                // solveur : matcher les points par `id`). false si la paire n'existait pas.
                bool GetPreviousManifold(uint32 a, uint32 b, NkManifold3D& out) const;

                // Marquer un corps comme trigger (zone de détection).
                void   SetTrigger(uint32 id, bool t) { if (NkBody* b = GetBody(id)) b->trigger = t; }

                // Raycast : renvoie le hit le PLUS PROCHE (filtré par mask de layers).
                bool   Raycast3D(const NkRay3D& ray, NkRayHit3D& hit, uint32 mask = 0xFFFFFFFFu) const;
                bool   Raycast2D(const NkRay2D& ray, NkRayHit2D& hit, uint32 mask = 0xFFFFFFFFu) const;

                // Requête d'overlap : tous les corps qui chevauchent la forme `s`
                // (broadphase AABB + narrowphase). Remplit `out` avec leurs ids, renvoie le nombre.
                uint32 Overlap(const NkShape& s, NkVector<uint32>& out, uint32 mask = 0xFFFFFFFFu) const;

                // Shape cast : translate la forme convexe `shape` le long de `dir` (unité) sur
                // `maxDist` et renvoie le 1er contact (TOI) avec un corps convexe. `hit.t` = distance.
                bool   ShapeCast(const NkShape& shape, const NkVec3f& dir, float32 maxDist,
                                 NkRayHit3D& hit, uint32 mask = 0xFFFFFFFFu) const;

                // CCD : balaie le corps `id` (convexe) de son `translation` et renvoie le 1er
                // contact (TOI) avec un AUTRE corps convexe. `hit.t` = distance avant impact.
                // Anti-tunneling : la physique au-dessus clampe le déplacement à hit.t.
                bool   SweepBody(uint32 id, const NkVec3f& translation, NkRayHit3D& hit,
                                 uint32 mask = 0xFFFFFFFFu) const;

                uint32 BodyCount() const noexcept { return (uint32)mBodies.Size(); }

            private:
                static bool Narrow(const NkShape& A, const NkShape& B, NkManifold3D& out) noexcept;

                NkVector<NkBody>           mBodies;
                NkVector<NkCollisionPair>  mPairs;
                NkVector<NkCollisionPair>  mPrevPairs; // paires de la frame précédente (warm-start)
                NkVector<NkCollisionEvent> mEnter, mStay, mExit;
                NkVector<nkentseu::uint64> mPrevKeys;   // clés de paires de la frame précédente
                mutable NkDbvh             mTree;       // broadphase persistante (DBVH)
                uint32                     mNextId = 1u;
        };

    } // namespace collision
} // namespace nkentseu
