#pragma once
// =============================================================================
// RihenIntroScene.h
// -----------------------------------------------------------------------------
// Intro Rihen — lecture d'une sequence d'images PNG charge ASYNCHRONEMENT.
//
// Pipeline de chargement :
//   - Un thread worker decode sequentiellement chaque PNG (NkImage::Load,
//     CPU-only, safe hors GL) et empile l'image decodee dans une file.
//   - Le main thread (OnUpdate) drain la file et UPLOAD les images en
//     textures GL au rythme de quelques-unes par frame (kUploadsPerUpdate).
//   - mFramesLoaded augmente au fur et a mesure ; le rendu clamp l'index
//     a la frame courante "mFramesLoaded - 1" tant que la suite n'est pas
//     chargee. L'animation DEMARRE des que la 1ere frame est uploadee et
//     le temps d'animation reste base sur l'horloge reelle (kDuration).
//
// Lecture :
//   - 72 frames sur kDuration = 4s
//   - Fond BLANC (les PNG sont a fond keye transparent par preprocess Python)
//   - Ratio largeur/hauteur strictement preserve, contraint a 85% de la
//     zone safe
//   - Fade-out blanc sur les kFadeOut dernieres secondes
//
// A la fin des kDuration secondes, transition Replace() vers NogeIntroScene.
// =============================================================================

#include "Pong/UI/Scene.h"
#include "Pong/Render/Texture2D.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace nkentseu { class NkImage; }

namespace nkentseu
{
    namespace pong
    {

        class RihenIntroScene : public Scene
        {
        public:
            // 156 frames (numerotation fichier de _00003 a _00158).
            static constexpr int   kFrameCount       = 156;
            static constexpr int   kStartFileIndex   = 3;     // 1er fichier
            static constexpr float kDuration         = 4.0f;
            static constexpr float kFadeOut          = 0.4f;
            /// Cap d'uploads GL par OnUpdate (rate limit) — evite de hoqueter
            /// le main thread si le worker a decode beaucoup d'avance.
            static constexpr int   kUploadsPerUpdate = 4;

            RihenIntroScene()  = default;
            ~RihenIntroScene() override = default;

            const char* Name() const noexcept override { return "RihenIntro"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnExit  (AppContext& ctx) override;

        private:
            // ── Textures GL (uploadees au fur et a mesure par OnUpdate) ─────
            Texture2D mFrames[kFrameCount];
            int       mFramesLoaded = 0;        ///< Nb de textures uploadees (monotone)
            float     mAspect       = 4.0f;     ///< Ratio W/H mesure sur la 1ere frame
            float     mTime         = 0.0f;
            bool      mDone         = false;

            // ── Worker thread (decode CPU only, pas d'appel GL) ─────────────
            std::thread        mWorker;
            std::atomic<bool>  mWorkerStop{false};
            std::mutex         mQueueMutex;
            /// File des images decodees, en attente d'upload main thread.
            /// L'index correspond a la position de la frame dans la sequence
            /// (0..kFrameCount-1). Sequentiel : le worker pousse dans l'ordre.
            struct PendingFrame { int index; NkImage* image; };
            // Buffer circulaire / file simple (tableau borne). On utilise un
            // index "head" qui ne fait qu'augmenter ; quand mFramesLoaded
            // rattrappe head, la file est vide. Toutes les operations sont
            // protegees par mQueueMutex.
            NkImage* mPending[kFrameCount] = { nullptr };
            int      mPendingNext = 0;          ///< Index suivant a uploader

            void StartWorker();
            void StopWorker();
            void WorkerProc();
            int  DrainQueue();   ///< Upload jusqu'a kUploadsPerUpdate. Retourne le nb.
        };

    } // namespace pong
} // namespace nkentseu
