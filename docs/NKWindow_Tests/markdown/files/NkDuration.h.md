# 📄 NkDuration.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Chemin:** `src\NKWindow\Time\NkDuration.h`

### 📦 Fichiers Inclus

- `../Core/NkTypes.h`
- `cmath`
- `string`

### 🔗 Inclus Par

- [`NkClock.h`](./NkClock.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)

## 🎯 Éléments (14)

### 🏛️ Classs (1)

<a name="nkduration-nkduration"></a>

#### 🏛️ `NkDuration`

```cpp
class NkDuration
```

**Immutable duration value stored in nanoseconds. * NkDuration is the base time unit used by NkClock and NkStopwatch. It supports integer and floating-point factories, conversions, and arithmetic operations.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:11`*


---

### 🔧 Methods (13)

<a name="nkduration-frommicroseconds"></a>

#### 🔧 `FromMicroseconds`

`constexpr`

```cpp
constexpr NkDuration FromMicroseconds(NkI64 us)
```

**Build from microseconds.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `us` | `NkI64` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:32`*


---

<a name="nkduration-frommicroseconds"></a>

#### 🔧 `FromMicroseconds`

`static`

```cpp
static NkDuration FromMicroseconds(double us)
```

**Build from floating microseconds (rounded).**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `us` | `double` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:39`*


---

<a name="nkduration-frommilliseconds"></a>

#### 🔧 `FromMilliseconds`

`constexpr`

```cpp
constexpr NkDuration FromMilliseconds(NkI64 ms)
```

**Build from milliseconds.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `ms` | `NkI64` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:34`*


---

<a name="nkduration-frommilliseconds"></a>

#### 🔧 `FromMilliseconds`

`static`

```cpp
static NkDuration FromMilliseconds(double ms)
```

**Build from floating milliseconds (rounded).**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `ms` | `double` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:45`*


---

<a name="nkduration-fromnanoseconds"></a>

#### 🔧 `FromNanoseconds`

`constexpr`

```cpp
constexpr NkDuration FromNanoseconds(NkI64 ns)
```

**Build from nanoseconds.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `ns` | `NkI64` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:30`*


---

<a name="nkduration-fromseconds"></a>

#### 🔧 `FromSeconds`

`constexpr`

```cpp
constexpr NkDuration FromSeconds(NkI64 s)
```

**Build from seconds.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `s` | `NkI64` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:36`*


---

<a name="nkduration-fromseconds"></a>

#### 🔧 `FromSeconds`

`static`

```cpp
static NkDuration FromSeconds(double s)
```

**Build from floating seconds (rounded).**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `s` | `double` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:51`*


---

<a name="nkduration-tomilliseconds"></a>

#### 🔧 `ToMilliseconds`

`const` `constexpr`

```cpp
constexpr NkI64 ToMilliseconds() const
```

**Convert to milliseconds (integer truncation).**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:61`*


---

<a name="nkduration-tonanoseconds"></a>

#### 🔧 `ToNanoseconds`

`const` `constexpr`

```cpp
constexpr NkI64 ToNanoseconds() const
```

**Convert to nanoseconds.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:57`*


---

<a name="nkduration-tostring"></a>

#### 🔧 `ToString`

`const`

```cpp
string ToString() const
```

**Human-readable representation (ns/us/ms/s).**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:70`*


---

<a name="nkduration-zero"></a>

#### 🔧 `Zero`

`constexpr`

```cpp
constexpr NkDuration Zero()
```

**Create a zero duration.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:21`*


---

<a name="nkduration-zero"></a>

#### 🔧 `Zero`

`constexpr`

```cpp
constexpr NkDuration Zero()
```

**Create a duration from raw nanoseconds.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:24`*


---

<a name="nkduration-zero"></a>

#### 🔧 `Zero`

`constexpr`

```cpp
constexpr NkDuration Zero()
```

**Return a zero duration constant.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Time\NkDuration.h:28`*


---

