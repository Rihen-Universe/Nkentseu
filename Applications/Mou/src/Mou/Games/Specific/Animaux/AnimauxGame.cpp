// =============================================================================
// Games/Specific/Animaux/AnimauxGame.cpp
// =============================================================================
#include "Games/Specific/Animaux/AnimauxGame.h"
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
        const char* kAnimalFile[AnimauxGame::NUM_ANIMALS] = {
            "duck.svg", "margouillat.svg", "animal_elephant.svg", "animal_lion.svg",
            "animal_poisson.svg", "animal_tortue.svg", "animal_singe.svg", "animal_oiseau.svg"
        };
        const char* kAnimalName[AnimauxGame::NUM_ANIMALS] = {
            "canard", "margouillat", "elephant", "lion",
            "poisson", "tortue", "singe", "oiseau"
        };
        // Clé de la voix "Touche le/la <animal>" (fichier assets/voice/<key>.wav).
        const char* kAnimalVoice[AnimauxGame::NUM_ANIMALS] = {
            "animaux_canard", "animaux_margouillat", "animaux_elephant", "animaux_lion",
            "animaux_poisson", "animaux_tortue", "animaux_singe", "animaux_oiseau"
        };
        // Article (avec espace ou apostrophe) propre au genre/initiale de l'animal.
        const char* kAnimalArt[AnimauxGame::NUM_ANIMALS] = {
            "le ", "le ", "l'", "le ",
            "le ", "la ", "le ", "l'"
        };
        math::NkColor CardTint(int32 i) noexcept {
            switch (i % 5) { case 0: return C::SUNNY(); case 1: return C::SKYB();
                             case 2: return C::LEAF(); case 3: return C::CORAL(); default: return C::ORANGE(); }
        }
        inline bool InRect(float32 px, float32 py, float32 x, float32 y, float32 w, float32 h) noexcept {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    }  // namespace

    float32 AnimauxGame::Rand01() noexcept {
        mRng = mRng * 1664525u + 1013904223u;
        return (float32)((mRng >> 8) & 0xFFFFFF) / 16777216.f;
    }

    bool AnimauxGame::Init() noexcept {
        mWantExit = false;
        if (mAssets) {
            for (int32 a = 0; a < NUM_ANIMALS; ++a) mAnimalTex[a] = mAssets->LoadSvg(kAnimalFile[a], 220, 220);
            mMascotTex = mAssets->LoadSvg("mascot_nana.svg", 256, 256);
            mStarTex   = mAssets->LoadSvg("star_reward.svg", 220, 220);
        }
        mFb.Init(mStarTex);
        mLevels.Load("animaux.json");
        mLevel = 0;
        StartLevel();
        MOU_LOG_INFO("Jeu Les Animaux initialise");
        return true;
    }

    void AnimauxGame::NewRound() noexcept {
        // Choisit mChoices animaux DISTINCTS + désigne la cible.
        int32 order[NUM_ANIMALS]; for (int32 i = 0; i < NUM_ANIMALS; ++i) order[i] = i;
        for (int32 i = NUM_ANIMALS - 1; i > 0; --i) {
            const int32 j = (int32)(Rand01() * (float32)(i + 1));
            const int32 t = order[i]; order[i] = order[j]; order[j] = t;
        }
        for (int32 i = 0; i < mChoices; ++i) mCardAnim[i] = order[i];
        mTargetSlot = (int32)(Rand01() * (float32)mChoices);
        if (mTargetSlot >= mChoices) mTargetSlot = mChoices - 1;
        mPendingVoice = kAnimalVoice[mCardAnim[mTargetSlot]];  // annonce vocale de la cible
    }

    void AnimauxGame::StartLevel() noexcept {
        mChoices = mLevels.GetInt(mLevel, "choices", 3);
        if (mChoices > MAX_CHOICES) mChoices = MAX_CHOICES;
        if (mChoices < 2) mChoices = 2;
        mRounds = mLevels.GetInt(mLevel, "rounds", 3);
        if (mRounds < 1) mRounds = 1;
        mRng = 909u + (uint32)mLevel * 7919u;
        mRound = 0;
        NewRound();
        mFb.ResetLevel(3);
    }

    void AnimauxGame::Update(float32 dt) noexcept {
        mTime += dt;
        mFb.Update(dt);
        if (mFb.ReadyToAdvance()) { mLevel = (mLevel + 1) % LevelCount(); StartLevel(); }
        else if (mFb.ReadyToRetry()) { StartLevel(); }
    }

    void AnimauxGame::Render(const MouFrame& frame) noexcept {
        const float32 W = frame.width, H = frame.height;
        MouBackground::Draw(frame, mLevel / 3, mTime);
        const bool playing = mFb.IsPlaying();

        if (mMascotTex) {
            const float32 ms = 104.f * mFb.MascotScale();
            frame.Image(mMascotTex, 16.f + (104.f - ms) * 0.5f, 16.f + (104.f - ms) * 0.5f, ms, ms);
        }

        // Consigne : "Touche le <animal> !"
        char consigne[64];
        const int32 tgt = mCardAnim[mTargetSlot];
        std::snprintf(consigne, sizeof(consigne), "Touche %s%s !", kAnimalArt[tgt], kAnimalName[tgt]);
        frame.TextCentered(frame.titleFont, 0.f, W, 30.f, consigne, C::TEXT_DARK());

        // Cartes-animaux côte à côte.
        const float32 cardH = H * 0.46f;
        const float32 cardW = cardH * 0.86f;
        const float32 gap   = 34.f;
        const float32 total = mChoices * cardW + (mChoices - 1) * gap;
        const float32 startX = (W - total) * 0.5f;
        const float32 cardY  = (H - cardH) * 0.5f + 30.f;

        for (int32 i = 0; i < mChoices; ++i) {
            const float32 cx = startX + i * (cardW + gap);
            const bool over = InRect(frame.pointer.x, frame.pointer.y, cx, cardY, cardW, cardH);
            const bool hot  = over && frame.pointerDown && playing;
            frame.Rect(cx, cardY + 8.f, cardW, cardH, ui::WithAlpha(C::INK(), 60), 26.f);
            frame.Rect(cx, cardY, cardW, cardH, hot ? CardTint(i) : C::SURFACE(), 26.f);
            frame.Border(cx, cardY, cardW, cardH, C::INK(), 5.f, 26.f);
            const float32 is = cardW * 0.74f;
            if (mAnimalTex[mCardAnim[i]])
                frame.Image(mAnimalTex[mCardAnim[i]], cx + (cardW - is) * 0.5f, cardY + (cardH - is) * 0.5f, is, is);

            if (playing && over && frame.pointerReleased) {
                const float32 ccx = cx + cardW * 0.5f, ccy = cardY + cardH * 0.5f;
                if (i == mTargetSlot) {
                    mFb.Good(ccx, ccy);
                    if (mRound + 1 >= mRounds) mFb.Win(W);
                    else { ++mRound; NewRound(); }
                } else {
                    mFb.Bad(ccx, ccy);
                }
            }
        }

        // Progression des manches (petits points).
        const float32 dotR = 8.f, dotGap = 26.f;
        const float32 dtot = (mRounds - 1) * dotGap;
        for (int32 r = 0; r < mRounds; ++r) {
            const float32 dx = (W - dtot) * 0.5f + r * dotGap;
            frame.Circle(math::NkVec2f{ dx, 96.f }, dotR, r <= mRound ? C::STAR_GOLD() : ui::WithAlpha(C::INK(), 70));
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

    void AnimauxGame::OnEvent(NkEvent* event) noexcept {
        if (auto* kp = event->As<NkKeyPressEvent>()) {
            if (kp->GetKey() == NkKey::NK_ESCAPE) mWantExit = true;
        }
    }

    void AnimauxGame::Unload() noexcept {
        MOU_LOG_INFO("Jeu Les Animaux decharge");
    }

}  // namespace mou
