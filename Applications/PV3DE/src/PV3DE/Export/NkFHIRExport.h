#pragma once

// =============================================================================
// NkFHIRExport.h
// Génère un rapport clinique au format FHIR R4 (JSON simplifié).
// FHIR = Fast Healthcare Interoperability Resources (HL7).
//
// Ressources générées :
//   - Patient            (données démographiques)
//   - Observation[]      (constantes vitales + symptômes)
//   - DiagnosticReport   (classement différentiel + notes)
//   - Condition[]        (hypothèses diagnostiques > 30%)
//
// Usage :
//   NkFHIRExport exporter;
//   exporter.SetPatientInfo("Martin", "Jean", 45, "M");
//   NkString json = exporter.GenerateReport(clinicalState);
//   NkFileSystem::WriteText("report.fhir.json", json);
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "PV3DE/Core/NkClinicalState.h"

namespace nkentseu {
    namespace pv3de {

        struct NkPatientInfo {
            NkString lastName;
            NkString firstName;
            nk_uint32 ageYears = 0;
            NkString  gender;    // "M" | "F" | "O"
            NkString  id;        // identifiant dossier (généré si vide)
        };

        class NkFHIRExport {
        public:
            NkFHIRExport() = default;

            void SetPatientInfo(const NkPatientInfo& info) { mPatient = info; }

            // Génère le bundle FHIR complet en JSON.
            NkString GenerateReport(const NkClinicalState& state) const;

            // Génère un résumé textuel court (pour ReportPanel).
            NkString GenerateSummary(const NkClinicalState& state) const;

        private:
            NkString BuildPatientResource()     const;
            NkString BuildObservations(const NkClinicalState& s) const;
            NkString BuildConditions(const NkClinicalState& s)   const;
            NkString BuildDiagnosticReport(const NkClinicalState& s) const;

            static NkString EscapeJson(const NkString& s);
            static NkString FloatToStr(nk_float32 v, int decimals = 1);

            NkPatientInfo mPatient;
        };

    } // namespace pv3de
} // namespace nkentseu
