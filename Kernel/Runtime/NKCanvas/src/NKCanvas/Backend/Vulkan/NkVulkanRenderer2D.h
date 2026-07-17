#pragma once
// =============================================================================
// NkVulkanRenderer2D.h — Vulkan 2D renderer backend
// Uses push constants for the projection matrix (no UBO needed for 2D).
// One descriptor set layout: binding 0 = combined image sampler.
// One pipeline per blend mode (4 pipelines max).
// Dynamic VBO/IBO via host-visible buffer (re-mapped each frame).
// =============================================================================
#include "NKCanvas/Renderer/Batch/NkBatchRenderer2D.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h" // NkTextureFilter / NkTextureWrap enums
#include "NkVulkanContextData.h"
#include "NKLogger/NkLog.h"

#if NKENTSEU_HAS_VULKAN_HEADERS
#include <vulkan/vulkan.h>
#endif

namespace nkentseu {
	namespace renderer {

#if NKENTSEU_HAS_VULKAN_HEADERS

		class NkVulkanRenderer2D final : public NkBatchRenderer2D {
			public:
				NkVulkanRenderer2D() = default;

				~NkVulkanRenderer2D() override {
					if (IsValid())
						Shutdown();
				}

				bool Initialize(NkIGraphicsContext *ctx) override;
				void Shutdown() override;

				bool IsValid() const override {
					return mIsValid;
				}

				void Clear(const NkColor2D &col) override;

				// Texture ops (called by NkTexture integration)
				static VkSampler GetDefaultSampler() {
					return sSampler;
				}

				// GPU texture ops (called by NkTexture via NkTextureBackend
				// dispatch table — see NkTextureBackend.h).
				// Les callbacks sont statiques : ils s'appuient sur une registry
				// globale (gVkTexRegistry dans le .cpp) capturee a Initialize().
				static uint32 CreateVulkanTexture(uint32 w, uint32 h, const uint8 *rgba);
				static void UpdateVulkanTexture(uint32 id, uint32 x, uint32 y, uint32 w, uint32 h, const uint8 *rgba);
				static void DeleteVulkanTexture(uint32 id);
				static void SetVulkanTextureFilter(uint32 id, NkTextureFilter f);
				static void SetVulkanTextureWrap(uint32 id, NkTextureWrap w);

			protected:
				void BeginBackend() override;
				void EndBackend() override;
				void SubmitBatches(const NkBatchGroup *groups, uint32 groupCount, const NkVertex2D *verts,
								   uint32 vCount, const uint32 *idx, uint32 iCount) override;
				void UploadProjection(const float32 proj[16]) override;

			private:
				// Pipeline creation
				bool CreateDescriptorPool();
				bool CreateDescriptorSetLayout();
				bool CreatePipelineLayout();
				bool CreatePipelines();
				bool CreateBuffers();
				bool CreateWhiteTexture();
				bool CreateSampler();

				VkPipeline GetPipelineForBlend(NkBlendMode mode);

				void DestroyBuffers();

				// Helpers
				bool FindMemoryType(uint32 filter, VkMemoryPropertyFlags props, uint32 &out);
				bool CreateBuffer_Internal(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
										   VkBuffer &buf, VkDeviceMemory &mem);

				NkIGraphicsContext *mCtx = nullptr;
				NkVulkanContextData *mVkData = nullptr;
				bool mIsValid = false;

				// Descriptor resources
				VkDescriptorPool mDescPool = VK_NULL_HANDLE;
				VkDescriptorSetLayout mSetLayout = VK_NULL_HANDLE;
				VkPipelineLayout mPipeLayout = VK_NULL_HANDLE;

				// One pipeline per blend mode
				VkPipeline mPipeAlpha = VK_NULL_HANDLE;
				VkPipeline mPipeAdd = VK_NULL_HANDLE;
				VkPipeline mPipeMul = VK_NULL_HANDLE;
				VkPipeline mPipeNone = VK_NULL_HANDLE;

				// Vertex / index buffers (host-visible, persistent map), utilisés en
				// RING par frame : chaque SubmitBatches écrit à un offset qui AVANCE
				// (mVBHead/mIBHead, reset en BeginBackend). Indispensable : le command
				// buffer Vulkan est ENREGISTRÉ puis exécuté au submit/present, donc
				// plusieurs SubmitBatches écrivant tous à l'offset 0 s'écraseraient
				// mutuellement (tous les draws liraient la dernière copie → fantômes).
				VkBuffer mVB = VK_NULL_HANDLE;
				VkDeviceMemory mVBMem = VK_NULL_HANDLE;
				VkBuffer mIB = VK_NULL_HANDLE;
				VkDeviceMemory mIBMem = VK_NULL_HANDLE;
				void *mVBMap = nullptr;
				void *mIBMap = nullptr;
				VkDeviceSize mVBSize = 0;  // capacité ring VB (octets)
				VkDeviceSize mIBSize = 0;  // capacité ring IB (octets)
				VkDeviceSize mVBHead = 0;  // tête d'écriture VB (octets) — reset/frame
				VkDeviceSize mIBHead = 0;  // tête d'écriture IB (octets) — reset/frame

				// White 1x1 texture
				VkImage mWhiteImage = VK_NULL_HANDLE;
				VkDeviceMemory mWhiteMem = VK_NULL_HANDLE;
				VkImageView mWhiteView = VK_NULL_HANDLE;
				VkDescriptorSet mWhiteSet = VK_NULL_HANDLE;

				static VkSampler sSampler; // shared sampler for all textures

				// Per-texture descriptor set cache. Cle = (pointeur, gpuId) : si le
				// gpuId change (ex. render-texture recreee au resize → nouvelle vue), le
				// cache rate et recree un set frais (evite d'echantillonner une vue morte).
				struct TexDescEntry {
						const NkTexture *texture = nullptr;
						uint32 gpuId = 0;
						VkDescriptorSet set = VK_NULL_HANDLE;
				};

				NkVector<TexDescEntry> mTexDescCache;

				VkDescriptorSet GetOrCreateDescSet(const NkTexture *tex);

				// ── Dispatch NkRenderTexture (offscreen : image color + framebuffer sur
				// le render pass swapchain + barrier vers SHADER_READ pour sampler) ──────
				static uint32 CreateVulkanRenderTexture(uint32 w, uint32 h);
				static void DestroyVulkanRenderTexture(uint32 handle);
				static void BindVulkanRenderTexture(uint32 handle);
				static void UnbindVulkanRenderTexture();
				static uint32 GetVulkanRenderTextureColorId(uint32 handle);

				// Push constants layout (projection = 64 bytes at offset 0)
				float32 mProjection[16] = {};
		};

#else

		// Stub when Vulkan headers are not available
		class NkVulkanRenderer2D final : public NkBatchRenderer2D {
			public:
				bool Initialize(NkIGraphicsContext *) override {
					logger.Errorf("[NkVkRenderer2D] Vulkan headers not available at build time");
					return false;
				}

				void Shutdown() override {
				}

				bool IsValid() const override {
					return false;
				}

				void Clear(const NkColor2D &) override {
				}

			protected:
				void SubmitBatches(const NkBatchGroup *, uint32, const NkVertex2D *, uint32, const uint32 *,
								   uint32) override {
				}

				void UploadProjection(const float32 *) override {
				}
		};

#endif // NKENTSEU_HAS_VULKAN_HEADERS

	} // namespace renderer
} // namespace nkentseu