#include "DiagnosticPanel.h"
#include "PV3DE/Layers/PatientLayer.h"
#include <cstdio>

namespace nkentseu {
    namespace pv3de {

        using namespace nkui;

        NkColor DiagnosticPanel::ProbabilityColor(nk_float32 p) noexcept {
            // 0–30% gris, 30–60% orange, 60–80% orange-rouge, 80–100% rouge
            if (p >= 0.80f) return {220, 60,  60,  255};
            if (p >= 0.60f) return {220, 140, 50,  255};
            if (p >= 0.30f) return {200, 180, 60,  255};
            return                  {130, 130, 130, 255};
        }

        NkColor DiagnosticPanel::SeverityColor(nk_float32 s) noexcept {
            if (s >= 0.80f) return {220, 60,  60,  220};
            if (s >= 0.50f) return {220, 140, 50,  220};
            return                  {80,  180, 80,  220};
        }

        // =====================================================================
        void DiagnosticPanel::Render(NkUIContext& ctx,
                                     NkUIWindowManager& wm,
                                     NkUIDrawList& dl,
                                     NkUIFont& font,
                                     NkUILayoutStack& ls,
                                     const PatientLayer& patient,
                                     NkUIRect rect) noexcept {
            NkUIWindow::SetNextWindowPos ({rect.x, rect.y});
            NkUIWindow::SetNextWindowSize({rect.w, rect.h});

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls,
                                   "Diagnostic différentiel##diag",
                                   nullptr,
                                   NkUIWindowFlags::NK_NO_MOVE |
                                   NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            const NkClinicalState& state = patient.GetClinicalState();
            const auto& ranking = state.differentialRanking;

            // ── Titre + toggle ────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 20.f);
            char titleBuf[64];
            snprintf(titleBuf, sizeof(titleBuf), "Diagnostic (%zu hypothèses)",
                     ranking.Size());
            NkUI::Text(ctx, ls, dl, font, titleBuf, {220, 220, 100, 255});
            NkUI::EndRow(ctx, ls);

            NkUI::BeginRow(ctx, ls, 20.f);
            NkUI::Toggle(ctx, ls, dl, font, "Afficher tout", mShowAll);
            NkUI::EndRow(ctx, ls);

            NkUI::Separator(ctx, ls, dl);

            if (ranking.IsEmpty()) {
                NkUI::BeginRow(ctx, ls, 20.f);
                NkUI::Text(ctx, ls, dl, font, "Aucun symptôme sélectionné",
                           {140, 140, 140, 255});
                NkUI::EndRow(ctx, ls);
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            // ── Liste des hypothèses ──────────────────────────────────────────
            nk_usize maxShow = mShowAll ? ranking.Size() : NkMin(ranking.Size(), (nk_usize)8);

            float32 scrollY = 0.f;
            NkUIRect scrollRect = {rect.x + 2.f, rect.y + 72.f,
                                   rect.w - 4.f,  rect.h - 80.f};
            if (NkUI::BeginScrollRegion(ctx, ls, "diag_scroll",
                                         scrollRect, nullptr, &scrollY)) {

                for (nk_usize i = 0; i < maxShow; ++i) {
                    const auto& entry = ranking[i];

                    // ── Rang + nom ─────────────────────────────────────────────
                    char rankBuf[4];
                    snprintf(rankBuf, sizeof(rankBuf), "%zu.", i + 1);

                    NkUI::BeginRow(ctx, ls, 18.f);
                    NkUI::SetNextWidth(ctx, ls, 20.f);
                    NkColor rankCol = (i == 0) ? NkColor{255,220,80,255}
                                    : (i == 1) ? NkColor{200,200,200,255}
                                    :            NkColor{150,120,80,255};
                    NkUI::TextColored(ctx, ls, dl, font, rankCol, rankBuf);
                    NkUI::SetNextGrow(ctx, ls);
                    NkUI::TextColored(ctx, ls, dl, font,
                                      ProbabilityColor(entry.probability),
                                      entry.diseaseName.CStr());
                    NkUI::EndRow(ctx, ls);

                    // ── Barre de probabilité ───────────────────────────────────
                    NkUI::BeginRow(ctx, ls, 14.f);
                    NkUI::SetNextGrow(ctx, ls);
                    ctx.PushStyleColor(NkStyleVar::ProgressFill,
                                       ProbabilityColor(entry.probability));
                    char probBuf[16];
                    snprintf(probBuf, sizeof(probBuf), "%.0f%%",
                             entry.probability * 100.f);
                    NkUI::ProgressBar(ctx, ls, dl, entry.probability,
                                      {0.f, 12.f}, probBuf);
                    ctx.PopStyle();
                    NkUI::EndRow(ctx, ls);

                    // ── Sévérité ──────────────────────────────────────────────
                    NkUI::BeginRow(ctx, ls, 12.f);
                    NkUI::SetNextWidth(ctx, ls, 60.f);
                    NkUI::Text(ctx, ls, dl, font, "Sévérité:",
                               {140, 140, 140, 255});
                    char sevBuf[32];
                    const char* sevStr =
                        (entry.severity >= 0.8f) ? "CRITIQUE" :
                        (entry.severity >= 0.6f) ? "Élevée"   :
                        (entry.severity >= 0.4f) ? "Modérée"  : "Faible";
                    snprintf(sevBuf, sizeof(sevBuf), "%s (%.0f%%)",
                             sevStr, entry.severity * 100.f);
                    NkUI::TextColored(ctx, ls, dl, font,
                                      SeverityColor(entry.severity), sevBuf);
                    NkUI::EndRow(ctx, ls);

                    NkUI::Spacing(ctx, ls, 4.f);
                }

                NkUI::EndScrollRegion(ctx, ls);
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

    } // namespace pv3de
} // namespace nkentseu
