//
// NkEditeurScriptsUi.h
// =============================================================================
// Description :
//   Les GESTES des scripts dans l'interface de l'editeur : ce que montre la
//   carte « Scripts » des Details (la liste ordonnee des scripts de l'entite,
//   leur statut, leurs variables, ce qu'on peut encore ajouter), les entrees
//   « Script C++ » et « Blueprint » du « + Ajouter » du navigateur, « Ajouter
//   un composant > Script : ... », et le double-clic sur un .cpp ou un .nkbp.
//
// Caracteristiques :
//   - (2026-10-02, fusion) PLUS DE BLOC BRUT : la carte « Scripts » est DESSINEE
//     par NkEditeurDetails.cpp avec les rangees des autres cartes (meme style,
//     pastille « Acteur »). Ce fichier ne dit que le SENS : statut, variables,
//     ouvrir, ajouter. L'ancien NkEditeurBlocScript (widgets NKGui sous les
//     cartes) est retire.
//   - Les actions vivent dans la plage 2200-2299 (NK_A_SCRIPT, NkEditeurInterface.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURSCRIPTSUI_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURSCRIPTSUI_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKECS/NkECSDefines.h"
#include "Unkeny/Script/NkUnkenyScript.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurCadre;
		struct NkEditeurModele;
		struct NkEditeurScripts;

		/// Les actions des scripts : NK_A_SCRIPT + l'une d'elles.
		enum NkActionScript : int32 {
			NK_SCRIPT_NOUVEAU_CPP = 0, ///< « + Ajouter > Script C++ » : le fichier, ouvert dans l'editeur de texte
			NK_SCRIPT_NOUVEAU_BP,	   ///< « + Ajouter > Blueprint » : l'asset, ouvert dans la page du graphe
			NK_SCRIPT_COMPILER,		   ///< recompiler les scripts C++ du projet maintenant
			/// (2026-10-02) « Ajouter un composant > Script : ... » : + k, le script
			/// `NkEditeurInterface::scriptsProposes[k]` sur la selection (k < 90).
			NK_SCRIPT_AJOUTER = 10
		};

		/// Une variable a montrer pour un script de l'entite : declaree par sa
		/// definition (avec son defaut), ou portee encore par le composant alors que
		/// la definition ne la connait plus (gardee, sauvee).
		struct NkVariableScriptMontree {
				NkString nom;
				unkeny::NkTypeVarScript type = unkeny::NkTypeVarScript::NK_REEL;
				math::NkVec2f defaut{0.f, 0.f};
				/// (2026-10-01) le defaut d'un TEXTE (une entite : son nom, vide).
				NkString texte;
		};

		/// Le statut du script `k` de l'entite : « C++ », « Blueprint », « inconnu »,
		/// « refusé » ou « EN FAUTE » ; `detail` dit pourquoi (vide si tout va bien).
		const char *NkEditeurStatutScript(NkEditeurScripts &s, ecs::NkEntityId id, uint32 k, const char *ref, NkString &detail);
		/// Les variables du script `k` de `sc`, dans l'ordre de sa definition.
		void NkEditeurVariablesScript(NkEditeurScripts &s, const unkeny::NkScript2D &sc, uint32 k,
									  NkVector<NkVariableScriptMontree> &sortie);
		/// Les scripts du projet qu'on peut encore poser sur `id` (pas deja dessus).
		void NkEditeurScriptsAAjouter(NkEditeurModele &m, ecs::NkEntityId id, NkVector<NkString> &sortie);
		/// Pose `ref` sur `id` (le composant est cree s'il manque ; les variables
		/// exposees d'une classe C++ prennent leur defaut). Annonce le resultat.
		bool NkEditeurAjouterScript(NkEditeurModele &m, ecs::NkEntityId id, const char *ref);
		/// « Ouvrir » un script de l'entite : un Blueprint dans sa page (son onglet),
		/// une classe C++ dans l'editeur de texte.
		void NkEditeurOuvrirScript(NkEditeurCadre &c, const char *ref);
		/// L'etat de la compilation C++ en une ligne (vide : rien a dire).
		NkString NkEditeurEtatCompilationScripts(NkEditeurScripts &s);

		/// Une action de la plage NK_A_SCRIPT.
		void NkEditeurActionScript(NkEditeurCadre &c, int32 action);
		/// Le double-clic du navigateur sur `cheminNav` : un .cpp s'ouvre dans
		/// NKCode, SUR le workspace Jenga du projet (ou dans l'editeur du systeme ;
		/// le Journal dit pourquoi), un .nkbp dans la page du graphe.
		/// false : ce n'est pas un script (le navigateur fait le reste).
		bool NkEditeurScriptOuvrirAsset(NkEditeurCadre &c, const char *cheminNav);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURSCRIPTSUI_H__
