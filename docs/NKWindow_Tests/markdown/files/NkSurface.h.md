# 📄 NkSurface.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Chemin:** `src\NKWindow\Core\NkSurface.h`

### 📦 Fichiers Inclus

- `NkTypes.h`
- `NkPlatformDetect.h`

### 🔗 Inclus Par

- [`NkRenderer.h`](./NkRenderer.h.md)
- [`NkSystem.h`](./NkSystem.h.md)
- [`NkWindow.h`](./NkWindow.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)

## 🎯 Éléments (12)

### 🔧 Methods (2)

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkgetnativedisplayhandle"></a>

#### 🔧 `NkGetNativeDisplayHandle`

```cpp
inline void* NkGetNativeDisplayHandle(const NkSurfaceDesc& surface)
```

**Return platform-native display/instance handle as opaque pointer.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `surface` | `const NkSurfaceDesc&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:223`*


---

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkgetnativewindowhandle"></a>

#### 🔧 `NkGetNativeWindowHandle`

```cpp
inline void* NkGetNativeWindowHandle(const NkSurfaceDesc& surface)
```

**Return platform-native window handle as opaque pointer.**

**Paramètres:**

| Nom | Type | Description |
|-----|------|-------------|
| `surface` | `const NkSurfaceDesc&` |  |

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:201`*


---

### 🏗️ Structs (7)

<a name="nkframebufferinfo-nkframebufferinfo"></a>

#### 🏗️ `NkFramebufferInfo`

```cpp
struct NkFramebufferInfo
```

**Selected backend API.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:100`*


---

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo"></a>

#### 🏗️ `NkFramebufferInfo`

```cpp
struct NkFramebufferInfo
```

**Color buffer pixel format.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:102`*


---

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo"></a>

#### 🏗️ `NkFramebufferInfo`

```cpp
struct NkFramebufferInfo
```

**Depth/stencil format.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:104`*


---

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo"></a>

#### 🏗️ `NkFramebufferInfo`

```cpp
struct NkFramebufferInfo
```

**Enable vertical synchronization.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:107`*


---

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo"></a>

#### 🏗️ `NkFramebufferInfo`

```cpp
struct NkFramebufferInfo
```

**Enable validation/debug layers when available.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:109`*


---

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo"></a>

#### 🏗️ `NkFramebufferInfo`

```cpp
struct NkFramebufferInfo
```

**When true, BeginFrame() automatically resizes the framebuffer if the**

window dimensions have changed since the last frame — the application does not need to handle NkWindowResizeEvent manually. When false, the application calls NkRenderer::Resize() itself.

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:112`*


---

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkopenglrenderercontext-nkopenglrenderercontext"></a>

#### 🏗️ `NkOpenGLRendererContext`

```cpp
struct NkOpenGLRendererContext
```

**Software framebuffer exposed by software backend.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:137`*


---

### 📦 Variables (3)

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-api"></a>

#### 📦 `api`

```cpp
NkRendererApi api
```

**Active backend API.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:187`*


---

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-framebuffer"></a>

#### 📦 `framebuffer`

`const`

```cpp
const NkFramebufferInfo& framebuffer
```

**Build a portable runtime context for renderer backends.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:245`*


---

<a name="nkframebufferinfo-nkframebufferinfo-nkframebufferinfo-userdata"></a>

#### 📦 `userData`

```cpp
void* userData
```

**Optional user-owned backend data pointer.**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKWindow\src\NKWindow\Core\NkSurface.h:197`*


---

