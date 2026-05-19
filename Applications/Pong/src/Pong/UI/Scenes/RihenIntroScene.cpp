// =============================================================================
// RihenIntroScene.cpp
// -----------------------------------------------------------------------------
// Chargement asynchrone : un thread worker decode les PNGs sequentiellement
// (CPU-only via NkImage::Load — safe hors GL) et pousse les images dans une
// file partagee. Le main thread (OnUpdate) drain la file et UPLOAD les
// textures GL au rythme de kUploadsPerUpdate par tick.
//
// L'animation demarre des que la 1ere frame est uploadee et avance sur le
// temps reel (mTime). L'index courant est clamp a "mFramesLoaded - 1" tant
// que le decode est en retard, donc la 1ere frame se fige jusqu'a ce que
// la suivante arrive — mais le timer continue, donc on rattrape ensuite.
// =============================================================================

#include "RihenIntroScene.h"
#include "NogeIntroScene.h"
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h"
#include "NKImage/Core/NkImage.h"
#include <cstdio>

namespace nkentseu
{
    namespace pong
    {

        // ── Easing : rampe douce pour le fade out final ──────────────────────
        static float EaseOutCubic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        // ─────────────────────────────────────────────────────────────────────
        void RihenIntroScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime         = 0.0f;
            mDone         = false;
            mFramesLoaded = 0;
            mAspect       = 4.0f;
            mPendingNext  = 0;
            for (int i = 0; i < kFrameCount; ++i) mPending[i] = nullptr;

            // Lance le decode en background.
            StartWorker();
            logger.Info("[RihenIntro] OnEnter - async loader started");
        }

        void RihenIntroScene::OnExit(AppContext& /*ctx*/)
        {
            // Arrete le worker proprement (signal stop + join).
            StopWorker();
            // Libere les images decodees mais non encore uploadees.
            for (int i = 0; i < kFrameCount; ++i)
            {
                if (mPending[i] != nullptr)
                {
                    mPending[i]->Free();
                    mPending[i] = nullptr;
                }
            }
            // Libere les textures GL deja uploadees.
            for (int i = 0; i < kFrameCount; ++i) mFrames[i].Shutdown();
            mFramesLoaded = 0;
        }

        // ─────────────────────────────────────────────────────────────────────
        // Worker thread
        // ─────────────────────────────────────────────────────────────────────
        void RihenIntroScene::StartWorker()
        {
            mWorkerStop.store(false);
            mWorker = std::thread(&RihenIntroScene::WorkerProc, this);
        }

        void RihenIntroScene::StopWorker()
        {
            mWorkerStop.store(true);
            if (mWorker.joinable()) mWorker.join();
        }

        // Decode sequentiel des 72 PNG. Chaque image decodee est stockee dans
        // mPending[i]. Aucun appel GL ici (thread non-GL).
        void RihenIntroScene::WorkerProc()
        {
            char path[256];
            for (int i = 0; i < kFrameCount; ++i)
            {
                if (mWorkerStop.load()) break;
                // Le naming fichier commence a kStartFileIndex (=3), pas a 0.
                const int fileIdx = i + kStartFileIndex;
                std::snprintf(path, sizeof(path),
                              "Resources/Pong/Textures/animrihen/RIHEN LOGO_%05d.png",
                              fileIdx);
                NkImage* img = Texture2D::DecodeFromFile(path);
                if (img == nullptr)
                {
                    // Stoppe l'animation a la derniere frame valide.
                    break;
                }
                {
                    std::lock_guard<std::mutex> lock(mQueueMutex);
                    mPending[i] = img;
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnUpdate : avance le timer + drain la queue (upload GL)
        // ─────────────────────────────────────────────────────────────────────
        int RihenIntroScene::DrainQueue()
        {
            int uploaded = 0;
            for (int n = 0; n < kUploadsPerUpdate; ++n)
            {
                NkImage* img = nullptr;
                {
                    std::lock_guard<std::mutex> lock(mQueueMutex);
                    if (mPendingNext >= kFrameCount) return uploaded;
                    img = mPending[mPendingNext];
                    if (img == nullptr) return uploaded;   // worker en retard
                    mPending[mPendingNext] = nullptr;
                }
                // Upload sur le main thread (contexte GL).
                if (mFrames[mPendingNext].UploadFromImage(img))
                {
                    if (mPendingNext == 0)
                        mAspect = mFrames[0].AspectRatio();
                    mFramesLoaded = mPendingNext + 1;
                }
                ++mPendingNext;
                ++uploaded;
            }
            return uploaded;
        }

        void RihenIntroScene::OnUpdate(AppContext& ctx, float dt)
        {
            mTime += dt;
            DrainQueue();
            if (!mDone && mTime >= kDuration)
            {
                mDone = true;
                ctx.scenes->Replace(new NogeIntroScene());
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnRender : fond blanc + frame courante centree avec ratio preserve.
        // ─────────────────────────────────────────────────────────────────────
        void RihenIntroScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            const int   W  = ctx.viewportW;
            const int   H  = ctx.viewportH;
            const float cx = ctx.safe.SafeCX();
            const float cy = ctx.safe.SafeCY();

            // Fond BLANC.
            r.Clear(1.0f, 1.0f, 1.0f, 1.0f);
            r.Begin(W, H);

            if (mFramesLoaded > 0)
            {
                // ── Choix de la frame courante ────────────────────────────
                // Index ideal selon le temps reel. On clamp a la derniere
                // frame uploadee : si le worker est en retard, la frame se
                // "fige" sur la derniere dispo mais le timer continue.
                float t = mTime / kDuration;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                int idx = static_cast<int>(t * (kFrameCount - 1) + 0.5f);
                if (idx >= mFramesLoaded) idx = mFramesLoaded - 1;
                if (idx < 0)              idx = 0;

                // ── Taille a l'ecran : ratio STRICTEMENT preserve ─────────
                const float safeW = static_cast<float>(ctx.safe.SafeW());
                const float safeH = static_cast<float>(ctx.safe.SafeH());
                const float maxW  = safeW * 0.85f;
                const float maxH  = safeH * 0.85f;
                float drawW = maxW;
                float drawH = (mAspect > 0.0001f) ? (drawW / mAspect) : maxH;
                if (drawH > maxH)
                {
                    drawH = maxH;
                    drawW = drawH * mAspect;
                }
                const float drawX = cx - drawW * 0.5f;
                const float drawY = cy - drawH * 0.5f;

                r.BindTexture(mFrames[idx].Id());
                r.DrawTexturedQuadRGBA(drawX, drawY, drawW, drawH,
                                       0.0f, 0.0f, 1.0f, 1.0f,
                                       theme::White());
            }

            // ── Fade out blanc en fin d'animation ─────────────────────────
            const float fadeStart = kDuration - kFadeOut;
            if (mTime > fadeStart)
            {
                float a = (mTime - fadeStart) / kFadeOut;
                if (a < 0.0f) a = 0.0f;
                if (a > 1.0f) a = 1.0f;
                a = EaseOutCubic(a);
                math::NkColor whiteFade = theme::White();
                whiteFade.a = static_cast<uint8_t>(255.0f * a);
                r.DrawQuad(0.0f, 0.0f,
                           static_cast<float>(W), static_cast<float>(H),
                           whiteFade);
            }

            r.End();
        }

    } // namespace pong
} // namespace nkentseu
