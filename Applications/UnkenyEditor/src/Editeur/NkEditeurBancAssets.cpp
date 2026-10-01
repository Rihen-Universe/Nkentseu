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
//
//   Les captures hors ecran : `--captures-assets=DOSSIER` (rasterisees, sans
//   fenetre ni GPU, comme --captures-formes).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
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

			m.chemin = cheminAvant;
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
			memory::NkGetDefaultAllocator().Delete(pm);
			return erreurs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
