# nkentseu::platform::CacheInfo

Information about the CPU cache hierarchy

`#include "NKPlatform/NkCPUFeatures.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### lineSize

```cpp
int32_t lineSize
```

Size of a cache line in bytes

### l1DataSize

```cpp
int32_t l1DataSize
```

L1 data cache size in KB

### l1InstructionSize

```cpp
int32_t l1InstructionSize
```

L1 instruction cache size in KB

### l2Size

```cpp
int32_t l2Size
```

L2 cache size in KB

### l3Size

```cpp
int32_t l3Size
```

L3 cache size in KB

### CacheInfo

```cpp
CacheInfo()
```

Default constructor with conservative values
