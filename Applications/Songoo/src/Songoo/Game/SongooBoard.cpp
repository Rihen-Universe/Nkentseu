// =============================================================================
// SongooBoard.cpp — CORRIGÉ (bugs 7, 8, 10)
// =============================================================================

#include "SongooBoard.h"
#include <cstring>

namespace nkentseu { namespace songoo {

    void SongooBoard::Init() {
        for (int i = 0; i < kNumPits; i++) mPits[i] = kInitGrains;
        mScore[0] = mScore[1] = 0;
        mCurrentPlayer = 0;
        for (int i = 0; i < kNumPits; i++) mCWPos[kCW[i]] = i;
    }

    int SongooBoard::NextPit(int pit) const {
        return kCW[(mCWPos[pit] + 1) % kNumPits];
    }

    bool SongooBoard::CanPlay(int player, int pit) const {
        if (pit < 0 || pit >= kNumPits) return false;
        if (player == 0 && pit >= 7)    return false;
        if (player == 1 && pit <  7)    return false;
        return mPits[pit] > 0;
    }

    bool SongooBoard::IsCampEmpty(int player) const {
        int s = (player == 0) ? 0 : 7;
        int e = (player == 0) ? 7 : 14;
        for (int i = s; i < e; i++) if (mPits[i] > 0) return false;
        return true;
    }

    // ── CORRECTION BUG 7 ─────────────────────────────────────────────────────
    // L'original décrémentait i-- inconditionnellement et vérifiait le camp
    // APRÈS → pouvait capturer ses propres graines.
    // Ici : on remonte depuis lastPit TANT QUE le trou est en zone adverse
    // ET a 2 ou 3 graines.
    void SongooBoard::DoCapture(int lastPit, int player) {
        int i = lastPit;
        while (i >= 0) {
            int g = mPits[i];
            if (g != 2 && g != 3) break;

            // Vérifier que le trou est dans le camp ADVERSE
            bool inOpponent = (player == 0) ? (i >= 7) : (i < 7);
            if (!inOpponent) break;

            mScore[player] += g;
            mPits[i] = 0;

            // Reculer dans la table horaire
            int pos  = mCWPos[i];
            int prev = (pos - 1 + kNumPits) % kNumPits;
            i = kCW[prev];
        }
    }

    SongooBoard::MoveResult SongooBoard::ExecuteMove(int player, int pit) {
        MoveResult res;
        res.traceLen = 0;
        res.lastPit  = pit;
        res.gameOver = false;
        res.winner   = -2;

        int grains = mPits[pit];
        if (grains == 0) return res;

        mPits[pit] = 0;

        int cur = pit;
        for (int k = 0; k < grains; k++) {
            cur = NextPit(cur);
            if (cur == pit) cur = NextPit(cur); // sauter la source
            mPits[cur]++;
            if (res.traceLen < kNumPits)
                res.trace[res.traceLen++] = cur;
        }
        res.lastPit = cur;

        DoCapture(cur, player);

        // ── CORRECTION BUG 8 ─────────────────────────────────────────────────
        // CheckGameOver() est appelé UNE FOIS, HORS des boucles sur les trous.
        int go = CheckGameOver();
        if (go != -2) {
            if (go == -1 || go == 0 || go == 1) {
                CollectRemaining();
            }
            res.gameOver = true;
            res.winner   = go;
        }

        mCurrentPlayer = 1 - player;
        return res;
    }

    // ── CORRECTION BUG 8 ─────────────────────────────────────────────────────
    // L'original vérifiait joueur->GetGain() >= 37 DANS la boucle sur les trous
    // (donc seulement pour les 7 premiers trous), et campVide s'arrêtait
    // au premier trou non-vide sans vérifier hasWinner.
    int SongooBoard::CheckGameOver() const {
        // Condition 1 : un joueur a capturé 37+ graines
        if (mScore[0] >= 37) return 0;
        if (mScore[1] >= 37) return 1;

        // Condition 2 : un camp est entièrement vide
        if (IsCampEmpty(0) || IsCampEmpty(1)) {
            if (mScore[0] > mScore[1]) return 0;
            if (mScore[1] > mScore[0]) return 1;
            return -1; // égalité
        }

        return -2; // partie en cours
    }

    void SongooBoard::CollectRemaining() {
        for (int i = 0; i <  7; i++) { mScore[0] += mPits[i]; mPits[i] = 0; }
        for (int i = 7; i < 14; i++) { mScore[1] += mPits[i]; mPits[i] = 0; }
    }

}} // namespace nkentseu::songoo
