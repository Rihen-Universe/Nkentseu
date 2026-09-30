//
// NkAppleGameController.mm
// =============================================================================
// Description :
//   Corps du backend GameController (macOS / iOS). Voir NkAppleGameController.h.
//
// ⚠️⚠️ NON COMPILE, NON VERIFIE (30/09/2026) : ecrit sans chaine Apple. Eteint
//    tant que NKENTSEU_GAMECONTROLLER_REEL n'est pas defini ; ce fichier n'est
//    alors qu'une unite de compilation vide. ⚠️⚠️
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NKWindow/Platform/Cocoa/NkAppleGameController.h"

#if (defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)) && defined(NKENTSEU_GAMECONTROLLER_REEL)

#import <GameController/GameController.h>

#include <cstdio>

namespace nkentseu {

	NkAppleGameController::~NkAppleGameController() {
		Shutdown();
	}

	bool NkAppleGameController::Init() {
		for (auto &s : mSnapshots) {
			s.Clear();
		}
		return true;
	}

	void NkAppleGameController::Shutdown() {
		for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
			Liberer(i);
		}
	}

	void NkAppleGameController::Liberer(uint32 i) noexcept {
		if (mManettes[i] != nullptr) {
			CFRelease(mManettes[i]);
			mManettes[i] = nullptr;
		}
		mSnapshots[i].Clear();
	}

	void NkAppleGameController::Poll() {
		@autoreleasepool {
			NSArray<GCController *> *liste = [GCController controllers];
			bool vu[NK_MAX_GAMEPADS] = {};
			for (GCController *c in liste) {
				GCExtendedGamepad *g = c.extendedGamepad;
				if (g == nil) {
					continue; // micro gamepad (telecommande Siri) : pas une manette de jeu
				}
				// L'IDENTITE de l'objet donne l'emplacement : l'ordre de la liste
				// change quand une autre manette se debranche.
				const void *id = (__bridge const void *)c;
				int32 slot = -1;
				int32 libre = -1;
				for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
					if (mManettes[i] == id) {
						slot = static_cast<int32>(i);
						break;
					}
					if (mManettes[i] == nullptr && libre < 0) {
						libre = static_cast<int32>(i);
					}
				}
				if (slot < 0) {
					if (libre < 0) {
						continue;
					}
					slot = libre;
					mManettes[slot] = CFBridgingRetain(c);
					NkGamepadSnapshot &n = mSnapshots[slot];
					n.Clear();
					n.info.index = static_cast<uint32>(slot);
					n.info.type = NkGamepadType::NK_GP_TYPE_GENERIC;
					n.info.numButtons = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
					n.info.numAxes = static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);
					const char *nom = c.vendorName != nil ? [c.vendorName UTF8String] : "GameController";
					std::snprintf(n.info.name, sizeof(n.info.name), "%s", nom != nullptr ? nom : "GameController");
					std::snprintf(n.info.id, sizeof(n.info.id), "GC_%d", slot);
				}
				vu[slot] = true;

				NkGamepadSnapshot &s = mSnapshots[slot];
				const NkGamepadInfo info = s.info;
				s.Clear();
				s.info = info;
				s.connected = true;
				auto bouton = [&](NkGamepadButton b, GCControllerButtonInput *entree) {
					if (entree != nil) {
						s.buttons[static_cast<uint32>(b)] = entree.isPressed ? true : false;
					}
				};
				bouton(NkGamepadButton::NK_GP_SOUTH, g.buttonA);
				bouton(NkGamepadButton::NK_GP_EAST, g.buttonB);
				bouton(NkGamepadButton::NK_GP_WEST, g.buttonX);
				bouton(NkGamepadButton::NK_GP_NORTH, g.buttonY);
				bouton(NkGamepadButton::NK_GP_LB, g.leftShoulder);
				bouton(NkGamepadButton::NK_GP_RB, g.rightShoulder);
				bouton(NkGamepadButton::NK_GP_DPAD_UP, g.dpad.up);
				bouton(NkGamepadButton::NK_GP_DPAD_DOWN, g.dpad.down);
				bouton(NkGamepadButton::NK_GP_DPAD_LEFT, g.dpad.left);
				bouton(NkGamepadButton::NK_GP_DPAD_RIGHT, g.dpad.right);
				if (@available(macOS 10.14.1, iOS 12.1, *)) {
					bouton(NkGamepadButton::NK_GP_LSTICK, g.leftThumbstickButton);
					bouton(NkGamepadButton::NK_GP_RSTICK, g.rightThumbstickButton);
				}
				if (@available(macOS 10.15, iOS 13.0, *)) {
					bouton(NkGamepadButton::NK_GP_START, g.buttonMenu);
					bouton(NkGamepadButton::NK_GP_BACK, g.buttonOptions);
				}
				if (@available(macOS 11.0, iOS 14.0, *)) {
					bouton(NkGamepadButton::NK_GP_GUIDE, g.buttonHome);
				}

				const float32 lt = g.leftTrigger.value;
				const float32 rt = g.rightTrigger.value;
				s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LX)] = g.leftThumbstick.xAxis.value;
				s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LY)] = g.leftThumbstick.yAxis.value;
				s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RX)] = g.rightThumbstick.xAxis.value;
				s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RY)] = g.rightThumbstick.yAxis.value;
				s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LT)] = lt;
				s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RT)] = rt;
				s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_LT_DIGITAL)] = lt > 0.5f;
				s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_RT_DIGITAL)] = rt > 0.5f;
				const bool droite = s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_RIGHT)];
				const bool gauche = s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_LEFT)];
				const bool haut = s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_UP)];
				const bool bas = s.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_DOWN)];
				s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_X)] = droite ? 1.f : (gauche ? -1.f : 0.f);
				s.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_Y)] = haut ? 1.f : (bas ? -1.f : 0.f);
			}
			for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
				if (!vu[i] && mManettes[i] != nullptr) {
					Liberer(i);
				}
			}
		}
	}

	uint32 NkAppleGameController::GetConnectedCount() const {
		uint32 n = 0;
		for (const auto &s : mSnapshots) {
			n += s.connected ? 1u : 0u;
		}
		return n;
	}

	const NkGamepadSnapshot &NkAppleGameController::GetSnapshot(uint32 idx) const {
		static const NkGamepadSnapshot vide{};
		return idx < NK_MAX_GAMEPADS ? mSnapshots[idx] : vide;
	}

	void NkAppleGameController::Rumble(uint32, float32, float32, float32, float32, uint32) {
	}

	const char *NkAppleGameController::GetName() const noexcept {
		return "GameController (sans vibration)";
	}

} // namespace nkentseu

#endif // (macOS || iOS) && NKENTSEU_GAMECONTROLLER_REEL
