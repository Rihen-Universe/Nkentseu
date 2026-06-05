#pragma once
// =============================================================================
// Noge/Layers/UILayer.h  —  v2
// =============================================================================
// Overlay NKUI de l'éditeur. Intègre :
//   - MenuBar fonctionnel (Fichier, Édition, Affichage, Aide)
//   - ViewportPanel  : affiche la texture FBO + transmet l'état souris
//   - SceneTreePanel : hiérarchie ECS dockable à gauche
//   - InspectorPanel : composants à droite
//   - AssetBrowser   : en bas à gauche
//   - ConsolePanel   : en bas à droite
//
// Layout fixe (peut devenir dockable Phase 5+) :
//
//   ┌─────────────────────────────────────────────┐
//   │                  MenuBar (25px)             │
//   ├──────────┬──────────────────────┬───────────┤
//   │ SceneTree│      Viewport        │ Inspector │
//   │  (22%)   │       (56%)          │  (22%)    │
//   ├──────────┴──────────────────────┴───────────┤
//   │      AssetBrowser (60%)  │  Console (40%)   │
//   │           (35% height)                      │
//   └─────────────────────────────────────────────┘
// =============================================================================

#include "Nkentseu/Core/Layer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUILayout2.h"
#include "NKUI/NkUIMenu.h"
#include "Noge/Panels/SceneTreePanel.h"
#include "Noge/Panels/InspectorPanel.h"
#include "Noge/Panels/AssetBrowser.h"
#include "Noge/Panels/ConsolePanel.h"
#include "Noge/Layers/EditorLayer.h"
#include "Noge/Layers/ViewportLayer.h"

namespace nkentseu {
    namespace noge {

        class UILayer : public Overlay {
        public:
            UILayer(const NkString& name,
                    NkIDevice* device,
                    NkICommandBuffer* cmd,
                    NkGraphicsApi api) noexcept;
            ~UILayer() override;

            void OnAttach()          override;
            void OnDetach()          override;
            void OnUpdate(float dt)  override;
            void OnRender()          override;
            void OnUIRender()        override;
            bool OnEvent(NkEvent*)   override;

            // ── Injections depuis NogeApp ───────────────────────────────────
            void SetEditorLayer  (EditorLayer*   el) noexcept { mEditorLayer   = el; }
            void SetViewportLayer(ViewportLayer* vl) noexcept { mViewportLayer = vl; }
            void SetWorld        (ecs::NkWorld*  w)  noexcept { mWorld         = w;  }
            void SetScene        (ecs::NkSceneGraph* s) noexcept { mScene      = s;  }

        private:
            // ── Rendu des sections ────────────────────────────────────────────
            void RenderMenuBar    () noexcept;
            void RenderViewport   () noexcept;
            void RenderSceneTree  () noexcept;
            void RenderInspector  () noexcept;
            void RenderAssetBrowser() noexcept;
            void RenderConsole    () noexcept;

            // ── Input bridge ──────────────────────────────────────────────────
            void UpdateInputState(const NkEvent* event) noexcept;
            nkui::NkUIInputState BuildInputState() const noexcept;

            // ── Layout helpers ────────────────────────────────────────────────
            void ComputeLayout() noexcept;

            NkIDevice*        mDevice = nullptr;
            NkICommandBuffer* mCmd    = nullptr;
            NkGraphicsApi     mApi    = NkGraphicsApi::NK_GFX_API_OPENGL;

            // NKUI
            nkui::NkUIContext      mCtx;
            nkui::NkUIWindowManager mWM;
            nkui::NkUIDockManager  mDock;
            nkui::NkUILayoutStack  mLS;
            nkui::NkUIDrawList     mDL;

            // Panels
            SceneTreePanel  mSceneTree;
            InspectorPanel  mInspector;
            AssetBrowser    mAssetBrowser;
            ConsolePanel    mConsole;

            // Connexions
            EditorLayer*     mEditorLayer   = nullptr;
            ViewportLayer*   mViewportLayer = nullptr;
            ecs::NkWorld*    mWorld         = nullptr;
            ecs::NkSceneGraph* mScene       = nullptr;

            // Rects calculés chaque frame
            struct Layout {
                nkui::NkUIRect menuBar;
                nkui::NkUIRect viewport;
                nkui::NkUIRect sceneTree;
                nkui::NkUIRect inspector;
                nkui::NkUIRect assetBrowser;
                nkui::NkUIRect console;
            } mLayout;

            // Input state accumulé
            nkui::NkUIInputState mInput;
            float32 mPrevMouseX = 0.f, mPrevMouseY = 0.f;

            // Visibilité des panels
            bool mShowSceneTree   = true;
            bool mShowInspector   = true;
            bool mShowAssetBrowser= true;
            bool mShowConsole     = true;
        };

    } // namespace noge
} // namespace nkentseu
