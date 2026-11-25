# NkPlatformExport.h

`#include "NKPlatform/NkPlatformExport.h"`

## Macros

### NKENTSEU_EXPORT_HAS_DECLSPEC

```cpp
#define NKENTSEU_EXPORT_HAS_DECLSPEC
```

Indique si le compilateur supporte __declspec(dllexport/dllimport)

### NKENTSEU_EXPORT_HAS_VISIBILITY

```cpp
#define NKENTSEU_EXPORT_HAS_VISIBILITY
```

Indique si le compilateur supporte __attribute__((visibility))

### NKENTSEU_PLATFORM_API_EXPORT

```cpp
#define NKENTSEU_PLATFORM_API_EXPORT
```

Macro pour exporter un symbole depuis NKPlatform

### NKENTSEU_PLATFORM_API_IMPORT

```cpp
#define NKENTSEU_PLATFORM_API_IMPORT
```

Macro pour importer un symbole depuis NKPlatform

### NKENTSEU_API_LOCAL

```cpp
#define NKENTSEU_API_LOCAL
```

Macro pour rendre un symbole privé à NKPlatform

### NKENTSEU_PLATFORM_API

```cpp
#define NKENTSEU_PLATFORM_API
```

Macro principale pour l'export/import des symboles NKPlatform

### NKENTSEU_CLASS_EXPORT

```cpp
#define NKENTSEU_CLASS_EXPORT
```

Macro pour exporter une classe complète

### NKENTSEU_EXTERN_C

```cpp
#define NKENTSEU_EXTERN_C
```

Macro pour la liaison C (extern "C")

### NKENTSEU_EXTERN_C_BEGIN

```cpp
#define NKENTSEU_EXTERN_C_BEGIN
```

Ouvre un bloc extern "C" en C++ ; ne fait rien en C.

### NKENTSEU_EXTERN_C_END

```cpp
#define NKENTSEU_EXTERN_C_END
```

Ferme le bloc ouvert par NKENTSEU_EXTERN_C_BEGIN.

