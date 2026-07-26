// =============================================================================
// NkReach2D.h — monde à ACTIONS CONTINUES : point 2D qui doit atteindre une
// cible (NKAI, Phase 4, Jalon 3 : PPO, espaces d'actions continus).
//
// Choix documenté : NkKeyDoorGridWorld (Jalon 1/2) est à états ET actions
// DISCRETS (4 directions), inadapté pour prouver le support d'actions
// continues. NkReach2D est le plus simple environnement à actions continues
// qui reste honnête à évaluer : un agent-point (x,y) dans un monde carré
// [0,worldSize]², déplacement (dx,dy) borné à [-maxStep,maxStep] par pas, doit
// s'approcher d'une cible (tx,ty) TIRÉE AU HASARD à chaque Reset (comme la
// position de départ) — pas de cible fixe, pour éviter qu'une politique
// mémorise une seule trajectoire.
//
// Observation NORMALISÉE dans [-1,1] par worldSize (positions RÉELLES suivies en
// interne, uniquement l'observation exposée est mise à l'échelle) : un MLP fraîchement
// initialisé (Xavier, cf NkDense.cpp) est calibré pour des entrées d'ordre 1 -- des
// coordonnées brutes jusqu'à worldSize (ex. 10) saturent/déséquilibrent l'apprentissage
// (observé empiriquement : la politique ignorait la cible). Pratique standard RL.
//
// Observation = (x, y, tx, ty) NORMALISÉS (StateDim=4). Action = (dx, dy) (ActionDim=2),
// saturée à [-maxStep,maxStep] puis la position résultante saturée à
// [0,worldSize]² (rester dans le monde). Récompense : +1 et épisode terminé
// si la distance à la cible <= reachRadius ; sinon coût de pas + terme dense
// proportionnel à la distance (encourage à se rapprocher à chaque pas, sans
// quoi un signal +1 unique et lointain serait trop rare pour un MLP peu
// profond en peu d'épisodes). Namespace : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKRL/NkContinuousEnv.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			class NkReach2D : public NkContinuousEnv {
				public:
					NkReach2D(float worldSize = 10.0f, float maxStep = 1.0f, float reachRadius = 0.5f,
							  float stepCost = 0.01f, float distScale = 0.05f, uint32 seed = 1u);

					uint32 StateDim() const override {
						return 4;
					}

					uint32 ActionDim() const override {
						return 2;
					}

					float ActionLow(uint32) const override {
						return -mMaxStep;
					}

					float ActionHigh(uint32) const override {
						return mMaxStep;
					}

					void Reset(NkVector<float> &state) override;
					void Step(const NkVector<float> &action, NkVector<float> &nextState, float &reward,
							  bool &done) override;

					float WorldSize() const {
						return mWorldSize;
					}

					float ReachRadius() const {
						return mReachRadius;
					}

					// Distance courante agent -> cible (debug / mesure de succès externe).
					float DistanceToTarget() const;

				private:
					float Rand01();		   // [0,1) déterministe (LCG, même schéma que le reste de NKRL)
					float Norm(float v) const; // coordonnée monde -> [-1,1] (cf en-tête)
					void FillObs(NkVector<float> &obs) const;

					float mWorldSize;
					float mMaxStep;
					float mReachRadius;
					float mStepCost;
					float mDistScale;
					uint32 mRng;

					float mX, mY;   // position courante
					float mTx, mTy; // cible courante (tirée au hasard à chaque Reset)
			};

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
