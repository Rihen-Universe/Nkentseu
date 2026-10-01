// =============================================================================
// NkTerminalVue.cpp — le dessin partage du terminal. Voir l'en-tete : la
// palette suit le theme, la grille a des marges, le curseur clignote, la
// barre de defilement est celle du kit.
// =============================================================================
#include "NKEditorKit/Terminal/NkTerminalVue.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKEditorKit/NkThemeToGui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editorkit {

		using nkgui::NkColor;
		using nkgui::NkGuiContext;
		using nkgui::NkGuiDrawList;
		using nkgui::NkGuiFont;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			NkColor Role(const NkTheme &t, NkRole r, NkRole repli) {
				return NkThemeUnpack(t.GetOuRepli(r, repli));
			}

			NkColor Melange(NkColor a, NkColor b, float32 t) {
				NkColor c = NkThemeMix(a, b, t);
				c.a = 255;
				return c;
			}

			NkColor Alpha(NkColor c, uint8 a) {
				c.a = a;
				return c;
			}

			/// Rapproche `c` de `vers` jusqu'a un contraste `min` contre `fond`.
			/// C'est la GARANTIE de lisibilite : elle vaut pour un theme qui
			/// n'existe pas encore.
			NkColor Assurer(NkColor c, const NkColor &fond, const NkColor &vers, float32 min) {
				for (int32 k = 0; k < 10 && NkTerminalContraste(c, fond) < min; ++k)
					c = Melange(c, vers, 0.2f);
				return c;
			}

			int32 EncoderU8(uint32 cp, char *dst) {
				if (cp < 0x80) {
					dst[0] = static_cast<char>(cp);
					return 1;
				}
				if (cp < 0x800) {
					dst[0] = static_cast<char>(0xC0 | (cp >> 6));
					dst[1] = static_cast<char>(0x80 | (cp & 0x3F));
					return 2;
				}
				if (cp < 0x10000) {
					dst[0] = static_cast<char>(0xE0 | (cp >> 12));
					dst[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
					dst[2] = static_cast<char>(0x80 | (cp & 0x3F));
					return 3;
				}
				dst[0] = static_cast<char>(0xF0 | (cp >> 18));
				dst[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
				dst[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
				dst[3] = static_cast<char>(0x80 | (cp & 0x3F));
				return 4;
			}

			const NkGuiFont *PoliceDe(const NkGuiContext &ctx, const NkTerminalGrilleStyle &style) {
				if (style.police && style.police->Valid())
					return style.police;
				if (ctx.codeFont && ctx.codeFont->Valid())
					return ctx.codeFont;
				return (ctx.font && ctx.font->Valid()) ? ctx.font : nullptr;
			}

			void Metriques(const NkGuiContext &ctx, const NkTerminalGrilleStyle &style, float32 &cw, float32 &lh,
						   float32 &asc) {
				const NkGuiFont *f = PoliceDe(ctx, style);
				cw = (f && f->Face()) ? f->Face()->CalcTextSizeX("M") : 8.f;
				if (cw < 1.f)
					cw = 8.f;
				lh = (f ? f->LineHeight() : 16.f) + style.interligne;
				asc = f ? f->Ascent() : 12.f;
			}

			NkRect Interieur(const NkRect &zone, const NkTerminalGrilleStyle &style) {
				const float32 sbW = style.barreDefilement ? NkScrollbarWidth() : 0.f;
				const float32 mx = style.marge, my = style.marge * 0.6f;
				return NkRect{zone.x + mx, zone.y + my, zone.w - 2.f * mx - sbW, zone.h - 2.f * my};
			}

			/// Les « bras » d'un caractere de cadre (haut 1, droite 2, bas 4,
			/// gauche 8) : de quoi le TRACER quand la police ne l'a pas.
			uint32 Bras(uint32 cp) {
				switch (cp) {
					case 0x2500: case 0x2501: case 0x2550: return 2 | 8;
					case 0x2502: case 0x2503: case 0x2551: return 1 | 4;
					case 0x250C: case 0x250F: case 0x2554: case 0x256D: return 2 | 4;
					case 0x2510: case 0x2513: case 0x2557: case 0x256E: return 4 | 8;
					case 0x2514: case 0x2517: case 0x255A: case 0x2570: return 1 | 2;
					case 0x2518: case 0x251B: case 0x255D: case 0x256F: return 1 | 8;
					case 0x251C: case 0x2523: case 0x2560: return 1 | 2 | 4;
					case 0x2524: case 0x252B: case 0x2563: return 1 | 4 | 8;
					case 0x252C: case 0x2533: case 0x2566: return 2 | 4 | 8;
					case 0x2534: case 0x253B: case 0x2569: return 1 | 2 | 8;
					case 0x253C: case 0x254B: case 0x256C: return 15;
					default: return 0;
				}
			}

			/// Trace un caractere de cadre ou de bloc. Rend faux s'il n'en est pas un.
			bool TracerCadre(NkGuiDrawList &dl, uint32 cp, float32 x, float32 y, float32 w, float32 h, NkColor c) {
				if (cp >= 0x2580 && cp <= 0x259F) { // blocs
					if (cp == 0x2588)
						dl.AddRectFilled({x, y, w + 0.5f, h}, c);
					else if (cp == 0x2580)
						dl.AddRectFilled({x, y, w + 0.5f, h * 0.5f}, c);
					else if (cp == 0x2584)
						dl.AddRectFilled({x, y + h * 0.5f, w + 0.5f, h * 0.5f}, c);
					else if (cp == 0x258C)
						dl.AddRectFilled({x, y, w * 0.5f, h}, c);
					else if (cp == 0x2590)
						dl.AddRectFilled({x + w * 0.5f, y, w * 0.5f, h}, c);
					else if (cp >= 0x2591 && cp <= 0x2593)
						dl.AddRectFilled({x, y, w + 0.5f, h}, Alpha(c, static_cast<uint8>(60 * (cp - 0x2590))));
					else
						return false;
					return true;
				}
				const uint32 b = Bras(cp);
				if (!b)
					return false;
				const float32 e = (cp == 0x2501 || cp == 0x2503 || (cp >= 0x250F && cp <= 0x254B && (cp & 3) == 3)) ? 2.f : 1.f;
				const float32 cx = std::floor(x + w * 0.5f), cy = std::floor(y + h * 0.5f);
				if (b & 1)
					dl.AddRectFilled({cx, y, e, cy - y + e}, c);
				if (b & 4)
					dl.AddRectFilled({cx, cy, e, y + h - cy}, c);
				if (b & 8)
					dl.AddRectFilled({x, cy, cx - x + e, e}, c);
				if (b & 2)
					dl.AddRectFilled({cx, cy, x + w - cx + 0.5f, e}, c);
				return true;
			}

			void Normaliser(const NkTerminalVue &v, int32 &aL, int32 &aC, int32 &bL, int32 &bC) {
				aL = v.selAL;
				aC = v.selAC;
				bL = v.selBL;
				bC = v.selBC;
				if (aL > bL || (aL == bL && aC > bC)) {
					int32 t = aL;
					aL = bL;
					bL = t;
					t = aC;
					aC = bC;
					bC = t;
				}
			}

			/// Coupe un chemin PAR LA GAUCHE (« …\Projets\Nkentseu ») : c'est la
			/// fin d'un chemin qui dit ou l'on est.
			NkString CouperAGauche(const NkGuiFont *f, const char *s, float32 largeur) {
				if (!f || !s)
					return NkString(s ? s : "");
				if (f->MeasureWidth(s) <= largeur)
					return NkString(s);
				const char *p = s;
				while (*p) {
					++p;
					while ((static_cast<unsigned char>(*p) & 0xC0) == 0x80)
						++p; // ne pas couper un caractere UTF-8
					const NkString essai = NkString("…") + p;
					if (f->MeasureWidth(essai.CStr()) <= largeur)
						return essai;
				}
				return NkString("…");
			}
		} // namespace

		// =====================================================================
		//  PALETTE
		// =====================================================================
		float32 NkTerminalContraste(const NkColor &a, const NkColor &b) {
			auto lum = [](const NkColor &c) {
				auto lin = [](uint8 v) {
					const float32 s = v / 255.f;
					return s <= 0.03928f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
				};
				return 0.2126f * lin(c.r) + 0.7152f * lin(c.g) + 0.0722f * lin(c.b);
			};
			const float32 la = lum(a), lb = lum(b);
			return la > lb ? (la + 0.05f) / (lb + 0.05f) : (lb + 0.05f) / (la + 0.05f);
		}

		NkTerminalPalette NkTerminalPaletteDuTheme(const NkTheme &t) {
			NkTerminalPalette p;
			p.sombre = t.IsDark();
			p.fond = Role(t, NkRole::CodeBg, NkRole::InputBg);
			p.texte = Role(t, NkRole::Text, NkRole::Text);
			p.attenue = Role(t, NkRole::TextMuted, NkRole::Text);
			p.entete = Role(t, NkRole::PanelHeader, NkRole::PanelBg);
			p.enteteTexte = p.texte;
			p.bord = Role(t, NkRole::Border, NkRole::Border);
			p.erreur = Role(t, NkRole::StatusErr, NkRole::AccentSel);
			const NkColor bleu = Role(t, NkRole::AccentUi, NkRole::AccentUi);
			const NkColor rouge = p.erreur;
			const NkColor vert = Role(t, NkRole::StatusOk, NkRole::AccentUi);
			const NkColor jaune = Role(t, NkRole::StatusWarn, NkRole::AccentSel);
			const NkColor violet = Role(t, NkRole::AccentAI, NkRole::AccentUi);
			const NkColor cyan = Melange(bleu, vert, 0.45f);
			const NkColor blanc = {255, 255, 255, 255}, noir = {0, 0, 0, 255};
			const NkColor bases[6] = {rouge, vert, jaune, bleu, violet, cyan};
			if (p.sombre) {
				p.ansi[0] = Melange(p.fond, p.texte, 0.28f);
				for (int32 i = 0; i < 6; ++i) {
					p.ansi[1 + i] = Melange(bases[i], blanc, 0.12f);
					p.ansi[9 + i] = Melange(bases[i], blanc, 0.38f);
				}
				p.ansi[7] = Melange(p.texte, p.fond, 0.12f);
				p.ansi[8] = p.attenue;
				p.ansi[15] = Melange(blanc, p.texte, 0.08f);
			} else {
				p.ansi[0] = p.texte;
				for (int32 i = 0; i < 6; ++i) {
					p.ansi[1 + i] = Melange(bases[i], noir, 0.22f);
					p.ansi[9 + i] = Melange(bases[i], noir, 0.08f);
				}
				p.ansi[7] = p.attenue;
				p.ansi[8] = Melange(p.attenue, p.fond, 0.15f);
				p.ansi[15] = Melange(p.attenue, p.fond, 0.30f);
			}
			// LA GARANTIE : chaque couleur de texte lisible contre le fond.
			for (int32 i = 1; i < 16; ++i)
				p.ansi[i] = Assurer(p.ansi[i], p.fond, p.texte, 3.f);
			p.ansi[0] = Assurer(p.ansi[0], p.fond, p.texte, p.sombre ? 1.6f : 4.5f);
			p.curseur = p.texte;
			p.sousCurseur = p.fond;
			p.selection = Alpha(bleu, p.sombre ? 110 : 80);
			p.lien = Assurer(Melange(bleu, blanc, p.sombre ? 0.3f : 0.f), p.fond, p.texte, 3.f);
			p.trouve = Alpha(jaune, 90);
			p.trouveActif = Alpha(jaune, 190);
			return p;
		}

		NkColor NkTerminalResoudre(const NkTerminalPalette &pal, uint32 c, bool estFond, bool gras) {
			if (c == kNkTermDefaut)
				return estFond ? pal.fond : pal.texte;
			if (NkTermEstRgb(c))
				return NkColor{static_cast<uint8>((c >> 16) & 0xFF), static_cast<uint8>((c >> 8) & 0xFF),
							   static_cast<uint8>(c & 0xFF), 255};
			uint32 i = c & 0xFF;
			if (!estFond && gras && i < 8)
				i += 8; // « gras = brillant », la convention des terminaux
			if (i < 16)
				return pal.ansi[i];
			if (i >= 232) {
				const uint8 v = static_cast<uint8>(8 + 10 * (i - 232));
				return NkColor{v, v, v, 255};
			}
			const uint32 k = i - 16;
			auto ch = [](uint32 v) -> uint8 { return static_cast<uint8>(v == 0 ? 0 : 55 + 40 * v); };
			return NkColor{ch((k / 36) % 6), ch((k / 6) % 6), ch(k % 6), 255};
		}

		NkColor NkTerminalCouleurShell(NkShellGenre g) {
			switch (g) {
				case NkShellGenre::PowerShell7: return {46, 140, 222, 255};
				case NkShellGenre::WindowsPowerShell: return {38, 104, 196, 255};
				case NkShellGenre::Cmd: return {104, 114, 128, 255};
				case NkShellGenre::GitBash: return {240, 80, 51, 255};
				case NkShellGenre::Msys2: return {142, 92, 214, 255};
				case NkShellGenre::Wsl: return {233, 84, 32, 255};
				case NkShellGenre::Bash: return {78, 170, 37, 255};
				case NkShellGenre::Zsh: return {200, 120, 40, 255};
				case NkShellGenre::Fish: return {52, 160, 150, 255};
				default: return {120, 128, 140, 255};
			}
		}

		void NkTerminalDessinerIcone(NkGuiDrawList &dl, const NkRect &r, NkShellGenre g) {
			const NkColor c = NkTerminalCouleurShell(g);
			dl.AddRectFilled(r, c, r.w * 0.22f);
			// « >_ » trace en blanc : chevron + soulignement.
			const NkColor b = {255, 255, 255, 240};
			const float32 u = r.w / 14.f;
			const float32 x0 = r.x + 3.2f * u, y0 = r.y + 3.6f * u, h = 6.6f * u;
			dl.AddLine({x0, y0}, {x0 + 3.3f * u, y0 + h * 0.5f}, b, 1.6f * u);
			dl.AddLine({x0 + 3.3f * u, y0 + h * 0.5f}, {x0, y0 + h}, b, 1.6f * u);
			dl.AddRectFilled({r.x + 7.4f * u, y0 + h - 1.2f * u, 3.6f * u, 1.5f * u}, b);
		}

		// =====================================================================
		//  GRILLE
		// =====================================================================
		void NkTerminalTailleGrille(const NkGuiContext &ctx, const NkRect &zone, const NkTerminalGrilleStyle &style,
									int16 &cols, int16 &rows) {
			float32 cw, lh, asc;
			Metriques(ctx, style, cw, lh, asc);
			const NkRect in = Interieur(zone, style);
			int32 c = static_cast<int32>(in.w / cw), r = static_cast<int32>(in.h / lh);
			cols = static_cast<int16>(c < 2 ? 2 : (c > 500 ? 500 : c));
			rows = static_cast<int16>(r < 1 ? 1 : (r > 300 ? 300 : r));
		}

		NkTerminalGrilleResultat NkTerminalDessinerGrille(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &zone,
														   const NkTerm &term, NkTerminalVue &vue,
														   const NkTerminalPalette &pal,
														   const NkTerminalGrilleStyle &style,
														   const NkTerminalSurlignage *surl, int32 nbSurl) {
			NkTerminalGrilleResultat res;
			dl.AddRectFilled(zone, pal.fond);
			if (zone.w < 16.f || zone.h < 8.f)
				return res;
			const NkGuiFont *police = PoliceDe(ctx, style);
			const nkentseu::NkFont *face = police ? police->Face() : nullptr;
			float32 cw, lh, asc;
			Metriques(ctx, style, cw, lh, asc);
			NkTerminalTailleGrille(ctx, zone, style, res.cols, res.rows);
			const NkRect in = Interieur(zone, style);
			const NkVec2 m = ctx.input.mousePos;
			const bool dedans = nkgui::NkGuiRectContains(zone, m) && ctx.PointReachable(m);
			res.survol = dedans;

			// ── Defilement : en pixels, borne au bas (l'ECRAN, jamais plus bas).
			const int32 total = static_cast<int32>(term.TotalLines());
			const float32 vueH = res.rows * lh;
			const float32 contenuH = total * lh;
			const float32 maxDefil = contenuH > vueH ? contenuH - vueH : 0.f;
			if (dedans && ctx.input.wheel != 0.f && ctx.popupDepth == 0) {
				vue.defil -= ctx.input.wheel * lh * 3.f;
				ctx.input.wheel = 0.f; // consommee : rien d'autre ne defile avec
				vue.suivre = false;
			}
			if (vue.suivre)
				vue.defil = maxDefil;
			if (vue.defil < 0.f)
				vue.defil = 0.f;
			if (vue.defil > maxDefil)
				vue.defil = maxDefil;
			const int32 premiere = static_cast<int32>(vue.defil / lh);
			const float32 decalage = vue.defil - premiere * lh;

			// ── Souris : cellule sous le pointeur ─────────────────────────────
			auto ligneA = [&](float32 y) {
				int32 L = premiere + static_cast<int32>(std::floor((y - in.y + decalage) / lh));
				return L < 0 ? 0 : (L >= total ? total - 1 : L);
			};
			auto colA = [&](float32 x) {
				int32 c = static_cast<int32>(std::floor((x - in.x) / cw + 0.5f));
				return c < 0 ? 0 : (c > term.Cols() ? term.Cols() : c);
			};
			const NkRect zoneTexte = {zone.x, zone.y, zone.w - (style.barreDefilement ? NkScrollbarWidth() : 0.f), zone.h};
			const bool surTexte = dedans && nkgui::NkGuiRectContains(zoneTexte, m);
			// Le lien sous le pointeur (souligne avec Ctrl, ouvert par Ctrl+clic).
			int32 lienL = -1, lienC0 = 0, lienC1 = 0;
			NkTerminalGrilleResultat lien;
			if (surTexte && ctx.input.ctrlDown && total > 0) {
				const int32 L = ligneA(m.y), C = static_cast<int32>(std::floor((m.x - in.x) / cw));
				if (NkTerminalLienSous(term, L, C, lienC0, lienC1, lien)) {
					lienL = L;
					ctx.wantCursor = nkgui::NkGuiCursor::Hand;
				}
			} else if (surTexte) {
				ctx.wantCursor = nkgui::NkGuiCursor::Text;
			}
			if (surTexte && ctx.popupDepth == 0 && ctx.input.mouseClicked[0]) {
				res.clicDedans = true;
				if (lienL >= 0) {
					res.lienClique = true;
					res.lienEstUrl = lien.lienEstUrl;
					res.lienFichier = lien.lienFichier;
					res.lienLigne = lien.lienLigne;
					res.lienColonne = lien.lienColonne;
				} else if (ctx.input.mouseDoubleClicked[0] && total > 0) {
					// Double-clic : le MOT sous le pointeur.
					const int32 L = ligneA(m.y);
					const NkTerm::Line &ln = term.LineAt(static_cast<usize>(L));
					int32 c = static_cast<int32>(std::floor((m.x - in.x) / cw));
					const int32 n = static_cast<int32>(ln.Size());
					if (c >= 0 && c < n && ln[c].cp != 0x20) {
						int32 a = c, b = c;
						while (a > 0 && ln[a - 1].cp != 0x20)
							--a;
						while (b + 1 < n && ln[b + 1].cp != 0x20)
							++b;
						vue.selAL = vue.selBL = L;
						vue.selAC = a;
						vue.selBC = b + 1;
					}
					vue.glisse = false;
				} else if (total > 0) {
					vue.selAL = vue.selBL = ligneA(m.y);
					vue.selAC = vue.selBC = colA(m.x);
					vue.glisse = true;
				}
			}
			if (vue.glisse && ctx.input.mouseDown[0] && total > 0) {
				vue.selBL = ligneA(m.y);
				vue.selBC = colA(m.x);
				// Glisser au-dela du bord : on defile, comme partout.
				if (m.y < in.y) {
					vue.defil -= lh;
					vue.suivre = false;
				} else if (m.y > in.y + in.h) {
					vue.defil += lh;
				}
			}
			if (!ctx.input.mouseDown[0])
				vue.glisse = false;
			if (surTexte && ctx.popupDepth == 0 && ctx.input.mouseClicked[1]) {
				res.menuDemande = true;
				res.menuX = m.x;
				res.menuY = m.y;
			}

			// ── Rendu ─────────────────────────────────────────────────────────
			int32 sAL, sAC, sBL, sBC;
			Normaliser(vue, sAL, sAC, sBL, sBC);
			const bool sel = vue.AUneSelection();
			const NkRect clip = {in.x - 2.f, in.y, in.w + 4.f, in.h};
			dl.PushClipRect(clip, true);
			const float32 hautTexte = police ? police->LineHeight() : lh;
			const float32 dy = (lh - hautTexte) * 0.5f; // le glyphe centre dans sa ligne
			const int32 derniere = premiere + res.rows + 1;
			for (int32 i = premiere; i <= derniere && i < total; ++i) {
				const float32 y = in.y + (i - premiere) * lh - decalage;
				const NkTerm::Line &ln = term.LineAt(static_cast<usize>(i));
				const int32 n = static_cast<int32>(ln.Size());
				// 1) Fonds de cellule (couleur de fond ANSI, inverse).
				for (int32 c = 0; c < n; ++c) {
					const NkTermCell &cell = ln[c];
					const bool inv = (cell.attr & NK_TERM_INVERSE) != 0;
					if (cell.bg == kNkTermDefaut && !inv)
						continue;
					const NkColor bg = inv ? NkTerminalResoudre(pal, cell.fg, false, (cell.attr & NK_TERM_GRAS) != 0)
										   : NkTerminalResoudre(pal, cell.bg, true, false);
					dl.AddRectFilled({in.x + c * cw, y, cw + 0.5f, lh}, bg);
				}
				// 2) Selection, recherche.
				if (sel && i >= sAL && i <= sBL) {
					const int32 c0 = (i == sAL) ? sAC : 0;
					const int32 c1 = (i == sBL) ? sBC : term.Cols();
					if (c1 > c0)
						dl.AddRectFilled({in.x + c0 * cw, y, (c1 - c0) * cw, lh}, pal.selection);
				}
				for (int32 k = 0; k < nbSurl; ++k)
					if (surl[k].ligne == i)
						dl.AddRectFilled({in.x + surl[k].col * cw, y, surl[k].len * cw, lh},
										 surl[k].courant ? pal.trouveActif : pal.trouve, 2.f);
				// 3) Glyphes.
				if (!face)
					continue;
				const float32 base = y + dy + asc;
				for (int32 c = 0; c < n; ++c) {
					const NkTermCell &cell = ln[c];
					const bool gras = (cell.attr & NK_TERM_GRAS) != 0;
					NkColor fg = NkTerminalResoudre(pal, cell.fg, false, gras);
					if (cell.attr & NK_TERM_INVERSE)
						fg = NkTerminalResoudre(pal, cell.bg, true, false);
					if (cell.attr & NK_TERM_PALE)
						fg = Melange(fg, pal.fond, 0.45f);
					const float32 x = in.x + c * cw;
					if (cell.cp != 0x20 && cell.cp != 0) {
						const bool cadre = (cell.cp >= 0x2500 && cell.cp <= 0x259F) && !face->FindGlyphNoFallback(cell.cp);
						if (!cadre || !TracerCadre(dl, cell.cp, x, y, cw, lh, fg)) {
							char u8[5];
							const int32 k = EncoderU8(cell.cp, u8);
							u8[k] = 0;
							dl.AddText(face, police->TexId(), {x, base}, u8, fg, -1.f,
									   (cell.attr & NK_TERM_ITALIQUE) ? 0.2f : 0.f, u8 + k);
						}
					}
					if (cell.attr & NK_TERM_SOULIGNE)
						dl.AddRectFilled({x, base + 2.f, cw + 0.5f, 1.f}, fg);
					if (cell.attr & NK_TERM_BARRE)
						dl.AddRectFilled({x, y + lh * 0.5f, cw + 0.5f, 1.f}, fg);
				}
				if (i == lienL)
					dl.AddRectFilled({in.x + lienC0 * cw, base + 2.f, (lienC1 - lienC0) * cw, 1.f}, pal.lien);
			}

			// 4) Le curseur. Net : un bloc plein (ou barre, ou souligne, selon ce
			//    que le shell demande) quand le terminal a le clavier, un simple
			//    contour sinon. Il CLIGNOTE, mais reste plein pendant la frappe.
			if (term.CursorVisible()) {
				const int32 cl = static_cast<int32>(term.CursorLine());
				const int32 cc = term.CursorCol();
				if (cl >= premiere && cl <= derniere) {
					const float32 x = in.x + cc * cw, y = in.y + (cl - premiere) * lh - decalage;
					const float32 depuis = ctx.time - vue.derniereActivite;
					const bool clignote = style.curseurClignote && term.CursorBlink() && vue.focus;
					const bool allume = !clignote || depuis < 0.6f || std::fmod(depuis, 1.06f) < 0.53f;
					if (!vue.focus) {
						dl.AddRect({x, y, cw, lh}, Alpha(pal.curseur, 150), 1.f);
					} else if (allume) {
						switch (term.CursorStyle()) {
							case NkTermCurseur::Barre:
								dl.AddRectFilled({x, y + 1.f, 2.f, lh - 2.f}, pal.curseur);
								break;
							case NkTermCurseur::Souligne:
								dl.AddRectFilled({x, y + lh - 2.f, cw, 2.f}, pal.curseur);
								break;
							default: {
								dl.AddRectFilled({x, y, cw, lh}, pal.curseur, 1.f);
								// Le glyphe SOUS le bloc, en couleur de fond : lisible.
								const NkTerm::Line &ln = term.LineAt(static_cast<usize>(cl));
								if (face && cc < static_cast<int32>(ln.Size()) && ln[cc].cp != 0x20 && ln[cc].cp != 0) {
									char u8[5];
									const int32 k = EncoderU8(ln[cc].cp, u8);
									u8[k] = 0;
									dl.AddText(face, police->TexId(), {x, y + dy + asc}, u8, pal.sousCurseur, -1.f, 0.f,
											   u8 + k);
								}
								break;
							}
						}
					}
				}
			}
			dl.PopClipRect();

			// 5) La barre de defilement DU KIT, seulement s'il y a un historique a
			//    parcourir (l'espace reste reserve : les colonnes ne sautent pas).
			if (style.barreDefilement && maxDefil > 0.f) {
				const float32 sbW = NkScrollbarWidth();
				const NkRect piste = {zone.x + zone.w - sbW, zone.y + 2.f, sbW, zone.h - 4.f};
				char id[48];
				std::snprintf(id, sizeof(id), "##nkterm.defil.%p", static_cast<const void *>(&vue));
				float32 d = vue.defil;
				if (NkVScrollbar(ctx, dl, piste, d, contenuH, vueH, ctx.GetId(id), lh, false)) {
					vue.defil = d;
					vue.suivre = false;
				}
			}
			if (vue.defil >= maxDefil - 1.f)
				vue.suivre = true; // revenu en bas : on suit de nouveau le flux
			return res;
		}

		void NkTerminalDessinerEntete(NkGuiContext &ctx, NkGuiDrawList &dl, const NkRect &bande,
									  const NkTerminalPalette &pal, NkShellGenre genre, const char *nom,
									  const char *dossier, const char *droite) {
			dl.AddRectFilled(bande, pal.entete);
			dl.AddRectFilled({bande.x, bande.y + bande.h - 1.f, bande.w, 1.f}, pal.bord);
			const NkGuiFont *f = ctx.font;
			const float32 ic = bande.h * 0.58f;
			NkTerminalDessinerIcone(dl, {bande.x + 10.f, bande.y + (bande.h - ic) * 0.5f, ic, ic}, genre);
			if (!f || !f->Valid())
				return;
			const float32 base = bande.y + (bande.h - f->LineHeight()) * 0.5f + f->Ascent();
			float32 x = bande.x + 10.f + ic + 8.f;
			const float32 wDroite = (droite && *droite) ? f->MeasureWidth(droite) + 12.f : 0.f;
			if (nom && *nom) {
				dl.AddText(f->Face(), f->TexId(), {x, base}, nom, pal.enteteTexte);
				x += f->MeasureWidth(nom) + 10.f;
			}
			if (dossier && *dossier) {
				dl.AddRectFilled({x - 4.f, bande.y + bande.h * 0.3f, 1.f, bande.h * 0.4f}, pal.bord);
				const float32 reste = bande.x + bande.w - wDroite - x - 6.f;
				if (reste > 30.f) {
					const NkString d = CouperAGauche(f, dossier, reste);
					dl.AddText(f->Face(), f->TexId(), {x + 4.f, base}, d.CStr(), pal.attenue);
				}
			}
			if (wDroite > 0.f)
				dl.AddText(f->Face(), f->TexId(), {bande.x + bande.w - wDroite, base}, droite, pal.attenue);
		}

		// =====================================================================
		//  SELECTION, COLLAGE, LIENS
		// =====================================================================
		NkString NkTerminalTexteSelection(const NkTerm &term, const NkTerminalVue &vue) {
			NkString out;
			if (!vue.AUneSelection())
				return out;
			int32 aL, aC, bL, bC;
			Normaliser(vue, aL, aC, bL, bC);
			const int32 total = static_cast<int32>(term.TotalLines());
			for (int32 L = aL; L <= bL && L < total; ++L) {
				if (L < 0)
					continue;
				const NkTerm::Line &ln = term.LineAt(static_cast<usize>(L));
				const int32 n = static_cast<int32>(ln.Size());
				int32 c0 = (L == aL) ? aC : 0, c1 = (L == bL) ? bC : n;
				if (c1 > n)
					c1 = n;
				while (c1 > c0 && (ln[c1 - 1].cp == 0x20 || ln[c1 - 1].cp == 0))
					--c1;
				for (int32 c = c0 < 0 ? 0 : c0; c < c1; ++c) {
					char u8[5];
					const int32 k = EncoderU8(ln[c].cp ? ln[c].cp : 0x20, u8);
					for (int32 j = 0; j < k; ++j)
						out += u8[j];
				}
				if (L < bL)
					out += '\n';
			}
			return out;
		}

		void NkTerminalToutSelectionner(const NkTerm &term, NkTerminalVue &vue) {
			vue.selAL = 0;
			vue.selAC = 0;
			vue.selBL = static_cast<int32>(term.TotalLines()) - 1;
			vue.selBC = term.Cols();
		}

		void NkTerminalPreparerCollage(const NkTerm &term, const NkString &texte, NkVector<char> &sortie) {
			if (texte.Empty())
				return;
			auto put = [&](const char *s) {
				for (; *s; ++s)
					sortie.PushBack(*s);
			};
			if (term.BracketedPaste())
				put("\x1b[200~");
			const char *s = texte.CStr();
			for (usize i = 0; i < texte.Size(); ++i) {
				// CRLF et LF -> CR : c'est ce que la touche Entree envoie. Un LF nu
				// colle dans PowerShell ou cmd n'execute pas la ligne.
				if (s[i] == '\r' && i + 1 < texte.Size() && s[i + 1] == '\n')
					continue;
				sortie.PushBack(s[i] == '\n' ? '\r' : s[i]);
			}
			if (term.BracketedPaste())
				put("\x1b[201~");
		}

		bool NkTerminalLienSous(const NkTerm &term, int32 ligne, int32 col, int32 &c0, int32 &c1,
								NkTerminalGrilleResultat &lien) {
			if (ligne < 0 || ligne >= static_cast<int32>(term.TotalLines()))
				return false;
			const NkTerm::Line &ln = term.LineAt(static_cast<usize>(ligne));
			const int32 n = static_cast<int32>(ln.Size());
			if (col < 0 || col >= n)
				return false;
			auto separe = [&](int32 c) {
				const uint32 cp = ln[c].cp;
				return cp == 0x20 || cp == 0 || cp == '"' || cp == '\'' || cp == '<' || cp == '>' || cp == '|' || cp == '`';
			};
			if (separe(col))
				return false;
			int32 a = col, b = col;
			while (a > 0 && !separe(a - 1))
				--a;
			while (b + 1 < n && !separe(b + 1))
				++b;
			// Le mot, en ASCII (un chemin non ASCII garde ses octets, il sera rendu tel quel).
			NkString mot;
			for (int32 c = a; c <= b; ++c) {
				char u8[5];
				const int32 k = EncoderU8(ln[c].cp, u8);
				for (int32 j = 0; j < k; ++j)
					mot += u8[j];
			}
			const char *s = mot.CStr();
			const usize L = mot.Size();
			// URL ?
			const usize sch = mot.Find("://");
			if (sch != NkString::npos && sch >= 3) {
				usize debut = sch;
				while (debut > 0 && ((s[debut - 1] >= 'a' && s[debut - 1] <= 'z') || (s[debut - 1] >= 'A' && s[debut - 1] <= 'Z')))
					--debut;
				usize fin = L;
				while (fin > sch + 3 && (s[fin - 1] == '.' || s[fin - 1] == ',' || s[fin - 1] == ')' || s[fin - 1] == ';'))
					--fin;
				lien.lienEstUrl = true;
				lien.lienFichier = mot.SubStr(debut, fin - debut);
				c0 = a + static_cast<int32>(debut);
				c1 = a + static_cast<int32>(fin);
				return true;
			}
			// `chemin(L[,C])` (MSVC) ou `chemin:L[:C]` (gcc, clang, python...).
			usize fin = L;
			while (fin > 0 && (s[fin - 1] == ':' || s[fin - 1] == ',' || s[fin - 1] == '.' || s[fin - 1] == ';'))
				--fin;
			auto chiffres = [&](usize i, usize j) {
				if (j <= i)
					return false;
				for (usize k = i; k < j; ++k)
					if (s[k] < '0' || s[k] > '9')
						return false;
				return true;
			};
			auto nombre = [&](usize i, usize j) {
				int32 v = 0;
				for (usize k = i; k < j; ++k)
					v = v * 10 + (s[k] - '0');
				return v;
			};
			usize finChemin = 0;
			int32 li = 0, co = 0;
			if (fin > 0 && s[fin - 1] == ')') {
				usize ouv = fin - 1;
				while (ouv > 0 && s[ouv] != '(')
					--ouv;
				if (s[ouv] == '(') {
					usize virg = ouv + 1;
					while (virg < fin - 1 && s[virg] != ',')
						++virg;
					if (chiffres(ouv + 1, virg) && (virg == fin - 1 || chiffres(virg + 1, fin - 1))) {
						li = nombre(ouv + 1, virg);
						co = virg < fin - 1 ? nombre(virg + 1, fin - 1) : 0;
						finChemin = ouv;
					}
				}
			} else {
				// Depuis la fin : « :C » puis « :L » (ou « :L » seul).
				usize p2 = fin;
				while (p2 > 0 && s[p2 - 1] >= '0' && s[p2 - 1] <= '9')
					--p2;
				if (p2 < fin && p2 > 0 && s[p2 - 1] == ':') {
					usize p1 = p2 - 1;
					while (p1 > 0 && s[p1 - 1] >= '0' && s[p1 - 1] <= '9')
						--p1;
					if (p1 < p2 - 1 && p1 > 0 && s[p1 - 1] == ':') {
						li = nombre(p1, p2 - 1);
						co = nombre(p2, fin);
						finChemin = p1 - 1;
					} else {
						li = nombre(p2, fin);
						finChemin = p2 - 1;
					}
				}
			}
			if (finChemin < 2 || li <= 0)
				return false;
			// Un chemin, pas « erreur:12 » : une extension ou un separateur.
			bool cheminPlausible = false;
			for (usize k = 0; k < finChemin; ++k)
				if (s[k] == '.' || s[k] == '/' || s[k] == '\\')
					cheminPlausible = true;
			if (!cheminPlausible)
				return false;
			lien.lienEstUrl = false;
			lien.lienFichier = mot.SubStr(0, finChemin);
			lien.lienLigne = li;
			lien.lienColonne = co;
			c0 = a;
			c1 = a + static_cast<int32>(fin);
			return true;
		}

		// =====================================================================
		//  CLAVIER
		// =====================================================================
		bool NkTerminalClavier(NkGuiContext &ctx, const NkTerm &term, NkTerminalVue &vue, NkVector<char> &sortie,
							   const NkTerminalClavierOptions &opt) {
			using nkgui::NkGuiKey;
			nkgui::NkGuiInput &in = ctx.input;
			const usize avant = sortie.Size();
			auto put = [&](const char *s) {
				for (; *s; ++s)
					sortie.PushBack(*s);
			};
			// Modificateurs xterm : 2 Maj, 3 Alt, 5 Ctrl (et leurs sommes).
			const int32 mod = 1 + (in.shiftDown ? 1 : 0) + (in.altDown ? 2 : 0) + (in.ctrlDown ? 4 : 0);
			auto fleche = [&](char lettre) {
				char b[16];
				if (mod > 1)
					std::snprintf(b, sizeof(b), "\x1b[1;%d%c", static_cast<int>(mod), lettre);
				else if (term.AppCursorKeys())
					std::snprintf(b, sizeof(b), "\x1bO%c", lettre);
				else
					std::snprintf(b, sizeof(b), "\x1b[%c", lettre);
				put(b);
			};
			// 1) Les caracteres tapes. AltGr (Ctrl+Alt sous Windows) produit des
			//    caracteres ordinaires (« @ » en AZERTY) : on ne les prefixe PAS
			//    d'ESC. Alt seul, oui (meta : Alt+b recule d'un mot).
			const bool meta = in.altDown && !in.ctrlDown;
			for (int32 i = 0; i < in.charCount; ++i) {
				const uint32 cp = in.chars[i];
				if (cp == '\t' || cp == '\r' || cp == '\n' || cp == 8 || cp == 127 || cp == 27)
					continue; // touches dediees ci-dessous
				if (cp == 3 && in.wantCopy)
					continue; // Ctrl+C : decide plus bas (copier OU interrompre)
				if (cp == 22 && in.wantPaste)
					continue;
				if (cp == 1 && opt.ctrlAToutSelectionne)
					continue;
				if (meta && cp >= 32)
					sortie.PushBack('\x1b');
				char u8[5];
				const int32 k = EncoderU8(cp, u8);
				for (int32 j = 0; j < k; ++j)
					sortie.PushBack(u8[j]);
			}
			// 2) Les touches d'edition.
			auto K = [&](NkGuiKey k) { return in.KeyPressedRepeat(k); };
			if (K(NkGuiKey::Enter))
				put("\r");
			if (K(NkGuiKey::Backspace))
				put(in.ctrlDown ? "\x17" : (meta ? "\x1b\x7f" : "\x7f"));
			if (K(NkGuiKey::Tab))
				put(in.shiftDown ? "\x1b[Z" : "\t");
			if (in.KeyPressed(NkGuiKey::Escape))
				put("\x1b");
			if (K(NkGuiKey::Up))
				fleche('A');
			if (K(NkGuiKey::Down))
				fleche('B');
			if (K(NkGuiKey::Right))
				fleche('C');
			if (K(NkGuiKey::Left))
				fleche('D');
			if (K(NkGuiKey::Home))
				fleche('H');
			if (K(NkGuiKey::End))
				fleche('F');
			if (K(NkGuiKey::Delete))
				put("\x1b[3~");
			if (K(NkGuiKey::F2))
				put("\x1bOQ");
			if (K(NkGuiKey::F5))
				put("\x1b[15~");
			if (K(NkGuiKey::F8))
				put("\x1b[19~");
			if (K(NkGuiKey::F12))
				put("\x1b[24~");
			// 3) Copier / coller / tout selectionner.
			if (in.wantCopy) {
				if (vue.AUneSelection()) {
					const NkString t = NkTerminalTexteSelection(term, vue);
					if (!t.Empty())
						ctx.SetClipboard(t.CStr());
					vue.Deselectionner();
				} else {
					put("\x03"); // Ctrl+C sans selection : interrompre
				}
				in.wantCopy = false;
			}
			if (in.wantPaste) {
				NkTerminalPreparerCollage(term, ctx.GetClipboard(), sortie);
				in.wantPaste = false;
			}
			if (in.wantSelectAll && opt.ctrlAToutSelectionne) {
				NkTerminalToutSelectionner(term, vue);
				in.wantSelectAll = false;
			}
			const bool tape = sortie.Size() > avant;
			if (tape) {
				vue.suivre = true; // taper ramene en bas, comme partout
				vue.derniereActivite = ctx.time;
				vue.Deselectionner();
			}
			return tape;
		}

	} // namespace editorkit
} // namespace nkentseu
