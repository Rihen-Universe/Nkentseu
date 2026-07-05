// =============================================================================
// NkRender3D.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkRender3D.h"
#include "../Shadow/NkShadowSystem.h"
#include "../Shadow/NkVirtualShadowMaps.h"
#include "../Environment/NkEnvironmentSystem.h"
#include "../VoxelAO/NkVoxelAOSystem.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKRenderer/Core/NkResources.h"
#include "NKRenderer/Core/NkRendererConfig.h"   // NkUnits() pour triplanar
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NkRender3D_PBRShaders.inl"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <algorithm>

namespace nkentseu {
    namespace renderer {

        NkRender3D::~NkRender3D() { Shutdown(); }

        bool NkRender3D::Init(NkIDevice* device, NkMeshSystem* mesh,
                            NkMaterialSystem* mat, NkRenderGraph* graph,
                            NkVirtualShadowMaps* shadow,
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
            mUBOCameraMirrorRing.Resize(mFramesInFlight);  // Phase Planar Reflection fix
            mUBOLightsRing.Resize(mFramesInFlight);
            mUBOObjectPool.Resize(mFramesInFlight);

            // Camera UBO — binding 0 (main + mirror dedicated)
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mUBOCameraRing[i]       = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkCameraUBO)));
                mUBOCameraMirrorRing[i] = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkCameraUBO)));
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
                // NkVSM v1 : shadowOverrides (.x receiveShadow, .z shadowBiasMul)
                // Doit matcher la struct ObjBlock locale dans FlushOpaque +
                // RenderShadowPass (sinon WriteBuffer overflow le buffer pool
                // -> GL_INVALID_VALUE silent + rien ne s'affiche).
                NkVec4f shadowOverrides;  // 192
                // 2026-05-24 Triplanar : .x = tileSize en metres (0=disabled),
                // .y = metersPerUnit (echelle globale, copie de NkUnits()),
                // .z = enable flag (0/1 redondant avec .x>0 mais explicite cote
                // shader), .w = reserve. Doit matcher ObjBlock 3 endroits +
                // ObjectUBO dans VS+FS (regle GLSL shared block).
                NkVec4f triplanarParams;  // 208
            };                            // total 224
            static_assert(sizeof(ObjectUBO) == 224, "ObjectUBO std140 layout");
            // Phase F.B.1 : pool d'ObjectUBO (frame x drawIdx). Pre-alloue
            // mFramesInFlight * mObjectPoolCap buffers a Init pour eviter
            // toute allocation dans le hot path et tout vkCmdUpdateBuffer dans
            // un renderPass actif (interdit par Vulkan).
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mUBOObjectPool[i].Resize(mObjectPoolCap);
                for (uint32 d=0; d<mObjectPoolCap; d++) {
                    mUBOObjectPool[i][d] = mDevice->CreateBuffer(
                        NkBufferDesc::Uniform(sizeof(ObjectUBO)));
                }
            }

            // Bones UBO (skinning GPU). Ring : 1 uniform buffer par
            // frame-in-flight pour eviter la course CPU(write frame N+1)/
            // GPU(read frame N) qui faisait clignoter Vulkan. mFramesInFlight=1
            // retombe sur le comportement legacy.
            // Migration ex-SSBO -> UBO : un mat4 bones[64] (std140, 4096 octets)
            // est portable et solide sur GL/VK/DX11/DX12. Le SSBO precedent etait
            // casse sur DX11/DX12 (StructuredBuffer/SRV ne remontait pas au
            // shader -> skinMat=0 -> mesh invisible) et en course sur Vulkan.
            mUBOBonesRing.Resize(mFramesInFlight);
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mUBOBonesRing[i] = mDevice->CreateBuffer(NkBufferDesc::Uniform(kMaxBonesUBO*sizeof(NkMat4f)));
            }

            // Buffer d'instances (GPU instancing 1-draw) : models[128]+tints[128]
            // (std140, 10240 octets). Même stratégie ring que les bones.
            mUBOInstanceRing.Resize(mFramesInFlight);
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mUBOInstanceRing[i] = mDevice->CreateBuffer(
                    NkBufferDesc::Uniform(kMaxInstancesUBO * (sizeof(NkMat4f) + sizeof(NkVec4f))));
            }

            // Pool de buffers d'instances pour les ombres instanciées : un buffer par
            // (batch × invocation de shadow pass), consommé via mShadowInstIdx et reset
            // par frame. Chaque buffer = même layout que mUBOInstanceRing (models+tints).
            // Distinct du ring principal pour éviter tout hazard write/draw entre les
            // multiples passes shadow (une par lumière/cascade/face) qui partageraient
            // sinon le même buffer.
            mUBOShadowInstPool.Resize(mFramesInFlight);
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mUBOShadowInstPool[i].Resize(kShadowInstPoolCap);
                for (uint32 d=0; d<kShadowInstPoolCap; d++) {
                    mUBOShadowInstPool[i][d] = mDevice->CreateBuffer(
                        NkBufferDesc::Uniform(kMaxInstancesUBO * (sizeof(NkMat4f) + sizeof(NkVec4f))));
                }
            }

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

            // ── MatCap texture (boule chrome studio) ──────────────────────────────
            // Génère une "boule matcap" 128x128 : chaque pixel = normale de sphère,
            // colorée par un éclairage chrome/studio. Échantillonnée par la normale-vue
            // en mode SOLID/WIREFRAME (matcap TEXTURE). Base pour charger de vrais
            // matcaps .exr/.png plus tard (remplacer la génération par un Load).
            {
                const uint32 S = 128;
                auto td = NkTextureDesc::Tex2D(S, S, NkGPUFormat::NK_RGBA8_UNORM, 1);
                td.debugName = "MatcapChrome";
                mMatcapTex = mDevice->CreateTexture(td);
                if (mMatcapTex.IsValid()) {
                    NkVector<uint8> px; px.Resize(S * S * 4);
                    auto norm3 = [](float a,float b,float c){ float l=sqrtf(a*a+b*b+c*c); return l>1e-6f?1.f/l:0.f; };
                    const float kl=norm3(0.40f,0.50f,0.77f), fl=norm3(-0.5f,0.15f,0.85f);
                    for (uint32 y=0; y<S; ++y) for (uint32 x=0; x<S; ++x) {
                        float nx = ((float)x/(float)(S-1))*2.f - 1.f;
                        float ny = 1.f - ((float)y/(float)(S-1))*2.f;
                        float r2 = nx*nx + ny*ny; float s;
                        if (r2 > 1.f) { s = 0.05f; }                    // hors sphère : fond sombre
                        else {
                            float nz = sqrtf(1.f - r2);
                            float key  = nx*0.40f*kl + ny*0.50f*kl + nz*0.77f*kl; if (key<0.f) key=0.f;
                            float spec = powf(key, 42.f);
                            float fill = nx*(-0.5f)*fl + ny*0.15f*fl + nz*0.85f*fl; if (fill<0.f) fill=0.f;
                            float fres = powf(1.f-nz, 3.f);
                            s = 0.12f + 0.45f*key + 0.9f*spec + 0.22f*fill + 0.32f*fres;
                            if (s>1.f) s=1.f;
                        }
                        uint8 v = (uint8)(s*255.f); uint32 i=(y*S+x)*4;
                        px[i]=v; px[i+1]=v; px[i+2]=v; px[i+3]=255;      // chrome = niveaux de gris
                    }
                    mDevice->WriteTextureRegion(mMatcapTex, px.Data(), 0,0,0, S,S,1, 0, 0);
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
                .Add(24, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                // Phase M.2 : binding=25 = Material Parameter Collection UBO
                // (pool global de params nommes, partage par tous les shaders
                // qui le declarent).
                .Add(25, NkDescriptorType::NK_UNIFORM_BUFFER,         ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                // Phase N v1 : binding=26 = tSkyEnvCube (samplerCube RGBA32F).
                // Cubemap HDR brut sans Reinhard, sample uniquement par le
                // shader skybox (set=0,binding=26). Permet un background avec
                // vrai dynamic range, separe du tEnvPrefilter qui doit garder
                // Reinhard pour l'IBL specular.
                .Add(26, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                // Phase H.6 : binding=27 = tVoxelOpacity (sampler3D R8_UNORM).
                // Voxel grid de la scene pour AO long-range via cone-tracing
                // dans le PBR shader. cf. NkVoxelAOSystem.
                .Add(27, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                // Mode d'affichage : binding=28 = tMatcap (sampler2D), boule matcap
                // échantillonnée par la normale-vue en mode SOLID/WIREFRAME (matcap texture).
                .Add(28, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
            mGlobalLayout = mDevice->CreateDescriptorSetLayout(frameLayout);

            // Object set layout (set 1) : Object UBO(1) + Bones/Instance UBO(4).
            // binding=4 = uniform buffer des joint matrices (skin) OU des instances
            // (instancing GPU) — lu par le vertex shader concerné. Declare ici pour
            // TOUS les pipelines qui partagent mObjectLayout (PBR/Shadow/Skin).
            // *** binding=4 et PAS 2 *** : sur GL/DX le modèle de binding est APLATI
            // (le numéro de set est ignoré), donc set1/binding2 (bones) collisionnait
            // avec set0/binding2 (LightsUBO du global set) au même point GL 2 — le bind
            // de l'object set écrasait les lumières par draw -> ZÉRO lumière directe +
            // aucune ombre sur GL/DX (VK, avec de vrais descriptor sets, n'était pas
            // touché). binding=4 est libre côté global set (0/2/3/8/25/27). UBO
            // (ex-SSBO) : portable et solide sur les 4 backends.
            NkDescriptorSetLayoutDesc objectLayout;
            objectLayout.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
            objectLayout.Add(4, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
            mObjectLayout = mDevice->CreateDescriptorSetLayout(objectLayout);

            // ── Allocate descriptor sets ─────────────────────────────────────────
            // Global : 1 par slot du ring (donnees per-frame).
            // Object : 1 par drawcall x frame (pattern UBO-per-draw, Base03/Vulkan).
            //   Chaque set est bind a son UBO du pool a Init (1:1, jamais re-bind).
            mGlobalSetRing.Resize(mFramesInFlight);
            mGlobalSetMirrorRing.Resize(mFramesInFlight);   // Phase Planar Reflection fix
            mObjectSetPool.Resize(mFramesInFlight);
            for (uint32 i=0; i<mFramesInFlight; i++) {
                mGlobalSetRing[i]       = mDevice->AllocateDescriptorSet(mGlobalLayout);
                mGlobalSetMirrorRing[i] = mDevice->AllocateDescriptorSet(mGlobalLayout);
                mObjectSetPool[i].Resize(mObjectPoolCap);
                for (uint32 d=0; d<mObjectPoolCap; d++) {
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

            // Phase Planar Reflection fix : lambda partagee qui pre-bind un
            // global set (main ou mirror) avec un UBO Camera donne. Toutes les
            // autres ressources (lights, shadow, env, voxel, cookies) sont
            // identiques entre les rings main et mirror.
            auto preBindGlobalSet = [&](NkDescSetHandle gs, NkBufferHandle cameraUBO, uint32 i) {
                if (!gs.IsValid()) return;
                mDevice->BindUniformBuffer(gs, 0, cameraUBO);
                mDevice->BindUniformBuffer(gs, 2, mUBOLightsRing[i]);

                // ShadowSlotsUBO depuis NkVSM : bind le buffer du ring i correspondant
                // au slot frame i du descriptor set (multi-frame in flight, evite
                // data hazard CPU/GPU).
                if (mShadow && i < mShadow->GetRingSize()) {
                    NkBufferHandle sub = mShadow->GetRingBuffer(i);
                    if (sub.IsValid()) {
                        mDevice->BindUniformBuffer(gs, 3, sub);
                    }
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
                    if (mEnv->GetSkyEnvCube().IsValid())
                        mDevice->BindTextureSampler(gs, 26, mEnv->GetSkyEnvCube(), envSamp);
                }

                if (mVoxelAO && mVoxelAO->GetVoxelTexture().IsValid()) {
                    mDevice->BindTextureSampler(gs, 27,
                        mVoxelAO->GetVoxelTexture(), mVoxelAO->GetVoxelSampler());
                }

                // Binding 28 : boule matcap (mode SOLID/WIREFRAME, matcap texture).
                // Sampler CLAMP (pas repeat !) : au bord des sphères l'UV frôle 0/1 et le
                // repeat rebouclerait sur le bord opposé -> stries sombres. Clamp = propre.
                NkSamplerHandle mcSamp = mResources ? mResources->GetSamplerLinearClamp() : defSampler;
                if (mMatcapTex.IsValid() && mcSamp.IsValid())
                    mDevice->BindTextureSampler(gs, 28, mMatcapTex, mcSamp);

                if (mShadow && mShadow->GetAtlasTexture().IsValid()) {
                    mDevice->BindTextureSampler(gs, 11,
                        mShadow->GetAtlasTexture(), mShadow->GetAtlasSampler());
                    mDevice->BindTextureSampler(gs, 12,
                        mShadow->GetAtlasTexture(), mShadow->GetAtlasRawSampler());
                }

                if (mResources) {
                    NkTextureHandle whiteTex = mResources->GetWhiteTex();
                    NkSamplerHandle whiteSamp = mResources->GetSamplerLinearRepeat();
                    for (uint32 ci = 0; ci < kMaxCookies3D; ci++) {
                        mDevice->BindTextureSampler(gs, 13 + ci, whiteTex, whiteSamp);
                    }
                    if (mDefaultCubeWhite.IsValid()) {
                        for (uint32 ci = 0; ci < kMaxCookiesCube3D; ci++) {
                            mDevice->BindTextureSampler(gs, 21 + ci,
                                mDefaultCubeWhite, whiteSamp);
                        }
                    }
                }
            };

            for (uint32 i=0; i<mFramesInFlight; i++) {
                // Bind main + mirror sets avec leur UBO Camera respectif.
                preBindGlobalSet(mGlobalSetRing[i],       mUBOCameraRing[i],       i);
                preBindGlobalSet(mGlobalSetMirrorRing[i], mUBOCameraMirrorRing[i], i);

                // Phase F.B.1 : bind chaque set du pool a son UBO du pool (1:1).
                // + Skinning : bind l'UBO de bones de la frame au binding=2 de
                //   chaque set objet (le contenu est reecrit par draw skinne dans
                //   FlushSkinned ; les draws non-skinnes ne lisent jamais ce slot
                //   mais le binding doit etre valide pour le layout VK/DX).
                for (uint32 d=0; d<mObjectPoolCap; d++) {
                    NkDescSetHandle os = mObjectSetPool[i][d];
                    if (os.IsValid()) {
                        mDevice->BindUniformBuffer(os, 1, mUBOObjectPool[i][d]);
                        // Ring : chaque frame i bind SON UBO de bones (pas un
                        // buffer partage) -> la frame N+1 ecrit mUBOBonesRing[N+1]
                        // pendant que le GPU lit encore mUBOBonesRing[N].
                        if (i < mUBOBonesRing.Size() && mUBOBonesRing[i].IsValid()) {
                            mDevice->BindUniformBuffer(os, 4, mUBOBonesRing[i]);  // binding 4 (anti-collision GL/DX)
                        }
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

            // ── Shadow INSTANCIÉ (ombres des mInstanced en 1 draw/batch) ──────────
            // Version depth-only du shader Instanced : projette N instances dans
            // l'atlas via lightVP (push const) + InstanceUBO (set1 binding4). Réutilise
            // le shadow render pass + mObjectLayout (qui a déjà binding1 + binding4).
            if (mShaderLib) {
                auto progSInst = mShaderLib->LoadOrCompileVF("ShadowInstanced", "", "");
                if (progSInst.IsValid())
                    mShadowInstanceShader = mShaderLib->GetRHIHandle(progSInst);
                logger.Info("[NkRender3D] ShadowInstanced shader compile: valid={0}\n",
                            mShadowInstanceShader.IsValid() ? 1 : 0);
            }
            if (mShadowInstanceShader.IsValid()) {
                NkGraphicsPipelineDesc pd;
                pd.shader       = mShadowInstanceShader;
                pd.depthStencil = NkDepthStencilDesc::Default();    // depth write
                if (mShadow) pd.renderPass = mShadow->GetShadowRenderPass();
                pd.rasterizer   = NkRasterizerDesc::NoCull();
                pd.blend        = NkBlendDesc::Opaque();
                pd.debugName    = "ShadowInstanced_DepthOnly";
                pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f));
                pd.descriptorSetLayouts.PushBack(mGlobalLayout);
                pd.descriptorSetLayouts.PushBack(mObjectLayout);    // binding1 + binding4
                pd.vertexLayout
                  .AddBinding(0, sizeof(NkVertex3D), false)
                  .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0);
                mShadowInstancePipeline = mDevice->CreateGraphicsPipeline(pd);
                logger.Info("[NkRender3D] ShadowInstanced pipeline create: shader_valid={0} pipeline_valid={1}\n",
                            mShadowInstanceShader.IsValid() ? 1 : 0, mShadowInstancePipeline.IsValid() ? 1 : 0);
            }

            // ── Skinning GPU : shader Skin ───────────────────────────────────
            // Source canonique : Resources/NKRenderer/Shaders/Skin/VK/skin.{vert,frag}.vk.glsl
            // (converti VK->GL/HLSL/MSL au run par SPIRV-Cross). Le vertex shader
            // fait du linear blend skinning a partir de l'UBO de bones (set=1,
            // binding=2). Pipeline cree lazy au 1er FlushSkinned (EnsureSkinPipeline).
            if (mShaderLib) {
                auto progSkin = mShaderLib->LoadOrCompileVF("Skin", "", "");
                if (progSkin.IsValid())
                    mSkinShader = mShaderLib->GetRHIHandle(progSkin);
                logger.Info("[NkRender3D] Skin shader compile: valid={0}\n",
                            mSkinShader.IsValid() ? 1 : 0);
            }

            // ── GPU instancing : shader Instanced (instanced.{vert,frag}.nksl) ───
            // Source NkSL UNIQUE Resources/.../Shaders/Instanced/NkSL/ compilée par
            // le VRAI NkSLCompiler vers le backend courant (chemin .nksl de
            // LoadOrCompileVF). Vertex layout standard ; lit la matrice par instance
            // via gl_InstanceID. Pipeline créé lazy (EnsureInstancePipeline).
            if (mShaderLib) {
                auto progInst = mShaderLib->LoadOrCompileVF("Instanced", "", "");
                if (progInst.IsValid())
                    mInstanceShader = mShaderLib->GetRHIHandle(progInst);
                logger.Info("[NkRender3D] Instanced shader compile: valid={0}\n",
                            mInstanceShader.IsValid() ? 1 : 0);
            }

            // ── Phase N v0.5 : Skybox shader ────────────────────────────────
            // Compile le shader Skybox au Init ; le pipeline est cree lazy au
            // 1er Flush quand mDrawSkybox=true (cf. EnsureSkyboxPipeline).
            if (mShaderLib) {
                auto progSky = mShaderLib->LoadOrCompileVF("Skybox", "", "");
                if (progSky.IsValid())
                    mSkyboxShader = mShaderLib->GetRHIHandle(progSky);
            }

            // ── Grille infinie (InfiniteGrid) : shader compilé au Init, pipeline lazy ──
            if (mShaderLib) {
                auto progGrid = mShaderLib->LoadOrCompileVF("InfiniteGrid", "", "");
                if (progGrid.IsValid())
                    mGridShader = mShaderLib->GetRHIHandle(progGrid);
                logger.Info("[NkRender3D] InfiniteGrid shader compile: valid={0}\n",
                            mGridShader.IsValid() ? 1 : 0);
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
            // set=2 = layout per-instance materiau (UBO + albedo/normal/orm/emissive).
            // Le shader PBR canonical sample tAlbedo dans set=2 binding=3 depuis
            // la migration set=0 -> set=2. Sans ce layout, Vulkan voit le set=2
            // comme non declare -> validation spam + chute FPS massive (mesuree
            // 500 -> 150 fps avant ce fix). mMat->GetInstanceLayout() expose le
            // meme layout que celui utilise par BindInstance().
            if (mMat && mMat->GetInstanceLayout().IsValid())
                pd.descriptorSetLayouts.PushBack(mMat->GetInstanceLayout());

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
                        mPBRShader.IsValid() ? 1 : 0, mPBRPipeline.IsValid() ? 1 : 0, currentRP.id);
            return mPBRPipeline.IsValid();
        }

        // ── Lazy create du pipeline de skinning GPU ──────────────────────────
        // Calque sur EnsurePBRPipeline mais avec le vertex layout NkVertexSkinned
        // (ajout de aBoneIdx/aBoneWeight) et le shader "Skin". L'UBO de bones
        // est deja lie au set objet (set=1, binding=2) a Init. Memes set layouts
        // que le PBR (global/object/material) -> compatible avec les binds du
        // FlushSkinned (set global + set objet + set materiau fallback).
        bool NkRender3D::EnsureSkinPipeline(NkRenderPassHandle currentRP) {
            if (!mSkinShader.IsValid()) return false;
            if (mSkinPipeline.IsValid()) return true;

            NkGraphicsPipelineDesc pd;
            pd.shader       = mSkinShader;
            pd.depthStencil = NkDepthStencilDesc::Default();
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.debugName    = "Skin_Opaque";
            pd.renderPass   = currentRP;
            pd.descriptorSetLayouts.PushBack(mGlobalLayout);
            pd.descriptorSetLayouts.PushBack(mObjectLayout);
            if (mMat && mMat->GetInstanceLayout().IsValid())
                pd.descriptorSetLayouts.PushBack(mMat->GetInstanceLayout());

            // Vertex layout NkVertexSkinned : NkVertex3D (56o) + boneIdx(vec4,16o)
            //   + boneWeight(vec4,16o). Stride = 88. Les indices sont en float
            //   (RGBA32_FLOAT) pour rester portables cross-backend (cf. struct).
            pd.vertexLayout
              .AddBinding(0, sizeof(NkVertexSkinned), false)
              .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT,  0,  "POSITION", 0)
              .AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT,  12, "NORMAL",   0)
              .AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT,  24, "TANGENT",  0)
              .AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT,   36, "TEXCOORD", 0)
              .AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT,   44, "TEXCOORD", 1)
              .AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM,  52, "COLOR",    0)
              // boneIdx/boneWeight : semantiques DX = TEXCOORD2 / TEXCOORD3 (PAS
              // BLENDINDICES/BLENDWEIGHT). Le generateur NkSL->HLSL mappe les inputs
              // @location(6)/@location(7) en TEXCOORD avec index sequentiel apres
              // les uv (uv=TEXCOORD0, uv2=TEXCOORD1, boneIdx=TEXCOORD2, boneWeight=
              // TEXCOORD3). Avec BLENDINDICES/BLENDWEIGHT : DX12 rejette l'input
              // layout (signature VS attend TEXCOORD2/3 -> CreateInputLayout echoue
              // -> modele invisible) et DX11 cree le layout mais lie du vide -> poids
              // de bones = 0 -> wsum~0 -> bind pose (modele NON skinne). VK/GL
              // utilisent les locations, les chaines de semantique sont ignorees.
              .AddAttribute(6, 0, NkVertexFormat::NK_RGBA32_FLOAT, 56, "TEXCOORD", 2)
              .AddAttribute(7, 0, NkVertexFormat::NK_RGBA32_FLOAT, 72, "TEXCOORD", 3);

            mSkinPipeline   = mDevice->CreateGraphicsPipeline(pd);
            mSkinPipelineRP = currentRP;
            logger.Info("[NkRender3D] Skin pipeline (lazy) create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
                        mSkinShader.IsValid() ? 1 : 0, mSkinPipeline.IsValid() ? 1 : 0,
                        currentRP.id);
            return mSkinPipeline.IsValid();
        }

        // ── Lazy create du pipeline d'instancing GPU ─────────────────────────
        // Calque sur EnsureSkinPipeline mais avec le vertex layout STANDARD
        // (NkVertex3D, sans bones) et le shader "Instanced". Le buffer d'instances
        // est lié au set objet (binding 2). Mêmes set layouts que le PBR/skin.
        bool NkRender3D::EnsureInstancePipeline(NkRenderPassHandle currentRP) {
            if (!mInstanceShader.IsValid()) return false;
            if (mInstancePipeline.IsValid()) return true;

            NkGraphicsPipelineDesc pd;
            pd.shader       = mInstanceShader;
            pd.depthStencil = NkDepthStencilDesc::Default();
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.debugName    = "Instanced_Opaque";
            pd.renderPass   = currentRP;
            pd.descriptorSetLayouts.PushBack(mGlobalLayout);
            pd.descriptorSetLayouts.PushBack(mObjectLayout);
            if (mMat && mMat->GetInstanceLayout().IsValid())
                pd.descriptorSetLayouts.PushBack(mMat->GetInstanceLayout());

            // Vertex layout STANDARD NkVertex3D (56 octets) — identique au PBR.
            pd.vertexLayout
              .AddBinding(0, sizeof(NkVertex3D), false)
              .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT,  0,  "POSITION", 0)
              .AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT,  12, "NORMAL",   0)
              .AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT,  24, "TANGENT",  0)
              .AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT,   36, "TEXCOORD", 0)
              .AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT,   44, "TEXCOORD", 1)
              .AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM,  52, "COLOR",    0);

            mInstancePipeline   = mDevice->CreateGraphicsPipeline(pd);
            mInstancePipelineRP = currentRP;
            logger.Info("[NkRender3D] Instanced pipeline (lazy) create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
                        mInstanceShader.IsValid() ? 1 : 0, mInstancePipeline.IsValid() ? 1 : 0,
                        currentRP.id);
            return mInstancePipeline.IsValid();
        }

        // ── Phase N v0.5 : EnsureSkyboxPipeline (lazy, RP-compatible) ───────
        // Pipeline minimal : pas de VBO (gl_VertexIndex pour 3 verts fullscreen),
        // depth test LEQUAL + depthWrite=false (les objets dessines apres
        // peuvent occlure la skybox sans qu'elle leur barre la route).
        bool NkRender3D::EnsureSkyboxPipeline(NkRenderPassHandle currentRP) {
            if (!mSkyboxShader.IsValid()) return false;
            if (mSkyboxPipeline.IsValid()) return true;

            NkGraphicsPipelineDesc pd;
            pd.shader     = mSkyboxShader;
            // Depth : test LEQUAL pour passer le clear=1.0 du depth buffer,
            // mais pas d'ecriture - les objets opaques garderont leur depth.
            {
                NkDepthStencilDesc ds;
                ds.depthTestEnable  = true;
                ds.depthWriteEnable = false;
                ds.depthCompareOp   = ::nkentseu::NkCompareOp::NK_LESS_EQUAL;
                pd.depthStencil = ds;
            }
            pd.rasterizer = NkRasterizerDesc::NoCull();
            pd.blend      = NkBlendDesc::Opaque();
            pd.debugName  = "Skybox";
            pd.renderPass = currentRP;
            // Set 0 = global set (CameraUBO @0 + tEnvPrefilter @9 reuse-direct).
            pd.descriptorSetLayouts.PushBack(mGlobalLayout);
            // Pas de vertex layout (gl_VertexIndex only dans le vert shader).

            mSkyboxPipeline = mDevice->CreateGraphicsPipeline(pd);
            mSkyboxPipelineRP = currentRP;
            logger.Info("[NkRender3D] Skybox pipeline create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
                        mSkyboxShader.IsValid() ? 1 : 0, mSkyboxPipeline.IsValid() ? 1 : 0,
                        currentRP.id);
            return mSkyboxPipeline.IsValid();
        }

        // Draw 1 triangle fullscreen avec le pipeline Skybox. Doit etre appele
        // dans la passe Geometry, AVANT les drawcalls opaques (pour que le
        // depth=1.0 ecrit par la skybox ne masque pas les objets — bien que
        // depthWrite=false ait neutralise ce risque, garder l'ordre logique).
        void NkRender3D::DrawSkybox(NkICommandBuffer* cmd) {
            if (!mDrawSkybox || !cmd) return;
            if (!mSkyboxPipeline.IsValid()) return;

            // Bind pipeline + global set (CameraUBO + prefilter cubemap).
            // Phase Planar Reflection fix : bind le ring mirror si en mirror pass.
            cmd->BindGraphicsPipeline(mSkyboxPipeline);
            NkDescSetHandle gs;
            if (mPendingMirrorActive) {
                gs = (mFrameSlot < mGlobalSetMirrorRing.Size())
                     ? mGlobalSetMirrorRing[mFrameSlot] : NkDescSetHandle{};
            } else {
                gs = (mFrameSlot < mGlobalSetRing.Size())
                     ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
            }
            if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);

            // 3 vertices, 1 instance (fullscreen triangle sans VBO)
            cmd->Draw(3, 1, 0, 0);
        }

        // ── Grille infinie style Blender (plan y=0) ─────────────────────────────
        bool NkRender3D::EnsureGridPipeline(NkRenderPassHandle currentRP) {
            if (!mGridShader.IsValid()) return false;
            if (mGridPipeline.IsValid() && mGridPipelineRP == currentRP) return true;
            if (mGridPipeline.IsValid()) { mDevice->DestroyPipeline(mGridPipeline); mGridPipeline = {}; }

            NkGraphicsPipelineDesc pd;
            pd.shader = mGridShader;
            // Depth : test LEQUAL (la grille au sol s'affiche sur/à hauteur du sol),
            // pas d'écriture (overlay qui respecte la profondeur des objets opaques).
            {
                NkDepthStencilDesc ds;
                ds.depthTestEnable  = true;
                ds.depthWriteEnable = false;
                ds.depthCompareOp   = ::nkentseu::NkCompareOp::NK_LESS_EQUAL;
                pd.depthStencil = ds;
            }
            pd.rasterizer = NkRasterizerDesc::NoCull();   // triangle plein-écran, pas de cull
            pd.blend      = NkBlendDesc::Alpha();          // fondu de l'intérieur + lignes
            pd.debugName  = "InfiniteGrid";
            pd.renderPass = currentRP;
            // Push constant = 6 vec4 (lineColor, cellColor, axisX, axisZ, params, extra).
            pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkVec4f) * 6);
            pd.descriptorSetLayouts.PushBack(mGlobalLayout);  // set 0 = CameraUBO
            // Pas de vertex layout (le quad est généré via gl_VertexID).

            mGridPipeline   = mDevice->CreateGraphicsPipeline(pd);
            mGridPipelineRP = currentRP;
            logger.Info("[NkRender3D] Grid pipeline create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
                        mGridShader.IsValid() ? 1 : 0, mGridPipeline.IsValid() ? 1 : 0, currentRP.id);
            return mGridPipeline.IsValid();
        }

        void NkRender3D::DrawGrid(NkICommandBuffer* cmd) {
            if (!mDrawGrid || !cmd || !mGridPipeline.IsValid()) return;
            cmd->BindGraphicsPipeline(mGridPipeline);

            // Push constant : doit matcher le bloc PC des shaders infinitegrid.*.nksl.
            struct GridPC {
                NkVec4f lineColor;
                NkVec4f cellColor;
                NkVec4f axisXColor;
                NkVec4f axisZColor;
                NkVec4f params;   // .x=cellSize .y=majorEvery .z=extent .w=fadeEnd
                NkVec4f extra;    // .x=planeY ; .yzw réservés
            } pc;
            pc.lineColor  = mGridParams.lineColor;
            pc.cellColor  = mGridParams.cellColor;
            pc.axisXColor = mGridParams.axisXColor;
            pc.axisZColor = mGridParams.axisZColor;
            pc.params     = NkVec4f{ mGridParams.cellSize, mGridParams.majorEvery,
                                     mGridParams.extent,   mGridParams.fadeEnd };
            pc.extra      = NkVec4f{ mGridParams.planeY,
                                     mGridParams.showMinor ? 1.f : 0.f,
                                     mGridParams.showMajor ? 1.f : 0.f,
                                     mGridParams.showAxes  ? 1.f : 0.f };
            cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(GridPC), &pc);

            NkDescSetHandle gs = (mFrameSlot < mGlobalSetRing.Size())
                               ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
            if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);

            cmd->Draw(3, 1, 0, 0);   // 1 triangle plein-écran (reconstruction de rayon)
        }

        void NkRender3D::Shutdown() {
            if (mSkyboxPipeline.IsValid()) { mDevice->DestroyPipeline(mSkyboxPipeline); mSkyboxPipeline={}; }
            if (mGridPipeline.IsValid())   { mDevice->DestroyPipeline(mGridPipeline);   mGridPipeline={}; }
            if (mSkinPipeline.IsValid())   { mDevice->DestroyPipeline(mSkinPipeline);   mSkinPipeline={}; }
            if (mShadowPipeline.IsValid()) { mDevice->DestroyPipeline(mShadowPipeline); mShadowPipeline={}; }
            if (mShadowInstancePipeline.IsValid()) { mDevice->DestroyPipeline(mShadowInstancePipeline); mShadowInstancePipeline={}; }
            if (mPBRPipeline.IsValid()) { mDevice->DestroyPipeline(mPBRPipeline); mPBRPipeline={}; }
            // Les shader handles sont detenus par NkShaderLibrary, pas a detruire ici.
            for (auto& s : mGlobalSetRing)       if (s.IsValid()) mDevice->FreeDescriptorSet(s);
            for (auto& s : mGlobalSetMirrorRing) if (s.IsValid()) mDevice->FreeDescriptorSet(s);
            // Phase F.B.1 : free le pool object 2D (frame x drawIdx).
            for (auto& perFrame : mObjectSetPool) {
                for (auto& s : perFrame) if (s.IsValid()) mDevice->FreeDescriptorSet(s);
                perFrame.Clear();
            }
            mGlobalSetRing.Clear();
            mGlobalSetMirrorRing.Clear();
            mObjectSetPool.Clear();
            if (mGlobalLayout.IsValid()) { mDevice->DestroyDescriptorSetLayout(mGlobalLayout); }
            if (mObjectLayout.IsValid()) { mDevice->DestroyDescriptorSetLayout(mObjectLayout); }
            for (auto& b : mUBOCameraRing)       if (b.IsValid()) mDevice->DestroyBuffer(b);
            for (auto& b : mUBOCameraMirrorRing) if (b.IsValid()) mDevice->DestroyBuffer(b);
            for (auto& perFrame : mUBOObjectPool) {
                for (auto& b : perFrame) if (b.IsValid()) mDevice->DestroyBuffer(b);
                perFrame.Clear();
            }
            for (auto& b : mUBOLightsRing) if (b.IsValid()) mDevice->DestroyBuffer(b);
            mUBOCameraRing.Clear();
            mUBOCameraMirrorRing.Clear();
            mUBOObjectPool.Clear();
            mUBOLightsRing.Clear();
            for (auto& b : mUBOBonesRing) if (b.IsValid()) mDevice->DestroyBuffer(b);
            mUBOBonesRing.Clear();
            for (auto& perFrame : mUBOShadowInstPool) {
                for (auto& b : perFrame) if (b.IsValid()) mDevice->DestroyBuffer(b);
                perFrame.Clear();
            }
            mUBOShadowInstPool.Clear();
            if(mDefaultCubeWhite.IsValid()){mDevice->DestroyTexture(mDefaultCubeWhite);mDefaultCubeWhite={};}

            // DEBUG triangle resources
            if (mDebugPipeline.IsValid()) { mDevice->DestroyPipeline(mDebugPipeline); mDebugPipeline={}; }
            if (mDebugVBO.IsValid())      { mDevice->DestroyBuffer(mDebugVBO); mDebugVBO={}; }
            if (mDebugIBO.IsValid())      { mDevice->DestroyBuffer(mDebugIBO); mDebugIBO={}; }
            // Lignes + triangles debug (gizmos/cage/faces éditeur).
            if (mLinePipeline.IsValid())        { mDevice->DestroyPipeline(mLinePipeline); mLinePipeline={}; }
            if (mLinePipelineNoDepth.IsValid()) { mDevice->DestroyPipeline(mLinePipelineNoDepth); mLinePipelineNoDepth={}; }
            if (mLineVBO.IsValid())             { mDevice->DestroyBuffer(mLineVBO); mLineVBO={}; }
            if (mTriPipeline.IsValid())         { mDevice->DestroyPipeline(mTriPipeline); mTriPipeline={}; }
            if (mTriPipelineNoDepth.IsValid())  { mDevice->DestroyPipeline(mTriPipelineNoDepth); mTriPipelineNoDepth={}; }
            if (mTriVBO.IsValid())              { mDevice->DestroyBuffer(mTriVBO); mTriVBO={}; }
            if (mEditLineBuf.IsValid())         { mDevice->DestroyBuffer(mEditLineBuf); mEditLineBuf={}; }
            if (mEditTriBuf.IsValid())          { mDevice->DestroyBuffer(mEditTriBuf); mEditTriBuf={}; }
            if (mEditPointBuf.IsValid())        { mDevice->DestroyBuffer(mEditPointBuf); mEditPointBuf={}; }
            mDebugInited = false;
        }

        // ── Scene ─────────────────────────────────────────────────────────────────
        void NkRender3D::GrowObjectPool(uint32 newCap) {
            if (newCap > kObjectPoolHardMax) newCap = kObjectPoolHardMax;
            if (newCap <= mObjectPoolCap || !mDevice) return;

            // Alloc les nouveaux buffers + descriptor sets pour CHAQUE frame-in-flight
            // (HORS render pass — appelé depuis ResetFrame). Réplique le pré-bind d'Init :
            // binding1 = ObjectUBO du slot, binding4 = bones ring de la frame (défaut ;
            // réécrit dynamiquement par les draws instanciés/skinnés).
            for (uint32 i=0; i<mFramesInFlight; i++) {
                const uint32 old = (uint32)mUBOObjectPool[i].Size();
                mUBOObjectPool[i].Resize(newCap);
                mObjectSetPool[i].Resize(newCap);
                for (uint32 d=old; d<newCap; d++) {
                    mUBOObjectPool[i][d] = mDevice->CreateBuffer(NkBufferDesc::Uniform(kObjectUBOBytes));
                    NkDescSetHandle os   = mDevice->AllocateDescriptorSet(mObjectLayout);
                    mObjectSetPool[i][d] = os;
                    if (os.IsValid()) {
                        if (mUBOObjectPool[i][d].IsValid())
                            mDevice->BindUniformBuffer(os, 1, mUBOObjectPool[i][d]);
                        if (i < mUBOBonesRing.Size() && mUBOBonesRing[i].IsValid())
                            mDevice->BindUniformBuffer(os, 4, mUBOBonesRing[i]);
                    }
                }
            }
            logger.Info("[NkRender3D] Object pool grown: {0} -> {1} (x{2} frames)\n",
                        mObjectPoolCap, newCap, mFramesInFlight);
            mObjectPoolCap = newCap;
        }

        void NkRender3D::ResetFrame() {
            // Appelee une seule fois par frame depuis NkRendererImpl::BeginFrame.
            // Reset l'index du pool d'UBO objets pour la nouvelle frame.
            // Doit NE PAS etre fait dans BeginScene : si la frame a deux passes
            // (ex: passe miroir + passe principale), reset entre les deux ecrase
            // les UBOs de la 1ere passe au moment du Execute() differe (backend GL).
            // Croissance dynamique du pool object-UBO : si la frame précédente a
            // atteint (donc frôlé/dépassé) la capacité, on double AVANT la nouvelle
            // frame (ici = hors render pass). mObjectDrawIdx est plafonné à mObjectPoolCap
            // par les guards → « == cap » signale un besoin non satisfait. Converge en
            // quelques frames ; les guards évitent tout crash entre-temps.
            if (mObjectDrawIdx >= mObjectPoolCap && mObjectPoolCap < kObjectPoolHardMax) {
                GrowObjectPool(mObjectPoolCap * 2);
            }
            mObjectDrawIdx = 0;
            mShadowInstIdx = 0;   // pool d'instances shadow : reset pour la nouvelle frame
        }

        void NkRender3D::BeginScene(const NkSceneContext& ctx) {
            mCtx = ctx;
            mInScene = true;
            mOpaque.Clear(); mTransparent.Clear();
            mShadowCasters.Clear();
            mInstanced.Clear(); mSkinned.Clear();
            // mObjectDrawIdx N'EST PAS reset ici — voir ResetFrame() ci-dessus.
        }

        // ── Submit ────────────────────────────────────────────────────────────────
        void NkRender3D::Submit(const NkDrawCall3D& dc) {
            if (!dc.visible) return;

            NkVec3f camPos = mCtx.camera.GetPosition();
            NkVec3f center = dc.aabb.Center();
            float32 dx=center.x-camPos.x, dy=center.y-camPos.y, dz=center.z-camPos.z;
            float32 depth = dx*dx + dy*dy + dz*dz;

            // Caster d'ombre : collecte AVANT le culling camera. Un objet hors
            // champ camera peut projeter une ombre dans la zone visible ; il
            // doit donc entrer dans la passe shadow meme s'il est cull du rendu
            // principal. (Cause racine "objets sans ombre" : avant, le culling
            // camera retirait le caster de mOpaque, et la passe shadow iterait
            // sur mOpaque.)
            if (dc.castShadow) mShadowCasters.PushBack({dc, depth});

            // Culling camera : uniquement pour le rendu visible (mOpaque).
            if (!mCtx.camera.IsAABBVisible(dc.aabb)) return;
            mOpaque.PushBack({dc, depth});
        }

        void NkRender3D::SubmitMany(const NkDrawCall3D* dcs, uint32 count) {
            for (uint32 i=0; i<count; i++) Submit(dcs[i]);
        }

        void NkRender3D::SubmitInstanced(const NkDrawCallInstanced& dc) {
            mInstanced.PushBack(dc);
        }

        NkAABB NkRender3D::GetShadowCasterBounds() const {
            NkAABB b;  // min = +inf, max = -inf (cf. NkRendererTypes.h)
            for (const auto& sdc : mShadowCasters) b.Merge(sdc.dc.aabb);
            for (const auto& idc : mInstanced)     b.Merge(idc.aabb);
            // Aucun caster -> AABB "inverse" (min>max) ; retourne un cube unite.
            if (b.min.x > b.max.x) { b.min = {-1.f, -1.f, -1.f}; b.max = {1.f, 1.f, 1.f}; }
            return b;
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
            // CORRECTIF flicker Vulkan : le slot de ring DOIT etre l'index de frame
            // REEL du device (protege par sa fence inFlightFence), pas un compteur
            // independant. Sinon le CPU ecrit (memcpy mappe) un slot d'UBO encore lu
            // par le GPU (frame in-flight) -> uObj.model corrompu -> gl_Position a
            // l'infini -> gros quad noir intermittent. Le device garantit que la
            // frame qui a utilise GetFrameIndex() precedemment est terminee.
            if (mDevice && mFramesInFlight > 0)
                mFrameSlot = mDevice->GetFrameIndex() % mFramesInFlight;
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
            // Skinning : cree (lazy) le pipeline skin compatible avec ce RP.
            // No-op si aucun shader skin (build sans assets) ou deja cree.
            if (!mSkinned.Empty()) EnsureSkinPipeline(currentRP);

            // Instancing GPU 1-draw — EXPÉRIMENTAL (opt-in STRICT NK_INSTANCING_GPU_1DRAW).
            // Le pipeline + shaders compilent sur les 5 backends (pipeline_valid=1) mais
            // la livraison du buffer d'instances (set objet binding 2) ne rend visible
            // que sur Vulkan ; GL/DX n'affichent pas les cubes (binding descriptor à
            // creuser, cf. RenderDoc). Tant que ce n'est pas résolu, le DÉFAUT reste
            // l'expansion object-UBO (N draws) dans FlushInstanced — CORRECTE + éclairée
            // sur tous les backends. NK_INSTANCING_GPU (ancien flag) ne suffit plus pour
            // éviter d'activer le chemin cassé par mégarde.
            {
                static int gpuInst = -1;
                if (gpuInst == -1) { const char* v = getenv("NK_INSTANCING_GPU_1DRAW"); gpuInst = (v && v[0] && v[0] != '0') ? 1 : 0; }
                if (gpuInst && !mInstanced.Empty()) EnsureInstancePipeline(currentRP);
            }

            // Phase N v0.5 : Background HDR skybox. Cree paresseusement le
            // pipeline (RP-compatible) puis draw 1 triangle fullscreen. Doit
            // etre fait AVANT FlushOpaque pour que les objets opaques puissent
            // ecrire leur depth correctement par dessus la skybox (qui a
            // depthWrite=false). Le globalSet du slot frame est bind a
            // l'interieur de DrawSkybox.
            if (mDrawSkybox) {
                EnsureSkyboxPipeline(currentRP);
                DrawSkybox(cmd);
            }

            // Notifie le material system du RP courant (Vulkan compat).
            // UpdateRenderPass invalide les pipelines material si le RP a change
            // (ex: resize swapchain). Idempotent si le RP est identique.
            if (mMat) mMat->UpdateRenderPass(currentRP);

            // Le pipeline est desormais lie par draw dans FlushOpaque (multi-materiau).
            // On bind d'abord le pipeline PBR par defaut pour les draws sans materiau.
            // BindGraphicsPipeline doit preceder BindDescriptorSet sur OpenGL (change le VAO).
            if (mPBRPipeline.IsValid())
                cmd->BindGraphicsPipeline(mPBRPipeline);

            // Bind per-frame descriptor set du slot courant. Phase Planar Reflection
            // fix : en mirror pass, bind le ring mirror dedie (UBO Camera mirror
            // avec view + reflectionFlags=1) au lieu du ring main.
            NkDescSetHandle gs;
            if (mPendingMirrorActive) {
                gs = (mFrameSlot < mGlobalSetMirrorRing.Size())
                     ? mGlobalSetMirrorRing[mFrameSlot] : NkDescSetHandle{};
            } else {
                gs = (mFrameSlot < mGlobalSetRing.Size())
                     ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
            }
            if (gs.IsValid())
                cmd->BindDescriptorSet(gs, 0);

            // Mode d'affichage wireframe : propage au material system (c'est lui qui binde
            // le pipeline final par BindInstance -> il doit choisir la variante fil-de-fer).
            if (mMat) mMat->SetWireframe(mWireframe);
            FlushOpaque(cmd);
            FlushInstanced(cmd);
            FlushSkinned(cmd);
            // Grille infinie : APRÈS l'opaque (occlusion correcte par les objets),
            // AVANT le transparent (le transparent se blend par-dessus la grille).
            if (mDrawGrid) {
                EnsureGridPipeline(currentRP);
                DrawGrid(cmd);
                // Le grid a re-bindé son pipeline/PC ; on rétablit le set global 0
                // pour les passes suivantes (transparent/debug le rebindent au besoin).
                if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);
            }
            FlushTransparent(cmd);
            FlushDebug(cmd, currentRP, gs);
            mInScene=false;

            // NOTE : plus d'auto-avance de mFrameSlot ici. Il est desormais derive
            // de mDevice->GetFrameIndex() au DEBUT de Flush (sync avec la fence du
            // device), ce qui corrige le flicker Vulkan (ecriture d'un slot in-flight).
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

        void NkRender3D::SetMaterialCollection(NkMaterialCollection* mpc) {
            if (!mpc) return;
            const NkBufferHandle ubo = mpc->GetUBO();
            if (!ubo.IsValid()) return;
            for (uint32 i = 0; i < mFramesInFlight; i++) {
                NkDescSetHandle gs = (i < mGlobalSetRing.Size()) ? mGlobalSetRing[i] : NkDescSetHandle{};
                if (gs.IsValid())
                    mDevice->BindUniformBuffer(gs, NkMaterialCollection::kBinding, ubo);
                // Phase Planar Reflection fix 2026-05-24 : aussi bind sur le
                // ring mirror sinon globalTint=undefined -> Layered noir sur VK.
                NkDescSetHandle gsm = (i < mGlobalSetMirrorRing.Size()) ? mGlobalSetMirrorRing[i] : NkDescSetHandle{};
                if (gsm.IsValid())
                    mDevice->BindUniformBuffer(gsm, NkMaterialCollection::kBinding, ubo);
            }
            logger.Info("[NkRender3D] MaterialCollection UBO bind a set=0 binding={0}\n",
                        NkMaterialCollection::kBinding);
        }

        // Phase H.6 : bind la texture 3D voxel au binding=27 sur tous les
        // sets du ring. Appele apres Init de Render3D (le pre-bind du Init
        // skip ce binding car mVoxelAO=nullptr a ce moment).
        void NkRender3D::SetVoxelAO(NkVoxelAOSystem* vao) {
            mVoxelAO = vao;
            if (!mVoxelAO) return;
            NkTextureHandle tex  = mVoxelAO->GetVoxelTexture();
            NkSamplerHandle samp = mVoxelAO->GetVoxelSampler();
            if (!tex.IsValid() || !samp.IsValid()) return;
            for (uint32 i = 0; i < mFramesInFlight; i++) {
                NkDescSetHandle gs = (i < mGlobalSetRing.Size()) ? mGlobalSetRing[i] : NkDescSetHandle{};
                if (gs.IsValid())
                    mDevice->BindTextureSampler(gs, 27, tex, samp);
                // Phase Planar Reflection fix 2026-05-24 : aussi bind sur le
                // ring mirror sinon sample voxel=undefined sur VK.
                NkDescSetHandle gsm = (i < mGlobalSetMirrorRing.Size()) ? mGlobalSetMirrorRing[i] : NkDescSetHandle{};
                if (gsm.IsValid())
                    mDevice->BindTextureSampler(gsm, 27, tex, samp);
            }
            logger.Info("[NkRender3D] VoxelAO texture bind a set=0 binding=27\n");
        }

        // Remplace À CHAUD la boule matcap (binding 28). Permet à l'utilisateur de charger
        // sa propre texture matcap (.exr/.png décodé) et de la changer au runtime.
        void NkRender3D::SetMatcapTexture(NkTextureHandle tex) {
            NkTextureHandle bind = tex.IsValid() ? tex : mMatcapTex;   // fallback = chrome généré
            if (!bind.IsValid() || !mResources) return;
            NkSamplerHandle samp = mResources->GetSamplerLinearClamp();
            if (!samp.IsValid()) return;
            for (uint32 i = 0; i < mFramesInFlight; i++) {
                if (i < mGlobalSetRing.Size() && mGlobalSetRing[i].IsValid())
                    mDevice->BindTextureSampler(mGlobalSetRing[i], 28, bind, samp);
                if (i < mGlobalSetMirrorRing.Size() && mGlobalSetMirrorRing[i].IsValid())
                    mDevice->BindTextureSampler(mGlobalSetMirrorRing[i], 28, bind, samp);
            }
        }

        void NkRender3D::FlushIntoRT(NkICommandBuffer* cmd, NkRenderPassHandle rp,
                                     const NkMat4f& mirrorMat,
                                     const NkMat4f& mirrorViewProj,
                                     const NkVec4f& clipPlane) {
            if (!mInScene) return;

            // Sauve l'etat de scene pour permettre le Flush principal apres.
            // mObjectDrawIdx avance dans le pool ; on le rewind PAS pour que
            // les UBO de la passe miroir et celle principale ne se chevauchent.
            const bool   savedInScene  = mInScene;
            const uint32 savedSlot     = mFrameSlot;
            const uint32 savedDrawIdx  = mObjectDrawIdx;

            // Active le mode mirror : FlushOpaque/Skinned/etc. pre-multiplie chaque
            // transform par mPendingMirror. mPendingMirrorViewProj est upload au
            // CameraUBO pour permettre aux shaders qui en ont besoin (ReflFloor
            // par exemple) de connaitre la projection miroir — mais durant la
            // PASSE MIROIR elle-meme, le sol ne devrait pas etre present.
            mPendingMirror         = mirrorMat;
            mPendingMirrorActive   = true;
            mPendingMirrorViewProj = mirrorViewProj;
            mPendingClipPlane      = clipPlane;
            mPendingRP             = rp;

            Flush(cmd);

            mPendingRP             = {};
            mPendingMirror         = NkMat4f::Identity();
            mPendingMirrorActive   = false;
            mPendingMirrorViewProj = NkMat4f::Identity();
            mPendingClipPlane      = {0.f, 0.f, 0.f, 0.f};

            // Restore l'etat de scene pour que le Flush principal puisse continuer.
            // On garde mObjectDrawIdx avance pour que les UBOs deja ecrits par la
            // passe miroir ne soient pas overwrites par la passe principale (qui
            // est encore en flight cote GPU).
            mInScene       = savedInScene;
            mFrameSlot     = savedSlot;
            // mObjectDrawIdx reste avance volontairement (cf. ci-dessus).
            (void)savedDrawIdx;
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
                // iblStrength : force IBL ambient (offset 304)
                // yFlipNDC : signe a appliquer a vNDC.y dans les shaders qui
                //   reconstruisent un rayon view-space depuis le clip space
                //   (cf. Skybox). En Vulkan, le viewport est rendu Y-flipped
                //   (VkViewport.height < 0) -> top screen = vNDC.y = -1,
                //   formule viewRay.y = vNDC.y * tanHalfY donne le bon "up".
                //   En OpenGL, pas de flip viewport -> top screen = vNDC.y = +1,
                //   donc on doit inverser pour obtenir le meme viewRay.y final.
                //   +1.0 en VK, -1.0 en GL. Les shaders qui n'utilisent pas ce
                //   champ ignorent le slot.
                float32 iblStrength, yFlipNDC, viewMode, matcapId;  // viewMode:0=PBR,>0.5=SOLID ; matcapId=preset
                // Phase Planar Reflection : viewProj de la cam miroir, exposée
                // au shader ReflFloor pour calculer reflectionUV via
                // projection explicite. Les shaders qui n'utilisent pas ce
                // champ ignorent simplement les bytes après leur fin de struct.
                NkMat4f mirrorViewProj;  // offset 320 (apres 16 bytes de padding)
                // Phase M.2-ish : flag indiquant si la frame courante est rendue
                // dans une passe miroir (NkPlanarReflectionSystem). Les shaders
                // doivent skip le shadow sampling pendant ces passes car
                // vWorldPos est en espace mirror (Y inverse), ce qui fait
                // sample le shadow map a la mauvaise position -> reflets faux.
                // .x = isMirrorPass (0=normal, 1=mirror), .yzw = reserve.
                NkVec4f reflectionFlags; // offset 384
            };
            PBRCamUBO cb{};
            cb.view        = mCtx.camera.GetView();
            cb.proj        = mCtx.camera.GetProj();
            cb.viewProj    = mCtx.camera.GetViewProj();

            // Correction clip-space Z : la projection NkCamera produit un NDC Z dans
            // [-1, 1] (convention OpenGL). Vulkan ET DirectX 11/12 attendent [0, 1] —
            // sans correction, la moitie pres de la camera est silencieusement clippee
            // (Z<0) -> ecran noir (DX12) ou geometrie partiellement clippee + reflets
            // fantomes (DX11, car la proj de REFLEXION corrige deja DX, pas la principale).
            // On compose clipZ01 * proj : z_new = 0.5*z + 0.5*w. NkMat4f column-major,
            // donc m[2][2]=0.5 (m22) et m[3][2]=0.5 (m23, translation Z). Le Y-flip DX est
            // gere par le shader (output._Position.y = -y), donc on ne touche QUE Z ici.
            // OpenGL INCLUS : le backend GL force glClipControl(GL_LOWER_LEFT,
            // GL_ZERO_TO_ONE) (NkOpenglDevice.cpp) -> il attend un NDC Z en [0,1] comme
            // VK/DX, PAS le [-1,1] historique. Sans cette correction, le mapping de
            // profondeur GL est incohérent : la comparaison de profondeur devient
            // non-fiable près de la caméra (ex. la grille au sol, décalée de 2 cm,
            // était rejetée par le sol -> invisible en avant-plan sur GL alors que
            // correcte sur VK). Même famille de bug que les ombres GL.
            const auto _depthApi = mDevice ? mDevice->GetApi() : ::nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL;
            if (_depthApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_VULKAN ||
                _depthApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX11   ||
                _depthApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX12   ||
                _depthApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL) {
                NkMat4f clipZ01 = NkMat4f::Identity();
                clipZ01[2][2] = 0.5f;
                clipZ01[3][2] = 0.5f;
                cb.proj     = clipZ01 * cb.proj;
                cb.viewProj = clipZ01 * cb.viewProj;
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
            cb.viewMode    = (float32)mViewMode;  // 0=rendered(PBR) 1=solid/unlit (indépendant du wireframe)
            cb.matcapId    = (float32)mMatcapId;  // preset matcap en mode SOLID/WIREFRAME
            // yFlipNDC : UNIQUEMENT consommé par le SKYBOX (reconstruction du view-ray à
            // partir de vNDC). C'est l'orientation Y du VS PLEIN ÉCRAN du skybox, qui
            // n'a PAS d'inputs → le générateur HLSL ne le Y-négate PAS sur DX (il ne
            // négate que les VS 3D avec inputs+varyings). Donc le skybox se comporte
            // comme en OpenGL sur DX (NDC.y=+1 en haut, pas de flip) → yFlipNDC = -1.
            //   - Vulkan : viewport Y-flippé → top = vNDC.y = -1 → yFlipNDC = +1.
            //   - OpenGL ET DX11/DX12 : pas de flip du VS skybox → yFlipNDC = -1.
            // (Le précédent +1 pour DX supposait à tort que le skybox était Y-négaté
            // comme la 3D → HDR/skybox à l'envers sur DX. yFlipNDC ne touche QUE le
            // skybox ; les ombres au sol ne dépendent PAS de yFlipNDC.)
            const auto _yApi = mDevice ? mDevice->GetApi() : ::nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL;
            const bool vkViewportFlip = (_yApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_VULKAN);
            cb.yFlipNDC = vkViewportFlip ? +1.f : -1.f;
            // Phase Planar Reflection : applique aussi la correction Vulkan
            // clip-space à mirrorViewProj (sinon le sampling du RT serait clippé
            // ou inversé sur Vulkan/DX). Identity en l'absence de reflet planaire.
            cb.mirrorViewProj = mCtx.mirrorViewProj;
            const auto reflApi = mDevice ? mDevice->GetApi() : ::nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL;
            if (reflApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_VULKAN) {
                NkMat4f vkClip = NkMat4f::Identity();
                vkClip[2][2] = 0.5f;
                vkClip[3][2] = 0.5f;
                cb.mirrorViewProj = vkClip * cb.mirrorViewProj;
            } else if (reflApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX11 ||
                       reflApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX12) {
                // DX : Z-remap [-1,1]→[0,1] (comme VK) ET FLIP Y du mirrorViewProj de
                // sampling. Vérifié PAR COMPARAISON VISUELLE demo4 DX12 vs VK (caméra
                // figée NK_FIX_CAM) : SANS ce flip Y, le reflet planaire du sol échantillonne
                // la MAUVAISE région (sol au lieu du bâtiment miroir, diff lower-third ~155
                // vs VK) ; AVEC, le reflet correspond EXACTEMENT à VK (diff ~0.1). Le RT de
                // réflexion est rendu par le pipeline 3D (VS HLSL Y-négaté → RT Y-down) ; la
                // coord projetée du sol doit donc être re-flippée en Y pour adresser la bonne
                // ligne. (Le retrait précédent — commit b0d81291 — était une mauvaise
                // correction du symptôme "reflet dans l'espace", en réalité dû au HDR/skybox
                // inversé, depuis corrigé par yFlipNDC ; on rétablit donc ce flip Y.)
                NkMat4f dxClip = NkMat4f::Identity();
                dxClip[2][2] = 0.5f;
                dxClip[3][2] = 0.5f;
                dxClip[1][1] = -1.f;
                cb.mirrorViewProj = dxClip * cb.mirrorViewProj;
            }
            // Flag isMirrorPass : lu par les shaders (Layered) pour skip le
            // shadow sampling pendant les passes miroir (les positions world
            // sont en Y inverse -> sample shadow incorrect).
            cb.reflectionFlags = {mPendingMirrorActive ? 1.f : 0.f, 0.f, 0.f, 0.f};
            // Phase Planar Reflection fix : ecrire dans le bon UBO selon le mode.
            // Sans cette redirection, Flush principal (qui suit FlushIntoRT)
            // ecrasait le UBO main avec reflectionFlags=0 + view=real, perdant
            // l'etat mirror.
            if (mPendingMirrorActive) {
                if (mFrameSlot < mUBOCameraMirrorRing.Size())
                    mDevice->WriteBuffer(mUBOCameraMirrorRing[mFrameSlot], &cb, sizeof(cb));
            } else {
                if (mFrameSlot < mUBOCameraRing.Size())
                    mDevice->WriteBuffer(mUBOCameraRing[mFrameSlot], &cb, sizeof(cb));
            }

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
                // NkVSM v1 : doit matcher ObjBlock dans FlushOpaque + ObjectUBO
                // dans Init (224 bytes std140). Sinon WriteBuffer overflow.
                NkVec4f shadowOverrides;
                // 2026-05-24 Triplanar (cf ObjectUBO Init pour le sens des champs).
                NkVec4f triplanarParams;
            };
            static_assert(sizeof(ObjBlock) == 224, "ObjBlock std140 shadow");

            // Phase F.B.1 : pattern UBO-per-draw. Chaque drawcall consume un slot
            // du pool (UBO + descriptor set pre-bind 1:1). WriteBuffer fait un
            // memcpy via mapped pointer (UBO HOST_VISIBLE), legal dans renderPass
            // actif contrairement a cmd->UpdateBuffer = vkCmdUpdateBuffer
            // (VUID-vkCmdUpdateBuffer-renderpass).
            const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size())
                                     && (mFrameSlot < mObjectSetPool.Size());
            if (!poolFrameValid) return;

            // Itere sur mShadowCasters (collecte SANS culling camera) et non
            // mOpaque : sinon les casters hors champ camera n'auraient pas
            // d'ombre (cf. Submit). Tous les elements ici ont castShadow=true.
            for (auto& sdc : mShadowCasters) {
                auto& dc = sdc.dc;
                if (mObjectDrawIdx >= mObjectPoolCap) {
                    logger.Errorf("[NkRender3D] ObjectUBO pool overflow (shadow): "
                                  "drawIdx=%u >= max=%u, skipping draw\n",
                                  mObjectDrawIdx, mObjectPoolCap);
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

            // ── Ombres des instanciés (mInstanced) : 1 draw/batch ────────────────
            // Pipeline shadow INSTANCIÉ : projette N instances via lightVP + InstanceUBO
            // (set1 binding4). NE consomme PAS un slot d'ObjectUBO par instance (ce qui
            // débordait le pool) mais UN slot par batch (identité binding1 + buffer
            // d'instances binding4). Le buffer d'instances vient d'un pool dédié
            // (mUBOShadowInstPool) → pas de hazard entre les passes shadow successives.
            if (mShadowInstancePipeline.IsValid() && !mInstanced.Empty()
                && mFrameSlot < mUBOShadowInstPool.Size()) {
                cmd->BindGraphicsPipeline(mShadowInstancePipeline);
                cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f), &lightVP);

                static NkMat4f sShModels[kMaxInstancesUBO];
                for (auto& dc : mInstanced) {
                    const uint32 total = (uint32)dc.transforms.Size();
                    if (total == 0) continue;
                    for (uint32 b = 0; b < total; b += kMaxInstancesUBO) {
                        if (mObjectDrawIdx >= mObjectPoolCap) break;
                        if (mShadowInstIdx >= kShadowInstPoolCap)   break;
                        const uint32 n = (total - b < kMaxInstancesUBO) ? (total - b) : kMaxInstancesUBO;
                        for (uint32 i = 0; i < n; i++) sShModels[i] = dc.transforms[b + i];

                        // Buffer d'instances dédié (models seulement ; le VS shadow ne lit
                        // pas les tints, mais le layout InstanceUBO les prévoit → offset OK).
                        NkBufferHandle ib = mUBOShadowInstPool[mFrameSlot][mShadowInstIdx];
                        if (ib.IsValid()) mDevice->WriteBuffer(ib, sShModels, n * sizeof(NkMat4f), 0);

                        // Set objet : binding1 = identité (le VS fait uObj.model*inst[id]),
                        // binding4 = buffer d'instances. 1 slot du pool object/batch.
                        NkBufferHandle  objUbo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
                        if (objUbo.IsValid()) {
                            ObjBlock ob{};
                            ob.model        = NkMat4f::Identity();
                            ob.normalMatrix = NkMat4f::Identity();
                            ob.tint         = {1, 1, 1, 1};
                            ob.metallic = 0.f; ob.roughness = 0.5f; ob.aoStrength = 1.f;
                            mDevice->WriteBuffer(objUbo, &ob, sizeof(ob), 0);
                        }
                        NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                        if (os.IsValid() && ib.IsValid()) mDevice->BindUniformBuffer(os, 4, ib);
                        if (os.IsValid()) cmd->BindDescriptorSet(os, 1);
                        mMesh->BindMesh(cmd, dc.mesh);
                        mMesh->DrawAll(cmd, dc.mesh, n);   // 1 DRAW, n instances
                        mObjectDrawIdx++;
                        mShadowInstIdx++;
                    }
                }
            }
        }

        void NkRender3D::FlushOpaque(NkICommandBuffer* cmd) {
            // Doit matcher EXACTEMENT la layout std140 du shader pbr.{vert,frag}.gl.glsl.
            // NkVSM v1 : +vec4 shadowOverrides a la fin (224B au lieu de 192).
            //   .x = receiveShadow (0/1)
            //   .y = castShadowAlphaTest (0/1, V1 reserve)
            //   .z = shadowBiasMul
            //   .w = reserve
            // 2026-05-24 Triplanar : +vec4 triplanarParams (224B au lieu de 208).
            //   .x = tileSize en metres (0 = disabled, UV mesh classique)
            //   .y = metersPerUnit (echelle globale Blender-style)
            //   .z = enable flag (0/1)
            //   .w = reserve
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
                NkVec4f shadowOverrides;
                NkVec4f triplanarParams;
            };
            static_assert(sizeof(ObjBlock) == 224, "ObjBlock std140");

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
                if (mObjectDrawIdx >= mObjectPoolCap) {
                    logger.Errorf("[NkRender3D] ObjectUBO pool overflow (opaque): "
                                  "drawIdx=%u >= max=%u, skipping draw\n",
                                  mObjectDrawIdx, mObjectPoolCap);
                    break;
                }

                // Clip plane filter (planar reflection : ne capture que les
                // objets cote actif du plan, exclut le sol et le mauvais cote).
                if (mPendingMirrorActive) {
                    const NkVec3f n = {mPendingClipPlane.x,
                                       mPendingClipPlane.y,
                                       mPendingClipPlane.z};
                    if (n.x*n.x + n.y*n.y + n.z*n.z > 1e-6f) {
                        const NkVec3f c = (dc.aabb.min + dc.aabb.max) * 0.5f;
                        const float32 side = n.x*c.x + n.y*c.y + n.z*c.z + mPendingClipPlane.w;
                        if (side <= 0.f) continue;
                    }
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

                // Fallback : drawcall sans material custom (Demo3D, raw draws, ...).
                // Le shader PBR canonical lit tAlbedo dans set=2 binding=3 (convention
                // NkMaterialSystem) ; sans bind set=2 le sample est undefined behavior.
                // On instancie lazy une instance Default_PBR avec textures white1x1
                // et la bind systematiquement pour les drawcalls sans material.
                if (!matInst && mMat) {
                    if (!mFallbackMatInst.IsValid()) {
                        auto* inst = mMat->CreateInstance(mMat->DefaultPBR());
                        if (inst) mFallbackMatInst = inst->GetHandle();
                    }
                    matInst = mMat->GetInstance(mFallbackMatInst);
                }

                // Bind pipeline seulement si change (evite le cout vkCmdBindPipeline redondant).
                if (pipeline != lastPipeline) {
                    if (pipeline.IsValid()) cmd->BindGraphicsPipeline(pipeline);
                    lastPipeline = pipeline;
                }

                ObjBlock ob{};
                // Pre-multiplie par la mirror matrix si actif (passe RT planar
                // reflection via FlushIntoRT). Identite hors mirror.
                const NkMat4f xform = mPendingMirrorActive
                    ? (mPendingMirror * dc.transform)
                    : dc.transform;
                ob.model            = xform;
                ob.normalMatrix     = xform.Inverse().Transpose();
                ob.tint             = {dc.tint.x, dc.tint.y, dc.tint.z, dc.alpha};
                ob.metallic         = dc.metallic;
                ob.roughness        = dc.roughness;
                ob.aoStrength       = dc.aoStrength;
                ob.emissiveStrength = 0.f;
                ob.normalStrength   = 1.f;
                // clearcoat / subsurface : 0 par defaut (zero-init via ObjBlock{}).

                // NkVSM v1 : copie les shadow overrides depuis le material.
                //   .x = receiveShadow (default 1.0 = receive)
                //   .y = castShadowAlphaTest (V1 reserve)
                //   .z = shadowBiasMul (default 1.0)
                //   .w = reserve
                if (matInst) {
                    ob.shadowOverrides = NkVec4f{
                        matInst->mReceiveShadow       ? 1.f : 0.f,
                        matInst->mCastShadowAlphaTest ? 1.f : 0.f,
                        matInst->mShadowBiasMul,
                        0.f
                    };
                } else {
                    ob.shadowOverrides = NkVec4f{1.f, 0.f, 1.f, 0.f};
                }

                // 2026-05-24 Triplanar : copie tileSize material + metersPerUnit
                // global. Le shader fait UV = worldPos / (tileSize/metersPerUnit)
                // pour avoir des tiles VRAIMENT carres en metres reels meme
                // si l'echelle globale n'est pas 1m/unit.
                {
                    const float32 tile = matInst ? matInst->mTriplanarTileSize : 0.f;
                    const float32 mpu  = NkUnits().metersPerUnit;
                    ob.triplanarParams = NkVec4f{
                        tile,
                        mpu > 0.f ? mpu : 1.f,
                        tile > 0.f ? 1.f : 0.f,
                        0.f
                    };
                }

                NkBufferHandle  ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
                NkDescSetHandle os  = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                if (ubo.IsValid()) mDevice->WriteBuffer(ubo, &ob, sizeof(ob), 0);
                if (os.IsValid())  cmd->BindDescriptorSet(os, 1);

                // Phase M.8 : multi-material par sous-mesh (style Blender/glTF).
                // Si materialSlots non-vide, iterer les sous-meshes du mesh et
                // bind materialSlots[i] avant chaque DrawSubMesh. Sinon comportement
                // single-material standard (matInst pour tout le mesh).
                //
                // Sur OpenGL, BindGraphicsPipeline change le VAO/program courant
                // -> BindMesh (VBO+IBO) DOIT etre rappele apres chaque switch
                // de pipeline pour rattacher les buffers au nouveau VAO. C'est
                // pourquoi BindMesh est dans la boucle (et non avant comme en
                // single-material classic).
                if (!dc.materialSlots.Empty() && mMesh) {
                    const uint32 nSubs = mMesh->GetSubMeshCount(dc.mesh);
                    for (uint32 si = 0; si < nSubs; si++) {
                        NkMaterialInstance* sInst = matInst;  // fallback global
                        if (si < dc.materialSlots.Size()
                            && dc.materialSlots[si].IsValid()) {
                            auto* cand = mMat->GetInstance(dc.materialSlots[si]);
                            if (cand) sInst = cand;
                        }
                        bool pipelineChanged = false;
                        if (sInst) {
                            NkPipelineHandle sPipeline = mMat->GetPipeline(sInst->GetTemplate());
                            if (sPipeline.IsValid() && sPipeline != lastPipeline) {
                                cmd->BindGraphicsPipeline(sPipeline);
                                lastPipeline = sPipeline;
                                pipelineChanged = true;
                            }
                            mMat->BindInstance(cmd, sInst);
                        }
                        // Bind mesh : la 1re iteration ou chaque switch de pipeline.
                        if (si == 0 || pipelineChanged)
                            mMesh->BindMesh(cmd, dc.mesh);
                        mMesh->DrawSubMesh(cmd, dc.mesh, si);
                    }
                } else {
                    // Single-material : bind l'instance globale + DrawAll/SubMesh.
                    if (matInst) mMat->BindInstance(cmd, matInst);
                    mMesh->BindMesh(cmd, dc.mesh);
                    if (dc.subMeshIdx == 0xFFFFFFFFu)
                        mMesh->DrawAll(cmd, dc.mesh);
                    else
                        mMesh->DrawSubMesh(cmd, dc.mesh, dc.subMeshIdx);
                }
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
            // ObjBlock std140 (224 octets) — layout commun aux deux chemins (binding 1).
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
                NkVec4f shadowOverrides;
                NkVec4f triplanarParams;
            };
            static_assert(sizeof(ObjBlock) == 224, "ObjBlock std140 instanced");
            // ObjBlock identité : le shader instancié fait worldPos = uObj.model *
            // inst.models[id] * pos ; avec model=identité, l'instance porte tout.
            auto MakeIdentityObj = []() {
                ObjBlock ob{};
                ob.model = NkMat4f::Identity();
                ob.normalMatrix = NkMat4f::Identity();
                ob.tint = {1.f, 1.f, 1.f, 1.f};
                ob.metallic = 0.f; ob.roughness = 0.5f; ob.aoStrength = 1.f;
                ob.normalStrength = 1.f;
                ob.shadowOverrides = NkVec4f{1.f, 0.f, 1.f, 0.f};
                const float32 mpu = NkUnits().metersPerUnit;
                ob.triplanarParams = NkVec4f{0.f, mpu > 0.f ? mpu : 1.f, 0.f, 0.f};
                return ob;
            };

            // ── Chemin GPU 1-draw (actif si le pipeline instancié a été créé, lui-
            //    même opt-in NK_INSTANCING_GPU). Sinon expansion object-UBO ci-après.
            //    Mirror EXACT du skin : set objet = binding1 (ObjectUBO identité) +
            //    binding2 (buffer d'instances). Indispensable pour que la root sig DX
            //    matche le set objet pré-câblé (sinon instances absentes sur GL/DX).
            if (mInstancePipeline.IsValid()
                && mFrameSlot < mUBOInstanceRing.Size()
                && mFrameSlot < mUBOObjectPool.Size()
                && mFrameSlot < mObjectSetPool.Size()) {

                cmd->BindGraphicsPipeline(mInstancePipeline);
                NkBufferHandle instBuf = mUBOInstanceRing[mFrameSlot];
                NkDescSetHandle gs = (mFrameSlot < mGlobalSetRing.Size())
                                   ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};

                static NkMat4f sModels[kMaxInstancesUBO];
                static NkVec4f sTints [kMaxInstancesUBO];

                for (auto& dc : mInstanced) {
                    const uint32 total = (uint32)dc.transforms.Size();
                    if (total == 0) continue;

                    NkMaterialInstance* matInst = nullptr;
                    if (dc.material.IsValid() && mMat) matInst = mMat->GetInstance(dc.material);
                    if (!matInst && mMat) {
                        if (!mFallbackMatInst.IsValid()) {
                            auto* in = mMat->CreateInstance(mMat->DefaultPBR());
                            if (in) mFallbackMatInst = in->GetHandle();
                        }
                        matInst = mMat->GetInstance(mFallbackMatInst);
                    }

                    for (uint32 b = 0; b < total; b += kMaxInstancesUBO) {
                        if (mObjectDrawIdx >= mObjectPoolCap) break;
                        const uint32 n = (total - b < kMaxInstancesUBO) ? (total - b) : kMaxInstancesUBO;
                        for (uint32 i = 0; i < n; i++) {
                            sModels[i] = dc.transforms[b + i];
                            const NkVec3f t = (b + i < (uint32)dc.tints.Size()) ? dc.tints[b + i] : NkVec3f{1,1,1};
                            sTints[i] = NkVec4f{ t.x, t.y, t.z, 1.f };
                        }
                        if (instBuf.IsValid()) {
                            mDevice->WriteBuffer(instBuf, sModels, n * sizeof(NkMat4f), 0);
                            mDevice->WriteBuffer(instBuf, sTints,  n * sizeof(NkVec4f),
                                                 kMaxInstancesUBO * sizeof(NkMat4f));
                        }

                        // ObjectUBO identité au binding 1 (le pool pré-câble binding 1
                        // -> ce buffer). Sans données valides ici, uObj.model serait nul
                        // -> cubes collapse. Mirror du skin (Object + bones tous deux liés).
                        NkBufferHandle  objUbo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
                        if (objUbo.IsValid()) {
                            ObjBlock ob = MakeIdentityObj();
                            mDevice->WriteBuffer(objUbo, &ob, sizeof(ob), 0);
                        }

                        NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                        // Buffer d'instances bindé au set objet, binding 2 (comme les bones).
                        if (os.IsValid() && instBuf.IsValid()) mDevice->BindUniformBuffer(os, 4, instBuf);  // binding 4 (anti-collision GL/DX)
                        if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);   // caméra/lumières
                        if (os.IsValid()) cmd->BindDescriptorSet(os, 1);
                        if (matInst)      mMat->BindInstance(cmd, matInst); // textures (set 2)
                        mMesh->BindMesh(cmd, dc.mesh);
                        mMesh->DrawAll(cmd, dc.mesh, n);   // 1 DRAW, n instances
                        mObjectDrawIdx++;
                    }
                }
                return;
            }

            // ── Instancing CORRECT (2026-06-30) ───────────────────────────────────
            // L'ancienne version emettait DrawAll(count) SANS jamais transmettre les
            // transforms par instance au shader -> les N instances se dessinaient
            // SUPERPOSEES (au transform de l'ObjectUBO courant). Bug.
            //
            // Ici on EXPANSE chaque instance via le chemin object-UBO PBR deja
            // prouve (cf. FlushOpaque) : materiau + mesh lies UNE fois par drawcall,
            // puis pour chaque instance on ecrit son model dans un slot d'ObjectUBO
            // et on dessine -> instances correctement placees ET eclairees.
            // (Optimisation future : vrai GPU instancing 1-draw via buffer SSBO +
            //  VS instancie lisant gl_InstanceIndex.)
            const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size())
                                     && (mFrameSlot < mObjectSetPool.Size());
            if (!poolFrameValid) return;

            NkPipelineHandle lastPipeline = mPBRPipeline;

            for (auto& dc : mInstanced) {
                const uint32 n = (uint32)dc.transforms.Size();
                if (n == 0) continue;

                // Materiau + pipeline : resolus UNE fois pour tout le drawcall.
                NkMaterialInstance* matInst  = nullptr;
                NkPipelineHandle    pipeline = mPBRPipeline;
                if (dc.material.IsValid() && mMat) {
                    matInst = mMat->GetInstance(dc.material);
                    if (matInst) {
                        NkPipelineHandle p = mMat->GetPipeline(matInst->GetTemplate());
                        if (p.IsValid()) pipeline = p;
                    }
                }
                if (!matInst && mMat) {
                    if (!mFallbackMatInst.IsValid()) {
                        auto* inst = mMat->CreateInstance(mMat->DefaultPBR());
                        if (inst) mFallbackMatInst = inst->GetHandle();
                    }
                    matInst = mMat->GetInstance(mFallbackMatInst);
                }
                if (pipeline != lastPipeline) {
                    if (pipeline.IsValid()) cmd->BindGraphicsPipeline(pipeline);
                    lastPipeline = pipeline;
                }
                if (matInst) mMat->BindInstance(cmd, matInst);
                mMesh->BindMesh(cmd, dc.mesh);

                // Une instance = un slot d'ObjectUBO + un draw (mesh deja lie).
                for (uint32 i = 0; i < n; ++i) {
                    if (mObjectDrawIdx >= mObjectPoolCap) {
                        logger.Errorf("[NkRender3D] ObjectUBO pool overflow (instanced): "
                                      "drawIdx=%u >= max=%u\n", mObjectDrawIdx, mObjectPoolCap);
                        break;
                    }
                    const NkMat4f& m = dc.transforms[i];

                    ObjBlock ob{};
                    ob.model            = m;
                    ob.normalMatrix     = m.Inverse().Transpose();
                    const NkVec3f t     = (i < (uint32)dc.tints.Size()) ? dc.tints[i] : NkVec3f{1,1,1};
                    ob.tint             = {t.x, t.y, t.z, 1.f};
                    ob.metallic = 0.f; ob.roughness = 0.5f; ob.aoStrength = 1.f;
                    ob.normalStrength   = 1.f;
                    ob.shadowOverrides  = NkVec4f{1.f, 0.f, 1.f, 0.f};
                    const float32 mpu   = NkUnits().metersPerUnit;
                    ob.triplanarParams  = NkVec4f{0.f, mpu > 0.f ? mpu : 1.f, 0.f, 0.f};

                    NkBufferHandle  ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
                    NkDescSetHandle os  = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                    if (ubo.IsValid()) mDevice->WriteBuffer(ubo, &ob, sizeof(ob), 0);
                    if (os.IsValid())  cmd->BindDescriptorSet(os, 1);
                    mMesh->DrawAll(cmd, dc.mesh);   // 1 instance, a SA position
                    mObjectDrawIdx++;
                }
            }
        }

        void NkRender3D::FlushSkinned(NkICommandBuffer* cmd) {
            const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size())
                                     && (mFrameSlot < mObjectSetPool.Size());
            if (!poolFrameValid) return;
            if (mSkinned.Empty()) return;

            // Le pipeline skin doit etre lie AVANT tout draw skinne (il a son
            // propre vertex layout NkVertexSkinned + lit l'UBO de bones).
            // Sans ce bind, FlushSkinned utilisait le dernier pipeline (PBR) ->
            // aucun skinning (bug d'origine). No-op si pipeline non cree.
            if (!mSkinPipeline.IsValid()) {
                logger.Warnf("[NkRender3D] FlushSkinned: pipeline skin invalide, "
                             "skip %u draws skinnes\n", (uint32)mSkinned.Size());
                return;
            }
            cmd->BindGraphicsPipeline(mSkinPipeline);

            // ── DIAGNOSTIC gated (NK_SKIN_DIAG) ──────────────────────────────
            // Preuve indirecte (sans capture pixel) que le skinning atteint le
            // GPU : pipeline skin valide + bones ring effectivement uploade
            // (lecture-retour du 1er bone : doit etre != 0). Sur les frames
            // suivantes les valeurs changent (anim) -> confirme la deformation.
            {
                static int sdiag = -1;
                static uint32 sframe = 0;
                if (sdiag == -1) { const char* v = getenv("NK_SKIN_DIAG"); sdiag = (v && v[0] && v[0]!='0') ? 1 : 0; }
                if (sdiag && sframe < 80 && !mSkinned.Empty()) {
                    NkBufferHandle rb = (mFrameSlot < mUBOBonesRing.Size()) ? mUBOBonesRing[mFrameSlot] : NkBufferHandle{};
                    // Upload AVANT readback pour lire la donnee de CETTE frame.
                    if (rb.IsValid() && mSkinned[0].boneMatrices.Size() >= 2) {
                        uint32 n = (uint32)mSkinned[0].boneMatrices.Size(); if (n>kMaxBonesUBO) n=kMaxBonesUBO;
                        mDevice->WriteBuffer(rb, mSkinned[0].boneMatrices.Data(), n*sizeof(NkMat4f));
                        // bone[1] = joint anime (SimpleSkin). On lit sa translation
                        // (m12,m13,m14) DEPUIS LE BUFFER GPU. Si elle CHANGE entre
                        // frames -> la matrice animee atteint bien le GPU -> le VS
                        // skin (qui lit ce meme buffer en SRV t0) deforme le mesh.
                        NkMat4f m1{}; bool ok = mDevice->ReadBuffer(rb, &m1, sizeof(NkMat4f), sizeof(NkMat4f));
                        // Log seulement toutes les ~12 frames pour rester lisible.
                        if ((sframe % 12) == 0)
                            logger.Info("[SKIN_DIAG] frame={0} slot={1} pipeline_valid={2} draws={3} readbackGPU={4} bone1_translate=({5}, {6}, {7})\n",
                                        sframe, mFrameSlot, mSkinPipeline.IsValid()?1:0,
                                        (uint32)mSkinned.Size(), ok?1:0,
                                        m1.data[12], m1.data[13], m1.data[14]);
                    }
                    sframe++;
                }
            }

            // Set global (set=0, camera/lights/env/shadow) du frame courant.
            // Re-bind PAR DRAW apres le re-bind du pipeline skin (cf. plus bas) :
            // sur DX12, changer de PSO/root signature (BindGraphicsPipeline)
            // invalide TOUS les root parameters -> les sets bindes AVANT le
            // dernier BindGraphicsPipeline sont perdus. Sur OpenGL le re-bind
            // est inoffensif (idempotent).
            NkDescSetHandle gs = mPendingMirrorActive
                ? ((mFrameSlot < mGlobalSetMirrorRing.Size()) ? mGlobalSetMirrorRing[mFrameSlot] : NkDescSetHandle{})
                : ((mFrameSlot < mGlobalSetRing.Size())       ? mGlobalSetRing[mFrameSlot]       : NkDescSetHandle{});

            // Ring UBO bones du frame courant (BUG Vulkan flicker).
            NkBufferHandle bonesBuf = (mFrameSlot < mUBOBonesRing.Size())
                                    ? mUBOBonesRing[mFrameSlot] : NkBufferHandle{};

            // Materiau pour set=2 (le frag skin sample tAlbedo/tNormal/tORM/
            // tEmissive). Fallback Default_PBR (textures white/normal 1x1) si le
            // drawcall n'a pas de materiau custom.
            NkMaterialInstance* fallback = nullptr;
            if (mMat) {
                if (!mFallbackMatInst.IsValid()) {
                    auto* inst = mMat->CreateInstance(mMat->DefaultPBR());
                    if (inst) mFallbackMatInst = inst->GetHandle();
                }
                fallback = mMat->GetInstance(mFallbackMatInst);
            }

            // UBO bones = mat4 bones[64] (std140). On clamp a 64 ; les squelettes
            // plus grands sont tronques (les indices > 63 sont clampes a 63 cote
            // shader). On ecrit toujours kMaxBonesUBO mat4 (le buffer fait cette
            // taille) avec identite au-dela de `count` pour eviter des matrices
            // stale d'un draw precedent qui collapseraient des vertices.
            const uint32 kMaxBones = kMaxBonesUBO;   // taille de l'UBO alloue a Init
            NkMat4f bonesScratch[kMaxBonesUBO];
            for (auto& dc : mSkinned) {
                if (dc.boneMatrices.Empty()) continue;
                if (mObjectDrawIdx >= mObjectPoolCap) {
                    logger.Errorf("[NkRender3D] ObjectUBO pool overflow (skinned): "
                                  "drawIdx={0} >= max={1}, skipping draw\n",
                                  mObjectDrawIdx, mObjectPoolCap);
                    break;
                }
                uint32 count=(uint32)dc.boneMatrices.Size();
                if (count > kMaxBones) count = kMaxBones;
                if (bonesBuf.IsValid()) {
                    // Remplit le scratch : [0,count) = bones du draw, [count,64) =
                    // identite (pad). Upload des 64 mat4 d'un coup (taille fixe UBO).
                    for (uint32 b=0; b<count; b++)        bonesScratch[b] = dc.boneMatrices[b];
                    for (uint32 b=count; b<kMaxBones; b++) bonesScratch[b] = NkMat4f::Identity();
                    mDevice->WriteBuffer(bonesBuf, bonesScratch,
                                            kMaxBones*sizeof(NkMat4f));
                }

                // ObjectUBO du draw (set=1, binding=1) : ecrit AVANT BindInstance
                // (WriteBuffer = memcpy mapped, legal a tout moment ; aucun bind
                // d'etat). Le set=1 lui-meme est re-bind plus bas, apres le skin.
                struct ObjB { NkMat4f m,nm; NkVec4f tint; float32 p[8]; } ob{};
                ob.m=dc.transform; ob.nm=dc.transform.Inverse().Transpose();
                ob.tint={dc.tint.x,dc.tint.y,dc.tint.z,dc.alpha};
                NkBufferHandle  ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
                NkDescSetHandle os  = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                if (ubo.IsValid()) mDevice->WriteBuffer(ubo,&ob,sizeof(ob));

                // Materiau du draw (set=2). Custom si fourni, sinon fallback.
                NkMaterialInstance* matInst = fallback;
                if (dc.material.IsValid() && mMat) {
                    auto* cand = mMat->GetInstance(dc.material);
                    if (cand) matInst = cand;
                }

                // Phase M.8 (skinne) : multi-material par sous-mesh. Si
                // materialSlots non-vide, on dessine CHAQUE sous-mesh avec son
                // propre materiau glTF (BrainStem = 59 primitives, 59 couleurs).
                // Sinon comportement mono-draw historique (matInst pour tout).
                //
                // Note skinne : le PIPELINE reste TOUJOURS mSkinPipeline (vertex
                // layout NkVertexSkinned + program skin) ; seul le descriptor
                // set=2 (materiau : factors + textures) change par sous-mesh. On
                // ne bind donc pas le pipeline PBR du materiau ici — uniquement
                // BindInstance (upload UBO/textures si dirty) + re-bind du
                // skin/sets dans l'ordre DX12 (set0 global, set2 mat, set1 objet).
                const bool multiMat = !dc.materialSlots.Empty() && mMesh;

                if (multiMat) {
                    const uint32 nSubs = mMesh->GetSubMeshCount(dc.mesh);
                    static int sslot = -1;
                    if (sslot == -1) { const char* v = getenv("NK_SKIN_DIAG"); sslot = (v && v[0] && v[0]!='0') ? 1 : 0; }
                    const NkMat4f dnm = dc.transform.Inverse().Transpose();
                    for (uint32 si = 0; si < nSubs; ++si) {
                        if (mObjectDrawIdx >= mObjectPoolCap) break;
                        NkMaterialInstance* sInst = matInst;  // fallback global
                        if (si < dc.materialSlots.Size()
                            && dc.materialSlots[si].IsValid()) {
                            auto* cand = mMat->GetInstance(dc.materialSlots[si]);
                            if (cand) sInst = cand;
                        }
                        // ObjectUBO PROPRE au sous-mesh : le frag skin n'a pas d'UBO
                        // materiau (set=2 = textures uniquement), il lit l'albedo via
                        // uObj.tint -> vColor. On met donc tint = baseColorFactor du
                        // materiau du sous-mesh (x dc.tint) pour que CHAQUE sous-mesh
                        // prenne sa couleur (BrainStem = 59 materiaux unis distincts).
                        NkVec4f alb = {1.f,1.f,1.f,1.f};
                        struct ObjB2 { NkMat4f m,nm; NkVec4f tint; float32 p[8]; } sob{};
                        sob.m = dc.transform; sob.nm = dnm;
                        // Params PBR du materiau (le frag skin lit metallic/roughness/
                        // aoStrength/... depuis uObj). Sans ca p[8]=0 -> ao=0 ->
                        // aucun ambient -> modele sombre + bords qui bloom (glow).
                        if (sInst) {
                            const NkPBRParams& pbr = sInst->GetPBR();
                            alb = pbr.albedo;
                            sob.p[0]=pbr.metallic;          sob.p[1]=pbr.roughness;
                            sob.p[2]=pbr.ao;                sob.p[3]=pbr.emissiveStrength;
                            sob.p[4]=pbr.normalStrength;    sob.p[5]=pbr.clearcoat;
                            sob.p[6]=pbr.clearcoatRough;    sob.p[7]=pbr.subsurface;
                        } else {
                            sob.p[2]=1.f; sob.p[1]=0.5f;    // ao=1, roughness=0.5 par defaut
                        }
                        sob.tint = { alb.x*dc.tint.x, alb.y*dc.tint.y,
                                     alb.z*dc.tint.z, dc.alpha*alb.w };
                        NkBufferHandle  subUbo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
                        NkDescSetHandle subOs  = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
                        if (subUbo.IsValid()) mDevice->WriteBuffer(subUbo, &sob, sizeof(sob));

                        // Met a jour le descriptor du materiau du sous-mesh.
                        if (sInst && mMat) mMat->BindInstance(cmd, sInst);

                        // BindInstance a rebinde le pipeline PBR (mauvais layout)
                        // -> on re-bind le pipeline skin + TOUS les sets a chaque
                        // sous-mesh (correctif DX12 root-params).
                        cmd->BindGraphicsPipeline(mSkinPipeline);
                        if (gs.IsValid())      cmd->BindDescriptorSet(gs, 0);
                        if (sInst && sInst->GetDescSet().IsValid())
                            cmd->BindDescriptorSet(sInst->GetDescSet(), 2);
                        if (subOs.IsValid())   cmd->BindDescriptorSet(subOs, 1);

                        mMesh->BindMesh(cmd, dc.mesh);
                        mMesh->DrawSubMesh(cmd, dc.mesh, si);
                        mObjectDrawIdx++;
                    }
                    if (sslot) {
                        static uint32 mmF = 0;
                        if (mmF == 0 && nSubs > 0) {
                            NkMaterialInstance* s0 = (dc.materialSlots.Size()>0 && dc.materialSlots[0].IsValid())
                                                   ? mMat->GetInstance(dc.materialSlots[0]) : nullptr;
                            NkVec4f a0 = s0 ? s0->GetPBR().albedo : NkVec4f{1,1,1,1};
                            logger.Info("[SKIN_MULTIMAT] {0} sous-meshes, sub0 albedo=({1},{2},{3})\n",
                                        nSubs, a0.x, a0.y, a0.z);
                        }
                        mmF++;
                    }
                    continue;
                }

                // BindInstance MET A JOUR le descriptor materiau (upload UBO +
                // textures si dirty) ET bind le pipeline PBR du materiau + le
                // set=2. On l'appelle pour l'effet de mise a jour ; le pipeline
                // PBR et les sets qu'il bind seront ECRASES juste apres.
                if (matInst && mMat) mMat->BindInstance(cmd, matInst);

                // CORRECTIF SKIN GPU + DX12 root-params : BindInstance rebinde le
                // pipeline PBR du materiau (layout NkVertex3D stride 56, SANS les
                // attributs aBoneIdx/aBoneWeight loc 6/7 et SANS le program skin).
                // On RE-BIND donc le pipeline skin. Sur DX12, ce changement de
                // PSO/root-signature INVALIDE tous les root parameters -> il faut
                // re-binder TOUS les sets APRES, dans l'ordre :
                //   set=0 (global), set=2 (materiau via GetDescSet), set=1 (objet).
                // Sans ca, set=0 (camera) et set=2 (textures) bindes avant le skin
                // sont perdus -> draw skin avec descriptors invalides -> rien ne
                // s'affiche sur DX11/DX12 (skin invisible). Sur OpenGL c'est
                // idempotent (deja fonctionnel).
                cmd->BindGraphicsPipeline(mSkinPipeline);
                if (gs.IsValid())     cmd->BindDescriptorSet(gs, 0);
                if (matInst && matInst->GetDescSet().IsValid())
                    cmd->BindDescriptorSet(matInst->GetDescSet(), 2);
                if (os.IsValid())     cmd->BindDescriptorSet(os, 1);

                mMesh->BindMesh(cmd,dc.mesh);
                mMesh->DrawAll(cmd,dc.mesh);
                mObjectDrawIdx++;
            }
        }

        bool NkRender3D::EnsureDebugLinePipeline(NkRenderPassHandle currentRP) {
            if (mLinePipeline.IsValid() && mLinePipelineRP == currentRP) return true;
            if (!mShaderLib) return false;
            if (!mLineShader.IsValid()) {
                auto prog = mShaderLib->LoadOrCompileVF("DebugLine", "", "");
                if (!prog.IsValid()) { logger.Errorf("[NkR3D::DebugLine] shader compile FAIL\n"); return false; }
                mLineShader = mShaderLib->GetRHIHandle(prog);
                if (!mLineShader.IsValid()) { logger.Errorf("[NkR3D::DebugLine] RHI handle FAIL\n"); return false; }
            }
            if (mLinePipeline.IsValid())        { mDevice->DestroyPipeline(mLinePipeline);        mLinePipeline = {}; }
            if (mLinePipelineNoDepth.IsValid()) { mDevice->DestroyPipeline(mLinePipelineNoDepth); mLinePipelineNoDepth = {}; }

            NkGraphicsPipelineDesc pd;
            pd.shader       = mLineShader;
            pd.depthStencil = NkDepthStencilDesc::Default();   // depth test ON (lignes occluses)
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.blend        = NkBlendDesc::Opaque();
            pd.topology     = NkPrimitiveTopology::NK_LINE_LIST;
            pd.debugName    = "DebugLine";
            pd.renderPass   = currentRP;
            pd.descriptorSetLayouts.PushBack(mGlobalLayout);   // set 0 = CameraUBO
            // vertex : pos vec3 (off 0) + couleur vec4 (off 12), stride 28
            pd.vertexLayout
              .AddBinding(0, 28, false)
              .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT,  0,  "POSITION", 0)
              .AddAttribute(1, 0, NkVertexFormat::NK_RGBA32_FLOAT, 12, "COLOR",    0);
            mLinePipeline   = mDevice->CreateGraphicsPipeline(pd);
            mLinePipelineRP = currentRP;

            // Variante OVERLAY : mêmes réglages mais depth-test OFF -> lignes toujours
            // au-dessus de la scène (gizmos/marqueurs éditeur, façon Blender).
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.debugName    = "DebugLineOverlay";
            mLinePipelineNoDepth = mDevice->CreateGraphicsPipeline(pd);

            logger.Info("[NkRender3D] DebugLine pipeline create: shader_valid={0} pipeline_valid={1} overlay_valid={2} rp.id={3}\n",
                        mLineShader.IsValid() ? 1 : 0, mLinePipeline.IsValid() ? 1 : 0,
                        mLinePipelineNoDepth.IsValid() ? 1 : 0, currentRP.id);
            return mLinePipeline.IsValid();
        }

        // Pipelines pour les TRIANGLES debug pleins (surlignage de faces, etc.) :
        // même shader/layout que les lignes (pos vec3 + couleur vec4), mais topologie
        // TRIANGLE_LIST + blend ALPHA. Variante depth (occluse) + overlay (au-dessus).
        bool NkRender3D::EnsureDebugTriOverlayPipeline(NkRenderPassHandle currentRP) {
            if (mTriPipeline.IsValid() && mTriPipelineRP == currentRP) return true;
            if (!EnsureDebugLinePipeline(currentRP)) return false;   // garantit mLineShader
            if (mTriPipeline.IsValid())        { mDevice->DestroyPipeline(mTriPipeline);        mTriPipeline = {}; }
            if (mTriPipelineNoDepth.IsValid()) { mDevice->DestroyPipeline(mTriPipelineNoDepth); mTriPipelineNoDepth = {}; }

            NkGraphicsPipelineDesc pd;
            pd.shader       = mLineShader;
            // Surbrillance de face façon Blender : UN seul triangle coplanaire visible des
            // DEUX CÔTÉS. depthCompareOp=LESS_EQUAL (le coplanaire passe) + lecture seule
            // (pas d'occlusion mutuelle) + biais NÉGATIF (tire vers la caméra) -> gagne le
            // z-fight contre sa propre surface quel que soit le côté regardé, tout en
            // restant occlus par les AUTRES objets devant.
            pd.depthStencil = NkDepthStencilDesc::Default();
            pd.depthStencil.depthCompareOp   = NkCompareOp::NK_LESS_EQUAL;
            pd.depthStencil.depthWriteEnable = false;
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            pd.rasterizer.depthBiasConst = -1.5f;
            pd.rasterizer.depthBiasSlope = -1.5f;
            pd.blend        = NkBlendDesc::Alpha();
            pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.debugName    = "DebugTriFill";
            pd.renderPass   = currentRP;
            pd.descriptorSetLayouts.PushBack(mGlobalLayout);
            pd.vertexLayout
              .AddBinding(0, 28, false)
              .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT,  0,  "POSITION", 0)
              .AddAttribute(1, 0, NkVertexFormat::NK_RGBA32_FLOAT, 12, "COLOR",    0);
            mTriPipeline   = mDevice->CreateGraphicsPipeline(pd);
            mTriPipelineRP = currentRP;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.debugName    = "DebugTriFillOverlay";
            mTriPipelineNoDepth = mDevice->CreateGraphicsPipeline(pd);
            return mTriPipeline.IsValid();
        }

        void NkRender3D::FlushDebug(NkICommandBuffer* cmd, NkRenderPassHandle currentRP,
                                    NkDescSetHandle gs) {
            // ── Edit overlay PERSISTANT (cage/faces/points) : rendu chaque frame depuis
            //    des buffers GPU gardés (aucune reconstruction CPU tant que rien ne
            //    change). Faces/points d'abord (fill), puis la cage par-dessus. ────────
            if ((mEditTriN || mEditPointN) && EnsureDebugTriOverlayPipeline(currentRP)) {
                NkPipelineHandle tp = mEditOverlayNoDepth ? mTriPipelineNoDepth : mTriPipeline;
                if (tp.IsValid()) {
                    cmd->BindGraphicsPipeline(tp);
                    if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);
                    if (mEditTriN)   { cmd->BindVertexBuffer(0, mEditTriBuf,   0); cmd->Draw(mEditTriN); }
                    if (mEditPointN) { cmd->BindVertexBuffer(0, mEditPointBuf, 0); cmd->Draw(mEditPointN); }
                }
            }
            if (mEditLineN && EnsureDebugLinePipeline(currentRP)) {
                NkPipelineHandle lp = mEditOverlayNoDepth ? mLinePipelineNoDepth : mLinePipeline;
                if (lp.IsValid()) {
                    cmd->BindGraphicsPipeline(lp);
                    if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);
                    cmd->BindVertexBuffer(0, mEditLineBuf, 0);
                    cmd->Draw(mEditLineN);
                }
            }

            if (mDebugLines.Empty() && mDebugTris.Empty()) return;

            // ── 0. TRIANGLES debug pleins (surlignage de faces) — AVANT les lignes,
            //        pour que la cage/les points passent par-dessus le fill. ────────
            if (!mDebugTris.Empty() && EnsureDebugTriOverlayPipeline(currentRP)) {
                struct LV { float x, y, z, r, g, b, a; };
                NkVector<LV> tv; tv.Reserve(mDebugTris.Size() * 3);
                auto emitT = [&](const DebugTri& t){
                    tv.PushBack({t.a.x,t.a.y,t.a.z, t.color.x,t.color.y,t.color.z,t.color.w});
                    tv.PushBack({t.b.x,t.b.y,t.b.z, t.color.x,t.color.y,t.color.z,t.color.w});
                    tv.PushBack({t.c.x,t.c.y,t.c.z, t.color.x,t.color.y,t.color.z,t.color.w});
                };
                for (uint32 i=0;i<mDebugTris.Size();++i) if (!mDebugTris[i].overlay) emitT(mDebugTris[i]);
                const uint32 tNormal = (uint32)tv.Size();
                for (uint32 i=0;i<mDebugTris.Size();++i) if ( mDebugTris[i].overlay) emitT(mDebugTris[i]);
                const uint32 tcount = (uint32)tv.Size();
                const uint32 tOverlay = tcount - tNormal;
                if (tcount > 0) {
                    if (!mTriVBO.IsValid() || mTriVBOCapVerts < tcount) {
                        if (mTriVBO.IsValid()) mDevice->DestroyBuffer(mTriVBO);
                        const uint32 cap = tcount + 256;
                        mTriVBO = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)cap * sizeof(LV)));
                        mTriVBOCapVerts = cap;
                    }
                    mDevice->WriteBuffer(mTriVBO, tv.Data(), (uint64)tcount * sizeof(LV), 0);
                    if (tNormal > 0) {
                        cmd->BindGraphicsPipeline(mTriPipeline);
                        if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);
                        cmd->BindVertexBuffer(0, mTriVBO, 0);
                        cmd->Draw(tNormal);
                    }
                    if (tOverlay > 0 && mTriPipelineNoDepth.IsValid()) {
                        cmd->BindGraphicsPipeline(mTriPipelineNoDepth);
                        if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);
                        cmd->BindVertexBuffer(0, mTriVBO, (uint64)tNormal * sizeof(LV));
                        cmd->Draw(tOverlay);
                    }
                }
                // Purge O(n) (mêmes règles que les lignes).
                uint32 tkeep = 0;
                for (uint32 i=0;i<(uint32)mDebugTris.Size();++i) {
                    if (mDebugTris[i].life <= 0.f) continue;
                    mDebugTris[i].life -= mCtx.deltaTime;
                    if (mDebugTris[i].life <= 0.f) continue;
                    if (tkeep != i) mDebugTris[tkeep] = mDebugTris[i];
                    ++tkeep;
                }
                mDebugTris.Resize(tkeep);
            }

            if (mDebugLines.Empty()) return;

            // ── 1. RENDU des lignes courantes ────────────────────────────────────
            if (EnsureDebugLinePipeline(currentRP)) {
                // Vertices : 2 par ligne (a,b) avec la couleur. Stride 28.
                // On range les lignes NORMALES (depth ON) d'abord, puis les lignes
                // OVERLAY (depth OFF), dans le MÊME VBO -> deux Draw depuis un offset.
                struct LV { float x, y, z, r, g, b, a; };
                NkVector<LV> verts;
                verts.Reserve(mDebugLines.Size() * 2);
                auto emit = [&](const DebugLine& l){
                    verts.PushBack({ l.a.x, l.a.y, l.a.z, l.color.x, l.color.y, l.color.z, l.color.w });
                    verts.PushBack({ l.b.x, l.b.y, l.b.z, l.color.x, l.color.y, l.color.z, l.color.w });
                };
                for (uint32 i = 0; i < mDebugLines.Size(); ++i) if (!mDebugLines[i].overlay) emit(mDebugLines[i]);
                const uint32 vNormal = (uint32)verts.Size();
                for (uint32 i = 0; i < mDebugLines.Size(); ++i) if ( mDebugLines[i].overlay) emit(mDebugLines[i]);
                const uint32 vcount  = (uint32)verts.Size();
                const uint32 vOverlay = vcount - vNormal;
                const uint64 bytes   = (uint64)vcount * sizeof(LV);

                // (Re)créer le VBO dynamique si trop petit, puis uploader.
                if (!mLineVBO.IsValid() || mLineVBOCapVerts < vcount) {
                    if (mLineVBO.IsValid()) mDevice->DestroyBuffer(mLineVBO);
                    const uint32 cap = vcount + 256;
                    mLineVBO = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)cap * sizeof(LV)));
                    mLineVBOCapVerts = cap;
                }
                mDevice->WriteBuffer(mLineVBO, verts.Data(), bytes, 0);

                // Lot 1 : lignes normales (depth-test ON).
                if (vNormal > 0) {
                    cmd->BindGraphicsPipeline(mLinePipeline);
                    if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);
                    cmd->BindVertexBuffer(0, mLineVBO, 0);
                    cmd->Draw(vNormal);
                }
                // Lot 2 : lignes OVERLAY (depth-test OFF) -> toujours au-dessus.
                if (vOverlay > 0 && mLinePipelineNoDepth.IsValid()) {
                    cmd->BindGraphicsPipeline(mLinePipelineNoDepth);
                    if (gs.IsValid()) cmd->BindDescriptorSet(gs, 0);
                    cmd->BindVertexBuffer(0, mLineVBO, (uint64)vNormal * sizeof(LV));
                    cmd->Draw(vOverlay);
                }
            }

            // ── 2. PURGE (après rendu) : one-frame (life<=0) retirées ; persistantes
            //        (life>0) décrémentées et retirées si expirées. Évite l'accumulation.
            // COMPACTION EN PLACE O(n) : indispensable quand beaucoup de lignes
            // one-frame sont émises par frame (ex. cage d'un mesh en Edit Mode :
            // des milliers d'arêtes). Un RemoveAt(i) par ligne serait O(n²) et
            // ferait chuter le framerate (12k arêtes -> ~144M ops/frame).
            uint32 keep = 0;
            for (uint32 i = 0; i < (uint32)mDebugLines.Size(); ++i) {
                if (mDebugLines[i].life <= 0.f) continue;          // one-frame -> drop
                mDebugLines[i].life -= mCtx.deltaTime;
                if (mDebugLines[i].life <= 0.f) continue;          // persistante expirée -> drop
                if (keep != i) mDebugLines[keep] = mDebugLines[i]; // survit -> compacte
                ++keep;
            }
            mDebugLines.Resize(keep);
        }

        // ── Phase E.6 : Light cookies 3D ─────────────────────────────────────────
        void NkRender3D::SetLightCookie3D(uint32 slot, NkTextureHandle tex) {
            if (slot >= kMaxCookies3D || !mResources) return;
            NkTextureHandle bind = tex.IsValid() ? tex : mResources->GetWhiteTex();
            NkSamplerHandle samp = mResources->GetSamplerLinearRepeat();
            // Bind sur tous les slots du ring (chaque slot a son descriptor set).
            // Phase Planar Reflection fix 2026-05-24 : aussi sur le ring mirror.
            for (uint32 i = 0; i < mFramesInFlight; i++) {
                if (mGlobalSetRing[i].IsValid()) {
                    mDevice->BindTextureSampler(mGlobalSetRing[i], 13 + slot, bind, samp);
                }
                if (i < mGlobalSetMirrorRing.Size() && mGlobalSetMirrorRing[i].IsValid()) {
                    mDevice->BindTextureSampler(mGlobalSetMirrorRing[i], 13 + slot, bind, samp);
                }
            }
        }

        // ── Phase E.6b : Light cube cookies ──────────────────────────────────────
        void NkRender3D::SetLightCookieCube3D(uint32 slot, NkTextureHandle cubeTex) {
            if (slot >= kMaxCookiesCube3D || !mResources) return;
            NkTextureHandle bind = cubeTex.IsValid() ? cubeTex : mDefaultCubeWhite;
            NkSamplerHandle samp = mResources->GetSamplerLinearRepeat();
            // Phase Planar Reflection fix 2026-05-24 : aussi sur le ring mirror.
            for (uint32 i = 0; i < mFramesInFlight; i++) {
                if (mGlobalSetRing[i].IsValid()) {
                    mDevice->BindTextureSampler(mGlobalSetRing[i], 21 + slot, bind, samp);
                }
                if (i < mGlobalSetMirrorRing.Size() && mGlobalSetMirrorRing[i].IsValid()) {
                    mDevice->BindTextureSampler(mGlobalSetMirrorRing[i], 21 + slot, bind, samp);
                }
            }
        }

        // ── Debug gizmos ─────────────────────────────────────────────────────────
        void NkRender3D::DrawDebugLine(NkVec3f a, NkVec3f b, NkVec4f color, float32 life, bool overlay) {
            // life<=0 => ligne "une frame" (rendue puis purgée par FlushDebug, évite
            // l'accumulation à haut FPS). life>0 => persiste cette durée (secondes).
            // overlay=true => rendue sans depth-test (toujours au-dessus, gizmo éditeur).
            mDebugLines.PushBack({a,b,color,life,overlay});
        }
        void NkRender3D::DrawDebugTriangle(NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f color, float32 life, bool overlay) {
            mDebugTris.PushBack({a,b,c,color,life,overlay});
        }
        // ── Edit overlay persistant ────────────────────────────────────────────────
        void NkRender3D::UploadEditBuf(NkBufferHandle& buf, uint32& cap, const float* v, uint32 vcount) {
            if (vcount == 0) return;
            const uint64 stride = 7 * sizeof(float);   // pos3 + rgba4
            if (!buf.IsValid() || cap < vcount) {
                if (buf.IsValid()) mDevice->DestroyBuffer(buf);
                cap = vcount + 256;
                buf = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)cap * stride));
            }
            mDevice->WriteBuffer(buf, v, (uint64)vcount * stride, 0);
        }
        void NkRender3D::SetEditOverlayLines (const float* v, uint32 n){ UploadEditBuf(mEditLineBuf, mEditLineCap, v, n); mEditLineN=n; }
        void NkRender3D::SetEditOverlayTris  (const float* v, uint32 n){ UploadEditBuf(mEditTriBuf,  mEditTriCap,  v, n); mEditTriN=n; }
        void NkRender3D::SetEditOverlayPoints(const float* v, uint32 n){ UploadEditBuf(mEditPointBuf,mEditPointCap,v, n); mEditPointN=n; }
        void NkRender3D::SetEditOverlayXray  (bool xray){ mEditOverlayNoDepth = xray; }
        void NkRender3D::ClearEditOverlay(){ mEditLineN=mEditTriN=mEditPointN=0; }
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
