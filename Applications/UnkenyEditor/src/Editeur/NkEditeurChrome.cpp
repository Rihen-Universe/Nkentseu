//
// NkEditeurChrome.cpp
// =============================================================================
// Description :
//   Le chrome de l'editeur : barre de menus et onglet de scene, barre d'outils,
//   barre d'etat, cloisons entre colonnes, menus deroulants. Et la TABLE
//   D'ACTIONS, par laquelle passent menus, boutons et raccourcis.
//
// Caracteristiques :
//   - Aucune couleur en dur : tout vient de NkPaletteEditeur, donc du theme.
//   - Boutons PLATS, coins de 2 px, rangees denses (UI_SPEC §3.3).
//   - Les glyphes de lecture (▶ ⏸ ⏹ ⏭) sont DESSINES : la police embarquee
//     ne les porte pas, et un carre vide a la place de « Jouer » ne se
//     remarque qu'une fois le logiciel livre.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurInterface.h"
#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "Editeur/NkEditeurLumiere.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/NkThemeToGui.h"
#include "Unkeny/Livraison/NkUnkenyLivraison.h"

namespace nkentseu {
	namespace editeur {

		using editorkit::NkRole;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {

			constexpr float32 HAUTEUR_MENUS = 30.f;   ///< la barre de titre, menus compris
			constexpr float32 HAUTEUR_ONGLETS = 26.f; ///< les onglets de scene, dessous
			constexpr float32 HAUTEUR_OUTILS = 34.f;
			constexpr float32 HAUTEUR_STATUT = 22.f;
			constexpr float32 HAUTEUR_BARRE_VUE = 28.f;
			constexpr float32 EPAISSEUR_CLOISON = 4.f;

			float32 Borne(float32 v, float32 lo, float32 hi) noexcept {
				if (hi < lo) {
					return lo;
				}
				if (v < lo) {
					return lo;
				}
				if (v > hi) {
					return hi;
				}
				return v;
			}

			const char *NomOutil(NkOutil o) noexcept {
				switch (o) {
					case NkOutil::NK_POSER:   return "Poser";
					case NkOutil::NK_EFFACER: return "Effacer";
					case NkOutil::NK_SAISIR:  return "Saisir";
					case NkOutil::NK_COUTEAU: return "Couteau";
					case NkOutil::NK_DEPLACER: return "Déplacer";
					case NkOutil::NK_TOURNER: return "Tourner";
					case NkOutil::NK_ECHELLE: return "Échelle";
					default:                  return "Sélection";
				}
			}

			/// Le nom de la scene pour l'onglet : le fichier, sans son dossier.
			NkString NomScene(const NkEditeurModele &m) {
				if (m.chemin.Empty()) {
					return NkString("Scene_01");
				}
				const char *s = m.chemin.CStr();
				const char *nom = s;
				for (const char *p = s; *p != '\0'; ++p) {
					if (*p == '/' || *p == '\\') {
						nom = p + 1;
					}
				}
				return NkString(nom);
			}

			/// Un petit triangle pointe en bas : la marque d'un bouton deroulant.
			void Chevron(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, const NkColor &c) {
				dl.AddTriangleFilled(NkVec2{cx - 3.5f, cy - 1.5f}, NkVec2{cx + 3.5f, cy - 1.5f}, NkVec2{cx, cy + 2.5f}, c);
			}

			/// Une coche, tracee (deux traits) : meme raison que les glyphes de lecture.
			void Coche(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, const NkColor &c) {
				dl.AddLine(NkVec2{cx - 4.f, cy}, NkVec2{cx - 1.f, cy + 3.f}, c, 1.6f);
				dl.AddLine(NkVec2{cx - 1.f, cy + 3.f}, NkVec2{cx + 4.5f, cy - 3.5f}, c, 1.6f);
			}

			/// Un separateur vertical de groupe dans la barre d'outils.
			float32 Trait(NkEditeurCadre &c, float32 x) {
				const NkRect &b = c.ui.barreOutils;
				c.ctx.dl.AddRectFilled(NkRect{x + 6.f, b.y + 7.f, 1.f, b.h - 14.f}, c.pal.bord);
				return x + 13.f;
			}

			/// Un bouton de la barre d'outils, a la largeur de son libelle.
			/// `deroulant` : il ouvre `menu` sous lui.
			/// `largeurMin` : la place reservee quand le libelle VARIE (« Outil :
			/// Selection » / « Outil : Poser »). Sans elle, changer d'outil
			/// decalait toute la barre, et le bouton vise la seconde d'avant
			/// n'etait plus sous le curseur (mesure du 2026-09-29).
			float32 BoutonOutil(NkEditeurCadre &c, float32 x, const char *texte, NkMenuEditeur menu, int32 action,
								float32 largeurMin = 0.f) {
				const NkRect &b = c.ui.barreOutils;
				const bool deroulant = menu != NkMenuEditeur::NK_AUCUN;
				float32 w = renderer::NkTexteLargeur(c.police, texte) + (deroulant ? 30.f : 20.f);
				w = w < largeurMin ? largeurMin : w;
				const NkRect r{x, b.y + 4.f, w, b.h - 8.f};
				const bool ouvert = deroulant && c.ui.menu == menu;
				const NkRect texteR{r.x, r.y, deroulant ? r.w - 10.f : r.w, r.h};
				const bool clic = NkEditeurBouton(c, r, "", ouvert);
				renderer::NkTexteDansBoite(c.ctx.dl, c.police, texteR, texte, ouvert ? c.pal.surAccent : c.pal.texte);
				if (deroulant) {
					Chevron(c.ctx.dl, r.x + r.w - 11.f, r.y + r.h * 0.5f, ouvert ? c.pal.surAccent : c.pal.attenue);
				}
				if (clic) {
					if (deroulant) {
						NkEditeurOuvrirMenu(c, menu, r);
					} else {
						NkEditeurExecuter(c, action);
					}
				}
				return x + w + 4.f;
			}

			/// Les quatre boutons de lecture, glyphes dessines.
			float32 BoutonsLecture(NkEditeurCadre &c, float32 x) {
				const NkRect &b = c.ui.barreOutils;
				const float32 cote = b.h - 8.f;
				const NkEtatJeu etat = c.m.etat;
				auto &dl = c.ctx.dl;
				for (int32 k = 0; k < 4; ++k) {
					const NkRect r{x, b.y + 4.f, cote, cote};
					const bool enfonce = (k == 0 && etat == NkEtatJeu::NK_JEU) || (k == 1 && etat == NkEtatJeu::NK_PAUSE);
					const bool actif = !(k == 2 && etat == NkEtatJeu::NK_EDITION);
					const bool clic = NkEditeurBouton(c, r, "", enfonce, actif);
					const NkColor g = enfonce ? c.pal.surAccent : (actif ? c.pal.texte : c.pal.attenue);
					const float32 cx = r.x + r.w * 0.5f;
					const float32 cy = r.y + r.h * 0.5f;
					switch (k) {
						case 0: // Jouer : un triangle
							dl.AddTriangleFilled(NkVec2{cx - 4.f, cy - 6.f}, NkVec2{cx - 4.f, cy + 6.f}, NkVec2{cx + 6.f, cy}, g);
							break;
						case 1: // Pause : deux barres
							dl.AddRectFilled(NkRect{cx - 5.f, cy - 6.f, 3.5f, 12.f}, g);
							dl.AddRectFilled(NkRect{cx + 1.5f, cy - 6.f, 3.5f, 12.f}, g);
							break;
						case 2: // Arreter : un carre
							dl.AddRectFilled(NkRect{cx - 5.f, cy - 5.f, 10.f, 10.f}, g);
							break;
						default: // Un pas : triangle + barre
							dl.AddTriangleFilled(NkVec2{cx - 5.f, cy - 6.f}, NkVec2{cx - 5.f, cy + 6.f}, NkVec2{cx + 3.f, cy}, g);
							dl.AddRectFilled(NkRect{cx + 3.5f, cy - 6.f, 2.5f, 12.f}, g);
							break;
					}
					if (clic) {
						static const int32 kActions[4] = {NK_A_JOUER, NK_A_PAUSE, NK_A_ARRETER, NK_A_PAS};
						NkEditeurExecuter(c, kActions[k]);
					}
					x += cote + 3.f;
				}
				return x + 2.f;
			}

			NkEntreeMenu Entree(const char *libelle, int32 action, const char *raccourci = "", bool coche = false,
								bool actif = true) {
				NkEntreeMenu e;
				e.libelle = NkString(libelle);
				e.action = action;
				e.raccourci = raccourci;
				e.coche = coche;
				e.actif = actif;
				return e;
			}

			NkEntreeMenu Separateur() {
				NkEntreeMenu e;
				e.separateur = true;
				return e;
			}

			/// Un intitule de groupe : ni cliquable ni coche, en attenue.
			NkEntreeMenu Intitule(const char *libelle) {
				NkEntreeMenu e;
				e.libelle = NkString(libelle);
				e.actif = false;
				return e;
			}

			/// Une entree qui ouvre un sous-menu a sa droite (« Ajouter ▸ »).
			NkEntreeMenu SousMenu(const char *libelle, NkMenuEditeur sous) {
				NkEntreeMenu e;
				e.libelle = NkString(libelle);
				e.sousMenu = sous;
				return e;
			}

			/// Le catalogue des acteurs, rubrique par rubrique, chaque entree
			/// portant `base + NkActeurSim`. Partage par « + Ajouter » (pose au
			/// centre de la vue) et « Ajouter ici » (pose au point du clic droit).
			void EntreesCatalogue(NkVector<NkEntreeMenu> &out, int32 base) {
				for (int32 k = 0; k < static_cast<int32>(NkCategorieActeur::NK_COUNT); ++k) {
					const NkCategorieActeur cat = static_cast<NkCategorieActeur>(k);
					out.PushBack(Separateur());
					out.PushBack(Intitule(NkCategorieActeurNom(cat)));
					for (int32 i = 0; i < static_cast<int32>(NkActeurSim::NK_COUNT); ++i) {
						const NkInfoActeurSim &info = NkActeurSimInfo(static_cast<NkActeurSim>(i));
						if (info.categorie == cat) {
							out.PushBack(Entree(info.nom, base + i));
						}
					}
				}
			}

			/// Le contenu d'un menu, relu A CHAQUE TRAME : une coche ou une entree
			/// grisee suit l'etat sans qu'aucun code n'ait a la remettre a jour.
			void RemplirMenu(NkEditeurCadre &c, NkMenuEditeur menu, NkVector<NkEntreeMenu> &out) {
				NkEditeurModele &m = c.m;
				out.Clear();
				switch (menu) {
					case NkMenuEditeur::NK_FICHIER:
						out.PushBack(Entree("Nouvelle scène", NK_A_NOUVEAU, "Ctrl+N"));
						out.PushBack(Entree("Ouvrir", NK_A_OUVRIR, "Ctrl+O"));
						out.PushBack(Entree("Enregistrer", NK_A_ENREGISTRER, "Ctrl+S"));
						out.PushBack(Entree("Fermer la scène", NK_A_FERMER_SCENE));
						out.PushBack(Separateur());
						// (2026-09-30) Le Contenu du projet : les memes que la barre du navigateur.
						out.PushBack(Entree("Importer…", NK_A_CONTENU_IMPORTER));
						out.PushBack(Entree("Exporter la sélection…", NK_A_CONTENU_EXPORTER, "", false, !c.ui.contenuChoisis.Empty()));
						out.PushBack(Separateur());
						out.PushBack(Entree("Construire…", NK_A_CONSTRUIRE));
						out.PushBack(Separateur());
						out.PushBack(Entree("Quitter", NK_A_QUITTER, "Ctrl+Q"));
						break;
					case NkMenuEditeur::NK_EDITION:
						out.PushBack(Entree("Annuler", NK_A_ANNULER, "Ctrl+Z", false, !m.historique.annuler.Empty()));
						out.PushBack(Entree("Rétablir", NK_A_REFAIRE, "Ctrl+Y", false, !m.historique.refaire.Empty()));
						out.PushBack(Separateur());
						out.PushBack(Entree("Nouvelle entité", NK_A_NOUVELLE_ENTITE, "Ctrl+E"));
						out.PushBack(Entree("Dupliquer", NK_A_DUPLIQUER, "Ctrl+D", false, m.aSelection));
						out.PushBack(Entree("Supprimer", NK_A_SUPPRIMER, "Suppr", false, m.aSelection));
						out.PushBack(Entree("Renommer", NK_A_RENOMMER, "F2", false, m.aSelection));
						out.PushBack(Separateur());
						out.PushBack(Entree("Cadrer la sélection", NK_A_CADRER_SELECTION, "F"));
						out.PushBack(Entree("Cadrer tout", NK_A_CADRER));
						break;
					case NkMenuEditeur::NK_FENETRE:
						out.PushBack(Entree("Outliner", NK_A_VOIR_OUTLINER, "", c.ui.voirOutliner));
						out.PushBack(Entree("Détails", NK_A_VOIR_DETAILS, "", c.ui.voirDetails));
						out.PushBack(Entree("Tiroir de contenu", NK_A_VOIR_TIROIR, "", c.ui.voirTiroir));
						out.PushBack(Entree("Entrées du jeu", NK_A_ENTREES, "", c.ui.panneauEntrees));
						out.PushBack(Separateur());
						out.PushBack(Entree("Disposition par défaut", NK_A_DISPOSITION));
						break;
					case NkMenuEditeur::NK_AIDE:
						out.PushBack(Entree("Raccourcis clavier", NK_A_RACCOURCIS));
						out.PushBack(Entree("À propos d'UnkenyEditor", NK_A_APROPOS));
						break;
					case NkMenuEditeur::NK_OUTIL: {
						// La selection et ses trois gizmos d'abord, avec les touches
						// d'UE5 ; puis les outils de la matiere.
						struct NkLigneOutil {
								NkOutil outil;
								const char *touche;
						};
						static const NkLigneOutil kLignes[4] = {
							{NkOutil::NK_SELECTION, "Q"},
							{NkOutil::NK_DEPLACER, "W"},
							{NkOutil::NK_TOURNER, "E"},
							{NkOutil::NK_ECHELLE, "R"},
						};
						for (int32 k = 0; k < 4; ++k) {
							const NkOutil o = kLignes[k].outil;
							out.PushBack(Entree(NomOutil(o), NK_A_OUTIL + static_cast<int32>(o), kLignes[k].touche, m.outil == o));
						}
						out.PushBack(Separateur());
						for (int32 k = static_cast<int32>(NkOutil::NK_POSER); k <= static_cast<int32>(NkOutil::NK_COUTEAU); ++k) {
							const NkOutil o = static_cast<NkOutil>(k);
							out.PushBack(Entree(NomOutil(o), NK_A_OUTIL + k, "", m.outil == o));
						}
						out.PushBack(Separateur());
						out.PushBack(Entree("Accrochage (Ctrl l'inverse)", NK_A_ACCROCHAGE, "",
											c.ui.accrocheGrille || c.ui.accrocheAngle || c.ui.accrocheEchelle));
						break;
					}
					case NkMenuEditeur::NK_AJOUTER:
						out.PushBack(Entree("Entité vide", NK_A_NOUVELLE_ENTITE, "Ctrl+E"));
						out.PushBack(Entree("Entité simple (sprite + boîte) : poser", NK_A_ARMER_SIMPLE));
						// (2026-10-01) Un element d'interface ancre dans la zone sure.
						out.PushBack(Entree("Élément d'interface (HUD ancré)",
											NK_A_OPTION_APPAREIL + static_cast<int32>(NkOptionAppareil::NK_AJOUTER_HUD), "",
											false, m.etat == NkEtatJeu::NK_EDITION));
						EntreesCatalogue(out, NK_A_POSER_ACTEUR);
						break;
					case NkMenuEditeur::NK_CTX_ENTITE: {
						// Le clic droit a CHOISI l'entite : ces actions la visent.
						NkString nom("(entité)");
						if (m.aSelection) {
							if (const NkEtiquette *e = m.scene.Monde().Get<NkEtiquette>(m.selection)) {
								nom = NkString(e->nom);
							}
						}
						out.PushBack(Intitule(nom.CStr()));
						out.PushBack(Entree("Renommer", NK_A_RENOMMER, "F2"));
						out.PushBack(Entree("Dupliquer", NK_A_DUPLIQUER, "Ctrl+D"));
						out.PushBack(Entree("Supprimer", NK_A_SUPPRIMER, "Suppr"));
						out.PushBack(Entree("Cadrer", NK_A_CADRER_SELECTION, "F"));
						out.PushBack(Separateur());
						out.PushBack(SousMenu("Ajouter un composant", NkMenuEditeur::NK_COMPOSANT));
						// (2026-09-29) La hierarchie et les prefabs.
						out.PushBack(Separateur());
						out.PushBack(Entree("Créer un prefab", NK_A_CREER_PREFAB, "", false,
											m.aSelection && m.etat == NkEtatJeu::NK_EDITION));
						out.PushBack(Entree("Détacher du parent", NK_A_DETACHER, "", false,
											m.aSelection && m.scene.Parent(m.selection).IsValid()));
						out.PushBack(Entree("Ancrer à l'écran (HUD)",
											NK_A_OPTION_APPAREIL + static_cast<int32>(NkOptionAppareil::NK_ANCRER_SELECTION), "",
											false,
											m.aSelection && m.etat == NkEtatJeu::NK_EDITION &&
												!m.scene.Monde().Has<NkAncrageEcran2D>(m.selection)));
						break;
					}
					case NkMenuEditeur::NK_CTX_VIDE:
						out.PushBack(SousMenu("Ajouter ici", NkMenuEditeur::NK_AJOUTER_ICI));
						out.PushBack(Separateur());
						out.PushBack(Entree("Cadrer tout", NK_A_CADRER, "F"));
						break;
					case NkMenuEditeur::NK_AJOUTER_ICI:
						out.PushBack(Entree("Entité vide", NK_A_ENTITE_ICI));
						out.PushBack(Entree("Entité simple (sprite + boîte)", NK_A_SIMPLE_ICI));
						EntreesCatalogue(out, NK_A_POSER_ICI);
						// 2026-09-30 : lumieres et effets, au point du clic droit.
						out.PushBack(Separateur());
						out.PushBack(Intitule("Lumière et effets"));
						out.PushBack(Entree("Lumière ponctuelle ici", NK_A_LUMIERE_ICI + static_cast<int32>(NkTypeLumiere2D::NK_PONCTUELLE)));
						out.PushBack(Entree("Projecteur (cône) ici", NK_A_LUMIERE_ICI + static_cast<int32>(NkTypeLumiere2D::NK_SPOT)));
						for (int32 p = 1; p < static_cast<int32>(NkPresetEffet2D::NK_COUNT); ++p) {
							out.PushBack(Entree(NkString::Format("%s ici", NkNomPresetEffet2D(static_cast<NkPresetEffet2D>(p))).CStr(),
												NK_A_EMETTEUR_ICI + p));
						}
						break;
					case NkMenuEditeur::NK_APPAREIL: {
						// (2026-10-01) Le catalogue PAR FAMILLE : 21 appareils en vrac ne
						// se parcourent pas. Les indices, eux, ne bougent pas.
						struct NkGroupe {
								const char *titre;
								NkFamilleAppareil a;
								NkFamilleAppareil b;
						};
						static const NkGroupe kGroupes[] = {
							{"Téléphones", NkFamilleAppareil::NK_TELEPHONE, NkFamilleAppareil::NK_TELEPHONE},
							{"Tablettes et pliables", NkFamilleAppareil::NK_TABLETTE, NkFamilleAppareil::NK_PLIABLE},
							{"Bureau, navigateur, TV", NkFamilleAppareil::NK_BUREAU, NkFamilleAppareil::NK_TV},
							{"Consoles, montre", NkFamilleAppareil::NK_CONSOLE, NkFamilleAppareil::NK_MONTRE},
						};
						for (const NkGroupe &g : kGroupes) {
							out.PushBack(Intitule(g.titre));
							for (int32 k = 0; k < NkNbProfils(); ++k) {
								const NkFamilleAppareil f = NkProfil(k).famille;
								const bool dedans = (f == g.a || f == g.b) ||
													(g.a == NkFamilleAppareil::NK_BUREAU && f == NkFamilleAppareil::NK_NAVIGATEUR);
								if (dedans) {
									out.PushBack(Entree(NkProfil(k).nom, NK_A_APPAREIL + k, "", m.profil == k));
								}
							}
						}
						out.PushBack(Separateur());
						out.PushBack(Entree("Personnalisé", NK_A_APPAREIL + NkNbProfils(), "", m.ProfilPersonnalise()));
						out.PushBack(Entree("Personnaliser cet appareil",
											NK_A_OPTION_APPAREIL + static_cast<int32>(NkOptionAppareil::NK_PERSONNALISER), "",
											false, !m.ProfilPersonnalise()));
						out.PushBack(Separateur());
						// (2026-10-01) Les QUATRE orientations : le sens du paysage
						// change le cote de la decoupe (document 03, §2.3).
						{
							const NkProfilAppareil base = NkProfil(m.profil);
							const bool naturelPaysage = base.largeur > base.hauteur;
							for (int32 k = 0; k < static_cast<int32>(NkOrientation::NK_COUNT); ++k) {
								const NkOrientation o = static_cast<NkOrientation>(k);
								out.PushBack(Entree(NkNomOrientation(o, naturelPaysage), NK_A_ORIENTATION + k, "",
													m.orientation == o));
							}
						}
						out.PushBack(Separateur());
						{
							static const char *kOptions[5] = {"Cadre", "Zone sûre", "Découpe de caméra", "Cadre clair",
															  "Aperçu de la caméra du jeu"};
							const bool etats[5] = {m.appareil.voirCadre, m.appareil.voirZoneSure, m.appareil.voirDecoupe,
												   m.appareil.cadreClair, m.appareil.apercuJeu};
							for (int32 k = 0; k < 5; ++k) {
								out.PushBack(Entree(kOptions[k], NK_A_OPTION_APPAREIL + k, "", etats[k]));
							}
						}
						// (2026-10-01) La camera du JEU sur un autre ecran (document 03, §2.6).
						out.PushBack(Separateur());
						out.PushBack(Intitule("Caméra du jeu selon l'écran"));
						{
							static const char *kRegles[5] = {"Tout montrer", "Hauteur fixe", "Largeur fixe",
															  "Tout montrer, avec bandes", "Remplir (rogner)"};
							for (int32 k = 0; k < 5; ++k) {
								out.PushBack(Entree(kRegles[k], NK_A_REGLE_CAMERA + k, "",
													static_cast<int32>(m.appareil.regleCamera) == k));
							}
						}
						break;
					}
					case NkMenuEditeur::NK_REGLAGES: {
						out.PushBack(Entree("Grille", NK_A_GRILLE, "", m.voirGrille));
						out.PushBack(Entree("Collisionneurs", NK_A_COLLISIONNEURS, "", m.voirCollisionneurs));
						out.PushBack(Separateur());
						out.PushBack(Intitule("Matière"));
						out.PushBack(Entree("Liens", NK_A_LIENS, "", m.rendu.liens));
						out.PushBack(Entree("Particules", NK_A_PARTICULES, "", m.rendu.particules));
						out.PushBack(Entree("Vitesses", NK_A_VITESSES, "", m.rendu.vitesses));
						out.PushBack(Separateur());
						static const char *kModes[4] = {"Éclairé", "Filaire", "Contraintes", "Vitesse"};
						for (int32 k = 0; k < 4; ++k) {
							const bool coche = static_cast<int32>(m.rendu.mode) == k;
							out.PushBack(Entree(kModes[k], NK_A_MODE_RENDU + k, "", coche));
						}
						break;
					}
					// (2026-09-30, lot 1) Le clic droit hors d'une entite.
					case NkMenuEditeur::NK_CTX_ARBRE:
						out.PushBack(Intitule("Scène"));
						out.PushBack(Entree("Entité vide", NK_A_NOUVELLE_ENTITE, "Ctrl+E"));
						out.PushBack(SousMenu("Ajouter", NkMenuEditeur::NK_AJOUTER));
						out.PushBack(Separateur());
						out.PushBack(Entree("Cadrer tout", NK_A_CADRER));
						break;
					case NkMenuEditeur::NK_CTX_CONTENU: {
						// (2026-10-01, document 02 §3.1) LE CLIC DROIT D'UNREAL, complet, sur
						// un asset ou un dossier du Contenu ; celui du catalogue reste le sien.
						const NkString &cible = c.ui.contenuMenuChemin;
						out.PushBack(Intitule(editorkit::NkDisqueNom(c.ui.contenuMenuNom.CStr()).CStr()));
						const bool projet = NkEditeurCheminEstContenu(cible.CStr());
						const bool racine = projet && NkEditeurRelatifContenu(cible.CStr()).Empty();
						const bool colle = !c.ui.pressePapierContenu.Empty();
						const uint32 n = c.ui.contenuChoisis.Size() > 1u ? static_cast<uint32>(c.ui.contenuChoisis.Size()) : 1u;
						if (c.ui.contenuMenuDossier) {
							out.PushBack(Entree("Ouvrir", NK_A_CONTENU_OUVRIR));
							if (projet) {
								out.PushBack(Entree("Nouveau dossier", NK_A_CONTENU_NOUVEAU_DOSSIER));
								out.PushBack(Entree("Importer ici…", NK_A_CONTENU_IMPORTER));
								out.PushBack(Separateur());
								out.PushBack(Entree("Couper", NK_A_CONTENU_COUPER, "Ctrl+X", false, !racine));
								out.PushBack(Entree("Copier", NK_A_CONTENU_COPIER, "Ctrl+C", false, !racine));
								out.PushBack(Entree("Coller ici", NK_A_CONTENU_COLLER, "Ctrl+V", false, colle));
								out.PushBack(Entree("Dupliquer", NK_A_CONTENU_DUPLIQUER, "Ctrl+D", false, !racine));
								out.PushBack(Entree("Renommer", NK_A_CONTENU_RENOMMER, "F2", false, !racine && n == 1u));
								out.PushBack(Entree(n > 1u ? NkString::Format("Supprimer (%u)", n).CStr() : "Supprimer", NK_A_CONTENU_SUPPRIMER,
													"Suppr", false, !racine));
								out.PushBack(Separateur());
								out.PushBack(SousMenu("Couleur du dossier", NkMenuEditeur::NK_CONTENU_COULEUR));
								const bool favori = c.ui.contenuMeta.EstFavori(NkEditeurRelatifContenu(cible.CStr()));
								out.PushBack(Entree(favori ? "Retirer des Favoris" : "Ajouter aux Favoris", NK_A_CONTENU_FAVORI));
								out.PushBack(Entree("Copier le chemin", NK_A_CONTENU_COPIER_CHEMIN));
							}
						} else if (projet) {
							// Un asset du projet : sa nature, puis les gestes d'Unreal.
							const NkNatureContenu nature = NkEditeurNatureFichier(cible.CStr());
							out.PushBack(Intitule(nature.libelle));
							if (nature.type == NkAssetType::Scene) {
								out.PushBack(Entree("Ouvrir la scène", NK_A_CONTENU_OUVRIR_ASSET));
							} else if (nature.type == NkAssetType::Prefab || nature.type == NkAssetType::Texture2D) {
								out.PushBack(Entree("Poser au centre de la vue", NK_A_CONTENU_POSER_ASSET));
							}
							out.PushBack(Separateur());
							out.PushBack(Entree("Couper", NK_A_CONTENU_COUPER, "Ctrl+X"));
							out.PushBack(Entree("Copier", NK_A_CONTENU_COPIER, "Ctrl+C"));
							out.PushBack(Entree("Coller", NK_A_CONTENU_COLLER, "Ctrl+V", false, colle));
							out.PushBack(Entree("Dupliquer", NK_A_CONTENU_DUPLIQUER, "Ctrl+D"));
							out.PushBack(Entree("Renommer", NK_A_CONTENU_RENOMMER, "F2", false, n == 1u));
							out.PushBack(Entree(n > 1u ? NkString::Format("Supprimer (%u)", n).CStr() : "Supprimer", NK_A_CONTENU_SUPPRIMER,
												"Suppr"));
							out.PushBack(Separateur());
							if (!c.ui.contenuMeta.collections.Empty()) {
								out.PushBack(SousMenu("Ajouter à une collection", NkMenuEditeur::NK_CONTENU_COLLECTION));
							} else {
								out.PushBack(Entree("Nouvelle collection avec la sélection", NK_A_CONTENU_NOUVELLE_COLLECTION));
							}
							if (c.ui.contenuCollection >= 0) {
								out.PushBack(Entree("Retirer de la collection", NK_A_CONTENU_RETIRER_COLLECTION));
							}
							out.PushBack(Entree("Exporter…", NK_A_CONTENU_EXPORTER));
							out.PushBack(Entree("Copier le chemin", NK_A_CONTENU_COPIER_CHEMIN));
						} else {
							out.PushBack(Entree("Poser au centre de la vue", NK_A_CONTENU_POSER));
							out.PushBack(Entree("Armer « Poser » (clic dans la vue)", NK_A_CONTENU_ARMER));
						}
						break;
					}
					case NkMenuEditeur::NK_CTX_CONTENU_VIDE:
						if (c.ui.contenuProjet || c.ui.contenuCollection >= 0) {
							out.PushBack(Intitule(c.ui.contenuDossier.Empty() ? NK_CONTENU_RACINE : c.ui.contenuDossier.CStr()));
							if (c.ui.contenuProjet) {
								out.PushBack(Entree("Nouveau dossier", NK_A_CONTENU_NOUVEAU_DOSSIER));
								out.PushBack(Separateur());
								out.PushBack(Intitule("Créer ici"));
								out.PushBack(Entree("Scène", NK_A_CONTENU_NOUVELLE_SCENE));
								out.PushBack(Entree("Prefab (de la sélection)", NK_A_CONTENU_NOUVEAU_PREFAB, "", false, m.aSelection));
								out.PushBack(Entree("Contrôleur d'animation", NK_A_CONTENU_NOUVEAU_CONTROLEUR));
								out.PushBack(Separateur());
								out.PushBack(Entree("Importer…", NK_A_CONTENU_IMPORTER));
								out.PushBack(Entree("Coller", NK_A_CONTENU_COLLER, "Ctrl+V", false, !c.ui.pressePapierContenu.Empty()));
							}
							out.PushBack(Entree("Tout sélectionner", NK_A_CONTENU_TOUT_SELECTIONNER, "Ctrl+A"));
							out.PushBack(Entree("Exporter la sélection…", NK_A_CONTENU_EXPORTER, "", false, !c.ui.contenuChoisis.Empty()));
							out.PushBack(Separateur());
							out.PushBack(Entree("Revenir à « Contenu »", NK_A_CONTENU_RACINE, "", false, !c.ui.contenuDossier.Empty()));
						} else {
							out.PushBack(Intitule(c.ui.categorie >= 0 ? NkCategorieActeurNom(static_cast<NkCategorieActeur>(c.ui.categorie))
																	   : "Acteurs"));
							out.PushBack(Entree("Revenir à « Acteurs »", NK_A_CONTENU_RACINE, "", false, c.ui.categorie >= 0));
							out.PushBack(Entree("Importer dans le Contenu…", NK_A_CONTENU_IMPORTER));
						}
						break;
					// ── Le navigateur a la maniere d'UE5 (2026-10-01) ──
					case NkMenuEditeur::NK_CONTENU_AJOUTER:
						out.PushBack(Intitule("Créer dans le dossier courant"));
						out.PushBack(Entree("Nouveau dossier", NK_A_CONTENU_NOUVEAU_DOSSIER));
						out.PushBack(Separateur());
						out.PushBack(Entree("Scène", NK_A_CONTENU_NOUVELLE_SCENE));
						out.PushBack(Entree("Prefab (de la sélection)", NK_A_CONTENU_NOUVEAU_PREFAB, "", false, m.aSelection));
						out.PushBack(Entree("Contrôleur d'animation", NK_A_CONTENU_NOUVEAU_CONTROLEUR));
						out.PushBack(Separateur());
						out.PushBack(Entree("Importer…", NK_A_CONTENU_IMPORTER));
						break;
					case NkMenuEditeur::NK_CONTENU_REGLAGES: {
						out.PushBack(Intitule("Taille des vignettes"));
						static const char *kTailles[4] = {"Petite", "Moyenne", "Grande", "Énorme"};
						static const float32 kPx[4] = {48.f, 72.f, 112.f, 160.f};
						for (int32 k = 0; k < 4; ++k) {
							const float32 t = c.ui.contenu.thumbSize;
							const bool coche = t >= (k == 0 ? 0.f : (kPx[k - 1] + kPx[k]) * 0.5f) && (k == 3 || t < (kPx[k] + kPx[k + 1]) * 0.5f);
							out.PushBack(Entree(kTailles[k], NK_A_CONTENU_TAILLE + k, k == 0 ? "Ctrl+molette" : "", coche));
						}
						out.PushBack(Separateur());
						out.PushBack(Intitule("Afficher"));
						out.PushBack(Entree("Les dossiers", NK_A_CONTENU_AFFICHER_DOSSIERS, "", c.ui.contenu.montrerDossiers));
						out.PushBack(Entree("Les filtres par type", NK_A_CONTENU_FILTRES, "", c.ui.contenu.filtresOuverts));
						out.PushBack(Entree("En liste", NK_A_CONTENU_VUE_LISTE, "", c.ui.contenu.viewMode == 1));
						break;
					}
					case NkMenuEditeur::NK_CONTENU_TRI: {
						out.PushBack(Intitule("Trier par"));
						static const char *kCles[4] = {"Nom", "Date", "Taille", "Type"};
						for (int32 k = 0; k < 4; ++k) {
							out.PushBack(Entree(kCles[k], NK_A_CONTENU_TRI + k, "", c.ui.contenu.sortCle == k));
						}
						out.PushBack(Separateur());
						out.PushBack(Entree(c.ui.contenu.sortAsc ? "Ordre : croissant" : "Ordre : décroissant", NK_A_CONTENU_TRI_SENS));
						break;
					}
					case NkMenuEditeur::NK_CONTENU_DEPOSER:
						// Le menu du GLISSER d'Unreal (« Move Here / Copy Here »).
						out.PushBack(Intitule(NkString::Format("%u élément(s) vers %s", static_cast<uint32>(c.ui.deposeSources.Size()),
															   editorkit::NkDisqueNom(c.ui.deposeCible.CStr()).CStr())
												  .CStr()));
						out.PushBack(Entree("Déplacer ici", NK_A_CONTENU_DEPLACER_ICI, "", false, !c.ui.deposeSources.Empty()));
						out.PushBack(Entree("Copier ici", NK_A_CONTENU_COPIER_ICI, "", false, !c.ui.deposeSources.Empty()));
						break;
					case NkMenuEditeur::NK_CONTENU_COULEUR:
						for (int32 k = 0; k < NkEditeurNbCouleursDossier(); ++k) {
							uint32 rgba = 0u;
							const char *nom = NkEditeurCouleurDossier(k, rgba);
							out.PushBack(Entree(nom, NK_A_CONTENU_COULEUR + k));
						}
						break;
					case NkMenuEditeur::NK_CONTENU_COLLECTION:
						for (uint32 k = 0; k < c.ui.contenuMeta.collections.Size() && k < 20u; ++k) {
							out.PushBack(Entree(c.ui.contenuMeta.collections[k].nom.CStr(), NK_A_CONTENU_COLLECTION + static_cast<int32>(k)));
						}
						out.PushBack(Separateur());
						out.PushBack(Entree("Nouvelle collection", NK_A_CONTENU_NOUVELLE_COLLECTION));
						break;
					case NkMenuEditeur::NK_CTX_COLLECTION:
						if (c.ui.collectionMenu >= 0 && static_cast<uint32>(c.ui.collectionMenu) < c.ui.contenuMeta.collections.Size()) {
							out.PushBack(Intitule(c.ui.contenuMeta.collections[static_cast<uint32>(c.ui.collectionMenu)].nom.CStr()));
						}
						out.PushBack(Entree("Supprimer la collection (les fichiers restent)", NK_A_CONTENU_SUPPRIMER_COLLECTION));
						break;
					case NkMenuEditeur::NK_CARTE: {
						const int32 k = c.ui.carteMenu;
						if (k < 0 || k >= static_cast<int32>(NkCarteEditeur::NK_COUNT) || !m.aSelection) {
							break;
						}
						const NkCarteEditeur carte = static_cast<NkCarteEditeur>(k);
						const bool copie = NkEditeurCarteSeCopie(carte);
						NkComposantEditeur comp;
						const bool composant = NkComposantDeCarte(carte, comp);
						const bool mobile = carte != NkCarteEditeur::NK_TRANSFORM;
						out.PushBack(Intitule(NkCarteEditeurNom(carte)));
						out.PushBack(Entree("Réinitialiser", NK_A_CARTE_REINIT, "", false, copie));
						out.PushBack(Entree("Retirer le composant", NK_A_CARTE_RETIRER, "", false, composant));
						out.PushBack(Separateur());
						out.PushBack(Entree("Monter", NK_A_CARTE_MONTER, "", false, mobile));
						out.PushBack(Entree("Descendre", NK_A_CARTE_DESCENDRE, "", false, mobile));
						out.PushBack(Separateur());
						out.PushBack(Entree("Copier les valeurs", NK_A_CARTE_COPIER, "", false, copie));
						out.PushBack(Entree("Coller les valeurs", NK_A_CARTE_COLLER, "", false, copie && c.ui.pressePapier.carte == k));
						break;
					}
					case NkMenuEditeur::NK_PAS_GRILLE:
					case NkMenuEditeur::NK_PAS_ANGLE:
					case NkMenuEditeur::NK_PAS_ECHELLE: {
						// Les pas de la barre flottante : la valeur courante cochee.
						int32 n = 0;
						const float32 *pas = menu == NkMenuEditeur::NK_PAS_GRILLE  ? NkPasGrille(n)
											 : menu == NkMenuEditeur::NK_PAS_ANGLE ? NkPasAngle(n)
																				   : NkPasEchelle(n);
						const float32 courant = menu == NkMenuEditeur::NK_PAS_GRILLE  ? c.ui.pasGrille
												: menu == NkMenuEditeur::NK_PAS_ANGLE ? c.ui.pasAngle
																					  : c.ui.pasEchelle;
						const int32 base = menu == NkMenuEditeur::NK_PAS_GRILLE  ? NK_A_PAS_GRILLE
										   : menu == NkMenuEditeur::NK_PAS_ANGLE ? NK_A_PAS_ANGLE
																				 : NK_A_PAS_ECHELLE;
						for (int32 k = 0; k < n; ++k) {
							const NkString t = menu == NkMenuEditeur::NK_PAS_GRILLE
												   ? NkString::Format("%g m", static_cast<double>(pas[k]))
												   : (menu == NkMenuEditeur::NK_PAS_ANGLE ? NkString::Format("%g°", static_cast<double>(pas[k]))
																						  : NkString::Format("x %g", static_cast<double>(pas[k])));
							const float32 ecart = pas[k] - courant;
							out.PushBack(Entree(t.CStr(), base + k, "", ecart > -1.0e-5f && ecart < 1.0e-5f));
						}
						break;
					}
					case NkMenuEditeur::NK_COMPOSANT: {
						if (!m.aSelection || !m.scene.Monde().IsAlive(m.selection)) {
							out.PushBack(Intitule("(aucune sélection)"));
							break;
						}
						// Ne s'y trouve que ce qui PEUT s'ajouter : un composant deja
						// present, ou un corps rigide sur de la matiere, n'y figure pas.
						for (int32 k = 0; k < static_cast<int32>(NkComposantEditeur::NK_COUNT); ++k) {
							const NkComposantEditeur comp = static_cast<NkComposantEditeur>(k);
							if (!NkEditeurPeutAjouter(m, m.selection, comp)) {
								continue;
							}
							// 2026-09-30 : une lumiere se choisit par son TYPE, un emetteur
							// par son PRESET, comme un corps mou par sa matiere.
							if (comp == NkComposantEditeur::NK_LUMIERE) {
								static const char *kTypes[3] = {"ponctuelle", "projecteur (cône)", "directionnelle (lune, soleil)"};
								out.PushBack(Separateur());
								out.PushBack(Intitule("Lumière 2D"));
								for (int32 t = 0; t < 3; ++t) {
									out.PushBack(Entree(kTypes[t], NK_A_LUMIERE + t));
								}
								continue;
							}
							if (comp == NkComposantEditeur::NK_EMETTEUR) {
								out.PushBack(Separateur());
								out.PushBack(Intitule("Émetteur de particules"));
								for (int32 p = 1; p < static_cast<int32>(NkPresetEffet2D::NK_COUNT); ++p) {
									out.PushBack(Entree(NkNomPresetEffet2D(static_cast<NkPresetEffet2D>(p)), NK_A_EMETTEUR + p));
								}
								continue;
							}
							if (comp != NkComposantEditeur::NK_CORPS_MOU) {
								out.PushBack(Entree(NkComposantEditeurNom(comp), NK_A_COMPOSANT + k));
								continue;
							}
							// Un corps mou est une MATIERE : on la choisit ici.
							out.PushBack(Separateur());
							out.PushBack(Intitule("Corps mou"));
							for (int32 i = 0; i < static_cast<int32>(NkActeurSim::NK_COUNT); ++i) {
								const NkInfoActeurSim &info = NkActeurSimInfo(static_cast<NkActeurSim>(i));
								if (!info.rigide) {
									out.PushBack(Entree(info.nom, NK_A_CORPS_MOU + i));
								}
							}
						}
						if (out.Empty()) {
							out.PushBack(Intitule("(tous les composants possibles sont là)"));
						}
						break;
					}
					default:
						break;
				}
			}

		} // namespace

		// =====================================================================
		// OUTILS COMMUNS
		// =====================================================================
		NkPaletteEditeur NkEditeurPalette(const editorkit::NkTheme &t) {
			auto R = [&t](NkRole r) {
				return editorkit::NkThemeUnpack(t.Get(r));
			};
			NkPaletteEditeur p;
			p.fond = R(NkRole::WindowBg);
			p.panneau = R(NkRole::PanelBg);
			p.entete = R(NkRole::PanelHeader);
			p.bord = R(NkRole::Border);
			p.champ = R(NkRole::InputBg);
			p.texte = R(NkRole::Text);
			p.attenue = R(NkRole::TextMuted);
			p.accent = R(NkRole::AccentUi);
			p.selection = R(NkRole::AccentSel);
			p.surAccent = R(NkRole::TextOnAccent);
			p.bouton = editorkit::NkThemeUnpack(t.GetOuRepli(NkRole::ButtonBg, NkRole::PanelHeader));
			// La MEME regle que la conversion du kit (NkThemeVersGui) : un survol
			// est une fonction des roles, pas une couleur de plus.
			p.boutonSurvol = editorkit::NkThemeMix(p.bouton, p.accent, 0.20f);
			return p;
		}

		bool NkEditeurDans(const NkRect &r, const NkVec2 &p) noexcept {
			return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
		}

		editorkit::NkComponentInput NkEditeurEntreeComposant(const nkgui::NkGuiContext &ctx) {
			// La meme traduction que NkEditorShell::DrawTabStrip : l'echelle vient
			// de la surface, les gestes de l'entree NKGui de CETTE trame.
			const nkgui::NkGuiInput &in = ctx.input;
			editorkit::NkComponentInput ci;
			ci.surfaceScale = ctx.scale;
			ci.mouseX = in.mousePos.x;
			ci.mouseY = in.mousePos.y;
			ci.wheel = in.wheel;
			ci.mouseDown = in.mouseDown[0];
			ci.mousePressed = in.mouseClicked[0];
			ci.mouseReleased = in.mouseReleased[0];
			ci.doubleClick = in.mouseDoubleClicked[0];
			ci.rightPressed = in.mouseClicked[1];
			ci.ctrl = in.ctrlDown;
			ci.shift = in.shiftDown;
			ci.alt = in.altDown;
			return ci;
		}

		bool NkEditeurBouton(NkEditeurCadre &c, const NkRect &r, const char *texte, bool enfonce, bool actif,
							 nkgui::NkGuiDrawList *dl) {
			nkgui::NkGuiDrawList &liste = dl != nullptr ? *dl : c.ctx.dl;
			const nkgui::NkGuiInput &in = c.ctx.input;
			const bool survol = actif && NkEditeurDans(r, in.mousePos);
			NkColor fond = c.pal.bouton;
			if (enfonce) {
				fond = c.pal.accent;
			} else if (survol) {
				fond = c.pal.boutonSurvol;
			}
			liste.AddRectFilled(r, fond, 2.f);
			liste.AddRect(r, c.pal.bord, 1.f, 2.f);
			if (texte != nullptr && texte[0] != '\0') {
				NkColor t = c.pal.texte;
				if (!actif) {
					t = c.pal.attenue;
				} else if (enfonce) {
					t = c.pal.surAccent;
				}
				renderer::NkTexteDansBoite(liste, c.police, r, texte, t);
			}
			return survol && in.mouseClicked[0];
		}

		bool NkEditeurOnglets(NkEditeurCadre &c, const NkRect &bande, const char *const *noms, int32 n, int32 &actif) {
			auto &dl = c.ctx.dl;
			dl.AddRectFilled(bande, c.pal.entete);
			dl.AddRectFilled(NkRect{bande.x, bande.y + bande.h - 1.f, bande.w, 1.f}, c.pal.bord);
			bool change = false;
			float32 x = bande.x;
			for (int32 i = 0; i < n; ++i) {
				const float32 w = renderer::NkTexteLargeur(c.police, noms[i]) + 24.f;
				const NkRect r{x, bande.y, w, bande.h};
				const bool survol = NkEditeurDans(r, c.ctx.input.mousePos);
				if (i == actif) {
					// L'onglet actif prend la couleur du panneau qu'il ouvre, et
					// un filet d'accent : c'est ainsi qu'UE5 le distingue.
					dl.AddRectFilled(r, c.pal.panneau);
					dl.AddRectFilled(NkRect{r.x, r.y, r.w, 2.f}, c.pal.accent);
				} else if (survol) {
					dl.AddRectFilled(r, c.pal.boutonSurvol);
				}
				renderer::NkTexteDansBoite(dl, c.police, r, noms[i], i == actif ? c.pal.texte : c.pal.attenue);
				if (survol && c.ctx.input.mouseClicked[0] && i != actif) {
					actif = i;
					change = true;
				}
				x += w;
			}
			return change;
		}

		void NkEditeurPlanifier(NkEditeurInterface &ui, float32 largeur, float32 hauteur) {
			const float32 W = largeur;
			const float32 H = hauteur;
			ui.ecran = NkRect{0.f, 0.f, W, H};
			ui.barreMenus = NkRect{0.f, 0.f, W, HAUTEUR_MENUS};
			ui.barreOnglets = NkRect{0.f, HAUTEUR_MENUS, W, HAUTEUR_ONGLETS};
			ui.barreOutils = NkRect{0.f, HAUTEUR_MENUS + HAUTEUR_ONGLETS, W, HAUTEUR_OUTILS};
			ui.statut = NkRect{0.f, H - HAUTEUR_STATUT, W, HAUTEUR_STATUT};
			const float32 corpsHaut = HAUTEUR_MENUS + HAUTEUR_ONGLETS + HAUTEUR_OUTILS;
			const float32 corpsBas = H - HAUTEUR_STATUT;
			const float32 corpsH = corpsBas - corpsHaut > 0.f ? corpsBas - corpsHaut : 0.f;

			// ⚠️ LES BORNES SONT RE-APPLIQUEES A CHAQUE TRAME, pas au glisser seul :
			//    une fenetre retrecie apres coup doit rendre la place au viseur, sinon
			//    un Outliner de 250 px mange un ecran de 400.
			float32 tiroirH = 0.f;
			if (ui.voirTiroir) {
				tiroirH = Borne(ui.hauteurTiroir, 90.f, corpsH - 160.f);
			}
			ui.tiroir = NkRect{0.f, corpsBas - tiroirH, W, tiroirH};
			const float32 colonnesBas = corpsBas - tiroirH - (ui.voirTiroir ? EPAISSEUR_CLOISON : 0.f);
			const float32 colonnesH = colonnesBas - corpsHaut > 0.f ? colonnesBas - corpsHaut : 0.f;

			const float32 L = ui.voirOutliner ? Borne(ui.largeurOutliner, 150.f, W * 0.35f) : 0.f;
			const float32 R = ui.voirDetails ? Borne(ui.largeurDetails, 240.f, W * 0.42f) : 0.f;
			ui.outliner = NkRect{0.f, corpsHaut, L, colonnesH};
			ui.details = NkRect{W - R, corpsHaut, R, colonnesH};
			const float32 vx = L + (L > 0.f ? EPAISSEUR_CLOISON : 0.f);
			float32 vw = W - R - (R > 0.f ? EPAISSEUR_CLOISON : 0.f) - vx;
			if (vw < 0.f) {
				vw = 0.f;
			}
			ui.vue = NkRect{vx, corpsHaut, vw, colonnesH};
			ui.barreVue = NkRect{vx, corpsHaut, vw, HAUTEUR_BARRE_VUE};
			const float32 viseurH = colonnesH - HAUTEUR_BARRE_VUE > 0.f ? colonnesH - HAUTEUR_BARRE_VUE : 0.f;
			ui.viseur = NkRect{vx, corpsHaut + HAUTEUR_BARRE_VUE, vw, viseurH};
		}

		void NkEditeurOuvrirMenu(NkEditeurCadre &c, NkMenuEditeur menu, const NkRect &ancre) {
			if (c.ui.menu == menu) {
				c.ui.menu = NkMenuEditeur::NK_AUCUN;
				return;
			}
			c.ui.menu = menu;
			c.ui.menuAncre = ancre;
			c.ui.menuFiltre[0] = '\0';
			c.ui.sousMenu = NkMenuEditeur::NK_AUCUN;
			// Le rectangle est recalcule au dessin ; en attendant, l'ancre
			// suffit a ce que le masquage de la trame suivante sache ou il est.
			c.ui.menuRect = NkRect{ancre.x, ancre.y + ancre.h, 1.f, 1.f};
			c.ui.sousMenuRect = NkRect{0.f, 0.f, 0.f, 0.f};
		}

		// =====================================================================
		// LES MODIFICATIONS NON ENREGISTREES
		// =====================================================================
		uint64 NkEditeurEmpreinte(NkEditeurModele &m) {
			// ⚠️ LA CAMERA EST NEUTRALISEE LE TEMPS DE L'EMPREINTE. La sauvegarde
			//    ecrit le centre, le zoom et la rotation de la vue : sans cela, le
			//    cadrage de la premiere trame suffisait a « modifier » une scene a
			//    peine ouverte (mesure du 2026-09-29 : le point de l'onglet
			//    s'allumait au demarrage), et chaque coup de molette aussi. Regarder
			//    une scene ne la change pas.
			NkVue2D &cam = m.scene.Camera();
			const NkVec2f centre = cam.Centre();
			const float32 zoom = cam.Zoom();
			const float32 rotation = cam.Rotation();
			cam.PoserCentre(NkVec2f(0.f, 0.f));
			cam.PoserZoom(1.f);
			cam.PoserRotation(0.f);
			NkString json;
			const bool ok = NkSauverSceneJSON(m.scene, json, &m.textures);
			cam.PoserCentre(centre);
			cam.PoserZoom(zoom);
			cam.PoserRotation(rotation);
			if (!ok) {
				return 0u;
			}
			// FNV-1a 64, celle du moteur : la MEME que l'empreinte de livraison
			// (temoin l1), pour qu'il n'y ait qu'une idee de « la meme scene ».
			return NkEmpreinteTexte(json.CStr(), static_cast<usize>(json.Length()));
		}

		void NkEditeurRetenirEmpreinte(NkEditeurModele &m, NkEditeurInterface &ui) {
			ui.empreinteEnregistree = NkEditeurEmpreinte(m);
			ui.modifiee = false;
			ui.ageEmpreinte = 0.f;
		}

		void NkEditeurSuivreModifications(NkEditeurModele &m, NkEditeurInterface &ui, float32 dt) {
			ui.ageEmpreinte += dt;
			if (ui.ageEmpreinte < 1.f || m.etat != NkEtatJeu::NK_EDITION) {
				return;
			}
			ui.ageEmpreinte = 0.f;
			ui.modifiee = NkEditeurEmpreinte(m) != ui.empreinteEnregistree;
		}

		namespace {

			/// L'action, sans plus rien demander.
			void Accomplir(NkEditeurCadre &c, int32 action) {
				NkEditeurModele &m = c.m;
				NkEditeurInterface &ui = c.ui;
				switch (action) {
					case NK_A_NOUVEAU:
					case NK_A_FERMER_SCENE:
						NkEditeurNouvelleScene(m);
						// L'onglet redevient « Scene_01 » : la scene neuve n'est pas
						// encore un fichier. Le prochain Enregistrer choisira le chemin.
						m.chemin = NkString();
						NkEditeurAnnoncer(m, action == NK_A_NOUVEAU ? "Nouvelle scene" : "Scene fermee");
						ui.cadrageEnAttente = true;
						NkEditeurRetenirEmpreinte(m, ui);
						break;
					case NK_A_OUVRIR:
						if (NkEditeurOuvrir(m)) {
							NkEditeurRetenirEmpreinte(m, ui);
						}
						break;
					case NK_A_CONTENU_OUVRIR_ASSET: {
						// La scene du navigateur DEVIENT la scene de l'editeur ; un
						// echec rend l'ancien chemin (on n'enregistrera pas ailleurs).
						const NkString avant = m.chemin;
						// (2026-10-01, retour 3 de Rihen) Le projet est RETENU : la scene
						// ouverte depuis le navigateur ne le change pas -- ni la racine
						// du Contenu, ni le dossier courant (NkEditeurDossierProjet).
						m.projet = NkEditeurDossierProjet(m);
						m.chemin = ui.sceneAOuvrir;
						ui.sceneAOuvrir = NkString();
						if (NkEditeurOuvrir(m)) {
							NkEditeurRetenirEmpreinte(m, ui);
							ui.cadrageEnAttente = true;
						} else {
							m.chemin = avant;
						}
						break;
					}
					case NK_A_QUITTER:
						ui.demandeQuitter = true;
						break;
					default:
						break;
				}
			}

			/// Pose la question si la scene a change ; sinon, agit tout de suite.
			void Demander(NkEditeurCadre &c, int32 action) {
				NkEditeurModele &m = c.m;
				// En jeu, la scene editee est la PHOTO d'avant « Jouer » : Arreter la
				// rend, et c'est elle qu'on compare -- pas un instant de simulation.
				// Chacun de ces gestes quitte de toute facon le jeu.
				if (m.etat != NkEtatJeu::NK_EDITION) {
					NkEditeurArreter(m);
				}
				c.ui.modifiee = NkEditeurEmpreinte(m) != c.ui.empreinteEnregistree;
				if (!c.ui.modifiee) {
					Accomplir(c, action);
					return;
				}
				c.ui.confirmation = action;
			}

		} // namespace

		void NkEditeurDessinerConfirmation(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			if (ui.confirmation == NK_A_AUCUNE) {
				return;
			}
			auto &dl = c.ctx.dlOverlay;
			const nkgui::NkGuiInput &in = c.ctx.input;
			// Le voile : tout le reste attend la reponse.
			dl.AddRectFilled(ui.ecran, NkColor{0, 0, 0, 110});
			const float32 w = 460.f;
			const float32 h = 132.f;
			const NkRect boite{(ui.ecran.w - w) * 0.5f, (ui.ecran.h - h) * 0.42f, w, h};
			dl.AddRectFilled(NkRect{boite.x + 4.f, boite.y + 6.f, boite.w, boite.h}, NkColor{0, 0, 0, 110}, 3.f);
			dl.AddRectFilled(boite, c.pal.entete, 3.f);
			dl.AddRect(boite, c.pal.bord, 1.f, 3.f);
			dl.AddRectFilled(NkRect{boite.x, boite.y, boite.w, 2.f}, c.pal.selection);

			const char *verbe = "fermer la scène";
			if (ui.confirmation == NK_A_NOUVEAU) {
				verbe = "créer une nouvelle scène";
			} else if (ui.confirmation == NK_A_OUVRIR) {
				verbe = "ouvrir la scène enregistrée";
			} else if (ui.confirmation == NK_A_QUITTER) {
				verbe = "quitter";
			}
			const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f);
			renderer::NkTexte(dl, c.police, boite.x + 16.f, boite.y + 14.f, "Modifications non enregistrées", c.pal.texte);
			renderer::NkTexte(dl, c.petite, boite.x + 16.f, boite.y + 20.f + lh,
							  "La scène a changé depuis son dernier enregistrement.", c.pal.attenue);
			const NkString suite = NkString::Format("L'enregistrer avant de %s ?", verbe);
			renderer::NkTexte(dl, c.petite, boite.x + 16.f, boite.y + 38.f + lh, suite.CStr(), c.pal.attenue);

			// Trois reponses, la sure a droite, comme partout.
			const float32 bh = 26.f;
			const float32 by = boite.y + boite.h - bh - 12.f;
			const float32 wEnr = renderer::NkTexteLargeur(c.police, "Enregistrer") + 28.f;
			const float32 wSans = renderer::NkTexteLargeur(c.police, "Ne pas enregistrer") + 28.f;
			const float32 wAnn = renderer::NkTexteLargeur(c.police, "Annuler") + 28.f;
			const NkRect rAnn{boite.x + boite.w - 12.f - wAnn, by, wAnn, bh};
			const NkRect rSans{rAnn.x - 8.f - wSans, by, wSans, bh};
			const NkRect rEnr{rSans.x - 8.f - wEnr, by, wEnr, bh};
			const int32 action = ui.confirmation;
			if (NkEditeurBouton(c, rEnr, "Enregistrer", true, true, &dl) || in.KeyPressed(nkgui::NkGuiKey::Enter)) {
				ui.confirmation = NK_A_AUCUNE;
				const bool enregistree = NkEditeurSauver(c.m);
				// Au journal MAINTENANT : l'action qui suit annonce aussi (« Scene
				// fermee »), et une seule annonce survit a la trame.
				NkEditeurJournaliser(c.m, ui);
				if (enregistree) {
					NkEditeurRetenirEmpreinte(c.m, ui);
					Accomplir(c, action);
				}
				// Un enregistrement rate ne ferme RIEN : le travail serait perdu
				// sur un message d'erreur que personne n'aurait le temps de lire.
				return;
			}
			if (NkEditeurBouton(c, rSans, "Ne pas enregistrer", false, true, &dl)) {
				ui.confirmation = NK_A_AUCUNE;
				Accomplir(c, action);
				return;
			}
			if (NkEditeurBouton(c, rAnn, "Annuler", false, true, &dl) || in.KeyPressed(nkgui::NkGuiKey::Escape)) {
				ui.confirmation = NK_A_AUCUNE;
			}
		}

		namespace {

			/// Pose un acteur (ou l'entite simple) en `point` SANS changer ce que
			/// l'outil Poser a d'arme : poser depuis un menu n'est pas choisir un
			/// pinceau, et le tiroir, qui montre l'acteur arme, ne doit pas changer
			/// de carte sous les yeux.
			void PoserSansArmer(NkEditeurModele &m, bool simple, NkActeurSim acteur, const NkVec2f &point) {
				const NkActeurSim acteurArme = m.acteur;
				const bool simpleArme = m.acteurSimple;
				m.acteur = acteur;
				m.acteurSimple = simple;
				NkEditeurPoser(m, point);
				m.acteur = acteurArme;
				m.acteurSimple = simpleArme;
			}

		} // namespace

		// =====================================================================
		// LA TABLE D'ACTIONS
		// =====================================================================
		void NkEditeurExecuter(NkEditeurCadre &c, int32 action) {
			NkEditeurModele &m = c.m;
			NkEditeurInterface &ui = c.ui;
			// Les plages d'abord : leur indice est ajoute a la base.
			// 2026-09-30 : lumieres et emetteurs (NkEditeurLumiere.h).
			if (action >= NK_A_LUMIERE && action < NK_A_LUMIERE + 3) {
				if (m.aSelection) {
					NkEditeurAjouterLumiere(m, m.selection, static_cast<NkTypeLumiere2D>(action - NK_A_LUMIERE));
				}
				return;
			}
			if (action >= NK_A_EMETTEUR && action < NK_A_EMETTEUR + static_cast<int32>(NkPresetEffet2D::NK_COUNT)) {
				if (m.aSelection) {
					NkEditeurAjouterEffet(m, m.selection, static_cast<NkPresetEffet2D>(action - NK_A_EMETTEUR));
				}
				return;
			}
			if (action >= NK_A_LUMIERE_ICI && action < NK_A_LUMIERE_ICI + 3) {
				NkEditeurPoserLumiere(m, ui.pointContexte, static_cast<NkTypeLumiere2D>(action - NK_A_LUMIERE_ICI));
				return;
			}
			if (action >= NK_A_EMETTEUR_ICI && action < NK_A_EMETTEUR_ICI + static_cast<int32>(NkPresetEffet2D::NK_COUNT)) {
				NkEditeurPoserEffet(m, ui.pointContexte, static_cast<NkPresetEffet2D>(action - NK_A_EMETTEUR_ICI));
				return;
			}
			if (action >= NK_A_POSER_ICI && action < NK_A_POSER_ICI + static_cast<int32>(NkActeurSim::NK_COUNT)) {
				PoserSansArmer(m, false, static_cast<NkActeurSim>(action - NK_A_POSER_ICI), ui.pointContexte);
				return;
			}
			if (action >= NK_A_CORPS_MOU && action < NK_A_CORPS_MOU + static_cast<int32>(NkActeurSim::NK_COUNT)) {
				if (m.aSelection) {
					const NkActeurSim matiere = static_cast<NkActeurSim>(action - NK_A_CORPS_MOU);
					NkEditeurAjouterComposant(m, m.selection, NkComposantEditeur::NK_CORPS_MOU, matiere);
				}
				return;
			}
			{
				// Les pas de la barre flottante.
				int32 n = 0;
				const float32 *pg = NkPasGrille(n);
				if (action >= NK_A_PAS_GRILLE && action < NK_A_PAS_GRILLE + n) {
					ui.pasGrille = pg[action - NK_A_PAS_GRILLE];
					ui.accrocheGrille = true;
					return;
				}
				const float32 *pa = NkPasAngle(n);
				if (action >= NK_A_PAS_ANGLE && action < NK_A_PAS_ANGLE + n) {
					ui.pasAngle = pa[action - NK_A_PAS_ANGLE];
					ui.accrocheAngle = true;
					return;
				}
				const float32 *pe = NkPasEchelle(n);
				if (action >= NK_A_PAS_ECHELLE && action < NK_A_PAS_ECHELLE + n) {
					ui.pasEchelle = pe[action - NK_A_PAS_ECHELLE];
					ui.accrocheEchelle = true;
					return;
				}
			}
			if (action >= NK_A_COMPOSANT && action < NK_A_COMPOSANT + static_cast<int32>(NkComposantEditeur::NK_COUNT)) {
				if (m.aSelection) {
					NkEditeurAjouterComposant(m, m.selection, static_cast<NkComposantEditeur>(action - NK_A_COMPOSANT));
				}
				return;
			}
			if (action >= NK_A_MODE_RENDU && action < NK_A_MODE_RENDU + 4) {
				m.rendu.mode = static_cast<NkModeRenduParticules>(action - NK_A_MODE_RENDU);
				return;
			}
			// <= : l'indice NkNbProfils() est l'appareil PERSONNALISE.
			if (action >= NK_A_APPAREIL && action <= NK_A_APPAREIL + NkNbProfils()) {
				m.profil = action - NK_A_APPAREIL;
				return;
			}
			if (action >= NK_A_OPTION_APPAREIL && action < NK_A_OPTION_APPAREIL + static_cast<int32>(NkOptionAppareil::NK_COUNT)) {
				NkReglagesAppareil &r = m.appareil;
				switch (static_cast<NkOptionAppareil>(action - NK_A_OPTION_APPAREIL)) {
					case NkOptionAppareil::NK_CADRE:
						r.voirCadre = !r.voirCadre;
						break;
					case NkOptionAppareil::NK_ZONE_SURE:
						r.voirZoneSure = !r.voirZoneSure;
						break;
					case NkOptionAppareil::NK_DECOUPE:
						r.voirDecoupe = !r.voirDecoupe;
						break;
					case NkOptionAppareil::NK_CADRE_CLAIR:
						r.cadreClair = !r.cadreClair;
						break;
					case NkOptionAppareil::NK_APERCU_JEU:
						r.apercuJeu = !r.apercuJeu;
						break;
					case NkOptionAppareil::NK_AJOUTER_HUD:
						if (m.etat == NkEtatJeu::NK_EDITION) {
							NkEditeurAjouterHud(m, NkAncre::NK_HAUT_DROITE, "Interface", 0xE0A030FFu, NkVec2f(1.2f, 0.6f));
						}
						break;
					case NkOptionAppareil::NK_ANCRER_SELECTION:
						NkEditeurAncrerSelection(m);
						break;
					case NkOptionAppareil::NK_PERSONNALISER:
						// Une copie modifiable de l'appareil regarde, qui devient
						// l'appareil courant (Details > Monde pour la modifier).
						if (!m.ProfilPersonnalise()) {
							r.persoBase = m.profil;
							r.perso = NkPersonnaliser(NkProfil(m.profil));
							m.profil = NkNbProfils();
						}
						break;
					default:
						break;
				}
				return;
			}
			if (action >= NK_A_REGLE_CAMERA && action < NK_A_REGLE_CAMERA + static_cast<int32>(NkRegleCamera::NK_COUNT)) {
				m.appareil.regleCamera = static_cast<NkRegleCamera>(action - NK_A_REGLE_CAMERA);
				return;
			}
			if (action >= NK_A_ORIENTATION && action < NK_A_ORIENTATION + static_cast<int32>(NkOrientation::NK_COUNT)) {
				m.orientation = static_cast<NkOrientation>(action - NK_A_ORIENTATION);
				return;
			}
			if (action >= NK_A_POSER_ACTEUR && action < NK_A_POSER_ACTEUR + static_cast<int32>(NkActeurSim::NK_COUNT)) {
				// « + Ajouter » POSE : au centre de la vue, la ou l'on regarde. Le
				// tiroir, lui, ARME l'outil (clic) ou pose au point de depot (glisser).
				PoserSansArmer(m, false, static_cast<NkActeurSim>(action - NK_A_POSER_ACTEUR), m.scene.Camera().Centre());
				return;
			}
			if (action >= NK_A_OUTIL && action <= NK_A_OUTIL + static_cast<int32>(NkOutil::NK_ECHELLE)) {
				m.outil = static_cast<NkOutil>(action - NK_A_OUTIL);
				return;
			}
			switch (action) {
				case NK_A_NOUVEAU:
				case NK_A_OUVRIR:
				case NK_A_QUITTER:
				case NK_A_FERMER_SCENE:
					// Les quatre gestes qui PERDENT la scene editee : ils passent par
					// la question « enregistrer ? » quand elle a change.
					Demander(c, action);
					break;
				case NK_A_ENREGISTRER:
					if (NkEditeurSauver(m)) {
						NkEditeurRetenirEmpreinte(m, ui);
					}
					break;
				case NK_A_CONSTRUIRE:
					// Une DEMANDE, comme la fermeture : la fenetre (et son etat,
					// processus compris) appartient a l'application.
					ui.construireDemande = true;
					break;
				case NK_A_NOUVELLE_ENTITE:
					NkEditeurCreerEntite(m, "Entite", m.scene.Camera().Centre());
					break;
				case NK_A_DUPLIQUER:
					if (m.aSelection) {
						NkEditeurDupliquer(m);
					}
					break;
				case NK_A_SUPPRIMER:
					if (m.aSelection) {
						NkEditeurSupprimerSelection(m);
					}
					break;
				case NK_A_CADRER:
					NkEditeurDemanderCadrage(c, true);
					break;
				case NK_A_CADRER_SELECTION:
					// Sans selection, F cadre toute la scene : NkEditeurZoneACadrer.
					NkEditeurDemanderCadrage(c, false);
					break;
				case NK_A_RENOMMER:
					// EN PLACE dans l'Outliner (F2 d'UE5) ; le champ Nom des Details
					// quand l'Outliner est ferme.
					if (m.aSelection && ui.voirOutliner) {
						ui.renommerEnPlace = true;
					} else if (m.aSelection) {
						ui.voirDetails = true;
						ui.ongletDroite = 0;
						ui.renommerDemande = true;
					}
					break;
				case NK_A_ENTITE_ICI:
					NkEditeurCreerEntite(m, "Entite", ui.pointContexte);
					break;
				case NK_A_SIMPLE_ICI:
					PoserSansArmer(m, true, m.acteur, ui.pointContexte);
					break;
				case NK_A_ACCROCHAGE:
					{
						// Le menu garde UN interrupteur : tout eteint si l'un est allume.
						const bool un = ui.accrocheGrille || ui.accrocheAngle || ui.accrocheEchelle;
						ui.accrocheGrille = !un;
						ui.accrocheAngle = !un;
						ui.accrocheEchelle = !un;
					}
					break;
				case NK_A_CARTE_REINIT:
				case NK_A_CARTE_RETIRER:
				case NK_A_CARTE_COPIER:
				case NK_A_CARTE_COLLER:
					if (m.aSelection && ui.carteMenu >= 0 && ui.carteMenu < static_cast<int32>(NkCarteEditeur::NK_COUNT)) {
						const NkCarteEditeur carte = static_cast<NkCarteEditeur>(ui.carteMenu);
						if (action == NK_A_CARTE_REINIT) {
							NkEditeurReinitialiserCarte(m, m.selection, carte);
						} else if (action == NK_A_CARTE_COPIER) {
							NkEditeurCopierCarte(m, m.selection, carte, ui.pressePapier);
						} else if (action == NK_A_CARTE_COLLER) {
							NkEditeurCollerCarte(m, m.selection, ui.pressePapier);
						} else {
							NkComposantEditeur comp;
							if (NkComposantDeCarte(carte, comp)) {
								NkEditeurRetirerComposant(m, m.selection, comp);
							}
						}
					}
					break;
				case NK_A_CARTE_MONTER:
					NkEditeurDeplacerCarte(c, -1);
					break;
				case NK_A_CARTE_DESCENDRE:
					NkEditeurDeplacerCarte(c, 1);
					break;
				case NK_A_ACCROCHE_GRILLE:
					ui.accrocheGrille = !ui.accrocheGrille;
					break;
				case NK_A_ACCROCHE_ANGLE:
					ui.accrocheAngle = !ui.accrocheAngle;
					break;
				case NK_A_ACCROCHE_ECHELLE:
					ui.accrocheEchelle = !ui.accrocheEchelle;
					break;
				case NK_A_REPERE_LOCAL:
					ui.repereLocal = !ui.repereLocal;
					break;
				case NK_A_CREER_PREFAB:
					NkEditeurCreerPrefab(m);
					break;
				case NK_A_ANNULER:
				case NK_A_REFAIRE: {
					// L'ordre de l'Outliner suit par IDENTITE : la restauration change
					// les poignees, et l'ordre perdu remettrait les lignes en vrac.
					NkVector<uint64> ordre;
					for (uint32 i = 0; i < ui.ordreArbre.Size(); ++i) {
						ordre.PushBack(m.scene.Uid(ui.ordreArbre[i]));
					}
					if (action == NK_A_ANNULER ? NkEditeurAnnuler(m) : NkEditeurRefaire(m)) {
						ui.ordreArbre.Clear();
						for (uint32 i = 0; i < ordre.Size(); ++i) {
							const ecs::NkEntityId e = ordre[i] != 0u ? m.scene.EntiteParUid(ordre[i]) : ecs::NkEntityId::Invalid();
							if (e.IsValid()) {
								ui.ordreArbre.PushBack(e);
							}
						}
					}
					break;
				}
				case NK_A_DETACHER:
					if (m.aSelection) {
						NkEditeurDetacher(m, m.selection);
					}
					break;
				case NK_A_JOUER:
					if (m.etat == NkEtatJeu::NK_JEU) {
						NkEditeurPause(m);
					} else {
						NkEditeurJouer(m);
					}
					break;
				case NK_A_PAUSE:
					NkEditeurPause(m);
					break;
				case NK_A_ARRETER:
					NkEditeurArreter(m);
					break;
				case NK_A_PAS:
					NkEditeurUnPas(m);
					break;
				case NK_A_GRILLE:
					m.voirGrille = !m.voirGrille;
					break;
				case NK_A_COLLISIONNEURS:
					m.voirCollisionneurs = !m.voirCollisionneurs;
					break;
				case NK_A_LIENS:
					m.rendu.liens = !m.rendu.liens;
					break;
				case NK_A_PARTICULES:
					m.rendu.particules = !m.rendu.particules;
					break;
				case NK_A_VITESSES:
					m.rendu.vitesses = !m.rendu.vitesses;
					break;
				case NK_A_PAYSAGE: // portrait <-> paysage gauche, comme avant
					m.orientation = NkEstPaysage(m.orientation) ? NkOrientation::NK_PORTRAIT : NkOrientation::NK_PAYSAGE_GAUCHE;
					break;
				case NK_A_VOIR_OUTLINER:
					ui.voirOutliner = !ui.voirOutliner;
					break;
				case NK_A_VOIR_DETAILS:
					ui.voirDetails = !ui.voirDetails;
					break;
				case NK_A_VOIR_TIROIR:
					ui.voirTiroir = !ui.voirTiroir;
					break;
				case NK_A_ENTREES:
					ui.panneauEntrees = !ui.panneauEntrees;
					break;
				case NK_A_DISPOSITION:
					ui.voirOutliner = true;
					ui.voirDetails = true;
					ui.voirTiroir = true;
					ui.largeurOutliner = 250.f;
					ui.largeurDetails = 340.f;
					ui.hauteurTiroir = 250.f;
					break;
				case NK_A_RACCOURCIS: {
					// Trop de gestes pour une barre d'etat : ils s'ecrivent au
					// JOURNAL, une ligne chacun, et l'onglet s'ouvre.
					static const char *kAide[] = {
						"Raccourcis et gestes du viseur :",
						"  Clic gauche : choisir (dans le vide : plus rien)  -  glisser une entite : la deplacer (en edition)",
						"  Molette : zoom sous le curseur  -  molette PRESSEE + glisser : deplacer la vue",
						"  Clic droit : menu (Renommer, Dupliquer, Supprimer, Cadrer, Ajouter ici...)",
						"  Q selection  -  W deplacer  -  E tourner  -  R echelle  (Ctrl inverse l'accrochage)",
						"  F cadrer la selection (sans selection : toute la scene)  -  F2 renommer",
						"  Ctrl+N / O / S fichier  -  Ctrl+E entite  -  Ctrl+D dupliquer  -  Suppr",
						"  Espace jouer / pause  -  Echap arreter  -  Ctrl+Q quitter",
					};
					for (const char *ligne : kAide) {
						ui.journal.PushBack(NkString(ligne));
					}
					ui.voirTiroir = true;
					ui.ongletTiroir = 1;
					NkEditeurAnnoncer(m, "Raccourcis : voir le Journal");
					break;
				}
				case NK_A_APROPOS:
					NkEditeurAnnoncer(m, "UnkenyEditor : l'éditeur du moteur 2D Unkeny (Rihen)");
					break;
				case NK_A_CONTENU_POSER:
				case NK_A_CONTENU_ARMER:
				case NK_A_CONTENU_OUVRIR:
				case NK_A_CONTENU_RACINE:
				case NK_A_CONTENU_IMPORTER:
				case NK_A_CONTENU_EXPORTER:
					// Le navigateur connait ses chemins (NkEditeurTiroir.cpp).
					NkEditeurActionContenu(c, action);
					break;
				case NK_A_CONTENU_OUVRIR_ASSET:
					// (2026-10-01) Une SCENE du Contenu s'ouvre -- apres la question
					// « non enregistree », comme Fichier > Ouvrir.
					NkEditeurActionContenu(c, action);
					if (!c.ui.sceneAOuvrir.Empty()) {
						Demander(c, NK_A_CONTENU_OUVRIR_ASSET);
					}
					break;
				case NK_A_ARMER_SIMPLE:
					m.acteurSimple = true;
					m.outil = NkOutil::NK_POSER;
					break;
				default:
					// (2026-10-01) Les gestes du navigateur : 1306 a 1399.
					if (action >= NK_A_CONTENU_NOUVEAU_DOSSIER && action < 1400) {
						NkEditeurActionContenu(c, action);
					}
					break;
			}
		}

		// =====================================================================
		// LES BORDS DE LA FENETRE SANS CADRE
		// =====================================================================
		void NkEditeurBordsFenetre(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			// Agrandie, la fenetre n'a pas de bord a saisir : c'est la regle de
			// toutes les fenetres du systeme.
			if (ui.fenetreAgrandie) {
				return;
			}
			nkgui::NkGuiInput &in = c.ctx.input;
			const float32 W = ui.ecran.w;
			const float32 H = ui.ecran.h;
			const float32 bande = 5.f;
			const NkVec2 p = in.mousePos;
			if (p.x < 0.f || p.y < 0.f || p.x >= W || p.y >= H) {
				return; // la sentinelle « nulle part » n'est pas un bord
			}
			// Masque : 1 gauche, 2 droite, 4 haut, 8 bas -- le code de NkEditorShell.
			int32 bords = 0;
			bords |= p.x < bande ? 1 : 0;
			bords |= p.x >= W - bande ? 2 : 0;
			bords |= p.y < bande ? 4 : 0;
			bords |= p.y >= H - bande ? 8 : 0;
			if (bords == 0) {
				return;
			}
			c.ctx.wantCursor = (bords & 3) != 0 ? nkgui::NkGuiCursor::ResizeEW : nkgui::NkGuiCursor::ResizeNS;
			if (!in.mouseClicked[0]) {
				return;
			}
			// Les valeurs de NkWindow::NkResizeEdge : Left, Right, Top, Bottom,
			// TopLeft, TopRight, BottomLeft, BottomRight.
			int32 bord = -1;
			switch (bords) {
				case 1:     bord = 0; break;
				case 2:     bord = 1; break;
				case 4:     bord = 2; break;
				case 8:     bord = 3; break;
				case 1 | 4: bord = 4; break;
				case 2 | 4: bord = 5; break;
				case 1 | 8: bord = 6; break;
				case 2 | 8: bord = 7; break;
				default:    break;
			}
			if (bord >= 0) {
				ui.redimDemande = bord;
				// Le clic est au bord, pas a ce qui est dessous : un redimensionnement
				// ne doit pas, en plus, ouvrir un menu ou poser un acteur.
				in.mouseClicked[0] = false;
			}
		}

		// =====================================================================
		// LA BARRE DE TITRE : menus, titre, boutons de fenetre
		// La fenetre est SANS CADRE (NkCanvasAppConfig::frame = false), comme
		// NK3DModeler : le menu principal est sur la ligne du titre.
		// =====================================================================
		void NkEditeurDessinerBarreMenus(NkEditeurCadre &c) {
			auto &dl = c.ctx.dl;
			NkEditeurInterface &ui = c.ui;
			const NkRect &b = ui.barreMenus;
			const nkgui::NkGuiInput &in = c.ctx.input;
			dl.AddRectFilled(b, c.pal.fond);
			// Rien sous la souris qui soit un element : c'est la poignee.
			bool surElement = false;

			// ── Les menus, a gauche ───────────────────────────────────────────
			static const char *kNoms[4] = {"Fichier", "Édition", "Fenêtre", "Aide"};
			float32 x = b.x + 8.f;
			for (int32 i = 0; i < 4; ++i) {
				const NkMenuEditeur menu = static_cast<NkMenuEditeur>(i);
				const float32 w = renderer::NkTexteLargeur(c.police, kNoms[i]) + 18.f;
				const NkRect r{x, b.y + 4.f, w, b.h - 8.f};
				const bool survol = NkEditeurDans(r, in.mousePos);
				surElement = surElement || survol;
				const bool ouvert = ui.menu == menu;
				if (ouvert) {
					dl.AddRectFilled(r, c.pal.accent, 2.f);
				} else if (survol) {
					dl.AddRectFilled(r, c.pal.boutonSurvol, 2.f);
				}
				renderer::NkTexteDansBoite(dl, c.police, r, kNoms[i], ouvert ? c.pal.surAccent : c.pal.texte);
				if (survol && in.mouseClicked[0]) {
					NkEditeurOuvrirMenu(c, menu, r);
				} else if (survol && !ouvert && ui.menu >= NkMenuEditeur::NK_FICHIER &&
						   ui.menu <= NkMenuEditeur::NK_AIDE) {
					// Un menu de la barre est deja ouvert : le survol d'un voisin
					// le remplace, comme dans tout logiciel de bureau.
					ui.menu = menu;
					ui.menuAncre = r;
				}
				x += w + 2.f;
			}

			// ── Le titre, au centre : l'application, et ce qu'on edite ───────
			const NkString titre = NkString::Format("Unkeny  —  %s%s", NomScene(c.m).CStr(), ui.modifiee ? " *" : "");
			const float32 ty = b.y + (b.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f;
			renderer::NkTexteCentre(dl, c.police, b.x + b.w * 0.5f, ty, titre.CStr(), c.pal.attenue);

			// ── Reduire / agrandir / fermer, a droite ─────────────────────────
			// Dessines au trait : aucun glyphe de la police n'est suppose.
			const float32 bw = 44.f;
			for (int32 i = 0; i < 3; ++i) {
				const NkRect r{b.x + b.w - static_cast<float32>(3 - i) * bw, b.y, bw, b.h};
				const bool survol = NkEditeurDans(r, in.mousePos);
				surElement = surElement || survol;
				if (survol) {
					// La fermeture rougit au survol : c'est le seul bouton qui perd
					// quelque chose, et on le voit avant de cliquer.
					dl.AddRectFilled(r, i == 2 ? c.ctx.theme.danger : c.pal.boutonSurvol);
				}
				const NkColor t = survol ? c.pal.texte : c.pal.attenue;
				const float32 cx = r.x + r.w * 0.5f;
				const float32 cy = r.y + r.h * 0.5f;
				if (i == 0) {
					dl.AddLine(NkVec2{cx - 5.f, cy + 0.5f}, NkVec2{cx + 5.f, cy + 0.5f}, t, 1.2f);
				} else if (i == 1) {
					// Deux etats, deux dessins : un carre pour agrandir, deux carres
					// decales pour restaurer. Le meme dessin dans les deux cas
					// obligerait a se souvenir de ce qu'on a fait.
					if (ui.fenetreAgrandie) {
						dl.AddRect(NkRect{cx - 3.f, cy - 5.f, 8.f, 8.f}, t, 1.f);
						dl.AddRectFilled(NkRect{cx - 5.f, cy - 3.f, 8.f, 8.f}, survol ? c.pal.boutonSurvol : c.pal.fond);
						dl.AddRect(NkRect{cx - 5.f, cy - 3.f, 8.f, 8.f}, t, 1.f);
					} else {
						dl.AddRect(NkRect{cx - 5.f, cy - 5.f, 10.f, 10.f}, t, 1.f);
					}
				} else {
					dl.AddLine(NkVec2{cx - 5.f, cy - 5.f}, NkVec2{cx + 5.f, cy + 5.f}, t, 1.2f);
					dl.AddLine(NkVec2{cx + 5.f, cy - 5.f}, NkVec2{cx - 5.f, cy + 5.f}, t, 1.2f);
				}
				if (survol && in.mouseClicked[0] && ui.menu == NkMenuEditeur::NK_AUCUN) {
					if (i == 0) {
						ui.reduireDemande = true;
					} else if (i == 1) {
						ui.agrandirDemande = true;
					} else {
						// La MEME politique que Fichier > Quitter et la croix de l'OS :
						// une scene modifiee pose la question.
						NkEditeurExecuter(c, NK_A_QUITTER);
					}
				}
			}

			// ── La poignee : tout ce qui n'est pas un element ────────────────
			const bool dansBarre = NkEditeurDans(b, in.mousePos);
			if (dansBarre && !surElement && ui.menu == NkMenuEditeur::NK_AUCUN) {
				if (in.mouseDoubleClicked[0]) {
					ui.titreArme = false;
					ui.agrandirDemande = true;
				} else if (in.mouseClicked[0]) {
					ui.titreArme = true;
					ui.titreAppui = in.mousePos;
				}
			}
			if (ui.titreArme) {
				if (!in.mouseDown[0]) {
					ui.titreArme = false;
				} else {
					const float32 dx = in.mousePos.x - ui.titreAppui.x;
					const float32 dy = in.mousePos.y - ui.titreAppui.y;
					if (dx * dx + dy * dy > 9.f) {
						ui.titreArme = false;
						ui.deplacerDemande = true;
						const float32 w = b.w > 1.f ? b.w : 1.f;
						ui.deplacerFractionX = Borne((ui.titreAppui.x - b.x) / w, 0.f, 1.f);
					}
				}
			}
		}

		// =====================================================================
		// LES ONGLETS DE SCENE, sous la barre de titre
		// =====================================================================
		void NkEditeurDessinerOnglets(NkEditeurCadre &c) {
			auto &dl = c.ctx.dl;
			const NkRect &b = c.ui.barreOnglets;
			const nkgui::NkGuiInput &in = c.ctx.input;
			dl.AddRectFilled(b, c.pal.fond);
			dl.AddRectFilled(NkRect{b.x, b.y + b.h - 1.f, b.w, 1.f}, c.pal.bord);

			// [ ● Scene_01  ✕ ] : le point (ambre) dit « modifiee depuis
			// l'enregistrement » ; la croix ferme, et pose la question si le point
			// est la.
			const NkString nom = NomScene(c.m);
			// ⚠️ LA PLACE DU POINT EST TOUJOURS RESERVEE. Elle ne l'etait que
			//    scene modifiee : l'onglet s'elargissait de 14 px a la premiere
			//    modification, et la croix visee l'instant d'avant n'etait plus
			//    sous le curseur (mesure du 2026-09-29, le clic tombait a cote).
			const float32 point = 14.f;
			const float32 croix = 20.f;
			const float32 tw = renderer::NkTexteLargeur(c.police, nom.CStr());
			const float32 w = 12.f + point + tw + 8.f + croix;
			const NkRect onglet{b.x + 8.f, b.y + 3.f, w, b.h - 3.f};
			dl.AddRectFilled(onglet, c.pal.panneau, 2.f);
			dl.AddRectFilled(NkRect{onglet.x, onglet.y, onglet.w, 2.f}, c.pal.accent);
			const float32 ty = onglet.y + (onglet.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f;
			if (c.ui.modifiee) {
				dl.AddCircleFilled(nkgui::NkVec2{onglet.x + 12.f + 4.f, onglet.y + onglet.h * 0.5f}, 3.5f, c.pal.selection);
			}
			renderer::NkTexte(dl, c.police, onglet.x + 12.f + point, ty, nom.CStr(), c.pal.texte);
			const NkRect fermer{onglet.x + onglet.w - croix - 4.f, onglet.y + (onglet.h - 16.f) * 0.5f, 16.f, 16.f};
			const bool survolX = NkEditeurDans(fermer, in.mousePos);
			if (survolX) {
				dl.AddRectFilled(fermer, c.pal.boutonSurvol, 2.f);
			}
			// La croix est TRACEE : deux traits, jamais un glyphe que la police
			// embarquee n'aurait peut-etre pas.
			const NkColor teinteX = survolX ? c.pal.texte : c.pal.attenue;
			const float32 x0 = fermer.x + 4.5f;
			const float32 y0 = fermer.y + 4.5f;
			const float32 x1 = fermer.x + fermer.w - 4.5f;
			const float32 y1 = fermer.y + fermer.h - 4.5f;
			dl.AddLine(nkgui::NkVec2{x0, y0}, nkgui::NkVec2{x1, y1}, teinteX, 1.4f);
			dl.AddLine(nkgui::NkVec2{x1, y0}, nkgui::NkVec2{x0, y1}, teinteX, 1.4f);
			if (survolX && in.mouseClicked[0]) {
				NkEditeurExecuter(c, NK_A_FERMER_SCENE);
			}
		}

		// =====================================================================
		// LE JOURNAL
		// =====================================================================
		void NkEditeurJournaliser(NkEditeurModele &m, NkEditeurInterface &ui) {
			// Une annonce neuve remet son age a zero. Deux annonces dans la MEME
			// trame (« Scene enregistree » puis « Scene fermee ») ont toutes deux
			// l'age zero : la seconde se reconnait a son texte.
			const bool rajeunie = m.messageAge < ui.agePrecedent;
			const bool autreTexte = m.messageAge <= 0.f && !(m.message == ui.derniereAnnonce);
			if ((rajeunie || autreTexte) && !m.message.Empty()) {
				const int32 s = static_cast<int32>(ui.temps);
				ui.journal.PushBack(NkString::Format("[%02d:%02d]  %s", s / 60, s % 60, m.message.CStr()));
				ui.derniereAnnonce = m.message;
				if (ui.journal.Size() > 200u) {
					ui.journal.RemoveAt(0);
				}
			}
			ui.agePrecedent = m.messageAge;
		}

		// =====================================================================
		// LA BARRE D'OUTILS
		// Enregistrer | Outil | + Ajouter | lecture | Appareil | Reglages
		// =====================================================================
		void NkEditeurDessinerBarreOutils(NkEditeurCadre &c) {
			const NkRect &b = c.ui.barreOutils;
			c.ctx.dl.AddRectFilled(b, c.pal.entete);
			c.ctx.dl.AddRectFilled(NkRect{b.x, b.y + b.h - 1.f, b.w, 1.f}, c.pal.bord);
			// La place des deux boutons dont le libelle varie : celle du PLUS LONG
			// de leurs libelles possibles. La barre ne bouge plus quand on change
			// d'outil ou d'appareil.
			float32 largeurOutil = 0.f;
			for (int32 k = 0; k <= static_cast<int32>(NkOutil::NK_ECHELLE); ++k) {
				const NkString s = NkString::Format("Outil : %s", NomOutil(static_cast<NkOutil>(k)));
				const float32 w = renderer::NkTexteLargeur(c.police, s.CStr()) + 30.f;
				largeurOutil = w > largeurOutil ? w : largeurOutil;
			}
			float32 largeurAppareil = 0.f;
			for (int32 k = 0; k < NkNbProfils(); ++k) {
				const NkString s = NkString::Format("Appareil : %s", NkProfil(k).nom);
				const float32 w = renderer::NkTexteLargeur(c.police, s.CStr()) + 30.f;
				largeurAppareil = w > largeurAppareil ? w : largeurAppareil;
			}
			float32 x = b.x + 8.f;
			x = BoutonOutil(c, x, "Enregistrer", NkMenuEditeur::NK_AUCUN, NK_A_ENREGISTRER);
			x = Trait(c, x);
			const NkString outil = NkString::Format("Outil : %s", NomOutil(c.m.outil));
			x = BoutonOutil(c, x, outil.CStr(), NkMenuEditeur::NK_OUTIL, NK_A_AUCUNE, largeurOutil);
			x = Trait(c, x);
			x = BoutonOutil(c, x, "+ Ajouter", NkMenuEditeur::NK_AJOUTER, NK_A_AUCUNE);
			x = Trait(c, x);
			x = BoutonsLecture(c, x);
			x = Trait(c, x);
			const NkString appareil = NkString::Format("Appareil : %s", c.m.ProfilCourant().nom);
			x = BoutonOutil(c, x, appareil.CStr(), NkMenuEditeur::NK_APPAREIL, NK_A_AUCUNE, largeurAppareil);
			x = Trait(c, x);
			BoutonOutil(c, x, "Réglages", NkMenuEditeur::NK_REGLAGES, NK_A_AUCUNE);
		}

		// =====================================================================
		// LA BARRE D'ETAT : l'etat de jeu, l'annonce, les compteurs
		// =====================================================================
		void NkEditeurDessinerStatut(NkEditeurCadre &c) {
			auto &dl = c.ctx.dl;
			const NkRect &b = c.ui.statut;
			dl.AddRectFilled(b, c.pal.entete);
			dl.AddRectFilled(NkRect{b.x, b.y, b.w, 1.f}, c.pal.bord);

			// L'etat de jeu, en PASTILLE : c'est la chose a ne jamais confondre.
			// En jeu, les modifications disparaitront a l'arret.
			const char *etat = "ÉDITION";
			NkColor fond = c.pal.bouton;
			NkColor texte = c.pal.texte;
			if (c.m.etat == NkEtatJeu::NK_JEU) {
				etat = "EN JEU";
				fond = c.pal.accent;
				texte = c.pal.surAccent;
			} else if (c.m.etat == NkEtatJeu::NK_PAUSE) {
				etat = "EN PAUSE";
				fond = c.pal.selection;
				texte = c.pal.fond;
			}
			const float32 pw = renderer::NkTexteLargeur(c.petite, etat) + 16.f;
			const NkRect pastille{b.x + 6.f, b.y + 3.f, pw, b.h - 6.f};
			dl.AddRectFilled(pastille, fond, 2.f);
			renderer::NkTexteDansBoite(dl, c.petite, pastille, etat, texte);

			const float32 ligneY = b.y + (b.h - renderer::NkTexteHauteurLigne(c.petite, 12.f)) * 0.5f;
			if (c.m.messageAge < 4.f && !c.m.message.Empty()) {
				renderer::NkTexte(dl, c.petite, pastille.x + pastille.w + 10.f, ligneY, c.m.message.CStr(), c.pal.texte,
								  b.w * 0.55f);
			}

			NkVector<ecs::NkEntityId> ids;
			c.m.scene.Entites(ids);
			// ⚠️ VUS et DESSINES : l'ecart entre les deux EST la mesure du hors-champ.
			// 2026-09-30 : les particules d'effet (vivantes / plafond) et les
			// lumieres, SEULEMENT s'il y en a -- une scene sans elles garde sa barre.
			NkString effets;
			const NkEffets2D &fx = c.m.scene.Effets();
			if (fx.NbParticules() > 0u || c.m.scene.Eclairage().actif) {
				effets = NkString::Format("   ·   effets %u / %u", fx.NbParticules(), fx.plafond);
				if (c.m.scene.Eclairage().actif) {
					effets.Append(NkString::Format("   ·   lumières %d", NkEditeurDerniersChiffresLumiere().lumieres).CStr());
				}
			}
			const NkString compteurs =
				NkString::Format("%u entités   ·   sprites vus %d / dessinés %d%s   ·   %.0f ips",
								 static_cast<uint32>(ids.Size()), c.m.stats.entitesVues, c.m.stats.entitesDessinees,
								 effets.CStr(), static_cast<double>(c.ui.ips));
			renderer::NkTexteADroite(dl, c.petite, b.x + b.w - 10.f, ligneY, compteurs.CStr(), c.pal.attenue);
		}

		// =====================================================================
		// LES CLOISONS
		// =====================================================================
		void NkEditeurCloisons(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			const nkgui::NkGuiInput &in = c.ctx.input;
			NkRect cloisons[3] = {
				NkRect{ui.outliner.x + ui.outliner.w, ui.outliner.y, EPAISSEUR_CLOISON, ui.outliner.h},
				NkRect{ui.details.x - EPAISSEUR_CLOISON, ui.details.y, EPAISSEUR_CLOISON, ui.details.h},
				NkRect{0.f, ui.tiroir.y - EPAISSEUR_CLOISON, ui.ecran.w, EPAISSEUR_CLOISON},
			};
			const bool visibles[3] = {ui.voirOutliner, ui.voirDetails, ui.voirTiroir};

			if (ui.cloisonTenue >= 0) {
				if (!in.mouseDown[0]) {
					ui.cloisonTenue = -1;
				} else if (ui.cloisonTenue == 0) {
					ui.largeurOutliner = in.mousePos.x;
				} else if (ui.cloisonTenue == 1) {
					ui.largeurDetails = ui.ecran.w - in.mousePos.x;
				} else {
					ui.hauteurTiroir = ui.statut.y - in.mousePos.y;
				}
			}
			for (int32 k = 0; k < 3; ++k) {
				if (!visibles[k]) {
					continue;
				}
				// La zone de saisie deborde de 2 px de chaque cote : 4 px se
				// visent mal, et le trait visible n'a pas a grossir pour autant.
				const NkRect prise = k < 2 ? NkRect{cloisons[k].x - 2.f, cloisons[k].y, cloisons[k].w + 4.f, cloisons[k].h}
										   : NkRect{cloisons[k].x, cloisons[k].y - 2.f, cloisons[k].w, cloisons[k].h + 4.f};
				const bool survol = NkEditeurDans(prise, in.mousePos);
				const bool tenue = ui.cloisonTenue == k;
				c.ctx.dl.AddRectFilled(cloisons[k], (survol || tenue) ? c.pal.accent : c.pal.fond);
				if (survol || tenue) {
					c.ctx.wantCursor = k < 2 ? nkgui::NkGuiCursor::ResizeEW : nkgui::NkGuiCursor::ResizeNS;
				}
				if (survol && in.mouseClicked[0] && ui.cloisonTenue < 0) {
					ui.cloisonTenue = k;
				}
			}
		}

		// =====================================================================
		// LE MENU OUVERT, dans dlOverlay
		// =====================================================================
		namespace {

			/// Ce que la peinture d'une liste de menu rapporte.
			struct NkListeMenu {
					NkRect cadre{0.f, 0.f, 0.f, 0.f};
					int32 choisie = NK_A_AUCUNE;
					/// L'entree survolee ouvre ce sous-menu (NK_AUCUN sinon).
					NkMenuEditeur sousSurvole = NkMenuEditeur::NK_AUCUN;
					NkRect ligneSous{0.f, 0.f, 0.f, 0.f};
					/// Une entree SANS sous-menu est survolee : le sous-menu ouvert se ferme.
					bool survolSimple = false;
			};

			/// Mesure, place (bornee a l'ecran) et peint une liste de menu en (x, y).
			/// Si elle deborde a droite, elle se pose a gauche de `xRepli` (un
			/// sous-menu passe alors a gauche de son parent) ; `xRepli` < 0 :
			/// simplement ramenee dans l'ecran.
			NkListeMenu PeindreListe(NkEditeurCadre &c, const NkVector<NkEntreeMenu> &entrees, float32 x, float32 y,
									 float32 largeurMin, NkMenuEditeur sousOuvert, float32 xRepli) {
				NkListeMenu res;
				const nkgui::NkGuiInput &in = c.ctx.input;
				auto &dl = c.ctx.dlOverlay;
				const NkRect &ecran = c.ui.ecran;
				const float32 ligneH = renderer::NkTexteHauteurLigne(c.police, 16.f) + 7.f;
				const float32 sepH = 7.f;

				// La largeur : le plus long libelle, son raccourci, la coche, la fleche.
				float32 largeur = largeurMin > 190.f ? largeurMin : 190.f;
				float32 hauteur = 8.f;
				for (uint32 i = 0; i < entrees.Size(); ++i) {
					const NkEntreeMenu &e = entrees[i];
					if (e.separateur) {
						hauteur += sepH;
						continue;
					}
					const float32 w = renderer::NkTexteLargeur(c.police, e.libelle.CStr()) +
									  renderer::NkTexteLargeur(c.petite, e.raccourci) + 64.f;
					largeur = w > largeur ? w : largeur;
					hauteur += ligneH;
				}
				if (x + largeur > ecran.w - 2.f) {
					x = xRepli >= 0.f ? xRepli - largeur : ecran.w - 2.f - largeur;
				}
				if (y + hauteur > ecran.h - 2.f) {
					y = ecran.h - 2.f - hauteur;
				}
				x = x < 0.f ? 0.f : x;
				y = y < 0.f ? 0.f : y;
				const NkRect cadre{x, y, largeur, hauteur};
				res.cadre = cadre;

				dl.AddRectFilled(NkRect{cadre.x + 3.f, cadre.y + 4.f, cadre.w, cadre.h}, NkColor{0, 0, 0, 90}, 3.f);
				dl.AddRectFilled(cadre, c.pal.entete, 2.f);
				dl.AddRect(cadre, c.pal.bord, 1.f, 2.f);

				float32 ly = cadre.y + 4.f;
				for (uint32 i = 0; i < entrees.Size(); ++i) {
					const NkEntreeMenu &e = entrees[i];
					if (e.separateur) {
						dl.AddRectFilled(NkRect{cadre.x + 8.f, ly + sepH * 0.5f, cadre.w - 16.f, 1.f}, c.pal.bord);
						ly += sepH;
						continue;
					}
					const NkRect r{cadre.x + 3.f, ly, cadre.w - 6.f, ligneH};
					const bool aSous = e.sousMenu != NkMenuEditeur::NK_AUCUN;
					const bool cliquable = e.actif && (e.action != NK_A_AUCUNE || aSous);
					const bool survol = cliquable && NkEditeurDans(r, in.mousePos);
					// L'entree d'un sous-menu OUVERT reste surlignee pendant qu'on
					// va vers lui : sinon on ne sait plus de quoi il est la suite.
					const bool eclairee = survol || (aSous && sousOuvert == e.sousMenu);
					if (eclairee) {
						dl.AddRectFilled(r, c.pal.accent, 2.f);
					}
					if (survol && aSous) {
						res.sousSurvole = e.sousMenu;
						res.ligneSous = r;
					} else if (survol) {
						res.survolSimple = true;
					}
					const float32 ty = r.y + (r.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f;
					NkColor t = cliquable ? c.pal.texte : c.pal.attenue;
					if (eclairee) {
						t = c.pal.surAccent;
					}
					if (e.coche) {
						Coche(dl, r.x + 12.f, r.y + r.h * 0.5f, t);
					}
					renderer::NkTexte(dl, c.police, r.x + 26.f, ty, e.libelle.CStr(), t);
					if (aSous) {
						// La fleche « ▸ », tracee : la police embarquee ne la porte pas.
						const float32 fx = r.x + r.w - 12.f;
						const float32 fy = r.y + r.h * 0.5f;
						dl.AddTriangleFilled(NkVec2{fx - 2.5f, fy - 4.f}, NkVec2{fx - 2.5f, fy + 4.f}, NkVec2{fx + 2.5f, fy}, t);
					} else if (e.raccourci != nullptr && e.raccourci[0] != '\0') {
						const float32 tyP = r.y + (r.h - renderer::NkTexteHauteurLigne(c.petite, 12.f)) * 0.5f;
						renderer::NkTexteADroite(dl, c.petite, r.x + r.w - 8.f, tyP, e.raccourci,
												 eclairee ? c.pal.surAccent : c.pal.attenue);
					}
					if (survol && !aSous && in.mouseClicked[0]) {
						res.choisie = e.action;
					}
					ly += ligneH;
				}
				return res;
			}

		} // namespace

		void NkEditeurDessinerMenuOuvert(NkEditeurCadre &c, NkMenuEditeur menuDebut) {
			NkEditeurInterface &ui = c.ui;
			if (ui.menu == NkMenuEditeur::NK_AUCUN) {
				ui.sousMenu = NkMenuEditeur::NK_AUCUN;
				return;
			}
			const nkgui::NkGuiInput &in = c.ctx.input;
			NkVector<NkEntreeMenu> entrees;
			RemplirMenu(c, ui.menu, entrees);
			// « Ajouter un composant » se CHERCHE (Unity) : on tape, la liste filtre,
			// Entree prend la premiere ligne restante.
			int32 premiere = NK_A_AUCUNE;
			if (ui.menu == NkMenuEditeur::NK_COMPOSANT && ui.sousMenu == NkMenuEditeur::NK_AUCUN) {
				usize n = 0;
				while (ui.menuFiltre[n] != '\0') {
					++n;
				}
				for (int32 i = 0; i < in.charCount; ++i) {
					const uint32 cp = in.chars[i];
					if (cp >= 32u && cp < 127u && n + 1u < sizeof(ui.menuFiltre)) {
						ui.menuFiltre[n++] = static_cast<char>(cp);
						ui.menuFiltre[n] = '\0';
					}
				}
				if (in.KeyPressedRepeat(nkgui::NkGuiKey::Backspace) && n > 0u) {
					ui.menuFiltre[--n] = '\0';
				}
				auto minuscule = [](char ch) { return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch; };
				auto contient = [&](const char *texte) {
					if (n == 0u) {
						return true;
					}
					for (const char *t = texte; *t != '\0'; ++t) {
						usize k = 0;
						while (k < n && t[k] != '\0' && minuscule(t[k]) == minuscule(ui.menuFiltre[k])) {
							++k;
						}
						if (k == n) {
							return true;
						}
					}
					return false;
				};
				NkVector<NkEntreeMenu> gardees;
				gardees.PushBack(Intitule(n > 0u ? NkString::Format("Rechercher : %s_", ui.menuFiltre).CStr() : "Rechercher : tapez un nom"));
				gardees.PushBack(Separateur());
				for (uint32 i = 0; i < entrees.Size(); ++i) {
					const NkEntreeMenu &e = entrees[i];
					// Filtre pose, seules restent les lignes qui AGISSENT et qui y repondent.
					if (n > 0u && (e.separateur || e.action == NK_A_AUCUNE || !contient(e.libelle.CStr()))) {
						continue;
					}
					if (premiere == NK_A_AUCUNE && e.action != NK_A_AUCUNE && e.actif) {
						premiere = e.action;
					}
					gardees.PushBack(e);
				}
				if (n > 0u && premiere == NK_A_AUCUNE) {
					gardees.PushBack(Intitule("(aucun composant de ce nom)"));
				}
				entrees = gardees;
			}
			const NkListeMenu principal = PeindreListe(c, entrees, ui.menuAncre.x, ui.menuAncre.y + ui.menuAncre.h,
													   ui.menuAncre.w, ui.sousMenu, -1.f);
			ui.menuRect = principal.cadre;
			// Le sous-menu suit le SURVOL : il s'ouvre sur son entree, et se ferme
			// quand on en survole une autre. Rester dessus (ou sur le vide entre les
			// deux) le garde ouvert.
			if (principal.sousSurvole != NkMenuEditeur::NK_AUCUN) {
				ui.sousMenu = principal.sousSurvole;
				ui.sousMenuLigne = principal.ligneSous;
			} else if (principal.survolSimple) {
				ui.sousMenu = NkMenuEditeur::NK_AUCUN;
			}
			NkListeMenu sous;
			if (ui.sousMenu != NkMenuEditeur::NK_AUCUN) {
				NkVector<NkEntreeMenu> sousEntrees;
				RemplirMenu(c, ui.sousMenu, sousEntrees);
				sous = PeindreListe(c, sousEntrees, principal.cadre.x + principal.cadre.w - 2.f, ui.sousMenuLigne.y - 4.f,
									170.f, NkMenuEditeur::NK_AUCUN, principal.cadre.x + 2.f);
				ui.sousMenuRect = sous.cadre;
			} else {
				ui.sousMenuRect = NkRect{0.f, 0.f, 0.f, 0.f};
			}

			int32 choisie = principal.choisie != NK_A_AUCUNE ? principal.choisie : sous.choisie;
			if (choisie == NK_A_AUCUNE && premiere != NK_A_AUCUNE && in.KeyPressed(nkgui::NkGuiKey::Enter)) {
				choisie = premiere;
			}
			if (choisie != NK_A_AUCUNE) {
				// Le menu du NAVIGATEUR vise l'element de son clic droit ; tout autre
				// menu (Fichier > Importer…) vise le navigateur tel qu'il est -- pas
				// un element d'un clic droit oublie.
				if (ui.menu != NkMenuEditeur::NK_CTX_CONTENU && ui.menu != NkMenuEditeur::NK_CTX_CONTENU_VIDE) {
					ui.contenuMenuChemin = NkString();
					ui.contenuMenuDossier = false;
				}
				ui.menu = NkMenuEditeur::NK_AUCUN;
				ui.sousMenu = NkMenuEditeur::NK_AUCUN;
				NkEditeurExecuter(c, choisie);
				return;
			}
			// Un clic HORS du menu (et de son sous-menu) le ferme -- sauf si ce menu
			// vient de s'ouvrir sur ce clic meme (il n'etait pas celui du debut de
			// trame : c'est le cas du clic droit qui ouvre le menu contextuel).
			const bool clic = in.mouseClicked[0] || in.mouseClicked[1] || in.mouseClicked[2];
			const bool dedans = NkEditeurDans(principal.cadre, in.mousePos) ||
								(ui.sousMenu != NkMenuEditeur::NK_AUCUN && NkEditeurDans(ui.sousMenuRect, in.mousePos));
			if (clic && ui.menu == menuDebut && !dedans) {
				ui.menu = NkMenuEditeur::NK_AUCUN;
				ui.sousMenu = NkMenuEditeur::NK_AUCUN;
			}
		}

	} // namespace editeur
} // namespace nkentseu
