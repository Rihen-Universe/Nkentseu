// -----------------------------------------------------------------------------
// FICHIER: Examples\ModularExports\NkAudioExport.h
// DESCRIPTION: Exemple d'export modulaire pour le module Audio
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_EXAMPLES_MODULAREXPORTS_NKAUDIOEXPORT_H_INCLUDED
#define NKENTSEU_EXAMPLES_MODULAREXPORTS_NKAUDIOEXPORT_H_INCLUDED

#include "NkExport.h"

// ============================================================
// CONFIGURATION DU MODULE AUDIO
// ============================================================

#ifndef NKENTSEU_BUILDING_AUDIO
#if defined(NKENTSEU_AUDIO_EXPORTS) || defined(AUDIO_EXPORTS) || defined(NKENTSEU_BUILDING_SND)
#define NKENTSEU_BUILDING_AUDIO 1
#else
#define NKENTSEU_BUILDING_AUDIO 0
#endif
#endif

// ============================================================
// MACROS D'EXPORT POUR LE MODULE AUDIO
// ============================================================

#if NKENTSEU_BUILDING_AUDIO
#define NKENTSEU_AUDIO_API NKENTSEU_SYMBOL_EXPORT
#elif NKENTSEU_SHARED_BUILD
#define NKENTSEU_AUDIO_API NKENTSEU_SYMBOL_IMPORT
#else
#define NKENTSEU_AUDIO_API
#endif

#define NKENTSEU_AUDIO_C_API NKENTSEU_EXTERN_C NKENTSEU_AUDIO_API NKENTSEU_CALL
#define NKENTSEU_AUDIO_PUBLIC NKENTSEU_AUDIO_API
#define NKENTSEU_AUDIO_PRIVATE NKENTSEU_SYMBOL_HIDDEN

#define NK_AUDIO_API NKENTSEU_AUDIO_API
#define NK_SND_API NKENTSEU_AUDIO_API

// ============================================================
// EXEMPLE D'UTILISATION
// ============================================================

/*
 * #include "NkAudioExport.h"
 *
 * namespace nkentseu {
 * namespace audio {
 *
 * class NKENTSEU_AUDIO_API AudioEngine {
 * public:
 *     NKENTSEU_AUDIO_API void initialize();
 *     NKENTSEU_AUDIO_API void playSound(const char* path);
 * };
 *
 * } // namespace audio
 * } // namespace nkentseu
 *
 * // API C
 * NKENTSEU_EXTERN_C_BEGIN
 * NKENTSEU_AUDIO_C_API void nkAudioInit(void);
 * NKENTSEU_AUDIO_C_API void nkAudioPlay(const char* path);
 * NKENTSEU_EXTERN_C_END
 */

#endif // NKENTSEU_EXAMPLES_MODULAREXPORTS_NKAUDIOEXPORT_H_INCLUDED