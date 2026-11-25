# NkPlatformConfig.h

`#include "NKPlatform/NkPlatformConfig.h"`

## Types

- [nkentseu::platform::PlatformConfig](../nkentseu-platform-platformconfig/index.md)
- [nkentseu::platform::PlatformCapabilities](../nkentseu-platform-platformcapabilities/index.md)

## Fonctions

### GetPlatformConfig

```cpp
const PlatformConfig &GetPlatformConfig()
```

Obtenir l'instance singleton de PlatformConfig

### GetPlatformCapabilities

```cpp
const PlatformCapabilities &GetPlatformCapabilities()
```

Obtenir l'instance singleton de PlatformCapabilities

### GetPlatformName

```cpp
const char *GetPlatformName()
```

Obtenir le nom de la plateforme sous forme de chaîne

### GetArchName

```cpp
const char *GetArchName()
```

Obtenir le nom de l'architecture sous forme de chaîne

### GetCompilerName

```cpp
const char *GetCompilerName()
```

Obtenir le nom du compilateur sous forme de chaîne

### Is64Bit

```cpp
bool Is64Bit()
```

Vérifier si l'architecture cible est 64-bit

### IsLittleEndian

```cpp
bool IsLittleEndian()
```

Vérifier si le système utilise le little-endian

## Macros

### NKENTSEU_PATH_SEPARATOR

```cpp
#define NKENTSEU_PATH_SEPARATOR
```

Séparateur de chemin pour Windows (caractère)

### NKENTSEU_PATH_SEPARATOR_STR

```cpp
#define NKENTSEU_PATH_SEPARATOR_STR
```

Séparateur de chemin pour Windows (chaîne)

### NKENTSEU_LINE_ENDING

```cpp
#define NKENTSEU_LINE_ENDING
```

Séquence de fin de ligne pour Windows

### NKENTSEU_MAX_PATH

```cpp
#define NKENTSEU_MAX_PATH
```

Longueur maximale de chemin pour Windows (MAX_PATH)

### NKENTSEU_DYNAMIC_LIB_EXT

```cpp
#define NKENTSEU_DYNAMIC_LIB_EXT
```

Extension des bibliothèques dynamiques Windows

### NKENTSEU_STATIC_LIB_EXT

```cpp
#define NKENTSEU_STATIC_LIB_EXT
```

Extension des bibliothèques statiques Windows

### NKENTSEU_EXECUTABLE_EXT

```cpp
#define NKENTSEU_EXECUTABLE_EXT
```

Extension des exécutables Windows

### NKENTSEU_HAS_UNICODE

```cpp
#define NKENTSEU_HAS_UNICODE
```

Indicateur de support Unicode

### NKENTSEU_HAS_THREADING

```cpp
#define NKENTSEU_HAS_THREADING
```

Indicateur de support du multithreading

### NKENTSEU_HAS_FILESYSTEM

```cpp
#define NKENTSEU_HAS_FILESYSTEM
```

Indicateur de support de <filesystem>

### NKENTSEU_HAS_NETWORK

```cpp
#define NKENTSEU_HAS_NETWORK
```

Indicateur de support des APIs réseau

### NKENTSEU_DEBUG_BUILD

```cpp
#define NKENTSEU_DEBUG_BUILD
```

Indicateur de build debug

### NKENTSEU_RELEASE_BUILD

```cpp
#define NKENTSEU_RELEASE_BUILD
```

Vaut 1 en construction Release (NDEBUG sans _DEBUG ni DEBUG), 0 sinon.

### NKENTSEU_OPTIMIZED_BUILD

```cpp
#define NKENTSEU_OPTIMIZED_BUILD
```

Indicateur de build optimisé

### NKENTSEU_CACHE_ALIGNED

```cpp
#define NKENTSEU_CACHE_ALIGNED
```

Alignement sur ligne de cache CPU

### NKENTSEU_SIMD_ALIGNMENT

```cpp
#define NKENTSEU_SIMD_ALIGNMENT
```

Alignement requis pour les instructions SIMD

### NKENTSEU_SIMD_ALIGNED

```cpp
#define NKENTSEU_SIMD_ALIGNED
```

Alignement pour les données SIMD

