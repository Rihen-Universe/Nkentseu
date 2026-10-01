#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkEditorRendererMemoire.h — un `NkIEditorRenderer` SANS FENETRE NI GPU.
//
// POURQUOI (2026-10-01). Photographier l'ecran d'accueil de NKCraft demandait
// jusqu'ici une fenetre a l'ecran : la capture lit la fenetre composee
// (`PrintWindow`), et le dorsal RHI n'a pas de relecture. Or l'interface entiere
// n'est qu'une `NkGuiDrawList`, et NKGui sait deja la rasteriser sans GPU
// (`NkGuiDrawListRaster`). Il manquait seulement QUELQU'UN QUI GARDE LES
// TEXTURES : la police, les icones (SVG decodes), les vignettes et l'image de
// version arrivent par `UploadFontGray8` / `UploadImageRGBA` -- c'est-a-dire
// par le renderer. Celui-ci les GARDE en memoire au lieu de les envoyer au GPU,
// et rend chaque liste soumise dans une image RGBA8 qu'on ecrit en PNG.
//
// ⚠️ CE N'EST PAS UNE NEUVIEME COPIE DE CAPTURE. Il ne lit aucun pixel d'ecran :
//    il RASTERISE la meme liste que le GPU aurait recue, par le rasteriseur de
//    la couche du dessous. Une application qui peint son interface dans une
//    liste peut donc etre photographiee par SON PROPRE CODE DE PEINTURE, sans
//    fenetre -- la regle « une sonde ne surgit pas sous les yeux de celui qui
//    travaille ».
//
// CE QU'IL NE FAIT PAS : aucun contexte 3D (`Init` n'ouvre rien), aucune
// relecture GPU. Une vue 3D rendue hors de la liste n'y apparait pas.
// =============================================================================

#include "NKEditorKit/NkIEditorRenderer.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/NKImage.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editorkit {

		class NkEditorRendererMemoire final : public NkIEditorRenderer {
			public:
				explicit NkEditorRendererMemoire(uint32 w = 1600u, uint32 h = 900u, uint32 fond = 0x000000FFu)
					: mW(w), mH(h), mFond(fond) {}

				// ── Pas de fenetre, pas de GPU ──────────────────────────────────
				bool Init(NkWindow &window, NkEditorGfxApi api) override {
					(void)window;
					(void)api;
					return true;
				}
				void Shutdown() override {
					mTex.Clear();
				}
				bool IsValid() const override {
					return true;
				}
				math::NkVec2u Size() const override {
					return math::NkVec2u(mW, mH);
				}
				void OnResize(uint32 width, uint32 height) override {
					mW = width;
					mH = height;
				}

				// ── La frame : effacer, rasteriser chaque liste, ecrire si arme ─
				void BeginFrame() override {
					mPret = mRas.Init((int32)mW, (int32)mH);
					if (mPret)
						mRas.Effacer(mFond);
					mInconnues = 0u;
				}
				void SubmitDrawList(const nkgui::NkGuiDrawList &dl, uint32 fbW, uint32 fbH) override {
					(void)fbW;
					(void)fbH;
					if (!mPret)
						return;
					// Les textures sont DECLAREES juste avant : le rasteriseur garde des
					// pointeurs, et un televersement entre deux listes peut avoir
					// deplace le stockage.
					for (usize i = 0; i < mTex.Size(); ++i) {
						const Texture &t = mTex[i];
						mRas.PoserTexture(t.id, t.px.Data(), t.w, t.h, t.canaux);
					}
					mInconnues += mRas.Rasteriser(dl);
				}
				void EndFrame() override {
					if (!mArme.Empty()) {
						mDernierOk = EcrirePNG(mArme.CStr());
						mArme.Clear();
					}
				}

				// ── Les textures, GARDEES ───────────────────────────────────────
				bool UploadFontGray8(uint32 texId, const uint8 *pixels, int32 w, int32 h) override {
					return Garder(texId, pixels, w, h, 1);
				}
				bool UploadImageRGBA(uint32 texId, const uint8 *pixels, int32 w, int32 h) override {
					return Garder(texId, pixels, w, h, 4);
				}

				/// Arme l'ecriture : le PNG part au prochain `EndFrame`, comme le
				/// contrat de l'interface le demande.
				bool CaptureNext(const char *path) noexcept override {
					if (!path || !*path)
						return false;
					mArme = NkString(path);
					return true;
				}

				// ── Ce que la sonde lit ─────────────────────────────────────────
				/// Ecrit l'image courante (opaque) en PNG. Faux si rien n'a ete rendu.
				bool EcrirePNG(const char *chemin) const {
					if (!mPret || !chemin || !*chemin)
						return false;
					NkImage img;
					if (!img.Create(mW, mH, math::NkColor(), 4) || !img.Pixels())
						return false;
					const uint8 *src = mRas.Pixels();
					uint8 *dst = img.Pixels();
					const usize n = (usize)mW * (usize)mH;
					for (usize k = 0; k < n; ++k) {
						dst[k * 4u + 0u] = src[k * 4u + 0u];
						dst[k * 4u + 1u] = src[k * 4u + 1u];
						dst[k * 4u + 2u] = src[k * 4u + 2u];
						dst[k * 4u + 3u] = 255u; // une capture d'ecran est opaque
					}
					return img.SavePNG(chemin);
				}
				/// Les commandes qui citaient une texture jamais televersee : ce que
				/// l'image NE montre PAS. Zero = tout ce qui a ete peint est la.
				uint32 TexturesInconnues() const noexcept {
					return mInconnues;
				}
				uint32 TexturesGardees() const noexcept {
					return (uint32)mTex.Size();
				}
				bool DerniereEcritureOk() const noexcept {
					return mDernierOk;
				}
				const nkgui::NkGuiDrawListRaster &Raster() const noexcept {
					return mRas;
				}

			private:
				struct Texture {
						uint32 id = 0;
						NkVector<uint8> px;
						int32 w = 0, h = 0, canaux = 4;
				};

				bool Garder(uint32 texId, const uint8 *pixels, int32 w, int32 h, int32 canaux) {
					if (!pixels || w <= 0 || h <= 0 || texId == 0u)
						return false;
					Texture *t = nullptr;
					for (usize i = 0; i < mTex.Size(); ++i)
						if (mTex[i].id == texId) {
							t = &mTex[i];
							break;
						}
					if (!t) {
						mTex.PushBack(Texture{});
						t = &mTex[mTex.Size() - 1u];
					}
					t->id = texId;
					t->w = w;
					t->h = h;
					t->canaux = canaux;
					const usize n = (usize)w * (usize)h * (usize)canaux;
					t->px.Resize(n);
					for (usize k = 0; k < n; ++k)
						t->px[k] = pixels[k];
					return true;
				}

				uint32 mW, mH, mFond;
				nkgui::NkGuiDrawListRaster mRas;
				NkVector<Texture> mTex;
				NkString mArme;
				bool mPret = false;
				bool mDernierOk = false;
				uint32 mInconnues = 0u;
		};

	} // namespace editorkit
} // namespace nkentseu
