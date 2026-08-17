#pragma once
// =============================================================================
// PV3DE/Export/NkReportWriter.h
// =============================================================================
// Modèle NEUTRE du rapport clinique : identité du patient saisie, résumé texte,
// écriture des fichiers FHIR JSON et PDF minimaliste. Aucune dépendance à une
// couche d'interface (ni NKUI ni NKGui) : c'est le test du modèle neutre
// (CLAUDE.md, « NKUI est dépréciée »). Extrait de ReportPanel (2026-08-18) lors
// du portage de l'UI médicale sur NKGui : le panneau ne garde que la saisie et
// les boutons, ce fichier garde le comportement d'export tel quel.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "PV3DE/Core/NkClinicalState.h"
#include "PV3DE/Export/NkFHIRExport.h"

namespace nkentseu {
	namespace pv3de {

		class NkReportWriter {
			public:
				NkReportWriter() = default;

				// Identité patient (copiée dans NkFHIRExport à chaque appel).
				void SetPatient(const char *lastName, const char *firstName, int age, const char *gender) noexcept;

				const NkPatientInfo &GetPatientInfo() const noexcept {
					return mPatientInfo;
				}

				// Résumé textuel court (le panneau le rafraîchit toutes les 2 s).
				NkString Summary(const NkClinicalState &state) const;

				// Écrit le rapport FHIR JSON. Retourne true si le fichier est écrit.
				bool WriteFHIR(const NkClinicalState &state, const char *path) noexcept;

				// Écrit un PDF minimaliste (texte Courier, une page). Retourne true si écrit.
				bool WritePDF(const NkClinicalState &state, const char *path) noexcept;

				// Nom de fichier horodaté : report_<Nom>_<Prénom>_<epoch><suffix>
				static void MakeFileName(char *out, nk_usize outSize, const char *lastName, const char *firstName,
										 const char *suffix) noexcept;

			private:
				NkFHIRExport mExporter;
				NkPatientInfo mPatientInfo;
		};

	} // namespace pv3de
} // namespace nkentseu
