#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Export/import macros for shared/static builds.
 * Main data: NKUI_API and visibility/compiler switches.
 * Change this file when: Build/link visibility changes are required.
 */
/**
 * @File    NkUIExport.h
 * @Brief   Macros d'export/import + types de base NkUI.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

// ── Export DLL ───────────────────────────────────────────────────────────────
#if defined(_WIN32) || defined(_WIN64)
  #ifdef NKUI_BUILD_DLL
    #define NKUI_API __declspec(dllexport)
  #elif defined(NKUI_USE_DLL)
    #define NKUI_API __declspec(dllimport)
  #else
    #define NKUI_API
  #endif
#elif defined(__GNUC__) || defined(__clang__)
  #define NKUI_API __attribute__((visibility("default")))
#else
  #define NKUI_API
#endif

#if defined(_MSC_VER)
#  define NKUI_INLINE __forceinline
#else
#  define NKUI_INLINE inline __attribute__((always_inline))
#endif
#define NKUI_NODISCARD [[nodiscard]]
#define NKUI_NORETURN  [[noreturn]]

// ── Types de base (NKCore + NKMath) ──────────────────────────────────────────
#include "NKCore/NkTypes.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkColor.h"
#include "NKMath/NkRectangle.h"

namespace nkentseu {
  namespace nkui {

    // ── NkUIID — identifiant unique de widget (hash FNV-1a) ──────────────────────
    using NkUIID = uint32;
    static constexpr NkUIID NKUI_ID_NONE = 0;

    // ── Alias vers les types NKMath ───────────────────────────────────────────────
    using NkVec2  = math::NkVec2;
    using NkColor = math::NkColor;
    using NkRect  = math::NkFloatRect;

    // ── Helpers géométriques ──────────────────────────────────────────────────────
    NKUI_INLINE bool NkRectContains(NkRect r, NkVec2 p) noexcept {
        return p.x>=r.x && p.x<r.x+r.w && p.y>=r.y && p.y<r.y+r.h;
    }
    NKUI_INLINE NkRect NkRectIntersect(NkRect a, NkRect b) noexcept {
        float32 x1=a.x>b.x?a.x:b.x;
        float32 y1=a.y>b.y?a.y:b.y;
        float32 x2=(a.x+a.w)<(b.x+b.w)?(a.x+a.w):(b.x+b.w);
        float32 y2=(a.y+a.h)<(b.y+b.h)?(a.y+a.h):(b.y+b.h);
        return {x1,y1,x2>x1?x2-x1:0.f,y2>y1?y2-y1:0.f};
    }

    // ── Constante PI ─────────────────────────────────────────────────────────────
    static constexpr float32 NKUI_PI = 3.14159265358979323846f;
  }
} // namespace nkentseu
