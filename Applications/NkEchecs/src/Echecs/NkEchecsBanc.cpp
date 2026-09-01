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

	logger.Infof("\n[banc] %s (%d echec(s))\n", echecs == 0 ? "TOUT EST VERT" : "DES CAS ONT ECHOUE", echecs);
	return echecs == 0 ? 0 : 1;
}

		} // namespace echecs
	} // namespace jeux
} // namespace nkentseu
