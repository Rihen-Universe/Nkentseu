#pragma once
// =============================================================================
// NkMarkdown.h — Rendu Markdown pour le viewer NKCode (preview des .md).
//   Blocs : titres #..######, listes - * + et 1., citations >, code ``` et
//   `inline`, regle ---, paragraphes avec WORD-WRAP. Inline : **gras**, `code`,
//   [texte](url). Defilement vertical via le scrollbar STANDARD (NkVScrollbar).
//   Faux-gras (double passe) faute d'atlas bold. Rendu seul (pas d'edition).
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKEditorKit/NkEditorScrollbar.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// Style d'un fragment inline.
		enum NkMdStyle { NKMD_NORMAL = 0, NKMD_BOLD, NKMD_CODE, NKMD_LINK };

		struct NkMdRun {
				NkString text;
				int32 style = NKMD_NORMAL;
		};

		// Decoupe une ligne en fragments styles (**gras**, `code`, [lien](url)).
		inline void NkMdParseInline(const char *s, NkVector<NkMdRun> &out) {
			NkString cur;
			int32 st = NKMD_NORMAL;
			auto flush = [&]() {
				if (!cur.Empty()) {
					NkMdRun r;
					r.text = cur;
					r.style = st;
					out.PushBack(r);
					cur.Clear();
				}
			};
			for (int32 i = 0; s[i];) {
				const char c = s[i];
				if (c == '*' && s[i + 1] == '*') { // **gras**
					flush();
					st = (st == NKMD_BOLD) ? NKMD_NORMAL : NKMD_BOLD;
					i += 2;
				} else if (c == '`') { // `code`
					flush();
					st = (st == NKMD_CODE) ? NKMD_NORMAL : NKMD_CODE;
					i += 1;
				} else if (c == '[') { // [texte](url) -> garde le texte, style lien
					int32 j = i + 1;
					while (s[j] && s[j] != ']')
						++j;
					if (s[j] == ']' && s[j + 1] == '(') {
						int32 k = j + 2;
						while (s[k] && s[k] != ')')
							++k;
						if (s[k] == ')') {
							flush();
							NkMdRun r;
							for (int32 p = i + 1; p < j; ++p)
								r.text += s[p];
							r.style = NKMD_LINK;
							out.PushBack(r);
							i = k + 1;
							continue;
						}
					}
					cur += c;
					++i;
				} else {
					cur += c;
					++i;
				}
			}
			flush();
		}

		// Rendu Markdown de `text` dans `area`. `scroll` = defilement vertical (persistant).
		inline void NkDrawMarkdown(NkGuiContext &ctx, const char *text, const NkRect &area, float32 &scroll) {
			NkGuiDrawList &dl = ctx.DL();
			const NkGuiFont *f = ctx.font;
			if (!f || !f->Valid())
				return;
			const float32 lh = f->LineHeight(), asc = f->Ascent();
			const float32 S = ctx.S(1.f);
			const NkColor cText = ctx.theme.text, cMuted = ctx.theme.textDisabled, cAccent = ctx.theme.accent;
			const NkColor cCodeBg = {40, 44, 52, 255}, cCode = {220, 190, 140, 255}, cQuote = {120, 128, 138, 255};

			dl.AddRectFilled(area, ctx.theme.bgPrimary);
			const float32 sbW = editorkit::NkScrollbarWidth();
			const float32 padX = 28.f * S, padTop = 20.f * S;
			const float32 contentW = area.w - padX * 2.f - sbW;
			const float32 x0 = area.x + padX;
			float32 y = area.y + padTop - scroll;

			dl.PushClipRect({area.x, area.y, area.w - sbW, area.h}, true);

			auto drawRuns = [&](NkVector<NkMdRun> &runs, float32 startX, float32 &yy, const NkColor &base, bool bold0) {
				float32 x = startX;
				for (usize ri = 0; ri < runs.Size(); ++ri) {
					const bool bold = bold0 || runs[ri].style == NKMD_BOLD;
					const bool code = runs[ri].style == NKMD_CODE;
					const NkColor col = runs[ri].style == NKMD_LINK ? cAccent : (code ? cCode : base);
					// mots (word-wrap)
					NkString w;
					const char *ss = runs[ri].text.CStr();
					auto emit = [&](const char *word) {
						if (!*word)
							return;
						const float32 ww = f->MeasureWidth(word);
						if (x > startX && x + ww > startX + contentW) { // wrap
							x = startX;
							yy += lh;
						}
						if (code)
							dl.AddRectFilled({x - 2.f * S, yy - 1.f * S, ww + 4.f * S, lh + 2.f * S}, cCodeBg, 3.f * S);
						dl.AddText(f->Face(), f->TexId(), {x, yy + asc}, word, col);
						if (bold) // faux-gras : 2e passe decalee
							dl.AddText(f->Face(), f->TexId(), {x + 0.6f * S, yy + asc}, word, col);
						x += ww;
					};
					for (int32 i = 0;; ++i) {
						if (ss[i] == ' ' || ss[i] == '\0') {
							w += ' ';
							emit(w.CStr());
							w.Clear();
							if (!ss[i])
								break;
						} else
							w += ss[i];
					}
				}
				yy += lh;
			};

			// Parcours ligne a ligne.
			NkString line;
			bool inCode = false;
			auto handleLine = [&](const char *ln) {
				// bloc de code ``` ```
				if (ln[0] == '`' && ln[1] == '`' && ln[2] == '`') {
					inCode = !inCode;
					y += 4.f * S;
					return;
				}
				if (inCode) {
					dl.AddRectFilled({x0 - 6.f * S, y - 2.f * S, contentW + 12.f * S, lh + 4.f * S}, cCodeBg);
					dl.AddText(f->Face(), f->TexId(), {x0, y + asc}, ln, cCode);
					y += lh;
					return;
				}
				// saut de x initiaux (espaces)
				int32 sp = 0;
				while (ln[sp] == ' ')
					++sp;
				const char *t = ln + sp;
				// titres
				int32 hl = 0;
				while (t[hl] == '#')
					++hl;
				if (hl >= 1 && hl <= 6 && t[hl] == ' ') {
					y += (hl <= 2 ? 12.f : 8.f) * S;
					NkVector<NkMdRun> runs;
					NkMdParseInline(t + hl + 1, runs);
					drawRuns(runs, x0, y, hl <= 2 ? cText : cAccent, true); // titres = gras
					if (hl <= 2) {						  // trait sous h1/h2
						dl.AddRectFilled({x0, y - lh + lh - 2.f * S, contentW, 1.f}, ctx.theme.border);
						y += 6.f * S;
					}
					return;
				}
				// regle horizontale
				if ((t[0] == '-' && t[1] == '-' && t[2] == '-') || (t[0] == '*' && t[1] == '*' && t[2] == '*')) {
					y += 6.f * S;
					dl.AddRectFilled({x0, y, contentW, 1.f}, ctx.theme.border);
					y += lh;
					return;
				}
				// citation >
				if (t[0] == '>') {
					const char *q = t[1] == ' ' ? t + 2 : t + 1;
					const float32 y0 = y;
					NkVector<NkMdRun> runs;
					NkMdParseInline(q, runs);
					drawRuns(runs, x0 + 16.f * S, y, cQuote, false);
					dl.AddRectFilled({x0, y0 - 1.f * S, 3.f * S, (y - y0)}, cAccent);
					return;
				}
				// liste - * + ou 1.
				bool bullet = ((t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ');
				int32 num = 0;
				if (!bullet) {
					while (t[num] >= '0' && t[num] <= '9')
						++num;
					bullet = (num > 0 && t[num] == '.' && t[num + 1] == ' ');
				}
				if (bullet) {
					const float32 ind = x0 + (sp > 0 ? 18.f * S : 0.f);
					const char *item = num > 0 ? t + num + 2 : t + 2;
					dl.AddText(f->Face(), f->TexId(), {ind, y + asc}, num > 0 ? "•" : "•", cAccent);
					NkVector<NkMdRun> runs;
					NkMdParseInline(item, runs);
					drawRuns(runs, ind + 16.f * S, y, cText, false);
					return;
				}
				// ligne vide
				if (t[0] == '\0') {
					y += lh * 0.5f;
					return;
				}
				// paragraphe
				NkVector<NkMdRun> runs;
				NkMdParseInline(t, runs);
				drawRuns(runs, x0, y, cText, false);
			};

			for (const char *p = text;; ++p) {
				if (*p == '\n' || *p == '\0') {
					handleLine(line.CStr());
					line.Clear();
					if (*p == '\0')
						break;
				} else if (*p != '\r')
					line += *p;
			}
			dl.PopClipRect();

			// Defilement (scrollbar standard).
			const float32 contentH = (y + scroll) - (area.y + padTop) + 40.f * S;
			const float32 maxS = contentH > area.h ? contentH - area.h : 0.f;
			if (scroll < 0.f)
				scroll = 0.f;
			if (scroll > maxS)
				scroll = maxS;
			if (NkGuiRectContains(area, ctx.input.mousePos) && ctx.input.wheel != 0.f) {
				scroll -= ctx.input.wheel * lh * 3.f;
				if (scroll < 0.f)
					scroll = 0.f;
				if (scroll > maxS)
					scroll = maxS;
				ctx.input.wheel = 0.f;
			}
			if (maxS > 0.5f) {
				const NkRect track = {area.x + area.w - sbW, area.y, sbW, area.h};
				editorkit::NkVScrollbar(ctx, dl, track, scroll, contentH, area.h, 0x33DD0001u, lh);
			}
		}

	} // namespace nkcode
} // namespace nkentseu
