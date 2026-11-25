# nkentseu::env::NkEnvUMap

Conteneur associatif non ordonné (recherche linéaire)

`#include "NKPlatform/NkEnv.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### NkEnvUMap

```cpp
NkEnvUMap() noexcept = default
```

Constructeur par défaut

### NkEnvUMap

```cpp
NkEnvUMap(const NkEnvUMap &other) = default
```

Constructeur de copie

### NkEnvUMap

```cpp
NkEnvUMap(NkEnvUMap &&other) noexcept = default
```

Constructeur de déplacement

### ~NkEnvUMap

```cpp
~NkEnvUMap() = default
```

Destructeur

### operator=

```cpp
NkEnvUMap &operator=(const NkEnvUMap &other) = default
```

Opérateur d'affectation par copie

### operator=

```cpp
NkEnvUMap &operator=(NkEnvUMap &&other) noexcept = default
```

Opérateur d'affectation par déplacement

### Set

```cpp
void Set(const K &key, const V &value)
```

Définir ou mettre à jour une paire clé/valeur (copie)

### Set

```cpp
void Set(K &&key, V &&value)
```

Définir ou mettre à jour une paire clé/valeur (déplacement)

### Find

```cpp
const V *Find(const K &key) const noexcept
```

Rechercher une valeur par clé

### Remove

```cpp
bool Remove(const K &key) noexcept
```

Supprimer une paire par clé

### Clear

```cpp
void Clear() noexcept
```

Vider complètement la map

### Size

```cpp
NkSize Size() const noexcept
```

Obtenir le nombre de paires stockées

### Data

```cpp
const NkEnvPair<K, V> *Data() const noexcept
```

Obtenir un accès direct aux données internes (lecture seule)
