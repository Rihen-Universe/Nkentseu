#pragma once
// =============================================================================
// FICHIER: Noge/ECS/Scripting/NkScriptBridge.h
// DESCRIPTION: Scripting sans recompilation du moteur via chargement de DLL/SO.
//
// Architecture "hot-reload" style Unity/Unreal :
//
//   1. L'utilisateur écrit ses scripts dans des fichiers .cpp séparés
//      (base NkScriptDLLBase + macro NK_EXPORT_DLL_SCRIPT — cf. NkScriptABI.h)
//   2. Le .cpp est compilé en .dll/.so par un simple appel compilateur
//      (aucun link avec le moteur : ABI C stable, cf. NkScriptABI.h)
//   3. NkScriptLoader charge la DLL à runtime (LoadLibrary/dlopen)
//   4. NkScriptLoader::HotReload() (à appeler chaque frame ou à la demande)
//      détecte le changement de mtime du fichier (NkFileSystem::GetLastWriteTime)
//      et recharge la DLL avec CAPTURE puis RESTAURATION de l'état des scripts
//      (Serialize/Deserialize via l'ABI).
//
// Windows : la DLL est chargée via une SHADOW COPY ("Foo.dll" -> "Foo.dll.hotN")
// car LoadLibrary verrouille le fichier — sans copie, impossible de recompiler
// le script pendant que le jeu tourne. Le mtime surveillé est celui du fichier
// D'ORIGINE (celui que la recompilation écrase).
//
// Mémoire : les instances de scripts DLL sont allouées via NKMemory — le loader
// injecte ses hooks (nkecs_set_allocator) dans la DLL juste après le chargement.
// Zéro STL dans ce module (NkSharedPtr/NkVector maison, cf. NkScriptComponent.h).
//
// ── Usage côté moteur ────────────────────────────────────────────────────────
//
//   auto &loader = NkScriptLoader::Global();
//   loader.LoadDLL("Scripts/PlayerController.dll");  // ou LoadDirectory("Scripts/")
//
//   NkScriptHost &host = world.Add<NkScriptHost>(entity);
//   AttachDLLScript(world, entity, host, "PlayerController");
//
//   // chaque frame (poll) :
//   loader.HotReload(world);
//   scriptSystem.Execute(world, dt);
//
// Implémentation : NkScriptBridge.cpp (chargement DLL réel, hot-reload).
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NkScriptABI.h"
#include "NkScriptComponent.h"
#include <cstring>

namespace nkentseu {
	namespace ecs {

		// Handle de module opaque (HMODULE Windows / void* dlopen) — pas de
		// <windows.h> dans ce header (pollution macro) : cast dans le .cpp.
		using NkDLLHandle = void *;

#if defined(_WIN32)
#define NKECS_DLL_EXT ".dll"
#elif defined(__APPLE__)
#define NKECS_DLL_EXT ".dylib"
#else
#define NKECS_DLL_EXT ".so"
#endif

		// =============================================================================
		// NkDLLScriptAdapter — adapte une factory DLL vers NkScriptComponent
		// =============================================================================
		// Fait le pont entre le cycle de vie NkScriptSystem (NkWorld/NkEntityId)
		// et la vtable C de la DLL (NkScriptContext opaque).
		class NkDLLScriptAdapter final : public NkScriptComponent {
			public:
				explicit NkDLLScriptAdapter(const NkScriptDLLFactory &factory) noexcept : mFactory(factory) {
					mInstance = factory.Create ? factory.Create() : nullptr;
					std::strncpy(mTypeName, factory.info.name, sizeof(mTypeName) - 1);
				}

				~NkDLLScriptAdapter() noexcept override {
					if (mInstance && mFactory.Destroy) {
						mFactory.Destroy(mInstance);
					}
					mInstance = nullptr;
				}

				NkDLLScriptAdapter(const NkDLLScriptAdapter &) = delete;
				NkDLLScriptAdapter &operator=(const NkDLLScriptAdapter &) = delete;

				[[nodiscard]] const char *GetTypeName() const noexcept override {
					return mTypeName;
				}

				[[nodiscard]] bool IsInstanceValid() const noexcept {
					return mInstance != nullptr;
				}

				void OnStart(NkWorld &world, NkEntityId self) noexcept override {
					if (auto fn = mFactory.vtable.OnStart; fn && mInstance) {
						auto ctx = MakeCtx(world, self, 0.f);
						fn(mInstance, &ctx);
					}
				}

				void OnUpdate(NkWorld &world, NkEntityId self, float32 dt) noexcept override {
					if (auto fn = mFactory.vtable.OnUpdate; fn && mInstance) {
						auto ctx = MakeCtx(world, self, dt);
						fn(mInstance, &ctx);
					}
				}

				void OnFixedUpdate(NkWorld &world, NkEntityId self, float32 fdt) noexcept override {
					if (auto fn = mFactory.vtable.OnFixedUpdate; fn && mInstance) {
						auto ctx = MakeCtx(world, self, fdt);
						fn(mInstance, &ctx);
					}
				}

				void OnLateUpdate(NkWorld &world, NkEntityId self, float32 dt) noexcept override {
					if (auto fn = mFactory.vtable.OnLateUpdate; fn && mInstance) {
						auto ctx = MakeCtx(world, self, dt);
						fn(mInstance, &ctx);
					}
				}

				void OnDestroy(NkWorld &world, NkEntityId self) noexcept override {
					if (auto fn = mFactory.vtable.OnDestroy; fn && mInstance) {
						auto ctx = MakeCtx(world, self, 0.f);
						fn(mInstance, &ctx);
					}
				}

				void Serialize(char *buf, uint32 bufSize) const noexcept override {
					if (buf && bufSize) {
						buf[0] = '\0';
					}
					if (auto fn = mFactory.vtable.Serialize; fn && mInstance) {
						fn(mInstance, buf, bufSize);
					}
				}

				void Deserialize(const char *json) noexcept override {
					if (auto fn = mFactory.vtable.Deserialize; fn && mInstance) {
						fn(mInstance, json);
					}
				}

				// Hot-reload : capture l'état (JSON) puis restauration dans la
				// NOUVELLE instance (créée depuis la nouvelle factory).
				void CaptureState(char *buf, uint32 bufSize) const noexcept {
					Serialize(buf, bufSize);
				}

				void RestoreState(const char *json) noexcept {
					Deserialize(json);
				}

			private:
				NkScriptDLLFactory mFactory;
				void *mInstance = nullptr;
				char mTypeName[128] = {};

				static NkScriptContext MakeCtx(NkWorld &world, NkEntityId id, float32 dt) noexcept {
					NkScriptContext ctx;
					ctx.world = &world;
					ctx.entityPack = id.Pack();
					ctx.dt = dt;
					ctx.fixedDt = dt;
					return ctx;
				}
		};

		// =============================================================================
		// NkLoadedDLL — une DLL de script chargée en mémoire
		// =============================================================================
		struct NkLoadedDLL {
				char path[512] = {};	   // fichier d'origine (surveillé pour le hot-reload)
				char shadowPath[512] = {}; // copie réellement chargée (Windows), vide sinon
				char name[128] = {};	   // nom du script (factory.info.name)
				NkDLLHandle handle = nullptr;
				NkScriptDLLFactory factory;
				uint64 fileMTime = 0;  // mtime (epoch) du fichier d'origine au chargement
				uint32 generation = 0; // n° de rechargement (suffixe de la shadow copy)

				[[nodiscard]] bool IsValid() const noexcept {
					return handle != nullptr;
				}
		};

		// =============================================================================
		// NkScriptLoader — charge/décharge/recharge les DLL de scripts
		// =============================================================================
		class NkScriptLoader {
			public:
				static NkScriptLoader &Global() noexcept;

				// Charge une DLL de script (chemin relatif ou absolu). Retourne false
				// si le fichier n'existe pas / ne s'ouvre pas / n'exporte pas
				// nkecs_get_factory. Injecte les hooks mémoire NKMemory dans la DLL.
				bool LoadDLL(const char *path) noexcept;

				// Charge tous les *.dll / *.so d'un dossier (non récursif).
				// Retourne le nombre de DLL chargées avec succès.
				uint32 LoadDirectory(const char *dir) noexcept;

				// Décharge une DLL par nom de script. ATTENTION : les NkScriptHost
				// doivent avoir détaché leurs scripts de ce type au préalable
				// (les adapters survivants pointeraient vers du code déchargé).
				void UnloadDLL(const char *name) noexcept;

				// Hot-reload : pour chaque DLL dont le fichier d'origine a changé
				// (mtime NkFileSystem::GetLastWriteTime), capture l'état des scripts
				// attachés, décharge, recharge, restaure l'état. À appeler chaque
				// frame (poll léger : un stat par DLL).
				// Retourne le nombre de DLL effectivement rechargées.
				uint32 HotReload(NkWorld &world) noexcept;

				// Crée un adapter (alloué via NKMemory) pour le script chargé `name`.
				[[nodiscard]] NkScriptPtr CreateScript(const char *name) noexcept;

				[[nodiscard]] uint32 DLLCount() const noexcept {
					return mCount;
				}

				[[nodiscard]] const char *GetDLLName(uint32 i) const noexcept {
					return (i < mCount) ? mDLLs[i].name : "";
				}

				[[nodiscard]] const NkScriptDLLInfo *GetDLLInfo(uint32 i) const noexcept {
					return (i < mCount) ? &mDLLs[i].factory.info : nullptr;
				}

				[[nodiscard]] const NkScriptDLLInfo *FindDLLInfo(const char *name) const noexcept {
					for (uint32 i = 0; i < mCount; ++i) {
						if (std::strcmp(mDLLs[i].name, name) == 0) {
							return &mDLLs[i].factory.info;
						}
					}
					return nullptr;
				}

				// Décharge tout (fin d'application). Les hosts doivent être détruits avant.
				void UnloadAll() noexcept;

			private:
				NkScriptLoader() noexcept = default;

				static constexpr uint32 kMaxDLLs = 64u;
				NkLoadedDLL mDLLs[kMaxDLLs] = {};
				uint32 mCount = 0;

				// Charge `path` dans `slot` (shadow copy Windows + résolution des
				// symboles + injection allocateur). N'altère pas mCount.
				bool LoadInto(NkLoadedDLL &slot, const char *path) noexcept;

				// Décharge le module de `slot` et supprime sa shadow copy.
				void UnloadSlot(NkLoadedDLL &slot) noexcept;

				// Recharge la DLL d'index `idx` avec préservation d'état.
				bool ReloadDLL(NkWorld &world, uint32 idx) noexcept;
		};

		// =============================================================================
		// AttachDLLScript — attache un script DLL chargé à un NkScriptHost
		// =============================================================================
		inline bool AttachDLLScript(NkScriptHost &host, const char *scriptName) noexcept {
			if (host.count >= NkScriptHost::kMaxScripts) {
				return false;
			}
			NkScriptPtr script = NkScriptLoader::Global().CreateScript(scriptName);
			if (!script) {
				return false;
			}
			host.scripts[host.count++] = script;
			host.pendingStart = true;
			return true;
		}

	} // namespace ecs
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================
