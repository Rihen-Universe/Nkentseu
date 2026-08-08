#pragma once
// =============================================================================
// NkWaylandWindow.h - Wayland platform data for NkWindow (data only)
//
// Nouveaux champs ajoutés :
//   - mPendingSizeRelease / mPendingMin* / mPendingMax* : gestion SetSize manuel
//   - mVisible   : état de visibilité (attach/detach buffer)
//   - mMouseX/Y  : position courante de la souris (SetMousePosition)
//   - mMouseHidden / mMouseCaptured : état curseur
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkSurfaceHint.h"
#include <cstdint>

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_shm;
struct wl_seat;
struct wl_data_device_manager;
struct wl_surface;
struct wl_pointer;
struct wl_cursor_theme;
struct wl_cursor;
struct wl_buffer;
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;
struct libdecor;
struct libdecor_frame;

namespace nkentseu {

	class NkWaylandDropTarget;

	struct NkWindowData {
			// ----------------------------------------------------------------
			// Wayland globals
			// ----------------------------------------------------------------
			::wl_display *mDisplay = nullptr;
			::wl_registry *mRegistry = nullptr;
			::wl_compositor *mCompositor = nullptr;
			::wl_shm *mShm = nullptr;
			::wl_seat *mSeat = nullptr;
			::wl_data_device_manager *mDataDeviceManager = nullptr;

			// ----------------------------------------------------------------
			// Shell objects
			// ----------------------------------------------------------------
			::wl_surface *mSurface = nullptr;
			::xdg_wm_base *mWmBase = nullptr;
			::xdg_surface *mXdgSurface = nullptr;
			::xdg_toplevel *mXdgToplevel = nullptr;
			::libdecor *mLibdecor = nullptr;
			::libdecor_frame *mLibdecorFrame = nullptr;
			bool mUsingLibdecor = false;

			// ----------------------------------------------------------------
			// Server-side decorations (xdg-decoration-unstable-v1)
			// ----------------------------------------------------------------
			::zxdg_decoration_manager_v1 *mDecorationManager = nullptr;
			::zxdg_toplevel_decoration_v1 *mToplevelDecoration = nullptr;

			// ----------------------------------------------------------------
			// Curseur (wl_pointer vient de NkWaylandEventSystem)
			// On stocke ici une surface dédiée au curseur pour ShowMouse / curseur custom.
			// ----------------------------------------------------------------
			::wl_surface *mCursorSurface = nullptr;
			::wl_cursor_theme *mCursorTheme = nullptr;

			// ----------------------------------------------------------------
			// Software framebuffer (renderer software / buffer initial)
			// ----------------------------------------------------------------
			::wl_buffer *mBuffer = nullptr;
			void *mPixels = nullptr;
			uint32_t mStride = 0;

			// ----------------------------------------------------------------
			// wl_egl_window du contexte graphique (type opaque : on n'impose
			// pas <wayland-egl.h> a tout NKWindow).
			//
			// Il DOIT etre redimensionne DES LA RECEPTION du configure, pas a
			// la frame suivante : Mesa acquiert son tampon arriere pendant le
			// rendu, donc un redimensionnement arrive apres coup ne vaut que
			// pour la frame d'APRES. Entre les deux, eglSwapBuffers presente un
			// tampon a l'ANCIENNE taille alors que le compositeur en attend
			// une nouvelle -> xdg_wm_base error 4, fatale.
			// ----------------------------------------------------------------
			void *mEglWindow = nullptr;

			// ----------------------------------------------------------------
			// Acquittement DIFFERE d'un configure qui change la taille.
			//
			// La contrainte de taille d'xdg_surface ne s'applique qu'A PARTIR
			// de l'acquittement. Or eglSwapBuffers attache le tampon DEJA
			// RENDU, valide, PUIS seulement reallouer pour la frame suivante
			// (verifie a la trace : attach(vieux) -> commit -> create_buffer).
			// Acquitter tout de suite rendait donc illegale une presentation
			// deja en vol, et le compositeur tuait la fenetre.
			//
			// En differant d'une frame : la presentation perimee a lieu sous
			// l'ANCIENNE configuration (legale), Mesa realloue apres ce commit,
			// puis on acquitte — la frame suivante part a la bonne taille.
			//
			// Le PREMIER configure (avant mappage) doit rester acquitte tout de
			// suite, sinon la fenetre n'apparait jamais.
			// ----------------------------------------------------------------
			uint32_t mPendingAckSerial = 0;
			int32_t mPendingAckDelay = 0;

			// ----------------------------------------------------------------
			// Dimensions courantes
			// ----------------------------------------------------------------
			uint32_t mWidth = 0;
			uint32_t mHeight = 0;
			uint32_t mPrevWidth = 0;
			uint32_t mPrevHeight = 0;

			// ----------------------------------------------------------------
			// Flags d'état
			// ----------------------------------------------------------------
			bool mConfigured = false;
			bool mWantsClose = false;
			bool mFullscreen = false;
			// Etat MAXIMISE annonce par le compositeur (xdg_toplevel.configure).
			// Il etait lu puis jete : IsMaximized() renvoyait `false` en dur, si
			// bien que l'icone de la barre de titre ne changeait jamais et que
			// Maximize() — qui BASCULE — ne pouvait plus restaurer la fenetre.
			bool mMaximized = false;
			bool mPendingResize = false;
			bool mVisible = true;

			// ----------------------------------------------------------------
			// SetSize manuel : suggestion de taille au compositeur
			//
			// Flux :
			//   SetSize() → set_min=set_max=taille → commit
			//   OnToplevelConfigure() → mPendingResize = true
			//   PumpOS détecte mPendingSizeRelease → relâche les contraintes
			// ----------------------------------------------------------------
			bool mPendingSizeRelease = false;
			uint32_t mPendingMinW = 0;
			uint32_t mPendingMinH = 0;
			uint32_t mPendingMaxW = 0;
			uint32_t mPendingMaxH = 0;

			// ----------------------------------------------------------------
			// Souris
			// ----------------------------------------------------------------
			bool mMouseHidden = false;
			bool mMouseCaptured = false;
			int32_t mMouseX = 0;
			int32_t mMouseY = 0;

			// ----------------------------------------------------------------
			// Drag & drop
			// ----------------------------------------------------------------
			NkWaylandDropTarget *mDropTarget = nullptr;

			// ----------------------------------------------------------------
			// Surface hints applied at create-time (forwarded in NkSurfaceDesc)
			// ----------------------------------------------------------------
			NkSurfaceHints mAppliedHints{};
	};

	// Backend registry accessors
	class NkWindow;
	NkWindow *NkWaylandFindWindow(::wl_surface *surface);
	void NkWaylandRegisterWindow(::wl_surface *surface, NkWindow *win);
	// Confie le wl_egl_window a la fenetre proprietaire de `surface`, pour
	// qu'elle le redimensionne des la reception du configure (cf. mEglWindow).
	// Appele par le contexte OpenGL/EGL a la creation de la surface EGL.
	void NkWaylandSetEglWindow(::wl_surface *surface, void *eglWindow);
	// Retire ce wl_egl_window de TOUTE fenetre qui le reference, avant sa
	// destruction. Par valeur et non par surface : le contexte graphique ne
	// memorise pas la wl_surface, et laisser une reference pendante ferait
	// planter le prochain configure recu.
	void NkWaylandClearEglWindow(void *eglWindow);
	void NkWaylandUnregisterWindow(::wl_surface *surface);
	NkWindow *NkWaylandGetLastWindow();

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
