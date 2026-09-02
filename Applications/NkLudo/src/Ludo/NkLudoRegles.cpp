// -----------------------------------------------------------------------------
// FICHIER: Ludo/NkLudoRegles.cpp
// DESCRIPTION: Regles du ludo. Aucun dessin, aucune fenetre.
//
// LA GEOMETRIE, ET POURQUOI ELLE EST ECRITE UNE SEULE FOIS
//   Le plateau est une croix inscrite dans une grille 15x15. La piste fait le
//   tour en 52 cases ; les quatre bras portent chacun une colonne de maison.
//   Cette table sert A LA FOIS a la regle (ou va un pion) et au dessin (ou on
//   le montre). Deux tables auraient fini par diverger, et un pion se serait
//   affiche ailleurs qu'il n'est — un defaut qu'on cherche dans la regle alors
//   qu'il est dans l'affichage.
//
// ⚠️ LA COHERENCE N'EST PAS UNE COINCIDENCE, ELLE EST VERIFIEE PAR LE BANC :
//   la derniere case de piste d'un joueur doit toucher la premiere case de sa
//   colonne de maison. Pour le joueur 0 : case 50 = (7,0), maison 0 = (7,1).
//   Si un jour quelqu'un modifie la piste sans toucher aux maisons, le banc
//   rougit au lieu de laisser les pions sauter par-dessus le vide.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Ludo/NkLudoRegles.h"

namespace nkentseu {
	namespace jeux {

		namespace {
			// La piste, dans l'ordre du parcours. 52 cases, sens horaire.
			// 5 + 6 + 1 + 6 + 6 + 1 + 6 + 6 + 1 + 6 + 6 + 1 + 1 = 52.
			struct Case15 {
					int8 l, c;
			};
			const Case15 kPiste[NK_LUDO_PISTE] = {
				// bras GAUCHE, on part vers la droite (entree du joueur 0)
				{6, 1},	 {6, 2},  {6, 3},  {6, 4},	{6, 5},
				// on monte la colonne 6
				{5, 6},	 {4, 6},  {3, 6},  {2, 6},	{1, 6},	 {0, 6},
				{0, 7},
				// on redescend la colonne 8 (entree du joueur 1 en 13)
				{0, 8},	 {1, 8},  {2, 8},  {3, 8},	{4, 8},	 {5, 8},
				// bras DROIT, vers la droite
				{6, 9},	 {6, 10}, {6, 11}, {6, 12}, {6, 13}, {6, 14},
				{7, 14},
				// retour vers la gauche par la ligne 8 (entree du joueur 2 en 26)
				{8, 14}, {8, 13}, {8, 12}, {8, 11}, {8, 10}, {8, 9},
				// on descend la colonne 8
				{9, 8},	 {10, 8}, {11, 8}, {12, 8}, {13, 8}, {14, 8},
				{14, 7},
				// on remonte la colonne 6 (entree du joueur 3 en 39)
				{14, 6}, {13, 6}, {12, 6}, {11, 6}, {10, 6}, {9, 6},
				// bras GAUCHE, retour par la ligne 8
				{8, 5},	 {8, 4},  {8, 3},  {8, 2},	{8, 1},	 {8, 0},
				{7, 0},
				{6, 0}};

			// Colonnes de maison : six cases par joueur, vers le centre.
			const Case15 kMaison[NK_LUDO_JOUEURS][NK_LUDO_MAISON] = {
				{{7, 1}, {7, 2}, {7, 3}, {7, 4}, {7, 5}, {7, 6}},		// joueur 0, depuis la gauche
				{{1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7}, {6, 7}},		// joueur 1, depuis le haut
				{{7, 13}, {7, 12}, {7, 11}, {7, 10}, {7, 9}, {7, 8}},	// joueur 2, depuis la droite
				{{13, 7}, {12, 7}, {11, 7}, {10, 7}, {9, 7}, {8, 7}}};	// joueur 3, depuis le bas

			// Ecuries : quatre emplacements dans chaque bloc d'angle 6x6.
			const Case15 kEcurie[NK_LUDO_JOUEURS][NK_LUDO_PIONS] = {
				{{1, 1}, {1, 4}, {4, 1}, {4, 4}},		// haut-gauche
				{{1, 10}, {1, 13}, {4, 10}, {4, 13}},	// haut-droite
				{{10, 10}, {10, 13}, {13, 10}, {13, 13}}, // bas-droite
				{{10, 1}, {10, 4}, {13, 1}, {13, 4}}};	// bas-gauche
		} // namespace

		void NkLudoGeometriePiste(int32 casePiste, int32 &ligne, int32 &colonne) noexcept {
			if (casePiste < 0 || casePiste >= NK_LUDO_PISTE) {
				ligne = -1;
				colonne = -1;
				return;
			}
			ligne = kPiste[casePiste].l;
			colonne = kPiste[casePiste].c;
		}

		void NkLudoGeometrieMaison(int32 joueur, int32 index, int32 &ligne, int32 &colonne) noexcept {
			if (joueur < 0 || joueur >= NK_LUDO_JOUEURS || index < 0 || index >= NK_LUDO_MAISON) {
				ligne = -1;
				colonne = -1;
				return;
			}
			ligne = kMaison[joueur][index].l;
			colonne = kMaison[joueur][index].c;
		}

		void NkLudoGeometrieEcurie(int32 joueur, int32 index, int32 &ligne, int32 &colonne) noexcept {
			if (joueur < 0 || joueur >= NK_LUDO_JOUEURS || index < 0 || index >= NK_LUDO_PIONS) {
				ligne = -1;
				colonne = -1;
				return;
			}
			ligne = kEcurie[joueur][index].l;
			colonne = kEcurie[joueur][index].c;
		}

		// =====================================================================
		void NkLudoPartie::Initialiser() noexcept {
			for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
				for (int32 p = 0; p < NK_LUDO_PIONS; ++p) {
					mAvancement[j][p] = static_cast<int8>(NK_LUDO_ECURIE);
				}
			}
			mJoueur = 0;
			mDe = 0;
			mSixDaffilee = 0;
		}

		int32 NkLudoPartie::LancerDe(uint32 &graine) noexcept {
			// Generateur congruentiel : reproductible a graine egale, donc un
			// banc peut rejouer une partie entiere a l'identique.
			graine = graine * 1664525u + 1013904223u;
			mDe = static_cast<int32>((graine >> 16) % 6u) + 1;
			return mDe;
		}

		// =====================================================================
		void NkLudoPartie::CoupsLegaux(NkVector<NkLudoCoup> &sortie) const {
			sortie.Clear();
			if (mDe < 1 || mDe > 6) {
				return;
			}

			for (int32 p = 0; p < NK_LUDO_PIONS; ++p) {
				const int32 av = mAvancement[mJoueur][p];
				if (av >= NK_LUDO_ARRIVEE) {
					continue; // deja rentre
				}

				int32 cible;
				bool sortie_ecurie = false;
				if (av == NK_LUDO_ECURIE) {
					if (mDe != 6) {
						continue; // il FAUT un six pour sortir
					}
					cible = 0;
					sortie_ecurie = true;
				} else {
					cible = av + mDe;
					// ⚠️ COMPTE EXACT : on ne depasse pas l'arrivee. Un de trop
					// grand ne fait pas rebondir, il rend le coup IMPOSSIBLE.
					if (cible > NK_LUDO_ARRIVEE) {
						continue;
					}
				}

				// On ne se marche pas dessus dans sa propre COLONNE DE MAISON.
				//
				// ⚠️ MAIS L'ARRIVEE N'EST PAS UNE CASE : c'est un etat, et les
				// QUATRE pions doivent pouvoir y etre en meme temps — c'est meme
				// la condition de victoire. La borne haute est donc STRICTE.
				// Sans elle, le deuxieme pion ne rentrait jamais et aucune
				// partie ne se terminait : defaut trouve par le banc, cas
				// "la partie se TERMINE", jamais par une partie a la main.
				bool occupeParSoi = false;
				for (int32 q = 0; q < NK_LUDO_PIONS; ++q) {
					if (q != p && cible >= 51 && cible < NK_LUDO_ARRIVEE &&
						mAvancement[mJoueur][q] == static_cast<int8>(cible)) {
						occupeParSoi = true;
					}
				}
				if (occupeParSoi) {
					continue;
				}

				NkLudoCoup coup;
				coup.pion = static_cast<int8>(p);
				coup.avancementApres = static_cast<int8>(cible);
				coup.sortieEcurie = sortie_ecurie;

				// --- Capture -------------------------------------------------
				const int32 casePiste = NkLudoCasePiste(mJoueur, cible);
				if (casePiste >= 0 && !NkLudoCaseSure(casePiste)) {
					int32 nbAdverses = 0;
					int32 jTrouve = -1, pTrouve = -1;
					for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
						if (j == mJoueur) {
							continue;
						}
						for (int32 q = 0; q < NK_LUDO_PIONS; ++q) {
							if (NkLudoCasePiste(j, mAvancement[j][q]) == casePiste) {
								++nbAdverses;
								jTrouve = j;
								pTrouve = q;
							}
						}
					}
					// On ne capture qu'un pion SEUL. Deux pions adverses sur la
					// meme case forment un bloc : on ne prend pas.
					if (nbAdverses == 1) {
						coup.capture = true;
						coup.pionCaptureJoueur = static_cast<int8>(jTrouve);
						coup.pionCaptureIndex = static_cast<int8>(pTrouve);
					}
				}

				sortie.PushBack(coup);
			}
		}

		// =====================================================================
		// ⚠️ JOUER NE FAIT PAS CONFIANCE AU COUP QU'ON LUI DONNE.
		//
		// La version precedente ne verifiait que les bornes de `coup.pion`, puis
		// ecrivait `coup.avancementApres` TEL QUEL. `CoupsLegaux` refusait bien
		// les depassements (`cible > NK_LUDO_ARRIVEE`), mais rien n'obligeait un
		// appelant a passer par elle : un coup construit a la main, ou un coup
		// GARDE d'un tour precedent et rejoue apres un autre lance de de,
		// deplacait le pion au-dela de l'arrivee -- « le compte exact » de la
		// regle etait contourne sans un mot.
		//
		// Le controle est ICI parce que c'est le seul endroit que TOUS les
		// chemins traversent : la souris, l'IA, le banc et une reprise de
		// partie. Le mettre dans l'interface l'aurait laisse hors du banc.
		bool NkLudoPartie::Jouer(const NkLudoCoup &coup) noexcept {
			if (coup.pion < 0 || coup.pion >= NK_LUDO_PIONS) {
				return false;
			}
			const int32 apres = static_cast<int32>(coup.avancementApres);
			const int32 avant = static_cast<int32>(mAvancement[mJoueur][coup.pion]);

			// 1. LE COMPTE EXACT. On ne depasse jamais l'arrivee, et l'on
			//    n'avance pas un pion deja rentre.
			if (apres > NK_LUDO_ARRIVEE || apres < 0) {
				return false;
			}
			if (avant >= NK_LUDO_ARRIVEE) {
				return false;
			}
			// 2. LE PAS EST CELUI DU DE. Sortir d'ecurie coute un six et pose le
			//    pion a 0 ; sinon l'ecart doit valoir exactement la face lue.
			if (avant == NK_LUDO_ECURIE) {
				if (mDe != 6 || apres != 0) {
					return false;
				}
			} else if (apres - avant != mDe) {
				return false;
			}
			// 3. ON NE SE MARCHE PAS DESSUS DANS SA COLONNE DE MAISON --
			//    l'arrivee exceptee, ou les quatre pions se rejoignent.
			if (apres >= 51 && apres < NK_LUDO_ARRIVEE) {
				for (int32 q = 0; q < NK_LUDO_PIONS; ++q) {
					if (q != coup.pion && mAvancement[mJoueur][q] == static_cast<int8>(apres)) {
						return false;
					}
				}
			}

			mAvancement[mJoueur][coup.pion] = static_cast<int8>(apres);
			if (coup.capture && coup.pionCaptureJoueur >= 0 &&
				coup.pionCaptureJoueur < NK_LUDO_JOUEURS && coup.pionCaptureIndex >= 0 &&
				coup.pionCaptureIndex < NK_LUDO_PIONS) {
				mAvancement[coup.pionCaptureJoueur][coup.pionCaptureIndex] = static_cast<int8>(NK_LUDO_ECURIE);
			}
			return true;
		}

		void NkLudoPartie::FinDeTour(bool aFaitSix) noexcept {
			if (aFaitSix) {
				++mSixDaffilee;
				// TROIS six d'affilee rendent la main : sans cette borne, un
				// joueur chanceux peut jouer indefiniment, et l'ordinateur
				// bloquerait la partie.
				if (mSixDaffilee < 3) {
					return; // il rejoue
				}
			}
			mSixDaffilee = 0;
			// ⚠️ ON SAUTE LES SIEGES DESACTIVES. Un `% NK_LUDO_JOUEURS` seul
			// donnerait la main a un siege sans pions : la partie attendrait un
			// coup qui ne peut pas venir, et le symptome serait « le jeu se
			// fige » — a des lieues de sa cause.
			mJoueur = ProchainSiegeActif((mJoueur + 1) % NK_LUDO_JOUEURS);
		}

		int32 NkLudoPartie::PionsRentres(int32 joueur) const noexcept {
			if (joueur < 0 || joueur >= NK_LUDO_JOUEURS) {
				return 0;
			}
			int32 n = 0;
			for (int32 p = 0; p < NK_LUDO_PIONS; ++p) {
				if (mAvancement[joueur][p] >= NK_LUDO_ARRIVEE) {
					++n;
				}
			}
			return n;
		}

		bool NkLudoPartie::EstTerminee(int32 &gagnant) const noexcept {
			for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
				// ⚠️ Le test `SiegeActif` est EXPLICITE meme s'il est
				// aujourd'hui redondant : un siege desactive ne joue pas, donc
				// il ne rentre aucun pion, donc il ne pouvait deja pas gagner.
				// Mais cette justesse est un ACCIDENT du chemin des donnees, pas
				// une intention ecrite -- et le jour ou une reprise de partie
				// chargera des avancements depuis un fichier, l'accident tombe.
				if (!SiegeActif(j)) {
					continue;
				}
				if (PionsRentres(j) == NK_LUDO_PIONS) {
					gagnant = j;
					return true;
				}
			}
			gagnant = -1;
			return false;
		}

		// =====================================================================
		// TROIS PALIERS, ET UN SEUL CHEMIN DE CODE.
		//
		// ⚠️ LE PALIER NE CHANGE QUE LA STRATEGIE. Il ne touche ni au de, ni aux
		// coups legaux, ni a la moindre regle : une IA facile et une IA difficile
		// jouent le MEME jeu, avec les MEMES des. C'est la condition pour que
		// « c'est la strategie qui fait la difference » soit vrai et mesurable.
		//
		// Trois fonctions distinctes auraient diverge au premier correctif de
		// regle. Un seul parcours, des poids differents.
		const NkLudoCoup *NkLudoChoisirCoup(const NkLudoPartie &partie, const NkVector<NkLudoCoup> &coups,
											uint32 &graine, NkNiveauIA niveau) noexcept {
			if (coups.Size() == 0) {
				return nullptr;
			}

			// ── FACILE : au hasard parmi les coups LEGAUX ────────────────────
			// Elle ne triche pas et ne joue jamais un coup impossible ; elle ne
			// choisit simplement pas. C'est la bonne facon de faire une IA
			// faible -- l'affaiblir en lui interdisant des coups la rendrait
			// exploitable, et le joueur le sentirait.
			if (niveau == NkNiveauIA::NK_FACILE) {
				graine = graine * 1664525u + 1013904223u;
				const uint32 k = (graine >> 16) % coups.Size();
				return &coups[k];
			}

			const bool difficile = (niveau == NkNiveauIA::NK_DIFFICILE);
			int32 meilleur = -1;
			int32 meilleurScore = -1000000;

			for (uint32 i = 0; i < coups.Size(); ++i) {
				const NkLudoCoup &c = coups[i];
				int32 score = 0;

				if (c.avancementApres >= NK_LUDO_ARRIVEE) {
					score += 1000; // rentrer un pion prime sur tout
				}
				if (c.capture) {
					score += 500; // renvoyer un adverse a l'ecurie
				}
				if (c.sortieEcurie) {
					score += 200; // avoir des pions en jeu vaut mieux qu'en avoir un loin
				}
				score += c.avancementApres; // a defaut, avancer

				// Se poser sur une case sure quand on le peut.
				const int32 casePiste = NkLudoCasePiste(partie.Joueur(), c.avancementApres);
				if (casePiste >= 0 && NkLudoCaseSure(casePiste)) {
					score += 30;
				}

				// ── DIFFICILE : UN COUP D'AVANCE ─────────────────────────────
				// Le palier moyen regarde ce que le coup RAPPORTE ; celui-ci
				// regarde en plus ce qu'il EXPOSE. On compte les pions adverses
				// qui pourraient atteindre la case d'arrivee au prochain lancer,
				// c'est-a-dire ceux qui sont a une a six cases derriere elle.
				//
				// ⚠️ C'est bien UN coup d'avance, pas une recherche : on ne
				// simule pas la partie, on lit le plateau. Le cout reste
				// negligeable, et c'est ce qui permet de le faire a chaque coup
				// sans que l'IA « reflechisse » visiblement.
				if (difficile && casePiste >= 0 && !NkLudoCaseSure(casePiste)) {
					int32 menaces = 0;
					for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
						if (j == partie.Joueur() || !partie.SiegeActif(j)) {
							continue;
						}
						for (int32 q = 0; q < NK_LUDO_PIONS; ++q) {
							const int32 av = partie.Avancement(j, q);
							const int32 pos = NkLudoCasePiste(j, av);
							if (pos < 0) {
								continue; // a l'ecurie ou en maison : il ne menace rien
							}
							// Distance sur la piste, dans le SENS du parcours.
							const int32 d = ((casePiste - pos) + NK_LUDO_PISTE) % NK_LUDO_PISTE;
							if (d >= 1 && d <= 6) {
								++menaces;
							}
						}
					}
					// 60 par menace : assez pour renoncer a quelques cases
					// d'avance, pas assez pour renoncer a une capture (500) ni a
					// une rentree (1000). Les poids se lisent les uns par rapport
					// aux autres, jamais isolement.
					score -= menaces * 60;
				}

				graine = graine * 1664525u + 1013904223u;
				score += static_cast<int32>((graine >> 16) & 0x7u);

				if (score > meilleurScore) {
					meilleurScore = score;
					meilleur = static_cast<int32>(i);
				}
			}
			return meilleur >= 0 ? &coups[static_cast<uint32>(meilleur)] : nullptr;
		}

	} // namespace jeux
} // namespace nkentseu
