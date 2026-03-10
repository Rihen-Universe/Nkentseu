# 📄 Log.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Logger par défaut avec API fluide pour un usage simplifié.
//              Fournit une instance singleton et des méthodes chaînables.

**Auteur:** Rihen

**Chemin:** `src\Logger\Log.h`

### 📦 Fichiers Inclus

- [`Logger/Logger.h`](./Logger.h.md)
- `memory`

### 🔗 Inclus Par

- [`Log.cpp`](./Log.cpp.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (5)

### ⚙️ Functions (3)

<a name="initialize"></a>

#### ⚙️ `Initialize`

`static`

```cpp
static void Initialize(const NkString& name, const NkString& pattern, LogLevel level)
```

**Initialise le logger par défaut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const NkString&` | [in] Nom du logger (optionnel, "default" par défaut) |
| `pattern` | `const NkString&` | [in] Pattern de formatage (optionnel) |
| `level` | `LogLevel` | [in] Niveau de log (optionnel, Info par défaut) |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Log.h:36`*


---

<a name="named"></a>

#### ⚙️ `Named`

```cpp
NkentseuLogger& Named(const NkString& name)
```

**Configure les informations de source pour le prochain log**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const NkString&` |  |

**Retour:** Objet SourceContext pour chaînage

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Log.h:55`*


---

<a name="shutdown"></a>

#### ⚙️ `Shutdown`

`static`

```cpp
static void Shutdown()
```

**Nettoie et détruit le logger par défaut**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Log.h:46`*


---

### 📦 Variables (2)

<a name="name"></a>

#### 📦 `name`

```cpp
string& name
```

**Obtient l'instance singleton du logger par défaut**

**Retour:** Référence à l'instance du logger par défaut

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Log.h:30`*


---

<a name="sourcefile"></a>

#### 📦 `sourceFile`

`const`

```cpp
const char* sourceFile
```

**Configure le nom du logger (retourne *this pour chaînage)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Log.h:64`*


---

