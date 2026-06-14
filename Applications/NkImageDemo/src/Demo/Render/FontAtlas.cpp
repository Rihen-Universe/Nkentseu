// =============================================================================
// FontAtlas.cpp
// -----------------------------------------------------------------------------
// Implementation : wrap NkFontAtlas + upload de l'atlas alpha-mask dans une
// texture GL_R8 utilisable par le shader textured du GLRenderer2D.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// ── GLAD2 : inclure avant tout (evite conflit gl.h systeme) ──────────────────
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(PONG_HAS_GLAD)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       include <glad/wgl.h>
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if defined(__has_include)
#           if __has_include(<glad/glx.h>)
#               include <glad/glx.h>
#           endif
#       endif
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       include <glad/gles2.h>
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       include <glad/gles2.h>
#   endif
#endif

#if defined(Bool)
#   undef Bool   // X11 pollue le namespace global avec un macro Bool
#endif

#include "FontAtlas.h"
#include "GLRenderer2D.h"
#include "NKFont/NkFont.h"
#include "NKFont/Embedded/NkFontEmbedded.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h"
#include <cstring>
#include <cstdlib>

namespace nkentseu
{
    namespace demo
    {

        // ── Tailles en pixels associees aux slots de FontAtlas::SizeSlot ─────
        // Volontairement modeste pour limiter la taille de l'atlas final.
        static const float kSizePx[FontAtlas::SlotCount] =
        {
            14.0f,   // SmallSlot
            18.0f,   // BodySlot
            28.0f,   // SubtitleSlot
            48.0f,   // HeadlineSlot
            72.0f    // DisplaySlot
        };

        FontAtlas::~FontAtlas()
        {
            Shutdown();
        }

        // ─────────────────────────────────────────────────────────────────────
        // FontAtlas::Init
        // Strategie : on tente Karla > DroidSans > Roboto > ProggyClean. La
        // premiere police embarquee disponible est rasterisee aux 5 tailles
        // configurees puis uploadee en texture GL_R8 (1 canal alpha).
        // ─────────────────────────────────────────────────────────────────────
        bool FontAtlas::Init()
        {
            mAtlas = new NkFontAtlas();

            // Choix de la police a utiliser. On prefere une vectorielle
            // sans-serif lisible plutot que la bitmap par defaut.
            NkEmbeddedFontId fontId = NkEmbeddedFontId::ProggyClean;
            if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::Karla))
            {
                fontId = NkEmbeddedFontId::Karla;
            }
            else if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::DroidSans))
            {
                fontId = NkEmbeddedFontId::DroidSans;
            }
            else if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::Roboto))
            {
                fontId = NkEmbeddedFontId::Roboto;
            }
            logger.Info("[FontAtlas] using embedded font: {0}",
                        NkFontEmbedded::GetName(fontId));

            // Rasterise la police a chaque taille de slot.
            const int slotN = static_cast<int>(SlotCount);
            for (int s = 0; s < slotN; ++s)
            {
                mFonts[s] = NkFontEmbedded::AddToAtlas(*mAtlas, fontId, kSizePx[s]);
                if (mFonts[s] == nullptr)
                {
                    logger.Error("[FontAtlas] AddToAtlas failed for slot {0} ({1}px)",
                                 s, kSizePx[s]);
                }
            }

            // Construit l'atlas (packing des glyphes dans une seule image).
            if (!mAtlas->Build())
            {
                logger.Error("[FontAtlas] atlas Build failed");
                delete mAtlas;
                mAtlas = nullptr;
                return false;
            }

            // Recupere les pixels alpha (1 canal) au format Gray8.
            nkft_uint8* pixels = nullptr;
            int w = 0;
            int h = 0;
            mAtlas->GetTexDataAsAlpha8(&pixels, &w, &h);
            if (pixels == nullptr || w == 0 || h == 0)
            {
                logger.Error("[FontAtlas] GetTexDataAsAlpha8 returned empty");
                return false;
            }
            mAtlasW = w;
            mAtlasH = h;

            // Upload en texture GL_R8 (single-channel alpha-mask).
            glGenTextures(1, &mTextureId);
            glBindTexture(GL_TEXTURE_2D, mTextureId);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0,
                         GL_RED, GL_UNSIGNED_BYTE, pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);

            logger.Info("[FontAtlas] atlas built: {0}x{1}, texId={2}",
                        w, h, mTextureId);
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────
        // FontAtlas::Shutdown — libere la texture GL puis l'atlas CPU.
        // ─────────────────────────────────────────────────────────────────────
        void FontAtlas::Shutdown()
        {
            if (mTextureId != 0)
            {
                glDeleteTextures(1, &mTextureId);
                mTextureId = 0;
            }
            if (mAtlas != nullptr)
            {
                delete mAtlas;
                mAtlas = nullptr;
            }
            for (int s = 0; s < static_cast<int>(SlotCount); ++s)
            {
                mFonts[s] = nullptr;
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // Mesures
        // ─────────────────────────────────────────────────────────────────────
        float FontAtlas::MeasureWidth(SizeSlot s, const char* text) const
        {
            if (text == nullptr || mFonts[s] == nullptr) return 0.0f;
            return mFonts[s]->CalcTextSizeX(text);
        }

        float FontAtlas::LineHeight(SizeSlot s) const
        {
            if (mFonts[s] == nullptr) return kSizePx[s];
            return kSizePx[s] * 1.2f;
        }

        // ─────────────────────────────────────────────────────────────────────
        // DrawString — emet un quad texturé par glyphe via GLRenderer2D.
        // Coordonnees : (x, y) = top-left du texte ; on convertit en baseline
        // via une approximation 0.82 * sizePx (suffisant pour les polices
        // sans-serif Karla / DroidSans).
        // ─────────────────────────────────────────────────────────────────────
        float FontAtlas::DrawString(GLRenderer2D& r, SizeSlot s,
                                  float x, float y,
                                  const char* text,
                                  math::NkColor color)
        {
            if (text == nullptr || mFonts[s] == nullptr || mTextureId == 0)
            {
                return 0.0f;
            }
            NkFont* font = mFonts[s];

            // S'assure que la texture font est bindee avant les draws.
            r.BindTexture(mTextureId);

            // Approximation baseline = ascent (~82% de la taille en px).
            const float fontPx = kSizePx[s];
            const float ascent = fontPx * 0.82f;
            float cursorX = x;
            float cursorY = y + ascent;

            // Parcourt la chaine UTF-8 codepoint par codepoint.
            const char* p = text;
            while (*p != '\0')
            {
                const char* pBefore = p;
                NkFontCodepoint cp = NkFont::DecodeUTF8(&p, nullptr);
                if (cp == 0)
                {
                    // Decodage echoue, on avance pour eviter boucle infinie.
                    if (p == pBefore) ++p;
                    continue;
                }
                const NkFontGlyph* g = font->FindGlyph(cp);
                if (g == nullptr) continue;
                if (g->visible)
                {
                    const float gx = cursorX + g->x0;
                    const float gy = cursorY + g->y0;
                    const float gw = g->x1 - g->x0;
                    const float gh = g->y1 - g->y0;
                    r.DrawTexturedQuad(gx, gy, gw, gh,
                                       g->u0, g->v0, g->u1, g->v1,
                                       color);
                }
                cursorX += g->advanceX;
            }
            return cursorX - x;
        }

        float FontAtlas::DrawStringCentered(GLRenderer2D& r, SizeSlot s,
                                          float cx, float y,
                                          const char* text,
                                          math::NkColor color)
        {
            const float w = MeasureWidth(s, text);
            return DrawString(r, s, cx - w * 0.5f, y, text, color);
        }

        // ─────────────────────────────────────────────────────────────────────
        // MeasureWidthScaled / DrawStringScaled — variantes avec scale.
        // Multiplie x0/y0/x1/y1/advanceX par scale a l'emission des quads.
        // L'upsampling est realise par le filter GL_LINEAR de la texture atlas.
        // ─────────────────────────────────────────────────────────────────────
        float FontAtlas::MeasureWidthScaled(SizeSlot s, float scale,
                                          const char* text) const
        {
            if (text == nullptr || mFonts[s] == nullptr) return 0.0f;
            return mFonts[s]->CalcTextSizeX(text) * scale;
        }

        float FontAtlas::DrawStringScaled(GLRenderer2D& r, SizeSlot s, float scale,
                                        float x, float y,
                                        const char* text,
                                        math::NkColor color)
        {
            if (text == nullptr || mFonts[s] == nullptr || mTextureId == 0
                || scale <= 0.0f)
            {
                return 0.0f;
            }
            NkFont* font = mFonts[s];
            r.BindTexture(mTextureId);

            const float fontPx = kSizePx[s];
            const float ascent = fontPx * 0.82f * scale;
            float cursorX = x;
            float cursorY = y + ascent;

            const char* p = text;
            while (*p != '\0')
            {
                const char* pBefore = p;
                NkFontCodepoint cp = NkFont::DecodeUTF8(&p, nullptr);
                if (cp == 0) { if (p == pBefore) ++p; continue; }
                const NkFontGlyph* g = font->FindGlyph(cp);
                if (g == nullptr) continue;
                if (g->visible)
                {
                    const float gx = cursorX + g->x0 * scale;
                    const float gy = cursorY + g->y0 * scale;
                    const float gw = (g->x1 - g->x0) * scale;
                    const float gh = (g->y1 - g->y0) * scale;
                    r.DrawTexturedQuad(gx, gy, gw, gh,
                                       g->u0, g->v0, g->u1, g->v1,
                                       color);
                }
                cursorX += g->advanceX * scale;
            }
            return cursorX - x;
        }

        float FontAtlas::DrawStringCenteredScaled(GLRenderer2D& r, SizeSlot s,
                                                float scale,
                                                float cx, float y,
                                                const char* text,
                                                math::NkColor color)
        {
            const float w = MeasureWidthScaled(s, scale, text);
            return DrawStringScaled(r, s, scale, cx - w * 0.5f, y, text, color);
        }

        void FontAtlas::DrawStringShadowScaled(GLRenderer2D& r, SizeSlot s,
                                             float scale, float x, float y,
                                             const char* text,
                                             math::NkColor textColor,
                                             math::NkColor shadowColor,
                                             int blur)
        {
            for (int oy = -blur; oy <= blur; ++oy)
            {
                for (int ox = -blur; ox <= blur; ++ox)
                {
                    if (ox == 0 && oy == 0) continue;
                    const float d = static_cast<float>(math::NkAbs(ox) + math::NkAbs(oy));
                    const float a = 0.18f / (d > 0.0f ? d : 1.0f);
                    math::NkColor c = shadowColor;
                    c.a = static_cast<uint8_t>(a * shadowColor.a);
                    if (c.a > 4)
                    {
                        DrawStringScaled(r, s, scale,
                                       x + static_cast<float>(ox),
                                       y + static_cast<float>(oy),
                                       text, c);
                    }
                }
            }
            DrawStringScaled(r, s, scale, x, y, text, textColor);
        }

        void FontAtlas::DrawStringShadowCenteredScaled(GLRenderer2D& r, SizeSlot s,
                                                     float scale,
                                                     float cx, float y,
                                                     const char* text,
                                                     math::NkColor textColor,
                                                     math::NkColor shadowColor,
                                                     int blur)
        {
            const float w = MeasureWidthScaled(s, scale, text);
            DrawStringShadowScaled(r, s, scale, cx - w * 0.5f, y, text,
                                 textColor, shadowColor, blur);
        }

        // ─────────────────────────────────────────────────────────────────────
        // DrawStringShadow — simule un text-shadow CSS (halo neon autour du texte).
        // On trace 8 directions avec alpha decroissant puis le texte principal.
        // ─────────────────────────────────────────────────────────────────────
        void FontAtlas::DrawStringShadow(GLRenderer2D& r, SizeSlot s,
                                       float x, float y,
                                       const char* text,
                                       math::NkColor textColor,
                                       math::NkColor shadowColor,
                                       int blur)
        {
            for (int oy = -blur; oy <= blur; ++oy)
            {
                for (int ox = -blur; ox <= blur; ++ox)
                {
                    if (ox == 0 && oy == 0) continue;
                    const float d = static_cast<float>(math::NkAbs(ox) + math::NkAbs(oy));
                    const float a = 0.18f / (d > 0.0f ? d : 1.0f);
                    math::NkColor c = shadowColor;
                    c.a = static_cast<uint8_t>(a * shadowColor.a);
                    if (c.a > 4)
                    {
                        DrawString(r, s,
                                 x + static_cast<float>(ox),
                                 y + static_cast<float>(oy),
                                 text, c);
                    }
                }
            }
            DrawString(r, s, x, y, text, textColor);
        }

        void FontAtlas::DrawStringShadowCentered(GLRenderer2D& r, SizeSlot s,
                                                float cx, float y,
                                                const char* text,
                                                math::NkColor textColor,
                                                math::NkColor shadowColor,
                                                int blur)
        {
            const float w = MeasureWidth(s, text);
            DrawStringShadow(r, s, cx - w * 0.5f, y, text,
                           textColor, shadowColor, blur);
        }

    } // namespace demo
} // namespace nkentseu
