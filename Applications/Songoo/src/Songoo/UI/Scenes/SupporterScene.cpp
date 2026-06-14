// =============================================================================
// SupporterScene.cpp
// -----------------------------------------------------------------------------
// Ecran SUPPORTER complet :
//   - Storytelling (vision de l'ecosysteme NKENTSEU : NOGE engine, JENGA build,
//     et les jeux comme Pong)
//   - Bouton PARTAGER (intent natif a brancher)
//   - 6 montants preset de don (USD)
//   - PAIEMENT DIRECT : identifiants Orange Money / Mobile Money / Wave / PayPal
//     que le joueur peut copier pour faire un virement manuel
//   - RESEAUX SOCIAUX : 5 cards (LinkedIn, GitHub, Facebook, Instagram, X)
//
// Tout est sur une zone scrollable verticalement (le contenu deborde sur mobile
// portrait). Header (RETOUR + titre) est sticky en haut.
//
// Tous les liens et numeros sont des PLACEHOLDERS — a remplacer par les vrais
// identifiants de l'user avant publication.
// =============================================================================

#include "SupporterScene.h"
#include "Songoo/Render/GLRenderer2D.h"
#include "Songoo/Render/FontAtlas.h"
#include "Songoo/UI/Theme.h"
#include "Songoo/UI/SceneManager.h"
#include "Songoo/UI/UIScale.h"
#include "Songoo/UI/ResponsiveLayout.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKWindow/Core/NkLauncher.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKMath/NkFunctions.h"
#include <cstdio>
#include <cstring>

namespace nkentseu
{
    namespace songoo
    {

        // ── Configuration externe ────────────────────────────────────────────
        // 2026-05-19 : pas de site dedie Rihen Universe pour l'instant. On
        // utilise la page LinkedIn officielle comme fallback pour le partage
        // et la donation (en attendant un domaine propre / page Ko-fi / etc.).
        static constexpr const char* kDonateBaseUrl =
            "https://www.linkedin.com/company/rihen-universe";
        static constexpr const char* kShareText =
            "Decouvre Pong Ultra Arena — un Pong avec obstacles, bonus, malus et IA.";
        static constexpr const char* kShareUrl  =
            "https://www.linkedin.com/company/rihen-universe";

        // Identifiants paiement directs. Mis a jour 2026-05-19 avec les
        // vraies coordonnees de l'auteur (Cameroun, +237). Visa/MC et PayPal
        // a venir bientot via un agregateur (Ko-fi / Stripe / CinetPay).
        struct PayId { const char* label; const char* value; math::NkColor accent; };
        static const PayId kPayIds[] = {
            { "ORANGE MONEY",     "+237 693 76 17 73",            { 255, 107,   0, 255 } },
            { "MTN MOBILE MONEY", "+237 673 48 26 14",            { 255, 215,   0, 255 } },
            { "VISA / MC",        "BIENTOT (VIA AGREGATEUR)",     { 204, 119, 255, 255 } },
        };
        static constexpr int kPayIdsN = (int)(sizeof(kPayIds) / sizeof(kPayIds[0]));

        // Reseaux sociaux. Mis a jour 2026-05-19 avec les comptes officiels
        // Rihen Universe. Les vraies icones sont stockees dans
        // Resources/Pong/Textures/socials/ et chargees dans OnEnter.
        // Le champ `badge` reste un fallback texte en cas d'echec de chargement
        // (assets manquants, decodeur SVG defaillant, etc.) — garantit que les
        // boutons restent reconnaissables meme sans assets.
        // Note : "linkdin.png" est volontaire (typo du fichier source). Le
        // github en SVG est gere par NkSVGCodec via NkImage::Load.
        struct Social { const char* name; const char* handle; const char* url;
                         math::NkColor accent; const char* badge;
                         const char* iconPath; };
        static const Social kSocials[SupporterScene::kSocialCount] = {
            { "LINKEDIN",    "@rihen-universe",
              "https://www.linkedin.com/company/rihen-universe",
              { 10, 102, 194, 255 }, "in",
              "Resources/Pong/Textures/socials/linkdin.png" },
            { "GITHUB",      "@RihenUniverse",
              "https://github.com/RihenUniverse",
              { 60,  70,  85, 255 }, "GH",
              "Resources/Pong/Textures/socials/github.svg" },
            { "FACEBOOK",    "@Rihen-Universe",
              "https://www.facebook.com/p/Rihen-Universe-61575051351258/",
              { 24, 119, 242, 255 }, "f",
              "Resources/Pong/Textures/socials/facebook.png" },
            { "INSTAGRAM",   "@rihen.universe",
              "https://www.instagram.com/rihen.universe/",
              { 220, 50, 130, 255 }, "IG",
              "Resources/Pong/Textures/socials/instagram.png" },
            { "X (TWITTER)", "@RihenUniverse",
              "https://x.com/RihenUniverse",
              {  30,  30,  30, 255 }, "X",
              "Resources/Pong/Textures/socials/x.png" },
        };

        // ── Montants preset (USD) ───────────────────────────────────────────
        static const int kAmountUSD[SupporterScene::kAmountCount] = { 0, 1, 2, 5, 10 };

        // ── Storytelling ────────────────────────────────────────────────────
        // 6 lignes, ton emotionnel mais grounded. Ecosysteme = combat pour
        // l'independance technologique africaine.
        static const char* kStory[] = {
            "PONG ULTRA ARENA EST PLUS QU'UN JEU.",
            "C'EST UNE PIECE D'UN ECOSYSTEME EN CONSTRUCTION :",
            "NOGE FAIT TOURNER LE MOTEUR.",
            "JENGA COMPILE TOUT, PARTOUT.",
            "NKENTSEU RELIE LES PIECES.",
            "ET LES JEUX RACONTENT L'HISTOIRE.",
            "",
            "CHAQUE LIGNE DE CODE EST UN PAS VERS",
            "L'INDEPENDANCE TECHNOLOGIQUE.",
            "TON PARTAGE, TA PIECE OU TON SOURIRE",
            "EST LE CARBURANT DE CE COMBAT.",
            "",
            "ON BATIT L'EVOLUTION. AVEC TOI.",
        };
        static constexpr int kStoryN = (int)(sizeof(kStory) / sizeof(kStory[0]));

        static float EaseOutCubic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        // ─────────────────────────────────────────────────────────────────────
        void SupporterScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime = 0.0f;
            mEnterAnim = 0.0f;
            mActiveTouchId = -1;
            mScrollY = 0.0f;
            mMaxScroll = 0.0f;
            mDragActive = false;
            mDragWasScroll = false;
            mDragTouchId = -1;
            mMouseDown = false;

            // Charge les icones reseaux sociaux. Synchrone : 5 fichiers tres
            // legers (icones < 50 ko chacun). Si un fichier manque, on garde
            // le badge texte en fallback (mSocialLoaded[i] reste a false).
            for (int i = 0; i < (int)kSocialCount; ++i)
            {
                mSocialLoaded[i] = mSocialIcons[i].LoadFromFile(kSocials[i].iconPath);
                if (!mSocialLoaded[i])
                {
                    logger.Warn("[Supporter] icone manquante : {0} -> fallback badge texte",
                                kSocials[i].iconPath);
                }
            }
            logger.Info("[Supporter] OnEnter");
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnExit : libere les textures GL (DOIT etre appele depuis le thread
        // GL — c'est le cas via SceneManager Pop/Replace).
        // ─────────────────────────────────────────────────────────────────────
        void SupporterScene::OnExit(AppContext& /*ctx*/)
        {
            for (int i = 0; i < (int)kSocialCount; ++i)
            {
                mSocialIcons[i].Shutdown();
                mSocialLoaded[i] = false;
            }
        }

        void SupporterScene::OnUpdate(AppContext& /*ctx*/, float dt)
        {
            mTime += dt;
            mEnterAnim += dt / 0.3f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;
        }

        float SupporterScene::ClampScroll(float v) const
        {
            if (v < 0.0f) return 0.0f;
            if (v > mMaxScroll) return mMaxScroll;
            return v;
        }

        bool SupporterScene::HitTestBack(float sx, float sy) const
        {
            return sx >= mBackX && sx <= mBackX + mBackW
                && sy >= mBackY && sy <= mBackY + mBackH;
        }
        bool SupporterScene::HitTestShare(float sx, float sy) const
        {
            return sx >= mShareX && sx <= mShareX + mShareW
                && sy >= mShareY && sy <= mShareY + mShareH;
        }
        int SupporterScene::HitTestAmount(float sx, float sy) const
        {
            for (int i = 0; i < kAmountCount + 1; ++i)
            {
                if (sx >= mAmountX[i] && sx <= mAmountX[i] + mAmountW
                 && sy >= mAmountY[i] && sy <= mAmountY[i] + mAmountH)
                    return i;
            }
            return -1;
        }
        int SupporterScene::HitTestSocial(float sx, float sy) const
        {
            for (int i = 0; i < (int)kSocialCount; ++i)
            {
                if (sx >= mSocialX[i] && sx <= mSocialX[i] + mSocialW
                 && sy >= mSocialY[i] && sy <= mSocialY[i] + mSocialH)
                    return i;
            }
            return -1;
        }

        // ─────────────────────────────────────────────────────────────────────
        // Actions
        // ─────────────────────────────────────────────────────────────────────
        void SupporterScene::DoShare(AppContext& /*ctx*/)
        {
            // Phase 1 : on ouvre la page projet (LinkedIn pour l'instant) via
            // NkLauncher. Le partage natif (Intent.ACTION_SEND / navigator.share)
            // viendra plus tard via une API dediee NkShare.
            logger.Info("[Supporter] SHARE -> open {0}", kShareUrl);
            NkLauncher::OpenURL(kShareUrl);
        }

        void SupporterScene::DoDonate(AppContext& /*ctx*/, int amountIndex)
        {
            // L'URL de donation peut etre etendue avec ?amount=X (Ko-fi par ex.).
            // Pour l'instant on ouvre juste kDonateBaseUrl.
            if (amountIndex == -1)
            {
                logger.Info("[Supporter] DONATE LIBRE -> {0}", kDonateBaseUrl);
            }
            else
            {
                const int usd = kAmountUSD[amountIndex];
                logger.Info("[Supporter] DONATE preset {0}$ -> {1}", usd, kDonateBaseUrl);
            }
            NkLauncher::OpenURL(kDonateBaseUrl);
        }

        void SupporterScene::DoOpenSocial(AppContext& /*ctx*/, int idx)
        {
            if (idx < 0 || idx >= (int)kSocialCount) return;
            logger.Info("[Supporter] OPEN SOCIAL {0} -> {1}",
                        kSocials[idx].name, kSocials[idx].url);
            NkLauncher::OpenURL(kSocials[idx].url);
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnRender — layout vertical scrollable :
        //   1. Header sticky (RETOUR + titre)
        //   2. Story (bloc texte emotive)
        //   3. PARTAGER (banderole)
        //   4. DON USD (6 cards)
        //   5. PAIEMENT DIRECT (identifiants)
        //   6. SUIVEZ-NOUS (5 social cards)
        //   7. Footer note
        // ─────────────────────────────────────────────────────────────────────
        void SupporterScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const float scale = GetUIScale(W, H);
            const float safeX = (float)ctx.safe.LeftX();
            const float safeW = (float)ctx.safe.SafeW();
            const float enterA = EaseOutCubic(mEnterAnim);

            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);

            // Layout responsive : on exprime les dimensions en % viewport
            // (W/H) plutot qu'en pixels*scale. Clamps doux pour eviter les
            // degeneres en petites/grandes resolutions. Le texte garde
            // scale (FontAtlas bitmap) pour rester net.
            mTopReserve = Pct::H(H, 0.085f, 56.0f, 110.0f);
            const float scrollTop = (float)ctx.safe.TopY() + mTopReserve;
            const float scrollBot = (float)ctx.safe.TopY() + (float)ctx.safe.SafeH()
                                  - Pct::H(H, 0.015f, 8.0f, 24.0f);
            const float scrollH   = scrollBot - scrollTop;

            // ── Geometrie du header (sticky, dessine en DERNIER pour
            // masquer le contenu scrollable qui passerait dessous). ─────
            mBackW = Pct::W(W, 0.11f, 78.0f, 150.0f);
            mBackH = Pct::H(H, 0.05f, 32.0f, 60.0f);
            mBackX = (float)ctx.safe.LeftX() + Pct::W(W, 0.02f, 10.0f, 28.0f);
            mBackY = (float)ctx.safe.TopY()  + Pct::H(H, 0.02f, 10.0f, 28.0f);

            // ── Contenu scrollable ──────────────────────────────────────────
            // gridLeft + availW : on garde une marge laterale en % avec un
            // plancher pour qu'on ait toujours un padding lisible sur mobile.
            const float sidePad  = Pct::W(W, 0.03f, 12.0f, 40.0f);
            const float gridLeft = safeX + sidePad;
            const float availW   = safeW - 2.0f * sidePad;
            float worldY = 0.0f;

            // 1. STORY block (card avec border accent)
            // La hauteur de la card et le pas vertical des lignes sont
            // exprimes en % H — sur petit ecran ca se ramasse, sur grand
            // ecran ca s'aere. On garde scale pour le rendu glyphe.
            {
                const float lineH    = Pct::H(H, 0.024f, 14.0f, 26.0f);
                const float headLineH = Pct::H(H, 0.034f, 20.0f, 36.0f);
                const float vPad     = Pct::H(H, 0.024f, 12.0f, 28.0f);
                const float storyH   = (kStoryN + 1) * lineH + 2.0f * vPad;
                const float bandW    = Pct::W(W, 0.006f, 3.0f, 8.0f);
                const float textPadL = Pct::W(W, 0.025f, 12.0f, 28.0f);
                const float blockGap = Pct::H(H, 0.025f, 12.0f, 30.0f);
                const float screenY  = worldY - mScrollY + scrollTop;
                if (screenY + storyH >= scrollTop - 20.0f * scale
                 && screenY <= scrollBot + 20.0f * scale)
                {
                    math::NkColor bg = { 0, 245, 255, (uint8_t)(18 * enterA) };
                    math::NkColor bd = { 0, 245, 255, (uint8_t)(180 * enterA) };
                    r.DrawQuad       (gridLeft, screenY, availW, storyH, bg);
                    r.DrawQuadOutline(gridLeft, screenY, availW, storyH, bd, 1.5f);

                    // Trait orange a gauche (accent visuel "combat")
                    r.DrawQuad(gridLeft, screenY, bandW, storyH,
                               { 255, 107, 0, 220 });

                    float ly = screenY + vPad;
                    for (int i = 0; i < kStoryN; ++i)
                    {
                        // Premiere ligne = headline (subtitle slot), reste body
                        const bool isHead = (i == 0);
                        const math::NkColor c = isHead
                            ? math::NkColor{ 0, 245, 255, (uint8_t)(255 * enterA) }
                            : math::NkColor{ 255, 255, 255, (uint8_t)(220 * enterA) };
                        f.DrawStringScaled(r,
                                     isHead ? FontAtlas::SubtitleSlot : FontAtlas::BodySlot,
                                     scale,
                                     gridLeft + textPadL, ly,
                                     kStory[i], c);
                        ly += isHead ? headLineH : lineH;
                    }
                }
                worldY += storyH + blockGap;
            }

            // 2. PARTAGER banderole
            // Largeur = availW (deja en %), hauteur en % H pour rester
            // proportionnee au viewport.
            {
                mShareW = availW;
                mShareH = Pct::H(H, 0.085f, 50.0f, 90.0f);
                mShareX = gridLeft;
                mShareY = worldY - mScrollY + scrollTop;

                const float iconCx    = Pct::W(W, 0.06f, 28.0f, 60.0f);
                const float iconSize  = Pct::H(H, 0.015f, 7.0f, 16.0f);
                const float textPadL  = Pct::W(W, 0.12f, 60.0f, 120.0f);
                const float titlePadT = Pct::H(H, 0.018f, 10.0f, 22.0f);
                const float subPadT   = Pct::H(H, 0.05f, 28.0f, 56.0f);
                const float sectionGap = Pct::H(H, 0.032f, 16.0f, 36.0f);

                if (mShareY + mShareH >= scrollTop - 20.0f * scale
                 && mShareY <= scrollBot + 20.0f * scale)
                {
                    const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 2.0f);
                    math::NkColor bg = { 0, 245, 255, (uint8_t)((30 + 20 * pulse) * enterA) };
                    math::NkColor bd = { 0, 245, 255, (uint8_t)(220 * enterA) };
                    r.DrawQuad       (mShareX, mShareY, mShareW, mShareH, bg);
                    r.DrawQuadOutline(mShareX, mShareY, mShareW, mShareH, bd, 2.0f);
                    // Icone "share"
                    const float cx = mShareX + iconCx;
                    const float cy = mShareY + mShareH * 0.5f;
                    const float s2 = iconSize;
                    r.DrawQuadOutline(cx - s2, cy - s2 * 0.6f, s2 * 1.6f, s2 * 1.6f,
                                      { 0, 245, 255, 240 }, 1.5f);
                    r.DrawLine(cx + s2 * 0.4f, cy - s2 * 1.3f, cx + s2 * 1.3f, cy - s2 * 1.3f,
                               { 0, 245, 255, 240 }, 2.0f);
                    r.DrawLine(cx + s2 * 1.3f, cy - s2 * 1.3f, cx + s2 * 1.3f, cy - s2 * 0.4f,
                               { 0, 245, 255, 240 }, 2.0f);

                    f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                                 mShareX + textPadL, mShareY + titlePadT,
                                 "PARTAGER LE JEU", theme::White());
                    f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                 mShareX + textPadL, mShareY + subPadT,
                                 "RECOMMANDE A UN AMI — 100% GRATUIT",
                                 { 255, 255, 255, 180 });
                }
                worldY += mShareH + sectionGap;
            }

            // 3. DON USD section (6 cards)
            // Cards en grille adaptative (3 cols paysage / 2 portrait).
            // Hauteur card et padding inter-cards en % H.
            {
                const float titleH      = Pct::H(H, 0.045f, 22.0f, 50.0f);
                const float pad         = Pct::W(W, 0.012f, 4.0f, 14.0f);
                const float amountCardH = Pct::H(H, 0.092f, 54.0f, 100.0f);
                const float sectionGap  = Pct::H(H, 0.03f, 14.0f, 36.0f);
                const float labelPadT   = Pct::H(H, 0.022f, 12.0f, 26.0f);
                const float subPadT     = Pct::H(H, 0.06f, 32.0f, 68.0f);

                const float screenSecY = worldY - mScrollY + scrollTop;
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   (float)W * 0.5f, screenSecY,
                                   "FAIRE UN DON (USD)", theme::Cyan());
                worldY += titleH;

                const int total = kAmountCount + 1;
                const int cols  = ((float)W > (float)H * 1.2f) ? 3 : 2;
                const int rows  = (total + cols - 1) / cols;
                mAmountW = (availW - pad * (cols - 1)) / cols;
                mAmountH = amountCardH;
                for (int i = 0; i < total; ++i)
                {
                    const int col = i % cols;
                    const int row = i / cols;
                    mAmountX[i] = gridLeft + col * (mAmountW + pad);
                    mAmountY[i] = worldY + row * (mAmountH + pad) - mScrollY + scrollTop;

                    const bool isFree = (i == kAmountCount);
                    const int  usd    = isFree ? -1 : kAmountUSD[i];

                    math::NkColor accent = isFree
                        ? math::NkColor{ 204, 119, 255, 255 }
                        : (usd == 0
                            ? math::NkColor{ 160, 160, 160, 255 }
                            : (usd >= 10
                                ? math::NkColor{ 255, 107,   0, 255 }
                                : math::NkColor{   0, 245, 255, 255 }));
                    math::NkColor bg = accent; bg.a = (uint8_t)(30 * enterA);
                    math::NkColor bd = accent; bd.a = (uint8_t)(220 * enterA);
                    r.DrawQuad       (mAmountX[i], mAmountY[i], mAmountW, mAmountH, bg);
                    r.DrawQuadOutline(mAmountX[i], mAmountY[i], mAmountW, mAmountH, bd, 1.5f);

                    char buf[16];
                    if (isFree)        std::snprintf(buf, sizeof(buf), "LIBRE");
                    else if (usd == 0) std::snprintf(buf, sizeof(buf), "0$ MERCI");
                    else               std::snprintf(buf, sizeof(buf), "%d$", usd);
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                       mAmountX[i] + mAmountW * 0.5f,
                                       mAmountY[i] + labelPadT,
                                       buf, accent);

                    const char* sub = isFree
                        ? "SAISIR UN MONTANT"
                        : (usd == 0 ? "GRATUIT, JUSTE MERCI" : "DON UNIQUE");
                    math::NkColor subC = { 255, 255, 255, (uint8_t)(160 * enterA) };
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       mAmountX[i] + mAmountW * 0.5f,
                                       mAmountY[i] + subPadT,
                                       sub, subC);
                }
                worldY += rows * (mAmountH + pad);
                worldY += sectionGap;
            }

            // 4. PAIEMENT DIRECT (identifiants)
            // Chaque ligne = card pleine largeur avec bande accent gauche.
            // Hauteur de ligne en % H pour rester lisible mobile/desktop.
            {
                const float titleH    = Pct::H(H, 0.034f, 18.0f, 38.0f);
                const float subH      = Pct::H(H, 0.034f, 18.0f, 40.0f);
                const float rowH      = Pct::H(H, 0.055f, 32.0f, 64.0f);
                const float rowPad    = Pct::H(H, 0.01f,  4.0f, 12.0f);
                const float bandW     = Pct::W(W, 0.006f, 3.0f, 8.0f);
                const float textPadL  = Pct::W(W, 0.025f, 12.0f, 28.0f);
                const float textPadT  = Pct::H(H, 0.016f, 8.0f, 20.0f);
                const float sectionGap = Pct::H(H, 0.022f, 10.0f, 28.0f);

                const float screenSecY = worldY - mScrollY + scrollTop;
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   (float)W * 0.5f, screenSecY,
                                   "PAIEMENT DIRECT", theme::Cyan());
                worldY += titleH;
                const float subY = worldY - mScrollY + scrollTop;
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   (float)W * 0.5f, subY,
                                   "ENVOIE DIRECTEMENT VERS CES COMPTES",
                                   { 255, 255, 255, 160 });
                worldY += subH;

                for (int i = 0; i < kPayIdsN; ++i)
                {
                    const float rx = gridLeft;
                    const float ry = worldY - mScrollY + scrollTop;
                    math::NkColor bg = kPayIds[i].accent; bg.a = (uint8_t)(22 * enterA);
                    math::NkColor bd = kPayIds[i].accent; bd.a = (uint8_t)(180 * enterA);
                    r.DrawQuad       (rx, ry, availW, rowH, bg);
                    r.DrawQuadOutline(rx, ry, availW, rowH, bd, 1.0f);
                    // Bande gauche
                    r.DrawQuad(rx, ry, bandW, rowH, kPayIds[i].accent);
                    // Label (gauche) + valeur (droite)
                    math::NkColor lab = kPayIds[i].accent;
                    lab.a = (uint8_t)(230 * enterA);
                    f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                                 rx + textPadL, ry + textPadT,
                                 kPayIds[i].label, lab);
                    f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                                 rx + availW * 0.5f, ry + textPadT,
                                 kPayIds[i].value,
                                 { 255, 255, 255, (uint8_t)(230 * enterA) });
                    worldY += rowH + rowPad;
                }
                worldY += sectionGap;
            }

            // 5. SUIVEZ-NOUS (reseaux sociaux)
            // 5 cards en grille adaptative. Pastille = disque accent +
            // icone (avec fallback badge texte). Toutes les dimensions
            // (rad, hauteur card, padding) en % H/W.
            {
                const float titleH      = Pct::H(H, 0.045f, 22.0f, 50.0f);
                const float pad         = Pct::W(W, 0.012f, 4.0f, 14.0f);
                const float socialH     = Pct::H(H, 0.09f, 52.0f, 100.0f);
                const float socialRad   = Pct::H(H, 0.025f, 14.0f, 30.0f);
                const float socialCx    = Pct::W(W, 0.045f, 22.0f, 50.0f);
                const float socialTextL = Pct::W(W, 0.085f, 44.0f, 100.0f);
                const float nameTopPad  = Pct::H(H, 0.015f, 8.0f, 20.0f);
                const float handleTopPad = Pct::H(H, 0.05f, 28.0f, 60.0f);
                const float sectionGap  = Pct::H(H, 0.02f, 8.0f, 24.0f);

                const float screenSecY = worldY - mScrollY + scrollTop;
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   (float)W * 0.5f, screenSecY,
                                   "SUIVEZ-NOUS", theme::Cyan());
                worldY += titleH;

                // 5 cards en grille adaptative (5 colonnes si large, sinon 3 + 2)
                const int cols  = (((float)W > (float)H * 1.2f) && availW > Pct::W(W, 0.6f, 400.0f, 900.0f))
                                  ? 5 : 3;
                const int rows  = ((int)kSocialCount + cols - 1) / cols;
                mSocialW = (availW - pad * (cols - 1)) / cols;
                mSocialH = socialH;
                for (int i = 0; i < (int)kSocialCount; ++i)
                {
                    const int col = i % cols;
                    const int row = i / cols;
                    mSocialX[i] = gridLeft + col * (mSocialW + pad);
                    mSocialY[i] = worldY + row * (mSocialH + pad) - mScrollY + scrollTop;

                    math::NkColor bg = kSocials[i].accent; bg.a = (uint8_t)(40 * enterA);
                    math::NkColor bd = kSocials[i].accent; bd.a = (uint8_t)(220 * enterA);
                    r.DrawQuad       (mSocialX[i], mSocialY[i], mSocialW, mSocialH, bg);
                    r.DrawQuadOutline(mSocialX[i], mSocialY[i], mSocialW, mSocialH, bd, 1.5f);

                    // Pastille a gauche : disque accent + icone reseau social
                    // dessinee par dessus avec ratio preserve. Fallback sur
                    // un badge texte si l'icone n'a pas pu etre chargee.
                    const float cx = mSocialX[i] + socialCx;
                    const float cy = mSocialY[i] + mSocialH * 0.5f;
                    const float rad = socialRad;
                    r.DrawCircle(cx, cy, rad, kSocials[i].accent, 24);
                    r.DrawCircleOutline(cx, cy, rad, { 255, 255, 255, 200 }, 1.0f, 24);

                    if (mSocialLoaded[i] && mSocialIcons[i].IsValid())
                    {
                        // Icone dans le disque, marge ~22% du rayon. Ratio
                        // d'aspect preserve (les SVG / PNG ne sont pas tous
                        // carres parfaitement).
                        const float aspect = mSocialIcons[i].AspectRatio();
                        const float box    = rad * 1.55f;
                        float iconW = box;
                        float iconH = (aspect > 0.0001f) ? (iconW / aspect) : box;
                        if (iconH > box) { iconH = box; iconW = iconH * aspect; }
                        const float ix = cx - iconW * 0.5f;
                        const float iy = cy - iconH * 0.5f;
                        r.BindTexture(mSocialIcons[i].Id());
                        math::NkColor tint = { 255, 255, 255, (uint8_t)(255 * enterA) };
                        r.DrawTexturedQuadRGBA(ix, iy, iconW, iconH,
                                               0.0f, 0.0f, 1.0f, 1.0f, tint);
                    }
                    else
                    {
                        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                           cx, cy - rad * 0.5f,
                                           kSocials[i].badge,
                                           { 255, 255, 255, (uint8_t)(240 * enterA) });
                    }

                    // Nom + handle a droite
                    f.DrawStringScaled(r, FontAtlas::BodySlot, scale,
                                 mSocialX[i] + socialTextL,
                                 mSocialY[i] + nameTopPad,
                                 kSocials[i].name,
                                 { 255, 255, 255, (uint8_t)(240 * enterA) });
                    f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                 mSocialX[i] + socialTextL,
                                 mSocialY[i] + handleTopPad,
                                 kSocials[i].handle,
                                 { 255, 255, 255, 170 });
                }
                worldY += rows * (mSocialH + pad);
                worldY += sectionGap;
            }

            // 6. Footer
            {
                const float footerH = Pct::H(H, 0.035f, 16.0f, 40.0f);
                const float screenFootY = worldY - mScrollY + scrollTop;
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   (float)W * 0.5f, screenFootY,
                                   "MERCI D'ETRE LA. — RIHEN",
                                   { 255, 107, 0, 200 });
                worldY += footerH;
            }

            // ── Calcul max scroll ──────────────────────────────────────────
            const float contentH = worldY;
            mMaxScroll = (contentH > scrollH) ? (contentH - scrollH) : 0.0f;
            if (mScrollY > mMaxScroll) mScrollY = mMaxScroll;

            // Scrollbar discrete a droite si overflow. Largeur en % W,
            // hauteur min thumb en % H pour rester saisissable au doigt.
            if (mMaxScroll > 0.5f)
            {
                const float sbW    = Pct::W(W, 0.005f, 3.0f, 8.0f);
                const float sbPadR = Pct::W(W, 0.005f, 2.0f, 8.0f);
                const float sbPadY = Pct::H(H, 0.004f, 2.0f, 6.0f);
                const float sbX = safeX + safeW - sbW - sbPadR;
                const float sbY = scrollTop + sbPadY;
                const float sbH = scrollH - 2.0f * sbPadY;
                r.DrawQuad(sbX, sbY, sbW, sbH, { 255, 255, 255, 16 });
                const float frac = scrollH / contentH;
                const float thumbMinH = Pct::H(H, 0.03f, 16.0f, 40.0f);
                const float thumbH = math::NkMax(thumbMinH, sbH * frac);
                const float thumbY = sbY
                                   + (sbH - thumbH) * (mScrollY / mMaxScroll);
                r.DrawQuad(sbX, thumbY, sbW, thumbH,
                           { 0, 245, 255, 180 });
            }

            // ── Header sticky OPAQUE (dessine EN DERNIER pour passer
            // par-dessus le contenu scrollable). Bande horizontale qui
            // couvre toute la zone reservee + bouton RETOUR + titre.
            {
                const float sepH      = Pct::H(H, 0.003f, 1.5f, 4.0f);
                const float titleTopP = Pct::H(H, 0.025f, 12.0f, 32.0f);

                math::NkColor headerBg = theme::Dark();
                headerBg.a = 240;
                r.DrawQuad(0.0f, 0.0f, (float)W, scrollTop - sepH, headerBg);
                // Ligne separatrice cyan fine en bas du header
                r.DrawQuad(0.0f, scrollTop - sepH, (float)W, sepH,
                           { 0, 245, 255, 120 });

                // Bouton RETOUR
                r.DrawQuad       (mBackX, mBackY, mBackW, mBackH, { 0, 245, 255, 30 });
                r.DrawQuadOutline(mBackX, mBackY, mBackW, mBackH, { 0, 245, 255, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                                   mBackX + mBackW * 0.5f,
                                   mBackY + mBackH * 0.18f,
                                   "RETOUR", theme::Cyan());
                // Titre centre
                f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                                   (float)W * 0.5f,
                                   (float)ctx.safe.TopY() + titleTopP,
                                   "SUPPORTER", theme::White());
            }

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        void SupporterScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            const float scrollStep = 60.0f;

            // Clavier
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

            // Molette
            if (auto* w = ev.As<NkMouseWheelVerticalEvent>())
            {
                mScrollY = ClampScroll(mScrollY - w->GetDeltaY() * scrollStep);
                return;
            }

            // Souris
            if (auto* mp = ev.As<NkMouseButtonPressEvent>())
            {
                if (mp->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    mMouseDown = true;
                    mDragActive = true;
                    mDragWasScroll = false;
                    mDragStartY = (float)mp->GetY();
                    mDragLastY  = (float)mp->GetY();
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
                    if (!mDragWasScroll)
                    {
                        const float px = (float)mr->GetX();
                        const float py = (float)mr->GetY();
                        if (HitTestBack(px, py))  { ctx.scenes->Pop(); return; }
                        if (HitTestShare(px, py)) { DoShare(ctx); return; }
                        const int ai = HitTestAmount(px, py);
                        if (ai >= 0) { DoDonate(ctx, (ai == kAmountCount) ? -1 : ai); return; }
                        const int si = HitTestSocial(px, py);
                        if (si >= 0) { DoOpenSocial(ctx, si); return; }
                    }
                }
                return;
            }

            // Touch begin : track id + start drag
            if (auto* tb = ev.As<NkTouchBeginEvent>())
            {
                if (tb->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = tb->GetTouch(0);
                    mActiveTouchId = (long long)tp.id;
                    mDragTouchId   = (long long)tp.id;
                    mDragActive    = true;
                    mDragWasScroll = false;
                    mDragStartY    = tp.clientY;
                    mDragLastY     = tp.clientY;
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
                    if (math::NkFabs(tp.clientY - mDragStartY) > 6.0f) mDragWasScroll = true;
                    mScrollY = ClampScroll(mScrollY - dy);
                    mDragLastY = tp.clientY;
                }
                return;
            }
            if (auto* te = ev.As<NkTouchEndEvent>())
            {
                if (te->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = te->GetTouch(0);
                    if ((long long)tp.id != mActiveTouchId) { mActiveTouchId = -1; return; }
                    mActiveTouchId = -1;
                    mDragTouchId   = -1;
                    mDragActive    = false;
                    if (!mDragWasScroll)
                    {
                        if (HitTestBack(tp.clientX, tp.clientY))  { ctx.scenes->Pop(); return; }
                        if (HitTestShare(tp.clientX, tp.clientY)) { DoShare(ctx); return; }
                        const int ai = HitTestAmount(tp.clientX, tp.clientY);
                        if (ai >= 0) { DoDonate(ctx, (ai == kAmountCount) ? -1 : ai); return; }
                        const int si = HitTestSocial(tp.clientX, tp.clientY);
                        if (si >= 0) { DoOpenSocial(ctx, si); return; }
                    }
                }
                return;
            }
        }

    } // namespace songoo
} // namespace nkentseu
