#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkMetalRenderer2D.h — rendu 2D de NKCanvas sur Metal (macOS / iOS)
//
// POURQUOI (2026-09-30) : NKCanvas avait un contexte Metal (NkMetalContext)
// mais aucun rendu 2D pour lui (« Metal 2D renderer not yet implemented ») :
// --backend=metal ouvrait la fenetre et s'arretait. Sur Mac, OpenGL est
// deprecie ; Metal est l'API native.
//
// Meme architecture que les autres dorsaux : NkBatchRenderer2D accumule les
// sommets, celui-ci ne fait que les envoyer. Modele : DX11 (meme NDC, meme
// viewport et scissor en haut-gauche) pour les etats, Vulkan pour l'anneau de
// tampons (plusieurs images en vol).
//
// TOUT EST EN PIXELS de la surface (drawableSize de la CAMetalLayer, que
// NkCocoaWindow tient a backingScaleFactor) : sur un ecran Retina, une vue de
// 1440x900 points est une surface de 2880x1800 pixels. NkCanvasApp fournit la
// taille en pixels a OnResize, y compris quand la fenetre change d'ecran.
//
// En-tete C++ PUR (void*) : NkRenderer2DFactory.cpp n'est pas de l'Objective-C.
// =============================================================================
#include "NKCanvas/Renderer/Batch/NkBatchRenderer2D.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

namespace nkentseu {
	namespace renderer {

		class NkMetalRenderer2D final : public NkBatchRenderer2D {
			public:
				NkMetalRenderer2D() = default;
				~NkMetalRenderer2D() override;

				bool Initialize(NkIGraphicsContext *ctx) override;
				void Shutdown() override;

				bool IsValid() const override {
					return mIsValid;
				}

				// L'effacement est fait par la passe du contexte (loadAction Clear,
				// couleur de NkRenderWindow::Clear -> SetClearColor), comme Vulkan.
				void Clear(const NkColor2D &col) override;

			protected:
				void BeginBackend() override;
				void EndBackend() override;
				void SubmitBatches(const NkBatchGroup *groups, uint32 groupCount, const NkVertex2D *verts,
								   uint32 vCount, const uint32 *idx, uint32 iCount) override;
				void UploadProjection(const float32 proj[16]) override;
				void ApplyScissor(bool enabled, const NkRect2i &rect) override;

			private:
				// Images en vol : autant de tampons que la CAMetalLayer a de drawables.
				static constexpr uint32 kImagesEnVol = 3;
				static constexpr uint32 kModesDeFusion = 8; // NkBlendMode

				bool CreerPipelines();
				bool AssurerPlace(uint64 octets);

				NkIGraphicsContext *mCtx = nullptr;
				bool mIsValid = false;
				bool mImageOuverte = false;

				void *mPipelines[kModesDeFusion] = {}; // id<MTLRenderPipelineState>
				void *mProfondeurCoupee = nullptr;	   // id<MTLDepthStencilState>
				void *mTextureBlanche = nullptr;	   // id<MTLTexture>
				void *mSamplerBlanc = nullptr;		   // id<MTLSamplerState>
				void *mSemaphore = nullptr;			   // dispatch_semaphore_t

				// Anneau : un tampon (sommets + indices) par image en vol.
				void *mAnneau[kImagesEnVol] = {}; // id<MTLBuffer>
				uint32 mCase = 0;
				uint64 mDecalage = 0;

				float32 mProj[16] = {};
				bool mScissorActif = false;
				NkRect2i mScissor{};
		};

	} // namespace renderer
} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS || NKENTSEU_PLATFORM_IOS
