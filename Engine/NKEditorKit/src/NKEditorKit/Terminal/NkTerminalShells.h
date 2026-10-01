#pragma once
// -----------------------------------------------------------------------------
// @File    NkTerminalShells.h
// @Brief   QUELS SHELLS Y A-T-IL SUR CETTE MACHINE ? La liste du menu « + » du
//          terminal : chaque shell trouve, son nom, sa commande, son chemin.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
//  Windows : PowerShell 7 (pwsh), Windows PowerShell, Invite de commandes,
//            Git Bash, MSYS2, et CHAQUE distribution WSL (`wsl -l -q`).
//  Unix    : le shell de la session ($SHELL), puis ceux de /etc/shells qui
//            existent (bash, zsh, fish, sh...), sans doublon.
//  Android, iOS, Web : liste VIDE -- il n'y a pas de shell systeme.
//
//  ⚠️ DEUX DEFAUTS DE LA DETECTION DE NKCode, A NE PAS REFAIRE :
//     1. `_popen("wsl --list --quiet")` dans une application FENETREE ouvre une
//        console le temps de la commande (un eclair noir a l'ecran). Ici :
//        CreateProcess + CREATE_NO_WINDOW, et un delai borne.
//     2. Sans WSL, `wsl -l -q` ECRIT un message (« Le sous-systeme Windows pour
//        Linux n'est pas installe... ») avec un code d'erreur : NKCode en
//        faisait une « distribution » nommee par la premiere ligne du
//        message. Ici : code de sortie verifie ET nom de distribution valide
//        (lettres, chiffres, . _ -) exige.
//
//  Les parseurs sont des FONCTIONS PURES : le banc les essaie sur n'importe
//  quelle plateforme, avec des octets fabriques (UTF-16 avec BOM, message
//  d'erreur, /etc/shells commente).
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		/// La FAMILLE d'un shell : elle choisit l'icone et la couleur de l'onglet.
		enum class NkShellGenre : uint8 {
			PowerShell7 = 0,
			WindowsPowerShell,
			Cmd,
			GitBash,
			Msys2,
			Wsl,
			Bash,
			Zsh,
			Fish,
			Sh,
			Autre,
			Count
		};

		struct NkShellDecouvert {
				NkShellGenre genre = NkShellGenre::Autre;
				NkString nom;		///< « PowerShell 7 », « WSL : Ubuntu-22.04 », « zsh »
				NkString commande;	///< la ligne complete passee a NkPty::Start
				NkString chemin;	///< l'executable (detail gris du menu)
				NkString distro;	///< WSL seulement
				bool session = false; ///< le shell de la session ($SHELL) ou le premier de Windows
		};

		/// La liste des shells de CETTE machine, dans l'ordre du menu. Le premier
		/// est le shell par defaut. `avecWsl` = faux : la detection WSL (qui lance
		/// un processus) est sautee -- a reserver aux appels qui ne peuvent pas
		/// attendre.
		void NkTerminalDecouvrirShells(NkVector<NkShellDecouvert> &out, bool avecWsl = true);

		/// Le shell de repli quand rien n'a ete decouvert (cmd sous Windows,
		/// /bin/sh ailleurs). Toujours une commande valide sur une plateforme
		/// qui a des shells.
		NkShellDecouvert NkTerminalShellDeRepli();

		/// Les distributions WSL installees (vide hors Windows, ou sans WSL).
		/// Lance `wsl -l -q` SANS fenetre, borne a quelques secondes.
		void NkTerminalListerWsl(NkVector<NkString> &distros);

		/// L'INVITE de la famille, par INDICES de palette (le chemin en bleu,
		/// « > » en vert : elle suit le theme) et qui ANNONCE le dossier courant
		/// (OSC 9;9) pour l'en-tete du panneau.
		///   PowerShell : la suite d'arguments a ajouter apres l'executable ;
		///   cmd        : le TEXTE de `prompt` (« $E]9;9;$P... »).
		const char *NkTerminalInvitePowerShell();
		const char *NkTerminalInviteCmdTexte();

		/// « PowerShell », « Invite de commandes »... (nom de FAMILLE).
		const char *NkShellGenreNom(NkShellGenre g);

		/// Le sigle trace sur l'icone : « PS », « C:\ », « $ », « W »...
		const char *NkShellGenreSigle(NkShellGenre g);

		/// La famille d'un executable d'apres son nom (« /usr/bin/zsh » -> Zsh).
		NkShellGenre NkTerminalGenreDuChemin(const char *chemin);

		// ── Parseurs purs (bancs) ─────────────────────────────────────────────
		/// La sortie de `wsl -l -q` : UTF-16LE (avec ou sans BOM) ou UTF-8 (si
		/// WSL_UTF8=1). Ne garde que des NOMS VALIDES ; un message d'erreur ne
		/// passe pas.
		void NkTerminalLireListeWsl(const char *octets, usize n, NkVector<NkString> &distros);

		/// Le contenu de /etc/shells : un chemin absolu par ligne, « # » commente.
		/// Ecarte les pseudo-shells (nologin, false, git-shell, rbash, screen,
		/// tmux) et les doublons de NOM (/bin/bash et /usr/bin/bash).
		void NkTerminalLireEtcShells(const char *texte, NkVector<NkString> &chemins);

	} // namespace editorkit
} // namespace nkentseu
