#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        // NkSymptomId — identifiant unique d'un symptôme
        // =====================================================================
        using NkSymptomId   = nk_uint32;
        using NkDiseaseId   = nk_uint32;

        // =====================================================================
        // NkDiagnosisEntry — entrée du diagnostic différentiel
        // =====================================================================
        struct NkDiagnosisEntry {
            NkDiseaseId diseaseId   = 0;
            NkString    diseaseName;
            nk_float32  probability = 0.f; // 0–1
            nk_float32  severity    = 0.f; // 0–1
        };

        // =====================================================================
        // EmotionState — états émotionnels de la FSM
        // =====================================================================
        enum class EmotionState : nk_uint8 {
            Neutral    = 0,
            Discomfort,
            PainMild,
            PainSevere,
            Anxious,
            Panic,
            Nauseous,
            Exhausted,
            Confused,
            COUNT
        };

        // =====================================================================
        // NkClinicalState
        // État clinique de sortie produit par NKDiagnostic.
        // Consommé par NKEmotion, NKFace, NKBody, NKSpeech.
        // =====================================================================
        struct NkClinicalState {
            // ── Niveaux physiologiques ─────────────────────────────────────
            nk_float32 painLevel           = 0.f; // 0–10 (EVA)
            nk_float32 nauseaLevel         = 0.f; // 0–1
            nk_float32 fatigueLevel        = 0.f; // 0–1
            nk_float32 anxietyLevel        = 0.f; // 0–1
            nk_float32 breathingDifficulty = 0.f; // 0–1
            nk_float32 heartRate           = 72.f; // BPM
            nk_float32 temperature         = 37.f; // °C
            nk_float32 spo2                = 98.f; // %

            bool isConscious = true;
            bool isTrembling = false;

            // ── État émotionnel déduit ─────────────────────────────────────
            EmotionState emotionState     = EmotionState::Neutral;
            nk_float32   emotionIntensity = 0.f; // 0–1

            // ── Symptômes actifs ──────────────────────────────────────────
            NkVector<NkSymptomId> activeSymptoms;

            // ── Diagnostic différentiel ───────────────────────────────────
            NkVector<NkDiagnosisEntry> differentialRanking; // trié par probabilité décroissante

            // ── Helpers ───────────────────────────────────────────────────
            bool HasSymptom(NkSymptomId id) const {
                for (auto s : activeSymptoms) { if (s == id) return true; }
                return false;
            }

            const NkDiagnosisEntry* GetTopDiagnosis() const {
                return differentialRanking.IsEmpty() ? nullptr : &differentialRanking[0];
            }

            // Déduit l'état émotionnel à partir des niveaux physiologiques
            void DeduceEmotion() {
                if (painLevel >= 7.f) {
                    emotionState     = EmotionState::PainSevere;
                    emotionIntensity = (painLevel - 7.f) / 3.f;
                } else if (painLevel >= 4.f) {
                    emotionState     = EmotionState::PainMild;
                    emotionIntensity = (painLevel - 4.f) / 3.f;
                } else if (painLevel >= 1.f) {
                    emotionState     = EmotionState::Discomfort;
                    emotionIntensity = painLevel / 4.f;
                } else if (anxietyLevel >= 0.7f) {
                    emotionState     = EmotionState::Anxious;
                    emotionIntensity = anxietyLevel;
                } else if (nauseaLevel >= 0.5f) {
                    emotionState     = EmotionState::Nauseous;
                    emotionIntensity = nauseaLevel;
                } else if (fatigueLevel >= 0.6f) {
                    emotionState     = EmotionState::Exhausted;
                    emotionIntensity = fatigueLevel;
                } else {
                    emotionState     = EmotionState::Neutral;
                    emotionIntensity = 0.f;
                }
            }
        };

    } // namespace pv3de
} // namespace nkentseu
