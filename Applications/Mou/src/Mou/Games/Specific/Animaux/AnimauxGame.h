// =============================================================================
// Games/Specific/Animaux/AnimauxGame.h
// Jeu "Les Animaux" — ecoute/lis la consigne et touche le bon animal parmi
// plusieurs cartes. Plusieurs manches par niveau (sous-missions). Feedback.
// =============================================================================
#pragma once

#ifndef MOU_ANIMAUX_GAME_H
#define MOU_ANIMAUX_GAME_H

#include "Games/Common/MouGame.h"
#include "Games/Common/MouFeedback.h"
#include "Levels/MouLevels.h"

namespace mou {

    class AnimauxGame : public MouGame {
    public:
        bool Init() noexcept override;
        void Update(nkentseu::float32 dt) noexcept override;
        void Render(const MouFrame& frame) noexcept override;
        void OnEvent(nkentseu::NkEvent* event) noexcept override;
        void Unload() noexcept override;

        const char* GetTitle() const noexcept override { return "Les Animaux"; }
        bool WantExit() const noexcept override { return mWantExit; }
        nkentseu::int32 LevelCount() const noexcept override { return mLevels.Count() > 0 ? mLevels.Count() : 8; }
        nkentseu::int32 GetStars() const noexcept override { return mFb.StarsLeft(); }
        MouFeedback::Cue ConsumeAudioCue() noexcept override { return mFb.ConsumeCue(); }

        static constexpr nkentseu::int32 NUM_ANIMALS = 8;
        static constexpr nkentseu::int32 MAX_CHOICES = 4;

    private:
        void    StartLevel() noexcept;
        void    NewRound() noexcept;
        nkentseu::float32 Rand01() noexcept;

        nkentseu::uint32 mAnimalTex[NUM_ANIMALS] = {0};
        nkentseu::uint32 mMascotTex = 0;
        nkentseu::uint32 mStarTex   = 0;

        nkentseu::int32  mChoices   = 3;       // cartes affichées
        nkentseu::int32  mCardAnim[MAX_CHOICES] = {0};  // index animal de chaque carte
        nkentseu::int32  mTargetSlot = 0;      // carte correcte
        nkentseu::int32  mRounds    = 3;       // manches par niveau
        nkentseu::int32  mRound     = 0;

        nkentseu::int32  mLevel     = 0;
        bool             mWantExit  = false;
        nkentseu::float32 mTime     = 0.f;
        nkentseu::uint32  mRng      = 2024u;
        MouFeedback      mFb;
        MouLevels        mLevels;
    };

}  // namespace mou

#endif // MOU_ANIMAUX_GAME_H
