# 📄 AsyncSink.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Logger asynchrone pour les performances.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\AsyncSink.h`

### 📦 Fichiers Inclus

- [`Logger/Logger.h`](./Logger.h.md)
- `queue`
- `thread`
- `mutex`
- `condition_variable`
- `atomic`
- `memory`

### 🔗 Inclus Par

- [`AsyncSink.cpp`](./AsyncSink.cpp.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (10)

### ⚙️ Functions (9)

<a name="flush"></a>

#### ⚙️ `Flush`

```cpp
void Flush() override
```

**Force le flush des messages en attente**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:56`*


---

<a name="getmaxqueuesize"></a>

#### ⚙️ `GetMaxQueueSize`

`const`

```cpp
size_t GetMaxQueueSize() const
```

**Obtient la taille maximum de la file**

**Retour:** Taille maximum

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:93`*


---

<a name="getqueuesize"></a>

#### ⚙️ `GetQueueSize`

`const`

```cpp
size_t GetQueueSize() const
```

**Obtient la taille actuelle de la file d'attente**

**Retour:** Taille de la file

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:81`*


---

<a name="isrunning"></a>

#### ⚙️ `IsRunning`

`const`

```cpp
bool IsRunning() const
```

**Vérifie si le logger est en cours d'exécution**

**Retour:** true si en cours d'exécution, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:75`*


---

<a name="log"></a>

#### ⚙️ `Log`

`virtual`

```cpp
virtual void Log(LogLevel level, const char* format, ... ) override
```

**Destructeur**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` |  |
| `format` | `const char*` |  |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:42`*


---

<a name="log"></a>

#### ⚙️ `Log`

`virtual`

```cpp
virtual void Log(LogLevel level, const char* format, ... ) override
```

**Log asynchrone**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` |  |
| `format` | `const char*` |  |
| `` | `...` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:51`*


---

<a name="setmaxqueuesize"></a>

#### ⚙️ `SetMaxQueueSize`

```cpp
void SetMaxQueueSize(size_t size)
```

**Définit la taille maximum de la file**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `size` | `size_t` | [in] Taille maximum |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:87`*


---

<a name="start"></a>

#### ⚙️ `Start`

```cpp
void Start()
```

**Démarre le thread de traitement**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:65`*


---

<a name="stop"></a>

#### ⚙️ `Stop`

```cpp
void Stop()
```

**Arrête le thread de traitement**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:70`*


---

### 📦 Variables (1)

<a name="queuesize"></a>

#### 📦 `queueSize`

```cpp
size_t queueSize
```

**Constructeur avec nom et configuration**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.h:34`*


---

