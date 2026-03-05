# 📄 DistributingSink.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Sink qui distribue les messages à plusieurs sous-sinks.

**Auteur:** Rihen

**Chemin:** `src\Logger\Sinks\DistributingSink.h`

### 📦 Fichiers Inclus

- [`Logger/Sink.h`](./Sink.h.md)
- `vector`
- `memory`
- `mutex`

### 🔗 Inclus Par

- [`DistributingSink.cpp`](./DistributingSink.cpp.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (13)

### ⚙️ Functions (13)

<a name="addsink"></a>

#### ⚙️ `AddSink`

```cpp
void AddSink(std::shared_ptr<ISink> sink)
```

**Ajoute un sous-sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `sink` | `std::shared_ptr<ISink>` | [in] Sink à ajouter |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:85`*


---

<a name="clearsinks"></a>

#### ⚙️ `ClearSinks`

```cpp
void ClearSinks()
```

**Supprime tous les sous-sinks**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:97`*


---

<a name="distributingsink"></a>

#### ⚙️ `DistributingSink`

```cpp
explicit DistributingSink(const std::vector<std::shared_ptr<ISink>>& sinks)
```

**Constructeur par défaut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `sinks` | `const std::vector<std::shared_ptr<ISink>>&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:31`*


---

<a name="distributingsink"></a>

#### ⚙️ `DistributingSink`

```cpp
explicit DistributingSink(const std::vector<std::shared_ptr<ISink>>& sinks)
```

**Constructeur avec liste initiale de sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `sinks` | `const std::vector<std::shared_ptr<ISink>>&` | [in] Liste de sinks à ajouter |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:36`*


---

<a name="flush"></a>

#### ⚙️ `Flush`

```cpp
void Flush() override
```

**Force le flush de tous les sous-sinks**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:56`*


---

<a name="getformatter"></a>

#### ⚙️ `GetFormatter`

```cpp
Formatter* GetFormatter() override
```

**Obtient le formatter (du premier sink)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:71`*


---

<a name="getpattern"></a>

#### ⚙️ `GetPattern`

```cpp
string GetPattern() override
```

**Obtient le pattern (du premier sink)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:76`*


---

<a name="getsinkcount"></a>

#### ⚙️ `GetSinkCount`

`const`

```cpp
size_t GetSinkCount() const
```

**Obtient la liste des sous-sinks**

**Retour:** Vecteur des sous-sinks

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:102`*


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

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:42`*


---

<a name="log"></a>

#### ⚙️ `Log`

```cpp
void Log(const LogMessage& message) override
```

**Distribue un message à tous les sous-sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `message` | `const LogMessage&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:51`*


---

<a name="removesink"></a>

#### ⚙️ `RemoveSink`

```cpp
void RemoveSink(std::shared_ptr<ISink> sink)
```

**Supprime un sous-sink**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `sink` | `std::shared_ptr<ISink>` | [in] Sink à supprimer |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:91`*


---

<a name="setformatter"></a>

#### ⚙️ `SetFormatter`

```cpp
void SetFormatter(std::unique_ptr<Formatter> formatter) override
```

**Définit le formatter pour tous les sous-sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `formatter` | `std::unique_ptr<Formatter>` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:61`*


---

<a name="setpattern"></a>

#### ⚙️ `SetPattern`

```cpp
void SetPattern(const std::string& pattern) override
```

**Définit le pattern de formatage pour tous les sous-sinks**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `pattern` | `const std::string&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\Sinks\DistributingSink.h:66`*


---

