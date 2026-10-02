// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkWindowIdentite.h — L'IDENTITE D'UNE APPLICATION AUPRES DE L'OS
//
// POURQUOI (2026-10-02, Rihen) : « je lance NKCode et Unkeny mais les deux
// icones se superposent, pourtant ce n'est pas la meme application ». La barre
// des taches de Windows REGROUPE les fenetres par AppUserModelID (AUMID). Le
// dorsal Win32 posait le MEME identifiant, `Rihen.Nkentseu.Pong`, dans TOUS les
// processus (un reste du jour ou Pong etait la seule application installee) :
// pour Windows, NKCode, UnkenyEditor, Nogee... etaient une seule application,
// donc un seul bouton, une seule icone (celle du premier lance).
//
// LA REGLE, par ordre de priorite :
//   1. la variable d'environnement `NkAppUserModelID` (installations
//      multi-versions, deja promise par l'ancien code : elle reste) ;
//   2. l'identifiant DECLARE par l'application (`NkWindowDeclarerIdentite`),
//      avant sa premiere fenetre ;
//   3. sinon `Rihen.Nkentseu.<NomDeL'exe>` : deux executables distincts ont
//      donc TOUJOURS deux identites distinctes, sans qu'aucune application
//      n'ait rien a ecrire. (Pong.exe garde `Rihen.Nkentseu.Pong`.)
//
// Hors Windows, l'identite est calculee de la meme facon (elle sert aux bancs
// et aux plateformes qui en auront l'usage : `app_id` Wayland, WM_CLASS X11) ;
// seul Win32 la POSE aujourd'hui.
// =============================================================================

#include "NKContainers/String/NkString.h"

namespace nkentseu {

	/// Declare l'identifiant de l'application (AUMID sous Windows). A appeler
	/// AVANT la premiere fenetre : l'OS lit l'identifiant du processus quand la
	/// fenetre apparait. `nullptr` ou vide : retour au calcul par defaut.
	void NkWindowDeclarerIdentite(const char *identifiant) noexcept;

	/// Le nom de l'executable de CE processus, sans dossier ni extension
	/// (« UnkenyEditor », « NKCode »). Vide si l'OS ne le dit pas.
	NkString NkWindowNomExecutable() noexcept;

	/// Un nom quelconque rendu en segment d'AUMID : lettres, chiffres, '.', '-'
	/// gardes ; tout le reste devient '-' ; 64 caracteres au plus.
	NkString NkWindowSegmentIdentite(const char *nom) noexcept;

	/// L'identifiant par defaut d'un executable : `Rihen.Nkentseu.<segment>`.
	/// Fonction PURE : c'est elle que le banc compare pour deux noms.
	NkString NkWindowIdentitePourExecutable(const char *nomExecutable) noexcept;

	/// L'identifiant que CE processus prendra : environnement > declare > exe.
	NkString NkWindowIdentiteCalculee() noexcept;

	/// Pose l'identifiant calcule (une seule fois par processus ; les appels
	/// suivants ne font rien). Rend vrai si l'OS l'a accepte. Sans objet hors
	/// Windows (rend vrai).
	bool NkWindowAppliquerIdentite() noexcept;

	/// L'identifiant effectivement tenu par l'OS pour ce processus (Windows :
	/// GetCurrentProcessExplicitAppUserModelID). Vide s'il n'y en a pas.
	NkString NkWindowIdentitePosee() noexcept;

} // namespace nkentseu
