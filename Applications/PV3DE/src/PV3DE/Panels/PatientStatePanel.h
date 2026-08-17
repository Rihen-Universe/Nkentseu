#pragma once
// =============================================================================
// PV3DE/Panels/PatientStatePanel.h
// =============================================================================
// Affiche l'état courant du patient en temps réel :
//   - Émotion dominante avec icône et intensité
//   - Jauges physiologiques (douleur, nausée, fatigue, anxiété)
//   - Constantes vitales live avec indicateurs d'alarme
//   - Paramètres respiratoires (rythme, pattern)
// Porté NKUI -> NKGui (2026-08-18) : BeginPanel défilable, widgets auto-layout.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKGui/NKGui.h"
#include "PV3DE/Core/NkClinicalState.h"
#include "PV3DE/Emotion/NkEmotionFSM.h"

namespace nkentseu {
	namespace pv3de {

		class PatientLayer;

		class PatientStatePanel {
			public:
				PatientStatePanel() = default;

				void Render(nkgui::NkGuiContext &ctx, const PatientLayer &patient,
							const nkgui::NkRect &rect) noexcept;

			private:
				void RenderEmotionBar(nkgui::NkGuiContext &ctx, const NkEmotionOutput &emotion) noexcept;

				void RenderPhysioGauge(nkgui::NkGuiContext &ctx, const char *label, nk_float32 value,
									   nk_float32 maxVal, nkgui::NkColor color) noexcept;

				void RenderVitals(nkgui::NkGuiContext &ctx, const NkClinicalState &state) noexcept;

				static const char *EmotionName(EmotionState s) noexcept;
				static const char *EmotionIcon(EmotionState s) noexcept;
				static nkgui::NkColor EmotionColor(EmotionState s) noexcept;

				// Animation clignotement alarme
				float32 mAlarmBlink = 0.f;
		};

	} // namespace pv3de
} // namespace nkentseu
