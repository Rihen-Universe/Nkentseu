#pragma once
// =============================================================================
// NkPostProcessStack.h  — NKRenderer v4.0  (Tools/PostProcess/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkRendererConfig.h"
#include "NKRenderer/Core/NkRenderTarget.h" // Phase H.2 : bloom mipchain via NkRenderTarget
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKTime/NkChrono.h" // auto-exposure : dt d'adaptation mesure en interne

namespace nkentseu {
	namespace renderer {
		class NkTextureLibrary;
		class NkMeshSystem;
		class NkShaderLibrary;
		class NkResources;

		class NkPostProcessStack {
			public:
				bool Init(NkIDevice *d, NkTextureLibrary *t, NkMeshSystem *m, NkShaderLibrary *sl, NkResources *res,
						  uint32 w, uint32 h);
				void Shutdown();
				void OnResize(uint32 w, uint32 h);

				void SetConfig(const NkPostConfig &c) {
					mCfg = c;
				}

				NkPostConfig &GetConfig() {
					return mCfg;
				}

				NkTexHandle Execute(NkICommandBuffer *cmd, NkTexHandle hdrIn, NkTexHandle depth,
									NkTexHandle velocity = NkTexHandle::Null());

				// Variante consommant directement des handles RHI (utilise par
				// NkRenderGraph qui stocke des NkTextureHandle dans ses transients).
				// Ne wrappe pas dans NkTextureLibrary — le bind est fait direct
				// via mDevice->BindTextureSampler dans DrawFullscreen.
				// Phase H.2 : bloomTex optionnel — si valide, sample au binding=1
				// du tonemap (= mip 0 du bloom upsample, vrai dual-Kawase).
				// Si invalide, le shader ignore le bloom (texture noire = 0).
				void ExecuteRHI(NkICommandBuffer *cmd, NkTextureHandle hdrIn,
								NkTextureHandle bloomTex = NkTextureHandle{},
								NkTextureHandle ssaoTex = NkTextureHandle{});

				// Phase H.2 : sub-passes bloom multi-pass. Le RenderGraph
				// (NkRendererImpl::BuildDefaultRenderGraph) appelle ces methodes
				// dans une pass deja ouverte (color attachment = mip cible).
				// Elles ne font QUE bind pipeline + descriptor + draw quad
				// (pas de BeginRenderPass — c'est le RG qui s'en charge).
				//
				// DrawBloomDown : extrait + downsample 2x (13-tap COD style).
				void DrawBloomDownPass(NkICommandBuffer *cmd, NkTextureHandle src, uint32 srcW, uint32 srcH,
									   float threshold);

				// DrawBloomUp : tent filter 3x3 + blend additif sur la mip courante.
				void DrawBloomUpPass(NkICommandBuffer *cmd, NkTextureHandle src, uint32 srcW, uint32 srcH,
									 float strength);

				// Phase H.3 : pass SSAO (depth-only). depthSrc = HDR depth
				// transient. ssaoW/ssaoH = dimensions du RT cible (typique W/2).
				void DrawSSAOPass(NkICommandBuffer *cmd, NkTextureHandle depthSrc, uint32 ssaoW, uint32 ssaoH);

				// Phase H.5b : blur cross-bilateral / gaussian apres GTAO pour
				// denoise le noise du random rotation per-pixel.
				void DrawSSAOBlurPass(NkICommandBuffer *cmd, NkTextureHandle aoSrc, uint32 ssaoW, uint32 ssaoH);

				// ── Phase L : AUTO-EXPOSURE V1 (2026-07-30) ────────────────────────
				// Mesure la luminance moyenne logarithmique de hdrIn (256 echantillons
				// ponderes vers le centre) dans une cible 1x1 persistante, avec
				// adaptation temporelle (ping-pong de deux cibles 1x1 : on lit celle de
				// la frame precedente, on ecrit l'autre). Le tonemap consomme ensuite
				// GetAvgLumaTexRHI() au binding 4.
				//
				// Cette methode gere SON PROPRE render pass (BeginRender/EndRender sur
				// une cible hors-graph, comme la passe d'ombres) : la passe RenderGraph
				// correspondante declare Reads(hdr) + SetAlwaysExecute(true) et ne
				// declare aucun attachement.
				//
				// dtSeconds <= 0 : le delta est mesure en interne (horloge propre).
				void RunAutoExposure(NkICommandBuffer *cmd, NkTextureHandle hdrIn, float32 dtSeconds = -1.f);

				// Handle RHI de la cible 1x1 contenant la luminance adaptee la plus
				// recente (celle ecrite par le dernier RunAutoExposure). Invalide si
				// l'auto-exposure n'a pas encore tourne.
				NkTextureHandle GetAvgLumaTexRHI() const;

				// Vrai si l'auto-exposure doit tourner cette frame (config ou override
				// d'environnement NK_AUTOEXP). Consulte par le RenderGraph.
				bool IsAutoExposureEnabled() const;

				// ── Phase L : TAA — Temporal Anti-Aliasing (2026-07-30) ────────────
				// Accumule les frames precedentes pour lisser les bords en escalier.
				// Fonctionne de pair avec le jitter sub-pixel de la projection
				// (NkRender3D::SetTAAJitterEnabled) : sans jitter, toutes les frames
				// echantillonnent au meme endroit et le TAA n'apporte RIEN.
				//
				// Opere sur le LDR tonemappe (en alternative au FXAA) : evite les
				// fireflies HDR et la perte de precision d'un historique HDR.
				// Historique en ping-pong de deux cibles plein ecran persistantes ;
				// la passe gere son propre render pass (cibles hors-graph, comme
				// l'auto-exposure et les ombres).
				//
				// reproj = prevViewProj * invViewProj(courante) : l'appelant la
				// construit depuis NkRender3D::GetRenderViewProj/InvViewProj (les
				// matrices REELLES du rendu, jitter et clip-Z compris) et conserve la
				// viewProj de la frame precedente.
				void RunTAA(NkICommandBuffer *cmd, NkTextureHandle ldrIn, NkTextureHandle depth,
							const NkMat4f &reproj, bool hasHistory);

				// Handle RHI du resultat TAA le plus recent (a blitter vers l'ecran).
				// Invalide tant que le TAA n'a pas tourne.
				NkTextureHandle GetTAAResultRHI() const;

				bool IsTAAEnabled() const;
				NkTexHandle RunSSAO(NkICommandBuffer *cmd, NkTexHandle depth, NkTexHandle normal);
				NkTexHandle RunBloom(NkICommandBuffer *cmd, NkTexHandle hdr);
				NkTexHandle RunTonemap(NkICommandBuffer *cmd, NkTexHandle hdr);
				NkTexHandle RunFXAA(NkICommandBuffer *cmd, NkTexHandle ldr);

				// Vrai si au moins un effet est actif dans la config — utilise par
				// BuildDefaultRenderGraph pour decider d'activer le path HDR transient.
				bool HasAnyEffect() const noexcept {
					return mCfg.ssao || mCfg.bloom || mCfg.toneMapping || mCfg.fxaa || mCfg.aces;
				}

				// Phase L FXAA wirage : intermediate LDR target pour split
				// tonemap (→mToneTex) + FXAA (→swapchain).
				NkTexHandle GetToneTexHandle() const {
					return mToneTex;
				}

				bool IsFXAAEnabled() const {
					return mCfg.fxaa;
				}

				// ExecuteFXAA : pass dediee FXAA, lit ldrIn (mToneTex), draw fullscreen
				// sur le RT courant (swapchain). Appele par RenderGraph FXAA pass.
				void ExecuteFXAA(NkICommandBuffer *cmd, NkTextureHandle ldrIn);

				// ExecuteBlit : recopie 1:1 de src vers le RT courant (fullscreen
				// triangle, shader Blit). Utilise par la passe MirrorPresent :
				// quand la cible finale est redirigee (capture/enregistrement),
				// cette passe garde la FENETRE vivante en recopiant la cible vers
				// le swapchain. Cout : 1 draw plein-ecran.
				void ExecuteBlit(NkICommandBuffer *cmd, NkTextureHandle src);

				// Phase L : upload une LUT 3D de color grading utilisateur.
				// rgba = size^3 voxels RGBA8, voxel (i,j,k) a l'index
				// ((k*size + j)*size + i)*4 (i=rouge, j=vert, k=bleu — meme
				// layout que la LUT identite). size dans [2..64]. L'effet
				// s'active via la config renderer : colorGrading=true +
				// lutStrength>0 (SetPostConfig). Operation de setup (WaitIdle
				// interne si recreation) — PAS un appel par-frame. Accessible
				// par renderer->GetPostProcess()->SetColorGradingLUT(...).
				bool SetColorGradingLUT(const uint8 *rgba, uint32 size);

			private:
				NkIDevice *mDevice = nullptr;
				NkTextureLibrary *mTex = nullptr;
				NkMeshSystem *mMesh = nullptr;
				NkShaderLibrary *mShaderLib = nullptr;
				NkResources *mResources = nullptr;
				NkPostConfig mCfg;
				uint32 mW = 0, mH = 0;
				NkTexHandle mSSAOTex, mToneTex, mFinalTex;

				// Phase L : Color grading 3D LUT. Default = identity (no
				// grading). User upload via SetColorGradingLUT(data, size).
				NkTextureHandle mLUTTex;
				uint32 mLUTSize = 16; // sync avec mCfg.lutSize

				// Phase H.2 : bloom mipchain via NkRenderTarget (6 niveaux,
				// W/2..W/64, RGBA16F sans depth). Chaque mip a son render pass
				// pour permettre BeginRender/EndRender pendant RunBloom.
				static constexpr int kBloomMips = 6;
				NkRenderTarget mBloomRT[kBloomMips];

				// Phase H.3 : RT R8_UNORM pour le SSAO (W/2 x H/2). Sert de
				// template de render pass pour mPipeSSAO (compatible avec le
				// transient ssaoTex du RG).
				NkRenderTarget mSSAORT;

				// ── Auto-exposure V1 : ping-pong 1x1 (RGBA16F) ────────────────────
				// Deux cibles PERSISTANTES (jamais recreees a l'OnResize : la valeur
				// adaptee doit survivre a un redimensionnement, sinon l'image
				// clignoterait). mLumaWrite = index ecrit par le dernier
				// RunAutoExposure ; l'autre contient l'etat de la frame precedente.
				NkRenderTarget mLumaRT[2];
				int mLumaWrite = -1; // -1 = jamais execute

				// ── TAA : historique ping-pong plein ecran (LDR RGBA8) ────────────
				// Persistant (recree seulement a l'OnResize, ou la resolution change
				// et l'historique n'est de toute facon plus valide).
				NkRenderTarget mTAART[2];
				int mTAAWrite = -1; // -1 = jamais execute (pas d'historique)
				NkPipelineHandle mPipeTAA;
				::nkentseu::NkShaderHandle mShaderTAA;
				NkDescSetHandle mTAALayout; // 3 samplers : courant + historique + depth
				static constexpr int kTAADescSets = 6;
				NkDescSetHandle mTAASets[kTAADescSets];
				int mTAASetCursor = 0;
				NkPipelineHandle mPipeAutoExp;
				::nkentseu::NkShaderHandle mShaderAutoExp;
				// Layout 2 samplers (uHDR + uPrevLuma) — distinct de mInputTexLayout.
				NkDescSetHandle mAutoExpLayout;
				// Pool : 2 cibles x 3 frames en vol. Updater un set encore utilise par
				// un draw en vol est un UB Vulkan (meme raison que le pool bloom).
				static constexpr int kAutoExpDescSets = 6;
				NkDescSetHandle mAutoExpSets[kAutoExpDescSets];
				int mAutoExpSetCursor = 0;
				// Horloge propre : le dt de l'adaptation ne doit dependre d'aucun
				// appelant (le renderer n'expose pas de delta de frame).
				NkChrono mAutoExpClock;
				bool mAutoExpClockStarted = false;

				NkPipelineHandle mPipeSSAO, mPipeTone, mPipeFXAA;
				// Phase H.5b : blur post-GTAO pour denoise.
				NkPipelineHandle mPipeSSAOBlur;
				// Phase H.2 : pipelines bloom multi-pass (downsample + upsample).
				NkPipelineHandle mPipeBloomDown;
				NkPipelineHandle mPipeBloomUp;

				// Shaders RHI (handle revoyes par NkShaderLibrary)
				::nkentseu::NkShaderHandle mShaderTone;
				::nkentseu::NkShaderHandle mShaderBloomDown;
				::nkentseu::NkShaderHandle mShaderBloomUp;
				::nkentseu::NkShaderHandle mShaderSSAO;
				::nkentseu::NkShaderHandle mShaderSSAOBlur;
				::nkentseu::NkShaderHandle mShaderFXAA; // Phase L : FXAA 3.11-style
				::nkentseu::NkShaderHandle mShaderBlit; // MirrorPresent : recopie 1:1
				NkPipelineHandle mPipeBlit;				// pipeline du blit (interface PP_FXAA)

				// Descriptor set layout (1 sampler) + set alloue, refresh par Run*.
				// Utilise par ssao, fxaa, et l'ancien tonemap mono-input.
				NkDescSetHandle mInputTexLayout;
				NkDescSetHandle mInputTexSet;
				// Set DEDIE au blit MirrorPresent : NE PAS reutiliser mInputTexSet
				// (les backends a execution differee — GL — appliquent les writes de
				// descriptor au Submit : un set partage serait ecrase par le dernier
				// bind et FXAA lirait la cible du blit -> feedback loop, image noire).
				NkDescSetHandle mBlitTexSet;

				// Phase H.2 : pool de descriptor sets pour les sub-passes bloom.
				// Vulkan interdit d'updater un descriptor set pendant qu'un draw
				// precedent l'utilise (UB). Chaque sub-pass bloom (6 down + 5 up
				// = 11 max par frame) doit avoir son propre set. Avec triple-
				// buffering (3 frames in flight) il faut 11*3 = 33 sets pour
				// que le set qu'on reutilise apres modulo soit hors-usage GPU.
				static constexpr int kBloomDescSets = 33;
				NkDescSetHandle mBloomSets[kBloomDescSets];
				int mBloomSetCursor = 0; // monotonic, modulo en runtime

				// Phase H.2 : descriptor set dedie au tonemap qui binde 2
				// textures simultanees (uHDR=0, uBloom=1). Layout separe car
				// les autres passes n'utilisent qu'un seul sampler.
				NkDescSetHandle mToneLayout;
				NkDescSetHandle mToneSet;

				NkMeshHandle mQuad;

				void CreateTextures();
				void DrawFullscreen(NkICommandBuffer *cmd, NkPipelineHandle pipe, NkTexHandle src,
									const void *pushConst, uint32 pcSize);

				// Phase H.2 : draw fullscreen vers la mip courante d'un
				// mBloomRT[targetIdx], en samplant mBloomRT[srcIdx] (ou hdr
				// pour le premier downsample). BeginRender / EndRender autour.
				void BloomPass(NkICommandBuffer *cmd, NkPipelineHandle pipe, NkTextureHandle src, int targetIdx,
							   const void *pushConst, uint32 pcSize);
		};
	} // namespace renderer
} // namespace nkentseu
