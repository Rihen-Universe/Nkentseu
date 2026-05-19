// =============================================================================
// MainMenuScene.cpp
// -----------------------------------------------------------------------------
// Menu principal — 5 items, layout responsive.
//
// Decision de design (cf. MainMenuScene.h) : un seul bouton JOUER qui ouvre le
// flow per-partie (mode + difficulte IA + obstacles). OPTIONS = uniquement
// les reglages globaux (audio/graphiques/controles/reseau).
//
// Layout :
//   - paysage large (W >= ~720 et plus large que haut)  : 2 colonnes
//   - sinon (mobile portrait / petite fenetre)          : 1 colonne compacte
// Dimensions et taille de police derivees de la zone safe.
// =============================================================================

#include "MainMenuScene.h"
#include "SelectModeScene.h"
#include "OptionsScene.h"
#include "SupporterScene.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include <cmath>
#include <algorithm>

namespace nkentseu
{
    namespace pong
    {

        // ── Plateformes : le bouton QUITTER n'est conserve QUE sur desktop ──
        // Sur Android/iOS, fermer une app via un bouton interne est anti-pattern
        // (rejete par App Store, deconseille par Google : le systeme gere le
        // cycle de vie). Sur Web il est techniquement impossible de fermer un
        // onglet via window.close() pour une page utilisateur. On retire donc
        // l'item Item_Quit du menu sur ces plateformes.
        // PREREQUIS : Item_Quit doit etre le DERNIER de l'enum pour que la
        // soustraction kItemCount - 1 le retire proprement.
#if defined(NKENTSEU_PLATFORM_ANDROID) \
 || defined(NKENTSEU_PLATFORM_IOS) \
 || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        static constexpr bool kHasQuitItem = false;
#else
        static constexpr bool kHasQuitItem = true;
#endif

        static inline int VisibleItemCount()
        {
            return kHasQuitItem
                 ? (int)MainMenuScene::kItemCount
                 : (int)MainMenuScene::kItemCount - 1;
        }

        // ── Description du menu ──────────────────────────────────────────────
        struct MenuItemDesc
        {
            MainMenuScene::ItemId id;
            const char* title;
            const char* sub;
            int         sectionIdx;
            int         iconId;
            int         badge;        ///< 0=none, 1=NEW, 2=HOT
        };
        struct SectionDesc
        {
            const char* title;
            math::NkColor accent;
        };

        // 2 sections, 5 items au total.
        static const SectionDesc kSections[2] =
        {
            { "JEU",     { 0, 245, 255, 255 } }, // cyan
            { "SYSTEME", { 0, 245, 255, 255 } }, // cyan
        };
        static const MenuItemDesc kItems[MainMenuScene::kItemCount] =
        {
            { MainMenuScene::Item_Play,        "JOUER",            "CHOISIR LE MODE ET LANCER UNE PARTIE", 0, 0, 0 },
            { MainMenuScene::Item_Competition, "MODE COMPETITION", "TOURNOIS, PARIS, SPECTATEURS 1V1",     0, 7, 1 },
            { MainMenuScene::Item_Leaderboard, "CLASSEMENT",       "MEILLEURS JOUEURS — ONLINE + LOCAL",   1, 8, 0 },
            { MainMenuScene::Item_Options,     "OPTIONS",          "AUDIO, GRAPHIQUES, CONTROLES, RESEAU", 1, 6, 0 },
            { MainMenuScene::Item_Supporter,   "SUPPORTER",        "PARTAGER & SOUTENIR LE PROJET",        1, 5, 1 },
            { MainMenuScene::Item_Quit,        "QUITTER",          "FERMER L'APPLICATION",                 1, 9, 0 },
        };

        // ── Layout responsive — calcule en fonction de la zone safe ──────────
        struct MenuLayout
        {
            float safeX, safeY, safeW, safeH;
            float scale;       ///< Multiplicateur UI (UIScale.h)
            float padding;
            float gutter;
            // Mode 2 colonnes ? (sinon : single-column compact)
            bool  twoCols;
            // Panneau gauche
            float leftX, leftY, leftW;
            // Liste d'items
            float listX, listY, listW;
            float itemH;
            float itemGap;
            float sectionH;
            float sectionGap;
            // Tailles de police
            FontAtlas::SizeSlot slotLogo;
            FontAtlas::SizeSlot slotTitle;
            FontAtlas::SizeSlot slotBody;
            FontAtlas::SizeSlot slotSmall;
            // Mini-field
            bool  showMiniField;
            float miniFieldW;
            float miniFieldH;
            // Hauteurs precalculees pour le centrage vertical.
            float leftPanelH;   ///< hauteur totale du panneau gauche
            float rightListH;   ///< hauteur totale de la liste droite
        };

        // Hauteurs en pixels des slots de police prefabriques.
        static constexpr float kSlotPx[5] = { 14, 18, 28, 48, 72 };

        static float Clampf(float v, float lo, float hi)
        {
            if (v < lo) return lo;
            if (v > hi) return hi;
            return v;
        }

        static MenuLayout ComputeLayout(const AppContext& ctx)
        {
            MenuLayout L{};
            L.safeX = (float)ctx.safe.LeftX();
            L.safeY = (float)ctx.safe.TopY();
            L.safeW = (float)ctx.safe.SafeW();
            L.safeH = (float)ctx.safe.SafeH();

            // Scale unifie via UIScale (boost mobile inclus).
            const float scale = GetUIScale(ctx.viewportW, ctx.viewportH);
            L.scale = scale;
            // Padding et gutter scales (sans clamp upper artificiel — UIScale
            // a deja clampe le scale).
            L.padding = math::NkMax(12.0f, 24.0f * scale);
            L.gutter  = math::NkMax(16.0f, 32.0f * scale);

            // Breakpoint : 2 colonnes seulement si on a la place horizontale.
            // En dessous, on passe en single-column (mobile portrait, fenetre
            // etroite).
            const bool wideEnough = (L.safeW >= 720.0f && L.safeW > L.safeH * 1.05f);
            L.twoCols = wideEnough;

            // Polices : on monte d'un slot si on a de la place, on descend si
            // l'ecran est petit. Ces slots sont les pre-fab du FontAtlas
            // (Small 14 / Body 18 / Subtitle 28 / Headline 48 / Display 72).
            if (L.safeH >= 800.0f)
            {
                L.slotLogo  = FontAtlas::DisplaySlot;
                L.slotTitle = FontAtlas::SubtitleSlot;
                L.slotBody  = FontAtlas::BodySlot;
                L.slotSmall = FontAtlas::SmallSlot;
            }
            else if (L.safeH >= 500.0f)
            {
                L.slotLogo  = FontAtlas::HeadlineSlot;
                L.slotTitle = FontAtlas::SubtitleSlot;
                L.slotBody  = FontAtlas::BodySlot;
                L.slotSmall = FontAtlas::SmallSlot;
            }
            else
            {
                L.slotLogo  = FontAtlas::SubtitleSlot;
                L.slotTitle = FontAtlas::BodySlot;
                L.slotBody  = FontAtlas::BodySlot;
                L.slotSmall = FontAtlas::SmallSlot;
            }

            // Dimensions des cards : scale + clamp upper pour eviter overflow
            // vertical sur mobile haute densite. On calcule la hauteur dispo
            // pour la liste, puis on plafonne itemH pour que les 5 items + 2
            // titres de section + footer rentrent en hauteur.
            L.itemH      = math::NkMax(40.0f, 56.0f * scale);
            L.itemGap    = math::NkMax( 4.0f,  8.0f * scale);
            L.sectionH   = math::NkMax(14.0f, 20.0f * scale);
            L.sectionGap = math::NkMax( 2.0f,  4.0f * scale);

            const int nItems    = VisibleItemCount();
            const int nSections = 2;
            const float footerReserveH = 30.0f * scale;  // hint clavier en bas
            const float headerReserveH = (L.twoCols ? 0.0f : 200.0f);  // si 1 col
            const float availForList = L.safeH - footerReserveH - headerReserveH
                                     - L.padding * 2.0f;
            const float neededList = nSections * (L.sectionH + L.sectionGap + 4.0f)
                                   + nItems * (L.itemH + L.itemGap);
            if (neededList > availForList && nItems > 0)
            {
                const float overhead = nSections * (L.sectionH + L.sectionGap + 4.0f)
                                     + nItems * L.itemGap;
                const float maxItemH = (availForList - overhead) / nItems;
                if (maxItemH > 28.0f) L.itemH = math::NkMin(L.itemH, maxItemH);
            }

            if (L.twoCols)
            {
                L.leftW = Clampf(L.safeW * 0.30f, 220.0f, 360.0f);
                L.leftX = L.safeX + L.padding;
                L.listX = L.leftX + L.leftW + L.gutter;
                L.listW = L.safeW - L.padding * 2.0f - L.leftW - L.gutter;

                L.showMiniField = true;
                L.miniFieldW    = math::NkMin(L.leftW, 240.0f);
                L.miniFieldH    = L.miniFieldW * 120.0f / 180.0f;
            }
            else
            {
                L.leftX = L.safeX + L.padding;
                L.leftW = L.safeW - L.padding * 2.0f;
                L.listX = L.leftX;
                L.listW = L.leftW;
                L.showMiniField = (L.safeH >= 520.0f && L.safeW >= 360.0f);
                L.miniFieldW = L.showMiniField ? math::NkMin(L.leftW, 320.0f) : 0.0f;
                L.miniFieldH = L.showMiniField ? L.miniFieldW * 120.0f / 180.0f : 0.0f;
            }

            // ── Hauteurs precalculees pour centrage vertical ─────────────────
            const float logoPx  = kSlotPx[(int)L.slotLogo];
            const float smallPx = kSlotPx[(int)L.slotSmall];

            // Panneau gauche : logo + 4 + tagline + 14 + (miniField+12)? + credit
            L.leftPanelH = logoPx + 4.0f
                         + smallPx + 14.0f
                         + (L.showMiniField ? (L.miniFieldH + 12.0f) : 0.0f)
                         + smallPx;

            // Liste droite : 2 titres de section + 5 items + gaps
            L.rightListH = nSections * (L.sectionH + L.sectionGap)
                         + nItems    * (L.itemH    + L.itemGap)
                         + 4.0f      * (nSections - 1)  // petit gap inter-section
                         - L.itemGap;                    // pas de gap apres le dernier

            // ── Y de depart centre verticalement ─────────────────────────────
            // On reserve un footer (~28px) en bas pour les hints clavier.
            const float footerReserve = 28.0f;
            const float availH = L.safeH - footerReserve;

            if (L.twoCols)
            {
                // Chaque colonne centree sur sa propre hauteur.
                L.leftY = L.safeY + (availH - L.leftPanelH) * 0.5f;
                L.listY = L.safeY + (availH - L.rightListH) * 0.5f;
                // Garde-fous : ne pas remonter au dessus du padding superieur.
                if (L.leftY < L.safeY + L.padding) L.leftY = L.safeY + L.padding;
                if (L.listY < L.safeY + L.padding) L.listY = L.safeY + L.padding;
            }
            else
            {
                // Single-col : centrer le bloc total (header + liste).
                // L'espace inter-bloc est deja inclus dans leftPanelH (le
                // credit + 12px final fait office de respiration).
                const float totalH = L.leftPanelH + 12.0f + L.rightListH;
                float startY = L.safeY + (availH - totalH) * 0.5f;
                if (startY < L.safeY + L.padding) startY = L.safeY + L.padding;
                L.leftY = startY;
                // listY sera recalcule en OnRender (= headerEndY).
                L.listY = startY;
            }

            return L;
        }

        // ── Easing ───────────────────────────────────────────────────────────
        static float EaseOutCubic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        // ─────────────────────────────────────────────────────────────────────
        // Lifecycle
        // ─────────────────────────────────────────────────────────────────────
        void MainMenuScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime       = 0.0f;
            mFocusIndex = 0;
            mEnterAnim  = 0.0f;
            logger.Info("[MainMenu] OnEnter");
        }

        void MainMenuScene::OnUpdate(AppContext& /*ctx*/, float dt)
        {
            mTime += dt;
            mEnterAnim += dt / 0.5f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;
        }

        // ─────────────────────────────────────────────────────────────────────
        // Background : grille + scanlines + sweep cyan
        // ─────────────────────────────────────────────────────────────────────
        static void DrawBackgroundGrid(GLRenderer2D& r, int W, int H, float globalTime)
        {
            // Step de la grille en fonction de la taille (plus grand sur petit
            // ecran pour eviter le moire).
            const float gridStep = (W < 700) ? 32.0f : 40.0f;
            const math::NkColor lineCol = { 0, 245, 255, 12 };
            for (float x = 0.0f; x < (float)W; x += gridStep)
            {
                r.DrawQuad(x, 0.0f, 1.0f, (float)H, lineCol);
            }
            for (float y = 0.0f; y < (float)H; y += gridStep)
            {
                r.DrawQuad(0.0f, y, (float)W, 1.0f, lineCol);
            }
            const math::NkColor scan = { 0, 0, 0, 36 };
            for (float y = 0.0f; y < (float)H; y += 8.0f)
            {
                r.DrawQuad(0.0f, y, (float)W, 1.0f, scan);
            }
            const float sweepY = math::NkFmod(globalTime * 60.0f, (float)H + 200.0f) - 100.0f;
            r.DrawQuad(0.0f, sweepY, (float)W, 2.0f, { 0, 245, 255, 14 });
        }

        // ─────────────────────────────────────────────────────────────────────
        // Mini-field decoratif. Tailles passees en parametres pour suivre le
        // layout responsive.
        // ─────────────────────────────────────────────────────────────────────
        static void DrawMiniField(GLRenderer2D& r, FontAtlas& f,
                                  float x, float y, float w, float h,
                                  FontAtlas::SizeSlot scoreSlot)
        {
            // Le scale est implicite : la taille du mini-field est deja
            // calculee scaled par ComputeLayout. Pour le texte centre, on
            // utilise un scale local proportionnel a la hauteur.
            const float localScale = math::NkMax(1.0f, h / 120.0f);

            r.DrawQuad        (x, y, w, h, { 0, 245, 255, 8  });
            r.DrawQuadOutline (x, y, w, h, { 0, 245, 255, 64 }, 1.0f);

            // Ligne pointillee centrale
            const float midX = x + w * 0.5f;
            const float dashLen = 4.0f * localScale;
            const float dashStep = 8.0f * localScale;
            for (float yy = y + 4.0f * localScale; yy < y + h - 4.0f * localScale; yy += dashStep)
            {
                r.DrawQuad(midX - 0.5f, yy, 1.0f, dashLen, { 255, 255, 255, 50 });
            }

            // Cercle central
            const float ringR = math::NkMin(w, h) * 0.16f;
            r.DrawCircleOutline(midX, y + h * 0.5f, ringR,
                                { 255, 255, 255, 40 }, 1.0f, 32);

            // Score 0 : 0 (scaled selon la hauteur du mini-field)
            f.DrawStringCenteredScaled(r, scoreSlot, localScale,
                               midX, y + h * 0.5f - 14.0f * localScale,
                               "0 : 0", { 255, 255, 255, 220 });

            // Paddles
            const float padH = math::NkMin(h * 0.32f, 40.0f * localScale);
            const float padW = 4.0f * localScale;
            const float padY = y + (h - padH) * 0.5f;
            r.DrawQuad(x + 6.0f * localScale,             padY, padW, padH, theme::Cyan());
            r.DrawQuad(x + w - 6.0f * localScale - padW,  padY, padW, padH, theme::Orange());
        }

        // ─────────────────────────────────────────────────────────────────────
        // Icones procedurales — chaque case dessine une icone dans un carre.
        // ─────────────────────────────────────────────────────────────────────
        static void DrawIcon(GLRenderer2D& r, int iconId,
                             float boxX, float boxY, float boxSize,
                             math::NkColor c)
        {
            const float cx = boxX + boxSize * 0.5f;
            const float cy = boxY + boxSize * 0.5f;
            const float hb = boxSize * 0.5f;
            switch (iconId)
            {
            case 0:
            {
                // Manette : pour le bouton JOUER. On stylise une manette
                // simple avec 2 boutons + d-pad.
                const float w = boxSize * 0.8f;
                const float h = boxSize * 0.5f;
                r.DrawQuadOutline(cx - w * 0.5f, cy - h * 0.5f, w, h, c, 1.5f);
                r.DrawCircle     (cx + w * 0.25f, cy,         2.0f, c, 8);
                r.DrawCircle     (cx + w * 0.35f, cy - 4.0f,  2.0f, c, 8);
                r.DrawQuad       (cx - w * 0.4f,  cy - 1.0f,  8.0f, 2.0f, c);
                r.DrawQuad       (cx - w * 0.35f, cy - 4.0f,  2.0f, 8.0f, c);
                break;
            }
            case 6:
            {
                // Engrenage (Options)
                r.DrawCircleOutline(cx, cy, hb * 0.5f, c, 1.5f, 32);
                r.DrawCircle       (cx, cy, 2.0f, c, 8);
                const int N = 6;
                for (int i = 0; i < N; ++i)
                {
                    const float a = 6.28318f * i / N;
                    const float rx = cx + math::NkCos(a) * hb * 0.65f;
                    const float ry = cy + math::NkSin(a) * hb * 0.65f;
                    r.DrawQuad(rx - 1.5f, ry - 1.5f, 3.0f, 3.0f, c);
                }
                break;
            }
            case 7:
            {
                // Trophee (Mode Competition)
                const float cupW = boxSize * 0.45f;
                const float cupH = boxSize * 0.40f;
                r.DrawQuadOutline(cx - cupW * 0.5f, cy - cupH * 0.6f, cupW, cupH, c, 1.5f);
                r.DrawLine(cx - cupW * 0.5f, cy - cupH * 0.4f,
                           cx - cupW * 0.7f, cy - cupH * 0.1f, c, 1.5f);
                r.DrawLine(cx + cupW * 0.5f, cy - cupH * 0.4f,
                           cx + cupW * 0.7f, cy - cupH * 0.1f, c, 1.5f);
                r.DrawQuad(cx - 1.5f, cy + cupH * 0.4f, 3.0f, 4.0f, c);
                r.DrawQuad(cx - cupW * 0.4f, cy + cupH * 0.4f + 4.0f, cupW * 0.8f, 2.0f, c);
                break;
            }
            case 8:
            {
                // Barres histogramme (Classement)
                const float bw = boxSize * 0.16f;
                const float baseY = cy + hb * 0.5f;
                const float maxH = boxSize * 0.5f;
                for (int i = 0; i < 3; ++i)
                {
                    const float bh = maxH * (0.35f + 0.30f * i);
                    const float bx = cx - bw * 2.0f + i * bw * 1.4f;
                    r.DrawQuad(bx, baseY - bh, bw, bh, c);
                }
                break;
            }
            case 9:
            default:
            {
                // Croix (Quitter)
                r.DrawLine(cx - hb * 0.45f, cy - hb * 0.45f,
                           cx + hb * 0.45f, cy + hb * 0.45f, c, 2.0f);
                r.DrawLine(cx + hb * 0.45f, cy - hb * 0.45f,
                           cx - hb * 0.45f, cy + hb * 0.45f, c, 2.0f);
                break;
            }
            }
        }

        // ── Badge (NEW / HOT) — texte scaled ────────────────────────────────
        static void DrawBadge(GLRenderer2D& r, FontAtlas& f,
                              FontAtlas::SizeSlot slot, float scale,
                              float x, float y, const char* text,
                              math::NkColor accent)
        {
            const float padX = 6.0f * scale;
            const float textW = f.MeasureWidthScaled(slot, scale, text);
            const float w = textW + padX * 2.0f;
            const float h = 16.0f * scale;
            math::NkColor bg = accent; bg.a = 50;
            r.DrawQuad       (x, y, w, h, bg);
            r.DrawQuadOutline(x, y, w, h, accent, 1.0f);
            f.DrawStringScaled(r, slot, scale, x + padX, y + 1.0f * scale, text, accent);
        }

        // ─────────────────────────────────────────────────────────────────────
        // DrawItemCard — dessine un item complet. Retourne la nouvelle Y.
        // ─────────────────────────────────────────────────────────────────────
        static float DrawItemCard(GLRenderer2D& r, FontAtlas& f,
                                  const MenuLayout& L,
                                  int itemIndex, float baseY,
                                  bool focused, float globalTime,
                                  float alphaItem)
        {
            const MenuItemDesc& it = kItems[itemIndex];
            const float slideX = (1.0f - alphaItem) * 40.0f;

            const float cardX = L.listX + slideX;
            const float cardY = baseY;
            const float cardW = L.listW;
            const float cardH = L.itemH;

            // Couleur d'accent (Quitter = rouge)
            math::NkColor accent = kSections[it.sectionIdx].accent;
            if (it.id == MainMenuScene::Item_Quit)
            {
                accent = { 255, 64, 64, 255 };
            }

            // Background + border
            math::NkColor bg   = { 0, 245, 255, 12 };
            math::NkColor bord = { 0, 245, 255, 24 };
            if (focused)
            {
                bg   = accent; bg.a   = 32;
                bord = accent; bord.a = 90;
                const float pulse = 0.5f + 0.5f * math::NkSin(globalTime * 4.0f);
                bg.a = static_cast<uint8>(32 + 24 * pulse);
            }
            bg.a   = static_cast<uint8>(bg.a   * alphaItem);
            bord.a = static_cast<uint8>(bord.a * alphaItem);
            r.DrawQuad       (cardX, cardY, cardW, cardH, bg);
            r.DrawQuadOutline(cardX, cardY, cardW, cardH, bord, 1.0f);

            if (focused)
            {
                math::NkColor lb = accent;
                lb.a = static_cast<uint8>(255 * alphaItem);
                r.DrawQuad(cardX, cardY, 3.0f, cardH, lb);
            }

            // Icone
            const float iconBox = math::NkMin(cardH * 0.7f, 44.0f);
            const float iconX   = cardX + 14.0f;
            const float iconY   = cardY + (cardH - iconBox) * 0.5f;
            math::NkColor iconCol = accent;
            iconCol.a = static_cast<uint8>(220 * alphaItem);
            DrawIcon(r, it.iconId, iconX, iconY, iconBox, iconCol);

            // Texte scaled
            const float textX = iconX + iconBox + 14.0f * L.scale;
            math::NkColor titleCol = theme::White(); titleCol.a = static_cast<uint8>(245 * alphaItem);
            math::NkColor subCol   = { 255, 255, 255, 100 }; subCol.a = static_cast<uint8>(100 * alphaItem);
            f.DrawStringScaled(r, L.slotTitle, L.scale,
                              textX, cardY + cardH * 0.18f,
                              it.title, titleCol);
            f.DrawStringScaled(r, L.slotSmall, L.scale,
                              textX, cardY + cardH * 0.62f,
                              it.sub, subCol);

            // Badge optionnel
            if (it.badge != 0)
            {
                const char* btext = (it.badge == 1) ? "NOUVEAU" : "EN LIGNE";
                math::NkColor bcol = (it.badge == 1)
                    ? math::NkColor{ 255, 215,   0, 255 }
                    : math::NkColor{ 255, 107,   0, 255 };
                bcol.a = static_cast<uint8>(255 * alphaItem);
                const float bw = f.MeasureWidthScaled(L.slotSmall, L.scale, btext)
                                + 12.0f * L.scale;
                const float bx = cardX + cardW - bw - 30.0f * L.scale;
                const float by = cardY + (cardH - 16.0f * L.scale) * 0.5f;
                DrawBadge(r, f, L.slotSmall, L.scale, bx, by, btext, bcol);
            }

            // Fleche droite scaled
            math::NkColor arrowCol = accent;
            arrowCol.a = static_cast<uint8>((focused ? 255 : 120) * alphaItem);
            f.DrawStringScaled(r, L.slotTitle, L.scale,
                              cardX + cardW - 20.0f * L.scale,
                              cardY + cardH * 0.20f, ">", arrowCol);

            return cardY + L.itemH + L.itemGap;
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnRender
        // ─────────────────────────────────────────────────────────────────────
        void MainMenuScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;

            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);
            DrawBackgroundGrid(r, W, H, mTime);

            const MenuLayout L = ComputeLayout(ctx);
            const float enterA = EaseOutCubic(mEnterAnim);

            // ── HEADER (panneau gauche en 2 cols, en haut en 1 col) ─────────
            // Logo PONG + tagline + (eventuellement) mini-field + credit Rihen.
            float headerEndY = 0.0f;
            {
                float hx = L.leftX;
                float hy = L.leftY + 4.0f;

                f.DrawStringShadowScaled(r, L.slotLogo, L.scale,
                                 hx, hy, "PONG",
                                 theme::White(), theme::Cyan(), 3);
                const float logoPx = kSlotPx[(int)L.slotLogo];
                hy += logoPx * L.scale + 4.0f * L.scale;

                f.DrawStringScaled(r, L.slotSmall, L.scale,
                             hx, hy, "ULTRA ARENA EDITION", theme::Cyan());
                hy += kSlotPx[(int)L.slotSmall] * L.scale + 14.0f * L.scale;

                if (L.showMiniField)
                {
                    DrawMiniField(r, f, hx, hy, L.miniFieldW, L.miniFieldH,
                                  L.slotTitle);
                    hy += L.miniFieldH + 12.0f * L.scale;
                }

                f.DrawStringScaled(r, L.slotSmall, L.scale,
                             hx, hy, "CONCU PAR RIHEN", { 255, 255, 255, 80 });
                hy += kSlotPx[(int)L.slotSmall] * L.scale + 12.0f * L.scale;
                headerEndY = hy;
            }

            // ── LISTE D'ITEMS ───────────────────────────────────────────────
            // En 1 colonne, la liste commence sous le header. En 2 colonnes,
            // elle suit son propre listY (en haut a droite).
            float ry = L.twoCols ? L.listY : headerEndY;

            // Memorise la geometrie des cards pour le hit-test (touch / clic).
            mCardListX   = L.listX;
            mCardListW   = L.listW;
            mCardItemH   = L.itemH;
            mCardItemGap = L.itemGap;

            int prevSection = -1;
            const int visibleCount = VisibleItemCount();
            for (int i = 0; i < visibleCount; ++i)
            {
                const MenuItemDesc& it = kItems[i];

                // Titre de section (skip si meme que le precedent)
                if (it.sectionIdx != prevSection)
                {
                    if (prevSection >= 0) ry += 4.0f * L.scale;
                    f.DrawStringScaled(r, L.slotSmall, L.scale,
                                 L.listX, ry,
                                 kSections[it.sectionIdx].title,
                                 { 255, 255, 255, 90 });
                    ry += L.sectionH + L.sectionGap;
                    prevSection = it.sectionIdx;
                }

                // Anim d'entree par item
                const float perItemDelay = 0.06f * (float)i;
                float itemAnim = mEnterAnim - perItemDelay;
                if (itemAnim < 0.0f) itemAnim = 0.0f;
                if (itemAnim > 1.0f) itemAnim = 1.0f;
                itemAnim = EaseOutCubic(itemAnim) * enterA;

                // Stocke Y pour hit-test
                if (i < 5) mCardItemYs[i] = ry;

                const bool focused = (i == mFocusIndex);
                ry = DrawItemCard(r, f, L, i, ry, focused, mTime, itemAnim);
            }

            // ── Footer : hints clavier (toujours visible en bas) ────────────
            const math::NkColor hintCol = { 255, 255, 255, 90 };
            f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, L.scale,
                               L.safeX + L.safeW * 0.5f,
                               L.safeY + L.safeH - 18.0f * L.scale,
                               "FLECHES : NAV       ENTER : SELECT       ECHAP : QUITTER       TAP / CLIC : SELECT",
                               hintCol);

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        // HitTestItem — retourne l'index de l'item sous (px, py) ou -1.
        // ─────────────────────────────────────────────────────────────────────
        int MainMenuScene::HitTestItem(float px, float py) const
        {
            if (px < mCardListX || px > mCardListX + mCardListW) return -1;
            const int vc = VisibleItemCount();
            for (int i = 0; i < vc && i < 5; ++i)
            {
                const float y0 = mCardItemYs[i];
                const float y1 = y0 + mCardItemH;
                if (py >= y0 && py <= y1) return i;
            }
            return -1;
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnEvent — clavier + touch + souris
        // ─────────────────────────────────────────────────────────────────────
        void MainMenuScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            // Clavier
            if (auto* kp = ev.As<NkKeyPressEvent>())
            {
                const NkKey k = kp->GetKey();
                if (k == NkKey::NK_DOWN)
                {
                    mFocusIndex = (mFocusIndex + 1) % VisibleItemCount();
                }
                else if (k == NkKey::NK_UP)
                {
                    mFocusIndex = (mFocusIndex - 1 + VisibleItemCount()) % VisibleItemCount();
                }
                else if (k == NkKey::NK_ENTER || k == NkKey::NK_NUMPAD_ENTER
                      || k == NkKey::NK_SPACE)
                {
                    ActivateItem(ctx, static_cast<ItemId>(mFocusIndex));
                }
                else if (k == NkKey::NK_ESCAPE)
                {
                    ActivateItem(ctx, Item_Quit);
                }
                return;
            }

            // Souris : clic gauche -> hit-test + activate
            if (auto* mp = ev.As<NkMouseButtonPressEvent>())
            {
                if (mp->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    const int idx = HitTestItem((float)mp->GetX(),
                                                (float)mp->GetY());
                    if (idx >= 0)
                    {
                        mFocusIndex = idx;
                        ActivateItem(ctx, static_cast<ItemId>(idx));
                    }
                }
                return;
            }

            // Touch : sur tap (Begin) -> hit-test + activate
            if (auto* tb = ev.As<NkTouchBeginEvent>())
            {
                if (tb->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = tb->GetTouch(0);
                    const int idx = HitTestItem(tp.clientX, tp.clientY);
                    if (idx >= 0)
                    {
                        mFocusIndex = idx;
                        ActivateItem(ctx, static_cast<ItemId>(idx));
                    }
                }
                return;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // ActivateItem
        // ─────────────────────────────────────────────────────────────────────
        void MainMenuScene::ActivateItem(AppContext& ctx, ItemId item)
        {
            switch (item)
            {
            case Item_Quit:
                logger.Info("[MainMenu] Quit requested");
                if (ctx.quitRequested != nullptr) *ctx.quitRequested = true;
                break;
            case Item_Play:
                // Ouvre le flow de selection : SelectMode -> Gameplay
                // (futur : SelectDifficulty si IA -> SelectObstacles).
                logger.Info("[MainMenu] Push SelectModeScene");
                ctx.scenes->Push(new SelectModeScene());
                break;
            case Item_Options:
                // Ouvre le hub OPTIONS : liste de categories (Aide, Audio,
                // Graphiques, Controles, Reseau). Aide est la seule active
                // aujourd'hui — les autres sont stubs jusqu'a implementation.
                logger.Info("[MainMenu] Push OptionsScene");
                ctx.scenes->Push(new OptionsScene());
                break;
            case Item_Supporter:
                logger.Info("[MainMenu] Push SupporterScene");
                ctx.scenes->Push(new SupporterScene());
                break;
            // Sous-ecrans pas encore implementes — on log pour ne pas creer
            // de stubs redondants. (Reprise prevue : CompetitionScene,
            // LeaderboardScene.)
            case Item_Competition:
            case Item_Leaderboard:
            default:
                logger.Info("[MainMenu] Item activated (TODO): {0}",
                            kItems[item].title);
                break;
            }
        }

    } // namespace pong
} // namespace nkentseu
