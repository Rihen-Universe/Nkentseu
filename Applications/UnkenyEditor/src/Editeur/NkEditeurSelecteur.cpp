//
// NkEditeurSelecteur.cpp
// =============================================================================
// Description :
//   L'emballage du selecteur de NKEditorKit pour l'editeur (voir le .h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurSelecteur.h"

#include "NKFileSystem/NkDirectory.h"

namespace nkentseu {
	namespace editeur {

		namespace {

			/// « *.png;*.jpg » -> « png;jpg » : la forme des filtres NOMMES du kit.
			NkString SansEtoiles(const char *motifs) {
				NkString s;
				for (const char *p = motifs; p != nullptr && *p != 0; ++p) {
					if (*p != '*' && *p != '.') {
						s.Append(*p);
					}
				}
				return s;
			}

		} // namespace

		const char *NkEditeurSelecteurEtat::PickerTitle() const {
			switch (usage) {
				case NkUsageSelecteur::NK_IMPORTER:
					return "Importer dans le Contenu du projet";
				case NkUsageSelecteur::NK_EXPORTER:
					return "Exporter la sélection vers…";
				case NkUsageSelecteur::NK_DOSSIER_SORTIE:
					return "Dossier de sortie du jeu construit";
				default:
					return NkFilePickerNavState::PickerTitle();
			}
		}

		const char *NkEditeurSelecteurEtat::PickerConfirmLabel() const {
			switch (usage) {
				case NkUsageSelecteur::NK_IMPORTER:
					return "Importer";
				case NkUsageSelecteur::NK_EXPORTER:
					return "Exporter ici";
				case NkUsageSelecteur::NK_DOSSIER_SORTIE:
					return "Choisir ce dossier";
				default:
					return NkFilePickerNavState::PickerConfirmLabel();
			}
		}

		void NkEditeurSelecteurEtat::PickerCancel() {
			usage = NkUsageSelecteur::NK_AUCUN;
			editorkit::NkFilePickerNavState::PickerCancel();
		}

		void NkEditeurOuvrirSelecteur(NkEditeurSelecteurEtat &s, NkUsageSelecteur usage, const char *depart) {
			// Le dossier de depart : celui qu'on donne, sinon le dernier ou le
			// selecteur a SERVI (la tete des recents du kit), sinon Documents.
			NkString dep = depart != nullptr ? NkString(depart) : NkString();
			if (dep.Empty() || !NkDirectory::Exists(dep.CStr())) {
				dep = NkString(s.DossierCourant());
			}
			if (dep.Empty() || !NkDirectory::Exists(dep.CStr())) {
				dep = NkDirectory::GetUserFolder(NkDirectory::NkUserFolder::Documents).ToString();
			}
			s.filtres.Clear();
			s.filtreActif = 0;
			s.tampon[0] = 0;
			s.confirme = NkUsageSelecteur::NK_AUCUN;
			if (usage == NkUsageSelecteur::NK_IMPORTER) {
				// PLUSIEURS fichiers d'un coup (Ctrl+clic), comme l'import d'UE5. Le
				// premier filtre : tout ce que l'editeur sait importer (la liste de
				// NkEditeurFiltreImport, pas une seconde table) ; « Tous les
				// fichiers » reste offert -- un format refuse se dit a l'import.
				s.selectionMultiple = true;
				s.AjouterFiltre("Formats importables", SansEtoiles(NkEditeurFiltreImport()).CStr());
				s.AjouterFiltre("Images", "png;jpg;jpeg;bmp;tga;gif;svg;webp;qoi;hdr");
				s.AjouterFiltre("Sons", "wav;ogg;mp3;flac;opus");
				s.AjouterFiltre("Polices", "ttf;otf");
				s.AjouterFiltre("Tous les fichiers", "*");
				s.OuvrirNav(editorkit::NkSelecteurOuvrirFichier, dep.CStr(), "", nullptr, s.tampon,
							static_cast<int32>(sizeof(s.tampon)));
			} else {
				s.selectionMultiple = false;
				s.OuvrirNav(editorkit::NkSelecteurOuvrirDossier, dep.CStr(), "", nullptr, s.tampon,
							static_cast<int32>(sizeof(s.tampon)));
			}
			// APRES l'ouverture : OpenPickerBase remet le mode a zero, pas l'usage.
			s.usage = usage;
		}

		NkUsageSelecteur NkEditeurDessinerSelecteur(NkEditeurCadre &c, NkEditeurSelecteurEtat &s) {
			s.confirme = NkUsageSelecteur::NK_AUCUN;
			// L'usage de CETTE fenetre : la confirmation passe par la porte de
			// sortie du kit, qui l'efface.
			const NkUsageSelecteur usage = s.usage;
			if (s.pickerOpen) {
				(void)editorkit::NkDrawSelecteur(c.ctx, s, c.theme);
			}
			if (s.pickerCancelled) {
				s.pickerCancelled = false;
				s.usage = NkUsageSelecteur::NK_AUCUN;
			}
			if (!s.pickerConfirmed) {
				return NkUsageSelecteur::NK_AUCUN;
			}
			s.pickerConfirmed = false;
			s.usage = NkUsageSelecteur::NK_AUCUN;
			s.confirme = usage;
			// Le dossier « courant » du kit : celui ou le selecteur a SERVI.
			if (usage == NkUsageSelecteur::NK_IMPORTER) {
				editorkit::NkFilePickerNavState::PoserRecent(s.recents, s.pickerPath);
			} else {
				editorkit::NkFilePickerNavState::PoserRecent(s.recents, s.pickerResultPath);
			}
			return usage;
		}

	} // namespace editeur
} // namespace nkentseu
