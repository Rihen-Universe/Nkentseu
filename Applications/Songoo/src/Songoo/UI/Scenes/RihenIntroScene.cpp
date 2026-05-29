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
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/UI/Theme.h"
#include "Songoo/UI/SceneManager.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h"
#include "NKImage/Core/NkImage.h"
#include <cstdio>

namespace nkentseu
{
    namespace songoo
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
            mState        = State::Loading;
            mLoadingTime  = 0.0f;
            mTime         = 0.0f;
            mDone         = false;
            mFramesLoaded = 0;
            mAspect       = 4.0f;
            mPendingNext  = 0;
            mCurrentFrame = 0;
            mFrameAccum   = 0.0f;
            for (int i = 0; i < kFrameCount; ++i) mPending[i] = nullptr;

            mWorkerDone.store(false);
            mWorkerLastAttempted.store(-1);

            // Charge le logo statique "loadingicon.png" affiche pendant la
            // phase Loading. Si echec, on continue sans logo (le spinner
            // restera seul). Petit fichier => chargement quasi-instantane.
            mLoadingLogo.LoadFromFile("Resources/Pong/Textures/iconexe/loadingicon.png");

            // Lance le decode en background.
            StartWorker();
            logger.Info("[RihenIntro] OnEnter - Loading phase (worker started)");
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
            mLoadingLogo.Shutdown();
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

        // Decode sequentiel des 156 PNG. Chaque image decodee est stockee dans
        // mPending[i]. Aucun appel GL ici (thread non-GL).
        //
        // Resilience : si une frame est introuvable (user a supprime un PNG,
        // path resolu vers le mauvais cwd, decode rate, etc.), on SKIP cette
        // frame (continue) au lieu d'arreter le worker. La frame manquante
        // est detectee par DrainQueue via mWorkerLastAttempted > i et
        // mPending[i] == nullptr -> on l'ignore visuellement (freeze sur
        // la derniere texture valide).
        void RihenIntroScene::WorkerProc()
        {
            char path[256];
            int missingCount = 0;
            int decoded = 0;
            for (int i = 0; i < kFrameCount; ++i)
            {
                if (mWorkerStop.load())
                {
                    logger.Warn("[RihenIntro] Worker INTERROMPU a i={0}/{1} (mWorkerStop signaled)",
                                i, kFrameCount);
                    break;
                }
                // Files renumerotes 2026-05-19 : noms `rihen_00000.png` a
                // `rihen_00155.png`, sans espaces, demarrant a 0. L'index i
                // est utilise directement dans le path.
                std::snprintf(path, sizeof(path),
                              "Resources/Pong/Textures/animrihen/rihen_%05d.png", i);
                NkImage* img = Texture2D::DecodeFromFile(path);
                if (img == nullptr)
                {
                    // Frame manquante : on continue (au lieu de break) pour
                    // tenter les suivantes. mPending[i] reste nullptr, le
                    // DrainQueue saura la sauter via mWorkerLastAttempted.
                    ++missingCount;
                    mWorkerLastAttempted.store(i);
                    logger.Warn("[RihenIntro] Decode FAIL i={0} path={1}", i, path);
                    continue;
                }
                {
                    std::lock_guard<std::mutex> lock(mQueueMutex);
                    mPending[i] = img;
                }
                mWorkerLastAttempted.store(i);
                ++decoded;
                // Log progress toutes les 30 frames pour suivre la cadence
                // (utile en Debug ou le decode est lent).
                if ((i + 1) % 30 == 0)
                {
                    logger.Info("[RihenIntro] Worker progress : {0}/{1} decodees ({2} miss)",
                                i + 1, kFrameCount, missingCount);
                }
            }
            mWorkerDone.store(true);
            logger.Info("[RihenIntro] Worker termine : {0}/{1} decodees, {2} manquantes",
                        decoded, kFrameCount, missingCount);
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnUpdate : avance le timer + drain la queue (upload GL)
        // ─────────────────────────────────────────────────────────────────────
        int RihenIntroScene::DrainQueue(int maxUploads)
        {
            int uploaded = 0;
            for (int n = 0; n < maxUploads; ++n)
            {
                NkImage* img = nullptr;
                bool isMissing = false;
                {
                    std::lock_guard<std::mutex> lock(mQueueMutex);
                    if (mPendingNext >= kFrameCount) return uploaded;
                    img = mPending[mPendingNext];
                    if (img == nullptr)
                    {
                        // Worker a-t-il deja depasse ce slot ?
                        const int workerAt = mWorkerLastAttempted.load();
                        if (workerAt > mPendingNext || mWorkerDone.load())
                        {
                            // Slot definitivement manquant : skip.
                            isMissing = true;
                        }
                        else
                        {
                            // Worker pas encore arrive ici : on attend.
                            return uploaded;
                        }
                    }
                    if (!isMissing) mPending[mPendingNext] = nullptr;
                }
                if (isMissing)
                {
                    // Frame manquante : pas d'upload, mais on AVANCE pour
                    // ne pas bloquer l'animation. mFrames[mPendingNext]
                    // reste invalide (Id == 0), OnRender retombe sur la
                    // derniere texture valide -> freeze visuel 1 frame.
                    mFramesLoaded = mPendingNext + 1;
                }
                else
                {
                    // Upload sur le main thread (contexte GL).
                    if (mFrames[mPendingNext].UploadFromImage(img))
                    {
                        if (mPendingNext == 0)
                            mAspect = mFrames[0].AspectRatio();
                        mFramesLoaded = mPendingNext + 1;
                    }
                }
                ++mPendingNext;
                ++uploaded;
            }
            return uploaded;
        }

        void RihenIntroScene::OnUpdate(AppContext& ctx, float dt)
        {
            if (mState == State::Loading)
            {
                mLoadingTime += dt;
                // Phase Loading : decode (worker thread) + upload (main thread)
                // s'overlap en parallele pour minimiser le temps total.
                //
                //   Worker thread : decode CPU des 156 PNG en RAM (mPending[]).
                //                    Independant du main thread.
                //   Main thread   : DrainQueue uploadse les textures GL des
                //                    qu'elles sont decodees, MAIS limite a
                //                    kUploadsPerUpdateLoading par tick pour
                //                    minimiser la saccade de la balle qui orbite
                //                    (upload GL bloque le main thread ~5-50ms
                //                    par texture sur Android lent).
                //
                // Temps total = max(decode, upload) au lieu de decode+upload.
                // Sur Android : ~1-3s perceptibles avec saccade tres legere
                // (vs 1.3-4s avec saccade concentree sur la fin).
                DrainQueue(kUploadsPerUpdateLoading);
                if (mFramesLoaded >= kFrameCount)
                {
                    mState = State::Playing;
                    mTime  = 0.0f;
                    logger.Info("[RihenIntro] Loading terminee en {0:.2}s -> Playing",
                                mLoadingTime);
                }
                return;
            }

            if (mState != State::Playing) return;

            mTime += dt;

            // Strategie de selection de frame :
            //   1. mTime accumule en temps reel.
            //   2. mCurrentFrame est calcule comme idx = t * (kFrameCount - 1).
            //   3. Clamp a mFramesLoaded - 1 (jamais une texture non uploadee).
            //   4. Transition Replace(Noge) UNIQUEMENT quand mCurrentFrame
            //      atteint la derniere frame (anim complete, contrainte stricte).
            float t = (kDuration > 0.0f) ? (mTime / kDuration) : 1.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            int idx = static_cast<int>(t * (float)(kFrameCount - 1) + 0.5f);
            if (idx >= kFrameCount) idx = kFrameCount - 1;
            if (idx < 0)            idx = 0;
            if (idx >= mFramesLoaded) idx = mFramesLoaded - 1;
            if (idx < 0)              idx = 0;
            mCurrentFrame = idx;

            if (!mDone && mCurrentFrame >= kFrameCount - 1)
            {
                mDone  = true;
                mState = State::Done;
                ctx.scenes->Replace(new NogeIntroScene());
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnRender :
        //   Phase Loading : loadingicon.png centre + spinner 8 dots tournant
        //   Phase Playing : frame courante de l'anim, ratio preserve, fade-out
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

            if (mState == State::Loading)
            {
                // ── Plateau Mancala anime avec grains en mouvement ──────────
                const float safeW = static_cast<float>(ctx.safe.SafeW());
                const float safeH = static_cast<float>(ctx.safe.SafeH());

                // Layout plateau : centré, 80% de la safe area
                const float boardW = safeW * 0.80f;
                const float boardH = boardW * 0.50f;  // Ratio 16:10
                const float boardX = cx - boardW * 0.5f;
                const float boardY = cy - boardH * 0.5f;

                // Grille des pits : 14 pits en 2 lignes (6 + 1 mancala)
                static constexpr int kNumPits = 14;
                float pitCX[kNumPits], pitCY[kNumPits];
                const float pitRadius = boardW / 25.0f;
                const float mancalaR = pitRadius * 1.5f;

                // Ligne 1 (Joueur 1, bas) : pits 0-5 + mancala 12
                const float row1Y = boardY + boardH * 0.65f;
                const float col1X = boardX + boardW * 0.05f;
                const float colSpacing = boardW / 8.0f;
                for (int i = 0; i < 6; ++i)
                {
                    pitCX[i] = col1X + (float)i * colSpacing;
                    pitCY[i] = row1Y;
                }
                pitCX[12] = boardX + boardW * 0.95f;  // Mancala 1 (droite)
                pitCY[12] = row1Y;

                // Ligne 2 (Joueur 2, haut) : pits 6-11 + mancala 13
                const float row2Y = boardY + boardH * 0.35f;
                for (int i = 6; i < 12; ++i)
                {
                    pitCX[i] = col1X + (float)(11 - i) * colSpacing;
                    pitCY[i] = row2Y;
                }
                pitCX[13] = boardX + boardW * 0.95f;  // Mancala 2 (droite)
                pitCY[13] = row2Y;

                // Dessiner les pits
                const math::NkColor color1 = theme::Orange();
                const math::NkColor color2 = theme::Cyan();
                for (int i = 0; i < 14; ++i)
                {
                    bool isMancala = (i == 12 || i == 13);
                    bool isPlayer1 = (i < 6 || i == 12);
                    float r_pit = isMancala ? mancalaR : pitRadius;
                    math::NkColor col = isPlayer1 ? color1 : color2;

                    r.DrawCircle(pitCX[i], pitCY[i], r_pit, AlphaF(col, 0.30f), 24);
                    r.DrawCircleOutline(pitCX[i], pitCY[i], r_pit + 1.5f,
                                       AlphaF(col, 0.60f), 2.0f, 24);
                }

                // Animer un grain qui se déplace de pit en pit (ordre horaire)
                // Ordre : 0,1,2,3,4,5,6,13,12,11,10,9,8,7,0...
                static constexpr int kClockwise[14] = {
                    0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7
                };
                // Grain se déplace toutes les 0.4 secondes
                int currentPitIdx = (int)(mLoadingTime / 0.4f) % 14;
                const float grainCX = pitCX[kClockwise[currentPitIdx]];
                const float grainCY = pitCY[kClockwise[currentPitIdx]];

                // Dessiner le grain animé (orange pulsant)
                const float pulse = 0.7f + 0.3f * math::NkSin(mLoadingTime * 4.0f);
                r.DrawCircle(grainCX, grainCY, pitRadius * 0.6f * pulse,
                           AlphaF(theme::Orange(), 0.90f), 32);

                // Statut de chargement
                char loadStr[64];
                const int progress = (mFramesLoaded * 100) / kFrameCount;
                std::snprintf(loadStr, sizeof(loadStr), "Chargement... %d%%", progress);
                // FontAtlas n'est pas inclus ici, donc on skip le texte pour éviter crash

                r.End();
                return;
            }

            if (mFramesLoaded > 0)
            {
                // ── Choix de la frame courante ────────────────────────────
                // mCurrentFrame avance dans OnUpdate par increments de 1
                // chaque kFrameDuration sec. Plus de mapping temporel
                // (t * kFrameCount) qui pouvait sauter des frames lors de
                // freezes. On clamp simplement a la derniere frame chargee.
                int idx = mCurrentFrame;
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
            // Synchronise avec mCurrentFrame (pas mTime) : en Debug le worker
            // est lent et mTime peut largement depasser kDuration alors que
            // l'anim n'est pas finie. Si on basait le fade sur mTime, on
            // afficherait un ecran blanc COMPLET pendant que les dernieres
            // frames continuent de jouer en arriere-plan. Sur mCurrentFrame
            // le fade joue toujours sur les N dernieres frames effectivement
            // affichees, peu importe le temps reel.
            constexpr int kFadeOutFrames = 12;   // ~0.3s a 39 fps theoriques
            const int fadeStartFrame = kFrameCount - kFadeOutFrames;
            if (mCurrentFrame >= fadeStartFrame)
            {
                float a = (float)(mCurrentFrame - fadeStartFrame + 1)
                        / (float)kFadeOutFrames;
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

    } // namespace songoo
} // namespace nkentseu
