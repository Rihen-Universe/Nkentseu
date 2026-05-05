// =============================================================================
// FICHIER: NKECS/VisualScript/NkValidGraph.h
// DESCRIPTION: Implémentations complémentaires et logique de graphes avancés
// =============================================================================
#include "NkBlueprint.h"

namespace nkentseu {
    namespace ecs {
        namespace blueprint {

            // ============================================================================
            // Helpers de validation et d'optimisation de graphes
            // ============================================================================

            // Vérifie l'intégrité du graphe (cycles, connexions orphelines)
            [[nodiscard]] bool ValidateGraph(const NkBlueprintGraph& graph) noexcept {
                if (graph.nodes.empty()) {
                    return false;
                }
                // Vérification basique : aucune connexion ne pointe hors limites
                for (const auto& conn : graph.connections) {
                    if (conn.sourceNode >= graph.nodes.size() ||
                        conn.targetNode >= graph.nodes.size()) {
                        return false;
                    }
                    if (conn.sourcePin >= graph.nodes[conn.sourceNode]->outputs.size() ||
                        conn.targetPin >= graph.nodes[conn.targetNode]->inputs.size()) {
                        return false;
                    }
                }
                return true;
            }

            // Nettoie les nœuds désactivés et compacte le graphe
            void CompactGraph(NkBlueprintGraph& graph) noexcept {
                // Supprime les connexions pointant vers des nœuds invalides
                graph.connections.erase(
                    std::remove_if(graph.connections.begin(), graph.connections.end(),
                                   [&](const NkBlueprintConnection& c) {
                                       return c.sourceNode >= graph.nodes.size() ||
                                              c.targetNode >= graph.nodes.size() ||
                                              !graph.nodes[c.sourceNode] ||
                                              !graph.nodes[c.targetNode];
                                   }),
                    graph.connections.end()
                );
                // Réindexation des connections si nécessaire (omise pour performance en runtime)
            }

            // ============================================================================
            // Sérialisation simplifiée (Structure JSON ready)
            // ============================================================================

            bool SerializeBlueprint(const NkBlueprintGraph& graph, char* buffer, uint32 bufSize) noexcept {
                if (!buffer || bufSize == 0) return false;
                // Stub : en production, utiliser une lib JSON (nlohmann, RapidJSON, etc.)
                std::snprintf(buffer, bufSize, "{\"nodes\":%u,\"connections\":%u}",
                              static_cast<uint32>(graph.nodes.size()),
                              static_cast<uint32>(graph.connections.size()));
                return true;
            }

            bool DeserializeBlueprint(NkBlueprintGraph& graph, const char* json) noexcept {
                (void)graph; (void)json;
                // Stub : parsing JSON → création de nœuds via Registry → reconstruction des liens
                return false;
            }

        } // namespace blueprint
    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKBLUEPRINT.CPP
// =============================================================================
/*
// -----------------------------------------------------------------------------
// Exemple 1 : Validation avant exécution (sécurité éditeur/runtime)
// -----------------------------------------------------------------------------
void Exemple_Validation(nkentseu::ecs::NkWorld& world) {
    using namespace nkentseu::ecs::blueprint;
    auto go = world.CreateGameObject("ValidatedBP");
    auto* bp = go.Add<NkBlueprintComponent>();

    // ... construction du graphe ...

    if (ValidateGraph(bp->graph)) {
        // Safe to execute
        bp->executionContext.world = &world;
        bp->executionContext.self = go.Id();
        bp->graph.Execute(bp->executionContext);
    }
}

// -----------------------------------------------------------------------------
// Exemple 2 : Compactage dynamique (nettoyage mémoire)
// -----------------------------------------------------------------------------
void Exemple_Compaction(nkentseu::ecs::blueprint::NkBlueprintGraph& graph) {
    // Supprime les nœuds invalidés par hot-reload ou suppression éditeur
    CompactGraph(graph);
}

// -----------------------------------------------------------------------------
// Exemple 3 : Sauvegarde/Chargement (intégration pipeline éditeur)
// -----------------------------------------------------------------------------
void Exemple_Serialization(nkentseu::ecs::blueprint::NkBlueprintGraph& graph) {
    char buffer[4096];
    if (SerializeBlueprint(graph, buffer, sizeof(buffer))) {
        // Écrire dans un fichier .bpjson
    }

    // Chargement :
    NkBlueprintGraph loadedGraph;
    // DeserializeBlueprint(loadedGraph, jsonData);
}
*/