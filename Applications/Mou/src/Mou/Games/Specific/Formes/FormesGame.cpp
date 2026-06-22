// =============================================================================
// Games/Specific/Formes/FormesGame.cpp
// =============================================================================
#include "Games/Specific/Formes/FormesGame.h"
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
        const char* kShapeFile[FormesGame::NUM_SHAPES] = {
            "shape_circle.svg", "shape_square.svg", "shape_triangle.svg",
            "shape_star.svg", "shape_heart.svg"
        };
        const char* kShapeVoice[FormesGame::NUM_SHAPES] = {
            "forme_rond", "forme_carre", "forme_triangle", "forme_etoile", "forme_coeur"
        };
        math::NkColor ShapeColor(int32 c) noexcept {
            switch (c % 5) { case 0: return C::CORAL(); case 1: return C::SUNNY();
                             case 2: return C::LEAF(); case 3: return C::SKYB(); default: return C::ORANGE(); }
        }
        inline bool InRect(float32 px, float32 py, float32 x, float32 y, float32 w, float32 h) noexcept {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    }  // namespace

    float32 FormesGame::Rand01() noexcept {
        mRng = mRng * 1664525u + 1013904223u;
        return (float32)((mRng >> 8) & 0xFFFFFF) / 16777216.f;
    }

    bool FormesGame::Init() noexcept {
        mWantExit = false;
        if (mAssets) {
            for (int32 s = 0; s < NUM_SHAPES; ++s) mShapeTex[s] = mAssets->LoadSvg(kShapeFile[s], 200, 200);
            mMascotTex = mAssets->LoadSvg("mascot_nana.svg", 256, 256);
            mStarTex   = mAssets->LoadSvg("star_reward.svg", 220, 220);
        }
        mFb.Init(mStarTex);
        mLevels.Load("formes.json");
        mLevel = 0;
        StartLevel();
        MOU_LOG_INFO("Jeu Les Formes initialise");
        return true;
    }

    void FormesGame::StartLevel() noexcept {
        mCount = mLevels.GetInt(mLevel, "shapes", 3 + (mLevel / 2));
        if (mCount > MAX_SLOTS) mCount = MAX_SLOTS;
        if (mCount < 2) mCount = 2;
        mRng = 8080u + (uint32)mLevel * 7919u;

        // Choix de mCount formes DISTINCTES (permutation des NUM_SHAPES).
        int32 order[NUM_SHAPES]; for (int32 i = 0; i < NUM_SHAPES; ++i) order[i] = i;
        for (int32 i = NUM_SHAPES - 1; i > 0; --i) {
            const int32 j = (int32)(Rand01() * (float32)(i + 1));
            const int32 t = order[i]; order[i] = order[j]; order[j] = t;
        }
        for (int32 i = 0; i < mCount; ++i) {
            mSlots[i].shape  = order[i];
            mSlots[i].filled = false;
            mPieces[i].shape = order[i];
            mPieces[i].color = i;
            mPieces[i].placed = false;
        }
        // Mélange l'ordre d'affichage des pièces (les pièces ne sont pas sous leur trou).
        for (int32 i = mCount - 1; i > 0; --i) {
            const int32 j = (int32)(Rand01() * (float32)(i + 1));
            const Piece t = mPieces[i]; mPieces[i] = mPieces[j]; mPieces[j] = t;
        }
        mPlaced    = 0;
        mDragging  = -1;
        mNeedSpawn = true;
        mFb.ResetLevel(3);
    }

    void FormesGame::Update(float32 dt) noexcept {
        mTime += dt;
        mFb.Update(dt);
        if (mFb.ReadyToAdvance()) { mLevel = (mLevel + 1) % LevelCount(); StartLevel(); }
        else if (mFb.ReadyToRetry()) { StartLevel(); }
    }

    void FormesGame::Render(const MouFrame& frame) noexcept {
        const float32 W = frame.width, H = frame.height;
        MouBackground::Draw(frame, mLevel / 3, mTime);

        const float32 slotSz  = 132.f;
        const float32 pieceSz = 104.f;
        const float32 slotY   = 170.f;
        const float32 pieceY  = H - pieceSz - 48.f;
        const bool playing = mFb.IsPlaying();

        // Placement (une fois la taille connue) : trous en haut, pièces en bas.
        if (mNeedSpawn && W > 1.f) {
            const float32 gapS = 28.f;
            const float32 totS = mCount * slotSz + (mCount - 1) * gapS;
            const float32 sx0  = (W - totS) * 0.5f;
            for (int32 i = 0; i < mCount; ++i) { mSlots[i].x = sx0 + i * (slotSz + gapS); mSlots[i].y = slotY; }
            const float32 gapP = 30.f;
            const float32 totP = mCount * pieceSz + (mCount - 1) * gapP;
            const float32 px0  = (W - totP) * 0.5f;
            for (int32 i = 0; i < mCount; ++i) {
                mPieces[i].homeX = px0 + i * (pieceSz + gapP); mPieces[i].homeY = pieceY;
                mPieces[i].x = mPieces[i].homeX; mPieces[i].y = mPieces[i].homeY;
            }
            mNeedSpawn = false;
        }

        if (mMascotTex) {
            const float32 ms = 104.f * mFb.MascotScale();
            frame.Image(mMascotTex, 16.f + (104.f - ms) * 0.5f, 16.f + (104.f - ms) * 0.5f, ms, ms);
        }
        frame.TextCentered(frame.titleFont, 0.f, W, 28.f, "Mets chaque forme a sa place !", C::TEXT_DARK());

        // Trous (silhouettes sombres) + forme posée si rempli.
        for (int32 i = 0; i < mCount; ++i) {
            const Slot& s = mSlots[i];
            frame.Rect(s.x - 8.f, s.y - 8.f, slotSz + 16.f, slotSz + 16.f, ui::WithAlpha(C::SURFACE(), 200), 22.f);
            frame.Border(s.x - 8.f, s.y - 8.f, slotSz + 16.f, slotSz + 16.f, C::INK(), 4.f, 22.f);
            if (mShapeTex[s.shape]) {
                if (s.filled) {
                    // Forme posée : on retrouve sa couleur (pièce d'origine).
                    int32 col = i;
                    for (int32 k = 0; k < mCount; ++k) if (mPieces[k].shape == s.shape) { col = mPieces[k].color; break; }
                    frame.Image(mShapeTex[s.shape], s.x, s.y, slotSz, slotSz, ShapeColor(col));
                } else {
                    frame.Image(mShapeTex[s.shape], s.x, s.y, slotSz, slotSz, ui::WithAlpha(C::INK(), 60));
                }
            }
        }

        // Saisie d'une pièce.
        if (playing && frame.pointerPressed && mDragging < 0) {
            for (int32 i = mCount - 1; i >= 0; --i) {
                if (mPieces[i].placed) continue;
                if (InRect(frame.pointer.x, frame.pointer.y, mPieces[i].x, mPieces[i].y, pieceSz, pieceSz)) {
                    mDragging = i;
                    mDragOffX = frame.pointer.x - mPieces[i].x;
                    mDragOffY = frame.pointer.y - mPieces[i].y;
                    mPendingVoice = kShapeVoice[mPieces[i].shape];   // nomme la forme saisie
                    break;
                }
            }
        }
        if (mDragging >= 0 && frame.pointerDown && playing) {
            mPieces[mDragging].x = frame.pointer.x - mDragOffX;
            mPieces[mDragging].y = frame.pointer.y - mDragOffY;
        }
        if (mDragging >= 0 && (frame.pointerReleased || !playing)) {
            Piece& p = mPieces[mDragging];
            const float32 cx = p.x + pieceSz * 0.5f, cy = p.y + pieceSz * 0.5f;
            int32 hit = -1;
            for (int32 i = 0; i < mCount; ++i)
                if (!mSlots[i].filled && InRect(cx, cy, mSlots[i].x - 8.f, mSlots[i].y - 8.f, slotSz + 16.f, slotSz + 16.f)) { hit = i; break; }
            if (hit >= 0 && mSlots[hit].shape == p.shape) {
                mSlots[hit].filled = true; p.placed = true; ++mPlaced;
                mFb.Good(mSlots[hit].x + slotSz * 0.5f, mSlots[hit].y + slotSz * 0.5f);
                if (mPlaced >= mCount) mFb.Win(W);
            } else {
                if (hit >= 0) mFb.Bad(cx, cy);     // mauvais trou -> perd une étoile
                p.x = p.homeX; p.y = p.homeY;
            }
            mDragging = -1;
        }

        // Pièces colorées (fixes), puis la pièce traînée par-dessus.
        for (int32 i = 0; i < mCount; ++i) {
            if (mPieces[i].placed || i == mDragging) continue;
            if (mShapeTex[mPieces[i].shape])
                frame.Image(mShapeTex[mPieces[i].shape], mPieces[i].x, mPieces[i].y, pieceSz, pieceSz, ShapeColor(mPieces[i].color));
        }
        if (mDragging >= 0) {
            const Piece& p = mPieces[mDragging];
            const float32 s = pieceSz * 1.15f;
            if (mShapeTex[p.shape])
                frame.Image(mShapeTex[p.shape], p.x - (s - pieceSz) * 0.5f, p.y - (s - pieceSz) * 0.5f, s, s, ShapeColor(p.color));
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

    void FormesGame::OnEvent(NkEvent* event) noexcept {
        if (auto* kp = event->As<NkKeyPressEvent>()) {
            if (kp->GetKey() == NkKey::NK_ESCAPE) mWantExit = true;
        }
    }

    void FormesGame::Unload() noexcept {
        MOU_LOG_INFO("Jeu Les Formes decharge");
    }

}  // namespace mou
