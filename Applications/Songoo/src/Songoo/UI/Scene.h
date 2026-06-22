#pragma once
// =============================================================================
// Scene.h — Interface abstraite d'une scène (copie exacte du repo Nkentseu)
// =============================================================================

#include "AppContext.h"

namespace nkentseu { class NkEvent; }

namespace nkentseu { namespace songoo {

    class Scene {
    public:
        virtual ~Scene() = default;
        virtual const char* Name() const noexcept { return "Scene"; }

        virtual void OnEnter (AppContext& /*ctx*/) {}
        virtual void OnUpdate(AppContext& /*ctx*/, float /*dt*/) {}
        virtual void OnRender(AppContext& /*ctx*/) {}
        virtual void OnResize(AppContext& /*ctx*/, int /*w*/, int /*h*/) {}
        virtual void OnEvent (AppContext& /*ctx*/, NkEvent& /*ev*/) {}
        virtual void OnPause (AppContext& /*ctx*/) {}
        virtual void OnResume(AppContext& /*ctx*/) {}
        virtual void OnResumedFromChild(AppContext& /*ctx*/) {}
        virtual void OnExit  (AppContext& /*ctx*/) {}
    };

}} // namespace nkentseu::songoo
