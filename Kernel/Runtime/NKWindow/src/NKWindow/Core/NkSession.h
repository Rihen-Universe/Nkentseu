#pragma once

// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkSession.h
// -----------------------------------------------------------------------------
// LA REOUVERTURE APRES UN REDEMARRAGE DU PC (decision de Rodolf, 07/11/2026 :
// « le lancement de toutes les fenetres fermees a la fermeture du systeme, qui n'ont
// pas ete fermees manuellement, comme VS Code »).
//
// Ce service ne sait PAS ce que chaque application garde ouvert : il sait seulement
//   1. dire a l'application que le SYSTEME met fin a la session (et non l'utilisateur
//      qui ferme une fenetre) : un rappel, appele une fois, de facon synchrone, avant
//      que le processus soit tue -- c'est l'instant d'ecrire la memoire ;
//   2. demander au systeme de relancer l'application apres le redemarrage, avec
//      l'argument `--reouvrir-session` ;
//   3. reconnaitre cet argument au lancement.
// Aucun type du systeme dans cette interface. Une fenetre fermee a la main ne revient
// jamais : c'est la memoire de l'application qui le garantit (elle ne l'inscrit pas, ou
// la desinscrit a la fermeture) ; ce service ne la rouvre pas lui-meme.
//
// TROIS CHOIX (NkSessionRouvrir) :
//   AuRedemarrage : le systeme relance l'application ; un lancement ordinaire (double-clic)
//                   ne reprend rien ;
//   AuDoubleClic  : aucune relance par le systeme ; un lancement ordinaire reprend ;
//   Aucun         : ni l'un ni l'autre (l'application s'ouvre vide, ou sur le chemin demande).
// Le choix est celui de l'application (son reglage) ; ce service l'applique cote systeme.
//
// PLATEFORMES
//   Windows : WM_QUERYENDSESSION / WM_ENDSESSION (NkWin32EventSystem.cpp) et
//             RegisterApplicationRestart. N'ACTIVE AUCUN REGLAGE de Windows : si la personne
//             a coupe « Redemarrer les applications » (Parametres > Comptes > Options de
//             connexion), Windows ne relance rien -- `SystemeRelanceActive()` le dit.
//   Autres  : repli qui compile : le rappel n'est jamais appele par le systeme, aucune relance
//             n'est inscrite (`Activer` rend false pour AuRedemarrage), le reste fonctionne.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {

	enum class NkSessionRouvrir : int32 {
		AuRedemarrage = 0,
		AuDoubleClic = 1,
		Aucun = 2
	};

	class NkSession {
		public:
			using Rappel = void (*)(void *utilisateur);

			/// L'argument que le systeme donne a l'application relancee.
			static const char *ArgumentRelance();

			/// Applique le choix cote systeme. AuRedemarrage inscrit la relance ; les deux autres la
			/// retirent. Rend vrai si le systeme a accepte (toujours vrai pour les deux autres).
			static bool Activer(NkSessionRouvrir mode);
			static NkSessionRouvrir Mode();

			/// Le rappel de la fin de session : appele une fois, de facon synchrone, quand le systeme
			/// met fin a la session. Un seul rappel (le dernier remplace).
			static void SurFinDeSession(Rappel rappel, void *utilisateur);

			/// Vrai des que le systeme a mis fin a la session (le rappel a ete appele).
			static bool FinDeSessionAnnoncee();

			/// Les arguments du lancement portent-ils `--reouvrir-session` ?
			static bool LanceParLeSysteme(int argc, const char *const *argv);

			/// Windows : le reglage « Redemarrer les applications » est-il active pour cette personne ?
			/// (lecture seule du registre ; jamais modifie). Les autres systemes : faux.
			/// @param connu mis a faux quand le systeme ne sait pas le dire.
			static bool SystemeRelanceActive(bool *connu = nullptr);

			// ── Appeles par les dorsaux de plateforme (jamais par les applications) ──
			/// Le systeme demande si la session peut finir. Rend vrai : NKWindow ne s'y oppose jamais.
			static bool NotifierDemande();
			/// Le systeme annonce la fin de session (`reelle` : elle aura bien lieu).
			static void NotifierFin(bool reelle); // pas : le rappel de l application peut tout faire, et tirerait std::terminate (libstdc++)
	};

} // namespace nkentseu
