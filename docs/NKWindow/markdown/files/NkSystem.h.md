# 📄 NkSystem.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Chemin:** `src\NKWindow\Core\NkSystem.h`

### 📦 Fichiers Inclus

- `NkTypes.h`
- `NkPlatformDetect.h`
- [`NkSurface.h`](./NkSurface.h.md)
- `string`
- `memory`

### 🔗 Inclus Par

- [`NkWindow.h`](./NkWindow.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)

## 🎯 Éléments (3)

### ⚙️ Functions (3)

<a name="nkclose"></a>

#### ⚙️ `NkClose`

```cpp
inline void NkClose()
```

**Libère toutes les ressources du framework. Appeler avant la fin de nkmain() / main().**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSystem.h:115`*


---

<a name="nkgeteventimpl"></a>

#### ⚙️ `NkGetEventImpl`

```cpp
inline IEventImpl* NkGetEventImpl()
```

**Accède à l'implémentation d'événements active (plateforme courante).**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSystem.h:124`*


---

<a name="nkinitialise"></a>

#### ⚙️ `NkInitialise`

```cpp
inline bool NkInitialise(const NkAppData& data)
```

**Initialise le framework NkWindow. Doit être appelé une fois en début de programme (ou dans nkmain()). ***

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `data` | `const NkAppData&` |  |

**Exemples:**

```cpp
*   nkentseu::NkAppData data;
 *   data.preferredRenderer = nkentseu::NkRendererApi::NK_OPENGL;
 *   nkentseu::NkInitialise(data);
 *
```

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSystem.h:100`*


---

