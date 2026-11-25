# nkentseu::platform::CPUTopology

Informations sur la topologie du processeur

`#include "NKPlatform/NkCPUFeatures.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### numPhysicalCores

```cpp
int32_t numPhysicalCores
```

Nombre de cœurs physiques réels

### numLogicalCores

```cpp
int32_t numLogicalCores
```

Nombre de cœurs logiques (avec HT/SMT activé)

### numSockets

```cpp
int32_t numSockets
```

Nombre de sockets CPU dans le système

### hasHyperThreading

```cpp
bool hasHyperThreading
```

Indique si l'Hyper-Threading/SMT est activé

### CPUTopology

```cpp
CPUTopology()
```

Constructeur par défaut avec valeurs minimales
