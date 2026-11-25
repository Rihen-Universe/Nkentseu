# nkentseu::env::NkEnvString

Dynamically allocated character string (without the STL)

`#include "NKPlatform/NkEnv.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### NkEnvString

```cpp
NkEnvString() noexcept
```

Default constructor (empty string)

### NkEnvString

```cpp
explicit NkEnvString(const char *str)
```

Constructor from a C string

### NkEnvString

```cpp
explicit NkEnvString(char ch)
```

Constructor from a single character

### NkEnvString

```cpp
NkEnvString(const NkEnvString &other)
```

Copy constructor

### NkEnvString

```cpp
NkEnvString(NkEnvString &&other) noexcept
```

Move constructor

### ~NkEnvString

```cpp
~NkEnvString()
```

Destructor (frees the allocated memory)

### operator=

```cpp
NkEnvString &operator=(const NkEnvString &other)
```

Copy assignment operator

### operator=

```cpp
NkEnvString &operator=(NkEnvString &&other) noexcept
```

Move assignment operator

### CStr

```cpp
const char *CStr() const noexcept
```

Gets the underlying C string (null-terminated)

### Size

```cpp
NkSize Size() const noexcept
```

Gets the length of the string (without the null terminator)

### Empty

```cpp
bool Empty() const noexcept
```

Checks whether the string is empty

### Clear

```cpp
void Clear() noexcept
```

Empties the string and frees the memory

### operator==

```cpp
bool operator==(const NkEnvString &other) const noexcept
```

Equality comparison

### operator!=

```cpp
bool operator!=(const NkEnvString &other) const noexcept
```

Inequality comparison

### operator<

```cpp
bool operator<(const NkEnvString &other) const noexcept
```

Lexicographic order comparison
