# NkPlatformInline.h

`#include "NKPlatform/NkPlatformInline.h"`

## Macros

### NKENTSEU_INLINE_COMPILER_MSVC

```cpp
#define NKENTSEU_INLINE_COMPILER_MSVC
```

Defined, with no value, when the compiler is MSVC (_MSC_VER).

### NKENTSEU_INLINE_COMPILER_NAME

```cpp
#define NKENTSEU_INLINE_COMPILER_NAME
```

Name of the compiler seen by NkPlatformInline.h, as a string ("MSVC", "Clang", "GCC"...).

### NKENTSEU_INLINE_COMPILER_CLANG

```cpp
#define NKENTSEU_INLINE_COMPILER_CLANG
```

Defined, with no value, when the compiler is Clang (and not MSVC).

### NKENTSEU_INLINE_COMPILER_GCC

```cpp
#define NKENTSEU_INLINE_COMPILER_GCC
```

Defined, with no value, when the compiler is GCC (neither MSVC nor Clang).

### NKENTSEU_INLINE_COMPILER_INTEL

```cpp
#define NKENTSEU_INLINE_COMPILER_INTEL
```

Defined, with no value, when the compiler is Intel's.

### NKENTSEU_INLINE_COMPILER_ARMCC

```cpp
#define NKENTSEU_INLINE_COMPILER_ARMCC
```

Defined, with no value, when the compiler is ARM Compiler (armcc).

### NKENTSEU_INLINE_COMPILER_IAR

```cpp
#define NKENTSEU_INLINE_COMPILER_IAR
```

Defined, with no value, when the compiler is IAR.

### NKENTSEU_INLINE_COMPILER_UNKNOWN

```cpp
#define NKENTSEU_INLINE_COMPILER_UNKNOWN
```

Defined, with no value, when no known compiler is recognised.

### NKENTSEU_INLINE

```cpp
#define NKENTSEU_INLINE
```

The language's inline keyword (inline in C++ and C99, __inline or __inline__ elsewhere).

### NKENTSEU_FORCE_INLINE

```cpp
#define NKENTSEU_FORCE_INLINE
```

Asks the compiler to inline the function at every call (__forceinline, always_inline).

### NKENTSEU_NO_INLINE

```cpp
#define NKENTSEU_NO_INLINE
```

Forbids the compiler from inlining the function (__declspec(noinline), noinline).

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

