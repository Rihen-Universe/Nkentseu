#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiCatalogue.h
// @Brief   LE CATALOGUE DES COMPOSANTS `.nkgui` : famille, catégorie, héritage,
//          et l'extrait à coller. **Extensible par greffon.**
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE
// =============================================================================
//  Rodolf, 28/09 : « ce que tu fais avec les icônes doit être fait avec les
//  composants, et classé par catégorie — widget, conteneur, fenêtre — et
//  sous-catégorie : bouton, vbox, hbox, onglet, etc. »
//
//  Puis, le même jour : « n'oublie pas, les utilisateurs pourraient grâce aux
//  greffons ajouter leurs propres catégories et sous-catégories, et ajouter
//  leurs propres composants qui héritent ou non de composants existants. »
//
//  🔴 CETTE SECONDE PHRASE CHANGE LA NATURE DE L'OBJET. Une TABLE figée aurait
//     répondu à la première et fermé la porte à la seconde : un greffon ne peut
//     pas s'ajouter à un tableau `static const`. C'est un REGISTRE — la table
//     des 54 rôles du format en est le contenu de départ, pas la totalité.
//
// =============================================================================
//  ⚠️ CE CATALOGUE EST UNE SECONDE LISTE, ET C'EST UN DANGER NOMMÉ
// =============================================================================
//  La VÉRITÉ des rôles est `NkGuiRoleDepuisNom` : c'est elle que le monteur
//  consulte. Les entrées INTÉGRÉES d'ici en sont une seconde liste — et *deux
//  listes qui peuvent se contredire finissent par le faire*.
//
//  D'où `NkGuiCatalogueVerifier`, dans les DEUX sens :
//    · tout nom INTÉGRÉ doit se RÉSOUDRE (sinon il annonce un rôle mort) ;
//    · tout rôle du format doit être AU catalogue (sinon il est invisible).
//  Un seul sens ne vaudrait rien : le premier laisserait un rôle neuf hors du
//  catalogue pour toujours, sans que personne ne le sache.
//
//  ⚠️ ET LES ENTRÉES DE GREFFON NE SONT PAS SOUMISES AU PREMIER SENS : un
//     composant apporté n'est pas un rôle du monteur, c'est un `component` du
//     document. Leur demander de se résoudre les refuserait toutes.
//
// =============================================================================
//  ⚠️ LE REGISTRE COPIE LES CHAÎNES, IL NE LES POINTE PAS
// =============================================================================
//  Un greffon se DÉCHARGE. S'il enregistrait des `const char*` de son propre
//  module, le catalogue pointerait dans de la mémoire libérée au premier
//  déchargement — un défaut qui ne se voit qu'après coup, et qui plante
//  ailleurs. Les entrées sont donc copiées dans des `NkString`.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKGui/Doc/NkGuiMonteur.h"

namespace nkentseu {
	namespace nkgui {

		/// La FAMILLE d'un composant — le premier niveau demandé par Rodolf.
		///
		/// ⚠️ `Greffon` EXISTE POUR CE QUI ARRIVE DU DEHORS SANS SE RANGER. Un
		///    greffon peut choisir une famille existante (son bouton va chez les
		///    Widgets, c'est sa place) ou la sienne. Forcer les cinq familles du
		///    format aurait interdit une catégorie neuve — exactement ce que
		///    Rodolf demande de permettre.
		enum class NkGuiFamille : uint8 {
			Fenetre = 0, ///< ce qui porte une surface : Window, Panel, DockSpace…
			Conteneur,	 ///< ce qui range d'autres composants : VBox, HBox, Grid…
			Widget,		 ///< ce qui se voit et s'actionne : Button, Slider…
			Menu,		 ///< la famille des menus, qui a ses propres règles
			Special,	 ///< ni l'un ni l'autre : Host, Canvas…
			Greffon,	 ///< une famille apportée : son nom vit dans `familleNom`
			Count
		};

		inline const char *NkGuiNomDeFamille(NkGuiFamille f) noexcept {
			switch (f) {
				case NkGuiFamille::Fenetre:
					return "Fenêtres";
				case NkGuiFamille::Conteneur:
					return "Conteneurs";
				case NkGuiFamille::Widget:
					return "Widgets";
				case NkGuiFamille::Menu:
					return "Menus";
				case NkGuiFamille::Greffon:
					return "Greffons";
				default:
					return "Spéciaux";
			}
		}

		/// Une entrée du catalogue.
		///
		/// ⚠️ `extrait` EST DU `.nkgui` VALIDE, PAS UNE ILLUSTRATION. Il part tel
		///    quel dans le presse-papiers : un extrait approximatif ferait écrire
		///    un document que le lecteur refuse, et l'utilisateur accuserait le
		///    format.
		struct NkGuiRoleInfo {
				NkString nom;		 ///< le mot écrit dans le document
				NkString categorie;	 ///< la sous-catégorie (Boutons, Saisie…)
				NkString resume;	 ///< une phrase, pas un paragraphe
				NkString extrait;	 ///< à coller dans un `.nkgui`
				NkString herite;	 ///< le composant dont il hérite (vide = aucun)
				NkString provenance; ///< « système » ou le nom du greffon
				NkString familleNom; ///< rempli quand `famille == Greffon`
				NkGuiFamille famille = NkGuiFamille::Widget;
				bool integre = true; ///< true = rôle du format ; false = apporté

				/// Le nom de famille À AFFICHER, quelle que soit la provenance.
				const char *Famille() const noexcept {
					if (famille == NkGuiFamille::Greffon && familleNom.Size() > 0u)
						return familleNom.CStr();
					return NkGuiNomDeFamille(famille);
				}
		};

		// =====================================================================
		//  LE REGISTRE
		// =====================================================================
		class NkGuiCatalogueRegistre {
			public:
				uint32 Taille() const noexcept {
					return (uint32)mEntrees.Size();
				}

				const NkGuiRoleInfo *At(uint32 i) const noexcept {
					return i < (uint32)mEntrees.Size() ? &mEntrees[i] : nullptr;
				}

				const NkGuiRoleInfo *Trouver(const char *nom) const noexcept {
					if (!nom || !*nom)
						return nullptr;
					for (uint32 i = 0; i < (uint32)mEntrees.Size(); ++i)
						if (mEntrees[i].nom.Compare(NkString(nom)) == 0)
							return &mEntrees[i];
					return nullptr;
				}

				/// Ajoute une entrée. Rend faux — et n'ajoute RIEN — si :
				///   · le nom est vide ou déjà pris (un doublon rendrait le clic
				///     ambigu et l'extrait imprévisible) ;
				///   · `herite` désigne un composant que le catalogue ne connaît
				///     pas (un héritage d'un parent absent est une promesse vide).
				///
				/// ⚠️ IL REFUSE PLUTÔT QUE D'ÉCRASER. Un greffon qui redéfinirait
				///    `Button` changerait l'extrait de TOUT le monde sans que
				///    personne ne l'ait demandé. Qu'il choisisse un autre nom.
				bool Ajouter(const NkGuiRoleInfo &e) noexcept {
					if (e.nom.Size() == 0u)
						return false;
					if (Trouver(e.nom.CStr()))
						return false;
					if (e.herite.Size() > 0u && !Trouver(e.herite.CStr()))
						return false;
					mEntrees.PushBack(e);
					return true;
				}

				/// Retire tout ce qu'un greffon a apporté — à appeler quand il se
				/// décharge. ⚠️ SANS ÇA, UN GREFFON DÉCHARGÉ RESTE AU CATALOGUE :
				///    on cliquerait un composant que plus rien ne sait monter.
				uint32 RetirerProvenance(const char *provenance) noexcept {
					if (!provenance || !*provenance)
						return 0u;
					NkVector<NkGuiRoleInfo> garde;
					uint32 retires = 0u;
					for (uint32 i = 0; i < (uint32)mEntrees.Size(); ++i) {
						if (mEntrees[i].provenance.Compare(NkString(provenance)) == 0) {
							++retires;
							continue;
						}
						garde.PushBack(mEntrees[i]);
					}
					mEntrees = garde;
					return retires;
				}

				void Vider() noexcept {
					mEntrees.Clear();
				}

			private:
				NkVector<NkGuiRoleInfo> mEntrees;
		};

		namespace detail {

			inline NkGuiRoleInfo NkGCatEntree(const char *nom, NkGuiFamille fam,
											  const char *categorie, const char *resume,
											  const char *extrait) noexcept {
				NkGuiRoleInfo e;
				e.nom = NkString(nom);
				e.famille = fam;
				e.categorie = NkString(categorie);
				e.resume = NkString(resume);
				e.extrait = NkString(extrait);
				e.provenance = NkString("système");
				e.integre = true;
				return e;
			}

			/// Les 54 rôles du format, dans l'ordre où le panneau les montre.
			/// ⚠️ L'ORDRE EST LE GROUPEMENT : le panneau pose un titre quand la
			///    famille ou la catégorie change. Trier ailleurs donnerait un
			///    second ordre à tenir d'accord avec celui-ci.
			inline void NkGCatRemplirIntegres(NkGuiCatalogueRegistre &r) noexcept {
				using F = NkGuiFamille;
				auto A = [&](const char *n, F f, const char *c, const char *s, const char *x) {
					(void)r.Ajouter(NkGCatEntree(n, f, c, s, x));
				};
				// ── FENÊTRES ────────────────────────────────────────────────
				A("Window", F::Fenetre, "Surfaces", "Une fenêtre : le seul rôle qui honore pos/size.",
				  "Window \"fenetre\" { pos = \"0 0\", size = \"400 300\" }");
				A("Panel", F::Fenetre, "Surfaces", "Un panneau titré, ancrable dans le dock.",
				  "Panel \"panneau\" { title = \"Mon panneau\" }");
				A("DockSpace", F::Fenetre, "Surfaces", "La zone où les panneaux s'ancrent.",
				  "DockSpace \"dock\" {}");
				A("Overlay", F::Fenetre, "Surfaces", "Une couche par-dessus, sans place réservée.",
				  "Overlay \"surimpression\" {}");
				// ── CONTENEURS ──────────────────────────────────────────────
				A("VBox", F::Conteneur, "Boîtes", "Empile ses enfants du haut vers le bas.",
				  "VBox \"colonne\" { gap = 6 }");
				A("HBox", F::Conteneur, "Boîtes", "Aligne ses enfants de gauche à droite.",
				  "HBox \"ligne\" { gap = 6 }");
				A("Flow", F::Conteneur, "Boîtes", "Comme HBox, mais revient à la ligne.",
				  "Flow \"flux\" { gap = 6 }");
				A("Grid", F::Conteneur, "Boîtes", "Une grille à N colonnes.",
				  "Grid \"grille\" { columns = 3, gap = 6 }");
				A("Stack", F::Conteneur, "Boîtes", "Superpose ses enfants au même endroit.",
				  "Stack \"pile\" {}");
				A("Group", F::Conteneur, "Groupes", "Un cadre titré autour d'un groupe.",
				  "Group \"groupe\" { label = \"Réglages\" }");
				A("Expander", F::Conteneur, "Groupes", "Une section qui se replie.",
				  "Expander \"section\" { label = \"Avancé\" }");
				A("Splitter", F::Conteneur, "Groupes", "Deux zones et une poignée entre elles.",
				  "Splitter \"separateur\" {}");
				A("Scroll", F::Conteneur, "Groupes", "Une zone qui défile quand ça déborde.",
				  "Scroll \"defilement\" {}");
				A("Center", F::Conteneur, "Cadrage", "Centre son enfant dans la place reçue.",
				  "Center \"centre\" {}");
				A("Padding", F::Conteneur, "Cadrage", "Une marge intérieure autour de l'enfant.",
				  "Padding \"marge\" { pad = 8 }");
				A("Aspect", F::Conteneur, "Cadrage", "Tient un rapport largeur/hauteur.",
				  "Aspect \"rapport\" { ratio = 1.777 }");
				A("TabBar", F::Conteneur, "Onglets", "Une barre d'onglets et son contenu.",
				  "TabBar \"onglets\" {}");
				A("Table", F::Conteneur, "Listes", "Un tableau à colonnes.",
				  "Table \"tableau\" { columns = 3 }");
				A("ListBox", F::Conteneur, "Listes", "Une liste d'éléments sélectionnables.",
				  "ListBox \"liste\" {}");
				A("Item", F::Conteneur, "Listes", "Une ligne de liste.",
				  "Item \"ligne\" { label = \"Élément\" }");
				A("TreeItem", F::Conteneur, "Listes", "Un nœud d'arbre, qui peut en contenir.",
				  "TreeItem \"noeud\" { label = \"Dossier\" }");
				// ── WIDGETS ─────────────────────────────────────────────────
				A("Text", F::Widget, "Texte", "Du texte. Accepte `@t:clé` pour la traduction.",
				  "Text \"titre\" { text = \"Bonjour\" }");
				A("Badge", F::Widget, "Texte", "Une pastille de texte courte.",
				  "Badge \"etat\" { label = \"neuf\" }");
				A("Button", F::Widget, "Boutons", "Un bouton. Son identifiant NOMME l'action.",
				  "Button \"mon.action\" { label = \"Valider\" }");
				A("RepeatButton", F::Widget, "Boutons", "Un bouton qui se répète si on le tient.",
				  "RepeatButton \"incr\" { label = \"+\" }");
				A("ImageButton", F::Widget, "Boutons", "Un bouton dont le visage est une image.",
				  "ImageButton \"img\" { image = \"icone.png\" }");
				A("ToggleButton", F::Widget, "Boutons", "Un bouton qui reste enfoncé.",
				  "ToggleButton \"gras\" { label = \"G\" }");
				A("SplitButton", F::Widget, "Boutons", "Une action, plus un chevron de variantes.",
				  "SplitButton \"exporter\" { label = \"Exporter\" }");
				A("Checkbox", F::Widget, "Choix", "Une case à cocher.",
				  "Checkbox \"visible\" { label = \"Visible\", checked = true }");
				A("Switch", F::Widget, "Choix", "Un interrupteur : la même chose, en plus lisible.",
				  "Switch \"actif\" { label = \"Actif\" }");
				A("RadioGroup", F::Widget, "Choix", "Un choix exclusif parmi N.",
				  "RadioGroup \"mode\" { options = [\"Un\", \"Deux\"] }");
				A("Dropdown", F::Widget, "Choix", "Une liste déroulante.",
				  "Dropdown \"choix\" { options = [\"A\", \"B\"] }");
				A("TextField", F::Widget, "Saisie", "Un champ de texte.",
				  "TextField \"nom\" { placeholder = \"Nom…\" }");
				A("NumberField", F::Widget, "Saisie", "Un champ numérique.",
				  "NumberField \"largeur\" { value = 100 }");
				A("TokenField", F::Widget, "Saisie", "Des étiquettes qu'on ajoute et retire.",
				  "TokenField \"mots\" {}");
				A("TimecodeField", F::Widget, "Saisie", "Un temps, en heures:minutes:images.",
				  "TimecodeField \"temps\" {}");
				A("VectorField", F::Widget, "Saisie", "Deux à quatre nombres d'un coup.",
				  "VectorField \"position\" { size = 2 }");
				A("ColorField", F::Widget, "Saisie", "Une couleur et son sélecteur.",
				  "ColorField \"fond\" { value = \"#2196F3\" }");
				A("Slider", F::Widget, "Réglages", "Une valeur entre deux bornes.",
				  "Slider \"opacite\" { min = 0, max = 1, value = 1 }");
				A("Drag", F::Widget, "Réglages", "Une valeur qu'on tire à la souris.",
				  "Drag \"vitesse\" { value = 1 }");
				A("CurveField", F::Widget, "Réglages", "Une courbe éditable.",
				  "CurveField \"courbe\" {}");
				A("KeyDiamond", F::Widget, "Réglages", "Le losange d'une clé d'animation.",
				  "KeyDiamond \"cle\" {}");
				A("Progress", F::Widget, "Retour", "Une barre de progression.",
				  "Progress \"avancement\" { value = 0.5 }");
				A("Image", F::Widget, "Retour", "Une image, chargée par l'hôte.",
				  "Image \"visuel\" { image = \"photo.png\" }");
				A("Chart", F::Widget, "Retour", "Un graphique simple.", "Chart \"courbes\" {}");
				A("Tile", F::Widget, "Retour", "Une tuile cliquable : image et légende.",
				  "Tile \"tuile\" { label = \"Modèle\" }");
				A("Separator", F::Widget, "Mise en page", "Un trait. Il suit l'AXE de son conteneur.",
				  "Separator \"sep\" {}");
				A("Spacer", F::Widget, "Mise en page", "Un vide. Sans `size`, il prend le reste.",
				  "Spacer \"vide\" { size = 12 }");
				// ── MENUS ───────────────────────────────────────────────────
				A("MenuBar", F::Menu, "Menus", "La bande des menus. Porte `maxMenus`.",
				  "MenuBar \"barre\" { maxMenus = 9 }");
				A("Menu", F::Menu, "Menus", "Un menu déroulant, imbricable.",
				  "Menu \"fichier\" { label = \"Fichier\" }");
				A("MenuItem", F::Menu, "Menus", "Une entrée. Son identifiant NOMME l'action.",
				  "MenuItem \"mon.action\" { label = \"Ouvrir\", shortcut = \"Ctrl+O\" }");
				A("ContextMenu", F::Menu, "Menus", "Le menu du clic droit ; l'hôte l'ouvre.",
				  "ContextMenu \"menu.scene\" {}");
				// ── SPÉCIAUX ────────────────────────────────────────────────
				A("Host", F::Special, "Hôte", "Un trou que l'application peint elle-même.",
				  "Host \"ma.zone\" { hint: \"ce que l'hôte y met\" }");
				A("Canvas", F::Special, "Hôte", "Une surface que le C++ dessine et écoute.",
				  "Canvas \"toile\" {}");
			}

		} // namespace detail

		/// LE registre du programme. Rempli des rôles du format au premier appel ;
		/// les greffons s'y ajoutent ensuite.
		///
		/// ⚠️ UN SEUL REGISTRE, ET C'EST LE POINT : un greffon qui s'enregistrerait
		///    dans une copie n'apparaîtrait nulle part.
		inline NkGuiCatalogueRegistre &NkGuiCatalogueGlobal() noexcept {
			static NkGuiCatalogueRegistre s_reg;
			static bool s_rempli = false;
			if (!s_rempli) {
				s_rempli = true;
				detail::NkGCatRemplirIntegres(s_reg);
			}
			return s_reg;
		}

		/// Enregistre un composant apporté par un greffon.
		/// `herite` peut être nul ; s'il est donné, le parent DOIT exister.
		inline bool NkGuiCatalogueAjouterGreffon(const char *nom, const char *famille,
												 const char *categorie, const char *resume,
												 const char *extrait, const char *herite,
												 const char *provenance) noexcept {
			NkGuiRoleInfo e;
			e.nom = NkString(nom ? nom : "");
			e.categorie = NkString(categorie ? categorie : "Divers");
			e.resume = NkString(resume ? resume : "");
			e.extrait = NkString(extrait ? extrait : "");
			e.herite = NkString(herite ? herite : "");
			e.provenance = NkString(provenance && *provenance ? provenance : "greffon");
			e.integre = false;
			// La famille : un nom connu s'y range, tout autre nom CRÉE la sienne.
			// ⚠️ C'est la demande exacte de Rodolf — « ajouter leurs propres
			//    catégories et sous-catégories ». Forcer les cinq familles du
			//    format aurait interdit la sienne.
			const char *f = famille ? famille : "";
			if (NkGMotEgal(NkStringView(f), "Fenêtres") || NkGMotEgal(NkStringView(f), "Fenetres"))
				e.famille = NkGuiFamille::Fenetre;
			else if (NkGMotEgal(NkStringView(f), "Conteneurs"))
				e.famille = NkGuiFamille::Conteneur;
			else if (NkGMotEgal(NkStringView(f), "Widgets"))
				e.famille = NkGuiFamille::Widget;
			else if (NkGMotEgal(NkStringView(f), "Menus"))
				e.famille = NkGuiFamille::Menu;
			else if (NkGMotEgal(NkStringView(f), "Spéciaux") || NkGMotEgal(NkStringView(f), "Speciaux"))
				e.famille = NkGuiFamille::Special;
			else {
				e.famille = NkGuiFamille::Greffon;
				e.familleNom = NkString(f[0] ? f : "Greffons");
			}
			return NkGuiCatalogueGlobal().Ajouter(e);
		}

		/// Ce qu'une vérification du catalogue a trouvé.
		struct NkGuiCatalogueRapport {
				uint32 entrees = 0;
				uint32 integrees = 0;
				uint32 apportees = 0;
				uint32 nomsInconnus = 0;	///< INTÉGRÉ, mais le monteur ne le résout pas
				uint32 rolesSansEntree = 0; ///< rôle du format absent du catalogue
				uint32 heritagesOrphelins = 0; ///< `herite` vers un parent absent
				NkString premierNomInconnu;
				NkString premierRoleSansEntree;
		};

		/// 🔴 DANS LES DEUX SENS, ET LE SECOND EST CELUI QU'ON OUBLIE.
		inline bool NkGuiCatalogueVerifier(NkGuiCatalogueRapport &rap) noexcept {
			NkGuiCatalogueRegistre &reg = NkGuiCatalogueGlobal();
			rap.entrees = reg.Taille();
			for (uint32 i = 0; i < rap.entrees; ++i) {
				const NkGuiRoleInfo *e = reg.At(i);
				if (!e)
					continue;
				if (e->integre) {
					++rap.integrees;
					// ⚠️ SEULES LES ENTRÉES INTÉGRÉES DOIVENT SE RÉSOUDRE : un
					//    composant de greffon est un `component` du document, pas
					//    un rôle du monteur. L'exiger les refuserait toutes.
					if (NkGuiRoleDepuisNom(NkStringView(e->nom.CStr())) == NkGuiRole::Inconnu) {
						++rap.nomsInconnus;
						if (rap.premierNomInconnu.Size() == 0u)
							rap.premierNomInconnu = e->nom;
					}
				} else {
					++rap.apportees;
				}
				if (e->herite.Size() > 0u && !reg.Trouver(e->herite.CStr()))
					++rap.heritagesOrphelins;
			}
			static const char *const kNoms[] = {
				"Window", "Panel", "VBox", "HBox", "Group", "Text", "Button", "RepeatButton",
				"Checkbox", "Slider", "TextField", "Dropdown", "Progress", "Separator", "Spacer",
				"Image", "Chart", "TabBar", "Expander", "Splitter", "ListBox", "Item", "TreeItem",
				"DockSpace", "Host", "Canvas", "Scroll", "MenuBar", "Menu", "MenuItem", "Flow",
				"Grid", "NumberField", "Drag", "ColorField", "Switch", "RadioGroup", "ImageButton",
				"Stack", "Table", "ToggleButton", "Badge", "Tile", "KeyDiamond", "SplitButton",
				"TimecodeField", "VectorField", "TokenField", "ContextMenu", "CurveField",
				"Center", "Padding", "Aspect", "Overlay"};
			for (uint32 r = 0; r < (uint32)(sizeof(kNoms) / sizeof(kNoms[0])); ++r) {
				if (reg.Trouver(kNoms[r]))
					continue;
				++rap.rolesSansEntree;
				if (rap.premierRoleSansEntree.Size() == 0u)
					rap.premierRoleSansEntree = NkString(kNoms[r]);
			}
			return rap.nomsInconnus == 0u && rap.rolesSansEntree == 0u
				   && rap.heritagesOrphelins == 0u;
		}

	} // namespace nkgui
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
