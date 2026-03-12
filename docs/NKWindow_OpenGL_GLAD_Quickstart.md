# NKWindow + OpenGL + GLAD Quickstart

This guide shows how to bootstrap OpenGL with `NkContext` and GLAD.

## 1) What `NkContext` does

- `NkContext` creates and owns the native graphics context for OpenGL.
- You configure it with defaults + hints (`NkContextWindowHint`) or full config (`NkContextSetHints`).
- For Vulkan/DX/Metal/Software, `NkContext` is in surface-only mode.

## 2) Download GLAD

You can use one of these workflows.

### Option A: GLAD generator (recommended)

1. Open GLAD generator.
2. API: OpenGL
3. Profile: Core
4. Version: match your `NkContext` hints (example: 4.5)
5. Language: C/C++
6. Generate and copy:
   - `include/glad/...`
   - `src/glad.c` (or `glad.cpp` depending on generator output)

### Option B: Git submodule/vendor

1. Add GLAD source in your repository under `Externals/glad`.
2. Expose include path and compile the GLAD source in your project.

## 3) Add GLAD to your Jenga project

In your app project:

- add include dir: `Externals/glad/include`
- add source file: `Externals/glad/src/glad.c` (or equivalent)

## 4) Configure OpenGL hints with NkContext

```cpp
#include "NKWindow/Core/NkContext.h"

nkentseu::NkContextInit();
nkentseu::NkContextResetHints();
nkentseu::NkContextSetApi(nkentseu::NkRendererApi::NK_OPENGL);
nkentseu::NkContextWindowHint(nkentseu::NkContextHint::NK_CONTEXT_HINT_VERSION_MAJOR, 4);
nkentseu::NkContextWindowHint(nkentseu::NkContextHint::NK_CONTEXT_HINT_VERSION_MINOR, 5);
nkentseu::NkContextWindowHint(nkentseu::NkContextHint::NK_CONTEXT_HINT_PROFILE,
                              (int)nkentseu::NkContextProfile::NK_CONTEXT_PROFILE_CORE);
nkentseu::NkContextWindowHint(nkentseu::NkContextHint::NK_CONTEXT_HINT_DOUBLEBUFFER, 1);
nkentseu::NkContextWindowHint(nkentseu::NkContextHint::NK_CONTEXT_HINT_DEPTH_BITS, 24);
nkentseu::NkContextWindowHint(nkentseu::NkContextHint::NK_CONTEXT_HINT_STENCIL_BITS, 8);
nkentseu::NkContextWindowHint(nkentseu::NkContextHint::NK_CONTEXT_HINT_MSAA_SAMPLES, 4);
```

Win32-only full `PIXELFORMATDESCRIPTOR` control:

```cpp
nkentseu::NkWin32PixelFormatConfig pfd{};
pfd.useCustomDescriptor = true;
pfd.colorBits = 32;
pfd.redBits = 8;
pfd.greenBits = 8;
pfd.blueBits = 8;
pfd.alphaBits = 8;
pfd.depthBits = 24;
pfd.stencilBits = 8;
nkentseu::NkContextSetWin32PixelFormat(pfd);
```

## 5) Create window + context + load GL

```cpp
nkentseu::NkWindowConfig wc{};
wc.title = "OpenGL App";
wc.width = 1280;
wc.height = 720;
nkentseu::NkContextApplyWindowHints(wc); // GLX path (Xlib/XCB)

nkentseu::NkWindow window;
window.Create(wc);

nkentseu::NkContext ctx{};
nkentseu::NkContextConfig cfg = nkentseu::NkContextGetHints();
nkentseu::NkContextCreate(window, ctx, &cfg);
nkentseu::NkContextMakeCurrent(ctx);

// GLAD:
// gladLoadGLLoader((GLADloadproc)nkentseu::NkContextGetProcAddressLoader(ctx));
```

## 6) Render loop skeleton

```cpp
while (window.IsOpen()) {
    nkentseu::NkEvent* ev = nullptr;
    while ((ev = nkentseu::NkEvents().PollEvent()) != nullptr) {
        if (ev->Is<nkentseu::NkWindowCloseEvent>()) {
            window.Close();
        }
    }

    // OpenGL draw calls...
    nkentseu::NkContextSwapBuffers(ctx);
}

nkentseu::NkContextDestroy(ctx);
nkentseu::NkContextShutdown();
```

## 7) Platform notes

- Xlib/XCB OpenGL uses GLX and consumes pre-window hints via `NkContextApplyWindowHints`.
- Wayland is EGL-oriented (no GLX visual hints).
- Current `NkContext` OpenGL creation path is implemented for Win32 + Xlib/XCB.
  Wayland OpenGL/EGL path is still TODO.
- Vulkan/DX11/DX12/Metal/Software do not require OpenGL context hints.

## 8) Ready-to-run examples in this repo

- Combined selectable backend: `Applications/Sandbox/src/main9.cpp`
- OpenGL only: `Applications/Sandbox/src/main10.cpp`
- Vulkan only: `Applications/Sandbox/src/main11.cpp`
- DirectX11 only: `Applications/Sandbox/src/main12.cpp`
- DirectX12 only: `Applications/Sandbox/src/main13.cpp`
- Metal only: `Applications/Sandbox/src/main14.cpp`
- Software only: `Applications/Sandbox/src/main15.cpp`
