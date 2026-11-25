# nkentseu::platform::PlatformConfig

Platform configuration structure (compile time)

`#include "NKPlatform/NkPlatformConfig.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### platformName

```cpp
const char *platformName
```

Readable name of the target platform

### archName

```cpp
const char *archName
```

Readable name of the CPU architecture

### compilerName

```cpp
const char *compilerName
```

Name of the compiler used

### compilerVersion

```cpp
int compilerVersion
```

Numeric compiler version

### isDebugBuild

```cpp
bool isDebugBuild
```

Tells whether the build is in debug mode

### isReleaseBuild

```cpp
bool isReleaseBuild
```

Tells whether the build is in release mode

### is64Bit

```cpp
bool is64Bit
```

Tells whether the target architecture is 64-bit

### isLittleEndian

```cpp
bool isLittleEndian
```

Tells whether the system is little-endian

### hasUnicode

```cpp
bool hasUnicode
```

Unicode support (wchar_t, UTF-16/UTF-8 APIs)

### hasThreading

```cpp
bool hasThreading
```

Multithreading support (C++11+, pthreads, Windows threads)

### hasFilesystem

```cpp
bool hasFilesystem
```

Support for the <filesystem> library (C++17)

### hasNetwork

```cpp
bool hasNetwork
```

Network API support (sockets, etc.)

### maxPathLength

```cpp
int maxPathLength
```

Maximum length of a file path

### cacheLineSize

```cpp
int cacheLineSize
```

Size of a CPU cache line in bytes

### PlatformConfig

```cpp
PlatformConfig()
```

Default constructor (initialised from the macros)
