#pragma once
// =============================================================================
// NkCsvView.h — Aperçu CSV de NKCode : TABLE (grille) avec en-têtes.
//   Parse le CSV (champs entre guillemets, "" échappé, séparateur , ou ;),
//   affiche une grille : 1re ligne = en-têtes, colonnes auto-dimensionnées,
//   lignes alternées, numéros de ligne, défilement H + V (scrollbars standard).
//   Modèle mis en cache par onglet (reparse au changement de texte).
// @Author  Rihen
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkFormat.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::nkgui;

		struct NkCsvRow {
				NkVector<NkString> cells;
		};
		struct NkCsvTable {
				NkVector<NkCsvRow> rows;
				int32 cols = 0;
				usize srcLen = 0;
				uint32 srcHash = 0;
				bool valid = false;
		};

		// Détecte le séparateur (',' ou ';' ou tab) sur la 1re ligne non vide.
		inline char NkCsvSep(const char *t, usize len) {
			int32 comma = 0, semi = 0, tab = 0;
			for (usize i = 0; i < len && t[i] != '\n'; ++i) {
				if (t[i] == ',')
					++comma;
				else if (t[i] == ';')
					++semi;
				else if (t[i] == '\t')
					++tab;
			}
			if (semi > comma && semi >= tab)
				return ';';
			if (tab > comma && tab > semi)
				return '\t';
			return ',';
		}

		inline void NkCsvParse(const char *t, usize len, NkCsvTable &tab) {
			tab.rows.Clear();
			tab.cols = 0;
			const char sep = NkCsvSep(t, len);
			usize i = 0;
			NkCsvRow cur;
			NkString field;
			bool inQ = false;
			bool any = false;
			auto endField = [&]() {
				cur.cells.PushBack(field);
				field = NkString();
			};
			auto endRow = [&]() {
				endField();
				if (cur.cells.Size() > (usize)tab.cols)
					tab.cols = (int32)cur.cells.Size();
				tab.rows.PushBack(cur);
				cur = NkCsvRow();
			};
			while (i < len) {
				const char c = t[i];
				if (inQ) {
					if (c == '"') {
						if (i + 1 < len && t[i + 1] == '"') {
							field += '"';
							i += 2;
							continue;
						}
						inQ = false;
						++i;
						continue;
					}
					field += c;
					++i;
					continue;
				}
				if (c == '"') {
					inQ = true;
					++i;
					continue;
				}
				if (c == sep) {
					endField();
					++i;
					any = true;
					continue;
				}
				if (c == '\r') {
					++i;
					continue;
				}
				if (c == '\n') {
					endRow();
					++i;
					any = true;
					continue;
				}
				field += c;
				++i;
				any = true;
			}
			// Dernière ligne (si le fichier ne finit pas par \n).
			if (field.Size() > 0 || cur.cells.Size() > 0)
				endRow();
			tab.valid = any && !tab.rows.Empty() && tab.cols > 0;
		}

		inline uint32 NkCsvHash(const char *s, usize n) {
			uint32 h = 2166136261u;
			for (usize i = 0; i < n; ++i)
				h = (h ^ (uint8)s[i]) * 16777619u;
			return h;
		}
		inline NkVector<NkCsvTable *> &NkCsvTables() {
			static NkVector<NkCsvTable *> v;
			return v;
		}
		inline NkCsvTable *NkCsvTableFor(const void *keyPtr, const char *text) {
			static NkVector<const void *> keys;
			auto &tabs = NkCsvTables();
			NkCsvTable *d = nullptr;
			for (usize i = 0; i < keys.Size(); ++i)
				if (keys[i] == keyPtr) {
					d = tabs[i];
					break;
				}
			if (!d) {
				d = new NkCsvTable();
				keys.PushBack(keyPtr);
				tabs.PushBack(d);
			}
			usize len = 0;
			while (text[len])
				++len;
			const uint32 h = NkCsvHash(text, len);
			if (d->srcLen != len || d->srcHash != h) {
				NkCsvParse(text, len, *d);
				d->srcLen = len;
				d->srcHash = h;
			}
			return d;
		}

		// ── Rendu de la table dans `area` ─────────────────────────────────────────
		inline void NkDrawCsv(NkGuiContext &ctx, const void *key, const char *text, const NkRect &area,
							  float32 &scrollY, float32 &scrollX) {
			NkGuiDrawList &dl = ctx.DL();
			const NkGuiFont *font = ctx.font;
			const float32 S = ctx.S(1.f);
			dl.PushClipRect(area, true);
			dl.AddRectFilled(area, NkColor{22, 24, 28, 255});
			if (!font || !font->Valid()) {
				dl.PopClipRect();
				return;
			}
			const float32 lh = font->LineHeight(), asc = font->Ascent();
			NkCsvTable *d = NkCsvTableFor(key, text);
			if (!d->valid) {
				dl.AddText(font->Face(), font->TexId(), {area.x + 16.f * S, area.y + 16.f * S + asc}, "CSV vide",
						   NkColor{200, 160, 90, 255});
				dl.PopClipRect();
				return;
			}
			auto &in = ctx.input;
			const NkVec2 mp = in.mousePos;
			const float32 rowH = lh + 8.f * S, pad = 8.f * S, sbW = 14.f * S;
			const float32 gutterW = 46.f * S; // colonne des numéros de ligne
			const float32 minCol = 54.f * S, maxCol = 340.f * S;
			const int32 nRows = (int32)d->rows.Size();
			const int32 nCols = d->cols;

			// Largeurs de colonnes (max du contenu, borné) — échantillonne jusqu'à 400 lignes.
			static NkVector<float32> cw;
			cw.Clear();
			for (int32 c = 0; c < nCols; ++c)
				cw.PushBack(minCol);
			const int32 sample = nRows < 400 ? nRows : 400;
			for (int32 rr = 0; rr < sample; ++rr) {
				const NkCsvRow &row = d->rows[rr];
				for (int32 c = 0; c < (int32)row.cells.Size(); ++c) {
					const float32 w = font->MeasureWidth(row.cells[c].CStr()) + 16.f * S;
					if (w > cw[c])
						cw[c] = w > maxCol ? maxCol : w;
				}
			}
			float32 totalW = gutterW;
			for (int32 c = 0; c < nCols; ++c)
				totalW += cw[c];
			const float32 headH = rowH;
			const float32 bodyTop = area.y + headH;
			const float32 contentH = (float32)(nRows - 1) * rowH; // sans l'en-tête (ligne 0)
			const float32 viewH = area.h - headH;

			const bool needV = contentH > viewH + 0.5f;
			const float32 availW = area.w - (needV ? sbW : 0.f);
			const bool needH = totalW > availW + 0.5f;
			const float32 viewW = availW;
			const float32 bodyH = viewH - (needH ? sbW : 0.f);

			const float32 maxY = contentH - bodyH > 0.f ? contentH - bodyH : 0.f;
			const float32 maxX = totalW - viewW > 0.f ? totalW - viewW : 0.f;
			scrollY = scrollY < 0.f ? 0.f : (scrollY > maxY ? maxY : scrollY);
			scrollX = scrollX < 0.f ? 0.f : (scrollX > maxX ? maxX : scrollX);
			if (NkGuiRectContains(area, mp)) {
				if (in.wheel != 0.f) {
					scrollY -= in.wheel * rowH * 3.f;
					in.wheel = 0.f;
					scrollY = scrollY < 0.f ? 0.f : (scrollY > maxY ? maxY : scrollY);
				}
				if (in.wheelH != 0.f) {
					scrollX -= in.wheelH * 40.f * S;
					in.wheelH = 0.f;
					scrollX = scrollX < 0.f ? 0.f : (scrollX > maxX ? maxX : scrollX);
				}
			}

			const NkColor grid{44, 49, 58, 255}, headBg{30, 34, 42, 255}, headFg{180, 200, 230, 255},
				cellFg{206, 214, 222, 255}, gutterFg{110, 118, 129, 255}, altBg{26, 29, 35, 255};

			// Corps (clippé sous l'en-tête et la gouttière fixe).
			const NkRect bodyClip = {area.x + gutterW, bodyTop, viewW - gutterW, bodyH};
			dl.PushClipRect(bodyClip, true);
			for (int32 rr = 1; rr < nRows; ++rr) {
				const float32 y = bodyTop + (float32)(rr - 1) * rowH - scrollY;
				if (y + rowH <= bodyTop || y >= bodyTop + bodyH)
					continue;
				if ((rr & 1) == 0)
					dl.AddRectFilled({area.x + gutterW, y, viewW - gutterW, rowH}, altBg);
				const NkCsvRow &row = d->rows[rr];
				float32 x = area.x + gutterW - scrollX;
				for (int32 c = 0; c < nCols; ++c) {
					if (c < (int32)row.cells.Size() && !row.cells[c].Empty())
						dl.AddText(font->Face(), font->TexId(), {x + pad, y + (rowH - lh) * 0.5f + asc},
								   row.cells[c].CStr(), cellFg);
					x += cw[c];
					dl.AddRectFilled({x - 1.f, y, 1.f, rowH}, grid); // séparateur colonne
				}
			}
			dl.PopClipRect();

			// Gouttière (numéros de ligne) — fixe horizontalement, défile en Y.
			dl.PushClipRect({area.x, bodyTop, gutterW, bodyH}, true);
			dl.AddRectFilled({area.x, bodyTop, gutterW, bodyH}, NkColor{18, 20, 25, 255});
			for (int32 rr = 1; rr < nRows; ++rr) {
				const float32 y = bodyTop + (float32)(rr - 1) * rowH - scrollY;
				if (y + rowH <= bodyTop || y >= bodyTop + bodyH)
					continue;
				const NkString num = NkPrintf("%d", rr);
				dl.AddText(font->Face(), font->TexId(),
						   {area.x + gutterW - font->MeasureWidth(num.CStr()) - 8.f * S, y + (rowH - lh) * 0.5f + asc},
						   num.CStr(), gutterFg);
			}
			dl.PopClipRect();

			// En-tête (ligne 0) — fixe verticalement, défile en X.
			dl.AddRectFilled({area.x, area.y, viewW, headH}, headBg);
			dl.PushClipRect({area.x + gutterW, area.y, viewW - gutterW, headH}, true);
			{
				const NkCsvRow &hd = d->rows[0];
				float32 x = area.x + gutterW - scrollX;
				for (int32 c = 0; c < nCols; ++c) {
					if (c < (int32)hd.cells.Size() && !hd.cells[c].Empty())
						dl.AddText(font->Face(), font->TexId(), {x + pad, area.y + (headH - lh) * 0.5f + asc},
								   hd.cells[c].CStr(), headFg);
					x += cw[c];
					dl.AddRectFilled({x - 1.f, area.y, 1.f, headH}, grid);
				}
			}
			dl.PopClipRect();
			dl.AddRectFilled({area.x, bodyTop - 1.f, viewW, 1.f}, NkColor{70, 78, 90, 255}); // ligne sous l'en-tête

			// Scrollbars standard.
			if (needV) {
				const NkRect vtrack = {area.x + area.w - sbW, bodyTop, sbW, area.h - headH - (needH ? sbW : 0.f)};
				editorkit::NkVScrollbar(ctx, dl, vtrack, scrollY, contentH, bodyH, 0xC50F0001u, rowH);
			}
			if (needH) {
				const NkRect htrack = {area.x, area.y + area.h - sbW, availW, sbW};
				editorkit::NkHScrollbar(ctx, dl, htrack, scrollX, totalW, viewW, 0xC50F0002u);
			}
			dl.PopClipRect();
		}

	} // namespace nkcode
} // namespace nkentseu
