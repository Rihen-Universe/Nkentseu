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
            // 156 frames numerotees de rihen_00000.png a rihen_00155.png.
            static constexpr int   kFrameCount       = 156;
            // 2026-05-19 (v2) : 4.0s pour 156 frames -> 39 fps de lecture
            // (cadence ~25.64 ms par frame). L'avance se fait FRAME-PAR-FRAME
            // (timer accumule, pas mapping temporel) pour garantir qu'AUCUNE
            // frame n'est sautee meme si l'app a un freeze de quelques ms.
            static constexpr float kDuration         = 4.0f;
            static constexpr float kFadeOut          = 0.3f;
            /// Duree en secondes d'affichage d'une frame d'animation.
            static constexpr float kFrameDuration    = kDuration / (float)kFrameCount;
            /// Cap d'uploads GL par OnUpdate en mode Playing (animation en
            /// cours) : on monte haut car le worker doit rattraper si en retard.
            static constexpr int   kUploadsPerUpdate = 16;
            /// Cap d'uploads GL par OnUpdate en mode Loading (spinner actif) :
            /// on limite VOLONTAIREMENT a 2-4 par tick pour ne pas saccader
            /// le spinner. L'upload GL est obligatoirement sur main thread
            /// (contrainte OpenGL/ES : 1 contexte = 1 thread). Reduire le
            /// throughput d'upload donne au renderer le temps de dessiner
            /// la balle qui orbite a 60 fps. Le decode CPU des PNG reste sur
            /// le thread worker separe -> aucun impact sur le main thread.
            static constexpr int   kUploadsPerUpdateLoading = 3;
            /// Seuil de frames pour lancer l'animation. 100% (156) :
            /// charge TOUTES les images avant de jouer. L'utilisateur voit
            /// le spinner pendant tout le chargement (1-5s selon device).
            /// Garantit zero saccade pendant l'anim car aucun upload GL
            /// n'est en cours pendant la lecture.
            static constexpr int   kFramesToStartAnim = kFrameCount;

            RihenIntroScene()  = default;
            ~RihenIntroScene() override = default;

            const char* Name() const noexcept override { return "RihenIntro"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnExit  (AppContext& ctx) override;

        private:
            // ── State machine : Loading -> Playing -> Done ─────────────────
            // Loading : on affiche loadingicon.png + un spinner pendant que
            //           le worker decode les premieres frames de l'anim.
            //           Bascule en Playing quand mFramesLoaded >= kFramesToStartAnim.
            // Playing : animation 156 frames classique (mTime / kDuration).
            // Done    : transition vers NogeIntroScene.
            enum class State : uint8 { Loading, Playing, Done };
            State     mState = State::Loading;
            float     mLoadingTime = 0.0f;      ///< Temps en Loading (pour spinner)

            // ── Textures GL (uploadees au fur et a mesure par OnUpdate) ─────
            Texture2D mFrames[kFrameCount];
            int       mFramesLoaded = 0;        ///< Nb de textures uploadees (monotone)
            float     mAspect       = 4.0f;     ///< Ratio W/H mesure sur la 1ere frame
            float     mTime         = 0.0f;     ///< Temps anim ecoule (depuis Playing, pour fade-out + transition)
            bool      mDone         = false;

            // Logo statique affiche pendant Loading. Charge a OnEnter.
            Texture2D mLoadingLogo;

            // ── Avance frame-par-frame (anti-saut) ──────────────────────────
            // mCurrentFrame avance de 1 chaque kFrameDuration sec accumule
            // dans mFrameAccum. Cette logique sequentielle (au lieu du mapping
            // idx = t * kFrameCount precedent) garantit que toutes les 156
            // frames sont rendues au moins une fois, meme si un frame de
            // l'app freeze brievement.
            int       mCurrentFrame = 0;
            float     mFrameAccum   = 0.0f;

            // ── Worker thread (decode CPU only, pas d'appel GL) ─────────────
            std::thread        mWorker;
            std::atomic<bool>  mWorkerStop{false};
            /// Vrai quand WorkerProc a termine sa boucle (succes OU echec).
            /// Utilise par DrainQueue pour distinguer "worker en retard"
            /// (= attendre) vs "frame definitivement absente" (= skip).
            std::atomic<bool>  mWorkerDone{false};
            /// Index courant atteint par le worker (-1 = pas demarre).
            /// Si mPending[i] == nullptr ET i < mWorkerLastAttempted ->
            /// la frame i est definitivement manquante (decode rate).
            std::atomic<int>   mWorkerLastAttempted{-1};
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
            /// Upload jusqu'a @p maxUploads textures GL depuis la file
            /// du worker. Appel avec kUploadsPerUpdateLoading pendant Loading
            /// (spinner fluide) ou kUploadsPerUpdate pendant Playing.
            int  DrainQueue(int maxUploads);
        };

    } // namespace pong
} // namespace nkentseu
