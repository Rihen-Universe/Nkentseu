#pragma once

// =============================================================================
// NkWindow.h
// Classe publique Window — façade PIMPL vers IWindowImpl.
//
// Usage simplifié (avec NkInitialise) :
//   nkentseu::NkInitialise();
//
//   nkentseu::NkWindowConfig cfg;
//   cfg.title = "Hello NkWindow";
//   nkentseu::Window window(cfg);
//   if (!window.IsOpen()) { /* erreur */ }
//
//   nkentseu::NkRenderer renderer(window);
//   while (window.IsOpen()) {
//       nkentseu::EventSystem::Instance().PollEvents();
//       renderer.BeginFrame(0x141414FF);
//       // draw...
//       renderer.EndFrame();
//       renderer.Present();
//   }
//   nkentseu::NkClose();
// =============================================================================

#include "NkWindowConfig.h"
#include "NkSafeArea.h"
#include "NkSurface.h"
#include "NkEvents.h"
#include "IWindowImpl.h"
#include "NKWindow/Events/IEventImpl.h"
#include "NkSystem.h"
#include <memory>
#include <string>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

class IEventImpl;

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

/**
 * @brief Cross-platform window facade.
 *
 * Window wraps platform implementations (Win32/X11/Wayland/Cocoa/UIKit/...)
 * and exposes a unified API for lifecycle, sizing, input policy and native
 * surface retrieval for rendering backends.
 */
class NkWindow {
public:
	// --- Construction ---

	/// @brief Create an empty window. Call Create() to initialize.
	NkWindow();
	/// @brief Create and initialize a window from config.
	explicit NkWindow(const NkWindowConfig &config);
	/// @brief Destroy the window and release platform resources.
	~NkWindow();

	NkWindow(const NkWindow &) = delete;
	NkWindow &operator=(const NkWindow &) = delete;
	NkWindow(NkWindow &&) = default;
	NkWindow &operator=(NkWindow &&) = default;

	// --- Cycle de vie ---

	/**
	 * Crée la fenêtre. NkInitialise() doit avoir été appelé avant.
	 * Utilise automatiquement l'IEventImpl fourni par NkSystem.
	 */
	bool Create(const NkWindowConfig &config);
	/// @brief Request window close.
	void Close();
	/// @brief True if native window is open.
	bool IsOpen() const;
	/// @brief True when platform implementation is valid.
	bool IsValid() const;

	// --- Propriétés ---

	std::string GetTitle() const;
	/// @brief Update window title.
	void SetTitle(const std::string &title);
	NkVec2u GetSize() const;
	NkVec2u GetPosition() const;
	float GetDpiScale() const;
	NkVec2u GetDisplaySize() const;
	NkVec2u GetDisplayPosition() const;
	NkError GetLastError() const;
	/// @brief Current runtime window configuration.
	NkWindowConfig GetConfig() const;

	// --- Manipulation ---

	/// @brief Resize window client area.
	void SetSize(NkU32 width, NkU32 height);
	/// @brief Move window on screen.
	void SetPosition(NkI32 x, NkI32 y);
	/// @brief Show or hide the window.
	void SetVisible(bool visible);
	/// @brief Minimize window.
	void Minimize();
	/// @brief Maximize window.
	void Maximize();
	/// @brief Restore from minimized/maximized state.
	void Restore();
	/// @brief Toggle fullscreen mode.
	void SetFullscreen(bool fullscreen);
	bool SupportsOrientationControl() const;
	void SetScreenOrientation(NkScreenOrientation orientation);
	NkScreenOrientation GetScreenOrientation() const;
	void SetAutoRotateEnabled(bool enabled);
	bool IsAutoRotateEnabled() const;

	// --- Souris ---

	/// @brief Warp mouse cursor inside the window.
	void SetMousePosition(NkU32 x, NkU32 y);
	/// @brief Show or hide mouse cursor.
	void ShowMouse(bool show);
	/// @brief Enable or disable mouse capture.
	void CaptureMouse(bool capture);

	// --- Web / WASM input policy ---

	/**
	 * @brief Configure le routage des entrées navigateur <-> application (WASM).
	 * Sur les autres plateformes: stocke la config mais n'a pas d'effet runtime.
	 */
	void SetWebInputOptions(const NkWebInputOptions &options);
	NkWebInputOptions GetWebInputOptions() const;

	// --- OS extras ---

	void SetProgress(float progress); ///< Progression barre des tâches

	// --- Safe Area (mobile) ---

	/**
	 * @brief Retourne les insets de la zone sécurisée.
	 * Sur desktop : tout à 0. Sur mobile : notch, home indicator…
	 * Utiliser avec NkWindowConfig::respectSafeArea = true.
	 */
	NkSafeAreaInsets GetSafeAreaInsets() const;

	// --- Surface graphique (pour NkRenderer) ---

	/// @brief Return native rendering surface description.
	NkSurfaceDesc GetSurfaceDesc() const;

	// --- Callback événements (délégué à l'EventImpl) ---

	/**
	 * Enregistre un callback pour les événements de CETTE fenêtre uniquement.
	 * Délégué à IEventImpl::SetWindowCallback().
	 */
	void SetEventCallback(NkEventCallback callback);

	// --- Accès impl interne ---

	IWindowImpl *GetImpl() {
		return mImpl.get();
	}
	const IWindowImpl *GetImpl() const {
		return mImpl.get();
	}

private:
	std::unique_ptr<IWindowImpl> mImpl;
	NkWindowConfig mConfig;
};

} // namespace nkentseu
