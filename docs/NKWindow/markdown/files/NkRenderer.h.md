# 📄 NkRenderer.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Chemin:** `src\NKWindow\Core\NkRenderer.h`

### 📦 Fichiers Inclus

- [`NkSurface.h`](./NkSurface.h.md)
- `NkTypes.h`
- `functional`
- `memory`
- `string`
- `vector`

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)

## 🎯 Éléments (3)

### 🏛️ Classs (1)

<a name="nkrenderer-nkrenderer"></a>

#### 🏛️ `NkRenderer`

```cpp
class NkRenderer
```

**Public rendering facade independent from backend implementation. * NkRenderer delegates rendering work to INkRendererImpl and exposes backend-agnostic frame lifecycle and pixel operations.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkRenderer.h:58`*


---

### 🔧 Methods (1)

<a name="nkrenderer-registerexternalrendererfactory"></a>

#### 🔧 `RegisterExternalRendererFactory`

`static`

```cpp
static bool RegisterExternalRendererFactory(NkRendererApi api, NkRendererFactory factory)
```

**Register a user-defined renderer factory for a given API. * This lets users plug their own OpenGL/Vulkan/DirectX/Metal/WebGL backend implementation while reusing NkWindow surface creation.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `api` | `NkRendererApi` |  |
| `factory` | `NkRendererFactory` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkRenderer.h:97`*


---

### 📦 Variables (1)

<a name="nkrenderer-a"></a>

#### 📦 `a`

```cpp
NkU8 a
```

**Active/désactive la présentation vers la fenêtre. Si désactivé, le renderer peut fonctionner en offscreen.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkRenderer.h:144`*


---

