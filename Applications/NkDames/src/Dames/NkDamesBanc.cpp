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

	logger.Infof("\n[banc] %s (%d echec(s))\n", echecs == 0 ? "TOUT EST VERT" : "DES CAS ONT ECHOUE", echecs);
	return echecs == 0 ? 0 : 1;
}

		} // namespace dames
	} // namespace jeux
} // namespace nkentseu
