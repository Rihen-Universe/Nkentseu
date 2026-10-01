// -----------------------------------------------------------------------------
// @File    NkFamilleVue.cpp
// @Brief   La colonne centrale d'Unreal 5 de la famille (voir NkFamilleVue.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleVue.h"

#include <cmath>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		int32 NkFamilleBarreVue(NkFamilleCtx &c, const NkRect &b, const NkFamilleBoutonVue *boutons, int32 n,
								const char *reperes) {
			auto &dl = c.ctx.dl;
			dl.AddRectFilled(b, c.pal.entete);
			dl.AddRectFilled(NkRect{b.x, b.y + b.h - 1.f, b.w, 1.f}, c.pal.bord);
			float32 x = b.x + 6.f;
			int32 clique = -1;
			for (int32 i = 0; i < n; ++i) {
				const float32 w = NkFamilleLargeur(c.petite, boutons[i].texte) + 16.f;
				const NkRect r{x, b.y + 4.f, w, b.h - 8.f};
				if (NkFamilleBouton(c, r, "", boutons[i].enfonce)) {
					clique = i;
				}
				NkFamilleTexteDansBoite(dl, c.petite, r, boutons[i].texte, boutons[i].enfonce ? c.pal.surAccent : c.pal.texte);
				x += w + 3.f;
			}
			if (reperes != nullptr && reperes[0] != '\0') {
				const float32 ty = b.y + (b.h - NkFamilleHauteurLigne(c.petite, 12.f)) * 0.5f;
				if (NkFamilleLargeur(c.petite, reperes) < b.x + b.w - x - 16.f) {
					NkFamilleTexteADroite(dl, c.petite, b.x + b.w - 8.f, ty, reperes, c.pal.attenue);
				}
			}
			return clique;
		}

		void NkFamillePeindreIconeBarre(nkgui::NkGuiDrawList &dl, NkFamilleIconeBarre ic, const NkRect &r,
										const NkColor &col) noexcept {
			const float32 cx = r.x + r.w * 0.5f;
			const float32 cy = r.y + r.h * 0.5f;
			auto P = [](float32 x, float32 y) { return NkVec2{x, y}; };
			auto Fleche = [&](float32 x, float32 y, float32 dx, float32 dy) {
				const float32 px = -dy;
				const float32 py = dx;
				dl.AddTriangleFilled(P(x + dx * 3.5f, y + dy * 3.5f), P(x + px * 3.f, y + py * 3.f), P(x - px * 3.f, y - py * 3.f), col);
			};
			switch (ic) {
				case NkFamilleIconeBarre::Selection: {
					const NkVec2 pts[7] = {P(cx - 4.f, cy - 7.f), P(cx - 4.f, cy + 5.f), P(cx - 1.f, cy + 2.f), P(cx + 1.5f, cy + 7.f),
										   P(cx + 3.2f, cy + 6.2f), P(cx + 0.8f, cy + 1.5f), P(cx + 5.f, cy + 1.5f)};
					dl.AddTriangleFilled(pts[0], pts[1], pts[6], col);
					dl.AddPolyline(pts, 7, col, 1.2f, true);
					break;
				}
				case NkFamilleIconeBarre::Deplacer:
					dl.AddLine(P(cx - 6.f, cy), P(cx + 6.f, cy), col, 1.4f);
					dl.AddLine(P(cx, cy - 6.f), P(cx, cy + 6.f), col, 1.4f);
					Fleche(cx + 5.5f, cy, 1.f, 0.f);
					Fleche(cx - 5.5f, cy, -1.f, 0.f);
					Fleche(cx, cy - 5.5f, 0.f, -1.f);
					Fleche(cx, cy + 5.5f, 0.f, 1.f);
					break;
				case NkFamilleIconeBarre::Tourner: {
					NkVec2 arc[16];
					for (int32 k = 0; k < 16; ++k) {
						const float32 a = 0.35f + 4.7f * static_cast<float32>(k) / 15.f;
						arc[k] = P(cx + 6.f * std::cos(a), cy - 6.f * std::sin(a));
					}
					dl.AddPolyline(arc, 16, col, 1.4f, false);
					const float32 af = 0.35f + 4.7f;
					Fleche(arc[15].x, arc[15].y, -std::sin(af), -std::cos(af));
					break;
				}
				case NkFamilleIconeBarre::Echelle:
					dl.AddRect(NkRect{cx - 6.f, cy - 6.f, 12.f, 12.f}, col, 1.2f);
					dl.AddRectFilled(NkRect{cx - 6.f, cy + 1.f, 5.f, 5.f}, col);
					dl.AddLine(P(cx - 1.f, cy + 1.f), P(cx + 4.f, cy - 4.f), col, 1.3f);
					Fleche(cx + 4.f, cy - 4.f, 0.7071f, -0.7071f);
					break;
				case NkFamilleIconeBarre::Monde: {
					dl.AddCircle(P(cx, cy), 6.5f, col, 1.2f, 20);
					NkVec2 meridien[16];
					for (int32 k = 0; k < 16; ++k) {
						const float32 a = 6.2831853f * static_cast<float32>(k) / 16.f;
						meridien[k] = P(cx + 2.8f * std::cos(a), cy + 6.5f * std::sin(a));
					}
					dl.AddPolyline(meridien, 16, col, 1.1f, true);
					dl.AddLine(P(cx - 6.5f, cy), P(cx + 6.5f, cy), col, 1.1f);
					break;
				}
				case NkFamilleIconeBarre::Local: {
					const float32 a = 0.5f;
					const float32 ux = std::cos(a), uy = -std::sin(a);
					const NkVec2 o = P(cx - 4.f, cy + 4.f);
					dl.AddLine(o, P(o.x + ux * 10.f, o.y + uy * 10.f), col, 1.4f);
					dl.AddLine(o, P(o.x + uy * 10.f, o.y - ux * 10.f), col, 1.4f);
					Fleche(o.x + ux * 10.f, o.y + uy * 10.f, ux, uy);
					Fleche(o.x + uy * 10.f, o.y - ux * 10.f, uy, -ux);
					break;
				}
				case NkFamilleIconeBarre::Aimant: {
					NkVec2 arc[9];
					for (int32 k = 0; k < 9; ++k) {
						const float32 a = 3.14159265f * static_cast<float32>(k) / 8.f;
						arc[k] = P(cx - 5.f * std::cos(a), cy + 1.f - 5.f * std::sin(a));
					}
					dl.AddPolyline(arc, 9, col, 2.6f, false);
					dl.AddLine(P(cx - 5.f, cy + 1.f), P(cx - 5.f, cy + 6.f), col, 2.6f);
					dl.AddLine(P(cx + 5.f, cy + 1.f), P(cx + 5.f, cy + 6.f), col, 2.6f);
					break;
				}
				case NkFamilleIconeBarre::Grille:
					for (int32 k = 0; k < 4; ++k) {
						const float32 t = -6.f + 4.f * static_cast<float32>(k);
						dl.AddLine(P(cx + t, cy - 6.f), P(cx + t, cy + 6.f), col, 1.f);
						dl.AddLine(P(cx - 6.f, cy + t), P(cx + 6.f, cy + t), col, 1.f);
					}
					break;
				case NkFamilleIconeBarre::Angle: {
					const NkVec2 o = P(cx - 6.f, cy + 5.f);
					dl.AddLine(o, P(cx + 7.f, cy + 5.f), col, 1.4f);
					dl.AddLine(o, P(cx + 3.f, cy - 6.f), col, 1.4f);
					NkVec2 arc[8];
					for (int32 k = 0; k < 8; ++k) {
						const float32 a = 1.22f * static_cast<float32>(k) / 7.f;
						arc[k] = P(o.x + 7.f * std::cos(a), o.y - 7.f * std::sin(a));
					}
					dl.AddPolyline(arc, 8, col, 1.1f, false);
					break;
				}
				case NkFamilleIconeBarre::PasEchelle:
					dl.AddLine(P(cx - 5.f, cy + 5.f), P(cx + 5.f, cy - 5.f), col, 1.4f);
					Fleche(cx + 5.f, cy - 5.f, 0.7071f, -0.7071f);
					Fleche(cx - 5.f, cy + 5.f, -0.7071f, 0.7071f);
					dl.AddLine(P(cx - 6.5f, cy - 6.5f), P(cx - 1.5f, cy - 6.5f), col, 1.1f);
					dl.AddLine(P(cx - 6.5f, cy - 6.5f), P(cx - 6.5f, cy - 1.5f), col, 1.1f);
					dl.AddLine(P(cx + 6.5f, cy + 6.5f), P(cx + 1.5f, cy + 6.5f), col, 1.1f);
					dl.AddLine(P(cx + 6.5f, cy + 6.5f), P(cx + 6.5f, cy + 1.5f), col, 1.1f);
					break;
				case NkFamilleIconeBarre::Camera:
					dl.AddRect(NkRect{cx - 7.f, cy - 4.f, 9.f, 8.f}, col, 1.2f, 1.f);
					dl.AddTriangleFilled(P(cx + 2.f, cy), P(cx + 7.f, cy - 4.f), P(cx + 7.f, cy + 4.f), col);
					break;
				case NkFamilleIconeBarre::Eclaire:
					for (int32 k = 0; k < 8; ++k) {
						const float32 a = 0.785398f * static_cast<float32>(k);
						dl.AddLine(P(cx + std::cos(a) * 4.f, cy + std::sin(a) * 4.f), P(cx + std::cos(a) * 7.f, cy + std::sin(a) * 7.f),
								   col, 1.2f);
					}
					dl.AddCircleFilled(P(cx, cy), 3.f, col);
					break;
			}
		}

		NkFamilleBarreFlottanteResultat NkFamilleBarreFlottante(NkFamilleCtx &c, const NkRect &aire,
																const NkFamilleElementBarre *elements, int32 n,
																bool menuOuvert) {
			NkFamilleBarreFlottanteResultat res;
			auto &dl = c.ctx.dl;
			const nkgui::NkGuiInput &in = c.ctx.input;
			constexpr float32 H = 28.f;	 ///< la barre
			constexpr float32 B = 24.f;	 ///< un bouton d'icone
			constexpr float32 ECART = 2.f;
			constexpr float32 SEP = 9.f; ///< entre deux groupes
			constexpr int32 MAX = 24;
			if (n <= 0 || n > MAX) {
				return res;
			}
			// La largeur d'abord : la barre est calee sur le bord DROIT du viseur.
			float32 largeurs[MAX];
			float32 total = 4.f;
			for (int32 i = 0; i < n; ++i) {
				largeurs[i] = elements[i].texte.Empty() ? B : NkFamilleLargeur(c.petite, elements[i].texte.CStr()) + 20.f;
				total += (elements[i].separateur ? SEP : (i > 0 ? ECART : 0.f)) + largeurs[i];
			}
			total += 4.f;
			const NkRect barre{aire.x + aire.w - total - 8.f, aire.y + 8.f, total, H};
			// Trop etroit : la barre se retire plutot que de couvrir la scene.
			if (barre.x < aire.x + 8.f || aire.h < H + 16.f) {
				return res;
			}
			res.barre = barre;
			NkColor fond = c.pal.entete;
			fond.a = 235;
			dl.AddRectFilled(NkRect{barre.x + 2.f, barre.y + 3.f, barre.w, barre.h}, NkColor{0, 0, 0, 70}, 5.f);
			dl.AddRectFilled(barre, fond, 5.f);
			dl.AddRect(barre, c.pal.bord, 1.f, 5.f);

			float32 x = barre.x + 4.f;
			int32 bulle = -1;
			NkRect rBulle{0.f, 0.f, 0.f, 0.f};
			for (int32 i = 0; i < n; ++i) {
				const NkFamilleElementBarre &e = elements[i];
				if (e.separateur) {
					dl.AddRectFilled(NkRect{x + SEP * 0.5f - 0.5f, barre.y + 6.f, 1.f, H - 12.f}, c.pal.bord);
					x += SEP;
				} else if (i > 0) {
					x += ECART;
				}
				const NkRect r{x, barre.y + (H - B) * 0.5f, largeurs[i], B};
				x += largeurs[i];
				const bool clic = NkFamilleBouton(c, r, "", e.enfonce);
				const NkColor col = e.enfonce ? c.pal.surAccent : (e.eteint ? c.pal.attenue : c.pal.texte);
				if (e.texte.Empty()) {
					NkFamillePeindreIconeBarre(dl, e.icone, r, col);
				} else {
					const NkRect rt{r.x, r.y, r.w - 8.f, r.h};
					NkFamilleTexteDansBoite(dl, c.petite, rt, e.texte.CStr(), col);
					const float32 fx = r.x + r.w - 8.f;
					const float32 fy = r.y + r.h * 0.5f;
					dl.AddTriangleFilled(NkVec2{fx - 3.f, fy - 1.5f}, NkVec2{fx + 3.f, fy - 1.5f}, NkVec2{fx, fy + 2.f}, col);
				}
				if (NkFamilleDans(r, in.mousePos)) {
					bulle = i;
					rBulle = r;
				}
				if (clic) {
					res.clic = i;
					res.rect = r;
				}
			}
			// L'infobulle, SOUS la barre : ce que fait le bouton survole.
			if (bulle >= 0 && !menuOuvert && !elements[bulle].bulle.Empty()) {
				const char *t = elements[bulle].bulle.CStr();
				const float32 tw = NkFamilleLargeur(c.petite, t) + 12.f;
				const float32 th = NkFamilleHauteurLigne(c.petite, 12.f) + 6.f;
				float32 bx = rBulle.x + rBulle.w * 0.5f - tw * 0.5f;
				bx = bx + tw > aire.x + aire.w - 4.f ? aire.x + aire.w - 4.f - tw : bx;
				const NkRect rb{bx, barre.y + barre.h + 4.f, tw, th};
				dl.AddRectFilled(rb, NkColor{16, 18, 24, 235}, 3.f);
				dl.AddRect(rb, c.pal.bord, 1.f, 3.f);
				NkFamilleTexteDansBoite(dl, c.petite, rb, t, c.pal.texte);
			}
			return res;
		}

		void NkFamilleRepereAxes(NkFamilleCtx &c, nkgui::NkGuiDrawList &dl, const NkRect &aire, const NkVec2 *axes,
								 int32 nbAxes) {
			const NkVec2 o{aire.x + 30.f, aire.y + aire.h - 30.f};
			const NkColor couleurs[3] = {c.pal.axeX, c.pal.axeY, c.pal.axeZ};
			static const char *const kNoms[3] = {"X", "Y", "Z"};
			NkColor fond = c.pal.fond;
			fond.a = 150;
			dl.AddCircleFilled(NkVec2{o.x + 6.f, o.y - 6.f}, 26.f, fond, 32);
			// Le plus « loin » d'abord : un axe qui pointe vers l'ecran passe devant.
			for (int32 k = 0; k < nbAxes && k < 3; ++k) {
				const NkVec2 d = axes[k];
				const float32 l = std::sqrt(d.x * d.x + d.y * d.y);
				const NkColor col = couleurs[k];
				if (l < 0.15f) {
					dl.AddCircleFilled(o, 3.f, col);
					continue;
				}
				const NkVec2 u{d.x / l, d.y / l};
				const float32 L = 24.f * (l > 1.f ? 1.f : l);
				const NkVec2 bout{o.x + u.x * L, o.y + u.y * L};
				dl.AddLine(o, bout, col, 2.f);
				const NkVec2 pointe{o.x + u.x * (L + 6.f), o.y + u.y * (L + 6.f)};
				const NkVec2 n{-u.y, u.x};
				dl.AddTriangleFilled(pointe, NkVec2{bout.x + n.x * 4.f, bout.y + n.y * 4.f},
									 NkVec2{bout.x - n.x * 4.f, bout.y - n.y * 4.f}, col);
				NkFamilleTexte(dl, c.petite, pointe.x + u.x * 4.f - 3.f, pointe.y + u.y * 4.f - 6.f, kNoms[k], col);
			}
			dl.AddCircleFilled(o, 2.5f, c.pal.texte);
		}

	} // namespace editorkit
} // namespace nkentseu
