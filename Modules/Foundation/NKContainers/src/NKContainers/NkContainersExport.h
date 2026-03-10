#pragma once

#ifndef NKENTSEU_CONTAINERS_NKCONTAINERSEXPORT_H_INCLUDED
#define NKENTSEU_CONTAINERS_NKCONTAINERSEXPORT_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NKCore/Assert/NkAssert.h"

// NKCore legacy macro from NkMacros.h conflicts with traits::NkSwap / free NkSwap.
#ifdef NkSwap
#undef NkSwap
#endif

#ifdef NkAllocAligned
#undef NkAllocAligned
#endif

#ifdef NkFreeAligned
#undef NkFreeAligned
#endif

// NKCore variadic macro clashes with functional::NkForEach.
#ifdef NkForEach
#undef NkForEach
#endif

// Static library in this workspace. Kept for future DLL/shared builds.
#define NKENTSEU_CONTAINERS_API

#endif // NKENTSEU_CONTAINERS_NKCONTAINERSEXPORT_H_INCLUDED
