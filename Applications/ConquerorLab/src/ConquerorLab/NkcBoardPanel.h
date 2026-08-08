#pragma once
// =============================================================================
// NkcBoardPanel — le plateau. Le heros de l'interface (HANDOFF §2.2) : il occupe
// le centre, il est grand, et il est le seul element vivement colore.
//
// CE QUE CE PANNEAU MONTRE, ET POURQUOI
// -------------------------------------
//   bandeau de score  qui mene, avec quelles ressources, et QUI est au trait
//   plateau           l'etat, cadre automatiquement, sans reglage de zoom
//   anneaux verts     les destinations legales de la source selectionnee
//   halos rouges      les ennemis que CE coup retournerait — la lecture tactique
//                     centrale du jeu (« quelle surface de contact suis-je en
//                     train d'offrir ? », NOTE_DESIGN §2)
//   anneau orange     le dernier coup, qui pulse puis s'eteint
//   CASCADE xN        le sommet emotionnel (PXG 1) : il doit se voir
//
// DEUX POINTS TECHNIQUES QUI COMPTENT
//
//   Le picking est en O(1), pas en O(cases) : `PixelToCoord` inverse la
//   projection (arrondi cube pour l'hexagone), puis on verifie que la
//   coordonnee existe. Tester 42 rectangles donnerait le meme resultat en
//   dessinant des losanges au lieu d'hexagones pres des aretes.
//
//   UNE SEULE decoupe pour tout le plateau. Chaque PushClipRect ouvre une
//   commande de dessin, donc un appel GPU : quarante-deux decoupes couteraient
//   quarante-deux draw calls pour afficher quarante-deux hexagones.
// =============================================================================

#include "NKEditorKit/NkEditorKit.h"

#include "ConquerorLab/NkcSession.h"
#include "ConquerorLab/NkcBoardRender.h"
#include "ConquerorLab/NkcLabTheme.h"
#include "ConquerorLab/NkcDraw.h"

#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		class NkcBoardPanel : public NkEditorPanel {
			public:
				explicit NkcBoardPanel(NkcSession *s) noexcept
					: NkEditorPanel("Plateau", NkEditorDockSide::NK_CENTER), mS(s) {}

				void OnUI(NkEditorFrameContext &ec) override {
					NkGuiContext &ctx = ec.Ui();
					mTime += ec.dt;

					const NkRect vis = NkcVisibleRect(ctx);
					if (vis.w < 60.f || vis.h < 60.f) return;
					ctx.NextItemRect(vis.w, vis.h);	 // reserve la place : contenu == visible

					if (!mS || !mS->Ready()) {
						NkcTextCenter(ctx, vis, "Aucun moteur de regles jouable — voir le panneau Modules.",
									  NkcPalette::Error());
						return;
					}

					const float32 pad	 = ctx.S(10.f);
					const float32 barH	 = ctx.S(34.f);
					const float32 scoreH = ctx.S(62.f);

					const NkRect toolbar = {vis.x + pad, vis.y + pad, vis.w - 2.f * pad, barH};
					const NkRect score	 = {vis.x + pad, toolbar.y + barH + ctx.S(6.f), vis.w - 2.f * pad, scoreH};
					NkRect		 board	 = {vis.x + pad, score.y + scoreH + ctx.S(8.f), vis.w - 2.f * pad,
											vis.y + vis.h - (score.y + scoreH) - ctx.S(8.f) - pad};
					if (board.h < ctx.S(80.f)) board.h = ctx.S(80.f);

					DrawToolbar(ctx, toolbar);
					DrawScoreBand(ctx, score);
					DrawBoard(ctx, board);
				}

			private:
				// -------------------------------------------------------------
				// Barre d'actions : ce qu'on veut a portee de pouce pendant qu'on
				// regarde une partie tourner.
				// -------------------------------------------------------------
				void DrawToolbar(NkGuiContext &ctx, const NkRect &r) noexcept {
					NkGuiDrawList &dl = ctx.DL();
					dl.AddRectFilled(r, NkcPalette::Track(), ctx.theme.rounding);

					const float32 gap = ctx.S(6.f);
					const float32 bw  = ctx.S(112.f);
					float32		  x	  = r.x + gap;
					const NkRect  b0  = {x, r.y + ctx.S(3.f), bw, r.h - ctx.S(6.f)};
					if (Button(ctx, "Nouvelle partie", b0)) mS->NewGame();
					x += bw + gap;

					const NkRect b1 = {x, b0.y, ctx.S(88.f), b0.h};
					if (Button(ctx, mS->AutoPlay() ? "Pause" : "Lecture", b1))
						mS->SetAutoPlay(!mS->AutoPlay());
					x += ctx.S(88.f) + gap;

					const NkRect b2 = {x, b0.y, ctx.S(88.f), b0.h};
					if (Button(ctx, "Pas a pas", b2)) mS->StepOnce();
					x += ctx.S(88.f) + gap;

					// PASSER : n'apparait QUE quand c'est le seul coup legal. Sans ce
					// bouton, un joueur humain bloque se retrouve devant un plateau
					// qui ne repond plus, sans rien lui dire.
					if (mS->IsHumanTurn() && mS->OnlyPass()) {
						const NkRect b3 = {x, b0.y, ctx.S(96.f), b0.h};
						if (Button(ctx, "Passer", b3)) mS->PlayPass();
						x += ctx.S(96.f) + gap;
					}

					// Etat, aligne a droite : graine, tour, empreinte.
					char buf[192];
					const NkcStateView &v = mS->DisplayView();
					if (mS->Cursor() >= 0) {
						std::snprintf(buf, sizeof(buf), "REJEU  coup %d / %u", mS->Cursor() + 1,
									  static_cast<unsigned>(mS->Journal().Size()));
					} else if (mS->Thinking()) {
						std::snprintf(buf, sizeof(buf), "Reflexion...   tour %u", v.turn);
					} else {
						std::snprintf(buf, sizeof(buf), "graine %llu   tour %u   coups legaux %u",
									  static_cast<unsigned long long>(mS->Seed()), v.turn,
									  static_cast<unsigned>(mS->Legal().Size()));
					}
					const NkRect info = {x, r.y, r.w - (x - r.x) - ctx.S(10.f), r.h};
					if (info.w > ctx.S(40.f))
						NkcTextRight(ctx, info, buf,
									 mS->Cursor() >= 0 ? NkcPalette::Accent() : NkcPalette::TextDim());
				}

				// -------------------------------------------------------------
				// Bandeau de score : une tuile par joueur. Le joueur au trait est
				// souligne d'un filet orange — jamais code par la seule couleur du
				// texte, qui se perd des qu'on regarde le plateau.
				// -------------------------------------------------------------
				void DrawScoreBand(NkGuiContext &ctx, const NkRect &r) noexcept {
					const NkcStateView &v	= mS->DisplayView();
					NkGuiDrawList	   &dl	= ctx.DL();
					const int32			n	= v.playerCount > 0 ? v.playerCount : 1;
					const float32		gap = ctx.S(8.f);
					const float32		tw	= (r.w - gap * static_cast<float32>(n - 1)) / static_cast<float32>(n);

					for (int32 p = 0; p < n; ++p) {
						const NkRect  t	  = {r.x + (tw + gap) * static_cast<float32>(p), r.y, tw, r.h};
						const NkColor col = NkcPalette::Player(p);
						const bool	  cur = (!v.finished && v.current == static_cast<uint8>(p));

						dl.AddRectFilled(t, NkcPalette::Panel(), ctx.theme.rounding);
						dl.AddRect(t, cur ? NkcPalette::Accent() : NkcPalette::Border(), 1.f);
						if (cur)   // filet du joueur au trait
							dl.AddRectFilled({t.x, t.y + t.h - ctx.S(3.f), t.w, ctx.S(3.f)},
											 NkcPalette::Accent(), 0.f);

						// pastille de couleur
						const float32 dotR = ctx.S(9.f);
						const NkVec2  dot  = {t.x + ctx.S(16.f), t.y + ctx.S(18.f)};
						dl.AddCircleFilled(dot, dotR, col);
						dl.AddCircleFilled(dot, dotR * 0.45f, NkcMix(col, NkcPalette::Text(), 0.55f));

						char buf[96];
						std::snprintf(buf, sizeof(buf), "Joueur %d", p);
						NkcText(ctx, t.x + ctx.S(32.f), t.y + ctx.S(8.f), buf,
								cur ? NkcPalette::Text() : NkcPalette::TextDim());

						const int32 totems = v.totemCount ? v.totemCount[p] : 0;
						std::snprintf(buf, sizeof(buf), "%d", totems);
						const NkRect big = {t.x + ctx.S(32.f), t.y + ctx.S(24.f), ctx.S(46.f), ctx.S(22.f)};
						NkcText(ctx, big.x, big.y, buf, col);

						// Energie et Points de Conquete. Les PC transitent en DIXIEMES
						// entiers (REGLES §11.2) : la division par 10 est un fait
						// d'AFFICHAGE, elle n'existe nulle part dans la logique.
						const int32 energy = v.energy ? v.energy[p] : 0;
						const int32 pc10   = v.conquestTenths ? v.conquestTenths[p] : 0;
						std::snprintf(buf, sizeof(buf), "E %d    PC %d,%d", energy, pc10 / 10,
									  (pc10 < 0 ? -pc10 : pc10) % 10);
						NkcText(ctx, t.x + ctx.S(78.f), t.y + ctx.S(30.f), buf, NkcPalette::TextDim(),
								t.w - ctx.S(86.f));
					}
				}

				// -------------------------------------------------------------
				void DrawBoard(NkGuiContext &ctx, const NkRect &area) noexcept {
					const NkcStateView &v = mS->DisplayView();
					if (!v.cells || !v.coords || v.cellCount == 0) return;

					NkGuiDrawList &dl = ctx.DL();
					dl.AddRectFilled(area, NkcPalette::Track(), ctx.theme.rounding);

					// Cible tactile : sur telephone, une case sous ~22 px de rayon
					// n'est plus atteignable au doigt (HANDOFF §2.5).
					const float32 minCell = ctx.S(11.f);
					mLayout = FitBoard(v.topology, v.coords, v.cellCount, area, 0.07f, minCell);

					// ---- picking, avant le dessin ----------------------------
					const bool interactive = (mS->Cursor() < 0) && ctx.popupDepth == 0;
					int32	   hover	   = -1;
					if (interactive && ctx.InputHits(area)) {
						const NkcCoord c = PixelToCoord(mLayout, ctx.input.mousePos);
						hover			 = FindCoord(v, c);
					}

					// ---- surbrillances calculees par le MOTEUR ---------------
					const NkVector<NkcPreview> &prev = mS->Previews();
					int32						hoverPrev = -1;
					if (hover >= 0)
						for (usize i = 0; i < prev.Size(); ++i)
							if (CoordEqual(prev[i].to, v.coords[hover])) { hoverPrev = static_cast<int32>(i); break; }

					// UNE SEULE decoupe pour tout le plateau.
					dl.PushClipRect(area, true);

					NkVec2 poly[8];
					for (uint32 i = 0; i < v.cellCount; ++i) {
						ctx.PushId(&v.coords[i]);	// identite stable si un widget vient un jour ici

						const NkcCoord	  c	   = v.coords[i];
						const NkcCellView &cell = v.cells[i];
						const int32		  n	   = CellPolygon(mLayout, c, poly, 0.93f);
						const NkVec2	  ctr  = CoordToPixel(mLayout, c);

						if (cell.owner == kCellBlocked) {
							NkcPolyFilled(dl, poly, n, NkcPalette::CellBlocked());
						} else {
							NkColor fill = NkcPalette::CellEmpty();
							if (static_cast<int32>(i) == hover)
								fill = NkcMix(fill, NkcPalette::ButtonHover(), 0.45f);
							NkcPolyFilled(dl, poly, n, fill);
							NkcPolyOutline(dl, poly, n, NkcPalette::CellEdge(), ctx.S(1.5f));
						}

						// ---- totem ------------------------------------------
						if (cell.owner >= 0) {
							// Le niveau se lit a la TAILLE et au liseré, jamais a la
							// seule couleur : la couleur porte deja le proprietaire.
							const float32 lvl = static_cast<float32>(cell.level < 0 ? 0 : cell.level);
							const float32 rad = mLayout.cell * (0.52f + 0.06f * lvl);
							const NkColor col = NkcPalette::Player(cell.owner);
							dl.AddCircleFilled(ctr, rad, col);
							dl.AddCircleFilled(ctr, rad * 0.68f, NkcMix(col, NkcPalette::BgPrimary(), 0.28f));
							if (cell.level > 0)
								NkcRing(dl, ctr, rad * 0.86f, NkcMix(col, NkcPalette::Text(), 0.7f),
										ctx.S(1.f) + static_cast<float32>(cell.level) * 0.6f);
						}

						ctx.PopId();
					}

					// ---- couche de lecture tactique --------------------------
					if (mS->Cursor() < 0) {
						// source selectionnee
						if (mS->HasSelection()) {
							const int32 si = FindCoord(v, mS->Selection());
							if (si >= 0) {
								const int32 n = CellPolygon(mLayout, v.coords[si], poly, 0.93f);
								NkcPolyOutline(dl, poly, n, NkcPalette::Accent(), ctx.S(2.5f));
							}
						}
						// destinations legales
						for (usize i = 0; i < prev.Size(); ++i) {
							const NkVec2 c = CoordToPixel(mLayout, prev[i].to);
							NkcRing(dl, c, mLayout.cell * 0.60f, NkcPalette::MoveLegal(), ctx.S(2.5f));
						}
						// ennemis qui seraient retournes par le coup SURVOLE
						if (hoverPrev >= 0) {
							const NkcPreview &pv = prev[static_cast<usize>(hoverPrev)];
							for (int32 f = 0; f < pv.flipCount; ++f) {
								const NkVec2 c = CoordToPixel(mLayout, pv.flips[f]);
								NkcRing(dl, c, mLayout.cell * 0.78f, NkcPalette::MoveThreat(), ctx.S(3.f));
								NkcRing(dl, c, mLayout.cell * 0.66f,
										NkcFade(NkcPalette::MoveThreat(), 0.45f), ctx.S(2.f));
							}
						}
					}

					// ---- dernier coup : anneau qui pulse puis s'eteint --------
					const NkcMoveEcho &e = mS->Echo();
					if (e.valid && e.age < kEchoTime && mS->Cursor() < 0) {
						const float32 t	   = e.age / kEchoTime;
						const float32 fade = 1.f - t;
						const float32 puls = 0.62f + 0.22f * math::NkSin(mTime * 9.f) * fade;
						const NkVec2  c	   = CoordToPixel(mLayout, e.to);
						NkcRing(dl, c, mLayout.cell * puls, NkcFade(NkcPalette::LastMove(), fade),
								ctx.S(3.f));
					}

					dl.PopClipRect();

					// ---- CASCADE : hors decoupe, il doit pouvoir deborder -----
					if (e.valid && e.cascade >= 2 && e.age < kCascadeTime) {
						const float32 t	   = e.age / kCascadeTime;
						char		  buf[48];
						std::snprintf(buf, sizeof(buf), "CASCADE  x%d", e.cascade);
						const float32 w	   = NkcTextW(ctx, buf) + ctx.S(28.f);
						const float32 h	   = NkcLineH(ctx) + ctx.S(14.f);
						const NkRect  box  = {area.x + (area.w - w) * 0.5f,
											  area.y + area.h * 0.34f - t * ctx.S(38.f), w, h};
						const float32 fade = 1.f - t;
						dl.AddRectFilled(box, NkcFade(NkcPalette::MoveThreat(), 0.86f * fade),
										 ctx.theme.rounding);
						NkcTextCenter(ctx, box, buf, NkcFade(NkcPalette::Text(), fade));
					}

					// ---- occupation + fin de partie --------------------------
					DrawOccupancy(ctx, area, v);
					if (v.finished) DrawVerdict(ctx, area, v);

					// ---- clic ------------------------------------------------
					if (interactive && hover >= 0) {
						const NkVec2 ctr = CoordToPixel(mLayout, v.coords[hover]);
						const NkRect box = {ctr.x - mLayout.cell, ctr.y - mLayout.cell, mLayout.cell * 2.f,
											mLayout.cell * 2.f};
						if (ctx.ClickIn(box)) mS->ClickCell(v.coords[hover]);
					}
				}

				// -------------------------------------------------------------
				void DrawOccupancy(NkGuiContext &ctx, const NkRect &area, const NkcStateView &v) noexcept {
					uint32 usable = 0, taken = 0;
					for (uint32 i = 0; i < v.cellCount; ++i) {
						if (v.cells[i].owner == kCellBlocked) continue;
						++usable;
						if (v.cells[i].owner >= 0) ++taken;
					}
					if (usable == 0) return;

					const float32 h	 = ctx.S(6.f);
					const NkRect  tr = {area.x + ctx.S(10.f), area.y + area.h - h - ctx.S(8.f),
										area.w - ctx.S(20.f), h};
					NkGuiDrawList &dl = ctx.DL();
					dl.AddRectFilled(tr, NkcPalette::BgPrimary(), h * 0.5f);

					// Une portion par joueur : la barre dit AUSSI qui occupe le terrain.
					float32 x = tr.x;
					for (uint32 p = 0; p < v.playerCount; ++p) {
						uint32 own = 0;
						for (uint32 i = 0; i < v.cellCount; ++i)
							if (v.cells[i].owner == static_cast<int8>(p)) ++own;
						const float32 w = tr.w * (static_cast<float32>(own) / static_cast<float32>(usable));
						if (w > 0.5f) dl.AddRectFilled({x, tr.y, w, tr.h}, NkcPalette::Player(static_cast<int32>(p)), h * 0.5f);
						x += w;
					}

					char buf[64];
					std::snprintf(buf, sizeof(buf), "%u / %u cases", static_cast<unsigned>(taken),
								  static_cast<unsigned>(usable));
					const NkRect lab = {tr.x, tr.y - NkcLineH(ctx) - ctx.S(2.f), tr.w, NkcLineH(ctx)};
					NkcTextRight(ctx, lab, buf, NkcPalette::TextDim());
				}

				void DrawVerdict(NkGuiContext &ctx, const NkRect &area, const NkcStateView &v) noexcept {
					char title[64];
					if (v.winner >= 0) std::snprintf(title, sizeof(title), "Vainqueur : Joueur %d", v.winner);
					else if (v.winner == -1) std::snprintf(title, sizeof(title), "Match nul");
					else return;

					char detail[128];
					int32 off = 0;
					off += std::snprintf(detail + off, sizeof(detail) - static_cast<usize>(off), "totems ");
					for (uint32 p = 0; p < v.playerCount && off > 0 && static_cast<usize>(off) < sizeof(detail); ++p)
						off += std::snprintf(detail + off, sizeof(detail) - static_cast<usize>(off), "%s%d",
											 p ? " - " : "", v.totemCount ? v.totemCount[p] : 0);

					const float32 w	  = (NkcTextW(ctx, title) > NkcTextW(ctx, detail) ? NkcTextW(ctx, title)
																					 : NkcTextW(ctx, detail)) +
									  ctx.S(52.f);
					const float32 h	  = NkcLineH(ctx) * 2.f + ctx.S(26.f);
					const NkRect  box = {area.x + (area.w - w) * 0.5f, area.y + (area.h - h) * 0.5f, w, h};

					NkGuiDrawList &dl = ctx.DL();
					dl.AddRectFilled(box, NkcFade(NkcPalette::BgPrimary(), 0.94f), ctx.theme.rounding);
					dl.AddRect(box, NkcPalette::Accent(), ctx.S(1.5f));
					NkcTextCenter(ctx, {box.x, box.y + ctx.S(8.f), box.w, NkcLineH(ctx)}, title,
								  NkcPalette::Text());
					NkcTextCenter(ctx, {box.x, box.y + ctx.S(10.f) + NkcLineH(ctx), box.w, NkcLineH(ctx)},
								  detail, NkcPalette::TextDim());
				}

				static int32 FindCoord(const NkcStateView &v, NkcCoord c) noexcept {
					for (uint32 i = 0; i < v.cellCount; ++i)
						if (CoordEqual(v.coords[i], c)) return static_cast<int32>(i);
					return -1;
				}

			private:
				NkcSession	  *mS = nullptr;
				NkcBoardLayout mLayout;
				float32		   mTime = 0.f;
		};

	} // namespace conqueror
} // namespace nkentseu
