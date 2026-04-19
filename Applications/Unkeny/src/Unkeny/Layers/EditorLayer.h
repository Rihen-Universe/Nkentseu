#pragma once
// =============================================================================
// Unkeny/Layers/EditorLayer.h  —  v2
// =============================================================================
// Orchestre tous les systèmes éditeur.
// N'effectue aucun rendu direct — délègue à ViewportLayer et UILayer.
//
// Systèmes hébergés :
//   NkSelectionManager — entité sélectionnée, multi-sélection
//   CommandHistory     — undo/redo
//   ProjectManager     — .nkproj save/load
//   AssetManager       — cache d'assets + hot-reload
//   NkGizmoSystem      — gizmos 3D dans le viewport
// =============================================================================

#include "Nkentseu/Core/Layer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "Nkentseu/Core/EventBus.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "../Editor/NkSelectionManager.h"
#include "../Editor/NkEditorCamera.h"
#include "../Editor/NkGizmoSystem.h"
#include "../Editor/CommandHistory.h"
#include "../Editor/ProjectManager.h"
#include "../Editor/AssetManager.h"
#include "NKECS/World/NkWorld.h"
#include "NKECS/Scene/NkSceneGraph.h"

namespace nkentseu {
    namespace Unkeny {

        class EditorLayer : public Layer {
        public:
            EditorLayer(const NkString& name,
                        NkIDevice* device,
                        NkICommandBuffer* cmd) noexcept;
            ~EditorLayer() override;

            void OnAttach()               override;
            void OnDetach()               override;
            void OnUpdate(float dt)       override;
            void OnFixedUpdate(float fdt) override;
            bool OnEvent(NkEvent* event)  override;

            // ── Accesseurs pour UILayer / ViewportLayer ───────────────────────
            NkSelectionManager& GetSelectionManager() noexcept { return mSel; }
            CommandHistory&     GetHistory()          noexcept { return mHistory; }
            ProjectManager&     GetProjectManager()   noexcept { return mProject; }
            AssetManager&       GetAssetManager()     noexcept { return mAssets; }
            NkGizmoSystem&      GetGizmoSystem()      noexcept { return mGizmos; }

            // ── Connexion à la scène active ───────────────────────────────────
            void SetScene(ecs::NkWorld* world, ecs::NkSceneGraph* scene) noexcept {
                mWorld = world;
                mScene = scene;
            }

            // ── Mode play/pause éditeur ───────────────────────────────────────
            bool IsPlaying()  const noexcept { return mPlaying; }
            void Play()  noexcept;
            void Stop()  noexcept;
            void Pause() noexcept;

            // ── Raccourcis clavier ────────────────────────────────────────────
            // W/E/R → mode gizmo Translate/Rotate/Scale
            // Ctrl+Z → Undo, Ctrl+Y → Redo
            // Ctrl+S → Save projet
            // F5     → Play/Stop
            // Delete → Supprimer l'entité sélectionnée

        private:
            void HandleShortcuts(const NkKeyPressEvent* kp) noexcept;
            void DeleteSelectedEntity() noexcept;

            NkIDevice*        mDevice = nullptr;
            NkICommandBuffer* mCmd    = nullptr;

            NkSelectionManager mSel;
            CommandHistory     mHistory;
            ProjectManager     mProject;
            AssetManager       mAssets;
            NkGizmoSystem      mGizmos;

            ecs::NkWorld*     mWorld  = nullptr;
            ecs::NkSceneGraph* mScene = nullptr;

            bool mPlaying = false;
            float mHotReloadTimer = 0.f;

            // Souscriptions EventBus
            NkEventHandlerId mSelChangedId = NK_INVALID_HANDLER_ID;
        };

    } // namespace Unkeny
} // namespace nkentseu
