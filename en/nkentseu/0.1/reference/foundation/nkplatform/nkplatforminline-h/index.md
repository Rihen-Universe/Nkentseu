# NkPlatformInline.h

`#include "NKPlatform/NkPlatformInline.h"`

## Macros

### NKENTSEU_INLINE_COMPILER_MSVC

```cpp
#define NKENTSEU_INLINE_COMPILER_MSVC
```



### NKENTSEU_INLINE_COMPILER_NAME

```cpp
#define NKENTSEU_INLINE_COMPILER_NAME
```



### NKENTSEU_INLINE_COMPILER_CLANG

```cpp
#define NKENTSEU_INLINE_COMPILER_CLANG
```



### NKENTSEU_INLINE_COMPILER_GCC

```cpp
#define NKENTSEU_INLINE_COMPILER_GCC
```



### NKENTSEU_INLINE_COMPILER_INTEL

```cpp
#define NKENTSEU_INLINE_COMPILER_INTEL
```



### NKENTSEU_INLINE_COMPILER_ARMCC

```cpp
#define NKENTSEU_INLINE_COMPILER_ARMCC
```



### NKENTSEU_INLINE_COMPILER_IAR

```cpp
#define NKENTSEU_INLINE_COMPILER_IAR
```



### NKENTSEU_INLINE_COMPILER_UNKNOWN

```cpp
#define NKENTSEU_INLINE_COMPILER_UNKNOWN
```



### NKENTSEU_INLINE

```cpp
#define NKENTSEU_INLINE
```



### NKENTSEU_FORCE_INLINE

```cpp
#define NKENTSEU_FORCE_INLINE
```



### NKENTSEU_NO_INLINE

```cpp
#define NKENTSEU_NO_INLINE
```



### NKENTSEU_PURE_FUNCTION

```cpp
#define NKENTSEU_PURE_FUNCTION
```

States that a function has no side effects

### NKENTSEU_CONST_FUNCTION

```cpp
#define NKENTSEU_CONST_FUNCTION
```

States that a function only depends on its parameters

### NKENTSEU_RESTRICT_RETURN

```cpp
#define NKENTSEU_RESTRICT_RETURN
```

States that the returned pointer aliases no other pointer

### NKENTSEU_RESTRICT_PARAM

```cpp
#define NKENTSEU_RESTRICT_PARAM(ptr)
```

States that the pointer parameters do not alias each other

### NKENTSEU_NORETURN

```cpp
#define NKENTSEU_NORETURN
```

States that the function may not return

### NKENTSEU_COLD_FUNCTION

```cpp
#define NKENTSEU_COLD_FUNCTION
```

States that the function is unlikely to be called

### NKENTSEU_HOT_FUNCTION

```cpp
#define NKENTSEU_HOT_FUNCTION
```

States that the function is very likely to be called

### NKENTSEU_LIKELY

```cpp
#define NKENTSEU_LIKELY(condition)
```

States that a condition is probably true

### NKENTSEU_UNLIKELY

```cpp
#define NKENTSEU_UNLIKELY(condition)
```

States that a condition is probably false

### NKENTSEU_API_INLINE

```cpp
#define NKENTSEU_API_INLINE
```

Exports an inline function

### NKENTSEU_API_FORCE_INLINE

```cpp
#define NKENTSEU_API_FORCE_INLINE
```

Exports a force_inline function

### NKENTSEU_API_NO_INLINE

```cpp
#define NKENTSEU_API_NO_INLINE
```

Exports a no_inline function

