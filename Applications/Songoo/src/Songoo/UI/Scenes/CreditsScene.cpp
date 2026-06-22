// =============================================================================
// CreditsScene.cpp — Crédits Songo'o avec défilement 35 px/s
// Textes IDENTIQUES à l'original — palette africaine camerounaise
// =============================================================================

#include "CreditsScene.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/Render/FontAtlas.h"
#include "Songoo/UI/Theme.h"
#include "Songoo/UI/UIScale.h"
#include "Songoo/UI/SceneManager.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu { namespace songoo {

    struct CredLine {
        const char* text;
        float       sizeRel;  // relatif à baseSize
        bool        isTitle;
        uint8_t     r, g, b;
    };

    static const CredLine kLines[] = {
        { "SONGO'O",                                           3.0f, true,  255, 200,  40 },
        { "Jeu Traditionnel Camerounais",                      1.2f, false, 210, 160,  60 },
        { "",                                                  0.8f, false,   0,   0,   0 },
        { "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
                                                               0.8f, false, 140,  80,  20 },
        { "",                                                  0.8f, false,   0,   0,   0 },
        { "AUTEUR & DEVELOPPEUR",                              1.1f, true,  255, 160,  30 },
        { "NGIATE KAMNANG INGRID",                             1.3f, false, 255, 230, 150 },
        { "",                                                  0.7f, false,   0,   0,   0 },
        { "ETUDIANT INGENIEUR",                                1.0f, false, 200, 160,  80 },
        { "Ecole Nationale Superieure Polytechnique de Yaounde", 0.9f, false, 200, 160,  80 },
        { "ENSPY",                                             1.1f, false, 255, 185,  40 },
        { "Filiere : ARTS NUMERIQUE INGENIEUR",                1.0f, false, 200, 160,  80 },
        { "",                                                  0.8f, false,   0,   0,   0 },
        { "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
                                                               0.8f, false, 140,  80,  20 },
        { "",                                                  0.8f, false,   0,   0,   0 },
        { "REMERCIEMENTS",                                     1.2f, true,  255, 160,  30 },
        { "Ma chere famille",                                  1.0f, false, 230, 200, 130 },
        { "qui m'a toujours soutenue et encouragee",           0.9f, false, 180, 150, 100 },
        { "",                                                  0.7f, false,   0,   0,   0 },
        { "Mes encadreurs a RIHEN",                            1.0f, false, 230, 200, 130 },
        { "pour leur soutien permanent et infaillible",        0.9f, false, 180, 150, 100 },
        { "",                                                  0.7f, false,   0,   0,   0 },
        { "Departement Genie Informatique - ENSPY",            0.9f, false, 180, 150, 100 },
        { "",                                                  0.8f, false,   0,   0,   0 },
        { "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
                                                               0.8f, false, 140,  80,  20 },
        { "",                                                  0.8f, false,   0,   0,   0 },
        { "DEDICACE",                                          1.2f, true,  255, 160,  30 },
        { "A tous les batisseurs du numerique camerounais",    1.0f, false, 230, 200, 130 },
        { "et a la jeunesse africaine creatrice",              0.9f, false, 180, 150, 100 },
        { "",                                                  0.8f, false,   0,   0,   0 },
        { "VIVE LA CULTURE CAMEROUNAISE",                      1.1f, false, 255, 185,  40 },
        { "",                                                  1.0f, false,   0,   0,   0 },
        { "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80",
                                                               0.8f, false, 140,  80,  20 },
        { "",                                                  1.0f, false,   0,   0,   0 },
        { "(c) 2026 RIHEN UNIVERS",                            0.85f,false, 140, 110,  60 },
        { "Tous droits reserves",                              0.8f, false, 120,  90,  50 },
        { "",                                                  2.0f, false,   0,   0,   0 },
    };
    static const int kLineCount = (int)(sizeof(kLines) / sizeof(kLines[0]));

    void CreditsScene::OnEnter(AppContext& ctx) {
        mScrollY = 0.f;
        mTime    = 0.f;
        if (ctx.audio) {
            ctx.audio->PauseBgMusic();
            ctx.audio->PlayCreditMusic(0.10f);
        }
    }

    void CreditsScene::OnExit(AppContext& ctx) {
        if (ctx.audio) {
            ctx.audio->StopCreditMusic();
            ctx.audio->ResumeBgMusic();
        }
    }

    void CreditsScene::OnUpdate(AppContext& /*ctx*/, float dt) {
        mTime    += dt;
        // Vitesse de défilement IDENTIQUE à l'original : 35 px/s (base 900p)
        mScrollY += 35.f * dt;
    }

    void CreditsScene::OnEvent(AppContext& ctx, NkEvent& ev) {
        if (auto* k = ev.As<NkKeyPressEvent>())
            if (k->GetKey() == NkKey::NK_ESCAPE) ctx.scenes->Pop();
        if (auto* m = ev.As<NkMouseButtonPressEvent>())
            (void)m, ctx.scenes->Pop();
        if (auto* t = ev.As<NkTouchEndEvent>())
            (void)t, ctx.scenes->Pop();
    }

    void CreditsScene::OnRender(AppContext& ctx) {
        GLRenderer2D& r = *ctx.renderer;
        FontAtlas&    f = *ctx.font;
        const int W = ctx.viewportW, H = ctx.viewportH;
        const float cx = ctx.safe.SafeCX();
        const float scale = GetUIScale(W, H);

        r.Clear(0.031f, 0.016f, 0.008f, 1.f);
        r.Begin(W, H);

        const float baseSize = 22.f * scale;

        // ── Calcul hauteur totale pour reset scroll ──────────────────────────
        float totalH = 0.f;
        for (int i = 0; i < kLineCount; i++)
            totalH += kLines[i].sizeRel * baseSize * 1.6f;

        if (mScrollY > totalH + (float)H) mScrollY = 0.f;

        // ── Dessin des lignes ────────────────────────────────────────────────
        float y = (float)H - mScrollY;
        for (int i = 0; i < kLineCount; i++) {
            float lineH = kLines[i].sizeRel * baseSize * 1.6f;

            if (y + lineH > 0.f && y < (float)H &&
                kLines[i].text && kLines[i].text[0] != '\0')
            {
                // Fond doré pour les titres
                if (kLines[i].isTitle) {
                    r.DrawQuad(0.f, y - 4.f, (float)W, lineH + 8.f,
                        { (uint8_t)(kLines[i].r / 2),
                          (uint8_t)(kLines[i].g / 2),
                          0, 60 });
                }

                // Choisir le slot de police selon la taille
                int slot = FontAtlas::BodySlot;
                if (kLines[i].sizeRel >= 2.5f)  slot = FontAtlas::DisplaySlot;
                else if (kLines[i].sizeRel >= 1.5f) slot = FontAtlas::HeadlineSlot;
                else if (kLines[i].sizeRel >= 1.1f) slot = FontAtlas::SubtitleSlot;
                else if (kLines[i].sizeRel >= 0.85f) slot = FontAtlas::BodySlot;
                else slot = FontAtlas::SmallSlot;

                float textScale = kLines[i].sizeRel * 0.7f * scale;

                f.DrawStringCenteredScaled(r, slot, textScale,
                    cx, y + lineH * 0.18f,
                    kLines[i].text,
                    { kLines[i].r, kLines[i].g, kLines[i].b, 255 });
            }
            y += lineH;
        }

        // ── Bandes kente verticales (gauche + droite) ────────────────────────
        math::NkColor kente[3] = {
            { 180,  70,  15, 210 },  // terre cuite
            { 210, 160,  30, 210 },  // or
            {  30, 100,  40, 210 },  // vert forêt
        };
        float segH  = (float)H / 21.f;
        float barW  = 7.f * scale;
        for (int k = 0; k < 21; k++) {
            r.DrawQuad(0.f,           k * segH, barW, segH + 1.f, kente[k % 3]);
            r.DrawQuad((float)W-barW, k * segH, barW, segH + 1.f, kente[k % 3]);
        }

        // ── Bandeau kente bas ────────────────────────────────────────────────
        float segW2 = (float)W / 14.f;
        for (int k = 0; k < 14; k++) {
            r.DrawQuad(k * segW2, (float)H - 12.f * scale,
                       segW2 + 1.f, 12.f * scale,
                       kente[k % 3]);
        }

        // ── Bouton Retour (bas droite) ───────────────────────────────────────
        float bW = 130.f * scale, bH = 38.f * scale;
        float bX = (float)W - bW - 16.f * scale;
        float bY = (float)H - bH - 16.f * scale;
        r.DrawQuad(bX, bY, bW, bH, { 70, 30, 6, 200 });
        r.DrawQuadOutline(bX, bY, bW, bH, { 210, 160, 30, 200 }, 2.f);
        f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, 0.85f * scale,
            bX + bW * 0.5f, bY + bH * 0.18f,
            "RETOUR", { 255, 235, 180, 255 });

        r.End();
    }

}} // namespace nkentseu::songoo
