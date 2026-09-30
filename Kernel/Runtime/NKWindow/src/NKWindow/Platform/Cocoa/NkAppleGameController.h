//
// NkAppleGameController.h
// =============================================================================
// Description :
//   Backend manette REEL pour macOS et iOS, sur le framework GameController
//   (GCController / GCExtendedGamepad). Declaration C++ seule : le corps est
//   en Objective-C++, dans NkAppleGameController.mm.
//
// ⚠️⚠️ NON COMPILE, NON VERIFIE (30/09/2026). ⚠️⚠️
//   Ecrit sous Windows, SANS chaine Apple : aucune ligne de ce backend n'a vu
//   un compilateur Objective-C, ni un Mac, ni un iPhone, ni une manette. Rien
//   ici n'est affirme au-dela de la documentation publique de GameController.
//
//   C'est pourquoi il est ETEINT PAR DEFAUT : sans la macro
//   NKENTSEU_GAMECONTROLLER_REEL, NkCocoaGamepad et NkUIKitGamepad restent les
//   bouchons d'avant (aucune manette), et la construction macOS / iOS existante
//   ne change pas d'un octet.
//
// POUR L'ACTIVER, SUR UN MAC (trois gestes, dans NKWindow.jenga) :
//   1. ajouter "src/NKWindow/Platform/Cocoa/NkAppleGameController.mm" aux
//      `files` des filtres system:macOS ET system:iOS ;
//   2. ajouter "GameController" aux `frameworks` de ces deux filtres ;
//   3. definir NKENTSEU_GAMECONTROLLER_REEL (defines) dans ces deux filtres.
//   Puis : construire, brancher une manette, lancer NKWindow_Tests ou un jeu,
//   et SEULEMENT ALORS retirer cet avertissement.
//
// Caracteristiques (telles qu'ecrites) :
//   - [GCController controllers] relu a chaque Poll : la liste suit les
//     branchements sans ecouter les notifications.
//   - Seuls les profils ETENDUS (GCExtendedGamepad) sont lus : la telecommande
//     Siri (micro gamepad) n'est pas une manette de jeu au sens de NKEvent.
//   - Emplacement STABLE par manette (identite de l'objet GCController).
//   - Axes : Y du stick vers le HAUT = + (convention GameController), comme
//     XInput et NkWin32Gamepad.
//   - Vibration : NON implementee (CoreHaptics, iOS 14 / macOS 11) -- Rumble
//     ne fait rien, et GetName le dit.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_WINDOW_NKAPPLEGAMECONTROLLER_H__
#define __NKENTSEU_WINDOW_NKAPPLEGAMECONTROLLER_H__

#include "NKPlatform/NkPlatformDetect.h"

#if (defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)) && defined(NKENTSEU_GAMECONTROLLER_REEL)

#include "NKEvent/NkGamepadSystem.h"

namespace nkentseu {

	class NkAppleGameController : public NkIGamepad {
		public:
			~NkAppleGameController() override;

			bool Init() override;
			void Shutdown() override;
			void Poll() override;
			uint32 GetConnectedCount() const override;
			const NkGamepadSnapshot &GetSnapshot(uint32 idx) const override;
			/// Sans effet : la vibration demande CoreHaptics (non ecrit).
			void Rumble(uint32 idx, float32 motorLow, float32 motorHigh, float32 triggerLeft, float32 triggerRight,
						uint32 durationMs) override;
			const char *GetName() const noexcept override;

		private:
			/// GCController retenus (CFBridgingRetain), un par emplacement.
			const void *mManettes[NK_MAX_GAMEPADS] = {};
			NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};

			void Liberer(uint32 i) noexcept;
	};

} // namespace nkentseu

#endif // (macOS || iOS) && NKENTSEU_GAMECONTROLLER_REEL

#endif // __NKENTSEU_WINDOW_NKAPPLEGAMECONTROLLER_H__
