// =============================================================================
// ParticleSystem.cpp
// =============================================================================

#include "ParticleSystem.h"
#include "Pong/Render/GLRenderer2D.h"
#include "NKMath/NkFunctions.h"
#include <cstdlib>

namespace nkentseu
{
    namespace pong
    {

        // ── Helpers locaux ───────────────────────────────────────────────────
        static float Rand01() { return (float)std::rand() / (float)RAND_MAX; }
        static float Rand11() { return Rand01() * 2.0f - 1.0f; }
        static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

        // ── Lifecycle ────────────────────────────────────────────────────────
        void ParticleSystem::Reset()
        {
            mParts.Clear();
        }

        // Met a jour position, applique le drag (amortissement) et incremente
        // l'age. Supprime les particules mortes via swap-with-last (O(1) par
        // suppression).
        void ParticleSystem::Update(float dt)
        {
            for (uint32 i = 0; i < mParts.Size(); )
            {
                Particle& p = mParts[i];
                p.life += dt;
                if (p.life >= p.maxLife)
                {
                    // Swap with last + pop (O(1)). Si i == taille-1, juste pop.
                    const uint32 last = mParts.Size() - 1;
                    if (i != last) mParts[i] = mParts[last];
                    mParts.RemoveAt(last);
                    continue;
                }
                // Integration : position += velocite. Drag applique chaque
                // frame (drag^dt). drag=0.92 ~ ralentit doucement.
                p.x += p.vx * dt;
                p.y += p.vy * dt;
                const float k = math::NkPow(p.drag, dt);
                p.vx *= k;
                p.vy *= k;
                ++i;
            }
        }

        // Rendu : cercles pleins de rayon decroissant + alpha decroissant
        // avec l'age (life/maxLife).
        void ParticleSystem::Render(GLRenderer2D& r, float ax, float ay) const
        {
            for (uint32 i = 0; i < mParts.Size(); ++i)
            {
                const Particle& p = mParts[i];
                const float t = math::NkClamp(p.life / p.maxLife, 0.0f, 1.0f);
                const float a01 = 1.0f - t;   // fade lineaire
                math::NkColor c = p.color;
                c.a = static_cast<uint8_t>(c.a * a01);
                // Taille decroissante (legerement) pour effet "puff"
                const float radius = math::NkMax(0.5f, p.size * (1.0f - t * 0.40f));
                r.DrawCircle(ax + p.x, ay + p.y, radius, c, 10);
            }
        }

        // ── Emission helpers ─────────────────────────────────────────────────
        void ParticleSystem::Push(const Particle& p)
        {
            if (mParts.Size() >= (uint32)kMaxParticles)
            {
                // Cap atteint : on evince la plus ancienne (index 0). Pas
                // O(1) mais arrive rarement (jeu typique < 200 simultanees).
                mParts.RemoveAt(0);
            }
            mParts.PushBack(p);
        }

        void ParticleSystem::EmitBurst(float x, float y, int count,
                                       math::NkColor color,
                                       float speedMin, float speedMax,
                                       float lifeMin , float lifeMax,
                                       float sizeMin , float sizeMax)
        {
            for (int i = 0; i < count; ++i)
            {
                Particle p;
                const float ang = Rand01() * 6.28318f;
                const float spd = Lerp(speedMin, speedMax, Rand01());
                p.x       = x;
                p.y       = y;
                p.vx      = math::NkCos(ang) * spd;
                p.vy      = math::NkSin(ang) * spd;
                p.life    = 0.0f;
                p.maxLife = Lerp(lifeMin, lifeMax, Rand01());
                p.size    = Lerp(sizeMin, sizeMax, Rand01());
                p.drag    = 0.30f;     // freine vite (puff)
                p.color   = color;
                Push(p);
            }
        }

        void ParticleSystem::EmitDirectional(float x, float y, int count,
                                             math::NkColor color,
                                             float dirAngle, float spread,
                                             float speedMin, float speedMax,
                                             float lifeMin , float lifeMax,
                                             float sizeMin , float sizeMax)
        {
            for (int i = 0; i < count; ++i)
            {
                Particle p;
                const float ang = dirAngle + Rand11() * spread;
                const float spd = Lerp(speedMin, speedMax, Rand01());
                p.x       = x;
                p.y       = y;
                p.vx      = math::NkCos(ang) * spd;
                p.vy      = math::NkSin(ang) * spd;
                p.life    = 0.0f;
                p.maxLife = Lerp(lifeMin, lifeMax, Rand01());
                p.size    = Lerp(sizeMin, sizeMax, Rand01());
                p.drag    = 0.40f;
                p.color   = color;
                Push(p);
            }
        }

        // Explosion de goal : on disperse 40 particules le long de la ligne
        // de but (x=wallX, y varie sur arenaH), avec vitesse principalement
        // horizontale dans le terrain.
        void ParticleSystem::EmitGoal(float wallX, float arenaH,
                                      math::NkColor color)
        {
            const int N = 40;
            for (int i = 0; i < N; ++i)
            {
                Particle p;
                p.x       = wallX;
                p.y       = Rand01() * arenaH;
                // Vitesse horizontale (vers l'interieur du terrain) + leger Y.
                const float spd  = 180.0f + Rand01() * 220.0f;
                const float sign = (wallX < 1.0f) ? 1.0f : -1.0f;
                // Heuristique simple : si on est au mur gauche (x petit) on
                // pousse vers la droite, sinon vers la gauche. Le caller passe
                // wallX = 0 ou arenaW, on detecte via sign... mais on peut
                // pas comparer a arenaW ici. On laisse le caller fournir
                // wallX et on pousse Rand11 * spd horizontalement (les 2 cotes
                // OK).
                (void)sign;
                p.vx      = Rand11() * spd;
                p.vy      = Rand11() * spd * 0.6f;
                p.life    = 0.0f;
                p.maxLife = 0.5f + Rand01() * 0.6f;
                p.size    = 2.5f + Rand01() * 2.5f;
                p.drag    = 0.30f;
                p.color   = color;
                Push(p);
            }
        }

    } // namespace pong
} // namespace nkentseu
