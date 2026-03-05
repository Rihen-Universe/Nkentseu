# 📄 NkTypes.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Typedefs et alias dans le namespace nkentseu

**Auteur:** Rihen

**Chemin:** `src\NKCore\NkTypes.h`

### 📦 Fichiers Inclus

- `NkCompilerDetect.h`
- `stdint.h`

### 🔗 Inclus Par

- [`NkAssertHandler.h`](./NkAssertHandler.h.md)
- [`NkAssertion.h`](./NkAssertion.h.md)
- [`NkAtomic.h`](./NkAtomic.h.md)
- [`NkBits.h`](./NkBits.h.md)
- [`NkCoreExport.h`](./NkCoreExport.h.md)
- [`NkLimits.h`](./NkLimits.h.md)
- [`NkMacros.h`](./NkMacros.h.md)
- [`NkPlatform.h`](./NkPlatform.h.md)
- [`NkTypeUtils.h`](./NkTypeUtils.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`core`](../namespaces/core.md)
- [`math`](../namespaces/math.md)

## 🎯 Éléments (33)

### 🔢 Enums (1)

<a name="nkentseu-byte-value"></a>

#### 🔢 `Value`

```cpp
enum Value : uint8
```

**Valeurs constantes prédéfinies**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:93`*


---

### ⚙️ Functions (2)

<a name="nkentseu-safeconstcast"></a>

#### ⚙️ `SafeConstCast`

`noexcept`

```cpp
template<typename T> inline T* SafeConstCast(ConstVoidPtr ptr) noexcept
```

**Conversion sécurisée de ConstVoidPtr vers T***

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `ptr` | `ConstVoidPtr` | [in] Pointeur source |

**Retour:** Pointeur converti

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:252`*


---

<a name="nkentseu-from"></a>

#### ⚙️ `from`

`constexpr` `noexcept`

```cpp
template<typename T> constexpr Byte from(T v) noexcept
```

**Conversion sécurisée depuis un type**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `v` | `T` | [in] Valeur à convertir |

**Retour:** Byte converti

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:124`*


---

### 🔧 Methods (2)

<a name="nkentseu-byte-byte"></a>

#### 🔧 `Byte`

```cpp
template<typename T> return Byte(value | b.value)
```

**Constructeur explicite**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `b.value` | `value |` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:102`*


---

<a name="nkentseu-byte-byte"></a>

#### 🔧 `Byte`

```cpp
template<typename T> return Byte(value | b.value)
```

**Constructeur par défaut (valeur 0)**

**Paramètres Template:**

- `typename T`

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `b.value` | `value |` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:107`*


---

### 🏗️ Structs (2)

<a name="nkentseu-byte-byte"></a>

#### 🏗️ `Byte`

```cpp
struct Byte
```

**Structure wrapper pour byte avec opérateurs * Encapsule un uint8 avec des opérations bitwise et conversions sécurisées. ***

**Exemples:**

```cpp
* Byte b = Byte::from(0xFF);
         * Byte masked = b & Byte::from(0x0F);  // 0x0F
         * uint8 value = static_cast<uint8>(b); // 0xFF
         *
```

> 📝 **Note:** Utiliser Byte::from() pour conversion sécurisée

> 📝 **Note:** Tous les opérateurs sont constexpr et noexcept *

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:77`*


---

<a name="nkentseu-int128"></a>

#### 🏗️ `int128`

```cpp
template<typename T> struct int128
```

**Conversion vers un type**

**Paramètres Template:**

- `typename T`

**Retour:** Valeur convertie

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:133`*


---

### 📝 Typedefs (26)

<a name="nkentseu-boolean"></a>

#### 📝 `Boolean`

```cpp
using Boolean = u
```

**Booléen 8-bit (0 = false, 1 = true)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:208`*


---

<a name="nkentseu-char"></a>

#### 📝 `Char`

```cpp
using Char = c
```

**Caractère par défaut du framework**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:179`*


---

<a name="nkentseu-nkentseu-nkentseu-nkdegrees"></a>

#### 📝 `NkDegrees`

```cpp
using NkDegrees = N
```

**Angle en degrés (float par défaut)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:623`*


---

<a name="nkentseu-nkentseu-nkduration"></a>

#### 📝 `NkDuration`

```cpp
using NkDuration = u
```

**Durée en millisecondes (32-bit pour économiser mémoire)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:584`*


---

<a name="nkentseu-nkentseu-nkentseu-nkduration64"></a>

#### 📝 `NkDuration64`

```cpp
using NkDuration64 = u
```

**Durée en millisecondes 64-bit (pour longues durées)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:590`*


---

<a name="nkentseu-nkhandle"></a>

#### 📝 `NkHandle`

```cpp
using NkHandle = u
```

**Handle opaque (pointeur ou index)**

> 📝 **Note:** Utilise taille pointeur pour compatibilité

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:533`*


---

<a name="nkentseu-nkhash32"></a>

#### 📝 `NkHash32`

```cpp
using NkHash32 = u
```

**Hash 32-bit explicite**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:517`*


---

<a name="nkentseu-nkhash64"></a>

#### 📝 `NkHash64`

```cpp
using NkHash64 = u
```

**Hash 64-bit explicite**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:523`*


---

<a name="nkentseu-nkhashvalue"></a>

#### 📝 `NkHashValue`

```cpp
using NkHashValue = u
```

**Type pour hash values (32-bit ou 64-bit selon arch)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:507`*


---

<a name="nkentseu-nkid32"></a>

#### 📝 `NkID32`

```cpp
using NkID32 = u
```

**Handle invalide**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:540`*


---

<a name="nkentseu-nkid32"></a>

#### 📝 `NkID32`

```cpp
using NkID32 = u
```

**Identifiant unique 32-bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:550`*


---

<a name="nkentseu-nkid64"></a>

#### 📝 `NkID64`

```cpp
using NkID64 = u
```

**Identifiant unique 64-bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:556`*


---

<a name="nkentseu-nkentseu-nkentseu-nkradians"></a>

#### 📝 `NkRadians`

```cpp
using NkRadians = N
```

**Angle en radians (float par défaut)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:617`*


---

<a name="nkentseu-nkentseu-nkentseu-nkreal"></a>

#### 📝 `NkReal`

```cpp
using NkReal = f
```

**Type flottant par défaut pour calculs mathématiques**

> 📝 **Note:** Configurable: float32 ou float64

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:606`*


---

<a name="nkentseu-nktimestamp"></a>

#### 📝 `NkTimestamp`

```cpp
using NkTimestamp = i
```

**ID invalide 64-bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:568`*


---

<a name="nkentseu-nktimestamp"></a>

#### 📝 `NkTimestamp`

```cpp
using NkTimestamp = i
```

**Timestamp en microsecondes (64-bit)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:578`*


---

<a name="nkentseu-ptr"></a>

#### 📝 `PTR`

```cpp
using PTR = v
```

**Valeur true pour Boolean**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:220`*


---

<a name="nkentseu-ptr"></a>

#### 📝 `PTR`

```cpp
using PTR = v
```

**Valeur false pour Boolean**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:226`*


---

<a name="nkentseu-bool32"></a>

#### 📝 `bool32`

```cpp
using bool32 = i
```

**Booléen 32-bit (pour alignement)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:214`*


---

<a name="nkentseu-float32"></a>

#### 📝 `float32`

```cpp
using float32 = f
```

**Flottant simple précision 32-bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:157`*


---

<a name="nkentseu-float64"></a>

#### 📝 `float64`

```cpp
using float64 = d
```

**Flottant double précision 64-bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:163`*


---

<a name="nkentseu-float80"></a>

#### 📝 `float80`

```cpp
using float80 = l
```

**Flottant étendu précision 80-bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:169`*


---

<a name="nkentseu-int8"></a>

#### 📝 `int8`

```cpp
using int8 = s
```

**Entier signé 8-bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:42`*


---

<a name="nkentseu-uint8"></a>

#### 📝 `uint8`

```cpp
using uint8 = u
```

**Entier non-signé 8-bit**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:48`*


---

<a name="nkentseu-uintl32"></a>

#### 📝 `uintl32`

```cpp
using uintl32 = u
```

**Entier non-signé 32-bit (long)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:54`*


---

<a name="nkentseu-usize_cpu"></a>

#### 📝 `usize_cpu`

```cpp
using usize_cpu = N
```

**Conversion sécurisée de VoidPtr vers T***

**Retour:** Pointeur converti

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkTypes.h:263`*


---

