#pragma once

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"

namespace nkentseu {
namespace ecs {

    // Forward declaration
    class NkGameObject;

    // =========================================================================
    // NkBehaviour — classe de base pour tous les scripts (comme MonoBehaviour)
    // =========================================================================
    class NkBehaviour {
    public:
        virtual ~NkBehaviour() = default;

        // Cycle de vie (appelé automatiquement par NkBehaviourSystem)
        virtual void OnAwake()   {}  // immédiatement après l'ajout du behaviour
        virtual void OnStart()   {}  // avant la première frame
        virtual void OnEnable()  {}  // quand le GameObject devient actif
        virtual void OnDisable() {}  // quand le GameObject devient inactif
        virtual void OnUpdate(float dt) {}
        virtual void OnLateUpdate(float dt) {}
        virtual void OnFixedUpdate(float fixedDt) {}
        virtual void OnDestroy() {}   // avant la destruction de l'entité

        // Accès au GameObject propriétaire (rempli automatiquement)
        NkGameObject* GetGameObject() const { return mGameObject; }

        // Active/désactive ce behaviour individuellement
        void SetEnabled(bool enabled) { mEnabled = enabled; OnEnableStateChanged(); }
        bool IsEnabled() const { return mEnabled; }

        // Nom du script (pour débogage)
        virtual const char* GetTypeName() const = 0;

    protected:
        NkGameObject* mGameObject = nullptr;
        bool mEnabled = true;
        bool mStarted = false;

    private:
        friend class NkGameObject;
        friend class NkBehaviourSystem;
        void SetGameObject(NkGameObject* go) { mGameObject = go; }
        virtual void OnEnableStateChanged() {
            if (mEnabled) OnEnable();
            else OnDisable();
        }
    };

} // namespace ecs
} // namespace nkentseu