#pragma once
// =============================================================================
// SongooBoard.h
// =============================================================================
// Moteur de logique Mancala — gestion de l'état du plateau, mouvements,
// captures en chaîne (PRISE), conditions de victoire.
// Implémentation conforme à la référence Songo'o.
// =============================================================================

namespace nkentseu
{
    namespace songoo
    {

        class SongooBoard
        {
        public:
            // Layout du plateau : 14 pits total
            // Joueur 1 (bas)  : pits[0..5] (gauche vers droite), mancala[0] (droite)
            // Joueur 2 (haut) : pits[6..11] (droite vers gauche), mancala[1] (gauche)
            static constexpr int kNumRegularPits = 12;
            static constexpr int kNumMancalas    = 2;
            static constexpr int kNumPits        = kNumRegularPits + kNumMancalas;
            static constexpr int kInitialGrains  = 4;

            // Statut de capture
            enum class PriseStatus { NOT_PRISE = 0, PRISE = 1 };

            SongooBoard();
            ~SongooBoard() = default;

            // Initialise le plateau au état de départ
            void Init();

            // Retourne le joueur actuel (0 ou 1)
            int GetCurrentPlayer() const { return mCurrentPlayer; }

            // Vérifie si un pit peut être joué par le joueur
            bool CanPlayPit(int playerIdx, int pitIdx) const;

            // Exécute un mouvement. Retourne true si le joueur a une extra chance.
            bool ExecuteMove(int playerIdx, int pitIdx);

            // Retourne le nombre de grains dans un pit (0..13 : pits[0..11] + mancala[0..1])
            int GetPitGrains(int pitIdx) const;

            // Retourne les grains du mancala d'un joueur
            int GetMancalaGrains(int playerIdx) const;

            // Retourne -1 si la partie n'est pas finie, sinon 0/1 = gagnant
            int CheckGameOver() const;

            // Retourne le nombre de pits vides pour un joueur
            int CountEmptyPits(int playerIdx) const;

            // Dump l'état du plateau (debug)
            void DebugPrint() const;

        private:
            int mPits[kNumPits];       // pits[0..11] = pits réguliers, pits[12..13] = mancalas
            int mCurrentPlayer;        // 0 ou 1
            bool mGameOver;
            PriseStatus mPriseStatus[kNumPits]; // Statut PRISE (2-3 grains) pour chaque pit

            // Ordre de distribution horaire : 0,1,2,3,4,5,6,13,12,11,10,9,8,7
            static constexpr int kClockwise[kNumPits] = {
                0, 1, 2, 3, 4, 5, 6, 13, 12, 11, 10, 9, 8, 7
            };
            int mClockwisePos[kNumPits];  // Position inverse : pour un pit donné, sa position dans kClockwise

            // Helpers
            int  GetPitOwner(int pitIdx) const;
            int  GetOppositePit(int pitIdx) const;
            bool IsPlayerMancala(int playerIdx, int pitIdx) const;
            bool IsGameBoardEmpty(int playerIdx) const;
            int  GetNextPitClockwise(int pitIdx) const;
            int  GetLastPitForMove(int pitIdx) const;
            void CheckPrise();
            void CheckGain(int lastPitIdx, int playerIdx);
        };

    } // namespace songoo
} // namespace nkentseu
