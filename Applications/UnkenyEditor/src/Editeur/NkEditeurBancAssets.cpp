//
// NkEditeurBancAssets.cpp
// =============================================================================
// Description :
//   `UnkenyEditor --selftest` : les retours de Rihen du 01/10 sur les CARTES des
//   Details, « Ajouter un composant », les ASSETS qui s'ouvrent, les prefabs,
//   le panneau Placer des acteurs, l'eclairage de la scene et les EFFETS
//   (feuille de route R33 et R34). Compte A PART (« BANC ASSETS EDITEUR ») :
//   les bancs d'avant gardent leurs comptes. Les gestes passent par la VRAIE
//   trame (NkEditeurBancTrame.h), la souris par NkEditeurSouris -- jamais la
//   vraie souris.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (m1)  RIEN HORS CADRE : une etoile posee (forme, collisionneur, corps) et
//         ancree a l'ecran montre QUATRE CARTES (Forme 2D, Collisionneur, Corps
//         rigide, Ancrage) ; la pastille Physique ne montre que Collisionneur et
//         Corps, Rendu montre la Forme, Acteur l'Ancrage ; le TYPE du
//         collisionneur se CHOISIT (clic sur « Cercle » : un cercle) ; le genre de
//         la forme aussi ; la recherche « Remplissage » ne garde que la Forme
//         (un bloc hors carte ne se cherchait pas)
//   (m2)  « AJOUTER UN COMPOSANT » (le + vert) sur un acteur vide propose,
//         rangés Physique / Rendu / Animation / Audio / Acteur : Collisionneur,
//         Corps rigide, Forme 2D, Animation (sprites), Animateur (le modele
//         « plateforme » ET le .nkanimctl du Contenu), Emetteur, Lumiere, Son,
//         Ancrage ; « anim » ne garde que l'animation et les animateurs ; un clic
//         sur le controleur du Contenu pose l'animateur (modele lu, animation de
//         sprites ajoutee) et ses deux cartes paraissent ; Forme 2D et Ancrage
//         s'ajoutent de meme
//   (m3)  CHAQUE ASSET S'OUVRE (double-clic dans le Content Browser) : une
//         texture ouvre son onglet (apercu, « Pixel » choisi puis enregistre dans
//         Contenu/.nkreglages et relu) ; une police montre 4 tailles ; un son se
//         LIT puis s'ARRETE ; un controleur liste ses etats ; un PREFAB ouvre le
//         mode prefab (la scene de cote, le prefab seul, sa racine choisie) ; sa
//         teinte changee puis « Enregistrer » et « Revenir a la scene » : la
//         scene revient ENTIERE et son instance a la nouvelle teinte
//   (m4)  PREFABS GLISSES : un vrai prefab (« Nouveau prefab » de la selection)
//         glisse du Content Browser dans l'OUTLINER nait au centre de la vue ;
//         glisse dans la VUE, au point lache ; les deux sont des instances du
//         prefab, visibles dans l'Outliner ; la Ctrl+Z retire la derniere
//
//   Les captures hors ecran : `--captures-assets=DOSSIER` (rasterisees, sans
//   fenetre ni GPU, comme --captures-formes).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurAssets.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurPlacer.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/Core/NkImage.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gE = 0, gR = 0;
			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}
			uint32 Bit(NkCarteEditeur k) {
				return 1u << static_cast<uint32>(k);
			}
			nkgui::NkVec2 Milieu(const nkgui::NkRect &r) {
				return nkgui::NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
			}
			/// La rangee `libelle` de la carte `carte`, dessinee a la derniere trame.
			bool Rangee(NkEditeurInterface &ui, NkCarteEditeur carte, const char *libelle, nkgui::NkRect &champ) {
				for (uint32 k = 0; k < ui.detailsRangees.Size(); ++k) {
					const NkEditeurInterface::NkRangeeDetails &r = ui.detailsRangees[k];
					if (r.carte == static_cast<int32>(carte) && r.libelle == NkString(libelle)) {
						champ = r.champ;
						return true;
					}
				}
				return false;
			}
			/// Cliquer le segment `i` (sur `n`) d'un choix segmente.
			void Segment(NkEditeurBancTrame &t, const nkgui::NkRect &champ, int32 i, int32 n) {
				const float32 w = champ.w / static_cast<float32>(n);
				t.Clic(0, champ.x + w * (static_cast<float32>(i) + 0.5f), champ.y + champ.h * 0.5f);
			}
			void Pastille(NkEditeurBancTrame &t, int32 cat) {
				const nkgui::NkVec2 p = Milieu(t.Ui().detailsPastilles[cat]);
				t.Clic(0, p.x, p.y);
				t.Trame();
			}

			/// La ligne de menu peinte dont le libelle CONTIENT `mot` (-1 sinon).
			int32 LigneMenu(NkEditeurInterface &ui, const char *mot) {
				for (uint32 k = 0; k < ui.menuLignes.Size(); ++k) {
					if (std::strstr(ui.menuLignes[k].libelle.CStr(), mot) != nullptr) {
						return static_cast<int32>(k);
					}
				}
				return -1;
			}
			/// Ouvre « Ajouter un composant » (le + vert) et clique la ligne `mot`.
			bool AjouterParMenu(NkEditeurBancTrame &t, const char *mot) {
				NkEditeurInterface &ui = t.Ui();
				t.Fermer();
				const nkgui::NkVec2 p = Milieu(ui.detailsAjouter);
				t.Clic(0, p.x, p.y);
				t.Trame();
				const int32 k = LigneMenu(ui, mot);
				if (k < 0) {
					return false;
				}
				const nkgui::NkVec2 q = Milieu(ui.menuLignes[static_cast<uint32>(k)].r);
				t.Clic(0, q.x, q.y);
				t.Fermer();
				return true;
			}

			uint32 NbEntites(NkScene &s) {
				NkVector<ecs::NkEntityId> ids;
				s.Entites(ids);
				return static_cast<uint32>(ids.Size());
			}
			ecs::NkEntityId Par(NkScene &s, const char *nom) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (std::strcmp(e.nom, nom) == 0 && !t.IsValid()) {
						t = id;
					}
				});
				return t;
			}
			/// Une image 8 x 8 (damier rouge / jaune), une VRAIE PNG (NKImage).
			bool EcrireImage(const char *chemin) {
				uint8 px[8 * 8 * 4];
				for (int32 i = 0; i < 64; ++i) {
					const bool a = ((i % 8) + (i / 8)) % 2 == 0;
					px[i * 4 + 0] = 230;
					px[i * 4 + 1] = a ? 60 : 200;
					px[i * 4 + 2] = 40;
					px[i * 4 + 3] = 255;
				}
				NkImage img = NkImage::Wrap(px, 8, 8, NkImagePixelFormat::NK_RGBA32);
				return img.SavePNG(chemin);
			}
			/// Un VRAI son : 0,25 s de la4 (440 Hz), PCM 16 bits mono 22 050 Hz.
			bool EcrireSon(const char *chemin) {
				const uint32 hz = 22050u, n = hz / 4u;
				NkVector<nk_uint8> o;
				auto U32 = [&](uint32 v) {
					for (int32 k = 0; k < 4; ++k) {
						o.PushBack(static_cast<nk_uint8>((v >> (8 * k)) & 0xFFu));
					}
				};
				auto U16 = [&](uint32 v) {
					o.PushBack(static_cast<nk_uint8>(v & 0xFFu));
					o.PushBack(static_cast<nk_uint8>((v >> 8) & 0xFFu));
				};
				auto Mot = [&](const char *t) {
					for (int32 k = 0; k < 4; ++k) {
						o.PushBack(static_cast<nk_uint8>(t[k]));
					}
				};
				Mot("RIFF");
				U32(36u + n * 2u);
				Mot("WAVE");
				Mot("fmt ");
				U32(16u);
				U16(1u);
				U16(1u);
				U32(hz);
				U32(hz * 2u);
				U16(2u);
				U16(16u);
				Mot("data");
				U32(n * 2u);
				for (uint32 i = 0; i < n; ++i) {
					const float32 s = math::NkSin(6.2831853f * 440.f * static_cast<float32>(i) / static_cast<float32>(hz)) * 0.4f;
					U16(static_cast<uint32>(static_cast<uint16>(static_cast<int16>(s * 32767.f))));
				}
				return NkFile::WriteAllBytes(chemin, o);
			}
			/// Une police du depot (NKCode), copiee dans le Contenu du banc.
			bool CopierPolice(const char *destination) {
				static const char *kSources[] = {"../../../../Applications/NKCode/data/fonts/NotoSans-Regular.ttf",
												 "Applications/NKCode/data/fonts/NotoSans-Regular.ttf", "C:/Windows/Fonts/arial.ttf"};
				for (const char *s : kSources) {
					if (NkFile::Exists(s)) {
						return NkFile::Copy(s, destination, true);
					}
				}
				return false;
			}

			/// La scene du point 1 : une etoile posee (forme, collisionneur, corps
			/// statique), ancree a l'ecran, choisie.
			ecs::NkEntityId SceneCartes(NkEditeurModele &m) {
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				const ecs::NkEntityId e = NkEditeurPoserForme(m, NkGenreForme2D::NK_ETOILE, NkVec2f(0.f, 2.f));
				NkEditeurAjouterComposant(m, e, NkComposantEditeur::NK_ANCRAGE);
				m.selection = e;
				m.aSelection = true;
				return e;
			}

			/// La trame courante, RASTERISEE sans fenetre ni GPU (NkGuiDrawListRaster),
			/// puis ecrite en PNG (NKImage) -- le patron de NkEditeurBancFormes.cpp.
			bool EcrirePng(NkEditeurBancTrame &T, const char *chemin) {
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
				const int32 w = static_cast<int32>(T.W), h = static_cast<int32>(T.H);
				ras->Init(w, h);
				ras->Effacer(0x141414FFu);
				ras->PoserTexture(T.police->TexId(), T.police->pixels, T.police->atlasW, T.police->atlasH, 1);
				for (uint32 k = 0; k < T.m.textures.Nombre(); ++k) {
					const uint32 id = NkTextures2D::kPremierId + k;
					int32 tw = 0, th = 0;
					if (T.m.textures.Taille(id, tw, th) && T.m.textures.Pixels(id) != nullptr) {
						ras->PoserTexture(id, T.m.textures.Pixels(id), tw, th, 4);
					}
				}
				// L'apercu d'une police ouverte (NkEditeurAssets.cpp) : son atlas.
				if (const nkgui::NkGuiFont *pa = T.pui->policeApercu) {
					if (pa->pixels != nullptr) {
						ras->PoserTexture(pa->TexId(), pa->pixels, pa->atlasW, pa->atlasH, 1);
					}
				}
				ras->Rasteriser(T.pctx->dl);
				ras->Rasteriser(T.pctx->dlOverlay);
				NkImage img = NkImage::Wrap(const_cast<uint8 *>(ras->Pixels()), w, h, NkImagePixelFormat::NK_RGBA32);
				const bool ok = img.SavePNG(chemin);
				std::printf("  capture %s : %s\n", chemin, ok ? "ok" : "ECHEC");
				tas.Delete(ras);
				return ok;
			}
		} // namespace

		int32 NkEditeurLancerBancAssets() {
			gE = gR = 0;
			std::printf("\nUnkenyEditor — banc des cartes, assets et effets (retours du 01/10, R33 / R34)\n\n");
			NkEditeurModele *pm = memory::NkGetDefaultAllocator().New<NkEditeurModele>(); // gros : sur le tas
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);
			const NkString cheminAvant = m.chemin;

			// (m1) RIEN HORS CADRE DANS LES DETAILS (retour 1 de Rihen).
			{
				const ecs::NkEntityId e = SceneCartes(m);
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				ui.hauteurTiroir = 120.f;
				t.Trame();
				t.Trame();
				const uint32 attendues = Bit(NkCarteEditeur::NK_FORME) | Bit(NkCarteEditeur::NK_COLLISIONNEUR) | Bit(NkCarteEditeur::NK_CORPS) |
										 Bit(NkCarteEditeur::NK_ANCRAGE);
				const bool cartes = (ui.detailsCartesDessinees & attendues) == attendues;
				// Les pastilles : chaque carte dans la sienne.
				Pastille(t, 3);
				const uint32 physique = Bit(NkCarteEditeur::NK_COLLISIONNEUR) | Bit(NkCarteEditeur::NK_CORPS) | Bit(NkCarteEditeur::NK_CORPS_MOU);
				const bool pPhysique = (ui.detailsCartesDessinees & ~physique) == 0u &&
									   (ui.detailsCartesDessinees & Bit(NkCarteEditeur::NK_COLLISIONNEUR)) != 0u &&
									   (ui.detailsCartesDessinees & Bit(NkCarteEditeur::NK_CORPS)) != 0u;
				// Le TYPE du collisionneur se choisit : « Cercle » (2e de la 1re rangee de 3).
				nkgui::NkRect champ;
				const bool aType = Rangee(ui, NkCarteEditeur::NK_COLLISIONNEUR, "Type", champ);
				if (aType) {
					Segment(t, champ, 1, 3);
					t.Trame();
				}
				const NkCollisionneur2D *col = m.scene.Monde().Get<NkCollisionneur2D>(e);
				const bool type = aType && col != nullptr && col->forme == NkForme2D::NK_CERCLE;
				Pastille(t, 4);
				const bool pRendu = (ui.detailsCartesDessinees & Bit(NkCarteEditeur::NK_FORME)) != 0u &&
									(ui.detailsCartesDessinees & physique) == 0u;
				// Le GENRE de la forme se choisit : « Cercle » (2e de la 1re rangee de 3).
				const bool aGenre = Rangee(ui, NkCarteEditeur::NK_FORME, "Genre", champ);
				if (aGenre) {
					Segment(t, champ, 1, 3);
					t.Trame();
				}
				const NkRenduForme2D *f = m.scene.Monde().Get<NkRenduForme2D>(e);
				const bool genre = aGenre && f != nullptr && f->genre == NkGenreForme2D::NK_CERCLE;
				Pastille(t, 2);
				const bool pActeur = (ui.detailsCartesDessinees & Bit(NkCarteEditeur::NK_ANCRAGE)) != 0u;
				Pastille(t, 0);
				// La recherche trouve une rangee de la forme : elle est DANS une carte.
				std::snprintf(ui.detailsRecherche, sizeof(ui.detailsRecherche), "%s", "Remplissage");
				t.Trame();
				t.Trame();
				const bool cherche = ui.detailsCartesDessinees == Bit(NkCarteEditeur::NK_FORME);
				ui.detailsRecherche[0] = '\0';
				t.Trame();
				const bool ok = cartes && pPhysique && type && pRendu && genre && pActeur && cherche;
				if (!ok) {
					std::printf("        cartes %d (%x) physique %d type %d rendu %d genre %d acteur %d cherche %d%c", cartes,
								ui.detailsCartesDessinees, pPhysique, type, pRendu, genre, pActeur, cherche, 10);
				}
				Temoin(ok, "(m1) Details : Forme, Collisionneur (type au choix), Corps, Ancrage en cartes rangees",
					   static_cast<float32>(cartes + pPhysique + type + pRendu + genre + pActeur + cherche));
			}

			// (m2) « AJOUTER UN COMPOSANT » propose tout, range, et l'animateur se voit.
			{
				NkDirectory::Delete("banc_m2", true);
				NkDirectory::CreateRecursive("banc_m2/projet/Contenu");
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				m.chemin = NkString("banc_m2/projet/scene.nkscene");
				const NkString ctl = NkEditeurNouveauControleurContenu(m, "Contenu");
				const ecs::NkEntityId e = NkEditeurCreerEntite(m, "Vide", NkVec2f(0.f, 1.f));
				m.selection = e;
				m.aSelection = true;
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				ui.hauteurTiroir = 120.f;
				t.Trame();
				t.Trame();
				const nkgui::NkVec2 p = Milieu(ui.detailsAjouter);
				t.Clic(0, p.x, p.y);
				t.Trame();
				static const char *kAttendus[] = {"Physique", "Collisionneur", "Corps rigide", "Rendu", "Forme 2D", "Émetteur", "Lumière",
												  "Animation", "Animation (sprites)", "Animateur : plateforme", "Animateur : NouveauControleur",
												  "Audio", "Son", "Acteur", "Ancrage à l'écran"};
				int32 trouves = 0;
				const int32 nAttendus = static_cast<int32>(sizeof(kAttendus) / sizeof(kAttendus[0]));
				for (int32 k = 0; k < nAttendus; ++k) {
					if (LigneMenu(ui, kAttendus[k]) >= 0) {
						++trouves;
					} else {
						std::printf("        absent du menu : %s\n", kAttendus[k]);
					}
				}
				const bool propose = trouves == nAttendus && ui.menu == NkMenuEditeur::NK_COMPOSANT;
				// « anim » : les seules lignes qui agissent sont l'animation et les animateurs.
				t.Taper("anim");
				t.Trame();
				int32 agissantes = 0, animees = 0;
				for (uint32 k = 0; k < ui.menuLignes.Size(); ++k) {
					if (ui.menuLignes[k].action != NK_A_AUCUNE) {
						++agissantes;
						animees += std::strstr(ui.menuLignes[k].libelle.CStr(), "Anim") != nullptr ? 1 : 0;
					}
				}
				const bool filtre = agissantes >= 3 && animees == agissantes;
				// Un clic sur le controleur du Contenu : l'animateur, son modele lu.
				bool anime = false;
				const int32 kc = LigneMenu(ui, "NouveauControleur");
				if (kc >= 0) {
					const nkgui::NkVec2 q = Milieu(ui.menuLignes[static_cast<uint32>(kc)].r);
					t.Clic(0, q.x, q.y);
					t.Fermer();
					const NkAnimateur2D *a = m.scene.Monde().Get<NkAnimateur2D>(e);
					anime = a != nullptr && std::strcmp(a->modele, "NouveauControleur") == 0 && NkModeleAnimateur("NouveauControleur") != nullptr &&
							m.scene.Monde().Has<NkAnimSprite2D>(e);
				}
				t.Trame();
				const uint32 deux = Bit(NkCarteEditeur::NK_ANIMATION) | Bit(NkCarteEditeur::NK_ANIMATEUR);
				const bool cartes = (ui.detailsCartesDessinees & deux) == deux;
				const bool forme = AjouterParMenu(t, "Forme 2D") && m.scene.Monde().Has<NkRenduForme2D>(e);
				const bool ancrage = AjouterParMenu(t, "Ancrage") && m.scene.Monde().Has<NkAncrageEcran2D>(e);
				const bool ok = !ctl.Empty() && propose && filtre && anime && cartes && forme && ancrage;
				if (!ok) {
					std::printf("        ctl '%s' propose %d (%d/%d) filtre %d (%d/%d) anime %d cartes %d forme %d ancrage %d%c", ctl.CStr(), propose,
								trouves, nAttendus, filtre, animees, agissantes, anime, cartes, forme, ancrage, 10);
				}
				Temoin(ok, "(m2) Ajouter un composant : range, animateur (.nkanimctl), forme, ancrage",
					   static_cast<float32>(trouves));
				NkDirectory::Delete("banc_m2", true);
			}

			// (m3) CHAQUE ASSET S'OUVRE (retour 3 de Rihen).
			{
				NkDirectory::Delete("banc_m3", true);
				NkDirectory::CreateRecursive("banc_m3/projet/Contenu");
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				m.chemin = NkString("banc_m3/projet/scene.nkscene");
				m.projet = NkString();
				const bool fichiers = EcrireImage("banc_m3/projet/Contenu/damier.png") && EcrireSon("banc_m3/projet/Contenu/la.wav");
				const bool police = CopierPolice("banc_m3/projet/Contenu/texte.ttf");
				const NkString ctl = NkEditeurNouveauControleurContenu(m, "Contenu");
				// Le prefab : une caisse teintee, faite prefab (elle en devient l'instance).
				const ecs::NkEntityId caisse = m.scene.Creer("Caisse", NkVec2f(2.f, 1.f));
				NkSprite2D sp;
				sp.couleur = 0x804020FFu;
				m.scene.Monde().Add<NkSprite2D>(caisse, sp);
				m.selection = caisse;
				m.aSelection = true;
				const NkString prefab = NkEditeurNouveauPrefabContenu(m, "Contenu");
				const uint64 uidCaisse = m.scene.AssurerUid(caisse);
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				ui.sonsMuets = true; // pas de peripherique au banc
				ui.voirTiroir = true;
				ui.ongletTiroir = 0;
				ui.hauteurTiroir = 260.f;
				t.Trame();
				t.Trame();
				auto Ouvrir = [&](const char *nav) {
					t.Fermer();
					const int32 k = t.Carte(nav);
					if (k >= 0) {
						const nkgui::NkRect r = ui.contenuCartes[static_cast<uint32>(k)];
						t.DoubleClic(r.x + r.w * 0.5f, r.y + r.h * 0.3f);
						t.Trame();
					}
					return k >= 0 && ui.ongletActif >= 0;
				};
				auto Actif = [&]() -> NkOngletAsset * {
					return ui.ongletActif >= 0 ? ui.onglets[static_cast<uint32>(ui.ongletActif)] : nullptr;
				};
				// La TEXTURE : apercu, « Pixel », Enregistrer, relu.
				bool texture = Ouvrir("Contenu/damier.png") && Actif()->genre == NkGenreAsset::NK_TEXTURE && Actif()->texId != 0u &&
							   ui.assetApercu.w > 0.f;
				if (texture) {
					t.Clic(0, ui.assetFiltrage.x + ui.assetFiltrage.w * 0.75f, ui.assetFiltrage.y + ui.assetFiltrage.h * 0.5f);
					t.Trame();
					const nkgui::NkVec2 e = Milieu(ui.assetEnregistrer);
					t.Clic(0, e.x, e.y);
					t.Trame();
					NkReglagesTexture relu;
					texture = Actif()->texture.pixel && NkEditeurLireReglagesTexture(m, "Contenu/damier.png", relu) && relu.pixel &&
							  NkFile::Exists("banc_m3/projet/Contenu/.nkreglages");
				}
				// La POLICE : quatre tailles.
				const bool policeOk = !police || (Ouvrir("Contenu/texte.ttf") && Actif()->genre == NkGenreAsset::NK_POLICE && Actif()->policeOk &&
												  ui.assetTailles == 4 && ui.policeApercu == Actif()->police);
				// Le SON : lire, puis arreter.
				bool son = Ouvrir("Contenu/la.wav") && Actif()->genre == NkGenreAsset::NK_SON && Actif()->sonId != 0u;
				if (son) {
					const nkgui::NkVec2 l = Milieu(ui.assetLire);
					t.Clic(0, l.x, l.y);
					const bool lit = Actif()->voix != 0u;
					t.Clic(0, l.x, l.y);
					son = lit && Actif()->voix == 0u;
				}
				// Le CONTROLEUR : ses etats, en lecture.
				const bool controleur = !ctl.Empty() && Ouvrir(ctl.CStr()) && Actif()->genre == NkGenreAsset::NK_CONTROLEUR && ui.assetEtats >= 2;
				// Le PREFAB : le mode prefab, la teinte changee, enregistree, et l'instance suit.
				const uint32 avant = NbEntites(m.scene);
				bool mode = !prefab.Empty() && Ouvrir(prefab.CStr()) && ui.modePrefab != nullptr && m.aSelection;
				const uint32 dansPrefab = NbEntites(m.scene);
				mode = mode && dansPrefab < avant && dansPrefab >= 1u;
				bool suit = false;
				if (mode) {
					NkSprite2D *s = m.scene.Monde().Get<NkSprite2D>(m.selection);
					if (s != nullptr) {
						s->couleur = 0x20C040FFu;
					}
					t.Trame();
					const nkgui::NkVec2 e = Milieu(ui.assetEnregistrer);
					t.Clic(0, e.x, e.y);
					const nkgui::NkVec2 r = Milieu(ui.assetRevenir);
					t.Clic(0, r.x, r.y);
					t.Trame();
					const ecs::NkEntityId c2 = m.scene.EntiteParUid(uidCaisse);
					const NkSprite2D *s2 = c2.IsValid() ? m.scene.Monde().Get<NkSprite2D>(c2) : nullptr;
					suit = ui.modePrefab == nullptr && ui.ongletActif == -1 && NbEntites(m.scene) == avant && s2 != nullptr && s2->couleur == 0x20C040FFu;
				}
				NkEditeurCadre cadre = t.Cadre();
				NkEditeurFermerTousOnglets(cadre);
				const bool ok = fichiers && texture && policeOk && son && controleur && mode && suit;
				if (!ok) {
					std::printf("        fichiers %d texture %d police %d (%d) son %d controleur %d mode %d (%u -> %u) suit %d prefab '%s'%c", fichiers,
								texture, policeOk, police, son, controleur, mode, avant, dansPrefab, suit, prefab.CStr(), 10);
				}
				Temoin(ok, "(m3) assets ouverts : texture (pixel enregistre), police, son lu/arrete, controleur, prefab",
					   static_cast<float32>(texture + policeOk + son + controleur + mode + suit));
				NkDirectory::Delete("banc_m3", true);
			}

			// (m4) PREFABS GLISSES DANS L'OUTLINER ET DANS LA VUE (retour 4 de Rihen).
			{
				NkDirectory::Delete("banc_m4", true);
				NkDirectory::CreateRecursive("banc_m4/projet/Contenu");
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				m.chemin = NkString("banc_m4/projet/scene.nkscene");
				m.projet = NkString();
				const ecs::NkEntityId caisse = m.scene.Creer("Tonneau", NkVec2f(-2.f, 1.f));
				NkSprite2D sp;
				sp.couleur = 0x6A4A2AFFu;
				m.scene.Monde().Add<NkSprite2D>(caisse, sp);
				m.selection = caisse;
				m.aSelection = true;
				const NkString prefab = NkEditeurNouveauPrefabContenu(m, "Contenu");
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				ui.hauteurTiroir = 240.f;
				t.Trame();
				t.Trame();
				auto Instance = [&](ecs::NkEntityId e) {
					const NkInstancePrefab2D *ip = e.IsValid() ? m.scene.Monde().Get<NkInstancePrefab2D>(e) : nullptr;
					const NkInstancePrefab2D *is = m.scene.Monde().Get<NkInstancePrefab2D>(caisse);
					return ip != nullptr && is != nullptr && ip->prefab == is->prefab;
				};
				auto DansOutliner = [&](ecs::NkEntityId e) {
					for (uint32 k = 0; k < ui.arbreEntites.Size(); ++k) {
						if (ui.arbreEntites[k] == e) {
							return true;
						}
					}
					return false;
				};
				const int32 k = t.Carte(prefab.CStr());
				bool outliner = false, vue = false, annule = false;
				if (k >= 0) {
					const nkgui::NkRect r = ui.contenuCartes[static_cast<uint32>(k)];
					// Dans l'OUTLINER : au centre de la vue.
					const uint32 n0 = NbEntites(m.scene);
					t.Glisser(r.x + r.w * 0.5f, r.y + r.h * 0.3f, ui.outliner.x + ui.outliner.w * 0.5f, ui.outliner.y + ui.outliner.h * 0.6f, 8);
					t.Trame();
					const ecs::NkEntityId a = m.aSelection ? m.selection : ecs::NkEntityId::Invalid();
					const NkTransform2D *ta = a.IsValid() ? m.scene.Monde().Get<NkTransform2D>(a) : nullptr;
					const NkVec2f centre = m.scene.Camera().Centre();
					outliner = NbEntites(m.scene) == n0 + 1u && a != caisse && Instance(a) && DansOutliner(a) && ta != nullptr &&
							   math::NkAbs(ta->position.x - centre.x) < 0.01f && math::NkAbs(ta->position.y - centre.y) < 0.01f;
					// Dans la VUE : au point lache.
					const nkgui::NkVec2 p{ui.viseur.x + ui.viseur.w * 0.7f, ui.viseur.y + ui.viseur.h * 0.4f};
					const NkVec2f monde = m.scene.Camera().EcranVersMonde(NkVec2f(p.x, p.y));
					t.Glisser(r.x + r.w * 0.5f, r.y + r.h * 0.3f, p.x, p.y, 8);
					t.Trame();
					const ecs::NkEntityId b = m.aSelection ? m.selection : ecs::NkEntityId::Invalid();
					const NkTransform2D *tb = b.IsValid() ? m.scene.Monde().Get<NkTransform2D>(b) : nullptr;
					vue = NbEntites(m.scene) == n0 + 2u && b != a && Instance(b) && DansOutliner(b) && tb != nullptr &&
						  math::NkAbs(tb->position.x - monde.x) < 0.05f && math::NkAbs(tb->position.y - monde.y) < 0.05f;
					// Ctrl+Z : la derniere instance s'en va.
					NkEditeurAnnuler(m);
					annule = NbEntites(m.scene) == n0 + 1u;
				}
				const bool ok = !prefab.Empty() && k >= 0 && outliner && vue && annule;
				if (!ok) {
					std::printf("        prefab '%s' carte %d outliner %d vue %d annule %d%c", prefab.CStr(), k, outliner, vue, annule, 10);
				}
				Temoin(ok, "(m4) prefab glisse : dans l'Outliner (centre de la vue) et dans la vue (point lache)",
					   static_cast<float32>(outliner + vue + annule));
				NkDirectory::Delete("banc_m4", true);
			}

			m.chemin = cheminAvant;
			m.projet = NkString();
			memory::NkGetDefaultAllocator().Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC ASSETS EDITEUR REUSSI" : "BANC ASSETS EDITEUR EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

		// =====================================================================
		// LES CAPTURES HORS ECRAN (--captures-assets=DOSSIER)
		// =====================================================================
		int32 NkEditeurCapturesAssets(const char *dossier) {
			NkEditeurModele *pm = memory::NkGetDefaultAllocator().New<NkEditeurModele>();
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkDirectory::CreateRecursive(dossier);
			int32 erreurs = 0;
			// 01 : les cartes de l'etoile, sous « Tout » puis « Physique ».
			{
				SceneCartes(m);
				NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				T.W = 1600.f;
				T.H = 900.f;
				T.pctx->Init(1600, 900);
				T.Ui().hauteurTiroir = 150.f;
				for (int32 k = 0; k < 4; ++k) {
					T.Trame();
				}
				erreurs += EcrirePng(T, NkString::Format("%s/01a_details_cartes_tout.png", dossier).CStr()) ? 0 : 1;
				Pastille(T, 3);
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/01b_details_physique.png", dossier).CStr()) ? 0 : 1;
				Pastille(T, 4);
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/01c_details_rendu_forme.png", dossier).CStr()) ? 0 : 1;
				// 02 : « Ajouter un composant », ouvert sur un acteur vide.
				Pastille(T, 0);
				m.selection = NkEditeurCreerEntite(m, "Vide", NkVec2f(3.f, 2.f));
				m.aSelection = true;
				T.Trame();
				T.Trame();
				const nkgui::NkVec2 p = Milieu(T.Ui().detailsAjouter);
				T.Clic(0, p.x, p.y);
				T.Trame();
				erreurs += EcrirePng(T, NkString::Format("%s/02a_ajouter_un_composant.png", dossier).CStr()) ? 0 : 1;
				T.Taper("anim");
				T.Trame();
				erreurs += EcrirePng(T, NkString::Format("%s/02b_ajouter_anim.png", dossier).CStr()) ? 0 : 1;
				const int32 k = LigneMenu(T.Ui(), "Animateur : plateforme");
				if (k >= 0) {
					const nkgui::NkVec2 q = Milieu(T.Ui().menuLignes[static_cast<uint32>(k)].r);
					T.Clic(0, q.x, q.y);
				}
				T.Fermer();
				Pastille(T, 5);
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/02c_animation_et_animateur.png", dossier).CStr()) ? 0 : 1;
				memory::NkGetDefaultAllocator().Delete(pt);
			}
			// 03 : chaque asset s'ouvre -- sur une COPIE du projet de demonstration
			// (References/Captures/etape1/projet_demo), ou l'on fabrique de VRAIS
			// prefabs et un vrai controleur (ceux de la demo sont factices).
			{
				const NkString demo = NkString::Format("%s/projet_demo", dossier);
				static const char *kSources[] = {"../../../../../References/Captures/etape1/projet_demo",
												 "C:/Users/rihen/Documents/Projects/References/Captures/etape1/projet_demo"};
				bool copie = NkDirectory::Exists(NkString::Format("%s/MonJeu2D/Contenu", demo.CStr()).CStr());
				for (const char *s : kSources) {
					if (!copie && NkDirectory::Exists(s)) {
						copie = NkDirectory::Copy(s, demo.CStr(), true, true);
					}
				}
				if (!copie) {
					std::printf("  captures 03 : projet de demonstration introuvable (saute)\n");
				} else {
					NkEditeurNouvelleScene(m);
					m.chemin = NkString::Format("%s/MonJeu2D/Contenu/Scenes/Capture.nkscene", demo.CStr());
					m.projet = NkString();
					// De VRAIS prefabs : une caisse texturee (block.png) et un joueur.
					const uint32 tex = m.textures.Charger(NkEditeurCheminContenuAbsolu(m, "Contenu/Textures/block.png").CStr());
					const ecs::NkEntityId caisse = m.scene.Creer("Caisse_Vraie", NkVec2f(-3.f, 1.f));
					NkSprite2D sp;
					sp.texId = tex;
					sp.taille = NkVec2f(1.2f, 1.2f);
					m.scene.Monde().Add<NkSprite2D>(caisse, sp);
					m.selection = caisse;
					m.aSelection = true;
					const NkString pCaisse = NkEditeurNouveauPrefabContenu(m, "Contenu/Prefabs");
					const ecs::NkEntityId joueur = NkEditeurPoserForme(m, NkGenreForme2D::NK_CAPSULE, NkVec2f(3.f, 1.f));
					if (NkEtiquette *et = m.scene.Monde().Get<NkEtiquette>(joueur)) {
						std::snprintf(et->nom, sizeof(et->nom), "%s", "Joueur_Vrai");
					}
					m.selection = joueur;
					const NkString pJoueur = NkEditeurNouveauPrefabContenu(m, "Contenu/Prefabs");
					const NkString ctl = NkEditeurNouveauControleurContenu(m, "Contenu/Animations");
					NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
					NkEditeurBancTrame &T = *pt;
					T.W = 1600.f;
					T.H = 900.f;
					T.pctx->Init(1600, 900);
					NkEditeurInterface &ui = T.Ui();
					ui.hauteurTiroir = 230.f;
					ui.sonsMuets = true;
					for (int32 k = 0; k < 3; ++k) {
						T.Trame();
					}
					NkEditeurCadre cadre = T.Cadre();
					// La texture, en « Pixel » (Checkerboard : 8 x 8 cases nettes).
					NkEditeurOuvrirAsset(cadre, "Contenu/Textures/NogeLogo.png");
					T.Trame();
					T.Trame();
					erreurs += EcrirePng(T, NkString::Format("%s/03a_texture_onglet.png", dossier).CStr()) ? 0 : 1;
					NkEditeurOuvrirAsset(cadre, "Contenu/Textures/Checkerboard.png");
					if (ui.ongletActif >= 0) {
						ui.onglets[static_cast<uint32>(ui.ongletActif)]->texture.pixel = true;
						ui.onglets[static_cast<uint32>(ui.ongletActif)]->modifie = true;
					}
					T.Trame();
					T.Trame();
					erreurs += EcrirePng(T, NkString::Format("%s/03b_texture_pixel.png", dossier).CStr()) ? 0 : 1;
					NkEditeurOuvrirAsset(cadre, "Contenu/Polices/Antonio-Bold.ttf");
					T.Trame();
					T.Trame();
					erreurs += EcrirePng(T, NkString::Format("%s/03c_police_tailles.png", dossier).CStr()) ? 0 : 1;
					NkEditeurOuvrirAsset(cadre, "Contenu/Sons/powerup.wav");
					T.Trame();
					const nkgui::NkVec2 l = Milieu(ui.assetLire);
					T.Clic(0, l.x, l.y);
					T.pctx->input.mousePos = nkgui::NkVec2{-100.f, -100.f};
					T.Trame();
					erreurs += EcrirePng(T, NkString::Format("%s/03d_son_lecture.png", dossier).CStr()) ? 0 : 1;
					if (!ctl.Empty()) {
						NkEditeurOuvrirAsset(cadre, ctl.CStr());
						T.Trame();
						T.Trame();
						erreurs += EcrirePng(T, NkString::Format("%s/03e_controleur.png", dossier).CStr()) ? 0 : 1;
					}
					if (!pCaisse.Empty()) {
						NkEditeurOuvrirAsset(cadre, pCaisse.CStr());
						T.Trame();
						T.Trame();
						erreurs += EcrirePng(T, NkString::Format("%s/03f_prefab_mode.png", dossier).CStr()) ? 0 : 1;
						NkEditeurActiverOnglet(cadre, -1);
						T.Trame();
					}
					(void)pJoueur;
					NkEditeurFermerTousOnglets(cadre);
					// 04 : le VRAI prefab glisse du dossier Prefabs dans l'Outliner, puis
					// dans la vue.
					{
						const int32 kd = T.Carte("Contenu/Prefabs");
						if (kd >= 0) {
							const nkgui::NkRect r = ui.contenuCartes[static_cast<uint32>(kd)];
							T.DoubleClic(r.x + r.w * 0.5f, r.y + r.h * 0.3f);
							T.Trame();
							T.Trame();
						}
						const int32 kp = pCaisse.Empty() ? -1 : T.Carte(pCaisse.CStr());
						if (kp >= 0) {
							const nkgui::NkRect r = ui.contenuCartes[static_cast<uint32>(kp)];
							const float32 x0 = r.x + r.w * 0.5f, y0 = r.y + r.h * 0.3f;
							const float32 x1 = ui.outliner.x + ui.outliner.w * 0.5f, y1 = ui.outliner.y + ui.outliner.h * 0.55f;
							T.pctx->input.mousePos = nkgui::NkVec2{x0, y0};
							T.souris.Appui(T.pctx->input, 0);
							T.Trame();
							for (int32 s = 1; s <= 8; ++s) {
								const float32 f = static_cast<float32>(s) / 8.f;
								T.pctx->input.mousePos = nkgui::NkVec2{x0 + (x1 - x0) * f, y0 + (y1 - y0) * f};
								T.Trame();
							}
							erreurs += EcrirePng(T, NkString::Format("%s/04a_prefab_glisse_outliner.png", dossier).CStr()) ? 0 : 1;
							T.souris.Relache(T.pctx->input, 0);
							T.Trame();
							const float32 x2 = ui.viseur.x + ui.viseur.w * 0.72f, y2 = ui.viseur.y + ui.viseur.h * 0.35f;
							T.Glisser(x0, y0, x2, y2, 8);
							T.Fermer();
							erreurs += EcrirePng(T, NkString::Format("%s/04b_prefab_instances.png", dossier).CStr()) ? 0 : 1;
						}
					}
					memory::NkGetDefaultAllocator().Delete(pt);
				}
			}
			memory::NkGetDefaultAllocator().Delete(pm);
			return erreurs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
