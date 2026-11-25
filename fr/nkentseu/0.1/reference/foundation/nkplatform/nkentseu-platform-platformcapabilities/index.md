# nkentseu::platform::PlatformCapabilities

Structure des capacités matérielles de la plateforme (runtime)

`#include "NKPlatform/NkPlatformConfig.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### totalPhysicalMemory

```cpp
size_t totalPhysicalMemory
```

Mémoire physique totale en octets

### availablePhysicalMemory

```cpp
size_t availablePhysicalMemory
```

Mémoire physique disponible en octets

### pageSize

```cpp
size_t pageSize
```

Taille d'une page mémoire système en octets

### processorCount

```cpp
int processorCount
```

Nombre de cœurs physiques détectés

### logicalProcessorCount

```cpp
int logicalProcessorCount
```

Nombre de cœurs logiques (avec HT/SMT)

### hasDisplay

```cpp
bool hasDisplay
```

Indique si un affichage graphique est disponible

### primaryScreenWidth

```cpp
int primaryScreenWidth
```

Largeur de l'écran principal en pixels

### primaryScreenHeight

```cpp
int primaryScreenHeight
```

Hauteur de l'écran principal en pixels

### hasSSE

```cpp
bool hasSSE
```

Support de SSE (x86/x64)

### hasSSE2

```cpp
bool hasSSE2
```

Support de SSE2 (x86/x64)

### hasAVX

```cpp
bool hasAVX
```

Support de AVX (x86/x64)

### hasAVX2

```cpp
bool hasAVX2
```

Support de AVX2 (x86/x64)

### hasNEON

```cpp
bool hasNEON
```

Support de NEON (ARM)

### PlatformCapabilities

```cpp
PlatformCapabilities()
```

Constructeur par défaut (détection via APIs système)
