#include "MedicalUILayer.h"
#include "PatientLayer.h"
#include "Noge/Core/NkApplication.h"
#include "NKRenderer/NkRenderer.h" // SetUIOverlayCallback (passe Overlay2D)
#include "NKLogger/NkLog.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include <cstdio>

namespace nkentseu {
	namespace pv3de {

		using namespace nkgui;

		MedicalUILayer::MedicalUILayer(const NkString &name, NkIDevice *device, NkICommandBuffer *cmd,
									   NkGraphicsApi api, PatientLayer *patient) noexcept
			: NkOverlay(name), mDevice(device), mCmd(cmd), mApi(api), mPatient(patient) {
		}

		MedicalUILayer::~MedicalUILayer() = default;

		// =====================================================================
		void MedicalUILayer::OnAttach() {
			nk_uint32 W = mDevice ? mDevice->GetSwapchainWidth() : 1280;
			nk_uint32 H = mDevice ? mDevice->GetSwapchainHeight() : 720;

			if (!mCtx.Init((nk_int32)W, (nk_int32)H)) {
				logger.Errorf("[MedicalUILayer] NkGuiContext::Init échoué — UI désactivée\n");
				return;
			}
			nkgui::SetCurrentContext(&mCtx);

			// Police d'interface (embarquée, même famille que Nkoung/Mou).
			if (!mFont.LoadEmbedded(NkEmbeddedFontId::DroidSans, 13.f)) {
				logger.Errorf("[MedicalUILayer] Police DroidSans 13 non chargée — UI désactivée\n");
				mCtx.Shutdown();
				return;
			}
			mCtx.font = &mFont;
			mUIReady = true;

			// Câblage du RENDU (patron Nogee/UILayer, backend NKGui->NKRHI) :
			//  1. backend initialisé sur la render pass swapchain ;
			//  2. atlas de police uploadé (Gray8 -> RGBA8) ;
			//  3. callback enregistré dans la passe Overlay2D du render graph —
			//     Submit(mMerged) UNE fois par frame (cf. NkGuiRHIBackend.h).
			if (mDevice) {
				if (mBackend.Init(mDevice, mDevice->GetSwapchainRenderPass(), mApi)) {
					mBackendReady = true;
					if (mFont.Valid() && mFont.pixels)
						mBackend.UploadTextureGray8(mFont.TexId(), mFont.pixels, mFont.atlasW, mFont.atlasH);

					auto *renderer = NkApplication::Get().GetRenderer();
					if (renderer) {
						renderer->SetUIOverlayCallback([this](NkICommandBuffer *cmd) {
							if (!mBackendReady || !mDevice)
								return;
							mBackend.Submit(cmd, mMerged, mDevice->GetSwapchainWidth(),
											mDevice->GetSwapchainHeight());
						});
						mCallbackSet = true;
					} else {
						logger.Errorf("[MedicalUILayer] GetRenderer() nul — UI non soumise au render graph\n");
					}
				} else {
					logger.Errorf("[MedicalUILayer] NkGuiRHIBackend::Init échoué — UI non rendue\n");
				}
			}

			// Initialiser les panels depuis la BDD du moteur diagnostique
			if (mPatient)
				mSymptomPanel.Init(&mPatient->GetDiagnosticEngine());

			logger.Infof("[MedicalUILayer] v3 (NKGui) attaché {}x{} (backend RHI: {})\n", W, H, mBackendReady ? 1 : 0);
		}

		// =====================================================================
		void MedicalUILayer::ReleaseGpu() noexcept {
			// Désenregistrer le callback AVANT de détruire le backend (le render
			// graph ne doit plus nous appeler pendant/après la destruction).
			if (mCallbackSet) {
				if (auto *renderer = NkApplication::Get().GetRenderer())
					renderer->SetUIOverlayCallback(renderer::NkUIOverlayCallback{});
				mCallbackSet = false;
			}
			if (mDevice && mBackendReady)
				mDevice->WaitIdle();
			if (mBackendReady) {
				mBackend.Destroy();
				mBackendReady = false;
			}
			mPatientTexRegistered = false;
		}

		void MedicalUILayer::OnDetach() {
			ReleaseGpu();
			if (mUIReady) {
				mCtx.Shutdown();
				mUIReady = false;
			}
			logger.Infof("[MedicalUILayer] Détaché ({} frames UI)\n", mFrames);
		}

		void MedicalUILayer::OnUpdate(float dt) {
			mDt = dt;
		}

		// =====================================================================
		void MedicalUILayer::OnUIRender() {
			if (!mUIReady || !mDevice || !mPatient)
				return;
			nk_uint32 W = mDevice->GetSwapchainWidth();
			nk_uint32 H = mDevice->GetSwapchainHeight();
			if (!W || !H)
				return;

			nkgui::SetCurrentContext(&mCtx);
			mCtx.viewW = (nk_int32)W;
			mCtx.viewH = (nk_int32)H;

			ComputeLayout();

			mCtx.BeginFrame(mDt);

			RenderMenuBar();
			RenderViewport();

			// ── 4 panels ──────────────────────────────────────────────────────
			mSymptomPanel.Render(mCtx, *mPatient, mLayout.symptom);
			mDiagPanel.Render(mCtx, *mPatient, mLayout.diagnostic);
			mStatePanel.Render(mCtx, *mPatient, mLayout.state);
			mReportPanel.Render(mCtx, *mPatient, mLayout.report);

			mCtx.EndFrame();

			// Fusion dl + dlOverlay : le backend RHI n'accepte QU'UNE Submit par
			// frame (cf. NkGuiRHIBackend.h / NkEditorRHIRenderer, leçon du 13/08).
			mMerged.Reset();
			mMerged.Append(mCtx.dl);
			mMerged.Append(mCtx.dlOverlay);

			if (mFrames == 0)
				logger.Infof("[MedicalUILayer] première frame UI construite : {} sommets, {} commandes\n",
							 mMerged.vtx.Size(), mMerged.cmds.Size());
			++mFrames;
		}

		// =====================================================================
		bool MedicalUILayer::OnEvent(NkEvent *e) {
			if (e)
				UpdateInput(e);
			return false;
		}

		static void SetEditKey(NkGuiInput &in, NkKey k, bool down) noexcept {
			switch (k) {
				case NkKey::NK_ENTER:
				case NkKey::NK_NUMPAD_ENTER:
					in.SetKey(NkGuiKey::Enter, down);
					break;
				case NkKey::NK_ESCAPE:
					in.SetKey(NkGuiKey::Escape, down);
					break;
				case NkKey::NK_BACK:
					in.SetKey(NkGuiKey::Backspace, down);
					break;
				case NkKey::NK_DELETE:
					in.SetKey(NkGuiKey::Delete, down);
					break;
				case NkKey::NK_LEFT:
					in.SetKey(NkGuiKey::Left, down);
					break;
				case NkKey::NK_RIGHT:
					in.SetKey(NkGuiKey::Right, down);
					break;
				case NkKey::NK_UP:
					in.SetKey(NkGuiKey::Up, down);
					break;
				case NkKey::NK_DOWN:
					in.SetKey(NkGuiKey::Down, down);
					break;
				case NkKey::NK_HOME:
					in.SetKey(NkGuiKey::Home, down);
					break;
				case NkKey::NK_END:
					in.SetKey(NkGuiKey::End, down);
					break;
				case NkKey::NK_TAB:
					in.SetKey(NkGuiKey::Tab, down);
					break;
				default:
					break;
			}
		}

		// =====================================================================
		// Pont NKEvent -> NkGuiInput. Même table que NK3DModeler/main.cpp
		// (touches d'ÉDITION + drapeaux copier/coller + saisie texte) : c'est le
		// pont de référence — sans lui, les InputText ne reçoivent rien.
		void MedicalUILayer::UpdateInput(NkEvent *e) noexcept {
			auto &in = mCtx.input;
			if (auto *mm = e->As<NkMouseMoveEvent>()) {
				in.mousePos = {(float32)mm->GetX(), (float32)mm->GetY()};
			} else if (auto *mbp = e->As<NkMouseButtonPressEvent>()) {
				// NkMouseButtonEvent est une base abstraite — tester les feuilles.
				int btn = mbp->IsLeft() ? 0 : mbp->IsRight() ? 1 : 2;
				in.mouseDown[btn] = true;
			} else if (auto *mbr = e->As<NkMouseButtonReleaseEvent>()) {
				int btn = mbr->IsLeft() ? 0 : mbr->IsRight() ? 1 : 2;
				in.mouseDown[btn] = false;
			} else if (auto *dc = e->As<NkMouseDoubleClickEvent>()) {
				in.SetDoubleClick(dc->IsLeft() ? 0 : dc->IsRight() ? 1 : 2);
			} else if (auto *mwv = e->As<NkMouseWheelVerticalEvent>()) {
				// La molette s'ACCUMULE : plusieurs crans par frame possibles.
				// (NkMouseWheelEvent est une base sans GetStaticType : feuilles.)
				in.wheel += (float32)mwv->GetDeltaY();
			} else if (auto *mwh = e->As<NkMouseWheelHorizontalEvent>()) {
				in.wheelH += (float32)mwh->GetDeltaX();
			} else if (auto *te = e->As<NkTextInputEvent>()) {
				in.PushChar(te->GetCodepoint());
			} else if (auto *kp = e->As<NkKeyPressEvent>()) {
				const auto mods = kp->GetModifiers();
				in.ctrlDown = mods.ctrl;
				in.shiftDown = mods.shift;
				in.altDown = mods.alt;
				if (mods.ctrl) {
					const NkKey k = kp->GetKey();
					if (k == NkKey::NK_C)
						in.wantCopy = true;
					else if (k == NkKey::NK_X)
						in.wantCut = true;
					else if (k == NkKey::NK_V)
						in.wantPaste = true;
					else if (k == NkKey::NK_A)
						in.wantSelectAll = true;
				}
				SetEditKey(in, kp->GetKey(), true);
			} else if (auto *kr = e->As<NkKeyReleaseEvent>()) {
				SetEditKey(in, kr->GetKey(), false);
			}
		}


		// =====================================================================
		void MedicalUILayer::ComputeLayout() noexcept {
			float32 W = (float32)mCtx.viewW;
			float32 H = (float32)mCtx.viewH;
			float32 menuH = kMenuBarH;
			float32 botH = 260.f; // hauteur des panels bas
			float32 repH = 60.f;  // barre rapport (saisie + export, contenu défilable)
			float32 vpH = H - menuH - botH - repH;

			mLayout.menuBar = {0, 0, W, menuH};
			mLayout.viewport = {0, menuH, W, vpH};

			float32 panW = W / 3.f;
			float32 panY = menuH + vpH;

			mLayout.symptom = {0, panY, panW, botH};
			mLayout.diagnostic = {panW, panY, panW, botH};
			mLayout.state = {panW * 2, panY, panW, botH};
			mLayout.report = {0, panY + botH, W, repH};
		}

		// =====================================================================
		void MedicalUILayer::RenderMenuBar() noexcept {
			if (!BeginMenuBar(mCtx, mLayout.menuBar))
				return;

			// ── Menu Cas clinique ─────────────────────────────────────────────
			if (BeginMenu(mCtx, "Cas clinique")) {
				if (MenuItem(mCtx, "Nouveau cas")) {
				}
				if (MenuItem(mCtx, "Charger (.nkcase)")) {
				}
				Separator(mCtx);
				if (MenuItem(mCtx, "Réinitialiser")) {
					if (mPatient) {
						mPatient->ClearSymptoms();
						mPatient->SetVitalSigns(72.f, 37.f, 98.f);
					}
				}
				EndMenu(mCtx);
			}

			// ── Menu Patient ──────────────────────────────────────────────────
			if (BeginMenu(mCtx, "Patient")) {
				if (MenuItem(mCtx, "Neutre", "F1")) {
					if (mPatient)
						mPatient->ForceEmotion(EmotionState::Neutral);
				}
				if (MenuItem(mCtx, "Douleur légère", "F2")) {
					if (mPatient)
						mPatient->ForceEmotion(EmotionState::PainMild, 0.6f);
				}
				if (MenuItem(mCtx, "Douleur sévère", "F3")) {
					if (mPatient)
						mPatient->ForceEmotion(EmotionState::PainSevere, 1.f);
				}
				if (MenuItem(mCtx, "Anxieux", "F4")) {
					if (mPatient)
						mPatient->ForceEmotion(EmotionState::Anxious, 0.8f);
				}
				if (MenuItem(mCtx, "Panique", "F5")) {
					if (mPatient)
						mPatient->ForceEmotion(EmotionState::Panic, 1.f);
				}
				if (MenuItem(mCtx, "Épuisé", "F6")) {
					if (mPatient)
						mPatient->ForceEmotion(EmotionState::Exhausted, 0.9f);
				}
				EndMenu(mCtx);
			}

			// ── Menu Aide ─────────────────────────────────────────────────────
			if (BeginMenu(mCtx, "Aide")) {
				if (MenuItem(mCtx, "À propos de PV3DE")) {
				}
				EndMenu(mCtx);
			}

			EndMenuBar(mCtx);
		}

		// =====================================================================
		// Dessin DIRECT dans ctx.dl (pas de fenêtre : le viewport est le fond).
		// Texte : AddText prend la BASELINE (= topY + Ascent), piège n1 des
		// portages NKUI->NKGui (couvert par l'API, cf. carnet Nkoung).
		void MedicalUILayer::RenderViewport() noexcept {
			const auto &r = mLayout.viewport;
			const NkColor white = {255, 255, 255, 255};
			const float32 asc = mFont.Ascent();

			NkTextureHandle fbo = mPatient ? mPatient->GetPatientFBO() : NkTextureHandle{};
			if (fbo.IsValid() && mBackendReady) {
				// Texture GPU externe (possédée par PatientLayer) : enregistrée
				// une fois auprès du backend sous kPatientTexId.
				if (!mPatientTexRegistered)
					mPatientTexRegistered = mBackend.RegisterTexture(kPatientTexId, fbo);
				if (mPatientTexRegistered)
					mCtx.dl.AddImage(kPatientTexId, r, {0.f, 0.f}, {1.f, 1.f}, white);
			} else {
				// Placeholder (Phase 6 : rendu 3D du patient)
				mCtx.dl.AddRectFilled(r, {20, 20, 30, 255});
				const char *ph = "Patient 3D — en attente du rendu GPU";
				float32 w = mFont.MeasureWidth(ph);
				mCtx.dl.AddText(mFont.face, mFont.TexId(), {r.x + (r.w - w) * 0.5f, r.y + r.h * 0.5f + asc * 0.5f},
								ph, {80, 80, 80, 255});
			}

			// Overlay : état émotionnel sur le viewport
			if (mPatient) {
				const auto &em = mPatient->GetEmotionOutput();
				char stateBuf[64];
				snprintf(stateBuf, sizeof(stateBuf), "[%s]",
						 (em.state == EmotionState::Neutral)	  ? "Calme"
						 : (em.state == EmotionState::PainMild)	  ? "Douleur légère"
						 : (em.state == EmotionState::PainSevere) ? "DOULEUR SÉVÈRE"
						 : (em.state == EmotionState::Panic)	  ? "PANIQUE"
						 : (em.state == EmotionState::Anxious)	  ? "Anxieux"
						 : (em.state == EmotionState::Exhausted)  ? "Épuisé"
																  : "...");
				mCtx.dl.AddText(mFont.face, mFont.TexId(), {r.x + 8.f, r.y + 8.f + asc}, stateBuf,
								{220, 220, 80, 200});

				// Indicateur respiration
				const auto &cs = mPatient->GetClinicalState();
				char breathBuf[32];
				snprintf(breathBuf, sizeof(breathBuf), "FC:%.0f  SpO2:%.0f%%", cs.heartRate, cs.spo2);
				float32 bw = mFont.MeasureWidth(breathBuf);
				mCtx.dl.AddText(mFont.face, mFont.TexId(), {r.x + r.w - bw - 8.f, r.y + 8.f + asc}, breathBuf,
								{180, 220, 180, 200});
			}
		}

	} // namespace pv3de
} // namespace nkentseu
