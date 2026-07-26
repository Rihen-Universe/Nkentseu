// =============================================================================
// NkKeyDoorGridWorld.h — monde-grille à DEUX étapes obligées (NKAI, Phase 4,
// support du Jalon 3 NKAgent : buts & planification).
//
// Grille N×N comme NkGridWorld, MAIS le but final est une PORTE FERMÉE tant
// que la CLÉ n'a pas été ramassée (marcher sur la case-clé la prend
// automatiquement). Tenter d'entrer sur la porte sans clé = BLOQUÉ (l'agent
// reste sur place, coût de pas) — c'est le « il fonce à la porte fermée » du
// jalon. Avec la clé, entrer sur la porte = +1, épisode terminé. Trous = -1,
// terminé. Actions : 0=haut 1=bas 2=gauche 3=droite.
//
// État = hasKey·N² + (y·N + x)  →  NumStates = 2·N².
// La possession de la clé fait PARTIE de l'état (processus markovien) ; c'est
// la structure en deux étapes (clé PUIS porte) qui justifie la décomposition
// en sous-buts côté NkAgent. Namespace : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKRL/NkEnv.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			class NkKeyDoorGridWorld : public NkEnvironment {
				public:
					// `startCell`, `keyCell`, `doorCell`, `holes` : indices de POSITION
					// (y·size + x), tous supposés distincts et hors trous.
					NkKeyDoorGridWorld(uint32 size, uint32 startCell, uint32 keyCell, uint32 doorCell,
									   const NkVector<uint32> &holes, float stepCost = 0.02f);

					uint32 NumStates() const override {
						return 2u * mSize * mSize;
					}

					uint32 NumActions() const override {
						return 4;
					}

					uint32 Reset() override;
					void Step(uint32 action, uint32 &nextState, float &reward, bool &done) override;

					uint32 Size() const {
						return mSize;
					}

					uint32 StartCell() const {
						return mStart;
					}

					uint32 KeyCell() const {
						return mKey;
					}

					uint32 DoorCell() const {
						return mDoor;
					}

					// État-cible « la clé vient d'être prise » (sur la case-clé, clé en
					// main) — le SOUS-BUT naturel du Jalon 3.
					uint32 KeyTakenState() const {
						return mSize * mSize + mKey;
					}

					// État-cible « porte franchie » (sur la porte, clé en main) — le BUT
					// FINAL (état terminal +1).
					uint32 DoorOpenedState() const {
						return mSize * mSize + mDoor;
					}

					// Décomposition d'un état brut.
					uint32 PositionOf(uint32 state) const {
						return state % (mSize * mSize);
					}

					bool HasKey(uint32 state) const {
						return state >= mSize * mSize;
					}

					bool IsHole(uint32 cell) const;

					// Nombre de tentatives d'entrée sur la porte SANS la clé depuis le
					// dernier Reset() — mesure du « foncer à la porte fermée ».
					uint32 BlockedDoorHits() const {
						return mBlockedDoorHits;
					}

				private:
					uint32 mSize;
					uint32 mStart; // position de départ (sans clé)
					uint32 mKey;	// case-clé
					uint32 mDoor;	// case-porte (but final)
					uint32 mCur;	// position courante
					bool mHasKey;
					float mStepCost;
					uint32 mBlockedDoorHits;
					NkVector<uint32> mHoles;
			};

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
