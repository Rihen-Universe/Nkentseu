#pragma once
// =============================================================================
// NkUWPGamepad.h - UWP gamepad backend
//
// ⚠️ CE N'EST PLUS UN BOUCHON (30/09). Jusque-la, Poll() laissait toutes les
//    manettes « debranchees » pour toujours. Le backend est desormais
//    Windows.Gaming.Input (NkWinRTGamepad.h), l'API des manettes d'UWP.
//
//    CE QUI A ETE VERIFIE : NkWinRTGamepad compile et s'execute sous Windows
//    de bureau (clang-mingw), ou la meme API repond ; son Init, son Poll et sa
//    table boutons/axes sont eprouves par NKWindow_Tests (test_winrt_gamepad).
//    CE QUI NE L'A PAS ETE : une construction UWP (chaine xbox-clang + SDK
//    Windows absents de ce poste) et un paquet UWP lance avec une manette.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_UWP)

#include "NKWindow/Platform/UWP/NkWinRTGamepad.h"

namespace nkentseu {

	class NkUWPGamepad final : public NkWinRTGamepad {
		public:
			const char *GetName() const noexcept override {
				return "UWPGamepad";
			}
	};

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_UWP
