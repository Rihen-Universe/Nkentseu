cmake_minimum_required(VERSION 3.20)
project(NkGraphics CXX)

# ─────────────────────────────────────────────────────────────────────────────
# Options
# ─────────────────────────────────────────────────────────────────────────────
option(NK_USE_GLAD2          "Use GLAD2 as GL/EGL/VK loader"         ON)
option(NK_ENABLE_VULKAN      "Enable Vulkan backend"                  ON)
option(NK_ENABLE_DIRECTX     "Enable DirectX 11/12 (Windows only)"   ON)
option(NK_ENABLE_METAL       "Enable Metal (macOS/iOS only)"          ON)
option(NK_ENABLE_SOFTWARE    "Enable Software renderer"               ON)
option(NK_ENABLE_COMPUTE     "Enable Compute backends"                ON)
option(NK_BUILD_DEMO         "Build NkGraphicsDemos"                  ON)
option(NK_DEBUG_CONTEXT      "Enable API debug/validation layers"     OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─────────────────────────────────────────────────────────────────────────────
# Détection plateforme
# ─────────────────────────────────────────────────────────────────────────────
if(WIN32)
    add_compile_definitions(NKENTSEU_PLATFORM_WINDOWS)
elseif(APPLE)
    if(IOS)
        add_compile_definitions(NKENTSEU_PLATFORM_IOS)
    else()
        add_compile_definitions(NKENTSEU_PLATFORM_MACOS)
    endif()
elseif(ANDROID)
    add_compile_definitions(NKENTSEU_PLATFORM_ANDROID)
elseif(EMSCRIPTEN)
    add_compile_definitions(NKENTSEU_PLATFORM_EMSCRIPTEN)
else()
    # Linux — choisir windowing system
    # Pour XLib (défaut) :
    add_compile_definitions(NKENTSEU_WINDOWING_XLIB)
    # Pour XCB : add_compile_definitions(NKENTSEU_WINDOWING_XCB)
    # Pour Wayland : add_compile_definitions(NKENTSEU_WINDOWING_WAYLAND)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Sources Core
# ─────────────────────────────────────────────────────────────────────────────
set(NK_CORE_SOURCES
    Core/NkTypes.h
    Core/NkSurfaceDesc.h
    Core/NkGraphicsApi.h
    Core/NkWGLPixelFormat.h
    Core/NkOpenGLDesc.h
    Core/NkContextDesc.h
    Core/NkContextInfo.h
    Core/NkIGraphicsContext.h
    Core/NkNativeContextAccess.h
)

set(NK_OPENGL_SOURCES
    OpenGL/NkOpenGLContextData.h
    OpenGL/NkOpenGLContext.h
    OpenGL/NkOpenGLContext.cpp
)
if(APPLE AND NOT IOS)
    list(APPEND NK_OPENGL_SOURCES OpenGL/NkOpenGLContext_macOS.mm)
endif()

set(NK_VULKAN_SOURCES
    Vulkan/NkVulkanContextData.h
    Vulkan/NkVulkanContext.h
    Vulkan/NkVulkanContext.cpp
)

set(NK_SOFTWARE_SOURCES
    Software/NkSoftwareContext.h
    Software/NkSoftwareContext.cpp
)

set(NK_FACTORY_SOURCES
    Factory/NkContextFactory.h
    Factory/NkContextFactory.cpp
)

set(NK_COMPUTE_SOURCES
    Compute/NkIComputeContext.h
    Compute/NkOpenGLComputeContext.h
    Compute/NkOpenGLComputeContext.cpp
    Compute/NkVulkanComputeContext.h
    Compute/NkVulkanComputeContext.cpp
)

# ─────────────────────────────────────────────────────────────────────────────
# Sources conditionnelles — Windows
# ─────────────────────────────────────────────────────────────────────────────
if(WIN32 AND NK_ENABLE_DIRECTX)
    list(APPEND NK_FACTORY_SOURCES
        DirectX/NkDirectXContextData.h
        DirectX/NkDXContext.h
        DirectX/NkDX11Context.cpp
        DirectX/NkDX12Context.cpp
    )
    if(NK_ENABLE_COMPUTE)
        list(APPEND NK_COMPUTE_SOURCES
            Compute/NkDX11ComputeContext.h
            Compute/NkDX11ComputeContext.cpp
            Compute/NkDX12ComputeContext.h
            Compute/NkDX12ComputeContext.cpp
        )
    endif()
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Sources conditionnelles — macOS / iOS
# ─────────────────────────────────────────────────────────────────────────────
if(APPLE AND NK_ENABLE_METAL)
    list(APPEND NK_FACTORY_SOURCES
        Metal/NkMetalContext.h
        Metal/NkMetalContext.mm
    )
    if(NK_ENABLE_COMPUTE)
        list(APPEND NK_COMPUTE_SOURCES
            Compute/NkMetalComputeContext.h
            Compute/NkMetalComputeContext.mm
        )
    endif()
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Fichier d'entrée Apple (remplace main.cpp sur macOS/iOS)
# Doit être la SEULE unité de compilation contenant main()
# ─────────────────────────────────────────────────────────────────────────────
if(APPLE)
    set(NK_ENTRY_SOURCE Metal/NkMetalEntryPoint.mm)
else()
    # Sur Windows/Linux, main() est dans votre propre fichier ou dans NkEngine
    set(NK_ENTRY_SOURCE "")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Cible principale
# ─────────────────────────────────────────────────────────────────────────────
add_library(NkGraphics STATIC
    ${NK_CORE_SOURCES}
    ${NK_OPENGL_SOURCES}
    ${NK_SOFTWARE_SOURCES}
    ${NK_FACTORY_SOURCES}
    ${NK_COMPUTE_SOURCES}
)
if(NK_ENABLE_VULKAN)
    target_sources(NkGraphics PRIVATE ${NK_VULKAN_SOURCES})
endif()

target_include_directories(NkGraphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ─────────────────────────────────────────────────────────────────────────────
# Propriétés Objective-C++ pour les fichiers .mm
# ─────────────────────────────────────────────────────────────────────────────
if(APPLE)
    set_source_files_properties(
        OpenGL/NkOpenGLContext_macOS.mm
        Metal/NkMetalContext.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++"
    )
    # Compute Metal si ajouté
    # set_source_files_properties(Compute/NkMetalComputeContext.mm
    #     PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# GLAD2
# ─────────────────────────────────────────────────────────────────────────────
if(NK_USE_GLAD2)
    # Suppose que GLAD2 est dans extern/glad2
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../extern/glad2/CMakeLists.txt")
        add_subdirectory(../extern/glad2 glad2_build)
        target_link_libraries(NkGraphics PUBLIC glad)
        target_compile_definitions(NkGraphics PUBLIC GLAD2_INCLUDED)
    else()
        message(WARNING "GLAD2 not found at extern/glad2 — set NK_USE_GLAD2=OFF or place glad2 there")
        target_compile_definitions(NkGraphics PUBLIC NK_NO_GLAD2)
    endif()
else()
    target_compile_definitions(NkGraphics PUBLIC NK_NO_GLAD2)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Vulkan SDK
# ─────────────────────────────────────────────────────────────────────────────
if(NK_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_link_libraries(NkGraphics PUBLIC Vulkan::Vulkan)
        target_compile_definitions(NkGraphics PUBLIC NK_VULKAN_AVAILABLE)
        message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")
    else()
        message(WARNING "Vulkan SDK not found — Vulkan backend disabled")
    endif()
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Liens plateforme
# ─────────────────────────────────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(NkGraphics PUBLIC
        opengl32 gdi32 user32
    )
    if(NK_ENABLE_DIRECTX)
        target_link_libraries(NkGraphics PUBLIC
            d3d11 d3d12 dxgi dxguid d3dcompiler
        )
    endif()

elseif(APPLE AND NOT IOS)
    target_link_libraries(NkGraphics PUBLIC
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework AppKit"
    )

elseif(APPLE AND IOS)
    target_link_libraries(NkGraphics PUBLIC
        "-framework UIKit"
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework OpenGLES"
    )

elseif(ANDROID)
    target_link_libraries(NkGraphics PUBLIC
        android EGL GLESv3 log
    )

elseif(EMSCRIPTEN)
    target_link_options(NkGraphics PUBLIC
        "-s USE_WEBGL2=1"
        "-s FULL_ES3=1"
        "-s ALLOW_MEMORY_GROWTH=1"
    )

else()  # Linux
    find_package(X11 REQUIRED)
    target_link_libraries(NkGraphics PUBLIC
        ${X11_LIBRARIES} GL
    )
    # XCB si nécessaire
    # target_link_libraries(NkGraphics PUBLIC xcb xcb-image)
    # Wayland si nécessaire
    # target_link_libraries(NkGraphics PUBLIC wayland-client)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Debug compile definitions
# ─────────────────────────────────────────────────────────────────────────────
if(NK_DEBUG_CONTEXT)
    target_compile_definitions(NkGraphics PUBLIC
        NK_DEBUG_CONTEXT=1
        NK_VK_VALIDATION=1
        NK_DX_DEBUG=1
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Demo
# ─────────────────────────────────────────────────────────────────────────────
if(NK_BUILD_DEMO)
    # Sur Apple: NkMetalEntryPoint.mm fournit le main() (NSApplication/UIApplication)
    # Sur Windows/Linux: fournir un main.cpp séparé ou laisser NkEngine le gérer
    if(APPLE)
        add_executable(NkGraphicsDemo
            Demo/NkGraphicsDemos.cpp
            Metal/NkMetalEntryPoint.mm
        )
        set_source_files_properties(Metal/NkMetalEntryPoint.mm
            PROPERTIES COMPILE_FLAGS "-x objective-c++")
        target_link_libraries(NkGraphicsDemo PRIVATE
            NkGraphics
            "-framework Cocoa"
            "-framework Metal"
            "-framework QuartzCore"
        )
    else()
        add_executable(NkGraphicsDemo Demo/NkGraphicsDemos.cpp)
        target_link_libraries(NkGraphicsDemo PRIVATE NkGraphics)
    endif()
    target_include_directories(NkGraphicsDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    # Définir NK_REAL_NKWINDOW si vous utilisez le vrai NkWindow de NkEngine
    # target_compile_definitions(NkGraphicsDemo PRIVATE NK_REAL_NKWINDOW)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Message de résumé
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "=== NkGraphics Configuration ===")
message(STATUS "  GLAD2         : ${NK_USE_GLAD2}")
message(STATUS "  Vulkan        : ${NK_ENABLE_VULKAN}")
message(STATUS "  DirectX       : ${NK_ENABLE_DIRECTX}")
message(STATUS "  Metal         : ${NK_ENABLE_METAL}")
message(STATUS "  Software      : ${NK_ENABLE_SOFTWARE}")
message(STATUS "  Compute       : ${NK_ENABLE_COMPUTE}")
message(STATUS "  Demo          : ${NK_BUILD_DEMO}")
message(STATUS "  Debug context : ${NK_DEBUG_CONTEXT}")
message(STATUS "================================")
