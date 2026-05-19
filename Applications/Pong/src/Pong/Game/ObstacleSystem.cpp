// =============================================================================
// ObstacleSystem.cpp
// =============================================================================

#include "ObstacleSystem.h"
#include "Pong/Render/GLRenderer2D.h"
#include "NKMath/NkFunctions.h"
#include <cstdlib>

namespace nkentseu
{
    namespace pong
    {

        // ── Helpers ──────────────────────────────────────────────────────────
        static float Rand01() { return (float)std::rand() / (float)RAND_MAX; }
        static float Rand11() { return Rand01() * 2.0f - 1.0f; }

        static math::NkColor MakeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        {
            math::NkColor c; c.r = r; c.g = g; c.b = b; c.a = a; return c;
        }
        static math::NkColor WithAlpha(math::NkColor c, float a01)
        {
            c.a = static_cast<uint8_t>(c.a * math::NkClamp(a01, 0.0f, 1.0f));
            return c;
        }

        // ── Geometrie : sommets d'un polygone obstacle ───────────────────────
        // Remplit @p out (max 6) avec les sommets MONDE de l'obstacle selon
        // sa forme. Retourne le nombre de sommets ecrits.
        struct Vec2 { float x, y; };
        static int GetPolyVertices(const Obstacle& o, Vec2* out)
        {
            const float cx = o.x + o.w * 0.5f;
            const float cy = o.y + o.h * 0.5f;
            const float rx = o.w * 0.5f;
            const float ry = o.h * 0.5f;
            const float cs = math::NkCos(o.rotation);
            const float sn = math::NkSin(o.rotation);
            auto put = [&](int i, float lx, float ly) {
                // Rotation locale puis translation au centre
                out[i].x = cx + lx * cs - ly * sn;
                out[i].y = cy + lx * sn + ly * cs;
            };

            switch (o.shape)
            {
            case ObstacleShape::Rectangle:
                // 4 coins : utiliser AABB sans rotation (la rotation des rects
                // serait coûteuse, on laisse non-rotatable).
                out[0] = { o.x,         o.y         };
                out[1] = { o.x + o.w,   o.y         };
                out[2] = { o.x + o.w,   o.y + o.h   };
                out[3] = { o.x,         o.y + o.h   };
                return 4;
            case ObstacleShape::Triangle:
                // Equilateral pointe-up : (0,-ry), (-rx, +ry), (+rx, +ry)
                put(0,  0.0f,  -ry);
                put(1, -rx,     ry);
                put(2,  rx,     ry);
                return 3;
            case ObstacleShape::Diamond:
                // Losange : haut/droite/bas/gauche
                put(0,  0.0f,  -ry);
                put(1,  rx,    0.0f);
                put(2,  0.0f,   ry);
                put(3, -rx,    0.0f);
                return 4;
            case ObstacleShape::Hexagon:
            {
                // Hexagone regulier inscrit dans rx (=ry car bbox carree)
                const float rad = (rx < ry) ? rx : ry;
                for (int k = 0; k < 6; ++k)
                {
                    const float a = -1.5708f + 6.28318f * k / 6.0f;
                    put(k, math::NkCos(a) * rad, math::NkSin(a) * rad);
                }
                return 6;
            }
            case ObstacleShape::Circle:
            default:
                return 0;  // pas de polygone (collision speciale)
            }
        }

        // Distance point-segment + closest point (cx, cy)
        static float DistPointSegment(float px, float py,
                                      float ax, float ay,
                                      float bx, float by,
                                      float& closestX, float& closestY)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float len2 = dx * dx + dy * dy;
            float t = 0.0f;
            if (len2 > 0.0001f)
            {
                t = ((px - ax) * dx + (py - ay) * dy) / len2;
                if (t < 0.0f) t = 0.0f;
                else if (t > 1.0f) t = 1.0f;
            }
            closestX = ax + t * dx;
            closestY = ay + t * dy;
            const float ddx = px - closestX;
            const float ddy = py - closestY;
            return math::NkSqrt(ddx * ddx + ddy * ddy);
        }

        // Test cercle (balle) contre polygone convexe : retourne true si hit
        // et remplit la normale (nx, ny) NORMALISEE pointant vers la balle.
        static bool TestCircleVsConvexPoly(float bx, float by, float br,
                                           const Vec2* verts, int n,
                                           float& nx, float& ny)
        {
            float bestDist = 1e9f;
            float bestNX = 0.0f, bestNY = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const Vec2& a = verts[i];
                const Vec2& bv = verts[(i + 1) % n];
                float cx, cy;
                const float d = DistPointSegment(bx, by, a.x, a.y, bv.x, bv.y,
                                                 cx, cy);
                if (d < bestDist)
                {
                    bestDist = d;
                    bestNX = bx - cx;
                    bestNY = by - cy;
                }
            }
            if (bestDist > br) return false;
            const float len = math::NkSqrt(bestNX * bestNX + bestNY * bestNY);
            if (len > 0.0001f)
            {
                nx = bestNX / len;
                ny = bestNY / len;
            }
            else
            {
                nx = 0.0f; ny = -1.0f;  // fallback : balle au centre exact
            }
            return true;
        }

        // Test cercle-cercle : balle vs obstacle Circle (rayon = w/2).
        static bool TestCircleVsCircle(float bx, float by, float br,
                                       float cx, float cy, float cr,
                                       float& nx, float& ny)
        {
            const float dx = bx - cx;
            const float dy = by - cy;
            const float d2 = dx * dx + dy * dy;
            const float rr = br + cr;
            if (d2 >= rr * rr) return false;
            const float d = math::NkSqrt(d2);
            if (d > 0.0001f) { nx = dx / d; ny = dy / d; }
            else             { nx = 0.0f;  ny = -1.0f;  }
            return true;
        }

        // Reflexion v' = v - 2(v.n)n
        static void ReflectVelocity(float vx, float vy, float nx, float ny,
                                    float& outVX, float& outVY)
        {
            const float dot = vx * nx + vy * ny;
            outVX = vx - 2.0f * dot * nx;
            outVY = vy - 2.0f * dot * ny;
        }

        // ── Spawn ────────────────────────────────────────────────────────────
        // Distribution sur l'ensemble du terrain (pas seulement le centre).
        // Chaque type a :
        //   - un nombre d'instances randomise dans [minCount, maxCount]
        //   - un facteur "power" randomise dans [0.6, 1.6] (intensite de l'effet)
        //   - des positions normalisees (fractions du viewport) avec jitter
        //
        // Resultat : chaque partie a un layout different et imprevisible.
        // ─────────────────────────────────────────────────────────────────────

        // Helper : positionne un obstacle a (fracX, fracY) du terrain + jitter
        // optionnel. Retourne l'obstacle pret a etre pushed.
        static void PlaceAtFraction(Obstacle& o, float arenaW, float arenaH,
                                    float fracX, float fracY,
                                    float jitterX, float jitterY)
        {
            const float jx = (Rand11() * jitterX) * arenaW;
            const float jy = (Rand11() * jitterY) * arenaH;
            o.x = arenaW * fracX + jx - o.w * 0.5f;
            o.y = arenaH * fracY + jy - o.h * 0.5f;
            // Clamp aux bornes du terrain
            const float margin = 4.0f;
            if (o.x < margin) o.x = margin;
            if (o.y < margin) o.y = margin;
            if (o.x + o.w > arenaW - margin) o.x = arenaW - margin - o.w;
            if (o.y + o.h > arenaH - margin) o.y = arenaH - margin - o.h;
        }

        // Test AABB-AABB avec marge de securite (evite que 2 obstacles se
        // touchent — la balle doit pouvoir passer entre).
        static bool AABBOverlaps(const Obstacle& a, const Obstacle& b, float margin)
        {
            return a.x + a.w + margin > b.x
                && a.x - margin < b.x + b.w
                && a.y + a.h + margin > b.y
                && a.y - margin < b.y + b.h;
        }
        static bool OverlapsAny(const NkVector<Obstacle>& list,
                                const Obstacle& o, float margin)
        {
            for (uint32 i = 0; i < list.Size(); ++i)
            {
                if (AABBOverlaps(o, list[i], margin)) return true;
            }
            return false;
        }

        // Wrapper : essaye de placer @p o sans overlap avec les obstacles
        // deja dans @p list. Retente jusqu'a @p maxTries en augmentant le
        // jitter a chaque echec. Si on n'y arrive pas, retourne false (le
        // caller doit NE PAS pusher cet obstacle).
        static bool TryPlaceNoOverlap(Obstacle& o, const NkVector<Obstacle>& list,
                                      float arenaW, float arenaH,
                                      float fracX, float fracY,
                                      float jitterX, float jitterY,
                                      int maxTries = 12,
                                      float margin = 6.0f)
        {
            for (int t = 0; t < maxTries; ++t)
            {
                // Augmente le jitter progressivement pour explorer plus loin.
                const float jx = jitterX + (t * 0.02f);
                const float jy = jitterY + (t * 0.02f);
                PlaceAtFraction(o, arenaW, arenaH, fracX, fracY, jx, jy);
                if (!OverlapsAny(list, o, margin)) return true;
            }
            return false;
        }

        static int RandIntRange(int lo, int hi)
        {
            if (hi <= lo) return lo;
            return lo + (int)(Rand01() * (hi - lo + 1));
        }

        // Power : facteur 0.6..1.6 (±40% autour de 1.0)
        static float RandPower() { return 0.6f + Rand01() * 1.0f; }

        // Power selon level utilisateur (1=Low, 2=Normal, 3=High).
        static float RandPowerLevel(int level)
        {
            switch (level)
            {
            case 1:  return 0.4f + Rand01() * 0.4f;   // 0.4..0.8 (-40%)
            case 3:  return 1.0f + Rand01() * 1.0f;   // 1.0..2.0 (+40..+100%)
            case 2:
            default: return 0.6f + Rand01() * 1.0f;   // 0.6..1.6 (default)
            }
        }

        // Helper : retourne soit le count force par l'utilisateur (s.obsCount[type]
        // si > 0) soit un random dans [randMin, randMax].
        static int ResolveCount(const GameSettings& s, ObstacleType t,
                                int randMin, int randMax)
        {
            const int forced = s.obsCount[(int)t];
            if (forced > 0) return forced;
            return RandIntRange(randMin, randMax);
        }

        // ── Configuration motion selon le type d'obstacle ───────────────────
        // Chaque type a un profil de mouvements probables. Certains restent
        // souvent statiques (Wall, Magnet) — d'autres bougent volontiers
        // (Mine, BonusStar). On randomise pour avoir une partie imprevisible.
        static void AssignRandomMotion(Obstacle& o, float arenaW, float arenaH,
                                       float scale)
        {
            // Position de base = centre courant
            o.baseX = o.x + o.w * 0.5f;
            o.baseY = o.y + o.h * 0.5f;
            o.motionPhase = Rand01() * 6.28318f;  // dephase aleatoire
            o.motion = ObstacleMotion::Static;
            o.visible = true;
            o.active  = true;

            // Probabilites par type (somme < 100 = reste a Static)
            int rollPercent = (int)(Rand01() * 100.0f);
            switch (o.type)
            {
            case ObstacleType::Wall:
                // 25% Translate, 20% Rotate, 5% Blink, sinon Static
                if (rollPercent < 25)      o.motion = ObstacleMotion::Translate;
                else if (rollPercent < 45) o.motion = ObstacleMotion::Rotate;
                else if (rollPercent < 50) o.motion = ObstacleMotion::Blink;
                break;
            case ObstacleType::Portal:
                // 40% Blink, 20% Translate, sinon Static
                if (rollPercent < 40)      o.motion = ObstacleMotion::Blink;
                else if (rollPercent < 60) o.motion = ObstacleMotion::Translate;
                break;
            case ObstacleType::Gravity:
                // 30% Translate (zone de gravite mouvante !), sinon Static
                if (rollPercent < 30)      o.motion = ObstacleMotion::Translate;
                break;
            case ObstacleType::Mine:
                // 25% Warp (mine teleportee !), 25% SpawnDespawn, 20% Blink, sinon Static.
                if (rollPercent < 25)      o.motion = ObstacleMotion::Warp;
                else if (rollPercent < 50) o.motion = ObstacleMotion::SpawnDespawn;
                else if (rollPercent < 70) o.motion = ObstacleMotion::Blink;
                break;
            case ObstacleType::AirCurrent:
                // 50% Translate horizontal (courant qui se deplace), sinon Static
                if (rollPercent < 50)      o.motion = ObstacleMotion::Translate;
                break;
            case ObstacleType::Magnet:
                // 15% Translate vertical, sinon Static
                if (rollPercent < 15)      o.motion = ObstacleMotion::Translate;
                break;
            case ObstacleType::GhostMirror:
                // 35% Warp (fantome qui se teleporte !), 30% SpawnDespawn,
                // 20% Blink, sinon Static.
                if (rollPercent < 35)      o.motion = ObstacleMotion::Warp;
                else if (rollPercent < 65) o.motion = ObstacleMotion::SpawnDespawn;
                else if (rollPercent < 85) o.motion = ObstacleMotion::Blink;
                break;
            case ObstacleType::BonusStar:
                // 40% Translate, 40% Rotate, sinon Static
                if (rollPercent < 40)      o.motion = ObstacleMotion::Translate;
                else if (rollPercent < 80) o.motion = ObstacleMotion::Rotate;
                break;
            }

            // Parametrage selon la motion choisie
            switch (o.motion)
            {
            case ObstacleMotion::Translate:
            {
                // Choix axe : 50/50 horizontal/vertical (random)
                const bool horiz = Rand01() > 0.5f;
                const float amp = (20.0f + Rand01() * 40.0f) * scale;
                o.motionAmpX = horiz ? amp : 0.0f;
                o.motionAmpY = horiz ? 0.0f : amp;
                o.motionSpeed = 0.4f + Rand01() * 0.8f;  // 0.4-1.2 Hz
                break;
            }
            case ObstacleMotion::Rotate:
                // Rotation continue, signe aleatoire
                o.rotationSpeed = (Rand01() > 0.5f ? 1.0f : -1.0f)
                                * (0.5f + Rand01() * 1.5f);  // 0.5..2.0 rad/s
                break;
            case ObstacleMotion::Blink:
                o.motionSpeed = 0.8f + Rand01() * 1.2f;     // 0.8-2.0 Hz
                break;
            case ObstacleMotion::SpawnDespawn:
                o.motionSpeed = 0.15f + Rand01() * 0.20f;   // 0.15-0.35 Hz
                                                            // => cycle 3-7s
                break;
            case ObstacleMotion::Warp:
                o.motionSpeed = 0.12f + Rand01() * 0.15f;   // 0.12-0.27 Hz
                                                            // => cycle 4-8s
                break;
            default: break;
            }
            (void)arenaW; (void)arenaH;
        }

        void ObstacleSystem::SpawnFromSettings(const GameSettings& s,
                                               float arenaW, float arenaH,
                                               float scale)
        {
            mObstacles.Clear();
            // Seed deterministe : si la session reseau a transmis une seed
            // commune (HOST -> CLIENT via PktStartMatch), on l'applique pour
            // que les 2 cotes spawnent EXACTEMENT les memes obstacles. En
            // mode local (seed == 0), on ne touche pas a la seed globale pour
            // garder le random natif du systeme (variete entre matchs).
            if (s.obstacleSeed != 0)
            {
                std::srand((unsigned int)s.obstacleSeed);
            }
            const float cx = arenaW * 0.5f;
            const float cy = arenaH * 0.5f;
            (void)cx; (void)cy;  // reserve pour les positions centrees historiques

            // Wall — 3 a 5 instances, formes variees, dispersees sur tout le
            // terrain via une grille 4x3 (cellules 0.25-wide). On choisit N
            // cellules au hasard.
            if (s.obsActive[(int)ObstacleType::Wall])
            {
                Obstacle base{};
                base.type = ObstacleType::Wall;
                base.color = MakeColor(60, 80, 110, 220);
                base.glowColor = MakeColor(100, 150, 200, 80);

                const ObstacleShape shapes[5] = {
                    ObstacleShape::Rectangle, ObstacleShape::Circle,
                    ObstacleShape::Triangle,  ObstacleShape::Diamond,
                    ObstacleShape::Hexagon
                };
                // Cellules disponibles (fractions du viewport, evitent les
                // bords et la zone centrale stricte pour laisser passer la balle).
                const float fracsX[6] = { 0.20f, 0.35f, 0.50f, 0.65f, 0.80f, 0.50f };
                const float fracsY[6] = { 0.30f, 0.65f, 0.25f, 0.70f, 0.45f, 0.45f };

                const int count = ResolveCount(s, ObstacleType::Wall, 3, 5);
                for (int i = 0; i < count; ++i)
                {
                    Obstacle o = base;
                    o.shape = shapes[(int)(Rand01() * 5.0f) % 5];
                    o.power = RandPowerLevel(s.obsPowerLevel[(int)ObstacleType::Wall]);
                    // Taille aleatoire : 26..50 px scaled
                    const float side = (26.0f + Rand01() * 24.0f) * scale;
                    o.w = side; o.h = side;
                    o.rotation = (o.shape != ObstacleShape::Rectangle
                               && o.shape != ObstacleShape::Circle)
                               ? Rand11() * 0.6f : 0.0f;
                    const int slot = i % 6;
                    // Tentative de placement sans overlap. Si echec apres
                    // 12 essais, on skip cet obstacle (mieux que de le placer
                    // a cheval sur un autre).
                    if (!TryPlaceNoOverlap(o, mObstacles, arenaW, arenaH,
                                           fracsX[slot], fracsY[slot],
                                           0.04f, 0.05f)) continue;
                    mObstacles.PushBack(o);
                }
            }

            // Gravity — 1 a 2 zones, dispersees verticalement
            if (s.obsActive[(int)ObstacleType::Gravity])
            {
                const int count = ResolveCount(s, ObstacleType::Gravity, 1, 2);
                for (int i = 0; i < count; ++i)
                {
                    Obstacle o{};
                    o.type = ObstacleType::Gravity;
                    o.color = MakeColor(204, 119, 255, 200);
                    o.glowColor = MakeColor(204, 119, 255, 100);
                    o.power = RandPowerLevel(s.obsPowerLevel[(int)ObstacleType::Gravity]);
                    o.w = (28.0f + Rand01() * 16.0f) * scale;
                    o.h = o.w;
                    if (!TryPlaceNoOverlap(o, mObstacles, arenaW, arenaH,
                                           0.30f + i * 0.40f,
                                           0.35f + Rand01() * 0.30f,
                                           0.05f, 0.08f)) continue;
                    mObstacles.PushBack(o);
                }
            }

            // Portal — 2 a 3 portails, dispersion large
            if (s.obsActive[(int)ObstacleType::Portal])
            {
                int count = ResolveCount(s, ObstacleType::Portal, 2, 3);
                if (count > 3) count = 3;
                const float fX[3] = { 0.18f, 0.50f, 0.82f };
                const float fY[3] = { 0.25f, 0.75f, 0.30f };
                for (int i = 0; i < count; ++i)
                {
                    Obstacle o{};
                    o.type = ObstacleType::Portal;
                    o.color = MakeColor(255, 215, 0, 220);
                    o.glowColor = MakeColor(255, 215, 0, 100);
                    o.power = RandPowerLevel(s.obsPowerLevel[(int)ObstacleType::Portal]);
                    o.w = (22.0f + Rand01() * 12.0f) * scale; o.h = o.w;
                    if (!TryPlaceNoOverlap(o, mObstacles, arenaW, arenaH,
                                           fX[i], fY[i], 0.04f, 0.06f)) continue;
                    mObstacles.PushBack(o);
                }
            }

            // Mine — 2 a 4 mines reparties imprevisiblement
            if (s.obsActive[(int)ObstacleType::Mine])
            {
                const int count = ResolveCount(s, ObstacleType::Mine, 2, 4);
                for (int i = 0; i < count; ++i)
                {
                    Obstacle o{};
                    o.type = ObstacleType::Mine;
                    o.color = MakeColor(255, 64, 64, 220);
                    o.glowColor = MakeColor(255, 64, 64, 110);
                    o.power = RandPowerLevel(s.obsPowerLevel[(int)ObstacleType::Mine]);
                    o.w = (16.0f + Rand01() * 12.0f) * scale; o.h = o.w;
                    if (!TryPlaceNoOverlap(o, mObstacles, arenaW, arenaH,
                                           0.15f + Rand01() * 0.70f,
                                           0.15f + Rand01() * 0.70f,
                                           0.0f, 0.0f)) continue;
                    mObstacles.PushBack(o);
                }
            }

            // AirCurrent — 2 a 4 bandes reparties haut/bas/cotes
            if (s.obsActive[(int)ObstacleType::AirCurrent])
            {
                int count = ResolveCount(s, ObstacleType::AirCurrent, 2, 4);
                if (count > 4) count = 4;
                const float fX[4] = { 0.30f, 0.70f, 0.30f, 0.70f };
                const float fY[4] = { 0.10f, 0.10f, 0.90f, 0.90f };
                for (int i = 0; i < count; ++i)
                {
                    Obstacle o{};
                    o.type = ObstacleType::AirCurrent;
                    o.color = MakeColor(0, 255, 100, 200);
                    o.glowColor = MakeColor(0, 255, 100, 100);
                    o.power = RandPowerLevel(s.obsPowerLevel[(int)ObstacleType::AirCurrent]);
                    o.w = (28.0f + Rand01() * 20.0f) * scale;
                    o.h = 14.0f * scale;
                    if (!TryPlaceNoOverlap(o, mObstacles, arenaW, arenaH,
                                           fX[i], fY[i], 0.04f, 0.02f)) continue;
                    mObstacles.PushBack(o);
                }
            }

            // Magnet — 1 a 2 sur les cotes
            if (s.obsActive[(int)ObstacleType::Magnet])
            {
                int count = ResolveCount(s, ObstacleType::Magnet, 1, 2);
                if (count > 2) count = 2;
                for (int i = 0; i < count; ++i)
                {
                    Obstacle o{};
                    o.type = ObstacleType::Magnet;
                    o.color = MakeColor(0, 245, 255, 200);
                    o.glowColor = MakeColor(0, 245, 255, 100);
                    o.power = RandPowerLevel(s.obsPowerLevel[(int)ObstacleType::Magnet]);
                    o.w = 18.0f * scale;
                    o.h = (50.0f + Rand01() * 30.0f) * scale;
                    if (!TryPlaceNoOverlap(o, mObstacles, arenaW, arenaH,
                                           i == 0 ? 0.12f : 0.88f,
                                           0.30f + Rand01() * 0.40f,
                                           0.0f, 0.05f)) continue;
                    mObstacles.PushBack(o);
                }
            }

            // GhostMirror — 1 a 2 losanges dispersés
            if (s.obsActive[(int)ObstacleType::GhostMirror])
            {
                const int count = ResolveCount(s, ObstacleType::GhostMirror, 1, 2);
                for (int i = 0; i < count; ++i)
                {
                    Obstacle o{};
                    o.type = ObstacleType::GhostMirror;
                    o.color = MakeColor(204, 119, 255, 180);
                    o.glowColor = MakeColor(204, 119, 255, 90);
                    o.power = RandPowerLevel(s.obsPowerLevel[(int)ObstacleType::GhostMirror]);
                    o.w = (28.0f + Rand01() * 12.0f) * scale; o.h = o.w;
                    if (!TryPlaceNoOverlap(o, mObstacles, arenaW, arenaH,
                                           i == 0 ? 0.30f : 0.70f,
                                           0.55f + Rand01() * 0.25f,
                                           0.05f, 0.05f)) continue;
                    mObstacles.PushBack(o);
                }
            }

            // BonusStar — 1 a 2 etoiles, positions surprises
            if (s.obsActive[(int)ObstacleType::BonusStar])
            {
                const int count = ResolveCount(s, ObstacleType::BonusStar, 1, 2);
                for (int i = 0; i < count; ++i)
                {
                    Obstacle o{};
                    o.type = ObstacleType::BonusStar;
                    o.color = MakeColor(255, 215, 0, 240);
                    o.glowColor = MakeColor(255, 215, 0, 120);
                    o.power = RandPowerLevel(s.obsPowerLevel[(int)ObstacleType::BonusStar]);
                    o.w = 22.0f * scale; o.h = 22.0f * scale;
                    o.collected = false;
                    if (!TryPlaceNoOverlap(o, mObstacles, arenaW, arenaH,
                                           0.20f + Rand01() * 0.60f,
                                           0.15f + Rand01() * 0.70f,
                                           0.0f, 0.0f)) continue;
                    mObstacles.PushBack(o);
                }
            }

            // ── Motions : attribuer un mouvement random a chaque obstacle ──
            // Si l'utilisateur a desactive le chaos pour ce type, l'obstacle
            // reste statique. Sinon on applique le profil random par type.
            for (uint32 i = 0; i < mObstacles.Size(); ++i)
            {
                Obstacle& o = mObstacles[i];
                if (!s.obsChaotic[(int)o.type])
                {
                    // Forcer statique (mais initialiser baseX/Y au cas ou)
                    o.motion = ObstacleMotion::Static;
                    o.baseX = o.x + o.w * 0.5f;
                    o.baseY = o.y + o.h * 0.5f;
                    o.visible = true;
                    o.active  = true;
                }
                else
                {
                    AssignRandomMotion(o, arenaW, arenaH, scale);
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void ObstacleSystem::Update(float dt, float arenaW, float arenaH)
        {
            for (uint32 i = 0; i < mObstacles.Size(); ++i)
            {
                Obstacle& o = mObstacles[i];
                o.pulse += dt * 4.0f;
                if (o.type == ObstacleType::Gravity)
                {
                    o.angle += dt * 1.2f;
                }

                // Application du mouvement
                switch (o.motion)
                {
                case ObstacleMotion::Static:
                    break;
                case ObstacleMotion::Translate:
                {
                    // Sinusoidal : position autour de la base
                    o.motionPhase += dt * o.motionSpeed * 6.28318f;
                    const float dx = math::NkSin(o.motionPhase) * o.motionAmpX;
                    const float dy = math::NkSin(o.motionPhase) * o.motionAmpY;
                    o.x = (o.baseX - o.w * 0.5f) + dx;
                    o.y = (o.baseY - o.h * 0.5f) + dy;
                    break;
                }
                case ObstacleMotion::Rotate:
                {
                    o.rotation += o.rotationSpeed * dt;
                    // Reste dans [-pi, pi] pour eviter overflow
                    if (o.rotation >  6.28318f) o.rotation -= 6.28318f;
                    if (o.rotation < -6.28318f) o.rotation += 6.28318f;
                    break;
                }
                case ObstacleMotion::Blink:
                {
                    // Cycle visible/invisible. On utilise sin pour avoir une
                    // transition franche : visible si sin > 0.
                    o.motionPhase += dt * o.motionSpeed * 6.28318f;
                    o.visible = math::NkSin(o.motionPhase) > 0.0f;
                    // Quand invisible, on garde active pour qu'on puisse
                    // toujours toucher l'obstacle (effet "fantome"). Si on
                    // veut "invincible quand invisible", on mettrait active.
                    // Pour l'instant on lie active a visible (= invisible
                    // = inactif), c'est plus juste visuellement.
                    o.active = o.visible;
                    break;
                }
                case ObstacleMotion::SpawnDespawn:
                {
                    // Cycle long : actif pendant la moitie du cycle, inactif
                    // pendant l'autre. La transition n'est pas une simple
                    // visibilite : l'obstacle disparait COMPLETEMENT (pas
                    // de collision, pas de rendu).
                    o.motionPhase += dt * o.motionSpeed * 6.28318f;
                    const float s = math::NkSin(o.motionPhase);
                    o.active  = (s > 0.0f);
                    o.visible = o.active;
                    break;
                }
                case ObstacleMotion::Warp:
                {
                    // Comme SpawnDespawn MAIS a la transition inactif -> actif,
                    // on teleporte l'obstacle a une nouvelle position random
                    // (>= 100px de l'ancienne, sans overlap).
                    o.motionPhase += dt * o.motionSpeed * 6.28318f;
                    const float s = math::NkSin(o.motionPhase);
                    const bool wasActive = o.active;
                    o.active  = (s > 0.0f);
                    o.visible = o.active;
                    if (!wasActive && o.active && arenaW > 0.0f && arenaH > 0.0f)
                    {
                        const float oldCx = o.baseX;
                        const float oldCy = o.baseY;
                        const float minDist2 = 100.0f * 100.0f;
                        for (int t = 0; t < 12; ++t)
                        {
                            const float fx = 0.10f + Rand01() * 0.80f;
                            const float fy = 0.15f + Rand01() * 0.70f;
                            const float nx = arenaW * fx - o.w * 0.5f;
                            const float ny = arenaH * fy - o.h * 0.5f;
                            const float ncx = nx + o.w * 0.5f;
                            const float ncy = ny + o.h * 0.5f;
                            const float ddx = ncx - oldCx;
                            const float ddy = ncy - oldCy;
                            if (ddx * ddx + ddy * ddy < minDist2) continue;
                            // Test overlap avec les autres obstacles
                            Obstacle tmp = o; tmp.x = nx; tmp.y = ny;
                            bool clash = false;
                            for (uint32 j = 0; j < mObstacles.Size(); ++j)
                            {
                                if (&mObstacles[j] == &o) continue;
                                if (AABBOverlaps(tmp, mObstacles[j], 6.0f))
                                {
                                    clash = true; break;
                                }
                            }
                            if (clash) continue;
                            o.x = nx; o.y = ny;
                            o.baseX = ncx; o.baseY = ncy;
                            break;
                        }
                    }
                    break;
                }
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // Rendu : chaque type a son look propre. Tous restent dans le style
        // neon / line-art coherent avec le reste du jeu.
        // ─────────────────────────────────────────────────────────────────────
        void ObstacleSystem::Render(GLRenderer2D& r, float ax, float ay,
                                    float scale) const
        {
            for (uint32 i = 0; i < mObstacles.Size(); ++i)
            {
                const Obstacle& o = mObstacles[i];
                // Skip rendu si invisible (Blink off, SpawnDespawn inactif).
                if (!o.visible) continue;
                const float ox = ax + o.x, oy = ay + o.y;
                const float ocx = ox + o.w * 0.5f, ocy = oy + o.h * 0.5f;
                const float pulse01 = 0.5f + 0.5f * math::NkSin(o.pulse);
                const float lineW = math::NkMax(1.0f, scale);

                switch (o.type)
                {
                case ObstacleType::Wall:
                {
                    // Variantes de forme : Rect / Circle / Triangle / Diamond / Hex
                    switch (o.shape)
                    {
                    case ObstacleShape::Rectangle:
                        r.DrawQuad       (ox, oy, o.w, o.h, o.color);
                        r.DrawQuadOutline(ox, oy, o.w, o.h, o.glowColor, lineW);
                        break;
                    case ObstacleShape::Circle:
                    {
                        const float rad = math::NkMin(o.w, o.h) * 0.5f;
                        r.DrawCircle       (ocx, ocy, rad, o.color, 48);
                        r.DrawCircleOutline(ocx, ocy, rad, o.glowColor, lineW, 48);
                        break;
                    }
                    case ObstacleShape::Triangle:
                    case ObstacleShape::Diamond:
                    case ObstacleShape::Hexagon:
                    {
                        // Polygone : on remplit par fan de triangles
                        Vec2 verts[6];
                        const int n = GetPolyVertices(o, verts);
                        if (n < 3) break;
                        // Cards: centre + sommets ; offset arena (ax, ay)
                        const float pcx = (o.x + o.w * 0.5f) + ax;
                        const float pcy = (o.y + o.h * 0.5f) + ay;
                        for (int k = 0; k < n; ++k)
                        {
                            const Vec2& a0 = verts[k];
                            const Vec2& a1 = verts[(k + 1) % n];
                            r.DrawTriangle(pcx,           pcy,
                                           a0.x + ax,     a0.y + ay,
                                           a1.x + ax,     a1.y + ay,
                                           o.color);
                        }
                        // Outline (lignes joignant les sommets)
                        for (int k = 0; k < n; ++k)
                        {
                            const Vec2& a0 = verts[k];
                            const Vec2& a1 = verts[(k + 1) % n];
                            r.DrawLine(a0.x + ax, a0.y + ay,
                                       a1.x + ax, a1.y + ay,
                                       o.glowColor, lineW);
                        }
                        break;
                    }
                    }
                    break;
                }
                case ObstacleType::Gravity:
                {
                    // 3 cercles concentriques animes + point central
                    for (int k = 0; k < 3; ++k)
                    {
                        const float rr = o.w * (0.20f + 0.18f * k);
                        math::NkColor c = WithAlpha(o.color,
                                          (0.20f + 0.15f * k) + pulse01 * 0.10f);
                        r.DrawCircleOutline(ocx, ocy, rr, c, 1.5f * lineW, 32);
                    }
                    r.DrawCircle(ocx, ocy, math::NkMax(2.0f, 3.0f * scale),
                                 WithAlpha(o.color, 0.7f), 12);
                    break;
                }
                case ObstacleType::Portal:
                {
                    const float a = 0.4f + pulse01 * 0.5f;
                    r.DrawCircle       (ocx, ocy, o.w * 0.5f, WithAlpha(o.color, a * 0.25f), 32);
                    r.DrawCircleOutline(ocx, ocy, o.w * 0.5f, WithAlpha(o.color, a), 2.0f * lineW, 32);
                    break;
                }
                case ObstacleType::Mine:
                {
                    // Cercle rouge + pics autour
                    const float pulse = 0.8f + pulse01 * 0.2f;
                    r.DrawCircle(ocx, ocy, o.w * 0.30f,
                                 WithAlpha(o.color, pulse * 0.6f), 16);
                    for (int k = 0; k < 8; ++k)
                    {
                        const float a = 6.28318f * k / 8.0f;
                        const float r1 = o.w * 0.35f;
                        const float r2 = o.w * 0.50f;
                        r.DrawLine(ocx + math::NkCos(a) * r1, ocy + math::NkSin(a) * r1,
                                   ocx + math::NkCos(a) * r2, ocy + math::NkSin(a) * r2,
                                   WithAlpha(o.color, pulse), 1.5f * lineW);
                    }
                    break;
                }
                case ObstacleType::AirCurrent:
                {
                    // 3 lignes ondulees horizontales (vent)
                    for (int k = 0; k < 3; ++k)
                    {
                        const float yy = oy + 3.0f + k * (o.h - 6.0f) / 2.0f;
                        r.DrawQuad(ox + 2.0f, yy, o.w - 4.0f, 1.5f * lineW, o.color);
                    }
                    r.DrawQuadOutline(ox, oy, o.w, o.h, o.glowColor, lineW);
                    break;
                }
                case ObstacleType::Magnet:
                {
                    // U vert : 2 bras + base. Effet d'attirance par halo.
                    r.DrawQuad(ox, oy, o.w * 0.30f, o.h, o.color);
                    r.DrawQuad(ox + o.w * 0.70f, oy, o.w * 0.30f, o.h, o.color);
                    r.DrawQuad(ox, oy + o.h * 0.75f, o.w, o.h * 0.25f, o.color);
                    r.DrawQuadOutline(ox, oy, o.w, o.h,
                                      WithAlpha(o.glowColor, 0.6f + pulse01 * 0.3f),
                                      lineW);
                    break;
                }
                case ObstacleType::GhostMirror:
                {
                    // Losange semi-transparent qui pulse
                    const float a = 0.30f + pulse01 * 0.35f;
                    math::NkColor c = WithAlpha(o.color, a);
                    r.DrawTriangle(ocx, oy, ox + o.w, ocy, ocx, oy + o.h, c);
                    r.DrawTriangle(ocx, oy, ox,       ocy, ocx, oy + o.h, c);
                    break;
                }
                case ObstacleType::BonusStar:
                {
                    if (o.collected) break;
                    const float rad = o.w * 0.50f;
                    const float scl = 0.85f + 0.15f * pulse01;
                    // Pentagram en lignes
                    for (int k = 0; k < 5; ++k)
                    {
                        const float a1 = -1.5708f + 6.28318f * k / 5.0f;
                        const float a2 = -1.5708f + 6.28318f * (k + 2) / 5.0f;
                        r.DrawLine(ocx + math::NkCos(a1) * rad * scl,
                                   ocy + math::NkSin(a1) * rad * scl,
                                   ocx + math::NkCos(a2) * rad * scl,
                                   ocy + math::NkSin(a2) * rad * scl,
                                   o.color, 1.5f * lineW);
                    }
                    break;
                }
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // Collision balle (AABB rapide). On retourne le 1er hit trouve dans
        // l'ordre des obstacles. Le caller applique les effets.
        // ─────────────────────────────────────────────────────────────────────
        ObstacleHit ObstacleSystem::CheckCollision(float bx, float by, float br,
                                                   float vx, float vy,
                                                   float dt60) const
        {
            (void)dt60;
            ObstacleHit hit{};
            for (uint32 i = 0; i < mObstacles.Size(); ++i)
            {
                const Obstacle& o = mObstacles[i];
                if (o.collected) continue;
                // Skip collision si l'obstacle est inactif (Blink invisible,
                // SpawnDespawn off) — la balle passe a travers.
                if (!o.active) continue;

                // BonusStar / Gravity / AirCurrent : zones d'effet larges,
                // collision = entree dans la zone (pas un rebond physique).
                const bool isZone = (o.type == ObstacleType::Gravity
                                  || o.type == ObstacleType::AirCurrent);
                // AABB-Circle approximative (ball traitee comme AABB
                // englobant r). Suffisant pour gameplay.
                const bool overlap = (bx + br > o.x && bx - br < o.x + o.w
                                   && by + br > o.y && by - br < o.y + o.h);
                if (!overlap) continue;

                switch (o.type)
                {
                case ObstacleType::Wall:
                {
                    // Dispatch selon la forme. Pour Rectangle (axis-aligned) on
                    // garde l'algo "cote le plus proche". Pour les autres, on
                    // calcule la normale de rebond et on l'applique via setVel.
                    if (o.shape == ObstacleShape::Rectangle)
                    {
                        const float fl = math::NkFabs(bx - o.x);
                        const float fr = math::NkFabs(bx - (o.x + o.w));
                        const float ft = math::NkFabs(by - o.y);
                        const float fb = math::NkFabs(by - (o.y + o.h));
                        const float minH = math::NkMin(fl, fr);
                        const float minV = math::NkMin(ft, fb);
                        if (minH < minV) hit.reflectX = true;
                        else             hit.reflectY = true;
                        hit.hit = true;
                        hit.particleColor = MakeColor(140, 170, 210, 255);
                        hit.particleCount = 8;
                        return hit;
                    }
                    else if (o.shape == ObstacleShape::Circle)
                    {
                        const float cxC = o.x + o.w * 0.5f;
                        const float cyC = o.y + o.h * 0.5f;
                        const float cr  = math::NkMin(o.w, o.h) * 0.5f;
                        float nx, ny;
                        if (TestCircleVsCircle(bx, by, br, cxC, cyC, cr, nx, ny))
                        {
                            float rvx, rvy;
                            ReflectVelocity(vx, vy, nx, ny, rvx, rvy);
                            hit.hit = true;
                            hit.setVel = true;
                            hit.setVX = rvx;
                            hit.setVY = rvy;
                            // Pousse la balle hors du cercle pour eviter
                            // qu'elle reste collee
                            hit.setPos = true;
                            hit.setX = cxC + nx * (cr + br + 0.5f);
                            hit.setY = cyC + ny * (cr + br + 0.5f);
                            hit.particleColor = MakeColor(140, 170, 210, 255);
                            hit.particleCount = 8;
                            return hit;
                        }
                        continue;  // pas de hit, skip ce wall
                    }
                    else  // Triangle / Diamond / Hexagon (polygones convexes)
                    {
                        Vec2 verts[6];
                        const int n = GetPolyVertices(o, verts);
                        float nx, ny;
                        if (TestCircleVsConvexPoly(bx, by, br, verts, n, nx, ny))
                        {
                            float rvx, rvy;
                            ReflectVelocity(vx, vy, nx, ny, rvx, rvy);
                            hit.hit = true;
                            hit.setVel = true;
                            hit.setVX = rvx;
                            hit.setVY = rvy;
                            hit.particleColor = MakeColor(140, 170, 210, 255);
                            hit.particleCount = 8;
                            return hit;
                        }
                        continue;
                    }
                }
                case ObstacleType::Portal:
                {
                    // Teleporte sur une portee proportionnelle au power.
                    // Boost vitesse aussi scale par power.
                    hit.hit = true;
                    hit.setPos = true;
                    const float teleRange = 80.0f * o.power;
                    hit.setX = (o.x + o.w * 0.5f) + Rand11() * teleRange;
                    hit.setY = (o.y + o.h * 0.5f) + Rand11() * teleRange;
                    hit.setVel = true;
                    const float spd = 4.5f * o.power;
                    hit.setVX = (Rand01() > 0.5f ? 1.0f : -1.0f) * spd;
                    hit.setVY = Rand11() * spd;
                    hit.particleColor = o.color;
                    hit.particleCount = 16;
                    return hit;
                }
                case ObstacleType::Gravity:
                {
                    // Force radiale ADOUCIE (0.35 * power au lieu de 0.8) et
                    // perpendiculaire si la balle est proche du centre (evite
                    // le vortex captif). Si la balle est proche du centre du
                    // gravity, on ajoute une composante tangentielle pour
                    // l'aider a s'echapper.
                    if (isZone) {} // marker
                    const float gcx = o.x + o.w * 0.5f;
                    const float gcy = o.y + o.h * 0.5f;
                    const float dx = gcx - bx;
                    const float dy = gcy - by;
                    const float dist = math::NkSqrt(dx * dx + dy * dy);
                    if (dist > 0.001f)
                    {
                        const float strength = 0.35f * o.power;
                        const float nxR = dx / dist;
                        const float nyR = dy / dist;
                        // Si tres proche du centre, ajouter une force
                        // tangentielle pour ejecter la balle
                        const float radius = o.w * 0.5f;
                        const float tangentBoost =
                            (dist < radius * 0.4f) ? 0.5f : 0.0f;
                        const float nxT = -nyR;
                        const float nyT =  nxR;
                        hit.hit = true;
                        hit.setVel = true;
                        hit.setVX = vx + nxR * strength + nxT * tangentBoost;
                        hit.setVY = vy + nyR * strength + nyT * tangentBoost;
                    }
                    return hit;
                }
                case ObstacleType::Magnet:
                {
                    // Propulse fortement (x[1.5..2.1] selon power)
                    hit.hit = true;
                    const float spd = math::NkSqrt(vx * vx + vy * vy);
                    const float mul = 1.5f + 0.6f * o.power;
                    const float boosted = math::NkMin(spd * mul, 12.0f);
                    const float ang = math::NkAtan2(vy, vx);
                    hit.setVel = true;
                    hit.setVX = math::NkCos(ang) * boosted;
                    hit.setVY = math::NkSin(ang) * boosted;
                    hit.particleColor = o.color;
                    hit.particleCount = 12;
                    return hit;
                }
                case ObstacleType::Mine:
                {
                    // Explosion : vitesse aleatoire totale, amplitude * power
                    hit.hit = true;
                    hit.setVel = true;
                    const float amp = 8.0f * o.power;
                    hit.setVX = Rand11() * amp;
                    hit.setVY = Rand11() * amp;
                    hit.particleColor = o.color;
                    hit.particleCount = 24;
                    // CHAIN REACTION : toutes les Mines voisines detonent.
                    // On les marque collected=true (=> skip dans rendu/collision)
                    // et on enregistre leur centre pour que GameplayScene emette
                    // des particules a chaque chain. Pas d'impulse cumulee sur
                    // la balle (sinon la physique devient incontrolable).
                    const float chainR  = 200.0f;
                    const float chainR2 = chainR * chainR;
                    const float mcx     = o.x + o.w * 0.5f;
                    const float mcy     = o.y + o.h * 0.5f;
                    for (uint32 k = 0; k < mObstacles.Size(); ++k)
                    {
                        const Obstacle& m = mObstacles[k];
                        if (&m == &o) continue;
                        if (m.type != ObstacleType::Mine) continue;
                        if (m.collected) continue;
                        const float cx = m.x + m.w * 0.5f;
                        const float cy = m.y + m.h * 0.5f;
                        const float dx = cx - mcx, dy = cy - mcy;
                        if (dx * dx + dy * dy > chainR2) continue;
                        const_cast<Obstacle&>(m).collected = true;
                        if (hit.chainCount < ObstacleHit::kMaxChain)
                        {
                            hit.chainX[hit.chainCount] = cx;
                            hit.chainY[hit.chainCount] = cy;
                            ++hit.chainCount;
                        }
                    }
                    return hit;
                }
                case ObstacleType::AirCurrent:
                {
                    // Push vertical * power
                    hit.hit = true;
                    const float pushDir = (by < o.y + o.h * 0.5f) ? 1.0f : -1.0f;
                    hit.setVel = true;
                    hit.setVX = vx;
                    hit.setVY = vy + pushDir * 0.5f * o.power;
                    return hit;
                }
                case ObstacleType::GhostMirror:
                {
                    // Traverse simplement (alpha visuel). Pas d'effet physique
                    // sur la balle pour l'instant. Le clone temporaire viendra
                    // avec le systeme de double-balle (power-ups).
                    hit.hit = false;  // pas de modif balle
                    return hit;
                }
                case ObstacleType::BonusStar:
                {
                    // Collecte : on signale via bonusStarCollected. Le
                    // GameplayScene appliquera un bonus aleatoire au dernier
                    // joueur ayant touche la balle (cf. PowerUpSystem).
                    const_cast<Obstacle&>(o).collected = true;
                    hit.hit                 = false;
                    hit.bonusStarCollected  = true;
                    hit.particleColor       = o.color;
                    hit.particleCount       = 20;
                    return hit;
                }
                }
            }
            return hit;
        }

        // ─────────────────────────────────────────────────────────────────────
        void ObstacleSystem::Rescale(float arenaW, float arenaH, float scale)
        {
            // Simple : on respawn avec le meme set de types actifs. C'est
            // acceptable car le resize n'arrive qu'au demarrage / au resize
            // window desktop.
            GameSettings tmp;
            for (uint32 i = 0; i < mObstacles.Size(); ++i)
            {
                tmp.obsActive[(int)mObstacles[i].type] = true;
            }
            SpawnFromSettings(tmp, arenaW, arenaH, scale);
        }

    } // namespace pong
} // namespace nkentseu
