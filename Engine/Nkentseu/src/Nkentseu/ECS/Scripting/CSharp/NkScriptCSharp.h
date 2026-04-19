#pragma once
// =============================================================================
// FICHIER: NkECS/Scripting/CSharp/NkScriptCSharp.h
// DESCRIPTION: Bridge C# pour NkECS — scripting Mono/CoreCLR.
//
// Architecture :
//   ┌─────────────────┐    P/Invoke + Mono    ┌──────────────────────┐
//   │   Moteur (C++)  │ ←──────────────────── │  CLR (Mono/CoreCLR)  │
//   │   NkWorld       │    NkCSContext         │  Scripts .cs/.dll    │
//   │   NkEntity      │                        │  NkECS.Runtime.dll   │
//   └─────────────────┘                        └──────────────────────┘
//
// Workflow :
//   1. NkCSharpBridge::Init("NkECS.Runtime.dll")  — charge le runtime Mono
//   2. Compiler les scripts C# : nkecs-csc Scripts/*.cs → Scripts.dll
//   3. NkCSharpBridge::LoadAssembly("Scripts.dll")
//   4. host.AttachCSharp("MyNamespace.PlayerScript")
//   5. Hot-reload : NkCSharpBridge::ReloadAssembly("Scripts.dll")
//
// Format d'un script C# :
//   // Scripts/PlayerController.cs
//   using NkECS;
//   namespace MyGame {
//       public class PlayerController : NkScript {
//           float speed = 5f;
//           public override void OnStart(NkContext ctx)   { }
//           public override void OnUpdate(NkContext ctx, float dt) {
//               var t = ctx.GetTransform();
//               t.Position += new Vector3(Input.Horizontal * speed * dt, 0, 0);
//           }
//           public override void OnCollisionEnter(NkContext ctx, ulong otherId) { }
//       }
//   }
//
// ⚠️  Dépendances : Mono (libmono-2.0-dev) ou .NET CoreCLR
//     CMake : find_package(Mono) ou configure manuellement
// =============================================================================

#include "../../NkECSDefines.h"
#include "../NkScriptComponent.h"
#include <functional>
#include <memory>
#include <cstring>

// Détection de Mono
#ifdef NKECS_MONO_AVAILABLE
#  include <mono/jit/jit.h>
#  include <mono/metadata/assembly.h>
#  include <mono/metadata/debug-helpers.h>
#  include <mono/metadata/mono-config.h>
#  include <mono/metadata/exception.h>
using NkMonoDomain   = MonoDomain;
using NkMonoAssembly = MonoAssembly;
using NkMonoImage    = MonoImage;
using NkMonoClass    = MonoClass;
using NkMonoObject   = MonoObject;
using NkMonoMethod   = MonoMethod;
#else
// Stubs pour compilation sans Mono
using NkMonoDomain   = void;
using NkMonoAssembly = void;
using NkMonoImage    = void;
using NkMonoClass    = void;
using NkMonoObject   = void;
using NkMonoMethod   = void;
#endif

namespace nkentseu { namespace ecs {

// =============================================================================
// NkCSContext — contexte C# (struct P/Invoke côté C++)
// Mappé côté C# comme struct NkContext
// =============================================================================

// Tous les champs sont blittables (pas de GC overhead)
struct alignas(8) NkCSContext {
    void*  worldPtr    = nullptr;   // NkWorld* (opaque côté C#)
    uint64 entityPack  = 0;         // NkEntityId::Pack()
    float32 dt         = 0.f;
    float32 fixedDt    = 0.f;
    float32 posX = 0, posY = 0, posZ = 0;
    float32 rotX = 0, rotY = 0, rotZ = 0, rotW = 1;
    float32 sclX = 1, sclY = 1, sclZ = 1;
    // Sortie (le script C# écrit ici pour communiquer avec le moteur)
    float32 outPosX = 0, outPosY = 0, outPosZ = 0;
    uint32  dirtyFlags = 0;  // bit 0 = position dirty, bit 1 = rotation, etc.
};

// =============================================================================
// NkCSHostFunctions — fonctions C exposées au C# via P/Invoke
// Enregistrées avec mono_add_internal_call()
// =============================================================================

namespace CSharpInternals {

    // Appelées depuis C# via [DllImport("__Internal")]
    extern "C" {
        // Gestion des entités
        static uint64 NkCS_CreateEntity(void* worldPtr, const char* name) {
            auto* world = static_cast<NkWorld*>(worldPtr);
            if (!world) return 0;
            NkEntityId id = world->CreateEntity();
            if (name) world->Add<NkName>(id, NkName(name));
            world->Add<NkTransform>(id);
            world->Add<NkTag>(id);
            return id.Pack();
        }

        static void NkCS_DestroyEntity(void* worldPtr, uint64 entityPack) {
            auto* world = static_cast<NkWorld*>(worldPtr);
            if (!world) return;
            world->DestroyDeferred(NkEntityId::Unpack(entityPack));
        }

        static int32 NkCS_IsAlive(void* worldPtr, uint64 entityPack) {
            auto* world = static_cast<NkWorld*>(worldPtr);
            return world && world->IsAlive(NkEntityId::Unpack(entityPack)) ? 1 : 0;
        }

        // Transform
        static void NkCS_GetTransform(void* worldPtr, uint64 entityPack,
                                       float32* outX, float32* outY, float32* outZ) {
            auto* world = static_cast<NkWorld*>(worldPtr);
            if (!world || !outX) return;
            auto id = NkEntityId::Unpack(entityPack);
            if (auto* t = world->Get<NkTransform>(id)) {
                *outX = t->position.x; *outY = t->position.y; *outZ = t->position.z;
            }
        }

        static void NkCS_SetPosition(void* worldPtr, uint64 entityPack,
                                      float32 x, float32 y, float32 z) {
            auto* world = static_cast<NkWorld*>(worldPtr);
            if (!world) return;
            auto id = NkEntityId::Unpack(entityPack);
            if (auto* t = world->Get<NkTransform>(id)) {
                t->position = {x, y, z};
                t->dirty = true;
            }
        }

        static void NkCS_Translate(void* worldPtr, uint64 entityPack,
                                    float32 dx, float32 dy, float32 dz) {
            auto* world = static_cast<NkWorld*>(worldPtr);
            if (!world) return;
            auto id = NkEntityId::Unpack(entityPack);
            if (auto* t = world->Get<NkTransform>(id)) {
                t->position.x += dx; t->position.y += dy; t->position.z += dz;
                t->dirty = true;
            }
        }

        static void NkCS_Rotate(void* worldPtr, uint64 entityPack,
                                  float32 axX, float32 axY, float32 axZ, float32 angleDeg) {
            auto* world = static_cast<NkWorld*>(worldPtr);
            if (!world) return;
            auto id = NkEntityId::Unpack(entityPack);
            if (auto* t = world->Get<NkTransform>(id)) {
                t->Rotate({axX, axY, axZ}, angleDeg);
            }
        }

        static void NkCS_LookAt(void* worldPtr, uint64 entityPack,
                                  float32 tx, float32 ty, float32 tz) {
            auto* world = static_cast<NkWorld*>(worldPtr);
            if (!world) return;
            auto id = NkEntityId::Unpack(entityPack);
            if (auto* t = world->Get<NkTransform>(id)) {
                t->LookAt({tx, ty, tz});
            }
        }

        static uint64 NkCS_GetParent(void* worldPtr, uint64 entityPack) {
            auto* world = static_cast<NkWorld*>(worldPtr);
            if (!world) return 0;
            auto id = NkEntityId::Unpack(entityPack);
            if (auto* t = world->Get<NkTransform>(id))
                return t->parent.Pack();
            return 0;
        }

        // Events
        static void NkCS_EmitPlayerDamaged(void* worldPtr, float32 damage) {
            // Exemple — en production, généraliser via un nom d'event
            (void)worldPtr; (void)damage;
        }

        // Log
        static void NkCS_Log(const char* msg) {
            if (msg) { /* NkLog(msg); */ }
        }
    } // extern "C"

} // namespace CSharpInternals

// =============================================================================
// NkCSharpBridge — gestion du runtime Mono/CoreCLR
// =============================================================================

class NkCSharpBridge {
public:
    static NkCSharpBridge& Global() noexcept {
        static NkCSharpBridge inst;
        return inst;
    }

    // ── Initialisation ────────────────────────────────────────────────────────

    bool Init(const char* runtimeDllPath = nullptr) noexcept {
#ifdef NKECS_MONO_AVAILABLE
        if (mInitialized) return true;

        // Configure Mono
        mono_config_parse(nullptr);
        mDomain = mono_jit_init("NkECS");
        if (!mDomain) return false;

        // Enregistre les fonctions internes (P/Invoke vers C++)
        RegisterInternals();

        // Charge l'assembly runtime NkECS
        if (runtimeDllPath) {
            if (!LoadAssembly(runtimeDllPath)) {
                // Le runtime DLL peut ne pas exister → warning seulement
            }
        }

        mInitialized = true;
        return true;
#else
        (void)runtimeDllPath; return false;
#endif
    }

    void Shutdown() noexcept {
#ifdef NKECS_MONO_AVAILABLE
        if (!mInitialized) return;
        if (mDomain) mono_jit_cleanup(mDomain);
        mDomain = nullptr;
        mInitialized = false;
#endif
    }

    [[nodiscard]] bool IsInitialized() const noexcept { return mInitialized; }

    // ── Chargement d'assemblies ───────────────────────────────────────────────

    bool LoadAssembly(const char* path) noexcept {
#ifdef NKECS_MONO_AVAILABLE
        if (!mInitialized || mAssemblyCount >= kMaxAssemblies) return false;

        MonoAssembly* assembly = mono_domain_assembly_open(mDomain, path);
        if (!assembly) return false;

        mAssemblies[mAssemblyCount] = assembly;
        std::strncpy(mAssemblyPaths[mAssemblyCount], path, 511);
        ++mAssemblyCount;
        return true;
#else
        (void)path; return false;
#endif
    }

    bool ReloadAssembly(const char* path) noexcept {
#ifdef NKECS_MONO_AVAILABLE
        // Mono ne supporte pas le vrai hot-reload → créer un nouveau domaine
        // En production : shadow-copy + AppDomain swap
        // Ici : rechargement simplifié
        return LoadAssembly(path);
#else
        (void)path; return false;
#endif
    }

    void HotReload(NkWorld& world) noexcept {
        // Vérifier timestamps des .dll et recharger si modifiées
        (void)world;
    }

    // ── Création d'instances ──────────────────────────────────────────────────

    std::shared_ptr<NkScriptComponent> CreateCSharpScript(
        const char* fullClassName) noexcept
    {
#ifdef NKECS_MONO_AVAILABLE
        if (!mInitialized) return nullptr;

        // Cherche la classe dans toutes les assemblies chargées
        for (uint32 i = 0; i < mAssemblyCount; ++i) {
            MonoImage* image = mono_assembly_get_image(mAssemblies[i]);
            // fullClassName = "MyNamespace.PlayerController"
            char ns[128] = {}, cls[128] = {};
            const char* dot = std::strrchr(fullClassName, '.');
            if (dot) {
                uint32 nsLen = static_cast<uint32>(dot - fullClassName);
                std::strncpy(ns, fullClassName, nsLen);
                std::strncpy(cls, dot + 1, 127);
            } else {
                std::strncpy(cls, fullClassName, 127);
            }

            MonoClass* monoClass = mono_class_from_name(image, ns, cls);
            if (!monoClass) continue;

            MonoObject* obj = mono_object_new(mDomain, monoClass);
            if (!obj) continue;
            mono_runtime_object_init(obj);

            return std::make_shared<NkCSharpScriptAdapter>(obj, monoClass, fullClassName);
        }
        return nullptr;
#else
        (void)fullClassName; return nullptr;
#endif
    }

private:
    void RegisterInternals() noexcept {
#ifdef NKECS_MONO_AVAILABLE
        using namespace CSharpInternals;
        #define NKCS_REG(name) mono_add_internal_call("NkECS.Internal::" #name, (void*)&name)
        NKCS_REG(NkCS_CreateEntity);
        NKCS_REG(NkCS_DestroyEntity);
        NKCS_REG(NkCS_IsAlive);
        NKCS_REG(NkCS_GetTransform);
        NKCS_REG(NkCS_SetPosition);
        NKCS_REG(NkCS_Translate);
        NKCS_REG(NkCS_Rotate);
        NKCS_REG(NkCS_LookAt);
        NKCS_REG(NkCS_GetParent);
        NKCS_REG(NkCS_EmitPlayerDamaged);
        NKCS_REG(NkCS_Log);
        #undef NKCS_REG
#endif
    }

    static constexpr uint32 kMaxAssemblies = 32u;
    NkMonoDomain*   mDomain = nullptr;
    NkMonoAssembly* mAssemblies[kMaxAssemblies] = {};
    char            mAssemblyPaths[kMaxAssemblies][512] = {};
    uint32          mAssemblyCount = 0;
    bool            mInitialized   = false;
};

// =============================================================================
// NkCSharpScriptAdapter — adapte un MonoObject → NkScriptComponent
// =============================================================================

class NkCSharpScriptAdapter final : public NkScriptComponent {
public:
    NkCSharpScriptAdapter(NkMonoObject* obj, NkMonoClass* cls,
                           const char* typeName) noexcept
        : mObject(obj), mClass(cls)
    {
        std::strncpy(mTypeName, typeName, 127);
#ifdef NKECS_MONO_AVAILABLE
        // Pré-résout les méthodes pour éviter les lookups répétés
        mOnStart        = FindMethod("OnStart");
        mOnUpdate       = FindMethod("OnUpdate");
        mOnFixedUpdate  = FindMethod("OnFixedUpdate");
        mOnLateUpdate   = FindMethod("OnLateUpdate");
        mOnDestroy      = FindMethod("OnDestroy");
        mOnCollEnter    = FindMethod("OnCollisionEnter");
        mSerialize      = FindMethod("Serialize");
        mDeserialize    = FindMethod("Deserialize");
#endif
    }

    const char* GetTypeName() const noexcept override { return mTypeName; }

    void OnStart(NkWorld& world, NkEntityId self) noexcept override {
        InvokeVoid(mOnStart, world, self, 0.f);
    }
    void OnUpdate(NkWorld& world, NkEntityId self, float32 dt) noexcept override {
        InvokeVoid(mOnUpdate, world, self, dt);
    }
    void OnFixedUpdate(NkWorld& world, NkEntityId self, float32 fdt) noexcept override {
        InvokeVoid(mOnFixedUpdate, world, self, fdt);
    }
    void OnLateUpdate(NkWorld& world, NkEntityId self, float32 dt) noexcept override {
        InvokeVoid(mOnLateUpdate, world, self, dt);
    }
    void OnDestroy(NkWorld& world, NkEntityId self) noexcept override {
        InvokeVoid(mOnDestroy, world, self, 0.f);
    }
    void OnCollisionEnter(NkWorld& world, NkEntityId self,
                          NkEntityId other) noexcept override {
        InvokeCollision(mOnCollEnter, world, self, other);
    }

    void Serialize(char* buf, uint32 bufSize) const noexcept override {
        (void)buf; (void)bufSize;
        // TODO: invoquer Serialize() et récupérer la string
    }

    void Deserialize(const char* json) noexcept override {
        (void)json;
        // TODO: invoquer Deserialize(string)
    }

private:
    NkMonoObject* mObject = nullptr;
    NkMonoClass*  mClass  = nullptr;
    NkMonoMethod* mOnStart = nullptr, *mOnUpdate = nullptr;
    NkMonoMethod* mOnFixedUpdate = nullptr, *mOnLateUpdate = nullptr;
    NkMonoMethod* mOnDestroy = nullptr, *mOnCollEnter = nullptr;
    NkMonoMethod* mSerialize = nullptr, *mDeserialize = nullptr;
    char          mTypeName[128] = {};

#ifdef NKECS_MONO_AVAILABLE
    NkMonoMethod* FindMethod(const char* name) noexcept {
        if (!mClass) return nullptr;
        return mono_class_get_method_from_name(mClass, name, -1);
    }
#else
    NkMonoMethod* FindMethod(const char*) noexcept { return nullptr; }
#endif

    void InvokeVoid(NkMonoMethod* method, NkWorld& world,
                    NkEntityId self, float32 dt) noexcept {
#ifdef NKECS_MONO_AVAILABLE
        if (!method || !mObject) return;
        // Construit le NkCSContext et le passe comme argument
        NkCSContext ctx;
        ctx.worldPtr   = &world;
        ctx.entityPack = self.Pack();
        ctx.dt         = dt;
        // Remplit position depuis le transform
        if (auto* t = world.Get<NkTransform>(self)) {
            ctx.posX = t->position.x; ctx.posY = t->position.y; ctx.posZ = t->position.z;
        }
        // Invoque la méthode Mono
        void* args[] = { &ctx };
        MonoObject* exc = nullptr;
        mono_runtime_invoke(method, mObject, args, &exc);
        if (exc) {
            // Log l'exception
            MonoString* msg = mono_object_to_string(exc, nullptr);
            if (msg) {
                char* s = mono_string_to_utf8(msg);
                (void)s; // NkLog(s); mono_free(s);
            }
        }
        // Applique les changements de position
        if ((ctx.dirtyFlags & 1) && !(ctx.outPosX == 0 && ctx.outPosY == 0 && ctx.outPosZ == 0)) {
            if (auto* t = world.Get<NkTransform>(self)) {
                t->position = {ctx.outPosX, ctx.outPosY, ctx.outPosZ};
                t->dirty = true;
            }
        }
#else
        (void)method; (void)world; (void)self; (void)dt;
#endif
    }

    void InvokeCollision(NkMonoMethod* method, NkWorld& world,
                          NkEntityId self, NkEntityId other) noexcept {
        (void)method; (void)world; (void)self; (void)other;
    }
};

// Extension de NkScriptHost
inline bool AttachCSharpScript(NkScriptHost& host, const char* fullClassName) noexcept {
    auto script = NkCSharpBridge::Global().CreateCSharpScript(fullClassName);
    if (!script) return false;
    if (host.scriptCount >= NkScriptHost::kMaxScripts) return false;
    host.scripts[host.scriptCount++] = script;
    host.pendingStart = true;
    return true;
}

}} // namespace nkentseu::ecs

// =============================================================================
// EXEMPLE DE SCRIPT C# (à compiler en Scripts.dll)
// =============================================================================
/*

// Scripts/PlayerController.cs
using System;
using System.Runtime.InteropServices;
using NkECS;

namespace MyGame
{
    [StructLayout(LayoutKind.Sequential)]
    public struct NkContext
    {
        public IntPtr  WorldPtr;
        public ulong   EntityPack;
        public float   Dt;
        public float   FixedDt;
        public float   PosX, PosY, PosZ;
        public float   RotX, RotY, RotZ, RotW;
        public float   SclX, SclY, SclZ;
        public float   OutPosX, OutPosY, OutPosZ;
        public uint    DirtyFlags;
    }

    public static class Internal
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void NkCS_SetPosition(IntPtr world, ulong entity, float x, float y, float z);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void NkCS_Translate(IntPtr world, ulong entity, float dx, float dy, float dz);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void NkCS_Log(string msg);
    }

    public class PlayerController : NkScript
    {
        float speed = 5f;
        float jumpForce = 8f;
        bool  isGrounded = true;
        float velocityY = 0f;

        public override void OnStart(ref NkContext ctx)
        {
            Internal.NkCS_Log($"[C#] PlayerController démarré sur {ctx.EntityPack}");
        }

        public override void OnUpdate(ref NkContext ctx, float dt)
        {
            // Gravité
            if (!isGrounded) velocityY -= 20f * dt;
            velocityY = Math.Max(velocityY, -30f);

            // Collision sol simple
            if (ctx.PosY <= 0f) { isGrounded = true; velocityY = 0f; }

            // Déplacement
            float inputX = GetInput(); // à brancher sur le vrai système d'input
            Internal.NkCS_Translate(ctx.WorldPtr, ctx.EntityPack,
                inputX * speed * dt, velocityY * dt, 0f);
        }

        private float GetInput() => 0f; // stub
    }

    public class CameraOrbit : NkScript
    {
        public float Distance = 8f;
        public float Height   = 3f;
        public float Speed    = 90f;   // degrés/seconde
        float angle = 0f;

        public override void OnLateUpdate(ref NkContext ctx, float dt)
        {
            angle += Speed * dt;
            float rad = angle * MathF.PI / 180f;
            float x   = ctx.PosX + MathF.Sin(rad) * Distance;
            float z   = ctx.PosZ + MathF.Cos(rad) * Distance;
            Internal.NkCS_SetPosition(ctx.WorldPtr, ctx.EntityPack, x, ctx.PosY + Height, z);
        }
    }
}

*/
