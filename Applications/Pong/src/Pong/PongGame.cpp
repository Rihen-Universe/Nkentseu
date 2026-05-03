// =============================================================================
// PongGame.cpp
// =============================================================================
// Description : Implementation complete de PongGame.
//               Contient toute la logique de jeu, les menus UI et le rendu.
//
// Organisation des sections :
//   1. Fonctions locales (helpers non-membres, anonymes)
//   2. PongGame::Init / StartGame / ResetBall / OnResize
//   3. SpawnObstacles (6 presets)
//   4. Update (dispatcher + navigation menus)
//   5. UpdateBall / UpdatePaddles / UpdateObstacles / Particles
//   6. Collisions (BallVsPaddle, BallVsObstacle, ApplyObstacleEffect)
//   7. Render (dispatcher + couches visuelles)
//   8. Helpers UI (DrawPanel, DrawButton, DrawStars, DrawObstaclePreview)
//
// Conventions :
//   - Une instruction par ligne.
//   - Chaque bloc delimitee par {} est indente d\'un niveau supplementaire.
//   - Chaque section de code est precedee d\'un commentaire descriptif.
//   - Tout texte affiche a l\'ecran est blanc (255,255,255) pour lisibilite.
// =============================================================================

#include "PongGame.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <cstring>
#include <cstdio>

using namespace nkentseu;


// =============================================================================
// Section 1 : Fonctions locales (espace anonyme)
// Description : Utilitaires de couleur utilises uniquement dans ce fichier.
//               Invisibles depuis les autres unites de compilation.
// =============================================================================
namespace
{
    // ── WithAlpha ─────────────────────────────────────────────────────────────
    // Multiplie le canal alpha existant d\'une couleur par le facteur a [0..1].
    // Utile pour les fondus progressifs sans modifier la teinte.
    inline math::NkColor WithAlpha(math::NkColor c, float a) noexcept
    {
        c.a = static_cast<uint8_t>(pongmath::Clamp(a, 0.0f, 1.0f) * c.a);
        return c;
    }

    // ── AlphaF ────────────────────────────────────────────────────────────────
    // Remplace le canal alpha d\'une couleur par a * 255 [0..255].
    // Utile pour appliquer une transparence independante de l\'alpha d\'origine.
    inline math::NkColor AlphaF(math::NkColor c, float a) noexcept
    {
        c.a = static_cast<uint8_t>(pongmath::Clamp(a, 0.0f, 1.0f) * 255.0f);
        return c;
    }

    // ── Blend ─────────────────────────────────────────────────────────────────
    // Fusion alpha standard Porter-Duff SRC_OVER.
    // Retourne la couleur resultante de src posee sur dst.
    inline math::NkColor Blend(math::NkColor dst, math::NkColor src) noexcept
    {
        if (src.a == 255)
        {
            return src;
        }
        if (src.a == 0)
        {
            return dst;
        }
        uint32_t a  = src.a;
        uint32_t ia = 255u - a;
        math::NkColor result;
        result.r = static_cast<uint8_t>((a * src.r + ia * dst.r) >> 8);
        result.g = static_cast<uint8_t>((a * src.g + ia * dst.g) >> 8);
        result.b = static_cast<uint8_t>((a * src.b + ia * dst.b) >> 8);
        result.a = static_cast<uint8_t>(src.a + ((ia * dst.a) >> 8));
        return result;
    }

} // namespace anonyme


// =============================================================================
// Section 2 : Init / StartGame / ResetBall / OnResize
// =============================================================================

// ── PongGame::Init ────────────────────────────────────────────────────────────
// Reinitialise tous les composants du jeu.
// Remet le score a zero, repositionne les raquettes, regenere les obstacles
// et place le jeu sur l\'ecran MainMenu.
// Doit etre rappele apres chaque redimensionnement de fenetre.
void PongGame::Init()
{
    // Recuperer les dimensions courantes du renderer
    uint32_t W = mRenderer.Width();
    uint32_t H = mRenderer.Height();

    // Configurer la raquette du joueur (cote gauche)
    mPlayer.x     = 20.0f;
    mPlayer.y     = static_cast<float>(H) * 0.5f - mPlayer.h * 0.5f;
    mPlayer.color = gamecolors::Player();
    mPlayer.isLeft = true;
    mPlayer.score = 0;

    // Configurer la raquette de l\'IA (cote droit)
    mAI.x     = static_cast<float>(W) - 20.0f - mAI.w;
    mAI.y     = static_cast<float>(H) * 0.5f - mAI.h * 0.5f;
    mAI.color = gamecolors::AI();
    mAI.isLeft = false;
    mAI.score = 0;

    // Generer les obstacles selon le preset courant
    SpawnObstacles();

    // Initialiser la balle (service vers la gauche au premier lancement)
    ResetBall(true);

    // Demarrer sur le splash screen (basculera vers MainMenu automatiquement)
    mState       = GameState::SplashScreen;
    mSplashTimer = 3.0f;
    mMainMenuSel = 0;
    mTime        = 0.0f;
}

// ── PongGame::OnResize ────────────────────────────────────────────────────────
// Adapte les positions des raquettes et regenere les obstacles apres un
// redimensionnement de fenetre, SANS changer l'etat du jeu, les scores,
// ou les selections des menus. L'etat courant est conserve.
// Appele depuis Apps.cpp lors d'un NkWindowResizeEvent / NkWindowMaximizeEvent.
void PongGame::OnResize()
{
    // Recuperer les nouvelles dimensions du renderer
    uint32_t W = mRenderer.Width();
    uint32_t H = mRenderer.Height();

    // Sauvegarder les donnees persistantes avant repositionnement
    int playerScore = mPlayer.score;
    int aiScore     = mAI.score;
    GameState savedState = mState;
    int savedMenuSel     = mMainMenuSel;
    int savedDiffSel     = mDiffSel;
    int savedObsSel      = mObsSel;
    int savedPauseSel    = mPauseSel;
    float savedTime      = mTime;

    // Repositionner la raquette du joueur en gardant la proportion verticale
    float playerYRatio = 0.5f;
    if (H > 0.0f)
    {
        playerYRatio = mPlayer.y / static_cast<float>(mRenderer.Height());
    }
    mPlayer.x = 20.0f;
    mPlayer.y = static_cast<float>(H) * playerYRatio;
    if (mPlayer.y + mPlayer.h > static_cast<float>(H))
    {
        mPlayer.y = static_cast<float>(H) - mPlayer.h;
    }
    if (mPlayer.y < 0.0f)
    {
        mPlayer.y = 0.0f;
    }
    mPlayer.score = playerScore;

    // Repositionner la raquette de l\'IA en gardant la proportion verticale
    float aiYRatio = 0.5f;
    if (H > 0.0f)
    {
        aiYRatio = mAI.y / static_cast<float>(mRenderer.Height());
    }
    mAI.x = static_cast<float>(W) - 20.0f - mAI.w;
    mAI.y = static_cast<float>(H) * aiYRatio;
    if (mAI.y + mAI.h > static_cast<float>(H))
    {
        mAI.y = static_cast<float>(H) - mAI.h;
    }
    if (mAI.y < 0.0f)
    {
        mAI.y = 0.0f;
    }
    mAI.score = aiScore;

    // Replacer la balle au centre si elle est hors du nouveau terrain
    if (mBall.x < 0.0f || mBall.x > static_cast<float>(W) ||
        mBall.y < 0.0f || mBall.y > static_cast<float>(H))
    {
        ResetBall(true);
    }

    // Regenere les obstacles adaptes aux nouvelles dimensions
    SpawnObstacles();

    // Restaurer l'etat du jeu et les selections des menus
    mState       = savedState;
    mMainMenuSel = savedMenuSel;
    mDiffSel     = savedDiffSel;
    mObsSel      = savedObsSel;
    mPauseSel    = savedPauseSel;
    mTime        = savedTime;

    // Nettoyer les particules et effets visuels qui pourraient etre hors ecran
    mParticles.clear();
    mShake      = 0.0f;
    mFlashAlpha = 0.0f;
}

// ── PongGame::StartGame ───────────────────────────────────────────────────────
// Applique les parametres selectionnes dans les menus et demarre la partie.
// Configure les parametres IA selon la difficulte choisie.
// Remet les scores a zero et place le jeu en etat Playing.
void PongGame::StartGame()
{
    // Recuperer les dimensions du terrain
    uint32_t W = mRenderer.Width();
    uint32_t H = mRenderer.Height();

    // Repositionner les raquettes et remettre les scores a zero
    mPlayer.x     = 20.0f;
    mPlayer.y     = static_cast<float>(H) * 0.5f - mPlayer.h * 0.5f;
    mPlayer.score = 0;
    mAI.x         = static_cast<float>(W) - 20.0f - mAI.w;
    mAI.y         = static_cast<float>(H) * 0.5f - mAI.h * 0.5f;
    mAI.score     = 0;

    // Appliquer la difficulte selectionnee aux parametres de l\'IA
    mSettings.difficulty     = static_cast<AIDifficulty>(mDiffSel);
    mSettings.obstaclePreset = static_cast<ObstaclePreset>(mObsSel);

    // Configurer vitesse et reaction de l\'IA selon la difficulte
    if (mSettings.difficulty == AIDifficulty::Easy)
    {
        mAI.aiReaction = 0.28f;
        mAI.aiSpeed    = 230.0f;
    }
    else if (mSettings.difficulty == AIDifficulty::Medium)
    {
        mAI.aiReaction = 0.52f;
        mAI.aiSpeed    = 340.0f;
    }
    else if (mSettings.difficulty == AIDifficulty::Hard)
    {
        mAI.aiReaction = 0.73f;
        mAI.aiSpeed    = 410.0f;
    }
    else // Expert
    {
        mAI.aiReaction = 0.88f;
        mAI.aiSpeed    = 450.0f;
    }

    // Reinitialiser l\'erreur et le timer IA
    mAI.aiError = 0.0f;
    mAiErrTimer = 0.0f;

    // Generer les obstacles du preset selectionne
    SpawnObstacles();

    // Reinitialiser la balle (service vers la gauche)
    ResetBall(true);

    // Nettoyer les effets residuels
    mParticles.clear();
    mShake      = 0.0f;
    mFlashAlpha = 0.0f;

    // Passer en etat de jeu actif
    mState = GameState::Playing;
}

// ── PongGame::ResetBall ───────────────────────────────────────────────────────
// Remet la balle au centre du terrain avec une direction aleatoire.
// La composante Y de la direction est aleatoirement variee dans [-0.45, 0.45]
// pour eviter les trajectoires trop horizontales ou trop verticales.
// Parametre leftServe : true = balle part vers la gauche (joueur sert).
void PongGame::ResetBall(bool leftServe)
{
    // Calculer la position centrale du terrain
    float W   = static_cast<float>(mRenderer.Width());
    float H   = static_cast<float>(mRenderer.Height());
    float cx  = W * 0.5f;
    float cy  = H * 0.5f;

    // Generer un angle aleatoire pour la composante verticale
    float angle = pongmath::RandF(mRng, -0.45f, 0.45f);

    // Choisir la direction horizontale selon le service
    float dx = leftServe ? 1.0f : -1.0f;
    float dy = math::NkSin(angle);

    // Reinitialiser la vitesse a la valeur de base
    mBall.speed = Ball::BASE_SPEED;

    // Placer la balle et appliquer la direction
    mBall.Init(cx, cy, dx, dy);

    // S\'assurer que la vitesse est exactement BASE_SPEED apres Init
    mBall.speed = Ball::BASE_SPEED;
    mBall.ClampSpeed();
}


// =============================================================================
// Section 3 : SpawnObstacles (6 presets)
// Description : Dispatch et implementation des 6 configurations d\'obstacles.
//               Chaque preset place les obstacles a des positions variees
//               (pas forcement au centre) selon la configuration du niveau.
// =============================================================================

// ── PongGame::SpawnObstacles ──────────────────────────────────────────────────
// Vide la liste d\'obstacles puis appelle la fonction du preset courant.
void PongGame::SpawnObstacles()
{
    mObstacles.clear();

    switch (mSettings.obstaclePreset)
    {
    case ObstaclePreset::None:
        SpawnObstacles_None();
        break;
    case ObstaclePreset::Classic:
        SpawnObstacles_Classic();
        break;
    case ObstaclePreset::Chaos:
        SpawnObstacles_Chaos();
        break;
    case ObstaclePreset::Gauntlet:
        SpawnObstacles_Gauntlet();
        break;
    case ObstaclePreset::Portal:
        SpawnObstacles_Portal();
        break;
    case ObstaclePreset::Boss:
        SpawnObstacles_Boss();
        break;
    }
}

// ── SpawnObstacles_None ───────────────────────────────────────────────────────
// Terrain vide. Aucun obstacle a creer.
void PongGame::SpawnObstacles_None()
{
    // Intentionnellement vide : Pong classique sans obstacles.
}

// ── SpawnObstacles_Classic ────────────────────────────────────────────────────
// 6 obstacles symetriques autour du centre du terrain.
// Layout : Boost en haut, Reduce en bas, 2 Redirect sur les flancs,
//          RandomDeflect a gauche, Magnetic au centre.
void PongGame::SpawnObstacles_Classic()
{
    float W  = static_cast<float>(mRenderer.Width());
    float H  = static_cast<float>(mRenderer.Height());
    float cx = W * 0.5f;

    // ── Obstacle 1 : SpeedBoost (haut-centre, mobile vertical) ───────────────
    {
        Obstacle o;
        o.x           = cx - 22.0f;
        o.y           = H * 0.16f;
        o.w           = 44.0f;
        o.h           = 18.0f;
        o.effect      = ObstacleEffect::SpeedBoost;
        o.speedMult   = 1.35f;
        o.color       = gamecolors::ObsBoost();
        o.glowColor   = { 50, 255, 100, 80 };
        o.vy          = 50.0f;
        o.hasPhase    = true;
        o.phaseCooldown = 4.0f;
        o.phaseDuration = 1.5f;
        o.phaseTimer  = 4.0f;
        mObstacles.push_back(o);
    }

    // ── Obstacle 2 : SpeedReduce (bas-centre, mobile vertical inverse) ────────
    {
        Obstacle o;
        o.x         = cx - 22.0f;
        o.y         = H * 0.72f;
        o.w         = 44.0f;
        o.h         = 18.0f;
        o.effect    = ObstacleEffect::SpeedReduce;
        o.speedMult = 1.4f;
        o.color     = gamecolors::ObsReduce();
        o.glowColor = { 200, 80, 80, 80 };
        o.vy        = -40.0f;
        mObstacles.push_back(o);
    }

    // ── Obstacle 3 : Redirect (flanc droit, haut) ────────────────────────────
    {
        Obstacle o;
        o.x            = cx + 70.0f;
        o.y            = H * 0.28f;
        o.w            = 14.0f;
        o.h            = 55.0f;
        o.effect       = ObstacleEffect::Redirect;
        o.fixedAngle   = 0.45f;
        o.color        = gamecolors::ObsRedirect();
        o.glowColor    = { 180, 100, 255, 80 };
        o.hasPhase     = true;
        o.phaseDuration = 0.9f;
        o.phaseCooldown = 2.5f;
        o.phaseTimer   = 1.0f;
        mObstacles.push_back(o);
    }

    // ── Obstacle 4 : Redirect (flanc gauche, haut - miroir) ──────────────────
    {
        Obstacle o;
        o.x            = cx - 84.0f;
        o.y            = H * 0.28f;
        o.w            = 14.0f;
        o.h            = 55.0f;
        o.effect       = ObstacleEffect::Redirect;
        o.fixedAngle   = -0.45f;
        o.color        = gamecolors::ObsRedirect();
        o.glowColor    = { 180, 100, 255, 80 };
        o.hasPhase     = true;
        o.phaseDuration = 0.9f;
        o.phaseCooldown = 2.5f;
        o.phaseTimer   = 2.0f;
        mObstacles.push_back(o);
    }

    // ── Obstacle 5 : RandomDeflect (bas-gauche, mobile horizontal) ───────────
    {
        Obstacle o;
        o.x          = cx - 140.0f;
        o.y          = H * 0.62f;
        o.w          = 22.0f;
        o.h          = 22.0f;
        o.effect     = ObstacleEffect::RandomDeflect;
        o.angleNoise = 0.55f;
        o.color      = gamecolors::ObsRandom();
        o.glowColor  = { 255, 180, 0, 80 };
        o.vx         = 35.0f;
        mObstacles.push_back(o);
    }

    // ── Obstacle 6 : Magnetic (centre, mobile lent avec phase) ───────────────
    {
        Obstacle o;
        o.x           = cx - 10.0f;
        o.y           = H * 0.5f - 10.0f;
        o.w           = 20.0f;
        o.h           = 20.0f;
        o.effect      = ObstacleEffect::Magnetic;
        o.magnetForce = 110.0f;
        o.color       = gamecolors::ObsMagnetic();
        o.glowColor   = { 0, 200, 255, 80 };
        o.pulseSpeed  = 3.0f;
        o.hasPhase    = true;
        o.phaseDuration = 2.0f;
        o.phaseCooldown = 2.0f;
        o.phaseTimer  = 1.5f;
        mObstacles.push_back(o);
    }
}

// ── SpawnObstacles_Chaos ──────────────────────────────────────────────────────
// 12 obstacles disperses aleatoirement sur tout le terrain.
// La seed 777 garantit une disposition reproductible a chaque partie.
// Les zones proches des raquettes (130 px des bords) sont evitees.
void PongGame::SpawnObstacles_Chaos()
{
    float W = static_cast<float>(mRenderer.Width());
    float H = static_cast<float>(mRenderer.Height());

    // RNG reproductible avec seed fixe pour un layout deterministe
    std::mt19937 rng(777u);

    // Zones interdites : eviter les raquettes et les bords extremes
    float padX = 130.0f;
    float padY =  30.0f;

    // Effets et couleurs disponibles pour le tirage aleatoire
    ObstacleEffect effects[] = {
        ObstacleEffect::SpeedBoost,
        ObstacleEffect::SpeedReduce,
        ObstacleEffect::Redirect,
        ObstacleEffect::RandomDeflect,
        ObstacleEffect::Phase
    };
    math::NkColor colors[] = {
        gamecolors::ObsBoost(),
        gamecolors::ObsReduce(),
        gamecolors::ObsRedirect(),
        gamecolors::ObsRandom(),
        gamecolors::ObsPhase()
    };

    for (int i = 0; i < 12; ++i)
    {
        Obstacle o;

        // Dimensions aleatoires
        o.w = pongmath::RandF(rng, 16.0f, 32.0f);
        o.h = pongmath::RandF(rng, 14.0f, 30.0f);

        // Position aleatoire dans la zone autorisee
        o.x = pongmath::RandF(rng, padX, W - padX - o.w);
        o.y = pongmath::RandF(rng, padY, H - padY - o.h);

        // Vitesse aleatoire (50% des obstacles sont mobiles)
        bool moving = pongmath::RandF(rng, 0.0f, 1.0f) > 0.5f;
        if (moving)
        {
            o.vx = pongmath::RandF(rng, -60.0f, 60.0f);
            o.vy = pongmath::RandF(rng, -50.0f, 50.0f);
        }

        // Type d\'effet aleatoire parmi 5 possibilites
        int typeIdx = static_cast<int>(pongmath::RandF(rng, 0.0f, 5.0f));
        if (typeIdx >= 5)
        {
            typeIdx = 4;
        }
        o.effect    = effects[typeIdx];
        o.color     = colors[typeIdx];
        o.glowColor = { o.color.r, o.color.g, o.color.b, 70 };

        // Parametres d\'effet aleatoires dans des plages raisonnables
        o.speedMult  = pongmath::RandF(rng, 1.2f, 1.5f);
        o.fixedAngle = pongmath::RandF(rng, -0.6f, 0.6f);
        o.angleNoise = pongmath::RandF(rng, 0.3f, 0.65f);
        o.pulseSpeed = pongmath::RandF(rng, 1.5f, 4.0f);

        // Phase aleatoire (40% des obstacles ont une phase)
        bool hasPhaseRoll = pongmath::RandF(rng, 0.0f, 1.0f) > 0.4f;
        if (hasPhaseRoll)
        {
            o.hasPhase      = true;
            o.phaseDuration = pongmath::RandF(rng, 0.8f, 2.0f);
            o.phaseCooldown = pongmath::RandF(rng, 1.5f, 4.0f);
            o.phaseTimer    = pongmath::RandF(rng, 0.0f, 4.0f);
        }

        mObstacles.push_back(o);
    }
}

// ── SpawnObstacles_Gauntlet ───────────────────────────────────────────────────
// 3 rangees horizontales d\'obstacles formant des corridors.
// Les gaps sont decales entre rangees pour que les passages soient variables.
// Rangee 1 (haut, y=22%) : 2 SpeedBoost fixes
// Rangee 2 (centre, y=50%) : 3 Redirect avec phase, decales
// Rangee 3 (bas, y=78%) : 2 SpeedReduce mobiles
// Plus 1 RandomDeflect central mobile pour pimenter.
void PongGame::SpawnObstacles_Gauntlet()
{
    float W      = static_cast<float>(mRenderer.Width());
    float H      = static_cast<float>(mRenderer.Height());
    float margin = 120.0f;
    float obW    = 38.0f;
    float obH    = 16.0f;
    // Espacement entre les 4 colonnes possibles
    float spacing = (W - 2.0f * margin - 4.0f * obW) / 3.0f;

    // ── Rangee 1 : SpeedBoost (colonnes 0 et 2, gaps en 1 et 3) ─────────────
    for (int i = 0; i < 4; ++i)
    {
        // Sauter les colonnes 1 et 3 pour creer des gaps
        if (i == 1 || i == 3)
        {
            continue;
        }

        Obstacle o;
        o.x         = margin + static_cast<float>(i) * (obW + spacing);
        o.y         = H * 0.22f;
        o.w         = obW;
        o.h         = obH;
        o.effect    = ObstacleEffect::SpeedBoost;
        o.speedMult = 1.25f;
        o.color     = gamecolors::ObsBoost();
        o.glowColor = { 50, 255, 100, 70 };
        mObstacles.push_back(o);
    }

    // ── Rangee 2 : Redirect (3 obstacles decales, avec phase) ────────────────
    float offset2 = obW * 0.5f + spacing * 0.5f;
    for (int i = 0; i < 3; ++i)
    {
        Obstacle o;
        o.x             = margin + offset2 + static_cast<float>(i) * (obW + spacing + 6.0f);
        o.y             = H * 0.50f - (obH + 4.0f) * 0.5f;
        o.w             = obW;
        o.h             = obH + 4.0f;
        o.effect        = ObstacleEffect::Redirect;
        o.fixedAngle    = (i % 2 == 0) ? 0.35f : -0.35f;
        o.color         = gamecolors::ObsRedirect();
        o.glowColor     = { 180, 100, 255, 70 };
        o.hasPhase      = true;
        o.phaseDuration = 1.2f;
        o.phaseCooldown = 2.8f;
        o.phaseTimer    = static_cast<float>(i) * 0.9f;
        mObstacles.push_back(o);
    }

    // ── Rangee 3 : SpeedReduce (colonnes 1 et 3, gaps en 0 et 2, mobiles) ────
    for (int i = 0; i < 4; ++i)
    {
        // Sauter les colonnes 0 et 2 (gaps decales par rapport rangee 1)
        if (i == 0 || i == 2)
        {
            continue;
        }

        Obstacle o;
        o.x         = margin + static_cast<float>(i) * (obW + spacing);
        o.y         = H * 0.78f;
        o.w         = obW;
        o.h         = obH;
        o.effect    = ObstacleEffect::SpeedReduce;
        o.speedMult = 1.3f;
        o.color     = gamecolors::ObsReduce();
        o.glowColor = { 200, 80, 80, 70 };
        // Mouvement horizontal alternatif
        o.vx = (i % 2 == 0) ? 30.0f : -30.0f;
        mObstacles.push_back(o);
    }

    // ── Obstacle supplementaire : RandomDeflect central mobile ───────────────
    {
        Obstacle o;
        o.x          = W * 0.5f - 10.0f;
        o.y          = H * 0.5f - 10.0f;
        o.w          = 20.0f;
        o.h          = 20.0f;
        o.effect     = ObstacleEffect::RandomDeflect;
        o.angleNoise = 0.6f;
        o.color      = gamecolors::ObsRandom();
        o.glowColor  = { 255, 180, 0, 70 };
        o.vx         = 20.0f;
        o.vy         = 15.0f;
        mObstacles.push_back(o);
    }
}

// ── SpawnObstacles_Portal ─────────────────────────────────────────────────────
// 4 paires de portails de teleportation disperses sur le terrain.
// Chaque paire est posee a des positions eloignees pour maximiser la surprise.
// 2 obstacles de deviation supplementaires pour complexifier les trajectoires.
void PongGame::SpawnObstacles_Portal()
{
    float W = static_cast<float>(mRenderer.Width());
    float H = static_cast<float>(mRenderer.Height());

    // Positions des 4 paires de portails (x1,y1) <-> (x2,y2)
    float px1[] = { W * 0.30f, W * 0.20f, W * 0.42f, W * 0.35f };
    float py1[] = { H * 0.20f, H * 0.55f, H * 0.12f, H * 0.60f };
    float px2[] = { W * 0.65f, W * 0.72f, W * 0.52f, W * 0.60f };
    float py2[] = { H * 0.75f, H * 0.30f, H * 0.82f, H * 0.18f };

    for (int p = 0; p < 4; ++p)
    {
        // Index du premier portail de cette paire
        int id1 = static_cast<int>(mObstacles.size());

        // Portail A
        {
            Obstacle o;
            o.x         = px1[p] - 12.0f;
            o.y         = py1[p] - 12.0f;
            o.w         = 24.0f;
            o.h         = 24.0f;
            o.effect    = ObstacleEffect::Portal;
            o.pairedId  = id1 + 1;
            o.color     = gamecolors::ObsPortal();
            o.glowColor = { 255, 60, 200, 80 };
            o.pulseSpeed = 4.0f;
            mObstacles.push_back(o);
        }

        // Portail B (paire avec A)
        {
            Obstacle o;
            o.x         = px2[p] - 12.0f;
            o.y         = py2[p] - 12.0f;
            o.w         = 24.0f;
            o.h         = 24.0f;
            o.effect    = ObstacleEffect::Portal;
            o.pairedId  = id1;
            o.color     = gamecolors::ObsPortal();
            o.glowColor = { 255, 60, 200, 80 };
            o.pulseSpeed = 4.0f;
            mObstacles.push_back(o);
        }
    }

    // ── Redirect vertical au centre (mobile) ──────────────────────────────────
    {
        Obstacle o;
        o.x          = W * 0.50f - 6.0f;
        o.y          = H * 0.38f;
        o.w          = 12.0f;
        o.h          = 45.0f;
        o.effect     = ObstacleEffect::Redirect;
        o.fixedAngle = 0.4f;
        o.color      = gamecolors::ObsRedirect();
        o.glowColor  = { 180, 100, 255, 70 };
        o.vy         = 30.0f;
        mObstacles.push_back(o);
    }

    // ── RandomDeflect horizontal (mobile) ────────────────────────────────────
    {
        Obstacle o;
        o.x          = W * 0.38f;
        o.y          = H * 0.50f;
        o.w          = 40.0f;
        o.h          = 12.0f;
        o.effect     = ObstacleEffect::RandomDeflect;
        o.angleNoise = 0.5f;
        o.color      = gamecolors::ObsRandom();
        o.glowColor  = { 255, 180, 0, 70 };
        o.vx         = -25.0f;
        mObstacles.push_back(o);
    }
}

// ── SpawnObstacles_Boss ───────────────────────────────────────────────────────
// Configuration la plus complexe : aimant central mobile + anneau de 6
// obstacles en orbite + 1 paire de portails aux coins.
// L\'anneau utilise des velocites orbitales pour un mouvement circulaire.
void PongGame::SpawnObstacles_Boss()
{
    float W  = static_cast<float>(mRenderer.Width());
    float H  = static_cast<float>(mRenderer.Height());
    float cx = W * 0.5f;
    float cy = H * 0.5f;

    // ── Aimant central (gros, mobile lentement) ───────────────────────────────
    {
        Obstacle o;
        o.x           = cx - 15.0f;
        o.y           = cy - 15.0f;
        o.w           = 30.0f;
        o.h           = 30.0f;
        o.effect      = ObstacleEffect::Magnetic;
        o.magnetForce = 160.0f;
        o.color       = gamecolors::ObsMagnetic();
        o.glowColor   = { 0, 200, 255, 90 };
        o.pulseSpeed  = 3.5f;
        o.vx          = 20.0f;
        o.vy          = 15.0f;
        mObstacles.push_back(o);
    }

    // ── Anneau de 6 obstacles en orbite ──────────────────────────────────────
    // Angles regulierement repartis (0, 60, 120, 180, 240, 300 degres)
    float ringRadius = H * 0.25f;
    float orbitSpeed = 45.0f;

    ObstacleEffect ringEffects[] = {
        ObstacleEffect::SpeedBoost,
        ObstacleEffect::SpeedReduce,
        ObstacleEffect::Redirect,
        ObstacleEffect::RandomDeflect,
        ObstacleEffect::Phase,
        ObstacleEffect::SpeedBoost
    };
    math::NkColor ringColors[] = {
        gamecolors::ObsBoost(),
        gamecolors::ObsReduce(),
        gamecolors::ObsRedirect(),
        gamecolors::ObsRandom(),
        gamecolors::ObsPhase(),
        gamecolors::ObsBoost()
    };

    for (int i = 0; i < 6; ++i)
    {
        float angle = static_cast<float>(i) * (6.2832f / 6.0f);

        Obstacle o;
        o.x          = cx + math::NkCos(angle) * ringRadius - 10.0f;
        o.y          = cy + math::NkSin(angle) * ringRadius - 10.0f;
        o.w          = 20.0f;
        o.h          = 20.0f;
        o.effect     = ringEffects[i];
        o.color      = ringColors[i];
        o.glowColor  = { ringColors[i].r, ringColors[i].g, ringColors[i].b, 70 };
        o.speedMult  = 1.3f;
        o.fixedAngle = 0.35f;
        o.angleNoise = 0.4f;
        o.pulseSpeed = 2.0f + static_cast<float>(i) * 0.3f;

        // Velocite orbitale perpendiculaire au rayon
        o.vx = -math::NkSin(angle) * orbitSpeed;
        o.vy =  math::NkCos(angle) * orbitSpeed;

        // Phase alternee pour les obstacles pairs
        if (i % 2 == 0)
        {
            o.hasPhase      = true;
            o.phaseDuration = 1.0f;
            o.phaseCooldown = 2.0f;
            o.phaseTimer    = static_cast<float>(i) * 0.4f;
        }

        mObstacles.push_back(o);
    }

    // ── Paire de portails aux coins haut-gauche et bas-droit ──────────────────
    int portalId1 = static_cast<int>(mObstacles.size());

    {
        Obstacle o;
        o.x         = W * 0.22f;
        o.y         = H * 0.22f;
        o.w         = 20.0f;
        o.h         = 20.0f;
        o.effect    = ObstacleEffect::Portal;
        o.pairedId  = portalId1 + 1;
        o.color     = gamecolors::ObsPortal();
        o.glowColor = { 255, 60, 200, 80 };
        o.pulseSpeed = 5.0f;
        mObstacles.push_back(o);
    }
    {
        Obstacle o;
        o.x         = W * 0.72f;
        o.y         = H * 0.72f;
        o.w         = 20.0f;
        o.h         = 20.0f;
        o.effect    = ObstacleEffect::Portal;
        o.pairedId  = portalId1;
        o.color     = gamecolors::ObsPortal();
        o.glowColor = { 255, 60, 200, 80 };
        o.pulseSpeed = 5.0f;
        mObstacles.push_back(o);
    }
}


// =============================================================================
// Section 4 : Update (dispatcher principal + navigation menus)
// =============================================================================

// ── PongGame::UIScale ─────────────────────────────────────────────────────────
float PongGame::UIScale() const noexcept
{
    float W = static_cast<float>(mRenderer.Width());
    float H = static_cast<float>(mRenderer.Height());
    float s = math::NkMin(W / 1280.f, H / 720.f);
    return math::NkMax(s, 0.4f);
}

// ── PongGame::Update ──────────────────────────────────────────────────────────
// Point d\'entree principal de la logique de jeu appelee chaque frame.
// Dispatche vers la methode de mise a jour appropriee selon l\'etat courant.
// L\'anti-repetition (mInputRepeat) empeche les navigations trop rapides.
void PongGame::Update(float dt,
                      bool  upHeld,
                      bool  downHeld,
                      bool  enterPressed,
                      bool  escapePressed,
                      bool  leftPressed,
                      bool  rightPressed,
                      int   mouseX,
                      int   mouseY,
                      bool  mouseClick)
{
    // Stocker l\'etat du pointeur pour les menus
    mMouseX     = mouseX;
    mMouseY     = mouseY;
    mMouseClick = mouseClick;

    // Incrementer le temps global
    mTime += dt;

    // Decrementer le compteur d\'anti-repetition
    if (mInputRepeat > 0.0f)
    {
        mInputRepeat -= dt;
    }

    // Calculer si une navigation est autorisee ce frame
    bool navUp    = upHeld    && mInputRepeat <= 0.0f;
    bool navDown  = downHeld  && mInputRepeat <= 0.0f;
    bool navLeft  = leftPressed;
    bool navRight = rightPressed;

    // Reinitialiser le compteur si une navigation vient d\'avoir lieu
    if (navUp || navDown || navLeft || navRight)
    {
        mInputRepeat = 0.18f;
    }

    // Dispatcher selon l\'etat courant
    switch (mState)
    {
    case GameState::SplashScreen:
        UpdateSplash(dt, enterPressed || escapePressed);
        break;

    case GameState::MainMenu:
        UpdateMainMenu(navUp, navDown, enterPressed, escapePressed);
        break;

    case GameState::SelectDifficulty:
        UpdateDiffSelect(navUp, navDown, enterPressed, escapePressed);
        break;

    case GameState::SelectObstacles:
        UpdateObsSelect(navUp, navDown, enterPressed, escapePressed,
                        navLeft, navRight);
        break;

    case GameState::Playing:
        UpdateObstacles(dt);
        UpdateBall(dt);
        UpdatePaddles(dt, upHeld, downHeld);
        UpdateParticles(dt);
        // Decroissance de l\'ecran-secousse
        if (mShake > 0.0f)
        {
            mShake -= dt * 8.0f;
        }
        if (mShake < 0.0f)
        {
            mShake = 0.0f;
        }
        // Decroissance du flash global
        if (mFlashAlpha > 0.0f)
        {
            mFlashAlpha -= dt * 3.0f;
        }
        // Touche Echap ou P -> pause
        if (escapePressed)
        {
            mState    = GameState::Paused;
            mPauseSel = 0;
        }
        break;

    case GameState::Paused:
        UpdatePauseMenu(enterPressed, escapePressed, navUp, navDown);
        break;

    case GameState::GoalFlash:
        mGoalTimer -= dt;
        UpdateParticles(dt);
        if (mFlashAlpha > 0.0f)
        {
            mFlashAlpha -= dt * 2.5f;
        }
        if (mGoalTimer <= 0.0f)
        {
            bool playerWon = mPlayer.score >= mSettings.maxScore;
            bool aiWon     = mAI.score     >= mSettings.maxScore;
            if (playerWon || aiWon)
            {
                mState = GameState::GameOver;
            }
            else
            {
                // Le perdant sert (mLastScorer==1 -> IA a marque -> joueur sert)
                ResetBall(mLastScorer == 1);
                mState = GameState::Playing;
            }
        }
        break;

    case GameState::GameOver:
        UpdateParticles(dt);
        if (enterPressed)
        {
            // Rejouer avec les memes parametres
            mPlayer.score = 0;
            mAI.score     = 0;
            SpawnObstacles();
            ResetBall(true);
            mParticles.clear();
            mState = GameState::Playing;
        }
        if (escapePressed)
        {
            // Retourner au menu principal
            mState = GameState::MainMenu;
        }
        break;
    }
}

// ── UpdateMainMenu ────────────────────────────────────────────────────────────
// Items : 0=JOUER, 1=DIFFICULTE, 2=OBSTACLES, 3=QUITTER
// La navigation est cyclique (4 items).
void PongGame::UpdateMainMenu(bool up, bool down, bool enter, bool esc)
{
    // Navigation verticale cyclique
    if (up)
    {
        mMainMenuSel = (mMainMenuSel + 3) % 4;
    }
    if (down)
    {
        mMainMenuSel = (mMainMenuSel + 1) % 4;
    }

    // Navigation et clic souris / tactile
    if (mMouseX >= 0 && mMouseY >= 0)
    {
        float S   = UIScale();
        int   W   = static_cast<int>(mRenderer.Width());
        int   H   = static_cast<int>(mRenderer.Height());
        int   cx  = W / 2;
        int   cy  = H / 2;
        int   pw  = static_cast<int>(320.f * S + 0.5f);
        int   mx  = cx - pw / 2;
        int   my  = cy - static_cast<int>(60.f * S + 0.5f);
        int   bh  = static_cast<int>(44.f * S + 0.5f);
        int   bw  = pw - static_cast<int>(24.f * S + 0.5f);
        int   bx  = mx + static_cast<int>(12.f * S + 0.5f);
        int   by  = my + static_cast<int>(14.f * S + 0.5f);

        for (int i = 0; i < 4; ++i)
        {
            int btnY = by + i * (bh + static_cast<int>(4.f * S + 0.5f));
            if (mMouseX >= bx && mMouseX < bx + bw &&
                mMouseY >= btnY && mMouseY < btnY + bh)
            {
                mMainMenuSel = i;
                if (mMouseClick)
                {
                    enter = true;
                }
                break;
            }
        }
    }

    // Confirmation
    if (enter)
    {
        if (mMainMenuSel == 0)
        {
            StartGame();
        }
        else if (mMainMenuSel == 1)
        {
            mState = GameState::SelectDifficulty;
        }
        else if (mMainMenuSel == 2)
        {
            mState = GameState::SelectObstacles;
        }
        // Item 3 = QUITTER : gere par Apps.cpp via GetState() si necessaire
    }

    (void)esc;
}

// ── UpdateDiffSelect ──────────────────────────────────────────────────────────
// 4 niveaux de difficulte (0=Facile .. 3=Expert).
// Entree confirme et retourne au menu principal.
// Echap annule et retourne au menu principal.
void PongGame::UpdateDiffSelect(bool up, bool down, bool enter, bool esc)
{
    // Navigation cyclique sur 4 niveaux
    if (up)
    {
        mDiffSel = (mDiffSel + 3) % 4;
    }
    if (down)
    {
        mDiffSel = (mDiffSel + 1) % 4;
    }

    // Navigation et clic souris / tactile
    if (mMouseX >= 0 && mMouseY >= 0)
    {
        float S  = UIScale();
        int   W  = static_cast<int>(mRenderer.Width());
        int   H  = static_cast<int>(mRenderer.Height());
        int   cx = W / 2;
        int   cy = H / 2;
        int   pw = static_cast<int>(520.f * S + 0.5f);
        int   px = cx - pw / 2;
        int   py = cy - static_cast<int>(60.f * S + 0.5f);
        int   bh = static_cast<int>(56.f * S + 0.5f);
        int   bw = pw - static_cast<int>(24.f * S + 0.5f);
        int   bx = px + static_cast<int>(12.f * S + 0.5f);
        int   by = py + static_cast<int>(12.f * S + 0.5f);
        int   btnH = static_cast<int>(36.f * S + 0.5f);

        for (int i = 0; i < 4; ++i)
        {
            int btnY = by + i * (bh + static_cast<int>(4.f * S + 0.5f));
            if (mMouseX >= bx && mMouseX < bx + bw &&
                mMouseY >= btnY && mMouseY < btnY + btnH)
            {
                mDiffSel = i;
                if (mMouseClick)
                {
                    enter = true;
                }
                break;
            }
        }
    }

    // Confirmer la difficulte selectionnee
    if (enter)
    {
        mSettings.difficulty = static_cast<AIDifficulty>(mDiffSel);
        mState               = GameState::MainMenu;
    }

    // Annuler et retourner au menu
    if (esc)
    {
        mState = GameState::MainMenu;
    }
}

// ── UpdateObsSelect ───────────────────────────────────────────────────────────
// 6 presets d\'obstacles (0=None .. 5=Boss).
// Entree confirme et retourne au menu principal.
// Echap annule et retourne au menu principal.
void PongGame::UpdateObsSelect(bool up, bool down, bool enter, bool esc,
                               bool left, bool right)
{
    (void)left;
    (void)right;

    // Navigation cyclique sur 6 presets
    if (up)
    {
        mObsSel = (mObsSel + 5) % 6;
    }
    if (down)
    {
        mObsSel = (mObsSel + 1) % 6;
    }

    // Navigation et clic souris / tactile (panneau gauche)
    if (mMouseX >= 0 && mMouseY >= 0)
    {
        float S   = UIScale();
        int   W   = static_cast<int>(mRenderer.Width());
        int   H   = static_cast<int>(mRenderer.Height());
        int   cx  = W / 2;
        int   cy  = H / 2;
        int   lpw = static_cast<int>(260.f * S + 0.5f);
        int   lpx = cx - lpw - static_cast<int>(10.f * S + 0.5f);
        int   lpy = cy - static_cast<int>(60.f * S + 0.5f);
        int   bh  = static_cast<int>(44.f * S + 0.5f);
        int   bw  = lpw - static_cast<int>(16.f * S + 0.5f);
        int   bx  = lpx + static_cast<int>(8.f * S + 0.5f);
        int   by  = lpy + static_cast<int>(10.f * S + 0.5f);

        for (int i = 0; i < 6; ++i)
        {
            int btnY = by + i * (bh + static_cast<int>(4.f * S + 0.5f));
            if (mMouseX >= bx && mMouseX < bx + bw &&
                mMouseY >= btnY && mMouseY < btnY + bh)
            {
                mObsSel = i;
                if (mMouseClick)
                {
                    enter = true;
                }
                break;
            }
        }
    }

    // Confirmer le preset selectionne
    if (enter)
    {
        mSettings.obstaclePreset = static_cast<ObstaclePreset>(mObsSel);
        mState                   = GameState::MainMenu;
    }

    // Annuler et retourner au menu
    if (esc)
    {
        mState = GameState::MainMenu;
    }
}

// ── UpdatePauseMenu ───────────────────────────────────────────────────────────
// 2 options : 0=Reprendre, 1=Menu principal.
// Echap reprend toujours directement la partie.
void PongGame::UpdatePauseMenu(bool enter, bool esc, bool navUp, bool navDown)
{
    // Echap reprend directement sans action supplementaire
    if (esc)
    {
        mState = GameState::Playing;
        return;
    }

    // Navigation clavier haut/bas
    if (navUp   && mPauseSel > 0) { --mPauseSel; }
    if (navDown && mPauseSel < 1) { ++mPauseSel; }

    // Navigation et clic souris / tactile
    if (mMouseX >= 0 && mMouseY >= 0)
    {
        float S    = UIScale();
        int   W    = static_cast<int>(mRenderer.Width());
        int   H    = static_cast<int>(mRenderer.Height());
        int   cx   = W / 2;
        int   cy   = H / 2;
        int   pw   = static_cast<int>(260.f * S + 0.5f);
        int   px   = cx - pw / 2;
        int   py   = cy - static_cast<int>(28.f * S + 0.5f);
        int   bw   = pw - static_cast<int>(20.f * S + 0.5f);
        int   bx   = px + static_cast<int>(10.f * S + 0.5f);
        int   bh   = static_cast<int>(48.f * S + 0.5f);
        int   btn1Y = py + static_cast<int>(10.f * S + 0.5f);
        int   btn2Y = py + static_cast<int>(64.f * S + 0.5f);

        if (mMouseX >= bx && mMouseX < bx + bw)
        {
            if (mMouseY >= btn1Y && mMouseY < btn1Y + bh)
            {
                mPauseSel = 0;
                if (mMouseClick) { enter = true; }
            }
            else if (mMouseY >= btn2Y && mMouseY < btn2Y + bh)
            {
                mPauseSel = 1;
                if (mMouseClick) { enter = true; }
            }
        }
    }

    // Entree execute l\'action selectionnee
    if (enter)
    {
        if (mPauseSel == 0)
        {
            // Reprendre la partie
            mState = GameState::Playing;
        }
        else
        {
            // Retourner au menu principal
            mPlayer.score = 0;
            mAI.score     = 0;
            SpawnObstacles();
            ResetBall(true);
            mParticles.clear();
            mState = GameState::MainMenu;
        }
    }
}


// =============================================================================
// Section 5 : UpdateBall / UpdatePaddles / UpdateObstacles / Particles
// =============================================================================

// ── PongGame::UpdateBall ──────────────────────────────────────────────────────
// Avance la physique de la balle et gere :
//   - Rebonds sur les murs haut et bas
//   - Detection des buts (sortie par la gauche ou la droite)
//   - Force magnetique des obstacles Magnetic
//   - Collisions avec les raquettes et les obstacles
void PongGame::UpdateBall(float dt)
{
    mBall.Update(dt);

    float W = static_cast<float>(mRenderer.Width());
    float H = static_cast<float>(mRenderer.Height());

    // ── Rebond sur le mur haut ────────────────────────────────────────────────
    if (mBall.y - mBall.radius < 0.0f)
    {
        mBall.y  = mBall.radius;
        mBall.vy = math::NkAbs(mBall.vy);
        SpawnParticles(mBall.x, 0.0f, gamecolors::BallCol(), 5, 70.0f);
    }

    // ── Rebond sur le mur bas ─────────────────────────────────────────────────
    if (mBall.y + mBall.radius > H)
    {
        mBall.y  = H - mBall.radius;
        mBall.vy = -math::NkAbs(mBall.vy);
        SpawnParticles(mBall.x, H, gamecolors::BallCol(), 5, 70.0f);
    }

    // ── But cote gauche (IA marque) ───────────────────────────────────────────
    if (mBall.x < -mBall.radius)
    {
        ++mAI.score;
        mLastScorer  = 1;
        mShake       = 0.4f;
        mFlashAlpha  = 1.0f;
        mFlashColor  = gamecolors::AI();
        mGoalTimer   = 1.8f;
        mState       = GameState::GoalFlash;
        SpawnParticles(0.0f, mBall.y, gamecolors::AI(), 35, 200.0f, true);
        return;
    }

    // ── But cote droit (joueur marque) ────────────────────────────────────────
    if (mBall.x > W + mBall.radius)
    {
        ++mPlayer.score;
        mLastScorer  = 0;
        mShake       = 0.4f;
        mFlashAlpha  = 1.0f;
        mFlashColor  = gamecolors::Player();
        mGoalTimer   = 1.8f;
        mState       = GameState::GoalFlash;
        SpawnParticles(W, mBall.y, gamecolors::Player(), 35, 200.0f, true);
        return;
    }

    // ── Force magnetique des obstacles Magnetic ───────────────────────────────
    for (auto& obs : mObstacles)
    {
        // Seuls les obstacles Magnetic en etat actif (non-phasing) exercent une force
        if (obs.effect != ObstacleEffect::Magnetic)
        {
            continue;
        }
        if (obs.isPhasing)
        {
            continue;
        }

        float ocx  = obs.x + obs.w * 0.5f;
        float ocy  = obs.y + obs.h * 0.5f;
        float dx   = ocx - mBall.x;
        float dy   = ocy - mBall.y;
        float dist = pongmath::Length(dx, dy);
        float range = 190.0f;

        // N\'appliquer la force que dans la portee et si la balle n\'est pas dessus
        if (dist < range && dist > 4.0f)
        {
            float t = 1.0f - dist / range;
            float f = obs.magnetForce * t * t * dt;
            pongmath::Normalize(dx, dy);
            mBall.vx += dx * f;
            mBall.vy += dy * f;
            mBall.speed = pongmath::Length(mBall.vx, mBall.vy);
            mBall.ClampSpeed();
        }
    }

    // ── Collisions avec les raquettes ─────────────────────────────────────────
    BallVsPaddle(mPlayer);
    BallVsPaddle(mAI);

    // ── Collisions avec les obstacles ─────────────────────────────────────────
    for (int i = 0; i < static_cast<int>(mObstacles.size()); ++i)
    {
        BallVsObstacle(i);
    }
}

// ── PongGame::UpdatePaddles ───────────────────────────────────────────────────
// Joueur : deplacement direct via les touches haut/bas.
// IA     : poursuite de la balle avec reaction, erreur et prediction
//          variables selon la difficulte.
void PongGame::UpdatePaddles(float dt, bool upHeld, bool downHeld)
{
    float H = static_cast<float>(mRenderer.Height());

    // ── Raquette du joueur ────────────────────────────────────────────────────
    if (upHeld)
    {
        mPlayer.MoveUp(dt);
    }
    if (downHeld)
    {
        mPlayer.MoveDown(dt);
    }
    mPlayer.ClampToField(H);

    // ── Raquette de l\'IA ──────────────────────────────────────────────────────
    // Calcul du temps de prediction selon la difficulte
    float predict = 0.0f;
    if (mSettings.difficulty == AIDifficulty::Hard)
    {
        predict = 0.09f;
    }
    else if (mSettings.difficulty == AIDifficulty::Expert)
    {
        predict = 0.16f;
    }

    // Position future estimee de la balle (prediction)
    float futureBy = mBall.y + mBall.vy * predict;

    // Cible verticale de l\'IA (centre de la raquette sur la balle) + erreur
    float targetY   = futureBy - mAI.h * 0.5f + mAI.aiError;
    float diff      = targetY - mAI.y;
    float maxMove   = mAI.aiSpeed * dt * mAI.aiReaction;

    // Avancer vers la cible sans depasser maxMove
    if (math::NkAbs(diff) > maxMove)
    {
        mAI.y += (diff > 0.0f) ? maxMove : -maxMove;
    }
    else
    {
        mAI.y = targetY;
    }

    // Rafraichir l\'erreur IA periodiquement selon la difficulte
    mAiErrTimer += dt;

    float refreshRate = 0.8f;
    float errRange    = 72.0f;

    if (mSettings.difficulty == AIDifficulty::Medium)
    {
        refreshRate = 1.2f;
        errRange    = 38.0f;
    }
    else if (mSettings.difficulty == AIDifficulty::Hard)
    {
        refreshRate = 1.8f;
        errRange    = 16.0f;
    }
    else if (mSettings.difficulty == AIDifficulty::Expert)
    {
        refreshRate = 2.5f;
        errRange    = 5.0f;
    }

    if (mAiErrTimer > refreshRate)
    {
        mAiErrTimer  = 0.0f;
        mAI.aiError  = pongmath::RandF(mRng, -errRange, errRange);

        // En mode Facile, l\'IA commet parfois des erreurs grossieres
        if (mSettings.difficulty == AIDifficulty::Easy)
        {
            float roll = pongmath::RandF(mRng, 0.0f, 1.0f);
            if (roll > 0.7f)
            {
                mAI.aiError += pongmath::RandF(mRng, -120.0f, 120.0f);
            }
        }
    }

    mAI.ClampToField(H);
}

// ── PongGame::UpdateObstacles ─────────────────────────────────────────────────
// Appelle Update() sur chaque obstacle pour gerer deplacement et phase.
void PongGame::UpdateObstacles(float dt)
{
    float W = static_cast<float>(mRenderer.Width());
    float H = static_cast<float>(mRenderer.Height());

    for (auto& o : mObstacles)
    {
        o.Update(dt, W, H);
    }
}

// ── PongGame::UpdateParticles ─────────────────────────────────────────────────
// Avance toutes les particules (deplacement, gravite, vieillissement).
// Supprime automatiquement les particules dont la duree de vie est expiree.
void PongGame::UpdateParticles(float dt)
{
    for (auto& p : mParticles)
    {
        p.x    += p.vx * dt;
        p.y    += p.vy * dt;
        p.vy   += 120.0f * dt;
        p.life -= dt;
        p.size  = math::NkMax(0.0f, p.size * 0.97f);
    }

    // Supprimer les particules mortes
    mParticles.erase(
        std::remove_if(
            mParticles.begin(),
            mParticles.end(),
            [](const Particle& p) { return p.life <= 0.0f; }
        ),
        mParticles.end()
    );
}

// ── PongGame::SpawnParticles ──────────────────────────────────────────────────
// Emet n particules depuis la position (px, py).
// Direction aleatoire uniforme sur 360 degres.
// Vitesse aleatoire dans [0.3*spd, spd].
void PongGame::SpawnParticles(float px, float py, math::NkColor c,
                               int n, float spd, bool add)
{
    for (int i = 0; i < n; ++i)
    {
        Particle p;
        float angle = pongmath::RandF(mRng, 0.0f, 6.2832f);
        float s     = pongmath::RandF(mRng, spd * 0.3f, spd);
        p.x       = px;
        p.y       = py;
        p.vx      = math::NkCos(angle) * s;
        p.vy      = math::NkSin(angle) * s;
        p.life    = pongmath::RandF(mRng, 0.4f, 1.0f);
        p.maxLife = p.life;
        p.size    = pongmath::RandF(mRng, 2.0f, 5.0f);
        p.color   = c;
        p.additive = add;
        mParticles.push_back(p);
    }
}


// =============================================================================
// Section 6 : Collisions
// =============================================================================

// ── PongGame::BallVsPaddle ────────────────────────────────────────────────────
// Teste la collision balle-raquette via une distance point-AABB.
// Si collision : repositionne la balle, calcule l\'angle de rebond selon
// la position d\'impact sur la raquette, augmente legerement la vitesse.
bool PongGame::BallVsPaddle(Paddle& p)
{
    // Point le plus proche de la raquette par rapport au centre de la balle
    float cx = pongmath::Clamp(mBall.x, p.x, p.x + p.w);
    float cy = pongmath::Clamp(mBall.y, p.y, p.y + p.h);

    // Distance entre le point le plus proche et le centre de la balle
    float dx = mBall.x - cx;
    float dy = mBall.y - cy;

    // Pas de collision si la distance est superieure au rayon
    if (dx * dx + dy * dy >= mBall.radius * mBall.radius)
    {
        return false;
    }

    // Repousser la balle hors de la raquette
    if (p.isLeft)
    {
        mBall.x  = p.x + p.w + mBall.radius;
        mBall.vx = math::NkAbs(mBall.vx);
    }
    else
    {
        mBall.x  = p.x - mBall.radius;
        mBall.vx = -math::NkAbs(mBall.vx);
    }

    // Augmenter legerement la vitesse a chaque contact (+6%, plafonne)
    mBall.speed = math::NkMin(mBall.speed * 1.06f, Ball::MAX_SPEED);

    // Calculer l\'angle de rebond selon la position d\'impact sur la raquette
    float hitY  = mBall.y - (p.y + p.h * 0.5f);
    float relY  = hitY / (p.h * 0.5f);
    float angle = relY * 1.1f;
    float dir   = p.isLeft ? 1.0f : -1.0f;

    mBall.vx = dir * mBall.speed * math::NkCos(angle);
    mBall.vy = mBall.speed * math::NkSin(angle);
    mBall.ClampSpeed();

    // Generer des particules d\'impact
    float hitX = p.isLeft ? p.x + p.w : p.x;
    SpawnParticles(hitX, mBall.y, p.color, 8, 100.0f);
    mShake += 0.07f;

    return true;
}

// ── PongGame::BallVsObstacle ──────────────────────────────────────────────────
// Teste la collision balle-obstacle via une distance point-AABB.
// Gere l\'etat de phase (traversee) et appelle ApplyObstacleEffect si collision.
bool PongGame::BallVsObstacle(int idx)
{
    auto& obs = mObstacles[idx];

    // Point le plus proche de l\'obstacle par rapport au centre de la balle
    float cx  = pongmath::Clamp(mBall.x, obs.x, obs.x + obs.w);
    float cy  = pongmath::Clamp(mBall.y, obs.y, obs.y + obs.h);
    float dx  = mBall.x - cx;
    float dy  = mBall.y - cy;
    float d2  = dx * dx + dy * dy;

    // Pas de collision si hors portee
    if (d2 >= mBall.radius * mBall.radius)
    {
        return false;
    }

    // Gestion de la phase : si l\'obstacle est traversable et la balle n\'est pas en phase
    if (obs.isPhasing && !mBall.isPhasing)
    {
        mBall.isPhasing  = true;
        mBall.phaseTimer = 0.3f;
        return false;
    }

    // Si la balle est en phase, elle traverse cet obstacle
    if (mBall.isPhasing)
    {
        return false;
    }

    // Calculer la normale de collision (direction de separation)
    float dist = math::NkSqrt(d2);
    float nx   = (dist > 0.001f) ? dx / dist : 0.0f;
    float ny   = (dist > 0.001f) ? dy / dist : -1.0f;

    // Repousser la balle hors de l\'obstacle
    float overlap = mBall.radius - dist + 0.5f;
    mBall.x += nx * overlap;
    mBall.y += ny * overlap;

    // Reflexion de la velocite selon la normale
    float dot = pongmath::Dot(mBall.vx, mBall.vy, nx, ny);
    mBall.vx -= 2.0f * dot * nx;
    mBall.vy -= 2.0f * dot * ny;

    // Appliquer l\'effet special de l\'obstacle
    ApplyObstacleEffect(idx);

    // Declencher les effets visuels
    obs.hitFlash = 1.0f;
    SpawnParticles(mBall.x, mBall.y, obs.color, 10, 90.0f);
    mShake += 0.08f;

    return true;
}

// ── PongGame::ApplyObstacleEffect ─────────────────────────────────────────────
// Applique l\'effet special de l\'obstacle apres une collision confirmee.
// Chaque cas correspond a une valeur d\'ObstacleEffect.
void PongGame::ApplyObstacleEffect(int idx)
{
    auto& obs = mObstacles[idx];

    switch (obs.effect)
    {
    // Acceleration : vitesse *= speedMult, plafonnee a MAX_SPEED
    case ObstacleEffect::SpeedBoost:
        mBall.speed = math::NkMin(mBall.speed * obs.speedMult, Ball::MAX_SPEED);
        mBall.ClampSpeed();
        break;

    // Deceleration : vitesse /= speedMult, plancher a BASE_SPEED*0.6
    case ObstacleEffect::SpeedReduce:
        mBall.speed = math::NkMax(mBall.speed / obs.speedMult,
                               Ball::BASE_SPEED * 0.6f);
        mBall.ClampSpeed();
        break;

    // Redirection : rotation du vecteur vitesse de fixedAngle radians
    case ObstacleEffect::Redirect:
    {
        float ca  = math::NkCos(obs.fixedAngle);
        float sa  = math::NkSin(obs.fixedAngle);
        float nvx = mBall.vx * ca - mBall.vy * sa;
        float nvy = mBall.vx * sa + mBall.vy * ca;
        mBall.vx = nvx;
        mBall.vy = nvy;
        mBall.ClampSpeed();
        break;
    }

    // Deflexion aleatoire : rotation par un angle aleatoire dans [-noise, +noise]
    case ObstacleEffect::RandomDeflect:
    {
        float noise = pongmath::RandF(mRng, -obs.angleNoise, obs.angleNoise);
        float ca    = math::NkCos(noise);
        float sa    = math::NkSin(noise);
        float nvx   = mBall.vx * ca - mBall.vy * sa;
        float nvy   = mBall.vx * sa + mBall.vy * ca;
        mBall.vx = nvx;
        mBall.vy = nvy;
        mBall.ClampSpeed();
        break;
    }

    // Magnetique : force appliquee chaque frame dans UpdateBall, pas d\'effet au contact
    case ObstacleEffect::Magnetic:
        break;

    // Portail : teleporte la balle vers l\'obstacle partenaire
    case ObstacleEffect::Portal:
        if (obs.pairedId >= 0 &&
            obs.pairedId < static_cast<int>(mObstacles.size()))
        {
            auto& target = mObstacles[obs.pairedId];
            mBall.x      = target.x + target.w * 0.5f;
            mBall.y      = target.y + target.h * 0.5f;
            // Phase breve pour eviter un reteleportage immediat
            mBall.isPhasing  = true;
            mBall.phaseTimer = 0.6f;
            SpawnParticles(mBall.x, mBall.y, obs.color, 20, 150.0f, true);
        }
        break;

    // Phase et None : pas d\'effet supplementaire au contact
    case ObstacleEffect::Phase:
    case ObstacleEffect::None:
    default:
        break;
    }
}


// =============================================================================
// Section 7 : Render (dispatcher + couches visuelles)
// =============================================================================

// ── PongGame::Render ──────────────────────────────────────────────────────────
// Point d\'entree principal du rendu.
// Dessine toutes les couches dans l\'ordre correct (fond -> jeu -> UI).
// L\'overlay de flash global est applique en derniere couche.
void PongGame::Render()
{
    // Le splash screen prend tout l\'ecran : rendu independant
    if (mState == GameState::SplashScreen)
    {
        RenderSplash();
        return;
    }

    // Toujours dessiner le fond (tous les etats)
    RenderBackground();

    // Couches de jeu (visibles pendant Playing, Paused, GoalFlash, GameOver)
    bool gameVisible = (mState == GameState::Playing   ||
                        mState == GameState::Paused     ||
                        mState == GameState::GoalFlash  ||
                        mState == GameState::GameOver);

    if (gameVisible)
    {
        RenderObstacles();
        RenderPaddles();
        RenderBall();
        RenderParticles();
        RenderHUD();
    }

    // Overlays specifiques a l\'etat courant
    switch (mState)
    {
    case GameState::MainMenu:
        RenderMainMenu();
        break;
    case GameState::SelectDifficulty:
        RenderDiffSelect();
        break;
    case GameState::SelectObstacles:
        RenderObsSelect();
        break;
    case GameState::Paused:
        RenderPauseOverlay();
        break;
    case GameState::GoalFlash:
        RenderGoalFlash();
        break;
    case GameState::GameOver:
        RenderGameOver();
        break;
    default:
        break;
    }

    // ── Boutons tactiles Android ─────────────────────────────────────────────
    if (mShowTouchButtons)
    {
        RenderTouchButtons();
    }

    // ── Flash global (sur toutes les couches, en fondu) ───────────────────────
    if (mFlashAlpha > 0.01f)
    {
        uint32_t W = mRenderer.Width();
        uint32_t H = mRenderer.Height();
        math::NkColor fc = AlphaF(mFlashColor, mFlashAlpha * 0.32f);

        for (uint32_t fy = 0; fy < H; fy += 2)
        {
            for (uint32_t fx = 0; fx < W; fx += 2)
            {
                math::NkColor dst = mRenderer.GetPixel(fx, fy);
                mRenderer.SetPixel(fx, fy, Blend(dst, fc));
            }
        }
    }
}

// ── PongGame::RenderBackground ────────────────────────────────────────────────
// Dessine le fond de l\'ecran : couleur de base, etoiles scintillantes,
// grille decorative et ligne centrale en pointilles.
void PongGame::RenderBackground()
{
    mRenderer.Clear(gamecolors::BG());

    uint32_t W = mRenderer.Width();
    uint32_t H = mRenderer.Height();

    // Etoiles scintillantes animees
    DrawStars(mTime);

    // Grille decorative horizontale et verticale
    math::NkColor gc = gamecolors::Grid();
    for (uint32_t gx = 0; gx < W; gx += 48)
    {
        mRasterizer.DrawLine(gx, 0, gx, H, gc);
    }
    for (uint32_t gy = 0; gy < H; gy += 48)
    {
        mRasterizer.DrawLine(0, gy, W, gy, gc);
    }

    // Ligne centrale en pointilles
    mRasterizer.DrawDashedLine(W / 2, 0, W / 2, H, gamecolors::CenterLine(), 12);
}

// ── PongGame::RenderPaddles ───────────────────────────────────────────────────
// Dessine chaque raquette avec : lueur de fond, corps plein, contour brillant.
void PongGame::RenderPaddles()
{
    // Fonction lambda locale pour dessiner une raquette
    auto drawPaddle = [&](const Paddle& p)
    {
        int px = static_cast<int>(p.x);
        int py = static_cast<int>(p.y);
        int pw = static_cast<int>(p.w);
        int ph = static_cast<int>(p.h);

        // Lueur de fond centree sur la raquette
        mRasterizer.DrawGlow(px + pw / 2, py + ph / 2,
                             pw + 18, AlphaF(p.color, 0.28f), 0.7f);

        // Corps plein de la raquette
        mRasterizer.FillRect(px, py, pw, ph, p.color);

        // Contour lumineux pour visibility
        mRasterizer.DrawRect(px - 1, py - 1, pw + 2, ph + 2,
                             AlphaF(gamecolors::White(), 0.55f));
    };

    drawPaddle(mPlayer);
    drawPaddle(mAI);
}

// ── PongGame::RenderBall ──────────────────────────────────────────────────────
// Dessine la balle avec : trail decroissant, lueur de charge, corps plein.
// En etat de phase : affiche un cercle en pointilles pour signaler la traversee.
void PongGame::RenderBall()
{
    int bx = static_cast<int>(mBall.x);
    int by = static_cast<int>(mBall.y);
    int br = static_cast<int>(mBall.radius);

    // ── Trail visuel (18 points historiques) ─────────────────────────────────
    for (int i = 1; i < Ball::TRAIL_LEN; ++i)
    {
        int   ti = (mBall.trailHead - i + Ball::TRAIL_LEN) % Ball::TRAIL_LEN;
        float t  = 1.0f - static_cast<float>(i) / Ball::TRAIL_LEN;
        int   tx = static_cast<int>(mBall.trailX[ti]);
        int   ty = static_cast<int>(mBall.trailY[ti]);
        int   tr = math::NkMax(1, static_cast<int>(mBall.radius * t * 0.7f));

        mRasterizer.DrawGlow(tx, ty, tr + 4,
                             AlphaF(gamecolors::TrailCol(), t * 0.5f), t * 0.45f);
        mRasterizer.FillCircle(tx, ty, tr,
                               AlphaF(gamecolors::TrailCol(), t * 0.55f));
    }

    // ── Lueur de charge (plus intense quand la balle est rapide) ─────────────
    float glowIntensity = 0.5f + mBall.chargeLevel * 0.5f;
    mRasterizer.DrawGlow(bx, by, br * 3,
                         AlphaF(mBall.CurrentColor(), glowIntensity * 0.6f),
                         1.0f + mBall.chargeLevel * 1.5f);

    // ── Corps de la balle ─────────────────────────────────────────────────────
    if (mBall.isPhasing)
    {
        // En phase : cercle en pointilles pour signaler la traversee
        for (int a = 0; a < 360; a += 24)
        {
            float rad = static_cast<float>(a) * 3.14159f / 180.0f;
            if ((a / 24) % 2 == 0)
            {
                int px2 = bx + static_cast<int>(math::NkCos(rad) * br);
                int py2 = by + static_cast<int>(math::NkSin(rad) * br);
                mRasterizer.BlendPixel(px2, py2,
                                       AlphaF(mBall.CurrentColor(), 0.8f));
            }
        }
    }
    else
    {
        // Normal : disque plein avec reflet speculaire
        mRasterizer.FillCircle(bx, by, br, mBall.CurrentColor());
        mRasterizer.FillCircle(bx - br / 3, by - br / 3,
                               math::NkMax(1, br / 3),
                               AlphaF(gamecolors::White(), 0.65f));
    }
}

// ── PongGame::RenderObstacles ─────────────────────────────────────────────────
// Dessine chaque obstacle avec : lueur pulsee, corps semi-transparent (phase),
// icone d\'effet et indicateur de phase.
void PongGame::RenderObstacles()
{
    for (const auto& obs : mObstacles)
    {
        int ox = static_cast<int>(obs.x);
        int oy = static_cast<int>(obs.y);
        int ow = static_cast<int>(obs.w);
        int oh = static_cast<int>(obs.h);

        // Pulsation visuelle (sinusoide lente)
        float pulse = 0.5f + 0.5f * math::NkSin(obs.pulseTimer);

        // Transparence quand l\'obstacle est en phase traversable
        float alpha = obs.isPhasing ? 0.28f : 1.0f;

        // Flash d\'impact : eclaircit la couleur apres un rebond
        math::NkColor col = obs.color;
        if (obs.hitFlash > 0.0f)
        {
            int flash = static_cast<int>(obs.hitFlash * 120);
            col.r = static_cast<uint8_t>(math::NkMin(255, col.r + flash));
            col.g = static_cast<uint8_t>(math::NkMin(255, col.g + flash));
            col.b = static_cast<uint8_t>(math::NkMin(255, col.b + flash));
        }
        math::NkColor drawCol = AlphaF(col, alpha);

        // Lueur pulsee autour de l\'obstacle
        mRasterizer.DrawGlow(ox + ow / 2, oy + oh / 2, ow + 12,
                             AlphaF(obs.color, pulse * alpha * 0.38f), 0.55f);

        // Corps plein de l\'obstacle
        mRasterizer.FillRect(ox, oy, ow, oh, drawCol);

        // Contour blanc
        mRasterizer.DrawRect(ox, oy, ow, oh,
                             AlphaF(gamecolors::White(), alpha * 0.5f));

        // Icone au centre indiquant le type d\'effet
        int icx = ox + ow / 2;
        int icy = oy + oh / 2;
        math::NkColor ic = AlphaF(gamecolors::White(), alpha * 0.8f);

        switch (obs.effect)
        {
        case ObstacleEffect::SpeedBoost:
            // Triangle pointe vers la droite (fleche d\'acceleration)
            mRasterizer.FillTriangle(icx - 3, icy - 3, icx + 4, icy,
                                     icx - 3, icy + 3, ic);
            break;
        case ObstacleEffect::SpeedReduce:
            // Carre (ralentissement)
            mRasterizer.FillRect(icx - 3, icy - 3, 6, 6, ic);
            break;
        case ObstacleEffect::Redirect:
            // Ligne diagonale (deflexion angulaire)
            mRasterizer.DrawLine(icx - 3, icy + 3, icx + 3, icy - 3, ic);
            break;
        case ObstacleEffect::RandomDeflect:
            // Point d\'interrogation (aleatoire)
            mRasterizer.DrawText(icx - 2, icy - 3, "?", ic, 1);
            break;
        case ObstacleEffect::Magnetic:
            // Disque plein (aimant)
            mRasterizer.FillCircle(icx, icy, 3, ic);
            break;
        case ObstacleEffect::Portal:
            // Arc tournant (teleportation)
            {
                float rotAngle = mTime * 4.0f;
                for (int k = 0; k < 6; ++k)
                {
                    float r  = rotAngle + static_cast<float>(k) * 1.047f;
                    int   px2 = icx + static_cast<int>(math::NkCos(r) * 4.5f);
                    int   py2 = icy + static_cast<int>(math::NkSin(r) * 4.5f);
                    mRasterizer.BlendPixel(px2, py2, ic);
                }
            }
            break;
        default:
            break;
        }

        // Indicateur de phase : anneau clignotant quand l\'obstacle est traversable
        if (obs.isPhasing)
        {
            float p2   = 0.5f + 0.5f * math::NkSin(mTime * 6.0f);
            int   ring = (ow > oh ? ow : oh) / 2 + 4;
            mRasterizer.DrawCircle(ox + ow / 2, oy + oh / 2, ring,
                                   AlphaF(obs.color, p2 * 0.85f));
        }
    }
}

// ── PongGame::RenderParticles ─────────────────────────────────────────────────
// Dessine chaque particule avec alpha proportionnel a la duree de vie restante.
// Mode additif : DrawGlow pour un effet de lueur lumineuse.
// Mode standard : FillCircle avec alpha.
void PongGame::RenderParticles()
{
    for (const auto& p : mParticles)
    {
        float       t   = p.life / p.maxLife;
        math::NkColor c = AlphaF(p.color, t);
        int         px  = static_cast<int>(p.x);
        int         py  = static_cast<int>(p.y);
        int         ps  = math::NkMax(1, static_cast<int>(p.size));

        if (p.additive)
        {
            mRasterizer.DrawGlow(px, py, ps + 3, c, t * 0.75f);
        }
        else
        {
            mRasterizer.FillCircle(px, py, ps, c);
        }
    }
}

// ── PongGame::RenderHUD ───────────────────────────────────────────────────────
// Affiche les informations de jeu en surimpression :
//   - Scores des deux joueurs (blanc, grand format)
//   - Indicateur de vitesse de la balle (centre, bas)
//   - Niveau de difficulte IA (bas droit)
//   - Rappel de la touche pause (bas gauche)
// Tout le texte est blanc pour une lisibilite maximale.
void PongGame::RenderHUD()
{
    uint32_t W = mRenderer.Width();
    uint32_t H = mRenderer.Height();

    // ── Score du joueur (gauche) ──────────────────────────────────────────────
    int scoreLeftX = static_cast<int>(W) / 4;
    mRasterizer.DrawGlow(scoreLeftX, 36, 32, AlphaF(gamecolors::Player(), 0.4f), 0.8f);
    mRasterizer.DrawNumber(scoreLeftX - 8, 18, mPlayer.score,
                           gamecolors::White(), 5);

    // ── Score de l\'IA (droite) ────────────────────────────────────────────────
    int scoreRightX = 3 * static_cast<int>(W) / 4;
    mRasterizer.DrawGlow(scoreRightX, 36, 32, AlphaF(gamecolors::AI(), 0.4f), 0.8f);
    mRasterizer.DrawNumber(scoreRightX - 8, 18, mAI.score,
                           gamecolors::White(), 5);

    // ── Vitesse de la balle (centre bas) ─────────────────────────────────────
    char spd[32];
    snprintf(spd, sizeof(spd), "%.0f px/s", mBall.speed);
    int tw = mRasterizer.TextWidth(spd, 1);
    int textCenterX = static_cast<int>(W) / 2 - tw / 2;
    mRasterizer.DrawText(textCenterX,
                         static_cast<int>(H) - 16,
                         spd,
                         gamecolors::White(),
                         1);

    // ── Niveau de difficulte IA (bas droit) ────────────────────────────────────
    const char* dn = AIDifficultyName(mSettings.difficulty);
    int dw = mRasterizer.TextWidth(dn, 1);
    int diffTextX = static_cast<int>(W) - dw - 8;
    mRasterizer.DrawText(diffTextX,
                         static_cast<int>(H) - 16,
                         dn,
                         gamecolors::White(),
                         1);

    // ── Rappel de la touche pause (bas gauche) ────────────────────────────────
    mRasterizer.DrawText(8,
                         static_cast<int>(H) - 16,
                         "[P] Pause",
                         gamecolors::White(),
                         1);
}


// =============================================================================
// Section 8 : Helpers UI (DrawPanel, DrawButton, DrawStars, DrawObstaclePreview)
// Description : Fonctions utilitaires pour le dessin des elements d\'interface.
//               Tout le texte affiche est BLANC pour garantir la lisibilite.
//               Tous les elements sont centres en fonction des dimensions reelles.
// =============================================================================

// ── PongGame::DrawPanel ───────────────────────────────────────────────────────
// Dessine un panneau rectangulaire avec fond semi-transparent (fusion alpha)
// et une bordure de largeur bw pixels.
void PongGame::DrawPanel(int x, int y, int w, int h,
                         math::NkColor fill, math::NkColor border, int bw)
{
    // Remplissage semi-transparent par fusion pixel par pixel
    for (int row = y; row < y + h; ++row)
    {
        for (int col = x; col < x + w; ++col)
        {
            math::NkColor dst = mRenderer.GetPixel(col, row);
            mRenderer.SetPixel(col, row, Blend(dst, fill));
        }
    }

    // Bordure multipassse pour l\'epaisseur
    for (int t = 0; t < bw; ++t)
    {
        mRasterizer.DrawRect(x + t, y + t, w - 2 * t, h - 2 * t, border);
    }
}

// ── PongGame::DrawButton ──────────────────────────────────────────────────────
// Dessine un bouton interactif avec fond colore, texte toujours blanc centre,
// et mise en evidence (lueur + bordure plus lumineuse) si selectionne.
// Le texte est TOUJOURS BLANC pour garantir la lisibilite maximale.
void PongGame::DrawButton(int x, int y, int w, int h,
                          const char* text, bool sel,
                          math::NkColor col, int ts)
{
    // Fond : plus opaque si selectionne
    math::NkColor fill   = AlphaF(col, sel ? 0.22f : 0.10f);
    math::NkColor border = sel ? gamecolors::SelBorder()
                               : AlphaF(gamecolors::PanelBorder(), 0.6f);

    // Dessiner le fond et la bordure
    DrawPanel(x, y, w, h, fill, border, sel ? 2 : 1);

    // Lueur supplementaire autour du bouton selectionne
    if (sel)
    {
        int lx = x - 2;
        int ly = y - 2;
        int lw = w + 4;
        int lh = h + 4;
        mRasterizer.DrawRect(lx, ly, lw, lh, AlphaF(gamecolors::SelBorder(), 0.4f));
        int lx2 = x - 3;
        int ly2 = y - 3;
        int lw2 = w + 6;
        int lh2 = h + 6;
        mRasterizer.DrawRect(lx2, ly2, lw2, lh2, AlphaF(gamecolors::SelBorder(), 0.15f));
    }

    // Texte TOUJOURS BLANC pour lisibilite maximale
    int tw = mRasterizer.TextWidth(text, ts);
    int tx = x + w / 2 - tw / 2;
    int ty = y + h / 2 - (5 * ts) / 2;

    mRasterizer.DrawText(tx, ty, text, gamecolors::White(), ts);
}

// ── PongGame::DrawStars ───────────────────────────────────────────────────────
// Dessine 80 etoiles statiques a positions determinees par seed 0xABCD.
// La luminosite de chaque etoile oscille sinusoidalement selon le temps.
void PongGame::DrawStars(float time)
{
    uint32_t W = mRenderer.Width();
    uint32_t H = mRenderer.Height();

    // RNG reproductible pour des positions d\'etoiles stables
    std::mt19937 srng(0xABCD);
    std::uniform_real_distribution<float> distX(0.0f, static_cast<float>(W));
    std::uniform_real_distribution<float> distY(0.0f, static_cast<float>(H));
    std::uniform_real_distribution<float> distS(0.0f, 1.0f);

    for (int i = 0; i < 80; ++i)
    {
        float sx      = distX(srng);
        float sy      = distY(srng);
        float base    = 0.3f + 0.7f * distS(srng);
        float flicker = 0.5f + 0.5f * math::NkSin(time * 1.5f + sx * 0.1f + sy * 0.07f);
        uint8_t v     = static_cast<uint8_t>(base * flicker * 180.0f);

        mRenderer.SetPixel(static_cast<int>(sx), static_cast<int>(sy),
                           { v, v, v, 255 });
    }
}

// ── PongGame::DrawObstaclePreview ─────────────────────────────────────────────
// Affiche une miniature du terrain montrant la disposition des obstacles
// du preset donne. Sert d\'apercu dans l\'ecran de selection des obstacles.
// Sauvegarde et restaure le preset courant pour ne pas perturber la partie.
void PongGame::DrawObstaclePreview(int px, int py, int pw, int ph,
                                   ObstaclePreset preset)
{
    // Fond du mini-terrain
    DrawPanel(px, py, pw, ph,
              AlphaF(gamecolors::BG(), 0.9f),
              AlphaF(gamecolors::PanelBorder(), 0.7f), 1);

    // Ligne centrale en pointilles du mini-terrain
    int lineX = px + pw / 2;
    int lineY1 = py + 2;
    int lineY2 = py + ph - 2;
    mRasterizer.DrawDashedLine(lineX, lineY1,
                               lineX, lineY2,
                               AlphaF(gamecolors::CenterLine(), 0.5f), 4);

    // Facteurs de mise a l\'echelle terrain -> preview
    float sW = static_cast<float>(mRenderer.Width());
    float sH = static_cast<float>(mRenderer.Height());
    float scaleX = static_cast<float>(pw) / sW;
    float scaleY = static_cast<float>(ph) / sH;

    // Sauvegarder le preset courant pour le restaurer apres le preview
    ObstaclePreset savedPreset = mSettings.obstaclePreset;

    // Generer temporairement les obstacles du preset a previsualiser
    mSettings.obstaclePreset = preset;
    SpawnObstacles();

    // Copier les obstacles generes pour la preview
    std::vector<Obstacle> previewObs = mObstacles;

    // Restaurer le preset original
    mSettings.obstaclePreset = savedPreset;
    SpawnObstacles();

    // Dessiner chaque obstacle en miniature dans le cadre de preview
    for (const auto& o : previewObs)
    {
        int rx = px + static_cast<int>(o.x * scaleX);
        int ry = py + static_cast<int>(o.y * scaleY);
        int rw = math::NkMax(2, static_cast<int>(o.w * scaleX));
        int rh = math::NkMax(2, static_cast<int>(o.h * scaleY));
        mRasterizer.FillRect(rx, ry, rw, rh, o.color);
    }

    // Message si le preset n\'a pas d\'obstacles
    if (previewObs.empty())
    {
        int msgX = px + pw / 2;
        int msgY = py + ph / 2 - 3;
        mRasterizer.DrawTextCentered(msgX, msgY,
                                     "VIDE",
                                     gamecolors::White(),
                                     1);
    }
}

// ── PongGame::RenderMainMenu ──────────────────────────────────────────────────
// Affiche l\'ecran titre avec : titre anime, 4 boutons (Jouer, Difficulte,
// Obstacles, Quitter), rappel des parametres actuels.
// Tout le texte est BLANC.
// L\'interface est parfaitement centree basee sur les dimensions reelles.
void PongGame::RenderMainMenu()
{
    uint32_t W    = mRenderer.Width();
    uint32_t H    = mRenderer.Height();
    int      cx   = static_cast<int>(W) / 2;
    int      cy   = static_cast<int>(H) / 2;
    float    wave = 0.5f + 0.5f * math::NkSin(mTime * 1.8f);
    float    S    = UIScale();
    auto     ui   = [S](int v) -> int { return static_cast<int>(static_cast<float>(v) * S + 0.5f); };
    int      ts   = S >= 0.8f ? 2 : 1;

    // ── Titre PONG ────────────────────────────────────────────────────────────
    int titleY = cy - ui(150);
    mRasterizer.DrawGlow(cx, titleY, ui(90), AlphaF(gamecolors::Player(), wave * 0.6f), 1.5f);
    int titleTextY = cy - ui(168);
    int titleScale = math::NkMax(2, ui(6));
    mRasterizer.DrawTextCentered(cx, titleTextY, "PONG", gamecolors::White(), titleScale);

    // ── Sous-titre ────────────────────────────────────────────────────────────
    int subtitleY = cy - ui(110);
    mRasterizer.DrawTextCentered(cx, subtitleY, "SOFTWARE RENDERER", gamecolors::White(), ts);

    // ── Panneau du menu ───────────────────────────────────────────────────────
    int pw = ui(320);
    int ph = ui(240);
    int mx = cx - pw / 2;
    int my = cy - ui(60);
    DrawPanel(mx, my, pw, ph, gamecolors::PanelBG(), gamecolors::PanelBorder(), 2);

    // ── 4 boutons du menu ─────────────────────────────────────────────────────
    const char* items[] = { "  JOUER  ", "  DIFFICULTE  ", "  OBSTACLES  ", "  QUITTER  " };
    math::NkColor cols[] = {
        gamecolors::Green(),
        gamecolors::Gold(),
        gamecolors::ObsPortal(),
        gamecolors::Red()
    };

    int bh = ui(44);
    int bw = pw - ui(24);
    int bx = mx + ui(12);
    int by = my + ui(14);

    for (int i = 0; i < 4; ++i)
    {
        bool sel = (mMainMenuSel == i);
        int btnY = by + i * (bh + ui(4));
        DrawButton(bx, btnY, bw, bh, items[i], sel, cols[i], ts);
    }

    // ── Rappel des parametres actuels ─────────────────────────────────────────
    char info[64];
    snprintf(info, sizeof(info), "Diff: %s  |  Obs: %s",
             AIDifficultyName(static_cast<AIDifficulty>(mDiffSel)),
             ObstaclePresetName(static_cast<ObstaclePreset>(mObsSel)));
    int infoY = my + ph + ui(10);
    mRasterizer.DrawTextCentered(cx, infoY, info, gamecolors::White(), ts);

    // ── Instructions de navigation (bas d\'ecran) ──────────────────────────────
    int instY1 = static_cast<int>(H) - ui(48);
    mRasterizer.DrawTextCentered(cx, instY1,
                                 "HAUT/BAS: naviguer   ENTREE/CLIC: confirmer",
                                 gamecolors::White(), 1);
    int instY2 = static_cast<int>(H) - ui(24);
    mRasterizer.DrawTextCentered(cx, instY2, "nkengine by Rihen", gamecolors::White(), 1);
}

// ── PongGame::RenderDiffSelect ────────────────────────────────────────────────
// Affiche l\'ecran de selection de la difficulte IA.
// Montre 4 boutons avec indicateurs de reaction/erreur et une description.
// Tout le texte est BLANC.
// L\'interface est parfaitement centree basee sur les dimensions reelles.
void PongGame::RenderDiffSelect()
{
    uint32_t W    = mRenderer.Width();
    uint32_t H    = mRenderer.Height();
    int      cx   = static_cast<int>(W) / 2;
    int      cy   = static_cast<int>(H) / 2;
    float    wave = 0.5f + 0.5f * math::NkSin(mTime * 2.0f);
    float    S    = UIScale();
    auto     ui   = [S](int v) -> int { return static_cast<int>(static_cast<float>(v) * S + 0.5f); };
    int      ts   = S >= 0.8f ? 2 : 1;

    // ── Titre ─────────────────────────────────────────────────────────────────
    int titleGlowY = cy - ui(90);
    mRasterizer.DrawGlow(cx, titleGlowY, ui(80), AlphaF(gamecolors::Gold(), wave * 0.5f), 1.2f);
    int titleTextY = cy - ui(108);
    mRasterizer.DrawTextCentered(cx, titleTextY, "DIFFICULTE", gamecolors::White(), math::NkMax(2, ui(3)));
    int subtitleTextY = cy - ui(76);
    mRasterizer.DrawTextCentered(cx, subtitleTextY,
                                 "Choisissez la puissance de l\'IA",
                                 gamecolors::White(), ts);

    // ── Panneau principal ─────────────────────────────────────────────────────
    int pw = ui(520);
    int ph = ui(320);
    int px = cx - pw / 2;
    int py = cy - ui(60);
    DrawPanel(px, py, pw, ph, gamecolors::PanelBG(), gamecolors::PanelBorder(), 2);

    // Couleurs par difficulte
    math::NkColor dcols[] = {
        gamecolors::Green(),
        gamecolors::Gold(),
        gamecolors::ObsRandom(),
        gamecolors::Red()
    };

    // Stats de reaction et d\'erreur par difficulte
    const char* stats[] = {
        "Reaction: 28%   Erreur: +-72px   Pas de prediction",
        "Reaction: 52%   Erreur: +-38px   Pas de prediction",
        "Reaction: 73%   Erreur: +-16px   Prediction: 0.09s",
        "Reaction: 88%   Erreur: +-5px    Prediction: 0.16s"
    };

    int bh  = ui(56);
    int bw  = pw - ui(24);
    int bx  = px + ui(12);
    int by  = py + ui(12);
    int bth = ui(36);

    for (int i = 0; i < 4; ++i)
    {
        bool sel = (mDiffSel == i);

        // Bouton principal avec le nom de la difficulte (texte blanc)
        int btnY = by + i * (bh + ui(4));
        DrawButton(bx, btnY, bw, bth,
                   AIDifficultyName(static_cast<AIDifficulty>(i)), sel, dcols[i], ts);

        // Stats en dessous du bouton (texte blanc)
        int statY = btnY + bth + ui(4);
        mRasterizer.DrawText(bx + ui(10), statY, stats[i], gamecolors::White(), 1);
    }

    // ── Description de la difficulte selectionnee ─────────────────────────────
    int descY = py + ph + ui(6);
    DrawPanel(px, descY, pw, ui(36),
              gamecolors::PanelBG(), AlphaF(gamecolors::PanelBorder(), 0.5f), 1);
    int descTextY = descY + ui(12);
    mRasterizer.DrawTextCentered(cx, descTextY,
        AIDifficultyDesc(static_cast<AIDifficulty>(mDiffSel)),
        gamecolors::White(), 1);

    // ── Instructions ──────────────────────────────────────────────────────────
    int instY1 = static_cast<int>(H) - ui(48);
    mRasterizer.DrawTextCentered(cx, instY1,
                                 "HAUT/BAS: choisir   ENTREE/CLIC: confirmer   ECHAP: retour",
                                 gamecolors::White(), 1);
    int instY2 = static_cast<int>(H) - ui(24);
    mRasterizer.DrawTextCentered(cx, instY2, "nkengine by Rihen", gamecolors::White(), 1);
}

// ── PongGame::RenderObsSelect ─────────────────────────────────────────────────
// Affiche l\'ecran de selection des obstacles.
// Deux panneaux : liste a gauche, description + preview miniature a droite.
// Tout le texte est BLANC.
// L\'interface est parfaitement centree basee sur les dimensions reelles.
void PongGame::RenderObsSelect()
{
    uint32_t W    = mRenderer.Width();
    uint32_t H    = mRenderer.Height();
    int      cx   = static_cast<int>(W) / 2;
    int      cy   = static_cast<int>(H) / 2;
    float    wave = 0.5f + 0.5f * math::NkSin(mTime * 2.0f);
    float    S    = UIScale();
    auto     ui   = [S](int v) -> int { return static_cast<int>(static_cast<float>(v) * S + 0.5f); };
    int      ts   = S >= 0.8f ? 2 : 1;

    // ── Titre ─────────────────────────────────────────────────────────────────
    int titleGlowY = cy - ui(90);
    mRasterizer.DrawGlow(cx, titleGlowY, ui(80), AlphaF(gamecolors::ObsPortal(), wave * 0.5f), 1.2f);
    int titleTextY = cy - ui(108);
    mRasterizer.DrawTextCentered(cx, titleTextY, "OBSTACLES", gamecolors::White(), math::NkMax(2, ui(3)));
    int subtitleTextY = cy - ui(76);
    mRasterizer.DrawTextCentered(cx, subtitleTextY, "Choisissez le type d\'obstacles", gamecolors::White(), ts);

    // ── Panneau gauche : liste des 6 presets ──────────────────────────────────
    int lpw = ui(260);
    int lph = ui(330);
    int lpx = cx - lpw - ui(10);
    int lpy = cy - ui(60);
    DrawPanel(lpx, lpy, lpw, lph, gamecolors::PanelBG(), gamecolors::PanelBorder(), 2);

    // Couleurs par preset
    math::NkColor ocols[] = {
        gamecolors::White(),
        gamecolors::ObsBoost(),
        gamecolors::ObsRandom(),
        gamecolors::ObsRedirect(),
        gamecolors::ObsPortal(),
        gamecolors::Red()
    };

    int bh = ui(44);
    int bw = lpw - ui(16);
    int bx = lpx + ui(8);
    int by = lpy + ui(10);

    for (int i = 0; i < 6; ++i)
    {
        bool sel = (mObsSel == i);
        int btnY = by + i * (bh + ui(4));
        DrawButton(bx, btnY, bw, bh,
                   ObstaclePresetName(static_cast<ObstaclePreset>(i)),
                   sel, ocols[i], ts);
    }

    // ── Panneau droit : description + preview ─────────────────────────────────
    int rpw = ui(260);
    int rph = lph;
    int rpx = cx + ui(10);
    int rpy = lpy;
    DrawPanel(rpx, rpy, rpw, rph, gamecolors::PanelBG(), gamecolors::PanelBorder(), 2);

    // Nom du preset selectionne (blanc)
    int presetNameX = rpx + rpw / 2;
    int presetNameY = rpy + ui(12);
    mRasterizer.DrawTextCentered(presetNameX, presetNameY,
        ObstaclePresetName(static_cast<ObstaclePreset>(mObsSel)),
        gamecolors::White(), ts);

    // Description du preset (blanc)
    int descX = rpx + ui(8);
    int descY = rpy + ui(34);
    mRasterizer.DrawText(descX, descY,
        ObstaclePresetDesc(static_cast<ObstaclePreset>(mObsSel)),
        gamecolors::White(), 1);

    // Preview miniature du terrain avec les obstacles
    int previewX = rpx + ui(8);
    int previewY = rpy + ui(68);
    int previewW = rpw - ui(16);
    int previewH = rph - ui(78);
    DrawObstaclePreview(previewX, previewY, previewW, previewH,
                        static_cast<ObstaclePreset>(mObsSel));

    // ── Instructions ──────────────────────────────────────────────────────────
    int instY1 = static_cast<int>(H) - ui(48);
    mRasterizer.DrawTextCentered(cx, instY1,
                                 "HAUT/BAS: choisir   ENTREE/CLIC: confirmer   ECHAP: retour",
                                 gamecolors::White(), 1);
    int instY2 = static_cast<int>(H) - ui(24);
    mRasterizer.DrawTextCentered(cx, instY2, "nkengine by Rihen", gamecolors::White(), 1);
}

// ── PongGame::RenderPauseOverlay ──────────────────────────────────────────────
// Overlay sur la scene de jeu : assombrit l\'ecran, affiche le titre "PAUSE"
// et les 2 boutons (Reprendre / Menu principal).
// Navigation possible avec HAUT/BAS (non implementee ici - un seul item).
// Tout le texte est BLANC.
// L\'interface est parfaitement centree.
void PongGame::RenderPauseOverlay()
{
    uint32_t W  = mRenderer.Width();
    uint32_t H  = mRenderer.Height();
    int      cx = static_cast<int>(W) / 2;
    int      cy = static_cast<int>(H) / 2;
    float    S  = UIScale();
    auto     ui = [S](int v) -> int { return static_cast<int>(static_cast<float>(v) * S + 0.5f); };
    int      ts = S >= 0.8f ? 2 : 1;

    float wave = 0.5f + 0.5f * math::NkSin(mTime * 2.5f);

    // Assombrissement de la scene de jeu
    for (int row = 0; row < static_cast<int>(H); row += 2)
    {
        for (int col = 0; col < static_cast<int>(W); col += 2)
        {
            math::NkColor dst = mRenderer.GetPixel(col, row);
            mRenderer.SetPixel(col, row, Blend(dst, { 0, 0, 0, 140 }));
        }
    }

    // ── Titre PAUSE ───────────────────────────────────────────────────────────
    int titleGlowY = cy - ui(80);
    mRasterizer.DrawGlow(cx, titleGlowY, ui(70), AlphaF(gamecolors::Gold(), wave * 0.5f), 1.2f);
    int titleTextY = cy - ui(92);
    mRasterizer.DrawTextCentered(cx, titleTextY, "PAUSE", gamecolors::White(), math::NkMax(2, ui(4)));

    // ── Panneau avec les 2 options ────────────────────────────────────────────
    int pw   = ui(260);
    int ph   = ui(120);
    int px   = cx - pw / 2;
    int py   = cy - ui(28);
    int bw   = pw - ui(20);
    int bx   = px + ui(10);
    int bh   = ui(48);
    DrawPanel(px, py, pw, ph, gamecolors::PanelBG(), gamecolors::PanelBorder(), 2);

    // Bouton Reprendre (texte blanc)
    int btn1Y = py + ui(10);
    DrawButton(bx, btn1Y, bw, bh, "  REPRENDRE  ", (mPauseSel == 0), gamecolors::Green(), ts);

    // Bouton Menu principal (texte blanc)
    int btn2Y = py + ui(64);
    DrawButton(bx, btn2Y, bw, bh, "  MENU PRINCIPAL  ", (mPauseSel == 1), gamecolors::Red(), ts);

    // ── Instructions ──────────────────────────────────────────────────────────
    int instY = static_cast<int>(H) - ui(24);
    mRasterizer.DrawTextCentered(cx, instY,
                                 "HAUT/BAS: naviguer   ENTREE/CLIC: confirmer   ECHAP: reprendre",
                                 gamecolors::White(), 1);
}

// ── PongGame::RenderGoalFlash ─────────────────────────────────────────────────
// Affiche un message de but avec la couleur du marqueur.
// L\'intensite decroit proportionnellement au timer restant.
// Parfaitement centre.
void PongGame::RenderGoalFlash()
{
    uint32_t W = mRenderer.Width();
    uint32_t H = mRenderer.Height();
    int cx = static_cast<int>(W) / 2;
    int cy = static_cast<int>(H) / 2;

    // Intensite proportionnelle au temps restant [0..1]
    float    t       = mGoalTimer / 1.8f;
    math::NkColor c  = (mLastScorer == 0) ? gamecolors::Player() : gamecolors::AI();
    const char* name = (mLastScorer == 0) ? "JOUEUR" : "IA";

    // Message compose
    char msg[32];
    snprintf(msg, sizeof(msg), "BUT! %s", name);

    // Lueur et texte (couleur du marqueur, toujours lisible)
    int glowY = cy;
    mRasterizer.DrawGlow(cx, glowY, 110,
                         AlphaF(c, t * 0.55f),
                         2.0f);
    int textY = cy - 24;
    mRasterizer.DrawTextCentered(cx, textY, msg,
                                 gamecolors::White(),
                                 5);
}

// ── PongGame::RenderGameOver ──────────────────────────────────────────────────
// Affiche l\'ecran de fin de partie avec le vainqueur, le score final
// et les options Rejouer / Retour au menu.
// Tout le texte est BLANC.
// Parfaitement centre.
void PongGame::RenderGameOver()
{
    uint32_t W = mRenderer.Width();
    uint32_t H = mRenderer.Height();
    int cx = static_cast<int>(W) / 2;
    int cy = static_cast<int>(H) / 2;
    float S  = UIScale();
    auto  ui = [S](int v) -> int { return static_cast<int>(static_cast<float>(v) * S + 0.5f); };
    int   ts = S >= 0.8f ? 2 : 1;

    bool playerWon = (mPlayer.score >= mSettings.maxScore);
    math::NkColor wc = playerWon ? gamecolors::Player() : gamecolors::AI();
    float wave = 0.6f + 0.4f * math::NkSin(mTime * 3.0f);

    // Assombrissement de la scene
    for (int row = 0; row < static_cast<int>(H); row += 2)
    {
        for (int col = 0; col < static_cast<int>(W); col += 2)
        {
            math::NkColor dst = mRenderer.GetPixel(col, row);
            mRenderer.SetPixel(col, row, Blend(dst, { 0, 0, 0, 120 }));
        }
    }

    // ── Titre Victoire / Defaite ──────────────────────────────────────────────
    int titleGlowY = cy - ui(60);
    mRasterizer.DrawGlow(cx, titleGlowY, ui(130), AlphaF(wc, wave * 0.55f), 1.6f);
    const char* msg = playerWon ? "VICTOIRE!" : "DEFAITE...";
    int titleTextY = cy - ui(80);
    mRasterizer.DrawTextCentered(cx, titleTextY, msg, gamecolors::White(), math::NkMax(2, ui(5)));

    // ── Score final ───────────────────────────────────────────────────────────
    char sc[32];
    snprintf(sc, sizeof(sc), "%d  -  %d", mPlayer.score, mAI.score);
    int scoreY = cy - ui(24);
    mRasterizer.DrawTextCentered(cx, scoreY, sc, gamecolors::White(), math::NkMax(1, ui(3)));

    // ── Bouton Rejouer ────────────────────────────────────────────────────────
    int pw = ui(360);
    int ph = ui(60);
    int px = cx - pw / 2;
    int py = cy + ui(10);
    DrawPanel(px, py, pw, ph, gamecolors::PanelBG(), gamecolors::PanelBorder(), 2);
    int btnX = px + ui(10);
    int btnY = py + ui(8);
    int btnW = pw - ui(20);
    int btnH = ui(44);
    DrawButton(btnX, btnY, btnW, btnH, "  REJOUER  ", true, gamecolors::Green(), ts);

    // ── Instructions ──────────────────────────────────────────────────────────
    int instY = static_cast<int>(H) - ui(24);
    mRasterizer.DrawTextCentered(cx, instY,
                                 "ENTREE/CLIC: rejouer   ECHAP: menu principal",
                                 gamecolors::White(), 1);
}

// =============================================================================
// Section 9 : Splash screen + boutons tactiles Android
// =============================================================================

// ── PongGame::UpdateSplash ────────────────────────────────────────────────────
// Decremente le timer du splash screen.
// Bascule vers MainMenu quand le timer expire ou qu'une touche est pressee.
void PongGame::UpdateSplash(float dt, bool anyKey)
{
    mSplashTimer -= dt;

    if (mSplashTimer <= 0.0f || anyKey)
    {
        mState       = GameState::MainMenu;
        mSplashTimer = 0.0f;
    }
}

// ── PongGame::RenderSplash ────────────────────────────────────────────────────
// Ecran de demarrage : fond etoile, nom de l\'entreprise "RIHEN",
// nom du moteur "NOGE" et titre "PONG". Pulse "appuyez" en bas.
void PongGame::RenderSplash()
{
    uint32_t W  = mRenderer.Width();
    uint32_t H  = mRenderer.Height();
    int      cx = static_cast<int>(W) / 2;
    int      cy = static_cast<int>(H) / 2;
    float    S  = UIScale();
    auto     ui = [S](int v) -> int { return static_cast<int>(static_cast<float>(v) * S + 0.5f); };

    // Fond tres sombre
    mRenderer.Clear({ 3, 5, 14, 255 });

    // Champ d\'etoiles
    DrawStars(mTime);

    // ── Ligne decorative superieure ───────────────────────────────────────────
    int lineW   = ui(180);
    int lineTopY = cy - ui(105);
    mRasterizer.DrawLine(cx - lineW, lineTopY, cx + lineW, lineTopY,
                         { 50, 80, 160, 180 });

    // ── Nom de l\'entreprise : RIHEN ──────────────────────────────────────────
    int rihenY = cy - ui(95);
    mRasterizer.DrawTextCentered(cx, rihenY, "RIHEN", { 243, 152, 15, 255 }, math::NkMax(2, ui(5)));

    // ── Sous-titre moteur ─────────────────────────────────────────────────────
    int poweredY = rihenY + ui(52);
    mRasterizer.DrawTextCentered(cx, poweredY, "POWERED BY", { 160, 190, 220, 200 }, S >= 0.8f ? 2 : 1);

    // ── Nom du moteur : NOGE ──────────────────────────────────────────────────
    int nogeY = poweredY + ui(22);
    mRasterizer.DrawTextCentered(cx, nogeY, "NOGE", { 9, 84, 96, 255 }, math::NkMax(2, ui(4)));

    // ── Ligne decorative centrale ─────────────────────────────────────────────
    int lineMidY = nogeY + ui(50);
    mRasterizer.DrawLine(cx - lineW, lineMidY, cx + lineW, lineMidY, { 50, 80, 160, 180 });

    // ── Titre du jeu : PONG ───────────────────────────────────────────────────
    int pongY = lineMidY + ui(14);
    mRasterizer.DrawTextCentered(cx, pongY, "PONG", gamecolors::White(), math::NkMax(2, ui(4)));

    // ── Barre de progression (temps restant) ──────────────────────────────────
    if (mSplashTimer > 0.0f)
    {
        float ratio = mSplashTimer / 3.0f;
        int   barW  = static_cast<int>(static_cast<float>(W) * 0.38f * ratio);
        int   barX  = cx - barW / 2;
        int   barY  = static_cast<int>(H) - ui(38);
        mRasterizer.FillRect(barX, barY, barW, ui(3), gamecolors::Gold());
    }

    // ── "Appuyez sur une touche" (pulse) ──────────────────────────────────────
    float   pulse = (math::NkSin(mTime * 3.0f) + 1.0f) * 0.5f;
    uint8_t alpha = static_cast<uint8_t>(120.0f + pulse * 120.0f);
    mRasterizer.DrawTextCentered(cx, static_cast<int>(H) - ui(28), "APPUYEZ SUR UNE TOUCHE", { 255, 255, 255, alpha }, 1);
}

// ── PongGame::GetTouchButtonRects ─────────────────────────────────────────────
// Calcule les rectangles des boutons tactiles :
//   - ENTER    : Haut a gauche (validation)
//   - ESCAPE   : Haut au centre (retour/menu)
//   - PAUSE    : Haut a droite (pause du jeu)
//   - UP/DOWN  : Bas au centre (navigation directionnelle)
PongGame::TouchButtonRects PongGame::GetTouchButtonRects() const noexcept
{
    uint32_t W  = mRenderer.Width();
    uint32_t H  = mRenderer.Height();
    
    const int BTN_W     = 80;   // Largeur des boutons d'action (haut)
    const int BTN_H     = 70;   // Hauteur des boutons d'action (haut)
    const int NAV_W     = 100;  // Largeur des boutons de navigation (bas)
    const int NAV_H     = 80;   // Hauteur des boutons de navigation (bas)
    const int MARGIN    = 12;   // Marge depuis les bords
    const int CENTER_X  = static_cast<int>(W) / 2;
    const int CENTER_Y  = static_cast<int>(H) / 2;
    const int NAV_GAP   = 15;   // Ecart entre UP et DOWN
    const int ACTION_GAP= 8;    // Ecart entre les boutons d'action du haut

    TouchButtonRects r;
    
    // ── Boutons d'action en haut ──────────────────────────────────────────────
    // ENTER : Haut a gauche
    r.enterX = MARGIN;
    r.enterY = MARGIN;
    r.enterW = BTN_W;
    r.enterH = BTN_H;
    
    // ESCAPE : Haut au centre
    r.escapeX = CENTER_X - BTN_W / 2;
    r.escapeY = MARGIN;
    r.escapeW = BTN_W;
    r.escapeH = BTN_H;
    
    // PAUSE : Haut a droite
    r.pauseX = static_cast<int>(W) - MARGIN - BTN_W;
    r.pauseY = MARGIN;
    r.pauseW = BTN_W;
    r.pauseH = BTN_H;
    
    // ── Boutons de navigation en bas ──────────────────────────────────────────
    // UP : Bas au centre, a gauche
    r.upX = CENTER_X - NAV_W - NAV_GAP / 2;
    r.upY = static_cast<int>(H) - MARGIN - NAV_H;
    r.upW = NAV_W;
    r.upH = NAV_H;
    
    // DOWN : Bas au centre, a droite
    r.dnX = CENTER_X + NAV_GAP / 2;
    r.dnY = static_cast<int>(H) - MARGIN - NAV_H;
    r.dnW = NAV_W;
    r.dnH = NAV_H;
    
    return r;
}

// ── PongGame::RenderTouchButtons ──────────────────────────────────────────────
// Affiche cinq boutons semi-transparents pour Android :
//   - ENTER    : Haut a gauche (validation)
//   - ESCAPE   : Haut au centre (retour/menu)
//   - PAUSE    : Haut a droite (pause du jeu)
//   - UP/DOWN  : Bas au centre (navigation directionnelle)
void PongGame::RenderTouchButtons()
{
    auto r = GetTouchButtonRects();
    
    math::NkColor bg  = { 20, 40, 80, 160 };
    math::NkColor bdr = { 80, 160, 255, 200 };
    
    // Lambda pour dessiner un bouton avec bordure
    auto DrawButtonBorder = [&](int x, int y, int w, int h, math::NkColor col) {
        for (int t = 0; t < 2; ++t) {
            mRasterizer.DrawLine(x + t, y + t, x + w - t, y + t, col);
            mRasterizer.DrawLine(x + t, y + h - t, x + w - t, y + h - t, col);
            mRasterizer.DrawLine(x + t, y + t, x + t, y + h - t, col);
            mRasterizer.DrawLine(x + w - t, y + t, x + w - t, y + h - t, col);
        }
    };
    
    // ── Bouton ENTER (haut a gauche) ──────────────────────────────────────────
    mRasterizer.FillRect(r.enterX, r.enterY, r.enterW, r.enterH, bg);
    DrawButtonBorder(r.enterX, r.enterY, r.enterW, r.enterH, bdr);
    mRasterizer.DrawTextCentered(r.enterX + r.enterW / 2, r.enterY + r.enterH / 2 - 8,
                                 "OK", gamecolors::Green(), 1);
    
    // ── Bouton ESCAPE (haut au centre) ────────────────────────────────────────
    mRasterizer.FillRect(r.escapeX, r.escapeY, r.escapeW, r.escapeH, bg);
    DrawButtonBorder(r.escapeX, r.escapeY, r.escapeW, r.escapeH, bdr);
    mRasterizer.DrawTextCentered(r.escapeX + r.escapeW / 2, r.escapeY + r.escapeH / 2 - 8,
                                 "BACK", gamecolors::Red(), 1);
    
    // ── Bouton PAUSE (haut a droite) ──────────────────────────────────────────
    mRasterizer.FillRect(r.pauseX, r.pauseY, r.pauseW, r.pauseH, bg);
    DrawButtonBorder(r.pauseX, r.pauseY, r.pauseW, r.pauseH, bdr);
    mRasterizer.DrawTextCentered(r.pauseX + r.pauseW / 2, r.pauseY + r.pauseH / 2 - 8,
                                 "PAUSE", gamecolors::Gold(), 1);
    
    // ── Bouton UP (bas, a gauche du centre) ───────────────────────────────────
    mRasterizer.FillRect(r.upX, r.upY, r.upW, r.upH, bg);
    DrawButtonBorder(r.upX, r.upY, r.upW, r.upH, bdr);
    {
        int ucx = r.upX + r.upW / 2;
        int ucy = r.upY + r.upH / 2;
        for (int dy = 0; dy <= 15; ++dy) {
            int hw = dy;
            int py = ucy + dy - 8;
            mRasterizer.DrawLine(ucx - hw, py, ucx + hw, py, gamecolors::Player());
        }
    }
    mRasterizer.DrawTextCentered(r.upX + r.upW / 2, r.upY + r.upH - 12,
                                 "UP", gamecolors::White(), 1);
    
    // ── Bouton DOWN (bas, a droite du centre) ─────────────────────────────────
    mRasterizer.FillRect(r.dnX, r.dnY, r.dnW, r.dnH, bg);
    DrawButtonBorder(r.dnX, r.dnY, r.dnW, r.dnH, bdr);
    {
        int dcx = r.dnX + r.dnW / 2;
        int dcy = r.dnY + r.dnH / 2;
        for (int dy = 0; dy <= 15; ++dy) {
            int hw = dy;
            int py = dcy - dy + 8;
            mRasterizer.DrawLine(dcx - hw, py, dcx + hw, py, gamecolors::Player());
        }
    }
    mRasterizer.DrawTextCentered(r.dnX + r.dnW / 2, r.dnY + 12,
                                 "DOWN", gamecolors::White(), 1);
}