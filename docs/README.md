# Nkentseu

[![Status](https://img.shields.io/badge/Status-80%25%20Complete-orange)](#ce-qui-est-déjà-fait)
[![Build System](https://img.shields.io/badge/Build-Jenga-4c8bf5)](#installer-jenga)
[![Language](https://img.shields.io/badge/C%2B%2B-17%20%2F%2020-blue)](#vision)
[![Platforms](https://img.shields.io/badge/Platforms-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Android%20%7C%20iOS%20%7C%20Web-green)](#compatibilité-plateformes)

Framework C++ modulaire multi-plateforme orienté performance, construit avec **Jenga**.

## Pitch produit

Nkentseu est un socle unifié pour construire des applications natives haute performance:
- rendu temps réel,
- systèmes interactifs (window/input/events),
- runtime modulaire,
- fondations robustes (mémoire, conteneurs, threading, logs).

L’objectif produit est de réduire le temps de mise en production en gardant une architecture propre, mesurable et extensible.

## État rapide

- Base Foundation/System/Runtime structurée et intégrée.
- Build workspace opérationnel via `Nkentseu.jenga`.
- Modules clés déjà renforcés: `NKThreading`, `NKMemory`, `NKLogger`, `NKContainers` (partiel).
- Statut global communiqué dans les docs: **~80%**.

## Compatibilité plateformes

| Plateforme | Statut | Notes |
|---|---|---|
| Linux (x86_64) | ✅ Validé | Build régulier dans le workspace actuel |
| Windows (x86_64) | ⚠ Configuré | Toolchain/SDK requis selon environnement |
| macOS | ⚠ Configuré | Cible déclarée dans le workspace |
| Android | ⚠ Configuré | `ANDROID_SDK_ROOT` / `ANDROID_NDK_ROOT` requis |
| iOS | ⚠ Configuré | Toolchain Apple requis |
| Web (WASM) | ⚠ Configuré | Emscripten requis |
| HarmonyOS | 🧪 Expérimental | Cible déclarée, validation selon SDK |
| Xbox Series / One | 🧪 Expérimental | Cibles déclarées, validation toolchain dédiée |

## Ce README couvre

Ce README synthétise l’architecture générale à partir de:
- `docs/dd2.md`
- `docs/dd3.md`
- `docs/dd4.md`

et donne l’état actuel du projet, la suite prévue, puis les commandes d’installation/build.

## Vision

Nkentseu vise un socle technique unique pour:
- applications desktop/mobile,
- rendu temps réel,
- simulation,
- outillage,
- extensions IA/domaines spécialisés.

Principes clés:
- modularité stricte,
- dépendances en couches (pas de dépendances circulaires),
- portabilité (Windows/Linux/macOS/Android/iOS/Web),
- performance et contrôle mémoire/threading.

## Architecture générale

```text
┌──────────────────────────────────────────────────────────────┐
│ Applications                                                 │
│  Sandbox, outils, apps métier                               │
└──────────────────────────────────────────────────────────────┘
                            ▲
┌──────────────────────────────────────────────────────────────┐
│ Runtime                                                      │
│  NKWindow, NKRenderer, NKCamera                             │
└──────────────────────────────────────────────────────────────┘
                            ▲
┌──────────────────────────────────────────────────────────────┐
│ System                                                       │
│  NKLogger, NKTime, NKStream, NKThreading, NKFileSystem,     │
│  NKReflection, NKSerialization                               │
└──────────────────────────────────────────────────────────────┘
                            ▲
┌──────────────────────────────────────────────────────────────┐
│ Foundation                                                   │
│  NKPlatform, NKCore, NKMath, NKMemory, NKContainers         │
└──────────────────────────────────────────────────────────────┘
```

### Graphe de dépendances (workspace actuel)

```text
NKPlatform
  └─ NKCore
      ├─ NKMath
      ├─ NKLogger
      ├─ NKMemory
      │   └─ NKContainers
      ├─ NKTime
      ├─ NKStream
      ├─ NKThreading
      │   └─ NKFileSystem
      │       └─ NKReflection
      │           └─ NKSerialization
      └─ NKWindow
          └─ NKRenderer
              └─ Applications/Sandbox
```

Source de vérité build: `Nkentseu.jenga`.

## Ce qui est déjà fait

D’après `docs/COMPLETION_STATUS.md` et les intégrations récentes:

- **Build system débloqué**: ajout des modules `.jenga` manquants (`NKThreading`, `NKFileSystem`, `NKReflection`, `NKSerialization`).
- **NKThreading**: socle complet (mutex/spinlock/thread/condvar/threadpool/future).
- **NKMemory**: optimisations majeures (pool allocators, multi-level allocator, tracking/profiling, SIMD memory ops).
- **NKContainers**: optimisation partielle (croissance vecteur, variantes rapides `NkVectorFast`, `NkMapFast`).
- **NKLogger**:
  - format indexé `{i:p}` comme API principale (`Info`, `Debug`, etc.),
  - API `printf` explicite en suffixe `f` (`Infof`, `Logf`, etc.),
  - suppression des anciens wrappers `*Fmt`.

Statut global communiqué dans les docs: **~80% (phase intégration/optimisation finale)**.

## La suite (roadmap)

D’après `docs/OPTIMIZATION_ROADMAP.md`:

1. **Finaliser NKContainers**
   - `NkBTreeFast` orienté cache,
   - optimisation SSO string,
   - enrichissement des itérateurs.
2. **Validation intégration**
   - build complet,
   - tests unitaires ciblés threading/memory/containers,
   - baseline benchmarks.
3. **Hardening production**
   - intégration profiling mémoire,
   - audit lock-free / contention,
   - validation exports/symboles.
4. **Documentation de release**
   - API docs consolidée,
   - guide migration,
   - release notes.

## Installer Jenga

Depuis la racine du dépôt (`/mnt/e/.../Jenga`):

```bash
pip install -e .
```

Vérification:

```bash
python3 Jenga/jenga.py --help
# ou
bash Jenga/jenga.sh --help
```

Prérequis généraux:
- Python 3.8+
- compilateur C/C++ (clang/gcc/msvc selon plateforme)
- dépendances plateforme (X11/Wayland, SDK Android/iOS/Web, etc. selon cible)

## Compiler Nkentseu

Depuis `Jenga/Exemples/Nkentseu`:

```bash
# Build workspace complet (plateforme hôte)
jenga build

# Build Linux ciblé
jenga build --platform linux

# Build Windows ciblé
jenga build --platform windows

# Build d’un projet précis
jenga build --platform linux --target NKLogger
jenga build --platform linux --target SandboxGamepadPS3
```

## Tests et benchmarks

```bash
# Exécuter les tests
jenga test --platform linux

# Exécuter les tests d’un projet spécifique (si configuré)
jenga test --platform linux --project NKLogger

# Benchmarks via tests dédiés (selon modules)
jenga test --platform linux --project Sandbox
```

## Structure utile

```text
Nkentseu/
├── Nkentseu.jenga
├── Modules/
│   ├── Foundation/
│   ├── System/
│   └── Runtime/
├── Applications/
│   └── Sandbox/
└── docs/
    ├── dd2.md
    ├── dd3.md
    ├── dd4.md
    ├── COMPLETION_STATUS.md
    └── OPTIMIZATION_ROADMAP.md
```

## Références architecture

- `docs/dd2.md`: architecture complète en couches + objectifs techniques.
- `docs/dd3.md`: conception détaillée, communication inter-modules, subdivision fine.
- `docs/dd4.md`: projection architecture IA et extension vers domaines avancés.
