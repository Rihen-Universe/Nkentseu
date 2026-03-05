# 📄 LogMessage.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Structure contenant toutes les informations d'un message de log.

**Auteur:** Rihen

**Chemin:** `src\Logger\LogMessage.h`

### 📦 Fichiers Inclus

- `string`
- `chrono`
- `NKCore/NkTypes.h`
- [`Logger/LogLevel.h`](./LogLevel.h.md)

### 🔗 Inclus Par

- [`Formatter.h`](./Formatter.h.md)
- [`Logger.cpp`](./Logger.cpp.md)
- [`LogMessage.cpp`](./LogMessage.cpp.md)
- [`Sink.h`](./Sink.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`logger`](../namespaces/logger.md)

## 🎯 Éléments (11)

### ⚙️ Functions (7)

<a name="getlocaltime"></a>

#### ⚙️ `GetLocalTime`

`const`

```cpp
tm GetLocalTime() const
```

**Obtient l'heure sous forme de structure tm**

**Retour:** Structure tm avec l'heure locale

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:122`*


---

<a name="getmicros"></a>

#### ⚙️ `GetMicros`

`const`

```cpp
uint64 GetMicros() const
```

**Obtient le timestamp en microsecondes**

**Retour:** Timestamp en microsecondes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:140`*


---

<a name="getmillis"></a>

#### ⚙️ `GetMillis`

`const`

```cpp
uint64 GetMillis() const
```

**Obtient le timestamp en millisecondes**

**Retour:** Timestamp en millisecondes

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:134`*


---

<a name="getseconds"></a>

#### ⚙️ `GetSeconds`

`const`

```cpp
double GetSeconds() const
```

**Obtient le timestamp en secondes**

**Retour:** Timestamp en secondes (double)

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:146`*


---

<a name="getutctime"></a>

#### ⚙️ `GetUTCTime`

`const`

```cpp
tm GetUTCTime() const
```

**Obtient l'heure sous forme de structure tm (UTC)**

**Retour:** Structure tm avec l'heure UTC

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:128`*


---

<a name="isvalid"></a>

#### ⚙️ `IsValid`

`const`

```cpp
bool IsValid() const
```

**Vérifie si le message est valide**

**Retour:** true si valide, false sinon

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:116`*


---

<a name="reset"></a>

#### ⚙️ `Reset`

```cpp
void Reset()
```

**Réinitialise le message**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:111`*


---

### 📦 Variables (4)

<a name="threadid"></a>

#### 📦 `threadId`

```cpp
uint32 threadId
```

**ID du thread émetteur**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:40`*


---

<a name="threadname"></a>

#### 📦 `threadName`

```cpp
string threadName
```

**Nom du thread (optionnel)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:43`*


---

<a name="timepoint"></a>

#### 📦 `timePoint`

```cpp
time_point timePoint
```

**Heure de réception du message**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:33`*


---

<a name="timestamp"></a>

#### 📦 `timestamp`

```cpp
uint64 timestamp
```

**Timestamp en nanosecondes depuis l'epoch**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKLogger\src\Logger\LogMessage.h:30`*


---

