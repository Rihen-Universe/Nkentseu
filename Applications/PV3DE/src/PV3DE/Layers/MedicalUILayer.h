#pragma once
// =============================================================================
// PV3DE/Layers/MedicalUILayer.h  —  v2
// =============================================================================
// Interface médecin complète avec les 4 panels :
//
//   ┌────────────────────────────────────────────────────────────┐
//   │                    Viewport 3D patient                     │
//   │                       (70% hauteur)                        │
//   ├──────────────┬───────────────────┬───────────────────────┤
//   │  Symptômes   │   Diagnostic diff │   État patient         │
//   │  (33%)       │   (33%)           │   (33%)                │
//   │              │                   │                        │
//   └──────────────┴──────────────────────────────────────────── ┘
//   │                   Rapport clinique (barre basse)           │
//   └────────────────────────────────────────────────────────────┘
// =============================================================================

#include "Nkentseu/Core/Layer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUILayout2.h"
#include "NKUI/NkUIMenu.h"
#include "PV3DE/Panels/SymptomInput.h"
#include "PV3DE/Panels/DiagnosticPanel.h"
#include "PV3DE/Panels/PatientStatePanel.h"
#include "PV3DE/Panels/ReportPanel.h"

namespace nkentseu {
    namespace pv3de {

        class PatientLayer;

        class MedicalUILayer : public nkentseu::Overlay {
        public:
            MedicalUILayer(const NkString& name,
                           NkIDevice* device,
                           NkICommandBuffer* cmd,
                           NkGraphicsApi api,
                           PatientLayer* patientLayer) noexcept;
            ~MedicalUILayer() override;

            void OnAttach()         override;
            void OnDetach()         override;
            void OnUpdate(float dt) override;
            void OnUIRender()       override;
            bool OnEvent(NkEvent*   e) override;

        private:
            void ComputeLayout() noexcept;
            void RenderMenuBar  () noexcept;
            void RenderViewport () noexcept;

            void UpdateInput(const NkEvent* e) noexcept;

            NkIDevice*        mDevice  = nullptr;
            NkICommandBuffer* mCmd     = nullptr;
            NkGraphicsApi     mApi;
            PatientLayer*     mPatient = nullptr;

            // NKUI
            nkui::NkUIContext       mCtx;
            nkui::NkUIWindowManager mWM;
            nkui::NkUILayoutStack   mLS;
            nkui::NkUIDrawList      mDL;

            // Panels
            SymptomInputPanel mSymptomPanel;
            DiagnosticPanel   mDiagPanel;
            PatientStatePanel mStatePanel;
            ReportPanel       mReportPanel;

            // Layout
            struct Layout {
                nkui::NkUIRect menuBar;
                nkui::NkUIRect viewport;
                nkui::NkUIRect symptom;
                nkui::NkUIRect diagnostic;
                nkui::NkUIRect state;
                nkui::NkUIRect report;
            } mLayout;

            nkui::NkUIInputState mInput;
            float32 mDt = 0.f;

            // Viewport 3D patient (FBO externe injecté)
            NkTextureHandle mPatientFBO;
            nk_uint32       mPatientFBOW = 0, mPatientFBOH = 0;
        };

    } // namespace pv3de
} // namespace nkentseu
