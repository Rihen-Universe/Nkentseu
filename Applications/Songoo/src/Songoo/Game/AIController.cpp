// =============================================================================
// AIController.cpp
// =============================================================================

#include "AIController.h"
#include "NKMath/NkFunctions.h"
#include <cstdlib>

namespace nkentseu
{
    namespace songoo
    {

        // ─────────────────────────────────────────────────────────────────────
        // Mapping AIDifficulty -> (speed, precision, anticipation)
        // Valeurs alignees avec docs/01_splash_et_menus.html (cards Difficulty)
        // ─────────────────────────────────────────────────────────────────────
        void AIController::SetDifficulty(AIDifficulty d)
        {
            mDifficulty = d;
            switch (d)
            {
            case AIDifficulty::Beginner:
                mSpeed = 0.20f; mPrecision = 0.40f; mAnticipation = 0; break;
            case AIDifficulty::Apprentice:
                mSpeed = 0.40f; mPrecision = 0.60f; mAnticipation = 1; break;
            case AIDifficulty::Competitor:
                mSpeed = 0.65f; mPrecision = 0.78f; mAnticipation = 1; break;
            case AIDifficulty::Expert:
                mSpeed = 0.85f; mPrecision = 0.92f; mAnticipation = 2; break;
            case AIDifficulty::Legend:
                mSpeed = 1.00f; mPrecision = 0.99f; mAnticipation = 2; break;
            case AIDifficulty::Chaos:
                mSpeed = 0.80f; mPrecision = 0.50f; mAnticipation = 1; break;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // Random helpers
        // ─────────────────────────────────────────────────────────────────────
        static float Rand01()
        {
            return (float)std::rand() / (float)RAND_MAX;
        }
        static float Rand11()
        {
            return Rand01() * 2.0f - 1.0f;
        }

        // ─────────────────────────────────────────────────────────────────────
        // PredictBallY — projete la trajectoire de la balle jusqu'a paddleX.
        // anticipation = 0  : retourne ballY direct (suivi naif)
        // anticipation = 1  : projection lineaire (sans rebonds)
        // anticipation = 2  : projection avec rebonds murs haut/bas
        // ─────────────────────────────────────────────────────────────────────
        static float PredictBallY(float ballX, float ballY,
                                  float ballVX, float ballVY,
                                  float paddleX,
                                  float arenaH,
                                  int anticipation)
        {
            if (anticipation == 0) return ballY;
            // Si la balle va dans le mauvais sens (s'eloigne du paddle),
            // l'IA ne sait pas ou viser. On la fait revenir au centre.
            const float dx = paddleX - ballX;
            if ((dx > 0.0f && ballVX <= 0.0f) || (dx < 0.0f && ballVX >= 0.0f))
            {
                return arenaH * 0.5f;
            }
            // Temps jusqu'a impact (px / px par frame = frames)
            const float frames = (ballVX != 0.0f) ? (dx / ballVX) : 0.0f;
            float projY = ballY + ballVY * frames;
            if (anticipation >= 2)
            {
                // Simulation rebonds haut/bas
                if (arenaH <= 0.0f) return projY;
                float y = projY;
                // Refleter dans [0, arenaH] (mirror sawtooth)
                const float twoH = arenaH * 2.0f;
                y = math::NkFmod(y, twoH);
                if (y < 0.0f) y += twoH;
                if (y > arenaH) y = twoH - y;
                projY = y;
            }
            return projY;
        }

        // ─────────────────────────────────────────────────────────────────────
        // Update — calcule le delta Y a appliquer.
        // ─────────────────────────────────────────────────────────────────────
        float AIController::Update(float dt,
                                   float ballX, float ballY,
                                   float ballVX, float ballVY,
                                   float paddleX, float paddleY, float paddleH,
                                   float arenaW, float arenaH,
                                   float maxPaddleSpd)
        {
            (void)arenaW;

            // Mode Chaos : decisions aleatoires periodiques pour deplacement
            // erratique (le user sait pas a quoi s'attendre).
            if (mDifficulty == AIDifficulty::Chaos)
            {
                mChaosTimer -= dt;
                if (mChaosTimer <= 0.0f)
                {
                    mChaosTargetY = Rand01() * (arenaH - paddleH);
                    mChaosTimer   = 0.3f + Rand01() * 0.8f;  // 0.3 - 1.1s
                }
                const float current = paddleY + paddleH * 0.5f;
                const float dir = (mChaosTargetY + paddleH * 0.5f) - current;
                const float step = maxPaddleSpd * mSpeed * dt * 60.0f;
                if (math::NkFabs(dir) <= step) return dir;
                return (dir > 0.0f ? step : -step);
            }

            // Cible : prediction balle (anticipation selon niveau)
            float targetY = PredictBallY(ballX, ballY, ballVX, ballVY,
                                         paddleX, arenaH, mAnticipation);

            // Bruit d'imprecision : (1 - precision) * arenaH * 0.15
            // Plus la precision est basse, plus la cible "vrille".
            const float errorAmp = (1.0f - mPrecision) * arenaH * 0.15f;
            targetY += Rand11() * errorAmp;

            // Centre du paddle aligne sur la cible
            const float paddleCenter = paddleY + paddleH * 0.5f;
            const float diff         = targetY - paddleCenter;

            // Vitesse maximale ce frame (px/frame @60fps -> dt-corrected)
            const float step = maxPaddleSpd * mSpeed * dt * 60.0f;

            // Zone morte : sous 0.5 step, on ne bouge pas (evite jitter)
            if (math::NkFabs(diff) < step * 0.5f) return 0.0f;

            return (diff > 0.0f) ? step : -step;
        }

    } // namespace songoo
} // namespace nkentseu
