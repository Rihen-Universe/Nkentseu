// =============================================================================
// NkImGuiRHIBackend.cpp — Backend de rendu Dear ImGui -> NKRHI (bas niveau)
// -----------------------------------------------------------------------------
// Calque sur NkUINKRHIBackend (Applications/Sandbox/.../Base04). Memes shaders,
// meme layout de sommet (pos vec2 / uv vec2 / col u32 = 20 octets, identique a
// ImDrawVert), meme pipeline alpha + scissor. On concatene les cmd-lists ImGui
// dans un VBO/IBO dynamique puis on emet un scissor + DrawIndexed par commande.
// =============================================================================
#include "NkImGuiRHIBackend.h"

#include "imgui.h"                 // ImDrawData/ImDrawList/ImDrawCmd/ImDrawVert/ImDrawIdx/ImFontAtlas
#include "NKSL/NKSL.h"             // NkShaderConverter / NkShaderCache (Vulkan SPIR-V)
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace imguiintegration {

        // --- Shaders (identiques a NkUINKRHIBackend) -------------------------------
        static constexpr const char* kVertGLSL = R"GLSL(
#version 460 core
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(std140, binding=1) uniform ViewportBlock {
    vec2 uViewport;
    vec2 _pad;
};
out vec2  vUV;
out vec4  vColor;
void main() {
    vColor = vec4(
        float((aColor >>  0u) & 0xFFu) / 255.0,
        float((aColor >>  8u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 24u) & 0xFFu) / 255.0
    );
    vUV = aUV;
    vec2 ndc = (aPos / uViewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)GLSL";

        static constexpr const char* kFragGLSL = R"GLSL(
#version 460 core
in  vec2 vUV;
in  vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D uTex;
void main() {
    fragColor = vColor * texture(uTex, vUV);
}
)GLSL";

        static constexpr const char* kVertVk = R"GLSL(
#version 460
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(set=0, binding=1, std140) uniform ViewportBlock {
    vec2 uViewport;
    vec2 _pad;
} vpb;
layout(location=0) out vec2  vUV;
layout(location=1) out vec4  vColor;
void main() {
    vColor = vec4(
        float((aColor >>  0u) & 0xFFu) / 255.0,
        float((aColor >>  8u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 24u) & 0xFFu) / 255.0
    );
    vUV = aUV;
    vec2 ndc = (aPos / vpb.uViewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)GLSL";

        static constexpr const char* kFragVk = R"GLSL(
#version 460
layout(location=0) in  vec2 vUV;
layout(location=1) in  vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0, binding=0) uniform sampler2D uTex;
void main() {
    fragColor = vColor * texture(uTex, vUV);
}
)GLSL";

        static constexpr const char* kVertHlslDx11 = R"HLSL(
cbuffer Constants : register(b1) { float2 uViewport; float2 _pad; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; uint col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 c;
    c.r = float((v.col >>  0u) & 0xFFu) / 255.0;
    c.g = float((v.col >>  8u) & 0xFFu) / 255.0;
    c.b = float((v.col >> 16u) & 0xFFu) / 255.0;
    c.a = float((v.col >> 24u) & 0xFFu) / 255.0;
    o.col = c;
    float2 ndc = (v.pos / uViewport) * 2.0 - 1.0;
    o.pos = float4(ndc.x, -ndc.y, 0.0, 1.0);
    o.uv  = v.uv;
    return o;
}
)HLSL";

        static constexpr const char* kFragHlslDx11 = R"HLSL(
Texture2D    uTex     : register(t0);
SamplerState uSampler : register(s0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    return i.col * uTex.Sample(uSampler, i.uv);
}
)HLSL";

        static constexpr const char* kVertHlslDx12 = R"HLSL(
cbuffer Constants : register(b0) { float2 uViewport; float2 _pad; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; uint col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 c;
    c.r = float((v.col >>  0u) & 0xFFu) / 255.0;
    c.g = float((v.col >>  8u) & 0xFFu) / 255.0;
    c.b = float((v.col >> 16u) & 0xFFu) / 255.0;
    c.a = float((v.col >> 24u) & 0xFFu) / 255.0;
    o.col = c;
    float2 ndc = (v.pos / uViewport) * 2.0 - 1.0;
    o.pos = float4(ndc.x, -ndc.y, 0.0, 1.0);
    o.uv  = v.uv;
    return o;
}
)HLSL";

        static constexpr const char* kFragHlslDx12 = R"HLSL(
Texture2D    uTex     : register(t1);
SamplerState uSampler : register(s1);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    return i.col * uTex.Sample(uSampler, i.uv);
}
)HLSL";

        static NkShaderConvertResult CompileVkSpirv(const char* source, NkSLStage stage, const char* dbgName) {
            NkShaderCache& cache = NkShaderCache::Global();
            const uint64 key = NkShaderCache::ComputeKey(NkString(source), stage, "spirv");
            NkShaderConvertResult cached = cache.Load(key);
            if (cached.success && cached.SpirvWordCount() > 0 && cached.SpirvWords()[0] == 0x07230203) {
                return cached;
            }
            NkShaderConvertResult result = NkShaderConverter::GlslToSpirv(NkString(source), stage, dbgName);
            if (result.success) {
                cache.Save(key, result);
            }
            return result;
        }

        static uint64 GrowCapacityU64(uint64 current, uint64 required, uint64 minimum) {
            uint64 cap = current > minimum ? current : minimum;
            while (cap < required) {
                cap = cap + cap / 2; // x1.5 growth
                if (cap < minimum) cap = minimum;
            }
            return cap;
        }

        bool NkImGuiRHIBackend::Init(NkIDevice* device, NkRenderPassHandle renderPass, NkGraphicsApi api) {
            if (!device) return false;
            mDevice = device;
            mApi = api;
            mRenderPass = renderPass;

            if (!CreatePipeline()) {
                logger.Errorf("[NkImGuiRHIBackend] Failed to create pipeline\n");
                Destroy();
                return false;
            }
            if (!CreateResources()) {
                logger.Errorf("[NkImGuiRHIBackend] Failed to create resources\n");
                Destroy();
                return false;
            }
            return true;
        }

        bool NkImGuiRHIBackend::CreatePipeline() {
            NkShaderDesc shaderDesc;
            shaderDesc.debugName = "NkImGui_Backend";

            if (mApi == NkGraphicsApi::NK_GFX_API_OPENGL || mApi == NkGraphicsApi::NK_GFX_API_SOFTWARE) {
                shaderDesc.AddGLSL(NkShaderStage::NK_VERTEX, kVertGLSL);
                shaderDesc.AddGLSL(NkShaderStage::NK_FRAGMENT, kFragGLSL);
            } else if (mApi == NkGraphicsApi::NK_GFX_API_DX11) {
                shaderDesc.AddHLSL(NkShaderStage::NK_VERTEX, kVertHlslDx11, "VSMain");
                shaderDesc.AddHLSL(NkShaderStage::NK_FRAGMENT, kFragHlslDx11, "PSMain");
            } else if (mApi == NkGraphicsApi::NK_GFX_API_DX12) {
                shaderDesc.AddHLSL(NkShaderStage::NK_VERTEX, kVertHlslDx12, "VSMain");
                shaderDesc.AddHLSL(NkShaderStage::NK_FRAGMENT, kFragHlslDx12, "PSMain");
            } else if (mApi == NkGraphicsApi::NK_GFX_API_VULKAN) {
                NkShaderConvertResult vert = CompileVkSpirv(kVertVk, NkSLStage::NK_VERTEX, "NkImGuiBackend.vert");
                NkShaderConvertResult frag = CompileVkSpirv(kFragVk, NkSLStage::NK_FRAGMENT, "NkImGuiBackend.frag");
                if (!vert.success || !frag.success) {
                    logger.Errorf("[NkImGuiRHIBackend] Vulkan shader compilation failed\n");
                    return false;
                }
                shaderDesc.AddSPIRV(NkShaderStage::NK_VERTEX, vert.binary.Data(), vert.binary.Size());
                shaderDesc.AddSPIRV(NkShaderStage::NK_FRAGMENT, frag.binary.Data(), frag.binary.Size());
            }

            mShader = mDevice->CreateShader(shaderDesc);
            if (!mShader.IsValid()) return false;

            // ImDrawVert : pos(vec2 @0) / uv(vec2 @8) / col(u32 @16), stride 20.
            // Identique a NkUIVertex.
            NkVertexLayout layout;
            layout
                .AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT, 0,  "POSITION", 0)
                .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 8,  "TEXCOORD", 0)
                .AddAttribute(2, 0, NkGPUFormat::NK_R32_UINT,   16, "COLOR",    0)
                .AddBinding(0, static_cast<uint32>(sizeof(ImDrawVert)));

            NkDescriptorSetLayoutDesc descLayout;
            descLayout.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
            descLayout.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX);
            mLayout = mDevice->CreateDescriptorSetLayout(descLayout);
            if (!mLayout.IsValid()) return false;

            NkGraphicsPipelineDesc pd;
            pd.shader = mShader;
            pd.vertexLayout = layout;
            pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer = NkRasterizerDesc::NoCull();
            pd.rasterizer.scissorTest = true;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.blend = NkBlendDesc::Alpha();
            pd.renderPass = mRenderPass;
            pd.debugName = "NkImGui_Backend_Pipeline";
            pd.descriptorSetLayouts.PushBack(mLayout);

            mPipeline = mDevice->CreateGraphicsPipeline(pd);
            return mPipeline.IsValid();
        }

        bool NkImGuiRHIBackend::CreateResources() {
            mUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(16));
            if (!mUBO.IsValid()) return false;

            if (!EnsureGeometryBuffers(kMinVBOCap, kMinIBOCap)) return false;

            NkSamplerDesc samplerDesc{};
            samplerDesc.magFilter = NkFilter::NK_LINEAR;
            samplerDesc.minFilter = NkFilter::NK_LINEAR;
            samplerDesc.mipFilter = NkMipFilter::NK_NONE;
            samplerDesc.addressU = NkAddressMode::NK_CLAMP_TO_EDGE;
            samplerDesc.addressV = NkAddressMode::NK_CLAMP_TO_EDGE;
            samplerDesc.addressW = NkAddressMode::NK_CLAMP_TO_EDGE;
            mSampler = mDevice->CreateSampler(samplerDesc);
            return mSampler.IsValid();
        }

        bool NkImGuiRHIBackend::CreateDescriptorSetForTexture(NkTextureHandle texture, NkDescSetHandle& outSet) {
            if (!mDevice || !mLayout.IsValid() || !mSampler.IsValid() || !mUBO.IsValid() || !texture.IsValid()) {
                return false;
            }

            NkDescSetHandle set = mDevice->AllocateDescriptorSet(mLayout);
            if (!set.IsValid()) return false;

            NkDescriptorWrite writes[2] = {};
            writes[0].set = set;
            writes[0].binding = 0;
            writes[0].type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            writes[0].texture = texture;
            writes[0].sampler = mSampler;
            writes[0].textureLayout = NkResourceState::NK_SHADER_READ;

            writes[1].set = set;
            writes[1].binding = 1;
            writes[1].type = NkDescriptorType::NK_UNIFORM_BUFFER;
            writes[1].buffer = mUBO;
            writes[1].bufferOffset = 0;
            writes[1].bufferRange = 16;

            mDevice->UpdateDescriptorSets(writes, 2);
            outSet = set;
            return true;
        }

        bool NkImGuiRHIBackend::EnsureGeometryBuffers(uint64 requiredVtxBytes, uint64 requiredIdxBytes) {
            if (!mDevice) return false;

            if (requiredVtxBytes < kMinVBOCap) requiredVtxBytes = kMinVBOCap;
            if (requiredIdxBytes < kMinIBOCap) requiredIdxBytes = kMinIBOCap;

            if (!mVBO.IsValid() || requiredVtxBytes > mVBOCap) {
                const uint64 wanted = GrowCapacityU64(mVBOCap, requiredVtxBytes, kMinVBOCap);
                NkBufferHandle newVBO = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic(wanted));
                if (!newVBO.IsValid()) return false;
                if (mVBO.IsValid()) mDevice->DestroyBuffer(mVBO);
                mVBO = newVBO;
                mVBOCap = wanted;
            }

            if (!mIBO.IsValid() || requiredIdxBytes > mIBOCap) {
                const uint64 wanted = GrowCapacityU64(mIBOCap, requiredIdxBytes, kMinIBOCap);
                NkBufferHandle newIBO = mDevice->CreateBuffer(NkBufferDesc::IndexDynamic(wanted));
                if (!newIBO.IsValid()) return false;
                if (mIBO.IsValid()) mDevice->DestroyBuffer(mIBO);
                mIBO = newIBO;
                mIBOCap = wanted;
            }

            return mVBO.IsValid() && mIBO.IsValid();
        }

        bool NkImGuiRHIBackend::RebuildFontAtlas() {
            if (!mDevice) return false;
            if (!ImGui::GetCurrentContext()) return false;

            ImGuiIO& io = ImGui::GetIO();
            if (!io.Fonts) return false;

            unsigned char* pixels = nullptr;
            int width = 0, height = 0, bpp = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bpp);  // RGBA8
            if (!pixels || width <= 0 || height <= 0) return false;

            // Recreer si la taille a change.
            const bool reuse = mFontTex.IsValid() && mFontDescSet.IsValid()
                               && mFontW == width && mFontH == height;
            if (!reuse) {
                if (mFontDescSet.IsValid()) { mDevice->FreeDescriptorSet(mFontDescSet); mFontDescSet = {}; }
                if (mFontTex.IsValid())     { mDevice->DestroyTexture(mFontTex);        mFontTex = {}; }

                NkTextureDesc desc = NkTextureDesc::Tex2D(width, height, NkGPUFormat::NK_RGBA8_UNORM, 1);
                desc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
                desc.debugName = "NkImGui_FontAtlas";
                mFontTex = mDevice->CreateTexture(desc);
                if (!mFontTex.IsValid()) return false;
                mFontW = width;
                mFontH = height;
            }

            if (!mDevice->WriteTexture(mFontTex, pixels, static_cast<uint32>(width * 4))) {
                return false;
            }
            if (!mFontDescSet.IsValid()) {
                if (!CreateDescriptorSetForTexture(mFontTex, mFontDescSet)) return false;
            }

            // ImGui rememorise l'atlas via cette ID (cf. ImDrawCmd::TextureId).
            io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(1)));
            return true;
        }

        void NkImGuiRHIBackend::RenderDrawData(NkICommandBuffer* cmd, const ImDrawData* drawData, uint32 fbW, uint32 fbH) {
            if (!cmd || !drawData || !mPipeline.IsValid() || !mVBO.IsValid() || !mIBO.IsValid() || !mUBO.IsValid()) {
                return;
            }
            if (fbW == 0u || fbH == 0u || drawData->CmdListsCount <= 0) return;
            if (!mFontDescSet.IsValid()) return;   // atlas non construit

            const uint64 totalVtx = static_cast<uint64>(drawData->TotalVtxCount);
            const uint64 totalIdx = static_cast<uint64>(drawData->TotalIdxCount);
            if (totalVtx == 0 || totalIdx == 0) return;
            if (totalVtx > 0xFFFFFFFFull || totalIdx > 0xFFFFFFFFull) {
                logger.Warn("[NkImGuiRHIBackend] geometry overflow vtx=%llu idx=%llu\n",
                            static_cast<unsigned long long>(totalVtx),
                            static_cast<unsigned long long>(totalIdx));
                return;
            }

            const uint64 requiredVtxBytes = totalVtx * sizeof(ImDrawVert);
            const uint64 requiredIdxBytes = totalIdx * sizeof(uint32);
            if (!EnsureGeometryBuffers(requiredVtxBytes, requiredIdxBytes)) return;

            mScratchVtx.Resize(static_cast<usize>(requiredVtxBytes));
            mScratchIdx.Resize(static_cast<usize>(totalIdx));

            // Concatenation des cmd-lists. Les sommets sont copies tels quels ; les
            // indices sont juste convertis u16 -> u32 SANS rebase (on resout les
            // sommets via le parametre vertexOffset de DrawIndexed = base de la
            // cmd-list + dc.VtxOffset, cf. boucle de dessin). On garde donc les
            // bases par cmd-list pour la passe de dessin.
            uint32 vtxBase = 0;
            uint32 idxBase = 0;
            for (int n = 0; n < drawData->CmdListsCount; ++n) {
                const ImDrawList* cl = drawData->CmdLists[n];
                if (!cl) continue;
                const int vc = cl->VtxBuffer.Size;
                const int ic = cl->IdxBuffer.Size;
                if (vc <= 0 || ic <= 0) continue;

                std::memcpy(mScratchVtx.Data() + static_cast<usize>(vtxBase) * sizeof(ImDrawVert),
                            cl->VtxBuffer.Data,
                            static_cast<usize>(vc) * sizeof(ImDrawVert));

                const ImDrawIdx* src = cl->IdxBuffer.Data;
                for (int i = 0; i < ic; ++i) {
                    mScratchIdx[static_cast<usize>(idxBase + i)] = static_cast<uint32>(src[i]);
                }
                vtxBase += static_cast<uint32>(vc);
                idxBase += static_cast<uint32>(ic);
            }

            mDevice->WriteBuffer(mVBO, mScratchVtx.Data(), requiredVtxBytes);
            mDevice->WriteBuffer(mIBO, mScratchIdx.Data(), requiredIdxBytes);

            const float32 vp[4] = { static_cast<float32>(fbW), static_cast<float32>(fbH), 0.f, 0.f };
            mDevice->WriteBuffer(mUBO, vp, sizeof(vp));

            cmd->BindGraphicsPipeline(mPipeline);
            cmd->BindVertexBuffer(0, mVBO);
            cmd->BindIndexBuffer(mIBO, NkIndexFormat::NK_UINT32);
            cmd->BindDescriptorSet(mFontDescSet, 0);

            NkViewport viewport{ 0.f, 0.f, static_cast<float32>(fbW), static_cast<float32>(fbH), 0.f, 1.f };
            cmd->SetViewport(viewport);

            // ImGui : ClipRect en coords ImGui. On ramene en pixels fb (DisplayPos
            // + FramebufferScale) puis on emet un scissor par commande.
            const float32 clipOffX = drawData->DisplayPos.x;
            const float32 clipOffY = drawData->DisplayPos.y;
            const float32 clipScaleX = drawData->FramebufferScale.x != 0.f ? drawData->FramebufferScale.x : 1.f;
            const float32 clipScaleY = drawData->FramebufferScale.y != 0.f ? drawData->FramebufferScale.y : 1.f;
            const float32 fbWf = static_cast<float32>(fbW);
            const float32 fbHf = static_cast<float32>(fbH);

            // Indices/sommets globaux : on accumule les offsets au fil des cmd-lists.
            // globalVtxBase / globalIdxOffset = bases de la cmd-list courante dans
            // les buffers concatenes.
            uint32 globalVtxBase = 0;
            uint32 globalIdxOffset = 0;
            for (int n = 0; n < drawData->CmdListsCount; ++n) {
                const ImDrawList* cl = drawData->CmdLists[n];
                if (!cl) continue;
                const int vc = cl->VtxBuffer.Size;
                const int ic = cl->IdxBuffer.Size;
                const int cc = cl->CmdBuffer.Size;
                if (vc <= 0 || ic <= 0 || cc <= 0) continue;

                for (int ci = 0; ci < cc; ++ci) {
                    const ImDrawCmd& dc = cl->CmdBuffer[ci];
                    if (dc.UserCallback != nullptr) continue;   // callbacks non supportes ici
                    if (dc.ElemCount == 0u) continue;

                    float32 x0 = (dc.ClipRect.x - clipOffX) * clipScaleX;
                    float32 y0 = (dc.ClipRect.y - clipOffY) * clipScaleY;
                    float32 x1 = (dc.ClipRect.z - clipOffX) * clipScaleX;
                    float32 y1 = (dc.ClipRect.w - clipOffY) * clipScaleY;
                    if (x0 < 0.f) x0 = 0.f;
                    if (y0 < 0.f) y0 = 0.f;
                    if (x1 > fbWf) x1 = fbWf;
                    if (y1 > fbHf) y1 = fbHf;
                    if (x1 <= x0 || y1 <= y0) continue;

                    NkRect2D scissor{};
                    scissor.x = static_cast<int32>(x0);
                    scissor.w = static_cast<int32>(x1 - x0);
                    scissor.h = static_cast<int32>(y1 - y0);
                    if (mApi == NkGraphicsApi::NK_GFX_API_OPENGL) {
                        // ImGui clip = haut-gauche ; scissor GL = bas-gauche.
                        scissor.y = static_cast<int32>(fbHf - y1);
                        if (scissor.y < 0) scissor.y = 0;
                    } else {
                        scissor.y = static_cast<int32>(y0);
                    }
                    cmd->SetScissor(scissor);

                    // Les indices scratch ne sont PAS rebases : on resout les
                    // sommets via vertexOffset = base de la cmd-list dans le buffer
                    // concatene + dc.VtxOffset (relatif a la cmd-list, >0 seulement
                    // au-dela de 64K sommets, ImGui >=1.71). firstIndex selectionne
                    // la plage [IdxOffset, +ElemCount) dans l'IBO concatene.
                    cmd->DrawIndexed(dc.ElemCount, 1,
                                     globalIdxOffset + dc.IdxOffset,
                                     static_cast<int32>(globalVtxBase + dc.VtxOffset), 0);
                }
                globalIdxOffset += static_cast<uint32>(ic);
                globalVtxBase   += static_cast<uint32>(vc);
            }
        }

        void NkImGuiRHIBackend::Destroy() {
            if (!mDevice) return;

            // Detacher l'atlas du contexte ImGui s'il est encore vivant.
            if (ImGui::GetCurrentContext()) {
                ImGuiIO& io = ImGui::GetIO();
                if (io.Fonts) io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(0));
            }

            if (mFontDescSet.IsValid()) mDevice->FreeDescriptorSet(mFontDescSet);
            if (mFontTex.IsValid())     mDevice->DestroyTexture(mFontTex);
            if (mLayout.IsValid())      mDevice->DestroyDescriptorSetLayout(mLayout);
            if (mPipeline.IsValid())    mDevice->DestroyPipeline(mPipeline);
            if (mShader.IsValid())      mDevice->DestroyShader(mShader);
            if (mVBO.IsValid())         mDevice->DestroyBuffer(mVBO);
            if (mIBO.IsValid())         mDevice->DestroyBuffer(mIBO);
            if (mUBO.IsValid())         mDevice->DestroyBuffer(mUBO);
            if (mSampler.IsValid())     mDevice->DestroySampler(mSampler);

            mFontDescSet = {};
            mFontTex = {};
            mLayout = {};
            mPipeline = {};
            mShader = {};
            mVBO = {};
            mIBO = {};
            mUBO = {};
            mSampler = {};
            mVBOCap = 0;
            mIBOCap = 0;
            mFontW = 0;
            mFontH = 0;
            mScratchVtx.Clear();
            mScratchIdx.Clear();
            mDevice = nullptr;
        }

    } // namespace imguiintegration
} // namespace nkentseu
