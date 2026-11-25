# nkentseu::platform::CPUFeatures

Complete structure of the detected CPU features

`#include "NKPlatform/NkCPUFeatures.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### VENDOR_CAPACITY

```cpp
static constexpr size_t VENDOR_CAPACITY = 32u
```

Maximum capacity of the vendor string

### BRAND_CAPACITY

```cpp
static constexpr size_t BRAND_CAPACITY = 128u
```

Maximum capacity of the brand string

### vendor

```cpp
char vendor[VENDOR_CAPACITY]
```

Vendor identification string

### brand

```cpp
char brand[BRAND_CAPACITY]
```

Full processor name (brand string)

### family

```cpp
int32_t family
```

CPU family number (CPUID)

### model

```cpp
int32_t model
```

CPU model number (CPUID)

### stepping

```cpp
int32_t stepping
```

CPU stepping number (CPUID)

### topology

```cpp
CPUTopology topology
```

Topology information

### cache

```cpp
CacheInfo cache
```

Cache information

### simd

```cpp
SIMDFeatures simd
```

SIMD features

### extended

```cpp
ExtendedFeatures extended
```

Extended features

### baseFrequency

```cpp
int32_t baseFrequency
```

Base frequency in MHz

### maxFrequency

```cpp
int32_t maxFrequency
```

Maximum frequency in MHz (Turbo)

### Get

```cpp
static const CPUFeatures &Get()
```

Gets the singleton instance (lazy, thread-safe initialisation)

### ToString

```cpp
void ToString(char *outBuffer, size_t outBufferSize) const
```

Formats the CPU information into a buffer provided by the caller

### ToString

```cpp
const char *ToString() const
```

Gets a convenient string representation (internal static buffer)
