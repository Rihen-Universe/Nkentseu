# 📄 DistributingSink.cpp

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Implémentation du sink distribuant les messages à plusieurs sous-sinks.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\DistributingSink.cpp`

### 📦 Fichiers Inclus

- [`Logger/Sinks/DistributingSink.h`](./DistributingSink.h.md)
- `algorithm`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (12)

### ⚙️ Functions (12)

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Destructeur**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:27`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Distribue un message à tous les sous-sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:33`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Force le flush de tous les sous-sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:49`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Définit le formatter pour tous les sous-sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:61`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Définit le pattern de formatage pour tous les sous-sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:75`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient le formatter (du premier sink)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:87`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient le pattern (du premier sink)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:98`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Ajoute un sous-sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:109`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Supprime un sous-sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:119`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Supprime tous les sous-sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:132`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient la liste des sous-sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:140`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Vérifie si un sink spécifique est présent**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.cpp:156`*


---

