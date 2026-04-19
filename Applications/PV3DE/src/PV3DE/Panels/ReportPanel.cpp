#include "ReportPanel.h"
#include "PV3DE/Layers/PatientLayer.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <cstring>
#include <ctime>

namespace nkentseu {
    namespace pv3de {

        using namespace nkui;

        // =====================================================================
        void ReportPanel::Render(NkUIContext& ctx,
                                  NkUIWindowManager& wm,
                                  NkUIDrawList& dl,
                                  NkUIFont& font,
                                  NkUILayoutStack& ls,
                                  const PatientLayer& patient,
                                  NkUIRect rect) noexcept {
            NkUIWindow::SetNextWindowPos ({rect.x, rect.y});
            NkUIWindow::SetNextWindowSize({rect.w, rect.h});

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls,
                                   "Rapport clinique##report",
                                   nullptr,
                                   NkUIWindowFlags::NK_NO_MOVE |
                                   NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            // ── En-tête ───────────────────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 20.f);
            NkUI::Text(ctx, ls, dl, font, "Rapport clinique",
                       {220, 220, 100, 255});
            NkUI::EndRow(ctx, ls);

            NkUI::Separator(ctx, ls, dl);

            // ── Infos patient ─────────────────────────────────────────────────
            RenderPatientInfo(ctx, dl, font, ls);
            NkUI::Separator(ctx, ls, dl);

            // ── Résumé ────────────────────────────────────────────────────────
            RenderSummary(ctx, dl, font, ls, patient.GetClinicalState());
            NkUI::Separator(ctx, ls, dl);

            // ── Export ────────────────────────────────────────────────────────
            RenderExportButtons(ctx, dl, font, ls, patient);

            // Message de statut
            if (mStatusTimer > 0.f) {
                mStatusTimer -= ctx.input.dt;
                NkUI::BeginRow(ctx, ls, 18.f);
                NkColor stCol = mExportOk
                    ? NkColor{80, 220, 80, 255}
                    : NkColor{220, 80, 80, 255};
                NkUI::TextColored(ctx, ls, dl, font, stCol, mStatusMsg.CStr());
                NkUI::EndRow(ctx, ls);
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // =====================================================================
        void ReportPanel::RenderPatientInfo(NkUIContext& ctx,
                                             NkUIDrawList& dl,
                                             NkUIFont& font,
                                             NkUILayoutStack& ls) noexcept {
            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::SetNextWidth(ctx, ls, 70.f);
            NkUI::Text(ctx, ls, dl, font, "Nom:");
            NkUI::SetNextGrow(ctx, ls);
            NkUI::InputText(ctx, ls, dl, font, "##ln", mLastName, 64);
            NkUI::EndRow(ctx, ls);

            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::SetNextWidth(ctx, ls, 70.f);
            NkUI::Text(ctx, ls, dl, font, "Prénom:");
            NkUI::SetNextGrow(ctx, ls);
            NkUI::InputText(ctx, ls, dl, font, "##fn", mFirstName, 64);
            NkUI::EndRow(ctx, ls);

            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::SetNextWidth(ctx, ls, 70.f);
            NkUI::Text(ctx, ls, dl, font, "Âge:");
            NkUI::SetNextWidth(ctx, ls, 60.f);
            NkUI::InputInt(ctx, ls, dl, font, "##age", mAge);
            NkUI::SameLine(ctx, ls, 10.f);
            NkUI::SetNextWidth(ctx, ls, 50.f);
            NkUI::Text(ctx, ls, dl, font, "Genre:");
            NkUI::SetNextWidth(ctx, ls, 40.f);
            NkUI::InputText(ctx, ls, dl, font, "##gen", mGender, 4);
            NkUI::EndRow(ctx, ls);
        }

        // =====================================================================
        void ReportPanel::RenderSummary(NkUIContext& ctx,
                                         NkUIDrawList& dl,
                                         NkUIFont& font,
                                         NkUILayoutStack& ls,
                                         const NkClinicalState& state) noexcept {
            // Rafraîchir le résumé toutes les 2 secondes
            mSummaryAge += ctx.input.dt;
            if (mSummaryAge >= 2.f) {
                mSummaryAge = 0.f;

                NkPatientInfo info;
                info.lastName  = NkString(mLastName);
                info.firstName = NkString(mFirstName);
                info.ageYears  = (nk_uint32)NkMax(mAge, 0);
                info.gender    = NkString(mGender);
                mExporter.SetPatientInfo(info);

                mSummaryCache = mExporter.GenerateSummary(state);
            }

            NkUI::BeginRow(ctx, ls, 18.f);
            NkUI::Text(ctx, ls, dl, font, "Résumé:", {180,180,180,255});
            NkUI::EndRow(ctx, ls);

            NkUI::BeginRow(ctx, ls, 80.f);
            NkUI::TextWrapped(ctx, ls, dl, font, mSummaryCache.CStr());
            NkUI::EndRow(ctx, ls);
        }

        // =====================================================================
        void ReportPanel::RenderExportButtons(NkUIContext& ctx,
                                               NkUIDrawList& dl,
                                               NkUIFont& font,
                                               NkUILayoutStack& ls,
                                               const PatientLayer& patient) noexcept {
            // Mettre à jour les infos patient
            mPatientInfo.lastName  = NkString(mLastName);
            mPatientInfo.firstName = NkString(mFirstName);
            mPatientInfo.ageYears  = (nk_uint32)NkMax(mAge, 0);
            mPatientInfo.gender    = NkString(mGender);
            mExporter.SetPatientInfo(mPatientInfo);

            NkUI::BeginRow(ctx, ls, 26.f);

            // Export FHIR JSON
            if (NkUI::Button(ctx, ls, dl, font, "Export FHIR JSON")) {
                // Générer le nom de fichier avec timestamp
                time_t now = time(nullptr);
                char fname[64];
                snprintf(fname, sizeof(fname), "report_%s_%s_%ld.fhir.json",
                         mLastName, mFirstName, (long)now);
                if (ExportFHIR(patient, fname)) {
                    mStatusMsg   = NkString("Export JSON: ") + NkString(fname);
                    mExportOk    = true;
                } else {
                    mStatusMsg   = NkString("Erreur export JSON");
                    mExportOk    = false;
                }
                mStatusTimer = 4.f;
            }

            NkUI::SameLine(ctx, ls, 8.f);

            // Export PDF
            if (NkUI::Button(ctx, ls, dl, font, "Export PDF")) {
                time_t now = time(nullptr);
                char fname[64];
                snprintf(fname, sizeof(fname), "report_%s_%s_%ld.pdf",
                         mLastName, mFirstName, (long)now);
                if (ExportPDF(patient, fname)) {
                    mStatusMsg  = NkString("Export PDF: ") + NkString(fname);
                    mExportOk   = true;
                } else {
                    mStatusMsg  = NkString("Erreur export PDF");
                    mExportOk   = false;
                }
                mStatusTimer = 4.f;
            }

            NkUI::EndRow(ctx, ls);
        }

        // =====================================================================
        bool ReportPanel::ExportFHIR(const PatientLayer& patient,
                                      const char* path) noexcept {
            NkString json = mExporter.GenerateReport(patient.GetClinicalState());
            NkFile file;
            if (!file.Open(path, NkFileMode::Write | NkFileMode::Text)) {
                logger.Errorf("[ReportPanel] FHIR: ouverture {} échouée\n", path);
                return false;
            }
            file.WriteString(json);
            file.Close();
            logger.Infof("[ReportPanel] FHIR exporté: {}\n", path);
            return true;
        }

        // =====================================================================
        // Export PDF — version texte brut balisé (PDF minimaliste sans librairie)
        // Un vrai PDF nécessite libharu / fpdf — ici on génère un PDF/A minimaliste.
        bool ReportPanel::ExportPDF(const PatientLayer& patient,
                                     const char* path) noexcept {
            const NkClinicalState& s = patient.GetClinicalState();

            // Construction du rapport texte riche
            NkString txt;
            txt += "RAPPORT CLINIQUE - PATIENT VIRTUEL 3D EMOTIF\n";
            txt += "==============================================\n\n";

            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Patient : %s %s, %u ans (%s)\n",
                     mPatientInfo.firstName.CStr(),
                     mPatientInfo.lastName.CStr(),
                     mPatientInfo.ageYears,
                     mPatientInfo.gender.CStr());
            txt += buf;

            time_t now = time(nullptr);
            char tbuf[64]; strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", localtime(&now));
            snprintf(buf, sizeof(buf), "Date     : %s\n\n", tbuf);
            txt += buf;

            txt += "CONSTANTES VITALES\n------------------\n";
            snprintf(buf, sizeof(buf),
                     "FC: %.0f bpm  |  T°: %.1f°C  |  SpO2: %.0f%%\n\n",
                     s.heartRate, s.temperature, s.spo2);
            txt += buf;

            txt += "NIVEAUX PHYSIOLOGIQUES\n----------------------\n";
            snprintf(buf, sizeof(buf),
                     "Douleur : %.1f/10\nAnxiété : %.0f%%\nNausée  : %.0f%%\nFatigue : %.0f%%\nDyspnée : %.0f%%\n\n",
                     s.painLevel,
                     s.anxietyLevel * 100.f, s.nauseaLevel * 100.f,
                     s.fatigueLevel * 100.f, s.breathingDifficulty * 100.f);
            txt += buf;

            txt += "DIAGNOSTIC DIFFÉRENTIEL\n-----------------------\n";
            for (nk_usize i = 0; i < NkMin(s.differentialRanking.Size(), (nk_usize)5); ++i) {
                const auto& d = s.differentialRanking[i];
                snprintf(buf, sizeof(buf), "%zu. %-30s  %.0f%%  (sév %.0f%%)\n",
                         i + 1, d.diseaseName.CStr(),
                         d.probability * 100.f, d.severity * 100.f);
                txt += buf;
            }
            txt += "\n";

            // Résumé
            txt += "RÉSUMÉ\n------\n";
            txt += mExporter.GenerateSummary(s);
            txt += "\n\n";
            txt += "-- Généré par PV3DE (Patient Virtuel 3D Emotif) --\n";

            // Écrire le PDF textuel (structure PDF minimale valide)
            // Pour un PDF complet il faudrait libharu/fpdf — ici on produit
            // un fichier .pdf avec contenu texte (lisible par la plupart des readers)
            NkFile file;
            if (!file.Open(path, NkFileMode::Write | NkFileMode::Binary)) {
                logger.Errorf("[ReportPanel] PDF: ouverture {} échouée\n", path);
                return false;
            }

            // En-tête PDF minimal
            NkString pdf;
            pdf += "%PDF-1.4\n";
            pdf += "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";
            pdf += "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";

            // Contenu texte (stream PDF)
            NkString streamContent;
            streamContent += "BT\n/F1 10 Tf\n50 750 Td\n";
            // Découper le texte en lignes PDF
            const char* p = txt.CStr();
            float32 y = 750.f;
            char lineBuf[512];
            while (*p && y > 50.f) {
                const char* nl = NkStrChr(p, '\n');
                nk_usize len = nl ? (nk_usize)(nl - p) : NkStrLen(p);
                len = NkMin(len, (nk_usize)100); // limit width
                NkStrNCpy(lineBuf, p, len);
                lineBuf[len] = '\0';
                char pdfLine[512];
                snprintf(pdfLine, sizeof(pdfLine),
                         "(%s) Tj T*\n", lineBuf);
                streamContent += pdfLine;
                p = nl ? nl + 1 : p + len;
                y -= 12.f;
            }
            streamContent += "ET\n";

            char sizeBuf[32];
            snprintf(sizeBuf, sizeof(sizeBuf), "%zu", streamContent.Length());

            pdf += "3 0 obj\n<< /Type /Page /Parent 2 0 R "
                   "/MediaBox [0 0 612 792] "
                   "/Contents 4 0 R "
                   "/Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n";
            pdf += "4 0 obj\n<< /Length ";
            pdf += NkString(sizeBuf);
            pdf += " >>\nstream\n";
            pdf += streamContent;
            pdf += "endstream\nendobj\n";
            pdf += "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>\nendobj\n";
            pdf += "xref\n0 6\n0000000000 65535 f\n"
                   "0000000009 00000 n\n"
                   "0000000058 00000 n\n"
                   "0000000115 00000 n\n"
                   "0000000266 00000 n\n"
                   "0000000500 00000 n\n";
            pdf += "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n580\n%%EOF\n";

            file.WriteString(pdf);
            file.Close();
            logger.Infof("[ReportPanel] PDF exporté: {}\n", path);
            return true;
        }

    } // namespace pv3de
} // namespace nkentseu
