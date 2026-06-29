#pragma once
// =============================================================================
// NkCollisionWorld.h — Monde de collision (ZÉRO STL). Corps + broadphase +
// narrowphase + requêtes (raycast, overlap). 2D et 3D (un corps est l'un OU
// l'autre selon sa forme ; les paires mixtes 2D/3D sont ignorées).
// Conteneurs = NKContainers (NkVector). v1 broadphase = balayage O(n²) avec marge ;
// SAP/grille/BVH = phase suivante (architecture prête).
// =============================================================================
#include "NKCollision/NkColTests.h"
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
        };

        // Paire en contact. Manifold universel 3D (les contacts 2D ont z=0).
        struct NkCollisionPair {
            uint32       a = 0;
            uint32       b = 0;
            NkManifold3D manifold;
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

                // Marquer un corps comme trigger (zone de détection).
                void   SetTrigger(uint32 id, bool t) { if (NkBody* b = GetBody(id)) b->trigger = t; }

                // Raycast : renvoie le hit le PLUS PROCHE (filtré par mask de layers).
                bool   Raycast3D(const NkRay3D& ray, NkRayHit3D& hit, uint32 mask = 0xFFFFFFFFu) const;
                bool   Raycast2D(const NkRay2D& ray, NkRayHit2D& hit, uint32 mask = 0xFFFFFFFFu) const;

                uint32 BodyCount() const noexcept { return (uint32)mBodies.Size(); }

            private:
                static bool Narrow(const NkShape& A, const NkShape& B, NkManifold3D& out) noexcept;

                NkVector<NkBody>           mBodies;
                NkVector<NkCollisionPair>  mPairs;
                NkVector<NkCollisionEvent> mEnter, mStay, mExit;
                NkVector<nkentseu::uint64> mPrevKeys;   // clés de paires de la frame précédente
                uint32                     mNextId = 1u;
        };

    } // namespace collision
} // namespace nkentseu
