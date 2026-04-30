#include "NkRenderer2D.h"

#include "NKLogger/NkLog.h"
#include "NKRenderer/Core/NkShaderAsset.h"
#include "NKRHI/Core/NkGraphicsApi.h"

#include <cmath>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        namespace {
            static NkVec2f V2(float x, float y) {
                NkVec2f v{};
                v.x = x;
                v.y = y;
                return v;
            }

            static NkVec2f V2Add(const NkVec2f& a, const NkVec2f& b) {
                return V2(a.x + b.x, a.y + b.y);
            }

            static NkVec2f V2Sub(const NkVec2f& a, const NkVec2f& b) {
                return V2(a.x - b.x, a.y - b.y);
            }

            static NkVec2f V2Mul(const NkVec2f& a, float s) {
                return V2(a.x * s, a.y * s);
            }

            static float V2Len(const NkVec2f& a) {
                return NkSqrt(a.x * a.x + a.y * a.y);
            }

            static NkVec2f V2Norm(const NkVec2f& a, const NkVec2f& fallback = NkVec2f{1.0f, 0.0f}) {
                const float l = V2Len(a);
                if (l <= 1e-6f) return fallback;
                const float inv = 1.0f / l;
                return V2(a.x * inv, a.y * inv);
            }

            static NkVec2f V2Perp(const NkVec2f& a) {
                return V2(-a.y, a.x);
            }

            static NkVec2f RotatePoint(const NkVec2f& p, float radians) {
                const float c = NkCos(radians);
                const float s = NkSin(radians);
                return V2(p.x * c - p.y * s, p.x * s + p.y * c);
            }

            static float Cross2(const NkVec2f& a, const NkVec2f& b, const NkVec2f& c) {
                return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            }

            static float PolySignedArea(const NkVec2f* pts, uint32 count) {
                if (!pts || count < 3) return 0.0f;
                float a = 0.0f;
                for (uint32 i = 0; i < count; ++i) {
                    const NkVec2f& p = pts[i];
                    const NkVec2f& q = pts[(i + 1) % count];
                    a += p.x * q.y - q.x * p.y;
                }
                return 0.5f * a;
            }

            static bool PointInTri(const NkVec2f& p, const NkVec2f& a, const NkVec2f& b, const NkVec2f& c) {
                const float c1 = Cross2(a, b, p);
                const float c2 = Cross2(b, c, p);
                const float c3 = Cross2(c, a, p);
                const bool hasNeg = (c1 < 0.0f) || (c2 < 0.0f) || (c3 < 0.0f);
                const bool hasPos = (c1 > 0.0f) || (c2 > 0.0f) || (c3 > 0.0f);
                return !(hasNeg && hasPos);
            }

            static NkBlendDesc BlendFromMode(NkBlendMode mode) {
                switch (mode) {
                    case NkBlendMode::NK_OPAQUE:   return NkBlendDesc::Opaque();
                    case NkBlendMode::NK_ADDITIVE: return NkBlendDesc::Additive();
                    default:                       return NkBlendDesc::Alpha();
                }
            }

            static uint64 MakeTexKey(NkTextureHandle tex, NkSamplerHandle samp) {
                return (tex.id << 1) ^ (samp.id * 0x9E3779B97F4A7C15ull);
            }

            static NkBatchVert MakeVert(NkVec2f p, NkVec2f uv, NkColor4f c) {
                NkBatchVert v{};
                v.x = p.x;
                v.y = p.y;
                v.u = uv.x;
                v.v = uv.y;
                v.r = c.r;
                v.g = c.g;
                v.b = c.b;
                v.a = c.a;
                return v;
            }
        } // namespace

        void NkSpriteAnimator::SetAnimation(const NkSpriteAnimation* anim) {
            mAnim = anim;
            mTime = 0.0f;
            mFrame = 0;
            mForward = true;
            mPlaying = (anim != nullptr);
        }

        void NkSpriteAnimator::Update(float dt) {
            if (!mAnim || !mPlaying || dt <= 0.0f || mAnim->frames.Empty()) return;

            const float fps = NkMax(0.01f, mAnim->fps);
            const float frameDuration = 1.0f / fps;
            mTime += dt;

            while (mTime >= frameDuration) {
                mTime -= frameDuration;
                if (!mAnim->pingPong) {
                    ++mFrame;
                    if (mFrame >= (uint32)mAnim->frames.Size()) {
                        if (mAnim->loop) mFrame = 0;
                        else {
                            mFrame = (uint32)mAnim->frames.Size() - 1;
                            mPlaying = false;
                            break;
                        }
                    }
                } else {
                    if (mForward) {
                        ++mFrame;
                        if (mFrame >= (uint32)mAnim->frames.Size()) {
                            if (mAnim->loop) {
                                mFrame = (uint32)NkMax<int32>(0, (int32)mAnim->frames.Size() - 2);
                                mForward = false;
                            } else {
                                mFrame = (uint32)mAnim->frames.Size() - 1;
                                mPlaying = false;
                                break;
                            }
                        }
                    } else {
                        if (mFrame == 0) {
                            if (mAnim->loop) {
                                mFrame = 1;
                                mForward = true;
                            } else {
                                mPlaying = false;
                                break;
                            }
                        } else {
                            --mFrame;
                        }
                    }
                }
            }
        }

        NkVec4f NkSpriteAnimator::GetCurrentUV() const {
            if (!mAnim || mAnim->frames.Empty()) return {0, 0, 1, 1};
            const uint32 idx = NkMin<uint32>(mFrame, (uint32)mAnim->frames.Size() - 1);
            return mAnim->frames[idx];
        }

        NkVec2f NkCamera2D::WorldToScreen(const NkVec2f& p, float vw, float vh) const {
            if (vw <= 0.0f || vh <= 0.0f) return {0, 0};
            const NkVec2f rel = {p.x - center.x, p.y - center.y};
            const float c = NkCos(-rotation);
            const float s = NkSin(-rotation);
            const NkVec2f r = {rel.x * c - rel.y * s, rel.x * s + rel.y * c};

            const float hw = (vw * 0.5f) / NkMax(0.01f, zoom);
            const float hh = (vh * 0.5f) / NkMax(0.01f, zoom);
            const float ndcX = r.x / NkMax(1e-6f, hw);
            const float ndcY = r.y / NkMax(1e-6f, hh);
            const float sx = (ndcX * 0.5f + 0.5f) * vw;
            const float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * vh;
            return {sx, sy};
        }

        NkVec2f NkCamera2D::ScreenToWorld(const NkVec2f& p, float vw, float vh) const {
            if (vw <= 0.0f || vh <= 0.0f) return center;

            const float ndcX = (p.x / vw) * 2.0f - 1.0f;
            const float ndcY = 1.0f - (p.y / vh) * 2.0f;
            const float hw = (vw * 0.5f) / NkMax(0.01f, zoom);
            const float hh = (vh * 0.5f) / NkMax(0.01f, zoom);

            NkVec2f r = {ndcX * hw, ndcY * hh};
            const float c = NkCos(rotation);
            const float s = NkSin(rotation);
            NkVec2f w = {r.x * c - r.y * s, r.x * s + r.y * c};
            w.x += center.x;
            w.y += center.y;
            return w;
        }

        bool NkRenderer2D::Init(NkIDevice* device, NkRenderPassHandle renderPass, uint32 width, uint32 height) {
            Shutdown();
            if (!device || !device->IsValid() || !renderPass.IsValid() || width == 0 || height == 0) {
                return false;
            }

            mDevice = device;
            mRenderPass = renderPass;
            mWidth = width;
            mHeight = height;
            mInFrame = false;
            mScissorDirty = true;

            if (!InitBuffers()) { Shutdown(); return false; }
            if (!InitBuiltinTextures()) { Shutdown(); return false; }
            if (!InitDescriptors()) { Shutdown(); return false; }
            if (!InitShaders()) { Shutdown(); return false; }
            if (!InitPipelines()) { Shutdown(); return false; }

            return true;
        }

        void NkRenderer2D::Shutdown() {
            if (!mDevice) {
                mVertexBuffer.Clear();
                mIndexBuffer.Clear();
                mBatches.Clear();
                mClipStack.Clear();
                return;
            }

            if (mSpritePipeline.IsValid()) mDevice->DestroyPipeline(mSpritePipeline);
            if (mShapeOpaquePipeline.IsValid()) mDevice->DestroyPipeline(mShapeOpaquePipeline);
            if (mShapeAlphaPipeline.IsValid()) mDevice->DestroyPipeline(mShapeAlphaPipeline);
            if (mShapeAdditivePipeline.IsValid()) mDevice->DestroyPipeline(mShapeAdditivePipeline);
            if (mSpriteShader.IsValid()) mDevice->DestroyShader(mSpriteShader);
            if (mShapeShader.IsValid()) mDevice->DestroyShader(mShapeShader);

            mTexDescSets.ForEach([&](const uint64&, NkDescSetHandle set) {
                if (set.IsValid() && set != mDescSet) mDevice->FreeDescriptorSet(set);
            });
            mTexDescSets.Clear();
            if (mDescSet.IsValid()) mDevice->FreeDescriptorSet(mDescSet);
            if (mLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mLayout);

            if (mUBO.IsValid()) mDevice->DestroyBuffer(mUBO);
            if (mIBO.IsValid()) mDevice->DestroyBuffer(mIBO);
            if (mVBO.IsValid()) mDevice->DestroyBuffer(mVBO);

            if (mWhiteTex) { delete mWhiteTex; mWhiteTex = nullptr; }
            if (mCircleTex) { delete mCircleTex; mCircleTex = nullptr; }

            mSpritePipeline = {};
            mShapeOpaquePipeline = {};
            mShapeAlphaPipeline = {};
            mShapeAdditivePipeline = {};
            mSpriteShader = {};
            mShapeShader = {};
            mLayout = {};
            mDescSet = {};
            mUBO = {};
            mIBO = {};
            mVBO = {};

            mVertexBuffer.Clear();
            mIndexBuffer.Clear();
            mBatches.Clear();
            mClipStack.Clear();
            mCurrentBatch = nullptr;
            mCmd = nullptr;
            mDevice = nullptr;
            mWidth = 0;
            mHeight = 0;
            mInFrame = false;
            mScissorDirty = false;
            mStats = {};
        }

        void NkRenderer2D::Resize(uint32 width, uint32 height) {
            mWidth = width;
            mHeight = height;
            mScissorDirty = true;
        }

        void NkRenderer2D::Begin(NkICommandBuffer* cmd, const NkCamera2D& camera) {
            if (!mDevice || !cmd || !mVBO.IsValid() || !mIBO.IsValid() || !mUBO.IsValid()) return;
            if (mInFrame) End();

            mCmd = cmd;
            mCamera = camera;
            mInFrame = true;
            mVertexBuffer.Clear();
            mIndexBuffer.Clear();
            mBatches.Clear();
            mCurrentBatch = nullptr;
            mStats = {};

            const NkMat4f vp = GetViewProjection();
            mDevice->WriteBuffer(mUBO, &vp, sizeof(vp), 0);

            NkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)mWidth;
            viewport.height = (float)mHeight;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            viewport.flipY = true;
            mCmd->SetViewport(viewport);

            mScissorDirty = true;
            ApplyClipScissor();
        }

        void NkRenderer2D::End() {
            if (!mInFrame) return;
            FlushBatch();
            mCmd = nullptr;
            mInFrame = false;
            mCurrentBatch = nullptr;
        }

        void NkRenderer2D::PushClipRect(float x, float y, float w, float h) {
            if (mInFrame && !mVertexBuffer.Empty()) FlushBatch();
            mClipStack.PushBack({x, y, w, h});
            mScissorDirty = true;
        }

        void NkRenderer2D::PopClipRect() {
            if (mInFrame && !mVertexBuffer.Empty()) FlushBatch();
            if (!mClipStack.Empty()) {
                mClipStack.PopBack();
                mScissorDirty = true;
            }
        }

        void NkRenderer2D::ClearClipRect() {
            if (mInFrame && !mVertexBuffer.Empty()) FlushBatch();
            mClipStack.Clear();
            mScissorDirty = true;
        }

        void NkRenderer2D::DrawSprite(const NkSprite& sprite) {
            if (!mInFrame || !sprite.visible) return;

            NkTexture2D* resolvedTex = (sprite.texture && sprite.texture->IsValid()) ? sprite.texture : mWhiteTex;
            if (!resolvedTex || !resolvedTex->IsValid()) return;

            const NkTextureHandle texHandle = resolvedTex->GetHandle();
            const NkSamplerHandle sampler = resolvedTex->GetSampler();
            if (!texHandle.IsValid() || !sampler.IsValid()) return;

            if (resolvedTex != mWhiteTex) {
                GetOrCreateTexDescSet(resolvedTex);
            }

            NkVec4f uvRect = sprite.uvRect;
            if (sprite.atlasRegion) {
                const NkAtlasRegion* r = sprite.atlasRegion;
                uvRect = {r->uvMin.x, r->uvMin.y, r->uvMax.x - r->uvMin.x, r->uvMax.y - r->uvMin.y};
            }

            float u0 = uvRect.x;
            float v0 = uvRect.y;
            float u1 = uvRect.x + uvRect.z;
            float v1 = uvRect.y + uvRect.w;
            if (sprite.flipX) { const float t = u0; u0 = u1; u1 = t; }
            if (sprite.flipY) { const float t = v0; v0 = v1; v1 = t; }

            const float w = NkMax(0.0f, sprite.size.x * sprite.scale.x);
            const float h = NkMax(0.0f, sprite.size.y * sprite.scale.y);
            if (w <= 0.0f || h <= 0.0f) return;

            const NkVec2f pivot = {sprite.origin.x * w, sprite.origin.y * h};
            NkVec2f p0 = V2(-pivot.x, -pivot.y);
            NkVec2f p1 = V2(w - pivot.x, -pivot.y);
            NkVec2f p2 = V2(w - pivot.x, h - pivot.y);
            NkVec2f p3 = V2(-pivot.x, h - pivot.y);
            p0 = V2Add(RotatePoint(p0, sprite.rotation), sprite.position);
            p1 = V2Add(RotatePoint(p1, sprite.rotation), sprite.position);
            p2 = V2Add(RotatePoint(p2, sprite.rotation), sprite.position);
            p3 = V2Add(RotatePoint(p3, sprite.rotation), sprite.position);

            EnsureBatch(texHandle, sampler, sprite.blendMode, sprite.depth);
            PushQuadVerts(
                p0, p1, p2, p3,
                {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1},
                sprite.color, sprite.color, sprite.color, sprite.color
            );
        }

        void NkRenderer2D::DrawTexture(NkTexture2D* tex, float x, float y, float w, float h,
                                       const NkColor4f& tint, float rotation, float depth) {
            NkSprite s{};
            s.texture = tex;
            s.position = {x, y};
            s.size = {
                w > 0.0f ? w : (tex ? (float)tex->GetWidth() : 1.0f),
                h > 0.0f ? h : (tex ? (float)tex->GetHeight() : 1.0f)
            };
            s.rotation = rotation;
            s.color = tint;
            s.depth = depth;
            s.visible = true;
            DrawSprite(s);
        }

        void NkRenderer2D::DrawSubTexture(NkTexture2D* tex,
                                          float x, float y, float w, float h,
                                          float srcX, float srcY, float srcW, float srcH,
                                          const NkColor4f& tint, float rotation) {
            if (!tex || !tex->IsValid()) return;
            const float tw = NkMax(1.0f, (float)tex->GetWidth());
            const float th = NkMax(1.0f, (float)tex->GetHeight());

            if (srcW <= 0.0f) srcW = tw;
            if (srcH <= 0.0f) srcH = th;

            NkSprite s{};
            s.texture = tex;
            s.position = {x, y};
            s.size = {w > 0.0f ? w : srcW, h > 0.0f ? h : srcH};
            s.rotation = rotation;
            s.color = tint;
            s.uvRect = {srcX / tw, srcY / th, srcW / tw, srcH / th};
            DrawSprite(s);
        }

        void NkRenderer2D::DrawAtlasRegion(NkTextureAtlas* atlas, const NkString& regionName,
                                           float x, float y, float w, float h,
                                           const NkColor4f& tint, float rotation) {
            if (!atlas) return;
            const NkAtlasRegion* r = atlas->FindRegion(regionName);
            NkTexture2D* tex = atlas->GetTexture();
            if (!r || !tex) return;

            DrawSubTexture(
                tex,
                x, y,
                w > 0.0f ? w : (float)r->pixW,
                h > 0.0f ? h : (float)r->pixH,
                (float)r->pixX, (float)r->pixY, (float)r->pixW, (float)r->pixH,
                tint, rotation
            );
        }

        void NkRenderer2D::DrawLine(float x0, float y0, float x1, float y1,
                                    const NkColor4f& color, float thickness, float depth) {
            if (!mInFrame) return;

            const NkVec2f a = {x0, y0};
            const NkVec2f b = {x1, y1};
            const NkVec2f d = V2Sub(b, a);
            const float len = V2Len(d);
            if (len <= 1e-6f) return;

            const NkVec2f n = V2Mul(V2Perp(V2Norm(d)), NkMax(0.5f, thickness * 0.5f));
            const NkVec2f p0 = V2Add(a, n);
            const NkVec2f p1 = V2Sub(a, n);
            const NkVec2f p2 = V2Sub(b, n);
            const NkVec2f p3 = V2Add(b, n);

            EnsureWhiteBatch(NkBlendMode::NK_TRANSLUCENT, depth);
            PushQuadVerts(
                p0, p1, p2, p3,
                {0, 0}, {1, 0}, {1, 1}, {0, 1},
                color, color, color, color
            );
        }

        void NkRenderer2D::DrawLine(NkVec2f a, NkVec2f b, const NkColor4f& color, float thickness) {
            DrawLine(a.x, a.y, b.x, b.y, color, thickness, 0.0f);
        }

        void NkRenderer2D::DrawPolyline(const NkVec2f* points, uint32 count,
                                        const NkColor4f& color, float thickness, bool closed) {
            if (!points || count < 2) return;
            for (uint32 i = 0; i + 1 < count; ++i) {
                DrawLine(points[i], points[i + 1], color, thickness);
            }
            if (closed) DrawLine(points[count - 1], points[0], color, thickness);
        }

        void NkRenderer2D::DrawArrow(NkVec2f from, NkVec2f to, const NkColor4f& color,
                                     float thickness, float headSize) {
            DrawLine(from, to, color, thickness);
            const NkVec2f dir = V2Norm(V2Sub(to, from));
            const NkVec2f side = V2Perp(dir);
            const NkVec2f back = V2Sub(to, V2Mul(dir, NkMax(0.0f, headSize)));
            DrawLine(to, V2Add(back, V2Mul(side, headSize * 0.45f)), color, thickness);
            DrawLine(to, V2Sub(back, V2Mul(side, headSize * 0.45f)), color, thickness);
        }

        void NkRenderer2D::DrawRect(float x, float y, float w, float h, const NkShapeStyle& style,
                                    float rotation, NkVec2f origin) {
            if (w <= 0.0f || h <= 0.0f) return;

            const NkVec2f pivot = {origin.x * w, origin.y * h};
            NkVec2f p0 = V2(-pivot.x, -pivot.y);
            NkVec2f p1 = V2(w - pivot.x, -pivot.y);
            NkVec2f p2 = V2(w - pivot.x, h - pivot.y);
            NkVec2f p3 = V2(-pivot.x, h - pivot.y);
            const NkVec2f t = {x, y};
            p0 = V2Add(RotatePoint(p0, rotation), t);
            p1 = V2Add(RotatePoint(p1, rotation), t);
            p2 = V2Add(RotatePoint(p2, rotation), t);
            p3 = V2Add(RotatePoint(p3, rotation), t);

            if (style.filled) {
                EnsureWhiteBatch(style.blendMode, style.depth);
                PushQuadVerts(p0, p1, p2, p3, {0, 0}, {1, 0}, {1, 1}, {0, 1},
                              style.fillColor, style.fillColor, style.fillColor, style.fillColor);
            }

            if (style.stroked && style.strokeWidth > 0.0f) {
                DrawLine(p0.x, p0.y, p1.x, p1.y, style.strokeColor, style.strokeWidth, style.depth);
                DrawLine(p1.x, p1.y, p2.x, p2.y, style.strokeColor, style.strokeWidth, style.depth);
                DrawLine(p2.x, p2.y, p3.x, p3.y, style.strokeColor, style.strokeWidth, style.depth);
                DrawLine(p3.x, p3.y, p0.x, p0.y, style.strokeColor, style.strokeWidth, style.depth);
            }
        }

        void NkRenderer2D::DrawRect(NkVec2f pos, NkVec2f size, const NkColor4f& fill, float depth) {
            NkShapeStyle s{};
            s.fillColor = fill;
            s.depth = depth;
            s.filled = true;
            s.stroked = false;
            DrawRect(pos.x, pos.y, size.x, size.y, s, 0.0f, {0.0f, 0.0f});
        }

        void NkRenderer2D::DrawRoundedRect(float x, float y, float w, float h, float radius,
                                           const NkShapeStyle& style, uint32 segments) {
            const float r = NkClamp(radius, 0.0f, NkMin(w, h) * 0.5f);
            if (r <= 0.001f || segments < 2) {
                DrawRect(x, y, w, h, style, 0.0f, {0.0f, 0.0f});
                return;
            }

            NkVector<NkVec2f> pts;
            pts.Reserve(segments * 4 + 8);
            const float step = (float)NkPi * 0.5f / (float)segments;

            auto appendArc = [&](NkVec2f c, float start) {
                for (uint32 i = 0; i <= segments; ++i) {
                    const float a = start + step * i;
                    pts.PushBack({c.x + NkCos(a) * r, c.y + NkSin(a) * r});
                }
            };

            appendArc({x + w - r, y + h - r}, 0.0f);
            appendArc({x + r, y + h - r}, (float)NkPi * 0.5f);
            appendArc({x + r, y + r}, (float)NkPi);
            appendArc({x + w - r, y + r}, (float)NkPi * 1.5f);
            DrawPolygon(pts.Data(), (uint32)pts.Size(), style);
        }

        void NkRenderer2D::DrawCircle(float cx, float cy, float radius,
                                      const NkShapeStyle& style, uint32 segments) {
            if (!mInFrame || radius <= 0.0f) return;
            segments = NkMax<uint32>(3, segments);

            const float step = 2.0f * (float)NkPi / (float)segments;
            const NkVec2f center = {cx, cy};

            if (style.filled) {
                EnsureWhiteBatch(style.blendMode, style.depth);
                NkVec2f p0 = {cx + radius, cy};
                for (uint32 i = 1; i <= segments; ++i) {
                    const float a = step * i;
                    NkVec2f p1 = {cx + NkCos(a) * radius, cy + NkSin(a) * radius};
                    PushTriVerts(center, p0, p1,
                                 {0.5f, 0.5f}, {1.0f, 0.5f}, {0.5f, 1.0f},
                                 style.fillColor, style.fillColor, style.fillColor);
                    p0 = p1;
                }
            }

            if (style.stroked && style.strokeWidth > 0.0f) {
                NkVec2f p0 = {cx + radius, cy};
                for (uint32 i = 1; i <= segments; ++i) {
                    const float a = step * i;
                    NkVec2f p1 = {cx + NkCos(a) * radius, cy + NkSin(a) * radius};
                    DrawLine(p0.x, p0.y, p1.x, p1.y, style.strokeColor, style.strokeWidth, style.depth);
                    p0 = p1;
                }
            }
        }

        void NkRenderer2D::DrawCircle(NkVec2f center, float radius,
                                      const NkColor4f& fill,
                                      const NkColor4f& stroke,
                                      float strokeWidth) {
            NkShapeStyle s{};
            s.fillColor = fill;
            s.strokeColor = stroke;
            s.strokeWidth = NkMax(0.0f, strokeWidth);
            s.filled = true;
            s.stroked = strokeWidth > 0.0f;
            DrawCircle(center.x, center.y, radius, s, 32);
        }

        void NkRenderer2D::DrawDisc(float cx, float cy, float radius,
                                    const NkColor4f& color, uint32 segments) {
            NkShapeStyle s{};
            s.fillColor = color;
            s.filled = true;
            s.stroked = false;
            DrawCircle(cx, cy, radius, s, segments);
        }

        void NkRenderer2D::DrawArc(float cx, float cy, float radius,
                                   float startAngleDeg, float endAngleDeg,
                                   const NkColor4f& color, float thickness,
                                   uint32 segments) {
            if (radius <= 0.0f || segments == 0) return;
            float startDeg = startAngleDeg;
            float endDeg = endAngleDeg;
            if (endDeg < startDeg) {
                const float t = startDeg;
                startDeg = endDeg;
                endDeg = t;
            }
            const float a0 = NkToRadians(startDeg);
            const float a1 = NkToRadians(endDeg);
            const float step = (a1 - a0) / (float)segments;

            NkVec2f p0 = {cx + NkCos(a0) * radius, cy + NkSin(a0) * radius};
            for (uint32 i = 1; i <= segments; ++i) {
                const float a = a0 + step * i;
                NkVec2f p1 = {cx + NkCos(a) * radius, cy + NkSin(a) * radius};
                DrawLine(p0, p1, color, thickness);
                p0 = p1;
            }
        }

        void NkRenderer2D::DrawEllipse(float cx, float cy, float rx, float ry,
                                       const NkShapeStyle& style, uint32 segments) {
            if (!mInFrame || rx <= 0.0f || ry <= 0.0f) return;
            segments = NkMax<uint32>(3, segments);

            const float step = 2.0f * (float)NkPi / (float)segments;
            const NkVec2f center = {cx, cy};

            if (style.filled) {
                EnsureWhiteBatch(style.blendMode, style.depth);
                NkVec2f p0 = {cx + rx, cy};
                for (uint32 i = 1; i <= segments; ++i) {
                    const float a = step * i;
                    NkVec2f p1 = {cx + NkCos(a) * rx, cy + NkSin(a) * ry};
                    PushTriVerts(center, p0, p1,
                                 {0.5f, 0.5f}, {1.0f, 0.5f}, {0.5f, 1.0f},
                                 style.fillColor, style.fillColor, style.fillColor);
                    p0 = p1;
                }
            }

            if (style.stroked && style.strokeWidth > 0.0f) {
                NkVec2f p0 = {cx + rx, cy};
                for (uint32 i = 1; i <= segments; ++i) {
                    const float a = step * i;
                    NkVec2f p1 = {cx + NkCos(a) * rx, cy + NkSin(a) * ry};
                    DrawLine(p0.x, p0.y, p1.x, p1.y, style.strokeColor, style.strokeWidth, style.depth);
                    p0 = p1;
                }
            }
        }

        void NkRenderer2D::DrawTriangle(NkVec2f a, NkVec2f b, NkVec2f c,
                                        const NkShapeStyle& style) {
            if (!mInFrame) return;

            if (style.filled) {
                EnsureWhiteBatch(style.blendMode, style.depth);
                PushTriVerts(a, b, c,
                             {0, 0}, {1, 0}, {0, 1},
                             style.fillColor, style.fillColor, style.fillColor);
            }

            if (style.stroked && style.strokeWidth > 0.0f) {
                DrawLine(a.x, a.y, b.x, b.y, style.strokeColor, style.strokeWidth, style.depth);
                DrawLine(b.x, b.y, c.x, c.y, style.strokeColor, style.strokeWidth, style.depth);
                DrawLine(c.x, c.y, a.x, a.y, style.strokeColor, style.strokeWidth, style.depth);
            }
        }

        void NkRenderer2D::DrawTriangleFilled(NkVec2f a, NkVec2f b, NkVec2f c,
                                              const NkColor4f& colorA, const NkColor4f& colorB,
                                              const NkColor4f& colorC) {
            if (!mInFrame) return;
            EnsureWhiteBatch(NkBlendMode::NK_TRANSLUCENT, 0.0f);
            PushTriVerts(a, b, c,
                         {0, 0}, {1, 0}, {0, 1},
                         colorA, colorB, colorC);
        }

        void NkRenderer2D::DrawPolygon(const NkVec2f* points, uint32 count,
                                       const NkShapeStyle& style) {
            if (!points || count < 3 || !mInFrame) return;

            if (style.filled) {
                NkVector<uint32> tri;
                if (TriangulatePolygon(points, count, tri) && !tri.Empty()) {
                    EnsureWhiteBatch(style.blendMode, style.depth);
                    for (usize i = 0; i + 2 < tri.Size(); i += 3) {
                        const NkVec2f& a = points[tri[i + 0]];
                        const NkVec2f& b = points[tri[i + 1]];
                        const NkVec2f& c = points[tri[i + 2]];
                        PushTriVerts(a, b, c,
                                     {0, 0}, {1, 0}, {0, 1},
                                     style.fillColor, style.fillColor, style.fillColor);
                    }
                } else {
                    EnsureWhiteBatch(style.blendMode, style.depth);
                    for (uint32 i = 1; i + 1 < count; ++i) {
                        PushTriVerts(points[0], points[i], points[i + 1],
                                     {0, 0}, {1, 0}, {0, 1},
                                     style.fillColor, style.fillColor, style.fillColor);
                    }
                }
            }

            if (style.stroked && style.strokeWidth > 0.0f) {
                for (uint32 i = 0; i + 1 < count; ++i) {
                    DrawLine(points[i].x, points[i].y,
                             points[i + 1].x, points[i + 1].y,
                             style.strokeColor, style.strokeWidth, style.depth);
                }
                DrawLine(points[count - 1].x, points[count - 1].y,
                         points[0].x, points[0].y,
                         style.strokeColor, style.strokeWidth, style.depth);
            }
        }

        void NkRenderer2D::DrawPolygonFilled(const NkVec2f* points, uint32 count,
                                             const NkColor4f& color) {
            if (!points || count < 3 || !mInFrame) return;

            NkVector<uint32> tri;
            if (TriangulatePolygon(points, count, tri) && !tri.Empty()) {
                EnsureWhiteBatch(NkBlendMode::NK_TRANSLUCENT, 0.0f);
                for (usize i = 0; i + 2 < tri.Size(); i += 3) {
                    const NkVec2f& a = points[tri[i + 0]];
                    const NkVec2f& b = points[tri[i + 1]];
                    const NkVec2f& c = points[tri[i + 2]];
                    PushTriVerts(a, b, c, {0, 0}, {1, 0}, {0, 1}, color, color, color);
                }
                return;
            }

            EnsureWhiteBatch(NkBlendMode::NK_TRANSLUCENT, 0.0f);
            for (uint32 i = 1; i + 1 < count; ++i) {
                PushTriVerts(points[0], points[i], points[i + 1],
                             {0, 0}, {1, 0}, {0, 1},
                             color, color, color);
            }
        }

        void NkRenderer2D::DrawQuad(NkVec2f a, NkVec2f b, NkVec2f c, NkVec2f d,
                                    const NkColor4f& color, NkTexture2D* tex) {
            if (!mInFrame) return;

            if (tex && tex->IsValid()) {
                GetOrCreateTexDescSet(tex);
                EnsureBatch(tex->GetHandle(), tex->GetSampler(), NkBlendMode::NK_TRANSLUCENT, 0.0f);
            } else {
                EnsureWhiteBatch(NkBlendMode::NK_TRANSLUCENT, 0.0f);
            }

            PushQuadVerts(a, b, c, d,
                          {0, 0}, {1, 0}, {1, 1}, {0, 1},
                          color, color, color, color);
        }

        void NkRenderer2D::DrawBezier(NkVec2f p0, NkVec2f p1, NkVec2f p2, NkVec2f p3,
                                      const NkColor4f& color, float thickness, uint32 segments) {
            segments = NkMax<uint32>(1, segments);
            NkVec2f prev = p0;
            for (uint32 i = 1; i <= segments; ++i) {
                const float t = (float)i / (float)segments;
                const float u = 1.0f - t;
                NkVec2f p = {
                    p0.x * u * u * u + 3.0f * p1.x * u * u * t + 3.0f * p2.x * u * t * t + p3.x * t * t * t,
                    p0.y * u * u * u + 3.0f * p1.y * u * u * t + 3.0f * p2.y * u * t * t + p3.y * t * t * t
                };
                DrawLine(prev, p, color, thickness);
                prev = p;
            }
        }

        void NkRenderer2D::DrawPie(float cx, float cy, float radius,
                                   float startAngleDeg, float endAngleDeg,
                                   const NkColor4f& color, uint32 segments) {
            if (!mInFrame || radius <= 0.0f || segments == 0) return;

            float startDeg = startAngleDeg;
            float endDeg = endAngleDeg;
            if (endDeg < startDeg) {
                const float t = startDeg;
                startDeg = endDeg;
                endDeg = t;
            }

            const float a0 = NkToRadians(startDeg);
            const float a1 = NkToRadians(endDeg);
            const float step = (a1 - a0) / (float)segments;
            const NkVec2f c = {cx, cy};

            EnsureWhiteBatch(NkBlendMode::NK_TRANSLUCENT, 0.0f);
            NkVec2f p0 = {cx + NkCos(a0) * radius, cy + NkSin(a0) * radius};
            for (uint32 i = 1; i <= segments; ++i) {
                const float a = a0 + step * i;
                NkVec2f p1 = {cx + NkCos(a) * radius, cy + NkSin(a) * radius};
                PushTriVerts(c, p0, p1,
                             {0.5f, 0.5f}, {1.0f, 0.5f}, {0.5f, 1.0f},
                             color, color, color);
                p0 = p1;
            }
        }

        void NkRenderer2D::DrawGradientRect(float x, float y, float w, float h,
                                            NkColor4f top, NkColor4f bottom, float depth) {
            if (!mInFrame || w <= 0.0f || h <= 0.0f) return;
            EnsureWhiteBatch(NkBlendMode::NK_TRANSLUCENT, depth);
            const NkVec2f p0 = {x, y};
            const NkVec2f p1 = {x + w, y};
            const NkVec2f p2 = {x + w, y + h};
            const NkVec2f p3 = {x, y + h};
            PushQuadVerts(p0, p1, p2, p3, {0, 0}, {1, 0}, {1, 1}, {0, 1}, top, top, bottom, bottom);
        }

        void NkRenderer2D::DrawGradientRectH(float x, float y, float w, float h,
                                             NkColor4f left, NkColor4f right, float depth) {
            if (!mInFrame || w <= 0.0f || h <= 0.0f) return;
            EnsureWhiteBatch(NkBlendMode::NK_TRANSLUCENT, depth);
            const NkVec2f p0 = {x, y};
            const NkVec2f p1 = {x + w, y};
            const NkVec2f p2 = {x + w, y + h};
            const NkVec2f p3 = {x, y + h};
            PushQuadVerts(p0, p1, p2, p3, {0, 0}, {1, 0}, {1, 1}, {0, 1}, left, right, right, left);
        }

        void NkRenderer2D::DrawPoint(float x, float y, const NkColor4f& color, float size) {
            if (size <= 0.0f) return;
            const float h = size * 0.5f;
            NkShapeStyle s{};
            s.fillColor = color;
            s.filled = true;
            s.stroked = false;
            DrawRect(x - h, y - h, size, size, s, 0.0f, {0.0f, 0.0f});
        }

        bool NkRenderer2D::InitShaders() {
            if (!mDevice) return false;
            const bool useVulkanGLSL = (mDevice->GetApi() == NkGraphicsApi::NK_GFX_API_VULKAN);
            const char* spriteVs = useVulkanGLSL ? NkBuiltinShaders::SpriteVertVK() : NkBuiltinShaders::SpriteVert();
            const char* spritePs = useVulkanGLSL ? NkBuiltinShaders::SpriteFragVK() : NkBuiltinShaders::SpriteFrag();
            const char* shapeVs  = useVulkanGLSL ? NkBuiltinShaders::Shape2DVertVK() : NkBuiltinShaders::Shape2DVert();
            const char* shapePs  = useVulkanGLSL ? NkBuiltinShaders::Shape2DFragVK() : NkBuiltinShaders::Shape2DFrag();

            NkShaderDesc sprite{};
            sprite.debugName = "NkRenderer2D.Sprite";
            sprite.AddGLSL(NkShaderStage::NK_VERTEX, spriteVs, "main");
            sprite.AddGLSL(NkShaderStage::NK_FRAGMENT, spritePs, "main");
            sprite.AddHLSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::SpriteVertHLSL(), "VSMain");
            sprite.AddHLSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::SpriteFragHLSL(), "PSMain");
            sprite.AddMSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::SpriteVertMSL(), "VSMain");
            sprite.AddMSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::SpriteFragMSL(), "PSMain");
            mSpriteShader = mDevice->CreateShader(sprite);
            if (!mSpriteShader.IsValid()) return false;

            NkShaderDesc shape{};
            shape.debugName = "NkRenderer2D.Shape";
            shape.AddGLSL(NkShaderStage::NK_VERTEX, shapeVs, "main");
            shape.AddGLSL(NkShaderStage::NK_FRAGMENT, shapePs, "main");
            shape.AddHLSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::Shape2DVertHLSL(), "VSMain");
            shape.AddHLSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::Shape2DFragHLSL(), "PSMain");
            shape.AddMSL(NkShaderStage::NK_VERTEX, NkBuiltinShaders::Shape2DVertMSL(), "VSMain");
            shape.AddMSL(NkShaderStage::NK_FRAGMENT, NkBuiltinShaders::Shape2DFragMSL(), "PSMain");
            mShapeShader = mDevice->CreateShader(shape);
            return mShapeShader.IsValid();
        }

        bool NkRenderer2D::InitPipelines() {
            if (!mDevice || !mRenderPass.IsValid() || !mLayout.IsValid()) return false;

            NkVertexLayout vtx{};
            vtx.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT, 0, "POSITION", 0);
            vtx.AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 8, "TEXCOORD", 0);
            vtx.AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 16, "COLOR", 0);
            vtx.AddBinding(0, sizeof(NkBatchVert));

            auto makePipe = [&](NkShaderHandle shader, NkBlendMode blend, const char* name) -> NkPipelineHandle {
                NkGraphicsPipelineDesc d{};
                d.shader = shader;
                d.vertexLayout = vtx;
                d.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
                d.rasterizer = NkRasterizerDesc::NoCull();
                d.rasterizer.scissorTest = true;
                d.depthStencil = NkDepthStencilDesc::NoDepth();
                d.blend = BlendFromMode(blend);
                d.renderPass = mRenderPass;
                d.descriptorSetLayouts.PushBack(mLayout);
                d.debugName = name;
                return mDevice->CreateGraphicsPipeline(d);
            };

            mSpritePipeline = makePipe(mSpriteShader, NkBlendMode::NK_TRANSLUCENT, "NkRenderer2D.Sprite");
            mShapeOpaquePipeline = makePipe(mShapeShader, NkBlendMode::NK_OPAQUE, "NkRenderer2D.ShapeOpaque");
            mShapeAlphaPipeline = makePipe(mShapeShader, NkBlendMode::NK_TRANSLUCENT, "NkRenderer2D.ShapeAlpha");
            mShapeAdditivePipeline = makePipe(mShapeShader, NkBlendMode::NK_ADDITIVE, "NkRenderer2D.ShapeAdd");

            return mSpritePipeline.IsValid()
                && mShapeOpaquePipeline.IsValid()
                && mShapeAlphaPipeline.IsValid()
                && mShapeAdditivePipeline.IsValid();
        }

        bool NkRenderer2D::InitBuffers() {
            if (!mDevice) return false;

            mVBO = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)MAX_VERTICES * sizeof(NkBatchVert)));
            mIBO = mDevice->CreateBuffer(NkBufferDesc::IndexDynamic((uint64)MAX_INDICES * sizeof(uint32)));
            mUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkMat4f)));
            if (!mVBO.IsValid() || !mIBO.IsValid() || !mUBO.IsValid()) {
                return false;
            }

            mVertexBuffer.Reserve(MAX_VERTICES);
            mIndexBuffer.Reserve(MAX_INDICES);
            mBatches.Reserve(1024);
            return true;
        }

        bool NkRenderer2D::InitDescriptors() {
            if (!mDevice || !mUBO.IsValid()) return false;

            NkDescriptorSetLayoutDesc layout{};
            layout.debugName = "NkRenderer2D.Layout";
            layout.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX);
            layout.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);

            mLayout = mDevice->CreateDescriptorSetLayout(layout);
            if (!mLayout.IsValid()) return false;

            mDescSet = mDevice->AllocateDescriptorSet(mLayout);
            if (!mDescSet.IsValid()) return false;

            NkTextureHandle whiteTex{};
            NkSamplerHandle whiteSamp{};
            if (mWhiteTex && mWhiteTex->IsValid()) {
                whiteTex = mWhiteTex->GetHandle();
                whiteSamp = mWhiteTex->GetSampler();
            }
            if (!whiteTex.IsValid() || !whiteSamp.IsValid()) return false;

            NkDescriptorWrite writes[2]{};
            writes[0].set = mDescSet;
            writes[0].binding = 0;
            writes[0].type = NkDescriptorType::NK_UNIFORM_BUFFER;
            writes[0].buffer = mUBO;
            writes[0].bufferRange = sizeof(NkMat4f);

            writes[1].set = mDescSet;
            writes[1].binding = 1;
            writes[1].type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            writes[1].texture = whiteTex;
            writes[1].sampler = whiteSamp;
            mDevice->UpdateDescriptorSets(writes, 2);

            mTexDescSets.Clear();
            mTexDescSets[MakeTexKey(whiteTex, whiteSamp)] = mDescSet;
            return true;
        }

        bool NkRenderer2D::InitBuiltinTextures() {
            if (!mDevice) return false;

            mWhiteTex = new NkTexture2D();
            if (!mWhiteTex || !mWhiteTex->CreateWhite(mDevice)) {
                if (mWhiteTex) {
                    delete mWhiteTex;
                    mWhiteTex = nullptr;
                }
                return false;
            }

            mCircleTex = new NkTexture2D();
            if (!mCircleTex) {
                return true;
            }

            constexpr uint32 kSize = 64;
            NkVector<uint8> pixels;
            pixels.Resize((usize)kSize * (usize)kSize * 4u);

            for (uint32 y = 0; y < kSize; ++y) {
                for (uint32 x = 0; x < kSize; ++x) {
                    const float fx = ((float)x + 0.5f) / (float)kSize * 2.0f - 1.0f;
                    const float fy = ((float)y + 0.5f) / (float)kSize * 2.0f - 1.0f;
                    const float d = NkSqrt(fx * fx + fy * fy);
                    float a = 1.0f - NkClamp((d - 0.90f) / 0.10f, 0.0f, 1.0f);
                    const usize i = ((usize)y * (usize)kSize + (usize)x) * 4u;
                    pixels[i + 0] = 255;
                    pixels[i + 1] = 255;
                    pixels[i + 2] = 255;
                    pixels[i + 3] = (uint8)(NkClamp(a, 0.0f, 1.0f) * 255.0f);
                }
            }

            NkTextureParams params = NkTextureParams::UISprite();
            params.srgb = false;
            params.generateMips = false;
            params.wrapU = NkAddressMode::NK_CLAMP_TO_EDGE;
            params.wrapV = NkAddressMode::NK_CLAMP_TO_EDGE;
            if (!mCircleTex->LoadFromMemory(mDevice, pixels.Data(), kSize, kSize, NkGPUFormat::NK_RGBA8_UNORM, params)) {
                delete mCircleTex;
                mCircleTex = nullptr;
            }

            return true;
        }

        void NkRenderer2D::FlushBatch() {
            if (!mInFrame || !mCmd || mVertexBuffer.Empty() || mIndexBuffer.Empty() || mBatches.Empty()) {
                mVertexBuffer.Clear();
                mIndexBuffer.Clear();
                mBatches.Clear();
                mCurrentBatch = nullptr;
                return;
            }

            const uint64 vBytes = (uint64)mVertexBuffer.Size() * sizeof(NkBatchVert);
            const uint64 iBytes = (uint64)mIndexBuffer.Size() * sizeof(uint32);
            if (!mDevice->WriteBuffer(mVBO, mVertexBuffer.Data(), vBytes, 0)) {
                mVertexBuffer.Clear();
                mIndexBuffer.Clear();
                mBatches.Clear();
                mCurrentBatch = nullptr;
                return;
            }
            if (!mDevice->WriteBuffer(mIBO, mIndexBuffer.Data(), iBytes, 0)) {
                mVertexBuffer.Clear();
                mIndexBuffer.Clear();
                mBatches.Clear();
                mCurrentBatch = nullptr;
                return;
            }

            NkViewport vp{};
            vp.x = 0.0f;
            vp.y = 0.0f;
            vp.width = (float)mWidth;
            vp.height = (float)mHeight;
            vp.minDepth = 0.0f;
            vp.maxDepth = 1.0f;
            vp.flipY = true;
            mCmd->SetViewport(vp);

            ApplyClipScissor();
            mCmd->BindVertexBuffer(0, mVBO, 0);
            mCmd->BindIndexBuffer(mIBO, NkIndexFormat::NK_UINT32, 0);

            NkPipelineHandle boundPipe{};
            NkDescSetHandle boundSet{};
            const NkTextureHandle whiteHandle = (mWhiteTex && mWhiteTex->IsValid()) ? mWhiteTex->GetHandle() : NkTextureHandle{};

            for (const auto& b : mBatches) {
                if (b.indexCount == 0) continue;

                const bool isWhite = whiteHandle.IsValid() && (b.textureHandle == whiteHandle);
                NkPipelineHandle pipe{};
                if (isWhite) {
                    switch (b.blendMode) {
                        case NkBlendMode::NK_OPAQUE:   pipe = mShapeOpaquePipeline; break;
                        case NkBlendMode::NK_ADDITIVE: pipe = mShapeAdditivePipeline; break;
                        default:                       pipe = mShapeAlphaPipeline; break;
                    }
                } else {
                    if (b.blendMode == NkBlendMode::NK_OPAQUE && mShapeOpaquePipeline.IsValid()) {
                        pipe = mShapeOpaquePipeline;
                    } else if (b.blendMode == NkBlendMode::NK_ADDITIVE && mShapeAdditivePipeline.IsValid()) {
                        pipe = mShapeAdditivePipeline;
                    } else {
                        pipe = mSpritePipeline.IsValid() ? mSpritePipeline : mShapeAlphaPipeline;
                    }
                }

                if (!pipe.IsValid()) continue;
                if (pipe != boundPipe) {
                    mCmd->BindGraphicsPipeline(pipe);
                    boundPipe = pipe;
                    ++mStats.pipelineChanges;
                }

                NkDescSetHandle set = mDescSet;
                if (!isWhite) {
                    const uint64 key = MakeTexKey(b.textureHandle, b.samplerHandle);
                    const NkDescSetHandle* found = mTexDescSets.Find(key);
                    if (found && found->IsValid()) {
                        set = *found;
                    }
                }

                if (set.IsValid() && (set != boundSet)) {
                    mCmd->BindDescriptorSet(set, 0);
                    boundSet = set;
                    ++mStats.textureBinds;
                }

                mCmd->DrawIndexed(b.indexCount, 1, b.firstIndex, 0, 0);
                ++mStats.drawCalls;
                mStats.triangles += b.indexCount / 3;
                mStats.vertices += b.vertexCount;
            }

            mVertexBuffer.Clear();
            mIndexBuffer.Clear();
            mBatches.Clear();
            mCurrentBatch = nullptr;
        }

        void NkRenderer2D::StartNewBatch(NkTextureHandle tex, NkSamplerHandle samp, NkBlendMode blend, float depth) {
            NkBatch2D b{};
            b.textureHandle = tex;
            b.samplerHandle = samp;
            b.blendMode = blend;
            b.firstVertex = (uint32)mVertexBuffer.Size();
            b.vertexCount = 0;
            b.firstIndex = (uint32)mIndexBuffer.Size();
            b.indexCount = 0;
            b.minDepth = depth;
            b.maxDepth = depth;
            mBatches.PushBack(b);
            mCurrentBatch = &mBatches.Back();
        }

        void NkRenderer2D::EnsureBatch(NkTextureHandle tex, NkSamplerHandle samp, NkBlendMode blend, float depth) {
            if (!mInFrame) return;

            if (!tex.IsValid() || !samp.IsValid()) {
                if (mWhiteTex && mWhiteTex->IsValid()) {
                    tex = mWhiteTex->GetHandle();
                    samp = mWhiteTex->GetSampler();
                } else {
                    return;
                }
            }

            const bool isWhite = (mWhiteTex && mWhiteTex->IsValid() && tex == mWhiteTex->GetHandle());
            if (!isWhite && mDevice && mLayout.IsValid()) {
                const uint64 key = MakeTexKey(tex, samp);
                if (!mTexDescSets.Contains(key)) {
                    NkDescSetHandle set = mDevice->AllocateDescriptorSet(mLayout);
                    if (set.IsValid()) {
                        NkDescriptorWrite writes[2]{};
                        writes[0].set = set;
                        writes[0].binding = 0;
                        writes[0].type = NkDescriptorType::NK_UNIFORM_BUFFER;
                        writes[0].buffer = mUBO;
                        writes[0].bufferRange = sizeof(NkMat4f);
                        writes[1].set = set;
                        writes[1].binding = 1;
                        writes[1].type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                        writes[1].texture = tex;
                        writes[1].sampler = samp;
                        mDevice->UpdateDescriptorSets(writes, 2);
                        mTexDescSets[key] = set;
                    }
                }
            }

            if (!mCurrentBatch) {
                StartNewBatch(tex, samp, blend, depth);
                return;
            }

            if (mCurrentBatch->textureHandle != tex
             || mCurrentBatch->samplerHandle != samp
             || mCurrentBatch->blendMode != blend) {
                StartNewBatch(tex, samp, blend, depth);
                return;
            }

            mCurrentBatch->minDepth = NkMin(mCurrentBatch->minDepth, depth);
            mCurrentBatch->maxDepth = NkMax(mCurrentBatch->maxDepth, depth);
        }

        void NkRenderer2D::EnsureWhiteBatch(NkBlendMode blend, float depth) {
            if (!mWhiteTex || !mWhiteTex->IsValid()) return;
            EnsureBatch(mWhiteTex->GetHandle(), mWhiteTex->GetSampler(), blend, depth);
        }

        void NkRenderer2D::PushQuadVerts(NkVec2f p0, NkVec2f p1, NkVec2f p2, NkVec2f p3,
                                         NkVec2f uv0, NkVec2f uv1, NkVec2f uv2, NkVec2f uv3,
                                         NkColor4f c0, NkColor4f c1, NkColor4f c2, NkColor4f c3) {
            if (!mInFrame || !mCurrentBatch) return;

            if (mVertexBuffer.Size() + 4 > MAX_VERTICES || mIndexBuffer.Size() + 6 > MAX_INDICES) {
                const NkTextureHandle tex = mCurrentBatch->textureHandle;
                const NkSamplerHandle samp = mCurrentBatch->samplerHandle;
                const NkBlendMode blend = mCurrentBatch->blendMode;
                const float depth = mCurrentBatch->minDepth;
                FlushBatch();
                StartNewBatch(tex, samp, blend, depth);
            }

            const uint32 base = (uint32)mVertexBuffer.Size();
            mVertexBuffer.PushBack(MakeVert(p0, uv0, c0));
            mVertexBuffer.PushBack(MakeVert(p1, uv1, c1));
            mVertexBuffer.PushBack(MakeVert(p2, uv2, c2));
            mVertexBuffer.PushBack(MakeVert(p3, uv3, c3));

            mIndexBuffer.PushBack(base + 0);
            mIndexBuffer.PushBack(base + 1);
            mIndexBuffer.PushBack(base + 2);
            mIndexBuffer.PushBack(base + 2);
            mIndexBuffer.PushBack(base + 3);
            mIndexBuffer.PushBack(base + 0);

            mCurrentBatch->vertexCount += 4;
            mCurrentBatch->indexCount += 6;
        }

        void NkRenderer2D::PushTriVerts(NkVec2f a, NkVec2f b, NkVec2f c,
                                        NkVec2f ua, NkVec2f ub, NkVec2f uc,
                                        NkColor4f ca, NkColor4f cb, NkColor4f cc) {
            if (!mInFrame || !mCurrentBatch) return;

            if (mVertexBuffer.Size() + 3 > MAX_VERTICES || mIndexBuffer.Size() + 3 > MAX_INDICES) {
                const NkTextureHandle tex = mCurrentBatch->textureHandle;
                const NkSamplerHandle samp = mCurrentBatch->samplerHandle;
                const NkBlendMode blend = mCurrentBatch->blendMode;
                const float depth = mCurrentBatch->minDepth;
                FlushBatch();
                StartNewBatch(tex, samp, blend, depth);
            }

            const uint32 base = (uint32)mVertexBuffer.Size();
            mVertexBuffer.PushBack(MakeVert(a, ua, ca));
            mVertexBuffer.PushBack(MakeVert(b, ub, cb));
            mVertexBuffer.PushBack(MakeVert(c, uc, cc));

            mIndexBuffer.PushBack(base + 0);
            mIndexBuffer.PushBack(base + 1);
            mIndexBuffer.PushBack(base + 2);

            mCurrentBatch->vertexCount += 3;
            mCurrentBatch->indexCount += 3;
        }

        NkDescSetHandle NkRenderer2D::GetOrCreateTexDescSet(NkTexture2D* tex) {
            if (!tex || !tex->IsValid() || !mDevice || !mLayout.IsValid()) return mDescSet;

            const NkTextureHandle t = tex->GetHandle();
            const NkSamplerHandle s = tex->GetSampler();
            if (!t.IsValid() || !s.IsValid()) return mDescSet;

            if (mWhiteTex && mWhiteTex->IsValid() && t == mWhiteTex->GetHandle()) {
                return mDescSet;
            }

            const uint64 key = MakeTexKey(t, s);
            NkDescSetHandle* found = mTexDescSets.Find(key);
            if (found && found->IsValid()) return *found;

            NkDescSetHandle set = mDevice->AllocateDescriptorSet(mLayout);
            if (!set.IsValid()) return mDescSet;

            NkDescriptorWrite writes[2]{};
            writes[0].set = set;
            writes[0].binding = 0;
            writes[0].type = NkDescriptorType::NK_UNIFORM_BUFFER;
            writes[0].buffer = mUBO;
            writes[0].bufferRange = sizeof(NkMat4f);
            writes[1].set = set;
            writes[1].binding = 1;
            writes[1].type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            writes[1].texture = t;
            writes[1].sampler = s;
            mDevice->UpdateDescriptorSets(writes, 2);

            mTexDescSets[key] = set;
            return set;
        }

        NkMat4f NkRenderer2D::GetViewProjection() const {
            if (mWidth == 0 || mHeight == 0) {
                return NkMat4f::Identity();
            }
            return mCamera.GetViewProjection((float)mWidth, (float)mHeight);
        }

        void NkRenderer2D::ApplyClipScissor() {
            if (!mCmd) return;

            float x0 = 0.0f;
            float y0 = 0.0f;
            float x1 = (float)mWidth;
            float y1 = (float)mHeight;

            for (const auto& c : mClipStack) {
                const float cx0 = c.x;
                const float cy0 = c.y;
                const float cx1 = c.x + NkMax(0.0f, c.w);
                const float cy1 = c.y + NkMax(0.0f, c.h);
                x0 = NkMax(x0, cx0);
                y0 = NkMax(y0, cy0);
                x1 = NkMin(x1, cx1);
                y1 = NkMin(y1, cy1);
            }

            x0 = NkClamp(x0, 0.0f, (float)mWidth);
            y0 = NkClamp(y0, 0.0f, (float)mHeight);
            x1 = NkClamp(x1, 0.0f, (float)mWidth);
            y1 = NkClamp(y1, 0.0f, (float)mHeight);

            const int32 sx = (int32)x0;
            const int32 sy = (int32)y0;
            const int32 sw = (int32)NkMax(0.0f, x1 - x0);
            const int32 sh = (int32)NkMax(0.0f, y1 - y0);
            mCmd->SetScissor({sx, sy, sw, sh});
            mScissorDirty = false;
        }

        bool NkRenderer2D::TriangulatePolygon(const NkVec2f* pts, uint32 cnt, NkVector<uint32>& indices) {
            indices.Clear();
            if (!pts || cnt < 3) return false;

            NkVector<uint32> v;
            v.Reserve(cnt);
            const bool ccw = PolySignedArea(pts, cnt) >= 0.0f;
            if (ccw) {
                for (uint32 i = 0; i < cnt; ++i) v.PushBack(i);
            } else {
                for (uint32 i = 0; i < cnt; ++i) v.PushBack(cnt - 1 - i);
            }

            auto isEar = [&](usize i) -> bool {
                const usize n = v.Size();
                const uint32 i0 = v[(i + n - 1) % n];
                const uint32 i1 = v[i];
                const uint32 i2 = v[(i + 1) % n];
                const NkVec2f& a = pts[i0];
                const NkVec2f& b = pts[i1];
                const NkVec2f& c = pts[i2];
                if (Cross2(a, b, c) <= 1e-6f) return false;

                for (usize j = 0; j < n; ++j) {
                    const uint32 k = v[j];
                    if (k == i0 || k == i1 || k == i2) continue;
                    if (PointInTri(pts[k], a, b, c)) return false;
                }
                return true;
            };

            usize guard = 0;
            const usize maxGuard = (usize)cnt * (usize)cnt;
            while (v.Size() > 3 && guard < maxGuard) {
                bool clipped = false;
                for (usize i = 0; i < v.Size(); ++i) {
                    if (!isEar(i)) continue;

                    const usize n = v.Size();
                    const uint32 i0 = v[(i + n - 1) % n];
                    const uint32 i1 = v[i];
                    const uint32 i2 = v[(i + 1) % n];
                    indices.PushBack(i0);
                    indices.PushBack(i1);
                    indices.PushBack(i2);
                    v.Erase(v.Begin() + i);
                    clipped = true;
                    break;
                }

                if (!clipped) break;
                ++guard;
            }

            if (v.Size() == 3) {
                indices.PushBack(v[0]);
                indices.PushBack(v[1]);
                indices.PushBack(v[2]);
            }

            if (!indices.Empty()) return true;

            for (uint32 i = 1; i + 1 < cnt; ++i) {
                indices.PushBack(0);
                indices.PushBack(i);
                indices.PushBack(i + 1);
            }
            return !indices.Empty();
        }

    } // namespace renderer
} // namespace nkentseu
