// =============================================================================
// NKAgent/NkAgentMemory.h — mémoire d'événements de l'agent (le passé) (NKAI, Phase 4).
//
// Buffer BORNÉ des dernières N transitions (observation, action, récompense,
// observation suivante, terminal), chacune PONDÉRÉE par une IMPORTANCE
// (Jalon 2). Politique d'oubli : le souvenir le plus récent est TOUJOURS
// stocké (la récence est garantie : toute expérience entre au moins une fois
// en mémoire) ; quand la mémoire est pleine, c'est le souvenir de MOINDRE
// importance qui est évincé (à égalité d'importance : le plus ancien), et non
// plus le plus ancien systématiquement. L'importance est fournie par
// l'appelant (NkAgent utilise |erreur TD| comme proxy, cf. NkAgent.cpp) ; par
// défaut (Push sans importance) elle vaut |récompense|.
// Namespace : nkentseu::ai::agent.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			// Un souvenir : ce que l'agent a perçu, fait, et obtenu.
			struct NkAgentTransition {
					NkVector<float> observation;	  // features perçues à l'instant t (le présent, vu à t)
					uint32 rawState = 0;			  // état brut de l'environnement (pour rebrancher NKRL)
					uint32 action = 0;				  // action choisie
					float reward = 0.0f;			  // récompense reçue
					NkVector<float> nextObservation; // features perçues à t+1
					uint32 nextRawState = 0;
					bool done = false; // épisode terminé après cette transition ?
			};

			// Mémoire d'événements bornée, avec pondération d'importance (Jalon 2).
			// Capacité fixée à la construction ; une fois pleine, Push() évince le
			// souvenir le moins important (à égalité : le plus ancien) — l'oubli
			// n'est plus purement FIFO.
			class NkAgentMemory {
				public:
					explicit NkAgentMemory(nk_size capacity);

					// Enregistre une transition (copie) avec son importance (>= 0, la
					// valeur absolue est prise). O(capacité) au pire (recherche du
					// souvenir le moins important à évincer), pas de réallocation après
					// la construction (capacité fixe).
					void Push(const NkAgentTransition &t, float importance);

					// Rétro-compatible : importance par défaut = |récompense| (dans un
					// monde à récompense terminale ±1 et coût de pas faible, cela
					// identifie déjà les transitions rares et informatives).
					void Push(const NkAgentTransition &t) {
						Push(t, t.reward < 0.0f ? -t.reward : t.reward);
					}

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

					// Accès du plus ancien souvenir encore détenu (index 0) au plus récent
					// (index Size()-1). Le rappel du passé de l'agent.
					const NkAgentTransition &At(nk_size indexFromOldest) const;

					// Importance du souvenir d'index `indexFromOldest` (même convention
					// d'indexation que At()).
					float ImportanceAt(nk_size indexFromOldest) const;

					// Dernier souvenir enregistré.
					const NkAgentTransition &Last() const;

					// Importances extrêmes actuellement retenues (0 si mémoire vide) —
					// utile pour vérifier que la pondération agit.
					float MinImportance() const;
					float MaxImportance() const;

					// Oublie tout d'un coup (remet la mémoire à zéro, garde la capacité).
					void Clear();

				private:
					NkVector<NkAgentTransition> mBuffer; // slots, taille fixe = capacité
					NkVector<float> mImportance;			  // importance par slot
					NkVector<nk_size> mOrder;			  // indices de slot, du plus ancien au plus récent
					nk_size mCount;						  // nombre de souvenirs valides (<= capacité)
			};

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
