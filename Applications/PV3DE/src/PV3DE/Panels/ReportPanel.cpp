#include "ReportPanel.h"
#include "PV3DE/Layers/PatientLayer.h"
#include "PV3DE/UI/PvGui.h"
#include "NKLogger/NkLog.h"
#include <cstdio>

namespace nkentseu {
	namespace pv3de {

		using namespace nkgui;

		// =====================================================================
		void ReportPanel::Render(NkGuiContext &ctx, const PatientLayer &patient, const NkRect &rect) noexcept {
			if (!BeginPanel(ctx, "Rapport clinique", rect))
				return;

			// ── En-tête ───────────────────────────────────────────────────────
			pvgui::TextColored(ctx, {220, 220, 100, 255}, "Rapport clinique");

			Separator(ctx);

			// ── Infos patient ─────────────────────────────────────────────────
			RenderPatientInfo(ctx);
			Separator(ctx);

			// ── Résumé ────────────────────────────────────────────────────────
			RenderSummary(ctx, patient.GetClinicalState());
			Separator(ctx);

			// ── Export ────────────────────────────────────────────────────────
			RenderExportButtons(ctx, patient);

			// Message de statut
			if (mStatusTimer > 0.f) {
				mStatusTimer -= ctx.input.dt;
				NkColor stCol = mExportOk ? NkColor{80, 220, 80, 255} : NkColor{220, 80, 80, 255};
				pvgui::TextColored(ctx, stCol, mStatusMsg.CStr());
			}

			EndPanel(ctx);
		}

		// =====================================================================
		void ReportPanel::RenderPatientInfo(NkGuiContext &ctx) noexcept {
			InputText(ctx, "Nom##ln", mLastName, 64);
			InputText(ctx, "Prénom##fn", mFirstName, 64);

			{
				const float32 sizes[4] = {-1.f, 100.f, 48.f, 90.f};
				BeginRow(ctx, 22.f, sizes, 4);
				pvgui::Label(ctx, "Âge / Genre:");
				InputInt(ctx, "##age", mAge);
				pvgui::Label(ctx, "Genre:");
				InputText(ctx, "##gen", mGender, 4);
				EndRow(ctx);
			}
		}

		// =====================================================================
		void ReportPanel::RenderSummary(NkGuiContext &ctx, const NkClinicalState &state) noexcept {
			// Rafraîchir le résumé toutes les 2 secondes
			mSummaryAge += ctx.input.dt;
			if (mSummaryAge >= 2.f) {
				mSummaryAge = 0.f;
				mWriter.SetPatient(mLastName, mFirstName, mAge, mGender);
				mSummaryCache = mWriter.Summary(state);
			}

			pvgui::TextColored(ctx, {180, 180, 180, 255}, "Résumé:");
			TextWrapped(ctx, mSummaryCache.CStr());
		}

		// =====================================================================
		void ReportPanel::RenderExportButtons(NkGuiContext &ctx, const PatientLayer &patient) noexcept {
			// Mettre à jour les infos patient
			mWriter.SetPatient(mLastName, mFirstName, mAge, mGender);

			const float32 sizes[3] = {150.f, 110.f, -1.f};
			BeginRow(ctx, 26.f, sizes, 3);

			// Export FHIR JSON
			if (Button(ctx, "Export FHIR JSON")) {
				char fname[96];
				NkReportWriter::MakeFileName(fname, sizeof(fname), mLastName, mFirstName, ".fhir.json");
				if (mWriter.WriteFHIR(patient.GetClinicalState(), fname)) {
					mStatusMsg = NkString("Export JSON: ") + NkString(fname);
					mExportOk = true;
				} else {
					mStatusMsg = NkString("Erreur export JSON");
					mExportOk = false;
				}
				mStatusTimer = 4.f;
			}

			// Export PDF
			if (Button(ctx, "Export PDF")) {
				char fname[96];
				NkReportWriter::MakeFileName(fname, sizeof(fname), mLastName, mFirstName, ".pdf");
				if (mWriter.WritePDF(patient.GetClinicalState(), fname)) {
					mStatusMsg = NkString("Export PDF: ") + NkString(fname);
					mExportOk = true;
				} else {
					mStatusMsg = NkString("Erreur export PDF");
					mExportOk = false;
				}
				mStatusTimer = 4.f;
			}

			Spacer(ctx, 0.f, 0.f); // cellule restante (aligne les boutons à gauche)
			EndRow(ctx);
		}

	} // namespace pv3de
} // namespace nkentseu
