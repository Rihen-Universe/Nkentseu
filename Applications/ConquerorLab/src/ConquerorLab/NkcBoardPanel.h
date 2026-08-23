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

					// Remise en configuration de MESURE : tous les sieges a l'IA. Un
					// clic pour retrouver l'etat dans lequel l'atelier repond a une
					// question, quel que soit le bricolage precedent.
					const NkRect bS = {x, b0.y, ctx.S(84.f), b0.h};
					if (Button(ctx, "IA vs IA", bS)) {
						for (uint8 p = 0; p < mS->PlayerCount(); ++p) mS->Player(p).aiModule = 0;
						mS->SetAutoPlay(true);
					}
					x += ctx.S(84.f) + gap;

					// PASSER : n'apparait QUE quand c'est le seul coup legal. Sans ce
					// bouton, un joueur humain bloque se retrouve devant un plateau
					// qui ne repond plus, sans rien lui dire.
					if (mS->IsHumanTurn() && mS->OnlyPass()) {
						const NkRect b3 = {x, b0.y, ctx.S(96.f), b0.h};
						if (Button(ctx, "Passer", b3)) mS->PlayPass();
						x += ctx.S(96.f) + gap;
					}

					// Bascule Humain / IA du siege au trait : c'est le geste qu'on fait
					// dix fois par heure, il n'a pas a passer par un autre panneau.
					const NkcStateView &v	 = mS->DisplayView();
					const uint8			seat = v.current;
					const bool			human = mS->Player(seat).aiModule < 0;
					const NkRect		b4	 = {x, b0.y, ctx.S(120.f), b0.h};
					if (Button(ctx, human ? "Siege : Humain" : "Siege : IA", b4)) {
						mS->Player(seat).aiModule = human ? 0 : -1;
					}
					x += ctx.S(120.f) + gap;

					// Voir le VOISINAGE au survol. Outil de mise au point : c'est le
					// seul moyen de verifier une adjacence a l'oeil, indispensable des
					// qu'un module declare sa propre geometrie.
					const NkRect b5 = {x, b0.y, ctx.S(104.f), b0.h};
					if (Button(ctx, mShowNeighbors ? "Voisinage : on" : "Voisinage", b5))
						mShowNeighbors = !mShowNeighbors;
					x += ctx.S(104.f) + gap;

					// Etat, aligne a droite. QUAND RIEN N'AVANCE, ON DIT POURQUOI :
					// une interface immobile et muette se lit comme une panne.
					char		buf[224];
					const char *idle = mS->IdleReason();
					NkColor		tint = NkcPalette::TextDim();
					if (mS->Cursor() >= 0) {
						std::snprintf(buf, sizeof(buf), "REJEU  coup %d / %u", mS->Cursor() + 1,
									  static_cast<unsigned>(mS->Journal().Size()));
						tint = NkcPalette::Accent();
					} else if (idle && *idle) {
						std::snprintf(buf, sizeof(buf), "%s   —   tour %u", idle, v.turn);
						tint = mS->Thinking() ? NkcPalette::Accent() : NkcPalette::Warn();
					} else {
						std::snprintf(buf, sizeof(buf), "graine %llu   tour %u   coups legaux %u",
									  static_cast<unsigned long long>(mS->Seed()), v.turn,
									  static_cast<unsigned>(mS->Legal().Size()));
					}
					const NkRect info = {x, r.y, r.w - (x - r.x) - ctx.S(10.f), r.h};
					if (info.w > ctx.S(40.f)) NkcTextRight(ctx, info, buf, tint);
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

					// LA GEOMETRIE VIENT DU MODULE QUAND IL LA DECLARE (ABI 3) :
					// centre et forme de chaque cellule. Sinon, projection standard
					// de la topologie. Le projecteur est reconstruit a chaque frame —
					// le garder obligerait a l'invalider au changement de module.
					const NkcProjector proj =
						NkcMakeProjector(mS->Vt(), mS->RulesInst(), v.topology, mS->CellShape());

					// Cible tactile : sur telephone, une case sous ~22 px de rayon
					// n'est plus atteignable au doigt (HANDOFF §2.5).
					const float32 minCell = ctx.S(11.f);
					mLayout = ProjFitBoard(proj, v.coords, v.cellCount, area, 0.07f, minCell);

					// ---- picking, avant le dessin ----------------------------
					const bool interactive = (mS->Cursor() < 0) && ctx.popupDepth == 0;
					int32	   hover	   = -1;
					if (interactive && ctx.InputHits(area))
						hover = ProjPickCell(proj, mLayout, v.coords, v.cellCount,
											 ctx.input.mousePos);

					// ---- surbrillances calculees par le MOTEUR ---------------
					const NkVector<NkcPreview> &prev = mS->Previews();
					int32						hoverPrev = -1;
					if (hover >= 0)
						for (usize i = 0; i < prev.Size(); ++i)
							if (CoordEqual(prev[i].to, v.coords[hover])) { hoverPrev = static_cast<int32>(i); break; }

					// UNE SEULE decoupe pour tout le plateau.
					dl.PushClipRect(area, true);

					NkVec2 poly[kMaxPolyPoints];
					for (uint32 i = 0; i < v.cellCount; ++i) {
						ctx.PushId(&v.coords[i]);	// identite stable si un widget vient un jour ici

						const NkcCoord	  c	   = v.coords[i];
						const NkcCellView &cell = v.cells[i];
						const int32		  n	   = ProjCellPolygon(proj, mLayout, c, poly,
																 static_cast<int32>(kMaxCellPoints), 0.93f);
						const NkVec2	  ctr  = ProjPixelCenter(proj, mLayout, c);

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
								const int32 n = ProjCellPolygon(proj, mLayout, v.coords[si], poly,
																static_cast<int32>(kMaxCellPoints), 0.93f);
								NkcPolyOutline(dl, poly, n, NkcPalette::Accent(), ctx.S(2.5f));
							}
						}
						// ---- CE QUE LE COUP FERA, PAS SEULEMENT OU IL VA ----
						//
						// L'anneau vert seul repondait « on peut cliquer la ». Il ne
						// disait ni QUOI s'y passerait, ni -- pour une fusion -- QUELLES
						// CASES seraient consommees. Le stagiaire voyait « FUSIONNER »
						// apparaitre dans le menu des regles et concluait qu'il manquait
						// un bouton : rien a l'ecran ne designait la case du RESULTAT,
						// et personne ne devine une case.
						//
						// Trois traits, et le geste devient lisible SANS SURVOL — c'est
						// la condition : ce qui n'apparait qu'au survol n'existe pas
						// pour qui ne sait pas ou passer la souris.
						//   1. les cases CONSOMMEES prennent un anneau orange et un
						//      trait vers le resultat ;
						//   2. la case RESULTAT prend un anneau plein, plus epais ;
						//   3. une etiquette dit le genre du coup (D / F3 / P2).
						for (usize i = 0; i < prev.Size(); ++i) {
							const NkcPreview &pv  = prev[i];
							const NkVec2	  cto = ProjPixelCenter(proj, mLayout, pv.to);

							if (pv.kind == NkcMoveKind::Fuse) {
								for (int32 k = 0; k < pv.partCount; ++k) {
									if (CoordEqual(pv.parts[k], pv.to)) continue;
									const NkVec2 cp = ProjPixelCenter(proj, mLayout, pv.parts[k]);
									dl.AddLine(cp, cto, NkcFade(NkcPalette::LastMove(), 0.75f), ctx.S(2.f));
									NkcRing(dl, cp, mLayout.cell * 0.70f,
												NkcFade(NkcPalette::LastMove(), 0.85f), ctx.S(2.f));
								}
								// Le resultat : plein, et plus gros que les participantes.
								NkcRing(dl, cto, mLayout.cell * 0.58f, NkcPalette::LastMove(), ctx.S(3.5f));
							} else {
								NkcRing(dl, cto, mLayout.cell * 0.60f, NkcPalette::MoveLegal(), ctx.S(2.5f));
							}

							// Etiquette du genre. Courte : elle tient dans une case, et elle
							// n'est la que pour distinguer deux anneaux voisins.
							char tag[8];
							if (pv.kind == NkcMoveKind::Fuse)
								std::snprintf(tag, sizeof(tag), "F%d", pv.partCount);
							else if (pv.kind == NkcMoveKind::Power)
								std::snprintf(tag, sizeof(tag), "P%d", static_cast<int32>(pv.powerId));
							else
								std::snprintf(tag, sizeof(tag), "D");
							const NkRect lab = {cto.x - mLayout.cell, cto.y + mLayout.cell * 0.42f,
															mLayout.cell * 2.f, NkcLineH(ctx)};
							NkcTextCenter(ctx, lab, tag, NkcPalette::Text());
						}

						// UNE MEME CASE, PLUSIEURS COUPS : le dire AVANT le clic. Sans cela
						// le joueur clique, un coup part, et il ne saura jamais que
						// l'autre existait.
						for (usize i = 0; i < prev.Size(); ++i) {
							int32 partages = 0;
							for (usize j = 0; j < prev.Size(); ++j)
								if (CoordEqual(prev[j].to, prev[i].to)) ++partages;
							if (partages < 2) continue;
							const NkVec2 c = ProjPixelCenter(proj, mLayout, prev[i].to);
							NkcRing(dl, c, mLayout.cell * 0.80f, NkcFade(NkcPalette::Accent(), 0.9f), ctx.S(2.f));
							char n[8];
							std::snprintf(n, sizeof(n), "x%d", partages);
							const NkRect lab = {c.x - mLayout.cell, c.y - mLayout.cell * 0.95f,
															mLayout.cell * 2.f, NkcLineH(ctx)};
							NkcTextCenter(ctx, lab, n, NkcPalette::Accent());
						}
						// ennemis qui seraient retournes par le coup SURVOLE
						if (hoverPrev >= 0) {
							const NkcPreview &pv = prev[static_cast<usize>(hoverPrev)];
							for (int32 f = 0; f < pv.flipCount; ++f) {
								const NkVec2 c = ProjPixelCenter(proj, mLayout, pv.flips[f]);
								NkcRing(dl, c, mLayout.cell * 0.78f, NkcPalette::MoveThreat(), ctx.S(3.f));
								NkcRing(dl, c, mLayout.cell * 0.66f,
										NkcFade(NkcPalette::MoveThreat(), 0.45f), ctx.S(2.f));
							}
						}
					}

					// ---- voisinage de la case survolee -----------------------
					// SEUL moyen de VOIR une adjacence, donc de la deboguer. Un
					// stagiaire dont GetNeighbors est faux le constate ici en une
					// seconde ; sans cela, il le decouvrirait par un coup legal
					// bizarre, trois heures plus tard.
					//
					// Optionnel et hors du chemin normal : on n'encombre pas la
					// lecture tactique de traits qui n'ont rien a y faire.
					if (mShowNeighbors && hover >= 0) {
						const NkVec2 from = ProjPixelCenter(proj, mLayout, v.coords[hover]);
						NkcCoord	 nb[32];
						const uint32 n = ProjNeighbors(proj, v.topology, v.coords[hover], nb, 32);
						for (uint32 k = 0; k < n && k < 32; ++k) {
							// Un voisin peut sortir du plateau : la topologie ne
							// connait pas les bords. On ne trace que ce qui existe.
							if (FindCoord(v, nb[k]) < 0) continue;
							const NkVec2 to = ProjPixelCenter(proj, mLayout, nb[k]);
							dl.AddLine(from, to, NkcFade(NkcPalette::Accent(), 0.55f), ctx.S(1.5f));
							NkcRing(dl, to, mLayout.cell * 0.34f,
									NkcFade(NkcPalette::Accent(), 0.75f), ctx.S(1.5f));
						}
						NkcRing(dl, from, mLayout.cell * 0.44f, NkcPalette::Accent(), ctx.S(2.f));
					}

					// ---- dernier coup : anneau qui pulse puis s'eteint --------
					const NkcMoveEcho &e = mS->Echo();
					if (e.valid && e.age < kEchoTime && mS->Cursor() < 0) {
						const float32 t	   = e.age / kEchoTime;
						const float32 fade = 1.f - t;
						const float32 puls = 0.62f + 0.22f * math::NkSin(mTime * 9.f) * fade;
						const NkVec2  c	   = ProjPixelCenter(proj, mLayout, e.to);
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

					// ---- QUEL COUP ? ----------------------------------------
					// Le menu se dessine APRES le plateau et AVANT la prise du clic :
					// tant qu'il est ouvert, le plateau ne repond plus. Un clic qui
					// traverserait un menu ouvert jouerait le coup que le joueur
					// croyait etre en train de choisir.
					const bool choixOuvert = DrawChoix(ctx, area, proj);

					// ---- clic ------------------------------------------------
					if (!choixOuvert && interactive && hover >= 0) {
						const NkVec2 ctr = ProjPixelCenter(proj, mLayout, v.coords[hover]);
						const NkRect box = {ctr.x - mLayout.cell, ctr.y - mLayout.cell, mLayout.cell * 2.f,
											mLayout.cell * 2.f};
						if (ctx.ClickIn(box)) mS->ClickCell(v.coords[hover]);
					}
				}

				// -------------------------------------------------------------
				// PLUSIEURS COUPS VISENT LA CASE CLIQUEE : on DEMANDE lequel.
				//
				// C'est le second defaut de l'interaction, et le plus difficile a
				// voir : un POUVOIR est designe par (lanceur, cible, powerId). Deux
				// pouvoirs du meme totem sur la meme cible partagent donc tout sauf
				// leur identifiant. L'interface jouait « le premier coup qui
				// correspond » : le second pouvoir figurait dans la liste des coups
				// legaux, l'IA pouvait le jouer, et l'humain ne pouvait PAS. Meme
				// histoire pour deux fusions dont le groupe differe mais dont la case
				// resultat est la meme.
				//
				// Mesure : Applications/ConquerorLab/tests/ambiguite.ps1.
				// Renvoie vrai tant que le menu est ouvert : le plateau est alors gele.
				bool DrawChoix(NkGuiContext &ctx, const NkRect &area, const NkcProjector &proj) noexcept {
					const int32 n = mS->ChoixCount();
					if (n <= 0) return false;

					NkGuiDrawList &dl	= ctx.DL();
					const float32  lh	= NkcLineH(ctx);
					const float32  rowH = lh + ctx.S(10.f);
					const float32  w	= ctx.S(232.f);
					const float32  h	= rowH * static_cast<float32>(n + 2) + ctx.S(14.f);

					// Ancre sur la case cliquee, puis rabattu dans le cadre : un menu a
					// moitie hors du panneau est un menu qu'on ne peut plus fermer.
					const NkVec2 c	 = ProjPixelCenter(proj, mLayout, mS->ChoixAt());
					NkRect		 box = {c.x + mLayout.cell * 0.6f, c.y - h * 0.4f, w, h};
					if (box.x + box.w > area.x + area.w) box.x = c.x - mLayout.cell * 0.6f - w;
					if (box.x < area.x) box.x = area.x + ctx.S(4.f);
					if (box.y < area.y) box.y = area.y + ctx.S(4.f);
					if (box.y + box.h > area.y + area.h) box.y = area.y + area.h - box.h - ctx.S(4.f);

					dl.AddRectFilled(box, NkcFade(NkcPalette::BgPrimary(), 0.97f), ctx.theme.rounding);
					dl.AddRect(box, NkcPalette::Accent(), ctx.S(1.5f));
					NkcTextCenter(ctx, {box.x, box.y + ctx.S(6.f), box.w, lh}, "Quel coup ?", NkcPalette::Text());

					float32 y = box.y + ctx.S(6.f) + rowH;
					for (int32 i = 0; i < n; ++i) {
						char nom[48];
						NkcSession::NommerCoup(mS->ChoixMove(i), nom, sizeof(nom));

						// Ce que le coup RETOURNE : deja simule, il suffit de le retrouver.
						char		ligne[80];
						const int32 flips = FlipsDe(mS->Previews(), mS->ChoixIndex(i));
						if (flips > 0) std::snprintf(ligne, sizeof(ligne), "%s  (retourne %d)", nom, flips);
						else		   std::snprintf(ligne, sizeof(ligne), "%s", nom);

						const NkRect r = {box.x + ctx.S(8.f), y, box.w - ctx.S(16.f), rowH - ctx.S(4.f)};
						if (Button(ctx, ligne, r)) { mS->JouerChoix(i); return true; }
						y += rowH;
					}
					const NkRect ra = {box.x + ctx.S(8.f), y, box.w - ctx.S(16.f), rowH - ctx.S(4.f)};
					if (Button(ctx, "Annuler", ra)) mS->AnnulerChoix();
					return true;
				}

				static int32 FlipsDe(const NkVector<NkcPreview> &prev, int32 moveIndex) noexcept {
					for (usize i = 0; i < prev.Size(); ++i)
						if (prev[i].moveIndex == static_cast<uint32>(moveIndex)) return prev[i].flipCount;
					return 0;
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
				bool		   mShowNeighbors = false;
		};

	} // namespace conqueror
} // namespace nkentseu
