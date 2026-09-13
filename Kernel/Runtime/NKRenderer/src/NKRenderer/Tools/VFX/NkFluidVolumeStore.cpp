// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidVolumeStore.cpp — le registre des volumes de fluide (2026-09-06).
// Voir NkFluidVolumeStore.h pour le partage des rôles et la sémantique du
// déplacement. CPU pur : aucun device, aucun include de NKRHI.
// =============================================================================
#include "NkFluidVolumeStore.h"
#include "NKMemory/NkAllocator.h"
#include "NKTime/NkChrono.h" // en dernier (cf. NkVFXSystem.cpp : son namespace `time`)

namespace nkentseu {
	namespace renderer {

		using namespace math;

		NkFluidVolumeStore::~NkFluidVolumeStore() {
			Clear();
		}

		void NkFluidVolumeStore::Clear() {
			for (uint32 i = 0; i < mVolumes.Size(); ++i) {
				if (mVolumes[i] == nullptr)
					continue;
				if (mVolumes[i]->grid != nullptr)
					memory::NkGetDefaultAllocator().Delete(mVolumes[i]->grid);
				memory::NkGetDefaultAllocator().Delete(mVolumes[i]);
			}
			mVolumes.Clear();
			mSteppedLastFrame = 0;
		}

		NkFluidVolumeStore::Volume *NkFluidVolumeStore::Find(NkFluidVolumeId id) {
			if (!id.IsValid())
				return nullptr;
			for (uint32 i = 0; i < mVolumes.Size(); ++i)
				if (mVolumes[i] != nullptr && mVolumes[i]->id.id == id.id)
					return mVolumes[i];
			return nullptr;
		}

		const NkFluidVolumeStore::Volume *NkFluidVolumeStore::Find(NkFluidVolumeId id) const {
			if (!id.IsValid())
				return nullptr;
			for (uint32 i = 0; i < mVolumes.Size(); ++i)
				if (mVolumes[i] != nullptr && mVolumes[i]->id.id == id.id)
					return mVolumes[i];
			return nullptr;
		}

		// Les bornes du descripteur sont RELATIVES au centre ; on les recale en monde.
		void NkFluidVolumeStore::ApplyCenter(Volume &v) {
			if (v.grid == nullptr)
				return;
			NkFluidGridParams &p = v.grid->Params();
			p.boundsMin = {v.desc.grid.boundsMin.x + v.center.x, v.desc.grid.boundsMin.y + v.center.y,
						   v.desc.grid.boundsMin.z + v.center.z};
			p.boundsMax = {v.desc.grid.boundsMax.x + v.center.x, v.desc.grid.boundsMax.y + v.center.y,
						   v.desc.grid.boundsMax.z + v.center.z};
		}

		NkFluidVolumeId NkFluidVolumeStore::Create(const NkFluidVolumeDesc &desc, const NkVec3f &worldCenter) {
			Volume *v = memory::NkGetDefaultAllocator().New<Volume>();
			if (v == nullptr)
				return NkFluidVolumeId{};
			v->desc = desc;
			v->center = worldCenter;
			v->grid = memory::NkGetDefaultAllocator().New<NkFluidGrid>();
			if (v->grid == nullptr) {
				memory::NkGetDefaultAllocator().Delete(v);
				return NkFluidVolumeId{};
			}
			// Init veut des bornes MONDE : on recale avant, pas après.
			NkFluidGridParams p = desc.grid;
			p.boundsMin = {desc.grid.boundsMin.x + worldCenter.x, desc.grid.boundsMin.y + worldCenter.y,
						   desc.grid.boundsMin.z + worldCenter.z};
			p.boundsMax = {desc.grid.boundsMax.x + worldCenter.x, desc.grid.boundsMax.y + worldCenter.y,
						   desc.grid.boundsMax.z + worldCenter.z};
			if (!v->grid->Init(p)) {
				// Un refus se DIT : on rend une poignée invalide plutôt qu'une poignée
				// qui pointerait sur une grille non initialisée.
				memory::NkGetDefaultAllocator().Delete(v->grid);
				memory::NkGetDefaultAllocator().Delete(v);
				return NkFluidVolumeId{};
			}
			v->id.id = mNextId++;
			mVolumes.PushBack(v);
			return v->id;
		}

		void NkFluidVolumeStore::Destroy(NkFluidVolumeId &id) {
			if (!id.IsValid())
				return;
			for (uint32 i = 0; i < mVolumes.Size(); ++i) {
				if (mVolumes[i] == nullptr || mVolumes[i]->id.id != id.id)
					continue;
				if (mVolumes[i]->grid != nullptr)
					memory::NkGetDefaultAllocator().Delete(mVolumes[i]->grid);
				memory::NkGetDefaultAllocator().Delete(mVolumes[i]);
				mVolumes.RemoveAt(i);
				id.id = 0; // la poignée de l'appelant meurt avec l'objet
				return;
			}
			id.id = 0;
		}

		void NkFluidVolumeStore::SetCenter(NkFluidVolumeId id, const NkVec3f &worldCenter) {
			Volume *v = Find(id);
			if (v == nullptr)
				return;
			v->center = worldCenter;
			ApplyCenter(*v);
		}

		void NkFluidVolumeStore::SetEnabled(NkFluidVolumeId id, bool on) {
			Volume *v = Find(id);
			if (v != nullptr)
				v->desc.enabled = on;
		}

		void NkFluidVolumeStore::SetField(NkFluidVolumeId id, const NkIForceField *field, float32 particleMass) {
			Volume *v = Find(id);
			if (v == nullptr || v->grid == nullptr)
				return;
			v->desc.grid.field = field;
			v->desc.grid.fieldParticleMass = particleMass;
			v->grid->Params().field = field;
			v->grid->Params().fieldParticleMass = particleMass;
		}

		NkFluidGrid *NkFluidVolumeStore::Grid(NkFluidVolumeId id) {
			Volume *v = Find(id);
			return (v != nullptr) ? v->grid : nullptr;
		}

		const NkFluidGrid *NkFluidVolumeStore::Grid(NkFluidVolumeId id) const {
			const Volume *v = Find(id);
			return (v != nullptr) ? v->grid : nullptr;
		}

		NkVec3f NkFluidVolumeStore::Center(NkFluidVolumeId id) const {
			const Volume *v = Find(id);
			return (v != nullptr) ? v->center : NkVec3f{0.f, 0.f, 0.f};
		}

		NkFluidVolumeDesc *NkFluidVolumeStore::Desc(NkFluidVolumeId id) {
			Volume *v = Find(id);
			return (v != nullptr) ? &v->desc : nullptr;
		}

		void NkFluidVolumeStore::StepAll(float32 dt) {
			mSteppedLastFrame = 0;
			mLastStepMs = 0.f;
			if (dt <= 0.f)
				return;
			const int64 t0 = ::nkentseu::NkChrono::Now().nanoseconds;
			for (uint32 i = 0; i < mVolumes.Size(); ++i) {
				Volume *v = mVolumes[i];
				if (v == nullptr || v->grid == nullptr || !v->desc.enabled)
					continue;
				const NkFluidSourceDesc &s = v->desc.source;
				if (s.enabled && s.radius > 0.f) {
					// L'entité déclare un DÉBIT (par seconde) ; le pas de temps est
					// l'affaire du registre, jamais celle de l'appelant.
					const NkVec3f p = {v->center.x + s.offset.x, v->center.y + s.offset.y, v->center.z + s.offset.z};
					v->grid->EmitSphere(p, s.radius, s.densityRate * dt, s.temperatureRate * dt, s.fuelRate * dt);
				}
				v->grid->Step(dt);
				// APRÈS le pas, jamais avant : un compteur d'intentions ne dirait pas si
				// le pas a eu lieu (porte « un compteur d'intentions n'est pas un
				// contrôle d'effets »).
				++mSteppedLastFrame;
				++mStepsTotal;
			}
			mLastStepMs = (float32)((::nkentseu::NkChrono::Now().nanoseconds - t0) / 1.0e6);
		}

	} // namespace renderer
} // namespace nkentseu
