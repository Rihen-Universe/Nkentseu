#pragma once
// =============================================================================
// NkSprite.h + NkText.h — High-level 2D drawables (similar to SFML)
// =============================================================================
#include "NkTexture.h"
#include "NkFont.h"
#include "NKContext/Renderer/Core/NkIRenderer2D.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        // =========================================================================
        // NkSprite — textured, transformable 2D quad
        // Similar to sf::Sprite
        // =========================================================================
        class NkSprite : public NkIDrawable2D {
            public:
                NkSprite()  = default;
                explicit NkSprite(const NkTexture& texture);
                NkSprite(const NkTexture& texture, const NkRect2i& textureRect);

                // ── Texture ───────────────────────────────────────────────────────────
                void SetTexture(const NkTexture& texture, bool resetRect = false);
                void SetTextureRect(const NkRect2i& rect);
                const NkTexture* GetTexture()     const { return mTexture; }
                NkRect2i         GetTextureRect() const { return mTextureRect; }

                // ── Transform ─────────────────────────────────────────────────────────
                void SetPosition(NkVec2f pos)          { mTransform.position = pos; }
                void SetPosition(float32 x, float32 y) { mTransform.position = {x, y}; }
                void SetRotation(float32 degrees)      { mTransform.rotation = degrees * 3.14159265f / 180.f; }
                void SetScale(NkVec2f scale)           { mTransform.scale = scale; }
                void SetScale(float32 sx, float32 sy)  { mTransform.scale = {sx, sy}; }
                void SetOrigin(NkVec2f origin)         { mTransform.origin = origin; }
                void SetOrigin(float32 ox, float32 oy) { mTransform.origin = {ox, oy}; }

                void Move(NkVec2f offset)  { mTransform.position.x += offset.x; mTransform.position.y += offset.y; }
                void Rotate(float32 deg)   { mTransform.rotation += deg * 3.14159265f / 180.f; }
                void Scale(NkVec2f factor) { mTransform.scale.x *= factor.x; mTransform.scale.y *= factor.y; }

                NkVec2f  GetPosition() const { return mTransform.position; }
                float32  GetRotation() const { return mTransform.rotation * 180.f / 3.14159265f; }
                NkVec2f  GetScale()    const { return mTransform.scale; }
                NkVec2f  GetOrigin()   const { return mTransform.origin; }

                const NkTransform2D& GetTransform() const { return mTransform; }

                // ── Color tint ────────────────────────────────────────────────────────
                void      SetColor(const NkColor2D& color) { mColor = color; }
                NkColor2D GetColor()                 const { return mColor; }

                // ── Flip ──────────────────────────────────────────────────────────────
                void SetFlipX(bool flip) { mFlipX = flip; }
                void SetFlipY(bool flip) { mFlipY = flip; }
                bool GetFlipX() const;
                bool GetFlipY() const;

                // ── Bounds ────────────────────────────────────────────────────────────
                NkRect2f GetLocalBounds()  const;
                NkRect2f GetGlobalBounds() const;

                // ── NkIDrawable2D ─────────────────────────────────────────────────────
                void Draw(NkIRenderer2D& renderer) const override;

            private:
                const NkTexture* mTexture     = nullptr;
                NkRect2i         mTextureRect;
                NkTransform2D    mTransform;
                NkColor2D        mColor       = NkColor2D::White();
                bool             mFlipX       = false;
                bool             mFlipY       = false;
        };

        // =========================================================================
        // NkText — UTF-8 text drawable with FreeType-rasterized glyphs
        // Similar to sf::Text
        // =========================================================================
        class NkText : public NkIDrawable2D {
            public:
                NkText()  = default;
                NkText(const NkFont& font, const char* string, uint32 characterSize = 24);

                // ── Content ───────────────────────────────────────────────────────────
                void SetString(const char* utf8String);
                void SetFont(const NkFont& font);
                void SetCharacterSize(uint32 size);
                void SetStyle(NkTextStyle style);
                void SetLetterSpacing(float32 factor);  // multiplier on advance (1.0 = normal)
                void SetLineSpacing(float32 factor);    // multiplier on line height

                const char*    GetString()        const { return mString.Data(); }
                uint32         GetCharacterSize() const { return mCharacterSize; }
                NkTextStyle    GetStyle()         const { return mStyle; }

                // ── Transform (same as sprite) ────────────────────────────────────────
                void SetPosition(NkVec2f pos)          { mTransform.position = pos; }
                void SetPosition(float32 x, float32 y) { mTransform.position = {x, y}; }
                void SetRotation(float32 degrees)      { mTransform.rotation = degrees * 3.14159265f / 180.f; }
                void SetScale(NkVec2f scale)           { mTransform.scale = scale; }
                void SetOrigin(NkVec2f origin)         { mTransform.origin = origin; }
                void Move(NkVec2f offset) {
                    mTransform.position.x += offset.x;
                    mTransform.position.y += offset.y;
                }

                NkVec2f GetPosition() const { return mTransform.position; }
                float32 GetRotation() const { return mTransform.rotation * 180.f / 3.14159265f; }
                NkVec2f GetScale()    const { return mTransform.scale; }

                const NkTransform2D& GetTransform() const { return mTransform; }

                // ── Color ─────────────────────────────────────────────────────────────
                void      SetFillColor   (const NkColor2D& c) { mFillColor    = c; }
                void      SetOutlineColor(const NkColor2D& c) { mOutlineColor = c; }
                void      SetOutlineThickness(float32 t)      { mOutlineThickness = t; }
                NkColor2D GetFillColor()        const { return mFillColor; }
                NkColor2D GetOutlineColor()     const { return mOutlineColor; }
                float32   GetOutlineThickness() const { return mOutlineThickness; }

                // ── Bounds ────────────────────────────────────────────────────────────
                NkRect2f GetLocalBounds()  const;
                NkRect2f GetGlobalBounds() const;

                // Find the index of the character at a given position
                uint32 FindCharacterPos(NkVec2f point) const;

                // ── NkIDrawable2D ─────────────────────────────────────────────────────
                void Draw(NkIRenderer2D& renderer) const override;

                // Internal: build vertex array for the current string
                struct GlyphVertex { NkVertex2D v[4]; };
                const NkVector<GlyphVertex>& GetVertices() const;

            private:
                void EnsureGeometryUpdate() const;

                const NkFont*      mFont          = nullptr;
                NkVector<char>     mString;
                uint32             mCharacterSize = 24;
                NkTextStyle        mStyle         = NkTextStyle::NK_REGULAR;
                float32            mLetterSpacing = 1.f;
                float32            mLineSpacing   = 1.f;
                NkColor2D          mFillColor     = NkColor2D::White();
                NkColor2D          mOutlineColor  = NkColor2D::Black();
                float32            mOutlineThickness = 0.f;
                NkTransform2D      mTransform;

                mutable bool                 mGeometryDirty = true;
                mutable NkVector<GlyphVertex> mVertices;
                mutable NkRect2f             mBounds;
        };

    } // namespace renderer
} // namespace nkentseu