# 📄 NkSafeArea.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Chemin:** `src\NKWindow\Core\NkSafeArea.h`

### 📦 Fichiers Inclus

- `NkTypes.h`
- `string`

### 🔗 Inclus Par

- [`NkWindow.h`](./NkWindow.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)

## 🎯 Éléments (2)

### ⚙️ Functions (2)

<a name="clippoint"></a>

#### ⚙️ `ClipPoint`

`const`

```cpp
bool ClipPoint(float x, float y, float totalW, float totalH) const
```

**Clipe un point dans la zone sûre (returns false si hors zone)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `x` | `float` |  |
| `y` | `float` |  |
| `totalW` | `float` |  |
| `totalH` | `float` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSafeArea.h:52`*


---

<a name="usablewidth"></a>

#### ⚙️ `UsableWidth`

`const`

```cpp
NkU32 UsableWidth(NkU32 totalWidth) const
```

**Surface utilisable (en pixels)**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `totalWidth` | `NkU32` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSafeArea.h:45`*


---

