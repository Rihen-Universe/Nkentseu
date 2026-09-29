// =============================================================================
// test_winrt_gamepad.cpp
//
// Le backend Windows.Gaming.Input (celui de NkUWPGamepad), COMPILE ET EXECUTE
// sous Windows de bureau : la meme API y repond. Aucune manette n'est exigee ;
// s'il y en a une de branchee, le banc dit combien il en voit.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (w1)  la table : A + haut de la croix + epaule droite, gachette gauche 0,7,
//         stick gauche (-0,5 ; 0,25) -> Sud, croix haut (et son axe a +1),
//         RB, LT numerique ; Est, RT numerique relaches ; les axes recopies
//   (w2)  Init repond sous Windows 10+ ; trois Poll ne cassent rien ; au plus
//         8 manettes ; un indice hors plage rend une manette debranchee ; une
//         vibration sur un emplacement vide ne casse rien
//   (w3)  derriere un vrai NkGamepadSystem : pret, et PollGamepads tourne
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKPlatform/NkPlatformDetect.h"

#if defined(_WIN32) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NKMemory/NkAllocator.h"
#include "NKWindow/Platform/UWP/NkWinRTGamepad.h"

#include <cstdio>

using namespace nkentseu;

TEST_CASE(NKWindowWinRTGamepad, W1_Table) {
	ABI::Windows::Gaming::Input::GamepadReading r{};
	r.Buttons = static_cast<ABI::Windows::Gaming::Input::GamepadButtons>(0x4 | 0x40 | 0x800);
	r.LeftTrigger = 0.7;
	r.RightTrigger = 0.2;
	r.LeftThumbstickX = -0.5;
	r.LeftThumbstickY = 0.25;
	NkGamepadSnapshot s;
	s.Clear();
	NkWinRTRemplir(r, s);
	ASSERT_TRUE(s.IsButtonDown(NkGamepadButton::NK_GP_SOUTH));
	ASSERT_TRUE(s.IsButtonDown(NkGamepadButton::NK_GP_DPAD_UP));
	ASSERT_TRUE(s.IsButtonDown(NkGamepadButton::NK_GP_RB));
	ASSERT_TRUE(s.IsButtonDown(NkGamepadButton::NK_GP_LT_DIGITAL));
	ASSERT_FALSE(s.IsButtonDown(NkGamepadButton::NK_GP_EAST));
	ASSERT_FALSE(s.IsButtonDown(NkGamepadButton::NK_GP_RT_DIGITAL));
	ASSERT_NEAR(-0.5f, s.GetAxis(NkGamepadAxis::NK_GP_AXIS_LX), 1e-6f);
	ASSERT_NEAR(0.25f, s.GetAxis(NkGamepadAxis::NK_GP_AXIS_LY), 1e-6f);
	ASSERT_NEAR(0.7f, s.GetAxis(NkGamepadAxis::NK_GP_AXIS_LT), 1e-6f);
	ASSERT_NEAR(1.f, s.GetAxis(NkGamepadAxis::NK_GP_AXIS_DPAD_Y), 1e-6f);
}

TEST_CASE(NKWindowWinRTGamepad, W2_ApiReelle) {
	NkWinRTGamepad g;
	const bool pret = g.Init();
	ASSERT_TRUE(pret);
	g.Poll();
	g.Poll();
	g.Poll();
	const uint32 n = g.GetConnectedCount();
	std::printf("  [info] Windows.Gaming.Input voit %u manette(s)\n", n);
	ASSERT_LESS_EQUAL(n, NK_MAX_GAMEPADS);
	ASSERT_FALSE(g.GetSnapshot(99).connected);
	g.Rumble(7, 0.5f, 0.5f, 0.f, 0.f, 10);
	g.Shutdown();
	ASSERT_FALSE(g.Disponible());
}

TEST_CASE(NKWindowWinRTGamepad, W3_DerriereNkGamepadSystem) {
	memory::NkAllocator &a = memory::NkGetDefaultAllocator();
	NkGamepadSystem sys;
	const bool ok = sys.Init(memory::NkUniquePtr<NkIGamepad>(a.New<NkWinRTGamepad>(),
															  memory::NkDefaultDelete<NkIGamepad>(&a)));
	ASSERT_TRUE(ok);
	ASSERT_TRUE(sys.IsReady());
	sys.PollGamepads();
	sys.PollGamepads();
	ASSERT_LESS_EQUAL(sys.GetConnectedCount(), NK_MAX_GAMEPADS);
	sys.Shutdown();
}

#endif // _WIN32
