#include "SymptomInput.h"
#include "PV3DE/Layers/PatientLayer.h"
#include "NKMath/NKMath.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {
    namespace pv3de {

        using namespace nkui;

        void SymptomInputPanel::Init(const NkDiagnosticEngine* engine) noexcept {
            if (!engine) return;
            mSymptoms.Clear();

            const auto& syms = engine->GetSymptoms();
            for (nk_usize i = 0; i < syms.Size(); ++i) {
                SymptomEntry e;
                e.id       = syms[i].id;
                e.name     = syms[i].name;
                // Catégorie déduite du premier tag de la BDD (simplifié)
                e.category = "Général";
                e.active   = false;
                mSymptoms.PushBack(e);
            }

            // Ouvrir toutes les catégories par défaut
            for (nk_uint32 i = 0; i < mCatCount; ++i)
                mCatOpen[i] = true;
        }

        // =====================================================================
        void SymptomInputPanel::Render(NkUIContext& ctx,
                                       NkUIWindowManager& wm,
                                       NkUIDrawList& dl,
                                       NkUIFont& font,
                                       NkUILayoutStack& ls,
                                       PatientLayer& patient,
                                       NkUIRect rect) noexcept {
            NkUIWindow::SetNextWindowPos ({rect.x, rect.y});
            NkUIWindow::SetNextWindowSize({rect.w, rect.h});

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls,
                                   "Symptômes##syminput",
                                   nullptr,
                                   NkUIWindowFlags::NK_NO_MOVE |
                                   NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            // ── Titre ─────────────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 20.f);
            NkUI::Text(ctx, ls, dl, font, "Constantes vitales", {220,220,100,255});
            NkUI::EndRow(ctx, ls);

            RenderVitalSigns(ctx, dl, font, ls, patient);
            NkUI::Separator(ctx, ls, dl);

            // ── Recherche ─────────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::SetNextGrow(ctx, ls);
            bool prevSearch = mSearchBuf[0] != '\0';
            NkUI::InputText(ctx, ls, dl, font, "Rechercher##sr",
                            mSearchBuf, (int)sizeof(mSearchBuf));
            mSearchActive = mSearchBuf[0] != '\0';
            if (!prevSearch && mSearchActive) { /* focus */ }
            NkUI::EndRow(ctx, ls);

            NkUI::Separator(ctx, ls, dl);

            // ── Boutons rapides ───────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 22.f);
            if (NkUI::ButtonSmall(ctx, ls, dl, font, "Tout effacer")) {
                for (nk_usize i = 0; i < mSymptoms.Size(); ++i)
                    mSymptoms[i].active = false;
                patient.ClearSymptoms();
            }
            NkUI::SameLine(ctx, ls);
            NkUI::Toggle(ctx, ls, dl, font, "Auto", mAutoApply);
            NkUI::EndRow(ctx, ls);

            // ── Liste des symptômes (scrollable) ──────────────────────────────
            float32 scrollY = 0.f;
            NkUIRect scrollRect = {rect.x + 2.f, rect.y + 120.f,
                                   rect.w - 4.f,  rect.h - 128.f};
            if (NkUI::BeginScrollRegion(ctx, ls, "sym_scroll",
                                         scrollRect, nullptr, &scrollY)) {
                RenderSymptomList(ctx, dl, font, ls, patient);
                NkUI::EndScrollRegion(ctx, ls);
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // =====================================================================
        void SymptomInputPanel::RenderVitalSigns(NkUIContext& ctx,
                                                  NkUIDrawList& dl,
                                                  NkUIFont& font,
                                                  NkUILayoutStack& ls,
                                                  PatientLayer& patient) noexcept {
            // ── FC ────────────────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::SetNextWidth(ctx, ls, 40.f);
            NkUI::Text(ctx, ls, dl, font, "FC:");
            NkUI::SetNextGrow(ctx, ls);
            bool changed = NkUI::SliderFloat(ctx, ls, dl, font,
                                              "##hr", mHR, 30.f, 200.f, "%.0f bpm");
            NkUI::EndRow(ctx, ls);

            // ── Température ───────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::SetNextWidth(ctx, ls, 40.f);
            NkUI::Text(ctx, ls, dl, font, "T°:");
            NkUI::SetNextGrow(ctx, ls);
            // Couleur selon valeur
            NkColor tCol = (mTemp > 38.f) ? NkColor{255,120,80,255}
                         : (mTemp < 36.f) ? NkColor{100,150,255,255}
                         :                  NkColor{200,200,200,255};
            ctx.PushStyleColor(NkStyleVar::SliderFill, tCol);
            changed |= NkUI::SliderFloat(ctx, ls, dl, font,
                                          "##temp", mTemp, 34.f, 42.f, "%.1f°C");
            ctx.PopStyle();
            NkUI::EndRow(ctx, ls);

            // ── SpO2 ──────────────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::SetNextWidth(ctx, ls, 40.f);
            NkUI::Text(ctx, ls, dl, font, "SpO2:");
            NkUI::SetNextGrow(ctx, ls);
            NkColor sCol = (mSpO2 < 90.f) ? NkColor{255,60,60,255}
                         : (mSpO2 < 95.f) ? NkColor{255,180,60,255}
                         :                  NkColor{80,200,80,255};
            ctx.PushStyleColor(NkStyleVar::SliderFill, sCol);
            changed |= NkUI::SliderFloat(ctx, ls, dl, font,
                                          "##spo2", mSpO2, 70.f, 100.f, "%.0f%%");
            ctx.PopStyle();
            NkUI::EndRow(ctx, ls);

            if (changed || mAutoApply)
                patient.SetVitalSigns(mHR, mTemp, mSpO2);
        }

        // =====================================================================
        void SymptomInputPanel::RenderSymptomList(NkUIContext& ctx,
                                                   NkUIDrawList& dl,
                                                   NkUIFont& font,
                                                   NkUILayoutStack& ls,
                                                   PatientLayer& patient) noexcept {
            for (nk_usize i = 0; i < mSymptoms.Size(); ++i) {
                auto& sym = mSymptoms[i];

                // Filtre texte
                if (mSearchActive) {
                    if (!sym.name.Contains(mSearchBuf)) continue;
                }

                NkUI::BeginRow(ctx, ls, 20.f);
                bool prev = sym.active;
                NkColor col = sym.active
                    ? NkColor{100, 220, 100, 255}
                    : NkColor{180, 180, 180, 255};
                ctx.PushStyleColor(NkStyleVar::CheckboxMark, col);
                bool changed = NkUI::Checkbox(ctx, ls, dl, font,
                                              sym.name.CStr(), sym.active);
                ctx.PopStyle();
                NkUI::EndRow(ctx, ls);

                if (changed && mAutoApply) {
                    if (sym.active) patient.AddSymptom(sym.id);
                    else            patient.RemoveSymptom(sym.id);
                }
            }
        }

        bool SymptomInputPanel::IsCatOpen(const char* cat) noexcept {
            for (nk_uint32 i = 0; i < mCatCount; ++i)
                if (NkStrEqual(mCatNames[i], cat)) return mCatOpen[i];
            return true;
        }

        void SymptomInputPanel::SetCatOpen(const char* cat, bool open) noexcept {
            for (nk_uint32 i = 0; i < mCatCount; ++i) {
                if (NkStrEqual(mCatNames[i], cat)) { mCatOpen[i] = open; return; }
            }
            if (mCatCount < kMaxCats) {
                NkStrNCpy(mCatNames[mCatCount], cat, 31);
                mCatOpen[mCatCount++] = open;
            }
        }

    } // namespace pv3de
} // namespace nkentseu
