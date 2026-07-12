#pragma once
// =============================================================================
// NkMarkdown.h — Rendu Markdown STYLISE pour la preview NKCode (.md).
//   Blocs : titres #..###### (poids/couleur/regle), code ``` (bloc styic) et
//   `inline`, listes - * + / 1., citations >, regle ---, TABLEAUX en grille,
//   paragraphes WORD-WRAP. Inline : **gras**, *italique*, `code`, [texte](url)
//   CLIQUABLE (NkLauncher::OpenURL). Texte via NkDrawTextU -> glyphes unicode /
//   box-drawing corrects. Defilement via scrollbar STANDARD (NkVScrollbar).
// =============================================================================
#include "NKGui/NKGui.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKCode/Editor/NkTextDraw.h"		 // NkDrawTextU (glyphes), NkDecodeU8
#include "NKWindow/Core/NkLauncher.h"		 // OpenURL (liens)

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		enum NkMdStyle { NKMD_NORMAL = 0, NKMD_BOLD, NKMD_ITALIC, NKMD_CODE, NKMD_LINK };

		struct NkMdRun {
				NkString text, url;
				int32 style = NKMD_NORMAL;
		};

		// Decoupe une ligne en fragments styles : **gras**, *italique*/_ital_, `code`, [txt](url).
		inline void NkMdInline(const char *s, NkVector<NkMdRun> &out) {
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
				if (c == '*' && s[i + 1] == '*') {
					flush();
					st = (st == NKMD_BOLD) ? NKMD_NORMAL : NKMD_BOLD;
					i += 2;
				} else if ((c == '*' || c == '_') && s[i + 1] && s[i + 1] != ' ' && st != NKMD_CODE) {
					flush();
					st = (st == NKMD_ITALIC) ? NKMD_NORMAL : NKMD_ITALIC;
					i += 1;
				} else if (c == '`') {
					flush();
					st = (st == NKMD_CODE) ? NKMD_NORMAL : NKMD_CODE;
					i += 1;
				} else if (c == '!' && s[i + 1] == '[') { // ![alt](src) -> "[image] alt"
					i += 1;								  // laisse le [ etre traite comme lien texte
				} else if (c == '[') {
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
							for (int32 p = j + 2; p < k; ++p)
								r.url += s[p];
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

		// Ligne = separateur de tableau ? (ex. |---|:--:|---|)
		inline bool NkMdIsTableSep(const char *s) {
			bool dash = false, ok = true;
			for (int32 i = 0; s[i]; ++i) {
				const char c = s[i];
				if (c == '-')
					dash = true;
				else if (c != '|' && c != ':' && c != ' ' && c != '\t') {
					ok = false;
					break;
				}
			}
			return ok && dash;
		}

		// Decoupe une ligne "| a | b |" en cellules (trim, sans les | de bord).
		inline void NkMdCells(const char *s, NkVector<NkString> &out) {
			NkString cell;
			int32 i = 0;
			if (s[i] == '|')
				++i; // saute le | initial
			for (;; ++i) {
				if (s[i] == '|' || s[i] == '\0') {
					// trim
					int32 a = 0, b = (int32)cell.Length();
					const char *cc = cell.CStr();
					while (a < b && (cc[a] == ' ' || cc[a] == '\t'))
						++a;
					while (b > a && (cc[b - 1] == ' ' || cc[b - 1] == '\t'))
						--b;
					NkString t;
					for (int32 k = a; k < b; ++k)
						t += cc[k];
					out.PushBack(t);
					cell.Clear();
					if (s[i] == '\0')
						break;
				} else
					cell += s[i];
			}
			// une ligne "| a | b |" produit une derniere cellule vide -> retire si vide
			if (!out.Empty() && out[out.Size() - 1].Empty())
				out.PopBack();
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
			const NkColor cHead = {235, 240, 248, 255}, cLink = {88, 166, 255, 255};
			const NkColor cCodeBg = {32, 36, 44, 255}, cCode = {224, 196, 148, 255}, cQuote = {130, 140, 152, 255};
			const NkColor cBorder = ctx.theme.border, cCellBg = {30, 34, 41, 255}, cHeadBg = {38, 43, 52, 255};

			dl.AddRectFilled(area, ctx.theme.bgPrimary);
			const float32 sbW = editorkit::NkScrollbarWidth();
			const float32 padX = 30.f * S, padTop = 22.f * S;
			const float32 x0 = area.x + padX;
			const float32 contentW = area.w - padX * 2.f - sbW;
			float32 y = area.y + padTop - scroll;
			const NkVec2 mp = ctx.input.mousePos;
			const bool click = ctx.input.mouseClicked[0];
			const bool overArea = NkGuiRectContains(area, mp);

			dl.PushClipRect({area.x, area.y, area.w - sbW, area.h}, true);

			// Dessine un mot (avec faux-gras + fond code + lien souligne/cliquable). Avance x.
			auto word = [&](float32 &x, float32 yy, const char *w, const NkMdRun &run, const NkColor &base) {
				const float32 ww = f->MeasureWidth(w);
				const bool code = run.style == NKMD_CODE;
				const bool link = run.style == NKMD_LINK;
				const NkColor col = link ? cLink : (code ? cCode : base);
				if (code)
					dl.AddRectFilled({x - 2.f * S, yy, ww + 4.f * S, lh}, cCodeBg, 3.f * S);
				NkDrawTextU(ctx, x, yy + asc, yy, lh, w, w + (int32)NkString(w).Length(), col);
				if (run.style == NKMD_BOLD || base.r == cHead.r) // faux-gras (titres/**)
					NkDrawTextU(ctx, x + 0.6f * S, yy + asc, yy, lh, w, w + (int32)NkString(w).Length(), col);
				if (link) {
					dl.AddRectFilled({x, yy + lh - 2.f * S, ww, 1.f}, cLink);
					const NkRect lr = {x, yy, ww, lh};
					if (overArea && click && NkGuiRectContains(lr, mp) && !run.url.Empty())
						NkLauncher::OpenURL(run.url.CStr());
				}
				x += ww;
			};
			// Rend des runs avec word-wrap dans [startX, startX+maxW]. Avance y.
			auto drawRuns = [&](NkVector<NkMdRun> &runs, float32 startX, float32 maxW, const NkColor &base) {
				float32 x = startX;
				for (usize ri = 0; ri < runs.Size(); ++ri) {
					const char *ss = runs[ri].text.CStr();
					NkString w;
					for (int32 i = 0;; ++i) {
						if (ss[i] == ' ' || ss[i] == '\0') {
							w += ' ';
							const float32 ww = f->MeasureWidth(w.CStr());
							if (x > startX && x + ww > startX + maxW) {
								x = startX;
								y += lh;
							}
							word(x, y, w.CStr(), runs[ri], base);
							w.Clear();
							if (!ss[i])
								break;
						} else
							w += ss[i];
					}
				}
				y += lh;
			};

			// Split en lignes.
			NkVector<NkString> L;
			{
				NkString ln;
				for (const char *p = text;; ++p) {
					if (*p == '\n' || *p == '\0') {
						L.PushBack(ln);
						ln.Clear();
						if (!*p)
							break;
					} else if (*p != '\r')
						ln += *p;
				}
			}

			for (usize li = 0; li < L.Size(); ++li) {
				const char *t = L[li].CStr();
				int32 sp = 0;
				while (t[sp] == ' ')
					++sp;
				const char *c = t + sp;

				// ── Bloc de code ``` ──
				if (c[0] == '`' && c[1] == '`' && c[2] == '`') {
					usize j = li + 1;
					while (j < L.Size()) {
						const char *e = L[j].CStr();
						int32 es = 0;
						while (e[es] == ' ')
							++es;
						if (e[es] == '`' && e[es + 1] == '`' && e[es + 2] == '`')
							break;
						++j;
					}
					const float32 top = y + 3.f * S;
					const float32 nRows = (float32)(j - li - 1);
					const float32 boxH = nRows * lh + 12.f * S;
					dl.AddRectFilled({x0, top, contentW, boxH}, cCodeBg, 5.f * S);
					dl.AddRect({x0, top, contentW, boxH}, cBorder, 1.f);
					float32 cy = top + 6.f * S;
					for (usize k = li + 1; k < j; ++k) {
						const char *cl = L[k].CStr();
						NkDrawTextU(ctx, x0 + 10.f * S, cy + asc, cy, lh, cl, cl + (int32)L[k].Length(), cCode);
						cy += lh;
					}
					y = top + boxH + 6.f * S;
					li = j;
					continue;
				}

				// ── Tableau ──
				if (c[0] == '|' && li + 1 < L.Size() && NkMdIsTableSep(L[li + 1].CStr())) {
					NkVector<NkVector<NkString>> rows;
					NkVector<NkString> hdr;
					NkMdCells(c, hdr);
					rows.PushBack(hdr);
					usize j = li + 2;
					while (j < L.Size()) {
						const char *r = L[j].CStr();
						int32 rs = 0;
						while (r[rs] == ' ')
							++rs;
						if (r[rs] != '|')
							break;
						NkVector<NkString> cells;
						NkMdCells(r + rs, cells);
						rows.PushBack(cells);
						++j;
					}
					int32 nCol = 0;
					for (usize ri = 0; ri < rows.Size(); ++ri)
						if ((int32)rows[ri].Size() > nCol)
							nCol = (int32)rows[ri].Size();
					if (nCol > 0) {
						NkVector<float32> cw;
						for (int32 ci = 0; ci < nCol; ++ci) {
							float32 w = 40.f * S;
							for (usize ri = 0; ri < rows.Size(); ++ri)
								if (ci < (int32)rows[ri].Size()) {
									const float32 tw = f->MeasureWidth(rows[ri][ci].CStr()) + 24.f * S;
									if (tw > w)
										w = tw;
								}
							cw.PushBack(w);
						}
						float32 tot = 0.f;
						for (int32 ci = 0; ci < nCol; ++ci)
							tot += cw[ci];
						if (tot > contentW) { // rentre dans la largeur
							const float32 k = contentW / tot;
							for (int32 ci = 0; ci < nCol; ++ci)
								cw[ci] *= k;
						}
						const float32 rowH = lh + 8.f * S;
						float32 ty = y + 4.f * S;
						for (usize ri = 0; ri < rows.Size(); ++ri) {
							float32 tx = x0;
							const bool head = (ri == 0);
							dl.AddRectFilled({x0, ty, tot > contentW ? contentW : tot, rowH}, head ? cHeadBg : cCellBg);
							for (int32 ci = 0; ci < nCol; ++ci) {
								const NkString &cell = ci < (int32)rows[ri].Size() ? rows[ri][ci] : NkString();
								dl.AddRect({tx, ty, cw[ci], rowH}, cBorder, 1.f);
								NkVector<NkMdRun> runs;
								NkMdInline(cell.CStr(), runs);
								float32 cx = tx + 10.f * S;
								for (usize rr = 0; rr < runs.Size(); ++rr) {
									const NkColor col = head ? cHead : cText;
									NkDrawTextU(ctx, cx, ty + (rowH - lh) * 0.5f + asc, ty, lh, runs[rr].text.CStr(),
												runs[rr].text.CStr() + (int32)runs[rr].text.Length(),
												runs[rr].style == NKMD_LINK ? cLink : col);
									cx += f->MeasureWidth(runs[rr].text.CStr());
								}
								tx += cw[ci];
							}
							ty += rowH;
						}
						y = ty + 6.f * S;
						li = j - 1;
						continue;
					}
				}

				// ── Titre # .. ###### ──
				int32 hl = 0;
				while (c[hl] == '#')
					++hl;
				if (hl >= 1 && hl <= 6 && c[hl] == ' ') {
					y += (hl <= 2 ? 14.f : 9.f) * S;
					NkVector<NkMdRun> runs;
					NkMdInline(c + hl + 1, runs);
					const float32 yTitle = y;
					drawRuns(runs, x0, contentW, hl <= 3 ? cHead : cAccent);
					if (hl <= 2) {
						dl.AddRectFilled({x0, yTitle + lh + 2.f * S, contentW, 1.f}, cBorder);
						y += 8.f * S;
					}
					continue;
				}

				// ── Regle horizontale ──
				if ((c[0] == '-' && c[1] == '-' && c[2] == '-') || (c[0] == '*' && c[1] == '*' && c[2] == '*') ||
					(c[0] == '_' && c[1] == '_' && c[2] == '_')) {
					y += 8.f * S;
					dl.AddRectFilled({x0, y, contentW, 1.f}, cBorder);
					y += lh;
					continue;
				}

				// ── Citation > ──
				if (c[0] == '>') {
					const char *q = c[1] == ' ' ? c + 2 : c + 1;
					const float32 y0 = y;
					NkVector<NkMdRun> runs;
					NkMdInline(q, runs);
					drawRuns(runs, x0 + 18.f * S, contentW - 18.f * S, cQuote);
					dl.AddRectFilled({x0, y0, 3.f * S, y - y0}, cAccent);
					continue;
				}

				// ── Liste - * + ou 1. (indentation par espaces) ──
				bool bullet = ((c[0] == '-' || c[0] == '*' || c[0] == '+') && c[1] == ' ');
				int32 num = 0;
				if (!bullet) {
					while (c[num] >= '0' && c[num] <= '9')
						++num;
					bullet = (num > 0 && c[num] == '.' && c[num + 1] == ' ');
				}
				if (bullet) {
					const float32 ind = x0 + (float32)(sp / 2) * 16.f * S;
					const char *item = num > 0 ? c + num + 2 : c + 2;
					if (num > 0) {
						NkString mk;
						for (int32 z = 0; z < num; ++z)
							mk += c[z];
						mk += ".";
						NkDrawTextU(ctx, ind, y + asc, y, lh, mk.CStr(), mk.CStr() + (int32)mk.Length(), cAccent);
					} else {
						dl.AddText(f->Face(), f->TexId(), {ind + 2.f * S, y + asc}, "\xE2\x80\xA2", cAccent); // •
					}
					NkVector<NkMdRun> runs;
					NkMdInline(item, runs);
					drawRuns(runs, ind + 20.f * S, contentW - (ind + 20.f * S - x0), cText);
					continue;
				}

				// ── Ligne vide ──
				if (c[0] == '\0') {
					y += lh * 0.5f;
					continue;
				}

				// ── Paragraphe ──
				NkVector<NkMdRun> runs;
				NkMdInline(c, runs);
				drawRuns(runs, x0, contentW, cText);
			}

			dl.PopClipRect();

			// Defilement (scrollbar standard).
			const float32 contentH = (y + scroll) - (area.y + padTop) + 40.f * S;
			const float32 maxS = contentH > area.h ? contentH - area.h : 0.f;
			if (scroll < 0.f)
				scroll = 0.f;
			if (scroll > maxS)
				scroll = maxS;
			if (overArea && ctx.input.wheel != 0.f) {
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
