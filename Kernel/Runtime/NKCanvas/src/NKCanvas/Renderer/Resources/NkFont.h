#pragma once
// =============================================================================
// NkFont.h — TrueType / OpenType font rasterization via FreeType 2
// Similar to sf::Font — loads a font, rasterizes glyphs, packs into a texture atlas.
//
// Usage:
//   NkFont font;
//   font.LoadFromFile(renderer, "assets/Roboto-Regular.ttf");
//   NkText label(font, "Hello World!", 24);
//   label.SetPosition({100, 200});
//   renderer.Draw(label);
// =============================================================================
#include "NkTexture.h"
#include "NKContainers/Sequential/NkVector.h"
#include <cstddef>

namespace nkentseu {

    // Forward declarations du module externe NKFont (NKFont/NkFont.h).
    // renderer::NkFont ci-dessous est un WRAPPER GPU autour de ces types CPU.
    struct NkFontAtlas;
    struct NkFont;

    namespace renderer {

        class NkIRenderer2D;

        // ── Per-glyph metrics ────────────────────────────────────────────────────
        struct NkGlyph {
            NkRect2i  textureRect;      // Sub-rect in the atlas texture (pixels)
            NkRect2f  bounds;           // Local bounding box (bearing + size, in units)
            float32   advance = 0.f;    // Horizontal advance width
            bool      valid   = false;
        };

        // ── Font rendering style flags ───────────────────────────────────────────
        enum class NkTextStyle : uint32 {
            NK_REGULAR          = 0,
            NK_BOLD             = 1 << 0,
            NK_ITALIC           = 1 << 1,
            NK_UNDERLINED       = 1 << 2,
            NK_STRIKE_THROUGH   = 1 << 3,
        };

        inline NkTextStyle operator|(NkTextStyle a, NkTextStyle b) {
            return static_cast<NkTextStyle>(uint32(a) | uint32(b));
        }
        
        inline bool HasStyle(NkTextStyle s, NkTextStyle f) {
            return (uint32(s) & uint32(f)) != 0;
        }

        // =========================================================================
        // NkFont — police rasterisée, wrapper GPU du module externe NKFont.
        //
        // Depuis le refactoring 2026-05-28, NKCanvas ne réimplémente plus la
        // rasterisation (ex-FreeType supprimé). renderer::NkFont délègue au module
        // NKFont (nkentseu::NkFontAtlas + nkentseu::NkFont) : une "page" (atlas
        // rasterisé + texture GPU) par taille de caractère demandée. Le module
        // rasterise tous les glyphes d'une fonte à une taille fixe lors de Build().
        //
        // API conservée pour NkText (SFML-like). GetGlyph(cp, charSize, bold).
        // =========================================================================
        class NkFont {
            public:
                NkFont()  = default;
                ~NkFont() { Destroy(); }

                NkFont(const NkFont&)            = delete;
                NkFont& operator=(const NkFont&) = delete;

                NkFont(NkFont&& other) noexcept;
                NkFont& operator=(NkFont&& other) noexcept;

                // ── Load (le renderer sert à créer les textures atlas GPU) ──────────
                bool LoadFromFile  (NkIRenderer2D& renderer, const char* path);
                bool LoadFromMemory(NkIRenderer2D& renderer,
                                    const void* data, usize sizeBytes);

                // ── Glyph access ────────────────────────────────────────────────────
                // bold ignoré (le module ne fait pas de faux-gras ; charger une
                // fonte bold dédiée si nécessaire).
                const NkGlyph& GetGlyph(uint32 codepoint, uint32 characterSize,
                                        bool bold = false) const;

                // Le module NKFont n'expose pas le kerning par paire -> 0.
                float32 GetKerning(uint32 first, uint32 second,
                                   uint32 characterSize) const;

                float32 GetLineHeight(uint32 characterSize) const;

                const NkTexture* GetAtlasTexture(uint32 characterSize) const;

                bool IsValid() const { return !mFontData.Empty(); }

                void Destroy();

            private:
                // Une page = atlas module (rasterisé à une taille) + texture GPU.
                struct Page {
                    uint32                    characterSize = 0;
                    ::nkentseu::NkFontAtlas*  atlas         = nullptr; // heap (placement new)
                    ::nkentseu::NkFont*       moduleFont    = nullptr; // possédé par atlas
                    NkTexture                 texture;
                };

                Page* GetOrCreatePage(uint32 characterSize) const;

                NkVector<uint8>          mFontData;     // données fonte conservées vivantes
                mutable NkVector<Page*>  mPages;        // une page par taille de caractère
                NkIRenderer2D*           mRenderer = nullptr;
                mutable NkGlyph          mScratchGlyph; // pour retourner une const ref
        };

    } // namespace renderer
} // namespace nkentseu