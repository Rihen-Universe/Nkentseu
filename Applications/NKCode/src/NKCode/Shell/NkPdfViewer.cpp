//
// NkPdfViewer.cpp — voir NkPdfViewer.h.
//
#include "NKCode/Shell/NkPdfViewer.h"

#include "NKEditorKit/NkEditorShell.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::nkcode::pdf;

		// Epaisseur des barres de defilement, en pixels logiques.
		static constexpr float32 kBarPx = 12.f;

		// Points par pouce correspondant au zoom demande, pour la page courante.
		static double DpiFor(NkPdfView *v, int32 viewW) {
			double x0 = 0, y0 = 0, x1 = 612, y1 = 792;
			v->doc.PageMediaBox(v->pageIdx, &x0, &y0, &x1, &y1);
			const int32 rot = v->doc.PageRotate(v->pageIdx);
			const double wPt = ((rot == 90 || rot == 270) ? (y1 - y0) : (x1 - x0));
			if (wPt < 1.0 || viewW < 1)
				return 72.0;
			// Zoom 1 = la page tient dans la largeur disponible.
			double dpi = (static_cast<double>(viewW) / wPt) * 72.0 * v->zoom;
			if (dpi < 6.0)
				dpi = 6.0;
			// Plus de plafond bas : la memoire ne depend PLUS du zoom, puisqu'on ne
			// rend que la fenetre visible. On borne tout de meme tres haut pour
			// eviter des coordonnees aberrantes.
			if (dpi > 20000.0)
				dpi = 20000.0;
			return dpi;
		}

		// Rend la fenetre visible si necessaire. PARESSEUX : ne refait rien tant que
		// page, zoom, defilement et taille du panneau n'ont pas change.
		static void EnsureWindow(editorkit::NkEditorShell *shell, NkPdfView *v, int32 texW, int32 texH,
								 double dpi) {
			if (!v->opened || v->failed || texW < 4 || texH < 4)
				return;
			const bool same = v->renderedPage == v->pageIdx && v->renderedZoom == v->zoom &&
							  v->renderedScrollX == v->scrollX && v->renderedScrollY == v->scrollY &&
							  v->texW == texW && v->texH == texH && v->texId != 0;
			if (same)
				return;

			// Le canevas garde EXACTEMENT la taille du panneau. C'est ce qui rend la
			// texture stable : le backend fige les dimensions d'une texture a sa
			// creation et n'offre aucune liberation — la reallouer a chaque zoom
			// fuyait des dizaines de mega-octets, puis plantait.
			if (!v->page.Valid() || v->page.Width() != texW || v->page.Height() != texH) {
				if (!v->page.Create(texW, texH)) {
					v->failed = true;
					return;
				}
				v->texId = 0; // la texture doit etre recreee a cette nouvelle taille
			}

			NkPdfRenderer rend;
			if (!rend.RenderPageWindow(v->doc, v->pageIdx, dpi, v->scrollX, v->scrollY, v->page)) {
				v->failed = true;
				return;
			}
			v->unsupported = rend.Unsupported();
			v->renderedPage = v->pageIdx;
			v->renderedZoom = v->zoom;
			v->renderedScrollX = v->scrollX;
			v->renderedScrollY = v->scrollY;

			const uint8 *px = v->page.Pixels();
			if (v->texId == 0) {
				v->texId = shell->UploadRGBA(px, texW, texH);
				v->texW = texW;
				v->texH = texH;
			} else
				shell->UpdateRGBA(v->texId, px, texW, texH);
		}

		void DrawPdfViewer(NkGuiContext &ctx, editorkit::NkEditorShell *shell, const NkString &path,
						   const NkRect &r) {
			NkGuiDrawList &dl = ctx.DL();
			const NkGuiFont *font = ctx.font;
			const float32 lh = (font && font->Valid()) ? font->LineHeight() : 16.f;
			const float32 asc = (font && font->Valid()) ? font->Ascent() : 12.f;

			dl.PushClipRect(r, true);
			dl.AddRectFilled(r, NkColor{32, 34, 40, 255});

			auto centered = [&](const char *msg, const NkColor &col) {
				if (!font || !font->Valid())
					return;
				const float32 tw = font->MeasureWidth(msg);
				dl.AddText(font->Face(), font->TexId(),
						   {r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f + asc}, msg, col);
			};

			NkPdfView *v = NkPdfViewFor(path);

			if (!v->opened) {
				v->opened = true;
				if (v->doc.Open(path.CStr()) != NK_PDF_OK) {
					v->failed = true;
					v->unsupported = v->doc.StatusText();
				}
			}
			if (v->failed) {
				centered(v->unsupported.Empty() ? NkT("pdf.error") : v->unsupported.CStr(),
						 NkColor{232, 106, 106, 255});
				dl.PopClipRect();
				return;
			}
			if (v->doc.PageCount() <= 0) {
				centered(NkT("pdf.empty"), NkCol::mutedFg);
				dl.PopClipRect();
				return;
			}

			// ── Barre d'outils FIXE ──────────────────────────────────────────────
			// Elle est dessinee APRES la page et sa propre zone est EXCLUE du clip de
			// la page : sans ca, une page defilee vers le haut passait par-dessus.
			const float32 barH = 34.f * ctx.S(1.f);
			const NkRect bar = {r.x, r.y, r.w, barH};
			const NkRect area = {r.x, r.y + barH, r.w, r.h - barH};
			if (area.w < 16.f || area.h < 16.f) {
				dl.PopClipRect();
				return;
			}

			// ── Geometrie : page entiere, fenetre visible, barres ────────────────
			const float32 sb = ctx.S(kBarPx);
			int32 fullW = 0, fullH = 0;
			double dpi = DpiFor(v, static_cast<int32>(area.w - sb));
			NkPdfRenderer::PagePixelSize(v->doc, v->pageIdx, dpi, &fullW, &fullH);

			// Les barres n'apparaissent que si elles servent, et leur presence reduit
			// la zone utile — ce qui peut faire apparaitre l'autre : on evalue donc
			// les deux ensemble.
			bool needV = static_cast<float32>(fullH) > area.h;
			bool needH = static_cast<float32>(fullW) > area.w - (needV ? sb : 0.f);
			if (needH && !needV)
				needV = static_cast<float32>(fullH) > area.h - sb;

			const NkRect view = {area.x, area.y, area.w - (needV ? sb : 0.f),
								 area.h - (needH ? sb : 0.f)};
			const float32 maxSx = (static_cast<float32>(fullW) > view.w)
									  ? (static_cast<float32>(fullW) - view.w) : 0.f;
			const float32 maxSy = (static_cast<float32>(fullH) > view.h)
									  ? (static_cast<float32>(fullH) - view.h) : 0.f;

			// ── Interactions ─────────────────────────────────────────────────────
			const bool overView = NkGuiRectContains(view, ctx.input.mousePos);
			if (overView && ctx.input.wheel != 0.f) {
				// Molette + Ctrl = zoom, comme partout ailleurs.
				if (ctx.input.ctrlDown) {
					const double z = v->zoom * (ctx.input.wheel > 0.f ? 1.15 : 1.0 / 1.15);
					v->zoom = (z < 0.1) ? 0.1 : (z > 40.0 ? 40.0 : z);
				} else
					v->scrollY -= ctx.input.wheel * lh * 3.f;
			}
			if (overView && ctx.input.wheelH != 0.f)
				v->scrollX -= ctx.input.wheelH * lh * 3.f;

			// Le defilement est borne APRES coup : le zoom a pu reduire la page.
			if (v->scrollX < 0.f) v->scrollX = 0.f;
			if (v->scrollX > maxSx) v->scrollX = maxSx;
			if (v->scrollY < 0.f) v->scrollY = 0.f;
			if (v->scrollY > maxSy) v->scrollY = maxSy;

			// ── Rendu de la fenetre visible ──────────────────────────────────────
			const int32 texW = static_cast<int32>(view.w);
			const int32 texH = static_cast<int32>(view.h);
			EnsureWindow(shell, v, texW, texH, dpi);
			if (v->failed || v->texId == 0) {
				centered(NkT("pdf.render.error"), NkColor{232, 106, 106, 255});
				dl.PopClipRect();
				return;
			}

			// La page est clippee a `view` : elle ne peut plus deborder sur la barre.
			dl.PushClipRect(view, true);
			dl.AddImage(v->texId, {view.x, view.y, static_cast<float32>(texW), static_cast<float32>(texH)},
						{0.f, 0.f}, {1.f, 1.f}, NkColor{255, 255, 255, 255});

			// ── Selection de texte ────────────────────────────────────────────────
			// Un PDF ne stocke PAS de texte : il place des glyphes un par un. La
			// selection travaille donc sur les boites relevees au rendu, dans l'ordre
			// du flux de contenu — qui est l'ordre de lecture dans l'immense majorite
			// des documents.
			{
				const NkVec2 mp = ctx.input.mousePos;
				const float32 lx = mp.x - view.x, ly = mp.y - view.y;

				// Element sous la souris, ou le plus proche sur la meme ligne : sans
				// cette tolerance, il faudrait viser le glyphe au pixel pres.
				auto pick = [&]() -> int32 {
					int32 best = -1;
					float32 bestD = 1e30f;
					for (usize i = 0; i < v->items.Size(); ++i) {
						const pdf::NkPdfRenderer::TextItem &t = v->items[i];
						if (ly < t.y || ly > t.y + t.h)
							continue; // pas sur cette ligne
						const float32 cx = t.x + t.w * 0.5f;
						const float32 d = (lx > cx) ? (lx - cx) : (cx - lx);
						if (d < bestD) {
							bestD = d;
							best = static_cast<int32>(i);
						}
					}
					return best;
				};

				if (overView && ctx.input.mouseClicked[0]) {
					const int32 k = pick();
					if (k >= 0) {
						v->selA = v->selB = k;
						v->selecting = true;
					} else
						v->selA = v->selB = -1;
				}
				if (v->selecting && ctx.input.mouseDown[0]) {
					const int32 k = pick();
					if (k >= 0)
						v->selB = k;
				}
				if (!ctx.input.mouseDown[0])
					v->selecting = false;

				// Surlignage
				if (v->selA >= 0 && v->selB >= 0) {
					const int32 a = v->selA < v->selB ? v->selA : v->selB;
					const int32 b = v->selA < v->selB ? v->selB : v->selA;
					for (int32 i = a; i <= b && static_cast<usize>(i) < v->items.Size(); ++i) {
						const pdf::NkPdfRenderer::TextItem &t = v->items[static_cast<usize>(i)];
						dl.AddRectFilled({view.x + t.x, view.y + t.y, t.w, t.h},
										 NkColor{70, 130, 220, 90});
					}
				}

				// Ctrl+C : copie. Les sauts de ligne sont DEDUITS d'un recul
				// horizontal ou d'un changement de hauteur — un PDF ne dit nulle part
				// ou finit une ligne.
				if (v->selA >= 0 && v->selB >= 0 && ctx.input.ctrlDown &&
					ctx.input.KeyPressed(NkGuiKey::C)) {
					const int32 a = v->selA < v->selB ? v->selA : v->selB;
					const int32 b = v->selA < v->selB ? v->selB : v->selA;
					NkString out;
					float32 prevX = -1e30f, prevY = -1e30f;
					int32 manquants = 0;
					for (int32 i = a; i <= b && static_cast<usize>(i) < v->items.Size(); ++i) {
						const pdf::NkPdfRenderer::TextItem &t = v->items[static_cast<usize>(i)];
						if (prevX > -1e29f) {
							const float32 dy = (t.y > prevY) ? (t.y - prevY) : (prevY - t.y);
							if (t.x + 1.f < prevX || dy > t.h * 0.5f)
								out += "\n";
						}
						if (t.text.Empty())
							++manquants;
						else
							out += t.text;
						prevX = t.x;
						prevY = t.y;
					}
					if (!out.Empty())
						ctx.SetClipboard(out.CStr());
					(void)manquants;
				}
			}
			dl.PopClipRect();

			// ── Barres de defilement ─────────────────────────────────────────────
			auto scrollbar = [&](bool vertical) {
				const NkRect track = vertical
										 ? NkRect{view.x + view.w, view.y, sb, view.h}
										 : NkRect{view.x, view.y + view.h, view.w, sb};
				dl.AddRectFilled(track, NkColor{26, 28, 34, 255});
				const float32 total = vertical ? static_cast<float32>(fullH) : static_cast<float32>(fullW);
				const float32 vis = vertical ? view.h : view.w;
				if (total <= vis || total <= 0.f)
					return;
				const float32 len = vertical ? track.h : track.w;
				// Curseur d'au moins 24 px : en dessous il devient inattrapable.
				float32 thumb = len * (vis / total);
				if (thumb < ctx.S(24.f))
					thumb = ctx.S(24.f);
				const float32 maxS = vertical ? maxSy : maxSx;
				const float32 cur = vertical ? v->scrollY : v->scrollX;
				const float32 pos = (maxS > 0.f) ? (cur / maxS) * (len - thumb) : 0.f;
				const NkRect th = vertical ? NkRect{track.x + 2.f, track.y + pos, sb - 4.f, thumb}
										   : NkRect{track.x + pos, track.y + 2.f, thumb, sb - 4.f};
				const bool hov = NkGuiRectContains(th, ctx.input.mousePos);
				dl.AddRectFilled(th, hov ? NkColor{120, 128, 144, 255} : NkColor{74, 80, 92, 255});

				// Glisser : on memorise l'ecart au point de saisie, sinon le curseur
				// saute sous la souris au premier pixel de mouvement.
				int32 &drag = vertical ? v->dragV : v->dragH;
				float32 &grab = vertical ? v->grabV : v->grabH;
				if (ctx.input.mouseClicked[0] && hov) {
					drag = 1;
					grab = (vertical ? ctx.input.mousePos.y - th.y : ctx.input.mousePos.x - th.x);
				}
				if (!ctx.input.mouseDown[0])
					drag = 0;
				if (drag) {
					const float32 p = (vertical ? ctx.input.mousePos.y - track.y - grab
												: ctx.input.mousePos.x - track.x - grab);
					const float32 span = len - thumb;
					const float32 frac = (span > 0.f) ? (p / span) : 0.f;
					const float32 val = frac * maxS;
					if (vertical)
						v->scrollY = (val < 0.f) ? 0.f : (val > maxSy ? maxSy : val);
					else
						v->scrollX = (val < 0.f) ? 0.f : (val > maxSx ? maxSx : val);
				} else if (ctx.input.mouseClicked[0] && NkGuiRectContains(track, ctx.input.mousePos)) {
					// Clic dans la piste hors du curseur : saut d'une « page ».
					const float32 m = vertical ? ctx.input.mousePos.y : ctx.input.mousePos.x;
					const float32 t0 = vertical ? th.y : th.x;
					const float32 step = vis * 0.9f;
					if (vertical)
						v->scrollY += (m < t0) ? -step : step;
					else
						v->scrollX += (m < t0) ? -step : step;
				}
			};
			if (needV)
				scrollbar(true);
			if (needH)
				scrollbar(false);
			if (needV && needH) // coin entre les deux barres
				dl.AddRectFilled({view.x + view.w, view.y + view.h, sb, sb}, NkColor{26, 28, 34, 255});

			// ── Barre d'outils, dessinee EN DERNIER pour rester au-dessus ────────
			dl.AddRectFilled(bar, NkColor{24, 26, 32, 255});
			float32 bx = bar.x + ctx.S(8);
			auto button = [&](const char *label, float32 wpx, bool enabled) -> bool {
				const NkRect b = {bx, bar.y + ctx.S(5), wpx, barH - ctx.S(10)};
				const bool hov = enabled && NkGuiRectContains(b, ctx.input.mousePos);
				dl.AddRectFilled(b, hov ? NkColor{58, 62, 74, 255}
										: NkColor{40, 43, 52, static_cast<uint8>(enabled ? 255 : 120)});
				if (font && font->Valid()) {
					const float32 tw = font->MeasureWidth(label);
					dl.AddText(font->Face(), font->TexId(),
							   {b.x + (b.w - tw) * 0.5f, b.y + (b.h - lh) * 0.5f + asc}, label,
							   enabled ? NkCol::foreground : NkCol::mutedFg);
				}
				bx += wpx + ctx.S(6);
				return hov && ctx.input.mouseClicked[0];
			};

			const bool canPrev = v->pageIdx > 0;
			const bool canNext = v->pageIdx + 1 < v->doc.PageCount();
			if (button("<", ctx.S(28), canPrev)) {
				--v->pageIdx;
				v->scrollX = v->scrollY = 0.f;
			}
			if (button(">", ctx.S(28), canNext)) {
				++v->pageIdx;
				v->scrollX = v->scrollY = 0.f;
			}
			{
				const NkString lbl = NkPrintf("%d / %d", v->pageIdx + 1, v->doc.PageCount());
				if (font && font->Valid())
					dl.AddText(font->Face(), font->TexId(), {bx, bar.y + (barH - lh) * 0.5f + asc},
							   lbl.CStr(), NkCol::mutedFg);
				bx += ctx.S(84);
			}
			if (button("-", ctx.S(28), v->zoom > 0.11))
				v->zoom /= 1.25;
			if (button("+", ctx.S(28), v->zoom < 39.0))
				v->zoom *= 1.25;
			if (button("100 %", ctx.S(52), true))
				v->zoom = 1.0;
			{
				const NkString lbl = NkPrintf("%d %%", static_cast<int32>(v->zoom * 100.0 + 0.5));
				if (font && font->Valid())
					dl.AddText(font->Face(), font->TexId(), {bx, bar.y + (barH - lh) * 0.5f + asc},
							   lbl.CStr(), NkCol::mutedFg);
			}

			// Avertissement de fidelite : l'utilisateur doit savoir qu'une page est
			// partielle, plutot que de croire son document abime.
			if (!v->unsupported.Empty() && font && font->Valid()) {
				const NkString msg = NkString(NkT("pdf.partial")) + " : " + v->unsupported;
				const float32 tw = font->MeasureWidth(msg.CStr());
				if (tw < r.w * 0.45f)
					dl.AddText(font->Face(), font->TexId(),
							   {r.x + r.w - tw - ctx.S(10), bar.y + (barH - lh) * 0.5f + asc},
							   msg.CStr(), NkColor{214, 168, 74, 255});
			}

			dl.PopClipRect();
		}

	} // namespace nkcode
} // namespace nkentseu
