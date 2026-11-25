# NkFoundationLog.h

`#include "NKPlatform/NkFoundationLog.h"`

## Fonctions

### NkFoundationSetLogSink

```cpp
void NkFoundationSetLogSink(NkFoundationLogSink sink)
```

Définir un sink de log personnalisé

### NkFoundationGetLogSink

```cpp
NkFoundationLogSink NkFoundationGetLogSink()
```

Récupérer le sink de log actuellement configuré

## Alias

### NkFoundationLogSink

```cpp
using NkFoundationLogSink = void (*)(const char *level, const char *file, int line, const char *message)
```

Type de fonction pour le sink de log personnalisé

## Variables

### gNkFoundationLogSink

```cpp
extern NkFoundationLogSink gNkFoundationLogSink
```

Sink de log global configuré par l'utilisateur

## Macros

### NK_FOUNDATION_HAS_ANDROID_LOG

```cpp
#define NK_FOUNDATION_HAS_ANDROID_LOG
```

Indicateur de disponibilité d'Android Log

### NK_FOUNDATION_RESTORE_LOGGER_MACRO

```cpp
#define NK_FOUNDATION_RESTORE_LOGGER_MACRO
```



### NK_FOUNDATION_LOG_LEVEL_NONE

```cpp
#define NK_FOUNDATION_LOG_LEVEL_NONE
```

Niveau de log : aucun log

### NK_FOUNDATION_LOG_LEVEL_ERROR

```cpp
#define NK_FOUNDATION_LOG_LEVEL_ERROR
```

Niveau de log : erreurs critiques

### NK_FOUNDATION_LOG_LEVEL_WARN

```cpp
#define NK_FOUNDATION_LOG_LEVEL_WARN
```

Niveau de log : avertissements

### NK_FOUNDATION_LOG_LEVEL_INFO

```cpp
#define NK_FOUNDATION_LOG_LEVEL_INFO
```

Niveau de log : informations générales

### NK_FOUNDATION_LOG_LEVEL_DEBUG

```cpp
#define NK_FOUNDATION_LOG_LEVEL_DEBUG
```

Niveau de log : messages de debug

### NK_FOUNDATION_LOG_LEVEL_TRACE

```cpp
#define NK_FOUNDATION_LOG_LEVEL_TRACE
```

Niveau de log : traces détaillées (très verbeux)

### NK_FOUNDATION_LOG_LEVEL

```cpp
#define NK_FOUNDATION_LOG_LEVEL
```

Niveau de log actif à la compilation

### NK_FOUNDATION_PRINT

```cpp
#define NK_FOUNDATION_PRINT(fmt, ...)
```

Macro de log générique (niveau INFO)

### NK_FOUNDATION_SPRINT

```cpp
#define NK_FOUNDATION_SPRINT(buffer, bufferSize, fmt, ...)
```

Macro de formatage sprintf-style sécurisée

### NK_FOUNDATION_LOG_ERROR

```cpp
#define NK_FOUNDATION_LOG_ERROR(fmt, ...)
```

Macro de log niveau ERROR

### NK_FOUNDATION_LOG_WARN

```cpp
#define NK_FOUNDATION_LOG_WARN(fmt, ...)
```

Macro de log niveau WARN

### NK_FOUNDATION_LOG_INFO

```cpp
#define NK_FOUNDATION_LOG_INFO(fmt, ...)
```

Macro de log niveau INFO

### NK_FOUNDATION_LOG_DEBUG

```cpp
#define NK_FOUNDATION_LOG_DEBUG(fmt, ...)
```

Macro de log niveau DEBUG

### NK_FOUNDATION_LOG_TRACE

```cpp
#define NK_FOUNDATION_LOG_TRACE(fmt, ...)
```

Macro de log niveau TRACE

### NK_FOUNDATION_LOG_ERROR_VALUE

```cpp
#define NK_FOUNDATION_LOG_ERROR_VALUE(label, value)
```

Macro de log niveau ERROR pour une valeur avec label

### NK_FOUNDATION_LOG_WARN_VALUE

```cpp
#define NK_FOUNDATION_LOG_WARN_VALUE(label, value)
```

Macro de log niveau WARN pour une valeur avec label

### NK_FOUNDATION_LOG_INFO_VALUE

```cpp
#define NK_FOUNDATION_LOG_INFO_VALUE(label, value)
```

Macro de log niveau INFO pour une valeur avec label

### NK_FOUNDATION_LOG_DEBUG_VALUE

```cpp
#define NK_FOUNDATION_LOG_DEBUG_VALUE(label, value)
```

Macro de log niveau DEBUG pour une valeur avec label

### NK_FOUNDATION_LOG_TRACE_VALUE

```cpp
#define NK_FOUNDATION_LOG_TRACE_VALUE(label, value)
```

Macro de log niveau TRACE pour une valeur avec label

