//
// NkXrInput.h
// =============================================================================
// Description :
//   Les entrées XR par ACTIONS : l'application déclare des intentions
//   (« sélectionner », « attraper », « se déplacer »), jamais des touches ni
//   des boutons de manette. C'est le backend qui lie l'intention au matériel.
//
// Caractéristiques :
//   - Le lien action → matériel passe par un USAGE sémantique (enum) et non
//     par des chemins-chaînes façon OpenXR (« /user/hand/... ») : à l'étage 0
//     il n'y a rien à parser, le simulateur mappe l'enum sur souris/clavier,
//     et l'étage 2 traduira l'enum vers les chemins des profils d'interaction
//     — la traduction vivra dans LE backend OpenXR, pas dans les apps.
//   - États à la OpenXR (valeur + changé-depuis-la-dernière-sync + actif) :
//     « changé » est ce qui distingue un front d'un niveau, et c'est ce dont
//     un jeu a besoin pour un clic — le fournir évite à chaque app de
//     re-bricoler sa détection de front.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKXRINPUT_H__
#define __NKENTSEU_XR_NKXRINPUT_H__

#include "NKXR/NkXrTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace xr {

		// ── Type de la valeur portée par une action ──────────────────────────
		enum class NkXrActionType : uint8 {
			NK_XR_ACTION_BOOL = 0,
			NK_XR_ACTION_FLOAT = 1,
			NK_XR_ACTION_VEC2 = 2,
			NK_XR_ACTION_POSE = 3,
			NK_XR_ACTION_HAPTIC = 4, ///< Sortie : vibration de la manette.
		};

		// ── Usage sémantique : ce que l'action VEUT DIRE ─────────────────────
		// Convention de mains : les usages SANS suffixe désignent la main
		// DROITE (la dominante par défaut), ceux en _LEFT la gauche. MOVE et
		// MENU restent à gauche — le schéma VR classique : la droite agit, la
		// gauche déplace. Les deux mains sont servies à égalité : un pupitre
		// de train, un volant ou un levier se manipulent à DEUX mains.
		enum class NkXrActionUsage : uint8 {
			NK_XR_USAGE_SELECT = 0,    ///< Le « clic » principal (gâchette droite).
			NK_XR_USAGE_GRAB = 1,      ///< Attraper (grip droit).
			NK_XR_USAGE_MENU = 2,      ///< Bouton menu/système (gauche).
			NK_XR_USAGE_MOVE = 3,      ///< Locomotion 2D (stick gauche).
			NK_XR_USAGE_AIM_POSE = 4,  ///< Pose de visée de la main droite.
			NK_XR_USAGE_GRIP_POSE = 5, ///< Pose de la paume de la main droite.
			NK_XR_USAGE_HAPTIC = 6,    ///< Vibration de la manette droite.
			// ── Main GAUCHE (symétrique) ─────────────────────────────────
			NK_XR_USAGE_SELECT_LEFT = 7,
			NK_XR_USAGE_GRAB_LEFT = 8,
			NK_XR_USAGE_AIM_POSE_LEFT = 9,
			NK_XR_USAGE_GRIP_POSE_LEFT = 10,
			NK_XR_USAGE_HAPTIC_LEFT = 11,
			NK_XR_USAGE_MOVE_RIGHT = 12, ///< Stick droit (rotation/regard).
		};

		// Handle opaque ; 0 = invalide (comme partout dans le moteur).
		using NkXrActionHandle = uint32;
		inline constexpr NkXrActionHandle NK_XR_ACTION_INVALID = 0u;

		struct NkXrActionDesc {
			const char *name = "";     ///< Diagnostic/journal uniquement.
			NkXrActionType type = NkXrActionType::NK_XR_ACTION_BOOL;
			NkXrActionUsage usage = NkXrActionUsage::NK_XR_USAGE_SELECT;
		};

		// ── États rendus par la session après SyncActions ────────────────────
		struct NkXrActionStateBool {
			bool current = false;
			bool changed = false;      ///< A basculé depuis la sync précédente.
			bool active = false;       ///< Une source matérielle est liée.
			NkXrTime lastChangeTime = 0;
		};

		struct NkXrActionStateFloat {
			float32 current = 0.f;
			bool changed = false;
			bool active = false;
		};

		struct NkXrActionStateVec2 {
			NkVec2f current{ 0.f, 0.f };
			bool changed = false;
			bool active = false;
		};

		// ── Vraies mains (hand tracking, sans manettes) ──────────────────────
		// 26 articulations par main — la disposition standard d'OpenXR
		// (XR_EXT_hand_tracking) : 0=paume, 1=poignet, puis 5 doigts de la
		// base à l'ongle. Reprise telle quelle : le jour venu, un maillage de
		// main squelettique se skinne directement dessus.
		inline constexpr uint32 NK_XR_HAND_JOINT_COUNT = 26u;

		enum class NkXrHandSide : uint8 {
			NK_XR_HAND_LEFT = 0,
			NK_XR_HAND_RIGHT = 1,
		};

		struct NkXrHandJoint {
			NkVec3f position{ 0.f, 0.f, 0.f };
			NkQuatf orientation{};
			float32 radius = 0.f;   ///< Rayon de l'articulation (mètres).
			bool valid = false;
		};

		struct NkXrHand {
			bool active = false;    ///< La main est vue par le tracking.
			NkXrHandJoint joints[NK_XR_HAND_JOINT_COUNT]{};
		};

		// ── Jeu d'actions : le paquet que l'app déclare puis attache ─────────
		// Un seul jeu attachable à l'étage 0 : les jeux multiples d'OpenXR
		// (par contexte de gameplay) viendront quand un besoin réel les
		// justifiera — les empiler aujourd'hui serait de l'architecture morte.
		class NkXrActionSet {
			public:
				NkXrActionHandle CreateAction(const NkXrActionDesc &desc) {
					mActions.PushBack(desc);
					// Le handle est l'index + 1 : stable car un jeu d'actions
					// est déclaratif — on n'y retire jamais rien.
					return NkXrActionHandle(mActions.Size());
				}

				uint32 Count() const noexcept {
					return uint32(mActions.Size());
				}

				const NkXrActionDesc *Data() const noexcept {
					return mActions.Size() ? &mActions[0] : nullptr;
				}

				const NkXrActionDesc &Get(NkXrActionHandle handle) const {
					return mActions[handle - 1u];
				}

				bool IsValidHandle(NkXrActionHandle handle) const noexcept {
					return handle != NK_XR_ACTION_INVALID && handle <= mActions.Size();
				}

			private:
				NkVector<NkXrActionDesc> mActions;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKXRINPUT_H__
