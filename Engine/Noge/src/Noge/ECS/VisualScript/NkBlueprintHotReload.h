// =============================================================================
// FICHIER: NKECS/VisualScript/NkBlueprintHotReload.h
// DESCRIPTION: Hot-Reload complet avec restauration d'état des pins.
// =============================================================================
#pragma once
#include "NkBlueprint.h"
#include "../Serialization/NkJsonSerialization.h"
#include "NKFileSystem/NkFileSystem.h"
#include <unordered_map>
#include <chrono>
#include <functional>

namespace nkentseu {
	namespace ecs {
		namespace blueprint {

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
					using OnReloadedFn = std::function<void(NkBlueprintComponent &, bool success)>;

					void RegisterComponent(NkBlueprintComponent *comp, const std::string &filePath) noexcept {
						if (comp && !filePath.empty()) {
							mRegistry[comp] = {filePath, GetFileTime(filePath), false};
						}
					}

					void UnregisterComponent(NkBlueprintComponent *comp) noexcept {
						mRegistry.erase(comp);
					}

					void Poll(NkWorld &world, NkEntityId self) noexcept {
						for (auto &[comp, entry] : mRegistry) {
							uint64 newTime = GetFileTime(entry.filePath);
							if (newTime > entry.lastModTime) {
								entry.lastModTime = newTime;
								ReloadGraph(comp, world, self);
							}
						}
					}

					void SetOnReloaded(OnReloadedFn fn) noexcept {
						mOnReloaded = std::move(fn);
					}

				private:
					struct Entry {
							std::string filePath;
							uint64 lastModTime = 0;
							bool pendingReload = false;
					};

					std::unordered_map<NkBlueprintComponent *, Entry> mRegistry;
					OnReloadedFn mOnReloaded;

					NkBlueprintStateSnapshot CaptureState(NkBlueprintGraph &oldGraph) noexcept {
						NkBlueprintStateSnapshot snap;
						for (size_t i = 0; i < oldGraph.Nodes.size(); ++i) {
							if (!oldGraph.Nodes[i])
								continue;
							const auto &node = *oldGraph.Nodes[i];
							for (size_t p = 0; p < node.Inputs.size(); ++p) {
								std::string key = node.Name + "_" + std::to_string(p);
								snap.pinDefaults[key] = node.Inputs[p].DefaultValue;
							}
						}
						return snap;
					}

					void ReloadGraph(NkBlueprintComponent *comp, NkWorld &world, NkEntityId self) noexcept {
						NkBlueprintGraph &oldGraph = comp->Graph;
						NkBlueprintStateSnapshot state = CaptureState(oldGraph);

						NkBlueprintGraph newGraph;
						bool success =
							nkentseu::ecs::serialization::LoadBlueprintFromFile(mRegistry[comp].filePath, newGraph);

						if (success) {
							// Migration des valeurs par défaut vers le nouveau graphe
							for (size_t i = 0; i < newGraph.Nodes.size(); ++i) {
								if (!newGraph.Nodes[i])
									continue;
								const auto &newNode = *newGraph.Nodes[i];
								for (size_t p = 0; p < newNode.Inputs.size(); ++p) {
									std::string key = newNode.Name + "_" + std::to_string(p);
									auto it = state.pinDefaults.find(key);
									if (it != state.pinDefaults.end()) {
										newGraph.Nodes[i]->Inputs[p].DefaultValue = it->second;
										newGraph.Nodes[i]->Inputs[p].IsConnected = true;
									}
								}
							}
							// Remplacement atomique
							oldGraph = std::move(newGraph);
							oldGraph.EntryNodeIndex = 0; // Reset point d'entrée sécurisé
						}

						if (mOnReloaded) {
							mOnReloaded(*comp, success);
						}
					}

					static uint64 GetFileTime(const std::string &path) noexcept {
						// Horodatage réel de dernière écriture via NKFileSystem (déjà une dépendance
						// de Noge, voir Noge.jenga : "NKFileSystem/NkFile.h (NkSceneSerializer)").
						// nk_int64 en epoch Unix ; 0 si le fichier est introuvable/inaccessible.
						return static_cast<uint64>(nkentseu::NkFileSystem::GetLastWriteTime(path.c_str()));
					}
			};

		} // namespace blueprint
	} // namespace ecs
} // namespace nkentseu

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
	bp->Graph.AddNode(std::make_unique<NkNodeEventBeginPlay>());
	bp->Graph.AddNode(std::make_unique<NkNodeCallFunction>());
	bp->Graph.Link(0, 0, 1, 0);

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