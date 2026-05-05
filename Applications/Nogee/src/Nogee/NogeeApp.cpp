#include "NogeApp.h"
#include "NKLogger/NkLog.h"
#include "Nkentseu/ECS/Scene/NkSceneManager.h"

namespace nkentseu {
    namespace noge {

        NogeApp::NogeApp(const NogeAppConfig& config)
            : Application(config.appConfig), mUkConfig(config) {}

        NogeApp::~NogeApp() = default;

        // =====================================================================
        void NogeApp::OnInit() {
            logger.Infof("[NogeApp] Init — {}\n", mUkConfig.windowTitle.CStr());

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
            // → on crée une caméra partagée propriété de NogeApp
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
                    logger.Infof("[NogeApp] Projet chargé: {}\n", mUkConfig.startupProjectPath.CStr());
                }
            }
        }

        // =====================================================================
        void NogeApp::OnStart() {
            // Scène de démonstration vide si aucun projet
            // (Une vraie scène est chargée par NkSceneManager depuis OnInit/projet)
        }

        void NogeApp::OnUpdate(float dt) {
            // Les layers ont leur propre OnUpdate dans la LayerStack
            (void)dt;
        }

        void NogeApp::OnRender() {
            // ViewportLayer::OnRender() gère le FBO
        }

        void NogeApp::OnUIRender() {
            // UILayer::OnUIRender() gère NKUI
        }

        void NogeApp::OnShutdown() {
            delete mEditorCamera;
            mEditorCamera = nullptr;
            logger.Infof("[NogeApp] Shutdown\n");
        }

        void NogeApp::OnClose() {
            // Auto-save si modifié
            if (mEditorLayer && mEditorLayer->GetProjectManager().IsModified()) {
                mEditorLayer->GetProjectManager().Save();
            }
            Quit();
        }

        void NogeApp::OnResize(nk_uint32 w, nk_uint32 h) {
            // Le FBO se redimensionne automatiquement via UILayer::ComputeLayout()
            (void)w; (void)h;
        }

    } // namespace Unkeny
} // namespace nkentseu
