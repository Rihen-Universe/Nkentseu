// =============================================================================
// NkWaylandWindowImpl.cpp — Fenêtre Wayland
//
// Rendu pixel via wl_shm (ARGB8888 en mémoire partagée POSIX).
// Le buffer est créé lors de Create() et recréé à chaque SetSize().
// La boucle de rendu de l'application appelle PresentPixels() sur le
// Renderer, qui écrit dans mData.pixels et commit la surface.
// =============================================================================

#include "NkWaylandWindowImpl.h"
#include "NkWaylandEventImpl.h"
#include "../../Core/NkSystem.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// Constructeur / destructeur
// ---------------------------------------------------------------------------

NkWaylandWindowImpl::~NkWaylandWindowImpl() {
	if (mData.isOpen)
		Close();
}

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------

bool NkWaylandWindowImpl::Create(const NkWindowConfig &config) {
	mConfig = config;
	mBgColor = config.bgColor;
	mTitle = config.title;

	// 1. Connexion au compositeur Wayland
	mData.display = wl_display_connect(nullptr);
	if (!mData.display) {
		mLastError = NkError(1, "Wayland : impossible de se connecter au compositeur.");
		return false;
	}

	// 2. Registre des objets globaux
	mData.registry = wl_display_get_registry(mData.display);
	wl_registry_add_listener(mData.registry, &kRegistryListener, this);
	wl_display_roundtrip(mData.display); // bind des globals

	if (!mData.compositor || !mData.wmBase) {
		mLastError = NkError(2, "Wayland : wl_compositor ou xdg_wm_base non disponible.");
		return false;
	}

	xdg_wm_base_add_listener(mData.wmBase, &kWmBaseListener, nullptr);

	// 3. Surface
	mData.surface = wl_compositor_create_surface(mData.compositor);

	// 4. xdg_surface + xdg_toplevel (fenêtre)
	mData.xdgSurface = xdg_wm_base_get_xdg_surface(mData.wmBase, mData.surface);
	xdg_surface_add_listener(mData.xdgSurface, &kXdgSurfaceListener, this);

	mData.toplevel = xdg_surface_get_toplevel(mData.xdgSurface);
	xdg_toplevel_add_listener(mData.toplevel, &kToplevelListener, this);

	xdg_toplevel_set_title(mData.toplevel, config.title.c_str());
	xdg_toplevel_set_app_id(mData.toplevel, config.title.c_str());

	if (!config.resizable)
		xdg_toplevel_set_max_size(mData.toplevel, config.width, config.height);

	if (config.fullscreen)
		xdg_toplevel_set_fullscreen(mData.toplevel, nullptr);

	// 5. Premier commit pour déclencher xdg_surface::configure
	wl_surface_commit(mData.surface);
	wl_display_roundtrip(mData.display);

	// 6. Buffer SHM pour le rendu pixel
	NkU32 w = (mData.width > 0) ? mData.width : config.width;
	NkU32 h = (mData.height > 0) ? mData.height : config.height;
	mData.width = w;
	mData.height = h;

	if (!CreateShmBuffer(w, h))
		return false;

	// Remplir avec la couleur de fond
	NkU8 r = (mBgColor >> 24) & 0xFF;
	NkU8 g = (mBgColor >> 16) & 0xFF;
	NkU8 b = (mBgColor >> 8) & 0xFF;
	NkU8 a = 0xFF;
	NkU32 packed = (a << 24) | (r << 16) | (g << 8) | b;
	NkU32 count = w * h;
	auto *px = static_cast<NkU32 *>(mData.pixels);
	for (NkU32 i = 0; i < count; ++i)
		px[i] = packed;

	// Attacher et committer
	wl_surface_attach(mData.surface, mData.buffer, 0, 0);
	wl_surface_damage(mData.surface, 0, 0, static_cast<int32_t>(w), static_cast<int32_t>(h));
	wl_surface_commit(mData.surface);
	wl_display_flush(mData.display);

	mData.isOpen = true;

	// Enregistrer dans le gestionnaire d'événements
	if (auto *ev = static_cast<NkWaylandEventImpl *>(NkGetEventImpl()))
		ev->Initialize(this, &mData);

	return true;
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------

void NkWaylandWindowImpl::Close() {
	if (!mData.isOpen)
		return;

	if (auto *ev = static_cast<NkWaylandEventImpl *>(NkGetEventImpl()))
		ev->Shutdown(&mData);

	DestroyShmBuffer();

	if (mData.toplevel) {
		xdg_toplevel_destroy(mData.toplevel);
		mData.toplevel = nullptr;
	}
	if (mData.xdgSurface) {
		xdg_surface_destroy(mData.xdgSurface);
		mData.xdgSurface = nullptr;
	}
	if (mData.surface) {
		wl_surface_destroy(mData.surface);
		mData.surface = nullptr;
	}
	if (mData.wmBase) {
		xdg_wm_base_destroy(mData.wmBase);
		mData.wmBase = nullptr;
	}
	if (mData.shm) {
		wl_shm_destroy(mData.shm);
		mData.shm = nullptr;
	}
	if (mData.seat) {
		wl_seat_destroy(mData.seat);
		mData.seat = nullptr;
	}
	if (mData.compositor) {
		wl_compositor_destroy(mData.compositor);
		mData.compositor = nullptr;
	}
	if (mData.registry) {
		wl_registry_destroy(mData.registry);
		mData.registry = nullptr;
	}
	if (mData.display) {
		wl_display_disconnect(mData.display);
		mData.display = nullptr;
	}

	mData.isOpen = false;
}

// ---------------------------------------------------------------------------
// Buffer SHM (shared memory — rendu pixel software)
// ---------------------------------------------------------------------------

bool NkWaylandWindowImpl::CreateShmBuffer(NkU32 w, NkU32 h) {
	mData.stride = w * 4;
	size_t size = static_cast<size_t>(mData.stride) * h;

	// Créer un fd de mémoire anonyme
	mData.shmFd = memfd_create("nk_wayland_shm", MFD_CLOEXEC);
	if (mData.shmFd < 0) {
		mLastError = NkError(3, "Wayland : memfd_create échoué.");
		return false;
	}

	if (ftruncate(mData.shmFd, static_cast<off_t>(size)) < 0) {
		close(mData.shmFd);
		mData.shmFd = -1;
		mLastError = NkError(4, "Wayland : ftruncate échoué.");
		return false;
	}

	mData.pixels = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, mData.shmFd, 0);
	if (mData.pixels == MAP_FAILED) {
		close(mData.shmFd);
		mData.shmFd = -1;
		mData.pixels = nullptr;
		mLastError = NkError(5, "Wayland : mmap échoué.");
		return false;
	}

	// Créer le wl_shm_pool + buffer
	wl_shm_pool *pool = wl_shm_create_pool(mData.shm, mData.shmFd, static_cast<int32_t>(size));
	mData.buffer = wl_shm_pool_create_buffer(pool, 0, static_cast<int32_t>(w), static_cast<int32_t>(h),
											 static_cast<int32_t>(mData.stride), WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);

	return mData.buffer != nullptr;
}

void NkWaylandWindowImpl::DestroyShmBuffer() {
	if (mData.buffer) {
		wl_buffer_destroy(mData.buffer);
		mData.buffer = nullptr;
	}
	if (mData.pixels && mData.shmFd >= 0) {
		size_t size = static_cast<size_t>(mData.stride) * mData.height;
		munmap(mData.pixels, size);
		mData.pixels = nullptr;
	}
	if (mData.shmFd >= 0) {
		close(mData.shmFd);
		mData.shmFd = -1;
	}
}

// ---------------------------------------------------------------------------
// Propriétés
// ---------------------------------------------------------------------------

bool NkWaylandWindowImpl::IsOpen() const {
	return mData.isOpen;
}
NkError NkWaylandWindowImpl::GetLastError() const {
	return mLastError;
}
std::string NkWaylandWindowImpl::GetTitle() const {
	return mTitle;
}
NkVec2u NkWaylandWindowImpl::GetSize() const {
	return {mData.width, mData.height};
}
NkVec2u NkWaylandWindowImpl::GetDisplaySize() const {
	// Wayland ne fournit pas directement la taille de l'écran sans wl_output
	// Retourne la taille courante de la fenêtre par défaut
	return {mData.width, mData.height};
}

void NkWaylandWindowImpl::SetTitle(const std::string &t) {
	mTitle = t;
	mConfig.title = t;
	if (mData.toplevel)
		xdg_toplevel_set_title(mData.toplevel, t.c_str());
}

// ---------------------------------------------------------------------------
// Window state
// ---------------------------------------------------------------------------

void NkWaylandWindowImpl::SetSize(NkU32 w, NkU32 h) {
	if (w == mData.width && h == mData.height)
		return;

	DestroyShmBuffer();
	mData.width = w;
	mData.height = h;
	CreateShmBuffer(w, h);

	if (mData.surface && mData.buffer) {
		wl_surface_attach(mData.surface, mData.buffer, 0, 0);
		wl_surface_damage(mData.surface, 0, 0, static_cast<int32_t>(w), static_cast<int32_t>(h));
		wl_surface_commit(mData.surface);
	}
}

void NkWaylandWindowImpl::SetVisible(bool v) {
	// Wayland : pas de masquage direct ; on peut détacher le buffer
	if (!v && mData.surface) {
		wl_surface_attach(mData.surface, nullptr, 0, 0);
		wl_surface_commit(mData.surface);
	} else if (v && mData.surface && mData.buffer) {
		wl_surface_attach(mData.surface, mData.buffer, 0, 0);
		wl_surface_damage(mData.surface, 0, 0, static_cast<int32_t>(mData.width), static_cast<int32_t>(mData.height));
		wl_surface_commit(mData.surface);
	}
}

void NkWaylandWindowImpl::Minimize() {
	if (mData.toplevel)
		xdg_toplevel_set_minimized(mData.toplevel);
}

void NkWaylandWindowImpl::Maximize() {
	if (mData.toplevel)
		xdg_toplevel_set_maximized(mData.toplevel);
}

void NkWaylandWindowImpl::Restore() {
	if (mData.toplevel)
		xdg_toplevel_unset_maximized(mData.toplevel);
}

void NkWaylandWindowImpl::SetFullscreen(bool fs) {
	mConfig.fullscreen = fs;
	mData.fullscreen = fs;
	if (!mData.toplevel)
		return;
	if (fs)
		xdg_toplevel_set_fullscreen(mData.toplevel, nullptr);
	else
		xdg_toplevel_unset_fullscreen(mData.toplevel);
	if (mData.surface)
		wl_surface_commit(mData.surface);
}

void NkWaylandWindowImpl::SetMousePosition(NkU32 /*x*/, NkU32 /*y*/) {
	// Wayland ne permet pas de repositionner le curseur arbitrairement
	// (seulement via wl_pointer::warp dans les surfaces confinées)
}

void NkWaylandWindowImpl::ShowMouse(bool /*show*/) {
	// Requiert wl_pointer + un curseur wl_cursor_theme — stub
}

void NkWaylandWindowImpl::CaptureMouse(bool /*cap*/) {
	// Requiert zwp_pointer_constraints — stub
}

// ---------------------------------------------------------------------------
// Surface de rendu (utilisée par NkSoftwareRendererImpl)
// ---------------------------------------------------------------------------

NkSurfaceDesc NkWaylandWindowImpl::GetSurfaceDesc() const {
	NkSurfaceDesc sd;
	sd.width = mData.width;
	sd.height = mData.height;
	// Passer les pointeurs Wayland dans les champs génériques
	sd.display = mData.display;
	sd.window = mData.surface; // wl_surface* casté en void*
	sd.pixels = mData.pixels;  // Accès direct aux pixels SHM
	sd.stride = mData.stride;
	return sd;
}

// ---------------------------------------------------------------------------
// Listeners Wayland (statiques)
// ---------------------------------------------------------------------------

void NkWaylandWindowImpl::OnRegistryGlobal(void *data, wl_registry *reg, uint32_t id, const char *iface, uint32_t ver) {
	auto *self = static_cast<NkWaylandWindowImpl *>(data);

	if (strcmp(iface, wl_compositor_interface.name) == 0)
		self->mData.compositor = static_cast<wl_compositor *>(wl_registry_bind(reg, id, &wl_compositor_interface, 4));
	else if (strcmp(iface, wl_shm_interface.name) == 0)
		self->mData.shm = static_cast<wl_shm *>(wl_registry_bind(reg, id, &wl_shm_interface, 1));
	else if (strcmp(iface, xdg_wm_base_interface.name) == 0)
		self->mData.wmBase = static_cast<xdg_wm_base *>(wl_registry_bind(reg, id, &xdg_wm_base_interface, 1));
	else if (strcmp(iface, wl_seat_interface.name) == 0)
		self->mData.seat = static_cast<wl_seat *>(wl_registry_bind(reg, id, &wl_seat_interface, (ver < 5) ? ver : 5));
	(void)ver;
}

void NkWaylandWindowImpl::OnRegistryGlobalRemove(void *, wl_registry *, uint32_t) {
}

void NkWaylandWindowImpl::OnXdgWmBasePing(void *, xdg_wm_base *base, uint32_t serial) {
	xdg_wm_base_pong(base, serial);
}

void NkWaylandWindowImpl::OnXdgSurfaceConfigure(void *data, xdg_surface *surf, uint32_t serial) {
	auto *self = static_cast<NkWaylandWindowImpl *>(data);
	xdg_surface_ack_configure(surf, serial);
	self->mData.configured = true;
}

void NkWaylandWindowImpl::OnXdgToplevelConfigure(void *data, xdg_toplevel *, int32_t w, int32_t h, wl_array *) {
	auto *self = static_cast<NkWaylandWindowImpl *>(data);
	if (w > 0 && h > 0) {
		// Le compositeur impose une nouvelle taille
		if (static_cast<NkU32>(w) != self->mData.width || static_cast<NkU32>(h) != self->mData.height) {
			self->mData.width = static_cast<NkU32>(w);
			self->mData.height = static_cast<NkU32>(h);
			// Le buffer sera recréé lors du prochain Present
		}
	}
}

void NkWaylandWindowImpl::OnXdgToplevelClose(void *data, xdg_toplevel *) {
	auto *self = static_cast<NkWaylandWindowImpl *>(data);
	self->mData.wantsClose = true;
}

} // namespace nkentseu
