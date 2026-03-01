#pragma once

// =============================================================================
// NkWaylandWindowImpl.h — Fenêtre Wayland (wl_surface + xdg-shell)
//
// Rendu pixel via wl_shm (shared memory buffer) : même API que XLib/XCB
// mais sans référence à X11. Compatible avec les compositeurs modernes
// (GNOME/Mutter, KDE/KWin, wlroots, etc.).
//
// Limites Wayland : pas de positionnement absolu des fenêtres (le
// compositeur décide), donc SetPosition() est un no-op.
// =============================================================================

#include "../../Core/IWindowImpl.h"
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <string>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// NkWaylandData — état interne de la fenêtre Wayland
// ---------------------------------------------------------------------------
struct NkWaylandData {
	// Globals Wayland
	wl_display *display = nullptr;		 ///< Connexion au compositeur
	wl_registry *registry = nullptr;	 ///< Registre des objets globaux
	wl_compositor *compositor = nullptr; ///< Fabrique de surfaces
	wl_shm *shm = nullptr;				 ///< Mémoire partagée (rendu pixel)
	wl_seat *seat = nullptr;			 ///< Clavier + souris + touch

	// Surface et shell
	wl_surface *surface = nullptr;	   ///< Surface de rendu
	xdg_wm_base *wmBase = nullptr;	   ///< Gestionnaire de fenêtres xdg
	xdg_surface *xdgSurface = nullptr; ///< Surface xdg (événements shell)
	xdg_toplevel *toplevel = nullptr;  ///< Fenêtre niveau supérieur

	// Buffer partagé (rendu software)
	wl_buffer *buffer = nullptr; ///< Buffer Wayland actif
	int shmFd = -1;				 ///< File descriptor mémoire partagée
	void *pixels = nullptr;		 ///< Pointeur vers les pixels ARGB8
	NkU32 stride = 0;			 ///< Largeur × 4 (octets par ligne)

	// Dimensions et état
	NkU32 width = 0;
	NkU32 height = 0;
	bool isOpen = false;
	bool configured = false; ///< xdg_surface::configure reçu
	bool wantsClose = false; ///< xdg_toplevel::close reçu
	bool fullscreen = false;
};

// ---------------------------------------------------------------------------
// NkWaylandWindowImpl
// ---------------------------------------------------------------------------
class NkWaylandWindowImpl : public IWindowImpl {
public:
	NkWaylandWindowImpl() = default;
	~NkWaylandWindowImpl() override;

	// --- Cycle de vie -------------------------------------------------------
	bool Create(const NkWindowConfig &config) override;
	void Close() override;
	bool IsOpen() const override;

	// --- Propriétés ---------------------------------------------------------
	std::string GetTitle() const override;
	void SetTitle(const std::string &t) override;
	NkVec2u GetSize() const override;
	NkVec2u GetPosition() const override {
		return {};
	} ///< Wayland n'expose pas la position
	float GetDpiScale() const override {
		return 1.f;
	}
	NkVec2u GetDisplaySize() const override;
	NkVec2u GetDisplayPosition() const override {
		return {};
	}
	NkError GetLastError() const override;

	// --- Window state -------------------------------------------------------
	void SetSize(NkU32 w, NkU32 h) override;
	void SetPosition(NkI32, NkI32) override {
	} ///< no-op sous Wayland
	void SetVisible(bool v) override;
	void Minimize() override;
	void Maximize() override;
	void Restore() override;
	void SetFullscreen(bool fs) override;
	void SetMousePosition(NkU32 x, NkU32 y) override;
	void ShowMouse(bool show) override;
	void CaptureMouse(bool cap) override;
	void SetProgress(float) override {
	} ///< non supporté

	// --- Surface de rendu ---------------------------------------------------
	NkSurfaceDesc GetSurfaceDesc() const override;

	// --- Accès natifs -------------------------------------------------------
	wl_display *GetWlDisplay() const {
		return mData.display;
	}
	wl_surface *GetWlSurface() const {
		return mData.surface;
	}
	bool WantsClose() const {
		return mData.wantsClose;
	}
	void ClearClose() {
		mData.wantsClose = false;
	}

private:
	NkWaylandData mData;
	NkU32 mBgColor = 0x141414FF;
	std::string mTitle;

	// --- Gestion du buffer SHM ----------------------------------------------
	bool CreateShmBuffer(NkU32 w, NkU32 h);
	void DestroyShmBuffer();

	// -----------------------------------------------------------------------
	// Listeners statiques (callbacks Wayland)
	// -----------------------------------------------------------------------

	// Registre global
	static void OnRegistryGlobal(void *data, wl_registry *reg, uint32_t id, const char *iface, uint32_t ver);
	static void OnRegistryGlobalRemove(void *data, wl_registry *reg, uint32_t id);

	static constexpr wl_registry_listener kRegistryListener = {
		.global = OnRegistryGlobal,
		.global_remove = OnRegistryGlobalRemove,
	};

	// xdg_wm_base ping/pong
	static void OnXdgWmBasePing(void *data, xdg_wm_base *base, uint32_t serial);

	static constexpr xdg_wm_base_listener kWmBaseListener = {
		.ping = OnXdgWmBasePing,
	};

	// xdg_surface configure
	static void OnXdgSurfaceConfigure(void *data, xdg_surface *surf, uint32_t serial);

	static constexpr xdg_surface_listener kXdgSurfaceListener = {
		.configure = OnXdgSurfaceConfigure,
	};

	// xdg_toplevel configure + close
	static void OnXdgToplevelConfigure(void *data, xdg_toplevel *tl, int32_t w, int32_t h, wl_array *states);
	static void OnXdgToplevelClose(void *data, xdg_toplevel *tl);

	static constexpr xdg_toplevel_listener kToplevelListener = {
		.configure = OnXdgToplevelConfigure,
		.close = OnXdgToplevelClose,
	};
};

} // namespace nkentseu
