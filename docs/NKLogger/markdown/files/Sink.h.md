# 📄 Sink.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Interface de base pour tous les sinks de logging.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sink.h`

### 📦 Fichiers Inclus

- `Logger/Export.h`
- [`Logger/LogMessage.h`](./LogMessage.h.md)
- [`Logger/Formatter.h`](./Formatter.h.md)
- `memory`
- `string`

### 🔗 Inclus Par

- [`Logger.h`](./Logger.h.md)
- [`ConsoleSink.h`](./ConsoleSink.h.md)
- [`DistributingSink.h`](./DistributingSink.h.md)
- [`FileSink.h`](./FileSink.h.md)
- [`NullSink.h`](./NullSink.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (10)

### ⚙️ Functions (10)

<a name="flush"></a>

#### ⚙️ `Flush`

`virtual`

```cpp
virtual void Flush() = 0
```

**Force l'écriture des données en attente**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:43`*


---

<a name="getformatter"></a>

#### ⚙️ `GetFormatter`

`const` `virtual`

```cpp
virtual Formatter* GetFormatter() const = 0
```

**Obtient le formatter courant**

**Retour:** Pointeur vers le formatter

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:60`*


---

<a name="getlevel"></a>

#### ⚙️ `GetLevel`

`const` `virtual`

```cpp
virtual LogLevel GetLevel() const
```

**Obtient le niveau minimum de log**

**Retour:** Niveau minimum

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:82`*


---

<a name="getpattern"></a>

#### ⚙️ `GetPattern`

`const`

```cpp
string GetPattern() const = 0
```

**Obtient le pattern courant**

**Retour:** Pattern de formatage

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:66`*


---

<a name="isenabled"></a>

#### ⚙️ `IsEnabled`

`const` `virtual`

```cpp
virtual bool IsEnabled() const
```

**Vérifie si le sink est activé**

**Retour:** true si activé, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:101`*


---

<a name="setenabled"></a>

#### ⚙️ `SetEnabled`

`virtual`

```cpp
virtual void SetEnabled(bool enabled)
```

**Active ou désactive le sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `enabled` | `bool` | [in] État d'activation |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:95`*


---

<a name="setformatter"></a>

#### ⚙️ `SetFormatter`

`virtual`

```cpp
virtual void SetFormatter(std::unique_ptr<Formatter> formatter) = 0
```

**Définit le formatter pour ce sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `formatter` | `std::unique_ptr<Formatter>` | [in] Formatter à utiliser |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:48`*


---

<a name="setlevel"></a>

#### ⚙️ `SetLevel`

`virtual`

```cpp
virtual void SetLevel(LogLevel level)
```

**Définit le niveau minimum de log pour ce sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau minimum |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:76`*


---

<a name="setpattern"></a>

#### ⚙️ `SetPattern`

`virtual`

```cpp
virtual void SetPattern(const std::string& pattern) = 0
```

**Définit le pattern de formatage**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const std::string&` | [in] Pattern à utiliser |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:54`*


---

<a name="shouldlog"></a>

#### ⚙️ `ShouldLog`

`const` `virtual`

```cpp
virtual bool ShouldLog(LogLevel level) const
```

**Vérifie si un niveau devrait être loggé**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau à vérifier |

**Retour:** true si le niveau est >= niveau minimum, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sink.h:88`*


---

