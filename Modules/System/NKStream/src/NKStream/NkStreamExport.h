#pragma once
// =============================================================================
// NkStreamExport.h — Macros d'export/import pour NKStream
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkInline.h"

#if defined(NKSTREAM_SHARED)
#  if defined(NKSTREAM_BUILD)
#    if defined(NKENTSEU_PLATFORM_WINDOWS)
#      define NKSTREAM_API __declspec(dllexport)
#    elif defined(__GNUC__) || defined(__clang__)
#      define NKSTREAM_API __attribute__((visibility("default")))
#    else
#      define NKSTREAM_API
#    endif
#  else
#    if defined(NKENTSEU_PLATFORM_WINDOWS)
#      define NKSTREAM_API __declspec(dllimport)
#    else
#      define NKSTREAM_API
#    endif
#  endif
#else
#  define NKSTREAM_API
#endif
