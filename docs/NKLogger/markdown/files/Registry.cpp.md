# 📄 Registry.cpp

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Implémentation du registre global des loggers.

**Auteur:** Rihen

**Chemin:** `src\Logger\Registry.cpp`

### 📦 Fichiers Inclus

- [`Logger/Registry.h`](./Registry.h.md)
- [`Logger/Sinks/ConsoleSink.h`](./ConsoleSink.h.md)
- `algorithm`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (20)

### ⚙️ Functions (16)

<a name="createlogger"></a>

#### ⚙️ `CreateLogger`

```cpp
shared_ptr<Logger> CreateLogger(const std::string& name)
```

**Crée un logger avec un nom spécifique**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const std::string&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:296`*


---

<a name="drop"></a>

#### ⚙️ `Drop`

```cpp
void Drop(const std::string& name)
```

**Supprime un logger spécifique**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const std::string&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:311`*


---

<a name="dropall"></a>

#### ⚙️ `DropAll`

```cpp
void DropAll()
```

**Supprime tous les loggers**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:304`*


---

<a name="getdefaultlogger"></a>

#### ⚙️ `GetDefaultLogger`

```cpp
shared_ptr<Logger> GetDefaultLogger()
```

**Obtient le logger par défaut**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:289`*


---

<a name="getlogger"></a>

#### ⚙️ `GetLogger`

```cpp
shared_ptr<Logger> GetLogger(const std::string& name)
```

**Obtient un logger par son nom**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const std::string&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:282`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(instance.m_Mutex )
```

**Initialise le registre avec des paramètres par défaut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `instance.m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:46`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Désenregistre un logger du registre**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:84`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient un logger par son nom**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:99`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Supprime tous les loggers du registre**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:141`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient la liste de tous les noms de loggers**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:150`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Définit le niveau de log global**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:173`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Définit le pattern global**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:196`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient le pattern global**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:211`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Force le flush de tous les loggers**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:219`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Définit le logger par défaut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:232`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient le logger par défaut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:245`*


---

### 📦 Variables (4)

<a name="instance"></a>

#### 📦 `instance`

`static`

```cpp
static Registry instance
```

**Obtient l'instance singleton du registre**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:38`*


---

<a name="instance"></a>

#### 📦 `instance`

```cpp
auto& instance
```

**Nettoie le registre (détruit tous les loggers)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:59`*


---

<a name="it"></a>

#### 📦 `it`

```cpp
auto it
```

**Obtient un logger par son nom (crée si non existant)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:113`*


---

<a name="m_globallevel"></a>

#### 📦 `m_GlobalLevel`

```cpp
return m_GlobalLevel
```

**Obtient le niveau de log global**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.cpp:188`*


---

