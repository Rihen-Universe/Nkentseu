// =============================================================================
// Games/Specific/Calcul/CalculGame.h
// Jeu "Calculs" — addition / soustraction 100 % visuelle (groupes de fruits).
// Addition : groupe A + groupe B. Soustraction : groupe A avec B fruits barrés.
// Réponse = 1 carte parmi 3 (chiffres NKFont). Niveaux progressifs.
// =============================================================================
#pragma once

#ifndef MOU_CALCUL_GAME_H
#define MOU_CALCUL_GAME_H

#include "Games/Common/MouGame.h"
#include "Games/Common/MouFeedback.h"
#include "Games/Common/MouPlant.h"
#include "Levels/MouLevels.h"

namespace mou {

    class CalculGame : public MouGame {
    public:
        bool Init() noexcept override;
        void Update(nkentseu::float32 dt) noexcept override;
        void Render(const MouFrame& frame) noexcept override;
        void OnEvent(nkentseu::NkEvent* event) noexcept override;
        void Unload() noexcept override;

        const char* GetTitle() const noexcept override { return "Calculs"; }
        bool WantExit() const noexcept override { return mWantExit; }
        nkentseu::int32 LevelCount() const noexcept override { return mLevels.Count() > 0 ? mLevels.Count() : 8; }
        nkentseu::int32 GetStars() const noexcept override { return mFb.StarsLeft(); }
        MouFeedback::Cue ConsumeAudioCue() noexcept override { return mFb.ConsumeCue(); }

        static constexpr nkentseu::int32 NUM_TYPES = 5;

    private:
        void    StartLevel() noexcept;
        nkentseu::float32 Rand01() noexcept;

        nkentseu::uint32 mObjTex[NUM_TYPES] = {0};
        nkentseu::uint32 mMascotTex = 0;
        nkentseu::uint32 mStarTex   = 0;
        nkentseu::uint32 mTreeTex   = 0;
        nkentseu::uint32 mBushTex   = 0;

        nkentseu::int32  mA = 1, mB = 1, mResult = 2;
        bool             mSub = false;        // false = addition, true = soustraction
        nkentseu::int32  mObjType = 0;
        nkentseu::int32  mAnswers[3] = {0, 0, 0};

        nkentseu::int32  mLevel     = 0;
        bool             mWantExit  = false;
        nkentseu::float32 mTime      = 0.f;
        nkentseu::uint32  mRng       = 9001u;
        MouFeedback      mFb;
        MouLevels        mLevels;
    };

}  // namespace mou

#endif // MOU_CALCUL_GAME_H
