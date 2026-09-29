//
// NkWinRTGamepad.h
// =============================================================================
// Description :
//   Backend manette sur Windows.Gaming.Input (WinRT), l'API des manettes
//   d'UWP. C'est le backend REEL de NkUWPGamepad (qui etait un bouchon
//   rendant « aucune manette » pour toujours).
//
// Caracteristiques :
//   - Ecrit contre l'ABI C++ de WinRT (ABI::Windows::Gaming::Input), que
//     fournissent A L'IDENTIQUE le SDK Windows et les en-tetes MinGW. C'est ce
//     qui permet de le COMPILER ET DE L'EXECUTER ici, sous Windows de bureau
//     (clang-mingw), alors qu'aucune chaine UWP n'est installee : l'API
//     Windows.Gaming.Input fonctionne aussi dans une application de bureau.
//   - UWP : lien direct (windowsapp, deja au lien de NKWindow). Bureau :
//     combase.dll charge a la main, comme XInput -- rien a ajouter au lien.
//   - Chaque manette garde SON emplacement tant qu'elle reste branchee :
//     l'ordre de la liste de WinRT change quand une autre se debranche, et
//     suivre l'indice de la liste ferait changer un joueur de manette.
//   - Les axes suivent XInput (Y du stick vers le HAUT = +), comme
//     NkWin32Gamepad : les deux dorsaux Windows disent la meme chose.
//   - Vibration : WinRT n'a pas de duree ; elle est tenue ici et coupee au
//     premier Poll apres l'echeance.
//
// ⚠️ NkWinRTRemplir est PURE (une lecture WinRT -> un NkGamepadSnapshot) : le
//    banc l'eprouve sans manette branchee.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_WINDOW_NKWINRTGAMEPAD_H__
#define __NKENTSEU_WINDOW_NKWINRTGAMEPAD_H__

#include "NKPlatform/NkPlatformDetect.h"

#if defined(_WIN32) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NKEvent/NkGamepadSystem.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.gaming.input.h>

#include <cstdio>

namespace nkentseu {

	namespace nk_winrt_dyn {
		using FnRoInitialize = HRESULT(WINAPI *)(RO_INIT_TYPE);
		using FnRoGetActivationFactory = HRESULT(WINAPI *)(HSTRING, REFIID, void **);
		using FnWindowsCreateString = HRESULT(WINAPI *)(LPCWSTR, UINT32, HSTRING *);
		using FnWindowsDeleteString = HRESULT(WINAPI *)(HSTRING);

		struct NkWinRTFonctions {
				bool charge = false;
				FnRoInitialize roInitialize = nullptr;
				FnRoGetActivationFactory roGetActivationFactory = nullptr;
				FnWindowsCreateString windowsCreateString = nullptr;
				FnWindowsDeleteString windowsDeleteString = nullptr;
		};

		inline const NkWinRTFonctions &Fonctions() noexcept {
			static NkWinRTFonctions f;
			if (f.charge) {
				return f;
			}
			f.charge = true;
#if defined(NKENTSEU_PLATFORM_UWP)
			// UWP : LoadLibrary n'existe pas dans la partition d'API ; windowsapp
			// est au lien, les fonctions sont la.
			f.roInitialize = &::RoInitialize;
			f.roGetActivationFactory = &::RoGetActivationFactory;
			f.windowsCreateString = &::WindowsCreateString;
			f.windowsDeleteString = &::WindowsDeleteString;
#else
			HMODULE h = ::LoadLibraryW(L"combase.dll");
			if (h != nullptr) {
				f.roInitialize = reinterpret_cast<FnRoInitialize>(::GetProcAddress(h, "RoInitialize"));
				f.roGetActivationFactory =
					reinterpret_cast<FnRoGetActivationFactory>(::GetProcAddress(h, "RoGetActivationFactory"));
				f.windowsCreateString =
					reinterpret_cast<FnWindowsCreateString>(::GetProcAddress(h, "WindowsCreateString"));
				f.windowsDeleteString =
					reinterpret_cast<FnWindowsDeleteString>(::GetProcAddress(h, "WindowsDeleteString"));
			}
#endif
			return f;
		}
	} // namespace nk_winrt_dyn

	/// Une lecture Windows.Gaming.Input -> un NkGamepadSnapshot (boutons et
	/// axes seulement ; `connected` et `info` sont a l'appelant).
	inline void NkWinRTRemplir(const ABI::Windows::Gaming::Input::GamepadReading &r, NkGamepadSnapshot &s) noexcept {
		using B = NkGamepadButton;
		using A = NkGamepadAxis;
		const uint32 m = static_cast<uint32>(r.Buttons);
		auto bouton = [&](B b, uint32 masque) { s.buttons[static_cast<uint32>(b)] = (m & masque) != 0u; };
		bouton(B::NK_GP_SOUTH, 0x4u);	   // A
		bouton(B::NK_GP_EAST, 0x8u);	   // B
		bouton(B::NK_GP_WEST, 0x10u);	   // X
		bouton(B::NK_GP_NORTH, 0x20u);	   // Y
		bouton(B::NK_GP_START, 0x1u);	   // Menu
		bouton(B::NK_GP_BACK, 0x2u);	   // View
		bouton(B::NK_GP_DPAD_UP, 0x40u);
		bouton(B::NK_GP_DPAD_DOWN, 0x80u);
		bouton(B::NK_GP_DPAD_LEFT, 0x100u);
		bouton(B::NK_GP_DPAD_RIGHT, 0x200u);
		bouton(B::NK_GP_LB, 0x400u);
		bouton(B::NK_GP_RB, 0x800u);
		bouton(B::NK_GP_LSTICK, 0x1000u);
		bouton(B::NK_GP_RSTICK, 0x2000u);
		// Les palettes (manette Elite) : masques en clair, parce que l'enumeration
		// ne les declare qu'a partir d'un contrat d'API (0x30000) que tous les
		// en-tetes ne supposent pas.
		bouton(B::NK_GP_PADDLE_1, 0x4000u);
		bouton(B::NK_GP_PADDLE_2, 0x8000u);
		bouton(B::NK_GP_PADDLE_3, 0x10000u);
		bouton(B::NK_GP_PADDLE_4, 0x20000u);

		const float32 lt = static_cast<float32>(r.LeftTrigger);
		const float32 rt = static_cast<float32>(r.RightTrigger);
		s.axes[static_cast<uint32>(A::NK_GP_AXIS_LX)] = static_cast<float32>(r.LeftThumbstickX);
		s.axes[static_cast<uint32>(A::NK_GP_AXIS_LY)] = static_cast<float32>(r.LeftThumbstickY);
		s.axes[static_cast<uint32>(A::NK_GP_AXIS_RX)] = static_cast<float32>(r.RightThumbstickX);
		s.axes[static_cast<uint32>(A::NK_GP_AXIS_RY)] = static_cast<float32>(r.RightThumbstickY);
		s.axes[static_cast<uint32>(A::NK_GP_AXIS_LT)] = lt;
		s.axes[static_cast<uint32>(A::NK_GP_AXIS_RT)] = rt;
		s.buttons[static_cast<uint32>(B::NK_GP_LT_DIGITAL)] = lt > 0.5f;
		s.buttons[static_cast<uint32>(B::NK_GP_RT_DIGITAL)] = rt > 0.5f;
		// La croix en axes aussi, dans la convention de NkWin32Gamepad (haut = +).
		const bool droite = s.buttons[static_cast<uint32>(B::NK_GP_DPAD_RIGHT)];
		const bool gauche = s.buttons[static_cast<uint32>(B::NK_GP_DPAD_LEFT)];
		const bool haut = s.buttons[static_cast<uint32>(B::NK_GP_DPAD_UP)];
		const bool bas = s.buttons[static_cast<uint32>(B::NK_GP_DPAD_DOWN)];
		s.axes[static_cast<uint32>(A::NK_GP_AXIS_DPAD_X)] = droite ? 1.f : (gauche ? -1.f : 0.f);
		s.axes[static_cast<uint32>(A::NK_GP_AXIS_DPAD_Y)] = haut ? 1.f : (bas ? -1.f : 0.f);
	}

	class NkWinRTGamepad : public NkIGamepad {
		public:
			~NkWinRTGamepad() override {
				Shutdown();
			}

			bool Init() override {
				for (auto &s : mSnapshots) {
					s.Clear();
				}
				const nk_winrt_dyn::NkWinRTFonctions &f = nk_winrt_dyn::Fonctions();
				if (f.roGetActivationFactory == nullptr || f.windowsCreateString == nullptr) {
					return false;
				}
				// Le thread doit avoir le Windows Runtime. Deja fait (OLE en STA
				// par NkWESystem, ou l'application) : S_FALSE ou RPC_E_CHANGED_MODE,
				// et les deux conviennent.
				if (f.roInitialize != nullptr) {
					f.roInitialize(RO_INIT_MULTITHREADED);
				}
				HSTRING classe = nullptr;
				const wchar_t *nom = L"Windows.Gaming.Input.Gamepad";
				if (FAILED(f.windowsCreateString(nom, static_cast<UINT32>(::wcslen(nom)), &classe))) {
					return false;
				}
				// {8BBCE529-D49C-39E9-9560-E47DDE96B7C8} : IGamepadStatics, ecrit en
				// clair pour ne dependre ni de __uuidof ni d'une bibliotheque d'IID.
				static const IID kIGamepadStatics = {
					0x8bbce529, 0xd49c, 0x39e9, {0x95, 0x60, 0xe4, 0x7d, 0xde, 0x96, 0xb7, 0xc8}};
				const HRESULT hr =
					f.roGetActivationFactory(classe, kIGamepadStatics, reinterpret_cast<void **>(&mStatics));
				if (f.windowsDeleteString != nullptr) {
					f.windowsDeleteString(classe);
				}
				return SUCCEEDED(hr) && mStatics != nullptr;
			}

			void Shutdown() override {
				for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
					Liberer(i);
				}
				if (mStatics != nullptr) {
					mStatics->Release();
					mStatics = nullptr;
				}
			}

			void Poll() override {
				if (mStatics == nullptr) {
					return;
				}
				using namespace ABI::Windows::Gaming::Input;
				using ABI::Windows::Foundation::Collections::IVectorView;
				IVectorView<Gamepad *> *liste = nullptr;
				if (FAILED(mStatics->get_Gamepads(&liste)) || liste == nullptr) {
					return;
				}
				UINT32 n = 0;
				liste->get_Size(&n);
				bool vue[NK_MAX_GAMEPADS] = {};
				for (UINT32 k = 0; k < n; ++k) {
					IGamepad *gp = nullptr;
					if (FAILED(liste->GetAt(k, &gp)) || gp == nullptr) {
						continue;
					}
					const int32 slot = Emplacement(gp);
					if (slot < 0) {
						gp->Release(); // plus de 8 manettes : la neuvieme attend
						continue;
					}
					vue[slot] = true;
					GamepadReading lecture{};
					if (SUCCEEDED(mPads[slot]->GetCurrentReading(&lecture))) {
						NkGamepadSnapshot &s = mSnapshots[slot];
						const NkGamepadInfo info = s.info;
						s.Clear();
						s.info = info;
						s.connected = true;
						NkWinRTRemplir(lecture, s);
					}
					gp->Release();
				}
				liste->Release();
				for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
					if (!vue[i] && mPads[i] != nullptr) {
						Liberer(i);
					}
				}
				// La vibration qui a fait son temps.
				const uint64 maintenant = ::GetTickCount64();
				for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
					if (mFinVibration[i] != 0u && maintenant >= mFinVibration[i] && mPads[i] != nullptr) {
						GamepadVibration zero{};
						mPads[i]->put_Vibration(zero);
						mFinVibration[i] = 0u;
					}
				}
			}

			uint32 GetConnectedCount() const override {
				uint32 c = 0;
				for (const auto &s : mSnapshots) {
					c += s.connected ? 1u : 0u;
				}
				return c;
			}

			const NkGamepadSnapshot &GetSnapshot(uint32 idx) const override {
				static const NkGamepadSnapshot vide{};
				return idx < NK_MAX_GAMEPADS ? mSnapshots[idx] : vide;
			}

			void Rumble(uint32 idx, float32 motorLow, float32 motorHigh, float32 triggerLeft, float32 triggerRight,
						uint32 durationMs) override {
				if (idx >= NK_MAX_GAMEPADS || mPads[idx] == nullptr) {
					return;
				}
				ABI::Windows::Gaming::Input::GamepadVibration v{};
				v.LeftMotor = motorLow;
				v.RightMotor = motorHigh;
				v.LeftTrigger = triggerLeft;
				v.RightTrigger = triggerRight;
				mPads[idx]->put_Vibration(v);
				mFinVibration[idx] = durationMs > 0u ? ::GetTickCount64() + durationMs : 0u;
			}

			const char *GetName() const noexcept override {
				return "WindowsGamingInput";
			}

			/// Diagnostic : l'API a-t-elle repondu a Init ?
			bool Disponible() const noexcept {
				return mStatics != nullptr;
			}

		private:
			/// L'emplacement de cette manette : le sien si on la connait deja,
			/// sinon le premier libre (et on la garde, AddRef). -1 si plein.
			int32 Emplacement(ABI::Windows::Gaming::Input::IGamepad *gp) noexcept {
				IUnknown *id = Identite(gp);
				int32 libre = -1;
				for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
					if (mPads[i] != nullptr && mIdentites[i] == id) {
						if (id != nullptr) {
							id->Release();
						}
						return static_cast<int32>(i);
					}
					if (mPads[i] == nullptr && libre < 0) {
						libre = static_cast<int32>(i);
					}
				}
				if (libre < 0) {
					if (id != nullptr) {
						id->Release();
					}
					return -1;
				}
				gp->AddRef();
				mPads[libre] = gp;
				mIdentites[libre] = id; // garde la reference d'Identite
				NkGamepadSnapshot &s = mSnapshots[libre];
				s.Clear();
				s.info.index = static_cast<uint32>(libre);
				s.info.type = NkGamepadType::NK_GP_TYPE_XBOX;
				s.info.numButtons = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
				s.info.numAxes = static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);
				s.info.hasRumble = true;
				s.info.hasTriggerRumble = true;
				std::snprintf(s.info.id, sizeof(s.info.id), "WGI_%d", libre);
				std::snprintf(s.info.name, sizeof(s.info.name), "Windows.Gaming.Input Gamepad");
				return libre;
			}

			/// L'identite COM d'un objet : son IUnknown. Deux pointeurs d'interface
			/// sur le MEME objet peuvent differer ; leurs IUnknown, jamais.
			static IUnknown *Identite(IUnknown *p) noexcept {
				IUnknown *u = nullptr;
				if (p == nullptr || FAILED(p->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void **>(&u)))) {
					return nullptr;
				}
				return u;
			}

			void Liberer(uint32 i) noexcept {
				if (mPads[i] != nullptr) {
					mPads[i]->Release();
					mPads[i] = nullptr;
				}
				if (mIdentites[i] != nullptr) {
					mIdentites[i]->Release();
					mIdentites[i] = nullptr;
				}
				mFinVibration[i] = 0u;
				mSnapshots[i].Clear();
			}

			ABI::Windows::Gaming::Input::IGamepadStatics *mStatics = nullptr;
			ABI::Windows::Gaming::Input::IGamepad *mPads[NK_MAX_GAMEPADS] = {};
			IUnknown *mIdentites[NK_MAX_GAMEPADS] = {};
			uint64 mFinVibration[NK_MAX_GAMEPADS] = {};
			NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
	};

} // namespace nkentseu

#endif // _WIN32 && !XBOX

#endif // __NKENTSEU_WINDOW_NKWINRTGAMEPAD_H__
