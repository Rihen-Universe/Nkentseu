//
// NkPdfViewer.cpp — voir NkPdfViewer.h.
//
#include "NKCode/Shell/NkPdfViewer.h"

#include "NKEditorKit/NkEditorShell.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::nkcode::pdf;

		// Rend la page demandee si necessaire, puis met la texture a jour.
		// PARESSEUX : ne refait rien tant que page et zoom n'ont pas change.
		static void EnsurePage(editorkit::NkEditorShell *shell, NkPdfView *v, int32 viewW) {
			if (!v->opened || v->failed)
				return;
			if (v->renderedPage == v->pageIdx && v->renderedZoom == v->zoom && v->texId != 0)
				return;

			// Le zoom 1 ajuste la page a la largeur disponible. On convertit en
			// points par pouce, unite du moteur de rendu.
			double x0 = 0, y0 = 0, x1 = 612, y1 = 792;
			v->doc.PageMediaBox(v->pageIdx, &x0, &y0, &x1, &y1);
			const int32 rot = v->doc.PageRotate(v->pageIdx);
			const double wPt = ((rot == 90 || rot == 270) ? (y1 - y0) : (x1 - x0));
			if (wPt < 1.0)
				return;
			double dpi = (static_cast<double>(viewW) / wPt) * 72.0 * v->zoom;
			// Bornes : sous 24 ppp c'est illisible, au-dela de 400 la page pese des
			// centaines de mega-octets pour rien.
			if (dpi < 24.0)
				dpi = 24.0;
			if (dpi > 400.0)
				dpi = 400.0;

			NkPdfRenderer rend;
			if (!rend.RenderPage(v->doc, v->pageIdx, dpi, v->page) || !v->page.Valid()) {
				v->failed = true;
				return;
			}
			v->unsupported = rend.Unsupported();
			v->renderedPage = v->pageIdx;
			v->renderedZoom = v->zoom;

			const uint8 *px = v->page.Pixels();
			if (v->texId == 0 || v->texW != v->page.Width() || v->texH != v->page.Height()) {
				v->texId = shell->UploadRGBA(px, v->page.Width(), v->page.Height());
				v->texW = v->page.Width();
				v->texH = v->page.Height();
			} else
				shell->UpdateRGBA(v->texId, px, v->page.Width(), v->page.Height());
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

			// ── Ouverture UNE fois ──
			if (!v->opened) {
				v->opened = true;
				const NkPdfStatus st = v->doc.Open(path.CStr());
				if (st != NK_PDF_OK) {
					v->failed = true;
					v->unsupported = v->doc.StatusText();
				}
			}
			if (v->failed) {
				// On DIT pourquoi (document chiffre, index corrompu...) plutot que
				// d'afficher une zone vide qui laisserait croire a un bug.
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

			// ── Barre d'outils : page precedente/suivante, zoom ──
			const float32 barH = 34.f * ctx.S(1.f);
			const NkRect bar = {r.x, r.y, r.w, barH};
			dl.AddRectFilled(bar, NkColor{24, 26, 32, 255});

			float32 bx = bar.x + ctx.S(8);
			auto button = [&](const char *label, float32 wpx) -> bool {
				const NkRect b = {bx, bar.y + ctx.S(5), wpx, barH - ctx.S(10)};
				const bool hov = NkGuiRectContains(b, ctx.input.mousePos);
				dl.AddRectFilled(b, hov ? NkColor{58, 62, 74, 255} : NkColor{40, 43, 52, 255});
				if (font && font->Valid()) {
					const float32 tw = font->MeasureWidth(label);
					dl.AddText(font->Face(), font->TexId(),
							   {b.x + (b.w - tw) * 0.5f, b.y + (b.h - lh) * 0.5f + asc}, label,
							   NkCol::foreground);
				}
				bx += wpx + ctx.S(6);
				return hov && ctx.input.mouseClicked[0];
			};

			if (button("<", ctx.S(28)) && v->pageIdx > 0) {
				--v->pageIdx;
				v->scroll = 0.f;
			}
			if (button(">", ctx.S(28)) && v->pageIdx + 1 < v->doc.PageCount()) {
				++v->pageIdx;
				v->scroll = 0.f;
			}
			{
				const NkString lblS = NkPrintf("%d / %d", v->pageIdx + 1, v->doc.PageCount());
				if (font && font->Valid())
					dl.AddText(font->Face(), font->TexId(),
							   {bx, bar.y + (barH - lh) * 0.5f + asc}, lblS.CStr(), NkCol::mutedFg);
				bx += ctx.S(80);
			}
			if (button("-", ctx.S(28)) && v->zoom > 0.3)
				v->zoom -= 0.25;
			if (button("+", ctx.S(28)) && v->zoom < 6.0)
				v->zoom += 0.25;
			{
				const NkString lblS = NkPrintf("%d %%", static_cast<int32>(v->zoom * 100.0 + 0.5));
				if (font && font->Valid())
					dl.AddText(font->Face(), font->TexId(),
							   {bx, bar.y + (barH - lh) * 0.5f + asc}, lblS.CStr(), NkCol::mutedFg);
			}

			// Avertissement de fidelite, a DROITE : l'utilisateur doit savoir que
			// certaines choses ne sont pas rendues, plutot que de croire a un
			// document abime.
			if (!v->unsupported.Empty() && font && font->Valid()) {
				const NkString msg = NkString(NkT("pdf.partial")) + " : " + v->unsupported;
				const float32 tw = font->MeasureWidth(msg.CStr());
				if (tw < r.w * 0.5f)
					dl.AddText(font->Face(), font->TexId(),
							   {r.x + r.w - tw - ctx.S(10), bar.y + (barH - lh) * 0.5f + asc}, msg.CStr(),
							   NkColor{214, 168, 74, 255});
			}

			// ── Zone de page ──
			const NkRect area = {r.x, r.y + barH, r.w, r.h - barH};
			const int32 viewW = static_cast<int32>(area.w - ctx.S(24));
			if (viewW < 32) {
				dl.PopClipRect();
				return;
			}
			EnsurePage(shell, v, viewW);
			if (v->failed || v->texId == 0 || v->texW <= 0) {
				centered(NkT("pdf.render.error"), NkColor{232, 106, 106, 255});
				dl.PopClipRect();
				return;
			}

			// Molette : defilement vertical. Le facteur suit la hauteur de ligne
			// pour rester coherent avec le reste de l'IDE.
			if (NkGuiRectContains(area, ctx.input.mousePos) && ctx.input.wheel != 0.f)
				v->scroll -= ctx.input.wheel * lh * 3.f;

			const float32 pw = static_cast<float32>(v->texW);
			const float32 ph = static_cast<float32>(v->texH);
			const float32 maxScroll = (ph > area.h) ? (ph - area.h) : 0.f;
			if (v->scroll < 0.f)
				v->scroll = 0.f;
			if (v->scroll > maxScroll)
				v->scroll = maxScroll;

			const NkRect dst = {area.x + (area.w - pw) * 0.5f, area.y - v->scroll, pw, ph};
			// Ombre portee discrete : distingue la page du fond, comme le font les
			// lecteurs habituels.
			dl.AddRectFilled({dst.x + 2.f, dst.y + 2.f, dst.w, dst.h}, NkColor{0, 0, 0, 90});
			dl.AddImage(v->texId, dst, {0.f, 0.f}, {1.f, 1.f}, NkColor{255, 255, 255, 255});

			dl.PopClipRect();
		}

	} // namespace nkcode
} // namespace nkentseu
