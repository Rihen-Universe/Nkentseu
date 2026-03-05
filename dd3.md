# 🏗️ ARCHITECTURE NKENTSEU - CONCEPTION DÉTAILLÉE COMPLÈTE

**Version:** 2.0
**Focus:** Subdivision modulaire, Communication inter-modules, Implémentation fichier par fichier

---

## 📋 TABLE DES MATIÈRES

1. [Vision Architecturale](https://claude.ai/chat/b58a719c-6f2f-4253-b6c3-9be4974e56be#1-vision-architecturale)
2. [Subdivision en Couches Principales](https://claude.ai/chat/b58a719c-6f2f-4253-b6c3-9be4974e56be#2-subdivision-en-couches-principales)
3. [Communication Inter-Modules](https://claude.ai/chat/b58a719c-6f2f-4253-b6c3-9be4974e56be#3-communication-inter-modules)
4. [Modules Foundation - Implémentation Détaillée](https://claude.ai/chat/b58a719c-6f2f-4253-b6c3-9be4974e56be#4-modules-foundation-implementation-detaillee)
5. [Bibliothèque de Fenêtrage et Événements](https://claude.ai/chat/b58a719c-6f2f-4253-b6c3-9be4974e56be#5-bibliotheque-de-fenetrage-et-evenements)
6. [Bibliothèque de Rendu (Abstraction Multi-Backend)](https://claude.ai/chat/b58a719c-6f2f-4253-b6c3-9be4974e56be#6-bibliotheque-de-rendu-abstraction-multi-backend)
7. [Scalabilité vers Domaines Avancés](https://claude.ai/chat/b58a719c-6f2f-4253-b6c3-9be4974e56be#7-scalabilite-vers-domaines-avances)
8. [Diagrammes de Communication](https://claude.ai/chat/b58a719c-6f2f-4253-b6c3-9be4974e56be#8-diagrammes-de-communication)

---

## 1. VISION ARCHITECTURALE

### 1.1 Principe Directeur

```
┌─────────────────────────────────────────────────────────────┐
│                    APPLICATIONS FINALES                      │
│  Jeux | Films | VR/AR/MR | Simulation | IA | Architecture   │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│                    MOTEURS SPÉCIALISÉS                       │
│  GameEngine | AnimEngine | VREngine | SimEngine | AIEngine  │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│                 SYSTÈMES HAUT NIVEAU                         │
│  Rendering | Physics | Audio | Animation | AI | Network     │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│            BIBLIOTHÈQUES MÉTIER                              │
│  RenderLib (Multi-Backend) | WindowLib | InputLib           │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│                   STL PERSONNALISÉE                          │
│  Containers | Memory | Math | Strings | Time | Threading    │
└─────────────────────────────────────────────────────────────┘
                            ▲
┌─────────────────────────────────────────────────────────────┐
│                   FONDATIONS                                 │
│  Platform | Core | Types | Assert                           │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 Les 3 Piliers Principaux

#### Pilier 1: **STL Personnalisée** (NKStandardLibrary)

* Containers (Vector, Map, Set, etc.)
* Memory Management (Allocators, Smart Pointers)
* Math (Vector, Matrix, Quaternion)
* Strings (String, StringView, Unicode)
* Time & Threading
* File I/O & Serialization

#### Pilier 2: **Windowing & Events** (NKWindowSystem)

* Window creation multi-plateforme
* Event system générique
* Input abstraction (Keyboard, Mouse, Gamepad, Touch)
* Context management (GL/Vulkan surface)

#### Pilier 3: **Rendering Abstraction** (NKRenderLib)

* Multi-backend (Software, OpenGL, Vulkan, DirectX, Metal, WebGPU)
* Unified API
* Render Graph
* Resource management
* Shader abstraction

---

## 2. SUBDIVISION EN COUCHES PRINCIPALES

### 2.1 Architecture Globale

```
┌──────────────────────────────────────────────────────────────┐
│ LAYER 7: APPLICATION                                         │
│  → Games, Tools, Simulations, VR Apps                        │
└──────────────────────────────────────────────────────────────┘
                            ▲
┌──────────────────────────────────────────────────────────────┐
│ LAYER 6: ENGINE                                              │
│  → NkenGameEngine, NkenAnimEngine, NkenVREngine              │
└──────────────────────────────────────────────────────────────┘
                            ▲
┌──────────────────────────────────────────────────────────────┐
│ LAYER 5: DOMAIN SYSTEMS                                      │
│  → Graphics, Physics, Audio, Animation, AI, Network          │
└──────────────────────────────────────────────────────────────┘
                            ▲
┌──────────────────────────────────────────────────────────────┐
│ LAYER 4: RENDERING LIBRARY (NKRenderLib)                     │
│  ├── NKRenderAbstraction (Unified API)                       │
│  ├── NKRenderSoftware (Software rasterizer)                  │
│  ├── NKRenderGL (OpenGL 4.6/ES 3.2)                          │
│  ├── NKRenderVulkan (Vulkan 1.3)                             │
│  ├── NKRenderDX (DirectX 12)                                 │
│  ├── NKRenderMetal (Metal 3)                                 │
│  └── NKRenderWebGPU (WebGPU)                                 │
└──────────────────────────────────────────────────────────────┘
                            ▲
┌──────────────────────────────────────────────────────────────┐
│ LAYER 3: WINDOWING & EVENTS (NKWindowSystem)                │
│  ├── NKWindow (Window creation)                              │
│  ├── NKInput (Input abstraction)                             │
│  ├── NKEvent (Event system)                                  │
│  └── NKContext (Graphics context)                            │
└──────────────────────────────────────────────────────────────┘
                            ▲
┌──────────────────────────────────────────────────────────────┐
│ LAYER 2: STL LIBRARY (NKStandardLibrary)                     │
│  ├── NKContainers (Vector, Map, Set, etc.)                   │
│  ├── NKMemory (Allocators, Smart Pointers)                   │
│  ├── NKMath (Vector, Matrix, Quaternion)                     │
│  ├── NKString (String, StringView, UTF-8/16/32)              │
│  ├── NKTime (Clock, Timer, Duration)                         │
│  ├── NKThreading (Thread, Mutex, Atomic)                     │
│  ├── NKFileSystem (File, Path, Directory)                    │
│  └── NKSerialization (JSON, XML, YAML, Binary)               │
└──────────────────────────────────────────────────────────────┘
                            ▲
┌──────────────────────────────────────────────────────────────┐
│ LAYER 1: FOUNDATION (NKFoundation)                           │
│  ├── NKPlatform (Platform detection)                         │
│  ├── NKCore (Types, Macros, Export)                          │
│  ├── NKAssert (Assertions)                                   │
│  └── NKLogger (Logging system)                               │
└──────────────────────────────────────────────────────────────┘
```

### 2.2 Dépendances entre Couches

```
Layer 7 (Application)
  ↓ depends on
Layer 6 (Engine)
  ↓ depends on
Layer 5 (Domain Systems)
  ↓ depends on
Layer 4 (RenderLib) + Layer 3 (WindowSystem)
  ↓ depends on
Layer 2 (STL)
  ↓ depends on
Layer 1 (Foundation)
```

**Règle d'or:** Une couche ne dépend **QUE** des couches inférieures. Jamais de dépendance circulaire.

---

## 3. COMMUNICATION INTER-MODULES

### 3.1 Patterns de Communication

#### Pattern 1: **Interfaces Abstraites** (Dependency Inversion)

```cpp
// Module A définit une interface
// NKRenderLib/include/NKRenderLib/INkRenderer.h
class INkRenderer {
public:
    virtual ~INkRenderer() = default;
    virtual void Draw(const DrawCall& call) = 0;
    virtual void Present() = 0;
};

// Module B implémente l'interface
// NKRenderVulkan/src/NkRendererVulkan.cpp
class NkRendererVulkan : public INkRenderer {
public:
    void Draw(const DrawCall& call) override { /* Vulkan impl */ }
    void Present() override { /* Vulkan present */ }
};

// Module C utilise l'interface
// NKGraphics/src/NkGraphicsEngine.cpp
class NkGraphicsEngine {
    INkRenderer* m_renderer;
public:
    void Render() {
        m_renderer->Draw(...);
        m_renderer->Present();
    }
};
```

#### Pattern 2: **Event System** (Observer)

```cpp
// NKEvent/include/NKEvent/NkEventBus.h
template<typename EventT>
class NkEventBus {
public:
    using CallbackFn = std::function<void(const EventT&)>;
  
    void Subscribe(CallbackFn callback) {
        m_callbacks.push_back(callback);
    }
  
    void Publish(const EventT& event) {
        for (auto& cb : m_callbacks) {
            cb(event);
        }
    }
private:
    std::vector<CallbackFn> m_callbacks;
};

// Usage
NkEventBus<KeyPressedEvent> keyBus;

// Module A subscribes
keyBus.Subscribe([](const KeyPressedEvent& e) {
    std::cout << "Key: " << e.keyCode << std::endl;
});

// Module B publishes
keyBus.Publish(KeyPressedEvent{NK_KEY_SPACE});
```

#### Pattern 3: **Service Locator** (Découplage)

```cpp
// NKCore/include/NKCore/NkServiceLocator.h
class NkServiceLocator {
public:
    template<typename T>
    static void Register(T* service) {
        auto typeId = typeid(T).hash_code();
        s_services[typeId] = static_cast<void*>(service);
    }
  
    template<typename T>
    static T* Get() {
        auto typeId = typeid(T).hash_code();
        return static_cast<T*>(s_services[typeId]);
    }
private:
    static std::unordered_map<size_t, void*> s_services;
};

// Usage
// Register
NkServiceLocator::Register<IFileSystem>(new NkFileSystem());

// Get
auto* fs = NkServiceLocator::Get<IFileSystem>();
fs->ReadFile("config.json");
```

#### Pattern 4: **Message Queue** (Async)

```cpp
// NKThreading/include/NKThreading/NkMessageQueue.h
template<typename MessageT>
class NkMessageQueue {
public:
    void Push(const MessageT& msg) {
        std::lock_guard lock(m_mutex);
        m_queue.push(msg);
    }
  
    bool Pop(MessageT& out) {
        std::lock_guard lock(m_mutex);
        if (m_queue.empty()) return false;
        out = m_queue.front();
        m_queue.pop();
        return true;
    }
private:
    std::queue<MessageT> m_queue;
    std::mutex m_mutex;
};

// Usage
NkMessageQueue<RenderCommand> renderQueue;

// Producer thread
renderQueue.Push(DrawMeshCommand{...});

// Consumer thread (render thread)
RenderCommand cmd;
while (renderQueue.Pop(cmd)) {
    ExecuteCommand(cmd);
}
```

#### Pattern 5: **Callbacks & Delegates**

```cpp
// NKContainers/include/NKContainers/NkDelegate.h
template<typename... Args>
class NkDelegate {
public:
    using FuncPtr = void(*)(Args...);
  
    void Bind(FuncPtr func) { m_func = func; }
    void Invoke(Args... args) { if (m_func) m_func(args...); }
private:
    FuncPtr m_func = nullptr;
};

// Usage
NkDelegate<int, int> onResize;
onResize.Bind([](int w, int h) {
    std::cout << "Resized: " << w << "x" << h << std::endl;
});
onResize.Invoke(1920, 1080);
```

### 3.2 Diagramme de Communication Global

```
┌─────────────────┐         ┌─────────────────┐
│  Application    │────────▶│  GameEngine     │
└─────────────────┘         └─────────────────┘
                                     │
                    ┌────────────────┼────────────────┐
                    ▼                ▼                ▼
            ┌───────────┐    ┌───────────┐   ┌───────────┐
            │ Graphics  │    │  Physics  │   │   Audio   │
            └───────────┘    └───────────┘   └───────────┘
                    │                                │
                    ▼                                ▼
            ┌───────────┐                    ┌───────────┐
            │ RenderLib │◀───────────────────│ WindowSys │
            └───────────┘   Context Creation └───────────┘
                    │                                │
         ┌──────────┼──────────┐                    │
         ▼          ▼          ▼                    ▼
    ┌─────┐   ┌─────┐   ┌─────┐            ┌──────────┐
    │ GL  │   │ VK  │   │ DX  │            │  Input   │
    └─────┘   └─────┘   └─────┘            └──────────┘
         │          │          │                    │
         └──────────┴──────────┘                    │
                    ▼                                ▼
            ┌───────────────────────────────────────┐
            │            STL Library                 │
            │  Containers | Memory | Math | String  │
            └───────────────────────────────────────┘
                             ▲
            ┌────────────────┴────────────────┐
            │         Foundation              │
            │  Platform | Core | Logger       │
            └─────────────────────────────────┘
```

### 3.3 Mécanismes de Communication par Type


| Communication                 | Mécanisme            | Exemple                    |
| ----------------------------- | --------------------- | -------------------------- |
| **Sync + Fortement couplé**  | Direct function call  | `renderer->Draw()`         |
| **Sync + Faiblement couplé** | Interface abstraction | `IRenderer* -> Draw()`     |
| **Async + Event-driven**      | Event Bus             | `EventBus::Publish(event)` |
| **Async + Decoupled**         | Message Queue         | `queue.Push(message)`      |
| **Service Discovery**         | Service Locator       | `Get<IFileSystem>()`       |
| **Callback-based**            | Delegates/Callbacks   | `onResize.Invoke()`        |

---

## 4. MODULES FOUNDATION - IMPLÉMENTATION DÉTAILLÉE

### 4.1 NKPlatform - Détection Plateforme

#### Fichiers à Implémenter

```
NKPlatform/
├── include/NKPlatform/
│   ├── NkPlatformDetect.h       ★ À implémenter
│   ├── NkArchDetect.h           ★ À implémenter
│   ├── NkCompilerDetect.h       ★ À implémenter
│   ├── NkCPUFeatures.h          ★ À implémenter
│   ├── NkEndianness.h           ★ À implémenter
│   └── NkPlatformConfig.h       ★ À implémenter
└── src/
    └── NkCPUFeatures.cpp        ★ À implémenter (runtime detection)
```

#### 4.1.1 NkPlatformDetect.h

**Rôle:** Détecter la plateforme (Windows/Linux/macOS/Android/iOS/Web) à compile-time

```cpp
// NKPlatform/include/NKPlatform/NkPlatformDetect.h
#pragma once

// ============================================================================
// PLATFORM DETECTION
// ============================================================================

// Windows
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
    #define NK_PLATFORM_WINDOWS 1
    #if defined(_WIN64)
        #define NK_PLATFORM_WINDOWS_64 1
    #else
        #define NK_PLATFORM_WINDOWS_32 1
    #endif
// Linux
#elif defined(__linux__) || defined(__linux) || defined(linux)
    #define NK_PLATFORM_LINUX 1
// macOS
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define NK_PLATFORM_MACOS 1
    #elif TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define NK_PLATFORM_IOS 1
    #endif
// Android
#elif defined(__ANDROID__)
    #define NK_PLATFORM_ANDROID 1
// Web (Emscripten)
#elif defined(__EMSCRIPTEN__)
    #define NK_PLATFORM_WEB 1
#else
    #error "Unknown platform"
#endif

// Platform Name
#if NK_PLATFORM_WINDOWS
    #define NK_PLATFORM_NAME "Windows"
#elif NK_PLATFORM_LINUX
    #define NK_PLATFORM_NAME "Linux"
#elif NK_PLATFORM_MACOS
    #define NK_PLATFORM_NAME "macOS"
#elif NK_PLATFORM_IOS
    #define NK_PLATFORM_NAME "iOS"
#elif NK_PLATFORM_ANDROID
    #define NK_PLATFORM_NAME "Android"
#elif NK_PLATFORM_WEB
    #define NK_PLATFORM_NAME "Web"
#endif

// Desktop vs Mobile
#if NK_PLATFORM_WINDOWS || NK_PLATFORM_LINUX || NK_PLATFORM_MACOS
    #define NK_PLATFORM_DESKTOP 1
#elif NK_PLATFORM_ANDROID || NK_PLATFORM_IOS
    #define NK_PLATFORM_MOBILE 1
#elif NK_PLATFORM_WEB
    #define NK_PLATFORM_WEB 1
#endif
```

**Utilisation:**

```cpp
#if NK_PLATFORM_WINDOWS
    // Code spécifique Windows
#elif NK_PLATFORM_LINUX
    // Code spécifique Linux
#endif
```

---

#### 4.1.2 NkArchDetect.h

**Rôle:** Détecter l'architecture CPU (x86/x64/ARM/ARM64/WASM)

```cpp
// NKPlatform/include/NKPlatform/NkArchDetect.h
#pragma once

// ============================================================================
// ARCHITECTURE DETECTION
// ============================================================================

// x86/x64
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    #define NK_ARCH_X64 1
    #define NK_ARCH_NAME "x64"
    #define NK_ARCH_BITS 64
#elif defined(__i386__) || defined(_M_IX86) || defined(__X86__)
    #define NK_ARCH_X86 1
    #define NK_ARCH_NAME "x86"
    #define NK_ARCH_BITS 32
// ARM
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define NK_ARCH_ARM64 1
    #define NK_ARCH_NAME "ARM64"
    #define NK_ARCH_BITS 64
#elif defined(__arm__) || defined(_M_ARM)
    #define NK_ARCH_ARM 1
    #define NK_ARCH_NAME "ARM"
    #define NK_ARCH_BITS 32
// WebAssembly
#elif defined(__wasm__) || defined(__wasm32__) || defined(__wasm64__)
    #define NK_ARCH_WASM 1
    #define NK_ARCH_NAME "WebAssembly"
    #if defined(__wasm64__)
        #define NK_ARCH_BITS 64
    #else
        #define NK_ARCH_BITS 32
    #endif
#else
    #error "Unknown architecture"
#endif

// Pointer size
#define NK_POINTER_SIZE (NK_ARCH_BITS / 8)

// SIMD Support (compile-time)
#if defined(__SSE__)
    #define NK_HAS_SSE 1
#endif
#if defined(__SSE2__)
    #define NK_HAS_SSE2 1
#endif
#if defined(__AVX__)
    #define NK_HAS_AVX 1
#endif
#if defined(__AVX2__)
    #define NK_HAS_AVX2 1
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #define NK_HAS_NEON 1
#endif
```

**Utilisation:**

```cpp
#if NK_ARCH_X64 && NK_HAS_AVX2
    // Code optimisé AVX2
#elif NK_ARCH_ARM64 && NK_HAS_NEON
    // Code optimisé NEON
#else
    // Code générique
#endif
```

---

#### 4.1.3 NkCompilerDetect.h

**Rôle:** Détecter le compilateur (MSVC/GCC/Clang) et version

```cpp
// NKPlatform/include/NKPlatform/NkCompilerDetect.h
#pragma once

// ============================================================================
// COMPILER DETECTION
// ============================================================================

// MSVC
#if defined(_MSC_VER)
    #define NK_COMPILER_MSVC 1
    #define NK_COMPILER_VERSION _MSC_VER
    #define NK_COMPILER_NAME "MSVC"
// Clang (must check before GCC)
#elif defined(__clang__)
    #define NK_COMPILER_CLANG 1
    #define NK_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100)
    #define NK_COMPILER_NAME "Clang"
// GCC
#elif defined(__GNUC__)
    #define NK_COMPILER_GCC 1
    #define NK_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100)
    #define NK_COMPILER_NAME "GCC"
#else
    #error "Unknown compiler"
#endif

// C++ Standard
#if __cplusplus >= 202002L
    #define NK_CPP20 1
#elif __cplusplus >= 201703L
    #define NK_CPP17 1
#elif __cplusplus >= 201402L
    #define NK_CPP14 1
#elif __cplusplus >= 201103L
    #define NK_CPP11 1
#else
    #error "C++11 minimum required"
#endif

// Attributes
#if NK_COMPILER_MSVC
    #define NK_FORCEINLINE __forceinline
    #define NK_NOINLINE __declspec(noinline)
    #define NK_RESTRICT __restrict
    #define NK_ALIGN(x) __declspec(align(x))
#elif NK_COMPILER_GCC || NK_COMPILER_CLANG
    #define NK_FORCEINLINE inline __attribute__((always_inline))
    #define NK_NOINLINE __attribute__((noinline))
    #define NK_RESTRICT __restrict__
    #define NK_ALIGN(x) __attribute__((aligned(x)))
#endif

// DLL Export/Import
#if NK_PLATFORM_WINDOWS
    #if NK_BUILD_SHARED
        #define NK_API __declspec(dllexport)
    #else
        #define NK_API __declspec(dllimport)
    #endif
#else
    #define NK_API __attribute__((visibility("default")))
#endif

// Likely/Unlikely (C++20 or compiler extension)
#if NK_CPP20
    #define NK_LIKELY [[likely]]
    #define NK_UNLIKELY [[unlikely]]
#elif NK_COMPILER_GCC || NK_COMPILER_CLANG
    #define NK_LIKELY __builtin_expect(!!(x), 1)
    #define NK_UNLIKELY __builtin_expect(!!(x), 0)
#else
    #define NK_LIKELY
    #define NK_UNLIKELY
#endif
```

**Utilisation:**

```cpp
NK_FORCEINLINE float Dot(const Vec3& a, const Vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

class NK_API MyClass {
    // Exporté dans DLL
};
```

---

#### 4.1.4 NkCPUFeatures.h + .cpp

**Rôle:** Détection runtime des features CPU (SIMD, cache size, etc.)

```cpp
// NKPlatform/include/NKPlatform/NkCPUFeatures.h
#pragma once
#include "NkArchDetect.h"

namespace Nk {

struct CPUFeatures {
    // SIMD (runtime check)
    bool hasSSE;
    bool hasSSE2;
    bool hasSSE3;
    bool hasSSE41;
    bool hasSSE42;
    bool hasAVX;
    bool hasAVX2;
    bool hasAVX512;
    bool hasNEON;
  
    // Cache info
    int cacheLineSize;
    int l1CacheSize;
    int l2CacheSize;
    int l3CacheSize;
  
    // CPU info
    int coreCount;
    int threadCount;
  
    static const CPUFeatures& Get();
private:
    CPUFeatures();
    static void DetectFeatures();
};

} // namespace Nk
```

```cpp
// NKPlatform/src/NkCPUFeatures.cpp
#include "NKPlatform/NkCPUFeatures.h"
#include <thread>

#if NK_ARCH_X86 || NK_ARCH_X64
    #include <cpuid.h>  // GCC/Clang
    #ifdef NK_COMPILER_MSVC
        #include <intrin.h>
    #endif
#endif

namespace Nk {

static CPUFeatures s_features;

const CPUFeatures& CPUFeatures::Get() {
    static bool initialized = false;
    if (!initialized) {
        DetectFeatures();
        initialized = true;
    }
    return s_features;
}

CPUFeatures::CPUFeatures()
    : hasSSE(false), hasSSE2(false), hasSSE3(false),
      hasSSE41(false), hasSSE42(false), hasAVX(false),
      hasAVX2(false), hasAVX512(false), hasNEON(false),
      cacheLineSize(64), l1CacheSize(32*1024),
      l2CacheSize(256*1024), l3CacheSize(8*1024*1024),
      coreCount(0), threadCount(0) {}

void CPUFeatures::DetectFeatures() {
#if NK_ARCH_X86 || NK_ARCH_X64
    int cpuInfo[4] = {0};
  
    // Get CPUID(1)
    #ifdef NK_COMPILER_MSVC
        __cpuid(cpuInfo, 1);
    #else
        __cpuid(1, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
    #endif
  
    // EDX register
    s_features.hasSSE   = (cpuInfo[3] & (1 << 25)) != 0;
    s_features.hasSSE2  = (cpuInfo[3] & (1 << 26)) != 0;
  
    // ECX register
    s_features.hasSSE3  = (cpuInfo[2] & (1 << 0))  != 0;
    s_features.hasSSE41 = (cpuInfo[2] & (1 << 19)) != 0;
    s_features.hasSSE42 = (cpuInfo[2] & (1 << 20)) != 0;
    s_features.hasAVX   = (cpuInfo[2] & (1 << 28)) != 0;
  
    // Get CPUID(7)
    #ifdef NK_COMPILER_MSVC
        __cpuidex(cpuInfo, 7, 0);
    #else
        __cpuid_count(7, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
    #endif
  
    s_features.hasAVX2    = (cpuInfo[1] & (1 << 5))  != 0;
    s_features.hasAVX512  = (cpuInfo[1] & (1 << 16)) != 0;
  
#elif NK_ARCH_ARM || NK_ARCH_ARM64
    // On ARM, assume NEON if compiled for ARM64
    #if NK_ARCH_ARM64
        s_features.hasNEON = true;
    #else
        // Check /proc/cpuinfo on Linux
        s_features.hasNEON = false; // TODO: runtime check
    #endif
#endif
  
    // Core/Thread count
    s_features.threadCount = std::thread::hardware_concurrency();
    s_features.coreCount = s_features.threadCount; // Approximation
}

} // namespace Nk
```

**Utilisation:**

```cpp
const auto& cpu = Nk::CPUFeatures::Get();
if (cpu.hasAVX2) {
    // Use AVX2 code path
} else if (cpu.hasSSE42) {
    // Use SSE4.2 code path
} else {
    // Fallback
}
```

---

#### 4.1.5 NkEndianness.h

**Rôle:** Détecter et convertir endianness

```cpp
// NKPlatform/include/NKPlatform/NkEndianness.h
#pragma once
#include <cstdint>

namespace Nk {

// Detect endianness at compile-time
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define NK_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
      __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define NK_BIG_ENDIAN 1
#elif defined(_WIN32)
    // Windows is always little-endian
    #define NK_LITTLE_ENDIAN 1
#else
    // Runtime check
    constexpr bool IsLittleEndian() {
        constexpr uint32_t value = 0x01020304;
        return (reinterpret_cast<const uint8_t*>(&value)[0] == 0x04);
    }
    #define NK_LITTLE_ENDIAN IsLittleEndian()
    #define NK_BIG_ENDIAN (!IsLittleEndian())
#endif

// Byte swap functions
inline uint16_t ByteSwap16(uint16_t value) {
    return (value >> 8) | (value << 8);
}

inline uint32_t ByteSwap32(uint32_t value) {
    return ((value >> 24) & 0x000000FF) |
           ((value >>  8) & 0x0000FF00) |
           ((value <<  8) & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
}

inline uint64_t ByteSwap64(uint64_t value) {
    return ((value >> 56) & 0x00000000000000FFULL) |
           ((value >> 40) & 0x000000000000FF00ULL) |
           ((value >> 24) & 0x0000000000FF0000ULL) |
           ((value >>  8) & 0x00000000FF000000ULL) |
           ((value <<  8) & 0x000000FF00000000ULL) |
           ((value << 24) & 0x0000FF0000000000ULL) |
           ((value << 40) & 0x00FF000000000000ULL) |
           ((value << 56) & 0xFF00000000000000ULL);
}

// Convert to/from network byte order (big-endian)
inline uint16_t HostToNetwork16(uint16_t value) {
    #if NK_LITTLE_ENDIAN
        return ByteSwap16(value);
    #else
        return value;
    #endif
}

inline uint32_t HostToNetwork32(uint32_t value) {
    #if NK_LITTLE_ENDIAN
        return ByteSwap32(value);
    #else
        return value;
    #endif
}

} // namespace Nk
```

**Utilisation:**

```cpp
uint32_t networkValue = Nk::HostToNetwork32(localValue);
```

---

### 4.2 NKCore - Types et Macros Fondamentaux

#### Fichiers à Implémenter

```
NKCore/
├── include/NKCore/
│   ├── NkTypes.h               ★ À implémenter
│   ├── NkTraits.h              ★ À implémenter
│   ├── NkLimits.h              ★ À implémenter
│   ├── NkAtomic.h              ★ À implémenter
│   ├── NkBits.h                ★ À implémenter
│   ├── NkMacros.h              ★ À implémenter
│   ├── NkExport.h              ★ À implémenter
│   └── Assert/
│       ├── NkAssert.h          ★ À implémenter
│       ├── NkDebugBreak.h      ★ À implémenter
│       └── NkAssertHandler.h   ★ À implémenter
└── src/
    ├── NkLimits.cpp
    └── Assert/NkAssert.cpp
```

#### 4.2.1 NkTypes.h

**Rôle:** Définir types portables (int8, uint32, float32, etc.)

```cpp
// NKCore/include/NKCore/NkTypes.h
#pragma once
#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkArchDetect.h"
#include <cstdint>
#include <cstddef>

namespace Nk {

// ============================================================================
// INTEGER TYPES
// ============================================================================

// Signed integers
using int8   = std::int8_t;
using int16  = std::int16_t;
using int32  = std::int32_t;
using int64  = std::int64_t;

// Unsigned integers
using uint8  = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

// Size types
using size_t = std::size_t;
using ptrdiff_t = std::ptrdiff_t;

// Pointer-sized integer
#if NK_ARCH_BITS == 64
    using intptr  = int64;
    using uintptr = uint64;
#else
    using intptr  = int32;
    using uintptr = uint32;
#endif

// ============================================================================
// FLOATING POINT TYPES
// ============================================================================

using float32 = float;
using float64 = double;

// Check sizes
static_assert(sizeof(float32) == 4, "float32 must be 4 bytes");
static_assert(sizeof(float64) == 8, "float64 must be 8 bytes");

// ============================================================================
// BYTE TYPE
// ============================================================================

using byte = unsigned char;

// ============================================================================
// CHAR TYPES
// ============================================================================

using char8  = char;
using char16 = char16_t;
using char32 = char32_t;

// ============================================================================
// COMMON CONSTANTS
// ============================================================================

constexpr size_t KILOBYTE = 1024;
constexpr size_t MEGABYTE = 1024 * KILOBYTE;
constexpr size_t GIGABYTE = 1024 * MEGABYTE;

// ============================================================================
// TYPE LIMITS (forward declare)
// ============================================================================

template<typename T> struct NumericLimits;

} // namespace Nk
```

**Utilisation:**

```cpp
Nk::uint32 frameCount = 0;
Nk::float32 deltaTime = 0.016f;
Nk::size_t memoryUsed = 512 * Nk::MEGABYTE;
```

---

#### 4.2.2 NkTraits.h

**Rôle:** Type traits (is\_pointer, remove\_const, etc.)

```cpp
// NKCore/include/NKCore/NkTraits.h
#pragma once
#include <type_traits>

namespace Nk {

// ============================================================================
// TYPE TRAITS (wrapping std for consistency)
// ============================================================================

// Remove cv-qualifiers
template<typename T>
using RemoveConst = std::remove_const_t<T>;

template<typename T>
using RemoveVolatile = std::remove_volatile_t<T>;

template<typename T>
using RemoveCV = std::remove_cv_t<T>;

// Remove reference
template<typename T>
using RemoveReference = std::remove_reference_t<T>;

// Remove pointer
template<typename T>
using RemovePointer = std::remove_pointer_t<T>;

// Decay
template<typename T>
using Decay = std::decay_t<T>;

// Type checks
template<typename T>
constexpr bool IsPointer = std::is_pointer_v<T>;

template<typename T>
constexpr bool IsReference = std::is_reference_v<T>;

template<typename T>
constexpr bool IsConst = std::is_const_v<T>;

template<typename T>
constexpr bool IsIntegral = std::is_integral_v<T>;

template<typename T>
constexpr bool IsFloatingPoint = std::is_floating_point_v<T>;

template<typename T>
constexpr bool IsSigned = std::is_signed_v<T>;

template<typename T>
constexpr bool IsUnsigned = std::is_unsigned_v<T>;

template<typename T, typename U>
constexpr bool IsSame = std::is_same_v<T, U>;

// Enable if
template<bool Condition, typename T = void>
using EnableIf = std::enable_if_t<Condition, T>;

// Conditional
template<bool Condition, typename TrueType, typename FalseType>
using Conditional = std::conditional_t<Condition, TrueType, FalseType>;

// ============================================================================
// CUSTOM TRAITS
// ============================================================================

// Is power of 2
template<typename T>
constexpr bool IsPowerOfTwo(T value) {
    return value > 0 && (value & (value - 1)) == 0;
}

// Alignment check
template<typename T>
constexpr bool IsAligned(T* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

} // namespace Nk
```

**Utilisation:**

```cpp
static_assert(Nk::IsIntegral<int>, "int is integral");
static_assert(Nk::IsSame<Nk::uint32, unsigned int>, "Same type");

template<typename T>
Nk::EnableIf<Nk::IsFloatingPoint<T>, T> Square(T value) {
    return value * value;
}
```

---

#### 4.2.3 NkAtomic.h

**Rôle:** Opérations atomiques (thread-safe)

```cpp
// NKCore/include/NKCore/NkAtomic.h
#pragma once
#include "NkTypes.h"
#include <atomic>

namespace Nk {

// ============================================================================
// ATOMIC TYPES
// ============================================================================

template<typename T>
using Atomic = std::atomic<T>;

using AtomicBool  = Atomic<bool>;
using AtomicInt32 = Atomic<int32>;
using AtomicInt64 = Atomic<int64>;
using AtomicUInt32 = Atomic<uint32>;
using AtomicUInt64 = Atomic<uint64>;

// ============================================================================
// MEMORY ORDER
// ============================================================================

using MemoryOrder = std::memory_order;

constexpr MemoryOrder MemoryOrderRelaxed = std::memory_order_relaxed;
constexpr MemoryOrder MemoryOrderConsume = std::memory_order_consume;
constexpr MemoryOrder MemoryOrderAcquire = std::memory_order_acquire;
constexpr MemoryOrder MemoryOrderRelease = std::memory_order_release;
constexpr MemoryOrder MemoryOrderAcqRel  = std::memory_order_acq_rel;
constexpr MemoryOrder MemoryOrderSeqCst  = std::memory_order_seq_cst;

// ============================================================================
// ATOMIC OPERATIONS
// ============================================================================

template<typename T>
inline T AtomicLoad(const Atomic<T>& atomic, MemoryOrder order = MemoryOrderSeqCst) {
    return atomic.load(order);
}

template<typename T>
inline void AtomicStore(Atomic<T>& atomic, T value, MemoryOrder order = MemoryOrderSeqCst) {
    atomic.store(value, order);
}

template<typename T>
inline T AtomicExchange(Atomic<T>& atomic, T value, MemoryOrder order = MemoryOrderSeqCst) {
    return atomic.exchange(value, order);
}

template<typename T>
inline bool AtomicCompareExchange(Atomic<T>& atomic, T& expected, T desired,
                                   MemoryOrder success = MemoryOrderSeqCst,
                                   MemoryOrder failure = MemoryOrderSeqCst) {
    return atomic.compare_exchange_strong(expected, desired, success, failure);
}

template<typename T>
inline T AtomicFetchAdd(Atomic<T>& atomic, T value, MemoryOrder order = MemoryOrderSeqCst) {
    return atomic.fetch_add(value, order);
}

template<typename T>
inline T AtomicFetchSub(Atomic<T>& atomic, T value, MemoryOrder order = MemoryOrderSeqCst) {
    return atomic.fetch_sub(value, order);
}

// Increment/Decrement (returns new value)
template<typename T>
inline T AtomicIncrement(Atomic<T>& atomic, MemoryOrder order = MemoryOrderSeqCst) {
    return atomic.fetch_add(1, order) + 1;
}

template<typename T>
inline T AtomicDecrement(Atomic<T>& atomic, MemoryOrder order = MemoryOrderSeqCst) {
    return atomic.fetch_sub(1, order) - 1;
}

// ============================================================================
// MEMORY FENCE
// ============================================================================

inline void AtomicThreadFence(MemoryOrder order = MemoryOrderSeqCst) {
    std::atomic_thread_fence(order);
}

} // namespace Nk
```

**Utilisation:**

```cpp
Nk::AtomicInt32 counter{0};

// Thread 1
Nk::AtomicIncrement(counter);

// Thread 2
int32 value = Nk::AtomicLoad(counter);
```

---

#### 4.2.4 NkBits.h

**Rôle:** Manipulation de bits

```cpp
// NKCore/include/NKCore/NkBits.h
#pragma once
#include "NkTypes.h"
#include "NKPlatform/NkCompilerDetect.h"

namespace Nk {

// ============================================================================
// BIT OPERATIONS
// ============================================================================

// Count trailing zeros
inline int32 CountTrailingZeros32(uint32 value) {
    if (value == 0) return 32;
    #if NK_COMPILER_MSVC
        unsigned long index;
        _BitScanForward(&index, value);
        return static_cast<int32>(index);
    #elif NK_COMPILER_GCC || NK_COMPILER_CLANG
        return __builtin_ctz(value);
    #else
        int32 count = 0;
        while ((value & 1) == 0) {
            value >>= 1;
            ++count;
        }
        return count;
    #endif
}

// Count leading zeros
inline int32 CountLeadingZeros32(uint32 value) {
    if (value == 0) return 32;
    #if NK_COMPILER_MSVC
        unsigned long index;
        _BitScanReverse(&index, value);
        return 31 - static_cast<int32>(index);
    #elif NK_COMPILER_GCC || NK_COMPILER_CLANG
        return __builtin_clz(value);
    #else
        int32 count = 0;
        for (int i = 31; i >= 0; --i) {
            if (value & (1u << i)) break;
            ++count;
        }
        return count;
    #endif
}

// Population count (number of 1 bits)
inline int32 PopCount32(uint32 value) {
    #if NK_COMPILER_MSVC
        return __popcnt(value);
    #elif NK_COMPILER_GCC || NK_COMPILER_CLANG
        return __builtin_popcount(value);
    #else
        int32 count = 0;
        while (value) {
            count += value & 1;
            value >>= 1;
        }
        return count;
    #endif
}

// Next power of 2
inline uint32 NextPowerOfTwo32(uint32 value) {
    if (value == 0) return 1;
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1;
}

// Is power of 2
inline bool IsPowerOfTwo32(uint32 value) {
    return value > 0 && (value & (value - 1)) == 0;
}

// Align value up
template<typename T>
inline T AlignUp(T value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// Align value down
template<typename T>
inline T AlignDown(T value, size_t alignment) {
    return value & ~(alignment - 1);
}

// Check if aligned
template<typename T>
inline bool IsAligned(T value, size_t alignment) {
    return (value & (alignment - 1)) == 0;
}

} // namespace Nk
```

**Utilisation:**

```cpp
uint32 aligned = Nk::AlignUp(123, 16); // 128
bool isPow2 = Nk::IsPowerOfTwo32(64);  // true
int bits = Nk::PopCount32(0b10110101); // 5
```

---

#### 4.2.5 NkAssert.h

**Rôle:** Système d'assertions

```cpp
// NKCore/include/NKCore/Assert/NkAssert.h
#pragma once
#include "NKCore/NkTypes.h"
#include "NkAssertHandler.h"
#include "NkDebugBreak.h"

namespace Nk {

// ============================================================================
// ASSERTION SYSTEM
// ============================================================================

enum class AssertLevel {
    Fatal,      // Always active, abort on failure
    Error,      // Active in debug, error message in release
    Warning,    // Active in debug only
    Info        // Active in verbose debug only
};

// Assertion function
inline void Assert(bool condition, const char* expr, const char* message,
                   const char* file, int line, AssertLevel level = AssertLevel::Fatal) {
    if (!condition) {
        AssertHandler::OnAssert(expr, message, file, line, level);
        if (level == AssertLevel::Fatal) {
            NK_DEBUG_BREAK();
            std::abort();
        }
    }
}

} // namespace Nk

// ============================================================================
// ASSERTION MACROS
// ============================================================================

#if defined(NK_DEBUG) || defined(_DEBUG)
    #define NK_ENABLE_ASSERTS 1
#endif

#if NK_ENABLE_ASSERTS

    #define NK_ASSERT(expr) \
        Nk::Assert(!!(expr), #expr, "", __FILE__, __LINE__, Nk::AssertLevel::Fatal)
  
    #define NK_ASSERT_MSG(expr, msg) \
        Nk::Assert(!!(expr), #expr, msg, __FILE__, __LINE__, Nk::AssertLevel::Fatal)
  
    #define NK_VERIFY(expr) NK_ASSERT(expr)
  
    #define NK_WARN(expr) \
        Nk::Assert(!!(expr), #expr, "", __FILE__, __LINE__, Nk::AssertLevel::Warning)

#else

    #define NK_ASSERT(expr) ((void)0)
    #define NK_ASSERT_MSG(expr, msg) ((void)0)
    #define NK_VERIFY(expr) (!!(expr))
    #define NK_WARN(expr) ((void)0)

#endif

// Static assert (compile-time)
#define NK_STATIC_ASSERT(expr, msg) static_assert(expr, msg)
```

**Utilisation:**

```cpp
NK_ASSERT(ptr != nullptr);
NK_ASSERT_MSG(index < size, "Index out of bounds");
NK_STATIC_ASSERT(sizeof(int) == 4, "int must be 4 bytes");
```

---

#### 4.2.6 NkDebugBreak.h

**Rôle:** Breakpoint debugger

```cpp
// NKCore/include/NKCore/Assert/NkDebugBreak.h
#pragma once
#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkCompilerDetect.h"

// ============================================================================
// DEBUG BREAK
// ============================================================================

#if NK_PLATFORM_WINDOWS
    #define NK_DEBUG_BREAK() __debugbreak()
#elif NK_COMPILER_GCC || NK_COMPILER_CLANG
    #if NK_ARCH_X86 || NK_ARCH_X64
        #define NK_DEBUG_BREAK() __asm__ volatile("int $0x03")
    #elif NK_ARCH_ARM || NK_ARCH_ARM64
        #define NK_DEBUG_BREAK() __builtin_trap()
    #else
        #define NK_DEBUG_BREAK() __builtin_trap()
    #endif
#else
    #include <csignal>
    #define NK_DEBUG_BREAK() std::raise(SIGTRAP)
#endif
```

**Utilisation:**

```cpp
if (criticalError) {
    NK_DEBUG_BREAK(); // Break into debugger
}
```

---

#### 4.2.7 NkAssertHandler.h

**Rôle:** Handler custom pour assertions

```cpp
// NKCore/include/NKCore/Assert/NkAssertHandler.h
#pragma once
#include "NKCore/NkTypes.h"
#include <functional>

namespace Nk {

enum class AssertLevel;

class AssertHandler {
public:
    using CallbackFn = std::function<void(const char* expr, const char* message,
                                          const char* file, int line,
                                          AssertLevel level)>;
  
    static void OnAssert(const char* expr, const char* message,
                        const char* file, int line, AssertLevel level);
  
    static void SetCallback(CallbackFn callback);
  
private:
    static CallbackFn s_callback;
};

} // namespace Nk
```

```cpp
// NKCore/src/Assert/NkAssert.cpp
#include "NKCore/Assert/NkAssertHandler.h"
#include "NKCore/Assert/NkAssert.h"
#include <iostream>

namespace Nk {

AssertHandler::CallbackFn AssertHandler::s_callback = nullptr;

void AssertHandler::OnAssert(const char* expr, const char* message,
                             const char* file, int line, AssertLevel level) {
    if (s_callback) {
        s_callback(expr, message, file, line, level);
    } else {
        // Default behavior: print to stderr
        const char* levelStr = "ASSERT";
        switch (level) {
            case AssertLevel::Fatal:   levelStr = "FATAL"; break;
            case AssertLevel::Error:   levelStr = "ERROR"; break;
            case AssertLevel::Warning: levelStr = "WARNING"; break;
            case AssertLevel::Info:    levelStr = "INFO"; break;
        }
      
        std::cerr << "[" << levelStr << "] Assertion failed: " << expr << "\n";
        if (message && message[0]) {
            std::cerr << "  Message: " << message << "\n";
        }
        std::cerr << "  File: " << file << ":" << line << "\n";
    }
}

void AssertHandler::SetCallback(CallbackFn callback) {
    s_callback = callback;
}

} // namespace Nk
```

**Utilisation:**

```cpp
// Custom handler
Nk::AssertHandler::SetCallback([](const char* expr, const char* msg,
                                   const char* file, int line,
                                   Nk::AssertLevel level) {
    MyLogger::Log("Assertion failed: %s at %s:%d", expr, file, line);
});
```

---

### 4.3 NKMemory - Gestion Mémoire

#### Fichiers à Implémenter

```
NKMemory/
├── include/NKMemory/
│   ├── NkAllocator.h            ★ Interface allocator
│   ├── NkMemoryArena.h          ★ Memory arena
│   ├── NkMemoryUtils.h          ★ Helpers (align, etc.)
│   ├── NkMemoryTracker.h        ★ Debug tracking
│   ├── Allocators/
│   │   ├── NkLinearAllocator.h  ★ Bump allocator
│   │   ├── NkPoolAllocator.h    ★ Fixed-size pool
│   │   ├── NkStackAllocator.h   ★ Stack-based
│   │   ├── NkFreeListAllocator.h★ Free list
│   │   ├── NkTLSFAllocator.h    ★ TLSF
│   │   ├── NkFrameAllocator.h   ★ Per-frame
│   │   └── NkMallocAllocator.h  ★ Wrapper malloc
│   └── SmartPointers/
│       ├── NkUniquePtr.h        ★ Unique ownership
│       ├── NkSharedPtr.h        ★ Shared ownership
│       ├── NkWeakPtr.h          ★ Weak reference
│       └── NkIntrusivePtr.h     ★ Intrusive refcount
└── src/
    ├── NkMemoryArena.cpp
    ├── NkMemoryTracker.cpp
    └── Allocators/
        └── [Implementations]
```

#### 4.3.1 NkAllocator.h - Interface

**Rôle:** Interface abstraite pour tous les allocateurs

```cpp
// NKMemory/include/NKMemory/NkAllocator.h
#pragma once
#include "NKCore/NkTypes.h"

namespace Nk {

// ============================================================================
// ALLOCATOR INTERFACE
// ============================================================================

class IAllocator {
public:
    virtual ~IAllocator() = default;
  
    // Allocate aligned memory
    virtual void* Allocate(size_t size, size_t alignment = 16) = 0;
  
    // Deallocate memory
    virtual void Deallocate(void* ptr) = 0;
  
    // Reallocate (optional, can be implemented via Allocate+Free)
    virtual void* Reallocate(void* ptr, size_t newSize, size_t alignment = 16) {
        if (!ptr) return Allocate(newSize, alignment);
        if (newSize == 0) {
            Deallocate(ptr);
            return nullptr;
        }
        void* newPtr = Allocate(newSize, alignment);
        if (newPtr) {
            // Note: assumes caller handles copy
            Deallocate(ptr);
        }
        return newPtr;
    }
  
    // Reset allocator (if supported)
    virtual void Reset() {}
  
    // Get stats (optional)
    virtual size_t GetAllocatedSize() const { return 0; }
    virtual size_t GetTotalCapacity() const { return 0; }
};

// ============================================================================
// ALLOCATION HELPERS
// ============================================================================

// Allocate with type
template<typename T>
inline T* Allocate(IAllocator* allocator, size_t count = 1) {
    return static_cast<T*>(allocator->Allocate(sizeof(T) * count, alignof(T)));
}

// Construct in-place
template<typename T, typename... Args>
inline T* New(IAllocator* allocator, Args&&... args) {
    T* ptr = Allocate<T>(allocator);
    if (ptr) {
        new (ptr) T(std::forward<Args>(args)...);
    }
    return ptr;
}

// Destroy and deallocate
template<typename T>
inline void Delete(IAllocator* allocator, T* ptr) {
    if (ptr) {
        ptr->~T();
        allocator->Deallocate(ptr);
    }
}

} // namespace Nk
```

**Utilisation:**

```cpp
IAllocator* allocator = GetAllocator();
int* data = Nk::Allocate<int>(allocator, 100);
allocator->Deallocate(data);

MyClass* obj = Nk::New<MyClass>(allocator, arg1, arg2);
Nk::Delete(allocator, obj);
```

---

#### 4.3.2 NkLinearAllocator.h

**Rôle:** Bump allocator ultra-rapide (pas de free individuel)

```cpp
// NKMemory/include/NKMemory/Allocators/NkLinearAllocator.h
#pragma once
#include "NKMemory/NkAllocator.h"
#include "NKCore/NkBits.h"

namespace Nk {

// ============================================================================
// LINEAR ALLOCATOR (Bump Allocator)
// ============================================================================
// - Très rapide (O(1))
// - Pas de free individuel
// - Parfait pour allocations temporaires (frame-based)
// ============================================================================

class LinearAllocator : public IAllocator {
public:
    explicit LinearAllocator(size_t capacity);
    ~LinearAllocator() override;
  
    void* Allocate(size_t size, size_t alignment = 16) override;
    void Deallocate(void* ptr) override { /* No-op */ }
    void Reset() override;
  
    size_t GetAllocatedSize() const override { return m_offset; }
    size_t GetTotalCapacity() const override { return m_capacity; }
  
private:
    byte* m_buffer;
    size_t m_capacity;
    size_t m_offset;
};

} // namespace Nk
```

```cpp
// NKMemory/src/Allocators/NkLinearAllocator.cpp
#include "NKMemory/Allocators/NkLinearAllocator.h"
#include "NKCore/Assert/NkAssert.h"
#include <cstdlib>
#include <cstring>

namespace Nk {

LinearAllocator::LinearAllocator(size_t capacity)
    : m_buffer(nullptr), m_capacity(capacity), m_offset(0) {
    m_buffer = static_cast<byte*>(std::malloc(capacity));
    NK_ASSERT_MSG(m_buffer != nullptr, "Failed to allocate linear buffer");
}

LinearAllocator::~LinearAllocator() {
    if (m_buffer) {
        std::free(m_buffer);
    }
}

void* LinearAllocator::Allocate(size_t size, size_t alignment) {
    // Align offset
    size_t alignedOffset = AlignUp(m_offset, alignment);
    size_t newOffset = alignedOffset + size;
  
    NK_ASSERT_MSG(newOffset <= m_capacity, "LinearAllocator out of memory");
  
    void* ptr = m_buffer + alignedOffset;
    m_offset = newOffset;
    return ptr;
}

void LinearAllocator::Reset() {
    m_offset = 0;
}

} // namespace Nk
```

**Utilisation:**

```cpp
LinearAllocator frameAllocator(1024 * 1024); // 1MB

// Allocate during frame
float* temp = Allocate<float>(&frameAllocator, 1000);

// Reset at end of frame
frameAllocator.Reset();
```

---

#### 4.3.3 NkPoolAllocator.h

**Rôle:** Pool d'objets de taille fixe (O(1) alloc/free)

```cpp
// NKMemory/include/NKMemory/Allocators/NkPoolAllocator.h
#pragma once
#include "NKMemory/NkAllocator.h"

namespace Nk {

// ============================================================================
// POOL ALLOCATOR
// ============================================================================
// - Objets de taille fixe
// - O(1) allocation et deallocation
// - Zero fragmentation
// - Parfait pour: Components, Nodes, Events
// ============================================================================

class PoolAllocator : public IAllocator {
public:
    PoolAllocator(size_t objectSize, size_t objectCount, size_t alignment = 16);
    ~PoolAllocator() override;
  
    void* Allocate(size_t size, size_t alignment = 16) override;
    void Deallocate(void* ptr) override;
    void Reset() override;
  
    size_t GetAllocatedSize() const override { return m_allocatedCount * m_objectSize; }
    size_t GetTotalCapacity() const override { return m_objectCount * m_objectSize; }
  
private:
    struct FreeNode {
        FreeNode* next;
    };
  
    byte* m_buffer;
    size_t m_objectSize;
    size_t m_objectCount;
    size_t m_alignment;
    FreeNode* m_freeList;
    size_t m_allocatedCount;
};

} // namespace Nk
```

```cpp
// NKMemory/src/Allocators/NkPoolAllocator.cpp
#include "NKMemory/Allocators/NkPoolAllocator.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKCore/NkBits.h"
#include <cstdlib>

namespace Nk {

PoolAllocator::PoolAllocator(size_t objectSize, size_t objectCount, size_t alignment)
    : m_buffer(nullptr), m_objectSize(0), m_objectCount(objectCount),
      m_alignment(alignment), m_freeList(nullptr), m_allocatedCount(0) {
  
    // Object size must fit at least a pointer (for free list)
    m_objectSize = std::max(objectSize, sizeof(FreeNode));
    m_objectSize = AlignUp(m_objectSize, alignment);
  
    size_t totalSize = m_objectSize * objectCount;
    m_buffer = static_cast<byte*>(std::aligned_alloc(alignment, totalSize));
    NK_ASSERT_MSG(m_buffer != nullptr, "Failed to allocate pool");
  
    // Initialize free list
    Reset();
}

PoolAllocator::~PoolAllocator() {
    if (m_buffer) {
        std::free(m_buffer);
    }
}

void* PoolAllocator::Allocate(size_t size, size_t alignment) {
    NK_ASSERT_MSG(size <= m_objectSize, "Size exceeds object size");
    NK_ASSERT_MSG(alignment <= m_alignment, "Alignment exceeds pool alignment");
    NK_ASSERT_MSG(m_freeList != nullptr, "Pool allocator out of memory");
  
    FreeNode* node = m_freeList;
    m_freeList = node->next;
    ++m_allocatedCount;
  
    return node;
}

void PoolAllocator::Deallocate(void* ptr) {
    if (!ptr) return;
  
    NK_ASSERT_MSG(ptr >= m_buffer && ptr < m_buffer + (m_objectSize * m_objectCount),
                  "Pointer not from this pool");
  
    FreeNode* node = static_cast<FreeNode*>(ptr);
    node->next = m_freeList;
    m_freeList = node;
    --m_allocatedCount;
}

void PoolAllocator::Reset() {
    m_freeList = nullptr;
    m_allocatedCount = 0;
  
    // Link all blocks
    for (size_t i = 0; i < m_objectCount; ++i) {
        FreeNode* node = reinterpret_cast<FreeNode*>(m_buffer + i * m_objectSize);
        node->next = m_freeList;
        m_freeList = node;
    }
}

} // namespace Nk
```

**Utilisation:**

```cpp
// Pool for 10000 Transform components
PoolAllocator transformPool(sizeof(Transform), 10000);

Transform* t1 = New<Transform>(&transformPool);
Transform* t2 = New<Transform>(&transformPool);

Delete(&transformPool, t1);
Delete(&transformPool, t2);
```

---

#### 4.3.4 NkFrameAllocator.h

**Rôle:** Allocateur per-frame avec reset automatique

```cpp
// NKMemory/include/NKMemory/Allocators/NkFrameAllocator.h
#pragma once
#include "NkLinearAllocator.h"
#include "NKCore/NkAtomic.h"

namespace Nk {

// ============================================================================
// FRAME ALLOCATOR
// ============================================================================
// - Basé sur LinearAllocator
// - Thread-safe (avec atomic offset)
// - Reset automatique chaque frame
// - Parfait pour: Render commands, Temp buffers, Frame data
// ============================================================================

class FrameAllocator : public IAllocator {
public:
    explicit FrameAllocator(size_t capacity);
    ~FrameAllocator() override;
  
    void* Allocate(size_t size, size_t alignment = 16) override;
    void Deallocate(void* ptr) override { /* No-op */ }
    void Reset() override;
  
    size_t GetAllocatedSize() const override {
        return AtomicLoad(m_offset, MemoryOrderRelaxed);
    }
    size_t GetTotalCapacity() const override { return m_capacity; }
  
private:
    byte* m_buffer;
    size_t m_capacity;
    AtomicUInt64 m_offset;
};

} // namespace Nk
```

```cpp
// NKMemory/src/Allocators/NkFrameAllocator.cpp
#include "NKMemory/Allocators/NkFrameAllocator.h"
#include "NKCore/Assert/NkAssert.h"
#include "NKCore/NkBits.h"
#include <cstdlib>

namespace Nk {

FrameAllocator::FrameAllocator(size_t capacity)
    : m_buffer(nullptr), m_capacity(capacity), m_offset(0) {
    m_buffer = static_cast<byte*>(std::malloc(capacity));
    NK_ASSERT_MSG(m_buffer != nullptr, "Failed to allocate frame buffer");
}

FrameAllocator::~FrameAllocator() {
    if (m_buffer) {
        std::free(m_buffer);
    }
}

void* FrameAllocator::Allocate(size_t size, size_t alignment) {
    // Atomic allocation
    uint64 oldOffset, alignedOffset, newOffset;
    do {
        oldOffset = AtomicLoad(m_offset, MemoryOrderRelaxed);
        alignedOffset = AlignUp(oldOffset, alignment);
        newOffset = alignedOffset + size;
        NK_ASSERT_MSG(newOffset <= m_capacity, "FrameAllocator out of memory");
    } while (!AtomicCompareExchange(m_offset, oldOffset, newOffset,
                                     MemoryOrderRelaxed, MemoryOrderRelaxed));
  
    return m_buffer + alignedOffset;
}

void FrameAllocator::Reset() {
    AtomicStore(m_offset, uint64(0), MemoryOrderRelease);
}

} // namespace Nk
```

**Utilisation:**

```cpp
FrameAllocator frameAlloc(4 * 1024 * 1024); // 4MB

void GameLoop() {
    while (running) {
        // Allocate frame data
        RenderCommand* cmd = New<RenderCommand>(&frameAlloc);
      
        Render();
      
        // Reset at end of frame
        frameAlloc.Reset();
    }
}
```

---

Je continue avec les **smart pointers**, la **bibliothèque de fenêtrage**, et le **système de rendu**. Le document devient très long. Veux-tu que je :

1. **Continue dans le même fichier** avec tout le reste (STL, Window, Render)?
2. **Crée des fichiers séparés** par grande section?
3. **Me concentre sur une section spécifique** que tu veux approfondir en priorité?

Je peux facilement générer 100-200 pages de documentation complète avec tous les fichiers et implémentations. Dis-moi comment tu préfères que j'organise la suite! 🚀
