// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemTutorial.cpp
// DESCRIPTION: Implémentation du didacticiel des trois premiers niveaux.
// AUTEUR: Rihen
// DATE: 2026-08-28
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Gemcrush/Ui/NkGemTutorial.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			using nkgui::NkGuiDrawList;
			using nkgui::NkGuiFont;
			using math::NkVec2f;

			namespace {

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

				/// Main qui glisse : deux cercles reliés, le second en pointillé.
				/// Dessinée, pas écrite — aucun glyphe de main dans l'atlas, et une
				/// flèche seule ne dit pas « pose le doigt PUIS tire ».
				void DrawSwipeHint(NkGuiDrawList &dl, const NkVec2f &from, float32 travel, float32 radius,
								   const NkColor &color, float32 phase) {
					const float32 t = 0.5f - 0.5f * math::NkCos(phase); // aller-retour doux
					const NkVec2f moving(from.x + travel * t, from.y);
					// Traînée : trois pastilles de plus en plus pâles derrière.
					for (int32 i = 3; i >= 1; --i) {
						const float32 k = static_cast<float32>(i) / 3.f;
						dl.AddCircleFilled(NkVec2f(from.x + travel * t * (1.f - k * 0.45f), from.y), radius * 0.55f,
										   NkWithAlpha(color, 0.10f * k));
					}
					dl.AddCircle(NkVec2f(from.x + travel, from.y), radius * 0.85f, NkWithAlpha(color, 0.55f),
								 math::NkMax(1.5f, radius * 0.10f));
					dl.AddCircleFilled(moving, radius * 0.62f, NkWithAlpha(color, 0.85f));
					dl.AddCircle(moving, radius * 0.62f, NkWithAlpha(NkColor(255, 255, 255, 255), 0.8f),
								 math::NkMax(1.5f, radius * 0.09f));
				}

			} // namespace

			// =========================================================
			// Script
			// =========================================================
			void NkGemTutorial::Begin(int32 levelIndex) {
				mLevel = levelIndex;
				mStep = 0;
				mTime = 0.f;
				mPulse = 0.f;
				mActive = (levelIndex >= 1 && levelIndex <= 3);
				// Seul le niveau 1 garde l'indice allumé : au niveau 2, le joueur
				// sait déjà jouer, et un plateau qui clignote en permanence
				// l'empêcherait de lire ses objectifs.
				mPermanentHint = (levelIndex == 1);
			}

			const char *NkGemTutorial::Title() const noexcept {
				if (mStep == 1) {
					return (mLevel == 1) ? "BIEN JOUE" : ((mLevel == 2) ? "C'EST CA" : "PARFAIT");
				}
				switch (mLevel) {
					case 1:
						return "GLISSE UNE GEMME";
					case 2:
						return "VISE TON OBJECTIF";
					case 3:
						return "CREE UNE GEMME SPECIALE";
					default:
						return "";
				}
			}

			const char *NkGemTutorial::Body() const noexcept {
				if (mStep == 1) {
					switch (mLevel) {
						case 1:
							return "Trois gemmes alignees disparaissent. Enchaine.";
						case 2:
							return "Les etoiles fondent avec le temps : va vite.";
						default:
							return "Echange-la pour declencher son effet.";
					}
				}
				switch (mLevel) {
					case 1:
						return "Tire-la vers sa voisine pour aligner trois couleurs.";
					case 2:
						return "En haut : les gemmes a collecter. C'est ELLES qui font gagner.";
					case 3:
						return "Aligne QUATRE gemmes de la meme couleur.";
					default:
						return "";
				}
			}

			NkGemTutorialEvent NkGemTutorial::AwaitedEvent() const noexcept {
				switch (mLevel) {
					case 1:
						return NkGemTutorialEvent::MatchResolved;
					case 2:
						return NkGemTutorialEvent::ObjectiveGained;
					default:
						return NkGemTutorialEvent::SpecialCreated;
				}
			}

			void NkGemTutorial::NextStep() {
				++mStep;
				mTime = 0.f;
				if (mStep >= 2) {
					mActive = false;
				}
				mPermanentHint = false; // dès la première réussite, on rend le plateau au joueur
			}

			void NkGemTutorial::Notify(NkGemTutorialEvent event) {
				if (!mActive || mStep != 0) {
					return;
				}
				if (event == AwaitedEvent()) {
					NextStep();
				}
			}

			void NkGemTutorial::Update(float32 deltaTime) {
				if (!mActive) {
					return;
				}
				mTime += deltaTime;
				mPulse += deltaTime * 2.4f;

				// SORTIE DE SECOURS. Le joueur peut très bien faire autre chose que
				// ce qu'on attend : une consigne qui l'attendrait indéfiniment
				// deviendrait un mur. Elle s'efface d'elle-même.
				const float32 limit = (mStep == 0) ? kStepTimeout : kPraiseDuration;
				if (mTime >= limit) {
					NextStep();
				}
			}

			// =========================================================
			// Dessin
			// =========================================================
			void NkGemTutorial::Draw(NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout) {
				if (!mActive) {
					return;
				}
				const NkGemTheme &th = NkTheme();
				const float32 s = layout.scale;

				// Fondu entrant court, fondu sortant sur la dernière demi-seconde.
				const float32 limit = (mStep == 0) ? kStepTimeout : kPraiseDuration;
				const float32 alpha = math::NkMin(math::NkClamp(mTime / 0.35f, 0.f, 1.f),
												  math::NkClamp((limit - mTime) / 0.5f, 0.f, 1.f));
				if (alpha <= 0.01f) {
					return;
				}

				// La bannière se pose SOUS le plateau, jamais dessus : masquer les
				// gemmes pendant qu'on explique comment les bouger serait absurde.
				const float32 pad = 14.f * s;
				const float32 titleH = LineH(fonts.body, 18.f * s);
				const float32 bodyH = LineH(fonts.small, 14.f * s);
				const float32 boxH = titleH + bodyH + pad * 1.8f;
				const float32 boxW = math::NkMin(layout.safe.w, 380.f * s);
				const float32 boxX = layout.boardArea.x + (layout.boardArea.w - boxW) * 0.5f;

				// PLACEMENT : sous le plateau si la place existe VRAIMENT, sinon
				// au-dessus. On teste l intersection avec la barre de boutons, pas
				// une marge devinee — en paysage les boutons sont dans la colonne
				// de gauche et ne genent pas, une regle sur Y seule deplacerait la
				// banniere pour rien.
				//
				// ⚠️ Defaut mesure a l ecran : la banniere se posait PAR-DESSUS
				// INDICE et MELANGER. Un bloc qu on place sans regarder ce qu il y
				// a dessous finit toujours sur autre chose.
				auto chevauche = [](const NkRect &a, const NkRect &b) {
					return b.w > 0.f && b.h > 0.f && a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
						   b.y < a.y + a.h;
				};

				// ⚠️ ON SE REFERE AU CADRE, PAS A LA ZONE DE JEU.
				//
				// Mesure du 2026-08-28 : la zone de jeu fait 400 px de haut, le
				// CADRE qui l entoure 500. Se placer sous la zone posait donc la
				// banniere A L INTERIEUR du cadre, par-dessus la derniere rangee
				// de gemmes. Le rectangle qu on VOIT est le cadre ; c est lui qui
				// doit servir de reference, pas le decoupage logique.
				bool surLePlateau = false; ///< la banniere recouvre-t-elle des gemmes ?
				float32 boxY = layout.boardPanel.y + layout.boardPanel.h + 10.f * s;
				NkRect box{boxX, boxY, boxW, boxH};
				const bool tropBas = (boxY + boxH > layout.safe.y + layout.safe.h);
				if (tropBas || chevauche(box, layout.bottomBar)) {
					// Au-dessus du plateau, sous la barre de score.
					boxY = layout.boardPanel.y - boxH - 10.f * s;
					box = NkRect{boxX, boxY, boxW, boxH};
					if (boxY < layout.topBar.y + layout.topBar.h || chevauche(box, layout.topBar)) {
						// Ni dessous ni dessus. En portrait c est le cas NORMAL :
						// le plateau occupe toute la hauteur entre le score et les
						// boutons, il n y a aucune bande libre de 60 px.
						//
						// On se pose donc SUR le plateau, mais sur sa MARGE BASSE
						// (celle du cadre, pas la premiere rangee de gemmes) et en
						// TRANSLUCIDE : la rangee du dessous reste lisible. Masquer
						// des gemmes vaut mieux que masquer les boutons, mais les
						// masquer a moitie vaut mieux que les masquer tout court.
						boxY = layout.boardPanel.y + layout.boardPanel.h - boxH - 4.f * s;
						box = NkRect{boxX, boxY, boxW, boxH};
						surLePlateau = true;
					}
				}

				dl.AddRectFilled(NkRect{box.x, box.y + 3.f * s, box.w, box.h}, NkWithAlpha(th.shadow, alpha),
								 th.radiusPanel);
				// Translucide UNIQUEMENT quand elle est posee sur les gemmes.
				dl.AddRectFilled(box, NkWithAlpha(NkColor(28, 24, 60, surLePlateau ? 205 : 245), alpha),
								 th.radiusPanel);
				dl.AddRect(box, NkWithAlpha(mStep == 1 ? th.accent : th.accentCool, alpha), 2.f * s, th.radiusPanel);

				TextCentered(dl, fonts.body, box, box.y + pad * 0.7f, Title(),
							 NkWithAlpha(mStep == 1 ? th.accent : th.accentCool, alpha));
				TextCentered(dl, fonts.small, box, box.y + pad * 0.7f + titleH, Body(),
							 NkWithAlpha(th.textPrimary, alpha));

				// Niveau 1, première consigne : la main qui glisse, posée sur le
				// plateau. C'est le seul niveau où le GESTE lui-même est l'inconnue.
				if (mLevel == 1 && mStep == 0) {
					const float32 cell = layout.cellSize;
					const NkVec2f from(layout.boardArea.x + cell * 1.5f,
									   layout.boardArea.y + layout.boardArea.h * 0.5f);
					DrawSwipeHint(dl, from, cell, cell * 0.34f, NkWithAlpha(th.accentCool, alpha), mPulse);
				}
			}

		} // namespace ui
	} // namespace game
} // namespace nkentseu
