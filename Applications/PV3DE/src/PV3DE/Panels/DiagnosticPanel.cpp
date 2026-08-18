#include "DiagnosticPanel.h"
#include "PV3DE/Layers/PatientLayer.h"
#include "PV3DE/UI/PvGui.h"
#include "NKMath/NKMath.h"
#include <cstdio>

namespace nkentseu {
	namespace pv3de {

		using namespace nkgui;
		using namespace nkentseu::math;

		NkColor DiagnosticPanel::ProbabilityColor(nk_float32 p) noexcept {
			// 0–30% gris, 30–60% jaune, 60–80% orange, 80–100% rouge
			if (p >= 0.80f)
				return {220, 60, 60, 255};
			if (p >= 0.60f)
				return {220, 140, 50, 255};
			if (p >= 0.30f)
				return {200, 180, 60, 255};
			return {130, 130, 130, 255};
		}

		NkColor DiagnosticPanel::SeverityColor(nk_float32 s) noexcept {
			if (s >= 0.80f)
				return {220, 60, 60, 220};
			if (s >= 0.50f)
				return {220, 140, 50, 220};
			return {80, 180, 80, 220};
		}

		// =====================================================================
		void DiagnosticPanel::Render(NkGuiContext &ctx, const PatientLayer &patient, const NkRect &rect) noexcept {
			if (!BeginPanel(ctx, "Diagnostic différentiel", rect))
				return;

			const NkClinicalState &state = patient.GetClinicalState();
			const auto &ranking = state.differentialRanking;

			// ── Titre + toggle ────────────────────────────────────────────────
			char titleBuf[64];
			snprintf(titleBuf, sizeof(titleBuf), "Diagnostic (%zu hypothèses)", ranking.Size());
			pvgui::TextColored(ctx, {220, 220, 100, 255}, titleBuf);

			pvgui::Toggle(ctx, "Afficher tout", mShowAll);

			Separator(ctx);

			if (ranking.IsEmpty()) {
				pvgui::TextColored(ctx, {140, 140, 140, 255}, "Aucun symptôme sélectionné");
				EndPanel(ctx);
				return;
			}

			// ── Liste des hypothèses (le panel défile) ────────────────────────
			nk_usize maxShow = mShowAll ? ranking.Size() : NkMin(ranking.Size(), (nk_usize)8);

			for (nk_usize i = 0; i < maxShow; ++i) {
				const auto &entry = ranking[i];

				// ── Rang + nom ─────────────────────────────────────────────
				char rankBuf[8];
				snprintf(rankBuf, sizeof(rankBuf), "%zu.", i + 1);

				NkColor rankCol = (i == 0)	 ? NkColor{255, 220, 80, 255}
								  : (i == 1) ? NkColor{200, 200, 200, 255}
											 : NkColor{150, 120, 80, 255};
				{
					const float32 sizes[2] = {22.f, -1.f};
					BeginRow(ctx, 18.f, sizes, 2);
					pvgui::TextColored(ctx, rankCol, rankBuf);
					pvgui::TextColored(ctx, ProbabilityColor(entry.probability), entry.diseaseName.CStr());
					EndRow(ctx);
				}

				// ── Barre de probabilité ───────────────────────────────────
				char probBuf[16];
				snprintf(probBuf, sizeof(probBuf), "%.0f%%", entry.probability * 100.f);
				ProgressBar(ctx, entry.probability, probBuf);

				// ── Sévérité ──────────────────────────────────────────────
				const char *sevStr = (entry.severity >= 0.8f)	? "CRITIQUE"
									 : (entry.severity >= 0.6f) ? "Élevée"
									 : (entry.severity >= 0.4f) ? "Modérée"
																: "Faible";
				char sevBuf[40];
				snprintf(sevBuf, sizeof(sevBuf), "%s (%.0f%%)", sevStr, entry.severity * 100.f);
				{
					const float32 sizes[2] = {64.f, -1.f};
					BeginRow(ctx, 14.f, sizes, 2);
					pvgui::TextColored(ctx, {140, 140, 140, 255}, "Sévérité:");
					pvgui::TextColored(ctx, SeverityColor(entry.severity), sevBuf);
					EndRow(ctx);
				}

				Spacer(ctx, 0.f, 4.f);
			}

			EndPanel(ctx);
		}

	} // namespace pv3de
} // namespace nkentseu
