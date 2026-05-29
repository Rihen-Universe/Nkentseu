// =============================================================================
// RulesScene.cpp
// =============================================================================

#include "RulesScene.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/Render/FontAtlas.h"
#include "Songoo/UI/Theme.h"
#include "Songoo/UI/SceneManager.h"
#include "Songoo/UI/UIScale.h"
#include "Songoo/UI/ResponsiveLayout.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKMath/NkFunctions.h"
#include <cstring>

namespace nkentseu
{
    namespace songoo
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
            kIllBoard       = 1,    ///< Layout plateau Mancala
            kIllObjectif    = 2,    ///< Trophee victoire
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
              "LE PREMIER JOUEUR A CAPTURER LE PLUS DE GRAINS\n"
              "REMPORTE LA PARTIE. CHAQUE PIT DE MANCALA EST\n"
              "UN POINT (TOTAL DE 24 GRAINS AU DEPART).\n"
              "LE JEU SE TERMINE QUAND LES PITS D'UN JOUEUR\n"
              "SONT VIDES. LES GRAINS RESTANTS SONT COMPTES\n"
              "DANS LE MANCALA ADVERSE.",
              kIllObjectif },

            { "PLATEAU ET SETUP",
              "2 JOUEURS, 1 PLATEAU AVEC :\n"
              "  - 2 MANCALAS (GRANDS PITS AUX EXTREMITES)\n"
              "  - 6 PITS PAR JOUEUR (12 PITS REGULIERS TOTAL)\n"
              "  - 4 GRAINS PAR PIT AU DEPART (24 GRAINS TOTAL)\n"
              "\n"
              "DISPOSITION :\n"
              "  JOUEUR 2 (HAUT)  : 6 PITS + MANCALA A DROITE\n"
              "  JOUEUR 1 (BAS)   : MANCALA A GAUCHE + 6 PITS",
              kIllBoard },

            { "COMMENT JOUER - MOUVEMENT",
              "1. CHOISISSEZ UN PIT DE VOTRE COTE (NON VIDE).\n"
              "2. PRELEVEZ TOUS LES GRAINS DE CE PIT.\n"
              "3. DISTRIBUEZ LES GRAINS 1 PAR 1 EN TOURNANT\n"
              "   DANS LE SENS CONTRAIRE AUX AIGUILLES.\n"
              "4. SI LE DERNIER GRAIN TOMBE DANS VOTRE MANCALA,\n"
              "   VOUS JOUEZ UN COUP SUPPLEMENTAIRE.\n"
              "5. SI LE DERNIER GRAIN TOMBE DANS UN PIT VIDE\n"
              "   DE VOTRE COTE, VOUS CAPTUREZ CE GRAIN ET TOUS\n"
              "   LES GRAINS OPPOSES (DANS LE PIT FACE).",
              kIllNone },

            { "REGLES DE CAPTURE",
              "CAPTURE SIMPLE :\n"
              "  - LE DERNIER GRAIN TOMBE DANS UN PIT VIDE\n"
              "    DE VOTRE COTE DU PLATEAU.\n"
              "  - VOUS CAPTUREZ TOUS LES GRAINS DU PIT OPPOSE.\n"
              "  - LES GRAINS CAPTURES VONT DANS VOTRE MANCALA.\n"
              "\n"
              "PIT OPPOSE VIDE :\n"
              "  - SI LE PIT OPPOSE EST VIDE, AUCUNE CAPTURE\n"
              "    N'A LIEU. LE DERNIER GRAIN RESTE SIMPLEMENT\n"
              "    DANS VOTRE PIT VIDE.",
              kIllNone },

            { "FIN DE PARTIE",
              "LA PARTIE PREND FIN QUAND :\n"
              "  - LES 6 PITS D'UN JOUEUR SONT TOUS VIDES.\n"
              "  - LE JEU NE PEUT PLUS CONTINUER (AUCUN MOUVEMENT).\n"
              "\n"
              "COMPTAGE FINAL :\n"
              "  - LES GRAINS RESTANTS DANS LES PITS ACTIFS\n"
              "    SONT AJOUTES AU MANCALA DU JOUEUR PROPRIETAIRE.\n"
              "  - LE JOUEUR AVEC LE PLUS DE GRAINS GAGNE.",
              kIllNone },
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

        // ── Illustrations Mancala simples ─────────────────────────────────

        static float IllustrationHeight(IllustrationKind kind, float scale)
        {
            const float rowH    = 56.0f * scale;
            const float pad     = 8.0f  * scale;
            switch (kind)
            {
            case kIllBoard:     return rowH + pad;       // Plateau Mancala
            case kIllObjectif:  return rowH + pad;       // Trophee
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
            case kIllBoard:
            {
                // Plateau Mancala simple : 2 rangees de 6 pits + 2 mancalas
                const float cx = x + w * 0.5f;
                const float cy = y + rowH * 0.5f;
                const float boardW = w * 0.8f;
                const float boardH = rowH * 0.6f;
                const float boardX = cx - boardW * 0.5f;
                const float boardY = cy - boardH * 0.5f;

                // Outline plateau
                r.DrawQuadOutline(boardX, boardY, boardW, boardH, { 0, 245, 255, 180 }, 1.5f);

                // Rangee haut (Joueur 2 - Cyan)
                const float topPitY = boardY + boardH * 0.25f;
                const float pitR = 6.0f * scale;
                const float mancalaR = 10.0f * scale;
                const float pitsW = boardW - mancalaR * 2.0f - 4.0f * scale;
                const float pitSpacing = pitsW / 6.0f;

                // Mancala haut droit (Joueur 2)
                r.DrawCircle(boardX + boardW - mancalaR - 2.0f * scale, topPitY,
                            mancalaR, { 0, 245, 255, 50 }, 18);
                r.DrawCircleOutline(boardX + boardW - mancalaR - 2.0f * scale, topPitY,
                                   mancalaR, { 0, 245, 255, 200 }, 1.0f, 18);

                // 6 pits haut
                for (int i = 0; i < 6; ++i)
                {
                    const float px = boardX + mancalaR + 2.0f * scale + pitSpacing * (i + 0.5f);
                    r.DrawCircle(px, topPitY, pitR, { 0, 245, 255, 40 }, 12);
                    r.DrawCircleOutline(px, topPitY, pitR, { 0, 245, 255, 180 }, 0.8f, 12);
                }

                // Rangee bas (Joueur 1 - Orange)
                const float botPitY = boardY + boardH * 0.75f;

                // Mancala bas gauche (Joueur 1)
                r.DrawCircle(boardX + mancalaR + 2.0f * scale, botPitY,
                            mancalaR, { 255, 107, 0, 50 }, 18);
                r.DrawCircleOutline(boardX + mancalaR + 2.0f * scale, botPitY,
                                   mancalaR, { 255, 107, 0, 200 }, 1.0f, 18);

                // 6 pits bas
                for (int i = 0; i < 6; ++i)
                {
                    const float px = boardX + mancalaR + 2.0f * scale + pitSpacing * (i + 0.5f);
                    r.DrawCircle(px, botPitY, pitR, { 255, 107, 0, 40 }, 12);
                    r.DrawCircleOutline(px, botPitY, pitR, { 255, 107, 0, 180 }, 0.8f, 12);
                }

                return rowH + pad;
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

            // Fond Afro-warm (dark brown)
            r.Clear(0.08f, 0.06f, 0.04f, 1.0f);
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
                // Fond Afro-warm (dark brown)
                math::NkColor headerBg = { 20, 15, 10, 240 };
                r.DrawQuad(0.0f, 0.0f, (float)W, scrollTop - hdrSep, headerBg);
                // Separateur orange chaud au lieu de cyan
                r.DrawQuad(0.0f, scrollTop - hdrSep, (float)W, hdrSep,
                           { 255, 107, 0, 120 });
                // Bouton RETOUR style Afro-warm arrondi
                const float backRadius = 6.0f * scale;
                const float bbx0 = mBackX, bby0 = mBackY, bbx1 = mBackX + mBackW, bby1 = mBackY + mBackH;
                math::NkColor backBgC = { 255, 107, 0, 30 };
                math::NkColor backBorderC = { 255, 107, 0, 200 };

                // Fond principal
                r.DrawQuad(bbx0 + backRadius, bby0, mBackW - backRadius * 2.0f, mBackH, backBgC);
                r.DrawQuad(bbx0, bby0 + backRadius, backRadius, mBackH - backRadius * 2.0f, backBgC);
                r.DrawQuad(bbx1 - backRadius, bby0 + backRadius, backRadius, mBackH - backRadius * 2.0f, backBgC);

                // Coins arrondis
                r.DrawCircle(bbx0 + backRadius, bby0 + backRadius, backRadius, backBgC, 12);
                r.DrawCircle(bbx1 - backRadius, bby0 + backRadius, backRadius, backBgC, 12);
                r.DrawCircle(bbx0 + backRadius, bby1 - backRadius, backRadius, backBgC, 12);
                r.DrawCircle(bbx1 - backRadius, bby1 - backRadius, backRadius, backBgC, 12);

                // Bordure arrondie
                r.DrawQuadOutline(bbx0 + backRadius, bby0, mBackW - backRadius * 2.0f, mBackH,
                                 backBorderC, 1.5f);
                r.DrawQuadOutline(bbx0, bby0 + backRadius, mBackW, mBackH - backRadius * 2.0f,
                                 backBorderC, 1.5f);

                // Texte RETOUR
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mBackX + mBackW * 0.5f,
                                   mBackY + mBackH * 0.25f,
                                   "RETOUR", { 255, 255, 255, 220 });
                // Titre "REGLES DU JEU" avec shadow Afro-warm
                const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 2.0f);
                math::NkColor titleShadow = { 200, 150, 50, (uint8_t)((0.40f + 0.30f * pulse) * 255) };
                f.DrawStringShadowCentered(r, FontAtlas::HeadlineSlot,
                                   (float)W * 0.5f,
                                   (float)ctx.safe.TopY() + Pct::H(H, 0.024f, 10.0f, 32.0f),
                                   "REGLES DU JEU", theme::White(), titleShadow, 2);
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

    } // namespace songoo
} // namespace nkentseu
