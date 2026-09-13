// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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

			NkEntityRecord *rec = mEntityIndex.GetRecord(id);
			if (rec && rec->archetypeId != kInvalidArchetypeId) {
				NkArchetype *arch = mGraph.Get(rec->archetypeId);
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
			mDeferredOps.PushBack(
				{DeferredOp::Kind::Destroy, id, kInvalidComponentId,
				 NkFunction<void(NkWorld &, NkEntityId)>([](NkWorld &w, NkEntityId eid) { w.Destroy(eid); })});
		}

		void NkWorld::FlushDeferred() noexcept {
			// Exécute les opérations dans l'ordre de soumission
			for (nk_usize i = 0; i < mDeferredOps.Size(); ++i) {
				DeferredOp &op = mDeferredOps[i];
				if (op.fn) {
					op.fn(*this, op.entity);
				}
			}
			mDeferredOps.Clear();
		}

		// =====================================================================
		// « LA BRIQUE » — composants sans le type concret (AJOUT 2026-09-13)
		// =====================================================================
		// AddRaw refait, sans patron, exactement le chemin de AddImpl<T> :
		//   CAS 1 le composant est deja la  -> on ecrase la valeur en place
		//   CAS 2 il n'y est pas            -> migration d'archetype puis ecriture
		// La seule difference est la source des octets : au lieu de `*ptr = value`
		// (qui exige T), on passe par les hooks de ComponentMeta que le registre
		// remplit deja pour CHAQUE type enregistre. Aucune allocation, aucune
		// exception, aucun STL.

		bool NkWorld::AddRaw(NkEntityId id, NkComponentId cid, const void *src) noexcept {
			// ── Refus propre : type non enregistre ──────────────────────
			// IMPORTANT : ce test vient AVANT toute mutation. Sans lui, le
			// constructeur de NkArchetype construirait une pool a partir d'un
			// ComponentMeta nul — ecriture sauvage garantie.
			const ComponentMeta *meta = NkTypeRegistry::Global().Get(cid);
			if (meta == nullptr || meta->id == kInvalidComponentId || cid >= kMaxComponentTypes) {
				return false;
			}

			if (!mEntityIndex.IsAlive(id)) {
				return false;
			}

			NkEntityRecord *rec = mEntityIndex.GetRecord(id);
			if (rec == nullptr) {
				return false;
			}

			// ── CAS 1 : composant deja present -> ecriture en place ─────
			if (rec->archetypeId != kInvalidArchetypeId) {
				NkArchetype *arch = mGraph.Get(rec->archetypeId);
				if (arch != nullptr && arch->Has(cid)) {
					WriteRawInto(*arch, rec->row, *meta, src);
					return true;
				}
			}

			// ── CAS 2 : nouveau composant -> migration d'archetype ──────
			const NkArchetypeId oldArchId = rec->archetypeId;
			const uint32 oldRow = rec->row;

			const NkArchetypeId newArchId = (oldArchId == kInvalidArchetypeId)
												? mGraph.GetOrCreate(SingleMask(cid))
												: mGraph.AddComponent(oldArchId, cid);

			NkArchetype *newArch = mGraph.Get(newArchId);
			if (newArch == nullptr) {
				return false;
			}

			uint32 newRow = 0;

			if (oldArchId != kInvalidArchetypeId) {
				NkArchetype *oldArch = mGraph.Get(oldArchId);
				newRow = newArch->MigrateFrom(*oldArch, oldRow, id);

				const NkEntityId swapped = oldArch->RemoveEntity(oldRow);
				if (swapped.IsValid()) {
					mEntityIndex.SetRecord(swapped, oldArchId, oldRow);
				}
			} else {
				newRow = newArch->AddEntity(id);
			}

			mEntityIndex.SetRecord(id, newArchId, newRow);

			WriteRawInto(*newArch, newRow, *meta, src);
			return true;
		}

		void NkWorld::WriteRawInto(NkArchetype &arch, uint32 row, const ComponentMeta &meta,
								   const void *src) noexcept {
			// Un tag (taille nulle) n'a pas de slot : sa seule donnee est le bit
			// de masque, deja pose par la migration. Rien a ecrire.
			if (meta.isZeroSize || src == nullptr) {
				return;
			}

			NkComponentPool *pool = arch.GetPool(meta.id);
			if (pool == nullptr) {
				return;
			}

			void *dst = pool->At(row);
			if (dst == nullptr || dst == src) {
				return;
			}

			// Le slot contient un objet DEJA construit par defaut (AddEntity /
			// MigrateFrom appellent PushDefault). L'equivalent type-erase de
			// l'affectation `*ptr = value` est donc : detruire puis copier-
			// construire. Pour un type trivial les deux hooks se reduisent a un
			// memcpy ; pour un type non trivial, c'est la seule sequence qui ne
			// fuit pas.
			if (meta.destruct != nullptr) {
				meta.destruct(dst, 1);
			}
			if (meta.copyConstruct != nullptr) {
				meta.copyConstruct(dst, src, 1);
			}
		}

		void *NkWorld::GetRaw(NkEntityId id, NkComponentId cid) noexcept {
			if (cid >= kMaxComponentTypes || !mEntityIndex.IsAlive(id)) {
				return nullptr;
			}

			const NkEntityRecord *rec = mEntityIndex.GetRecord(id);
			if (rec == nullptr || rec->archetypeId == kInvalidArchetypeId) {
				return nullptr;
			}

			NkArchetype *arch = mGraph.Get(rec->archetypeId);
			if (arch == nullptr || !arch->Has(cid)) {
				return nullptr;
			}

			NkComponentPool *pool = arch->GetPool(cid);
			return (pool != nullptr) ? pool->At(rec->row) : nullptr;
		}

		const void *NkWorld::GetRaw(NkEntityId id, NkComponentId cid) const noexcept {
			// Meme chemin, en lecture seule. NkWorld n'expose pas de Get const
			// sur le graphe : on delegue a la version non-const via un cast
			// explicite, comme le fait deja Get<T>() const par le biais du
			// NkArchetype const.
			return const_cast<NkWorld *>(this)->GetRaw(id, cid);
		}

		bool NkWorld::HasRaw(NkEntityId id, NkComponentId cid) const noexcept {
			if (cid >= kMaxComponentTypes || !mEntityIndex.IsAlive(id)) {
				return false;
			}

			const NkEntityRecord *rec = mEntityIndex.GetRecord(id);
			if (rec == nullptr || rec->archetypeId == kInvalidArchetypeId) {
				return false;
			}

			const NkArchetype *arch = mGraph.Get(rec->archetypeId);
			return arch != nullptr && arch->Has(cid);
		}

		bool NkWorld::RemoveRaw(NkEntityId id, NkComponentId cid) noexcept {
			if (!HasRaw(id, cid)) {
				return false;
			}
			RemoveImpl(id, cid);
			return true;
		}

		void NkWorld::RemoveImpl(NkEntityId id, NkComponentId cid) noexcept {
			if (!mEntityIndex.IsAlive(id)) {
				return;
			}

			NkEntityRecord *rec = mEntityIndex.GetRecord(id);
			if (!rec || rec->archetypeId == kInvalidArchetypeId) {
				return; // Aucun composant → rien à faire
			}

			NkArchetype *oldArch = mGraph.Get(rec->archetypeId);
			if (!oldArch || !oldArch->Has(cid)) {
				return; // Le composant n'existe pas sur cette entité
			}

			// Calcule l'archétype cible (sans ce composant)
			const NkArchetypeId oldArchId = rec->archetypeId;
			const uint32 oldRow = rec->row;
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

			NkArchetype *newArch = mGraph.Get(newArchId);
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
