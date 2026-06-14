// =============================================================================
// SongooBoard.cpp
// =============================================================================

#include "SongooBoard.h"
#include "NKLogger/NkLog.h"

namespace nkentseu
{
    namespace songoo
    {

        // ─────────────────────────────────────────────────────────────────────
        SongooBoard::SongooBoard()
            : mCurrentPlayer(0), mGameOver(false)
        {
            for (int i = 0; i < kNumPits; ++i)
            {
                mPits[i] = 0;
                mPriseStatus[i] = PriseStatus::NOT_PRISE;
            }

            // Initialiser mClockwisePos : pour chaque pit, sa position dans kClockwise
            for (int i = 0; i < kNumPits; ++i)
            {
                mClockwisePos[kClockwise[i]] = i;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void SongooBoard::Init()
        {
            // Initialise 4 grains par pit régulier, 0 pour les mancalas
            for (int i = 0; i < kNumRegularPits; ++i)
                mPits[i] = kInitialGrains;
            mPits[12] = 0;  // Mancala joueur 0
            mPits[13] = 0;  // Mancala joueur 1

            for (int i = 0; i < kNumPits; ++i)
                mPriseStatus[i] = PriseStatus::NOT_PRISE;

            mCurrentPlayer = 0;
            mGameOver = false;
        }

        // ─────────────────────────────────────────────────────────────────────
        int SongooBoard::GetPitOwner(int pitIdx) const
        {
            // Pits 0..5 et mancala 12 = joueur 0
            // Pits 6..11 et mancala 13 = joueur 1
            if (pitIdx == 12) return 0;
            if (pitIdx == 13) return 1;
            return (pitIdx < 6) ? 0 : 1;
        }

        int SongooBoard::GetOppositePit(int pitIdx) const
        {
            // Pit 0 <-> pit 11, pit 1 <-> pit 10, etc.
            if (pitIdx >= 6 || pitIdx < 0) return -1;
            return 11 - pitIdx;
        }

        bool SongooBoard::IsPlayerMancala(int playerIdx, int pitIdx) const
        {
            return (playerIdx == 0 && pitIdx == 12) || (playerIdx == 1 && pitIdx == 13);
        }

        bool SongooBoard::IsGameBoardEmpty(int playerIdx) const
        {
            const int startIdx = (playerIdx == 0) ? 0 : 6;
            const int endIdx = startIdx + 6;
            for (int i = startIdx; i < endIdx; ++i)
            {
                if (mPits[i] > 0) return false;
            }
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────
        int SongooBoard::GetNextPitClockwise(int pitIdx) const
        {
            // Retourne le pit suivant dans l'ordre horaire
            int pos = mClockwisePos[pitIdx];
            return kClockwise[(pos + 1) % kNumPits];
        }

        int SongooBoard::GetLastPitForMove(int pitIdx) const
        {
            // Calcule dans quel pit atterrit la dernière graine
            int grains = mPits[pitIdx];
            if (grains == 0) return pitIdx;

            int last = pitIdx;
            for (int i = 0; i < grains; ++i)
            {
                last = GetNextPitClockwise(last);
                // Sauter le pit source
                if (last == pitIdx)
                    last = GetNextPitClockwise(last);
            }
            return last;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SongooBoard::CheckPrise()
        {
            // Marquer tous les pits avec 2 ou 3 grains comme PRISE
            for (int i = 0; i < kNumPits; ++i)
            {
                int n = mPits[i];
                mPriseStatus[i] = (n == 2 || n == 3) ? PriseStatus::PRISE : PriseStatus::NOT_PRISE;
            }
        }

        void SongooBoard::CheckGain(int lastPitIdx, int playerIdx)
        {
            // Logique de capture en chaîne arrière
            // Commence à lastPitIdx et remonte (i--)
            // Capture les pits PRISE de l'adversaire uniquement
            CheckPrise();

            int i = lastPitIdx;
            while (i >= 0 && mPriseStatus[i] == PriseStatus::PRISE)
            {
                CheckPrise();

                // Joueur 0 capture les pits de l'adversaire (i > 6)
                if (playerIdx == 0 && i > 6)
                {
                    const int mancalaIdx = 12;
                    mPits[mancalaIdx] += mPits[i];
                    mPits[i] = 0;
                    mPriseStatus[i] = PriseStatus::NOT_PRISE;
                }
                // Joueur 1 capture les pits de l'adversaire (i <= 6)
                else if (playerIdx == 1 && i <= 6)
                {
                    const int mancalaIdx = 13;
                    mPits[mancalaIdx] += mPits[i];
                    mPits[i] = 0;
                    mPriseStatus[i] = PriseStatus::NOT_PRISE;
                }

                i--;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        bool SongooBoard::CanPlayPit(int playerIdx, int pitIdx) const
        {
            // Vérifie que le pit appartient au joueur et n'est pas vide
            if (GetPitOwner(pitIdx) != playerIdx) return false;
            if (pitIdx >= 12) return false;  // Ne peut pas jouer depuis un mancala
            if (mPits[pitIdx] == 0) return false;
            if (mGameOver) return false;
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────
        bool SongooBoard::ExecuteMove(int playerIdx, int pitIdx)
        {
            if (!CanPlayPit(playerIdx, pitIdx)) return false;

            // Étape 1 : Prélever les grains
            int grains = mPits[pitIdx];
            mPits[pitIdx] = 0;
            int currentPit = pitIdx;
            int lastPit = -1;

            // Étape 2 : Distribuer les grains en ordre horaire
            for (int i = 0; i < grains; ++i)
            {
                currentPit = GetNextPitClockwise(currentPit);
                // Sauter le pit source
                if (currentPit == pitIdx)
                    currentPit = GetNextPitClockwise(currentPit);

                mPits[currentPit]++;
                lastPit = currentPit;
            }

            // Étape 3 : Vérifier les captures en chaîne (PRISE)
            if (lastPit >= 0 && lastPit < 12)  // Dernière graine dans un pit régulier
            {
                CheckGain(lastPit, playerIdx);
            }

            // Étape 4 : Vérifier une chance supplémentaire
            bool extraTurn = IsPlayerMancala(playerIdx, lastPit);

            // Étape 5 : Changer de joueur (sauf si extra chance)
            if (!extraTurn)
            {
                mCurrentPlayer = 1 - mCurrentPlayer;
            }

            // Étape 6 : Vérifier si partie est finie
            if (IsGameBoardEmpty(0) || IsGameBoardEmpty(1))
            {
                mGameOver = true;
                // Collecter les grains restants
                for (int i = 0; i < 6; ++i)
                {
                    mPits[12] += mPits[i];
                    mPits[i] = 0;
                }
                for (int i = 6; i < 12; ++i)
                {
                    mPits[13] += mPits[i];
                    mPits[i] = 0;
                }
            }

            return extraTurn;
        }

        // ─────────────────────────────────────────────────────────────────────
        int SongooBoard::GetPitGrains(int pitIdx) const
        {
            if (pitIdx < 0 || pitIdx >= kNumPits) return 0;
            return mPits[pitIdx];
        }

        int SongooBoard::GetMancalaGrains(int playerIdx) const
        {
            if (playerIdx < 0 || playerIdx > 1) return 0;
            return mPits[(playerIdx == 0) ? 12 : 13];
        }

        int SongooBoard::CheckGameOver() const
        {
            if (!mGameOver) return -1;
            const int p1Score = mPits[12];
            const int p2Score = mPits[13];
            return (p1Score > p2Score) ? 0 : 1;
        }

        int SongooBoard::CountEmptyPits(int playerIdx) const
        {
            int count = 0;
            const int startIdx = (playerIdx == 0) ? 0 : 6;
            for (int i = startIdx; i < startIdx + 6; ++i)
            {
                if (mPits[i] == 0) count++;
            }
            return count;
        }

        void SongooBoard::DebugPrint() const
        {
            logger.Info("[Board] P2: {} {} {} {} {} {}", mPits[11], mPits[10], mPits[9], mPits[8], mPits[7], mPits[6]);
            logger.Info("[Board] M2: {} M1: {}", mPits[13], mPits[12]);
            logger.Info("[Board] P1: {} {} {} {} {} {}", mPits[0], mPits[1], mPits[2], mPits[3], mPits[4], mPits[5]);
            logger.Info("[Board] Current: P{}, GameOver: {}", mCurrentPlayer, mGameOver);
        }

    } // namespace songoo
} // namespace nkentseu
