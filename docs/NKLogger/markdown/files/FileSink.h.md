# 📄 FileSink.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Sink pour l'écriture dans un fichier.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\FileSink.h`

### 📦 Fichiers Inclus

- [`Logger/Sink.h`](./Sink.h.md)
- `fstream`
- `mutex`

### 🔗 Inclus Par

- [`Log.cpp`](./Log.cpp.md)
- [`DailyFileSink.h`](./DailyFileSink.h.md)
- [`FileSink.cpp`](./FileSink.cpp.md)
- [`RotatingFileSink.h`](./RotatingFileSink.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (14)

### ⚙️ Functions (14)

<a name="close"></a>

#### ⚙️ `Close`

```cpp
void Close()
```

**Ferme le fichier**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:86`*


---

<a name="filesink"></a>

#### ⚙️ `FileSink`

```cpp
explicit FileSink(const std::string& filename, bool truncate)
```

**Constructeur avec chemin de fichier**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `filename` | `const std::string&` | [in] Chemin du fichier |
| `truncate` | `bool` | [in] true pour tronquer le fichier existant |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:30`*


---

<a name="flush"></a>

#### ⚙️ `Flush`

```cpp
void Flush() override
```

**Force l'écriture des données en attente**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:51`*


---

<a name="getfilesize"></a>

#### ⚙️ `GetFileSize`

`const`

```cpp
size_t GetFileSize() const
```

**Obtient la taille actuelle du fichier**

**Retour:** Taille en octets

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:109`*


---

<a name="getfilename"></a>

#### ⚙️ `GetFilename`

`const`

```cpp
string GetFilename() const
```

**Obtient le nom du fichier**

**Retour:** Nom du fichier

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:97`*


---

<a name="getformatter"></a>

#### ⚙️ `GetFormatter`

```cpp
Formatter* GetFormatter() override
```

**Obtient le formatter courant**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:66`*


---

<a name="getpattern"></a>

#### ⚙️ `GetPattern`

```cpp
string GetPattern() override
```

**Obtient le pattern courant**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:71`*


---

<a name="isopen"></a>

#### ⚙️ `IsOpen`

`const`

```cpp
bool IsOpen() const
```

**Vérifie si le fichier est ouvert**

**Retour:** true si ouvert, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:91`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(const LogMessage& message) override
```

**Destructeur**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:37`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(const LogMessage& message) override
```

**Logge un message dans le fichier**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:46`*


---

<a name="open"></a>

#### ⚙️ `Open`

```cpp
bool Open()
```

**Ouvre le fichier (si non ouvert)**

**Retour:** true si ouvert avec succès, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:80`*


---

<a name="setfilename"></a>

#### ⚙️ `SetFilename`

```cpp
void SetFilename(const std::string& filename)
```

**Définit un nouveau nom de fichier**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `filename` | `const std::string&` | [in] Nouveau nom de fichier |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:103`*


---

<a name="setformatter"></a>

#### ⚙️ `SetFormatter`

```cpp
void SetFormatter(std::unique_ptr<Formatter> formatter) override
```

**Définit le formatter pour ce sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `formatter` | `std::unique_ptr<Formatter>` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:56`*


---

<a name="setpattern"></a>

#### ⚙️ `SetPattern`

```cpp
void SetPattern(const std::string& pattern) override
```

**Définit le pattern de formatage**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const std::string&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\FileSink.h:61`*


---

