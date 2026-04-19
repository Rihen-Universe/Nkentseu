#pragma once
// =============================================================================
// PV3DE/Diagnostic/NkCaseLoader.h
// =============================================================================
// Charge et interprète les cas cliniques scriptés (.nkcase).
// Un fichier .nkcase est un JSON qui décrit :
//   - Les métadonnées du cas (nom, auteur, niveau de difficulté)
//   - L'état initial du patient (constantes vitales, symptômes)
//   - Les événements temporels (progression de la maladie)
//   - Les réponses prédéfinies aux questions du médecin
//   - Les objectifs pédagogiques et le corrigé
//
// Format .nkcase :
// {
//   "version": 1,
//   "id": "case_idm_001",
//   "name": "Infarctus du myocarde — présentation typique",
//   "author": "Dr. Martin",
//   "difficulty": 2,
//   "patient": { "name": "Jean Dupont", "age": 58, "gender": "M" },
//   "initial_state": {
//     "heart_rate": 95, "temperature": 37.2, "spo2": 96,
//     "symptoms": [1, 9, 10, 11],
//     "pain_level": 8.0
//   },
//   "events": [
//     { "time_s": 60, "type": "add_symptom", "symptom_id": 2 },
//     { "time_s": 120, "type": "set_pain", "value": 9.5 },
//     { "time_s": 300, "type": "force_emotion", "emotion": "PainSevere" }
//   ],
//   "qa_pairs": [
//     { "question": "où avez-vous mal", "answer": "J'ai une douleur dans la poitrine..." }
//   ],
//   "objectives": ["Reconnaître l'IDM", "Demander ECG"],
//   "correct_diagnosis": 100
// }
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "PV3DE/Core/NkClinicalState.h"

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        // NkCaseEvent — événement temporel dans un cas clinique
        // =====================================================================
        enum class NkCaseEventType : nk_uint8 {
            AddSymptom = 0,
            RemoveSymptom,
            SetPain,
            SetVitals,
            ForceEmotion,
            PlaySpeech,
            SetBreathPattern,
        };

        struct NkCaseEvent {
            nk_float32     timeSeconds = 0.f;
            NkCaseEventType type;

            // Payload (union-like)
            NkSymptomId    symptomId    = 0;
            nk_float32     floatValue   = 0.f;
            NkString       stringValue;  // emotion name, speech text, pattern name
            nk_float32     heartRate    = 72.f;
            nk_float32     temperature  = 37.f;
            nk_float32     spo2         = 98.f;
        };

        // =====================================================================
        // NkQAPair — paire question/réponse pour le dialogue médecin-patient
        // =====================================================================
        struct NkQAPair {
            NkString question;   // mots-clés déclencheurs (ex: "douleur,mal,chest")
            NkString answer;     // réponse du patient
            nk_float32 emotionIntensity = 0.f; // intensité émotionnelle de la réponse
        };

        // =====================================================================
        // NkCaseInitialState — état initial du patient au chargement
        // =====================================================================
        struct NkCaseInitialState {
            nk_float32 heartRate    = 72.f;
            nk_float32 temperature  = 37.f;
            nk_float32 spo2         = 98.f;
            nk_float32 painLevel    = 0.f;
            NkVector<NkSymptomId> symptoms;
        };

        // =====================================================================
        // NkCaseData — données complètes d'un cas chargé
        // =====================================================================
        struct NkPatientDemographics {
            NkString   name;
            nk_uint32  age    = 0;
            NkString   gender;   // "M", "F", "O"
        };

        struct NkCaseData {
            NkString                id;
            NkString                name;
            NkString                author;
            nk_uint32               difficulty   = 1;  // 1–5
            NkPatientDemographics   patient;
            NkCaseInitialState      initialState;
            NkVector<NkCaseEvent>   events;
            NkVector<NkQAPair>      qaPairs;
            NkVector<NkString>      objectives;
            NkDiseaseId             correctDiagnosis = 0;
            bool                    loaded = false;
        };

        // =====================================================================
        // NkCaseLoader
        // =====================================================================
        class NkCaseLoader {
        public:
            NkCaseLoader() = default;

            // ── Chargement ────────────────────────────────────────────────────
            bool Load(const char* path, NkCaseData& outCase) noexcept;

            // Valider un fichier sans charger complètement
            bool Validate(const char* path) const noexcept;

            // Lister tous les cas dans un dossier
            void ScanDirectory(const char* dir,
                               NkVector<NkString>& outPaths) const noexcept;

            // Lire uniquement les métadonnées (nom, id, difficulté)
            bool LoadMeta(const char* path, NkCaseData& outMeta) const noexcept;

            // ── Sauvegarde (génération de cas) ────────────────────────────────
            bool Save(const char* path, const NkCaseData& caseData) const noexcept;

            // ── Exécution des événements ──────────────────────────────────────
            // Retourne les événements à déclencher pour le temps [prevT, currT[
            void GetPendingEvents(const NkCaseData& caseData,
                                  nk_float32 prevTimeS,
                                  nk_float32 currTimeS,
                                  NkVector<const NkCaseEvent*>& out) const noexcept;

            // ── Dialogue ──────────────────────────────────────────────────────
            // Cherche une réponse aux mots-clés dans la question
            const NkQAPair* FindAnswer(const NkCaseData& caseData,
                                       const char* question) const noexcept;

        private:
            bool ParseInitialState(const char* json, NkCaseData& out) const noexcept;
            bool ParseEvents      (const char* json, NkCaseData& out) const noexcept;
            bool ParseQAPairs     (const char* json, NkCaseData& out) const noexcept;
            bool ParseObjectives  (const char* json, NkCaseData& out) const noexcept;

            NkCaseEventType ParseEventType(const char* type) const noexcept;
        };

        // =====================================================================
        // NkCaseRunner — exécuteur de cas en temps réel
        // =====================================================================
        class NkCaseRunner {
        public:
            NkCaseRunner() = default;

            void Load(const NkCaseData* caseData) noexcept;
            void Reset() noexcept;
            void Pause() noexcept  { mPaused = true;  }
            void Resume() noexcept { mPaused = false; }

            bool IsRunning() const noexcept { return mRunning && !mPaused; }
            bool IsFinished() const noexcept { return mFinished; }
            nk_float32 GetElapsedSeconds() const noexcept { return mElapsed; }

            // Retourne les événements déclenchés ce tick
            const NkVector<const NkCaseEvent*>& Update(
                nk_float32 dt,
                const NkCaseLoader& loader) noexcept;

        private:
            const NkCaseData* mCase    = nullptr;
            nk_float32        mElapsed = 0.f;
            nk_float32        mPrev    = 0.f;
            bool              mRunning  = false;
            bool              mPaused   = false;
            bool              mFinished = false;

            NkVector<const NkCaseEvent*> mPendingEvents;
        };

    } // namespace pv3de
} // namespace nkentseu
