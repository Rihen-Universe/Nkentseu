// =============================================================================
// NkFrameCapture.cpp — capture de frames asynchrone (ring staging + fences).
// Voir NkFrameCapture.h pour l'architecture. Zéro WaitIdle, zéro stall.
// =============================================================================
#include "NkFrameCapture.h"

#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKLogger/NkLog.h"

#include <cstring>

namespace nkentseu {
	namespace renderer {

		NkFrameCapture::~NkFrameCapture() {
			Shutdown();
		}

		bool NkFrameCapture::Init(NkIDevice *device, const NkFrameCaptureDesc &desc) {
			if (device == nullptr || desc.width == 0 || desc.height == 0)
				return false;
			Shutdown();
			mDevice = device;
			mDesc = desc;
			if (mDesc.ringSize < 1)
				mDesc.ringSize = 1;

			const uint64 bytes = (uint64)mDesc.width * mDesc.height * 4u;
			mSlots.Resize(mDesc.ringSize);
			for (uint32 i = 0; i < mDesc.ringSize; ++i) {
				NkBufferDesc bd;
				bd.sizeBytes = bytes;
				bd.type = NkBufferType::NK_STAGING;
				bd.usage = NkResourceUsage::NK_READBACK;
				mSlots[i].staging = mDevice->CreateBuffer(bd);
				mSlots[i].fence = mDevice->CreateFence(false);
				if (!mSlots[i].staging.IsValid() || !mSlots[i].fence.IsValid()) {
					Shutdown();
					return false;
				}
			}
			mScratch.Resize((usize)bytes);
			mValid = true;
			return true;
		}

		void NkFrameCapture::Shutdown() {
			if (mDevice == nullptr)
				return;
			// Les slots encore en vol doivent finir avant de libérer leurs
			// ressources — seul endroit où on accepte d'attendre.
			bool anyInFlight = false;
			for (usize i = 0; i < mSlots.Size(); ++i)
				anyInFlight = anyInFlight || mSlots[i].inFlight;
			if (anyInFlight)
				mDevice->WaitIdle();
			for (usize i = 0; i < mSlots.Size(); ++i) {
				Slot &s = mSlots[i];
				if (s.cmd != nullptr)
					mDevice->DestroyCommandBuffer(s.cmd);
				if (s.staging.IsValid())
					mDevice->DestroyBuffer(s.staging);
				if (s.fence.IsValid())
					mDevice->DestroyFence(s.fence);
			}
			mSlots.Clear();
			mScratch.Clear();
			mDevice = nullptr;
			mValid = false;
		}

		bool NkFrameCapture::EnqueueCopy(NkTextureHandle src, uint64 tag) {
			if (!mValid || !src.IsValid())
				return false;

			// Slot libre ? (ring plein = frame sautée, pas d'attente)
			Slot *slot = nullptr;
			for (usize i = 0; i < mSlots.Size(); ++i) {
				if (!mSlots[i].inFlight) {
					slot = &mSlots[i];
					break;
				}
			}
			if (slot == nullptr)
				return false;

			NkICommandBuffer *cmd = mDevice->CreateCommandBuffer();
			if (cmd == nullptr || !cmd->Begin()) {
				if (cmd != nullptr)
					mDevice->DestroyCommandBuffer(cmd);
				return false;
			}
			cmd->TextureBarrier(src, NkResourceState::NK_SHADER_READ, NkResourceState::NK_TRANSFER_SRC);
			NkBufferTextureCopyRegion region{};
			region.width = mDesc.width;
			region.height = mDesc.height;
			region.depth = 1;
			region.bufferRowPitch = 0; // jointif (width*4)
			cmd->CopyTextureToBuffer(src, slot->staging, region);
			cmd->TextureBarrier(src, NkResourceState::NK_TRANSFER_SRC, NkResourceState::NK_SHADER_READ);
			cmd->End();

			mDevice->ResetFence(slot->fence);
			mDevice->Submit(&cmd, 1, slot->fence);

			slot->cmd = cmd; // détruit au Poll, après fence signalée
			slot->tag = tag;
			slot->sequence = mNextSequence++;
			slot->inFlight = true;
			return true;
		}

		bool NkFrameCapture::Poll(const FrameReadyCb &cb) {
			if (!mValid || !cb)
				return false;

			// Plus ancienne capture en vol (livraison FIFO).
			Slot *oldest = nullptr;
			for (usize i = 0; i < mSlots.Size(); ++i) {
				Slot &s = mSlots[i];
				if (s.inFlight && (oldest == nullptr || s.sequence < oldest->sequence))
					oldest = &s;
			}
			if (oldest == nullptr || !mDevice->IsFenceSignaled(oldest->fence))
				return false;

			// Copie staging → scratch top-down (flip Y sur OpenGL).
			const bool flipY = mDevice->GetApi() == ::nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL;
			const uint32 rowBytes = mDesc.width * 4u;
			NkMappedMemory mapped = mDevice->MapBuffer(oldest->staging);
			if (mapped.IsValid()) {
				for (uint32 row = 0; row < mDesc.height; ++row) {
					const uint32 srcRow = flipY ? (mDesc.height - 1 - row) : row;
					std::memcpy(mScratch.Data() + (usize)row * rowBytes,
								(const uint8 *)mapped.ptr + (usize)srcRow * rowBytes, rowBytes);
				}
				mDevice->UnmapBuffer(oldest->staging);
				cb(mScratch.Data(), mDesc.width, mDesc.height, oldest->tag);
			}

			// Recycle le slot.
			if (oldest->cmd != nullptr)
				mDevice->DestroyCommandBuffer(oldest->cmd);
			oldest->cmd = nullptr;
			oldest->inFlight = false;
			return mapped.IsValid();
		}

		uint32 NkFrameCapture::PendingCount() const {
			uint32 n = 0;
			for (usize i = 0; i < mSlots.Size(); ++i) {
				if (mSlots[i].inFlight)
					++n;
			}
			return n;
		}

	} // namespace renderer
} // namespace nkentseu
