#include "NkFHIRExport.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"
#include <stdio.h>
#include <string.h>

namespace nkentseu {
    namespace pv3de {

        // =====================================================================
        NkString NkFHIRExport::EscapeJson(const NkString& s) {
            NkString out;
            for (nk_usize i = 0; i < s.Length(); ++i) {
                char c = s[i];
                if      (c == '"')  out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else                out += c;
            }
            return out;
        }

        NkString NkFHIRExport::FloatToStr(nk_float32 v, int dec) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.*f", dec, (double)v);
            return NkString(buf);
        }

        // =====================================================================
        NkString NkFHIRExport::GenerateReport(const NkClinicalState& state) const {
            // Bundle FHIR R4 minimaliste
            NkString bundle = "{\n";
            bundle += "  \"resourceType\": \"Bundle\",\n";
            bundle += "  \"type\": \"collection\",\n";
            bundle += "  \"entry\": [\n";

            // Patient
            bundle += "    { \"resource\": ";
            bundle += BuildPatientResource();
            bundle += " },\n";

            // Observations
            bundle += BuildObservations(state);

            // Conditions
            bundle += BuildConditions(state);

            // Rapport diagnostic
            bundle += "    { \"resource\": ";
            bundle += BuildDiagnosticReport(state);
            bundle += " }\n";

            bundle += "  ]\n";
            bundle += "}\n";
            return bundle;
        }

        // =====================================================================
        NkString NkFHIRExport::BuildPatientResource() const {
            NkString r = "{\n";
            r += "      \"resourceType\": \"Patient\",\n";
            r += "      \"id\": \"" + EscapeJson(
                mPatient.id.IsEmpty() ? NkString("pv3de-001") : mPatient.id) + "\",\n";
            r += "      \"name\": [{ \"family\": \"" +
                 EscapeJson(mPatient.lastName) + "\", \"given\": [\"" +
                 EscapeJson(mPatient.firstName) + "\"] }],\n";
            char age[32];
            snprintf(age, sizeof(age), "%u", mPatient.ageYears);
            r += "      \"gender\": \"" + EscapeJson(mPatient.gender) + "\"\n";
            r += "    }";
            return r;
        }

        NkString NkFHIRExport::BuildObservations(const NkClinicalState& s) const {
            NkString out;

            auto AddObs = [&](const char* code, const char* display,
                               const char* unit, nk_float32 val) {
                char buf[512];
                snprintf(buf, sizeof(buf),
                    "    { \"resource\": {\n"
                    "      \"resourceType\": \"Observation\",\n"
                    "      \"status\": \"final\",\n"
                    "      \"code\": { \"coding\": [{ \"code\": \"%s\", \"display\": \"%s\" }] },\n"
                    "      \"valueQuantity\": { \"value\": %.2f, \"unit\": \"%s\" }\n"
                    "    }},\n",
                    code, display, (double)val, unit);
                out += NkString(buf);
            };

            AddObs("8867-4",  "Heart rate",    "bpm", s.heartRate);
            AddObs("8310-5",  "Body temperature", "Cel", s.temperature);
            AddObs("59408-5", "Oxygen saturation", "%",  s.spo2);
            AddObs("72514-3", "Pain severity",  "/10", s.painLevel);

            return out;
        }

        NkString NkFHIRExport::BuildConditions(const NkClinicalState& s) const {
            NkString out;
            for (const auto& d : s.differentialRanking) {
                if (d.probability < 0.30f) continue;
                char buf[512];
                const char* certainty = (d.probability >= 0.70f) ? "confirmed"
                                      : (d.probability >= 0.45f) ? "probable"
                                      : "possible";
                snprintf(buf, sizeof(buf),
                    "    { \"resource\": {\n"
                    "      \"resourceType\": \"Condition\",\n"
                    "      \"code\": { \"text\": \"%s\" },\n"
                    "      \"verificationStatus\": { \"coding\": [{ \"code\": \"%s\" }] },\n"
                    "      \"extension\": [{ \"url\": \"probability\", \"valueDecimal\": %.2f }]\n"
                    "    }},\n",
                    d.diseaseName.CStr(), certainty, (double)d.probability);
                out += NkString(buf);
            }
            return out;
        }

        NkString NkFHIRExport::BuildDiagnosticReport(const NkClinicalState& s) const {
            NkString r = "{\n";
            r += "      \"resourceType\": \"DiagnosticReport\",\n";
            r += "      \"status\": \"preliminary\",\n";
            r += "      \"conclusion\": \"";

            if (!s.differentialRanking.IsEmpty()) {
                const auto& top = s.differentialRanking[0];
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "Hypothèse principale: %s (%.0f%%). ",
                    top.diseaseName.CStr(), (double)(top.probability * 100.f));
                r += EscapeJson(NkString(buf));
            }

            char pain[64];
            snprintf(pain, sizeof(pain),
                "Douleur EVA: %.1f/10. FC: %.0f bpm. SpO2: %.0f%%.",
                (double)s.painLevel, (double)s.heartRate, (double)s.spo2);
            r += EscapeJson(NkString(pain));
            r += "\"\n    }";
            return r;
        }

        // =====================================================================
        NkString NkFHIRExport::GenerateSummary(const NkClinicalState& s) const {
            NkString out = "=== RAPPORT CLINIQUE PV3DE ===\n\n";

            out += "PATIENT: " + mPatient.firstName + " " + mPatient.lastName;
            char age[32];
            snprintf(age, sizeof(age), " | %u ans | %s\n\n",
                     mPatient.ageYears, mPatient.gender.CStr());
            out += NkString(age);

            out += "CONSTANTES VITALES:\n";
            char vitals[256];
            snprintf(vitals, sizeof(vitals),
                "  FC: %.0f bpm | Temp: %.1f°C | SpO2: %.0f%%\n"
                "  Douleur (EVA): %.1f/10\n\n",
                (double)s.heartRate, (double)s.temperature,
                (double)s.spo2,     (double)s.painLevel);
            out += NkString(vitals);

            out += "DIAGNOSTIC DIFFERENTIEL:\n";
            for (nk_uint32 i = 0; i < s.differentialRanking.Size() && i < 5; ++i) {
                const auto& d = s.differentialRanking[i];
                char line[256];
                snprintf(line, sizeof(line), "  %d. %-30s %.0f%%\n",
                         i + 1, d.diseaseName.CStr(), (double)(d.probability * 100.f));
                out += NkString(line);
            }

            if (s.differentialRanking.IsEmpty())
                out += "  Aucune hypothèse (symptômes insuffisants)\n";

            out += "\n[Généré par PV3DE — Nkentseu Engine]\n";
            return out;
        }

    } // namespace pv3de
} // namespace nkentseu
