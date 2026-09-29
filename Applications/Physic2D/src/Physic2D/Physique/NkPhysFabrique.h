// =============================================================================
// NkPhysFabrique.h — les "acteurs" que l'on pose, et les niveaux
//
// Un ACTEUR est ce que l'utilisateur choisit dans "Placer des acteurs" ; il
// fabrique un CORPS (ou en prolonge un, pour les pinceaux : eau, miel, sable).
// Un NIVEAU est une scene complete, rechargeable.
//
// La table des acteurs est la SEULE source : le panneau, les raccourcis 1..9
// et le banc la lisent. Ajouter un acteur, c'est une ligne ici et un cas
// dans NkCreerActeur — rien d'autre.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

#include "Physic2D/Physique/NkPhysMonde.h"

namespace nkentseu {
	namespace physic2d {

		enum class NkActeur : uint8 {
			NK_A_BALLON = 0,
			NK_A_BLOB,
			NK_A_SLIME,
			NK_A_GELEE,
			NK_A_CAISSE,
			NK_A_EAU,
			NK_A_MIEL,
			NK_A_SABLE,
			NK_A_ATOMES_BLOC,
			NK_A_ATOMES_DISQUE,
			NK_A_TISSU,
			NK_A_CORDE,
			NK_A_PONT,
			NK_A_COUNT
		};

		enum class NkCategorie : uint8 {
			NK_CAT_CORPS_MOUS = 0,
			NK_CAT_FLUIDES,
			NK_CAT_STRUCTURES,
			NK_CAT_COUNT
		};

		struct NkInfoActeur {
				const char *nom;
				const char *description;
				NkCategorie categorie;
				NkMateriau materiau;
				bool pinceau; ///< maintenir le clic verse de la matiere
				uint8 couleur[3];
		};

		const NkInfoActeur &NkActeurInfo(NkActeur a) noexcept;
		const char *NkCategorieNom(NkCategorie c) noexcept;

		/// Pose un acteur centre en `pos`. Rend l'index du corps cree, ou -1.
		int32 NkCreerActeur(NkMonde &monde, NkActeur a, const NkV2 &pos) noexcept;

		/// Ouvre un corps VIDE pour un pinceau (eau, miel, sable). -1 sinon.
		int32 NkOuvrirPinceau(NkMonde &monde, NkActeur a) noexcept;

		/// Pinceau : ajoute jusqu'a `n` particules au corps `corps` autour de
		/// `pos`. Ne fait rien si ce corps n'est plus le DERNIER (il doit rester
		/// contigu). Rend le nombre de particules ajoutees.
		uint32 NkVerser(NkMonde &monde, uint32 corps, const NkV2 &pos, float32 rayon, int32 n, NkAlea &alea) noexcept;

		/// Pont suspendu entre deux points epingles.
		int32 NkCreerPont(NkMonde &monde, const NkV2 &a, const NkV2 &b) noexcept;

		constexpr int32 NK_NB_NIVEAUX = 5;
		const char *NkNiveauNom(int32 i) noexcept;
		void NkChargerNiveau(NkMonde &monde, int32 i) noexcept;

	} // namespace physic2d
} // namespace nkentseu
