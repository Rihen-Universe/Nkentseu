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
//   (m5)  « PLACER DES ACTEURS » SE REPLIE : le chevron de son en-tete ne lui
//         laisse que sa colonne d'onglets (NK_PLACER_REPLIE_L), la vue gagne la
//         difference, la liste n'est plus peinte, sa largeur est gardee ; un
//         onglet clique (Formes) le deplie SUR cet onglet ; le chevron replie
//         puis deplie de nouveau
//   (m6)  L'ECLAIRAGE DE LA SCENE S'ALLUME ET S'ETEINT : l'interrupteur de la
//         barre de la vue l'allume puis l'eteint ; celui de l'onglet Monde
//         (« Allumé » / « Éteint ») aussi ; Ctrl+Z rend l'etat d'avant le
//         dernier geste
//   (m7)  LES EFFETS NE TOURNENT QU'EN JEU (R34) : un feu pose ne fait rien en
//         edition ; « Aperçu en édition » coche le fait bruler, decoche l'efface ;
//         Jouer : le feu (jouer au demarrage) part seul, l'etincelle (sans) attend
//         NkEffets2D::Jouer ; Pause fige ses particules et ses naissances, la
//         sortie de pause les relance ; Arreter : plus de naissance, les
//         particules finissent leur vie ; Arreter(vider) les efface ; les boutons
//         Jouer / Pause / Arreter de la carte pilotent en jeu ; les deux reglages
//         survivent a Enregistrer / Ouvrir
//   (m8)  JOUER COMME LE PIE D'UNREAL : en jeu, la molette ne zoome plus, le
//         bouton du milieu ne deplace plus, un clic ne choisit rien ; F8 EJECTE
//         (camera libre : la molette zoome, le jeu avance a part) ; F8 de
//         nouveau : la vue revient a la camera du jeu ; le bouton Ejecter de la
//         barre fait de meme ; Arreter rend la camera d'avant Jouer
//   (m9)  L'APPAREIL ENTIER SE ZOOME : un telephone affiche, la molette grossit
//         son ecran du MEME facteur que la camera, et le monde sous son centre
//         ne change pas ; le panoramique le deplace du meme nombre de pixels ;
//         en Jouer, il se recadre entier dans la vue (le jeu a sa camera : le
//         meme monde sous son centre) ; Arreter rend la vue d'avant ;
//         « Recadrer l'appareil » lui rend sa taille ajustee
//   (m10) UN DOSSIER PLEIN SE DISTINGUE D'UN VIDE (Content Browser, variante
//         Unreal) : le kit peint une FEUILLE claire (la teinte du dossier,
//         eclaircie) et ses apercus (la vignette de l'image) pour le plein, rien
//         de tel pour le vide, et la feuille reste en petite vignette ; l'editeur
//         renseigne plein / vide et les apercus de chaque dossier du Contenu
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
#include "Editeur/NkEditeurLumiere.h"
#include "Editeur/NkEditeurPlacer.h"
#include "Editeur/NkEditeurViseur.h"

#include "NKEditorKit/Components/NkRecordingPaint.h"
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
					return k >= 0 && NkEditeurOngletAssetActif(ui) >= 0;
				};
				auto Actif = [&]() -> NkOngletAsset * {
					const int32 k = NkEditeurOngletAssetActif(ui);
					return k >= 0 ? ui.onglets[static_cast<uint32>(k)] : nullptr;
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
				// Le CONTROLEUR : (fusion du 02/10) il s'ouvre dans la page ANIMATEUR,
				// son graphe editable (NkEditeurPagesAnim.h) -- le point d'accroche
				// prevu par R33 est branche. Puis on revient a la scene.
				bool controleur = false;
				if (!ctl.Empty()) {
					(void)Ouvrir(ctl.CStr());
					const NkDocAnim *d = NkEditeurDocAnimActif(ui);
					controleur = d != nullptr && d->genre == NkGenreDocAnim::NK_ANIMATEUR && d->graphe.nodes.Size() >= 2u;
					NkEditeurActiverDocument(m, ui, NkDocScene());
					t.Trame();
				}
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
					suit = ui.modePrefab == nullptr && NkEditeurSceneDevant(ui) && NbEntites(m.scene) == avant && s2 != nullptr && s2->couleur == 0x20C040FFu;
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

			// (m5) LE PANNEAU « PLACER DES ACTEURS » SE REPLIE (retour 5 de Rihen).
			{
				NkEditeurNouvelleScene(m);
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				t.Trame();
				t.Trame();
				const float32 large = ui.placer.w, vue0 = ui.vue.w, garde = ui.largeurPlacer;
				auto Liste = [&]() {
					uint32 n = 0u;
					for (uint32 k = 0; k < ui.placerRects.Size(); ++k) {
						n += ui.placerRects[k].w > 0.f ? 1u : 0u;
					}
					return n;
				};
				const uint32 lignes0 = Liste();
				nkgui::NkVec2 p = Milieu(ui.placerChevron);
				t.Clic(0, p.x, p.y);
				t.Trame();
				const bool replie = ui.placerReplie && math::NkAbs(ui.placer.w - NK_PLACER_REPLIE_L) < 0.5f &&
									ui.vue.w > vue0 + (large - NK_PLACER_REPLIE_L) - 1.f && ui.largeurPlacer == garde && ui.placerOngletsRects[4].w > 0.f;
				// Un onglet clique, replie : il deplie, sur lui (Formes = 4).
				p = Milieu(ui.placerOngletsRects[static_cast<int32>(NkOngletPlacer::NK_FORMES)]);
				t.Clic(0, p.x, p.y);
				t.Trame();
				const bool surOnglet = !ui.placerReplie && ui.placerOnglet == static_cast<int32>(NkOngletPlacer::NK_FORMES) &&
									   math::NkAbs(ui.placer.w - large) < 0.5f && Liste() > 0u;
				// Le chevron : replie, puis deplie.
				p = Milieu(ui.placerChevron);
				t.Clic(0, p.x, p.y);
				t.Trame();
				const bool deux = ui.placerReplie;
				p = Milieu(ui.placerChevron);
				t.Clic(0, p.x, p.y);
				t.Trame();
				const bool rouvre = !ui.placerReplie && math::NkAbs(ui.vue.w - vue0) < 0.5f;
				const bool ok = lignes0 > 0u && replie && surOnglet && deux && rouvre;
				if (!ok) {
					std::printf("        lignes0 %u replie %d (placer %.1f vue %.1f -> %.1f) surOnglet %d deux %d rouvre %d%c", lignes0, replie,
								static_cast<double>(ui.placer.w), static_cast<double>(vue0), static_cast<double>(ui.vue.w), surOnglet, deux, rouvre, 10);
				}
				Temoin(ok, "(m5) Placer des acteurs : replie en colonne d'onglets, deplie sur un onglet", static_cast<float32>(large - NK_PLACER_REPLIE_L));
			}

			// (m6) L'INTERRUPTEUR DE L'ECLAIRAGE (retour 6 de Rihen).
			{
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				m.scene.Eclairage().actif = false;
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				t.Trame();
				t.Trame();
				nkgui::NkVec2 p = Milieu(ui.boutonEclairageVue);
				t.Clic(0, p.x, p.y);
				const bool vueAllume = m.scene.Eclairage().actif;
				p = Milieu(ui.boutonEclairageVue);
				t.Clic(0, p.x, p.y);
				const bool vueEteint = !m.scene.Eclairage().actif;
				ui.ongletDroite = 1; // l'onglet Monde
				t.Trame();
				p = Milieu(ui.boutonEclairageMonde);
				t.Clic(0, p.x, p.y);
				const bool mondeAllume = m.scene.Eclairage().actif;
				t.Trame();
				p = Milieu(ui.boutonEclairageMonde);
				t.Clic(0, p.x, p.y);
				const bool mondeEteint = !m.scene.Eclairage().actif;
				NkEditeurAnnuler(m);
				const bool annule = m.scene.Eclairage().actif;
				ui.ongletDroite = 0;
				const bool ok = vueAllume && vueEteint && mondeAllume && mondeEteint && annule;
				if (!ok) {
					std::printf("        vue %d/%d monde %d/%d annule %d%c", vueAllume, vueEteint, mondeAllume, mondeEteint, annule, 10);
				}
				Temoin(ok, "(m6) eclairage de la scene : interrupteurs de la vue et du Monde, Ctrl+Z",
					   static_cast<float32>(vueAllume + vueEteint + mondeAllume + mondeEteint + annule));
			}

			// (m7) LES EFFETS EN JEU, PILOTABLES (R34).
			{
				NkDirectory::Delete("banc_m7", true);
				NkDirectory::CreateRecursive("banc_m7");
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				const ecs::NkEntityId feu = m.scene.Creer("Feu", NkVec2f(-2.f, 0.f));
				const ecs::NkEntityId eti = m.scene.Creer("Etincelles", NkVec2f(2.f, 0.f));
				NkEditeurAjouterEffet(m, feu, NkPresetEffet2D::NK_FEU);
				NkEditeurAjouterEffet(m, eti, NkPresetEffet2D::NK_FEU);
				m.scene.Monde().Get<NkEmetteur2D>(eti)->jouerAuDemarrage = false;
				NkEffets2D &fx = m.scene.Effets();
				auto De = [&](ecs::NkEntityId id) {
					uint32 n = 0u;
					for (uint32 i = 0; i < fx.Particules().Size(); ++i) {
						n += fx.Particules()[i].emetteur == id.Pack() ? 1u : 0u;
					}
					return n;
				};
				auto Avancer = [&](int32 n) {
					for (int32 k = 0; k < n; ++k) {
						NkEditeurAvancer(m, 1.f / 60.f);
					}
				};
				// En EDITION : rien, sauf l'apercu choisi.
				Avancer(30);
				const bool rienEnEdition = fx.NbParticules() == 0u;
				m.scene.Monde().Get<NkEmetteur2D>(feu)->apercuEdition = true;
				Avancer(30);
				const bool apercu = De(feu) > 0u && De(eti) == 0u;
				m.scene.Monde().Get<NkEmetteur2D>(feu)->apercuEdition = false;
				Avancer(1);
				const bool efface = fx.NbParticules() == 0u;
				// En JEU : le feu part seul, l'etincelle attend.
				NkEditeurJouer(m);
				Avancer(30);
				const bool depart = De(feu) > 0u && De(eti) == 0u && fx.Lecture(m.scene, eti) == NkLectureEffet2D::NK_ARRETE;
				fx.Jouer(m.scene, eti);
				Avancer(20);
				const bool joue = De(eti) > 0u;
				// PAUSE : figees, sans naissance.
				fx.Pause(m.scene, eti, true);
				Avancer(1);
				const uint32 n0 = De(eti);
				NkVec2f p0(0.f, 0.f);
				for (uint32 i = 0; i < fx.Particules().Size(); ++i) {
					if (fx.Particules()[i].emetteur == eti.Pack()) {
						p0 = fx.Particules()[i].pos;
						break;
					}
				}
				Avancer(15);
				NkVec2f p1(1.e9f, 1.e9f);
				for (uint32 i = 0; i < fx.Particules().Size(); ++i) {
					if (fx.Particules()[i].emetteur == eti.Pack()) {
						p1 = fx.Particules()[i].pos;
						break;
					}
				}
				const bool fige = n0 > 0u && De(eti) == n0 && p0.x == p1.x && p0.y == p1.y;
				fx.Pause(m.scene, eti, false);
				Avancer(5);
				const bool reprend = fx.Lecture(m.scene, eti) == NkLectureEffet2D::NK_JOUE && De(eti) != n0;
				// ARRETER : plus de naissance ; tout s'eteint en une vie (0,9 s au plus).
				fx.Arreter(m.scene, feu);
				Avancer(70);
				const bool arrete = De(feu) == 0u && fx.Lecture(m.scene, feu) == NkLectureEffet2D::NK_ARRETE;
				fx.Arreter(m.scene, eti, true);
				const bool vide = De(eti) == 0u;
				// La CARTE pilote : « Jouer » sur le feu choisi.
				m.selection = feu;
				m.aSelection = true;
				bool carte = false;
				{
					NkEditeurBancTrame t(m);
					NkEditeurInterface &ui = t.Ui();
					ui.hauteurTiroir = 100.f;
					t.Trame();
					t.Trame();
					if (ui.effetJouer.w > 0.f) {
						const nkgui::NkVec2 q = Milieu(ui.effetJouer);
						t.Clic(0, q.x, q.y);
						const bool j = fx.Lecture(m.scene, feu) == NkLectureEffet2D::NK_JOUE;
						const nkgui::NkVec2 r = Milieu(ui.effetPause);
						t.Clic(0, r.x, r.y);
						carte = j && fx.Lecture(m.scene, feu) == NkLectureEffet2D::NK_PAUSE;
					}
				}
				NkEditeurArreter(m);
				// Les deux reglages survivent au fichier. (Arreter a refait les
				// entites : on retrouve le feu par son nom.)
				const ecs::NkEntityId feu2 = Par(m.scene, "Feu");
				if (NkEmetteur2D *ef = feu2.IsValid() ? m.scene.Monde().Get<NkEmetteur2D>(feu2) : nullptr) {
					ef->apercuEdition = true;
				}
				m.chemin = NkString("banc_m7/effets.nkscene");
				bool fichier = NkEditeurSauver(m) && NkEditeurOuvrir(m);
				if (fichier) {
					bool apercuLu = false, departLu = true;
					m.scene.Monde().Query<NkEtiquette, NkEmetteur2D>().ForEach([&](ecs::NkEntityId, NkEtiquette &et, NkEmetteur2D &e) {
						if (std::strcmp(et.nom, "Feu") == 0) {
							apercuLu = e.apercuEdition;
						}
						if (std::strcmp(et.nom, "Etincelles") == 0) {
							departLu = e.jouerAuDemarrage;
						}
					});
					fichier = apercuLu && !departLu;
				}
				const bool ok = rienEnEdition && apercu && efface && depart && joue && fige && reprend && arrete && vide && carte && fichier;
				if (!ok) {
					std::printf("        edition %d apercu %d efface %d depart %d joue %d fige %d reprend %d arrete %d vide %d carte %d fichier %d%c",
								rienEnEdition, apercu, efface, depart, joue, fige, reprend, arrete, vide, carte, fichier, 10);
				}
				Temoin(ok, "(m7) effets : en jeu seulement, apercu au choix, depart, Jouer / Pause / Arreter",
					   static_cast<float32>(rienEnEdition + apercu + efface + depart + joue + fige + reprend + arrete + vide + carte + fichier));
				NkDirectory::Delete("banc_m7", true);
			}

			// (m8) JOUER : LA VUE EST AU JEU ; EJECTER (Rihen, 01/10).
			{
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				t.Trame();
				t.Trame();
				ui.cadrageAnime = false;
				NkVue2D &cam = m.scene.Camera();
				const float32 zEdition = cam.Zoom();
				const NkVec2f cEdition = cam.Centre();
				const nkgui::NkVec2 milieu{ui.viseur.x + ui.viseur.w * 0.5f, ui.viseur.y + ui.viseur.h * 0.5f};
				auto Molette = [&](float32 w) {
					t.Ctx().input.mousePos = milieu;
					t.Ctx().input.wheel = w;
					t.Trame();
					t.Ctx().input.wheel = 0.f;
					t.Trame();
				};
				NkEditeurCadre cadre = t.Cadre();
				NkEditeurExecuter(cadre, NK_A_JOUER);
				t.Trame();
				const float32 zJeu = cam.Zoom();
				Molette(3.f);
				const bool molette = cam.Zoom() == zJeu;
				// Le bouton du milieu : pas de panoramique.
				const NkVec2f c0 = cam.Centre();
				t.Ctx().input.mousePos = milieu;
				t.souris.Appui(t.Ctx().input, 2);
				t.Trame();
				t.Ctx().input.mousePos = nkgui::NkVec2{milieu.x + 80.f, milieu.y + 40.f};
				t.Trame();
				t.souris.Relache(t.Ctx().input, 2);
				t.Trame();
				const bool pano = cam.Centre().x == c0.x && cam.Centre().y == c0.y;
				// Un clic sur une caisse : rien n'est choisi.
				m.aSelection = false;
				ecs::NkEntityId caisse;
				m.scene.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (std::strncmp(e.nom, "Caisse", 6) == 0 && !caisse.IsValid()) {
						caisse = id;
					}
				});
				bool choix = true;
				if (const NkTransform2D *tc = caisse.IsValid() ? m.scene.Monde().Get<NkTransform2D>(caisse) : nullptr) {
					const NkVec2f e = cam.MondeVersEcran(tc->position);
					t.Clic(0, e.x, e.y);
					choix = m.aSelection;
				}
				// F8 : ejecte, la molette zoome ; le jeu avance avec SA camera.
				t.Touche(nkgui::NkGuiKey::F8);
				const bool ejecte = m.ejecte;
				Molette(3.f);
				const float32 zLibre = cam.Zoom();
				for (int32 k = 0; k < 5; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
				}
				const bool libre = ejecte && zLibre != zJeu && cam.Zoom() == zLibre && m.cameraJeu.Zoom() == zJeu;
				// F8 : retour a la camera du jeu.
				t.Touche(nkgui::NkGuiKey::F8);
				const bool retour = !m.ejecte && cam.Zoom() == zJeu;
				// Le bouton de la barre : ejecte puis revient.
				nkgui::NkVec2 p = Milieu(ui.boutonEjecter);
				t.Clic(0, p.x, p.y);
				const bool bouton1 = m.ejecte;
				p = Milieu(ui.boutonEjecter);
				t.Clic(0, p.x, p.y);
				const bool bouton = bouton1 && !m.ejecte;
				// Arreter : la camera d'avant Jouer.
				Molette(3.f); // (au jeu : sans effet)
				NkEditeurExecuter(cadre, NK_A_ARRETER);
				t.Trame();
				const bool arret = cam.Zoom() == zEdition && cam.Centre().x == cEdition.x && cam.Centre().y == cEdition.y && !m.ejecte;
				// En EDITION, la molette zoome de nouveau.
				Molette(3.f);
				const bool edition = cam.Zoom() != zEdition;
				const bool ok = molette && pano && !choix && libre && retour && bouton && arret && edition;
				if (!ok) {
					std::printf("        molette %d pano %d choix %d libre %d retour %d bouton %d arret %d edition %d%c", molette, pano, choix, libre,
								retour, bouton, arret, edition, 10);
				}
				Temoin(ok, "(m8) Jouer : vue au jeu (molette, pano, clic coupes), F8 ejecte / revient, Arreter",
					   static_cast<float32>(molette + pano + !choix + libre + retour + bouton + arret + edition));
			}

			// (m9) L'APPAREIL SIMULE ZOOME EN ENTIER (Rihen, 01/10).
			{
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				const int32 profilAvant = m.profil;
				for (int32 k = 0; k < NkNbProfils(); ++k) {
					if (NkProfil(k).famille == NkFamilleAppareil::NK_TELEPHONE) {
						m.profil = k;
						break;
					}
				}
				const bool cadreAvant = m.appareil.voirCadre;
				m.appareil.voirCadre = true;
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				t.Trame();
				t.Trame();
				ui.cadrageAnime = false;
				NkVue2D &cam = m.scene.Camera();
				auto CentreR = [](const nkgui::NkRect &r) { return NkVec2f(r.x + r.w * 0.5f, r.y + r.h * 0.5f); };
				const nkgui::NkRect r0 = ui.appareilEcran;
				const float32 z0 = cam.Zoom();
				const NkVec2f w0 = cam.EcranVersMonde(CentreR(r0));
				// La molette, au centre de la vue.
				t.Ctx().input.mousePos = nkgui::NkVec2{ui.viseur.x + ui.viseur.w * 0.5f, ui.viseur.y + ui.viseur.h * 0.5f};
				t.Ctx().input.wheel = 2.f;
				t.Trame();
				t.Ctx().input.wheel = 0.f;
				t.Trame();
				const nkgui::NkRect r1 = ui.appareilEcran;
				const float32 k = cam.Zoom() / z0;
				const NkVec2f w1 = cam.EcranVersMonde(CentreR(r1));
				const bool zoom = k > 1.1f && math::NkAbs(r1.w / r0.w - k) < 0.01f * k && math::NkAbs(r1.h / r0.h - k) < 0.01f * k &&
								  math::NkAbs(w1.x - w0.x) < 0.01f && math::NkAbs(w1.y - w0.y) < 0.01f;
				// Le panoramique : le meme deplacement.
				const nkgui::NkVec2 a{ui.viseur.x + 200.f, ui.viseur.y + 200.f};
				t.Ctx().input.mousePos = a;
				t.souris.Appui(t.Ctx().input, 2);
				t.Trame();
				t.Ctx().input.mousePos = nkgui::NkVec2{a.x + 60.f, a.y + 30.f};
				t.Trame();
				t.souris.Relache(t.Ctx().input, 2);
				t.Trame();
				const nkgui::NkRect r2 = ui.appareilEcran;
				const bool pano = math::NkAbs(r2.x - r1.x - 60.f) < 1.f && math::NkAbs(r2.y - r1.y - 30.f) < 1.f && math::NkAbs(r2.w - r1.w) < 0.5f;
				// Jouer : recadre entier dans la vue, le meme monde au centre.
				const nkgui::NkRect ajuste = NkAireAppareil(ui.viseur, m.ProfilCourant(), true);
				NkEditeurCadre cadre = t.Cadre();
				NkEditeurExecuter(cadre, NK_A_JOUER);
				t.Trame();
				t.Trame();
				const nkgui::NkRect r3 = ui.appareilEcran;
				const NkVec2f w3 = cam.EcranVersMonde(CentreR(r3));
				const bool jeu = math::NkAbs(r3.w - ajuste.w) < 1.f && math::NkAbs(r3.x - ajuste.x) < 1.f && math::NkAbs(r3.y - ajuste.y) < 1.f &&
								 math::NkAbs(w3.x - w0.x) < 0.01f && math::NkAbs(w3.y - w0.y) < 0.01f;
				NkEditeurExecuter(cadre, NK_A_ARRETER);
				t.Trame();
				const nkgui::NkRect r4 = ui.appareilEcran;
				const bool arret = math::NkAbs(r4.x - r2.x) < 1.f && math::NkAbs(r4.w - r2.w) < 1.f;
				// « Recadrer l'appareil » : sa taille ajustee, a la vue de maintenant.
				NkEditeurExecuter(cadre, NK_A_RECADRER_APPAREIL);
				t.Trame();
				const bool recadre = math::NkAbs(ui.appareilEcran.w - ajuste.w) < 1.f && math::NkAbs(ui.appareilEcran.x - ajuste.x) < 1.f;
				m.profil = profilAvant;
				m.appareil.voirCadre = cadreAvant;
				const bool ok = zoom && pano && jeu && arret && recadre;
				if (!ok) {
					std::printf("        zoom %d (k %.3f, %.1f -> %.1f) pano %d jeu %d (%.1f vs %.1f) arret %d recadre %d%c", zoom, static_cast<double>(k),
								static_cast<double>(r0.w), static_cast<double>(r1.w), pano, jeu, static_cast<double>(r3.w),
								static_cast<double>(ajuste.w), arret, recadre, 10);
				}
				Temoin(ok, "(m9) appareil : zoom et panoramique de l'appareil entier, recadre en Jouer",
					   static_cast<float32>(zoom + pano + jeu + arret + recadre));
			}

			// (m10) LES DOSSIERS PLEINS ET VIDES (Rihen, 01/10).
			{
				using namespace editorkit;
				// A. Le KIT : deux dossiers de meme couleur, l'un plein (3 apercus, une
				//    vignette 77), l'autre vide ; en grande puis en petite vignette.
				NkComponentInstance inst(NkContentBrowserDecl());
				inst.SetVariantByName("unreal");
				NkContentBrowserStyle st;
				st.values = &inst;
				st.panelBg = 1;
				st.headerBg = 2;
				st.border = 3;
				st.text = 4;
				st.textMuted = 6;
				st.cardBg = 7;
				st.cardFooterBg = 8;
				st.activeMark = 9;
				st.chosenMark = 10;
				st.folderTint = 11;
				const uint32 bleu = 0x3C9AE0FFu;
				const uint32 papier = NkTeinter(bleu, 0.86f);
				auto Peindre = [&](float32 taille, int32 &feuilles, int32 &vignettes) {
					NkContentBrowserModel mod;
					mod.thumbSize = taille;
					NkAssetEntry plein;
					plein.name = NkString("Plein");
					plein.path = NkString("Contenu/Plein");
					plein.isFolder = true;
					plein.couleur = bleu;
					plein.contenu = static_cast<uint8>(NkContenuDossier::Plein);
					plein.nbApercus = 3;
					plein.apercusIcone[0] = static_cast<uint8>(NkAssetIcone::Image);
					plein.apercusVignette[0] = 77u;
					plein.apercusIcone[1] = static_cast<uint8>(NkAssetIcone::Archive);
					plein.apercusIcone[2] = static_cast<uint8>(NkAssetIcone::Dossier);
					NkAssetEntry vide = plein;
					vide.name = NkString("Vide");
					vide.path = NkString("Contenu/Vide");
					vide.contenu = static_cast<uint8>(NkContenuDossier::Vide);
					vide.nbApercus = 0;
					vide.apercusVignette[0] = 0u;
					mod.entries.PushBack(plein);
					mod.entries.PushBack(vide);
					NkContentBrowserHooks h;
					NkComponentInput in;
					NkRecordingPaint rp;
					NkDrawContentBrowser(rp, in, NkPaintRect{0.f, 0.f, 1000.f, 500.f}, mod, st, h);
					feuilles = vignettes = 0;
					for (uint32 i = 0; i < rp.cmds.Size(); ++i) {
						feuilles += rp.cmds[i].op == NkPaintOp::FillColor && rp.cmds[i].rgba == papier ? 1 : 0;
						vignettes += rp.cmds[i].op == NkPaintOp::Image && rp.cmds[i].image == 77u ? 1 : 0;
					}
				};
				int32 fG = 0, vG = 0, fP = 0, vP = 0;
				Peindre(110.f, fG, vG);
				Peindre(36.f, fP, vP);
				const bool kit = fG == 1 && vG == 1 && fP == 1 && vP == 0;
				// B. L'EDITEUR : plein / vide et les apercus de son Contenu.
				NkDirectory::Delete("banc_m10", true);
				NkDirectory::CreateRecursive("banc_m10/projet/Contenu/Plein");
				NkDirectory::CreateRecursive("banc_m10/projet/Contenu/Vide");
				EcrireImage("banc_m10/projet/Contenu/Plein/damier.png");
				EcrireSon("banc_m10/projet/Contenu/Plein/la.wav");
				NkEditeurNouvelleScene(m);
				m.chemin = NkString("banc_m10/projet/scene.nkscene");
				m.projet = NkString();
				bool hote = false;
				{
					NkEditeurBancTrame t(m);
					NkEditeurInterface &ui = t.Ui();
					for (int32 k = 0; k < 4; ++k) {
						t.Trame();
					}
					const NkAssetEntry *ep = nullptr;
					const NkAssetEntry *ev = nullptr;
					for (uint32 k = 0; k < ui.contenu.entries.Size(); ++k) {
						const NkAssetEntry &e = ui.contenu.entries[k];
						ep = e.path == NkString("Contenu/Plein") ? &e : ep;
						ev = e.path == NkString("Contenu/Vide") ? &e : ev;
					}
					hote = ep != nullptr && ev != nullptr && ep->contenu == static_cast<uint8>(NkContenuDossier::Plein) && ep->nbApercus == 2u &&
						   ep->apercusVignette[0] != 0u && ev->contenu == static_cast<uint8>(NkContenuDossier::Vide) && ev->nbApercus == 0u;
					if (!hote) {
						std::printf("        hote : plein %d (%u apercus, vignette %llu) vide %d%c", ep != nullptr ? ep->contenu : -1,
									ep != nullptr ? static_cast<unsigned>(ep->nbApercus) : 0u,
									ep != nullptr ? static_cast<unsigned long long>(ep->apercusVignette[0]) : 0ull, ev != nullptr ? ev->contenu : -1, 10);
					}
				}
				NkDirectory::Delete("banc_m10", true);
				const bool ok = kit && hote;
				if (!ok) {
					std::printf("        kit : feuilles %d/%d vignettes %d/%d%c", fG, fP, vG, vP, 10);
				}
				Temoin(ok, "(m10) dossiers : plein = feuille et apercus (toutes tailles), vide = dossier seul",
					   static_cast<float32>(fG + vG + fP));
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
			// 05 : « Placer des acteurs » replie (la vue prend la place), puis deplie.
			{
				NkEditeurNouvelleScene(m);
				NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				T.W = 1600.f;
				T.H = 900.f;
				T.pctx->Init(1600, 900);
				T.Ui().hauteurTiroir = 150.f;
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				T.Ui().placerReplie = true;
				T.Trame();
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/05a_placer_replie.png", dossier).CStr()) ? 0 : 1;
				const nkgui::NkVec2 p = Milieu(T.Ui().placerOngletsRects[static_cast<int32>(NkOngletPlacer::NK_LUMIERES)]);
				T.pctx->input.mousePos = p;
				T.Trame();
				erreurs += EcrirePng(T, NkString::Format("%s/05b_placer_replie_survol.png", dossier).CStr()) ? 0 : 1;
				T.Clic(0, p.x, p.y);
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/05c_placer_deplie_lumieres.png", dossier).CStr()) ? 0 : 1;
				memory::NkGetDefaultAllocator().Delete(pt);
			}
			// 06 : l'interrupteur de l'eclairage, dans la barre de la vue et dans le
			// Monde, sur la nuit au feu de camp (allumee, puis eteinte).
			{
				NkEditeurNouvelleScene(m);
				NkEditeurSceneNuit(m);
				m.aSelection = false;
				NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				T.W = 1600.f;
				T.H = 900.f;
				T.pctx->Init(1600, 900);
				T.Ui().hauteurTiroir = 150.f;
				T.Ui().ongletDroite = 1;
				for (int32 k = 0; k < 4; ++k) {
					T.Trame();
				}
				erreurs += EcrirePng(T, NkString::Format("%s/06a_eclairage_allume.png", dossier).CStr()) ? 0 : 1;
				const nkgui::NkVec2 p = Milieu(T.Ui().boutonEclairageVue);
				T.Clic(0, p.x, p.y);
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/06b_eclairage_eteint.png", dossier).CStr()) ? 0 : 1;
				memory::NkGetDefaultAllocator().Delete(pt);
			}
			// 07 : les effets ne tournent qu'en jeu (R34) : la nuit, apercu decoche,
			// le feu choisi ; puis Jouer : le feu brule, la carte pilote.
			{
				NkEditeurNouvelleScene(m);
				NkEditeurSceneNuit(m);
				m.scene.Monde().Query<NkEmetteur2D>().ForEach([](ecs::NkEntityId, NkEmetteur2D &e) { e.apercuEdition = false; });
				m.scene.Effets().Vider();
				m.selection = Par(m.scene, "Feu");
				m.aSelection = m.selection.IsValid();
				NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				T.W = 1600.f;
				T.H = 900.f;
				T.pctx->Init(1600, 900);
				T.Ui().hauteurTiroir = 120.f;
				T.Ui().detailsCategorie = 4; // Rendu : la carte Emetteur
				for (int32 k = 0; k < 30; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
					T.Trame();
				}
				erreurs += EcrirePng(T, NkString::Format("%s/07a_effets_edition_sans_apercu.png", dossier).CStr()) ? 0 : 1;
				NkEditeurJouer(m);
				for (int32 k = 0; k < 60; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
					T.Trame();
				}
				m.selection = Par(m.scene, "Feu");
				m.aSelection = m.selection.IsValid();
				T.Trame();
				T.Trame();
				erreurs += EcrirePng(T, NkString::Format("%s/07b_effets_en_jeu_pilotes.png", dossier).CStr()) ? 0 : 1;
				if (T.Ui().effetPause.w > 0.f) {
					const nkgui::NkVec2 p = Milieu(T.Ui().effetPause);
					T.Clic(0, p.x, p.y);
				}
				for (int32 k = 0; k < 20; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
					T.Trame();
				}
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/07c_effet_en_pause.png", dossier).CStr()) ? 0 : 1;
				NkEditeurArreter(m);
				memory::NkGetDefaultAllocator().Delete(pt);
			}
			// 08 : Jouer (la vue au jeu), puis Ejecter (F8) et zoomer.
			{
				NkEditeurNouvelleScene(m);
				m.aSelection = false;
				NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				T.W = 1600.f;
				T.H = 900.f;
				T.pctx->Init(1600, 900);
				T.Ui().hauteurTiroir = 150.f;
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				NkEditeurCadre cadre = T.Cadre();
				NkEditeurExecuter(cadre, NK_A_JOUER);
				for (int32 k = 0; k < 40; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
					T.Trame();
				}
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/08a_jouer_vue_au_jeu.png", dossier).CStr()) ? 0 : 1;
				T.Touche(nkgui::NkGuiKey::F8);
				const nkgui::NkVec2 mil{T.Ui().viseur.x + T.Ui().viseur.w * 0.5f, T.Ui().viseur.y + T.Ui().viseur.h * 0.5f};
				for (int32 k = 0; k < 4; ++k) {
					T.Ctx().input.mousePos = mil;
					T.Ctx().input.wheel = -2.f;
					T.Trame();
				}
				T.Ctx().input.wheel = 0.f;
				for (int32 k = 0; k < 20; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
					T.Trame();
				}
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/08b_ejecte_camera_libre.png", dossier).CStr()) ? 0 : 1;
				NkEditeurExecuter(cadre, NK_A_ARRETER);
				memory::NkGetDefaultAllocator().Delete(pt);
			}
			// 09 : un telephone affiche ; la molette grossit l'APPAREIL ENTIER ; en
			// Jouer, il se recadre et le jeu y tourne.
			{
				NkEditeurNouvelleScene(m);
				NkEditeurSceneNuit(m);
				m.aSelection = false;
				const int32 profilAvant = m.profil;
				for (int32 k = 0; k < NkNbProfils(); ++k) {
					if (NkProfil(k).famille == NkFamilleAppareil::NK_TELEPHONE) {
						m.profil = k;
						break;
					}
				}
				m.orientation = NkOrientation::NK_PAYSAGE_GAUCHE;
				const bool cadreAvant = m.appareil.voirCadre;
				m.appareil.voirCadre = true;
				NkEditeurBancTrame *pt = memory::NkGetDefaultAllocator().New<NkEditeurBancTrame>(m);
				NkEditeurBancTrame &T = *pt;
				T.W = 1600.f;
				T.H = 900.f;
				T.pctx->Init(1600, 900);
				T.Ui().hauteurTiroir = 150.f;
				for (int32 k = 0; k < 3; ++k) {
					T.Trame();
				}
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/09a_appareil_pose.png", dossier).CStr()) ? 0 : 1;
				const nkgui::NkRect e = T.Ui().appareilEcran;
				for (int32 k = 0; k < 4; ++k) {
					T.Ctx().input.mousePos = nkgui::NkVec2{e.x + e.w * 0.5f, e.y + e.h * 0.5f};
					T.Ctx().input.wheel = -1.f;
					T.Trame();
				}
				T.Ctx().input.wheel = 0.f;
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/09b_appareil_reduit_en_entier.png", dossier).CStr()) ? 0 : 1;
				NkEditeurCadre cadre = T.Cadre();
				NkEditeurExecuter(cadre, NK_A_JOUER);
				for (int32 k = 0; k < 40; ++k) {
					NkEditeurAvancer(m, 1.f / 60.f);
					T.Trame();
				}
				T.Fermer();
				erreurs += EcrirePng(T, NkString::Format("%s/09c_jouer_dans_l_appareil.png", dossier).CStr()) ? 0 : 1;
				NkEditeurExecuter(cadre, NK_A_ARRETER);
				m.profil = profilAvant;
				m.orientation = NkOrientation::NK_PORTRAIT;
				m.appareil.voirCadre = cadreAvant;
				memory::NkGetDefaultAllocator().Delete(pt);
			}
			// 03 : chaque asset s'ouvre -- sur une COPIE du projet de demonstration
			// (References/Captures/etape1/projet_demo), ou l'on fabrique de VRAIS
			// prefabs et un vrai controleur (ceux de la demo sont factices).
			{
				const NkString demo = NkString::Format("%s/projet_demo", dossier);
				static const char *kSources[] = {"../../../../../References/Captures/etape1/projet_demo",
												 "C:/Users/rihen/Documents/Projects/References/Captures/etape1/projet_demo"};
				// Une copie NEUVE a chaque fois : les prefabs fabriques ne s'y accumulent pas.
				NkDirectory::Delete(demo.CStr(), true);
				bool copie = false;
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
					if (NkEditeurOngletAssetActif(ui) >= 0) {
						ui.onglets[static_cast<uint32>(NkEditeurOngletAssetActif(ui))]->texture.pixel = true;
						ui.onglets[static_cast<uint32>(NkEditeurOngletAssetActif(ui))]->modifie = true;
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
					// 10 : les dossiers PLEINS (feuille et apercus) et le dossier VIDE, en
					// trois tailles de vignettes.
					{
						ui.hauteurTiroir = 330.f;
						static const float32 kTailles[3] = {56.f, 96.f, 150.f};
						static const char *kNoms[3] = {"10a_dossiers_petits.png", "10b_dossiers_moyens.png", "10c_dossiers_grands.png"};
						for (int32 k = 0; k < 3; ++k) {
							ui.contenu.thumbSize = kTailles[k];
							for (int32 j = 0; j < 6; ++j) {
								T.Trame(); // les vignettes des images se chargent deux par trame
							}
							T.Fermer();
							erreurs += EcrirePng(T, NkString::Format("%s/%s", dossier, kNoms[k]).CStr()) ? 0 : 1;
						}
						ui.hauteurTiroir = 230.f;
						ui.contenu.thumbSize = 96.f;
						T.Trame(); // les cartes reprennent leur place avant le geste suivant
						T.Trame();
					}
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
						if (kd < 0 || kp < 0) {
							std::printf("  captures 04 : dossier %d, prefab %d (%s)\n", kd, kp, pCaisse.CStr());
						}
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
