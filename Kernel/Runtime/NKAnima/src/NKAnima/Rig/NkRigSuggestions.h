//
// NkRigSuggestions.h
// =============================================================================
// Description :
//   LES CONSEILS DU RIG : ce qu'un assistant (l'IA de l'editeur, ou un humain
//   qui relit) proposerait de corriger dans un document de rig -- PLACEMENT des
//   reperes, NOMMAGE des os, POIDS, et l'ETAPE suivante du rig automatique.
//   Chaque conseil porte son ACTION ; l'appliquer est UNE operation annulable du
//   document (NkRigApplySuggestion).
//
// Caracteristiques :
//   - Placement : paires .L / .R qui ne sont plus symetriques (le repere
//     deplace a la main fait reference), reperes INTERIEURS sortis du volume
//     (le centre des sommets voisins, s'il est dedans).
//   - Nommage : un squelette IMPORTE (CesiumMan : « Skeleton_arm_joint_L__2_ »)
//     reconnu par sa STRUCTURE (colonne, bras, jambes) et renomme selon le
//     gabarit humanoide (hips, spine, chest, neck, head, shoulder.L,
//     upperarm.L...) -- ce qui donne ensuite ses CONTROLES IK a un rig importe ;
//     un os hors du milieu sans cote dans son nom ; un os de cote sans miroir.
//   - Poids : sommes fausses, trop d'influences, sommets sans poids.
//   - Etapes : detecter les reperes, construire le rig, creer les controles.
//   - DETERMINISTE et sans modele : c'est la base que l'IA lit (son outil
//     « rig_suggestions ») et que l'interface montre telle quelle.
//
// Algorithmes implementes :
//   - Reconnaissance de la structure d'un squelette : la colonne est le chemin
//     de la racine a la feuille centrale la plus haute ; les bras partent de la
//     colonne vers les cotes, les jambes partent vers le bas.
//
// Auteur   : TEUGUIA TADJUIDJE Rodolf Séderis (« Rihen »)
// Copyright: (c) 2022-2026 TEUGUIA TADJUIDJE Rodolf Séderis — Rihen Universe.
//            Tous droits réservés. Logiciel propriétaire : voir LICENSE.
//            Copie, reproduction, modification, redistribution et usage par une
//            IA interdits sans autorisation écrite.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKANIMA_NKRIGSUGGESTIONS_H__
#define __NKENTSEU_NKANIMA_NKRIGSUGGESTIONS_H__

#include "NKAnima/Rig/NkRigDocument.h"

namespace nkentseu {
	namespace anim {

		/// La famille d'un conseil (ce qu'il corrige).
		enum class NkRigSuggestionKind : uint8 {
			NK_RigSuggestionKind_Placement = 0, ///< les reperes du rig automatique
			NK_RigSuggestionKind_Naming,		///< les noms des os
			NK_RigSuggestionKind_Weights,		///< les poids de peau
			NK_RigSuggestionKind_Step,			///< l'etape suivante (detecter, construire, controles)
			NK_RigSuggestionKind_Count
		};
		const char *NkRigSuggestionKindName(NkRigSuggestionKind k);

		/// Ce que fait un conseil quand on l'applique.
		enum class NkRigActionKind : uint8 {
			NK_RigActionKind_Move_Landmarks = 0, ///< `ids` vers `positions`
			NK_RigActionKind_Rename_Bones,		 ///< `bones` recoivent `names`
			NK_RigActionKind_Symmetrize_Bone,	 ///< le miroir de `bones[0]`
			NK_RigActionKind_Detect_Landmarks,	 ///< avec `rigTemplate`
			NK_RigActionKind_Build_Rig,			 ///< avec `rigTemplate`, poids automatiques
			NK_RigActionKind_Auto_Weights,		 ///< avec `method`
			NK_RigActionKind_Normalize_Weights,
			NK_RigActionKind_Limit_Weights,		 ///< 4 influences
			NK_RigActionKind_Generate_Controls,	 ///< les controles du gabarit `rigTemplate`
			NK_RigActionKind_Count
		};

		struct NkRigAction {
				NkRigActionKind kind = NkRigActionKind::NK_RigActionKind_Normalize_Weights;
				NkVector<NkString> ids;
				NkVector<math::NkVec3f> positions;
				NkVector<int32> bones;
				NkVector<NkString> names;
				NkRigTemplate rigTemplate = NkRigTemplate::NK_RigTemplate_Humanoide;
				NkAutoWeightMethod method = NkAutoWeightMethod::NK_AutoWeightMethod_Chaleur;
		};

		/// UN CONSEIL.
		struct NkRigSuggestion {
				NkRigSuggestionKind kind = NkRigSuggestionKind::NK_RigSuggestionKind_Step;
				/// La cle STABLE du conseil (« placement:coude », « nommage:gabarit ») :
				/// l'interface et l'IA s'y referent d'une image a l'autre.
				NkString key;
				NkString title;	 ///< une phrase : ce qui est propose
				NkString detail; ///< pourquoi, et ce qui changera (quelques lignes)
				/// 0..1 : la confiance (1 = sur ; un renommage par la structure, moins).
				float32 confidence = 1.f;
				NkRigAction action;
		};

		struct NkRigSuggestOptions {
				/// Le repere de l'OBJET (du repos a l'espace affiche) : la structure se lit
				/// Y en haut, et CesiumMan est rigge couche. Identite le plus souvent.
				math::NkMat4f objet = math::NkMat4f::Identity();
				/// Ecart de symetrie d'une paire de reperes, en part de la hauteur.
				float32 symmetryTolerance = 0.01f;
		};

		/// Les conseils du document, du plus utile au moins utile.
		void NkRigSuggest(const NkRigDocument &doc, const NkRigSuggestOptions &opt, NkVector<NkRigSuggestion> &out);

		/// APPLIQUE un conseil : UNE operation du document (un seul Ctrl+Z).
		/// `effet` (optionnel) : ce qui a change, en une phrase. Faux si rien n'a pu
		/// etre fait (et l'historique n'en garde rien).
		bool NkRigApplySuggestion(NkRigDocument &doc, const NkRigSuggestion &s, NkString *effet = nullptr);

		/// LA STRUCTURE d'un squelette humanoide, lue dans ses positions : pour chaque
		/// os, le nom du gabarit (« upperarm.L »), VIDE s'il n'a pas de role reconnu.
		/// Rend le nombre d'os nommes ; 0 si la structure n'est pas humanoide.
		uint32 NkRigGuessHumanoidNames(const NkArmature &a, const math::NkMat4f &objet, NkVector<NkString> &noms);

	} // namespace anim
} // namespace nkentseu

#endif // __NKENTSEU_NKANIMA_NKRIGSUGGESTIONS_H__
