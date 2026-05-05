# 🕐 NKTime — Module de gestion du temps pour NKEntseu

[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Emscripten-lightgrey)](#plateformes-supportées)
[![C++ Standard](https://img.shields.io/badge/c%2B%2B-17-blue)](#prérequis)

> **NKTime** est un module de gestion du temps haute précision, sans dépendance STL, conçu pour les applications temps-réel, les moteurs de jeu et les systèmes embarqués.

---

## 📋 Table des matières

- [✨ Fonctionnalités](#-fonctionnalités)
- [🎯 Cas d'usage](#-cas-dusage)
- [📦 Installation](#-installation)
- [🚀 Démarrage rapide](#-démarrage-rapide)
- [📚 Référence API](#-référence-api)
- [🔧 Configuration du build](#-configuration-du-build)
- [💻 Plateformes supportées](#-plateformes-supportées)
- [⚡ Performance et précision](#-performance-et-précision)
- [🧪 Tests et validation](#-tests-et-validation)
- [🤝 Contribuer](#-contribuer)
- [📄 Licence](#-licence)

---

## ✨ Fonctionnalités

### 🎯 Précision haute résolution

- **Nanoseconde** : Mesures temporelles avec précision jusqu'à la nanoseconde
- **Monotone** : Horloges qui ne reculent jamais, même en cas de changement d'heure système
- **Multiplateforme** : Implémentations optimisées pour Windows (QPC), Linux/macOS (CLOCK_MONOTONIC), Web (Emscripten)

### 🧩 Types temporels spécialisés


| Type            | Usage                                     | Précision                              | Mutable |
| --------------- | ----------------------------------------- | --------------------------------------- | ------- |
| `NkDuration`    | Spécifier une durée (sleep, timeout)    | `int64` ns                              | ✅      |
| `NkElapsedTime` | Résultat de mesure de chrono             | `float64` ns + 4 unités précalculées | ❌      |
| `NkTimeSpan`    | Intervalle calendaire signé              | `int64` ns + décomposition             | ✅      |
| `NkTime`        | Heure quotidienne (HH:MM:SS.mmm.nnnnnn)   | Composants validés                     | ✅      |
| `NkDate`        | Date grégorienne avec validation         | Année [1601, 30827]                    | ✅      |
| `NkTimeZone`    | Fuseaux horaires et conversions UTC/local | DST supporté                           | ❌      |

### 🎮 Orchestration de boucle de jeu

```cpp
NkClock clock;
clock.SetFixedDelta(1.f / 60.f);  // Physique à 60 Hz

while (running) {
    const auto& t = clock.Tick();
    physics.Step(t.FixedScaled());  // Pas fixe avec time scale
    renderer.Draw(t.delta);          // Rendu avec delta réel
}
```

### 🔧 API sans dépendance STL

- Aucun `std::chrono`, `std::thread`, ou conteneurs STL
- Compatible avec les environnements embarqués et les kernels
- Allocation contrôlée : pas d'heap dans les chemins critiques

### 🧵 Thread-safety par design

- Types immuables après construction (`NkElapsedTime`, `NkTimeZone`)
- Méthodes `const` thread-safe sans verrou
- Documentation claire des garanties de concurrence

---

## 🎯 Cas d'usage

### 🎮 Moteur de jeu / Simulation

```cpp
#include "NKTime/NKTime.h"

void GameLoop() {
    using namespace nkentseu;
  
    NkClock clock;
    clock.SetFixedDelta(1.f / 50.f);  // 50 Hz pour la physique
  
    while (IsRunning()) {
        const auto& t = clock.Tick();
      
        // Physique déterministe avec pas fixe
        for (float acc = 0.f; acc < t.delta; acc += t.fixedDelta) {
            physics.Step(t.FixedScaled());
        }
      
        // Rendu interpolé avec delta réel
        renderer.InterpolateAndDraw(t.delta);
      
        // Cap de FPS optionnel
        if (t.fps > 144.f) {
            NkChrono::SleepMilliseconds(1);
        }
    }
}
```

### 🌐 Réseau et timeouts

```cpp
bool WaitForResponse(NkSocket& socket, float timeoutSeconds) {
    using namespace nkentseu;
  
    NkChrono timer;
    const NkDuration timeout = NkDuration::FromSeconds(timeoutSeconds);
  
    while (!socket.HasData()) {
        if (timer.Elapsed() > timeout) {
            return false;  // Timeout expiré
        }
        NkChrono::YieldThread();  // Cède le CPU sans bloquer
    }
    return true;
}
```

### 📊 Profiling et métriques

```cpp
void ProfileOperation() {
    using namespace nkentseu;
  
    NkChrono chrono;
  
    // ... code à profiler ...
  
    NkElapsedTime elapsed = chrono.Elapsed();
  
    NK_LOG_INFO("Operation: {} ({} FPS equivalent)",
                elapsed.ToString(),
                1.0 / elapsed.ToSeconds());
}
```

### 🌍 Conversions de fuseaux horaires

```cpp
void ConvertTimezones() {
    using namespace nkentseu;
  
    NkTime utcTime(14, 30, 0);  // 14:30 UTC
    NkDate refDate(2024, 7, 15);  // Date pour calcul DST
  
    NkTimeZone tokyo = NkTimeZone::FromName(NkString("UTC+9"));
    NkTime tokyoTime = tokyo.ToLocal(utcTime, refDate);  // 23:30
  
    NK_LOG_INFO("UTC: {}, Tokyo: {}",
                utcTime.ToString(),
                tokyoTime.ToString());
}
```

---

## 📦 Installation

### Prérequis

- **Compilateur** : C++17 ou supérieur (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake** : 3.15 ou supérieur (pour l'intégration build system)
- **Dépendances NKEntseu** : `NKCore`, `NKPlatform`, `NKContainers`

### Intégration via CMake

#### Option 1 : Submodule Git

```bash
# Dans votre projet
git submodule add https://github.com/votre-org/nkentseu-time.git extern/NKTime
git submodule update --init --recursive
```

```cmake
# CMakeLists.txt
add_subdirectory(extern/NKTime)

target_link_libraries(votre_cible PRIVATE NKTime::NKTime)
```

#### Option 2 : Package installé

```bash
# Après installation de NKTime
cmake -DCMAKE_PREFIX_PATH=/path/to/nkentseu-install ..
```

```cmake
find_package(NKTime REQUIRED)
target_link_libraries(votre_cible PRIVATE NKTime::NKTime)
```

#### Option 3 : Header-only (mode limité)

```cpp
// Définir avant toute inclusion
#define NKENTSEU_TIME_HEADER_ONLY
#include "NKTime/NKTime.h"
```

> ⚠️ Le mode header-only peut augmenter la taille du binaire car le code est dupliqué dans chaque unité de traduction.

---

## 🚀 Démarrage rapide

### Exemple minimal : Mesurer une opération

```cpp
#include "NKTime/NKTime.h"
#include <iostream>

int main() {
    using namespace nkentseu;
  
    NkChrono timer;
  
    // ... votre code ...
  
    NkElapsedTime elapsed = timer.Elapsed();
    std::cout << "Durée: " << elapsed.ToString() << std::endl;
  
    return 0;
}
```

### Exemple complet : Boucle de jeu avec pause et time scale

```cpp
#include "NKTime/NKTime.h"

void RunGame() {
    using namespace nkentseu;
  
    NkClock clock;
    clock.SetFixedDelta(1.f / 60.f);  // Physique à 60 FPS
  
    bool paused = false;
  
    while (true) {
        // Gestion des entrées
        if (KeyPressed(Key::P)) {
            paused = !paused;
            paused ? clock.Pause() : clock.Resume();
        }
        if (KeyPressed(Key::M)) {
            clock.SetTimeScale(paused ? 1.f : 0.25f);  // Slow-mo
        }
      
        // Avance de frame
        const auto& t = clock.Tick();
      
        // Mise à jour logique (affectée par time scale)
        if (!paused) {
            UpdateGameLogic(t.Scaled());
            physics.Step(t.FixedScaled());
        }
      
        // Rendu (toujours avec delta réel pour fluidité)
        RenderFrame(t.delta);
    }
}
```

---

## 📚 Référence API

### 🧭 Diagramme des types

```
NKTime Module
├── Types de base
│   ├── NkDuration      → Durée à spécifier (int64 ns, mutable)
│   ├── NkElapsedTime   → Résultat de mesure (float64, lecture seule)
│   └── NkTimeSpan      → Intervalle calendaire signé
│
├── Composants calendaires
│   ├── NkDate          → Date grégorienne (année, mois, jour)
│   ├── NkTime          → Heure quotidienne (HH:MM:SS.mmm.nnnnnn)
│   └── NkTimeZone      → Fuseaux horaires + conversions UTC/local
│
├── Chronométrage
│   ├── NkChrono        → Chronomètre haute précision + sleep/yield
│   └── NkClock         → Orchestrateur de frame pour game loop
│
└── Utilitaires
    ├── NkTimeConstants → Constantes de conversion centralisées
    └── NKTime.h        → Umbrella header (inclusion unique)
```

### 🔑 Méthodes essentielles

#### NkChrono — Chronomètre haute précision

```cpp
NkChrono chrono;                    // Démarre à la construction
NkElapsedTime elapsed = chrono.Elapsed();  // Lecture sans reset
NkElapsedTime delta = chrono.Reset();      // Lecture + reset atomique
NkChrono::Sleep(NkDuration::FromMilliseconds(16));  // Sleep du thread
NkChrono::YieldThread();            // Cède le CPU au scheduler
```

#### NkClock — Orchestrateur de frame

```cpp
NkClock clock;
clock.SetFixedDelta(1.f / 60.f);    // Pas fixe pour physique déterministe
clock.SetTimeScale(0.5f);           // Ralenti 2x la logique métier
clock.Pause(); / clock.Resume();    // Pause/Resume de la boucle

const auto& t = clock.Tick();       // Avance d'une frame
float logicDelta = t.Scaled();      // delta × timeScale
float physicsDelta = t.FixedScaled(); // fixedDelta × timeScale
```

#### NkDuration — Durée à spécifier

```cpp
// Fabriques statiques
auto d1 = NkDuration::FromMilliseconds(100);
auto d2 = NkDuration::FromSeconds(2.5f);
auto d3 = NkDuration::FromMinutes(1);

// Conversions
int64 ms = d1.ToMilliseconds();
float64 precise = d1.ToMillisecondsF();

// Arithmétique
auto total = d1 + d2;
auto scaled = d1 * 2.0;
auto ratio = d2 / d1;  // Retourne float64
```

#### NkElapsedTime — Résultat de mesure

```cpp
// Accès direct aux 4 unités précalculées (zéro calcul)
float64 ns = elapsed.nanoseconds;
float64 us = elapsed.microseconds;
float64 ms = elapsed.milliseconds;
float64 s  = elapsed.seconds;

// Ou via accesseurs nommés
float64 alsoMs = elapsed.ToMilliseconds();

// Formatage adaptatif
NkString formatted = elapsed.ToString();  // "45.678 ms", "1.234 s", etc.
```

#### NkTimeZone — Conversions de fuseaux

```cpp
// Fabriques
auto local = NkTimeZone::GetLocal();  // Fuseau système avec DST
auto utc = NkTimeZone::GetUtc();      // UTC fixe
auto fixed = NkTimeZone::FromName(NkString("UTC+5:30"));  // Offset fixe

// Conversions (avec date de référence pour DST)
NkTime utcTime(14, 30, 0);
NkDate refDate(2024, 7, 15);
NkTime localTime = local.ToLocal(utcTime, refDate);

// Informations
auto offset = local.GetUtcOffset(refDate);  // NkTimeSpan
bool isDst = local.IsDaylightSavingTime(refDate);
```

---

## 🔧 Configuration du build

### Options CMake

```cmake
option(NKTIME_BUILD_SHARED "Build NKTime as shared library" ON)
option(NKTIME_HEADER_ONLY "Use NKTime in header-only mode" OFF)
option(NKTIME_BUILD_TESTS "Build unit tests for NKTime" ON)
option(NKTIME_ENABLE_PROFILING "Enable internal profiling hooks" OFF)
```

### Macros de configuration


| Macro                            | Effet                                    | Usage typique                       |
| -------------------------------- | ---------------------------------------- | ----------------------------------- |
| `NKENTSEU_TIME_BUILD_SHARED_LIB` | Compile NKTime en DLL                    | Build de la bibliothèque partagée |
| `NKENTSEU_TIME_STATIC_LIB`       | Utilise NKTime en static                 | Application linkée statiquement    |
| `NKENTSEU_TIME_HEADER_ONLY`      | Mode header-only (tout inline)           | Petits projets, prototypes          |
| `NKENTSEU_TIME_DEBUG`            | Active les messages de debug compilation | Debug du système de build          |

### Exemple CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(NKTime VERSION 1.0.0 LANGUAGES CXX)

# Options
option(NKTIME_BUILD_SHARED "Build as shared library" ON)

# Configuration des defines
if(NKTIME_BUILD_SHARED)
    target_compile_definitions(nktime PRIVATE NKENTSEU_TIME_BUILD_SHARED_LIB)
else()
    target_compile_definitions(nktime PRIVATE NKENTSEU_TIME_STATIC_LIB)
endif()

# Dépendances
find_package(NKCore REQUIRED)
find_package(NKPlatform REQUIRED)

# Bibliothèque
add_library(nktime 
    src/NkChrono.cpp
    src/NkClock.cpp
    src/NkDate.cpp
    src/NkDuration.cpp
    src/NkElapsedTime.cpp
    src/NkTimeSpan.cpp
    src/NkTimeZone.cpp
    src/NkTimes.cpp
)

target_include_directories(nktime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(nktime PUBLIC
    NKCore::NKCore
    NKPlatform::NKPlatform
)

# Installation
install(TARGETS nktime EXPORT NKTimeTargets)
install(DIRECTORY include/NKTime DESTINATION include)
```

---

## 💻 Plateformes supportées


| Plateforme     | Horloge                          | Précision | Sleep                            | Notes                                      |
| -------------- | -------------------------------- | ---------- | -------------------------------- | ------------------------------------------ |
| **Windows**    | QueryPerformanceCounter          | ~100 ns    | `::Sleep()` (granularité ~15ms) | `timeBeginPeriod(1)` optionnel pour sub-ms |
| **Linux**      | `clock_gettime(CLOCK_MONOTONIC)` | ~1 ns      | `clock_nanosleep()`              | Préféré pour précision monotone        |
| **macOS**      | `clock_gettime(CLOCK_MONOTONIC)` | ~1 ns      | `nanosleep()`                    | Résolution dépend du hardware            |
| **Emscripten** | Browser high-res timer           | ~1 ms      | `emscripten_sleep()`             | Nécessite`-s ASYNCIFY=1`                  |
| **Embarqué**  | Platform-specific                | Variable   | Platform-specific                | Aucun dépendance STL requise              |

### Détection automatique

Le module détecte la plateforme via `NKPlatform/NkPlatformDetect.h`. Aucune configuration manuelle nécessaire dans la plupart des cas.

---

## ⚡ Performance et précision

### Benchmarks indicatifs (x86_64, 3.5 GHz)


| Opération                   | Cycles CPU | Temps moyen | Notes                              |
| ---------------------------- | ---------- | ----------- | ---------------------------------- |
| `NkChrono::Now()`            | ~25-40     | ~10-15 ns   | Appel système + conversion        |
| `NkChrono::Elapsed()`        | ~30-50     | ~15-20 ns   | Now() + soustraction float64       |
| `NkChrono::Reset()`          | ~35-55     | ~18-25 ns   | Elapsed() + affectation            |
| `NkElapsedTime` accès champ | ~1         | <1 ns       | Lecture mémoire directe           |
| `NkDuration` arithmétique   | ~3-5       | ~2-3 ns     | Opérations entières inline       |
| `NkClock::Tick()`            | ~50-80     | ~25-35 ns   | 2× chrono + construction snapshot |

### Caractéristiques de précision

```
Windows (QPC):
├─ Fréquence typique: 3-4 MHz
├─ Précision effective: ~100-300 ns
├─ Sleep granularité: ~15.6 ms (sans timeBeginPeriod)
└─ Overflow protection: Décomposition safe int64

POSIX (clock_gettime):
├─ Résolution nominale: 1 ns
├─ Précision effective: 1-100 ns selon hardware
├─ Sleep: clock_nanosleep (Linux) ou nanosleep
└─ Monotonic guarantee: CLOCK_MONOTONIC

Emscripten:
├─ Résolution: ~1 ms (limité par le navigateur)
├─ Sleep: emscripten_sleep() avec ASYNCIFY
└─ Yield: emscripten_sleep(0) pour boucles actives
```

### Optimisations appliquées

- ✅ **Précalcul des unités** : `NkElapsedTime` stocke ns/µs/ms/s pour accès O(1)
- ✅ **Inline agressif** : Méthodes triviales définies in-class avec `constexpr`/`inline`
- ✅ **Pas d'allocation** : Aucun `new`/`malloc` dans les chemins critiques
- ✅ **Cache-friendly** : Layout mémoire optimisé (NkElapsedTime = 32 bytes alignés)
- ✅ **Branchless quand possible** : Opérations arithmétiques sans conditions

---

## 🧪 Tests et validation

### Exécution des tests unitaires

```bash
# Build avec tests
cmake -DNKTIME_BUILD_TESTS=ON ..
cmake --build .

# Exécution
./tests/NKTimeTests
```

### Couverture des tests

- [X]  Constructeurs et fabriques de tous les types
- [X]  Conversions d'unités et précision des calculs
- [X]  Arithmétique et opérateurs de comparaison
- [X]  Conversions de fuseaux horaires (avec/without DST)
- [X]  Pause/Resume et time scale de NkClock
- [X]  Thread-safety des méthodes const
- [X]  Gestion des valeurs limites (overflow, underflow, zéro)

### Validation multiplateforme

Les tests sont exécutés automatiquement sur :

- ✅ Windows (MSVC 2019, MinGW)
- ✅ Linux (GCC 9+, Clang 10+)
- ✅ macOS (Clang, Xcode toolchain)
- ✅ Emscripten (via Node.js)

---

## 🤝 Contribuer

### Standards de code

Ce module suit les conventions strictes de NKEntseu :

- 📐 **Indentation** : 4 espaces, blocs `{}`, namespaces et sections de visibilité indentés
- 📝 **Commentaires** : Doxygen-style, expliquant le "pourquoi" pas juste le "quoi"
- 🔤 **Nommage** : `PascalCase` pour types, `camelCase` pour variables/fonctions, `UPPER_CASE` pour constantes
- 🚫 **Pas de duplication** : Réutilisation des macros NKPlatform, pas de redéfinition locale
- 📦 **Inclusions** : Ordre strict : pch.h → header correspondant → headers projet → headers système

### Processus de contribution

1. Forker le dépôt et créer une branche feature (`feature/ma-nouvelle-fonction`)
2. Implémenter les changements en respectant les standards ci-dessus
3. Ajouter des tests unitaires pour toute nouvelle fonctionnalité
4. Vérifier que tous les tests passent sur toutes les plateformes cibles
5. Soumettre une Pull Request avec description détaillée des changements

### Outils recommandés

```bash
# Formatage automatique (si configuré)
clang-format -i include/NKTime/*.h src/*.cpp

# Analyse statique
clang-tidy src/*.cpp -- -Iinclude -std=c++17

# Build avec warnings maximaux
cmake -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" ..
```

---

## 📄 Licence

```
Copyright © 2024-2026 TEUGUIA TADJUIDJE Rodolf Séderis

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

### Résumé des permissions

- ✅ Utilisation commerciale et non-commerciale libre
- ✅ Modification et distribution du code source
- ✅ Intégration dans des projets propriétaires
- ❌ Pas de garantie implicite (fourni "AS IS")
- ❌ Attribution requise dans les distributions

---

## 🔗 Ressources additionnelles

- 📖 [Documentation Doxygen générée](https://votre-docs-url/nktime)
- 🐛 [Signaler un bug](https://github.com/votre-org/nkentseu-time/issues)
- 💬 [Discussions et support](https://github.com/votre-org/nkentseu-time/discussions)
- 🔄 [Changelog et releases](https://github.com/votre-org/nkentseu-time/releases)

### Modules NKEntseu connexes


| Module         | Description                                 | Lien                                                |
| -------------- | ------------------------------------------- | --------------------------------------------------- |
| `NKCore`       | Types de base, assertions, logging          | [NKCore/README.md](../NKCore/README.md)             |
| `NKPlatform`   | Détection plateforme, macros d'export      | [NKPlatform/README.md](../NKPlatform/README.md)     |
| `NKContainers` | Conteneurs légers sans STL                 | [NKContainers/README.md](../NKContainers/README.md) |
| `NKMath`       | Mathématiques vectorielles et matricielles | [NKMath/README.md](../NKMath/README.md)             |

---

> 💡 **Conseil de pro** : Pour une intégration optimale, incluez `NKTime/NKTime.h` dans votre `pch.h` (precompiled header) et liez la cible `NKTime::NKTime` via CMake. Évitez les inclusions multiples dans un même fichier de traduction.

---

*Document généré pour NKTime v1.0.0 — Dernière mise à jour : $(date)*
