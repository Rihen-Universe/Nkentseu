# nkentseu::env::NkEnvUMap

Unordered associative container (linear search)

`#include "NKPlatform/NkEnv.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### NkEnvUMap

```cpp
NkEnvUMap() noexcept = default
```

Default constructor

### NkEnvUMap

```cpp
NkEnvUMap(const NkEnvUMap &other) = default
```

Copy constructor

### NkEnvUMap

```cpp
NkEnvUMap(NkEnvUMap &&other) noexcept = default
```

Move constructor

### ~NkEnvUMap

```cpp
~NkEnvUMap() = default
```

Destructor

### operator=

```cpp
NkEnvUMap &operator=(const NkEnvUMap &other) = default
```

Copy assignment operator

### operator=

```cpp
NkEnvUMap &operator=(NkEnvUMap &&other) noexcept = default
```

Move assignment operator

### Set

```cpp
void Set(const K &key, const V &value)
```

Sets or updates a key/value pair (copy)

### Set

```cpp
void Set(K &&key, V &&value)
```

Sets or updates a key/value pair (move)

### Find

```cpp
const V *Find(const K &key) const noexcept
```

Looks up a value by key

### Remove

```cpp
bool Remove(const K &key) noexcept
```

Removes a pair by key

### Clear

```cpp
void Clear() noexcept
```

Empties the map completely

### Size

```cpp
NkSize Size() const noexcept
```

Gets the number of stored pairs

### Data

```cpp
const NkEnvPair<K, V> *Data() const noexcept
```

Gets direct, read-only access to the internal data
