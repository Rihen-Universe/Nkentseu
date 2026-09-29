// =============================================================================
// Physic2DActeurs.h — ce qu'on pose dans la scene, et les niveaux
//
// UN ACTEUR = UNE ENTITE D'UNKENY. Rien d'autre ici ne simule :
//   corps mous, fluides   -> NKPhysics::NkParticules2D (fabriques et presets
//                            mesures au banc), enveloppes par NkScene::CreerCorpsMou
//   caisse, balle, sol    -> NKPhysics::NkPhysicsWorld via NkScene::AjouterCorps
//   obstacles             -> corps STATIQUES a collisionneur capsule ou cercle
// Ce fichier ne porte que ce qui est propre a CETTE demo : les noms, les
// couleurs, la taille des objets, et la composition des niveaux.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "Unkeny/Unkeny.h"

namespace nkentseu {
	namespace physic2d {

		using math::NkVec2f;

		// Le CATALOGUE est celui d'Unkeny (Unkeny/Simulation/NkUnkenyActeurs.h) :
		// l'editeur pose les memes acteurs que la demo. Les noms courts ci-dessous
		// gardent le code de la demo lisible.
		using NkActeur = unkeny::NkActeurSim;
		using NkInfoActeur = unkeny::NkInfoActeurSim;
		inline const NkInfoActeur &NkActeurInfo(NkActeur a) noexcept {
			return unkeny::NkActeurSimInfo(a);
		}

		/// Pose un acteur en `pos`, ramene DANS la boite des niveaux (un acteur
		/// ne nait pas dans un mur), avec les ressources de la demo.
		ecs::NkEntityId NkPoserActeur(unkeny::NkScene &scene, NkActeur a, const NkVec2f &pos);
		inline ecs::NkEntityId NkOuvrirPinceau(unkeny::NkScene &scene, NkActeur a) {
			return unkeny::NkOuvrirPinceauSim(scene, a);
		}
		inline bool NkVerser(unkeny::NkScene &scene, ecs::NkEntityId e, const NkVec2f &pos, int32 n, uint32 &graine) {
			return unkeny::NkVerserSim(scene, e, pos, n, graine);
		}
		inline ecs::NkEntityId NkPoserPont(unkeny::NkScene &scene, const NkVec2f &a, const NkVec2f &b) {
			return unkeny::NkPoserPontSim(scene, a, b);
		}
		/// Paroi statique de la demo : celle d'Unkeny, MARQUEE comme decor posé
		/// (donc gommable, a la difference de la boite).
		ecs::NkEntityId NkPoserObstacle(unkeny::NkScene &scene, const NkVec2f &a, const NkVec2f &b, float32 rayon);

		/// Les textures et bruitages de la demo (ceux d'Unkeny, fabriques par
		/// programme). A appeler une fois ; les acteurs poses ENSUITE les portent.
		/// Sans cet appel (banc, rendu absent), ils restent des aplats muets.
		void NkCreerRessourcesDemo(unkeny::NkTextures2D *textures, unkeny::NkSons2D *sons);
		const unkeny::NkRessourcesSim &NkRessourcesDemo() noexcept;

		/// Marqueur de la boite des niveaux et des obstacles poses.
		struct NkDecor2D {
				bool sol = false; ///< sol et murs : non gommables
		};

		constexpr int32 NK_NB_NIVEAUX = 5;
		const char *NkNiveauNom(int32 i) noexcept;
		/// Vide la scene et construit le niveau : boite (sol, murs, plafond en
		/// corps statiques) puis ses acteurs.
		void NkChargerNiveau(unkeny::NkScene &scene, int32 i);

		/// Demi-largeur et hauteur de la boite des niveaux (m).
		constexpr float32 NK_DEMI_LARGEUR = 12.f;
		constexpr float32 NK_HAUTEUR = 14.f;

	} // namespace physic2d
} // namespace nkentseu
