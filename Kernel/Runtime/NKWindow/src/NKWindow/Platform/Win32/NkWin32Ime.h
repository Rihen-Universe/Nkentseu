//
// NkWin32Ime.h
// =============================================================================
// Description :
//   La composition IME sous Win32 (IMM32) : lire la chaine en cours et son
//   curseur, et dire a l'IME OU ouvrir sa fenetre de candidats.
//
// Caracteristiques :
//   - IMM32 est charge DYNAMIQUEMENT (LoadLibrary + GetProcAddress), comme
//     XInput dans NkWin32Gamepad.h : aucun `imm32` a ajouter au lien de
//     NKWindow, donc aucune application a relier. Sans la DLL, tout rend
//     « rien » et la saisie ordinaire (WM_CHAR) continue.
//   - L'etat par fenetre (zone de saisie, composition dessinee par
//     l'application) vit dans des PROPRIETES du HWND, pas dans NkWindowData :
//     meme raison que la brosse de fond (NkWin32Window.h), l'agencement de
//     NkWindowData doit rester stable entre unites de compilation.
//   - NkWin32ImeConvertir est PURE (UTF-16 -> UTF-8 et curseur UTF-16 -> code
//     points) : c'est elle que le banc eprouve, sans IME installe.
//
// ⚠️ PAR DEFAUT, RIEN NE CHANGE A L'ECRAN : l'IME garde SA fenetre de
//    composition, et le texte valide arrive toujours par WM_CHAR. Les
//    evenements NkTextCompositionEvent sont EN PLUS. Une application qui
//    dessine elle-meme la composition (NKGui) le demande par
//    NkWindow::SetImeInlineComposition(true) ; alors seulement la fenetre de
//    composition de l'IME est masquee.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_WINDOW_NKWIN32IME_H__
#define __NKENTSEU_WINDOW_NKWIN32IME_H__

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

#include "NKCore/NkTypes.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <imm.h> // TYPES et constantes seulement : les fonctions sont chargees a la main

namespace nkentseu {

	// -------------------------------------------------------------------------
	// Proprietes du HWND
	// -------------------------------------------------------------------------
	inline const wchar_t *NkWin32ImePropInline() noexcept {
		return L"NkImeInline";
	}
	inline const wchar_t *NkWin32ImePropXY() noexcept {
		return L"NkImeXY";
	}
	inline const wchar_t *NkWin32ImePropWH() noexcept {
		return L"NkImeWH";
	}

	/// Deux entiers de 16 bits dans une valeur de propriete. 16 bits suffisent
	/// a une zone client (+-32767 px) et tiennent dans un HANDLE 32 bits aussi.
	inline HANDLE NkWin32ImePack(int32 a, int32 b) noexcept {
		const uint32 ua = static_cast<uint32>(static_cast<uint16>(static_cast<int16>(a)));
		const uint32 ub = static_cast<uint32>(static_cast<uint16>(static_cast<int16>(b)));
		// +1 : une propriete de valeur nulle ne se distingue pas d'une absente.
		return reinterpret_cast<HANDLE>(static_cast<uintptr>((ua << 16) | ub) + 1u);
	}

	inline bool NkWin32ImeUnpack(HANDLE h, int32 &a, int32 &b) noexcept {
		if (h == nullptr) {
			return false;
		}
		const uint32 v = static_cast<uint32>(reinterpret_cast<uintptr>(h) - 1u);
		a = static_cast<int32>(static_cast<int16>(static_cast<uint16>(v >> 16)));
		b = static_cast<int32>(static_cast<int16>(static_cast<uint16>(v & 0xFFFFu)));
		return true;
	}

	inline bool NkWin32ImeEstInline(HWND hwnd) noexcept {
		return hwnd != nullptr && ::GetPropW(hwnd, NkWin32ImePropInline()) != nullptr;
	}

	/// A appeler avant la destruction du HWND : la documentation Win32 veut que
	/// l'application retire elle-meme les proprietes qu'elle a posees.
	inline void NkWin32ImeOublier(HWND hwnd) noexcept {
		if (hwnd == nullptr) {
			return;
		}
		::RemovePropW(hwnd, NkWin32ImePropInline());
		::RemovePropW(hwnd, NkWin32ImePropXY());
		::RemovePropW(hwnd, NkWin32ImePropWH());
	}

	// -------------------------------------------------------------------------
	// IMM32 dynamique
	// -------------------------------------------------------------------------
	namespace nk_imm_dyn {
		using FnGetContext = HIMC(WINAPI *)(HWND);
		using FnReleaseContext = BOOL(WINAPI *)(HWND, HIMC);
		using FnGetCompositionStringW = LONG(WINAPI *)(HIMC, DWORD, LPVOID, DWORD);
		using FnSetCompositionWindow = BOOL(WINAPI *)(HIMC, LPCOMPOSITIONFORM);
		using FnSetCandidateWindow = BOOL(WINAPI *)(HIMC, LPCANDIDATEFORM);

		struct NkImmFonctions {
				bool charge = false;
				FnGetContext getContext = nullptr;
				FnReleaseContext releaseContext = nullptr;
				FnGetCompositionStringW getCompositionStringW = nullptr;
				FnSetCompositionWindow setCompositionWindow = nullptr;
				FnSetCandidateWindow setCandidateWindow = nullptr;
		};

		inline const NkImmFonctions &Fonctions() noexcept {
			static NkImmFonctions f;
			if (!f.charge) {
				f.charge = true;
				HMODULE h = ::LoadLibraryW(L"imm32.dll");
				if (h != nullptr) {
					f.getContext = reinterpret_cast<FnGetContext>(::GetProcAddress(h, "ImmGetContext"));
					f.releaseContext = reinterpret_cast<FnReleaseContext>(::GetProcAddress(h, "ImmReleaseContext"));
					f.getCompositionStringW =
						reinterpret_cast<FnGetCompositionStringW>(::GetProcAddress(h, "ImmGetCompositionStringW"));
					f.setCompositionWindow =
						reinterpret_cast<FnSetCompositionWindow>(::GetProcAddress(h, "ImmSetCompositionWindow"));
					f.setCandidateWindow =
						reinterpret_cast<FnSetCandidateWindow>(::GetProcAddress(h, "ImmSetCandidateWindow"));
				}
			}
			return f;
		}

		inline bool Disponible() noexcept {
			const NkImmFonctions &f = Fonctions();
			return f.getContext != nullptr && f.releaseContext != nullptr;
		}
	} // namespace nk_imm_dyn

	// -------------------------------------------------------------------------
	// Conversion PURE : UTF-16 de l'IME -> UTF-8 de NKEvent
	// -------------------------------------------------------------------------

	/// @param w         la composition, UTF-16 (sans zero final obligatoire)
	/// @param n         nombre d'unites UTF-16
	/// @param curseur16 curseur de l'IME, en unites UTF-16
	/// @param sortie    tampon UTF-8, termine par un zero
	/// @param capacite  taille du tampon en octets
	/// @param curseurCp recoit le curseur en CODE POINTS
	/// @return nombre d'octets ecrits (hors zero final)
	/// ⚠️ LE CURSEUR DE L'IME COMPTE DES UNITES UTF-16 : un caractere hors du plan
	///    de base (emoji, idéogrammes rares) en vaut DEUX. Le rendre tel quel a
	///    NKGui, qui compte des code points, placerait le curseur trop loin.
	inline uint32 NkWin32ImeConvertir(const wchar_t *w, int32 n, int32 curseur16, char *sortie, uint32 capacite,
									  int32 &curseurCp) noexcept {
		curseurCp = 0;
		if (sortie == nullptr || capacite == 0) {
			return 0;
		}
		sortie[0] = '\0';
		if (w == nullptr || n <= 0) {
			return 0;
		}
		uint32 o = 0;
		int32 cp16 = 0;
		int32 cps = 0;
		for (int32 i = 0; i < n;) {
			uint32 c = static_cast<uint32>(w[i]);
			int32 unites = 1;
			if (c >= 0xD800u && c <= 0xDBFFu && i + 1 < n) {
				const uint32 bas = static_cast<uint32>(w[i + 1]);
				if (bas >= 0xDC00u && bas <= 0xDFFFu) {
					c = 0x10000u + ((c - 0xD800u) << 10) + (bas - 0xDC00u);
					unites = 2;
				}
			}
			char tmp[4];
			uint32 k = 0;
			if (c < 0x80u) {
				tmp[k++] = static_cast<char>(c);
			} else if (c < 0x800u) {
				tmp[k++] = static_cast<char>(0xC0u | (c >> 6));
				tmp[k++] = static_cast<char>(0x80u | (c & 0x3Fu));
			} else if (c < 0x10000u) {
				tmp[k++] = static_cast<char>(0xE0u | (c >> 12));
				tmp[k++] = static_cast<char>(0x80u | ((c >> 6) & 0x3Fu));
				tmp[k++] = static_cast<char>(0x80u | (c & 0x3Fu));
			} else {
				tmp[k++] = static_cast<char>(0xF0u | (c >> 18));
				tmp[k++] = static_cast<char>(0x80u | ((c >> 12) & 0x3Fu));
				tmp[k++] = static_cast<char>(0x80u | ((c >> 6) & 0x3Fu));
				tmp[k++] = static_cast<char>(0x80u | (c & 0x3Fu));
			}
			if (o + k >= capacite) {
				break; // tronque sur une frontiere de caractere
			}
			for (uint32 j = 0; j < k; ++j) {
				sortie[o++] = tmp[j];
			}
			if (cp16 < curseur16) {
				++cps;
			}
			cp16 += unites;
			i += unites;
		}
		sortie[o] = '\0';
		curseurCp = cps;
		return o;
	}

	/// Lit la composition courante de `hwnd`. Rend false si IMM32 est absent ou
	/// si la fenetre n'a pas de contexte ; `sortie` est alors vide.
	inline bool NkWin32ImeLireComposition(HWND hwnd, char *sortie, uint32 capacite, int32 &curseurCp) noexcept {
		curseurCp = 0;
		if (sortie != nullptr && capacite > 0) {
			sortie[0] = '\0';
		}
		const nk_imm_dyn::NkImmFonctions &f = nk_imm_dyn::Fonctions();
		if (!nk_imm_dyn::Disponible() || f.getCompositionStringW == nullptr) {
			return false;
		}
		HIMC himc = f.getContext(hwnd);
		if (himc == nullptr) {
			return false;
		}
		wchar_t w[256];
		const LONG octets = f.getCompositionStringW(himc, GCS_COMPSTR, w, sizeof(w));
		const LONG curseur = f.getCompositionStringW(himc, GCS_CURSORPOS, nullptr, 0);
		f.releaseContext(hwnd, himc);
		// Une valeur negative est un code d'erreur IMM (IMM_ERROR_NODATA...) :
		// pas une longueur. La traiter comme telle lirait hors du tampon.
		const int32 n = octets > 0 ? static_cast<int32>(octets / static_cast<LONG>(sizeof(wchar_t))) : 0;
		NkWin32ImeConvertir(w, n, curseur > 0 ? static_cast<int32>(curseur) : 0, sortie, capacite, curseurCp);
		return true;
	}

	/// Place la composition et les candidats de l'IME sur la zone de saisie
	/// posee par NkWindow::SetTextInputArea. Sans zone posee, ne fait rien :
	/// l'IME garde son placement a lui (pres du curseur systeme).
	inline void NkWin32ImeAppliquerZone(HWND hwnd) noexcept {
		int32 x = 0;
		int32 y = 0;
		int32 lw = 0;
		int32 lh = 0;
		if (!NkWin32ImeUnpack(::GetPropW(hwnd, NkWin32ImePropXY()), x, y) ||
			!NkWin32ImeUnpack(::GetPropW(hwnd, NkWin32ImePropWH()), lw, lh)) {
			return;
		}
		const nk_imm_dyn::NkImmFonctions &f = nk_imm_dyn::Fonctions();
		if (!nk_imm_dyn::Disponible()) {
			return;
		}
		HIMC himc = f.getContext(hwnd);
		if (himc == nullptr) {
			return;
		}
		if (f.setCompositionWindow != nullptr) {
			COMPOSITIONFORM cf{};
			cf.dwStyle = CFS_POINT;
			cf.ptCurrentPos.x = x;
			cf.ptCurrentPos.y = y;
			f.setCompositionWindow(himc, &cf);
		}
		if (f.setCandidateWindow != nullptr) {
			// CFS_EXCLUDE : la liste s'ouvre SOUS la zone et ne la recouvre
			// jamais, ce qui cacherait le texte qu'on est en train d'ecrire.
			CANDIDATEFORM cand{};
			cand.dwIndex = 0;
			cand.dwStyle = CFS_EXCLUDE;
			cand.ptCurrentPos.x = x;
			cand.ptCurrentPos.y = y + lh;
			cand.rcArea.left = x;
			cand.rcArea.top = y;
			cand.rcArea.right = x + lw;
			cand.rcArea.bottom = y + lh;
			f.setCandidateWindow(himc, &cand);
		}
		f.releaseContext(hwnd, himc);
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS && !NKENTSEU_PLATFORM_UWP && !NKENTSEU_PLATFORM_XBOX

#endif // __NKENTSEU_WINDOW_NKWIN32IME_H__
