// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkParticleStore.cpp — stockage CPU (SoA) de l'état des particules (2026-09-04)
// Voir NkParticleStore.h. Le comportement est celui que NkVFXSystem portait en
// tableau de structures avant ce fichier : mêmes formules d'intégration, même
// interpolation couleur/taille, même pile d'emplacements libres, même
// construction d'un NkParticleInstance de 24 o par vivante — déplacé derrière
// l'interface, pas réécrit. Les chiffres du lot A doivent rester les mêmes.
// =============================================================================
#include "NkParticleStore.h"
#include "NkVFXSystem.h"
#include "NKTime/NkChrono.h"

namespace nkentseu {
	namespace renderer {

		bool NkParticleStoreCPU::Init(NkIDevice *device, const NkEmitterDesc &desc) {
			mDevice = device;
			capacity = desc.maxParticles;
			pos.Resize(capacity);
			vel.Resize(capacity);
			life.Resize(capacity);
			maxLife.Resize(capacity);
			size.Resize(capacity);
			rotation.Resize(capacity);
			rotSpeed.Resize(capacity);
			color.Resize(capacity);
			alive.Resize(capacity);
			for (uint32 i = 0; i < capacity; ++i)
				alive[i] = 0;
			freeSlots.Clear();
			freeSlots.Reserve(capacity);
			for (uint32 i = capacity; i > 0; --i)
				freeSlots.PushBack(i - 1); // l'emplacement 0 sort en premier
			mAliveCount = 0;
			// Tampon PAR INSTANCE : NkParticleInstance (24 o) × capacité (lot A, 04/09).
			mVbo = mDevice ? mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)capacity * sizeof(NkParticleInstance)))
						   : NkBufferHandle{};
			return mDevice == nullptr || mVbo.IsValid();
		}

		void NkParticleStoreCPU::Shutdown(NkIDevice *device) {
			if (device && mVbo.IsValid())
				device->DestroyBuffer(mVbo);
			mVbo = {};
			mDevice = nullptr;
		}

		void NkParticleStoreCPU::Spawn(const NkParticleBirth *births, uint32 n) {
			for (uint32 k = 0; k < n; ++k) {
				if (freeSlots.Empty())
					return; // plein : la naissance est perdue, comme avant
				const uint32 i = freeSlots.Back();
				freeSlots.PopBack();
				alive[i] = 1;
				pos[i] = births[k].pos;
				vel[i] = births[k].vel;
				life[i] = births[k].life;
				maxLife[i] = births[k].life;
				rotation[i] = births[k].rotation;
				rotSpeed[i] = births[k].rotSpeed;
				// taille et couleur de départ : interpolées au premier pas depuis desc
				size[i] = 0.f;
				color[i] = {1, 1, 1, 1};
				++mAliveCount;
			}
		}

		void NkParticleStoreCPU::Step(NkICommandBuffer *cmd, const NkEmitterDesc &desc, float32 dt,
									  NkParticleStepStats &stats) {
			(void)cmd;
			const int64 t1 = ::nkentseu::NkChrono::Now().nanoseconds;
			mAliveCount = 0;
			// Pointeurs bruts pour la boucle chaude : en Debug, NkVector::operator[] est un
			// appel par acces -- mesure du 04/09 : 3,5-4,6 ms d'integration a 50 000 contre
			// 1,4-1,9 avec une reference sur la structure d'avant.
			NkVec3f *P = pos.Data(), *V = vel.Data();
			float32 *L = life.Data(), *ML = maxLife.Data(), *S = size.Data(), *R = rotation.Data(), *RS = rotSpeed.Data();
			NkVec4f *C = color.Data();
			uint8 *A = alive.Data();
			if (!solver) {
				// UNE passe (mesure du 04/09 : trois passes sur `capacity` doublaient
				// l'integration, 1,4-1,9 -> 3,6 ms a 50 000) : vie, gravite, position,
				// rotation, couleur, taille -- les memes formules qu'avant.
				const NkVec3f g = desc.gravity;
				for (uint32 i = 0; i < capacity; ++i) {
					if (!A[i])
						continue;
					L[i] -= dt;
					if (L[i] <= 0.f) {
						A[i] = 0;
						freeSlots.PushBack(i);
						continue;
					}
					V[i].x += g.x * dt;
					V[i].y += g.y * dt;
					V[i].z += g.z * dt;
					P[i].x += V[i].x * dt;
					P[i].y += V[i].y * dt;
					P[i].z += V[i].z * dt;
					R[i] += RS[i] * dt;
					const float32 t = 1.f - (L[i] / ML[i]);
					C[i].x = desc.colorStart.x + (desc.colorEnd.x - desc.colorStart.x) * t;
					C[i].y = desc.colorStart.y + (desc.colorEnd.y - desc.colorStart.y) * t;
					C[i].z = desc.colorStart.z + (desc.colorEnd.z - desc.colorStart.z) * t;
					C[i].w = desc.colorStart.w + (desc.colorEnd.w - desc.colorStart.w) * t;
					S[i] = desc.sizeStart + (desc.sizeEnd - desc.sizeStart) * t;
					++mAliveCount;
				}
			} else {
				// Avec un solveur (SPH) : les mortes rendent leur emplacement AVANT lui
				// (il ne voit que des vivantes) ; il integre LUI-MEME vitesses et
				// positions (sous-pas) ; ici : rotation, couleur, taille, compte.
				for (uint32 i = 0; i < capacity; ++i) {
					if (!A[i])
						continue;
					L[i] -= dt;
					if (L[i] <= 0.f) {
						A[i] = 0;
						freeSlots.PushBack(i);
					}
				}
				solver->Apply(*this, desc, dt);
				for (uint32 i = 0; i < capacity; ++i) {
					if (!A[i])
						continue;
					R[i] += RS[i] * dt;
					const float32 t = 1.f - (L[i] / ML[i]);
					C[i].x = desc.colorStart.x + (desc.colorEnd.x - desc.colorStart.x) * t;
					C[i].y = desc.colorStart.y + (desc.colorEnd.y - desc.colorStart.y) * t;
					C[i].z = desc.colorStart.z + (desc.colorEnd.z - desc.colorStart.z) * t;
					C[i].w = desc.colorStart.w + (desc.colorEnd.w - desc.colorStart.w) * t;
					S[i] = desc.sizeStart + (desc.sizeEnd - desc.sizeStart) * t;
					++mAliveCount;
				}
			}
			const int64 t2 = ::nkentseu::NkChrono::Now().nanoseconds;
			stats.simMs += (float32)((t2 - t1) / 1.0e6);
			stats.alive += mAliveCount;
			if (mAliveCount == 0 || !mVbo.IsValid())
				return;
			// 4) UN enregistrement de 24 o par vivante (lot A) — tampon réutilisé.
			mScratch.Clear();
			mScratch.Reserve(mAliveCount);
			for (uint32 i = 0; i < capacity; ++i) {
				if (!A[i])
					continue;
				NkParticleInstance r;
				r.pos = P[i];
				r.size = S[i];
				r.color = ((uint32)(C[i].w * 255) << 24) | ((uint32)(C[i].z * 255) << 16) |
						  ((uint32)(C[i].y * 255) << 8) | (uint32)(C[i].x * 255);
				r.rotation = R[i];
				mScratch.PushBack(r);
			}
			const int64 t3 = ::nkentseu::NkChrono::Now().nanoseconds;
			stats.buildMs += (float32)((t3 - t2) / 1.0e6);
			mDevice->WriteBuffer(mVbo, mScratch.Data(), (uint32)mScratch.Size() * sizeof(NkParticleInstance));
			const int64 t4 = ::nkentseu::NkChrono::Now().nanoseconds;
			stats.uploadMs += (float32)((t4 - t3) / 1.0e6);
			stats.uploadBytes += (uint32)mScratch.Size() * (uint32)sizeof(NkParticleInstance);
		}

	} // namespace renderer
} // namespace nkentseu
