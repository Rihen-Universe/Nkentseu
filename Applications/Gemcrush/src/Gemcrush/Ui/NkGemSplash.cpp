// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemSplash.cpp
// DESCRIPTION: Implémentation des écrans d'ouverture.
//
//              COULEURS DE MARQUE : pétrole #0A555F et orange #F79A28 — la
//              charte Rihen. Elles ne viennent PAS du thème du jeu et ne
//              doivent pas en suivre les variations : une marque ne change pas
//              de couleur parce que le jeu change d'ambiance.
//
// AUTEUR: Rihen
// DATE: 2026-08-28
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Gemcrush/Ui/NkGemSplash.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			using nkgui::NkGuiDrawList;
			using nkgui::NkGuiFont;
			using math::NkVec2f;

			namespace {

				// Charte Rihen — cf. CLAUDE.md du dépôt.
				const NkColor kRihenPetrole(10, 85, 95, 255);
				const NkColor kRihenPetroleFonce(5, 48, 55, 255);
				const NkColor kRihenOrange(247, 154, 40, 255);

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

			} // namespace

			// =========================================================
			// Minutage
			// =========================================================
			bool NkGemSplash::Update(float32 deltaTime) {
				if (mDone) {
					return true;
				}
				mTime += deltaTime;
				if (mTime >= kPanelDuration) {
					mTime = 0.f;
					++mPanel;
					if (mPanel > 1) {
						mDone = true;
					}
				}
				return mDone;
			}

			void NkGemSplash::Skip() {
				// Un appui passe au volet SUIVANT, deux appuis terminent. Sauter
				// volet par volet plutôt que tout d'un coup : celui qui appuie
				// par réflexe au lancement ne rate pas les deux écrans.
				if (mDone) {
					return;
				}
				mTime = 0.f;
				++mPanel;
				if (mPanel > 1) {
					mDone = true;
				}
			}

			float32 NkGemSplash::PanelAlpha() const noexcept {
				if (mTime < kFadeIn) {
					return math::NkClamp(mTime / kFadeIn, 0.f, 1.f);
				}
				if (mTime < kFadeIn + kHold) {
					return 1.f;
				}
				return math::NkClamp(1.f - (mTime - kFadeIn - kHold) / kFadeOut, 0.f, 1.f);
			}

			// =========================================================
			// Dessin
			// =========================================================
			void NkGemSplash::Draw(NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout) {
				if (mDone) {
					return;
				}
				const float32 alpha = PanelAlpha();
				if (mPanel == 0) {
					DrawRihen(dl, fonts, layout, alpha);
				} else {
					DrawEngine(dl, fonts, layout, alpha);
				}
			}

			// ---------------------------------------------------------
			// Volet 1 : la marque
			// ---------------------------------------------------------
			void NkGemSplash::DrawRihen(NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout,
										float32 alpha) {
				const NkRect full{0.f, 0.f, layout.width, layout.height};
				const float32 s = layout.scale;

				// Fond pétrole PLEIN ÉCRAN, encoche comprise : un fond de marque
				// qui s'arrêterait à la zone sûre laisserait des bandes noires.
				dl.AddRectFilledMultiColor(full, NkWithAlpha(kRihenPetrole, alpha), NkWithAlpha(kRihenPetrole, alpha),
										   NkWithAlpha(kRihenPetroleFonce, alpha),
										   NkWithAlpha(kRihenPetroleFonce, alpha));

				const float32 cx = layout.width * 0.5f;
				const float32 cy = layout.height * 0.5f;

				// Une gemme qui tourne lentement, dans l'orange de la charte.
				// Elle vient du MÊME code que celles du plateau : la marque et le
				// jeu montrent le même objet, ce qui les relie sans un mot.
				{
					const float32 pulse = 0.5f + 0.5f * math::NkSin(mTime * 2.2f);
					NkGemVisual gem;
					gem.center = NkVec2f(cx, cy - 58.f * s);
					gem.size = (74.f + pulse * 5.f) * s;
					gem.color = NkGemColor::NK_GEM_COLOR_ORANGE;
					gem.shape = NkGemShape::NK_GEM_SHAPE_HEXAGON;
					gem.alpha = alpha;
					gem.shadow = false;
					gem.time = mTime;
					NkGemArt::Draw(dl, gem);
				}

				// Le mot. Pas de logo inventé : la charte donne les couleurs, pas
				// un dessin de marque — en fabriquer un serait une invention.
				const float32 titleH = LineH(fonts.title, 34.f * s);
				TextCentered(dl, fonts.title, full, cy + 8.f * s, "RIHEN", NkWithAlpha(NkColor(255, 255, 255, 255), alpha));

				// Filet orange sous le mot, qui se TRACE pendant le fondu entrant.
				{
					const float32 grow = math::NkClamp(mTime / (kFadeIn * 1.6f), 0.f, 1.f);
					const float32 w = math::NkMin(layout.width * 0.42f, 240.f * s) * grow;
					dl.AddRectFilled(NkRect{cx - w * 0.5f, cy + 8.f * s + titleH * 1.02f, w, 3.f * s},
									 NkWithAlpha(kRihenOrange, alpha), 2.f);
				}

				TextCentered(dl, fonts.small, full, cy + 8.f * s + titleH * 1.02f + 14.f * s, "RIHEN UNIVERSE",
							 NkWithAlpha(NkColor(200, 226, 230, 255), alpha * 0.85f));
			}

			// ---------------------------------------------------------
			// Volet 2 : le moteur
			// ---------------------------------------------------------
			void NkGemSplash::DrawEngine(NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout,
										 float32 alpha) {
				const NkGemTheme &th = NkTheme();
				const NkRect full{0.f, 0.f, layout.width, layout.height};
				const float32 s = layout.scale;

				dl.AddRectFilledMultiColor(full, NkWithAlpha(th.backdropTop, alpha), NkWithAlpha(th.backdropTop, alpha),
										   NkWithAlpha(th.backdropBottom, alpha), NkWithAlpha(th.backdropBottom, alpha));

				const float32 cx = layout.width * 0.5f;
				const float32 cy = layout.height * 0.5f;

				// Six gemmes en arc, une par couleur du jeu : le moteur dessine
				// tout ce qu'on voit, autant le montrer.
				for (int32 i = 0; i < 6; ++i) {
					const float32 t = static_cast<float32>(i) / 5.f;
					// Elles arrivent l'une après l'autre pendant le fondu entrant.
					const float32 appear = math::NkClamp((mTime - t * 0.10f) / kFadeIn, 0.f, 1.f);
					if (appear <= 0.f) {
						continue;
					}
					const float32 angle = math::NK_PI_F * (0.18f + 0.64f * t);
					const float32 radius = math::NkMin(layout.width * 0.30f, 150.f * s);
					NkGemVisual gem;
					gem.center = NkVec2f(cx - math::NkCos(angle) * radius, cy - 78.f * s - math::NkSin(angle) * radius * 0.42f);
					gem.size = 40.f * s;
					gem.color = static_cast<NkGemColor>(1 + i);
					gem.shape = NkGemShapeForColor(gem.color);
					gem.alpha = alpha * appear;
					gem.scale = appear;
					gem.shadow = false;
					gem.time = mTime;
					NkGemArt::Draw(dl, gem);
				}

				float32 y = cy + 6.f * s;
				TextCentered(dl, fonts.small, full, y, "PROPULSE PAR", NkWithAlpha(th.textSecondary, alpha));
				y += LineH(fonts.small, 14.f * s) + 4.f * s;
				TextCentered(dl, fonts.title, full, y, "NKENTSEU", NkWithAlpha(th.accent, alpha));
				y += LineH(fonts.title, 34.f * s) + 10.f * s;

				// Les modules réellement utilisés par CE jeu, pas une liste
				// d'apparat : NKCanvas rend, NKGui dessine, NKAudio sonne.
				TextCentered(dl, fonts.body, full, y, "NKCANVAS   NKGUI   NKAUDIO",
							 NkWithAlpha(th.accentCool, alpha * 0.95f));
				y += LineH(fonts.body, 18.f * s) + 18.f * s;
				TextCentered(dl, fonts.small, full, y, "MOTEUR C++ SANS BIBLIOTHEQUE STANDARD",
							 NkWithAlpha(th.textSecondary, alpha * 0.8f));
			}

		} // namespace ui
	} // namespace game
} // namespace nkentseu
