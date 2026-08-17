#include "SymptomInput.h"
#include "PV3DE/Layers/PatientLayer.h"
#include "PV3DE/UI/PvGui.h"
#include "NKMath/NKMath.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {
	namespace pv3de {

		using namespace nkgui;

		void SymptomInputPanel::Init(const NkDiagnosticEngine *engine) noexcept {
			if (!engine)
				return;
			mSymptoms.Clear();

			const auto &syms = engine->GetSymptoms();
			for (nk_usize i = 0; i < syms.Size(); ++i) {
				SymptomEntry e;
				e.id = syms[i].id;
				e.name = syms[i].name;
				// Catégorie déduite du premier tag de la BDD (simplifié)
				e.category = "Général";
				e.active = false;
				mSymptoms.PushBack(e);
			}

			// Ouvrir toutes les catégories par défaut
			for (nk_uint32 i = 0; i < mCatCount; ++i)
				mCatOpen[i] = true;
		}

		// =====================================================================
		void SymptomInputPanel::Render(NkGuiContext &ctx, PatientLayer &patient, const NkRect &rect) noexcept {
			// BeginPanel : fond + titre + région de contenu défilable (remplace
			// NkUIWindow + BeginScrollRegion du code NKUI).
			if (!BeginPanel(ctx, "Symptômes", rect))
				return;

			// ── Titre ─────────────────────────────────────────────────────────
			pvgui::TextColored(ctx, {220, 220, 100, 255}, "Constantes vitales");

			RenderVitalSigns(ctx, patient);
			Separator(ctx);

			// ── Recherche ─────────────────────────────────────────────────────
			InputText(ctx, "Rechercher##sr", mSearchBuf, (int)sizeof(mSearchBuf));
			mSearchActive = mSearchBuf[0] != '\0';

			Separator(ctx);

			// ── Boutons rapides ───────────────────────────────────────────────
			{
				const float32 sizes[2] = {110.f, -1.f};
				BeginRow(ctx, 22.f, sizes, 2);
				if (Button(ctx, "Tout effacer")) {
					for (nk_usize i = 0; i < mSymptoms.Size(); ++i)
						mSymptoms[i].active = false;
					patient.ClearSymptoms();
				}
				pvgui::Toggle(ctx, "Auto", mAutoApply);
				EndRow(ctx);
			}

			Separator(ctx);

			// ── Liste des symptômes (le panel défile) ─────────────────────────
			RenderSymptomList(ctx, patient);

			EndPanel(ctx);
		}

		// =====================================================================
		void SymptomInputPanel::RenderVitalSigns(NkGuiContext &ctx, PatientLayer &patient) noexcept {
			// Rangées {libellé 42px | slider poids 1 | valeur 64px}. NKGui n'a pas
			// de format printf sur SliderFloat : la valeur formatée est un Label.
			const float32 sizes[3] = {42.f, -1.f, 64.f};
			char vbuf[24];
			bool changed = false;

			// ── FC ────────────────────────────────────────────────────────────
			BeginRow(ctx, 22.f, sizes, 3);
			pvgui::Label(ctx, "FC:");
			changed |= SliderFloat(ctx, "##hr", mHR, 30.f, 200.f);
			snprintf(vbuf, sizeof(vbuf), "%.0f bpm", mHR);
			pvgui::Label(ctx, vbuf);
			EndRow(ctx);

			// ── Température (couleur selon valeur) ────────────────────────────
			BeginRow(ctx, 22.f, sizes, 3);
			pvgui::Label(ctx, "T°:");
			changed |= SliderFloat(ctx, "##temp", mTemp, 34.f, 42.f);
			NkColor tCol = (mTemp > 38.f)	? NkColor{255, 120, 80, 255}
						   : (mTemp < 36.f) ? NkColor{100, 150, 255, 255}
											: NkColor{200, 200, 200, 255};
			snprintf(vbuf, sizeof(vbuf), "%.1f°C", mTemp);
			pvgui::TextColored(ctx, tCol, vbuf);
			EndRow(ctx);

			// ── SpO2 ──────────────────────────────────────────────────────────
			BeginRow(ctx, 22.f, sizes, 3);
			pvgui::Label(ctx, "SpO2:");
			changed |= SliderFloat(ctx, "##spo2", mSpO2, 70.f, 100.f);
			NkColor sCol = (mSpO2 < 90.f)	? NkColor{255, 60, 60, 255}
						   : (mSpO2 < 95.f) ? NkColor{255, 180, 60, 255}
											: NkColor{80, 200, 80, 255};
			snprintf(vbuf, sizeof(vbuf), "%.0f%%", mSpO2);
			pvgui::TextColored(ctx, sCol, vbuf);
			EndRow(ctx);

			if (changed || mAutoApply)
				patient.SetVitalSigns(mHR, mTemp, mSpO2);
		}

		// =====================================================================
		void SymptomInputPanel::RenderSymptomList(NkGuiContext &ctx, PatientLayer &patient) noexcept {
			for (nk_usize i = 0; i < mSymptoms.Size(); ++i) {
				auto &sym = mSymptoms[i];

				// Filtre texte
				if (mSearchActive) {
					if (!sym.name.Contains(mSearchBuf))
						continue;
				}

				bool changed = Checkbox(ctx, sym.name.CStr(), sym.active);

				if (changed && mAutoApply) {
					if (sym.active)
						patient.AddSymptom(sym.id);
					else
						patient.RemoveSymptom(sym.id);
				}
			}
		}

		bool SymptomInputPanel::IsCatOpen(const char *cat) noexcept {
			for (nk_uint32 i = 0; i < mCatCount; ++i)
				if (strcmp(mCatNames[i], cat) == 0)
					return mCatOpen[i];
			return true;
		}

		void SymptomInputPanel::SetCatOpen(const char *cat, bool open) noexcept {
			for (nk_uint32 i = 0; i < mCatCount; ++i) {
				if (strcmp(mCatNames[i], cat) == 0) {
					mCatOpen[i] = open;
					return;
				}
			}
			if (mCatCount < kMaxCats) {
				strncpy(mCatNames[mCatCount], cat, 31);
				mCatNames[mCatCount][31] = '\0';
				mCatOpen[mCatCount++] = open;
			}
		}

	} // namespace pv3de
} // namespace nkentseu
