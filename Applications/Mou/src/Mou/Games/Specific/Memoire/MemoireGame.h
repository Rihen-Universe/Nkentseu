// =============================================================================
// Games/Specific/Memoire/MemoireGame.h
// Jeu "La Memoire" — retrouver les paires de fruits cachees. Retourne 2 cartes :
// identiques -> restent ; differentes -> se recachent. Doux (pas de perte
// d'etoile sur erreur), feedback a chaque paire et a la victoire.
// =============================================================================
#pragma once

#ifndef MOU_MEMOIRE_GAME_H
#define MOU_MEMOIRE_GAME_H

#include "Games/Common/MouGame.h"
#include "Games/Common/MouFeedback.h"
#include "Levels/MouLevels.h"

namespace mou {

    class MemoireGame : public MouGame {
    public:
        bool Init() noexcept override;
        void Update(nkentseu::float32 dt) noexcept override;
        void Render(const MouFrame& frame) noexcept override;
        void OnEvent(nkentseu::NkEvent* event) noexcept override;
        void Unload() noexcept override;

        const char* GetTitle() const noexcept override { return "La Memoire"; }
        bool WantExit() const noexcept override { return mWantExit; }
        nkentseu::int32 LevelCount() const noexcept override { return mLevels.Count() > 0 ? mLevels.Count() : 8; }
        nkentseu::int32 GetStars() const noexcept override { return mFb.StarsLeft(); }
        MouFeedback::Cue ConsumeAudioCue() noexcept override { return mFb.ConsumeCue(); }

        static constexpr nkentseu::int32 NUM_FACES = 6;
        static constexpr nkentseu::int32 MAX_PAIRS = 6;
        static constexpr nkentseu::int32 MAX_CARDS = MAX_PAIRS * 2;

    private:
        void    StartLevel() noexcept;
        nkentseu::float32 Rand01() noexcept;

        struct Card { nkentseu::int32 face = 0; bool up = false; bool matched = false; };

        nkentseu::uint32 mFaceTex[NUM_FACES] = {0};
        nkentseu::uint32 mMascotTex = 0;
        nkentseu::uint32 mStarTex   = 0;

        Card             mCards[MAX_CARDS];
        nkentseu::int32  mPairs     = 3;
        nkentseu::int32  mCardCount = 6;
        nkentseu::int32  mFlipA     = -1;
        nkentseu::int32  mFlipB     = -1;
        nkentseu::int32  mFound     = 0;
        nkentseu::float32 mHideTimer = 0.f;   // delai avant de recacher une mauvaise paire
        nkentseu::float32 mPreviewTimer = 0.f; // au debut : toutes les cartes visibles puis se cachent

        nkentseu::int32  mLevel     = 0;
        bool             mWantExit  = false;
        nkentseu::float32 mTime     = 0.f;
        nkentseu::uint32  mRng      = 7777u;
        MouFeedback      mFb;
        MouLevels        mLevels;
    };

}  // namespace mou

#endif // MOU_MEMOIRE_GAME_H
