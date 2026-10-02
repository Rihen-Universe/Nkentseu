// -----------------------------------------------------------------------------
// @File    NogeeVue3D.cpp
// @Brief   La vue 3D de Nogee (voir NogeeVue3D.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Nogee/Editeur/NogeeVue3D.h"

#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Materials/NkMaterialCollection.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace nogee {

		using namespace renderer;

		namespace {
			NkOffscreenTarget *Cible(void *p) {
				return static_cast<NkOffscreenTarget *>(p);
			}
		} // namespace

		bool NogeeVue3D::Init(NkIDevice *device, uint32 w, uint32 h) {
			if (device == nullptr || !device->IsValid()) {
				logger.Errorf("[Nogee/Vue3D] pas de device : la vue reste vide\n");
				return false;
			}
			mDevice = device;
			mW = w < 32u ? 32u : w;
			mH = h < 32u ? 32u : h;
			// La configuration du jeu (celle de NkApplication, donc de NogeDemo), et
			// la cible hors ecran en plus.
			NkRendererConfig cfg;
			cfg.api = device->GetApi();
			cfg.width = mW;
			cfg.height = mH;
			cfg.Enable(NK_SS_OFFSCREEN);
			mRendu = NkRenderer::Create(device, cfg);
			if (mRendu == nullptr) {
				logger.Errorf("[Nogee/Vue3D] le renderer de la vue est refuse\n");
				return false;
			}
			NkOffscreenDesc od;
			od.width = mW;
			od.height = mH;
			od.hdr = false;
			od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
			od.hasDepth = true;
			od.readable = true;
			od.readback = true;
			od.name = "NogeeVue3D";
			NkOffscreenTarget *cible = mRendu->CreateOffscreen(od);
			if (cible == nullptr || !cible->IsValid()) {
				logger.Errorf("[Nogee/Vue3D] la cible hors ecran est refusee\n");
				NkRenderer::Destroy(mRendu);
				mRendu = nullptr;
				return false;
			}
			mCible = cible;
			// Sans l'override, le graphe rendrait a la taille de la FENETRE dans une
			// cible plus petite.
			mRendu->SetRenderSizeOverride(mW, mH);
			if (NkTextureLibrary *tex = mRendu->GetTextures()) {
				mRendu->SetFinalColorTarget(tex->GetRHIHandle(cible->GetColorResult()));
			}
			// Le LISERE de la selection (Unreal : orange). Le setter du fond
			// reconstruit le graphe, qui prend alors les passes de selection : sans
			// BeginFrame, rien d'autre ne le ferait.
			if (NkRender3D *r3d = mRendu->GetRender3D()) {
				r3d->SetSelectionOutline(true, {0.95f, 0.60f, 0.05f, 1.f}, 2.5f);
				(void)r3d->ConsumeSelOutlineGraphDirty();
			}
			mRendu->SetBackgroundColor({0.13f, 0.14f, 0.16f, 1.f});
			++mGeneration;
			logger.Infof("[Nogee/Vue3D] vue 3D prete (%ux%u), renderer dedie hors ecran\n", mW, mH);
			return true;
		}

		void NogeeVue3D::Shutdown() {
			if (mRendu != nullptr) {
				if (mDevice != nullptr) {
					mDevice->WaitIdle();
				}
				if (mCible != nullptr) {
					NkOffscreenTarget *c = Cible(mCible);
					mRendu->SetFinalColorTarget(NkTextureHandle{});
					mRendu->DestroyOffscreen(c);
					mCible = nullptr;
				}
				NkRenderer::Destroy(mRendu);
				mRendu = nullptr;
			}
		}

		void NogeeVue3D::PreparerTrame(uint32 w, uint32 h) {
			if (!Pret()) {
				return;
			}
			w = w < 32u ? 32u : w;
			h = h < 32u ? 32u : h;
			if (w != mW || h != mH) {
				NkOffscreenTarget *c = Cible(mCible);
				// La cible a peut-etre ete lue par l'image d'avant : on attend le GPU.
				mDevice->WaitIdle();
				if (c->Resize(w, h)) {
					mW = w;
					mH = h;
					mRendu->SetRenderSizeOverride(w, h);
					if (NkTextureLibrary *tex = mRendu->GetTextures()) {
						mRendu->SetFinalColorTarget(tex->GetRHIHandle(c->GetColorResult()));
					}
					++mGeneration;
				}
			}
			// Ce que NkRenderer::BeginFrame ferait si ce renderer possedait la frame.
			mRendu->FlushGraphRebuilds();
			if (NkRender3D *r3d = mRendu->GetRender3D()) {
				r3d->ResetFrame();
			}
			if (NkMaterialCollection *mc = mRendu->GetMaterialCollection()) {
				mc->Upload();
			}
		}

		void NogeeVue3D::Executer(NkICommandBuffer *cmd) {
			if (!Pret() || cmd == nullptr) {
				return;
			}
			// Presenter = executer le graphe DANS le command buffer de l'image : il
			// ecrit dans la cible hors ecran, que l'interface affiche ensuite.
			if (NkRenderGraph *graphe = mRendu->GetRenderGraph()) {
				graphe->Execute(cmd);
			}
		}

		bool NogeeVue3D::Relire(uint8 *rgba) {
			if (!Pret() || rgba == nullptr) {
				return false;
			}
			return Cible(mCible)->ReadbackPixels(rgba, mW * 4u);
		}

		NkTextureHandle NogeeVue3D::Texture() const noexcept {
			if (mRendu == nullptr || mCible == nullptr) {
				return NkTextureHandle{};
			}
			NkTextureLibrary *tex = mRendu->GetTextures();
			return tex != nullptr ? tex->GetRHIHandle(Cible(mCible)->GetColorResult()) : NkTextureHandle{};
		}

	} // namespace nogee
} // namespace nkentseu
