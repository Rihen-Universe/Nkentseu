// ============================================================================
// main.cpp — VUE 2D JETABLE (NKCanvas) du prototype « Spread ».
// ----------------------------------------------------------------------------
// C'est la GLUE jetable : fenêtre + boucle + dessin + clic->case. Elle ne
// contient AUCUNE règle de jeu (tout est dans ConquerorCore, pur).
// Volontairement : PAS de SceneManager, PAS d'ECS, PAS d'AssetManager.
//   -> un seul écran, une grille en dur, une police chargée une fois.
// Le jour où on aurait besoin de scènes/ECS/assets réutilisables = signal pour
// passer à Noge. Ici on court-circuite tout ça avec ~200 lignes à jeter.
// Ce qu'on GARDE, c'est ConquerorCore ; ce fichier, on le jette.
// ============================================================================
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkTime.h"

#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Shapes/NkRectangleShape.h"
#include "NKCanvas/Renderer/Shapes/NkCircleShape.h"
#include "NKCanvas/Renderer/Resources/NkFont.h"     // NkFont
#include "NKCanvas/Renderer/Resources/NkSprite.h"    // NkText (défini ici)

#include "ConquerorProto/ConquerorCore.h"

#include <cstdio>   // snprintf (formatage du HUD — glue jetable, OK)

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

struct Layout { float ox, oy, cell; };

// Calcule le placement du plateau (carré) dans la fenêtre, sous un bandeau HUD.
Layout ComputeLayout(float W, float H) {
	const float top = 84.f;
	float avail = (W < (H - top)) ? W : (H - top);
	avail -= 24.f;
	float cell = avail / static_cast<float>(conq::COLS); // ROWS == COLS == 6
	float bw = cell * conq::COLS, bh = cell * conq::ROWS;
	Layout L;
	L.cell = cell;
	L.ox = (W - bw) * 0.5f;
	L.oy = top + (H - top - bh) * 0.5f;
	return L;
}

// Pixel écran -> (r,c). Renvoie false hors plateau. Vue par défaut = 1:1 pixels,
// donc pas besoin de MapPixelToCoords ici.
bool CellAtPixel(const Layout& L, float px, float py, int& r, int& c) {
	if (px < L.ox || py < L.oy) return false;
	c = static_cast<int>((px - L.ox) / L.cell);
	r = static_cast<int>((py - L.oy) / L.cell);
	return conq::InBounds(r, c);
}

NkColor2D OwnerColor(conq::Owner o) {
	if (o == conq::Owner::P1) return NkColor2D{ 232, 120, 40, 255 };  // orange
	if (o == conq::Owner::P2) return NkColor2D{ 40, 150, 170, 255 };  // teal
	return NkColor2D{ 60, 66, 70, 255 };
}

} // namespace

int nkmain(const NkEntryState& state) {
	(void)state;

	// Harness minimal : on teste le moteur PUR au démarrage (sans rien afficher).
	logger.Info("[ConquerorProto] SelfTest: %s", conq::SelfTest() ? "OK" : "FAIL");

	NkWindowConfig cfg;
	cfg.title     = "Conqueror - Prototype Spread (NKCanvas)";
	cfg.width     = 900;
	cfg.height    = 1000;
	cfg.centered  = true;
	cfg.resizable = true;

	NkWindow window(cfg);
	if (!window.IsOpen()) { logger.Error("[ConquerorProto] creation fenetre echouee"); return -1; }

	NkContextDesc desc;
	desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;   // backend le mieux validé partout
	NkRenderWindow target(window, desc);
	if (!target.IsValid()) { logger.Error("[ConquerorProto] cible de rendu invalide"); return -2; }

	// NkFont existe dans 2 namespaces (nkentseu::NkFont bas niveau vs
	// nkentseu::renderer::NkFont de NKCanvas) -> on qualifie pour lever l'ambiguïté.
	renderer::NkFont font;
	bool hasFont = font.LoadFromFile(*target.GetRenderer(), "Resources/Fonts/Antonio-Bold.ttf");
	if (!hasFont) logger.Warn("[ConquerorProto] police introuvable -> texte desactive (le jeu reste jouable)");

	conq::GameState game = conq::InitGame();
	int selR = -1, selC = -1;   // totem source sélectionné (ou -1)
	int hovR = -1, hovC = -1;   // case survolée (feedback PC)

	auto& events = NkEvents();
	math::NkVec2u lastSize = target.GetSize();

	while (window.IsOpen()) {
		// ── Événements ──────────────────────────────────────────────────────
		while (NkEvent* ev = events.PollEvent()) {
			if (ev->Is<NkWindowCloseEvent>()) { window.Close(); break; }

			if (auto* mm = ev->As<NkMouseMoveEvent>()) {
				math::NkVec2u sz = target.GetSize();
				Layout L = ComputeLayout(static_cast<float>(sz.x), static_cast<float>(sz.y));
				if (!CellAtPixel(L, static_cast<float>(mm->GetX()), static_cast<float>(mm->GetY()), hovR, hovC))
					{ hovR = hovC = -1; }
			}

			if (auto* mb = ev->As<NkMouseButtonPressEvent>()) {
				if (mb->GetButton() == NkMouseButton::NK_MB_LEFT) {
					if (game.finished) {                       // clic = rejouer
						game = conq::InitGame();
						selR = selC = -1;
					} else {
						math::NkVec2u sz = target.GetSize();
						Layout L = ComputeLayout(static_cast<float>(sz.x), static_cast<float>(sz.y));
						int r, c;
						if (CellAtPixel(L, static_cast<float>(mb->GetX()), static_cast<float>(mb->GetY()), r, c)) {
							if (selR < 0) {
								// rien de sélectionné : on sélectionne un totem à soi.
								if (game.cells[r][c].owner == game.current) { selR = r; selC = c; }
							} else if (conq::ApplyDuplicate(game, selR, selC, r, c)) {
								// coup joué.
								selR = selC = -1;
								logger.Info("[ConquerorProto] P1=%d  P2=%d  tour=%d",
								            conq::CountTotems(game, conq::Owner::P1),
								            conq::CountTotems(game, conq::Owner::P2), game.turn);
							} else if (game.cells[r][c].owner == game.current) {
								selR = r; selC = c;             // re-sélection d'un autre totem
							} else {
								selR = selC = -1;              // clic invalide : on désélectionne
							}
						} else {
							selR = selC = -1;
						}
					}
				}
			}
		}
		if (!window.IsOpen()) break;

		// ── Resize (garde-fou WM_SIZE, cf. guide NKCanvas §14.2) ────────────
		math::NkVec2u sz = target.GetSize();
		if (sz.x != lastSize.x || sz.y != lastSize.y) {
			if (lastSize.x != 0 && sz.x > 0 && sz.y > 0) target.OnResize(sz.x, sz.y);
			lastSize = sz;
		}

		// ── Rendu ───────────────────────────────────────────────────────────
		target.Clear(NkColor2D{ 24, 26, 30, 255 });
		float W = static_cast<float>(sz.x), H = static_cast<float>(sz.y);
		Layout L = ComputeLayout(W, H);

		for (int r = 0; r < conq::ROWS; ++r)
		for (int c = 0; c < conq::COLS; ++c) {
			float x = L.ox + c * L.cell, y = L.oy + r * L.cell;

			NkRectangleShape cellRect({ L.cell - 3.f, L.cell - 3.f });
			cellRect.SetPosition({ x + 1.5f, y + 1.5f });
			cellRect.SetFillColor(NkColor2D{ 38, 42, 48, 255 });
			cellRect.SetOutlineColor(NkColor2D{ 58, 64, 72, 255 });
			cellRect.SetOutlineThickness(1.f);
			target.Draw(cellRect);

			bool isSel       = (r == selR && c == selC);
			bool isValidDest = (selR >= 0) && conq::IsLegalDuplicate(game, selR, selC, r, c);
			bool isHover     = (r == hovR && c == hovC);

			if (isValidDest) {                            // destination légale
				NkRectangleShape hl({ L.cell - 3.f, L.cell - 3.f });
				hl.SetPosition({ x + 1.5f, y + 1.5f });
				hl.SetFillColor(NkColor2D{ 60, 120, 70, 120 });
				hl.SetOutlineColor(NkColor2D{ 120, 220, 130, 255 });
				hl.SetOutlineThickness(2.f);
				target.Draw(hl);
			} else if (isHover) {                          // survol
				NkRectangleShape hl({ L.cell - 3.f, L.cell - 3.f });
				hl.SetPosition({ x + 1.5f, y + 1.5f });
				hl.SetFillColor(NkColor2D{ 255, 255, 255, 16 });
				target.Draw(hl);
			}

			const conq::Cell& cd = game.cells[r][c];
			if (!cd.Empty()) {                             // totem
				float rad = L.cell * 0.34f;
				NkCircleShape tot(rad);
				tot.SetPointCount(48);
				tot.SetOrigin({ rad, rad });
				tot.SetPosition({ x + L.cell * 0.5f, y + L.cell * 0.5f });
				tot.SetFillColor(OwnerColor(cd.owner));
				tot.SetOutlineColor(isSel ? NkColor2D::White : NkColor2D{ 0, 0, 0, 160 });
				tot.SetOutlineThickness(isSel ? 4.f : 2.f);
				target.Draw(tot);

				if (hasFont) {                             // HP
					char buf[8];
					int hp = cd.hp < 0 ? 0 : cd.hp;
					std::snprintf(buf, sizeof(buf), "%d", hp);
					NkText hpT(font, buf, static_cast<unsigned>(L.cell * 0.30f));
					hpT.SetFillColor(NkColor2D::White);
					hpT.SetPosition({ x + L.cell * 0.5f - L.cell * 0.10f,
					                  y + L.cell * 0.5f - L.cell * 0.20f });
					// NkText hérite de NkIDrawable2D ET NkDrawable -> on lève
					// l'ambiguïté de Draw() en ciblant le chemin NkDrawable.
					target.Draw(hpT);
				}
			}
		}

		// ── HUD ─────────────────────────────────────────────────────────────
		if (hasFont) {
			int p1 = conq::CountTotems(game, conq::Owner::P1);
			int p2 = conq::CountTotems(game, conq::Owner::P2);
			char line1[128];
			std::snprintf(line1, sizeof(line1), "P1 (orange): %d      P2 (teal): %d", p1, p2);
			NkText t1(font, line1, 26);
			t1.SetFillColor(NkColor2D::White);
			t1.SetPosition({ 20.f, 16.f });
			target.Draw(t1);

			char line2[160];
			if (game.finished) {
				if (game.winner == conq::Owner::None)
					std::snprintf(line2, sizeof(line2), "Partie terminee - Egalite. (clic pour rejouer)");
				else
					std::snprintf(line2, sizeof(line2), "Partie terminee - Vainqueur: Joueur %d. (clic pour rejouer)",
					              game.winner == conq::Owner::P1 ? 1 : 2);
			} else {
				std::snprintf(line2, sizeof(line2),
				              "Tour Joueur %d : clique un de tes totems, puis une case verte.",
				              game.current == conq::Owner::P1 ? 1 : 2);
			}
			NkText t2(font, line2, 20);
			t2.SetFillColor(NkColor2D{ 200, 210, 220, 255 });
			t2.SetPosition({ 20.f, 48.f });
			target.Draw(t2);
		}

		target.Display();
		NkChrono::Sleep(12.f);   // ~cap léger (jeu au tour par tour, pas d'animation)
	}

	window.Close();
	return 0;
}
