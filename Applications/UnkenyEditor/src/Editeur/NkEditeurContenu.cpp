//
// NkEditeurContenu.cpp
// =============================================================================
// Description :
//   Le contenu du projet : nature d'un fichier, dossier « Contenu », liste
//   d'un dossier, import et export (voir NkEditeurContenu.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurContenu.h"

#include "Editeur/NkEditeurActions.h"

#include "NKEditorKit/Components/NkSilhouettes.h"
#include "NKEditorKit/NkTheme.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKSerialization/Asset/NkAssetImporter.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {

			/// La barre oblique inverse (Windows). Ecrite par son code : un chemin
			/// se compare aux deux separateurs.
			constexpr char ANTISLASH = 92;

			bool Separateur(char c) noexcept {
				return c == '/' || c == ANTISLASH;
			}

			/// Le nom apres le dernier separateur (« a/b/c.png » -> « c.png »).
			NkString NomDe(const char *chemin) {
				const char *nom = chemin;
				for (const char *p = chemin; *p != 0; ++p) {
					if (Separateur(*p)) {
						nom = p + 1;
					}
				}
				return NkString(nom);
			}

			/// L'extension sans le point, dans le NOM seulement (« a.b/c » n'en a pas).
			const char *ExtensionDe(const char *chemin) noexcept {
				const char *ext = nullptr;
				for (const char *p = chemin; *p != 0; ++p) {
					if (Separateur(*p)) {
						ext = nullptr;
					} else if (*p == '.') {
						ext = p + 1;
					}
				}
				return ext != nullptr ? ext : "";
			}

			/// L'ordre du navigateur : sans casse, octet par octet.
			bool Avant(const NkString &a, const NkString &b) noexcept {
				const char *p = a.CStr();
				const char *q = b.CStr();
				for (; *p != 0 && *q != 0; ++p, ++q) {
					char x = *p, y = *q;
					x = (x >= 'A' && x <= 'Z') ? static_cast<char>(x - 'A' + 'a') : x;
					y = (y >= 'A' && y <= 'Z') ? static_cast<char>(y - 'A' + 'a') : y;
					if (x != y) {
						return x < y;
					}
				}
				return *p == 0 && *q != 0;
			}

			/// `dossier/nom`, ou `dossier/pied_2.ext`, `_3`... : le premier libre.
			/// ⚠️ UN IMPORT N'ECRASE JAMAIS : le fichier deja la reste tel quel.
			NkString CheminLibre(const NkString &dossier, const NkString &nom) {
				NkString chemin = dossier;
				chemin.Append('/');
				chemin.Append(nom);
				if (!NkFile::Exists(chemin.CStr()) && !NkDirectory::Exists(chemin.CStr())) {
					return chemin;
				}
				const char *ext = ExtensionDe(nom.CStr());
				const usize lPied = ext[0] != 0 ? static_cast<usize>(ext - nom.CStr()) - 1u : static_cast<usize>(nom.Length());
				const NkString pied(nom.CStr(), lPied);
				for (int32 k = 2; k < 10000; ++k) {
					chemin = dossier;
					chemin.Append('/');
					chemin.Append(pied);
					chemin.Append(NkString::Format("_%d", k).CStr());
					if (ext[0] != 0) {
						chemin.Append('.');
						chemin.Append(ext);
					}
					if (!NkFile::Exists(chemin.CStr()) && !NkDirectory::Exists(chemin.CStr())) {
						return chemin;
					}
				}
				return NkString();
			}

			/// Le chemin relatif d'un chemin du navigateur (« Contenu/a » -> « a »).
			NkString Relatif(const char *cheminNavigateur) {
				if (cheminNavigateur == nullptr) {
					return NkString();
				}
				const usize n = std::strlen(NK_CONTENU_RACINE);
				if (std::strncmp(cheminNavigateur, NK_CONTENU_RACINE, n) == 0 &&
					(cheminNavigateur[n] == 0 || Separateur(cheminNavigateur[n]))) {
					cheminNavigateur += n;
				}
				while (Separateur(*cheminNavigateur)) {
					++cheminNavigateur;
				}
				return NkString(cheminNavigateur);
			}

			void Parcourir(const NkString &absolu, const NkString &relatif, NkVector<NkString> &sortie, uint32 maxi, uint32 profondeur) {
				if (profondeur > 8u || sortie.Size() >= maxi) {
					return;
				}
				NkVector<NkDirectoryEntry> ent = NkDirectory::GetEntries(absolu.CStr());
				NkVector<NkString> noms;
				for (uint32 i = 0; i < ent.Size(); ++i) {
					if (ent[i].IsDirectory && !ent[i].IsHidden && ent[i].Name.Length() > 0 && ent[i].Name.CStr()[0] != '.') {
						noms.PushBack(ent[i].Name);
					}
				}
				for (uint32 i = 1; i < noms.Size(); ++i) {
					for (uint32 j = i; j > 0 && Avant(noms[j], noms[j - 1]); --j) {
						const NkString t = noms[j];
						noms[j] = noms[j - 1];
						noms[j - 1] = t;
					}
				}
				for (uint32 i = 0; i < noms.Size() && sortie.Size() < maxi; ++i) {
					NkString rel = relatif;
					if (!rel.Empty()) {
						rel.Append('/');
					}
					rel.Append(noms[i]);
					sortie.PushBack(rel);
					NkString abs = absolu;
					abs.Append('/');
					abs.Append(noms[i]);
					Parcourir(abs, rel, sortie, maxi, profondeur + 1u);
				}
			}

		} // namespace

		bool NkEditeurCheminEstContenu(const char *cheminNavigateur) noexcept {
			if (cheminNavigateur == nullptr) {
				return false;
			}
			const usize n = std::strlen(NK_CONTENU_RACINE);
			return std::strncmp(cheminNavigateur, NK_CONTENU_RACINE, n) == 0 &&
				   (cheminNavigateur[n] == 0 || Separateur(cheminNavigateur[n]));
		}

		NkString NkEditeurRelatifContenu(const char *cheminNavigateur) {
			return Relatif(cheminNavigateur);
		}

		NkNatureContenu NkEditeurNatureFichier(const char *chemin) noexcept {
			NkNatureContenu n;
			if (chemin == nullptr) {
				return n;
			}
			const char *ext = ExtensionDe(chemin);
			// Les formats DU PROJET d'abord (la table de NKSerialization), puis les
			// sources (celle de NkAssetImporter). Aucune des deux n'est recopiee.
			NkAssetType t = NkAssetTypeFromExtension(ext);
			const bool natif = t != NkAssetType::Unknown;
			if (!natif) {
				t = NkAssetImporter::DetectType(chemin);
			}
			n.type = t;
			n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Inconnu);
			n.role = static_cast<uint16>(editorkit::NkRole::TextMuted);
			n.importable = true;
			switch (t) {
				case NkAssetType::Texture2D:
				case NkAssetType::TextureCube:
					n.libelle = "Texture";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Image);
					n.role = static_cast<uint16>(editorkit::NkRole::TypeTex);
					break;
				case NkAssetType::Sound:
					n.libelle = "Son";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Archive);
					n.role = static_cast<uint16>(editorkit::NkRole::TypeAnim);
					break;
				case NkAssetType::Font:
					n.libelle = "Police";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Texte);
					n.role = static_cast<uint16>(editorkit::NkRole::TypeMat);
					break;
				case NkAssetType::Scene:
					n.libelle = "Scène";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Code);
					n.role = static_cast<uint16>(editorkit::NkRole::TypeMesh);
					break;
				case NkAssetType::Prefab:
					n.libelle = "Prefab";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Code);
					n.role = static_cast<uint16>(editorkit::NkRole::TypeMesh);
					break;
				case NkAssetType::Animation:
				case NkAssetType::AnimationController:
					n.libelle = "Animation";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Code);
					n.role = static_cast<uint16>(editorkit::NkRole::TypeAnim);
					break;
				case NkAssetType::StaticMesh:
				case NkAssetType::SkeletalMesh:
					n.libelle = "Maillage";
					n.role = static_cast<uint16>(editorkit::NkRole::TypeMesh);
					break;
				case NkAssetType::Material:
				case NkAssetType::MaterialInstance:
					n.libelle = "Matériau";
					n.role = static_cast<uint16>(editorkit::NkRole::TypeMat);
					break;
				case NkAssetType::Shader:
					n.libelle = "Shader";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Code);
					break;
				case NkAssetType::Script:
				case NkAssetType::Blueprint:
					n.libelle = "Script";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Code);
					break;
				case NkAssetType::DataTable:
					n.libelle = "Données";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Texte);
					break;
				case NkAssetType::Map:
				case NkAssetType::World:
					n.libelle = "Niveau";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Code);
					n.role = static_cast<uint16>(editorkit::NkRole::TypeMesh);
					break;
				case NkAssetType::SaveGame:
					n.libelle = "Sauvegarde";
					n.icone = static_cast<uint8>(editorkit::NkAssetIcone::Archive);
					break;
				case NkAssetType::Custom:
					// « .nkasset » est un asset du projet de nature libre ; une SOURCE
					// que personne ne reconnait (Custom de DetectType) ne s'importe pas.
					n.libelle = natif ? "Asset" : "Fichier";
					n.importable = natif;
					break;
				default:
					n.libelle = "Fichier";
					n.importable = false;
					break;
			}
			return n;
		}

		NkString NkEditeurDossierContenu(NkEditeurModele &m) {
			// A cote de la scene, comme ses prefabs (NkEditeurCheminPrefab). ⚠️ Tant
			// que l'editeur n'a pas de PROJET (palier U1), le dossier de la scene en
			// tient lieu : CONVENTIONS_FICHIERS.md § 5.
			const NkString scene(NkEditeurChemin(m));
			usize fin = 0;
			for (usize i = 0; i < static_cast<usize>(scene.Length()); ++i) {
				if (Separateur(scene.CStr()[i])) {
					fin = i + 1u;
				}
			}
			NkString dossier(scene.CStr(), fin);
			if (dossier.Empty()) {
				dossier = NkString("./");
			}
			dossier.Append(NK_CONTENU_RACINE);
			// PAS cree ici : regarder le navigateur ne doit rien ecrire sur le
			// disque. L'import cree le dossier qu'il remplit.
			return dossier;
		}

		NkString NkEditeurCheminContenu(NkEditeurModele &m, const char *cheminNavigateur) {
			NkString abs = NkEditeurDossierContenu(m);
			const NkString rel = Relatif(cheminNavigateur);
			if (!rel.Empty()) {
				abs.Append('/');
				abs.Append(rel);
			}
			return abs;
		}

		void NkEditeurListerContenu(NkEditeurModele &m, const char *relatif, NkVector<NkElementContenu> &sortie) {
			sortie.Clear();
			const NkString rel = Relatif(relatif);
			const NkString abs = NkEditeurCheminContenu(m, rel.CStr());
			if (!NkDirectory::Exists(abs.CStr())) {
				return;
			}
			NkVector<NkDirectoryEntry> ent = NkDirectory::GetEntries(abs.CStr());
			for (uint32 i = 0; i < ent.Size(); ++i) {
				const NkDirectoryEntry &d = ent[i];
				if (d.IsHidden || d.Name.Empty() || d.Name.CStr()[0] == '.' || (!d.IsDirectory && !d.IsFile)) {
					continue;
				}
				NkElementContenu e;
				e.nom = d.Name;
				e.relatif = rel;
				if (!e.relatif.Empty()) {
					e.relatif.Append('/');
				}
				e.relatif.Append(d.Name);
				e.dossier = d.IsDirectory;
				e.taille = d.IsDirectory ? 0 : d.Size;
				if (!e.dossier) {
					e.nature = NkEditeurNatureFichier(d.Name.CStr());
				}
				sortie.PushBack(e);
			}
			// Dossiers d'abord, puis par nom : l'ordre ne depend pas du disque.
			for (uint32 i = 1; i < sortie.Size(); ++i) {
				for (uint32 j = i; j > 0; --j) {
					const NkElementContenu &a = sortie[j];
					const NkElementContenu &b = sortie[j - 1];
					const bool avant = (a.dossier && !b.dossier) || (a.dossier == b.dossier && Avant(a.nom, b.nom));
					if (!avant) {
						break;
					}
					const NkElementContenu t = sortie[j];
					sortie[j] = sortie[j - 1];
					sortie[j - 1] = t;
				}
			}
		}

		void NkEditeurDossiersContenu(NkEditeurModele &m, NkVector<NkString> &sortie, uint32 maxi) {
			sortie.Clear();
			Parcourir(NkEditeurDossierContenu(m), NkString(), sortie, maxi, 0u);
		}

		NkRapportImport NkEditeurImporter(NkEditeurModele &m, const char *relatif, const NkVector<NkString> &sources) {
			NkRapportImport r;
			const NkString rel = Relatif(relatif);
			const NkString dossier = NkEditeurCheminContenu(m, rel.CStr());
			NkDirectory::CreateRecursive(dossier.CStr());
			NkString premierRefus;
			for (uint32 i = 0; i < sources.Size(); ++i) {
				const char *src = sources[i].CStr();
				// Un DOSSIER depose n'est pas un fichier : il est refuse, et dit.
				if (NkDirectory::Exists(src) || !NkFile::Exists(src) || !NkEditeurNatureFichier(src).importable) {
					++r.refuses;
					if (premierRefus.Empty()) {
						premierRefus = NomDe(src);
					}
					continue;
				}
				const NkString dest = CheminLibre(dossier, NomDe(src));
				if (dest.Empty() || !NkFile::Copy(src, dest.CStr(), false)) {
					++r.echecs;
					continue;
				}
				++r.importes;
				NkString nav(NK_CONTENU_RACINE);
				if (!rel.Empty()) {
					nav.Append('/');
					nav.Append(rel);
				}
				nav.Append('/');
				nav.Append(NomDe(dest.CStr()));
				r.crees.PushBack(nav);
			}
			NkString lieu(NK_CONTENU_RACINE);
			if (!rel.Empty()) {
				lieu.Append('/');
				lieu.Append(rel);
			}
			NkString annonce = NkString::Format("Import : %u fichier(s) dans %s", r.importes, lieu.CStr());
			if (r.refuses > 0u) {
				annonce.Append(NkString::Format(" ; %u refusé(s), format non reconnu (%s)", r.refuses, premierRefus.CStr()).CStr());
			}
			if (r.echecs > 0u) {
				annonce.Append(NkString::Format(" ; %u copie(s) impossible(s)", r.echecs).CStr());
			}
			NkEditeurAnnoncer(m, annonce.CStr());
			return r;
		}

		NkRapportExport NkEditeurExporter(NkEditeurModele &m, const NkVector<NkString> &chemins, const char *destination) {
			NkRapportExport r;
			if (destination == nullptr || destination[0] == 0 || !NkDirectory::Exists(destination)) {
				NkEditeurAnnoncer(m, "Export : le dossier de destination n'existe pas");
				return r;
			}
			NkString dest(destination);
			while (!dest.Empty() && Separateur(dest.CStr()[dest.Length() - 1u])) {
				dest.PopBack();
			}
			for (uint32 i = 0; i < chemins.Size(); ++i) {
				const NkString src = NkEditeurCheminContenu(m, chemins[i].CStr());
				if (NkDirectory::Exists(src.CStr()) || !NkFile::Exists(src.CStr())) {
					++r.echecs;
					continue;
				}
				const NkString cible = CheminLibre(dest, NomDe(src.CStr()));
				if (cible.Empty() || !NkFile::Copy(src.CStr(), cible.CStr(), false)) {
					++r.echecs;
					continue;
				}
				++r.exportes;
			}
			NkString annonce = NkString::Format("Export : %u asset(s) vers %s", r.exportes, dest.CStr());
			if (r.echecs > 0u) {
				annonce.Append(NkString::Format(" ; %u impossible(s)", r.echecs).CStr());
			}
			NkEditeurAnnoncer(m, annonce.CStr());
			return r;
		}

		const char *NkEditeurFiltreImport() noexcept {
			// Les formats que le moteur LIT (NKImage, NKAudio, NKFont) et ceux du
			// projet. Un filtre de dialogue, pas une table de natures : la nature
			// reste decidee par NkEditeurNatureFichier.
			return "*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.gif;*.svg;*.webp;*.qoi;*.hdr;"
				   "*.wav;*.ogg;*.mp3;*.flac;*.opus;*.ttf;*.otf;"
				   "*.nkscene;*.nkprefab;*.nkanimctl;*.json";
		}

	} // namespace editeur
} // namespace nkentseu
