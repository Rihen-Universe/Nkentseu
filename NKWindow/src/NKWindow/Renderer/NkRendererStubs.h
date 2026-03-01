#pragma once

// =============================================================================
// NkRendererStubs.h
// Stubs GPU — compilent mais ne font rien.
// À remplacer par les implémentations complètes ultérieurement.
// =============================================================================

#include "../Core/IRendererImpl.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// Macro helper pour les stubs
// ---------------------------------------------------------------------------

#define NK_STUB_RENDERER_IMPL(ClassName, ApiEnum, ApiStr, HwAccel)                                                     \
	class ClassName : public INkRendererImpl {                                                                         \
	public:                                                                                                            \
		ClassName() = default;                                                                                         \
		~ClassName() override = default;                                                                               \
		bool Init(const NkRendererConfig &c, const NkSurfaceDesc &s) override {                                        \
			mConfig = c;                                                                                               \
			mSurface = s;                                                                                              \
			mReady = true;                                                                                             \
			return true;                                                                                               \
		}                                                                                                              \
		void Shutdown() override {                                                                                     \
			mReady = false;                                                                                            \
		}                                                                                                              \
		bool IsValid() const override {                                                                                \
			return mReady;                                                                                             \
		}                                                                                                              \
		NkRendererApi GetApi() const override {                                                                        \
			return NkRendererApi::ApiEnum;                                                                             \
		}                                                                                                              \
		std::string GetApiName() const override {                                                                      \
			return ApiStr;                                                                                             \
		}                                                                                                              \
		bool IsHardwareAccelerated() const override {                                                                  \
			return HwAccel;                                                                                            \
		}                                                                                                              \
		NkError GetLastError() const override {                                                                        \
			return mLastError;                                                                                         \
		}                                                                                                              \
		const NkFramebufferInfo &GetFramebufferInfo() const override {                                                 \
			static NkFramebufferInfo sDummy;                                                                           \
			return sDummy;                                                                                             \
		}                                                                                                              \
		void BeginFrame(NkU32) override {                                                                              \
		}                                                                                                              \
		void EndFrame() override {                                                                                     \
		}                                                                                                              \
		void Present(const NkSurfaceDesc &) override {                                                                 \
		}                                                                                                              \
		void Resize(NkU32, NkU32) override {                                                                           \
		}                                                                                                              \
		void SetPixel(NkI32, NkI32, NkU32) override {                                                                  \
		}                                                                                                              \
		void SetBackgroundColor(NkU32 rgba) override {                                                                 \
			mBgColor = rgba;                                                                                           \
		}                                                                                                              \
		NkU32 GetBackgroundColor() const override {                                                                    \
			return mBgColor;                                                                                           \
		}                                                                                                              \
	};

NK_STUB_RENDERER_IMPL(NkVulkanRendererImpl, NK_VULKAN, "Vulkan", true)
NK_STUB_RENDERER_IMPL(NkOpenGLRendererImpl, NK_OPENGL, "OpenGL", true)
NK_STUB_RENDERER_IMPL(NkDX11RendererImpl, NK_DIRECTX11, "DirectX 11", true)
NK_STUB_RENDERER_IMPL(NkDX12RendererImpl, NK_DIRECTX12, "DirectX 12", true)
NK_STUB_RENDERER_IMPL(NkMetalRendererImpl, NK_METAL, "Metal", true)

#undef NK_STUB_RENDERER_IMPL

} // namespace nkentseu
