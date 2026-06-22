// =============================================================================
// Games/Specific/Couleurs/CouleursGame.cpp
// =============================================================================
#include "Games/Specific/Couleurs/CouleursGame.h"
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
        const char* kBasketFile[CouleursGame::NUM_COLORS] = {
            "basket_red.svg", "basket_yellow.svg", "basket_green.svg",
            "basket_blue.svg", "basket_orange.svg"
        };
        const char* kFruitFile[CouleursGame::NUM_COLORS] = {
            "fruit_tomate.svg", "fruit_mangue.svg", "fruit_avocat.svg",
            "fruit_safou.svg", "fruit_papaye.svg"
        };
        const char* kFruitName[CouleursGame::NUM_COLORS] = {
            "Tomate", "Mangue", "Avocat", "Safou", "Papaye"
        };
        const char* kFruitVoice[CouleursGame::NUM_COLORS] = {
            "fruit_tomate", "fruit_mangue", "fruit_avocat", "fruit_safou", "fruit_papaye"
        };
        math::NkColor ColorVal(int32 c) noexcept {
            switch (c) { case 0: return C::CORAL(); case 1: return C::SUNNY();
                         case 2: return C::LEAF(); case 3: return C::SKYB(); default: return C::ORANGE(); }
        }
        inline bool InRect(float32 px, float32 py, float32 x, float32 y, float32 w, float32 h) noexcept {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    }  // namespace

    float32 CouleursGame::Rand01() noexcept {
        mRng = mRng * 1664525u + 1013904223u;
        return (float32)((mRng >> 8) & 0xFFFFFF) / 16777216.f;
    }

    bool CouleursGame::Init() noexcept {
        mWantExit = false;
        if (mAssets) {
            for (int32 c = 0; c < NUM_COLORS; ++c) {
                mBasketTex[c] = mAssets->LoadSvg(kBasketFile[c], 256, 256);
                mFruitTex[c]  = mAssets->LoadSvg(kFruitFile[c],  220, 220);
            }
            mMascotTex = mAssets->LoadSvg("mascot_nana.svg", 256, 256);
            mStarTex   = mAssets->LoadSvg("star_reward.svg", 220, 220);
            mTreeTex   = mAssets->LoadSvg("tree.svg", 360, 408);
            mBushTex   = mAssets->LoadSvg("bush.svg", 360, 276);
        }
        mFb.Init(mStarTex);
        mLevels.Load("couleurs.json");   // niveaux pilotés par fichier (fallback code si absent)
        mLevel = 0;
        StartLevel();
        MOU_LOG_INFO("Jeu Les Couleurs initialise");
        return true;
    }

    void CouleursGame::StartLevel() noexcept {
        mActiveColors = mLevels.GetInt(mLevel, "colors", 2 + mLevel);
        if (mActiveColors > NUM_COLORS) mActiveColors = NUM_COLORS;
        if (mActiveColors < 1) mActiveColors = 1;
        mFruitCount = mLevels.GetInt(mLevel, "fruits", 3 + mLevel);
        if (mFruitCount > MAX_FRUITS) mFruitCount = MAX_FRUITS;
        if (mFruitCount < 1) mFruitCount = 1;
        mRng = 1234u + (uint32)mLevel * 7919u;
        // Couleurs en round-robin : chaque couleur active est présente et équilibrée.
        for (int32 i = 0; i < mFruitCount; ++i) {
            mFruits[i].color = i % mActiveColors;
            mFruits[i].sorted = false;
        }
        // Ordre des plantes mélangé (la plante d'une couleur n'est pas au-dessus de son panier).
        for (int32 c = 0; c < mActiveColors; ++c) mPlantColor[c] = c;
        for (int32 c = mActiveColors - 1; c > 0; --c) {
            const int32 j = (int32)(Rand01() * (float32)(c + 1));
            const int32 t = mPlantColor[c]; mPlantColor[c] = mPlantColor[j]; mPlantColor[j] = t;
        }
        mSortedCount = 0;
        mNeedSpawn   = true;
        mDragging    = -1;
        mFb.ResetLevel(3);
    }

    void CouleursGame::Update(float32 dt) noexcept {
        mTime += dt;
        mFb.Update(dt);
        if (mFb.ReadyToAdvance()) { mLevel = (mLevel + 1) % LevelCount(); StartLevel(); }
        else if (mFb.ReadyToRetry()) { StartLevel(); }   // rejoue le même niveau
    }

    void CouleursGame::Render(const MouFrame& frame) noexcept {
        const float32 W = frame.width, H = frame.height;
        MouBackground::Draw(frame, mLevel / 3, mTime);   // décor du monde (change tous les 3 niveaux)

        const float32 fruitSz  = 96.f;
        const float32 basketSz = 150.f;
        const float32 basketY  = H - basketSz - 24.f;
        const bool playing = mFb.IsPlaying();

        // ── UNE plante par couleur (un type de fruit ne se mélange pas) ──
        // Rouge=tomate -> buisson ; les autres -> arbre. Ordre des plantes mélangé.
        const int32 nP = mActiveColors;
        const float32 pm = 20.f, pgap = 14.f;
        const float32 groundY = H * 0.64f;             // doit matcher MouBackground
        const float32 plantTop = 50.f;
        const float32 plantH = (groundY + 30.f) - plantTop;  // bas DANS l'herbe -> tronc planté au sol
        const float32 plantW = (W - 2.f * pm - pgap * (float32)(nP - 1)) / (float32)(nP > 0 ? nP : 1);
        float32 prx[NUM_COLORS], pry[NUM_COLORS], prw[NUM_COLORS], prh[NUM_COLORS];
        for (int32 p = 0; p < nP; ++p) {
            const int32 col = mPlantColor[p];
            const bool bush = (col == 0);
            const float32 px = pm + (float32)p * (plantW + pgap);
            MouPlant::Draw(frame, bush ? mBushTex : mTreeTex,
                           bush ? MouPlant::BUSH_AR : MouPlant::TREE_AR,
                           px, plantTop, plantW, plantH, prx[p], pry[p], prw[p], prh[p]);
        }
        if (mNeedSpawn && W > 1.f) {
            for (int32 p = 0; p < nP; ++p) {
                const int32 col = mPlantColor[p];
                const bool bush = (col == 0);
                int32 idxs[MAX_FRUITS]; int32 cnt = 0;
                for (int32 i = 0; i < mFruitCount; ++i) if (mFruits[i].color == col) idxs[cnt++] = i;
                float32 sx[MAX_FRUITS], sy[MAX_FRUITS];
                MouPlant::Slots(prx[p], pry[p], prw[p], prh[p],
                                bush ? 0.46f : 0.34f, 0.58f, bush ? 0.34f : 0.42f,
                                cnt, fruitSz, sx, sy);
                for (int32 k = 0; k < cnt; ++k) {
                    mFruits[idxs[k]].homeX = sx[k]; mFruits[idxs[k]].homeY = sy[k];
                    mFruits[idxs[k]].x = sx[k];     mFruits[idxs[k]].y = sy[k];
                }
            }
            mNeedSpawn = false;
        }

        if (mMascotTex) {
            const float32 ms = 104.f * mFb.MascotScale();
            frame.Image(mMascotTex, 16.f + (104.f - ms) * 0.5f, 16.f + (104.f - ms) * 0.5f, ms, ms);
        }
        frame.TextCentered(frame.titleFont, 0.f, W, 28.f,
                           "Cueille les fruits et range-les !", C::TEXT_DARK());

        // Paniers.
        float32 basketX[NUM_COLORS];
        const float32 gap = 24.f;
        const float32 total = mActiveColors * basketSz + (mActiveColors - 1) * gap;
        const float32 startX = (W - total) * 0.5f;
        for (int32 c = 0; c < mActiveColors; ++c) {
            basketX[c] = startX + c * (basketSz + gap);
            if (mBasketTex[c]) frame.Image(mBasketTex[c], basketX[c], basketY, basketSz, basketSz);
            else { frame.Rect(basketX[c], basketY, basketSz, basketSz, ColorVal(c), 18.f); }
        }

        // Entrée (uniquement en jeu).
        if (playing && frame.pointerPressed && mDragging < 0) {
            for (int32 i = mFruitCount - 1; i >= 0; --i) {
                if (mFruits[i].sorted) continue;
                if (InRect(frame.pointer.x, frame.pointer.y, mFruits[i].x, mFruits[i].y, fruitSz, fruitSz)) {
                    mDragging = i;
                    mDragOffX = frame.pointer.x - mFruits[i].x;
                    mDragOffY = frame.pointer.y - mFruits[i].y;
                    mPendingVoice = kFruitVoice[mFruits[i].color];   // nomme le fruit cueilli
                    break;
                }
            }
        }
        if (mDragging >= 0 && frame.pointerDown && playing) {
            mFruits[mDragging].x = frame.pointer.x - mDragOffX;
            mFruits[mDragging].y = frame.pointer.y - mDragOffY;
        }
        if (mDragging >= 0 && (frame.pointerReleased || !playing)) {
            FruitItem& f = mFruits[mDragging];
            const float32 cx = f.x + fruitSz * 0.5f, cy = f.y + fruitSz * 0.5f;
            int32 matched = -1;
            for (int32 c = 0; c < mActiveColors; ++c)
                if (InRect(cx, cy, basketX[c], basketY, basketSz, basketSz)) { matched = c; break; }
            if (matched == f.color) {
                f.sorted = true; ++mSortedCount;
                mFb.Good(basketX[matched] + basketSz * 0.5f, basketY + basketSz * 0.4f);  // sous-mission réussie
                if (mSortedCount >= mFruitCount) mFb.Win(W);                              // mission réussie
            } else {
                if (matched >= 0) mFb.Bad(cx, cy);   // mauvais panier -> perd une étoile
                f.x = f.homeX; f.y = f.homeY;         // le fruit retourne sur sa branche
            }
            mDragging = -1;
        }

        // Fruits accrochés sur l'arbre (fixes ; pas de mouvement autonome).
        for (int32 i = 0; i < mFruitCount; ++i) {
            if (mFruits[i].sorted || i == mDragging) continue;
            if (mFruitTex[mFruits[i].color])
                frame.Image(mFruitTex[mFruits[i].color], mFruits[i].x, mFruits[i].y, fruitSz, fruitSz);
        }
        if (mDragging >= 0) {
            const FruitItem& f = mFruits[mDragging];
            const float32 s = fruitSz * 1.15f;
            if (mFruitTex[f.color])
                frame.Image(mFruitTex[f.color], f.x - (s - fruitSz) * 0.5f, f.y - (s - fruitSz) * 0.5f, s, s);
            frame.TextCentered(frame.font, f.x - 40.f, fruitSz + 80.f, f.y + s, kFruitName[f.color], C::TEXT_DARK());
        }

        // Effets + HUD étoiles + overlay victoire/échec.
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

    void CouleursGame::OnEvent(NkEvent* event) noexcept {
        if (auto* kp = event->As<NkKeyPressEvent>()) {
            if (kp->GetKey() == NkKey::NK_ESCAPE) mWantExit = true;
        }
    }

    void CouleursGame::Unload() noexcept {
        MOU_LOG_INFO("Jeu Les Couleurs decharge");
    }

}  // namespace mou
