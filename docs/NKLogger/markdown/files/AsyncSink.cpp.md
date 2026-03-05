# 📄 AsyncSink.cpp

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Implémentation du logger asynchrone avec file d'attente.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\AsyncSink.cpp`

### 📦 Fichiers Inclus

- [`Logger/Sinks/AsyncSink.h`](./AsyncSink.h.md)
- `thread`
- `cstdarg`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (10)

### ⚙️ Functions (5)

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_QueueMutex )
```

**Vérifie si le logger est en cours d'exécution**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_QueueMutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:100`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_QueueMutex )
```

**Obtient l'intervalle de flush**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_QueueMutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:139`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
unique_lock<std::mutex> lock(m_QueueMutex )
```

**Ajoute un message à la file**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_QueueMutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:170`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Traite un message de la file**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:187`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
unique_lock<std::mutex> lock(m_QueueMutex )
```

**Vide toute la file d'attente**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_QueueMutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:208`*


---

### 📦 Variables (5)

<a name="m_flushinterval"></a>

#### 📦 `m_FlushInterval`

```cpp
return m_FlushInterval
```

**Définit l'intervalle de flush**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:131`*


---

<a name="m_maxqueuesize"></a>

#### 📦 `m_MaxQueueSize`

```cpp
return m_MaxQueueSize
```

**Définit la taille maximum de la file**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:115`*


---

<a name="m_maxqueuesize"></a>

#### 📦 `m_MaxQueueSize`

```cpp
return m_MaxQueueSize
```

**Obtient la taille maximum de la file**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:123`*


---

<a name="msg"></a>

#### 📦 `msg`

```cpp
LogMessage msg
```

**Log asynchrone avec message pré-formaté**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:47`*


---

<a name="msg"></a>

#### 📦 `msg`

```cpp
LogMessage msg
```

**Fonction du thread de traitement**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\AsyncSink.cpp:147`*


---

