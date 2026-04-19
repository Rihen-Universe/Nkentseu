#include "NkCaseLoader.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        bool NkCaseLoader::Load(const char* path, NkCaseData& out) noexcept {
            NkJSONReader reader;
            NkArchive    archive;
            if (!reader.Read(archive, path)) {
                logger.Errorf("[NkCaseLoader] Lecture échouée: {}\n", path);
                return false;
            }

            NkString s;
            nk_int64 i64 = 0;

            if (archive.GetString("id",         s)) out.id         = s;
            if (archive.GetString("name",        s)) out.name       = s;
            if (archive.GetString("author",      s)) out.author     = s;
            if (archive.GetInt   ("difficulty",  i64)) out.difficulty = (nk_uint32)i64;
            if (archive.GetInt   ("correct_diagnosis", i64)) out.correctDiagnosis = (NkDiseaseId)i64;

            // Patient
            NkArchive patArc;
            if (archive.GetNode("patient", patArc)) {
                if (patArc.GetString("name",   s)) out.patient.name   = s;
                if (patArc.GetString("gender", s)) out.patient.gender = s;
                if (patArc.GetInt   ("age",    i64)) out.patient.age  = (nk_uint32)i64;
            }

            // Initial state
            NkArchive initArc;
            if (archive.GetNode("initial_state", initArc)) {
                nk_float32 fv = 0.f;
                if (initArc.GetFloat("heart_rate",   fv)) out.initialState.heartRate   = fv;
                if (initArc.GetFloat("temperature",  fv)) out.initialState.temperature = fv;
                if (initArc.GetFloat("spo2",         fv)) out.initialState.spo2        = fv;
                if (initArc.GetFloat("pain_level",   fv)) out.initialState.painLevel   = fv;

                nk_int64 symCount = 0;
                initArc.GetInt("symptom_count", symCount);
                for (nk_int64 k = 0; k < symCount; ++k) {
                    NkString key = NkFormat("symptom_{}", k);
                    nk_int64 sid = 0;
                    if (initArc.GetInt(key.CStr(), sid))
                        out.initialState.symptoms.PushBack((NkSymptomId)sid);
                }
            }

            // Events
            nk_int64 evCount = 0;
            archive.GetInt("event_count", evCount);
            for (nk_int64 k = 0; k < evCount; ++k) {
                NkString key = NkFormat("event_{}", k);
                NkArchive evArc;
                if (!archive.GetNode(key.CStr(), evArc)) continue;

                NkCaseEvent ev;
                nk_float32 fv = 0.f;
                NkString   sv;
                if (evArc.GetFloat ("time_s", fv)) ev.timeSeconds = fv;
                if (evArc.GetString("type",   sv)) ev.type = ParseEventType(sv.CStr());
                if (evArc.GetFloat ("value",  fv)) ev.floatValue = fv;
                if (evArc.GetString("value_str", sv)) ev.stringValue = sv;
                if (evArc.GetInt   ("symptom_id", i64)) ev.symptomId = (NkSymptomId)i64;
                if (evArc.GetFloat ("heart_rate",   fv)) ev.heartRate   = fv;
                if (evArc.GetFloat ("temperature",  fv)) ev.temperature = fv;
                if (evArc.GetFloat ("spo2",         fv)) ev.spo2        = fv;
                out.events.PushBack(ev);
            }

            // QA pairs
            nk_int64 qaCount = 0;
            archive.GetInt("qa_count", qaCount);
            for (nk_int64 k = 0; k < qaCount; ++k) {
                NkString key = NkFormat("qa_{}", k);
                NkArchive qaArc;
                if (!archive.GetNode(key.CStr(), qaArc)) continue;

                NkQAPair qa;
                if (qaArc.GetString("question", s)) qa.question = s;
                if (qaArc.GetString("answer",   s)) qa.answer   = s;
                nk_float32 fv = 0.f;
                if (qaArc.GetFloat("emotion_intensity", fv)) qa.emotionIntensity = fv;
                out.qaPairs.PushBack(qa);
            }

            // Objectives
            nk_int64 objCount = 0;
            archive.GetInt("objective_count", objCount);
            for (nk_int64 k = 0; k < objCount; ++k) {
                NkString key = NkFormat("obj_{}", k);
                if (archive.GetString(key.CStr(), s)) out.objectives.PushBack(s);
            }

            out.loaded = true;
            logger.Infof("[NkCaseLoader] Cas chargé: '{}' ({})\n",
                         out.name.CStr(), path);
            return true;
        }

        // =====================================================================
        bool NkCaseLoader::Validate(const char* path) const noexcept {
            NkCaseData tmp;
            NkCaseLoader loader;
            return loader.Load(path, tmp) && !tmp.id.IsEmpty();
        }

        void NkCaseLoader::ScanDirectory(const char* dir,
                                          NkVector<NkString>& out) const noexcept {
            NkDirectory d;
            if (!d.Open(dir)) return;
            NkVector<NkString> files;
            d.GetFiles(files, ".nkcase");
            for (nk_usize i = 0; i < files.Size(); ++i)
                out.PushBack(files[i]);
        }

        bool NkCaseLoader::LoadMeta(const char* path, NkCaseData& out) const noexcept {
            NkJSONReader reader;
            NkArchive    archive;
            if (!reader.Read(archive, path)) return false;
            NkString s;
            nk_int64 i64 = 0;
            if (archive.GetString("id",         s)) out.id         = s;
            if (archive.GetString("name",        s)) out.name       = s;
            if (archive.GetString("author",      s)) out.author     = s;
            if (archive.GetInt   ("difficulty", i64)) out.difficulty = (nk_uint32)i64;
            return true;
        }

        // =====================================================================
        bool NkCaseLoader::Save(const char* path, const NkCaseData& c) const noexcept {
            NkArchive archive;
            archive.SetString("id",          c.id.CStr());
            archive.SetString("name",        c.name.CStr());
            archive.SetString("author",      c.author.CStr());
            archive.SetInt   ("difficulty",  (nk_int64)c.difficulty);
            archive.SetInt   ("correct_diagnosis", (nk_int64)c.correctDiagnosis);

            // Patient
            NkArchive patArc;
            patArc.SetString("name",   c.patient.name.CStr());
            patArc.SetString("gender", c.patient.gender.CStr());
            patArc.SetInt   ("age",    (nk_int64)c.patient.age);
            archive.SetNode ("patient", patArc);

            // Initial state
            NkArchive initArc;
            initArc.SetFloat("heart_rate",  c.initialState.heartRate);
            initArc.SetFloat("temperature", c.initialState.temperature);
            initArc.SetFloat("spo2",        c.initialState.spo2);
            initArc.SetFloat("pain_level",  c.initialState.painLevel);
            initArc.SetInt  ("symptom_count", (nk_int64)c.initialState.symptoms.Size());
            for (nk_usize i = 0; i < c.initialState.symptoms.Size(); ++i) {
                NkString k = NkFormat("symptom_{}", i);
                initArc.SetInt(k.CStr(), (nk_int64)c.initialState.symptoms[i]);
            }
            archive.SetNode("initial_state", initArc);

            // Events
            archive.SetInt("event_count", (nk_int64)c.events.Size());
            for (nk_usize i = 0; i < c.events.Size(); ++i) {
                const auto& ev = c.events[i];
                NkArchive evArc;
                evArc.SetFloat("time_s", ev.timeSeconds);
                evArc.SetFloat("value",  ev.floatValue);
                evArc.SetInt  ("symptom_id", (nk_int64)ev.symptomId);
                // Type en string
                switch (ev.type) {
                    case NkCaseEventType::AddSymptom:    evArc.SetString("type","add_symptom");    break;
                    case NkCaseEventType::RemoveSymptom: evArc.SetString("type","remove_symptom"); break;
                    case NkCaseEventType::SetPain:       evArc.SetString("type","set_pain");       break;
                    case NkCaseEventType::SetVitals:     evArc.SetString("type","set_vitals");     break;
                    case NkCaseEventType::ForceEmotion:  evArc.SetString("type","force_emotion");  break;
                    case NkCaseEventType::PlaySpeech:    evArc.SetString("type","play_speech");    break;
                    default:                             evArc.SetString("type","unknown");        break;
                }
                evArc.SetString("value_str", ev.stringValue.CStr());
                NkString k = NkFormat("event_{}", i);
                archive.SetNode(k.CStr(), evArc);
            }

            // QA
            archive.SetInt("qa_count", (nk_int64)c.qaPairs.Size());
            for (nk_usize i = 0; i < c.qaPairs.Size(); ++i) {
                NkArchive qa;
                qa.SetString("question", c.qaPairs[i].question.CStr());
                qa.SetString("answer",   c.qaPairs[i].answer.CStr());
                qa.SetFloat ("emotion_intensity", c.qaPairs[i].emotionIntensity);
                NkString k = NkFormat("qa_{}", i);
                archive.SetNode(k.CStr(), qa);
            }

            // Objectives
            archive.SetInt("objective_count", (nk_int64)c.objectives.Size());
            for (nk_usize i = 0; i < c.objectives.Size(); ++i) {
                NkString k = NkFormat("obj_{}", i);
                archive.SetString(k.CStr(), c.objectives[i].CStr());
            }

            NkJSONWriter writer;
            return writer.Write(archive, path);
        }

        // =====================================================================
        void NkCaseLoader::GetPendingEvents(const NkCaseData& c,
                                             nk_float32 prev,
                                             nk_float32 curr,
                                             NkVector<const NkCaseEvent*>& out) const noexcept {
            for (nk_usize i = 0; i < c.events.Size(); ++i) {
                nk_float32 t = c.events[i].timeSeconds;
                if (t >= prev && t < curr)
                    out.PushBack(&c.events[i]);
            }
        }

        const NkQAPair* NkCaseLoader::FindAnswer(const NkCaseData& c,
                                                  const char* question) const noexcept {
            if (!question || !*question) return nullptr;
            NkString q(question);
            // Normalisation : minuscules
            // (NkString.ToLower() si disponible, sinon comparison directe)
            for (nk_usize i = 0; i < c.qaPairs.Size(); ++i) {
                // Vérifier si un des mots-clés de la question est dans la question posée
                const NkString& keys = c.qaPairs[i].question;
                // Tokeniser par ','
                const char* p = keys.CStr();
                while (p && *p) {
                    const char* comma = NkStrChr(p, ',');
                    nk_usize len = comma ? (nk_usize)(comma - p) : NkStrLen(p);
                    char keyword[64] = {};
                    NkStrNCpy(keyword, p, NkMin(len, (nk_usize)63));
                    if (q.Contains(keyword)) return &c.qaPairs[i];
                    p = comma ? comma + 1 : nullptr;
                }
            }
            return nullptr;
        }

        NkCaseEventType NkCaseLoader::ParseEventType(const char* type) const noexcept {
            if (NkStrEqual(type, "add_symptom"))    return NkCaseEventType::AddSymptom;
            if (NkStrEqual(type, "remove_symptom")) return NkCaseEventType::RemoveSymptom;
            if (NkStrEqual(type, "set_pain"))       return NkCaseEventType::SetPain;
            if (NkStrEqual(type, "set_vitals"))     return NkCaseEventType::SetVitals;
            if (NkStrEqual(type, "force_emotion"))  return NkCaseEventType::ForceEmotion;
            if (NkStrEqual(type, "play_speech"))    return NkCaseEventType::PlaySpeech;
            if (NkStrEqual(type, "set_breath"))     return NkCaseEventType::SetBreathPattern;
            return NkCaseEventType::AddSymptom;
        }

        // =====================================================================
        // NkCaseRunner
        // =====================================================================
        void NkCaseRunner::Load(const NkCaseData* c) noexcept {
            mCase    = c;
            mElapsed = 0.f;
            mPrev    = 0.f;
            mRunning = (c != nullptr);
            mFinished= false;
            mPendingEvents.Clear();
        }

        void NkCaseRunner::Reset() noexcept {
            mElapsed = 0.f;
            mPrev    = 0.f;
            mFinished= false;
            mPendingEvents.Clear();
        }

        const NkVector<const NkCaseEvent*>& NkCaseRunner::Update(
            nk_float32 dt,
            const NkCaseLoader& loader) noexcept {
            mPendingEvents.Clear();
            if (!IsRunning() || !mCase) return mPendingEvents;

            mPrev     = mElapsed;
            mElapsed += dt;

            loader.GetPendingEvents(*mCase, mPrev, mElapsed, mPendingEvents);

            // Vérifier si tous les événements ont été déclenchés
            if (!mCase->events.IsEmpty()) {
                nk_float32 lastEvt = mCase->events[mCase->events.Size()-1].timeSeconds;
                if (mElapsed > lastEvt + 30.f) mFinished = true;
            }

            return mPendingEvents;
        }

    } // namespace pv3de
} // namespace nkentseu
