// -----------------------------------------------------------------------------
// @File    NkFamilleStyle.cpp
// @Brief   Le dessin commun de la famille (voir NkFamilleStyle.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleStyle.h"
#include "NKEditorKit/NkEditorTextField.h"

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		NkFamillePalette NkFamillePaletteDe(const NkTheme &t) noexcept {
			auto R = [&t](NkRole r) { return NkThemeUnpack(t.Get(r)); };
			NkFamillePalette p;
			p.fond = R(NkRole::WindowBg);
			p.panneau = R(NkRole::PanelBg);
			p.entete = R(NkRole::PanelHeader);
			p.bord = R(NkRole::Border);
			p.champ = R(NkRole::InputBg);
			p.texte = R(NkRole::Text);
			p.attenue = R(NkRole::TextMuted);
			p.accent = R(NkRole::AccentUi);
			p.selection = R(NkRole::AccentSel);
			p.surAccent = R(NkRole::TextOnAccent);
			p.bouton = NkThemeUnpack(t.GetOuRepli(NkRole::ButtonBg, NkRole::PanelHeader));
			// La MEME regle que la conversion du kit (NkThemeVersGui) : un survol
			// est une fonction des roles, pas une couleur de plus.
			p.boutonSurvol = NkThemeMix(p.bouton, p.accent, 0.20f);
			p.danger = NkThemeUnpack(t.GetOuRepli(NkRole::StatusErr, NkRole::AccentSel));
			p.vert = NkThemeUnpack(t.GetOuRepli(NkRole::StatusOk, NkRole::AccentUi));
			p.axeX = R(NkRole::AxisX);
			p.axeY = R(NkRole::AxisY);
			p.axeZ = R(NkRole::AxisZ);
			return p;
		}

		// =====================================================================
		// LE TEXTE
		// =====================================================================
		float32 NkFamilleLargeur(const nkgui::NkGuiFont *f, const char *s) noexcept {
			return (f != nullptr && s != nullptr) ? f->MeasureWidth(s) : 0.f;
		}

		float32 NkFamilleHauteurLigne(const nkgui::NkGuiFont *f, float32 secours) noexcept {
			return (f != nullptr && f->Face() != nullptr) ? f->LineHeight() : secours;
		}

		void NkFamilleTexte(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 x, float32 hautY, const char *s,
							const NkColor &c, float32 largeurMax) noexcept {
			if (f != nullptr && f->Face() != nullptr && s != nullptr) {
				dl.AddText(f->Face(), f->TexId(), math::NkVec2f(x, hautY + f->Ascent()), s, c, largeurMax);
			}
		}

		void NkFamilleTexteCentre(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 cx, float32 hautY,
								  const char *s, const NkColor &c) noexcept {
			NkFamilleTexte(dl, f, cx - NkFamilleLargeur(f, s) * 0.5f, hautY, s, c);
		}

		void NkFamilleTexteADroite(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 droite, float32 hautY,
								   const char *s, const NkColor &c) noexcept {
			NkFamilleTexte(dl, f, droite - NkFamilleLargeur(f, s), hautY, s, c);
		}

		void NkFamilleTexteDansBoite(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, const NkRect &boite,
									 const char *s, const NkColor &c) noexcept {
			if (f == nullptr || f->Face() == nullptr) {
				return;
			}
			const float32 hautY = boite.y + (boite.h - f->LineHeight()) * 0.5f;
			NkFamilleTexte(dl, f, boite.x + (boite.w - NkFamilleLargeur(f, s)) * 0.5f, hautY, s, c);
		}

		void NkFamilleTexteGras(nkgui::NkGuiDrawList &dl, const nkgui::NkGuiFont *f, float32 x, float32 hautY,
								const char *s, const NkColor &c) noexcept {
			NkFamilleTexte(dl, f, x, hautY, s, c);
			NkFamilleTexte(dl, f, x + 0.6f, hautY, s, c);
		}

		// =====================================================================
		// LES PETITES AIDES
		// =====================================================================
		bool NkFamilleDans(const NkRect &r, const NkVec2 &p) noexcept {
			return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
		}

		NkColor NkFamilleMelange(const NkColor &a, const NkColor &b, float32 t) noexcept {
			auto m = [t](uint8 x, uint8 y) {
				return static_cast<uint8>(static_cast<float32>(x) + (static_cast<float32>(y) - static_cast<float32>(x)) * t);
			};
			return NkColor{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
		}

		void NkFamilleChevron(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, const NkColor &c) noexcept {
			dl.AddTriangleFilled(NkVec2{cx - 3.5f, cy - 1.5f}, NkVec2{cx + 3.5f, cy - 1.5f}, NkVec2{cx, cy + 2.5f}, c);
		}

		void NkFamilleCoche(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, const NkColor &c) noexcept {
			dl.AddLine(NkVec2{cx - 4.f, cy}, NkVec2{cx - 1.f, cy + 3.f}, c, 1.6f);
			dl.AddLine(NkVec2{cx - 1.f, cy + 3.f}, NkVec2{cx + 4.5f, cy - 3.5f}, c, 1.6f);
		}

		void NkFamilleCroix(nkgui::NkGuiDrawList &dl, const NkRect &r, const NkColor &c, float32 marge,
							float32 epaisseur) noexcept {
			const float32 x0 = r.x + marge;
			const float32 y0 = r.y + marge;
			const float32 x1 = r.x + r.w - marge;
			const float32 y1 = r.y + r.h - marge;
			dl.AddLine(NkVec2{x0, y0}, NkVec2{x1, y1}, c, epaisseur);
			dl.AddLine(NkVec2{x1, y0}, NkVec2{x0, y1}, c, epaisseur);
		}

		NkComponentInput NkFamilleEntreeComposant(const nkgui::NkGuiContext &ctx) noexcept {
			// La meme traduction que NkEditorShell::DrawTabStrip : l'echelle vient
			// de la surface, les gestes de l'entree NKGui de CETTE trame.
			const nkgui::NkGuiInput &in = ctx.input;
			NkComponentInput ci;
			ci.surfaceScale = ctx.scale;
			ci.mouseX = in.mousePos.x;
			ci.mouseY = in.mousePos.y;
			ci.wheel = in.wheel;
			ci.mouseDown = in.mouseDown[0];
			ci.mousePressed = in.mouseClicked[0];
			ci.mouseReleased = in.mouseReleased[0];
			ci.doubleClick = in.mouseDoubleClicked[0];
			ci.rightPressed = in.mouseClicked[1];
			ci.ctrl = in.ctrlDown;
			ci.shift = in.shiftDown;
			ci.alt = in.altDown;
			return ci;
		}

		// =====================================================================
		// LES WIDGETS DU CHROME
		// =====================================================================
		bool NkFamilleBouton(NkFamilleCtx &c, const NkRect &r, const char *texte, bool enfonce, bool actif,
							 nkgui::NkGuiDrawList *dl, const nkgui::NkGuiFont *police) {
			nkgui::NkGuiDrawList &liste = dl != nullptr ? *dl : c.ctx.dl;
			const nkgui::NkGuiInput &in = c.ctx.input;
			const bool survol = actif && NkFamilleDans(r, in.mousePos);
			NkColor fond = c.pal.bouton;
			if (enfonce) {
				fond = c.pal.accent;
			} else if (survol) {
				fond = c.pal.boutonSurvol;
			}
			liste.AddRectFilled(r, fond, 2.f);
			liste.AddRect(r, c.pal.bord, 1.f, 2.f);
			if (texte != nullptr && texte[0] != '\0') {
				NkColor t = c.pal.texte;
				if (!actif) {
					t = c.pal.attenue;
				} else if (enfonce) {
					t = c.pal.surAccent;
				}
				NkFamilleTexteDansBoite(liste, police != nullptr ? police : c.police, r, texte, t);
			}
			return survol && in.mouseClicked[0];
		}

		bool NkFamilleOnglets(NkFamilleCtx &c, const NkRect &bande, const char *const *noms, int32 n, int32 &actif) {
			auto &dl = c.ctx.dl;
			dl.AddRectFilled(bande, c.pal.entete);
			dl.AddRectFilled(NkRect{bande.x, bande.y + bande.h - 1.f, bande.w, 1.f}, c.pal.bord);
			bool change = false;
			float32 x = bande.x;
			for (int32 i = 0; i < n; ++i) {
				const float32 w = NkFamilleLargeur(c.police, noms[i]) + 24.f;
				const NkRect r{x, bande.y, w, bande.h};
				const bool survol = NkFamilleDans(r, c.ctx.input.mousePos);
				if (i == actif) {
					// L'onglet actif prend la couleur du panneau qu'il ouvre, et
					// un filet d'accent : c'est ainsi qu'UE5 le distingue.
					dl.AddRectFilled(r, c.pal.panneau);
					dl.AddRectFilled(NkRect{r.x, r.y, r.w, 2.f}, c.pal.accent);
				} else if (survol) {
					dl.AddRectFilled(r, c.pal.boutonSurvol);
				}
				NkFamilleTexteDansBoite(dl, c.police, r, noms[i], i == actif ? c.pal.texte : c.pal.attenue);
				if (survol && c.ctx.input.mouseClicked[0] && i != actif) {
					actif = i;
					change = true;
				}
				x += w;
			}
			return change;
		}

		void NkFamilleLoupe(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, const NkColor &c) noexcept {
			dl.AddCircle(NkVec2{cx - 1.f, cy - 1.f}, 4.5f, c, 1.4f);
			dl.AddLine(NkVec2{cx + 2.f, cy + 2.f}, NkVec2{cx + 6.f, cy + 6.f}, c, 1.6f);
		}

		void NkFamilleRecherche(NkFamilleCtx &c, const NkRect &r, char *tampon, int32 capacite, bool &focus,
								const char *attente, bool loupe, const nkgui::NkGuiFont *police) {
			const nkgui::NkGuiInput &in = c.ctx.input;
			auto &dl = c.ctx.dl;
			if (in.mouseClicked[0]) {
				focus = NkFamilleDans(r, in.mousePos);
			}
			if (focus && in.KeyPressed(nkgui::NkGuiKey::Escape)) {
				if (tampon != nullptr && capacite > 0) {
					tampon[0] = '\0';
				}
				focus = false;
			}
			if (focus && in.KeyPressed(nkgui::NkGuiKey::Enter)) {
				focus = false;
			}
			dl.AddRectFilled(r, c.pal.champ, 3.f);
			dl.AddRect(r, focus ? c.pal.accent : c.pal.bord, 1.f, 3.f);
			const float32 gauche = loupe ? 22.f : 6.f;
			if (loupe) {
				NkFamilleLoupe(dl, r.x + 12.f, r.y + r.h * 0.5f, c.pal.attenue);
			}
			NkOverlayFieldStyle st;
			st.fond = false;
			st.bord = false;
			st.texte = c.pal.texte;
			st.utf8 = true;
			const NkRect champ{r.x + gauche, r.y, r.w - gauche - 4.f, r.h};
			if (tampon != nullptr && capacite > 0) {
				NkOverlayTextField(c.ctx, dl, police, champ, tampon, capacite, focus, &st);
				if (tampon[0] == '\0' && !focus && attente != nullptr) {
					const float32 ty = r.y + (r.h - NkFamilleHauteurLigne(police, 12.f)) * 0.5f;
					NkFamilleTexte(dl, police, champ.x + 2.f, ty, attente, c.pal.attenue);
				}
			}
		}

	} // namespace editorkit
} // namespace nkentseu
