//
// NkUnkenyScript.h
// =============================================================================
// Description :
//   Le COMPOSANT de script (document 01, § 4.2, decision Q1 de Rihen : PLUSIEURS
//   scripts par entite, a la maniere d'Unity). Une liste ORDONNEE de scripts ;
//   chacun designe un Blueprint (« Contenu/Scripts/Porte.nkbp ») ou une classe
//   C++ (« cpp:Porte ») PAR SON NOM, et a ses propres variables, sauvees par NOM.
//
// Caracteristiques :
//   - DONNEES SEULEMENT, copiable bit a bit : la photo de Jouer / Arreter le
//     porte sans code nouveau, et « Arreter » rend les variables d'avant Jouer.
//   - Ce qui n'y est PAS : « deja demarre », « en faute », l'instance C++, les
//     registres de la VM. Ce sont des etats d'EXECUTION, tenus par l'hote
//     (NkHoteScripts2D) dans sa table, indexee par entite. Dans le composant, la
//     photo les recopierait : apres Arreter puis Jouer, « demarre » vaudrait
//     vrai et Debut ne passerait plus jamais.
//   - Les evenements sont livres a chaque script de la liste, DANS L'ORDRE de la
//     liste (celui que montrent et reordonnent les Details).
//   - Ecrit CHAMP PAR CHAMP dans .nkscene (NkScene::Init le declare) : les noms
//     de script et de variable en texte, les valeurs en nombres. Un moteur qui ne
//     connait pas ce composant l'ignore, comme tout composant decrit inconnu.
//
// ⚠️ LES VALEURS TIENNENT EN DEUX REELS (NkVec2f) : reel (x), entier (x, exact
//    jusqu'a 2^24), booleen (x != 0), vec2 (x, y). C'est le plus simple qui ne
//    ferme aucune porte : une valeur plus large (une entite, M3) prendra un
//    genre de plus sans changer la forme du fichier (« vars.type »).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYSCRIPT_H__
#define __NKENTSEU_UNKENY_NKUNKENYSCRIPT_H__

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "Unkeny/Scene/NkUnkenyChamps.h"

namespace nkentseu {
	namespace unkeny {

		/// Scripts par entite.
		static constexpr uint32 NK_UNKENY_SCRIPTS_MAX = 4u;
		/// Variables par entite, TOUS scripts confondus (chacune dit son script).
		static constexpr uint32 NK_UNKENY_SCRIPT_VARS_MAX = 16u;
		/// Longueur d'un nom de script, zero final compris.
		static constexpr uint32 NK_UNKENY_SCRIPT_NOM_MAX = 64u;
		/// Longueur d'un nom de variable, zero final compris.
		static constexpr uint32 NK_UNKENY_VAR_NOM_MAX = 20u;

		/// Le genre d'une variable (les valeurs NK_UNK_* de NkUnkenyScriptABI.h).
		enum class NkTypeVarScript : uint8 { NK_REEL = 0, NK_ENTIER, NK_BOOLEEN, NK_VEC2 };

		/// Une variable d'un script de l'entite.
		struct NkVarScript {
				char nom[NK_UNKENY_VAR_NOM_MAX] = {}; ///< vide = case libre
				uint8 script = 0;					  ///< l'emplacement du script qui la porte
				uint8 type = 0;						  ///< NkTypeVarScript
				math::NkVec2f valeur{0.f, 0.f};
		};

		/// Le composant.
		struct NkScript2D {
				/// Les scripts, dans l'ordre d'execution. « » = emplacement libre ;
				/// les emplacements utilises sont les `nombre` premiers.
				char refs[NK_UNKENY_SCRIPTS_MAX][NK_UNKENY_SCRIPT_NOM_MAX] = {};
				bool actifs[NK_UNKENY_SCRIPTS_MAX] = {true, true, true, true};
				uint8 nombre = 0;
				NkVarScript vars[NK_UNKENY_SCRIPT_VARS_MAX];
		};

		// --- Les gestes sur le composant (l'editeur, les bancs, le jeu) ---------

		/// Ajoute `ref` en FIN de liste. Rend l'emplacement, ou -1 (liste pleine,
		/// nom trop long ou vide).
		int32 NkScriptAjouter(NkScript2D &s, const char *ref) noexcept;
		/// Retire l'emplacement `i` (les suivants remontent, leurs variables
		/// suivent ; celles de `i` sont effacees).
		bool NkScriptRetirer(NkScript2D &s, uint32 i) noexcept;
		/// Echange les emplacements `i` et `j` (l'ORDRE d'execution).
		bool NkScriptEchanger(NkScript2D &s, uint32 i, uint32 j) noexcept;
		/// L'emplacement qui porte `ref`, ou -1.
		int32 NkScriptTrouver(const NkScript2D &s, const char *ref) noexcept;

		/// La variable `nom` du script `emplacement`, ou nul.
		NkVarScript *NkScriptVariable(NkScript2D &s, uint32 emplacement, const char *nom) noexcept;
		const NkVarScript *NkScriptVariable(const NkScript2D &s, uint32 emplacement, const char *nom) noexcept;
		/// Pose (ou cree) la variable. false : plus de place, ou nom trop long.
		bool NkScriptPoserVariable(NkScript2D &s, uint32 emplacement, const char *nom, NkTypeVarScript type,
								   const math::NkVec2f &valeur) noexcept;
		/// Le nombre de variables du script `emplacement`.
		uint32 NkScriptNbVariables(const NkScript2D &s, uint32 emplacement) noexcept;

		/// La description du composant pour la sauvegarde (NkScene::Init la pose).
		const NkChampSauve *NkChampsScript2D(uint32 &nombre) noexcept;

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYSCRIPT_H__
