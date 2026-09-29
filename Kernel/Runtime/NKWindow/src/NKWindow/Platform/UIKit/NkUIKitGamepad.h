#pragma once
// =============================================================================
// NkUIKitGamepad.h - UIKit gamepad backend
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_IOS)

#if defined(NKENTSEU_GAMECONTROLLER_REEL)
// ⚠️ NON COMPILE, NON VERIFIE (30/09) : le backend GameController reel, ETEINT
//    par defaut. Voir NkAppleGameController.h pour l'activer sur un Mac.
#include "NKWindow/Platform/Cocoa/NkAppleGameController.h"

namespace nkentseu {

	class NkUIKitGamepad final : public NkAppleGameController {};

} // namespace nkentseu

#else // le bouchon d'avant, inchange : aucune manette

#include "NKEvent/NkGamepadSystem.h"

namespace nkentseu {

	class NkUIKitGamepad final : public NkIGamepad {
		public:
			bool Init() override {
				for (auto &snapshot : mSnapshots) {
					snapshot.Clear();
				}
				return true;
			}

			void Shutdown() override {
				for (auto &snapshot : mSnapshots) {
					snapshot.Clear();
				}
			}

			void Poll() override {
				// Native GCController polling can be wired here for iOS/tvOS.
				for (auto &snapshot : mSnapshots) {
					snapshot.connected = false;
				}
			}

			uint32 GetConnectedCount() const override {
				uint32 count = 0;
				for (const auto &snapshot : mSnapshots) {
					if (snapshot.connected) {
						++count;
					}
				}
				return count;
			}

			const NkGamepadSnapshot &GetSnapshot(uint32 idx) const override {
				static NkGamepadSnapshot dummy{};
				return idx < NK_MAX_GAMEPADS ? mSnapshots[idx] : dummy;
			}

			void Rumble(uint32, float32, float32, float32, float32, uint32) override {
			}

			const char *GetName() const noexcept override {
				return "UIKitGamepad";
			}

		private:
			NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
	};

} // namespace nkentseu

#endif // NKENTSEU_GAMECONTROLLER_REEL

#endif // NKENTSEU_PLATFORM_IOS
