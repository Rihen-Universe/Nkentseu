// -----------------------------------------------------------------------------
// FICHIER: NKAudio/src/NKAudio/NkAudioExport.h
// DESCRIPTION: Macros d'export DLL/DSO pour le module NKAudio
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 2.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOEXPORT_H_INCLUDED
#define NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOEXPORT_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NKCore/NkExport.h"

// ============================================================
// CONFIGURATION DU MODULE AUDIO
// ============================================================

#ifndef NKENTSEU_BUILDING_AUDIO
#   if defined(NKENTSEU_AUDIO_EXPORTS) || defined(AUDIO_EXPORTS) || defined(NKENTSEU_BUILDING_SND)
#       define NKENTSEU_BUILDING_AUDIO 1
#   else
#       define NKENTSEU_BUILDING_AUDIO 0
#   endif
#endif

// ============================================================
// MACROS D'EXPORT
// ============================================================

#if NKENTSEU_BUILDING_AUDIO
#   define NKENTSEU_AUDIO_API NKENTSEU_SYMBOL_EXPORT
#elif NKENTSEU_SHARED_BUILD
#   define NKENTSEU_AUDIO_API NKENTSEU_SYMBOL_IMPORT
#else
#   define NKENTSEU_AUDIO_API
#endif

#define NKENTSEU_AUDIO_C_API   NKENTSEU_EXTERN_C NKENTSEU_AUDIO_API NKENTSEU_CALL
#define NKENTSEU_AUDIO_PUBLIC  NKENTSEU_AUDIO_API
#define NKENTSEU_AUDIO_PRIVATE NKENTSEU_SYMBOL_HIDDEN

// Alias courts
#define NK_AUDIO_API NKENTSEU_AUDIO_API

#endif // NKENTSEU_NKAUDIO_SRC_NKAUDIO_NKAUDIOEXPORT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
