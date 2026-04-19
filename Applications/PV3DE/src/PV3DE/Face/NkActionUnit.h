#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        // NkActionUnitId — 46 AU du Facial Action Coding System (Ekman & Friesen)
        // =====================================================================
        enum class NkActionUnitId : nk_uint8 {
            // ── Sourcils / front ──────────────────────────────────────────────
            AU1  = 1,   // Inner brow raise
            AU2  = 2,   // Outer brow raise
            AU4  = 4,   // Brow lowerer
            // ── Yeux / nez ────────────────────────────────────────────────────
            AU5  = 5,   // Upper lid raiser
            AU6  = 6,   // Cheek raiser
            AU7  = 7,   // Lid tightener
            AU9  = 9,   // Nose wrinkler
            AU10 = 10,  // Upper lip raiser
            // ── Bouche ────────────────────────────────────────────────────────
            AU11 = 11,  // Nasolabial deepener
            AU12 = 12,  // Lip corner puller (sourire)
            AU13 = 13,  // Cheek puffer
            AU14 = 14,  // Dimpler
            AU15 = 15,  // Lip corner depressor (tristesse)
            AU16 = 16,  // Lower lip depressor
            AU17 = 17,  // Chin raiser
            AU18 = 18,  // Lip pucker
            AU20 = 20,  // Lip stretcher (peur)
            AU22 = 22,  // Lip funneler
            AU23 = 23,  // Lip tightener
            AU24 = 24,  // Lip pressor
            AU25 = 25,  // Lips part
            AU26 = 26,  // Jaw drop
            AU27 = 27,  // Mouth stretch
            AU28 = 28,  // Lip suck
            // ── Yeux (fermeture) ──────────────────────────────────────────────
            AU41 = 41,  // Lid droop
            AU42 = 42,  // Slit
            AU43 = 43,  // Eyes closed
            AU44 = 44,  // Squint
            AU45 = 45,  // Blink
            AU46 = 46,  // Wink
            // ── Divers ────────────────────────────────────────────────────────
            AU51 = 51,  // Head turn left
            AU52 = 52,  // Head turn right
            AU53 = 53,  // Head up
            AU54 = 54,  // Head down
            AU55 = 55,  // Head tilt left
            AU56 = 56,  // Head tilt right
            AU57 = 57,  // Head forward
            AU58 = 58,  // Head back
            COUNT
        };

        // =====================================================================
        // NkActionUnit — état courant d'un AU
        // =====================================================================
        struct NkActionUnit {
            NkActionUnitId id        = NkActionUnitId::AU1;
            nk_float32     intensity = 0.f;  // 0 (absent) → 1 (maximum FACS)
            nk_float32     target    = 0.f;  // cible d'interpolation
            nk_float32     speed     = 4.f;  // vitesse de transition (unités/s)

            void Update(nk_float32 dt) {
                nk_float32 diff = target - intensity;
                nk_float32 step = speed * dt;
                if (diff > 0.f) intensity = (diff < step) ? target : intensity + step;
                else            intensity = (-diff < step) ? target : intensity - step;
            }
        };

        // =====================================================================
        // Présets FACS pour les émotions médicales courantes
        // Retourne un tableau de paires (AU, intensité cible)
        // =====================================================================
        struct NkAUPreset {
            NkActionUnitId au;
            nk_float32     intensity;
        };

        // Douleur légère (EVA 1–3)
        inline constexpr NkAUPreset kAU_PainMild[] = {
            {NkActionUnitId::AU4,  0.4f}, // froncement
            {NkActionUnitId::AU6,  0.2f},
            {NkActionUnitId::AU7,  0.3f},
            {NkActionUnitId::AU25, 0.2f},
        };

        // Douleur sévère (EVA 7–10)
        inline constexpr NkAUPreset kAU_PainSevere[] = {
            {NkActionUnitId::AU4,  0.9f},
            {NkActionUnitId::AU6,  0.7f},
            {NkActionUnitId::AU7,  0.8f},
            {NkActionUnitId::AU9,  0.5f},
            {NkActionUnitId::AU10, 0.4f},
            {NkActionUnitId::AU25, 0.6f},
            {NkActionUnitId::AU43, 0.3f},
        };

        // Anxiété
        inline constexpr NkAUPreset kAU_Anxious[] = {
            {NkActionUnitId::AU1,  0.6f},
            {NkActionUnitId::AU2,  0.5f},
            {NkActionUnitId::AU4,  0.3f},
            {NkActionUnitId::AU5,  0.7f},
            {NkActionUnitId::AU20, 0.4f},
            {NkActionUnitId::AU26, 0.2f},
        };

        // Nausée
        inline constexpr NkAUPreset kAU_Nauseous[] = {
            {NkActionUnitId::AU9,  0.6f},
            {NkActionUnitId::AU15, 0.5f},
            {NkActionUnitId::AU16, 0.3f},
            {NkActionUnitId::AU17, 0.4f},
            {NkActionUnitId::AU25, 0.3f},
        };

        // Épuisement
        inline constexpr NkAUPreset kAU_Exhausted[] = {
            {NkActionUnitId::AU41, 0.7f},
            {NkActionUnitId::AU4,  0.2f},
            {NkActionUnitId::AU15, 0.3f},
            {NkActionUnitId::AU17, 0.3f},
        };

        // Neutre (tous à 0 — utilisé comme reset)
        inline constexpr NkAUPreset kAU_Neutral[] = {};

    } // namespace pv3de
} // namespace nkentseu
