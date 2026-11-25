# NkCompilerDetect.h

`#include "NKPlatform/NkCompilerDetect.h"`

## Alias

### NKENTSEU_int128

```cpp
typedef __int128 NKENTSEU_int128
```

Type entier signé 128 bits

### NKENTSEU_uint128

```cpp
typedef unsigned __int128 NKENTSEU_uint128
```

Type entier non signé 128 bits

## Macros

### NKENTSEU_COMPILER_MSVC

```cpp
#define NKENTSEU_COMPILER_MSVC
```

Macro de détection MSVC (Microsoft Visual C++)

### NKENTSEU_COMPILER_VERSION

```cpp
#define NKENTSEU_COMPILER_VERSION
```

Version numérique du compilateur

### NKENTSEU_COMPILER_MSVC_2024

```cpp
#define NKENTSEU_COMPILER_MSVC_2024
```

MSVC 2024 ou supérieur (VS 17.10+)

### NKENTSEU_COMPILER_MSVC_2022

```cpp
#define NKENTSEU_COMPILER_MSVC_2022
```

MSVC 2022 ou supérieur (VS 17.0+)

### NKENTSEU_COMPILER_MSVC_2019

```cpp
#define NKENTSEU_COMPILER_MSVC_2019
```

MSVC 2019 ou supérieur (VS 16.0+)

### NKENTSEU_COMPILER_MSVC_2017

```cpp
#define NKENTSEU_COMPILER_MSVC_2017
```

MSVC 2017 ou supérieur (VS 15.0+)

### NKENTSEU_COMPILER_CLANG

```cpp
#define NKENTSEU_COMPILER_CLANG
```

Détection du compilateur Clang (LLVM)

### NKENTSEU_COMPILER_APPLE_CLANG

```cpp
#define NKENTSEU_COMPILER_APPLE_CLANG
```

Détection d'Apple Clang

### NKENTSEU_COMPILER_GCC

```cpp
#define NKENTSEU_COMPILER_GCC
```

Détection du compilateur GCC

### NKENTSEU_COMPILER_INTEL

```cpp
#define NKENTSEU_COMPILER_INTEL
```

Détection du compilateur Intel C++

### NKENTSEU_COMPILER_EMSCRIPTEN

```cpp
#define NKENTSEU_COMPILER_EMSCRIPTEN
```

Détection du compilateur Emscripten

### NKENTSEU_COMPILER_NVCC

```cpp
#define NKENTSEU_COMPILER_NVCC
```

Détection du compilateur NVCC (CUDA)

### NKENTSEU_COMPILER_SUNPRO

```cpp
#define NKENTSEU_COMPILER_SUNPRO
```

Détection du compilateur SunPro/Oracle

### NKENTSEU_COMPILER_XLC

```cpp
#define NKENTSEU_COMPILER_XLC
```

Détection du compilateur IBM XL C/C++

### NKENTSEU_CPP23

```cpp
#define NKENTSEU_CPP23
```

Standard C++23 ou supérieur

### NKENTSEU_CPP_VERSION

```cpp
#define NKENTSEU_CPP_VERSION
```



### NKENTSEU_CPP20

```cpp
#define NKENTSEU_CPP20
```

Standard C++20 ou supérieur

### NKENTSEU_CPP17

```cpp
#define NKENTSEU_CPP17
```

Standard C++17 ou supérieur

### NKENTSEU_CPP14

```cpp
#define NKENTSEU_CPP14
```

Standard C++14 ou supérieur

### NKENTSEU_CPP11

```cpp
#define NKENTSEU_CPP11
```

Standard C++11 ou supérieur

### NKENTSEU_CPP98

```cpp
#define NKENTSEU_CPP98
```

Standard C++98/C++03

### NKENTSEU_HAS_CPP11

```cpp
#define NKENTSEU_HAS_CPP11
```

Support général de C++11

### NKENTSEU_HAS_NULLPTR

```cpp
#define NKENTSEU_HAS_NULLPTR
```

Support du mot-clé nullptr

### NKENTSEU_HAS_AUTO

```cpp
#define NKENTSEU_HAS_AUTO
```

Support du mot-clé auto pour l'inférence de type

### NKENTSEU_HAS_DECLTYPE

```cpp
#define NKENTSEU_HAS_DECLTYPE
```

Support de decltype pour l'inspection de type

### NKENTSEU_HAS_RVALUE_REFERENCES

```cpp
#define NKENTSEU_HAS_RVALUE_REFERENCES
```

Support des références rvalue et du move semantics

### NKENTSEU_HAS_VARIADIC_TEMPLATES

```cpp
#define NKENTSEU_HAS_VARIADIC_TEMPLATES
```

Support des templates variadiques

### NKENTSEU_HAS_STATIC_ASSERT

```cpp
#define NKENTSEU_HAS_STATIC_ASSERT
```

Support de static_assert pour les assertions à la compilation

### NKENTSEU_HAS_CONSTEXPR

```cpp
#define NKENTSEU_HAS_CONSTEXPR
```

Support de constexpr pour l'évaluation à la compilation

### NKENTSEU_HAS_NOEXCEPT

```cpp
#define NKENTSEU_HAS_NOEXCEPT
```

Support de noexcept pour spécifier l'absence d'exceptions

### NKENTSEU_HAS_OVERRIDE

```cpp
#define NKENTSEU_HAS_OVERRIDE
```

Support du spécificateur override pour les méthodes virtuelles

### NKENTSEU_HAS_FINAL

```cpp
#define NKENTSEU_HAS_FINAL
```

Support du spécificateur final pour empêcher l'héritage

### NKENTSEU_HAS_DEFAULT_DELETE

```cpp
#define NKENTSEU_HAS_DEFAULT_DELETE
```

Support des fonctions default/delete

### NKENTSEU_HAS_LAMBDA

```cpp
#define NKENTSEU_HAS_LAMBDA
```

Support des expressions lambda

### NKENTSEU_HAS_RANGE_FOR

```cpp
#define NKENTSEU_HAS_RANGE_FOR
```

Support des boucles range-based for

### NK_CPP11

```cpp
#define NK_CPP11
```

Alias historique de NKENTSEU_HAS_CPP11, utilisé par les gardes de NKContainers (`#if defined(NK_CPP11)` : move, forwarding, noexcept).

### NKENTSEU_HAS_CPP14

```cpp
#define NKENTSEU_HAS_CPP14
```

Support général de C++14

### NKENTSEU_HAS_GENERIC_LAMBDAS

```cpp
#define NKENTSEU_HAS_GENERIC_LAMBDAS
```

Support des lambdas génériques (paramètres auto)

### NKENTSEU_HAS_VARIABLE_TEMPLATES

```cpp
#define NKENTSEU_HAS_VARIABLE_TEMPLATES
```

Support des variables templates

### NKENTSEU_HAS_RELAXED_CONSTEXPR

```cpp
#define NKENTSEU_HAS_RELAXED_CONSTEXPR
```

Support de constexpr relaxé (plus d'expressions autorisées)

### NKENTSEU_HAS_BINARY_LITERALS

```cpp
#define NKENTSEU_HAS_BINARY_LITERALS
```

Support des littéraux binaires (0b1010)

### NKENTSEU_HAS_DECLTYPE_AUTO

```cpp
#define NKENTSEU_HAS_DECLTYPE_AUTO
```

Support de decltype(auto) pour l'inférence de type parfaite

### NKENTSEU_HAS_CPP17

```cpp
#define NKENTSEU_HAS_CPP17
```

Support général de C++17

### NKENTSEU_HAS_INLINE_VARIABLES

```cpp
#define NKENTSEU_HAS_INLINE_VARIABLES
```

Support des variables inline (définition dans les headers)

### NKENTSEU_HAS_FOLD_EXPRESSIONS

```cpp
#define NKENTSEU_HAS_FOLD_EXPRESSIONS
```

Support des fold expressions pour les templates variadiques

### NKENTSEU_HAS_IF_CONSTEXPR

```cpp
#define NKENTSEU_HAS_IF_CONSTEXPR
```

Support de if constexpr pour la compilation conditionnelle

### NKENTSEU_HAS_STRUCTURED_BINDINGS

```cpp
#define NKENTSEU_HAS_STRUCTURED_BINDINGS
```

Support des structured bindings pour la décomposition de tuples/structs

### NKENTSEU_HAS_CONSTEXPR_IF

```cpp
#define NKENTSEU_HAS_CONSTEXPR_IF
```

Alias pour NKENTSEU_HAS_IF_CONSTEXPR (cohérence de nommage)

### NKENTSEU_HAS_NODISCARD

```cpp
#define NKENTSEU_HAS_NODISCARD
```

Support de l'attribut [[nodiscard]]

### NKENTSEU_HAS_MAYBE_UNUSED

```cpp
#define NKENTSEU_HAS_MAYBE_UNUSED
```

Support de l'attribut [[maybe_unused]]

### NKENTSEU_HAS_FALLTHROUGH

```cpp
#define NKENTSEU_HAS_FALLTHROUGH
```

Support de l'attribut [[fallthrough]] pour les switch

### NKENTSEU_HAS_CPP20

```cpp
#define NKENTSEU_HAS_CPP20
```

Support général de C++20

### NKENTSEU_HAS_CONCEPTS

```cpp
#define NKENTSEU_HAS_CONCEPTS
```

Support des concepts pour la contrainte de templates

### NKENTSEU_HAS_MODULES

```cpp
#define NKENTSEU_HAS_MODULES
```

Support des modules pour une compilation modulaire

### NKENTSEU_HAS_COROUTINES

```cpp
#define NKENTSEU_HAS_COROUTINES
```

Support des coroutines pour la programmation asynchrone

### NKENTSEU_HAS_CONSTEXPR_VIRTUAL

```cpp
#define NKENTSEU_HAS_CONSTEXPR_VIRTUAL
```

Support de constexpr sur les fonctions virtuelles

### NKENTSEU_HAS_CONSTEVAL

```cpp
#define NKENTSEU_HAS_CONSTEVAL
```

Support de consteval pour l'évaluation immédiate à la compilation

### NKENTSEU_HAS_CONSTINIT

```cpp
#define NKENTSEU_HAS_CONSTINIT
```

Support de constinit pour l'initialisation statique garantie

### NKENTSEU_HAS_THREE_WAY_COMPARISON

```cpp
#define NKENTSEU_HAS_THREE_WAY_COMPARISON
```

Support de l'opérateur de comparaison à trois voies (<=>)

### NKENTSEU_HAS_DESIGNATED_INITIALIZERS

```cpp
#define NKENTSEU_HAS_DESIGNATED_INITIALIZERS
```

Support des designated initializers (initialiseurs nommés)

### NKENTSEU_HAS_CPP23

```cpp
#define NKENTSEU_HAS_CPP23
```

Support général de C++23

### NKENTSEU_HAS_DEDUCING_THIS

```cpp
#define NKENTSEU_HAS_DEDUCING_THIS
```

Support de 'deducing this' pour les méthodes explicites

### NKENTSEU_HAS_IF_CONSTEVAL

```cpp
#define NKENTSEU_HAS_IF_CONSTEVAL
```

Support de if consteval pour la détection constexpr à l'exécution

### NKENTSEU_HAS_MULTIDIMENSIONAL_SUBSCRIPT

```cpp
#define NKENTSEU_HAS_MULTIDIMENSIONAL_SUBSCRIPT
```

Support des subscript multidimensionnels (obj[1, 2, 3])

### NKENTSEU_CONSTEXPR

```cpp
#define NKENTSEU_CONSTEXPR
```

Macro portable pour constexpr

### NKENTSEU_NOEXCEPT

```cpp
#define NKENTSEU_NOEXCEPT
```

Macro portable pour noexcept

### NKENTSEU_NOEXCEPT_IF

```cpp
#define NKENTSEU_NOEXCEPT_IF(condition)
```

Macro pour noexcept conditionnel

### NKENTSEU_OVERRIDE

```cpp
#define NKENTSEU_OVERRIDE
```

Macro portable pour override

### NKENTSEU_FINAL

```cpp
#define NKENTSEU_FINAL
```

Macro portable pour final

### NKENTSEU_NODISCARD

```cpp
#define NKENTSEU_NODISCARD
```

Macro portable pour [[nodiscard]]

### NKENTSEU_NODISCARD_MSG

```cpp
#define NKENTSEU_NODISCARD_MSG(msg)
```

[[nodiscard]] avec message personnalisé

### NKENTSEU_NODISCARD_SIMPLE

```cpp
#define NKENTSEU_NODISCARD_SIMPLE
```

Alias simple pour [[nodiscard]]

### NKENTSEU_MAYBE_UNUSED

```cpp
#define NKENTSEU_MAYBE_UNUSED
```

Macro portable pour [[maybe_unused]]

### NKENTSEU_FALLTHROUGH

```cpp
#define NKENTSEU_FALLTHROUGH
```

Macro portable pour [[fallthrough]]

### NKENTSEU_PACKED

```cpp
#define NKENTSEU_PACKED
```

Structure packed (sans padding mémoire)

### NKENTSEU_PACK_BEGIN

```cpp
#define NKENTSEU_PACK_BEGIN
```

Début de bloc packed avec push/pop

### NKENTSEU_PACK_END

```cpp
#define NKENTSEU_PACK_END
```

Fin de bloc packed

### NKENTSEU_RESTRICT

```cpp
#define NKENTSEU_RESTRICT
```

Mot-clé restrict pour les pointeurs sans aliasing

### NKENTSEU_THREAD_LOCAL

```cpp
#define NKENTSEU_THREAD_LOCAL
```

Stockage thread-local (variable par thread)

### NKENTSEU_DEPRECATED

```cpp
#define NKENTSEU_DEPRECATED
```

Attribut deprecated avec message optionnel

### NKENTSEU_DEPRECATED_MSG

```cpp
#define NKENTSEU_DEPRECATED_MSG(msg)
```

Attribut deprecated avec message personnalisé

### NKENTSEU_FUNCTION_NAME

```cpp
#define NKENTSEU_FUNCTION_NAME
```

Nom signé de la fonction actuelle (avec signature complète)

### NKENTSEU_FILE_NAME

```cpp
#define NKENTSEU_FILE_NAME
```

Chemin du fichier source actuel

### NKENTSEU_LINE_NUMBER

```cpp
#define NKENTSEU_LINE_NUMBER
```

Numéro de ligne source actuelle

### NKENTSEU_HAS_RTTI

```cpp
#define NKENTSEU_HAS_RTTI
```

Support de RTTI (Run-Time Type Information)

### NKENTSEU_HAS_EXCEPTIONS

```cpp
#define NKENTSEU_HAS_EXCEPTIONS
```

Support des exceptions C++

### NKENTSEU_HAS_INT128

```cpp
#define NKENTSEU_HAS_INT128
```

Support des entiers 128 bits (__int128)

### NKENTSEU_PRAGMA

```cpp
#define NKENTSEU_PRAGMA(x)
```

Macro pour émettre un pragma de manière portable

### NKENTSEU_DISABLE_WARNING_PUSH

```cpp
#define NKENTSEU_DISABLE_WARNING_PUSH
```

Sauvegarder l'état actuel des warnings (push)

### NKENTSEU_DISABLE_WARNING_POP

```cpp
#define NKENTSEU_DISABLE_WARNING_POP
```

Restaurer l'état des warnings après un push (pop)

### NKENTSEU_DISABLE_WARNING

```cpp
#define NKENTSEU_DISABLE_WARNING(warningNumber)
```

Désactiver un warning spécifique

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



