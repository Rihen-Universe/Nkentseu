#include "MedicalUILayer.h"
#include "PatientLayer.h"
#include "NKLogger/NkLog.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkTextEvent.h"

namespace nkentseu {
    namespace pv3de {

        using namespace nkui;

        MedicalUILayer::MedicalUILayer(const NkString& name,
                                       NkIDevice* device,
                                       NkICommandBuffer* cmd,
                                       NkGraphicsApi api,
                                       PatientLayer* patient) noexcept
            : Overlay(name)
            , mDevice(device), mCmd(cmd), mApi(api), mPatient(patient) {}

        MedicalUILayer::~MedicalUILayer() = default;

        // =====================================================================
        void MedicalUILayer::OnAttach() {
            nk_uint32 W = mDevice ? mDevice->GetSwapchainWidth()  : 1280;
            nk_uint32 H = mDevice ? mDevice->GetSwapchainHeight() : 720;

            NkUIFontConfig fc;
            fc.defaultFontSize = 13.f;
            fc.enableBitmapFallback = true;
            if (!mCtx.Init((nk_int32)W, (nk_int32)H, fc)) {
                logger.Errorf("[MedicalUILayer] NkUIContext::Init échoué\n");
                return;
            }
            mCtx.SetTheme(NkUITheme::Dark());
            mWM.Init();
            mDL.Init();

            // Initialiser les panels depuis la BDD du moteur diagnostique
            if (mPatient)
                mSymptomPanel.Init(&mPatient->GetDiagnosticEngine());

            logger.Infof("[MedicalUILayer] v2 attaché {}x{}\n", W, H);
        }

        void MedicalUILayer::OnDetach() {
            mDL.Destroy();
            mCtx.Destroy();
            mWM.Destroy();
        }

        void MedicalUILayer::OnUpdate(float dt) { mDt = dt; }

        // =====================================================================
        void MedicalUILayer::OnUIRender() {
            if (!mDevice || !mPatient) return;
            nk_uint32 W = mDevice->GetSwapchainWidth();
            nk_uint32 H = mDevice->GetSwapchainHeight();
            if (!W || !H) return;

            mCtx.viewW = (nk_int32)W;
            mCtx.viewH = (nk_int32)H;
            mInput.dt  = mDt;

            ComputeLayout();

            mCtx.BeginFrame(mInput, mDt);
            mWM.BeginFrame(mCtx);
            mInput.BeginFrame();
            mLS.depth = 0;
            mDL.Reset();

            RenderMenuBar();
            RenderViewport();

            NkUIFont& font = *mCtx.fontManager.GetDefault();

            // ── 4 panels du bas ───────────────────────────────────────────────
            mSymptomPanel.Render(mCtx, mWM, mDL, font, mLS,
                                  *mPatient, mLayout.symptom);

            mDiagPanel.Render(mCtx, mWM, mDL, font, mLS,
                               *mPatient, mLayout.diagnostic);

            mStatePanel.Render(mCtx, mWM, mDL, font, mLS,
                                *mPatient, mLayout.state);

            mReportPanel.Render(mCtx, mWM, mDL, font, mLS,
                                 *mPatient, mLayout.report);

            mCtx.EndFrame();
            mWM.EndFrame(mCtx);
        }

        // =====================================================================
        bool MedicalUILayer::OnEvent(NkEvent* e) {
            if (e) UpdateInput(e);
            return false;
        }

        // =====================================================================
        void MedicalUILayer::UpdateInput(const NkEvent* e) noexcept {
            if (auto* mm = e->As<NkMouseMoveEvent>()) {
                mInput.SetMousePos((float32)mm->GetX(), (float32)mm->GetY());
            } else if (auto* mb = e->As<NkMouseButtonEvent>()) {
                int btn = (mb->GetButton() == NkMouseButton::NK_LEFT)  ? 0
                        : (mb->GetButton() == NkMouseButton::NK_RIGHT) ? 1 : 2;
                mInput.SetMouseButton(btn, mb->IsPressed());
            } else if (auto* kp = e->As<NkKeyPressEvent>()) {
                mInput.SetKey(static_cast<NkKey>(kp->GetKey()), true);
            } else if (auto* kr = e->As<NkKeyReleaseEvent>()) {
                mInput.SetKey(static_cast<NkKey>(kr->GetKey()), false);
            } else if (auto* te = e->As<NkTextEvent>()) {
                mInput.AddInputChar(te->GetCodepoint());
            }
        }

        // =====================================================================
        void MedicalUILayer::ComputeLayout() noexcept {
            float32 W    = (float32)mCtx.viewW;
            float32 H    = (float32)mCtx.viewH;
            float32 menuH= mCtx.theme.metrics.titleBarHeight;
            float32 botH = 260.f;   // hauteur des panels bas
            float32 repH = 30.f;    // barre rapport (boutons export)
            float32 vpH  = H - menuH - botH - repH;

            mLayout.menuBar    = {0,     0,         W,       menuH};
            mLayout.viewport   = {0,     menuH,     W,       vpH};

            float32 panW = W / 3.f;
            float32 panY = menuH + vpH;

            mLayout.symptom    = {0,       panY, panW,    botH};
            mLayout.diagnostic = {panW,    panY, panW,    botH};
            mLayout.state      = {panW*2,  panY, panW,    botH};
            mLayout.report     = {0,       panY + botH, W, repH + 30.f};
        }

        // =====================================================================
        void MedicalUILayer::RenderMenuBar() noexcept {
            NkUIFont& font = *mCtx.fontManager.GetDefault();
            if (!NkUIMenu::BeginMenuBar(mCtx, mDL, font, mLayout.menuBar)) return;

            // ── Menu Cas clinique ─────────────────────────────────────────────
            if (NkUIMenu::BeginMenu(mCtx, mDL, font, "Cas clinique")) {
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Nouveau cas"))   {}
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Charger (.nkcase)")) {}
                NkUIMenu::Separator(mCtx, mDL);
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Réinitialiser")) {
                    if (mPatient) {
                        mPatient->ClearSymptoms();
                        mPatient->SetVitalSigns(72.f, 37.f, 98.f);
                    }
                }
                NkUIMenu::EndMenu(mCtx);
            }

            // ── Menu Patient ──────────────────────────────────────────────────
            if (NkUIMenu::BeginMenu(mCtx, mDL, font, "Patient")) {
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Neutre",
                                       "F1")) {
                    if (mPatient) mPatient->ForceEmotion(EmotionState::Neutral);
                }
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Douleur légère",  "F2"))
                    { if (mPatient) mPatient->ForceEmotion(EmotionState::PainMild, 0.6f); }
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Douleur sévère",  "F3"))
                    { if (mPatient) mPatient->ForceEmotion(EmotionState::PainSevere, 1.f); }
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Anxieux",         "F4"))
                    { if (mPatient) mPatient->ForceEmotion(EmotionState::Anxious, 0.8f); }
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Panique",         "F5"))
                    { if (mPatient) mPatient->ForceEmotion(EmotionState::Panic, 1.f); }
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "Épuisé",          "F6"))
                    { if (mPatient) mPatient->ForceEmotion(EmotionState::Exhausted, 0.9f); }
                NkUIMenu::EndMenu(mCtx);
            }

            // ── Menu Aide ─────────────────────────────────────────────────────
            if (NkUIMenu::BeginMenu(mCtx, mDL, font, "Aide")) {
                if (NkUIMenu::MenuItem(mCtx, mDL, font, "À propos de PV3DE")) {}
                NkUIMenu::EndMenu(mCtx);
            }

            NkUIMenu::EndMenuBar(mCtx);
        }

        // =====================================================================
        void MedicalUILayer::RenderViewport() noexcept {
            auto& r = mLayout.viewport;
            NkUIFont& font = *mCtx.fontManager.GetDefault();

            NkUIWindow::SetNextWindowPos ({r.x, r.y});
            NkUIWindow::SetNextWindowSize({r.w, r.h});

            if (!NkUIWindow::Begin(mCtx, mWM, mDL, font, mLS,
                                   "PatientView##pv",
                                   nullptr,
                                   NkUIWindowFlags::NK_NO_TITLE_BAR |
                                   NkUIWindowFlags::NK_NO_RESIZE    |
                                   NkUIWindowFlags::NK_NO_MOVE      |
                                   NkUIWindowFlags::NK_NO_BACKGROUND)) {
                NkUIWindow::End(mCtx, mWM, mDL, mLS);
                return;
            }

            if (mPatientFBO.IsValid()) {
                mDL.AddImage((nk_uint32)mPatientFBO.id,
                             {r.x, r.y, r.w, r.h},
                             {0,0}, {1,1}, NkColor::White);
            } else {
                // Placeholder
                mDL.AddRectFilled({r.x, r.y, r.w, r.h}, {20,20,30,255});
                mDL.AddText({r.x + r.w*0.5f - 100.f, r.y + r.h*0.5f},
                            "Patient 3D — en attente du rendu GPU",
                            {80, 80, 80, 255});
            }

            // Overlay : état émotionnel sur le viewport
            if (mPatient) {
                const auto& em = mPatient->GetEmotionOutput();
                char stateBuf[64];
                snprintf(stateBuf, sizeof(stateBuf), "[%s]",
                         (em.state == EmotionState::Neutral)    ? "Calme"         :
                         (em.state == EmotionState::PainMild)   ? "Douleur légère":
                         (em.state == EmotionState::PainSevere) ? "DOULEUR SÉVÈRE":
                         (em.state == EmotionState::Panic)      ? "PANIQUE"       :
                         (em.state == EmotionState::Anxious)    ? "Anxieux"       :
                         (em.state == EmotionState::Exhausted)  ? "Épuisé"        : "...");
                mDL.AddText({r.x + 8.f, r.y + 8.f}, stateBuf, {220,220,80,200});

                // Indicateur respiration
                const auto& cs = mPatient->GetClinicalState();
                char breathBuf[32];
                snprintf(breathBuf, sizeof(breathBuf), "FC:%.0f  SpO2:%.0f%%",
                         cs.heartRate, cs.spo2);
                mDL.AddText({r.x + r.w - 120.f, r.y + 8.f},
                            breathBuf, {180,220,180,200});
            }

            NkUIWindow::End(mCtx, mWM, mDL, mLS);
        }

    } // namespace pv3de
} // namespace nkentseu
