cmake_minimum_required(VERSION 3.18)
project(CollisionLib VERSION 1.0.0 LANGUAGES CXX)

# ─────────────────────────────────────────────────────────────────────────────
#  C++ standard and global settings
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────────────────────────────────────
#  Options
# ─────────────────────────────────────────────────────────────────────────────
option(COL_ENABLE_CUDA    "Enable CUDA GPU backend"           OFF)
option(COL_ENABLE_OPENGL  "Enable OpenGL Compute GPU backend" OFF)
option(COL_ENABLE_VULKAN  "Enable Vulkan Compute GPU backend" OFF)
option(COL_ENABLE_DX11    "Enable DirectX 11 GPU backend"     OFF)
option(COL_ENABLE_DX12    "Enable DirectX 12 GPU backend"     OFF)
option(COL_ENABLE_WEBGPU  "Enable WebGPU backend (via Dawn)"  OFF)
option(COL_BUILD_TESTS    "Build unit tests"                  ON)
option(COL_BUILD_EXAMPLES "Build examples"                    ON)
option(COL_SIMD           "Enable SIMD (SSE4.2/AVX2)"         ON)

# ─────────────────────────────────────────────────────────────────────────────
#  Core library (header-only + small cpp)
# ─────────────────────────────────────────────────────────────────────────────
add_library(collision STATIC
    src/shapes.cpp
)

target_include_directories(collision PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Compiler flags
if(MSVC)
    target_compile_options(collision PUBLIC /W4 /O2 /arch:AVX2)
else()
    target_compile_options(collision PUBLIC
        -Wall -Wextra -O3 -ffast-math
        $<$<BOOL:${COL_SIMD}>:-msse4.2 -mavx2 -mfma>
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  CUDA backend
# ─────────────────────────────────────────────────────────────────────────────
if(COL_ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    target_sources(collision PRIVATE
        shaders/cuda/collision_broadphase.cu
    )
    target_compile_definitions(collision PUBLIC COL_ENABLE_CUDA)
    target_link_libraries(collision PUBLIC CUDA::cudart)
    set_target_properties(collision PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_ARCHITECTURES "75;80;86;89;90"  # Turing, Ampere, Ada
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  OpenGL Compute backend
# ─────────────────────────────────────────────────────────────────────────────
if(COL_ENABLE_OPENGL)
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    target_sources(collision PRIVATE
        src/gpu/opengl_backend.cpp
    )
    target_compile_definitions(collision PUBLIC COL_ENABLE_OPENGL)
    target_link_libraries(collision PUBLIC OpenGL::GL GLEW::GLEW)
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Vulkan backend
# ─────────────────────────────────────────────────────────────────────────────
if(COL_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
    target_sources(collision PRIVATE
        src/gpu/vulkan_backend.cpp
    )
    target_compile_definitions(collision PUBLIC COL_ENABLE_VULKAN)
    target_link_libraries(collision PUBLIC Vulkan::Vulkan)
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  DirectX backends (Windows only)
# ─────────────────────────────────────────────────────────────────────────────
if(COL_ENABLE_DX11 AND WIN32)
    target_sources(collision PRIVATE src/gpu/dx11_backend.cpp)
    target_compile_definitions(collision PUBLIC COL_ENABLE_DX11)
    target_link_libraries(collision PUBLIC d3d11 d3dcompiler dxgi)
endif()

if(COL_ENABLE_DX12 AND WIN32)
    target_sources(collision PRIVATE src/gpu/dx12_backend.cpp)
    target_compile_definitions(collision PUBLIC COL_ENABLE_DX12)
    target_link_libraries(collision PUBLIC d3d12 d3dcompiler dxgi)
endif()

# Threading
find_package(Threads REQUIRED)
target_link_libraries(collision PUBLIC Threads::Threads)

# ─────────────────────────────────────────────────────────────────────────────
#  Tests
# ─────────────────────────────────────────────────────────────────────────────
if(COL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Examples
# ─────────────────────────────────────────────────────────────────────────────
if(COL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Install
# ─────────────────────────────────────────────────────────────────────────────
install(TARGETS collision EXPORT CollisionTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT CollisionTargets
    FILE CollisionConfig.cmake
    NAMESPACE col::
    DESTINATION lib/cmake/collision
)
