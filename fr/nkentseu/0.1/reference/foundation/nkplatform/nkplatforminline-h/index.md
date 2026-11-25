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

Indique qu'une fonction n'a pas d'effets de bord

### NKENTSEU_CONST_FUNCTION

```cpp
#define NKENTSEU_CONST_FUNCTION
```

Indique qu'une fonction dépend uniquement de ses paramètres

### NKENTSEU_RESTRICT_RETURN

```cpp
#define NKENTSEU_RESTRICT_RETURN
```

Indique que le pointeur de retour n'alias aucun autre pointeur

### NKENTSEU_RESTRICT_PARAM

```cpp
#define NKENTSEU_RESTRICT_PARAM(ptr)
```

Indique que les paramètres pointeurs ne s'aliasent pas entre eux

### NKENTSEU_NORETURN

```cpp
#define NKENTSEU_NORETURN
```

Indique que la fonction est susceptible de ne pas retourner

### NKENTSEU_COLD_FUNCTION

```cpp
#define NKENTSEU_COLD_FUNCTION
```

Indique que la fonction est improbable d'être appelée

### NKENTSEU_HOT_FUNCTION

```cpp
#define NKENTSEU_HOT_FUNCTION
```

Indique que la fonction est très probable d'être appelée

### NKENTSEU_LIKELY

```cpp
#define NKENTSEU_LIKELY(condition)
```

Indique qu'une condition est probablement vraie

### NKENTSEU_UNLIKELY

```cpp
#define NKENTSEU_UNLIKELY(condition)
```

Indique qu'une condition est probablement fausse

### NKENTSEU_API_INLINE

```cpp
#define NKENTSEU_API_INLINE
```

Exporte une fonction inline

### NKENTSEU_API_FORCE_INLINE

```cpp
#define NKENTSEU_API_FORCE_INLINE
```

Exporte une fonction force_inline

### NKENTSEU_API_NO_INLINE

```cpp
#define NKENTSEU_API_NO_INLINE
```

Exporte une fonction no_inline

