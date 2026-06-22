// =============================================================================
// Games/Specific/Calcul/CalculGame.cpp
// =============================================================================
#include "Games/Specific/Calcul/CalculGame.h"
#include "Assets/MouAssets.h"
#include "Core/MouConfig.h"
#include "UI/MouUIColor.h"
#include "Games/Common/MouBackground.h"
#include "NKEvent/NkKeyboardEvent.h"
#include <cstdio>
#include <cmath>
#include <cstring>

namespace mou {

    using namespace nkentseu;
    using C = ui::MouUIColor;

    namespace {
        const char* kObjFile[CalculGame::NUM_TYPES] = {
            "fruit_mangue.svg", "fruit_tomate.svg", "fruit_papaye.svg",
            "fruit_avocat.svg", "fruit_safou.svg"
        };
        const char* kObjPlural[CalculGame::NUM_TYPES] = {
            "mangues", "tomates", "papayes", "avocats", "safous"
        };
        inline bool InRect(float32 px, float32 py, float32 x, float32 y, float32 w, float32 h) noexcept {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    }  // namespace

    float32 CalculGame::Rand01() noexcept {
        mRng = mRng * 1664525u + 1013904223u;
        return (float32)((mRng >> 8) & 0xFFFFFF) / 16777216.f;
    }

    bool CalculGame::Init() noexcept {
        mWantExit = false;
        if (mAssets) {
            for (int32 i = 0; i < NUM_TYPES; ++i) mObjTex[i] = mAssets->LoadSvg(kObjFile[i], 200, 200);
            mMascotTex = mAssets->LoadSvg("mascot_nana.svg", 256, 256);
            mStarTex   = mAssets->LoadSvg("star_reward.svg", 220, 220);
            mTreeTex   = mAssets->LoadSvg("tree.svg", 360, 408);
            mBushTex   = mAssets->LoadSvg("bush.svg", 360, 276);
        }
        mFb.Init(mStarTex);
        mLevels.Load("calcul.json");
        mLevel = 0;
        StartLevel();
        MOU_LOG_INFO("Jeu Calculs initialise");
        return true;
    }

    void CalculGame::StartLevel() noexcept {
        mRng = 31337u + (uint32)mLevel * 7919u;
        int32 maxR = mLevels.GetInt(mLevel, "max", 3 + mLevel);
        if (maxR > 10) maxR = 10; if (maxR < 2) maxR = 2;

        // Opération : depuis le JSON ("add"/"sub"/"mix"), sinon progression par défaut.
        char op[8];
        if (mLevels.GetStr(mLevel, "op", op, sizeof(op))) {
            if      (std::strcmp(op, "add") == 0) mSub = false;
            else if (std::strcmp(op, "sub") == 0) mSub = true;
            else                                  mSub = (Rand01() < 0.5f);  // "mix"
        } else {
            if (mLevel < 3)      mSub = false;
            else if (mLevel < 5) mSub = true;
            else                 mSub = (Rand01() < 0.5f);
        }

        if (!mSub) {
            mResult = 2 + (int32)(Rand01() * (float32)(maxR - 1));   // 2..maxR
            if (mResult > maxR) mResult = maxR;
            mA = 1 + (int32)(Rand01() * (float32)(mResult - 1));     // 1..result-1
            if (mA >= mResult) mA = mResult - 1;
            if (mA < 1) mA = 1;
            mB = mResult - mA;
        } else {
            mA = 2 + (int32)(Rand01() * (float32)(maxR - 1));        // 2..maxR
            if (mA > maxR) mA = maxR;
            mB = 1 + (int32)(Rand01() * (float32)(mA - 1));          // 1..a-1
            if (mB >= mA) mB = mA - 1;
            if (mB < 1) mB = 1;
            mResult = mA - mB;
        }
        mObjType = mLevel % NUM_TYPES;

        // Réponses : bonne + 2 distracteurs valides distincts (0..10).
        mAnswers[0] = mResult;
        int32 got = 1;
        const int32 cand[4] = { mResult - 1, mResult + 1, mResult + 2, mResult - 2 };
        for (int32 k = 0; k < 4 && got < 3; ++k) {
            const int32 v = cand[k];
            if (v < 0 || v > 10) continue;
            bool dup = false;
            for (int32 j = 0; j < got; ++j) if (mAnswers[j] == v) dup = true;
            if (!dup) mAnswers[got++] = v;
        }
        while (got < 3) {
            const int32 v = (int32)(Rand01() * 11.f);
            bool dup = false;
            for (int32 j = 0; j < got; ++j) if (mAnswers[j] == v) dup = true;
            if (!dup && v >= 0 && v <= 10) mAnswers[got++] = v;
        }
        for (int32 i = 2; i > 0; --i) {
            const int32 j = (int32)(Rand01() * (float32)(i + 1));
            const int32 t = mAnswers[i]; mAnswers[i] = mAnswers[j]; mAnswers[j] = t;
        }

        mFb.ResetLevel(3);
    }

    void CalculGame::Update(float32 dt) noexcept {
        mTime += dt;
        mFb.Update(dt);
        if (mFb.ReadyToAdvance()) { mLevel = (mLevel + 1) % LevelCount(); StartLevel(); }
        else if (mFb.ReadyToRetry()) { StartLevel(); }
    }

    void CalculGame::Render(const MouFrame& frame) noexcept {
        const float32 W = frame.width, H = frame.height;
        MouBackground::Draw(frame, mLevel / 3, mTime);

        const float32 objSz = 64.f;
        const float32 cardH = 120.f, cardW = 150.f, cardGap = 28.f;
        const float32 cardY = H - cardH - 26.f;
        const uint32  tex = mObjTex[mObjType];

        // Plante porteuse (arbre, ou buisson si tomate), troncs au sol.
        const float32 groundY  = H * 0.64f;
        const float32 plantTop = 50.f;
        const float32 plantH   = (groundY + 30.f) - plantTop;
        const bool    bush     = (mObjType == 1);
        const uint32  ptex     = bush ? mBushTex : mTreeTex;
        const float32 par      = bush ? MouPlant::BUSH_AR : MouPlant::TREE_AR;
        const float32 ccyr     = bush ? 0.46f : 0.34f;
        const float32 bhr      = bush ? 0.34f : 0.42f;

        // Dessine une plante dans la colonne (rx,rw) + ses n fruits sur le houppier.
        // crossFrom>=0 : fruits d'index >= crossFrom barrés (soustraction).
        auto drawCluster = [&](int32 n, float32 rx, float32 rw, int32 crossFrom) {
            if (n <= 0) return;
            float32 dx, dy, dw, dh;
            MouPlant::Draw(frame, ptex, par, rx, plantTop, rw, plantH, dx, dy, dw, dh);
            const int32 nn = (n > 12) ? 12 : n;
            float32 sx[12], sy[12];
            MouPlant::Slots(dx, dy, dw, dh, ccyr, 0.58f, bhr, nn, objSz, sx, sy);
            for (int32 i = 0; i < nn; ++i) {
                const bool crossed = (crossFrom >= 0 && i >= crossFrom);
                if (tex) frame.Image(tex, sx[i], sy[i], objSz, objSz,
                                     crossed ? math::NkColor{255,255,255,80} : math::NkColor{255,255,255,255});
                if (crossed) {
                    frame.Line(math::NkVec2f{ sx[i] + 8.f, sy[i] + 8.f }, math::NkVec2f{ sx[i] + objSz - 8.f, sy[i] + objSz - 8.f }, C::CORAL(), 5.f);
                    frame.Line(math::NkVec2f{ sx[i] + objSz - 8.f, sy[i] + 8.f }, math::NkVec2f{ sx[i] + 8.f, sy[i] + objSz - 8.f }, C::CORAL(), 5.f);
                }
            }
        };

        if (mMascotTex) {
            const float32 ms = 104.f * mFb.MascotScale();
            frame.Image(mMascotTex, 16.f + (104.f - ms) * 0.5f, 16.f + (104.f - ms) * 0.5f, ms, ms);
        }
        char consigne[56];
        std::snprintf(consigne, sizeof(consigne), mSub ? "Combien de %s reste-t-il ?" : "Combien de %s en tout ?",
                      kObjPlural[mObjType]);
        frame.TextCentered(frame.titleFont, 0.f, W, 28.f, consigne, C::TEXT_DARK());

        const float32 opY = (plantTop + groundY) * 0.5f - frame.LineH(frame.titleFont) * 0.5f;
        if (!mSub) {
            // Addition : arbre A  +  arbre B
            drawCluster(mA, W * 0.04f, W * 0.40f, -1);
            drawCluster(mB, W * 0.56f, W * 0.40f, -1);
            frame.TextCentered(frame.titleFont, W * 0.43f, W * 0.14f, opY, "+", C::ORANGE());
        } else {
            // Soustraction : un arbre, les B derniers fruits barrés
            drawCluster(mA, W * 0.25f, W * 0.50f, mA - mB);
            frame.TextCentered(frame.font, 0.f, W, plantTop - 4.f, "on enleve les fruits barres", C::ORANGE());
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
                if (mAnswers[i] == mResult) { mFb.Good(ccx, ccy); mFb.Win(W); }
                else                        { mFb.Bad(ccx, ccy); }
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

    void CalculGame::OnEvent(NkEvent* event) noexcept {
        if (auto* kp = event->As<NkKeyPressEvent>()) {
            if (kp->GetKey() == NkKey::NK_ESCAPE) mWantExit = true;
        }
    }

    void CalculGame::Unload() noexcept {
        MOU_LOG_INFO("Jeu Calculs decharge");
    }

}  // namespace mou
