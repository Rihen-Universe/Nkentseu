# 📄 NkAssertHandler.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Gestionnaire d'assertions

**Auteur:** Rihen

**Chemin:** `src\NKCore\Assert\NkAssertHandler.h`

### 📦 Fichiers Inclus

- [`NkAssertion.h`](./NkAssertion.h.md)
- [`NKCore/NkExport.h`](./NkExport.h.md)
- [`NKCore/NkTypes.h`](./NkTypes.h.md)

### 🔗 Inclus Par

- [`NkAssert.h`](./NkAssert.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`core`](../namespaces/core.md)

## 🎯 Éléments (6)

### 🏛️ Classs (2)

<a name="nkasserthandler-nkasserthandler"></a>

#### 🏛️ `NkAssertHandler`

```cpp
class NkAssertHandler
```

**Type de callback pour assertions**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\Assert\NkAssertHandler.h:20`*


---

<a name="nkasserthandler-nkasserthandler-nkasserthandler"></a>

#### 🏛️ `NkAssertHandler`

```cpp
class NkAssertHandler
```

**Gestionnaire global d'assertions**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\Assert\NkAssertHandler.h:25`*


---

### ⚙️ Functions (2)

<a name="defaultcallback"></a>

#### ⚙️ `DefaultCallback`

`static`

```cpp
static NkAssertAction DefaultCallback(const NkAssertionInfo& info)
```

**Callback par défaut**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `info` | `const NkAssertionInfo&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\Assert\NkAssertHandler.h:40`*


---

<a name="handleassertion"></a>

#### ⚙️ `HandleAssertion`

`static`

```cpp
static NkAssertAction HandleAssertion(const NkAssertionInfo& info)
```

**Gère une assertion échouée**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `info` | `const NkAssertionInfo&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\Assert\NkAssertHandler.h:45`*


---

### 🔧 Methods (2)

<a name="nkasserthandler-nkasserthandler-getcallback"></a>

#### 🔧 `GetCallback`

`static`

```cpp
static NkAssertCallback GetCallback()
```

**Récupère callback actuel**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\Assert\NkAssertHandler.h:30`*


---

<a name="nkasserthandler-setcallback"></a>

#### 🔧 `SetCallback`

`static`

```cpp
static void SetCallback(NkAssertCallback callback)
```

**Définit callback custom**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `callback` | `NkAssertCallback` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\Assert\NkAssertHandler.h:35`*


---

