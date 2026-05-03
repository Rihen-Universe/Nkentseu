#pragma once
// =============================================================================
// Fichier : NkFramebuffer.h
// Description : Framebuffer logiciel à double tampon pour le rendu graphique
// =============================================================================
// Composants principaux :
//   - NkPixelBuffer        : Stockage de pixels aligné sur 32 octets (compatible SIMD)
//   - NkPlatformPresenter  : Affichage via DIBSection Win32 (conversion RGBA→BGRA via SIMD)
//   - NkRenderer           : Gestion du double tampon + effacement + présentation à l'écran
//
// Stratégie d'optimisation SIMD :
//   - Effacement (Clear)   : AVX2 (8 pixels/itération) → SSE2 (4 pixels/itération) → scalaire
//   - Présentation (Present) : Échange R↔B en scalaire (une seule fois par frame, non critique)
//   - Copie (CopyTo)       : memcpy (le compilateur utilise automatiquement rep movs / SSE)
// =============================================================================

#include "NKWindow/NkWindow.h"
#include "NKMath/NKMath.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkPlatform.h"
#include <cstring>
#include <cstdlib>
#include <utility>

// Inclusions spécifiques à chaque plateforme cible
// NOTE: NKPlatform/NkPlatform.h gère déjà tous les includes spécifiques de plateforme,
// donc nous n'avons pas besoin de les répéter ici.
#if 0  // Anciens includes - maintenant gérés par NKPlatform
#if defined(_WIN32) || defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <windows.h>
#elif defined(__ANDROID__)
#   include <android/native_window.h>
#   include <malloc.h>  // Pour memalign sur Android
#elif defined(__EMSCRIPTEN__)
#   include <emscripten/emscripten.h>
#   include <emscripten/html5.h>
#elif defined(__linux__)
#   if defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
#       include <wayland-client.h>
#   elif defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
#       include <xcb/xcb.h>
#   else
#       include <X11/Xlib.h>
#       include <X11/Xutil.h>
#   endif
#endif
#endif

// ── Détection automatique des extensions SIMD disponibles ─────────────────────
#if defined(__AVX2__)
#   include <immintrin.h>
#   define NK_SIMD_AVX2 1
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#   include <emmintrin.h>
#   define NK_SIMD_SSE2 1
#endif

// ── Fonctions utilitaires pour l'allocation mémoire alignée ───────────────────
// Réutilise l'API multi-plateforme de NKPlatform pour cohérence et maintenabilité
namespace nkentseu {
    namespace detail {
        // Alloue un bloc de mémoire aligné sur 32 octets (requis pour les instructions SIMD)
        // Délègue à nkentseu::platform::memory::NkAllocateAligned qui gère :
        //   - Windows : _aligned_malloc
        //   - Android : posix_memalign (C11) ou memalign fallback
        //   - macOS : posix_memalign
        //   - Linux : aligned_alloc (C11) ou posix_memalign fallback
        inline void* AlignedAlloc32(size_t bytes) {
            // Arrondir la taille demandée au multiple de 32 supérieur
            size_t aligned = (bytes + 31u) & ~size_t(31u);
            
            // Utiliser l'allocateur multi-plateforme du framework
            return nkentseu::memory::NkAllocateAligned(aligned, 32u);
        }

        // Libère un bloc de mémoire précédemment alloué avec AlignedAlloc32
        // Utilise l'API standard du framework qui détecte automatiquement le type d'allocation
        inline void AlignedFree32(void* p) {
            nkentseu::memory::NkFreeAligned(p);
        }

        /*
        // Alloue un bloc de mémoire aligné sur 32 octets (requis pour les instructions SIMD)
        inline void* AlignedAlloc32(size_t bytes) {
            // Arrondir la taille demandée au multiple de 32 supérieur
            size_t aligned = (bytes + 31u) & ~size_t(31u);

            // Utilisation des fonctions spécifiques selon le compilateur
            #if defined(_MSC_VER) || defined(__MINGW32__)
                return _aligned_malloc(aligned, 32);
            #elif defined(__ANDROID__)
                return ::memalign(32, aligned);
            #else
                return ::aligned_alloc(32, aligned);
            #endif
        }

        // Libère un bloc de mémoire précédemment alloué avec AlignedAlloc32
        inline void AlignedFree32(void* p) {
            #if defined(_MSC_VER) || defined(__MINGW32__)
                _aligned_free(p);
            #else
                ::free(p);
            #endif
        }
        */
    } // namespace detail
} // namespace nkentseu

namespace nkentseu {
    // =============================================================================
    // Structure : NkPixelBuffer
    // Description : Stockage linéaire de pixels au format RGBA8 (8 bits par composante)
    // =============================================================================
    struct NkPixelBuffer {
        uint8* data;            // Pointeur vers le tableau de pixels brut
        uint32 width;           // Largeur de l'image en pixels
        uint32 height;          // Hauteur de l'image en pixels
        uint32 stride;          // Nombre d'octets par ligne (égal à width * 4 pour RGBA)

        // Constructeur par défaut : initialise tous les membres à zéro/nullptr
        NkPixelBuffer() = default;

        // Constructeur avec allocation immédiate pour une taille donnée
        explicit NkPixelBuffer(uint32 w, uint32 h) {
            Allocate(w, h);
        }

        // Destructeur : libère automatiquement la mémoire allouée
        ~NkPixelBuffer() {
            Free();
        }

        // Suppression du constructeur de copie (sémantique de déplacement uniquement)
        NkPixelBuffer(const NkPixelBuffer&) = delete;

        // Suppression de l'opérateur d'affectation par copie
        NkPixelBuffer& operator=(const NkPixelBuffer&) = delete;

        // Constructeur de déplacement : transfère la possession des ressources
        NkPixelBuffer(NkPixelBuffer&& o) noexcept
            : data(o.data)
            , width(o.width)
            , height(o.height)
            , stride(o.stride) {
            o.data = nullptr;
            o.width = 0;
            o.height = 0;
            o.stride = 0;
        }

        // Opérateur d'affectation par déplacement : libère puis transfère les ressources
        NkPixelBuffer& operator=(NkPixelBuffer&& o) noexcept {
            if (this != &o) {
                Free();
                data = o.data;
                width = o.width;
                height = o.height;
                stride = o.stride;
                o.data = nullptr;
                o.width = 0;
                o.height = 0;
                o.stride = 0;
            }
            return *this;
        }

        // ── Gestion du cycle de vie : allocation et libération ─────────────────
        // Alloue un tampon de pixels pour les dimensions spécifiées
        bool Allocate(uint32 w, uint32 h) {
            Free();

            if (w == 0 || h == 0) {
                return false;
            }

            stride = w * 4u;
            size_t bytes = static_cast<size_t>(stride) * h;
            data = static_cast<uint8*>(detail::AlignedAlloc32(bytes));

            if (!data) {
                return false;
            }

            width = w;
            height = h;
            return true;
        }

        // Libère la mémoire du tampon de pixels et réinitialise les métadonnées
        void Free() {
            if (data) {
                detail::AlignedFree32(data);
                data = nullptr;
            }
            width = 0;
            height = 0;
            stride = 0;
        }

        // Vérifie si le tampon est valide (pointeur de données non nul)
        bool IsValid() const noexcept {
            return data != nullptr;
        }

        // Retourne la taille totale du tampon en octets
        size_t SizeBytes() const noexcept {
            return static_cast<size_t>(stride) * height;
        }

        // ── Accès aux lignes de pixels ─────────────────────────────────────────
        // Retourne un pointeur vers le début de la ligne y (version modifiable)
        inline uint8* RowPtr(uint32 y) noexcept {
            return data + static_cast<size_t>(y) * stride;
        }

        // Retourne un pointeur vers le début de la ligne y (version lecture seule)
        inline const uint8* RowPtr(uint32 y) const noexcept {
            return data + static_cast<size_t>(y) * stride;
        }

        // ── Accès aux pixels individuels ───────────────────────────────────────
        // Définit la couleur d'un pixel avec vérification des bornes
        inline void SetPixel(int32_t x, int32_t y, const math::NkColor& c) noexcept {
            if (static_cast<uint32>(x) >= width || static_cast<uint32>(y) >= height) {
                return;
            }

            uint8* p = RowPtr(static_cast<uint32>(y)) + static_cast<uint32>(x) * 4u;
            p[0] = c.r;
            p[1] = c.g;
            p[2] = c.b;
            p[3] = c.a;
        }

        // Définit la couleur d'un pixel sans vérification des bornes (plus rapide)
        // Attention : l'appelant doit garantir que les coordonnées sont valides
        inline void SetPixelFast(uint32 x, uint32 y, uint8 r, uint8 g, uint8 b, uint8 a = 255) noexcept {
            uint8* p = RowPtr(y) + x * 4u;
            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = a;
        }

        // Récupère la couleur d'un pixel avec vérification des bornes
        inline math::NkColor GetPixel(int32_t x, int32_t y) const noexcept {
            if (static_cast<uint32>(x) >= width || static_cast<uint32>(y) >= height) {
                return math::NkColor(0, 0, 0, 0);
            }

            const uint8* p = RowPtr(static_cast<uint32>(y)) + static_cast<uint32>(x) * 4u;
            return math::NkColor(p[0], p[1], p[2], p[3]);
        }

        // ── Effacement accéléré par SIMD du tampon ─────────────────────────────
        // Remplit tout le tampon avec la couleur spécifiée en utilisant les instructions SIMD disponibles
        void Clear(const math::NkColor& c) noexcept {
            if (!IsValid()) {
                return;
            }

            // Empaquetage de la couleur RGBA dans un entier 32 bits (ordre little-endian)
            uint32 px = static_cast<uint32>(c.r);
            px |= (static_cast<uint32>(c.g) << 8);
            px |= (static_cast<uint32>(c.b) << 16);
            px |= (static_cast<uint32>(c.a) << 24);

            uint32* dst = reinterpret_cast<uint32*>(data);
            size_t totalPx = static_cast<size_t>(width) * height;
            size_t i = 0;

            // Utilisation d'AVX2 si disponible : traite 8 pixels par itération
            #if defined(NK_SIMD_AVX2)
                __m256i v = _mm256_set1_epi32(static_cast<int>(px));

                for (; i + 8 <= totalPx; i += 8) {
                    _mm256_store_si256(reinterpret_cast<__m256i*>(dst + i), v);
                }
            // Fallback sur SSE2 si disponible : traite 4 pixels par itération
            #elif defined(NK_SIMD_SSE2)
                __m128i v = _mm_set1_epi32(static_cast<int>(px));

                for (; i + 4 <= totalPx; i += 4) {
                    _mm_store_si128(reinterpret_cast<__m128i*>(dst + i), v);
                }
            #endif

            // Traitement scalaire des pixels restants
            for (; i < totalPx; ++i) {
                dst[i] = px;
            }
        }

        // ── Copie brute du tampon vers un autre tampon ─────────────────────────
        // Copie le contenu de ce tampon vers un tampon de destination de mêmes dimensions
        void CopyTo(NkPixelBuffer& dst) const noexcept {
            if (!IsValid() || !dst.IsValid()) {
                return;
            }

            if (dst.width != width || dst.height != height) {
                return;
            }

            memcpy(dst.data, data, SizeBytes());
        }
    }; // struct NkPixelBuffer

    // =============================================================================
    // Structure : NkPlatformPresenter
    // Description : Présente le tampon de pixels vers la fenêtre du système d'exploitation.
    //               Windows  : GDI DIBSection + BitBlt (conversion RGBA→BGRA)
    //               Android  : ANativeWindow software blit (RGBA direct)
    //               Web      : Emscripten Canvas 2D putImageData (RGBA direct)
    //               Linux XLib    : XPutImage (conversion RGBA→BGRA)
    //               Linux XCB     : xcb_put_image (conversion RGBA→BGRA)
    //               Linux Wayland : SHM buffer + wl_surface_commit (conversion RGBA→BGRA)
    // =============================================================================
    struct NkPlatformPresenter {

        // ── Membres spécifiques à la plateforme ────────────────────────────────────
#if defined(_WIN32) || defined(NKENTSEU_PLATFORM_WINDOWS)
        HWND    hwnd    = nullptr;
        HDC     hdc     = nullptr;
        HBITMAP dibBmp  = nullptr;
        HDC     dibDC   = nullptr;
        void*   dibBits = nullptr;
        uint32  dibW    = 0;
        uint32  dibH    = 0;

#elif defined(__ANDROID__)
        ANativeWindow* androidWindow = nullptr;
        uint32         androidW      = 0;
        uint32         androidH      = 0;

#elif defined(__EMSCRIPTEN__)
        const char* canvasId = nullptr;
        uint32      webW     = 0;
        uint32      webH     = 0;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
        wl_display* wlDisplay = nullptr;
        wl_surface* wlSurface = nullptr;
        wl_buffer*  wlBuffer  = nullptr;
        void*       wlPixels  = nullptr;
        uint32      wlStride  = 0;
        uint32      wlW       = 0;
        uint32      wlH       = 0;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
        xcb_connection_t* xcbConn   = nullptr;
        xcb_window_t      xcbWindow = 0;
        xcb_gcontext_t    xcbGC     = 0;
        uint8_t*          xcbPixels = nullptr;
        uint8_t           xcbDepth  = 24;
        uint32            xcbW      = 0;
        uint32            xcbH      = 0;

#elif defined(__linux__)
        Display*  xDisplay = nullptr;
        ::Window  xWindow  = 0;
        GC        xGC      = nullptr;
        XImage*   xImage   = nullptr;
        uint8_t*  xPixels  = nullptr;
        uint32    xW       = 0;
        uint32    xH       = 0;
#endif

        // ── Initialisation ─────────────────────────────────────────────────────────
        bool Init(NkWindow& window, uint32 w, uint32 h) {
#if defined(_WIN32) || defined(NKENTSEU_PLATFORM_WINDOWS)
            Destroy();
            hwnd = window.GetSurfaceDesc().hwnd;
            hdc  = GetDC(hwnd);
            dibW = w;
            dibH = h;
            return MakeDIB(w, h);

#elif defined(__ANDROID__)
            Destroy();
            androidWindow = window.GetSurfaceDesc().nativeWindow;
            if (!androidWindow) {
                return false;
            }
            ANativeWindow_acquire(androidWindow);
            if (ANativeWindow_setBuffersGeometry(androidWindow,
                    static_cast<int>(w), static_cast<int>(h),
                    WINDOW_FORMAT_RGBA_8888) != 0) {
                ANativeWindow_release(androidWindow);
                androidWindow = nullptr;
                return false;
            }
            androidW = w;
            androidH = h;
            return true;

#elif defined(__EMSCRIPTEN__)
            Destroy();
            canvasId = window.GetSurfaceDesc().canvasId;
            webW     = w;
            webH     = h;
            if (canvasId) {
                emscripten_set_canvas_element_size(canvasId,
                    static_cast<int>(w), static_cast<int>(h));
            }
            return canvasId != nullptr;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
            Destroy();
            const auto& sd = window.GetSurfaceDesc();
            wlDisplay = sd.display;
            wlSurface = sd.surface;
            wlBuffer  = sd.shmBuffer;
            wlPixels  = sd.shmPixels;
            wlStride  = sd.shmStride;
            wlW       = w;
            wlH       = h;
            return wlDisplay && wlSurface && wlPixels;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
            Destroy();
            const auto& sd = window.GetSurfaceDesc();
            xcbConn   = sd.connection;
            xcbWindow = sd.window;
            xcbDepth  = sd.screen
                      ? static_cast<uint8_t>(sd.screen->root_depth)
                      : uint8_t(24);
            xcbW      = w;
            xcbH      = h;
            xcbPixels = static_cast<uint8_t*>(
                std::malloc(static_cast<size_t>(w) * h * 4u));
            if (!xcbPixels) {
                return false;
            }
            xcbGC = xcb_generate_id(xcbConn);
            {
                uint32_t gcVal = 0u;
                xcb_create_gc(xcbConn, xcbGC, xcbWindow,
                              XCB_GC_FOREGROUND, &gcVal);
            }
            return xcbConn != nullptr;

#elif defined(__linux__)
            Destroy();
            const auto& sd = window.GetSurfaceDesc();
            xDisplay  = sd.display;
            xWindow   = sd.window;
            xW        = w;
            xH        = h;
            xPixels   = static_cast<uint8_t*>(
                std::malloc(static_cast<size_t>(w) * h * 4u));
            if (!xPixels) {
                return false;
            }
            xGC = XCreateGC(xDisplay, xWindow, 0, nullptr);
            {
                Visual* vis = XDefaultVisual(xDisplay, DefaultScreen(xDisplay));
                int     dep = XDefaultDepth(xDisplay, DefaultScreen(xDisplay));
                xImage = XCreateImage(xDisplay, vis,
                                      static_cast<unsigned>(dep),
                                      ZPixmap, 0,
                                      reinterpret_cast<char*>(xPixels),
                                      w, h, 32,
                                      static_cast<int>(w) * 4);
            }
            if (!xImage) {
                std::free(xPixels);
                xPixels = nullptr;
                return false;
            }
            return true;
#else
            (void)window; (void)w; (void)h;
            return false;
#endif
        }

        // ── Destruction ────────────────────────────────────────────────────────────
        void Destroy() {
#if defined(_WIN32) || defined(NKENTSEU_PLATFORM_WINDOWS)
            if (dibDC) {
                DeleteDC(dibDC);
                dibDC = nullptr;
            }
            if (dibBmp) {
                DeleteObject(dibBmp);
                dibBmp = nullptr;
            }
            if (hdc && hwnd) {
                ReleaseDC(hwnd, hdc);
                hdc = nullptr;
            }
            dibBits = nullptr;
            dibW    = 0;
            dibH    = 0;

#elif defined(__ANDROID__)
            if (androidWindow) {
                ANativeWindow_release(androidWindow);
                androidWindow = nullptr;
            }
            androidW = 0;
            androidH = 0;

#elif defined(__EMSCRIPTEN__)
            canvasId = nullptr;
            webW     = 0;
            webH     = 0;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
            wlDisplay = nullptr;
            wlSurface = nullptr;
            wlBuffer  = nullptr;
            wlPixels  = nullptr;
            wlStride  = 0;
            wlW       = 0;
            wlH       = 0;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
            if (xcbGC && xcbConn) {
                xcb_free_gc(xcbConn, xcbGC);
                xcbGC = 0;
            }
            if (xcbPixels) {
                std::free(xcbPixels);
                xcbPixels = nullptr;
            }
            xcbConn   = nullptr;
            xcbWindow = 0;
            xcbDepth  = 24;
            xcbW      = 0;
            xcbH      = 0;

#elif defined(__linux__)
            if (xImage) {
                xImage->data = nullptr; // prevent double-free: we own xPixels separately
                XDestroyImage(xImage);
                xImage = nullptr;
            }
            if (xPixels) {
                std::free(xPixels);
                xPixels = nullptr;
            }
            if (xGC && xDisplay) {
                XFreeGC(xDisplay, xGC);
                xGC = nullptr;
            }
            xDisplay = nullptr;
            xWindow  = 0;
            xW       = 0;
            xH       = 0;
#endif
        }

        // ── Redimensionnement ──────────────────────────────────────────────────────
        bool Resize(uint32 w, uint32 h) {
#if defined(_WIN32) || defined(NKENTSEU_PLATFORM_WINDOWS)
            if (dibDC) {
                DeleteDC(dibDC);
                dibDC = nullptr;
            }
            if (dibBmp) {
                DeleteObject(dibBmp);
                dibBmp = nullptr;
            }
            dibBits = nullptr;
            dibW    = w;
            dibH    = h;
            return MakeDIB(w, h);

#elif defined(__ANDROID__)
            if (!androidWindow) {
                return false;
            }
            ANativeWindow_setBuffersGeometry(androidWindow,
                static_cast<int>(w), static_cast<int>(h),
                WINDOW_FORMAT_RGBA_8888);
            androidW = w;
            androidH = h;
            return true;

#elif defined(__EMSCRIPTEN__)
            webW = w;
            webH = h;
            if (canvasId) {
                emscripten_set_canvas_element_size(canvasId,
                    static_cast<int>(w), static_cast<int>(h));
            }
            return true;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
            wlW = w;
            wlH = h;
            return true;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
            if (xcbPixels) {
                std::free(xcbPixels);
                xcbPixels = nullptr;
            }
            xcbPixels = static_cast<uint8_t*>(
                std::malloc(static_cast<size_t>(w) * h * 4u));
            xcbW = w;
            xcbH = h;
            return xcbPixels != nullptr;

#elif defined(__linux__)
            if (xImage) {
                xImage->data = nullptr;
                XDestroyImage(xImage);
                xImage = nullptr;
            }
            if (xPixels) {
                std::free(xPixels);
                xPixels = nullptr;
            }
            xW      = w;
            xH      = h;
            xPixels = static_cast<uint8_t*>(
                std::malloc(static_cast<size_t>(w) * h * 4u));
            if (!xPixels) {
                return false;
            }
            {
                Visual* vis = XDefaultVisual(xDisplay, DefaultScreen(xDisplay));
                int     dep = XDefaultDepth(xDisplay, DefaultScreen(xDisplay));
                xImage = XCreateImage(xDisplay, vis,
                                      static_cast<unsigned>(dep),
                                      ZPixmap, 0,
                                      reinterpret_cast<char*>(xPixels),
                                      w, h, 32,
                                      static_cast<int>(w) * 4);
            }
            return xImage != nullptr;
#else
            (void)w; (void)h;
            return false;
#endif
        }

        // ── Présentation : affichage du tampon vers la fenêtre ─────────────────────
        void Present(const NkPixelBuffer& buf) noexcept {
#if defined(_WIN32) || defined(NKENTSEU_PLATFORM_WINDOWS)
            if (!dibBits || !dibDC || !hdc || !buf.IsValid()) {
                return;
            }
            {
                size_t        totalPx = static_cast<size_t>(buf.width) * buf.height;
                const uint32* src     = reinterpret_cast<const uint32*>(buf.data);
                uint32*       dst     = reinterpret_cast<uint32*>(dibBits);
                for (size_t i = 0; i < totalPx; ++i) {
                    uint32 p = src[i];
                    dst[i] = (p & 0xFF00FF00u)
                           | ((p & 0x00FF0000u) >> 16)
                           | ((p & 0x000000FFu) << 16);
                }
            }
            BitBlt(hdc, 0, 0,
                   static_cast<int>(buf.width),
                   static_cast<int>(buf.height),
                   dibDC, 0, 0, SRCCOPY);

#elif defined(__ANDROID__)
            if (!androidWindow || !buf.IsValid()) {
                return;
            }
            {
                ANativeWindow_Buffer anb = {};
                if (ANativeWindow_lock(androidWindow, &anb, nullptr) != 0) {
                    return;
                }
                const uint8_t* src      = buf.data;
                uint8_t*       dst      = static_cast<uint8_t*>(anb.bits);
                uint32_t copyW  = static_cast<uint32_t>(anb.width)  < buf.width
                                ? static_cast<uint32_t>(anb.width)  : buf.width;
                uint32_t copyH  = static_cast<uint32_t>(anb.height) < buf.height
                                ? static_cast<uint32_t>(anb.height) : buf.height;
                uint32_t dstRow = static_cast<uint32_t>(anb.stride) * 4u;
                for (uint32_t y = 0; y < copyH; ++y) {
                    std::memcpy(dst + y * dstRow,
                                src + y * buf.stride,
                                copyW * 4u);
                }
                ANativeWindow_unlockAndPost(androidWindow);
            }

#elif defined(__EMSCRIPTEN__)
            if (!canvasId || !buf.IsValid()) {
                return;
            }
            {
                int      w   = static_cast<int>(buf.width);
                int      h   = static_cast<int>(buf.height);
                intptr_t ptr = reinterpret_cast<intptr_t>(buf.data);
                EM_ASM({
                    var canvas = document.querySelector(UTF8ToString($0));
                    if (!canvas) return;
                    var ctx = canvas.getContext('2d');
                    if (!ctx) return;
                    var imgData = new ImageData(
                        new Uint8ClampedArray(HEAPU8.buffer, $3, $1 * $2 * 4),
                        $1, $2);
                    ctx.putImageData(imgData, 0, 0);
                }, canvasId, w, h, static_cast<int>(ptr));
            }

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
            if (!wlPixels || !wlSurface || !wlBuffer || !buf.IsValid()) {
                return;
            }
            {
                size_t        totalPx = static_cast<size_t>(buf.width) * buf.height;
                const uint32* src     = reinterpret_cast<const uint32*>(buf.data);
                uint32*       dst     = static_cast<uint32*>(wlPixels);
                for (size_t i = 0; i < totalPx; ++i) {
                    uint32 p = src[i];
                    dst[i] = (p & 0xFF00FF00u)
                           | ((p & 0x00FF0000u) >> 16)
                           | ((p & 0x000000FFu) << 16);
                }
            }
            wl_surface_attach(wlSurface, wlBuffer, 0, 0);
            wl_surface_damage_buffer(wlSurface, 0, 0,
                                     static_cast<int32_t>(wlW),
                                     static_cast<int32_t>(wlH));
            wl_surface_commit(wlSurface);
            wl_display_flush(wlDisplay);

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
            if (!xcbConn || !xcbPixels || !buf.IsValid()) {
                return;
            }
            {
                size_t        totalPx = static_cast<size_t>(buf.width) * buf.height;
                const uint32* src     = reinterpret_cast<const uint32*>(buf.data);
                uint32*       dst     = reinterpret_cast<uint32*>(xcbPixels);
                for (size_t i = 0; i < totalPx; ++i) {
                    uint32 p = src[i];
                    dst[i] = (p & 0xFF00FF00u)
                           | ((p & 0x00FF0000u) >> 16)
                           | ((p & 0x000000FFu) << 16);
                }
            }
            xcb_put_image(xcbConn,
                          XCB_IMAGE_FORMAT_Z_PIXMAP,
                          xcbWindow, xcbGC,
                          static_cast<uint16_t>(buf.width),
                          static_cast<uint16_t>(buf.height),
                          0, 0, 0, xcbDepth,
                          static_cast<uint32_t>(buf.width * buf.height * 4u),
                          xcbPixels);
            xcb_flush(xcbConn);

#elif defined(__linux__)
            if (!xImage || !xPixels || !buf.IsValid()) {
                return;
            }
            {
                size_t        totalPx = static_cast<size_t>(buf.width) * buf.height;
                const uint32* src     = reinterpret_cast<const uint32*>(buf.data);
                uint32*       dst     = reinterpret_cast<uint32*>(xPixels);
                for (size_t i = 0; i < totalPx; ++i) {
                    uint32 p = src[i];
                    dst[i] = (p & 0xFF00FF00u)
                           | ((p & 0x00FF0000u) >> 16)
                           | ((p & 0x000000FFu) << 16);
                }
            }
            XPutImage(xDisplay, xWindow, xGC, xImage,
                      0, 0, 0, 0,
                      buf.width, buf.height);
            XFlush(xDisplay);
#else
            (void)buf;
#endif
        }

        // ── Validité ───────────────────────────────────────────────────────────────
        bool IsValid() const noexcept {
#if defined(_WIN32) || defined(NKENTSEU_PLATFORM_WINDOWS)
            return hdc     != nullptr
                && dibDC   != nullptr
                && dibBmp  != nullptr
                && dibBits != nullptr;

#elif defined(__ANDROID__)
            return androidWindow != nullptr;

#elif defined(__EMSCRIPTEN__)
            return canvasId != nullptr;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
            return wlDisplay != nullptr
                && wlSurface != nullptr
                && wlPixels  != nullptr;

#elif defined(__linux__) && defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
            return xcbConn   != nullptr
                && xcbPixels != nullptr;

#elif defined(__linux__)
            return xImage  != nullptr
                && xPixels != nullptr;
#else
            return false;
#endif
        }

    private:
#if defined(_WIN32) || defined(NKENTSEU_PLATFORM_WINDOWS)
        bool MakeDIB(uint32 w, uint32 h) {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth       = static_cast<LONG>(w);
            bmi.bmiHeader.biHeight      = -static_cast<LONG>(h);
            bmi.bmiHeader.biPlanes      = 1;
            bmi.bmiHeader.biBitCount    = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            dibBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (!dibBmp) {
                return false;
            }
            dibBits = bits;
            dibDC   = CreateCompatibleDC(hdc);
            SelectObject(dibDC, dibBmp);
            return true;
        }
#endif
    }; // struct NkPlatformPresenter

    // =============================================================================
    // Classe : NkRenderer
    // Description : Moteur de rendu logiciel à double tampon pour éviter le tearing
    // =============================================================================
    // Principe de fonctionnement :
    //   - Frame N   : L'application dessine dans le tampon arrière (mBuffers[mBack])
    //   - Present() : Copie arrière→avant, affiche le tampon avant, puis échange les indices
    // =============================================================================
    class NkRenderer {
        public:
            // Constructeur par défaut
            NkRenderer() = default;

            // Destructeur : nettoie automatiquement les ressources
            ~NkRenderer() {
                Destroy();
            }

            // Suppression du constructeur de copie
            NkRenderer(const NkRenderer&) = delete;

            // Suppression de l'opérateur d'affectation par copie
            NkRenderer& operator=(const NkRenderer&) = delete;

            // ── Initialisation du moteur de rendu ───────────────────────────────────
            // Initialise le renderer avec les dimensions de la fenêtre fournie
            bool Init(NkWindow& window) {
                auto sz = window.GetSize();
                return Init(window, sz.width, sz.height);
            }

            // Initialise le renderer avec des dimensions explicites
            bool Init(NkWindow& window, uint32 w, uint32 h) {
                if (w == 0 || h == 0) {
                    return false;
                }

                if (!mBuffers[0].Allocate(w, h)) {
                    return false;
                }

                if (!mBuffers[1].Allocate(w, h)) {
                    return false;
                }

                if (!mPresenter.Init(window, w, h)) {
                    return false;
                }

                mBack = 0;
                mFront = 1;
                return true;
            }

            // Détruit toutes les ressources allouées par le renderer
            void Destroy() {
                mPresenter.Destroy();
                mBuffers[0].Free();
                mBuffers[1].Free();
            }

            // Redimensionne les tampons et le présentateur pour une nouvelle résolution
            bool Resize(NkWindow& window, uint32 w, uint32 h) {
                if (w == 0 || h == 0) {
                    return false;
                }

                if (!mBuffers[0].Allocate(w, h)) {
                    return false;
                }

                if (!mBuffers[1].Allocate(w, h)) {
                    return false;
                }

                return mPresenter.Resize(w, h);
            }

            // Vérifie si le renderer est correctement initialisé et fonctionnel
            bool IsValid() const noexcept {
                return mBuffers[0].IsValid()
                    && mBuffers[1].IsValid()
                    && mPresenter.IsValid();
            }

            // ── Accès au tampon de rendu courant (arrière) ──────────────────────────
            // Retourne une référence modifiable vers le tampon arrière (celui en cours de dessin)
            NkPixelBuffer& BackBuffer() noexcept {
                return mBuffers[mBack];
            }

            // Retourne une référence en lecture seule vers le tampon arrière
            const NkPixelBuffer& BackBuffer() const noexcept {
                return mBuffers[mBack];
            }

            // Retourne la largeur courante du tampon de rendu
            uint32 Width() const noexcept {
                return mBuffers[mBack].width;
            }

            // Retourne la hauteur courante du tampon de rendu
            uint32 Height() const noexcept {
                return mBuffers[mBack].height;
            }

            // Retourne les dimensions du tampon sous forme de vecteur 2D
            math::NkVec2u Size() const noexcept {
                return { Width(), Height() };
            }

            // ── Fonctions de dessin déléguées au tampon arrière ─────────────────────
            // Efface tout le tampon arrière avec la couleur spécifiée
            inline void Clear(const math::NkColor& c) {
                BackBuffer().Clear(c);
            }

            // Définit la couleur d'un pixel dans le tampon arrière (avec vérification des bornes)
            inline void SetPixel(int32_t x, int32_t y, const math::NkColor& c) noexcept {
                BackBuffer().SetPixel(x, y, c);
            }

            // Récupère la couleur d'un pixel depuis le tampon arrière
            inline math::NkColor GetPixel(int32_t x, int32_t y) const noexcept {
                return BackBuffer().GetPixel(x, y);
            }

            // ── Présentation finale : affichage à l'écran ───────────────────────────
            // Copie le tampon arrière vers l'avant, l'affiche, puis échange les rôles
            void Present() {
                // Étape 1 : Copie complète du tampon arrière vers le tampon avant
                mBuffers[mBack].CopyTo(mBuffers[mFront]);

                // Étape 2 : Envoi du tampon avant vers la fenêtre système (avec conversion RGBA→BGRA)
                mPresenter.Present(mBuffers[mFront]);

                // Étape 3 : Échange des indices pour que le prochain frame dessine dans l'ancien avant
                std::swap(mBack, mFront);
            }

        private:
            NkPixelBuffer       mBuffers[2];   // Tableau des deux tampons (avant/arrière)
            int                 mBack;         // Index du tampon en cours de dessin (arrière)
            int                 mFront;        // Index du tampon en cours d'affichage (avant)
            NkPlatformPresenter mPresenter;    // Composant responsable de l'affichage système
    }; // class NkRenderer

} // namespace nkentseu


// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================

/*
 * ── Exemple 1 : Initialisation basique du renderer ───────────────────────────
 *
 * // Dans votre fonction principale ou d'initialisation
 * nkentseu::NkWindow window;
 * // ... configuration de la fenêtre ...
 *
 * nkentseu::NkRenderer renderer;
 *
 * if (!renderer.Init(window))
 * {
 *     // Gestion de l'erreur d'initialisation
 *     return -1;
 * }
 *
 * // Boucle de rendu principale
 * while (window.IsOpen())
 * {
 *     // Effacer l'écran en noir
 *     renderer.Clear(math::NkColor(0, 0, 0, 255));
 *
 *     // Dessiner un pixel rouge au centre de l'écran
 *     uint32 centerX = renderer.Width() / 2;
 *     uint32 centerY = renderer.Height() / 2;
 *     renderer.SetPixel(centerX, centerY, math::NkColor(255, 0, 0, 255));
 *
 *     // Afficher le frame à l'écran
 *     renderer.Present();
 *
 *     // Gestion des événements...
 * }
 *
 * // Nettoyage automatique via le destructeur
 */


/*
 * ── Exemple 2 : Dessin d'un rectangle rempli ─────────────────────────────────
 *
 * // Fonction utilitaire pour dessiner un rectangle rempli
 * void FillRect(nkentseu::NkRenderer& renderer,
 *               int32_t x, int32_t y,
 *               uint32_t width, uint32_t height,
 *               const math::NkColor& color)
 * {
 *     for (uint32_t dy = 0; dy < height; ++dy)
 *     {
 *         for (uint32_t dx = 0; dx < width; ++dx)
 *         {
 *             renderer.SetPixel(x + dx, y + dy, color);
 *         }
 *     }
 * }
 *
 * // Utilisation dans la boucle de rendu
 * FillRect(renderer, 10, 10, 100, 50, math::NkColor(0, 255, 0, 255)); // Rectangle vert
 */


/*
 * ── Exemple 3 : Accès direct au tampon pour des opérations avancées ──────────
 *
 * // Pour des performances maximales, accédez directement au tampon arrière
 * nkentseu::NkPixelBuffer& backBuffer = renderer.BackBuffer();
 *
 * // Dessin rapide sans vérification des bornes (à utiliser avec précaution)
 * for (uint32_t y = 0; y < renderer.Height(); ++y)
 * {
 *     for (uint32_t x = 0; x < renderer.Width(); ++x)
 *     {
 *         // Calcul d'un dégradé horizontal
 *         uint8_t intensity = static_cast<uint8_t>((x * 255) / renderer.Width());
 *         backBuffer.SetPixelFast(x, y, intensity, 0, 255 - intensity, 255);
 *     }
 * }
 *
 * // N'oubliez pas d'appeler Present() pour afficher le résultat
 * renderer.Present();
 */


/*
 * ── Exemple 4 : Gestion du redimensionnement de fenêtre ──────────────────────
 *
 * // Dans votre gestionnaire d'événements de redimensionnement
 * void OnWindowResize(nkentseu::NkWindow& window,
 *                     nkentseu::NkRenderer& renderer,
 *                     uint32 newWidth, uint32 newHeight)
 * {
 *     if (!renderer.Resize(window, newWidth, newHeight))
 *     {
 *         // Gestion de l'erreur de redimensionnement
 *         // Le renderer peut être dans un état invalide
 *     }
 * }
 */


/*
 * ── Exemple 5 : Animation simple avec double buffering ───────────────────────
 *
 * // Variable d'état pour l'animation
 * float animationTime = 0.0f;
 *
 * while (window.IsOpen())
 * {
 *     // Effacer avec un fond dégradé bleu
 *     renderer.Clear(math::NkColor(20, 40, 80, 255));
 *
 *     // Calculer la position d'un cercle animé
 *     uint32_t radius = 30;
 *     uint32_t centerX = static_cast<uint32>(
 *         (renderer.Width() / 2) + cosf(animationTime) * 100
 *     );
 *     uint32_t centerY = static_cast<uint32>(
 *         (renderer.Height() / 2) + sinf(animationTime * 0.7f) * 80
 *     );
 *
 *     // Dessiner un cercle approximatif (algorithme naïf)
 *     for (int32_t dy = -radius; dy <= static_cast<int32_t>(radius); ++dy)
 *     {
 *         for (int32_t dx = -radius; dx <= static_cast<int32_t>(radius); ++dx)
 *         {
 *             if (dx*dx + dy*dy <= radius*radius)
 *             {
 *                 renderer.SetPixel(
 *                     centerX + dx,
 *                     centerY + dy,
 *                     math::NkColor(255, 200, 0, 255) // Jaune
 *                 );
 *             }
 *         }
 *     }
 *
 *     // Afficher le frame
 *     renderer.Present();
 *
 *     // Mettre à jour le temps d'animation (60 FPS approximatif)
 *     animationTime += 0.016f;
 *
 *     // Gestion des événements et contrôle du framerate...
 * }
 */


/*
 * ── Bonnes pratiques et recommandations ─────────────────────────────────────
 *
 * 1. Toujours vérifier IsValid() après Init() avant d'utiliser le renderer
 * 2. Appeler Present() une seule fois par frame pour éviter le tearing
 * 3. Utiliser SetPixelFast() uniquement quand les coordonnées sont garanties valides
 * 4. Pour des performances optimales, minimiser les appels à SetPixel en boucle
 *    et privilégier les opérations par blocs ou l'accès direct au tampon
 * 5. Le double buffering élimine le tearing mais double l'utilisation mémoire
 * 6. Sur Windows, la conversion RGBA→BGRA est nécessaire pour la compatibilité DIB
 *
 * ── Limitations connues ─────────────────────────────────────────────────────
 *
 * • Rendu logiciel uniquement : performances limitées par le CPU
 * • Support Windows uniquement via NkPlatformPresenter
 * • Pas de support de la transparence alpha compositing dans ce module
 * • Format de pixel fixe : RGBA8 (32 bits par pixel)
 *
 * ── Extensions possibles ────────────────────────────────────────────────────
 *
 * • Ajouter un support OpenGL/Vulkan pour un rendu matériel
 * • Implémenter des primitives de dessin optimisées (lignes, triangles)
 * • Ajouter un système de sprites et de tiles pour les jeux 2D
 * • Implémenter le blending alpha pour les effets de transparence
 */