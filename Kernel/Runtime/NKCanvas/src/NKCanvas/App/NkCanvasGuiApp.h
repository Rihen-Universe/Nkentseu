// =============================================================================
// NkCanvasGuiApp.h — coquille NKCanvas + interface NKGui, EN-TETE SEUL
//
// A QUOI SERT CE FICHIER
//   NkCanvasApp donne la fenetre, la boucle et le cycle de vie. Il ne donne
//   AUCUN moyen de dessiner un rectangle arrondi ou une ligne de texte. Ce
//   fichier ajoute cet etage : contexte NKGui, backend, trois polices, et la
//   liste d'affichage prete a l'emploi a chaque trame.
//
// ⚠️ POURQUOI IL N'A PAS DE .cpp — CE N'EST PAS UN OUBLI
//   Un `.cpp` dans NKCanvas force TOUS ses dependants a compiler et lier ce
//   qu'il inclut. Si cet etage avait un `.cpp`, les quatorze dependants de
//   NKCanvas lieraient NKGui, y compris ceux qui n'en veulent pas.
//   C'est EXACTEMENT le mecanisme qui a fait que NKCanvas liait NKUI — module
//   deprecie — pendant des mois : `NkUICanvasBackend.cpp` existait, et sa seule
//   existence suffisait. `NkGuiCanvasBackend.h`, lui, n'a jamais eu de `.cpp`,
//   et c'est la seule raison pour laquelle NKGui n'a jamais ete impose.
//   NE PAS AJOUTER DE .cpp ICI.
//
// CE QU'IL REGLE ET QU'ON OUBLIE TOUJOURS
//   1. UN texId DISTINCT PAR POLICE. Toute NkGuiFont porte le meme identifiant
//      par defaut : deux polices qui le gardent partagent le meme atlas cote
//      backend et s'ecrasent. Piege paye dans Mou, puis dans Gemcrush. Ici il
//      est impossible de l'oublier.
//   2. LA TAILLE DE POLICE SUIT L'ECRAN, et se RECHARGE a la rotation : un
//      atlas construit pour 14 px reste flou affiche a 28 px.
//   3. LES AIDES DE TEXTE — la vraie dette. `NkGuiDrawList::AddText` ne sait
//      ecrire qu'a la LIGNE DE BASE : ni centrer, ni caler a droite, ni
//      mesurer une hauteur de ligne. C'est ce manque qui force chaque
//      application a reecrire les memes quinze lignes.
//      Mesure : TROIS copies dans le depot avant ce fichier —
//        Applications/Mou/src/Mou/UI/MouDraw.h
//        Applications/Nkoung/...
//        Applications/Gemcrush/src/Gemcrush/Ui/NkGemHud.cpp  (dette datee du
//          2026-08-27, qui nommait deja NKGui comme destination)
//      Trois jeux de plus en auraient fait six. Elles vivent ici desormais.
//
//   📌 LEUR PLACE DEFINITIVE RESTE NKGUI, pas NKCanvas : une application qui
//   utilise NKGui SANS NKCanvas (NKCode, NK3DModeler) ne les voit toujours pas.
//   Le jour ou quelqu'un ouvre NKGui pour autre chose, elles descendent d'un
//   etage sous le nom NkGuiDrawText.h et cet en-tete les re-exporte. Tant que
//   ce n'est pas fait, ce fichier evite au moins que le compte monte.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une aide de dessin utile a TOUTE application  -> ici
//   - une aide propre a un jeu                      -> chez le jeu
//   - une primitive qui manque a NkGuiDrawList      -> dans NKGui, pas ici
// =============================================================================
#pragma once

#include "NKCanvas/App/NkCanvasApp.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"
#include "NKGui/Core/NkGuiContext.h"

namespace nkentseu {
	namespace renderer {

		class NkCanvasGuiApp : public NkCanvasApp {
			protected:
				// --- A remplir par le jeu -------------------------------------

				/// Dessin de la trame. `dl` est deja ouverte et sera soumise.
				virtual void OnDraw(nkgui::NkGuiDrawList &dl) {
					(void)dl;
				}

				/// Appele apres le chargement des polices. Rendre false = abandon.
				virtual bool OnGuiInit() {
					return true;
				}

				/// Le pas de temps du jeu. (OnUpdate est deja pris par le relais
				/// interne du delta vers NKGui : le redefinir couperait la trame.)
				virtual void OnTick(float32 deltaTime) {
					(void)deltaTime;
				}

				// --- Services -------------------------------------------------
				nkgui::NkGuiContext &Gui() noexcept {
					return mGuiContext;
				}
				nkgui::NkGuiFont *FontBody() noexcept {
					return &mFontBody;
				}
				nkgui::NkGuiFont *FontTitle() noexcept {
					return &mFontTitle;
				}
				nkgui::NkGuiFont *FontSmall() noexcept {
					return &mFontSmall;
				}

				// =============================================================
				// AIDES DE TEXTE — voir la dette en tete de fichier.
				//
				// ⚠️ `topY` est le HAUT du texte, pas sa ligne de base. C'est la
				// difference qui oblige a ajouter Ascent(), et c'est exactement
				// ce que les trois copies precedentes corrigeaient chacune de
				// leur cote. Toutes tolerent une police nulle : une application
				// sans police doit rester dessinable, pas planter.
				// =============================================================
				static float32 MeasureW(nkgui::NkGuiFont *f, const char *s) noexcept {
					return (f != nullptr && s != nullptr) ? f->MeasureWidth(s) : 0.f;
				}

				static float32 LineH(nkgui::NkGuiFont *f, float32 fallback) noexcept {
					return (f != nullptr && f->Face() != nullptr) ? f->LineHeight() : fallback;
				}

				/// `maxWidth >= 0` tronque proprement au lieu de deborder.
				static void Text(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 x, float32 topY, const char *s,
								 const nkgui::NkColor &c, float32 maxWidth = -1.f) noexcept {
					if (f != nullptr && f->Face() != nullptr && s != nullptr) {
						dl.AddText(f->Face(), f->TexId(), math::NkVec2f(x, topY + f->Ascent()), s, c, maxWidth);
					}
				}

				static void TextCentered(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 cx, float32 topY,
										 const char *s, const nkgui::NkColor &c) noexcept {
					Text(dl, f, cx - MeasureW(f, s) * 0.5f, topY, s, c);
				}

				/// Centre dans une boite, horizontalement ET verticalement.
				/// C'est la forme d'un libelle de bouton — celle qu'on veut 9
				/// fois sur 10 et que personne n'a jamais ecrite du premier coup.
				static void TextInBox(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, const nkgui::NkRect &box, const char *s,
									  const nkgui::NkColor &c) noexcept {
					if (f == nullptr || f->Face() == nullptr) {
						return;
					}
					const float32 topY = box.y + (box.h - f->LineHeight()) * 0.5f;
					Text(dl, f, box.x + (box.w - MeasureW(f, s)) * 0.5f, topY, s, c);
				}

				static void TextRight(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *f, float32 right, float32 topY,
									  const char *s, const nkgui::NkColor &c) noexcept {
					Text(dl, f, right - MeasureW(f, s), topY, s, c);
				}

				// --- Cycle de vie de la coquille ------------------------------
				bool OnInit() override {
					const NkLayoutInfo &lay = Layout();

					if (!mGuiContext.Init(static_cast<int32>(lay.width), static_cast<int32>(lay.height))) {
						logger.Error("[nkcanvasgui] initialisation de NkGuiContext ECHOUEE");
						return false;
					}
					nkgui::SetCurrentContext(&mGuiContext);

					if (!mGuiBackend.Init(Target().GetRenderer())) {
						logger.Error("[nkcanvasgui] initialisation de NkGuiCanvasBackend ECHOUEE");
						return false;
					}

					LoadFonts(SuggestedBodyPx(lay));
					mGuiContext.font = &mFontBody;
					mGuiReady = true;
					return OnGuiInit();
				}

				void OnUpdate(float32 deltaTime) final {
					// La liste d'affichage reclame le pas de temps a BeginFrame,
					// plus tard dans la trame. On le retient plutot que de le
					// recalculer : deux horloges qui doivent s'accorder finissent
					// toujours par diverger.
					mLastDelta = deltaTime;
					OnTick(deltaTime);
				}

				void OnRender(NkRenderWindow &target) final {
					const math::NkVec2u size = target.GetSize();
					mGuiContext.BeginFrame(mLastDelta);
					OnDraw(mGuiContext.dl);
					mGuiContext.EndFrame();
					mGuiBackend.Submit(mGuiContext.dl, size.x, size.y);
					mGuiBackend.Submit(mGuiContext.dlOverlay, size.x, size.y);
				}

				void OnLayout(const NkLayoutInfo &layout) override {
					if (!mGuiReady) {
						return; // OnInit n'a pas encore tourne : rien a recharger
					}
					const float32 wanted = SuggestedBodyPx(layout);
					const float32 ecart = wanted > mLoadedPx ? wanted - mLoadedPx : mLoadedPx - wanted;
					if (ecart < 1.5f) {
						return; // sous ce seuil le rechargement ne se voit pas
					}
					LoadFonts(wanted);
				}

			private:
				static float32 SuggestedBodyPx(const NkLayoutInfo &lay) noexcept {
					// La police suit la plus PETITE dimension : c'est elle qui
					// borne la lecture, en portrait comme en paysage.
					const float32 minSide = static_cast<float32>(lay.width < lay.height ? lay.width : lay.height);
					const float32 px = minSide * 0.036f;
					return px < 11.f ? 11.f : (px > 34.f ? 34.f : px);
				}

				void LoadFonts(float32 bodyPx) {
					// ⚠️ texId DISTINCT par police — voir l'en-tete de fichier.
					mFontBody.texId = 0x4E4B4654u;
					mFontTitle.texId = mFontBody.texId + 1u;
					mFontSmall.texId = mFontBody.texId + 2u;

					const bool ok = mFontBody.LoadEmbedded(NkEmbeddedFontId::DroidSans, bodyPx) ||
									mFontBody.LoadEmbedded(NkEmbeddedFontId::ProggyClean, bodyPx);
					mFontTitle.LoadEmbedded(NkEmbeddedFontId::DroidSans, bodyPx * 1.85f);
					mFontSmall.LoadEmbedded(NkEmbeddedFontId::DroidSans, bodyPx * 0.78f);

					nkgui::NkGuiFont *fonts[3] = {&mFontBody, &mFontTitle, &mFontSmall};
					for (int32 i = 0; i < 3; ++i) {
						if (fonts[i]->pixels != nullptr && fonts[i]->atlasW > 0 && fonts[i]->atlasH > 0) {
							mGuiBackend.UploadFontGray8(fonts[i]->TexId(), fonts[i]->pixels, fonts[i]->atlasW,
														fonts[i]->atlasH);
						}
					}
					if (!ok) {
						logger.Warn("[nkcanvasgui] aucune police chargee — les textes seront absents");
					}
					mLoadedPx = bodyPx;
				}

				nkgui::NkGuiContext mGuiContext;
				NkGuiCanvasBackend mGuiBackend;
				nkgui::NkGuiFont mFontBody, mFontTitle, mFontSmall;
				float32 mLoadedPx = 0.f;
				float32 mLastDelta = 1.f / 60.f;
				bool mGuiReady = false;
		};

	} // namespace renderer
} // namespace nkentseu
