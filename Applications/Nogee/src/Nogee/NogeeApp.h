#pragma once
// =============================================================================
// Noge/NogeApp.h  —  v2
// =============================================================================

#include "Nkentseu/Core/Application.h"
#include "UkConfig.h"
#include "Layers/EditorLayer.h"
#include "Layers/ViewportLayer.h"
#include "Layers/UILayer.h"
#include "Editor/NkEditorCamera.h"

namespace nkentseu {
    namespace noge {

        class NogeApp : public Application {
            public:
                explicit NogeApp(const NogeAppConfig& config);
                ~NogeApp() override;

            protected:
                void OnInit()                        override;
                void OnStart()                       override;
                void OnUpdate(float dt)              override;
                void OnRender()                      override;
                void OnUIRender()                    override;
                void OnShutdown()                    override;
                void OnClose()                       override;
                void OnResize(nk_uint32 w, nk_uint32 h) override;

            private:
                NogeAppConfig mUkConfig;

                // Layers — pointeurs non-owning (ownership dans LayerStack)
                EditorLayer*    mEditorLayer   = nullptr;
                ViewportLayer*  mViewportLayer = nullptr;
                UILayer*        mUILayer       = nullptr;

                // Caméra éditeur (owned ici, partagée avec ViewportLayer)
                NkEditorCamera* mEditorCamera  = nullptr;
        };

    } // namespace Noge
} // namespace nkentseu
