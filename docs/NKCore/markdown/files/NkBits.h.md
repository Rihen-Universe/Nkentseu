# 📄 NkBits.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Manipulation de bits avancée

**Auteur:** Rihen

**Chemin:** `src\NKCore\NkBits.h`

### 📦 Fichiers Inclus

- `NkCompilerDetect.h`
- [`NkTypes.h`](./NkTypes.h.md)
- [`NkInline.h`](./NkInline.h.md)
- [`NkExport.h`](./NkExport.h.md)
- [`NkPlatform.h`](./NkPlatform.h.md)
- [`Assert/NkAssert.h`](./NkAssert.h.md)
- `cstdint`
- `cstdlib`
- `stdlib.h`
- `intrin.h`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`core`](../namespaces/core.md)

## 🎯 Éléments (22)

### 🏛️ Classs (1)

<a name="nkbits-nkbits"></a>

#### 🏛️ `NkBits`

```cpp
template<typename T> class NkBits
```

**Classe utilitaire pour manipulation avancée de bits**

**Paramètres Template:**

- `typename T`

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:263`*


---

### ⚙️ Functions (2)

<a name="nk_byteswap16"></a>

#### ⚙️ `NK_BYTESWAP16`

```cpp
nk_uint16 NK_BYTESWAP16(nkentseu::core::nk_uint16 x)
```

**Inverse l'ordre des octets (16-bit)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `x` | `nkentseu::core::nk_uint16` | [in] Valeur 16-bit |

**Retour:** Valeur avec octets inversés

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:221`*


---

<a name="nk_ctz64"></a>

#### ⚙️ `NK_CTZ64`

```cpp
nk_uint32 NK_CTZ64(nkentseu::core::nk_uint64 x) = 0
```

**Macro pour compter les zéros à droite (64-bit)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `x` | `nkentseu::core::nk_uint64` | [in] Entier 64-bit |

**Retour:** Nombre de zéros à droite (0-63)

> 📝 **Note:** Retourne 64 si x = 0

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:101`*


---

### 🔧 Methods (16)

<a name="nkbits-byteswap16"></a>

#### 🔧 `ByteSwap16`

```cpp
nk_uint16 ByteSwap16(nkentseu::core::nk_uint16 value)
```

**Inverse l'ordre des octets (16-bit)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `nkentseu::core::nk_uint16` | [in] Valeur 16-bit |

**Retour:** Valeur avec octets inversés

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:383`*


---

<a name="nkbits-byteswap32"></a>

#### 🔧 `ByteSwap32`

```cpp
nk_uint32 ByteSwap32(nkentseu::core::nk_uint32 value)
```

**Inverse l'ordre des octets (32-bit)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `nkentseu::core::nk_uint32` | [in] Valeur 32-bit |

**Retour:** Valeur avec octets inversés

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:392`*


---

<a name="nkbits-byteswap64"></a>

#### 🔧 `ByteSwap64`

```cpp
template<typename T> nk_uint64 ByteSwap64(nkentseu::core::nk_uint64 value)
```

**Inverse l'ordre des octets (64-bit)**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `nkentseu::core::nk_uint64` | [in] Valeur 64-bit |

**Retour:** Valeur avec octets inversés

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:401`*


---

<a name="nkbits-countbits"></a>

#### 🔧 `CountBits`

`static`

```cpp
template<typename T> static nk_int32 CountBits(T value)
```

**Compte le nombre de bits à 1 (population count)**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur à analyser |

**Retour:** Nombre de bits à 1

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:274`*


---

<a name="nkbits-countleadingzeros"></a>

#### 🔧 `CountLeadingZeros`

`static`

```cpp
template<typename T> static nk_int32 CountLeadingZeros(T value) = 0
```

**Compte les zéros à gauche (leading zeros)**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur à analyser |

**Retour:** Nombre de zéros à gauche

> 📝 **Note:** Retourne sizeof(T)*8 si value = 0

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:306`*


---

<a name="nkbits-counttrailingzeros"></a>

#### 🔧 `CountTrailingZeros`

`static`

```cpp
template<typename T> static nk_int32 CountTrailingZeros(T value) = 0
```

**Compte les zéros à droite (trailing zeros)**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur à analyser |

**Retour:** Nombre de zéros à droite

> 📝 **Note:** Retourne sizeof(T)*8 si value = 0

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:289`*


---

<a name="nkbits-extractbits"></a>

#### 🔧 `ExtractBits`

`static`

```cpp
template<typename T> static T ExtractBits(T value, nk_int32 position, nk_int32 count) = 0
```

**Extrait un champ de bits**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur source |
| `position` | `nk_int32` | [in] Position du premier bit (0-based) |
| `count` | `nk_int32` | [in] Nombre de bits à extraire |

**Retour:** Champ de bits extrait

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:456`*


---

<a name="nkbits-findfirstset"></a>

#### 🔧 `FindFirstSet`

`static`

```cpp
template<typename T> static nk_int32 FindFirstSet(T value) = 0
```

**Trouve l'index du bit le plus bas à 1**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur à analyser |

**Retour:** Index du premier bit à 1 (0-based), -1 si aucun bit à 1

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:323`*


---

<a name="nkbits-findlastset"></a>

#### 🔧 `FindLastSet`

`static`

```cpp
template<typename T> static nk_int32 FindLastSet(T value) = 0
```

**Trouve l'index du bit le plus haut à 1**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur à analyser |

**Retour:** Index du dernier bit à 1 (0-based), -1 si aucun bit à 1

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:335`*


---

<a name="nkbits-insertbits"></a>

#### 🔧 `InsertBits`

`static`

```cpp
template<typename T> static T InsertBits(T dest, T src, nk_int32 position, nk_int32 count) = 0
```

**Insère un champ de bits**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `dest` | `T` | [in] Destination où insérer |
| `src` | `T` | [in] Source des bits à insérer |
| `position` | `nk_int32` | [in] Position du premier bit (0-based) |
| `count` | `nk_int32` | [in] Nombre de bits à insérer |

**Retour:** Destination avec bits insérés

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:472`*


---

<a name="nkbits-log2"></a>

#### 🔧 `Log2`

`static`

```cpp
template<typename T> static nk_int32 Log2(T value)
```

**Log2 d'une puissance de 2**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur (doit être puissance de 2) |

**Retour:** Log2 de la valeur

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:439`*


---

<a name="nkbits-nextpoweroftwo"></a>

#### 🔧 `NextPowerOfTwo`

```cpp
nk_uint32 NextPowerOfTwo(nkentseu::core::nk_uint32 value)
```

**Arrondit vers le haut à la prochaine puissance de 2**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `nkentseu::core::nk_uint32` | [in] Valeur 32-bit |

**Retour:** Prochaine puissance de 2 supérieure ou égale

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:425`*


---

<a name="nkbits-nextpoweroftwo"></a>

#### 🔧 `NextPowerOfTwo`

```cpp
template<typename T> nk_uint64 NextPowerOfTwo(nkentseu::core::nk_uint64 value)
```

**Arrondit vers le haut à la prochaine puissance de 2**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `nkentseu::core::nk_uint64` | [in] Valeur 64-bit |

**Retour:** Prochaine puissance de 2 supérieure ou égale

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:432`*


---

<a name="nkbits-reversebits"></a>

#### 🔧 `ReverseBits`

`static`

```cpp
template<typename T> static T ReverseBits(T value)
```

**Inverse tous les bits d'une valeur**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur à inverser |

**Retour:** Valeur avec bits inversés

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:490`*


---

<a name="nkbits-rotateleft"></a>

#### 🔧 `RotateLeft`

`static`

```cpp
template<typename T> static T RotateLeft(T value, nk_int32 shift)
```

**Rotation gauche (rotate left)**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur à faire tourner |
| `shift` | `nk_int32` | [in] Nombre de positions de rotation (modulo bits) |

**Retour:** Valeur après rotation

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:351`*


---

<a name="nkbits-rotateright"></a>

#### 🔧 `RotateRight`

`static`

```cpp
template<typename T> static T RotateRight(T value, nk_int32 shift)
```

**Rotation droite (rotate right)**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `value` | `T` | [in] Valeur à faire tourner |
| `shift` | `nk_int32` | [in] Nombre de positions de rotation (modulo bits) |

**Retour:** Valeur après rotation

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:365`*


---

### 📦 Variables (3)

<a name="index"></a>

#### 📦 `index`

```cpp
long index
```

**Macro pour compter les zéros à droite (32-bit)**

**Retour:** Nombre de zéros à droite (0-31)

> 📝 **Note:** Retourne 32 si x = 0

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:73`*


---

<a name="index"></a>

#### 📦 `index`

```cpp
long index
```

**Macro pour compter les zéros à gauche (32-bit)**

**Retour:** Nombre de zéros à gauche (0-31)

> 📝 **Note:** Retourne 32 si x = 0

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:145`*


---

<a name="index"></a>

#### 📦 `index`

```cpp
long index
```

**Macro pour compter les zéros à gauche (64-bit)**

**Retour:** Nombre de zéros à gauche (0-63)

> 📝 **Note:** Retourne 64 si x = 0

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkBits.h:174`*


---

