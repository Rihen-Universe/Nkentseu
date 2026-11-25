# nkentseu::platform::PlatformConfig

Structure de configuration de plateforme (compile-time)

`#include "NKPlatform/NkPlatformConfig.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### platformName

```cpp
const char *platformName
```

Nom lisible de la plateforme cible

### archName

```cpp
const char *archName
```

Nom lisible de l'architecture CPU

### compilerName

```cpp
const char *compilerName
```

Nom du compilateur utilisé

### compilerVersion

```cpp
int compilerVersion
```

Version numérique du compilateur

### isDebugBuild

```cpp
bool isDebugBuild
```

Indique si le build est en mode debug

### isReleaseBuild

```cpp
bool isReleaseBuild
```

Indique si le build est en mode release

### is64Bit

```cpp
bool is64Bit
```

Indique si l'architecture cible est 64-bit

### isLittleEndian

```cpp
bool isLittleEndian
```

Indique si le système utilise le little-endian

### hasUnicode

```cpp
bool hasUnicode
```

Support de l'Unicode (wchar_t, APIs UTF-16/UTF-8)

### hasThreading

```cpp
bool hasThreading
```

Support du multithreading (C++11+, pthreads, Windows threads)

### hasFilesystem

```cpp
bool hasFilesystem
```

Support de la bibliothèque <filesystem> (C++17)

### hasNetwork

```cpp
bool hasNetwork
```

Support des APIs réseau (sockets, etc.)

### maxPathLength

```cpp
int maxPathLength
```

Longueur maximale d'un chemin de fichier

### cacheLineSize

```cpp
int cacheLineSize
```

Taille d'une ligne de cache CPU en octets

### PlatformConfig

```cpp
PlatformConfig()
```

Constructeur par défaut (initialisation via macros)
