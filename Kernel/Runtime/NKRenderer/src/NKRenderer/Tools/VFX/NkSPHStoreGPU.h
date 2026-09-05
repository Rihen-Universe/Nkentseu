#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkSPHStoreGPU.h — le fluide DFSPH sur GPU (2026-09-05), derrière NkIParticleStore.
// Le SPH CPU (NkSPHSolver) tient au repos et donne un front juste depuis le 04/09 ;
// il plafonne à 500-780 ms/image pour 50 000 particules. Ici, les MÊMES étapes, une
// passe = un noyau NkSL :
//   1. grille de voisinage par TRI (Green, « Particle Simulation using CUDA », NVIDIA
//      2010, § « Building the Grid using Sorting ») : clé de cellule par particule, tri
//      bitonique (aucun atomique : NkSL n'en a pas sur les tampons), début/fin de
//      cellule là où la clé change. Les fantômes de paroi (Akinci) sont triés UNE fois
//      dans leur propre grille ; le fluide est retrié à chaque sous-pas ;
//   2. densité et alpha ; 3. divergence nulle itérée ; 4. XSPH + Morris (Jacobi, lu sur
//      tampon) + gravité ; 5. démarrage à chaud puis densité constante itérée ;
//   6. intégration, filets, instances (le dessin lit le tampon d'instances tel quel).
// Le résidu de chaque itération est réduit en 256 partiels et RELU une fois par
// itération (une synchronisation par itération, dite dans le coût) : le nombre
// d'itérations est celui que le résidu décide, comme sur CPU.
// Les paramètres et les statistiques sont ceux du NkSPHSolver propriétaire : la sonde
// lit Stats() sans savoir où le fluide a été calculé. Seul OpenGL est éprouvé.
// =============================================================================
#include "NkParticleStore.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
	namespace renderer {

		class NkSPHSolver;

		class NkSPHStoreGPU final : public NkIParticleStore {
			public:
				explicit NkSPHStoreGPU(NkSPHSolver *owner) : mOwner(owner) {}
				bool Init(NkIDevice *device, const NkEmitterDesc &desc) override;
				void Shutdown(NkIDevice *device) override;
				void Spawn(const NkParticleBirth *births, uint32 n) override;
				void Step(NkICommandBuffer *cmd, const NkEmitterDesc &desc, float32 dt,
						  NkParticleStepStats &stats) override;
				NkBufferHandle InstanceBuffer() const override {
					return mInstances;
				}
				uint32 DrawCount() const override {
					return mCapacity;
				}
				uint32 AliveCount() const override {
					return mAliveCount;
				}
				bool IsGPU() const override {
					return true;
				}
				const char *FailReason() const {
					return mFail;
				}

			private:
				// Paramètres du noyau (160 o, std140) — voir le préambule NkSL dans le .cpp.
				struct Params {
						NkVec4f hm, grav, gmin, bmin, bmax, visc;
						uint32 nx = 0, ny = 0, nz = 0, numCells = 0;
						uint32 cap = 0, nb = 0, fpad = 0, gpad = 0;
						uint32 surfaceMode = 1, mode = 0, accum = 0, nbirths = 0;
						float32 size = 0.f;
						uint32 color = 0, pad0 = 0, pad1 = 0;
				};
				struct SortParams {
						uint32 j = 0, k = 0, which = 0, n = 0;
				};
				struct Kernel {
						::nkentseu::NkShaderHandle shader;
						NkPipelineHandle pipe;
				};
				enum { K_BIRTH = 0, K_CELLCOUNT, K_FILL, K_SCATTER, K_DENS, K_KAPPA, K_CORRECT, K_WARM, K_STOREWARM,
					   K_NONP, K_APPLY, K_REDUCE, K_INTEG, K_STATS, K_NEIGH, K_COUNT };

				bool CompileKernel(int which, const char *name, const char *body);
				void Dispatch(int which, uint32 count);
				void SortGrid(uint32 which, uint32 n); // tri PAR COMPTAGE (05/09) : count, prefixe CPU, fill, scatter
				bool Flush();						   // End + Submit, puis Begin (pour une relecture)
				float32 ReadResidual(uint32 n);
				void StepOnce(float32 dt);
				void ReadStats(uint32 sub, uint32 subVisc, float32 ms);

				NkSPHSolver *mOwner = nullptr;
				NkIDevice *mDevice = nullptr;
				uint32 mCapacity = 0, mBoundary = 0, mFpad = 0, mGpad = 0, mNumCells = 0;
				Params mParams;
				// 12 blocs de stockage (NVIDIA en expose 16 par etage) : paires (cle, indice) et (debut, fin), et un
				// bloc de champs par particule a foulee 8 (rho, alpha, kappa, kappa chaud, kappa total, erreur, voisines).
				NkBufferHandle mX, mV, mKV, mGKV, mFSE, mGSE, mFL, mT, mRed, mInstances, mBirths, mUbo, mSortUbo;
				NkBufferHandle mFieldUbo; // le vent (3 x vec4), écrit à chaque image
				float32 mTime = 0.f;
				NkBufferHandle mCC, mCF; // comptes par cellule et curseurs (tri par comptage)
				NkVector<uint32> mScratchU, mScratchSE; // relecture des comptes, (debut, fin) a envoyer
				NkBufferHandle mNB; // listes de voisines : cap x 64 indices, refaites a chaque sous-pas (05/09, levier de cout)
				float32 mNeighMax = 0.f;
				bool mNeighOverflowSaid = false;
				Kernel mKernels[K_COUNT];
				NkDescSetHandle mLayout, mSet;
				NkICommandBuffer *mCmd = nullptr;
				bool mRecording = false;
				// Le CPU décide qui naît et tient l'horloge des vies (comme NkParticleStoreGPU).
				struct GpuBirth {
						NkVec4f posLife, velRot, slotFlag; // slotFlag.z : 1 naissance, 0 mort
				};
				NkVector<GpuBirth> mPending;
				NkVector<float32> mLife;
				NkVector<uint8> mAlive;
				NkVector<uint32> mFree;
				NkVector<float32> mScratch; // relectures
				uint32 mAliveCount = 0;
				float32 mLastVmax = 0.f;
				uint32 mIterD = 0, mIterV = 0, mCaps = 0, mWarm = 0;
				float32 mResD = 0.f, mResV = 0.f;
				uint32 mSyncs = 0; // relectures (synchronisations) de l'image
				// profil par passe : un chrono du device par sorte de noyau (index 4 + sorte), draine apres chaque relecture
				bool mProfile = false;
				float32 mPassMs[K_COUNT] = {};
				uint32 mPassN[K_COUNT] = {};
				float32 mWaitMs = 0.f;
				void Drain();
				const char *mFail = "";
		};

	} // namespace renderer
} // namespace nkentseu
