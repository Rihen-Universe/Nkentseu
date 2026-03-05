# 📄 NkCGXDetect.h

[🏠 Accueil](../index.md) | [📁 Fichiers](./index.md)

## Informations

**Description:** Détection GPU, APIs graphiques et calcul (CUDA/OpenCL)

**Auteur:** Rihen

**Chemin:** `src\NKCore\NkCGXDetect.h`

### 📦 Fichiers Inclus

- `NkPlatformDetect.h`
- `NkArchDetect.h`

### 🔗 Inclus Par

- [`NkConfig.h`](./NkConfig.h.md)

### 🗂️ Namespaces

- [`nkentseu`](../namespaces/nkentseu.md)
- [`platform`](../namespaces/platform.md)
- [`graphics`](../namespaces/graphics.md)

## 🎯 Éléments (15)

### 🔣 Macros (15)

<a name="nkentseu_graphics_d3d11_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_D3D11_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_D3D11_AVAILABLE /**
```

**Direct3D 11 disponible sur Windows**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:40`*


---

<a name="nkentseu_graphics_d3d12_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_D3D12_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_D3D12_AVAILABLE /**
```

**Direct3D 12 disponible sur Windows 10+**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:46`*


---

<a name="nkentseu_graphics_d3d12_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_D3D12_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_D3D12_AVAILABLE )
```

**Direct3D 12 disponible sur Xbox Series X|S**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:179`*


---

<a name="nkentseu_graphics_gles3_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_GLES3_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_GLES3_AVAILABLE /**
```

**OpenGL ES 3.x disponible sur Android**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:121`*


---

<a name="nkentseu_graphics_gnm_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_GNM_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_GNM_AVAILABLE /**
```

**GNM disponible sur PS5**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:157`*


---

<a name="nkentseu_graphics_metal_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_METAL_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_METAL_AVAILABLE /**
```

**Metal disponible sur macOS**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:84`*


---

<a name="nkentseu-nkentseu_graphics_nvn_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_NVN_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_NVN_AVAILABLE /**
```

**NVN (NVIDIA API) disponible sur Switch**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:196`*


---

<a name="nkentseu_graphics_opengl_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_OPENGL_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE /**
```

**OpenGL disponible sur Windows**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:52`*


---

<a name="nkentseu_graphics_opengl_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_OPENGL_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE #define NKENTSEU_GRAPHICS_OPENGL_DEPRECATED
```

**OpenGL disponible sur macOS (deprecated depuis 10.14)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:90`*


---

<a name="nkentseu_graphics_vulkan_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_VULKAN_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE )
```

**Vulkan disponible sur Windows**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:58`*


---

<a name="nkentseu_graphics_vulkan_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_VULKAN_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE #define NKENTSEU_GRAPHICS_VULKAN_VIA_MOLTENVK
```

**Vulkan disponible via MoltenVK**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:97`*


---

<a name="nkentseu_graphics_vulkan_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_VULKAN_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE )
```

**Vulkan disponible sur Android (API 24+)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:127`*


---

<a name="nkentseu_graphics_vulkan_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_VULKAN_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE )
```

**Vulkan disponible sur PS5**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:163`*


---

<a name="nkentseu_graphics_webgl2_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE /**
```

**WebGL 2.0 disponible via Emscripten**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:139`*


---

<a name="nkentseu_graphics_webgpu_available"></a>

#### 🔣 `NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE`

```cpp
#define NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE )
```

**WebGPU disponible (expérimental)**

*Défini dans: `E:\Projets\MacShared\Projets\Jenga\Jenga\Exemples\NKWindow\NKCore\src\NKCore\NkCGXDetect.h:145`*


---

