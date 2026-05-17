#pragma once
// =============================================================================
// FontAtlas.h
// -----------------------------------------------------------------------------
// Atlas de glyphes pour le rendu de texte OpenGL.
// Strategie :
//   - On utilise NKFont (lib du moteur) pour rasteriser des polices embarquees
//     (Karla / DroidSans / Roboto / ProggyClean) a plusieurs tailles.
//   - L'atlas resultant (image alpha 1 canal) est uploade dans une texture
//     GL_R8 unique.
//   - Les glyphes portent leurs UV [0..1] dans cet atlas + un quad rectangle
//     (x0,y0,x1,y1) relatif au curseur. Le rendu se fait via
//     GLRenderer2D::DrawTexturedQuad : chaque glyphe = 1 quad textured.
//
// Slots : 5 tailles preconfigurees (Small/Body/Subtitle/Headline/Display).
//   - Small    : 14 px  — credits, key badges, instructions
//   - Body     : 18 px  — texte courant, items menu
//   - Subtitle : 28 px  — sous-titres ecran
//   - Headline : 48 px  — titres ecran
//   - Display  : 72 px  — logo PONG (splash)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include "NKMath/NkColor.h"

namespace nkentseu
{
    // Forward declarations
    struct NkFontAtlas;
    struct NkFont;
}

namespace nkentseu
{
    namespace pong
    {

        class GLRenderer2D;

        class FontAtlas
        {
        public:
            // ── Slots de tailles ─────────────────────────────────────────────
            enum SizeSlot
            {
                SmallSlot     = 0,   // 14 px
                BodySlot      = 1,   // 18 px
                SubtitleSlot  = 2,   // 28 px
                HeadlineSlot  = 3,   // 48 px
                DisplaySlot   = 4,   // 72 px
                SlotCount     = 5
            };

            FontAtlas()  = default;
            ~FontAtlas();

            // ── Lifecycle ────────────────────────────────────────────────────
            /// Charge la meilleure police embarquee dispo, rasterise les
            /// 5 tailles et upload l'atlas en texture GL. Retourne false sur
            /// echec (NKFont indisponible ou GL invalide).
            bool Init();
            /// Libere la texture GL et detruit l'atlas CPU.
            void Shutdown();

            // ── Accesseurs ───────────────────────────────────────────────────
            uint32 TextureId() const noexcept { return mTextureId; }

            // ── Mesures ──────────────────────────────────────────────────────
            /// Largeur en pixels d'une chaine ASCII pour le slot @p s.
            float MeasureWidth(SizeSlot s, const char* text) const;
            /// Variante avec scale lineaire applique a la taille du glyphe.
            float MeasureWidthScaled(SizeSlot s, float scale, const char* text) const;
            /// Hauteur de ligne approximative (sizePx * 1.2).
            float LineHeight(SizeSlot s) const;

            // ── Trace ────────────────────────────────────────────────────────
            /// Trace une chaine LTR. Coordonnees (x, y) = top-left.
            /// Retourne la largeur tracee (en pixels).
            float DrawString(GLRenderer2D& r, SizeSlot s,
                           float x, float y,
                           const char* text,
                           math::NkColor color);
            /// Idem, centre horizontalement autour de @p cx.
            float DrawStringCentered(GLRenderer2D& r, SizeSlot s,
                                   float cx, float y,
                                   const char* text,
                                   math::NkColor color);

            // ── Variantes scalees (taille de glyphe multipliee par scale) ────
            // Le slot reste rasterise a sa taille de base, mais les quads sont
            // emis en taille (px_base * scale) — donne un upsampling bilineaire
            // GL (texture filter LINEAR). Acceptable visuellement jusqu'a
            // scale ~3x ; au dela il vaut mieux ajouter un slot plus grand.
            float DrawStringScaled(GLRenderer2D& r, SizeSlot s, float scale,
                                 float x, float y, const char* text,
                                 math::NkColor color);
            float DrawStringCenteredScaled(GLRenderer2D& r, SizeSlot s, float scale,
                                         float cx, float y, const char* text,
                                         math::NkColor color);
            void DrawStringShadowScaled(GLRenderer2D& r, SizeSlot s, float scale,
                                      float x, float y, const char* text,
                                      math::NkColor textColor,
                                      math::NkColor shadowColor,
                                      int blurPixels = 2);
            void DrawStringShadowCenteredScaled(GLRenderer2D& r, SizeSlot s, float scale,
                                              float cx, float y, const char* text,
                                              math::NkColor textColor,
                                              math::NkColor shadowColor,
                                              int blurPixels = 2);

            // ── Text-shadow CSS-like ─────────────────────────────────────────
            /// Trace 8 directions avec alpha decroissant + le texte principal.
            /// Simule l'effet `text-shadow` CSS (halo neon autour des lettres).
            void DrawStringShadow(GLRenderer2D& r, SizeSlot s,
                                float x, float y,
                                const char* text,
                                math::NkColor textColor,
                                math::NkColor shadowColor,
                                int blurPixels = 2);
            void DrawStringShadowCentered(GLRenderer2D& r, SizeSlot s,
                                        float cx, float y,
                                        const char* text,
                                        math::NkColor textColor,
                                        math::NkColor shadowColor,
                                        int blurPixels = 2);

        private:
            NkFontAtlas* mAtlas             = nullptr;  ///< Possede ; libere par Shutdown
            NkFont*      mFonts[SlotCount]  = { nullptr, nullptr, nullptr, nullptr, nullptr };
            uint32       mTextureId         = 0;
            int          mAtlasW            = 0;
            int          mAtlasH            = 0;
        };

    } // namespace pong
} // namespace nkentseu
