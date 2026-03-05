# 📄 NkDialogs.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Chemin:** `src\NKWindow\Core\NkDialogs.h`

### 📦 Fichiers Inclus

- `NkPlatformDetect.h`
- `NkTypes.h`
- `string`
- `vector`
- `cstdlib`
- `cstdio`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)

## 🎯 Éléments (4)

### ⚙️ Functions (3)

<a name="colorpicker"></a>

#### ⚙️ `ColorPicker`

`static`

```cpp
static NkDialogResult ColorPicker(NkU32 initial) = 0
```

**Ouvre un sélecteur de couleur.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `initial` | `NkU32` | [in] Couleur initiale RGBA. |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkDialogs.h:63`*


---

<a name="openfiledialog"></a>

#### ⚙️ `OpenFileDialog`

`static`

```cpp
static NkDialogResult OpenFileDialog(const std::string& filter, const std::string& title)
```

**Ouvre un dialogue de sélection de fichier.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `filter` | `const std::string&` | [in] Filtre type "*.png;*.jpg" (peut être vide pour tous) |
| `title` | `const std::string&` | [in] Titre de la boîte de dialogue. |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkDialogs.h:34`*


---

<a name="savefiledialog"></a>

#### ⚙️ `SaveFileDialog`

`static`

```cpp
static NkDialogResult SaveFileDialog(const std::string& defaultExt, const std::string& title)
```

**Ouvre un dialogue de sauvegarde de fichier.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `defaultExt` | `const std::string&` | [in] Extension par défaut sans point (ex: "png") |
| `title` | `const std::string&` | [in] Titre de la boîte de dialogue. |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkDialogs.h:43`*


---

### 📦 Variables (1)

<a name="title"></a>

#### 📦 `title`

```cpp
string& title
```

**Affiche une boîte de message.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkDialogs.h:52`*


---

