#pragma once

// =============================================================================
// NkSurface.h
// Descripteur de surface graphique natif par plateforme.
//
// NkSurfaceDesc contient tous les handles natifs nÃƒÂ©cessaires ÃƒÂ  un backend
// graphique (Vulkan, Metal, DirectX, OpenGL, Software) pour crÃƒÂ©er ses
// propres ressources de rendu.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKPlatform/NkPlatformDetect.h"

#include "NKWindow/Core/NkSurface.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	// ---------------------------------------------------------------------------
	// NkRendererConfig - configuration de crÃƒÂ©ation du renderer
	// ---------------------------------------------------------------------------

	struct NkRendererConfig {
		/// @brief Selected backend API.
		NkRendererApi api = NkRendererApi::NK_SOFTWARE;
		/// @brief Color buffer pixel format.
		NkPixelFormat colorFormat = NkPixelFormat::NK_PIXEL_R8G8B8A8_UNORM;
		/// @brief Depth/stencil format.
		NkPixelFormat depthFormat = NkPixelFormat::NK_PIXEL_D24_UNORM_S8_UINT;
		uint32 sampleCount = 1; ///< MSAA (1 = dÃƒÂ©sactivÃƒÂ©)
		/// @brief Enable vertical synchronization.
		bool vsync = true;
		/// @brief Enable validation/debug layers when available.
		bool debug = false; ///< Couche de validation

		/// When true, BeginFrame() automatically resizes the framebuffer if the
		/// window dimensions have changed since the last frame Ã¢â‚¬â€ the application
		/// does not need to handle NkWindowResizeEvent manually.
		/// When false, the application calls NkRenderer::Resize() itself.
		bool autoResizeFramebuffer = true;
	};

	// ---------------------------------------------------------------------------
	// NkFramebufferInfo - infos du framebuffer
	// ---------------------------------------------------------------------------

	struct NkFramebufferInfo {
		uint32 width = 0;
		uint32 height = 0;
		uint32 pitch = 0;		///< Octets par ligne (width * 4 pour RGBA8)
		uint8 *pixels = nullptr; ///< Pointeur vers pixels (Software uniquement)
	};

	// ---------------------------------------------------------------------------
	// NkRendererContext - contexte runtime exposÃƒÂ© par backend
	// ---------------------------------------------------------------------------

	struct NkSoftwareRendererContext {
		/// @brief Software framebuffer exposed by software backend.
		NkFramebufferInfo framebuffer{};
	};

	/**
	 * @brief Struct NkOpenGLRendererContext.
	 */
	struct NkOpenGLRendererContext {
		void *nativeDisplay = nullptr; // Display*/HDC/xcb_connection/...
		void *nativeWindow = nullptr;  // HWND/X11 Window/NSView/...
		void *context = nullptr;	   // GL context owned by backend
	};

	/**
	 * @brief Struct NkVulkanRendererContext.
	 */
	struct NkVulkanRendererContext {
		void *instance = nullptr;		// VkInstance
		void *physicalDevice = nullptr; // VkPhysicalDevice
		void *device = nullptr;			// VkDevice
		void *queue = nullptr;			// VkQueue
		void *surface = nullptr;		// VkSurfaceKHR (or bootstrap native surface)
		void *nativeDisplay = nullptr;
		void *nativeWindow = nullptr;
	};

	/**
	 * @brief Struct NkDirectX11RendererContext.
	 */
	struct NkDirectX11RendererContext {
		void *device = nullptr;			  // ID3D11Device*
		void *deviceContext = nullptr;	  // ID3D11DeviceContext*
		void *swapChain = nullptr;		  // IDXGISwapChain*
		void *renderTargetView = nullptr; // ID3D11RenderTargetView*
		void *nativeWindow = nullptr;	  // HWND
	};

	/**
	 * @brief Struct NkDirectX12RendererContext.
	 */
	struct NkDirectX12RendererContext {
		void *device = nullptr;		  // ID3D12Device*
		void *commandQueue = nullptr; // ID3D12CommandQueue*
		void *swapChain = nullptr;	  // IDXGISwapChain*
		void *nativeWindow = nullptr; // HWND
	};

	/**
	 * @brief Struct NkMetalRendererContext.
	 */
	struct NkMetalRendererContext {
		void *device = nullptr;		  // id<MTLDevice>
		void *commandQueue = nullptr; // id<MTLCommandQueue>
		void *layer = nullptr;		  // CAMetalLayer*
		void *drawable = nullptr;	  // id<CAMetalDrawable>
		void *view = nullptr;		  // NSView*/UIView*
	};

	/**
	 * @brief Struct NkRendererContext.
	 */
	struct NkRendererContext {
		/// @brief Active backend API.
		NkRendererApi api = NkRendererApi::NK_NONE;
		/// @brief Native surface descriptor used by backend.
		NkSurfaceDesc surface{};
		NkSoftwareRendererContext software{};
		NkOpenGLRendererContext opengl{};
		NkVulkanRendererContext vulkan{};
		NkDirectX11RendererContext dx11{};
		NkDirectX12RendererContext dx12{};
		NkMetalRendererContext metal{};
		/// @brief Optional user-owned backend data pointer.
		void *userData = nullptr;
	};

	/// @brief Return platform-native window handle as opaque pointer.
	inline void *NkGetNativeWindowHandle(const NkSurfaceDesc &surface) {
        #if defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
            return surface.nativeWindow;
        #elif defined(NKENTSEU_PLATFORM_WINDOWS)
            return reinterpret_cast<void *>(surface.hwnd);
        #elif defined(NKENTSEU_PLATFORM_MACOS)
            return static_cast<void *>(surface.view);
        #elif defined(NKENTSEU_PLATFORM_IOS)
            return static_cast<void *>(surface.view);
        #elif defined(NKENTSEU_WINDOWING_XCB)
            return reinterpret_cast<void *>(static_cast<std::uintptr_t>(surface.window));
        #elif defined(NKENTSEU_WINDOWING_XLIB)
            return reinterpret_cast<void *>(static_cast<std::uintptr_t>(surface.window));
        #elif defined(NKENTSEU_WINDOWING_WAYLAND)
            return static_cast<void *>(surface.surface);
        #elif defined(NKENTSEU_PLATFORM_ANDROID)
            return static_cast<void *>(surface.nativeWindow);
        #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            return const_cast<char *>(surface.canvasId ? surface.canvasId : "#canvas");
        #else
            return nullptr;
        #endif
	}

	/// @brief Return platform-native display/instance handle as opaque pointer.
	inline void *NkGetNativeDisplayHandle(const NkSurfaceDesc &surface) {
        #if defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
            return surface.nativeWindow;
        #elif defined(NKENTSEU_PLATFORM_WINDOWS)
            return reinterpret_cast<void *>(surface.hinstance);
        #elif defined(NKENTSEU_PLATFORM_MACOS)
            return static_cast<void *>(surface.view);
        #elif defined(NKENTSEU_PLATFORM_IOS)
            return static_cast<void *>(surface.view);
        #elif defined(NKENTSEU_WINDOWING_XCB)
            return static_cast<void *>(surface.connection);
        #elif defined(NKENTSEU_WINDOWING_XLIB)
            return static_cast<void *>(surface.display);
        #elif defined(NKENTSEU_WINDOWING_WAYLAND)
            return static_cast<void *>(surface.display);
        #elif defined(NKENTSEU_PLATFORM_ANDROID)
            return static_cast<void *>(surface.nativeWindow);
        #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            return const_cast<char *>(surface.canvasId ? surface.canvasId : "#canvas");
        #else
            return nullptr;
        #endif
	}

	/// @brief Build a portable runtime context for renderer backends.
	inline NkRendererContext NkMakeRendererContext(NkRendererApi api, const NkSurfaceDesc &surface,
												const NkFramebufferInfo &framebuffer = {}) {
		NkRendererContext context{};
		context.api = api;
		context.surface = surface;
		context.software.framebuffer = framebuffer;

		const void *nativeWindowConst = NkGetNativeWindowHandle(surface);
		const void *nativeDisplayConst = NkGetNativeDisplayHandle(surface);
		void *nativeWindow = const_cast<void *>(nativeWindowConst);
		void *nativeDisplay = const_cast<void *>(nativeDisplayConst);

		switch (api) {
			case NkRendererApi::NK_SOFTWARE:
				context.software.framebuffer = framebuffer;
				break;
			case NkRendererApi::NK_OPENGL:
				context.opengl.nativeDisplay = nativeDisplay;
				context.opengl.nativeWindow = nativeWindow;
				break;
			case NkRendererApi::NK_VULKAN:
				context.vulkan.nativeDisplay = nativeDisplay;
				context.vulkan.nativeWindow = nativeWindow;
				// Surface bootstrap handle. Real VkSurfaceKHR can replace this later.
				context.vulkan.surface = nativeWindow;
				break;
			case NkRendererApi::NK_DIRECTX11:
				context.dx11.nativeWindow = nativeWindow;
				break;
			case NkRendererApi::NK_DIRECTX12:
				context.dx12.nativeWindow = nativeWindow;
				break;
			case NkRendererApi::NK_METAL:
				context.metal.view = nativeWindow;
				break;
			case NkRendererApi::NK_NONE:
			case NkRendererApi::NK_RENDERER_API_MAX:
			default:
				break;
		}

        #if defined(NKENTSEU_PLATFORM_MACOS)
            context.metal.view = static_cast<void *>(surface.view);
            context.metal.layer = static_cast<void *>(surface.metalLayer);
        #elif defined(NKENTSEU_PLATFORM_IOS)
            context.metal.view = static_cast<void *>(surface.view);
            context.metal.layer = static_cast<void *>(surface.metalLayer);
        #endif

		return context;
	}

} // namespace nkentseu
