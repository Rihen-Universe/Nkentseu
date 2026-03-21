//
// NkMath.h — Master header for the NkMath library
// Include this single header to access all math types.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#pragma once
#ifndef NKENTSEU_NKMATH_H_INCLUDED
#define NKENTSEU_NKMATH_H_INCLUDED

// Core scalar math + constants
#include "NKMath/NkFunctions.h"

// Angle types
#include "NKMath/NkAngle.h"
#include "NKMath/NkEulerAngle.h"

// Vectors (Vec2/3/4 × f/d/i/u, single template)
#include "NKMath/NkVec.h"

// Matrices (Mat2/3/4 × f/d, single template)
#include "NKMath/NkMat.h"

// Quaternion
#include "NKMath/NkQuat.h"

// Utilities
#include "NKMath/NkRange.h"
#include "NKMath/NkRandom.h"
#include "NKMath/NkColor.h"

// SIMD batch helpers (optional, guarded by NK_ENABLE_SSE42 / NK_ENABLE_NEON)
#include "NKMath/NkSIMD.h"

// Shapes
#include "NKMath/NkRectangle.h"
#include "NKMath/NkSegment.h"

#endif // NKENTSEU_NKMATH_H_INCLUDED