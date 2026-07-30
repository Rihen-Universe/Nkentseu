// =============================================================================
// NKEmbodied/NkEmbodiedGridSensor.h — capteur de position/but (NKAI, Phase 6, Jalon 1).
//
// Implémentation CONCRÈTE de NkSensor pour un NkEmbodiedBody (corps
// grille) : lit la position courante du corps et la RENVOIE encodée
// exactement comme agent::NkAgentPerception (4 features : x normalisé,
// y normalisé, dx signé vers le but, dy signé vers le but) — RÉUTILISE cette
// brique de NKAgent au lieu de dupliquer la formule d'encodage, pour que les
// observations perçues ici soient DIRECTEMENT compatibles avec les features
// que NKAgent attend déjà (cf. NkEmbodiedAgentPolicy). Ne connaît rien du
// mouvement ni de l'actionneur en face : Sense() est une lecture pure (const),
// aucune écriture sur le corps.
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKAgent/NkAgentPerception.h"
#include "NKEmbodied/NkEmbodiedBody.h"
#include "NKEmbodied/NkSensor.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkEmbodiedGridSensor : public NkSensor {
				public:
					// `body` non possédé : sa durée de vie est gérée par l'appelant
					// (même convention que civ::NkCivAgentRef::agent, cf. NKCivilization).
					explicit NkEmbodiedGridSensor(const NkEmbodiedBody &body)
						: mBody(&body), mPerception(body.World()) {
					}

					uint32 Dim() const override {
						return mPerception.FeatureDim();
					}

					// [0] x normalisé [0,1] · [1] y normalisé [0,1]
					// [2] dx signé vers le but [-1,1] · [3] dy signé vers le but [-1,1]
					void Sense(NkVector<float> &outObservations) const override {
						mPerception.Encode(mBody->State(), outObservations);
					}

					// Jalon 2 (bruit capteur) : inverse l'encodage x,y normalisé de
					// [0]/[1] (cf. agent::NkAgentPerception::Encode) pour retrouver
					// l'indice de grille le plus proche. Fonctionne sur des observations
					// BRUITÉES (les valeurs peuvent déborder [0,1] ou tomber entre deux
					// cases -- arrondi au plus proche + bornage dans la grille). C'est
					// cette estimation, pas la vérité terrain, que la boucle (cf.
					// NkEmbodiedLoop::EmbodiedTick) transmet à la politique : un capteur
					// bruité produit donc un état perçu réellement différent de l'état
					// réel du corps.
					bool EstimateRawState(const NkVector<float> &observations, uint32 &outState) const override {
						if (observations.Size() < 2)
							return false;

						const uint32 size = mBody->Size() ? mBody->Size() : 1;
						const float denom = size > 1 ? (float)(size - 1) : 1.0f;

						float xf = observations[0] * denom;
						float yf = observations[1] * denom;
						int32 x = (int32)(xf + (xf >= 0.0f ? 0.5f : -0.5f));
						int32 y = (int32)(yf + (yf >= 0.0f ? 0.5f : -0.5f));

						if (x < 0)
							x = 0;
						if (x >= (int32)size)
							x = (int32)size - 1;
						if (y < 0)
							y = 0;
						if (y >= (int32)size)
							y = (int32)size - 1;

						outState = (uint32)y * size + (uint32)x;
						return true;
					}

				private:
					const NkEmbodiedBody *mBody;
					agent::NkAgentPerception mPerception;
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
