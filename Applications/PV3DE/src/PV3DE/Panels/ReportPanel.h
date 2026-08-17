#pragma once
// =============================================================================
// PV3DE/Panels/ReportPanel.h
// =============================================================================
// Panel de génération et d'export du rapport clinique.
// Affiche un résumé textuel + boutons d'export JSON FHIR et PDF.
// Permet de saisir les informations patient avant export.
// Porté NKUI -> NKGui (2026-08-18). Le COMPORTEMENT d'export (FHIR, PDF,
// résumé, nom de fichier) vit dans Export/NkReportWriter — modèle neutre sans
// dépendance UI, repris tel quel de l'ancien panneau ; ce fichier ne garde que
// la saisie et les boutons.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKGui/NKGui.h"
#include "PV3DE/Core/NkClinicalState.h"
#include "PV3DE/Export/NkReportWriter.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace pv3de {

		class PatientLayer;

		class ReportPanel {
			public:
				ReportPanel() = default;

				void Render(nkgui::NkGuiContext &ctx, const PatientLayer &patient,
							const nkgui::NkRect &rect) noexcept;

			private:
				void RenderPatientInfo(nkgui::NkGuiContext &ctx) noexcept;

				void RenderSummary(nkgui::NkGuiContext &ctx, const NkClinicalState &state) noexcept;

				void RenderExportButtons(nkgui::NkGuiContext &ctx, const PatientLayer &patient) noexcept;

				NkReportWriter mWriter;

				char mLastName[64] = "Dupont";
				char mFirstName[64] = "Jean";
				char mGender[8] = "M";
				int mAge = 45;

				NkString mStatusMsg;
				float32 mStatusTimer = 0.f;
				bool mExportOk = false;

				// Résumé textuel mis en cache
				NkString mSummaryCache;
				float32 mSummaryAge = 999.f; // âge du cache
		};

	} // namespace pv3de
} // namespace nkentseu
