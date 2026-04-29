// =============================================================================
// NkRender2D.cpp — Renderer 2D avec batching automatique
// =============================================================================
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include <cstring>
#include <cmath>

// ── Shaders GLSL intégrés (pour OpenGL 4.6+) ──────────────────────────────────
namespace {
    // Vertex shader 2D
    static const char* kVert2D_GL = R"(
#version 460 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
layout(std140, binding=0) uniform VP { mat4 uVP; };
out vec2 vUV;
out vec4 vColor;
void main() {
    vUV    = aUV;
    vColor = aColor;
    gl_Position = uVP * vec4(aPos, 0.0, 1.0);
}
)";

    // Fragment shader 2D
    static const char* kFrag2D_GL = R"(
#version 460 core
in vec2 vUV;
in vec4 vColor;
layout(binding=0) uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    vec4 c = vColor;
    if (vUV.x >= 0.0) c *= texture(uTex, vUV);
    FragColor = c;
}
)";

    static const char* kVert2D_VK = R"(
#version 450
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
layout(set=0, binding=0) uniform VP { mat4 uVP; };
layout(location=0) out vec2 vUV;
layout(location=1) out vec4 vColor;
void main() {
    vUV    = aUV;
    vColor = aColor;
    gl_Position = uVP * vec4(aPos, 0.0, 1.0);
}
)";

    static const char* kFrag2D_VK = R"(
#version 450
layout(location=0) in vec2 vUV;
layout(location=1) in vec4 vColor;
layout(set=0, binding=1) uniform sampler2D uTex;
layout(location=0) out vec4 FragColor;
void main() {
    vec4 c = vColor;
    if (vUV.x >= 0.0) c *= texture(uTex, vUV);
    FragColor = c;
}
)";
}

namespace nkentseu {
    namespace renderer {

        bool NkRender2D::Init(NkIDevice* device, NkResourceManager* resources,
                               NkRenderTargetHandle swapRP,
                               const NkRender2DConfig& cfg) {
            if (!device || !resources) return false;
            mDev       = device;
            mResources = resources;
            mCfg       = cfg;

            // Pré-allouer les buffers CPU
            mVerts.Reserve(cfg.maxVertices);
            mIdx.Reserve(cfg.maxIndices);

            // Créer VBO/IBO dynamiques
            NkBufferHandle vbo = device->CreateBuffer(
                NkBufferDesc::VertexDynamic((uint64)cfg.maxVertices * sizeof(NkVertex2D)));
            NkBufferHandle ibo = device->CreateBuffer(
                NkBufferDesc::IndexDynamic((uint64)cfg.maxIndices * sizeof(uint32)));
            if (!vbo.IsValid() || !ibo.IsValid()) return false;
            mVBORHI = vbo.id;
            mIBORHI = ibo.id;

            // UBO viewport
            NkBufferHandle ubo = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkMat4f)));
            if (!ubo.IsValid()) return false;
            mUBORHI = ubo.id;

            // Sampler par défaut
            NkSamplerHandle samp = device->CreateSampler(NkSamplerDesc::Clamp());
            mSamplerRHI = samp.id;

            // Shader
            NkShaderDesc sd{};
            sd.debugName = "Shader2D";
            switch (device->GetApi()) {
                case NkGraphicsApi::NK_API_OPENGL:
                case NkGraphicsApi::NK_API_OPENGLES:
                    sd.AddGLSL(NkShaderStage::NK_VERTEX,   kVert2D_GL);
                    sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kFrag2D_GL);
                    break;
                case NkGraphicsApi::NK_API_VULKAN:
                    sd.AddGLSL(NkShaderStage::NK_VERTEX,   kVert2D_VK);
                    sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kFrag2D_VK);
                    break;
                default:
                    sd.AddGLSL(NkShaderStage::NK_VERTEX,   kVert2D_GL);
                    sd.AddGLSL(NkShaderStage::NK_FRAGMENT, kFrag2D_GL);
                    break;
            }
            NkShaderHandle sh = device->CreateShader(sd);
            if (!sh.IsValid()) return false;
            mShaderRHI = sh.id;

            // Descriptor Set Layout
            NkDescriptorSetLayoutDesc dl{};
            dl.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER,          NkShaderStage::NK_VERTEX);
            dl.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,  NkShaderStage::NK_FRAGMENT);
            NkDescSetHandle layout = device->CreateDescriptorSetLayout(dl);
            if (!layout.IsValid()) return false;
            mDescLayoutRHI = layout.id;

            // Descriptor Set
            NkDescSetHandle ds = device->AllocateDescriptorSet(layout);
            if (!ds.IsValid()) return false;
            mDescSetRHI = ds.id;

            // Lier UBO
            NkDescriptorWrite w{};
            w.set = ds; w.binding = 0;
            w.type = NkDescriptorType::NK_UNIFORM_BUFFER;
            NkBufferHandle uh; uh.id = mUBORHI;
            w.buffer = uh; w.bufferRange = sizeof(NkMat4f);
            device->UpdateDescriptorSets(&w, 1);

            mReady = true;
            return true;
        }

        void NkRender2D::Shutdown() {
            if (!mDev) return;
            auto destroy = [&](uint64& id, auto destroyFn) {
                if (id) { destroyFn(id); id = 0; }
            };
            // Destroy all GPU resources via NKRHI
            if (mShaderRHI)     { NkShaderHandle h; h.id=mShaderRHI; mDev->DestroyShader(h); mShaderRHI=0; }
            if (mVBORHI)        { NkBufferHandle h; h.id=mVBORHI; mDev->DestroyBuffer(h); mVBORHI=0; }
            if (mIBORHI)        { NkBufferHandle h; h.id=mIBORHI; mDev->DestroyBuffer(h); mIBORHI=0; }
            if (mUBORHI)        { NkBufferHandle h; h.id=mUBORHI; mDev->DestroyBuffer(h); mUBORHI=0; }
            if (mPipelineRHI)   { NkPipelineHandle h; h.id=mPipelineRHI; mDev->DestroyPipeline(h); mPipelineRHI=0; }
            if (mDescSetRHI)    { NkDescSetHandle h; h.id=mDescSetRHI; mDev->FreeDescriptorSet(h); mDescSetRHI=0; }
            if (mDescLayoutRHI) { NkDescSetHandle h; h.id=mDescLayoutRHI; mDev->DestroyDescriptorSetLayout(h); mDescLayoutRHI=0; }
            if (mSamplerRHI)    { NkSamplerHandle h; h.id=mSamplerRHI; mDev->DestroySampler(h); mSamplerRHI=0; }
            mReady = false;
            mDev   = nullptr;
        }

        void NkRender2D::Begin(NkICommandBuffer* cmd, uint32 fbW, uint32 fbH,
                                const NkCamera2D* camera) {
            if (!mReady || !cmd) return;
            mCmd = cmd;
            mFBW = fbW;
            mFBH = fbH;
            mVerts.Clear();
            mIdx.Clear();
            mBatches.Clear();
            mCurTex    = {};
            mHasScissor= false;
            mCurStats  = {};

            // Matrice View-Projection
            if (camera) {
                mVP = camera->ViewProjection();
            } else {
                // Pixel-space : origine = top-left, Y vers le bas
                mVP = NkMat4f::Orthogonal(
                    NkVec2f(0.f, 0.f),
                    NkVec2f((float32)fbW, (float32)fbH),
                    -1.f, 1.f, false);
            }

            // Upload VP dans UBO
            NkBufferHandle uh; uh.id = mUBORHI;
            mDev->WriteBuffer(uh, &mVP, sizeof(NkMat4f));

            // Viewport + scissor
            cmd->SetViewport({0.f, 0.f, (float32)fbW, (float32)fbH});
            cmd->SetScissor({0, 0, (int32)fbW, (int32)fbH});
        }

        void NkRender2D::End() {
            Flush();
            mCmd = nullptr;
            mLastStats = mCurStats;
        }

        void NkRender2D::Flush() {
            if (mVerts.IsEmpty() || !mCmd) return;
            FlushInternal();
        }

        void NkRender2D::FlushInternal() {
            if (mVerts.IsEmpty() || mBatches.IsEmpty()) return;

            // Upload vertices et indices
            NkBufferHandle vbo; vbo.id = mVBORHI;
            NkBufferHandle ibo; ibo.id = mIBORHI;
            mDev->WriteBuffer(vbo, mVerts.Data(), (uint64)mVerts.Size() * sizeof(NkVertex2D));
            mDev->WriteBuffer(ibo, mIdx.Data(),   (uint64)mIdx.Size()   * sizeof(uint32));

            // Pipeline (lazy creation)
            if (!mPipelineRHI) {
                NkGraphicsPipelineDesc pd{};
                NkShaderHandle sh; sh.id = mShaderRHI;
                pd.shader = sh;
                pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
                pd.rasterizer = NkRasterizerDesc::NoCull();
                pd.depthStencil = NkDepthStencilDesc::NoDepth();
                pd.blend = NkBlendDesc::Alpha();
                pd.vertexLayout = NkVertex2D::Layout(); // direct use
                NkDescSetHandle dl; dl.id = mDescLayoutRHI;
                pd.descriptorSetLayouts.PushBack(dl);
                pd.renderPass = mDev->GetSwapchainRenderPass();
                NkPipelineHandle pipe = mDev->CreateGraphicsPipeline(pd);
                mPipelineRHI = pipe.id;
            }

            NkPipelineHandle pipe; pipe.id = mPipelineRHI;
            mCmd->BindGraphicsPipeline(pipe);
            mCmd->BindVertexBuffer(0, vbo);
            mCmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32);

            NkDescSetHandle ds; ds.id = mDescSetRHI;
            mCmd->BindDescriptorSet(ds, 0);

            // Exécuter les batches
            for (uint32 bi = 0; bi < (uint32)mBatches.Size(); ++bi) {
                const Batch& batch = mBatches[bi];
                if (batch.idxCount == 0) continue;

                // Texture
                uint64 texRHI  = mResources->GetTextureRHIId(batch.tex);
                uint64 sampRHI = mResources->GetSamplerRHIId(batch.tex);
                if (!texRHI) {
                    texRHI  = mResources->GetTextureRHIId(mResources->GetWhiteTexture());
                    sampRHI = mResources->GetSamplerRHIId(mResources->GetWhiteTexture());
                }

                NkDescriptorWrite tw{};
                tw.set = ds; tw.binding = 1;
                tw.type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                NkTextureHandle th; th.id = texRHI; tw.texture = th;
                NkSamplerHandle sh; sh.id = sampRHI; tw.sampler = sh;
                tw.textureLayout = NkResourceState::NK_SHADER_READ;
                mDev->UpdateDescriptorSets(&tw, 1);
                mCmd->BindDescriptorSet(ds, 0);

                // Scissor
                if (batch.hasScissor) {
                    mCmd->SetScissor({batch.scissor.x, batch.scissor.y,
                                       batch.scissor.width, batch.scissor.height});
                } else {
                    mCmd->SetScissor({0, 0, (int32)mFBW, (int32)mFBH});
                }

                mCmd->DrawIndexed(batch.idxCount, 1, batch.idxStart, 0, 0);
                mCurStats.drawCalls++;
            }

            mCurStats.vertices += (uint32)mVerts.Size();
            mCurStats.indices  += (uint32)mIdx.Size();
            mVerts.Clear();
            mIdx.Clear();
            mBatches.Clear();
            mCurTex = {};
        }

        void NkRender2D::EnsureTex(NkTextureHandle tex) {
            if (tex != mCurTex) {
                if (!mBatches.IsEmpty()) {
                    // Clore le batch courant
                    mBatches.Back().idxCount = (uint32)mIdx.Size() -
                                                mBatches.Back().idxStart;
                }
                NewBatch(tex);
                mCurStats.texSwaps++;
            }
        }

        void NkRender2D::NewBatch(NkTextureHandle tex) {
            Batch b{};
            b.tex       = tex;
            b.idxStart  = (uint32)mIdx.Size();
            b.idxCount  = 0;
            b.hasScissor= mHasScissor;
            b.scissor   = mCurScissor;
            mBatches.PushBack(b);
            mCurTex = tex;
        }

        void NkRender2D::AddQuad(const NkVertex2D& v0, const NkVertex2D& v1,
                                   const NkVertex2D& v2, const NkVertex2D& v3) {
            // Flush si dépassement
            if (mVerts.Size() + 4 >= mCfg.maxVertices ||
                mIdx.Size() + 6  >= mCfg.maxIndices) {
                FlushInternal();
                NewBatch(mCurTex);
            }
            if (mBatches.IsEmpty()) NewBatch(mCurTex);

            uint32 base = (uint32)mVerts.Size();
            mVerts.PushBack(v0); mVerts.PushBack(v1);
            mVerts.PushBack(v2); mVerts.PushBack(v3);
            mIdx.PushBack(base+0); mIdx.PushBack(base+1); mIdx.PushBack(base+2);
            mIdx.PushBack(base+0); mIdx.PushBack(base+2); mIdx.PushBack(base+3);
        }

        void NkRender2D::AddTri(const NkVertex2D& a, const NkVertex2D& b,
                                  const NkVertex2D& c) {
            if (mVerts.Size() + 3 >= mCfg.maxVertices ||
                mIdx.Size() + 3   >= mCfg.maxIndices) {
                FlushInternal();
                NewBatch(mCurTex);
            }
            if (mBatches.IsEmpty()) NewBatch(mCurTex);
            uint32 base = (uint32)mVerts.Size();
            mVerts.PushBack(a); mVerts.PushBack(b); mVerts.PushBack(c);
            mIdx.PushBack(base+0); mIdx.PushBack(base+1); mIdx.PushBack(base+2);
        }

        // ── Clip ──────────────────────────────────────────────────────────────────
        void NkRender2D::PushClip(const NkRectF& r) {
            Flush();
            mClipStack.PushBack(r);
            mHasScissor = true;
            mCurScissor = {(int32)r.x, (int32)r.y,
                            (int32)r.w, (int32)r.h};
        }
        void NkRender2D::PopClip() {
            if (mClipStack.IsEmpty()) return;
            Flush();
            mClipStack.PopBack();
            if (mClipStack.IsEmpty()) {
                mHasScissor = false;
            } else {
                const NkRectF& r = mClipStack.Back();
                mCurScissor = {(int32)r.x, (int32)r.y, (int32)r.w, (int32)r.h};
            }
        }
        void NkRender2D::ResetClip() {
            Flush();
            mClipStack.Clear();
            mHasScissor = false;
        }

        // ── DrawSprite ────────────────────────────────────────────────────────────
        void NkRender2D::DrawSprite(const NkRectF& dst, NkTextureHandle tex,
                                     const NkColorF& tint, const NkRectF& uv,
                                     Nk2DFlags flags) {
            if (!mCmd) return;
            EnsureTex(tex);

            float u0 = uv.x, v0 = uv.y, u1 = uv.x+uv.w, v1 = uv.y+uv.h;
            if (NK2D(flags, Nk2DFlags::NK_FLIP_X)) { float t=u0; u0=u1; u1=t; }
            if (NK2D(flags, Nk2DFlags::NK_FLIP_Y)) { float t=v0; v0=v1; v1=t; }

            AddQuad(
                {dst.x,      dst.y,      u0, v0, tint},
                {dst.Right(), dst.y,     u1, v0, tint},
                {dst.Right(), dst.Bottom(),u1,v1, tint},
                {dst.x,      dst.Bottom(), u0,v1, tint}
            );
        }

        void NkRender2D::DrawImage(NkTextureHandle tex, const NkRectF& dst,
                                    const NkRectF& uv, const NkColorF& tint) {
            DrawSprite(dst, tex, tint, uv);
        }

        // ── FillRect ──────────────────────────────────────────────────────────────
        void NkRender2D::FillRect(const NkRectF& r, const NkColorF& c) {
            if (!mCmd) return;
            EnsureTex({});
            AddQuad(
                {r.x,       r.y,       -1,-1, c},
                {r.Right(), r.y,       -1,-1, c},
                {r.Right(), r.Bottom(),-1,-1, c},
                {r.x,       r.Bottom(),-1,-1, c}
            );
        }

        void NkRender2D::FillRectGradH(const NkRectF& r,
                                         const NkColorF& cL, const NkColorF& cR) {
            EnsureTex({});
            AddQuad(
                {r.x,       r.y,       -1,-1, cL},
                {r.Right(), r.y,       -1,-1, cR},
                {r.Right(), r.Bottom(),-1,-1, cR},
                {r.x,       r.Bottom(),-1,-1, cL}
            );
        }

        void NkRender2D::FillRectGradV(const NkRectF& r,
                                         const NkColorF& cT, const NkColorF& cB) {
            EnsureTex({});
            AddQuad(
                {r.x,       r.y,       -1,-1, cT},
                {r.Right(), r.y,       -1,-1, cT},
                {r.Right(), r.Bottom(),-1,-1, cB},
                {r.x,       r.Bottom(),-1,-1, cB}
            );
        }

        void NkRender2D::FillCircle(const NkVec2f& center, float32 radius,
                                     const NkColorF& c, uint32 seg) {
            EnsureTex({});
            const float step = 2.f * 3.14159265f / seg;
            for (uint32 i = 0; i < seg; ++i) {
                float a0 = i * step, a1 = (i+1) * step;
                NkVertex2D v0{center.x, center.y, -1,-1, c};
                NkVertex2D v1{center.x + cosf(a0)*radius, center.y + sinf(a0)*radius,-1,-1,c};
                NkVertex2D v2{center.x + cosf(a1)*radius, center.y + sinf(a1)*radius,-1,-1,c};
                AddTri(v0, v1, v2);
            }
        }

        void NkRender2D::FillTriangle(const NkVec2f& a, const NkVec2f& b,
                                       const NkVec2f& c_, const NkColorF& col) {
            EnsureTex({});
            AddTri({a.x,a.y,-1,-1,col}, {b.x,b.y,-1,-1,col}, {c_.x,c_.y,-1,-1,col});
        }

        void NkRender2D::DrawLine(const NkVec2f& a, const NkVec2f& b,
                                   const NkColorF& c, float32 thick) {
            EnsureTex({});
            float dx = b.x-a.x, dy = b.y-a.y;
            float len = sqrtf(dx*dx+dy*dy);
            if (len < 0.0001f) return;
            float nx = -dy/len * thick*.5f;
            float ny =  dx/len * thick*.5f;
            AddQuad(
                {a.x-nx, a.y-ny, -1,-1, c},
                {b.x-nx, b.y-ny, -1,-1, c},
                {b.x+nx, b.y+ny, -1,-1, c},
                {a.x+nx, a.y+ny, -1,-1, c}
            );
        }

        void NkRender2D::DrawRect(const NkRectF& r, const NkColorF& c, float32 thick) {
            DrawLine({r.x,r.y},       {r.Right(),r.y},        c, thick);
            DrawLine({r.Right(),r.y}, {r.Right(),r.Bottom()},  c, thick);
            DrawLine({r.Right(),r.Bottom()},{r.x,r.Bottom()},  c, thick);
            DrawLine({r.x,r.Bottom()},{r.x,r.y},               c, thick);
        }

        void NkRender2D::DrawCircle(const NkVec2f& center, float32 radius,
                                     const NkColorF& c, float32 thick, uint32 seg) {
            const float step = 2.f * 3.14159265f / seg;
            for (uint32 i = 0; i < seg; ++i) {
                float a0 = i*step, a1 = (i+1)*step;
                NkVec2f p0{center.x+cosf(a0)*radius, center.y+sinf(a0)*radius};
                NkVec2f p1{center.x+cosf(a1)*radius, center.y+sinf(a1)*radius};
                DrawLine(p0, p1, c, thick);
            }
        }

        void NkRender2D::DrawPolyline(const NkVec2f* pts, uint32 n,
                                       const NkColorF& c, float32 thick, bool closed) {
            if (n < 2) return;
            for (uint32 i = 0; i < n-1; ++i) DrawLine(pts[i], pts[i+1], c, thick);
            if (closed) DrawLine(pts[n-1], pts[0], c, thick);
        }

        void NkRender2D::FillRoundRect(const NkRectF& r, const NkColorF& c,
                                        float32 radius, uint32 seg) {
            // Fill rect center + corners
            FillRect({r.x+radius, r.y, r.w-2*radius, r.h}, c);
            FillRect({r.x, r.y+radius, r.w, r.h-2*radius}, c);
            // Coins (quart de cercle)
            auto arc = [&](float cx, float cy, float startAngle) {
                const float step = (3.14159265f * .5f) / seg;
                for (uint32 i = 0; i < seg; ++i) {
                    float a0 = startAngle + i*step, a1 = startAngle + (i+1)*step;
                    NkVertex2D v0{cx, cy, -1,-1, c};
                    NkVertex2D v1{cx+cosf(a0)*radius, cy+sinf(a0)*radius,-1,-1,c};
                    NkVertex2D v2{cx+cosf(a1)*radius, cy+sinf(a1)*radius,-1,-1,c};
                    AddTri(v0,v1,v2);
                }
            };
            arc(r.x+radius,       r.y+radius,       3.14159265f);
            arc(r.Right()-radius, r.y+radius,       -3.14159265f*.5f);
            arc(r.Right()-radius, r.Bottom()-radius, 0.f);
            arc(r.x+radius,       r.Bottom()-radius,  3.14159265f*.5f);
        }

        void NkRender2D::DrawSpriteRotated(const NkRectF& dst, NkTextureHandle tex,
                                             float32 angleDeg, const NkVec2f& pivot,
                                             const NkColorF& tint, const NkRectF& uv) {
            EnsureTex(tex);
            float cx = dst.x + dst.w * pivot.x;
            float cy = dst.y + dst.h * pivot.y;
            float rad = angleDeg * 3.14159265f / 180.f;
            float co = cosf(rad), si = sinf(rad);

            auto rot = [&](float x, float y) -> NkVec2f {
                float rx = x - cx, ry = y - cy;
                return {cx + rx*co - ry*si, cy + rx*si + ry*co};
            };
            NkVec2f tl = rot(dst.x,       dst.y);
            NkVec2f tr = rot(dst.Right(), dst.y);
            NkVec2f br = rot(dst.Right(), dst.Bottom());
            NkVec2f bl = rot(dst.x,       dst.Bottom());

            AddQuad(
                {tl.x,tl.y, uv.x,       uv.y,       tint},
                {tr.x,tr.y, uv.x+uv.w,  uv.y,       tint},
                {br.x,br.y, uv.x+uv.w,  uv.y+uv.h,  tint},
                {bl.x,bl.y, uv.x,       uv.y+uv.h,  tint}
            );
        }

        void NkRender2D::DrawArrow(const NkVec2f& from, const NkVec2f& to,
                                    const NkColorF& c, float32 thick, float32 headSz) {
            float dx = to.x-from.x, dy = to.y-from.y;
            float len = sqrtf(dx*dx+dy*dy);
            if (len < 1.f) return;
            float nx = dx/len, ny = dy/len;
            NkVec2f shaft_to{to.x - nx*headSz, to.y - ny*headSz};
            DrawLine(from, shaft_to, c, thick);
            // Tête
            NkVec2f lx{to.x - nx*headSz - ny*headSz*.5f, to.y - ny*headSz + nx*headSz*.5f};
            NkVec2f rx{to.x - nx*headSz + ny*headSz*.5f, to.y - ny*headSz - nx*headSz*.5f};
            EnsureTex({});
            AddTri({to.x,to.y,-1,-1,c}, {lx.x,lx.y,-1,-1,c}, {rx.x,rx.y,-1,-1,c});
        }

        void NkRender2D::DrawBezier(const NkVec2f& p0, const NkVec2f& p1,
                                     const NkVec2f& p2, const NkVec2f& p3,
                                     const NkColorF& c, float32 thick, uint32 steps) {
            NkVec2f prev = p0;
            for (uint32 i = 1; i <= steps; ++i) {
                float t = (float)i / steps;
                float u = 1.f - t;
                float b0 = u*u*u, b1 = 3*u*u*t, b2 = 3*u*t*t, b3 = t*t*t;
                NkVec2f cur{
                    b0*p0.x + b1*p1.x + b2*p2.x + b3*p3.x,
                    b0*p0.y + b1*p1.y + b2*p2.y + b3*p3.y
                };
                DrawLine(prev, cur, c, thick);
                prev = cur;
            }
        }

        void NkRender2D::DrawNineSlice(const NkRectF& dst, NkTextureHandle tex,
                                         float32 bL, float32 bR, float32 bT, float32 bB,
                                         const NkColorF& tint) {
            // 9 quads : coins + bords + centre
            float xs[4] = {dst.x, dst.x+bL, dst.Right()-bR, dst.Right()};
            float ys[4] = {dst.y, dst.y+bT, dst.Bottom()-bB, dst.Bottom()};
            float us[4] = {0.f, bL/(float)mResources->GetTextureWidth(tex),
                            1.f - bR/(float)mResources->GetTextureWidth(tex), 1.f};
            float vs[4] = {0.f, bT/(float)mResources->GetTextureHeight(tex),
                            1.f - bB/(float)mResources->GetTextureHeight(tex), 1.f};
            EnsureTex(tex);
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    AddQuad(
                        {xs[col],   ys[row],   us[col],   vs[row],   tint},
                        {xs[col+1], ys[row],   us[col+1], vs[row],   tint},
                        {xs[col+1], ys[row+1], us[col+1], vs[row+1], tint},
                        {xs[col],   ys[row+1], us[col],   vs[row+1], tint}
                    );
                }
            }
        }

    } // namespace renderer
} // namespace nkentseu
