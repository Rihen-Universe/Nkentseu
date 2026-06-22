// =============================================================================
// Games/Specific/Memoire/MemoireGame.cpp
// =============================================================================
#include "Games/Specific/Memoire/MemoireGame.h"
#include "Assets/MouAssets.h"
#include "Core/MouConfig.h"
#include "UI/MouUIColor.h"
#include "Games/Common/MouBackground.h"
#include "NKEvent/NkKeyboardEvent.h"
#include <cmath>

namespace mou {

    using namespace nkentseu;
    using C = ui::MouUIColor;

    namespace {
        const char* kFaceFile[MemoireGame::NUM_FACES] = {
            "fruit_mangue.svg", "fruit_tomate.svg", "fruit_avocat.svg",
            "fruit_safou.svg", "fruit_papaye.svg", "fruit_plantain.svg"
        };
        math::NkColor BackTint(int32 i) noexcept {
            switch (i % 4) { case 0: return C::SKYB(); case 1: return C::CORAL();
                             case 2: return C::LEAF(); default: return C::ORANGE(); }
        }
        inline bool InRect(float32 px, float32 py, float32 x, float32 y, float32 w, float32 h) noexcept {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    }  // namespace

    float32 MemoireGame::Rand01() noexcept {
        mRng = mRng * 1664525u + 1013904223u;
        return (float32)((mRng >> 8) & 0xFFFFFF) / 16777216.f;
    }

    bool MemoireGame::Init() noexcept {
        mWantExit = false;
        if (mAssets) {
            for (int32 f = 0; f < NUM_FACES; ++f) mFaceTex[f] = mAssets->LoadSvg(kFaceFile[f], 200, 200);
            mMascotTex = mAssets->LoadSvg("mascot_nana.svg", 256, 256);
            mStarTex   = mAssets->LoadSvg("star_reward.svg", 220, 220);
        }
        mFb.Init(mStarTex);
        mLevels.Load("memoire.json");
        mLevel = 0;
        StartLevel();
        MOU_LOG_INFO("Jeu La Memoire initialise");
        return true;
    }

    void MemoireGame::StartLevel() noexcept {
        mPairs = mLevels.GetInt(mLevel, "pairs", 3 + (mLevel / 2));
        if (mPairs > MAX_PAIRS) mPairs = MAX_PAIRS;
        if (mPairs < 2) mPairs = 2;
        mCardCount = mPairs * 2;
        mRng = 313u + (uint32)mLevel * 7919u;

        // Choisit mPairs faces DISTINCTES + en pose 2 exemplaires.
        int32 order[NUM_FACES]; for (int32 i = 0; i < NUM_FACES; ++i) order[i] = i;
        for (int32 i = NUM_FACES - 1; i > 0; --i) {
            const int32 j = (int32)(Rand01() * (float32)(i + 1));
            const int32 t = order[i]; order[i] = order[j]; order[j] = t;
        }
        for (int32 p = 0; p < mPairs; ++p) {
            mCards[2 * p].face     = order[p]; mCards[2 * p].up = false;     mCards[2 * p].matched = false;
            mCards[2 * p + 1].face = order[p]; mCards[2 * p + 1].up = false; mCards[2 * p + 1].matched = false;
        }
        // Mélange du plateau (Fisher-Yates).
        for (int32 i = mCardCount - 1; i > 0; --i) {
            const int32 j = (int32)(Rand01() * (float32)(i + 1));
            const Card t = mCards[i]; mCards[i] = mCards[j]; mCards[j] = t;
        }
        // Apercu : toutes les cartes sont montrees au depart, puis se cachent.
        for (int32 i = 0; i < mCardCount; ++i) mCards[i].up = true;
        mFlipA = mFlipB = -1;
        mFound = 0;
        mHideTimer = 0.f;
        mPreviewTimer = 1.8f;   // ~1,8 s pour memoriser avant de cacher
        mFb.ResetLevel(3);
    }

    void MemoireGame::Update(float32 dt) noexcept {
        mTime += dt;
        mFb.Update(dt);
        // Aperçu de départ : on cache toutes les cartes une fois le temps écoulé.
        if (mPreviewTimer > 0.f) {
            mPreviewTimer -= dt;
            if (mPreviewTimer <= 0.f)
                for (int32 i = 0; i < mCardCount; ++i)
                    if (!mCards[i].matched) mCards[i].up = false;
        }
        // Recacher une mauvaise paire après le délai d'observation.
        if (mHideTimer > 0.f) {
            mHideTimer -= dt;
            if (mHideTimer <= 0.f) {
                if (mFlipA >= 0) mCards[mFlipA].up = false;
                if (mFlipB >= 0) mCards[mFlipB].up = false;
                mFlipA = mFlipB = -1;
            }
        }
        if (mFb.ReadyToAdvance()) { mLevel = (mLevel + 1) % LevelCount(); StartLevel(); }
        else if (mFb.ReadyToRetry()) { StartLevel(); }
    }

    void MemoireGame::Render(const MouFrame& frame) noexcept {
        const float32 W = frame.width, H = frame.height;
        MouBackground::Draw(frame, mLevel / 3, mTime);
        const bool playing = mFb.IsPlaying();

        if (mMascotTex) {
            const float32 ms = 104.f * mFb.MascotScale();
            frame.Image(mMascotTex, 16.f + (104.f - ms) * 0.5f, 16.f + (104.f - ms) * 0.5f, ms, ms);
        }
        frame.TextCentered(frame.titleFont, 0.f, W, 28.f, "Retrouve les paires !", C::TEXT_DARK());

        // Grille : colonnes selon le nombre de cartes.
        const int32 cols = (mCardCount <= 4) ? 2 : (mCardCount <= 6) ? 3 : 4;
        const int32 rows = (mCardCount + cols - 1) / cols;
        const float32 areaX = 60.f, areaTop = 130.f;
        const float32 areaW = W - 120.f, areaH = (H - 110.f) - areaTop;
        const float32 gap = 18.f;
        const float32 cw = (areaW - (cols - 1) * gap) / (float32)cols;
        const float32 ch = (areaH - (rows - 1) * gap) / (float32)rows;
        const float32 cell = (cw < ch) ? cw : ch;
        const float32 gridW = cols * cell + (cols - 1) * gap;
        const float32 gridH = rows * cell + (rows - 1) * gap;
        const float32 gx0 = areaX + (areaW - gridW) * 0.5f;
        const float32 gy0 = areaTop + (areaH - gridH) * 0.5f;

        const bool canFlip = playing && mHideTimer <= 0.f && mPreviewTimer <= 0.f;

        for (int32 i = 0; i < mCardCount; ++i) {
            const int32 r = i / cols, c = i % cols;
            const float32 x = gx0 + c * (cell + gap);
            const float32 y = gy0 + r * (cell + gap);
            Card& card = mCards[i];
            const bool over = InRect(frame.pointer.x, frame.pointer.y, x, y, cell, cell);

            frame.Rect(x, y + 6.f, cell, cell, ui::WithAlpha(C::INK(), 50), 18.f);
            if (card.matched) {
                frame.Rect(x, y, cell, cell, ui::WithAlpha(C::SUCCESS(), 70), 18.f);
                frame.Border(x, y, cell, cell, C::SUCCESS(), 5.f, 18.f);
                const float32 is = cell * 0.74f;
                if (mFaceTex[card.face]) frame.Image(mFaceTex[card.face], x + (cell - is) * 0.5f, y + (cell - is) * 0.5f, is, is);
            } else if (card.up) {
                frame.Rect(x, y, cell, cell, C::SURFACE(), 18.f);
                frame.Border(x, y, cell, cell, C::INK(), 5.f, 18.f);
                const float32 is = cell * 0.74f;
                if (mFaceTex[card.face]) frame.Image(mFaceTex[card.face], x + (cell - is) * 0.5f, y + (cell - is) * 0.5f, is, is);
            } else {
                // Dos de carte (couleur + "?").
                frame.Rect(x, y, cell, cell, over && canFlip ? C::SUNNY() : BackTint(i), 18.f);
                frame.Border(x, y, cell, cell, C::INK(), 5.f, 18.f);
                frame.TextCentered(frame.titleFont, x, cell, y + (cell - frame.LineH(frame.titleFont)) * 0.5f, "?", C::TEXT_ON_COLOR());
            }

            // Retourner une carte.
            if (canFlip && over && frame.pointerReleased && !card.up && !card.matched && mFlipB < 0) {
                card.up = true;
                mFb.Tap(x + cell * 0.5f, y + cell * 0.5f);
                if (mFlipA < 0) {
                    mFlipA = i;
                } else if (i != mFlipA) {
                    mFlipB = i;
                    if (mCards[mFlipA].face == mCards[mFlipB].face) {
                        mCards[mFlipA].matched = true; mCards[mFlipB].matched = true;
                        mFb.Good(x + cell * 0.5f, y + cell * 0.5f);
                        mFlipA = mFlipB = -1;
                        ++mFound;
                        if (mFound >= mPairs) mFb.Win(W);
                    } else {
                        mHideTimer = 0.9f;   // laisse l'enfant mémoriser puis recache
                    }
                }
            }
        }

        // Effets + HUD étoiles + overlay.
        mFb.RenderFx(frame);
        mFb.RenderHud(frame);
        mFb.RenderOverlay(frame);

        const float32 bw = 220.f, bh = 92.f;
        const math::NkColor border = C::INK();
        if (frame.Button(24.f, H - bh - 24.f, bw, bh, "Retour", C::CORAL(), C::ORANGE(),
                         C::TEXT_ON_COLOR(), &border, frame.titleFont)) {
            mWantExit = true;
        }
    }

    void MemoireGame::OnEvent(NkEvent* event) noexcept {
        if (auto* kp = event->As<NkKeyPressEvent>()) {
            if (kp->GetKey() == NkKey::NK_ESCAPE) mWantExit = true;
        }
    }

    void MemoireGame::Unload() noexcept {
        MOU_LOG_INFO("Jeu La Memoire decharge");
    }

}  // namespace mou
