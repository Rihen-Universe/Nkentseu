#pragma once

// =============================================================================
// NkWindow.h — Header principal NkWindow v2
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#include "Core/NkTypes.h"
#include "Core/NkWindowConfig.h"
#include "Core/NkSurface.h"
#include "Core/NkContext.h"
#include "Core/NkSystem.h"
#include "Core/NkEvent.h"
#include "Core/NkWindow.h"

#include "NKEvent/NkGamepadSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKEvent/NkGamepadEvent.h"
#include "NKEvent/NkDropEvent.h"
#include "NKEvent/NkEventSystem.h"
// NKRenderer is intentionally not included here to avoid reverse dependency
// NKWindow -> NKRenderer. Include NKRenderer headers explicitly where needed.

#include "Core/NkDialogs.h"
#include "Core/NkEntry.h"
