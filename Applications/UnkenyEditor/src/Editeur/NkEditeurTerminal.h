//
// NkEditeurTerminal.h
// =============================================================================
// Description :
//   L'onglet TERMINAL du tiroir du bas (a cote de Contenu et Journal) : le
//   VRAI terminal de la plateforme -- PowerShell, cmd, Git Bash, MSYS2, chaque
//   distribution WSL sous Windows ; $SHELL, bash, zsh, fish sous macOS et
//   Linux -- en onglets, demarre dans le dossier du projet. Demande de Rihen
//   du 01/10 : « ajoute aussi une partie terminal, on ne sait jamais, ca peut
//   aider ».
//
// Caracteristiques :
//   - TOUT LE TERMINAL EST DANS LE KIT (NKEditorKit/Terminal) : moteur,
//     emulateur, decouverte des shells, dessin, panneau. Ce fichier ne fait
//     que le POSER : le dossier de depart, la police a chasse fixe, le
//     clavier, les demandes de capture, le banc.
//   - Quand le terminal a le clavier, l'editeur n'a AUCUN de ses raccourcis
//     (Suppr, Espace, Q/W/E/R, Ctrl+S...) : NkEditeurTerminalAuClavier.
//   - Un fichier a moi : les autres chantiers du tiroir (Contenu, Journal) ne
//     voient que trois lignes dans NkEditeurTiroir.cpp.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURTERMINAL_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURTERMINAL_H__

#include "NKContainers/String/NkString.h"
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurCadre;
		struct NkEditeurInterface;

		/// L'indice de l'onglet Terminal dans le tiroir (0 Contenu, 1 Journal).
		inline constexpr int32 NK_TIROIR_TERMINAL = 2;

		/// Dessine le panneau Terminal dans `zone` (le corps du tiroir).
		void NkEditeurDessinerTerminal(NkEditeurCadre &c, const nkgui::NkRect &zone);

		/// A appeler A CHAQUE TRAME, avant les raccourcis : fait avancer les
		/// shells (meme onglet cache) et rend VRAI si le terminal a le clavier --
		/// l'editeur ne prend alors aucun raccourci.
		bool NkEditeurTerminalAuClavier(NkEditeurCadre &c);

		/// Les arguments de capture, sans souris :
		///   --tiroir-onglet=terminal|journal|contenu   l'onglet du tiroir au depart
		///   --terminal=powershell,gitbash              des terminaux ouverts au depart
		///   --terminal-menu                            le menu « + » des shells ouvert
		///   --terminal-taper=TEXTE                     tape TEXTE + Entree, apres l'invite
		///   --terminal-focus                           le clavier au terminal (curseur plein)
		/// Rend vrai si l'argument etait pour le terminal.
		bool NkEditeurTerminalArgument(NkEditeurInterface &ui, const NkString &arg);

		/// La police a chasse fixe du terminal (DejaVu Sans Mono, chargee une fois a
		/// `px`). L'application la TELEVERSE (NkCanvasGuiApp::TeleverserPolice) :
		/// les trois polices de la coquille sont proportionnelles.
		nkgui::NkGuiFont &NkEditeurTerminalPolice(float32 px);

		/// `--selftest` : le banc du terminal (NKEditorKit/Terminal/NkTerminalProbe.h),
		/// compte a part. 0 quand tout tient.
		int32 NkEditeurLancerBancTerminal();

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURTERMINAL_H__
