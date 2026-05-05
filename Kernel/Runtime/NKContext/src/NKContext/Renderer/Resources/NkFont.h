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
        class NkFont {
            public:
                NkFont()  = default;
                ~NkFont() { Destroy(); }

                NkFont(const NkFont&)            = delete;
                NkFont& operator=(const NkFont&) = delete;

                NkFont(NkFont&&) noexcept;
                NkFont& operator=(NkFont&&) noexcept;

                // ── Load ──────────────────────────────────────────────────────────────
                bool LoadFromFile  (NkIRenderer2D& renderer, const char* path);
                bool LoadFromMemory(NkIRenderer2D& renderer,
                                    const void* data, usize sizeBytes);

                // ── Glyph access ──────────────────────────────────────────────────────
                // Returns the glyph for a Unicode codepoint, rasterized at the given
                // pixel size.  Glyphs are cached; first call triggers rasterization.
                const NkGlyph& GetGlyph(uint32 codepoint, uint32 characterSize, bool bold = false) const;

                // Kerning between two consecutive codepoints
                float32 GetKerning(uint32 first, uint32 second, uint32 characterSize) const;

                // Line height (baseline to baseline) for the given size
                float32 GetLineHeight(uint32 characterSize) const;

                // Glyph atlas texture (the renderer uses this to draw text)
                const NkTexture* GetAtlasTexture(uint32 characterSize) const;

                bool IsValid() const { return mFTFace != nullptr; }

                void Destroy();
            private:

                // FreeType objects (stored as void* to avoid pulling ft2 headers here)
                void* mFTLibrary = nullptr;  // FT_Library
                void* mFTFace    = nullptr;  // FT_Face

                // Font file kept in memory (needed for the FT_Face lifetime)
                NkVector<uint8> mFontData;

                // ── Glyph cache key ───────────────────────────────────────────────────
                struct GlyphKey {
                    uint32 codepoint;
                    uint32 characterSize;
                    bool   bold;
                    bool operator==(const GlyphKey& o) const {
                        return codepoint == o.codepoint &&
                            characterSize == o.characterSize &&
                            bold == o.bold;
                    }
                };

                // ── Atlas page ────────────────────────────────────────────────────────
                // One atlas per character size to keep texture binding changes minimal.
                struct AtlasPage {
                    uint32      characterSize = 0;
                    NkTexture   texture;

                    // Current pack cursor (row-based bin packing)
                    uint32 packX    = 2;   // 2px border
                    uint32 packY    = 2;
                    uint32 rowHeight= 0;
                    uint32 atlasW   = 512;
                    uint32 atlasH   = 512;

                    NkVector<uint8>  cpuPixels; // RGBA atlas pixels (for incremental upload)

                    struct CachedGlyph {
                        GlyphKey key;
                        NkGlyph  glyph;
                    };
                    NkVector<CachedGlyph> cache;

                    const NkGlyph* Find(const GlyphKey& key) const;
                };

                mutable NkVector<AtlasPage> mPages;

                AtlasPage& GetOrCreatePage(NkIRenderer2D& renderer, uint32 charSize) const;
                NkGlyph LoadGlyph(AtlasPage& page, uint32 codepoint, uint32 charSize, bool bold) const;

                NkIRenderer2D* mRenderer = nullptr;
        };

    } // namespace renderer
} // namespace nkentseu