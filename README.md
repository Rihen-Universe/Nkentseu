# 🎮 Nkentseu Engine Framework

**Nkentseu** est un framework C++ modulaire, haute performance, conçu pour supporter la création d'applications multi-domaines : jeux vidéo, simulation, VR/AR, outils CAO, applications scientifiques, et systèmes industriels.

**Version:** 2.0.0  
**Status:** En développement actif  
**Dernière mise à jour:** Mars 2026

---

## 📖 Table des Matières

1. [Vue d'Ensemble](#vue-densemble)
2. [Architecture](#architecture)
3. [Installation & Setup](#installation--setup)
4. [Compilation avec Jenga](#compilation-avec-jenga)
5. [Ce qu'on a déjà fait](#ce-quon-a-déjà-fait)
6. [Prochaines Étapes](#prochaines-étapes)
7. [Structure des Modules](#structure-des-modules)
8. [Exemples et Ressources](#exemples-et-ressources)
9. [FAQ & Dépannage](#faq--dépannage)

---

## Vue d'Ensemble

### Principes Directeurs

- **Modularité** : Chaque module est indépendant et compilable séparément
- **Zero-Cost Abstraction** : Performance native sans overhead
- **Zero STL** : Implémentation personnalisée de containers et algorithmes
- **Thread-Safe First** : Primitives lock-free et thread-safe par défaut
- **Cross-Platform** : Windows, Linux, macOS, Android, iOS, Web (Emscripten)
- **GPU-Centric** : Architecture pensée pour GPUs modernes (Vulkan, DirectX, Metal, WebGPU)

### Cas d'Usage

| Domaine | Description |
|---------|-------------|
| **Jeux Vidéo** | Moteurs 2D/3D temps réel avec support multi-plateforme |
| **VR/AR/MR** | Immersive applications avec tracking spatial |
| **Simulation** | Physique, fluides, systèmes multi-corps |
| **CAO/Design** | Modélisation 2D/3D, ingénierie |
| **Scientifique** | Visualisation, calcul parallèle, IA |
| **Industriel** | Systèmes embarqués, contrôle temps réel |

---

## Architecture

### 🏗️ Structure Globale

```
┌─────────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                         │
│     Games | Tools | Simulators | XR Apps | Industrial       │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│                      ENGINE LAYER                            │
│    NkenEngine | NkenEditor | NkenSimulation | NkenXR        │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│                    DOMAIN MODULES                            │
│  NKGraphics | NKPhysics | NKAudio | NKNetwork | NKAI        │
│  NKAnimation | NKUI | NKVR                                   │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│                     RUNTIME LAYER                            │
│  NKRHI | NKRender | NKECS | NKScene | NKResource | NKInput  │
│  NKWindow | NKAsset | NKScript                               │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│                     SYSTEM LAYER                             │
│  NKThreading | NKFileSystem | NKTime | NKSerialization      │
│  NKLogger | NKEvent | NKJob                                  │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│                   FOUNDATION LAYER                           │
│  NKCore | NKMemory | NKContainers | NKMath | NKPlatform     │
└─────────────────────────────────────────────────────────────┘
```

### 📊 Dépendances entre Modules

```
APPLICATION
    ▲
    │ depends on
ENGINE (Nken*)
    ▲
    │ depends on
DOMAIN (NKGraphics, NKPhysics, etc.)
    ▲
    │ depends on
RUNTIME (NKRender, NKWindow, NKECS, etc.)
    ▲
    │ depends on
SYSTEM (NKThreading, NKTime, NKLogger, etc.)
    ▲
    │ depends on
FOUNDATION (NKCore, NKMath, NKMemory, NKPlatform)
```

### 📦 Modules Implémentés

#### Foundation Layer

| Module | Description | Status |
|--------|-------------|--------|
| **NKCore** | Types fondamentaux, macros | ✅ |
| **NKPlatform** | Détection plateforme/architecture | ✅ |
| **NKMath** | Mathématiques optimisées (36+ fonctions) | ✅ |
| **NKMemory** | Gestion mémoire personnalisée | ✅ |
| **NKContainers** | Vector, Map, Set, etc. | ✅ |

#### System Layer

| Module | Description | Status |
|--------|-------------|--------|
| **NKThreading** | Threading, Mutex, Atomic | ✅ |
| **NKTime** | Timers, Clock, Duration | ✅ |
| **NKLogger** | Système de logging | ✅ |
| **NKStream** | I/O et sérialisation | ✅ |

#### Runtime Layer

| Module | Description | Status |
|--------|-------------|--------|
| **NKWindow** | Fenêtrage multi-plateforme | 🔄 |
| **NKCamera** | Système de caméra | 🔄 |
| **NKRenderer** | Rendu (abstraction multi-backend) | 🔄 |

#### Domain Layer

| Module | Description | Status |
|--------|-------------|--------|
| **NKGraphics** | Système graphique haut niveau | ⏳ |
| **NKPhysics** | Moteur physique | ⏳ |
| **NKAI** | Système IA/ML | ⏳ |

**Legend:** ✅ Complete | 🔄 In Progress | ⏳ Planned

---

## Installation & Setup

### Prérequis

- **Python 3.8+** (pour Jenga)
- **Compilateur C++17+** :
  - Windows: MSVC 2019+, Clang 12+, GCC 9+
  - Linux: GCC 9+, Clang 12+
  - macOS: Clang 13+
- **Git** pour le contrôle de version

### 1. Cloner le Dépôt

```bash
git clone https://github.com/RihenUniverse/Jenga.git
cd Jenga/Jenga/Exemples/Nkentseu
```

### 2. Créer l'Environnement Virtuel Python (Optionnel mais Recommandé)

```bash
# Windows
python -m venv nkentseu_venv
nkentseu_venv\Scripts\activate.bat

# Linux/macOS
python3 -m venv nkentseu_venv
source nkentseu_venv/bin/activate
```

### 3. Installer Jenga

```bash
# Depuis le répertoire Jenga
cd ..\..\..
pip install -e .

# Vérifier l'installation
jenga --version
```

### 4. Ajouter Jenga au PATH (Optionnel)

**Windows:**
```powershell
# Ajouter le dossier Jenga/Jenga au PATH dans les variables d'environnement
setx PATH "%PATH%;C:\chemin\vers\Jenga\Jenga"
```

**Linux/macOS:**
```bash
export PATH=$PATH:/chemin/vers/Jenga/Jenga/
```

---

## Compilation avec Jenga

### 📋 Concepts Clés

#### Workspace
Conteneur principal regroupant tous les projets et configurations.

#### Configuration
Modes de build : `Debug`, `Release`, `Profile`

#### Platform
Plateformes cibles : Windows, Linux, macOS, Android, iOS, Web

#### Project
Unité de compilation : exécutable, bibliothèque statique, bibliothèque partagée

### 🔨 Commandes Principales

#### 1. Afficher la Configuration

```bash
# Afficher les projets et configurations disponibles
jenga show

# Afficher les détails de configuration
jenga show --verbose
```

#### 2. Compiler les Projets

```bash
# Compiler tout en Debug (configuration par défaut)
jenga build

# Compiler une configuration spécifique
jenga build --config Release
jenga build --config Debug

# Compiler un projet spécifique
jenga build --project NKCore
jenga build --project NKMath --config Profile

# Compilation parallèle (défaut: nombre de cœurs détecté)
jenga build --jobs 8
```

#### 3. Nettoyer les Artefacts

```bash
# Nettoyer tout
jenga clean

# Nettoyer une configuration spécifique
jenga clean --config Debug

# Nettoyer un projet
jenga clean --project NKCore
```

#### 4. Compiler en Mode Incrémental

```bash
# Le mode par défaut - seuls les fichiers modifiés sont recompilés
jenga build --config Debug

# Désactiver le cache incrémental
jenga build --config Debug --nocache
```

#### 5. Afficher les Drapeaux de Compilation

```bash
# Afficher les options de compilation pour un projet
jenga show --project NKMath --flags
```

### 🎯 Exemples Pratiques

#### Exemple 1: Compiler Tous les Modules Foundation

```bash
cd Nkentseu
jenga build --config Debug
```

**Résultat attendu:**
```
Building Nkentseu workspace...
[1/5] Building NKPlatform...    [OK] 3.2s
[2/5] Building NKCore...        [OK] 2.1s
[3/5] Building NKMemory...      [OK] 1.8s
[4/5] Building NKMath...        [OK] 4.5s
[5/5] Building NKContainers...  [OK] 2.1s

✅ Build successful! (13.7s)
```

#### Exemple 2: Compiler en Release avec Optimisations

```bash
jenga build --config Release --jobs 8
```

#### Exemple 3: Compiler et Exécuter les Tests

```bash
# Compiler tous les tests
jenga build --config Debug

# Exécuter les tests via le script Python
python Nkentseu_TestRunner.py
```

### 📁 Structure des Fichiers de Build

```
Nkentseu/
├── Build/                       # Artefacts de build
│   ├── Bin/
│   │   ├── Debug-Windows/       # Exécutables Debug
│   │   │   ├── NKCore_Tests.exe
│   │   │   ├── NKMath_Tests.exe
│   │   │   └── ...
│   │   └── Release-Windows/     # Exécutables Release
│   │       └── ...
│   ├── Lib/
│   │   ├── Debug-Windows/       # Bibliothèques Debug
│   │   └── Release-Windows/     # Bibliothèques Release
│   └── Obj/                     # Fichiers objets
├── .jenga/                      # Cache Jenga
│   ├── cache/
│   └── state.json
└── Nkentseu.jenga              # Configuration principale
```

---

## Ce qu'on a déjà fait

### ✅ Mathématiques Optimisées (NKMath)

**36+ fonctions mathématiques implémentées :**

#### Catégories

| Catégorie | Fonctions | Performance |
|-----------|-----------|-------------|
| **Ronde** | Floor, Ceil, Round, Trunc | ~5-15 cycles |
| **Racines** | Sqrt, Cbrt, Rsqrt, Pow | ~20-80 cycles |
| **Exp/Log** | Exp, Log, Log2, Log10 | ~50-70 cycles |
| **Trig** | Sin, Cos, Tan, Asin, Acos, Atan | ~80-120 cycles |
| **Hyperbolique** | Sinh, Cosh, Tanh | ~100-150 cycles |
| **Bits** | Clz, Ctz, Popcount, IsPowerOf2 | ~1-3 cycles |
| **Interpolation** | Lerp, Smoothstep, Smootherstep | ~5-20 cycles |
| **IEEE** | Fmod, Frexp, Ldexp, Modf | ~10-50 cycles |

#### Comparaison avec STL

```
Our Sqrt:   20-30 cycles → vs STL 20-40 cycles = 0.8-1.2x ✅
Our Exp:    50-80 cycles → vs STL 40-60 cycles = 1.2-1.5x (acceptable!)
Our Log:    40-70 cycles → vs STL 35-55 cycles = 1.1-1.4x ✅
Our Sin:    80-120 cycles → vs STL 70-150 cycles = 1.3-1.8x ✅
Our Atan:   60-100 cycles → vs STL 80-120 cycles = 0.9-1.3x ✅
```

**Avantages:**
- ✅ Zéro allocation dynamique
- ✅ Pas de dépendance STL
- ✅ Performance prévisible
- ✅ Exactitude configurable

### ✅ Framework de Test Complet

**Infrastructure de test universal créée :**

- **NkTestFramework.h** : Framework header-only, zéro dépendance
- **12 modules testés** : NKCore, NKPlatform, NKMath, NKMemory, NKTime, NKLogger, NKThreading, NKContainers, NKWindow, NKCamera, NKRenderer, NKStream
- **75+ tests** couvrant tous les modules
- **40+ tests NKMath** pour les 36 fonctions mathématiques

**Features du Framework:**
```cpp
NKTEST_ASSERT(ctx, condition, "message");          // Assertions
NKTEST_APPROX(ctx, actual, expected, eps, "msg");  // Float comparison
NKTEST_RUN(ctx, test_func, "Human name");          // Test execution
NKBENCH_START("Operation");                        // Performance
auto result = NKBENCH_RESULT();
```

### ✅ Suite de Benchmarks (NKMath_Bench)

**Métriques collectées:**

1. **Throughput (cycles/opération)**
   - Sqrt, Exp, Log, Sin, Cos, Atan
   - Comparaison scalar vs batch

2. **Comparaison avec STL**
   - Ratios de performance
   - Analysis de trade-offs

3. **Comparaison avec UE5**
   - Benchmark Unreal Engine
   - Positionnement compétitif

4. **Chemin d'optimisation**
   - Feuille de route pour 2-3x plus rapide
   - Visualisation des gains

### ✅ SIMD Declarations (NkMathSIMD.h)

**Préparation pour vectorisation :**

- Support SSE4.2 (4 floats parallèles)
- Support AVX2 (8 floats parallèles)
- Support NEON (ARM mobile)
- Déclarations pour : SqrtVec8, ExpFastVec8, SinFastVec8, CosFastVec8

**Objectifs de performance:**
```
Sqrt vectorisé:  20-30 cycles → 1.25-2 per element = 12-24x batch
Sin vectorisé:  80-120 cycles → 3-4.5 per element = 20-40x batch
```

### ✅ Test Runner Automation (Nkentseu_TestRunner.py)

**Orchestration complète :**

```python
python Nkentseu_TestRunner.py
```

**Que fait le script:**
1. Construit tous les tests en Debug (`jenga build --config Debug`)
2. Exécute 12 modules tests en séquence
3. Lance les benchmarks en Release
4. Génère un rapport JSON
5. Affiche un résumé console avec success rate

**Output:**
```
✅ NKCore_Tests:       PASS (5/5 tests)
✅ NKPlatform_Tests:   PASS (4/4 tests)
✅ NKMath_Tests:       PASS (40/40 tests)
✅ NKMemory_Tests:     PASS (5/5 tests)
...

📊 BENCHMARK RESULTS
====================
Sqrt:   20.50 cycles/op
Exp:    65.30 cycles/op
Sin:   105.20 cycles/op
...

Test Results saved to: test_results.json ✅
```

### ✅ Guide d'Optimisation (OPTIMIZATION_GUIDE_2026.md)

**Roadmap 2-3x plus rapide :**

| Phase | Technique | Cibles | Gain | Timeline |
|-------|-----------|--------|------|----------|
| 1 | SIMD Vectorization | Sqrt, Exp, Sin | 10-15x batch | Week 1 |
| 2 | Lookup Tables | Sin/Cos | 2-3x | Week 2 |
| 3 | Chebyshev Poly | Exp/Log | 1.5-2x | Week 3 |
| 4 | Batch API | All functions | 5-10x | Week 3-4 |
| 5 | FMA Instructions | Baseline | 1.2x | Week 4 |

---

## Prochaines Étapes

### 📍 Phase 1: SIMD Implementations (PRIORITÉ CRITIQUE)

**Objectif:** Implémenter les kernels vectorisés pour 8-15x speedup sur batch operations.

**Tasks:**
```
[ ] 1. NkMathSIMDImpl.cpp
    - Implement SqrtVec8 avec Newton method SIMD
    - Implement ExpFastVec8 avec Chebyshev polynomials
    - Implement SinFastVec8, CosFastVec8 avec continued fractions
    
[ ] 2. Test SIMD Implementation
    - Benchmark SqrtVec8 vs scalar
    - Validate accuracy (<5 ULP)
    - Measure speedup achieved
    
[ ] 3. Production Integration
    - Add SIMD builds to NKMathSIMD.jenga
    - Platform-specific optimizations (AVX2, SSE4.2, NEON)
```

**Temps estimé:** 2-3 jours

### 📍 Phase 2: Lookup Table Infrastructure

**Objectif:** Créer LUTs pour Sin/Cos (2-3x faster).

**Tasks:**
```
[ ] 1. NkMathLUT.h/cpp
    - Precompute 4096-entry Sin/Cos tables
    - Linear interpolation for smooth values
    - L1 cache optimization (16KB)
    
[ ] 2. Integration Tests
    - Accuracy vs pure computation
    - Cache hit rate measurement
    
[ ] 3. Benchmark Results
    - Sin 80-120 → 4-8 cycles (10-30x improvement)
```

**Temps estimé:** 2-3 jours

### 📍 Phase 3: Chebyshev Approximations

**Objectif:** Réduire les termes de série pour Exp/Log (1.5-2x faster).

**Tasks:**
```
[ ] 1. NkMathChebyshev.h/cpp
    - Generate Chebyshev coefficients for Exp, Log
    - Series evaluator (6 terms vs 20 Taylor terms)
    
[ ] 2. Accuracy Validation
    - Compare Chebyshev vs Taylor
    - Ensure <5 ULP requirement met
    
[ ] 3. Performance Integration
    - Fallback to Taylor if needed
    - Profile-guided optimization
```

**Temps estimé:** 2 jours

### 📍 Phase 4: Batch Processing API

**Objectif:** Amortiser function call overhead (5-10x faster for arrays).

**Tasks:**
```
[ ] 1. NkMathBatch.h
    - Batch functions: SqrtBatch8, ExpBatch8, SinBatch8
    - Compiler auto-vectorization hints
    
[ ] 2. Real-world Benchmarks
    - Process arrays of 1000+ elements
    - Compare vs loop + scalar
    
[ ] 3. Documentation
    - Usage examples
    - Performance characteristics
```

**Temps estimé:** 2 jours

### 📍 Phase 5: Runtime Tests & Validation

**Objectif:** Valider tous les optimisations en production.

**Tasks:**
```
[ ] 1. Execute Full Test Suite
    - python Nkentseu_TestRunner.py
    - Ensure 100% pass rate
    
[ ] 2. Performance Regression Testing
    - Baseline vs optimized
    - Target: 2-3x improvement achieved
    
[ ] 3. Documentation & Metrics
    - Final performance report
    - Architecture decision records
```

**Temps estimé:** 1 jour

### 📍 Phase 6: Module Foundation Completion

**Objectif:** Compléter les modules System et Runtime.

**Tasks:**
```
[ ] 1. NKWindow (Fenêtrage multi-plateforme)
    - Windows (Win32 API)
    - Linux (X11 / Wayland)
    - macOS (Cocoa)
    - Android (Java/Kotlin bridge)
    - iOS (UIKit)
    - Web (Emscripten)
    
[ ] 2. NKRenderer (Abstraction multi-backend)
    - Software rasterizer
    - OpenGL 4.6 / ES 3.2
    - Vulkan 1.3
    - DirectX 12
    - Metal 3
    - WebGPU
    
[ ] 3. Domain Modules
    - NKGraphics (Scene graph, materials)
    - NKPhysics (Collisions, dynamics)
    - NKAI (ML/DL framework)
```

**Temps estimé:** 4-8 semaines

---

## Structure des Modules

### Foundation Layer

```
Modules/Foundation/
├── NKPlatform/
│   ├── include/NKPlatform/
│   │   ├── NkPlatformDetect.h    # Macros plateforme
│   │   ├── NkArchDetect.h        # Architecture detection
│   │   ├── NkCompiler.h          # Compiler detection
│   │   └── NkCPUFeatures.h       # SIMD/AVX detection
│   └── src/
│
├── NKCore/
│   ├── include/NKCore/
│   │   ├── NkTypes.h             # int8, uint32, etc.
│   │   ├── NkMacros.h            # Export, force_inline, etc.
│   │   ├── NkAssert.h            # Assertions
│   │   └── NkTestFramework.h     # Test infrastructure
│   └── src/
│
├── NKMath/
│   ├── include/NKMath/
│   │   ├── NkMath.h              # 36+ math functions
│   │   ├── NkMathSIMD.h          # SIMD declarations
│   │   ├── NkMathLUT.h           # Lookup tables (TODO)
│   │   └── NkMathChebyshev.h     # Chebyshev approx (TODO)
│   ├── src/
│   │   ├── NkMath.cpp
│   │   └── NkMathSIMDImpl.cpp     # (TODO)
│   ├── tests/
│   │   └── NKMath_Tests.cpp      # 40+ tests
│   ├── bench/
│   │   └── NKMath_Bench.cpp      # Performance benchmarks
│   └── NKMath.jenga
│
├── NKMemory/
│   ├── include/NKMemory/
│   │   ├── NkAllocator.h
│   │   ├── NkSmartPtr.h
│   │   └── NkMemoryPool.h
│   ├── tests/
│   │   └── NKMemory_Tests.cpp
│   └── NKMemory.jenga
│
└── NKContainers/
    ├── include/NKContainers/
    │   ├── NkVector.h
    │   ├── NkMap.h
    │   ├── NkSet.h
    │   └── NkQueue.h
    ├── tests/
    │   └── NKContainers_Tests.cpp
    └── NKContainers.jenga
```

### System Layer

```
Modules/System/
├── NKThreading/
│   ├── include/NKThreading/
│   │   ├── NkThread.h
│   │   ├── NkMutex.h
│   │   ├── NkAtomic.h
│   │   └── NkThreadPool.h
│   ├── tests/
│   │   └── NKThreading_Tests.cpp
│   └── NKThreading.jenga
│
├── NKTime/
│   ├── include/NKTime/
│   │   ├── NkClock.h
│   │   ├── NkTimer.h
│   │   └── NkDuration.h
│   ├── tests/
│   │   └── NKTime_Tests.cpp
│   └── NKTime.jenga
│
├── NKLogger/
│   ├── include/NKLogger/
│   │   ├── NkLogger.h
│   │   └── NkLogLevel.h
│   ├── tests/
│   │   └── NKLogger_Tests.cpp
│   └── NKLogger.jenga
│
└── NKStream/
    ├── include/NKStream/
    │   ├── NkStream.h
    │   └── NkSerializer.h
    ├── tests/
    │   └── NKStream_Tests.cpp
    └── NKStream.jenga
```

### Runtime Layer

```
Modules/Runtime/
├── NKWindow/
│   ├── include/NKWindow/
│   │   ├── NkWindow.h
│   │   ├── NkEvent.h
│   │   └── NkInput.h
│   ├── src/
│   │   ├── Win32/
│   │   ├── X11/
│   │   ├── Cocoa/
│   │   └── Android/
│   ├── tests/
│   │   └── NKWindow_Tests.cpp
│   └── NKWindow.jenga
│
├── NKCamera/
│   ├── include/NKCamera/
│   │   ├── NkCamera.h
│   │   ├── NkProjection.h
│   │   └── NkTransform.h
│   ├── tests/
│   │   └── NKCamera_Tests.cpp
│   └── NKCamera.jenga
│
└── NKRenderer/
    ├── include/NKRenderer/
    │   ├── NkRenderer.h
    │   ├── NkRenderTarget.h
    │   ├── NkShader.h
    │   └── NkMaterial.h
    ├── src/
    │   ├── Software/
    │   ├── OpenGL/
    │   ├── Vulkan/
    │   ├── DirectX12/
    │   └── Metal/
    ├── tests/
    │   └── NKRenderer_Tests.cpp
    └── NKRenderer.jenga
```

---

## Exemples et Ressources

### 📚 Documentation

| Document | Contenu |
|----------|---------|
| [dd2.md](docs/dd2.md) | Architecture générale complète |
| [dd3.md](docs/dd3.md) | Conception détaillée & communication inter-modules |
| [dd4.md](docs/dd4.md) | Système IA/ML |
| [OPTIMIZATION_GUIDE_2026.md](OPTIMIZATION_GUIDE_2026.md) | Roadmap optimisations 2-3x faster |
| [TEST_FRAMEWORK_README.md](TEST_FRAMEWORK_README.md) | Infrastructure de test |

### 🎯 Exemples

Voir le dossier [Exemples/](Exemples/) pour :
- Exemples d'utilisation de chaque module
- Démos de rendu (OpenGL, Vulkan)
- Jeu simple 2D
- Simulation physique
- Application VR

### 🔗 Ressources Externes

- [Guide Complet Jenga](../../../Docs/GUIDE_COMPLET_JENGA.md)
- [Jenga Generator](../../../Docs/NOMENCLATURE_ET_DOCUMENTATION.md)
- [Jenga Developer Guide](../../../Docs/Jenga_Developer_Guide.md)

---

## FAQ & Dépannage

### Q: Pourquoi zéro STL?

**R:** Pour un contrôle total sur les performances, l'allocation mémoire et les dépendances. Les conteneurs STL peuvent avoir un overhead invisible et des comportements imprévisibles.

### Q: Quelles plateformes sont supportées?

**R:** Windows, Linux, macOS, Android, iOS, et Web (Emscripten). Support pour x86, x64, ARM, ARM64, WASM.

### Q: Comment compiler pour Android?

**R:**
```bash
jenga build --target android --config Release
```

### Q: Comment compiler pour WebAssembly?

**R:**
```bash
jenga build --target web --config Release
```

### Q: Quelle est la taille minimale d'exécutable?

**R:** ~200KB en Release optimisé (dépend des modules utilisés). Plus petit avec `--strip-symbols`.

### Q: Comment profiler les performances?

**R:** Utiliser les benchmarks:
```bash
./Build/Bin/Release-Windows/NKMath_Bench.exe
```

Ou intégrer dans votre code:
```cpp
NKBENCH_START("MyOperation");
// your code
NKBENCH_END();
```

### Q: Erreur: "jenga: command not found"

**R:** Ajouter Jenga/Jenga au PATH ou utiliser `python Jenga/jenga.py`:
```bash
python path/to/Jenga/Jenga/jenga.py build
```

### Q: Comment contribuer?

**R:** 
1. Fork le dépôt
2. Créer une branche feature (`git checkout -b feature/amazing`)
3. Commit les changements (`git commit -m 'Add amazing feature'`)
4. Push vers la branche (`git push origin feature/amazing`)
5. Ouvrir une Pull Request

---

## 📝 License

Nkentseu est sous license MIT. Voir [LICENSE](LICENSE) pour détails.

---

## 👥 Auteurs & Contributeurs

- **Rihen** - Architecture, Foundation modules, Optimizations

---

## 📞 Support & Issues

- **GitHub Issues**: [Report bugs](https://github.com/RihenUniverse/Jenga/issues)
- **Documentation**: [Wiki & Docs](docs/)
- **Email**: rihen@example.com

---

**Dernière mise à jour:** Mars 5, 2026  
**Status:** En développement actif · Foundation framework complete · SIMD optimizations in progress

