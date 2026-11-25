# NkFoundationLog.h

`#include "NKPlatform/NkFoundationLog.h"`

## Functions

### NkFoundationSetLogSink

```cpp
void NkFoundationSetLogSink(NkFoundationLogSink sink)
```

Sets a custom log sink

### NkFoundationGetLogSink

```cpp
NkFoundationLogSink NkFoundationGetLogSink()
```

Gets the log sink currently configured

## Aliases

### NkFoundationLogSink

```cpp
using NkFoundationLogSink = void (*)(const char *level, const char *file, int line, const char *message)
```

Function type for the custom log sink

## Variables

### gNkFoundationLogSink

```cpp
extern NkFoundationLogSink gNkFoundationLogSink
```

Global log sink configured by the user

## Macros

### NK_FOUNDATION_HAS_ANDROID_LOG

```cpp
#define NK_FOUNDATION_HAS_ANDROID_LOG
```

Flag for Android Log availability

### NK_FOUNDATION_RESTORE_LOGGER_MACRO

```cpp
#define NK_FOUNDATION_RESTORE_LOGGER_MACRO
```

Defined, with no value, when a « logger » macro already existed: it is restored at the end of the header.

### NK_FOUNDATION_LOG_LEVEL_NONE

```cpp
#define NK_FOUNDATION_LOG_LEVEL_NONE
```

Log level: no log

### NK_FOUNDATION_LOG_LEVEL_ERROR

```cpp
#define NK_FOUNDATION_LOG_LEVEL_ERROR
```

Log level: critical errors

### NK_FOUNDATION_LOG_LEVEL_WARN

```cpp
#define NK_FOUNDATION_LOG_LEVEL_WARN
```

Log level: warnings

### NK_FOUNDATION_LOG_LEVEL_INFO

```cpp
#define NK_FOUNDATION_LOG_LEVEL_INFO
```

Log level: general information

### NK_FOUNDATION_LOG_LEVEL_DEBUG

```cpp
#define NK_FOUNDATION_LOG_LEVEL_DEBUG
```

Log level: debug messages

### NK_FOUNDATION_LOG_LEVEL_TRACE

```cpp
#define NK_FOUNDATION_LOG_LEVEL_TRACE
```

Log level: detailed traces (very verbose)

### NK_FOUNDATION_LOG_LEVEL

```cpp
#define NK_FOUNDATION_LOG_LEVEL
```

Log level active at compile time

### NK_FOUNDATION_PRINT

```cpp
#define NK_FOUNDATION_PRINT(fmt, ...)
```

Generic log macro (INFO level)

### NK_FOUNDATION_SPRINT

```cpp
#define NK_FOUNDATION_SPRINT(buffer, bufferSize, fmt, ...)
```

Safe sprintf-style formatting macro

### NK_FOUNDATION_LOG_ERROR

```cpp
#define NK_FOUNDATION_LOG_ERROR(fmt, ...)
```

ERROR level log macro

### NK_FOUNDATION_LOG_WARN

```cpp
#define NK_FOUNDATION_LOG_WARN(fmt, ...)
```

WARN level log macro

### NK_FOUNDATION_LOG_INFO

```cpp
#define NK_FOUNDATION_LOG_INFO(fmt, ...)
```

INFO level log macro

### NK_FOUNDATION_LOG_DEBUG

```cpp
#define NK_FOUNDATION_LOG_DEBUG(fmt, ...)
```

DEBUG level log macro

### NK_FOUNDATION_LOG_TRACE

```cpp
#define NK_FOUNDATION_LOG_TRACE(fmt, ...)
```

TRACE level log macro

### NK_FOUNDATION_LOG_ERROR_VALUE

```cpp
#define NK_FOUNDATION_LOG_ERROR_VALUE(label, value)
```

ERROR level log macro for a labelled value

### NK_FOUNDATION_LOG_WARN_VALUE

```cpp
#define NK_FOUNDATION_LOG_WARN_VALUE(label, value)
```

WARN level log macro for a labelled value

### NK_FOUNDATION_LOG_INFO_VALUE

```cpp
#define NK_FOUNDATION_LOG_INFO_VALUE(label, value)
```

INFO level log macro for a labelled value

### NK_FOUNDATION_LOG_DEBUG_VALUE

```cpp
#define NK_FOUNDATION_LOG_DEBUG_VALUE(label, value)
```

DEBUG level log macro for a labelled value

### NK_FOUNDATION_LOG_TRACE_VALUE

```cpp
#define NK_FOUNDATION_LOG_TRACE_VALUE(label, value)
```

TRACE level log macro for a labelled value

