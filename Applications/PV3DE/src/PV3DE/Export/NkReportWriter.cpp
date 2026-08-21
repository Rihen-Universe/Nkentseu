#include "NkReportWriter.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"
#include <cstdio>
#include <cstring>
#include <ctime>

namespace nkentseu {
	namespace pv3de {

		using namespace nkentseu::math;

		void NkReportWriter::SetPatient(const char *lastName, const char *firstName, int age,
										const char *gender) noexcept {
			mPatientInfo.lastName = NkString(lastName ? lastName : "");
			mPatientInfo.firstName = NkString(firstName ? firstName : "");
			mPatientInfo.ageYears = (nk_uint32)NkMax(age, 0);
			mPatientInfo.gender = NkString(gender ? gender : "");
			mExporter.SetPatientInfo(mPatientInfo);
		}

		NkString NkReportWriter::Summary(const NkClinicalState &state) const {
			return mExporter.GenerateSummary(state);
		}

		void NkReportWriter::MakeFileName(char *out, nk_usize outSize, const char *lastName, const char *firstName,
										  const char *suffix) noexcept {
			time_t now = time(nullptr);
			snprintf(out, outSize, "report_%s_%s_%ld%s", lastName ? lastName : "", firstName ? firstName : "",
					 (long)now, suffix ? suffix : "");
		}

		// =====================================================================
		bool NkReportWriter::WriteFHIR(const NkClinicalState &state, const char *path) noexcept {
			NkString json = mExporter.GenerateReport(state);
			NkFile file;
			if (!file.Open(path, NkFileMode::NK_WRITE)) {
				logger.Errorf("[NkReportWriter] FHIR: ouverture {} échouée\n", path);
				return false;
			}
			file.Write(json);
			file.Close();
			logger.Infof("[NkReportWriter] FHIR exporté: {}\n", path);
			return true;
		}

		// =====================================================================
		// Export PDF — version texte brut balisé (PDF minimaliste sans librairie)
		// Un vrai PDF nécessite libharu / fpdf — ici on génère un PDF/A minimaliste.
		// (Comportement repris tel quel de ReportPanel::ExportPDF, 2026-08-18.)
		bool NkReportWriter::WritePDF(const NkClinicalState &s, const char *path) noexcept {
			// Construction du rapport texte riche
			NkString txt;
			txt += "RAPPORT CLINIQUE - PATIENT VIRTUEL 3D EMOTIF\n";
			txt += "==============================================\n\n";

			char buf[256];
			snprintf(buf, sizeof(buf), "Patient : %s %s, %u ans (%s)\n", mPatientInfo.firstName.CStr(),
					 mPatientInfo.lastName.CStr(), mPatientInfo.ageYears, mPatientInfo.gender.CStr());
			txt += buf;

			time_t now = time(nullptr);
			char tbuf[64];
			strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", localtime(&now));
			snprintf(buf, sizeof(buf), "Date     : %s\n\n", tbuf);
			txt += buf;

			txt += "CONSTANTES VITALES\n------------------\n";
			snprintf(buf, sizeof(buf), "FC: %.0f bpm  |  T°: %.1f°C  |  SpO2: %.0f%%\n\n", s.heartRate, s.temperature,
					 s.spo2);
			txt += buf;

			txt += "NIVEAUX PHYSIOLOGIQUES\n----------------------\n";
			snprintf(buf, sizeof(buf),
					 "Douleur : %.1f/10\nAnxiété : %.0f%%\nNausée  : %.0f%%\nFatigue : %.0f%%\nDyspnée : %.0f%%\n\n",
					 s.painLevel, s.anxietyLevel * 100.f, s.nauseaLevel * 100.f, s.fatigueLevel * 100.f,
					 s.breathingDifficulty * 100.f);
			txt += buf;

			txt += "DIAGNOSTIC DIFFÉRENTIEL\n-----------------------\n";
			for (nk_usize i = 0; i < NkMin(s.differentialRanking.Size(), (nk_usize)5); ++i) {
				const auto &d = s.differentialRanking[i];
				snprintf(buf, sizeof(buf), "%zu. %-30s  %.0f%%  (sév %.0f%%)\n", i + 1, d.diseaseName.CStr(),
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
			if (!file.Open(path, NkFileMode::NK_WRITE_BINARY)) {
				logger.Errorf("[NkReportWriter] PDF: ouverture {} échouée\n", path);
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
			const char *p = txt.CStr();
			float32 y = 750.f;
			char lineBuf[512];
			while (*p && y > 50.f) {
				const char *nl = strchr(p, '\n');
				nk_usize len = nl ? (nk_usize)(nl - p) : strlen(p);
				len = NkMin(len, (nk_usize)100); // limit width
				strncpy(lineBuf, p, len);
				lineBuf[len] = '\0';
				char pdfLine[512];
				snprintf(pdfLine, sizeof(pdfLine), "(%s) Tj T*\n", lineBuf);
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

			file.Write(pdf);
			file.Close();
			logger.Infof("[NkReportWriter] PDF exporté: {}\n", path);
			return true;
		}

	} // namespace pv3de
} // namespace nkentseu
