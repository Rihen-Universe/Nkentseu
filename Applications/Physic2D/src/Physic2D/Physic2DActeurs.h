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

		enum class NkActeur : uint8 {
			NK_BALLON = 0,
			NK_BLOB,
			NK_SLIME,
			NK_GELEE,
			NK_EAU,
			NK_MIEL,
			NK_SABLE,
			NK_CRISTAL,
			NK_DISQUE,
			NK_TISSU,
			NK_CORDE,
			NK_PONT,
			NK_CAISSE, ///< corps RIGIDE (NkPhysicsWorld)
			NK_BALLE,  ///< corps RIGIDE (NkPhysicsWorld)
			NK_COUNT
		};

		struct NkInfoActeur {
				const char *nom;
				const char *description;
				uint32 couleur; ///< RGBA
				bool pinceau;	///< maintenir verse de la matiere
				bool rigide;
		};

		const NkInfoActeur &NkActeurInfo(NkActeur a) noexcept;

		/// Pose un acteur en `pos`. Rend son entite (invalide si refuse).
		ecs::NkEntityId NkPoserActeur(unkeny::NkScene &scene, NkActeur a, const NkVec2f &pos);
		/// Pinceau : ouvre un corps de fluide vide et en fait une entite.
		ecs::NkEntityId NkOuvrirPinceau(unkeny::NkScene &scene, NkActeur a);
		/// Verse jusqu'a `n` particules dans le corps de l'entite `e`. false si
		/// ce corps n'est plus le dernier (il faut en rouvrir un).
		bool NkVerser(unkeny::NkScene &scene, ecs::NkEntityId e, const NkVec2f &pos, int32 n, uint32 &graine);
		/// Pont trace entre deux points.
		ecs::NkEntityId NkPoserPont(unkeny::NkScene &scene, const NkVec2f &a, const NkVec2f &b);
		/// Paroi statique (capsule) de a a b ; a == b donne une cheville ronde.
		ecs::NkEntityId NkPoserObstacle(unkeny::NkScene &scene, const NkVec2f &a, const NkVec2f &b, float32 rayon);

		/// Marqueur des obstacles et du decor (pour les dessiner et les gommer).
		/// Fabrique les textures des acteurs RIGIDES (caisse en planches, balle
		/// a quartiers) par programme — aucun fichier, comme le reste de la
		/// demo — et les enregistre. Les acteurs poses ENSUITE les portent.
		/// Sans cet appel (banc, rendu absent), ils restent des aplats.
		void NkCreerTexturesActeurs(unkeny::NkTextures2D &textures);

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
