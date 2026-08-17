#pragma once
// =============================================================================
// PV3DE/Panels/SymptomInput.h
// =============================================================================
// Panel de saisie des symptômes par le médecin.
// Affiche la liste des symptômes disponibles sous forme de cases à cocher,
// groupées par catégorie. Inclut un champ de recherche texte.
// Les constantes vitales sont saisies via des sliders.
// Porté NKUI -> NKGui (2026-08-18) : BeginPanel (titre + défilement intégrés),
// widgets auto-layout, rangées via BeginRow(tailles px / poids).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKGui/NKGui.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "PV3DE/Core/NkClinicalState.h"
#include "PV3DE/Diagnostic/NkDiagnosticEngine.h"

namespace nkentseu {
	namespace pv3de {

		class PatientLayer; // forward

		class SymptomInputPanel {
			public:
				SymptomInputPanel() = default;

				void Init(const NkDiagnosticEngine *engine) noexcept;

				void Render(nkgui::NkGuiContext &ctx, PatientLayer &patient, const nkgui::NkRect &rect) noexcept;

				// Constantes vitales saisies
				nk_float32 GetHeartRate() const noexcept {
					return mHR;
				}

				nk_float32 GetTemperature() const noexcept {
					return mTemp;
				}

				nk_float32 GetSpO2() const noexcept {
					return mSpO2;
				}

			private:
				void RenderVitalSigns(nkgui::NkGuiContext &ctx, PatientLayer &patient) noexcept;
				void RenderSymptomList(nkgui::NkGuiContext &ctx, PatientLayer &patient) noexcept;

				struct SymptomEntry {
						NkSymptomId id;
						NkString name;
						NkString category;
						bool active = false;
				};

				NkVector<SymptomEntry> mSymptoms;
				char mSearchBuf[128] = {};
				bool mSearchActive = false;

				// Constantes vitales
				nk_float32 mHR = 72.f;
				nk_float32 mTemp = 37.0f;
				nk_float32 mSpO2 = 98.f;

				// Auto-apply : applique les changements dès le toggle
				bool mAutoApply = true;

				// Catégories ouvertes
				static constexpr nk_uint32 kMaxCats = 16;
				char mCatNames[kMaxCats][32] = {};
				bool mCatOpen[kMaxCats] = {};
				nk_uint32 mCatCount = 0;

				bool IsCatOpen(const char *cat) noexcept;
				void SetCatOpen(const char *cat, bool open) noexcept;
		};

	} // namespace pv3de
} // namespace nkentseu
