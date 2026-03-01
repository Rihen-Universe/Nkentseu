#pragma once

// =============================================================================
// NkSoftwareRendererImpl.h — Renderer logiciel (déclaration)
// Séparé de l'implémentation dans NkSoftwareRendererImpl.cpp.
// Present(surface) fait le blit OS (Win32/XCB/XLib/Android/WASM/iOS).
// =============================================================================

#include "../../Core/IRendererImpl.h"
#include <vector>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Class NkSoftwareRendererImpl.
 */
class NkSoftwareRendererImpl : public INkRendererImpl {
public:
	NkSoftwareRendererImpl() = default;
	~NkSoftwareRendererImpl() override;

	// -----------------------------------------------------------------------
	// IRendererImpl
	// -----------------------------------------------------------------------

	bool Init(const NkRendererConfig &config, const NkSurfaceDesc &surface) override;
	void Shutdown() override;

	bool IsValid() const override;
	NkRendererApi GetApi() const override;
	std::string GetApiName() const override;
	bool IsHardwareAccelerated() const override;
	NkError GetLastError() const override;

	const NkFramebufferInfo &GetFramebufferInfo() const override;

	void SetBackgroundColor(NkU32 rgba) override;
	NkU32 GetBackgroundColor() const override;

	void BeginFrame(NkU32 clearColor) override;
	void EndFrame() override;
	void Present(const NkSurfaceDesc &surface) override;
	void Resize(NkU32 width, NkU32 height) override;
	void SetPixel(NkI32 x, NkI32 y, NkU32 rgba) override;

	// -----------------------------------------------------------------------
	// Accès au pixel buffer (debug / rendu externe)
	// -----------------------------------------------------------------------

	const NkU8 *GetPixelBuffer() const {
		return mBuffer.data();
	}
	NkU8 *GetPixelBuffer() {
		return mBuffer.data();
	}

private:
	std::vector<NkU8> mBuffer;

	void AllocBuffer(NkU32 w, NkU32 h);
	void BlitOS(const NkSurfaceDesc &surface, NkU32 w, NkU32 h);

	// Blit par plateforme — définis dans le .cpp
	void BlitWin32(const NkSurfaceDesc &sd, NkU32 w, NkU32 h);
	void BlitXCB(const NkSurfaceDesc &sd, NkU32 w, NkU32 h);
	void BlitXLib(const NkSurfaceDesc &sd, NkU32 w, NkU32 h);
	void BlitANW(const NkSurfaceDesc &sd, NkU32 w, NkU32 h);
	void BlitWASM(const NkSurfaceDesc &sd, NkU32 w, NkU32 h);
};

} // namespace nkentseu
