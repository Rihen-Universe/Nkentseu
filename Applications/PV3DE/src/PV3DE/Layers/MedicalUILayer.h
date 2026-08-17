#pragma once
// =============================================================================
// PV3DE/Layers/MedicalUILayer.h  —  v3 (NKGui, 2026-08-18)
// =============================================================================
// Interface médecin complète avec les 4 panels :
//
//   ┌────────────────────────────────────────────────────────────┐
//   │  Barre de menus : Cas clinique · Patient · Aide            │
//   ├────────────────────────────────────────────────────────────┤
//   │                    Viewport 3D patient                     │
//   │                 (placeholder tant que Phase 6)             │
//   ├──────────────┬───────────────────┬───────────────────────┤
//   │  Symptômes   │   Diagnostic diff │   État patient         │
//   │  (33%)       │   (33%)           │   (33%)                │
//   ├──────────────┴───────────────────┴───────────────────────┤
//   │                   Rapport clinique (barre basse)           │
//   └────────────────────────────────────────────────────────────┘
//
// v2 -> v3 : NKUI (dépréciée, décision Rodolf 2026-08-16) remplacée par NKGui.
//   - un NkGuiContext + une NkGuiFont + un NkGuiRHIBackend (Integrations/NKGui),
//     soumis dans la passe Overlay2D du render graph via
//     NkRenderer::SetUIOverlayCallback (render pass active sur la sortie finale) ;
//   - les panneaux sont des BeginPanel/EndPanel NKGui positionnés par rect
//     (mise en page fixe, calculée chaque frame depuis la swapchain) ;
//   - l'entrée passe par ctx.input (souris, molette, touches d'édition, texte).
// La v2 (NKUI) n'a jamais été branchée (`PushOverlay` = TODO Phase 5, draw-list
// jamais soumise) : cette v3 est la PREMIÈRE fois que l'UI médicale s'affiche.
// =============================================================================

#include "Noge/Core/NkLayer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKGui/NKGui.h"
#include "NKGui/NkGuiRHIBackend.h" // Integrations/NKGui — backend NKGui -> NKRHI
#include "PV3DE/Panels/SymptomInput.h"
#include "PV3DE/Panels/DiagnosticPanel.h"
#include "PV3DE/Panels/PatientStatePanel.h"
#include "PV3DE/Panels/ReportPanel.h"

namespace nkentseu {
	namespace pv3de {

		class PatientLayer;

		class MedicalUILayer : public nkentseu::NkOverlay {
			public:
				MedicalUILayer(const NkString &name, NkIDevice *device, NkICommandBuffer *cmd, NkGraphicsApi api,
							   PatientLayer *patientLayer) noexcept;
				~MedicalUILayer() override;

				void OnAttach() override;
				void OnDetach() override;
				void OnUpdate(float dt) override;
				void OnUIRender() override;
				bool OnEvent(NkEvent *e) override;

				// Libère les ressources GPU (backend) et désenregistre le callback
				// de rendu. À appeler AVANT la destruction du device : la pile de
				// layers de NkApplication est détruite APRÈS le device, OnDetach
				// arriverait trop tard. PatientVirtualApp::OnShutdown l'appelle
				// (via PopOverlay). Idempotent.
				void ReleaseGpu() noexcept;

			private:
				void ComputeLayout() noexcept;
				void RenderMenuBar() noexcept;
				void RenderViewport() noexcept;

				void UpdateInput(NkEvent *e) noexcept;

				NkIDevice *mDevice = nullptr;
				NkICommandBuffer *mCmd = nullptr;
				NkGraphicsApi mApi;
				PatientLayer *mPatient = nullptr;

				// NKGui
				nkgui::NkGuiContext mCtx;
				nkgui::NkGuiFont mFont;
				nkgui::NkGuiRHIBackend mBackend;
				nkgui::NkGuiDrawList mMerged; ///< dl + dlOverlay fusionnées (Submit = 1 fois/frame)
				bool mUIReady = false;
				bool mBackendReady = false;
				bool mCallbackSet = false;

				// Panels
				SymptomInputPanel mSymptomPanel;
				DiagnosticPanel mDiagPanel;
				PatientStatePanel mStatePanel;
				ReportPanel mReportPanel;

				// Layout
				struct Layout {
						nkgui::NkRect menuBar;
						nkgui::NkRect viewport;
						nkgui::NkRect symptom;
						nkgui::NkRect diagnostic;
						nkgui::NkRect state;
						nkgui::NkRect report;
				} mLayout;

				float32 mDt = 0.f;
				nk_uint32 mFrames = 0; ///< frames UI construites (journal du premier affichage)
				static constexpr float32 kMenuBarH = 26.f; ///< hauteur de la barre de menus

				// Viewport 3D patient (texture externe de PatientLayer::GetPatientFBO,
				// enregistrée auprès du backend sous kPatientTexId)
				static constexpr uint32 kPatientTexId = 0x50563344u; // 'PV3D'
				bool mPatientTexRegistered = false;
		};

	} // namespace pv3de
} // namespace nkentseu
