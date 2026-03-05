# 📄 NkTypeUtils.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Utilitaires de types (macros, littéraux, validation)

**Auteur:** Rihen

**Chemin:** `src\NKCore\NkTypeUtils.h`

### 📦 Fichiers Inclus

- `typeinfo`
- `stddef.h`
- `cfloat`
- `cwchar`
- [`NkTypes.h`](./NkTypes.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`core`](../namespaces/core.md)
- [`literals`](../namespaces/literals.md)

## 🎯 Éléments (20)

### ⚙️ Functions (3)

<a name="nkentseu-nkentseu-nkfromhandle"></a>

#### ⚙️ `NkFromHandle`

`noexcept`

```cpp
template<typename T> inline T* NkFromHandle(core::NkHandle handle) noexcept
```

**Convertit un handle en pointeur**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `handle` | `core::NkHandle` | [in] Handle à convertir |

**Retour:** Pointeur correspondant

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:367`*


---

<a name="nkentseu-nksafecast"></a>

#### ⚙️ `NkSafeCast`

`noexcept`

```cpp
template<typename To, typename From> inline To NkSafeCast(From value) noexcept
```

**Cast sécurisé avec vérification debug**

**Paramètres Template:**

- `typename To`
- `typename From`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `From` | [in] Valeur à convertir |

**Retour:** Valeur convertie

> 📝 **Note:** En debug, vérifie qu'il n'y a pas de perte de données

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:378`*


---

<a name="nkentseu-nkentseu-nktohandle"></a>

#### ⚙️ `NkToHandle`

`noexcept`

```cpp
template<typename T> NkHandle NkToHandle(T* ptr) noexcept
```

**Convertit un pointeur en handle**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `ptr` | `T*` | [in] Pointeur à convertir |

**Retour:** Handle correspondant

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:356`*


---

### 🔣 Macros (17)

<a name="nkaligndown"></a>

#### 🔣 `NkAlignDown`

```cpp
#define NkAlignDown ((x) & ~((a) - 1))
```

**Aligne une valeur vers le bas**

**Retour:** Valeur alignée

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:46`*


---

<a name="nkalignup"></a>

#### 🔣 `NkAlignUp`

```cpp
#define NkAlignUp (((x) + ((a) - 1)) & ~((a) - 1))
```

**Aligne une valeur vers le haut**

**Retour:** Valeur alignée

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:37`*


---

<a name="nkarraysize"></a>

#### 🔣 `NkArraySize`

```cpp
#define NkArraySize (sizeof(a) / sizeof((a)[0]))
```

**Calcule la taille d'un tableau statique**

**Retour:** Nombre d'éléments dans le tableau

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:71`*


---

<a name="nkbit"></a>

#### 🔣 `NkBit`

```cpp
#define NkBit (1ULL << (x))
```

**Crée un masque de bit à la position x**

**Retour:** Masque avec le bit x à 1

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:29`*


---

<a name="nkentseu-nkentseu-nkccast"></a>

#### 🔣 `NkCCast`

```cpp
#define NkCCast ((type)(value))
```

**Cast de type C-style (à éviter)**

**Retour:** Valeur convertie

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:342`*


---

<a name="nkclamp"></a>

#### 🔣 `NkClamp`

```cpp
#define NkClamp ((v) < (mi) ? (mi) : ((v) > (ma) ? (ma) : (v)))
```

**Clampe une valeur entre min et max**

**Retour:** Valeur clampée dans [mi, ma]

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:62`*


---

<a name="nkclearbit"></a>

#### 🔣 `NkClearBit`

```cpp
#define NkClearBit ((x) &= ~NkBit(b))
```

**Met un bit à 0**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:85`*


---

<a name="nkentseu-nkconstcast"></a>

#### 🔣 `NkConstCast`

```cpp
#define NkConstCast const_cast<type>(value)
```

**Const cast**

**Retour:** Valeur convertie

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:334`*


---

<a name="nkcontainerof"></a>

#### 🔣 `NkContainerOf`

```cpp
#define NkContainerOf ((type *)((nkentseu::core::nk_uint8 *)(ptr) - NkOffsetOf(type, member)))
```

**Calcule le pointeur de conteneur à partir d'un membre**

**Retour:** Pointeur vers le conteneur

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:108`*


---

<a name="nkisaligned"></a>

#### 🔣 `NkIsAligned`

```cpp
#define NkIsAligned (((n) & ((align) - 1)) == 0)
```

**Vérifie si une valeur est alignée**

**Retour:** true si alignée, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:124`*


---

<a name="nkoffsetof"></a>

#### 🔣 `NkOffsetOf`

```cpp
#define NkOffsetOf ((nkentseu::core::nk_usize) &((type *)NKENTSEU_NULL)->member)
```

**Calcule l'offset d'un membre dans une structure**

**Retour:** Offset en octets

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:100`*


---

<a name="nkreinterpretcast"></a>

#### 🔣 `NkReinterpretCast`

```cpp
#define NkReinterpretCast reinterpret_cast<type>(value)
```

**Reinterpret cast (dangereux)**

**Retour:** Valeur convertie

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:326`*


---

<a name="nksetbit"></a>

#### 🔣 `NkSetBit`

```cpp
#define NkSetBit ((x) |= NkBit(b))
```

**Met un bit à 1**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:78`*


---

<a name="nkstaticcast"></a>

#### 🔣 `NkStaticCast`

```cpp
#define NkStaticCast static_cast<type>(value)
```

**Cast statique sécurisé**

**Retour:** Valeur convertie

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:306`*


---

<a name="nkstrbool"></a>

#### 🔣 `NkStrBool`

```cpp
#define NkStrBool ((b) ? "True" : "False")
```

**Convertit un booléen en chaîne "True" ou "False"**

**Retour:** "True" si vrai, "False" sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:55`*


---

<a name="nktestbit"></a>

#### 🔣 `NkTestBit`

```cpp
#define NkTestBit ((x) & NkBit(b))
```

**Teste si un bit est à 1**

**Retour:** true si le bit est à 1, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:92`*


---

<a name="nktogglebit"></a>

#### 🔣 `NkToggleBit`

```cpp
#define NkToggleBit ((val) ^= NkBit(bit))
```

**Inverse un bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypeUtils.h:117`*


---

