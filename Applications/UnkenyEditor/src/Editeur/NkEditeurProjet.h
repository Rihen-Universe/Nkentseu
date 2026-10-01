//
// NkEditeurProjet.h
// =============================================================================
// Description :
//   Les REGLAGES DU PROJET, gardes avec lui : `<dossier du projet>/.nkprojet`,
//   un texte « cle=valeur » par ligne. Le premier : le dossier de sortie de
//   la fenetre « Construire » (2026-10-01, retour 6 de Rihen : « il n'y a pas
//   de bouton pour choisir le dossier de generation du jeu »).
//
// Caracteristiques :
//   - RETRO-COMPATIBLE dans les deux sens : un projet sans `.nkprojet` prend
//     les valeurs par defaut ; une cle inconnue (d'une version plus recente)
//     est GARDEE telle quelle a la reecriture ; « # » commence un commentaire.
//   - Un chemin SOUS le dossier du projet s'y ecrit RELATIF (« Construit ») :
//     le projet se deplace ou se partage sans casser ses reglages.
//   - Le dossier du projet est celui de NkEditeurDossierProjet (celui qui
//     porte « Contenu »).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURPROJET_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURPROJET_H__

#include "Editeur/NkEditeurModele.h"

#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editeur {

		/// Le nom du fichier des reglages, a la racine du projet.
		constexpr const char *NK_PROJET_REGLAGES = ".nkprojet";
		/// La cle du dossier de sortie de « Construire ».
		constexpr const char *NK_PROJET_SORTIE = "construire.sortie";

		/// La valeur de `cle` dans les reglages du projet ; vide si absente (ou
		/// pas de fichier : un projet d'avant).
		NkString NkEditeurProjetLire(NkEditeurModele &m, const char *cle);

		/// Ecrit `cle=valeur` (remplace la ligne de la cle, garde les autres).
		bool NkEditeurProjetEcrire(NkEditeurModele &m, const char *cle, const char *valeur);

		/// Le projet est-il un VRAI projet (une scene de l'utilisateur), et non la
		/// scene de secours de l'editeur dans son dossier d'application ?
		bool NkEditeurProjetReel(NkEditeurModele &m);

		/// Le dossier de sortie de « Construire » pour ce projet : celui qu'il a
		/// retenu, sinon `<dossier du projet>/Construit`, sinon (pas de vrai projet)
		/// l'ancien defaut, Documents/Unkeny/Jeux (NkSortieParDefaut).
		NkString NkEditeurSortieDuProjet(NkEditeurModele &m);

		/// Retient `sortie` pour ce projet (relatif s'il est dedans).
		bool NkEditeurRetenirSortie(NkEditeurModele &m, const char *sortie);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURPROJET_H__
