//
// NkPdfViewer.cpp — voir NkPdfViewer.h.
//
#include "NKCode/Shell/NkPdfViewer.h"

#include "NKEditorKit/NkEditorScrollbar.h" // barres STANDARD du moteur
#include "NKEditorKit/NkEditorShell.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::media::pdf;

		// Espace entre deux pages en mode continu, en pixels logiques.
		static constexpr float32 kGapPx = 10.f;
		// Nombre de pages gardees en cache. 4 suffit a couvrir la page courante,
		// ses voisines immediates, et la precedente — c'est ce qui rend la
		// navigation instantanee au lieu de rendre a chaque changement.
		static constexpr usize kCacheMax = 4;

		static float32 BarPx() { return editorkit::NkScrollbarWidth(); }

		// Points par pouce pour une largeur de panneau donnee, a partir de la page 0
		// (les pages d'un meme document ont presque toujours la meme largeur ; les
		// autres seront simplement un peu plus larges ou etroites).
		static double DpiFor(NkPdfView *v, int32 viewW) {
			double x0 = 0, y0 = 0, x1 = 612, y1 = 792;
			v->doc.PageMediaBox(0, &x0, &y0, &x1, &y1);
			const int32 rot = v->doc.PageRotate(0);
			const double wPt = ((rot == 90 || rot == 270) ? (y1 - y0) : (x1 - x0));
			if (wPt < 1.0 || viewW < 1)
				return 72.0;
			double dpi = (static_cast<double>(viewW) / wPt) * 72.0 * v->zoom;
			if (dpi < 6.0)
				dpi = 6.0;
			if (dpi > 20000.0)
				dpi = 20000.0;
			return dpi;
		}

		// Calcule la position de chaque page SANS en rendre aucune : seules les
		// dimensions sont lues. C'est ce qui permet un defilement sur tout le
		// document sans payer le rendu des 95 pages.
		static void EnsureLayout(NkPdfView *v, double dpi, int32 panelW, float32 gap) {
			if (v->layoutZoom == v->zoom && v->layoutPanelW == panelW && !v->pageTop.Empty())
				return;
			v->pageW.Clear();
			v->pageH.Clear();
			v->pageTop.Clear();
			float32 y = 0.f;
			float32 maxW = 0.f;
			const int32 n = v->doc.PageCount();
			for (int32 i = 0; i < n; ++i) {
				int32 w = 0, h = 0;
				if (!NkPdfRenderer::PagePixelSize(v->doc, i, dpi, &w, &h)) {
					w = 1;
					h = 1;
				}
				v->pageW.PushBack(w);
				v->pageH.PushBack(h);
				v->pageTop.PushBack(y);
				y += static_cast<float32>(h) + gap;
				if (static_cast<float32>(w) > maxW)
					maxW = static_cast<float32>(w);
			}
			v->docW = maxW;
			v->docH = (n > 0) ? (y - gap) : 0.f;
			v->layoutZoom = v->zoom;
			v->layoutPanelW = panelW;
		}

		// Recupere les pages terminees par le fil de fond et les range en cache.
		static void DrainWorker(NkPdfView *v) {
			for (;;) {
				int32 page = -1;
				double zoom = 1.0;
				NkPdfCanvas canvas;
				NkVector<NkPdfRenderer::TextItem> items;
				NkString unsup;
				if (!v->worker.TakeResult(&page, &zoom, canvas, items, unsup))
					return;
				if (page < 0)
					continue;
				NkPdfPageCache *slot = nullptr;
				for (usize i = 0; i < v->cache.Size(); ++i)
					if (v->cache[i]->page == page && v->cache[i]->zoom == zoom)
						slot = v->cache[i];
				if (!slot) {
					if (v->cache.Size() >= kCacheMax) {
						usize oldest = 0;
						for (usize i = 1; i < v->cache.Size(); ++i)
							if (v->cache[i]->lastUse < v->cache[oldest]->lastUse)
								oldest = i;
						slot = v->cache[oldest];
					} else {
						slot = new NkPdfPageCache();
						v->cache.PushBack(slot);
					}
				}
				slot->canvas.Swap(canvas);
				slot->items = items;
				slot->unsupported = unsup;
				slot->page = page;
				slot->zoom = zoom;
				slot->lastUse = ++v->useClock;
				// Le contenu a change : la fenetre doit etre reassemblee.
				v->builtScrollY = -1e30f;
			}
		}

		// Page prete, ou nullptr. NE BLOQUE JAMAIS : si la page manque, elle est
		// DEMANDEE au fil de fond et l'interface continue. C'est tout l'objet du
		// changement — auparavant, chaque zoom ou changement de page figeait
		// l'application le temps du rendu.
		static NkPdfPageCache *GetPage(NkPdfView *v, int32 page, double dpi) {
			for (usize i = 0; i < v->cache.Size(); ++i)
				if (v->cache[i]->page == page && v->cache[i]->zoom == v->zoom) {
					v->cache[i]->lastUse = ++v->useClock;
					return v->cache[i];
				}
			v->worker.Request(page, v->zoom, dpi);
			v->waiting = true;
			return nullptr;
		}

		// A defaut de la page au bon zoom, une version rendue a un AUTRE zoom :
		// affichee mise a l'echelle, elle donne un apercu immediat au lieu d'un
		// trou gris. C'est ce que font les lecteurs pendant qu'ils recalculent.
		static NkPdfPageCache *GetPageAnyZoom(NkPdfView *v, int32 page) {
			NkPdfPageCache *best = nullptr;
			for (usize i = 0; i < v->cache.Size(); ++i)
				if (v->cache[i]->page == page && v->cache[i]->canvas.Valid())
					if (!best || v->cache[i]->lastUse > best->lastUse)
						best = v->cache[i];
			return best;
		}

		// Assemble la fenetre visible a partir des pages, en recopiant. Aucun rendu
		// n'a lieu ici pour une page deja en cache : c'est ce qui rend le defilement
		// fluide.
		static void BuildWindow(editorkit::NkEditorShell *shell, NkPdfView *v, int32 texW, int32 texH,
								double dpi, float32 gap) {
			const bool same = v->builtScrollX == v->scrollX && v->builtScrollY == v->scrollY &&
							  v->builtZoom == v->zoom && v->builtPanelW == texW &&
							  v->builtPanelH == texH && v->builtContinuous == v->continuous &&
							  v->texId != 0;
			if (same)
				return;

			if (!v->window.Valid() || v->window.Width() != texW || v->window.Height() != texH) {
				if (!v->window.Create(texW, texH)) {
					v->failed = true;
					return;
				}
				v->texId = 0; // taille figee a la creation cote backend : on recree
			}
			// Fond gris : distingue le hors-page du blanc d'une page.
			v->window.Clear(210, 210, 214, 255);

			v->items.Clear();
			v->unsupported.Clear();

			const int32 first = v->continuous ? 0 : v->pageIdx;
			const int32 last = v->continuous ? (v->doc.PageCount() - 1) : v->pageIdx;
			uint8 *dst = v->window.Pixels();

			for (int32 p = first; p <= last; ++p) {
				const float32 top = v->continuous ? v->pageTop[static_cast<usize>(p)] : 0.f;
				const float32 ph = static_cast<float32>(v->pageH[static_cast<usize>(p)]);
				// Ignore les pages hors de la fenetre : c'est ce qui permet un
				// document de 95 pages sans en rendre 95.
				if (top + ph < v->scrollY || top > v->scrollY + static_cast<float32>(texH))
					continue;

				NkPdfPageCache *pc = GetPage(v, p, dpi);
				double sxScale = 1.0, syScale = 1.0;
				if (!pc) {
					// Page pas encore rendue au zoom courant : on affiche celle d'un
					// autre zoom, mise a l'echelle. Un apercu approximatif vaut mieux
					// qu'un trou gris, et il disparait des que le rendu arrive.
					pc = GetPageAnyZoom(v, p);
					if (!pc)
						continue;
					sxScale = static_cast<double>(pc->canvas.Width()) /
							  static_cast<double>(v->pageW[static_cast<usize>(p)]);
					syScale = static_cast<double>(pc->canvas.Height()) /
							  static_cast<double>(v->pageH[static_cast<usize>(p)]);
				}
				if (!pc->unsupported.Empty() && v->unsupported.Empty())
					v->unsupported = pc->unsupported;

				// Page CENTREE horizontalement dans la largeur du document.
				const float32 pw = static_cast<float32>(v->pageW[static_cast<usize>(p)]);
				const float32 left = (v->docW > pw) ? ((v->docW - pw) * 0.5f) : 0.f;

				const int32 sw = pc->canvas.Width(), shh = pc->canvas.Height();
				const uint8 *src = pc->canvas.Pixels();
				for (int32 y = 0; y < texH; ++y) {
					const float32 docY = v->scrollY + static_cast<float32>(y);
					const int32 sy = static_cast<int32>((docY - top) * syScale);
					if (sy < 0 || sy >= shh)
						continue;
					uint8 *drow = dst + static_cast<usize>(y) * static_cast<usize>(texW) * 4u;
					for (int32 x = 0; x < texW; ++x) {
						const float32 docX = v->scrollX + static_cast<float32>(x);
						const int32 sx = static_cast<int32>((docX - left) * sxScale);
						if (sx < 0 || sx >= sw)
							continue;
						const uint8 *s = src + (static_cast<usize>(sy) * static_cast<usize>(sw) +
											   static_cast<usize>(sx)) * 4u;
						uint8 *d = drow + static_cast<usize>(x) * 4u;
						d[0] = s[0];
						d[1] = s[1];
						d[2] = s[2];
						d[3] = 255;
					}
				}

				// Elements de texte, ramenes en coordonnees du DOCUMENT pour que la
				// selection fonctionne a travers les pages.
				// Les boites de texte d'un rendu a un AUTRE zoom seraient a la
				// mauvaise echelle : on ne les prend pas, plutot que de proposer une
				// selection decalee. Elles arrivent avec le vrai rendu.
				if (sxScale == 1.0 && syScale == 1.0)
					for (usize i = 0; i < pc->items.Size(); ++i) {
						NkPdfRenderer::TextItem it = pc->items[i];
						it.x += left;
						it.y += top;
						v->items.PushBack(it);
					}
			}

			v->builtScrollX = v->scrollX;
			v->builtScrollY = v->scrollY;
			v->builtZoom = v->zoom;
			v->builtPanelW = texW;
			v->builtPanelH = texH;
			v->builtContinuous = v->continuous;
			(void)gap;

			const uint8 *px = v->window.Pixels();
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
				} else
					v->worker.Open(path); // le fil ouvre sa PROPRE instance
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

			const float32 barH = 34.f * ctx.S(1.f);
			const NkRect bar = {r.x, r.y, r.w, barH};
			const NkRect area = {r.x, r.y + barH, r.w, r.h - barH};
			if (area.w < 16.f || area.h < 16.f) {
				dl.PopClipRect();
				return;
			}

			const float32 sb = ctx.S(BarPx());
			const float32 gap = ctx.S(kGapPx);
			const double dpi = DpiFor(v, static_cast<int32>(area.w - sb));
			EnsureLayout(v, dpi, static_cast<int32>(area.w - sb), gap);

			// GARDE-FOUS. La disposition est indexee par numero de page dans
			// plusieurs formules. Si elle est vide ou desynchronisee du document, on
			// sort proprement au lieu de lire hors des tableaux : ce genre de
			// depassement passe souvent inapercu en Debug et corrompt la memoire en
			// Release.
			const int32 nPages = v->doc.PageCount();
			if (v->pageTop.Size() != static_cast<usize>(nPages) ||
				v->pageH.Size() != static_cast<usize>(nPages) ||
				v->pageW.Size() != static_cast<usize>(nPages)) {
				centered(NkT("pdf.render.error"), NkColor{232, 106, 106, 255});
				dl.PopClipRect();
				return;
			}
			if (v->pageIdx < 0)
				v->pageIdx = 0;
			if (v->pageIdx >= nPages)
				v->pageIdx = nPages - 1;

			// Etendue defilable : tout le document en continu, la page seule sinon.
			const float32 totalH = v->continuous
									   ? v->docH
									   : static_cast<float32>(v->pageH[static_cast<usize>(v->pageIdx)]);
			const float32 totalW = v->docW;

			bool needV = totalH > area.h;
			bool needH = totalW > area.w - (needV ? sb : 0.f);
			if (needH && !needV)
				needV = totalH > area.h - sb;
			const NkRect view = {area.x, area.y, area.w - (needV ? sb : 0.f),
								 area.h - (needH ? sb : 0.f)};
			const float32 maxSx = (totalW > view.w) ? (totalW - view.w) : 0.f;
			const float32 maxSy = (totalH > view.h) ? (totalH - view.h) : 0.f;

			// ── Interactions ──
			const bool overView = NkGuiRectContains(view, ctx.input.mousePos);
			if (overView && ctx.input.wheel != 0.f) {
				if (ctx.input.ctrlDown) {
					const double z = v->zoom * (ctx.input.wheel > 0.f ? 1.15 : 1.0 / 1.15);
					v->zoom = (z < 0.1) ? 0.1 : (z > 40.0 ? 40.0 : z);
				} else
					v->scrollY -= ctx.input.wheel * lh * 3.f;
			}
			if (overView && ctx.input.wheelH != 0.f)
				v->scrollX -= ctx.input.wheelH * lh * 3.f;

			if (v->scrollX < 0.f) v->scrollX = 0.f;
			if (v->scrollX > maxSx) v->scrollX = maxSx;
			if (v->scrollY < 0.f) v->scrollY = 0.f;
			if (v->scrollY > maxSy) v->scrollY = maxSy;

			// En mode continu, la page « courante » est celle qui occupe le haut de la
			// fenetre : c'est elle qu'affiche le compteur.
			if (v->continuous) {
				for (int32 i = 0; i < v->doc.PageCount(); ++i) {
					const float32 top = v->pageTop[static_cast<usize>(i)];
					const float32 bot = top + static_cast<float32>(v->pageH[static_cast<usize>(i)]);
					if (v->scrollY + view.h * 0.3f < bot) {
						v->pageIdx = i;
						break;
					}
					(void)top;
				}
			}

			DrainWorker(v); // recupere ce que le fil a termine
			v->waiting = false;
			const int32 texW = static_cast<int32>(view.w);
			const int32 texH = static_cast<int32>(view.h);
			BuildWindow(shell, v, texW, texH, dpi, gap);
			if (v->failed || v->texId == 0) {
				centered(NkT("pdf.render.error"), NkColor{232, 106, 106, 255});
				dl.PopClipRect();
				return;
			}

			dl.PushClipRect(view, true);
			dl.AddImage(v->texId, {view.x, view.y, static_cast<float32>(texW), static_cast<float32>(texH)},
						{0.f, 0.f}, {1.f, 1.f}, NkColor{255, 255, 255, 255});

			// ── Selection de texte ──
			// Les boites sont en coordonnees du DOCUMENT : la selection traverse donc
			// les pages en mode continu, ce qu'on attend d'un lecteur.
			{
				const NkVec2 mp = ctx.input.mousePos;
				const float32 lx = mp.x - view.x + v->scrollX;
				const float32 ly = mp.y - view.y + v->scrollY;

				auto pick = [&]() -> int32 {
					int32 best = -1;
					float32 bestD = 1e30f;
					for (usize i = 0; i < v->items.Size(); ++i) {
						const NkPdfRenderer::TextItem &t = v->items[i];
						if (ly < t.y || ly > t.y + t.h)
							continue;
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
					v->selA = v->selB = k;
					v->selecting = k >= 0;
				}
				if (v->selecting && ctx.input.mouseDown[0]) {
					const int32 k = pick();
					if (k >= 0)
						v->selB = k;
				}
				if (!ctx.input.mouseDown[0])
					v->selecting = false;

				if (v->selA >= 0 && v->selB >= 0) {
					const int32 a = v->selA < v->selB ? v->selA : v->selB;
					const int32 b = v->selA < v->selB ? v->selB : v->selA;
					for (int32 i = a; i <= b && static_cast<usize>(i) < v->items.Size(); ++i) {
						const NkPdfRenderer::TextItem &t = v->items[static_cast<usize>(i)];
						dl.AddRectFilled({view.x + t.x - v->scrollX, view.y + t.y - v->scrollY, t.w, t.h},
										 NkColor{70, 130, 220, 90});
					}
					if (ctx.input.ctrlDown && ctx.input.KeyPressed(NkGuiKey::C)) {
						NkString out;
						float32 prevX = -1e30f, prevY = -1e30f;
						for (int32 i = a; i <= b && static_cast<usize>(i) < v->items.Size(); ++i) {
							const NkPdfRenderer::TextItem &t = v->items[static_cast<usize>(i)];
							if (prevX > -1e29f) {
								const float32 dy = (t.y > prevY) ? (t.y - prevY) : (prevY - t.y);
								// Un saut de ligne ne se lit nulle part dans un PDF : on le
								// DEDUIT d'un recul horizontal ou d'un changement de hauteur.
								if (t.x + 1.f < prevX || dy > t.h * 0.5f)
									out += "\n";
							}
							out += t.text;
							prevX = t.x;
							prevY = t.y;
						}
						if (!out.Empty())
							ctx.SetClipboard(out.CStr());
					}
				}
			}
			dl.PopClipRect();

			// Reperage discret pendant qu'une page se rend : l'utilisateur doit
			// voir que ca TRAVAILLE, sinon il croit a un blocage — ce qui etait
			// justement le cas avant le passage en tache de fond.
			if (v->waiting && font && font->Valid()) {
				const char *msg = NkT("pdf.rendering");
				const float32 tw = font->MeasureWidth(msg);
				const NkRect box = {view.x + view.w - tw - ctx.S(24), view.y + ctx.S(8), tw + ctx.S(16),
									lh + ctx.S(8)};
				dl.AddRectFilled(box, NkColor{24, 26, 32, 220});
				dl.AddText(font->Face(), font->TexId(),
						   {box.x + ctx.S(8), box.y + ctx.S(4) + asc}, msg, NkCol::mutedFg);
			}

			// ── Barres STANDARD du moteur ──
			if (needV) {
				const NkRect track = {view.x + view.w, view.y, sb, view.h};
				editorkit::NkVScrollbar(ctx, dl, track, v->scrollY, totalH, view.h, 0x50DF0001u, lh * 3.f);
			}
			if (needH) {
				const NkRect track = {view.x, view.y + view.h, view.w, sb};
				editorkit::NkHScrollbar(ctx, dl, track, v->scrollX, totalW, view.w, 0x50DF0002u, lh * 3.f);
			}
			if (needV && needH)
				dl.AddRectFilled({view.x + view.w, view.y + view.h, sb, sb}, NkColor{26, 28, 34, 255});

			// ── Barre d'outils, dessinee EN DERNIER pour rester au-dessus ──
			dl.AddRectFilled(bar, NkColor{24, 26, 32, 255});
			float32 bx = bar.x + ctx.S(8);
			auto button = [&](const char *label, float32 wpx, bool enabled, bool active = false) -> bool {
				const NkRect b = {bx, bar.y + ctx.S(5), wpx, barH - ctx.S(10)};
				const bool hov = enabled && NkGuiRectContains(b, ctx.input.mousePos);
				dl.AddRectFilled(b, active ? NkCol::primary
										   : (hov ? NkColor{58, 62, 74, 255}
												  : NkColor{40, 43, 52,
															static_cast<uint8>(enabled ? 255 : 120)}));
				if (font && font->Valid()) {
					const float32 tw = font->MeasureWidth(label);
					dl.AddText(font->Face(), font->TexId(),
							   {b.x + (b.w - tw) * 0.5f, b.y + (b.h - lh) * 0.5f + asc}, label,
							   enabled ? NkCol::foreground : NkCol::mutedFg);
				}
				bx += wpx + ctx.S(6);
				return hov && ctx.input.mouseClicked[0];
			};

			// Naviguer = DEPLACER LE DEFILEMENT en mode continu (instantane, la page
			// est deja rendue ou le sera seule), changer de page sinon.
			if (button("<", ctx.S(28), v->pageIdx > 0)) {
				if (v->continuous)
					v->scrollY = v->pageTop[static_cast<usize>(v->pageIdx > 0 ? v->pageIdx - 1 : 0)];
				else {
					--v->pageIdx;
					v->scrollY = 0.f;
				}
			}
			if (button(">", ctx.S(28), v->pageIdx + 1 < v->doc.PageCount())) {
				if (v->continuous)
					v->scrollY = v->pageTop[static_cast<usize>(v->pageIdx + 1)];
				else {
					++v->pageIdx;
					v->scrollY = 0.f;
				}
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
				bx += ctx.S(60);
			}
			// Bascule de mode, mise en evidence quand le mode continu est actif.
			if (button(NkT("pdf.mode.continuous"), ctx.S(96), true, v->continuous)) {
				// On conserve la position de lecture en passant d'un mode a l'autre :
				// sans ca, l'utilisateur perd sa place a chaque bascule.
				if (v->continuous)
					v->scrollY -= v->pageTop[static_cast<usize>(v->pageIdx)];
				else
					v->scrollY += v->pageTop[static_cast<usize>(v->pageIdx)];
				v->continuous = !v->continuous;
			}

			if (!v->unsupported.Empty() && font && font->Valid()) {
				const NkString msg = NkString(NkT("pdf.partial")) + " : " + v->unsupported;
				const float32 tw = font->MeasureWidth(msg.CStr());
				if (tw < r.w * 0.35f)
					dl.AddText(font->Face(), font->TexId(),
							   {r.x + r.w - tw - ctx.S(10), bar.y + (barH - lh) * 0.5f + asc},
							   msg.CStr(), NkColor{214, 168, 74, 255});
			}

			dl.PopClipRect();
		}

	} // namespace nkcode
} // namespace nkentseu
