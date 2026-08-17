#pragma once

#include "Noge/Core/NkApplication.h"
#include "Noge/Core/NkApplicationConfig.h"
#include "Layers/PatientLayer.h"
#include "Layers/MedicalUILayer.h"

namespace nkentseu {
	namespace pv3de {

		// =====================================================================
		// PatientVirtualApp
		// Application principale du Patient Virtuel 3D Emotif.
		// Hérite de NkApplication, configure les layers PV3DE.
		// =====================================================================
		class PatientVirtualApp : public nkentseu::NkApplication {
			public:
				explicit PatientVirtualApp(const NkApplicationConfig &config);
				~PatientVirtualApp() override;

			protected:
				void OnInit() override;
				void OnStart() override;
				void OnUpdate(float dt) override;
				void OnRender() override;
				void OnUIRender() override;
				void OnShutdown() override;

			private:
				PatientLayer *mPatientLayer = nullptr;
				// UI médicale (Phase 5, branchée 2026-08-18 lors du portage NKGui).
				// Possédée par la LayerStack de NkApplication (delete au dtor) ;
				// OnShutdown appelle ReleaseGpu() AVANT la destruction du device.
				MedicalUILayer *mMedicalUI = nullptr;
				// TODO Phase 6 : ViewportLayer 3D (rendu GPU du patient)
		};

	} // namespace pv3de
} // namespace nkentseu
