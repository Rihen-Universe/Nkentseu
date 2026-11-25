# nkentseu::env::NkEnvPair

Generic key/value pair for associative containers

`#include "NKPlatform/NkEnv.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### key

```cpp
K key
```

Key of the pair

### value

```cpp
V value
```

Value associated with the key

### NkEnvPair

```cpp
NkEnvPair() = default
```

Default constructor

### NkEnvPair

```cpp
NkEnvPair(const K &k, const V &v)
```

Constructor with copy initialisation

### NkEnvPair

```cpp
NkEnvPair(K &&k, V &&v)
```

Constructor with move initialisation
