// =============================================================================
// RegleFusion.cpp — LE PALIER 1 : DUPLIQUER **ET** FUSIONNER.
//
// C'est `RegleFacile.cpp` avec UNE action de plus. Lisez d'abord RegleFacile,
// puis revenez ici : la difference tient en une trentaine de lignes, et elle
// repond a la seule question que le cours ne repondait pas —
//
//     « Comment la fusion se joue-t-elle SUR L'INTERFACE ? »
//
// LA REPONSE, EN TROIS PHRASES
// ----------------------------
// 1. Vous CLIQUEZ une des cases a consommer  -> elle prend le contour orange
//    « selection », et des anneaux verts apparaissent sur les destinations.
// 2. Vous CLIQUEZ la destination (l'anneau vert) -> le coup part.
// 3. Le totem resultat apparait un niveau plus haut, le journal affiche
//    « FUSIONNER », et le bandeau de l'atelier montre la fusion.
//
// C'est EXACTEMENT le meme geste que DUPLIQUER : deux clics, source puis
// destination. Vous ne selectionnez pas les trois cases une par une — le
// moteur a deja enumere les groupes possibles dans `CoupsPossibles`, et
// l'interface ne fait que retrouver le coup que vous designez.
//
// CE QUI EST DIFFERENT SOUS LE CAPOT, ET POURQUOI IL FAUT LE SAVOIR
// -----------------------------------------------------------------
// Un coup DUPLIQUER porte sa source dans `from`. Un coup FUSIONNER laisse
// `from` a zero et porte les cases consommees dans `fuseCells[0..fuseCount-1]`.
// Si vous fabriquez le NkcMove a la main, N'OUBLIEZ PAS la mise a zero : le
// contrat compare les coups OCTET PAR OCTET, et un octet de reste rend votre
// coup different de lui-meme — l'atelier le refuse alors sans qu'aucune de vos
// regles ne soit fausse. `out.Fusionner(...)` fait le memset pour vous ; c'est
// tout ce a quoi ce raccourci sert.
//
// LA REGLE, ICI, EST VOLONTAIREMENT LA PLUS SIMPLE QUI SOIT
// --------------------------------------------------------
// Deux totems A MOI, VOISINS, DE MEME NIVEAU -> un totem de niveau+1 sur l'une
// des deux cases. Ce n'est pas « la » regle de Conqueror : c'est un exemple
// jouable qu'on peut lire en entier. Le vrai equilibrage se mesure a la
// campagne, pas se decide ici (REGLES §1).
//
// CE QU'IL FAUT EN TIRER COMME MESURE
// -----------------------------------
// Lancez une campagne IA contre IA, une fois avec `fusion_active` a 1, une
// fois a 0, meme graine. Si la duree des parties ne bouge pas, votre fusion ne
// sert a rien et le probleme est dans le COUT que vous lui donnez, pas dans le
// code. C'est le genre de resultat qu'on defend dans la fiche de travail.
//
// POUR L'UTILISER : copier dans travail/rules/ et sauvegarder.
// =============================================================================

#include "Conqueror/ConquerorRegleFacile.h"

using namespace nkentseu;
using namespace nkentseu::conqueror;
using namespace nkentseu::conqueror::facile;

struct RegleFusion {

		static constexpr int32 kCote	  = 5;
		static constexpr int8  kNiveauMax = 3;	///< NkcCellView::level va de 0 a 4

		// ---- 1. A QUOI LA PARTIE RESSEMBLE AU DEPART -------------------------
		void Construire(Grille &g) {
			g.topologie = NkcTopology::Square4;
			g.nbJoueurs = 2;
			g.AjouterRectangle(kCote, kCote);
			g.PoserAuxCoins(2);
		}

		// ---- 2. CE QU'ON A LE DROIT DE FAIRE ---------------------------------
		//
		// L'ORDRE EST NORMATIF (REGLES §17.4) : cases par index croissant, puis
		// voisins dans l'ordre de la topologie. On genere donc les DUPLIQUER
		// d'abord, les FUSIONNER ensuite, chacun dans cet ordre-la. Changer cet
		// ordre rend toutes vos mesures anterieures incomparables.
		void CoupsPossibles(const Partie &p, ListeCoups &out) {
			NkcCoord voisins[8];

			// -- DUPLIQUER : identique a RegleFacile -------------------------
			for (int32 i = 0; i < p.nbCases; ++i) {
				if (p.cases[i].owner != static_cast<int8>(p.joueur)) continue;
				const NkcCoord de = p.ou[i];
				const int32	   n  = p.Voisins(de, voisins, 8);
				for (int32 k = 0; k < n; ++k)
					if (p.Vide(voisins[k])) out.Dupliquer(p.joueur, de, voisins[k]);
			}

			// -- FUSIONNER ---------------------------------------------------
			// Une paire {A, B} ne doit etre proposee QU'UNE FOIS. Sans le
			// filtre `Index(voisin) > i`, chaque paire sortirait deux fois —
			// une fois vue de A, une fois vue de B — et l'IA croirait avoir
			// deux coups distincts la ou il n'y en a qu'un. Le symptome est
			// vicieux : rien ne plante, la distribution des coups est
			// seulement fausse, et la campagne mesure alors autre chose que ce
			// qu'on croit mesurer.
			for (int32 i = 0; i < p.nbCases; ++i) {
				if (p.cases[i].owner != static_cast<int8>(p.joueur)) continue;
				const int8 niv = p.cases[i].level;
				if (niv >= kNiveauMax) continue;   // deja au plafond

				const NkcCoord a = p.ou[i];
				const int32	   n = p.Voisins(a, voisins, 8);
				for (int32 k = 0; k < n; ++k) {
					const int32 j = p.Index(voisins[k]);
					if (j <= i) continue;					   // paire deja vue
					if (p.cases[j].owner != static_cast<int8>(p.joueur)) continue;
					if (p.cases[j].level != niv) continue;	   // meme niveau exige

					const NkcCoord paire[2] = {a, voisins[k]};
					// Le RESULTAT se pose sur la premiere case du groupe. Un
					// autre choix est defendable — c'est un parametre de
					// conception, pas une verite.
					out.Fusionner(p.joueur, paire, 2, a, static_cast<int8>(niv + 1));
				}
			}
			// PASSER est ajoute par le cadre si la liste reste vide.
		}

		// ---- 3. CE QUI SE PASSE QUAND ON LE FAIT -----------------------------
		void Appliquer(Partie &p, const NkcMove &m, Evenements &ev) {

			if (m.kind == NkcMoveKind::Fuse) {
				// On VIDE d'abord toutes les cases consommees, on pose ENSUITE.
				// L'ordre compte : `m.to` fait partie du groupe, et poser avant
				// de vider effacerait le totem qu'on vient de creer. C'est le
				// bug numero un des premieres fusions, et il ne se voit qu'a la
				// dixieme partie, quand un totem disparait sans raison.
				for (int32 k = 0; k < m.fuseCount; ++k) p.Vider(m.fuseCells[k]);

				const int8 niveau = (m.targetLevel >= 0) ? m.targetLevel : 1;
				p.Poser(m.to, m.player, niveau);
				ev.Fusionne(m.player, m.to, niveau);

				// Un totem de niveau n vaut n+1 totems : la conquete doit le
				// refleter, sinon fusionner fait PERDRE des points et aucune IA
				// ne le fera jamais — vous croiriez alors que votre generateur
				// de coups est casse.
				p.conquete[m.player] += 10 * static_cast<int32>(niveau);
				p.energie[m.player]	 += 1;

				p.PasserLaMain();
				return;
			}

			// -- DUPLIQUER : le palier 0, inchange ---------------------------
			p.Poser(m.to, m.player);
			ev.Duplique(m.player, m.from, m.to);
			p.conquete[m.player] += 10;

			NkcCoord	voisins[8];
			const int32 n		  = p.Voisins(m.to, voisins, 8);
			int32		retournes = 0;
			for (int32 k = 0; k < n; ++k) {
				if (!p.Ennemie(voisins[k], m.player)) continue;
				const int8 ancien = p.Proprietaire(voisins[k]);
				// Le niveau ne survit pas au retournement : un totem pris
				// redevient un totem de base. Autre choix possible, autre jeu.
				p.Poser(voisins[k], m.player, 0);
				ev.Retourne(m.player, voisins[k], ancien);
				p.energie[m.player]	 += 2;
				p.conquete[m.player] += 10;
				++retournes;
			}
			if (retournes >= 2) ev.Cascade(m.player, m.to, retournes);

			p.PasserLaMain();
		}
};

NKC_REGLES(RegleFusion, "RegleFusion", "1.0.0", "Cours ConquerorLab")
