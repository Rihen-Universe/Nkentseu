/*
    * NkEditorRHIRenderer.h
    *
    * Implementation NKRHI GENERIQUE de editorkit::NkIEditorRenderer.
    *
    * Ce fichier fait partie de Nkentseu.
    *
    * Generalisation (2026-07-24) de l'implementation de reference validee a
    * l'ecran dans Applications/NkAnimaEditor : toute application moteur
    * (NkAnimaEditor, Nogee, ...) qui veut la coquille NkEditorShell rendue sur
    * NKRHI/NKRenderer (et PAS NKCanvas — regle « une fenetre = une pile »)
    * injecte cette classe via NkEditorShellConfig::renderer.
    *
    * Pilote la frame au niveau DEVICE : device + swapchain + render pass
    * backbuffer, et rend les draw-lists NKGui via NkGuiRHIBackend.
    *
    * Pour un viewport 3D offscreen (NkRenderer) partageant CE device :
    *   - GetDevice()  : le device NKRHI a partager,
    *   - SetPreUI()   : callback appele en debut de frame (device frame ouvert,
    *                    AVANT la passe backbuffer) — la scene y est rendue dans
    *                    sa cible offscreen sur le MEME command buffer,
    *   - GetBackend() : NkGuiRHIBackend::RegisterTexture(texId, tex) publie la
    *                    cible offscreen pour NkGuiDrawList::AddImage.
    *
    * Header-only. Symbole : nkentseu::nkgui::NkEditorRHIRenderer.
    * NOTE jenga : le consommateur doit dependre de NKEditorKit (interface
    * NkIEditorRenderer) en plus de NKGuiIntegration ; NKGuiIntegration lui-meme
    * ne depend PAS de NKEditorKit (ses .cpp n'incluent pas ce header).
*/

#pragma once

#include "NKEditorKit/NkIEditorRenderer.h"
#include "NKGui/NkGuiRHIBackend.h" // Integrations/NKGui
#include "NKWindow/NKWindow.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Core/NkDeviceInitInfo.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace nkgui {

		// Backend de rendu NKRHI injectable dans NkEditorShell (cfg.renderer).
		// Doit survivre au shell (le shell ne possede pas un renderer injecte).
		class NkEditorRHIRenderer final : public editorkit::NkIEditorRenderer {
			public:
				bool Init(NkWindow &window, editorkit::NkEditorGfxApi api) override {
					NkDeviceInitInfo di;
					switch (api) {
						case editorkit::NkEditorGfxApi::OpenGL:
							di.api = NkGraphicsApi::NK_GFX_API_OPENGL;
							break;
						case editorkit::NkEditorGfxApi::Vulkan:
							di.api = NkGraphicsApi::NK_GFX_API_VULKAN;
							break;
						case editorkit::NkEditorGfxApi::DX11:
							di.api = NkGraphicsApi::NK_GFX_API_DX11;
							break;
						case editorkit::NkEditorGfxApi::DX12:
							di.api = NkGraphicsApi::NK_GFX_API_DX12;
							break;
						case editorkit::NkEditorGfxApi::Software:
							di.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
							break;
						default:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
							di.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
							di.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
							break;
					}
					di.surface = window.GetSurfaceDesc();
					di.width = window.GetSize().width;
					di.height = window.GetSize().height;
					di.context.swapchainFormat = NkSwapchainFormat::NK_SWAPCHAIN_BGRA8_UNORM;

					mDev = NkDeviceFactory::CreateWithFallback(
						di, {di.api, NkGraphicsApi::NK_GFX_API_VULKAN, NkGraphicsApi::NK_GFX_API_OPENGL});
					if (!mDev || !mDev->IsValid()) {
						logger.Errorf("[NkEditorRHI] device echec\n");
						return false;
					}

					mCmd = mDev->CreateCommandBuffer();
					if (!mCmd) {
						logger.Errorf("[NkEditorRHI] command buffer echec\n");
						return false;
					}

					if (!mBackend.Init(mDev, mDev->GetSwapchainRenderPass(), mDev->GetApi())) {
						logger.Errorf("[NkEditorRHI] backend NKGui echec\n");
						return false;
					}
					logger.Info("[NkEditorRHI] pret : API={0}\n", NkGraphicsApiName(mDev->GetApi()));
					return true;
				}

				void Shutdown() override {
					if (mDev)
						mDev->WaitIdle();
					mBackend.Destroy();
					if (mDev) {
						NkDeviceFactory::Destroy(mDev);
						mDev = nullptr;
					}
					mCmd = nullptr;
				}

				bool IsValid() const override {
					return mDev && mDev->IsValid();
				}

				math::NkVec2u Size() const override {
					if (!mDev)
						return {0, 0};
					return {mDev->GetSwapchainWidth(), mDev->GetSwapchainHeight()};
				}

				void OnResize(uint32 w, uint32 h) override {
					// GARDE ANTI-MINIMISATION : une fenetre en cours de reduction
					// glisse son rect placeholder (~160x28 sous Windows) entre le
					// test « minimisee ? » de la boucle principale et la lecture
					// de taille -- la course est reelle (defaut 4.3, reproduite).
					// Sous 32 px, les cibles divisees du rendu (bloom /32) tombent
					// a zero et CreateTexture2D echoue en E_INVALIDARG -> mort a
					// la restauration. On refuse net : la vraie taille arrivera
					// avec le retour de la fenetre.
					if (w < 32 || h < 32)
						return;
					if (mDev)
						mDev->OnResize(w, h);
				}

				void BeginFrame() override {
					mFrameOk = false;
					if (!mDev || !mCmd)
						return;
					if (!mDev->BeginFrame(mFrame))
						return;
					const uint32 w = mDev->GetSwapchainWidth(), h = mDev->GetSwapchainHeight();
					if (w == 0 || h == 0) {
						mDev->EndFrame(mFrame);
						return;
					}

					mCmd->Reset();
					if (!mCmd->Begin()) {
						mDev->EndFrame(mFrame);
						return;
					}

					// Hook pre-UI (viewport 3D offscreen) : rendu sur le MEME cmd, APRES
					// Begin() (sinon Reset() efface ses commandes) et AVANT la passe
					// backbuffer (ses BeginCapture/EndCapture ouvrent leurs propres
					// passes ; pas de render pass imbriquee avec celle du backbuffer).
					if (mPreUI)
						mPreUI(mCmd, mPreUIUser);

					if (!mCmd->BeginRenderPass(mDev->GetSwapchainRenderPass(), mDev->GetSwapchainFramebuffer(),
											   NkRect2D{0, 0, (int32)w, (int32)h})) {
						mCmd->End();
						mDev->EndFrame(mFrame);
						return;
					}
					mCmd->SetViewport(NkViewport(0.f, 0.f, (float32)w, (float32)h));
					mCmd->SetScissor(NkRect2D(0u, 0u, w, h));
					mFrameOk = true;
				}

				void SubmitDrawList(const nkgui::NkGuiDrawList &dl, uint32 fbW, uint32 fbH) override {
					if (mFrameOk)
						mBackend.Submit(mCmd, dl, fbW, fbH);
				}

				void EndFrame() override {
					if (!mDev)
						return;
					if (mFrameOk) {
						mCmd->EndRenderPass();
						mCmd->End();
						mDev->SubmitAndPresent(mCmd);
					}
					mDev->EndFrame(mFrame);
					mFrameOk = false;
				}

				bool UploadFontGray8(uint32 texId, const uint8 *px, int32 w, int32 h) override {
					return mBackend.UploadTextureGray8(texId, px, w, h);
				}

				bool UploadImageRGBA(uint32 texId, const uint8 *px, int32 w, int32 h) override {
					return mBackend.UploadTextureRGBA8(texId, px, w, h);
				}

				// ── Partage pour un viewport 3D offscreen (device partage) ───────────
				NkIDevice *GetDevice() noexcept {
					return mDev;
				}

				NkGuiRHIBackend &GetBackend() noexcept {
					return mBackend;
				}

				// Callback appele en debut de frame (device frame ouvert, AVANT la
				// passe backbuffer) : la scene 3D y rend dans sa cible offscreen.
				using PreUIFn = void (*)(NkICommandBuffer *cmd, void *user);

				void SetPreUI(PreUIFn fn, void *user) noexcept {
					mPreUI = fn;
					mPreUIUser = user;
				}

			private:
				NkIDevice *mDev = nullptr;
				NkICommandBuffer *mCmd = nullptr;
				NkGuiRHIBackend mBackend;
				NkFrameContext mFrame{};
				bool mFrameOk = false;
				PreUIFn mPreUI = nullptr;
				void *mPreUIUser = nullptr;
		};

	} // namespace nkgui
} // namespace nkentseu
