// -----------------------------------------------------------------------------
// FICHIER: Echecs/NkEchecsBanc.cpp
// DESCRIPTION: Le banc des regles. Ni fenetre, ni GPU, ni image.
//
// ⚠️ SON COEUR N EST PAS DE MOI : les valeurs PERFT (20, 400, 8902, 197281)
// comptent les positions atteignables en 1 a 4 demi-coups depuis la position de
// depart. Elles sont publiees et verifiees depuis des decennies par des moteurs
// sans aucun rapport avec celui-ci. Un banc qui compare un code aux attentes de
// son auteur valide surtout son auteur ; celui-ci ne le peut pas.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Echecs/NkEchecsBanc.h"
#include "Echecs/NkEchecsEcran.h"
#include "Echecs/NkEchecsRegles.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace jeux {
		namespace echecs {

int32 NkEchecsLancerBanc() {
	int32 echecs = 0;
	auto verifier = [&](const char *nom, bool ok) {
		logger.Infof("[banc] %-56s %s\n", nom, ok ? "ok" : "ECHEC");
		if (!ok) {
			++echecs;
		}
	};

	// --- 1. PERFT depuis la position de depart ---------------------
	{
		NkEchecsPartie p;
		const uint64 attendu[5] = {1ull, 20ull, 400ull, 8902ull, 197281ull};
		for (int32 d = 1; d <= 4; ++d) {
			const uint64 obtenu = p.Perft(d);
			logger.Infof("[banc]   perft(%d) = %llu  (attendu %llu)\n", d, (unsigned long long)obtenu,
						 (unsigned long long)attendu[d]);
			NkString nom = NkString::Format("perft(%d) conforme a la reference publiee", d);
			verifier(nom.Data(), obtenu == attendu[d]);
		}
	}

	// --- 2. Le ROQUE, ses deux cotes -------------------------------
	{
		NkEchecsPartie p;
		p.Vider();
		p.PoserCase(7, 4, NkEchecsPiece::NK_ROI_B);
		p.PoserCase(7, 0, NkEchecsPiece::NK_TOUR_B);
		p.PoserCase(7, 7, NkEchecsPiece::NK_TOUR_B);
		p.PoserCase(0, 4, NkEchecsPiece::NK_ROI_N);
		p.PoserDroitsRoque(true, true, false, false);
		p.PoserTrait(NkEchecsCamp::NK_BLANC);

		NkVector<NkEchecsCoup> coups;
		p.CoupsDepuis(7, 4, coups);
		bool petit = false, grand = false;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].roque && coups[i].arrC == 6) {
				petit = true;
			}
			if (coups[i].roque && coups[i].arrC == 2) {
				grand = true;
			}
		}
		verifier("roque : le petit est propose", petit);
		verifier("roque : le grand est propose", grand);
	}

	// --- 3. Le roi ne TRAVERSE pas une case attaquee (cas negatif) --
	// C'est la condition qu'on oublie : il ne suffit pas que la case
	// d'arrivee soit sure.
	{
		NkEchecsPartie p;
		p.Vider();
		p.PoserCase(7, 4, NkEchecsPiece::NK_ROI_B);
		p.PoserCase(7, 7, NkEchecsPiece::NK_TOUR_B);
		p.PoserCase(0, 4, NkEchecsPiece::NK_ROI_N);
		p.PoserCase(0, 5, NkEchecsPiece::NK_TOUR_N); // attaque la colonne 5 = f1
		p.PoserDroitsRoque(true, false, false, false);
		p.PoserTrait(NkEchecsCamp::NK_BLANC);

		NkVector<NkEchecsCoup> coups;
		p.CoupsDepuis(7, 4, coups);
		bool petit = false;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].roque) {
				petit = true;
			}
		}
		verifier("roque INTERDIT si le roi traverse une case attaquee", !petit);
	}

	// --- 4. PRISE EN PASSANT ---------------------------------------
	{
		NkEchecsPartie p;
		p.Vider();
		p.PoserCase(7, 4, NkEchecsPiece::NK_ROI_B);
		p.PoserCase(0, 4, NkEchecsPiece::NK_ROI_N);
		p.PoserCase(3, 4, NkEchecsPiece::NK_PION_B); // pion blanc en e5
		p.PoserCase(1, 3, NkEchecsPiece::NK_PION_N); // pion noir en d7
		p.PoserTrait(NkEchecsCamp::NK_NOIR);

		// Le noir avance de deux : d7-d5, a cote du pion blanc.
		NkEchecsCoup avance;
		avance.depR = 1;
		avance.depC = 3;
		avance.arrR = 3;
		avance.arrC = 3;
		verifier("en passant : le double pas se joue", p.Jouer(avance));

		NkVector<NkEchecsCoup> coups;
		p.CoupsDepuis(3, 4, coups);
		bool ep = false;
		for (uint32 i = 0; i < coups.Size(); ++i) {
			if (coups[i].enPassant && coups[i].arrR == 2 && coups[i].arrC == 3) {
				ep = true;
			}
		}
		verifier("en passant : la prise est proposee au coup suivant", ep);

		// CAS NEGATIF : elle ne vaut qu'UN coup. On joue autre chose, et
		// elle doit avoir disparu.
		NkEchecsPartie q = p;
		NkEchecsCoup neutre;
		neutre.depR = 7;
		neutre.depC = 4;
		neutre.arrR = 6;
		neutre.arrC = 4;
		q.Jouer(neutre); // le blanc bouge son roi
		NkEchecsCoup neutre2;
		neutre2.depR = 0;
		neutre2.depC = 4;
		neutre2.arrR = 1;
		neutre2.arrC = 4;
		q.Jouer(neutre2); // le noir bouge le sien
		NkVector<NkEchecsCoup> plusTard;
		q.CoupsDepuis(3, 4, plusTard);
		bool encore = false;
		for (uint32 i = 0; i < plusTard.Size(); ++i) {
			if (plusTard[i].enPassant) {
				encore = true;
			}
		}
		verifier("en passant : elle EXPIRE apres un coup (cas negatif)", !encore);
	}

	// --- 5. PROMOTION : quatre choix, pas un -----------------------
	{
		NkEchecsPartie p;
		p.Vider();
		p.PoserCase(7, 4, NkEchecsPiece::NK_ROI_B);
		p.PoserCase(0, 0, NkEchecsPiece::NK_ROI_N);
		p.PoserCase(1, 4, NkEchecsPiece::NK_PION_B);
		p.PoserTrait(NkEchecsCamp::NK_BLANC);

		NkVector<NkEchecsCoup> coups;
		p.CoupsDepuis(1, 4, coups);
		verifier("promotion : quatre coups distincts proposes", coups.Size() == 4);
	}

	// --- 6. ECHEC ET MAT (le mat du berger, en quatre coups) -------
	{
		NkEchecsPartie p;
		p.Vider();
		p.PoserCase(0, 4, NkEchecsPiece::NK_ROI_N);
		p.PoserCase(1, 5, NkEchecsPiece::NK_PION_N);
		p.PoserCase(1, 6, NkEchecsPiece::NK_PION_N);
		p.PoserCase(7, 4, NkEchecsPiece::NK_ROI_B);
		p.PoserCase(1, 4, NkEchecsPiece::NK_DAME_B); // dame en e7, soutenue
		p.PoserCase(4, 1, NkEchecsPiece::NK_FOU_B);	 // fou en b5 qui la soutient
		p.PoserTrait(NkEchecsCamp::NK_NOIR);
		verifier("mat : l'etat est ECHEC ET MAT", p.Etat() == NkEchecsEtat::NK_ECHEC_ET_MAT);
	}

	// --- 7. PAT : aucun coup, mais PAS en echec --------------------
	// ⚠️ CAS NEGATIF DU 6. Un moteur qui declare mat des qu'il n'y a plus de
	// coup transforme une nulle en defaite, et rien ne le signale.
	{
		NkEchecsPartie p;
		p.Vider();
		p.PoserCase(0, 0, NkEchecsPiece::NK_ROI_N); // roi noir en a8
		p.PoserCase(2, 1, NkEchecsPiece::NK_DAME_B); // dame blanche en b6
		p.PoserCase(7, 7, NkEchecsPiece::NK_ROI_B);
		p.PoserTrait(NkEchecsCamp::NK_NOIR);
		verifier("pat : l'etat est PAT, pas ECHEC ET MAT", p.Etat() == NkEchecsEtat::NK_PAT);
		verifier("pat : le camp au trait n'est PAS en echec", !p.EstEnEchec(NkEchecsCamp::NK_NOIR));
	}

	// --- 8. Un coup qui exposerait son roi est ILLEGAL -------------
	{
		NkEchecsPartie p;
		p.Vider();
		p.PoserCase(7, 4, NkEchecsPiece::NK_ROI_B);
		p.PoserCase(6, 4, NkEchecsPiece::NK_FOU_B);	 // cloue
		p.PoserCase(0, 4, NkEchecsPiece::NK_TOUR_N); // le cloue sur la colonne e
		p.PoserCase(0, 0, NkEchecsPiece::NK_ROI_N);
		p.PoserTrait(NkEchecsCamp::NK_BLANC);
		NkVector<NkEchecsCoup> coups;
		p.CoupsDepuis(6, 4, coups);
		verifier("clouage : la piece clouee n'a AUCUN coup", coups.Size() == 0);
	}

	// --- 9. Une partie complete ------------------------------------
	{
		NkEchecsPartie p;
		uint32 graine = 777u;
		int32 tours = 0;
		bool refuse = false;
		while (tours < 200) {
			const NkEchecsEtat e = p.Etat();
			if (e == NkEchecsEtat::NK_ECHEC_ET_MAT || e == NkEchecsEtat::NK_PAT ||
				e == NkEchecsEtat::NK_NULLE_MATERIEL || e == NkEchecsEtat::NK_NULLE_50_COUPS) {
				break;
			}
			NkVector<NkEchecsCoup> coups;
			p.CoupsLegaux(coups);
			const NkEchecsCoup *choisi = NkEchecsChoisirCoup(p, coups, graine);
			if (choisi == nullptr || !p.Jouer(*choisi)) {
				refuse = true;
				break;
			}
			++tours;
		}
		verifier("partie complete : aucun coup refuse", !refuse);
		verifier("partie complete : au moins 20 coups joues", tours >= 20);
		logger.Infof("[banc]   (partie de controle : %d demi-coups)\n", tours);
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

			NkEchecsGeometrie g;
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

		} // namespace echecs
	} // namespace jeux
} // namespace nkentseu
