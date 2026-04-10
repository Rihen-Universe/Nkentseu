// =============================================================================
// NkSprite.cpp + NkText.cpp — Drawable implementations
// =============================================================================
#include "NkSprite.h"
#include "NKContext/Renderer/Core/NkIRenderer2D.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // NkSprite
        // =============================================================================
        NkSprite::NkSprite(const NkTexture& texture) {
            SetTexture(texture, true);
        }

        NkSprite::NkSprite(const NkTexture& texture, const NkRect2i& textureRect) {
            SetTexture(texture, false);
            SetTextureRect(textureRect);
        }

        void NkSprite::SetTexture(const NkTexture& texture, bool resetRect) {
            mTexture = &texture;
            if (resetRect || (mTextureRect.width == 0 && mTextureRect.height == 0)) {
                mTextureRect = { 0, 0, (int32)texture.GetWidth(), (int32)texture.GetHeight() };
            }
        }

        void NkSprite::SetTextureRect(const NkRect2i& rect) {
            mTextureRect = rect;
        }

        NkRect2f NkSprite::GetLocalBounds() const {
            return { 0.f, 0.f, (float32)mTextureRect.width, (float32)mTextureRect.height };
        }

        NkRect2f NkSprite::GetGlobalBounds() const {
            // Approximate AABB around the transformed quad
            float32 w = (float32)mTextureRect.width  * mTransform.scale.x;
            float32 h = (float32)mTextureRect.height * mTransform.scale.y;
            const float32 cos_r = fabsf(cosf(mTransform.rotation));
            const float32 sin_r = fabsf(sinf(mTransform.rotation));
            const float32 aabb_w = w * cos_r + h * sin_r;
            const float32 aabb_h = w * sin_r + h * cos_r;
            return {
                mTransform.position.x - aabb_w * 0.5f,
                mTransform.position.y - aabb_h * 0.5f,
                aabb_w, aabb_h
            };
        }

        bool NkSprite::GetFlipX() const { return mFlipX; }
        bool NkSprite::GetFlipY() const { return mFlipY; }

        void NkSprite::Draw(NkIRenderer2D& renderer) const {
            renderer.Draw(*this);
        }

        // =============================================================================
        // NkText
        // =============================================================================
        NkText::NkText(const NkFont& font, const char* string, uint32 characterSize) {
            mFont          = &font;
            mCharacterSize = characterSize;
            SetString(string);
        }

        void NkText::SetString(const char* utf8String) {
            mString.Clear();
            if (utf8String) {
                const char* p = utf8String;
                while (*p) { mString.PushBack(*p++); }
                mString.PushBack('\0');
            } else {
                mString.PushBack('\0');
            }
            mGeometryDirty = true;
        }

        void NkText::SetFont(const NkFont& font) {
            mFont          = &font;
            mGeometryDirty = true;
        }

        void NkText::SetCharacterSize(uint32 size) {
            mCharacterSize = size;
            mGeometryDirty = true;
        }

        void NkText::SetStyle(NkTextStyle style) {
            mStyle         = style;
            mGeometryDirty = true;
        }

        void NkText::SetLetterSpacing(float32 factor) {
            mLetterSpacing = factor;
            mGeometryDirty = true;
        }

        void NkText::SetLineSpacing(float32 factor) {
            mLineSpacing   = factor;
            mGeometryDirty = true;
        }

        // =============================================================================
        // UTF-8 decode: returns codepoint and advances the pointer
        static uint32 Utf8Decode(const char*& it) {
            uint8 c = (uint8)*it++;
            if (c < 0x80)  return c;
            if (c < 0xC0)  return 0xFFFD; // continuation byte — invalid start
            uint32 cp;
            int extra;
            if      (c < 0xE0) { cp = c & 0x1F; extra = 1; }
            else if (c < 0xF0) { cp = c & 0x0F; extra = 2; }
            else               { cp = c & 0x07; extra = 3; }
            while (extra-- > 0) {
                if (!(*it)) break;
                cp = (cp << 6) | ((uint8)*it++ & 0x3F);
            }
            return cp;
        }

        // =============================================================================
        void NkText::EnsureGeometryUpdate() const {
            if (!mGeometryDirty || !mFont || !mFont->IsValid()) return;
            mGeometryDirty = false;
            mVertices.Clear();
            mBounds = {};

            const bool bold       = HasStyle(mStyle, NkTextStyle::NK_BOLD);
            const float32 lineH   = mFont->GetLineHeight(mCharacterSize) * mLineSpacing;

            float32 curX = 0.f;
            float32 curY = 0.f;
            float32 minX = 0.f, maxX = 0.f, minY = 0.f, maxY = 0.f;
            bool first = true;
            uint32 prevCp = 0;

            const char* it = mString.Data();
            while (it && *it) {
                const uint32 cp = Utf8Decode(it);
                if (cp == 0) break;

                if (cp == '\n') {
                    curX  = 0.f;
                    curY += lineH;
                    prevCp = 0;
                    continue;
                }
                if (cp == '\r') continue;

                // Kerning
                if (prevCp) curX += mFont->GetKerning(prevCp, cp, mCharacterSize);
                prevCp = cp;

                const NkGlyph& g = mFont->GetGlyph(cp, mCharacterSize, bold);
                if (!g.valid) {
                    curX += g.advance * mLetterSpacing;
                    continue;
                }

                // Quad corners (local space)
                const float32 left   = curX + g.bounds.left;
                const float32 top    = curY + g.bounds.top;
                const float32 right  = left + g.bounds.width;
                const float32 bottom = top  + g.bounds.height;

                // Texture atlas UV
                // (g.textureRect holds pixel coords in the atlas)
                // We pass these as integer rects; the batch system converts via GetTexCoords
                // For text we bake UV directly into the vertex
                const NkTexture* atlas = mFont->GetAtlasTexture(mCharacterSize);
                float32 u0 = 0, v0 = 0, u1 = 1, v1 = 1;
                if (atlas && atlas->IsValid()) {
                    NkRect2f uv = atlas->GetTexCoords(g.textureRect);
                    u0 = uv.left; v0 = uv.top;
                    u1 = uv.left + uv.width;
                    v1 = uv.top  + uv.height;
                }

                GlyphVertex gv;
                auto fillV = [&](NkVertex2D& v, float32 x, float32 y, float32 u, float32 vv) {
                    v.x = x; v.y = y; v.u = u; v.v = vv;
                    v.r = mFillColor.r; v.g = mFillColor.g;
                    v.b = mFillColor.b; v.a = mFillColor.a;
                };
                fillV(gv.v[0], left,  top,    u0, v0);
                fillV(gv.v[1], right, top,    u1, v0);
                fillV(gv.v[2], right, bottom, u1, v1);
                fillV(gv.v[3], left,  bottom, u0, v1);
                mVertices.PushBack(gv);

                // Update bounds
                if (first) {
                    minX = left; maxX = right; minY = top; maxY = bottom;
                    first = false;
                } else {
                    if (left   < minX) minX = left;
                    if (right  > maxX) maxX = right;
                    if (top    < minY) minY = top;
                    if (bottom > maxY) maxY = bottom;
                }

                curX += g.advance * mLetterSpacing;
            }

            mBounds = { minX, minY, maxX - minX, maxY - minY };
        }

        // =============================================================================
        const NkVector<NkText::GlyphVertex>& NkText::GetVertices() const {
            EnsureGeometryUpdate();
            return mVertices;
        }

        NkRect2f NkText::GetLocalBounds() const {
            EnsureGeometryUpdate();
            return mBounds;
        }

        NkRect2f NkText::GetGlobalBounds() const {
            NkRect2f lb = GetLocalBounds();
            // Apply position (simplified — ignores rotation/scale for AABB)
            return {
                lb.left + mTransform.position.x,
                lb.top  + mTransform.position.y,
                lb.width, lb.height
            };
        }

        // =============================================================================
        void NkText::Draw(NkIRenderer2D& renderer) const {
            EnsureGeometryUpdate();
            if (mVertices.Empty() || !mFont) return;

            const NkTexture* atlas = mFont->GetAtlasTexture(mCharacterSize);

            // Build transform matrix
            float32 tm[16];
            mTransform.ToMatrix4(tm);

            // Emit vertices through DrawVertices — transform them inline
            const uint32 gCount = mVertices.Size();
            NkVector<NkVertex2D> tverts;
            tverts.Reserve((NkVector<NkVertex2D>::SizeType)(gCount * 4));
            NkVector<uint32> indices;
            indices.Reserve((NkVector<uint32>::SizeType)(gCount * 6));

            const float32 cos_r = cosf(mTransform.rotation);
            const float32 sin_r = sinf(mTransform.rotation);
            const float32 sx    = mTransform.scale.x;
            const float32 sy    = mTransform.scale.y;
            const float32 ox    = mTransform.origin.x;
            const float32 oy    = mTransform.origin.y;
            const float32 tx    = mTransform.position.x;
            const float32 ty    = mTransform.position.y;

            for (uint32 g = 0; g < gCount; ++g) {
                const uint32 base = tverts.Size();
                for (int i = 0; i < 4; ++i) {
                    NkVertex2D v = mVertices[g].v[i];
                    // Origin offset + scale + rotate + translate
                    float32 lx = (v.x - ox) * sx;
                    float32 ly = (v.y - oy) * sy;
                    v.x = lx * cos_r - ly * sin_r + tx;
                    v.y = lx * sin_r + ly * cos_r + ty;
                    tverts.PushBack(v);
                }
                indices.PushBack(base+0); indices.PushBack(base+1); indices.PushBack(base+2);
                indices.PushBack(base+0); indices.PushBack(base+2); indices.PushBack(base+3);
            }

            renderer.DrawVertices(tverts.Data(), tverts.Size(),
                                indices.Data(), indices.Size(),
                                atlas);
        }

        uint32 NkText::FindCharacterPos(NkVec2f /*point*/) const {
            return 0; // TODO: binary search on glyph bounds
        }

    } // namespace renderer
} // namespace nkentseu