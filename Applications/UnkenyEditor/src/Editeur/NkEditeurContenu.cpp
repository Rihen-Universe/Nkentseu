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

#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "NKEditorKit/Components/NkSilhouettes.h"
#include "NKEditorKit/NkTheme.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKMemory/NkAllocator.h"
#include "NKSerialization/Asset/NkAssetImporter.h"
#include "Unkeny/Anim/NkUnkenyAnimateur.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"

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
					// (2026-10-01) UNE COULEUR PAR NATURE : la bande de type d'Unreal ne
					// dit rien si le son et l'animation portent la meme.
					n.role = static_cast<uint16>(editorkit::NkRole::StatusWarn);
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
					n.role = static_cast<uint16>(editorkit::NkRole::AxisZ); // distinct de la scene (2026-10-01)
					break;
				case NkAssetType::Animation:
				case NkAssetType::AnimationController:
					n.libelle = t == NkAssetType::AnimationController ? "Contrôleur d'animation" : "Animation";
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
				e.date = d.ModificationTime;
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

		namespace {
			/// Le filtre des FICHIERS d'un import : la nature, decidee par la table
			/// existante (NkEditeurNatureFichier), jamais recopiee.
			bool Importable(void *, const char *fichier) {
				return NkEditeurNatureFichier(fichier).importable;
			}
		} // namespace

		NkRapportImport NkEditeurImporter(NkEditeurModele &m, const char *relatif, const NkVector<NkString> &sources) {
			NkRapportImport r;
			const NkString rel = Relatif(relatif);
			const NkString racine = NkEditeurDossierContenu(m);
			const NkString dossier = NkEditeurCheminContenu(m, rel.CStr());
			NkDirectory::CreateRecursive(dossier.CStr());
			// (2026-10-01) LE KIT FAIT LA COPIE (NkContentBrowserDisque.h) : fichiers
			// ET DOSSIERS, confinee au Contenu, jamais d'ecrasement. Un dossier depose
			// etait REFUSE ; Rihen, 01/10 : « pas de poser des fichiers ou des dossiers ».
			const editorkit::NkDisqueRapport d =
				editorkit::NkDisqueImporter(racine.CStr(), dossier.CStr(), sources, &Importable, nullptr);
			r.importes = d.fichiers;
			r.refuses = d.refuses;
			r.echecs = d.echecs;
			r.dossiers = d.dossiers; // la racine de chaque arborescence deposee comprise
			for (uint32 i = 0; i < d.crees.Size(); ++i) {
				const NkString nav = NkEditeurNavigateurDe(m, d.crees[i].CStr());
				if (!nav.Empty()) {
					r.crees.PushBack(nav);
				}
			}
			NkString lieu(NK_CONTENU_RACINE);
			if (!rel.Empty()) {
				lieu.Append('/');
				lieu.Append(rel);
			}
			NkString annonce = NkString::Format("Import : %u fichier(s) dans %s", r.importes, lieu.CStr());
			if (r.dossiers > 0u) {
				annonce.Append(NkString::Format(", %u dossier(s)", r.dossiers).CStr());
			}
			if (r.refuses > 0u) {
				annonce.Append(NkString::Format(" ; %u refusé(s), format non reconnu (%s)", r.refuses, d.premierRefus.CStr()).CStr());
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

		// =====================================================================
		// LES GESTES DU NAVIGATEUR (2026-10-01, document 02 §3.1)
		// =====================================================================
		namespace {
			NkString Absolu(NkEditeurModele &m, const char *nav) {
				return NkEditeurCheminContenu(m, nav);
			}

			/// Un chemin rendu absolu (repertoire courant) et normalise : la racine du
			/// Contenu peut etre relative et le chemin rendu par l'OS absolu.
			NkString AbsoluNormal(const char *p) {
				if (p == nullptr) {
					return NkString();
				}
				const bool abs = p[0] == '/' || Separateur(p[0]) || (p[0] != 0 && p[1] == ':');
				if (abs) {
					return editorkit::NkDisqueNormaliser(p);
				}
				NkString r = NkDirectory::GetCurrentDirectory().ToString();
				r.Append('/');
				r.Append(p);
				return editorkit::NkDisqueNormaliser(r.CStr());
			}

			/// Une phrase d'annonce : « Copie : 2 element(s) dans Contenu/Decor ».
			void Annoncer(NkEditeurModele &m, const char *verbe, uint32 faits, uint32 total, const char *lieu,
						  const NkString &raison) {
				NkString a = NkString::Format("%s : %u élément(s)", verbe, faits);
				if (lieu != nullptr && lieu[0] != 0) {
					a.Append(NkString::Format(" dans %s", lieu).CStr());
				}
				if (faits < total) {
					a.Append(NkString::Format(" ; %u refusé(s)", total - faits).CStr());
					if (!raison.Empty()) {
						a.Append(NkString::Format(" (%s)", raison.CStr()).CStr());
					}
				}
				NkEditeurAnnoncer(m, a.CStr());
			}
		} // namespace

		NkString NkEditeurNavigateurDe(NkEditeurModele &m, const char *chemin) {
			const NkString racine = NkEditeurDossierContenu(m);
			if (chemin == nullptr || !editorkit::NkDisqueSous(racine.CStr(), chemin)) {
				return NkString();
			}
			// La part APRES la racine, comptee sur les formes absolues normalisees
			// (NkDisqueSous a deja verifie que `b` commence par `a` puis un « / »).
			const NkString a = AbsoluNormal(racine.CStr());
			const NkString b = AbsoluNormal(chemin);
			NkString nav(NK_CONTENU_RACINE);
			if (b.Length() > a.Length() + 1u) {
				nav.Append('/');
				nav.Append(b.CStr() + a.Length() + 1u);
			}
			return nav;
		}

		NkString NkEditeurCheminContenuAbsolu(NkEditeurModele &m, const char *cheminNav) {
			return AbsoluNormal(NkEditeurCheminContenu(m, cheminNav).CStr());
		}

		NkString NkEditeurNomProjet(NkEditeurModele &m) {
			const NkString contenu = AbsoluNormal(NkEditeurDossierContenu(m).CStr());
			const NkString nom = editorkit::NkDisqueNom(editorkit::NkDisqueParent(contenu.CStr()).CStr());
			return nom.Empty() ? NkString("Projet") : nom;
		}

		NkString NkEditeurNouveauDossier(NkEditeurModele &m, const char *dossierNav) {
			const NkString racine = NkEditeurDossierContenu(m);
			NkDirectory::CreateRecursive(racine.CStr());
			NkString raison;
			const NkString cree =
				editorkit::NkDisqueNouveauDossier(racine.CStr(), Absolu(m, dossierNav).CStr(), "Nouveau dossier", &raison);
			if (cree.Empty()) {
				NkEditeurAnnoncer(m, NkString::Format("Nouveau dossier : %s", raison.CStr()).CStr());
				return NkString();
			}
			const NkString nav = NkEditeurNavigateurDe(m, cree.CStr());
			NkEditeurAnnoncer(m, NkString::Format("Dossier créé : %s", nav.CStr()).CStr());
			return nav;
		}

		uint32 NkEditeurCopierContenu(NkEditeurModele &m, const NkVector<NkString> &chemins, const char *dossierNav, bool deplacer,
									  NkVector<NkString> *crees) {
			const NkString racine = NkEditeurDossierContenu(m);
			const NkString cible = Absolu(m, dossierNav);
			uint32 faits = 0;
			NkString raison;
			for (uint32 i = 0; i < chemins.Size(); ++i) {
				if (!NkEditeurCheminEstContenu(chemins[i].CStr())) {
					raison = NkString("hors du contenu du projet");
					continue;
				}
				const NkString src = Absolu(m, chemins[i].CStr());
				const NkString dst = deplacer ? editorkit::NkDisqueDeplacer(racine.CStr(), src.CStr(), cible.CStr(), &raison)
											  : editorkit::NkDisqueCopier(racine.CStr(), src.CStr(), cible.CStr(), &raison);
				if (dst.Empty()) {
					continue;
				}
				++faits;
				const NkString nav = NkEditeurNavigateurDe(m, dst.CStr());
				if (deplacer) {
					NkEditeurMetaSuivre(m, chemins[i].CStr(), nav.CStr());
				}
				if (crees != nullptr) {
					crees->PushBack(nav);
				}
			}
			Annoncer(m, deplacer ? "Déplacement" : "Copie", faits, static_cast<uint32>(chemins.Size()), dossierNav, raison);
			return faits;
		}

		uint32 NkEditeurDupliquerContenu(NkEditeurModele &m, const NkVector<NkString> &chemins, NkVector<NkString> *crees) {
			const NkString racine = NkEditeurDossierContenu(m);
			uint32 faits = 0;
			NkString raison;
			for (uint32 i = 0; i < chemins.Size(); ++i) {
				if (!NkEditeurCheminEstContenu(chemins[i].CStr())) {
					continue;
				}
				const NkString dst = editorkit::NkDisqueDupliquer(racine.CStr(), Absolu(m, chemins[i].CStr()).CStr(), &raison);
				if (dst.Empty()) {
					continue;
				}
				++faits;
				if (crees != nullptr) {
					crees->PushBack(NkEditeurNavigateurDe(m, dst.CStr()));
				}
			}
			Annoncer(m, "Duplication", faits, static_cast<uint32>(chemins.Size()), nullptr, raison);
			return faits;
		}

		NkString NkEditeurRenommerContenu(NkEditeurModele &m, const char *cheminNav, const char *nouveauNom) {
			const NkString racine = NkEditeurDossierContenu(m);
			NkString raison;
			if (!NkEditeurCheminEstContenu(cheminNav)) {
				NkEditeurAnnoncer(m, "Renommer : hors du contenu du projet");
				return NkString();
			}
			const NkString dst = editorkit::NkDisqueRenommer(racine.CStr(), Absolu(m, cheminNav).CStr(), nouveauNom, &raison);
			if (dst.Empty()) {
				NkEditeurAnnoncer(m, NkString::Format("Renommer « %s » : %s", nouveauNom != nullptr ? nouveauNom : "", raison.CStr()).CStr());
				return NkString();
			}
			const NkString nav = NkEditeurNavigateurDe(m, dst.CStr());
			NkEditeurMetaSuivre(m, cheminNav, nav.CStr());
			NkEditeurAnnoncer(m, NkString::Format("Renommé : %s", nav.CStr()).CStr());
			return nav;
		}

		uint32 NkEditeurSupprimerContenu(NkEditeurModele &m, const NkVector<NkString> &chemins, bool corbeille) {
			const NkString racine = NkEditeurDossierContenu(m);
			uint32 faits = 0;
			NkString raison;
			for (uint32 i = 0; i < chemins.Size(); ++i) {
				if (!NkEditeurCheminEstContenu(chemins[i].CStr())) {
					continue;
				}
				if (editorkit::NkDisqueSupprimer(racine.CStr(), Absolu(m, chemins[i].CStr()).CStr(), corbeille, &raison)) {
					++faits;
					NkEditeurMetaSuivre(m, chemins[i].CStr(), nullptr);
				}
			}
			Annoncer(m, corbeille ? "Suppression (corbeille)" : "Suppression", faits, static_cast<uint32>(chemins.Size()), nullptr,
					 raison);
			return faits;
		}

		void NkEditeurReferencesContenu(NkEditeurModele &m, const NkVector<NkString> &chemins, NkVector<NkString> &refs) {
			refs.Clear();
			const NkString racine = NkEditeurDossierContenu(m);
			if (!NkDirectory::Exists(racine.CStr())) {
				return;
			}
			const NkVector<NkString> fichiers = NkDirectory::GetFiles(racine.CStr(), "*", NkSearchOption::NK_ALL_DIRECTORIES);
			for (uint32 f = 0; f < fichiers.Size() && refs.Size() < 16u; ++f) {
				const NkAssetType t = NkEditeurNatureFichier(fichiers[f].CStr()).type;
				if (t != NkAssetType::Scene && t != NkAssetType::Prefab) {
					continue;
				}
				const NkString nav = NkEditeurNavigateurDe(m, fichiers[f].CStr());
				bool supprimee = false;
				for (uint32 i = 0; i < chemins.Size(); ++i) {
					supprimee = supprimee || nav == chemins[i];
				}
				if (supprimee) {
					continue; // une scene supprimee ne compte pas parmi ceux qui la citent
				}
				const NkString texte = NkFile::ReadAllText(fichiers[f].CStr());
				for (uint32 i = 0; i < chemins.Size(); ++i) {
					// Un asset se cite par son NOM de fichier (les chemins des textures
					// et des prefabs sont ecrits tels que charges).
					const NkString nom = NomDe(chemins[i].CStr());
					if (!nom.Empty() && texte.Find(nom.CStr()) != NkString::npos) {
						refs.PushBack(nav);
						break;
					}
				}
			}
		}

		NkString NkEditeurNouvelleSceneContenu(NkEditeurModele &m, const char *dossierNav) {
			const NkString dossier = Absolu(m, dossierNav);
			NkDirectory::CreateRecursive(dossier.CStr());
			const NkString chemin = editorkit::NkDisqueCheminLibre(dossier.CStr(), "NouvelleScene.nkscene");
			if (chemin.Empty()) {
				return NkString();
			}
			// Une scene VIDE : ni sol ni caisse -- ceux-la sont la scene neuve de
			// l'editeur, pas un niveau qu'on cree.
			memory::NkAllocator &alloc = memory::NkGetDefaultAllocator();
			NkScene *s = alloc.New<NkScene>();
			NkSceneConfig cfg;
			s->Init(cfg);
			const bool ok = NkSauverSceneFichier(*s, chemin.CStr(), m.RessourcesScene());
			alloc.Delete(s);
			const NkString nav = ok ? NkEditeurNavigateurDe(m, chemin.CStr()) : NkString();
			NkEditeurAnnoncer(m, ok ? NkString::Format("Scène créée : %s", nav.CStr()).CStr() : "Nouvelle scène : écriture impossible");
			return nav;
		}

		NkString NkEditeurNouveauControleurContenu(NkEditeurModele &m, const char *dossierNav) {
			const NkString dossier = Absolu(m, dossierNav);
			NkDirectory::CreateRecursive(dossier.CStr());
			NkString nom("NouveauControleur.");
			nom.Append(NkAssetExtensionFor(NkAssetType::AnimationController));
			const NkString chemin = editorkit::NkDisqueCheminLibre(dossier.CStr(), nom.CStr());
			// Le point de depart : le modele « plateforme », qui existe toujours
			// (NkUnkenyAnimateur.h) -- un controleur sans etat ne s'ecrirait pas.
			anim::NkAnimStateMachine *modele = unkeny::NkModeleAnimateur("plateforme");
			const bool ok = !chemin.Empty() && modele != nullptr && modele->SaveBinary(chemin);
			const NkString nav = ok ? NkEditeurNavigateurDe(m, chemin.CStr()) : NkString();
			NkEditeurAnnoncer(m, ok ? NkString::Format("Contrôleur d'animation créé : %s", nav.CStr()).CStr()
									: "Nouveau contrôleur d'animation : écriture impossible");
			return nav;
		}

		NkString NkEditeurNouveauPrefabContenu(NkEditeurModele &m, const char *dossierNav) {
			if (!m.aSelection || !m.scene.Monde().IsAlive(m.selection) || m.etat != NkEtatJeu::NK_EDITION) {
				NkEditeurAnnoncer(m, "Nouveau prefab : choisissez d'abord une entité dans la scène");
				return NkString();
			}
			const NkString dossier = Absolu(m, dossierNav);
			NkDirectory::CreateRecursive(dossier.CStr());
			const NkEtiquette *e = m.scene.Monde().Get<NkEtiquette>(m.selection);
			NkString nom(e != nullptr && e->nom[0] != 0 ? e->nom : "Prefab");
			nom.Append(".nkprefab");
			const NkString chemin = editorkit::NkDisqueCheminLibre(dossier.CStr(), nom.CStr());
			const uint32 id = chemin.Empty() ? 0u : m.prefabs.Creer(m.scene, m.selection, chemin.CStr());
			const bool ok = id != 0u && m.prefabs.Enregistrer(m.scene, id, chemin.CStr(), m.RessourcesScene());
			const NkString nav = ok ? NkEditeurNavigateurDe(m, chemin.CStr()) : NkString();
			NkEditeurAnnoncer(m, ok ? NkString::Format("Prefab créé : %s", nav.CStr()).CStr() : "Nouveau prefab : écriture impossible");
			return nav;
		}

		void NkEditeurMetaSuivre(NkEditeurModele &m, const char *ancienNav, const char *nouveauNav) {
			const NkString racine = NkEditeurDossierContenu(m);
			NkString f = racine;
			f.Append('/');
			f.Append(editorkit::NK_DISQUE_META);
			// Pas de memoire : rien a suivre, et surtout rien a CREER.
			if (!NkFile::Exists(f.CStr())) {
				return;
			}
			const NkString a = Relatif(ancienNav);
			if (a.Empty()) {
				return; // la racine ne se renomme ni ne s'oublie
			}
			editorkit::NkDisqueMeta meta;
			editorkit::NkDisqueMetaLire(racine.CStr(), meta);
			if (nouveauNav != nullptr && nouveauNav[0] != 0) {
				meta.Renommer(a, Relatif(nouveauNav));
			} else {
				meta.Oublier(a);
			}
			editorkit::NkDisqueMetaEcrire(racine.CStr(), meta);
		}

	} // namespace editeur
} // namespace nkentseu
