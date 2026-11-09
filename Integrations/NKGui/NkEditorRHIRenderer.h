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
#include "NKGui/Core/NkGuiFont.h"	  // (26/10) les ideogrammes a la demande
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
					// Meme garde que dans NkEditorCanvasRenderer, et pour la meme
					// raison : sans elle, `Metal` demande hors Apple tomberait dans
					// le `default:` et lancerait DX11 ou OpenGL en silence.
					// ⚠️ Elle est d'autant plus necessaire ICI que la creation du
					//    device passe ensuite par `CreateWithFallback` -- un repli
					//    deliberé, utile en production, mais qui rendrait un backend
					//    refuse indiscernable d'un backend obtenu.
					const char *raison = "";
					if (!editorkit::NkEditorGfxApiSupported(api, &raison)) {
						logger.Errorf("[NkEditorRHI] backend graphique '%s' REFUSE : %s\n",
									  editorkit::NkEditorGfxApiName(api), raison);
						return false;
					}

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
						case editorkit::NkEditorGfxApi::Metal:
							// Atteignable UNIQUEMENT sur Apple (garde ci-dessus).
							di.api = NkGraphicsApi::NK_GFX_API_METAL;
							break;
						case editorkit::NkEditorGfxApi::Software:
							di.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
							break;
						default:
#if defined(NKENTSEU_PLATFORM_WINDOWS)
							di.api = NkGraphicsApi::NK_GFX_API_DX11;
#elif defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
							// Sur Apple, l'OpenGL de NKRHI n'existe pas (refus nomme) et
							// le device Metal n'a pas encore de MSL pour NKGui : `auto`
							// y retombait sur OpenGL et NKCraft refusait de s'ouvrir. Le
							// rendu logiciel est le seul dorsal qui tient aujourd'hui
							// (CI macOS du 2026-09-30, voir NkDeviceFactory.cpp).
							di.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
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
					LibererCapture(); // (09/11) la cible de capture, si elle a servi
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
					// (26/10) LES IDEOGRAMMES DEMANDES a l'image precedente : l'atlas de leur police se refait ICI,
					// avant que l'interface de cette image ne s'ecrive, et repart au GPU sous le meme texId
					// (NKGui/Core/NkGuiFont.h, les ideogrammes a la demande)
					(void)NkGuiPolicesAppliquerDemandes(
						[](uint32 tex, const uint8 *px, int32 w, int32 h, void *u) {
							return static_cast<NkEditorRHIRenderer *>(u)->mBackend.UploadTextureGray8(tex, px, w, h);
						},
						this);
					mFrameOk = false;
					// La liste fusionnee de la frame precedente est caduque. `Reset`
					// vide sans rendre la capacite : aucune reallocation par frame.
					mMerged.Reset();
					mFbW = 0;
					mFbH = 0;
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

					// ── LA PASSE BACKBUFFER EFFACE. C'est le loadOp CLEAR de cette
					// passe, et il se DEMANDE : `BeginRenderPass` n'efface que si un
					// `SetClearColor` l'a arme juste avant (cf. NkDirectX11CommandBuffer,
					// « Clear UNIQUEMENT si demande »). Personne ne l'armait ici : la
					// passe etait donc en LOAD, et chaque frame heritait de l'image
					// laissee dans ce buffer de swapchain -- pas celle d'avant, mais
					// celle d'il y a deux ou trois frames, selon le nombre de buffers.
					//
					// Tant que l'interface repeignait chaque pixel en opaque, l'heritage
					// etait integralement recouvert et ne se voyait pas. Il s'est vu des
					// que des surfaces ont cesse de couvrir : une fenetre modale qu'on
					// deplace laisse la trace de ses positions passees, et un voile
					// semi-transparent se REPEINT sur sa propre trace a chaque frame --
					// d'ou l'assombrissement progressif de la bande du haut, cherche en
					// vain du cote du voile lui-meme (Rihen, 12-13 aout : « on ne voit
					// plus l'entete », puis « le probleme actuel c'est l'effacement »).
					//
					// Noir opaque : l'interface recouvre tout ce qui se voit, cette
					// couleur n'est qu'un fond de depart franc.
					mCmd->SetClearColor(0.f, 0.f, 0.f, 1.f);
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

				// ── PLUSIEURS LISTES PAR FRAME : ON FUSIONNE, ON N'ENVOIE PAS DEUX FOIS ─
				// `NkGuiRHIBackend::Submit` ecrit la geometrie AU DEBUT de son tampon de
				// sommets partage, puis enregistre des DrawIndexed qui ne s'executeront
				// qu'a la presentation. Son contrat est donc « une fois par frame ».
				//
				// L'appeler deux fois -- contenu puis surcouche, depuis que les
				// composants de NKEditorKit dessinent dans `dlOverlay` -- ecrasait le
				// DEBUT du tampon avec les sommets de la surcouche. A l'execution, les
				// dessins de la premiere liste lisaient une geometrie qui n'etait plus la
				// leur ; seuls survivaient les sommets situes AU-DELA de la taille de la
				// seconde liste, c'est-a-dire ce qui avait ete peint EN DERNIER.
				//
				// Le symptome se lisait a l'envers : « la bande du haut n'est pas
				// peinte » (Rihen, 12-13 aout) -- barre de menus et hierarchie sont
				// peintes en PREMIER, donc detruites en premier, et la zone detruite
				// grandissait avec la taille de la surcouche (une modale : la bande du
				// haut ; le selecteur et son arbre : la moitie de l'ecran).
				//
				// On accumule donc dans une liste unique -- `Append` decale deja les
				// index -- et on envoie une seule fois, en fin de frame. L'ordre d'appel
				// est conserve : la surcouche reste dessinee par-dessus.
				void SubmitDrawList(const nkgui::NkGuiDrawList &dl, uint32 fbW, uint32 fbH) override {
					if (!mFrameOk)
						return;
					mMerged.Append(dl);
					mFbW = fbW;
					mFbH = fbH;
				}

				void EndFrame() override {
					if (!mDev)
						return;
					bool relire = false;
					if (mFrameOk) {
						if (mFbW > 0 && mFbH > 0)
							mBackend.Submit(mCmd, mMerged, mFbW, mFbH);
						mCmd->EndRenderPass();
						// (09/11) LA CAPTURE DEMANDEE : la MEME liste, rendue une seconde fois dans la cible de
						// capture, puis copiee vers le tampon de relecture (voir DemanderCapture)
						if (mCaptureDemandee && mFbW > 0 && mFbH > 0)
							relire = EnregistrerCapture();
						mCmd->End();
						mDev->SubmitAndPresent(mCmd);
					}
					mDev->EndFrame(mFrame);
					mFrameOk = false;
					mCaptureDemandee = false;
					if (relire)
						LireCapture();
				}

				// ── (09/11) L'IMAGE ENTIERE, INTERFACE COMPRISE, RELUE SUR LE GPU ───────
				// NKCraft photographiait sa fenetre par PrintWindow, qui demande a la FENETRE de se
				// redessiner : une fenetre jamais montree (NK_FENETRE_CACHEE=1) ne rend rien, la photo
				// echoue (agent archviz, 09/11, journal e1_interface_dx11). Ici, rien ne passe par l'OS :
				// la liste fusionnee de l'image est rendue une seconde fois, telle quelle, dans une cible
				// hors ecran du format de la chaine d'echange (meme pipeline : la meme geometrie, les memes
				// textures), puis copiee vers un tampon de relecture et lue apres la presentation -- comme
				// NkOffscreenTarget::ReadbackPixels lit la vue 3D (Demo3DHostCaptureView). Une demande vaut
				// pour UNE image : la prochaine EndFrame. Le cout (une attente du GPU) n'est paye que la.
				void DemanderCapture() noexcept {
					mCaptureDemandee = true;
				}
				/// Vrai si une image a ete relue et pas encore oubliee.
				bool CaptureDisponible() const noexcept {
					return mCapturePrete;
				}
				/// L'image relue : RGBA8, la ligne du HAUT en premier, alpha opaque.
				const uint8 *CapturePixels() const noexcept {
					return mCapturePrete ? mCapturePx.Data() : nullptr;
				}
				uint32 CaptureLargeur() const noexcept {
					return mCapturePrete ? mCapW : 0u;
				}
				uint32 CaptureHauteur() const noexcept {
					return mCapturePrete ? mCapH : 0u;
				}
				void OublierCapture() noexcept {
					mCapturePrete = false;
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
				/// (09/11) La cible de capture a la taille de la chaine d'echange (refaite si elle change).
				bool PreparerCapture(uint32 w, uint32 h) {
					if (mCapTex.IsValid() && mCapRP.IsValid() && mCapFB.IsValid() && mCapBuf.IsValid() && mCapW == w &&
						mCapH == h)
						return true;
					LibererCapture();
					const NkGPUFormat fc = mDev->GetSwapchainFormat();
					const NkGPUFormat fp = mDev->GetSwapchainDepthFormat();
					NkTextureDesc d;
					d.type = NkTextureType::NK_TEX2D;
					d.format = fc;
					d.width = w;
					d.height = h;
					d.depth = 1;
					d.arrayLayers = 1;
					d.mipLevels = 1;
					d.bindFlags = NkBindFlags::NK_RENDER_TARGET | NkBindFlags::NK_SHADER_RESOURCE;
					d.usage = NkResourceUsage::NK_DEFAULT;
					d.debugName = "NkEditorCapture_Couleur";
					mCapTex = mDev->CreateTexture(d);
					// la profondeur, si la passe de la chaine d'echange en a une : la meme forme de passe pour le
					// meme pipeline (Vulkan exige des passes compatibles)
					if (fp != NkGPUFormat::NK_UNDEFINED) {
						NkTextureDesc p = d;
						p.format = fp;
						p.bindFlags = NkBindFlags::NK_DEPTH_STENCIL;
						p.debugName = "NkEditorCapture_Profondeur";
						mCapProf = mDev->CreateTexture(p);
					}
					NkRenderPassDesc rpd;
					rpd.AddColor(NkAttachmentDesc::Color(fc));
					if (mCapProf.IsValid())
						rpd.SetDepth(NkAttachmentDesc::Depth(fp));
					mCapRP = mDev->CreateRenderPass(rpd);
					NkFramebufferDesc fbd;
					fbd.width = w;
					fbd.height = h;
					fbd.renderPass = mCapRP;
					fbd.colorAttachments.PushBack(mCapTex);
					if (mCapProf.IsValid())
						fbd.depthAttachment = mCapProf;
					fbd.debugName = "NkEditorCapture";
					mCapFB = mDev->CreateFramebuffer(fbd);
					NkBufferDesc bd;
					bd.sizeBytes = (uint64)((w * 4u + 255u) & ~255u) * h;
					bd.type = NkBufferType::NK_STAGING;
					bd.usage = NkResourceUsage::NK_READBACK;
					mCapBuf = mDev->CreateBuffer(bd);
					mCapW = w;
					mCapH = h;
					if (!mCapTex.IsValid() || !mCapRP.IsValid() || !mCapFB.IsValid() || !mCapBuf.IsValid()) {
						logger.Errorf("[NkEditorRHI] capture : la cible %ux%u n'a pas pu etre creee\n", w, h);
						LibererCapture();
						return false;
					}
					return true;
				}

				void LibererCapture() {
					if (!mDev)
						return;
					if (mCapFB.IsValid())
						mDev->DestroyFramebuffer(mCapFB);
					if (mCapRP.IsValid())
						mDev->DestroyRenderPass(mCapRP);
					if (mCapBuf.IsValid())
						mDev->DestroyBuffer(mCapBuf);
					if (mCapTex.IsValid())
						mDev->DestroyTexture(mCapTex);
					if (mCapProf.IsValid())
						mDev->DestroyTexture(mCapProf);
					mCapW = mCapH = 0u;
				}

				/// Enregistre, sur le command buffer de l'image, la seconde passe et la copie. Faux si la cible manque.
				bool EnregistrerCapture() {
					const uint32 w = mDev->GetSwapchainWidth(), h = mDev->GetSwapchainHeight();
					if (w == 0u || h == 0u || !PreparerCapture(w, h))
						return false;
					mCmd->TextureBarrier(mCapTex, NkResourceState::NK_UNDEFINED, NkResourceState::NK_RENDER_TARGET);
					mCmd->SetClearColor(0.f, 0.f, 0.f, 1.f);
					if (!mCmd->BeginRenderPass(mCapRP, mCapFB, NkRect2D{0, 0, (int32)w, (int32)h}))
						return false;
					mCmd->SetViewport(NkViewport(0.f, 0.f, (float32)w, (float32)h));
					mCmd->SetScissor(NkRect2D(0u, 0u, w, h));
					// la MEME liste : le backend reecrit les memes sommets au meme endroit (le contrat « une fois
					// par image » tient : le contenu ne change pas)
					mBackend.Submit(mCmd, mMerged, mFbW, mFbH);
					mCmd->EndRenderPass();
					mCmd->TextureBarrier(mCapTex, NkResourceState::NK_RENDER_TARGET, NkResourceState::NK_TRANSFER_SRC);
					NkBufferTextureCopyRegion r{};
					r.width = w;
					r.height = h;
					r.depth = 1;
					r.bufferRowPitch = 0;
					mCmd->CopyTextureToBuffer(mCapTex, mCapBuf, r);
					mCmd->TextureBarrier(mCapTex, NkResourceState::NK_TRANSFER_SRC, NkResourceState::NK_SHADER_READ);
					return true;
				}

				/// Apres la presentation : attend le GPU, lit le tampon, le range en RGBA8 (haut en premier).
				void LireCapture() {
					mDev->WaitIdle();
					const uint32 w = mCapW, h = mCapH;
					const NkGraphicsApi api = mDev->GetApi();
					const bool bas = api == NkGraphicsApi::NK_GFX_API_OPENGL; // OpenGL range la ligne du BAS d'abord
					const uint32 pas = (api == NkGraphicsApi::NK_GFX_API_DX12) ? ((w * 4u + 255u) & ~255u) : w * 4u;
					const NkGPUFormat fc = mDev->GetSwapchainFormat();
					const bool bgra = fc == NkGPUFormat::NK_BGRA8_UNORM || fc == NkGPUFormat::NK_BGRA8_SRGB;
					NkMappedMemory m = mDev->MapBuffer(mCapBuf);
					if (!m.IsValid()) {
						mCapturePrete = false;
						return;
					}
					mCapturePx.Resize((usize)w * h * 4u);
					const uint8 *src = (const uint8 *)m.ptr;
					for (uint32 y = 0; y < h; ++y) {
						const uint8 *s = src + (uint64)(bas ? (h - 1u - y) : y) * pas;
						uint8 *o = mCapturePx.Data() + (uint64)y * w * 4u;
						for (uint32 x = 0; x < w; ++x) {
							o[4u * x + 0u] = s[4u * x + (bgra ? 2u : 0u)];
							o[4u * x + 1u] = s[4u * x + 1u];
							o[4u * x + 2u] = s[4u * x + (bgra ? 0u : 2u)];
							o[4u * x + 3u] = 255u;
						}
					}
					mDev->UnmapBuffer(mCapBuf);
					mCapturePrete = true;
				}

				NkIDevice *mDev = nullptr;
				NkICommandBuffer *mCmd = nullptr;
				// (09/11) la capture relue sur le GPU
				bool mCaptureDemandee = false, mCapturePrete = false;
				NkTextureHandle mCapTex, mCapProf;
				NkRenderPassHandle mCapRP;
				NkFramebufferHandle mCapFB;
				NkBufferHandle mCapBuf;
				uint32 mCapW = 0u, mCapH = 0u;
				NkVector<uint8> mCapturePx;
				NkGuiRHIBackend mBackend;
				/// Toutes les listes de la frame, dans l'ordre de soumission : le
				/// backend n'en accepte qu'une (tampon de sommets partage).
				nkgui::NkGuiDrawList mMerged;
				uint32 mFbW = 0, mFbH = 0;
				NkFrameContext mFrame{};
				bool mFrameOk = false;
				PreUIFn mPreUI = nullptr;
				void *mPreUIUser = nullptr;
		};

	} // namespace nkgui
} // namespace nkentseu
