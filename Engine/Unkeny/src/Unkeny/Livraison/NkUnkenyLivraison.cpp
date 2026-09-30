//
// NkUnkenyLivraison.cpp
// =============================================================================
// Description :
//   La cuisson d'un jeu et sa relecture par le joueur autonome (voir l'en-tete).
//
// Caracteristiques :
//   - Le payload d'un son cuit (`.nksnd`) est defini ICI, faute d'un format de
//     son dans NKSerialization/Asset (il n'y a que la texture,
//     NkTextureAssetFormat.h). Il descendra la-bas au second consommateur :
//         magie "NKSNDPL1" (8 o) | version u16 | taille d'en-tete u16 |
//         frequence u32 | canaux u32 (1) | echantillons u32 |
//         echantillons float32, petit-boutiste
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Livraison/NkUnkenyLivraison.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKImage/Core/NkTextureOven.h"
#include "NKLogger/NkLog.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/NkArchive.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Son/NkUnkenySon.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			NkStringView V(const char *s) {
				return NkStringView(s);
			}

			constexpr char kMagieSon[8] = {'N', 'K', 'S', 'N', 'D', 'P', 'L', '1'};
			constexpr uint32 kTailleEnTeteSon = 24u;

			void EcrireU16(NkVector<uint8> &b, uint32 v) {
				b.PushBack(static_cast<uint8>(v & 0xFFu));
				b.PushBack(static_cast<uint8>((v >> 8) & 0xFFu));
			}
			void EcrireU32(NkVector<uint8> &b, uint32 v) {
				for (int32 k = 0; k < 4; ++k) {
					b.PushBack(static_cast<uint8>((v >> (8 * k)) & 0xFFu));
				}
			}
			uint32 LireU16(const uint8 *p) {
				return static_cast<uint32>(p[0]) | (static_cast<uint32>(p[1]) << 8);
			}
			uint32 LireU32(const uint8 *p) {
				return static_cast<uint32>(p[0]) | (static_cast<uint32>(p[1]) << 8) | (static_cast<uint32>(p[2]) << 16) |
					   (static_cast<uint32>(p[3]) << 24);
			}

			void EncoderSon(const NkSonACuire &son, NkVector<uint8> &sortie) {
				sortie.Clear();
				for (int32 k = 0; k < 8; ++k) {
					sortie.PushBack(static_cast<uint8>(kMagieSon[k]));
				}
				EcrireU16(sortie, 1u);
				EcrireU16(sortie, kTailleEnTeteSon);
				EcrireU32(sortie, static_cast<uint32>(son.frequence));
				EcrireU32(sortie, 1u);
				EcrireU32(sortie, static_cast<uint32>(son.mono.Size()));
				for (usize i = 0; i < son.mono.Size(); ++i) {
					uint32 bits = 0u;
					std::memcpy(&bits, &son.mono[i], sizeof(bits));
					EcrireU32(sortie, bits);
				}
			}

			/// false : ce n'est pas un son cuit, ou il est tronque.
			bool DecoderSon(const NkVector<uint8> &p, NkVector<float32> &mono, int32 &frequence) {
				if (p.Size() < kTailleEnTeteSon || std::memcmp(p.Data(), kMagieSon, 8) != 0) {
					return false;
				}
				const uint32 enTete = LireU16(p.Data() + 10);
				frequence = static_cast<int32>(LireU32(p.Data() + 12));
				const uint32 canaux = LireU32(p.Data() + 16);
				const uint32 n = LireU32(p.Data() + 20);
				if (canaux != 1u || enTete < kTailleEnTeteSon || p.Size() < enTete + static_cast<usize>(n) * 4u) {
					return false;
				}
				mono.Resize(n);
				for (uint32 i = 0; i < n; ++i) {
					const uint32 bits = LireU32(p.Data() + enTete + i * 4u);
					std::memcpy(&mono[i], &bits, sizeof(bits));
				}
				return true;
			}

			NkString Hex64(uint64 v) {
				char b[24];
				std::snprintf(b, sizeof(b), "%016llx", static_cast<unsigned long long>(v));
				return NkString(b);
			}

			uint64 DeHex64(const NkString &s) {
				return static_cast<uint64>(std::strtoull(s.CStr(), nullptr, 16));
			}

			/// Le nom de fichier d'une ressource, unique dans ce qui est deja pris.
			NkString NomUnique(const char *dossier, const char *nom, const char *extension, NkVector<NkString> &pris) {
				const NkString base = NkNomDeFichierSur(nom);
				NkString essai = NkString::Format("%s/%s.%s", dossier, base.CStr(), extension);
				for (int32 n = 2;; ++n) {
					bool libre = true;
					for (usize i = 0; i < pris.Size(); ++i) {
						if (pris[i] == essai) {
							libre = false;
						}
					}
					if (libre) {
						break;
					}
					essai = NkString::Format("%s/%s_%d.%s", dossier, base.CStr(), n, extension);
				}
				pris.PushBack(essai);
				return essai;
			}

			bool Contient(const NkVector<NkString> &liste, const NkString &s) {
				for (usize i = 0; i < liste.Size(); ++i) {
					if (liste[i] == s) {
						return true;
					}
				}
				return false;
			}
		} // namespace

		// =====================================================================
		NkString NkNomDeFichierSur(const char *nom) {
			NkString s;
			if (nom == nullptr || nom[0] == '\0') {
				return NkString("sans_nom");
			}
			for (const char *p = nom; *p != '\0'; ++p) {
				const char c = *p;
				const bool garde = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
								   c == '-' || c == '_';
				s.Append(garde ? c : '_');
			}
			return s;
		}

		uint64 NkEmpreinteTexte(const char *texte, usize longueur) noexcept {
			// FNV-1a 64 : une collision ferait croire « rien n'a change » ; sur
			// 2^64 valeurs et des scenes de jeu, c'est un risque assume (le meme
			// que celui de l'editeur pour « modifiee »).
			uint64 h = 14695981039346656037ull;
			for (usize i = 0; i < longueur; ++i) {
				h ^= static_cast<uint8>(texte[i]);
				h *= 1099511628211ull;
			}
			return h;
		}

		uint64 NkEmpreinteScene(NkScene &scene, const NkTextures2D *textures) {
			NkString json;
			if (!NkSauverSceneJSON(scene, json, textures)) {
				return 0u;
			}
			return NkEmpreinteTexte(json.CStr(), static_cast<usize>(json.Length()));
		}

		bool NkTexturesNommees(NkStringView json, NkVector<NkString> &noms, NkString *erreur) {
			noms.Clear();
			NkArchive a;
			if (!NkJSONReader::ReadArchive(json, a, erreur)) {
				return false;
			}
			NkVector<NkArchive> entites;
			(void)a.GetObjectArray(V("entites"), entites);
			for (usize i = 0; i < entites.Size(); ++i) {
				NkArchive sprite;
				NkString nom;
				if (entites[i].GetObject(V("sprite"), sprite) && sprite.GetString(V("texture"), nom) && !nom.Empty() &&
					!Contient(noms, nom)) {
					noms.PushBack(nom);
				}
			}
			return true;
		}

		// =====================================================================
		// CUIRE
		// =====================================================================
		bool NkCuireJeu(NkScene &scene, const NkTextures2D &textures, const NkDemandeCuisson &demande,
						NkRapportCuisson &rapport) {
			rapport = NkRapportCuisson();
			const NkString racine = demande.dossier.EndsWith('/') || demande.dossier.EndsWith('\\')
										? demande.dossier
										: demande.dossier + "/";
			NkDirectory::CreateRecursive((racine + "textures").CStr());
			NkDirectory::CreateRecursive((racine + "sons").CStr());

			// ── 1. La scene, telle que l'enregistrement l'ecrit ─────────────────
			NkString json;
			if (!NkSauverSceneJSON(scene, json, &textures)) {
				rapport.erreurs.PushBack(NkString("la scene ne se serialise pas"));
				return false;
			}
			rapport.empreinte = NkEmpreinteTexte(json.CStr(), static_cast<usize>(json.Length()));
			const NkString fichierScene("scene.nkscene");
			if (!NkFile::WriteAllText((racine + fichierScene).CStr(), json.CStr())) {
				rapport.erreurs.PushBack(NkString("ecriture impossible : ") + racine + fichierScene);
				return false;
			}
			rapport.fichiers.PushBack(fichierScene);

			// ── 2. Les textures que la scene NOMME ────────────────────────────
			// Seulement celles-la : une texture chargee et jamais posee n'a rien
			// a faire dans l'APK.
			NkVector<NkString> noms;
			(void)NkTexturesNommees(json.View(), noms, nullptr);
			NkVector<NkString> pris;
			NkVector<NkArchive> tableTextures;
			for (usize i = 0; i < noms.Size(); ++i) {
				const char *nom = noms[i].CStr();
				const uint32 id = textures.Trouver(nom);
				int32 w = 0;
				int32 h = 0;
				const uint8 *px = textures.Pixels(id);
				if (id == 0u || px == nullptr || !textures.Taille(id, w, h)) {
					rapport.erreurs.PushBack(NkString::Format("texture « %s » : ses pixels ne sont pas en memoire (garderPixels), non cuite", nom));
					continue;
				}
				NkTexOvenReglages reglages;
				reglages.genererMips = false;			  // NKCanvas ne lit que le niveau 0
				reglages.compression = NKTEXFMT_INCONNU; // brut : NKCanvas televerse du RGBA
				NkVector<uint8> payload;
				NkString err;
				if (!NkTextureOven::CuireDepuisPixels(px, static_cast<uint32>(w), static_cast<uint32>(h), NkImagePixelFormat::NK_RGBA32,
													  static_cast<uint32>(w) * 4u, reglages, payload, &err)) {
					rapport.erreurs.PushBack(NkString::Format("texture « %s » : four : %s", nom, err.CStr()));
					continue;
				}
				NkAssetMetadata meta;
				meta.type = NkAssetType::Texture2D;
				meta.typeName = "unkeny.NkTextures2D";
				meta.assetPath.path = noms[i];
				meta.assetPath.name = noms[i];
				meta.sourceFilePath = noms[i];
				const NkString fichier = NomUnique("textures", nom, NkAssetExtensionFor(NkAssetType::Texture2D), pris);
				if (!NkAssetIO::Write((racine + fichier).CStr(), meta, payload.Data(), payload.Size(), &err)) {
					rapport.erreurs.PushBack(NkString::Format("texture « %s » : ecriture de %s : %s", nom, fichier.CStr(), err.CStr()));
					continue;
				}
				NkArchive entree;
				entree.SetString(V("nom"), noms[i].View());
				entree.SetString(V("fichier"), fichier.View());
				tableTextures.PushBack(entree);
				rapport.fichiers.PushBack(fichier);
				++rapport.textures;
			}

			// ── 3. Les sons demandes ──────────────────────────────────────────
			NkVector<NkArchive> tableSons;
			for (usize i = 0; i < demande.sons.Size(); ++i) {
				const NkSonACuire &son = demande.sons[i];
				NkVector<uint8> payload;
				EncoderSon(son, payload);
				NkAssetMetadata meta;
				meta.type = NkAssetType::Sound;
				meta.typeName = "unkeny.NkSons2D";
				meta.assetPath.path = son.nom;
				meta.assetPath.name = son.nom;
				const NkString fichier = NomUnique("sons", son.nom.CStr(), NkAssetExtensionFor(NkAssetType::Sound), pris);
				NkString err;
				if (!NkAssetIO::Write((racine + fichier).CStr(), meta, payload.Data(), payload.Size(), &err)) {
					rapport.erreurs.PushBack(NkString::Format("son « %s » : ecriture de %s : %s", son.nom.CStr(), fichier.CStr(), err.CStr()));
					continue;
				}
				NkArchive entree;
				entree.SetString(V("nom"), son.nom.View());
				entree.SetString(V("fichier"), fichier.View());
				tableSons.PushBack(entree);
				rapport.fichiers.PushBack(fichier);
				++rapport.sons;
			}

			// ── 3 bis. Les entrees du jeu (30/09) ────────────────────────────
			bool entreesEcrites = false;
			if (!demande.entrees.Empty()) {
				entreesEcrites = NkFile::WriteAllText((racine + NK_LIVRAISON_ENTREES).CStr(), demande.entrees.CStr());
				if (entreesEcrites) {
					rapport.fichiers.PushBack(NkString(NK_LIVRAISON_ENTREES));
				} else {
					rapport.erreurs.PushBack(NkString("entrees : ecriture impossible de ") + racine + NK_LIVRAISON_ENTREES);
				}
			}

			// ── 4. Le sommaire, en dernier : il ne cite que ce qui est ecrit ──
			NkArchive s;
			s.SetString(V("format"), V("unkeny.jeu"));
			s.SetInt32(V("version"), NK_LIVRAISON_VERSION);
			s.SetString(V("nom"), demande.nomJeu.View());
			s.SetString(V("scene"), fichierScene.View());
			s.SetString(V("empreinte"), Hex64(rapport.empreinte).View());
			const NkString vue = NkString::Format("%.9g %.9g", static_cast<double>(demande.vueLargeur), static_cast<double>(demande.vueHauteur));
			s.SetString(V("vue"), vue.View());
			s.SetObjectArray(V("textures"), tableTextures);
			s.SetObjectArray(V("sons"), tableSons);
			if (entreesEcrites) {
				s.SetString(V("entrees"), V(NK_LIVRAISON_ENTREES));
			}
			NkString texte;
			if (!NkJSONWriter::WriteArchive(s, texte, true, 1) ||
				!NkFile::WriteAllText((racine + NK_LIVRAISON_SOMMAIRE).CStr(), texte.CStr())) {
				rapport.erreurs.PushBack(NkString("ecriture impossible : ") + racine + NK_LIVRAISON_SOMMAIRE);
				return false;
			}
			rapport.fichiers.PushBack(NkString(NK_LIVRAISON_SOMMAIRE));
			return true;
		}

		// =====================================================================
		// RELIRE
		// =====================================================================
		NkString NkNomDuJeu(const char *dossier) {
			const NkString texte = NkFile::ReadAllText((NkString(dossier != nullptr ? dossier : "") + NK_LIVRAISON_SOMMAIRE).CStr());
			NkArchive s;
			NkString nom;
			if (texte.Empty() || !NkJSONReader::ReadArchive(texte.View(), s, nullptr)) {
				return nom;
			}
			(void)s.GetString(V("nom"), nom);
			return nom;
		}

		bool NkChargerJeu(const char *dossier, NkScene &scene, NkTextures2D &textures, NkSons2D *sons, NkJeuCharge &sortie) {
			sortie = NkJeuCharge();
			const NkString racine(dossier != nullptr ? dossier : "");

			// ── 1. Le sommaire ─────────────────────────────────────────────────
			const NkString cheminSommaire = racine + NK_LIVRAISON_SOMMAIRE;
			const NkString texte = NkFile::ReadAllText(cheminSommaire.CStr());
			if (texte.Empty()) {
				sortie.erreur = NkString("sommaire du jeu introuvable : ") + cheminSommaire;
				logger.Warn("[unkeny] {0}", sortie.erreur.CStr());
				return false;
			}
			NkArchive s;
			NkString err;
			NkString format;
			if (!NkJSONReader::ReadArchive(texte.View(), s, &err) || !s.GetString(V("format"), format) || !(format == "unkeny.jeu")) {
				sortie.erreur = NkString("sommaire illisible : ") + cheminSommaire + (err.Empty() ? NkString("") : NkString(" (") + err + ")");
				return false;
			}
			nk_int32 version = 0;
			if (!s.GetInt32(V("version"), version) || version < 1 || version > NK_LIVRAISON_VERSION) {
				sortie.erreur = NkString("version de sommaire inconnue : ") + cheminSommaire;
				return false;
			}
			(void)s.GetString(V("nom"), sortie.nom);
			(void)s.GetString(V("scene"), sortie.scene);
			NkString t;
			if (s.GetString(V("empreinte"), t)) {
				sortie.empreinteAttendue = DeHex64(t);
			}
			if (s.GetString(V("vue"), t)) {
				char *fin = nullptr;
				sortie.vueLargeur = std::strtof(t.CStr(), &fin);
				sortie.vueHauteur = std::strtof(fin, nullptr);
			}

			// ── 1 bis. Les entrees (facultatives) : citees mais absentes, c'est
			//    une ressource MANQUANTE nommee ; le jeu joue alors aux liaisons
			//    standard.
			NkString fichierEntrees;
			if (s.GetString(V("entrees"), fichierEntrees) && !fichierEntrees.Empty()) {
				sortie.entrees = NkFile::ReadAllText((racine + fichierEntrees).CStr());
				if (sortie.entrees.Empty()) {
					sortie.manquantes.PushBack(NkString::Format("entrees : fichier %s absent ou illisible (liaisons standard)", fichierEntrees.CStr()));
				}
			}

			// ── 2. Les textures, sous leur nom d'origine ──────────────────────
			NkVector<NkString> signalees;
			NkVector<NkArchive> table;
			(void)s.GetObjectArray(V("textures"), table);
			for (usize i = 0; i < table.Size(); ++i) {
				NkString nom;
				NkString fichier;
				(void)table[i].GetString(V("nom"), nom);
				(void)table[i].GetString(V("fichier"), fichier);
				NkAssetMetadata meta;
				NkVector<nk_uint8> payload;
				NkTexVue vue;
				const NkString chemin = racine + fichier;
				if (!NkAssetIO::ReadFull(chemin.CStr(), meta, payload, &err)) {
					sortie.manquantes.PushBack(NkString::Format("texture « %s » : fichier %s absent ou illisible", nom.CStr(), fichier.CStr()));
					signalees.PushBack(nom);
					continue;
				}
				const bool rgba = NkTexturePayload::Decode(payload.Data(), payload.Size(), vue, &err) &&
								  (vue.formatCode == NKTEXFMT_RGBA8_UNORM || vue.formatCode == NKTEXFMT_RGBA8_SRGB) &&
								  vue.levels.Size() > 0u && vue.levels[0].data != nullptr;
				if (!rgba) {
					sortie.manquantes.PushBack(NkString::Format("texture « %s » : %s n'est pas une texture RGBA cuite", nom.CStr(), fichier.CStr()));
					signalees.PushBack(nom);
					continue;
				}
				// Les lignes, serrees : le four ecrit un pas de ligne a lui, NKCanvas
				// veut w * 4.
				const NkTexNiveauVue &n0 = vue.levels[0];
				const uint32 ligne = n0.width * 4u;
				NkVector<uint8> px;
				px.Resize(static_cast<usize>(ligne) * n0.height);
				const uint32 pas = n0.rowPitch != 0u ? n0.rowPitch : ligne;
				for (uint32 y = 0; y < n0.height; ++y) {
					std::memcpy(px.Data() + static_cast<usize>(ligne) * y, n0.data + static_cast<usize>(pas) * y, ligne);
				}
				const uint32 deja = textures.Trouver(nom.CStr());
				if (deja != 0u) {
					textures.Remplacer(deja, px.Data(), static_cast<int32>(n0.width), static_cast<int32>(n0.height));
				} else {
					textures.Creer(px.Data(), static_cast<int32>(n0.width), static_cast<int32>(n0.height), nom.CStr());
				}
				++sortie.textures;
			}

			// ── 3. Les sons ────────────────────────────────────────────────────
			table.Clear();
			(void)s.GetObjectArray(V("sons"), table);
			for (usize i = 0; sons != nullptr && i < table.Size(); ++i) {
				NkString nom;
				NkString fichier;
				(void)table[i].GetString(V("nom"), nom);
				(void)table[i].GetString(V("fichier"), fichier);
				NkAssetMetadata meta;
				NkVector<nk_uint8> payload;
				NkVector<float32> mono;
				int32 frequence = 0;
				const NkString chemin = racine + fichier;
				if (!NkAssetIO::ReadFull(chemin.CStr(), meta, payload, &err) || !DecoderSon(payload, mono, frequence)) {
					sortie.manquantes.PushBack(NkString::Format("son « %s » : fichier %s absent ou illisible", nom.CStr(), fichier.CStr()));
					continue;
				}
				if (sons->Trouver(nom.CStr()) == 0u) {
					sons->Creer(mono.Data(), mono.Size(), frequence, nom.CStr());
				}
				++sortie.sons;
			}

			// ── 4. La scene ────────────────────────────────────────────────────
			const NkString cheminScene = racine + sortie.scene;
			const NkString json = NkFile::ReadAllText(cheminScene.CStr());
			if (sortie.scene.Empty() || json.Empty()) {
				sortie.erreur = NkString("scene introuvable : ") + cheminScene;
				return false;
			}
			// Ce que la scene NOMME et que le sommaire n'a pas apporte : une
			// texture oubliee a la cuisson, ou un sommaire retouche a la main.
			NkVector<NkString> noms;
			(void)NkTexturesNommees(json.View(), noms, nullptr);
			for (usize i = 0; i < noms.Size(); ++i) {
				if (textures.Trouver(noms[i].CStr()) == 0u && !Contient(signalees, noms[i])) {
					sortie.manquantes.PushBack(NkString::Format("texture « %s » : nommee par la scene, absente du jeu", noms[i].CStr()));
				}
			}
			if (!NkChargerSceneJSON(scene, json.View(), &textures, &err)) {
				sortie.erreur = NkString("scene illisible : ") + cheminScene + " (" + err + ")";
				return false;
			}
			// La gravite des rigides suit celle des particules : le fichier ne
			// garde que la seconde, et elle doit rester UNE -- ce que font
			// l'editeur (Aligner) et Physic2D (AlignerGravite) apres un chargement.
			if (scene.MondePhysique() != nullptr && scene.Particules() != nullptr) {
				const NkVec2f g = scene.Particules()->reglages.gravite;
				scene.MondePhysique()->SetGravity(math::NkVec3f(g.x, g.y, 0.f));
			}
			// L'empreinte AVANT toute trame et tout reglage de camera : c'est la
			// scene relue, pas un instant de la partie.
			sortie.empreinteLue = NkEmpreinteScene(scene, &textures);
			return true;
		}

	} // namespace unkeny
} // namespace nkentseu
