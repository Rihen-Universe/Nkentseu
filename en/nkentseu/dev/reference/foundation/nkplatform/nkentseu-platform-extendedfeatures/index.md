# nkentseu::platform::ExtendedFeatures

Additional CPU features (security, memory, performance)

`#include "NKPlatform/NkCPUFeatures.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### hasAES

```cpp
bool hasAES
```

AES-NI support (hardware AES encryption)

### hasSHA

```cpp
bool hasSHA
```

Hardware SHA extensions support

### hasRDRAND

```cpp
bool hasRDRAND
```

RDRAND support (hardware random generator)

### hasRDSEED

```cpp
bool hasRDSEED
```

RDSEED support (seed for a PRNG)

### hasCLFLUSH

```cpp
bool hasCLFLUSH
```

CLFLUSH support (cache line flush)

### hasCLFLUSHOPT

```cpp
bool hasCLFLUSHOPT
```

CLFLUSHOPT support (optimised version)

### hasPREFETCHWT1

```cpp
bool hasPREFETCHWT1
```

PREFETCHWT1 support (prefetch into L2)

### hasMOVBE

```cpp
bool hasMOVBE
```

MOVBE support (move with byte swap)

### hasPOPCNT

```cpp
bool hasPOPCNT
```

POPCNT support (count of set bits)

### hasLZCNT

```cpp
bool hasLZCNT
```

LZCNT support (count of leading zeros)

### hasBMI1

```cpp
bool hasBMI1
```

BMI1 (Bit Manipulation Instruction Set 1) support

### hasBMI2

```cpp
bool hasBMI2
```

BMI2 (Bit Manipulation Instruction Set 2) support

### hasADX

```cpp
bool hasADX
```

ADX support (multi-precision add with carry)

### hasVMX

```cpp
bool hasVMX
```

VMX support (Intel VT-x)

### hasSVM

```cpp
bool hasSVM
```

SVM support (AMD-V)

### ExtendedFeatures

```cpp
ExtendedFeatures()
```

Default constructor with every feature disabled
