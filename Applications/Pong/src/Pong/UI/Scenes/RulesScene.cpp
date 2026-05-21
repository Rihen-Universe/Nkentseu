// =============================================================================
// RulesScene.cpp
// =============================================================================

#include "RulesScene.h"
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include "Pong/UI/ResponsiveLayout.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKMath/NkFunctions.h"
#include <cstring>

namespace nkentseu
{
    namespace pong
    {

        // ── Sections du contenu ──────────────────────────────────────────────
        // Chaque section : titre + corps multi-ligne (separe par '\n'). On
        // garde le texte SOURCE en majuscules avec ponctuation simple pour
        // matcher le style du jeu (font atlas est borne aux ASCII basiques).
        // Types d'illustrations possibles au-dessus du corps d'une section.
        // Pour les sections sans illustration (objectif / parametres),
        // on garde kIllNone (juste le texte).
        enum IllustrationKind
        {
            kIllNone        = 0,
            kIllModes       = 1,    ///< 4 mini-ecrans (Local, VsAI, IAvsAI, Reseau)
            kIllControls    = 2,    ///< Touches WASD + fleches + doigt drag
            kIllObstacles   = 3,    ///< 8 mini-obstacles
            kIllBonus       = 4,    ///< 6 mini-orbes bonus
            kIllMalus       = 5,    ///< 6 mini-orbes malus
            kIllDrops       = 6,    ///< Orbe qui descend
            kIllParams      = 7,    ///< Stepper + chrono
            kIllObjectif    = 8,    ///< Trophee
        };

        struct RulesSection
        {
            const char* title;
            const char* body;
            IllustrationKind illustration;
        };

        static const RulesSection kSections[] =
        {
            { "OBJECTIF",
              "LE PREMIER JOUEUR A ATTEINDRE LE SCORE CIBLE GAGNE.\n"
              "SI UN TEMPS LIMITE EST DEFINI, A L'EXPIRATION C'EST\n"
              "LE JOUEUR EN TETE QUI GAGNE.\n"
              "L'OPTION 'VICTOIRE A 2 PTS D'ECART' (REGLE TENNIS)\n"
              "EXIGE 2 POINTS DE MARGE POUR VALIDER LA VICTOIRE.",
              kIllObjectif },

            { "MODES",
              "LOCAL    : 2 JOUEURS, MEME CLAVIER (W/S ET FLECHES).\n"
              "VS IA    : 1 JOUEUR CONTRE L'IA (RAQUETTE DROITE).\n"
              "IA VS IA : DEMO, DEUX IA S'AFFRONTENT.\n"
              "RESEAU   : 1V1 LAN OU INTERNET (HEBERGER / REJOINDRE).",
              kIllModes },

            { "RESEAU — COMMENT SE CONNECTER",
              "AU DEMARRAGE, PONG TE DONNE UN IDENTIFIANT UNIQUE\n"
              "AU FORMAT : PAYS/VILLE-CODE\n"
              "(EX: CAMEROUN/DOUALA-047281639). CET IDENTIFIANT EST\n"
              "TA CARTE DE VISITE SUR LE RESEAU.\n"
              "\n"
              "1. 2 APPAREILS SUR LE MEME WIFI (LAN) :\n"
              "   - HOTE : OUVRE LE LOBBY, CLIQUE HEBERGER.\n"
              "   - HOTE : ATTENDS QU'UN INVITE TE REJOIGNE.\n"
              "   - INVITE : CLIQUE REJOINDRE, LA LISTE DES HOTES\n"
              "     VISIBLES APPARAIT AVEC LEUR PAYS/VILLE-CODE.\n"
              "   - INVITE : TAPE SUR LE NOM DE L'HOTE POUR LE\n"
              "     REJOINDRE — AUCUNE IP A SAISIR.\n"
              "\n"
              "2. 2 APPAREILS DE RESEAUX DIFFERENTS (INTERNET) :\n"
              "   - PAS ENCORE SUPPORTE EN V1 (CHANTIER EN COURS).\n"
              "   - PROCHAINEMENT : MATCHMAKING PAR SERVEUR DEDIE\n"
              "     QUI ASSOCIERA LES PAYS/VILLE-CODE A TRAVERS\n"
              "     LE MONDE.\n"
              "\n"
              "3. VALIDATION DU CHALLENGER (PHASE B EN COURS) :\n"
              "   - SI PLUSIEURS INVITES TENTENT DE TE REJOINDRE,\n"
              "     L'HOTE CHOISIRA QUEL JOUEUR ACCEPTER POUR LE\n"
              "     MATCH ; LES AUTRES SERONT REFUSES.\n"
              "\n"
              "ASTUCE DEBUG : 2 INSTANCES PONG SUR LE MEME PC ->\n"
              "L'INVITE VERRA L'HOTE DANS SA LISTE.",
              kIllNone },

            { "PARAMETRES DU MATCH",
              "POINTS POUR GAGNER : 3 / 5 / 7 / 11 / 15 / 21 / ILLIMITE.\n"
              "TEMPS LIMITE       : 0 (LIBRE) OU DE 1MIN A 5MIN.\n"
              "VITESSE BALLE      : MULTIPLICATEUR X1.0 A X10.0\n"
              "                     (PAS DE 0.1, HOLD POUR ALLER VITE).\n"
              "VICTOIRE 2 PTS D'ECART : ON / OFF.",
              kIllParams },

            { "CONTROLES",
              "P1 CLAVIER  : W (HAUT) / S (BAS).\n"
              "P2 CLAVIER  : FLECHES HAUT / BAS.\n"
              "SOURIS/TOUCH: DRAG VERTICAL SUR TA MOITIE DE TERRAIN.\n"
              "PAUSE       : ESPACE OU BOUTON HUD.\n"
              "RETOUR      : ECHAP (PC) OU BOUTON RETOUR.",
              kIllControls },

            { "OBSTACLES (8 TYPES)",
              "TOGGLE PAR TYPE + COUNT + PUISSANCE + CHAOTIQUE PAR\n"
              "OBSTACLE (CLIQUER UNE CARTE POUR EDITER).\n"
              "\n"
              "MUR SOLIDE       : REBOND A 90 DEGRES.\n"
              "PORTAIL          : TELEPORTE + BOOST X1.5.\n"
              "ZONE GRAVITE     : ATTIRE LA BALLE.\n"
              "AIMANT           : PROPULSE X2 AU CONTACT.\n"
              "MINE             : EXPLOSION, VITESSE ALEATOIRE.\n"
              "MIROIR FANTOME   : TRAVERSE (PAS D'EFFET).\n"
              "COURANT D'AIR    : POUSSE VERTICAL.\n"
              "ETOILE BONUS     : DECLENCHE UN BONUS ALEATOIRE\n"
              "                   AU DERNIER JOUEUR AYANT TOUCHE.",
              kIllObstacles },

            { "BONUS (POUR TOI)",
              "GEANT       : TA RAQUETTE X2 (8S).\n"
              "VITESSE     : TA RAQUETTE PLUS RAPIDE (6S).\n"
              "DOUBLE PT   : TON PROCHAIN BUT COMPTE DOUBLE.\n"
              "BOUCLIER    : ABSORBE 1 BUT CONTRE TOI (12S).\n"
              "BALLE LENTE : VITESSE BALLE / 2 (5S).\n"
              "SURPRISE    : TIRAGE ALEATOIRE PARMI CES 5.",
              kIllBonus },

            { "MALUS (POUR L'ADVERSAIRE)",
              "AVEUGLE      : SA MOITIE DE TERRAIN ASSOMBRIE (2S).\n"
              "MINI         : SA RAQUETTE DIVISEE PAR 2 (6S).\n"
              "INVERSE      : SES CONTROLES INVERSES (4S).\n"
              "GEL          : SA RAQUETTE GELEE (1.5S).\n"
              "BALLE RAPIDE : VITESSE BALLE X3 (10S).\n"
              "TELEPORT     : RAQUETTE TELEPORTEE.",
              kIllMalus },

            { "DROPS",
              "DES ORBES TOMBENT DU HAUT DU TERRAIN TOUTES LES\n"
              "8 A 15 SECONDES. ATTRAPE-LES AVEC TA RAQUETTE :\n"
              " - ORBE BONUS (POINT BLANC) -> EFFET BONUS POUR TOI\n"
              " - ORBE MALUS (CROIX BLANCHE) -> MALUS A L'ADVERSAIRE\n"
              "REPARTITION : 60% BONUS / 40% MALUS.",
              kIllDrops },
        };
        static const int kSectionsN = (int)(sizeof(kSections)
                                          / sizeof(kSections[0]));

        // ── Easing ───────────────────────────────────────────────────────────
        static float EaseOutCubic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        static math::NkColor MakeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        {
            math::NkColor c; c.r = r; c.g = g; c.b = b; c.a = a; return c;
        }

        // ── Mini illustrations procedurales ─────────────────────────────────
        // Toutes prennent (x, y) = coin haut-gauche de la zone alouee,
        // (w, h) = dimensions, et dessinent l'illustration centree dedans.
        // ─────────────────────────────────────────────────────────────────────

        // Dessine une mini-carte d'item : fond + outline + label en-dessous.
        // Le contenu visuel est trace par @p draw qui recoit le centre + rayon.
        struct MiniItem
        {
            math::NkColor accent;
            const char*   label;
        };

        // Helper : icone obstacle (8 types) dans une zone (cx, cy) rayon ~r.
        static void DrawObstacleIcon(GLRenderer2D& r,
                                     int kind,        // 0..7 = ObstacleType
                                     float cx, float cy, float rad,
                                     float pulse01)
        {
            switch (kind)
            {
            case 0:  // Wall : carre teal
                r.DrawQuad       (cx - rad, cy - rad, rad * 2, rad * 2, MakeColor(60, 80, 110, 230));
                r.DrawQuadOutline(cx - rad, cy - rad, rad * 2, rad * 2, MakeColor(100, 150, 200, 220), 1.5f);
                break;
            case 1:  // Portal : cercle dore
                r.DrawCircle       (cx, cy, rad, MakeColor(255, 215, 0, 90 + (int)(pulse01 * 80)), 24);
                r.DrawCircleOutline(cx, cy, rad, MakeColor(255, 215, 0, 230), 1.5f, 24);
                break;
            case 2:  // Gravity : 3 cercles concentriques
                for (int k = 0; k < 3; ++k)
                {
                    const float rr = rad * (0.35f + 0.25f * k);
                    r.DrawCircleOutline(cx, cy, rr, MakeColor(204, 119, 255, 180), 1.5f, 24);
                }
                r.DrawCircle(cx, cy, rad * 0.18f, MakeColor(204, 119, 255, 230), 12);
                break;
            case 3:  // Magnet : U
                r.DrawQuad(cx - rad,            cy - rad,        rad * 0.55f, rad * 2,     MakeColor(0, 245, 255, 230));
                r.DrawQuad(cx + rad * 0.45f,    cy - rad,        rad * 0.55f, rad * 2,     MakeColor(0, 245, 255, 230));
                r.DrawQuad(cx - rad,            cy + rad * 0.55f,rad * 2,     rad * 0.45f, MakeColor(0, 245, 255, 230));
                break;
            case 4:  // Mine : cercle rouge + pics
                r.DrawCircle(cx, cy, rad * 0.55f, MakeColor(255, 64, 64, 200), 16);
                for (int k = 0; k < 8; ++k)
                {
                    const float a  = 6.28318f * k / 8.0f;
                    const float r1 = rad * 0.60f, r2 = rad;
                    r.DrawLine(cx + math::NkCos(a) * r1, cy + math::NkSin(a) * r1,
                               cx + math::NkCos(a) * r2, cy + math::NkSin(a) * r2,
                               MakeColor(255, 64, 64, 230), 1.5f);
                }
                break;
            case 5:  // GhostMirror : losange purple
            {
                const float a = 0.6f + pulse01 * 0.3f;
                const math::NkColor c = MakeColor(204, 119, 255, (uint8_t)(a * 255));
                r.DrawTriangle(cx,        cy - rad, cx + rad, cy,        cx,        cy + rad, c);
                r.DrawTriangle(cx,        cy - rad, cx - rad, cy,        cx,        cy + rad, c);
                break;
            }
            case 6:  // AirCurrent : 3 bandes vertes horizontales
                for (int k = 0; k < 3; ++k)
                {
                    const float yy = cy - rad * 0.6f + k * rad * 0.6f;
                    r.DrawQuad(cx - rad, yy, rad * 2, rad * 0.2f, MakeColor(0, 255, 100, 200));
                }
                break;
            case 7:  // BonusStar : pentagram dore
            {
                const float scl = 0.85f + 0.15f * pulse01;
                for (int k = 0; k < 5; ++k)
                {
                    const float a1 = -1.5708f + 6.28318f * k / 5.0f;
                    const float a2 = -1.5708f + 6.28318f * (k + 2) / 5.0f;
                    r.DrawLine(cx + math::NkCos(a1) * rad * scl, cy + math::NkSin(a1) * rad * scl,
                               cx + math::NkCos(a2) * rad * scl, cy + math::NkSin(a2) * rad * scl,
                               MakeColor(255, 215, 0, 240), 1.5f);
                }
                break;
            }
            }
        }

        // Helper : orbe bonus/malus (orbe halo + cercle plein + marqueur).
        // @p isBonus controle le marqueur (point blanc vs croix).
        static void DrawOrbIcon(GLRenderer2D& r, math::NkColor color,
                                bool isBonus, float cx, float cy, float rad)
        {
            r.DrawCircle       (cx, cy, rad * 1.5f, MakeColor(color.r, color.g, color.b, 50), 20);
            r.DrawCircle       (cx, cy, rad,         color,                                    20);
            r.DrawCircleOutline(cx, cy, rad,         MakeColor(255, 255, 255, 220), 1.0f, 20);
            if (isBonus)
            {
                r.DrawCircle(cx, cy, rad * 0.30f, MakeColor(255, 255, 255, 230), 12);
            }
            else
            {
                const float k = rad * 0.50f;
                r.DrawLine(cx - k, cy - k, cx + k, cy + k, MakeColor(255, 255, 255, 230), 2.0f);
                r.DrawLine(cx - k, cy + k, cx + k, cy - k, MakeColor(255, 255, 255, 230), 2.0f);
            }
        }

        // Retourne la hauteur RESERVEE pour l'illustration de @p kind.
        // DOIT etre identique a la hauteur effective dessinee par
        // DrawIllustration() pour que le layout soit stable peu importe
        // que la section soit visible ou hors-champ scroll.
        // Hauteur par cellule pour les illustrations a icone+label : 76 px
        // (au lieu de 56) pour laisser respirer le label sous l'icone.
        static constexpr float kIconRowH = 76.0f;   // px @ scale 1.0

        static float IllustrationHeight(IllustrationKind kind, float scale)
        {
            const float rowH    = 56.0f      * scale;
            const float iconH   = kIconRowH  * scale;
            const float pad     = 8.0f       * scale;
            switch (kind)
            {
            case kIllObstacles: return iconH * 2 + pad;         // grille 4x2 icone+label
            case kIllBonus:     return iconH + pad;
            case kIllMalus:     return iconH + pad;
            case kIllModes:     return rowH + pad;
            case kIllControls:  return rowH + pad + 12.0f * scale;
            case kIllDrops:     return rowH + pad + 10.0f * scale;
            case kIllParams:    return rowH + pad + 8.0f  * scale;
            case kIllObjectif:  return rowH + pad;
            case kIllNone:
            default:            return 0.0f;
            }
        }

        // Dessine l'illustration appropriee pour la section donnee.
        // Retourne la hauteur dessinee (= IllustrationHeight pour le meme kind).
        static float DrawIllustration(GLRenderer2D& r, FontAtlas& f,
                                      IllustrationKind kind,
                                      float x, float y, float w, float scale,
                                      float anim)
        {
            const float pulse01 = 0.5f + 0.5f * math::NkSin(anim * 6.0f);
            const float rowH    = 56.0f * scale;
            const float labelH  = 14.0f * scale;
            const float pad     = 8.0f  * scale;

            switch (kind)
            {
            case kIllObstacles:
            {
                // 8 mini-obstacles en grille 4x2. cellH = kIconRowH pour
                // laisser une marge claire entre l'icone et le label en-dessous.
                static const char* kLbl[8] = {
                    "MUR","PORTAIL","GRAVITE","AIMANT",
                    "MINE","FANTOME","COURANT","ETOILE"
                };
                const int cols = 4, rows = 2;
                const float cellW = w / cols;
                const float cellH = kIconRowH * scale;
                for (int i = 0; i < 8; ++i)
                {
                    const int col = i % cols, row = i / cols;
                    const float cx = x + cellW * (col + 0.5f);
                    // Icone en haut de la cellule, label en bas — separes par
                    // une marge confortable.
                    const float rad = math::NkMin(cellW, cellH) * 0.22f;
                    const float iconCY = y + cellH * row + rad + 6.0f * scale;
                    DrawObstacleIcon(r, i, cx, iconCY, rad, pulse01);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       cx, iconCY + rad + 10.0f * scale,
                                       kLbl[i], { 255, 255, 255, 200 });
                }
                return cellH * rows + pad;
            }

            case kIllBonus:
            {
                // 6 orbes bonus en ligne avec labels. Hauteur cellule = kIconRowH
                // pour donner de l'air entre orbe et label dessous.
                struct B { math::NkColor c; const char* lbl; };
                static const B kB[6] = {
                    { {255, 215,   0, 255}, "GEANT"     },
                    { {  0, 245, 255, 255}, "VITESSE"   },
                    { {255, 180,  60, 255}, "DBL PT"    },
                    { {  0, 200, 255, 255}, "BOUCLIER"  },
                    { { 80, 255, 100, 255}, "LENTE"     },
                    { {255, 255, 255, 255}, "SURPRISE"  },
                };
                const int cols = 6;
                const float cellW = w / cols;
                const float cellH = kIconRowH * scale;
                for (int i = 0; i < 6; ++i)
                {
                    const float cx = x + cellW * (i + 0.5f);
                    const float rad = math::NkMin(cellW, cellH) * 0.22f;
                    const float iconCY = y + rad + 6.0f * scale;
                    DrawOrbIcon(r, kB[i].c, true, cx, iconCY, rad);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       cx, iconCY + rad + 10.0f * scale,
                                       kB[i].lbl, { 255, 255, 255, 200 });
                }
                return cellH + pad;
            }

            case kIllMalus:
            {
                struct M { math::NkColor c; const char* lbl; };
                static const M kM[6] = {
                    { { 80,   0, 120, 255}, "AVEUGLE"  },
                    { {160, 160, 160, 255}, "MINI"     },
                    { {255,  60, 200, 255}, "INVERSE"  },
                    { {120, 200, 255, 255}, "GEL"      },
                    { {255,  60,  60, 255}, "RAPIDE"   },
                    { {255, 255, 255, 255}, "TELEPORT" },
                };
                const int cols = 6;
                const float cellW = w / cols;
                const float cellH = kIconRowH * scale;
                for (int i = 0; i < 6; ++i)
                {
                    const float cx = x + cellW * (i + 0.5f);
                    const float rad = math::NkMin(cellW, cellH) * 0.22f;
                    const float iconCY = y + rad + 6.0f * scale;
                    DrawOrbIcon(r, kM[i].c, false, cx, iconCY, rad);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       cx, iconCY + rad + 10.0f * scale,
                                       kM[i].lbl, { 255, 255, 255, 200 });
                }
                return cellH + pad;
            }

            case kIllModes:
            {
                // 4 mini-terrains : 2 paddles (positions varient selon mode).
                static const char* kLbl[4] = { "LOCAL", "VS IA", "IA VS IA", "RESEAU" };
                const int cols = 4;
                const float cellW = w / cols;
                const float pondW = cellW * 0.7f;
                const float pondH = rowH * 0.7f;
                for (int i = 0; i < 4; ++i)
                {
                    const float cx = x + cellW * (i + 0.5f);
                    const float cy = y + rowH * 0.5f - labelH * 0.3f;
                    const float pX = cx - pondW * 0.5f;
                    const float pY = cy - pondH * 0.5f;
                    // Terrain (rectangle outline)
                    r.DrawQuadOutline(pX, pY, pondW, pondH, { 0, 245, 255, 120 }, 1.0f);
                    // 2 raquettes
                    const float padW = 3.0f * scale;
                    const float padH = pondH * 0.40f;
                    const float padYL = pY + pondH * 0.30f;
                    const float padYR = pY + pondH * 0.30f;
                    r.DrawQuad(pX + 2.0f * scale,            padYL, padW, padH, { 0, 245, 255, 220 });
                    r.DrawQuad(pX + pondW - padW - 2.0f * scale, padYR, padW, padH, { 255, 107, 0, 220 });
                    // Petit cercle = ball au centre
                    r.DrawCircle(pX + pondW * 0.5f, pY + pondH * 0.5f, 2.0f * scale,
                                 { 255, 255, 255, 220 }, 10);
                    // Marqueurs IA pour mode 1 (droite) et 2 (les deux)
                    if (i == 1 || i == 2)
                    {
                        f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                     pX + pondW - 8.0f * scale, padYR - 6.0f * scale,
                                     "AI", { 255, 107, 0, 220 });
                    }
                    if (i == 2)
                    {
                        f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                     pX, padYL - 6.0f * scale,
                                     "AI", { 0, 245, 255, 220 });
                    }
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       cx, cy + pondH * 0.55f,
                                       kLbl[i], { 255, 255, 255, 200 });
                }
                return rowH + pad;
            }

            case kIllControls:
            {
                // Diagramme : W/S + UP/DOWN + doigt
                const float cellW = w / 3.0f;
                const float keyW  = 22.0f * scale;
                const float keyH  = 22.0f * scale;
                // Cellule 1 : P1 = W/S
                {
                    const float cx = x + cellW * 0.5f;
                    const float cy = y + rowH * 0.45f;
                    r.DrawQuad       (cx - keyW * 0.5f, cy - keyH - 2.0f * scale, keyW, keyH, { 0, 245, 255, 40 });
                    r.DrawQuadOutline(cx - keyW * 0.5f, cy - keyH - 2.0f * scale, keyW, keyH, { 0, 245, 255, 220 }, 1.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale, cx, cy - keyH + 3.0f * scale, "W", { 255, 255, 255, 240 });
                    r.DrawQuad       (cx - keyW * 0.5f, cy + 2.0f * scale, keyW, keyH, { 0, 245, 255, 40 });
                    r.DrawQuadOutline(cx - keyW * 0.5f, cy + 2.0f * scale, keyW, keyH, { 0, 245, 255, 220 }, 1.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale, cx, cy + 6.0f * scale, "S", { 255, 255, 255, 240 });
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale, cx, cy + keyH + 10.0f * scale, "P1", { 0, 245, 255, 220 });
                }
                // Cellule 2 : P2 = UP/DOWN
                {
                    const float cx = x + cellW * 1.5f;
                    const float cy = y + rowH * 0.45f;
                    r.DrawQuad       (cx - keyW * 0.5f, cy - keyH - 2.0f * scale, keyW, keyH, { 255, 107, 0, 40 });
                    r.DrawQuadOutline(cx - keyW * 0.5f, cy - keyH - 2.0f * scale, keyW, keyH, { 255, 107, 0, 220 }, 1.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale, cx, cy - keyH + 3.0f * scale, "^", { 255, 255, 255, 240 });
                    r.DrawQuad       (cx - keyW * 0.5f, cy + 2.0f * scale, keyW, keyH, { 255, 107, 0, 40 });
                    r.DrawQuadOutline(cx - keyW * 0.5f, cy + 2.0f * scale, keyW, keyH, { 255, 107, 0, 220 }, 1.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale, cx, cy + 6.0f * scale, "v", { 255, 255, 255, 240 });
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale, cx, cy + keyH + 10.0f * scale, "P2", { 255, 107, 0, 220 });
                }
                // Cellule 3 : Touch drag
                {
                    const float cx = x + cellW * 2.5f;
                    const float cy = y + rowH * 0.45f;
                    r.DrawCircle       (cx, cy - 6.0f * scale, 8.0f * scale, { 255, 255, 255, 60 }, 16);
                    r.DrawCircleOutline(cx, cy - 6.0f * scale, 8.0f * scale, { 255, 255, 255, 220 }, 1.5f, 16);
                    r.DrawLine(cx, cy + 2.0f * scale, cx, cy + 18.0f * scale, { 255, 255, 255, 220 }, 2.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale, cx, cy + keyH + 10.0f * scale, "DRAG", { 255, 255, 255, 220 });
                }
                return rowH + pad + 12.0f * scale;
            }

            case kIllDrops:
            {
                // 2 orbes qui descendent : 1 bonus 1 malus, fleches Y
                const float cellW = w / 2.0f;
                for (int i = 0; i < 2; ++i)
                {
                    const float cx = x + cellW * (i + 0.5f);
                    const float cy = y + rowH * 0.5f - labelH * 0.3f;
                    const float rad = 10.0f * scale;
                    const bool isB = (i == 0);
                    const math::NkColor c = isB ? MakeColor(80, 255, 100) : MakeColor(255, 60, 60);
                    DrawOrbIcon(r, c, isB, cx, cy, rad);
                    // Fleche vers le bas
                    r.DrawLine(cx, cy + rad + 3.0f * scale, cx, cy + rad + 16.0f * scale,
                               { 255, 255, 255, 180 }, 1.5f);
                    r.DrawLine(cx, cy + rad + 16.0f * scale, cx - 4.0f * scale, cy + rad + 12.0f * scale,
                               { 255, 255, 255, 180 }, 1.5f);
                    r.DrawLine(cx, cy + rad + 16.0f * scale, cx + 4.0f * scale, cy + rad + 12.0f * scale,
                               { 255, 255, 255, 180 }, 1.5f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       cx, cy + rad + 22.0f * scale,
                                       isB ? "BONUS" : "MALUS",
                                       { 255, 255, 255, 200 });
                }
                return rowH + pad + 10.0f * scale;
            }

            case kIllParams:
            {
                // Stepper minus/plus + chrono mm:ss
                const float cellW = w / 2.0f;
                const float keyW = 22.0f * scale;
                {
                    const float cx = x + cellW * 0.5f;
                    const float cy = y + rowH * 0.45f;
                    r.DrawQuad       (cx - 38.0f * scale, cy - 12.0f * scale, keyW, 24.0f * scale, { 0, 245, 255, 40 });
                    r.DrawQuadOutline(cx - 38.0f * scale, cy - 12.0f * scale, keyW, 24.0f * scale, { 0, 245, 255, 220 }, 1.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale, cx - 27.0f * scale, cy - 7.0f * scale, "-", { 0, 245, 255, 240 });
                    f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale, cx,              cy - 7.0f * scale, "11", { 255, 255, 255, 240 });
                    r.DrawQuad       (cx + 16.0f * scale, cy - 12.0f * scale, keyW, 24.0f * scale, { 0, 245, 255, 40 });
                    r.DrawQuadOutline(cx + 16.0f * scale, cy - 12.0f * scale, keyW, 24.0f * scale, { 0, 245, 255, 220 }, 1.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale, cx + 27.0f * scale, cy - 7.0f * scale, "+", { 0, 245, 255, 240 });
                    // Label decale vers le bas pour ne pas chevaucher le bouton +.
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale, cx, cy + 24.0f * scale, "SCORE", { 255, 255, 255, 200 });
                }
                {
                    const float cx = x + cellW * 1.5f;
                    const float cy = y + rowH * 0.45f;
                    // SubtitleSlot (plus petit que Headline) pour eviter que
                    // "01:30" chevauche le label "TEMPS" en-dessous.
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale, cx, cy - 8.0f * scale, "01:30", { 0, 245, 255, 240 });
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale, cx, cy + 24.0f * scale, "TEMPS", { 255, 255, 255, 200 });
                }
                return rowH + pad + 8.0f * scale;
            }

            case kIllObjectif:
            {
                // Gros score 21 + label "PREMIER A"
                const float cx = x + w * 0.5f;
                const float cy = y + rowH * 0.5f - labelH * 0.3f;
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   cx, cy - 22.0f * scale, "PREMIER A", { 255, 255, 255, 180 });
                f.DrawStringCenteredScaled(r, FontAtlas::DisplaySlot, scale,
                                   cx, cy - 8.0f * scale, "21", { 0, 245, 255, 240 });
                return rowH + pad;
            }

            default:
                return 0.0f;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        void RulesScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime = 0.0f;
            mEnterAnim = 0.0f;
            mScrollY = 0.0f;
            mMaxScroll = 0.0f;
            mDragActive = false;
            mDragWasScroll = false;
            mDragTouchId = -1;
            mMouseDown = false;
            // Accordion : toutes fermees au demarrage. L'user click sur la
            // section qu'il veut lire.
            mExpandedSection = -1;
            logger.Info("[Rules] OnEnter");
        }

        void RulesScene::OnUpdate(AppContext& /*ctx*/, float dt)
        {
            mTime += dt;
            mEnterAnim += dt / 0.3f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;
        }

        float RulesScene::ClampScroll(float v) const
        {
            if (v < 0.0f) return 0.0f;
            if (v > mMaxScroll) return mMaxScroll;
            return v;
        }
        bool RulesScene::HitTestBack(float sx, float sy) const
        {
            return sx >= mBackX && sx <= mBackX + mBackW
                && sy >= mBackY && sy <= mBackY + mBackH;
        }
        int RulesScene::HitTestTitle(float wx, float wy) const
        {
            if (wx < mTitleX || wx > mTitleX + mTitleW) return -1;
            for (int i = 0; i < mNumSections; ++i)
            {
                if (wy >= mTitleY[i] && wy <= mTitleY[i] + mTitleH)
                    return i;
            }
            return -1;
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnRender — header sticky + zone scrollable + scrollbar a droite.
        // ─────────────────────────────────────────────────────────────────────
        void RulesScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const float scale = GetUIScale(W, H);
            const float safeX = (float)ctx.safe.LeftX();
            const float safeW = (float)ctx.safe.SafeW();
            const float enterA = EaseOutCubic(mEnterAnim);

            // Fond degrade simple
            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);

            // ── Zone top reservee (header sticky) ──────────────────────────
            // Layout responsive en % viewport (2026-05-19) : on garde `scale`
            // UNIQUEMENT pour DrawStringScaled (bitmaps police) et epaisseurs
            // d'outline. Les positions / boites sont en fractions de W/H.
            mTopReserve    = Pct::H(H, 0.095f, 56.0f, 110.0f);
            mBottomReserve = Pct::H(H, 0.025f, 12.0f,  30.0f);
            const float scrollTop = (float)ctx.safe.TopY() + mTopReserve;
            const float scrollBot = (float)ctx.safe.TopY() + (float)ctx.safe.SafeH()
                                  - mBottomReserve;
            const float scrollH   = scrollBot - scrollTop;

            // Geometrie du header (sticky). Le rendu visible du header est
            // fait EN DERNIER pour passer par-dessus le contenu scrollable.
            mBackW = Pct::W(W, 0.10f,  70.0f, 140.0f);
            mBackH = Pct::H(H, 0.055f, 32.0f,  56.0f);
            mBackX = (float)ctx.safe.LeftX() + Pct::W(W, 0.012f, 8.0f, 24.0f);
            mBackY = (float)ctx.safe.TopY()  + Pct::H(H, 0.020f, 8.0f, 28.0f);

            // ── Contenu scrollable (accordion) ────────────────────────────
            // Chaque section est une barre cliquable. Si la section est
            // ouverte (mExpandedSection == s), on affiche son corps en
            // dessous. Sinon, seulement le titre. Click sur un titre
            // bascule l'etat (et ferme l'ancien si different).
            const float sideMargin = Pct::W(W, 0.020f, 12.0f, 36.0f);
            const float gridLeft = safeX + sideMargin;
            const float availW   = safeW - sideMargin * 2.0f;
            const float titleBarH = Pct::H(H, 0.060f, 36.0f,  64.0f);
            const float lineH     = Pct::H(H, 0.026f, 14.0f,  26.0f);
            const float sectionGap = Pct::H(H, 0.014f, 6.0f,  18.0f);

            mTitleX = gridLeft;
            mTitleW = availW;
            mTitleH = titleBarH;
            mNumSections = (kSectionsN > kMaxSections) ? kMaxSections : kSectionsN;

            // Paddings internes responsive.
            const float titlePadL  = Pct::W(W, 0.014f, 10.0f, 24.0f);
            const float chevPadR   = Pct::W(W, 0.024f, 16.0f, 36.0f);
            const float titleClipMargin = Pct::H(H, 0.006f, 3.0f, 8.0f);
            const float bodyTopGap = Pct::H(H, 0.010f, 4.0f, 12.0f);
            const float bodyEndGap = Pct::H(H, 0.014f, 6.0f, 14.0f);
            const float illGap     = Pct::H(H, 0.010f, 4.0f, 12.0f);
            const float bodyClip   = Pct::H(H, 0.028f, 12.0f, 28.0f);

            float worldY = Pct::H(H, 0.006f, 2.0f, 8.0f);
            for (int s = 0; s < mNumSections; ++s)
            {
                const RulesSection& sec = kSections[s];
                mTitleY[s] = worldY;
                const float screenTitleY = worldY - mScrollY + scrollTop;
                const bool  isOpen = (s == mExpandedSection);

                // Barre titre cliquable : fond + outline + texte + chevron.
                if (screenTitleY + titleBarH >= scrollTop - titleClipMargin
                 && screenTitleY <= scrollBot + titleClipMargin)
                {
                    math::NkColor bg = isOpen
                        ? math::NkColor{ 0, 245, 255, (uint8_t)(50  * enterA) }
                        : math::NkColor{ 0, 245, 255, (uint8_t)(20  * enterA) };
                    math::NkColor bd = isOpen
                        ? math::NkColor{ 0, 245, 255, (uint8_t)(220 * enterA) }
                        : math::NkColor{ 0, 245, 255, (uint8_t)(90  * enterA) };
                    r.DrawQuad       (gridLeft, screenTitleY, availW, titleBarH, bg);
                    r.DrawQuadOutline(gridLeft, screenTitleY, availW, titleBarH, bd,
                                      isOpen ? 2.0f : 1.5f);

                    // Texte titre, centre vertical
                    math::NkColor txt = isOpen
                        ? math::NkColor{ 255, 255, 255, (uint8_t)(255 * enterA) }
                        : math::NkColor{ 255, 255, 255, (uint8_t)(220 * enterA) };
                    f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                                 gridLeft + titlePadL,
                                 screenTitleY + titleBarH * 0.22f,
                                 sec.title, txt);

                    // Chevron a droite : "v" si ferme, "^" si ouvert.
                    const char* chev = isOpen ? "-" : "+";
                    f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                                 gridLeft + availW - chevPadR,
                                 screenTitleY + titleBarH * 0.22f,
                                 chev, txt);
                }
                worldY += titleBarH;

                // Corps : visible uniquement si cette section est ouverte.
                if (isOpen)
                {
                    const float bodyPad = Pct::W(W, 0.018f, 10.0f, 22.0f);
                    worldY += bodyTopGap;  // gap titre/corps

                    // Illustration au-dessus du texte : icones procedurales
                    // specifiques (obstacles, bonus, malus, modes, etc.).
                    // La hauteur reservee est DETERMINISTE (cf. IllustrationHeight)
                    // pour eviter tout decalage de layout entre visible / hors-champ.
                    const float illX = gridLeft + bodyPad;
                    const float illW = availW - bodyPad * 2.0f;
                    const float illH = IllustrationHeight(sec.illustration, scale);
                    const float illScreenY = worldY - mScrollY + scrollTop;
                    if (illH > 0.0f
                     && illScreenY + illH >= scrollTop - bodyClip
                     && illScreenY <= scrollBot + bodyClip)
                    {
                        DrawIllustration(r, f, sec.illustration,
                                         illX, illScreenY, illW, scale, mTime);
                    }
                    worldY += illH;
                    if (illH > 0.0f) worldY += illGap;  // gap entre illustration et corps

                    const char* p = sec.body;
                    while (*p)
                    {
                        char line[160];
                        int  li = 0;
                        while (*p && *p != '\n' && li < (int)sizeof(line) - 1)
                        {
                            line[li++] = *p++;
                        }
                        line[li] = '\0';
                        if (*p == '\n') ++p;
                        const float screenLY = worldY - mScrollY + scrollTop;
                        if (screenLY >= scrollTop - bodyClip
                         && screenLY <= scrollBot + bodyClip)
                        {
                            math::NkColor c = { 255, 255, 255, (uint8_t)(220 * enterA) };
                            f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                                         gridLeft + bodyPad, screenLY, line, c);
                        }
                        worldY += lineH;
                    }
                    worldY += bodyEndGap;  // gap fin de corps
                }
                worldY += sectionGap;
            }

            // ── Calcul mMaxScroll ──────────────────────────────────────────
            // worldY = hauteur totale du contenu. mMaxScroll = max(0, content - viewH).
            const float contentH = worldY;
            mMaxScroll = (contentH > scrollH) ? (contentH - scrollH) : 0.0f;
            if (mScrollY > mMaxScroll) mScrollY = mMaxScroll;

            // ── Scrollbar minimaliste a droite si overflow ─────────────────
            if (mMaxScroll > 0.5f)
            {
                const float sbW = Pct::W(W, 0.006f, 3.0f, 8.0f);
                const float sbX = safeX + safeW - sbW - Pct::W(W, 0.004f, 2.0f, 6.0f);
                const float sbInset = Pct::H(H, 0.004f, 1.0f, 4.0f);
                const float sbY = scrollTop + sbInset;
                const float sbH = scrollH - sbInset * 2.0f;
                r.DrawQuad(sbX, sbY, sbW, sbH, { 255, 255, 255, 16 });
                const float frac = scrollH / contentH;
                const float thumbMin = Pct::H(H, 0.030f, 16.0f, 36.0f);
                const float thumbH = math::NkMax(thumbMin, sbH * frac);
                const float thumbY = sbY
                                   + (sbH - thumbH) * (mScrollY / mMaxScroll);
                r.DrawQuad(sbX, thumbY, sbW, thumbH,
                           { 0, 245, 255, 180 });
            }

            // ── Header sticky OPAQUE (dessine en DERNIER pour masquer ce
            // qui scrolle dessous). Bande horizontale + RETOUR + titre.
            {
                const float hdrSep = Pct::H(H, 0.0035f, 1.5f, 4.0f);
                math::NkColor headerBg = theme::Dark();
                headerBg.a = 240;
                r.DrawQuad(0.0f, 0.0f, (float)W, scrollTop - hdrSep, headerBg);
                r.DrawQuad(0.0f, scrollTop - hdrSep, (float)W, hdrSep,
                           { 0, 245, 255, 120 });
                r.DrawQuad       (mBackX, mBackY, mBackW, mBackH, { 0, 245, 255, 30 });
                r.DrawQuadOutline(mBackX, mBackY, mBackW, mBackH, { 0, 245, 255, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mBackX + mBackW * 0.5f,
                                   mBackY + mBackH * 0.18f,
                                   "RETOUR", theme::Cyan());
                f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                                   (float)W * 0.5f,
                                   (float)ctx.safe.TopY() + Pct::H(H, 0.024f, 10.0f, 32.0f),
                                   "REGLES DU JEU", theme::White());
            }

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        void RulesScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            const float scrollStep = 60.0f;

            // Clavier : Echap = back ; PageUp/Down + fleches = scroll
            if (auto* k = ev.As<NkKeyPressEvent>())
            {
                switch (k->GetKey())
                {
                case NkKey::NK_ESCAPE:    ctx.scenes->Pop(); return;
                case NkKey::NK_PAGE_UP:
                case NkKey::NK_UP:        mScrollY = ClampScroll(mScrollY - scrollStep); return;
                case NkKey::NK_PAGE_DOWN:
                case NkKey::NK_DOWN:      mScrollY = ClampScroll(mScrollY + scrollStep); return;
                case NkKey::NK_HOME:      mScrollY = 0.0f; return;
                case NkKey::NK_END:       mScrollY = mMaxScroll; return;
                default: break;
                }
                return;
            }

            // Molette verticale (souris). Pour le scroll trackpad 2D unifie,
            // il faudrait aussi gerer NkMouseScrollEvent — pas necessaire ici.
            if (auto* w = ev.As<NkMouseWheelVerticalEvent>())
            {
                mScrollY = ClampScroll(mScrollY - w->GetDeltaY() * scrollStep);
                return;
            }

            // Souris : drag scroll OU tap RETOUR
            if (auto* mp = ev.As<NkMouseButtonPressEvent>())
            {
                if (mp->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    const float px = (float)mp->GetX();
                    const float py = (float)mp->GetY();
                    if (HitTestBack(px, py)) { ctx.scenes->Pop(); return; }
                    mMouseDown = true;
                    mDragActive = true;
                    mDragWasScroll = false;
                    mDragStartY = py;
                    mDragLastY  = py;
                }
                return;
            }
            if (auto* mm = ev.As<NkMouseMoveEvent>())
            {
                if (mMouseDown && mDragActive)
                {
                    const float py = (float)mm->GetY();
                    const float dy = py - mDragLastY;
                    if (math::NkFabs(py - mDragStartY) > 6.0f) mDragWasScroll = true;
                    mScrollY = ClampScroll(mScrollY - dy);
                    mDragLastY = py;
                }
                return;
            }
            if (auto* mr = ev.As<NkMouseButtonReleaseEvent>())
            {
                if (mr->GetButton() == NkMouseButton::NK_MB_LEFT && mMouseDown)
                {
                    mMouseDown = false;
                    mDragActive = false;
                    // Tap (pas de scroll significatif) sur un titre = toggle.
                    if (!mDragWasScroll)
                    {
                        const float px = (float)mr->GetX();
                        const float py = (float)mr->GetY();
                        const float scrollTop = (float)ctx.safe.TopY() + mTopReserve;
                        const float worldX = px;
                        const float worldY = (py - scrollTop) + mScrollY;
                        const int idx = HitTestTitle(worldX, worldY);
                        if (idx >= 0)
                        {
                            mExpandedSection = (mExpandedSection == idx) ? -1 : idx;
                        }
                    }
                }
                return;
            }

            // Touch : drag scroll + tap RETOUR (action sur TouchEnd seulement,
            // pour eviter que le TouchEnd "fuite" sur la scene suivante apres
            // un Pop premature sur TouchBegin).
            if (auto* tb = ev.As<NkTouchBeginEvent>())
            {
                if (tb->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = tb->GetTouch(0);
                    mDragActive    = true;
                    mDragWasScroll = false;
                    mDragStartY    = tp.clientY;
                    mDragLastY     = tp.clientY;
                    mDragTouchId   = (long long)tp.id;
                }
                return;
            }
            if (auto* tm = ev.As<NkTouchMoveEvent>())
            {
                for (uint32 i = 0; i < tm->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = tm->GetTouch(i);
                    if ((long long)tp.id != mDragTouchId) continue;
                    const float dy = tp.clientY - mDragLastY;
                    if (math::NkFabs(tp.clientY - mDragStartY) > 6.0f)
                        mDragWasScroll = true;
                    mScrollY = ClampScroll(mScrollY - dy);
                    mDragLastY = tp.clientY;
                }
                return;
            }
            if (auto* te = ev.As<NkTouchEndEvent>())
            {
                for (uint32 i = 0; i < te->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = te->GetTouch(i);
                    if ((long long)tp.id != mDragTouchId) continue;
                    if (!mDragWasScroll)
                    {
                        // 1. RETOUR ?
                        if (HitTestBack(tp.clientX, tp.clientY))
                        {
                            mDragActive = false; mDragTouchId = -1;
                            ctx.scenes->Pop();
                            return;
                        }
                        // 2. Tap sur titre = toggle accordion.
                        const float scrollTop = (float)ctx.safe.TopY() + mTopReserve;
                        const float worldX = tp.clientX;
                        const float worldY = (tp.clientY - scrollTop) + mScrollY;
                        const int idx = HitTestTitle(worldX, worldY);
                        if (idx >= 0)
                        {
                            mExpandedSection = (mExpandedSection == idx) ? -1 : idx;
                        }
                    }
                    mDragActive = false;
                    mDragTouchId = -1;
                }
                return;
            }
        }

    } // namespace pong
} // namespace nkentseu
