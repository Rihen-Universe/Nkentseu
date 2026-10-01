//
// NkEditeurTiroir.cpp
// =============================================================================
// Description :
//   Le tiroir de contenu, en bas sur toute la largeur, en deux onglets :
//     Contenu  le NAVIGATEUR DE CONTENU d'Unreal 5 (la variante « unreal » du
//              composant partage NkDrawContentBrowser de NKEditorKit) : le
//              Contenu du projet, et le catalogue d'acteurs d'Unkeny
//     Journal  les annonces de l'editeur (enregistre, erreur...), datees
//
// Caracteristiques :
//   - (2026-10-01, document 02 §3 et §3.1) LES SEPT ZONES D'UNREAL, dessinees
//     par le KIT (une variante, pas une copie) : barre + Ajouter / Importer /
//     Tout enregistrer, precedent / suivant, fil d'Ariane « Tout > Contenu >
//     ... », verrou, Reglages ; sources (Favoris, le projet, Collections) ;
//     puces de type ; recherche et tri ; cartes ; « N elements (M
//     selectionnes) ». L'editeur ne fait qu'y brancher SES donnees et SA touche.
//   - LES GESTES DE RIHEN (01/10) : on selectionne les fichiers ET les
//     dossiers (simple, Ctrl, Maj, cadre) ; Ctrl+X / Ctrl+C / Ctrl+V, Ctrl+D,
//     F2 (le nom s'edite en place), Suppr (avec confirmation et references) ;
//     glisser d'un dossier vers un autre ouvre « Deplacer ici / Copier ici » ;
//     deposer fichiers ET dossiers de l'OS = importer ; clic droit complet sur
//     un asset, un dossier, le vide, une collection. Le travail sur le disque
//     est celui du kit (NkContentBrowserDisque.h) : confine au Contenu, jamais
//     d'ecrasement, la corbeille pour supprimer.
//   - LA TOUCHE D'UNKENY : les icones de ses natures (scene, prefab, controleur
//     d'animation, son, police) peintes par la greffe `vignetteApp`, la VRAIE
//     image pour les textures, une couleur par nature dans la bande de type, et
//     le glisser d'un prefab ou d'une image vers le viseur pour le POSER.
//   - Le catalogue d'acteurs reste la, dans le dossier virtuel « Acteurs »
//     (comme le dossier « C++ Classes » d'Unreal) : clic sur une carte = l'outil
//     Poser arme ; glisser vers la vue = pose au point de depot.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurInterface.h"
#include "Editeur/NkEditeurTerminal.h"
#include "Editeur/NkEditeurReferences.h"
#include "Livraison/NkEditeurFenetreConstruire.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/Components/NkSilhouettes.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKEditorKit/NkEditorTextField.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using editorkit::NkRole;
		using nkgui::NkColor;
		using nkgui::NkRect;

		namespace {

			constexpr int32 NB_CATEGORIES = static_cast<int32>(NkCategorieActeur::NK_COUNT);

			/// La couleur d'une categorie : un ROLE du theme, jamais un litteral.
			/// Les roles « Type* » sont justement ceux que le kit reserve aux
			/// natures d'actifs (UI_SPEC §3.4).
			NkRole RoleCategorie(int32 k) noexcept {
				switch (static_cast<NkCategorieActeur>(k)) {
					case NkCategorieActeur::NK_CORPS_MOUS: return NkRole::TypeMat;
					case NkCategorieActeur::NK_FLUIDES:    return NkRole::TypeTex;
					case NkCategorieActeur::NK_ATOMES:     return NkRole::TypeAnim;
					case NkCategorieActeur::NK_TISSUS:     return NkRole::TypeMesh;
					default:                               return NkRole::AccentSel;
				}
			}

			constexpr const char *CHEMIN_RACINE = "Acteurs";
			constexpr const char *CHEMIN_SIMPLE = "simple";
			/// La racine du fil d'Ariane (Unreal « All ») : ses dossiers sont
			/// « Contenu » et « Acteurs ».
			constexpr const char *CHEMIN_TOUT = "Tout";
			constexpr const char *CHEMIN_COLLECTIONS = "Collections";

			/// « acteur:7 » -> 7 ; « simple » -> -2 ; autre -> -1.
			int32 ActeurDuChemin(const char *chemin) noexcept {
				if (chemin == nullptr) {
					return -1;
				}
				if (std::strcmp(chemin, CHEMIN_SIMPLE) == 0) {
					return -2;
				}
				if (std::strncmp(chemin, "acteur:", 7) != 0) {
					return -1;
				}
				const int32 i = std::atoi(chemin + 7);
				return (i >= 0 && i < static_cast<int32>(NkActeurSim::NK_COUNT)) ? i : -1;
			}

			void Armer(NkEditeurModele &m, int32 acteur) {
				if (acteur == -2) {
					m.acteurSimple = true;
					m.outil = NkOutil::NK_POSER;
				} else if (acteur >= 0) {
					m.acteur = static_cast<NkActeurSim>(acteur);
					m.acteurSimple = false;
					m.outil = NkOutil::NK_POSER;
				}
			}

			/// La categorie d'un chemin de dossier du catalogue, -1 pour sa racine.
			/// Deux formes : le nom seul (un libelle du fil d'Ariane), ou
			/// « Acteurs/<nom> » (le chemin d'un nœud du rail ou d'une carte).
			int32 CategorieDuChemin(const char *chemin) noexcept {
				if (chemin == nullptr) {
					return -1;
				}
				const usize n = std::strlen(CHEMIN_RACINE);
				if (std::strncmp(chemin, CHEMIN_RACINE, n) == 0 && chemin[n] == '/') {
					chemin += n + 1u;
				}
				for (int32 k = 0; k < NB_CATEGORIES; ++k) {
					if (std::strcmp(chemin, NkCategorieActeurNom(static_cast<NkCategorieActeur>(k))) == 0) {
						return k;
					}
				}
				return -1;
			}

			bool Contient(const NkVector<NkString> &v, const NkString &s) noexcept {
				for (uint32 i = 0; i < v.Size(); ++i) {
					if (v[i] == s) {
						return true;
					}
				}
				return false;
			}

			NkString CheminNavigateur(const NkString &relatif) {
				NkString chemin(NK_CONTENU_RACINE);
				if (!relatif.Empty()) {
					chemin.Append('/');
					chemin.Append(relatif);
				}
				return chemin;
			}

			bool CommencePar(const char *s, const char *p) noexcept {
				return s != nullptr && std::strncmp(s, p, std::strlen(p)) == 0;
			}

			// ── LA PALETTE DES DOSSIERS (Unreal « Set Color ») ─────────────────
			// Des DONNEES de l'utilisateur, pas un theme : la couleur choisie pour
			// un dossier est gardee dans la memoire du navigateur, a cote du contenu.
			struct NkCouleurDossier {
					const char *nom;
					uint32 rgba;
			};
			const NkCouleurDossier kCouleurs[] = {{"Par défaut", 0u},		{"Rouge", 0xE5534BFFu}, {"Orange", 0xDB6D28FFu},
												  {"Jaune", 0xD29922FFu},	{"Vert", 0x3FB950FFu},	{"Cyan", 0x39C5CFFFu},
												  {"Bleu", 0x58A6FFFFu},	{"Violet", 0xA371F7FFu}, {"Rose", 0xDB61A2FFu}};
			constexpr int32 NB_COULEURS = static_cast<int32>(sizeof(kCouleurs) / sizeof(kCouleurs[0]));

			/// Le chemin RELATIF au Contenu d'un chemin du navigateur (« » = la racine).
			NkString Rel(const NkString &nav) {
				return NkEditeurRelatifContenu(nav.CStr());
			}

			// ── LA NAVIGATION ──────────────────────────────────────────────────
			/// Le navigateur va dans le dossier `relatif` du Contenu du projet.
			void AllerContenu(NkEditeurInterface &ui, const NkString &relatif) {
				ui.contenuProjet = true;
				ui.contenuTout = false;
				ui.contenuCollection = -1;
				ui.categorie = -1;
				if (!(ui.contenuDossier == relatif)) {
					ui.contenuChoisis.Clear();
					ui.contenuActif = NkString();
					ui.contenuAncre = NkString();
				}
				ui.contenuDossier = relatif;
				ui.contenuPerime = true;
				ui.contenu.scroll = 0.f;
			}

			void AllerCatalogue(NkEditeurInterface &ui, int32 categorie) {
				ui.contenuProjet = false;
				ui.contenuTout = false;
				ui.contenuCollection = -1;
				ui.categorie = categorie;
				ui.contenu.scroll = 0.f;
			}

			void AllerTout(NkEditeurInterface &ui) {
				ui.contenuProjet = false;
				ui.contenuTout = true;
				ui.contenuCollection = -1;
				ui.categorie = -1;
				ui.contenuChoisis.Clear();
				ui.contenu.scroll = 0.f;
			}

			void AllerCollection(NkEditeurInterface &ui, int32 k) {
				ui.contenuProjet = false;
				ui.contenuTout = false;
				ui.categorie = -1;
				if (ui.contenuCollection != k) {
					ui.contenuChoisis.Clear();
				}
				ui.contenuCollection = k;
				ui.contenu.collectionActive = k;
				ui.contenu.scroll = 0.f;
			}

			/// UN chemin du navigateur, ou qu'il vienne (rail, fil d'Ariane refait,
			/// favori, historique, carte de dossier). Rend faux si inconnu.
			bool Aller(NkEditeurInterface &ui, const char *chemin) {
				if (chemin == nullptr || chemin[0] == 0) {
					return false;
				}
				if (std::strcmp(chemin, CHEMIN_TOUT) == 0) {
					AllerTout(ui);
					return true;
				}
				if (NkEditeurCheminEstContenu(chemin)) {
					AllerContenu(ui, NkEditeurRelatifContenu(chemin));
					return true;
				}
				if (CommencePar(chemin, CHEMIN_COLLECTIONS) && chemin[std::strlen(CHEMIN_COLLECTIONS)] == '/') {
					const char *nom = chemin + std::strlen(CHEMIN_COLLECTIONS) + 1u;
					for (uint32 k = 0; k < ui.contenuMeta.collections.Size(); ++k) {
						if (ui.contenuMeta.collections[k].nom == NkString(nom)) {
							AllerCollection(ui, static_cast<int32>(k));
							return true;
						}
					}
					return false;
				}
				if (std::strcmp(chemin, CHEMIN_RACINE) == 0) {
					AllerCatalogue(ui, -1);
					return true;
				}
				const int32 k = CategorieDuChemin(chemin);
				if (k >= 0) {
					AllerCatalogue(ui, k);
					return true;
				}
				return false;
			}

			/// Le fil d'Ariane de la vue courante : « Tout », puis les segments.
			void FilDAriane(const NkEditeurInterface &ui, NkVector<NkString> &fil) {
				fil.Clear();
				fil.PushBack(NkString(CHEMIN_TOUT));
				if (ui.contenuTout) {
					return;
				}
				if (ui.contenuCollection >= 0 && static_cast<uint32>(ui.contenuCollection) < ui.contenuMeta.collections.Size()) {
					fil.PushBack(NkString(CHEMIN_COLLECTIONS));
					fil.PushBack(ui.contenuMeta.collections[static_cast<uint32>(ui.contenuCollection)].nom);
					return;
				}
				if (ui.contenuProjet) {
					fil.PushBack(NkString(NK_CONTENU_RACINE));
					const char *d = ui.contenuDossier.CStr();
					while (*d != 0) {
						const char *f = d;
						while (*f != 0 && *f != '/') {
							++f;
						}
						fil.PushBack(NkString(d, static_cast<usize>(f - d)));
						d = *f == '/' ? f + 1 : f;
					}
					return;
				}
				fil.PushBack(NkString(CHEMIN_RACINE));
				if (ui.categorie >= 0) {
					fil.PushBack(NkString(NkCategorieActeurNom(static_cast<NkCategorieActeur>(ui.categorie))));
				}
			}

			/// Le chemin de la miette `i` : les segments 1..i joints (0 = « Tout »).
			NkString CheminDeMiette(const NkVector<NkString> &fil, int32 i) {
				if (i <= 0) {
					return NkString(CHEMIN_TOUT);
				}
				NkString r;
				for (int32 k = 1; k <= i && k < static_cast<int32>(fil.Size()); ++k) {
					if (!r.Empty()) {
						r.Append('/');
					}
					r.Append(fil[static_cast<uint32>(k)]);
				}
				// « Collections » seul n'est pas un lieu : on remonte a « Tout ».
				return r == NkString(CHEMIN_COLLECTIONS) ? NkString(CHEMIN_TOUT) : r;
			}

			/// Le DOSSIER ou un geste cree ou colle : le dossier du clic droit s'il en
			/// vise un du Contenu, sinon le dossier courant (la racine du Contenu
			/// quand le navigateur montre autre chose).
			NkString DossierCible(const NkEditeurInterface &ui) {
				if (ui.contenuMenuDossier && NkEditeurCheminEstContenu(ui.contenuMenuChemin.CStr())) {
					return ui.contenuMenuChemin;
				}
				return ui.contenuProjet ? CheminNavigateur(ui.contenuDossier) : NkString(NK_CONTENU_RACINE);
			}

			/// Ce sur quoi un geste porte : la SELECTION, a laquelle le clic droit a
			/// deja ajoute sa carte (le composant la choisit). Les seuls chemins du
			/// Contenu, sa racine exclue.
			NkVector<NkString> Cibles(const NkEditeurInterface &ui) {
				NkVector<NkString> r;
				const bool menuHors = !ui.contenuMenuChemin.Empty() && !Contient(ui.contenuChoisis, ui.contenuMenuChemin);
				const NkVector<NkString> &src = menuHors ? NkVector<NkString>() : ui.contenuChoisis;
				if (menuHors) {
					r.PushBack(ui.contenuMenuChemin);
				}
				for (uint32 i = 0; i < src.Size(); ++i) {
					r.PushBack(src[i]);
				}
				NkVector<NkString> sur;
				for (uint32 i = 0; i < r.Size(); ++i) {
					if (NkEditeurCheminEstContenu(r[i].CStr()) && !Rel(r[i]).Empty()) {
						sur.PushBack(r[i]);
					}
				}
				return sur;
			}

			// ── LES CROCHETS DU COMPOSANT ─────────────────────────────────────
			void SurSelection(void *user, int32 index, const char *chemin) {
				(void)index;
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				Armer(c.m, ActeurDuChemin(chemin));
			}

			void SurNavigation(void *user, const char *chemin) {
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				// Le rail, un favori, l'historique : un CHEMIN complet. Le fil
				// d'Ariane : le seul LIBELLE de la miette -- recale apres le dessin
				// par son indice (NkContentBrowserResult::navigatedCrumb). Un libelle
				// inconnu ne fait donc rien ici.
				Aller(c.ui, chemin);
			}

			/// Double-clic : un DOSSIER s'ouvre (UE5) ; une scene aussi ; un prefab ou
			/// une image se pose au centre de la vue.
			void SurDoubleClic(void *user, int32 index, const char *chemin) {
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				if (index < 0 || index >= static_cast<int32>(c.ui.contenu.entries.Size())) {
					return;
				}
				if (c.ui.contenu.entries[static_cast<uint32>(index)].isFolder) {
					Aller(c.ui, chemin);
					return;
				}
				if (NkEditeurCheminEstContenu(chemin)) {
					c.ui.contenuMenuChemin = NkString(chemin);
					c.ui.contenuMenuDossier = false;
					NkEditeurExecuter(c, NK_A_CONTENU_OUVRIR_ASSET);
				}
			}

			/// Le rectangle de chaque carte A L'ECRAN : seul le composant connait sa
			/// grille ; le banc y vise ses clics, le depot de l'OS ses dossiers.
			void SurCarte(void *user, editorkit::NkComponentPaint &p, int32 index, float32 x, float32 y, float32 w, float32 h) {
				(void)p;
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				if (index >= 0 && index < static_cast<int32>(c.ui.contenuCartes.Size())) {
					c.ui.contenuCartes[static_cast<uint32>(index)] = NkRect{x, y, w, h};
				}
			}

			// ── LA TOUCHE D'UNKENY : SES VIGNETTES ─────────────────────────────
			// Peintes avec les primitives du kit (aucun atlas), dans un CARRE centre
			// dans la zone, teintes par le role de la nature (la bande de type a la
			// meme couleur). Aucune couleur en dur : des nuances du role.

			/// Les textures chargees A CETTE TRAME : on n'en decode que deux par
			/// trame, pour qu'un dossier de cent images ne fige pas l'editeur.
			uint32 gChargees = 0;

			uint32 TextureDe(NkEditeurCadre &c, const NkString &nav) {
				NkEditeurInterface &ui = c.ui;
				for (uint32 i = 0; i < ui.vignettesCles.Size(); ++i) {
					if (ui.vignettesCles[i] == nav) {
						return ui.vignettesTex[i];
					}
				}
				if (gChargees >= 2u) {
					return 0u; // la suivante a la trame d'apres
				}
				++gChargees;
				const NkString abs = NkEditeurCheminContenuAbsolu(c.m, nav.CStr());
				const uint32 id = c.m.textures.Charger(abs.CStr());
				ui.vignettesCles.PushBack(nav);
				ui.vignettesTex.PushBack(id);
				return id;
			}

			NkRect Carre(float32 x, float32 y, float32 w, float32 h, float32 k) {
				const float32 c = (w < h ? w : h) * k;
				return NkRect{x + (w - c) * 0.5f, y + (h - c) * 0.5f, c, c};
			}

			/// SCENE : un petit niveau 2D -- le ciel, le sol, une plate-forme, et la
			/// balle qui REBONDIT (le logo d'Unkeny), sa trace en pointilles.
			void PictoScene(editorkit::NkComponentPaint &p, const NkRect &b, uint16 role) {
				const uint32 vif = p.ColorOf(role);
				p.FillColor({b.x, b.y, b.w, b.h}, editorkit::NkTeinter(vif, -0.72f), b.w * 0.06f);
				p.FillColor({b.x, b.y + b.h * 0.78f, b.w, b.h * 0.22f}, editorkit::NkTeinter(vif, -0.25f), b.w * 0.04f);
				p.FillColor({b.x + b.w * 0.55f, b.y + b.h * 0.52f, b.w * 0.32f, b.h * 0.08f}, vif, b.w * 0.02f);
				p.FillColor({b.x + b.w * 0.12f, b.y + b.h * 0.62f, b.w * 0.2f, b.h * 0.16f}, editorkit::NkTeinter(vif, 0.25f), b.w * 0.02f);
				const float32 r = b.w * 0.1f;
				p.Ellipse({b.x + b.w * 0.4f - r, b.y + b.h * 0.3f - r, 2.f * r, 2.f * r}, static_cast<uint16>(NkRole::AccentSel));
				for (int32 k = 0; k < 3; ++k) {
					const float32 t = 0.12f + 0.07f * static_cast<float32>(k);
					const float32 pr = b.w * 0.025f;
					p.FillColor({b.x + b.w * (0.4f - t) - pr, b.y + b.h * (0.3f + t * 1.6f) - pr, 2.f * pr, 2.f * pr},
								editorkit::NkTeinter(p.ColorOf(static_cast<uint16>(NkRole::AccentSel)), -0.3f), pr);
				}
			}

			/// PREFAB : trois blocs empiles, le modele et ses instances.
			void PictoPrefab(editorkit::NkComponentPaint &p, const NkRect &b, uint16 role) {
				const uint32 vif = p.ColorOf(role);
				const float32 u = b.w / 16.f;
				auto Bloc = [&](float32 x, float32 y, float32 w, float32 h, float32 k) {
					p.FillColor({b.x + x * u, b.y + y * u, w * u, h * u}, editorkit::NkTeinter(vif, k), u);
					p.FillColor({b.x + (x + 1.f) * u, b.y + (y + 1.f) * u, (w - 2.f) * u, (h - 2.f) * u}, editorkit::NkTeinter(vif, k - 0.45f),
								u * 0.6f);
				};
				Bloc(2.f, 8.5f, 6.f, 6.f, 0.f);
				Bloc(8.f, 8.5f, 6.f, 6.f, -0.1f);
				Bloc(5.f, 2.5f, 6.f, 6.f, 0.15f);
			}

			/// CONTROLEUR D'ANIMATION : trois etats relies, l'entree en vert.
			void PictoControleur(editorkit::NkComponentPaint &p, const NkRect &b, uint16 role) {
				const uint32 vif = p.ColorOf(role);
				const float32 u = b.w / 16.f;
				p.Line(b.x + 4.f * u, b.y + 4.5f * u, b.x + 11.5f * u, b.y + 4.5f * u, role, u * 0.8f);
				p.Line(b.x + 11.5f * u, b.y + 4.5f * u, b.x + 8.f * u, b.y + 11.5f * u, role, u * 0.8f);
				p.Line(b.x + 4.f * u, b.y + 4.5f * u, b.x + 8.f * u, b.y + 11.5f * u, role, u * 0.8f);
				p.FillColor({b.x + 1.f * u, b.y + 3.f * u, 6.f * u, 3.f * u}, p.ColorOf(static_cast<uint16>(NkRole::StatusOk)), u);
				p.FillColor({b.x + 9.f * u, b.y + 3.f * u, 6.f * u, 3.f * u}, vif, u);
				p.FillColor({b.x + 5.f * u, b.y + 10.f * u, 6.f * u, 3.f * u}, editorkit::NkTeinter(vif, -0.2f), u);
			}

			/// SON : le haut-parleur et deux ondes.
			void PictoSon(editorkit::NkComponentPaint &p, const NkRect &b, uint16 role) {
				const uint32 vif = p.ColorOf(role);
				const float32 u = b.w / 16.f;
				p.FillColor({b.x + 2.f * u, b.y + 6.f * u, 3.f * u, 4.f * u}, vif, u * 0.4f);
				const float32 tri[8] = {b.x + 4.5f * u, b.y + 6.f * u, b.x + 8.5f * u, b.y + 2.5f * u,
										b.x + 8.5f * u, b.y + 13.5f * u, b.x + 4.5f * u, b.y + 10.f * u};
				if (!p.PolygonHex(tri, 4, vif)) {
					p.FillColor({b.x + 4.5f * u, b.y + 4.f * u, 4.f * u, 8.f * u}, vif);
				}
				for (int32 k = 0; k < 2; ++k) {
					const float32 x = b.x + (10.5f + 2.2f * static_cast<float32>(k)) * u;
					const float32 h = (2.f + 2.f * static_cast<float32>(k)) * u;
					p.Line(x, b.y + 8.f * u - h, x + u * 1.2f, b.y + 8.f * u, role, u * 0.8f);
					p.Line(x + u * 1.2f, b.y + 8.f * u, x, b.y + 8.f * u + h, role, u * 0.8f);
				}
			}

			/// POLICE : « Aa », en traits.
			void PictoPolice(editorkit::NkComponentPaint &p, const NkRect &b, uint16 role) {
				const float32 u = b.w / 16.f;
				const float32 e = u * 1.3f;
				p.Line(b.x + 1.5f * u, b.y + 13.f * u, b.x + 5.5f * u, b.y + 2.5f * u, role, e);
				p.Line(b.x + 5.5f * u, b.y + 2.5f * u, b.x + 9.5f * u, b.y + 13.f * u, role, e);
				p.Line(b.x + 3.f * u, b.y + 9.f * u, b.x + 8.f * u, b.y + 9.f * u, role, e);
				const NkRect o{b.x + 9.8f * u, b.y + 7.5f * u, 5.f * u, 5.5f * u};
				if (p.Ellipse({o.x, o.y, o.w, o.h}, role)) {
					p.Ellipse({o.x + e, o.y + e, o.w - 2.f * e, o.h - 2.f * e}, static_cast<uint16>(NkRole::WindowBg));
				}
				p.Line(b.x + 14.8f * u, b.y + 7.5f * u, b.x + 14.8f * u, b.y + 13.f * u, role, e);
			}

			/// La greffe `vignetteApp` : la touche d'Unkeny sur chaque carte.
			bool SurVignette(void *user, editorkit::NkComponentPaint &p, int32 index, float32 x, float32 y, float32 w, float32 h) {
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				if (index < 0 || index >= static_cast<int32>(c.ui.contenu.entries.Size())) {
					return false;
				}
				const editorkit::NkAssetEntry &e = c.ui.contenu.entries[static_cast<uint32>(index)];
				// Un ACTEUR du catalogue : la pastille de sa couleur dans la scene --
				// ce qu'une icone generique ne dit pas.
				const int32 a = ActeurDuChemin(e.path.CStr());
				if (a >= 0 || a == -2) {
					uint32 couleur = 0xB0B0B0FFu;
					if (a >= 0) {
						couleur = NkActeurSimInfo(static_cast<NkActeurSim>(a)).couleur | 0xFFu;
					}
					const NkRect r = Carre(x, y, w, h, a == -2 ? 0.5f : 0.55f);
					p.FillColor({r.x, r.y, r.w, r.h}, couleur, a == -2 ? r.w * 0.1f : r.w * 0.5f);
					return true;
				}
				if (!NkEditeurCheminEstContenu(e.path.CStr())) {
					return false;
				}
				const NkNatureContenu n = NkEditeurNatureFichier(e.path.CStr());
				switch (n.type) {
					case NkAssetType::Texture2D: {
						// LA VRAIE IMAGE, dans son rapport (de l'air, pas d'etirement).
						const uint32 tex = TextureDe(c, e.path);
						int32 tw = 0, th = 0;
						if (tex == 0u || !c.m.textures.Taille(tex, tw, th) || tw <= 0 || th <= 0) {
							return false;
						}
						float32 iw = w, ih = h;
						if (static_cast<float32>(tw) / static_cast<float32>(th) > w / h) {
							ih = w * static_cast<float32>(th) / static_cast<float32>(tw);
						} else {
							iw = h * static_cast<float32>(tw) / static_cast<float32>(th);
						}
						const float32 ix = x + (w - iw) * 0.5f, iy = y + (h - ih) * 0.5f;
						const float32 xy[8] = {ix, iy, ix + iw, iy, ix + iw, iy + ih, ix, iy + ih};
						const float32 uv[8] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
						return p.ImagePolygone(xy, uv, 4, tex, 100.f);
					}
					case NkAssetType::Scene:
					case NkAssetType::Map:
					case NkAssetType::World:
						PictoScene(p, Carre(x, y, w, h, 0.86f), e.kindRole);
						return true;
					case NkAssetType::Prefab:
						PictoPrefab(p, Carre(x, y, w, h, 0.8f), e.kindRole);
						return true;
					case NkAssetType::Animation:
					case NkAssetType::AnimationController:
						PictoControleur(p, Carre(x, y, w, h, 0.8f), e.kindRole);
						return true;
					case NkAssetType::Sound:
						PictoSon(p, Carre(x, y, w, h, 0.75f), e.kindRole);
						return true;
					case NkAssetType::Font:
						PictoPolice(p, Carre(x, y, w, h, 0.75f), e.kindRole);
						return true;
					default:
						return false;
				}
			}

			void SurImporter(void *user) {
				NkEditeurExecuter(*static_cast<NkEditeurCadre *>(user), NK_A_CONTENU_IMPORTER);
			}

			void SurToutEnregistrer(void *user) {
				NkEditeurExecuter(*static_cast<NkEditeurCadre *>(user), NK_A_ENREGISTRER);
			}

			// ── LES REGLAGES ET LA RECONSTRUCTION ──────────────────────────────
			/// Les puces de type du CONTENU : une couleur par nature d'Unkeny (celles
			/// de NkEditeurNatureFichier, jamais recopiees ailleurs).
			struct NkPuce {
					const char *nom;
					NkRole role;
			};
			const NkPuce kPucesContenu[] = {{"Scène", NkRole::TypeMesh},	  {"Prefab", NkRole::AxisZ}, {"Image", NkRole::TypeTex},
											{"Son", NkRole::StatusWarn},	  {"Police", NkRole::TypeMat},
											{"Animation", NkRole::TypeAnim}};

			void PreparerReglages(NkEditeurInterface &ui) {
				if (ui.contenuPret) {
					return;
				}
				ui.contenuPret = true;
				ui.contenuReglages.Bind(editorkit::NkContentBrowserDecl());
				// (2026-10-01) LA VARIANTE D'UNREAL (document 02 §3) : sept zones.
				ui.contenuReglages.SetVariantByName("unreal");
				ui.contenuReglages.SetParam("show_badge", 0.f);
				ui.contenuReglages.SetParam("tree_width", 0.17f);
				ui.contenu.thumbSize = 72.f;
				// Unreal 5.8 : la rangee des filtres s'ouvre par l'entonnoir.
				ui.contenu.filtresOuverts = ui.demFiltres;
				// La vue s'ouvre sur le CONTENU, comme Unreal sur /Content -- Rihen,
				// 01/10 : « a gauche il n'y a que Acteurs ».
				ui.contenuProjet = true;
				ui.contenuDossier = NkString();
				ui.pucesAutres.Clear();
				for (int32 k = 0; k < NB_CATEGORIES; ++k) {
					editorkit::NkBrowserKind kind;
					kind.label = NkString(NkCategorieActeurNom(static_cast<NkCategorieActeur>(k)));
					kind.role = static_cast<uint16>(RoleCategorie(k));
					ui.pucesAutres.PushBack(kind);
				}
				ui.contenu.kinds.Clear();
				for (const NkPuce &p : kPucesContenu) {
					editorkit::NkBrowserKind kind;
					kind.label = NkString(p.nom);
					kind.role = static_cast<uint16>(p.role);
					ui.contenu.kinds.PushBack(kind);
				}
				ui.pucesContenu = true;
				ui.contenu.folders.toggled.PushBack(1u);
			}

			// ── LE CONTENU DU PROJET (lu du disque) ────────────────────────────
			/// Les identifiants de noeud du rail pour le Contenu : sa racine, puis un
			/// par sous-dossier (le catalogue garde 1 et 2..6).
			constexpr nk_uint64 ID_CONTENU = 1000u;

			/// Relit le dossier courant, le rail et la memoire : au plus une fois par
			/// seconde (un fichier ajoute hors de l'editeur finit par paraitre), et
			/// AUSSITOT apres un geste ou un changement de dossier.
			void RelireContenu(NkEditeurCadre &c) {
				NkEditeurInterface &ui = c.ui;
				ui.contenuListeAge += ui.dt;
				if (ui.contenuMetaPerimee || ui.contenuPerime || ui.contenuListeAge >= 1.f) {
					editorkit::NkDisqueMetaLire(NkEditeurDossierContenu(c.m).CStr(), ui.contenuMeta);
					ui.contenuMetaPerimee = false;
					if (ui.contenuCollection >= static_cast<int32>(ui.contenuMeta.collections.Size())) {
						ui.contenuCollection = -1;
					}
				}
				if (!ui.contenuPerime && ui.contenuListeAge < 1.f && ui.contenuListeDe == ui.contenuDossier) {
					return;
				}
				NkEditeurDossiersContenu(c.m, ui.contenuSousDossiers);
				// Un dossier supprime hors de l'editeur : on remonte a la racine.
				if (!ui.contenuDossier.Empty() && !Contient(ui.contenuSousDossiers, ui.contenuDossier)) {
					ui.contenuDossier = NkString();
				}
				NkEditeurListerContenu(c.m, ui.contenuDossier.CStr(), ui.contenuListe);
				ui.contenuListeDe = ui.contenuDossier;
				ui.contenuListeAge = 0.f;
				ui.contenuPerime = false;
			}

			/// Le noeud du rail d'un dossier du Contenu (sa racine a defaut).
			nk_uint64 IdDossierContenu(const NkEditeurInterface &ui, const NkString &relatif) {
				for (uint32 k = 0; k < ui.contenuSousDossiers.Size(); ++k) {
					if (ui.contenuSousDossiers[k] == relatif) {
						return ID_CONTENU + 1u + k;
					}
				}
				return ID_CONTENU;
			}

			/// La racine « Contenu » et ses sous-dossiers, dans le rail. Le rail veut
			/// l'ordre PREFIXE : NkEditeurDossiersContenu le rend ainsi. Chaque
			/// dossier porte la COULEUR que l'utilisateur lui a choisie.
			void DossiersContenu(NkEditeurCadre &c) {
				NkEditeurInterface &ui = c.ui;
				editorkit::NkTreeViewModel &f = ui.contenu.folders;
				editorkit::NkTreeNode r;
				r.id = ID_CONTENU;
				r.parent = -1;
				r.label = NkString(NK_CONTENU_RACINE);
				r.path = NkString(NK_CONTENU_RACINE);
				r.kindRole = static_cast<uint16>(NkRole::TypeFolder);
				r.silhouette = static_cast<uint8>(editorkit::NkAssetIcone::Dossier);
				r.couleur = ui.contenuMeta.Couleur(NkString());
				const int32 iRacine = static_cast<int32>(f.nodes.Size());
				f.nodes.PushBack(r);
				for (uint32 k = 0; k < ui.contenuSousDossiers.Size(); ++k) {
					const NkString &rel = ui.contenuSousDossiers[k];
					usize coupe = 0;
					bool aParent = false;
					for (usize i = 0; i < static_cast<usize>(rel.Length()); ++i) {
						if (rel.CStr()[i] == '/') {
							coupe = i;
							aParent = true;
						}
					}
					int32 parent = iRacine;
					if (aParent) {
						const NkString cheminParent = CheminNavigateur(NkString(rel.CStr(), coupe));
						for (int32 j = static_cast<int32>(f.nodes.Size()) - 1; j > iRacine; --j) {
							if (f.nodes[static_cast<uint32>(j)].path == cheminParent) {
								parent = j;
								break;
							}
						}
					}
					editorkit::NkTreeNode n;
					n.id = ID_CONTENU + 1u + k;
					n.parent = parent;
					n.label = NkString(aParent ? rel.CStr() + coupe + 1u : rel.CStr());
					n.path = CheminNavigateur(rel);
					n.kindRole = static_cast<uint16>(NkRole::TypeFolder);
					n.silhouette = static_cast<uint8>(editorkit::NkAssetIcone::Dossier);
					n.couleur = ui.contenuMeta.Couleur(rel);
					f.nodes.PushBack(n);
				}
			}

			/// L'extension d'un nom de fichier (le point compris), « » s'il n'en a pas.
			NkString ExtensionDe(const NkString &nom) {
				const char *ext = nullptr;
				for (const char *q = nom.CStr(); *q != 0; ++q) {
					if (*q == '.' && q != nom.CStr()) {
						ext = q;
					}
				}
				return ext != nullptr ? NkString(ext) : NkString();
			}

			/// Une carte du Contenu (fichier ou dossier), avec sa nature, sa couleur.
			editorkit::NkAssetEntry EntreeDe(const NkEditeurInterface &ui, const NkElementContenu &e) {
				editorkit::NkAssetEntry a;
				a.name = e.nom;
				if (!e.dossier) {
					// Sans EXTENSION, comme Unreal : le type est ecrit dessous et la
					// bande de couleur le redit. Le chemin, lui, la garde.
					const NkString ext = ExtensionDe(e.nom);
					a.name = NkString(e.nom.CStr(), e.nom.Length() - ext.Length());
				}
				a.path = CheminNavigateur(e.relatif);
				a.isFolder = e.dossier;
				// ⚠️ Chaines STATIQUES : l'entree ne garde qu'un pointeur.
				a.kindLabel = e.dossier ? "Dossier" : e.nature.libelle;
				a.kindRole = e.dossier ? static_cast<uint16>(NkRole::TypeFolder) : e.nature.role;
				a.icone = e.dossier ? static_cast<uint8>(editorkit::NkAssetIcone::Dossier) : e.nature.icone;
				a.taille = e.taille;
				a.dateModif = e.date;
				if (e.dossier) {
					a.couleur = ui.contenuMeta.Couleur(e.relatif);
				}
				return a;
			}

			/// La selection relue des CHEMINS gardes (les cartes sont reconstruites a
			/// chaque trame) : choisies, active, ancre.
			void Restaurer(NkEditeurInterface &ui) {
				editorkit::NkContentBrowserModel &m = ui.contenu;
				m.chosen.Clear();
				m.active = -1;
				m.ancre = -1;
				for (uint32 i = 0; i < m.entries.Size(); ++i) {
					const NkString &p = m.entries[i].path;
					if (Contient(ui.contenuChoisis, p)) {
						m.chosen.PushBack(static_cast<int32>(i));
						if (ui.contenuActif.Empty() || p == ui.contenuActif) {
							m.active = static_cast<int32>(i);
						}
					}
					if (p == ui.contenuAncre) {
						m.ancre = static_cast<int32>(i);
					}
				}
			}

			void Reconstruire(NkEditeurCadre &c) {
				NkEditeurInterface &ui = c.ui;
				editorkit::NkContentBrowserModel &m = ui.contenu;

				// ── Le rail : « Contenu » (le projet) PUIS « Acteurs » (le catalogue),
				//    deux racines -- l'ordre d'Unreal, ou /Content vient en tete et
				//    « C++ Classes » apres ─────────────────────────────────────────
				m.folders.nodes.Clear();
				RelireContenu(c);
				DossiersContenu(c);
				const int32 iActeurs = static_cast<int32>(m.folders.nodes.Size());
				editorkit::NkTreeNode racine;
				racine.id = 1u;
				racine.parent = -1;
				racine.label = NkString(CHEMIN_RACINE);
				racine.path = NkString(CHEMIN_RACINE);
				racine.silhouette = static_cast<uint8>(editorkit::NkAssetIcone::Dossier);
				racine.kindRole = static_cast<uint16>(NkRole::AccentSel);
				m.folders.nodes.PushBack(racine);
				for (int32 k = 0; k < NB_CATEGORIES; ++k) {
					editorkit::NkTreeNode n;
					n.id = static_cast<nk_uint64>(k) + 2u;
					n.parent = iActeurs;
					n.label = NkString(NkCategorieActeurNom(static_cast<NkCategorieActeur>(k)));
					n.path = NkString::Format("%s/%s", CHEMIN_RACINE, n.label.CStr());
					n.kindRole = static_cast<uint16>(RoleCategorie(k));
					n.silhouette = static_cast<uint8>(editorkit::NkAssetIcone::Dossier);
					m.folders.nodes.PushBack(n);
				}
				m.folders.active = 0u;
				if (ui.contenuProjet) {
					m.folders.active = IdDossierContenu(ui, ui.contenuDossier);
				} else if (!ui.contenuTout && ui.contenuCollection < 0) {
					m.folders.active = ui.categorie >= 0 ? static_cast<nk_uint64>(ui.categorie) + 2u : 1u;
				}
				m.folders.chosen.Clear();
				if (m.folders.active != 0u) {
					m.folders.chosen.PushBack(m.folders.active);
				}

				// ── Le fil d'Ariane, le chemin courant (l'historique du kit) ───────
				FilDAriane(ui, m.breadcrumb);
				m.cheminCourant = CheminDeMiette(m.breadcrumb, static_cast<int32>(m.breadcrumb.Size()) - 1);
				m.nomProjet = NkEditeurNomProjet(c.m);

				// ── Les sections Favoris et Collections, de la memoire ─────────────
				m.favoris.Clear();
				m.favorisCouleurs.Clear();
				for (uint32 i = 0; i < ui.contenuMeta.favoris.Size(); ++i) {
					m.favoris.PushBack(CheminNavigateur(ui.contenuMeta.favoris[i]));
					m.favorisCouleurs.PushBack(ui.contenuMeta.Couleur(ui.contenuMeta.favoris[i]));
				}
				m.collections.Clear();
				for (uint32 k = 0; k < ui.contenuMeta.collections.Size(); ++k) {
					editorkit::NkBrowserCollection col;
					col.nom = ui.contenuMeta.collections[k].nom;
					col.couleur = ui.contenuMeta.collections[k].couleur;
					col.compte = static_cast<uint32>(ui.contenuMeta.collections[k].elements.Size());
					m.collections.PushBack(col);
				}
				m.collectionActive = ui.contenuCollection;

				// ── Les cartes ────────────────────────────────────────────────────
				m.entries.Clear();
				if (ui.contenuTout) {
					// « Tout » : ses deux dossiers, comme « All » d'Unreal.
					editorkit::NkAssetEntry d;
					d.name = NkString(NK_CONTENU_RACINE);
					d.path = d.name;
					d.isFolder = true;
					d.kindLabel = "Dossier";
					d.kindRole = static_cast<uint16>(NkRole::TypeFolder);
					d.couleur = ui.contenuMeta.Couleur(NkString());
					m.entries.PushBack(d);
					editorkit::NkAssetEntry a;
					a.name = NkString(CHEMIN_RACINE);
					a.path = a.name;
					a.isFolder = true;
					a.kindLabel = "Catalogue";
					a.kindRole = static_cast<uint16>(NkRole::AccentSel);
					m.entries.PushBack(a);
					Restaurer(ui);
				} else if (ui.contenuCollection >= 0) {
					// UNE COLLECTION : ses assets qui existent encore.
					const editorkit::NkDisqueCollection &col = ui.contenuMeta.collections[static_cast<uint32>(ui.contenuCollection)];
					for (uint32 i = 0; i < col.elements.Size(); ++i) {
						const NkString abs = NkEditeurCheminContenu(c.m, col.elements[i].CStr());
						if (!NkFile::Exists(abs.CStr())) {
							continue;
						}
						NkElementContenu e;
						e.nom = editorkit::NkDisqueNom(col.elements[i].CStr());
						e.relatif = col.elements[i];
						e.nature = NkEditeurNatureFichier(e.nom.CStr());
						m.entries.PushBack(EntreeDe(ui, e));
					}
					Restaurer(ui);
				} else if (ui.contenuProjet) {
					for (uint32 i = 0; i < ui.contenuListe.Size(); ++i) {
						m.entries.PushBack(EntreeDe(ui, ui.contenuListe[i]));
					}
					Restaurer(ui);
				} else {
					// LE CATALOGUE. A la racine : les categories en dossiers, puis
					// l'entite simple. Dans un dossier : ses acteurs. Une RECHERCHE en
					// cours : tous les acteurs -- chercher « eau » doit trouver l'eau.
					int32 arme = -1;
					const bool cherche = m.filter[0] != '\0';
					if (ui.categorie < 0 && !cherche) {
						for (int32 k = 0; k < NB_CATEGORIES; ++k) {
							editorkit::NkAssetEntry d;
							d.name = NkString(NkCategorieActeurNom(static_cast<NkCategorieActeur>(k)));
							d.path = NkString::Format("%s/%s", CHEMIN_RACINE, d.name.CStr());
							d.isFolder = true;
							d.kindLabel = "Dossier";
							d.kindRole = static_cast<uint16>(RoleCategorie(k));
							m.entries.PushBack(d);
						}
					}
					if (ui.categorie < 0) {
						editorkit::NkAssetEntry e;
						e.name = NkString("Entité simple");
						e.path = NkString(CHEMIN_SIMPLE);
						e.kindLabel = "Entité";
						e.kindRole = static_cast<uint16>(NkRole::TextMuted);
						if (c.m.outil == NkOutil::NK_POSER && c.m.acteurSimple) {
							arme = static_cast<int32>(m.entries.Size());
						}
						m.entries.PushBack(e);
					}
					for (int32 i = 0; i < static_cast<int32>(NkActeurSim::NK_COUNT); ++i) {
						const NkInfoActeurSim &info = NkActeurSimInfo(static_cast<NkActeurSim>(i));
						const int32 k = static_cast<int32>(info.categorie);
						if ((ui.categorie < 0 && !cherche) || (ui.categorie >= 0 && !cherche && k != ui.categorie)) {
							continue;
						}
						editorkit::NkAssetEntry e;
						e.name = NkString(info.nom);
						char chemin[24];
						std::snprintf(chemin, sizeof(chemin), "acteur:%d", i);
						e.path = NkString(chemin);
						e.kindLabel = NkCategorieActeurNom(info.categorie);
						e.kindRole = static_cast<uint16>(RoleCategorie(k));
						e.userTag = static_cast<uint32>(i);
						if (c.m.outil == NkOutil::NK_POSER && !c.m.acteurSimple && c.m.acteur == static_cast<NkActeurSim>(i)) {
							arme = static_cast<int32>(m.entries.Size());
						}
						m.entries.PushBack(e);
					}
					// La carte ACTIVE est l'acteur ARME : ce que le prochain clic posera.
					m.active = arme;
					m.ancre = arme;
					m.chosen.Clear();
					if (arme >= 0) {
						m.chosen.PushBack(arme);
					}
				}
				// La ligne d'etat, a droite : ce que le presse-papiers tient.
				m.statusRight = NkString();
				if (!ui.pressePapierContenu.Empty()) {
					m.statusRight = NkString::Format("%u élément(s) %s", static_cast<uint32>(ui.pressePapierContenu.Size()),
													 ui.pressePapierCouper ? "à déplacer (Ctrl+V)" : "copié(s) (Ctrl+V)");
				}
				// Le RENOMMAGE en place : l'indice de la carte visee.
				m.renomme = -1;
				for (uint32 i = 0; i < m.entries.Size() && !ui.renommeChemin.Empty(); ++i) {
					if (m.entries[i].path == ui.renommeChemin) {
						m.renomme = static_cast<int32>(i);
					}
				}
				// Les rectangles des cartes : remis a zero, le dessin les releve.
				ui.contenuCartes.Clear();
				ui.contenuCartes.Resize(m.entries.Size());
				for (uint32 i = 0; i < ui.contenuCartes.Size(); ++i) {
					ui.contenuCartes[i] = NkRect{0.f, 0.f, 0.f, 0.f};
				}
			}

			// ── LE RENOMMAGE EN PLACE (F2) ─────────────────────────────────────
			void CommencerRenommage(NkEditeurModele &m, NkEditeurInterface &ui, const NkString &chemin) {
				if (!NkEditeurCheminEstContenu(chemin.CStr()) || Rel(chemin).Empty()) {
					return;
				}
				ui.renommeChemin = chemin;
				// Le champ se pose sur la CARTE : un dossier renomme depuis le RAIL (ou
				// un favori) n'est peut-etre pas dans la grille -- on montre son parent.
				const NkString parent = editorkit::NkDisqueParent(Rel(chemin).CStr());
				if (!ui.contenuProjet || !(ui.contenuDossier == parent)) {
					AllerContenu(ui, parent);
					ui.contenuChoisis.PushBack(chemin);
					ui.contenuActif = chemin;
				}
				// On edite le PIED : l'extension reste (Unreal edite le nom d'asset).
				// Un DOSSIER s'edite en entier (« v1.2 » n'a pas d'extension).
				const NkString nom = editorkit::NkDisqueNom(chemin.CStr());
				const bool dossier = NkDirectory::Exists(NkEditeurCheminContenu(m, chemin.CStr()).CStr());
				const NkString ext = dossier ? NkString() : ExtensionDe(nom);
				const usize lPied = nom.Length() - ext.Length();
				std::snprintf(ui.contenu.renommeTampon, sizeof(ui.contenu.renommeTampon), "%.*s", static_cast<int32>(lPied), nom.CStr());
				ui.contenu.focus = true;
				ui.renommeToutChoisir = true;
			}

			/// Valide (ou abandonne) le renommage en cours.
			void FinirRenommage(NkEditeurCadre &c, bool garder) {
				NkEditeurInterface &ui = c.ui;
				const NkString ancien = ui.renommeChemin;
				ui.renommeChemin = NkString();
				ui.contenu.renomme = -1;
				if (!garder || ancien.Empty()) {
					return;
				}
				const NkString nom = editorkit::NkDisqueNom(ancien.CStr());
				const bool dossier = NkDirectory::Exists(NkEditeurCheminContenu(c.m, ancien.CStr()).CStr());
				NkString neuf(ui.contenu.renommeTampon);
				if (!dossier) {
					neuf.Append(ExtensionDe(nom));
				}
				if (neuf.Empty() || neuf == nom) {
					return;
				}
				const NkString nav = NkEditeurRenommerContenu(c.m, ancien.CStr(), neuf.CStr());
				if (!nav.Empty()) {
					for (uint32 i = 0; i < ui.contenuChoisis.Size(); ++i) {
						if (ui.contenuChoisis[i] == ancien) {
							ui.contenuChoisis[i] = nav;
						}
					}
					if (ui.contenuActif == ancien) {
						ui.contenuActif = nav;
					}
					ui.contenuPerime = true;
					ui.contenuMetaPerimee = true;
				}
			}

			// ── POSER UN ASSET DANS LA SCENE (le glisser vers le viseur) ───────
			/// Un PREFAB s'instancie, une IMAGE devient un sprite -- deux gestes que
			/// le modele de l'editeur savait deja faire (NkPrefabs2D::Instancier,
			/// NkTextures2D::Charger). Annulables (Ctrl+Z).
			bool PoserAsset(NkEditeurModele &m, const NkString &nav, const NkVec2f &monde) {
				const NkNatureContenu n = NkEditeurNatureFichier(nav.CStr());
				// ABSOLU : NkTextures2D prefixe un chemin relatif par sa racine d'assets.
				const NkString abs = NkEditeurCheminContenuAbsolu(m, nav.CStr());
				if (n.type == NkAssetType::Prefab) {
					NkEditeurRetenir(m);
					NkString erreur;
					const uint32 id = m.prefabs.Charger(m.scene, abs.CStr(), m.RessourcesScene(), &erreur);
					const ecs::NkEntityId e = id != 0u ? m.prefabs.Instancier(m.scene, id, monde) : ecs::NkEntityId();
					if (!e.IsValid()) {
						NkEditeurAnnoncer(m, NkString::Format("Prefab illisible : %s", erreur.CStr()).CStr());
						return false;
					}
					m.selection = e;
					m.aSelection = true;
					NkEditeurAnnoncer(m, NkString::Format("Prefab posé : %s", nav.CStr()).CStr());
					return true;
				}
				if (n.type == NkAssetType::Texture2D) {
					const uint32 tex = m.textures.Charger(abs.CStr());
					int32 tw = 0, th = 0;
					if (tex == 0u || !m.textures.Taille(tex, tw, th) || tw <= 0 || th <= 0) {
						NkEditeurAnnoncer(m, "Image illisible");
						return false;
					}
					NkEditeurRetenir(m);
					NkString nom = editorkit::NkDisqueNom(nav.CStr());
					const NkString ext = ExtensionDe(nom);
					nom = NkString(nom.CStr(), nom.Length() - ext.Length());
					const ecs::NkEntityId e = m.scene.Creer(nom.CStr(), monde);
					NkSprite2D s;
					s.texId = tex;
					// Un metre de large, la hauteur au rapport de l'image.
					s.taille = NkVec2f(1.f, static_cast<float32>(th) / static_cast<float32>(tw));
					m.scene.Monde().Add<NkSprite2D>(e, s);
					m.selection = e;
					m.aSelection = true;
					NkEditeurAnnoncer(m, NkString::Format("Sprite posé : %s", nav.CStr()).CStr());
					return true;
				}
				NkEditeurAnnoncer(m, NkString::Format("« %s » ne se pose pas dans la scène", n.libelle).CStr());
				return false;
			}

			// ── LES DEMANDES DE DEMARRAGE (captures sans souris) ───────────────
			int32 CarteDuChemin(const NkEditeurInterface &ui, const NkString &chemin) {
				for (uint32 k = 0; k < ui.contenu.entries.Size() && k < ui.contenuCartes.Size(); ++k) {
					if (ui.contenu.entries[k].path == chemin && ui.contenuCartes[k].w > 0.f) {
						return static_cast<int32>(k);
					}
				}
				return -1;
			}

			void DemandesDemarrage(NkEditeurCadre &c) {
				NkEditeurInterface &ui = c.ui;
				if (ui.demContenu.Empty() && ui.demChoisir.Empty() && ui.demMenu.Empty() && ui.demDeposer.Empty() &&
					ui.demRenommer.Empty()) {
					return;
				}
				++ui.demTrame;
				if (ui.demTrame == 1) {
					if (!ui.demContenu.Empty()) {
						Aller(ui, ui.demContenu.CStr());
					}
					if (!ui.demChoisir.Empty()) {
						ui.contenuChoisis.Clear();
						const char *d = ui.demChoisir.CStr();
						while (*d != 0) {
							const char *f = d;
							while (*f != 0 && *f != ';') {
								++f;
							}
							if (f > d) {
								ui.contenuChoisis.PushBack(NkString(d, static_cast<usize>(f - d)));
							}
							d = *f == ';' ? f + 1 : f;
						}
						if (!ui.contenuChoisis.Empty()) {
							ui.contenuActif = ui.contenuChoisis[0];
						}
					}
				}
				if (ui.demTrame < 4) {
					return;
				}
				if (!ui.demMenu.Empty()) {
					const int32 k = CarteDuChemin(ui, ui.demMenu);
					if (k >= 0) {
						const NkRect r = ui.contenuCartes[static_cast<uint32>(k)];
						const editorkit::NkAssetEntry &e = ui.contenu.entries[static_cast<uint32>(k)];
						ui.contenuChoisis.Clear();
						ui.contenuChoisis.PushBack(e.path);
						ui.contenuActif = e.path;
						ui.contenuMenuChemin = e.path;
						ui.contenuMenuNom = e.name;
						ui.contenuMenuDossier = e.isFolder;
						ui.contenu.focus = true;
						NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CTX_CONTENU, NkRect{r.x + r.w * 0.6f, r.y + r.h * 0.4f, 0.f, 0.f});
					}
				}
				if (!ui.demDeposer.Empty()) {
					const char *chevron = std::strchr(ui.demDeposer.CStr(), '>');
					if (chevron != nullptr) {
						const NkString source(ui.demDeposer.CStr(), static_cast<usize>(chevron - ui.demDeposer.CStr()));
						const NkString cible(chevron + 1);
						const int32 k = CarteDuChemin(ui, cible);
						if (k >= 0) {
							const NkRect r = ui.contenuCartes[static_cast<uint32>(k)];
							ui.deposeSources.Clear();
							ui.deposeSources.PushBack(source);
							ui.deposeCible = cible;
							ui.contenuChoisis.Clear();
							ui.contenuChoisis.PushBack(source);
							ui.contenuActif = source;
							NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CONTENU_DEPOSER, NkRect{r.x + r.w * 0.55f, r.y + r.h * 0.45f, 0.f, 0.f});
						}
					}
				}
				// --contenu-renommer= : le champ du renommage ouvert, le nom choisi.
				if (!ui.demRenommer.Empty()) {
					ui.contenuChoisis.Clear();
					ui.contenuChoisis.PushBack(ui.demRenommer);
					ui.contenuActif = ui.demRenommer;
					CommencerRenommage(c.m, ui, ui.demRenommer);
				}
				ui.demContenu = NkString();
				ui.demChoisir = NkString();
				ui.demMenu = NkString();
				ui.demDeposer = NkString();
				ui.demRenommer = NkString();
			}

			// ── L'ONGLET « CONTENU » ────────────────────────────────────────────
			void OngletContenu(NkEditeurCadre &c, const NkRect &zone) {
				NkEditeurInterface &ui = c.ui;
				PreparerReglages(ui);
				gChargees = 0;
				ui.contenuZone = zone;
				// Le Contenu se choisit A PLUSIEURS ; le catalogue arme UN acteur.
				const bool catalogue = !ui.contenuProjet && !ui.contenuTout && ui.contenuCollection < 0;
				ui.contenuReglages.SetParam("multi_select", catalogue ? 0.f : 1.f);
				// Les puces : celles du Contenu, ou les categories du catalogue.
				if (ui.pucesContenu == catalogue) {
					const NkVector<editorkit::NkBrowserKind> t = ui.contenu.kinds;
					ui.contenu.kinds = ui.pucesAutres;
					ui.pucesAutres = t;
					ui.pucesContenu = !catalogue;
				}
				Reconstruire(c);
				const bool etaitContenu = !catalogue;

				editorkit::NkContentBrowserStyle s;
				s.values = &ui.contenuReglages;
				s.panelBg = static_cast<uint16>(NkRole::PanelBg);
				s.headerBg = static_cast<uint16>(NkRole::PanelHeader);
				s.border = static_cast<uint16>(NkRole::Border);
				s.text = static_cast<uint16>(NkRole::Text);
				s.textMuted = static_cast<uint16>(NkRole::TextMuted);
				// Unreal : la vignette sur un fond PLUS SOMBRE que le panneau, le pied
				// un ton au-dessus ; le survol eclaircit franchement.
				s.cardBg = static_cast<uint16>(NkRole::WindowBg);
				s.cardFooterBg = static_cast<uint16>(NkRole::PanelHeader);
				s.activeMark = static_cast<uint16>(NkRole::AccentUi);
				s.chosenMark = static_cast<uint16>(NkRole::AccentSel);
				s.folderTint = static_cast<uint16>(NkRole::TypeFolder);
				s.chipBg = static_cast<uint16>(NkRole::PanelHeader);
				s.badgeText = static_cast<uint16>(NkRole::PanelBg);
				s.statusBg = static_cast<uint16>(NkRole::PanelHeader);
				s.cardHover = static_cast<uint16>(NkRole::Border);
				s.addMark = static_cast<uint16>(NkRole::StatusOk);
				s.dragMark = static_cast<uint16>(NkRole::AccentSel);
				s.textOnAccent = static_cast<uint16>(NkRole::TextOnAccent);

				editorkit::NkContentBrowserHooks hooks;
				hooks.user = &c;
				hooks.onSelect = &SurSelection;
				hooks.onNavigate = &SurNavigation;
				hooks.onDoubleClick = &SurDoubleClic;
				hooks.cardOverlay = &SurCarte;
				hooks.vignetteApp = &SurVignette;
				hooks.onImport = &SurImporter;
				hooks.onSaveAll = &SurToutEnregistrer;

				const editorkit::NkComponentInput ci = NkEditeurEntreeComposant(c.ctx);
				editorkit::NkGuiComponentPaint peintre(c.ctx, c.theme);
				const editorkit::NkContentBrowserResult res = editorkit::NkDrawContentBrowser(
					peintre, ci, editorkit::NkPaintRect{zone.x, zone.y, zone.w, zone.h}, ui.contenu, s, hooks);
				// Ce qu'on TRAINE : les cibles de depot hors du tiroir s'eclairent.
				ui.contenuGlisse = res.glisserChemin;
				ui.boutonImporter = NkRect{res.importerX, res.importerY, res.importerW, res.importerH};
				ui.contenuPrecedent = NkRect{res.precedentX, res.precedentY, res.precedentW, res.precedentH};
				ui.contenuSuivant = NkRect{res.suivantX, res.suivantY, res.suivantW, res.suivantH};

				if (res.defilContenu > res.defilVue && res.defilW > 0.f && res.defilH > 0.f) {
					editorkit::NkVScrollbar(c.ctx, c.ctx.dl, NkRect{res.defilX, res.defilY, res.defilW, res.defilH}, ui.contenu.scroll,
											res.defilContenu, res.defilVue, c.ctx.GetId("tiroir.defil"), res.defilPas);
				}
				if (res.railDefilContenu > res.railDefilVue && res.railDefilW > 0.f && res.railDefilH > 0.f) {
					editorkit::NkVScrollbar(c.ctx, c.ctx.dl, NkRect{res.railDefilX, res.railDefilY, res.railDefilW, res.railDefilH},
											ui.contenu.folders.scroll, res.railDefilContenu, res.railDefilVue, c.ctx.GetId("tiroir.rail"),
											res.railDefilPas);
				}

				// ── Le fil d'Ariane : une miette rend son LIBELLE ; on refait le
				//    chemin avec son INDICE ─────────────────────────────────────────
				if (res.navigatedCrumb >= 0) {
					Aller(ui, CheminDeMiette(ui.contenu.breadcrumb, res.navigatedCrumb).CStr());
				}
				if (res.collectionCliquee >= 0) {
					AllerCollection(ui, res.collectionCliquee);
				}
				// Les CHOISIS, relus du composant : des CHEMINS -- les DOSSIERS compris
				// (Rihen, 01/10 : « on ne peut pas selectionner les dossiers »).
				if (etaitContenu && !catalogue && res.selectionChanged && !res.navigated) {
					ui.contenuChoisis.Clear();
					for (uint32 i = 0; i < ui.contenu.chosen.Size(); ++i) {
						const int32 k = ui.contenu.chosen[i];
						if (k >= 0 && k < static_cast<int32>(ui.contenu.entries.Size())) {
							ui.contenuChoisis.PushBack(ui.contenu.entries[static_cast<uint32>(k)].path);
						}
					}
					const int32 a = ui.contenu.active;
					ui.contenuActif = (a >= 0 && a < static_cast<int32>(ui.contenu.entries.Size())) ? ui.contenu.entries[static_cast<uint32>(a)].path
																									   : NkString();
					const int32 an = ui.contenu.ancre;
					ui.contenuAncre = (an >= 0 && an < static_cast<int32>(ui.contenu.entries.Size())) ? ui.contenu.entries[static_cast<uint32>(an)].path
																									   : NkString();
				}

				// ── Le clic DROIT : une carte, un dossier du rail ou un favori, une
				//    collection, ou le vide ─────────────────────────────────────────
				if (res.menuCollection >= 0) {
					ui.collectionMenu = res.menuCollection;
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CTX_COLLECTION, NkRect{res.menuX, res.menuY, 0.f, 0.f});
				} else if (res.menuIndex != -2) {
					ui.contenuMenuChemin = NkString();
					ui.contenuMenuNom = NkString();
					ui.contenuMenuDossier = false;
					if (!res.menuCheminRail.Empty()) {
						ui.contenuMenuChemin = res.menuCheminRail;
						ui.contenuMenuNom = res.menuCheminRail;
						ui.contenuMenuDossier = true;
					} else if (res.menuIndex >= 0 && res.menuIndex < static_cast<int32>(ui.contenu.entries.Size())) {
						const editorkit::NkAssetEntry &e = ui.contenu.entries[static_cast<uint32>(res.menuIndex)];
						ui.contenuMenuChemin = e.path;
						ui.contenuMenuNom = e.name;
						ui.contenuMenuDossier = e.isFolder;
					}
					NkEditeurOuvrirMenu(c, ui.contenuMenuChemin.Empty() ? NkMenuEditeur::NK_CTX_CONTENU_VIDE : NkMenuEditeur::NK_CTX_CONTENU,
										NkRect{res.menuX, res.menuY, 0.f, 0.f});
				}
				// ── Les trois menus de bouton : sous le bouton ──
				const NkRect bouton{res.menuBoutonX, res.menuBoutonY, res.menuBoutonW, res.menuBoutonH};
				if (res.ajouterDemande) {
					ui.contenuMenuChemin = NkString();
					ui.contenuMenuDossier = false;
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CONTENU_AJOUTER, bouton);
				} else if (res.reglagesDemandes) {
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CONTENU_REGLAGES, bouton);
				} else if (res.triDemande) {
					NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CONTENU_TRI, bouton);
				}
				if (res.collectionCreer) {
					NkEditeurExecuter(c, NK_A_CONTENU_NOUVELLE_COLLECTION);
				}
				// ── Le GLISSER lache sur un dossier : « Deplacer ici / Copier ici »
				//    (Unreal). Seulement du Contenu vers le Contenu. ────────────────
				if (!res.deposeSources.Empty() && NkEditeurCheminEstContenu(res.deposeCible.CStr())) {
					NkVector<NkString> sources;
					for (uint32 i = 0; i < res.deposeSources.Size(); ++i) {
						if (NkEditeurCheminEstContenu(res.deposeSources[i].CStr()) && !Rel(res.deposeSources[i]).Empty()) {
							sources.PushBack(res.deposeSources[i]);
						}
					}
					if (!sources.Empty()) {
						ui.deposeSources = sources;
						ui.deposeCible = res.deposeCible;
						NkEditeurOuvrirMenu(c, NkMenuEditeur::NK_CONTENU_DEPOSER, NkRect{res.deposeX, res.deposeY, 0.f, 0.f});
					}
				}

				const nkgui::NkGuiInput &in = c.ctx.input;
				editorkit::NkOverlayFieldStyle st;
				st.fond = false;
				st.bord = false;
				st.texte = c.pal.texte;
				// ── La recherche : le composant a pose le focus, l'hote a le clavier ─
				if (res.rechercheW > 0.f && ui.contenu.searchFocused) {
					if (in.KeyPressed(nkgui::NkGuiKey::Escape) || in.KeyPressed(nkgui::NkGuiKey::Enter)) {
						ui.contenu.searchFocused = false;
						ui.toucheChamp = true;
					}
					const NkRect champ{res.rechercheX, res.rechercheY, res.rechercheW - 4.f, res.rechercheH};
					c.ctx.dl.AddRectFilled(NkRect{champ.x, champ.y + 1.f, champ.w, champ.h - 2.f}, c.pal.fond);
					editorkit::NkOverlayTextField(c.ctx, c.ctx.dl, c.police, champ, ui.contenu.filter,
												  static_cast<int32>(sizeof(ui.contenu.filter)), ui.contenu.searchFocused, &st);
				}
				// ── La recherche d'une SECTION des sources (Favoris, projet, Collections)
				if (res.sourcesRechercheW > 0.f && res.sourcesTampon != nullptr && ui.contenu.sourcesRechercheFocus) {
					if (in.KeyPressed(nkgui::NkGuiKey::Escape) || in.KeyPressed(nkgui::NkGuiKey::Enter)) {
						ui.contenu.sourcesRechercheFocus = false;
						ui.toucheChamp = true;
					}
					const NkRect champ{res.sourcesRechercheX, res.sourcesRechercheY, res.sourcesRechercheW, res.sourcesRechercheH};
					c.ctx.dl.AddRectFilled(NkRect{champ.x, champ.y + 1.f, champ.w, champ.h - 2.f}, c.pal.fond);
					editorkit::NkOverlayTextField(c.ctx, c.ctx.dl, c.police, champ, res.sourcesTampon, res.sourcesTamponTaille,
												  ui.contenu.sourcesRechercheFocus, &st);
				}
				// ── Le RENOMMAGE en place (F2) : le champ sur le nom de la carte ──
				ui.contenuRenommeRect = NkRect{0.f, 0.f, 0.f, 0.f};
				if (!ui.renommeChemin.Empty()) {
					if (res.renommeW > 0.f) {
						const NkRect champ{res.renommeX, res.renommeY, res.renommeW, res.renommeH};
						ui.contenuRenommeRect = champ;
						c.ctx.dl.AddRectFilled(champ, c.pal.champ, 2.f);
						c.ctx.dl.AddRect(champ, c.pal.accent, 1.f, 2.f);
						// (2026-10-01, retour de Rihen : « pas un vrai champ ») Le VRAI
						// champ du kit, regle comme un nom de fichier d'Unreal : le nom
						// entier choisi a l'ouverture, double-clic = un mot, les accents
						// s'ecrivent. Curseur, Maj+fleches, Origine / Fin, Ctrl+A / C /
						// X / V sont ceux du champ ; Echap et Entree, juste dessous.
						editorkit::NkOverlayFieldStyle stNom = st;
						stNom.motAuDoubleClic = true;
						stNom.utf8 = true;
						if (ui.renommeToutChoisir) {
							ui.renommeToutChoisir = false;
							c.ctx.input.wantSelectAll = true;
						}
						editorkit::NkOverlayTextField(c.ctx, c.ctx.dl, c.police, champ, ui.contenu.renommeTampon,
													  static_cast<int32>(sizeof(ui.contenu.renommeTampon)), true, &stNom);
						if (in.KeyPressed(nkgui::NkGuiKey::Enter)) {
							FinirRenommage(c, true);
							ui.toucheChamp = true;
						} else if (in.KeyPressed(nkgui::NkGuiKey::Escape)) {
							FinirRenommage(c, false);
							ui.toucheChamp = true;
						} else if ((in.mouseClicked[0] || in.mouseClicked[1]) && !NkEditeurDans(champ, in.mousePos)) {
							// Un clic AILLEURS valide (Unreal) : ce qui est tape est garde.
							FinirRenommage(c, true);
						}
					} else if (in.mouseClicked[0] || in.mouseClicked[1]) {
						FinirRenommage(c, true);
					}
				}
				// ── La CLOISON sources | vue, qui se tire (UE5) ────────────────────
				if (res.panneauxH > 0.f && !ui.contenu.treeCollapsed) {
					const float32 frac = ui.contenuReglages.Param("tree_width");
					const float32 x = zone.x + zone.w * frac;
					const NkRect poignee{x - 3.f, res.panneauxY, 6.f, res.panneauxH};
					const bool survol = NkEditeurDans(poignee, in.mousePos) && res.glisserChemin.Empty();
					if (survol && in.mouseClicked[0]) {
						ui.cloisonContenu = true;
					}
					if (ui.cloisonContenu) {
						if (in.mouseDown[0]) {
							// Bornes de la declaration du kit : 10 % a 45 %.
							float32 f = (in.mousePos.x - zone.x) / (zone.w > 1.f ? zone.w : 1.f);
							f = f < 0.10f ? 0.10f : (f > 0.45f ? 0.45f : f);
							ui.contenuReglages.SetParam("tree_width", f);
						} else {
							ui.cloisonContenu = false;
						}
					}
					if (survol || ui.cloisonContenu) {
						c.ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
						c.ctx.dl.AddRectFilled(NkRect{x - 1.f, res.panneauxY, 2.f, res.panneauxH}, c.pal.accent);
					}
				}

				auto &over = c.ctx.dlOverlay;
				// (2026-10-01, retour 4 de Rihen) Une IMAGE trainee vise aussi un
				// SPRITE : la reference de texture des Details, ou le sprite sous le
				// curseur dans la vue (Unreal pose un materiau ainsi).
				const bool image = !res.glisserChemin.Empty() && NkEditeurEstImage(res.glisserChemin.CStr());
				const bool surDetails = image && c.m.aSelection && NkEditeurDans(ui.detailsTexture, in.mousePos);
				ecs::NkEntityId spriteVise;
				if (image && !surDetails && NkEditeurDans(ui.viseur, in.mousePos)) {
					const NkVec2f monde = c.m.scene.Camera().EcranVersMonde(NkVec2f(in.mousePos.x, in.mousePos.y));
					ecs::NkEntityId e;
					if (NkEditeurPrendreSous(c.m, monde, e) && c.m.scene.Monde().Has<NkSprite2D>(e)) {
						spriteVise = e;
					}
				}
				if (!res.glisserChemin.Empty()) {
					if (in.mouseReleased[0]) {
						// LE DEPOT DANS LA VUE : lache hors des volets du composant. Un
						// acteur du catalogue s'y pose ; un PREFAB s'y instancie ; une
						// IMAGE y devient un sprite -- ou, lachee SUR un sprite (ou sur
						// la reference de texture des Details), devient SA texture. Un
						// dossier ne pose rien.
						if (surDetails) {
							NkEditeurTextureSprite(c.m, c.m.selection, res.glisserChemin.CStr());
						} else if (spriteVise.IsValid()) {
							NkEditeurTextureSprite(c.m, spriteVise, res.glisserChemin.CStr());
						} else if (NkEditeurDans(ui.viseur, in.mousePos)) {
							const NkVec2f monde = c.m.scene.Camera().EcranVersMonde(NkVec2f(in.mousePos.x, in.mousePos.y));
							const int32 acteur = ActeurDuChemin(res.glisserChemin.CStr());
							if (acteur != -1) {
								Armer(c.m, acteur);
								NkEditeurPoser(c.m, monde);
							} else if (NkEditeurCheminEstContenu(res.glisserChemin.CStr()) &&
									   !NkDirectory::Exists(NkEditeurCheminContenu(c.m, res.glisserChemin.CStr()).CStr())) {
								PoserAsset(c.m, res.glisserChemin, monde);
							}
						}
					} else {
						// Le fantome : ce qu'on emporte, sous le curseur -- et ce qu'il
						// deviendra s'il vise un sprite.
						NkString libelle = res.glisserLibelle.Empty() ? res.glisserChemin : res.glisserLibelle;
						if (surDetails || spriteVise.IsValid()) {
							const NkEtiquette *et = c.m.scene.Monde().Get<NkEtiquette>(surDetails ? c.m.selection : spriteVise);
							libelle.Append(NkString::Format("  →  texture de « %s »", et != nullptr ? et->nom : "").CStr());
						}
						const char *lib = libelle.CStr();
						const float32 w = renderer::NkTexteLargeur(c.police, lib) + 16.f;
						const NkRect g{in.mousePos.x + 12.f, in.mousePos.y + 10.f, w, 22.f};
						over.AddRectFilled(g, c.pal.entete, 2.f);
						over.AddRect(g, res.glisserCible.Empty() ? c.pal.accent : c.pal.selection, 1.f, 2.f);
						renderer::NkTexteDansBoite(over, c.police, g, lib, c.pal.texte);
					}
				}
				// L'infobulle, relevee par le composant et peinte par l'hote, au-dessus.
				if (!res.infobulle.Empty() && res.glisserChemin.Empty()) {
					const float32 w = renderer::NkTexteLargeur(c.petite, res.infobulle.CStr()) + 14.f;
					float32 x = res.infobulleX;
					if (x + w > ui.ecran.w - 4.f) {
						x = ui.ecran.w - 4.f - w;
					}
					const float32 h = 20.f;
					const NkRect b{x, res.infobulleY - h - 4.f, w, h};
					over.AddRectFilled(b, c.pal.entete, 2.f);
					over.AddRect(b, c.pal.bord, 1.f, 2.f);
					renderer::NkTexteDansBoite(over, c.petite, b, res.infobulle.CStr(), c.pal.texte);
				}
				DemandesDemarrage(c);
			}

			/// LE JOURNAL DU TIROIR, a la maniere de l'Output Log d'Unreal (2026-10-01,
			/// retour 7 de Rihen : « le Journal du bas n'a pas de filtres ») : les
			/// puces Tout / Avertissements / Erreurs avec leurs comptes, la recherche,
			/// « Copier » et « Effacer » ; chaque ligne nettoyee de ses codes ANSI
			/// (NkSansAnsi), classee (NkJournalConstruction::NiveauDe) et coloree par
			/// niveau -- les MEMES pieces que l'onglet Journal de « Construire »
			/// (NkEditeurFenetreConstruire.h), pas une copie.
			void OngletJournal(NkEditeurCadre &c, const NkRect &zone) {
				NkEditeurInterface &ui = c.ui;
				auto &dl = c.ctx.dl;
				const nkgui::NkGuiInput &in = c.ctx.input;
				dl.AddRectFilled(zone, c.pal.panneau);
				ui.journalMontrees.Clear();
				ui.journalNiveaux.Clear();
				// ── Le classement : une fois par trame (le journal est borne) ──
				NkVector<NkString> propres;
				NkVector<uint8> niveaux;
				int32 nAvt = 0, nErr = 0;
				for (uint32 i = 0; i < ui.journal.Size(); ++i) {
					const NkString p = NkSansAnsi(ui.journal[i].CStr());
					const NkNiveauLigne n = NkJournalConstruction::NiveauDe(p);
					nAvt += n == NkNiveauLigne::NK_AVERTISSEMENT ? 1 : 0;
					nErr += n == NkNiveauLigne::NK_ERREUR ? 1 : 0;
					propres.PushBack(p);
					niveaux.PushBack(static_cast<uint8>(n));
				}
				// ── La barre : puces, recherche, Copier, Effacer ──
				const float32 bh = 22.f;
				const float32 by = zone.y + 5.f;
				float32 x = NkEditeurPucesNiveau(c, dl, zone.x + 8.f, by, bh, static_cast<int32>(ui.journal.Size()), nAvt, nErr,
												 ui.journalFiltre, nullptr, ui.journalPuces);
				const float32 wE = renderer::NkTexteLargeur(c.petite, "Effacer") + 22.f;
				const float32 wC = renderer::NkTexteLargeur(c.petite, "Copier") + 22.f;
				ui.journalEffacer = NkRect{zone.x + zone.w - 8.f - wE, by, wE, bh};
				ui.journalCopier = NkRect{ui.journalEffacer.x - 6.f - wC, by, wC, bh};
				const float32 xr = x + 6.f;
				ui.journalRechercheRect = NkRect{xr, by, ui.journalCopier.x - 10.f - xr, bh};
				const NkRect &rr = ui.journalRechercheRect;
				if (rr.w > 40.f) {
					if (in.mouseClicked[0]) {
						ui.journalRechercheFocus = NkEditeurDans(rr, in.mousePos);
					}
					if (ui.journalRechercheFocus && (in.KeyPressed(nkgui::NkGuiKey::Escape) || in.KeyPressed(nkgui::NkGuiKey::Enter))) {
						ui.journalRechercheFocus = false;
						ui.toucheChamp = true;
					}
					dl.AddRectFilled(rr, c.pal.champ, 2.f);
					dl.AddRect(rr, ui.journalRechercheFocus ? c.pal.accent : c.pal.bord, 1.f, 2.f);
					// la loupe
					const float32 lx = rr.x + 11.f, ly = rr.y + bh * 0.5f - 1.f;
					dl.AddCircle(nkgui::NkVec2{lx, ly}, 4.f, c.pal.attenue, 1.3f);
					dl.AddLine(nkgui::NkVec2{lx + 3.f, ly + 3.f}, nkgui::NkVec2{lx + 6.f, ly + 6.f}, c.pal.attenue, 1.5f);
					if (ui.journalRecherche[0] == '\0' && !ui.journalRechercheFocus) {
						renderer::NkTexte(dl, c.petite, rr.x + 22.f, rr.y + (bh - renderer::NkTexteHauteurLigne(c.petite, 12.f)) * 0.5f,
										  "Rechercher dans le journal", c.pal.attenue);
					}
					editorkit::NkOverlayFieldStyle st;
					st.fond = false;
					st.bord = false;
					st.texte = c.pal.texte;
					st.utf8 = true;
					editorkit::NkOverlayTextField(c.ctx, dl, c.police, NkRect{rr.x + 18.f, rr.y, rr.w - 20.f, rr.h}, ui.journalRecherche,
												  static_cast<int32>(sizeof(ui.journalRecherche)), ui.journalRechercheFocus, &st);
				}
				// ── Les lignes MONTREES : le plus RECENT en haut (c'est lui qu'on
				//    vient chercher, sans defiler) ──
				NkVector<uint32> montrees;
				for (int32 i = static_cast<int32>(ui.journal.Size()) - 1; i >= 0; --i) {
					const uint32 k = static_cast<uint32>(i);
					if (NkEditeurNiveauMontre(static_cast<NkNiveauLigne>(niveaux[k]), ui.journalFiltre) &&
						NkEditeurContientSansCasse(propres[k].CStr(), ui.journalRecherche)) {
						montrees.PushBack(k);
						ui.journalMontrees.PushBack(propres[k]);
						ui.journalNiveaux.PushBack(niveaux[k]);
					}
				}
				if (NkEditeurBouton(c, ui.journalCopier, "Copier", false, !montrees.Empty())) {
					// Dans l'ordre du temps : c'est ainsi qu'on colle un journal.
					NkString tout;
					for (int32 i = static_cast<int32>(montrees.Size()) - 1; i >= 0; --i) {
						tout += propres[montrees[static_cast<uint32>(i)]] + "\n";
					}
					c.ctx.SetClipboard(tout.CStr());
					ui.journalRetour = NkString::Format("%u ligne(s) copiée(s)", static_cast<unsigned>(montrees.Size()));
					ui.journalRetourJusqua = ui.temps + 2.5f;
				}
				if (NkEditeurBouton(c, ui.journalEffacer, "Effacer", false, !ui.journal.Empty())) {
					ui.journal.Clear();
					ui.defilJournal = 0.f;
					ui.journalMontrees.Clear();
					ui.journalNiveaux.Clear();
					montrees.Clear();
				}
				// ── La liste ──
				const NkRect liste{zone.x, by + bh + 6.f, zone.w, zone.y + zone.h - (by + bh + 6.f)};
				const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f) + 3.f;
				if (NkEditeurDans(liste, in.mousePos) && in.wheel != 0.f) {
					ui.defilJournal -= in.wheel * 3.f;
				}
				const float32 maxi = static_cast<float32>(montrees.Size()) - liste.h / lh + 1.f;
				ui.defilJournal = ui.defilJournal > maxi ? maxi : ui.defilJournal;
				ui.defilJournal = ui.defilJournal < 0.f ? 0.f : ui.defilJournal;
				if (!ui.journalRetour.Empty() && ui.temps < ui.journalRetourJusqua) {
					renderer::NkTexteADroite(dl, c.petite, ui.journalCopier.x - 10.f,
											 by + (bh - renderer::NkTexteHauteurLigne(c.petite, 12.f)) * 0.5f, ui.journalRetour.CStr(),
											 NkEditeurCouleurNiveau(c, NkNiveauLigne::NK_SUCCES));
				}
				if (montrees.Empty()) {
					const char *vide = ui.journal.Empty()					  ? "Le journal est vide : les annonces de l'éditeur s'y inscrivent."
									   : ui.journalRecherche[0] != '\0' ? "Aucune ligne ne correspond à la recherche."
									   : ui.journalFiltre == 1			  ? "Aucun avertissement."
																		  : "Aucune erreur.";
					renderer::NkTexteCentre(dl, c.police, liste.x + liste.w * 0.5f, liste.y + liste.h * 0.4f, vide, c.pal.attenue);
					return;
				}
				dl.PushClipRect(liste, true);
				float32 y = liste.y + 2.f;
				for (uint32 i = static_cast<uint32>(ui.defilJournal); i < montrees.Size() && y < liste.y + liste.h; ++i) {
					const uint32 k = montrees[i];
					const NkNiveauLigne n = static_cast<NkNiveauLigne>(niveaux[k]);
					NkEditeurBandeNiveau(c, dl, NkRect{liste.x + 4.f, y - 1.f, liste.w - 8.f, lh}, n);
					// Une ligne SIMPLE garde l'usage d'avant : la plus recente vive, les
					// autres attenuees ; un niveau (erreur, avertissement, succes) a sa
					// couleur.
					NkColor teinte = NkEditeurCouleurNiveau(c, n);
					if (n == NkNiveauLigne::NK_INFO) {
						teinte = k + 1u == ui.journal.Size() ? c.pal.texte : c.pal.attenue;
					}
					renderer::NkTexte(dl, c.police, liste.x + 10.f, y, propres[k].CStr(), teinte);
					y += lh;
				}
				dl.PopClipRect();
			}

			/// La memoire du navigateur, ecrite apres un geste qui la change.
			void EcrireMeta(NkEditeurCadre &c) {
				editorkit::NkDisqueMetaEcrire(NkEditeurDossierContenu(c.m).CStr(), c.ui.contenuMeta);
				c.ui.contenuMetaPerimee = true;
			}

		} // namespace

		int32 NkEditeurNbCouleursDossier() noexcept {
			return NB_COULEURS;
		}

		const char *NkEditeurCouleurDossier(int32 k, uint32 &rgba) noexcept {
			if (k < 0 || k >= NB_COULEURS) {
				rgba = 0u;
				return "";
			}
			rgba = kCouleurs[k].rgba;
			return kCouleurs[k].nom;
		}

		void NkEditeurActionContenu(NkEditeurCadre &c, int32 action) {
			NkEditeurModele &m = c.m;
			NkEditeurInterface &ui = c.ui;
			const char *chemin = ui.contenuMenuChemin.CStr();
			if (action >= NK_A_CONTENU_COULEUR && action < NK_A_CONTENU_COULEUR + NB_COULEURS) {
				// La couleur des DOSSIERS vises (Unreal « Set Color »).
				const uint32 rgba = kCouleurs[action - NK_A_CONTENU_COULEUR].rgba;
				const NkVector<NkString> cibles = Cibles(ui);
				uint32 n = 0;
				for (uint32 i = 0; i < cibles.Size(); ++i) {
					if (NkDirectory::Exists(NkEditeurCheminContenu(m, cibles[i].CStr()).CStr())) {
						ui.contenuMeta.PoserCouleur(Rel(cibles[i]), rgba);
						++n;
					}
				}
				if (n > 0u) {
					EcrireMeta(c);
				}
				return;
			}
			if (action >= NK_A_CONTENU_TAILLE && action < NK_A_CONTENU_TAILLE + 4) {
				static const float32 kTailles[4] = {48.f, 72.f, 112.f, 160.f};
				ui.contenu.thumbSize = kTailles[action - NK_A_CONTENU_TAILLE];
				return;
			}
			if (action >= NK_A_CONTENU_TRI && action < NK_A_CONTENU_TRI + static_cast<int32>(editorkit::NkBrowserTri::Count)) {
				ui.contenu.sortCle = static_cast<uint8>(action - NK_A_CONTENU_TRI);
				return;
			}
			if (action >= NK_A_CONTENU_COLLECTION && action < NK_A_CONTENU_COLLECTION + 20) {
				const uint32 k = static_cast<uint32>(action - NK_A_CONTENU_COLLECTION);
				if (k < ui.contenuMeta.collections.Size()) {
					const NkVector<NkString> cibles = Cibles(ui);
					for (uint32 i = 0; i < cibles.Size(); ++i) {
						const NkString rel = Rel(cibles[i]);
						if (!Contient(ui.contenuMeta.collections[k].elements, rel)) {
							ui.contenuMeta.collections[k].elements.PushBack(rel);
						}
					}
					EcrireMeta(c);
					NkEditeurAnnoncer(m, NkString::Format("%u élément(s) dans la collection « %s »", static_cast<uint32>(cibles.Size()),
														   ui.contenuMeta.collections[k].nom.CStr())
											 .CStr());
				}
				return;
			}
			switch (action) {
				case NK_A_CONTENU_POSER: {
					// Pose SANS armer : l'outil arme reste celui d'avant (la meme
					// regle que « + Ajouter », qui pose au centre de la vue).
					const int32 a = ActeurDuChemin(chemin);
					if (a == -1) {
						return;
					}
					const NkActeurSim acteurArme = m.acteur;
					const bool simpleArme = m.acteurSimple;
					m.acteurSimple = a == -2;
					if (a >= 0) {
						m.acteur = static_cast<NkActeurSim>(a);
					}
					NkEditeurPoser(m, m.scene.Camera().Centre());
					m.acteur = acteurArme;
					m.acteurSimple = simpleArme;
					break;
				}
				case NK_A_CONTENU_ARMER:
					Armer(m, ActeurDuChemin(chemin));
					break;
				case NK_A_CONTENU_OUVRIR:
					// Un dossier du Contenu, du catalogue, ou « Tout ».
					Aller(ui, chemin);
					break;
				case NK_A_CONTENU_RACINE:
					if (ui.contenuProjet) {
						AllerContenu(ui, NkString());
					} else {
						AllerCatalogue(ui, -1);
					}
					break;
				case NK_A_CONTENU_IMPORTER:
					// Le dossier vise par le clic droit (« Importer ici »), sinon le
					// courant. Le DIALOGUE s'ouvre a la trame suivante (NkEditeurApp).
					ui.importCible = NkString();
					if (ui.contenuMenuDossier && NkEditeurCheminEstContenu(chemin)) {
						ui.importCible = ui.contenuMenuChemin; // « Contenu » ou « Contenu/... »
					}
					ui.importDemande = true;
					break;
				case NK_A_CONTENU_EXPORTER:
					// Le fichier du clic droit s'ajoute aux choisis s'il n'en est pas.
					if (!ui.contenuMenuDossier && NkEditeurCheminEstContenu(chemin) && !Contient(ui.contenuChoisis, ui.contenuMenuChemin)) {
						ui.contenuChoisis.Clear();
						ui.contenuChoisis.PushBack(ui.contenuMenuChemin);
					}
					if (ui.contenuChoisis.Empty()) {
						NkEditeurAnnoncer(m, "Export : choisissez d'abord des assets du Contenu (Ctrl+clic pour plusieurs)");
					} else {
						ui.exportDemande = true;
					}
					break;
				// ── Creer (le menu « + Ajouter », le clic droit) ──
				case NK_A_CONTENU_NOUVEAU_DOSSIER:
				case NK_A_CONTENU_NOUVELLE_SCENE:
				case NK_A_CONTENU_NOUVEAU_PREFAB:
				case NK_A_CONTENU_NOUVEAU_CONTROLEUR: {
					const NkString dossier = DossierCible(ui);
					NkString cree;
					if (action == NK_A_CONTENU_NOUVEAU_DOSSIER) {
						cree = NkEditeurNouveauDossier(m, dossier.CStr());
					} else if (action == NK_A_CONTENU_NOUVELLE_SCENE) {
						cree = NkEditeurNouvelleSceneContenu(m, dossier.CStr());
					} else if (action == NK_A_CONTENU_NOUVEAU_PREFAB) {
						cree = NkEditeurNouveauPrefabContenu(m, dossier.CStr());
					} else {
						cree = NkEditeurNouveauControleurContenu(m, dossier.CStr());
					}
					if (!cree.Empty()) {
						// On le MONTRE, choisi, son nom pret a etre change (Unreal).
						if (!ui.contenu.verrouille) {
							AllerContenu(ui, Rel(dossier));
						}
						ui.contenuChoisis.Clear();
						ui.contenuChoisis.PushBack(cree);
						ui.contenuActif = cree;
						ui.contenuPerime = true;
						CommencerRenommage(m, ui, cree);
					}
					break;
				}
				// ── Le presse-papiers (Ctrl+X / Ctrl+C / Ctrl+V) ──
				case NK_A_CONTENU_COUPER:
				case NK_A_CONTENU_COPIER: {
					const NkVector<NkString> cibles = Cibles(ui);
					if (cibles.Empty()) {
						NkEditeurAnnoncer(m, "Rien à copier : choisissez des fichiers ou des dossiers du Contenu");
						return;
					}
					ui.pressePapierContenu = cibles;
					ui.pressePapierCouper = action == NK_A_CONTENU_COUPER;
					NkEditeurAnnoncer(m, NkString::Format("%u élément(s) %s", static_cast<uint32>(cibles.Size()),
														   ui.pressePapierCouper ? "coupé(s)" : "copié(s)")
											 .CStr());
					break;
				}
				case NK_A_CONTENU_COLLER: {
					if (ui.pressePapierContenu.Empty()) {
						NkEditeurAnnoncer(m, "Coller : le presse-papiers du navigateur est vide");
						return;
					}
					const NkString dossier = DossierCible(ui);
					NkVector<NkString> crees;
					NkEditeurCopierContenu(m, ui.pressePapierContenu, dossier.CStr(), ui.pressePapierCouper, &crees);
					// Un COUPER ne se colle qu'une fois : les sources n'existent plus.
					if (ui.pressePapierCouper) {
						ui.pressePapierContenu.Clear();
						ui.pressePapierCouper = false;
					}
					if (!ui.contenu.verrouille) {
						AllerContenu(ui, Rel(dossier));
					}
					ui.contenuChoisis = crees;
					ui.contenuActif = crees.Empty() ? NkString() : crees[0];
					ui.contenuPerime = true;
					ui.contenuMetaPerimee = true;
					break;
				}
				case NK_A_CONTENU_DUPLIQUER: {
					NkVector<NkString> crees;
					NkEditeurDupliquerContenu(m, Cibles(ui), &crees);
					if (!crees.Empty()) {
						ui.contenuChoisis = crees;
						ui.contenuActif = crees[0];
					}
					ui.contenuPerime = true;
					break;
				}
				case NK_A_CONTENU_RENOMMER: {
					// Le dossier du clic droit, sinon l'entree ACTIVE.
					NkString cible = ui.contenuMenuChemin;
					if (cible.Empty()) {
						cible = !ui.contenuActif.Empty() ? ui.contenuActif : (ui.contenuChoisis.Empty() ? NkString() : ui.contenuChoisis[0]);
					}
					CommencerRenommage(m, ui, cible);
					break;
				}
				case NK_A_CONTENU_SUPPRIMER: {
					// ⚠️ JAMAIS SANS CONFIRMATION : la boite s'ouvre, elle seule supprime.
					ui.contenuASupprimer = Cibles(ui);
					if (ui.contenuASupprimer.Empty()) {
						NkEditeurAnnoncer(m, "Supprimer : choisissez des fichiers ou des dossiers du Contenu");
						return;
					}
					NkEditeurReferencesContenu(m, ui.contenuASupprimer, ui.contenuReferences);
					break;
				}
				case NK_A_CONTENU_SUPPRIMER_OUI: {
					const NkVector<NkString> cibles = ui.contenuASupprimer;
					ui.contenuASupprimer.Clear();
					ui.contenuReferences.Clear();
					NkEditeurSupprimerContenu(m, cibles, ui.contenuCorbeille);
					ui.contenuChoisis.Clear();
					ui.contenuActif = NkString();
					ui.contenuPerime = true;
					ui.contenuMetaPerimee = true;
					// Le presse-papiers ne garde pas ce qui n'existe plus.
					NkVector<NkString> garde;
					for (uint32 i = 0; i < ui.pressePapierContenu.Size(); ++i) {
						if (NkFile::Exists(NkEditeurCheminContenu(m, ui.pressePapierContenu[i].CStr()).CStr()) ||
							NkDirectory::Exists(NkEditeurCheminContenu(m, ui.pressePapierContenu[i].CStr()).CStr())) {
							garde.PushBack(ui.pressePapierContenu[i]);
						}
					}
					ui.pressePapierContenu = garde;
					break;
				}
				// ── Le menu du glisser ──
				case NK_A_CONTENU_DEPLACER_ICI:
				case NK_A_CONTENU_COPIER_ICI: {
					const NkVector<NkString> sources = ui.deposeSources;
					const NkString cible = ui.deposeCible;
					ui.deposeSources.Clear();
					ui.deposeCible = NkString();
					NkVector<NkString> crees;
					NkEditeurCopierContenu(m, sources, cible.CStr(), action == NK_A_CONTENU_DEPLACER_ICI, &crees);
					ui.contenuChoisis.Clear();
					ui.contenuPerime = true;
					ui.contenuMetaPerimee = true;
					break;
				}
				case NK_A_CONTENU_FAVORI: {
					const NkString cible = ui.contenuMenuDossier ? ui.contenuMenuChemin : (ui.contenuChoisis.Empty() ? NkString() : ui.contenuChoisis[0]);
					if (!NkEditeurCheminEstContenu(cible.CStr())) {
						return;
					}
					ui.contenuMeta.BasculerFavori(Rel(cible));
					EcrireMeta(c);
					break;
				}
				case NK_A_CONTENU_TOUT_SELECTIONNER:
					ui.contenuChoisis.Clear();
					for (uint32 i = 0; i < ui.contenu.entries.Size(); ++i) {
						ui.contenuChoisis.PushBack(ui.contenu.entries[i].path);
					}
					ui.contenuActif = ui.contenuChoisis.Empty() ? NkString() : ui.contenuChoisis[0];
					break;
				case NK_A_CONTENU_COPIER_CHEMIN: {
					const NkString cible = !ui.contenuMenuChemin.Empty() ? ui.contenuMenuChemin : ui.contenuActif;
					if (NkEditeurCheminEstContenu(cible.CStr())) {
						c.ctx.SetClipboard(NkEditeurCheminContenu(m, cible.CStr()).CStr());
						NkEditeurAnnoncer(m, "Chemin copié dans le presse-papiers");
					}
					break;
				}
				case NK_A_CONTENU_OUVRIR_ASSET: {
					// Une SCENE s'ouvre (la question « non enregistree » est a
					// NkEditeurChrome, qui relit `sceneAOuvrir`). Un prefab ou une image
					// n'a pas encore d'editeur : on dit comment le poser, on ne pose
					// RIEN par surprise (un double-clic n'est pas un geste sur la scene).
					ui.sceneAOuvrir = NkString();
					const NkNatureContenu n = NkEditeurNatureFichier(chemin);
					if (n.type == NkAssetType::Scene) {
						ui.sceneAOuvrir = NkEditeurCheminContenu(m, chemin);
					} else if (n.type == NkAssetType::Prefab || n.type == NkAssetType::Texture2D) {
						NkEditeurAnnoncer(m, NkString::Format("« %s » : glissez-le dans la vue pour le poser (ou clic droit > Poser)",
															   editorkit::NkDisqueNom(chemin).CStr())
												 .CStr());
					} else {
						NkEditeurAnnoncer(m, NkString::Format("« %s » : pas d'éditeur pour ce type (glissez-le dans la vue s'il se pose)", n.libelle).CStr());
					}
					break;
				}
				case NK_A_CONTENU_POSER_ASSET:
					PoserAsset(m, ui.contenuMenuChemin, m.scene.Camera().Centre());
					break;
				case NK_A_CONTENU_AFFICHER_DOSSIERS:
					ui.contenu.montrerDossiers = !ui.contenu.montrerDossiers;
					break;
				case NK_A_CONTENU_FILTRES:
					ui.contenu.filtresOuverts = !ui.contenu.filtresOuverts;
					break;
				case NK_A_CONTENU_VUE_LISTE:
					ui.contenu.viewMode = ui.contenu.viewMode == 1 ? 0 : 1;
					break;
				case NK_A_CONTENU_TRI_SENS:
					ui.contenu.sortAsc = !ui.contenu.sortAsc;
					break;
				case NK_A_CONTENU_NOUVELLE_COLLECTION: {
					editorkit::NkDisqueCollection col;
					col.nom = NkString::Format("Collection %u", static_cast<uint32>(ui.contenuMeta.collections.Size()) + 1u);
					col.couleur = kCouleurs[1 + static_cast<int32>(ui.contenuMeta.collections.Size()) % (NB_COULEURS - 1)].rgba;
					// La selection y entre (Unreal : « Add to new collection »).
					const NkVector<NkString> cibles = Cibles(ui);
					for (uint32 i = 0; i < cibles.Size(); ++i) {
						col.elements.PushBack(Rel(cibles[i]));
					}
					ui.contenuMeta.collections.PushBack(col);
					EcrireMeta(c);
					NkEditeurAnnoncer(m, NkString::Format("Collection créée : %s", col.nom.CStr()).CStr());
					break;
				}
				case NK_A_CONTENU_SUPPRIMER_COLLECTION:
					// Une collection n'est qu'une LISTE : la retirer ne touche a aucun fichier.
					if (ui.collectionMenu >= 0 && static_cast<uint32>(ui.collectionMenu) < ui.contenuMeta.collections.Size()) {
						ui.contenuMeta.collections.RemoveAt(static_cast<uint32>(ui.collectionMenu));
						if (ui.contenuCollection == ui.collectionMenu) {
							AllerContenu(ui, NkString());
						}
						ui.collectionMenu = -1;
						EcrireMeta(c);
					}
					break;
				case NK_A_CONTENU_RETIRER_COLLECTION:
					if (ui.contenuCollection >= 0 && static_cast<uint32>(ui.contenuCollection) < ui.contenuMeta.collections.Size()) {
						NkVector<NkString> &el = ui.contenuMeta.collections[static_cast<uint32>(ui.contenuCollection)].elements;
						const NkVector<NkString> cibles = Cibles(ui);
						for (uint32 i = 0; i < cibles.Size(); ++i) {
							const NkString rel = Rel(cibles[i]);
							for (uint32 k = 0; k < el.Size(); ++k) {
								if (el[k] == rel) {
									el.RemoveAt(k);
									break;
								}
							}
						}
						ui.contenuChoisis.Clear();
						EcrireMeta(c);
					}
					break;
				default:
					break;
			}
		}

		bool NkEditeurContenuAuClavier(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			const nkgui::NkGuiInput &in = c.ctx.input;
			// Le navigateur a le clavier quand le DERNIER CLIC est tombe dedans.
			if (!ui.voirTiroir || ui.ongletTiroir != 0 || !ui.contenu.focus) {
				return false;
			}
			const bool fichiers = ui.contenuProjet || ui.contenuCollection >= 0;
			struct NkTouche {
					nkgui::NkGuiKey touche;
					int32 action;
			};
			if (in.ctrlDown) {
				static const NkTouche kCtrl[] = {{nkgui::NkGuiKey::C, NK_A_CONTENU_COPIER},
												 {nkgui::NkGuiKey::X, NK_A_CONTENU_COUPER},
												 {nkgui::NkGuiKey::V, NK_A_CONTENU_COLLER},
												 {nkgui::NkGuiKey::D, NK_A_CONTENU_DUPLIQUER},
												 {nkgui::NkGuiKey::A, NK_A_CONTENU_TOUT_SELECTIONNER}};
				for (const NkTouche &t : kCtrl) {
					if (in.KeyPressed(t.touche)) {
						if (fichiers || t.action == NK_A_CONTENU_COLLER) {
							// Le clavier vise la SELECTION, pas un clic droit oublie.
							ui.contenuMenuChemin = NkString();
							ui.contenuMenuDossier = false;
							NkEditeurExecuter(c, t.action);
						}
						return true; // consommee : la scene ne la recoit pas aussi
					}
				}
				return false;
			}
			static const NkTouche kSimples[] = {{nkgui::NkGuiKey::F2, NK_A_CONTENU_RENOMMER},
												{nkgui::NkGuiKey::Delete, NK_A_CONTENU_SUPPRIMER}};
			for (const NkTouche &t : kSimples) {
				if (in.KeyPressed(t.touche)) {
					if (fichiers) {
						ui.contenuMenuChemin = NkString();
						ui.contenuMenuDossier = false;
						NkEditeurExecuter(c, t.action);
					}
					return true;
				}
			}
			return false;
		}

		void NkEditeurDessinerSuppressionContenu(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			if (ui.contenuASupprimer.Empty()) {
				return;
			}
			auto &dl = c.ctx.dlOverlay;
			const nkgui::NkGuiInput &in = c.ctx.input;
			dl.AddRectFilled(ui.ecran, NkColor{0, 0, 0, 110});
			const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f);
			const uint32 n = static_cast<uint32>(ui.contenuASupprimer.Size());
			const uint32 montres = n < 5u ? n : 5u;
			const bool refs = !ui.contenuReferences.Empty();
			const float32 w = 520.f;
			const float32 h = 110.f + static_cast<float32>(montres + (n > montres ? 1u : 0u)) * (lh + 2.f) + (refs ? 2.f * (lh + 2.f) : 0.f);
			const NkRect boite{(ui.ecran.w - w) * 0.5f, (ui.ecran.h - h) * 0.42f, w, h};
			dl.AddRectFilled(NkRect{boite.x + 4.f, boite.y + 6.f, boite.w, boite.h}, NkColor{0, 0, 0, 110}, 3.f);
			dl.AddRectFilled(boite, c.pal.entete, 3.f);
			dl.AddRect(boite, c.pal.bord, 1.f, 3.f);
			dl.AddRectFilled(NkRect{boite.x, boite.y, boite.w, 2.f}, c.pal.selection);
			float32 y = boite.y + 14.f;
			renderer::NkTexte(dl, c.police, boite.x + 16.f, y, NkString::Format("Supprimer %u élément(s) ?", n).CStr(), c.pal.texte);
			y += lh + 8.f;
			for (uint32 i = 0; i < montres; ++i) {
				renderer::NkTexte(dl, c.petite, boite.x + 24.f, y, ui.contenuASupprimer[i].CStr(), c.pal.attenue);
				y += lh + 2.f;
			}
			if (n > montres) {
				renderer::NkTexte(dl, c.petite, boite.x + 24.f, y, NkString::Format("… et %u autre(s)", n - montres).CStr(), c.pal.attenue);
				y += lh + 2.f;
			}
			if (refs) {
				// LES REFERENCES (Unreal les montre avant de supprimer).
				NkString t("Cité par : ");
				for (uint32 i = 0; i < ui.contenuReferences.Size() && i < 4u; ++i) {
					if (i > 0u) {
						t.Append(", ");
					}
					t.Append(editorkit::NkDisqueNom(ui.contenuReferences[i].CStr()));
				}
				renderer::NkTexte(dl, c.petite, boite.x + 16.f, y + 4.f, t.CStr(), c.pal.selection);
				y += 2.f * (lh + 2.f);
			}
			renderer::NkTexte(dl, c.petite, boite.x + 16.f, y + 2.f,
							  ui.contenuCorbeille ? "Les fichiers partent dans la corbeille (récupérables)." : "La suppression est définitive.",
							  c.pal.attenue);
			const float32 bh = 26.f;
			const float32 by = boite.y + boite.h - bh - 12.f;
			const float32 wSup = renderer::NkTexteLargeur(c.police, "Supprimer") + 28.f;
			const float32 wAnn = renderer::NkTexteLargeur(c.police, "Annuler") + 28.f;
			const NkRect rAnn{boite.x + boite.w - 12.f - wAnn, by, wAnn, bh};
			const NkRect rSup{rAnn.x - 8.f - wSup, by, wSup, bh};
			if (NkEditeurBouton(c, rSup, "Supprimer", true, true, &dl) || in.KeyPressed(nkgui::NkGuiKey::Enter)) {
				NkEditeurExecuter(c, NK_A_CONTENU_SUPPRIMER_OUI);
				return;
			}
			if (NkEditeurBouton(c, rAnn, "Annuler", false, true, &dl) || in.KeyPressed(nkgui::NkGuiKey::Escape)) {
				ui.contenuASupprimer.Clear();
				ui.contenuReferences.Clear();
			}
		}

		NkRapportImport NkEditeurImporterIci(NkEditeurModele &m, NkEditeurInterface &ui, const NkVector<NkString> &sources,
											 const char *relatif) {
			// Le dossier COURANT du navigateur quand il montre le Contenu ; sa
			// racine sinon (CONVENTIONS_FICHIERS.md : une destination par defaut,
			// jamais imposee).
			const NkString cible = relatif != nullptr ? NkString(relatif) : (ui.contenuProjet ? ui.contenuDossier : NkString());
			const NkRapportImport r = NkEditeurImporter(m, cible.CStr(), sources);
			// Le resultat se VOIT : le tiroir s'ouvre sur ce dossier, les fichiers
			// crees choisis -- « Exporter » les renverrait tels quels. Sauf si le
			// navigateur est VERROUILLE (Unreal) : il ne suit plus.
			ui.voirTiroir = true;
			ui.ongletTiroir = 0;
			if (!ui.contenu.verrouille) {
				AllerContenu(ui, cible);
				ui.contenuChoisis = r.crees;
				ui.contenuActif = r.crees.Empty() ? NkString() : r.crees[0];
			}
			ui.contenuPerime = true;
			return r;
		}

		NkRapportImport NkEditeurDeposerFichiers(NkEditeurModele &m, NkEditeurInterface &ui, const NkVector<NkString> &sources,
												 float32 x, float32 y) {
			// Sur une carte de DOSSIER du Contenu : dans ce dossier (le geste
			// d'UE5). Les rectangles sont ceux de la derniere trame.
			if (ui.voirTiroir && ui.ongletTiroir == 0 && NkEditeurDans(ui.tiroir, nkgui::NkVec2{x, y})) {
				for (uint32 k = 0; k < ui.contenuCartes.Size() && k < ui.contenu.entries.Size(); ++k) {
					const editorkit::NkAssetEntry &e = ui.contenu.entries[k];
					if (e.isFolder && NkEditeurCheminEstContenu(e.path.CStr()) && NkEditeurDans(ui.contenuCartes[k], nkgui::NkVec2{x, y})) {
						const NkString rel = NkEditeurRelatifContenu(e.path.CStr());
						return NkEditeurImporterIci(m, ui, sources, rel.CStr());
					}
				}
			}
			// Ailleurs : exactement « Importer… ».
			return NkEditeurImporterIci(m, ui, sources, nullptr);
		}

		NkRapportExport NkEditeurExporterChoisis(NkEditeurModele &m, NkEditeurInterface &ui, const char *destination) {
			// (2026-10-01) Les DOSSIERS se choisissent desormais : ils ne s'exportent
			// pas pour autant (un asset, oui) -- on ne les compte pas en echec.
			NkVector<NkString> fichiers;
			for (uint32 i = 0; i < ui.contenuChoisis.Size(); ++i) {
				if (!NkDirectory::Exists(NkEditeurCheminContenu(m, ui.contenuChoisis[i].CStr()).CStr())) {
					fichiers.PushBack(ui.contenuChoisis[i]);
				}
			}
			return NkEditeurExporter(m, fichiers, destination);
		}

		/// `texte` contient-il `motif` (sans tenir compte de la casse ASCII) ?
		bool NkEditeurContientSansCasse(const char *texte, const char *motif) {
			auto bas = [](char ch) { return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch; };
			if (motif == nullptr || motif[0] == '\0') {
				return true;
			}
			for (const char *t = texte; *t != '\0'; ++t) {
				usize k = 0;
				while (motif[k] != '\0' && t[k] != '\0' && bas(t[k]) == bas(motif[k])) {
				++k;
				}
				if (motif[k] == '\0') {
				return true;
				}
			}
			return false;
		}

		void NkEditeurContenuMontrer(NkEditeurInterface &ui, const NkString &nav) {
			// Unreal « Browse to Asset » : meme verrouille, le navigateur y va -- c'est
			// une demande explicite.
			if (!NkEditeurCheminEstContenu(nav.CStr()) || Rel(nav).Empty()) {
				return;
			}
			ui.voirTiroir = true;
			ui.ongletTiroir = 0;
			AllerContenu(ui, editorkit::NkDisqueParent(Rel(nav).CStr()));
			ui.contenuChoisis.Clear();
			ui.contenuChoisis.PushBack(nav);
			ui.contenuActif = nav;
			ui.contenuAncre = nav;
			ui.contenu.focus = true;
		}

		void NkEditeurDessinerTiroir(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			const NkRect zone = ui.tiroir;
			ui.boutonImporter = NkRect{0.f, 0.f, 0.f, 0.f};
			ui.boutonExporter = NkRect{0.f, 0.f, 0.f, 0.f};
			if (!ui.voirTiroir || zone.w < 8.f || zone.h < 40.f) {
				return;
			}
			// (2026-09-30) « Contenu » : le contenu du projet ET le catalogue
			// d'acteurs. L'onglet s'appelait « Acteurs » quand il n'y avait qu'eux.
			// (2026-10-01) « Terminal » : le vrai terminal de la plateforme (NkEditeurTerminal.cpp).
			static const char *kOnglets[3] = {"Contenu", "Journal", "Terminal"};
			const float32 ongletsH = 26.f;
			NkEditeurOnglets(c, NkRect{zone.x, zone.y, zone.w, ongletsH}, kOnglets, 3, ui.ongletTiroir);
			// « Exporter… » a droite des onglets : la touche d'Unkeny (Unreal le range
			// dans le clic droit, ou il est aussi). « Importer » est dans la barre du
			// navigateur (zone 1 d'Unreal).
			if (ui.ongletTiroir == 0) {
				const float32 wE = renderer::NkTexteLargeur(c.petite, "Exporter…") + 16.f;
				ui.boutonExporter = NkRect{zone.x + zone.w - wE - 4.f, zone.y + 3.f, wE, ongletsH - 6.f};
				const bool exportable = !ui.contenuChoisis.Empty();
				// Exporter reste CLIQUABLE sans selection : l'annonce dit quoi faire,
				// un bouton mort ne dit rien.
				if (NkEditeurBouton(c, ui.boutonExporter, "", false)) {
					ui.contenuMenuChemin = NkString();
					ui.contenuMenuDossier = false;
					NkEditeurExecuter(c, NK_A_CONTENU_EXPORTER);
				}
				renderer::NkTexteDansBoite(c.ctx.dl, c.petite, ui.boutonExporter, "Exporter…", exportable ? c.pal.texte : c.pal.attenue);
			}
			const NkRect contenu{zone.x, zone.y + ongletsH, zone.w, zone.h - ongletsH};
			if (ui.ongletTiroir == 0) {
				OngletContenu(c, contenu);
			} else if (ui.ongletTiroir == NK_TIROIR_TERMINAL) {
				NkEditeurDessinerTerminal(c, contenu);
			} else {
				OngletJournal(c, contenu);
			}
		}

	} // namespace editeur
} // namespace nkentseu
