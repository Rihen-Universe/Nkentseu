#pragma once
// =============================================================================
// Tutoriels3D / Commun — CAPTURE HORS ECRAN D'UNE IMAGE (2026-09-30)
//
// POURQUOI : la CI macOS n'a pas d'ecran que l'on puisse photographier (pas
// d'autorisation d'enregistrement de l'ecran), et « la fenetre est ouverte » ne
// dit pas « l'image est juste ». Ce petit outil fait rendre UNE image dans une
// cible hors ecran, la relit et l'ecrit en PNG, puis demande la sortie propre.
// C'est la meme mecanique que NK_CAPTURE du Sandbox (SetFinalColorTarget +
// NkOffscreenTarget), en une classe.
//
// VARIABLES D'ENVIRONNEMENT (sans elles, l'outil ne fait RIEN) :
//   NK_CAPTURE=<n>          capturer l'image n (compte a partir de 1)
//   NK_CAPTURE_PATH=<png>   fichier ecrit (defaut : nk_capture.png)
//   NK_GFX_BACKEND=metal    (lu par NKRHI) choisir l'API a essayer d'abord
//
// En mode capture, les tutoriels figent leur horloge d'animation (temps = 0) :
// deux captures, en Metal et en logiciel, montrent la MEME scene.
//
// USAGE, dans la boucle :
//   NkTutoCapture capture;
//   while (...) {
//       if (capture.Etape(renderer, device, W, H)) break;  // fini : sortir
//       ... BeginFrame / rendu / Present / EndFrame ...
//       capture.ImageRendue();
//   }
//   return capture.CodeDeSortie(codeNormal);
// =============================================================================
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKLogger/NkLog.h"
#include <cstdlib>

namespace nkentseu {
	namespace tuto {

		class NkTutoCapture {
			public:
				NkTutoCapture() {
					const char *f = getenv("NK_CAPTURE");
					mImageVisee = (f && *f) ? (uint64)atoll(f) : 0;
					const char *p = getenv("NK_CAPTURE_PATH");
					mChemin = (p && *p) ? p : "nk_capture.png";
				}

				bool Actif() const {
					return mImageVisee > 0;
				}

				// Avant BeginFrame. Rend true quand la capture est ECRITE (ou a
				// echoue) : la boucle doit alors se terminer.
				bool Etape(renderer::NkRenderer *r, NkIDevice *d, uint32 w, uint32 h) {
					if (!Actif() || !r || !d)
						return false;
					// Trois images d'avance : le graphe de rendu se reconstruit
					// quand la cible finale change.
					if (!mArmee && mImages + 3 >= mImageVisee) {
						renderer::NkOffscreenDesc od;
						od.width = w;
						od.height = h;
						od.hasDepth = false;
						od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
						od.readback = true;
						od.name = "nk_tuto_capture";
						if (mCible.Init(d, r->GetTextures(), od)) {
							r->SetFinalColorTarget(r->GetTextures()->GetRHIHandle(mCible.GetColorResult()));
							mArmee = true;
						} else {
							logger.Error("[NkTutoCapture] cible hors ecran KO : pas de capture");
							mEchec = true;
							return true;
						}
					} else if (mArmee && mImages >= mImageVisee) {
						d->WaitIdle();
						const bool ok = mCible.Capture(mChemin);
						logger.Info("[NkTutoCapture] image {0} ({1}x{2}, {3}) -> {4} : {5}", mImages, w, h,
									NkGraphicsApiName(d->GetApi()), mChemin, ok ? "OK" : "ECHEC");
						r->SetFinalColorTarget(NkTextureHandle{});
						mCible.Shutdown();
						mArmee = false;
						mEchec = !ok;
						return true;
					}
					return false;
				}

				void ImageRendue() {
					++mImages;
				}

				// Temps d'animation : fige en mode capture.
				float32 Temps(float32 tempsReel) const {
					return Actif() ? 0.f : tempsReel;
				}

				int CodeDeSortie(int codeNormal) const {
					return mEchec ? 5 : codeNormal;
				}

			private:
				uint64 mImageVisee = 0;
				uint64 mImages = 0;
				const char *mChemin = "nk_capture.png";
				bool mArmee = false;
				bool mEchec = false;
				renderer::NkOffscreenTarget mCible;
		};

	} // namespace tuto
} // namespace nkentseu
