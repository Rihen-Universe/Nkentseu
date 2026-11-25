# NkEnv.h

`#include "NKPlatform/NkEnv.h"`

## Types

- [nkentseu::env::NkEnvString](../nkentseu-env-nkenvstring/index.md)
- [nkentseu::env::NkEnvPair](../nkentseu-env-nkenvpair/index.md)
- [nkentseu::env::NkEnvVector](../nkentseu-env-nkenvvector/index.md)
- [nkentseu::env::NkEnvUMap](../nkentseu-env-nkenvumap/index.md)

## Functions

### NkMove

```cpp
template <typename T> constexpr T &&NkMove(T &t) noexcept
```

Generic move of a value (equivalent to std::move)

### operator+

```cpp
NkEnvString operator+(const NkEnvString &lhs, const NkEnvString &rhs)
```

Concatenation of two NkEnvString

### NkGet

```cpp
NkEnvString NkGet(const NkEnvString &name, NkEnvString &result) noexcept
```

Gets the value of an environment variable

### GetEnvVar

```cpp
const char *GetEnvVar(const char *name) noexcept
```

Reads an environment variable: a pointer to its value (in UTF-8), or nullptr if it does not exist.

### SetEnvVar

```cpp
bool SetEnvVar(const char *name, const char *value) noexcept
```

Writes an environment variable of the current process, where GetEnvVar reads it (the C runtime environment).

### UnsetEnvVar

```cpp
bool UnsetEnvVar(const char *name) noexcept
```

Removes an environment variable from the current process; GetEnvVar then returns nullptr.

### NkSet

```cpp
bool NkSet(const NkEnvString &name, const NkEnvString &value, bool overwrite = true) noexcept
```

Sets an environment variable

### NkUnset

```cpp
bool NkUnset(const NkEnvString &name) noexcept
```

Removes an environment variable

### NkExists

```cpp
bool NkExists(const NkEnvString &name) noexcept
```

Checks whether an environment variable exists

### NkGetAll

```cpp
NkEnvUMap<NkEnvString, NkEnvString> NkGetAll() noexcept
```

Gets a copy of every environment variable

### NkGetPathSeparator

```cpp
char NkGetPathSeparator() noexcept
```

Gets the platform-specific path separator

### NkPrependToPath

```cpp
bool NkPrependToPath(const NkEnvString &directory, const NkEnvString &varName = NkEnvString("PATH"))
```

Adds a directory at the start of a path variable

### NkAppendToPath

```cpp
bool NkAppendToPath(const NkEnvString &directory, const NkEnvString &varName = NkEnvString("PATH"))
```

Adds a directory at the end of a path variable

## Aliases

### NkSize

```cpp
using NkSize = unsigned int
```

Unsigned integer type for sizes and indices

