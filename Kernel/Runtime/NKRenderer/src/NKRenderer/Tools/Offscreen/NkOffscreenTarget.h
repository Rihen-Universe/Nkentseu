#pragma once
// NkOffscreenTarget.h — NKRenderer v4.0 (Tools/Offscreen/)
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#include "../../Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
	namespace renderer {
		class NkTextureLibrary;

		// ── L'ORIENTATION DU CONTENU D'UNE CIBLE HORS ECRAN ──────────────────
		//
		// Une cible hors ecran a DEUX consommateurs qui n'ont rien en commun :
		// l'un la RELIT vers le CPU (ReadbackPixels, Capture, NkFrameCapture),
		// l'autre l'ECHANTILLONNE comme une texture pour l'afficher. La regle
		// ci-dessous etait ecrite DEUX FOIS, en dur, et seul le premier la
		// consommait -- le second affichait la cible telle quelle. Sur OpenGL,
		// les deux ne montrent donc pas la meme image.
		//
		// LA REGLE. OpenGL a son origine de framebuffer en BAS a gauche : la
		// premiere rangee stockee est la rangee du BAS. Les autres dorsaux
		// l'ont en haut. Qui veut une image haut-bas doit donc inverser les
		// rangees d'une cible OpenGL, et d'elle seule.
		//
		// ⚠️ ET « D'ELLE SEULE » EST MESURE, PAS SUPPOSE. On a cru la regle
		// incomplete et voulu l'etendre a DirectX. Mesure du 08/09, dans DEUX
		// applications independantes, en comparant une cible ECHANTILLONNEE a
		// la meme cible RELUE (banc d'ombre NK_BANC_HORSECRAN, et le viseur du
		// modeleur contre sa region d'ecran) :
		//
		//     banc      opengl  droit 16.329  retourne  0.549  -> RETOURNE
		//     banc      dx11    droit  0.549  retourne 16.329  -> fidele
		//     banc      dx12    droit  0.549  retourne 16.329  -> fidele
		//     modeleur  opengl  droit 99.171  retourne 18.130  -> RETOURNE
		//     modeleur  vulkan  droit 13.449  retourne 100.189 -> fidele
		//     modeleur  dx11    droit 13.826  retourne 83.527  -> fidele
		//
		// DirectX et Vulkan echantillonnent FIDELEMENT dans les deux. Etendre
		// la regle a DirectX aurait donc casse ce qui marche : la regle est
		// complete en ETENDUE, elle etait incomplete en CONSOMMATION.
		//
		// ⚠️ ET ELLE NE PEUT PAS SE POSER A LA CREATION DE LA CIBLE. Le contenu
		// STOCKE, deduit des relectures, ne suit pas le dorsal :
		//     banc      GL = miroir(VK) = miroir(DX)
		//     modeleur  GL = DX = miroir(VK)
		// GL et VK sont stables d'une application a l'autre, DX bascule -- parce
		// que la negation Y de Vulkan est ecrite A LA MAIN dans 21 des 25
		// sources .nksl (`@target VK { gl_Position.y = -gl_Position.y; }`), donc
		// l'orientation depend DU NUANCEUR QUI DESSINE. Une regle attachee a
		// l'API serait vraie pour une application et fausse pour l'autre.
		// Celle-ci ne parle QUE de l'origine du framebuffer, qui, elle, ne
		// depend pas des nuanceurs.
		inline bool NkOffscreenStoredIsBottomUp(NkGraphicsApi api) {
			return api == ::nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL;
		}

		struct NkOffscreenDesc {
				uint32 width = 1024, height = 1024;
				bool hasDepth = true, hasStencil = false;
				NkGPUFormat colorFmt = NkGPUFormat::NK_RGBA8_SRGB;
				NkGPUFormat depthFmt = NkGPUFormat::NK_D32_FLOAT;
				bool hdr = false, readable = true, readback = false;
				uint32 layers = 1;
				NkString name;
		};

		class NkOffscreenTarget {
			public:
				bool Init(NkIDevice *d, NkTextureLibrary *t, const NkOffscreenDesc &desc);
				void Shutdown();

				bool IsValid() const {
					return mValid;
				}

				void BeginCapture(NkICommandBuffer *cmd, bool clearColor = true, NkVec4f cc = {0, 0, 0, 1},
								  bool clearDepth = true);
				void EndCapture(NkICommandBuffer *cmd);
				bool ReadbackPixels(uint8 *dst, uint32 rowPitch = 0);
				// Capture le rendu offscreen vers un fichier image (format selon
				// l'extension via NkImage). Exige readback=true + colorFmt LDR RGBA8.
				// Pendant NKRHI de NkRenderTarget::Capture (cf. système de capture).
				bool Capture(const char *path);
				bool Resize(uint32 w, uint32 h);

				NkTexHandle GetColorResult() const {
					return mColor;
				}

				NkTexHandle GetDepthResult() const {
					return mDepth;
				}

				NkRenderPassHandle GetRP() const {
					return mRP;
				}

				NkFramebufferHandle GetFBO() const {
					return mFBO;
				}

				uint32 GetWidth() const {
					return mDesc.width;
				}

				uint32 GetHeight() const {
					return mDesc.height;
				}

				const NkOffscreenDesc &GetDesc() const {
					return mDesc;
				}

			private:
				NkIDevice *mDevice = nullptr;
				NkTextureLibrary *mTexLib = nullptr;
				NkOffscreenDesc mDesc;
				NkTexHandle mColor, mDepth;
				NkFramebufferHandle mFBO;
				NkRenderPassHandle mRP;
				NkBufferHandle mReadBuf;
				bool mValid = false, mCapturing = false;
		};
	} // namespace renderer
} // namespace nkentseu
