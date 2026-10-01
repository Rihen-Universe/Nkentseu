#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkModelerBrand.h — la MARQUE de NKCraft, dessinee au painter.
//
// LOGO PROVISOIRE : L'ARAIGNEE (choix de Rihen, 29/09/2026, piste 4 de la
// planche « Logos NKCraft »), en attendant la proposition du designer. Rihen :
// elle « donne une vision de vertex et d'indices dans son schema » -- le corps
// est un cube vu en isometrie, les pattes sont des ARETES, leurs bouts des
// SOMMETS, et le fil tenu entre elles la toile d'un maillage. L'araignee est un
// motif frequent des arts des Grassfields (Bamileke, Bamoum), associe a la
// sagesse et a la divination.
//
// MEME DESSIN PARTOUT. Les coordonnees sont celles du SVG maitre
// (data/brand/nkcraft_araignee_{sombre,clair}.svg, repere 100 x 100) et de l'icone
// d'application (Resources/gen_icon.ps1) : changer le logo, c'est changer ces
// trois endroits ensemble.
//
// ⚠ LES COULEURS VIENNENT DES ROLES DU THEME, jamais de valeurs en dur : un
// theme clair repeint les pattes (Text), le corps (AccentSel, l'ambre de
// selection) et les sommets (AccentUi, le bleu d'etat). Seule l'icone
// d'application, qui n'a pas de theme, fige ses couleurs.
//
// ⚠ LE FOND EST UN PARAMETRE. Les aretes du cube sur le corps sont tracees
// dans la couleur de ce qu'il y a DERRIERE (WindowBg sur l'accueil,
// PanelHeader dans la barre de menus) : les deviner donnerait des traits de la
// mauvaise couleur des qu'on pose la marque ailleurs.
// =============================================================================

#include "NKCraft/Shell/NkModelerUI.h"
#include "NKEditorKit/Components/NkComponentPaint.h" // (01/10) la meme araignee pour le lanceur du kit

namespace nkentseu {
	namespace nk3d {

		/// Pattes : (hanche, genou, bout), cote gauche ; la droite est son miroir.
		/// UNE table pour les deux dessins ci-dessous (le peintre du modeleur et
		/// celui du kit) : deux copies des coordonnees finiraient par diverger.
		inline const float32 (&NkBrandPattes())[4][6] {
			static const float32 k[4][6] = {
				{42.f, 43.f, 28.f, 26.f, 16.f, 22.f},
				{40.f, 48.f, 22.f, 40.f, 8.f, 44.f},
				{40.f, 53.f, 22.f, 60.f, 8.f, 58.f},
				{42.f, 58.f, 28.f, 74.f, 16.f, 80.f},
			};
			return k;
		}

		/// Dessine l'araignee dans le carre (x, y, s) — s = cote en pixels.
		/// `fond` = role de la surface sous la marque.
		inline void PaintBrandMark(NkModelerPainter &p, float32 x, float32 y, float32 s,
								   NkRole fond = NkRole::WindowBg) {
			const float32 u = s / 100.f;
			auto P = [&](float32 vx, float32 vy) { return NkVec2{x + vx * u, y + vy * u}; };
			auto L = [&](float32 ax, float32 ay, float32 bx, float32 by, const NkColor &c,
						 float32 ep) { p.Line(x + ax * u, y + ay * u, x + bx * u, y + by * u, c, ep); };

			const NkColor cPatte = p.C(NkRole::Text);
			const NkColor cToile = p.C(NkRole::TextMuted);
			const NkColor cCorps = p.C(NkRole::AccentSel);
			const NkColor cSommet = p.C(NkRole::AccentUi);
			const NkColor cFond = p.C(fond);

			const float32(&kPattes)[4][6] = NkBrandPattes();

			// 1. La toile : le fil entre les bouts de pattes, discret.
			const float32 epToile = u * 1.4f;
			for (int32 cote = 0; cote < 2; ++cote) {
				auto X = [&](float32 v) { return cote == 0 ? v : 100.f - v; };
				for (int32 i = 0; i < 3; ++i)
					L(X(kPattes[i][4]), kPattes[i][5], X(kPattes[i + 1][4]), kPattes[i + 1][5], cToile,
					  epToile);
			}

			// 2. Les pattes : deux segments chacune ; un disque au genou arrondit
			//    la jointure (le painter trace des segments bout a bout).
			const float32 epPatte = u * 4.f;
			for (int32 cote = 0; cote < 2; ++cote) {
				auto X = [&](float32 v) { return cote == 0 ? v : 100.f - v; };
				for (int32 i = 0; i < 4; ++i) {
					const float32 *k = kPattes[i];
					L(X(k[0]), k[1], X(k[2]), k[3], cPatte, epPatte);
					L(X(k[2]), k[3], X(k[4]), k[5], cPatte, epPatte);
					p.DiscColor(x + X(k[2]) * u, y + k[3] * u, epPatte * 0.5f, cPatte);
				}
			}

			// 3. Le corps : un hexagone (le cube en isometrie), convexe.
			const NkVec2 corps[6] = {P(50.f, 36.f), P(62.f, 43.f), P(62.f, 57.f),
									 P(50.f, 64.f), P(38.f, 57.f), P(38.f, 43.f)};
			p.PolyFilled(corps, 6, cCorps);
			//    ... et ses trois aretes visibles, dans la couleur du fond.
			const float32 epArete = u * 3.f;
			L(38.f, 43.f, 50.f, 50.f, cFond, epArete);
			L(50.f, 50.f, 62.f, 43.f, cFond, epArete);
			L(50.f, 50.f, 50.f, 64.f, cFond, epArete);

			// 4. Les sommets : le bout de chaque patte.
			const float32 rSommet = u * 4.f;
			for (int32 cote = 0; cote < 2; ++cote)
				for (int32 i = 0; i < 4; ++i) {
					const float32 vx = cote == 0 ? kPattes[i][4] : 100.f - kPattes[i][4];
					p.DiscColor(x + vx * u, y + kPattes[i][5] * u, rSommet, cSommet);
				}
		}

		/// (01/10) LA MEME ARAIGNEE, PAR LE PEINTRE DU KIT : c'est elle que montre
		/// le lanceur de projets partage (NkProjectLauncherModel.h, crochet
		/// `peindreLogo`). Memes coordonnees (NkBrandPattes), memes ROLES ; seul
		/// le pinceau change. `fond` = role de la surface sous la marque (les
		/// aretes du cube sont tracees dans SA couleur).
		inline void PaintBrandMarkKit(editorkit::NkComponentPaint &p, const editorkit::NkPaintRect &r,
									  uint16 fond = (uint16)NkRole::PanelBg) {
			const float32 s = r.w < r.h ? r.w : r.h;
			const float32 x = r.x + (r.w - s) * 0.5f, y = r.y + (r.h - s) * 0.5f;
			const float32 u = s / 100.f;
			const float32(&kPattes)[4][6] = NkBrandPattes();
			auto L = [&](float32 ax, float32 ay, float32 bx, float32 by, NkRole role, float32 ep) {
				(void)p.Line(x + ax * u, y + ay * u, x + bx * u, y + by * u, (uint16)role, ep);
			};
			auto D = [&](float32 cx, float32 cy, float32 rr, uint16 role) {
				(void)p.Ellipse({cx - rr, cy - rr, rr * 2.f, rr * 2.f}, role);
			};
			for (int32 cote = 0; cote < 2; ++cote) {
				auto X = [&](float32 v) { return cote == 0 ? v : 100.f - v; };
				for (int32 i = 0; i < 3; ++i)
					L(X(kPattes[i][4]), kPattes[i][5], X(kPattes[i + 1][4]), kPattes[i + 1][5], NkRole::TextMuted,
					  u * 1.4f);
			}
			const float32 epPatte = u * 4.f;
			for (int32 cote = 0; cote < 2; ++cote) {
				auto X = [&](float32 v) { return cote == 0 ? v : 100.f - v; };
				for (int32 i = 0; i < 4; ++i) {
					const float32 *k = kPattes[i];
					L(X(k[0]), k[1], X(k[2]), k[3], NkRole::Text, epPatte);
					L(X(k[2]), k[3], X(k[4]), k[5], NkRole::Text, epPatte);
					D(x + X(k[2]) * u, y + k[3] * u, epPatte * 0.5f, (uint16)NkRole::Text);
				}
			}
			const float32 corps[12] = {x + 50.f * u, y + 36.f * u, x + 62.f * u, y + 43.f * u, x + 62.f * u, y + 57.f * u,
									   x + 50.f * u, y + 64.f * u, x + 38.f * u, y + 57.f * u, x + 38.f * u, y + 43.f * u};
			(void)p.PolygonHex(corps, 6, p.ColorOf((uint16)NkRole::AccentSel));
			const float32 epArete = u * 3.f;
			(void)p.Line(x + 38.f * u, y + 43.f * u, x + 50.f * u, y + 50.f * u, fond, epArete);
			(void)p.Line(x + 50.f * u, y + 50.f * u, x + 62.f * u, y + 43.f * u, fond, epArete);
			(void)p.Line(x + 50.f * u, y + 50.f * u, x + 50.f * u, y + 64.f * u, fond, epArete);
			const float32 rSommet = u * 4.f;
			for (int32 cote = 0; cote < 2; ++cote)
				for (int32 i = 0; i < 4; ++i) {
					const float32 vx = cote == 0 ? kPattes[i][4] : 100.f - kPattes[i][4];
					D(x + vx * u, y + kPattes[i][5] * u, rSommet, (uint16)NkRole::AccentUi);
				}
		}

	} // namespace nk3d
} // namespace nkentseu
