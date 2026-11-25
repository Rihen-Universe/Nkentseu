# NkCompilerDetect.h

`#include "NKPlatform/NkCompilerDetect.h"`

## Aliases

### NKENTSEU_int128

```cpp
typedef __int128 NKENTSEU_int128
```

Signed 128-bit integer type

### NKENTSEU_uint128

```cpp
typedef unsigned __int128 NKENTSEU_uint128
```

Unsigned 128-bit integer type

## Macros

### NKENTSEU_COMPILER_MSVC

```cpp
#define NKENTSEU_COMPILER_MSVC
```

Detection macro for MSVC (Microsoft Visual C++)

### NKENTSEU_COMPILER_VERSION

```cpp
#define NKENTSEU_COMPILER_VERSION
```

Numeric compiler version

### NKENTSEU_COMPILER_MSVC_2024

```cpp
#define NKENTSEU_COMPILER_MSVC_2024
```

MSVC 2024 or later (VS 17.10+)

### NKENTSEU_COMPILER_MSVC_2022

```cpp
#define NKENTSEU_COMPILER_MSVC_2022
```

MSVC 2022 or later (VS 17.0+)

### NKENTSEU_COMPILER_MSVC_2019

```cpp
#define NKENTSEU_COMPILER_MSVC_2019
```

MSVC 2019 or later (VS 16.0+)

### NKENTSEU_COMPILER_MSVC_2017

```cpp
#define NKENTSEU_COMPILER_MSVC_2017
```

MSVC 2017 or later (VS 15.0+)

### NKENTSEU_COMPILER_CLANG

```cpp
#define NKENTSEU_COMPILER_CLANG
```

Detection of the Clang (LLVM) compiler

### NKENTSEU_COMPILER_APPLE_CLANG

```cpp
#define NKENTSEU_COMPILER_APPLE_CLANG
```

Detection of Apple Clang

### NKENTSEU_COMPILER_GCC

```cpp
#define NKENTSEU_COMPILER_GCC
```

Detection of the GCC compiler

### NKENTSEU_COMPILER_INTEL

```cpp
#define NKENTSEU_COMPILER_INTEL
```

Detection of the Intel C++ compiler

### NKENTSEU_COMPILER_EMSCRIPTEN

```cpp
#define NKENTSEU_COMPILER_EMSCRIPTEN
```

Detection of the Emscripten compiler

### NKENTSEU_COMPILER_NVCC

```cpp
#define NKENTSEU_COMPILER_NVCC
```

Detection of the NVCC (CUDA) compiler

### NKENTSEU_COMPILER_SUNPRO

```cpp
#define NKENTSEU_COMPILER_SUNPRO
```

Detection of the SunPro/Oracle compiler

### NKENTSEU_COMPILER_XLC

```cpp
#define NKENTSEU_COMPILER_XLC
```

Detection of the IBM XL C/C++ compiler

### NKENTSEU_CPP23

```cpp
#define NKENTSEU_CPP23
```

C++23 standard or later

### NKENTSEU_CPP_VERSION

```cpp
#define NKENTSEU_CPP_VERSION
```



### NKENTSEU_CPP20

```cpp
#define NKENTSEU_CPP20
```

C++20 standard or later

### NKENTSEU_CPP17

```cpp
#define NKENTSEU_CPP17
```

C++17 standard or later

### NKENTSEU_CPP14

```cpp
#define NKENTSEU_CPP14
```

C++14 standard or later

### NKENTSEU_CPP11

```cpp
#define NKENTSEU_CPP11
```

C++11 standard or later

### NKENTSEU_CPP98

```cpp
#define NKENTSEU_CPP98
```

C++98/C++03 standard

### NKENTSEU_HAS_CPP11

```cpp
#define NKENTSEU_HAS_CPP11
```

General C++11 support

### NKENTSEU_HAS_NULLPTR

```cpp
#define NKENTSEU_HAS_NULLPTR
```

Support for the nullptr keyword

### NKENTSEU_HAS_AUTO

```cpp
#define NKENTSEU_HAS_AUTO
```

Support for the auto keyword for type inference

### NKENTSEU_HAS_DECLTYPE

```cpp
#define NKENTSEU_HAS_DECLTYPE
```

Support for decltype for type inspection

### NKENTSEU_HAS_RVALUE_REFERENCES

```cpp
#define NKENTSEU_HAS_RVALUE_REFERENCES
```

Support for rvalue references and move semantics

### NKENTSEU_HAS_VARIADIC_TEMPLATES

```cpp
#define NKENTSEU_HAS_VARIADIC_TEMPLATES
```

Support for variadic templates

### NKENTSEU_HAS_STATIC_ASSERT

```cpp
#define NKENTSEU_HAS_STATIC_ASSERT
```

Support for static_assert for compile-time assertions

### NKENTSEU_HAS_CONSTEXPR

```cpp
#define NKENTSEU_HAS_CONSTEXPR
```

Support for constexpr for compile-time evaluation

### NKENTSEU_HAS_NOEXCEPT

```cpp
#define NKENTSEU_HAS_NOEXCEPT
```

Support for noexcept to state that no exception is thrown

### NKENTSEU_HAS_OVERRIDE

```cpp
#define NKENTSEU_HAS_OVERRIDE
```

Support for the override specifier on virtual methods

### NKENTSEU_HAS_FINAL

```cpp
#define NKENTSEU_HAS_FINAL
```

Support for the final specifier to prevent inheritance

### NKENTSEU_HAS_DEFAULT_DELETE

```cpp
#define NKENTSEU_HAS_DEFAULT_DELETE
```

Support for default/delete functions

### NKENTSEU_HAS_LAMBDA

```cpp
#define NKENTSEU_HAS_LAMBDA
```

Support for lambda expressions

### NKENTSEU_HAS_RANGE_FOR

```cpp
#define NKENTSEU_HAS_RANGE_FOR
```

Support for range-based for loops

### NK_CPP11

```cpp
#define NK_CPP11
```

Historical alias of NKENTSEU_HAS_CPP11, used by the NKContainers guards (`#if defined(NK_CPP11)`: move, forwarding, noexcept).

### NKENTSEU_HAS_CPP14

```cpp
#define NKENTSEU_HAS_CPP14
```

General C++14 support

### NKENTSEU_HAS_GENERIC_LAMBDAS

```cpp
#define NKENTSEU_HAS_GENERIC_LAMBDAS
```

Support for generic lambdas (auto parameters)

### NKENTSEU_HAS_VARIABLE_TEMPLATES

```cpp
#define NKENTSEU_HAS_VARIABLE_TEMPLATES
```

Support for variable templates

### NKENTSEU_HAS_RELAXED_CONSTEXPR

```cpp
#define NKENTSEU_HAS_RELAXED_CONSTEXPR
```

Support for relaxed constexpr (more expressions allowed)

### NKENTSEU_HAS_BINARY_LITERALS

```cpp
#define NKENTSEU_HAS_BINARY_LITERALS
```

Support for binary literals (0b1010)

### NKENTSEU_HAS_DECLTYPE_AUTO

```cpp
#define NKENTSEU_HAS_DECLTYPE_AUTO
```

Support for decltype(auto) for perfect type inference

### NKENTSEU_HAS_CPP17

```cpp
#define NKENTSEU_HAS_CPP17
```

General C++17 support

### NKENTSEU_HAS_INLINE_VARIABLES

```cpp
#define NKENTSEU_HAS_INLINE_VARIABLES
```

Support for inline variables (definition in headers)

### NKENTSEU_HAS_FOLD_EXPRESSIONS

```cpp
#define NKENTSEU_HAS_FOLD_EXPRESSIONS
```

Support for fold expressions on variadic templates

### NKENTSEU_HAS_IF_CONSTEXPR

```cpp
#define NKENTSEU_HAS_IF_CONSTEXPR
```

Support for if constexpr for conditional compilation

### NKENTSEU_HAS_STRUCTURED_BINDINGS

```cpp
#define NKENTSEU_HAS_STRUCTURED_BINDINGS
```

Support for structured bindings to decompose tuples/structs

### NKENTSEU_HAS_CONSTEXPR_IF

```cpp
#define NKENTSEU_HAS_CONSTEXPR_IF
```

Alias for NKENTSEU_HAS_IF_CONSTEXPR (naming consistency)

### NKENTSEU_HAS_NODISCARD

```cpp
#define NKENTSEU_HAS_NODISCARD
```

Support for the [[nodiscard]] attribute

### NKENTSEU_HAS_MAYBE_UNUSED

```cpp
#define NKENTSEU_HAS_MAYBE_UNUSED
```

Support for the [[maybe_unused]] attribute

### NKENTSEU_HAS_FALLTHROUGH

```cpp
#define NKENTSEU_HAS_FALLTHROUGH
```

Support for the [[fallthrough]] attribute in switches

### NKENTSEU_HAS_CPP20

```cpp
#define NKENTSEU_HAS_CPP20
```

General C++20 support

### NKENTSEU_HAS_CONCEPTS

```cpp
#define NKENTSEU_HAS_CONCEPTS
```

Support for concepts to constrain templates

### NKENTSEU_HAS_MODULES

```cpp
#define NKENTSEU_HAS_MODULES
```

Support for modules for modular compilation

### NKENTSEU_HAS_COROUTINES

```cpp
#define NKENTSEU_HAS_COROUTINES
```

Support for coroutines for asynchronous programming

### NKENTSEU_HAS_CONSTEXPR_VIRTUAL

```cpp
#define NKENTSEU_HAS_CONSTEXPR_VIRTUAL
```

Support for constexpr on virtual functions

### NKENTSEU_HAS_CONSTEVAL

```cpp
#define NKENTSEU_HAS_CONSTEVAL
```

Support for consteval for immediate compile-time evaluation

### NKENTSEU_HAS_CONSTINIT

```cpp
#define NKENTSEU_HAS_CONSTINIT
```

Support for constinit for guaranteed static initialisation

### NKENTSEU_HAS_THREE_WAY_COMPARISON

```cpp
#define NKENTSEU_HAS_THREE_WAY_COMPARISON
```

Support for the three-way comparison operator (<=>)

### NKENTSEU_HAS_DESIGNATED_INITIALIZERS

```cpp
#define NKENTSEU_HAS_DESIGNATED_INITIALIZERS
```

Support for designated initializers

### NKENTSEU_HAS_CPP23

```cpp
#define NKENTSEU_HAS_CPP23
```

General C++23 support

### NKENTSEU_HAS_DEDUCING_THIS

```cpp
#define NKENTSEU_HAS_DEDUCING_THIS
```

Support for 'deducing this' for explicit object methods

### NKENTSEU_HAS_IF_CONSTEVAL

```cpp
#define NKENTSEU_HAS_IF_CONSTEVAL
```

Support for if consteval to detect constant evaluation

### NKENTSEU_HAS_MULTIDIMENSIONAL_SUBSCRIPT

```cpp
#define NKENTSEU_HAS_MULTIDIMENSIONAL_SUBSCRIPT
```

Support for multidimensional subscripts (obj[1, 2, 3])

### NKENTSEU_CONSTEXPR

```cpp
#define NKENTSEU_CONSTEXPR
```

Portable macro for constexpr

### NKENTSEU_NOEXCEPT

```cpp
#define NKENTSEU_NOEXCEPT
```

Portable macro for noexcept

### NKENTSEU_NOEXCEPT_IF

```cpp
#define NKENTSEU_NOEXCEPT_IF(condition)
```

Macro for conditional noexcept

### NKENTSEU_OVERRIDE

```cpp
#define NKENTSEU_OVERRIDE
```

Portable macro for override

### NKENTSEU_FINAL

```cpp
#define NKENTSEU_FINAL
```

Portable macro for final

### NKENTSEU_NODISCARD

```cpp
#define NKENTSEU_NODISCARD
```

Portable macro for [[nodiscard]]

### NKENTSEU_NODISCARD_MSG

```cpp
#define NKENTSEU_NODISCARD_MSG(msg)
```

[[nodiscard]] with a custom message

### NKENTSEU_NODISCARD_SIMPLE

```cpp
#define NKENTSEU_NODISCARD_SIMPLE
```

Simple alias for [[nodiscard]]

### NKENTSEU_MAYBE_UNUSED

```cpp
#define NKENTSEU_MAYBE_UNUSED
```

Portable macro for [[maybe_unused]]

### NKENTSEU_FALLTHROUGH

```cpp
#define NKENTSEU_FALLTHROUGH
```

Portable macro for [[fallthrough]]

### NKENTSEU_PACKED

```cpp
#define NKENTSEU_PACKED
```

Packed structure (no memory padding)

### NKENTSEU_PACK_BEGIN

```cpp
#define NKENTSEU_PACK_BEGIN
```

Start of a packed block with push/pop

### NKENTSEU_PACK_END

```cpp
#define NKENTSEU_PACK_END
```

End of a packed block

### NKENTSEU_RESTRICT

```cpp
#define NKENTSEU_RESTRICT
```

restrict keyword for pointers without aliasing

### NKENTSEU_THREAD_LOCAL

```cpp
#define NKENTSEU_THREAD_LOCAL
```

Thread-local storage (one variable per thread)

### NKENTSEU_DEPRECATED

```cpp
#define NKENTSEU_DEPRECATED
```

deprecated attribute with an optional message

### NKENTSEU_DEPRECATED_MSG

```cpp
#define NKENTSEU_DEPRECATED_MSG(msg)
```

deprecated attribute with a custom message

### NKENTSEU_FUNCTION_NAME

```cpp
#define NKENTSEU_FUNCTION_NAME
```

Decorated name of the current function (with its full signature)

### NKENTSEU_FILE_NAME

```cpp
#define NKENTSEU_FILE_NAME
```

Path of the current source file

### NKENTSEU_LINE_NUMBER

```cpp
#define NKENTSEU_LINE_NUMBER
```

Current source line number

### NKENTSEU_HAS_RTTI

```cpp
#define NKENTSEU_HAS_RTTI
```

RTTI (Run-Time Type Information) support

### NKENTSEU_HAS_EXCEPTIONS

```cpp
#define NKENTSEU_HAS_EXCEPTIONS
```

C++ exceptions support

### NKENTSEU_HAS_INT128

```cpp
#define NKENTSEU_HAS_INT128
```

128-bit integer support (__int128)

### NKENTSEU_PRAGMA

```cpp
#define NKENTSEU_PRAGMA(x)
```

Macro to emit a pragma portably

### NKENTSEU_DISABLE_WARNING_PUSH

```cpp
#define NKENTSEU_DISABLE_WARNING_PUSH
```

Saves the current warning state (push)

### NKENTSEU_DISABLE_WARNING_POP

```cpp
#define NKENTSEU_DISABLE_WARNING_POP
```

Restores the warning state after a push (pop)

### NKENTSEU_DISABLE_WARNING

```cpp
#define NKENTSEU_DISABLE_WARNING(warningNumber)
```

Disables a specific warning

### NKENTSEU_DISABLE_WARNING_MSVC

```cpp
#define NKENTSEU_DISABLE_WARNING_MSVC(x)
```



### NKENTSEU_DISABLE_WARNING_CLANG

```cpp
#define NKENTSEU_DISABLE_WARNING_CLANG(x)
```



### NKENTSEU_DISABLE_WARNING_GCC

```cpp
#define NKENTSEU_DISABLE_WARNING_GCC(x)
```



