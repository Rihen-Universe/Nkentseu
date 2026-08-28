// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemScreens.cpp
// DESCRIPTION: Barre de titre, menu principal et carte d'aventure.
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Gemcrush/Ui/NkGemScreens.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			using nkgui::NkGuiDrawList;
			using nkgui::NkGuiFont;
			using math::NkVec2f;

			namespace {

				constexpr float32 kTau = 6.28318530718f;

				// Mêmes aides de texte que NkGemHud.cpp — voir la dette signalée
				// en tête de ce fichier-là : leur place est NKGui.
				void Text(NkGuiDrawList &dl, NkGuiFont *f, float32 x, float32 topY, const char *s, const NkColor &c) {
					if (f != nullptr && f->Face() != nullptr && s != nullptr) {
						dl.AddText(f->Face(), f->TexId(), NkVec2f(x, topY + f->Ascent()), s, c);
					}
				}

				float32 MeasureW(NkGuiFont *f, const char *s) {
					return (f != nullptr && s != nullptr) ? f->MeasureWidth(s) : 0.f;
				}

				void TextCentered(NkGuiDrawList &dl, NkGuiFont *f, const NkRect &box, float32 topY, const char *s,
								  const NkColor &c) {
					Text(dl, f, box.x + (box.w - MeasureW(f, s)) * 0.5f, topY, s, c);
				}

				float32 LineH(NkGuiFont *f, float32 fallback) {
					return (f != nullptr && f->Face() != nullptr) ? f->LineHeight() : fallback;
				}

				void Button(NkGuiDrawList &dl, const NkRect &r, NkGuiFont *font, const char *label,
							const NkColor &faceColor, const NkColor &labelColor, bool hot) {
					if (r.w <= 0.f || r.h <= 0.f) {
						return;
					}
					const NkGemTheme &th = NkTheme();
					const float32 depth = math::NkMax(3.f, r.h * 0.12f);
					const NkColor base = NkLerpColor(faceColor, NkColor(0, 0, 0, faceColor.a), 0.42f);
					dl.AddRectFilled(NkRect{r.x, r.y + depth, r.w, r.h - depth}, base, th.radiusChip);
					const NkColor top =
						hot ? NkLerpColor(faceColor, NkColor(255, 255, 255, faceColor.a), 0.18f) : faceColor;
					dl.AddRectFilled(NkRect{r.x, r.y, r.w, r.h - depth}, top, th.radiusChip);
					dl.AddRectFilled(NkRect{r.x + th.radiusChip * 0.5f, r.y + 2.f, r.w - th.radiusChip, 2.f},
									 NkColor(255, 255, 255, 70), 1.f);
					const float32 lh = LineH(font, r.h * 0.45f);
					TextCentered(dl, font, r, r.y + (r.h - depth - lh) * 0.5f, label, labelColor);
				}

				/// Étoile pleine ou vide — même dessin que le HUD (une seule
				/// forme d'étoile dans tout le jeu).
				void Star(NkGuiDrawList &dl, const NkVec2f &center, float32 radius, bool filled) {
					const NkGemTheme &th = NkTheme();
					NkVec2f pts[10];
					for (int32 i = 0; i < 10; ++i) {
						const float32 rr = (i % 2 == 0) ? radius : radius * 0.45f;
						const float32 a = -math::NK_PI_F * 0.5f + math::NK_PI_F * static_cast<float32>(i) / 5.f;
						pts[i] = NkVec2f(center.x + math::NkCos(a) * rr, center.y + math::NkSin(a) * rr);
					}
					const NkColor fill = filled ? th.accent : NkColor(255, 255, 255, 38);
					for (int32 i = 0; i < 10; ++i) {
						dl.AddTriangleFilled(center, pts[i], pts[(i + 1) % 10], fill);
					}
					dl.AddPolyline(pts, 10, filled ? th.accentDeep : NkColor(255, 255, 255, 55),
								   math::NkMax(1.f, radius * 0.13f), true);
				}

				/// Cadenas d'un niveau verrouillé.
				void Padlock(NkGuiDrawList &dl, const NkVec2f &center, float32 size, const NkColor &color) {
					const float32 w = size * 0.62f;
					const float32 h = size * 0.50f;
					// Anse : un demi-anneau, pas un cercle complet.
					dl.AddCircle(NkVec2f(center.x, center.y - h * 0.45f), w * 0.36f, color,
								 math::NkMax(1.5f, size * 0.10f));
					dl.AddRectFilled(NkRect{center.x - w * 0.5f, center.y - h * 0.20f, w, h}, color, size * 0.10f);
				}

			} // namespace

			// =========================================================
			// Barre de titre
			// =========================================================
			float32 NkGemTitleBarHeight(float32 uiScale) {
				return math::NkClamp(34.f * uiScale, 28.f, 56.f);
			}

			NkGemTitleBar NkGemDrawTitleBar(NkGuiDrawList &dl, const NkGemFonts &fonts, float32 width, float32 uiScale,
											const char *title, bool maximized, const NkVec2f &pointer) {
				const NkGemTheme &th = NkTheme();
				NkGemTitleBar bar;
				const float32 h = NkGemTitleBarHeight(uiScale);
				bar.bar = NkRect{0.f, 0.f, width, h};

				// Fond : dégradé qui se fond dans le fond de scène. Une barre
				// opaque « collée » se verrait comme une pièce rapportée.
				dl.AddRectFilledMultiColor(bar.bar, NkColor(38, 32, 76, 255), NkColor(38, 32, 76, 255),
										   NkColor(26, 22, 56, 235), NkColor(26, 22, 56, 235));
				dl.AddRectFilled(NkRect{0.f, h - 1.f, width, 1.f}, NkWithAlpha(th.panelBorder, 0.7f), 0.f);

				// Marque : une gemme dessinée + le titre. La gemme sert d'icône
				// d'application — elle vient du MÊME code que celles du plateau.
				const float32 pad = h * 0.28f;
				NkGemArt::DrawIcon(dl, NkGemColor::NK_GEM_COLOR_PURPLE, NkVec2f(pad + h * 0.28f, h * 0.5f), h * 0.56f);
				Text(dl, fonts.body, pad + h * 0.72f, (h - LineH(fonts.body, 16.f * uiScale)) * 0.5f, title,
					 th.textPrimary);

				// Boutons système, à droite, dans l'ordre attendu.
				const float32 bw = math::NkMax(34.f * uiScale, h * 1.05f);
				bar.close = NkRect{width - bw, 0.f, bw, h};
				bar.maximize = NkRect{width - bw * 2.f, 0.f, bw, h};
				bar.minimize = NkRect{width - bw * 3.f, 0.f, bw, h};

				const NkColor icon = th.textSecondary;
				const float32 g = h * 0.24f; // demi-taille du glyphe

				// Réduire : un trait bas.
				if (NkGemHudButtons::Contains(bar.minimize, pointer)) {
					dl.AddRectFilled(bar.minimize, NkColor(255, 255, 255, 26), 0.f);
				}
				dl.AddRectFilled(NkRect{bar.minimize.x + bw * 0.5f - g, h * 0.60f, g * 2.f, math::NkMax(1.5f, uiScale)},
								 icon, 0.f);

				// Agrandir / restaurer : un carré, ou deux décalés.
				if (NkGemHudButtons::Contains(bar.maximize, pointer)) {
					dl.AddRectFilled(bar.maximize, NkColor(255, 255, 255, 26), 0.f);
				}
				const float32 mx = bar.maximize.x + bw * 0.5f;
				const float32 th2 = math::NkMax(1.5f, uiScale);
				if (maximized) {
					dl.AddRect(NkRect{mx - g * 0.85f, h * 0.5f - g * 0.55f, g * 1.6f, g * 1.6f}, icon, th2, 1.f);
					dl.AddRect(NkRect{mx - g * 0.45f, h * 0.5f - g * 0.95f, g * 1.6f, g * 1.6f}, icon, th2, 1.f);
				} else {
					dl.AddRect(NkRect{mx - g, h * 0.5f - g, g * 2.f, g * 2.f}, icon, th2, 1.f);
				}

				// Fermer : croix. Survol ROUGE — c'est la convention de toutes
				// les plateformes de bureau, et un utilisateur la cherche.
				const bool hotClose = NkGemHudButtons::Contains(bar.close, pointer);
				if (hotClose) {
					dl.AddRectFilled(bar.close, NkColor(232, 60, 72, 235), 0.f);
				}
				const NkColor closeIcon = hotClose ? NkColor(255, 255, 255, 255) : icon;
				const float32 cx = bar.close.x + bw * 0.5f;
				dl.AddLine(NkVec2f(cx - g, h * 0.5f - g), NkVec2f(cx + g, h * 0.5f + g), closeIcon, th2 * 1.4f);
				dl.AddLine(NkVec2f(cx - g, h * 0.5f + g), NkVec2f(cx + g, h * 0.5f - g), closeIcon, th2 * 1.4f);

				return bar;
			}

			// =========================================================
			// Menu principal
			// =========================================================
			NkGemMenuButtons NkGemDrawMainMenu(NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout,
											   const NkGemProgress &progress, float32 time, const NkVec2f &pointer,
											   bool muted) {
				const NkGemTheme &th = NkTheme();
				NkGemMenuButtons buttons;
				const float32 s = layout.scale;
				const NkRect safe = layout.safe;

				// Gemmes qui flottent en fond : elles disent de quel jeu il
				// s'agit avant qu'on ait lu une seule ligne.
				for (int32 i = 0; i < 7; ++i) {
					const float32 fi = static_cast<float32>(i);
					const float32 driftX = math::NkSin(time * 0.32f + fi * 1.7f) * 0.5f + 0.5f;
					const float32 driftY = math::NkCos(time * 0.24f + fi * 2.3f) * 0.5f + 0.5f;
					const NkVec2f p(safe.x + safe.w * (0.08f + 0.84f * driftX),
									safe.y + safe.h * (0.10f + 0.80f * driftY));
					NkGemVisual gem;
					gem.center = p;
					gem.size = (34.f + fi * 5.f) * s;
					gem.color = static_cast<NkGemColor>(1 + (i % 6));
					gem.shape = NkGemShapeForColor(gem.color);
					gem.alpha = 0.18f;
					gem.shadow = false;
					gem.time = time;
					NkGemArt::Draw(dl, gem);
				}

				// Titre.
				float32 y = safe.y + safe.h * 0.14f;
				const float32 titleH = LineH(fonts.title, 34.f * s);
				TextCentered(dl, fonts.title, safe, y, "GEMCRUSH", th.accent);
				y += titleH * 0.92f;
				TextCentered(dl, fonts.small, safe, y, "UN JEU RIHEN", th.textSecondary);
				y += LineH(fonts.small, 14.f * s) + 18.f * s;

				// Trois mesures de progression : ce qu on a gagne (etoiles), le
				// meilleur score, et jusqu ou on est monte. Un accueil qui ne
				// montre rien de la partie precedente ne donne aucune raison d y
				// revenir.
				{
					const float32 chipH = 34.f * s;
					const NkString stars =
						NkString::Fmt("{0} / {1}", progress.GetTotalStars(), NK_GEM_LEVEL_COUNT * 3);
					const float32 w = MeasureW(fonts.body, stars.CStr()) + 54.f * s;
					const NkRect chip{safe.x + (safe.w - w) * 0.5f, y, w, chipH};
					dl.AddRectFilled(chip, NkColor(255, 255, 255, 22), chip.h * 0.5f);
					dl.AddRect(chip, NkWithAlpha(th.panelBorder, 0.8f), 1.5f, chip.h * 0.5f);
					Star(dl, NkVec2f(chip.x + chip.h * 0.62f, chip.y + chip.h * 0.5f), chip.h * 0.30f, true);
					Text(dl, fonts.body, chip.x + chip.h * 1.05f,
						 chip.y + (chip.h - LineH(fonts.body, 18.f * s)) * 0.5f, stars.CStr(), th.textPrimary);
					y += chip.h + 12.f * s;

					// Deux cartouches cote a cote : meilleur score et niveau max.
					const float32 cardW = math::NkMin(safe.w * 0.78f, 320.f * s);
					const float32 cardH = 52.f * s;
					const float32 half = (cardW - 10.f * s) * 0.5f;
					const float32 cardX = safe.x + (safe.w - cardW) * 0.5f;
					const NkRect cards[2] = {NkRect{cardX, y, half, cardH},
											 NkRect{cardX + half + 10.f * s, y, half, cardH}};
					const NkString values[2] = {NkString::Fmt("{0}", progress.GetBestScore()),
												NkString::Fmt("{0}", progress.GetHighestUnlocked())};
					const char *labels[2] = {"MEILLEUR SCORE", "NIVEAU MAX"};
					const NkColor tints[2] = {th.accent, th.accentCool};
					for (int32 i = 0; i < 2; ++i) {
						dl.AddRectFilled(cards[i], NkColor(255, 255, 255, 20), th.radiusChip);
						dl.AddRect(cards[i], NkWithAlpha(th.panelBorder, 0.7f), 1.5f, th.radiusChip);
						TextCentered(dl, fonts.small, cards[i], cards[i].y + 6.f * s, labels[i], th.textSecondary);
						TextCentered(dl, fonts.body, cards[i], cards[i].y + 6.f * s + LineH(fonts.small, 13.f * s),
									 values[i].CStr(), tints[i]);
					}
					y += cardH + 22.f * s;
				}

				// Boutons principaux.
				const float32 bw = math::NkMin(safe.w * 0.78f, 320.f * s);
				const float32 bh = math::NkMax(NkTheme().minTouchTarget * s, 58.f * s);
				const float32 bx = safe.x + (safe.w - bw) * 0.5f;

				buttons.play = NkRect{bx, y, bw, bh};
				const NkString playLabel = (progress.GetHighestUnlocked() > 1)
											   ? NkString::Fmt("CONTINUER - NIVEAU {0}", progress.GetHighestUnlocked())
											   : NkString("AVENTURE");
				Button(dl, buttons.play, fonts.body, playLabel.CStr(), th.accent, th.textOnAccent,
					   NkGemHudButtons::Contains(buttons.play, pointer));
				y += bh + 14.f * s;

				buttons.quickPlay = NkRect{bx, y, bw, bh};
				Button(dl, buttons.quickPlay, fonts.body, "DEFI CHRONO", th.accentCool, th.textOnAccent,
					   NkGemHudButtons::Contains(buttons.quickPlay, pointer));
				y += bh + 18.f * s;

				// Réglages : deux petits boutons côte à côte.
				const float32 sw = (bw - 12.f * s) * 0.5f;
				const float32 sh = math::NkMax(NkTheme().minTouchTarget * s * 0.85f, 46.f * s);
				buttons.sound = NkRect{bx, y, sw, sh};
				buttons.reset = NkRect{bx + sw + 12.f * s, y, sw, sh};
				Button(dl, buttons.sound, fonts.small, muted ? "SON : COUPE" : "SON : ACTIF",
					   NkColor(255, 255, 255, 34), th.textPrimary, NkGemHudButtons::Contains(buttons.sound, pointer));
				Button(dl, buttons.reset, fonts.small, "REINITIALISER", NkColor(255, 255, 255, 34), th.textSecondary,
					   NkGemHudButtons::Contains(buttons.reset, pointer));

				return buttons;
			}

			// =========================================================
			// Carte d'aventure — géométrie
			// =========================================================
			NkGemMapGeometry NkGemMapGeometry::Compute(const NkGemLayout &layout, float32 scroll) {
				NkGemMapGeometry g;
				g.viewport = layout.safe;
				g.spacing = 104.f * layout.scale;
				g.nodeRadius = 27.f * layout.scale;
				g.amplitude = math::NkMin(layout.safe.w * 0.28f, 86.f * layout.scale);
				g.levelCount = NK_GEM_LEVEL_COUNT;
				g.scroll = math::NkClamp(scroll, 0.f, g.MaxScroll());
				return g;
			}

			float32 NkGemMapGeometry::MaxScroll() const noexcept {
				// Le niveau 1 est EN BAS (on monte en progressant, comme une
				// carte de randonnée). La hauteur totale inclut une marge en
				// haut et en bas pour que le premier et le dernier nœud ne
				// soient jamais collés au bord.
				const float32 total = spacing * static_cast<float32>(levelCount - 1) + nodeRadius * 6.f;
				return math::NkMax(0.f, total - viewport.h);
			}

			math::NkVec2f NkGemMapGeometry::NodeCenter(int32 levelIndex) const noexcept {
				const float32 i = static_cast<float32>(math::NkClamp(levelIndex, 1, levelCount) - 1);
				// Serpentin : une sinusoïde, pas un zigzag. Le chemin doit être
				// lisible d'un coup d'œil, pas amusant à décrire.
				// Période ~5 nœuds : mesuré à l'écran, 0,78 rad donnait une
				// période de 8 nœuds, dont on ne voit jamais qu'un morceau — le
				// chemin se lisait comme une DIAGONALE, pas comme un serpentin.
				const float32 x = viewport.x + viewport.w * 0.5f + math::NkSin(i * 1.25f) * amplitude;
				const float32 bottom = viewport.y + viewport.h - nodeRadius * 2.4f;
				const float32 y = bottom - i * spacing + scroll;
				return math::NkVec2f(x, y);
			}

			int32 NkGemMapGeometry::HitTest(const NkVec2f &point) const noexcept {
				for (int32 level = 1; level <= levelCount; ++level) {
					const NkVec2f c = NodeCenter(level);
					if (c.y < viewport.y - nodeRadius * 2.f || c.y > viewport.y + viewport.h + nodeRadius * 2.f) {
						continue; // hors écran : ni dessiné, ni cliquable
					}
					const float32 dx = point.x - c.x;
					const float32 dy = point.y - c.y;
					// Rayon de CLIC plus large que le rayon dessiné : un doigt
					// vise moins bien qu'une souris, et rater un nœud est la
					// frustration la plus banale d'une carte de niveaux.
					const float32 reach = nodeRadius * 1.35f;
					if (dx * dx + dy * dy <= reach * reach) {
						return level;
					}
				}
				return -1;
			}

			float32 NkGemMapGeometry::ScrollToCenter(int32 levelIndex) const noexcept {
				const float32 i = static_cast<float32>(math::NkClamp(levelIndex, 1, levelCount) - 1);
				const float32 bottom = viewport.h - nodeRadius * 2.4f;
				return math::NkClamp(i * spacing - bottom + viewport.h * 0.55f, 0.f, MaxScroll());
			}

			// =========================================================
			// Carte d'aventure — dessin
			// =========================================================
			NkGemMapButtons NkGemDrawAdventureMap(NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout,
												  const NkGemMapGeometry &geometry, const NkGemProgress &progress,
												  float32 time, const NkVec2f &pointer) {
				const NkGemTheme &th = NkTheme();
				NkGemMapButtons buttons;
				const float32 s = layout.scale;
				const int32 highest = progress.GetHighestUnlocked();

				// -- Chemin : d'abord le tracé, ensuite les nœuds ----------
				// Deux passes, sinon le trait coupe les pastilles.
				for (int32 level = 1; level < geometry.levelCount; ++level) {
					const NkVec2f a = geometry.NodeCenter(level);
					const NkVec2f b = geometry.NodeCenter(level + 1);
					if ((a.y < geometry.viewport.y - 80.f && b.y < geometry.viewport.y - 80.f) ||
						(a.y > geometry.viewport.y + geometry.viewport.h + 80.f &&
						 b.y > geometry.viewport.y + geometry.viewport.h + 80.f)) {
						continue;
					}
					// Le segment déjà PARCOURU est doré, le reste est éteint :
					// la couleur porte la progression, pas seulement les nœuds.
					const bool walked = (level < highest);
					const NkColor colour = walked ? NkWithAlpha(th.accent, 0.55f) : NkColor(255, 255, 255, 30);
					const NkVec2f pts[2] = {a, b};
					dl.AddPolyline(pts, 2, colour, math::NkMax(3.f, 7.f * s), false);
				}

				for (int32 level = 1; level <= geometry.levelCount; ++level) {
					const NkVec2f c = geometry.NodeCenter(level);
					if (c.y < geometry.viewport.y - geometry.nodeRadius * 3.f ||
						c.y > geometry.viewport.y + geometry.viewport.h + geometry.nodeRadius * 3.f) {
						continue;
					}
					const bool unlocked = progress.IsUnlocked(level);
					const bool current = (level == highest);
					const int32 stars = progress.GetStars(level);
					const NkGemLevelDef def = NkMakeLevel(level);

					// Le niveau COURANT pulse : c'est le seul repère dont le
					// joueur a besoin en ouvrant la carte.
					if (current) {
						const float32 pulse = 0.5f + 0.5f * math::NkSin(time * 3.f);
						// Le halo prend la couleur de SA pastille : doré sur un
						// niveau déjà réussi, cyan sur un niveau neuf. Un halo
						// doré sur une pastille cyan rendait un anneau brunâtre.
						const NkColor haloColour = (progress.GetStars(level) > 0) ? th.accent : th.accentCool;
						dl.AddCircleFilled(c, geometry.nodeRadius * (1.45f + pulse * 0.18f),
										   NkWithAlpha(haloColour, 0.16f + 0.12f * pulse));
					}

					// Pastille : socle + face, comme les boutons — un nœud est
					// un bouton, il doit s'annoncer comme tel.
					const NkColor face = !unlocked	? NkColor(52, 46, 92, 255)
										 : (stars > 0 ? th.accent : th.accentCool);
					const NkColor base = NkLerpColor(face, NkColor(0, 0, 0, 255), 0.45f);
					const bool hot = NkGemHudButtons::Contains(
						NkRect{c.x - geometry.nodeRadius, c.y - geometry.nodeRadius, geometry.nodeRadius * 2.f,
							   geometry.nodeRadius * 2.f},
						pointer);
					dl.AddCircleFilled(NkVec2f(c.x, c.y + geometry.nodeRadius * 0.16f), geometry.nodeRadius, base);
					dl.AddCircleFilled(c, geometry.nodeRadius,
									   hot && unlocked ? NkLerpColor(face, NkColor(255, 255, 255, 255), 0.18f) : face);

					if (!unlocked) {
						Padlock(dl, c, geometry.nodeRadius * 1.15f, NkColor(150, 142, 190, 255));
					} else {
						const NkString number = NkString::Fmt("{0}", level);
						const NkRect box{c.x - geometry.nodeRadius, c.y - geometry.nodeRadius,
										 geometry.nodeRadius * 2.f, geometry.nodeRadius * 2.f};
						TextCentered(dl, fonts.body, box,
									 c.y - LineH(fonts.body, 18.f * s) * 0.5f, number.CStr(), th.textOnAccent);

						// Trois étoiles au-dessus, seulement si le niveau est fini.
						if (stars > 0) {
							const float32 sr = geometry.nodeRadius * 0.30f;
							for (int32 k = 0; k < 3; ++k) {
								Star(dl, NkVec2f(c.x + (static_cast<float32>(k) - 1.f) * sr * 2.3f,
												 c.y - geometry.nodeRadius - sr * 1.1f),
									 sr, k < stars);
							}
						}
						// Pastille de MODE sous le nœud : le joueur doit savoir
						// ce qui l'attend avant d'entrer, surtout pour le chrono.
						if (def.mode != NkGemMode::NK_MODE_MOVES) {
							const char *label = NkGemModeName(def.mode);
							const float32 w = MeasureW(fonts.small, label) + 14.f * s;
							const float32 h = LineH(fonts.small, 13.f * s) + 6.f * s;
							const NkRect tag{c.x - w * 0.5f, c.y + geometry.nodeRadius + 4.f * s, w, h};
							dl.AddRectFilled(tag, NkColor(20, 16, 44, 220), h * 0.5f);
							dl.AddRect(tag, NkWithAlpha(def.mode == NkGemMode::NK_MODE_TIMED ? th.danger : th.accentCool,
														0.9f),
									   1.f, h * 0.5f);
							TextCentered(dl, fonts.small, tag, tag.y + 3.f * s, label,
										 def.mode == NkGemMode::NK_MODE_TIMED ? th.danger : th.accentCool);
						}
					}
				}

				// -- En-tête flottant : titre + retour ---------------------
				const float32 headerH = math::NkMax(NkTheme().minTouchTarget * s, 52.f * s);
				const NkRect header{layout.safe.x, layout.safe.y, layout.safe.w, headerH};
				dl.AddRectFilled(NkRect{header.x - 6.f, header.y - 6.f, header.w + 12.f, header.h + 12.f},
								 NkColor(16, 13, 38, 205), th.radiusPanel);
				buttons.back = NkRect{header.x, header.y, headerH, headerH};
				dl.AddRectFilled(buttons.back, NkColor(255, 255, 255, NkGemHudButtons::Contains(buttons.back, pointer) ? 46u : 26u),
								 th.radiusChip);
				// Flèche retour, dessinée (aucun glyphe fléché dans l'atlas).
				{
					const float32 g = headerH * 0.22f;
					const NkVec2f centre(buttons.back.x + headerH * 0.5f, buttons.back.y + headerH * 0.5f);
					const NkVec2f arrow[3] = {NkVec2f(centre.x - g, centre.y), NkVec2f(centre.x + g * 0.5f, centre.y - g),
											  NkVec2f(centre.x + g * 0.5f, centre.y + g)};
					dl.AddConvexPolyFilled(arrow, 3, th.textPrimary);
				}
				TextCentered(dl, fonts.body, header, header.y + (header.h - LineH(fonts.body, 18.f * s)) * 0.5f,
							 "AVENTURE", th.textPrimary);

				return buttons;
			}

		} // namespace ui
	} // namespace game
} // namespace nkentseu
