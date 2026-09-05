#pragma once
// =============================================================================
// NkSoftwareRenderer2D.h — CPU software renderer backend (SIMD optimisé)
// =============================================================================
#include "NKCanvas/Renderer/Batch/NkBatchRenderer2D.h"
#include "NkSoftwareContext.h"

namespace nkentseu {
	namespace renderer {

		class NkSoftwareRenderer2D final : public NkBatchRenderer2D {
			public:
				NkSoftwareRenderer2D() = default;

				~NkSoftwareRenderer2D() override {
					if (IsValid())
						Shutdown();
				}

				bool Initialize(NkIGraphicsContext *ctx) override;
				void Shutdown() override;

				bool IsValid() const override {
					return mIsValid;
				}

				void Clear(const NkColor2D &col) override;

			protected:
				void BeginBackend() override;
				void EndBackend() override;

				void SubmitBatches(const NkBatchGroup *groups, uint32 groupCount, const NkVertex2D *verts,
								   uint32 vCount, const uint32 *idx, uint32 iCount) override;

				// ── Projection ────────────────────────────────────────────────────────
				// Cette methode etait VIDE, et le rasteriseur prend les coordonnees des
				// sommets pour des pixels. Consequence : le backend logiciel ignorait
				// entierement la camera. SetView, le zoom, le defilement, la rotation de
				// vue et le viewport ne faisaient RIEN, sans le moindre avertissement.
				// Sur le seul backend disponible partout, tout un pan de l'API etait
				// donc mort en silence. Constate le 2026-09-05.
				void UploadProjection(const float32 proj[16]) override {
					for (int32 i = 0; i < 16; ++i)
						mProj[i] = proj[i];
					mHasProj = true;
				}

				void Draw(const NkSprite &sprite) override;

				// ── Dispatch NkRenderTexture (offscreen = framebuffer CPU dedie) ──────
				// Bind redirige le rasterizer vers le framebuffer offscreen ; le sampling
				// de cette RT lit directement ses pixels CPU (registry par colorId).
				static uint32 CreateSWRenderTexture(uint32 w, uint32 h);
				static void DestroySWRenderTexture(uint32 handle);
				static void BindSWRenderTexture(uint32 handle);
				static void UnbindSWRenderTexture();
				static uint32 GetSWRenderTextureColorId(uint32 handle);

			private:
				NkIGraphicsContext *mCtx = nullptr;
				NkSoftwareContext *mSWCtx = nullptr;
				bool mIsValid = false;

				/// Projection courante, colonne-majeure, telle que NkView2D la produit.
				/// mHasProj reste faux tant qu'aucune vue n'a ete posee : on se comporte
				/// alors comme avant, sommets = pixels, ce qui garantit qu'aucun rendu
				/// existant ne change tant que personne n'appelle SetView.
				float32 mProj[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
									 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
				bool mHasProj = false;

				/// Sommets transformes de la trame courante. Membre plutot que local :
				/// une allocation par trame sur le chemin le plus chaud du moteur
				/// couterait plus cher que le calcul lui-meme.
				NkVector<NkVertex2D> mProjetes;

				/// Vrai quand la projection et le viewport se ramenent a l'identite
				/// pixel : le chemin rapide des sprites est alors exact.
				bool EnEspaceEcran(const NkSoftwareFramebuffer &fb) const noexcept;

				void BlitTexture(NkSoftwareFramebuffer &fb, const NkTexture *tex, const NkRect2i &srcRect, int32 dstX,
								 int32 dstY, int32 dstW, int32 dstH, const NkColor2D &tint);

				// Scan-line rasterizer SIMD + BGRA-direct
				void RasterizeTriangle(NkSoftwareFramebuffer &fb, const NkVertex2D &v0, const NkVertex2D &v1,
									   const NkVertex2D &v2, const NkTexture *tex,
									   NkBlendMode blendMode = NkBlendMode::NK_ALPHA);
		};

	} // namespace renderer
} // namespace nkentseu