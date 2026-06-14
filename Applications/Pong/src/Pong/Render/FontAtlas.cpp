// =============================================================================
// FontAtlas.cpp
// -----------------------------------------------------------------------------
// Implementation : wrap NkFontAtlas + upload de l'atlas alpha-mask dans une
// texture GL_R8 utilisable par le shader textured du GLRenderer2D.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#include "FontAtlas.h"
#include "NKFont/NkFont.h"
#include "NKFont/Embedded/NkFontEmbedded.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkFunctions.h"

// Rendu via NKCanvas : l'atlas devient une NkTexture, le texte est emis en
// quads textures via NkRenderer2D::DrawVertices (plus de raw-GL).
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKImage/Core/NkImage.h"

#include <cstring>
#include <cstdlib>

namespace nkentseu
{
    namespace pong
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
        bool FontAtlas::Init(renderer::NkRenderer2D& r)
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

            // L'atlas NKFont est alpha8 (1 canal = couverture du glyphe). Le
            // shader 2D de NKCanvas echantillonne la texture en RGBA puis la
            // multiplie par la couleur du vertex. On convertit donc l'atlas en
            // RGBA8 "blanc + alpha=couverture" : texture(1,1,1,cov) * color =
            // (color.rgb, color.a*cov) => exactement le rendu de texte voulu.
            const usize pixCount = static_cast<usize>(w) * static_cast<usize>(h);
            NkVector<uint8> rgba;
            rgba.Resize(pixCount * 4u);
            for (usize i = 0; i < pixCount; ++i)
            {
                rgba[i * 4 + 0] = 255;
                rgba[i * 4 + 1] = 255;
                rgba[i * 4 + 2] = 255;
                rgba[i * 4 + 3] = pixels[i];   // couverture du glyphe
            }

            // On enveloppe nos pixels RGBA dans une NkImage puis on upload en un
            // seul LoadFromImage (qui passe par gTextureBackend.Create — cable).
            // NB : surtout PAS Create()+Update() : Update exige gTextureBackend.Update
            // qui n'est pas cable sur tous les backends -> echec silencieux.
            // Signature : Create(w, h, fillColor, desiredChannels=4). On cree une
            // image RGBA (4 canaux) ; la couleur de remplissage importe peu car on
            // ecrase tout via memcpy juste apres.
            NkImage atlasImg;
            if (!atlasImg.Create(static_cast<uint32>(w), static_cast<uint32>(h),
                                 math::NkColor(0, 0, 0, 0), 4))
            {
                logger.Error("[FontAtlas] atlas NkImage create failed");
                return false;
            }
            std::memcpy(atlasImg.Pixels(), rgba.Data(), pixCount * 4);

            mTexture = new renderer::NkTexture();
            if (!mTexture->LoadFromImage(*r.GetBackend(), atlasImg))
            {
                logger.Error("[FontAtlas] NkTexture LoadFromImage failed");
                delete mTexture;
                mTexture = nullptr;
                return false;
            }
            mTexture->SetFilter(renderer::NkTextureFilter::NK_LINEAR);

            logger.Info("[FontAtlas] atlas built: {0}x{1} (NkTexture)", w, h);
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────
        // FontAtlas::Shutdown — libere la texture GL puis l'atlas CPU.
        // ─────────────────────────────────────────────────────────────────────
        void FontAtlas::Shutdown()
        {
            if (mTexture != nullptr)
            {
                delete mTexture;   // ~NkTexture libere la ressource GPU
                mTexture = nullptr;
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
        // PushGlyphQuad — append un quad texture (4 sommets + 6 indices) dans
        // les buffers locaux. UV = position du glyphe dans l'atlas, couleur =
        // couleur du texte (le shader fait texture(blanc,alpha) * color).
        // ─────────────────────────────────────────────────────────────────────
        static void PushGlyphQuad(NkVector<renderer::NkVertex>& verts,
                                  NkVector<uint32>& idx,
                                  float gx, float gy, float gw, float gh,
                                  float u0, float v0, float u1, float v1,
                                  const math::NkColor& c)
        {
            const uint32 base = static_cast<uint32>(verts.Size());
            renderer::NkVertex vtx;
            vtx.r = c.r; vtx.g = c.g; vtx.b = c.b; vtx.a = c.a;
            vtx.x = gx;      vtx.y = gy;      vtx.u = u0; vtx.v = v0; verts.PushBack(vtx); // TL
            vtx.x = gx + gw; vtx.y = gy;      vtx.u = u1; vtx.v = v0; verts.PushBack(vtx); // TR
            vtx.x = gx + gw; vtx.y = gy + gh; vtx.u = u1; vtx.v = v1; verts.PushBack(vtx); // BR
            vtx.x = gx;      vtx.y = gy + gh; vtx.u = u0; vtx.v = v1; verts.PushBack(vtx); // BL
            idx.PushBack(base + 0); idx.PushBack(base + 1); idx.PushBack(base + 2);
            idx.PushBack(base + 0); idx.PushBack(base + 2); idx.PushBack(base + 3);
        }

        // ─────────────────────────────────────────────────────────────────────
        // DrawString — accumule les glyphes en quads textures puis emet 1 seul
        // DrawVertices (atlas NkTexture). (x, y) = top-left du texte ; baseline
        // approximee a 0.82 * sizePx (polices sans-serif Karla / DroidSans).
        // ─────────────────────────────────────────────────────────────────────
        float FontAtlas::DrawString(renderer::NkRenderer2D& r, SizeSlot s,
                                  float x, float y,
                                  const char* text,
                                  math::NkColor color)
        {
            if (text == nullptr || mFonts[s] == nullptr || mTexture == nullptr)
            {
                return 0.0f;
            }
            NkFont* font = mFonts[s];

            const float fontPx = kSizePx[s];
            const float ascent = fontPx * 0.82f;
            float cursorX = x;
            float cursorY = y + ascent;

            NkVector<renderer::NkVertex> verts;
            NkVector<uint32>             idx;

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
                    PushGlyphQuad(verts, idx,
                                  cursorX + g->x0, cursorY + g->y0,
                                  g->x1 - g->x0,   g->y1 - g->y0,
                                  g->u0, g->v0, g->u1, g->v1, color);
                }
                cursorX += g->advanceX;
            }
            if (!verts.Empty())
            {
                r.DrawVertices(verts.Data(), static_cast<uint32>(verts.Size()),
                               idx.Data(),   static_cast<uint32>(idx.Size()),
                               mTexture);
            }
            return cursorX - x;
        }

        float FontAtlas::DrawStringCentered(renderer::NkRenderer2D& r, SizeSlot s,
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

        float FontAtlas::DrawStringScaled(renderer::NkRenderer2D& r, SizeSlot s, float scale,
                                        float x, float y,
                                        const char* text,
                                        math::NkColor color)
        {
            if (text == nullptr || mFonts[s] == nullptr || mTexture == nullptr
                || scale <= 0.0f)
            {
                return 0.0f;
            }
            NkFont* font = mFonts[s];

            const float fontPx = kSizePx[s];
            const float ascent = fontPx * 0.82f * scale;
            float cursorX = x;
            float cursorY = y + ascent;

            NkVector<renderer::NkVertex> verts;
            NkVector<uint32>             idx;

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
                    PushGlyphQuad(verts, idx,
                                  cursorX + g->x0 * scale, cursorY + g->y0 * scale,
                                  (g->x1 - g->x0) * scale, (g->y1 - g->y0) * scale,
                                  g->u0, g->v0, g->u1, g->v1, color);
                }
                cursorX += g->advanceX * scale;
            }
            if (!verts.Empty())
            {
                r.DrawVertices(verts.Data(), static_cast<uint32>(verts.Size()),
                               idx.Data(),   static_cast<uint32>(idx.Size()),
                               mTexture);
            }
            return cursorX - x;
        }

        float FontAtlas::DrawStringCenteredScaled(renderer::NkRenderer2D& r, SizeSlot s,
                                                float scale,
                                                float cx, float y,
                                                const char* text,
                                                math::NkColor color)
        {
            const float w = MeasureWidthScaled(s, scale, text);
            return DrawStringScaled(r, s, scale, cx - w * 0.5f, y, text, color);
        }

        void FontAtlas::DrawStringShadowScaled(renderer::NkRenderer2D& r, SizeSlot s,
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

        void FontAtlas::DrawStringShadowCenteredScaled(renderer::NkRenderer2D& r, SizeSlot s,
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
        void FontAtlas::DrawStringShadow(renderer::NkRenderer2D& r, SizeSlot s,
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

        void FontAtlas::DrawStringShadowCentered(renderer::NkRenderer2D& r, SizeSlot s,
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

    } // namespace pong
} // namespace nkentseu
