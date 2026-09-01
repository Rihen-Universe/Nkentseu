#pragma once
// -----------------------------------------------------------------------------
// @File    Sommets.h
// @Brief   LES SOMMETS D'UNE FORME — la table unique du PEINTRE et du MODE POINTS.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ CE FICHIER EXISTE AVANT LE DÉFAUT QU'IL EMPÊCHE
// =============================================================================
//  Le mode points doit poser une poignée SUR CHAQUE SOMMET DESSINÉ. Or les
//  sommets étaient calculés *dans le peintre* (`Renderers.h`, tables `kTri`,
//  `kPenta`, `kEtoile`, locales à la fonction de dessin). Les relire là où on
//  en a besoin aurait voulu dire les RECOPIER — et deux tables de sommets, ce
//  sont des poignées qui dérivent du dessin au premier ajustement d'une
//  étoile : l'utilisateur tirerait un coin qui n'est pas là.
//
//  C'est le motif de la porte du 28/08 (« quels chemins frères partagent le
//  même danger »), et c'est la **quatrième fois de la semaine** qu'il se
//  présente sur ce chantier. On l'écrit donc une fois, ici, AVANT d'écrire le
//  second consommateur — pas après l'avoir vu mordre.
//
// =============================================================================
//  CE QU'UN SOMMET VEUT DIRE N'EST PAS LE MÊME SELON LA FORME
// =============================================================================
//  ⚠️ ET C'EST LA VRAIE DIFFICULTÉ, PAS LE CALCUL. Trois natures :
//
//    • `Bouts`    — la LIGNE : ses deux extrémités. Les déplacer change la
//                   BOÎTE (une ligne est la diagonale de son rectangle) : c'est
//                   exprimable avec le modèle d'aujourd'hui, sans rien ajouter.
//    • `Polygone` — triangle, pentagone, étoile : des sommets RÉELS. Les
//                   déplacer demande de les STOCKER (une étoile dont on bouge
//                   une pointe n'est plus l'étoile régulière de la table).
//    • `Coins`    — rect, ellipse, image, avatar, cadre : leurs « sommets » ne
//                   sont pas des sommets, ce sont les COINS DE LA BOÎTE. Les
//                   tirer REDIMENSIONNE. Lunacy le fait aussi, et l'outil doit
//                   le DIRE au lieu de laisser croire à une édition vectorielle.
//
//  Confondre les trois donnerait une interface qui a l'air de tout savoir faire
//  et qui ment sur deux cas sur trois.
// -----------------------------------------------------------------------------
#include "Document.h"

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;

	/// Ce que les sommets d'une forme VEULENT DIRE.
	enum class NkNatureSommets {
		Aucun,	  ///< rien à éditer (un texte n'a pas de sommets)
		Bouts,	  ///< les deux extrémités d'une ligne
		Polygone, ///< des sommets réels, déplaçables un à un
		Coins	  ///< les coins de la boîte : les tirer REDIMENSIONNE
	};

	/// La nature des sommets d'une forme, par son nom.
	inline NkNatureSommets NkNatureDe(const char *shape) {
		if (!shape || !*shape)
			return NkNatureSommets::Coins;
		if (NkComponentDecl::StrEq(shape, "line") || NkComponentDecl::StrEq(shape, "line_up"))
			return NkNatureSommets::Bouts;
		if (NkComponentDecl::StrEq(shape, "triangle") || NkComponentDecl::StrEq(shape, "pentagone")
			|| NkComponentDecl::StrEq(shape, "etoile"))
			return NkNatureSommets::Polygone;
		if (NkComponentDecl::StrEq(shape, "text"))
			return NkNatureSommets::Aucun;
		return NkNatureSommets::Coins;
	}

	/// Les sommets UNITAIRES d'un polygone régulier (cercle inscrit, pointe en
	/// haut), en coordonnées -1..1. Rend le nombre de sommets, 0 si la forme
	/// n'est pas un polygone.
	/// ⚠️ PRÉCALCULÉS : pas de trigonométrie à l'exécution (la raison d'origine
	///    du peintre, conservée telle quelle en déménageant).
	inline uint32 NkSommetsUnitaires(const char *shape, const float32 *&out) {
		static const float32 kTri[6] = {0.f, -1.f, 1.f, 1.f, -1.f, 1.f};
		static const float32 kPenta[10] = {0.f,	   -1.f,   .9511f, -.3090f, .5878f,
										   .8090f, -.5878f, .8090f, -.9511f, -.3090f};
		static const float32 kEtoile[20] = {
			0.f,	 -1.f,	  .2246f,  -.3090f, .9511f,	 -.3090f, .3633f,  .1180f,
			.5878f,	 .8090f,  0.f,	   .3820f,	-.5878f, .8090f,  -.3633f, .1180f,
			-.9511f, -.3090f, -.2246f, -.3090f};
		if (NkComponentDecl::StrEq(shape, "triangle")) {
			out = kTri;
			return 3;
		}
		if (NkComponentDecl::StrEq(shape, "pentagone")) {
			out = kPenta;
			return 5;
		}
		if (NkComponentDecl::StrEq(shape, "etoile")) {
			out = kEtoile;
			return 10;
		}
		out = nullptr;
		return 0;
	}

	/// LES SOMMETS D'UN NŒUD, dans le rectangle `r` (écran ou document, selon ce
	/// qu'on lui donne — la fonction ne s'en mêle pas).
	/// ⚠️ LA LISTE DU DOCUMENT PRIME SUR LA TABLE. `n.sommets` vide = le polygone
	///    RÉGULIER de la table ; non vide = les sommets que la main a déplacés.
	///    C'est exactement la discipline des remplissages : la forme simple est
	///    « la liste par défaut », et le fichier ne gagne une clé que le jour où
	///    quelqu'un s'en sert.
	/// @param xy tableau de sortie, 2 floats par sommet.
	/// @return le nombre de sommets écrits.
	inline uint32 NkSommetsDe(const NkUINode &n, const NkPaintRect &r, float32 *xy, uint32 cap) {
		const char *shape = n.shape.Data();
		const NkNatureSommets nat = NkNatureDe(shape);
		const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
		const float32 hx = r.w * 0.5f, hy = r.h * 0.5f;
		if (nat == NkNatureSommets::Bouts) {
			if (cap < 2)
				return 0;
			// `line` descend, `line_up` monte : la diagonale de la boîte.
			const bool monte = NkComponentDecl::StrEq(shape, "line_up");
			xy[0] = r.x;
			xy[1] = monte ? r.y + r.h : r.y;
			xy[2] = r.x + r.w;
			xy[3] = monte ? r.y : r.y + r.h;
			return 2;
		}
		if (nat == NkNatureSommets::Polygone) {
			if (!n.sommets.Empty()) {
				// LES SOMMETS DU DOCUMENT (déplacés à la main), en unitaire.
				uint32 k = 0;
				for (uint32 i = 0; i < (uint32)n.sommets.Size() && k + 1 < cap; ++i) {
					xy[k * 2] = cx + n.sommets[i].x * hx;
					xy[k * 2 + 1] = cy + n.sommets[i].y * hy;
					++k;
				}
				return k;
			}
			const float32 *unit = nullptr;
			const uint32 nb = NkSommetsUnitaires(shape, unit);
			if (!unit || nb > cap)
				return 0;
			for (uint32 i = 0; i < nb; ++i) {
				xy[i * 2] = cx + unit[i * 2] * hx;
				xy[i * 2 + 1] = cy + unit[i * 2 + 1] * hy;
			}
			return nb;
		}
		if (nat == NkNatureSommets::Coins) {
			if (cap < 4)
				return 0;
			xy[0] = r.x;
			xy[1] = r.y;
			xy[2] = r.x + r.w;
			xy[3] = r.y;
			xy[4] = r.x + r.w;
			xy[5] = r.y + r.h;
			xy[6] = r.x;
			xy[7] = r.y + r.h;
			return 4;
		}
		return 0;
	}

	/// MATÉRIALISER les sommets d'un polygone : la table régulière devient une
	/// liste que la main peut modifier. Idempotent, et sans effet sur une forme
	/// qui n'est pas un polygone.
	/// ⚠️ MÊME RÈGLE QUE `MaterialiserFills` : on ne matérialise que s'il y a
	///    quelque chose à préserver — ici, la table du polygone régulier.
	inline void NkMaterialiserSommets(NkUINode &n) {
		if (!n.sommets.Empty())
			return;
		if (NkNatureDe(n.shape.Data()) != NkNatureSommets::Polygone)
			return;
		const float32 *unit = nullptr;
		const uint32 nb = NkSommetsUnitaires(n.shape.Data(), unit);
		for (uint32 i = 0; i < nb; ++i)
			n.sommets.PushBack(NkPoint2{unit[i * 2], unit[i * 2 + 1]});
	}

} // namespace nkuidesign
