// =============================================================================
// NKRenderer/Tools/Director/NkRoleContext.cpp — implémentation (voir .h)
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKRenderer/Tools/Director/NkRoleContext.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKMath/NKMath.h"
#include <cmath>

namespace nkentseu {
	namespace renderer {

		// ── Table nom <-> émotion (ordre = valeurs de l'énum) ───────────────────────
		static const char *kEmotionNames[(uint8)NkEmotion::NK_EMOTION_COUNT] = {
			"neutral", "joy", "sadness", "anger", "fear", "surprise", "disgust",
		};

		const char *NkEmotionToString(NkEmotion e) {
			const uint8 i = (uint8)e;
			if (i >= (uint8)NkEmotion::NK_EMOTION_COUNT)
				return "?";
			return kEmotionNames[i];
		}

		// Comparaison ASCII insensible à la casse (pas de dépendance STL).
		static bool EqualsIgnoreCase(const NkString &a, const char *b) {
			const char *pa = a.CStr();
			nk_size i = 0;
			for (; pa[i] != '\0' && b[i] != '\0'; ++i) {
				char ca = pa[i], cb = b[i];
				if (ca >= 'A' && ca <= 'Z')
					ca = (char)(ca - 'A' + 'a');
				if (cb >= 'A' && cb <= 'Z')
					cb = (char)(cb - 'A' + 'a');
				if (ca != cb)
					return false;
			}
			return pa[i] == '\0' && b[i] == '\0';
		}

		NkEmotion NkEmotionFromString(const NkString &name, bool &ok) {
			for (uint8 i = 0; i < (uint8)NkEmotion::NK_EMOTION_COUNT; ++i) {
				if (EqualsIgnoreCase(name, kEmotionNames[i])) {
					ok = true;
					return (NkEmotion)i;
				}
			}
			ok = false;
			return NkEmotion::NK_EMOTION_NEUTRAL;
		}

		// ── NkRoleContext ────────────────────────────────────────────────────────

		void NkRoleContext::PushHistory(const NkString &text, float32 timeStamp) {
			NkHistoryEvent ev;
			ev.text = text;
			ev.timestamp = timeStamp;
			history.PushBack(ev);
			if (maxHistory == 0)
				return;
			// FIFO : retire le(s) plus ancien(s) tant qu'on dépasse la capacité.
			while ((uint32)history.Size() > maxHistory)
				history.Erase(history.Begin());
		}

		NkArchive NkRoleContext::ToArchive() const {
			NkArchive arc;
			arc.SetString("role", roleName.View());

			NkVector<NkArchive> traitArr;
			for (nk_size i = 0; i < traits.Size(); ++i) {
				NkArchive t;
				t.SetString("name", traits[i].name.View());
				t.SetFloat32("value", traits[i].value);
				traitArr.PushBack(t);
			}
			arc.SetObjectArray("traits", traitArr);

			NkArchive emo;
			emo.SetString("primary", NkStringView(NkEmotionToString(emotion)));
			emo.SetFloat32("intensity", emotionIntensity);
			arc.SetObject("emotion", emo);

			NkArchive obj;
			obj.SetString("description", objective.description.View());
			obj.SetInt32("priority", objective.priority);
			obj.SetFloat32("urgency", objective.urgency);
			arc.SetObject("objective", obj);

			NkVector<NkArchive> histArr;
			for (nk_size i = 0; i < history.Size(); ++i) {
				NkArchive h;
				h.SetString("text", history[i].text.View());
				h.SetFloat32("timestamp", history[i].timestamp);
				histArr.PushBack(h);
			}
			arc.SetObjectArray("history", histArr);
			arc.SetUInt32("maxHistory", maxHistory);

			return arc;
		}

		bool NkRoleContext::FromArchive(const NkArchive &arc, NkString *outError) {
			NkString err;
			if (!NkRoleContextSchema::Validate(arc, err)) {
				if (outError)
					*outError = err;
				return false;
			}

			roleName.Clear();
			(void)arc.GetString("role", roleName);

			traits.Clear();
			NkVector<NkArchive> traitArr;
			if (arc.GetObjectArray("traits", traitArr)) {
				for (nk_size i = 0; i < traitArr.Size(); ++i) {
					NkPersonalityTrait t;
					(void)traitArr[i].GetString("name", t.name);
					float32 v = 0.f;
					(void)traitArr[i].GetFloat32("value", v);
					t.value = v;
					traits.PushBack(t);
				}
			}

			NkArchive emo;
			if (arc.GetObject("emotion", emo)) {
				NkString primName;
				(void)emo.GetString("primary", primName);
				bool ok = false;
				emotion = NkEmotionFromString(primName, ok);
				float32 inten = 0.f;
				(void)emo.GetFloat32("intensity", inten);
				emotionIntensity = inten;
			}

			NkArchive obj;
			if (arc.GetObject("objective", obj)) {
				(void)obj.GetString("description", objective.description);
				int32 prio = 0;
				(void)obj.GetInt32("priority", prio);
				objective.priority = prio;
				float32 urg = 0.f;
				(void)obj.GetFloat32("urgency", urg);
				objective.urgency = urg;
			}

			history.Clear();
			NkVector<NkArchive> histArr;
			if (arc.GetObjectArray("history", histArr)) {
				for (nk_size i = 0; i < histArr.Size(); ++i) {
					NkHistoryEvent h;
					(void)histArr[i].GetString("text", h.text);
					float32 ts = 0.f;
					(void)histArr[i].GetFloat32("timestamp", ts);
					h.timestamp = ts;
					history.PushBack(h);
				}
			}

			uint32 mh = 8;
			(void)arc.GetUInt32("maxHistory", mh);
			maxHistory = mh;

			return true;
		}

		NkString NkRoleContext::ToJSON(bool pretty) const {
			const NkArchive arc = ToArchive();
			return NkJSONWriter::WriteArchive(arc, pretty);
		}

		bool NkRoleContext::FromJSON(const NkString &json, NkString *outError) {
			NkArchive arc;
			NkString parseErr;
			if (!NkJSONReader::ReadArchive(json.View(), arc, &parseErr)) {
				if (outError)
					*outError = parseErr;
				return false;
			}
			return FromArchive(arc, outError);
		}

		// ── NkRoleContextSchema ──────────────────────────────────────────────────

		static bool GetF32InRange(const NkArchive &a, const char *key, float32 lo, float32 hi, float32 &out,
								   NkString &err) {
			float32 v = 0.f;
			if (!a.GetFloat32(key, v)) {
				err = NkString::Fmtf("champ numerique manquant/invalide : %s", key);
				return false;
			}
			if (v < lo || v > hi) {
				err = NkString::Fmtf("%s hors bornes [%.2f,%.2f] : %.4f", key, lo, hi, v);
				return false;
			}
			out = v;
			return true;
		}

		bool NkRoleContextSchema::Validate(const NkArchive &arc, NkString &outError) {
			// role : string non vide
			NkString role;
			if (!arc.GetString("role", role) || role.Empty()) {
				outError = NkString("champ 'role' manquant ou vide");
				return false;
			}

			// emotion : objet {primary:string connu, intensity:[0,1]}
			NkArchive emo;
			if (!arc.GetObject("emotion", emo)) {
				outError = NkString("objet 'emotion' manquant");
				return false;
			}
			NkString primary;
			if (!emo.GetString("primary", primary) || primary.Empty()) {
				outError = NkString("champ 'emotion.primary' manquant ou vide");
				return false;
			}
			bool emoOk = false;
			NkEmotionFromString(primary, emoOk);
			if (!emoOk) {
				outError = NkString::Fmtf("emotion.primary inconnue (hors schema) : %s", primary.CStr());
				return false;
			}
			float32 tmp = 0.f;
			if (!GetF32InRange(emo, "intensity", 0.f, 1.f, tmp, outError))
				return false;

			// objective : objet {description:string non vide, priority:entier>=0, urgency:[0,1]}
			NkArchive obj;
			if (!arc.GetObject("objective", obj)) {
				outError = NkString("objet 'objective' manquant");
				return false;
			}
			NkString desc;
			if (!obj.GetString("description", desc) || desc.Empty()) {
				outError = NkString("champ 'objective.description' manquant ou vide");
				return false;
			}
			int32 prio = 0;
			if (!obj.GetInt32("priority", prio) || prio < 0) {
				outError = NkString("champ 'objective.priority' manquant ou negatif");
				return false;
			}
			if (!GetF32InRange(obj, "urgency", 0.f, 1.f, tmp, outError))
				return false;

			// traits[] (optionnel) : chaque element {name:string non vide, value:[0,1]}
			NkVector<NkArchive> traitArr;
			if (arc.GetObjectArray("traits", traitArr)) {
				for (nk_size i = 0; i < traitArr.Size(); ++i) {
					NkString tn;
					if (!traitArr[i].GetString("name", tn) || tn.Empty()) {
						outError = NkString::Fmtf("traits[%zu].name manquant ou vide", (nk_size)i);
						return false;
					}
					if (!GetF32InRange(traitArr[i], "value", 0.f, 1.f, tmp, outError)) {
						outError = NkString::Fmtf("traits[%zu].%s", (nk_size)i, outError.CStr());
						return false;
					}
				}
			}

			// history[] (optionnel) : chaque element {text:string, timestamp:nombre}
			NkVector<NkArchive> histArr;
			if (arc.GetObjectArray("history", histArr)) {
				for (nk_size i = 0; i < histArr.Size(); ++i) {
					NkString txt;
					if (!histArr[i].GetString("text", txt)) {
						outError = NkString::Fmtf("history[%zu].text manquant", (nk_size)i);
						return false;
					}
					float64 ts = 0.0;
					if (!histArr[i].GetFloat64("timestamp", ts)) {
						outError = NkString::Fmtf("history[%zu].timestamp manquant/invalide", (nk_size)i);
						return false;
					}
				}
			}

			return true;
		}

		// ── Auto-test headless ───────────────────────────────────────────────────

		bool NkRoleContext::SelfTest() {
			// 1) Construction d'un contexte VALIDE.
			NkRoleContext ctx;
			ctx.roleName = NkString("Garde du village");
			ctx.traits.PushBack(NkPersonalityTrait{NkString("bravoure"), 0.8f});
			ctx.traits.PushBack(NkPersonalityTrait{NkString("loyaute"), 0.6f});
			ctx.emotion = NkEmotion::NK_EMOTION_ANGER;
			ctx.emotionIntensity = 0.7f;
			ctx.objective.description = NkString("Empecher l'intrus d'entrer");
			ctx.objective.priority = 1;
			ctx.objective.urgency = 0.9f;
			ctx.maxHistory = 2;
			ctx.PushHistory(NkString("L'intrus approche du portail"), 0.f);
			ctx.PushHistory(NkString("L'intrus force le portail"), 1.5f);
			ctx.PushHistory(NkString("L'intrus est entre"), 3.0f); // doit ejecter le plus ancien

			// FIFO d'historique : taille bornee a maxHistory, le plus ancien est parti.
			if (ctx.history.Size() != 2)
				return false;
			if (ctx.history[0].text != NkString("L'intrus force le portail"))
				return false;
			if (ctx.history[1].text != NkString("L'intrus est entre"))
				return false;

			// 2) Round-trip via Archive.
			NkArchive arc = ctx.ToArchive();
			NkRoleContext fromArc;
			NkString err;
			if (!fromArc.FromArchive(arc, &err))
				return false;
			if (fromArc.roleName != ctx.roleName)
				return false;
			if (fromArc.traits.Size() != 2)
				return false;
			if (fromArc.emotion != NkEmotion::NK_EMOTION_ANGER)
				return false;
			if (fromArc.objective.priority != 1)
				return false;
			if (fromArc.history.Size() != 2)
				return false;

			// 3) Round-trip via JSON TEXTE (forme que produirait/consommerait le futur
			// pont directeur). Verifie que le texte JSON reparse a l'identique.
			NkString json = ctx.ToJSON(true);
			if (json.Empty())
				return false;
			NkRoleContext fromJson;
			if (!fromJson.FromJSON(json, &err))
				return false;
			if (fromJson.roleName != ctx.roleName)
				return false;
			if (fromJson.emotion != ctx.emotion)
				return false;
			if (fromJson.history.Size() != ctx.history.Size())
				return false;
			if (fabsf(fromJson.objective.urgency - ctx.objective.urgency) > 1e-4f)
				return false;

			// 4) Le schema ACCEPTE l'archive bien formee.
			NkString validErr;
			if (!NkRoleContextSchema::Validate(arc, validErr))
				return false;

			// 5) Le schema REJETTE des variantes malformees (simulerait une sortie
			// LLM invalide) — chaque cas doit echouer AVEC un message d'erreur.
			{
				NkArchive bad = arc;
				bad.Remove("role");
				NkString e;
				if (NkRoleContextSchema::Validate(bad, e) || e.Empty())
					return false;
			}
			{
				NkArchive bad = arc;
				NkArchive emo;
				(void)bad.GetObject("emotion", emo);
				emo.SetFloat32("intensity", 1.7f); // hors [0,1]
				bad.SetObject("emotion", emo);
				NkString e;
				if (NkRoleContextSchema::Validate(bad, e) || e.Empty())
					return false;
			}
			{
				NkArchive bad = arc;
				NkArchive emo;
				(void)bad.GetObject("emotion", emo);
				emo.SetString("primary", NkStringView("furieux")); // emotion hors schema
				bad.SetObject("emotion", emo);
				NkString e;
				if (NkRoleContextSchema::Validate(bad, e) || e.Empty())
					return false;
			}
			{
				NkArchive bad = arc;
				NkArchive obj;
				(void)bad.GetObject("objective", obj);
				obj.SetInt32("priority", -3); // negatif
				bad.SetObject("objective", obj);
				NkString e;
				if (NkRoleContextSchema::Validate(bad, e) || e.Empty())
					return false;
			}
			{
				// JSON texte carrement malforme (pas du JSON valide) : FromJSON doit
				// echouer proprement (pas de crash), jamais interprete comme du texte
				// libre injectable.
				NkRoleContext junk;
				NkString jerr;
				if (junk.FromJSON(NkString("ceci n'est pas du JSON { trucs libres"), &jerr))
					return false;
				if (jerr.Empty())
					return false;
			}

			return true;
		}

	} // namespace renderer
} // namespace nkentseu
