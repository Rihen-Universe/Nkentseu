// -----------------------------------------------------------------------------
// FICHIER: Dames/NkDamesBanc.cpp
// DESCRIPTION: Le banc des regles. Ni fenetre, ni GPU, ni image.
//
// ⚠️ IL CONTIENT SES CAS NEGATIFS. Un banc qui ne verifie que ce qui doit
// marcher passe au vert sur un moteur qui autorise TOUT. Les cinq regles que
// les implementations de dames ratent ont chacune leur cas ici, et ils ont ete
// ecrits AVANT de savoir si le code les respectait.
//
// CONTRE-EPREUVE FAITE le 2026-09-01 : en retirant le filtre de rafle maximale,
// le banc est passe au ROUGE sur le bon cas, code de sortie 1. Un banc qui n a
// jamais rougi est une intention, pas un controle.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Dames/NkDamesBanc.h"
#include "Dames/NkDamesEcran.h"
#include "Dames/NkDamesRegles.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace jeux {
		namespace dames {

int32 NkDamesLancerBanc() {
	int32 echecs = 0;
	auto verifier = [&](const char *nom, bool ok) {
		logger.Infof("[banc] %-58s %s\n", nom, ok ? "ok" : "ECHEC");
		if (!ok) {
			++echecs;
		}
	};

	// --- 1. Position de depart -------------------------------------
	{
		NkDamesPartie p;
		verifier("depart : 20 pieces blanches", p.CompterPieces(NkDamesCamp::NK_BLANC) == 20);
		verifier("depart : 20 pieces noires", p.CompterPieces(NkDamesCamp::NK_NOIR) == 20);
		NkVector<NkDamesCoup> coups;
		p.CoupsLegaux(coups);
		verifier("depart : 9 coups d'ouverture", coups.Size() == 9);
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].EstPrise()) {
				verifier("depart : AUCUNE prise possible (cas negatif)", false);
				break;
			}
		}
	}

	// --- 2. La prise est OBLIGATOIRE -------------------------------
	{
		NkDamesPartie p;
		for (int32 r = 0; r < 10; ++r) {
			for (int32 c = 0; c < 10; ++c) {
				p.PoserCase(r, c, NkDamesPiece::NK_VIDE);
			}
		}
		p.PoserCase(5, 4, NkDamesPiece::NK_PION_BLANC);
		p.PoserCase(4, 3, NkDamesPiece::NK_PION_NOIR);
		p.PoserCase(9, 0, NkDamesPiece::NK_PION_BLANC); // un coup simple existe ailleurs
		p.PoserTrait(NkDamesCamp::NK_BLANC);

		NkVector<NkDamesCoup> coups;
		p.CoupsLegaux(coups);
		bool toutesPrises = coups.Size() > 0;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (!coups[i].EstPrise()) {
				toutesPrises = false;
			}
		}
		// CAS NEGATIF : le pion en (9,0) peut avancer, et pourtant AUCUN
		// coup simple ne doit etre propose tant qu'une prise existe.
		verifier("prise obligatoire : les coups simples disparaissent", toutesPrises);
	}

	// --- 3. RAFLE MAXIMALE -----------------------------------------
	{
		NkDamesPartie p;
		for (int32 r = 0; r < 10; ++r) {
			for (int32 c = 0; c < 10; ++c) {
				p.PoserCase(r, c, NkDamesPiece::NK_VIDE);
			}
		}
		// Blanc en (7,2). A gauche une prise simple, a droite une double.
		p.PoserCase(7, 2, NkDamesPiece::NK_PION_BLANC);
		p.PoserCase(6, 1, NkDamesPiece::NK_PION_NOIR); // prise simple -> (5,0)
		p.PoserCase(6, 3, NkDamesPiece::NK_PION_NOIR); // -> (5,4)
		p.PoserCase(4, 5, NkDamesPiece::NK_PION_NOIR); // puis -> (3,6) : deux prises
		p.PoserTrait(NkDamesCamp::NK_BLANC);

		NkVector<NkDamesCoup> coups;
		p.CoupsLegaux(coups);
		bool toutesDoubles = coups.Size() > 0;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].nbPrises != 2) {
				toutesDoubles = false;
			}
		}
		verifier("rafle maximale : la prise simple est ecartee", toutesDoubles);
	}

	// --- 4. Le pion prend EN ARRIERE (regle internationale) --------
	{
		NkDamesPartie p;
		for (int32 r = 0; r < 10; ++r) {
			for (int32 c = 0; c < 10; ++c) {
				p.PoserCase(r, c, NkDamesPiece::NK_VIDE);
			}
		}
		p.PoserCase(5, 4, NkDamesPiece::NK_PION_BLANC);
		p.PoserCase(6, 5, NkDamesPiece::NK_PION_NOIR); // DERRIERE le blanc
		p.PoserTrait(NkDamesCamp::NK_BLANC);

		NkVector<NkDamesCoup> coups;
		p.CoupsLegaux(coups);
		bool trouve = false;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].arrR == 7 && coups[i].arrC == 6) {
				trouve = true;
			}
		}
		verifier("pion : prise EN ARRIERE autorisee", trouve);
	}

	// --- 5. Le pion n'AVANCE pas en arriere (cas negatif du 4) -----
	{
		NkDamesPartie p;
		for (int32 r = 0; r < 10; ++r) {
			for (int32 c = 0; c < 10; ++c) {
				p.PoserCase(r, c, NkDamesPiece::NK_VIDE);
			}
		}
		p.PoserCase(5, 4, NkDamesPiece::NK_PION_BLANC);
		p.PoserTrait(NkDamesCamp::NK_BLANC);

		NkVector<NkDamesCoup> coups;
		p.CoupsLegaux(coups);
		bool recule = false;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].arrR > 5) {
				recule = true;
			}
		}
		verifier("pion : deplacement simple vers l'arriere INTERDIT", !recule);
		verifier("pion : deux avances possibles", coups.Size() == 2);
	}

	// --- 6. La dame est VOLANTE ------------------------------------
	{
		NkDamesPartie p;
		for (int32 r = 0; r < 10; ++r) {
			for (int32 c = 0; c < 10; ++c) {
				p.PoserCase(r, c, NkDamesPiece::NK_VIDE);
			}
		}
		p.PoserCase(9, 0, NkDamesPiece::NK_DAME_BLANCHE);
		p.PoserTrait(NkDamesCamp::NK_BLANC);

		NkVector<NkDamesCoup> coups;
		p.CoupsLegaux(coups);
		bool loin = false;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].arrR == 0 && coups[i].arrC == 9) {
				loin = true;
			}
		}
		verifier("dame : glisse sur toute la diagonale", loin);
	}

	// --- 7. PROMOTION seulement en FIN de coup ---------------------
	{
		NkDamesPartie p;
		for (int32 r = 0; r < 10; ++r) {
			for (int32 c = 0; c < 10; ++c) {
				p.PoserCase(r, c, NkDamesPiece::NK_VIDE);
			}
		}
		p.PoserCase(1, 2, NkDamesPiece::NK_PION_BLANC);
		p.PoserTrait(NkDamesCamp::NK_BLANC);
		NkVector<NkDamesCoup> coups;
		p.CoupsLegaux(coups);
		bool joue = false;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].arrR == 0) {
				joue = p.Jouer(coups[i]);
				break;
			}
		}
		verifier("promotion : le coup s'est joue", joue);
		verifier("promotion : le pion arrive en rangee 0 devient dame",
				 NkDamesEstDame(p.Case(0, 1)) || NkDamesEstDame(p.Case(0, 3)));
	}

	// --- 8. Un camp sans coup legal a PERDU ------------------------
	{
		NkDamesPartie p;
		for (int32 r = 0; r < 10; ++r) {
			for (int32 c = 0; c < 10; ++c) {
				p.PoserCase(r, c, NkDamesPiece::NK_VIDE);
			}
		}
		// Blanc en (9,0), enferme par un noir en (8,1) soutenu par (7,2).
		p.PoserCase(9, 0, NkDamesPiece::NK_PION_BLANC);
		p.PoserCase(8, 1, NkDamesPiece::NK_PION_NOIR);
		p.PoserCase(7, 2, NkDamesPiece::NK_PION_NOIR);
		p.PoserTrait(NkDamesCamp::NK_BLANC);
		NkDamesCamp gagnant = NkDamesCamp::NK_BLANC;
		const bool fini = p.EstTerminee(gagnant);
		verifier("blocage : la partie est declaree terminee", fini);
		verifier("blocage : c'est le NOIR qui gagne", fini && gagnant == NkDamesCamp::NK_NOIR);
	}

	// --- 9. Une partie entiere se joue jusqu'au bout ---------------
	// ⚠️ Ce cas ne verifie pas une regle : il verifie que le moteur ne se
	// bloque pas et ne produit pas de coup illegal sur 400 tours. C'est le
	// seul cas qui exerce le code dans les conditions REELLES.
	{
		NkDamesPartie p;
		uint32 graine = 12345u;
		int32 tours = 0;
		NkDamesCamp gagnant = NkDamesCamp::NK_BLANC;
		bool coupRefuse = false;
		while (!p.EstTerminee(gagnant) && tours < 400) {
			NkVector<NkDamesCoup> coups;
			p.CoupsLegaux(coups);
			const NkDamesCoup *choisi = NkDamesChoisirCoup(p, coups, graine);
			if (choisi == nullptr || !p.Jouer(*choisi)) {
				coupRefuse = true;
				break;
			}
			++tours;
		}
		verifier("partie complete : aucun coup refuse", !coupRefuse);
		verifier("partie complete : au moins 20 tours joues", tours >= 20);
		logger.Infof("[banc]   (partie de controle : %d tours)\n", tours);
	}


	// --- MISE EN PAGE : LES DEUX ORIENTATIONS ------------------------------
	//
	// ⚠️ CE CAS N'EXISTAIT PAS AVANT LE 2026-09-03, et son absence a coute deux
	// fois. Seul NkLudo avait un cas de mise en page ; quand un defaut y a ete
	// trouve et corrige, ici on a suppose que « meme structure » suffisait.
	// Rodolf a signale le meme defaut le 03/09 : « une fois roter non seulement
	// ce n'est pas proportionnel mais ca laisse du vide ».
	//
	// 📌 « Meme structure que l'autre » n'est pas une mesure. Un correctif
	// recopie a la main dans trois fichiers, avec un banc dans un seul, revient
	// toujours par les deux autres.
	//
	// SIX CRITERES, et les deux derniers sont ceux qui ont mordu :
	//   1. tout tient dans la zone sure ;
	//   2. le damier reste CARRE ;
	//   3. le bandeau ne recouvre pas le damier ;
	//   4. le damier occupe la place -- seuil declare, voir plus bas ;
	//   5. CENTREE : ce qui reste fait DEUX marges egales, jamais un trou ;
	//   6. PROPORTIONNEE : en deux colonnes, la colonne ne concurrence pas le
	//      damier.
	{
		struct Ecran {
				const char *nom;
				uint32 w, h;
				float32 sHaut, sBas, sGauche, sDroite;
		};
		// Des formats REELS, plus deux cas limites : le carre et le presque-carre,
		// ou aucune des deux mises en page ne peut donner beaucoup au damier.
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
		bool damierUtile = true;
		bool centree = true;
		bool proportionnee = true;
		bool actionEnBas = true;
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

			NkDamesGeometrie g;
			g.Calculer(info);

			const float32 x0 = s.sGauche;
			const float32 y0 = s.sHaut;
			const float32 x1 = static_cast<float32>(s.w) - s.sDroite;
			const float32 y1 = static_cast<float32>(s.h) - s.sBas;
			const float32 eps = 0.5f;

			// Les cinq rectangles de JEU. Le menu se centre a part, sur l'ecran.
			const NkRect *tous[5] = {&g.plateau, &g.bandeau, &g.siege[0], &g.siege[1], &g.rejouer};
			const uint32 n = 5;

			float32 gMin = tous[0]->x;
			float32 hMin = tous[0]->y;
			float32 dMax = tous[0]->x + tous[0]->w;
			float32 bMax = tous[0]->y + tous[0]->h;
			for (uint32 i = 0; i < n; ++i) {
				const NkRect &r = *tous[i];
				if (r.x < x0 - eps || r.y < y0 - eps || r.x + r.w > x1 + eps || r.y + r.h > y1 + eps) {
					tousDedans = false;
					fautif = s.nom;
				}
				if (r.x < gMin) {
					gMin = r.x;
				}
				if (r.y < hMin) {
					hMin = r.y;
				}
				if (r.x + r.w > dMax) {
					dMax = r.x + r.w;
				}
				if (r.y + r.h > bMax) {
					bMax = r.y + r.h;
				}
			}

			const float32 d = g.plateau.w > g.plateau.h ? g.plateau.w - g.plateau.h : g.plateau.h - g.plateau.w;
			if (d > eps) {
				tousCarres = false;
				fautif = s.nom;
			}

			// Le bandeau est soit A COTE du damier, soit AU-DESSUS : jamais dessus.
			const bool aCote = g.bandeau.x >= g.plateau.x + g.plateau.w - eps ||
							   g.plateau.x >= g.bandeau.x + g.bandeau.w - eps;
			const bool dessus = g.bandeau.y + g.bandeau.h <= g.plateau.y + eps ||
								g.plateau.y + g.plateau.h <= g.bandeau.y + eps;
			if (!aCote && !dessus) {
				aucunRecouvrement = false;
				fautif = s.nom;
			}

			// ⚠️ CE CRITERE DECLARE SON PERIMETRE : il ne vaut que pour les ecrans
			// FRANCHEMENT allonges. Entre les deux -- 1100 x 1000 -- le cas est
			// reellement contraint, et exiger 72 % ferait rougir une mise en page
			// qu'on ne sait pas ameliorer : un banc qui punit sans dire quoi
			// corriger finit par etre desarme.
			const float32 zw = x1 - x0;
			const float32 zh = y1 - y0;
			const float32 petitCote = zw < zh ? zw : zh;
			const bool franchementAllonge = zw > zh * 1.30f || zh > zw * 1.30f;
			if (franchementAllonge && g.plateau.w / petitCote < 0.72f) {
				damierUtile = false;
				fautif = s.nom;
			}

			// CENTRAGE : les marges opposees doivent etre egales. Un pixel de
			// tolerance -- une dimension impaire laisse un demi-pixel de chaque
			// cote.
			const float32 mG = gMin - x0;
			const float32 mD = x1 - dMax;
			const float32 mH = hMin - y0;
			const float32 mB = y1 - bMax;
			const float32 ecartH = mD > mG ? mD - mG : mG - mD;
			const float32 ecartV = mB > mH ? mB - mH : mH - mB;
			logger.Infof("[mesure] %-20s %ux%u  damier=%.0f colonne=%.0f  "
						 "marges G/D=%.0f/%.0f (ecart %.0f)  H/B=%.0f/%.0f (ecart %.0f)\n",
						 s.nom, s.w, s.h, g.plateau.w, g.bandeau.w, mG, mD, ecartH, mH, mB, ecartV);
			if (ecartH > 1.f || ecartV > 1.f) {
				centree = false;
				fautif = s.nom;
			}

			// ⚠️ LA PROPORTION NE SE JUGE QU'EN DEUX COLONNES : en une colonne le
			// bandeau prend TOUTE la largeur, donc son rapport au damier vaut 1
			// par construction. L'y appliquer ferait rougir une page correcte.
			if (aCote && g.bandeau.w > g.plateau.w * 0.75f) {
				proportionnee = false;
				fautif = s.nom;
			}

			// ── 7. L'ACTION TOUCHE LE BAS ─────────────────────────────────
			// En paysage elle s'aligne sur le bas du plateau ; en portrait sur
			// le bas de la zone. Dans les deux cas : le bord inferieur de la
			// zone sure, a une marge pres. Sans ancrage, un centrage en bloc la
			// laissait a 290 px du bas sur un telephone en portrait -- le
			// critere de centrage, lui, restait VERT : il ne voit que la boite.
			if (bMax < y1 - petitCote * 0.03f - 1.f) {
				actionEnBas = false;
				fautif = s.nom;
			}
		}

		verifier("mise en page : tout tient dans la zone sure", tousDedans);
		verifier("mise en page : le damier reste CARRE", tousCarres);
		verifier("mise en page : le bandeau ne recouvre pas le damier", aucunRecouvrement);
		verifier("mise en page : le damier occupe la place disponible", damierUtile);
		verifier("mise en page : centree, ce qui reste fait DEUX marges egales", centree);
		verifier("mise en page : la colonne ne concurrence pas le damier", proportionnee);
		verifier("mise en page : l'action touche le bas, sous le pouce", actionEnBas);
		if (!tousDedans || !tousCarres || !aucunRecouvrement || !damierUtile || !centree || !proportionnee ||
			!actionEnBas) {
			logger.Infof("[banc]   (ecran fautif : %s)\n", fautif);
		}
	}

	logger.Infof("\n[banc] %s (%d echec(s))\n", echecs == 0 ? "TOUT EST VERT" : "DES CAS ONT ECHOUE", echecs);
	return echecs == 0 ? 0 : 1;
}

		} // namespace dames
	} // namespace jeux
} // namespace nkentseu
