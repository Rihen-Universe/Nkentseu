#include "PatientVirtualApp.h"
#include "Nkentseu/Core/Application.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace pv3de {

        PatientVirtualApp::PatientVirtualApp(const NkApplicationConfig& config)
            : Application(config) {}

        PatientVirtualApp::~PatientVirtualApp() = default;

        void PatientVirtualApp::OnInit() {
            logger.Infof("[PV3DE] OnInit\n");

            mPatientLayer = new PatientLayer("PatientLayer", mDevice, mCmd);
            PushLayer(mPatientLayer);

            // TODO Phase 5 : PushLayer(new ViewportLayer(...));
            // TODO Phase 5 : PushOverlay(new MedicalUILayer(...));
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
                const auto& cs = mPatientLayer->GetClinicalState();
                const auto& eo = mPatientLayer->GetEmotionOutput();
                logger.Infof("[PV3DE] Douleur={:.1f}/10 Anx={:.2f} État={}\n",
                             cs.painLevel, cs.anxietyLevel,
                             static_cast<int>(eo.state));
                if (!cs.differentialRanking.IsEmpty()) {
                    logger.Infof("[PV3DE] Top diag: {} ({:.0f}%)\n",
                                 cs.differentialRanking[0].diseaseName.CStr(),
                                 cs.differentialRanking[0].probability * 100.f);
                }
            }
        }

        void PatientVirtualApp::OnRender() {
            // TODO Phase 6 : soumettre le rendu 3D du patient via mCmd
        }

        void PatientVirtualApp::OnUIRender() {
            // TODO Phase 5 : MedicalUILayer
        }

        void PatientVirtualApp::OnShutdown() {
            logger.Infof("[PV3DE] Shutdown\n");
        }

    } // namespace pv3de
} // namespace nkentseu

// =============================================================================
// CreateApplication — point d'entrée du framework
// =============================================================================
nkentseu::Application* nkentseu::CreateApplication(
    const nkentseu::NkApplicationConfig& baseConfig)
{
    using namespace nkentseu;

    NkApplicationConfig config = baseConfig;

    config.appName    = "PatientVirtuel3D";
    config.appVersion = "0.1.0";
    config.logLevel   = NkLogLevel::NK_DEBUG;

    config.windowConfig.title     = "Patient Virtuel 3D Emotif — Diagnostic Assistant";
    config.windowConfig.width     = 1280;
    config.windowConfig.height    = 720;
    config.windowConfig.centered  = true;
    config.windowConfig.resizable = true;

    config.deviceInfo.api = NkGraphicsApi::NK_API_OPENGL;
    config.deviceInfo.context.vulkan.appName    = "PV3DE";
    config.deviceInfo.context.vulkan.engineName = "Nkentseu";

    NkShaderCache::Global().SetCacheDir("Build/ShaderCache");

    return new pv3de::PatientVirtualApp(config);
}
