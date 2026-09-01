// -----------------------------------------------------------------------------
// FICHIER: Ludo/NkLudoBanc.cpp
// DESCRIPTION: Le banc des regles ET DE LA GEOMETRIE. Ni fenetre, ni GPU.
//
// ⚠️ SES DEUX PREMIERS CAS VERIFIENT LA GEOMETRIE, pas les regles — et ce sont
// eux qui comptent le plus. Si la derniere case de piste d un joueur ne touche
// pas sa colonne de maison, les pions SAUTENT par-dessus le vide : la regle
// reste juste, le jeu devient incomprehensible, et on cherche le defaut dans le
// calcul d avancement pendant des heures.
//
// ⚠️ ET LE PREMIER CAS A ETE FAUX AVANT D ETRE JUSTE : il exigeait un pas
// orthogonal partout et rougissait sur quatre transitions. La geometrie etait
// bonne — c est le CRITERE qui ne l etait pas. Voir le commentaire sur place.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Ludo/NkLudoBanc.h"
#include "Ludo/NkLudoRegles.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace jeux {
		namespace ludo {

int32 NkLudoLancerBanc() {
	int32 echecs = 0;
	auto verifier = [&](const char *nom, bool ok) {
		logger.Infof("[banc] %-58s %s\n", nom, ok ? "ok" : "ECHEC");
		if (!ok) {
			++echecs;
		}
	};

	// --- 1. La piste est CONTINUE ----------------------------------
	//
	// ⚠️ MA PREMIERE VERSION DE CE CAS ETAIT FAUSSE, et c'est instructif :
	// elle exigeait un pas ORTHOGONAL partout et rougissait sur quatre
	// transitions. La geometrie etait juste — c'est le CRITERE qui ne
	// l'etait pas. Sur un vrai plateau, les quatre virages autour du carre
	// central sont DIAGONAUX : la case (8,6) appartient au centre, elle
	// n'est pas jouable, donc (9,6) touche (8,5) par le coin.
	//
	// Le critere corrige n'est pas seulement plus permissif : il EXIGE
	// exactement quatre diagonales. Accepter "orthogonal ou diagonal"
	// sans compter laisserait passer une piste pleine de raccourcis.
	{
		bool contigu = true;
		int32 diagonales = 0;
		for (int32 i = 0; i < NK_LUDO_PISTE; ++i) {
			int32 l1, c1, l2, c2;
			NkLudoGeometriePiste(i, l1, c1);
			NkLudoGeometriePiste((i + 1) % NK_LUDO_PISTE, l2, c2);
			const int32 dl = l2 > l1 ? l2 - l1 : l1 - l2;
			const int32 dc = c2 > c1 ? c2 - c1 : c1 - c2;
			if (dl + dc == 1) {
				continue; // pas orthogonal : le cas courant
			}
			if (dl == 1 && dc == 1) {
				++diagonales; // virage autour du carre central
				continue;
			}
			contigu = false;
			logger.Infof("[banc]   rupture de piste entre %d (%d,%d) et %d (%d,%d)\n", i, l1, c1,
								 (i + 1) % NK_LUDO_PISTE, l2, c2);
		}
		verifier("piste : 52 cases contigues, boucle fermee comprise", contigu);
		verifier("piste : EXACTEMENT quatre virages diagonaux", diagonales == 4);
	}

	// --- 2. Piste et maison se REJOIGNENT --------------------------
	{
		bool jointes = true;
		for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
			int32 l1, c1, l2, c2;
			NkLudoGeometriePiste(NkLudoCasePiste(j, 50), l1, c1); // derniere case de piste
			NkLudoGeometrieMaison(j, 0, l2, c2);				 // premiere case de maison
			const int32 dl = l2 > l1 ? l2 - l1 : l1 - l2;
			const int32 dc = c2 > c1 ? c2 - c1 : c1 - c2;
			if (dl + dc != 1) {
				jointes = false;
				logger.Infof("[banc]   joueur %d : piste (%d,%d) et maison (%d,%d) ne se touchent pas\n", j, l1, c1,
							 l2, c2);
			}
		}
		verifier("geometrie : chaque colonne de maison touche sa piste", jointes);
	}

	// --- 3. Les quatre entrees sont distinctes et sures -------------
	{
		bool ok = true;
		for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
			if (!NkLudoCaseSure(NkLudoEntree(j))) {
				ok = false;
			}
			for (int32 k = j + 1; k < NK_LUDO_JOUEURS; ++k) {
				if (NkLudoEntree(j) == NkLudoEntree(k)) {
					ok = false;
				}
			}
		}
		verifier("entrees : quatre cases distinctes, toutes sures", ok);
	}

	// --- 4. Il FAUT un six pour sortir (avec son cas negatif) ------
	{
		NkLudoPartie p;
		NkVector<NkLudoCoup> coups;
		for (int32 de = 1; de <= 5; ++de) {
			p.PoserDe(de);
			p.CoupsLegaux(coups);
			if (coups.Size() != 0) {
				verifier("sortie : un de de 1 a 5 ne sort AUCUN pion (cas negatif)", false);
				break;
			}
		}
		p.PoserDe(6);
		p.CoupsLegaux(coups);
		verifier("sortie : un six sort les quatre pions possibles", coups.Size() == 4);
	}

	// --- 5. COMPTE EXACT pour rentrer ------------------------------
	{
		NkLudoPartie p;
		p.PoserAvancement(0, 0, 55); // il reste 2 pas jusqu'a 57
		p.PoserJoueur(0);

		p.PoserDe(3); // un de trop : le coup est IMPOSSIBLE
		NkVector<NkLudoCoup> trop;
		p.CoupsLegaux(trop);
		bool pionZeroBouge = false;
		for (uint32 i = 0; i < trop.Size(); ++i) {
			if (trop[i].pion == 0) {
				pionZeroBouge = true;
			}
		}
		verifier("arrivee : un de TROP GRAND ne rentre pas (cas negatif)", !pionZeroBouge);

		p.PoserDe(2); // le compte exact
		NkVector<NkLudoCoup> juste;
		p.CoupsLegaux(juste);
		bool rentre = false;
		for (uint32 i = 0; i < juste.Size(); ++i) {
			if (juste[i].pion == 0 && juste[i].avancementApres == NK_LUDO_ARRIVEE) {
				rentre = true;
			}
		}
		verifier("arrivee : le compte EXACT rentre le pion", rentre);
	}

	// --- 6. CAPTURE, et ses deux exceptions ------------------------
	{
		NkLudoPartie p;
		p.PoserJoueur(0);
		// On veut une case NON sure atteinte par le joueur 0.
		int32 avCible = -1;
		for (int32 av = 1; av <= 20; ++av) {
			if (!NkLudoCaseSure(NkLudoCasePiste(0, av))) {
				avCible = av;
				break;
			}
		}
		const int32 casePiste = NkLudoCasePiste(0, avCible);

		// On place un pion du joueur 1 sur cette meme case absolue.
		int32 avAdverse = -1;
		for (int32 av = 0; av <= 50; ++av) {
			if (NkLudoCasePiste(1, av) == casePiste) {
				avAdverse = av;
				break;
			}
		}
		p.PoserAvancement(0, 0, avCible - 1);
		p.PoserAvancement(1, 0, avAdverse);
		p.PoserDe(1);
		NkVector<NkLudoCoup> coups;
		p.CoupsLegaux(coups);
		bool capture = false;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].pion == 0 && coups[i].capture) {
				capture = true;
			}
		}
		verifier("capture : un pion adverse SEUL hors case sure est pris", capture);

		// CAS NEGATIF A : deux pions adverses = un bloc, on ne prend pas.
		NkLudoPartie q = p;
		q.PoserAvancement(1, 1, avAdverse);
		NkVector<NkLudoCoup> c2;
		q.CoupsLegaux(c2);
		bool captureBloc = false;
		for (uint32 i = 0; i < c2.Size(); ++i) {
			if (c2[i].pion == 0 && c2[i].capture) {
				captureBloc = true;
			}
		}
		verifier("capture : DEUX pions adverses forment un bloc (cas negatif)", !captureBloc);
	}

	// --- 7. CAS NEGATIF : rien ne se prend sur une case SURE -------
	{
		NkLudoPartie p;
		p.PoserJoueur(0);
		const int32 avSure = 8; // +8 depuis l'entree : case sure par definition
		const int32 casePiste = NkLudoCasePiste(0, avSure);
		int32 avAdverse = -1;
		for (int32 av = 0; av <= 50; ++av) {
			if (NkLudoCasePiste(1, av) == casePiste) {
				avAdverse = av;
				break;
			}
		}
		p.PoserAvancement(0, 0, avSure - 1);
		p.PoserAvancement(1, 0, avAdverse);
		p.PoserDe(1);
		NkVector<NkLudoCoup> coups;
		p.CoupsLegaux(coups);
		bool capture = false;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].pion == 0 && coups[i].capture) {
				capture = true;
			}
		}
		verifier("case sure : le pion qui s'y trouve est INTOUCHABLE (cas negatif)", !capture);
	}

	// --- 8. Trois six d'affilee rendent la main --------------------
	{
		NkLudoPartie p;
		p.PoserJoueur(0);
		p.FinDeTour(true);
		verifier("six : le premier rejoue", p.Joueur() == 0);
		p.FinDeTour(true);
		verifier("six : le deuxieme rejoue", p.Joueur() == 0);
		p.FinDeTour(true);
		verifier("six : le TROISIEME rend la main (cas negatif)", p.Joueur() == 1);
	}

	// --- 9. Une partie entiere va au bout --------------------------
	{
		NkLudoPartie p;
		uint32 graine = 4242u;
		int32 tours = 0;
		int32 gagnant = -1;
		bool refuse = false;
		while (!p.EstTerminee(gagnant) && tours < 4000) {
			const int32 de = p.LancerDe(graine);
			NkVector<NkLudoCoup> coups;
			p.CoupsLegaux(coups);
			if (coups.Size() > 0) {
				const NkLudoCoup *choisi = NkLudoChoisirCoup(p, coups, graine);
				if (choisi == nullptr || !p.Jouer(*choisi)) {
					refuse = true;
					break;
				}
			}
			p.FinDeTour(de == 6);
			++tours;
		}
		verifier("partie complete : aucun coup refuse", !refuse);
		verifier("partie complete : elle se TERMINE (pas de blocage)", gagnant >= 0);
		logger.Infof("[banc]   (partie de controle : %d tours, gagnant = %d)\n", tours, gagnant);
	}

	logger.Infof("\n[banc] %s (%d echec(s))\n", echecs == 0 ? "TOUT EST VERT" : "DES CAS ONT ECHOUE", echecs);
	return echecs == 0 ? 0 : 1;
}

		} // namespace ludo
	} // namespace jeux
} // namespace nkentseu
