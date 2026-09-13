#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkParticleStore.h — le STOCKAGE de l'état des particules d'un émetteur (2026-09-04)
//
// Décision de Rodolf (04/09) : cible de simulation PAR ÉMETTEUR
// (NkEmitterDesc::simTarget = AUTO | CPU | GPU), l'état derrière une interface
// de stockage (CPU SoA / GPU SSBO), UN SEUL chemin de dessin. Le dessin ne
// connaît que InstanceBuffer() + DrawCount() : un tampon de NkParticleInstance
// (24 o, binding 1, par instance) et le nombre d'instances à tirer.
//
// Qui décide QUI naît : toujours le CPU (formes d'émission, aléa, Burst) — une
// seule sémantique de NkEmitterDesc pour les deux cibles ; le CPU pousse des
// NkParticleBirth, jamais l'état.
//
// Ce fichier livre le stockage CPU (SoA). Le stockage GPU (trois SSBO + noyau
// NkSL, plan (B) dans Engine/Noge/DECISIONS_RODOLF.md) n'est PAS livré :
// un émetteur qui le demande retombe sur le CPU et le DIT.
//
// Un solveur (NkIParticleSolver) peut remplacer la gravité du stockage CPU par
// ses propres forces avant l'intégration — c'est la porte d'entrée du SPH
// (NkSPHSolver), sur le MÊME stockage, le MÊME dessin.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	class NkICommandBuffer;
	namespace renderer {

		struct NkEmitterDesc;
		class NkParticleStoreCPU;
		class NkIParticleStore;

		// Ce que le CPU décide à la naissance d'une particule.
		struct NkParticleBirth {
				NkVec3f pos = {0, 0, 0};
				NkVec3f vel = {0, 0, 0};
				float32 life = 1.f; // durée de vie totale (s)
				float32 rotation = 0.f;
				float32 rotSpeed = 0.f;
		};

		// Ce qu'un pas de simulation rapporte (les postes de NkVFXSystem::Profile).
		struct NkParticleStepStats {
				float32 simMs = 0.f, buildMs = 0.f, uploadMs = 0.f;
				uint32 uploadBytes = 0;
				uint32 alive = 0;
		};

		// Un solveur de forces sur le stockage CPU : appelé AVANT l'intégration des
		// positions, à la place de la gravité du stockage. Le SPH en est un.
		class NkIParticleSolver {
			public:
				virtual ~NkIParticleSolver() = default;
				virtual void Apply(NkParticleStoreCPU &store, const NkEmitterDesc &desc, float32 dt) = 0;
				// Un solveur qui sait vivre sur le GPU fournit SON stockage (2026-09-05, le SPH) : l'appelant
				// l'initialise, et retombe sur le CPU en le disant si Init refuse. Nul = CPU seulement.
				virtual NkIParticleStore *CreateGPUStore(NkIDevice *device, const NkEmitterDesc &desc) {
					(void)device;
					(void)desc;
					return nullptr;
				}
		};

		class NkIParticleStore {
			public:
				virtual ~NkIParticleStore() = default;
				virtual bool Init(NkIDevice *device, const NkEmitterDesc &desc) = 0;
				virtual void Shutdown(NkIDevice *device) = 0;
				virtual void Spawn(const NkParticleBirth *births, uint32 n) = 0;
				// CPU : boucle d'intégration + construction des instances + envoi.
				// GPU (plan) : envoi des naissances + Dispatch ; `cmd` peut être nul sur CPU.
				virtual void Step(NkICommandBuffer *cmd, const NkEmitterDesc &desc, float32 dt,
								  NkParticleStepStats &stats) = 0;
				virtual NkBufferHandle InstanceBuffer() const = 0; // NkParticleInstance × DrawCount(), binding 1
				virtual uint32 DrawCount() const = 0;			   // instances à tirer (CPU : vivantes)
				virtual uint32 AliveCount() const = 0;			   // GPU (plan) : valeur de l'image précédente
				virtual bool IsGPU() const = 0;
		};

		// =========================================================================
		// Stockage CPU, structure de tableaux (SoA) : une colonne par attribut, un
		// indice = une particule. `alive` dit si la colonne est occupée ; la pile
		// `freeSlots` donne un emplacement libre en O(1) (mesuré le 04/09 : le
		// balayage linéaire coûtait 6 à 793 ms par image à 50 000).
		// =========================================================================
		class NkParticleStoreCPU final : public NkIParticleStore {
			public:
				// ── colonnes ───────────────────────────────────────────────────
				NkVector<NkVec3f> pos, vel;
				NkVector<float32> life, maxLife, size, rotation, rotSpeed;
				NkVector<NkVec4f> color;
				NkVector<uint8> alive;
				NkVector<uint32> freeSlots;
				uint32 capacity = 0;

				// Le solveur (SPH…) : nul = gravité de NkEmitterDesc, comme avant.
				NkIParticleSolver *solver = nullptr;

				// ── NkIParticleStore ─────────────────────────────────────────
				bool Init(NkIDevice *device, const NkEmitterDesc &desc) override;
				void Shutdown(NkIDevice *device) override;
				void Spawn(const NkParticleBirth *births, uint32 n) override;
				void Step(NkICommandBuffer *cmd, const NkEmitterDesc &desc, float32 dt,
						  NkParticleStepStats &stats) override;
				NkBufferHandle InstanceBuffer() const override {
					return mVbo;
				}
				uint32 DrawCount() const override {
					return mAliveCount;
				}
				uint32 AliveCount() const override {
					return mAliveCount;
				}
				bool IsGPU() const override {
					return false;
				}

			private:
				NkIDevice *mDevice = nullptr;
				NkBufferHandle mVbo;						 // NkParticleInstance × capacity, binding 1
				NkVector<NkParticleInstance> mScratch; // réutilisé : pas de réallocation par image
				uint32 mAliveCount = 0;
				float32 mTime = 0.f; // horloge du vent (bruits qui dérivent)
		};

	} // namespace renderer
} // namespace nkentseu
