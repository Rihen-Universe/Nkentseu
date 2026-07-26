#include "NkCaseLoader.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {
	namespace pv3de {

		using namespace nkentseu::math;

		// =====================================================================
		bool NkCaseLoader::Load(const char *path, NkCaseData &out) noexcept {
			// API réelle : NkJSONReader::ReadArchive() est statique et parse du texte
			// JSON déjà en mémoire (pas un chemin fichier) — on lit d'abord le fichier
			// via NkFile::ReadAllText, cf. NKSerialization/JSON/NkJSONReader.h.
			NkString content = NkFile::ReadAllText(path);
			if (content.Empty()) {
				logger.Errorf("[NkCaseLoader] Lecture échouée: {}\n", path);
				return false;
			}

			NkArchive archive;
			NkString err;
			if (!NkJSONReader::ReadArchive(content.View(), archive, &err)) {
				logger.Errorf("[NkCaseLoader] Parse JSON échoué: {} ({})\n", path, err.CStr());
				return false;
			}

			NkString s;
			nk_int64 i64 = 0;

			if (archive.GetString("id", s))
				out.id = s;
			if (archive.GetString("name", s))
				out.name = s;
			if (archive.GetString("author", s))
				out.author = s;
			if (archive.GetInt64("difficulty", i64))
				out.difficulty = (nk_uint32)i64;
			if (archive.GetInt64("correct_diagnosis", i64))
				out.correctDiagnosis = (NkDiseaseId)i64;

			// Patient
			NkArchive patArc;
			if (archive.GetObject("patient", patArc)) {
				if (patArc.GetString("name", s))
					out.patient.name = s;
				if (patArc.GetString("gender", s))
					out.patient.gender = s;
				if (patArc.GetInt64("age", i64))
					out.patient.age = (nk_uint32)i64;
			}

			// Initial state
			NkArchive initArc;
			if (archive.GetObject("initial_state", initArc)) {
				nk_float32 fv = 0.f;
				if (initArc.GetFloat32("heart_rate", fv))
					out.initialState.heartRate = fv;
				if (initArc.GetFloat32("temperature", fv))
					out.initialState.temperature = fv;
				if (initArc.GetFloat32("spo2", fv))
					out.initialState.spo2 = fv;
				if (initArc.GetFloat32("pain_level", fv))
					out.initialState.painLevel = fv;

				nk_int64 symCount = 0;
				(void)initArc.GetInt64("symptom_count", symCount);
				for (nk_int64 k = 0; k < symCount; ++k) {
					NkString key = NkFormat("symptom_{}", k);
					nk_int64 sid = 0;
					if (initArc.GetInt64(key.CStr(), sid))
						out.initialState.symptoms.PushBack((NkSymptomId)sid);
				}
			}

			// Events
			nk_int64 evCount = 0;
			(void)archive.GetInt64("event_count", evCount);
			for (nk_int64 k = 0; k < evCount; ++k) {
				NkString key = NkFormat("event_{}", k);
				NkArchive evArc;
				if (!archive.GetObject(key.CStr(), evArc))
					continue;

				NkCaseEvent ev;
				nk_float32 fv = 0.f;
				NkString sv;
				if (evArc.GetFloat32("time_s", fv))
					ev.timeSeconds = fv;
				if (evArc.GetString("type", sv))
					ev.type = ParseEventType(sv.CStr());
				if (evArc.GetFloat32("value", fv))
					ev.floatValue = fv;
				if (evArc.GetString("value_str", sv))
					ev.stringValue = sv;
				if (evArc.GetInt64("symptom_id", i64))
					ev.symptomId = (NkSymptomId)i64;
				if (evArc.GetFloat32("heart_rate", fv))
					ev.heartRate = fv;
				if (evArc.GetFloat32("temperature", fv))
					ev.temperature = fv;
				if (evArc.GetFloat32("spo2", fv))
					ev.spo2 = fv;
				out.events.PushBack(ev);
			}

			// QA pairs
			nk_int64 qaCount = 0;
			(void)archive.GetInt64("qa_count", qaCount);
			for (nk_int64 k = 0; k < qaCount; ++k) {
				NkString key = NkFormat("qa_{}", k);
				NkArchive qaArc;
				if (!archive.GetObject(key.CStr(), qaArc))
					continue;

				NkQAPair qa;
				if (qaArc.GetString("question", s))
					qa.question = s;
				if (qaArc.GetString("answer", s))
					qa.answer = s;
				nk_float32 fv = 0.f;
				if (qaArc.GetFloat32("emotion_intensity", fv))
					qa.emotionIntensity = fv;
				out.qaPairs.PushBack(qa);
			}

			// Objectives
			nk_int64 objCount = 0;
			(void)archive.GetInt64("objective_count", objCount);
			for (nk_int64 k = 0; k < objCount; ++k) {
				NkString key = NkFormat("obj_{}", k);
				if (archive.GetString(key.CStr(), s))
					out.objectives.PushBack(s);
			}

			out.loaded = true;
			logger.Infof("[NkCaseLoader] Cas chargé: '{}' ({})\n", out.name.CStr(), path);
			return true;
		}

		// =====================================================================
		bool NkCaseLoader::Validate(const char *path) const noexcept {
			NkCaseData tmp;
			NkCaseLoader loader;
			return loader.Load(path, tmp) && !tmp.id.Empty();
		}

		void NkCaseLoader::ScanDirectory(const char *dir, NkVector<NkString> &out) const noexcept {
			// API réelle : NkDirectory expose des méthodes statiques (pas d'instance
			// à Open()) — GetFiles(path, pattern) retourne directement les chemins.
			if (!NkDirectory::Exists(dir))
				return;
			NkVector<NkString> files = NkDirectory::GetFiles(dir, "*.nkcase");
			for (nk_usize i = 0; i < files.Size(); ++i)
				out.PushBack(files[i]);
		}

		bool NkCaseLoader::LoadMeta(const char *path, NkCaseData &out) const noexcept {
			NkString content = NkFile::ReadAllText(path);
			if (content.Empty())
				return false;
			NkArchive archive;
			if (!NkJSONReader::ReadArchive(content.View(), archive))
				return false;
			NkString s;
			nk_int64 i64 = 0;
			if (archive.GetString("id", s))
				out.id = s;
			if (archive.GetString("name", s))
				out.name = s;
			if (archive.GetString("author", s))
				out.author = s;
			if (archive.GetInt64("difficulty", i64))
				out.difficulty = (nk_uint32)i64;
			return true;
		}

		// =====================================================================
		bool NkCaseLoader::Save(const char *path, const NkCaseData &c) const noexcept {
			NkArchive archive;
			archive.SetString("id", c.id.CStr());
			archive.SetString("name", c.name.CStr());
			archive.SetString("author", c.author.CStr());
			archive.SetInt64("difficulty", (nk_int64)c.difficulty);
			archive.SetInt64("correct_diagnosis", (nk_int64)c.correctDiagnosis);

			// Patient
			NkArchive patArc;
			patArc.SetString("name", c.patient.name.CStr());
			patArc.SetString("gender", c.patient.gender.CStr());
			patArc.SetInt64("age", (nk_int64)c.patient.age);
			archive.SetObject("patient", patArc);

			// Initial state
			NkArchive initArc;
			initArc.SetFloat32("heart_rate", c.initialState.heartRate);
			initArc.SetFloat32("temperature", c.initialState.temperature);
			initArc.SetFloat32("spo2", c.initialState.spo2);
			initArc.SetFloat32("pain_level", c.initialState.painLevel);
			initArc.SetInt64("symptom_count", (nk_int64)c.initialState.symptoms.Size());
			for (nk_usize i = 0; i < c.initialState.symptoms.Size(); ++i) {
				NkString k = NkFormat("symptom_{}", i);
				initArc.SetInt64(k.CStr(), (nk_int64)c.initialState.symptoms[i]);
			}
			archive.SetObject("initial_state", initArc);

			// Events
			archive.SetInt64("event_count", (nk_int64)c.events.Size());
			for (nk_usize i = 0; i < c.events.Size(); ++i) {
				const auto &ev = c.events[i];
				NkArchive evArc;
				evArc.SetFloat32("time_s", ev.timeSeconds);
				evArc.SetFloat32("value", ev.floatValue);
				evArc.SetInt64("symptom_id", (nk_int64)ev.symptomId);
				// Type en string
				switch (ev.type) {
					case NkCaseEventType::AddSymptom:
						evArc.SetString("type", "add_symptom");
						break;
					case NkCaseEventType::RemoveSymptom:
						evArc.SetString("type", "remove_symptom");
						break;
					case NkCaseEventType::SetPain:
						evArc.SetString("type", "set_pain");
						break;
					case NkCaseEventType::SetVitals:
						evArc.SetString("type", "set_vitals");
						break;
					case NkCaseEventType::ForceEmotion:
						evArc.SetString("type", "force_emotion");
						break;
					case NkCaseEventType::PlaySpeech:
						evArc.SetString("type", "play_speech");
						break;
					default:
						evArc.SetString("type", "unknown");
						break;
				}
				evArc.SetString("value_str", ev.stringValue.CStr());
				NkString k = NkFormat("event_{}", i);
				archive.SetObject(k.CStr(), evArc);
			}

			// QA
			archive.SetInt64("qa_count", (nk_int64)c.qaPairs.Size());
			for (nk_usize i = 0; i < c.qaPairs.Size(); ++i) {
				NkArchive qa;
				qa.SetString("question", c.qaPairs[i].question.CStr());
				qa.SetString("answer", c.qaPairs[i].answer.CStr());
				qa.SetFloat32("emotion_intensity", c.qaPairs[i].emotionIntensity);
				NkString k = NkFormat("qa_{}", i);
				archive.SetObject(k.CStr(), qa);
			}

			// Objectives
			archive.SetInt64("objective_count", (nk_int64)c.objectives.Size());
			for (nk_usize i = 0; i < c.objectives.Size(); ++i) {
				NkString k = NkFormat("obj_{}", i);
				archive.SetString(k.CStr(), c.objectives[i].CStr());
			}

			// API réelle : NkJSONWriter::WriteArchive() est statique et retourne le
			// texte JSON en mémoire (pas d'écriture fichier intégrée) — on écrit
			// nous-mêmes via NkFile::WriteAllText, cf. NKSerialization/JSON/NkJSONWriter.h.
			NkString json = NkJSONWriter::WriteArchive(archive, true);
			return NkFile::WriteAllText(path, json);
		}

		// =====================================================================
		void NkCaseLoader::GetPendingEvents(const NkCaseData &c, nk_float32 prev, nk_float32 curr,
											NkVector<const NkCaseEvent *> &out) const noexcept {
			for (nk_usize i = 0; i < c.events.Size(); ++i) {
				nk_float32 t = c.events[i].timeSeconds;
				if (t >= prev && t < curr)
					out.PushBack(&c.events[i]);
			}
		}

		const NkQAPair *NkCaseLoader::FindAnswer(const NkCaseData &c, const char *question) const noexcept {
			if (!question || !*question)
				return nullptr;
			NkString q(question);
			// Normalisation : minuscules
			// (NkString.ToLower() si disponible, sinon comparison directe)
			for (nk_usize i = 0; i < c.qaPairs.Size(); ++i) {
				// Vérifier si un des mots-clés de la question est dans la question posée
				const NkString &keys = c.qaPairs[i].question;
				// Tokeniser par ','
				const char *p = keys.CStr();
				while (p && *p) {
					const char *comma = strchr(p, ',');
					nk_usize len = comma ? (nk_usize)(comma - p) : strlen(p);
					char keyword[64] = {};
					nk_usize copyLen = NkMin(len, (nk_usize)63);
					strncpy(keyword, p, copyLen);
					keyword[copyLen] = '\0';
					if (q.Contains(keyword))
						return &c.qaPairs[i];
					p = comma ? comma + 1 : nullptr;
				}
			}
			return nullptr;
		}

		NkCaseEventType NkCaseLoader::ParseEventType(const char *type) const noexcept {
			if (strcmp(type, "add_symptom") == 0)
				return NkCaseEventType::AddSymptom;
			if (strcmp(type, "remove_symptom") == 0)
				return NkCaseEventType::RemoveSymptom;
			if (strcmp(type, "set_pain") == 0)
				return NkCaseEventType::SetPain;
			if (strcmp(type, "set_vitals") == 0)
				return NkCaseEventType::SetVitals;
			if (strcmp(type, "force_emotion") == 0)
				return NkCaseEventType::ForceEmotion;
			if (strcmp(type, "play_speech") == 0)
				return NkCaseEventType::PlaySpeech;
			if (strcmp(type, "set_breath") == 0)
				return NkCaseEventType::SetBreathPattern;
			return NkCaseEventType::AddSymptom;
		}

		// =====================================================================
		// NkCaseRunner
		// =====================================================================
		void NkCaseRunner::Load(const NkCaseData *c) noexcept {
			mCase = c;
			mElapsed = 0.f;
			mPrev = 0.f;
			mRunning = (c != nullptr);
			mFinished = false;
			mPendingEvents.Clear();
		}

		void NkCaseRunner::Reset() noexcept {
			mElapsed = 0.f;
			mPrev = 0.f;
			mFinished = false;
			mPendingEvents.Clear();
		}

		const NkVector<const NkCaseEvent *> &NkCaseRunner::Update(nk_float32 dt, const NkCaseLoader &loader) noexcept {
			mPendingEvents.Clear();
			if (!IsRunning() || !mCase)
				return mPendingEvents;

			mPrev = mElapsed;
			mElapsed += dt;

			loader.GetPendingEvents(*mCase, mPrev, mElapsed, mPendingEvents);

			// Vérifier si tous les événements ont été déclenchés
			if (!mCase->events.IsEmpty()) {
				nk_float32 lastEvt = mCase->events[mCase->events.Size() - 1].timeSeconds;
				if (mElapsed > lastEvt + 30.f)
					mFinished = true;
			}

			return mPendingEvents;
		}

	} // namespace pv3de
} // namespace nkentseu
