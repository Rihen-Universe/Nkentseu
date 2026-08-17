#include "PatientStatePanel.h"
#include "PV3DE/Layers/PatientLayer.h"
#include "PV3DE/UI/PvGui.h"
#include "NKMath/NKMath.h"
#include <cstdio>

namespace nkentseu {
	namespace pv3de {

		using namespace nkgui;
		using namespace nkentseu::math;

		const char *PatientStatePanel::EmotionName(EmotionState s) noexcept {
			switch (s) {
				case EmotionState::Neutral:
					return "Calme";
				case EmotionState::Discomfort:
					return "Inconfort";
				case EmotionState::PainMild:
					return "Douleur légère";
				case EmotionState::PainSevere:
					return "Douleur sévère";
				case EmotionState::Anxious:
					return "Anxieux";
				case EmotionState::Panic:
					return "Panique";
				case EmotionState::Nauseous:
					return "Nausée";
				case EmotionState::Exhausted:
					return "Épuisé";
				case EmotionState::Confused:
					return "Confus";
				default:
					return "Inconnu";
			}
		}

		const char *PatientStatePanel::EmotionIcon(EmotionState s) noexcept {
			switch (s) {
				case EmotionState::Neutral:
					return "[=]";
				case EmotionState::Discomfort:
					return "[~]";
				case EmotionState::PainMild:
					return "[!]";
				case EmotionState::PainSevere:
					return "[!!]";
				case EmotionState::Anxious:
					return "[?!]";
				case EmotionState::Panic:
					return "[!!!]";
				case EmotionState::Nauseous:
					return "[~!]";
				case EmotionState::Exhausted:
					return "[zzz]";
				case EmotionState::Confused:
					return "[???]";
				default:
					return "[?]";
			}
		}

		NkColor PatientStatePanel::EmotionColor(EmotionState s) noexcept {
			switch (s) {
				case EmotionState::Neutral:
					return {120, 200, 120, 255};
				case EmotionState::Discomfort:
					return {200, 200, 100, 255};
				case EmotionState::PainMild:
					return {220, 160, 60, 255};
				case EmotionState::PainSevere:
					return {220, 60, 60, 255};
				case EmotionState::Anxious:
					return {180, 80, 220, 255};
				case EmotionState::Panic:
					return {255, 40, 40, 255};
				case EmotionState::Nauseous:
					return {100, 200, 140, 255};
				case EmotionState::Exhausted:
					return {100, 120, 180, 255};
				case EmotionState::Confused:
					return {200, 160, 80, 255};
				default:
					return {150, 150, 150, 255};
			}
		}

		// =====================================================================
		void PatientStatePanel::Render(NkGuiContext &ctx, const PatientLayer &patient, const NkRect &rect) noexcept {
			if (!BeginPanel(ctx, "État du patient", rect))
				return;

			// Animation clignotement alarmes
			mAlarmBlink += ctx.input.dt * 2.f;
			if (mAlarmBlink > (2.0f * (float32)NkPi))
				mAlarmBlink -= (2.0f * (float32)NkPi);
			bool blinkOn = NkSin(mAlarmBlink) > 0.f;

			const NkClinicalState &state = patient.GetClinicalState();
			const NkEmotionOutput &emotion = patient.GetEmotionOutput();

			// ── Émotion dominante ─────────────────────────────────────────────
			pvgui::TextColored(ctx, {180, 180, 180, 255}, "État émotionnel");

			RenderEmotionBar(ctx, emotion);
			Separator(ctx);

			// ── Jauges physiologiques ─────────────────────────────────────────
			pvgui::TextColored(ctx, {180, 180, 180, 255}, "Physiologie");

			RenderPhysioGauge(ctx, "Douleur", state.painLevel, 10.f, {220, 60, 60, 255});
			RenderPhysioGauge(ctx, "Anxiété", state.anxietyLevel, 1.f, {180, 80, 220, 255});
			RenderPhysioGauge(ctx, "Nausée", state.nauseaLevel, 1.f, {100, 200, 140, 255});
			RenderPhysioGauge(ctx, "Fatigue", state.fatigueLevel, 1.f, {100, 120, 180, 255});
			RenderPhysioGauge(ctx, "Dyspnée", state.breathingDifficulty, 1.f, {60, 160, 220, 255});

			Separator(ctx);

			// ── Constantes vitales ────────────────────────────────────────────
			RenderVitals(ctx, state);

			// ── Alarmes critiques ─────────────────────────────────────────────
			bool alarm =
				(state.heartRate > 120.f || state.heartRate < 40.f || state.spo2 < 90.f || state.temperature > 39.5f);
			if (alarm && blinkOn) {
				pvgui::TextColored(ctx, {255, 40, 40, 255}, "⚠ ALARME — Paramètres critiques");
			}

			EndPanel(ctx);
		}

		// =====================================================================
		void PatientStatePanel::RenderEmotionBar(NkGuiContext &ctx, const NkEmotionOutput &em) noexcept {
			NkColor col = EmotionColor(em.state);
			char buf[64];
			snprintf(buf, sizeof(buf), "%s  %s  (%.0f%%)", EmotionIcon(em.state), EmotionName(em.state),
					 em.intensity * 100.f);

			pvgui::TextColored(ctx, col, buf);

			// Barre d'intensité
			ProgressBar(ctx, em.intensity);
		}

		void PatientStatePanel::RenderPhysioGauge(NkGuiContext &ctx, const char *label, nk_float32 value,
												  nk_float32 maxVal, NkColor color) noexcept {
			float32 norm = NkClamp(value / maxVal, 0.f, 1.f);
			char buf[64];
			snprintf(buf, sizeof(buf), "%s: %.1f / %.0f", label, value, maxVal);

			// NOTE : pas de hook de couleur par barre dans NKGui (comme NKUI) —
			// couleur documentée mais non appliquée (même dette qu'en v2).
			(void)color;

			const float32 sizes[2] = {92.f, -1.f};
			BeginRow(ctx, 18.f, sizes, 2);
			pvgui::TextColored(ctx, {180, 180, 180, 255}, buf);
			ProgressBar(ctx, norm);
			EndRow(ctx);
		}

		void PatientStatePanel::RenderVitals(NkGuiContext &ctx, const NkClinicalState &s) noexcept {
			pvgui::TextColored(ctx, {180, 180, 180, 255}, "Constantes vitales");

			// FC
			bool fcAlarm = (s.heartRate > 120.f || s.heartRate < 40.f);
			NkColor fcCol = fcAlarm ? NkColor{255, 60, 60, 255} : NkColor{200, 200, 200, 255};
			char fcBuf[32];
			snprintf(fcBuf, sizeof(fcBuf), "FC: %.0f bpm", s.heartRate);
			pvgui::TextColored(ctx, fcCol, fcBuf);

			// Température
			bool tempAlarm = (s.temperature > 39.f || s.temperature < 35.5f);
			NkColor tCol = tempAlarm ? NkColor{255, 120, 60, 255} : NkColor{200, 200, 200, 255};
			char tBuf[32];
			snprintf(tBuf, sizeof(tBuf), "T°: %.1f°C", s.temperature);
			pvgui::TextColored(ctx, tCol, tBuf);

			// SpO2
			bool spoAlarm = (s.spo2 < 90.f);
			bool spoWarn = (s.spo2 < 95.f);
			NkColor spoCol = spoAlarm  ? NkColor{255, 60, 60, 255}
							 : spoWarn ? NkColor{255, 180, 60, 255}
									   : NkColor{80, 200, 80, 255};
			char spoBuf[32];
			snprintf(spoBuf, sizeof(spoBuf), "SpO2: %.0f%%", s.spo2);
			pvgui::TextColored(ctx, spoCol, spoBuf);
		}

	} // namespace pv3de
} // namespace nkentseu
