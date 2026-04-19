#include "UnkenyApp.h"
#include "NKLogger/NkLog.h"
#include "Nkentseu/ECS/Scene/NkSceneManager.h"

namespace nkentseu {
    namespace Unkeny {

        UnkenyApp::UnkenyApp(const UnkenyAppConfig& config)
            : Application(config.appConfig), mUkConfig(config) {}

        UnkenyApp::~UnkenyApp() = default;

        // =====================================================================
        void UnkenyApp::OnInit() {
            logger.Infof("[UnkenyApp] Init — {}\n", mUkConfig.windowTitle.CStr());

            NkIDevice*        dev = GetDevice();
            NkICommandBuffer* cmd = GetCmd();
            NkGraphicsApi     api = GetConfig().deviceInfo.api;

            // ── Créer les layers dans l'ordre ─────────────────────────────────
            // 1. EditorLayer : systèmes éditeur (sélection, undo, assets, gizmos)
            auto* editor   = new EditorLayer("EditorLayer",   dev, cmd);
            // 2. ViewportLayer : FBO offscreen + rendu scène
            auto* viewport = new ViewportLayer("ViewportLayer", dev, cmd);
            // 3. UILayer (Overlay) : NKUI par-dessus tout
            auto* ui       = new UILayer("UILayer", dev, cmd, api);

            // ── Connexions inter-layers ───────────────────────────────────────
            // ViewportLayer reçoit les systèmes éditeur
            viewport->SetEditorCamera    (&editor->GetGizmoSystem()==nullptr
                                           ? nullptr  // recalculé dessous
                                           : nullptr);

            // On connecte directement les pointeurs après empilement
            // (les layers vivent dans LayerStack — ownership)
            mEditorLayer   = editor;
            mViewportLayer = viewport;
            mUILayer       = ui;

            // Injecter la caméra éditeur dans ViewportLayer
            // La caméra est dans UILayer car elle lit l'input NKUI
            // → on crée une caméra partagée propriété de UnkenyApp
            mEditorCamera = new NkEditorCamera();
            viewport->SetEditorCamera    (mEditorCamera);
            viewport->SetGizmoSystem     (&editor->GetGizmoSystem());
            viewport->SetSelectionManager(&editor->GetSelectionManager());

            // UILayer reçoit toutes les références
            ui->SetEditorLayer  (editor);
            ui->SetViewportLayer(viewport);

            // ── Empilement ────────────────────────────────────────────────────
            PushLayer(editor);
            PushLayer(viewport);
            PushOverlay(ui);

            // ── Projet de démarrage ───────────────────────────────────────────
            if (!mUkConfig.startupProjectPath.IsEmpty()) {
                if (editor->GetProjectManager().Load(mUkConfig.startupProjectPath.CStr())) {
                    logger.Infof("[UnkenyApp] Projet chargé: {}\n", mUkConfig.startupProjectPath.CStr());
                }
            }
        }

        // =====================================================================
        void UnkenyApp::OnStart() {
            // Scène de démonstration vide si aucun projet
            // (Une vraie scène est chargée par NkSceneManager depuis OnInit/projet)
        }

        void UnkenyApp::OnUpdate(float dt) {
            // Les layers ont leur propre OnUpdate dans la LayerStack
            (void)dt;
        }

        void UnkenyApp::OnRender() {
            // ViewportLayer::OnRender() gère le FBO
        }

        void UnkenyApp::OnUIRender() {
            // UILayer::OnUIRender() gère NKUI
        }

        void UnkenyApp::OnShutdown() {
            delete mEditorCamera;
            mEditorCamera = nullptr;
            logger.Infof("[UnkenyApp] Shutdown\n");
        }

        void UnkenyApp::OnClose() {
            // Auto-save si modifié
            if (mEditorLayer && mEditorLayer->GetProjectManager().IsModified()) {
                mEditorLayer->GetProjectManager().Save();
            }
            Quit();
        }

        void UnkenyApp::OnResize(nk_uint32 w, nk_uint32 h) {
            // Le FBO se redimensionne automatiquement via UILayer::ComputeLayout()
            (void)w; (void)h;
        }

    } // namespace Unkeny
} // namespace nkentseu
