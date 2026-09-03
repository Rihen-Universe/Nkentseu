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
#include "Ludo/NkLudoEcran.h"
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

	// --- 10. SIEGES DESACTIVES : le tour les saute, la partie finit ---
	//
	// ⚠️ Les neuf cas ci-dessus tournent tous a QUATRE sieges actifs. Ils
	// passeraient a l'identique si le saut de siege etait faux : ce cas-ci est
	// le seul qui l'exerce.
	{
		NkLudoPartie p;
		// Le siege 1 est eteint. On le choisit AU MILIEU et non au bout : un
		// saut qui ne marcherait que sur le dernier siege passerait un test
		// pose sur le siege 3, par le seul effet du modulo.
		const bool pose = p.PoserSiegeActif(1, false);
		verifier("siege eteint : la desactivation est acceptee", pose);
		verifier("siege eteint : il en reste trois", p.NbSiegesActifs() == 3);

		// CAS NEGATIF : la borne des deux sieges REFUSE d'aller plus bas.
		// Sans lui, un `PoserSiegeActif` qui accepterait tout resterait vert.
		p.PoserSiegeActif(2, false);
		const bool refuseTrop = !p.PoserSiegeActif(3, false);
		verifier("siege eteint : on ne descend PAS sous deux (cas negatif)", refuseTrop);
		verifier("siege eteint : il en reste bien deux", p.NbSiegesActifs() == 2);

		// On revient a trois sieges pour jouer la partie.
		p.PoserSiegeActif(2, true);
		p.PoserJoueur(p.ProchainSiegeActif(0));

		uint32 graine = 7777u;
		int32 tours = 0;
		int32 gagnant = -1;
		bool eteintAJoue = false;
		while (!p.EstTerminee(gagnant) && tours < 6000) {
			// LE CONTROLE QUI COMPTE : le siege eteint ne prend jamais la main.
			if (p.Joueur() == 1) {
				eteintAJoue = true;
				break;
			}
			const int32 de = p.LancerDe(graine);
			NkVector<NkLudoCoup> coups;
			p.CoupsLegaux(coups);
			if (coups.Size() > 0) {
				const NkLudoCoup *choisi = NkLudoChoisirCoup(p, coups, graine);
				if (choisi != nullptr) {
					p.Jouer(*choisi);
				}
			}
			p.FinDeTour(de == 6);
			++tours;
		}
		verifier("siege eteint : il ne prend JAMAIS la main", !eteintAJoue);
		verifier("siege eteint : la partie se TERMINE quand meme", gagnant >= 0);
		verifier("siege eteint : il ne peut pas gagner", gagnant != 1);
		logger.Infof("[banc]   (partie a trois sieges : %d tours, gagnant = %d)\n", tours, gagnant);
	}

	// --- 11. LE COMPTE EXACT EST TENU PAR `Jouer`, PAS SEULEMENT PAR LA
	//         GENERATION DES COUPS ------------------------------------------
	//
	// ⚠️ CE CAS NE PASSE PAS PAR `CoupsLegaux`, ET C'EST TOUT SON INTERET.
	// `CoupsLegaux` refusait deja les depassements ; `Jouer`, lui, ecrivait
	// `avancementApres` TEL QUEL. Un coup fabrique a la main -- ou garde d'un
	// tour precedent et rejoue apres un autre lance -- passait outre.
	//
	// Rodolf, 2026-09-02 : « dans le ludo on ne verifie pas les cases restantes
	// avant d'accepter le deplacement ».
	{
		NkLudoPartie p;
		for (int32 j = 0; j < NK_LUDO_JOUEURS; ++j) {
			for (int32 q = 0; q < NK_LUDO_PIONS; ++q) {
				p.PoserAvancement(j, q, NK_LUDO_ECURIE);
			}
		}
		p.PoserJoueur(0);

		// Un pion a deux cases de l'arrivee, et un de de 5 : il en faut 2.
		p.PoserAvancement(0, 0, NK_LUDO_ARRIVEE - 2);
		p.PoserDe(5);
		NkLudoCoup triche{};
		triche.pion = 0;
		triche.avancementApres = static_cast<int8>(NK_LUDO_ARRIVEE + 3);
		verifier("compte exact : Jouer REFUSE de depasser l'arrivee", !p.Jouer(triche));
		verifier("compte exact : et le pion n'a PAS bouge",
				 p.Avancement(0, 0) == NK_LUDO_ARRIVEE - 2);

		// Le pas doit valoir exactement la face lue.
		NkLudoCoup faux{};
		faux.pion = 0;
		faux.avancementApres = static_cast<int8>(NK_LUDO_ARRIVEE - 1); // +1, pas +5
		verifier("compte exact : Jouer REFUSE un pas different du de", !p.Jouer(faux));

		// Et le coup JUSTE passe -- sans ce cas positif, un `Jouer` qui
		// refuserait TOUT passerait les deux cas ci-dessus.
		p.PoserDe(2);
		NkLudoCoup bon{};
		bon.pion = 0;
		bon.avancementApres = static_cast<int8>(NK_LUDO_ARRIVEE);
		verifier("compte exact : le coup JUSTE est accepte", p.Jouer(bon));
		verifier("compte exact : et le pion est rentre", p.Avancement(0, 0) == NK_LUDO_ARRIVEE);

		// On ne sort d'ecurie qu'avec un six, et seulement vers 0.
		p.PoserDe(3);
		NkLudoCoup sortie{};
		sortie.pion = 1;
		sortie.avancementApres = 0;
		verifier("compte exact : pas de sortie d'ecurie sans six", !p.Jouer(sortie));
		p.PoserDe(6);
		verifier("compte exact : sortie d'ecurie avec un six", p.Jouer(sortie));
	}

	// --- 12. LE DE NE DEPEND PAS DE QUI JOUE --------------------------------
	//
	// La suite des des doit etre la MEME que les sieges soient humains ou
	// tenus par l'IA. Elle ne l'etait pas : `NkLudoChoisirCoup` avancait la
	// graine PARTAGEE une fois par coup candidat, donc un tour d'IA consommait
	// des tirages qu'un tour humain ne consommait pas.
	//
	// On rejoue ici les deux situations sur la MEME graine de de : d'un cote
	// des lances seuls, de l'autre des lances entrecoupes d'appels a la
	// strategie qui, eux, tirent sur une graine SEPAREE.
	{
		const uint32 kDepart = 20260901u;
		const int32 kN = 200;

		NkLudoPartie a;
		uint32 gA = kDepart;
		NkVector<int32> suiteA;
		for (int32 i = 0; i < kN; ++i) {
			suiteA.PushBack(a.LancerDe(gA));
		}

		NkLudoPartie b;
		uint32 gB = kDepart;	  // la MEME graine de de
		uint32 gStrategie = 123u; // et une graine de strategie, a cote
		NkVector<int32> suiteB;
		for (int32 i = 0; i < kN; ++i) {
			suiteB.PushBack(b.LancerDe(gB));
			// Ce que fait un tour d'IA : elle departage ses options. Cela ne
			// doit RIEN changer au de.
			NkVector<NkLudoCoup> bidon;
			bidon.PushBack(NkLudoCoup{});
			bidon.PushBack(NkLudoCoup{});
			bidon.PushBack(NkLudoCoup{});
			NkLudoChoisirCoup(b, bidon, gStrategie);
		}

		bool identique = suiteA.Size() == suiteB.Size();
		for (uint32 i = 0; identique && i < suiteA.Size(); ++i) {
			if (suiteA[i] != suiteB[i]) {
				identique = false;
			}
		}
		verifier("le de : meme suite avec ou sans strategie qui tire", identique);

		// ⚠️ ET LE CONTRE-CAS, sinon le precedent passerait aussi sur un de
		// CONSTANT. On verifie que la suite varie vraiment.
		bool varie = false;
		for (uint32 i = 1; i < suiteA.Size(); ++i) {
			if (suiteA[i] != suiteA[0]) {
				varie = true;
			}
		}
		verifier("le de : la suite n'est PAS constante", varie);

		// Uniformite grossiere : chaque face doit sortir au moins une fois sur
		// 200 lances. Un de bloque sur trois valeurs passerait les cas
		// ci-dessus.
		bool toutesLesFaces = true;
		for (int32 f = 1; f <= 6; ++f) {
			bool vue = false;
			for (uint32 i = 0; i < suiteA.Size(); ++i) {
				if (suiteA[i] == f) {
					vue = true;
				}
			}
			if (!vue) {
				toutesLesFaces = false;
			}
		}
		verifier("le de : les six faces sortent sur 200 lances", toutesLesFaces);
	}

	// --- 13. LA MISE EN PAGE TIENT DANS LES DEUX ORIENTATIONS --------------
	//
	// ⚠️ ELLE NE LES DISTINGUAIT PAS. `IsPortrait()` n'etait consulte que pour
	// choisir un RATIO de hauteur de bandeau ; la STRUCTURE restait une colonne
	// unique. En paysage, la hauteur commande le plateau : il devenait minuscule
	// et les deux tiers de la largeur restaient vides.
	//
	// Rodolf, 2026-09-02 : « la restructuration de l'interface en paysage
	// portrait n'est pas correcte ».
	//
	// Ce cas verifie des RELATIONS, jamais des valeurs : tout tient dans la zone
	// sure, le plateau est carre, la colonne ne le recouvre pas. Un cas ecrit
	// sur des coordonnees precises casserait au premier ajustement esthetique.
	{
		struct Ecran {
				const char *nom;
				uint32 w, h;
				float32 sHaut, sBas, sGauche, sDroite;
		};
		// Des formats REELS, et deux cas limites : le carre, et le presque-carre
		// ou la colonne ne tient pas -- c'est lui qui a motive la reprise du
		// plateau dans le calcul.
		const Ecran ecrans[] = {
			{"telephone portrait", 1080, 2400, 90.f, 60.f, 0.f, 0.f},
			{"telephone paysage", 2400, 1080, 0.f, 0.f, 90.f, 60.f},
			{"tablette portrait", 1600, 2560, 40.f, 40.f, 0.f, 0.f},
			{"tablette paysage", 2560, 1600, 0.f, 0.f, 40.f, 40.f},
			{"fenetre bureau", 1280, 800, 0.f, 0.f, 0.f, 0.f},
			{"carre", 1000, 1000, 0.f, 0.f, 0.f, 0.f},
			{"presque carre", 1100, 1000, 0.f, 0.f, 0.f, 0.f},
			{"tres large", 3440, 1000, 0.f, 0.f, 0.f, 0.f},
		};

		bool tousDedans = true;
		bool tousCarres = true;
		bool aucunRecouvrement = true;
		bool plateauUtile = true;
		bool centree = true;
		bool proportionnee = true;
		const char *fautif = "";

		for (uint32 e = 0; e < sizeof(ecrans) / sizeof(ecrans[0]); ++e) {
			const Ecran &s = ecrans[e];
			renderer::NkLayoutInfo info;
			info.width = s.w;
			info.height = s.h;
			info.safeArea.top = s.sHaut;
			info.safeArea.bottom = s.sBas;
			info.safeArea.left = s.sGauche;
			info.safeArea.right = s.sDroite;

			NkLudoGeometrie g;
			g.Calculer(info);

			const float32 x0 = s.sGauche;
			const float32 y0 = s.sHaut;
			const float32 x1 = static_cast<float32>(s.w) - s.sDroite;
			const float32 y1 = static_cast<float32>(s.h) - s.sBas;

			// Un demi-pixel de tolerance : les calculs sont en flottants.
			const float32 eps = 0.5f;
			NkRect aTester[3 + NK_LUDO_JOUEURS];
			aTester[0] = g.plateau;
			aTester[1] = g.bandeau;
			aTester[2] = g.bouton;
			for (int32 i = 0; i < NK_LUDO_JOUEURS; ++i) {
				aTester[3 + i] = g.siege[i];
			}
			for (uint32 i = 0; i < sizeof(aTester) / sizeof(aTester[0]); ++i) {
				const NkRect &r = aTester[i];
				if (r.x < x0 - eps || r.y < y0 - eps || r.x + r.w > x1 + eps || r.y + r.h > y1 + eps) {
					tousDedans = false;
					fautif = s.nom;
				}
			}

			const float32 d = g.plateau.w > g.plateau.h ? g.plateau.w - g.plateau.h : g.plateau.h - g.plateau.w;
			if (d > eps) {
				tousCarres = false;
				fautif = s.nom;
			}

			// ── LE CONTROLE QUI COMPTE : LE PLATEAU EST-IL AUSSI GRAND QUE
			//    LA PLACE LE PERMET ? ────────────────────────────────────────
			//
			// ⚠️ DEUX CRITERES ONT ETE ECRITS AVANT CELUI-CI, ET LES DEUX ONT
			// ECHOUE A LA CONTRE-EPREUVE. Ils valent d'etre nommes :
			//
			//   1. « rien ne deborde » -- l'ancienne mise en page ne debordait
			//      pas, elle laissait la largeur VIDE. Vert sur le defaut.
			//   2. « la boite englobante couvre 80 % de la zone » -- MESURE sur
			//      les deux mises en page : l'ancienne rendait 0.97 x 0.93 et la
			//      nouvelle 0.87 x 0.94. La contre-epreuve accusait donc la
			//      BONNE. Cause : le bandeau prend toute la largeur dans
			//      l'ancienne, donc la boite couvre l'ecran pendant que le
			//      PLATEAU est minuscule.
			//
			// Le defaut n'est ni un debordement ni un vide : c'est un PLATEAU
			// TROP PETIT pour la place disponible. C'est donc lui qu'on mesure.
			// Chiffres releves : ancienne mise en page en paysage 0.57 du petit
			// cote, nouvelle 1.00. Le seuil de 0.72 les separe largement.
			//
			// ⚠️ ET CE CONTROLE DECLARE SON PERIMETRE : il ne s'applique qu'aux
			// ecrans FRANCHEMENT larges ou FRANCHEMENT hauts. Entre les deux --
			// 1100 x 1000, par exemple -- aucune des deux mises en page ne peut
			// donner 72 % au plateau : empilee, la barre d'information et les
			// sieges mangent 40 % d'une hauteur de 940 px ; en deux colonnes, la
			// colonne mange la largeur. Le cas est reellement contraint, et
			// exiger 72 % la-bas ferait rougir une mise en page qu'on ne sait
			// pas ameliorer -- un banc qui punit sans indiquer quoi corriger.
			//
			// Les trois autres controles, eux, s'appliquent A TOUS les ecrans.
			const float32 zw = x1 - x0;
			const float32 zh = y1 - y0;
			const float32 petitCote = zw < zh ? zw : zh;
			const bool franchementAllonge = zw > zh * 1.30f || zh > zw * 1.30f;
			const float32 partPlateau = g.plateau.w / petitCote;
			if (franchementAllonge && partPlateau < 0.72f) {
				plateauUtile = false;
				fautif = s.nom;
			}

			// Le bandeau ne recouvre pas le plateau : soit dessous, soit a cote.
			const bool aCote = g.bandeau.x >= g.plateau.x + g.plateau.w - eps ||
							   g.plateau.x >= g.bandeau.x + g.bandeau.w - eps;
			const bool dessus = g.bandeau.y + g.bandeau.h <= g.plateau.y + eps ||
								g.plateau.y + g.plateau.h <= g.bandeau.y + eps;
			if (!aCote && !dessus) {
				aucunRecouvrement = false;
				fautif = s.nom;
			}

			// ── DEUX CONTROLES NES DU SIGNALEMENT DU 03/09 ──────────────────
			//
			// Rodolf : « une fois roter non seulement ce n'est pas proportionnel
			// mais j'ai l'impression que ca laisse du vide ». Les deux moitiés de
			// la phrase sont deux defauts distincts, et AUCUN des quatre
			// controles ci-dessus ne les voyait -- ils etaient tous verts.
			//
			// ⚠️ LE PREMIER BANC AVAIT MEME ECARTE « rien ne deborde » EN NOTANT
			// que l'ancienne mise en page « laissait la largeur VIDE » : le mot
			// etait ecrit, le controle n'a jamais suivi. Un defaut nomme dans un
			// commentaire et non mesure reste un defaut.
			//
			//   1. CENTREE -- ce qui reste se partage en deux marges egales. Un
			//      vide d'un seul cote est ce que l'oeil lit comme inachevé ;
			//      deux marges egales, comme une respiration voulue.
			//   2. PROPORTIONNEE -- en deux colonnes, la colonne ne concurrence
			//      pas le plateau. Mesure avant correctif : 918 px de colonne
			//      contre 1015 de plateau sur un telephone en paysage, et 1420
			//      contre 940 sur un ecran tres large -- la colonne y etait plus
			//      large que le jeu.
			{
				float32 droiteMax = g.plateau.x + g.plateau.w;
				const NkRect *tous[3 + NK_LUDO_JOUEURS];
				tous[0] = &g.plateau;
				tous[1] = &g.bandeau;
				tous[2] = &g.bouton;
				for (int32 i = 0; i < NK_LUDO_JOUEURS; ++i) {
					tous[3 + i] = &g.siege[i];
				}
				float32 basMax = 0.f;
				for (uint32 i = 0; i < sizeof(tous) / sizeof(tous[0]); ++i) {
					const float32 d = tous[i]->x + tous[i]->w;
					if (d > droiteMax) {
						droiteMax = d;
					}
					const float32 b = tous[i]->y + tous[i]->h;
					if (b > basMax) {
						basMax = b;
					}
				}
				float32 gaucheMin = g.plateau.x;
				float32 hautMin = g.plateau.y;
				for (uint32 i = 0; i < sizeof(tous) / sizeof(tous[0]); ++i) {
					if (tous[i]->x < gaucheMin) {
						gaucheMin = tous[i]->x;
					}
					if (tous[i]->y < hautMin) {
						hautMin = tous[i]->y;
					}
				}
				const float32 mG = gaucheMin - x0;
				const float32 mD = x1 - droiteMax;
				const float32 mH = hautMin - y0;
				const float32 mB = y1 - basMax;
				const float32 ecartH = mD > mG ? mD - mG : mG - mD;
				const float32 ecartV = mB > mH ? mB - mH : mH - mB;
				logger.Infof("[mesure] %-20s %ux%u  plateau=%.0f colonne=%.0f  "
							 "marges G/D=%.0f/%.0f (ecart %.0f)  H/B=%.0f/%.0f (ecart %.0f)\n",
							 s.nom, s.w, s.h, g.plateau.w, g.bandeau.w, mG, mD, ecartH, mH, mB, ecartV);

				// Un pixel de tolerance : le centrage se fait en flottants et une
				// dimension impaire laisse un demi-pixel de chaque cote.
				if (ecartH > 1.f || ecartV > 1.f) {
					centree = false;
					fautif = s.nom;
				}

				// ⚠️ LE CRITERE DE PROPORTION NE S'APPLIQUE QU'EN DEUX COLONNES.
				// En une colonne le bandeau prend TOUTE la largeur, donc son
				// rapport au plateau vaut 1 par construction : l'y appliquer
				// ferait rougir une mise en page correcte. `aCote`, calcule
				// ci-dessus, dit precisement dans quel mode on est.
				if (aCote && g.bandeau.w > g.plateau.w * 0.75f) {
					proportionnee = false;
					fautif = s.nom;
				}
			}
		}

		verifier("mise en page : tout tient dans la zone sure", tousDedans);
		verifier("mise en page : le plateau reste CARRE", tousCarres);
		verifier("mise en page : le plateau occupe la place disponible", plateauUtile);
		verifier("mise en page : le bandeau ne recouvre pas le plateau", aucunRecouvrement);
		verifier("mise en page : centree, ce qui reste fait DEUX marges egales", centree);
		verifier("mise en page : la colonne ne concurrence pas le plateau", proportionnee);
		if (!tousDedans || !tousCarres || !plateauUtile || !aucunRecouvrement || !centree || !proportionnee) {
			logger.Infof("[banc]   (ecran fautif : %s)\n", fautif);
		}
	}

	// --- 14. LES PALIERS D'IA DIFFERENT VRAIMENT ---------------------------
	//
	// ⚠️ SANS CE CAS, « facile / moyen / difficile » ne sont que trois mots dans
	// un menu. Un palier qui ne change pas la force du jeu est un reglage qui
	// n'est pas honore -- pire qu'un reglage absent, parce que le joueur le
	// croit.
	//
	// LE PROTOCOLE, et ses deux precautions :
	//   1. les DEUX camps recoivent la MEME suite de des (meme graine de de),
	//      donc l'ecart ne peut venir que de la strategie ;
	//   2. on joue DEUX SERIES en INVERSANT les sieges. Le siege 0 commence, et
	//      commencer est un avantage : sans l'inversion, on mesurerait cet
	//      avantage autant que la force de l'IA.
	{
		auto duel = [](NkNiveauIA niveauSiege0, NkNiveauIA niveauSiege1, int32 parties,
					   uint32 graineDepart) -> int32 {
			int32 victoiresSiege0 = 0;
			for (int32 g = 0; g < parties; ++g) {
				NkLudoPartie p;
				// Deux sieges seulement : les deux autres sont eteints.
				p.PoserSiegeActif(2, false);
				p.PoserSiegeActif(3, false);

				uint32 gDe = graineDepart + static_cast<uint32>(g) * 7919u;
				uint32 gIA = 424242u + static_cast<uint32>(g);
				int32 gagnant = -1;
				int32 tours = 0;
				// 4000 tours : large. Une partie qui n'a pas fini a ce stade est
				// une anomalie, pas une partie longue -- et on ne la compte pas.
				while (tours < 4000 && !p.EstTerminee(gagnant)) {
					const int32 j = p.Joueur();
					const int32 de = p.LancerDe(gDe);
					NkVector<NkLudoCoup> coups;
					p.CoupsLegaux(coups);
					if (coups.Size() > 0) {
						const NkNiveauIA n = (j == 0) ? niveauSiege0 : niveauSiege1;
						const NkLudoCoup *choisi = NkLudoChoisirCoup(p, coups, gIA, n);
						if (choisi != nullptr) {
							p.Jouer(*choisi);
						}
					}
					p.FinDeTour(de == 6);
					++tours;
				}
				if (gagnant == 0) {
					++victoiresSiege0;
				}
			}
			return victoiresSiege0;
		};

		const int32 kParties = 120;

		// DIFFICILE contre FACILE, dans les deux sens.
		const int32 dif0 = duel(NkNiveauIA::NK_DIFFICILE, NkNiveauIA::NK_FACILE, kParties, 1000u);
		const int32 dif1 = kParties - duel(NkNiveauIA::NK_FACILE, NkNiveauIA::NK_DIFFICILE, kParties, 2000u);
		const int32 victoiresDifficile = dif0 + dif1;
		const int32 total = kParties * 2;
		logger.Infof("[banc]   difficile contre facile : %d / %d (%d en premier, %d en second)\n",
					 victoiresDifficile, total, dif0, dif1);
		verifier("paliers : DIFFICILE bat FACILE sur 240 parties", victoiresDifficile * 100 > total * 55);

		// MOYEN contre FACILE : l'ecart doit exister aussi, meme s'il est
		// moindre. Sans ce second duel, un palier intermediaire pourrait etre
		// identique a l'un des deux autres sans qu'on le voie.
		const int32 moy0 = duel(NkNiveauIA::NK_MOYEN, NkNiveauIA::NK_FACILE, kParties, 3000u);
		const int32 moy1 = kParties - duel(NkNiveauIA::NK_FACILE, NkNiveauIA::NK_MOYEN, kParties, 4000u);
		const int32 victoiresMoyen = moy0 + moy1;
		logger.Infof("[banc]   moyen contre facile     : %d / %d\n", victoiresMoyen, total);
		verifier("paliers : MOYEN bat FACILE sur 240 parties", victoiresMoyen * 100 > total * 55);

		// DIFFICILE contre MOYEN : c'est l'ordre qui compte pour le joueur. Sans
		// ce duel direct, on saurait seulement que les deux battent FACILE --
		// ce qui n'ordonne pas les trois paliers entre eux.
		const int32 dm0 = duel(NkNiveauIA::NK_DIFFICILE, NkNiveauIA::NK_MOYEN, kParties, 7000u);
		const int32 dm1 = kParties - duel(NkNiveauIA::NK_MOYEN, NkNiveauIA::NK_DIFFICILE, kParties, 8000u);
		const int32 victoiresDM = dm0 + dm1;
		logger.Infof("[banc]   difficile contre moyen  : %d / %d\n", victoiresDM, total);
		verifier("paliers : DIFFICILE bat MOYEN sur 240 parties", victoiresDM * 100 > total * 52);

		// ⚠️ ET LE CONTROLE QUI EMPECHE DE SE MENTIR : le meme palier contre
		// lui-meme doit rendre un resultat EQUILIBRE. S'il ne l'est pas, ce que
		// les deux duels ci-dessus mesurent n'est pas la force de l'IA mais un
		// biais du protocole -- l'avantage de commencer, par exemple.
		const int32 pair0 = duel(NkNiveauIA::NK_MOYEN, NkNiveauIA::NK_MOYEN, kParties, 5000u);
		const int32 pair1 = kParties - duel(NkNiveauIA::NK_MOYEN, NkNiveauIA::NK_MOYEN, kParties, 6000u);
		const int32 equilibre = pair0 + pair1;
		logger.Infof("[banc]   moyen contre moyen      : %d / %d (doit tourner autour de %d)\n", equilibre, total,
					 total / 2);
		const int32 ecart = equilibre * 2 > total ? equilibre * 2 - total : total - equilibre * 2;
		verifier("paliers : MOYEN contre lui-meme reste equilibre", ecart * 100 <= total * 30);
	}

	logger.Infof("\n[banc] %s (%d echec(s))\n", echecs == 0 ? "TOUT EST VERT" : "DES CAS ONT ECHOUE", echecs);
	return echecs == 0 ? 0 : 1;
}

		} // namespace ludo
	} // namespace jeux
} // namespace nkentseu
