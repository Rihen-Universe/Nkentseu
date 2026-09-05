// =============================================================================
// NKPlatform/NkShell.h
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Ouvrir un fichier ou un dossier AVEC LE SYSTEME (l'application associée, ou
// l'explorateur de fichiers positionné sur le fichier).
//
// POURQUOI ICI, ET PAS DANS L'APPLICATION
//   « Ouvrir le dossier » n'est pas une connaissance d'éditeur : c'est un
//   service de la plateforme, au même titre que les variables d'environnement
//   voisines. Trois applications de la maison en auront besoin le jour où elles
//   exportent quelque chose ; l'écrire dans NkUIDesign en ferait la première
//   des trois copies.
//
// ⚠️ LES DEUX FONCTIONS PEUVENT ÉCHOUER, ET C'EST UN RÉSULTAT, PAS UNE PANNE.
//    Une machine sans explorateur de fichiers (un conteneur, une session sans
//    bureau) n'en a pas. Elles rendent alors `false`, et l'appelant doit avoir
//    prévu quoi faire — chez nous, laisser le chemin lisible et copiable.
//    Une fonction qui prétendrait toujours réussir obligerait l'utilisateur à
//    deviner pourquoi rien ne s'ouvre.
//
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKSHELL_H
#define NKENTSEU_PLATFORM_NKSHELL_H

#include "NKPlatform/NkPlatformExport.h"

namespace nkentseu {
	namespace shell {

		/// Ouvre `chemin` avec l'application que le système lui associe (un `.png`
		/// dans la visionneuse, un dossier dans l'explorateur).
		/// @return faux si le système ne sait pas ouvrir, ou n'a rien pour le faire.
		NKENTSEU_PLATFORM_API bool Ouvrir(const char *chemin);

		/// Ouvre le dossier CONTENANT `cheminFichier`, en y sélectionnant le fichier
		/// quand le système sait le faire (Windows : `explorer /select,` ;
		/// macOS : `open -R`). Sinon, ouvre simplement le dossier.
		/// @return faux si rien n'a pu être ouvert.
		NKENTSEU_PLATFORM_API bool Reveler(const char *cheminFichier);

	} // namespace shell
} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_NKSHELL_H
