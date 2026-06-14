// =============================================================================
// Games/Specific/LaserPuzzle/LaserPuzzleGame.cpp
// Laser Puzzle — simulation de rayon + miroirs, multi-niveaux, responsive.
// =============================================================================
#include "Games/Specific/LaserPuzzle/LaserPuzzleGame.h"
#include "Core/NkoungConfig.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "../../../UI/NkoungUIColor.h"
#include <cstdio>

using namespace nkentseu;

namespace nkoung {

    using C = ui::NkoungUIColor;

    namespace {
        // Niveaux en cartes ASCII : '>v<^'=source(dir), '\' '/'=miroir, 'T'=cible,
        // '#'=mur, ' '=vide. Les miroirs démarrent dans une orientation à corriger.
        struct LevelDef { const char* name; int32 w, h; const char* rows[12]; };
        static const LevelDef kLevels[] = {
            { "Premiers reflets", 6, 4, { ">  /  ", "      ", "      ", "   T  " } },
            { "Double miroir",    7, 4, { ">   /  ", "       ", "       ", "T   \\  " } },
            { "Le detour",        6, 5, { "> / # ", "      ", "      ", "      ", "  /  T" } },
        };
        static const int32 kLevelCount = static_cast<int32>(sizeof(kLevels) / sizeof(kLevels[0]));

        static char CharAt(const char* row, int32 x) noexcept {
            int32 n = 0; while (row[n]) ++n;
            return (x >= 0 && x < n) ? row[x] : ' ';
        }
    }  // namespace

    LaserPuzzleGame::LaserPuzzleGame() = default;

    bool LaserPuzzleGame::Init() noexcept {
        NKOUNG_LOG_INFO("Initialisation Laser Puzzle");
        return LoadLevel(0);
    }

    bool LaserPuzzleGame::LoadLevel(int32 index) noexcept {
        if (index < 0 || index >= kLevelCount) index = 0;  // boucle sur les niveaux
        mLevelIndex = index;
        const LevelDef& L = kLevels[index];
        mGridW = L.w; mGridH = L.h;
        mGrid.Resize(static_cast<usize>(mGridW * mGridH));
        mTargetsTotal = 0;

        for (int32 y = 0; y < mGridH; ++y) {
            const char* row = L.rows[y];
            for (int32 x = 0; x < mGridW; ++x) {
                LaserCell c;
                switch (CharAt(row, x)) {
                    case '>': c.type = LaserTile::Source; c.dir = 0; break;
                    case 'v': c.type = LaserTile::Source; c.dir = 1; break;
                    case '<': c.type = LaserTile::Source; c.dir = 2; break;
                    case '^': c.type = LaserTile::Source; c.dir = 3; break;
                    case '\\': c.type = LaserTile::Mirror; c.orient = 0; break;
                    case '/':  c.type = LaserTile::Mirror; c.orient = 1; break;
                    case 'T':  c.type = LaserTile::Target; ++mTargetsTotal; break;
                    case '#':  c.type = LaserTile::Wall; break;
                    default:   c.type = LaserTile::Empty; break;
                }
                mGrid[static_cast<usize>(y * mGridW + x)] = c;
            }
        }
        mMoves = 0; mSelected = -1; mWon = false;
        Simulate();
        NKOUNG_LOG_INFOF("Niveau %d charge (%dx%d)", index + 1, mGridW, mGridH);
        return true;
    }

    void LaserPuzzleGame::Simulate() noexcept {
        mRay.Clear();
        mTargetsHit = 0;
        for (usize i = 0; i < mGrid.Size(); ++i)
            if (mGrid[i].type == LaserTile::Target) mGrid[i].hit = false;

        // Source
        int32 sx = -1, sy = -1, sdir = 0;
        for (int32 y = 0; y < mGridH; ++y)
            for (int32 x = 0; x < mGridW; ++x)
                if (At(x, y).type == LaserTile::Source) { sx = x; sy = y; sdir = At(x, y).dir; }
        if (sx < 0) { mWon = false; return; }

        const int32 DX[4] = { 1, 0, -1, 0 };
        const int32 DY[4] = { 0, 1, 0, -1 };
        int32 dx = DX[sdir], dy = DY[sdir], x = sx, y = sy;
        mRay.PushBack(math::NkVec2f{ static_cast<float32>(x), static_cast<float32>(y) });

        int32 guard = mGridW * mGridH * 4 + 8;
        while (guard-- > 0) {
            x += dx; y += dy;
            if (x < 0 || x >= mGridW || y < 0 || y >= mGridH) break;
            LaserCell& c = At(x, y);
            if (c.type == LaserTile::Wall) break;
            mRay.PushBack(math::NkVec2f{ static_cast<float32>(x), static_cast<float32>(y) });
            if (c.type == LaserTile::Mirror) {
                if (c.orient == 0) { int32 ndx = dy, ndy = dx; dx = ndx; dy = ndy; }    // "\"
                else               { int32 ndx = -dy, ndy = -dx; dx = ndx; dy = ndy; }  // "/"
            } else if (c.type == LaserTile::Target) {
                if (!c.hit) { c.hit = true; ++mTargetsHit; }
                break;
            }
        }
        mWon = (mTargetsTotal > 0 && mTargetsHit == mTargetsTotal);
    }

    void LaserPuzzleGame::RotateMirror(int32 idx) noexcept {
        if (idx < 0 || idx >= static_cast<int32>(mGrid.Size())) return;
        if (mGrid[idx].type != LaserTile::Mirror) return;
        mGrid[idx].orient ^= 1;
        ++mMoves;
        Simulate();
    }

    void LaserPuzzleGame::NextLevel() noexcept { LoadLevel((mLevelIndex + 1) % kLevelCount); }

    void LaserPuzzleGame::Update(float32 dt) noexcept { mAnim += dt; }

    void LaserPuzzleGame::Render(const NkoungFrame& frame) noexcept {
        const float32 SX = frame.safeX, SY = frame.safeY, SW = frame.safeW, SH = frame.safeH;
        const float32 headerH = 66.f, footerH = 62.f;

        // Zone du plateau (centrée), taille de cellule responsive.
        const float32 areaX = SX, areaY = SY + headerH;
        const float32 areaW = SW, areaH = SH - headerH - footerH;
        if (mGridW <= 0 || mGridH <= 0 || areaW <= 0.f || areaH <= 0.f) return;
        float32 cell = (areaW / mGridW < areaH / mGridH) ? areaW / mGridW : areaH / mGridH;
        cell *= 0.94f;
        const float32 boardW = cell * mGridW, boardH = cell * mGridH;
        const float32 boardX = areaX + (areaW - boardW) * 0.5f;
        const float32 boardY = areaY + (areaH - boardH) * 0.5f;

        // ── Entrée : tap/clic sur une cellule (rotation des miroirs) ──────
        if (frame.pointerPressed && cell > 0.f) {
            const float32 fx = (frame.pointer.x - boardX) / cell;
            const float32 fy = (frame.pointer.y - boardY) / cell;
            if (fx >= 0.f && fy >= 0.f) {
                const int32 gx = static_cast<int32>(fx), gy = static_cast<int32>(fy);
                if (gx < mGridW && gy < mGridH) {
                    const int32 idx = gy * mGridW + gx;
                    if (mGrid[static_cast<usize>(idx)].type == LaserTile::Mirror) {
                        mSelected = idx; RotateMirror(idx);
                    } else { mSelected = -1; }
                }
            }
        }

        // ── En-tête ────────────────────────────────────────────────────
        frame.Rect(SX, SY, SW, headerH, C::BG_SECONDARY());
        frame.Rect(SX, SY + headerH - 2.f, SW, 2.f, C::CYAN_BRIGHT());
        char buf[128];
        snprintf(buf, sizeof(buf), "Laser Puzzle  -  Niveau %d : %s", mLevelIndex + 1, kLevels[mLevelIndex].name);
        frame.Text(frame.font, SX + 16.f, SY + 10.f, buf, C::CYAN_BRIGHT());
        frame.Text(frame.font, SX + 16.f, SY + 38.f,
                   "Oriente les miroirs pour guider le rayon vers la cible doree.", C::TEXT_TERTIARY());
        snprintf(buf, sizeof(buf), "Coups : %d", mMoves);
        frame.Text(frame.font, SX + SW - 16.f - frame.TextW(frame.font, buf), SY + 12.f, buf, C::TEXT_SECONDARY());

        // ── Plateau ─────────────────────────────────────────────────────
        const int32 DX[4] = { 1, 0, -1, 0 };
        const int32 DY[4] = { 0, 1, 0, -1 };
        for (int32 y = 0; y < mGridH; ++y) {
            for (int32 x = 0; x < mGridW; ++x) {
                const LaserCell& c = At(x, y);
                const float32 px = boardX + x * cell, py = boardY + y * cell;
                const float32 pad = cell * 0.06f;
                const float32 cx = px + cell * 0.5f, cy = py + cell * 0.5f;
                const float32 rr = cell * 0.30f;
                const int32 idx = y * mGridW + x;

                frame.Rect(px + pad, py + pad, cell - 2 * pad, cell - 2 * pad, C::CARD_BG(), 6.f);
                if (idx == mSelected)
                    frame.Border(px + pad, py + pad, cell - 2 * pad, cell - 2 * pad, C::CYAN_BRIGHT(), 2.5f, 6.f);
                else
                    frame.Border(px + pad, py + pad, cell - 2 * pad, cell - 2 * pad, C::BORDER_SUBTLE(), 1.f, 6.f);

                switch (c.type) {
                    case LaserTile::Source:
                        frame.Circle(math::NkVec2f{ cx, cy }, rr, C::GREEN_SUCCESS(), 0);
                        frame.Line(math::NkVec2f{ cx, cy },
                                   math::NkVec2f{ cx + DX[c.dir] * rr * 1.4f, cy + DY[c.dir] * rr * 1.4f },
                                   C::BG_DARK(), cell * 0.06f);
                        break;
                    case LaserTile::Target:
                        if (c.hit) frame.Circle(math::NkVec2f{ cx, cy }, rr, C::GREEN_SUCCESS(), 0);
                        else       frame.CircleOutline(math::NkVec2f{ cx, cy }, rr, math::NkColor{ 255, 215, 0, 255 }, 3.f, 0);
                        break;
                    case LaserTile::Mirror: {
                        const float32 m = cell * 0.30f;
                        math::NkVec2f a, b;
                        if (c.orient == 0) { a = { cx - m, cy - m }; b = { cx + m, cy + m }; }  // "\"
                        else               { a = { cx - m, cy + m }; b = { cx + m, cy - m }; }  // "/"
                        const math::NkColor mc = (idx == mSelected) ? C::CYAN_BRIGHT()
                                                                    : math::NkColor{ 185, 205, 235, 255 };
                        frame.Line(a, b, mc, cell * 0.10f);
                        break;
                    }
                    case LaserTile::Wall:
                        frame.Rect(px + pad * 2, py + pad * 2, cell - 4 * pad, cell - 4 * pad, C::TEXT_TERTIARY(), 4.f);
                        break;
                    default: break;
                }
            }
        }

        // ── Rayon (lueur + cœur, pulsé) ─────────────────────────────────
        if (mRay.Size() >= 2 && cell > 0.f) {
            float32 t = mAnim * 1.5f; t -= static_cast<float32>(static_cast<int32>(t));
            const float32 pulse = (t < 0.5f) ? t * 2.f : (1.f - t) * 2.f;   // triangle 0..1
            const uint8 glowA = static_cast<uint8>(70.f + pulse * 80.f);
            for (usize i = 1; i < mRay.Size(); ++i) {
                const math::NkVec2f a{ boardX + (mRay[i - 1].x + 0.5f) * cell, boardY + (mRay[i - 1].y + 0.5f) * cell };
                const math::NkVec2f b{ boardX + (mRay[i].x + 0.5f) * cell,     boardY + (mRay[i].y + 0.5f) * cell };
                frame.Line(a, b, math::NkColor{ 255, 0, 110, glowA }, cell * 0.18f);   // lueur
                frame.Line(a, b, math::NkColor{ 255, 140, 190, 255 }, cell * 0.05f);   // cœur
            }
        }

        // ── Bandeau de victoire ─────────────────────────────────────────
        if (mWon) {
            const char* msg = "Niveau reussi !";
            nkui::NkUIFont* tf = frame.titleFont ? frame.titleFont : frame.font;
            const float32 bw = frame.TextW(tf, msg) + 48.f;
            const float32 bx = SX + (SW - bw) * 0.5f, by = areaY + 6.f;
            frame.Rect(bx, by, bw, 46.f, math::NkColor{ 0, 208, 132, 45 }, 12.f);
            frame.Border(bx, by, bw, 46.f, C::GREEN_SUCCESS(), 1.5f, 12.f);
            frame.TextCentered(tf, bx, bw, by + 8.f, msg, C::GREEN_SUCCESS());
        }

        // ── Pied : boutons (souris + tactile) ───────────────────────────
        const float32 fy = SY + SH - footerH + 8.f;
        const float32 bh = footerH - 16.f;
        if (frame.Button(SX + 16.f, fy, 110.f, bh, "Menu",
                         C::BG_TERTIARY(), C::CYAN_MID(), C::TEXT_PRIMARY()))
            mWantExit = true;
        if (frame.Button(SX + 16.f + 122.f, fy, 150.f, bh, "Recommencer",
                         C::BG_TERTIARY(), C::CYAN_MID(), C::TEXT_PRIMARY()))
            LoadLevel(mLevelIndex);
        if (mWon) {
            const math::NkColor brd = C::GREEN_SUCCESS();
            if (frame.Button(SX + SW - 16.f - 170.f, fy, 170.f, bh, "Niveau suivant >",
                             C::GREEN_SUCCESS(), C::GRADIENT_GREEN_END(), C::BG_DARK(), &brd))
                NextLevel();
        }
    }

    void LaserPuzzleGame::OnEvent(NkEvent* event) noexcept {
        if (!event) return;
        auto* kp = event->As<NkKeyPressEvent>();
        if (!kp) return;
        switch (kp->GetKey()) {
            case NkKey::NK_R:      if (mSelected >= 0) RotateMirror(mSelected); break;
            case NkKey::NK_ESCAPE: mWantExit = true; break;
            case NkKey::NK_ENTER:  if (mWon) NextLevel(); break;
            default: break;
        }
    }

    void LaserPuzzleGame::Unload() noexcept {
        mGrid.Clear();
        mRay.Clear();
        NKOUNG_LOG_INFO("Laser Puzzle decharge");
    }

    const char* LaserPuzzleGame::GetCurrentLevelTitle() const noexcept {
        return kLevels[mLevelIndex].name;
    }

    float32 LaserPuzzleGame::GetProgress() const noexcept {
        return (mTargetsTotal > 0) ? static_cast<float32>(mTargetsHit) / mTargetsTotal : 0.f;
    }

}  // namespace nkoung
