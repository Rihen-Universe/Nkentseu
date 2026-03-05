# 📄 DailyFileSink.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Sink pour fichiers avec rotation quotidienne.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\DailyFileSink.h`

### 📦 Fichiers Inclus

- [`Logger/Sinks/FileSink.h`](./FileSink.h.md)
- `chrono`

### 🔗 Inclus Par

- [`DailyFileSink.cpp`](./DailyFileSink.cpp.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (7)

### ⚙️ Functions (7)

<a name="getrotationhour"></a>

#### ⚙️ `GetRotationHour`

`const`

```cpp
int GetRotationHour() const
```

**Obtient l'heure de rotation**

**Retour:** Heure de rotation

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DailyFileSink.h:63`*


---

<a name="getrotationminute"></a>

#### ⚙️ `GetRotationMinute`

`const`

```cpp
int GetRotationMinute() const
```

**Obtient la minute de rotation**

**Retour:** Minute de rotation

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DailyFileSink.h:69`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(const LogMessage& message) override = 0
```

**Constructeur avec configuration**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DailyFileSink.h:29`*


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

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DailyFileSink.h:38`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(const LogMessage& message) override
```

**Logge un message avec vérification de rotation quotidienne**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DailyFileSink.h:47`*


---

<a name="setmaxdays"></a>

#### ⚙️ `SetMaxDays`

```cpp
void SetMaxDays(size_t maxDays)
```

**Définit le nombre maximum de jours à conserver**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `maxDays` | `size_t` | [in] Nombre de jours (0 = illimité) |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DailyFileSink.h:75`*


---

<a name="setrotationtime"></a>

#### ⚙️ `SetRotationTime`

```cpp
void SetRotationTime(int hour, int minute)
```

**Définit l'heure de rotation**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `hour` | `int` | [in] Heure (0-23) |
| `minute` | `int` | [in] Minute (0-59) |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DailyFileSink.h:56`*


---

