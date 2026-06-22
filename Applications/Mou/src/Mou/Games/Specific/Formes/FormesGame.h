// =============================================================================
// Games/Specific/Formes/FormesGame.h
// Jeu "Les Formes" — encastrement : glisser chaque forme dans le trou de sa
// silhouette (rond/carre/triangle/etoile/coeur). Drag-and-drop + feedback.
// =============================================================================
#pragma once

#ifndef MOU_FORMES_GAME_H
#define MOU_FORMES_GAME_H

#include "Games/Common/MouGame.h"
#include "Games/Common/MouFeedback.h"
#include "Levels/MouLevels.h"

namespace mou {

    class FormesGame : public MouGame {
    public:
        bool Init() noexcept override;
        void Update(nkentseu::float32 dt) noexcept override;
        void Render(const MouFrame& frame) noexcept override;
        void OnEvent(nkentseu::NkEvent* event) noexcept override;
        void Unload() noexcept override;

        const char* GetTitle() const noexcept override { return "Les Formes"; }
        bool WantExit() const noexcept override { return mWantExit; }
        nkentseu::int32 LevelCount() const noexcept override { return mLevels.Count() > 0 ? mLevels.Count() : 8; }
        nkentseu::int32 GetStars() const noexcept override { return mFb.StarsLeft(); }
        MouFeedback::Cue ConsumeAudioCue() noexcept override { return mFb.ConsumeCue(); }

        static constexpr nkentseu::int32 NUM_SHAPES = 5;   // rond,carre,triangle,etoile,coeur
        static constexpr nkentseu::int32 MAX_SLOTS  = 5;

    private:
        void    StartLevel() noexcept;
        nkentseu::float32 Rand01() noexcept;

        struct Slot  { nkentseu::int32 shape = 0; nkentseu::float32 x = 0.f, y = 0.f; bool filled = false; };
        struct Piece { nkentseu::int32 shape = 0; nkentseu::int32 color = 0;
                       nkentseu::float32 x = 0.f, y = 0.f, homeX = 0.f, homeY = 0.f; bool placed = false; };

        nkentseu::uint32 mShapeTex[NUM_SHAPES] = {0};
        nkentseu::uint32 mMascotTex = 0;
        nkentseu::uint32 mStarTex   = 0;

        Slot             mSlots[MAX_SLOTS];
        Piece            mPieces[MAX_SLOTS];
        nkentseu::int32  mCount     = 3;
        nkentseu::int32  mDragging  = -1;
        nkentseu::float32 mDragOffX = 0.f, mDragOffY = 0.f;
        nkentseu::int32  mPlaced    = 0;

        nkentseu::int32  mLevel     = 0;
        bool             mNeedSpawn = true;
        bool             mWantExit  = false;
        nkentseu::float32 mTime     = 0.f;
        nkentseu::uint32  mRng      = 5150u;
        MouFeedback      mFb;
        MouLevels        mLevels;
    };

}  // namespace mou

#endif // MOU_FORMES_GAME_H
