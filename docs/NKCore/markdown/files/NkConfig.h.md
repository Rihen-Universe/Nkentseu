# 📄 NkConfig.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Configuration globale du framework

**Auteur:** Rihen

**Chemin:** `src\NKCore\NkConfig.h`

### 📦 Fichiers Inclus

- [`NkPlatform.h`](./NkPlatform.h.md)
- `NkCompilerDetect.h`
- [`NkVersion.h`](./NkVersion.h.md)
- [`NkCGXDetect.h`](./NkCGXDetect.h.md)
- [`NkMacros.h`](./NkMacros.h.md)

### 🔗 Inclus Par

- [`NkAssert.h`](./NkAssert.h.md)

## 🎯 Éléments (13)

### 🔣 Macros (13)

<a name="nkentseu_default_page_size"></a>

#### 🔣 `NKENTSEU_DEFAULT_PAGE_SIZE`

```cpp
#define NKENTSEU_DEFAULT_PAGE_SIZE (4 * 1024)  // 4 KB
```

**Taille page mémoire par défaut**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:222`*


---

<a name="nkentseu_default_thread_count"></a>

#### 🔣 `NKENTSEU_DEFAULT_THREAD_COUNT`

```cpp
#define NKENTSEU_DEFAULT_THREAD_COUNT 0
```

**Nombre de threads par défaut (0 = auto-detect)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:197`*


---

<a name="nkentseu_enable_string_sso"></a>

#### 🔣 `NKENTSEU_ENABLE_STRING_SSO`

```cpp
#define NKENTSEU_ENABLE_STRING_SSO /**
```

**Activer Small String Optimization (SSO)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:252`*


---

<a name="nkentseu_hashmap_default_capacity"></a>

#### 🔣 `NKENTSEU_HASHMAP_DEFAULT_CAPACITY`

```cpp
#define NKENTSEU_HASHMAP_DEFAULT_CAPACITY 16
```

**Capacité initiale HashMap par défaut**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:285`*


---

<a name="nkentseu_hashmap_max_load_factor"></a>

#### 🔣 `NKENTSEU_HASHMAP_MAX_LOAD_FACTOR`

```cpp
#define NKENTSEU_HASHMAP_MAX_LOAD_FACTOR 0.75f
```

**Load factor max HashMap (0.75 = 75%)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:291`*


---

<a name="nkentseu_math_epsilon"></a>

#### 🔣 `NKENTSEU_MATH_EPSILON`

```cpp
#define NKENTSEU_MATH_EPSILON 1e-6f
```

**Epsilon pour comparaisons flottantes**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:316`*


---

<a name="nkentseu_pi"></a>

#### 🔣 `NKENTSEU_PI`

```cpp
#define NKENTSEU_PI 3.14159265358979323846f
```

**Constante PI**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:323`*


---

<a name="nkentseu_string_default_capacity"></a>

#### 🔣 `NKENTSEU_STRING_DEFAULT_CAPACITY`

```cpp
#define NKENTSEU_STRING_DEFAULT_CAPACITY 64
```

**Taille buffer string par défaut**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:246`*


---

<a name="nkentseu_string_sso_size"></a>

#### 🔣 `NKENTSEU_STRING_SSO_SIZE`

```cpp
#define NKENTSEU_STRING_SSO_SIZE 23
```

**Taille SSO buffer**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:258`*


---

<a name="nkentseu_use_posix_api"></a>

#### 🔣 `NKENTSEU_USE_POSIX_API`

```cpp
#define NKENTSEU_USE_POSIX_API #endif
```

**Utiliser POSIX API**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:392`*


---

<a name="nkentseu_use_win32_api"></a>

#### 🔣 `NKENTSEU_USE_WIN32_API`

```cpp
#define NKENTSEU_USE_WIN32_API /**
```

**Utiliser Win32 API**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:376`*


---

<a name="nkentseu_vector_default_capacity"></a>

#### 🔣 `NKENTSEU_VECTOR_DEFAULT_CAPACITY`

```cpp
#define NKENTSEU_VECTOR_DEFAULT_CAPACITY 8
```

**Capacité initiale Vector par défaut**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:273`*


---

<a name="nkentseu_vector_growth_factor"></a>

#### 🔣 `NKENTSEU_VECTOR_GROWTH_FACTOR`

```cpp
#define NKENTSEU_VECTOR_GROWTH_FACTOR 1.5f
```

**Facteur de croissance Vector (1.5 ou 2.0)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkConfig.h:279`*


---

