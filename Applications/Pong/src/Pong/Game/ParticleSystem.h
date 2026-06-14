#pragma once
// =============================================================================
// ParticleSystem.h
// -----------------------------------------------------------------------------
// Systeme de particules leger pour le "juice" visuel de Pong.
// Pas de physique avancee : chaque particule = position + velocite + vie +
// couleur + taille, mise a jour lineaire, alpha decroissant avec la vie.
//
// Emis a chaque evenement gameplay (rebond paddle, hit obstacle, collecte
// bonus, drop catched, goal). Le caller passe directement les params au
// helper d'emission (EmitBurst / EmitDirectional / EmitGoal).
//
// Cap a kMaxParticles pour ne jamais derouler la perf en fin de match
// (les plus anciennes sont evincees quand le cap est atteint).
// =============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkColor.h"
#include "NKCore/NkTypes.h"

namespace nkentseu { namespace renderer { class NkRenderer2D; } }

namespace nkentseu
{
    namespace pong
    {


        // ── Particule simple (dot) ───────────────────────────────────────────
        // Toutes en coords MONDE de l'arene (sans offset HUD). Le caller du
        // Render passe (ax, ay) = origine arene pour traduire en coords ecran.
        struct Particle
        {
            float x      = 0.0f;
            float y      = 0.0f;
            float vx     = 0.0f;
            float vy     = 0.0f;
            float life   = 0.0f;   ///< secondes ecoulees depuis spawn
            float maxLife= 1.0f;   ///< duree de vie totale
            float size   = 3.0f;   ///< rayon affiche (en px monde)
            float drag   = 0.92f;  ///< amortissement par seconde (1.0 = aucun)
            math::NkColor color;   ///< couleur (alpha modulee par 1-life/maxLife)
        };

        class ParticleSystem
        {
        public:
            static constexpr int kMaxParticles = 512;

            void Reset();
            void Update(float dt);
            void Render(renderer::NkRenderer2D& r, float arenaOX, float arenaOY) const;

            // ── Helpers d'emission ──────────────────────────────────────────
            /// Burst omnidirectionnel : @p count particules dispersees autour
            /// de (x,y) avec vitesses aleatoires dans [speedMin, speedMax] et
            /// duree de vie dans [lifeMin, lifeMax].
            void EmitBurst(float x, float y, int count,
                           math::NkColor color,
                           float speedMin, float speedMax,
                           float lifeMin , float lifeMax,
                           float sizeMin , float sizeMax);

            /// Burst directionnel : particules emises dans un cone centre sur
            /// @p dirAngle (rad), demi-ouverture @p spread (rad). Utile pour
            /// l'impact paddle (jet dans la direction de rebond).
            void EmitDirectional(float x, float y, int count,
                                 math::NkColor color,
                                 float dirAngle, float spread,
                                 float speedMin, float speedMax,
                                 float lifeMin , float lifeMax,
                                 float sizeMin , float sizeMax);

            /// Explosion de goal : 40 particules dispersees sur la ligne de
            /// but (vertical x=arenaX), de la couleur du marqueur.
            void EmitGoal(float wallX, float arenaH, math::NkColor color);

            int Count() const noexcept { return (int)mParts.Size(); }

        private:
            NkVector<Particle> mParts;
            /// Ajoute une particule, en evictant la plus ancienne si on a
            /// atteint le cap kMaxParticles.
            void Push(const Particle& p);
        };

    } // namespace pong
} // namespace nkentseu
