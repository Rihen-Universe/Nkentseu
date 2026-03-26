#pragma once

// =============================================================================
// NkGraphicsEvent.h
// Hiérarchie des événements liés au rendu graphique et au contexte GPU.
//
// Catégorie : NK_CAT_GRAPHICS
//
// Enumerations :
//   NkGraphicsApi       — API graphique active (OpenGL, Vulkan, D3D12...)
//   NkGpuMemoryLevel    — niveau de pression mémoire GPU
//
// Classes :
//   NkGraphicsEvent               — base abstraite (catégorie GRAPHICS)
//     NkGraphicsContextReadyEvent — contexte GPU initialisé et prêt
//     NkGraphicsContextLostEvent  — contexte GPU perdu (device lost)
//     NkGraphicsContextResizeEvent— framebuffer redimensionné
//     NkGraphicsFrameBeginEvent   — début d'une frame GPU
//     NkGraphicsFrameEndEvent     — fin d'une frame GPU (présentation)
//     NkGraphicsGpuMemoryEvent    — pression mémoire GPU (mobile / intégré)
//     NkGraphicsVSyncEvent        — signal de synchronisation verticale
// =============================================================================

#include "NkEvent.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKCore/NkTraits.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    // NkGraphicsApi — API graphique active
    // =========================================================================

    enum class NkGraphicsApi : uint32 {
        // Canonical names used by NKWindow events.
        NK_GFX_API_NONE    = 0,
        NK_GFX_API_OPENGL,       ///< OpenGL 3.3+
        NK_GFX_API_OPENGLES,     ///< OpenGL ES 2.0 / 3.x (mobile)
        NK_GFX_API_VULKAN,       ///< Vulkan 1.0+
        NK_GFX_API_D3D11,        ///< Direct3D 11
        NK_GFX_API_D3D12,        ///< Direct3D 12
        NK_GFX_API_METAL,        ///< Metal (macOS / iOS)
        NK_GFX_API_WEBGL,        ///< WebGL (navigateur / WASM)
        NK_GFX_API_WEBGPU,       ///< WebGPU (navigateur / WASM)
        NK_GFX_API_SOFTWARE,     ///< Rasterisation logicielle (fallback)
        NK_GFX_API_MAX,

        // Compatibility aliases used by NKRenderer.
        NK_API_NONE       = NK_GFX_API_NONE,
        NK_API_OPENGL     = NK_GFX_API_OPENGL,
        NK_API_OPENGLES   = NK_GFX_API_OPENGLES,
        NK_API_VULKAN     = NK_GFX_API_VULKAN,
        NK_API_DIRECTX11  = NK_GFX_API_D3D11,
        NK_API_DIRECTX12  = NK_GFX_API_D3D12,
        NK_API_METAL      = NK_GFX_API_METAL,
        NK_API_WEBGL      = NK_GFX_API_WEBGL,
        NK_API_SOFTWARE   = NK_GFX_API_SOFTWARE,
        NK_API_COUNT      = NK_GFX_API_MAX
    };

    inline const char* NkGraphicsApiToString(NkGraphicsApi api) noexcept {
        switch (api) {
            case NkGraphicsApi::NK_GFX_API_OPENGL:   return "OpenGL";
            case NkGraphicsApi::NK_GFX_API_OPENGLES: return "OpenGLES";
            case NkGraphicsApi::NK_GFX_API_VULKAN:   return "Vulkan";
            case NkGraphicsApi::NK_GFX_API_D3D11:    return "D3D11";
            case NkGraphicsApi::NK_GFX_API_D3D12:    return "D3D12";
            case NkGraphicsApi::NK_GFX_API_METAL:    return "Metal";
            case NkGraphicsApi::NK_GFX_API_WEBGL:    return "WebGL";
            case NkGraphicsApi::NK_GFX_API_WEBGPU:   return "WebGPU";
            case NkGraphicsApi::NK_GFX_API_SOFTWARE: return "Software";
            default:                                   return "None";
        }
    }

    // =========================================================================
    // NkGpuMemoryLevel — niveau de pression mémoire GPU
    // =========================================================================

    enum class NkGpuMemoryLevel : uint32 {
        NK_GPU_MEM_NORMAL   = 0, ///< Mémoire suffisante
        NK_GPU_MEM_LOW,          ///< Avertissement : libérer les caches
        NK_GPU_MEM_CRITICAL,     ///< Critique : libérer toutes les ressources non essentielles
        NK_GPU_MEM_MAX
    };

    // =========================================================================
    // NkGraphicsEvent — base abstraite pour tous les événements graphiques
    // =========================================================================

    /**
     * @brief Classe de base pour les événements liés au rendu graphique.
     *
     * Catégorie : NK_CAT_GRAPHICS
     */
    class NkGraphicsEvent : public NkEvent {
    public:
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_GRAPHICS)

    protected:
        explicit NkGraphicsEvent(uint64 windowId = 0) noexcept : NkEvent(windowId) {}
    };

    // =========================================================================
    // NkGraphicsContextReadyEvent — contexte GPU initialisé et prêt
    // =========================================================================

    /**
     * @brief Émis lorsque le contexte graphique (OpenGL, Vulkan, D3D...) vient
     *        d'être créé avec succès et est prêt pour le rendu.
     *
     * C'est ici que l'application doit charger ses ressources GPU (shaders,
     * textures, buffers, pipelines...).
     */
    class NkGraphicsContextReadyEvent final : public NkGraphicsEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_RENDER)   // utilise NK_APP_RENDER comme proxy

        /**
         * @param api         API graphique utilisée.
         * @param width       Largeur initiale du framebuffer [pixels].
         * @param height      Hauteur initiale du framebuffer [pixels].
         * @param windowId    Identifiant de la fenêtre associée.
         */
        NkGraphicsContextReadyEvent(NkGraphicsApi api,
                                     uint32 width,
                                     uint32 height,
                                     uint64 windowId = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mApi(api)
            , mWidth(width)
            , mHeight(height)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsContextReadyEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GfxContextReady({0} {1}x{2})",
                NkGraphicsApiToString(mApi), mWidth, mHeight);
        }

        NkGraphicsApi GetApi()    const noexcept { return mApi; }
        uint32         GetWidth()  const noexcept { return mWidth; }
        uint32         GetHeight() const noexcept { return mHeight; }

    private:
        NkGraphicsApi mApi    = NkGraphicsApi::NK_GFX_API_NONE;
        uint32         mWidth  = 0;
        uint32         mHeight = 0;
    };

    // =========================================================================
    // NkGraphicsContextLostEvent — contexte GPU perdu
    // =========================================================================

    /**
     * @brief Émis lorsque le contexte graphique est perdu de façon inattendue
     *        (device removed / lost, mise en veille GPU, changement d'adaptateur).
     *
     * L'application doit libérer toutes ses ressources GPU et attendre un
     * NkGraphicsContextReadyEvent pour les recréer.
     */
    class NkGraphicsContextLostEvent final : public NkGraphicsEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_CLOSE)  // proxy

        /**
         * @param reason   Description textuelle de la cause (ex: "VK_ERROR_DEVICE_LOST").
         * @param windowId Identifiant de la fenêtre concernée.
         */
        NkGraphicsContextLostEvent(NkString reason   = {},
                                    uint64       windowId = 0)
            : NkGraphicsEvent(windowId)
            , mReason(traits::NkMove(reason))
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsContextLostEvent(*this); }
        NkString ToString() const override {
            return "GfxContextLost(" + (mReason.Empty() ? "unknown" : mReason) + ")";
        }

        const NkString& GetReason() const noexcept { return mReason; }

    private:
        NkString mReason;
    };

    // =========================================================================
    // NkGraphicsContextResizeEvent — framebuffer redimensionné
    // =========================================================================

    /**
     * @brief Émis lorsque le framebuffer de rendu change de dimensions.
     *
     * Cela arrive lors du redimensionnement de la fenêtre, d'un changement de
     * DPI, ou d'une transition plein-écran / fenêtré.
     * L'application doit recréer ses ressources dépendantes de la taille
     * (swapchain, render targets, viewports...).
     */
    class NkGraphicsContextResizeEvent final : public NkGraphicsEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_UPDATE)  // proxy

        /**
         * @param width     Nouvelle largeur du framebuffer [pixels].
         * @param height    Nouvelle hauteur du framebuffer [pixels].
         * @param prevW     Largeur précédente.
         * @param prevH     Hauteur précédente.
         * @param windowId  Identifiant de la fenêtre concernée.
         */
        NkGraphicsContextResizeEvent(uint32 width,
                                      uint32 height,
                                      uint32 prevW    = 0,
                                      uint32 prevH    = 0,
                                      uint64 windowId = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mWidth(width)
            , mHeight(height)
            , mPrevWidth(prevW)
            , mPrevHeight(prevH)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsContextResizeEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GfxContextResize({0}x{1} -> {2}x{3})",
                mPrevWidth, mPrevHeight, mWidth, mHeight);
        }

        uint32 GetWidth()      const noexcept { return mWidth; }
        uint32 GetHeight()     const noexcept { return mHeight; }
        uint32 GetPrevWidth()  const noexcept { return mPrevWidth; }
        uint32 GetPrevHeight() const noexcept { return mPrevHeight; }

        float GetAspectRatio() const noexcept {
            return (mHeight > 0) ? static_cast<float>(mWidth) / static_cast<float>(mHeight) : 0.f;
        }

    private:
        uint32 mWidth      = 0;
        uint32 mHeight     = 0;
        uint32 mPrevWidth  = 0;
        uint32 mPrevHeight = 0;
    };

    // =========================================================================
    // NkGraphicsFrameBeginEvent — début d'une frame GPU
    // =========================================================================

    /**
     * @brief Émis au début de chaque frame de rendu, avant l'enregistrement
     *        des commandes GPU.
     *
     * Utile pour réinitialiser les compteurs de performance, synchroniser les
     * ressources dynamiques (uniform buffers, staging...).
     */
    class NkGraphicsFrameBeginEvent final : public NkGraphicsEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_TICK)  // proxy

        /**
         * @param frameIndex    Index de la frame courante (depuis le lancement).
         * @param frameInFlight Index de la ressource en vol (0..N-1, double/triple buffering).
         * @param windowId      Identifiant de la fenêtre.
         */
        NkGraphicsFrameBeginEvent(uint64 frameIndex    = 0,
                                   uint32 frameInFlight = 0,
                                   uint64 windowId     = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mFrameIndex(frameIndex)
            , mFrameInFlight(frameInFlight)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsFrameBeginEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GfxFrameBegin(frame={0} inFlight={1})",
                mFrameIndex, mFrameInFlight);
        }

        uint64 GetFrameIndex()    const noexcept { return mFrameIndex; }
        uint32 GetFrameInFlight() const noexcept { return mFrameInFlight; }

    private:
        uint64 mFrameIndex    = 0;
        uint32 mFrameInFlight = 0;
    };

    // =========================================================================
    // NkGraphicsFrameEndEvent — fin d'une frame GPU (présentation)
    // =========================================================================

    /**
     * @brief Émis après la présentation (swapchain present) d'une frame.
     *
     * Transporte des métriques de performance comme le temps GPU effectif,
     * utiles pour le profiling et la régulation du framerate.
     */
    class NkGraphicsFrameEndEvent final : public NkGraphicsEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_LAUNCH)  // proxy

        /**
         * @param frameIndex    Index de la frame qui vient d'être présentée.
         * @param gpuTimeMs     Temps GPU mesuré pour cette frame [ms] (0 = inconnu).
         * @param cpuTimeMs     Temps CPU d'enregistrement des commandes [ms].
         * @param windowId      Identifiant de la fenêtre.
         */
        NkGraphicsFrameEndEvent(uint64  frameIndex = 0,
                                 float64  gpuTimeMs  = 0.0,
                                 float64  cpuTimeMs  = 0.0,
                                 uint64  windowId   = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mFrameIndex(frameIndex)
            , mGpuTimeMs(gpuTimeMs)
            , mCpuTimeMs(cpuTimeMs)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsFrameEndEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GfxFrameEnd(frame={0} GPU={1:.3}ms CPU={2:.3}ms)",
                mFrameIndex, mGpuTimeMs, mCpuTimeMs);
        }

        uint64 GetFrameIndex() const noexcept { return mFrameIndex; }
        float64 GetGpuTimeMs()  const noexcept { return mGpuTimeMs; }
        float64 GetCpuTimeMs()  const noexcept { return mCpuTimeMs; }

    private:
        uint64 mFrameIndex = 0;
        float64 mGpuTimeMs  = 0.0;
        float64 mCpuTimeMs  = 0.0;
    };

    // =========================================================================
    // NkGraphicsGpuMemoryEvent — pression mémoire GPU
    // =========================================================================

    /**
     * @brief Émis lorsque le pilote signale une pression sur la mémoire GPU.
     *
     * Particulièrement important sur mobile (GPU intégrés avec mémoire partagée)
     * et sur les GPU bas de gamme. L'application doit libérer les caches et les
     * ressources à basse priorité en réponse à cet événement.
     */
    class NkGraphicsGpuMemoryEvent final : public NkGraphicsEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_TICK)  // proxy (NK_SYSTEM_MEMORY est dans NK_CAT_SYSTEM)

        /**
         * @param level           Niveau de pression mémoire.
         * @param availableMb     Mémoire GPU disponible [Mo] (0 = inconnu).
         * @param totalMb         Mémoire GPU totale [Mo] (0 = inconnu).
         * @param windowId        Identifiant de la fenêtre.
         */
        NkGraphicsGpuMemoryEvent(NkGpuMemoryLevel level,
                                  uint32 availableMb = 0,
                                  uint32 totalMb     = 0,
                                  uint64 windowId    = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mLevel(level)
            , mAvailableMb(availableMb)
            , mTotalMb(totalMb)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsGpuMemoryEvent(*this); }
        NkString ToString() const override {
            const char* lvls[] = {"NORMAL", "LOW", "CRITICAL"};
            uint32 idx = static_cast<uint32>(mLevel);
            return NkString::Fmt("GfxGpuMemory({0} avail={1}MB)",
                (idx < 3 ? lvls[idx] : "?"), mAvailableMb);
        }

        NkGpuMemoryLevel GetLevel()       const noexcept { return mLevel; }
        uint32            GetAvailableMb() const noexcept { return mAvailableMb; }
        uint32            GetTotalMb()     const noexcept { return mTotalMb; }
        bool             IsCritical()     const noexcept { return mLevel == NkGpuMemoryLevel::NK_GPU_MEM_CRITICAL; }

    private:
        NkGpuMemoryLevel mLevel       = NkGpuMemoryLevel::NK_GPU_MEM_NORMAL;
        uint32            mAvailableMb = 0;
        uint32            mTotalMb     = 0;
    };

    // =========================================================================
    // NkGraphicsVSyncEvent — signal de synchronisation verticale
    // =========================================================================

    /**
     * @brief Émis lorsque le signal VSync est reçu du moniteur.
     *
     * Peut être utilisé pour synchroniser la logique de jeu sur le taux de
     * rafraîchissement de l'écran sans dépendre du timer OS.
     *
     * @note  Tous les backends graphiques ne supportent pas cet événement.
     */
    class NkGraphicsVSyncEvent final : public NkGraphicsEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_APP_RENDER)  // proxy

        /**
         * @param displayIndex     Index du moniteur émettant le VSync.
         * @param refreshRateHz    Taux de rafraîchissement du moniteur [Hz].
         * @param windowId         Identifiant de la fenêtre.
         */
        NkGraphicsVSyncEvent(uint32 displayIndex  = 0,
                              uint32 refreshRateHz = 60,
                              uint64 windowId      = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mDisplayIndex(displayIndex)
            , mRefreshRateHz(refreshRateHz)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsVSyncEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GfxVSync(display={0} {1}Hz)", mDisplayIndex, mRefreshRateHz);
        }

        uint32 GetDisplayIndex()  const noexcept { return mDisplayIndex; }
        uint32 GetRefreshRateHz() const noexcept { return mRefreshRateHz; }

    private:
        uint32 mDisplayIndex  = 0;
        uint32 mRefreshRateHz = 60;
    };

} // namespace nkentseu
