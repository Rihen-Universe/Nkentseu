# 📄 ConsoleSink.cpp

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Implémentation du sink console avec support couleur.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\ConsoleSink.cpp`

### 📦 Fichiers Inclus

- [`Logger/Sinks/ConsoleSink.h`](./ConsoleSink.h.md)
- [`Logger/LogLevel.h`](./LogLevel.h.md)
- `Windows.h`
- `unistd.h`
- `cstring`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (14)

### ⚙️ Functions (7)

<a name="logleveltoansicolor"></a>

#### ⚙️ `LogLevelToANSIColor`

```cpp
return LogLevelToANSIColor(level )
```

**Obtient le code couleur pour un niveau de log**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `level` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:244`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Force l'écriture des données en attente**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:81`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Définit le formatter pour ce sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:98`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Définit le pattern de formatage**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:106`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient le formatter courant**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:116`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient le pattern courant**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:124`*


---

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Vérifie si le sink utilise stderr pour les erreurs**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:175`*


---

### 📦 Variables (7)

<a name="formatted"></a>

#### 📦 `formatted`

```cpp
string formatted
```

**Logge un message dans la console**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:56`*


---

<a name="hconsole"></a>

#### 📦 `hConsole`

```cpp
HANDLE hConsole
```

**Obtient le code de réinitialisation de couleur**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:251`*


---

<a name="m_stream"></a>

#### 📦 `m_Stream`

```cpp
return m_Stream
```

**Définit le flux de console**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:151`*


---

<a name="m_stream"></a>

#### 📦 `m_Stream`

```cpp
return m_Stream
```

**Obtient le flux de console courant**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:159`*


---

<a name="m_usecolors"></a>

#### 📦 `m_UseColors`

```cpp
return m_UseColors
```

**Active ou désactive les couleurs**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:135`*


---

<a name="m_usecolors"></a>

#### 📦 `m_UseColors`

```cpp
return m_UseColors
```

**Vérifie si les couleurs sont activées**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:143`*


---

<a name="m_usestderrforerrors"></a>

#### 📦 `m_UseStderrForErrors`

```cpp
return m_UseStderrForErrors
```

**Définit si le sink doit utiliser stderr pour les niveaux d'erreur**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\ConsoleSink.cpp:167`*


---

