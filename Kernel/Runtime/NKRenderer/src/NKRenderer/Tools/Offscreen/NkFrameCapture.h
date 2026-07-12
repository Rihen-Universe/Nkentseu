#pragma once
// =============================================================================
// NkFrameCapture.h — NKRenderer (Tools/Offscreen/)
//
// Capture de frames ASYNCHRONE (zéro stall GPU) — brique du pipeline
// enregistrement vidéo / cinématiques / tutoriels demandé par Rihen :
//
//   rendu (NkRenderer, SetFinalColorTarget) ─┐
//                                            ▼
//   NkFrameCapture::EnqueueCopy(tex)   copie GPU→staging + fence (par frame)
//                                            ▼        (ring, non bloquant)
//   NkFrameCapture::Poll(cb)           fence signalée → pixels RGBA8 top-down
//                                            ▼   (appelable depuis un thread
//   consommateur : NkVideoRecorder (NKMedia), PNG, tutoriel, réseau…)
//
// Principe : un RING de buffers de staging (readback). EnqueueCopy soumet la
// copie texture→staging avec une fence et rend la main IMMÉDIATEMENT ; si le
// ring est plein, la frame est SAUTÉE (jamais de stall du rendu). Poll est
// non bloquant : il livre la plus ancienne frame dont la fence est signalée.
// La cadence vidéo se règle en appelant EnqueueCopy 1 frame sur N.
//
// Threading : EnqueueCopy s'appelle depuis le thread de rendu (après le
// Present de la frame dont on veut le contenu). Poll peut s'appeler depuis
// le thread de rendu OU un thread consommateur dédié — mais pas les deux
// en même temps (protéger à l'appelant le cas multi-thread).
//
// La copie livrée est TOP-DOWN (flip Y automatique sur OpenGL) et RGBA8
// jointive — directement consommable par NKMedia/NKImage.
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {
	namespace renderer {

		struct NkFrameCaptureDesc {
				uint32 width = 0;
				uint32 height = 0;
				// Nombre de captures simultanément en vol. 3 couvre les
				// frames-in-flight usuels ; augmenter si le consommateur est
				// lent et qu'on préfère de la latence à des frames sautées.
				uint32 ringSize = 3;
		};

		class NkFrameCapture {
			public:
				NkFrameCapture() = default;
				~NkFrameCapture();

				NkFrameCapture(const NkFrameCapture &) = delete;
				NkFrameCapture &operator=(const NkFrameCapture &) = delete;

				bool Init(NkIDevice *device, const NkFrameCaptureDesc &desc);
				void Shutdown();

				bool IsValid() const {
					return mValid;
				}

				// Enfile la copie GPU de `src` (texture RGBA8 width×height, état
				// SHADER_READ) vers un slot libre du ring, signalée par fence.
				// Retourne false si le ring est plein (frame sautée) ou sur
				// erreur — jamais d'attente. `tag` = valeur libre rendue au Poll
				// (ex : numéro de frame, timestamp).
				bool EnqueueCopy(NkTextureHandle src, uint64 tag = 0);

				// Callback de livraison : pixels RGBA8 top-down jointifs, valides
				// UNIQUEMENT pendant l'appel (copiés/encodés par le consommateur).
				using FrameReadyCb = NkFunction<void(const uint8 *rgba, uint32 width, uint32 height, uint64 tag)>;

				// Non bloquant : livre LA plus ancienne capture dont la fence est
				// signalée (une par appel). Retourne true si une frame a été
				// livrée. Appeler en boucle pour drainer.
				bool Poll(const FrameReadyCb &cb);

				// Nombre de captures en vol (soumises, pas encore livrées).
				uint32 PendingCount() const;

			private:
				struct Slot {
						NkBufferHandle staging;
						NkFenceHandle fence;
						NkICommandBuffer *cmd = nullptr; // détruit après fence signalée
						uint64 tag = 0;
						uint64 sequence = 0; // ordre de soumission (livraison FIFO)
						bool inFlight = false;
				};

				NkIDevice *mDevice = nullptr;
				NkFrameCaptureDesc mDesc;
				NkVector<Slot> mSlots;
				NkVector<uint8> mScratch; // frame top-down livrée au callback
				uint64 mNextSequence = 1;
				bool mValid = false;
		};

	} // namespace renderer
} // namespace nkentseu
