// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Noge/ECS/Systems/NkFluidVolumeSystem.cpp — pont ECS -> registre de fluides.
// Voir l'en-tête pour le partage des rôles (qui déclare, qui fait avancer).
// =============================================================================
#include "NkFluidVolumeSystem.h"

namespace nkentseu {

	using namespace ecs;
	using namespace renderer;
	using namespace math;

	void NkFluidVolumeSystem::Execute(ecs::NkWorld &world, float32 dt) noexcept {
		(void)dt; // le pas de simulation appartient à NkVFXSystem::Update, pas ici
		mFollowed = 0;
		if (mStore == nullptr)
			return;

		// Les poignées réclamées CETTE image. On ne ramasse que ce que CE système a
		// créé : un registre peut porter des volumes venus d'ailleurs, et une liste
		// d'exclusion ne protège que ce qu'on a pensé à y mettre.
		NkVector<uint64> reclamees;

		world.Query<NkFluidVolume, NkTransform>().ForEach([&](NkEntityId, NkFluidVolume &fv, const NkTransform &tf) {
			const NkVec3f pos = tf.GetWorldPosition();

			// Recréation si la configuration a changé.
			if (fv.dirty && fv.volumeId != 0) {
				NkFluidVolumeId old{fv.volumeId};
				mStore->Destroy(old);
				fv.volumeId = 0;
				fv.dirty = false;
			}

			// Création paresseuse.
			if (fv.volumeId == 0) {
				if (!fv.enabled)
					return;
				const NkFluidVolumeId id = mStore->Create(fv.desc, pos);
				if (!id.IsValid())
					return; // le registre a refusé et l'a dit : on ne fabrique pas de poignée
				fv.volumeId = id.id;
				++mCreated;
				reclamees.PushBack(id.id);
				return;
			}

			// Suivi du transform + activation.
			const NkFluidVolumeId id{fv.volumeId};
			mStore->SetCenter(id, pos);
			mStore->SetEnabled(id, fv.enabled);
			++mFollowed;
			reclamees.PushBack(fv.volumeId);
		});

		// Ramassage des ORPHELINS : les volumes que ce système a créés et qu'aucune
		// entité ne réclame plus (composant retiré, entité détruite). Sans ça, un
		// volume survivrait à son entité et continuerait d'être simulé, invisible.
		for (uint32 i = 0; i < mOwned.Size();) {
			bool vivant = false;
			for (uint32 j = 0; j < reclamees.Size(); ++j)
				if (reclamees[j] == mOwned[i]) {
					vivant = true;
					break;
				}
			if (vivant) {
				++i;
				continue;
			}
			NkFluidVolumeId mort{mOwned[i]};
			mStore->Destroy(mort);
			mOwned.RemoveAt(i);
			++mReaped;
		}
		// Ce que ce système possède désormais.
		mOwned.Clear();
		for (uint32 j = 0; j < reclamees.Size(); ++j)
			mOwned.PushBack(reclamees[j]);
	}

} // namespace nkentseu
