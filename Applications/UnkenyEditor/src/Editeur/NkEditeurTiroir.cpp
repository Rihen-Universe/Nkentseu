//
// NkEditeurTiroir.cpp
// =============================================================================
// Description :
//   Le tiroir de contenu, en bas sur toute la largeur, en deux onglets :
//     Acteurs  le catalogue de simulation d'Unkeny en CARTES, rangees par
//              categorie (NkDrawContentBrowser du kit)
//     Journal  les annonces de l'editeur (enregistre, erreur...), datees
//
// Caracteristiques :
//   - Les categories sont les DOSSIERS du navigateur (le rail de gauche) ET ses
//     puces de filtre ; la barre de couleur d'une carte dit sa categorie.
//   - Clic sur une carte : l'outil « Poser » est arme sur cet acteur, le clic
//     suivant dans la vue le pose. Glisser une carte vers la vue : il est pose
//     au point de depot.
//   - Le composant ne pose rien : il rend ce qui a ete choisi, glisse, lache.
//     C'est l'editeur qui appelle NkEditeurPoser (meme partage que partout).
//   - A LA MANIERE D'UE5 (2026-09-30) : a gauche l'arbre des dossiers (icone
//     de dossier, chevron), a droite leur CONTENU -- a la racine, les dossiers
//     eux-memes en grandes vignettes (double-clic : on y entre) ; le fil
//     d'Ariane au-dessus ; la cloison entre les deux se TIRE ; la recherche
//     cherche dans TOUS les dossiers.
//   - ⚠️ LA RECHERCHE NE MARCHAIT PAS : le kit reserve la boite et en rapporte
//     le rectangle, mais c'est a l'HOTE d'y ecrire (NkComponentInput n'a pas de
//     clavier). Personne n'y ecrivait : la boite prenait le focus, et la frappe
///     partait dans le vide. Le champ du kit (NkOverlayTextField) y est pose.
//   - LE CLIC DROIT (2026-09-30, lot 1) : une carte d'acteur (poser au centre,
//     armer), un dossier (ouvrir), le fond (revenir a la racine). Le kit le
//     rapportait (NkContentBrowserResult::menuIndex) ; l'hote ne le lisait pas.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurInterface.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/Components/NkSilhouettes.h"
#include "NKEditorKit/NkEditorScrollbar.h"
#include "NKEditorKit/NkEditorTextField.h"

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

			void SurSelection(void *user, int32 index, const char *chemin) {
				(void)index;
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				Armer(c.m, ActeurDuChemin(chemin));
			}

			/// La categorie d'un chemin de dossier (son nom), -1 pour la racine.
			int32 CategorieDuChemin(const char *chemin) noexcept {
				for (int32 k = 0; k < NB_CATEGORIES && chemin != nullptr; ++k) {
					if (std::strcmp(chemin, NkCategorieActeurNom(static_cast<NkCategorieActeur>(k))) == 0) {
						return k;
					}
				}
				return -1;
			}

			/// Double-clic sur un DOSSIER de la grille : on y entre (UE5).
			void SurDoubleClic(void *user, int32 index, const char *chemin) {
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				if (index < 0 || index >= static_cast<int32>(c.ui.contenu.entries.Size()) ||
					!c.ui.contenu.entries[static_cast<uint32>(index)].isFolder) {
					return;
				}
				c.ui.categorie = CategorieDuChemin(chemin);
				c.ui.contenu.scroll = 0.f;
			}

			void SurNavigation(void *user, const char *chemin) {
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				c.ui.categorie = -1;
				for (int32 k = 0; k < NB_CATEGORIES && chemin != nullptr; ++k) {
					if (std::strcmp(chemin, NkCategorieActeurNom(static_cast<NkCategorieActeur>(k))) == 0) {
						c.ui.categorie = k;
					}
				}
				c.ui.contenu.scroll = 0.f;
			}

			/// La pastille de couleur de l'acteur, sur sa carte : c'est la couleur
			/// qu'il aura dans la scene, ce qu'une icone generique ne dit pas.
			void SurCarte(void *user, editorkit::NkComponentPaint &p, int32 index, float32 x, float32 y, float32 w,
						  float32 h) {
				NkEditeurCadre &c = *static_cast<NkEditeurCadre *>(user);
				// Le rectangle de la carte, dossier compris : le composant est le seul
				// a connaitre sa grille (NkEditeurInterface::contenuCartes).
				if (index >= 0 && index < static_cast<int32>(c.ui.contenuCartes.Size())) {
					c.ui.contenuCartes[static_cast<uint32>(index)] = NkRect{x, y, w, h};
				}
				if (index < 0 || index >= static_cast<int32>(c.ui.contenu.entries.Size()) ||
					c.ui.contenu.entries[static_cast<uint32>(index)].isFolder) {
					return; // un dossier a sa silhouette, pas de pastille
				}
				const int32 a = ActeurDuChemin(c.ui.contenu.entries[static_cast<uint32>(index)].path.CStr());
				uint32 couleur = 0xB0B0B0FFu;
				if (a >= 0) {
					couleur = NkActeurSimInfo(static_cast<NkActeurSim>(a)).couleur;
				}
				const float32 cote = (w < h ? w : h) * 0.30f;
				const editorkit::NkPaintRect r{x + (w - cote) * 0.5f, y + h * 0.30f - cote * 0.5f, cote, cote};
				p.FillColor(r, couleur | 0xFFu, cote * 0.5f);
			}

			void PreparerReglages(NkEditeurInterface &ui) {
				if (ui.contenuPret) {
					return;
				}
				ui.contenuPret = true;
				ui.contenuReglages.Bind(editorkit::NkContentBrowserDecl());
				// La tete du panneau est l'onglet « Acteurs » ; les boutons
				// Creer / Importer / Tout enregistrer n'ont pas de sens pour un
				// catalogue fixe : ils seraient des decors.
				// La bande de TITRE (« Contenu ») repeterait l'onglet « Acteurs » ; le
				// fil d'Ariane et la recherche ont leur propre rangee, gardee.
				ui.contenuReglages.SetParam("show_header", 0.f);
				ui.contenuReglages.SetParam("show_actions", 0.f);
				ui.contenuReglages.SetParam("show_select_all", 0.f);
				ui.contenuReglages.SetParam("show_sort", 0.f);
				ui.contenuReglages.SetParam("multi_select", 0.f);
				// Sa barre d'etat (« Aucune selection » / « N acteurs ») repetait la
				// rangee d'information juste au-dessus, et prenait aux cartes une
				// hauteur qu'un tiroir de bas d'ecran n'a pas.
				ui.contenuReglages.SetParam("show_status", 0.f);
				// Le badge dit le FORMAT d'un fichier ; un acteur n'en a pas, et sa
				// categorie est deja dans le pied de carte. Allume, il la recopiait
				// sur la vignette -- seulement quand elle tenait (« Eau », pas
				// « Corps rigides »), d'ou des cartes qui ne se ressemblaient pas.
				ui.contenuReglages.SetParam("show_badge", 0.f);
				ui.contenuReglages.SetParam("tree_width", 0.15f);
				ui.contenu.thumbSize = 60.f;
				for (int32 k = 0; k < NB_CATEGORIES; ++k) {
					editorkit::NkBrowserKind kind;
					kind.label = NkString(NkCategorieActeurNom(static_cast<NkCategorieActeur>(k)));
					kind.role = static_cast<uint16>(RoleCategorie(k));
					ui.contenu.kinds.PushBack(kind);
				}
			}

			void Reconstruire(NkEditeurCadre &c) {
				NkEditeurInterface &ui = c.ui;
				editorkit::NkContentBrowserModel &m = ui.contenu;

				// ── Les dossiers : la racine, puis une categorie par dossier ─
				m.folders.nodes.Clear();
				editorkit::NkTreeNode racine;
				racine.id = 1u;
				racine.parent = -1;
				racine.label = NkString(CHEMIN_RACINE);
				racine.path = NkString(CHEMIN_RACINE);
				racine.silhouette = static_cast<uint8>(editorkit::NkAssetIcone::Dossier);
				m.folders.nodes.PushBack(racine);
				for (int32 k = 0; k < NB_CATEGORIES; ++k) {
					editorkit::NkTreeNode n;
					n.id = static_cast<nk_uint64>(k) + 2u;
					n.parent = 0;
					n.label = NkString(NkCategorieActeurNom(static_cast<NkCategorieActeur>(k)));
					n.path = n.label;
					n.kindRole = static_cast<uint16>(RoleCategorie(k));
					n.silhouette = static_cast<uint8>(editorkit::NkAssetIcone::Dossier);
					m.folders.nodes.PushBack(n);
				}
				m.folders.active = ui.categorie >= 0 ? static_cast<nk_uint64>(ui.categorie) + 2u : 1u;
				m.folders.chosen.Clear();
				m.folders.chosen.PushBack(m.folders.active);
				m.breadcrumb.Clear();
				m.breadcrumb.PushBack(NkString(CHEMIN_RACINE));
				if (ui.categorie >= 0) {
					m.breadcrumb.PushBack(NkString(NkCategorieActeurNom(static_cast<NkCategorieActeur>(ui.categorie))));
				}

				// ── Les cartes ──────────────────────────────────────────────
				// A la RACINE : les dossiers en vignettes, puis l'entite simple
				// (elle n'est d'aucune categorie). Dans un dossier : ses acteurs.
				// Une RECHERCHE en cours : tous les acteurs, de tous les dossiers --
				// chercher « eau » depuis la racine doit trouver l'eau.
				m.entries.Clear();
				int32 arme = -1;
				const bool cherche = m.filter[0] != '\0';
				if (ui.categorie < 0 && !cherche) {
					for (int32 k = 0; k < NB_CATEGORIES; ++k) {
						editorkit::NkAssetEntry d;
						d.name = NkString(NkCategorieActeurNom(static_cast<NkCategorieActeur>(k)));
						d.path = d.name;
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
					// La racine sans recherche ne montre que ses dossiers ;
					// un dossier, que les siens.
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
				m.chosen.Clear();
				if (arme >= 0) {
					m.chosen.PushBack(arme);
				}
				m.statusRight = NkString::Format("%u élément(s)", static_cast<uint32>(m.entries.Size()));
				// Les rectangles des cartes : remis a zero, le dessin les releve.
				ui.contenuCartes.Clear();
				ui.contenuCartes.Resize(m.entries.Size());
				for (uint32 i = 0; i < ui.contenuCartes.Size(); ++i) {
					ui.contenuCartes[i] = NkRect{0.f, 0.f, 0.f, 0.f};
				}
			}

			void OngletActeurs(NkEditeurCadre &c, const NkRect &zone) {
				NkEditeurInterface &ui = c.ui;
				PreparerReglages(ui);
				Reconstruire(c);

				editorkit::NkContentBrowserStyle s;
				s.values = &ui.contenuReglages;
				s.panelBg = static_cast<uint16>(NkRole::PanelBg);
				s.headerBg = static_cast<uint16>(NkRole::PanelHeader);
				s.border = static_cast<uint16>(NkRole::Border);
				s.text = static_cast<uint16>(NkRole::Text);
				s.textMuted = static_cast<uint16>(NkRole::TextMuted);
				s.cardBg = static_cast<uint16>(NkRole::InputBg);
				s.cardFooterBg = static_cast<uint16>(NkRole::PanelHeader);
				s.activeMark = static_cast<uint16>(NkRole::AccentUi);
				s.chosenMark = static_cast<uint16>(NkRole::AccentUi);
				s.folderTint = static_cast<uint16>(NkRole::TypeFolder);
				s.chipBg = static_cast<uint16>(NkRole::InputBg);
				s.badgeText = static_cast<uint16>(NkRole::PanelBg);
				s.statusBg = static_cast<uint16>(NkRole::PanelHeader);

				editorkit::NkContentBrowserHooks hooks;
				hooks.user = &c;
				hooks.onSelect = &SurSelection;
				hooks.onNavigate = &SurNavigation;
				hooks.onDoubleClick = &SurDoubleClic;
				hooks.cardOverlay = &SurCarte;

				const editorkit::NkComponentInput ci = NkEditeurEntreeComposant(c.ctx);
				editorkit::NkGuiComponentPaint peintre(c.ctx, c.theme);
				const editorkit::NkContentBrowserResult res = editorkit::NkDrawContentBrowser(
					peintre, ci, editorkit::NkPaintRect{zone.x, zone.y, zone.w, zone.h}, ui.contenu, s, hooks);

				if (res.defilContenu > res.defilVue && res.defilW > 0.f && res.defilH > 0.f) {
					editorkit::NkVScrollbar(c.ctx, c.ctx.dl, NkRect{res.defilX, res.defilY, res.defilW, res.defilH},
											ui.contenu.scroll, res.defilContenu, res.defilVue, c.ctx.GetId("tiroir.defil"),
											res.defilPas);
				}

				// ── Le clic DROIT (2026-09-30, lot 1) ────────────────────────────
				// ⚠️ LE KIT LE RAPPORTAIT, PERSONNE NE L'ECOUTAIT : ni `onContextMenu`
				//    ni `menuIndex` n'etaient lus, et le clic droit du navigateur « ne
				//    faisait rien ». On lit le RESULTAT plutot que le crochet : il
				//    couvre aussi le rail des dossiers (`menuCheminRail`), que le
				//    crochet ne voit pas. -2 = aucun clic droit a cette trame.
				if (res.menuIndex != -2) {
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

				const nkgui::NkGuiInput &in = c.ctx.input;
				// ── La recherche : le composant a pose le focus, l'hote a le clavier ─
				if (res.rechercheW > 0.f && ui.contenu.searchFocused) {
					if (in.KeyPressed(nkgui::NkGuiKey::Escape) || in.KeyPressed(nkgui::NkGuiKey::Enter)) {
						ui.contenu.searchFocused = false;
					}
					editorkit::NkOverlayFieldStyle st;
					st.fond = false;
					st.bord = false;
					st.texte = c.pal.texte;
					const NkRect champ{res.rechercheX + 4.f, res.rechercheY, res.rechercheW - 6.f, res.rechercheH};
					// Le fond de la boite, par-dessus le texte que le kit y a peint :
					// c'est le champ, desormais, qui l'affiche avec son curseur.
					c.ctx.dl.AddRectFilled(NkRect{champ.x + 1.f, champ.y + 1.f, champ.w - 2.f, champ.h - 2.f}, c.pal.champ);
					editorkit::NkOverlayTextField(c.ctx, c.ctx.dl, c.police, champ, ui.contenu.filter,
												  static_cast<int32>(sizeof(ui.contenu.filter)), ui.contenu.searchFocused, &st);
				}
				// ── La CLOISON dossiers | cartes, qui se tire (UE5) ───────────────
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
				if (!res.glisserChemin.Empty()) {
					if (in.mouseReleased[0]) {
						// LE DEPOT DANS LA VUE : le composant l'a vu lache « dans le
						// vide » (hors de ses volets) ; pour l'editeur, c'est une pose.
						// Un DOSSIER lache dans la vue ne pose rien (il poserait
						// l'acteur arme avant lui, sans qu'on l'ait demande).
						const int32 acteur = ActeurDuChemin(res.glisserChemin.CStr());
						if (NkEditeurDans(ui.viseur, in.mousePos) && acteur != -1) {
							Armer(c.m, acteur);
							const NkVec2f monde = c.m.scene.Camera().EcranVersMonde(NkVec2f(in.mousePos.x, in.mousePos.y));
							NkEditeurPoser(c.m, monde);
						}
					} else {
						// Le fantome : ce qu'on emporte, sous le curseur.
						const char *lib = res.glisserLibelle.Empty() ? res.glisserChemin.CStr() : res.glisserLibelle.CStr();
						const float32 w = renderer::NkTexteLargeur(c.police, lib) + 16.f;
						const NkRect g{in.mousePos.x + 12.f, in.mousePos.y + 10.f, w, 22.f};
						over.AddRectFilled(g, c.pal.entete, 2.f);
						over.AddRect(g, c.pal.accent, 1.f, 2.f);
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
			}

			void OngletJournal(NkEditeurCadre &c, const NkRect &zone) {
				NkEditeurInterface &ui = c.ui;
				auto &dl = c.ctx.dl;
				dl.AddRectFilled(zone, c.pal.panneau);
				if (ui.journal.Empty()) {
					renderer::NkTexteCentre(dl, c.police, zone.x + zone.w * 0.5f, zone.y + zone.h * 0.4f,
											"Le journal est vide : les annonces de l'éditeur s'y inscrivent.",
											c.pal.attenue);
					return;
				}
				// Le plus RECENT en haut : c'est lui qu'on vient chercher, et il ne
				// doit pas falloir defiler pour le lire.
				const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f) + 3.f;
				dl.PushClipRect(zone, true);
				float32 y = zone.y + 6.f;
				for (int32 i = static_cast<int32>(ui.journal.Size()) - 1; i >= 0 && y < zone.y + zone.h; --i) {
					renderer::NkTexte(dl, c.police, zone.x + 10.f, y, ui.journal[static_cast<uint32>(i)].CStr(),
									  i + 1 == static_cast<int32>(ui.journal.Size()) ? c.pal.texte : c.pal.attenue);
					y += lh;
				}
				dl.PopClipRect();
			}

		} // namespace

		void NkEditeurActionContenu(NkEditeurCadre &c, int32 action) {
			NkEditeurModele &m = c.m;
			NkEditeurInterface &ui = c.ui;
			const char *chemin = ui.contenuMenuChemin.CStr();
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
					// La racine (« Acteurs ») est un dossier comme un autre.
					ui.categorie = CategorieDuChemin(chemin);
					ui.contenu.scroll = 0.f;
					break;
				case NK_A_CONTENU_RACINE:
					ui.categorie = -1;
					ui.contenu.scroll = 0.f;
					break;
				default:
					break;
			}
		}

		void NkEditeurDessinerTiroir(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			const NkRect zone = ui.tiroir;
			if (!ui.voirTiroir || zone.w < 8.f || zone.h < 40.f) {
				return;
			}
			static const char *kOnglets[2] = {"Acteurs", "Journal"};
			const float32 ongletsH = 26.f;
			NkEditeurOnglets(c, NkRect{zone.x, zone.y, zone.w, ongletsH}, kOnglets, 2, ui.ongletTiroir);
			const NkRect contenu{zone.x, zone.y + ongletsH, zone.w, zone.h - ongletsH};
			if (ui.ongletTiroir == 0) {
				OngletActeurs(c, contenu);
			} else {
				OngletJournal(c, contenu);
			}
		}

	} // namespace editeur
} // namespace nkentseu
