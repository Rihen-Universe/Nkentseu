# 📄 RotatingFileSink.cpp

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Implémentation du sink avec rotation basée sur la taille.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\RotatingFileSink.cpp`

### 📦 Fichiers Inclus

- [`Logger/Sinks/RotatingFileSink.h`](./RotatingFileSink.h.md)
- `filesystem`
- `sstream`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (6)

### ⚙️ Functions (1)

<a name="lock"></a>

#### ⚙️ `lock`

```cpp
lock_guard<std::mutex> lock(m_Mutex )
```

**Obtient le nombre maximum de fichiers**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `` | `m_Mutex` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\RotatingFileSink.cpp:63`*


---

### 📦 Variables (5)

<a name="i"></a>

#### 📦 `i`

```cpp
size_t i
```

**Effectue la rotation des fichiers**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\RotatingFileSink.cpp:88`*


---

<a name="m_maxfiles"></a>

#### 📦 `m_MaxFiles`

```cpp
return m_MaxFiles
```

**Définit le nombre maximum de fichiers**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\RotatingFileSink.cpp:55`*


---

<a name="m_maxsize"></a>

#### 📦 `m_MaxSize`

```cpp
return m_MaxSize
```

**Définit la taille maximum des fichiers**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\RotatingFileSink.cpp:39`*


---

<a name="m_maxsize"></a>

#### 📦 `m_MaxSize`

```cpp
return m_MaxSize
```

**Obtient la taille maximum des fichiers**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\RotatingFileSink.cpp:47`*


---

<a name="true"></a>

#### 📦 `true`

```cpp
return true
```

**Force la rotation du fichier**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\RotatingFileSink.cpp:71`*


---

