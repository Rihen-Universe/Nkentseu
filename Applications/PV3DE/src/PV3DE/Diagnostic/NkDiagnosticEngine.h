#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "PV3DE/Core/NkClinicalState.h"

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        // NkSymptomEntry — entrée de la base de données de symptômes
        // =====================================================================
        struct NkSymptomEntry {
            NkSymptomId id;
            NkString    name;
            NkString    description;
            nk_float32  defaultPainContrib;    // contribution au niveau de douleur
            nk_float32  defaultAnxietyContrib;
            nk_float32  defaultNauseaContrib;
            nk_float32  defaultFatigueContrib;
        };

        // =====================================================================
        // NkDiseaseEntry — entrée de la base de données de pathologies
        // =====================================================================
        struct NkDiseaseEntry {
            NkDiseaseId id;
            NkString    name;
            nk_float32  baseSeverity;       // 0–1
            // Symptômes diagnostiques avec leurs poids
            struct SymptomCriterion {
                NkSymptomId id;
                nk_float32  weight; // contribution au score différentiel
                bool        required; // symptôme obligatoire
            };
            NkVector<SymptomCriterion> criteria;
        };

        // =====================================================================
        // NkDiagnosticEngine
        // Reçoit les symptômes actifs + constantes vitales,
        // met à jour NkClinicalState (niveaux + différentiel).
        // =====================================================================
        class NkDiagnosticEngine {
        public:
            NkDiagnosticEngine()  = default;
            ~NkDiagnosticEngine() = default;

            // ── Init ──────────────────────────────────────────────────────────
            void Init();

            // Chargement depuis fichier JSON (Phase 5+)
            bool LoadSymptomDatabase(const char* path);
            bool LoadDiseaseDatabase(const char* path);

            // ── API clinique ──────────────────────────────────────────────────
            void AddSymptom(NkSymptomId id);
            void RemoveSymptom(NkSymptomId id);
            void ClearSymptoms();

            void SetVitalSigns(nk_float32 heartRate,
                               nk_float32 temperature,
                               nk_float32 spo2);

            // ── Update ────────────────────────────────────────────────────────
            // Recalcule l'état clinique complet.
            void Update(NkClinicalState& outState);

            // ── Accès BDD ─────────────────────────────────────────────────────
            const NkSymptomEntry* FindSymptom(NkSymptomId id) const;
            const NkDiseaseEntry* FindDisease(NkDiseaseId id)  const;
            const NkVector<NkSymptomEntry>& GetSymptoms()  const { return mSymptoms; }
            const NkVector<NkDiseaseEntry>& GetDiseases()  const { return mDiseases; }

        private:
            void RegisterBuiltinData();
            void ComputePhysiologicalLevels(NkClinicalState& state) const;
            void ComputeDifferential(NkClinicalState& state) const;

            NkVector<NkSymptomEntry> mSymptoms;
            NkVector<NkDiseaseEntry> mDiseases;

            // État courant des entrées
            NkVector<NkSymptomId> mActiveSymptoms;
            nk_float32 mHeartRate    = 72.f;
            nk_float32 mTemperature  = 37.f;
            nk_float32 mSpO2         = 98.f;
        };

    } // namespace pv3de
} // namespace nkentseu
