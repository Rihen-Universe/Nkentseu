#pragma once
// =============================================================================
// PV3DE/Panels/ReportPanel.h
// =============================================================================
// Panel de génération et d'export du rapport clinique.
// Affiche un résumé textuel + boutons d'export JSON FHIR et PDF.
// Permet de saisir les informations patient avant export.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "PV3DE/Core/NkClinicalState.h"
#include "PV3DE/Export/NkFHIRExport.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace pv3de {

        class PatientLayer;

        class ReportPanel {
        public:
            ReportPanel() = default;

            void Render(nkui::NkUIContext& ctx,
                        nkui::NkUIWindowManager& wm,
                        nkui::NkUIDrawList& dl,
                        nkui::NkUIFont& font,
                        nkui::NkUILayoutStack& ls,
                        const PatientLayer& patient,
                        nkui::NkUIRect rect) noexcept;

        private:
            void RenderPatientInfo(nkui::NkUIContext& ctx,
                                    nkui::NkUIDrawList& dl,
                                    nkui::NkUIFont& font,
                                    nkui::NkUILayoutStack& ls) noexcept;

            void RenderSummary(nkui::NkUIContext& ctx,
                                nkui::NkUIDrawList& dl,
                                nkui::NkUIFont& font,
                                nkui::NkUILayoutStack& ls,
                                const NkClinicalState& state) noexcept;

            void RenderExportButtons(nkui::NkUIContext& ctx,
                                      nkui::NkUIDrawList& dl,
                                      nkui::NkUIFont& font,
                                      nkui::NkUILayoutStack& ls,
                                      const PatientLayer& patient) noexcept;

            bool ExportFHIR(const PatientLayer& patient,
                            const char* path) noexcept;
            bool ExportPDF (const PatientLayer& patient,
                            const char* path) noexcept;

            NkFHIRExport  mExporter;
            NkPatientInfo mPatientInfo;

            char mLastName [64]  = "Dupont";
            char mFirstName[64]  = "Jean";
            char mGender   [8]   = "M";
            int  mAge            = 45;

            NkString mLastExportPath;
            NkString mStatusMsg;
            float32  mStatusTimer = 0.f;
            bool     mExportOk    = false;

            // Résumé textuel mis en cache
            NkString mSummaryCache;
            float32  mSummaryAge = 999.f;  // âge du cache
        };

    } // namespace pv3de
} // namespace nkentseu
