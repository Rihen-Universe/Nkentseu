# 📄 Formatter.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Classe de formatage des messages de log avec patterns.

**Auteur:** Rihen

**Chemin:** `src\Logger\Formatter.h`

### 📦 Fichiers Inclus

- `Logger/Export.h`
- [`Logger/LogMessage.h`](./LogMessage.h.md)
- `string`
- `memory`
- `vector`

### 🔗 Inclus Par

- [`Formatter.cpp`](./Formatter.cpp.md)
- [`Logger.h`](./Logger.h.md)
- [`Sink.h`](./Sink.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (7)

### ⚙️ Functions (7)

<a name="format"></a>

#### ⚙️ `Format`

```cpp
string Format(const LogMessage& message)
```

**Formate un message de log**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` | [in] Message à formater |

**Retour:** Message formaté

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Formatter.h:102`*


---

<a name="format"></a>

#### ⚙️ `Format`

```cpp
string Format(const LogMessage& message, bool useColors)
```

**Formate un message de log avec des couleurs**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` | [in] Message à formater |
| `useColors` | `bool` | [in] true pour inclure les codes couleur |

**Retour:** Message formaté

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Formatter.h:109`*


---

<a name="formatter"></a>

#### ⚙️ `Formatter`

```cpp
explicit Formatter(const std::string& pattern)
```

**Constructeur par défaut (pattern par défaut)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const std::string&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Formatter.h:66`*


---

<a name="formatter"></a>

#### ⚙️ `Formatter`

```cpp
explicit Formatter(const std::string& pattern)
```

**Constructeur avec pattern spécifique**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const std::string&` | [in] Pattern de formatage |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Formatter.h:71`*


---

<a name="getpattern"></a>

#### ⚙️ `GetPattern`

`const`

```cpp
string& GetPattern() const
```

**Obtient le pattern courant**

**Retour:** Pattern de formatage

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Formatter.h:92`*


---

<a name="setpattern"></a>

#### ⚙️ `SetPattern`

```cpp
void SetPattern(const std::string& pattern)
```

**Destructeur**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const std::string&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Formatter.h:77`*


---

<a name="setpattern"></a>

#### ⚙️ `SetPattern`

```cpp
void SetPattern(const std::string& pattern)
```

**Définit le pattern de formatage**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const std::string&` | [in] Pattern à utiliser |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Formatter.h:86`*


---

