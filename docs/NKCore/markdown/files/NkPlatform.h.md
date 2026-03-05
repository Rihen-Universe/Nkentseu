# 📄 NkPlatform.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Informations plateforme runtime et utilitaires système

**Auteur:** Rihen

**Chemin:** `src\NKCore\NkPlatform.h`

### 📦 Fichiers Inclus

- `NkPlatformDetect.h`
- `NkArchDetect.h`
- `NkCompilerDetect.h`
- [`NkTypes.h`](./NkTypes.h.md)
- [`NkExport.h`](./NkExport.h.md)
- [`NkCoreExport.h`](./NkCoreExport.h.md)
- [`NkVersion.h`](./NkVersion.h.md)
- `cstddef`

### 🔗 Inclus Par

- [`NkAssert.h`](./NkAssert.h.md)
- [`NkBits.h`](./NkBits.h.md)
- [`NkConfig.h`](./NkConfig.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`core`](../namespaces/core.md)
- [`platform`](../namespaces/platform.md)
- [`arch`](../namespaces/arch.md)
- [`memory`](../namespaces/memory.md)

## 🎯 Éléments (65)

### 🏛️ Classs (1)

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nksourcelocation"></a>

#### 🏛️ `NkSourceLocation`

```cpp
class NkSourceLocation
```

**Informations de localisation dans le code source**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:283`*


---

### 🔣 Macros (4)

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nkallocaligned"></a>

#### 🔣 `NkAllocAligned`

```cpp
#define NkAllocAligned \
```

**Alloue mémoire alignée sur cache line**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:892`*


---

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nkallocpagealigned"></a>

#### 🔣 `NkAllocPageAligned`

```cpp
#define NkAllocPageAligned \
```

**Alloue mémoire alignée sur page**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:898`*


---

<a name="nkentseu-nkcurrentsourcelocation"></a>

#### 🔣 `NkCurrentSourceLocation`

```cpp
#define NkCurrentSourceLocation \
```

**Capture la localisation actuelle**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:66`*


---

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nkfreealigned"></a>

#### 🔣 `NkFreeAligned`

```cpp
#define NkFreeAligned \
```

**Libère mémoire alignée**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:904`*


---

### 🔧 Methods (57)

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-column"></a>

#### 🔧 `Column`

`constexpr` `noexcept`

```cpp
constexpr nk_uint32 Column() noexcept
```

**Récupère le numéro de colonne**

**Retour:** Numéro de colonne

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:321`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-filename"></a>

#### 🔧 `FileName`

`noexcept`

```cpp
nk_char* FileName() noexcept
```

**Constructeur par défaut**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:292`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-filename"></a>

#### 🔧 `FileName`

`noexcept`

```cpp
nk_char* FileName() noexcept
```

**Récupère le nom du fichier source**

**Retour:** Nom du fichier

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:297`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-functionname"></a>

#### 🔧 `FunctionName`

`noexcept`

```cpp
nk_char* FunctionName() noexcept
```

**Récupère le nom de la fonction**

**Retour:** Nom de la fonction

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:305`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-line"></a>

#### 🔧 `Line`

`constexpr` `noexcept`

```cpp
constexpr nk_uint32 Line() noexcept
```

**Récupère le numéro de ligne**

**Retour:** Numéro de ligne

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:313`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkalignaddress"></a>

#### 🔧 `NkAlignAddress`

```cpp
nk_ptr NkAlignAddress(nkentseu::core::nk_ptr address, nkentseu::core::nk_size alignment)
```

**Aligne une adresse vers le haut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `address` | `nkentseu::core::nk_ptr` | [in] Adresse à aligner |
| `alignment` | `nkentseu::core::nk_size` | [in] Alignement requis (doit être puissance de 2) |

**Retour:** Adresse alignée

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:550`*


---

<a name="nkentseu-nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkalignaddressconst"></a>

#### 🔧 `NkAlignAddressConst`

```cpp
nk_ptr NkAlignAddressConst(const nkentseu::core::nk_ptr address, nkentseu::core::nk_size alignment)
```

**Aligne une adresse constante vers le haut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `address` | `const nkentseu::core::nk_ptr` | [in] Adresse à aligner |
| `alignment` | `nkentseu::core::nk_size` | [in] Alignement requis (doit être puissance de 2) |

**Retour:** Adresse alignée

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:559`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkaligndown"></a>

#### 🔧 `NkAlignDown`

`noexcept`

```cpp
template<typename T> inline T* NkAlignDown(T* addr, nk_size align) noexcept
```

**Aligne une adresse vers le bas**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `addr` | `T*` | [in] Adresse à aligner |
| `align` | `nk_size` | [in] Alignement (puissance de 2) |

**Retour:** Adresse alignée

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:774`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkalignup"></a>

#### 🔧 `NkAlignUp`

`noexcept`

```cpp
template<typename T> inline T* NkAlignUp(T* addr, nk_size align) noexcept
```

**Aligne une adresse vers le haut**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `addr` | `T*` | [in] Adresse à aligner |
| `align` | `nk_size` | [in] Alignement (puissance de 2) |

**Retour:** Adresse alignée

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:760`*


---

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkallocatealigned"></a>

#### 🔧 `NkAllocateAligned`

`noexcept`

```cpp
nk_ptr NkAllocateAligned(nk_size size, nk_size alignment) noexcept
```

**Alloue de la mémoire alignée**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `size` | `nk_size` | [in] Taille à allouer |
| `alignment` | `nk_size` | [in] Alignement requis (puissance de 2) |

**Retour:** Pointeur vers mémoire alignée, nullptr en cas d'erreur

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:822`*


---

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkallocatealignedarray"></a>

#### 🔧 `NkAllocateAlignedArray`

`noexcept`

```cpp
template<typename T> inline T* NkAllocateAlignedArray(nk_size count, nk_size alignment) noexcept
```

**Alloue un tableau d'éléments alignés**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `count` | `nk_size` | [in] Nombre d'éléments |
| `alignment` | `nk_size` | [in] Alignement requis |

**Retour:** Pointeur vers le tableau

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:844`*


---

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkcalculatepadding"></a>

#### 🔧 `NkCalculatePadding`

`noexcept`

```cpp
template<typename T> inline nk_size NkCalculatePadding(const T* addr, nk_size align) noexcept
```

**Calcule le padding nécessaire pour l'alignement**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `addr` | `const T*` | [in] Adresse à vérifier |
| `align` | `nk_size` | [in] Alignement (puissance de 2) |

**Retour:** Nombre de bytes de padding

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:800`*


---

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkconstructaligned"></a>

#### 🔧 `NkConstructAligned`

`noexcept`

```cpp
template<typename T, typename... Args> inline T* NkConstructAligned(core::nk_ptr ptr, Args&&... args) noexcept
```

**Construit un objet dans de la mémoire alignée**

**Paramètres Template:**

- `typename T`
- `typename... Args`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `ptr` | `core::nk_ptr` | [in] Mémoire alignée |
| `args` | `Args&&...` | [in] Arguments pour le constructeur |

**Retour:** Pointeur vers l'objet construit

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:858`*


---

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nkdestroyaligned"></a>

#### 🔧 `NkDestroyAligned`

`noexcept`

```cpp
template<typename T> inline void NkDestroyAligned(T* ptr) noexcept
```

**Détruit un objet dans de la mémoire alignée**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `ptr` | `T*` | [in] Pointeur vers l'objet |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:871`*


---

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkfreealigned"></a>

#### 🔧 `NkFreeAligned`

`noexcept`

```cpp
void NkFreeAligned(core::nk_ptr ptr) noexcept
```

**Libère de la mémoire allouée avec AllocateAligned**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `ptr` | `core::nk_ptr` | [in] Pointeur à libérer |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:830`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetallocationgranularity"></a>

#### 🔧 `NkGetAllocationGranularity`

```cpp
nk_uint32 NkGetAllocationGranularity()
```

**Récupère la granularité d'allocation mémoire**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:481`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetarchname"></a>

#### 🔧 `NkGetArchName`

`noexcept`

```cpp
nk_char* NkGetArchName() noexcept
```

**Retourne le nom de l'architecture**

**Retour:** Nom de l'architecture

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:672`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetarchversion"></a>

#### 🔧 `NkGetArchVersion`

`noexcept`

```cpp
nk_char* NkGetArchVersion() noexcept
```

**Retourne la version de l'architecture**

**Retour:** Version de l'architecture

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:680`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetarchitecturename"></a>

#### 🔧 `NkGetArchitectureName`

```cpp
nk_char* NkGetArchitectureName()
```

**Récupère le nom de l'architecture**

**Retour:** Nom de l'architecture (ex: "x86_64", "ARM64")

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:395`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetavailablememory"></a>

#### 🔧 `NkGetAvailableMemory`

```cpp
nk_uint64 NkGetAvailableMemory()
```

**Récupère la RAM disponible**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:467`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetbuildtype"></a>

#### 🔧 `NkGetBuildType`

```cpp
nk_char* NkGetBuildType()
```

**Récupère le type de build**

**Retour:** "Debug", "Release", "RelWithDebInfo", etc.

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:506`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetcpucorecount"></a>

#### 🔧 `NkGetCPUCoreCount`

```cpp
nk_uint32 NkGetCPUCoreCount()
```

**Récupère le nombre de cœurs CPU physiques**

**Retour:** Nombre de cœurs

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:414`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetcputhreadcount"></a>

#### 🔧 `NkGetCPUThreadCount`

```cpp
nk_uint32 NkGetCPUThreadCount()
```

**Récupère le nombre total de threads matériels**

**Retour:** Nombre de threads (incluant hyperthreading)

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:421`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetcachelinesize"></a>

#### 🔧 `NkGetCacheLineSize`

```cpp
nk_uint32 NkGetCacheLineSize()
```

**Récupère la taille de ligne de cache**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:449`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetcachelinesize"></a>

#### 🔧 `NkGetCacheLineSize`

`noexcept`

```cpp
inline nk_uint32 NkGetCacheLineSize() noexcept
```

**Retourne la taille de ligne de cache**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:736`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetendianness"></a>

#### 🔧 `NkGetEndianness`

```cpp
NkEndianness NkGetEndianness()
```

**Détecte l'endianness du système**

**Retour:** Type d'endianness

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:517`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetl1cachesize"></a>

#### 🔧 `NkGetL1CacheSize`

```cpp
nk_uint32 NkGetL1CacheSize()
```

**Récupère la taille du cache L1**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:428`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetl2cachesize"></a>

#### 🔧 `NkGetL2CacheSize`

```cpp
nk_uint32 NkGetL2CacheSize()
```

**Récupère la taille du cache L2**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:435`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetl3cachesize"></a>

#### 🔧 `NkGetL3CacheSize`

```cpp
nk_uint32 NkGetL3CacheSize()
```

**Récupère la taille du cache L3**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:442`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetpagesize"></a>

#### 🔧 `NkGetPageSize`

```cpp
nk_uint32 NkGetPageSize()
```

**Récupère la taille de page mémoire**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:474`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetpagesize"></a>

#### 🔧 `NkGetPageSize`

`noexcept`

```cpp
inline nk_uint32 NkGetPageSize() noexcept
```

**Retourne la taille de page mémoire**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:744`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetplatforminfo"></a>

#### 🔧 `NkGetPlatformInfo`

```cpp
NkPlatformInfo& NkGetPlatformInfo()
```

**Fonctions pour récupérer les informations plateforme**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:368`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetplatforminfo"></a>

#### 🔧 `NkGetPlatformInfo`

```cpp
NkPlatformInfo& NkGetPlatformInfo()
```

**Récupère les informations complètes de la plateforme**

**Retour:** Référence constante vers PlatformInfo

> 📝 **Note:** Thread-safe, initialisation automatique au premier appel

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:373`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetplatformname"></a>

#### 🔧 `NkGetPlatformName`

```cpp
nk_char* NkGetPlatformName()
```

**Récupère le nom de la plateforme**

**Retour:** Nom de la plateforme (ex: "Windows", "Linux", "macOS")

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:388`*


---

<a name="nkentseu-nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetplatformname"></a>

#### 🔧 `NkGetPlatformName`

`noexcept`

```cpp
nk_char* NkGetPlatformName() noexcept
```

**Fonctions inline pour informations plateforme compile-time**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:578`*


---

<a name="nkentseu-nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetplatformname"></a>

#### 🔧 `NkGetPlatformName`

`noexcept`

```cpp
nk_char* NkGetPlatformName() noexcept
```

**Retourne le nom de la plateforme (compile-time)**

**Retour:** Nom de la plateforme

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:583`*


---

<a name="nkentseu-nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetplatformversion"></a>

#### 🔧 `NkGetPlatformVersion`

`noexcept`

```cpp
nk_char* NkGetPlatformVersion() noexcept
```

**Retourne la version/description de la plateforme**

**Retour:** Description de la plateforme

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:592`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgettotalmemory"></a>

#### 🔧 `NkGetTotalMemory`

```cpp
nk_uint64 NkGetTotalMemory()
```

**Récupère la RAM totale du système**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:460`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkgetwordsize"></a>

#### 🔧 `NkGetWordSize`

`noexcept`

```cpp
inline nk_uint32 NkGetWordSize() noexcept
```

**Retourne la taille de mot machine**

**Retour:** Taille en bytes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:752`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkhassimdfeature"></a>

#### 🔧 `NkHasSIMDFeature`

```cpp
nk_bool NkHasSIMDFeature(const nkentseu::core::nk_char* feature)
```

**Vérifie le support d'une fonctionnalité SIMD**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `feature` | `const nkentseu::core::nk_char*` | [in] Nom de la fonctionnalité (ex: "SSE", "AVX", "NEON") |

**Retour:** true si supportée, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:402`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkinitializeplatforminfo"></a>

#### 🔧 `NkInitializePlatformInfo`

```cpp
void NkInitializePlatformInfo()
```

**Initialise explicitement les informations plateforme**

> 📝 **Note:** Appelé automatiquement par NkGetPlatformInfo()

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:381`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkis32bit"></a>

#### 🔧 `NkIs32Bit`

`noexcept`

```cpp
inline nk_bool NkIs32Bit() noexcept
```

**Vérifie si l'architecture est 32-bit**

**Retour:** true si 32-bit, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:700`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkis64bit"></a>

#### 🔧 `NkIs64Bit`

```cpp
nk_bool NkIs64Bit()
```

**Vérifie si l'architecture est 64-bit**

**Retour:** true si 64-bit, false si 32-bit

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:524`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkis64bit"></a>

#### 🔧 `NkIs64Bit`

`noexcept`

```cpp
inline nk_bool NkIs64Bit() noexcept
```

**Vérifie si l'architecture est 64-bit**

**Retour:** true si 64-bit, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:688`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkisaligned"></a>

#### 🔧 `NkIsAligned`

```cpp
nk_bool NkIsAligned(const nkentseu::core::nk_ptr address, nkentseu::core::nk_size alignment)
```

**Vérifie si une adresse est alignée**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `address` | `const nkentseu::core::nk_ptr` | [in] Adresse à vérifier |
| `alignment` | `nkentseu::core::nk_size` | [in] Alignement requis (doit être puissance de 2) |

**Retour:** true si alignée, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:541`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkisaligned"></a>

#### 🔧 `NkIsAligned`

`noexcept`

```cpp
template<typename T> inline nk_bool NkIsAligned(const T* addr, nk_size align) noexcept = 0
```

**Vérifie si une adresse est alignée**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `addr` | `const T*` | [in] Adresse à vérifier |
| `align` | `nk_size` | [in] Alignement (puissance de 2) |

**Retour:** true si alignée, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:788`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkisbigendian"></a>

#### 🔧 `NkIsBigEndian`

`noexcept`

```cpp
inline nk_bool NkIsBigEndian() noexcept
```

**Vérifie si l'architecture est big-endian**

**Retour:** true si big-endian, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:724`*


---

<a name="nkentseu-nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkisconsole"></a>

#### 🔧 `NkIsConsole`

`noexcept`

```cpp
inline nk_bool NkIsConsole() noexcept
```

**Vérifie si la plateforme est une console**

**Retour:** true si console, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:627`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkisdebugbuild"></a>

#### 🔧 `NkIsDebugBuild`

```cpp
nk_bool NkIsDebugBuild()
```

**Vérifie si c'est un build debug**

**Retour:** true si debug, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:492`*


---

<a name="nkentseu-nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkisdesktop"></a>

#### 🔧 `NkIsDesktop`

`noexcept`

```cpp
inline nk_bool NkIsDesktop() noexcept
```

**Vérifie si la plateforme est desktop**

**Retour:** true si desktop, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:601`*


---

<a name="nkentseu-nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkisembedded"></a>

#### 🔧 `NkIsEmbedded`

`noexcept`

```cpp
inline nk_bool NkIsEmbedded() noexcept
```

**Vérifie si la plateforme est embarquée**

**Retour:** true si embarqué, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:640`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkislittleendian"></a>

#### 🔧 `NkIsLittleEndian`

`noexcept`

```cpp
inline nk_bool NkIsLittleEndian() noexcept
```

**Vérifie si l'architecture est little-endian**

**Retour:** true si little-endian, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:712`*


---

<a name="nkentseu-nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkismobile"></a>

#### 🔧 `NkIsMobile`

`noexcept`

```cpp
inline nk_bool NkIsMobile() noexcept
```

**Vérifie si la plateforme est mobile**

**Retour:** true si mobile, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:614`*


---

<a name="nkentseu-nkentseu-arch-memory-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkispointeraligned"></a>

#### 🔧 `NkIsPointerAligned`

`noexcept`

```cpp
template<typename T> nk_bool NkIsPointerAligned(const core::nk_ptr ptr, nk_size alignment) noexcept
```

**Vérifie si un pointeur est aligné**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `ptr` | `const core::nk_ptr` | [in] Pointeur à vérifier |
| `alignment` | `nk_size` | [in] Alignement à tester |

**Retour:** true si aligné, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:836`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkissharedlibrary"></a>

#### 🔧 `NkIsSharedLibrary`

```cpp
nk_bool NkIsSharedLibrary()
```

**Vérifie si c'est une bibliothèque partagée**

**Retour:** true si partagée, false si statique

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:499`*


---

<a name="nkentseu-nkentseu-arch-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkisweb"></a>

#### 🔧 `NkIsWeb`

`noexcept`

```cpp
inline nk_bool NkIsWeb() noexcept
```

**Vérifie si la plateforme est web**

**Retour:** true si web, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:653`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-nkprintplatforminfo"></a>

#### 🔧 `NkPrintPlatformInfo`

```cpp
void NkPrintPlatformInfo()
```

**Affiche les informations plateforme sur stdout**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:535`*


---

### 🏗️ Structs (2)

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nkplatforminfo"></a>

#### 🏗️ `NkPlatformInfo`

```cpp
struct NkPlatformInfo
```

**Informations complètes sur la plateforme runtime**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:198`*


---

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkversioninfo"></a>

#### 🏗️ `NkVersionInfo`

```cpp
struct NkVersionInfo
```

**Informations de version (Major.Minor.Patch)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:183`*


---

### 📦 Variables (1)

<a name="nkentseu-nkplatformtype-nkplatformtype-nkarchitecturetype-nkdisplaytype-nkgraphicsapi-nkendianness-nkversioninfo-nkplatforminfo-nksourcelocation-file"></a>

#### 📦 `file`

`const`

```cpp
const nk_char* file
```

**Crée une instance avec la localisation actuelle**

**Retour:** Instance de SourceLocation

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkPlatform.h:329`*


---

