# 📄 Registry.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Registre global des loggers (similaire à spdlog).

**Auteur:** Rihen

**Chemin:** `src\Logger\Registry.h`

### 📦 Fichiers Inclus

- `Logger/Export.h`
- [`Logger/Logger.h`](./Logger.h.md)
- `memory`
- `string`
- `unordered_map`
- `mutex`
- `vector`

### 🔗 Inclus Par

- [`Registry.cpp`](./Registry.cpp.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (17)

### ⚙️ Functions (17)

<a name="clear"></a>

#### ⚙️ `Clear`

```cpp
void Clear()
```

**Supprime tous les loggers du registre**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:89`*


---

<a name="exists"></a>

#### ⚙️ `Exists`

`const`

```cpp
bool Exists(const NkString& name) const
```

**Vérifie si un logger existe**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const NkString&` | [in] Nom du logger |

**Retour:** true si existe, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:82`*


---

<a name="flushall"></a>

#### ⚙️ `FlushAll`

```cpp
void FlushAll()
```

**Force le flush de tous les loggers**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:134`*


---

<a name="get"></a>

#### ⚙️ `Get`

```cpp
shared_ptr<Logger> Get(const NkString& name)
```

**Obtient un logger par son nom**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const NkString&` | [in] Nom du logger |

**Retour:** Pointeur vers le logger, nullptr si non trouvé

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:68`*


---

<a name="getgloballevel"></a>

#### ⚙️ `GetGlobalLevel`

`const`

```cpp
LogLevel GetGlobalLevel() const
```

**Obtient le niveau de log global**

**Retour:** Niveau de log global

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:116`*


---

<a name="getglobalpattern"></a>

#### ⚙️ `GetGlobalPattern`

`const`

```cpp
string GetGlobalPattern() const
```

**Obtient le pattern global**

**Retour:** Pattern de formatage global

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:128`*


---

<a name="getloggercount"></a>

#### ⚙️ `GetLoggerCount`

`const`

```cpp
size_t GetLoggerCount() const
```

**Obtient le nombre de loggers enregistrés**

**Retour:** Nombre de loggers

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:100`*


---

<a name="getloggernames"></a>

#### ⚙️ `GetLoggerNames`

`const`

```cpp
vector<NkString> GetLoggerNames() const
```

**Obtient la liste de tous les noms de loggers**

**Retour:** Vecteur des noms de loggers

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:94`*


---

<a name="getorcreate"></a>

#### ⚙️ `GetOrCreate`

```cpp
shared_ptr<Logger> GetOrCreate(const NkString& name)
```

**Obtient un logger par son nom (crée si non existant)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const NkString&` | [in] Nom du logger |

**Retour:** Pointeur vers le logger (existant ou nouvellement créé)

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:75`*


---

<a name="initialize"></a>

#### ⚙️ `Initialize`

`static`

```cpp
static void Initialize()
```

**Initialise le registre avec des paramètres par défaut**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:40`*


---

<a name="instance"></a>

#### ⚙️ `Instance`

`static`

```cpp
static Registry& Instance()
```

**Obtient l'instance singleton du registre**

**Retour:** Référence à l'instance du registre

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:34`*


---

<a name="register"></a>

#### ⚙️ `Register`

```cpp
bool Register(std::shared_ptr<Logger> logger)
```

**Enregistre un logger dans le registre**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `logger` | `std::shared_ptr<Logger>` | [in] Logger à enregistrer |

**Retour:** true si enregistré, false si nom déjà existant

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:54`*


---

<a name="setdefaultlogger"></a>

#### ⚙️ `SetDefaultLogger`

```cpp
void SetDefaultLogger(std::shared_ptr<Logger> logger)
```

**Définit le logger par défaut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `logger` | `std::shared_ptr<Logger>` | [in] Logger par défaut |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:139`*


---

<a name="setgloballevel"></a>

#### ⚙️ `SetGlobalLevel`

```cpp
void SetGlobalLevel(LogLevel level)
```

**Définit le niveau de log global**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `level` | `LogLevel` | [in] Niveau de log global |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:110`*


---

<a name="setglobalpattern"></a>

#### ⚙️ `SetGlobalPattern`

```cpp
void SetGlobalPattern(const NkString& pattern)
```

**Définit le pattern global**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const NkString&` | [in] Pattern de formatage global |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:122`*


---

<a name="shutdown"></a>

#### ⚙️ `Shutdown`

`static`

```cpp
static void Shutdown()
```

**Nettoie le registre (détruit tous les loggers)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:45`*


---

<a name="unregister"></a>

#### ⚙️ `Unregister`

```cpp
bool Unregister(const NkString& name)
```

**Désenregistre un logger du registre**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `name` | `const NkString&` | [in] Nom du logger à désenregistrer |

**Retour:** true si désenregistré, false si non trouvé

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Registry.h:61`*


---

