// =============================================================================
// FICHIER: NKECS/Serialization/NkJsonSerialization.h
// DESCRIPTION: Sérialisation JSON robuste pour Prefabs & Blueprints.
// =============================================================================
#pragma once
#include "../NkECSDefines.h"
#include "../Prefab/NkPrefab.h"
#include "../VisualScript/NkBlueprint.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace nkentseu { namespace ecs { namespace serialization {

    using json = nlohmann::json;

    // ============================================================================
    // Helpers de sérialisation NkValue / NkPinType
    // ============================================================================
    inline void to_json(json& j, const blueprint::NkValue& v) noexcept {
        j = json::object();
        j["type"] = static_cast<uint32>(v.type);
        switch (v.type) {
            case blueprint::NkValueType::Int:    j["value"] = v.data.i; break;
            case blueprint::NkValueType::Float:  j["value"] = v.data.f; break;
            case blueprint::NkValueType::Bool:   j["value"] = v.data.b; break;
            case blueprint::NkValueType::Vec3:
                j["value"] = json{{"x", v.data.vec3.x}, {"y", v.data.vec3.y}, {"z", v.data.vec3.z}};
                break;
            case blueprint::NkValueType::String: j["value"] = v.data.str; break;
            case blueprint::NkValueType::EntityId: j["value"] = v.data.id; break;
            default: break;
        }
    }

    inline void from_json(const json& j, blueprint::NkValue& v) noexcept {
        v.type = static_cast<blueprint::NkValueType>(j.value("type", 0));
        const json& val = j["value"];
        switch (v.type) {
            case blueprint::NkValueType::Int:    v.data.i = val.get<int32>(); break;
            case blueprint::NkValueType::Float:  v.data.f = val.get<float32>(); break;
            case blueprint::NkValueType::Bool:   v.data.b = val.get<bool>(); break;
            case blueprint::NkValueType::Vec3:
                v.data.vec3 = {val["x"].get<float32>(), val["y"].get<float32>(), val["z"].get<float32>()};
                break;
            case blueprint::NkValueType::String:
                std::strncpy(v.data.str, val.get<std::string>().c_str(), sizeof(v.data.str)-1);
                break;
            case blueprint::NkValueType::EntityId: v.data.id = val.get<uint64>(); break;
            default: break;
        }
    }

    // ============================================================================
    // Sérialisation NkPrefab
    // ============================================================================
    inline void to_json(json& j, const NkPrefab& p) noexcept {
        j = json::object();
        j["name"] = p.name;
        j["version"] = p.version;
        j["tagBits"] = p.tagBits;
        j["layer"] = p.layer;

        j["components"] = json::array();
        for (const auto& [typeName, compData] : p.components) {
            json c = json::object();
            c["type"] = typeName;
            c["data"] = compData.jsonValue; // Stocké en JSON brut
            c["overridden"] = compData.isOverridden;
            j["components"].push_back(c);
        }

        j["children"] = json::array();
        for (const auto& child : p.children) {
            json ch = json::object();
            ch["name"] = child.name;
            ch["prefabPath"] = child.prefabPath;
            ch["position"] = {child.localPosition.x, child.localPosition.y, child.localPosition.z};
            ch["scale"] = {child.localScale.x, child.localScale.y, child.localScale.z};
            ch["active"] = child.isActive;
            j["children"].push_back(ch);
        }

        if (!p.blueprintPath.empty()) {
            j["blueprintPath"] = p.blueprintPath;
        }
    }

    inline void from_json(const json& j, NkPrefab& p) noexcept {
        p.name = j.value("name", "UnnamedPrefab");
        p.version = j.value("version", "1.0");
        p.tagBits = j.value("tagBits", 0);
        p.layer = j.value("layer", 0);
        p.blueprintPath = j.value("blueprintPath", "");
        p.components.clear();
        p.children.clear();

        if (j.contains("components")) {
            for (const auto& c : j["components"]) {
                NkPrefabComponentData data;
                data.typeName = c.value("type", "");
                data.jsonValue = c.value("data", "");
                data.isOverridden = c.value("overridden", false);
                p.components[data.typeName] = data;
            }
        }

        if (j.contains("children")) {
            for (const auto& c : j["children"]) {
                NkPrefabChild ch;
                ch.name = c.value("name", "");
                ch.prefabPath = c.value("prefabPath", "");
                ch.isActive = c.value("active", true);
                if (c.contains("position")) {
                    ch.localPosition = {c["position"][0].get<float32>(), c["position"][1].get<float32>(), c["position"][2].get<float32>()};
                }
                if (c.contains("scale")) {
                    ch.localScale = {c["scale"][0].get<float32>(), c["scale"][1].get<float32>(), c["scale"][2].get<float32>()};
                }
                p.children.push_back(ch);
            }
        }
    }

    // ============================================================================
    // Sérialisation NkBlueprintGraph
    // ============================================================================
    inline void to_json(json& j, const blueprint::NkBlueprintGraph& g) noexcept {
        j = json::object();
        j["nodes"] = json::array();
        j["connections"] = json::array();

        for (size_t i = 0; i < g.nodes.size(); ++i) {
            if (!g.nodes[i]) continue;
            const auto& node = *g.nodes[i];
            json n = json::object();
            n["id"] = static_cast<uint32>(i);
            n["type"] = node.name;
            n["enabled"] = node.enabled;
            n["inputs"] = json::array();
            n["outputs"] = json::array();

            for (size_t p = 0; p < node.inputs.size(); ++p) {
                json pin = json::object();
                pin["index"] = static_cast<uint32>(p);
                pin["name"] = node.inputs[p].name;
                pin["default"] = node.inputs[p].defaultValue;
                n["inputs"].push_back(pin);
            }
            for (size_t p = 0; p < node.outputs.size(); ++p) {
                json pin = json::object();
                pin["index"] = static_cast<uint32>(p);
                pin["name"] = node.outputs[p].name;
                n["outputs"].push_back(pin);
            }
            j["nodes"].push_back(n);
        }

        for (const auto& conn : g.connections) {
            j["connections"].push_back({
                {"srcNode", conn.sourceNode},
                {"srcPin", conn.sourcePin},
                {"tgtNode", conn.targetNode},
                {"tgtPin", conn.targetPin}
            });
        }
    }

    inline void from_json(const json& j, blueprint::NkBlueprintGraph& g) noexcept {
        g.nodes.clear();
        g.connections.clear();

        auto& reg = blueprint::NkNodeRegistry::Global();
        if (j.contains("nodes")) {
            for (const auto& n : j["nodes"]) {
                auto nodePtr = reg.Create(n.value("type", "").c_str());
                if (nodePtr) {
                    nodePtr->enabled = n.value("enabled", true);
                    // Restaurer les valeurs par défaut des pins
                    if (n.contains("inputs")) {
                        for (const auto& pinJson : n["inputs"]) {
                            uint32 idx = pinJson.value("index", 0);
                            if (idx < nodePtr->inputs.size()) {
                                from_json(pinJson["default"], nodePtr->inputs[idx].defaultValue);
                            }
                        }
                    }
                    g.nodes.push_back(std::move(nodePtr));
                }
            }
        }

        if (j.contains("connections")) {
            for (const auto& c : j["connections"]) {
                g.connections.push_back({
                    c.value("srcNode", 0xFFFFFFFFu),
                    c.value("srcPin", 0),
                    c.value("tgtNode", 0xFFFFFFFFu),
                    c.value("tgtPin", 0)
                });
            }
            // Reconnecter les flags IsConnected
            for (const auto& conn : g.connections) {
                if (conn.targetNode < g.nodes.size() && conn.targetPin < g.nodes[conn.targetNode]->inputs.size()) {
                    g.nodes[conn.targetNode]->inputs[conn.targetPin].isConnected = true;
                }
            }
        }
    }

    // ============================================================================
    // Fonctions Utilitaires Fichier
    // ============================================================================
    inline bool SaveToFile(const std::string& path, const NkPrefab& prefab) noexcept {
        try {
            std::ofstream file(path);
            if (!file.is_open()) return false;
            file << json(prefab).dump(4);
            return true;
        } catch (...) { return false; }
    }

    inline bool LoadFromFile(const std::string& path, NkPrefab& prefab) noexcept {
        try {
            std::ifstream file(path);
            if (!file.is_open()) return false;
            json j;
            file >> j;
            from_json(j, prefab);
            return true;
        } catch (...) { return false; }
    }

    inline bool SaveBlueprintToFile(const std::string& path, const blueprint::NkBlueprintGraph& graph) noexcept {
        try {
            std::ofstream file(path);
            if (!file.is_open()) return false;
            file << json(graph).dump(4);
            return true;
        } catch (...) { return false; }
    }

    inline bool LoadBlueprintFromFile(const std::string& path, blueprint::NkBlueprintGraph& graph) noexcept {
        try {
            std::ifstream file(path);
            if (!file.is_open()) return false;
            json j;
            file >> j;
            from_json(j, graph);
            return true;
        } catch (...) { return false; }
    }

}}} // namespace nkentseu::ecs::serialization

// =============================================================================
// EXEMPLES D'UTILISATION DE NKJSONSERIALIZATION.H
// =============================================================================
/*
// -----------------------------------------------------------------------------
// Exemple 1 : Sauvegarde/Chargement d'un Prefab
// -----------------------------------------------------------------------------
void Exemple_PrefabJson() {
    using namespace nkentseu::ecs;
    using namespace nkentseu::ecs::serialization;

    NkPrefab player;
    player.name = "Hero";
    player.WithComponent<NkTransform>()
          .WithTag(NkTagBit::Player)
          .WithChild(NkPrefabChild{"Weapon", "Assets/Prefabs/Sword.prefab"});

    SaveToFile("Assets/Prefabs/Hero.json", player);

    NkPrefab loaded;
    if (LoadFromFile("Assets/Prefabs/Hero.json", loaded)) {
        printf("Prefab chargé : %s (%zu composants, %zu enfants)\n",
               loaded.name.c_str(), loaded.components.size(), loaded.children.size());
    }
}

// -----------------------------------------------------------------------------
// Exemple 2 : Sauvegarde d'un Blueprint
// -----------------------------------------------------------------------------
void Exemple_BlueprintJson() {
    using namespace nkentseu::ecs;
    using namespace nkentseu::ecs::blueprint;
    using namespace nkentseu::ecs::serialization;

    NkBlueprintGraph graph;
    graph.AddNode(std::make_unique<NkNodeEventBeginPlay>());
    graph.AddNode(std::make_unique<NkNodePrintString>());
    graph.Link(0, 0, 1, 0);

    SaveBlueprintToFile("Assets/Blueprints/PlayerLogic.json", graph);

    NkBlueprintGraph loadedGraph;
    if (LoadBlueprintFromFile("Assets/Blueprints/PlayerLogic.json", loadedGraph)) {
        printf("Blueprint chargé : %zu nœuds, %zu connexions\n",
               loadedGraph.nodes.size(), loadedGraph.connections.size());
    }
}
*/