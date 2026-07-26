// =============================================================================
// FICHIER: NKECS/VisualScript/NkBlueprintHotReload.h
// DESCRIPTION: Hot-Reload complet avec restauration d'état des pins.
// =============================================================================
#pragma once
#include "NkBlueprint.h"
#include "NKFileSystem/NkFileSystem.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkFormat.h"

namespace nkentseu {
	namespace ecs {
		namespace serialization {

			// Déclaration anticipée : le corps réside dans un .cpp de sérialisation
			// absent du build isolé. Une déclaration sans définition est légale.
			bool LoadBlueprintFromFile(const NkString &path, blueprint::NkBlueprintGraph &outGraph) noexcept;

		} // namespace serialization
	}	  // namespace ecs
} // namespace nkentseu

namespace nkentseu {
	namespace ecs {
		namespace blueprint {

			// ============================================================================
			// Snapshot d'état pour migration
			// ============================================================================
			struct NkBlueprintStateSnapshot {
					NkHashMap<NkString, NkValue> pinDefaults; // "NodeName_PinIndex" → valeur
					bool wasPlaying = false;
					float32 executionTime = 0.f;
			};

			// ============================================================================
			// NkBlueprintHotReloadManager
			// ============================================================================
			class NkBlueprintHotReloadManager {
				public:
					using OnReloadedFn = NkFunction<void(NkBlueprintComponent &, bool success)>;

					void RegisterComponent(NkBlueprintComponent *comp, const NkString &filePath) noexcept {
						if (comp && !filePath.Empty()) {
							mRegistry.Insert(comp, {filePath, GetFileTime(filePath), false});
						}
					}

					void UnregisterComponent(NkBlueprintComponent *comp) noexcept {
						mRegistry.Erase(comp);
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
						mOnReloaded = traits::NkMove(fn);
					}

				private:
					struct Entry {
							NkString filePath;
							uint64 lastModTime = 0;
							bool pendingReload = false;
					};

					// Hachage de clé pointeur (NkHash n'est pas spécialisé pour les pointeurs).
					struct NkComponentPtrHasher {
							usize operator()(NkBlueprintComponent *ptr) const noexcept {
								return static_cast<usize>(reinterpret_cast<nk_uintptr>(ptr));
							}
					};

					NkHashMap<NkBlueprintComponent *, Entry, memory::NkAllocator, NkComponentPtrHasher> mRegistry;
					OnReloadedFn mOnReloaded;

					NkBlueprintStateSnapshot CaptureState(NkBlueprintGraph &oldGraph) noexcept {
						NkBlueprintStateSnapshot snap;
						for (uint32 i = 0; i < oldGraph.Nodes.size(); ++i) {
							if (!oldGraph.Nodes[i])
								continue;
							const auto &node = *oldGraph.Nodes[i];
							for (uint32 p = 0; p < node.Inputs.size(); ++p) {
								NkString key = node.Name + "_" + NkFormat("{0}", p);
								snap.pinDefaults.Insert(key, node.Inputs[p].DefaultValue);
							}
						}
						return snap;
					}

					void ReloadGraph(NkBlueprintComponent *comp, NkWorld &world, NkEntityId self) noexcept {
						NkBlueprintGraph &oldGraph = comp->Graph;
						NkBlueprintStateSnapshot state = CaptureState(oldGraph);

						NkBlueprintGraph newGraph;
						bool success =
							nkentseu::ecs::serialization::LoadBlueprintFromFile(mRegistry.At(comp).filePath, newGraph);

						if (success) {
							// Migration des valeurs par défaut vers le nouveau graphe
							for (uint32 i = 0; i < newGraph.Nodes.size(); ++i) {
								if (!newGraph.Nodes[i])
									continue;
								const auto &newNode = *newGraph.Nodes[i];
								for (uint32 p = 0; p < newNode.Inputs.size(); ++p) {
									NkString key = newNode.Name + "_" + NkFormat("{0}", p);
									NkValue *it = state.pinDefaults.Find(key);
									if (it) {
										newGraph.Nodes[i]->Inputs[p].DefaultValue = *it;
										newGraph.Nodes[i]->Inputs[p].IsConnected = true;
									}
								}
							}
							// Remplacement atomique
							oldGraph = traits::NkMove(newGraph);
							oldGraph.EntryNodeIndex = 0; // Reset point d'entrée sécurisé
						}

						if (mOnReloaded) {
							mOnReloaded(*comp, success);
						}
					}

					static uint64 GetFileTime(const NkString &path) noexcept {
						// Horodatage réel de dernière écriture via NKFileSystem (déjà une dépendance
						// de Noge, voir Noge.jenga : "NKFileSystem/NkFile.h (NkSceneSerializer)").
						// nk_int64 en epoch Unix ; 0 si le fichier est introuvable/inaccessible.
						return static_cast<uint64>(nkentseu::NkFileSystem::GetLastWriteTime(path.CStr()));
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
	bp->Graph.AddNode(memory::NkMakeUnique<NkNodeEventBeginPlay>());
	bp->Graph.AddNode(memory::NkMakeUnique<NkNodeCallFunction>());
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