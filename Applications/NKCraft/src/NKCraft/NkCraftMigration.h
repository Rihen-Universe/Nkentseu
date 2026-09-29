//
// NkCraftMigration.h
// =============================================================================
// Description :
//   NK3DModeler s'appelle NKCraft depuis le 29/09/2026. Ce que l'application a
//   ecrit chez l'utilisateur SOUS L'ANCIEN NOM reste a lui : ce fichier est le
//   seul endroit ou l'ancien nom est encore lu.
//
// Caracteristiques :
//   - Fichier d'etat (recents, disposition, chats, recolte) : si le NOUVEAU
//     fichier manque et que l'ANCIEN existe, on COPIE l'ancien vers le nouveau.
//     Jamais de deplacement, jamais d'effacement : une version d'avant le
//     renommage, relancee, retrouve son fichier intact (fige a la date de la
//     copie, mais entier).
//   - Dossier lu seulement (themes personnels, dossier des projets propose) :
//     le nouveau s'il existe, sinon l'ancien s'il existe. Rien n'est copie --
//     un dossier que l'application ne fait que LIRE n'a pas a etre duplique.
//   - Idempotent : une fois le nouveau fichier la, chaque appel ne coute qu'un
//     test d'existence.
//
// Ce qui n'est PAS migre, et pourquoi :
//   - l'extension `.nk3dm` : c'est un FORMAT (CONVENTIONS_FICHIERS.md §5), pas
//     le nom du produit. Les projets existants s'ouvrent donc sans rien faire ;
//   - le champ « application » des .nk3dm et des metadonnees d'asset : ecrit,
//     jamais relu. Les fichiers d'avant le 29/09 portent « NK3DModeler », les
//     suivants « NKCraft » ; un futur lecteur doit accepter les deux ;
//   - les sondes (`NK_SONDE`) et les chemins explicites (`NK_RECENTS`,
//     `NK_UI_ETAT`, `NK_AI_CHATS`) : ils ne passent jamais par ici, puisqu'ils
//     n'ont pas le droit d'ecrire chez l'utilisateur.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKCRAFT_NKCRAFTMIGRATION_H__
#define __NKENTSEU_NKCRAFT_NKCRAFTMIGRATION_H__

#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace nk3d {

		/// Reprend un fichier d'etat ecrit sous l'ancien nom. Rend vrai si une
		/// copie vient d'avoir lieu.
		///
		/// ⚠️ UN ECHEC EST DIT, ET IL NE COUTE RIEN A L'UTILISATEUR. Profil en
		///    lecture seule, disque plein : l'application part alors d'un etat
		///    vide au nouveau nom, mais l'ancien fichier est toujours la, entier,
		///    et le journal dit ou.
		inline bool NkCraftReprendreFichier(const NkString &ancien, const NkString &nouveau) {
			if (ancien.Empty() || nouveau.Empty())
				return false;
			if (NkFile::Exists(nouveau.CStr()))
				return false;
			if (!NkFile::Exists(ancien.CStr()))
				return false;
			// `overwrite = false` : si une autre instance a cree le nouveau fichier
			// entre le test et la copie, c'est le sien qui gagne.
			if (NkFile::Copy(ancien.CStr(), nouveau.CStr(), false)) {
				NkLog::Instance().Infof("[nkcraft] etat repris de l'ancien nom NK3DModeler : %s -> %s "
										"(l'ancien fichier reste en place)\n",
										ancien.CStr(), nouveau.CStr());
				return true;
			}
			NkLog::Instance().Warn("[nkcraft] reprise IMPOSSIBLE de {0} vers {1} : l'ancien fichier "
								   "est intact, l'application part d'un etat vide\n",
								   ancien.CStr(), nouveau.CStr());
			return false;
		}

		/// Le dossier a LIRE : `nouveau` s'il existe, sinon `ancien` s'il existe,
		/// sinon `nouveau` (celui ou l'utilisateur doit desormais deposer).
		inline NkString NkCraftDossierOuAncien(const NkString &nouveau, const NkString &ancien) {
			if (NkDirectory::Exists(nouveau.CStr()))
				return nouveau;
			if (!ancien.Empty() && NkDirectory::Exists(ancien.CStr()))
				return ancien;
			return nouveau;
		}

	} // namespace nk3d
} // namespace nkentseu

#endif // __NKENTSEU_NKCRAFT_NKCRAFTMIGRATION_H__
