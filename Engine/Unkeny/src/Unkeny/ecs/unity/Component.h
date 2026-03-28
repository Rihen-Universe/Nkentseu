#pragma once
// =============================================================================
// Component.h  —  Style Unity
// Classe de base de tous les composants.
// Un Component appartient toujours à un GameObject.
// =============================================================================
#include <string>
#include <memory>

namespace Unity {

class GameObject;   // forward

// ---------------------------------------------------------------------------
// Component — classe abstraite dont dérivent tous les comportements
// ---------------------------------------------------------------------------
class Component {
    friend class GameObject;   // seul GameObject peut appeler Init/Shutdown

public:
    virtual ~Component() = default;

    // ── Cycle de vie (appelés par le moteur dans l'ordre) ──────────────────
    virtual void Awake()  {}   // 1. Juste après l'attachement au GameObject
    virtual void Start()  {}   // 2. Avant le premier Update(), après tous les Awake()
    virtual void Update(float dt) {}   // 3. Chaque frame
    virtual void LateUpdate(float dt) {}  // 4. Après tous les Update()
    virtual void OnDestroy() {}  // 5. Juste avant la destruction

    // ── Accesseur vers le propriétaire ────────────────────────────────────
    GameObject* GetGameObject() const { return mOwner; }

    // ── État actif ────────────────────────────────────────────────────────
    bool IsEnabled() const { return mEnabled; }
    void SetEnabled(bool v) { mEnabled = v; }

protected:
    // Raccourci : GetComponent<T>() sur le même GameObject
    template<typename T>
    T* GetComponent();

private:
    GameObject* mOwner   = nullptr;
    bool        mEnabled = true;
};

} // namespace Unity
