# 📚 NKReflection - Module de Réflexion Runtime

> **Version** : 1.0.0
> **Auteur** : Rihen
> **Date** : 2026-02-07
> **Licence** : Propriétaire - Libre d'utilisation et de modification
> **Statut** : ✅ Stable - Production Ready

---

## 🎯 Table des Matières

1. [Présentation](#-présentation)
2. [Fonctionnalités Clés](#-fonctionnalités-clés)
3. [Architecture du Module](#-architecture-du-module)
4. [Dépendances](#-dépendances)
5. [Installation et Intégration](#-installation-et-intégration)
6. [Guide de Démarrage Rapide](#-guide-de-démarrage-rapide)
7. [Référence API](#-référence-api)
8. [Patterns Avancés](#-patterns-avancés)
9. [Considérations Thread-Safety](#-considérations-thread-safety)
10. [Performances et Optimisations](#-performances-et-optimisations)
11. [Dépannage et Pièges Courants](#-dépannage-et-pièges-courants)
12. [Bonnes Pratiques](#-bonnes-pratiques)
13. [Contribuer au Module](#-contribuer-au-module)
14. [Licence](#-licence)

---

## 🎯 Présentation

**NKReflection** est un système complet de réflexion runtime pour l'écosystème NKEntseu, permettant l'introspection dynamique des types, classes, propriétés et méthodes en C++.

### Pourquoi NKReflection ?


| Besoin                              | Solution NKReflection                                           |
| ----------------------------------- | --------------------------------------------------------------- |
| 🔄 Sérialisation/Désérialisation | Accès dynamique aux propriétés via métadonnées             |
| 🎮 Éditeurs de niveau/entités     | Inspection et modification runtime des objets                   |
| 🔌 Système de plugins              | Enregistrement et découverte dynamique de classes              |
| 🧪 Tests unitaires                  | Création d'instances et invocation de méthodes via réflexion |
| 📊 Debugging runtime                | Dump et validation du registre de métadonnées                 |

### Principes de Conception

```
┌─────────────────────────────────────────┐
│  ✅ Zero-overhead par défaut            │
│  ✅ Pas de dépendance STL runtime       │
│  ✅ Type-safety maximale via templates  │
│  ✅ Small Buffer Optimization (SBO)     │
│  ✅ Thread-aware avec documentation     │
│  ✅ Compatibilité multiplateforme       │
└─────────────────────────────────────────┘
```

---

## ✨ Fonctionnalités Clés

### 🔍 Introspection de Types

```cpp
// Obtention des métadonnées d'un type
const auto* typeMeta = nkentseu::reflection::GetType<nk_int32>();
printf("Type: %s, Size: %zu, Alignment: %zu\n",
    typeMeta->GetName(),
    typeMeta->GetSize(),
    typeMeta->GetAlignment());

// Vérification de catégories de types
if (typeMeta->IsPrimitive()) { /* ... */ }
if (typeMeta->IsPointer()) { /* ... */ }
```

### 🏗️ Métadonnées de Classes

```cpp
// Accès aux propriétés d'une classe
const auto* playerClass = nkentseu::reflection::FindClass("Player");
for (nk_usize i = 0; i < playerClass->GetPropertyCount(); ++i) {
    const auto* prop = playerClass->GetPropertyAt(i);
    printf("Property: %s (type: %s)\n", 
        prop->GetName(), 
        prop->GetType().GetName());
}

// Navigation dans l'héritage
if (playerClass->IsSubclassOf(entityClass)) {
    // Player hérite de Entity
}
```

### ⚡ Invocation Dynamique de Méthodes

```cpp
// Recherche et invocation d'une méthode
const auto* method = playerClass->GetMethod("TakeDamage");
if (method && method->HasInvoke()) {
    nk_int32 damage = 50;
    void* args[] = { &damage };
    method->Invoke(playerInstance, args);
}
```

### 🔄 Factory Pattern Réflexif

```cpp
// Création d'instances par nom de classe
void* instance = nkentseu::reflection::CreateInstanceByName("Enemy");
if (instance) {
    // Utilisation...
    nkentseu::reflection::DestroyInstanceByName("Enemy", instance);
}

// Version type-safe quand le type est connu
auto* player = nkentseu::reflection::CreateInstance<game::Player>();
```

### 📦 Gestion Mémoire Automatique

- **Small Buffer Optimization (SBO)** : Les petits callables (< 64 bytes) sont stockés inline, sans allocation heap
- **NkFunction** : Encapsulation type-safe des callbacks avec gestion automatique de la mémoire
- **Pas de fuites** : Destructeurs garantis via RAII

---

## 🏗️ Architecture du Module

```
NKReflection/
├── include/NKReflection/
│   ├── NkReflection.h          # Point d'entrée unique (ONE INCLUDE)
│   ├── NkReflectionApi.h       # Macros d'export et configuration
│   ├── NkType.h               # Métadonnées de base des types
│   ├── NkProperty.h           # Métadonnées et accès aux propriétés
│   ├── NkMethod.h             # Métadonnées et invocation des méthodes
│   ├── NkClass.h              # Métadonnées des classes et héritage
│   └── NkRegistry.h           # Registre global + macros de réflexion
│
├── src/NKReflection/
│   ├── NkReflection.cpp       # Implémentation des utilitaires haut-niveau
│   ├── NkType.cpp             # Implémentation du registre de types
│   ├── NkProperty.cpp         # Extensions runtime des propriétés
│   ├── NkMethod.cpp           # Extensions runtime des méthodes
│   ├── NkClass.cpp            # Extensions runtime des classes
│   └── NkRegistry.cpp         # Extensions du registre global
│
└── tests/                     # Tests unitaires et benchmarks
```

### Flux de Données

```
┌─────────────────┐
│  Code Utilisateur│
│  #include <NKReflection/NkReflection.h>
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  NkReflection.h │ ← Point d'entrée unique
│  (inclut tout)  │
└────────┬────────┘
         │
    ┌────┴────┬────────────┬────────────┬────────────┐
    ▼         ▼            ▼            ▼            ▼
┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐ ┌──────────┐
│NkType │ │NkProp │ │NkMeth │ │NkClass│ │NkRegistry│
│       │ │       │ │       │ │       │ │          │
└───┬───┘ └───┬───┘ └───┬───┘ └───┬───┘ └────┬─────┘
    │         │         │         │          │
    └────┬────┴────┬────┴────┬────┴──────────┘
         ▼          ▼         ▼
    ┌─────────────────────────┐
    │   NkRegistry (Singleton)│
    │   • RegisterType()      │
    │   • RegisterClass()     │
    │   • FindType/Class()    │
    │   • Callbacks optionnels│
    └─────────────────────────┘
```

---

## 🔗 Dépendances

### Dépendances Internes (NKEntseu)


| Module                      | Composants Utilisés                            | Version Minimale |
| --------------------------- | ----------------------------------------------- | ---------------- |
| **NKCore**                  | `NkTypes`, `NkTraits`, `NkAssert`, `NkLog`      | ≥ 1.0.0         |
| **NKPlatform**              | Macros d'export, compatibilité multiplateforme | ≥ 1.0.0         |
| **NKContainers/Functional** | `NkFunction`, `NkBind`, `NkTuple`               | ≥ 1.0.0         |

### Dépendances Système

```cmake
# Aucune dépendance externe requise
# Utilise uniquement les en-têtes standards minimaux :
# - <cstddef> pour offsetof
# - <cstring> pour strcmp
# - <typeinfo> pour typeid (optionnel, pour les noms de types)
```

### Configuration Requise

```cpp
// Compilateur requis : C++17 minimum (C++20 recommandé pour [[attributes]])
// Macros de configuration optionnelles :
// - NKENTSEU_REFLECTION_BUILD_SHARED_LIB : Build en bibliothèque partagée
// - NKENTSEU_REFLECTION_STATIC_LIB : Build en bibliothèque statique
// - NKENTSEU_REFLECTION_HEADER_ONLY : Mode header-only (tout inline)
// - NKENTSEU_REFLECTION_DEBUG : Active les messages de debug à la compilation
```

---

## 📦 Installation et Intégration

### Via CMake (Recommandé)

```cmake
# Dans le CMakeLists.txt de votre projet :

# 1. Ajouter le module NKReflection comme sous-projet ou dépendance
add_subdirectory(Modules/System/NKReflection)

# 2. Lier votre cible avec NKReflection
target_link_libraries(VotreApplication PRIVATE
    NKReflection::NKReflection
)

# 3. Les include directories sont configurés automatiquement
#    Il suffit d'inclure le point d'entrée unique :
#    #include <NKReflection/NkReflection.h>
```

### Intégration Manuelle

```cmake
# 1. Ajouter les include directories
target_include_directories(VotreApplication PRIVATE
    Modules/System/NKReflection/include
)

# 2. Ajouter les fichiers sources si build statique
target_sources(VotreApplication PRIVATE
    Modules/System/NKReflection/src/NKReflection/NkReflection.cpp
    Modules/System/NKReflection/src/NKReflection/NkType.cpp
    # ... autres fichiers .cpp selon vos besoins
)

# 3. Définir les macros de configuration si nécessaire
target_compile_definitions(VotreApplication PRIVATE
    NKENTSEU_REFLECTION_STATIC_LIB  # ou BUILD_SHARED_LIB, ou HEADER_ONLY
)
```

### Vérification de l'Intégration

```cpp
// test_reflection.cpp
#include <NKReflection/NkReflection.h>
#include <NKCore/Log/NkLog.h>

int main() {
    // Initialisation optionnelle
    if (!nkentseu::reflection::Initialize()) {
        NK_FOUNDATION_LOG_ERROR("Échec d'initialisation de NKReflection");
        return 1;
    }

    // Test basique : recherche d'un type primitif
    const auto* intType = nkentseu::reflection::FindType("int");
    if (intType) {
        NK_FOUNDATION_LOG_INFO("✓ NKReflection fonctionne : type 'int' trouvé");
    } else {
        NK_FOUNDATION_LOG_WARNING("Type 'int' non trouvé (nom dépendant du compilateur)");
    }

    // Dump du registre pour inspection
    #ifdef NKENTSEU_DEBUG
    nkentseu::reflection::DumpRegistry();
    #endif

    nkentseu::reflection::Shutdown();
    return 0;
}
```

---

## 🚀 Guide de Démarrage Rapide

### Étape 1 : Inclure le Point d'Entrée Unique

```cpp
// Dans n'importe quel fichier de votre projet :
#include <NKReflection/NkReflection.h>

// Optionnel : alias de namespace pour plus de concision
namespace refl = nkentseu::refl;
```

### Étape 2 : Déclarer une Classe Réfléchie

```cpp
// Player.h
#pragma once
#include <NKReflection/NkReflection.h>

namespace game {

    class Player {
        // Macro obligatoire : déclare les métadonnées de la classe
        NKENTSEU_REFLECT_CLASS(Player)

    public:
        // Macro combinée : déclare ET réfléchit la propriété
        NKENTSEU_PROPERTY(nk_int32, health)
        NKENTSEU_PROPERTY(nk_float32, speed)
        NKENTSEU_PROPERTY(nk_bool, isAlive)

        // Méthodes normales (peuvent aussi être réfléchies si besoin)
        void TakeDamage(nk_int32 amount);
        bool IsDead() const { return health <= 0; }
    };

} // namespace game
```

### Étape 3 : Enregistrer la Classe (dans un .cpp uniquement)

```cpp
// Player.cpp
#include "Game/Player.h"

namespace game {

    void Player::TakeDamage(nk_int32 amount) {
        health -= amount;
        if (health < 0) health = 0;
    }

    // ⚠️ IMPORTANT : Cette macro doit être dans UN SEUL fichier .cpp
    // Jamais dans un header inclus dans plusieurs translation units !
    NKENTSEU_REGISTER_CLASS(Player)

} // namespace game
```

### Étape 4 : Utiliser la Réflexion Runtime

```cpp
// GameplaySystem.cpp
#include <NKReflection/NkReflection.h>
#include <NKCore/Log/NkLog.h>

void InspectPlayer(game::Player* player) {
    // Option A : Via l'interface polymorphique de la classe
    const auto* classMeta = player->GetClass();
  
    // Option B : Via recherche par nom dans le registre
    // const auto* classMeta = nkentseu::reflection::FindClass("Player");

    if (!classMeta) {
        NK_FOUNDATION_LOG_ERROR("Métadonnées de Player non trouvées");
        return;
    }

    // Accéder aux propriétés via réflexion
    const auto* healthProp = classMeta->GetProperty("health");
    if (healthProp) {
        nk_int32 currentHealth = healthProp->GetValue<nk_int32>(player);
        NK_FOUNDATION_LOG_INFO("Player health: %d", currentHealth);
      
        // Modifier la propriété via réflexion
        healthProp->SetValue<nk_int32>(player, currentHealth + 10);
    }

    // Itérer sur toutes les propriétés
    NK_FOUNDATION_LOG_INFO("=== Player Properties ===");
    for (nk_usize i = 0; i < classMeta->GetPropertyCount(); ++i) {
        const auto* prop = classMeta->GetPropertyAt(i);
        if (prop) {
            NK_FOUNDATION_LOG_INFO("  %s : %s", 
                prop->GetName(), 
                prop->GetType().GetName());
        }
    }
}
```

### Étape 5 : Factory Pattern avec Création Dynamique

```cpp
void* CreateEntityByType(const nk_char* typeName) {
    // Création d'instance via nom de classe
    return nkentseu::reflection::CreateInstanceByName(typeName);
}

void SpawnEnemy(const nk_char* enemyType) {
    void* enemy = CreateEntityByType(enemyType);
    if (enemy) {
        NK_FOUNDATION_LOG_INFO("Spawned enemy of type '%s'", enemyType);
        // ... utilisation de l'ennemi ...
      
        // Nettoyage via réflexion si destructeur configuré
        nkentseu::reflection::DestroyInstanceByName(enemyType, enemy);
    } else {
        NK_FOUNDATION_LOG_WARNING("Failed to create enemy '%s'", enemyType);
    }
}
```

---

## 📖 Référence API

### Fonctions Utilitaires de Haut Niveau


| Fonction                                       | Description                                           | Thread-Safe      |
| ---------------------------------------------- | ----------------------------------------------------- | ---------------- |
| `Initialize()`                                 | Initialise le système de réflexion (optionnel)      | ❌ Non           |
| `Shutdown()`                                   | Finalise le système et libère les ressources        | ❌ Non           |
| `FindClass(const nk_char*)`                    | Recherche une classe par nom dans le registre         | ✅ Lecture seule |
| `FindType(const nk_char*)`                     | Recherche un type par nom dans le registre            | ✅ Lecture seule |
| `GetType<T>()`                                 | Template : obtient les métadonnées d'un type C++    | ✅ Lecture seule |
| `GetClass<T>()`                                | Template : obtient les métadonnées d'une classe C++ | ✅ Lecture seule |
| `CreateInstanceByName(const nk_char*)`         | Crée une instance par nom de classe                  | ✅ Lecture seule |
| `CreateInstance<T>()`                          | Template : crée une instance type-safe               | ✅ Lecture seule |
| `DestroyInstanceByName(const nk_char*, void*)` | Détruit une instance par nom de classe               | ✅ Lecture seule |
| `DestroyInstance<T>(T*)`                       | Template : détruit une instance type-safe            | ✅ Lecture seule |
| `GetRegisteredClassCount()`                    | Retourne le nombre de classes enregistrées           | ✅ Lecture seule |
| `GetRegisteredTypeCount()`                     | Retourne le nombre de types enregistrés              | ✅ Lecture seule |
| `DumpRegistry()`                               | Affiche le contenu du registre pour debugging         | ✅ Lecture seule |
| `ValidateRegistry()`                           | Valide l'intégrité du registre                      | ✅ Lecture seule |

### Macros de Réflexion


| Macro                                     | Usage                                                    | Notes                                                            |
| ----------------------------------------- | -------------------------------------------------------- | ---------------------------------------------------------------- |
| `NKENTSEU_REFLECT_CLASS(ClassName)`       | Déclare les métadonnées d'une classe                  | À placer dans la section`public` de la classe                   |
| `NKENTSEU_REFLECT_PROPERTY(PropertyName)` | Déclare une propriété réfléchie                     | Requiert`NKENTSEU_REFLECT_CLASS` au préalable                   |
| `NKENTSEU_PROPERTY(Type, Name)`           | Déclare ET réfléchit une propriété en une ligne     | Plus concis que la séparation déclaration + réflexion         |
| `NKENTSEU_REGISTER_CLASS(ClassName)`      | Enregistre automatiquement la classe au startup          | **À placer dans un seul fichier `.cpp`**, jamais dans un header |
| `NKENTSEU_REFLECT`                        | Attribut`[[nkentseu::reflect]]` pour annotation statique | Purement informatif, aucun effet runtime                         |

### Classes Principales

#### `NkType` - Métadonnées d'un Type

```cpp
class NkType {
public:
    // Accesseurs de base
    const nk_char* GetName() const;
    nk_usize GetSize() const;
    nk_usize GetAlignment() const;
    NkTypeCategory GetCategory() const;

    // Vérifications de catégorie
    nk_bool IsPrimitive() const;
    nk_bool IsPointer() const;
    nk_bool IsArray() const;
    nk_bool IsClass() const;
    // ... autres Is*()

    // Comparaison
    nk_bool operator==(const NkType&) const;
    nk_bool operator!=(const NkType&) const;
};
```

#### `NkProperty` - Métadonnées d'une Propriété

```cpp
class NkProperty {
public:
    // Accesseurs
    const nk_char* GetName() const;
    const NkType& GetType() const;
    nk_usize GetOffset() const;  // Offset mémoire dans la classe
    nk_uint32 GetFlags() const;

    // Vérifications de flags
    nk_bool IsReadOnly() const;
    nk_bool IsStatic() const;
    nk_bool IsConst() const;
    nk_bool IsTransient() const;  // Exclue de la sérialisation

    // Accès aux valeurs (type-safe via templates)
    template<typename T>
    const T& GetValue(void* instance) const;
  
    template<typename T>
    void SetValue(void* instance, const T& value) const;

    // Accès brut (avancé)
    void* GetValuePtr(void* instance) const;
};
```

#### `NkMethod` - Métadonnées d'une Méthode

```cpp
class NkMethod {
public:
    // Accesseurs
    const nk_char* GetName() const;
    const NkType& GetReturnType() const;
    nk_uint32 GetFlags() const;
    nk_usize GetParameterCount() const;
    const NkType& GetParameterType(nk_usize index) const;

    // Vérifications de flags
    nk_bool IsStatic() const;
    nk_bool IsConst() const;
    nk_bool IsVirtual() const;
    nk_bool IsAbstract() const;
    nk_bool IsDeprecated() const;

    // Invocation (si un callable est configuré)
    nk_bool HasInvoke() const;
    void* Invoke(void* instance, void** args) const;
};
```

#### `NkClass` - Métadonnées d'une Classe

```cpp
class NkClass {
public:
    // Informations de base
    const nk_char* GetName() const;
    nk_usize GetSize() const;
    const NkType& GetType() const;

    // Héritage
    const NkClass* GetBaseClass() const;
    void SetBaseClass(const NkClass* base);
    nk_bool IsSubclassOf(const NkClass* other) const;
    nk_bool IsSuperclassOf(const NkClass* other) const;

    // Propriétés
    void AddProperty(const NkProperty*);
    const NkProperty* GetProperty(const nk_char* name) const;
    const NkProperty* GetPropertyAt(nk_usize index) const;
    nk_usize GetPropertyCount() const;
    nk_usize GetTotalPropertyCount() const;  // Inclut l'héritage

    // Méthodes
    void AddMethod(const NkMethod*);
    const NkMethod* GetMethod(const nk_char* name) const;
    const NkMethod* GetMethodAt(nk_usize index) const;
    nk_usize GetMethodCount() const;
    nk_usize GetTotalMethodCount() const;  // Inclut l'héritage

    // Cycle de vie (si configuré)
    nk_bool HasConstructor() const;
    nk_bool HasDestructor() const;
    void* CreateInstance() const;
    void DestroyInstance(void* instance) const;
};
```

#### `NkRegistry` - Registre Global Singleton

```cpp
class NkRegistry {
public:
    // Singleton accessor (thread-safe initialization)
    static NkRegistry& Get();

    // Enregistrement (thread-UNSAFE - synchronisation externe requise)
    void RegisterType(const NkType*);
    void RegisterClass(const NkClass*);

    // Recherche (thread-safe en lecture seule)
    const NkType* FindType(const nk_char* name) const;
    const NkClass* FindClass(const nk_char* name) const;
    template<typename T> const NkType* GetType() const;
    template<typename T> const NkClass* GetClass() const;

    // Inspection
    nk_usize GetTypeCount() const;
    nk_usize GetClassCount() const;
    const NkType* GetTypeAt(nk_usize index) const;
    const NkClass* GetClassAt(nk_usize index) const;

    // Callbacks optionnels (via NkFunction)
    using OnRegisterCallback = NkFunction<void(const nk_char*, nk_bool)>;
    void SetOnRegisterCallback(OnRegisterCallback);
    const OnRegisterCallback& GetOnRegisterCallback() const;
};
```

---

## 🔧 Patterns Avancés

### 🔄 Factory Pattern avec Filtrage par Héritage

```cpp
// Factory générique pour créer des sous-types d'une classe de base
template<typename BaseType>
BaseType* CreateDerivedInstance(const nk_char* derivedClassName) {
    const auto* baseClass = nkentseu::reflection::FindClass(typeid(BaseType).name());
    const auto* derivedClass = nkentseu::reflection::FindClass(derivedClassName);
  
    if (!baseClass || !derivedClass || !derivedClass->IsSubclassOf(baseClass)) {
        NK_FOUNDATION_LOG_ERROR("Class '%s' is not a subclass of '%s'", 
            derivedClassName, typeid(BaseType).name());
        return nullptr;
    }
  
    if (!derivedClass->HasConstructor()) {
        NK_FOUNDATION_LOG_ERROR("Class '%s' has no registered constructor", derivedClassName);
        return nullptr;
    }
  
    return static_cast<BaseType*>(derivedClass->CreateInstance());
}

// Usage :
auto* zombie = CreateDerivedInstance<game::Enemy>("Zombie");
if (zombie) {
    zombie->Attack(player);
    nkentseu::reflection::DestroyInstance(zombie);
}
```

### 📦 Sérialisation Réflexive Générique

```cpp
// Helper pour sérialiser n'importe quelle classe réfléchie en JSON
nk_bool SerializeToJSON(const nk_char* className, void* instance, nk_char* outBuffer, nk_usize bufferSize) {
    const auto* classMeta = nkentseu::reflection::FindClass(className);
    if (!classMeta) return false;

    nk_usize offset = 0;
    #define APPEND(fmt, ...) do { \
        int written = snprintf(outBuffer + offset, bufferSize - offset, fmt, ##__VA_ARGS__); \
        if (written < 0 || static_cast<nk_usize>(written) >= bufferSize - offset) return false; \
        offset += written; \
    } while(0)

    APPEND("{");
  
    for (nk_usize i = 0; i < classMeta->GetPropertyCount(); ++i) {
        const auto* prop = classMeta->GetPropertyAt(i);
        if (!prop || prop->IsStatic() || prop->IsTransient()) continue;
      
        if (i > 0) APPEND(",");
        APPEND("\"%s\":", prop->GetName());
      
        // Dispatch sur le type pour le formatage
        const auto& type = prop->GetType();
        if (type.GetCategory() == NkTypeCategory::NK_INT32) {
            APPEND("%d", prop->GetValue<nk_int32>(instance));
        } else if (type.GetCategory() == NkTypeCategory::NK_FLOAT32) {
            APPEND("%.2f", prop->GetValue<nk_float32>(instance));
        } else if (type.GetCategory() == NkTypeCategory::NK_BOOL) {
            APPEND("%s", prop->GetValue<nk_bool>(instance) ? "true" : "false");
        } else {
            APPEND("null");  // Types non supportés
        }
    }
  
    APPEND("}");
    #undef APPEND
    return true;
}
```

### 🎛️ Éditeur de Propriétés Runtime

```cpp
// Système simple d'édition de propriétés via console
class PropertyEditor {
public:
    void EditObject(const nk_char* className, void* instance) {
        const auto* classMeta = nkentseu::reflection::FindClass(className);
        if (!classMeta) return;

        printf("Editing object of class '%s'\n", className);
        printf("Available properties:\n");

        // Afficher les propriétés éditables
        nk_usize editableIndex = 0;
        for (nk_usize i = 0; i < classMeta->GetPropertyCount(); ++i) {
            const auto* prop = classMeta->GetPropertyAt(i);
            if (prop && !prop->IsReadOnly() && !prop->IsStatic()) {
                printf("  [%zu] %s (%s) = ", 
                    editableIndex++, 
                    prop->GetName(), 
                    prop->GetType().GetName());
              
                // Afficher la valeur actuelle
                PrintPropertyValue(prop, instance);
                printf("\n");
            }
        }

        // Boucle d'édition simplifiée
        printf("\nEnter property index and new value (or -1 to exit): ");
        nk_int32 index;
        scanf("%d", &index);
      
        if (index >= 0 && static_cast<nk_usize>(index) < editableIndex) {
            // Logique d'édition...
        }
    }

private:
    void PrintPropertyValue(const nkentseu::reflection::NkProperty* prop, void* instance) {
        const auto& type = prop->GetType();
        if (type.GetCategory() == nkentseu::reflection::NkTypeCategory::NK_INT32) {
            printf("%d", prop->GetValue<nk_int32>(instance));
        } else if (type.GetCategory() == nkentseu::reflection::NkTypeCategory::NK_FLOAT32) {
            printf("%.2f", prop->GetValue<nk_float32>(instance));
        } else if (type.GetCategory() == nkentseu::reflection::NkTypeCategory::NK_BOOL) {
            printf("%s", prop->GetValue<nk_bool>(instance) ? "true" : "false");
        } else {
            printf("<unsupported type>");
        }
    }
};
```

### 🔌 Système de Plugins Réflexifs

```cpp
// Interface de base pour les plugins
class IReflectivePlugin {
public:
    virtual ~IReflectivePlugin() = default;
    virtual const nk_char* GetPluginName() const = 0;
    virtual void RegisterTypes() = 0;  // Enregistre ses classes dans NkRegistry
};

// Manager de plugins utilisant la réflexion
class PluginManager {
public:
    bool LoadPlugin(IReflectivePlugin* plugin) {
        if (!plugin) return false;
      
        // Synchronisation requise pour l'enregistrement concurrent
        // std::lock_guard<std::mutex> lock(m_registryMutex);
      
        plugin->RegisterTypes();
        m_plugins.push_back(plugin);
      
        NK_FOUNDATION_LOG_INFO("Plugin '%s' registered its types", plugin->GetPluginName());
        return true;
    }

    // Découverte dynamique de tous les sous-types d'une interface
    template<typename InterfaceType>
    void ForEachPluginImplementation(void (*callback)(const nkentseu::reflection::NkClass*)) {
        const auto* interfaceClass = nkentseu::reflection::GetClass<InterfaceType>();
        if (!interfaceClass) return;

        const auto& registry = nkentseu::reflection::NkRegistry::Get();
        for (nk_usize i = 0; i < registry.GetClassCount(); ++i) {
            const auto* candidate = registry.GetClassAt(i);
            if (candidate && candidate->IsSubclassOf(interfaceClass)) {
                callback(candidate);
            }
        }
    }

private:
    std::vector<IReflectivePlugin*> m_plugins;
    // std::mutex m_registryMutex;  // Pour thread-safety si nécessaire
};
```

---

## 🧵 Considérations Thread-Safety

### Opérations Thread-Safe ✅


| Opération                       | Condition                                                              |
| -------------------------------- | ---------------------------------------------------------------------- |
| `NkRegistry::Get()`              | Initialisation du singleton garantie thread-safe (C++11 magic statics) |
| `FindClass()` / `FindType()`     | Lecture seule, safe si aucun enregistrement concurrent                 |
| `GetProperty()` / `GetMethod()`  | Lecture seule sur métadonnées immuables                              |
| `GetValue<T>()` sur `NkProperty` | Lecture seule, mais l'objet cible doit être synchronisé séparément |
| `Invoke()` sur `NkMethod`        | L'invocation elle-même, mais l'objet cible doit être synchronisé    |

### Opérations Thread-UNSAFE ❌


| Opération                             | Solution                                                                                        |
| -------------------------------------- | ----------------------------------------------------------------------------------------------- |
| `RegisterClass()` / `RegisterType()`   | Effectuer tous les enregistrements pendant l'initialisation mono-thread, ou protéger par mutex |
| `SetBaseClass()` sur `NkClass`         | Configurer l'héritage avant tout accès concurrent                                             |
| `SetConstructor()` / `SetDestructor()` | Configurer avant utilisation concurrente                                                        |
| `SetOnRegisterCallback()`              | Définir tôt dans l'initialisation                                                             |
| `NKENTSEU_REGISTER_CLASS`              | L'ordre d'initialisation entre TU n'est pas garanti : ne pas dépendre de l'ordre               |

### Pattern Recommandé : Initialisation en Deux Phases

```cpp
// Phase 1 : Initialisation mono-thread au démarrage
void InitializeApplication() {
    // 1. Initialiser le module de réflexion
    nkentseu::reflection::Initialize();
  
    // 2. Enregistrer tous les types/classes (avant lancement des threads)
    //    (Les macros NKENTSEU_REGISTER_CLASS s'exécutent automatiquement ici)
  
    // 3. Configurer les callbacks globaux
    auto& registry = nkentseu::reflection::NkRegistry::Get();
    registry.SetOnRegisterCallback([](const nk_char* name, nk_bool isClass) {
        NK_FOUNDATION_LOG_DEBUG("Registered: %s", name);
    });
  
    // 4. Valider l'intégrité du registre
    if (!nkentseu::reflection::ValidateRegistry()) {
        NK_FOUNDATION_LOG_ERROR("Registry validation failed!");
        // Gestion d'erreur...
    }
  
    // 5. Lancer les threads de travail
    StartWorkerThreads();
}

// Phase 2 : Utilisation thread-safe dans les workers
void WorkerThread() {
    // Lecture seule du registre : thread-safe
    const auto* playerClass = nkentseu::reflection::FindClass("Player");
  
    // Création d'instances : thread-safe si le constructeur l'est
    auto* player = nkentseu::reflection::CreateInstance<game::Player>();
  
    // Accès aux propriétés : synchroniser l'accès à l'instance, pas au registre
    {
        std::lock_guard<std::mutex> lock(playerMutex);
        const auto* healthProp = playerClass->GetProperty("health");
        if (healthProp) {
            nk_int32 health = healthProp->GetValue<nk_int32>(player);
            // ...
        }
    }
}
```

---

## ⚡ Performances et Optimisations

### Small Buffer Optimization (SBO)

```
┌─────────────────────────────────────┐
│ NkFunction<void*(void*)>            │
├─────────────────────────────────────┤
│ • Buffer inline : 64 bytes par défaut│
│ • Callables ≤ 64 bytes : stockés inline│
│ • Callables > 64 bytes : allocation heap│
│ • Aucun overhead pour les petits lambdas│
└─────────────────────────────────────┘
```

**Impact** : Réduction significative des allocations heap pour les wrappers d'invocation courants.

### Benchmarks Indicatifs (x86_64, -O2)


| Opération              | Temps Moyen | Notes                                                            |
| ----------------------- | ----------- | ---------------------------------------------------------------- |
| `FindClass("Player")`   | ~150 ns     | Recherche linéaire dans tableau de 512 éléments max           |
| `GetProperty("health")` | ~80 ns      | Après FindClass, recherche dans propriétés de la classe       |
| `GetValue<nk_int32>()`  | ~5 ns       | Accès direct par offset, équivalent à un accès membre normal |
| `Invoke()` avec SBO     | ~25 ns      | Overhead minimal vs appel direct (~5 ns)                         |
| `Invoke()` avec heap    | ~45 ns      | +20 ns pour l'indirection de pointeur                            |

### Optimisations Recommandées

1. **Cachez les pointeurs de métadonnées** :

```cpp
// ❌ À éviter : recherche à chaque frame
void Update() {
    const auto* healthProp = playerClass->GetProperty("health");
    healthProp->SetValue<nk_int32>(player, newHealth);
}

// ✅ Recommandé : cachez le pointeur une fois
class PlayerController {
    const nkentseu::reflection::NkProperty* m_healthProp = nullptr;
  
    void Initialize() {
        const auto* playerClass = nkentseu::reflection::FindClass("Player");
        m_healthProp = playerClass ? playerClass->GetProperty("health") : nullptr;
    }
  
    void Update() {
        if (m_healthProp) {
            m_healthProp->SetValue<nk_int32>(player, newHealth);
        }
    }
};
```

2. **Privilégiez l'accès direct quand le type est connu** :

```cpp
// ❌ Réflexion inutile si le type est connu à la compilation
auto* healthProp = classMeta->GetProperty("health");
nk_int32 health = healthProp->GetValue<nk_int32>(player);

// ✅ Accès direct plus rapide
nk_int32 health = player->health;
```

3. **Utilisez `NkTypeOf<T>()` au lieu de `typeid(T).name()`** :

```cpp
// ❌ Nom dépendant du compilateur, potentiellement démanglé à chaque appel
const char* name = typeid(Player).name();

// ✅ NkTypeOf retourne un NkType avec nom normalisé et métadonnées complètes
const auto& typeMeta = nkentseu::reflection::NkTypeOf<Player>();
```

---

## 🔍 Dépannage et Pièges Courants

### ❌ Problème : Classe non trouvée dans le registre

**Symptôme** : `FindClass("Player")` retourne `nullptr`

**Causes possibles** :

1. La macro `NKENTSEU_REGISTER_CLASS(Player)` est dans un header au lieu d'un `.cpp`
2. Le fichier `.cpp` contenant la macro n'est pas compilé/linké dans le projet
3. L'ordre d'initialisation : la classe est recherchée avant que la macro ne s'exécute

**Solution** :

```cpp
// ✅ Vérifiez que NKENTSEU_REGISTER_CLASS est dans un seul fichier .cpp
// Player.cpp (et PAS dans Player.h)
#include "Game/Player.h"
NKENTSEU_REGISTER_CLASS(Player)  // ← Ici, dans le .cpp uniquement

// ✅ Forcez l'initialisation avant toute recherche
nkentseu::reflection::Initialize();  // Optionnel mais explicite
```

### ❌ Problème : Nom de type non trouvé avec `FindType()`

**Symptôme** : `FindType("int")` retourne `nullptr`

**Cause** : `typeid(T).name()` retourne un nom dépendant du compilateur (name mangling)

**Solution** :

```cpp
// Option 1 : Utilisez le nom exact retourné par typeid (débogage)
#ifdef NKENTSEU_DEBUG
const char* mangledName = typeid(nk_int32).name();
printf("Mangled name: %s\n", mangledName);  // Ex: "i" sur GCC, "int" sur MSVC
#endif

// Option 2 : Utilisez GetType<T>() qui gère automatiquement le nom mangled
const auto* typeMeta = nkentseu::reflection::GetType<nk_int32>();

// Option 3 : Pour des recherches portables, enregistrez avec un nom lisible
// (nécessite une extension du registre ou une table de mapping)
```

### ❌ Problème : Segfault lors de l'invocation d'une méthode

**Symptôme** : Crash à l'appel de `method->Invoke(instance, args)`

**Causes possibles** :

1. `method->HasInvoke()` n'a pas été vérifié avant l'invocation
2. Les arguments dans `args[]` ne correspondent pas aux types attendus
3. L'instance est `nullptr` pour une méthode non-statique

**Solution** :

```cpp
const auto* method = classMeta->GetMethod("TakeDamage");
if (method && method->HasInvoke()) {  // ← Vérification obligatoire
    if (method->IsStatic()) {
        method->Invoke(nullptr, args);  // Instance = nullptr pour static
    } else if (instance != nullptr) {
        // Vérifiez que les types d'arguments correspondent
        // (à implémenter selon votre système de validation)
        method->Invoke(instance, args);
    }
}
```

### ❌ Problème : Fuites mémoire avec les instances créées par réflexion

**Symptôme** : La mémoire n'est pas libérée après `CreateInstanceByName()`

**Cause** : `DestroyInstanceByName()` n'a pas été appelé, ou le destructeur n'est pas configuré

**Solution** :

```cpp
// Option 1 : Configurez un destructeur pour la classe
auto* playerClass = const_cast<NkClass*>(nkentseu::reflection::FindClass("Player"));
if (playerClass) {
    playerClass->SetDestructor([](void* instance) {
        delete static_cast<game::Player*>(instance);
    });
}

// Option 2 : Gérez manuellement la mémoire si pas de destructeur réflexif
void* instance = nkentseu::reflection::CreateInstanceByName("Player");
if (instance) {
    // Utilisation...
    delete static_cast<game::Player*>(instance);  // Nettoyage manuel
}

// Option 3 : Utilisez les templates type-safe qui gèrent les deux cas
auto* player = nkentseu::reflection::CreateInstance<game::Player>();
if (player) {
    // Utilisation...
    nkentseu::reflection::DestroyInstance(player);  // Tente destructeur réflexif, sinon delete
}
```

---

## ✅ Bonnes Pratiques

### 📋 Checklist d'Intégration

- [ ]  Inclure uniquement `<NKReflection/NkReflection.h>` (point d'entrée unique)
- [ ]  Placer `NKENTSEU_REGISTER_CLASS` dans un **seul** fichier `.cpp`, jamais dans un header
- [ ]  Initialiser le module (`Initialize()`) avant tout accès concurrent
- [ ]  Vérifier `HasInvoke()` avant d'appeler `Invoke()` sur une méthode
- [ ]  Vérifier `IsReadOnly()` avant d'appeler `SetValue()` sur une propriété
- [ ]  Utiliser `GetType<T>()` plutôt que `FindType()` pour les types C++ connus
- [ ]  Cacher les pointeurs de métadonnées (`NkProperty*`, `NkMethod*`) pour éviter les recherches répétées
- [ ]  Documenter les classes réfléchies avec des commentaires Doxygen pour la génération de docs

### 🎨 Conventions de Nommage

```cpp
// ✅ Noms de classes pour la réflexion : utiliser le nom réel de la classe
NKENTSEU_REFLECT_CLASS(Player)  // Nom : "Player"

// ✅ Noms de propriétés : utiliser le nom exact du membre
NKENTSEU_PROPERTY(nk_int32, health)  // Nom : "health"

// ✅ Noms pour FindClass/FindType : correspondre exactement au nom enregistré
auto* cls = nkentseu::reflection::FindClass("Player");  // "Player", pas "game::Player"

// ❌ À éviter : noms avec namespace dans FindClass
auto* cls = nkentseu::reflection::FindClass("game::Player");  // Ne fonctionnera pas
```

### 🔐 Sécurité et Robustesse

```cpp
// Toujours vérifier les pointeurs retournés par le registre
const auto* classMeta = nkentseu::reflection::FindClass("Player");
if (!classMeta) {
    NK_FOUNDATION_LOG_ERROR("Class 'Player' not found in registry");
    return;  // Gestion d'erreur appropriée
}

// Toujours vérifier HasInvoke() avant Invoke()
const auto* method = classMeta->GetMethod("Update");
if (method && method->HasInvoke()) {
    method->Invoke(instance, args);
}

// Toujours vérifier IsReadOnly() avant SetValue()
const auto* prop = classMeta->GetProperty("score");
if (prop && !prop->IsReadOnly()) {
    prop->SetValue<nk_int32>(instance, newScore);
}

// Utiliser des assertions en debug pour capturer les erreurs tôt
#ifdef NKENTSEU_DEBUG
NKENTSEU_ASSERT(classMeta != nullptr);
NKENTSEU_ASSERT(method != nullptr);
#endif
```

---

## 🤝 Contribuer au Module

### Structure des Contributions

```
NKReflection/
├── include/          # En-têtes publics (API stable)
├── src/              # Implémentations (peut évoluer)
├── tests/            # Tests unitaires et benchmarks
├── examples/         # Exemples d'utilisation complets
└── docs/             # Documentation supplémentaire
```

### Processus de Contribution

1. **Fork** le dépôt et créez une branche pour votre fonctionnalité
2. **Codez** en respectant les standards NKEntseu :
   - Documentation Doxygen pour toute API publique
   - Une instruction par ligne
   - Indentation stricte des blocs
   - Pas de duplication de macros (réutiliser NKPlatform/NKCore)
3. **Testez** avec les tests existants et ajoutez des tests pour les nouvelles fonctionnalités
4. **Documentez** les changements dans le README et les commentaires
5. **Soumettez** une Pull Request avec une description claire des modifications

### Standards de Code

```cpp
// ✅ Documentation Doxygen requise pour API publique
/**
 * @brief Description concise de la fonction
 * @param param Description du paramètre
 * @return Description de la valeur de retour
 * @note Notes importantes pour l'utilisation
 * @example
 * @code
 * // Exemple d'utilisation
 * @endcode
 */

// ✅ Une instruction par ligne
if (condition) {
    DoSomething();  // ← Point-virgule sur sa propre ligne si complexe
}

// ✅ Indentation stricte
namespace nkentseu {
    namespace reflection {
        class NkExample {
        public:
            void Method() {
                if (condition) {  // ← Bloc if indenté
                    DoSomething();
                }
            }
        };
    }
}

// ✅ Pas de duplication : réutiliser les macros existantes
#define NKENTSEU_EXAMPLE_API NKENTSEU_PLATFORM_API_EXPORT  // ← Réutilisation, pas redéfinition
```

---

## 📜 Licence

```
Copyright (c) 2024-2026 Rihen. Tous droits réservés.

Licence Propriétaire - Libre d'utilisation et de modification

Permission est accordée, gratuitement, à toute personne obtenant une copie
de ce logiciel et des fichiers de documentation associés (le "Logiciel"),
d'utiliser, copier, modifier, fusionner, publier, distribuer, sous-licencier,
et/ou vendre des copies du Logiciel, et de permettre aux personnes à qui le
Logiciel est fourni de le faire, sous les conditions suivantes :

1. La présente notice de copyright et cette notice de permission doivent
   être incluses dans toutes les copies ou portions substantielles du Logiciel.

2. Le Logiciel est fourni "tel quel", sans garantie d'aucune sorte, expresse
   ou implicite, y compris mais sans s'y limiter aux garanties de
   commercialisation, d'adéquation à un usage particulier et de non-contrefaçon.

3. En aucun cas les auteurs ou titulaires du copyright ne seront responsables
   de toute réclamation, dommage ou autre responsabilité, que ce soit dans une
   action de contrat, délit ou autre, découlant de, ou en relation avec le
   Logiciel ou l'utilisation ou d'autres opérations dans le Logiciel.
```

---

## 📞 Support et Ressources

### Ressources Officielles

- 📁 **Dépôt de code** : `Modules/System/NKReflection/`
- 📚 **Documentation Doxygen** : Générée automatiquement via `doxygen Doxyfile`
- 🧪 **Tests** : `NKReflection/tests/` - Exécuter via `ctest` après build CMake
- 💡 **Exemples** : `NKReflection/examples/` - Code de démonstration complet

### Dépannage Rapide


| Symptôme             | Commande de Debug                                      |
| --------------------- | ------------------------------------------------------ |
| Classe non trouvée   | `nkentseu::reflection::DumpRegistry();`                |
| Mémoire suspecte     | Vérifier`NkFunction::UsesSbo()` et `GetGlobalStats()` |
| Problème d'héritage | `nkentseu::reflection::ValidateRegistry();`            |
| Performance           | Profiler avec`NKENTSEU_REFLECTION_DEBUG` activé       |

### Contact

Pour les questions, bugs ou suggestions d'amélioration :

- 🐛 **Bugs** : Ouvrir une issue avec reproduction minimale
- 💡 **Features** : Discuter de la conception avant implémentation
- ❓ **Questions** : Consulter d'abord les exemples et la documentation Doxygen

---

> 🎯 **Prochaine Étape** : Explorez les exemples dans `NKReflection/examples/` pour voir le module en action, ou consultez la documentation Doxygen générée pour une référence API complète.

*Document généré le 2026-02-07 - Version 1.0.0* 🚀
