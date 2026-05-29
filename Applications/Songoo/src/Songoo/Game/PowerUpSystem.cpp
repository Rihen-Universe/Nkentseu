// =============================================================================
// PowerUpSystem.cpp
// =============================================================================

#include "PowerUpSystem.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "NKMath/NkFunctions.h"
#include <cstdlib>
#include <cmath>

namespace nkentseu
{
    namespace songoo
    {

        // ── Helpers locaux ───────────────────────────────────────────────────
        static float Rand01() { return (float)std::rand() / (float)RAND_MAX; }

        static math::NkColor MakeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        {
            math::NkColor c; c.r = r; c.g = g; c.b = b; c.a = a; return c;
        }
        static math::NkColor WithAlpha(math::NkColor c, float a01)
        {
            c.a = static_cast<uint8_t>(c.a * math::NkClamp(a01, 0.0f, 1.0f));
            return c;
        }

        // ── Tables de description : duree, couleur HUD par type ─────────────
        // Les durees sont en secondes, conformes au GDD §2.3.
        struct BonusDescL { float duration; int charges; math::NkColor color; };
        struct MalusDescL { float duration; int charges; math::NkColor color; };

        static BonusDescL BonusDesc(BonusType b)
        {
            switch (b)
            {
            case BonusType::GiantPaddle: return {  8.0f, 0, MakeColor(255, 215,   0) };
            case BonusType::PaddleSpeed: return {  6.0f, 0, MakeColor(  0, 245, 255) };
            case BonusType::DoublePoint: return { 60.0f, 1, MakeColor(255, 180,  60) };
            case BonusType::Shield:      return { 12.0f, 1, MakeColor(  0, 200, 255) };
            case BonusType::SlowBall:    return {  5.0f, 0, MakeColor( 80, 255, 100) };
            case BonusType::RandomStar:  return {  0.0f, 0, MakeColor(255, 255, 255) };
            }
            return { 6.0f, 0, MakeColor(255, 255, 255) };
        }

        static MalusDescL MalusDesc(MalusType m)
        {
            switch (m)
            {
            case MalusType::Blind:          return { 2.0f , 0, MakeColor( 80,   0, 120) };
            case MalusType::MiniPaddle:     return { 6.0f , 0, MakeColor(160, 160, 160) };
            case MalusType::InvertControls: return { 4.0f , 0, MakeColor(255,  60, 200) };
            case MalusType::Freeze:         return { 1.5f , 0, MakeColor(120, 200, 255) };
            case MalusType::FastBall:       return { 10.0f, 0, MakeColor(255,  60,  60) };
            case MalusType::TeleportPaddle: return { 0.0f , 0, MakeColor(255, 255, 255) };
            }
            return { 4.0f, 0, MakeColor(255, 255, 255) };
        }

        // Noms courts affichables (HUD + notification).
        static const char* BonusName(BonusType b)
        {
            switch (b)
            {
            case BonusType::GiantPaddle: return "GEANT";
            case BonusType::PaddleSpeed: return "VITESSE";
            case BonusType::DoublePoint: return "DOUBLE PT";
            case BonusType::Shield:      return "BOUCLIER";
            case BonusType::SlowBall:    return "BALLE LENTE";
            case BonusType::RandomStar:  return "SURPRISE";
            }
            return "BONUS";
        }
        static const char* MalusName(MalusType m)
        {
            switch (m)
            {
            case MalusType::Blind:          return "AVEUGLE";
            case MalusType::MiniPaddle:     return "MINI";
            case MalusType::InvertControls: return "INVERSE";
            case MalusType::Freeze:         return "GEL";
            case MalusType::FastBall:       return "BALLE RAPIDE";
            case MalusType::TeleportPaddle: return "TELEPORT";
            }
            return "MALUS";
        }

        // ── Lifecycle ────────────────────────────────────────────────────────
        void PowerUpSystem::Reset()
        {
            mEffects.Clear();
            mDrops.Clear();
            mDropTimer = RandPeriod();
            mTeleportPending[0] = mTeleportPending[1] = false;
            mNotifs[0] = Notification{};
            mNotifs[1] = Notification{};
        }

        float PowerUpSystem::RandPeriod()
        {
            return mDropPeriodMin
                 + Rand01() * (mDropPeriodMax - mDropPeriodMin);
        }

        void PowerUpSystem::ScheduleNextDrop()
        {
            mDropTimer = RandPeriod();
        }

        // ── Update ───────────────────────────────────────────────────────────
        // Decremente les durees, supprime les effets expires, fait tomber les
        // drops et detruit ceux qui sortent par le bas.
        void PowerUpSystem::Update(float dt, float arenaW, float arenaH, float scale)
        {
            (void)arenaW; (void)scale;

            // Effets : decrement timeLeft, supprime ceux expires (sauf charges > 0)
            for (uint32 i = 0; i < mEffects.Size(); )
            {
                ActiveEffect& e = mEffects[i];
                e.timeLeft -= dt;
                const bool expired = (e.timeLeft <= 0.0f) && (e.charges <= 0);
                if (expired)
                {
                    mEffects.RemoveAt(i);
                    continue;
                }
                ++i;
            }

            // Drops : descente + derive horizontale vers paddle + suppression
            // au sortie de l'arene (bas OU cotes pour les drops trop deportes).
            for (uint32 i = 0; i < mDrops.Size(); )
            {
                PowerUpDrop& d = mDrops[i];
                if (!d.alive) { mDrops.RemoveAt(i); continue; }
                d.x     += d.vx * dt;
                d.y     += d.vy * dt;
                d.pulse += dt * 5.0f;
                // Suppression si sort par le bas ou (rare) par les cotes
                // au-dela des paddles. Marge confortable de 32 px.
                if (d.y - d.r > arenaH + 8.0f
                 || d.x + d.r < -32.0f
                 || d.x - d.r > arenaW + 32.0f)
                {
                    mDrops.RemoveAt(i); continue;
                }
                ++i;
            }

            // Timer de spawn periodique.
            mDropTimer -= dt;
            if (mDropTimer <= 0.0f)
            {
                SpawnRandomDrop(arenaW);
                ScheduleNextDrop();
            }

            // Notifications : decompte temps restant.
            for (int k = 0; k < 2; ++k)
            {
                Notification& n = mNotifs[k];
                if (!n.active) continue;
                n.timeLeft -= dt;
                if (n.timeLeft <= 0.0f) n.active = false;
            }
        }

        // ── Rendu ────────────────────────────────────────────────────────────
        // Drops : orbe glow + cercle plein. Couleur depend du type.
        void PowerUpSystem::Render(GLRenderer2D& r, FontAtlas& /*f*/,
                                   float arenaOX, float arenaOY, float scale) const
        {
            (void)scale;
            for (uint32 i = 0; i < mDrops.Size(); ++i)
            {
                const PowerUpDrop& d = mDrops[i];
                if (!d.alive) continue;
                const math::NkColor base = d.isBonus
                    ? BonusDesc((BonusType)d.kind).color
                    : MalusDesc((MalusType)d.kind).color;
                const float pulse01 = 0.5f + 0.5f * math::NkSin(d.pulse);

                const float cx = arenaOX + d.x;
                const float cy = arenaOY + d.y;
                // Halo exterieur
                r.DrawCircle(cx, cy, d.r * 1.6f, WithAlpha(base, 0.10f + pulse01 * 0.10f), 24);
                // Orbe pleine
                r.DrawCircle(cx, cy, d.r,         WithAlpha(base, 0.85f), 24);
                // Outline
                r.DrawCircleOutline(cx, cy, d.r,  WithAlpha(base, 1.0f), 1.5f, 24);
                // Marqueur central pour distinguer bonus/malus :
                //   bonus = petit cercle blanc, malus = croix rouge clair
                if (d.isBonus)
                {
                    r.DrawCircle(cx, cy, d.r * 0.30f, MakeColor(255, 255, 255, 220), 12);
                }
                else
                {
                    const float k = d.r * 0.45f;
                    r.DrawLine(cx - k, cy - k, cx + k, cy + k,
                               MakeColor(255, 255, 255, 220), 2.0f);
                    r.DrawLine(cx - k, cy + k, cx + k, cy - k,
                               MakeColor(255, 255, 255, 220), 2.0f);
                }
            }
        }

        // ── Application d'un bonus ───────────────────────────────────────────
        // RandomStar : on tire un autre type de bonus uniformement et on
        // s'appelle recursivement. Pour les autres, on rafraichit la duree
        // si l'effet est deja actif (pas de cumul).
        void PowerUpSystem::ApplyBonus(BonusType b, int collectorSide)
        {
            if (b == BonusType::RandomStar)
            {
                // Tirage uniforme parmi les 5 autres bonus (exclut RandomStar).
                static const BonusType kReal[5] = {
                    BonusType::GiantPaddle, BonusType::PaddleSpeed,
                    BonusType::DoublePoint, BonusType::Shield,
                    BonusType::SlowBall
                };
                ApplyBonus(kReal[(int)(Rand01() * 5.0f) % 5], collectorSide);
                return;
            }
            const BonusDescL d = BonusDesc(b);
            // SlowBall et FastBall : mutuellement exclusifs (la vitesse balle
            // suit un seul effet a la fois). On purge la familly "ball speed".
            if (b == BonusType::SlowBall)
            {
                RemoveEffectsByKind(false, (uint8)MalusType::FastBall, -1);
                RemoveEffectsByKind(false, (uint8)MalusType::FastBall, +1);
            }
            RemoveEffectsByKind(true, (uint8)b, collectorSide);  // refresh
            AddEffect(true, (uint8)b, collectorSide, d.duration, d.charges);
            // Notification toast pour le joueur affecte.
            Notification& n = mNotifs[SideIdx(collectorSide)];
            n.active   = true;
            n.isBonus  = true;
            n.kind     = (uint8)b;
            n.side     = collectorSide;
            n.duration = kNotifDuration;
            n.timeLeft = kNotifDuration;
        }

        void PowerUpSystem::ApplyMalus(MalusType m, int targetSide)
        {
            auto pushNotif = [&]()
            {
                Notification& n = mNotifs[SideIdx(targetSide)];
                n.active   = true;
                n.isBonus  = false;
                n.kind     = (uint8)m;
                n.side     = targetSide;
                n.duration = kNotifDuration;
                n.timeLeft = kNotifDuration;
            };
            if (m == MalusType::TeleportPaddle)
            {
                // Instantane : on memorise une position cible, le GameplayScene
                // appellera TryTeleport pour effectuer le snap.
                mTeleportPending[SideIdx(targetSide)] = true;
                // FracY entre 0.10 et 0.90 (evite que la raquette colle au bord).
                mTeleportFracY  [SideIdx(targetSide)] = 0.10f + Rand01() * 0.80f;
                pushNotif();
                return;
            }
            const MalusDescL d = MalusDesc(m);
            if (m == MalusType::FastBall)
            {
                // Purge SlowBall (les deux affectent la vitesse balle).
                RemoveEffectsByKind(true, (uint8)BonusType::SlowBall, -1);
                RemoveEffectsByKind(true, (uint8)BonusType::SlowBall, +1);
            }
            RemoveEffectsByKind(false, (uint8)m, targetSide);
            AddEffect(false, (uint8)m, targetSide, d.duration, d.charges);
            pushNotif();
        }

        void PowerUpSystem::ApplyRandomBonus(int collectorSide)
        {
            // RandomStar = tirage uniforme parmi tous les BonusType (y compris
            // RandomStar lui-meme — qui resout en cascade, donc equivalent).
            const BonusType all[6] = {
                BonusType::GiantPaddle, BonusType::PaddleSpeed,
                BonusType::DoublePoint, BonusType::Shield,
                BonusType::SlowBall,    BonusType::RandomStar
            };
            ApplyBonus(all[(int)(Rand01() * 6.0f) % 6], collectorSide);
        }
        void PowerUpSystem::ApplyRandomMalus(int targetSide)
        {
            const MalusType all[6] = {
                MalusType::Blind,    MalusType::MiniPaddle,
                MalusType::InvertControls, MalusType::Freeze,
                MalusType::FastBall, MalusType::TeleportPaddle
            };
            ApplyMalus(all[(int)(Rand01() * 6.0f) % 6], targetSide);
        }

        // ── Drops periodiques ────────────────────────────────────────────────
        // 60% bonus, 40% malus. Position X aleatoire (10..90% arenaW).
        // Vitesse de chute = 80 px/sec, rayon = 16 px (taille fixe a 1.0 scale).
        // ────────────────────────────────────────────────────────────────────
        // CheckBallCollision — transfere le momentum balle -> drop a l'impact.
        // Cercle-cercle simple. La balle est passive (sa vitesse ne change pas) :
        // c'est le drop qui prend l'impulsion et part en diagonale.
        // ────────────────────────────────────────────────────────────────────
        void PowerUpSystem::CheckBallCollision(float bx, float by, float br,
                                               float ballVX, float ballVY)
        {
            for (uint32 i = 0; i < mDrops.Size(); ++i)
            {
                PowerUpDrop& d = mDrops[i];
                if (!d.alive) continue;
                const float dx = d.x - bx;
                const float dy = d.y - by;
                const float distSq = dx*dx + dy*dy;
                const float radSum = d.r + br;
                if (distSq >= radSum * radSum) continue;

                // Conversion vitesse balle (px/frame@60fps) -> px/sec :
                // *60. Facteur 0.6 = transfert partiel du momentum, le reste
                // est "absorbe" cosmetiquement par le drop.
                constexpr float kFrameToSec = 60.0f;
                constexpr float kTransfer   = 0.6f;
                d.vx = ballVX * kFrameToSec * kTransfer;
                d.vy = ballVY * kFrameToSec * kTransfer;

                // Sort le drop legerement de la balle pour ne pas re-toucher
                // le frame suivant (sinon on multiplie le boost a chaque tick).
                const float dist = (distSq > 0.0001f) ? std::sqrt(distSq) : 1.0f;
                const float nx = dx / dist;
                const float ny = dy / dist;
                d.x = bx + nx * (radSum + 1.0f);
                d.y = by + ny * (radSum + 1.0f);
            }
        }

        // ────────────────────────────────────────────────────────────────────
        // SetNetDrops — remplace mDrops avec la liste fournie. Cote CLIENT
        // reseau : reconstruit l'etat visuel des orbes a partir du snapshot
        // HOST (PktState.drops). On preserve juste les params visuels (pulse,
        // alive, rayon de defaut, kind, isBonus) -- la collision est decidee
        // cote HOST, le CLIENT n'a qu'a afficher.
        // ────────────────────────────────────────────────────────────────────
        void PowerUpSystem::SetNetDrops(const PowerUpDrop* drops, int count)
        {
            mDrops.Clear();
            for (int i = 0; i < count; ++i)
            {
                PowerUpDrop d = drops[i];
                d.alive = true;
                if (d.r <= 0.0f) d.r = 16.0f;
                mDrops.PushBack(d);
            }
        }

        // ────────────────────────────────────────────────────────────────────
        // SetNetNotification — cote CLIENT, le HOST envoie ses notifications
        // power-up via le PktState. On les injecte ici dans mNotifs pour que
        // le rendering existant (PowerUpSystem::Render OU GameplayScene) les
        // affiche normalement. timeLeft=0 -> clear la notif.
        // ────────────────────────────────────────────────────────────────────
        void PowerUpSystem::SetNetNotification(int side, bool isBonus,
                                               uint8 kind, float timeLeft)
        {
            Notification& n = mNotifs[SideIdx(side)];
            if (timeLeft <= 0.0f)
            {
                n.active = false;
                return;
            }
            // Nouvelle notif (changement de type) : reset duration/timer.
            if (!n.active || n.kind != kind || n.isBonus != isBonus)
            {
                n.isBonus  = isBonus;
                n.kind     = kind;
                n.side     = side;
                n.duration = kNotifDuration;
                n.timeLeft = timeLeft;
                n.active   = true;
            }
            else
            {
                // Meme notif que precedente : juste sync le timer (peut
                // descendre si plusieurs snapshots arrivent dans la duree).
                n.timeLeft = timeLeft;
            }
        }

        void PowerUpSystem::SpawnRandomDrop(float arenaW)
        {
            PowerUpDrop d{};
            d.alive   = true;
            // Spawn dans le tiers central du terrain (35% - 65%). Le drop
            // descend VERTICALEMENT jusqu'a ce qu'une balle le frappe : la
            // collision transfere le momentum de la balle au drop, qui part
            // alors en diagonale -- typiquement vers le paddle adverse.
            // Mecanique : "le drop est un projectile passif que la balle
            // propulse logiquement vers l'autre cote du terrain".
            d.x     = arenaW * (0.35f + Rand01() * 0.30f);
            d.y     = -10.0f;
            d.r     = 16.0f;
            d.vx    = 0.0f;        // derive horizontale nulle au spawn
            d.vy    = 80.0f;       // descente verticale (px/sec)
            d.pulse = Rand01() * 6.28318f;
            d.isBonus = (Rand01() < 0.60f);
            if (d.isBonus)
            {
                const BonusType all[6] = {
                    BonusType::GiantPaddle, BonusType::PaddleSpeed,
                    BonusType::DoublePoint, BonusType::Shield,
                    BonusType::SlowBall,    BonusType::RandomStar
                };
                d.kind = (uint8)all[(int)(Rand01() * 6.0f) % 6];
            }
            else
            {
                const MalusType all[6] = {
                    MalusType::Blind,    MalusType::MiniPaddle,
                    MalusType::InvertControls, MalusType::Freeze,
                    MalusType::FastBall, MalusType::TeleportPaddle
                };
                d.kind = (uint8)all[(int)(Rand01() * 6.0f) % 6];
            }
            mDrops.PushBack(d);
        }

        bool PowerUpSystem::CheckPaddleCollision(int side,
                                                 float paddleX, float paddleY,
                                                 float paddleW, float paddleH,
                                                 float* outX, float* outY,
                                                 math::NkColor* outColor)
        {
            for (uint32 i = 0; i < mDrops.Size(); ++i)
            {
                PowerUpDrop& d = mDrops[i];
                if (!d.alive) continue;
                // AABB-cercle approx : on teste centre orbe vs paddle AABB
                // augmente du rayon.
                if (d.x + d.r < paddleX || d.x - d.r > paddleX + paddleW) continue;
                if (d.y + d.r < paddleY || d.y - d.r > paddleY + paddleH) continue;

                // Hit ! Renseigne les out params si demandes.
                if (outX)     *outX     = d.x;
                if (outY)     *outY     = d.y;
                if (outColor) *outColor = d.isBonus
                    ? BonusDesc((BonusType)d.kind).color
                    : MalusDesc((MalusType)d.kind).color;
                // Applique l'effet.
                if (d.isBonus)
                {
                    ApplyBonus((BonusType)d.kind, side);
                }
                else
                {
                    // Malus -> a l'adversaire du joueur qui a touche l'orbe
                    ApplyMalus((MalusType)d.kind, -side);
                }
                d.alive = false;  // sera retire au prochain Update
                return true;
            }
            return false;
        }

        // ── Queries pour GameplayScene ───────────────────────────────────────
        bool PowerUpSystem::HasEffect(bool isBonus, uint8 kind, int side) const
        {
            for (uint32 i = 0; i < mEffects.Size(); ++i)
            {
                const ActiveEffect& e = mEffects[i];
                if (e.isBonus == isBonus && e.kind == kind && e.side == side)
                    return true;
            }
            return false;
        }

        float PowerUpSystem::GetPaddleHeightMul(int side) const
        {
            const bool giant = HasEffect(true,  (uint8)BonusType::GiantPaddle, side);
            const bool mini  = HasEffect(false, (uint8)MalusType::MiniPaddle,  side);
            // En cas de cumul "giant + mini" : on prend l'effet le plus
            // recent. Approximation simple : giant prioritaire (rarement
            // simultane car les triggers different).
            if (giant) return 2.0f;
            if (mini)  return 0.5f;
            return 1.0f;
        }
        float PowerUpSystem::GetPaddleSpeedMul(int side) const
        {
            if (HasEffect(true, (uint8)BonusType::PaddleSpeed, side)) return 2.0f;
            return 1.0f;
        }
        float PowerUpSystem::GetBallSpeedMul() const
        {
            // SlowBall et FastBall sont mutuellement exclusifs (ApplyBonus /
            // ApplyMalus purgent l'autre avant d'appliquer).
            for (uint32 i = 0; i < mEffects.Size(); ++i)
            {
                const ActiveEffect& e = mEffects[i];
                if (e.isBonus && e.kind == (uint8)BonusType::SlowBall) return 0.5f;
                if (!e.isBonus && e.kind == (uint8)MalusType::FastBall) return 3.0f;
            }
            return 1.0f;
        }
        bool PowerUpSystem::IsBlind(int side) const
        {
            return HasEffect(false, (uint8)MalusType::Blind, side);
        }
        bool PowerUpSystem::IsFrozen(int side) const
        {
            return HasEffect(false, (uint8)MalusType::Freeze, side);
        }
        bool PowerUpSystem::HasInvertedControls(int side) const
        {
            return HasEffect(false, (uint8)MalusType::InvertControls, side);
        }
        bool PowerUpSystem::ConsumeShield(int side)
        {
            for (uint32 i = 0; i < mEffects.Size(); ++i)
            {
                ActiveEffect& e = mEffects[i];
                if (e.isBonus && e.kind == (uint8)BonusType::Shield && e.side == side
                 && e.charges > 0)
                {
                    e.charges--;
                    if (e.charges <= 0) e.timeLeft = 0.0f;  // expire au prochain update
                    return true;
                }
            }
            return false;
        }
        bool PowerUpSystem::ConsumeDoublePoint(int side)
        {
            for (uint32 i = 0; i < mEffects.Size(); ++i)
            {
                ActiveEffect& e = mEffects[i];
                if (e.isBonus && e.kind == (uint8)BonusType::DoublePoint && e.side == side
                 && e.charges > 0)
                {
                    e.charges--;
                    if (e.charges <= 0) e.timeLeft = 0.0f;
                    return true;
                }
            }
            return false;
        }
        math::NkColor PowerUpSystem::GetEffectColor(int i) const
        {
            const ActiveEffect& e = mEffects[i];
            if (e.isBonus) return BonusDesc((BonusType)e.kind).color;
            return MalusDesc((MalusType)e.kind).color;
        }
        const char* PowerUpSystem::GetEffectName(int i) const
        {
            const ActiveEffect& e = mEffects[i];
            if (e.isBonus) return BonusName((BonusType)e.kind);
            return MalusName((MalusType)e.kind);
        }
        math::NkColor PowerUpSystem::NotifColor(const Notification& n) const
        {
            if (n.isBonus) return BonusDesc((BonusType)n.kind).color;
            return MalusDesc((MalusType)n.kind).color;
        }
        const char* PowerUpSystem::NotifName(const Notification& n) const
        {
            if (n.isBonus) return BonusName((BonusType)n.kind);
            return MalusName((MalusType)n.kind);
        }

        bool PowerUpSystem::TryTeleport(int side, float& outFracY)
        {
            const int idx = SideIdx(side);
            if (!mTeleportPending[idx]) return false;
            outFracY = mTeleportFracY[idx];
            mTeleportPending[idx] = false;
            return true;
        }

        // ── Helpers internes ────────────────────────────────────────────────
        void PowerUpSystem::AddEffect(bool isBonus, uint8 kind, int side,
                                      float duration, int charges)
        {
            ActiveEffect e{};
            e.isBonus  = isBonus;
            e.kind     = kind;
            e.side     = side;
            e.duration = (duration > 0.0f) ? duration : 60.0f;  // DoublePoint = jusqu'a usage
            e.timeLeft = e.duration;
            e.charges  = charges;
            mEffects.PushBack(e);
        }
        void PowerUpSystem::RemoveEffectsByKind(bool isBonus, uint8 kind, int side)
        {
            for (uint32 i = 0; i < mEffects.Size(); )
            {
                const ActiveEffect& e = mEffects[i];
                if (e.isBonus == isBonus && e.kind == kind && e.side == side)
                {
                    mEffects.RemoveAt(i);
                    continue;
                }
                ++i;
            }
        }

    } // namespace songoo
} // namespace nkentseu
