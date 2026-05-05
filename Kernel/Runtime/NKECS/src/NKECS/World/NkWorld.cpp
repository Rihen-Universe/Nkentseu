// =============================================================================
// World/NkWorld.cpp — Implémentation des méthodes non-inline de NkWorld
// =============================================================================
#include "NKECS/World/NkWorld.h"
#include "NKECS/Storage/NkArchetypeGraph.h"

namespace nkentseu {
    namespace ecs {

        NkWorld::NkWorld() noexcept {
            // Le graphe d'archétypes et l'index d'entités s'initialisent
            // automatiquement via leurs constructeurs par défaut.
        }

        void NkWorld::Destroy(NkEntityId id) noexcept {
            if (!mEntityIndex.IsAlive(id)) {
                return;
            }

            NkEntityRecord* rec = mEntityIndex.GetRecord(id);
            if (rec && rec->archetypeId != kInvalidArchetypeId) {
                NkArchetype* arch = mGraph.Get(rec->archetypeId);
                if (arch) {
                    // Swap-remove : l'entité en dernière position est déplacée
                    const NkEntityId swapped = arch->RemoveEntity(rec->row);
                    if (swapped.IsValid()) {
                        // Mettre à jour l'index de l'entité déplacée
                        mEntityIndex.SetRecord(swapped, rec->archetypeId, rec->row);
                    }
                }
            }

            // Libère l'identifiant (incrémente la génération)
            mEntityIndex.Free(id);
        }

        void NkWorld::DestroyDeferred(NkEntityId id) noexcept {
            mDeferredOps.PushBack({
                DeferredOp::Kind::Destroy,
                id,
                kInvalidComponentId,
                NkFunction<void(NkWorld&, NkEntityId)>(
                    [](NkWorld& w, NkEntityId eid) { w.Destroy(eid); })
            });
        }

        void NkWorld::FlushDeferred() noexcept {
            // Exécute les opérations dans l'ordre de soumission
            for (nk_usize i = 0; i < mDeferredOps.Size(); ++i) {
                DeferredOp& op = mDeferredOps[i];
                if (op.fn) {
                    op.fn(*this, op.entity);
                }
            }
            mDeferredOps.Clear();
        }

        void NkWorld::RemoveImpl(NkEntityId id, NkComponentId cid) noexcept {
            if (!mEntityIndex.IsAlive(id)) {
                return;
            }

            NkEntityRecord* rec = mEntityIndex.GetRecord(id);
            if (!rec || rec->archetypeId == kInvalidArchetypeId) {
                return; // Aucun composant → rien à faire
            }

            NkArchetype* oldArch = mGraph.Get(rec->archetypeId);
            if (!oldArch || !oldArch->Has(cid)) {
                return; // Le composant n'existe pas sur cette entité
            }

            // Calcule l'archétype cible (sans ce composant)
            const NkArchetypeId oldArchId = rec->archetypeId;
            const uint32        oldRow    = rec->row;
            const NkArchetypeId newArchId = mGraph.RemoveComponent(oldArchId, cid);

            if (newArchId == kInvalidArchetypeId) {
                // Plus aucun composant → l'entité retourne à l'état "vide"
                const NkEntityId swapped = oldArch->RemoveEntity(oldRow);
                if (swapped.IsValid()) {
                    mEntityIndex.SetRecord(swapped, oldArchId, oldRow);
                }
                mEntityIndex.SetRecord(id, kInvalidArchetypeId, 0xFFFFFFFFu);
                return;
            }

            NkArchetype* newArch = mGraph.Get(newArchId);
            NKECS_ASSERT(newArch);

            // Migration : copie les composants restants dans le nouvel archétype
            const uint32 newRow = newArch->MigrateFrom(*oldArch, oldRow, id);

            // Suppression dans l'ancien archétype
            const NkEntityId swapped = oldArch->RemoveEntity(oldRow);
            if (swapped.IsValid()) {
                mEntityIndex.SetRecord(swapped, oldArchId, oldRow);
            }

            // Mise à jour de l'index
            mEntityIndex.SetRecord(id, newArchId, newRow);
        }

    } // namespace ecs
} // namespace nkentseu
