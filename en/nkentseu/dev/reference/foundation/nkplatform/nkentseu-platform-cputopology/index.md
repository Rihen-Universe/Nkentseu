# nkentseu::platform::CPUTopology

Information about the processor topology

`#include "NKPlatform/NkCPUFeatures.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### numPhysicalCores

```cpp
int32_t numPhysicalCores
```

Number of real physical cores

### numLogicalCores

```cpp
int32_t numLogicalCores
```

Number of logical cores (with HT/SMT enabled)

### numSockets

```cpp
int32_t numSockets
```

Number of CPU sockets in the system

### hasHyperThreading

```cpp
bool hasHyperThreading
```

Tells whether Hyper-Threading/SMT is enabled

### CPUTopology

```cpp
CPUTopology()
```

Default constructor with minimal values
