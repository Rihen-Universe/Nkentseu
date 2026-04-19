#pragma once
// =============================================================================
// PV3DE/Panels/DiagnosticPanel.h
// =============================================================================
// Affiche le classement différentiel en temps réel.
// Barres de probabilité colorées (vert→orange→rouge selon probabilité).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "PV3DE/Core/NkClinicalState.h"

namespace nkentseu {
    namespace pv3de {

        class PatientLayer;

        class DiagnosticPanel {
        public:
            DiagnosticPanel() = default;

            void Render(nkui::NkUIContext& ctx,
                        nkui::NkUIWindowManager& wm,
                        nkui::NkUIDrawList& dl,
                        nkui::NkUIFont& font,
                        nkui::NkUILayoutStack& ls,
                        const PatientLayer& patient,
                        nkui::NkUIRect rect) noexcept;

        private:
            static nkui::NkColor ProbabilityColor(nk_float32 prob) noexcept;
            static nkui::NkColor SeverityColor   (nk_float32 sev)  noexcept;

            bool mShowAll = false;  // afficher toutes ou seulement top-5
        };

    } // namespace pv3de
} // namespace nkentseu
