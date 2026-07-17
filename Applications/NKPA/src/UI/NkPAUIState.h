#pragma once
/**
 * @File    NkPAUIState.h
 * @Brief   État partagé entre l'application NKPA et son interface (NKUI ou NKGui).
 *
 *  Ne dépend d'AUCUN header d'UI → utilisable des deux côtés sans couplage.
 */

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nkpa {

		// 12 espèces (ordre = ordre d'affichage dans l'UI).
		enum class Species : int32 {
			Fish = 0,
			Shark,
			Eel,
			Jellyfish,
			Snake,
			Caterpillar,
			Centipede,
			Worm,
			Lizard,
			Turtle,
			Cat,
			Bird,
			Count
		};

		static constexpr int32 kSpeciesCount = (int32)Species::Count;

		// Noms lisibles (UI).
		inline const char *SpeciesName(int32 i) {
			static const char *kNames[kSpeciesCount] = {"Poissons",	 "Requins",	   "Anguilles", "Meduses",
														"Serpents",	 "Chenilles",  "Mille-pattes", "Vers",
														"Lezards",	 "Tortues",	   "Chats",		"Oiseaux"};
			return (i >= 0 && i < kSpeciesCount) ? kNames[i] : "?";
		}

		struct NkPAUIState {
				// ── Simulation ────────────────────────────────────────────────────────────
				bool paused = false;
				bool showUI = true;
				float32 speedScale = 1.f; ///< Multiplicateur global de vitesse [0.1 .. 3.0]

				// ── Espèces : actif/inactif par espèce (défaut : toutes actives) ──────────
				bool speciesEnabled[kSpeciesCount] = {true, true, true, true, true, true,
													  true, true, true, true, true, true};

				// ── Trainées (trails) ─────────────────────────────────────────────────────
				bool trailsEnabled = false;	   ///< active le rendu des trainées
				bool trailAll = true;		   ///< true = toutes les espèces ; false = seulement `trailSpecies`
				int32 trailSpecies = 0;		   ///< espèce ciblée si !trailAll (index 0..11)

				// ── Backend (affichage + demande de changement → redémarrage) ─────────────
				int32 requestBackendIndex = -1; ///< -1 = aucune ; 0..4 = gl/vk/dx11/dx12/sw (rempli par l'UI)
				bool requestRestart = false;	///< l'UI a demandé « Appliquer & redémarrer »

				// ── Éditeur (hiérarchie / inspecteur / toolbar) ───────────────────────────
				int32 selectedSpecies = -1;		///< espèce sélectionnée dans la hiérarchie (-1 = aucune)
				bool recording = false;			///< bouton Record de la toolbar (capture — futur)
				bool showHierarchy = true;		///< panneau hiérarchie visible
				bool showInspector = true;		///< panneau inspecteur visible
		};

	} // namespace nkpa
} // namespace nkentseu
