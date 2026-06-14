// =============================================================================
// Games/Specific/Labyrinth/LabyrinthGame.h
// Gardien du Labyrinthe — top-down : atteindre la sortie en évitant les murs.
// Responsive, multi-niveaux, jouable clavier (flèches/WASD) + D-pad tactile.
// =============================================================================
#pragma once

#ifndef NKOUNG_LABYRINTH_GAME_H
#define NKOUNG_LABYRINTH_GAME_H

#include "Games/Common/NkoungGame.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkoung {

    class LabyrinthGame : public NkoungGame {
    public:
        LabyrinthGame();
        ~LabyrinthGame() override = default;
        LabyrinthGame(const LabyrinthGame&) = delete;
        LabyrinthGame& operator=(const LabyrinthGame&) = delete;

        bool Init() noexcept override;
        void Update(nkentseu::float32 dt) noexcept override;
        void Render(const NkoungFrame& frame) noexcept override;
        void OnEvent(nkentseu::NkEvent* event) noexcept override;
        void Unload() noexcept override;

        const char* GetTitle() const noexcept override { return "Gardien du Labyrinthe"; }
        bool WantExit() const noexcept override { return mWantExit; }
        const char* GetCurrentLevelTitle() const noexcept override;
        nkentseu::float32 GetProgress() const noexcept override;

    private:
        bool LoadLevel(nkentseu::int32 index) noexcept;
        void Move(nkentseu::int32 dx, nkentseu::int32 dy) noexcept;
        void NextLevel() noexcept;
        char Cell(nkentseu::int32 x, nkentseu::int32 y) const noexcept;

        nkentseu::NkVector<char> mGrid;     // '#'=mur, ' '=sol
        nkentseu::int32 mW = 0, mH = 0;
        nkentseu::int32 mPX = 0, mPY = 0;   // position du joueur
        nkentseu::int32 mExitX = 0, mExitY = 0;
        nkentseu::int32 mLevelIndex = 0, mSteps = 0;
        bool mWon = false, mWantExit = false;
        nkentseu::float32 mAnim = 0.f;
    };

}  // namespace nkoung

#endif // NKOUNG_LABYRINTH_GAME_H
