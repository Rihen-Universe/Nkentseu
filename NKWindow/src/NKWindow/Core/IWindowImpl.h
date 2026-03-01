#pragma once

// =============================================================================
// IWindowImpl.h
// Interface PIMPL interne que chaque plateforme doit implémenter.
//
// Règles d'architecture (refonte v2) :
//   - NE détient PAS de pointeur vers IEventImpl.
//   - Create() reçoit IEventImpl& pour appeler Initialize/Shutdown,
//     mais ne stocke PAS ce pointeur.
//   - SetEventCallback / DispatchEvent → IEventImpl.
//   - BlitSoftwareFramebuffer / GetSurfaceDesc → IRendererImpl.
//   - SetBackgroundColor / GetBackgroundColor → IRendererImpl.
// =============================================================================

#include "NkWindowConfig.h"
#include "NKWindow/Events/NkEvent.h"
#include "NkSurface.h" // pour NkSurfaceDesc (accès par Renderer)
#include "NkSafeArea.h"
#include <string>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

class IEventImpl;

// ---------------------------------------------------------------------------
// IWindowImpl
// ---------------------------------------------------------------------------

class IWindowImpl {
public:
	virtual ~IWindowImpl() = default;

	// -----------------------------------------------------------------------
	// Cycle de vie
	// -----------------------------------------------------------------------

	/**
	 * Crée la fenêtre native.
	 * En fin de Create(), appeler :
	 *     eventImpl.Initialize(this, &nativeHandle);
	 * En début de Close(), appeler :
	 *     eventImpl.Shutdown(&nativeHandle);  // si on stocke l'impl
	 *
	 * Note : l'impl ne STOCKE PAS eventImpl — elle le reçoit uniquement
	 * pour appeler Initialize.
	 */
	virtual bool Create(const NkWindowConfig &config) = 0;
	virtual void Close() = 0;
	virtual bool IsOpen() const = 0;

	// -----------------------------------------------------------------------
	// Propriétés lecture
	// -----------------------------------------------------------------------

	virtual std::string GetTitle() const = 0;
	virtual NkVec2u GetSize() const = 0;
	virtual NkVec2u GetPosition() const = 0;
	virtual float GetDpiScale() const = 0;
	virtual NkVec2u GetDisplaySize() const = 0;
	virtual NkVec2u GetDisplayPosition() const = 0;
	virtual NkError GetLastError() const = 0;

	// -----------------------------------------------------------------------
	// Propriétés écriture
	// -----------------------------------------------------------------------

	virtual void SetTitle(const std::string &title) = 0;
	virtual void SetSize(NkU32 width, NkU32 height) = 0;
	virtual void SetPosition(NkI32 x, NkI32 y) = 0;
	virtual void SetVisible(bool visible) = 0;
	virtual void Minimize() = 0;
	virtual void Maximize() = 0;
	virtual void Restore() = 0;
	virtual void SetFullscreen(bool fullscreen) = 0;

	// -----------------------------------------------------------------------
	// Orientation écran (mobile)
	// -----------------------------------------------------------------------

	virtual bool SupportsOrientationControl() const {
		return false;
	}
	virtual void SetScreenOrientation(NkScreenOrientation) {
	}
	virtual NkScreenOrientation GetScreenOrientation() const {
		return NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
	}
	virtual void SetAutoRotateEnabled(bool enabled) {
		if (enabled)
			SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO);
	}
	virtual bool IsAutoRotateEnabled() const {
		return GetScreenOrientation() == NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
	}

	// -----------------------------------------------------------------------
	// Souris
	// -----------------------------------------------------------------------

	virtual void SetMousePosition(NkU32 x, NkU32 y) = 0;
	virtual void ShowMouse(bool show) = 0;
	virtual void CaptureMouse(bool capture) = 0;

	// -----------------------------------------------------------------------
	// Web input routing (WASM) — no-op par défaut
	// -----------------------------------------------------------------------

	virtual void SetWebInputOptions(const NkWebInputOptions &) {
	}
	virtual NkWebInputOptions GetWebInputOptions() const {
		return {};
	}

	// -----------------------------------------------------------------------
	// Divers
	// -----------------------------------------------------------------------

	virtual void SetProgress(float progress) = 0; ///< Barre de tâches OS

	// -----------------------------------------------------------------------
	// Descripteur de surface — accédé par Renderer pour créer ses ressources
	// -----------------------------------------------------------------------

	virtual NkSurfaceDesc GetSurfaceDesc() const = 0;

	// -----------------------------------------------------------------------
	// Safe area (mobile uniquement — retourne {0,0,0,0} sur desktop)
	// -----------------------------------------------------------------------

	/**
	 * @brief Retourne les marges de la zone sûre (encoche, home indicator…).
	 * Sur desktop : retourne NkSafeAreaInsets{} (tout à zéro).
	 * Sur iOS     : lit UIView.safeAreaInsets.
	 * Sur Android : lit WindowInsets système.
	 */
	virtual NkSafeAreaInsets GetSafeAreaInsets() const {
		return {};
	}

protected:
	NkWindowConfig mConfig;
	NkError mLastError;
};

} // namespace nkentseu
