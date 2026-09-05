// =============================================================================
// NKPlatform/NkShell.cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include "pch.h"

#include "NKPlatform/NkShell.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
// ⚠️ `windows.h` EN PREMIER : `shellapi.h` s'appuie sur `DECLARE_HANDLE` et
//    `EXTERN_C`, qui viennent de lui. L'ordre alphabetique du formateur casse
//    la compilation — c'est une dependance d'en-tete, pas un gout.
#include <windows.h>
// clang-format off
#include <shellapi.h>
// clang-format on
#endif

namespace nkentseu {
	namespace shell {

		namespace {

			bool Vide(const char *s) {
				return !s || !*s;
			}

#ifndef _WIN32
			// ⚠️ LES GUILLEMETS SIMPLES SONT DOUBLES, PAS ECHAPPES PAR UN ANTISLASH :
			//    dans un shell POSIX, `\'` n'existe pas a l'interieur de `'...'`. La
			//    seule forme correcte est de fermer, mettre `'\''`, rouvrir. Un chemin
			//    d'utilisateur peut contenir une apostrophe (« Rodolf's »), et sans ca
			//    la commande devient autre chose que ce qu'on croit executer.
			void CiterPosix(const char *chemin, char *out, size_t cap) {
				size_t k = 0;
				if (cap == 0)
					return;
				if (k + 1 < cap)
					out[k++] = '\'';
				for (const char *p = chemin; *p && k + 5 < cap; ++p) {
					if (*p == '\'') {
						out[k++] = '\'';
						out[k++] = '\\';
						out[k++] = '\'';
						out[k++] = '\'';
					} else
						out[k++] = *p;
				}
				if (k + 1 < cap)
					out[k++] = '\'';
				out[k] = '\0';
			}
#endif

		} // namespace

		bool Ouvrir(const char *chemin) {
			if (Vide(chemin))
				return false;
#ifdef _WIN32
			// ShellExecuteA rend une valeur > 32 en cas de succes (API historique).
			const HINSTANCE r = ShellExecuteA(NULL, "open", chemin, NULL, NULL, SW_SHOWNORMAL);
			return (INT_PTR)r > 32;
#elif defined(__APPLE__)
			char cite[2048], cmd[2176];
			CiterPosix(chemin, cite, sizeof(cite));
			snprintf(cmd, sizeof(cmd), "open %s >/dev/null 2>&1 &", cite);
			return system(cmd) == 0;
#else
			char cite[2048], cmd[2176];
			CiterPosix(chemin, cite, sizeof(cite));
			snprintf(cmd, sizeof(cmd), "xdg-open %s >/dev/null 2>&1 &", cite);
			return system(cmd) == 0;
#endif
		}

		bool Reveler(const char *cheminFichier) {
			if (Vide(cheminFichier))
				return false;
#ifdef _WIN32
			// `explorer /select,"<chemin>"` ouvre le dossier ET y met le fichier en
			// evidence. ⚠️ Explorer rend souvent un code d'echec meme quand il a
			// ouvert la fenetre : on ne peut donc PAS se fier a sa valeur de retour.
			// On considere l'appel reussi s'il a demarre, et l'appelant garde de
			// toute facon le chemin lisible.
			char arg[2176];
			snprintf(arg, sizeof(arg), "/select,\"%s\"", cheminFichier);
			const HINSTANCE r = ShellExecuteA(NULL, "open", "explorer.exe", arg, NULL, SW_SHOWNORMAL);
			return (INT_PTR)r > 32;
#elif defined(__APPLE__)
			char cite[2048], cmd[2176];
			CiterPosix(cheminFichier, cite, sizeof(cite));
			snprintf(cmd, sizeof(cmd), "open -R %s >/dev/null 2>&1 &", cite);
			return system(cmd) == 0;
#else
			// ⚠️ AUCUN STANDARD FREEDESKTOP POUR « SELECTIONNER » UN FICHIER : chaque
			//    gestionnaire a sa propre option. On ouvre donc LE DOSSIER, et on le
			//    dit ici plutot que de laisser croire a une selection.
			const char *fin = nullptr;
			for (const char *p = cheminFichier; *p; ++p)
				if (*p == '/' || *p == '\\')
					fin = p;
			if (!fin)
				return Ouvrir(".");
			char dossier[2048];
			const size_t n = (size_t)(fin - cheminFichier);
			const size_t m = n < sizeof(dossier) - 1 ? n : sizeof(dossier) - 1;
			memcpy(dossier, cheminFichier, m);
			dossier[m] = '\0';
			return Ouvrir(dossier);
#endif
		}

	} // namespace shell
} // namespace nkentseu
