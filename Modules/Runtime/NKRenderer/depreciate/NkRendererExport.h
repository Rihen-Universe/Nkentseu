#pragma once
// =============================================================================
// NkRendererExport.h — macros de visibilité DLL (statique ou dynamique)
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"

#ifdef NKRENDERER_BUILD_SHARED
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       define NKRENDERER_API  __declspec(dllexport)
#   else
#       define NKRENDERER_API  __attribute__((visibility("default")))
#   endif
#elif defined(NKRENDERER_USE_SHARED)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       define NKRENDERER_API  __declspec(dllimport)
#   else
#       define NKRENDERER_API
#   endif
#else
#   define NKRENDERER_API
#endif