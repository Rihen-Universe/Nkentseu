#pragma once
// =============================================================================
// NkcMoveChoice — QUEL COUP PART QUAND LE JOUEUR CLIQUE CETTE CASE ?
//
// Trois fonctions, aucun etat, aucune dependance au-dela du contrat et d'un
// NkVector. C'est deliberé : la question « quel coup ce clic designe-t-il ? »
// est la SEULE partie de l'interaction qui puisse etre fausse en silence, et
// une fonction qu'on ne peut pas appeler sans ouvrir une fenetre n'est jamais
// mesuree. `tests/NkcAmbiguite.cpp` appelle celles-ci directement.
//
// LE DEFAUT QUE CE FICHIER EXISTE POUR EMPECHER
// ---------------------------------------------
// L'interface jouait « le PREMIER coup qui touche la selection et qui va sur la
// case cliquee ». Or la destination NE SUFFIT PAS a designer un coup :
//
//   POUVOIR   `from` = lanceur, `to` = cible, `powerId` = LEQUEL. Deux pouvoirs
//             differents du meme totem sur la meme cible sont deux coups legaux
//             et distincts ; le second n'etait JAMAIS jouable a la souris.
//             Aucun message, aucune trace : il figurait dans la liste, l'IA
//             pouvait le jouer, l'humain non.
//   FUSIONNER meme case resultat, groupes de cases differents. Trois totems
//             voisins de meme niveau : {A,B}->A et {A,C}->A partagent `to`.
//   DUPLIQUER + POUVOIR vers la meme case : meme collision.
//
// La correction ne devine pas : elle COLLECTE. Si l'ambiguite est reelle, elle
// rend la main au joueur (menu « Quel coup ? » du panneau Plateau).
// =============================================================================

#include "Conqueror/ConquerorRulesABI.h"
#include "Conqueror/ConquerorGeometry.h"

#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		inline constexpr int32 kMaxChoix = 8;	///< coups proposes quand une case est ambigue

		/// UN COUP PART-IL DE CETTE CASE ?
		///
		/// La question n'a pas la meme reponse selon le GENRE du coup, et c'est ce
		/// qui rendait la FUSION injouable a la souris :
		///
		///   DUPLIQUER  `from` = la source, `to` = la case creee.
		///   FUSIONNER  `from` est INUTILISE (donc {0,0} apres la mise a zero
		///              exigee par le contrat) ; les cases consommees sont dans
		///              `fuseCells[0..fuseCount-1]`, le resultat dans `to`.
		///
		/// Les endroits qui filtraient sur `m.from == case` voyaient donc TOUTES
		/// les fusions comme partant de la case (0,0). Consequence mesurable : un
		/// moteur qui genere des fusions les laisse jouer a l'IA, le journal
		/// affiche bien « FUSIONNER », et l'humain ne peut PAS les cliquer.
		inline bool NkcMoveTouches(const NkcMove &m, NkcCoord c) noexcept {
			if (m.kind == NkcMoveKind::Pass) return false;
			if (m.kind == NkcMoveKind::Fuse) {
				const int32 n = m.fuseCount < static_cast<int32>(kMaxFuseCells)
									? m.fuseCount
									: static_cast<int32>(kMaxFuseCells);
				for (int32 k = 0; k < n; ++k)
					if (CoordEqual(m.fuseCells[k], c)) return true;
				return false;
			}
			return CoordEqual(m.from, c);
		}

		/// TOUS les coups qui partent de `sel` et aboutissent sur `dest`. Renvoie
		/// leur nombre ; `out[0..n-1]` sont des index dans `legal`, dans l'ordre
		/// du moteur. Zero quand la case n'est pas une destination.
		inline int32 NkcCollectMoves(const NkVector<NkcMove> &legal, NkcCoord sel, NkcCoord dest,
									 int32 *out, int32 cap) noexcept {
			int32 n = 0;
			for (usize i = 0; i < legal.Size() && n < cap; ++i) {
				const NkcMove &m = legal[i];
				if (m.kind == NkcMoveKind::Pass) continue;
				if (!NkcMoveTouches(m, sel)) continue;
				if (!CoordEqual(m.to, dest)) continue;
				out[n++] = static_cast<int32>(i);
			}
			return n;
		}

		/// Nom court d'un coup, pour le menu de choix ET pour l'etiquette posee sur
		/// la case. Deux coups differents doivent donner deux noms DIFFERENTS —
		/// sinon le menu qu'on vient d'ajouter ne repond pas a la question qu'il
		/// pose. C'est pour cela que `powerId` et `fuseCount` y figurent.
		inline void NkcNameMove(const NkcMove &m, char *buf, usize cap) noexcept {
			if (!buf || cap == 0) return;
			switch (m.kind) {
				case NkcMoveKind::Duplicate: std::snprintf(buf, cap, "DUPLIQUER"); break;
				case NkcMoveKind::Fuse:
					std::snprintf(buf, cap, "FUSIONNER %d cases", m.fuseCount);
					break;
				case NkcMoveKind::Power:
					std::snprintf(buf, cap, "POUVOIR n%d", static_cast<int32>(m.powerId));
					break;
				case NkcMoveKind::Pass: std::snprintf(buf, cap, "PASSER"); break;
				default: std::snprintf(buf, cap, "COUP"); break;
			}
		}

	}  // namespace conqueror
}  // namespace nkentseu
