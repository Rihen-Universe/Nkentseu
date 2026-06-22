// =============================================================================
// Games/Specific/Compter/CompterGame.h
// Jeu "Compter" — dénombrer des objets locaux (1 à 10) et choisir le bon chiffre.
// Tap sur chaque objet = compté (feedback). Cartes-réponses chiffrées (NKFont).
// =============================================================================
#pragma once

#ifndef MOU_COMPTER_GAME_H
#define MOU_COMPTER_GAME_H

#include "Games/Common/MouGame.h"
#include "Games/Common/MouFeedback.h"
#include "Games/Common/MouPlant.h"
#include "Levels/MouLevels.h"

namespace mou {

    class CompterGame : public MouGame {
    public:
        bool Init() noexcept override;
        void Update(nkentseu::float32 dt) noexcept override;
        void Render(const MouFrame& frame) noexcept override;
        void OnEvent(nkentseu::NkEvent* event) noexcept override;
        void Unload() noexcept override;

        const char* GetTitle() const noexcept override { return "Compter"; }
        bool WantExit() const noexcept override { return mWantExit; }
        nkentseu::int32 LevelCount() const noexcept override { return mLevels.Count() > 0 ? mLevels.Count() : 8; }
        nkentseu::int32 GetStars() const noexcept override { return mFb.StarsLeft(); }
        MouFeedback::Cue ConsumeAudioCue() noexcept override { return mFb.ConsumeCue(); }

        static constexpr nkentseu::int32 MAX_OBJ   = 10;
        static constexpr nkentseu::int32 NUM_TYPES = 5;

    private:
        void    StartLevel() noexcept;
        nkentseu::float32 Rand01() noexcept;

        struct Obj { nkentseu::float32 x = 0.f, y = 0.f; bool counted = false; };

        nkentseu::uint32 mObjTex[NUM_TYPES] = {0};
        nkentseu::uint32 mMascotTex = 0;
        nkentseu::uint32 mStarTex   = 0;
        nkentseu::uint32 mTreeTex   = 0;
        nkentseu::uint32 mBushTex   = 0;

        Obj              mObjs[MAX_OBJ];
        nkentseu::int32  mCount   = 1;
        nkentseu::int32  mObjType = 0;
        nkentseu::int32  mAnswers[3] = {0, 0, 0};

        nkentseu::int32  mLevel     = 0;
        nkentseu::int32  mMax       = 3;
        bool             mNeedSpawn = true;
        bool             mWantExit  = false;
        nkentseu::float32 mTime      = 0.f;
        nkentseu::uint32  mRng       = 777u;
        MouFeedback      mFb;
        MouLevels        mLevels;
    };

}  // namespace mou

#endif // MOU_COMPTER_GAME_H
