#pragma once

// =============================================================================
// Fichier : NkGraphicsEvent.h
// =============================================================================
// Description :
//   Définition de la hiérarchie des événements liés au rendu graphique et 
//   à la gestion du contexte GPU au sein du moteur NkEntseu.
//
// Catégorie : NK_CAT_GRAPHICS
//
// Responsabilités :
//   - Fournir des types d'événements fortement typés pour le cycle de vie 
//     graphique (création, perte, redimensionnement du contexte).
//   - Exposer des métriques de performance GPU/CPU pour le profiling.
//   - Gérer les signaux système liés à la mémoire GPU et à la synchronisation.
//
// Enumerations définies :
//   - NkGraphicsApi    : API graphique active (OpenGL, Vulkan, D3D12, etc.)
//   - NkGpuMemoryLevel : Niveau de pression sur la mémoire vidéo
//
// Classes définies :
//   - NkGraphicsEvent               : Classe de base abstraite
//   - NkGraphicsContextReadyEvent   : Contexte GPU initialisé avec succès
//   - NkGraphicsContextLostEvent    : Perte inattendue du contexte GPU
//   - NkGraphicsContextResizeEvent  : Changement de dimensions du framebuffer
//   - NkGraphicsFrameBeginEvent     : Début d'une frame de rendu
//   - NkGraphicsFrameEndEvent       : Fin d'une frame avec métriques
//   - NkGraphicsGpuMemoryEvent      : Alerte pression mémoire GPU
//   - NkGraphicsVSyncEvent          : Signal de synchronisation verticale
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

#ifndef NKENTSEU_EVENT_NKGRAPHICSEVENT_H
#define NKENTSEU_EVENT_NKGRAPHICSEVENT_H


    #include "NkEvent.h"
    #include "NKContainers/String/NkStringUtils.h"
    #include "NKCore/NkTraits.h"
    #include "NKCore/NkTypes.h"
    #include "NKCore/NkCGXDetect.h"

    namespace nkentseu {
        // =========================================================================
        // Section : Définition des types et énumérations graphiques
        // =========================================================================

        // -------------------------------------------------------------------------
        // Type : NkGraphicsApi
        // -------------------------------------------------------------------------
        // Description :
        //   Alias vers le type d'API graphique défini dans le module platform.
        //   Permet d'identifier le backend de rendu actif à l'exécution.
        // -------------------------------------------------------------------------
        using NkGraphicsApi = platform::graphics::NkGraphicsApi;

        // -------------------------------------------------------------------------
        // Fonction : NkGraphicsApiToString
        // -------------------------------------------------------------------------
        // Description :
        //   Convertit une valeur NkGraphicsApi en chaîne de caractères lisible.
        // Paramètres :
        //   - api : Valeur de l'énumération NkGraphicsApi à convertir.
        // Retour :
        //   Pointeur vers une chaîne statique décrivant l'API graphique.
        // Notes :
        //   - Fonction noexcept : garantie de ne pas lever d'exception.
        //   - Retourne "None" pour toute valeur non reconnue (fallback sécurisé).
        // -------------------------------------------------------------------------
        inline const char* NkGraphicsApiToString(NkGraphicsApi api) noexcept {
            switch (api) {
                case NkGraphicsApi::NK_GFX_API_OPENGL:
                    return "OpenGL";
                case NkGraphicsApi::NK_GFX_API_OPENGLES:
                    return "OpenGLES";
                case NkGraphicsApi::NK_GFX_API_VULKAN:
                    return "Vulkan";
                case NkGraphicsApi::NK_GFX_API_D3D11:
                    return "D3D11";
                case NkGraphicsApi::NK_GFX_API_D3D12:
                    return "D3D12";
                case NkGraphicsApi::NK_GFX_API_METAL:
                    return "Metal";
                case NkGraphicsApi::NK_GFX_API_WEBGL:
                    return "WebGL";
                case NkGraphicsApi::NK_GFX_API_WEBGPU:
                    return "WebGPU";
                case NkGraphicsApi::NK_GFX_API_SOFTWARE:
                    return "Software";
                default:
                    return "None";
            }
        }

        // -------------------------------------------------------------------------
        // Énumération : NkGpuMemoryLevel
        // -------------------------------------------------------------------------
        // Description :
        //   Niveaux de pression mémoire GPU utilisés pour signaler l'état de la 
        //   mémoire vidéo et déclencher des stratégies de libération adaptatives.
        // Valeurs :
        //   - NK_GPU_MEM_NORMAL   : Mémoire suffisante, aucun action requise.
        //   - NK_GPU_MEM_LOW      : Avertissement, libérer les caches non essentiels.
        //   - NK_GPU_MEM_CRITICAL : Urgence, libérer toutes ressources temporaires.
        //   - NK_GPU_MEM_MAX      : Valeur sentinelle pour les itérations.
        // -------------------------------------------------------------------------
        enum class NkGpuMemoryLevel : uint32 {
            NK_GPU_MEM_NORMAL   = 0,
            NK_GPU_MEM_LOW,
            NK_GPU_MEM_CRITICAL,
            NK_GPU_MEM_MAX
        };

        // =========================================================================
        // Section : Hiérarchie des classes d'événements graphiques
        // =========================================================================

        // -------------------------------------------------------------------------
        // Classe : NkGraphicsEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Classe de base abstraite pour tous les événements de la catégorie 
        //   graphique. Centralise la gestion de l'identifiant de fenêtre source.
        // Héritage :
        //   - Public : NkEvent
        // Catégorie :
        //   - NK_CAT_GRAPHICS (via macro NK_EVENT_CATEGORY_FLAGS)
        // Utilisation :
        //   Ne pas instancier directement. Utiliser les classes dérivées.
        // -------------------------------------------------------------------------
        class NkGraphicsEvent : public NkEvent {
            public:
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_GRAPHICS)

            protected:
                explicit NkGraphicsEvent(uint64 windowId = 0) noexcept
                    : NkEvent(windowId) {
                }
        };

        // -------------------------------------------------------------------------
        // Classe : NkGraphicsContextReadyEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsque le contexte graphique est créé et opérationnel.
        //   Signale que l'application peut charger ses ressources GPU.
        // Déclenchement :
        //   - Après l'initialisation réussie d'OpenGL/Vulkan/D3D/etc.
        //   - Après une récupération suite à NkGraphicsContextLostEvent.
        // Données transportées :
        //   - API graphique utilisée, dimensions initiales du framebuffer.
        // -------------------------------------------------------------------------
        class NkGraphicsContextReadyEvent final : public NkGraphicsEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_APP_RENDER)

                NkGraphicsContextReadyEvent(
                    NkGraphicsApi api,
                    uint32 width,
                    uint32 height,
                    uint64 windowId = 0
                ) noexcept
                    : NkGraphicsEvent(windowId)
                    , mApi(api)
                    , mWidth(width)
                    , mHeight(height) {
                }

                NkEvent* Clone() const override {
                    return new NkGraphicsContextReadyEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GfxContextReady({0} {1}x{2})",
                        NkGraphicsApiToString(mApi),
                        mWidth,
                        mHeight
                    );
                }

                NkGraphicsApi GetApi() const noexcept {
                    return mApi;
                }

                uint32 GetWidth() const noexcept {
                    return mWidth;
                }

                uint32 GetHeight() const noexcept {
                    return mHeight;
                }

            private:
                NkGraphicsApi mApi = NkGraphicsApi::NK_GFX_API_NONE;
                uint32 mWidth = 0;
                uint32 mHeight = 0;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGraphicsContextLostEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lors de la perte inattendue du contexte graphique.
        //   Nécessite la libération immédiate de toutes les ressources GPU.
        // Causes possibles :
        //   - Mise en veille de l'appareil, changement d'adaptateur, crash driver.
        //   - Erreurs matérielles ou logicielles (VK_ERROR_DEVICE_LOST).
        // Action requise :
        //   - Libérer textures, buffers, pipelines, shaders.
        //   - Attendre NkGraphicsContextReadyEvent pour reconstruction.
        // -------------------------------------------------------------------------
        class NkGraphicsContextLostEvent final : public NkGraphicsEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_APP_CLOSE)

                NkGraphicsContextLostEvent(
                    NkString reason = {},
                    uint64 windowId = 0
                )
                    : NkGraphicsEvent(windowId)
                    , mReason(traits::NkMove(reason)) {
                }

                NkEvent* Clone() const override {
                    return new NkGraphicsContextLostEvent(*this);
                }

                NkString ToString() const override {
                    return "GfxContextLost(" + (mReason.Empty() ? "unknown" : mReason) + ")";
                }

                const NkString& GetReason() const noexcept {
                    return mReason;
                }

            private:
                NkString mReason;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGraphicsContextResizeEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsque les dimensions du framebuffer changent.
        //   Déclenche la recréation des ressources dépendantes de la résolution.
        // Cas d'usage :
        //   - Redimensionnement de fenêtre, changement DPI, mode plein écran.
        // Données transportées :
        //   - Nouvelles dimensions, dimensions précédentes, ratio d'aspect calculé.
        // -------------------------------------------------------------------------
        class NkGraphicsContextResizeEvent final : public NkGraphicsEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_APP_UPDATE)

                NkGraphicsContextResizeEvent(
                    uint32 width,
                    uint32 height,
                    uint32 prevW = 0,
                    uint32 prevH = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkGraphicsEvent(windowId)
                    , mWidth(width)
                    , mHeight(height)
                    , mPrevWidth(prevW)
                    , mPrevHeight(prevH) {
                }

                NkEvent* Clone() const override {
                    return new NkGraphicsContextResizeEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GfxContextResize({0}x{1} -> {2}x{3})",
                        mPrevWidth,
                        mPrevHeight,
                        mWidth,
                        mHeight
                    );
                }

                uint32 GetWidth() const noexcept {
                    return mWidth;
                }

                uint32 GetHeight() const noexcept {
                    return mHeight;
                }

                uint32 GetPrevWidth() const noexcept {
                    return mPrevWidth;
                }

                uint32 GetPrevHeight() const noexcept {
                    return mPrevHeight;
                }

                float GetAspectRatio() const noexcept {
                    return (mHeight > 0) 
                        ? static_cast<float>(mWidth) / static_cast<float>(mHeight) 
                        : 0.f;
                }

            private:
                uint32 mWidth = 0;
                uint32 mHeight = 0;
                uint32 mPrevWidth = 0;
                uint32 mPrevHeight = 0;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGraphicsFrameBeginEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis au début de chaque frame de rendu GPU.
        //   Point d'entrée pour la préparation des données de frame.
        // Utilisation typique :
        //   - Réinitialisation des uniform buffers dynamiques.
        //   - Synchronisation des ressources en double/triple buffering.
        //   - Début des markers de profiling GPU/CPU.
        // -------------------------------------------------------------------------
        class NkGraphicsFrameBeginEvent final : public NkGraphicsEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_APP_TICK)

                NkGraphicsFrameBeginEvent(
                    uint64 frameIndex = 0,
                    uint32 frameInFlight = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkGraphicsEvent(windowId)
                    , mFrameIndex(frameIndex)
                    , mFrameInFlight(frameInFlight) {
                }

                NkEvent* Clone() const override {
                    return new NkGraphicsFrameBeginEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GfxFrameBegin(frame={0} inFlight={1})",
                        mFrameIndex,
                        mFrameInFlight
                    );
                }

                uint64 GetFrameIndex() const noexcept {
                    return mFrameIndex;
                }

                uint32 GetFrameInFlight() const noexcept {
                    return mFrameInFlight;
                }

            private:
                uint64 mFrameIndex = 0;
                uint32 mFrameInFlight = 0;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGraphicsFrameEndEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis après la présentation d'une frame (swapchain present).
        //   Transporte les métriques de performance pour l'analyse et l'ajustement.
        // Métriques fournies :
        //   - Temps GPU effectif de rendu (ms).
        //   - Temps CPU d'enregistrement des commandes (ms).
        // Utilisation :
        //   - Profiling temps réel, adaptation dynamique de la qualité graphique.
        // -------------------------------------------------------------------------
        class NkGraphicsFrameEndEvent final : public NkGraphicsEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_APP_LAUNCH)

                NkGraphicsFrameEndEvent(
                    uint64 frameIndex = 0,
                    float64 gpuTimeMs = 0.0,
                    float64 cpuTimeMs = 0.0,
                    uint64 windowId = 0
                ) noexcept
                    : NkGraphicsEvent(windowId)
                    , mFrameIndex(frameIndex)
                    , mGpuTimeMs(gpuTimeMs)
                    , mCpuTimeMs(cpuTimeMs) {
                }

                NkEvent* Clone() const override {
                    return new NkGraphicsFrameEndEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GfxFrameEnd(frame={0} GPU={1:.3}ms CPU={2:.3}ms)",
                        mFrameIndex,
                        mGpuTimeMs,
                        mCpuTimeMs
                    );
                }

                uint64 GetFrameIndex() const noexcept {
                    return mFrameIndex;
                }

                float64 GetGpuTimeMs() const noexcept {
                    return mGpuTimeMs;
                }

                float64 GetCpuTimeMs() const noexcept {
                    return mCpuTimeMs;
                }

            private:
                uint64 mFrameIndex = 0;
                float64 mGpuTimeMs = 0.0;
                float64 mCpuTimeMs = 0.0;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGraphicsGpuMemoryEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis lorsque le pilote signale une pression mémoire GPU.
        //   Critique pour les plateformes mobiles et GPU à mémoire partagée.
        // Stratégie de réponse recommandée :
        //   - LOW : Libérer les caches de textures, réduire les LOD.
        //   - CRITICAL : Libérer toutes ressources non visibles, baisser la qualité.
        // Données transportées :
        //   - Niveau de pression, mémoire disponible/totale en Mo.
        // -------------------------------------------------------------------------
        class NkGraphicsGpuMemoryEvent final : public NkGraphicsEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_APP_TICK)

                NkGraphicsGpuMemoryEvent(
                    NkGpuMemoryLevel level,
                    uint32 availableMb = 0,
                    uint32 totalMb = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkGraphicsEvent(windowId)
                    , mLevel(level)
                    , mAvailableMb(availableMb)
                    , mTotalMb(totalMb) {
                }

                NkEvent* Clone() const override {
                    return new NkGraphicsGpuMemoryEvent(*this);
                }

                NkString ToString() const override {
                    const char* lvls[] = {"NORMAL", "LOW", "CRITICAL"};
                    uint32 idx = static_cast<uint32>(mLevel);
                    return NkString::Fmt(
                        "GfxGpuMemory({0} avail={1}MB)",
                        (idx < 3 ? lvls[idx] : "?"),
                        mAvailableMb
                    );
                }

                NkGpuMemoryLevel GetLevel() const noexcept {
                    return mLevel;
                }

                uint32 GetAvailableMb() const noexcept {
                    return mAvailableMb;
                }

                uint32 GetTotalMb() const noexcept {
                    return mTotalMb;
                }

                bool IsCritical() const noexcept {
                    return mLevel == NkGpuMemoryLevel::NK_GPU_MEM_CRITICAL;
                }

            private:
                NkGpuMemoryLevel mLevel = NkGpuMemoryLevel::NK_GPU_MEM_NORMAL;
                uint32 mAvailableMb = 0;
                uint32 mTotalMb = 0;
        };

        // -------------------------------------------------------------------------
        // Classe : NkGraphicsVSyncEvent
        // -------------------------------------------------------------------------
        // Description :
        //   Événement émis à la réception du signal VSync du moniteur.
        //   Permet une synchronisation précise sans dépendre du timer système.
        // Notes importantes :
        //   - Support dépendant du backend graphique et du pilote.
        //   - Utile pour le mode "frame-locked" ou la régulation manuelle du FPS.
        // -------------------------------------------------------------------------
        class NkGraphicsVSyncEvent final : public NkGraphicsEvent {
            public:
                NK_EVENT_TYPE_FLAGS(NK_APP_RENDER)

                NkGraphicsVSyncEvent(
                    uint32 displayIndex = 0,
                    uint32 refreshRateHz = 60,
                    uint64 windowId = 0
                ) noexcept
                    : NkGraphicsEvent(windowId)
                    , mDisplayIndex(displayIndex)
                    , mRefreshRateHz(refreshRateHz) {
                }

                NkEvent* Clone() const override {
                    return new NkGraphicsVSyncEvent(*this);
                }

                NkString ToString() const override {
                    return NkString::Fmt(
                        "GfxVSync(display={0} {1}Hz)",
                        mDisplayIndex,
                        mRefreshRateHz
                    );
                }

                uint32 GetDisplayIndex() const noexcept {
                    return mDisplayIndex;
                }

                uint32 GetRefreshRateHz() const noexcept {
                    return mRefreshRateHz;
                }

            private:
                uint32 mDisplayIndex = 0;
                uint32 mRefreshRateHz = 60;
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKGRAPHICSEVENT_H

// =============================================================================
// Section : Exemples d'utilisation (à titre informatif - en commentaire)
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Écoute d'un événement de contexte prêt
// -----------------------------------------------------------------------------
void OnGraphicsContextReady(const NkGraphicsContextReadyEvent& event) {
    // Récupération des informations du contexte
    const auto api = event.GetApi();
    const auto width = event.GetWidth();
    const auto height = event.GetHeight();

    // Journalisation de l'initialisation
    NK_LOG_INFO(
        "Contexte graphique prêt : {} en {}x{}",
        NkGraphicsApiToString(api),
        width,
        height
    );

    // Chargement des ressources GPU (shaders, textures, pipelines...)
    // LoadGraphicsResources(api, width, height);
}

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion de la perte de contexte GPU
// -----------------------------------------------------------------------------
void OnGraphicsContextLost(const NkGraphicsContextLostEvent& event) {
    // Libération obligatoire de toutes les ressources GPU
    // DestroyAllGraphicsResources();

    // Journalisation de l'erreur pour débogage
    NK_LOG_WARN(
        "Contexte GPU perdu : {}",
        event.GetReason().Empty() ? "cause inconnue" : event.GetReason()
    );

    // Mise en attente de la reconstruction automatique ou manuelle
}

// -----------------------------------------------------------------------------
// Exemple 3 : Adaptation au redimensionnement du framebuffer
// -----------------------------------------------------------------------------
void OnGraphicsContextResize(const NkGraphicsContextResizeEvent& event) {
    // Mise à jour de la viewport et des scissor rects
    // GraphicsDevice::SetViewport(0, 0, event.GetWidth(), event.GetHeight());

    // Recréation des render targets dépendants de la résolution
    // RecreateSwapchain(event.GetWidth(), event.GetHeight());

    // Ajustement des matrices de projection si nécessaire
    const float aspect = event.GetAspectRatio();
    // UpdateProjectionMatrix(aspect);
}

// -----------------------------------------------------------------------------
// Exemple 4 : Profiling des frames avec métriques GPU/CPU
// -----------------------------------------------------------------------------
void OnGraphicsFrameEnd(const NkGraphicsFrameEndEvent& event) {
    // Collecte des métriques pour l'analyse de performance
    static constexpr float64 TARGET_FRAME_TIME_MS = 16.666; // 60 FPS

    if (event.GetGpuTimeMs() > TARGET_FRAME_TIME_MS) {
        NK_LOG_DEBUG(
            "Frame {} lente : GPU={:.2f}ms CPU={:.2f}ms",
            event.GetFrameIndex(),
            event.GetGpuTimeMs(),
            event.GetCpuTimeMs()
        );

        // Déclenchement potentiel d'ajustement de qualité graphique
        // QualityManager::ReduceSettingsIfNeeded();
    }
}

// -----------------------------------------------------------------------------
// Exemple 5 : Réponse à la pression mémoire GPU (mobile/intégré)
// -----------------------------------------------------------------------------
void OnGpuMemoryPressure(const NkGraphicsGpuMemoryEvent& event) {
    switch (event.GetLevel()) {
        case NkGpuMemoryLevel::NK_GPU_MEM_LOW:
            // Libération des caches de textures non utilisées récemment
            // TextureCache::Trim(0.3f); // Libérer 30% du cache
            break;

        case NkGpuMemoryLevel::NK_GPU_MEM_CRITICAL:
            // Libération agressive : LOD minimum, suppression des assets hors écran
            // TextureCache::Trim(0.7f);
            // MeshPool::ReleaseUnused();
            // QualityManager::SetPreset(QualityPreset::LOW);
            break;

        default:
            // Niveau normal : aucune action requise
            break;
    }

    NK_LOG_INFO(
        "Mémoire GPU : {} ({}/{} MB)",
        event.GetLevel() == NkGpuMemoryLevel::NK_GPU_MEM_CRITICAL ? "CRITIQUE" : "ATTENTION",
        event.GetAvailableMb(),
        event.GetTotalMb()
    );
}

// -----------------------------------------------------------------------------
// Exemple 6 : Synchronisation manuelle sur VSync
// -----------------------------------------------------------------------------
void OnVSyncSignal(const NkGraphicsVSyncEvent& event) {
    // Utilisation du signal VSync pour une boucle de rendu frame-locked
    // sans dépendre du timer système (plus précis pour le vsync logiciel)

    static uint64 lastFrame = 0;
    const uint64 currentFrame = event.GetFrameIndex();

    if (currentFrame != lastFrame) {
        // Mise à jour de la logique jeu synchronisée à l'affichage
        // GameLogic::UpdateFixedStep();
        // Renderer::SubmitFrame();
        lastFrame = currentFrame;
    }
}

// -----------------------------------------------------------------------------
// Exemple 7 : Enregistrement des listeners d'événements graphiques
// -----------------------------------------------------------------------------
void RegisterGraphicsEventListeners(NkEventManager& eventManager) {
    // Inscription des callbacks pour chaque type d'événement graphique
    eventManager.Subscribe<NkGraphicsContextReadyEvent>(
        [](const NkGraphicsContextReadyEvent& e) {
            OnGraphicsContextReady(e);
        }
    );

    eventManager.Subscribe<NkGraphicsContextLostEvent>(
        [](const NkGraphicsContextLostEvent& e) {
            OnGraphicsContextLost(e);
        }
    );

    eventManager.Subscribe<NkGraphicsContextResizeEvent>(
        [](const NkGraphicsContextResizeEvent& e) {
            OnGraphicsContextResize(e);
        }
    );

    eventManager.Subscribe<NkGraphicsFrameEndEvent>(
        [](const NkGraphicsFrameEndEvent& e) {
            OnGraphicsFrameEnd(e);
        }
    );

    eventManager.Subscribe<NkGraphicsGpuMemoryEvent>(
        [](const NkGraphicsGpuMemoryEvent& e) {
            OnGpuMemoryPressure(e);
        }
    );

    eventManager.Subscribe<NkGraphicsVSyncEvent>(
        [](const NkGraphicsVSyncEvent& e) {
            OnVSyncSignal(e);
        }
    );
}

// -----------------------------------------------------------------------------
// Exemple 8 : Création et dispatch manuel d'événements (tests/debug)
// -----------------------------------------------------------------------------
void DispatchTestGraphicsEvents(NkEventManager& eventManager) {
    // Simulation d'un contexte OpenGL 1920x1080 prêt
    NkGraphicsContextReadyEvent readyEvent(
        NkGraphicsApi::NK_GFX_API_OPENGL,
        1920,
        1080,
        1 // windowId
    );
    eventManager.Dispatch(readyEvent);

    // Simulation d'un redimensionnement vers 2560x1440
    NkGraphicsContextResizeEvent resizeEvent(
        2560,   // nouvelle largeur
        1440,   // nouvelle hauteur
        1920,   // largeur précédente
        1080,   // hauteur précédente
        1       // windowId
    );
    eventManager.Dispatch(resizeEvent);

    // Simulation d'une alerte mémoire critique
    NkGraphicsGpuMemoryEvent memoryEvent(
        NkGpuMemoryLevel::NK_GPU_MEM_CRITICAL,
        128,    // 128 Mo disponibles
        2048,   // 2 Go totaux
        1       // windowId
    );
    eventManager.Dispatch(memoryEvent);
}
*/