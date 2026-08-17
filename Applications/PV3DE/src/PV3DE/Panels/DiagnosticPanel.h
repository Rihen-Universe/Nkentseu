#pragma once
// =============================================================================
// PV3DE/Panels/DiagnosticPanel.h
// =============================================================================
// Affiche le classement différentiel en temps réel.
// Barres de probabilité + texte coloré (gris→orange→rouge selon probabilité).
// Porté NKUI -> NKGui (2026-08-18) : BeginPanel défilable, widgets auto-layout.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKGui/NKGui.h"
#include "PV3DE/Core/NkClinicalState.h"

namespace nkentseu {
	namespace pv3de {

		class PatientLayer;

		class DiagnosticPanel {
			public:
				DiagnosticPanel() = default;

				void Render(nkgui::NkGuiContext &ctx, const PatientLayer &patient,
							const nkgui::NkRect &rect) noexcept;

			private:
				static nkgui::NkColor ProbabilityColor(nk_float32 prob) noexcept;
				static nkgui::NkColor SeverityColor(nk_float32 sev) noexcept;

				bool mShowAll = false; // afficher toutes ou seulement top-8
		};

	} // namespace pv3de
} // namespace nkentseu
