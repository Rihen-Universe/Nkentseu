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
#include <string>

namespace nkentseu {

    // =========================================================================
    // NkGraphicsApi — API graphique active
    // =========================================================================

    enum class NkGraphicsApi : NkU32 {
        NK_GFX_API_NONE    = 0,
        NK_GFX_API_OPENGL,       ///< OpenGL 3.3+
        NK_GFX_API_OPENGLES,     ///< OpenGL ES 2.0 / 3.x (mobile)
        NK_GFX_API_VULKAN,       ///< Vulkan 1.0+
        NK_GFX_API_D3D11,        ///< Direct3D 11
        NK_GFX_API_D3D12,        ///< Direct3D 12
        NK_GFX_API_METAL,        ///< Metal (macOS / iOS)
        NK_GFX_API_WEBGPU,       ///< WebGPU (navigateur / WASM)
        NK_GFX_API_SOFTWARE,     ///< Rasterisation logicielle (fallback)
        NK_GFX_API_MAX
    };

    inline const char* NkGraphicsApiToString(NkGraphicsApi api) noexcept {
        switch (api) {
            case NkGraphicsApi::NK_GFX_API_OPENGL:   return "OpenGL";
            case NkGraphicsApi::NK_GFX_API_OPENGLES: return "OpenGLES";
            case NkGraphicsApi::NK_GFX_API_VULKAN:   return "Vulkan";
            case NkGraphicsApi::NK_GFX_API_D3D11:    return "D3D11";
            case NkGraphicsApi::NK_GFX_API_D3D12:    return "D3D12";
            case NkGraphicsApi::NK_GFX_API_METAL:    return "Metal";
            case NkGraphicsApi::NK_GFX_API_WEBGPU:   return "WebGPU";
            case NkGraphicsApi::NK_GFX_API_SOFTWARE: return "Software";
            default:                                   return "None";
        }
    }

    // =========================================================================
    // NkGpuMemoryLevel — niveau de pression mémoire GPU
    // =========================================================================

    enum class NkGpuMemoryLevel : NkU32 {
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
        explicit NkGraphicsEvent(NkU64 windowId = 0) noexcept : NkEvent(windowId) {}
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
                                     NkU32 width,
                                     NkU32 height,
                                     NkU64 windowId = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mApi(api)
            , mWidth(width)
            , mHeight(height)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsContextReadyEvent(*this); }
        std::string ToString() const override {
            return std::string("GfxContextReady(") + NkGraphicsApiToString(mApi)
                 + " " + std::to_string(mWidth) + "x" + std::to_string(mHeight) + ")";
        }

        NkGraphicsApi GetApi()    const noexcept { return mApi; }
        NkU32         GetWidth()  const noexcept { return mWidth; }
        NkU32         GetHeight() const noexcept { return mHeight; }

    private:
        NkGraphicsApi mApi    = NkGraphicsApi::NK_GFX_API_NONE;
        NkU32         mWidth  = 0;
        NkU32         mHeight = 0;
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
        NkGraphicsContextLostEvent(std::string reason   = {},
                                    NkU64       windowId = 0)
            : NkGraphicsEvent(windowId)
            , mReason(std::move(reason))
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsContextLostEvent(*this); }
        std::string ToString() const override {
            return "GfxContextLost(" + (mReason.empty() ? "unknown" : mReason) + ")";
        }

        const std::string& GetReason() const noexcept { return mReason; }

    private:
        std::string mReason;
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
        NkGraphicsContextResizeEvent(NkU32 width,
                                      NkU32 height,
                                      NkU32 prevW    = 0,
                                      NkU32 prevH    = 0,
                                      NkU64 windowId = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mWidth(width)
            , mHeight(height)
            , mPrevWidth(prevW)
            , mPrevHeight(prevH)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsContextResizeEvent(*this); }
        std::string ToString() const override {
            return "GfxContextResize(" + std::to_string(mPrevWidth) + "x"
                 + std::to_string(mPrevHeight) + " -> "
                 + std::to_string(mWidth) + "x" + std::to_string(mHeight) + ")";
        }

        NkU32 GetWidth()      const noexcept { return mWidth; }
        NkU32 GetHeight()     const noexcept { return mHeight; }
        NkU32 GetPrevWidth()  const noexcept { return mPrevWidth; }
        NkU32 GetPrevHeight() const noexcept { return mPrevHeight; }

        float GetAspectRatio() const noexcept {
            return (mHeight > 0) ? static_cast<float>(mWidth) / static_cast<float>(mHeight) : 0.f;
        }

    private:
        NkU32 mWidth      = 0;
        NkU32 mHeight     = 0;
        NkU32 mPrevWidth  = 0;
        NkU32 mPrevHeight = 0;
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
        NkGraphicsFrameBeginEvent(NkU64 frameIndex    = 0,
                                   NkU32 frameInFlight = 0,
                                   NkU64 windowId     = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mFrameIndex(frameIndex)
            , mFrameInFlight(frameInFlight)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsFrameBeginEvent(*this); }
        std::string ToString() const override {
            return "GfxFrameBegin(frame=" + std::to_string(mFrameIndex)
                 + " inFlight=" + std::to_string(mFrameInFlight) + ")";
        }

        NkU64 GetFrameIndex()    const noexcept { return mFrameIndex; }
        NkU32 GetFrameInFlight() const noexcept { return mFrameInFlight; }

    private:
        NkU64 mFrameIndex    = 0;
        NkU32 mFrameInFlight = 0;
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
        NkGraphicsFrameEndEvent(NkU64  frameIndex = 0,
                                 NkF64  gpuTimeMs  = 0.0,
                                 NkF64  cpuTimeMs  = 0.0,
                                 NkU64  windowId   = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mFrameIndex(frameIndex)
            , mGpuTimeMs(gpuTimeMs)
            , mCpuTimeMs(cpuTimeMs)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsFrameEndEvent(*this); }
        std::string ToString() const override {
            return "GfxFrameEnd(frame=" + std::to_string(mFrameIndex)
                 + " GPU=" + std::to_string(mGpuTimeMs) + "ms"
                 + " CPU=" + std::to_string(mCpuTimeMs) + "ms)";
        }

        NkU64 GetFrameIndex() const noexcept { return mFrameIndex; }
        NkF64 GetGpuTimeMs()  const noexcept { return mGpuTimeMs; }
        NkF64 GetCpuTimeMs()  const noexcept { return mCpuTimeMs; }

    private:
        NkU64 mFrameIndex = 0;
        NkF64 mGpuTimeMs  = 0.0;
        NkF64 mCpuTimeMs  = 0.0;
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
                                  NkU32 availableMb = 0,
                                  NkU32 totalMb     = 0,
                                  NkU64 windowId    = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mLevel(level)
            , mAvailableMb(availableMb)
            , mTotalMb(totalMb)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsGpuMemoryEvent(*this); }
        std::string ToString() const override {
            const char* lvls[] = {"NORMAL", "LOW", "CRITICAL"};
            NkU32 idx = static_cast<NkU32>(mLevel);
            return std::string("GfxGpuMemory(") + (idx < 3 ? lvls[idx] : "?")
                 + " avail=" + std::to_string(mAvailableMb) + "MB)";
        }

        NkGpuMemoryLevel GetLevel()       const noexcept { return mLevel; }
        NkU32            GetAvailableMb() const noexcept { return mAvailableMb; }
        NkU32            GetTotalMb()     const noexcept { return mTotalMb; }
        bool             IsCritical()     const noexcept { return mLevel == NkGpuMemoryLevel::NK_GPU_MEM_CRITICAL; }

    private:
        NkGpuMemoryLevel mLevel       = NkGpuMemoryLevel::NK_GPU_MEM_NORMAL;
        NkU32            mAvailableMb = 0;
        NkU32            mTotalMb     = 0;
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
        NkGraphicsVSyncEvent(NkU32 displayIndex  = 0,
                              NkU32 refreshRateHz = 60,
                              NkU64 windowId      = 0) noexcept
            : NkGraphicsEvent(windowId)
            , mDisplayIndex(displayIndex)
            , mRefreshRateHz(refreshRateHz)
        {}

        NkEvent*    Clone()    const override { return new NkGraphicsVSyncEvent(*this); }
        std::string ToString() const override {
            return "GfxVSync(display=" + std::to_string(mDisplayIndex)
                 + " " + std::to_string(mRefreshRateHz) + "Hz)";
        }

        NkU32 GetDisplayIndex()  const noexcept { return mDisplayIndex; }
        NkU32 GetRefreshRateHz() const noexcept { return mRefreshRateHz; }

    private:
        NkU32 mDisplayIndex  = 0;
        NkU32 mRefreshRateHz = 60;
    };

} // namespace nkentseu