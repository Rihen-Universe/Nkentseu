#pragma once
// =============================================================================
// FICHIER: NkECS/Scripting/NkScriptBridge.h
// DESCRIPTION: Scripting sans recompilation via chargement dynamique de DLL/SO.
//
// Architecture "hot-reload" style Unity/Unreal :
//
//   1. L'utilisateur écrit ses scripts dans des fichiers .cpp séparés
//   2. Un outil (nkecs-compiler) compile ces .cpp en .dll/.so
//   3. NkScriptLoader charge la .dll à runtime via dlopen/LoadLibrary
//   4. Les scripts exposent leurs fonctions via une ABI C stable
//   5. NkHotReloadWatcher surveille les .dll et recharge si modifiées
//
// L'ABI C stable garantit que les DLL compilées avec n'importe quel
// compilateur C++ fonctionnent sans recompilation du moteur.
//
// ── ABI des scripts ──────────────────────────────────────────────────────────
//
//   Dans chaque DLL de script, on expose :
//     extern "C" {
//         NkScriptDLLInfo    nkecs_script_info();
//         NkScriptInstance*  nkecs_create();
//         void               nkecs_destroy(NkScriptInstance*);
//     }
//
//   NkScriptInstance contient des pointeurs de fonction pour chaque hook.
//
// ── Usage ────────────────────────────────────────────────────────────────────
//
//   // Côté moteur (nkmain.cpp) :
//   NkScriptLoader loader;
//   loader.LoadDirectory("Scripts/");       // charge tous les .dll/.so
//   loader.HotReload();                     // appeler chaque frame (poll)
//
//   // Attacher un script DLL à une entité :
//   NkScriptHost& host = entity.Add<NkScriptHost>();
//   host.AttachDLL("PlayerController");     // nom sans extension
//
//   // Côté script (PlayerController.cpp) :
//   #include "NkECS/Scripting/NkScriptABI.h"
//   class PlayerController : public NkScriptDLL {
//   public:
//       void OnUpdate(float dt) override { ... }
//   };
//   NK_EXPORT_SCRIPT(PlayerController)
//
// =============================================================================

#include "../NkECSDefines.h"
#include "NkScriptComponent.h"
#include <functional>
#include <memory>
#include <vector>
#include <cstring>

#if defined(_WIN32)
#  define NKECS_DLL_EXT ".dll"
#  include <windows.h>
using NkDLLHandle = HMODULE;
#  define NKECS_LOAD_DLL(path)   LoadLibraryA(path)
#  define NKECS_CLOSE_DLL(h)     FreeLibrary(h)
#  define NKECS_FIND_SYMBOL(h,s) GetProcAddress(h, s)
#elif defined(__APPLE__)
#  define NKECS_DLL_EXT ".dylib"
#  include <dlfcn.h>
using NkDLLHandle = void*;
#  define NKECS_LOAD_DLL(path)   dlopen(path, RTLD_NOW | RTLD_LOCAL)
#  define NKECS_CLOSE_DLL(h)     dlclose(h)
#  define NKECS_FIND_SYMBOL(h,s) dlsym(h, s)
#else
#  define NKECS_DLL_EXT ".so"
#  include <dlfcn.h>
using NkDLLHandle = void*;
#  define NKECS_LOAD_DLL(path)   dlopen(path, RTLD_NOW | RTLD_LOCAL)
#  define NKECS_CLOSE_DLL(h)     dlclose(h)
#  define NKECS_FIND_SYMBOL(h,s) dlsym(h, s)
#endif

namespace nkentseu { namespace ecs {

// =============================================================================
// ABI stable C — struct partagée entre moteur et scripts DLL
// =============================================================================

// Contexte passé à chaque fonction de script (pointeurs opaques)
struct NkScriptContext {
    void*  world      = nullptr;  // NkWorld*
    uint64 entityPack = 0;        // NkEntityId::Pack()
    float32 dt        = 0.f;
    float32 fixedDt   = 0.f;
};

// Informations statiques d'un script DLL
struct NkScriptDLLInfo {
    char name[128]    = {};       // nom du script
    char version[32]  = "1.0.0";
    char author[64]   = {};
    bool isThreadSafe = false;
};

// Table de fonctions d'un script (ABI C, stable entre compilateurs)
struct NkScriptVTable {
    void (*OnCreate)          (void* self, NkScriptContext* ctx) = nullptr;
    void (*OnStart)           (void* self, NkScriptContext* ctx) = nullptr;
    void (*OnUpdate)          (void* self, NkScriptContext* ctx) = nullptr;
    void (*OnFixedUpdate)     (void* self, NkScriptContext* ctx) = nullptr;
    void (*OnLateUpdate)      (void* self, NkScriptContext* ctx) = nullptr;
    void (*OnDestroy)         (void* self, NkScriptContext* ctx) = nullptr;
    void (*OnEnable)          (void* self, NkScriptContext* ctx) = nullptr;
    void (*OnDisable)         (void* self, NkScriptContext* ctx) = nullptr;
    void (*OnCollisionEnter)  (void* self, NkScriptContext* ctx, uint64 otherPack) = nullptr;
    void (*OnCollisionExit)   (void* self, NkScriptContext* ctx, uint64 otherPack) = nullptr;
    void (*OnTriggerEnter)    (void* self, NkScriptContext* ctx, uint64 otherPack) = nullptr;
    void (*OnTriggerExit)     (void* self, NkScriptContext* ctx, uint64 otherPack) = nullptr;
    // Sérialisation
    void (*Serialize)         (void* self, char* buf, uint32 bufSize) = nullptr;
    void (*Deserialize)       (void* self, const char* json) = nullptr;
    // Reflection pour l'éditeur
    const char* (*GetFieldsJSON)(void* self) = nullptr;
    void (*SetFieldFromJSON)    (void* self, const char* fieldName, const char* json) = nullptr;
};

// Entrée de factory DLL
struct NkScriptDLLFactory {
    NkScriptDLLInfo  info;
    void*          (*Create)  ()       = nullptr;  // alloue une instance
    void           (*Destroy) (void*)  = nullptr;  // libère une instance
    NkScriptVTable   vtable;
};

// Fonctions exportées par la DLL
using NkScript_GetInfoFn    = NkScriptDLLInfo    (*)();
using NkScript_GetFactoryFn = NkScriptDLLFactory (*)();

// =============================================================================
// NkDLLScriptAdapter — adapte une DLL vers NkScriptComponent
// =============================================================================

class NkDLLScriptAdapter final : public NkScriptComponent {
public:
    NkDLLScriptAdapter(const NkScriptDLLFactory& factory) noexcept
        : mFactory(factory)
    {
        mInstance = factory.Create ? factory.Create() : nullptr;
        std::strncpy(mTypeName, factory.info.name, 127);
    }

    ~NkDLLScriptAdapter() noexcept override {
        if (mInstance && mFactory.Destroy) mFactory.Destroy(mInstance);
    }

    const char* GetTypeName() const noexcept override { return mTypeName; }

    void OnStart(NkWorld& world, NkEntityId self) noexcept override {
        if (auto fn = mFactory.vtable.OnStart) {
            auto ctx = MakeCtx(world, self, 0.f);
            fn(mInstance, &ctx);
        }
    }

    void OnUpdate(NkWorld& world, NkEntityId self, float32 dt) noexcept override {
        if (auto fn = mFactory.vtable.OnUpdate) {
            auto ctx = MakeCtx(world, self, dt);
            fn(mInstance, &ctx);
        }
    }

    void OnFixedUpdate(NkWorld& world, NkEntityId self, float32 fdt) noexcept override {
        if (auto fn = mFactory.vtable.OnFixedUpdate) {
            auto ctx = MakeCtx(world, self, fdt);
            fn(mInstance, &ctx);
        }
    }

    void OnLateUpdate(NkWorld& world, NkEntityId self, float32 dt) noexcept override {
        if (auto fn = mFactory.vtable.OnLateUpdate) {
            auto ctx = MakeCtx(world, self, dt);
            fn(mInstance, &ctx);
        }
    }

    void OnDestroy(NkWorld& world, NkEntityId self) noexcept override {
        if (auto fn = mFactory.vtable.OnDestroy) {
            auto ctx = MakeCtx(world, self, 0.f);
            fn(mInstance, &ctx);
        }
    }

    void Serialize(char* buf, uint32 bufSize) const noexcept override {
        if (auto fn = mFactory.vtable.Serialize)
            fn(mInstance, buf, bufSize);
    }

    void Deserialize(const char* json) noexcept override {
        if (auto fn = mFactory.vtable.Deserialize)
            fn(mInstance, json);
    }

    // Accès à l'état pour le hot-reload : sérialise, puis désérialise dans la nouvelle instance
    void CaptureState(char* buf, uint32 bufSize) const noexcept { Serialize(buf, bufSize); }
    void RestoreState(const char* json)          noexcept       { Deserialize(json); }

private:
    NkScriptDLLFactory mFactory;
    void*              mInstance = nullptr;
    char               mTypeName[128] = {};

    static NkScriptContext MakeCtx(NkWorld& world, NkEntityId id, float32 dt) noexcept {
        return { &world, id.Pack(), dt, dt };
    }
};

// =============================================================================
// NkLoadedDLL — DLL chargée en mémoire
// =============================================================================

struct NkLoadedDLL {
    char            path[512]   = {};
    char            name[128]   = {};  // nom sans extension
    NkDLLHandle     handle      = nullptr;
    NkScriptDLLFactory factory;
    uint64          loadTime    = 0;   // timestamp de chargement

    bool IsValid() const noexcept { return handle != nullptr; }

    void Unload() noexcept {
        if (handle) { NKECS_CLOSE_DLL(handle); handle = nullptr; }
    }
};

// =============================================================================
// NkScriptLoader — charge/décharge/recharge les DLL de scripts
// =============================================================================

class NkScriptLoader {
public:
    static NkScriptLoader& Global() noexcept {
        static NkScriptLoader inst;
        return inst;
    }

    // Charge une DLL depuis son chemin absolu
    bool LoadDLL(const char* path) noexcept {
        if (mCount >= kMaxDLLs) return false;

        NkLoadedDLL dll;
        std::strncpy(dll.path, path, 511);

        dll.handle = NKECS_LOAD_DLL(path);
        if (!dll.handle) return false;

        // Récupère la factory
        auto getFactory = reinterpret_cast<NkScript_GetFactoryFn>(
            NKECS_FIND_SYMBOL(dll.handle, "nkecs_get_factory"));
        if (!getFactory) { dll.Unload(); return false; }

        dll.factory = getFactory();
        std::strncpy(dll.name, dll.factory.info.name, 127);
        dll.loadTime = GetTimestamp();

        mDLLs[mCount++] = dll;
        return true;
    }

    // Charge toutes les DLL d'un dossier
    void LoadDirectory(const char* dir) noexcept {
        // Implementation platform-specific (scan du dossier)
        // Pour l'exemple, on documente l'API
        (void)dir;
        // En production : utiliser opendir/FindFirstFile + charger chaque .dll/.so
    }

    // Décharge une DLL par nom
    void UnloadDLL(const char* name) noexcept {
        for (uint32 i = 0; i < mCount; ++i) {
            if (std::strcmp(mDLLs[i].name, name) == 0) {
                mDLLs[i].Unload();
                mDLLs[i] = mDLLs[--mCount];
                return;
            }
        }
    }

    // Hot-reload : vérifie si une DLL a changé et la recharge
    void HotReload(NkWorld& world) noexcept {
        for (uint32 i = 0; i < mCount; ++i) {
            uint64 mtime = GetFileModTime(mDLLs[i].path);
            if (mtime > mDLLs[i].loadTime) {
                ReloadDLL(world, i);
            }
        }
    }

    // Crée un adapter pour un script DLL
    std::shared_ptr<NkDLLScriptAdapter> CreateScript(const char* name) noexcept {
        for (uint32 i = 0; i < mCount; ++i) {
            if (std::strcmp(mDLLs[i].name, name) == 0) {
                return std::make_shared<NkDLLScriptAdapter>(mDLLs[i].factory);
            }
        }
        return nullptr;
    }

    [[nodiscard]] uint32 DLLCount() const noexcept { return mCount; }
    [[nodiscard]] const char* GetDLLName(uint32 i) const noexcept {
        return (i < mCount) ? mDLLs[i].name : "";
    }

private:
    static constexpr uint32 kMaxDLLs = 256u;
    NkLoadedDLL mDLLs[kMaxDLLs]     = {};
    uint32      mCount               = 0;

    static uint64 GetTimestamp() noexcept {
        // Utiliser std::chrono ou platform time
        return 0; // stub
    }

    static uint64 GetFileModTime(const char* /*path*/) noexcept {
        return 0; // stub — impl platform-specific
    }

    void ReloadDLL(NkWorld& world, uint32 idx) noexcept {
        NkLoadedDLL& old = mDLLs[idx];

        // 1. Sauvegarder l'état de tous les scripts de ce type
        struct StateBackup { NkEntityId id; char state[4096]; };
        std::vector<StateBackup> backups;

        world.Query<NkScriptHost>().ForEach([&](NkEntityId id, NkScriptHost& host) {
            for (uint32 s = 0; s < host.scriptCount; ++s) {
                if (auto* adapter = dynamic_cast<NkDLLScriptAdapter*>(host.scripts[s].get())) {
                    if (std::strcmp(adapter->GetTypeName(), old.name) == 0) {
                        StateBackup backup;
                        backup.id = id;
                        adapter->CaptureState(backup.state, sizeof(backup.state));
                        backups.push_back(backup);
                    }
                }
            }
        });

        // 2. Décharger l'ancienne DLL
        old.Unload();

        // 3. Charger la nouvelle
        char path[512];
        std::strncpy(path, old.path, 511);
        if (!LoadDLL(path)) return; // échec → garder l'ancienne (déjà déchargée)

        NkLoadedDLL& newDLL = mDLLs[mCount - 1];

        // 4. Restaurer les scripts avec le nouvel état
        world.Query<NkScriptHost>().ForEach([&](NkEntityId id, NkScriptHost& host) {
            for (uint32 s = 0; s < host.scriptCount; ++s) {
                if (auto* adapter = dynamic_cast<NkDLLScriptAdapter*>(host.scripts[s].get())) {
                    if (std::strcmp(adapter->GetTypeName(), newDLL.name) == 0) {
                        // Remplace l'instance par une nouvelle
                        host.scripts[s] = std::make_shared<NkDLLScriptAdapter>(newDLL.factory);
                        // Restaure l'état
                        for (const auto& bk : backups) {
                            if (bk.id == id) {
                                static_cast<NkDLLScriptAdapter*>(host.scripts[s].get())
                                    ->RestoreState(bk.state);
                                break;
                            }
                        }
                    }
                }
            }
        });

        // 5. Remplace l'entrée dans le tableau
        mDLLs[idx] = newDLL;
        --mCount; // la nouvelle DLL a été ajoutée à la fin, on la retire
    }
};

// =============================================================================
// MACRO pour exporter un script depuis une DLL
//
// Usage dans PlayerController.cpp :
//   #include "NkECS/Scripting/NkScriptBridge.h"
//
//   class PlayerController : public NkScriptDLLBase { ... };
//   NK_EXPORT_DLL_SCRIPT(PlayerController)
// =============================================================================

// Classe de base helper pour les scripts DLL (simplifie l'accès au monde)
class NkScriptDLLBase {
public:
    virtual ~NkScriptDLLBase() noexcept = default;
    virtual void OnCreate()      noexcept {}
    virtual void OnStart()       noexcept {}
    virtual void OnUpdate(float32 /*dt*/) noexcept {}
    virtual void OnFixedUpdate(float32 /*fdt*/) noexcept {}
    virtual void OnLateUpdate(float32 /*dt*/) noexcept {}
    virtual void OnDestroy()     noexcept {}
    virtual void OnEnable()      noexcept {}
    virtual void OnDisable()     noexcept {}
    virtual void OnCollisionEnter(NkEntityId /*other*/) noexcept {}
    virtual void OnCollisionExit(NkEntityId /*other*/)  noexcept {}
    virtual void OnTriggerEnter(NkEntityId /*other*/)   noexcept {}
    virtual void OnTriggerExit(NkEntityId /*other*/)    noexcept {}

    // Sérialisation (surcharger pour la persistance)
    virtual void Serialize(char* /*buf*/, uint32 /*size*/) const noexcept {}
    virtual void Deserialize(const char* /*json*/) noexcept {}

    // Accès au contexte
    [[nodiscard]] NkWorld*   GetWorld()    const noexcept { return mWorld;  }
    [[nodiscard]] NkEntityId GetEntityId() const noexcept { return mEntity; }
    [[nodiscard]] float32    GetDt()       const noexcept { return mDt;     }

    // Accès aux composants via le monde
    template<typename T>
    [[nodiscard]] T* GetComponent() noexcept {
        return mWorld ? mWorld->Get<T>(mEntity) : nullptr;
    }

    template<typename T>
    T& AddComponent(const T& value = T{}) noexcept {
        return mWorld->Add<T>(mEntity, value);
    }

    template<typename T>
    void RemoveComponent() noexcept {
        if (mWorld) mWorld->Remove<T>(mEntity);
    }

    template<typename T>
    [[nodiscard]] bool HasComponent() const noexcept {
        return mWorld && mWorld->Has<T>(mEntity);
    }

    template<typename T>
    void Emit(const T& event) noexcept {
        if (mWorld) mWorld->Emit(event);
    }

    // Callbacks pour la VTable
    void _SetContext(NkWorld* world, NkEntityId id, float32 dt) noexcept {
        mWorld = world; mEntity = id; mDt = dt;
    }

private:
    NkWorld*   mWorld  = nullptr;
    NkEntityId mEntity = NkEntityId::Invalid();
    float32    mDt     = 0.f;
};

// Macro qui génère l'ABI C pour exporter le script depuis la DLL
#define NK_EXPORT_DLL_SCRIPT(ClassName)                                      \
    static void _nkecs_dispatch(void* self, nkentseu::ecs::NkScriptContext* ctx, \
                                 void (ClassName::*fn)()) {                   \
        auto* s = static_cast<ClassName*>(self);                              \
        s->_SetContext(static_cast<nkentseu::ecs::NkWorld*>(ctx->world),      \
                       nkentseu::ecs::NkEntityId::Unpack(ctx->entityPack),   \
                       ctx->dt);                                              \
        (s->*fn)();                                                           \
    }                                                                         \
    extern "C" {                                                              \
        NKECS_EXPORT nkentseu::ecs::NkScriptDLLFactory nkecs_get_factory() { \
            nkentseu::ecs::NkScriptDLLFactory f;                             \
            std::strncpy(f.info.name, #ClassName, 127);                      \
            f.Create  = []() -> void* { return new ClassName(); };           \
            f.Destroy = [](void* p) { delete static_cast<ClassName*>(p); };  \
            f.vtable.OnStart  = [](void* s, nkentseu::ecs::NkScriptContext* c) { \
                _nkecs_dispatch(s, c, &ClassName::OnStart); };               \
            f.vtable.OnUpdate = [](void* s, nkentseu::ecs::NkScriptContext* c) { \
                auto* sc = static_cast<ClassName*>(s);                        \
                sc->_SetContext(static_cast<nkentseu::ecs::NkWorld*>(c->world), \
                    nkentseu::ecs::NkEntityId::Unpack(c->entityPack), c->dt); \
                sc->OnUpdate(c->dt); };                                       \
            f.vtable.OnDestroy = [](void* s, nkentseu::ecs::NkScriptContext* c) { \
                _nkecs_dispatch(s, c, &ClassName::OnDestroy); };              \
            f.vtable.Serialize = [](void* s, char* b, nkentseu::ecs::uint32 sz) { \
                static_cast<ClassName*>(s)->Serialize(b, sz); };              \
            f.vtable.Deserialize = [](void* s, const char* j) {              \
                static_cast<ClassName*>(s)->Deserialize(j); };                \
            return f;                                                         \
        }                                                                     \
    }

#if defined(_WIN32)
#  define NKECS_EXPORT __declspec(dllexport)
#else
#  define NKECS_EXPORT __attribute__((visibility("default")))
#endif

// Extension de NkScriptHost pour charger les scripts DLL
// (méthode ajoutée via free function pour garder NkScriptHost POD-friendly)
inline bool AttachDLLScript(NkScriptHost& host, const char* scriptName) noexcept {
    auto script = NkScriptLoader::Global().CreateScript(scriptName);
    if (!script) return false;
    if (host.scriptCount >= NkScriptHost::kMaxScripts) return false;
    host.scripts[host.scriptCount++] = script;
    host.pendingStart = true;
    return true;
}

}} // namespace nkentseu::ecs
