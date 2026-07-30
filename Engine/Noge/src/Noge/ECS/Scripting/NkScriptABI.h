#pragma once
// =============================================================================
// FICHIER: Noge/ECS/Scripting/NkScriptABI.h
// DESCRIPTION: ABI C stable partagée entre le moteur et les scripts DLL.
//
// Ce header est AUTONOME (standalone) : il ne dépend d'AUCUN autre header du
// moteur, uniquement de la libc (<stdint.h>, <string.h>, ...). C'est voulu :
// un script utilisateur (.cpp) doit pouvoir être compilé en .dll/.so avec un
// simple appel compilateur :
//
//   clang++ -shared -std=c++17 -I <racine>/Engine/Noge/src \
//           -o MonScript.dll MonScript.cpp
//
// sans linker la moindre bibliothèque du moteur. Toute la communication
// moteur <-> script passe par les structs POD + pointeurs de fonction
// ci-dessous (ABI C stable entre compilateurs).
//
// ── Côté script (MonScript.cpp) ──────────────────────────────────────────────
//
//   #include "Noge/ECS/Scripting/NkScriptABI.h"
//
//   class MonScript : public nkentseu::ecs::NkScriptDLLBase {
//   public:
//       void OnUpdate(float dt) noexcept override { ... }
//       void Serialize(char *buf, uint32_t size) const noexcept override { ... }
//       void Deserialize(const char *json) noexcept override { ... }
//   };
//   NK_EXPORT_DLL_SCRIPT(MonScript)                        // version "1.0.0"
//   // ou : NK_EXPORT_DLL_SCRIPT_VERSIONED(MonScript, "2.0.0")
//
// ── Mémoire (convention zéro new/delete bruts) ───────────────────────────────
//
// La DLL exporte `nkecs_set_allocator(alloc, free)`. Le moteur (NkScriptLoader)
// l'appelle juste après le chargement pour brancher NKMemory : dès lors, TOUTES
// les instances de scripts sont allouées/libérées via NKMemory (placement new
// dans un bloc alloué par le hook + destructeur explicite + free du hook).
// Sans moteur (tests standalone), repli sur malloc/free — jamais new/delete.
// =============================================================================

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <new>

#if defined(_WIN32)
#define NKECS_ABI_EXPORT __declspec(dllexport)
#else
#define NKECS_ABI_EXPORT __attribute__((visibility("default")))
#endif

namespace nkentseu {
	namespace ecs {

		// =====================================================================
		// Structs ABI (POD, layout C stable — ne pas réordonner les champs)
		// =====================================================================

		// Contexte passé à chaque hook de script (pointeurs opaques côté script)
		struct NkScriptContext {
				void *world = nullptr;	 // NkWorld* (opaque pour la DLL)
				uint64_t entityPack = 0; // NkEntityId::Pack()
				float dt = 0.f;
				float fixedDt = 0.f;
		};

		// Informations statiques d'un script DLL
		struct NkScriptDLLInfo {
				char name[128] = {};
				char version[32] = "1.0.0";
				char author[64] = {};
				bool isThreadSafe = false;
		};

		// Table de fonctions d'un script (ABI C, stable entre compilateurs)
		struct NkScriptVTable {
				void (*OnCreate)(void *self, NkScriptContext *ctx) = nullptr;
				void (*OnStart)(void *self, NkScriptContext *ctx) = nullptr;
				void (*OnUpdate)(void *self, NkScriptContext *ctx) = nullptr;
				void (*OnFixedUpdate)(void *self, NkScriptContext *ctx) = nullptr;
				void (*OnLateUpdate)(void *self, NkScriptContext *ctx) = nullptr;
				void (*OnDestroy)(void *self, NkScriptContext *ctx) = nullptr;
				void (*OnEnable)(void *self, NkScriptContext *ctx) = nullptr;
				void (*OnDisable)(void *self, NkScriptContext *ctx) = nullptr;
				void (*OnCollisionEnter)(void *self, NkScriptContext *ctx, uint64_t otherPack) = nullptr;
				void (*OnCollisionExit)(void *self, NkScriptContext *ctx, uint64_t otherPack) = nullptr;
				void (*OnTriggerEnter)(void *self, NkScriptContext *ctx, uint64_t otherPack) = nullptr;
				void (*OnTriggerExit)(void *self, NkScriptContext *ctx, uint64_t otherPack) = nullptr;
				// Sérialisation (hot-reload : capture/restauration d'état)
				void (*Serialize)(void *self, char *buf, uint32_t bufSize) = nullptr;
				void (*Deserialize)(void *self, const char *json) = nullptr;
				// Reflection pour l'éditeur (optionnel, peut rester nullptr)
				const char *(*GetFieldsJSON)(void *self) = nullptr;
				void (*SetFieldFromJSON)(void *self, const char *fieldName, const char *json) = nullptr;
		};

		// Hooks mémoire injectés par le moteur (NKMemory) via nkecs_set_allocator
		using NkScriptAllocFn = void *(*)(size_t size);
		using NkScriptFreeFn = void (*)(void *ptr);

		// Entrée de factory exportée par la DLL
		struct NkScriptDLLFactory {
				NkScriptDLLInfo info;
				void *(*Create)() = nullptr;	   // alloue + construit une instance
				void (*Destroy)(void *) = nullptr; // détruit + libère une instance
				NkScriptVTable vtable;
		};

		// Signatures des fonctions exportées par la DLL.
		// nkecs_get_factory remplit un paramètre de sortie (pas de retour de
		// struct C++ par valeur à travers une frontière extern "C" — ABI C pure).
		using NkScript_GetFactoryFn = void (*)(NkScriptDLLFactory *outFactory);
		using NkScript_SetAllocatorFn = void (*)(NkScriptAllocFn allocFn, NkScriptFreeFn freeFn);

		// =====================================================================
		// NkScriptDLLBase — classe de base côté script (autonome, zéro moteur)
		// =====================================================================
		// Le monde est opaque (void*) côté DLL : un script qui veut manipuler
		// des composants passe par les hooks/événements du moteur, pas par un
		// accès direct (l'ABI reste stable et la DLL reste autonome).
		class NkScriptDLLBase {
			public:
				virtual ~NkScriptDLLBase() noexcept = default;

				virtual void OnCreate() noexcept {
				}

				virtual void OnStart() noexcept {
				}

				virtual void OnUpdate(float /*dt*/) noexcept {
				}

				virtual void OnFixedUpdate(float /*fdt*/) noexcept {
				}

				virtual void OnLateUpdate(float /*dt*/) noexcept {
				}

				virtual void OnDestroy() noexcept {
				}

				virtual void OnEnable() noexcept {
				}

				virtual void OnDisable() noexcept {
				}

				// Sérialisation (surcharger pour préserver l'état au hot-reload)
				virtual void Serialize(char * /*buf*/, uint32_t /*size*/) const noexcept {
				}

				virtual void Deserialize(const char * /*json*/) noexcept {
				}

				// Accès au contexte courant (rafraîchi avant chaque hook)
				[[nodiscard]] void *GetWorldRaw() const noexcept {
					return mWorld;
				}

				[[nodiscard]] uint64_t GetEntityPack() const noexcept {
					return mEntityPack;
				}

				[[nodiscard]] float GetDt() const noexcept {
					return mDt;
				}

				// Appelé par la glue générée (NK_EXPORT_DLL_SCRIPT)
				void _SetContext(void *world, uint64_t entityPack, float dt) noexcept {
					mWorld = world;
					mEntityPack = entityPack;
					mDt = dt;
				}

			private:
				void *mWorld = nullptr;
				uint64_t mEntityPack = 0;
				float mDt = 0.f;
		};

	} // namespace ecs
} // namespace nkentseu

// =============================================================================
// MACROS d'export — génèrent l'ABI C de la DLL
// =============================================================================
// Zéro new/delete bruts : les instances passent par les hooks mémoire injectés
// (NKMemory via NkScriptLoader) avec repli malloc/free (placement new +
// destructeur explicite dans les deux cas).
// =============================================================================

#define NK_EXPORT_DLL_SCRIPT_VERSIONED(ClassName, VersionStr)                                                          \
	namespace {                                                                                                        \
		::nkentseu::ecs::NkScriptAllocFn g_nkecsHookAlloc = nullptr;                                                   \
		::nkentseu::ecs::NkScriptFreeFn g_nkecsHookFree = nullptr;                                                     \
		void *NkecsRawAlloc(size_t n) noexcept {                                                                       \
			return g_nkecsHookAlloc ? g_nkecsHookAlloc(n) : ::malloc(n);                                               \
		}                                                                                                              \
		void NkecsRawFree(void *p) noexcept {                                                                          \
			if (!p)                                                                                                    \
				return;                                                                                                \
			if (g_nkecsHookFree)                                                                                       \
				g_nkecsHookFree(p);                                                                                    \
			else                                                                                                       \
				::free(p);                                                                                             \
		}                                                                                                              \
		inline ClassName *NkecsSelf(void *s, ::nkentseu::ecs::NkScriptContext *c) noexcept {                           \
			auto *p = static_cast<ClassName *>(s);                                                                     \
			if (c)                                                                                                     \
				p->_SetContext(c->world, c->entityPack, c->dt);                                                        \
			return p;                                                                                                  \
		}                                                                                                              \
	}                                                                                                                  \
	extern "C" {                                                                                                       \
	NKECS_ABI_EXPORT void nkecs_set_allocator(::nkentseu::ecs::NkScriptAllocFn allocFn,                                \
											  ::nkentseu::ecs::NkScriptFreeFn freeFn) {                                \
		g_nkecsHookAlloc = allocFn;                                                                                    \
		g_nkecsHookFree = freeFn;                                                                                      \
	}                                                                                                                  \
	NKECS_ABI_EXPORT void nkecs_get_factory(::nkentseu::ecs::NkScriptDLLFactory *outFactory) {                          \
		using namespace ::nkentseu::ecs;                                                                               \
		if (!outFactory)                                                                                               \
			return;                                                                                                    \
		NkScriptDLLFactory f;                                                                                          \
		::strncpy(f.info.name, #ClassName, sizeof(f.info.name) - 1);                                                   \
		::strncpy(f.info.version, VersionStr, sizeof(f.info.version) - 1);                                             \
		f.Create = []() -> void * {                                                                                    \
			void *mem = NkecsRawAlloc(sizeof(ClassName));                                                              \
			if (!mem)                                                                                                  \
				return nullptr;                                                                                        \
			return static_cast<void *>(new (mem) ClassName());                                                         \
		};                                                                                                             \
		f.Destroy = [](void *p) {                                                                                      \
			if (!p)                                                                                                    \
				return;                                                                                                \
			auto *s = static_cast<ClassName *>(p);                                                                     \
			s->~ClassName();                                                                                           \
			NkecsRawFree(p);                                                                                           \
		};                                                                                                             \
		f.vtable.OnCreate = [](void *s, NkScriptContext *c) { NkecsSelf(s, c)->OnCreate(); };                          \
		f.vtable.OnStart = [](void *s, NkScriptContext *c) { NkecsSelf(s, c)->OnStart(); };                            \
		f.vtable.OnUpdate = [](void *s, NkScriptContext *c) { NkecsSelf(s, c)->OnUpdate(c ? c->dt : 0.f); };           \
		f.vtable.OnFixedUpdate = [](void *s, NkScriptContext *c) {                                                     \
			NkecsSelf(s, c)->OnFixedUpdate(c ? c->fixedDt : 0.f);                                                      \
		};                                                                                                             \
		f.vtable.OnLateUpdate = [](void *s, NkScriptContext *c) { NkecsSelf(s, c)->OnLateUpdate(c ? c->dt : 0.f); };   \
		f.vtable.OnDestroy = [](void *s, NkScriptContext *c) { NkecsSelf(s, c)->OnDestroy(); };                        \
		f.vtable.OnEnable = [](void *s, NkScriptContext *c) { NkecsSelf(s, c)->OnEnable(); };                          \
		f.vtable.OnDisable = [](void *s, NkScriptContext *c) { NkecsSelf(s, c)->OnDisable(); };                        \
		f.vtable.Serialize = [](void *s, char *b, uint32_t sz) { static_cast<ClassName *>(s)->Serialize(b, sz); };     \
		f.vtable.Deserialize = [](void *s, const char *j) { static_cast<ClassName *>(s)->Deserialize(j); };            \
		*outFactory = f;                                                                                               \
	}                                                                                                                  \
	}

#define NK_EXPORT_DLL_SCRIPT(ClassName) NK_EXPORT_DLL_SCRIPT_VERSIONED(ClassName, "1.0.0")

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================
