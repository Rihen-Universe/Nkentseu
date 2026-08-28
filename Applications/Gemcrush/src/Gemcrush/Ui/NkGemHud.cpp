// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemHud.cpp
// DESCRIPTION: Implémentation de la mise en page et de l'interface.
//
//              ⚠️ DETTE ASSUMÉE, ET ELLE A UN PROPRIÉTAIRE : les six aides de
//              texte ci-dessous (Text / TextCentered / TextRight / MeasureW)
//              sont la TROISIÈME copie du même code dans le dépôt, après
//              Applications/Mou/src/Mou/UI/MouDraw.h et Nkoung. Leur vraie
//              place est NKGui, à côté de NkGuiDrawList::AddText, qui ne sait
//              écrire qu'à la LIGNE DE BASE — c'est ce manque qui fait que
//              chaque application les réécrit. À promouvoir dans NKGui sous le
//              nom NkGuiDrawText.h ; tant que ce n'est pas fait, toute nouvelle
//              application paiera la même copie.
//
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Gemcrush/Ui/NkGemHud.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			using nkgui::NkGuiDrawList;
			using nkgui::NkGuiFont;
			using math::NkVec2f;
			using math::NkVec2i;

			namespace {

				// =====================================================
				// Aides de texte (voir la dette signalée en tête de fichier)
				// =====================================================
				void Text(NkGuiDrawList &dl, NkGuiFont *f, float32 x, float32 topY, const char *s, const NkColor &c,
						  float32 maxWidth = -1.f) {
					if (f != nullptr && f->Face() != nullptr && s != nullptr) {
						dl.AddText(f->Face(), f->TexId(), NkVec2f(x, topY + f->Ascent()), s, c, maxWidth);
					}
				}

				float32 MeasureW(NkGuiFont *f, const char *s) {
					return (f != nullptr && s != nullptr) ? f->MeasureWidth(s) : 0.f;
				}

				void TextCentered(NkGuiDrawList &dl, NkGuiFont *f, const NkRect &box, float32 topY, const char *s,
								  const NkColor &c) {
					Text(dl, f, box.x + (box.w - MeasureW(f, s)) * 0.5f, topY, s, c);
				}

				void TextRight(NkGuiDrawList &dl, NkGuiFont *f, float32 right, float32 topY, const char *s,
							   const NkColor &c) {
					Text(dl, f, right - MeasureW(f, s), topY, s, c);
				}

				float32 LineH(NkGuiFont *f, float32 fallback) {
					return (f != nullptr && f->Face() != nullptr) ? f->LineHeight() : fallback;
				}

				// =====================================================
				// Panneau : ombre + dégradé + liseré + filet clair.
				//
				// Ces quatre couches sont ce qui sépare un « rectangle
				// coloré » d'un panneau d'interface de jeu. Aucune n'est
				// décorative : l'ombre détache du fond, le dégradé donne
				// une direction de lumière, le liseré ferme la forme, le
				// filet clair simule le biseau du bord haut.
				// =====================================================
				void Panel(NkGuiDrawList &dl, const NkRect &r, float32 radius, const NkGemTheme &th,
						   float32 opacity = 1.f) {
					if (r.w <= 0.f || r.h <= 0.f) {
						return;
					}
					const float32 lift = math::NkMax(2.f, r.h * 0.03f);
					dl.AddRectFilled(NkRect{r.x + lift * 0.4f, r.y + lift, r.w, r.h}, NkWithAlpha(th.shadow, opacity),
									 radius);
					dl.AddRectFilledMultiColor(NkRect{r.x, r.y, r.w, r.h}, NkWithAlpha(th.panelTop, opacity),
											   NkWithAlpha(th.panelTop, opacity), NkWithAlpha(th.panel, opacity),
											   NkWithAlpha(th.panel, opacity));
					// Le dégradé ne peut pas être arrondi (AddRectFilledMultiColor
					// est droit) : on repose donc les coins par-dessus, dans la
					// teinte du haut, pour que l'arrondi reste net.
					dl.AddRect(r, NkWithAlpha(th.panelBorder, opacity), th.borderWidth, radius);
					dl.AddRectFilled(NkRect{r.x + radius * 0.5f, r.y + th.borderWidth, r.w - radius, 1.5f},
									 NkWithAlpha(th.panelSheen, opacity), 1.f);
				}

				/// Panneau à coins arrondis SANS dégradé — utilisé là où le
				/// dégradé droit trahirait l'arrondi (petites pastilles).
				void Chip(NkGuiDrawList &dl, const NkRect &r, float32 radius, const NkColor &fill,
						  const NkColor &border, float32 borderWidth) {
					dl.AddRectFilled(r, fill, radius);
					if (border.a > 0 && borderWidth > 0.f) {
						dl.AddRect(r, border, borderWidth, radius);
					}
				}

				/// Bouton : socle sombre + face colorée + libellé.
				/// `hot` = le pointeur est dessus (PC) ; sur mobile il ne l'est
				/// jamais, et c'est voulu — un doigt n'a pas de survol.
				void Button(NkGuiDrawList &dl, const NkRect &r, NkGuiFont *font, const char *label,
							const NkColor &faceColor, const NkColor &labelColor, const NkGemTheme &th, bool hot,
							bool enabled = true) {
					if (r.w <= 0.f || r.h <= 0.f) {
						return;
					}
					const float32 opacity = enabled ? 1.f : 0.45f;
					const float32 depth = math::NkMax(3.f, r.h * 0.11f);
					// Socle : c'est lui qui donne le relief « bouton de jeu ».
					//
					// ⚠️ Il DÉRIVE de la face, il n'est plus fixé à l'or. Mesuré à
					// l'écran : un socle doré sous une face blanche translucide
					// rendait le bouton SECONDAIRE (RECOMMENCER) aussi doré que le
					// bouton principal — les deux se disputaient le regard alors
					// que la hiérarchie était bien écrite dans l'appelant.
					const NkColor base = NkLerpColor(faceColor, NkColor(0, 0, 0, faceColor.a), 0.42f);
					dl.AddRectFilled(NkRect{r.x, r.y + depth, r.w, r.h - depth}, NkWithAlpha(base, opacity * 0.95f),
									 th.radiusChip);
					const NkColor top = hot && enabled ? NkLerpColor(faceColor, NkColor(255, 255, 255, faceColor.a), 0.18f)
													  : faceColor;
					dl.AddRectFilled(NkRect{r.x, r.y, r.w, r.h - depth}, NkWithAlpha(top, opacity), th.radiusChip);
					dl.AddRectFilled(NkRect{r.x + th.radiusChip * 0.5f, r.y + 2.f, r.w - th.radiusChip, 2.f},
									 NkColor(255, 255, 255, static_cast<uint8>(70.f * opacity)), 1.f);
					if (label != nullptr) {
						const float32 lh = LineH(font, r.h * 0.5f);
						TextCentered(dl, font, r, r.y + (r.h - depth - lh) * 0.5f, label,
									 NkWithAlpha(labelColor, opacity));
					}
				}

				/// Icône « pause » : deux barres. Dessinée, pas écrite — un
				/// glyphe ⏸ n'existe pas dans l'atlas des polices embarquées.
				void IconPause(NkGuiDrawList &dl, const NkRect &r, const NkColor &c) {
					const float32 barW = r.w * 0.16f;
					const float32 barH = r.h * 0.46f;
					const float32 cy = r.y + r.h * 0.5f - barH * 0.5f;
					dl.AddRectFilled(NkRect{r.x + r.w * 0.30f - barW * 0.5f, cy, barW, barH}, c, barW * 0.4f);
					dl.AddRectFilled(NkRect{r.x + r.w * 0.70f - barW * 0.5f, cy, barW, barH}, c, barW * 0.4f);
				}

				/// Étoile de progression, pleine ou vide.
				void ProgressStar(NkGuiDrawList &dl, const NkVec2f &center, float32 radius, bool filled,
								  const NkGemTheme &th) {
					const int32 spikes = 5;
					NkVec2f pts[10];
					for (int32 i = 0; i < spikes * 2; ++i) {
						const float32 rr = (i % 2 == 0) ? radius : radius * 0.45f;
						const float32 a = -math::NK_PI_F * 0.5f +
										  math::NK_PI_F * static_cast<float32>(i) / static_cast<float32>(spikes);
						pts[i] = NkVec2f(center.x + math::NkCos(a) * rr, center.y + math::NkSin(a) * rr);
					}
					// Étoile non convexe : éventail depuis le centre, comme les gemmes.
					const NkColor fill = filled ? th.accent : NkColor(255, 255, 255, 40);
					for (int32 i = 0; i < spikes * 2; ++i) {
						dl.AddTriangleFilled(center, pts[i], pts[(i + 1) % (spikes * 2)], fill);
					}
					dl.AddPolyline(pts, spikes * 2, filled ? th.accentDeep : NkColor(255, 255, 255, 60),
								   math::NkMax(1.f, radius * 0.12f), true);
				}

				/// Étoile à remplissage PARTIEL (0..1), de gauche à droite.
				///
				/// Le remplissage passe par un rectangle de découpe, pas par une
				/// étoile plus petite : une étoile qui rétrécit se lit comme « une
				/// petite étoile », pas comme « une étoile à moitié perdue ».
				void ProgressStarPartial(NkGuiDrawList &dl, const NkVec2f &center, float32 radius, float32 fill01,
										 const NkGemTheme &th) {
					ProgressStar(dl, center, radius, false, th);
					// Meme seuil que NkStarIsLit : entre les deux, le dessin
					// montrerait un filet d etoile que le verdict compte eteint.
					const float32 fill = math::NkClamp(fill01, 0.f, 1.f);
					if (fill <= 0.005f) {
						return;
					}
					if (fill < 0.999f) {
						dl.PushClipRect(NkRect{center.x - radius * 1.2f, center.y - radius * 1.2f,
											   radius * 2.4f * fill, radius * 2.4f},
										true);
					}
					ProgressStar(dl, center, radius, true, th);
					if (fill < 0.999f) {
						dl.PopClipRect();
					}
				}

			} // namespace

			// =========================================================
			// Part de ressource consommée — LA source, lue aussi par les
			// règles de partie (voir NkGemHud.h).
			// =========================================================
			float32 NkGemSpentFraction(const NkGemHudState &state) noexcept {
				if (state.mode == NkGemMode::NK_MODE_TIMED) {
					return (state.timeTotal > 0.f) ? math::NkClamp(1.f - state.timeLeft / state.timeTotal, 0.f, 1.f)
												   : 0.f;
				}
				return (state.movesTotal > 0)
						   ? math::NkClamp(
								 1.f - static_cast<float32>(state.movesLeft) / static_cast<float32>(state.movesTotal),
								 0.f, 1.f)
						   : 0.f;
			}

			bool NkGemObjectivesComplete(const NkGemHudState &state) noexcept {
				if (state.objectiveCount <= 0) {
					return false; // pas d'objectif = rien à atteindre, donc rien de gagné
				}
				for (int32 i = 0; i < state.objectiveCount; ++i) {
					if (state.objectives[i].collected < state.objectives[i].goal) {
						return false;
					}
				}
				return true;
			}

			bool NkGemResourceRuns(const NkGemHudState &state) noexcept {
				return !state.paused && !state.gameOver && !NkGemObjectivesComplete(state);
			}

			// =========================================================
			// NkGemLayout
			// =========================================================
			float32 NkGemLayout::SuggestedBodyFontPx(float32 width, float32 height, float32 density) {
				const float32 minSide = math::NkMin(width, height);
				// Basé sur le PLUS PETIT côté ET sur la densité : une rotation
				// d écran ne change ni l un ni l autre, donc les atlas ne sont
				// pas rechargés pour rien au passage portrait <-> paysage.
				const float32 d = math::NkClamp(density, 0.75f, 4.f);
				const float32 fit = math::NkClamp(minSide / (420.f * d), 0.85f, 1.25f);
				return math::NkClamp(17.f * d * fit, 13.f, 40.f);
			}

			NkGemLayout NkGemLayout::Compute(float32 width, float32 height, const NkSafeAreaInsets &insets,
											 int32 rowCount, int32 columnCount, float32 density) {
				NkGemLayout l;
				l.width = width;
				l.height = height;
				l.rowCount = rowCount;
				l.columnCount = columnCount;
				l.landscape = (width > height * 1.15f);

				// ÉCHELLE = DENSITÉ, ajustée à la marge par la place disponible.
				//
				// ⚠️ Mesuré à l écran le 2026-08-27 : l échelle valait
				// minSide/420, donc 1,81 dans une fenêtre PC de 760 px — texte et
				// panneaux gonflaient jusqu à ne plus laisser de place au
				// plateau. Le nombre de pixels ne dit RIEN de la taille physique :
				// un pouce fait 96 px sur un écran de bureau et 460 sur un
				// téléphone récent. Ce qui doit grandir avec les pixels, c est le
				// PLATEAU ; le texte et les boutons, eux, suivent la densité.
				const float32 minSide = math::NkMin(width, height);
				const float32 d = math::NkClamp(density, 0.75f, 4.f);
				const float32 fit = math::NkClamp(minSide / (420.f * d), 0.85f, 1.25f);
				l.scale = math::NkClamp(d * fit, 0.72f, 3.0f);

				// Zone sûre : demandée à la plateforme, JAMAIS codée en dur.
				// Sur PC les quatre marges valent 0 et tout se comporte comme
				// si cette étape n'existait pas.
				const float32 pad = 12.f * l.scale;
				l.safe = NkRect{insets.left + pad, insets.top + pad, math::NkMax(0.f, width - insets.left - insets.right - pad * 2.f),
								math::NkMax(0.f, height - insets.top - insets.bottom - pad * 2.f)};

				const float32 gap = 10.f * l.scale;
				const float32 buttonH = math::NkMax(NkTheme().minTouchTarget * l.scale * 0.9f, 52.f * l.scale);

				if (!l.landscape) {
					// -- PORTRAIT : HUD en haut, boosters en bas -------------
					// Hauteur DÉRIVÉE du contenu, pas un nombre choisi.
					//
					// ⚠️ Défaut mesuré à l'écran le 2026-08-27 : le plafond était
					// à 0,30 · hauteur sûre, EN DESSOUS de ce que la barre porte
					// (badge + score + jauge + pastilles). Les pastilles
					// d'objectif se dessinaient DEHORS, par-dessus le plateau.
					// Un panneau doit garantir la place de ce qu'il contient ;
					// s'il ne le peut pas, c'est le contenu qui doit céder, pas
					// la frontière.
					const float32 needed = (14.f + 30.f + 10.f + 14.f + 34.f + 8.f + 10.f + 12.f + 30.f + 14.f) * l.scale;
					const float32 topH = math::NkMin(needed, l.safe.h * 0.42f);
					l.topBar = NkRect{l.safe.x, l.safe.y, l.safe.w, topH};
					l.bottomBar = NkRect{l.safe.x, l.safe.y + l.safe.h - buttonH, l.safe.w, buttonH};
					const float32 midY = l.topBar.y + l.topBar.h + gap;
					const float32 midH = math::NkMax(0.f, l.bottomBar.y - gap - midY);
					l.boardPanel = NkRect{l.safe.x, midY, l.safe.w, midH};
				} else {
					// -- PAYSAGE : HUD en colonne à gauche, plateau à droite --
					const float32 colW = math::NkClamp(l.safe.w * 0.32f, 210.f * l.scale, 340.f * l.scale);
					// Le panneau EPOUSE son contenu au lieu de remplir la colonne :
					// mesure a l ecran le 2026-08-27, un panneau bien plus haut que
					// ce qu il porte se lit comme un defaut de mise en page, pas
					// comme de l espace.
					const float32 columnH =
						math::NkMin(math::NkMax(0.f, l.safe.h - buttonH - gap), 330.f * l.scale);
					l.topBar = NkRect{l.safe.x, l.safe.y, colW, columnH};
					l.bottomBar = NkRect{l.safe.x, l.safe.y + l.safe.h - buttonH, colW, buttonH};
					l.boardPanel = NkRect{l.safe.x + colW + gap, l.safe.y, math::NkMax(0.f, l.safe.w - colW - gap),
										  l.safe.h};
				}

				// -- Plateau : carré, centré dans son panneau ---------------
				const float32 inner = 14.f * l.scale;
				const float32 availW = math::NkMax(0.f, l.boardPanel.w - inner * 2.f);
				const float32 availH = math::NkMax(0.f, l.boardPanel.h - inner * 2.f);
				const float32 cell = math::NkMax(
					16.f, math::NkMin(availW / static_cast<float32>(columnCount), availH / static_cast<float32>(rowCount)));
				l.cellSize = cell;

				const float32 boardW = cell * static_cast<float32>(columnCount);
				const float32 boardH = cell * static_cast<float32>(rowCount);
				l.boardOrigin = NkVec2f(l.boardPanel.x + (l.boardPanel.w - boardW) * 0.5f,
										l.boardPanel.y + (l.boardPanel.h - boardH) * 0.5f);
				l.boardArea = NkRect{l.boardOrigin.x, l.boardOrigin.y, boardW, boardH};

				// Le cadre décoratif épouse le plateau au lieu de remplir toute
				// la zone : un panneau bien plus grand que son contenu se lit
				// comme un défaut de mise en page.
				l.boardPanel = NkRect{l.boardArea.x - inner, l.boardArea.y - inner, boardW + inner * 2.f,
									  boardH + inner * 2.f};
				return l;
			}

			// =========================================================
			// Fond de scène
			// =========================================================
			void NkGemHud::DrawBackground(NkGuiDrawList &dl, const NkGemLayout &layout, float32 time) {
				const NkGemTheme &th = NkTheme();
				const NkRect full{0.f, 0.f, layout.width, layout.height};

				// Le fond traverse l'écran ENTIER, encoche comprise : c'est la
				// règle mobile (le fond va au bord, le contenu reste dans la
				// zone sûre). Un fond arrêté à la zone sûre laisse des bandes.
				dl.AddRectFilledMultiColor(full, th.backdropTop, th.backdropTop, th.backdropBottom, th.backdropBottom);

				// Deux lueurs lentes : elles évitent l'aplat mort d'un dégradé
				// pur, sans coûter une texture ni un shader.
				const float32 r0 = layout.width * 0.55f;
				const NkVec2f g0(layout.width * (0.22f + 0.05f * math::NkSin(time * 0.23f)),
								 layout.height * (0.18f + 0.03f * math::NkCos(time * 0.19f)));
				const NkVec2f g1(layout.width * (0.80f + 0.05f * math::NkCos(time * 0.17f)),
								 layout.height * (0.78f + 0.03f * math::NkSin(time * 0.21f)));
				for (int32 i = 3; i >= 1; --i) {
					const float32 k = static_cast<float32>(i) / 3.f;
					dl.AddCircleFilled(g0, r0 * k, NkColor(120, 70, 220, 12));
					dl.AddCircleFilled(g1, r0 * 0.8f * k, NkColor(40, 140, 220, 10));
				}

				// Vignette : quatre bandes dégradées vers les bords. Elle
				// ramène le regard au centre, là où se joue la partie.
				const float32 vh = layout.height * 0.22f;
				const float32 vw = layout.width * 0.18f;
				const NkColor clear(th.vignette.r, th.vignette.g, th.vignette.b, 0);
				dl.AddRectFilledMultiColor(NkRect{0.f, 0.f, layout.width, vh}, th.vignette, th.vignette, clear, clear);
				dl.AddRectFilledMultiColor(NkRect{0.f, layout.height - vh, layout.width, vh}, clear, clear, th.vignette,
										   th.vignette);
				dl.AddRectFilledMultiColor(NkRect{0.f, 0.f, vw, layout.height}, th.vignette, clear, clear, th.vignette);
				dl.AddRectFilledMultiColor(NkRect{layout.width - vw, 0.f, vw, layout.height}, clear, th.vignette,
										   th.vignette, clear);
			}

			// =========================================================
			// Cadre du plateau + damier
			// =========================================================
			void NkGemHud::DrawBoardFrame(NkGuiDrawList &dl, const NkGemLayout &layout, float32 time,
										  const NkVec2i &hintCellA, const NkVec2i &hintCellB,
										  const NkVec2i &targetCell) {
				const NkGemTheme &th = NkTheme();
				Panel(dl, layout.boardPanel, th.radiusPanel, th);

				// Creux intérieur : le plateau doit sembler ENFONCÉ dans le
				// cadre, pas posé dessus. C'est le liseré sombre qui le dit.
				dl.AddRect(NkRect{layout.boardArea.x - 2.f, layout.boardArea.y - 2.f, layout.boardArea.w + 4.f,
								  layout.boardArea.h + 4.f},
						   NkColor(12, 10, 28, 160), 3.f, th.radiusCell + 2.f);

				const float32 cell = layout.cellSize;
				const float32 inset = cell * 0.045f;
				for (int32 row = 0; row < layout.rowCount; ++row) {
					for (int32 col = 0; col < layout.columnCount; ++col) {
						const NkRect r{layout.boardOrigin.x + static_cast<float32>(col) * cell + inset,
									   layout.boardOrigin.y + static_cast<float32>(row) * cell + inset,
									   cell - inset * 2.f, cell - inset * 2.f};
						dl.AddRectFilled(r, ((row + col) % 2 == 0) ? th.cellEven : th.cellOdd, th.radiusCell);

						const bool isHint = (hintCellA.x == row && hintCellA.y == col) ||
											(hintCellB.x == row && hintCellB.y == col);
						if (isHint) {
							// Pulsation : l'indice doit attirer l'œil sans
							// masquer la gemme qu'il désigne.
							const float32 pulse = 0.5f + 0.5f * math::NkSin(time * 4.f);
							dl.AddRectFilled(r, NkWithAlpha(th.cellHint, 0.4f + 0.6f * pulse), th.radiusCell);
							dl.AddRect(r, NkWithAlpha(th.accent, 0.5f + 0.5f * pulse), 2.f * layout.scale,
									   th.radiusCell);
						}
						if (targetCell.x == row && targetCell.y == col) {
							dl.AddRectFilled(r, th.cellTarget, th.radiusCell);
							dl.AddRect(r, NkColor(255, 255, 255, 120), 2.f * layout.scale, th.radiusCell);
						}
					}
				}
			}

			// =========================================================
			// HUD principal
			// =========================================================
			NkGemHudButtons NkGemHud::DrawHud(NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout,
											  const NkGemHudState &state, float32 time, const NkVec2f &pointer) {
				const NkGemTheme &th = NkTheme();
				NkGemHudButtons buttons;
				const float32 s = layout.scale;
				const NkRect bar = layout.topBar;
				if (bar.w <= 0.f || bar.h <= 0.f) {
					return buttons;
				}

				Panel(dl, bar, th.radiusPanel, th);

				const float32 pad = 14.f * s;
				const float32 innerX = bar.x + pad;
				const float32 innerW = bar.w - pad * 2.f;
				float32 y = bar.y + pad;

				// -- Ligne 1 : badge de niveau + bouton pause ---------------
				const float32 badgeH = math::NkMax(30.f * s, LineH(fonts.body, 20.f * s) + 8.f * s);
				const float32 pauseSize = math::NkMax(th.minTouchTarget * s * 0.86f, badgeH);
				buttons.pause = NkRect{bar.x + bar.w - pad - pauseSize, y, pauseSize, pauseSize};

				{
					const NkString levelText = NkString::Fmt("NIVEAU {0}", state.level);
					const float32 badgeW = MeasureW(fonts.body, levelText.CStr()) + 22.f * s;
					const NkRect badge{innerX, y, badgeW, badgeH};
					Chip(dl, badge, badgeH * 0.5f, NkColor(255, 255, 255, 22), NkWithAlpha(th.panelBorder, 0.8f), 1.5f);
					TextCentered(dl, fonts.body, badge, badge.y + (badge.h - LineH(fonts.body, 18.f * s)) * 0.5f,
								 levelText.CStr(), th.textSecondary);

					const bool hotPause = NkGemHudButtons::Contains(buttons.pause, pointer);
					Chip(dl, buttons.pause, th.radiusChip, NkColor(255, 255, 255, hotPause ? 46u : 26u),
						 NkWithAlpha(th.panelBorder, 0.8f), 1.5f);
					IconPause(dl, buttons.pause, th.textPrimary);
				}
				y += badgeH + 10.f * s;

				// -- Ligne 2 : score (gros) + coups restants ---------------
				{
					const float32 titleH = LineH(fonts.title, 30.f * s);
					Text(dl, fonts.small, innerX, y, "SCORE", th.textSecondary);
					const float32 labelH = LineH(fonts.small, 14.f * s);
					const NkString scoreText = NkString::Fmt("{0}", state.displayedScore);
					Text(dl, fonts.title, innerX, y + labelH * 0.9f, scoreText.CStr(), th.accent);

					// La RESSOURCE de la partie : des coups, ou du temps. Le
					// mode change ce qu'on affiche, jamais où on l'affiche — un
					// joueur cherche le compteur toujours au même endroit.
					const bool timed = (state.mode == NkGemMode::NK_MODE_TIMED);
					const bool low = timed ? (state.timeLeft <= 10.f) : (state.movesLeft <= 5);
					NkString movesText;
					if (timed) {
						const int32 total = math::NkMax(0, static_cast<int32>(state.timeLeft + 0.5f));
						// Sous une minute on n'écrit plus « 0:07 » mais « 7 » :
						// le zéro de gauche vole de la place au moment où le
						// chiffre compte le plus.
						movesText = (total >= 60) ? NkString::Fmt("{0}:{1:02}", total / 60, total % 60)
												  : NkString::Fmt("{0}", total);
					} else {
						movesText = NkString::Fmt("{0}", state.movesLeft);
					}
					const float32 right = bar.x + bar.w - pad;
					TextRight(dl, fonts.small, right, y, timed ? "TEMPS" : "COUPS", th.textSecondary);
					TextRight(dl, fonts.title, right, y + labelH * 0.9f, movesText.CStr(),
							  low ? th.danger : th.textPrimary);
					if (low) {
						const float32 pulse = 0.5f + 0.5f * math::NkSin(time * 6.f);
						const float32 w = MeasureW(fonts.title, movesText.CStr());
						dl.AddRectFilled(NkRect{right - w - 8.f * s, y + labelH * 0.9f - 4.f * s, w + 16.f * s,
												titleH + 8.f * s},
										 NkWithAlpha(th.danger, 0.10f + 0.10f * pulse), 10.f * s);
					}
					y += labelH * 0.9f + titleH + 8.f * s;
				}

				// -- Ligne 3 : barre de progression + 3 étoiles -------------
				{
					const float32 starR = 9.f * s;
					const float32 starsW = starR * 7.2f;
					const float32 barH = 10.f * s;
					const float32 trackW = math::NkMax(20.f, innerW - starsW - 10.f * s);
					const NkRect track{innerX, y, trackW, barH};
					dl.AddRectFilled(track, NkColor(10, 8, 24, 170), barH * 0.5f);

					// LA JAUGE MONTRE L'OBJECTIF, pas le score : c'est l'objectif
					// qui décide de la victoire, donc c'est lui que le joueur doit
					// pouvoir suivre d'un coup d'œil.
					int32 collected = 0, goal = 0;
					for (int32 i = 0; i < state.objectiveCount; ++i) {
						collected += math::NkMin(state.objectives[i].collected, state.objectives[i].goal);
						goal += state.objectives[i].goal;
					}
					const float32 ratio =
						(goal > 0) ? math::NkClamp(static_cast<float32>(collected) / static_cast<float32>(goal), 0.f, 1.f)
								   : 0.f;
					if (ratio > 0.f) {
						const NkRect fill{track.x, track.y, math::NkMax(barH, track.w * ratio), barH};
						dl.AddRectFilled(fill, th.accentDeep, barH * 0.5f);
						dl.AddRectFilled(NkRect{fill.x, fill.y, fill.w, barH * 0.55f}, th.accent, barH * 0.5f);
					}

					// LES ÉTOILES FONDENT AVEC LA RESSOURCE. Elles se vident
					// continûment : une étoile à moitié pleine pousse à finir
					// vite, un compteur qui saute de 3 à 2 ne le fait pas.
					const float32 spent = NkGemSpentFraction(state);
					for (int32 i = 0; i < 3; ++i) {
						const NkVec2f c(innerX + trackW + 10.f * s + starR + static_cast<float32>(i) * starR * 2.4f,
										y + barH * 0.5f);
						ProgressStarPartial(dl, c, starR, NkStarFill(2 - i, spent), th);
					}
					// Mode CHRONO : une seconde barre, dédiée au temps. Deux
					// jauges empilées se lisent d'un coup d'œil ; un chiffre seul
					// oblige à le lire ET à le comparer de tête.
					if (state.mode == NkGemMode::NK_MODE_TIMED && state.timeTotal > 0.f) {
						const NkRect timeTrack{innerX, y + barH + 5.f * s, trackW, barH * 0.7f};
						dl.AddRectFilled(timeTrack, NkColor(10, 8, 24, 170), timeTrack.h * 0.5f);
						const float32 ratioTime = math::NkClamp(state.timeLeft / state.timeTotal, 0.f, 1.f);
						if (ratioTime > 0.f) {
							const NkColor colour = (state.timeLeft <= 10.f) ? th.danger : th.accentCool;
							dl.AddRectFilled(
								NkRect{timeTrack.x, timeTrack.y, math::NkMax(timeTrack.h, timeTrack.w * ratioTime),
									   timeTrack.h},
								colour, timeTrack.h * 0.5f);
						}
						y += timeTrack.h + 5.f * s;
					}
					y += barH + 12.f * s;
				}

				// -- Ligne 4 : objectifs ------------------------------------
				//
				// ⚠️ LA LARGEUR VIENT DU TEXTE, PAS DE LA DISPOSITION.
				// Défaut mesuré à l'écran le 2026-08-27 : en paysage, la colonne
				// de HUD est étroite, trois pastilles à largeur imposée y
				// entraient de force et « 0/12 » s'affichait « 0/1 ». Le libellé
				// n'était pas fautif — le conteneur l'était. Un compteur tronqué
				// ne se voit pas comme un bogue : il se lit comme un autre chiffre.
				const float32 objectiveRoom = (bar.y + bar.h - pad * 0.5f) - y;
				if (state.objectiveCount > 0 && objectiveRoom > 16.f * s) {
					// La pastille ne dépasse JAMAIS la place restante : quand la
					// barre est courte, c'est la pastille qui rétrécit.
					const float32 chipH =
						math::NkMin(objectiveRoom, math::NkMax(26.f * s, LineH(fonts.body, 20.f * s) + 8.f * s));
					const float32 gapX = 8.f * s;

					// 1. Mesurer ce que chaque pastille EXIGE.
					NkString labels[3];
					float32 needed[3] = {0.f, 0.f, 0.f};
					float32 totalNeeded = 0.f;
					float32 widest = 0.f;
					for (int32 i = 0; i < state.objectiveCount; ++i) {
						const NkGemObjective &obj = state.objectives[i];
						const int32 shown = math::NkMin(obj.collected, obj.goal);
						labels[i] = NkString::Fmt("{0}/{1}", shown, obj.goal);
						needed[i] = chipH * 1.05f + MeasureW(fonts.body, labels[i].CStr()) + 12.f * s;
						totalNeeded += needed[i];
						widest = math::NkMax(widest, needed[i]);
					}
					totalNeeded += gapX * static_cast<float32>(state.objectiveCount - 1);

					// 2. Choisir la disposition : en ligne si ça tient, sinon
					//    EMPILÉ. Le flot est la réponse, pas l'ellipse.
					const bool inRow = (totalNeeded <= innerW);
					const float32 stackHeight =
						(chipH + 4.f * s) * static_cast<float32>(state.objectiveCount) - 4.f * s;
					const bool canStack = (y + stackHeight) <= (bar.y + bar.h - pad * 0.5f);

					for (int32 i = 0; i < state.objectiveCount; ++i) {
						const NkGemObjective &obj = state.objectives[i];
						const bool done = obj.collected >= obj.goal;
						NkRect chip;
						if (inRow) {
							float32 x = innerX;
							for (int32 k = 0; k < i; ++k) {
								x += needed[k] + gapX;
							}
							chip = NkRect{x, y, needed[i], chipH};
						} else if (canStack) {
							chip = NkRect{innerX, y + static_cast<float32>(i) * (chipH + 4.f * s),
										  math::NkMin(widest, innerW), chipH};
						} else {
							// Dernier recours : ni en ligne, ni empilé. On garde
							// une répartition égale — et là, seulement là, le
							// texte peut être borné.
							const float32 w =
								(innerW - gapX * static_cast<float32>(state.objectiveCount - 1)) /
								static_cast<float32>(state.objectiveCount);
							chip = NkRect{innerX + static_cast<float32>(i) * (w + gapX), y, w, chipH};
						}

						Chip(dl, chip, chipH * 0.5f, NkColor(255, 255, 255, done ? 34u : 20u),
							 done ? NkWithAlpha(th.accent, 0.9f) : NkWithAlpha(th.panelBorder, 0.7f), 1.5f);
						NkGemArt::DrawIcon(dl, obj.color, NkVec2f(chip.x + chipH * 0.55f, chip.y + chipH * 0.5f),
										   chipH * 0.66f);
						Text(dl, fonts.body, chip.x + chipH * 1.02f,
							 chip.y + (chip.h - LineH(fonts.body, 18.f * s)) * 0.5f, labels[i].CStr(),
							 done ? th.accent : th.textPrimary, chip.w - chipH * 1.02f - 6.f * s);
						if (done) {
							// Objectif atteint : un éclat, pas seulement une
							// couleur — le daltonien doit le voir aussi.
							NkGemArt::DrawSparkle(dl, NkVec2f(chip.x + chip.w - chipH * 0.42f, chip.y + chipH * 0.5f),
												  chipH * 0.22f, th.accent);
						}
					}
				}

				// -- Boosters : deux actions RÉELLES ------------------------
				// Indice et Mélange sont branchés sur le plateau. Aucun bouton
				// inerte n'est dessiné : un réglage visible qui n'agit pas fait
				// chercher la panne ailleurs.
				{
					const NkRect bb = layout.bottomBar;
					if (bb.w > 0.f && bb.h > 0.f) {
						const float32 gap = 10.f * s;
						const float32 w = (bb.w - gap) * 0.5f;
						buttons.hint = NkRect{bb.x, bb.y, w, bb.h};
						buttons.shuffle = NkRect{bb.x + w + gap, bb.y, w, bb.h};
						Button(dl, buttons.hint, fonts.body, "INDICE", th.accentCool, th.textOnAccent, th,
							   NkGemHudButtons::Contains(buttons.hint, pointer));
						Button(dl, buttons.shuffle, fonts.body, "MELANGER", th.accent, th.textOnAccent, th,
							   NkGemHudButtons::Contains(buttons.shuffle, pointer));
					}
				}

				return buttons;
			}

			// =========================================================
			// Bannière de combo
			// =========================================================
			void NkGemHud::DrawComboBanner(NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout,
										   const NkGemHudState &state) {
				if (state.comboTimer <= 0.f || state.comboCount < 2) {
					return;
				}
				const NkGemTheme &th = NkTheme();
				const float32 s = layout.scale;
				// Fondu sortant sur la dernière demi-seconde.
				const float32 alpha = math::NkClamp(state.comboTimer / 0.5f, 0.f, 1.f);
				const NkString txt = NkString::Fmt("COMBO x{0} !", state.comboCount);
				const float32 w = MeasureW(fonts.title, txt.CStr()) + 34.f * s;
				const float32 h = LineH(fonts.title, 30.f * s) + 18.f * s;
				const NkRect box{layout.boardArea.x + (layout.boardArea.w - w) * 0.5f,
								 layout.boardArea.y + layout.boardArea.h * 0.42f, w, h};
				dl.AddRectFilled(NkRect{box.x, box.y + 4.f, box.w, box.h}, NkWithAlpha(th.shadow, alpha), h * 0.5f);
				dl.AddRectFilled(box, NkWithAlpha(th.accent, alpha), h * 0.5f);
				TextCentered(dl, fonts.title, box, box.y + (box.h - LineH(fonts.title, 30.f * s)) * 0.5f, txt.CStr(),
							 NkWithAlpha(th.textOnAccent, alpha));
			}

			// =========================================================
			// Écrans de pause et de fin de partie
			// =========================================================
			NkGemHudButtons NkGemHud::DrawOverlay(NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout,
												  const NkGemHudState &state, float32 time, const NkVec2f &pointer) {
				NkGemHudButtons buttons;
				if (!state.paused && !state.gameOver) {
					return buttons;
				}
				const NkGemTheme &th = NkTheme();
				const float32 s = layout.scale;

				// Voile plein écran : il coupe l'interaction avec le plateau
				// AUTANT qu'il le montre en retrait.
				dl.AddRectFilled(NkRect{0.f, 0.f, layout.width, layout.height}, NkColor(8, 6, 20, 195));

				const float32 w = math::NkMin(layout.safe.w, 340.f * s);
				const float32 h = math::NkMin(layout.safe.h, (state.paused ? 420.f : 460.f) * s);
				const NkRect box{layout.safe.x + (layout.safe.w - w) * 0.5f, layout.safe.y + (layout.safe.h - h) * 0.5f,
								 w, h};
				Panel(dl, box, th.radiusPanel, th);

				const char *title = state.paused
										? "PAUSE"
										: (state.victory ? "NIVEAU REUSSI"
														 : (state.mode == NkGemMode::NK_MODE_TIMED ? "TEMPS ECOULE"
																								   : "PLUS DE COUPS"));
				float32 y = box.y + 26.f * s;
				TextCentered(dl, fonts.title, box, y, title, state.victory ? th.accent : th.textPrimary);
				y += LineH(fonts.title, 30.f * s) + 14.f * s;

				// Score final + étoiles obtenues : la récompense se montre.
				if (!state.paused) {
					const int32 earned = NkStarsFromSpend(state.victory, NkGemSpentFraction(state));
					const float32 starR = 20.f * s;
					for (int32 i = 0; i < 3; ++i) {
						const NkVec2f c(box.x + box.w * 0.5f + (static_cast<float32>(i) - 1.f) * starR * 2.6f,
										y + starR);
						// La 3e étoile est un peu plus haute : composition
						// classique de fin de niveau, elle hiérarchise le gain.
						const float32 lift = (i == 1) ? -starR * 0.35f : 0.f;
						ProgressStar(dl, NkVec2f(c.x, c.y + lift), starR, i < earned, th);
					}
					y += starR * 2.4f;
					TextCentered(dl, fonts.small, box, y, "SCORE", th.textSecondary);
					y += LineH(fonts.small, 14.f * s);
					const NkString sc = NkString::Fmt("{0}", state.score);
					TextCentered(dl, fonts.title, box, y, sc.CStr(), th.accent);
					y += LineH(fonts.title, 30.f * s) + 12.f * s;
				} else {
					// Un panneau de pause vide n'aide personne : on y remet ce que
					// le joueur vient de quitter, puisque le HUD est masqué derrière.
					y += 10.f * s;
					const NkString resume = NkString::Fmt("SCORE {0}   —   {1} COUPS RESTANTS", state.score,
														 state.movesLeft);
					TextCentered(dl, fonts.body, box, y, resume.CStr(), th.textSecondary);
					y += LineH(fonts.body, 18.f * s) + 10.f * s;
					for (int32 i = 0; i < state.objectiveCount; ++i) {
						const NkGemObjective &obj = state.objectives[i];
						const float32 iconSize = 26.f * s;
						const float32 slot = box.w / static_cast<float32>(state.objectiveCount + 1);
						const NkVec2f centre(box.x + slot * static_cast<float32>(i + 1), y + iconSize * 0.5f);
						NkGemArt::DrawIcon(dl, obj.color, centre, iconSize);
						const NkString txt = NkString::Fmt("{0}/{1}", math::NkMin(obj.collected, obj.goal), obj.goal);
						Text(dl, fonts.small, centre.x - MeasureW(fonts.small, txt.CStr()) * 0.5f,
							 centre.y + iconSize * 0.55f, txt.CStr(), th.textSecondary);
					}
					// Hauteur REELLE de la rangee (icone + son compteur), pas une
					// constante : mesuree, la ligne suivante ne chevauche plus le
					// premier bouton.
					y += 26.f * s * 0.55f + 26.f * s * 0.5f + LineH(fonts.small, 14.f * s) + 12.f * s;
				}

				const float32 btnH = math::NkMax(th.minTouchTarget * s * 0.95f, 54.f * s);
				const float32 btnW = box.w - 44.f * s;
				const float32 btnX = box.x + 22.f * s;
				float32 btnY = box.y + box.h - 22.f * s - btnH;

				// De bas en haut : l'action la plus DOUCE en bas (quitter), la
				// plus engageante en haut. Le pouce tombe naturellement en bas
				// d'un téléphone — on n'y met donc pas « recommencer ».
				// Deux sorties, cote a cote : la CARTE (le niveau suivant) et
				// l'ACCUEIL (tout quitter). Les separer evite le retour en
				// arriere repete que demande un seul bouton « retour ».
				const float32 halfW = (btnW - 10.f * s) * 0.5f;
				buttons.quit = NkRect{btnX, btnY, halfW, btnH};
				buttons.home = NkRect{btnX + halfW + 10.f * s, btnY, halfW, btnH};
				Button(dl, buttons.quit, fonts.small, "CARTE", NkColor(255, 255, 255, 34), th.textSecondary, th,
					   NkGemHudButtons::Contains(buttons.quit, pointer));
				Button(dl, buttons.home, fonts.small, "ACCUEIL", NkColor(255, 255, 255, 34), th.textSecondary, th,
					   NkGemHudButtons::Contains(buttons.home, pointer));
				btnY -= btnH + 12.f * s;

				buttons.restart = NkRect{btnX, btnY, btnW, btnH};
				Button(dl, buttons.restart, fonts.body, state.paused ? "RECOMMENCER" : "REJOUER",
					   NkColor(255, 255, 255, 40), th.textPrimary, th,
					   NkGemHudButtons::Contains(buttons.restart, pointer));
				btnY -= btnH + 12.f * s;

				if (state.paused) {
					buttons.resume = NkRect{btnX, btnY, btnW, btnH};
					Button(dl, buttons.resume, fonts.body, "REPRENDRE", th.accent, th.textOnAccent, th,
						   NkGemHudButtons::Contains(buttons.resume, pointer));
				} else if (state.victory) {
					buttons.next = NkRect{btnX, btnY, btnW, btnH};
					Button(dl, buttons.next, fonts.body, "NIVEAU SUIVANT", th.accent, th.textOnAccent, th,
						   NkGemHudButtons::Contains(buttons.next, pointer));
				}

				(void)time;
				return buttons;
			}

		} // namespace ui
	} // namespace game
} // namespace nkentseu
