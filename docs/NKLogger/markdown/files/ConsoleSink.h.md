# 📄 ConsoleSink.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Sink pour la sortie console (stdout/stderr) avec support couleur.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\ConsoleSink.h`

### 📦 Fichiers Inclus

- [`Logger/Sink.h`](./Sink.h.md)
- `iostream`
- `mutex`

### 🔗 Inclus Par

- [`Log.cpp`](./Log.cpp.md)
- [`Registry.cpp`](./Registry.cpp.md)
- [`ConsoleSink.cpp`](./ConsoleSink.cpp.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (13)

### ⚙️ Functions (13)

<a name="consolesink"></a>

#### ⚙️ `ConsoleSink`

```cpp
explicit ConsoleSink(ConsoleStream stream, bool useColors)
```

**Constructeur par défaut (stdout, avec couleurs)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `stream` | `ConsoleStream` |  |
| `useColors` | `bool` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:39`*


---

<a name="consolesink"></a>

#### ⚙️ `ConsoleSink`

```cpp
explicit ConsoleSink(ConsoleStream stream, bool useColors)
```

**Constructeur avec flux spécifique**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `stream` | `ConsoleStream` | [in] Flux à utiliser |
| `useColors` | `bool` | [in] Utiliser les couleurs (si supporté) |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:44`*


---

<a name="flush"></a>

#### ⚙️ `Flush`

```cpp
void Flush() override
```

**Force l'écriture des données en attente**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:65`*


---

<a name="getformatter"></a>

#### ⚙️ `GetFormatter`

```cpp
Formatter* GetFormatter() override
```

**Obtient le formatter courant**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:80`*


---

<a name="getpattern"></a>

#### ⚙️ `GetPattern`

```cpp
string GetPattern() override
```

**Obtient le pattern courant**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:85`*


---

<a name="getstream"></a>

#### ⚙️ `GetStream`

`const`

```cpp
ConsoleStream GetStream() const
```

**Obtient le flux de console courant**

**Retour:** Flux courant

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:112`*


---

<a name="iscolorenabled"></a>

#### ⚙️ `IsColorEnabled`

`const`

```cpp
bool IsColorEnabled() const
```

**Vérifie si les couleurs sont activées**

**Retour:** true si activées, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:100`*


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

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:51`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(const LogMessage& message) override
```

**Logge un message dans la console**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:60`*


---

<a name="setcolorenabled"></a>

#### ⚙️ `SetColorEnabled`

```cpp
void SetColorEnabled(bool enable)
```

**Active ou désactive les couleurs**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `enable` | `bool` | [in] true pour activer les couleurs |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:94`*


---

<a name="setformatter"></a>

#### ⚙️ `SetFormatter`

```cpp
void SetFormatter(std::unique_ptr<Formatter> formatter) override
```

**Définit le formatter pour ce sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `formatter` | `std::unique_ptr<Formatter>` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:70`*


---

<a name="setpattern"></a>

#### ⚙️ `SetPattern`

```cpp
void SetPattern(const std::string& pattern) override
```

**Définit le pattern de formatage**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const std::string&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:75`*


---

<a name="setstream"></a>

#### ⚙️ `SetStream`

```cpp
void SetStream(ConsoleStream stream)
```

**Définit le flux de console**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `stream` | `ConsoleStream` | [in] Flux à utiliser |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.h:106`*


---

