#pragma once
// =============================================================================
// PV3DE/UI/PvGui.h — petits helpers NKGui partagés par les panneaux médicaux
// =============================================================================
// Portage NKUI -> NKGui (2026-08-18). NKGui n'a pas de `TextColored` en
// auto-layout ni de `Toggle` : on compose ici avec ses primitives publiques
// (NextItemRect + TextAt), sans rien dupliquer de la bibliothèque. Header-only,
// même patron que Nkoung/UI/NkoungDraw.h et Mou/UI/MouDraw.h.
// =============================================================================

#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace pv3de {
		namespace pvgui {

			// Texte coloré en auto-layout, centré verticalement dans sa cellule
			// (dans un BeginRow la cellule fait la hauteur de la rangée).
			inline void TextColored(nkgui::NkGuiContext &ctx, const nkgui::NkColor &col, const char *s) noexcept {
				const bool ok = ctx.font && ctx.font->Valid();
				const float32 lh = ok ? ctx.font->LineHeight() : 16.f;
				const float32 w = ok ? ctx.font->MeasureWidth(s) : 0.f;
				const nkgui::NkRect r = ctx.NextItemRect(w, lh);
				const float32 y = r.y + (r.h > lh ? (r.h - lh) * 0.5f : 0.f);
				nkgui::TextAt(ctx, {r.x, y}, s, col);
			}

			// Texte thème (même centrage vertical que TextColored).
			inline void Label(nkgui::NkGuiContext &ctx, const char *s) noexcept {
				TextColored(ctx, ctx.IsDisabled() ? ctx.theme.textDisabled : ctx.theme.text, s);
			}

			// Ancien NkUI::Toggle : même sémantique qu'une case à cocher.
			inline bool Toggle(nkgui::NkGuiContext &ctx, const char *label, bool &value) noexcept {
				return nkgui::Checkbox(ctx, label, value);
			}

		} // namespace pvgui
	} // namespace pv3de
} // namespace nkentseu
