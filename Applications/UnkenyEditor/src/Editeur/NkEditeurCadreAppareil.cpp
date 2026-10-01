// -----------------------------------------------------------------------------
// FICHIER: Editeur/NkEditeurCadreAppareil.cpp
// DESCRIPTION: Le CADRE de l'appareil simule, dessine en vecteurs (document 03,
//              §2.4) : bordure, coins, boutons, decoupe de camera, zone sure,
//              en version sombre et claire.
//
// ⚠️ TOUT EST DECRIT EN PORTRAIT, PUIS TOURNE
//   Bordure, boutons, decoupe, pied d'ecran : chaque point est ecrit dans le
//   repere de l'appareil EN PORTRAIT (points, origine au coin haut gauche de
//   l'ecran), puis passe par NkTournerPoint. C'est ce qui fait tourner le cadre
//   GEOMETRIQUEMENT avec l'appareil (les boutons de volume passent en haut en
//   paysage gauche...) au lieu de le redessiner par orientation.
//
// ⚠️ AUCUNE IMAGE : des formes. Ni droits sur une photo d'appareil, ni flou a
//   l'agrandissement, et le theme clair n'est qu'une autre palette.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Editeur/NkEditeurViseur.h"

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {

			/// Ce que le cadre ajoute autour de l'ecran, EN PORTRAIT, en points.
			struct NkDebords {
					float32 haut = 0.f;
					float32 droite = 0.f;
					float32 bas = 0.f;
					float32 gauche = 0.f;
			};

			constexpr float32 kSaillieBouton = 3.f; ///< un bouton depasse de la tranche
			constexpr float32 kTranche = 2.5f;		///< la bande de metal autour du verre

			/// La bordure par defaut d'une famille qui n'en precise pas.
			void Bordures(const NkProfilAppareil &p, float32 &cote, float32 &hautBas) {
				cote = p.bordure;
				hautBas = p.bordureHautBas;
				if (cote <= 0.f && hautBas <= 0.f) {
					switch (p.famille) {
						case NkFamilleAppareil::NK_BUREAU:
							cote = hautBas = 16.f;
							break;
						case NkFamilleAppareil::NK_TV:
							cote = hautBas = 9.f;
							break;
						case NkFamilleAppareil::NK_CONSOLE:
							cote = hautBas = 14.f;
							break;
						case NkFamilleAppareil::NK_NAVIGATEUR:
							cote = hautBas = 1.f;
							break;
						default:
							cote = hautBas = 10.f;
							break;
					}
				}
			}

			float32 HauteurBarreNavigateur() {
				return 38.f;
			}

			/// Largeur d'une poignee de console (portrait = sens naturel, ici paysage).
			float32 LargeurPoignee(const NkProfilAppareil &p) {
				return static_cast<float32>(p.hauteur) * 0.42f;
			}

			NkDebords Debords(const NkProfilAppareil &p) {
				float32 cote = 0.f;
				float32 hautBas = 0.f;
				Bordures(p, cote, hautBas);
				NkDebords d;
				d.haut = d.bas = hautBas + kTranche;
				d.gauche = d.droite = cote + kTranche;
				for (uint8 i = 0; i < p.nbBoutons; ++i) {
					float32 *b[4] = {&d.haut, &d.droite, &d.bas, &d.gauche};
					*b[p.boutons[i].cote & 3] = (p.boutons[i].cote & 1 ? cote : hautBas) + kTranche + kSaillieBouton;
				}
				const float32 h = static_cast<float32>(p.hauteur);
				switch (p.famille) {
					case NkFamilleAppareil::NK_NAVIGATEUR:
						d.haut += HauteurBarreNavigateur();
						break;
					case NkFamilleAppareil::NK_BUREAU:
						d.bas += h * 0.16f; // le pied
						break;
					case NkFamilleAppareil::NK_TV:
						d.bas += h * 0.06f;
						break;
					case NkFamilleAppareil::NK_CONSOLE:
						d.gauche += LargeurPoignee(p);
						d.droite += LargeurPoignee(p);
						break;
					case NkFamilleAppareil::NK_MONTRE:
						d.haut += h * 0.30f; // les bracelets
						d.bas += h * 0.30f;
						break;
					default:
						break;
				}
				return d;
			}

			/// Le passage PORTRAIT -> VISEUR : rotation, echelle, translation.
			struct NkRepere {
					NkOrientation o = NkOrientation::NK_PORTRAIT;
					float32 l = 1.f; ///< largeur de l'ecran en portrait (points)
					float32 h = 1.f;
					float32 s = 1.f; ///< pixels du viseur par point
					NkVec2 origine{0.f, 0.f};

					NkVec2 V(float32 x, float32 y) const {
						float32 rx = 0.f;
						float32 ry = 0.f;
						NkTournerPoint(o, l, h, x, y, rx, ry);
						return NkVec2(origine.x + rx * s, origine.y + ry * s);
					}
					/// Un rectangle droit du portrait, droit aussi dans le viseur
					/// (les rotations sont des quarts de tour).
					NkRect R(float32 x, float32 y, float32 w, float32 hh) const {
						const NkVec2 a = V(x, y);
						const NkVec2 b = V(x + w, y + hh);
						const float32 x0 = a.x < b.x ? a.x : b.x;
						const float32 y0 = a.y < b.y ? a.y : b.y;
						return NkRect{x0, y0, (a.x < b.x ? b.x - a.x : a.x - b.x), (a.y < b.y ? b.y - a.y : a.y - b.y)};
					}
			};

			constexpr int32 kSeg = 10;					   ///< segments par coin
			constexpr int32 kPtsContour = 4 * (kSeg + 1); ///< points d'un contour arrondi

			/// Contour d'un rectangle a coins arrondis (rayons propres a chaque
			/// coin : haut gauche, haut droit, bas droit, bas gauche), decrit en
			/// portrait, rendu dans le viseur. Sens horaire, depuis le haut gauche.
			void Contour(const NkRepere &rep, float32 x, float32 y, float32 w, float32 h, const float32 r[4], NkVec2 *out) {
				const float32 cx[4] = {x + r[0], x + w - r[1], x + w - r[2], x + r[3]};
				const float32 cy[4] = {y + r[0], y + r[1], y + h - r[2], y + h - r[3]};
				const float32 depart[4] = {3.14159265f, 4.71238898f, 0.f, 1.57079633f};
				int32 k = 0;
				for (int32 c = 0; c < 4; ++c) {
					for (int32 i = 0; i <= kSeg; ++i) {
						const float32 a = depart[c] + 1.57079633f * static_cast<float32>(i) / static_cast<float32>(kSeg);
						out[k++] = rep.V(cx[c] + math::NkCos(a) * r[c], cy[c] + math::NkSin(a) * r[c]);
					}
				}
			}

			void ContourUni(const NkRepere &rep, float32 x, float32 y, float32 w, float32 h, float32 r, NkVec2 *out) {
				const float32 m = (w < h ? w : h) * 0.5f;
				r = r < 0.f ? 0.f : (r > m ? m : r);
				const float32 rr[4] = {r, r, r, r};
				Contour(rep, x, y, w, h, rr, out);
			}

			/// L'anneau entre deux contours de meme nombre de points.
			void Anneau(nkgui::NkGuiDrawList &dl, const NkVec2 *dedans, const NkVec2 *dehors, int32 n, const NkColor &c) {
				for (int32 i = 0; i < n; ++i) {
					const int32 j = (i + 1) % n;
					dl.AddTriangleFilled(dedans[i], dedans[j], dehors[j], c);
					dl.AddTriangleFilled(dedans[i], dehors[j], dehors[i], c);
				}
			}

			/// Le verre (la bordure noire, ou blanche en clair), la bande de metal
			/// et son reflet : ce qui fait lire « un appareil » et non « un cadre ».
			struct NkPalette {
					NkColor corps, metal, reflet1, tranche, filet, bouton, boutonOmbre, decoupe, lentille, reflet, voile,
						accent;
			};

			NkPalette Palette(bool clair) {
				NkPalette p;
				if (clair) {
					p.corps = NkColor(246, 246, 248);
					p.metal = NkColor(194, 198, 207);
					p.reflet1 = NkColor(232, 235, 240);
					p.tranche = NkColor(150, 156, 168);
					p.filet = NkColor(206, 208, 214);
					p.bouton = NkColor(214, 217, 224);
					p.boutonOmbre = NkColor(168, 173, 184);
					p.accent = NkColor(236, 237, 241);
				} else {
					p.corps = NkColor(11, 11, 14);
					p.metal = NkColor(88, 94, 108);
					p.reflet1 = NkColor(146, 152, 166);
					p.tranche = NkColor(70, 75, 88);
					p.filet = NkColor(2, 2, 3);
					p.bouton = NkColor(110, 116, 130);
					p.boutonOmbre = NkColor(64, 68, 80);
					p.accent = NkColor(30, 32, 38);
				}
				p.decoupe = NkColor(4, 4, 6);
				p.lentille = NkColor(20, 28, 52);
				p.reflet = NkColor(110, 130, 190, 170);
				p.voile = NkColor(10, 12, 18, 120);
				return p;
			}

			/// Une lentille de camera : un disque sombre et un reflet.
			void Lentille(nkgui::NkGuiDrawList &dl, const NkRepere &rep, const NkPalette &pal, float32 x, float32 y, float32 r) {
				const NkVec2 c = rep.V(x, y);
				const float32 rp = r * rep.s;
				if (rp < 0.8f) {
					return;
				}
				dl.AddCircleFilled(c, rp, pal.lentille);
				dl.AddCircleFilled(NkVec2(c.x - rp * 0.3f, c.y - rp * 0.3f), rp * 0.32f, pal.reflet);
			}

			/// La decoupe de camera, en portrait, accrochee au bord physique haut.
			void Decoupe(nkgui::NkGuiDrawList &dl, const NkRepere &rep, const NkPalette &pal, const NkProfilAppareil &b) {
				const NkRectAppareil &d = b.rectDecoupe;
				if (b.decoupe == NkTypeDecoupe::NK_AUCUNE || d.w <= 0.f || d.h <= 0.f) {
					return;
				}
				NkVec2 pts[kPtsContour];
				switch (b.decoupe) {
					case NkTypeDecoupe::NK_ENCOCHE: {
						const float32 r = d.h * 0.55f < d.w * 0.25f ? d.h * 0.55f : d.w * 0.25f;
						const float32 rr[4] = {0.f, 0.f, r, r};
						Contour(rep, d.x, d.y, d.w, d.h, rr, pts);
						dl.AddConvexPolyFilled(pts, kPtsContour, pal.decoupe);
						// Le haut-parleur, puis la camera.
						const NkRect hp = rep.R(d.x + d.w * 0.36f, d.y + d.h * 0.22f, d.w * 0.28f, 3.f);
						dl.AddRectFilled(hp, NkColor(30, 32, 38), 1.5f * rep.s);
						Lentille(dl, rep, pal, d.x + d.w * 0.74f, d.y + d.h * 0.42f, d.h * 0.17f);
						break;
					}
					case NkTypeDecoupe::NK_ILOT:
						ContourUni(rep, d.x, d.y, d.w, d.h, d.h * 0.5f, pts);
						dl.AddConvexPolyFilled(pts, kPtsContour, pal.decoupe);
						Lentille(dl, rep, pal, d.x + d.w - d.h * 0.5f, d.y + d.h * 0.5f, d.h * 0.24f);
						break;
					case NkTypeDecoupe::NK_POINCON: {
						const float32 r = (d.w < d.h ? d.w : d.h) * 0.5f;
						ContourUni(rep, d.x, d.y, d.w, d.h, r, pts);
						dl.AddConvexPolyFilled(pts, kPtsContour, pal.decoupe);
						Lentille(dl, rep, pal, d.x + d.w * 0.5f, d.y + d.h * 0.5f, r * 0.55f);
						break;
					}
					case NkTypeDecoupe::NK_GOUTTE: {
						const float32 r = d.w * 0.5f < d.h ? d.w * 0.5f : d.h;
						const float32 rr[4] = {0.f, 0.f, r, r};
						Contour(rep, d.x, d.y, d.w, d.h, rr, pts);
						dl.AddConvexPolyFilled(pts, kPtsContour, pal.decoupe);
						Lentille(dl, rep, pal, d.x + d.w * 0.5f, d.y + d.h * 0.52f, d.w * 0.2f);
						break;
					}
					default:
						break;
				}
			}

			/// Un trait tirete (marge CONSEILLEE : elle n'est pas rendue par le
			/// systeme, elle ne doit pas ressembler a la zone sure).
			void Tirets(nkgui::NkGuiDrawList &dl, NkVec2 a, NkVec2 b, const NkColor &c) {
				const float32 dx = b.x - a.x;
				const float32 dy = b.y - a.y;
				const float32 l = math::NkSqrt(dx * dx + dy * dy);
				if (l < 1.f) {
					return;
				}
				const int32 n = static_cast<int32>(l / 8.f);
				for (int32 i = 0; i < n; i += 2) {
					const float32 t0 = static_cast<float32>(i) / static_cast<float32>(n);
					const float32 t1 = static_cast<float32>(i + 1) / static_cast<float32>(n);
					dl.AddLine(NkVec2(a.x + dx * t0, a.y + dy * t0), NkVec2(a.x + dx * t1, a.y + dy * t1), c, 1.f);
				}
			}

			void RectTirete(nkgui::NkGuiDrawList &dl, const NkRect &r, const NkColor &c) {
				Tirets(dl, NkVec2(r.x, r.y), NkVec2(r.x + r.w, r.y), c);
				Tirets(dl, NkVec2(r.x + r.w, r.y), NkVec2(r.x + r.w, r.y + r.h), c);
				Tirets(dl, NkVec2(r.x + r.w, r.y + r.h), NkVec2(r.x, r.y + r.h), c);
				Tirets(dl, NkVec2(r.x, r.y + r.h), NkVec2(r.x, r.y), c);
			}

		} // namespace

		nkgui::NkRect NkAireAppareil(const nkgui::NkRect &viseur, const NkProfilAppareil &profil, bool avecCadre) noexcept {
			const float32 l = static_cast<float32>(profil.largeur);
			const float32 h = static_cast<float32>(profil.hauteur);
			// Les debords du cadre, tournes avec l'appareil : en paysage gauche,
			// la bordure HAUTE du portrait est a gauche.
			float32 m[4] = {0.f, 0.f, 0.f, 0.f}; // haut, droite, bas, gauche -- du viseur
			if (avecCadre) {
				const NkProfilAppareil base = NkOrienter(profil, NkOrientation::NK_PORTRAIT);
				const NkDebords d = Debords(base);
				const float32 portrait[4] = {d.haut, d.droite, d.bas, d.gauche};
				for (int32 b = 0; b < 4; ++b) {
					m[NkBordTourne(profil.orientation, b)] = portrait[b];
				}
			}
			const float32 marge = 24.f;
			const float32 totalL = l + m[1] + m[3];
			const float32 totalH = h + m[0] + m[2];
			float32 s = (viseur.w - marge * 2.f) / totalL;
			const float32 sh = (viseur.h - marge * 2.f) / totalH;
			s = sh < s ? sh : s;
			float32 aw = l * s;
			float32 ah = h * s;
			// ⚠️ Un panneau peut etre reduit a presque rien par l'ancrage. Sans
			// ce plancher, l'aire devient negative, la camera recoit un viseur
			// vide, et la division par sa largeur rend des coordonnees infinies
			// -- une panne qui sort loin d'ici, dans la conversion ecran/monde.
			if (aw < 1.f || ah < 1.f || s <= 0.f) {
				aw = aw < 1.f ? 1.f : aw;
				ah = ah < 1.f ? 1.f : ah;
				s = 0.f;
			}
			// L'ensemble (ecran + cadre) est centre ; l'ecran y est decale de ses
			// debords.
			const float32 x0 = viseur.x + (viseur.w - totalL * s) * 0.5f + m[3] * s;
			const float32 y0 = viseur.y + (viseur.h - totalH * s) * 0.5f + m[0] * s;
			return nkgui::NkRect{s > 0.f ? x0 : viseur.x + (viseur.w - aw) * 0.5f, s > 0.f ? y0 : viseur.y + (viseur.h - ah) * 0.5f,
								 aw, ah};
		}

		void NkDessinerAppareil(nkgui::NkGuiDrawList &dl, const NkProfilAppareil &p, const nkgui::NkRect &ecran,
								const NkReglagesAppareil &r, const nkgui::NkRect &viseur) {
			if (ecran.w < 2.f || ecran.h < 2.f || p.largeur == 0u || p.hauteur == 0u) {
				return;
			}
			const NkProfilAppareil b = NkOrienter(p, NkOrientation::NK_PORTRAIT); // la description en portrait
			NkRepere rep;
			rep.o = p.orientation;
			rep.l = static_cast<float32>(b.largeur);
			rep.h = static_cast<float32>(b.hauteur);
			rep.s = ecran.w / static_cast<float32>(p.largeur);
			rep.origine = NkVec2(ecran.x, ecran.y);
			const NkPalette pal = Palette(r.cadreClair);
			const float32 L = rep.l;
			const float32 H = rep.h;
			const float32 rayon = b.rond ? (L < H ? L : H) * 0.5f : b.rayonCoins;

			NkVec2 ecranPts[kPtsContour];
			ContourUni(rep, 0.f, 0.f, L, H, rayon, ecranPts);

			if (r.voirCadre) {
				float32 cote = 0.f;
				float32 hautBas = 0.f;
				Bordures(b, cote, hautBas);
				const bool navigateur = b.famille == NkFamilleAppareil::NK_NAVIGATEUR;
				const float32 barre = navigateur ? HauteurBarreNavigateur() : 0.f;
				// Le corps : l'ecran agrandi de ses bordures, coins concentriques.
				const float32 cx = -cote;
				const float32 cy = -hautBas - barre;
				const float32 cw = L + cote * 2.f;
				const float32 ch = H + hautBas * 2.f + barre;
				const float32 rCorps = b.rond ? (cw < ch ? cw : ch) * 0.5f : (navigateur ? 8.f : rayon + (cote < hautBas ? cote : hautBas));

				// 0. Ce qui est HORS de l'appareil s'efface un peu : l'appareil se
				//    detache, le monde reste lisible (il remplit toujours le viseur).
				NkVec2 corps[kPtsContour];
				ContourUni(rep, cx, cy, cw, ch, rCorps, corps);
				const float32 tr = navigateur ? 1.f : kTranche;
				NkVec2 metal[kPtsContour];
				ContourUni(rep, cx - tr, cy - tr, cw + tr * 2.f, ch + tr * 2.f, rCorps + tr, metal);
				{
					NkVec2 boite[kPtsContour];
					ContourUni(rep, cx - tr, cy - tr, cw + tr * 2.f, ch + tr * 2.f, 0.f, boite);
					Anneau(dl, metal, boite, kPtsContour, pal.voile);
					const NkRect bb = rep.R(cx - tr, cy - tr, cw + tr * 2.f, ch + tr * 2.f);
					dl.AddRectFilled(NkRect{viseur.x, viseur.y, viseur.w, bb.y - viseur.y}, pal.voile);
					dl.AddRectFilled(NkRect{viseur.x, bb.y + bb.h, viseur.w, viseur.y + viseur.h - bb.y - bb.h}, pal.voile);
					dl.AddRectFilled(NkRect{viseur.x, bb.y, bb.x - viseur.x, bb.h}, pal.voile);
					dl.AddRectFilled(NkRect{bb.x + bb.w, bb.y, viseur.x + viseur.w - bb.x - bb.w, bb.h}, pal.voile);
				}

				// 1. Les accessoires DERRIERE le corps : pied, poignees, bracelets.
				switch (b.famille) {
					case NkFamilleAppareil::NK_BUREAU: {
						const NkRect cou = rep.R(L * 0.45f, H + hautBas, L * 0.10f, H * 0.12f);
						dl.AddRectFilled(cou, pal.boutonOmbre);
						const NkRect socle = rep.R(L * 0.33f, H + hautBas + H * 0.12f, L * 0.34f, H * 0.035f);
						dl.AddRectFilled(socle, pal.bouton, 3.f * rep.s);
						break;
					}
					case NkFamilleAppareil::NK_TV:
						dl.AddRectFilled(rep.R(L * 0.12f, H + hautBas, L * 0.05f, H * 0.055f), pal.boutonOmbre, 2.f * rep.s);
						dl.AddRectFilled(rep.R(L * 0.83f, H + hautBas, L * 0.05f, H * 0.055f), pal.boutonOmbre, 2.f * rep.s);
						break;
					case NkFamilleAppareil::NK_CONSOLE: {
						const float32 g = LargeurPoignee(b);
						const NkColor poignee = r.cadreClair ? NkColor(206, 209, 216) : NkColor(42, 45, 54);
						dl.AddRectFilled(rep.R(cx - g, cy, g + 8.f, ch), poignee, g * 0.45f * rep.s);
						dl.AddRectFilled(rep.R(cx + cw - 8.f, cy, g + 8.f, ch), poignee, g * 0.45f * rep.s);
						// Les sticks, la croix, les quatre boutons.
						const NkColor fonce = r.cadreClair ? NkColor(120, 126, 138) : NkColor(18, 19, 24);
						dl.AddCircleFilled(rep.V(cx - g * 0.5f, cy + ch * 0.28f), g * 0.22f * rep.s, fonce);
						dl.AddCircleFilled(rep.V(cx + cw + g * 0.5f, cy + ch * 0.62f), g * 0.22f * rep.s, fonce);
						for (int32 k = 0; k < 4; ++k) {
							const float32 ax = (k == 0 ? 0.f : (k == 1 ? 1.f : (k == 2 ? 0.f : -1.f))) * g * 0.17f;
							const float32 ay = (k == 0 ? -1.f : (k == 1 ? 0.f : (k == 2 ? 1.f : 0.f))) * g * 0.17f;
							dl.AddCircleFilled(rep.V(cx + cw + g * 0.5f + ax, cy + ch * 0.30f + ay), g * 0.07f * rep.s, fonce);
							dl.AddCircleFilled(rep.V(cx - g * 0.5f + ax, cy + ch * 0.64f + ay), g * 0.07f * rep.s, fonce);
						}
						break;
					}
					case NkFamilleAppareil::NK_MONTRE: {
						const NkColor bracelet = r.cadreClair ? NkColor(200, 196, 188) : NkColor(36, 38, 44);
						dl.AddRectFilled(rep.R(L * 0.18f, -hautBas - H * 0.32f, L * 0.64f, H * 0.36f), bracelet, 6.f * rep.s);
						dl.AddRectFilled(rep.R(L * 0.18f, H + hautBas - H * 0.04f, L * 0.64f, H * 0.36f), bracelet, 6.f * rep.s);
						break;
					}
					default:
						break;
				}

				// 2. Les boutons de la tranche, sous le corps (ils en depassent).
				for (uint8 i = 0; i < b.nbBoutons; ++i) {
					const NkBoutonAppareil &bt = b.boutons[i];
					NkRect rb;
					if ((bt.cote & 1) != 0) { // droite ou gauche
						const float32 y = cy + bt.debut * ch;
						const float32 x = bt.cote == 1 ? cx + cw + tr - 1.f : cx - tr - kSaillieBouton;
						rb = rep.R(x, y, kSaillieBouton + 1.f, bt.longueur * ch);
					} else {
						const float32 x = cx + bt.debut * cw;
						const float32 y = bt.cote == 0 ? cy - tr - kSaillieBouton : cy + ch + tr - 1.f;
						rb = rep.R(x, y, bt.longueur * cw, kSaillieBouton + 1.f);
					}
					dl.AddRectFilled(rb, pal.boutonOmbre, 1.5f * rep.s);
					dl.AddRect(rb, pal.bouton, 1.f, 1.5f * rep.s);
				}

				// 3. Le corps : l'anneau entre l'ecran et la tranche, puis la
				//    tranche (un filet clair) et le filet noir autour de la dalle.
				Anneau(dl, ecranPts, corps, kPtsContour, pal.corps);
				Anneau(dl, corps, metal, kPtsContour, pal.metal);
				dl.AddPolyline(metal, kPtsContour, pal.reflet1, 1.f, true);
				dl.AddPolyline(corps, kPtsContour, pal.tranche, 1.f, true);
				dl.AddPolyline(ecranPts, kPtsContour, pal.filet, 1.5f, true);

				// 4. Ce qui est DANS la bordure.
				if (navigateur) {
					// La barre du navigateur : trois pastilles, un onglet, l'adresse.
					const NkRect barreR = rep.R(0.f, -barre, L, barre);
					const NkColor fondBarre = r.cadreClair ? NkColor(232, 234, 238) : NkColor(40, 43, 51);
					dl.AddRectFilled(barreR, fondBarre);
					const NkColor pastilles[3] = {NkColor(236, 106, 94), NkColor(244, 190, 80), NkColor(98, 197, 84)};
					for (int32 k = 0; k < 3; ++k) {
						dl.AddCircleFilled(rep.V(14.f + 16.f * static_cast<float32>(k), -barre * 0.5f), 4.5f * rep.s, pastilles[k]);
					}
					const NkColor champ = r.cadreClair ? NkColor(255, 255, 255) : NkColor(26, 28, 34);
					dl.AddRectFilled(rep.R(L * 0.16f, -barre * 0.76f, L * 0.68f, barre * 0.52f), champ, barre * 0.26f * rep.s);
					dl.AddCircleFilled(rep.V(L * 0.16f + 12.f, -barre * 0.5f), 3.f * rep.s, pal.tranche); // le cadenas
				}
				if (b.boutonAccueil) {
					const NkVec2 c = rep.V(L * 0.5f, H + hautBas * 0.5f);
					const float32 rr = hautBas * 0.30f * rep.s;
					dl.AddCircleFilled(c, rr, pal.accent);
					dl.AddCircle(c, rr, pal.tranche, 1.5f);
					dl.AddRectFilled(rep.R(L * 0.42f, -hautBas * 0.5f - 2.f, L * 0.16f, 4.f), pal.boutonOmbre, 2.f * rep.s);
				}
				// Une camera frontale DANS la bordure quand l'ecran n'a pas de
				// decoupe (tablettes, anciens telephones).
				const bool aCameraBordure = b.decoupe == NkTypeDecoupe::NK_AUCUNE && hautBas >= 9.f &&
											(b.famille == NkFamilleAppareil::NK_TELEPHONE ||
											 b.famille == NkFamilleAppareil::NK_TABLETTE || b.famille == NkFamilleAppareil::NK_BUREAU);
				if (aCameraBordure) {
					Lentille(dl, rep, pal, L * 0.5f + (b.boutonAccueil ? -L * 0.13f : 0.f), -hautBas * 0.5f,
							 hautBas * 0.16f < 3.5f ? hautBas * 0.16f : 3.5f);
				}
				if (b.famille == NkFamilleAppareil::NK_PLIABLE && b.largeur * 10u > b.hauteur * 7u) {
					// La charniere de l'ecran interieur : deux reperes dans la bordure.
					dl.AddRectFilled(rep.R(L * 0.5f - 1.f, -hautBas, 2.f, hautBas), pal.tranche);
					dl.AddRectFilled(rep.R(L * 0.5f - 1.f, H, 2.f, hautBas), pal.tranche);
				}
				if (b.famille == NkFamilleAppareil::NK_MONTRE) {
					// La couronne et le poussoir sont deja des boutons ; un liseré.
					NkVec2 lunette[kPtsContour];
					ContourUni(rep, -cote * 0.5f, -hautBas * 0.5f, L + cote, H + hautBas, rayon + cote * 0.5f, lunette);
					dl.AddPolyline(lunette, kPtsContour, pal.tranche, 1.f, true);
				}
			} else {
				// Sans cadre : le contour de l'ecran seul, coins compris.
				dl.AddPolyline(ecranPts, kPtsContour, NkColor(200, 205, 215, 140), 1.f, true);
			}

			// --- LA ZONE SURE SIMULEE ----------------------------------------
			// ⚠️ C'est la raison d'etre de ce reglage. Ce qui tombe dans les
			// bandes est INATTEIGNABLE sur l'appareil — pas mal place :
			// inatteignable. Et cela ne se voit JAMAIS depuis une machine de
			// bureau.
			if (r.voirZoneSure) {
				const float32 ex = ecran.w / static_cast<float32>(p.largeur);
				const float32 ey = ecran.h / static_cast<float32>(p.hauteur);
				const NkColor voile(220, 90, 90, 46);
				const NkColor trait(235, 120, 120, 190);
				auto bande = [&](const NkRect &rr) {
					if (rr.w <= 0.f || rr.h <= 0.f) {
						return;
					}
					dl.AddRectFilled(rr, voile);
				};
				const float32 hHaut = p.zoneSure.top * ey;
				const float32 hBas = p.zoneSure.bottom * ey;
				const float32 lG = p.zoneSure.left * ex;
				const float32 lD = p.zoneSure.right * ex;
				bande(NkRect{ecran.x, ecran.y, ecran.w, hHaut});
				bande(NkRect{ecran.x, ecran.y + ecran.h - hBas, ecran.w, hBas});
				bande(NkRect{ecran.x, ecran.y + hHaut, lG, ecran.h - hHaut - hBas});
				bande(NkRect{ecran.x + ecran.w - lD, ecran.y + hHaut, lD, ecran.h - hHaut - hBas});
				// Le cadre de la zone SURE elle-meme : c'est dedans qu'un bouton
				// doit tenir.
				if (hHaut > 0.f || hBas > 0.f || lG > 0.f || lD > 0.f) {
					dl.AddRect(NkRect{ecran.x + lG, ecran.y + hHaut, ecran.w - lG - lD, ecran.h - hHaut - hBas}, trait, 1.f);
				}
				// La marge CONSEILLEE (TV, montre ronde) : tiretee, ambre -- le
				// systeme ne la rend pas au jeu.
				if (p.margeConseillee > 0.f) {
					const float32 mx = ecran.w * p.margeConseillee;
					const float32 my = ecran.h * p.margeConseillee;
					RectTirete(dl, NkRect{ecran.x + mx, ecran.y + my, ecran.w - 2.f * mx, ecran.h - 2.f * my},
							   NkColor(240, 190, 90, 220));
				}
			}

			// --- LA DECOUPE, PAR-DESSUS TOUT : sur l'appareil, rien ne s'y voit.
			if (r.voirDecoupe) {
				Decoupe(dl, rep, pal, b);
			}
		}

	} // namespace editeur
} // namespace nkentseu
