#pragma once
// =============================================================================
// FICHIER: NkECS/Scripting/Python/NkScriptPython.h
// DESCRIPTION: Bridge Python pour NkECS — scripting rapide sans recompilation.
//
// Architecture :
//   ┌─────────────────┐    C ABI stable    ┌──────────────────┐
//   │   Moteur (C++)  │ ←────────────────→ │  Interpréteur    │
//   │   NkWorld       │    NkPyContext      │  Python 3.x      │
//   │   NkEntity      │                    │  Scripts .py     │
//   └─────────────────┘                    └──────────────────┘
//
// Workflow :
//   1. Démarrer l'interpréteur : NkPythonBridge::Init()
//   2. Exposer l'API NkECS à Python via NkPyModule
//   3. Charger un script : NkPythonBridge::LoadScript("Scripts/Enemy.py")
//   4. Attacher à une entité : host.AttachPython("EnemyScript")
//   5. Hot-reload : NkPythonBridge::ReloadScript("Scripts/Enemy.py")
//
// Format d'un script Python :
//   # Scripts/EnemyScript.py
//   from nkecs import *
//
//   class EnemyScript(NkScript):
//       def on_start(self, ctx):
//           self.speed = 3.0
//       def on_update(self, ctx, dt):
//           t = ctx.get_transform()
//           t.position.x += self.speed * dt
//       def on_collision_enter(self, ctx, other):
//           ctx.emit(OnPlayerDamaged(10.0))
//
// ⚠️  Dépendances : Python 3.10+ (libpython3.x-dev)
//     CMake : find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
//             target_link_libraries(MyGame PRIVATE Python3::Python)
// =============================================================================

#include "../../NkECSDefines.h"
#include "../NkScriptComponent.h"
#include "../NkScriptBridge.h"
#include <functional>
#include <memory>
#include <cstring>

// Détection de Python
#ifdef NKECS_PYTHON_AVAILABLE
// Python.h doit être inclus AVANT tout autre header sur Windows
#  define PY_SSIZE_T_CLEAN
#  include <Python.h>
#endif

namespace nkentseu { namespace ecs {

// =============================================================================
// NkPyContext — objet Python exposant l'API NkECS à un script
// Wrappé comme objet Python (capsule ou classe PyObject)
// =============================================================================

struct NkPyContext {
    NkWorld*   world  = nullptr;
    NkEntityId entity = NkEntityId::Invalid();
    float32    dt     = 0.f;

    // ── API exposée aux scripts Python ────────────────────────────────────────
    // Ces méthodes correspondent aux fonctions Python du module nkecs

    // ctx.get_transform() → dict avec position, rotation, scale
    // ctx.set_position(x, y, z)
    // ctx.translate(x, y, z)
    // ctx.get_component("ComponentName") → dict des champs
    // ctx.set_component("ComponentName", dict)
    // ctx.has_component("ComponentName") → bool
    // ctx.add_component("ComponentName") → None
    // ctx.remove_component("ComponentName") → None
    // ctx.get_parent() → entityId
    // ctx.get_children() → list[entityId]
    // ctx.get_entity_id() → int (entityId packed)
    // ctx.emit(event_name, **kwargs)
    // ctx.destroy() → schedules destroy
    // ctx.is_alive(entityId) → bool
    // ctx.find_entity("name") → entityId | None
    // ctx.query(component_list) → list[dict]
};

// =============================================================================
// NkPythonBridge — gestion de l'interpréteur Python et du hot-reload
// =============================================================================

class NkPythonBridge {
public:
    static NkPythonBridge& Global() noexcept {
        static NkPythonBridge inst;
        return inst;
    }

    // ── Initialisation ────────────────────────────────────────────────────────

    bool Init(const char* pythonHome = nullptr) noexcept {
#ifdef NKECS_PYTHON_AVAILABLE
        if (mInitialized) return true;

        // Configure le chemin Python si spécifié
        if (pythonHome) {
            // Py_SetPythonHome(Py_DecodeLocale(pythonHome, nullptr));
        }

        Py_Initialize();
        if (!Py_IsInitialized()) return false;

        // Enregistre le module nkecs
        if (!RegisterModule()) return false;

        // Ajoute le dossier Scripts au PYTHONPATH
        PyRun_SimpleString(
            "import sys\n"
            "sys.path.insert(0, 'Scripts')\n"
            "sys.path.insert(0, '.')\n"
        );

        mInitialized = true;
        return true;
#else
        return false; // Python non disponible
#endif
    }

    void Shutdown() noexcept {
#ifdef NKECS_PYTHON_AVAILABLE
        if (!mInitialized) return;
        Py_Finalize();
        mInitialized = false;
#endif
    }

    [[nodiscard]] bool IsInitialized() const noexcept { return mInitialized; }

    // ── Chargement de scripts ─────────────────────────────────────────────────

    // Charge un fichier .py et enregistre toutes les classes NkScript dedans
    bool LoadScript(const char* path) noexcept {
#ifdef NKECS_PYTHON_AVAILABLE
        if (!mInitialized) return false;

        FILE* f = std::fopen(path, "r");
        if (!f) return false;

        // Exécute le fichier dans le module __main__
        PyObject* mainModule = PyImport_AddModule("__main__");
        PyObject* globalDict = PyModule_GetDict(mainModule);
        PyRun_File(f, path, Py_file_input, globalDict, globalDict);
        std::fclose(f);

        if (PyErr_Occurred()) {
            PyErr_Print();
            return false;
        }

        // Enregistre le chemin pour le hot-reload
        if (mScriptCount < kMaxScripts) {
            std::strncpy(mScriptPaths[mScriptCount++], path, 511);
        }

        return true;
#else
        (void)path; return false;
#endif
    }

    // Hot-reload d'un script (préserve l'état des instances)
    bool ReloadScript(const char* path) noexcept {
#ifdef NKECS_PYTHON_AVAILABLE
        // TODO: implémenter importlib.reload() + restauration d'état
        return LoadScript(path);
#else
        (void)path; return false;
#endif
    }

    // Hot-reload automatique (vérifie les timestamps)
    void HotReload(NkWorld& world) noexcept {
        (void)world;
        // Vérifier les timestamps de chaque .py et recharger si modifié
        for (uint32 i = 0; i < mScriptCount; ++i) {
            // uint64 mtime = GetFileModTime(mScriptPaths[i]);
            // if (mtime > mScriptTimestamps[i]) ReloadScript(mScriptPaths[i]);
        }
    }

    // ── Création d'instances ──────────────────────────────────────────────────

    // Crée une instance Python d'une classe de script
    // Retourne un adapter NkScriptComponent
    std::shared_ptr<NkScriptComponent> CreatePythonScript(
        const char* className) noexcept
    {
#ifdef NKECS_PYTHON_AVAILABLE
        if (!mInitialized) return nullptr;

        PyObject* mainModule = PyImport_AddModule("__main__");
        PyObject* cls = PyObject_GetAttrString(mainModule, className);
        if (!cls || !PyCallable_Check(cls)) {
            PyErr_Print();
            return nullptr;
        }

        PyObject* instance = PyObject_CallNoArgs(cls);
        Py_DECREF(cls);
        if (!instance) { PyErr_Print(); return nullptr; }

        return std::make_shared<NkPythonScriptAdapter>(instance, className);
#else
        (void)className; return nullptr;
#endif
    }

    // ── Module Python nkecs ───────────────────────────────────────────────────
    //
    // Ce module est exposé aux scripts Python comme :
    //   from nkecs import NkScript, NkVec3, NkColor4, ...
    //
    // Fonctions globales disponibles dans les scripts :
    //   nkecs.find_entity(name) → entityId
    //   nkecs.create_entity(name) → entityId
    //   nkecs.destroy_entity(entityId) → None
    //   nkecs.get_component(entityId, name) → dict
    //   nkecs.query([names...]) → list[entityId]

private:
    bool RegisterModule() noexcept {
#ifdef NKECS_PYTHON_AVAILABLE
        // Injection du module nkecs via PyRun_SimpleString (approche simple)
        // En production, utiliser PyModule_Create avec une vraie table de méthodes
        const char* moduleCode = R"PY(
import sys

class NkVec2:
    def __init__(self, x=0.0, y=0.0):
        self.x = float(x)
        self.y = float(y)
    def __repr__(self):
        return f"NkVec2({self.x:.3f}, {self.y:.3f})"
    def __add__(self, o): return NkVec2(self.x+o.x, self.y+o.y)
    def __mul__(self, s): return NkVec2(self.x*s, self.y*s)
    def length(self): return (self.x**2 + self.y**2)**0.5

class NkVec3:
    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)
    def __repr__(self):
        return f"NkVec3({self.x:.3f}, {self.y:.3f}, {self.z:.3f})"
    def __add__(self, o): return NkVec3(self.x+o.x, self.y+o.y, self.z+o.z)
    def __sub__(self, o): return NkVec3(self.x-o.x, self.y-o.y, self.z-o.z)
    def __mul__(self, s): return NkVec3(self.x*s, self.y*s, self.z*s)
    def length(self): return (self.x**2 + self.y**2 + self.z**2)**0.5
    def normalized(self):
        l = self.length()
        return NkVec3(self.x/l, self.y/l, self.z/l) if l > 1e-6 else NkVec3()

class NkColor4:
    def __init__(self, r=1.0, g=1.0, b=1.0, a=1.0):
        self.r, self.g, self.b, self.a = float(r),float(g),float(b),float(a)
    @staticmethod
    def white():  return NkColor4(1,1,1,1)
    @staticmethod
    def red():    return NkColor4(1,0,0,1)
    @staticmethod
    def green():  return NkColor4(0,1,0,1)
    @staticmethod
    def blue():   return NkColor4(0,0,1,1)
    @staticmethod
    def yellow(): return NkColor4(1,1,0,1)
    @staticmethod
    def from_hex(hex_val):
        return NkColor4(
            ((hex_val>>16)&0xFF)/255.0,
            ((hex_val>>8)&0xFF)/255.0,
            (hex_val&0xFF)/255.0,
            ((hex_val>>24)&0xFF)/255.0)

class NkScript:
    """Classe de base pour tous les scripts Python NkECS."""
    def on_start(self, ctx):
        """Appelé une fois avant le premier on_update."""
        pass
    def on_update(self, ctx, dt):
        """Appelé chaque frame."""
        pass
    def on_fixed_update(self, ctx, fixed_dt):
        """Appelé à taux fixe (physique)."""
        pass
    def on_late_update(self, ctx, dt):
        """Appelé après tous les on_update."""
        pass
    def on_destroy(self, ctx):
        """Appelé avant la destruction."""
        pass
    def on_enable(self, ctx):
        pass
    def on_disable(self, ctx):
        pass
    def on_collision_enter(self, ctx, other_id):
        pass
    def on_collision_exit(self, ctx, other_id):
        pass
    def on_trigger_enter(self, ctx, other_id):
        pass
    def on_trigger_exit(self, ctx, other_id):
        pass
    def serialize(self):
        """Retourne un dict sérialisable (pour hot-reload / sauvegarde)."""
        return {}
    def deserialize(self, data):
        """Restaure l'état depuis un dict."""
        for k, v in data.items():
            if hasattr(self, k):
                setattr(self, k, v)

# Enregistre le module
import importlib
import importlib.util

class _NkECSModule:
    NkScript  = NkScript
    NkVec2    = NkVec2
    NkVec3    = NkVec3
    NkColor4  = NkColor4

sys.modules['nkecs'] = _NkECSModule()
print("[NkECS] Module Python 'nkecs' initialisé")
)PY";

        return PyRun_SimpleString(moduleCode) == 0;
#else
        return false;
#endif
    }

    // ── Données ───────────────────────────────────────────────────────────────

    static constexpr uint32 kMaxScripts = 128u;
    char   mScriptPaths[kMaxScripts][512] = {};
    uint64 mScriptTimestamps[kMaxScripts] = {};
    uint32 mScriptCount = 0;
    bool   mInitialized = false;
};

// =============================================================================
// NkPythonScriptAdapter — adapte une instance PyObject → NkScriptComponent
// =============================================================================

class NkPythonScriptAdapter final : public NkScriptComponent {
public:
#ifdef NKECS_PYTHON_AVAILABLE
    NkPythonScriptAdapter(PyObject* instance, const char* typeName) noexcept
        : mInstance(instance)
    {
        std::strncpy(mTypeName, typeName, 127);
    }

    ~NkPythonScriptAdapter() noexcept override {
        if (mInstance) { Py_DECREF(mInstance); mInstance = nullptr; }
    }
#else
    NkPythonScriptAdapter(void*, const char* typeName) noexcept {
        std::strncpy(mTypeName, typeName, 127);
    }
#endif

    const char* GetTypeName() const noexcept override { return mTypeName; }

    void OnStart(NkWorld& world, NkEntityId self) noexcept override {
        CallMethod("on_start", world, self, 0.f);
    }

    void OnUpdate(NkWorld& world, NkEntityId self, float32 dt) noexcept override {
        CallMethod("on_update", world, self, dt);
    }

    void OnFixedUpdate(NkWorld& world, NkEntityId self, float32 fdt) noexcept override {
        CallMethod("on_fixed_update", world, self, fdt);
    }

    void OnLateUpdate(NkWorld& world, NkEntityId self, float32 dt) noexcept override {
        CallMethod("on_late_update", world, self, dt);
    }

    void OnDestroy(NkWorld& world, NkEntityId self) noexcept override {
        CallMethod("on_destroy", world, self, 0.f);
    }

    void OnCollisionEnter(NkWorld& world, NkEntityId self,
                          NkEntityId other) noexcept override {
        CallCollisionMethod("on_collision_enter", world, self, other);
    }

    void Serialize(char* buf, uint32 bufSize) const noexcept override {
#ifdef NKECS_PYTHON_AVAILABLE
        if (!mInstance) return;
        PyObject* result = PyObject_CallMethod(mInstance, "serialize", nullptr);
        if (!result) { PyErr_Print(); return; }
        // Convertit le dict Python en JSON simplifié
        PyObject* repr = PyObject_Repr(result);
        if (repr) {
            const char* s = PyUnicode_AsUTF8(repr);
            if (s) std::strncpy(buf, s, bufSize - 1);
            Py_DECREF(repr);
        }
        Py_DECREF(result);
#else
        (void)buf; (void)bufSize;
#endif
    }

    void Deserialize(const char* json) noexcept override {
#ifdef NKECS_PYTHON_AVAILABLE
        if (!mInstance || !json) return;
        // Exécute : self.deserialize(eval(json))
        char code[2048];
        std::snprintf(code, sizeof(code),
            "import ast\n"
            "_nk_tmp_data = ast.literal_eval(%s)\n", json);
        PyRun_SimpleString(code);
#else
        (void)json;
#endif
    }

private:
#ifdef NKECS_PYTHON_AVAILABLE
    PyObject* mInstance = nullptr;
#endif
    char      mTypeName[128] = {};

    void CallMethod(const char* method, NkWorld& world,
                    NkEntityId self, float32 dt) noexcept {
#ifdef NKECS_PYTHON_AVAILABLE
        if (!mInstance) return;
        // Construit le contexte Python (objet capsule)
        // En production : créer un vrai PyObject NkPyContext
        // Ici simplifié avec un dict Python
        PyObject* ctx = PyDict_New();
        if (!ctx) return;
        // Injecte world/entity/dt dans le contexte
        PyDict_SetItemString(ctx, "entity_id", PyLong_FromUnsignedLongLong(self.Pack()));
        PyDict_SetItemString(ctx, "dt", PyFloat_FromDouble(dt));
        // Appel de la méthode
        PyObject* result = PyObject_CallMethod(mInstance, method, "Of", ctx, (double)dt);
        if (!result && PyErr_Occurred()) PyErr_Print();
        else if (result) Py_DECREF(result);
        Py_DECREF(ctx);
        (void)world;
#else
        (void)method; (void)world; (void)self; (void)dt;
#endif
    }

    void CallCollisionMethod(const char* method, NkWorld& world,
                              NkEntityId self, NkEntityId other) noexcept {
#ifdef NKECS_PYTHON_AVAILABLE
        if (!mInstance) return;
        PyObject* ctx = PyDict_New();
        if (!ctx) return;
        PyDict_SetItemString(ctx, "entity_id", PyLong_FromUnsignedLongLong(self.Pack()));
        PyObject* result = PyObject_CallMethod(mInstance, method, "OK",
            ctx, (unsigned long long)other.Pack());
        if (!result && PyErr_Occurred()) PyErr_Print();
        else if (result) Py_DECREF(result);
        Py_DECREF(ctx);
        (void)world;
#else
        (void)method; (void)world; (void)self; (void)other;
#endif
    }
};

// =============================================================================
// Extension de NkScriptHost pour les scripts Python
// =============================================================================

inline bool AttachPythonScript(NkScriptHost& host, const char* className) noexcept {
    auto script = NkPythonBridge::Global().CreatePythonScript(className);
    if (!script) return false;
    if (host.scriptCount >= NkScriptHost::kMaxScripts) return false;
    host.scripts[host.scriptCount++] = script;
    host.pendingStart = true;
    return true;
}

}} // namespace nkentseu::ecs

// =============================================================================
// EXEMPLE DE SCRIPT PYTHON (à placer dans Scripts/EnemyAI.py)
// =============================================================================
/*

# Scripts/EnemyAI.py
from nkecs import NkScript, NkVec3
import math

class EnemyAI(NkScript):
    """IA ennemie simple : détecte le joueur et le charge."""

    def on_start(self, ctx):
        self.speed      = 3.0
        self.detect_range = 15.0
        self.attack_range = 2.0
        self.attack_cd  = 1.5
        self.attack_timer = 0.0
        self.state      = "idle"   # idle / chase / attack
        self.player_id  = None
        print(f"[EnemyAI] Démarré sur entité {ctx['entity_id']}")

    def on_update(self, ctx, dt):
        self.attack_timer = max(0.0, self.attack_timer - dt)

        # Cherche le joueur si pas encore trouvé
        if self.player_id is None:
            players = ctx.get('query_result_player', [])
            if players:
                self.player_id = players[0]

        if self.player_id is None:
            return

        # Calcul distance
        my_pos    = ctx.get('position', NkVec3())
        target_pos = ctx.get('target_position', NkVec3())
        diff = target_pos - my_pos
        dist = diff.length()

        if self.state == "idle":
            if dist < self.detect_range:
                self.state = "chase"

        elif self.state == "chase":
            if dist < self.attack_range:
                self.state = "attack"
            elif dist > self.detect_range * 1.5:
                self.state = "idle"
            else:
                # Déplace vers le joueur
                direction = diff.normalized()
                ctx['move'] = direction * self.speed * dt

        elif self.state == "attack":
            if dist > self.attack_range * 1.5:
                self.state = "chase"
            elif self.attack_timer <= 0.0:
                ctx['emit'] = ('OnPlayerDamaged', {'damage': 10.0})
                self.attack_timer = self.attack_cd

    def serialize(self):
        return {
            'speed': self.speed,
            'state': self.state,
            'attack_timer': self.attack_timer,
        }

    def deserialize(self, data):
        self.speed        = data.get('speed', 3.0)
        self.state        = data.get('state', 'idle')
        self.attack_timer = data.get('attack_timer', 0.0)


class RotatingPlatform(NkScript):
    """Plateforme rotative simple."""

    def on_start(self, ctx):
        self.speed_deg = 45.0   # degrés par seconde
        self.axis = NkVec3(0, 1, 0)

    def on_update(self, ctx, dt):
        ctx['rotate'] = (self.axis, self.speed_deg * dt)

    def serialize(self):
        return {'speed_deg': self.speed_deg}


class CoinCollector(NkScript):
    """Détecte les pièces proches et les collecte."""

    def on_start(self, ctx):
        self.coins = 0
        self.collect_range = 1.5

    def on_trigger_enter(self, ctx, other_id):
        # Vérifie si c'est une pièce
        self.coins += 1
        ctx['emit'] = ('OnCoinCollected', {'total': self.coins})
        ctx['destroy_entity'] = other_id
        print(f"[CoinCollector] {self.coins} pièces collectées !")

*/
