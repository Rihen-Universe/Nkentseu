#include "NkDiagnosticEngine.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace pv3de {

        void NkDiagnosticEngine::Init() {
            RegisterBuiltinData();
            logger.Infof("[NkDiagnosticEngine] Init — {} symptômes, {} pathologies\n",
                         mSymptoms.Size(), mDiseases.Size());
        }

        // =====================================================================
        // Base de données embarquée (sous-ensemble minimal pour démarrage)
        // =====================================================================
        void NkDiagnosticEngine::RegisterBuiltinData() {
            // ── Symptômes ─────────────────────────────────────────────────────
            auto AddSym = [&](NkSymptomId id, const char* name, const char* desc,
                              nk_float32 pain, nk_float32 anx,
                              nk_float32 nau, nk_float32 fat) {
                NkSymptomEntry e;
                e.id = id; e.name = name; e.description = desc;
                e.defaultPainContrib    = pain;
                e.defaultAnxietyContrib = anx;
                e.defaultNauseaContrib  = nau;
                e.defaultFatigueContrib = fat;
                mSymptoms.PushBack(e);
            };

            AddSym(1,  "Douleur thoracique",    "Oppression ou douleur dans la poitrine", 7.f, 0.6f, 0.1f, 0.2f);
            AddSym(2,  "Dyspnée",               "Difficulté respiratoire",                3.f, 0.5f, 0.1f, 0.5f);
            AddSym(3,  "Nausée",                "Sensation de malaise gastrique",          2.f, 0.2f, 0.8f, 0.3f);
            AddSym(4,  "Céphalée",              "Maux de tête",                            4.f, 0.3f, 0.2f, 0.3f);
            AddSym(5,  "Fièvre",                "Température > 38°C",                      2.f, 0.2f, 0.2f, 0.5f);
            AddSym(6,  "Fatigue intense",       "Épuisement général",                      1.f, 0.1f, 0.1f, 0.9f);
            AddSym(7,  "Douleur abdominale",    "Crampes ou douleur abdominale",           5.f, 0.3f, 0.5f, 0.2f);
            AddSym(8,  "Vertiges",              "Sensation de tournis",                    2.f, 0.4f, 0.3f, 0.3f);
            AddSym(9,  "Palpitations",          "Battements cardiaques irréguliers",       3.f, 0.7f, 0.2f, 0.2f);
            AddSym(10, "Irradiation bras gauche","Douleur irradiant dans le bras gauche",  6.f, 0.5f, 0.1f, 0.2f);
            AddSym(11, "Sueurs froides",        "Diaphorèse",                             2.f, 0.6f, 0.2f, 0.2f);
            AddSym(12, "Vomissements",          "Nausées avec émèse",                      2.f, 0.3f, 0.9f, 0.4f);
            AddSym(13, "Toux productive",       "Toux avec expectorations",               1.f, 0.2f, 0.2f, 0.4f);
            AddSym(14, "Douleur pleurétique",   "Douleur à l'inspiration profonde",        5.f, 0.3f, 0.1f, 0.3f);
            AddSym(15, "Confusion mentale",     "Désorientation, altération conscience",   1.f, 0.5f, 0.2f, 0.6f);

            // ── Pathologies ───────────────────────────────────────────────────
            auto AddDis = [&](NkDiseaseId id, const char* name, nk_float32 sev) -> NkDiseaseEntry& {
                NkDiseaseEntry e;
                e.id = id; e.name = name; e.baseSeverity = sev;
                mDiseases.PushBack(e);
                return mDiseases[mDiseases.Size() - 1];
            };

            // IDM (Infarctus du myocarde)
            {
                auto& d = AddDis(100, "Infarctus du myocarde", 0.95f);
                d.criteria.PushBack({1,  0.9f, true});   // douleur thoracique
                d.criteria.PushBack({10, 0.8f, false});  // irradiation
                d.criteria.PushBack({9,  0.5f, false});  // palpitations
                d.criteria.PushBack({11, 0.6f, false});  // sueurs
                d.criteria.PushBack({2,  0.4f, false});  // dyspnée
            }

            // Pneumonie
            {
                auto& d = AddDis(101, "Pneumonie", 0.7f);
                d.criteria.PushBack({5,  0.8f, true});   // fièvre
                d.criteria.PushBack({13, 0.7f, true});   // toux
                d.criteria.PushBack({2,  0.6f, false});  // dyspnée
                d.criteria.PushBack({14, 0.5f, false});  // douleur pleurétique
                d.criteria.PushBack({6,  0.4f, false});  // fatigue
            }

            // Crise d'anxiété / Attaque de panique
            {
                auto& d = AddDis(102, "Attaque de panique", 0.5f);
                d.criteria.PushBack({9,  0.7f, true});   // palpitations
                d.criteria.PushBack({1,  0.5f, false});  // douleur thoracique
                d.criteria.PushBack({8,  0.6f, false});  // vertiges
                d.criteria.PushBack({2,  0.5f, false});  // dyspnée
            }

            // Appendicite
            {
                auto& d = AddDis(103, "Appendicite", 0.75f);
                d.criteria.PushBack({7,  0.9f, true});   // douleur abdominale
                d.criteria.PushBack({3,  0.6f, false});  // nausée
                d.criteria.PushBack({5,  0.5f, false});  // fièvre
                d.criteria.PushBack({12, 0.4f, false});  // vomissements
            }

            // Migraine
            {
                auto& d = AddDis(104, "Migraine", 0.55f);
                d.criteria.PushBack({4,  0.9f, true});   // céphalée
                d.criteria.PushBack({3,  0.6f, false});  // nausée
                d.criteria.PushBack({8,  0.4f, false});  // vertiges
                d.criteria.PushBack({12, 0.3f, false});  // vomissements
            }
        }

        // =====================================================================
        void NkDiagnosticEngine::AddSymptom(NkSymptomId id) {
            for (auto s : mActiveSymptoms) { if (s == id) return; }
            mActiveSymptoms.PushBack(id);
        }

        void NkDiagnosticEngine::RemoveSymptom(NkSymptomId id) {
            for (nk_isize i = (nk_isize)mActiveSymptoms.Size() - 1; i >= 0; --i) {
                if (mActiveSymptoms[i] == id) { mActiveSymptoms.EraseAt(i); break; }
            }
        }

        void NkDiagnosticEngine::ClearSymptoms() {
            mActiveSymptoms.Clear();
        }

        void NkDiagnosticEngine::SetVitalSigns(nk_float32 hr, nk_float32 temp, nk_float32 spo2) {
            mHeartRate   = hr;
            mTemperature = temp;
            mSpO2        = spo2;
        }

        // =====================================================================
        void NkDiagnosticEngine::Update(NkClinicalState& out) {
            out.activeSymptoms   = mActiveSymptoms;
            out.heartRate        = mHeartRate;
            out.temperature      = mTemperature;
            out.spo2             = mSpO2;

            ComputePhysiologicalLevels(out);
            ComputeDifferential(out);
            out.DeduceEmotion();
        }

        void NkDiagnosticEngine::ComputePhysiologicalLevels(NkClinicalState& state) const {
            state.painLevel    = 0.f;
            state.nauseaLevel  = 0.f;
            state.fatigueLevel = 0.f;
            state.anxietyLevel = 0.f;

            for (auto sid : mActiveSymptoms) {
                const NkSymptomEntry* e = FindSymptom(sid);
                if (!e) continue;
                state.painLevel    = NkMin(state.painLevel    + e->defaultPainContrib,    10.f);
                state.nauseaLevel  = NkMin(state.nauseaLevel  + e->defaultNauseaContrib,   1.f);
                state.fatigueLevel = NkMin(state.fatigueLevel + e->defaultFatigueContrib,  1.f);
                state.anxietyLevel = NkMin(state.anxietyLevel + e->defaultAnxietyContrib,  1.f);
            }

            // Contribution des constantes vitales
            if (mTemperature > 38.5f) {
                state.fatigueLevel = NkMin(state.fatigueLevel + 0.3f, 1.f);
                state.anxietyLevel = NkMin(state.anxietyLevel + 0.1f, 1.f);
            }
            if (mSpO2 < 94.f) {
                state.breathingDifficulty = NkClamp((94.f - mSpO2) / 10.f, 0.f, 1.f);
                state.anxietyLevel = NkMin(state.anxietyLevel + 0.3f, 1.f);
            }
            if (mHeartRate > 100.f) {
                state.anxietyLevel = NkMin(state.anxietyLevel + 0.2f, 1.f);
            }

            state.isTrembling = state.painLevel > 6.f || state.anxietyLevel > 0.7f;
        }

        void NkDiagnosticEngine::ComputeDifferential(NkClinicalState& state) const {
            state.differentialRanking.Clear();

            for (const auto& disease : mDiseases) {
                nk_float32 score   = 0.f;
                nk_float32 maxScore = 0.f;
                bool       requiredMet = true;

                for (const auto& c : disease.criteria) {
                    maxScore += c.weight;
                    bool present = state.HasSymptom(c.id);
                    if (c.required && !present) { requiredMet = false; break; }
                    if (present) score += c.weight;
                }

                if (!requiredMet || maxScore <= 0.f) continue;

                nk_float32 prob = score / maxScore;
                if (prob < 0.05f) continue;

                NkDiagnosisEntry entry;
                entry.diseaseId   = disease.id;
                entry.diseaseName = disease.name;
                entry.probability = prob;
                entry.severity    = disease.baseSeverity * prob;
                state.differentialRanking.PushBack(entry);
            }

            // Tri décroissant par probabilité (insertion sort — liste courte)
            for (nk_isize i = 1; i < (nk_isize)state.differentialRanking.Size(); ++i) {
                NkDiagnosisEntry key = state.differentialRanking[i];
                nk_isize j = i - 1;
                while (j >= 0 && state.differentialRanking[j].probability < key.probability) {
                    state.differentialRanking[j + 1] = state.differentialRanking[j];
                    --j;
                }
                state.differentialRanking[j + 1] = key;
            }
        }

        bool NkDiagnosticEngine::LoadSymptomDatabase(const char* path) {
            (void)path;
            // TODO Phase 5 : parser JSON via NKStream
            logger.Warn("[NkDiagnosticEngine] LoadSymptomDatabase non implémenté (Phase 5)\n");
            return false;
        }

        bool NkDiagnosticEngine::LoadDiseaseDatabase(const char* path) {
            (void)path;
            logger.Warn("[NkDiagnosticEngine] LoadDiseaseDatabase non implémenté (Phase 5)\n");
            return false;
        }

        const NkSymptomEntry* NkDiagnosticEngine::FindSymptom(NkSymptomId id) const {
            for (const auto& s : mSymptoms) { if (s.id == id) return &s; }
            return nullptr;
        }

        const NkDiseaseEntry* NkDiagnosticEngine::FindDisease(NkDiseaseId id) const {
            for (const auto& d : mDiseases) { if (d.id == id) return &d; }
            return nullptr;
        }

    } // namespace pv3de
} // namespace nkentseu
