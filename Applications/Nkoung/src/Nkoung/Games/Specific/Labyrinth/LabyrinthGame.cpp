// =============================================================================
// Games/Specific/Labyrinth/LabyrinthGame.cpp
// Gardien du Labyrinthe — top-down, multi-niveaux, responsive, clavier + tactile.
// =============================================================================
#include "Games/Specific/Labyrinth/LabyrinthGame.h"
#include "Core/NkoungConfig.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "../../../UI/NkoungUIColor.h"
#include <cstdio>

using namespace nkentseu;

namespace nkoung {

    using C = ui::NkoungUIColor;

    namespace {
        // '#'=mur, ' '=sol, 'P'=départ, 'E'=sortie. Salles ouvertes à piliers (toujours solubles).
        struct MazeDef { const char* name; int32 w, h; const char* rows[12]; };
        static const MazeDef kMazes[] = {
            { "Premiere salle", 10, 7, {
                "##########",
                "#P       #",
                "#  #  #  #",
                "#        #",
                "#  #  #  #",
                "#       E#",
                "##########" } },
            { "Le dedale", 12, 8, {
                "############",
                "#P         #",
                "#  #  #  # #",
                "#          #",
                "#  #  #  # #",
                "#          #",
                "#  #  #  #E#",
                "############" } },
            { "Salle du gardien", 14, 9, {
                "##############",
                "#P           #",
                "#  #  #  #   #",
                "#            #",
                "#  #  #  #   #",
                "#            #",
                "#  #  #  #   #",
                "#          E #",
                "##############" } },
        };
        static const int32 kMazeCount = static_cast<int32>(sizeof(kMazes) / sizeof(kMazes[0]));

        static char CharAt(const char* row, int32 x) noexcept {
            int32 n = 0; while (row[n]) ++n;
            return (x >= 0 && x < n) ? row[x] : '#';
        }
    }  // namespace

    LabyrinthGame::LabyrinthGame() = default;

    bool LabyrinthGame::Init() noexcept {
        NKOUNG_LOG_INFO("Initialisation Gardien du Labyrinthe");
        return LoadLevel(0);
    }

    bool LabyrinthGame::LoadLevel(int32 index) noexcept {
        if (index < 0 || index >= kMazeCount) index = 0;
        mLevelIndex = index;
        const MazeDef& M = kMazes[index];
        mW = M.w; mH = M.h;
        mGrid.Resize(static_cast<usize>(mW * mH));
        mPX = mPY = mExitX = mExitY = 0;

        for (int32 y = 0; y < mH; ++y) {
            for (int32 x = 0; x < mW; ++x) {
                char ch = CharAt(M.rows[y], x);
                if (ch == 'P') { mPX = x; mPY = y; ch = ' '; }
                else if (ch == 'E') { mExitX = x; mExitY = y; ch = ' '; }
                mGrid[static_cast<usize>(y * mW + x)] = ch;
            }
        }
        mSteps = 0; mWon = false;
        NKOUNG_LOG_INFOF("Labyrinthe %d charge (%dx%d)", index + 1, mW, mH);
        return true;
    }

    char LabyrinthGame::Cell(int32 x, int32 y) const noexcept {
        if (x < 0 || x >= mW || y < 0 || y >= mH) return '#';
        return mGrid[static_cast<usize>(y * mW + x)];
    }

    void LabyrinthGame::Move(int32 dx, int32 dy) noexcept {
        if (mWon) return;
        const int32 nx = mPX + dx, ny = mPY + dy;
        if (Cell(nx, ny) == '#') return;
        mPX = nx; mPY = ny; ++mSteps;
        if (mPX == mExitX && mPY == mExitY) mWon = true;
    }

    void LabyrinthGame::NextLevel() noexcept { LoadLevel((mLevelIndex + 1) % kMazeCount); }

    void LabyrinthGame::Update(float32 dt) noexcept { mAnim += dt; }

    void LabyrinthGame::Render(const NkoungFrame& frame) noexcept {
        const float32 SX = frame.safeX, SY = frame.safeY, SW = frame.safeW, SH = frame.safeH;
        const float32 headerH = 66.f, footerH = 64.f;
        const float32 areaX = SX, areaY = SY + headerH;
        const float32 areaW = SW, areaH = SH - headerH - footerH;
        if (mW <= 0 || mH <= 0 || areaW <= 0.f || areaH <= 0.f) return;

        float32 cell = (areaW / mW < areaH / mH) ? areaW / mW : areaH / mH;
        cell *= 0.96f;
        const float32 boardW = cell * mW, boardH = cell * mH;
        const float32 boardX = areaX + (areaW - boardW) * 0.5f;
        const float32 boardY = areaY + (areaH - boardH) * 0.5f;

        // pulsation
        float32 t = mAnim * 1.4f; t -= static_cast<float32>(static_cast<int32>(t));
        const float32 pulse = (t < 0.5f) ? t * 2.f : (1.f - t) * 2.f;

        // ── En-tête ──
        frame.Rect(SX, SY, SW, headerH, C::BG_SECONDARY());
        frame.Rect(SX, SY + headerH - 2.f, SW, 2.f, C::CYAN_BRIGHT());
        char buf[128];
        snprintf(buf, sizeof(buf), "Gardien du Labyrinthe  -  Niveau %d : %s", mLevelIndex + 1, kMazes[mLevelIndex].name);
        frame.Text(frame.font, SX + 16.f, SY + 10.f, buf, C::CYAN_BRIGHT());
        frame.Text(frame.font, SX + 16.f, SY + 38.f,
                   "Atteins le portail vert. Fleches/WASD ou les boutons.", C::TEXT_TERTIARY());
        snprintf(buf, sizeof(buf), "Pas : %d", mSteps);
        frame.Text(frame.font, SX + SW - 16.f - frame.TextW(frame.font, buf), SY + 12.f, buf, C::TEXT_SECONDARY());

        // ── Plateau ──
        for (int32 y = 0; y < mH; ++y) {
            for (int32 x = 0; x < mW; ++x) {
                const float32 px = boardX + x * cell, py = boardY + y * cell;
                if (Cell(x, y) == '#') {
                    frame.Rect(px, py, cell, cell, C::BG_TERTIARY(), 4.f);
                } else {
                    frame.Rect(px + 1.f, py + 1.f, cell - 2.f, cell - 2.f, math::NkColor{ 22, 27, 40, 255 }, 3.f);
                }
            }
        }

        // Sortie : portail vert pulsant.
        {
            const float32 ex = boardX + (mExitX + 0.5f) * cell, ey = boardY + (mExitY + 0.5f) * cell;
            frame.Circle(math::NkVec2f{ ex, ey }, cell * (0.34f + pulse * 0.05f),
                         math::NkColor{ 0, 208, 132, static_cast<uint8>(90 + pulse * 90) }, 0);
            frame.CircleOutline(math::NkVec2f{ ex, ey }, cell * 0.30f, C::GREEN_SUCCESS(), 3.f, 0);
        }

        // Joueur : cercle cyan pulsant.
        {
            const float32 plx = boardX + (mPX + 0.5f) * cell, ply = boardY + (mPY + 0.5f) * cell;
            frame.Circle(math::NkVec2f{ plx, ply }, cell * (0.30f + pulse * 0.04f),
                         math::NkColor{ 0, 217, 255, static_cast<uint8>(70 + pulse * 70) }, 0);
            frame.Circle(math::NkVec2f{ plx, ply }, cell * 0.22f, C::CYAN_BRIGHT(), 0);
        }

        // ── Bandeau de victoire ──
        if (mWon) {
            const char* msg = "Sortie atteinte !";
            nkui::NkUIFont* tf = frame.titleFont ? frame.titleFont : frame.font;
            const float32 bw = frame.TextW(tf, msg) + 48.f;
            const float32 bx = SX + (SW - bw) * 0.5f, by = areaY + 6.f;
            frame.Rect(bx, by, bw, 46.f, math::NkColor{ 0, 208, 132, 45 }, 12.f);
            frame.Border(bx, by, bw, 46.f, C::GREEN_SUCCESS(), 1.5f, 12.f);
            frame.TextCentered(tf, bx, bw, by + 8.f, msg, C::GREEN_SUCCESS());
        }

        // ── Pied : Menu + D-pad + Recommencer + Suivant ──
        const float32 fy = SY + SH - footerH + 8.f;
        const float32 bh = footerH - 16.f;
        const math::NkColor bg = C::BG_TERTIARY(), bgh = C::CYAN_MID(), fg = C::TEXT_PRIMARY();
        if (frame.Button(SX + 12.f, fy, 84.f, bh, "Menu", bg, bgh, fg)) mWantExit = true;

        const float32 dpx = SX + 110.f, db = bh;
        if (frame.Button(dpx,             fy, db, bh, "<", bg, bgh, fg)) Move(-1, 0);
        if (frame.Button(dpx + db + 6.f,  fy, db, bh, "^", bg, bgh, fg)) Move(0, -1);
        if (frame.Button(dpx + 2*(db+6.f),fy, db, bh, "v", bg, bgh, fg)) Move(0, 1);
        if (frame.Button(dpx + 3*(db+6.f),fy, db, bh, ">", bg, bgh, fg)) Move(1, 0);

        if (frame.Button(dpx + 4*(db+6.f) + 8.f, fy, 130.f, bh, "Recommencer", bg, bgh, fg))
            LoadLevel(mLevelIndex);

        if (mWon) {
            const math::NkColor brd = C::GREEN_SUCCESS();
            if (frame.Button(SX + SW - 16.f - 170.f, fy, 170.f, bh, "Niveau suivant >",
                             C::GREEN_SUCCESS(), C::GRADIENT_GREEN_END(), C::BG_DARK(), &brd))
                NextLevel();
        }
    }

    void LabyrinthGame::OnEvent(NkEvent* event) noexcept {
        if (!event) return;
        auto* kp = event->As<NkKeyPressEvent>();
        if (!kp) return;
        switch (kp->GetKey()) {
            case NkKey::NK_LEFT:  case NkKey::NK_A: Move(-1, 0); break;
            case NkKey::NK_RIGHT: case NkKey::NK_D: Move(1, 0); break;
            case NkKey::NK_UP:    case NkKey::NK_W: Move(0, -1); break;
            case NkKey::NK_DOWN:  case NkKey::NK_S: Move(0, 1); break;
            case NkKey::NK_ESCAPE: mWantExit = true; break;
            case NkKey::NK_ENTER:  if (mWon) NextLevel(); break;
            default: break;
        }
    }

    void LabyrinthGame::Unload() noexcept {
        mGrid.Clear();
        NKOUNG_LOG_INFO("Labyrinthe decharge");
    }

    const char* LabyrinthGame::GetCurrentLevelTitle() const noexcept { return kMazes[mLevelIndex].name; }
    float32 LabyrinthGame::GetProgress() const noexcept {
        return (kMazeCount > 0) ? static_cast<float32>(mLevelIndex) / kMazeCount : 0.f;
    }

}  // namespace nkoung
