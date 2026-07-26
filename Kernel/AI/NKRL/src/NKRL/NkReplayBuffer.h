// =============================================================================
// NkReplayBuffer.h — mémoire de rejeu du DQN (NKAI, Phase 4, Jalon 2).
//
// Buffer CIRCULAIRE à CAPACITÉ FIXE : une seule allocation (NkVector::Resize)
// à la construction, JAMAIS de réallocation ensuite. Politique d'oubli FIFO
// (écrase le plus ancien slot quand plein) — c'est le replay buffer « vanille »
// du DQN original (Mnih et al., 2015). Échantillonnage UNIFORME avec remise
// (RNG LCG déterministe, même schéma que rl::NkQLearning). Pas de priorisation
// (PER) : limite documentée dans ROADMAP.md.
//
// Stocke des transitions BRUTES (état, action, récompense, état_suivant,
// terminé) par INDICES discrets, compatibles avec rl::NkEnvironment. C'est
// rl::NkDQN qui encode l'état (one-hot) au moment de l'entraînement, pas le
// buffer. Namespace : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			// Une transition brute vécue par l'agent : (s, a, r, s', terminé).
			struct NkReplayTransition {
					uint32 state = 0;
					uint32 action = 0;
					float reward = 0.0f;
					uint32 nextState = 0;
					bool done = false;
			};

			class NkReplayBuffer {
				public:
					explicit NkReplayBuffer(nk_size capacity, uint32 seed = 1u);

					// Enregistre une transition (copie). O(1), jamais de réallocation
					// (capacité fixée à la construction).
					void Push(uint32 state, uint32 action, float reward, uint32 nextState, bool done);

					nk_size Size() const {
						return mCount;
					}

					nk_size Capacity() const {
						return mBuffer.Size();
					}

					bool IsEmpty() const {
						return mCount == 0;
					}

					bool IsFull() const {
						return mCount == mBuffer.Size();
					}

					// Accès direct par index physique dans le buffer (pas d'ordre
					// chronologique requis pour l'échantillonnage aléatoire).
					const NkReplayTransition &At(nk_size index) const {
						return mBuffer[index];
					}

					// Échantillonne `batchSize` indices UNIFORMES avec remise dans
					// [0, Size()). Vide `outIndices` si le buffer est vide.
					void Sample(nk_size batchSize, NkVector<nk_size> &outIndices);

				private:
					float Rand01(); // [0,1) déterministe (LCG)

					NkVector<NkReplayTransition> mBuffer; // slots, taille fixe = capacité
					nk_size mCount;						  // nombre de transitions valides (<= capacité)
					nk_size mNextSlot;						  // prochain slot à écraser (FIFO circulaire)
					uint32 mRng;
			};

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
