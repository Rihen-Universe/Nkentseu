# nkentseu::platform::PlatformCapabilities

Structure of the platform hardware capabilities (run time)

`#include "NKPlatform/NkPlatformConfig.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### totalPhysicalMemory

```cpp
size_t totalPhysicalMemory
```

Total physical memory in bytes

### availablePhysicalMemory

```cpp
size_t availablePhysicalMemory
```

Available physical memory in bytes

### pageSize

```cpp
size_t pageSize
```

Size of a system memory page in bytes

### processorCount

```cpp
int processorCount
```

Number of physical cores detected

### logicalProcessorCount

```cpp
int logicalProcessorCount
```

Number of logical cores (with HT/SMT)

### hasDisplay

```cpp
bool hasDisplay
```

Tells whether a graphical display is available

### primaryScreenWidth

```cpp
int primaryScreenWidth
```

Width of the main screen in pixels

### primaryScreenHeight

```cpp
int primaryScreenHeight
```

Height of the main screen in pixels

### hasSSE

```cpp
bool hasSSE
```

SSE support (x86/x64)

### hasSSE2

```cpp
bool hasSSE2
```

SSE2 support (x86/x64)

### hasAVX

```cpp
bool hasAVX
```

AVX support (x86/x64)

### hasAVX2

```cpp
bool hasAVX2
```

AVX2 support (x86/x64)

### hasNEON

```cpp
bool hasNEON
```

NEON support (ARM)

### PlatformCapabilities

```cpp
PlatformCapabilities()
```

Default constructor (detection through system APIs)
