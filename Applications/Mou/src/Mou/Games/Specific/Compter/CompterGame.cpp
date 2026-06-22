// =============================================================================
// Games/Specific/Compter/CompterGame.cpp
// =============================================================================
#include "Games/Specific/Compter/CompterGame.h"
#include "Assets/MouAssets.h"
#include "Core/MouConfig.h"
#include "UI/MouUIColor.h"
#include "Games/Common/MouBackground.h"
#include "NKEvent/NkKeyboardEvent.h"
#include <cstdio>
#include <cmath>

namespace mou {

    using namespace nkentseu;
    using C = ui::MouUIColor;

    namespace {
        const char* kObjFile[CompterGame::NUM_TYPES] = {
            "fruit_mangue.svg", "fruit_tomate.svg", "fruit_avocat.svg",
            "fruit_papaye.svg", "fruit_safou.svg"
        };
        const char* kObjPlural[CompterGame::NUM_TYPES] = {
            "mangues", "tomates", "avocats", "papayes", "safous"
        };
        const char* kNombreVoice[11] = {
            "nombre_1", "nombre_1", "nombre_2", "nombre_3", "nombre_4", "nombre_5",
            "nombre_6", "nombre_7", "nombre_8", "nombre_9", "nombre_10"
        };
        inline bool InRect(float32 px, float32 py, float32 x, float32 y, float32 w, float32 h) noexcept {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    }  // namespace

    float32 CompterGame::Rand01() noexcept {
        mRng = mRng * 1664525u + 1013904223u;
        return (float32)((mRng >> 8) & 0xFFFFFF) / 16777216.f;
    }

    bool CompterGame::Init() noexcept {
        mWantExit = false;
        if (mAssets) {
            for (int32 i = 0; i < NUM_TYPES; ++i) mObjTex[i] = mAssets->LoadSvg(kObjFile[i], 200, 200);
            mMascotTex = mAssets->LoadSvg("mascot_nana.svg", 256, 256);
            mStarTex   = mAssets->LoadSvg("star_reward.svg", 220, 220);
            mTreeTex   = mAssets->LoadSvg("tree.svg", 360, 408);
            mBushTex   = mAssets->LoadSvg("bush.svg", 360, 276);
        }
        mFb.Init(mStarTex);
        mLevels.Load("compter.json");
        mLevel = 0;
        StartLevel();
        MOU_LOG_INFO("Jeu Compter initialise");
        return true;
    }

    void CompterGame::StartLevel() noexcept {
        mMax = mLevels.GetInt(mLevel, "max", 3 + mLevel);
        if (mMax > MAX_OBJ) mMax = MAX_OBJ; if (mMax < 1) mMax = 1;
        mRng = 4242u + (uint32)mLevel * 7919u;
        mCount = 1 + (int32)(Rand01() * (float32)mMax);
        if (mCount > mMax) mCount = mMax;
        if (mCount < 1) mCount = 1;
        mObjType = mLevel % NUM_TYPES;

        // Réponses : la bonne + 2 distracteurs valides distincts.
        mAnswers[0] = mCount;
        int32 got = 1;
        const int32 cand[4] = { mCount - 1, mCount + 1, mCount - 2, mCount + 2 };
        for (int32 k = 0; k < 4 && got < 3; ++k) {
            const int32 v = cand[k];
            if (v < 1 || v > 10) continue;
            bool dup = false;
            for (int32 j = 0; j < got; ++j) if (mAnswers[j] == v) dup = true;
            if (!dup) mAnswers[got++] = v;
        }
        while (got < 3) {
            const int32 v = 1 + (int32)(Rand01() * 10.f);
            bool dup = false;
            for (int32 j = 0; j < got; ++j) if (mAnswers[j] == v) dup = true;
            if (!dup && v >= 1 && v <= 10) mAnswers[got++] = v;
        }
        // Mélange (Fisher-Yates sur 3).
        for (int32 i = 2; i > 0; --i) {
            const int32 j = (int32)(Rand01() * (float32)(i + 1));
            const int32 t = mAnswers[i]; mAnswers[i] = mAnswers[j]; mAnswers[j] = t;
        }

        for (int32 i = 0; i < MAX_OBJ; ++i) mObjs[i].counted = false;
        mNeedSpawn = true;
        mFb.ResetLevel(3);
    }

    void CompterGame::Update(float32 dt) noexcept {
        mTime += dt;
        mFb.Update(dt);
        if (mFb.ReadyToAdvance()) { mLevel = (mLevel + 1) % LevelCount(); StartLevel(); }
        else if (mFb.ReadyToRetry()) { StartLevel(); }
    }

    void CompterGame::Render(const MouFrame& frame) noexcept {
        const float32 W = frame.width, H = frame.height;
        MouBackground::Draw(frame, mLevel / 3, mTime);

        const float32 objSz   = 82.f;
        const float32 cardH   = 120.f, cardW = 150.f, cardGap = 28.f;
        const float32 cardY   = H - cardH - 26.f;

        // Verger : plusieurs arbres du MÊME type (≤4 fruits/arbre), troncs au sol.
        const float32 groundY  = H * 0.64f;
        const float32 plantTop = 50.f;
        const float32 plantH   = (groundY + 30.f) - plantTop;
        const bool    bush     = (mObjType == 1);   // tomate -> buisson
        const uint32  ptex     = bush ? mBushTex : mTreeTex;
        const float32 par      = bush ? MouPlant::BUSH_AR : MouPlant::TREE_AR;
        const float32 ccyr     = bush ? 0.46f : 0.34f;
        const float32 bhr      = bush ? 0.34f : 0.42f;
        int32 nTrees = (mCount + 3) / 4; if (nTrees < 1) nTrees = 1; if (nTrees > 5) nTrees = 5;
        const float32 pm = 20.f, pgap = 14.f;
        const float32 treeW = (W - 2.f * pm - pgap * (float32)(nTrees - 1)) / (float32)nTrees;
        float32 trX[5], trY[5], trW[5], trH[5];
        for (int32 t = 0; t < nTrees; ++t) {
            const float32 px = pm + (float32)t * (treeW + pgap);
            MouPlant::Draw(frame, ptex, par, px, plantTop, treeW, plantH, trX[t], trY[t], trW[t], trH[t]);
        }
        if (mNeedSpawn && W > 1.f) {
            const int32 perTree = (mCount + nTrees - 1) / nTrees;
            for (int32 t = 0; t < nTrees; ++t) {
                const int32 start = t * perTree;
                int32 cnt = mCount - start; if (cnt > perTree) cnt = perTree; if (cnt < 0) cnt = 0;
                float32 sx[MAX_OBJ], sy[MAX_OBJ];
                MouPlant::Slots(trX[t], trY[t], trW[t], trH[t], ccyr, 0.58f, bhr, cnt, objSz, sx, sy);
                for (int32 k = 0; k < cnt; ++k) { mObjs[start + k].x = sx[k]; mObjs[start + k].y = sy[k]; }
            }
            mNeedSpawn = false;
        }

        if (mMascotTex) {
            const float32 ms = 104.f * mFb.MascotScale();
            frame.Image(mMascotTex, 16.f + (104.f - ms) * 0.5f, 16.f + (104.f - ms) * 0.5f, ms, ms);
        }
        char consigne[48];
        std::snprintf(consigne, sizeof(consigne), "Combien de %s ?", kObjPlural[mObjType]);
        frame.TextCentered(frame.titleFont, 0.f, W, 28.f, consigne, C::TEXT_DARK());

        // Tap sur un objet -> compté (étincelle).
        if (mFb.IsPlaying() && frame.pointerPressed) {
            for (int32 i = 0; i < mCount; ++i) {
                if (!mObjs[i].counted && InRect(frame.pointer.x, frame.pointer.y, mObjs[i].x, mObjs[i].y, objSz, objSz)) {
                    mObjs[i].counted = true;
                    mFb.Tap(mObjs[i].x + objSz * 0.5f, mObjs[i].y + objSz * 0.5f);
                    int32 cs = 0; for (int32 k = 0; k < mCount; ++k) if (mObjs[k].counted) ++cs;
                    if (cs > 10) cs = 10;
                    mPendingVoice = kNombreVoice[cs];   // dit "un", "deux"... au fil du comptage
                    break;
                }
            }
        }

        // Dessin des objets sur les arbres (fixes ; anneau vert si compté).
        for (int32 i = 0; i < mCount; ++i) {
            if (mObjs[i].counted)
                frame.CircleOutline(math::NkVec2f{ mObjs[i].x + objSz * 0.5f, mObjs[i].y + objSz * 0.5f },
                                    objSz * 0.58f, C::SUCCESS(), 6.f);
            if (mObjTex[mObjType]) frame.Image(mObjTex[mObjType], mObjs[i].x, mObjs[i].y, objSz, objSz);
        }

        // Cartes-réponses (chiffres).
        const float32 totalW = 3 * cardW + 2 * cardGap;
        const float32 startCX = (W - totalW) * 0.5f;
        for (int32 i = 0; i < 3; ++i) {
            const float32 cx = startCX + i * (cardW + cardGap);
            const bool over = InRect(frame.pointer.x, frame.pointer.y, cx, cardY, cardW, cardH);
            const bool hot = over && frame.pointerDown && mFb.IsPlaying();  // surligne SEULEMENT pendant l'appui
            frame.Rect(cx, cardY, cardW, cardH, hot ? C::SUNNY() : C::SURFACE(), 20.f);
            frame.Border(cx, cardY, cardW, cardH, C::INK(), 5.f, 20.f);
            char num[8]; std::snprintf(num, sizeof(num), "%d", mAnswers[i]);
            frame.TextCentered(frame.titleFont, cx, cardW, cardY + (cardH - frame.LineH(frame.titleFont)) * 0.5f,
                               num, C::TEXT_DARK());
            if (mFb.IsPlaying() && over && frame.pointerReleased) {
                const float32 ccx = cx + cardW * 0.5f, ccy = cardY + cardH * 0.5f;
                if (mAnswers[i] == mCount) { mFb.Good(ccx, ccy); mFb.Win(W); }
                else                       { mFb.Bad(ccx, ccy); }
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

    void CompterGame::OnEvent(NkEvent* event) noexcept {
        if (auto* kp = event->As<NkKeyPressEvent>()) {
            if (kp->GetKey() == NkKey::NK_ESCAPE) mWantExit = true;
        }
    }

    void CompterGame::Unload() noexcept {
        MOU_LOG_INFO("Jeu Compter decharge");
    }

}  // namespace mou
