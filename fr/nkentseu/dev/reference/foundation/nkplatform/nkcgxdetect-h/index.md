# NkCGXDetect.h

`#include "NKPlatform/NkCGXDetect.h"`

## Types

- [nkentseu::graphics::NkGraphicsApi](../nkentseu-graphics-nkgraphicsapi/index.md)
- [nkentseu::graphics::NkGPUVendor](../nkentseu-graphics-nkgpuvendor/index.md)
- [nkentseu::graphics::NkGPUType](../nkentseu-graphics-nkgputype/index.md)

## Fonctions

### ToString

```cpp
template <typename CharT = char> inline constexpr const CharT *ToString(NkGraphicsApi api) noexcept
```

Convertit une enum NkGraphicsApi en chaîne de caractères

### ToString

```cpp
template <typename CharT = char> inline constexpr const CharT *ToString(NkGPUVendor vendor) noexcept
```

Convertit une enum NkGPUVendor en chaîne de caractères

### ToString

```cpp
template <typename CharT = char> inline constexpr const CharT *ToString(NkGPUType type) noexcept
```

Convertit une enum NkGPUType en chaîne de caractères

### IsAPIAvailable

```cpp
template <NkGraphicsApi API> inline constexpr bool IsAPIAvailable() noexcept
```

Vérifie à la compilation si une API graphique est disponible

### GetDefaultAPI

```cpp
template <typename = void> inline constexpr NkGraphicsApi GetDefaultAPI() noexcept
```

Obtient l'API graphique par défaut recommandée pour la plateforme

### GetModernAPI

```cpp
template <typename = void> inline constexpr NkGraphicsApi GetModernAPI() noexcept
```

Obtient l'API graphique la plus moderne disponible sur la plateforme

## Macros

### NKENTSEU_GRAPHICS_D3D11_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_D3D11_AVAILABLE
```

Direct3D 11 disponible sur Windows

### NKENTSEU_GRAPHICS_D3D12_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_D3D12_AVAILABLE
```

Direct3D 12 disponible sur Windows 10+

### NKENTSEU_GRAPHICS_OPENGL_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE
```

OpenGL disponible sur Windows

### NKENTSEU_GRAPHICS_VULKAN_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
```

Vulkan disponible sur Windows

### NKENTSEU_GRAPHICS_METAL_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_METAL_AVAILABLE
```

Metal disponible sur macOS

### NKENTSEU_GRAPHICS_OPENGL_DEPRECATED

```cpp
#define NKENTSEU_GRAPHICS_OPENGL_DEPRECATED
```

Définie, sans valeur, sur macOS : Apple y déclare OpenGL obsolète.

### NKENTSEU_GRAPHICS_VULKAN_VIA_MOLTENVK

```cpp
#define NKENTSEU_GRAPHICS_VULKAN_VIA_MOLTENVK
```

Définie, sans valeur, sur les systèmes d'Apple : Vulkan y passe par MoltenVK.

### NKENTSEU_GRAPHICS_GLES3_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_GLES3_AVAILABLE
```

Définie, sans valeur, quand OpenGL ES 3 est disponible sur la cible.

### NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE
```

WebGL 2.0 disponible via Emscripten

### NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE
```

WebGPU disponible (support expérimental)

### NKENTSEU_GRAPHICS_GNM_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_GNM_AVAILABLE
```

GNM (API Sony) disponible sur PS5

### NKENTSEU_GRAPHICS_NVN_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_NVN_AVAILABLE
```

NVN (API NVIDIA) disponible sur Nintendo Switch

### NKENTSEU_GRAPHICS_DEFAULT

```cpp
#define NKENTSEU_GRAPHICS_DEFAULT
```

L'API graphique choisie par défaut pour la cible (une valeur de NkGraphicsApi).

### NKENTSEU_GRAPHICS_MODERN

```cpp
#define NKENTSEU_GRAPHICS_MODERN
```

L'API graphique moderne préférée sur la cible (une valeur de NkGraphicsApi).

### NKENTSEU_D3D11_ONLY

```cpp
#define NKENTSEU_D3D11_ONLY(...)
```

Garde son argument quand Direct3D 11 est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_D3D11

```cpp
#define NKENTSEU_NOT_D3D11(...)
```

Retire son argument quand Direct3D 11 est disponible sur la cible, le garde sinon.

### NKENTSEU_D3D12_ONLY

```cpp
#define NKENTSEU_D3D12_ONLY(...)
```

Garde son argument quand Direct3D 12 est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_D3D12

```cpp
#define NKENTSEU_NOT_D3D12(...)
```

Retire son argument quand Direct3D 12 est disponible sur la cible, le garde sinon.

### NKENTSEU_VULKAN_ONLY

```cpp
#define NKENTSEU_VULKAN_ONLY(...)
```

Garde son argument quand Vulkan est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_VULKAN

```cpp
#define NKENTSEU_NOT_VULKAN(...)
```

Retire son argument quand Vulkan est disponible sur la cible, le garde sinon.

### NKENTSEU_METAL_ONLY

```cpp
#define NKENTSEU_METAL_ONLY(...)
```

Garde son argument quand Metal est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_METAL

```cpp
#define NKENTSEU_NOT_METAL(...)
```

Retire son argument quand Metal est disponible sur la cible, le garde sinon.

### NKENTSEU_OPENGL_ONLY

```cpp
#define NKENTSEU_OPENGL_ONLY(...)
```

Garde son argument quand OpenGL est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_OPENGL

```cpp
#define NKENTSEU_NOT_OPENGL(...)
```

Retire son argument quand OpenGL est disponible sur la cible, le garde sinon.

### NKENTSEU_GLES_ONLY

```cpp
#define NKENTSEU_GLES_ONLY(...)
```

Garde son argument quand OpenGL ES 3 est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_GLES

```cpp
#define NKENTSEU_NOT_GLES(...)
```

Retire son argument quand OpenGL ES 3 est disponible sur la cible, le garde sinon.

### NKENTSEU_WEBGL_ONLY

```cpp
#define NKENTSEU_WEBGL_ONLY(...)
```

Garde son argument quand WebGL 2 est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_WEBGL

```cpp
#define NKENTSEU_NOT_WEBGL(...)
```

Retire son argument quand WebGL 2 est disponible sur la cible, le garde sinon.

### NKENTSEU_WEBGPU_ONLY

```cpp
#define NKENTSEU_WEBGPU_ONLY(...)
```

Garde son argument quand WebGPU est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_WEBGPU

```cpp
#define NKENTSEU_NOT_WEBGPU(...)
```

Retire son argument quand WebGPU est disponible sur la cible, le garde sinon.

### NKENTSEU_GNM_ONLY

```cpp
#define NKENTSEU_GNM_ONLY(...)
```

Garde son argument quand GNM (PlayStation) est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_GNM

```cpp
#define NKENTSEU_NOT_GNM(...)
```

Retire son argument quand GNM (PlayStation) est disponible sur la cible, le garde sinon.

### NKENTSEU_NVN_ONLY

```cpp
#define NKENTSEU_NVN_ONLY(...)
```

Garde son argument quand NVN (Nintendo Switch) est disponible sur la cible, le retire sinon.

### NKENTSEU_NOT_NVN

```cpp
#define NKENTSEU_NOT_NVN(...)
```

Retire son argument quand NVN (Nintendo Switch) est disponible sur la cible, le garde sinon.

### NKENTSEU_COMPUTE_CUDA_AVAILABLE

```cpp
#define NKENTSEU_COMPUTE_CUDA_AVAILABLE
```

CUDA Compiler detected - NVIDIA CUDA available

### NKENTSEU_COMPUTE_AVAILABLE

```cpp
#define NKENTSEU_COMPUTE_AVAILABLE
```

Définie, sans valeur, quand le code est compilé pour CUDA, OpenCL ou SYCL.

### NKENTSEU_COMPUTE_OPENCL_AVAILABLE

```cpp
#define NKENTSEU_COMPUTE_OPENCL_AVAILABLE
```

OpenCL C version macro detected - OpenCL available

### NKENTSEU_COMPUTE_SYCL_AVAILABLE

```cpp
#define NKENTSEU_COMPUTE_SYCL_AVAILABLE
```

SYCL language macros detected - SYCL available

### NKENTSEU_CUDA_ONLY

```cpp
#define NKENTSEU_CUDA_ONLY(...)
```

Garde son argument quand le code est compilé pour CUDA, le retire sinon.

### NKENTSEU_NOT_CUDA

```cpp
#define NKENTSEU_NOT_CUDA(...)
```

Retire son argument quand le code est compilé pour CUDA, le garde sinon.

### NKENTSEU_OPENCL_ONLY

```cpp
#define NKENTSEU_OPENCL_ONLY(...)
```

Garde son argument quand le code est compilé pour OpenCL, le retire sinon.

### NKENTSEU_NOT_OPENCL

```cpp
#define NKENTSEU_NOT_OPENCL(...)
```

Retire son argument quand le code est compilé pour OpenCL, le garde sinon.

### NKENTSEU_SYCL_ONLY

```cpp
#define NKENTSEU_SYCL_ONLY(...)
```

Garde son argument quand le code est compilé pour SYCL, le retire sinon.

### NKENTSEU_NOT_SYCL

```cpp
#define NKENTSEU_NOT_SYCL(...)
```

Retire son argument quand le code est compilé pour SYCL, le garde sinon.

### NKENTSEU_DISPLAY_WAYLAND

```cpp
#define NKENTSEU_DISPLAY_WAYLAND
```

Wayland display server detected

### NKENTSEU_DISPLAY_XCB

```cpp
#define NKENTSEU_DISPLAY_XCB
```

XCB (X C Binding) detected - low-level X11 interface

### NKENTSEU_DISPLAY_XLIB

```cpp
#define NKENTSEU_DISPLAY_XLIB
```

Xlib detected - traditional X11 interface

### NKENTSEU_GPU_VENDOR_NVIDIA_ID

```cpp
#define NKENTSEU_GPU_VENDOR_NVIDIA_ID
```

NVIDIA Corporation

### NKENTSEU_GPU_VENDOR_AMD_ID

```cpp
#define NKENTSEU_GPU_VENDOR_AMD_ID
```

AMD / ATI Technologies

### NKENTSEU_GPU_VENDOR_INTEL_ID

```cpp
#define NKENTSEU_GPU_VENDOR_INTEL_ID
```

Intel Corporation

### NKENTSEU_GPU_VENDOR_ARM_ID

```cpp
#define NKENTSEU_GPU_VENDOR_ARM_ID
```

ARM Holdings (Mali)

### NKENTSEU_GPU_VENDOR_QUALCOMM_ID

```cpp
#define NKENTSEU_GPU_VENDOR_QUALCOMM_ID
```

Qualcomm (Adreno)

### NKENTSEU_GPU_VENDOR_APPLE_ID

```cpp
#define NKENTSEU_GPU_VENDOR_APPLE_ID
```

Apple Inc.

### NKENTSEU_GPU_VENDOR_IMGTEC_ID

```cpp
#define NKENTSEU_GPU_VENDOR_IMGTEC_ID
```

Imagination Technologies (PowerVR)

### NKENTSEU_GPU_VENDOR_BROADCOM_ID

```cpp
#define NKENTSEU_GPU_VENDOR_BROADCOM_ID
```

Broadcom (VideoCore)

### NKENTSEU_GPU_VENDOR_MICROSOFT_ID

```cpp
#define NKENTSEU_GPU_VENDOR_MICROSOFT_ID
```

Microsoft (WARP software renderer)

### NKENTSEU_GFX_NONE

```cpp
#define NKENTSEU_GFX_NONE
```

Aucune API (état invalide)

### NKENTSEU_GFX_VULKAN

```cpp
#define NKENTSEU_GFX_VULKAN
```

Vulkan

### NKENTSEU_GFX_METAL

```cpp
#define NKENTSEU_GFX_METAL
```

Metal

### NKENTSEU_GFX_DIRECTX

```cpp
#define NKENTSEU_GFX_DIRECTX
```

Direct3D (11 ou 12)

### NKENTSEU_GFX_OPENGL

```cpp
#define NKENTSEU_GFX_OPENGL
```

OpenGL / OpenGL ES

### NKENTSEU_GFX_SOFTWARE

```cpp
#define NKENTSEU_GFX_SOFTWARE
```

Renderer logiciel (fallback)

### NKENTSEU_GFX_VERSION_CALC

```cpp
#define NKENTSEU_GFX_VERSION_CALC(major, minor)
```

Encode une version (major, minor) en entier 32 bits

### NKENTSEU_GFX_ACTIVE

```cpp
#define NKENTSEU_GFX_ACTIVE
```

L'API graphique active (une valeur NKENTSEU_GFX_*), forcée par NKENTSEU_GFX_FORCE ou détectée.

### NKENTSEU_GFX_VERSION

```cpp
#define NKENTSEU_GFX_VERSION
```

La version de l'API graphique active, calculée par NKENTSEU_GFX_VERSION_CALC.

