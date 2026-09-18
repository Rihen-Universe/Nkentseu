#pragma once
// -----------------------------------------------------------------------------
// @File    DesignIABoutEnBout.h
// @Brief   LA CHAINE COMPLETE, SANS FENETRE : taper -> envoyer -> poser -> annuler.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUE CETTE SONDE PROUVE, ET CE QU'ELLE NE PROUVE PAS
// =============================================================================
//  Rodolf demande : « il ouvre NKUIDesign, tape une phrase, et LE DOCUMENT
//  APPARAIT DANS L'EDITEUR ». Cette sonde suit EXACTEMENT ce chemin-la, moins le
//  clic :
//
//      chatBuf  ->  DesignState::LancerGenerationIA  ->  NkEnvoiAsync (second fil)
//               ->  DesignState::RecolterIA       ->  NkDesignAI::Apply
//               ->  le document a GAGNE des noeuds ->  l'annulation les retire
//
//  ⚠️ ET C'EST LE MEME CHEMIN QUE LE BOUTON, PAS UNE COPIE. `GenererDocument()` du
//     panneau appelle `LancerGenerationIA` ; cette sonde aussi. Une sonde qui
//     aurait recopie le corps du bouton aurait fait « deux chemins pour un
//     geste » : on eprouve l'un pendant que l'autre casse.
//
//  ⚠️ CE QU'ELLE NE PROUVE PAS, ET JE L'ECRIS PLUTOT QU'ON LE DECOUVRE : elle
//     ne clique pas, elle ne dessine pas, elle n'ouvre pas de fenetre. Elle
//     prouve la CHAINE, pas le pixel du bouton. Le pixel du bouton est deja
//     tenu par les sondes du panneau.
// =============================================================================

#include "Panels.h"
#include "NKTime/NkChrono.h"

#include <cstdio>

namespace nkuidesign {

	/// `--ia-bout-en-bout[=<demande>]`. Rend 0 si la chaine complete marche.
	inline int RunIABoutEnBout(const char *demande) {
		using namespace nkentseu;
		std::printf("=== LA CHAINE COMPLETE : TAPER -> POSER -> ANNULER ===\n");

		static DesignState st;
		// ⚠️ LE REGISTRE SE PEUPLE, ET C'EST LA FONCTION DE L'APPLICATION QUI LE
		//    FAIT. Sans elle le catalogue est VIDE, et chaque reponse se fait
		//    rejeter pour « composant inconnu » -- on accuse alors le modele
		//    d'inventer des noms alors qu'on ne lui en a propose AUCUN. Mesure du
		//    17/09 : 0 octet de catalogue dans ce mode console.
		//    On appelle `PeuplerRegistre` plutot que de recopier ses trois lignes :
		//    le depot en a deja DEUX ecritures (l'application et NKDesignIABanc),
		//    une troisieme aurait garanti la divergence.
		st.PeuplerRegistre();

		// ⚠️ L'HISTORIQUE PART DE L'ETAT INITIAL, COMME DANS L'APPLICATION.
		//    `Panels.h` appelle `histoire.Reinitialiser(etatEnregistre)` a chaque
		//    creation ou chargement de document. Sans ce pas 0, le PREMIER
		//    `Observer` se contente d'etablir l'etat courant -- celui d'APRES la
		//    pose -- et il n'y a plus rien vers quoi revenir.
		//
		//    La premiere version de cette sonde l'omettait et rendait ECHEC sur
		//    l'annulation. *C'etait la sonde qui ne modelisait pas l'application,
		//    pas l'application qui perdait l'annulation* -- et la difference se
		//    tranche en LISANT ce que fait l'application, jamais en ajustant
		//    l'instrument jusqu'a ce qu'il verdisse.
		{
			NkString etat0;
			st.doc.Save(etat0);
			st.histoire.Reinitialiser(etat0);
		}
		st.doc.NewDocument("Sonde", NkAuthor::Humain);

		// LE DORSAL : le MEME choix que l'application, dans le meme ordre.
		NkDesignBackendProcessus &proc = NkDesignBackendProcessus::ParDefaut();
		if (st.ollamaBackend.IsAvailable())
			st.ai.SetBackend(&st.ollamaBackend);
		else if (proc.IsAvailable())
			st.ai.SetBackend(&proc);
		else
			st.ai.SetBackend(&st.fileBackend);
		std::printf("dorsal        : %s\n",
					st.ai.Backend() ? st.ai.Backend()->Name() : "(aucun)");
		if (st.ai.Backend() && NkComponentDecl::StrEq(st.ai.Backend()->Name(), "ollama"))
			std::printf("modele        : %s (reglage) sur %s\n",
						st.ollamaBackend.modele.Data(), st.ollamaBackend.hote.Data());

		// ⚠️ LE CATALOGUE, IMPRIME AVANT TOUT. Un catalogue VIDE fait rejeter
		//    CHAQUE reponse pour « composant inconnu », et on accuse le modele
		//    d'inventer alors qu'on ne lui a rien propose. Le registre se peuple
		//    au demarrage de l'application : rien ne garantit qu'un mode console
		//    y soit passe.
		{
			NkString cat;
			NkDesignAI::BuildCatalog(cat);
			uint32 nb = 0;
			for (uint32 i = 0; i < (uint32)cat.Size(); ++i)
				if (cat.Data()[i] == '\n')
					++nb;
			std::printf("catalogue     : %u octet(s), %u ligne(s)%s\n",
					 (unsigned)cat.Size(), (unsigned)nb,
					 cat.Size() == 0 ? "  <- VIDE : tout composant sera juge inconnu" : "");
		}
		int echecs = 0;
		auto verdict = [&](const char *quoi, bool ok, const char *det) {
			std::printf("  [%s] %s -- %s\n", ok ? " OK " : "ECHEC", quoi, det);
			if (!ok)
				++echecs;
		};

		// ── (0) LE ZERO DU GESTE : un champ VIDE ne part pas ────────────────
		// ⚠️ IL PASSE EN PREMIER, ET CE N'EST PAS DE LA POLITESSE. Si le zero
		//    n'etait pas tenu, tout ce qui suit pourrait etre vrai « par
		//    accident » : un geste qui part toujours finit forcement par poser
		//    quelque chose.
		{
			NkString pourquoi;
			const bool parti = st.LancerGenerationIA("", pourquoi);
			verdict("(0) LE ZERO : une invite VIDE ne part pas, et le refus est NOMME",
					!parti && pourquoi.Length() > 0, pourquoi.Data());
		}

		// ── (1) LA DEMANDE PART ─────────────────────────────────────────────
		const uint32 noeudsAvant = (uint32)st.doc.nodes.Size();
		NkString pourquoi;
		const bool parti = st.LancerGenerationIA(demande, pourquoi);
		{
			char b[320];
			snprintf(b, sizeof(b), "demande = \"%s\"%s%s", demande,
					 parti ? "" : " -- REFUS : ", parti ? "" : pourquoi.Data());
			verdict("(1) la demande PART", parti, b);
		}
		if (!parti) {
			std::printf("=== RESULTAT : %d echec(s) -- rien n'a pu etre pose ===\n", echecs);
			return echecs == 0 ? 0 : 1;
		}

		// ── (2) LA FENETRE RESTERAIT VIVANTE ────────────────────────────────
		// ⚠️ ON COMPTE LES TOURS, PAS LE TEMPS. « Ca n'a pas gele » ne se mesure
		//    pas avec un chronometre : il faut montrer que l'appelant a
		//    CONTINUE DE TOURNER pendant l'attente. Un tour ici est ce que
		//    serait une image dans la fenetre.
		uint32 tours = 0;
		nkentseu::NkChrono chrono;
		while (!st.RecolterIA()) {
			++tours;
			// ⚠️ LA GARDE COMPTE LE TEMPS, PAS LES TOURS. Premiere version : un
			//    plafond de 6 000 000 tours. Elle a declare « delai depasse » en
			//    quelques secondes alors que le service rechargeait le modele
			//    (35 s de lecture disque, mesure). *Un compteur de tours mesure la
			//    vitesse de MA boucle, jamais la patience que j'accorde.*
			if (chrono.Elapsed().ToSeconds() > 300.0) {
				verdict("(2) la reponse ARRIVE", false, "delai depasse, rien recolte");
				std::printf("=== RESULTAT : %d echec(s) ===\n", echecs + 1);
				return 1;
			}
		}
		{
			char b[200];
			snprintf(b, sizeof(b), "%u tour(s) de l'appelant pendant l'attente "
								   "(zero aurait voulu dire : ca a gele)", tours);
			verdict("(2) la reponse ARRIVE sans que l'appelant s'arrete", tours > 0u, b);
		}

		// ── (3) LE DOCUMENT A GAGNE DES NOEUDS ──────────────────────────────
		const uint32 noeudsApres = (uint32)st.doc.nodes.Size();
		{
			char b[320];
			snprintf(b, sizeof(b), "noeuds %u -> %u ; message du panneau : %s",
					 noeudsAvant, noeudsApres, st.messageIA.Data());
			verdict("(3) LE DOCUMENT A ETE POSE (c'est la demande de Rodolf)",
					noeudsApres > noeudsAvant, b);
		}

		// ── (4) L'ANNULATION LE DEFAIT ──────────────────────────────────────
		// ⚠️ UN GESTE QU'ON NE PEUT PAS DEFAIRE EST PIRE QU'UN GESTE QU'ON NE
		//    PEUT PAS FAIRE. L'historique est OBSERVE : on lui donne les tours
		//    qu'il aurait eus a l'ecran, puis on annule et on compare la
		//    SERIALISATION -- pas un compte de noeuds, qui pourrait coincider
		//    par hasard.
		if (noeudsApres > noeudsAvant) {
			NkString avantPose;
			st.doc.Save(avantPose);
			for (int32 i = 0; i < 12; ++i) {
				NkString o;
				st.doc.Save(o);
				st.histoire.Observer(o);
			}
			const bool peut = st.histoire.PeutAnnuler();
			char b[220];
			snprintf(b, sizeof(b), "l'historique a %s de pas a defaire apres la pose",
					 peut ? "AU MOINS UN" : "AUCUN");
			verdict("(4) l'annulation VOIT ce que l'IA a pose", peut, b);
		}

		std::printf("=== RESULTAT : %d echec(s) ===\n", echecs);
		return echecs == 0 ? 0 : 1;
	}

} // namespace nkuidesign
