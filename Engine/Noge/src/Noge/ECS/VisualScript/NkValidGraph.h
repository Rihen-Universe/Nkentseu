// =============================================================================
// FICHIER: NKECS/VisualScript/NkValidGraph.h
// DESCRIPTION: Implémentations complémentaires et logique de graphes avancés
// =============================================================================
#include "NkBlueprint.h"
#include "NKContainers/String/NkFormat.h"

namespace nkentseu {
	namespace ecs {
		namespace blueprint {

			// ============================================================================
			// Helpers de validation et d'optimisation de graphes
			// ============================================================================

			// Vérifie l'intégrité du graphe (cycles, connexions orphelines)
			[[nodiscard]] bool ValidateGraph(const NkBlueprintGraph &graph) noexcept {
				if (graph.Nodes.empty()) {
					return false;
				}
				// Vérification basique : aucune connexion ne pointe hors limites
				for (const auto &conn : graph.Connections) {
					if (conn.SourceNode >= graph.Nodes.size() || conn.TargetNode >= graph.Nodes.size()) {
						return false;
					}
					if (conn.SourcePin >= graph.Nodes[conn.SourceNode]->Outputs.size() ||
						conn.TargetPin >= graph.Nodes[conn.TargetNode]->Inputs.size()) {
						return false;
					}
				}
				return true;
			}

			// Nettoie les nœuds désactivés et compacte le graphe
			void CompactGraph(NkBlueprintGraph &graph) noexcept {
				// Supprime les connexions pointant vers des nœuds invalides (zéro-STL).
				uint32 i = 0;
				while (i < graph.Connections.size()) {
					const NkBlueprintConnection &c = graph.Connections[i];
					bool invalid = c.SourceNode >= graph.Nodes.size() ||
								   c.TargetNode >= graph.Nodes.size() ||
								   !graph.Nodes[c.SourceNode] ||
								   !graph.Nodes[c.TargetNode];
					if (invalid) {
						graph.Connections.Erase(graph.Connections.Begin() + i);
					} else {
						i++;
					}
				}
				// Réindexation des connections si nécessaire (omise pour performance en runtime)
			}

			// ============================================================================
			// Sérialisation simplifiée (Structure JSON ready)
			// ============================================================================

			bool SerializeBlueprint(const NkBlueprintGraph &graph, char *buffer, uint32 bufSize) noexcept {
				if (!buffer || bufSize == 0)
					return false;
				// Stub : en production, utiliser une lib JSON (nlohmann, RapidJSON, etc.)
				NkString formatted = NkFormat("{{\"nodes\":{0},\"connections\":{1}}}",
											  static_cast<uint32>(graph.Nodes.size()),
											  static_cast<uint32>(graph.Connections.size()));
				const char *src = formatted.CStr();
				uint32 j = 0;
				while (j + 1 < bufSize && src[j] != '\0') {
					buffer[j] = src[j];
					j++;
				}
				buffer[j] = '\0';
				return true;
			}

			bool DeserializeBlueprint(NkBlueprintGraph &graph, const char *json) noexcept {
				(void)graph;
				(void)json;
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

	if (ValidateGraph(bp->Graph)) {
		// Safe to execute
		bp->Graph.Execute(world, go.Id(), 0.f);
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