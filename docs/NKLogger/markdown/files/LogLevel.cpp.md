# 📄 LogLevel.cpp

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Implémentation des fonctions utilitaires pour LogLevel.

**Auteur:** Rihen

**Chemin:** `src\Logger\LogLevel.cpp`

### 📦 Fichiers Inclus

- [`Logger/LogLevel.h`](./LogLevel.h.md)
- `cstring`
- `cctype`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (4)

### ⚙️ Functions (4)

<a name="logleveltoansicolor"></a>

#### ⚙️ `LogLevelToANSIColor`

```cpp
char* LogLevelToANSIColor(LogLevel level)
```

**Obtient la couleur ANSI associée à un niveau de log**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogLevel.cpp:95`*


---

<a name="logleveltoshortstring"></a>

#### ⚙️ `LogLevelToShortString`

```cpp
char* LogLevelToShortString(LogLevel level)
```

**Convertit un LogLevel en chaîne courte (3 caractères)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogLevel.cpp:39`*


---

<a name="logleveltostring"></a>

#### ⚙️ `LogLevelToString`

```cpp
char* LogLevelToString(LogLevel level)
```

**Convertit un LogLevel en chaîne de caractères**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogLevel.cpp:22`*


---

<a name="logleveltowindowscolor"></a>

#### ⚙️ `LogLevelToWindowsColor`

```cpp
uint16 LogLevelToWindowsColor(LogLevel level)
```

**Obtient la couleur Windows associée à un niveau de log**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogLevel.cpp:111`*


---

