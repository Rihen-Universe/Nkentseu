# 📄 NullSink.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Sink nul qui ignore tous les messages (pour désactiver le logging).

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\NullSink.h`

### 📦 Fichiers Inclus

- [`Logger/Sink.h`](./Sink.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (8)

### ⚙️ Functions (8)

<a name="flush"></a>

#### ⚙️ `Flush`

```cpp
void Flush() override
```

**No-op**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\NullSink.h:47`*


---

<a name="getformatter"></a>

#### ⚙️ `GetFormatter`

```cpp
Formatter* GetFormatter() override
```

**Retourne nullptr**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\NullSink.h:62`*


---

<a name="getpattern"></a>

#### ⚙️ `GetPattern`

```cpp
string GetPattern() override
```

**Retourne une chaîne vide**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\NullSink.h:67`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(const LogMessage& message) override
```

**Constructeur par défaut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\NullSink.h:28`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(const LogMessage& message) override
```

**Destructeur**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\NullSink.h:33`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(const LogMessage& message) override
```

**Ignore le message (no-op)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\NullSink.h:42`*


---

<a name="setformatter"></a>

#### ⚙️ `SetFormatter`

```cpp
void SetFormatter(std::unique_ptr<Formatter> formatter) override
```

**No-op**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `formatter` | `std::unique_ptr<Formatter>` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\NullSink.h:52`*


---

<a name="setpattern"></a>

#### ⚙️ `SetPattern`

```cpp
void SetPattern(const NkString& pattern) override
```

**No-op**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const NkString&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\NullSink.h:57`*


---

