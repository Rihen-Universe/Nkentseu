// =============================================================================
// FICHIER: Noge/ECS/Scripting/NkScriptBridge.cpp
// DESCRIPTION: Implémentation du chargeur de scripts DLL avec hot-reload.
//
// - Chargement réel : LoadLibraryA/GetProcAddress (Windows), dlopen/dlsym (POSIX)
// - Windows : shadow copy ("Foo.dll" -> "Foo.dll.hotN") pour que le fichier
//   d'origine reste réécrivable par une recompilation pendant l'exécution
// - Mémoire : instances de scripts allouées via NKMemory (hooks injectés dans
//   la DLL via nkecs_set_allocator) — zéro new/delete bruts
// - Hot-reload : détection mtime (NkFileSystem::GetLastWriteTime, réel et
//   cross-plateforme) -> CaptureState (Serialize) -> déchargement ->
//   rechargement -> RestoreState (Deserialize). En cas d'échec du rechargement
//   (ex. le script ne compile plus), l'ANCIENNE version reste active.
//
// Zéro STL : NkVector/NkString (NKContainers), NkSharedPtr/NkAllocator (NKMemory).
// =============================================================================

#include "NkScriptBridge.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkFileSystem.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NKMemory.h"

#include <cstdio>
#include <cstring>

// Includes plateforme en DERNIER (windows.h pollue les macros — les headers
// moteur doivent être parsés avant).
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace nkentseu {
	namespace ecs {

		// =====================================================================
		// Primitives plateforme (handle opaque NkDLLHandle = void*)
		// =====================================================================
		namespace {

#if defined(_WIN32)
			NkDLLHandle OpenModule(const char *path) noexcept {
				return reinterpret_cast<NkDLLHandle>(::LoadLibraryA(path));
			}

			void CloseModule(NkDLLHandle h) noexcept {
				if (h) {
					::FreeLibrary(reinterpret_cast<HMODULE>(h));
				}
			}

			void *FindSymbol(NkDLLHandle h, const char *name) noexcept {
				return h ? reinterpret_cast<void *>(::GetProcAddress(reinterpret_cast<HMODULE>(h), name)) : nullptr;
			}
#else
			NkDLLHandle OpenModule(const char *path) noexcept {
				return ::dlopen(path, RTLD_NOW | RTLD_LOCAL);
			}

			void CloseModule(NkDLLHandle h) noexcept {
				if (h) {
					::dlclose(h);
				}
			}

			void *FindSymbol(NkDLLHandle h, const char *name) noexcept {
				return h ? ::dlsym(h, name) : nullptr;
			}
#endif

			// Hooks mémoire injectés dans les DLL de scripts : toutes les instances
			// de scripts passent par NKMemory (tracking/leak detection inclus).
			void *NkScriptDLLAllocHook(size_t n) {
				return NK_MEMORY_SYSTEM.Allocate(n, alignof(void *) * 2u, __FILE__, __LINE__, __func__, "dll-script");
			}

			void NkScriptDLLFreeHook(void *p) {
				NK_MEMORY_SYSTEM.Free(p);
			}

		} // namespace

		// =====================================================================
		// NkScriptLoader
		// =====================================================================

		NkScriptLoader &NkScriptLoader::Global() noexcept {
			static NkScriptLoader inst;
			return inst;
		}

		bool NkScriptLoader::LoadInto(NkLoadedDLL &slot, const char *path) noexcept {
			if (!path || !path[0]) {
				return false;
			}
			if (path != slot.path) {
				std::strncpy(slot.path, path, sizeof(slot.path) - 1);
				slot.path[sizeof(slot.path) - 1] = '\0';
			}

			// mtime du fichier d'ORIGINE au moment du chargement : c'est la
			// référence comparée par HotReload() (même source : NkFileSystem).
			slot.fileMTime = static_cast<uint64>(NkFileSystem::GetLastWriteTime(slot.path));

			const char *toLoad = slot.path;
#if defined(_WIN32)
			// Shadow copy : LoadLibrary verrouille le fichier chargé. On charge une
			// copie suffixée pour que la recompilation puisse écraser l'original.
			++slot.generation;
			std::snprintf(slot.shadowPath, sizeof(slot.shadowPath), "%s.hot%u", slot.path, slot.generation);
			if (!NkFile::Copy(slot.path, slot.shadowPath, /*overwrite*/ true)) {
				logger.Error("NkScriptLoader: shadow copy impossible '{0}' -> '{1}'", slot.path, slot.shadowPath);
				slot.shadowPath[0] = '\0';
				return false;
			}
			toLoad = slot.shadowPath;
#endif

			slot.handle = OpenModule(toLoad);
			if (!slot.handle) {
				logger.Error("NkScriptLoader: echec LoadLibrary/dlopen sur '{0}'", toLoad);
				if (slot.shadowPath[0]) {
					NkFile::Delete(slot.shadowPath);
					slot.shadowPath[0] = '\0';
				}
				return false;
			}

			auto getFactory = reinterpret_cast<NkScript_GetFactoryFn>(FindSymbol(slot.handle, "nkecs_get_factory"));
			if (!getFactory) {
				logger.Error("NkScriptLoader: '{0}' n'exporte pas nkecs_get_factory (macro NK_EXPORT_DLL_SCRIPT "
							 "manquante ?)",
							 toLoad);
				UnloadSlot(slot);
				return false;
			}

			// Injection des hooks mémoire NKMemory AVANT toute création d'instance.
			auto setAllocator =
				reinterpret_cast<NkScript_SetAllocatorFn>(FindSymbol(slot.handle, "nkecs_set_allocator"));
			if (setAllocator) {
				setAllocator(&NkScriptDLLAllocHook, &NkScriptDLLFreeHook);
			} else {
				logger.Warn("NkScriptLoader: '{0}' sans nkecs_set_allocator — instances hors NKMemory (fallback "
							"malloc/free de la DLL)",
							toLoad);
			}

			slot.factory = NkScriptDLLFactory{};
			getFactory(&slot.factory);
			if (!slot.factory.Create || !slot.factory.Destroy) {
				logger.Error("NkScriptLoader: factory invalide dans '{0}' (Create/Destroy nuls)", toLoad);
				UnloadSlot(slot);
				return false;
			}
			std::strncpy(slot.name, slot.factory.info.name, sizeof(slot.name) - 1);
			slot.name[sizeof(slot.name) - 1] = '\0';

			logger.Info("NkScriptLoader: '{0}' charge (script '{1}' v{2}, mtime={3})", slot.path, slot.name,
						slot.factory.info.version, slot.fileMTime);
			return true;
		}

		void NkScriptLoader::UnloadSlot(NkLoadedDLL &slot) noexcept {
			if (slot.handle) {
				CloseModule(slot.handle);
				slot.handle = nullptr;
			}
			if (slot.shadowPath[0]) {
				// Peut échouer si l'OS garde un verrou résiduel : sans gravité, la
				// génération suivante utilise un autre suffixe.
				NkFile::Delete(slot.shadowPath);
				slot.shadowPath[0] = '\0';
			}
		}

		bool NkScriptLoader::LoadDLL(const char *path) noexcept {
			if (!path || !path[0]) {
				return false;
			}
			if (mCount >= kMaxDLLs) {
				logger.Error("NkScriptLoader: limite de {0} DLL atteinte, '{1}' ignore", kMaxDLLs, path);
				return false;
			}
			// Déjà chargée ? (même chemin d'origine)
			for (uint32 i = 0; i < mCount; ++i) {
				if (std::strcmp(mDLLs[i].path, path) == 0) {
					return mDLLs[i].IsValid();
				}
			}

			NkLoadedDLL &slot = mDLLs[mCount];
			slot = NkLoadedDLL{};
			if (!LoadInto(slot, path)) {
				return false;
			}
			++mCount;
			return true;
		}

		uint32 NkScriptLoader::LoadDirectory(const char *dir) noexcept {
			if (!dir || !dir[0]) {
				return 0;
			}
			uint32 loaded = 0;
			NkVector<NkString> files = NkDirectory::GetFiles(dir, "*" NKECS_DLL_EXT);
			for (usize i = 0; i < files.Size(); ++i) {
				if (LoadDLL(files[i].CStr())) {
					++loaded;
				}
			}
			logger.Info("NkScriptLoader: {0}/{1} DLL chargees depuis '{2}'", loaded, files.Size(), dir);
			return loaded;
		}

		void NkScriptLoader::UnloadDLL(const char *name) noexcept {
			if (!name) {
				return;
			}
			for (uint32 i = 0; i < mCount; ++i) {
				if (std::strcmp(mDLLs[i].name, name) == 0) {
					UnloadSlot(mDLLs[i]);
					mDLLs[i] = mDLLs[mCount - 1];
					mDLLs[mCount - 1] = NkLoadedDLL{};
					--mCount;
					return;
				}
			}
		}

		void NkScriptLoader::UnloadAll() noexcept {
			for (uint32 i = 0; i < mCount; ++i) {
				UnloadSlot(mDLLs[i]);
				mDLLs[i] = NkLoadedDLL{};
			}
			mCount = 0;
		}

		NkScriptPtr NkScriptLoader::CreateScript(const char *name) noexcept {
			if (!name) {
				return NkScriptPtr();
			}
			for (uint32 i = 0; i < mCount; ++i) {
				if (mDLLs[i].IsValid() && std::strcmp(mDLLs[i].name, name) == 0) {
					memory::NkAllocator &alloc = memory::NkGetDefaultAllocator();
					NkDLLScriptAdapter *raw = alloc.New<NkDLLScriptAdapter>(mDLLs[i].factory);
					if (!raw) {
						return NkScriptPtr();
					}
					if (!raw->IsInstanceValid()) {
						logger.Error("NkScriptLoader: Create() de '{0}' a retourne null", name);
						alloc.Delete(raw);
						return NkScriptPtr();
					}
					return NkScriptPtr(static_cast<NkScriptComponent *>(raw), &alloc);
				}
			}
			logger.Warn("NkScriptLoader: script '{0}' introuvable (DLL non chargee ?)", name);
			return NkScriptPtr();
		}

		uint32 NkScriptLoader::HotReload(NkWorld &world) noexcept {
			uint32 reloaded = 0;
			for (uint32 i = 0; i < mCount; ++i) {
				const uint64 mtime = static_cast<uint64>(NkFileSystem::GetLastWriteTime(mDLLs[i].path));
				// mtime==0 : fichier disparu/inaccessible -> on garde la version
				// chargée. Un mtime DIFFÉRENT de la référence = fichier réécrit.
				if (mtime != 0 && mtime != mDLLs[i].fileMTime) {
					if (ReloadDLL(world, i)) {
						++reloaded;
					}
				}
			}
			return reloaded;
		}

		bool NkScriptLoader::ReloadDLL(NkWorld &world, uint32 idx) noexcept {
			if (idx >= mCount) {
				return false;
			}
			NkLoadedDLL &slot = mDLLs[idx];
			logger.Info("NkScriptLoader: hot-reload de '{0}' (script '{1}')...", slot.path, slot.name);

			// 1. Charger la NOUVELLE version D'ABORD (shadow copy à suffixe distinct,
			//    aucune collision avec l'ancienne encore chargée). Si le rechargement
			//    échoue (ex. DLL corrompue), l'ancienne version reste active.
			NkLoadedDLL fresh;
			std::strncpy(fresh.path, slot.path, sizeof(fresh.path) - 1);
			fresh.generation = slot.generation; // continuité des suffixes .hotN
			if (!LoadInto(fresh, fresh.path)) {
				logger.Error("NkScriptLoader: rechargement de '{0}' RATE — l'ancienne version reste active",
							 slot.path);
				// Consigner le mtime/génération tentés pour ne pas réessayer chaque frame
				// (une nouvelle écriture du fichier re-déclenchera un essai).
				slot.fileMTime = fresh.fileMTime;
				slot.generation = fresh.generation;
				return false;
			}

			// 2. Capturer l'état des scripts de ce type et DÉTRUIRE leurs instances
			//    pendant que l'ancienne DLL est encore chargée (factory.Destroy vit
			//    dans le code de l'ancienne DLL).
			struct StateBackup {
					NkEntityId id;
					uint32 slotIndex = 0;
					char state[4096];
			};

			NkVector<StateBackup> backups;
			world.Query<NkScriptHost>().ForEach([&](NkEntityId id, NkScriptHost &host) {
				for (uint32 s = 0; s < host.count; ++s) {
					auto *adapter = dynamic_cast<NkDLLScriptAdapter *>(host.scripts[s].Get());
					if (adapter && std::strcmp(adapter->GetTypeName(), slot.name) == 0) {
						StateBackup bk;
						bk.id = id;
						bk.slotIndex = s;
						bk.state[0] = '\0';
						adapter->CaptureState(bk.state, sizeof(bk.state));
						backups.PushBack(bk);
						host.scripts[s].Reset(); // détruit l'ancienne instance MAINTENANT
					}
				}
			});

			// 3. Décharger l'ancienne DLL (plus aucune instance vivante) et basculer.
			UnloadSlot(slot);
			slot = fresh;

			// 4. Recréer les instances depuis la NOUVELLE factory et restaurer l'état.
			uint32 restored = 0;
			for (usize b = 0; b < backups.Size(); ++b) {
				NkScriptHost *host = world.Get<NkScriptHost>(backups[b].id);
				if (!host) {
					continue;
				}
				NkScriptPtr script = CreateScript(slot.name);
				if (!script) {
					continue;
				}
				static_cast<NkDLLScriptAdapter *>(script.Get())->RestoreState(backups[b].state);
				const uint32 s = backups[b].slotIndex;
				if (s < host->count) {
					host->scripts[s] = script;
				} else if (host->count < NkScriptHost::kMaxScripts) {
					host->scripts[host->count++] = script;
				}
				// Le nouvel adapter (HasStarted()==false) recevra OnStart au prochain
				// tick — les scripts déjà démarrés ne sont PAS redémarrés.
				host->pendingStart = true;
				++restored;
			}

			logger.Info("NkScriptLoader: '{0}' recharge (v{1}), {2} instance(s) restauree(s), etat preserve",
						slot.name, slot.factory.info.version, restored);
			return true;
		}

	} // namespace ecs
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================
