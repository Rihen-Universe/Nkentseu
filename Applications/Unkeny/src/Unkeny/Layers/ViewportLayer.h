#pragma once
// =============================================================================
// Unkeny/Layers/ViewportLayer.h  —  v2
// =============================================================================
// Rend la scène ECS dans un FBO offscreen.
// Se connecte à NkWorld via NkRenderSystem pour produire les draw calls.
// Intègre NkEditorCamera et NkGizmoSystem.
// Resize automatique quand la fenêtre NKUI du viewport change de taille.
// =============================================================================

#include "Nkentseu/Core/Layer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkHandles.h"
#include "NKMath/NKMath.h"
#include "NKECS/World/NkWorld.h"
#include "NKRenderer/src/NKRenderer/Scene/NkRenderScene.h"
#include "../Editor/NkEditorCamera.h"
#include "../Editor/NkGizmoSystem.h"
#include "../Editor/NkSelectionManager.h"

namespace nkentseu {
    namespace Unkeny {

        class ViewportLayer : public Layer {
        public:
            ViewportLayer(const NkString& name,
                          NkIDevice* device,
                          NkICommandBuffer* cmd) noexcept;
            ~ViewportLayer() override;

            void OnAttach() override;
            void OnDetach() override;
            void OnUpdate(float dt) override;
            void OnRender()         override;

            // ── Connexions (injectées par UnkenyApp::OnInit) ──────────────────
            void SetEditorCamera   (NkEditorCamera*    cam)  noexcept { mCamera = cam; }
            void SetGizmoSystem    (NkGizmoSystem*     gizmo)noexcept { mGizmos = gizmo; }
            void SetSelectionManager(NkSelectionManager* sel) noexcept { mSel = sel; }
            void SetWorld          (ecs::NkWorld*      world)noexcept { mWorld = world; }

            // ── Texture FBO → UILayer ─────────────────────────────────────────
            NkTextureHandle GetOutputTexture() const noexcept { return mColorTarget; }
            nk_uint32 GetViewportWidth()  const noexcept { return mVpWidth; }
            nk_uint32 GetViewportHeight() const noexcept { return mVpHeight; }

            // Appelé par UILayer quand le panel viewport change de taille
            void ResizeFBO(nk_uint32 w, nk_uint32 h) noexcept;

            // ── État souris dans le viewport ──────────────────────────────────
            // UILayer met à jour ces valeurs chaque frame
            struct MouseState {
                float32 x = 0, y = 0;       // position dans le viewport (pixels)
                float32 dx = 0, dy = 0;      // delta depuis frame précédente
                float32 scroll = 0;
                bool    leftDown  = false;
                bool    rightDown = false;
                bool    altDown   = false;
                bool    isHovered = false;   // souris dans le panel viewport
            };
            MouseState& GetMouseState() noexcept { return mMouse; }

        private:
            bool CreateFBO(nk_uint32 w, nk_uint32 h) noexcept;
            void DestroyFBO() noexcept;
            void RenderScene() noexcept;
            void RenderGizmos() noexcept;

            NkIDevice*        mDevice = nullptr;
            NkICommandBuffer* mCmd    = nullptr;

            nk_uint32 mVpWidth  = 0;
            nk_uint32 mVpHeight = 0;

            NkTextureHandle     mColorTarget;
            NkTextureHandle     mDepthTarget;
            NkRenderPassHandle  mRenderPass;
            NkFramebufferHandle mFramebuffer;

            // Connexions
            NkEditorCamera*     mCamera = nullptr;
            NkGizmoSystem*      mGizmos = nullptr;
            NkSelectionManager* mSel    = nullptr;
            ecs::NkWorld*       mWorld  = nullptr;

            renderer::NkRenderScene mRenderScene;
            MouseState              mMouse;
        };

    } // namespace Unkeny
} // namespace nkentseu
