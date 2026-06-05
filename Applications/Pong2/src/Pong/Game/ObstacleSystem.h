#pragma once
// =============================================================================
// ObstacleSystem.h
// -----------------------------------------------------------------------------
// Systeme d'obstacles in-game pour Pong. Implementation des 8 types definis
// dans le GDD §2.2 (cf. ObstacleType) :
//   1. Wall         — rebond 90 deg, mur solide
//   2. Portal       — teleporte la balle ailleurs + boost x1.5
//   3. Gravity      — attire la balle vers son centre
//   4. Magnet       — propulse la balle x1.8 au contact
//   5. Mine         — explosion + vitesse aleatoire
//   6. GhostMirror  — traverse + spawn d'une 2e balle (simplifie : alpha)
//   7. AirCurrent   — pousse la balle dans une direction fixe
//   8. BonusStar    — collectable (pour futur power-up)
//
// Layout des obstacles sur le terrain inspire de
// docs/02_terrain_de_jeu_1v1.html. Toutes les dimensions sont scalees au
// moment du Spawn (les positions sont relatives au centre de l'arene).
// =============================================================================

#include "Pong/Game/GameTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkColor.h"
#include "NKCore/NkTypes.h"

namespace nkentseu
{
    namespace pong
    {

        class GLRenderer2D;

        // ── Forme geometrique d'un obstacle ──────────────────────────────────
        // L'AABB englobant reste (x,y,w,h) — la forme decrite par "shape"
        // determine la collision et le rendu :
        //   - Rectangle : pleine bbox (collision AABB classique)
        //   - Circle    : cercle inscrit dans la bbox, rayon = w/2 = h/2
        //   - Triangle  : equilateral pointe-up dans la bbox, rotatable
        //   - Diamond   : losange (4 sommets aux milieux de la bbox)
        // ─────────────────────────────────────────────────────────────────────
        enum class ObstacleShape : uint8
        {
            Rectangle,
            Circle,
            Triangle,
            Diamond,
            Hexagon
        };

        // ── Mouvement applique a l'obstacle ──────────────────────────────────
        //   Static       : ne bouge pas (defaut)
        //   Translate    : oscille autour de sa position de base (sin)
        //   Rotate       : tourne sur lui-meme (incremente .rotation)
        //   Blink        : alterne visible/invisible (cycle ~0.6-1.2s)
        //   SpawnDespawn : alterne actif/inactif sur un cycle long (4-8s).
        //                   Quand inactif : pas de collision NI de rendu.
        // ─────────────────────────────────────────────────────────────────────
        enum class ObstacleMotion : uint8
        {
            Static,
            Translate,
            Rotate,
            Blink,
            SpawnDespawn,
            /// Warp : disparait pendant la moitie du cycle (comme SpawnDespawn)
            /// puis REAPPARAIT ailleurs (position randomisee a chaque cycle).
            /// Garantit un saut min de ~100px pour que ce soit visible.
            Warp
        };

        struct Obstacle
        {
            ObstacleType  type;
            ObstacleShape shape;           ///< forme geometrique (cf. enum)
            float         x, y, w, h;      ///< AABB englobant, coin haut-gauche
            float         rotation;        ///< rotation en radians (pour Triangle/Diamond)
            math::NkColor color;           ///< couleur de base
            math::NkColor glowColor;       ///< halo
            float         pulse;           ///< accumulateur d'anim (0..2pi)
            float         angle;           ///< rotation animee (gravity)
            bool          collected;       ///< pour BonusStar

            // ── Parametres specifiques randomises au spawn ─────────────────
            // Multiplie l'intensite de l'effet du type. Par exemple :
            //   Gravity : force radiale = base_force * power
            //   Magnet  : boost speed   = base_mul   * power
            //   Mine    : magnitude explosion = base * power
            //   Portal  : portee teleport     = base * power
            // Plage typique : 0.6 .. 1.6 (gives ±40% variation).
            float         power = 1.0f;

            // ── Mouvement (anime sur Update) ───────────────────────────────
            ObstacleMotion motion       = ObstacleMotion::Static;
            float          baseX        = 0.0f;   ///< position centre x au spawn
            float          baseY        = 0.0f;   ///< idem y
            float          motionPhase  = 0.0f;   ///< accumulateur radian
            float          motionAmpX   = 0.0f;   ///< amplitude oscillation (px)
            float          motionAmpY   = 0.0f;
            float          motionSpeed  = 1.0f;   ///< Hz pour Translate/Blink
            float          rotationSpeed= 0.0f;   ///< rad/sec si Rotate

            // ── Etat visible / actif ───────────────────────────────────────
            // visible = false       : pas de rendu (mais collision peut etre active)
            // active  = false       : pas de collision (et pas de rendu)
            // Utilises par Blink (visible alterne) et SpawnDespawn (active alterne).
            bool           visible = true;
            bool           active  = true;
        };

        // Resultat d'une collision balle-obstacle, applique par la scene.
        struct ObstacleHit
        {
            bool   hit          = false;
            // Modifications a appliquer a la balle.
            bool   reflectX     = false;
            bool   reflectY     = false;
            float  setX         = 0.0f;
            float  setY         = 0.0f;
            bool   setPos       = false;
            float  setVX        = 0.0f;
            float  setVY        = 0.0f;
            bool   setVel       = false;
            float  multSpeed    = 1.0f;     ///< multiplie la vitesse (1=neutre)
            // Effet visuel a declencher (signal pour la scene)
            math::NkColor particleColor;
            int   particleCount = 0;        ///< 0 = pas de particules
            // Signal : une BonusStar a ete collectee a ce frame. Le caller
            // doit appliquer un bonus random au dernier joueur ayant touche
            // la balle (cf. PowerUpSystem::ApplyRandomBonus).
            bool   bonusStarCollected = false;
            // Chain Mine : lorsqu'une Mine est touchee, les Mines voisines
            // detonent aussi. Le caller emet des particules au centre de
            // chacune via (chainX, chainY). Pas d'impulse cumulee sur la balle.
            static constexpr int kMaxChain = 8;
            int    chainCount   = 0;
            float  chainX[kMaxChain] = {0};
            float  chainY[kMaxChain] = {0};
        };

        class ObstacleSystem
        {
        public:
            /// Vide la liste et spawn une instance de chaque type ACTIF
            /// (settings.obsActive[i]). Les positions sont scalees par
            /// @p scale et centrees sur l'arene (arenaW, arenaH).
            void SpawnFromSettings(const GameSettings& s,
                                   float arenaW, float arenaH, float scale);

            /// Avance les anims (pulse, rotation, blink, warp).
            /// @p arenaW/arenaH sont necessaires pour le warp (repositionnement
            /// aleatoire dans le terrain). Si <= 0, le warp est saute.
            void Update(float dt, float arenaW = 0.0f, float arenaH = 0.0f);

            /// Trace tous les obstacles dans l'arene avec offset (ax, ay).
            void Render(GLRenderer2D& r, float ax, float ay, float scale) const;

            /// Teste les collisions balle-obstacle et applique les effets.
            /// La balle est decrite par (bx, by, br) + (vx, vy). On retourne
            /// le 1er hit trouve (les obstacles ne s'enchainent pas dans un
            /// meme frame). Le caller applique les modifs sur sa balle.
            ObstacleHit CheckCollision(float bx, float by, float br,
                                       float vx, float vy,
                                       float dt60) const;

            /// Re-genere les positions sans changer la liste active. Utile
            /// apres un resize (le scale a change).
            void Rescale(float arenaW, float arenaH, float scale);

            /// Helper pour le debug : nombre d'obstacles spawnes.
            int Count() const noexcept { return (int)mObstacles.Size(); }

            /// Acces read-only par index (utilise pour serialiser les obstacles
            /// en reseau cote HOST). 0 <= i < Count().
            const Obstacle& Get(int i) const { return mObstacles[(size_t)i]; }

            /// Acces mutable par index (utilise par le CLIENT pour appliquer
            /// les positions/rotations recues du HOST). 0 <= i < Count().
            Obstacle& At(int i) { return mObstacles[(size_t)i]; }

            /// Vide la liste (utilise par CLIENT avant de recreer depuis un
            /// snapshot reseau autoritatif).
            void Clear() { mObstacles.Clear(); }

            /// Ajoute un obstacle minimal (type/shape + position + dimensions
            /// + rotation + active/visible) avec les couleurs derivees du
            /// type. Utilise par le CLIENT pour appliquer un snapshot HOST :
            /// on n'a besoin que des champs visibles (pas motion params, pas
            /// power, etc. — ils sont autoritatifs cote HOST et les
            /// collisions cote CLIENT sont ignorees).
            void AppendFromNet(ObstacleType type, ObstacleShape shape,
                               float xTopLeft, float yTopLeft,
                               float w, float h, float rotation,
                               bool active, bool visible);

        private:
            NkVector<Obstacle> mObstacles;
        };

    } // namespace pong
} // namespace nkentseu
