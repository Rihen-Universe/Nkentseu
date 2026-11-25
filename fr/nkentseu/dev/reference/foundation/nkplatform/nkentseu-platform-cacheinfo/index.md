# nkentseu::platform::CacheInfo

Informations sur la hiérarchie de cache du CPU

`#include "NKPlatform/NkCPUFeatures.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### lineSize

```cpp
int32_t lineSize
```

Taille d'une ligne de cache en octets

### l1DataSize

```cpp
int32_t l1DataSize
```

Taille du cache L1 données en KB

### l1InstructionSize

```cpp
int32_t l1InstructionSize
```

Taille du cache L1 instructions en KB

### l2Size

```cpp
int32_t l2Size
```

Taille du cache L2 en KB

### l3Size

```cpp
int32_t l3Size
```

Taille du cache L3 en KB

### CacheInfo

```cpp
CacheInfo()
```

Constructeur par défaut avec valeurs conservatrices
