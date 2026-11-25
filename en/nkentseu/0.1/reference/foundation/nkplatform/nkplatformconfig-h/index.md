# NkPlatformConfig.h

`#include "NKPlatform/NkPlatformConfig.h"`

## Types

- [nkentseu::platform::PlatformConfig](../nkentseu-platform-platformconfig/index.md)
- [nkentseu::platform::PlatformCapabilities](../nkentseu-platform-platformcapabilities/index.md)

## Functions

### GetPlatformConfig

```cpp
const PlatformConfig &GetPlatformConfig()
```

Gets the PlatformConfig singleton instance

### GetPlatformCapabilities

```cpp
const PlatformCapabilities &GetPlatformCapabilities()
```

Gets the PlatformCapabilities singleton instance

### GetPlatformName

```cpp
const char *GetPlatformName()
```

Gets the platform name as a string

### GetArchName

```cpp
const char *GetArchName()
```

Gets the architecture name as a string

### GetCompilerName

```cpp
const char *GetCompilerName()
```

Gets the compiler name as a string

### Is64Bit

```cpp
bool Is64Bit()
```

Checks whether the target architecture is 64-bit

### IsLittleEndian

```cpp
bool IsLittleEndian()
```

Checks whether the system is little-endian

## Macros

### NKENTSEU_PATH_SEPARATOR

```cpp
#define NKENTSEU_PATH_SEPARATOR
```

Path separator on Windows (character)

### NKENTSEU_PATH_SEPARATOR_STR

```cpp
#define NKENTSEU_PATH_SEPARATOR_STR
```

Path separator on Windows (string)

### NKENTSEU_LINE_ENDING

```cpp
#define NKENTSEU_LINE_ENDING
```

End-of-line sequence on Windows

### NKENTSEU_MAX_PATH

```cpp
#define NKENTSEU_MAX_PATH
```

Maximum path length on Windows (MAX_PATH)

### NKENTSEU_DYNAMIC_LIB_EXT

```cpp
#define NKENTSEU_DYNAMIC_LIB_EXT
```

Windows dynamic library extension

### NKENTSEU_STATIC_LIB_EXT

```cpp
#define NKENTSEU_STATIC_LIB_EXT
```

Windows static library extension

### NKENTSEU_EXECUTABLE_EXT

```cpp
#define NKENTSEU_EXECUTABLE_EXT
```

Windows executable extension

### NKENTSEU_HAS_UNICODE

```cpp
#define NKENTSEU_HAS_UNICODE
```

Unicode support flag

### NKENTSEU_HAS_THREADING

```cpp
#define NKENTSEU_HAS_THREADING
```

Multithreading support flag

### NKENTSEU_HAS_FILESYSTEM

```cpp
#define NKENTSEU_HAS_FILESYSTEM
```

<filesystem> support flag

### NKENTSEU_HAS_NETWORK

```cpp
#define NKENTSEU_HAS_NETWORK
```

Network API support flag

### NKENTSEU_DEBUG_BUILD

```cpp
#define NKENTSEU_DEBUG_BUILD
```

Debug build flag

### NKENTSEU_RELEASE_BUILD

```cpp
#define NKENTSEU_RELEASE_BUILD
```



### NKENTSEU_OPTIMIZED_BUILD

```cpp
#define NKENTSEU_OPTIMIZED_BUILD
```

Optimised build flag

### NKENTSEU_CACHE_ALIGNED

```cpp
#define NKENTSEU_CACHE_ALIGNED
```

Alignment on a CPU cache line

### NKENTSEU_SIMD_ALIGNMENT

```cpp
#define NKENTSEU_SIMD_ALIGNMENT
```

Alignment required by SIMD instructions

### NKENTSEU_SIMD_ALIGNED

```cpp
#define NKENTSEU_SIMD_ALIGNED
```

Alignment for SIMD data

