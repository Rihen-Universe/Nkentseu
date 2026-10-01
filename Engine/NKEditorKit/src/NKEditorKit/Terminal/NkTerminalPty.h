#pragma once
// -----------------------------------------------------------------------------
// @File    NkTerminalPty.h
// @Brief   LE VRAI TERMINAL DE LA PLATEFORME : un shell interactif persistant
//          attache a un pseudo-terminal. On ECRIT les frappes, on LIT la sortie
//          brute (avec ses sequences VT), on redimensionne, on ferme sans
//          laisser d'orphelin.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  D'OU IL VIENT — descendu de NKCode le 2026-10-01
// =============================================================================
//  NKCode avait ce moteur seul (`NKCode/Project/NkPty.h/.cpp`). Rihen l'a
//  demande pour toute la famille (« un terminal, on ne sait jamais, ca peut
//  aider ») : ce qui sert plusieurs editeurs descend dans le kit, une fois.
//  NKCode garde une couche MINCE (`NKCode/Project/NkPty.h`) qui renvoie ici.
//
//  TROIS IMPLANTATIONS, UNE PAR FICHIER, CHOISIES A LA COMPILATION :
//    NkTerminalPtyWin32.cpp  Windows : ConPTY (Windows 10 1809 et plus)
//    NkTerminalPtyPosix.cpp  macOS, Linux, BSD : posix_openpt + fork
//    NkTerminalPtyAucun.cpp  Android, iOS, Web : PAS de shell systeme
//  Chaque fichier est entierement garde par sa macro : sous Windows, le
//  fichier POSIX compile en unite VIDE, et inversement.
//
// =============================================================================
//  DEUX DEFAUTS DE L'ANCIEN MOTEUR, CORRIGES EN DESCENDANT
// =============================================================================
//  1. ConPTY : sans STARTF_USESTDHANDLES (poignees nulles), le shell HERITE
//     des sorties standard de l'application quand elles sont redirigees (un
//     lancement depuis une console, un script, une CI). Mesure du 01/10 :
//     NKCode lance depuis un PowerShell ecrivait l'invite du shell DANS la
//     console de l'appelant ; le terminal restait vide.
//  2. Fermeture : le shell etait tue, pas sa descendance. Un `ping -t` lance
//     dedans survivait a la fermeture de l'onglet. Windows : un Job « tuer a
//     la fermeture » ; POSIX : setsid + signal au GROUPE de processus.
//
// ⚠️ CET EN-TETE N'INCLUT NI windows.h NI NKThreading : l'etat vit dans une
//    structure privee (`Impl`). `NkMutex.h` tire `windef.h` et ses macros
//    `near`/`far`/`pascal`, et un en-tete inclus par toute l'interface d'un
//    editeur ne doit pas les propager (piege paye par NkScreenLogSink.h).
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

#if defined(__APPLE__)
#	include <TargetConditionals.h>
#endif

// ── LE CHOIX DU MOTEUR, UNE SEULE FOIS ─────────────────────────────────────
#if defined(_WIN32)
#	define NKENTSEU_TERMINAL_CONPTY 1
#elif defined(__ANDROID__) || defined(__EMSCRIPTEN__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
// Android, iOS (et iPadOS, tvOS, visionOS : TARGET_OS_IPHONE les couvre) et le
// Web : une application n'y lance pas de shell systeme. Le panneau le DIT.
#	define NKENTSEU_TERMINAL_AUCUN 1
#else
#	define NKENTSEU_TERMINAL_POSIX 1
#endif

namespace nkentseu {
	namespace editorkit {

		class NkPty {
			public:
				NkPty();
				~NkPty();
				NkPty(const NkPty &) = delete;
				NkPty &operator=(const NkPty &) = delete;

				/// Lance `cmdline` (« powershell.exe », « wsl.exe -d Ubuntu »,
				/// « /bin/zsh -i »...) attache a un pseudo-terminal de `cols` x `rows`,
				/// dans le dossier `cwd` (vide = celui de l'application).
				/// false si echec : `Erreur()` dit pourquoi, en francais.
				bool Start(const NkString &cmdline, int16 cols, int16 rows, const NkString &cwd = NkString());

				/// Octets bruts (UTF-8) vers l'entree du shell.
				void Write(const char *data, usize len);
				void Write(const char *s);

				/// Nouvelle taille. ConPTY : ResizePseudoConsole ; POSIX : TIOCSWINSZ,
				/// qui fait envoyer SIGWINCH au groupe de premier plan par le noyau.
				void Resize(int16 cols, int16 rows);

				/// Recupere (et vide) la sortie accumulee. Thread-safe.
				void Drain(NkVector<char> &out);

				/// Arrete le shell ET SA DESCENDANCE, joint le fil de lecture, libere
				/// tout. Idempotent.
				void Stop();

				/// Le shell tourne-t-il encore ? (faux apres sa sortie, ex. `exit`).
				bool Running() const;

				int16 Cols() const {
					return mCols;
				}

				int16 Rows() const {
					return mRows;
				}

				/// Identifiant du processus du shell (0 si aucun). Pour les bancs.
				int64 Pid() const {
					return mPid;
				}

				/// Code de sortie du shell une fois termine, -1 tant qu'il ne l'est pas
				/// (ou si la plateforme ne le rend pas).
				int32 CodeSortie() const;

				/// La raison du dernier echec de Start (vide si aucun).
				const NkString &Erreur() const {
					return mErreur;
				}

				/// Cette plateforme sait-elle lancer un shell systeme ?
				static bool Disponible();

				/// « ConPTY », « POSIX (posix_openpt) », « aucun ».
				static const char *NomMoteur();

				/// Le processus `pid` est-il encore vivant ? (bancs : « aucun orphelin »).
				static bool ProcessusVivant(int64 pid);

				/// Tue le processus `pid` (bancs : nettoyer ce qu'une contre-epreuve a
				/// volontairement laisse vivre).
				static void Tuer(int64 pid);

				/// Les processus qui mourront avec ce terminal : le shell et tout ce
				/// qu'il a lance. Windows : le Job ; Linux : la session ; macOS : le
				/// terminal de controle. Sert a Stop() et aux bancs.
				void ProcessusDuGroupe(NkVector<int64> &pids) const;

			private:
				struct Impl;
				Impl *mImpl = nullptr;
				int16 mCols = 80;
				int16 mRows = 24;
				int64 mPid = 0;
				NkString mErreur;
		};

		// ── DECOUPAGE D'UNE LIGNE DE COMMANDE ─────────────────────────────────
		/// « "C:\Program Files\Git\bin\bash.exe" -i -l » -> trois arguments. Les
		/// guillemets doubles regroupent, l'antislash n'echappe PAS (chemins
		/// Windows). Sert au moteur POSIX, qui fait execve SANS passer par
		/// /bin/sh (le PID rendu est alors celui du shell, pas d'un intermediaire),
		/// et aux bancs. Fonction pure.
		void NkTerminalDecouperCommande(const char *ligne, NkVector<NkString> &args);

		/// Mutations de banc (contre-epreuves) : vrai si la variable
		/// d'environnement NK_TERMINAL_MUTATION vaut exactement `nom`. Hors banc,
		/// la variable n'existe pas et rien ne change.
		bool NkTerminalMutation(const char *nom);

	} // namespace editorkit
} // namespace nkentseu
