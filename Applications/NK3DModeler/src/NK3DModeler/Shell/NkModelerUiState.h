#pragma once
// -----------------------------------------------------------------------------
// @File    NkModelerUiState.h
// @Brief   LA DISPOSITION SURVIT A LA FERMETURE : les fractions des separateurs,
//          ecrites dans un petit fichier clef=valeur et relues au demarrage.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// [!] POURQUOI CE FICHIER EXISTE — UNE PROMESSE QUI N'ETAIT PAS TENUE
//   `NkModelerInput.h` ecrit, a cote des quatre fractions :
//     « a la prochaine ouverture, la disposition se retrouve identique »
//     « fermer puis rouvrir retrouve sa largeur, pas le minimum »
//   MESURE DU 25/09 : c'etait FAUX pour l'application. Les quatre fractions sont
//   des membres de `NkModelerState`, jamais ecrites nulle part. Recherche faite
//   sur tout le depot : `SaveUiState` / `LoadUiState` existent bien -- mais dans
//   `NkEditorShell` (NKEditorKit), et NK3DModeler n'emploie PAS `NkEditorShell`.
//   Ses seuls etats persistants sont la liste des recents et les themes.
//   La phrase parlait donc de fermer un PANNEAU, pas l'application ; elle se
//   lisait comme l'inverse. *Declarer n'est pas livrer.*
//
// CE QU'IL FAIT, ET RIEN DE PLUS
//   Cinq nombres, plus un interrupteur d'affichage (25/09). Pas de docks, pas d'onglets, pas de position de fenetre : ce
//   qui n'est pas demande n'est pas ecrit, et un format qu'on n'etend pas est un
//   format qu'on ne casse pas.
//
// ⚠️ IL NE TOUCHE PAS L'ETAT DE RODOLF QUAND UNE SONDE TOURNE
//   Meme porte que la liste des recents (`NkRecentFilePath`, 25/09) :
//   `NK_UI_ETAT=<chemin>` pour choisir ou, et sous `NK_SONDE` la redirection est
//   LE DEFAUT. Une sonde qui reecrit la disposition de Rodolf modifie ce qu'elle
//   mesure -- sa liste de recents a deja paye ce prix (105 entrees).
// -----------------------------------------------------------------------------

#include "NK3DModeler/Shell/NkModelerInput.h" // NkModelerState
#include "NKEditorKit/NkSondeInerte.h"		  // la porte de redirection des sondes
#include "NKPlatform/NkEnv.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace nkentseu {
	namespace nk3d {

		/// Ou vit le fichier. Meme patron que `NkRecentFilePath()`.
		inline NkString NkUiStatePath() {
			if (const char *v = env::GetEnvVar("NK_UI_ETAT"))
				if (*v)
					return NkString(v);
			{
				const NkString red = editorkit::NkSondeChemin(nullptr, "nk3dmodeler_ui.cfg");
				if (!red.Empty())
					return red;
			}
			const char *home = env::GetEnvVar("USERPROFILE");
			if (!home || !*home)
				home = env::GetEnvVar("HOME");
			if (home && *home) {
				NkString p(home);
				p.Append("/.nk3dmodeler_ui.cfg");
				return p;
			}
			return NkString(".nk3dmodeler_ui.cfg");
		}

		/// Bornes de TOUTES les fractions ecrites ici. Elles sont appliquees A LA
		/// LECTURE, pas seulement a l'ecriture.
		///
		/// ⚠️ MESUREZ LA BORNE, NE LA SUPPOSEZ PAS. Un precedent tout proche : le
		///    tiroir de rail declarait 999 980 px de large et faisait apparaitre
		///    une barre de defilement fantome. Un fichier de configuration est une
		///    ENTREE : edite a la main, tronque par une coupure de courant, ou
		///    ecrit par une version d'apres, il peut porter n'importe quoi. Sans
		///    cette borne a la lecture, une ligne `browser.tree=9999` fermerait la
		///    grille et l'application s'ouvrirait inutilisable, sans aucun moyen
		///    de revenir en arriere a la souris.
		inline float32 NkUiFracBornee(float32 f, float32 mini, float32 maxi) {
			if (!(f == f)) // NaN : un `atof` sur une ligne abimee en rend
				return mini;
			if (f < mini)
				return mini;
			if (f > maxi)
				return maxi;
			return f;
		}

		/// `cheminExplicite` sert A LA SONDE, et a elle seule : elle eprouve
		/// l'aller-retour sans toucher au fichier de l'utilisateur ni dependre
		/// d'une variable d'environnement qu'il faudrait penser a poser.
		inline void NkSaveUiState(const NkModelerState &st, const char *cheminExplicite = nullptr) {
			const NkString chemin = (cheminExplicite && *cheminExplicite) ? NkString(cheminExplicite)
																		 : NkUiStatePath();
			FILE *f = std::fopen(chemin.CStr(), "w");
			if (!f)
				return; // un profil en lecture seule n'est pas une raison de planter
			std::fprintf(f, "# NK3DModeler — disposition. Ecrit par l'application.\n");
			std::fprintf(f, "left=%.4f\n", (double)st.leftFrac);
			std::fprintf(f, "right=%.4f\n", (double)st.rightFrac);
			std::fprintf(f, "browser=%.4f\n", (double)st.browserFrac);
			std::fprintf(f, "props=%.4f\n", (double)st.propsFrac);
			std::fprintf(f, "browser.tree=%.4f\n", (double)st.browserTreeFrac);
			// L'INTERRUPTEUR DES COMPTEURS, avec les fractions : c'est la meme
			// chose sous une autre designation -- ce que l'utilisateur a regle
			// dans son interface et qu'il doit retrouver.
			std::fprintf(f, "compteurs=%d\n", st.compteursOn ? 1 : 0);
			std::fprintf(f, "aide=%d\n", st.aideOn ? 1 : 0);
			std::fclose(f);
		}

		inline void NkLoadUiState(NkModelerState &st, const char *cheminExplicite = nullptr) {
			const NkString chemin = (cheminExplicite && *cheminExplicite) ? NkString(cheminExplicite)
																		 : NkUiStatePath();
			FILE *f = std::fopen(chemin.CStr(), "r");
			if (!f)
				return; // premier lancement : les valeurs par defaut tiennent
			char ligne[256];
			while (std::fgets(ligne, sizeof(ligne), f)) {
				if (ligne[0] == '#')
					continue;
				char *eq = std::strchr(ligne, '=');
				if (!eq)
					continue;
				*eq = 0;
				const char *clef = ligne;
				const float32 v = (float32)std::atof(eq + 1);
				// ⚠️ `leftFrac` et `rightFrac` acceptent ZERO, et c'est VOULU : zero
				//    est la valeur « jamais touchee », qui laisse `Compute()` poser
				//    son plancher en pixels (kMinLeftW / kMinRightW). La borner a
				//    0,08 changerait la largeur d'ouverture d'un utilisateur qui n'a
				//    jamais bouge ses separateurs.
				if (std::strcmp(clef, "left") == 0)
					st.leftFrac = (v <= 0.f) ? 0.f : NkUiFracBornee(v, 0.08f, 0.60f);
				else if (std::strcmp(clef, "right") == 0)
					st.rightFrac = (v <= 0.f) ? 0.f : NkUiFracBornee(v, 0.08f, 0.60f);
				else if (std::strcmp(clef, "browser") == 0)
					st.browserFrac = NkUiFracBornee(v, 0.08f, 0.60f);
				else if (std::strcmp(clef, "props") == 0)
					st.propsFrac = NkUiFracBornee(v, 0.08f, 0.60f);
				else if (std::strcmp(clef, "browser.tree") == 0)
					st.browserTreeFrac = NkUiFracBornee(v, kBrowserTreeFracMin, kBrowserTreeFracMax);
				// ⚠️ TOUT CE QUI N'EST PAS EXACTEMENT « 1 » EST ETEINT, y compris une
				//    ligne abimee. Le defaut SUR d'un interrupteur d'affichage est
				//    ETEINT : allumer sur une valeur qu'on n'a pas comprise, c'est
				//    exactement ce qui polluait les captures de Rodolf.
				else if (std::strcmp(clef, "compteurs") == 0)
					st.compteursOn = (v > 0.5f && v < 1.5f);
				// ⚠️ LE DEFAUT SUR EST L'INVERSE DE CELUI DES COMPTEURS, et c'est
				//    voulu : l'aide est ALLUMEE par defaut, donc seule la valeur
				//    explicite « 0 » l'eteint. Une ligne abimee la laisse allumee --
				//    perdre l'aide sur un fichier mal relu couterait plus cher a
				//    Rodolf que de la garder.
				else if (std::strcmp(clef, "aide") == 0)
					st.aideOn = !(v > -0.5f && v < 0.5f);
			}
			std::fclose(f);
		}

	} // namespace nk3d
} // namespace nkentseu
