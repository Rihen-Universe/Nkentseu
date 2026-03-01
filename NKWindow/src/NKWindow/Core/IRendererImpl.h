#pragma once

// =============================================================================
// IRendererImpl.h
// Interface PIMPL interne pour chaque backend de rendu.
//
// V2 : SetBackgroundColor / GetBackgroundColor vivent ici (plus dans Window).
//      Present() reçoit la surface et fait le blit vers la fenêtre.
// =============================================================================

#include "NkSurface.h"
#include "NkTypes.h"
#include <string>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

class Window;

/**
 * @brief Class INkRendererImpl.
 */
class INkRendererImpl {
public:
	virtual ~INkRendererImpl() = default;

	// Création / destruction
	virtual bool Init(const NkRendererConfig &config, const NkSurfaceDesc &surface) = 0;
	virtual void Shutdown() = 0;
	virtual bool IsValid() const = 0;

	// Informations
	virtual NkRendererApi GetApi() const = 0;
	virtual std::string GetApiName() const = 0;
	virtual bool IsHardwareAccelerated() const = 0;
	virtual NkError GetLastError() const = 0;

	virtual const NkFramebufferInfo &GetFramebufferInfo() const = 0;
	virtual NkRendererContext GetContext() const {
		return NkMakeRendererContext(GetApi(), mSurface, mFbInfo);
	}

	// Trame
	virtual void BeginFrame(NkU32 clearColor) = 0;
	virtual void EndFrame() = 0;

	/**
	 * @brief Présente le framebuffer vers la fenêtre.
	 *        Responsable du blit OS (StretchDIBits, CAMetalLayer, glSwapBuffers…).
	 * @param surface  Descripteur natif de la fenêtre cible.
	 */
	virtual void Present(const NkSurfaceDesc &surface) = 0;

	virtual void Resize(NkU32 width, NkU32 height) = 0;
	virtual void SetPixel(NkI32 x, NkI32 y, NkU32 rgba) = 0;

	// Couleur de fond (anciennement dans Window)
	virtual void SetBackgroundColor(NkU32 rgba) = 0;
	virtual NkU32 GetBackgroundColor() const = 0;

protected:
	NkRendererConfig mConfig;
	NkSurfaceDesc mSurface;
	NkFramebufferInfo mFbInfo;
	NkError mLastError;
	bool mReady = false;
	NkU32 mBgColor = 0x141414FF;
};

} // namespace nkentseu
