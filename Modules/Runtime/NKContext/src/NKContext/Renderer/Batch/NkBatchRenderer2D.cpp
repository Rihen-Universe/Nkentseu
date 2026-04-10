// =============================================================================
// NkBatchRenderer2D.cpp — CPU geometry generation + batching logic
// =============================================================================
#include "NkBatchRenderer2D.h"
#include "NKContext/Renderer/Resources/NkSprite.h"
#include "NKContext/Renderer/Resources/NkTexture.h"
#include "NKLogger/NkLog.h"

#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        NkBatchRenderer2D::NkBatchRenderer2D() {
            mVertices.Reserve((NkVector<NkVertex2D>::SizeType)kMaxVertices);
            mIndices.Reserve((NkVector<uint32>::SizeType)kMaxIndices);
            mGroups.Reserve(64);
        }

        // =============================================================================
        bool NkBatchRenderer2D::Begin() {
            if (mInFrame) { logger.Warnf("[NkBatch] Begin() called twice"); return false; }
            mInFrame = true;
            mVertices.Clear();
            mIndices.Clear();
            mGroups.Clear();
            mCurrentTexture = nullptr;
            BeginBackend();
            return true;
        }

        // =============================================================================
        void NkBatchRenderer2D::End() {
            if (!mInFrame) return;
            Flush();
            EndBackend();
            mInFrame = false;
        }

        // =============================================================================
        void NkBatchRenderer2D::Flush() {
            if (mGroups.Empty() || mVertices.Empty()) return;

            // Close current group
            if (!mGroups.Empty()) {
                auto& g = mGroups.Back();
                g.indexCount = mIndices.Size() - g.indexStart;
            }

            // Remove empty groups
            uint32 validCount = 0;
            for (uint32 i = 0; i < mGroups.Size(); ++i) {
                if (mGroups[i].indexCount > 0) {
                    if (validCount != i) mGroups[validCount] = mGroups[i];
                    ++validCount;
                }
            }

            if (validCount > 0) {
                SubmitBatches(mGroups.Data(), validCount,
                            mVertices.Data(), mVertices.Size(),
                            mIndices.Data(),  mIndices.Size());
                mStats.drawCalls   += validCount;
                mStats.vertexCount += mVertices.Size();
                mStats.indexCount  += mIndices.Size();
            }

            mVertices.Clear();
            mIndices.Clear();
            mGroups.Clear();
            mCurrentTexture = nullptr;
        }

        // =============================================================================
        void NkBatchRenderer2D::SetView(const NkView2D& view) {
            Flush(); // commit current batch before changing projection
            mCurrentView = view;
            float proj[16];
            view.ToProjectionMatrix(proj);
            UploadProjection(proj);
        }

        // =============================================================================
        void NkBatchRenderer2D::SetBlendMode(NkBlendMode mode) {
            if (mBlendMode == mode) return;
            Flush();
            mBlendMode = mode;
        }

        // =============================================================================
        void NkBatchRenderer2D::EnsureGroup(const NkTexture* tex, NkBlendMode blend) {
            bool needNew = mGroups.Empty();
            if (!needNew) {
                const auto& back = mGroups.Back();
                needNew = (back.texture != tex || back.blendMode != blend);
            }
            if (needNew) {
                // Close previous group
                if (!mGroups.Empty()) {
                    mGroups.Back().indexCount = mIndices.Size() - mGroups.Back().indexStart;
                }
                NkBatchGroup g;
                g.texture    = tex;
                g.blendMode  = blend;
                g.indexStart = mIndices.Size();
                g.indexCount = 0;
                mGroups.PushBack(g);
                if (tex != mCurrentTexture) {
                    ++mStats.textureSwap;
                    mCurrentTexture = tex;
                }
            }
        }

        // =============================================================================
        void NkBatchRenderer2D::PushQuad(NkVec2f tl, NkVec2f tr, NkVec2f br, NkVec2f bl,
                                        NkVec2f uvTL, NkVec2f uvBR,
                                        const NkColor2D& color,
                                        const NkTexture* texture) {
            // Auto-flush if full
            if (mVertices.Size() + 4 > kMaxVertices || mIndices.Size() + 6 > kMaxIndices) {
                Flush();
            }
            EnsureGroup(texture, mBlendMode);

            const uint32 base = mVertices.Size();
            NkVertex2D v;
            v.r = color.r; v.g = color.g; v.b = color.b; v.a = color.a;

            v.x = tl.x; v.y = tl.y; v.u = uvTL.x; v.v = uvTL.y; mVertices.PushBack(v);
            v.x = tr.x; v.y = tr.y; v.u = uvBR.x; v.v = uvTL.y; mVertices.PushBack(v);
            v.x = br.x; v.y = br.y; v.u = uvBR.x; v.v = uvBR.y; mVertices.PushBack(v);
            v.x = bl.x; v.y = bl.y; v.u = uvTL.x; v.v = uvBR.y; mVertices.PushBack(v);

            mIndices.PushBack(base+0); mIndices.PushBack(base+1); mIndices.PushBack(base+2);
            mIndices.PushBack(base+0); mIndices.PushBack(base+2); mIndices.PushBack(base+3);
        }

        // =============================================================================
        void NkBatchRenderer2D::Draw(const NkSprite& sprite) {
            const NkTexture* tex = sprite.GetTexture();
            if (!tex) return;

            NkRect2i  srcRect = sprite.GetTextureRect();
            NkRect2f  uvRect  = tex->GetTexCoords(srcRect);
            NkColor2D col     = sprite.GetColor();

            const float32 w = (float32)srcRect.width;
            const float32 h = (float32)srcRect.height;

            // Local corners (centered at origin before transform)
            NkVec2f corners[4] = {
                {0, 0}, {w, 0}, {w, h}, {0, h}
            };

            // Apply transform
            const NkTransform2D& t = sprite.GetTransform();
            const float32 cos_r = cosf(t.rotation);
            const float32 sin_r = sinf(t.rotation);

            for (auto& c : corners) {
                // Origin offset
                c.x -= t.origin.x;
                c.y -= t.origin.y;
                // Scale
                c.x *= t.scale.x;
                c.y *= t.scale.y;
                // Rotate
                const float32 rx = c.x * cos_r - c.y * sin_r;
                const float32 ry = c.x * sin_r + c.y * cos_r;
                // Translate
                c.x = rx + t.position.x;
                c.y = ry + t.position.y;
            }

            // UV flip
            NkVec2f uvTL = {uvRect.left, uvRect.top};
            NkVec2f uvBR = {uvRect.left + uvRect.width, uvRect.top + uvRect.height};
            // if (sprite.GetFlipX ? false : false) {}  // handled via sprite accessors if needed

            if (mVertices.Size() + 4 > kMaxVertices || mIndices.Size() + 6 > kMaxIndices) Flush();
            EnsureGroup(tex, mBlendMode);

            const uint32 base = mVertices.Size();
            NkVertex2D v;
            v.r = col.r; v.g = col.g; v.b = col.b; v.a = col.a;

            v.x = corners[0].x; v.y = corners[0].y; v.u = uvTL.x; v.v = uvTL.y; mVertices.PushBack(v);
            v.x = corners[1].x; v.y = corners[1].y; v.u = uvBR.x; v.v = uvTL.y; mVertices.PushBack(v);
            v.x = corners[2].x; v.y = corners[2].y; v.u = uvBR.x; v.v = uvBR.y; mVertices.PushBack(v);
            v.x = corners[3].x; v.y = corners[3].y; v.u = uvTL.x; v.v = uvBR.y; mVertices.PushBack(v);

            mIndices.PushBack(base+0); mIndices.PushBack(base+1); mIndices.PushBack(base+2);
            mIndices.PushBack(base+0); mIndices.PushBack(base+2); mIndices.PushBack(base+3);
        }

        // =============================================================================
        void NkBatchRenderer2D::Draw(const NkText& text) {
            if (!text.GetString() || !*text.GetString()) return;
            // Geometry is built lazily inside NkText::GetVertices()
            // We just submit it here with the atlas texture.
            const auto& verts = text.GetVertices();
            if (verts.Empty()) return;

            // Resolve atlas texture for this character size
            // (NkText caches font reference; we pull the atlas from it)
            // This is a simplified path — a full implementation would call font.GetAtlasTexture()
            // In practice NkText::Draw() could directly emit into the batch via DrawVertices().
            text.Draw(*this);
        }

        // =============================================================================
        void NkBatchRenderer2D::DrawPoint(NkVec2f pos, const NkColor2D& col, float32 size) {
            const float32 half = size * 0.5f;
            DrawFilledRect({pos.x - half, pos.y - half, size, size}, col);
        }

        // =============================================================================
        void NkBatchRenderer2D::DrawLine(NkVec2f a, NkVec2f b,
                                        const NkColor2D& col, float32 thick) {
            // Build a thick line as a quad
            float32 dx = b.x - a.x;
            float32 dy = b.y - a.y;
            const float32 len = sqrtf(dx*dx + dy*dy);
            if (len < 1e-5f) return;
            dx /= len; dy /= len;
            // Perpendicular
            const float32 px = -dy * thick * 0.5f;
            const float32 py =  dx * thick * 0.5f;

            NkTexture* white = NkTexture::GetWhiteTexture(*this);
            PushQuad(
                {a.x + px, a.y + py},
                {b.x + px, b.y + py},
                {b.x - px, b.y - py},
                {a.x - px, a.y - py},
                {0,0}, {1,1}, col, white
            );
        }

        // =============================================================================
        void NkBatchRenderer2D::DrawFilledRect(NkRect2f r, const NkColor2D& col) {
            NkTexture* white = NkTexture::GetWhiteTexture(*this);
            PushQuad(
                {r.left, r.top},
                {r.left + r.width, r.top},
                {r.left + r.width, r.top + r.height},
                {r.left, r.top + r.height},
                {0,0}, {1,1}, col, white
            );
        }

        // =============================================================================
        void NkBatchRenderer2D::DrawRect(NkRect2f r, const NkColor2D& col,
                                        float32 outline, const NkColor2D& oc) {
            DrawFilledRect(r, col);
            if (outline > 0.f) {
                const float32 t = outline;
                // Top
                DrawFilledRect({r.left-t, r.top-t, r.width+2*t, t}, oc);
                // Bottom
                DrawFilledRect({r.left-t, r.top+r.height, r.width+2*t, t}, oc);
                // Left
                DrawFilledRect({r.left-t, r.top, t, r.height}, oc);
                // Right
                DrawFilledRect({r.left+r.width, r.top, t, r.height}, oc);
            }
        }

        // =============================================================================
        void NkBatchRenderer2D::DrawFilledCircle(NkVec2f center, float32 radius,
                                                const NkColor2D& col, uint32 segs) {
            if (segs < 3) segs = 3;
            NkTexture* white = NkTexture::GetWhiteTexture(*this);

            // Fan triangulation: center + rim
            const uint32 triCount = segs;
            if (mVertices.Size() + triCount*3 > kMaxVertices) Flush();
            EnsureGroup(white, mBlendMode);

            for (uint32 i = 0; i < segs; ++i) {
                const float32 a0 = (float32)(i)   / segs * (float32)(M_PI * 2.0);
                const float32 a1 = (float32)(i+1) / segs * (float32)(M_PI * 2.0);

                const uint32 base = mVertices.Size();
                NkVertex2D v;
                v.r = col.r; v.g = col.g; v.b = col.b; v.a = col.a;

                v.x = center.x;                     v.y = center.y;                     v.u = 0.5f; v.v = 0.5f; mVertices.PushBack(v);
                v.x = center.x + cosf(a0)*radius;   v.y = center.y + sinf(a0)*radius;   v.u = 1.f;  v.v = 0.5f; mVertices.PushBack(v);
                v.x = center.x + cosf(a1)*radius;   v.y = center.y + sinf(a1)*radius;   v.u = 1.f;  v.v = 1.f;  mVertices.PushBack(v);

                mIndices.PushBack(base); mIndices.PushBack(base+1); mIndices.PushBack(base+2);
            }
        }

        // =============================================================================
        void NkBatchRenderer2D::DrawCircle(NkVec2f center, float32 radius,
                                            const NkColor2D& col, uint32 segs,
                                            float32 outline, const NkColor2D& oc) {
            DrawFilledCircle(center, radius, col, segs);
            if (outline > 0.f) {
                DrawFilledCircle(center, radius + outline, oc, segs);
                DrawFilledCircle(center, radius,            col, segs); // overdraw inner
            }
        }

        // =============================================================================
        void NkBatchRenderer2D::DrawFilledTriangle(NkVec2f a, NkVec2f b, NkVec2f c,
                                                    const NkColor2D& col) {
            NkTexture* white = NkTexture::GetWhiteTexture(*this);
            if (mVertices.Size() + 3 > kMaxVertices || mIndices.Size() + 3 > kMaxIndices) Flush();
            EnsureGroup(white, mBlendMode);

            const uint32 base = mVertices.Size();
            NkVertex2D v;
            v.r = col.r; v.g = col.g; v.b = col.b; v.a = col.a;
            v.x = a.x; v.y = a.y; v.u = 0; v.v = 0; mVertices.PushBack(v);
            v.x = b.x; v.y = b.y; v.u = 1; v.v = 0; mVertices.PushBack(v);
            v.x = c.x; v.y = c.y; v.u = 0; v.v = 1; mVertices.PushBack(v);
            mIndices.PushBack(base); mIndices.PushBack(base+1); mIndices.PushBack(base+2);
        }

        // =============================================================================
        void NkBatchRenderer2D::DrawTriangle(NkVec2f a, NkVec2f b, NkVec2f c,
                                            const NkColor2D& col, float32 outline,
                                            const NkColor2D& oc) {
            DrawFilledTriangle(a, b, c, col);
            if (outline > 0.f) {
                DrawLine(a, b, oc, outline);
                DrawLine(b, c, oc, outline);
                DrawLine(c, a, oc, outline);
            }
        }

        // =============================================================================
        void NkBatchRenderer2D::DrawVertices(const NkVertex2D* verts, uint32 vCount,
                                            const uint32* idx, uint32 iCount,
                                            const NkTexture* tex) {
            if (!verts || vCount == 0 || !idx || iCount == 0) return;
            if (mVertices.Size() + vCount > kMaxVertices || mIndices.Size() + iCount > kMaxIndices) Flush();
            EnsureGroup(tex, mBlendMode);

            const uint32 base = mVertices.Size();
            for (uint32 i = 0; i < vCount; ++i) mVertices.PushBack(verts[i]);
            for (uint32 i = 0; i < iCount; ++i) mIndices.PushBack(idx[i] + base);
        }

        // =============================================================================
        NkVec2f NkBatchRenderer2D::MapPixelToCoords(NkVec2i pixel) const {
            // Simple orthographic un-projection
            const float32 nx = ((float32)pixel.x / mViewport.width  * 2.f - 1.f);
            const float32 ny = (1.f - (float32)pixel.y / mViewport.height * 2.f);
            return { nx * mCurrentView.size.x * 0.5f + mCurrentView.center.x,
                    ny * mCurrentView.size.y * 0.5f + mCurrentView.center.y };
        }

        NkVec2i NkBatchRenderer2D::MapCoordsToPixel(NkVec2f point) const {
            const float32 nx = (point.x - mCurrentView.center.x) / (mCurrentView.size.x * 0.5f);
            const float32 ny = (point.y - mCurrentView.center.y) / (mCurrentView.size.y * 0.5f);
            return { (int32)((nx + 1.f) * 0.5f * mViewport.width),
                    (int32)((1.f - ny) * 0.5f * mViewport.height) };
        }

    } // namespace renderer
} // namespace nkentseu