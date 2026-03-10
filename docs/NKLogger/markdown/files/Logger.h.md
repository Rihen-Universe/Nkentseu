# 📄 Logger.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Classe principale du système de logging, similaire à spdlog.
//              Gère les sinks, formatters et fournit une API de logging.

**Auteur:** Rihen

**Chemin:** `src\Logger\Logger.h`

### 📦 Fichiers Inclus

- `Logger/Export.h`
- [`Logger/LogLevel.h`](./LogLevel.h.md)
- [`Logger/Sink.h`](./Sink.h.md)
- [`Logger/Formatter.h`](./Formatter.h.md)
- `memory`
- `vector`
- `string`
- `mutex`

### 🔗 Inclus Par

- [`Log.h`](./Log.h.md)
- [`Logger.cpp`](./Logger.cpp.md)
- [`Registry.h`](./Registry.h.md)
- [`AsyncSink.h`](./AsyncSink.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (32)

### ⚙️ Functions (31)

<a name="addsink"></a>

#### ⚙️ `AddSink`

```cpp
void AddSink(std::shared_ptr<ISink> sink)
```

**Destructeur du logger**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `sink` | `std::shared_ptr<ISink>` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:42`*


---

<a name="addsink"></a>

#### ⚙️ `AddSink`

```cpp
void AddSink(std::shared_ptr<ISink> sink)
```

**Ajoute un sink au logger**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `sink` | `std::shared_ptr<ISink>` | [in] Sink à ajouter (partagé) |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:51`*


---

<a name="clearsinks"></a>

#### ⚙️ `ClearSinks`

```cpp
void ClearSinks()
```

**Supprime tous les sinks du logger**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:57`*


---

<a name="critical"></a>

#### ⚙️ `Critical`

```cpp
void Critical(const char* format, ... )
```

**Log critical avec format string**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `format` | `const char*` | [in] Format string |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:186`*


---

<a name="critical"></a>

#### ⚙️ `Critical`

```cpp
void Critical(const NkString& message)
```

**Log critical avec stream style**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const NkString&` | [in] Message à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:241`*


---

<a name="debug"></a>

#### ⚙️ `Debug`

```cpp
void Debug(const char* format, ... )
```

**Log debug avec format string**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `format` | `const char*` | [in] Format string |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:158`*


---

<a name="debug"></a>

#### ⚙️ `Debug`

```cpp
void Debug(const NkString& message)
```

**Log debug avec stream style**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const NkString&` | [in] Message à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:217`*


---

<a name="error"></a>

#### ⚙️ `Error`

```cpp
void Error(const char* format, ... )
```

**Log error avec format string**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `format` | `const char*` | [in] Format string |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:179`*


---

<a name="error"></a>

#### ⚙️ `Error`

```cpp
void Error(const NkString& message)
```

**Log error avec stream style**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const NkString&` | [in] Message à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:235`*


---

<a name="fatal"></a>

#### ⚙️ `Fatal`

```cpp
void Fatal(const char* format, ... )
```

**Log fatal avec format string**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `format` | `const char*` | [in] Format string |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:193`*


---

<a name="fatal"></a>

#### ⚙️ `Fatal`

```cpp
void Fatal(const NkString& message)
```

**Log fatal avec stream style**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const NkString&` | [in] Message à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:247`*


---

<a name="flush"></a>

#### ⚙️ `Flush`

`virtual`

```cpp
virtual void Flush()
```

**Force le flush de tous les sinks**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:257`*


---

<a name="getlevel"></a>

#### ⚙️ `GetLevel`

`const`

```cpp
LogLevel GetLevel() const
```

**Obtient le niveau de log courant**

**Retour:** Niveau de log

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:94`*


---

<a name="getname"></a>

#### ⚙️ `GetName`

`const`

```cpp
string& GetName() const
```

**Obtient le nom du logger**

**Retour:** Nom du logger

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:262`*


---

<a name="getsinkcount"></a>

#### ⚙️ `GetSinkCount`

`const`

```cpp
size_t GetSinkCount() const
```

**Obtient le nombre de sinks attachés**

**Retour:** Nombre de sinks

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:62`*


---

<a name="info"></a>

#### ⚙️ `Info`

```cpp
void Info(const char* format, ... )
```

**Log info avec format string**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `format` | `const char*` | [in] Format string |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:165`*


---

<a name="info"></a>

#### ⚙️ `Info`

```cpp
void Info(const NkString& message)
```

**Log info avec stream style**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const NkString&` | [in] Message à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:223`*


---

<a name="log"></a>

#### ⚙️ `Log`

`virtual`

```cpp
virtual void Log(LogLevel level, const char* format, ... )
```

**Log avec format string (style printf)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau de log |
| `format` | `const char*` | [in] Format string |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:111`*


---

<a name="log"></a>

#### ⚙️ `Log`

`virtual`

```cpp
virtual void Log(LogLevel level, const char* file, int line, const char* func, const char* format, ... )
```

**Log avec format string et informations de source**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau de log |
| `file` | `const char*` | [in] Fichier source |
| `line` | `int` | [in] Ligne source |
| `func` | `const char*` | [in] Fonction source |
| `format` | `const char*` | [in] Format string |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:119`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(LogLevel level, const char* file, int line, const char* func, const NkString& message)
```

**Log avec message string et informations de source**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau de log |
| `file` | `const char*` | [in] Fichier source |
| `line` | `int` | [in] Ligne source |
| `func` | `const char*` | [in] Fonction source |
| `message` | `const NkString&` | [in] Message à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:130`*


---

<a name="log"></a>

#### ⚙️ `Log`

`virtual`

```cpp
virtual void Log(LogLevel level, const char* file, int line, const char* func, const char* format, va_list args)
```

**Log avec format string, informations de source et va_list**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau de log |
| `file` | `const char*` | [in] Fichier source |
| `line` | `int` | [in] Ligne source |
| `func` | `const char*` | [in] Fonction source |
| `format` | `const char*` | [in] Format string |
| `args` | `va_list` | [in] Arguments variables (va_list) |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:140`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(LogLevel level, const NkString& message)
```

**Log avec stream style**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau de log |
| `message` | `const NkString&` | [in] Message à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:204`*


---

<a name="logger"></a>

#### ⚙️ `Logger`

```cpp
explicit Logger(const NkString& name)
```

**Constructeur de logger avec nom**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const NkString&` | [in] Nom du logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:36`*


---

<a name="setformatter"></a>

#### ⚙️ `SetFormatter`

```cpp
void SetFormatter(std::unique_ptr<Formatter> formatter)
```

**Définit le formatter pour tous les sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `formatter` | `std::unique_ptr<Formatter>` | [in] Formatter à utiliser |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:72`*


---

<a name="setlevel"></a>

#### ⚙️ `SetLevel`

```cpp
void SetLevel(LogLevel level)
```

**Définit le niveau de log minimum**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau minimum à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:88`*


---

<a name="setpattern"></a>

#### ⚙️ `SetPattern`

```cpp
void SetPattern(const NkString& pattern)
```

**Définit le pattern de formatage**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const NkString&` | [in] Pattern à utiliser |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:78`*


---

<a name="shouldlog"></a>

#### ⚙️ `ShouldLog`

`const`

```cpp
bool ShouldLog(LogLevel level) const
```

**Vérifie si un niveau devrait être loggé**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau à vérifier |

**Retour:** true si le niveau est >= niveau minimum, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:100`*


---

<a name="trace"></a>

#### ⚙️ `Trace`

```cpp
void Trace(const char* format, ... )
```

**Log trace avec format string**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `format` | `const char*` | [in] Format string |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:151`*


---

<a name="trace"></a>

#### ⚙️ `Trace`

```cpp
void Trace(const NkString& message)
```

**Log trace avec stream style**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const NkString&` | [in] Message à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:211`*


---

<a name="warn"></a>

#### ⚙️ `Warn`

```cpp
void Warn(const char* format, ... )
```

**Log warning avec format string**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `format` | `const char*` | [in] Format string |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:172`*


---

<a name="warn"></a>

#### ⚙️ `Warn`

```cpp
void Warn(const NkString& message)
```

**Log warning avec stream style**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const NkString&` | [in] Message à logger |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:229`*


---

### 📦 Variables (1)

<a name="sourcefile"></a>

#### 📦 `sourceFile`

`const`

```cpp
const char* sourceFile
```

**Vérifie si le logger est actif**

**Retour:** true si actif, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Logger.h:268`*


---

