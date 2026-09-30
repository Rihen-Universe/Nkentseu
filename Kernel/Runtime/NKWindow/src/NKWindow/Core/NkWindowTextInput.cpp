//
// NkWindowTextInput.cpp
// =============================================================================
// Description :
//   NkWindow::SetTextInputArea et SetImeInlineComposition : dire a l'IME ou
//   se trouve le champ, et qui dessine la composition.
//
// Caracteristiques :
//   - Win32 : IMM32, charge dynamiquement (NkWin32Ime.h). L'etat vit dans des
//     proprietes du HWND, relues par NkWin32EventSystem a chaque message IME.
//   - Ailleurs : un REFUS NOMME, une seule fois, et jamais un silence. Ce que
//     chaque dorsal devrait appeler est ecrit dans le refus lui-meme.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowAudit.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
#include "NKWindow/Platform/Win32/NkWin32Ime.h"
#endif

namespace nkentseu {

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)

	void NkWindow::SetTextInputArea(int32 x, int32 y, int32 width, int32 height) {
		HWND hwnd = mData.mHwnd;
		if (hwnd == nullptr) {
			return;
		}
		::SetPropW(hwnd, NkWin32ImePropXY(), NkWin32ImePack(x, y));
		::SetPropW(hwnd, NkWin32ImePropWH(), NkWin32ImePack(width < 0 ? 0 : width, height < 0 ? 0 : height));
		// Tout de suite, et pas seulement au prochain message IME : une liste de
		// candidats DEJA ouverte doit suivre le curseur qui vient de bouger.
		NkWin32ImeAppliquerZone(hwnd);
	}

	void NkWindow::SetImeInlineComposition(bool inlineComposition) {
		HWND hwnd = mData.mHwnd;
		if (hwnd == nullptr) {
			return;
		}
		// ⚠️ LA FENETRE DE COMPOSITION DE L'IME se decide a WM_IME_SETCONTEXT,
		//    c'est-a-dire a l'ACTIVATION de la fenetre. Le changement vaut donc
		//    pleinement a la prochaine activation ; d'ici la, seul le debut de
		//    composition (WM_IME_STARTCOMPOSITION) est deja retenu.
		if (inlineComposition) {
			::SetPropW(hwnd, NkWin32ImePropInline(), reinterpret_cast<HANDLE>(static_cast<uintptr>(1)));
		} else {
			::RemovePropW(hwnd, NkWin32ImePropInline());
		}
	}

	bool NkWindow::GetImeInlineComposition() const {
		return NkWin32ImeEstInline(mData.mHwnd);
	}

#else

	// ── CE QUI MANQUE, DORSAL PAR DORSAL (29/09) ─────────────────────────────
	// Rien de ceci n'est ecrit, et le refus le dit plutot que de faire croire
	// qu'une zone a ete posee :
	//   macOS   : NSTextInputClient (setMarkedText:selectedRange:, firstRectForCharacterRange:)
	//   iOS     : UITextInput (setMarkedText:selectedRange:, caretRectForPosition:)
	//   Web     : evenements compositionstart / compositionupdate / compositionend
	//             sur un <input> cache place sous le champ
	//   Android : InputConnection.setComposingText / finishComposingText
	//   Wayland : zwp_text_input_v3 (preedit_string, set_cursor_rectangle)
	//   X11     : XIM (XNPreeditAttributes, XNSpotLocation)
	void NkWindow::SetTextInputArea(int32 /*x*/, int32 /*y*/, int32 /*width*/, int32 /*height*/) {
		NkWindowRefuserMethode("SetTextInputArea", "ce dorsal",
							   "La zone de saisie de l'IME n'est implementee que sur Win32 (IMM32).");
	}

	void NkWindow::SetImeInlineComposition(bool /*inlineComposition*/) {
		NkWindowRefuserMethode("SetImeInlineComposition", "ce dorsal",
							   "La composition IME n'est remontee que sur Win32 (IMM32).");
	}

	bool NkWindow::GetImeInlineComposition() const {
		return false;
	}

#endif

} // namespace nkentseu
