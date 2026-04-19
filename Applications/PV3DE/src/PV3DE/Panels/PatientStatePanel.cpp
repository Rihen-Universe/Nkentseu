#include "PatientStatePanel.h"
#include "PV3DE/Layers/PatientLayer.h"
#include "NKMath/NKMath.h"
#include <cstdio>

namespace nkentseu {
    namespace pv3de {

        using namespace nkui;

        const char* PatientStatePanel::EmotionName(EmotionState s) noexcept {
            switch (s) {
                case EmotionState::Neutral:    return "Calme";
                case EmotionState::Discomfort: return "Inconfort";
                case EmotionState::PainMild:   return "Douleur légère";
                case EmotionState::PainSevere: return "Douleur sévère";
                case EmotionState::Anxious:    return "Anxieux";
                case EmotionState::Panic:      return "Panique";
                case EmotionState::Nauseous:   return "Nausée";
                case EmotionState::Exhausted:  return "Épuisé";
                case EmotionState::Confused:   return "Confus";
                default:                       return "Inconnu";
            }
        }

        const char* PatientStatePanel::EmotionIcon(EmotionState s) noexcept {
            switch (s) {
                case EmotionState::Neutral:    return "[=]";
                case EmotionState::Discomfort: return "[~]";
                case EmotionState::PainMild:   return "[!]";
                case EmotionState::PainSevere: return "[!!]";
                case EmotionState::Anxious:    return "[?!]";
                case EmotionState::Panic:      return "[!!!]";
                case EmotionState::Nauseous:   return "[~!]";
                case EmotionState::Exhausted:  return "[zzz]";
                case EmotionState::Confused:   return "[???]";
                default:                       return "[?]";
            }
        }

        NkColor PatientStatePanel::EmotionColor(EmotionState s) noexcept {
            switch (s) {
                case EmotionState::Neutral:    return {120, 200, 120, 255};
                case EmotionState::Discomfort: return {200, 200, 100, 255};
                case EmotionState::PainMild:   return {220, 160,  60, 255};
                case EmotionState::PainSevere: return {220,  60,  60, 255};
                case EmotionState::Anxious:    return {180,  80, 220, 255};
                case EmotionState::Panic:      return {255,  40,  40, 255};
                case EmotionState::Nauseous:   return {100, 200, 140, 255};
                case EmotionState::Exhausted:  return {100, 120, 180, 255};
                case EmotionState::Confused:   return {200, 160,  80, 255};
                default:                       return {150, 150, 150, 255};
            }
        }

        // =====================================================================
        void PatientStatePanel::Render(NkUIContext& ctx,
                                       NkUIWindowManager& wm,
                                       NkUIDrawList& dl,
                                       NkUIFont& font,
                                       NkUILayoutStack& ls,
                                       const PatientLayer& patient,
                                       NkUIRect rect) noexcept {
            NkUIWindow::SetNextWindowPos ({rect.x, rect.y});
            NkUIWindow::SetNextWindowSize({rect.w, rect.h});

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls,
                                   "État du patient##patstate",
                                   nullptr,
                                   NkUIWindowFlags::NK_NO_MOVE |
                                   NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            // Animation clignotement alarmes
            mAlarmBlink += ctx.input.dt * 2.f;
            if (mAlarmBlink > NkTwoPi) mAlarmBlink -= NkTwoPi;
            bool blinkOn = NkSin(mAlarmBlink) > 0.f;

            const NkClinicalState& state   = patient.GetClinicalState();
            const NkEmotionOutput& emotion = patient.GetEmotionOutput();

            // ── Émotion dominante ─────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 28.f);
            NkUI::Text(ctx, ls, dl, font, "État émotionnel",
                       {180, 180, 180, 255});
            NkUI::EndRow(ctx, ls);

            RenderEmotionBar(ctx, dl, font, ls, emotion);
            NkUI::Separator(ctx, ls, dl);

            // ── Jauges physiologiques ─────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 18.f);
            NkUI::Text(ctx, ls, dl, font, "Physiologie", {180,180,180,255});
            NkUI::EndRow(ctx, ls);

            RenderPhysioGauge(ctx, dl, font, ls, "Douleur",
                              state.painLevel, 10.f,
                              {220, 60, 60, 255});
            RenderPhysioGauge(ctx, dl, font, ls, "Anxiété",
                              state.anxietyLevel, 1.f,
                              {180, 80, 220, 255});
            RenderPhysioGauge(ctx, dl, font, ls, "Nausée",
                              state.nauseaLevel, 1.f,
                              {100, 200, 140, 255});
            RenderPhysioGauge(ctx, dl, font, ls, "Fatigue",
                              state.fatigueLevel, 1.f,
                              {100, 120, 180, 255});
            RenderPhysioGauge(ctx, dl, font, ls, "Dyspnée",
                              state.breathingDifficulty, 1.f,
                              {60, 160, 220, 255});

            NkUI::Separator(ctx, ls, dl);

            // ── Constantes vitales ────────────────────────────────────────────
            RenderVitals(ctx, dl, font, ls, state);

            // ── Alarmes critiques ─────────────────────────────────────────────
            bool alarm = (state.heartRate > 120.f || state.heartRate < 40.f ||
                          state.spo2 < 90.f || state.temperature > 39.5f);
            if (alarm && blinkOn) {
                NkUI::BeginRow(ctx, ls, 22.f);
                NkUI::TextColored(ctx, ls, dl, font, {255, 40, 40, 255},
                                  "⚠ ALARME — Paramètres critiques");
                NkUI::EndRow(ctx, ls);
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // =====================================================================
        void PatientStatePanel::RenderEmotionBar(NkUIContext& ctx,
                                                  NkUIDrawList& dl,
                                                  NkUIFont& font,
                                                  NkUILayoutStack& ls,
                                                  const NkEmotionOutput& em) noexcept {
            NkColor col = EmotionColor(em.state);
            char buf[64];
            snprintf(buf, sizeof(buf), "%s  %s  (%.0f%%)",
                     EmotionIcon(em.state),
                     EmotionName(em.state),
                     em.intensity * 100.f);

            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::TextColored(ctx, ls, dl, font, col, buf);
            NkUI::EndRow(ctx, ls);

            // Barre d'intensité
            NkUI::BeginRow(ctx, ls, 12.f);
            NkUI::SetNextGrow(ctx, ls);
            ctx.PushStyleColor(NkStyleVar::ProgressFill, col);
            NkUI::ProgressBar(ctx, ls, dl, em.intensity, {0.f, 10.f});
            ctx.PopStyle();
            NkUI::EndRow(ctx, ls);
        }

        void PatientStatePanel::RenderPhysioGauge(NkUIContext& ctx,
                                                   NkUIDrawList& dl,
                                                   NkUIFont& font,
                                                   NkUILayoutStack& ls,
                                                   const char* label,
                                                   nk_float32 value,
                                                   nk_float32 maxVal,
                                                   NkColor color) noexcept {
            float32 norm = NkClamp(value / maxVal, 0.f, 1.f);
            char buf[64];
            snprintf(buf, sizeof(buf), "%s: %.1f / %.0f", label, value, maxVal);

            NkUI::BeginRow(ctx, ls, 18.f);
            NkUI::SetNextWidth(ctx, ls, 80.f);
            NkUI::Text(ctx, ls, dl, font, buf, {180, 180, 180, 255});
            NkUI::SetNextGrow(ctx, ls);
            ctx.PushStyleColor(NkStyleVar::ProgressFill, color);
            NkUI::ProgressBar(ctx, ls, dl, norm, {0.f, 14.f});
            ctx.PopStyle();
            NkUI::EndRow(ctx, ls);
        }

        void PatientStatePanel::RenderVitals(NkUIContext& ctx,
                                              NkUIDrawList& dl,
                                              NkUIFont& font,
                                              NkUILayoutStack& ls,
                                              const NkClinicalState& s) noexcept {
            NkUI::BeginRow(ctx, ls, 18.f);
            NkUI::Text(ctx, ls, dl, font, "Constantes vitales",
                       {180,180,180,255});
            NkUI::EndRow(ctx, ls);

            // FC
            bool fcAlarm = (s.heartRate > 120.f || s.heartRate < 40.f);
            NkColor fcCol = fcAlarm ? NkColor{255,60,60,255} : NkColor{200,200,200,255};
            char fcBuf[32]; snprintf(fcBuf, sizeof(fcBuf), "FC: %.0f bpm", s.heartRate);
            NkUI::BeginRow(ctx, ls, 18.f);
            NkUI::TextColored(ctx, ls, dl, font, fcCol, fcBuf);
            NkUI::EndRow(ctx, ls);

            // Température
            bool tempAlarm = (s.temperature > 39.f || s.temperature < 35.5f);
            NkColor tCol = tempAlarm ? NkColor{255,120,60,255} : NkColor{200,200,200,255};
            char tBuf[32]; snprintf(tBuf, sizeof(tBuf), "T°: %.1f°C", s.temperature);
            NkUI::BeginRow(ctx, ls, 18.f);
            NkUI::TextColored(ctx, ls, dl, font, tCol, tBuf);
            NkUI::EndRow(ctx, ls);

            // SpO2
            bool spoAlarm = (s.spo2 < 90.f);
            bool spoWarn  = (s.spo2 < 95.f);
            NkColor spoCol = spoAlarm ? NkColor{255,60,60,255}
                           : spoWarn  ? NkColor{255,180,60,255}
                           :            NkColor{80,200,80,255};
            char spoBuf[32]; snprintf(spoBuf, sizeof(spoBuf), "SpO2: %.0f%%", s.spo2);
            NkUI::BeginRow(ctx, ls, 18.f);
            NkUI::TextColored(ctx, ls, dl, font, spoCol, spoBuf);
            NkUI::EndRow(ctx, ls);
        }

    } // namespace pv3de
} // namespace nkentseu
