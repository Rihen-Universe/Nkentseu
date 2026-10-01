// -----------------------------------------------------------------------------
// @File    NogeeEditeurApp.cpp
// @Brief   L'application de l'editeur Nogee (voir NogeeEditeurApp.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Nogee/Editeur/NogeeEditeurApp.h"
#include "Nogee/Editeur/NogeeInterface.h"

#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "NKLogger/NkLog.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nogee {

		using namespace ecs;
		using namespace renderer;

		namespace {
			/// Prepare la vue AVANT que le moteur ne lui soumette la scene : elle est
			/// empilee avant NkEngineLayer, dont OnRender vient juste apres.
			class NogeeCouchePrepa final : public ::nkentseu::NkLayer {
				public:
					explicit NogeeCouchePrepa(NogeeEditeurApp *app) noexcept : ::nkentseu::NkLayer("NogeeCouchePrepa"), mApp(app) {
					}
					void OnRender() override {
						mApp->PreparerVue();
					}

				private:
					NogeeEditeurApp *mApp;
			};

			NkOffscreenTarget *Cible(void *p) {
				return static_cast<NkOffscreenTarget *>(p);
			}
		} // namespace

		// =====================================================================
		void NogeeCoucheMoteur::OnFixedUpdate(float fixedDt) {
			// En EDITION, rien ne tombe : la physique n'avance qu'en jeu (et d'un
			// pas a la fois en pause, bouton « ▶| »).
			if (modele != nullptr && modele->PhysiqueAvance()) {
				NkEngineLayer::OnFixedUpdate(fixedDt);
			}
		}

		// =====================================================================
		NogeeEditeurApp::NogeeEditeurApp(const NkApplicationConfig &config, const NogeeOptions &options)
			: NkApplication(config), mOptions(options) {
		}

		NogeeEditeurApp::~NogeeEditeurApp() = default;

		void NogeeEditeurApp::OnInit() {
			// ── 1. Le renderer de l'APPLICATION ne peint que l'interface ─────────
			// Sa passe Overlay2D efface alors la swapchain et y pose l'interface ;
			// la 3D est au renderer de la vue (NogeeVue3D.h : pourquoi deux).
			if (mRenderer != nullptr) {
				mRenderer->DisableSubsystem(NK_SS_RENDER3D | NK_SS_POST_PROCESS | NK_SS_VFX | NK_SS_SHADOW);
			}
			// ── 2. La vue 3D ──────────────────────────────────────────────────
			const bool vue = mVue.Init(mDevice, 960u, 540u);
			// ── 3. La preparation, puis le moteur, qui EMPRUNTE le renderer de la
			//    vue (NkEngineLayer::InitRenderer lit NkApplication::GetRenderer)
			PushLayer(new NogeeCouchePrepa(this));
			mMoteur = new NogeeCoucheMoteur();
			mMoteur->modele = &mModele;
			renderer::NkRenderer *rApp = mRenderer;
			if (vue) {
				mRenderer = mVue.Rendu();
			}
			PushLayer(mMoteur);
			mRenderer = rApp;
			// ── 4. La scene ───────────────────────────────────────────────────
			mModele.Brancher(&mMoteur->GetWorld(), mMoteur->GetPhysicsSystem());
			mModele.NouvelleScene();
			if (!mOptions.scene.Empty()) {
				(void)mModele.Ouvrir(mOptions.scene.CStr());
			}
			// ── 5. L'interface, par-dessus ────────────────────────────────────
			NogeeHote hote;
			hote.device = mDevice;
			hote.api = mConfig.deviceInfo.api;
			hote.fenetre = &mWindow;
			hote.modele = &mModele;
			hote.vue = &mVue;
			hote.moteur = mMoteur;
			hote.contenu = mOptions.contenu;
			mUi = new NogeeInterface(hote);
			PushOverlay(mUi);
			logger.Infof("[Nogee] editeur pret : vue 3D %s, interface de la famille (R32)\n", vue ? "hors ecran" : "ABSENTE");
		}

		void NogeeEditeurApp::AppliquerOptions() {
			if (mOptionsAppliquees || mUi == nullptr) {
				return;
			}
			mOptionsAppliquees = true;
			if (mOptions.clair) {
				mUi->PoserTheme(true);
			}
			if (mOptions.tiroir >= 0) {
				mUi->PoserOngletTiroir(mOptions.tiroir);
			}
			if (mOptions.outil >= 0) {
				mModele.outil = static_cast<NogeeOutil>(mOptions.outil);
			}
			if (!mOptions.selection.Empty()) {
				mModele.Monde().Query<const NkName>().ForEach([&](NkEntityId id, const NkName &n) {
					if (!mModele.aSelection && std::strncmp(n.value, mOptions.selection.CStr(), mOptions.selection.Length()) == 0) {
						mModele.Choisir(id);
					}
				});
			}
			if (mOptions.jouer) {
				mModele.Jouer();
			}
			if (mOptions.selecteur) {
				mUi->Executer(NOGEE_A_OUVRIR_FICHIER);
			}
		}

		void NogeeEditeurApp::OnUpdate(float dt) {
			mTemps += dt;
			AppliquerOptions();
			if (mUi != nullptr && mUi->DemandeQuitter()) {
				Quit();
			}
			if (!mOptions.capture.Empty()) {
				PiloterCapture();
			}
		}

		void NogeeEditeurApp::PreparerVue() {
			if (mUi == nullptr) {
				return;
			}
			const nkgui::NkRect &v = mUi->Viseur();
			const uint32 w = v.w > 32.f ? static_cast<uint32>(v.w) : 960u;
			const uint32 h = v.h > 32.f ? static_cast<uint32>(v.h) : 540u;
			mVue.PreparerTrame(w, h);
		}

		void NogeeEditeurApp::SoumettreSelection() {
			if (!mVue.Pret() || !mModele.SelectionValide()) {
				return;
			}
			NkRender3D *r3d = mVue.Rendu()->GetRender3D();
			NkMeshSystem *ms = mVue.Rendu()->GetMeshSystem();
			const NkEntityId id = mModele.selection;
			NkWorld &w = mModele.Monde();
			const NkMeshComponent *mesh = w.Get<NkMeshComponent>(id);
			const NkTransform *tf = w.Get<NkTransform>(id);
			if (r3d == nullptr || mesh == nullptr || tf == nullptr || mesh->meshHandle == 0 || !mesh->visible ||
				w.Has<NkInactive>(id)) {
				return;
			}
			// LE LISERE ORANGE d'Unreal, rendu par le moteur (passe SelectionMask +
			// SelectionOutline) : la file de selection est videe par BeginScene,
			// on la remplit apres les soumissions du moteur.
			NkDrawCall3D dc;
			dc.mesh = NkMeshHandle{mesh->meshHandle};
			dc.transform = tf->worldMatrix;
			dc.aabb.min = {-1.0e6f, -1.0e6f, -1.0e6f};
			dc.aabb.max = {1.0e6f, 1.0e6f, 1.0e6f};
			(void)ms;
			r3d->SubmitSelection(dc, true);
		}

		void NogeeEditeurApp::OnRender() {
			SoumettreSelection();
			// Le graphe de la vue, dans le command buffer de l'image : il ecrit la
			// cible hors ecran que la passe d'interface lira juste apres.
			mVue.Executer(GetCmd());
		}

		// =====================================================================
		// LA CAPTURE : la sortie finale du renderer de l'application (donc toute
		// l'interface, la vue comprise) va dans une cible hors ecran, relue.
		// =====================================================================
		void NogeeEditeurApp::PiloterCapture() {
			if (mRenderer == nullptr || mDevice == nullptr) {
				mCodeSortie = 2;
				Quit();
				return;
			}
			switch (mPhase) {
				case 0: {
					if (mTemps < mOptions.tempsCapture) {
						return;
					}
					auto *cible = new NkOffscreenTarget();
					NkOffscreenDesc od;
					od.width = mDevice->GetSwapchainWidth();
					od.height = mDevice->GetSwapchainHeight();
					od.hasDepth = false;
					od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
					od.readback = true;
					od.name = "nogee_capture";
					if (!cible->Init(mDevice, mRenderer->GetTextures(), od)) {
						std::printf("[Nogee] capture : cible hors ecran refusee\n");
						delete cible;
						mCodeSortie = 2;
						Quit();
						return;
					}
					mCapture = cible;
					mRenderer->SetFinalColorTarget(mRenderer->GetTextures()->GetRHIHandle(cible->GetColorResult()));
					mPhase = 1;
					mImages = 0;
					return;
				}
				case 1: {
					if (++mImages < 4) {
						return;
					}
					mDevice->WaitIdle();
					const bool ok = Cible(mCapture)->Capture(mOptions.capture.CStr());
					std::printf("[Nogee] capture : %s -> %s (%ux%u)\n", ok ? "ecrite" : "ECHEC", mOptions.capture.CStr(),
								Cible(mCapture)->GetWidth(), mDevice->GetSwapchainHeight());
					std::fflush(stdout);
					mRenderer->SetFinalColorTarget(NkTextureHandle{});
					mCodeSortie = ok ? 0 : 1;
					mPhase = 2;
					Quit();
					return;
				}
				default:
					return;
			}
		}

		void NogeeEditeurApp::OnClose() {
			// La croix de l'OS, Alt+F4 : la meme sortie que Fichier > Quitter.
			Quit();
		}

		void NogeeEditeurApp::OnShutdown() {
			if (mDevice != nullptr) {
				mDevice->WaitIdle();
			}
			if (mUi != nullptr) {
				mUi->LibererGpu();
			}
			if (mCapture != nullptr) {
				if (mRenderer != nullptr) {
					mRenderer->SetFinalColorTarget(NkTextureHandle{});
				}
				Cible(mCapture)->Shutdown();
				delete Cible(mCapture);
				mCapture = nullptr;
			}
			// La vue AVANT le device (NkApplication le detruit juste apres). Le moteur
			// (dans la pile de couches) l'empruntait : il n'en a plus besoin a l'arret.
			mVue.Shutdown();
		}

	} // namespace nogee
} // namespace nkentseu
