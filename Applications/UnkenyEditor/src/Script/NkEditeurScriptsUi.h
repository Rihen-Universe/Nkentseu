//
// NkEditeurScriptsUi.h
// =============================================================================
// Description :
//   Les GESTES des scripts dans l'interface de l'editeur : le bloc « Scripts »
//   des Details (la liste ordonnee des scripts de l'entite, leurs variables, «
//   Ajouter un script »), les entrees « Script C++ » et « Blueprint » du « +
//   Ajouter » du navigateur, et le double-clic sur un .cpp ou un .nkbp.
//
// Caracteristiques :
//   - Le bloc des Details est SIMPLE (widgets NKGui) : il sera mis au style des
//     cartes par le chantier des Details apres fusion.
//   - Les actions vivent dans la plage 2200-2299 (NK_A_SCRIPT, NkEditeurInterface.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURSCRIPTSUI_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURSCRIPTSUI_H__

#include "NKCore/NkTypes.h"
#include "NKECS/NkECSDefines.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurCadre;

		/// Les actions des scripts : NK_A_SCRIPT + l'une d'elles.
		enum NkActionScript : int32 {
			NK_SCRIPT_NOUVEAU_CPP = 0, ///< « + Ajouter > Script C++ » : le fichier, ouvert dans l'editeur de texte
			NK_SCRIPT_NOUVEAU_BP,	   ///< « + Ajouter > Blueprint » : l'asset, ouvert dans la page du graphe
			NK_SCRIPT_COMPILER		   ///< recompiler les scripts C++ du projet maintenant
		};

		/// Le bloc « Scripts » des Details de l'entite `id`.
		void NkEditeurBlocScript(NkEditeurCadre &c, ecs::NkEntityId id);
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
