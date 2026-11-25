# NkCGXDetect.h

`#include "NKPlatform/NkCGXDetect.h"`

## Types

- [nkentseu::graphics::NkGraphicsApi](../nkentseu-graphics-nkgraphicsapi/index.md)
- [nkentseu::graphics::NkGPUVendor](../nkentseu-graphics-nkgpuvendor/index.md)
- [nkentseu::graphics::NkGPUType](../nkentseu-graphics-nkgputype/index.md)

## Functions

### ToString

```cpp
template <typename CharT = char> inline constexpr const CharT *ToString(NkGraphicsApi api) noexcept
```

Converts a NkGraphicsApi enum to a string

### ToString

```cpp
template <typename CharT = char> inline constexpr const CharT *ToString(NkGPUVendor vendor) noexcept
```

Converts a NkGPUVendor enum to a string

### ToString

```cpp
template <typename CharT = char> inline constexpr const CharT *ToString(NkGPUType type) noexcept
```

Converts a NkGPUType enum to a string

### IsAPIAvailable

```cpp
template <NkGraphicsApi API> inline constexpr bool IsAPIAvailable() noexcept
```

Checks at compile time whether a graphics API is available

### GetDefaultAPI

```cpp
template <typename = void> inline constexpr NkGraphicsApi GetDefaultAPI() noexcept
```

Gets the default graphics API recommended for the platform

### GetModernAPI

```cpp
template <typename = void> inline constexpr NkGraphicsApi GetModernAPI() noexcept
```

Gets the most modern graphics API available on the platform

## Macros

### NKENTSEU_GRAPHICS_D3D11_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_D3D11_AVAILABLE
```

Direct3D 11 available on Windows

### NKENTSEU_GRAPHICS_D3D12_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_D3D12_AVAILABLE
```

Direct3D 12 available on Windows 10+

### NKENTSEU_GRAPHICS_OPENGL_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_OPENGL_AVAILABLE
```

OpenGL available on Windows

### NKENTSEU_GRAPHICS_VULKAN_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_VULKAN_AVAILABLE
```

Vulkan available on Windows

### NKENTSEU_GRAPHICS_METAL_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_METAL_AVAILABLE
```

Metal available on macOS

### NKENTSEU_GRAPHICS_OPENGL_DEPRECATED

```cpp
#define NKENTSEU_GRAPHICS_OPENGL_DEPRECATED
```

Defined, with no value, on macOS: Apple marks OpenGL as deprecated there.

### NKENTSEU_GRAPHICS_VULKAN_VIA_MOLTENVK

```cpp
#define NKENTSEU_GRAPHICS_VULKAN_VIA_MOLTENVK
```

Defined, with no value, on Apple systems: Vulkan goes through MoltenVK there.

### NKENTSEU_GRAPHICS_GLES3_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_GLES3_AVAILABLE
```

Defined, with no value, when OpenGL ES 3 is available on the target.

### NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_WEBGL2_AVAILABLE
```

WebGL 2.0 available through Emscripten

### NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_WEBGPU_AVAILABLE
```

WebGPU available (experimental support)

### NKENTSEU_GRAPHICS_GNM_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_GNM_AVAILABLE
```

GNM (Sony API) available on PS5

### NKENTSEU_GRAPHICS_NVN_AVAILABLE

```cpp
#define NKENTSEU_GRAPHICS_NVN_AVAILABLE
```

NVN (NVIDIA API) available on Nintendo Switch

### NKENTSEU_GRAPHICS_DEFAULT

```cpp
#define NKENTSEU_GRAPHICS_DEFAULT
```

The graphics API chosen by default for the target (a NkGraphicsApi value).

### NKENTSEU_GRAPHICS_MODERN

```cpp
#define NKENTSEU_GRAPHICS_MODERN
```

The preferred modern graphics API on the target (a NkGraphicsApi value).

### NKENTSEU_D3D11_ONLY

```cpp
#define NKENTSEU_D3D11_ONLY(...)
```

Keeps its argument when Direct3D 11 is available on the target, removes it otherwise.

### NKENTSEU_NOT_D3D11

```cpp
#define NKENTSEU_NOT_D3D11(...)
```

Removes its argument when Direct3D 11 is available on the target, keeps it otherwise.

### NKENTSEU_D3D12_ONLY

```cpp
#define NKENTSEU_D3D12_ONLY(...)
```

Keeps its argument when Direct3D 12 is available on the target, removes it otherwise.

### NKENTSEU_NOT_D3D12

```cpp
#define NKENTSEU_NOT_D3D12(...)
```

Removes its argument when Direct3D 12 is available on the target, keeps it otherwise.

### NKENTSEU_VULKAN_ONLY

```cpp
#define NKENTSEU_VULKAN_ONLY(...)
```

Keeps its argument when Vulkan is available on the target, removes it otherwise.

### NKENTSEU_NOT_VULKAN

```cpp
#define NKENTSEU_NOT_VULKAN(...)
```

Removes its argument when Vulkan is available on the target, keeps it otherwise.

### NKENTSEU_METAL_ONLY

```cpp
#define NKENTSEU_METAL_ONLY(...)
```

Keeps its argument when Metal is available on the target, removes it otherwise.

### NKENTSEU_NOT_METAL

```cpp
#define NKENTSEU_NOT_METAL(...)
```

Removes its argument when Metal is available on the target, keeps it otherwise.

### NKENTSEU_OPENGL_ONLY

```cpp
#define NKENTSEU_OPENGL_ONLY(...)
```

Keeps its argument when OpenGL is available on the target, removes it otherwise.

### NKENTSEU_NOT_OPENGL

```cpp
#define NKENTSEU_NOT_OPENGL(...)
```

Removes its argument when OpenGL is available on the target, keeps it otherwise.

### NKENTSEU_GLES_ONLY

```cpp
#define NKENTSEU_GLES_ONLY(...)
```

Keeps its argument when OpenGL ES 3 is available on the target, removes it otherwise.

### NKENTSEU_NOT_GLES

```cpp
#define NKENTSEU_NOT_GLES(...)
```

Removes its argument when OpenGL ES 3 is available on the target, keeps it otherwise.

### NKENTSEU_WEBGL_ONLY

```cpp
#define NKENTSEU_WEBGL_ONLY(...)
```

Keeps its argument when WebGL 2 is available on the target, removes it otherwise.

### NKENTSEU_NOT_WEBGL

```cpp
#define NKENTSEU_NOT_WEBGL(...)
```

Removes its argument when WebGL 2 is available on the target, keeps it otherwise.

### NKENTSEU_WEBGPU_ONLY

```cpp
#define NKENTSEU_WEBGPU_ONLY(...)
```

Keeps its argument when WebGPU is available on the target, removes it otherwise.

### NKENTSEU_NOT_WEBGPU

```cpp
#define NKENTSEU_NOT_WEBGPU(...)
```

Removes its argument when WebGPU is available on the target, keeps it otherwise.

### NKENTSEU_GNM_ONLY

```cpp
#define NKENTSEU_GNM_ONLY(...)
```

Keeps its argument when GNM (PlayStation) is available on the target, removes it otherwise.

### NKENTSEU_NOT_GNM

```cpp
#define NKENTSEU_NOT_GNM(...)
```

Removes its argument when GNM (PlayStation) is available on the target, keeps it otherwise.

### NKENTSEU_NVN_ONLY

```cpp
#define NKENTSEU_NVN_ONLY(...)
```

Keeps its argument when NVN (Nintendo Switch) is available on the target, removes it otherwise.

### NKENTSEU_NOT_NVN

```cpp
#define NKENTSEU_NOT_NVN(...)
```

Removes its argument when NVN (Nintendo Switch) is available on the target, keeps it otherwise.

### NKENTSEU_COMPUTE_CUDA_AVAILABLE

```cpp
#define NKENTSEU_COMPUTE_CUDA_AVAILABLE
```

CUDA compiler detected: NVIDIA CUDA available

### NKENTSEU_COMPUTE_AVAILABLE

```cpp
#define NKENTSEU_COMPUTE_AVAILABLE
```

Defined, with no value, when the code is compiled for CUDA, OpenCL or SYCL.

### NKENTSEU_COMPUTE_OPENCL_AVAILABLE

```cpp
#define NKENTSEU_COMPUTE_OPENCL_AVAILABLE
```

OpenCL C version macro detected: OpenCL available

### NKENTSEU_COMPUTE_SYCL_AVAILABLE

```cpp
#define NKENTSEU_COMPUTE_SYCL_AVAILABLE
```

SYCL language macros detected: SYCL available

### NKENTSEU_CUDA_ONLY

```cpp
#define NKENTSEU_CUDA_ONLY(...)
```

Keeps its argument when the code is compiled for CUDA, removes it otherwise.

### NKENTSEU_NOT_CUDA

```cpp
#define NKENTSEU_NOT_CUDA(...)
```

Removes its argument when the code is compiled for CUDA, keeps it otherwise.

### NKENTSEU_OPENCL_ONLY

```cpp
#define NKENTSEU_OPENCL_ONLY(...)
```

Keeps its argument when the code is compiled for OpenCL, removes it otherwise.

### NKENTSEU_NOT_OPENCL

```cpp
#define NKENTSEU_NOT_OPENCL(...)
```

Removes its argument when the code is compiled for OpenCL, keeps it otherwise.

### NKENTSEU_SYCL_ONLY

```cpp
#define NKENTSEU_SYCL_ONLY(...)
```

Keeps its argument when the code is compiled for SYCL, removes it otherwise.

### NKENTSEU_NOT_SYCL

```cpp
#define NKENTSEU_NOT_SYCL(...)
```

Removes its argument when the code is compiled for SYCL, keeps it otherwise.

### NKENTSEU_DISPLAY_WAYLAND

```cpp
#define NKENTSEU_DISPLAY_WAYLAND
```

Wayland display server detected

### NKENTSEU_DISPLAY_XCB

```cpp
#define NKENTSEU_DISPLAY_XCB
```

XCB (X C Binding) detected: low-level X11 interface

### NKENTSEU_DISPLAY_XLIB

```cpp
#define NKENTSEU_DISPLAY_XLIB
```

Xlib detected: traditional X11 interface

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

No API (invalid state)

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

Direct3D (11 or 12)

### NKENTSEU_GFX_OPENGL

```cpp
#define NKENTSEU_GFX_OPENGL
```

OpenGL / OpenGL ES

### NKENTSEU_GFX_SOFTWARE

```cpp
#define NKENTSEU_GFX_SOFTWARE
```

Software renderer (fallback)

### NKENTSEU_GFX_VERSION_CALC

```cpp
#define NKENTSEU_GFX_VERSION_CALC(major, minor)
```

Encodes a (major, minor) version as a 32-bit integer

### NKENTSEU_GFX_ACTIVE

```cpp
#define NKENTSEU_GFX_ACTIVE
```

The active graphics API (an NKENTSEU_GFX_* value), forced by NKENTSEU_GFX_FORCE or detected.

### NKENTSEU_GFX_VERSION

```cpp
#define NKENTSEU_GFX_VERSION
```

The version of the active graphics API, computed by NKENTSEU_GFX_VERSION_CALC.

