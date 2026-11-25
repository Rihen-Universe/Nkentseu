# nkentseu::env::NkEnvPair

Paire générique clé/valeur pour les conteneurs associatifs

`#include "NKPlatform/NkEnv.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### key

```cpp
K key
```

Clé de la paire

### value

```cpp
V value
```

Valeur associée à la clé

### NkEnvPair

```cpp
NkEnvPair() = default
```

Constructeur par défaut

### NkEnvPair

```cpp
NkEnvPair(const K &k, const V &v)
```

Constructeur avec initialisation par copie

### NkEnvPair

```cpp
NkEnvPair(K &&k, V &&v)
```

Constructeur avec initialisation par déplacement
