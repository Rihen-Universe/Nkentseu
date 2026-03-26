// =============================================================================
// NkRenderer2D.cpp — Batch renderer 2D.
// =============================================================================
#include "pch/pch.h"
#include "NKRenderer/2D/NkRenderer2D.h"
#include "NKMath/NkFunctions.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {

    // =========================================================================
    // Shaders intégrés
    // =========================================================================

    const char* NkRenderer2D::s_VertGLSL = R"GLSL(
        #version 450
        layout(location=0) in vec2  aPos;
        layout(location=1) in vec2  aUV;
        layout(location=2) in vec4  aColor;

        layout(set=0, binding=0) uniform Camera { mat4 uVP; };

        layout(location=0) out vec2 vUV;
        layout(location=1) out vec4 vColor;

        void main() {
            gl_Position = uVP * vec4(aPos, 0.0, 1.0);
            vUV    = aUV;
            vColor = aColor;
        }
    )GLSL";

    const char* NkRenderer2D::s_FragGLSL = R"GLSL(
        #version 450
        layout(location=0) in  vec2 vUV;
        layout(location=1) in  vec4 vColor;
        layout(location=0) out vec4 outColor;

        layout(set=1, binding=0) uniform sampler2D uTexture;

        void main() {
            outColor = texture(uTexture, vUV) * vColor;
        }
    )GLSL";

    const char* NkRenderer2D::s_VertHLSL = R"HLSL(
        cbuffer Camera : register(b0) { float4x4 uVP; }
        struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };
        struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };
        VSOut main(VSIn i) {
            VSOut o;
            o.pos = mul(float4(i.pos, 0, 1), uVP);
            o.uv  = i.uv; o.col = i.col;
            return o;
        }
    )HLSL";

    const char* NkRenderer2D::s_FragHLSL = R"HLSL(
        Texture2D    uTexture : register(t0);
        SamplerState uSampler : register(s0);
        struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };
        float4 main(PSIn i) : SV_TARGET {
            return uTexture.Sample(uSampler, i.uv) * i.col;
        }
    )HLSL";

    // =========================================================================
    // Init / Destroy
    // =========================================================================

    bool NkRenderer2D::Init(NkIDevice* device,
                            NkRenderPassHandle renderPass,
                            const NkRenderer2DDesc& desc) noexcept {
        if (!device || !renderPass.IsValid()) return false;
        mDevice   = device;
        mDesc     = desc;
        mMaxQuads = desc.maxQuadsPerBatch;

        if (!InitBuffers())            return false;
        if (!InitDescSets())           return false;
        if (!InitPipelines(renderPass))return false;

        mIsValid = true;
        return true;
    }

    bool NkRenderer2D::InitBuffers() noexcept {
        // Vertex buffer dynamique (max_quads × 4 vertices)
        mVBO = mDevice->CreateBuffer(
            NkBufferDesc::VertexDynamic(mMaxQuads * 4 * sizeof(NkVertex2D)));
        if (!mVBO.IsValid()) return false;

        // Index buffer statique : 0,1,2, 2,3,0  pour chaque quad
        NkVector<uint32> indices;
        indices.Resize(mMaxQuads * 6);
        for (uint32 i = 0, v = 0; i < mMaxQuads * 6; i += 6, v += 4) {
            indices[i+0] = v+0; indices[i+1] = v+1; indices[i+2] = v+2;
            indices[i+3] = v+2; indices[i+4] = v+3; indices[i+5] = v+0;
        }
        mIBO = mDevice->CreateBuffer(
            NkBufferDesc::Index(mMaxQuads * 6 * sizeof(uint32), indices.Data()));
        if (!mIBO.IsValid()) return false;

        // UBO caméra (view-projection matrix)
        mCameraUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkMat4)));
        if (!mCameraUBO.IsValid()) return false;

        mVertices.Resize(mMaxQuads * 4);
        return true;
    }

    bool NkRenderer2D::InitDescSets() noexcept {
        // Set 0 : camera UBO
        {
            NkDescriptorSetLayoutDesc layout;
            layout.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX);
            mDescSetLayout = mDevice->CreateDescriptorSetLayout(layout);
            if (!mDescSetLayout.IsValid()) return false;

            mDescSet = mDevice->AllocateDescriptorSet(mDescSetLayout);
            if (!mDescSet.IsValid()) return false;

            mDevice->BindUniformBuffer(mDescSet, 0, mCameraUBO, sizeof(NkMat4));
        }
        return true;
    }

    bool NkRenderer2D::InitPipelines(NkRenderPassHandle rp) noexcept {
        // Compiler le shader
        NkShaderSource src;
        src.vertexGLSL   = s_VertGLSL;
        src.fragmentGLSL = s_FragGLSL;
        src.vertexHLSLdx11 = src.vertexHLSLdx12 = s_VertHLSL;
        src.fragmentHLSLdx11 = src.fragmentHLSLdx12 = s_FragHLSL;
        src.debugName = "NkRenderer2D";

        if (!mShader.Compile(mDevice, src)) return false;

        // Layout vertex : pos(2f), uv(2f), color(1u = vec4 packed)
        NkVertexLayout vl;
        vl.AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,  0, "POSITION")
          .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,  8, "TEXCOORD")
          .AddAttribute(2, 0, NkGPUFormat::NK_RGBA8_UNORM, 16, "COLOR")
          .AddBinding(0, sizeof(NkVertex2D));

        // Pipeline opaque
        {
            NkGraphicsPipelineDesc d;
            d.shader       = mShader.GetHandle();
            d.vertexLayout = vl;
            d.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            d.rasterizer   = NkRasterizerDesc::NoCull();
            d.depthStencil = NkDepthStencilDesc::NoDepth();
            d.blend        = NkBlendDesc::Opaque();
            d.renderPass   = rp;
            d.debugName    = "2D_Opaque";
            if (!mPipelineOpaque.Create(mDevice, d)) return false;
        }

        // Pipeline alpha
        {
            NkGraphicsPipelineDesc d;
            d.shader       = mShader.GetHandle();
            d.vertexLayout = vl;
            d.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            d.rasterizer   = NkRasterizerDesc::NoCull();
            d.depthStencil = NkDepthStencilDesc::NoDepth();
            d.blend        = NkBlendDesc::Alpha();
            d.renderPass   = rp;
            d.debugName    = "2D_Alpha";
            if (!mPipelineAlpha.Create(mDevice, d)) return false;
        }

        return true;
    }

    void NkRenderer2D::Destroy() noexcept {
        if (!mDevice) return;
        mPipelineAlpha.Destroy();
        mPipelineOpaque.Destroy();
        mShader.Destroy();
        if (mDescSet.IsValid())       mDevice->FreeDescriptorSet(mDescSet);
        if (mDescSetLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mDescSetLayout);
        if (mCameraUBO.IsValid())     mDevice->DestroyBuffer(mCameraUBO);
        if (mIBO.IsValid())           mDevice->DestroyBuffer(mIBO);
        if (mVBO.IsValid())           mDevice->DestroyBuffer(mVBO);
        mDevice  = nullptr;
        mIsValid = false;
    }

    // =========================================================================
    // Frame
    // =========================================================================

    void NkRenderer2D::SetCamera(const NkCamera2D& cam) noexcept {
        SetViewProjection(cam.GetViewProjection());
    }

    void NkRenderer2D::SetViewProjection(const NkMat4& vp) noexcept {
        mVP = vp;
        if (mDevice && mCameraUBO.IsValid()) {
            mDevice->WriteBuffer(mCameraUBO, &mVP, sizeof(NkMat4));
        }
    }

    void NkRenderer2D::Begin(NkICommandBuffer* cmd,
                              NkFramebufferHandle fb,
                              uint32 w, uint32 h) noexcept {
        if (!mIsValid || !cmd) return;
        mCmd       = cmd;
        mInBatch   = true;
        mQuadCount = 0;
        mBatches.Clear();
        mCurrentTex  = {};
        mCurrentSamp = {};
        mCurrentBatchStart = 0;
        mStats = {};

        // Upload VP si pas encore fait
        mDevice->WriteBuffer(mCameraUBO, &mVP, sizeof(NkMat4));

        // Start render pass
        NkClearValue cv; cv.color = {0,0,0,0};
        cmd->BeginRenderPass(mDevice->GetSwapchainRenderPass(), fb,
                             {0,0,(int32)w,(int32)h}, &cv, 1);

        // Viewport
        cmd->SetViewport({0.f, 0.f, (float32)w, (float32)h});
        cmd->SetScissor ({0,0,(int32)w,(int32)h});
    }

    void NkRenderer2D::End() noexcept {
        if (!mInBatch) return;
        FlushBatch();
        if (mCmd) mCmd->EndRenderPass();
        mCmd    = nullptr;
        mInBatch= false;
    }

    // =========================================================================
    // Batch helpers
    // =========================================================================

    void NkRenderer2D::FlushBatch() noexcept {
        if (mQuadCount == 0 || !mCmd) return;

        // Upload vertices
        mDevice->WriteBuffer(mVBO, mVertices.Data(),
                             mQuadCount * 4 * sizeof(NkVertex2D));

        // Bind buffers
        mCmd->BindVertexBuffer(0, mVBO, 0);
        mCmd->BindIndexBuffer(mIBO, NkIndexFormat::NK_UINT32, 0);

        // Bind camera descriptor set 0
        mCmd->BindDescriptorSet(mPipelineAlpha.GetHandle(), 0, mDescSet);

        // Rendu par batch (une texture = un draw call)
        for (auto& batch : mBatches) {
            if (batch.quadCount == 0) continue;

            // Descriptor set 1 : texture
            // (On utilise BindTextureSampler directement via UpdateDescriptorSets
            //  dans une implémentation complète — ici on passe par push constant ou
            //  on rebind le set 1 à chaque batch)
            // TODO: pour une implémentation complète, utiliser un pool de descriptor sets
            // et assigner set1 à chaque batch unique texture.

            // Pipeline selon la présence d'alpha (simplifié : toujours alpha)
            mCmd->BindPipeline(mPipelineAlpha.GetHandle());

            uint32 firstIndex = batch.quadStart * 6;
            uint32 indexCount = batch.quadCount * 6;
            mCmd->DrawIndexed(indexCount, 1, firstIndex, 0, 0);

            ++mStats.drawCalls;
        }

        mStats.quadCount += mQuadCount;
        mQuadCount         = 0;
        mCurrentBatchStart = 0;
        mBatches.Clear();
    }

    void NkRenderer2D::PushQuad(const NkVertex2D* v4,
                                 NkTextureHandle tex, NkSamplerHandle samp) noexcept {
        if (mQuadCount >= mMaxQuads) FlushBatch();

        uint32 base = mQuadCount * 4;
        mVertices[base+0] = v4[0];
        mVertices[base+1] = v4[1];
        mVertices[base+2] = v4[2];
        mVertices[base+3] = v4[3];

        // Batch grouping : si même texture, on continue
        bool newBatch = mBatches.IsEmpty()
                      || mCurrentTex.id  != tex.id
                      || mCurrentSamp.id != samp.id;

        if (newBatch) {
            if (!mBatches.IsEmpty()) {
                mBatches.Back().quadCount = mQuadCount - mCurrentBatchStart;
            }
            mCurrentBatchStart = mQuadCount;
            mCurrentTex  = tex;
            mCurrentSamp = samp;
            BatchEntry be;
            be.tex = tex; be.samp = samp;
            be.quadStart = mQuadCount;
            be.quadCount = 0;
            mBatches.PushBack(be);
            ++mStats.textureSwaps;
        }

        ++mQuadCount;
        mBatches.Back().quadCount = mQuadCount - mCurrentBatchStart;
    }

    void NkRenderer2D::BuildQuad(NkVertex2D* out4,
                                  NkVec2 pos, NkVec2 size, float32 rot,
                                  NkVec2 pivot, NkUVRect uv, uint32 color) const noexcept {
        // Corners in local space (pivot at origin)
        float32 px = pivot.x * size.x;
        float32 py = pivot.y * size.y;
        float32 x0 = -px,         y0 = -py;
        float32 x1 = size.x - px, y1 = size.y - py;

        // Rotation
        float32 cosR = 1.f, sinR = 0.f;
        if (rot != 0.f) {
            cosR = math::Cos(math::DegToRad(rot));
            sinR = math::Sin(math::DegToRad(rot));
        }

        auto rotated = [&](float32 x, float32 y, float32& ox, float32& oy) {
            ox = pos.x + x * cosR - y * sinR;
            oy = pos.y + x * sinR + y * cosR;
        };

        rotated(x0, y0, out4[0].x, out4[0].y); out4[0].u=uv.u0; out4[0].v=uv.v0; out4[0].color=color;
        rotated(x1, y0, out4[1].x, out4[1].y); out4[1].u=uv.u1; out4[1].v=uv.v0; out4[1].color=color;
        rotated(x1, y1, out4[2].x, out4[2].y); out4[2].u=uv.u1; out4[2].v=uv.v1; out4[2].color=color;
        rotated(x0, y1, out4[3].x, out4[3].y); out4[3].u=uv.u0; out4[3].v=uv.v1; out4[3].color=color;
    }

    // =========================================================================
    // Dessins
    // =========================================================================

    void NkRenderer2D::DrawSprite(const NkSprite& s) noexcept {
        if (!s.visible || !s.texture.IsValid() || !s.texture->IsValid()) return;
        NkVertex2D v[4];
        uint32 col = NkPackColor(s.tint);
        BuildQuad(v, s.position, {s.size.x*s.scale.x, s.size.y*s.scale.y},
                  s.rotation, s.pivot, s.GetFinalUV(), col);
        PushQuad(v, s.texture->GetHandle(), s.texture->GetSampler());
    }

    void NkRenderer2D::DrawTexture(NkTextureRef tex, NkVec2 pos, NkVec2 size,
                                    NkColor4f tint, NkUVRect uv, float32 rot) noexcept {
        if (!tex.IsValid() || !tex->IsValid()) return;
        NkVertex2D v[4];
        BuildQuad(v, pos, size, rot, {0,0}, uv, NkPackColor(tint));
        PushQuad(v, tex->GetHandle(), tex->GetSampler());
    }

    void NkRenderer2D::DrawRect(NkVec2 pos, NkVec2 size,
                                 NkColor4f color, float32 rot) noexcept {
        // Quad blanc (texture 1×1 blanche) — nécessite une white texture
        NkVertex2D v[4];
        BuildQuad(v, pos, size, rot, {0,0}, NkUVRect::Full(), NkPackColor(color));
        // Sans texture (on utilise un blanc intégré) — le shader multiplie
        // tint × texture : si texture=blanc → couleur pure
        PushQuad(v, {}, {});
    }

    void NkRenderer2D::DrawLine(NkVec2 a, NkVec2 b,
                                 float32 thickness, NkColor4f color) noexcept {
        NkVec2 dir = {b.x-a.x, b.y-a.y};
        float32 len = math::Sqrt(dir.x*dir.x + dir.y*dir.y);
        if (len < 0.001f) return;
        NkVec2 norm = {-dir.y/len * thickness*0.5f, dir.x/len * thickness*0.5f};

        NkVertex2D v[4];
        uint32 col = NkPackColor(color);
        v[0] = {a.x+norm.x, a.y+norm.y, 0,0, col};
        v[1] = {b.x+norm.x, b.y+norm.y, 1,0, col};
        v[2] = {b.x-norm.x, b.y-norm.y, 1,1, col};
        v[3] = {a.x-norm.x, a.y-norm.y, 0,1, col};
        PushQuad(v, {}, {});
    }

    void NkRenderer2D::DrawRectOutline(NkVec2 pos, NkVec2 size,
                                        float32 thickness, NkColor4f color) noexcept {
        NkVec2 br = {pos.x+size.x, pos.y+size.y};
        DrawLine({pos.x, pos.y},   {br.x,  pos.y}, thickness, color);
        DrawLine({br.x,  pos.y},   {br.x,  br.y},  thickness, color);
        DrawLine({br.x,  br.y},    {pos.x, br.y},  thickness, color);
        DrawLine({pos.x, br.y},    {pos.x, pos.y}, thickness, color);
    }

    void NkRenderer2D::DrawCircle(NkVec2 center, float32 radius,
                                   NkColor4f color, uint32 segments) noexcept {
        uint32 col = NkPackColor(color);
        float32 step = 2.f * 3.14159265f / (float32)segments;
        for (uint32 i = 0; i < segments; ++i) {
            float32 a0 = step * (float32)i;
            float32 a1 = step * (float32)(i+1);
            float32 c0 = math::Cos(a0), s0 = math::Sin(a0);
            float32 c1 = math::Cos(a1), s1 = math::Sin(a1);
            // Triangle fan : center + 2 points
            NkVertex2D v[4];
            v[0] = {center.x, center.y, 0.5f, 0.5f, col};
            v[1] = {center.x+c0*radius, center.y+s0*radius, (c0+1)*0.5f, (s0+1)*0.5f, col};
            v[2] = {center.x+c1*radius, center.y+s1*radius, (c1+1)*0.5f, (s1+1)*0.5f, col};
            v[3] = v[2]; // dégénéré — on utilise 3 vertices sur 4 (quad avec dernier = v2)
            PushQuad(v, {}, {});
        }
    }

    void NkRenderer2D::DrawRoundedRect(NkVec2 pos, NkVec2 size,
                                        float32 radius, NkColor4f color) noexcept {
        // Centre + 4 côtés droits + 4 arcs de coins
        float32 r   = math::Min(radius, math::Min(size.x, size.y) * 0.5f);
        uint32  col = NkPackColor(color);

        // Rect central (sans les coins)
        DrawRect({pos.x + r, pos.y},         {size.x - 2*r, size.y},          color);
        DrawRect({pos.x,     pos.y + r},      {r,            size.y - 2*r},     color);
        DrawRect({pos.x + size.x - r, pos.y + r}, {r,        size.y - 2*r},     color);

        // 4 coins (disque à 1/4 de tour)
        auto corner = [&](NkVec2 c, float32 fromDeg, float32 toDeg) {
            constexpr uint32 segs = 8;
            float32 step = (toDeg - fromDeg) / (float32)segs;
            for (uint32 i = 0; i < segs; ++i) {
                float32 a0 = math::DegToRad(fromDeg + step * (float32)i);
                float32 a1 = math::DegToRad(fromDeg + step * (float32)(i+1));
                NkVertex2D v[4];
                v[0] = {c.x,                    c.y,                    0.5f, 0.5f, col};
                v[1] = {c.x + math::Cos(a0)*r,  c.y + math::Sin(a0)*r, 0.f,  0.f,  col};
                v[2] = {c.x + math::Cos(a1)*r,  c.y + math::Sin(a1)*r, 1.f,  1.f,  col};
                v[3] = v[2];
                PushQuad(v, {}, {});
            }
        };

        corner({pos.x + r,          pos.y + r         }, 180.f, 270.f);
        corner({pos.x + size.x - r, pos.y + r         }, 270.f, 360.f);
        corner({pos.x + size.x - r, pos.y + size.y - r},   0.f,  90.f);
        corner({pos.x + r,          pos.y + size.y - r},  90.f, 180.f);
        (void)col;
    }

    // =========================================================================
    // Formes libres
    // =========================================================================

    void NkRenderer2D::DrawTriangle(NkVec2 v0, NkVec2 v1, NkVec2 v2,
                                     NkColor4f color) noexcept {
        uint32 col = NkPackColor(color);
        NkVertex2D v[4];
        v[0] = {v0.x, v0.y, 0.f, 0.f, col};
        v[1] = {v1.x, v1.y, 1.f, 0.f, col};
        v[2] = {v2.x, v2.y, 0.5f,1.f, col};
        v[3] = v[2]; // dégénéré
        PushQuad(v, {}, {});
    }

    void NkRenderer2D::DrawTriangleTextured(NkVec2 v0, NkVec2 v1, NkVec2 v2,
                                             NkVec2 uv0, NkVec2 uv1, NkVec2 uv2,
                                             NkTextureRef tex,
                                             NkColor4f tint) noexcept {
        if (!tex.IsValid() || !tex->IsValid()) return;
        uint32 col = NkPackColor(tint);
        NkVertex2D v[4];
        v[0] = {v0.x, v0.y, uv0.x, uv0.y, col};
        v[1] = {v1.x, v1.y, uv1.x, uv1.y, col};
        v[2] = {v2.x, v2.y, uv2.x, uv2.y, col};
        v[3] = v[2]; // dégénéré
        PushQuad(v, tex->GetHandle(), tex->GetSampler());
    }

    void NkRenderer2D::DrawConvexPolygon(const NkVec2* points, uint32 count,
                                          NkColor4f color) noexcept {
        if (!points || count < 3) return;
        // Triangle fan depuis points[0]
        for (uint32 i = 1; i + 1 < count; ++i) {
            DrawTriangle(points[0], points[i], points[i+1], color);
        }
    }

    void NkRenderer2D::DrawConvexPolygonTextured(const NkVec2* points, const NkVec2* uvs,
                                                  uint32 count, NkTextureRef tex,
                                                  NkColor4f tint) noexcept {
        if (!points || !uvs || count < 3) return;
        for (uint32 i = 1; i + 1 < count; ++i) {
            DrawTriangleTextured(points[0], points[i], points[i+1],
                                 uvs[0],   uvs[i],   uvs[i+1],
                                 tex, tint);
        }
    }

    void NkRenderer2D::DrawCircleTextured(NkVec2 center, float32 radius,
                                           NkTextureRef tex,
                                           NkColor4f tint, uint32 segments) noexcept {
        if (!tex.IsValid() || !tex->IsValid()) return;
        uint32  col  = NkPackColor(tint);
        float32 step = 2.f * 3.14159265f / (float32)segments;
        for (uint32 i = 0; i < segments; ++i) {
            float32 a0 = step * (float32)i;
            float32 a1 = step * (float32)(i+1);
            float32 c0 = math::Cos(a0), s0 = math::Sin(a0);
            float32 c1 = math::Cos(a1), s1 = math::Sin(a1);
            NkVertex2D v[4];
            v[0] = {center.x,             center.y,             0.5f,          0.5f,          col};
            v[1] = {center.x+c0*radius,   center.y+s0*radius,   (c0+1.f)*0.5f, (s0+1.f)*0.5f, col};
            v[2] = {center.x+c1*radius,   center.y+s1*radius,   (c1+1.f)*0.5f, (s1+1.f)*0.5f, col};
            v[3] = v[2];
            PushQuad(v, tex->GetHandle(), tex->GetSampler());
        }
    }

    void NkRenderer2D::DrawArc(NkVec2 center, float32 radius,
                                float32 startAngleDeg, float32 endAngleDeg,
                                float32 thickness, NkColor4f color, uint32 segments) noexcept {
        if (segments < 1) return;
        float32 fromRad = math::DegToRad(startAngleDeg);
        float32 toRad   = math::DegToRad(endAngleDeg);
        float32 step    = (toRad - fromRad) / (float32)segments;

        for (uint32 i = 0; i < segments; ++i) {
            float32 a0 = fromRad + step * (float32)i;
            float32 a1 = fromRad + step * (float32)(i+1);
            NkVec2 p0 = {center.x + math::Cos(a0)*radius, center.y + math::Sin(a0)*radius};
            NkVec2 p1 = {center.x + math::Cos(a1)*radius, center.y + math::Sin(a1)*radius};
            DrawLine(p0, p1, thickness, color);
        }
    }

    void NkRenderer2D::DrawBezierCubic(NkVec2 p0, NkVec2 p1, NkVec2 p2, NkVec2 p3,
                                        float32 thickness, NkColor4f color,
                                        uint32 segments) noexcept {
        if (segments < 1) return;
        NkVec2 prev = p0;
        for (uint32 i = 1; i <= segments; ++i) {
            float32 t  = (float32)i / (float32)segments;
            float32 it = 1.f - t;
            float32 b0 = it*it*it;
            float32 b1 = 3.f*it*it*t;
            float32 b2 = 3.f*it*t*t;
            float32 b3 = t*t*t;
            NkVec2 pt  = {b0*p0.x + b1*p1.x + b2*p2.x + b3*p3.x,
                           b0*p0.y + b1*p1.y + b2*p2.y + b3*p3.y};
            DrawLine(prev, pt, thickness, color);
            prev = pt;
        }
    }

    void NkRenderer2D::DrawBezierQuadratic(NkVec2 p0, NkVec2 ctrl, NkVec2 p2,
                                            float32 thickness, NkColor4f color,
                                            uint32 segments) noexcept {
        if (segments < 1) return;
        NkVec2 prev = p0;
        for (uint32 i = 1; i <= segments; ++i) {
            float32 t  = (float32)i / (float32)segments;
            float32 it = 1.f - t;
            float32 b0 = it*it;
            float32 b1 = 2.f*it*t;
            float32 b2 = t*t;
            NkVec2 pt  = {b0*p0.x + b1*ctrl.x + b2*p2.x,
                           b0*p0.y + b1*ctrl.y + b2*p2.y};
            DrawLine(prev, pt, thickness, color);
            prev = pt;
        }
    }

    void NkRenderer2D::DrawText(NkFontFace* font, const char* text,
                                 NkVec2 pos, float32 scale,
                                 NkColor4f color, NkTextureRef fontAtlasTex) noexcept {
        if (!font || !text || !fontAtlasTex.IsValid()) return;

        NkGlyphRun run;
        if (!font->ShapeText(text, run)) return;

        float32 x = pos.x;
        uint32  col = NkPackColor(color);

        for (auto& gi : run) {
            NkGlyph glyph;
            if (!font->GetGlyphById(gi.glyphId, glyph)) { x += (float32)gi.xAdvance / 64.f * scale; continue; }
            if (glyph.isEmpty) { x += (float32)gi.xAdvance / 64.f * scale; continue; }

            float32 gx = x + (float32)glyph.metrics.bearingX / 64.f * scale;
            float32 gy = pos.y - (float32)glyph.metrics.bearingY / 64.f * scale;
            float32 gw = (float32)glyph.bitmap.width  * scale;
            float32 gh = (float32)glyph.bitmap.height * scale;

            NkUVRect uv{glyph.uv.u0, glyph.uv.v0, glyph.uv.u1, glyph.uv.v1};
            NkVertex2D v[4];
            BuildQuad(v, {gx,gy}, {gw,gh}, 0, {0,0}, uv, col);
            PushQuad(v, fontAtlasTex->GetHandle(), fontAtlasTex->GetSampler());

            x += (float32)gi.xAdvance / 64.f * scale;
        }
    }

} // namespace nkentseu
