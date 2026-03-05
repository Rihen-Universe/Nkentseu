# 📄 NkInline.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Macros d'inlining et optimisation fonctions

**Auteur:** Rihen

**Chemin:** `src\NKCore\NkInline.h`

### 📦 Fichiers Inclus

- `NkCompilerDetect.h`

### 🔗 Inclus Par

- [`NkAtomic.h`](./NkAtomic.h.md)
- [`NkBits.h`](./NkBits.h.md)

## 🎯 Éléments (8)

### ⚙️ Functions (5)

<a name="nkabs"></a>

#### ⚙️ `NkAbs`

```cpp
int32 NkAbs(int32 x)
```

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `x` | `int32` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkInline.h:401`*


---

<a name="nkadd"></a>

#### ⚙️ `NkAdd`

```cpp
int32 NkAdd(int32 a, int32 b)
```

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `a` | `int32` |  |
| `b` | `int32` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkInline.h:372`*


---

<a name="nkhandleerror"></a>

#### ⚙️ `NkHandleError`

```cpp
void NkHandleError(const char* message)
```

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const char*` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkInline.h:386`*


---

<a name="nkprocessframe"></a>

#### ⚙️ `NkProcessFrame`

```cpp
void NkProcessFrame(float32 deltaTime)
```

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `deltaTime` | `float32` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkInline.h:379`*


---

<a name="nksquareroot"></a>

#### ⚙️ `NkSquareRoot`

```cpp
float32 NkSquareRoot(float32 x)
```

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `x` | `float32` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkInline.h:393`*


---

### 🔣 Macros (3)

<a name="nkentseu_force_inline"></a>

#### 🔣 `NKENTSEU_FORCE_INLINE`

```cpp
#define NKENTSEU_FORCE_INLINE __forceinline
```

**Force l'inlining**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkInline.h:349`*


---

<a name="nkentseu_inline"></a>

#### 🔣 `NKENTSEU_INLINE`

```cpp
#define NKENTSEU_INLINE inline
```

**Inline standard C++**

> 📝 **Note:** Suggestion au compilateur, peut être ignorée

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkInline.h:25`*


---

<a name="nkentseu_noinline"></a>

#### 🔣 `NKENTSEU_NOINLINE`

```cpp
#define NKENTSEU_NOINLINE __declspec(noinline)
```

**Empêche l'inlining**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkInline.h:343`*


---

