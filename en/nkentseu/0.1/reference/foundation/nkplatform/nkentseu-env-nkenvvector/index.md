# nkentseu::env::NkEnvVector

Generic dynamic vector container (without the STL)

`#include "NKPlatform/NkEnv.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### NkEnvVector

```cpp
NkEnvVector() noexcept
```

Default constructor (empty vector)

### NkEnvVector

```cpp
NkEnvVector(const NkEnvVector &other)
```

Copy constructor

### NkEnvVector

```cpp
NkEnvVector(NkEnvVector &&other) noexcept
```

Move constructor

### ~NkEnvVector

```cpp
~NkEnvVector()
```

Destructor (frees the memory and destroys the elements)

### operator=

```cpp
NkEnvVector &operator=(const NkEnvVector &other)
```

Copy assignment operator

### operator=

```cpp
NkEnvVector &operator=(NkEnvVector &&other) noexcept
```

Move assignment operator

### PushBack

```cpp
void PushBack(const T &value)
```

Appends an element at the end by copy

### PushBack

```cpp
void PushBack(T &&value)
```

Appends an element at the end by move

### PopBack

```cpp
void PopBack() noexcept
```

Removes the last element (destroys it)

### Clear

```cpp
void Clear() noexcept
```

Empties the container (destroys every element)

### Reserve

```cpp
void Reserve(NkSize capacity)
```

Reserves capacity to avoid reallocations

### At

```cpp
T &At(NkSize index)
```

Accesses an element by index (mutable version)

### At

```cpp
const T &At(NkSize index) const
```

Accesses an element by index (const version)

### operator[]

```cpp
T &operator[](NkSize index)
```

Index access operator (mutable version)

### operator[]

```cpp
const T &operator[](NkSize index) const
```

Index access operator (const version)

### Size

```cpp
NkSize Size() const noexcept
```

Gets the number of stored elements

### Capacity

```cpp
NkSize Capacity() const noexcept
```

Gets the allocated capacity

### Empty

```cpp
bool Empty() const noexcept
```

Checks whether the container is empty

### Begin

```cpp
T *Begin() noexcept
```

Gets a pointer to the first element (mutable)

### Begin

```cpp
const T *Begin() const noexcept
```

Gets a pointer to the first element (const)

### End

```cpp
T *End() noexcept
```

Gets a pointer past the last element (mutable)

### End

```cpp
const T *End() const noexcept
```

Gets a pointer past the last element (const)
