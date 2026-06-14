#pragma once
// =============================================================================
// AIController.h
// -----------------------------------------------------------------------------
// IA pour piloter un paddle. 6 niveaux de difficulte selon GDD §3.1 mappes
// sur 3 axes :
//   - speed       (0..1) : fraction de la vitesse paddle max utilisable
//   - precision   (0..1) : exactitude du targeting (1 = parfait, 0 = aleatoire)
//   - anticipation (0..2): profondeur de prediction de la trajectoire balle
//     0 = aucune (suit Y de la balle en direct)
//     1 = projection lineaire jusqu'au paddle
//     2 = projection avec rebonds murs
//
// Usage :
//   AIController ai(AIDifficulty::Competitor);
//   float dy = ai.Update(dt, ballX, ballY, ballVX, ballVY,
//                        paddleX, paddleY, paddleH,
//                        arenaW, arenaH, maxPaddleSpd);
//   paddleY += dy;
// =============================================================================

#include "Pong/Game/GameTypes.h"
#include "NKCore/NkTypes.h"

namespace nkentseu
{
    namespace pong
    {

        class AIController
        {
        public:
            AIController() = default;
            explicit AIController(AIDifficulty d) { SetDifficulty(d); }

            /// Reconfigure les parametres selon le niveau choisi.
            void SetDifficulty(AIDifficulty d);

            /// Calcule le deplacement vertical (en pixels) a appliquer au
            /// paddle ce frame. Le signe = direction (negatif = vers le haut).
            /// @param dt              delta-time du frame (secondes)
            /// @param ballX/Y         position courante de la balle
            /// @param ballVX/Y        vitesse courante balle (px/frame@60fps,
            ///                        meme convention que GameplayScene)
            /// @param paddleX         X du paddle (utilise pour predire le temps
            ///                        de collision)
            /// @param paddleY         Y courant du paddle (haut-gauche)
            /// @param paddleH         hauteur du paddle
            /// @param arenaW/H        dimensions de l'arene
            /// @param maxPaddleSpd    vitesse max paddle (px/frame@60fps)
            /// @return delta Y a ajouter a paddleY ce frame (px).
            float Update(float dt,
                         float ballX, float ballY,
                         float ballVX, float ballVY,
                         float paddleX, float paddleY, float paddleH,
                         float arenaW, float arenaH,
                         float maxPaddleSpd);

            AIDifficulty GetDifficulty() const noexcept { return mDifficulty; }

        private:
            AIDifficulty mDifficulty   = AIDifficulty::Competitor;
            float        mSpeed        = 0.65f;  // 0..1
            float        mPrecision    = 0.78f;  // 0..1
            int          mAnticipation = 1;      // 0, 1 ou 2

            // Etat interne pour le mode Chaos (decisions aleatoires periodiques)
            float        mChaosTimer   = 0.0f;
            float        mChaosTargetY = 0.0f;
        };

    } // namespace pong
} // namespace nkentseu
