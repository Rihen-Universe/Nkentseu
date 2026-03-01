#pragma once

// =============================================================================
// NkRenderer.h
// Public NkRenderer facade for INkRendererImpl backends.
// =============================================================================

#include "NkSurface.h"
#include "NkTypes.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

class NkWindow;
class INkRendererImpl;

// ---------------------------------------------------------------------------
// NkRenderTexture - offscreen CPU RGBA8 target
// ---------------------------------------------------------------------------

struct NkRenderTexture {
	/// @brief Target width in pixels.
	NkU32 width = 0;
	/// @brief Target height in pixels.
	NkU32 height = 0;
	/// @brief Bytes per row.
	NkU32 pitch = 0;
	/// @brief RGBA8 pixel storage.
	std::vector<NkU8> pixels;
};

// ---------------------------------------------------------------------------
// NkRenderer
// ---------------------------------------------------------------------------

/**
 * @brief Public rendering facade independent from backend implementation.
 *
 * NkRenderer delegates rendering work to INkRendererImpl and exposes
 * backend-agnostic frame lifecycle and pixel operations.
 */
class NkRenderer {
public:
	using NkRendererFactory = std::function<std::unique_ptr<INkRendererImpl>()>;

	/// @brief Construct an empty renderer. Call Create() before use.
	NkRenderer();
	/// @brief Construct and initialize from window + config.
	NkRenderer(NkWindow &window, const NkRendererConfig &config = {});
	/// @brief Construct with a user-provided backend implementation.
	NkRenderer(NkWindow &window, std::unique_ptr<INkRendererImpl> externalImpl, const NkRendererConfig &config = {});
	~NkRenderer();

	NkRenderer(const NkRenderer &) = delete;
	NkRenderer &operator=(const NkRenderer &) = delete;

	// --- Lifecycle ---

	/// @brief Initialize renderer backend from config.
	bool Create(NkWindow &window, const NkRendererConfig &config = {});
	/// @brief Initialize renderer with external backend implementation.
	bool Create(NkWindow &window, std::unique_ptr<INkRendererImpl> externalImpl, const NkRendererConfig &config = {});
	/// @brief Release renderer resources and backend state.
	void Shutdown();
	/// @brief True when backend is initialized and usable.
	bool IsValid() const;
	/// @brief True when rendering is enabled (api != NONE and backend created).
	bool IsEnabled() const;

	/**
	 * @brief Register a user-defined renderer factory for a given API.
	 */
	static bool RegisterExternalRendererFactory(NkRendererApi api, NkRendererFactory factory);
	/// @brief Remove an externally registered backend factory.
	static bool UnregisterExternalRendererFactory(NkRendererApi api);
	/// @brief Check if an external backend factory exists for an API.
	static bool HasExternalRendererFactory(NkRendererApi api);

	// --- Info ---

	/// @brief Active backend API for this renderer.
	NkRendererApi GetApi() const;
	/// @brief Human-readable backend API name.
	std::string GetApiName() const;
	/// @brief True when backend uses hardware acceleration.
	bool IsHardwareAccelerated() const;
	/// @brief Last backend error.
	NkError GetLastError() const;

	/// @brief Current framebuffer metadata.
	const NkFramebufferInfo &GetFramebufferInfo() const;
	/// @brief Runtime backend context (surface + backend-native handles).
	NkRendererContext GetContext() const;

	// --- Background color ---

	/// @brief Set default clear color used by BeginFrame sentinel color.
	void SetBackgroundColor(NkU32 rgba);
	/// @brief Get default clear color.
	NkU32 GetBackgroundColor() const;

	// --- Frame ---

	void BeginFrame(NkU32 clearColor = 0xFFFFFFFF);
	/// @brief Finish current frame recording.
	void EndFrame();
	/// @brief Present current frame to window surface.
	void Present();
	/// @brief Resize framebuffer/resources.
	void Resize(NkU32 width, NkU32 height);

	// --- Output ---

	/**
	 * @brief Enable/disable presentation to the window.
	 */
	void SetWindowPresentEnabled(bool enabled);
	bool IsWindowPresentEnabled() const;

	/**
	 * @brief Optional offscreen target (copy framebuffer on Present()).
	 */
	void SetExternalRenderTarget(NkRenderTexture *target);
	/// @brief Access currently configured external render target.
	NkRenderTexture *GetExternalRenderTarget() const;
	/// @brief Copy framebuffer into external render target.
	bool ResolveToExternalRenderTarget();

	// --- Color helpers ---

	/// @brief Pack RGBA bytes into 0xRRGGBBAA.
	static NkU32 PackColor(NkU8 r, NkU8 g, NkU8 b, NkU8 a = 255);
	/// @brief Unpack 0xRRGGBBAA into RGBA bytes.
	static void UnpackColor(NkU32 rgba, NkU8 &r, NkU8 &g, NkU8 &b, NkU8 &a);

	// --- 2D primitives ---

	void SetPixel(NkI32 x, NkI32 y, NkU32 rgba);
	/// @brief Compatibility alias of SetPixel().
	void DrawPixel(NkI32 x, NkI32 y, NkU32 rgba);

	// --- Impl access ---

	INkRendererImpl *GetImpl() {
		return mImpl.get();
	}
	const INkRendererImpl *GetImpl() const {
		return mImpl.get();
	}

private:
	std::unique_ptr<INkRendererImpl> mImpl;
	NkWindow *mWindow = nullptr;
	NkRenderTexture *mExternalTarget = nullptr;
	bool mWindowPresentEnabled = true;
	NkRendererConfig mConfig;
};

} // namespace nkentseu
