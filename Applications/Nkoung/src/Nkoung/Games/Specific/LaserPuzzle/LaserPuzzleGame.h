// =============================================================================
// Games/Specific/LaserPuzzle/LaserPuzzleGame.h
// Laser Puzzle — guider un rayon vers la/les cible(s) en orientant des miroirs.
// Responsive (s'adapte à l'écran), multi-niveaux, jouable souris/tactile/clavier.
// =============================================================================
#pragma once

#ifndef NKOUNG_LASER_PUZZLE_GAME_H
#define NKOUNG_LASER_PUZZLE_GAME_H

#include "Games/Common/NkoungGame.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkoung {

    enum class LaserTile : nkentseu::nk_uint8 {
        Empty = 0, Source, Mirror, Target, Wall
    };

    struct LaserCell {
        LaserTile type = LaserTile::Empty;
        nkentseu::int8 orient = 0;  ///< miroir : 0 = "\", 1 = "/"
        nkentseu::int8 dir = 0;     ///< source : 0=droite 1=bas 2=gauche 3=haut
        bool hit = false;           ///< cible atteinte par le rayon
    };

    class LaserPuzzleGame : public NkoungGame {
    public:
        LaserPuzzleGame();
        ~LaserPuzzleGame() override = default;
        LaserPuzzleGame(const LaserPuzzleGame&) = delete;
        LaserPuzzleGame& operator=(const LaserPuzzleGame&) = delete;

        bool Init() noexcept override;
        void Update(nkentseu::float32 dt) noexcept override;
        void Render(const NkoungFrame& frame) noexcept override;
        void OnEvent(nkentseu::NkEvent* event) noexcept override;
        void Unload() noexcept override;

        const char* GetTitle() const noexcept override { return "Laser Puzzle"; }
        bool WantExit() const noexcept override { return mWantExit; }
        const char* GetCurrentLevelTitle() const noexcept override;
        nkentseu::float32 GetProgress() const noexcept override;

    private:
        bool LoadLevel(nkentseu::int32 index) noexcept;
        void Simulate() noexcept;                         ///< trace le rayon + détecte la victoire
        void RotateMirror(nkentseu::int32 idx) noexcept;
        void NextLevel() noexcept;

        LaserCell& At(nkentseu::int32 x, nkentseu::int32 y) noexcept { return mGrid[y * mGridW + x]; }
        const LaserCell& At(nkentseu::int32 x, nkentseu::int32 y) const noexcept { return mGrid[y * mGridW + x]; }

        nkentseu::NkVector<LaserCell> mGrid;
        nkentseu::int32 mGridW = 0, mGridH = 0;
        nkentseu::int32 mLevelIndex = 0;
        nkentseu::int32 mMoves = 0;
        nkentseu::int32 mSelected = -1;
        bool mWon = false;
        bool mWantExit = false;
        nkentseu::int32 mTargetsTotal = 0, mTargetsHit = 0;

        // Rayon : suite de centres de cellules en COORDONNÉES GRILLE (converties en pixels au rendu).
        nkentseu::NkVector<nkentseu::math::NkVec2f> mRay;
        nkentseu::float32 mAnim = 0.f;  ///< pulsation visuelle du rayon
    };

}  // namespace nkoung

#endif // NKOUNG_LASER_PUZZLE_GAME_H
