// -----------------------------------------------------------------------------
// FICHIER: NKAudio/pch/pch.h
// DESCRIPTION: En-tête précompilé du module NKAudio
// Auteur: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2026
// VERSION: 2.0.0
// NOTES: Inclure UNIQUEMENT les headers stables qui ne changent pas souvent.
//        Zéro STL dans les headers exposés. cmath/cstring OK en interne.
// -----------------------------------------------------------------------------

#pragma once

// ── Fondation Nkentseu ────────────────────────────────────────────────────────
#include "NKCore/NkTypes.h"
#include "NKCore/NkMacros.h"
#include "NKPlatform/NkPlatformInline.h"
#include "NKCore/NkPlatform.h"
#include "NKMemory/NkAllocator.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKCore/NkAtomic.h"

// ── C runtime (interne uniquement) ────────────────────────────────────────────
#include <cmath>
#include <cstring>
#include <cstdio>

// ── API publique du module ───────────────────────────────────────────────────
#include "NKAudio/NkAudioApi.h"
