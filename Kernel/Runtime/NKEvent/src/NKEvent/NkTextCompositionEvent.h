//
// NkTextCompositionEvent.h
// =============================================================================
// Description :
//   La composition d'un editeur de methode de saisie (IME) : le texte que
//   l'utilisateur est EN TRAIN d'ecrire en japonais, en chinois, en coreen...
//   avant de le valider. « にほん » souligne sous le curseur, pendant qu'une
//   liste de candidats propose « 日本 ».
//
// Caracteristiques :
//   - Trois phases : DEBUT, MISE A JOUR (chaque touche), FIN (validee ou
//     abandonnee). Le texte porte la composition ENTIERE a chaque mise a jour,
//     pas un delta : un champ qui l'affiche n'a rien a reconstituer.
//   - Le texte VALIDE n'est PAS dans cet evenement. Il arrive, comme avant,
//     par NkTextInputEvent, un code point a la fois. Un champ qui ignore la
//     composition continue donc de recevoir exactement ce qu'il recevait.
//   - Tampon fixe de 256 octets UTF-8 : une composition depasse rarement une
//     phrase, et un evenement sans allocation se clone et se met en file sans
//     cout. Au-dela, le texte est TRONQUE sur une frontiere de caractere.
//
// ⚠️ UN NOUVEAU TYPE, PAS UN DETOURNEMENT : NK_TEXT_INPUT et NK_CHAR_ENTERED
//    gardent leur sens. NK_TEXT_COMPOSITION est ajoute APRES NK_CUSTOM pour
//    qu'aucune valeur existante de NkEventType ne bouge.
//
// Emetteurs : Win32 (IMM32, NkWin32EventSystem.cpp). Les autres dorsaux n'en
//   emettent pas encore -- voir le rapport du 29/09 pour ce qu'il faudrait
//   (NSTextInputClient sur macOS, UITextInput sur iOS, compositionstart /
//   compositionupdate / compositionend sur le Web, InputConnection sur
//   Android, zwp_text_input_v3 sous Wayland, XIM sous X11).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_EVENT_NKTEXTCOMPOSITIONEVENT_H__
#define __NKENTSEU_EVENT_NKTEXTCOMPOSITIONEVENT_H__

#include "NKEvent/NkEvent.h"
#include "NKEvent/NkEventApi.h"

namespace nkentseu {

	enum class NkCompositionPhase : uint8 {
		NK_COMPOSITION_BEGIN = 0, ///< l'IME ouvre une composition (texte vide)
		NK_COMPOSITION_UPDATE,	  ///< le texte en cours a change
		NK_COMPOSITION_END		  ///< composition terminee (validee ou abandonnee)
	};

	inline const char *NkCompositionPhaseToString(NkCompositionPhase p) noexcept {
		switch (p) {
			case NkCompositionPhase::NK_COMPOSITION_BEGIN:  return "BEGIN";
			case NkCompositionPhase::NK_COMPOSITION_UPDATE: return "UPDATE";
			case NkCompositionPhase::NK_COMPOSITION_END:    return "END";
			default:                                        return "?";
		}
	}

	class NKENTSEU_EVENT_CLASS_EXPORT NkTextCompositionEvent final : public NkEvent {
		public:
			static constexpr uint32 TEXT_CAPACITY = 256;

			NK_EVENT_TYPE_FLAGS(NK_TEXT_COMPOSITION)

			NKENTSEU_EVENT_API uint32 GetCategoryFlags() const override {
				return static_cast<uint32>(NkEventCategory::NK_CAT_KEYBOARD) |
					   static_cast<uint32>(NkEventCategory::NK_CAT_INPUT);
			}

			/// @param phase   debut, mise a jour ou fin
			/// @param utf8    la composition entiere (nullptr = vide)
			/// @param cursor  position du curseur DANS la composition, en code
			///                points (0 = avant le premier caractere)
			/// @param windowId fenetre source
			NKENTSEU_EVENT_API NkTextCompositionEvent(NkCompositionPhase phase, const char *utf8, int32 cursor,
													  uint64 windowId = 0) noexcept
				: NkEvent(windowId), mPhase(phase), mCursor(cursor < 0 ? 0 : cursor) {
				Copier(utf8);
			}

			NKENTSEU_EVENT_API NkEvent *Clone() const override {
				return nkentseu::memory::NkGetDefaultAllocator().New<NkTextCompositionEvent>(*this);
			}

			NKENTSEU_EVENT_API NkString ToString() const override {
				return NkString("TextComposition(") + NkCompositionPhaseToString(mPhase) + " \"" + mText + "\")";
			}

			NKENTSEU_EVENT_API_INLINE NkCompositionPhase GetPhase() const noexcept {
				return mPhase;
			}

			/// La composition entiere, UTF-8, terminee par un zero.
			NKENTSEU_EVENT_API_INLINE const char *GetText() const noexcept {
				return mText;
			}

			/// Longueur en OCTETS (pas en caracteres).
			NKENTSEU_EVENT_API_INLINE uint32 GetLength() const noexcept {
				return mLength;
			}

			/// Le curseur, en code points depuis le debut de la composition.
			NKENTSEU_EVENT_API_INLINE int32 GetCursor() const noexcept {
				return mCursor;
			}

			NKENTSEU_EVENT_API_INLINE bool IsEmpty() const noexcept {
				return mLength == 0;
			}

		private:
			NkCompositionPhase mPhase = NkCompositionPhase::NK_COMPOSITION_UPDATE;
			int32 mCursor = 0;
			uint32 mLength = 0;
			char mText[TEXT_CAPACITY] = {};

			/// Copie en tronquant sur une frontiere de caractere : couper au
			/// milieu d'une sequence UTF-8 laisserait un octet de continuation
			/// orphelin, que la police dessinerait en losange.
			void Copier(const char *utf8) noexcept {
				mLength = 0;
				mText[0] = '\0';
				if (utf8 == nullptr) {
					return;
				}
				uint32 n = 0;
				while (utf8[n] != '\0' && n < TEXT_CAPACITY - 1) {
					++n;
				}
				if (utf8[n] != '\0') {
					while (n > 0 && (static_cast<uint8>(utf8[n]) & 0xC0u) == 0x80u) {
						--n;
					}
				}
				for (uint32 i = 0; i < n; ++i) {
					mText[i] = utf8[i];
				}
				mText[n] = '\0';
				mLength = n;
			}
	};

} // namespace nkentseu

#endif // __NKENTSEU_EVENT_NKTEXTCOMPOSITIONEVENT_H__
