// =============================================================================
// RihenIntroScene.cpp — Intro Rihen 156 frames — Songo'o
// Adapté du RihenIntroScene.cpp du repo Nkentseu
// Chemin assets : Resources/Songoo/assets/animrihen/rihen_NNNNN.png
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

namespace nkentseu { namespace songoo {

    static float EaseOutCubic(float t) {
        if (t <= 0.f) return 0.f;
        if (t >= 1.f) return 1.f;
        float u = 1.f - t; return 1.f - u * u * u;
    }

    // ── OnEnter ───────────────────────────────────────────────────────────────
    void RihenIntroScene::OnEnter(AppContext& /*ctx*/) {
        mState = State::Loading; mLoadingTime = 0.f; mTime = 0.f;
        mDone = false; mFramesLoaded = 0; mAspect = 4.f;
        mPendingNext = 0; mCurrentFrame = 0; mFrameAccum = 0.f;
        for (int i = 0; i < kFrameCount; ++i) mPending[i] = nullptr;
        mWorkerDone.store(false);
        mWorkerLastAttempted.store(-1);

        // Logo statique pendant le chargement
        mLoadingLogo.LoadFromFile("Resources/Songoo/assets/Icon.png");

        StartWorker();
        logger.Info("[RihenIntro] OnEnter — Loading phase");
    }

    // ── OnExit ────────────────────────────────────────────────────────────────
    void RihenIntroScene::OnExit(AppContext& /*ctx*/) {
        StopWorker();
        for (int i = 0; i < kFrameCount; ++i) {
            if (mPending[i]) { mPending[i]->Free(); mPending[i] = nullptr; }
        }
        for (int i = 0; i < kFrameCount; ++i) mFrames[i].Shutdown();
        mFramesLoaded = 0;
        mLoadingLogo.Shutdown();
    }

    // ── Worker ────────────────────────────────────────────────────────────────
    void RihenIntroScene::StartWorker() {
        mWorkerStop.store(false);
        mWorker = std::thread(&RihenIntroScene::WorkerProc, this);
    }

    void RihenIntroScene::StopWorker() {
        mWorkerStop.store(true);
        if (mWorker.joinable()) mWorker.join();
    }

    void RihenIntroScene::WorkerProc() {
        char path[256];
        int missing = 0;
        for (int i = 0; i < kFrameCount; ++i) {
            if (mWorkerStop.load()) break;

            // Renommage : rihen_00000.png … rihen_00155.png
            std::snprintf(path, sizeof(path),
                "Resources/Songoo/assets/animrihen/rihen_%05d.png", i);

            NkImage* img = Texture2D::DecodeFromFile(path);
            if (!img) {
                ++missing;
                mWorkerLastAttempted.store(i);
                logger.Warn("[RihenIntro] Decode FAIL i={} path={}", i, path);
                continue;
            }
            { std::lock_guard<std::mutex> lk(mQueueMutex); mPending[i] = img; }
            mWorkerLastAttempted.store(i);
            if ((i + 1) % 30 == 0)
                logger.Info("[RihenIntro] Worker {}/{} ({} miss)", i+1, kFrameCount, missing);
        }
        mWorkerDone.store(true);
        logger.Info("[RihenIntro] Worker done — {} miss / {}", missing, kFrameCount);
    }

    int RihenIntroScene::DrainQueue(int maxUploads) {
        int uploaded = 0;
        while (uploaded < maxUploads && mPendingNext < kFrameCount) {
            NkImage* img = nullptr;
            {
                std::lock_guard<std::mutex> lk(mQueueMutex);
                img = mPending[mPendingNext];
            }
            // Pas encore décodée et worker pas encore arrivé là
            if (!img && !mWorkerDone.load() &&
                mWorkerLastAttempted.load() < mPendingNext)
                break;

            if (img) {
                bool ok = mFrames[mPendingNext].UploadFromImage(img);
                if (ok && mPendingNext == 0)
                    mAspect = (float)mFrames[0].Width() /
                              (float)(mFrames[0].Height() > 0 ? mFrames[0].Height() : 1);
                if (ok) { ++mFramesLoaded; ++uploaded; }
                {
                    std::lock_guard<std::mutex> lk(mQueueMutex);
                    mPending[mPendingNext] = nullptr;
                }
            }
            ++mPendingNext;
        }
        return uploaded;
    }

    // ── OnUpdate ──────────────────────────────────────────────────────────────
    void RihenIntroScene::OnUpdate(AppContext& ctx, float dt) {
        if (mState == State::Loading) {
            mLoadingTime += dt;
            DrainQueue(kUploadsPerUpdateLoading);
            if (mFramesLoaded >= kFrameCount) {
                mState = State::Playing;
                mTime  = 0.f;
                logger.Info("[RihenIntro] Loading done in {:.2}s -> Playing", mLoadingTime);
            }
            return;
        }
        if (mState != State::Playing) return;

        mTime += dt;
        float t = (kDuration > 0.f) ? (mTime / kDuration) : 1.f;
        if (t < 0.f) t = 0.f; if (t > 1.f) t = 1.f;
        int idx = (int)(t * (float)(kFrameCount - 1) + 0.5f);
        if (idx >= kFrameCount) idx = kFrameCount - 1;
        if (idx >= mFramesLoaded) idx = mFramesLoaded - 1;
        if (idx < 0) idx = 0;
        mCurrentFrame = idx;

        if (!mDone && mCurrentFrame >= kFrameCount - 1) {
            mDone = true; mState = State::Done;
            ctx.scenes->Replace(new NogeIntroScene());
        }
    }

    // ── OnRender ──────────────────────────────────────────────────────────────
    void RihenIntroScene::OnRender(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        const int W = ctx.viewportW, H = ctx.viewportH;
        const float cx = ctx.safe.SafeCX(), cy = ctx.safe.SafeCY();

        // Fond blanc (logo Rihen sur fond blanc)
        r.Clear(1.f, 1.f, 1.f, 1.f);
        r.Begin(W, H);

        if (mState == State::Loading) {
            // Plateau Mancala animé pendant le chargement
            const float safeW = ctx.safe.SafeW();
            const float safeH = ctx.safe.SafeH();
            const float boardW = safeW * 0.80f;
            const float boardH = boardW * 0.50f;
            const float boardX = cx - boardW * 0.5f;
            const float boardY = cy - boardH * 0.5f;

            float pitCX[14], pitCY[14];
            const float pitR     = boardW / 25.f;
            const float mancalaR = pitR * 1.5f;
            const float row1Y    = boardY + boardH * 0.65f;
            const float col1X    = boardX + boardW * 0.05f;
            const float colSpacing = boardW / 8.f;

            for (int i = 0; i < 6; ++i) {
                pitCX[i] = col1X + (float)i * colSpacing;
                pitCY[i] = row1Y;
            }
            pitCX[6] = boardX + boardW * 0.05f; pitCY[6] = row1Y;
            const float row2Y = boardY + boardH * 0.35f;
            for (int i = 7; i < 13; ++i) {
                pitCX[i] = col1X + (float)(12 - i) * colSpacing;
                pitCY[i] = row2Y;
            }
            pitCX[12] = boardX + boardW * 0.95f; pitCY[12] = row1Y;
            pitCX[13] = boardX + boardW * 0.95f; pitCY[13] = row2Y;

            static constexpr int kCW14[14] = {0,1,2,3,4,5,6,13,12,11,10,9,8,7};

            for (int i = 0; i < 14; ++i) {
                bool isMancala = (i == 12 || i == 13);
                bool isP1      = (i < 7 || i == 12);
                float rp = isMancala ? mancalaR : pitR;
                math::NkColor col = isP1 ? theme::Orange() : theme::Cyan();
                r.DrawCircle(pitCX[i], pitCY[i], rp, AlphaF(col, 0.30f), 24);
                r.DrawCircleOutline(pitCX[i], pitCY[i], rp+1.5f, AlphaF(col, 0.60f), 2.f, 24);
            }

            // Grain se déplaçant dans l'ordre horaire
            int grainIdx = (int)(mLoadingTime / 0.4f) % 14;
            float gpx = pitCX[kCW14[grainIdx]];
            float gpy = pitCY[kCW14[grainIdx]];
            float pulse = 0.7f + 0.3f * math::NkSin(mLoadingTime * 4.f);
            r.DrawCircle(gpx, gpy, pitR * 0.6f * pulse, AlphaF(theme::Orange(), 0.90f), 32);

            // Logo RIHEN centré (si chargé)
            if (mLoadingLogo.IsValid()) {
                float lw = boardW * 0.20f, lh = lw;
                r.BindTexture(mLoadingLogo.Id());
                r.DrawTexturedQuadRGBA(cx - lw*0.5f, boardY - lh*1.2f, lw, lh,
                                       0.f, 0.f, 1.f, 1.f, theme::White());
            }

            // Barre de progression
            int pct = (mFramesLoaded * 100) / kFrameCount;
            float barW = boardW * 0.7f;
            float barH = 6.f;
            float barX = cx - barW * 0.5f;
            float barY = boardY + boardH + 20.f;
            r.DrawQuad(barX, barY, barW, barH, { 60, 50, 40, 200 });
            r.DrawQuad(barX, barY, barW * pct / 100.f, barH, theme::Orange());

            r.End();
            return;
        }

        // ── Phase Playing : frame courante ────────────────────────────────────
        if (mFramesLoaded > 0) {
            int idx = mCurrentFrame;
            if (idx >= mFramesLoaded) idx = mFramesLoaded - 1;
            if (idx < 0) idx = 0;

            const float safeW = ctx.safe.SafeW();
            const float safeH = ctx.safe.SafeH();
            const float maxW  = safeW * 0.85f;
            const float maxH  = safeH * 0.85f;
            float drawW = maxW;
            float drawH = (mAspect > 0.0001f) ? (drawW / mAspect) : maxH;
            if (drawH > maxH) { drawH = maxH; drawW = drawH * mAspect; }
            float drawX = cx - drawW * 0.5f;
            float drawY = cy - drawH * 0.5f;

            r.BindTexture(mFrames[idx].Id());
            r.DrawTexturedQuadRGBA(drawX, drawY, drawW, drawH,
                                   0.f, 0.f, 1.f, 1.f, theme::White());
        }

        // Fade-out blanc sur les 12 dernières frames
        constexpr int kFadeFrames = 12;
        const int fadeStart = kFrameCount - kFadeFrames;
        if (mCurrentFrame >= fadeStart) {
            float a = (float)(mCurrentFrame - fadeStart + 1) / (float)kFadeFrames;
            a = EaseOutCubic(a < 0.f ? 0.f : a > 1.f ? 1.f : a);
            math::NkColor wf = theme::White();
            wf.a = (uint8_t)(255.f * a);
            r.DrawQuad(0.f, 0.f, (float)W, (float)H, wf);
        }

        r.End();
    }

}} // namespace nkentseu::songoo
