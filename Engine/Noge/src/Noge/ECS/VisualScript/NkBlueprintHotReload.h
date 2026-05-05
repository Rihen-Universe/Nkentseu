// =============================================================================
// FICHIER: NKECS/VisualScript/NkBlueprintHotReload.h
// DESCRIPTION: Hot-Reload complet avec restauration d'état des pins.
// =============================================================================
#pragma once
#include "NkBlueprint.h"
#include "../Serialization/NkJsonSerialization.h"
#include <unordered_map>
#include <chrono>
#include <functional>

namespace nkentseu { namespace ecs { namespace blueprint {

    // ============================================================================
    // Snapshot d'état pour migration
    // ============================================================================
    struct NkBlueprintStateSnapshot {
        std::unordered_map<std::string, NkValue> pinDefaults; // "NodeName_PinIndex" → valeur
        bool wasPlaying = false;
        float32 executionTime = 0.f;
    };

    // ============================================================================
    // NkBlueprintHotReloadManager
    // ============================================================================
    class NkBlueprintHotReloadManager {
    public:
        using OnReloadedFn = std::function<void(NkBlueprintComponent&, bool success)>;

        void RegisterComponent(NkBlueprintComponent* comp, const std::string& filePath) noexcept {
            if (comp && !filePath.empty()) {
                mRegistry[comp] = {filePath, GetFileTime(filePath), false};
            }
        }

        void UnregisterComponent(NkBlueprintComponent* comp) noexcept {
            mRegistry.erase(comp);
        }

        void Poll(NkWorld& world, NkEntityId self) noexcept {
            for (auto& [comp, entry] : mRegistry) {
                uint64 newTime = GetFileTime(entry.filePath);
                if (newTime > entry.lastModTime) {
                    entry.lastModTime = newTime;
                    ReloadGraph(comp, world, self);
                }
            }
        }

        void SetOnReloaded(OnReloadedFn fn) noexcept { mOnReloaded = std::move(fn); }

    private:
        struct Entry {
            std::string filePath;
            uint64 lastModTime = 0;
            bool pendingReload = false;
        };
        std::unordered_map<NkBlueprintComponent*, Entry> mRegistry;
        OnReloadedFn mOnReloaded;

        NkBlueprintStateSnapshot CaptureState(NkBlueprintGraph& oldGraph) noexcept {
            NkBlueprintStateSnapshot snap;
            for (size_t i = 0; i < oldGraph.nodes.size(); ++i) {
                if (!oldGraph.nodes[i]) continue;
                const auto& node = *oldGraph.nodes[i];
                for (size_t p = 0; p < node.inputs.size(); ++p) {
                    std::string key = node.name + "_" + std::to_string(p);
                    snap.pinDefaults[key] = node.inputs[p].defaultValue;
                }
            }
            return snap;
        }

        void ReloadGraph(NkBlueprintComponent* comp, NkWorld& world, NkEntityId self) noexcept {
            NkBlueprintGraph& oldGraph = comp->graph;
            NkBlueprintStateSnapshot state = CaptureState(oldGraph);

            NkBlueprintGraph newGraph;
            bool success = nkentseu::ecs::serialization::LoadBlueprintFromFile(
                mRegistry[comp].filePath, newGraph);

            if (success) {
                // Migration des valeurs par défaut vers le nouveau graphe
                for (size_t i = 0; i < newGraph.nodes.size(); ++i) {
                    if (!newGraph.nodes[i]) continue;
                    const auto& newNode = *newGraph.nodes[i];
                    for (size_t p = 0; p < newNode.inputs.size(); ++p) {
                        std::string key = newNode.name + "_" + std::to_string(p);
                        auto it = state.pinDefaults.find(key);
                        if (it != state.pinDefaults.end()) {
                            newGraph.nodes[i]->inputs[p].defaultValue = it->second;
                            newGraph.nodes[i]->inputs[p].isConnected = true;
                        }
                    }
                }
                // Remplacement atomique
                oldGraph = std::move(newGraph);
                oldGraph.entryNodeIndex = 0; // Reset point d'entrée sécurisé
            }

            if (mOnReloaded) {
                mOnReloaded(*comp, success);
            }
        }

        static uint64 GetFileTime(const std::string& path) noexcept {
            // Stub cross-platform : utiliser std::filesystem::last_write_time en prod
            (void)path;
            return static_cast<uint64>(std::chrono::system_clock::now().time_since_epoch().count());
        }
    };

}}} // namespace nkentseu::ecs::blueprint

// =============================================================================
// EXEMPLES D'UTILISATION DE NKBLUEPRINTHOTRELOAD.H
// =============================================================================
/*
// -----------------------------------------------------------------------------
// Exemple 1 : Mise en place du Hot-Reload
// -----------------------------------------------------------------------------
void Exemple_BlueprintHotReload(nkentseu::ecs::NkWorld& world) {
    using namespace nkentseu::ecs;
    using namespace nkentseu::ecs::blueprint;

    auto go = world.CreateGameObject("Player");
    auto* bp = go.Add<NkBlueprintComponent>();
    bp->graph.AddNode(std::make_unique<NkNodeEventBeginPlay>());
    bp->graph.AddNode(std::make_unique<NkNodePrintString>());
    bp->graph.Link(0, 0, 1, 0);

    // Enregistrer pour surveillance
    NkBlueprintHotReloadManager& hotReload = NkBlueprintHotReloadManager::Instance();
    hotReload.RegisterComponent(bp, "Assets/Blueprints/PlayerLogic.json");
    hotReload.SetOnReloaded([](NkBlueprintComponent& c, bool ok) {
        printf("[HotReload] Blueprint %s rechargé : %s\n",
               c.GetTypeName(), ok ? "SUCCÈS" : "ÉCHEC");
    });

    // Dans la boucle de jeu :
    // hotReload.Poll(world, go.Id());
}
*/