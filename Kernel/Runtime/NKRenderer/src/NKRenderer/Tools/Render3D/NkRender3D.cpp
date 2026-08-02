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
#include "NKRenderer/Core/NkRendererConfig.h" // NkUnits() pour triplanar
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKRenderer/Materials/NkMatcapLibrary.h"
#include "NkRender3D_PBRShaders.inl"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <algorithm>

namespace nkentseu {
	namespace renderer {

		NkRender3D::~NkRender3D() {
			Shutdown();
		}

		bool NkRender3D::Init(NkIDevice *device, NkMeshSystem *mesh, NkMaterialSystem *mat, NkRenderGraph *graph,
							  NkVirtualShadowMaps *shadow, NkEnvironmentSystem *env, NkShaderLibrary *shaderLib,
							  NkResources *resources, uint32 framesInFlight) {
			mDevice = device;
			mMesh = mesh;
			mMat = mat;
			mGraph = graph;
			mShadow = shadow;
			mEnv = env;
			mShaderLib = shaderLib;
			mResources = resources;

			// PROFONDEUR DES RINGS = celle du DEVICE, imperativement.
			// mFrameSlot est calcule par `mDevice->GetFrameIndex() % mFramesInFlight`
			// et GetFrameIndex() cycle modulo la profondeur du device (MAX_FRAMES = 3
			// sur GL/VK/DX11/DX12). Si mFramesInFlight est PLUS PETIT, le modulo n'est
			// plus bijectif : la suite des slots devient 0,1,0 | 0,1,0 | ... et le slot 0
			// revient sur DEUX FRAMES CONSECUTIVES -> le CPU reecrit un buffer que le GPU
			// lit encore pour la frame precedente (la fence de BeginFrame ne couvre que
			// N-devFIF) -> donnees dechirees -> clignotement. Voir NkRendererImpl.
			{
				const uint32 devFIF = mDevice ? mDevice->GetMaxFramesInFlight() : 0u;
				uint32 want = framesInFlight < 1u ? 1u : framesInFlight;
				if (devFIF > want)
					want = devFIF;
				mFramesInFlight = want;
			}
			mFrameSlot = 0;

			// ── UBOs (matchent le shader pbr.vert/frag.gl.glsl) ──────────────────
			// Une copie par slot du ring : evite que le CPU stalle quand il
			// ecrit un buffer encore lu par le GPU. Avec mFramesInFlight=1 on
			// retombe sur le comportement legacy (1 buffer partage).
			mUBOCameraRing.Resize(mFramesInFlight);
			mUBOCameraMirrorRing.Resize(mFramesInFlight); // Phase Planar Reflection fix
			mUBOLightsRing.Resize(mFramesInFlight);
			mUBOObjectPool.Resize(mFramesInFlight);

			// Camera UBO — binding 0 (main + mirror dedicated)
			for (uint32 i = 0; i < mFramesInFlight; i++) {
				mUBOCameraRing[i] = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkCameraUBO)));
				mUBOCameraMirrorRing[i] = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkCameraUBO)));
			}

			// Lights UBO — binding 2 (32 lights max)
			struct LightsUBO {
					NkVec4f pos[32], color[32], dir[32], angles[32];
					int32 count, _p[3];
			};

			for (uint32 i = 0; i < mFramesInFlight; i++) {
				mUBOLightsRing[i] = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(LightsUBO)));
			}

			// Object UBO — binding 1. Layout std140 doit matcher EXACTEMENT le shader
			// pbr.vert/frag.gl.glsl (sinon le linker GL refuse, ou les reads out-of-buffer
			// donnent un undefined behavior selon le driver).
			struct ObjectUBO {
					NkMat4f model;			  // 0
					NkMat4f normalMatrix;	  // 64
					NkVec4f tint;			  // 128
					float32 metallic;		  // 144
					float32 roughness;		  // 148
					float32 aoStrength;		  // 152
					float32 emissiveStrength; // 156
					float32 normalStrength;	  // 160
					float32 clearcoat;		  // 164
					float32 clearcoatRough;	  // 168
					float32 subsurface;		  // 172
					NkVec4f subsurfaceColor;  // 176 (aligned to 16)
					// NkVSM v1 : shadowOverrides (.x receiveShadow, .z shadowBiasMul)
					// Doit matcher la struct ObjBlock locale dans FlushOpaque +
					// RenderShadowPass (sinon WriteBuffer overflow le buffer pool
					// -> GL_INVALID_VALUE silent + rien ne s'affiche).
					NkVec4f shadowOverrides; // 192
					// 2026-05-24 Triplanar : .x = tileSize en metres (0=disabled),
					// .y = metersPerUnit (echelle globale, copie de NkUnits()),
					// .z = enable flag (0/1 redondant avec .x>0 mais explicite cote
					// shader), .w = reserve. Doit matcher ObjBlock 3 endroits +
					// ObjectUBO dans VS+FS (regle GLSL shared block).
					NkVec4f triplanarParams; // 208
			}; // total 224

			static_assert(sizeof(ObjectUBO) == 224, "ObjectUBO std140 layout");
			// Phase F.B.1 : pool d'ObjectUBO (frame x drawIdx). Pre-alloue
			// mFramesInFlight * mObjectPoolCap buffers a Init pour eviter
			// toute allocation dans le hot path et tout vkCmdUpdateBuffer dans
			// un renderPass actif (interdit par Vulkan).
			for (uint32 i = 0; i < mFramesInFlight; i++) {
				mUBOObjectPool[i].Resize(mObjectPoolCap);
				for (uint32 d = 0; d < mObjectPoolCap; d++) {
					mUBOObjectPool[i][d] = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(ObjectUBO)));
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
			for (uint32 i = 0; i < mFramesInFlight; i++) {
				mUBOBonesRing[i] = mDevice->CreateBuffer(NkBufferDesc::Uniform(kMaxBonesUBO * sizeof(NkMat4f)));
			}

			// Buffer d'instances (GPU instancing 1-draw) : models[128]+tints[128]
			// (std140, 10240 octets). Même stratégie ring que les bones.
			mUBOInstanceRing.Resize(mFramesInFlight);
			for (uint32 i = 0; i < mFramesInFlight; i++) {
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
			for (uint32 i = 0; i < mFramesInFlight; i++) {
				mUBOShadowInstPool[i].Resize(kShadowInstPoolCap);
				for (uint32 d = 0; d < kShadowInstPoolCap; d++) {
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
						mDevice->WriteTextureRegion(mDefaultCubeWhite, white, 0, 0, 0, 1, 1, 1, 0, face);
					}
				}
			}

			// ── ATLAS MATCAP (30 boules, façon Blender) ──────────────────────────
			// Une matcap est une image de BOULE éclairée, échantillonnée par la normale
			// en ESPACE VUE : uv = normaleVue.xy * 0.5 + 0.5. L'éclairage est peint dans
			// la texture — aucune lumière de scène, aucune ombre, aucun IBL.
			//
			// Les 30 boules tiennent dans UN atlas 6x5 (cf. NkMatcapLibrary) plutôt que
			// dans 30 textures ou un tableau de textures : un seul binding, un seul
			// sampler, aucun changement de descripteur quand l'utilisateur change de
			// matcap. Le shader calcule lui-même l'offset de tuile depuis matcapId, donc
			// AUCUN uniforme supplémentaire n'est nécessaire.
			//
			// UNE SEULE MIP : une matcap est déjà lisse à l'écran, et des mips
			// mélangeraient les tuiles voisines entre elles.
			{
				const uint32 W = NkMatcapLibrary::kAtlasW, H = NkMatcapLibrary::kAtlasH;
				auto td = NkTextureDesc::Tex2D(W, H, NkGPUFormat::NK_RGBA8_UNORM, 1);
				td.debugName = "MatcapAtlas30";
				mMatcapTex = mDevice->CreateTexture(td);
				if (mMatcapTex.IsValid()) {
					NkVector<uint8> px;
					px.Resize(W * H * 4);
					NkMatcapLibrary::GenerateAtlas(px.Data());
					mDevice->WriteTextureRegion(mMatcapTex, px.Data(), 0, 0, 0, W, H, 1, 0, 0);
				}
			}

			// ── Descriptor set layouts ────────────────────────────────────────────
			// Frame set (set 0) : Camera(0) + Lights(2) + Shadow(3) + 4 textures
			// materiel par defaut(4-7) + Env irradiance/prefilter/BRDFLUT(8/9/10)
			// + Shadow map(11). Bindings matchent le shader PBR.
			NkDescriptorSetLayoutDesc frameLayout;
			frameLayout.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
				.Add(2, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
				.Add(3, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
				.Add(4, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
				.Add(5, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
				.Add(6, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
				.Add(7, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
				.Add(8, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
				.Add(9, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
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
				.Add(25, NkDescriptorType::NK_UNIFORM_BUFFER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
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
			mGlobalSetMirrorRing.Resize(mFramesInFlight); // Phase Planar Reflection fix
			mObjectSetPool.Resize(mFramesInFlight);
			for (uint32 i = 0; i < mFramesInFlight; i++) {
				mGlobalSetRing[i] = mDevice->AllocateDescriptorSet(mGlobalLayout);
				mGlobalSetMirrorRing[i] = mDevice->AllocateDescriptorSet(mGlobalLayout);
				mObjectSetPool[i].Resize(mObjectPoolCap);
				for (uint32 d = 0; d < mObjectPoolCap; d++) {
					mObjectSetPool[i][d] = mDevice->AllocateDescriptorSet(mObjectLayout);
				}
			}

			// ── Pre-bind static buffers + textures into chaque slot ──────────────
			// Resources statiques (textures defauts, env maps, shadow atlas) : memes
			// valeurs pour tous les slots. UBOs ring : binding sur le buffer du slot.
			NkTextureHandle defAlbedo = mResources ? mResources->GetWhiteTex() : NkTextureHandle{};
			NkTextureHandle defNormal = mResources ? mResources->GetNormalTex() : NkTextureHandle{};
			NkTextureHandle defORM = mResources ? mResources->GetWhiteTex() : NkTextureHandle{};
			NkTextureHandle defEmissive = mResources ? mResources->GetBlackTex() : NkTextureHandle{};
			NkSamplerHandle defSampler = mResources ? mResources->GetSamplerLinearRepeat() : NkSamplerHandle{};

			// Phase Planar Reflection fix : lambda partagee qui pre-bind un
			// global set (main ou mirror) avec un UBO Camera donne. Toutes les
			// autres ressources (lights, shadow, env, voxel, cookies) sont
			// identiques entre les rings main et mirror.
			auto preBindGlobalSet = [&](NkDescSetHandle gs, NkBufferHandle cameraUBO, uint32 i) {
				if (!gs.IsValid())
					return;
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

				if (defAlbedo.IsValid())
					mDevice->BindTextureSampler(gs, 4, defAlbedo, defSampler);
				if (defNormal.IsValid())
					mDevice->BindTextureSampler(gs, 5, defNormal, defSampler);
				if (defORM.IsValid())
					mDevice->BindTextureSampler(gs, 6, defORM, defSampler);
				if (defEmissive.IsValid())
					mDevice->BindTextureSampler(gs, 7, defEmissive, defSampler);

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
					mDevice->BindTextureSampler(gs, 27, mVoxelAO->GetVoxelTexture(), mVoxelAO->GetVoxelSampler());
				}

				// Binding 28 : boule matcap (mode SOLID/WIREFRAME, matcap texture).
				// Sampler CLAMP (pas repeat !) : au bord des sphères l'UV frôle 0/1 et le
				// repeat rebouclerait sur le bord opposé -> stries sombres. Clamp = propre.
				NkSamplerHandle mcSamp = mResources ? mResources->GetSamplerLinearClamp() : defSampler;
				if (mMatcapTex.IsValid() && mcSamp.IsValid())
					mDevice->BindTextureSampler(gs, 28, mMatcapTex, mcSamp);

				if (mShadow && mShadow->GetAtlasTexture().IsValid()) {
					mDevice->BindTextureSampler(gs, 11, mShadow->GetAtlasTexture(), mShadow->GetAtlasSampler());
					mDevice->BindTextureSampler(gs, 12, mShadow->GetAtlasTexture(), mShadow->GetAtlasRawSampler());
				}

				if (mResources) {
					NkTextureHandle whiteTex = mResources->GetWhiteTex();
					NkSamplerHandle whiteSamp = mResources->GetSamplerLinearRepeat();
					for (uint32 ci = 0; ci < kMaxCookies3D; ci++) {
						mDevice->BindTextureSampler(gs, 13 + ci, whiteTex, whiteSamp);
					}
					if (mDefaultCubeWhite.IsValid()) {
						for (uint32 ci = 0; ci < kMaxCookiesCube3D; ci++) {
							mDevice->BindTextureSampler(gs, 21 + ci, mDefaultCubeWhite, whiteSamp);
						}
					}
				}
			};

			for (uint32 i = 0; i < mFramesInFlight; i++) {
				// Bind main + mirror sets avec leur UBO Camera respectif.
				preBindGlobalSet(mGlobalSetRing[i], mUBOCameraRing[i], i);
				preBindGlobalSet(mGlobalSetMirrorRing[i], mUBOCameraMirrorRing[i], i);

				// Phase F.B.1 : bind chaque set du pool a son UBO du pool (1:1).
				// + Skinning : bind l'UBO de bones de la frame au binding=2 de
				//   chaque set objet (le contenu est reecrit par draw skinne dans
				//   FlushSkinned ; les draws non-skinnes ne lisent jamais ce slot
				//   mais le binding doit etre valide pour le layout VK/DX).
				for (uint32 d = 0; d < mObjectPoolCap; d++) {
					NkDescSetHandle os = mObjectSetPool[i][d];
					if (os.IsValid()) {
						mDevice->BindUniformBuffer(os, 1, mUBOObjectPool[i][d]);
						// Ring : chaque frame i bind SON UBO de bones (pas un
						// buffer partage) -> la frame N+1 ecrit mUBOBonesRing[N+1]
						// pendant que le GPU lit encore mUBOBonesRing[N].
						if (i < mUBOBonesRing.Size() && mUBOBonesRing[i].IsValid()) {
							mDevice->BindUniformBuffer(os, 4, mUBOBonesRing[i]); // binding 4 (anti-collision GL/DX)
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
				pd.shader = mShadowShader;
				pd.depthStencil = NkDepthStencilDesc::Default(); // depth write enabled
				// Pipeline Shadow rend dans le shadow atlas (depth-only). En VK
				// le pipeline doit etre cree avec un RP compatible (sinon le
				// fallback swapchain RP color+depth donne un draw incompatible).
				if (mShadow)
					pd.renderPass = mShadow->GetShadowRenderPass();
				// Shadow casters typiquement render avec front-face culling pour
				// reduire le shadow acne (peter-panning). Mais sans winding fiable
				// sur les meshes primitifs, on garde NoCull.
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "Shadow_DepthOnly";
				// Range push_constant ALL_GRAPHICS : permet aux appelants qui
				// pushent avec stage=ALL_GRAPHICS (convention NkShadowSystem /
				// NkRender3D) de respecter le range declare. Le shader Shadow VS
				// est le seul a lire le push_constant en pratique.
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f));
				// Layout : [global, object] meme si Shadow VS n'utilise que object.
				// Necessaire pour que le bind a set=1 (convention) reste valide en VK.
				pd.descriptorSetLayouts.PushBack(mGlobalLayout);
				pd.descriptorSetLayouts.PushBack(mObjectLayout); // reutilise ObjectUBO
				pd.vertexLayout.AddBinding(0, sizeof(NkVertex3D), false)
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
				pd.shader = mShadowInstanceShader;
				pd.depthStencil = NkDepthStencilDesc::Default(); // depth write
				if (mShadow)
					pd.renderPass = mShadow->GetShadowRenderPass();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "ShadowInstanced_DepthOnly";
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f));
				pd.descriptorSetLayouts.PushBack(mGlobalLayout);
				pd.descriptorSetLayouts.PushBack(mObjectLayout); // binding1 + binding4
				pd.vertexLayout.AddBinding(0, sizeof(NkVertex3D), false)
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
				logger.Info("[NkRender3D] Skin shader compile: valid={0}\n", mSkinShader.IsValid() ? 1 : 0);
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
				logger.Info("[NkRender3D] Instanced shader compile: valid={0}\n", mInstanceShader.IsValid() ? 1 : 0);
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
				logger.Info("[NkRender3D] InfiniteGrid shader compile: valid={0}\n", mGridShader.IsValid() ? 1 : 0);
			}

			// Fournit les layouts partagés au material system afin que ses pipelines
			// soient RP-compatibles et aient la même layout set 0/1 que le PBR pipeline.
			// Le renderPass est inconnu ici (lazy), mis à jour dans Flush() via UpdateRenderPass().
			if (mMat) {
				// NkVertexLayout ici = type RHI (nkentseu::NkVertexLayout),
				// distinct de nkentseu::renderer::NkVertexLayout (NkMeshSystem.h).
				::nkentseu::NkVertexLayout sharedVL;
				sharedVL.AddBinding(0, sizeof(NkVertex3D), false)
					.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
					.AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL", 0)
					.AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT", 0)
					.AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT, 36, "TEXCOORD", 0)
					.AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT, 44, "TEXCOORD", 1)
					.AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR", 0);
				mMat->SetSharedContext(mGlobalLayout, mObjectLayout, sharedVL);
			}

			// ── Shadow ALPHA-TESTED (NkVSM v2) : casters feuillage/masked ─────────
			// Variante du pipeline Shadow qui passe l'UV et sample l'albedo du
			// material (discard < 0.5). set=2 = layout UNIVERSEL des instances
			// (GetInstanceLayout) → on binde matInst->GetDescSet() tel quel dans
			// RenderShadowPass. Cree ici car depend de mMat (layout) + mShadow (RP).
			if (mShaderLib && mMat) {
				auto progHandle = mShaderLib->LoadOrCompileVF("ShadowAlpha", "", "");
				if (progHandle.IsValid())
					mShadowAlphaShader = mShaderLib->GetRHIHandle(progHandle);
			}
			if (mShadowAlphaShader.IsValid() && mMat) {
				NkGraphicsPipelineDesc pd;
				pd.shader = mShadowAlphaShader;
				pd.depthStencil = NkDepthStencilDesc::Default();
				if (mShadow)
					pd.renderPass = mShadow->GetShadowRenderPass();
				pd.rasterizer = NkRasterizerDesc::NoCull();
				pd.blend = NkBlendDesc::Opaque();
				pd.debugName = "Shadow_AlphaTest";
				pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f));
				pd.descriptorSetLayouts.PushBack(mGlobalLayout);
				pd.descriptorSetLayouts.PushBack(mObjectLayout);
				pd.descriptorSetLayouts.PushBack(mMat->GetInstanceLayout());
				pd.vertexLayout.AddBinding(0, sizeof(NkVertex3D), false)
					.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
					.AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT, 36, "TEXCOORD", 0);
				mShadowAlphaPipeline = mDevice->CreateGraphicsPipeline(pd);
				logger.Info("[NkRender3D] ShadowAlpha pipeline create: shader_valid={0} pipeline_valid={1}\n",
							mShadowAlphaShader.IsValid() ? 1 : 0, mShadowAlphaPipeline.IsValid() ? 1 : 0);
			}

			bool ringValid = !mUBOCameraRing.Empty() && mUBOCameraRing[0].IsValid();
			logger.Info(
				"[NkRender3D] Init final: ringValid={0} pbrShader.valid={1} (PBR pipeline: lazy create at 1st flush)\n",
				ringValid ? 1 : 0, mPBRShader.IsValid() ? 1 : 0);
			return ringValid && mPBRShader.IsValid();
		}

		// ── Lazy create du pipeline PBR (Bug fix Vulkan : RP compat) ────────────
		bool NkRender3D::EnsurePBRPipeline(NkRenderPassHandle currentRP) {
			if (!mPBRShader.IsValid())
				return false;

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
			if (mPBRPipeline.IsValid())
				return true;

			NkGraphicsPipelineDesc pd;
			pd.shader = mPBRShader;
			pd.depthStencil = NkDepthStencilDesc::Default(); // depth test enabled
			// D.1 : NoCull tant que les meshes primitifs n'ont pas de winding
			// CCW garanti (le plane par exemple a un winding inverse).
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Opaque();
			pd.debugName = "PBR_Opaque";
			pd.renderPass = currentRP;
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
			pd.vertexLayout.AddBinding(0, sizeof(NkVertex3D), false)
				.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
				.AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL", 0)
				.AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT", 0)
				.AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT, 36, "TEXCOORD", 0)
				.AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT, 44, "TEXCOORD", 1)
				.AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR", 0);

			mPBRPipeline = mDevice->CreateGraphicsPipeline(pd);
			mPBRPipelineRP = currentRP;
			logger.Info("[NkRender3D] PBR pipeline (lazy) create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
						mPBRShader.IsValid() ? 1 : 0, mPBRPipeline.IsValid() ? 1 : 0, currentRP.id);
			return mPBRPipeline.IsValid();
		}

		// ── DEFERRED v1 : pipeline G-buffer fill (lazy, RP-compatible) ────────
		// Calque d'EnsurePBRPipeline avec le shader DeferredGeom (MRT 3 cibles).
		// Le blend desc unique s'applique a toutes les cibles (opaque partout).
		bool NkRender3D::EnsureDeferredGeomPipeline(NkRenderPassHandle currentRP) {
			if (mDeferredGeomPipeline.IsValid())
				return true;
			if (!mDeferredGeomShader.IsValid() && mShaderLib) {
				auto prog = mShaderLib->LoadOrCompileVF("DeferredGeom", "", "");
				if (prog.IsValid())
					mDeferredGeomShader = mShaderLib->GetRHIHandle(prog);
			}
			if (!mDeferredGeomShader.IsValid())
				return false;

			NkGraphicsPipelineDesc pd;
			pd.shader = mDeferredGeomShader;
			pd.depthStencil = NkDepthStencilDesc::Default();
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Opaque();
			// MRT : Vulkan (et DX12) exigent UN blend state PAR attachement
			// couleur — avec 1 seul, les cibles 1..2 sont indefinies (VK sombre,
			// DX12 blanc). 3 cibles => 3 blends opaques.
			while (pd.blend.attachments.Size() < 3)
				pd.blend.attachments.PushBack(NkBlendAttachment::Opaque());
			pd.debugName = "Deferred_GBuffer";
			pd.renderPass = currentRP;
			pd.descriptorSetLayouts.PushBack(mGlobalLayout);
			pd.descriptorSetLayouts.PushBack(mObjectLayout);
			if (mMat && mMat->GetInstanceLayout().IsValid())
				pd.descriptorSetLayouts.PushBack(mMat->GetInstanceLayout());
			pd.vertexLayout.AddBinding(0, sizeof(NkVertex3D), false)
				.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
				.AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL", 0)
				.AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT", 0)
				.AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT, 36, "TEXCOORD", 0)
				.AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT, 44, "TEXCOORD", 1)
				.AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR", 0);
			mDeferredGeomPipeline = mDevice->CreateGraphicsPipeline(pd);
			logger.Info("[NkRender3D] DeferredGeom pipeline: shader={0} pipeline={1}\n",
						mDeferredGeomShader.IsValid() ? 1 : 0, mDeferredGeomPipeline.IsValid() ? 1 : 0);
			return mDeferredGeomPipeline.IsValid();
		}

		// ── DEFERRED v1 : pipeline lighting fullscreen (lazy) ─────────────────
		// Layouts [global, gbuf]. Le set gbuf (4 samplers) est alloue ICI
		// (lazy) et re-ecrit chaque frame dans RenderDeferredLighting.
		bool NkRender3D::EnsureDeferredLightPipeline(NkRenderPassHandle currentRP) {
			if (mDeferredLightPipeline.IsValid())
				return true;
			if (!mDeferredLightShader.IsValid() && mShaderLib) {
				auto prog = mShaderLib->LoadOrCompileVF("DeferredLight", "", "");
				if (prog.IsValid())
					mDeferredLightShader = mShaderLib->GetRHIHandle(prog);
			}
			if (!mDeferredLightShader.IsValid())
				return false;

			if (!mGBufLayout.IsValid()) {
				using RHIStage = ::nkentseu::NkShaderStage;
				NkDescriptorSetLayoutDesc gl;
				gl.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, RHIStage::NK_ALL_GRAPHICS)
					.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, RHIStage::NK_ALL_GRAPHICS)
					.Add(2, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, RHIStage::NK_ALL_GRAPHICS)
					.Add(3, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, RHIStage::NK_ALL_GRAPHICS);
				mGBufLayout = mDevice->CreateDescriptorSetLayout(gl);
				mGBufSet = mDevice->AllocateDescriptorSet(mGBufLayout);
			}

			NkGraphicsPipelineDesc pd;
			pd.shader = mDeferredLightShader;
			pd.depthStencil = NkDepthStencilDesc::NoDepth();
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Opaque();
			pd.debugName = "Deferred_Lighting";
			pd.renderPass = currentRP;
			pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, 16);
			pd.descriptorSetLayouts.PushBack(mGlobalLayout);
			pd.descriptorSetLayouts.PushBack(mGBufLayout);
			// Fullscreen triangle via gl_VertexID : aucun vertex input.
			mDeferredLightPipeline = mDevice->CreateGraphicsPipeline(pd);
			logger.Info("[NkRender3D] DeferredLight pipeline: shader={0} pipeline={1}\n",
						mDeferredLightShader.IsValid() ? 1 : 0, mDeferredLightPipeline.IsValid() ? 1 : 0);
			return mDeferredLightPipeline.IsValid();
		}


		// ── Lazy create du pipeline de skinning GPU ──────────────────────────
		// Calque sur EnsurePBRPipeline mais avec le vertex layout NkVertexSkinned
		// (ajout de aBoneIdx/aBoneWeight) et le shader "Skin". L'UBO de bones
		// est deja lie au set objet (set=1, binding=2) a Init. Memes set layouts
		// que le PBR (global/object/material) -> compatible avec les binds du
		// FlushSkinned (set global + set objet + set materiau fallback).
		bool NkRender3D::EnsureSkinPipeline(NkRenderPassHandle currentRP) {
			if (!mSkinShader.IsValid())
				return false;
			if (mSkinPipeline.IsValid())
				return true;

			NkGraphicsPipelineDesc pd;
			pd.shader = mSkinShader;
			pd.depthStencil = NkDepthStencilDesc::Default();
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Opaque();
			pd.debugName = "Skin_Opaque";
			pd.renderPass = currentRP;
			pd.descriptorSetLayouts.PushBack(mGlobalLayout);
			pd.descriptorSetLayouts.PushBack(mObjectLayout);
			if (mMat && mMat->GetInstanceLayout().IsValid())
				pd.descriptorSetLayouts.PushBack(mMat->GetInstanceLayout());

			// Vertex layout NkVertexSkinned : NkVertex3D (56o) + boneIdx(vec4,16o)
			//   + boneWeight(vec4,16o). Stride = 88. Les indices sont en float
			//   (RGBA32_FLOAT) pour rester portables cross-backend (cf. struct).
			pd.vertexLayout.AddBinding(0, sizeof(NkVertexSkinned), false)
				.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
				.AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL", 0)
				.AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT", 0)
				.AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT, 36, "TEXCOORD", 0)
				.AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT, 44, "TEXCOORD", 1)
				.AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR", 0)
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

			mSkinPipeline = mDevice->CreateGraphicsPipeline(pd);
			mSkinPipelineRP = currentRP;
			logger.Info("[NkRender3D] Skin pipeline (lazy) create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
						mSkinShader.IsValid() ? 1 : 0, mSkinPipeline.IsValid() ? 1 : 0, currentRP.id);
			return mSkinPipeline.IsValid();
		}

		// ── Lazy create du pipeline d'instancing GPU ─────────────────────────
		// Calque sur EnsureSkinPipeline mais avec le vertex layout STANDARD
		// (NkVertex3D, sans bones) et le shader "Instanced". Le buffer d'instances
		// est lié au set objet (binding 2). Mêmes set layouts que le PBR/skin.
		bool NkRender3D::EnsureInstancePipeline(NkRenderPassHandle currentRP) {
			if (!mInstanceShader.IsValid())
				return false;
			if (mInstancePipeline.IsValid())
				return true;

			NkGraphicsPipelineDesc pd;
			pd.shader = mInstanceShader;
			pd.depthStencil = NkDepthStencilDesc::Default();
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Opaque();
			pd.debugName = "Instanced_Opaque";
			pd.renderPass = currentRP;
			pd.descriptorSetLayouts.PushBack(mGlobalLayout);
			pd.descriptorSetLayouts.PushBack(mObjectLayout);
			if (mMat && mMat->GetInstanceLayout().IsValid())
				pd.descriptorSetLayouts.PushBack(mMat->GetInstanceLayout());

			// Vertex layout STANDARD NkVertex3D (56 octets) — identique au PBR.
			pd.vertexLayout.AddBinding(0, sizeof(NkVertex3D), false)
				.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
				.AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL", 0)
				.AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT", 0)
				.AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT, 36, "TEXCOORD", 0)
				.AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT, 44, "TEXCOORD", 1)
				.AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR", 0);

			mInstancePipeline = mDevice->CreateGraphicsPipeline(pd);
			mInstancePipelineRP = currentRP;
			logger.Info(
				"[NkRender3D] Instanced pipeline (lazy) create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
				mInstanceShader.IsValid() ? 1 : 0, mInstancePipeline.IsValid() ? 1 : 0, currentRP.id);
			return mInstancePipeline.IsValid();
		}

		// ── Phase N v0.5 : EnsureSkyboxPipeline (lazy, RP-compatible) ───────
		// Pipeline minimal : pas de VBO (gl_VertexIndex pour 3 verts fullscreen),
		// depth test LEQUAL + depthWrite=false (les objets dessines apres
		// peuvent occlure la skybox sans qu'elle leur barre la route).
		bool NkRender3D::EnsureSkyboxPipeline(NkRenderPassHandle currentRP) {
			if (!mSkyboxShader.IsValid())
				return false;
			if (mSkyboxPipeline.IsValid())
				return true;

			NkGraphicsPipelineDesc pd;
			pd.shader = mSkyboxShader;
			// Depth : test LEQUAL pour passer le clear=1.0 du depth buffer,
			// mais pas d'ecriture - les objets opaques garderont leur depth.
			{
				NkDepthStencilDesc ds;
				ds.depthTestEnable = true;
				ds.depthWriteEnable = false;
				ds.depthCompareOp = ::nkentseu::NkCompareOp::NK_LESS_EQUAL;
				pd.depthStencil = ds;
			}
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Opaque();
			pd.debugName = "Skybox";
			pd.renderPass = currentRP;
			// Set 0 = global set (CameraUBO @0 + tEnvPrefilter @9 reuse-direct).
			pd.descriptorSetLayouts.PushBack(mGlobalLayout);
			// Pas de vertex layout (gl_VertexIndex only dans le vert shader).

			mSkyboxPipeline = mDevice->CreateGraphicsPipeline(pd);
			mSkyboxPipelineRP = currentRP;
			logger.Info("[NkRender3D] Skybox pipeline create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
						mSkyboxShader.IsValid() ? 1 : 0, mSkyboxPipeline.IsValid() ? 1 : 0, currentRP.id);
			return mSkyboxPipeline.IsValid();
		}

		// Draw 1 triangle fullscreen avec le pipeline Skybox. Doit etre appele
		// dans la passe Geometry, AVANT les drawcalls opaques (pour que le
		// depth=1.0 ecrit par la skybox ne masque pas les objets — bien que
		// depthWrite=false ait neutralise ce risque, garder l'ordre logique).
		void NkRender3D::DrawSkybox(NkICommandBuffer *cmd) {
			if (!mDrawSkybox || !cmd)
				return;
			if (!mSkyboxPipeline.IsValid())
				return;

			// Bind pipeline + global set (CameraUBO + prefilter cubemap).
			// Phase Planar Reflection fix : bind le ring mirror si en mirror pass.
			cmd->BindGraphicsPipeline(mSkyboxPipeline);
			NkDescSetHandle gs;
			if (mPendingMirrorActive) {
				gs = (mFrameSlot < mGlobalSetMirrorRing.Size()) ? mGlobalSetMirrorRing[mFrameSlot] : NkDescSetHandle{};
			} else {
				gs = (mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
			}
			if (gs.IsValid())
				cmd->BindDescriptorSet(gs, 0);

			// 3 vertices, 1 instance (fullscreen triangle sans VBO)
			cmd->Draw(3, 1, 0, 0);
		}

		// ── Grille infinie style Blender (plan y=0) ─────────────────────────────
		bool NkRender3D::EnsureGridPipeline(NkRenderPassHandle currentRP) {
			if (!mGridShader.IsValid())
				return false;
			if (mGridPipeline.IsValid() && mGridPipelineRP == currentRP)
				return true;
			if (mGridPipeline.IsValid()) {
				mDevice->DestroyPipeline(mGridPipeline);
				mGridPipeline = {};
			}

			NkGraphicsPipelineDesc pd;
			pd.shader = mGridShader;
			// Depth : test LEQUAL (la grille au sol s'affiche sur/à hauteur du sol),
			// pas d'écriture (overlay qui respecte la profondeur des objets opaques).
			{
				NkDepthStencilDesc ds;
				ds.depthTestEnable = true;
				ds.depthWriteEnable = false;
				ds.depthCompareOp = ::nkentseu::NkCompareOp::NK_LESS_EQUAL;
				pd.depthStencil = ds;
			}
			pd.rasterizer = NkRasterizerDesc::NoCull(); // triangle plein-écran, pas de cull
			pd.blend = NkBlendDesc::Alpha();			// fondu de l'intérieur + lignes
			pd.debugName = "InfiniteGrid";
			pd.renderPass = currentRP;
			// Push constant = 6 vec4 (lineColor, cellColor, axisX, axisZ, params, extra).
			pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkVec4f) * 6);
			pd.descriptorSetLayouts.PushBack(mGlobalLayout); // set 0 = CameraUBO
			// Pas de vertex layout (le quad est généré via gl_VertexID).

			mGridPipeline = mDevice->CreateGraphicsPipeline(pd);
			mGridPipelineRP = currentRP;
			logger.Info("[NkRender3D] Grid pipeline create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
						mGridShader.IsValid() ? 1 : 0, mGridPipeline.IsValid() ? 1 : 0, currentRP.id);
			return mGridPipeline.IsValid();
		}

		void NkRender3D::DrawGrid(NkICommandBuffer *cmd) {
			if (!mDrawGrid || !cmd || !mGridPipeline.IsValid())
				return;
			cmd->BindGraphicsPipeline(mGridPipeline);

			// Push constant : doit matcher le bloc PC des shaders infinitegrid.*.nksl.
			struct GridPC {
					NkVec4f lineColor;
					NkVec4f cellColor;
					NkVec4f axisXColor;
					NkVec4f axisZColor;
					NkVec4f params; // .x=cellSize .y=majorEvery .z=extent .w=fadeEnd
					NkVec4f extra;	// .x=planeY ; .yzw réservés
			} pc;

			pc.lineColor = mGridParams.lineColor;
			pc.cellColor = mGridParams.cellColor;
			pc.axisXColor = mGridParams.axisXColor;
			pc.axisZColor = mGridParams.axisZColor;
			pc.params = NkVec4f{mGridParams.cellSize, mGridParams.majorEvery, mGridParams.extent, mGridParams.fadeEnd};
			pc.extra = NkVec4f{mGridParams.planeY, mGridParams.showMinor ? 1.f : 0.f, mGridParams.showMajor ? 1.f : 0.f,
							   mGridParams.showAxes ? 1.f : 0.f};
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(GridPC), &pc);

			NkDescSetHandle gs = (mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
			if (gs.IsValid())
				cmd->BindDescriptorSet(gs, 0);

			cmd->Draw(3, 1, 0, 0); // 1 triangle plein-écran (reconstruction de rayon)
		}

		// ═════════════════════════════════════════════════════════════════════════
		// Sélection « outline silhouette » façon Blender (post-process edge-detect).
		// Passe 1 (RenderSelectionMask) : objets sélectionnés rendus SEULS en blanc
		//   dans une cible offscreen R8 -> silhouette pleine.
		// Passe 2 (CompositeSelectionOutline) : plein écran, dilatation-différence du
		//   masque -> fin liseré orange composité par-dessus l'image finale.
		// ═════════════════════════════════════════════════════════════════════════
		void NkRender3D::SetSelectionOutline(bool enabled, NkVec4f color, float32 thicknessPx) {
			if (enabled != mSelOutline)
				mSelOutlineGraphDirty = true; // (dé)active les passes -> rebuild du graph
			mSelOutline = enabled;
			mSelOutlineColor = color;
			mSelOutlineThickness = (thicknessPx > 0.f) ? thicknessPx : 1.f;
			if (mSelOutlineThickness > 8.f)
				mSelOutlineThickness = 8.f; // borne raisonnable (rayon de recherche en px)
		}

		void NkRender3D::SubmitSelection(const NkDrawCall3D &dc, bool isActive) {
			if (!dc.mesh.IsValid())
				return;
			mSelection.PushBack(dc);
			// Niveau ecrit dans le masque : 1.0 = ACTIF, 0.5 = selectionne. C'est ce
			// niveau qui permet au shader de contour de distinguer les deux, comme
			// Blender le fait. Un masque binaire ne pouvait pas porter l'information.
			mSelectionActive.PushBack(isActive ? (uint8)1 : (uint8)0);
		}

		// Pipeline MASQUE : shader trivial (VS = viewProj*model, FS = blanc), sans
		// depth (silhouette pleine), NoCull (les deux faces -> masque plein). Set 0 =
		// CameraUBO (même UBO que la scène -> alignement exact) ; model en push const.
		bool NkRender3D::EnsureSelMaskPipeline(NkRenderPassHandle currentRP) {
			if (mSelMaskPipeline.IsValid() && mSelMaskPipelineRP == currentRP)
				return true;
			if (!mShaderLib)
				return false;
			if (!mSelMaskShader.IsValid()) {
				auto prog = mShaderLib->LoadOrCompileVF("SelMask", "", "");
				if (!prog.IsValid()) {
					logger.Errorf("[NkR3D::SelMask] shader compile FAIL\n");
					return false;
				}
				mSelMaskShader = mShaderLib->GetRHIHandle(prog);
			}
			if (!mSelMaskShader.IsValid())
				return false;
			if (mSelMaskPipeline.IsValid()) {
				mDevice->DestroyPipeline(mSelMaskPipeline);
				mSelMaskPipeline = {};
			}
			NkGraphicsPipelineDesc pd;
			pd.shader = mSelMaskShader;
			pd.depthStencil = NkDepthStencilDesc::NoDepth();
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Opaque();
			pd.debugName = "SelMask";
			pd.renderPass = currentRP;
			// model (mat4) + level (vec4). La plage DOIT couvrir les deux : declaree a
			// sizeof(NkMat4f) seul, le niveau pousse au-dela de la plage etait rejete et
			// le masque ressortait VIDE — plus aucun lisere, sans le moindre message
			// d'erreur. Toute evolution du push-constant d'un shader doit etre reportee
			// ICI, sous peine d'une disparition silencieuse.
			pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0,
							   (uint32)(sizeof(NkMat4f) + sizeof(NkVec4f)));
			pd.descriptorSetLayouts.PushBack(mGlobalLayout);								   // set 0 = CameraUBO
			pd.vertexLayout.AddBinding(0, sizeof(NkVertex3D), false)
				.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0);
			mSelMaskPipeline = mDevice->CreateGraphicsPipeline(pd);
			mSelMaskPipelineRP = currentRP;
			logger.Info("[NkRender3D] SelMask pipeline create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
						mSelMaskShader.IsValid() ? 1 : 0, mSelMaskPipeline.IsValid() ? 1 : 0, currentRP.id);
			return mSelMaskPipeline.IsValid();
		}

		void NkRender3D::RenderSelectionMask(NkICommandBuffer *cmd) {
			if (!cmd || !mSelOutline || mSelection.Empty() || !mMesh)
				return;
			NkRenderPassHandle rp{};
			if (mGraph)
				rp = mGraph->GetPassRenderPass("SelectionMask");
			if (!EnsureSelMaskPipeline(rp))
				return;
			cmd->BindGraphicsPipeline(mSelMaskPipeline);
			// set 0 = per-frame (CameraUBO). Même slot que le Flush principal (déjà
			// uploadé cette frame) -> viewProj identique -> masque aligné pixel-exact.
			NkDescSetHandle gs = (mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
			if (gs.IsValid())
				cmd->BindDescriptorSet(gs, 0);
			for (auto &dc : mSelection) {
				if (!dc.mesh.IsValid())
					continue;
				struct MaskPC {
						NkMat4f model;
						NkVec4f level; // .x = 1.0 (actif) ou 0.5 (selectionne)
				} mpc;
				mpc.model = dc.transform;
				const uint32 di = (uint32)(&dc - mSelection.Data());
				const bool act = (di < (uint32)mSelectionActive.Size()) && mSelectionActive[di] != 0;
				mpc.level = {act ? 1.f : 0.5f, 0.f, 0.f, 0.f};
				cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(mpc), &mpc);
				mMesh->BindMesh(cmd, dc.mesh);
				if (dc.subMeshIdx == 0xFFFFFFFFu)
					mMesh->DrawAll(cmd, dc.mesh);
				else
					mMesh->DrawSubMesh(cmd, dc.mesh, dc.subMeshIdx);
			}
		}

		// Pipeline OUTLINE : fullscreen triangle (comme Blit/FXAA), lit le masque au
		// binding 0, alpha-blend du liseré sur la cible finale. Layout+set du sampler
		// créés paresseusement ici (dédiés : pas de partage -> pas d'écrasement au
		// Submit sur les backends à commandes différées, cf. NkPostProcessStack).
		bool NkRender3D::EnsureSelOutlinePipeline(NkRenderPassHandle currentRP) {
			if (mSelOutlinePipeline.IsValid() && mSelOutlinePipelineRP == currentRP)
				return true;
			if (!mShaderLib)
				return false;
			if (!mSelOutlineShader.IsValid()) {
				auto prog = mShaderLib->LoadOrCompileVF("SelOutline", "", "");
				if (!prog.IsValid()) {
					logger.Errorf("[NkR3D::SelOutline] shader compile FAIL\n");
					return false;
				}
				mSelOutlineShader = mShaderLib->GetRHIHandle(prog);
			}
			if (!mSelOutlineShader.IsValid())
				return false;
			if (!mSelTexLayout.IsValid()) {
				NkDescriptorSetLayoutDesc layout;
				layout.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
				mSelTexLayout = mDevice->CreateDescriptorSetLayout(layout);
				mSelTexSet = mDevice->AllocateDescriptorSet(mSelTexLayout);
			}
			if (mSelOutlinePipeline.IsValid()) {
				mDevice->DestroyPipeline(mSelOutlinePipeline);
				mSelOutlinePipeline = {};
			}
			NkGraphicsPipelineDesc pd;
			pd.shader = mSelOutlineShader;
			pd.depthStencil = NkDepthStencilDesc::NoDepth();
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Alpha(); // liseré composité par dessus l'image finale
			pd.debugName = "SelOutline";
			pd.renderPass = currentRP;
			// params + color : DEUX vec4, pas plus. Sur le chemin OpenGL, au-dela de
			// cette taille les valeurs ne sont pas livrees de facon fiable — le shader
			// derive donc lui-meme la teinte de l'objet actif.
			pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkVec4f) * 2);
			if (mSelTexLayout.IsValid())
				pd.descriptorSetLayouts.PushBack(mSelTexLayout);
			// Pas de vertex layout (triangle plein-écran généré via gl_VertexID).
			mSelOutlinePipeline = mDevice->CreateGraphicsPipeline(pd);
			mSelOutlinePipelineRP = currentRP;
			logger.Info("[NkRender3D] SelOutline pipeline create: shader_valid={0} pipeline_valid={1} rp.id={2}\n",
						mSelOutlineShader.IsValid() ? 1 : 0, mSelOutlinePipeline.IsValid() ? 1 : 0, currentRP.id);
			return mSelOutlinePipeline.IsValid();
		}

		void NkRender3D::CompositeSelectionOutline(NkICommandBuffer *cmd, NkTextureHandle maskTex) {
			if (!cmd || !mSelOutline || !maskTex.IsValid())
				return;
			NkRenderPassHandle rp{};
			if (mGraph)
				rp = mGraph->GetPassRenderPass("SelectionOutline");
			if (!EnsureSelOutlinePipeline(rp))
				return;
			// Filet de sécurité : si aucune propagation de taille n'a eu lieu (mW/mH
			// à 0), l'edge-detect diviserait par 1 -> offsets d'échantillonnage à
			// l'échelle de tout l'écran -> liseré invisible. On retombe alors sur la
			// taille swapchain (== taille de rendu hors mode SetRenderSizeOverride).
			if ((mW == 0 || mH == 0) && mDevice) {
				uint32 sw = mDevice->GetSwapchainWidth();
				uint32 sh = mDevice->GetSwapchainHeight();
				if (sw > 0 && sh > 0) {
					mW = sw;
					mH = sh;
				}
			}
			if (mSelTexSet.IsValid() && mResources) {
				NkSamplerHandle samp = mResources->GetSamplerLinearClamp();
				if (samp.IsValid())
					mDevice->BindTextureSampler(mSelTexSet, 0, maskTex, samp);
			}
			cmd->BindGraphicsPipeline(mSelOutlinePipeline);
			if (mSelTexSet.IsValid())
				cmd->BindDescriptorSet(mSelTexSet, 0);

			// yFlipUV : le masque et la cible finale sont des offscreens de MÊME
			// orientation. Sur GL (origine bas-gauche partout) l'UV direct aligne le
			// masque sur la scène ; VK/DX rendent la cible Y-flippée -> flip vertical.
			const bool isGL = mDevice && mDevice->GetApi() == ::nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL;
			struct OutlinePC {
					NkVec4f params; // .x=invResW .y=invResH .z=thicknessPx .w=yFlipUV
					NkVec4f color; // teinte de base ; le shader en derive l'actif
			} pc;
			pc.params = {1.f / (float32)(mW > 0 ? mW : 1), 1.f / (float32)(mH > 0 ? mH : 1), mSelOutlineThickness,
						 isGL ? 1.f : -1.f};
			pc.color = mSelOutlineColor;
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);
			cmd->Draw(3, 1, 0, 0);
		}

		void NkRender3D::Shutdown() {
			if (mSkyboxPipeline.IsValid()) {
				mDevice->DestroyPipeline(mSkyboxPipeline);
				mSkyboxPipeline = {};
			}
			if (mGridPipeline.IsValid()) {
				mDevice->DestroyPipeline(mGridPipeline);
				mGridPipeline = {};
			}
			if (mSkinPipeline.IsValid()) {
				mDevice->DestroyPipeline(mSkinPipeline);
				mSkinPipeline = {};
			}
			if (mShadowPipeline.IsValid()) {
				mDevice->DestroyPipeline(mShadowPipeline);
				mShadowPipeline = {};
			}
			if (mShadowAlphaPipeline.IsValid()) {
				mDevice->DestroyPipeline(mShadowAlphaPipeline);
				mShadowAlphaPipeline = {};
			}
			if (mShadowInstancePipeline.IsValid()) {
				mDevice->DestroyPipeline(mShadowInstancePipeline);
				mShadowInstancePipeline = {};
			}
			if (mPBRPipeline.IsValid()) {
				mDevice->DestroyPipeline(mPBRPipeline);
				mPBRPipeline = {};
			}
			// Sélection outline silhouette : pipelines + layout/set du sampler masque.
			if (mSelMaskPipeline.IsValid()) {
				mDevice->DestroyPipeline(mSelMaskPipeline);
				mSelMaskPipeline = {};
			}
			if (mSelOutlinePipeline.IsValid()) {
				mDevice->DestroyPipeline(mSelOutlinePipeline);
				mSelOutlinePipeline = {};
			}
			if (mSelTexSet.IsValid()) {
				mDevice->FreeDescriptorSet(mSelTexSet);
				mSelTexSet = {};
			}
			if (mSelTexLayout.IsValid()) {
				mDevice->DestroyDescriptorSetLayout(mSelTexLayout);
				mSelTexLayout = {};
			}
			// Les shader handles sont detenus par NkShaderLibrary, pas a detruire ici.
			for (auto &s : mGlobalSetRing)
				if (s.IsValid())
					mDevice->FreeDescriptorSet(s);
			for (auto &s : mGlobalSetMirrorRing)
				if (s.IsValid())
					mDevice->FreeDescriptorSet(s);
			// Phase F.B.1 : free le pool object 2D (frame x drawIdx).
			for (auto &perFrame : mObjectSetPool) {
				for (auto &s : perFrame)
					if (s.IsValid())
						mDevice->FreeDescriptorSet(s);
				perFrame.Clear();
			}
			mGlobalSetRing.Clear();
			mGlobalSetMirrorRing.Clear();
			mObjectSetPool.Clear();
			if (mGlobalLayout.IsValid()) {
				mDevice->DestroyDescriptorSetLayout(mGlobalLayout);
			}
			if (mObjectLayout.IsValid()) {
				mDevice->DestroyDescriptorSetLayout(mObjectLayout);
			}
			for (auto &b : mUBOCameraRing)
				if (b.IsValid())
					mDevice->DestroyBuffer(b);
			for (auto &b : mUBOCameraMirrorRing)
				if (b.IsValid())
					mDevice->DestroyBuffer(b);
			for (auto &perFrame : mUBOObjectPool) {
				for (auto &b : perFrame)
					if (b.IsValid())
						mDevice->DestroyBuffer(b);
				perFrame.Clear();
			}
			for (auto &b : mUBOLightsRing)
				if (b.IsValid())
					mDevice->DestroyBuffer(b);
			mUBOCameraRing.Clear();
			mUBOCameraMirrorRing.Clear();
			mUBOObjectPool.Clear();
			mUBOLightsRing.Clear();
			for (auto &b : mUBOBonesRing)
				if (b.IsValid())
					mDevice->DestroyBuffer(b);
			mUBOBonesRing.Clear();
			for (auto &perFrame : mUBOShadowInstPool) {
				for (auto &b : perFrame)
					if (b.IsValid())
						mDevice->DestroyBuffer(b);
				perFrame.Clear();
			}
			mUBOShadowInstPool.Clear();
			if (mDefaultCubeWhite.IsValid()) {
				mDevice->DestroyTexture(mDefaultCubeWhite);
				mDefaultCubeWhite = {};
			}

			// DEBUG triangle resources
			if (mDebugPipeline.IsValid()) {
				mDevice->DestroyPipeline(mDebugPipeline);
				mDebugPipeline = {};
			}
			if (mDebugVBO.IsValid()) {
				mDevice->DestroyBuffer(mDebugVBO);
				mDebugVBO = {};
			}
			if (mDebugIBO.IsValid()) {
				mDevice->DestroyBuffer(mDebugIBO);
				mDebugIBO = {};
			}
			// Lignes + triangles debug (gizmos/cage/faces éditeur).
			if (mLinePipeline.IsValid()) {
				mDevice->DestroyPipeline(mLinePipeline);
				mLinePipeline = {};
			}
			if (mLinePipelineNoDepth.IsValid()) {
				mDevice->DestroyPipeline(mLinePipelineNoDepth);
				mLinePipelineNoDepth = {};
			}
			for (uint32 s = 0; s < (uint32)mLineVBORing.Size(); s++)
				if (mLineVBORing[s].IsValid())
					mDevice->DestroyBuffer(mLineVBORing[s]);
			mLineVBORing.Clear();
			mLineVBORingCap.Clear();
			if (mTriPipeline.IsValid()) {
				mDevice->DestroyPipeline(mTriPipeline);
				mTriPipeline = {};
			}
			if (mTriPipelineNoDepth.IsValid()) {
				mDevice->DestroyPipeline(mTriPipelineNoDepth);
				mTriPipelineNoDepth = {};
			}
			for (uint32 s = 0; s < (uint32)mTriVBORing.Size(); s++)
				if (mTriVBORing[s].IsValid())
					mDevice->DestroyBuffer(mTriVBORing[s]);
			mTriVBORing.Clear();
			mTriVBORingCap.Clear();
			for (uint32 s = 0; s < (uint32)mNgonWireRing.Size(); s++)
				if (mNgonWireRing[s].IsValid())
					mDevice->DestroyBuffer(mNgonWireRing[s]);
			mNgonWireRing.Clear();
			mNgonWireRingCap.Clear();
			mNgonWireDirtyLo.Clear();
			mNgonWireDirtyHi.Clear();
			mNgonWireCPU.Clear();
			mNgonWireN = 0;
			{
				NkVector<NkBufferHandle> *rings[3] = {&mEditLineRing, &mEditTriRing, &mEditPointRing};
				NkVector<uint32> *caps[3] = {&mEditLineRingCap, &mEditTriRingCap, &mEditPointRingCap};
				NkVector<uint8> *dirt[3] = {&mEditLineDirty, &mEditTriDirty, &mEditPointDirty};
				NkVector<float32> *cpus[3] = {&mEditLineCPU, &mEditTriCPU, &mEditPointCPU};
				for (uint32 k = 0; k < 3u; k++) {
					for (uint32 s = 0; s < (uint32)rings[k]->Size(); s++)
						if ((*rings[k])[s].IsValid())
							mDevice->DestroyBuffer((*rings[k])[s]);
					rings[k]->Clear();
					caps[k]->Clear();
					dirt[k]->Clear();
					cpus[k]->Clear();
				}
				mEditLineN = 0;
				mEditTriN = 0;
				mEditPointN = 0;
			}
			if (mEditPointPipeline.IsValid()) {
				mDevice->DestroyPipeline(mEditPointPipeline);
				mEditPointPipeline = {};
			}
			if (mEditPointPipelineNoDepth.IsValid()) {
				mDevice->DestroyPipeline(mEditPointPipelineNoDepth);
				mEditPointPipelineNoDepth = {};
			}
			mDebugInited = false;
		}

		// ── Scene ─────────────────────────────────────────────────────────────────
		void NkRender3D::GrowObjectPool(uint32 newCap) {
			if (newCap > kObjectPoolHardMax)
				newCap = kObjectPoolHardMax;
			if (newCap <= mObjectPoolCap || !mDevice)
				return;

			// Alloc les nouveaux buffers + descriptor sets pour CHAQUE frame-in-flight
			// (HORS render pass — appelé depuis ResetFrame). Réplique le pré-bind d'Init :
			// binding1 = ObjectUBO du slot, binding4 = bones ring de la frame (défaut ;
			// réécrit dynamiquement par les draws instanciés/skinnés).
			for (uint32 i = 0; i < mFramesInFlight; i++) {
				const uint32 old = (uint32)mUBOObjectPool[i].Size();
				mUBOObjectPool[i].Resize(newCap);
				mObjectSetPool[i].Resize(newCap);
				for (uint32 d = old; d < newCap; d++) {
					mUBOObjectPool[i][d] = mDevice->CreateBuffer(NkBufferDesc::Uniform(kObjectUBOBytes));
					NkDescSetHandle os = mDevice->AllocateDescriptorSet(mObjectLayout);
					mObjectSetPool[i][d] = os;
					if (os.IsValid()) {
						if (mUBOObjectPool[i][d].IsValid())
							mDevice->BindUniformBuffer(os, 1, mUBOObjectPool[i][d]);
						if (i < mUBOBonesRing.Size() && mUBOBonesRing[i].IsValid())
							mDevice->BindUniformBuffer(os, 4, mUBOBonesRing[i]);
					}
				}
			}
			logger.Info("[NkRender3D] Object pool grown: {0} -> {1} (x{2} frames)\n", mObjectPoolCap, newCap,
						mFramesInFlight);
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
			mShadowInstIdx = 0; // pool d'instances shadow : reset pour la nouvelle frame
		}

		void NkRender3D::BeginScene(const NkSceneContext &ctx) {
			mCtx = ctx;
			mInScene = true;
			// TAA : une phase de jitter par FRAME (ici, pas dans UploadUBOs qui peut
			// etre appele plusieurs fois — passe miroir — et jitterait alors chaque
			// passe differemment, ce qui casserait la coherence de la profondeur).
			if (mTAAJitter)
				mTAAJitterIdx++;
			mOpaque.Clear();
			mTransparent.Clear();
			mShadowCasters.Clear();
			mInstanced.Clear();
			mSkinned.Clear();
			mSelection.Clear(); // file de sélection (outline silhouette) : re-soumise par frame
			mSelectionActive.Clear(); // marqueur d'objet actif, parallèle à mSelection
			mCullStats = NkCullStats{}; // stats de culling : nouvelles soumissions
			mShadowStamp = 1469598103934665603ull; // FNV-1a : nouvelle empreinte
			// mObjectDrawIdx N'EST PAS reset ici — voir ResetFrame() ci-dessus.
		}

		// ── Submit ────────────────────────────────────────────────────────────────
		void NkRender3D::Submit(const NkDrawCall3D &dc) {
			if (!dc.visible)
				return;

			NkVec3f camPos = mCtx.camera.GetPosition();
			NkVec3f center = dc.aabb.Center();
			float32 dx = center.x - camPos.x, dy = center.y - camPos.y, dz = center.z - camPos.z;
			float32 depth = dx * dx + dy * dy + dz * dz;

			// Caster d'ombre : collecte AVANT le culling camera. Un objet hors
			// champ camera peut projeter une ombre dans la zone visible ; il
			// doit donc entrer dans la passe shadow meme s'il est cull du rendu
			// principal. (Cause racine "objets sans ombre" : avant, le culling
			// camera retirait le caster de mOpaque, et la passe shadow iterait
			// sur mOpaque.)
			if (dc.castShadow) {
				mShadowCasters.PushBack({dc, depth});
				// EMPREINTE DE LA GEOMETRIE OMBRANTE. Le cache NkVSM ne surveillait
				// que la LUMIERE : un objet deplace laissait donc son ombre figee a
				// son ancienne place (ombre "detachee" constatee par Rihen). On
				// resume ici, au seul endroit ou passent tous les casters, la pose
				// et l'identite de chacun ; le VSM invalide son cache des que ce
				// resume change.
				for (int32 k = 0; k < 16; ++k) {
					uint32 bits = 0;
					std::memcpy(&bits, &dc.transform.data[k], sizeof(bits));
					mShadowStamp = (mShadowStamp ^ (uint64)bits) * 1099511628211ull;
				}
				mShadowStamp = (mShadowStamp ^ dc.mesh.id) * 1099511628211ull;
			}

			// Culling camera : uniquement pour le rendu visible (mOpaque).
			mCullStats.opaqueSubmitted++;
			if (!mCtx.camera.IsAABBVisible(dc.aabb)) {
				mCullStats.opaqueCulled++;
				return;
			}
			mOpaque.PushBack({dc, depth});
		}

		void NkRender3D::SubmitMany(const NkDrawCall3D *dcs, uint32 count) {
			for (uint32 i = 0; i < count; i++)
				Submit(dcs[i]);
		}

		void NkRender3D::SubmitInstanced(const NkDrawCallInstanced &dc) {
			mInstanced.PushBack(dc);
		}

		NkAABB NkRender3D::GetShadowCasterBounds() const {
			NkAABB b; // min = +inf, max = -inf (cf. NkRendererTypes.h)
			for (const auto &sdc : mShadowCasters)
				b.Merge(sdc.dc.aabb);
			for (const auto &idc : mInstanced)
				b.Merge(idc.aabb);
			// Aucun caster -> AABB "inverse" (min>max) ; retourne un cube unite.
			if (b.min.x > b.max.x) {
				b.min = {-1.f, -1.f, -1.f};
				b.max = {1.f, 1.f, 1.f};
			}
			return b;
		}

		void NkRender3D::SubmitSkinned(const NkDrawCallSkinned &dc) {
			mSkinned.PushBack(dc);
		}

		void NkRender3D::SubmitSkinnedTinted(const NkDrawCallSkinned &dc, NkVec3f tint, float32 alpha) {
			NkDrawCallSkinned copy = dc;
			copy.tint = tint;
			copy.alpha = alpha;
			mSkinned.PushBack(copy);
		}

		// ── Sort ──────────────────────────────────────────────────────────────────
		void NkRender3D::SortDrawCalls() {
			for (uint32 i = 1; i < (uint32)mOpaque.Size(); i++) {
				SortedDC key = mOpaque[i];
				int32 j = (int32)i - 1;
				while (j >= 0 && mOpaque[j].depth > key.depth) {
					mOpaque[j + 1] = mOpaque[j];
					j--;
				}
				mOpaque[j + 1] = key;
			}
		}

		// ── Flush ────────────────────────────────────────────────────────────────
		// =====================================================================
		// DEFERRED v1 — G-buffer fill + lighting fullscreen + reste forward.
		// Sequence par frame (RebuildRenderGraph, branche cfg.deferred) :
		//   DeferredGeom -> DeferredLight -> ForwardRest.
		// =====================================================================
		void NkRender3D::FlushDeferredGeometry(NkICommandBuffer *cmd) {
			if (!mInScene || !cmd)
				return;
			if (mDevice && mFramesInFlight > 0)
				mFrameSlot = mDevice->GetFrameIndex() % mFramesInFlight;
			SortDrawCalls();
			UploadUBOs(cmd);

			NkRenderPassHandle rp{};
			if (mGraph)
				rp = mGraph->GetPassRenderPass("DeferredGeom");
			if (!EnsureDeferredGeomPipeline(rp))
				return;

			cmd->BindGraphicsPipeline(mDeferredGeomPipeline);
			NkDescSetHandle gs = (mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
			if (gs.IsValid())
				cmd->BindDescriptorSet(gs, 0);

			// Meme ObjBlock 224B que FlushOpaque/RenderShadowPass (linker GL).
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
			static_assert(sizeof(ObjBlock) == 224, "ObjBlock std140 deferred");

			const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size()) && (mFrameSlot < mObjectSetPool.Size());
			if (!poolFrameValid)
				return;

			for (auto &sdc : mOpaque) {
				auto &dc = sdc.dc;
				// PAS de re-cull ici : mOpaque est DEJA culle au Submit. Un double
				// cull avec l'etat camera du moment faisait DISPARAITRE des objets
				// valides selon l'angle de vue (bug panneau, constate DX11).
				if (mObjectDrawIdx >= mObjectPoolCap) {
					logger.Errorf("[NkRender3D] ObjectUBO pool overflow (deferred)\n");
					break;
				}
				NkMaterialInstance *matInst = nullptr;
				if (dc.material.IsValid() && mMat)
					matInst = mMat->GetInstance(dc.material);
				if (!matInst && mMat) {
					if (!mFallbackMatInst.IsValid()) {
						auto *inst = mMat->CreateInstance(mMat->DefaultPBR());
						if (inst)
							mFallbackMatInst = inst->GetHandle();
					}
					matInst = mMat->GetInstance(mFallbackMatInst);
				}

				ObjBlock ob{};
				ob.model = dc.transform;
				ob.normalMatrix = dc.transform.Inverse().Transpose();
				ob.tint = {dc.tint.x, dc.tint.y, dc.tint.z, 1.f};
				ob.metallic = dc.metallic;
				ob.roughness = dc.roughness;
				ob.aoStrength = dc.aoStrength;
				ob.emissiveStrength = 0.f;				 // emissive via materiau (v1)
				ob.normalStrength = matInst ? 1.f : 0.f; // normal map si materiau
				ob.shadowOverrides = NkVec4f{1.f, 0.f, 1.f, 0.f};

				NkBufferHandle ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
				NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
				if (ubo.IsValid())
					mDevice->WriteBuffer(ubo, &ob, sizeof(ob), 0);
				if (matInst && matInst->GetDescSet().IsValid()) {
					// Upload UBO/textures de l'instance si dirty : on ne passe PAS
					// par BindInstance (pipeline unique G-buffer) — sans ca les
					// textures ne sont jamais ecrites (albedo blanc, bug panneau).
					mMat->UpdateInstanceDescriptors(matInst);
					cmd->BindDescriptorSet(matInst->GetDescSet(), 2);
				}
				if (os.IsValid())
					cmd->BindDescriptorSet(os, 1);
				mMesh->BindMesh(cmd, dc.mesh);
				if (dc.subMeshIdx == 0xFFFFFFFFu)
					mMesh->DrawAll(cmd, dc.mesh);
				else
					mMesh->DrawSubMesh(cmd, dc.mesh, dc.subMeshIdx);
				mObjectDrawIdx++;
			}
			// mInScene reste VRAI : FlushForwardRest termine la frame.
		}

		void NkRender3D::RenderDeferredLighting(NkICommandBuffer *cmd, ::nkentseu::NkTextureHandle texA,
												::nkentseu::NkTextureHandle texN, ::nkentseu::NkTextureHandle texE,
												::nkentseu::NkTextureHandle texD) {
			if (!cmd)
				return;
			NkRenderPassHandle rp{};
			if (mGraph)
				rp = mGraph->GetPassRenderPass("DeferredLight");
			if (!EnsureDeferredLightPipeline(rp))
				return;

			// Ecrit les 4 textures du G-buffer dans le set (refait chaque frame,
			// pattern mToneSet du post-process).
			if (mGBufSet.IsValid() && mResources) {
				NkSamplerHandle samp = mResources->GetSamplerLinearClamp();
				mDevice->BindTextureSampler(mGBufSet, 0, texA, samp);
				mDevice->BindTextureSampler(mGBufSet, 1, texN, samp);
				mDevice->BindTextureSampler(mGBufSet, 2, texE, samp);
				mDevice->BindTextureSampler(mGBufSet, 3, texD, samp);
			}

			cmd->BindGraphicsPipeline(mDeferredLightPipeline);
			NkDescSetHandle gs = (mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
			if (gs.IsValid())
				cmd->BindDescriptorSet(gs, 0);
			if (mGBufSet.IsValid())
				cmd->BindDescriptorSet(mGBufSet, 1);

			struct PC {
					float32 invResW, invResH, yFlipUV, _pad;
			} pc;
			// Conventions PAR BACKEND (validees capture) :
			//   GL : sample sans flip, NDC Y = -(uv*2-1) (VS 3D negate + flip codegen)
			//   VK : sample sans flip, NDC Y = +(uv*2-1) (pas de negate VS en SPIRV)
			//   DX : sample FLIPPE,   NDC Y = -(uv*2-1) (VS HLSL negate Y)
			const NkGraphicsApi dApi = mDevice ? mDevice->GetApi() : NkGraphicsApi::NK_GFX_API_OPENGL;
			const bool dIsVK = (dApi == NkGraphicsApi::NK_GFX_API_VULKAN);
			const bool dIsDX =
				(dApi == NkGraphicsApi::NK_GFX_API_DX11) || (dApi == NkGraphicsApi::NK_GFX_API_DX12);
			// VK valide capture : memes conventions que DX (sample flippe +
			// ndcY negatif) — l'essai sample direct donnait l'image inversee.
			// ndcYSign PAR BACKEND (consomme par le shader, pc.invResolution.x) :
			// le VS flippe vUV sur DX pour echantillonner le G-buffer -> le signe
			// NDC s'inverse pour retrouver la position ECRAN. -1 fixe donnait un
			// worldPos MIROITE sur DX -> rayons parasites du spot cookie.
			pc.invResW = dIsDX ? 1.f : -1.f; // = ndcYSign
			pc.invResH = 0.f;
			pc.yFlipUV = (dIsDX || dIsVK) ? -1.f : 1.f;
			static int sDbg = -1;
			if (sDbg < 0) {
				const char *v = getenv("NK_DEFLIGHT_DEBUG");
				sDbg = (v && v[0]) ? atoi(v) : 0;
			}
			pc._pad = (float32)sDbg; // 1=N, 2=worldPos, 3=albedo (diag)
			cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(pc), &pc);
			cmd->Draw(3, 1, 0, 0);
		}

		void NkRender3D::FlushForwardRest(NkICommandBuffer *cmd) {
			if (!mInScene || !cmd)
				return;
			NkRenderPassHandle currentRP{};
			if (mGraph)
				currentRP = mGraph->GetPassRenderPass("ForwardRest");

			if (!mSkinned.Empty())
				EnsureSkinPipeline(currentRP);
			if (mDrawSkybox) {
				EnsureSkyboxPipeline(currentRP);
				DrawSkybox(cmd);
			}
			if (mMat)
				mMat->UpdateRenderPass(currentRP);
			if (mPBRPipeline.IsValid())
				cmd->BindGraphicsPipeline(mPBRPipeline);
			NkDescSetHandle gs = (mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
			if (gs.IsValid())
				cmd->BindDescriptorSet(gs, 0);
			FlushInstanced(cmd);
			FlushSkinned(cmd);
			if (mDrawGrid) {
				EnsureGridPipeline(currentRP);
				DrawGrid(cmd);
				if (gs.IsValid())
					cmd->BindDescriptorSet(gs, 0);
			}
			FlushTransparent(cmd);
			// Overlays : emis ICI seulement si l'ancien chemin est demande. Par defaut
			// ils partent dans la passe Overlay3D, apres le post-process, pour ne pas
			// etre bloomes (cf. SetOverlayAfterPost).
			if (!mPendingMirrorActive && !mOverlayAfterPost)
				FlushDebug(cmd, currentRP, gs);
			mInScene = false;
		}

		// Emet les overlays 3D dans la passe dediee. Le RP est celui d'« Overlay3D » :
		// les pipelines d'overlay sont caches PAR RENDER PASS, il faut donc les creer
		// pour cette passe — c'est ce que font les Ensure*Pipeline appelees par
		// FlushDebug. Sur OpenGL le RP vaut {} et le backend l'ignore, comme ailleurs.
		void NkRender3D::FlushOverlay3D(NkICommandBuffer *cmd) {
			if (!mOverlayAfterPost)
				return;
			NkRenderPassHandle rp{};
			if (mGraph)
				rp = mGraph->GetPassRenderPass("Overlay3D");
			const NkDescSetHandle gs =
				(mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
			if (gs.IsValid())
				cmd->BindDescriptorSet(gs, 0);
			FlushDebug(cmd, rp, gs);
		}

		void NkRender3D::Flush(NkICommandBuffer *cmd) {
			if (!mInScene)
				return;
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
			if (mGraph)
				currentRP = mGraph->GetPassRenderPass("Geometry");
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
					if constexpr (kDebugTriangleMode == 1)
						DebugDrawTriangleNoIdx(cmd);
					else
						DebugDrawTriangleIdx(cmd);
				}
				mInScene = false;
				mFrameSlot = (mFrameSlot + 1) % mFramesInFlight;
				return;
			}

			EnsurePBRPipeline(currentRP);
			// Skinning : cree (lazy) le pipeline skin compatible avec ce RP.
			// No-op si aucun shader skin (build sans assets) ou deja cree.
			if (!mSkinned.Empty())
				EnsureSkinPipeline(currentRP);

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
				if (gpuInst == -1) {
					const char *v = getenv("NK_INSTANCING_GPU_1DRAW");
					gpuInst = (v && v[0] && v[0] != '0') ? 1 : 0;
				}
				if (gpuInst && !mInstanced.Empty())
					EnsureInstancePipeline(currentRP);
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
			if (mMat)
				mMat->UpdateRenderPass(currentRP);

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
				gs = (mFrameSlot < mGlobalSetMirrorRing.Size()) ? mGlobalSetMirrorRing[mFrameSlot] : NkDescSetHandle{};
			} else {
				gs = (mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};
			}
			if (gs.IsValid())
				cmd->BindDescriptorSet(gs, 0);

			// Mode d'affichage wireframe : propage au material system (c'est lui qui binde
			// le pipeline final par BindInstance -> il doit choisir la variante fil-de-fer).
			if (mMat)
				mMat->SetWireframe(mWireframe && !mNgonWire);
			// WIREFRAME N-GON : les maillages ne sont PAS rasterises (ni pleins, ni en fil
			// de fer triangulaire) ; seul le batch d'aretes n-gon est dessine (cf.
			// FlushDebug). C'est la seule facon d'obtenir un fil de fer SANS diagonale :
			// le rasteriseur, lui, ne connait que des triangles.
			// DIAG (NK_WIRE_DRAWDIAG=1) : que reste-t-il RASTERISE en mode fil de fer ?
			{
				static int32 diag = -1;
				if (diag == -1) {
					const char *dv = getenv("NK_WIRE_DRAWDIAG");
					diag = (dv && dv[0] && dv[0] != '0') ? 1 : 0;
				}
				if (diag) {
					static uint32 nD = 0;
					if (nD < 40u)
						logger.Info("[WireRaster] n={0} ngonWire={1} opaque={2} instanced={3} skinned={4} "
									"transparent={5}\n",
									(int32)nD, mNgonWire ? 1 : 0, (int32)mOpaque.Size(), (int32)mInstanced.Size(),
									(int32)mSkinned.Size(), (int32)mTransparent.Size());
					nD++;
				}
			}
			if (!mNgonWire) {
				FlushOpaque(cmd);
				FlushInstanced(cmd);
				FlushSkinned(cmd);
			}
			// Grille infinie : APRÈS l'opaque (occlusion correcte par les objets),
			// AVANT le transparent (le transparent se blend par-dessus la grille).
			if (mDrawGrid) {
				EnsureGridPipeline(currentRP);
				DrawGrid(cmd);
				// Le grid a re-bindé son pipeline/PC ; on rétablit le set global 0
				// pour les passes suivantes (transparent/debug le rebindent au besoin).
				if (gs.IsValid())
					cmd->BindDescriptorSet(gs, 0);
			}
			FlushTransparent(cmd);
			// Overlays debug/édition : PAS dans la passe miroir (ce sont des
			// aides d'éditeur, pas du contenu de scène — un reflet ne doit pas
			// les montrer). Accessoirement, FlushDebug décrémente la vie des
			// primitives one-frame et les purge : s'il tournait dans la passe
			// miroir (rendue AVANT la vue principale), il les CONSOMMAIT et la
			// vue principale ne les affichait jamais (bug « cercle vert visible
			// seulement dans le miroir », Demo4/5 surlignage matériau actif).
			// Overlays : emis ICI seulement si l'ancien chemin est demande. Par defaut
			// ils partent dans la passe Overlay3D, apres le post-process, pour ne pas
			// etre bloomes (cf. SetOverlayAfterPost).
			if (!mPendingMirrorActive && !mOverlayAfterPost)
				FlushDebug(cmd, currentRP, gs);
			mInScene = false;

			// NOTE : plus d'auto-avance de mFrameSlot ici. Il est desormais derive
			// de mDevice->GetFrameIndex() au DEBUT de Flush (sync avec la fence du
			// device), ce qui corrige le flicker Vulkan (ecriture d'un slot in-flight).
		}

		void NkRender3D::Flush(NkICommandBuffer *cmd, NkRenderPassHandle renderPass) {
			// Render-to-texture : expose le RP du RT a Flush(cmd) via mPendingRP.
			// Il sera utilise UNIQUEMENT si le Geometry RP du RenderGraph n'est
			// pas encore disponible (FB lazy, 1re frame). Le RT doit avoir des
			// formats compatibles avec Geometry pass (HDR R16G16B16A16 + D32_FLOAT)
			// pour que le pipeline reste utilisable a la 2e passe sans recompile.
			mPendingRP = renderPass;
			Flush(cmd);
			mPendingRP = {};
		}

		void NkRender3D::SetMaterialCollection(NkMaterialCollection *mpc) {
			if (!mpc)
				return;
			const NkBufferHandle ubo = mpc->GetUBO();
			if (!ubo.IsValid())
				return;
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
		void NkRender3D::SetVoxelAO(NkVoxelAOSystem *vao) {
			mVoxelAO = vao;
			if (!mVoxelAO)
				return;
			NkTextureHandle tex = mVoxelAO->GetVoxelTexture();
			NkSamplerHandle samp = mVoxelAO->GetVoxelSampler();
			if (!tex.IsValid() || !samp.IsValid())
				return;
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
			// Une texture utilisateur est une boule SEULE, pas un atlas 6x5 : le shader
			// doit cesser d'appliquer la transformation de tuile, sinon il n'en lirait
			// qu'un trentieme. C'est le role de mMatcapCustom (-> uCam.viewOpts.x).
			mMatcapCustom = tex.IsValid();
			NkTextureHandle bind = tex.IsValid() ? tex : mMatcapTex; // fallback = atlas genere
			if (!bind.IsValid() || !mResources)
				return;
			NkSamplerHandle samp = mResources->GetSamplerLinearClamp();
			if (!samp.IsValid())
				return;
			for (uint32 i = 0; i < mFramesInFlight; i++) {
				if (i < mGlobalSetRing.Size() && mGlobalSetRing[i].IsValid())
					mDevice->BindTextureSampler(mGlobalSetRing[i], 28, bind, samp);
				if (i < mGlobalSetMirrorRing.Size() && mGlobalSetMirrorRing[i].IsValid())
					mDevice->BindTextureSampler(mGlobalSetMirrorRing[i], 28, bind, samp);
			}
		}

		void NkRender3D::FlushIntoRT(NkICommandBuffer *cmd, NkRenderPassHandle rp, const NkMat4f &mirrorMat,
									 const NkMat4f &mirrorViewProj, const NkVec4f &clipPlane) {
			if (!mInScene)
				return;

			// Sauve l'etat de scene pour permettre le Flush principal apres.
			// mObjectDrawIdx avance dans le pool ; on le rewind PAS pour que
			// les UBO de la passe miroir et celle principale ne se chevauchent.
			const bool savedInScene = mInScene;
			const uint32 savedSlot = mFrameSlot;
			const uint32 savedDrawIdx = mObjectDrawIdx;

			// Active le mode mirror : FlushOpaque/Skinned/etc. pre-multiplie chaque
			// transform par mPendingMirror. mPendingMirrorViewProj est upload au
			// CameraUBO pour permettre aux shaders qui en ont besoin (ReflFloor
			// par exemple) de connaitre la projection miroir — mais durant la
			// PASSE MIROIR elle-meme, le sol ne devrait pas etre present.
			mPendingMirror = mirrorMat;
			mPendingMirrorActive = true;
			mPendingMirrorViewProj = mirrorViewProj;
			mPendingClipPlane = clipPlane;
			mPendingRP = rp;

			Flush(cmd);

			mPendingRP = {};
			mPendingMirror = NkMat4f::Identity();
			mPendingMirrorActive = false;
			mPendingMirrorViewProj = NkMat4f::Identity();
			mPendingClipPlane = {0.f, 0.f, 0.f, 0.f};

			// Restore l'etat de scene pour que le Flush principal puisse continuer.
			// On garde mObjectDrawIdx avance pour que les UBOs deja ecrits par la
			// passe miroir ne soient pas overwrites par la passe principale (qui
			// est encore en flight cote GPU).
			mInScene = savedInScene;
			mFrameSlot = savedSlot;
			// mObjectDrawIdx reste avance volontairement (cf. ci-dessus).
			(void)savedDrawIdx;
		}

		void NkRender3D::UploadUBOs(NkICommandBuffer *cmd) {
			(void)cmd;

			// Camera UBO — layout std140 qui matche EXACTEMENT le shader PBR (binding=0).
			// NB : NkCameraUBO (NkRendererTypes.h) a une layout differente (avec invView/
			// invProj separes) ; on ne peut pas l'utiliser directement.
			struct PBRCamUBO {
					NkMat4f view;
					NkMat4f proj;
					NkMat4f viewProj;
					NkMat4f invViewProj;
					NkVec4f camPos; // .xyz = pos, .w = near
					NkVec4f camDir; // .xyz = forward, .w = far
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
					float32 iblStrength, yFlipNDC, viewMode, matcapId; // viewMode:0=PBR,>0.5=SOLID ; matcapId=preset
					// Phase Planar Reflection : viewProj de la cam miroir, exposée
					// au shader ReflFloor pour calculer reflectionUV via
					// projection explicite. Les shaders qui n'utilisent pas ce
					// champ ignorent simplement les bytes après leur fin de struct.
					NkMat4f mirrorViewProj; // offset 320 (apres 16 bytes de padding)
					// Phase M.2-ish : flag indiquant si la frame courante est rendue
					// dans une passe miroir (NkPlanarReflectionSystem). Les shaders
					// doivent skip le shadow sampling pendant ces passes car
					// vWorldPos est en espace mirror (Y inverse), ce qui fait
					// sample le shadow map a la mauvaise position -> reflets faux.
					// .x = isMirrorPass (0=normal, 1=mirror), .yzw = reserve.
					NkVec4f reflectionFlags; // offset 384
					// Options du viewport (offset 400). .x = 1 si une matcap PERSONNALISEE
					// (texture unique chargee par l'utilisateur) a remplace l'atlas des 30 :
					// le shader doit alors echantillonner la texture ENTIERE au lieu d'une
					// tuile. .yzw reserves. Ajoute EN FIN de struct : les shaders qui
					// declarent un CameraUBO plus court ignorent simplement ces octets.
					NkVec4f viewOpts;
					// COULEUR DE L'AMBIANCE (offset 416), .xyz ; .w reserve. C'est le
					// pendant du « World > Color » de Blender : l'intensite dit
					// COMBIEN la scene recoit de son environnement, la couleur dit
					// DE QUELLE TEINTE. Sans elle l'ambiance est forcement neutre,
					// alors qu'un ciel bleute ou chaud teinte les faces a l'ombre --
					// c'est une part importante de l'aspect d'un rendu.
					// Ajoutee EN FIN de struct : les shaders qui declarent un
					// CameraUBO plus court ignorent simplement ces octets.
					NkVec4f iblColor;
					// BROUILLARD (offsets 432 et 448). Ces reglages vivaient dans
					// le contexte de scene depuis toujours mais AUCUNE passe ne les
					// lisait : les regler n'avait aucun effet. Ils descendent
					// desormais jusqu'au shader, qui les applique en dernier.
					NkVec4f fogColor;  // .xyz couleur, .w densite (loi exponentielle)
					NkVec4f fogParams; // .x actif, .y debut, .z fin, .w 0=lineaire 1=exp
			};

			PBRCamUBO cb{};
			cb.view = mCtx.camera.GetView();
			cb.proj = mCtx.camera.GetProj();
			cb.viewProj = mCtx.camera.GetViewProj();

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
				_depthApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX11 ||
				_depthApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX12 ||
				_depthApi == ::nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL) {
				NkMat4f clipZ01 = NkMat4f::Identity();
				clipZ01[2][2] = 0.5f;
				clipZ01[3][2] = 0.5f;
				cb.proj = clipZ01 * cb.proj;
				cb.viewProj = clipZ01 * cb.viewProj;
			}

			// ── TAA : jitter SUB-PIXEL de la projection ─────────────────────────
			// Suite de Halton (bases 2 et 3), 8 phases : repartition
			// low-discrepancy des positions d'echantillonnage a l'interieur du
			// pixel, bien plus uniforme qu'un tirage aleatoire sur si peu de
			// frames. Le decalage vaut au maximum un demi-pixel, donc invisible
			// frame par frame ; c'est l'accumulation du TAA qui le transforme en
			// super-echantillonnage. APRES la correction clip-Z pour que le jitter
			// s'exprime bien en NDC de la projection finale.
			if (mTAAJitter && mW > 0 && mH > 0) {
				const uint32 n = (mTAAJitterIdx % 8u) + 1u;
				float32 hx = 0.f;
				float32 hy = 0.f;
				for (uint32 i = n, f = 0; i != 0u; i >>= 1, ++f) {
					float32 w = 1.f;
					for (uint32 k = 0; k <= f; ++k)
						w *= 0.5f;
					hx += w * (float32)(i & 1u);
				}
				for (uint32 i = n, f = 0; i != 0u; i /= 3u, ++f) {
					float32 w = 1.f;
					for (uint32 k = 0; k <= f; ++k)
						w /= 3.f;
					hy += w * (float32)(i % 3u);
				}
				// Halton donne [0,1[ -> recentrer sur [-0.5, +0.5] pixel, puis
				// convertir en NDC : un pixel vaut 2/resolution en NDC.
				const float32 jx = (hx - 0.5f) * 2.f / (float32)mW;
				const float32 jy = (hy - 0.5f) * 2.f / (float32)mH;
				NkMat4f jit = NkMat4f::Identity();
				jit[3][0] = jx; // column-major : [3][*] = colonne de translation
				jit[3][1] = jy;
				cb.proj = jit * cb.proj;
				cb.viewProj = jit * cb.viewProj;
			}

			cb.invViewProj = cb.viewProj.Inverse();
			// Memorise les matrices REELLEMENT utilisees (jitter + clip-Z compris) :
			// le TAA reprojette avec elles, et la reconstruction de position monde
			// du deferred lit une profondeur produite par cette meme projection.
			mRenderViewProj = cb.viewProj;
			mRenderInvViewProj = cb.invViewProj;
			NkVec3f pos = mCtx.camera.GetPosition();
			NkVec3f fwd = mCtx.camera.GetForward();
			cb.camPos = {pos.x, pos.y, pos.z, mCtx.camera.GetNear()};
			cb.camDir = {fwd.x, fwd.y, fwd.z, mCtx.camera.GetFar()};
			cb.viewportX = (float32)mW;
			cb.viewportY = (float32)mH;
			cb.time = mCtx.time;
			cb.deltaTime = mCtx.deltaTime;
			cb.iblStrength = mIBLStrength;
			// .w = UN ENVIRONNEMENT EST-IL CHARGE ? Sans lui, le shader
			// echantillonnait quand meme les cubemaps d'irradiance et de reflet
			// PAR LA NORMALE : leur contenu par defaut n'etant pas uniforme, trois
			// faces d'un cube ressortaient plus claires que les trois autres, et
			// toujours les memes quelle que soit la vue -- ce que Rihen a
			// constate, alors que Blender donne un aplat parfait sans monde
			// image. A zero, le shader prend une ambiance UNIFORME.
			// Le moteur genere TOUJOURS un ciel procedural de repli : tester la
			// seule validite de la cubemap repondait donc « oui » en permanence,
			// et l'ambiance restait directionnelle. C'est un CHOIX explicite.
			const bool hasEnv = mIBLUseEnv && mEnv && mEnv->GetIrradianceCubemap().IsValid();
			cb.iblColor = {mIBLColor.x, mIBLColor.y, mIBLColor.z, hasEnv ? 1.f : 0.f};
			cb.fogColor = {mCtx.fogColor.x, mCtx.fogColor.y, mCtx.fogColor.z, mCtx.fogDensity};
			cb.fogParams = {mCtx.fogEnabled ? 1.f : 0.f, mCtx.fogStart, mCtx.fogEnd,
							mCtx.fogDensity > 0.f ? 1.f : 0.f};
			cb.viewMode = (float32)mViewMode; // 0=rendered(PBR) 1=solid/unlit (indépendant du wireframe)
			cb.matcapId = (float32)mMatcapId; // index 0..29 dans l'atlas des 30 matcaps
			// .x = 1 si une matcap PERSONNALISEE remplace l'atlas (cf. SetMatcapTexture) :
			// le shader echantillonne alors la texture entiere au lieu d'une tuile.
			cb.viewOpts = {mMatcapCustom ? 1.f : 0.f, 0.f, 0.f, 0.f};
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
					NkVec4f pos[32], color[32], dir[32], angles[32];
					int32 count, _p[3];
			} lb{};

			lb.count = (int32)mCtx.lights.Size();
			for (int32 i = 0; i < lb.count && i < 32; i++) {
				auto &l = mCtx.lights[i];
				lb.pos[i] = {l.position.x, l.position.y, l.position.z, (float32)l.type};
				lb.color[i] = {l.color.x, l.color.y, l.color.z, l.intensity};
				lb.dir[i] = {l.direction.x, l.direction.y, l.direction.z, l.range};
				// angles : .x = cos(inner), .y = cos(outer) (precompute pour le shader),
				// .z = castShadow flag, .w = cookieIdx (-1 = pas de cookie).
				const float deg2rad = 3.14159265f / 180.f;
				lb.angles[i] = {std::cos(l.innerAngle * deg2rad), std::cos(l.outerAngle * deg2rad),
								(float32)l.castShadow, (float32)l.cookieIdx};
			}
			if (mFrameSlot < mUBOLightsRing.Size())
				mDevice->WriteBuffer(mUBOLightsRing[mFrameSlot], &lb, sizeof(lb));
		}

		void NkRender3D::RenderShadowPass(NkICommandBuffer *cmd, const NkMat4f &lightVP) {
			if (!cmd || !mShadowPipeline.IsValid())
				return;

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
			const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size()) && (mFrameSlot < mObjectSetPool.Size());
			if (!poolFrameValid)
				return;

			// Itere sur mShadowCasters (collecte SANS culling camera) et non
			// mOpaque : sinon les casters hors champ camera n'auraient pas
			// d'ombre (cf. Submit). Tous les elements ici ont castShadow=true.
			// NkVSM v2 : les casters dont le material a mCastShadowAlphaTest
			// passent par mShadowAlphaPipeline (sample albedo + discard) — on
			// suit le pipeline courant et on ne re-binde qu'au changement
			// (re-push du PC obligatoire apres switch : DX12 invalide les
			// root parameters au changement de PSO).
			bool usingAlpha = false;
			for (auto &sdc : mShadowCasters) {
				auto &dc = sdc.dc;
				if (mObjectDrawIdx >= mObjectPoolCap) {
					logger.Errorf("[NkRender3D] ObjectUBO pool overflow (shadow): "
								  "drawIdx=%u >= max=%u, skipping draw\n",
								  mObjectDrawIdx, mObjectPoolCap);
					break;
				}
				NkMaterialInstance *matInst = nullptr;
				if (mMat && dc.material.IsValid())
					matInst = mMat->GetInstance(dc.material);
				const bool wantAlpha =
					mShadowAlphaPipeline.IsValid() && matInst && matInst->mCastShadowAlphaTest;
				if (wantAlpha != usingAlpha) {
					usingAlpha = wantAlpha;
					cmd->BindGraphicsPipeline(usingAlpha ? mShadowAlphaPipeline : mShadowPipeline);
					cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f), &lightVP);
				}
				if (usingAlpha && matInst->GetDescSet().IsValid())
					cmd->BindDescriptorSet(matInst->GetDescSet(), 2);
				ObjBlock ob{};
				ob.model = dc.transform;
				ob.normalMatrix = dc.transform.Inverse().Transpose();
				ob.tint = {1, 1, 1, 1};
				ob.metallic = 0.f;
				ob.roughness = 0.5f;
				ob.aoStrength = 1.f;

				NkBufferHandle ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
				NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
				if (ubo.IsValid())
					mDevice->WriteBuffer(ubo, &ob, sizeof(ob), 0);
				if (os.IsValid())
					cmd->BindDescriptorSet(os, 1);
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
			if (mShadowInstancePipeline.IsValid() && !mInstanced.Empty() && mFrameSlot < mUBOShadowInstPool.Size()) {
				cmd->BindGraphicsPipeline(mShadowInstancePipeline);
				cmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f), &lightVP);

				static NkMat4f sShModels[kMaxInstancesUBO];
				for (auto &dc : mInstanced) {
					const uint32 total = (uint32)dc.transforms.Size();
					if (total == 0)
						continue;
					for (uint32 b = 0; b < total; b += kMaxInstancesUBO) {
						if (mObjectDrawIdx >= mObjectPoolCap)
							break;
						if (mShadowInstIdx >= kShadowInstPoolCap)
							break;
						const uint32 n = (total - b < kMaxInstancesUBO) ? (total - b) : kMaxInstancesUBO;
						for (uint32 i = 0; i < n; i++)
							sShModels[i] = dc.transforms[b + i];

						// Buffer d'instances dédié (models seulement ; le VS shadow ne lit
						// pas les tints, mais le layout InstanceUBO les prévoit → offset OK).
						NkBufferHandle ib = mUBOShadowInstPool[mFrameSlot][mShadowInstIdx];
						if (ib.IsValid())
							mDevice->WriteBuffer(ib, sShModels, n * sizeof(NkMat4f), 0);

						// Set objet : binding1 = identité (le VS fait uObj.model*inst[id]),
						// binding4 = buffer d'instances. 1 slot du pool object/batch.
						NkBufferHandle objUbo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
						if (objUbo.IsValid()) {
							ObjBlock ob{};
							ob.model = NkMat4f::Identity();
							ob.normalMatrix = NkMat4f::Identity();
							ob.tint = {1, 1, 1, 1};
							ob.metallic = 0.f;
							ob.roughness = 0.5f;
							ob.aoStrength = 1.f;
							mDevice->WriteBuffer(objUbo, &ob, sizeof(ob), 0);
						}
						NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
						if (os.IsValid() && ib.IsValid())
							mDevice->BindUniformBuffer(os, 4, ib);
						if (os.IsValid())
							cmd->BindDescriptorSet(os, 1);
						mMesh->BindMesh(cmd, dc.mesh);
						mMesh->DrawAll(cmd, dc.mesh, n); // 1 DRAW, n instances
						mObjectDrawIdx++;
						mShadowInstIdx++;
					}
				}
			}
		}

		void NkRender3D::FlushOpaque(NkICommandBuffer *cmd) {
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
			const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size()) && (mFrameSlot < mObjectSetPool.Size());
			if (!poolFrameValid)
				return;

			// Multi-material : on change de pipeline uniquement si le draw courant
			// utilise un materiau different du precedent. Le pipeline PBR est le
			// fallback (deja lie dans Flush() avant FlushOpaque).
			NkPipelineHandle lastPipeline = mPBRPipeline;

			for (auto &sdc : mOpaque) {
				auto &dc = sdc.dc;
				if (mObjectDrawIdx >= mObjectPoolCap) {
					logger.Errorf("[NkRender3D] ObjectUBO pool overflow (opaque): "
								  "drawIdx=%u >= max=%u, skipping draw\n",
								  mObjectDrawIdx, mObjectPoolCap);
					break;
				}

				// Clip plane filter (planar reflection : ne capture que les
				// objets cote actif du plan, exclut le sol et le mauvais cote).
				if (mPendingMirrorActive) {
					const NkVec3f n = {mPendingClipPlane.x, mPendingClipPlane.y, mPendingClipPlane.z};
					if (n.x * n.x + n.y * n.y + n.z * n.z > 1e-6f) {
						const NkVec3f c = (dc.aabb.min + dc.aabb.max) * 0.5f;
						const float32 side = n.x * c.x + n.y * c.y + n.z * c.z + mPendingClipPlane.w;
						if (side <= 0.f)
							continue;
					}
				}

				// Determine pipeline + instance materiau.
				NkMaterialInstance *matInst = nullptr;
				NkPipelineHandle pipeline = mPBRPipeline; // fallback PBR

				if (dc.material.IsValid() && mMat) {
					matInst = mMat->GetInstance(dc.material);
					if (matInst) {
						NkPipelineHandle matPipeline = mMat->GetPipeline(matInst->GetTemplate());
						if (matPipeline.IsValid())
							pipeline = matPipeline;
					}
				}

				// Fallback : drawcall sans material custom (Demo3D, raw draws, ...).
				// Le shader PBR canonical lit tAlbedo dans set=2 binding=3 (convention
				// NkMaterialSystem) ; sans bind set=2 le sample est undefined behavior.
				// On instancie lazy une instance Default_PBR avec textures white1x1
				// et la bind systematiquement pour les drawcalls sans material.
				if (!matInst && mMat) {
					if (!mFallbackMatInst.IsValid()) {
						auto *inst = mMat->CreateInstance(mMat->DefaultPBR());
						if (inst)
							mFallbackMatInst = inst->GetHandle();
					}
					matInst = mMat->GetInstance(mFallbackMatInst);
				}

				// Bind pipeline seulement si change (evite le cout vkCmdBindPipeline redondant).
				if (pipeline != lastPipeline) {
					if (pipeline.IsValid())
						cmd->BindGraphicsPipeline(pipeline);
					lastPipeline = pipeline;
				}

				ObjBlock ob{};
				// Pre-multiplie par la mirror matrix si actif (passe RT planar
				// reflection via FlushIntoRT). Identite hors mirror.
				const NkMat4f xform = mPendingMirrorActive ? (mPendingMirror * dc.transform) : dc.transform;
				ob.model = xform;
				ob.normalMatrix = xform.Inverse().Transpose();
				ob.tint = {dc.tint.x, dc.tint.y, dc.tint.z, dc.alpha};
				ob.metallic = dc.metallic;
				ob.roughness = dc.roughness;
				ob.aoStrength = dc.aoStrength;
				ob.emissiveStrength = 0.f;
				ob.normalStrength = 1.f;
				// clearcoat / subsurface : 0 par defaut (zero-init via ObjBlock{}).

				// NkVSM v1 : copie les shadow overrides depuis le material.
				//   .x = receiveShadow (default 1.0 = receive)
				//   .y = castShadowAlphaTest (V1 reserve)
				//   .z = shadowBiasMul (default 1.0)
				//   .w = reserve
				if (matInst) {
					ob.shadowOverrides =
						NkVec4f{matInst->mReceiveShadow ? 1.f : 0.f, matInst->mCastShadowAlphaTest ? 1.f : 0.f,
								matInst->mShadowBiasMul, 0.f};
				} else {
					ob.shadowOverrides = NkVec4f{1.f, 0.f, 1.f, 0.f};
				}

				// 2026-05-24 Triplanar : copie tileSize material + metersPerUnit
				// global. Le shader fait UV = worldPos / (tileSize/metersPerUnit)
				// pour avoir des tiles VRAIMENT carres en metres reels meme
				// si l'echelle globale n'est pas 1m/unit.
				{
					const float32 tile = matInst ? matInst->mTriplanarTileSize : 0.f;
					const float32 mpu = NkUnits().metersPerUnit;
					ob.triplanarParams = NkVec4f{tile, mpu > 0.f ? mpu : 1.f, tile > 0.f ? 1.f : 0.f, 0.f};
				}

				NkBufferHandle ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
				NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
				if (ubo.IsValid())
					mDevice->WriteBuffer(ubo, &ob, sizeof(ob), 0);
				if (os.IsValid())
					cmd->BindDescriptorSet(os, 1);

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
						NkMaterialInstance *sInst = matInst; // fallback global
						if (si < dc.materialSlots.Size() && dc.materialSlots[si].IsValid()) {
							auto *cand = mMat->GetInstance(dc.materialSlots[si]);
							if (cand)
								sInst = cand;
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
					if (matInst)
						mMat->BindInstance(cmd, matInst);
					mMesh->BindMesh(cmd, dc.mesh);
					if (dc.subMeshIdx == 0xFFFFFFFFu)
						mMesh->DrawAll(cmd, dc.mesh);
					else
						mMesh->DrawSubMesh(cmd, dc.mesh, dc.subMeshIdx);
				}
				mObjectDrawIdx++;
			}
		}

		void NkRender3D::FlushTransparent(NkICommandBuffer *cmd) {
			for (auto &sdc : mTransparent) {
				mMesh->BindMesh(cmd, sdc.dc.mesh);
				mMesh->DrawAll(cmd, sdc.dc.mesh);
			}
		}

		void NkRender3D::FlushInstanced(NkICommandBuffer *cmd) {
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
				ob.metallic = 0.f;
				ob.roughness = 0.5f;
				ob.aoStrength = 1.f;
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
			if (mInstancePipeline.IsValid() && mFrameSlot < mUBOInstanceRing.Size() &&
				mFrameSlot < mUBOObjectPool.Size() && mFrameSlot < mObjectSetPool.Size()) {
				cmd->BindGraphicsPipeline(mInstancePipeline);
				NkBufferHandle instBuf = mUBOInstanceRing[mFrameSlot];
				NkDescSetHandle gs =
					(mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{};

				static NkMat4f sModels[kMaxInstancesUBO];
				static NkVec4f sTints[kMaxInstancesUBO];

				for (auto &dc : mInstanced) {
					const uint32 total = (uint32)dc.transforms.Size();
					if (total == 0)
						continue;
					// Frustum cull par BATCH — mêmes règles que le chemin
					// fallback ci-dessous (pas en miroir, shadow non affectée).
					if (!mPendingMirrorActive) {
						mCullStats.instancedBatches++;
						if (!mCtx.camera.IsAABBVisible(dc.aabb)) {
							mCullStats.instancedCulled++;
							continue;
						}
					}

					NkMaterialInstance *matInst = nullptr;
					if (dc.material.IsValid() && mMat)
						matInst = mMat->GetInstance(dc.material);
					if (!matInst && mMat) {
						if (!mFallbackMatInst.IsValid()) {
							auto *in = mMat->CreateInstance(mMat->DefaultPBR());
							if (in)
								mFallbackMatInst = in->GetHandle();
						}
						matInst = mMat->GetInstance(mFallbackMatInst);
					}

					for (uint32 b = 0; b < total; b += kMaxInstancesUBO) {
						if (mObjectDrawIdx >= mObjectPoolCap)
							break;
						const uint32 n = (total - b < kMaxInstancesUBO) ? (total - b) : kMaxInstancesUBO;
						for (uint32 i = 0; i < n; i++) {
							sModels[i] = dc.transforms[b + i];
							const NkVec3f t = (b + i < (uint32)dc.tints.Size()) ? dc.tints[b + i] : NkVec3f{1, 1, 1};
							sTints[i] = NkVec4f{t.x, t.y, t.z, 1.f};
						}
						if (instBuf.IsValid()) {
							mDevice->WriteBuffer(instBuf, sModels, n * sizeof(NkMat4f), 0);
							mDevice->WriteBuffer(instBuf, sTints, n * sizeof(NkVec4f),
												 kMaxInstancesUBO * sizeof(NkMat4f));
						}

						// ObjectUBO identité au binding 1 (le pool pré-câble binding 1
						// -> ce buffer). Sans données valides ici, uObj.model serait nul
						// -> cubes collapse. Mirror du skin (Object + bones tous deux liés).
						NkBufferHandle objUbo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
						if (objUbo.IsValid()) {
							ObjBlock ob = MakeIdentityObj();
							mDevice->WriteBuffer(objUbo, &ob, sizeof(ob), 0);
						}

						NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
						// Buffer d'instances bindé au set objet, binding 2 (comme les bones).
						if (os.IsValid() && instBuf.IsValid())
							mDevice->BindUniformBuffer(os, 4, instBuf); // binding 4 (anti-collision GL/DX)
						if (gs.IsValid())
							cmd->BindDescriptorSet(gs, 0); // caméra/lumières
						if (os.IsValid())
							cmd->BindDescriptorSet(os, 1);
						if (matInst)
							mMat->BindInstance(cmd, matInst); // textures (set 2)
						mMesh->BindMesh(cmd, dc.mesh);
						mMesh->DrawAll(cmd, dc.mesh, n); // 1 DRAW, n instances
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
			const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size()) && (mFrameSlot < mObjectSetPool.Size());
			if (!poolFrameValid)
				return;

			NkPipelineHandle lastPipeline = mPBRPipeline;

			for (auto &dc : mInstanced) {
				const uint32 n = (uint32)dc.transforms.Size();
				if (n == 0)
					continue;
				// Frustum cull par BATCH (AABB monde fusionné des instances).
				// Pas en passe miroir (caméra différente) ; la passe shadow
				// itère mInstanced directement et n'est pas affectée.
				if (!mPendingMirrorActive) {
					mCullStats.instancedBatches++;
					if (!mCtx.camera.IsAABBVisible(dc.aabb)) {
						mCullStats.instancedCulled++;
						continue;
					}
				}

				// Materiau + pipeline : resolus UNE fois pour tout le drawcall.
				NkMaterialInstance *matInst = nullptr;
				NkPipelineHandle pipeline = mPBRPipeline;
				if (dc.material.IsValid() && mMat) {
					matInst = mMat->GetInstance(dc.material);
					if (matInst) {
						NkPipelineHandle p = mMat->GetPipeline(matInst->GetTemplate());
						if (p.IsValid())
							pipeline = p;
					}
				}
				if (!matInst && mMat) {
					if (!mFallbackMatInst.IsValid()) {
						auto *inst = mMat->CreateInstance(mMat->DefaultPBR());
						if (inst)
							mFallbackMatInst = inst->GetHandle();
					}
					matInst = mMat->GetInstance(mFallbackMatInst);
				}
				if (pipeline != lastPipeline) {
					if (pipeline.IsValid())
						cmd->BindGraphicsPipeline(pipeline);
					lastPipeline = pipeline;
				}
				if (matInst)
					mMat->BindInstance(cmd, matInst);
				mMesh->BindMesh(cmd, dc.mesh);

				// Une instance = un slot d'ObjectUBO + un draw (mesh deja lie).
				for (uint32 i = 0; i < n; ++i) {
					if (mObjectDrawIdx >= mObjectPoolCap) {
						logger.Errorf("[NkRender3D] ObjectUBO pool overflow (instanced): "
									  "drawIdx=%u >= max=%u\n",
									  mObjectDrawIdx, mObjectPoolCap);
						break;
					}
					const NkMat4f &m = dc.transforms[i];

					ObjBlock ob{};
					ob.model = m;
					ob.normalMatrix = m.Inverse().Transpose();
					const NkVec3f t = (i < (uint32)dc.tints.Size()) ? dc.tints[i] : NkVec3f{1, 1, 1};
					ob.tint = {t.x, t.y, t.z, 1.f};
					ob.metallic = 0.f;
					ob.roughness = 0.5f;
					ob.aoStrength = 1.f;
					ob.normalStrength = 1.f;
					ob.shadowOverrides = NkVec4f{1.f, 0.f, 1.f, 0.f};
					const float32 mpu = NkUnits().metersPerUnit;
					ob.triplanarParams = NkVec4f{0.f, mpu > 0.f ? mpu : 1.f, 0.f, 0.f};

					NkBufferHandle ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
					NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
					if (ubo.IsValid())
						mDevice->WriteBuffer(ubo, &ob, sizeof(ob), 0);
					if (os.IsValid())
						cmd->BindDescriptorSet(os, 1);
					mMesh->DrawAll(cmd, dc.mesh); // 1 instance, a SA position
					mObjectDrawIdx++;
				}
			}
		}

		void NkRender3D::FlushSkinned(NkICommandBuffer *cmd) {
			const bool poolFrameValid = (mFrameSlot < mUBOObjectPool.Size()) && (mFrameSlot < mObjectSetPool.Size());
			if (!poolFrameValid)
				return;
			if (mSkinned.Empty())
				return;

			// Le pipeline skin doit etre lie AVANT tout draw skinne (il a son
			// propre vertex layout NkVertexSkinned + lit l'UBO de bones).
			// Sans ce bind, FlushSkinned utilisait le dernier pipeline (PBR) ->
			// aucun skinning (bug d'origine). No-op si pipeline non cree.
			if (!mSkinPipeline.IsValid()) {
				logger.Warnf("[NkRender3D] FlushSkinned: pipeline skin invalide, "
							 "skip %u draws skinnes\n",
							 (uint32)mSkinned.Size());
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
				if (sdiag == -1) {
					const char *v = getenv("NK_SKIN_DIAG");
					sdiag = (v && v[0] && v[0] != '0') ? 1 : 0;
				}
				if (sdiag && sframe < 80 && !mSkinned.Empty()) {
					NkBufferHandle rb =
						(mFrameSlot < mUBOBonesRing.Size()) ? mUBOBonesRing[mFrameSlot] : NkBufferHandle{};
					// Upload AVANT readback pour lire la donnee de CETTE frame.
					if (rb.IsValid() && mSkinned[0].boneMatrices.Size() >= 2) {
						uint32 n = (uint32)mSkinned[0].boneMatrices.Size();
						if (n > kMaxBonesUBO)
							n = kMaxBonesUBO;
						mDevice->WriteBuffer(rb, mSkinned[0].boneMatrices.Data(), n * sizeof(NkMat4f));
						// bone[1] = joint anime (SimpleSkin). On lit sa translation
						// (m12,m13,m14) DEPUIS LE BUFFER GPU. Si elle CHANGE entre
						// frames -> la matrice animee atteint bien le GPU -> le VS
						// skin (qui lit ce meme buffer en SRV t0) deforme le mesh.
						NkMat4f m1{};
						bool ok = mDevice->ReadBuffer(rb, &m1, sizeof(NkMat4f), sizeof(NkMat4f));
						// Log seulement toutes les ~12 frames pour rester lisible.
						if ((sframe % 12) == 0)
							logger.Info("[SKIN_DIAG] frame={0} slot={1} pipeline_valid={2} draws={3} readbackGPU={4} "
										"bone1_translate=({5}, {6}, {7})\n",
										sframe, mFrameSlot, mSkinPipeline.IsValid() ? 1 : 0, (uint32)mSkinned.Size(),
										ok ? 1 : 0, m1.data[12], m1.data[13], m1.data[14]);
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
			NkDescSetHandle gs =
				mPendingMirrorActive
					? ((mFrameSlot < mGlobalSetMirrorRing.Size()) ? mGlobalSetMirrorRing[mFrameSlot]
																  : NkDescSetHandle{})
					: ((mFrameSlot < mGlobalSetRing.Size()) ? mGlobalSetRing[mFrameSlot] : NkDescSetHandle{});

			// Ring UBO bones du frame courant (BUG Vulkan flicker).
			NkBufferHandle bonesBuf =
				(mFrameSlot < mUBOBonesRing.Size()) ? mUBOBonesRing[mFrameSlot] : NkBufferHandle{};

			// Materiau pour set=2 (le frag skin sample tAlbedo/tNormal/tORM/
			// tEmissive). Fallback Default_PBR (textures white/normal 1x1) si le
			// drawcall n'a pas de materiau custom.
			NkMaterialInstance *fallback = nullptr;
			if (mMat) {
				if (!mFallbackMatInst.IsValid()) {
					auto *inst = mMat->CreateInstance(mMat->DefaultPBR());
					if (inst)
						mFallbackMatInst = inst->GetHandle();
				}
				fallback = mMat->GetInstance(mFallbackMatInst);
			}

			// UBO bones = mat4 bones[64] (std140). On clamp a 64 ; les squelettes
			// plus grands sont tronques (les indices > 63 sont clampes a 63 cote
			// shader). On ecrit toujours kMaxBonesUBO mat4 (le buffer fait cette
			// taille) avec identite au-dela de `count` pour eviter des matrices
			// stale d'un draw precedent qui collapseraient des vertices.
			const uint32 kMaxBones = kMaxBonesUBO; // taille de l'UBO alloue a Init
			NkMat4f bonesScratch[kMaxBonesUBO];
			for (auto &dc : mSkinned) {
				if (dc.boneMatrices.Empty())
					continue;
				if (mObjectDrawIdx >= mObjectPoolCap) {
					logger.Errorf("[NkRender3D] ObjectUBO pool overflow (skinned): "
								  "drawIdx={0} >= max={1}, skipping draw\n",
								  mObjectDrawIdx, mObjectPoolCap);
					break;
				}
				uint32 count = (uint32)dc.boneMatrices.Size();
				if (count > kMaxBones)
					count = kMaxBones;
				if (bonesBuf.IsValid()) {
					// Remplit le scratch : [0,count) = bones du draw, [count,64) =
					// identite (pad). Upload des 64 mat4 d'un coup (taille fixe UBO).
					for (uint32 b = 0; b < count; b++)
						bonesScratch[b] = dc.boneMatrices[b];
					for (uint32 b = count; b < kMaxBones; b++)
						bonesScratch[b] = NkMat4f::Identity();
					mDevice->WriteBuffer(bonesBuf, bonesScratch, kMaxBones * sizeof(NkMat4f));
				}

				// ObjectUBO du draw (set=1, binding=1) : ecrit AVANT BindInstance
				// (WriteBuffer = memcpy mapped, legal a tout moment ; aucun bind
				// d'etat). Le set=1 lui-meme est re-bind plus bas, apres le skin.
				struct ObjB {
						NkMat4f m, nm;
						NkVec4f tint;
						float32 p[8];
				} ob{};

				ob.m = dc.transform;
				ob.nm = dc.transform.Inverse().Transpose();
				ob.tint = {dc.tint.x, dc.tint.y, dc.tint.z, dc.alpha};
				NkBufferHandle ubo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
				NkDescSetHandle os = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
				if (ubo.IsValid())
					mDevice->WriteBuffer(ubo, &ob, sizeof(ob));

				// Materiau du draw (set=2). Custom si fourni, sinon fallback.
				NkMaterialInstance *matInst = fallback;
				if (dc.material.IsValid() && mMat) {
					auto *cand = mMat->GetInstance(dc.material);
					if (cand)
						matInst = cand;
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
					if (sslot == -1) {
						const char *v = getenv("NK_SKIN_DIAG");
						sslot = (v && v[0] && v[0] != '0') ? 1 : 0;
					}
					const NkMat4f dnm = dc.transform.Inverse().Transpose();
					for (uint32 si = 0; si < nSubs; ++si) {
						if (mObjectDrawIdx >= mObjectPoolCap)
							break;
						NkMaterialInstance *sInst = matInst; // fallback global
						if (si < dc.materialSlots.Size() && dc.materialSlots[si].IsValid()) {
							auto *cand = mMat->GetInstance(dc.materialSlots[si]);
							if (cand)
								sInst = cand;
						}
						// ObjectUBO PROPRE au sous-mesh : le frag skin n'a pas d'UBO
						// materiau (set=2 = textures uniquement), il lit l'albedo via
						// uObj.tint -> vColor. On met donc tint = baseColorFactor du
						// materiau du sous-mesh (x dc.tint) pour que CHAQUE sous-mesh
						// prenne sa couleur (BrainStem = 59 materiaux unis distincts).
						NkVec4f alb = {1.f, 1.f, 1.f, 1.f};

						struct ObjB2 {
								NkMat4f m, nm;
								NkVec4f tint;
								float32 p[8];
						} sob{};

						sob.m = dc.transform;
						sob.nm = dnm;
						// Params PBR du materiau (le frag skin lit metallic/roughness/
						// aoStrength/... depuis uObj). Sans ca p[8]=0 -> ao=0 ->
						// aucun ambient -> modele sombre + bords qui bloom (glow).
						if (sInst) {
							const NkPBRParams &pbr = sInst->GetPBR();
							alb = pbr.albedo;
							sob.p[0] = pbr.metallic;
							sob.p[1] = pbr.roughness;
							sob.p[2] = pbr.ao;
							sob.p[3] = pbr.emissiveStrength;
							sob.p[4] = pbr.normalStrength;
							sob.p[5] = pbr.clearcoat;
							sob.p[6] = pbr.clearcoatRough;
							sob.p[7] = pbr.subsurface;
						} else {
							sob.p[2] = 1.f;
							sob.p[1] = 0.5f; // ao=1, roughness=0.5 par defaut
						}
						sob.tint = {alb.x * dc.tint.x, alb.y * dc.tint.y, alb.z * dc.tint.z, dc.alpha * alb.w};
						NkBufferHandle subUbo = mUBOObjectPool[mFrameSlot][mObjectDrawIdx];
						NkDescSetHandle subOs = mObjectSetPool[mFrameSlot][mObjectDrawIdx];
						if (subUbo.IsValid())
							mDevice->WriteBuffer(subUbo, &sob, sizeof(sob));

						// Met a jour le descriptor du materiau du sous-mesh.
						if (sInst && mMat)
							mMat->BindInstance(cmd, sInst);

						// BindInstance a rebinde le pipeline PBR (mauvais layout)
						// -> on re-bind le pipeline skin + TOUS les sets a chaque
						// sous-mesh (correctif DX12 root-params).
						cmd->BindGraphicsPipeline(mSkinPipeline);
						if (gs.IsValid())
							cmd->BindDescriptorSet(gs, 0);
						if (sInst && sInst->GetDescSet().IsValid())
							cmd->BindDescriptorSet(sInst->GetDescSet(), 2);
						if (subOs.IsValid())
							cmd->BindDescriptorSet(subOs, 1);

						mMesh->BindMesh(cmd, dc.mesh);
						mMesh->DrawSubMesh(cmd, dc.mesh, si);
						mObjectDrawIdx++;
					}
					if (sslot) {
						static uint32 mmF = 0;
						if (mmF == 0 && nSubs > 0) {
							NkMaterialInstance *s0 = (dc.materialSlots.Size() > 0 && dc.materialSlots[0].IsValid())
														 ? mMat->GetInstance(dc.materialSlots[0])
														 : nullptr;
							NkVec4f a0 = s0 ? s0->GetPBR().albedo : NkVec4f{1, 1, 1, 1};
							logger.Info("[SKIN_MULTIMAT] {0} sous-meshes, sub0 albedo=({1},{2},{3})\n", nSubs, a0.x,
										a0.y, a0.z);
						}
						mmF++;
					}
					continue;
				}

				// BindInstance MET A JOUR le descriptor materiau (upload UBO +
				// textures si dirty) ET bind le pipeline PBR du materiau + le
				// set=2. On l'appelle pour l'effet de mise a jour ; le pipeline
				// PBR et les sets qu'il bind seront ECRASES juste apres.
				if (matInst && mMat)
					mMat->BindInstance(cmd, matInst);

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
				if (gs.IsValid())
					cmd->BindDescriptorSet(gs, 0);
				if (matInst && matInst->GetDescSet().IsValid())
					cmd->BindDescriptorSet(matInst->GetDescSet(), 2);
				if (os.IsValid())
					cmd->BindDescriptorSet(os, 1);

				mMesh->BindMesh(cmd, dc.mesh);
				mMesh->DrawAll(cmd, dc.mesh);
				mObjectDrawIdx++;
			}
		}

		bool NkRender3D::EnsureDebugLinePipeline(NkRenderPassHandle currentRP) {
			if (mLinePipeline.IsValid() && mLinePipelineRP == currentRP)
				return true;
			if (!mShaderLib)
				return false;
			if (!mLineShader.IsValid()) {
				auto prog = mShaderLib->LoadOrCompileVF("DebugLine", "", "");
				if (!prog.IsValid()) {
					logger.Errorf("[NkR3D::DebugLine] shader compile FAIL\n");
					return false;
				}
				mLineShader = mShaderLib->GetRHIHandle(prog);
				if (!mLineShader.IsValid()) {
					logger.Errorf("[NkR3D::DebugLine] RHI handle FAIL\n");
					return false;
				}
			}
			if (mLinePipeline.IsValid()) {
				mDevice->DestroyPipeline(mLinePipeline);
				mLinePipeline = {};
			}
			if (mLinePipelineNoDepth.IsValid()) {
				mDevice->DestroyPipeline(mLinePipelineNoDepth);
				mLinePipelineNoDepth = {};
			}

			NkGraphicsPipelineDesc pd;
			pd.shader = mLineShader;
			// Lignes debug depth-testées (cage d'édition, axes) : elles doivent COLLER à
			// la surface du modèle sans z-fighting ni flottement. Technique standard =
			// POLYGON OFFSET / depth-bias (cf. glPolygonOffset) : LESS_EQUAL (le coplanaire
			// passe) + lecture seule + biais négatif (tire vers la caméra) -> la cage
			// adhère pile au maillage, occluse par le reste de la scène.
			pd.depthStencil = NkDepthStencilDesc::Default();
			pd.depthStencil.depthCompareOp = NkCompareOp::NK_LESS_EQUAL;
			pd.depthStencil.depthWriteEnable = false;
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.rasterizer.depthBiasConst = -1.5f;
			pd.rasterizer.depthBiasSlope = -1.5f;
			pd.blend = NkBlendDesc::Opaque();
			pd.topology = NkPrimitiveTopology::NK_LINE_LIST;
			pd.debugName = "DebugLine";
			pd.renderPass = currentRP;
			pd.descriptorSetLayouts.PushBack(mGlobalLayout); // set 0 = CameraUBO
			// vertex : pos vec3 (off 0) + couleur vec4 (off 12), stride 28
			pd.vertexLayout.AddBinding(0, 28, false)
				.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
				.AddAttribute(1, 0, NkVertexFormat::NK_RGBA32_FLOAT, 12, "COLOR", 0);
			mLinePipeline = mDevice->CreateGraphicsPipeline(pd);
			mLinePipelineRP = currentRP;

			// Variante OVERLAY : mêmes réglages mais depth-test OFF -> lignes toujours
			// au-dessus de la scène (gizmos/marqueurs éditeur, façon Blender).
			pd.depthStencil = NkDepthStencilDesc::NoDepth();
			pd.debugName = "DebugLineOverlay";
			mLinePipelineNoDepth = mDevice->CreateGraphicsPipeline(pd);

			logger.Info("[NkRender3D] DebugLine pipeline create: shader_valid={0} pipeline_valid={1} overlay_valid={2} "
						"rp.id={3}\n",
						mLineShader.IsValid() ? 1 : 0, mLinePipeline.IsValid() ? 1 : 0,
						mLinePipelineNoDepth.IsValid() ? 1 : 0, currentRP.id);
			return mLinePipeline.IsValid();
		}

		// Pipelines pour les TRIANGLES debug pleins (surlignage de faces, etc.) :
		// même shader/layout que les lignes (pos vec3 + couleur vec4), mais topologie
		// TRIANGLE_LIST + blend ALPHA. Variante depth (occluse) + overlay (au-dessus).
		bool NkRender3D::EnsureDebugTriOverlayPipeline(NkRenderPassHandle currentRP) {
			if (mTriPipeline.IsValid() && mTriPipelineRP == currentRP)
				return true;
			if (!EnsureDebugLinePipeline(currentRP))
				return false; // garantit mLineShader
			if (mTriPipeline.IsValid()) {
				mDevice->DestroyPipeline(mTriPipeline);
				mTriPipeline = {};
			}
			if (mTriPipelineNoDepth.IsValid()) {
				mDevice->DestroyPipeline(mTriPipelineNoDepth);
				mTriPipelineNoDepth = {};
			}

			NkGraphicsPipelineDesc pd;
			pd.shader = mLineShader;
			// Surbrillance de face façon Blender : UN seul triangle coplanaire visible des
			// DEUX CÔTÉS. depthCompareOp=LESS_EQUAL (le coplanaire passe) + lecture seule
			// (pas d'occlusion mutuelle) + biais NÉGATIF (tire vers la caméra) -> gagne le
			// z-fight contre sa propre surface quel que soit le côté regardé, tout en
			// restant occlus par les AUTRES objets devant.
			pd.depthStencil = NkDepthStencilDesc::Default();
			pd.depthStencil.depthCompareOp = NkCompareOp::NK_LESS_EQUAL;
			pd.depthStencil.depthWriteEnable = false;
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.rasterizer.depthBiasConst = -1.5f;
			pd.rasterizer.depthBiasSlope = -1.5f;
			pd.blend = NkBlendDesc::Alpha();
			pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
			pd.debugName = "DebugTriFill";
			pd.renderPass = currentRP;
			pd.descriptorSetLayouts.PushBack(mGlobalLayout);
			pd.vertexLayout.AddBinding(0, 28, false)
				.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
				.AddAttribute(1, 0, NkVertexFormat::NK_RGBA32_FLOAT, 12, "COLOR", 0);
			mTriPipeline = mDevice->CreateGraphicsPipeline(pd);
			mTriPipelineRP = currentRP;
			pd.depthStencil = NkDepthStencilDesc::NoDepth();
			pd.debugName = "DebugTriFillOverlay";
			mTriPipelineNoDepth = mDevice->CreateGraphicsPipeline(pd);
			return mTriPipeline.IsValid();
		}

		// Pipeline point-sprite écran-constant (marqueurs de vertices façon Blender) :
		// shader EditPoint (billboard en espace écran), quads (TRIANGLE_LIST), stride 36
		// (pos3 + corner2 + rgba4). Même polygon-offset que le fill pour coller à la surface.
		bool NkRender3D::EnsureEditPointPipeline(NkRenderPassHandle currentRP) {
			if (mEditPointPipeline.IsValid() && mEditPointPipelineRP == currentRP)
				return true;
			if (!mShaderLib)
				return false;
			if (!mEditPointShader.IsValid()) {
				auto prog = mShaderLib->LoadOrCompileVF("EditPoint", "", "");
				if (!prog.IsValid()) {
					logger.Errorf("[NkR3D::EditPoint] shader compile FAIL\n");
					return false;
				}
				mEditPointShader = mShaderLib->GetRHIHandle(prog);
				if (!mEditPointShader.IsValid()) {
					logger.Errorf("[NkR3D::EditPoint] RHI handle FAIL\n");
					return false;
				}
			}
			if (mEditPointPipeline.IsValid()) {
				mDevice->DestroyPipeline(mEditPointPipeline);
				mEditPointPipeline = {};
			}
			if (mEditPointPipelineNoDepth.IsValid()) {
				mDevice->DestroyPipeline(mEditPointPipelineNoDepth);
				mEditPointPipelineNoDepth = {};
			}
			NkGraphicsPipelineDesc pd;
			pd.shader = mEditPointShader;
			pd.depthStencil = NkDepthStencilDesc::Default();
			pd.depthStencil.depthCompareOp = NkCompareOp::NK_LESS_EQUAL;
			pd.depthStencil.depthWriteEnable = false;
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.rasterizer.depthBiasConst = -2.0f;
			pd.rasterizer.depthBiasSlope = -2.0f;
			pd.blend = NkBlendDesc::Alpha();
			pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
			pd.debugName = "EditPoint";
			pd.renderPass = currentRP;
			pd.descriptorSetLayouts.PushBack(mGlobalLayout);
			pd.vertexLayout.AddBinding(0, 36, false)
				.AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0)
				.AddAttribute(1, 0, NkVertexFormat::NK_RG32_FLOAT, 12, "TEXCOORD", 0)
				.AddAttribute(2, 0, NkVertexFormat::NK_RGBA32_FLOAT, 20, "COLOR", 0);
			mEditPointPipeline = mDevice->CreateGraphicsPipeline(pd);
			mEditPointPipelineRP = currentRP;
			pd.depthStencil = NkDepthStencilDesc::NoDepth();
			pd.debugName = "EditPointOverlay";
			mEditPointPipelineNoDepth = mDevice->CreateGraphicsPipeline(pd);
			return mEditPointPipeline.IsValid();
		}

		void NkRender3D::FlushDebug(NkICommandBuffer *cmd, NkRenderPassHandle currentRP, NkDescSetHandle gs) {
			// DIAG (NK_WIRE_DRAWDIAG=1) : compte ce qui est REELLEMENT emis par frame.
			// Sert a comparer mode OBJET vs mode EDITION (surdessin des aretes).
			{
				static int32 diag = -1;
				if (diag == -1) {
					const char *dv = getenv("NK_WIRE_DRAWDIAG");
					diag = (dv && dv[0] && dv[0] != '0') ? 1 : 0;
				}
				if (diag) {
					static uint32 nD = 0;
					if (nD < 40u)
						logger.Info("[WireDraw] n={0} ngonWire={1} ngonVerts={2} (={3} aretes) editLineV={4} "
									"(={5} aretes) editTriV={6} editPointV={7} debugLines={8} debugTris={9}\n",
									(int32)nD, mNgonWire ? 1 : 0, (int32)mNgonWireN, (int32)(mNgonWireN / 2),
									(int32)mEditLineN, (int32)(mEditLineN / 2), (int32)mEditTriN,
									(int32)mEditPointN, (int32)mDebugLines.Size(), (int32)mDebugTris.Size());
					nD++;
				}
			}
			// ── Batch persistant d'aretes N-GON (mode wireframe sans diagonales) ──
			// Un seul draw, buffer garde d'une frame sur l'autre. Depth-teste : les
			// aretes s'occultent correctement contre la grille/le sol.
			if (mNgonWire && mNgonWireN && EnsureDebugLinePipeline(currentRP) && mLinePipeline.IsValid()) {
				// Buffer du slot de CETTE frame (remis a niveau ici, jamais pendant qu'une
				// frame precedente le lit) -> plus de scintillement du fil de fer.
				const NkBufferHandle wb = NgonWireBufferForFrame();
				if (wb.IsValid()) {
					cmd->BindGraphicsPipeline(mLinePipeline);
					if (gs.IsValid())
						cmd->BindDescriptorSet(gs, 0);
					cmd->BindVertexBuffer(0, wb, 0);
					cmd->Draw(mNgonWireN);
				}
			}
			// ── Edit overlay PERSISTANT (cage/faces/points) : rendu chaque frame depuis
			//    des buffers GPU gardés (aucune reconstruction CPU tant que rien ne
			//    change). Faces/points d'abord (fill), puis la cage par-dessus. ────────
			// Faces (fill translucide) — pipeline triangles.
			if (mEditTriN && EnsureDebugTriOverlayPipeline(currentRP)) {
				NkPipelineHandle tp = mEditOverlayNoDepth ? mTriPipelineNoDepth : mTriPipeline;
				const NkBufferHandle b =
					EditBufForFrame(mEditTriRing, mEditTriRingCap, mEditTriDirty, mEditTriCPU, mEditTriN, 7);
				if (tp.IsValid() && b.IsValid()) {
					cmd->BindGraphicsPipeline(tp);
					if (gs.IsValid())
						cmd->BindDescriptorSet(gs, 0);
					cmd->BindVertexBuffer(0, b, 0);
					cmd->Draw(mEditTriN);
				}
			}
			// Points (marqueurs de vertices) — pipeline POINT SPRITE écran-constant.
			if (mEditPointN && EnsureEditPointPipeline(currentRP)) {
				NkPipelineHandle pp = mEditOverlayNoDepth ? mEditPointPipelineNoDepth : mEditPointPipeline;
				const NkBufferHandle b =
					EditBufForFrame(mEditPointRing, mEditPointRingCap, mEditPointDirty, mEditPointCPU, mEditPointN, 9);
				if (pp.IsValid() && b.IsValid()) {
					cmd->BindGraphicsPipeline(pp);
					if (gs.IsValid())
						cmd->BindDescriptorSet(gs, 0);
					cmd->BindVertexBuffer(0, b, 0);
					cmd->Draw(mEditPointN);
				}
			}
			if (mEditLineN && EnsureDebugLinePipeline(currentRP)) {
				NkPipelineHandle lp = mEditOverlayNoDepth ? mLinePipelineNoDepth : mLinePipeline;
				const NkBufferHandle b =
					EditBufForFrame(mEditLineRing, mEditLineRingCap, mEditLineDirty, mEditLineCPU, mEditLineN, 7);
				if (lp.IsValid() && b.IsValid()) {
					cmd->BindGraphicsPipeline(lp);
					if (gs.IsValid())
						cmd->BindDescriptorSet(gs, 0);
					cmd->BindVertexBuffer(0, b, 0);
					cmd->Draw(mEditLineN);
				}
			}

			if (mDebugLines.Empty() && mDebugTris.Empty())
				return;

			// ── 0. TRIANGLES debug pleins (surlignage de faces) — AVANT les lignes,
			//        pour que la cage/les points passent par-dessus le fill. ────────
			if (!mDebugTris.Empty() && EnsureDebugTriOverlayPipeline(currentRP)) {
				struct LV {
						float x, y, z, r, g, b, a;
				};

				NkVector<LV> tv;
				tv.Reserve(mDebugTris.Size() * 3);
				auto emitT = [&](const DebugTri &t) {
					tv.PushBack({t.a.x, t.a.y, t.a.z, t.color.x, t.color.y, t.color.z, t.color.w});
					tv.PushBack({t.b.x, t.b.y, t.b.z, t.color.x, t.color.y, t.color.z, t.color.w});
					tv.PushBack({t.c.x, t.c.y, t.c.z, t.color.x, t.color.y, t.color.z, t.color.w});
				};
				for (uint32 i = 0; i < mDebugTris.Size(); ++i)
					if (!mDebugTris[i].overlay)
						emitT(mDebugTris[i]);
				const uint32 tNormal = (uint32)tv.Size();
				for (uint32 i = 0; i < mDebugTris.Size(); ++i)
					if (mDebugTris[i].overlay)
						emitT(mDebugTris[i]);
				const uint32 tcount = (uint32)tv.Size();
				const uint32 tOverlay = tcount - tNormal;
				if (tcount > 0) {
					// Ring par frame en vol (ce contenu est reecrit CHAQUE frame).
					const NkBufferHandle tb =
						DebugRingUpload(mTriVBORing, mTriVBORingCap, tv.Data(), tcount, (uint32)sizeof(LV));
					if (tb.IsValid() && tNormal > 0) {
						cmd->BindGraphicsPipeline(mTriPipeline);
						if (gs.IsValid())
							cmd->BindDescriptorSet(gs, 0);
						cmd->BindVertexBuffer(0, tb, 0);
						cmd->Draw(tNormal);
					}
					if (tb.IsValid() && tOverlay > 0 && mTriPipelineNoDepth.IsValid()) {
						cmd->BindGraphicsPipeline(mTriPipelineNoDepth);
						if (gs.IsValid())
							cmd->BindDescriptorSet(gs, 0);
						cmd->BindVertexBuffer(0, tb, (uint64)tNormal * sizeof(LV));
						cmd->Draw(tOverlay);
					}
				}
				// Purge O(n) (mêmes règles que les lignes).
				uint32 tkeep = 0;
				for (uint32 i = 0; i < (uint32)mDebugTris.Size(); ++i) {
					if (mDebugTris[i].life <= 0.f)
						continue;
					mDebugTris[i].life -= mCtx.deltaTime;
					if (mDebugTris[i].life <= 0.f)
						continue;
					if (tkeep != i)
						mDebugTris[tkeep] = mDebugTris[i];
					++tkeep;
				}
				mDebugTris.Resize(tkeep);
			}

			if (mDebugLines.Empty())
				return;

			// ── 1. RENDU des lignes courantes ────────────────────────────────────
			if (EnsureDebugLinePipeline(currentRP)) {
				// Vertices : 2 par ligne (a,b) avec la couleur. Stride 28.
				// On range les lignes NORMALES (depth ON) d'abord, puis les lignes
				// OVERLAY (depth OFF), dans le MÊME VBO -> deux Draw depuis un offset.
				struct LV {
						float x, y, z, r, g, b, a;
				};

				NkVector<LV> verts;
				verts.Reserve(mDebugLines.Size() * 2);
				auto emit = [&](const DebugLine &l) {
					verts.PushBack({l.a.x, l.a.y, l.a.z, l.color.x, l.color.y, l.color.z, l.color.w});
					verts.PushBack({l.b.x, l.b.y, l.b.z, l.color.x, l.color.y, l.color.z, l.color.w});
				};
				for (uint32 i = 0; i < mDebugLines.Size(); ++i)
					if (!mDebugLines[i].overlay)
						emit(mDebugLines[i]);
				const uint32 vNormal = (uint32)verts.Size();
				for (uint32 i = 0; i < mDebugLines.Size(); ++i)
					if (mDebugLines[i].overlay)
						emit(mDebugLines[i]);
				const uint32 vcount = (uint32)verts.Size();
				const uint32 vOverlay = vcount - vNormal;
				const uint64 bytes = (uint64)vcount * sizeof(LV);

				// Ring par frame en vol (ce contenu est reecrit CHAQUE frame) :
				// un buffer unique serait reecrit pendant que le GPU le lit encore.
				(void)bytes;
				const NkBufferHandle lb =
					DebugRingUpload(mLineVBORing, mLineVBORingCap, verts.Data(), vcount, (uint32)sizeof(LV));

				// Lot 1 : lignes normales (depth-test ON).
				if (lb.IsValid() && vNormal > 0) {
					cmd->BindGraphicsPipeline(mLinePipeline);
					if (gs.IsValid())
						cmd->BindDescriptorSet(gs, 0);
					cmd->BindVertexBuffer(0, lb, 0);
					cmd->Draw(vNormal);
				}
				// Lot 2 : lignes OVERLAY (depth-test OFF) -> toujours au-dessus.
				if (lb.IsValid() && vOverlay > 0 && mLinePipelineNoDepth.IsValid()) {
					cmd->BindGraphicsPipeline(mLinePipelineNoDepth);
					if (gs.IsValid())
						cmd->BindDescriptorSet(gs, 0);
					cmd->BindVertexBuffer(0, lb, (uint64)vNormal * sizeof(LV));
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
				if (mDebugLines[i].life <= 0.f)
					continue; // one-frame -> drop
				mDebugLines[i].life -= mCtx.deltaTime;
				if (mDebugLines[i].life <= 0.f)
					continue; // persistante expirée -> drop
				if (keep != i)
					mDebugLines[keep] = mDebugLines[i]; // survit -> compacte
				++keep;
			}
			mDebugLines.Resize(keep);
		}

		// ── Phase E.6 : Light cookies 3D ─────────────────────────────────────────
		void NkRender3D::SetLightCookie3D(uint32 slot, NkTextureHandle tex) {
			if (slot >= kMaxCookies3D || !mResources)
				return;
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
			if (slot >= kMaxCookiesCube3D || !mResources)
				return;
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
			mDebugLines.PushBack({a, b, color, life, overlay});
		}

		void NkRender3D::DrawDebugTriangle(NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f color, float32 life, bool overlay) {
			mDebugTris.PushBack({a, b, c, color, life, overlay});
		}

		// ── Edit overlay persistant ────────────────────────────────────────────────
		// La copie CPU est l'AUTORITE : on n'ecrit jamais dans un buffer GPU ici (une
		// frame en vol pourrait le lire). Tous les slots sont marques perimes ; chacun
		// sera remis a niveau par EditBufForFrame au moment ou il est dessine.
		void NkRender3D::UploadEditBuf(NkVector<float32> &cpu, NkVector<uint8> &dirty, const float *v, uint32 vcount,
									   uint32 floatsPerVertex) {
			if (vcount == 0) {
				cpu.Clear();
				for (uint32 s = 0; s < (uint32)dirty.Size(); s++)
					dirty[s] = 1;
				return;
			}
			cpu.Resize(vcount * floatsPerVertex);
			if (v)
				memcpy(cpu.Data(), v, (size_t)vcount * floatsPerVertex * sizeof(float));
			for (uint32 s = 0; s < (uint32)dirty.Size(); s++)
				dirty[s] = 1;
		}

		// Buffer du slot COURANT, remis a niveau depuis la copie CPU juste avant le draw.
		NkBufferHandle NkRender3D::EditBufForFrame(NkVector<NkBufferHandle> &ring, NkVector<uint32> &caps,
												   NkVector<uint8> &dirty, const NkVector<float32> &cpu, uint32 vcount,
												   uint32 floatsPerVertex) {
			const uint32 slots = (mFramesInFlight < 1u) ? 1u : mFramesInFlight;
			if ((uint32)ring.Size() != slots) {
				for (uint32 s = 0; s < (uint32)ring.Size(); s++)
					if (ring[s].IsValid())
						mDevice->DestroyBuffer(ring[s]);
				ring.Clear();
				caps.Clear();
				dirty.Clear();
				ring.Resize(slots);
				caps.Resize(slots);
				dirty.Resize(slots);
				for (uint32 s = 0; s < slots; s++) {
					ring[s] = NkBufferHandle{};
					caps[s] = 0;
					dirty[s] = 1;
				}
			}
			const uint32 slot = (mFrameSlot < slots) ? mFrameSlot : 0u;
			if (vcount == 0)
				return NkBufferHandle{};
			const uint32 strideBytes = floatsPerVertex * (uint32)sizeof(float);
			if (!ring[slot].IsValid() || caps[slot] < vcount) {
				if (ring[slot].IsValid())
					mDevice->DestroyBuffer(ring[slot]);
				caps[slot] = vcount + 256u;
				ring[slot] = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)caps[slot] * strideBytes));
				dirty[slot] = 1; // buffer neuf -> contenu complet a ecrire
			}
			if (dirty[slot] && (uint64)vcount * floatsPerVertex <= (uint64)cpu.Size()) {
				mDevice->WriteBuffer(ring[slot], cpu.Data(), (uint64)vcount * strideBytes, 0);
				dirty[slot] = 0;
			}
			return ring[slot];
		}

		void NkRender3D::SetEditOverlayLines(const float *v, uint32 n) {
			UploadEditBuf(mEditLineCPU, mEditLineDirty, v, n, 7);
			mEditLineN = n;
		}

		void NkRender3D::SetEditOverlayTris(const float *v, uint32 n) {
			UploadEditBuf(mEditTriCPU, mEditTriDirty, v, n, 7);
			mEditTriN = n;
		}

		// Points = sprites écran-constant : vertex = pos3 + corner2(px) + rgba4 = 9 float.
		void NkRender3D::SetEditOverlayPoints(const float *v, uint32 n) {
			UploadEditBuf(mEditPointCPU, mEditPointDirty, v, n, 9);
			mEditPointN = n;
		}

		// Ring des VBO debug « une frame » (lignes/triangles de gizmo) : le contenu est
		// entierement reconstruit chaque frame, il suffit donc d'ecrire le slot courant.
		NkBufferHandle NkRender3D::DebugRingUpload(NkVector<NkBufferHandle> &ring, NkVector<uint32> &caps,
												   const void *v, uint32 vcount, uint32 strideBytes) {
			const uint32 slots = (mFramesInFlight < 1u) ? 1u : mFramesInFlight;
			if ((uint32)ring.Size() != slots) {
				for (uint32 s = 0; s < (uint32)ring.Size(); s++)
					if (ring[s].IsValid())
						mDevice->DestroyBuffer(ring[s]);
				ring.Clear();
				caps.Clear();
				ring.Resize(slots);
				caps.Resize(slots);
				for (uint32 s = 0; s < slots; s++) {
					ring[s] = NkBufferHandle{};
					caps[s] = 0;
				}
			}
			const uint32 slot = (mFrameSlot < slots) ? mFrameSlot : 0u;
			if (vcount == 0 || !v)
				return NkBufferHandle{};
			if (!ring[slot].IsValid() || caps[slot] < vcount) {
				if (ring[slot].IsValid())
					mDevice->DestroyBuffer(ring[slot]);
				caps[slot] = vcount + 256u;
				ring[slot] = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic((uint64)caps[slot] * strideBytes));
			}
			mDevice->WriteBuffer(ring[slot], v, (uint64)vcount * strideBytes, 0);
			return ring[slot];
		}

		// ── Batch d'aretes n-gon — RING PAR FRAME EN VOL ──────────────────────────
		// Marque la plage [firstVertex, firstVertex+count) comme perimee POUR TOUS LES
		// SLOTS : chaque frame en vol a son propre buffer, chacun doit donc recevoir la
		// modification, mais a son propre rythme (au moment ou il est dessine).
		void NkRender3D::NgonWireMarkDirty(uint32 firstVertex, uint32 count) {
			if (count == 0)
				return;
			const uint32 lo = firstVertex, hi = firstVertex + count;
			for (uint32 s = 0; s < (uint32)mNgonWireDirtyLo.Size(); s++) {
				if (mNgonWireDirtyHi[s] <= mNgonWireDirtyLo[s]) { // slot propre -> nouvelle plage
					mNgonWireDirtyLo[s] = lo;
					mNgonWireDirtyHi[s] = hi;
				} else { // coalesce (une seule plage par slot : simple et suffisant ici)
					mNgonWireDirtyLo[s] = (lo < mNgonWireDirtyLo[s]) ? lo : mNgonWireDirtyLo[s];
					mNgonWireDirtyHi[s] = (hi > mNgonWireDirtyHi[s]) ? hi : mNgonWireDirtyHi[s];
				}
			}
		}

		// Buffer du slot COURANT, remis a niveau depuis la copie CPU juste avant le draw.
		// C'est le SEUL endroit qui ecrit dans un buffer du ring -> on n'ecrit jamais dans
		// celui qu'une frame encore en vol est en train de lire (cause du clignotement).
		NkBufferHandle NkRender3D::NgonWireBufferForFrame() {
			const uint32 slots = (mFramesInFlight < 1u) ? 1u : mFramesInFlight;
			if ((uint32)mNgonWireRing.Size() != slots) {
				for (uint32 s = 0; s < (uint32)mNgonWireRing.Size(); s++)
					if (mNgonWireRing[s].IsValid())
						mDevice->DestroyBuffer(mNgonWireRing[s]);
				mNgonWireRing.Clear();
				mNgonWireRingCap.Clear();
				mNgonWireDirtyLo.Clear();
				mNgonWireDirtyHi.Clear();
				mNgonWireRing.Resize(slots);
				mNgonWireRingCap.Resize(slots);
				mNgonWireDirtyLo.Resize(slots);
				mNgonWireDirtyHi.Resize(slots);
				for (uint32 s = 0; s < slots; s++) {
					mNgonWireRing[s] = NkBufferHandle{};
					mNgonWireRingCap[s] = 0;
					mNgonWireDirtyLo[s] = 0;
					mNgonWireDirtyHi[s] = mNgonWireN; // tout est a uploader
				}
			}
			const uint32 slot = (mFrameSlot < slots) ? mFrameSlot : 0u;
			if (mNgonWireN == 0)
				return mNgonWireRing[slot];
			if (!mNgonWireRing[slot].IsValid() || mNgonWireRingCap[slot] < mNgonWireN) {
				if (mNgonWireRing[slot].IsValid())
					mDevice->DestroyBuffer(mNgonWireRing[slot]);
				mNgonWireRingCap[slot] = mNgonWireN + 256u;
				mNgonWireRing[slot] = mDevice->CreateBuffer(
					NkBufferDesc::VertexDynamic((uint64)mNgonWireRingCap[slot] * 7 * sizeof(float)));
				mNgonWireDirtyLo[slot] = 0; // buffer neuf -> contenu COMPLET a ecrire
				mNgonWireDirtyHi[slot] = mNgonWireN;
			}
			uint32 lo = mNgonWireDirtyLo[slot], hi = mNgonWireDirtyHi[slot];
			if (hi > mNgonWireN)
				hi = mNgonWireN;
			if (hi > lo && (uint64)hi * 7 <= (uint64)mNgonWireCPU.Size())
				mDevice->WriteBuffer(mNgonWireRing[slot], mNgonWireCPU.Data() + (uint64)lo * 7,
									 (uint64)(hi - lo) * 7 * sizeof(float), (uint64)lo * 7 * sizeof(float));
			// DIAG (NK_WIRE_SLOTDIAG=1) : trace la suite des slots reellement utilises.
			// Sert a prouver que deux frames CONSECUTIVES ne retombent jamais sur le
			// meme slot (sinon = ecriture d'un buffer encore lu par le GPU).
			{
				static int32 diag = -1;
				if (diag == -1) {
					const char *dv = getenv("NK_WIRE_SLOTDIAG");
					diag = (dv && dv[0] && dv[0] != '0') ? 1 : 0;
				}
				if (diag) {
					static uint32 nDiag = 0;
					static uint32 prevSlot = 0xFFFFFFFFu;
					static uint32 collisions = 0;
					if (slot == prevSlot)
						collisions++;
					if (nDiag < 60u)
						logger.Info("[WireSlot] n={0} devFrameIdx={1} devMaxFIF={2} slots={3} slot={4} "
									"upload=[{5},{6}) verts={7} collisionsConsecutives={8}\n",
									(int32)nDiag, (int32)mDevice->GetFrameIndex(),
									(int32)mDevice->GetMaxFramesInFlight(), (int32)slots, (int32)slot, (int32)lo,
									(int32)hi, (int32)mNgonWireN, (int32)collisions);
					prevSlot = slot;
					nDiag++;
				}
			}
			mNgonWireDirtyLo[slot] = 0;
			mNgonWireDirtyHi[slot] = 0; // slot a jour
			return mNgonWireRing[slot];
		}

		void NkRender3D::SetNgonWireLines(const float *v, uint32 n) {
			mNgonWireCPU.Resize(n * 7u);
			if (n > 0 && v)
				memcpy(mNgonWireCPU.Data(), v, (size_t)n * 7 * sizeof(float));
			mNgonWireN = n;
			// Batch entierement reconstruit -> TOUT est perime dans CHAQUE slot.
			for (uint32 s = 0; s < (uint32)mNgonWireDirtyLo.Size(); s++) {
				mNgonWireDirtyLo[s] = 0;
				mNgonWireDirtyHi[s] = n;
			}
		}

		// Reecrit UNIQUEMENT la tranche [firstVertex, firstVertex+count) : les aretes
		// d'une primitive sont calculees une fois pour toutes, seule la transform d'un
		// objet qui bouge oblige a re-transformer SA tranche. On patche la copie CPU
		// (autorite) et on marque la plage perimee dans TOUS les slots du ring.
		void NkRender3D::UpdateNgonWireLines(const float *v, uint32 firstVertex, uint32 count) {
			if (!v || count == 0 || (uint64)(firstVertex + count) * 7 > (uint64)mNgonWireCPU.Size())
				return;
			memcpy(mNgonWireCPU.Data() + (uint64)firstVertex * 7, v, (size_t)count * 7 * sizeof(float));
			NgonWireMarkDirty(firstVertex, count);
		}

		void NkRender3D::ClearNgonWire() {
			mNgonWireN = 0;
		}

		void NkRender3D::SetEditOverlayXray(bool xray) {
			mEditOverlayNoDepth = xray;
		}

		void NkRender3D::ClearEditOverlay() {
			mEditLineN = mEditTriN = mEditPointN = 0;
		}

		void NkRender3D::DrawDebugSphere(NkVec3f c, float32 r, NkVec4f color) {
			const int N = 16;
			for (int i = 0; i < N; i++) {
				float32 a0 = 2 * 3.14159f * i / N, a1 = 2 * 3.14159f * (i + 1) / N;
				DrawDebugLine({c.x + cosf(a0) * r, c.y + sinf(a0) * r, c.z},
							  {c.x + cosf(a1) * r, c.y + sinf(a1) * r, c.z}, color);
			}
		}

		void NkRender3D::DrawDebugAABB(const NkAABB &box, NkVec4f color) {
			NkVec3f mn = box.min, mx = box.max;
			DrawDebugLine({mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, color);
			DrawDebugLine({mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, color);
			DrawDebugLine({mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z}, color);
			DrawDebugLine({mn.x, mx.y, mn.z}, {mn.x, mn.y, mn.z}, color);
			DrawDebugLine({mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, color);
			DrawDebugLine({mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, color);
			DrawDebugLine({mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}, color);
			DrawDebugLine({mn.x, mx.y, mx.z}, {mn.x, mn.y, mx.z}, color);
			DrawDebugLine({mn.x, mn.y, mn.z}, {mn.x, mn.y, mx.z}, color);
			DrawDebugLine({mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z}, color);
			DrawDebugLine({mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z}, color);
			DrawDebugLine({mn.x, mx.y, mn.z}, {mn.x, mx.y, mx.z}, color);
		}

		void NkRender3D::DrawDebugAxes(const NkMat4f &t, float32 s) {
			NkVec3f orig = {t[3][0], t[3][1], t[3][2]};
			DrawDebugLine(orig, {orig.x + t[0][0] * s, orig.y + t[0][1] * s, orig.z + t[0][2] * s}, {1, 0, 0, 1});
			DrawDebugLine(orig, {orig.x + t[1][0] * s, orig.y + t[1][1] * s, orig.z + t[1][2] * s}, {0, 1, 0, 1});
			DrawDebugLine(orig, {orig.x + t[2][0] * s, orig.y + t[2][1] * s, orig.z + t[2][2] * s}, {0, 0, 1, 1});
		}

		void NkRender3D::DrawDebugGrid(NkVec3f o, float32 sp, int32 lines, NkVec4f color) {
			float32 ext = sp * lines * 0.5f;
			for (int32 i = -lines / 2; i <= lines / 2; i++) {
				float32 f = (float32)i * sp;
				DrawDebugLine({o.x + f, o.y, o.z - ext}, {o.x + f, o.y, o.z + ext}, color);
				DrawDebugLine({o.x - ext, o.y, o.z + f}, {o.x + ext, o.y, o.z + f}, color);
			}
		}

		void NkRender3D::DrawDebugArrow(NkVec3f from, NkVec3f to, NkVec4f color) {
			DrawDebugLine(from, to, color);
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
			if (mDebugInited && mDebugPipelineRP == currentRP)
				return mDebugPipeline.IsValid();

			// 1. Compile shader debug (VK : Resources/.../DebugTri/VK/debugtri.*)
			//    LoadOrCompileVF retourne un handle Renderer-side (cache lookup),
			//    PAS le RHI shader handle attendu par CreateGraphicsPipeline.
			//    Il faut convertir via GetRHIHandle (cf. mPBRShader plus haut).
			if (!mDebugShader.IsValid()) {
				if (!mShaderLib)
					return false;
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
				struct V {
						float x, y, z;
				};

				V verts[3] = {
					{-0.9f, -0.9f, 0.f},
					{0.9f, -0.9f, 0.f},
					{0.0f, 0.9f, 0.f},
				};
				mDebugVBO = mDevice->CreateBuffer(NkBufferDesc::Vertex(sizeof(verts), verts));
			}

			// 3. IBO 3 indices = 0,1,2 (test indexed path)
			if (!mDebugIBO.IsValid()) {
				uint32 idx[3] = {0, 1, 2};
				mDebugIBO = mDevice->CreateBuffer(NkBufferDesc::Index(sizeof(idx), idx));
			}

			// 4. Pipeline. Aucun descriptor set, vertex layout 12-byte vec3.
			if (mDebugPipeline.IsValid()) {
				mDevice->DestroyPipeline(mDebugPipeline);
				mDebugPipeline = {};
			}
			NkGraphicsPipelineDesc pd;
			pd.shader = mDebugShader;
			// pd.depthStencil = NkDepthStencilDesc::Default();
			pd.depthStencil = NkDepthStencilDesc::NoDepth();
			pd.rasterizer = NkRasterizerDesc::NoCull();
			pd.blend = NkBlendDesc::Opaque();
			pd.debugName = "DebugTriangle";
			pd.renderPass = currentRP;
			pd.vertexLayout.AddBinding(0, sizeof(float) * 3, false)
				.AddAttribute(0, 0, ::nkentseu::NkVertexFormat::NK_RGB32_FLOAT, 0, "POSITION", 0);
			mDebugPipeline = mDevice->CreateGraphicsPipeline(pd);
			mDebugPipelineRP = currentRP;
			mDebugInited = true;
			logger.Info("[NkR3D::DebugTriangle] pipeline create: shader.valid={0} pipe.valid={1} rp.id={2}\n",
						mDebugShader.IsValid() ? 1 : 0, mDebugPipeline.IsValid() ? 1 : 0, currentRP.id);
			return mDebugPipeline.IsValid();
		}

		void NkRender3D::DebugDrawTriangleNoIdx(NkICommandBuffer *cmd) {
			if (!mDebugPipeline.IsValid() || !mDebugVBO.IsValid())
				return;
			cmd->BindGraphicsPipeline(mDebugPipeline);
			cmd->BindVertexBuffer(0, mDebugVBO, 0);
			cmd->Draw(3);
		}

		void NkRender3D::DebugDrawTriangleIdx(NkICommandBuffer *cmd) {
			if (!mDebugPipeline.IsValid() || !mDebugVBO.IsValid() || !mDebugIBO.IsValid())
				return;
			cmd->BindGraphicsPipeline(mDebugPipeline);
			cmd->BindVertexBuffer(0, mDebugVBO, 0);
			cmd->BindIndexBuffer(mDebugIBO, NkIndexFormat::NK_UINT32, 0);
			cmd->DrawIndexed(3, 1, 0, 0, 0);
		}

		// ────────────────────────────────────────────────────────────────────
		// DEBUG : dessin direct dans swapchain (bypass Geometry pass)
		// ────────────────────────────────────────────────────────────────────
		void NkRender3D::DebugDrawDirectSwapchain(NkICommandBuffer *cmd) {
			// EnsureDebugTriangle avec rp=invalid -> CreateGraphicsPipeline
			// fallback sur swapchain RP. Le pipeline est recree si on avait
			// deja un pipeline pour rp Geometry (1191).
			NkRenderPassHandle rpInvalid{};
			if (!EnsureDebugTriangle(rpInvalid))
				return;
			DebugDrawTriangleNoIdx(cmd);
		}

	} // namespace renderer
} // namespace nkentseu
