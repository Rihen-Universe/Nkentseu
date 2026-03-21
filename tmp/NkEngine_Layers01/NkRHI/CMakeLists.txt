cmake_minimum_required(VERSION 3.18)
project(NkRHI VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# =============================================================================
# Options
# =============================================================================
option(NK_RHI_GL     "Backend OpenGL 4.3+"   ON)
option(NK_RHI_VK     "Backend Vulkan"         OFF)
option(NK_RHI_DX11   "Backend DX11"           OFF)
option(NK_RHI_DX12   "Backend DX12"           OFF)
option(NK_RHI_METAL  "Backend Metal"          OFF)
option(NK_RHI_ML     "Module ML/Compute"      ON)
option(NK_RHI_DEMO   "Démo RHI"               ON)

# =============================================================================
# Sources communes
# =============================================================================
set(NK_RHI_CORE_SOURCES
    Core/NkRHI_Types.h
    Core/NkRHI_Descs.h
    Core/NkIDevice.h
    Core/NkDeviceFactory.h
    Core/NkDeviceFactory.cpp
    Commands/NkICommandBuffer.h
)

set(NK_RHI_SOURCES ${NK_RHI_CORE_SOURCES})

# =============================================================================
# Backend OpenGL
# =============================================================================
if(NK_RHI_GL)
    list(APPEND NK_RHI_SOURCES
        RHI_GL/NkRHI_Device_GL.h
        RHI_GL/NkRHI_Device_GL.cpp
        RHI_GL/NkRHI_Device_GL_Internal.cpp
        RHI_GL/NkCommandBuffer_GL.h
        RHI_GL/NkCommandBuffer_GL.cpp
    )
    add_compile_definitions(NK_RHI_GL_ENABLED)
endif()

# =============================================================================
# Module ML
# =============================================================================
if(NK_RHI_ML)
    list(APPEND NK_RHI_SOURCES
        Core/NkRHI_ML.h
        Core/NkRHI_ML.cpp
    )
    add_compile_definitions(NK_RHI_ML_ENABLED)
endif()

# =============================================================================
# Bibliothèque
# =============================================================================
add_library(NkRHI STATIC ${NK_RHI_SOURCES})
target_include_directories(NkRHI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # Pour accéder à NkFinal_work/
)

# Lier avec NkGraphics (couche 0)
if(TARGET NkGraphics)
    target_link_libraries(NkRHI PUBLIC NkGraphics)
endif()

# OpenGL + GLAD2
if(NK_RHI_GL)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../extern/glad2/CMakeLists.txt")
        add_subdirectory(../extern/glad2 glad2_rhi_build)
        target_link_libraries(NkRHI PUBLIC glad)
    endif()
    find_package(OpenGL REQUIRED)
    target_link_libraries(NkRHI PUBLIC OpenGL::GL)
endif()

# =============================================================================
# Démo
# =============================================================================
if(NK_RHI_DEMO)
    add_executable(NkRHIDemo Core/NkRHI_Demo.cpp)
    target_link_libraries(NkRHIDemo PRIVATE NkRHI)
    target_include_directories(NkRHIDemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# =============================================================================
# Résumé
# =============================================================================
message(STATUS "")
message(STATUS "=== NkRHI Configuration ===")
message(STATUS "  OpenGL  : ${NK_RHI_GL}")
message(STATUS "  Vulkan  : ${NK_RHI_VK}")
message(STATUS "  DX11    : ${NK_RHI_DX11}")
message(STATUS "  DX12    : ${NK_RHI_DX12}")
message(STATUS "  Metal   : ${NK_RHI_METAL}")
message(STATUS "  ML      : ${NK_RHI_ML}")
message(STATUS "  Demo    : ${NK_RHI_DEMO}")
message(STATUS "")
