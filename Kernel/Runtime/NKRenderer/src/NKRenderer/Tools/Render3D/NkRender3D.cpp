// =============================================================================
// NkRender3D.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkRender3D.h"
#include "../Shadow/NkShadowSystem.h"
#include "../Environment/NkEnvironmentSystem.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKRenderer/Core/NkResources.h"
#include "NkRender3D_PBRShaders.inl"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <algorithm>

namespace nkentseu {
    namespace renderer {

        NkRender3D::~NkRender3D() { Shutdown(); }

        bool NkRender3D::Init(NkIDevice* device, NkMeshSystem* mesh,
                            NkMaterialSystem* mat, NkRenderGraph* graph,
                            NkShadowSystem* shadow,
                            NkEnvironmentSystem* env,
                            NkShaderLibrary* shaderLib,
                            NkResources* resources,
                            uint32 framesInFlight) {
            mDevice=device; mMesh=mesh; mMat=mat; mGraph=graph;
            mShadow=shadow; mEnv=env; mShaderLib=shaderLib; mResources=resources;

            // Clamp [1,3]. Au-dela = 3 c'est gaspillage VRAM sans gain perceptible.
            mFramesInFlight = framesInFlight < 1 ? 1 : (framesInFlight > 3 ? 3 : framesInFlight);
            mFrameSlot      = 0;

            // ── UBOs (matchent le shader pbr.vert/frag.gl.glsl) ──────────────────
            // Une copie par slot du ring : evite que le CPU stalle quand il
            // ecrit un buffer encore lu par le GPU. Avec mFramesInFlight=1 on
            // retombe sur le comportement legacy (1 buffer partage).
            mUBOCameraRing.Resize(mFramesInFlight);
            mUBOLightsRing.Resize(mFramesInFlight);
            mUBOObjectPool.Resize(mFramesInFlight);

            // Camera UBO — binding 0
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mUBOCameraRing[i] = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkCameraUBO)));
            }

            // Lights UBO — binding 2 (32 lights max)
            struct LightsUBO { NkVec4f pos[32],color[32],dir[32],angles[32]; int32 count,_p[3]; };
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mUBOLightsRing[i] = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(LightsUBO)));
            }

            // Object UBO — binding 1. Layout std140 doit matcher EXACTEMENT le shader
            // pbr.vert/frag.gl.glsl (sinon le linker GL refuse, ou les reads out-of-buffer
            // donnent un undefined behavior selon le driver).
            struct ObjectUBO {
                NkMat4f model;            // 0
                NkMat4f normalMatrix;     // 64
                NkVec4f tint;             // 128
                float32 metallic;         // 144
                float32 roughness;        // 148
                float32 aoStrength;       // 152
                float32 emissiveStrength; // 156
                float32 normalStrength;   // 160
                float32 clearcoat;        // 164
                float32 clearcoatRough;   // 168
                float32 subsurface;       // 172
                NkVec4f subsurfaceColor;  // 176 (aligned to 16)
            };                            // total 192
            static_assert(sizeof(ObjectUBO) == 192, "ObjectUBO std140 layout");
            // Phase F.B.1 : pool d'ObjectUBO (frame x drawIdx). Pre-alloue
            // mFramesInFlight * kMaxObjectsPerFrame buffers a Init pour eviter
            // toute allocation dans le hot path et tout vkCmdUpdateBuffer dans
            // un renderPass actif (interdit par Vulkan).
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mUBOObjectPool[i].Resize(kMaxObjectsPerFrame);
                for (uint32 d=0; d<kMaxObjectsPerFrame; d++) {
                    mUBOObjectPool[i][d] = mDevice->CreateBuffer(
                        NkBufferDesc::Uniform(sizeof(ObjectUBO)));
                }
            }

            // Bones SSBO (utilise pour skinning — pas par le shader PBR de base)
            mSSBOBones = mDevice->CreateBuffer(NkBufferDesc::Storage(256*sizeof(NkMat4f)));

            // Phase E.6b : default white cubemap pour les 4 slots de point cookie.
            // 1x1 par face, blanc pur. User override via SetLightCookieCube3D.
            {
                auto td = NkTextureDesc::Cubemap(1, NkGPUFormat::NK_RGBA8_UNORM, 1);
                td.debugName = "DefaultCubeWhite";
                mDefaultCubeWhite = mDevice->CreateTexture(td);
                if (mDefaultCubeWhite.IsValid()) {
                    const uint8_t white[4] = {255, 255, 255, 255};
                    for (uint32 face = 0; face < 6; face++) {
                        mDevice->WriteTextureRegion(mDefaultCubeWhite, white,
                            0, 0, 0, 1, 1, 1, 0, face);
                    }
                }
            }

            // ── Descriptor set layouts ────────────────────────────────────────────
            // Frame set (set 0) : Camera(0) + Lights(2) + Shadow(3) + 4 textures
            // materiel par defaut(4-7) + Env irradiance/prefilter/BRDFLUT(8/9/10)
            // + Shadow map(11). Bindings matchent le shader PBR.
            NkDescriptorSetLayoutDesc frameLayout;
            frameLayout
                .Add(0,  NkDescriptorType::NK_UNIFORM_BUFFER,        ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(2,  NkDescriptorType::NK_UNIFORM_BUFFER,        ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(3,  NkDescriptorType::NK_UNIFORM_BUFFER,        ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(4,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(5,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(6,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(7,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(8,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(9,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(10, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(11, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                // binding=12 : meme shadow atlas qu'au binding 11 mais lue avec
                // un sampler non-compare (sampler2D au lieu de sampler2DShadow)
                // pour le blocker search PCSS.
                .Add(12, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                // Phase E.6 : bindings 13..20 = 8 light cookies (sampler2D)
                .Add(13, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(14, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(15, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(16, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(17, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(18, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(19, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(20, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                // Phase E.6b : bindings 21..24 = 4 point cookies (samplerCube)
                .Add(21, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(22, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(23, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                .Add(24, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
            mGlobalLayout = mDevice->CreateDescriptorSetLayout(frameLayout);

            // Object set layout (set 1) : Object UBO(1)
            NkDescriptorSetLayoutDesc objectLayout;
            objectLayout.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
            mObjectLayout = mDevice->CreateDescriptorSetLayout(objectLayout);

            // ── Allocate descriptor sets ─────────────────────────────────────────
            // Global : 1 par slot du ring (donnees per-frame).
            // Object : 1 par drawcall x frame (pattern UBO-per-draw, Base03/Vulkan).
            //   Chaque set est bind a son UBO du pool a Init (1:1, jamais re-bind).
            mGlobalSetRing.Resize(mFramesInFlight);
            mObjectSetPool.Resize(mFramesInFlight);
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mGlobalSetRing[i] = mDevice->AllocateDescriptorSet(mGlobalLayout);
                mObjectSetPool[i].Resize(kMaxObjectsPerFrame);
                for (uint32 d=0; d<kMaxObjectsPerFrame; d++) {
                    mObjectSetPool[i][d] = mDevice->AllocateDescriptorSet(mObjectLayout);
                }
            }

            // ── Pre-bind static buffers + textures into chaque slot ──────────────
            // Resources statiques (textures defauts, env maps, shadow atlas) : memes
            // valeurs pour tous les slots. UBOs ring : binding sur le buffer du slot.
            NkTextureHandle defAlbedo  = mResources ? mResources->GetWhiteTex()  : NkTextureHandle{};
            NkTextureHandle defNormal  = mResources ? mResources->GetNormalTex() : NkTextureHandle{};
            NkTextureHandle defORM     = mResources ? mResources->GetWhiteTex()  : NkTextureHandle{};
            NkTextureHandle defEmissive= mResources ? mResources->GetBlackTex()  : NkTextureHandle{};
            NkSamplerHandle defSampler = mResources ? mResources->GetSamplerLinearRepeat() : NkSamplerHandle{};

            for (uint32 i=0; i<mFramesInFlight; i++) {
                NkDescSetHandle gs = mGlobalSetRing[i];
                if (gs.IsValid()) {
                    mDevice->BindUniformBuffer(gs, 0, mUBOCameraRing[i]);
                    mDevice->BindUniformBuffer(gs, 2, mUBOLightsRing[i]);

                    // ShadowUBO depuis le ShadowSystem (cascadeCount=0 en mode stub D.3a)
                    if (mShadow && mShadow->GetShadowUBO().IsValid()) {
                        mDevice->BindUniformBuffer(gs, 3, mShadow->GetShadowUBO());
                    }

                    if (defAlbedo.IsValid())   mDevice->BindTextureSampler(gs, 4, defAlbedo,   defSampler);
                    if (defNormal.IsValid())   mDevice->BindTextureSampler(gs, 5, defNormal,   defSampler);
                    if (defORM.IsValid())      mDevice->BindTextureSampler(gs, 6, defORM,      defSampler);
                    if (defEmissive.IsValid()) mDevice->BindTextureSampler(gs, 7, defEmissive, defSampler);

                    if (mEnv) {
                        NkSamplerHandle envSamp = mEnv->GetEnvSampler();
                        NkSamplerHandle lutSamp = mEnv->GetLUTSampler();
                        if (mEnv->GetIrradianceCubemap().IsValid())
                            mDevice->BindTextureSampler(gs, 8, mEnv->GetIrradianceCubemap(), envSamp);
                        if (mEnv->GetPrefilterCubemap().IsValid())
                            mDevice->BindTextureSampler(gs, 9, mEnv->GetPrefilterCubemap(), envSamp);
                        if (mEnv->GetBRDFLUT().IsValid())
                            mDevice->BindTextureSampler(gs, 10, mEnv->GetBRDFLUT(), lutSamp);
                    }

                    if (mShadow && mShadow->GetAtlasTexture().IsValid()) {
                        mDevice->BindTextureSampler(gs, 11,
                            mShadow->GetAtlasTexture(), mShadow->GetAtlasSampler());
                        // Shadow atlas raw read (PCSS blocker search). Meme texture,
                        // sampler different (clamp/linear sans compare-mode).
                        mDevice->BindTextureSampler(gs, 12,
                            mShadow->GetAtlasTexture(), mShadow->GetAtlasRawSampler());
                    }

                    // Phase E.6 : pre-bind les 8 cookies 3D sur white1x1 (no-op).
                    // L'utilisateur remplace via SetLightCookie3D(slot, tex).
                    if (mResources) {
                        NkTextureHandle whiteTex = mResources->GetWhiteTex();
                        NkSamplerHandle whiteSamp = mResources->GetSamplerLinearRepeat();
                        for (uint32 ci = 0; ci < kMaxCookies3D; ci++) {
                            mDevice->BindTextureSampler(gs, 13 + ci, whiteTex, whiteSamp);
                        }
                        // Phase E.6b : pre-bind les 4 cube cookies sur la cubemap
                        // white par defaut (creee une seule fois pour tous les slots).
                        if (mDefaultCubeWhite.IsValid()) {
                            for (uint32 ci = 0; ci < kMaxCookiesCube3D; ci++) {
                                mDevice->BindTextureSampler(gs, 21 + ci,
                                    mDefaultCubeWhite, whiteSamp);
                            }
                        }
                    }
                }
                // Phase F.B.1 : bind chaque set du pool a son UBO du pool (1:1).
                // Bind statique a Init -> au draw on fait juste BindDescriptorSet.
                for (uint32 d=0; d<kMaxObjectsPerFrame; d++) {
                    NkDescSetHandle os = mObjectSetPool[i][d];
                    if (os.IsValid()) {
                        mDevice->BindUniformBuffer(os, 1, mUBOObjectPool[i][d]);
                    }
                }
            }

            // ── Compile shader PBR + cree pipeline ───────────────────────────────
            // LoadOrCompileVF : cherche d'abord Resources/NKRenderer/Shaders/PBR/GL/
            // pour permettre l'override par l'utilisateur, fallback sur les sources
            // embarquees dans NkRender3D_PBRShaders.inl si absent.
            if (mShaderLib) {
                auto progHandle = mShaderLib->LoadOrCompileVF("PBR", kPBR_VS, kPBR_FS);
                if (progHandle.IsValid()) {
                    mPBRShader = mShaderLib->GetRHIHandle(progHandle);
                }
            }

            // Le pipeline PBR est cree paresseusement au 1er FlushOpaque (cf.
            // EnsurePBRPipeline) : Vulkan/DX12 exigent que le pipeline soit
            // RP-compatible avec le fb cible, et le RP de la pass Geometry est
            // construit par le RenderGraph au 1er Execute (apres Init). Creer
            // le pipeline ici avec un fallback swapchain RP genere une
            // incompatibilite format (R16G16B16A16_SFLOAT du HDR target vs
            // B8G8R8A8_SRGB du swapchain) : VUID-vkCmdDraw-renderPass-02684.
            if (!mPBRShader.IsValid()) {
                logger.Errorf("[NkRender3D] PBR shader handle INVALID after LoadOrCompileVF\n");
            }

            // ── Shadow pipeline (D.3b) ───────────────────────────────────────────
            if (mShaderLib) {
                auto progHandle = mShaderLib->LoadOrCompileVF("Shadow", kShadow_VS, kShadow_FS);
                if (progHandle.IsValid()) {
                    mShadowShader = mShaderLib->GetRHIHandle(progHandle);
                }
            }
            if (mShadowShader.IsValid()) {
                NkGraphicsPipelineDesc pd;
                pd.shader       = mShadowShader;
                pd.depthStencil = NkDepthStencilDesc::Default();    // depth write enabled
                // Pipeline Shadow rend dans le shadow atlas (depth-only). En VK
                // le pipeline doit etre cree avec un RP compatible (sinon le
                // fallback swapchain RP color+depth donne un draw incompatible).
                if (mShadow) pd.renderPass = mShadow->GetShadowRenderPass();
                // Shadow casters typiquement render avec front-face culling pour
                // reduire le shadow acne (peter-panning). Mais sans winding fiable
                // sur les meshes primitifs, on garde NoCull.
                pd.rasterizer   = NkRasterizerDesc::NoCull();
                pd.blend        = NkBlendDesc::Opaque();
                pd.debugName    = "Shadow_DepthOnly";
                // Range push_constant ALL_GRAPHICS : permet aux appelants qui
                // pushent avec stage=ALL_GRAPHICS (convention NkShadowSystem /
                // NkRender3D) de respecter le range declare. Le shader Shadow VS
                // est le seul a lire le push_constant en pratique.
                pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f));
                // Layout : [global, object] meme si Shadow VS n'utilise que object.
                // Necessaire pour que le bind a set=1 (convention) reste valide en VK.
                pd.descriptorSetLayouts.PushBack(mGlobalLayout);
                pd.descriptorSetLayouts.PushBack(mObjectLayout);    // reutilise ObjectUBO
                pd.vertexLayout
                  .AddBinding(0, sizeof(NkVertex3D), false)
                  .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0);
                  // Les autres attributs sont ignores par le shader shadow (depth-only).
                mShadowPipeline = mDevice->CreateGraphicsPipeline(pd);
                logger.Info("[NkRender3D] Shadow pipeline create: shader_valid={0} pipeline_valid={1}\n",
                            mShadowShader.IsValid() ? 1 : 0, mShadowPipeline.IsValid() ? 1 : 0);
            }

            // Fournit les layouts partagés au material system afin que ses pipelines
            // soient RP-compatibles et aient la même layout set 0/1 que le PBR pipeline.
            // Le renderPass est inconnu ici (lazy), mis à jour dans Flush() via UpdateRenderPass().
            if (mMat) {
                // NkVertexLayout ici = type RHI (nkentseu::NkVertexLayout),
                // distinct de nkentseu::renderer::NkVertexLayout (NkMeshSystem.h).
                ::nkentseu::NkVertexLayout sharedVL;
                sharedVL.AddBinding(0, sizeof(NkVertex3D), false)
                  .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0,  "POSITION", 0)
                  .AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL",   0)
                  .AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT",  0)
                  .AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT,  36, "TEXCOORD", 0)
                  .AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT,  44, "TEXCOORD", 1)
                  .AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR",    0);
                mMat->SetSharedContext(mGlobalLayout, mObjectLayout, sharedVL);
            }

            bool ringValid = !mUBOCameraRing.Empty() && mUBOCameraRing[0].IsValid();
            logger.Info("[NkRender3D] Init final: ringValid={0} pbrShader.valid={1} (PBR pipeline: lazy create at 1st flush)\n",
                        ringValid ? 1 : 0, mPBRShader.IsValid() ? 1 : 0);
            return ringValid && mPBRShader.IsValid();
        }

        // ── Lazy create du pipeline PBR (Bug fix Vulkan : RP compat) ────────────
        bool NkRender3D::EnsurePBRPipeline(NkRenderPassHandle currentRP) {
            if (!mPBRShader.IsValid()) return false;

            // Si le pipeline existe deja et est compatible avec le RP courant
            // (meme handle, donc meme format/layout), rien a faire. Cas typique :
            // 2eme frame et plus, le fb cache du graph reutilise le meme RP.
            // Si pipeline existe deja, on le reutilise tel quel. Le projet garantit
            // que tous les RPs ou le PBR est dessine partagent les memes formats
            // (HDR R16G16B16A16 + D32_FLOAT) : Vulkan "render pass compatibility"
            // accepte un pipeline cree pour rt_rp utilise dans Geometry_rp et
            // inversement. Detruire+recreer a chaque changement de RP invalide
            // le cmd buffer en cours (vkCmdPipelineBarrier suivants rejetes →
            // EndCapture ne transitionne plus l'image RT → reflet noir).
            if (mPBRPipeline.IsValid()) return true;

            NkGraphicsPipelineDesc pd;
            pd.shader       = mPBRShader;
            pd.depthStencil = NkDepthStencilDesc::Default();   // depth test enabled
            // D.1 : NoCull tant que les meshes primitifs n'ont pas de winding
            // CCW garanti (le plane par exemple a un winding inverse).
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.debugName    = "PBR_Opaque";
            pd.renderPass   = currentRP;
            pd.descriptorSetLayouts.PushBack(mGlobalLayout);
            pd.descriptorSetLayouts.PushBack(mObjectLayout);

            // Vertex layout — NkVertex3D : pos(vec3), normal(vec3), tangent(vec3),
            //   uv(vec2), uv2(vec2), color(uint32 RGBA8)
            //   Stride = 12+12+12+8+8+4 = 56 bytes
            pd.vertexLayout
              .AddBinding(0, sizeof(NkVertex3D), false)
              .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0,  "POSITION", 0)
              .AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL",   0)
              .AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT",  0)
              .AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT,  36, "TEXCOORD", 0)
              .AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT,  44, "TEXCOORD", 1)
              .AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR",    0);

            mPBRPipeline = mDevice->CreateGraphicsPipeline(pd);
            mPBRPipelineRP = currentRP;
            logger.Info("[NkRender3D] PBR pipeline (lazy) create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
                        mPBRShader.IsValid() ? 1 : 0, mPBRPipeline.IsValid() ? 1 : 0,
                        currentRP.id);
            return mPBRPipeline.IsValid();
        }

        void NkRender3D::Shutdown() {
            if (mShadowPipeline.IsValid()) { mDevice->DestroyPipeline(mShadowPipeline); mShadowPipeline={}; }
            if (mPBRPipeline.IsValid()) { mDevice->DestroyPipeline(mPBRPipeline); mPBRPipeline={}; }
            // Les shader handles sont detenus par NkShaderLibrary, pas a detruire ici.
            for (auto& s : mGlobalSetRing) if (s.IsValid()) mDevice->FreeDescriptorSet(s);
            // Phase F.B.1 : free le pool object 2D (frame x drawIdx).
            for (auto& perFrame : mObjectSetPool) {
                for (auto& s : perFrame) if (s.IsValid()) mDevice->FreeDescriptorSet(s);
                perFrame.Clear();
            }
            mGlobalSetRing.Clear();
            mObjectSetPool.Clear();
            if (mGlobalLayout.IsValid()) { mDevice->DestroyDescriptorSetLayout(mGlobalLayout); }
            if (mObjectLayout.IsValid()) { mDevice->DestroyDescriptorSetLayout(mObjectLayout); }
            for (auto& b : mUBOCameraRing) if (b.IsValid()) mDevice->DestroyBuffer(b);
            for (auto& perFrame : mUBOObjectPool) {
                for (auto& b : perFrame) if (b.IsValid()) mDevice->DestroyBuffer(b);
                perFrame.Clear();
            }
            for (auto& b : mUBOLightsRing) if (b.IsValid()) mDevice->DestroyBuffer(b);
            mUBOCameraRing.Clear();
            mUBOObjectPool.Clear();
            mUBOLightsRing.Clear();
            if(mSSBOBones.IsValid()){mDevice->DestroyBuffer(mSSBOBones);mSSBOBones={};}
            if(mDefaultCubeWhite.IsValid()){mDevice->DestroyTexture(mDefaultCubeWhite);mDefaultCubeWhite={};}

            // DEBUG triangle resources
            if (mDebugPipeline.IsValid()) { mDevice->DestroyPipeline(mDebugPipeline); mDebugPipeline={}; }
            if (mDebugVBO.IsValid())      { mDevice->DestroyBuffer(mDebugVBO); mDebugVBO={}; }
            if (mDebugIBO.IsValid())      { mDevice->DestroyBuffer(mDebugIBO); mDebugIBO={}; }
            mDebugInited = false;
        }

        // ── Scene ─────────────────────────────────────────────────────────────────
        void NkRender3D::ResetFrame() {
            // Appelee une seule fois par frame depuis NkRendererImpl::BeginFrame.
            // Reset l'index du pool d'UBO objets pour la nouvelle frame.
            // Doit NE PAS etre fait dans BeginScene : si la frame a deux passes
            // (ex: passe miroir + passe principale), reset entre les deux ecrase
            // les UBOs de la 1ere passe au moment du Execute() differe (backend GL).
            mObjectDrawIdx = 0;
        }

        void NkRender3D::BeginScene(const NkSceneContext& ctx) {
            mCtx = ctx;
            mInScene = true;
            mOpaque.Clear(); mTransparent.Clear();
            mInstanced.Clear(); mSkinned.Clear();
            // mObjectDrawIdx N'EST PAS reset ici — voir ResetFrame() ci-dessus.
        }

        // ── Submit ────────────────────────────────────────────────────────────────
        void NkRender3D::Submit(const NkDrawCall3D& dc) {
            if (!dc.visible) return;
            if (!mCtx.camera.IsAABBVisible(dc.aabb)) return;

            NkVec3f camPos = mCtx.camera.GetPosition();
            NkVec3f center = dc.aabb.Center();
            float32 dx=center.x-camPos.x, dy=center.y-camPos.y, dz=center.z-camPos.z;
            float32 depth = dx*dx + dy*dy + dz*dz;
            mOpaque.PushBack({dc, depth});
        }

        void NkRender3D::SubmitMany(const NkDrawCall3D* dcs, uint32 count) {
            for (uint32 i=0; i<count; i++) Submit(dcs[i]);
        }

        void NkRender3D::SubmitInstanced(const NkDrawCallInstanced& dc) {
            mInstanced.PushBack(dc);
        }

        void NkRender3D::SubmitSkinned(const NkDrawCallSkinned& dc) {
            mSkinned.PushBack(dc);
        }

        void NkRender3D::SubmitSkinnedTinted(const NkDrawCallSkinned& dc,
                                            NkVec3f tint, float32 alpha) {
            NkDrawCallSkinned copy = dc;
            copy.tint  = tint;
            copy.alpha = alpha;
            mSkinned.PushBack(copy);
        }

        // ── Sort ──────────────────────────────────────────────────────────────────
        void NkRender3D::SortDrawCalls() {
            for (uint32 i=1;i<(uint32)mOpaque.Size();i++) {
                SortedDC key=mOpaque[i];
                int32 j=(int32)i-1;
                while(j>=0 && mOpaque[j].depth>key.depth){
                    mOpaque[j+1]=mOpaque[j]; j--;
                }
                mOpaque[j+1]=key;
            }
        }

        // ── Flush ────────────────────────────────────────────────────────────────
        void NkRender3D::Flush(NkICommandBuffer* cmd) {
            if (!mInScene) return;
            SortDrawCalls();
            UploadUBOs(cmd);

            // Lazy create du pipeline PBR maintenant qu'on est dans la pass
            // Geometry (RP cible connu via le RenderGraph). Sur OpenGL, le RP
            // retourne {} : pas grave, le backend l'ignore. Sur Vulkan/DX12 il
            // permet la creation d'un pipeline RP-compatible.
            NkRenderPassHandle currentRP{};
            if (mGraph) currentRP = mGraph->GetPassRenderPass("Geometry");
            // Override par la passe RT si fournie (planar reflection) : permet
            // de compiler les pipelines materiaux pour le RP du RT avant que
            // Geometry FB existe (1re frame). Si rt_rp et Geometry_rp ont memes
            // formats, les pipelines restent valides pour les deux passes.
            if (!currentRP.IsValid() && mPendingRP.IsValid())
                currentRP = mPendingRP;

            // ── DEBUG triangle minimal (isolation bug PBR Vulkan) ────────────
            // Mode 1 = non-indexed Draw(3). Mode 2 = indexed DrawIndexed(3).
            // Si visible -> pipeline VK basique fonctionne, ajouter UBO/sets.
            if constexpr (kDebugTriangleMode != 0) {
                if (EnsureDebugTriangle(currentRP)) {
                    if constexpr (kDebugTriangleMode == 1) DebugDrawTriangleNoIdx(cmd);
                    else                                   DebugDrawTriangleIdx  (cmd);
                }
                mInScene = false;
                mFrameSlot = (mFrameSlot + 1) % mFramesInFlight;
                return;
            }

            EnsurePBRPipeline(currentRP);

            // Notifie le material system du RP courant (Vulkan compat).
            // UpdateRenderPass invalide les pipelines material si le RP a change
            // (ex: resize swapchain). Idempotent si le RP est identique.
            if (mMat) mMat->UpdateRenderPass(currentRP);

            // Le pipeline est desormais lie par draw dans FlushOpaque (multi-materiau).
            // On bind d'abord le pipeline PBR par defaut pour les draws sans materiau.
            // BindGraphicsPipeline doit preceder BindDescriptorSet sur OpenGL (change le VAO).
            if (mPBRPipeline.IsValid())
                cmd->BindGraphicsPipeline(mPBRPipeline);

            // Bind per-frame descriptor set du slot courant (camera + lights + shadow
            // + env + textures defaut). Chaque slot du ring est pre-bound a son UBO,
            // donc juste un BindDescriptorSet suffit, pas de UpdateDescriptorSets.
            NkDescSetHandle gs = (mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
            if (gs.IsValid())
                cmd->BindDescriptorSet(gs, 0);

            FlushOpaque(cmd);
            FlushInstanced(cmd);
            FlushSkinned(cmd);
            FlushTransparent(cmd);
            FlushDebug(cmd);
            mInScene=false;

            // Avance au slot suivant pour la prochaine frame.
            // Avec mFramesInFlight=1 on reste sur le slot 0 (pas de ring).
            mFrameSlot = (mFrameSlot + 1) % mFramesInFlight;
        }

        void NkRender3D::Flush(NkICommandBuffer* cmd, NkRenderPassHandle renderPass) {
            // Render-to-texture : expose le RP du RT a Flush(cmd) via mPendingRP.
            // Il sera utilise UNIQUEMENT si le Geometry RP du RenderGraph n'est
            // pas encore disponible (FB lazy, 1re frame). Le RT doit avoir des
            // formats compatibles avec Geometry pass (HDR R16G16B16A16 + D32_FLOAT)
            // pour que le pipeline reste utilisable a la 2e passe sans recompile.
            mPendingRP = renderPass;
            Flush(cmd);
            mPendingRP = {};
        }

        void NkRender3D::UploadUBOs(NkICommandBuffer* cmd) {
            (void)cmd;
            // Camera UBO — layout std140 qui matche EXACTEMENT le shader PBR (binding=0).
            // NB : NkCameraUBO (NkRendererTypes.h) a une layout differente (avec invView/
            // invProj separes) ; on ne peut pas l'utiliser directement.
            struct PBRCamUBO {
                NkMat4f view;
                NkMat4f proj;
                NkMat4f viewProj;
                NkMat4f invViewProj;
                NkVec4f camPos;       // .xyz = pos, .w = near
                NkVec4f camDir;       // .xyz = forward, .w = far
                float32 viewportX, viewportY;
                float32 time, deltaTime;
                float32 iblStrength, _p0, _p1, _p2;  // offset 304 : force IBL ambient
                // Phase Planar Reflection : viewProj de la cam miroir, exposée
                // au shader ReflFloor pour calculer reflectionUV via
                // projection explicite. Les shaders qui n'utilisent pas ce
                // champ ignorent simplement les bytes après leur fin de struct.
                NkMat4f mirrorViewProj;  // offset 320 (apres 16 bytes de padding)
            };
            PBRCamUBO cb{};
            cb.view        = mCtx.camera.GetView();
            cb.proj        = mCtx.camera.GetProj();
            cb.viewProj    = mCtx.camera.GetViewProj();

            // Correction Vulkan clip-space : la projection NkCamera produit
            // un NDC Z dans [-1, 1] (convention OpenGL). Vulkan attend [0, 1] —
            // sans correction, la moitie pres de la camera est silencieusement
            // clippee (Z<0) -> ecran noir. On compose vkClip * proj pour passer
            // de [-1,1] a [0,1] : z_new = 0.5*z + 0.5*w. NkMat4f est column-major,
            // donc m[2][2]=0.5 (m22) et m[3][2]=0.5 (m23, translation Z).
            if (mDevice && mDevice->GetApi() == ::nkentseu::NkGraphicsApi::NK_GFX_API_VULKAN) {
                NkMat4f vkClip = NkMat4f::Identity();
                vkClip[2][2] = 0.5f;
                vkClip[3][2] = 0.5f;
                cb.proj     = vkClip * cb.proj;
                cb.viewProj = vkClip * cb.viewProj;
            }
            cb.invViewProj = cb.viewProj.Inverse();
            NkVec3f pos = mCtx.camera.GetPosition();
            NkVec3f fwd = mCtx.camera.GetForward();
            cb.camPos    = {pos.x, pos.y, pos.z, mCtx.camera.GetNear()};
            cb.camDir    = {fwd.x, fwd.y, fwd.z, mCtx.camera.GetFar()};
            cb.viewportX   = (float32)mW;
            cb.viewportY   = (float32)mH;
            cb.time        = mCtx.time;
            cb.deltaTime   = mCtx.deltaTime;
            cb.iblStrength = mIBLStrength;
            // Phase Planar Reflection : applique aussi la correction Vulkan
            // clip-space à mirrorViewProj (sinon le sampling du RT serait clippé
            // ou inversé sur Vulkan/DX). Identity en l'absence de reflet planaire.
            cb.mirrorViewProj = mCtx.mirrorViewProj;
            if (mDevice && mDevice->GetApi() == ::nkentseu::NkGraphicsApi::NK_GFX_API_VULKAN) {
                NkMat4f vkClip = NkMat4f::Identity();
                vkClip[2][2] = 0.5f;
                vkClip[3][2] = 0.5f;
                cb.mirrorViewProj = vkClip * cb.mirrorViewProj;
            }
            // Ring : on ecrit dans le buffer du slot courant. Le GPU lit (au pire)
            // celui du slot N-1, donc pas de stall.
            if (mFrameSlot < mUBOCameraRing.Size())
                mDevice->WriteBuffer(mUBOCameraRing[mFrameSlot], &cb, sizeof(cb));

            // Lights UBO
            struct LightsBlock {
                NkVec4f pos[32],color[32],dir[32],angles[32]; int32 count,_p[3];
            } lb{};
            lb.count=(int32)mCtx.lights.Size();
            for(int32 i=0;i<lb.count&&i<32;i++){
                auto& l=mCtx.lights[i];
                lb.pos[i]   ={l.position.x,l.position.y,l.position.z,(float32)l.type};
                lb.color[i] ={l.color.x,l.color.y,l.color.z,l.intensity};
                lb.dir[i]   ={l.direction.x,l.direction.y,l.direction.z,l.range};
                // angles : .x = cos(inner), .y = cos(outer) (precompute pour le shader),
                // .z = castShadow flag, .w = cookieIdx (-1 = pas de cookie).
                const float deg2rad = 3.14159265f / 180.f;
                lb.angles[i]={std::cos(l.innerAngle * deg2rad),
                              std::cos(l.outerAngle * deg2rad),
                              (float32)l.castShadow,
                              (float32)l.cookieIdx};
            }
            if (mFrameSlot < mUBOLightsRing.Size())
                mDevice->WriteBuffer(mUBOLightsRing[mFrameSlot], &lb, sizeof(lb));
        }

        void NkRender3D::RenderShadowPass(NkICommandBuffer* cmd, const NkMat4f& lightVP) {
            if (!cmd || !mShadowPipeline.IsValid()) return;

            cmd->BindGraphicsPipeline(mShadowPipeline);
            // lightVP en push constant (4 vec4 = 64 bytes)
            cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f), &lightVP);

            // Re-uploaded ObjectUBO + draw mesh pour chaque opaque qui caste.
            // Meme layout ObjBlock que dans FlushOpaque (le linker GLSL exige une
            // declaration std140 identique entre les deux shaders qui partagent
            // l'UBO binding 1).
            struct ObjBlock {
                NkMat4f model;
                NkMat4f normalMatrix;
                NkVec4f tint;
                float32 metallic;
                float32 roughness;
                float32 aoStrength;
                float32 emissiveStrength;
                float32 normalStrength;
                float32 clearcoat;
                float32 clearcoatRough;
                float32 subsurface;
                NkVec4f subsurfaceColor;
            };

            // Phase F.B.1 : pattern UBO-per-draw. Chaque drawcall consume un slot
            // du pool (UBO + descriptor set pre-bind 1:1). WriteBuffer fait un
            // memcpy via mapped pointer (UBO HOST_VISIBLE), legal dans renderPass
            // actif contrairement a cmd->UpdateBuffer = vkCmdUpdateBuffer
            // (VUID-vkCmdUpdateBuffer-renderpass).
            const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size())
                                     && (mFrameSlot < mObjectSetPool.Size());
            if (!poolFrameValid) return;

            for (auto& sdc : mOpaque) {
                auto& dc = sdc.dc;
                if (!dc.castShadow) continue;
                if (mObjectDrawIdx >= kMaxObjectsPerFrame) {
                    logger.Errorf("[NkRender3D] ObjectUBO pool overflow (shadow): "
                                  "drawIdx={0} >= max={1}, skipping draw\n",
                                  mObjectDrawIdx, kMaxObjectsPerFrame);
                    break;
                }
                ObjBlock ob{};
                ob.model            = dc.transform;
                ob.normalMatrix     = dc.transform.Inverse().Transpose();
                ob.tint             = {1, 1, 1, 1};
                ob.metallic = 0.f; ob.roughness = 0.5f; ob.aoStrength = 1.f;

                NkBufferHandle  ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
                NkDescSetHandle os  = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                if (ubo.IsValid()) mDevice->WriteBuffer(ubo, &ob, sizeof(ob), 0);
                if (os.IsValid())  cmd->BindDescriptorSet(os, 1);
                mMesh->BindMesh(cmd, dc.mesh);
                if (dc.subMeshIdx == 0xFFFFFFFFu)
                    mMesh->DrawAll(cmd, dc.mesh);
                else
                    mMesh->DrawSubMesh(cmd, dc.mesh, dc.subMeshIdx);
                mObjectDrawIdx++;
            }
        }

        void NkRender3D::FlushOpaque(NkICommandBuffer* cmd) {
            // Doit matcher EXACTEMENT la layout std140 du shader pbr.{vert,frag}.gl.glsl.
            struct ObjBlock {
                NkMat4f model;
                NkMat4f normalMatrix;
                NkVec4f tint;
                float32 metallic;
                float32 roughness;
                float32 aoStrength;
                float32 emissiveStrength;
                float32 normalStrength;
                float32 clearcoat;
                float32 clearcoatRough;
                float32 subsurface;
                NkVec4f subsurfaceColor;
            };
            static_assert(sizeof(ObjBlock) == 192, "ObjBlock std140");

            // Phase F.B.1 : pattern UBO-per-draw. mObjectDrawIdx est deja
            // incremente par RenderShadowPass (qui tourne AVANT cette passe via
            // NkShadowSystem). On continue a partir de la pour ne pas ecraser les
            // UBOs ombres encore en flight cote GPU.
            const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size())
                                     && (mFrameSlot < mObjectSetPool.Size());
            if (!poolFrameValid) return;

            // Multi-material : on change de pipeline uniquement si le draw courant
            // utilise un materiau different du precedent. Le pipeline PBR est le
            // fallback (deja lie dans Flush() avant FlushOpaque).
            NkPipelineHandle lastPipeline = mPBRPipeline;

            for (auto& sdc : mOpaque) {
                auto& dc = sdc.dc;
                if (mObjectDrawIdx >= kMaxObjectsPerFrame) {
                    logger.Errorf("[NkRender3D] ObjectUBO pool overflow (opaque): "
                                  "drawIdx={0} >= max={1}, skipping draw\n",
                                  mObjectDrawIdx, kMaxObjectsPerFrame);
                    break;
                }

                // Determine pipeline + instance materiau.
                NkMaterialInstance* matInst = nullptr;
                NkPipelineHandle    pipeline = mPBRPipeline;  // fallback PBR

                if (dc.material.IsValid() && mMat) {
                    matInst = mMat->GetInstance(dc.material);
                    if (matInst) {
                        NkPipelineHandle matPipeline = mMat->GetPipeline(matInst->GetTemplate());
                        if (matPipeline.IsValid()) pipeline = matPipeline;
                    }
                }

                // Bind pipeline seulement si change (evite le cout vkCmdBindPipeline redondant).
                if (pipeline != lastPipeline) {
                    if (pipeline.IsValid()) cmd->BindGraphicsPipeline(pipeline);
                    lastPipeline = pipeline;
                }

                ObjBlock ob{};
                ob.model            = dc.transform;
                ob.normalMatrix     = dc.transform.Inverse().Transpose();
                ob.tint             = {dc.tint.x, dc.tint.y, dc.tint.z, dc.alpha};
                ob.metallic         = dc.metallic;
                ob.roughness        = dc.roughness;
                ob.aoStrength       = dc.aoStrength;
                ob.emissiveStrength = 0.f;
                ob.normalStrength   = 1.f;
                // clearcoat / subsurface : 0 par defaut (zero-init via ObjBlock{}).

                NkBufferHandle  ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
                NkDescSetHandle os  = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                if (ubo.IsValid()) mDevice->WriteBuffer(ubo, &ob, sizeof(ob), 0);
                if (os.IsValid())  cmd->BindDescriptorSet(os, 1);

                // Bind material instance (set 2) si presente.
                // Charge le UBO PBR/Toon et les textures de l'instance.
                if (matInst) mMat->BindInstance(cmd, matInst);

                mMesh->BindMesh(cmd, dc.mesh);
                if (dc.subMeshIdx == 0xFFFFFFFFu)
                    mMesh->DrawAll(cmd, dc.mesh);
                else
                    mMesh->DrawSubMesh(cmd, dc.mesh, dc.subMeshIdx);
                mObjectDrawIdx++;
            }
        }

        void NkRender3D::FlushTransparent(NkICommandBuffer* cmd) {
            for (auto& sdc : mTransparent) {
                mMesh->BindMesh(cmd, sdc.dc.mesh);
                mMesh->DrawAll(cmd, sdc.dc.mesh);
            }
        }

        void NkRender3D::FlushInstanced(NkICommandBuffer* cmd) {
            // Instanced ne touche pas a ObjectUBO (les transforms passent via
            // SSBOBones). On bind quand meme un set du pool pour satisfaire le
            // pipeline layout (set=1 attendu) ; le contenu de ce slot est ignore
            // par le shader instanced.
            const bool poolFrameValid = (mFrameSlot < mObjectSetPool.Size());
            for (auto& dc : mInstanced) {
                if (dc.transforms.Empty()) continue;
                uint32 count=(uint32)dc.transforms.Size();
                mDevice->WriteBuffer(mSSBOBones, dc.transforms.Data(),
                                        count*sizeof(NkMat4f));
                if (poolFrameValid && mObjectDrawIdx < kMaxObjectsPerFrame) {
                    NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                    if (os.IsValid()) cmd->BindDescriptorSet(os, 1);
                    mObjectDrawIdx++;
                }
                mMesh->BindMesh(cmd, dc.mesh);
                mMesh->DrawAll(cmd, dc.mesh, count);
            }
        }

        void NkRender3D::FlushSkinned(NkICommandBuffer* cmd) {
            const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size())
                                     && (mFrameSlot < mObjectSetPool.Size());
            if (!poolFrameValid) return;
            for (auto& dc : mSkinned) {
                if (dc.boneMatrices.Empty()) continue;
                if (mObjectDrawIdx >= kMaxObjectsPerFrame) {
                    logger.Errorf("[NkRender3D] ObjectUBO pool overflow (skinned): "
                                  "drawIdx={0} >= max={1}, skipping draw\n",
                                  mObjectDrawIdx, kMaxObjectsPerFrame);
                    break;
                }
                uint32 count=(uint32)dc.boneMatrices.Size();
                mDevice->WriteBuffer(mSSBOBones, dc.boneMatrices.Data(),
                                        count*sizeof(NkMat4f));

                struct ObjB { NkMat4f m,nm; NkVec4f tint; float32 p[8]; } ob{};
                ob.m=dc.transform; ob.nm=dc.transform.Inverse().Transpose();
                ob.tint={dc.tint.x,dc.tint.y,dc.tint.z,dc.alpha};

                NkBufferHandle  ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
                NkDescSetHandle os  = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                if (ubo.IsValid()) mDevice->WriteBuffer(ubo,&ob,sizeof(ob));
                if (os.IsValid()) cmd->BindDescriptorSet(os, 1);
                mMesh->BindMesh(cmd,dc.mesh);
                mMesh->DrawAll(cmd,dc.mesh);
                mObjectDrawIdx++;
            }
        }

        void NkRender3D::FlushDebug(NkICommandBuffer* cmd) {
            (void)cmd;
            for (uint32 i=0;i<(uint32)mDebugLines.Size();) {
                if (mDebugLines[i].life > 0.f) {
                    mDebugLines[i].life -= mCtx.deltaTime;
                    if (mDebugLines[i].life <= 0.f) { mDebugLines.RemoveAt(i); continue; }
                } else {
                    mDebugLines.RemoveAt(i); continue;
                }
                i++;
            }
        }

        // ── Phase E.6 : Light cookies 3D ─────────────────────────────────────────
        void NkRender3D::SetLightCookie3D(uint32 slot, NkTextureHandle tex) {
            if (slot >= kMaxCookies3D || !mResources) return;
            NkTextureHandle bind = tex.IsValid() ? tex : mResources->GetWhiteTex();
            NkSamplerHandle samp = mResources->GetSamplerLinearRepeat();
            // Bind sur tous les slots du ring (chaque slot a son descriptor set).
            for (uint32 i = 0; i < mFramesInFlight; i++) {
                if (mGlobalSetRing[i].IsValid()) {
                    mDevice->BindTextureSampler(mGlobalSetRing[i], 13 + slot, bind, samp);
                }
            }
        }

        // ── Phase E.6b : Light cube cookies ──────────────────────────────────────
        void NkRender3D::SetLightCookieCube3D(uint32 slot, NkTextureHandle cubeTex) {
            if (slot >= kMaxCookiesCube3D || !mResources) return;
            NkTextureHandle bind = cubeTex.IsValid() ? cubeTex : mDefaultCubeWhite;
            NkSamplerHandle samp = mResources->GetSamplerLinearRepeat();
            for (uint32 i = 0; i < mFramesInFlight; i++) {
                if (mGlobalSetRing[i].IsValid()) {
                    mDevice->BindTextureSampler(mGlobalSetRing[i], 21 + slot, bind, samp);
                }
            }
        }

        // ── Debug gizmos ─────────────────────────────────────────────────────────
        void NkRender3D::DrawDebugLine(NkVec3f a, NkVec3f b, NkVec4f color, float32 life) {
            mDebugLines.PushBack({a,b,color,life>0?life:0.016f});
        }
        void NkRender3D::DrawDebugSphere(NkVec3f c, float32 r, NkVec4f color) {
            const int N=16;
            for(int i=0;i<N;i++){
                float32 a0=2*3.14159f*i/N, a1=2*3.14159f*(i+1)/N;
                DrawDebugLine({c.x+cosf(a0)*r,c.y+sinf(a0)*r,c.z},
                            {c.x+cosf(a1)*r,c.y+sinf(a1)*r,c.z},color);
            }
        }
        void NkRender3D::DrawDebugAABB(const NkAABB& box, NkVec4f color) {
            NkVec3f mn=box.min,mx=box.max;
            DrawDebugLine({mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},color);
            DrawDebugLine({mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},color);
            DrawDebugLine({mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},color);
            DrawDebugLine({mn.x,mx.y,mn.z},{mn.x,mn.y,mn.z},color);
            DrawDebugLine({mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},color);
            DrawDebugLine({mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},color);
            DrawDebugLine({mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z},color);
            DrawDebugLine({mn.x,mx.y,mx.z},{mn.x,mn.y,mx.z},color);
            DrawDebugLine({mn.x,mn.y,mn.z},{mn.x,mn.y,mx.z},color);
            DrawDebugLine({mx.x,mn.y,mn.z},{mx.x,mn.y,mx.z},color);
            DrawDebugLine({mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},color);
            DrawDebugLine({mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},color);
        }
        void NkRender3D::DrawDebugAxes(const NkMat4f& t, float32 s) {
            NkVec3f orig={t[3][0],t[3][1],t[3][2]};
            DrawDebugLine(orig,{orig.x+t[0][0]*s,orig.y+t[0][1]*s,orig.z+t[0][2]*s},{1,0,0,1});
            DrawDebugLine(orig,{orig.x+t[1][0]*s,orig.y+t[1][1]*s,orig.z+t[1][2]*s},{0,1,0,1});
            DrawDebugLine(orig,{orig.x+t[2][0]*s,orig.y+t[2][1]*s,orig.z+t[2][2]*s},{0,0,1,1});
        }
        void NkRender3D::DrawDebugGrid(NkVec3f o, float32 sp, int32 lines, NkVec4f color) {
            float32 ext=sp*lines*0.5f;
            for(int32 i=-lines/2;i<=lines/2;i++){
                float32 f=(float32)i*sp;
                DrawDebugLine({o.x+f,o.y,o.z-ext},{o.x+f,o.y,o.z+ext},color);
                DrawDebugLine({o.x-ext,o.y,o.z+f},{o.x+ext,o.y,o.z+f},color);
            }
        }
        void NkRender3D::DrawDebugArrow(NkVec3f from, NkVec3f to, NkVec4f color) {
            DrawDebugLine(from,to,color);
        }

        // =====================================================================
        // DEBUG : triangle minimal Vulkan (isolation bug PBR)
        // ----------------------------------------------------------------------
        // Pipeline le plus simple possible :
        //   - Vertex layout = 1 binding, 1 attribut (vec3 position) = 12 bytes
        //   - Aucun descriptor set, aucun UBO, aucun sampler
        //   - Shader VS qui ecrit gl_Position = vec4(aPos.xy, 0.5, 1.0)
        //   - Shader FS qui ecrit fragColor = vec4(vColor, 1.0) (degrade)
        //   - 3 vertices NDC : grand triangle qui couvre l'ecran
        //
        // Si CE pipeline ne dessine pas en VK, le bug est structurel (RP, FB,
        // pipeline state, vertex format, ou compilation glslang).
        // S'il dessine, on ajoute graduellement (UBO -> 2 sets -> NkVertex3D)
        // jusqu'a reproduire le bug PBR.
        // =====================================================================
        bool NkRender3D::EnsureDebugTriangle(NkRenderPassHandle currentRP) {
            if (mDebugInited && mDebugPipelineRP == currentRP) return mDebugPipeline.IsValid();

            // 1. Compile shader debug (VK : Resources/.../DebugTri/VK/debugtri.*)
            //    LoadOrCompileVF retourne un handle Renderer-side (cache lookup),
            //    PAS le RHI shader handle attendu par CreateGraphicsPipeline.
            //    Il faut convertir via GetRHIHandle (cf. mPBRShader plus haut).
            if (!mDebugShader.IsValid()) {
                if (!mShaderLib) return false;
                auto progHandle = mShaderLib->LoadOrCompileVF("DebugTri", "", "");
                if (!progHandle.IsValid()) {
                    logger.Errorf("[NkR3D::DebugTriangle] shader compile FAIL\n");
                    return false;
                }
                mDebugShader = mShaderLib->GetRHIHandle(progHandle);
                if (!mDebugShader.IsValid()) {
                    logger.Errorf("[NkR3D::DebugTriangle] shader RHI handle FAIL (prog id={0})\n", progHandle.id);
                    return false;
                }
            }

            // 2. VBO 3 vertices NDC. Grand triangle qui couvre tout l'ecran :
            //    - bottom-left   (-0.9, -0.9, 0)
            //    - bottom-right  ( 0.9, -0.9, 0)
            //    - top-center    ( 0.0,  0.9, 0)
            if (!mDebugVBO.IsValid()) {
                struct V { float x, y, z; };
                V verts[3] = {
                    { -0.9f, -0.9f, 0.f },
                    {  0.9f, -0.9f, 0.f },
                    {  0.0f,  0.9f, 0.f },
                };
                mDebugVBO = mDevice->CreateBuffer(NkBufferDesc::Vertex(sizeof(verts), verts));
            }

            // 3. IBO 3 indices = 0,1,2 (test indexed path)
            if (!mDebugIBO.IsValid()) {
                uint32 idx[3] = { 0, 1, 2 };
                mDebugIBO = mDevice->CreateBuffer(NkBufferDesc::Index(sizeof(idx), idx));
            }

            // 4. Pipeline. Aucun descriptor set, vertex layout 12-byte vec3.
            if (mDebugPipeline.IsValid()) {
                mDevice->DestroyPipeline(mDebugPipeline);
                mDebugPipeline = {};
            }
            NkGraphicsPipelineDesc pd;
            pd.shader       = mDebugShader;
            // pd.depthStencil = NkDepthStencilDesc::Default();
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.debugName    = "DebugTriangle";
            pd.renderPass   = currentRP;
            pd.vertexLayout
              .AddBinding(0, sizeof(float)*3, false)
              .AddAttribute(0, 0, ::nkentseu::NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0);
            mDebugPipeline   = mDevice->CreateGraphicsPipeline(pd);
            mDebugPipelineRP = currentRP;
            mDebugInited     = true;
            logger.Info("[NkR3D::DebugTriangle] pipeline create: shader.valid={0} pipe.valid={1} rp.id={2}\n",
                        mDebugShader.IsValid()?1:0, mDebugPipeline.IsValid()?1:0, currentRP.id);
            return mDebugPipeline.IsValid();
        }

        void NkRender3D::DebugDrawTriangleNoIdx(NkICommandBuffer* cmd) {
            if (!mDebugPipeline.IsValid() || !mDebugVBO.IsValid()) return;
            cmd->BindGraphicsPipeline(mDebugPipeline);
            cmd->BindVertexBuffer(0, mDebugVBO, 0);
            cmd->Draw(3);
        }

        void NkRender3D::DebugDrawTriangleIdx(NkICommandBuffer* cmd) {
            if (!mDebugPipeline.IsValid() || !mDebugVBO.IsValid() || !mDebugIBO.IsValid()) return;
            cmd->BindGraphicsPipeline(mDebugPipeline);
            cmd->BindVertexBuffer(0, mDebugVBO, 0);
            cmd->BindIndexBuffer(mDebugIBO, NkIndexFormat::NK_UINT32, 0);
            cmd->DrawIndexed(3, 1, 0, 0, 0);
        }

        // ────────────────────────────────────────────────────────────────────
        // DEBUG : dessin direct dans swapchain (bypass Geometry pass)
        // ────────────────────────────────────────────────────────────────────
        void NkRender3D::DebugDrawDirectSwapchain(NkICommandBuffer* cmd) {
            // EnsureDebugTriangle avec rp=invalid -> CreateGraphicsPipeline
            // fallback sur swapchain RP. Le pipeline est recree si on avait
            // deja un pipeline pour rp Geometry (1191).
            NkRenderPassHandle rpInvalid{};
            if (!EnsureDebugTriangle(rpInvalid)) return;
            DebugDrawTriangleNoIdx(cmd);
        }

    } // namespace renderer
} // namespace nkentseu
