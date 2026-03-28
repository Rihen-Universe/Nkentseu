#pragma once
// =============================================================================
// GameObject.h  —  Style Unity
// Un GameObject est un conteneur de composants.
// Il possède toujours un Transform (non-removable).
// Les composants sont stockés par type_index pour accès O(1).
// =============================================================================
#include "Component.h"
#include "Transform.h"
#include <unordered_map>
#include <typeindex>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace Unity {

class Scene;  // forward

// ---------------------------------------------------------------------------
class GameObject : public std::enable_shared_from_this<GameObject> {
    friend class Scene;

public:
    std::string name;
    std::string tag;
    int         layer = 0;

    // ── Construction (toujours via Scene::CreateGameObject) ───────────────
    explicit GameObject(std::string n = "GameObject")
        : name(std::move(n))
    {
        // Transform est TOUJOURS présent, créé en premier
        auto t = std::make_unique<Transform>();
        t->mOwner = this;
        mTransform = t.get();
        mComponents[std::type_index(typeid(Transform))] = std::move(t);
    }

    ~GameObject() { OnDestroy(); }

    // ── Actif ──────────────────────────────────────────────────────────────
    bool IsActive() const { return mActive; }
    void SetActive(bool v) { mActive = v; }

    // ── Transform : accès rapide ───────────────────────────────────────────
    Transform* GetTransform() const { return mTransform; }
    Transform* transform()    const { return mTransform; }  // alias style Unity

    // ── AddComponent<T>(args...) ──────────────────────────────────────────
    // Crée et attache un composant du type T.
    // Appelle Awake() immédiatement après l'attachement.
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>,
                      "T must derive from Component");

        auto key = std::type_index(typeid(T));
        if (mComponents.count(key)) {
            // Unity autorise plusieurs composants du même type,
            // mais pour simplifier on retourne l'existant ici.
            return static_cast<T*>(mComponents[key].get());
        }

        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = comp.get();
        ptr->mOwner = this;
        mComponents[key] = std::move(comp);

        // Awake() est appelé immédiatement
        ptr->Awake();

        // On mémorise pour l'ordre d'update
        mUpdateOrder.push_back(ptr);

        // Start() sera appelé la frame suivante (drapeau)
        mPendingStart.push_back(ptr);

        return ptr;
    }

    // ── GetComponent<T>() ─────────────────────────────────────────────────
    template<typename T>
    T* GetComponent() const {
        auto it = mComponents.find(std::type_index(typeid(T)));
        if (it == mComponents.end()) return nullptr;
        return static_cast<T*>(it->second.get());
    }

    // ── RemoveComponent<T>() ──────────────────────────────────────────────
    template<typename T>
    void RemoveComponent() {
        static_assert(!std::is_same_v<T, Transform>,
                      "Cannot remove Transform");
        auto key = std::type_index(typeid(T));
        auto it  = mComponents.find(key);
        if (it == mComponents.end()) return;

        Component* ptr = it->second.get();
        ptr->OnDestroy();

        mUpdateOrder.erase(
            std::remove(mUpdateOrder.begin(), mUpdateOrder.end(), ptr),
            mUpdateOrder.end());
        mPendingStart.erase(
            std::remove(mPendingStart.begin(), mPendingStart.end(), ptr),
            mPendingStart.end());

        mComponents.erase(it);
    }

    // ── RequireComponent<T>() : récupère ou crée ──────────────────────────
    template<typename T>
    T* RequireComponent() {
        T* c = GetComponent<T>();
        return c ? c : AddComponent<T>();
    }

    // ── Hiérarchie : parent/enfants via Transform ──────────────────────────
    void SetParent(GameObject* parent) {
        transform()->SetParent(parent ? parent->transform() : nullptr);
        mParent = parent;
    }
    GameObject* GetParent() const { return mParent; }

    // ── Marqué pour destruction ────────────────────────────────────────────
    bool IsMarkedForDestroy() const { return mMarkedForDestroy; }
    void Destroy() { mMarkedForDestroy = true; }

private:
    // Appelé par Scene chaque frame
    void TickStart() {
        for (Component* c : mPendingStart)
            if (c->mEnabled) c->Start();
        mPendingStart.clear();
    }

    void TickUpdate(float dt) {
        if (!mActive) return;
        for (Component* c : mUpdateOrder)
            if (c->mEnabled) c->Update(dt);
    }

    void TickLateUpdate(float dt) {
        if (!mActive) return;
        for (Component* c : mUpdateOrder)
            if (c->mEnabled) c->LateUpdate(dt);
    }

    void OnDestroy() {
        for (auto& [key, comp] : mComponents)
            comp->OnDestroy();
    }

    // ── Données ───────────────────────────────────────────────────────────
    std::unordered_map<std::type_index, std::unique_ptr<Component>> mComponents;
    std::vector<Component*> mUpdateOrder;
    std::vector<Component*> mPendingStart;

    Transform*   mTransform       = nullptr;
    GameObject*  mParent          = nullptr;
    bool         mActive          = true;
    bool         mMarkedForDestroy = false;
};

// ---------------------------------------------------------------------------
// Implémentation du raccourci Component::GetComponent<T>()
// (doit être après la définition complète de GameObject)
template<typename T>
T* Component::GetComponent() {
    return mOwner ? mOwner->GetComponent<T>() : nullptr;
}

} // namespace Unity
