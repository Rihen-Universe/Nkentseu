//
// NkEditeurWorkspaceCpp.h
// =============================================================================
// Description :
//   LE WORKSPACE JENGA DES SCRIPTS C++ d'un projet de jeu -- ce que fait Unreal
//   en generant la solution du projet avant d'ouvrir l'IDE dessus.
//   `<projet>/<Projet>.jenga` declare UNE bibliotheque partagee « Scripts » :
//   les sources `Contenu/**.cpp` et le registre genere par l'editeur
//   (`Intermediaire/Scripts/NkUnkRegistre.cpp`), l'en-tete de la table venant
//   du depot. NKCode s'ouvre dessus ; son « Construire » (jenga build) produit
//   `Intermediaire/Scripts/Jenga/Scripts.dll`, que l'editeur RECHARGE A CHAUD
//   des qu'elle change (NkEditeurScripts.h).
//
// Caracteristiques :
//   - Ecrit (ou mis a jour) par l'editeur quand le projet a au moins un .cpp :
//     a l'ouverture du projet et a la creation d'un script C++.
//   - CE QUE L'UTILISATEUR ECRIT N'EST JAMAIS ECRASE. Seule la partie entre les
//     deux reperes « # >>> UNKENY » et « # <<< UNKENY » est refaite ; le reste
//     du fichier est rendu octet pour octet. Un fichier SANS les deux reperes
//     (un workspace a soi, ou des reperes effaces) n'est pas touche du tout, et
//     le Journal dit comment le faire refaire.
//   - Un fichier deja a jour n'est PAS reecrit (sa date ne bouge pas : NKCode
//     ne recharge pas le workspace pour rien).
//   - Autonome : il n'emploie que Jenga (RegisterJengaGlobalToolchains, la
//     chaine « clang-mingw » sous Windows) -- pas les useconfig du depot, qui
//     exigent un talon `jengaconfig`. Les options de lien sont celles de la
//     compilation directe de l'editeur (DLL qui ne depend que du systeme).
//   - Le chemin du depot (l'en-tete NkUnkenyScriptABI.h) est DANS la partie
//     generee : un projet ouvert par l'editeur d'un autre depot est remis sur
//     celui-la a l'ouverture.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURWORKSPACECPP_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURWORKSPACECPP_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editeur {

		/// Les deux reperes de la partie generee (debut de ligne).
		constexpr const char *NK_WS_DEBUT = "# >>> UNKENY";
		constexpr const char *NK_WS_FIN = "# <<< UNKENY";
		/// Le projet Jenga des scripts, et le dossier de sa DLL (relatif au projet).
		constexpr const char *NK_WS_PROJET = "Scripts";
		constexpr const char *NK_WS_SORTIE = "Intermediaire/Scripts/Jenga";

		enum class NkEtatWorkspace : uint8 {
			NK_CREE = 0,   ///< il n'existait pas : ecrit en entier
			NK_MIS_A_JOUR, ///< la partie generee a change (depot deplace...) : refaite
			NK_A_JOUR,	   ///< rien a faire, rien d'ecrit
			NK_RESPECTE,   ///< fichier sans reperes : laisse tel quel (le Journal le dit)
			NK_ECHEC	   ///< ecriture impossible
		};

		struct NkWorkspaceCpp {
				NkEtatWorkspace etat = NkEtatWorkspace::NK_ECHEC;
				NkString nom;	  ///< « Portes » (le nom du workspace)
				NkString chemin;  ///< « D:/Jeux/Portes/Portes.jenga »
				NkString message; ///< la ligne du Journal (vide si NK_A_JOUR)
		};

		/// Le nom du workspace : l'identifiant du dossier du projet (« Ma Gelée »
		/// -> « MaGelee », NkIdentifiantJeu). `projet` : avec ou sans barre finale.
		NkString NkEditeurWorkspaceNom(const char *projet);
		/// `<projet>/<Nom>.jenga`.
		NkString NkEditeurWorkspaceChemin(const char *projet);
		/// La DLL que produit `jenga build` sur ce workspace, a la plateforme
		/// courante (« <projet>/Intermediaire/Scripts/Jenga/Scripts.dll »).
		NkString NkEditeurWorkspaceDll(const char *projet);

		/// La partie generee, reperes compris (exposee pour le banc).
		NkString NkEditeurWorkspacePartie(const char *nom, const char *depot);
		/// Le fichier complet d'un workspace neuf : en-tete, partie generee, et la
		/// place laissee a l'utilisateur.
		NkString NkEditeurWorkspaceNeuf(const char *nom, const char *depot);
		/// Remplace la partie generee de `texte` par `partie` (tout le reste rendu
		/// tel quel). false : les deux reperes n'y sont pas (dans l'ordre).
		bool NkEditeurWorkspaceRemplacer(const NkString &texte, const NkString &partie, NkString &sortie);

		/// Ecrit ou met a jour le workspace de `projet` (voir l'en-tete). `depot` :
		/// le depot Nkentseu (NkTrouverDepot, barre finale ou non).
		NkWorkspaceCpp NkEditeurAssurerWorkspaceCpp(const char *projet, const char *depot);

		/// La plateforme de `jenga build` sur CETTE machine, comme NKCode l'ecrit
		/// (« Windows-x86_64 »).
		const char *NkEditeurWorkspacePlateforme() noexcept;
		/// `jenga <arguments> --jenga-file "<workspace>"`, lance dans le dossier du
		/// workspace et ATTENDU (bancs, preuve). `sortie` recoit ses lignes, sans
		/// sequences ANSI. Rend son code de sortie ; -1 s'il n'a pas pu partir.
		int32 NkEditeurJengaSurWorkspace(const char *workspace, const char *arguments, NkVector<NkString> &sortie);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURWORKSPACECPP_H__
