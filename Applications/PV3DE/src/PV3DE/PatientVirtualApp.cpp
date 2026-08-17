#include "PatientVirtualApp.h"
#include "Noge/Core/NkApplication.h"
#include "NKWindow/NKMain.h"
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace pv3de {

		PatientVirtualApp::PatientVirtualApp(const NkApplicationConfig &config) : NkApplication(config) {
		}

		PatientVirtualApp::~PatientVirtualApp() = default;

		void PatientVirtualApp::OnInit() {
			logger.Infof("[PV3DE] OnInit\n");

			mPatientLayer = new PatientLayer("PatientLayer", mDevice, mCmd);
			PushLayer(mPatientLayer);

			// Phase 5 (2026-08-18) : UI médicale NKGui branchée en overlay —
			// première fois que la couche s'affiche (la v2 NKUI n'a jamais été
			// poussée sur la pile).
			mMedicalUI = new MedicalUILayer("MedicalUILayer", mDevice, mCmd, GetConfig().deviceInfo.api,
											mPatientLayer);
			PushOverlay(mMedicalUI);

			// TODO Phase 6 : PushLayer(new ViewportLayer(...)) — rendu 3D patient.
		}

		void PatientVirtualApp::OnStart() {
			logger.Infof("[PV3DE] Démarrage — Patient Virtuel 3D Emotif v0.1\n");
			logger.Infof("[PV3DE] Cas démo : douleur thoracique + dyspnée\n");
		}

		void PatientVirtualApp::OnUpdate(float dt) {
			(void)dt;
			// Log périodique de l'état (debug Phase 1)
			static float logTimer = 0.f;
			logTimer += dt;
			if (logTimer >= 3.f) {
				logTimer = 0.f;
				const auto &cs = mPatientLayer->GetClinicalState();
				const auto &eo = mPatientLayer->GetEmotionOutput();
				logger.Infof("[PV3DE] Douleur={:.1f}/10 Anx={:.2f} État={}\n", cs.painLevel, cs.anxietyLevel,
							 static_cast<int>(eo.state));
				if (!cs.differentialRanking.IsEmpty()) {
					logger.Infof("[PV3DE] Top diag: {} ({:.0f}%)\n", cs.differentialRanking[0].diseaseName.CStr(),
								 cs.differentialRanking[0].probability * 100.f);
				}
			}
		}

		void PatientVirtualApp::OnRender() {
			// TODO Phase 6 : soumettre le rendu 3D du patient via mCmd
		}

		void PatientVirtualApp::OnUIRender() {
			// L'UI médicale est un overlay de la LayerStack : son OnUIRender est
			// appelé par la boucle NkApplication, rien à faire ici.
		}

		void PatientVirtualApp::OnShutdown() {
			// La LayerStack est détruite APRÈS le device (membre de NkApplication,
			// détruit au dtor) : libérer les ressources GPU de l'UI maintenant,
			// pendant que le device existe encore. Idempotent.
			if (mMedicalUI)
				mMedicalUI->ReleaseGpu();
			logger.Infof("[PV3DE] Shutdown\n");
		}

	} // namespace pv3de
} // namespace nkentseu

// =============================================================================
// nkmain — point d'entrée natif du framework (appelé par WinMain/EntryPoints,
// cf. NKWindow/Core/NkMain.h). Même pattern que Applications/Nogee/Nogee.cpp
// (Phase R0 2026-07-25) : construction directe de NkApplicationConfig depuis
// NkEntryState — l'ancien contrat libre `nkentseu::CreateApplication(config)`
// n'existe plus côté framework (classe Application supprimée au profit de
// NkApplication), ce n'était qu'un renommage de surface non détecté par
// l'audit initial (le fichier échouait avant sur les includes Noge/*).
// =============================================================================
int nkmain(const nkentseu::NkEntryState &state) {
	using namespace nkentseu;

	NkApplicationConfig config(state);

	config.appName = "PatientVirtuel3D";
	config.appVersion = "0.1.0";
	config.logLevel = NkLogLevel::NK_DEBUG;

	config.windowConfig.title = "Patient Virtuel 3D Emotif — Diagnostic Assistant";
	config.windowConfig.width = 1280;
	config.windowConfig.height = 720;
	config.windowConfig.centered = true;
	config.windowConfig.resizable = true;

	config.deviceInfo.api = NkGraphicsApi::NK_GFX_API_OPENGL;
	config.deviceInfo.context.vulkan.appName = "PV3DE";
	config.deviceInfo.context.vulkan.engineName = "Nkentseu";

	NkShaderCache::Global().SetCacheDir("Build/ShaderCache");

	pv3de::PatientVirtualApp *app = new pv3de::PatientVirtualApp(config);
	if (!app->Init()) {
		logger.Error("[PV3DE] Erreur d'initialisation de l'application");
		delete app;
		return 2;
	}
	app->Run();
	delete app;
	return 0;
}
