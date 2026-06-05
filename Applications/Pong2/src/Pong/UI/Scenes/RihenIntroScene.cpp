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
                // ── Calcul taille/pos du logo (centre safe area) ────────────
                const float safeW = static_cast<float>(ctx.safe.SafeW());
                const float safeH = static_cast<float>(ctx.safe.SafeH());
                float logoW = 0.0f, logoH = 0.0f;
                float logoCX = cx, logoCY = cy;   // defaut centre safe
                if (mLoadingLogo.IsValid())
                {
                    const float logoMaxW = safeW * 0.35f;
                    const float logoMaxH = safeH * 0.35f;
                    const float aspect   = mLoadingLogo.AspectRatio();
                    logoW = logoMaxW;
                    logoH = (aspect > 0.0001f) ? (logoW / aspect) : logoMaxH;
                    if (logoH > logoMaxH) { logoH = logoMaxH; logoW = logoH * aspect; }
                    logoCX = cx;
                    logoCY = cy;
                    r.BindTexture(mLoadingLogo.Id());
                    r.DrawTexturedQuadRGBA(logoCX - logoW * 0.5f,
                                           logoCY - logoH * 0.5f,
                                           logoW, logoH,
                                           0.0f, 0.0f, 1.0f, 1.0f,
                                           theme::White());
                }

                // ── Spinner thematique : balle Pong qui orbite AUTOUR du logo
                // Implementation :
                //  - Rayon orbite = max(logoW, logoH) / 2 + marge -> la balle
                //    contourne le logo sans le toucher.
                //  - 1 disque plein cyan = la balle (devant)
                //  - 10 disques plus petits = trail, alpha + taille decroissants
                //    aux angles precedents.
                const float minDim    = math::NkMin(safeW, safeH);
                const float ballRadius = minDim * 0.018f;
                // Rayon d'orbite : juste a l'exterieur du logo (avec marge).
                // Si pas de logo (loadingicon manquant) : rayon = 8% du minDim.
                float orbitR = minDim * 0.08f;
                if (logoW > 0.0f && logoH > 0.0f)
                {
                    const float halfMaxLogo = math::NkMax(logoW, logoH) * 0.5f;
                    orbitR = halfMaxLogo + ballRadius * 3.0f;
                }
                const float spinnerCX = logoCX;
                const float spinnerCY = logoCY;
                constexpr float kTwoPi = 6.28318530718f;
                // Vitesse : 1 tour par 1.0s = sensation rapide cohrente avec
                // la balle Pong qui rebondit.
                const float currentAngle = mLoadingTime * kTwoPi;
                constexpr int kTrailLength = 10;
                constexpr float kAngleStep = 0.10f;  // ~5.7 deg entre segments
                // On dessine du PLUS LOIN dans le passe (i=trailLength-1) vers
                // le PRESENT (i=0) pour que la balle finale couvre le trail.
                // Fond BLANC -> on utilise cyan (couleur P1 Pong) pour la
                // balle, sinon elle disparait visuellement.
                const math::NkColor ballColor = theme::Cyan();
                for (int i = kTrailLength - 1; i >= 1; --i)
                {
                    const float ang = currentAngle - (float)i * kAngleStep;
                    const float bx  = spinnerCX + math::NkCos(ang) * orbitR;
                    const float by  = spinnerCY + math::NkSin(ang) * orbitR;
                    // Taille + alpha decroissant en s'eloignant.
                    const float t   = (float)i / (float)kTrailLength;
                    const float rad = ballRadius * (1.0f - t * 0.6f);
                    math::NkColor col = ballColor;
                    col.a = static_cast<uint8>(255.0f * (1.0f - t));
                    r.DrawCircle(bx, by, rad, col, 24);
                }
                // La balle (devant) : pleine, cyan, opaque.
                const float bx = spinnerCX + math::NkCos(currentAngle) * orbitR;
                const float by = spinnerCY + math::NkSin(currentAngle) * orbitR;
                r.DrawCircle(bx, by, ballRadius, ballColor, 32);

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

    } // namespace pong
} // namespace nkentseu
