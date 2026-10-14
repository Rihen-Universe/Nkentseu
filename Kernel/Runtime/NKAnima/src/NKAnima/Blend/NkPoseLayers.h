#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Blend/NkPoseLayers.h — LA PILE DE COUCHES DE POSES FABRIQUEES (13/10/2026).
//
// D'OU VIENT CE FICHIER. Les couches de NkAnimController (NkAnimMix.h) melangent des
// CLIPS, pilotes par des machines a etats. Le patient virtuel de PV3DE, lui, fabrique
// ses gestes : une image d'un mouvement de l'artiste qu'il echantillonne lui-meme,
// un poing, une tete tournee. Ce sont des POSES, pas des clips, et le controleur ne
// les prend pas. Cette pile les prend.
//
// LA REGLE. Une BASE, puis des couches, dans l'ordre :
//   - REMPLACEMENT : la pose en cours va vers celle de la couche, a son poids, os par
//     os (NkBlendPose). Une couche posee APRES l'emporte sur les precedentes : l'ordre
//     EST la priorite ;
//   - ADDITIF : l'ecart de la couche s'ajoute a la pose en cours, a son poids
//     (NkApplyAdditive ; l'ecart se fabrique par NkMakeAdditive).
// Chaque couche a un poids par os facultatif (un masque : « le bras droit seul »).
// A poids 0 une couche ne touche a rien, au bit pres.
//
// CE QUE LA PILE NE FAIT PAS : elle ne resout rien (ni IK, ni equilibre). Un geste
// dont le poids deplace une CIBLE (une main qui va vers une zone) n'est pas un
// melange de poses : le melanger ici ferait aller la main en arc dans l'espace des
// articulations au lieu d'aller droit. L'hote garde ces gestes-la.
// =============================================================================

#include "NKAnima/Blend/NkAnimMix.h"

namespace nkentseu {
	namespace anim {

		enum class NkPoseLayerMode : uint8 { NK_POSE_OVERRIDE = 0, NK_POSE_ADDITIVE };

		/// UNE COUCHE : une pose fabriquee par l'hote, son poids, un poids par os.
		struct NkPoseLayer {
				NkString name;
				NkPoseLayerMode mode = NkPoseLayerMode::NK_POSE_OVERRIDE;
				float32 weight = 1.f;		   ///< 0 : la couche ne touche a rien
				NkAnimPose pose;			   ///< REMPLACEMENT : la pose visee ; ADDITIF : l'ecart a ajouter
				NkVector<float32> boneWeights; ///< un poids par os (vide : tous a 1)
		};

		/// LA PILE. Les couches sont evaluees dans l'ordre ou elles ont ete ajoutees.
		class NkPoseLayerStack {
			public:
				void Clear() {
					mLayers.Clear();
				}
				/// Ajoute une couche au-dessus des autres ; la reference rendue se remplit sur place.
				NkPoseLayer &Add(const char *name, NkPoseLayerMode mode = NkPoseLayerMode::NK_POSE_OVERRIDE, float32 weight = 1.f);
				[[nodiscard]] uint32 Count() const {
					return (uint32)mLayers.Size();
				}
				[[nodiscard]] NkPoseLayer &Layer(uint32 i) {
					return mLayers[i];
				}
				[[nodiscard]] const NkPoseLayer &Layer(uint32 i) const {
					return mLayers[i];
				}

				/// out = la base, puis chaque couche. `base` et `out` peuvent etre la meme pose.
				void Evaluate(const NkAnimPose &base, NkAnimPose &out) const;

				/// QUI TIENT CET OS ? La part FINALE de chaque couche de REMPLACEMENT sur l'os
				/// `bone` (ce qu'il reste d'elle apres les remplacements poses au-dessus), et
				/// celle de la base : leur somme vaut 1. Une couche ADDITIVE recoit son poids
				/// sur cet os (elle ne retire rien aux autres).
				void Contributions(uint32 bone, NkVector<float32> &parts, float32 &basePart) const;

			private:
				NkVector<NkPoseLayer> mLayers;
		};

	} // namespace anim
} // namespace nkentseu
