// =============================================================================
// NKEmbodied/NkSensor.h — abstraction générique de CAPTEUR (NKAI, Phase 6, Jalon 1).
//
// Contrat minimal d'un capteur : lire l'état courant d'un corps (simulé ou,
// plus tard via Kernel/Bare, réel) et le RENDRE sous forme d'un vecteur
// d'observations NORMALISÉES (float, ordre fixe défini par le capteur
// concret). Ne connaît ni la politique de décision (NKAgent) ni l'actionneur
// en face : NkSensor se contente de PERCEVOIR, sans effet de bord sur le
// corps qu'il lit (Sense() est const). C'est la moitié « perception » de la
// boucle perception -> décision -> action du Jalon 1 (cf. NkEmbodiedLoop.h).
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkSensor {
				public:
					virtual ~NkSensor() {
					}

					// Dimension du vecteur d'observations produit par Sense() (fixe pour
					// un capteur donné, connue à l'avance par la politique qui le lit).
					virtual uint32 Dim() const = 0;

					// Lit l'état courant du corps observé et remplit `outObservations`
					// (redimensionné à Dim()). Aucun effet de bord : deux appels
					// consécutifs sans action entre les deux renvoient la même chose.
					virtual void Sense(NkVector<float> &outObservations) const = 0;

					// Jalon 2 (bruit capteur) : tente d'estimer l'état BRUT à partir des
					// `observations` DÉJÀ lues par Sense() (potentiellement bruitées, cf.
					// NkEmbodiedNoisySensor). Repli par défaut : aucune estimation
					// possible (renvoie false, `outState` inchangé) -- l'appelant utilise
					// alors la vérité terrain (cf. NkEmbodiedLoop::EmbodiedTick). Un
					// capteur concret qui sait inverser son propre encodage (ex.
					// NkEmbodiedGridSensor : position normalisée -> indice de grille)
					// redéfinit cette méthode : c'est ce qui permet au bruit capteur
					// d'affecter RÉELLEMENT la décision d'une politique tabulaire (ex.
					// NKAgent, qui indexe par état brut et ignore `observations`) au lieu
					// de rester aveugle au bruit.
					virtual bool EstimateRawState(const NkVector<float> &observations, uint32 &outState) const {
						(void)observations;
						(void)outState;
						return false;
					}
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
