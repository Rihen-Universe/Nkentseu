#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkParticleStoreGPU.h — stockage GPU de l'état des particules ORDINAIRES (2026-09-05)
// Plan (B), étapes b-c (Engine/Noge/DECISIONS_RODOLF.md) : l'état vit dans des tampons de
// stockage (état, naissances, instances) et un noyau NkSL de calcul l'intègre ; le dessin
// lit le MÊME tampon d'instances que le stockage CPU (24 o par emplacement, créé
// STORAGE | VERTEX) -- un seul chemin de dessin, aucune copie.
//
// Qui naît reste décidé par le CPU (NkParticleBirth). Et parce que c'est le CPU qui a
// choisi la VIE de chaque particule, il tient lui-même l'horloge des morts : la vie
// décroît de dt par pas, ici comme dans le noyau, avec les mêmes opérations en float32.
// Conséquence : les emplacements libres, le nombre de vivantes et l'emplacement de chaque
// naissance sont EXACTS côté CPU sans aucune relecture -- pas d'atomique dans le noyau,
// deux dispatchs (les naissances vers leurs emplacements, puis l'intégration de tous les
// emplacements), un résultat déterministe. Le plan promettait AliveCount « une image plus
// tard » ; il est exact.
//
// Une morte garde son emplacement avec une taille 0 : DrawCount() = capacité, le quad
// dégénère. Un solveur (SPH) force le stockage CPU : le SPH GPU vient après.
// OpenGL est le seul dorsal où ce chemin est ÉPROUVÉ à l'image (05/09) ; les autres
// compilent le noyau par la même chaîne que NkIBLCompute (HLSL / SPIR-V / MSL), non mesurés.
// =============================================================================
#include "NkParticleStore.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
	namespace renderer {

		class NkParticleStoreGPU final : public NkIParticleStore {
			public:
				bool Init(NkIDevice *device, const NkEmitterDesc &desc) override;
				void Shutdown(NkIDevice *device) override;
				void Spawn(const NkParticleBirth *births, uint32 n) override;
				void Step(NkICommandBuffer *cmd, const NkEmitterDesc &desc, float32 dt,
						  NkParticleStepStats &stats) override;
				NkBufferHandle InstanceBuffer() const override {
					return mInstances;
				}
				uint32 DrawCount() const override {
					return mCapacity; // les mortes ont une taille 0
				}
				uint32 AliveCount() const override {
					return mAliveCount; // exact : le CPU tient l'horloge des vies
				}
				bool IsGPU() const override {
					return true;
				}
				// Pourquoi Init a refusé (compute absent, NkSL, shader, pipeline, tampon) : l'appelant le dit.
				const char *FailReason() const {
					return mFail;
				}

			private:
				// Une naissance telle que le noyau la lit : 3 x vec4 (48 o), l'emplacement en float (exact < 2^24).
				struct GpuBirth {
						NkVec4f posLife;
						NkVec4f velRot;
						NkVec4f slotRotSpeed;
				};
				// Paramètres du noyau (64 o, std140) : mode 0 = naissances, 1 = intégration.
				struct Params {
						NkVec4f gravityDt;
						NkVec4f colorStart;
						NkVec4f colorEnd;
						float32 sizeStart = 0.f, sizeEnd = 0.f;
						uint32 count = 0, mode = 0;
				};
				bool CompileKernel();

				NkIDevice *mDevice = nullptr;
				uint32 mCapacity = 0;
				NkBufferHandle mState;		// 3 x vec4 par emplacement : (pos, vie) (vitesse, vie totale) (rotation, vitesse de rotation, vivante, 0)
				NkBufferHandle mBirths;		// GpuBirth x capacité
				NkBufferHandle mInstances;	// NkParticleInstance x capacité, STORAGE | VERTEX : le dessin le lit tel quel
				NkBufferHandle mParamsBirth, mParamsSim;
				::nkentseu::NkShaderHandle mShader; // celui du RHI -- renderer::NkShaderHandle est un AUTRE type (piege dit dans CLAUDE.md)
				NkPipelineHandle mPipe;
				NkDescSetHandle mLayout, mSetBirth, mSetSim;
				NkICommandBuffer *mCmd = nullptr;
				NkVector<GpuBirth> mPending; // les naissances de l'image
				// L'horloge des vies, côté CPU : miroir exact du noyau.
				NkVector<float32> mLife;
				NkVector<uint8> mAlive;
				NkVector<uint32> mFree;
				uint32 mAliveCount = 0;
				const char *mFail = "";
		};

	} // namespace renderer
} // namespace nkentseu
