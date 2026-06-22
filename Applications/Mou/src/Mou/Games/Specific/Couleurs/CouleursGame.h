// =============================================================================
// Games/Specific/Couleurs/CouleursGame.h
// Jeu "Les Couleurs" — trier des fruits locaux dans le panier de leur couleur.
// Drag-and-drop + feedback (MouFeedback : effets + étoiles qui diminuent), niveaux.
// =============================================================================
#pragma once

#ifndef MOU_COULEURS_GAME_H
#define MOU_COULEURS_GAME_H

#include "Games/Common/MouGame.h"
#include "Games/Common/MouFeedback.h"
#include "Games/Common/MouPlant.h"
#include "Levels/MouLevels.h"

namespace mou {

    class CouleursGame : public MouGame {
    public:
        bool Init() noexcept override;
        void Update(nkentseu::float32 dt) noexcept override;
        void Render(const MouFrame& frame) noexcept override;
        void OnEvent(nkentseu::NkEvent* event) noexcept override;
        void Unload() noexcept override;

        const char* GetTitle() const noexcept override { return "Les Couleurs"; }
        bool WantExit() const noexcept override { return mWantExit; }
        nkentseu::int32 LevelCount() const noexcept override { return mLevels.Count() > 0 ? mLevels.Count() : 8; }
        nkentseu::int32 GetStars() const noexcept override { return mFb.StarsLeft(); }
        MouFeedback::Cue ConsumeAudioCue() noexcept override { return mFb.ConsumeCue(); }

        static constexpr nkentseu::int32 NUM_COLORS = 5;   // rouge,jaune,vert,bleu,orange
        static constexpr nkentseu::int32 MAX_FRUITS = 8;

    private:
        void    StartLevel() noexcept;
        nkentseu::float32 Rand01() noexcept;

        struct FruitItem {
            nkentseu::int32   color = 0;
            nkentseu::float32 x = 0.f, y = 0.f;          // position courante (suit le doigt si cueilli)
            nkentseu::float32 homeX = 0.f, homeY = 0.f;  // emplacement fixe sur l'arbre
            bool              sorted = false;
        };

        nkentseu::uint32 mBasketTex[NUM_COLORS] = {0};
        nkentseu::uint32 mFruitTex [NUM_COLORS] = {0};
        nkentseu::uint32 mMascotTex = 0;
        nkentseu::uint32 mStarTex   = 0;
        nkentseu::uint32 mTreeTex   = 0;
        nkentseu::uint32 mBushTex   = 0;
        nkentseu::int32  mPlantColor[NUM_COLORS] = {0};  // couleur de la plante à chaque position (mélangé)

        FruitItem        mFruits[MAX_FRUITS];
        nkentseu::int32  mFruitCount    = 0;
        nkentseu::int32  mDragging      = -1;
        nkentseu::float32 mDragOffX = 0.f, mDragOffY = 0.f;

        nkentseu::int32   mLevel        = 0;
        nkentseu::int32   mActiveColors = 2;
        nkentseu::int32   mSortedCount  = 0;
        bool              mNeedSpawn    = true;
        nkentseu::float32 mTime         = 0.f;
        nkentseu::uint32  mRng          = 12345u;
        bool              mWantExit     = false;

        MouFeedback       mFb;
        MouLevels         mLevels;
    };

}  // namespace mou

#endif // MOU_COULEURS_GAME_H
