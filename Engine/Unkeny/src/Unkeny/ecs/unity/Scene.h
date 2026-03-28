#pragma once
// =============================================================================
// Scene.h  —  Style Unity
// La Scene gère le cycle de vie de tous les GameObjects.
// Elle maintient la liste des objets, pilote les Update(), gère la destruction.
// =============================================================================
#include "GameObject.h"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <functional>
#include <unordered_map>

namespace Unity {

// ---------------------------------------------------------------------------
class Scene {
public:
    std::string name;

    explicit Scene(std::string n = "Main") : name(std::move(n)) {}

    // ── Création ───────────────────────────────────────────────────────────
    // Crée un GameObject et le rend propriété de la Scene.
    // Retourne un pointeur nu (la Scene possède l'objet via shared_ptr).
    GameObject* CreateGameObject(const std::string& name = "GameObject") {
        auto go = std::make_shared<GameObject>(name);
        GameObject* ptr = go.get();
        mObjects.push_back(std::move(go));
        return ptr;
    }

    // ── Recherche ──────────────────────────────────────────────────────────
    GameObject* Find(const std::string& name) const {
        for (auto& go : mObjects)
            if (go->name == name) return go.get();
        return nullptr;
    }

    std::vector<GameObject*> FindAll(const std::string& tag) const {
        std::vector<GameObject*> result;
        for (auto& go : mObjects)
            if (go->tag == tag) result.push_back(go.get());
        return result;
    }

    // ── Destruction différée ───────────────────────────────────────────────
    void Destroy(GameObject* go) {
        if (go) go->Destroy();
    }

    // Détruit tous les objets avec ce tag
    void DestroyByTag(const std::string& tag) {
        for (auto& go : mObjects)
            if (go->tag == tag) go->Destroy();
    }

    // ── Boucle principale ──────────────────────────────────────────────────
    // À appeler depuis la boucle de rendu du moteur.
    void Update(float dt) {
        // Snapshot : on itère sur la taille actuelle, les nouveaux GO
        // créés pendant l'update seront tickés à la frame suivante.
        const size_t countAtStart = mObjects.size();

        // 1. Start() pour les nouveaux composants
        for (size_t i = 0; i < countAtStart; ++i)
            mObjects[i]->TickStart();

        // 2. Update() sur tous les actifs
        for (size_t i = 0; i < countAtStart; ++i)
            mObjects[i]->TickUpdate(dt);

        // 3. LateUpdate() — après tous les Update()
        for (size_t i = 0; i < countAtStart; ++i)
            mObjects[i]->TickLateUpdate(dt);

        // 4. Nettoyage : destruction différée
        mObjects.erase(
            std::remove_if(mObjects.begin(), mObjects.end(),
                [](const std::shared_ptr<GameObject>& go){
                    return go->IsMarkedForDestroy();
                }),
            mObjects.end());
    }

    // ── Statistiques ──────────────────────────────────────────────────────
    size_t ObjectCount() const { return mObjects.size(); }

    // ── Itération ─────────────────────────────────────────────────────────
    void ForEach(std::function<void(GameObject&)> fn) {
        for (auto& go : mObjects) fn(*go);
    }

    // Récupère tous les composants d'un type T dans toute la scène
    template<typename T>
    std::vector<T*> FindAllComponents() const {
        std::vector<T*> result;
        for (auto& go : mObjects) {
            if (T* c = go->GetComponent<T>())
                result.push_back(c);
        }
        return result;
    }

private:
    std::vector<std::shared_ptr<GameObject>> mObjects;
};

} // namespace Unity
