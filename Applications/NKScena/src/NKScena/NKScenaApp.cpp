// -----------------------------------------------------------------------------
// @File    NKScenaApp.cpp
// @Brief   L'application NKScena (voir NKScenaApp.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Tools/Offscreen/NkOffscreenTarget.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"

#include "NKScena/NKScenaApp.h"
#include "NKScena/NKScenaRendu.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NKMemory.h"
#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nkscena {

		using namespace ecs;
		using namespace renderer;

		namespace {
			/// LE TEMPS, avant le moteur ; la preparation de la vue, avant ses
			/// soumissions (le patron de NogeeCouchePrepa, avec le temps en plus).
			class NkScenaCoucheTemps final : public ::nkentseu::NkLayer {
				public:
					explicit NkScenaCoucheTemps(NkScenaApp *app) noexcept : ::nkentseu::NkLayer("NkScenaCoucheTemps"), mApp(app) {
					}
					void OnUpdate(float dt) override {
						mApp->Temps(dt);
					}
					void OnRender() override {
						mApp->PreparerVue();
					}

				private:
					NkScenaApp *mApp;
			};

			NkOffscreenTarget *Cible(void *p) {
				return static_cast<NkOffscreenTarget *>(p);
			}
		} // namespace

		NkScenaApp::NkScenaApp(const NkApplicationConfig &config, const NkScenaOptions &options)
			: NkApplication(config), mOptions(options) {
		}

		NkScenaApp::~NkScenaApp() {
			if (mPixels != nullptr) {
				memory::NkFree(mPixels);
				mPixels = nullptr;
			}
		}

		void NkScenaApp::OnInit() {
			// ── 1. Le renderer de l'APPLICATION ne peint que l'interface (Nogee) ──
			if (mRenderer != nullptr) {
				mRenderer->DisableSubsystem(NK_SS_RENDER3D | NK_SS_POST_PROCESS | NK_SS_VFX | NK_SS_SHADOW);
			}
			// ── 2. La vue 3D (celle de Nogee, empruntee) ─────────────────────
			const bool vue = mVue.Init(mDevice, 960u, 540u);
			// ── 3. Le temps, puis le moteur, qui EMPRUNTE le renderer de la vue ──
			PushLayer(new NkScenaCoucheTemps(this));
			mMoteur = new NkScenaCoucheMoteur();
			renderer::NkRenderer *rApp = mRenderer;
			if (vue) {
				mRenderer = mVue.Rendu();
			}
			PushLayer(mMoteur);
			mRenderer = rApp;
			// ── 4. La scene et la sequence ──────────────────────────────────
			mModele.Brancher(&mMoteur->GetWorld(), mMoteur->GetPhysicsSystem());
			if (!mOptions.sequence.Empty()) {
				(void)mModele.SceneDemo();
				mModele.NouvelleSequence();
				(void)mModele.Ouvrir(mOptions.sequence.CStr());
			} else if (mOptions.exemple) {
				(void)mModele.Exemple();
			} else {
				if (mOptions.scene.Empty() || !mModele.OuvrirScene(mOptions.scene.CStr())) {
					(void)mModele.SceneDemo();
				}
				mModele.NouvelleSequence();
				mModele.Annoncer("Une scène, une frise vide : « + Piste » pose une piste, I une clé, « Rendre » le film");
			}
			if (!mOptions.sortie.Empty()) {
				mModele.sortie.dossier = mOptions.sortie;
			}
			// ── 5. L'interface, par-dessus ──────────────────────────────────
			NkScenaHote hote;
			hote.device = mDevice;
			hote.api = mConfig.deviceInfo.api;
			hote.fenetre = &mWindow;
			hote.modele = &mModele;
			hote.vue = &mVue;
			hote.moteur = mMoteur;
			hote.rendu = &mRendu;
			hote.demandeRendu = &mDemandeRendu;
			hote.contenu = NkDirectory::Exists("Build/NKScena") ? NkString("Build/NKScena") : NkString(".");
			hote.lanceur = mOptions.lanceur;
			mUi = new NkScenaInterface(hote);
			PushOverlay(mUi);
			logger.Infof("[NKScena] pret : vue 3D %s, frise partagee, sequence « %s »\n", vue ? "hors ecran" : "ABSENTE",
						 mModele.nom.CStr());
		}

		void NkScenaApp::AppliquerOptions() {
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
			if (mOptions.curseur >= 0.f) {
				mModele.frise.SetCursor(mOptions.curseur);
			}
			mModele.vueCamera = mOptions.vueCamera;
			if (!mOptions.selection.Empty()) {
				const NkEntityId e = mModele.Entite(mOptions.selection.CStr());
				if (e.IsValid()) {
					mModele.scene.Choisir(e);
					mModele.choix = NkScenaChoix::Entite;
				}
			}
			if (mOptions.choix == NkString("piste")) {
				// La premiere piste de transformation (pas celle des plans).
				for (uint32 i = 0; i < mModele.frise.tracks.Size(); ++i) {
					if (mModele.frise.tracks[i].kind != editorkit::NkTimelineValueKind::Clips) {
						mModele.frise.activeTrack = mModele.frise.tracks[i].id;
						break;
					}
				}
				mModele.choix = NkScenaChoix::Piste;
			} else if (mOptions.choix == NkString("cle")) {
				// La DERNIERE cle de la premiere piste de transformation.
				for (uint32 i = 0; i < mModele.frise.tracks.Size(); ++i) {
					editorkit::NkTimelineTrack &t = mModele.frise.tracks[i];
					if (t.kind != editorkit::NkTimelineValueKind::Clips && !t.keys.Empty()) {
						mModele.frise.SelectKey(t.id, static_cast<int32>(t.keys.Size()) - 1, false);
						mModele.frise.activeTrack = t.id;
						break;
					}
				}
				mModele.choix = NkScenaChoix::Cle;
			} else if (mOptions.choix == NkString("plan")) {
				if (const editorkit::NkTimelineTrack *p = mModele.frise.Track(mModele.PistePlans())) {
					if (!p->clips.Empty()) {
						mModele.frise.SelectClip(p->clips[0].id, false);
						mModele.frise.activeClip = p->clips[0].id;
					}
				}
				mModele.choix = NkScenaChoix::Plan;
			} else if (mOptions.choix == NkString("sequence")) {
				mModele.choix = NkScenaChoix::Sequence;
			}
			if (mOptions.courbes) {
				mModele.frise.curveMode = true;
			}
			if (!mOptions.enregistrer.Empty()) {
				(void)mModele.Enregistrer(mOptions.enregistrer == NkString("1") ? nullptr : mOptions.enregistrer.CStr());
			}
			if (mOptions.outil >= 0) {
				mUi->Executer(SCENA_A_OUTIL + mOptions.outil);
			}
			if (mOptions.jouer) {
				mModele.frise.playing = true;
			}
			if (mOptions.rendre) {
				mDemandeRendu = true;
			}
		}

		// =====================================================================
		// LE TEMPS (avant le moteur)
		// =====================================================================
		void NkScenaApp::Temps(float32 dt) {
			if (mDemandeRendu) {
				mDemandeRendu = false;
				if (!mRendu.actif) {
					DemarrerRendu();
				}
			}
			if (mRendu.actif) {
				PiloterRendu();
				return;
			}
			(void)mModele.Image(dt);
		}

		void NkScenaApp::PreparerVue() {
			if (mUi == nullptr) {
				return;
			}
			if (mRendu.actif) {
				mVue.PreparerTrame(mRenduW, mRenduH);
				return;
			}
			const nkgui::NkRect &v = mUi->Viseur();
			const uint32 w = v.w > 32.f ? static_cast<uint32>(v.w) : 960u;
			const uint32 h = v.h > 32.f ? static_cast<uint32>(v.h) : 540u;
			mVue.PreparerTrame(w, h);
		}

		// =====================================================================
		// LE RENDU DE LA SEQUENCE, image par image, dans la vue
		// =====================================================================
		void NkScenaApp::DemarrerRendu() {
			editorkit::NkTimelineModel &f = mModele.frise;
			const float32 ips = f.fps > 0.f ? f.fps : 24.f;
			int32 n = static_cast<int32>((f.PlayEnd() - f.PlayStart()) * ips + 0.5f);
			n = n < 1 ? 1 : n;
			mRenduW = mModele.sortie.largeur < 64u ? 64u : mModele.sortie.largeur;
			mRenduH = mModele.sortie.hauteur < 64u ? 64u : mModele.sortie.hauteur;
			mRendu = NkScenaRenduEnCours{};
			mRendu.total = n;
			mRendu.dossier = mModele.sortie.dossier;
			(void)NkDirectory::CreateRecursive(mRendu.dossier.CStr());
			if (!mEcrivain.Open(mRendu.dossier.CStr(), mModele.sortie.prefixe.CStr(), static_cast<int32>(mRenduW),
								static_cast<int32>(mRenduH), media::NkImageSeqFormat::PNG, 4)) {
				mModele.Annoncer(NkString::Format("Rendre : le dossier %s est refusé", mRendu.dossier.CStr()).CStr(), 3);
				return;
			}
			if (mPixels != nullptr) {
				memory::NkFree(mPixels);
			}
			mPixels = static_cast<uint8 *>(memory::NkAlloc(static_cast<usize>(mRenduW) * mRenduH * 4u));
			if (mPixels == nullptr) {
				mModele.Annoncer("Rendre : mémoire refusée", 3);
				return;
			}
			if (mModele.CameraDuPlan(f.PlayStart()).IsValid() == false && mModele.PistePlans() == 0) {
				mModele.Annoncer("Rendre : la séquence n'a pas de plan caméra — la vue rend la caméra d'édition", 2);
			}
			mCurseurAvant = f.cursor;
			mVueCameraAvant = mModele.vueCamera;
			f.playing = false;
			mModele.vueCamera = true; // le film est ce que voit la camera du plan
			mRenduDebut = f.PlayStart();
			mRendu.actif = true;
			mRendu.image = 0;
			mAttente = 2; // la vue se refait a la taille de la sortie ; on la laisse se poser
			f.cursor = mRenduDebut;
			mModele.Evaluer(mRenduDebut);
			const NkString t = NkString::Format("Rendu : %d images %ux%u, par la caméra du plan, vers %s/%s_0001.png …", n, mRenduW, mRenduH,
												mRendu.dossier.CStr(), mModele.sortie.prefixe.CStr());
			mModele.Annoncer(t.CStr());
		}

		void NkScenaApp::PiloterRendu() {
			if (mRendu.annuler) {
				FinirRendu(false);
				return;
			}
			if (mAttente > 0) {
				--mAttente;
				return;
			}
			// L'image d'avant a ete rendue a t(i) : on la relit, GPU au repos.
			if (mVue.Largeur() != mRenduW || mVue.Hauteur() != mRenduH) {
				return; // la vue n'est pas encore a la taille de la sortie
			}
			mDevice->WaitIdle();
			// ⚠️ La relecture rend deja les rangees de HAUT en BAS, OpenGL compris
			//    (ReadbackPixels redresse) : les retourner ici donnait un film a
			//    l'envers -- vu sur la premiere image rendue, le 02/10.
			if (!mVue.Relire(mPixels)) {
				mModele.Annoncer("Rendre : la vue ne se relit pas", 3);
				FinirRendu(false);
				return;
			}
			if (!mEcrivain.WriteFrame(mPixels, media::NkVideoInputFormat::RGBA32)) {
				mModele.Annoncer(NkString::Format("Rendre : l'image %d ne s'écrit pas", mRendu.image + 1).CStr(), 3);
				FinirRendu(false);
				return;
			}
			++mRendu.image;
			// LA PROGRESSION AU JOURNAL : une ligne par dixieme (et la premiere).
			const int32 dixieme = mRendu.total >= 10 ? mRendu.total / 10 : 1;
			if (mRendu.image == 1 || mRendu.image % dixieme == 0 || mRendu.image == mRendu.total) {
				const NkString t = NkString::Format("Rendu : image %d / %d (%d %%)", mRendu.image, mRendu.total,
													mRendu.image * 100 / (mRendu.total > 0 ? mRendu.total : 1));
				if (mUi != nullptr) {
					mUi->Journal(t.CStr());
				}
				logger.Infof("[NKScena] %s\n", t.CStr());
			}
			if (mRendu.image >= mRendu.total) {
				FinirRendu(true);
				return;
			}
			const float32 ips = mModele.frise.fps > 0.f ? mModele.frise.fps : 24.f;
			const float32 t = mRenduDebut + static_cast<float32>(mRendu.image) / ips;
			mModele.frise.cursor = t;
			mModele.Evaluer(t);
		}

		void NkScenaApp::FinirRendu(bool complet) {
			mEcrivain.Close();
			mRendu.actif = false;
			mModele.vueCamera = mVueCameraAvant;
			mModele.frise.cursor = mCurseurAvant;
			mModele.Evaluer(mCurseurAvant);
			if (complet) {
				mModele.Annoncer(NkString::Format("Rendu terminé : %d images dans %s", mRendu.image, mRendu.dossier.CStr()).CStr(), 1);
			} else {
				mModele.Annoncer(NkString::Format("Rendu ARRÊTÉ à l'image %d / %d", mRendu.image, mRendu.total).CStr(), 2);
			}
			mRenduFini = true;
			if (mOptions.rendre && mOptions.capture.Empty()) {
				mCodeSortie = complet ? 0 : 1;
				Quit();
			}
		}

		// =====================================================================
		void NkScenaApp::OnUpdate(float dt) {
			mTemps += dt;
			AppliquerOptions();
			if (mUi != nullptr && mUi->DemandeQuitter()) {
				Quit();
			}
			if (!mOptions.capture.Empty()) {
				PiloterCapture();
			}
		}

		void NkScenaApp::SoumettreSelection() {
			nogee::NogeeModele &sc = mModele.scene;
			if (!mVue.Pret() || !sc.SelectionValide() || mRendu.actif) {
				return;
			}
			NkRender3D *r3d = mVue.Rendu()->GetRender3D();
			NkWorld &w = sc.Monde();
			const NkEntityId id = sc.selection;
			const NkMeshComponent *mesh = w.Get<NkMeshComponent>(id);
			const NkTransform *tf = w.Get<NkTransform>(id);
			if (r3d == nullptr || mesh == nullptr || tf == nullptr || mesh->meshHandle == 0 || !mesh->visible || w.Has<NkInactive>(id)) {
				return;
			}
			// Le LISERE ORANGE d'Unreal (le patron de Nogee : apres les soumissions).
			NkDrawCall3D dc;
			dc.mesh = NkMeshHandle{mesh->meshHandle};
			dc.transform = tf->worldMatrix;
			dc.aabb.min = {-1.0e6f, -1.0e6f, -1.0e6f};
			dc.aabb.max = {1.0e6f, 1.0e6f, 1.0e6f};
			r3d->SubmitSelection(dc, true);
		}

		void NkScenaApp::OnRender() {
			SoumettreSelection();
			mVue.Executer(GetCmd());
		}

		// =====================================================================
		// LA CAPTURE (le patron de Nogee : toute la fenetre, hors ecran, relue)
		// =====================================================================
		void NkScenaApp::PiloterCapture() {
			if (mRenderer == nullptr || mDevice == nullptr) {
				mCodeSortie = 2;
				Quit();
				return;
			}
			switch (mPhase) {
				case 0: {
					// La capture attend son instant ; pendant un rendu, elle attend
					// l'image demandee (--capture-image=N), sinon la fin du rendu.
					if (mOptions.captureImage > 0) {
						if (!mRendu.actif || mRendu.image < mOptions.captureImage) {
							return;
						}
					} else if (mTemps < mOptions.tempsCapture || mRendu.actif || (mOptions.rendre && !mRenduFini)) {
						return;
					}
					auto *cible = new NkOffscreenTarget();
					NkOffscreenDesc od;
					od.width = mDevice->GetSwapchainWidth();
					od.height = mDevice->GetSwapchainHeight();
					od.hasDepth = false;
					od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
					od.readback = true;
					od.name = "nkscena_capture";
					if (!cible->Init(mDevice, mRenderer->GetTextures(), od)) {
						std::printf("[NKScena] capture : cible hors ecran refusee\n");
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
					std::printf("[NKScena] capture : %s -> %s (%ux%u)\n", ok ? "ecrite" : "ECHEC", mOptions.capture.CStr(),
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

		void NkScenaApp::OnClose() {
			Quit();
		}

		void NkScenaApp::OnShutdown() {
			if (mDevice != nullptr) {
				mDevice->WaitIdle();
			}
			if (mRendu.actif) {
				mEcrivain.Close();
				mRendu.actif = false;
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
			mVue.Shutdown();
		}

	} // namespace nkscena
} // namespace nkentseu
