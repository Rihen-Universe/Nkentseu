#pragma once
// =============================================================================
// PV3DE/Panels/PatientStatePanel.h
// =============================================================================
// Affiche l'état courant du patient en temps réel :
//   - Émotion dominante avec icône et intensité
//   - Jauges physiologiques (douleur, nausée, fatigue, anxiété)
//   - Constantes vitales live avec indicateurs d'alarme
//   - Paramètres respiratoires (rythme, pattern)
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "PV3DE/Core/NkClinicalState.h"
#include "PV3DE/Emotion/NkEmotionFSM.h"

namespace nkentseu {
    namespace pv3de {

        class PatientLayer;

        class PatientStatePanel {
        public:
            PatientStatePanel() = default;

            void Render(nkui::NkUIContext& ctx,
                        nkui::NkUIWindowManager& wm,
                        nkui::NkUIDrawList& dl,
                        nkui::NkUIFont& font,
                        nkui::NkUILayoutStack& ls,
                        const PatientLayer& patient,
                        nkui::NkUIRect rect) noexcept;

        private:
            void RenderEmotionBar(nkui::NkUIContext& ctx,
                                   nkui::NkUIDrawList& dl,
                                   nkui::NkUIFont& font,
                                   nkui::NkUILayoutStack& ls,
                                   const NkEmotionOutput& emotion) noexcept;

            void RenderPhysioGauge(nkui::NkUIContext& ctx,
                                    nkui::NkUIDrawList& dl,
                                    nkui::NkUIFont& font,
                                    nkui::NkUILayoutStack& ls,
                                    const char* label,
                                    nk_float32 value,
                                    nk_float32 maxVal,
                                    nkui::NkColor color) noexcept;

            void RenderVitals(nkui::NkUIContext& ctx,
                               nkui::NkUIDrawList& dl,
                               nkui::NkUIFont& font,
                               nkui::NkUILayoutStack& ls,
                               const NkClinicalState& state) noexcept;

            static const char* EmotionName(EmotionState s) noexcept;
            static const char* EmotionIcon(EmotionState s) noexcept;
            static nkui::NkColor EmotionColor(EmotionState s) noexcept;

            // Animation clignotement alarme
            float32 mAlarmBlink = 0.f;
        };

    } // namespace pv3de
} // namespace nkentseu
