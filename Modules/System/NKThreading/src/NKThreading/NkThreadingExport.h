// ============================================================
// FILE: NkThreadingExport.h
// DESCRIPTION: Export macros for NKThreading DLL/shared library
// AUTHOR: Rihen
// DATE: 2026-03-05
// VERSION: 1.0.0
// ============================================================

#pragma once

#ifndef NKENTSEU_THREADING_NKTHREADING_EXPORT_H_INCLUDED
#define NKENTSEU_THREADING_NKTHREADING_EXPORT_H_INCLUDED

#include "NKPlatform/NkCompilerDetect.h"

// Déterminer si on compile une DLL/shared lib ou on la linke
#if defined(NKENTSEU_THREADING_BUILDING_DLL)
    // Définit les exports lors de la compilation de la lib
    #if defined(NKENTSEU_COMPILER_MSVC)
        #define NKTHREADING_API __declspec(dllexport)
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKTHREADING_API __attribute__((visibility("default")))
    #else
        #define NKTHREADING_API
    #endif
#else
    // Définit les imports lors du linking
    #if defined(NKENTSEU_COMPILER_MSVC)
        #define NKTHREADING_API __declspec(dllimport)
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKTHREADING_API
    #else
        #define NKTHREADING_API
    #endif
#endif

// Staticlib ou header-only
#if !defined(NKENTSEU_THREADING_BUILDING_DLL)
    #undef NKTHREADING_API
    #define NKTHREADING_API
#endif

#endif // NKENTSEU_THREADING_NKTHREADING_EXPORT_H_INCLUDED
