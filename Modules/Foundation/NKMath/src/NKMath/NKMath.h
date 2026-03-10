// ============================================================
// FILE: NkMath.h
// DESCRIPTION: Master header for mathematics module
// ============================================================

#pragma once

#ifndef NKENTSEU_NKMATH_H_INCLUDED
#define NKENTSEU_NKMATH_H_INCLUDED

#include "NkFunctions.h"
#include "NkSIMD.h"
#include "NkTypes.h"

namespace nkentseu {
    namespace math {

        // Backward-compatible names
        using Float3 = NkVec3f;
        using Double3 = NkVec3d;

    } // namespace math
} // namespace nkentseu

#endif // NKENTSEU_NKMATH_H_INCLUDED
