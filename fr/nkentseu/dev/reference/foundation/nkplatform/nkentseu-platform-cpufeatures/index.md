# nkentseu::platform::CPUFeatures

Structure complète des fonctionnalités CPU détectées

`#include "NKPlatform/NkCPUFeatures.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### VENDOR_CAPACITY

```cpp
static constexpr size_t VENDOR_CAPACITY = 32u
```

Capacité maximale pour la chaîne vendor

### BRAND_CAPACITY

```cpp
static constexpr size_t BRAND_CAPACITY = 128u
```

Capacité maximale pour la chaîne brand

### vendor

```cpp
char vendor[VENDOR_CAPACITY]
```

Chaîne d'identification du vendor

### brand

```cpp
char brand[BRAND_CAPACITY]
```

Nom complet du processeur (brand string)

### family

```cpp
int32_t family
```

Numéro de famille du CPU (CPUID)

### model

```cpp
int32_t model
```

Numéro de modèle du CPU (CPUID)

### stepping

```cpp
int32_t stepping
```

Numéro de stepping du CPU (CPUID)

### topology

```cpp
CPUTopology topology
```

Informations de topologie

### cache

```cpp
CacheInfo cache
```

Informations de cache

### simd

```cpp
SIMDFeatures simd
```

Fonctionnalités SIMD

### extended

```cpp
ExtendedFeatures extended
```

Fonctionnalités étendues

### baseFrequency

```cpp
int32_t baseFrequency
```

Fréquence de base en MHz

### maxFrequency

```cpp
int32_t maxFrequency
```

Fréquence maximale en MHz (Turbo)

### Get

```cpp
static const CPUFeatures &Get()
```

Obtenir l'instance singleton (initialisation paresseuse thread-safe)

### ToString

```cpp
void ToString(char *outBuffer, size_t outBufferSize) const
```

Formater les informations CPU dans un buffer fourni par l'appelant

### ToString

```cpp
const char *ToString() const
```

Obtenir une représentation chaîne pratique (buffer statique interne)
