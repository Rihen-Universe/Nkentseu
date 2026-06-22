#pragma once
// =============================================================================
// SongooBoard.h — Logique plateau Songo'o CORRIGÉE
//
// CORRECTIONS :
//   BUG 7 : CheckGain décrémentait i sans valider le camp adverse au préalable
//   BUG 8 : IsGameOver vérifiait hasWinner À L'INTÉRIEUR de la boucle trous
//   BUG 10: Le plateau logique était exécuté APRÈS que l'animation ait déjà
//           vidé le trou source visuellement → désync.
//           SOLUTION : Le plateau maintient son propre état; l'animation
//           est purement visuelle et ne modifie JAMAIS mBoard directement.
//           ExecuteMove() est appelé UNE SEULE FOIS dans FinishAnimation().
// =============================================================================

namespace nkentseu { namespace songoo {

    class SongooBoard {
    public:
        static constexpr int kNumPits    = 14;
        static constexpr int kInitGrains = 5;

        SongooBoard() { Init(); }

        void Init();

        // ── Accès lecture (pour affichage) ───────────────────────────────────
        int  GetCurrentPlayer()       const { return mCurrentPlayer; }
        int  GetPitGrains(int pit)    const { return (pit>=0&&pit<kNumPits)?mPits[pit]:0; }
        int  GetScore(int player)     const { return (player>=0&&player<2)?mScore[player]:0; }

        // ── Validation ────────────────────────────────────────────────────────
        // Retourne true si le joueur peut légalement jouer ce trou
        bool CanPlay(int player, int pit) const;

        // ── Coup ──────────────────────────────────────────────────────────────
        // Exécute la distribution + capture, change le joueur courant.
        // Retourne la liste ordonnée des trous visités (pour l'animation).
        struct MoveResult {
            int  trace[kNumPits]; // trous visités dans l'ordre
            int  traceLen;        // nombre de trous visités
            int  lastPit;         // dernier trou semé
            bool gameOver;
            int  winner;          // -1=match nul, 0=J0, 1=J1 (valide si gameOver)
        };
        MoveResult ExecuteMove(int player, int pit);

        // ── Fin de partie ─────────────────────────────────────────────────────
        // CORRECTION BUG 8 : vérification hors des boucles trous
        // Retourne -2 si la partie continue
        // Retourne -1 si égalité, 0 ou 1 sinon
        int  CheckGameOver() const;

        // Ramasse toutes les graines restantes (fin de partie)
        void CollectRemaining();

    private:
        int mPits[kNumPits];
        int mScore[2];
        int mCurrentPlayer;

        // Table de parcours horaire : 0,1,2,3,4,5,6,13,12,11,10,9,8,7
        static constexpr int kCW[kNumPits] = {0,1,2,3,4,5,6,13,12,11,10,9,8,7};
        int mCWPos[kNumPits]; // position inverse

        int  NextPit(int pit) const;
        bool IsCampEmpty(int player) const;

        // CORRECTION BUG 7 : capture strictement dans le camp adverse,
        // en remontant depuis lastPit TANT QUE la condition est remplie.
        void DoCapture(int lastPit, int player);
    };

}} // namespace nkentseu::songoo
