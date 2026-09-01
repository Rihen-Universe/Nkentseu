#pragma once
// -----------------------------------------------------------------------------
// @File    Panels.h
// @Brief   Les panneaux de NkUIDesign : palette, composition, apercu, proprietes, IA.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI EST DEDANS, ET CE QUI N'Y EST PAS
// =============================================================================
//  Etat au 2026-08-18. La tranche verticale du 18/08 (un composant, ses
//  reglages, sa sauvegarde) est devenue une APPLICATION :
//
//    - **palette**     : les composants DECLARES, lus dans le registre, poses
//                        dans le document ;
//    - **composition** : l'arbre — un composant DANS un autre, meme mecanisme
//                        aux deux echelles ;
//    - **agencement**  : taille et disposition a la souris, qui ecrivent des
//                        PROPRIETES et jamais une coordonnee ;
//    - **document**    : une interface complete se charge et se sauve, avec sa
//                        PROVENANCE posee des la creation ;
//    - **IA**          : la place — un prompt, un backend remplacable, et une
//                        sortie qui passe par la meme porte que la main.
//
//  ⚠️ HORS TRANCHE, nomme, differe, volontairement absent :
//    - les **blueprints** : les evenements sont declares avec leur charge, rien
//      ne s'y branche encore ;
//    - la **creation de composants ex nihilo** : on compose ce qui est declare ;
//    - le **dessin d'icones** ;
//    - l'**ancrage** au sens complet (marges par bord) — cf. `Document.h` ;
//    - le **modele specialise** — cf. `DesignAI.h`, qui livre sa place, pas lui.
//
// REGLE DE LECTURE POUR CE FICHIER : **aucun panneau ne connait le nom d'un
//   composant.** Palette, arbre, reglages et catalogue d'IA bouclent tous sur le
//   registre ou sur les tables de la declaration. Le seul endroit du programme ou
//   un nom de composant est ecrit en clair est `Renderers.h`, parce qu'il faut
//   bien appeler une fonction — et ce fichier-la dit pourquoi, et ce qu'il
//   faudrait pour s'en passer.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKEditorKit/NkEditorContextMenu.h"	// NkCtxMenu — le menu contextuel du kit (3e consommateur)
#include "NKEditorKit/NkEditorInspectorFrame.h" // LA charpente d inspecteur (kit)
#include "NKEditorKit/NkEditorTooltip.h"		// NkTooltip(ctx, survol, texte) — infobulle sans widget
#include "NKEditorKit/NkTheme.h"
#include "NKFileSystem/NkFile.h"
// ⚠️ LES PLAFONDS DU KIT DOIVENT CRIER (kMaxComponents = 64, kMaxDepth = 64) :
//    un panneau qui les franchit JOURNALISE. Sans NKLogger ici, il ne pourrait
//    que se taire ou refuser — et se taire est exactement ce qui est interdit.
#include "NKLogger/NkLog.h"
#include "NKTime/NkChrono.h" // l'instrument de fluidite (mandat 01/09)
// ⚠️ LES PLAFONDS DU KIT DOIVENT CRIER (kMaxComponents = 64, kMaxDepth = 64) :
//    un panneau qui les franchit JOURNALISE. Sans NKLogger ici, il ne pourrait
//    que se taire ou refuser — et se taire est exactement ce qui est interdit.
#include "NKLogger/NkLog.h"

#include "Canvas.h"
#include "Costume.h" // le costume exact Banani (polices + icônes, remandat 31/08)
#include "Historique.h" // l'annulation unifiée (§7) — instantanés de sérialisation
#include "MenuFormat.h" // le catalogue des formats de page (chantier Cible, 31/08)
#include "MenuRole.h" // le menu des rôles (écrans 5-6-7) — le geste « promouvoir »
#include "Selection.h"
#include "SelectionGeste.h" // le contrat de selection : les DEUX tables, cote a cote
#include "MenuContexte.h" // le menu du clic droit (Lunacy) : LA table, sans NKGui
#include "Snap.h" // l'aimantation Lunacy — un MECANISME, pas un dessin
#include "Transfo.h" // rotation et miroirs : le MEME calcul pour le dessin et le clic
#include "DesignAI.h"
#include "Renderers.h"

#include <cstdio>

namespace nkuidesign {

	using namespace nkentseu::editorkit;
	using namespace nkentseu::nkgui;

	static const char *kDocumentPath = "nkuidesign_document.nkuidoc";

	// ═══════════════════════════════════════════════════════════════════════════
	//  LE VOCABULAIRE DE MISE EN PAGE — ce qui manquait pour que ca RESSEMBLE a
	//  une interface
	// ═══════════════════════════════════════════════════════════════════════════
	//  ⚠️ NE PAS CONFONDRE AVEC UN COMPOSANT. Ce qui suit n'est ni declare, ni
	//     enregistrable, ni editable : ce sont des raccourcis d'AGENCEMENT pour
	//     les panneaux de l'editeur (le « mobilier »), bases sur des conteneurs
	//     que NKGui fournit deja. Aucun widget n'est reimplemente ici -- la regle
	//     du kit tient : on ASSEMBLE, on ne recree pas.
	//
	//  ⚠️ POURQUOI IL EXISTE, ET C'EST UNE DETTE ASSUMEE PUBLIQUEMENT. Rodolf,
	//     devant l'ecran du 18/08 : « la partie gauche et droite de cette
	//     interface est voulue, ou on est juste en train de faire des tests ? »
	//     La reponse honnete etait : **c'etait le mecanisme rendu visible**. Une
	//     pile de boutons pleine largeur et une liste de valeurs cliquables
	//     prouvent qu'une declaration se lit, se modifie et se sauve ; elles ne
	//     ressemblent pas a une interface. La regle qui en est sortie dit les deux
	//     devoirs : **le dire sans attendre la question**, et **ne pas s'y
	//     arreter**.
	//
	//  ⚠️ LE SYMPTOME QUI TRAHIT UN ECRAN NON DESSINE : les libelles tronques.
	//     « Rentrer dans le voisin du dessu… », « Metriques du document (px
	//     logiqu… », « max (<= mir… ». Aucune de ces coupes n'est un bug de
	//     rendu : c'est **l'absence de decision de mise en page**. Un bouton pleine
	//     largeur dans une colonne etroite coupe son texte ; un bouton pose dans un
	//     FLOT prend la largeur de son texte et passe a la ligne quand il n'y en a
	//     plus. La correction n'est donc pas une ellipse -- c'est un conteneur.
	namespace designkit {

		/// Une rangee de boutons qui **prennent la largeur de leur texte** et
		/// reviennent a la ligne toutes seules. C'est le remede exact aux libelles
		/// tronques : `BeginFlow` mesure chaque item, la colonne ne les ecrase plus.
		struct Flow {
				explicit Flow(NkGuiContext &c) : ctx(c) {
					nkgui::BeginFlow(ctx);
				}
				~Flow() {
					nkgui::EndFlow(ctx);
				}
				NkGuiContext &ctx;
		};

		// ── CE QUE L'INTERFACE A REELLEMENT DESSINE ─────────────────────────
		// ⚠️ `UiRects` A VECU ICI, ET IL EST MORT LE 2026-08-29. Il portait son
		//    propre stockage, son propre filtre et sa propre ecriture — un
		//    REGISTRE COMPLET, dans une seule application. Tout cela vit desormais
		//    dans **NKGui** (`NkGui/Core/NkGuiIntrospect.h`), c'est-a-dire dans la
		//    couche qui dessine, donc la seule qui sache ce qu'elle a dessine.
		//
		// ⚠️ ON NE LES A PAS FAIT COEXISTER, ET C'ETAIT LE POINT. Un doublon qui
		//    s'accorde aujourd'hui est un doublon qui divergera demain : ce depot
		//    l'a paye cette semaine avec deux registres de roles homonymes et deux
		//    objets theme. Le but n'etait jamais que les deux rendent la meme
		//    chose — c'etait qu'il n'y en ait plus qu'un.
		//
		// ⚠️ CE QUI RESTE ICI N'EST PAS UN SECOND REGISTRE : ce sont trois
		//    adaptateurs SANS ETAT, qui ne stockent rien, ne filtrent rien,
		//    n'ecrivent rien. Ils NOMMENT. Le nommage, lui, appartient bien a
		//    l'application : NKGui ne peut pas savoir que ce panneau s'appelle
		//    « hierarchie ».
		//
		// ⚠️ ET LE REFUS DE 2026-08-18 EST LEVE, PAS OUBLIE. On avait ecarte ici
		//    « elargir NKGui pour exposer un identifiant de test », au motif qu'on
		//    rembourse pendant des mois une dette posee dans un module partage pour
		//    un besoin d'essai. Le refus etait juste POUR CE QU'IL REFUSAIT — un
		//    crochet d'essai. Ce qui l'a remplace n'en est pas un : c'est une
		//    capacite du socle, justifiee par trois pertes mesurees hors de tout
		//    essai, et dont les consommateurs sont les quatre editeurs.
		namespace releve {

			/// La REGION d'un panneau hote, publiee sous « panneau.<nom> ».
			/// A appeler en TETE de chaque `OnUI`. C'est la seule chose que l'hote
			/// connaisse et qui distingue un panneau dessine d'un panneau cache
			/// derriere un onglet.
			///
			/// ⚠️ LE FILTRE A DISPARU, ET C'EST LA CORRECTION. L'ancien
			///    `UiRects::Note` REFUSAIT de publier quand `region.w < 4` — un
			///    panneau cache derriere un onglet recevait quand meme son `OnUI`,
			///    avec une region de largeur 0, et ses widgets se taisaient. La garde
			///    avait raison contre ce qu'elle visait (une cible inatteignable ne
			///    doit pas passer pour prete) mais elle confondait **absent** et
			///    **invisible**. NKGui, lui, note TOUJOURS et **marque** : un panneau
			///    replie sort `vide`, un panneau hors champ sort `hors-vue`, et un
			///    panneau qui n'a jamais ete dessine ne sort pas du tout. Les trois
			///    cas se distinguent enfin.
			inline void Zone(NkGuiContext &ctx, const char *nom) {
				if (!nkgui::NkGuiIntrospectActif(ctx) || !nom || !*nom)
					return;
				char clef[96];
				snprintf(clef, sizeof(clef), "panneau.%s", nom);
				nkgui::NkGuiNoter(ctx, nkgui::NkGuiNature::Panneau, ctx.GetId(clef), nom,
								  ctx.layout.region, nkgui::NK_GUI_ETAT_AUCUN);
				nkgui::NkGuiIntrospectCler(ctx, clef);
			}

			/// Un rectangle DEJA CALCULE (une disposition, une aire de dessin), qui
			/// n'est le rectangle d'aucun widget.
			inline void Rect(NkGuiContext &ctx, const char *cle, const nkgui::NkRect &r) {
				if (!nkgui::NkGuiIntrospectActif(ctx) || !cle || !*cle)
					return;
				nkgui::NkGuiNoter(ctx, nkgui::NkGuiNature::Region, ctx.GetId(cle), cle, r,
								  nkgui::NK_GUI_ETAT_AUCUN);
				nkgui::NkGuiIntrospectCler(ctx, cle);
			}

			/// Donne une CLE STABLE au widget qui vient d'etre dessine.
			/// ⚠️ IL NE CREE AUCUNE NOTE — c'est toute la difference avec l'ancien
			///    `UiRects::Note`, qui ajoutait SA ligne a cote de rien du tout (NKGui
			///    ne notait pas encore). Aujourd'hui le widget se note lui-meme, avec
			///    son libelle ET son etat ; l'application ne fait qu'ajouter le nom
			///    qui ne bougera pas quand le libelle changera.
			inline void Cle(NkGuiContext &ctx, const char *cle) {
				nkgui::NkGuiIntrospectCler(ctx, cle);
			}

		} // namespace releve

		/// Enregistre le rectangle de la case qui vient d'etre dessinee, sous
		/// « id.libelle ». Le libelle plutot qu'un indice : un essai qui clique
		/// « prefs.gfx.vulkan » dit ce qu'il fait, un essai qui clique
		/// « prefs.gfx.2 » se casse en silence le jour ou l'ordre change.
		inline void NoteCell(NkGuiContext &ctx, const char *id, const char *label) {
			if (!id || !*id)
				return;
			char clef[96];
			snprintf(clef, sizeof(clef), "%s.%s", id, label ? label : "");
			releve::Cle(ctx, clef);
		}

		/// Un bouton qui publie son rectangle. Meme forme que `nkgui::Button`,
		/// avec un identifiant STABLE qui ne depend ni du libelle affiche ni de sa
		/// position — c'est lui que les essais visent.
		inline bool Button(NkGuiContext &ctx, const char *label, const char *id) {
			const bool clique = nkgui::Button(ctx, label);
			releve::Cle(ctx, id);
			return clique;
		}

		/// Un choix parmi N, **sur une seule ligne**, cellules de largeur egale.
		/// Remplace la pile verticale de `Selectable` qui donnait la « liste de
		/// valeurs cliquables » : cinq modes de taille empiles prenaient cinq
		/// lignes et se lisaient comme un menu, alors que c'est UN reglage.
		///
		/// ⚠️ REND L'INDICE CHOISI, ou -1. Il ne modifie rien lui-meme : l'appelant
		///    ecrit, et c'est lui qui sait s'il doit marquer une edition humaine.
		inline int32 Segmented(NkGuiContext &ctx, const char *const *labels, int32 count,
							   int32 current, const char *id = nullptr) {
			if (count <= 0)
				return -1;
			const int32 n = count < 12 ? count : 12;

			// ⚠️ LES CELLULES SONT PROPORTIONNELLES AU TEXTE, PAS EGALES -- et
			//    c'est une correction MESUREE, pas une preference. La premiere
			//    version donnait a chacune la meme part : sur un panneau etroit,
			//    « expand » (le plus long des cinq modes) s'affichait « expanc ».
			//    **Le controle segmente venait de recreer la troncature qu'il
			//    devait supprimer.** Cinq parts egales pour cinq mots de longueurs
			//    differentes, c'est la meme faute que le bouton pleine largeur :
			//    une largeur decidee sans regarder le contenu.
			//
			//    On mesure donc avec le MEME moteur que le rendu
			//    (`NkGuiFont::MeasureWidth` -> `CalcTextSizeX`) : mesurer avec une
			//    approximation maison — « tant de pixels par caractere » — aurait
			//    fabrique une seconde verite, fausse des la premiere police
			//    proportionnelle.
			//
			//    Les poids restent NEGATIFS (parts de l'espace restant) et non des
			//    pixels fixes : en pixels, un panneau plus etroit que la somme des
			//    libelles deborderait au lieu de serrer.
			float32 poids[12];
			float32 total = 0.f;
			for (int32 i = 0; i < n; ++i) {
				const float32 w = ctx.font ? ctx.font->MeasureWidth(labels[i]) : 0.f;
				// Le « + 24 » est la marge interne que `Selectable` ajoute autour
				// de son libelle : sans elle, la cellule vaudrait exactement le
				// texte et le rognerait de la valeur du padding.
				poids[i] = -(w + 24.f);
				total += w + 24.f;
			}

			// ⚠️ ET SI LES LIBELLES NE TIENNENT PAS SUR UNE LIGNE ? MESURE, PUIS
			//    CHANGEMENT DE CONTENEUR. Les poids proportionnels partagent
			//    l'espace DISPONIBLE : quand le total necessaire le depasse, ils
			//    retrecissent tout **en meme proportion** — et six noms d'API dans
			//    une colonne etroite se sont affiches « autoopenglvulkandx11dx12softw ».
			//    C'est la TROISIEME fois que la troncature revient sous une forme
			//    nouvelle, et a chaque fois la cause est la meme : **une largeur
			//    decidee sans regarder si le contenu y tient**.
			//
			//    Le remede n'est pas un libelle plus court — les noms d'API sont un
			//    vocabulaire partage avec le fichier et la ligne de commande, les
			//    abreger ici enseignerait un mot que le fichier ne comprend pas.
			//    C'est le CONTENEUR qui change : au-dela de la place disponible, on
			//    passe en FLOT, qui met a la ligne. La forme « une ligne, N cases »
			//    est preferable — un choix exclusif se lit d'un coup d'oeil — mais
			//    **elle n'est pas preferable au point d'etre illisible**.
			const float32 dispo = ctx.layout.region.w;
			int32 chosen = -1;
			if (dispo > 0.f && total > dispo) {
				nkgui::BeginFlow(ctx);
				for (int32 i = 0; i < n; ++i) {
					if (nkgui::Selectable(ctx, labels[i], i == current))
						chosen = i;
					NoteCell(ctx, id, labels[i]);
				}
				nkgui::EndFlow(ctx);
				return chosen;
			}
			nkgui::BeginRow(ctx, 0.f, poids, n);
			for (int32 i = 0; i < n; ++i) {
				if (nkgui::Selectable(ctx, labels[i], i == current))
					chosen = i;
				NoteCell(ctx, id, labels[i]);
			}
			nkgui::EndRow(ctx);
			return chosen;
		}

		/// Un titre de section repliable. Un seul point de passage : le jour ou la
		/// charte donne une forme aux titres, elle se pose ICI et nulle part
		/// ailleurs.
		inline bool Section(NkGuiContext &ctx, const char *title) {
			return nkgui::CollapsingHeader(ctx, title);
		}

		/// Une ligne « cle : valeur » en DEUX COLONNES alignees, au lieu d'une
		/// phrase qui se fait couper. La cle est bornee, la valeur prend le reste.
		/// L'ORDONNÉE DU BAS **VISIBLE** d'un panneau ancré.
		///
		/// ⚠️ ELLE NE SE DÉDUIT PAS DE `ctx.layout.region`. Mesure `--dump-ui` :
		///        panneau.hierarchie = 48.0 84.0 219.8 **1000000.0**
		///     `region.h` porte la hauteur du CONTENU défilable, volontairement
		///     sans borne. Un panneau qui s'en sert pour partager sa hauteur
		///     envoie sa seconde moitié à y = 999 874 — hors écran, et le
		///     diagnostic devient « il n'y a qu'une section ».
		///
		/// La zone VISIBLE est celle du cadre de défilement empilé par
		/// `nkgui::Begin` (`BeginScrollFrame(..., content, ...)`) :
		/// `childStack[childDepth-1].area`. On la lit là, ou nulle part.
		///
		/// ⚠️ ET LE CAS « PAS DE CADRE » CRIE, une fois, plutôt que de rendre un
		///    chiffre plausible : un panneau dessiné hors fenêtre est un montage
		///    que personne n'a prévu, et une valeur inventée le rendrait
		///    indétectable.
		inline float32 HauteurVisibleBas(NkGuiContext &ctx, const char *quiDemande) {
			if (ctx.childDepth > 0) {
				const NkRect &a = ctx.childStack[ctx.childDepth - 1].area;
				return a.y + a.h;
			}
			static bool criAdresse = false;
			if (!criAdresse) {
				criAdresse = true;
				logger.Error("[NKUIDesign] {0} : aucun cadre de défilement (childDepth=0). La "
							 "hauteur visible est INCONNUE ; on retombe sur un repli de 600 px, "
							 "qui n'est PAS une mesure.",
							 quiDemande ? quiDemande : "?");
			}
			return ctx.layout.cursor.y + 600.f;
		}

		inline void KeyValue(NkGuiContext &ctx, const char *key, const char *value) {
			static const float32 kPoids[2] = {-2.f, -3.f};
			nkgui::BeginRow(ctx, 0.f, kPoids, 2);
			nkgui::Text(ctx, key);
			nkgui::Text(ctx, value ? value : "");
			nkgui::EndRow(ctx);
		}

	} // namespace designkit

	// ═══════════════════════════════════════════════════════════════════════════
	//  L'ETAT PARTAGE
	// ═══════════════════════════════════════════════════════════════════════════
	// ⚠️ IL NE CONTIENT AUCUNE VALEUR DE REGLAGE NI AUCUN RECTANGLE. Les reglages
	//    vivent dans l'instance de chaque noeud ; les rectangles se recalculent a
	//    chaque image et se jettent. C'est ce qui garantit que ce que l'apercu
	//    dessine est EXACTEMENT ce que la sauvegarde ecrit — deux copies auraient
	//    diverge des la premiere seance.
	struct DesignState {
			NkUIDocument doc;
			NkLayoutResult layout;
			NkDocumentHost host;
			NkTheme theme;

			NkDesignAI ai;
			NkFileBackend fileBackend;

			/// ⚠️ UNE SEULE SELECTION POUR LES TROIS PANNEAUX. Document 3 §11.5 :
			///    « deux notions de "ce qui est sélectionné" finiraient par
			///    diverger, et personne ne saurait laquelle fait foi. » Elle vit
			///    donc ici, dans l'etat partage, et JAMAIS dans un panneau.
			///
			/// Elle vit aussi dans la VUE et non dans le document : `Save` ne
			/// l'ecrit pas, et le controle 40h le mesure -- selectionner ne change
			/// pas un octet du fichier.
			NkSelection sel;
			/// Le noeud PRINCIPAL, derive de `sel`. Garde parce que tout le code
			/// existant s'en sert ; il ne se pose plus a la main.
			int32 selected = 0;		 ///< index de noeud ; -1 = aucun

			/// ── LE MODE ÉDITION DE FORME ─────────────────────────────────────
			/// ⚠️ IL A DÉMÉNAGÉ DE `PreviewPanel` VERS L'ÉTAT PARTAGÉ LE 01/09, ET
			///    POUR LA MÊME RAISON QUE LA SÉLECTION CI-DESSUS : **deux panneaux
			///    en ont besoin**. La toile peint les sommets ; l'Inspecteur doit
			///    afficher la section « ÉDITION DE FORME » (X/Y/rayon du sommet,
			///    « Terminer ») — c'est le point 2 de la comparaison en deux temps
			///    de Rodolf (`lunacy_2temps_edition_181745.png`), celui qui montre
			///    que Lunacy change le CONTENU DU PANNEAU DROIT en même temps que
			///    la toile.
			///
			/// ⚠️ ET IL DÉMÉNAGE PLUTÔT QU'IL NE SE RECOPIE. Un miroir posé à
			///    chaque image aurait « marché » et créé la divergence exacte que
			///    le document 3 §11.5 interdit pour la sélection : un drapeau
			///    oublié à un site de mutation ment pour toujours. Il n'y a qu'un
			///    seul état, et les deux panneaux le lisent.
			///
			/// Comme la sélection, il vit dans la VUE : `Save` ne l'écrit pas.
			struct NkModeForme {
				int32 noeud = -1;  ///< le nœud dont les sommets s'éditent, -1 = aucun
				/// ⚠️ `sommet` ET `tire` SONT DEUX CHOSES, et les confondre viderait
				///    le panneau au relâchement. `tire` est le sommet SOUS LA MAIN
				///    (il retombe à -1 dès qu'on lâche) ; `sommet` est le sommet
				///    SÉLECTIONNÉ, celui dont l'Inspecteur montre les coordonnées —
				///    il survit au relâchement, sinon les champs X/Y clignoteraient
				///    et seraient inutilisables.
				int32 sommet = -1; ///< le sommet PRINCIPAL (survit au relâchement)
				int32 tire = -1;   ///< le sommet en cours de GLISSEMENT
				/// La POIGNÉE DE TANGENTE tenue : sommet, et côté (0 = entrante,
				/// 1 = sortante). -1 = aucune.
				/// ⚠️ SÉPARÉ DE `tire`, ET CE N'EST PAS DU CONFORT : les deux
				///    gestes écrivent des choses différentes (l'un déplace le
				///    sommet et ses jumeaux marqués, l'autre pose un vecteur et
				///    laisse le sommet tranquille). Un seul champ aurait forcé un
				///    drapeau « c'est une tangente » à côté — et un drapeau oublié
				///    à un site de mutation ment pour toujours.
				int32 tangenteSommet = -1;
				uint32 tangenteCote = 0;
				/// ── LA SÉLECTION MULTIPLE DE SOMMETS ─────────────────────────
				/// Retour de Rodolf, 01/09 (soir) : *« on doit pouvoir
				/// sélectionner plusieurs vertices pour déplacement ou
				/// transformation simultanés. »*
				///
				/// ⚠️ MÊME DISCIPLINE QUE `sel` / `selected` VINGT LIGNES PLUS
				///    HAUT, et ce n'est pas une coïncidence de style : c'est la
				///    règle du document 3 §11.5, *« deux notions de ce qui est
				///    sélectionné finiraient par diverger »*. `marques` porte
				///    l'ensemble, `sommet` en est le PRINCIPAL (celui dont
				///    l'Inspecteur montre les coordonnées) — et il se DÉRIVE, il
				///    ne se pose pas à la main.
				///
				/// ⚠️ UN MASQUE DE 64 BITS, ET LE PLAFOND EST DIT PLUTÔT QUE SUBI.
				///    `NkSommetsDe` écrit au plus 32 ancres, donc 64 est déjà le
				///    double de ce que la couche du dessous produit. Au-delà, un
				///    sommet ne se marque pas — il ne se marque pas *en silence
				///    faux*, il ne se marque pas du tout, et `Marque()` rend faux.
				///    Le jour où un tracé dépasse, c'est un `NkVector` qu'il
				///    faudra, pas un second masque.
				nkentseu::uint64 marques = 0;

				bool Marque(int32 i) const {
					return i >= 0 && i < 64 && (marques & (1ull << (nkentseu::uint32)i)) != 0ull;
				}
				uint32 NbMarques() const {
					uint32 n = 0;
					for (uint32 b = 0; b < 64; ++b)
						if ((marques & (1ull << b)) != 0ull)
							++n;
					return n;
				}
				/// La sélection devient CE seul sommet (le clic nu).
				void MarquerSeul(int32 i) {
					marques = (i >= 0 && i < 64) ? (1ull << (nkentseu::uint32)i) : 0ull;
					sommet = (i >= 0 && i < 64) ? i : -1;
				}
				/// Ajoute / retire (Maj+clic).
				/// ⚠️ ET LE PRINCIPAL SUIT, DANS LES DEUX SENS. Retirer le sommet
				///    principal sans en réélire un autre laisserait l'Inspecteur
				///    afficher les coordonnées d'un point qui n'est plus
				///    sélectionné — la même faute que `sel`/`selected` évite.
				void BasculerMarque(int32 i) {
					if (i < 0 || i >= 64)
						return;
					const nkentseu::uint64 bit = 1ull << (nkentseu::uint32)i;
					if ((marques & bit) != 0ull) {
						marques &= ~bit;
						if (sommet == i) {
							sommet = -1;
							for (uint32 b = 0; b < 64; ++b)
								if ((marques & (1ull << b)) != 0ull) {
									sommet = (int32)b;
									break;
								}
						}
					} else {
						marques |= bit;
						sommet = i;
					}
				}
				bool Actif() const {
					return noeud >= 0;
				}
				/// ⚠️ SORTIR EST UN GESTE, PAS TROIS AFFECTATIONS. Le mode se ferme
				///    à QUATRE endroits (Échap, le nœud qui disparaît, la sélection
				///    qui change, le bouton « Terminer »), et il porte désormais
				///    TROIS champs. Écrits à la main, le quatrième site en oublie
				///    un — et un `sommet` resté posé ferait afficher à l'Inspecteur
				///    les coordonnées d'un point qui n'est plus édité.
				void Quitter() {
					noeud = -1;
					sommet = -1;
					tire = -1;
					marques = 0;
					tangenteSommet = -1;
				}
			};
			NkModeForme modeForme;

			/// Les trois seuls gestes qui changent la selection. Passer par eux
			/// garantit que `sel` et `selected` ne peuvent pas diverger -- c'est
			/// exactement la divergence que le document 3 §11.5 interdit.
			void SelectSingle(int32 i) {
				sel.Set(i);
				selected = sel.Primary();
			}
			void SelectToggle(int32 i) {
				sel.Toggle(i);
				selected = sel.Primary();
			}
			void SelectClear() {
				sel.Clear();
				selected = -1;
			}
			/// Vrai si `a` descend (strictement) de `b`.
			bool EstDescendantDe(int32 a, int32 b) const {
				if (!doc.IsValidIndex(a) || !doc.IsValidIndex(b))
					return false;
				for (int32 p = doc.nodes[(uint32)a].parent; p >= 0;
					 p = doc.nodes[(uint32)p].parent)
					if (p == b)
						return true;
				return false;
			}
			/// Les RACINES de la selection : les selectionnes dont aucun ancetre
			/// n'est lui-meme selectionne. Copier un parent copie deja ses
			/// enfants — sans ce filtre, Coller rendrait l'enfant EN DOUBLE.
			void RacinesSelection(NkVector<int32> &out) const {
				out.Clear();
				for (uint32 k = 0; k < (uint32)sel.items.Size(); ++k) {
					const int32 i = sel.items[k];
					if (i == 0 || !doc.IsValidIndex(i))
						continue; // la racine du document n'est jamais un element
					bool couvert = false;
					for (uint32 j = 0; j < (uint32)sel.items.Size() && !couvert; ++j)
						if (j != k && EstDescendantDe(i, sel.items[j]))
							couvert = true;
					if (!couvert)
						out.PushBack(i);
				}
			}

			/// LA SUPPRESSION DE LA SELECTION (multi comprise) — LE geste partagé
			/// (bouton Composition, menu contextuel, touche Suppr). Une seule
			/// réparation de sélection après renumérotation, pas trois.
			/// Annulable (recette annulation, cas « suppression »).
			bool SupprimerSelection() {
				NkVector<int32> racines;
				RacinesSelection(racines);
				if (racines.Empty()) {
					status = NkString(sel.Contains(0) ? "La racine ne se supprime pas."
													  : "Rien à supprimer.");
					return false;
				}
				int32 n = 0;
				for (uint32 k = 0; k < (uint32)racines.Size(); ++k) {
					const int32 r = racines[k];
					if (!doc.IsValidIndex(r))
						continue; // déjà emporté par une suppression précédente
					NkVector<int32> remap;
					if (!doc.RemoveSubtree(r, &remap))
						continue;
					++n;
					// ⚠️ LA SUPPRESSION RENUMEROTE : les racines RESTANTES se
					//    reparent par la table, sinon elles designeraient un
					//    autre noeud.
					for (uint32 j = k + 1; j < (uint32)racines.Size(); ++j)
						racines[j] = (racines[j] >= 0 && racines[j] < (int32)remap.Size())
										 ? remap[(uint32)racines[j]]
										 : -1;
				}
				SelectClear();
				host.demoModels.Clear();
				host.SyncTo(doc);
				char msg[64];
				snprintf(msg, sizeof(msg), "%d élément(s) supprimé(s) — Ctrl+Z les ramène.", n);
				status = NkString(msg);
				return n > 0;
			}

			// ── LE PRESSE-PAPIERS INTERNE (raccourcis Lunacy, 01/09) ─────────
			/// Un vrai DOCUMENT : la copie complete (CopierSousArbre) re-interne
			/// les noms de metrique — aucun pointeur ne survit a son pool.
			/// Le presse-papiers SYSTEME (coller hors de l'application) est un
			/// chantier nomme, pas celui-ci.
			NkUIDocument pressePapiers;
			bool pressePapiersPlein = false;

			uint32 CopierSelection() {
				NkVector<int32> racines;
				RacinesSelection(racines);
				if (racines.Empty()) {
					status = NkString("Rien à copier.");
					return 0;
				}
				pressePapiers.NewDocument("presse-papiers", NkAuthor::Humain);
				uint32 n = 0;
				for (uint32 k = 0; k < (uint32)racines.Size(); ++k)
					if (pressePapiers.CopierSousArbre(doc, racines[k], 0) >= 0)
						++n;
				pressePapiersPlein = n > 0;
				char msg[48];
				snprintf(msg, sizeof(msg), "Copié : %u élément(s).", n);
				status = NkString(msg);
				return n;
			}

			bool CouperSelection() {
				const uint32 n = CopierSelection();
				if (n == 0)
					return false;
				SupprimerSelection(); // copie + suppression = UN pas d'annulation
				char msg[48];
				snprintf(msg, sizeof(msg), "Coupé : %u élément(s).", n);
				status = NkString(msg);
				return true;
			}

			/// Colle dans le parent de la selection courante (sinon la racine),
			/// decale de 10 px — un collage aux memes coordonnees disparaitrait
			/// SOUS l'original et passerait pour un non-geste.
			bool CollerPressePapiers() {
				if (!pressePapiersPlein || pressePapiers.nodes.Empty()
					|| pressePapiers.nodes[0].children.Empty()) {
					status = NkString("Le presse-papiers est vide.");
					return false;
				}
				int32 parent = 0;
				if (doc.IsValidIndex(selected)) {
					const int32 p = doc.nodes[(uint32)selected].parent;
					if (doc.IsValidIndex(p))
						parent = p;
				}
				sel.Clear();
				const NkVector<int32> src = pressePapiers.nodes[0].children; // copie
				uint32 n = 0;
				for (uint32 k = 0; k < (uint32)src.Size(); ++k) {
					const int32 ni = doc.CopierSousArbre(pressePapiers, src[k], parent);
					if (!doc.IsValidIndex(ni))
						continue;
					doc.nodes[(uint32)ni].posX += 10.f;
					doc.nodes[(uint32)ni].posY += 10.f;
					doc.MarkHumanEdit(ni);
					sel.Add(ni);
					++n;
				}
				selected = sel.Primary();
				host.demoModels.Clear();
				host.SyncTo(doc);
				char msg[64];
				snprintf(msg, sizeof(msg), "Collé : %u élément(s) (décalés de 10 px).", n);
				status = NkString(msg);
				return n > 0;
			}

			/// Ctrl+D : la copie directe, sans passer par le presse-papiers.
			bool DupliquerSelection() {
				NkVector<int32> racines;
				RacinesSelection(racines);
				if (racines.Empty()) {
					status = NkString("Rien à dupliquer.");
					return false;
				}
				NkVector<int32> nouveaux;
				for (uint32 k = 0; k < (uint32)racines.Size(); ++k) {
					const int32 r = racines[k];
					int32 parent = doc.nodes[(uint32)r].parent;
					if (!doc.IsValidIndex(parent))
						parent = 0;
					const int32 ni = doc.CopierSousArbre(doc, r, parent);
					if (!doc.IsValidIndex(ni))
						continue;
					doc.nodes[(uint32)ni].posX += 10.f;
					doc.nodes[(uint32)ni].posY += 10.f;
					doc.MarkHumanEdit(ni);
					nouveaux.PushBack(ni);
				}
				sel.Clear();
				for (uint32 k = 0; k < (uint32)nouveaux.Size(); ++k)
					sel.Add(nouveaux[k]);
				selected = sel.Primary();
				host.demoModels.Clear();
				host.SyncTo(doc);
				char msg[48];
				snprintf(msg, sizeof(msg), "Dupliqué : %u élément(s).",
						 (uint32)nouveaux.Size());
				status = NkString(msg);
				return !nouveaux.Empty();
			}

			/// Ctrl+G : un conteneur POSE (layout Free) adopte la selection en
			/// preservant les positions A L'ECRAN — les positions absolues
			/// viennent de la disposition RESOLUE (`layout`), pas d'un recalcul.
			bool GrouperSelection() {
				NkVector<int32> racines;
				RacinesSelection(racines);
				if (racines.Empty()) {
					status = NkString("Rien à grouper.");
					return false;
				}
				// ── LE GROUPE NAIT AU PLUS PROCHE ANCETRE COMMUN ─────────────
				// Retour du coordinateur, 01/09. Avant : on REFUSAIT des que deux
				// elements n'avaient pas le meme parent (« pour l'instant ») —
				// c'est-a-dire le cas le plus courant en usage reel : une
				// etiquette dans une carte plus une icone dans une autre.
				// ⚠️ ET LA REPONSE N'EST PAS « LA RACINE ». Poser tous les groupes
				//    a la racine serait plus simple et FAUX : grouper deux
				//    elements d'une meme page les sortirait de cette page, donc de
				//    son cadrage, de sa cible et de son ancrage. Le groupe nait
				//    AUSSI BAS QUE POSSIBLE — le calcul vit dans SelectionGeste.h
				//    et se mesure sans fenetre.
				const int32 parent = NkAncetreCommun(doc, racines.Data(), (uint32)racines.Size());
				if (!doc.IsValidIndex(parent)) {
					status = NkString("Grouper : aucun ancêtre commun.");
					return false;
				}
				if (parent != 0
					&& (!doc.IsValidIndex(parent)
						|| doc.nodes[(uint32)parent].layout.kind != NkLayoutKind::Free)) {
					status = NkString("Grouper : le parent doit poser librement ses enfants "
									  "(agencement Free).");
					return false;
				}
				// la boite englobante, en espace DOCUMENT resolu
				float32 minX = 0.f, minY = 0.f, maxX = 0.f, maxY = 0.f;
				for (uint32 k = 0; k < (uint32)racines.Size(); ++k) {
					if (!layout.Has(racines[k])) {
						status = NkString("Grouper : la disposition n'est pas encore calculée.");
						return false;
					}
					const NkPaintRect r = layout.At(racines[k]);
					if (k == 0) {
						minX = r.x;
						minY = r.y;
						maxX = r.x + r.w;
						maxY = r.y + r.h;
					} else {
						if (r.x < minX)
							minX = r.x;
						if (r.y < minY)
							minY = r.y;
						if (r.x + r.w > maxX)
							maxX = r.x + r.w;
						if (r.y + r.h > maxY)
							maxY = r.y + r.h;
					}
				}
				const float32 pax = layout.Has(parent) ? layout.At(parent).x : 0.f;
				const float32 pay = layout.Has(parent) ? layout.At(parent).y : 0.f;
				// les absolues des membres, AVANT que le re-parentage ne perime quoi
				// que ce soit
				NkVector<float32> absX, absY;
				for (uint32 k = 0; k < (uint32)racines.Size(); ++k) {
					absX.PushBack(layout.At(racines[k]).x);
					absY.PushBack(layout.At(racines[k]).y);
				}
				const int32 g = doc.AddChild(parent, "", NkAuthor::Humain);
				if (!doc.IsValidIndex(g)) {
					status = NkString("Grouper : échec de création.");
					return false;
				}
				{
					// « Groupe N » : N = groupes existants + 1
					int32 nb = 0;
					for (uint32 i = 0; i < (uint32)doc.nodes.Size(); ++i)
						if (doc.nodes[i].label.Data()
							&& 0 == strncmp(doc.nodes[i].label.Data(), "Groupe ", 7))
							++nb;
					char nom[32];
					snprintf(nom, sizeof(nom), "Groupe %d", nb + 1);
					NkUINode &gn = doc.nodes[(uint32)g];
					gn.label = NkString(nom);
					gn.layout.kind = NkLayoutKind::Free;
					gn.posX = minX - pax;
					gn.posY = minY - pay;
					gn.width.mode = NkSizeMode::Fixed;
					gn.width.value = maxX - minX;
					gn.height.mode = NkSizeMode::Fixed;
					gn.height.value = maxY - minY;
				}
				for (uint32 k = 0; k < (uint32)racines.Size(); ++k) {
					if (!doc.Reparent(racines[k], g))
						continue;
					doc.nodes[(uint32)racines[k]].posX = absX[k] - minX;
					doc.nodes[(uint32)racines[k]].posY = absY[k] - minY;
				}
				doc.MarkHumanEdit(g);
				SelectSingle(g);
				host.demoModels.Clear();
				host.SyncTo(doc);
				status = NkString("Groupé — Ctrl+Maj+G dégroupe.");
				return true;
			}

			/// Ctrl+Maj+G : les enfants remontent, positions à l'écran intactes.
			bool DegrouperSelection() {
				const int32 g = selected;
				if (!doc.IsValidIndex(g) || g == 0 || doc.nodes[(uint32)g].children.Empty()) {
					status = NkString("Dégrouper : sélectionner un groupe (un conteneur avec "
									  "des enfants).");
					return false;
				}
				if (StrEq(doc.nodes[(uint32)g].shape.Data(), "frame")) {
					status = NkString("Dégrouper : une PAGE ne se dégroupe pas (ses enfants y "
									  "restent).");
					return false;
				}
				int32 parent = doc.nodes[(uint32)g].parent;
				if (!doc.IsValidIndex(parent))
					parent = 0;
				if (!layout.Has(g)) {
					status = NkString("Dégrouper : la disposition n'est pas encore calculée.");
					return false;
				}
				const float32 pax = layout.Has(parent) ? layout.At(parent).x : 0.f;
				const float32 pay = layout.Has(parent) ? layout.At(parent).y : 0.f;
				const NkVector<int32> enfants = doc.nodes[(uint32)g].children; // copie
				// les absolues d'abord — le re-parentage ne perime pas `layout`,
				// mais l'ordre rend l'intention lisible
				NkVector<float32> absX, absY;
				for (uint32 k = 0; k < (uint32)enfants.Size(); ++k) {
					absX.PushBack(layout.Has(enfants[k]) ? layout.At(enfants[k]).x : pax);
					absY.PushBack(layout.Has(enfants[k]) ? layout.At(enfants[k]).y : pay);
				}
				for (uint32 k = 0; k < (uint32)enfants.Size(); ++k) {
					if (!doc.Reparent(enfants[k], parent))
						continue;
					doc.nodes[(uint32)enfants[k]].posX = absX[k] - pax;
					doc.nodes[(uint32)enfants[k]].posY = absY[k] - pay;
					doc.MarkHumanEdit(enfants[k]);
				}
				// le groupe, VIDE desormais, s'en va — et renumerote : la
				// selection se repare par la table
				NkVector<int32> remap;
				doc.RemoveSubtree(g, &remap);
				sel.Clear();
				for (uint32 k = 0; k < (uint32)enfants.Size(); ++k) {
					const int32 nc = (enfants[k] >= 0 && enfants[k] < (int32)remap.Size())
										 ? remap[(uint32)enfants[k]]
										 : -1;
					if (nc >= 0)
						sel.Add(nc);
				}
				selected = sel.Primary();
				host.demoModels.Clear();
				host.SyncTo(doc);
				char msg[48];
				snprintf(msg, sizeof(msg), "Dégroupé : %u élément(s).", (uint32)enfants.Size());
				status = NkString(msg);
				return true;
			}
			int32 paletteChoice = 0; ///< 0 = cadre, puis 1+ = index dans le registre
			NkString status;
			/// Vrai si le DERNIER chargement a echoue sur un fichier PRESENT.
			bool loadFailed = false;
			/// Ce que l'audit des roles a trouve au demarrage, en une ligne. Lu par
			/// `main` pour le journal : le journal du lancement doit porter l'etat
			/// des roles, sinon il faut ouvrir la fenetre pour l'apprendre -- et
			/// c'est precisement ce qui a coute la seance du 18/08.
			NkString roleAudit;

			// ── CE QUE `main` A RESOLU, RECOPIE ICI POUR L'AFFICHAGE ─────────
			// ⚠️ RECOPIE, ET NON RECALCULEE. Le panneau pourrait rappeler
			//    `NkGfxResolve` — et il afficherait alors une SECONDE verite, qui
			//    divergerait le jour ou l'un des deux appels changerait d'argument.
			//    C'est exactement le defaut qui a laisse vivre le magenta. Une
			//    seule resolution, au lancement ; l'interface en montre le
			//    resultat.
			NkString gfxEffective = NkString("?");
			NkString gfxSource = NkString("?");
			int32 prefsChoice = 0;		   ///< index dans la liste des API du panneau
			bool prefsNeedsRestart = false; ///< un enregistrement attend un relancement
			NkString prefsStatus;

			/// ⚠️ CE QUE LE FICHIER NOMME, ET C'EST UN AUTRE FAIT QUE `gfxEffective`.
			///    `gfxEffective` dit ce qui TOURNE ; celui-ci dit ce que
			///    `nkuidesign.cfg` demandera au PROCHAIN lancement. Les deux
			///    diffèrent dès qu'on passe `--gfx=` en ligne de commande ou qu'on
			///    vient de changer le réglage : cocher le menu avec le mauvais des
			///    deux ferait croire à un réglage qui n'a pas pris.
			///    Vide = le fichier ne porte AUCUNE clé `gfx` ; ce n'est pas
			///    « auto », et aucune entrée du menu n'est alors cochée.
			NkString cfgChoice;

			char promptBuf[512] = {0};

			// ── LE SEUL CHEMIN D'ÉCRITURE DU BACKEND ────────────────────────
			// ⚠️ LE MENU ET LE PANNEAU L'APPELLENT TOUS LES DEUX, et c'est la
			//    raison d'être de cette fonction. Pendant la transition, le menu
			//    écrivait de son côté et le panneau du sien : la première ligne de
			//    message changée les aurait fait diverger, et surtout un choix fait
			//    au menu ne se voyait pas dans le panneau. Une seule écriture, un
			//    seul état.
			//
			// Relit avant d'écrire, remplace la seule ligne `gfx`, écrit de façon
			// atomique — voir `Backend.h`.
			bool SetGfxConfig(const char *api) {
				if (NkGfxConfigSetKey(NkGfxConfigPath(), "gfx", api)) {
					cfgChoice = NkString(api);
					prefsNeedsRestart = true;
					prefsStatus = NkString("Écrit : gfx = ");
					prefsStatus.Append(api);
					prefsStatus.Append("  (nkuidesign.cfg)");
					return true;
				}
				// ⚠️ UN ÉCHEC D'ÉCRITURE SE DIT AUSSI. Un bouton qui ne fait rien et
				//    ne dit rien est pire qu'un bouton absent : on croit avoir
				//    réglé, et on mesure sur autre chose.
				// ⚠️ ET `cfgChoice` N'EST PAS TOUCHÉ : cocher l'entrée après un échec
				//    afficherait un réglage que le fichier ne porte pas.
				prefsNeedsRestart = false;
				prefsStatus = NkString("ÉCHEC d'écriture : le fichier n'a PAS été modifié "
									   "(rien n'est perdu).");
				return false;
			}
			/// L'ancien nom, gardé tant que le panneau Préférences existe encore.
			void SavePrefs(const char *api) {
				(void)SetGfxConfig(api);
			}

			void Init() {
				theme = NkTheme::Dark();
				// Le registre est la SOURCE de la palette. On y inscrit ce que cette
				// application connait ; les autres composants s'y inscriront de leur
				// cote, et la palette les affichera sans qu'une ligne bouge ici.
				NkComponentRegistry::Register(NkContentBrowserDecl());
				// ⚠️ LE SECOND COMPOSANT REEL, et c'est lui qui rend l'affirmation
				//    « aucun panneau ne nomme un composant » verifiable. Jusqu'ici
				//    la palette bouclait sur un registre a UNE entree : elle
				//    « marchait » sans rien prouver.
				NkComponentRegistry::Register(NkTreeViewDecl());
				// ⚠️ `host.resolve` N'EST PLUS POSE ICI, et c'est le correctif du
				//    18/08 : sa valeur par defaut EST la resolution de
				//    l'application (`NkDesignResolveRole`). Tant que chaque hote
				//    posait la sienne, la sonde a pu en poser une autre -- un
				//    hachage permissif -- et mesurer autre chose que l'ecran.
				ai.SetBackend(&fileBackend);

				if (!LoadDoc()) {
					BuildStarterDocument();
					// Fichier ABSENT (premier lancement) : Ctrl+S ecrira le chemin
					// historique. Fichier PRESENT mais illisible : chemin vide —
					// Ctrl+S n'ecrasera PAS le fichier qu'on n'a pas su lire
					// (c'est la regle « ne pas enregistrer par-dessus »).
					if (!loadFailed)
						cheminActif = NkString(kDocumentPath);
				}

				// ── L'ONGLET 0 EXISTE DES LE DEMARRAGE (multi-documents) ─────
				// Son ardoise sera rangee au premier Basculer ; ici seulement le
				// nom et le chemin, pour que la bande d'onglets ait a lire.
				{
					NkDocOuvert s0;
					s0.nom = doc.title;
					s0.chemin = cheminActif;
					ouverts.PushBack(s0);
					ongletActif = 0;
				}
				ChargerOngletsDemonstration();

				AuditDeclaredRoles();
			}

			/// LES DEUX ONGLETS DE DEMONSTRATION (5e retour) : Landing_Page et
			/// HUD_Jeu sont DES DOCUMENTS DISTINCTS (.nkuidoc — du contenu, pas
			/// du code), cherches a cote du document principal puis au chemin du
			/// depot. Introuvables = pas d'onglet, et le journal le dit — pas de
			/// chrome qui promet.
			void ChargerOngletsDemonstration() {
				static const char *const kDemos[2] = {"demo_landing_page.nkuidoc",
													  "demo_hud_jeu.nkuidoc"};
				for (uint32 d = 0; d < 2; ++d) {
					// 1. a cote du document principal (le repertoire de travail)
					NkString c1 = NkString(kDemos[d]);
					// 2. le chemin du depot (lancement depuis la racine de l'arbre)
					NkString c2 = NkString("Applications/NKUIDesign/design/mises_en_scene/");
					c2.Append(kDemos[d]);
					const char *trouve = nullptr;
					if (nkentseu::NkFile::Exists(c1.Data()))
						trouve = c1.Data();
					else if (nkentseu::NkFile::Exists(c2.Data()))
						trouve = c2.Data();
					if (!trouve) {
						logger.Info("[NKUIDesign] démo « {0} » introuvable : pas d'onglet.",
									kDemos[d]);
						continue;
					}
					const NkString texte = nkentseu::NkFile::ReadAllText(trouve);
					NkUIDocument dd;
					if (texte.Empty() || !dd.Load(texte.Data())) {
						logger.Warn("[NKUIDesign] démo « {0} » illisible : pas d'onglet.",
									trouve);
						continue;
					}
					OuvrirOngletInactif(dd, trouve);
				}
			}

			// ── L'AUDIT DES ROLES, AU DEMARRAGE ET SANS ATTENDRE UNE IMAGE ──
			// ⚠️ POURQUOI ICI ET PAS AU PREMIER DESSIN : le dessin ne resout que
			//    les roles des composants REELLEMENT POSES dans le document
			//    courant. Un composant declare mais absent du document passerait
			//    donc l'audit sans etre regarde, et son magenta n'apparaitrait
			//    qu'au jour ou quelqu'un le pose. On resout TOUT ce qui est
			//    declare, tout de suite : le journal du demarrage dit alors l'etat
			//    de la BIBLIOTHEQUE, pas celui du document ouvert.
			//
			//    Il boucle sur le registre et ne nomme aucun composant.
			void AuditDeclaredRoles() {
				NkRoleAudit::Reset();
				const uint16 n = NkComponentRegistry::Count();
				for (uint16 c = 0; c < n; ++c) {
					const NkComponentDecl *d = NkComponentRegistry::At(c);
					if (!d)
						continue;
					for (uint16 t = 0; t < d->tokenCount; ++t)
						NkDesignResolveRole(d->tokens[t].defaultRole);
				}
				NkRoleAudit::Summary(roleAudit, 8);
			}

			/// ⚠️ « NOUVEAU PROJET » CRÉE LA TOILE, PLUS LE DOCUMENT DE DÉMONSTRATION
			///    (mandat du 2026-08-30 : « designer nos premiers composants »).
			///    §3 « Vierge » : canvas infini vide, une page « Page 1 » — c'est
			///    `NkBuildBlankDocument`. Le document de démonstration reste bâti
			///    par sa fonction libre, mesuré tel quel par les essais 41d/41e.
			void BuildStarterDocument() {
				// ⚠️ LE DOCUMENT EST BATI PAR UNE FONCTION LIBRE (`Renderers.h`), et
				//    ce qui reste ici est l ETAT DE L APPLICATION. Le banc peut donc
				//    mesurer LE document de Ctrl+N, pas une copie ecrite dans le banc
				//    -- la lecon de T5 : un cote de la mesure doit venir d ailleurs
				//    que du code teste.
				NkBuildBlankDocument(doc);
				SelectSingle(0);
				PrendreEtatEnregistre();
				// L'historique repart du document neuf (on n'annule pas a
				// travers un Ctrl+N).
				histoire.Reinitialiser(etatEnregistre);
				// ⚠️ NE PAS EFFACER LE DIAGNOSTIC DE L ECHEC. C est ce qui a rendu le
				//    defaut du 28/08 indechiffrable : le chargement echouait, posait
				//    son message, et la ligne suivante le remplacait par « Document de
				//    demonstration cree. » -- un message rassurant sur un incident.
				//    L utilisateur voyait un autre document que le sien SANS SAVOIR
				//    POURQUOI, et un Ctrl+S l aurait ecrase.
				if (loadFailed) {
					status.Append("  |  document de démonstration affiché À LA PLACE.");
				} else {
					status = NkString("Document de démonstration créé.");
				}
				host.demoModels.Clear();
				host.SyncTo(doc);
			}

			/// LE PIED DE FENETRE (le rail bas fusionne, §4/§13) : l'application
			/// y pousse l'aide contextuelle et les messages d'etat. Pointeur de
			/// fonction, pas d'include du shell — le panneau ne connait que le
			/// geste « dire au pied », jamais la coquille.
			void (*pied)(void *user, const char *texte) = nullptr;
			void *piedUser = nullptr;
			/// LE TITRE : « ● nom.nkgui » quand le document est MODIFIE (Banani
			/// TopHeader, pastille non-enregistre). L'etat se MESURE — la
			/// serialisation comparee a celle du dernier enregistrement — il ne
			/// se declare pas par drapeau : un drapeau oublie a un site de
			/// mutation ment pour toujours.
			void (*titre)(void *user, bool modifie) = nullptr;
			void *titreUser = nullptr;
			NkString etatEnregistre;
			/// Selection initiale demandee en ligne de commande (--selection=N) :
			/// un levier de MISE EN SCENE pour les captures et les bancs — le
			/// premier OnUI de la toile l'applique apres le chargement, puis
			/// l'eteint. -1 = aucun.
			int32 selectionInitiale = -1;
			/// Mode EDITION DE FORME demande en ligne de commande
			/// (--mode-forme=N) : le meme etat que le double-clic pose sur une
			/// forme a sommets. -1 = aucun.
			/// ⚠️ MEME FAMILLE QUE `--editer-texte=`, ET POUR LA MEME RAISON : le
			///    harnais `--clic` ne sait pas produire un double-clic sur une
			///    coordonnee de toile qu'on ne connait pas d'avance. Sans ce
			///    levier, la seule facon de photographier le mode serait de
			///    DEVINER un pixel -- et une capture qui vise a cote ne prouve
			///    rien, elle rend une image de plus a interpreter.
			int32 modeFormeInitial = -1;
			/// Les sommets a MARQUER au premier affichage (--sommets=0,2,3), en
			/// masque. 0 = aucun, et le mode marque alors le sommet 0 seul.
			/// ⚠️ MEME FAMILLE QUE `--mode-forme=`, ET LA MEME RAISON : la
			///    multi-selection de sommets se fait au Maj+CLIC, et le harnais
			///    `--clic` ne sait pas viser une ancre dont on ignore la coordonnee
			///    d'ecran. Sans ce levier, « les trois sommets sont bien marques »
			///    resterait une affirmation que seule la main de Rodolf peut juger.
			nkentseu::uint64 sommetsInitiaux = 0;
			/// --courber : pose une tangente miroir sur les sommets marques.
			bool courberInitial = false;
			/// Edition en place demandee en ligne de commande (--editer-texte=N) :
			/// meme famille que --selection= — ouvre le champ superpose sur le
			/// noeud N (s'il est un texte) au premier affichage. -1 = aucune.
			int32 editTexteInitial = -1;
			/// Lignes de magnétisme FIGÉES (mise en scène, levier `--lignes=`) :
			/// la maquette fige un instantané de geste — verticale en FRACTION de
			/// la toile (0..1), horizontale en PIXELS depuis son haut. -1 = rien.
			/// ⚠️ CE SONT DES DÉCORS, PAS LE MAGNÉTISME. Ils existaient avant lui
			///    et servaient aux captures ; le vrai aimant vit dans `snapVif`,
			///    ci-dessous, et il est CALCULÉ. Les confondre ferait passer une
			///    mise en scène pour une mesure.
			float32 ligneV = -1.f;
			float32 ligneH = -1.f;
			/// ── LE MAGNÉTISME (Lunacy) ───────────────────────────────────────
			/// L'aimant du cluster de zoom le bascule. Vrai par défaut : c'est
			/// l'état de Lunacy au démarrage, et un aimant qu'il faut allumer
			/// n'est jamais allumé.
			bool aimantActif = true;
			/// ── LA GRILLE DE POINTS DE LA TOILE ──────────────────────────────
			/// Le menu du clic droit dans le vide la bascule (Lunacy « Pixel
			/// Grid »). Vraie par défaut : c'est ce que la toile a toujours
			/// peint, et ce chantier ne change pas ce que Rodolf voit au
			/// démarrage — il lui donne l'interrupteur qui manquait.
			bool grilleVisible = true;
			/// Le résultat VIVANT du geste en cours — les guides à peindre.
			/// Remis à zéro au relâcher : un guide qui survit à son geste est un
			/// trait qui ment.
			NkSnapResultat snapVif;
			/// Tolérance d'aimantation, en pixels ÉCRAN (÷ zoom avant l'appel).
			/// 6 px : la valeur de Lunacy à la mesure de ses captures.
			static constexpr float32 kSnapTolEcran = 6.f;
			/// Afficher la ZONE SURE des cadres a cible Mobile (ecrans 11/12,
			/// pilote par le menu Cible a terme ; levier --zone-sure).
			bool zoneSure = false;
			/// Mise en scene (--proposer) : lancer une proposition IA au premier
			/// affichage du panneau (le fichier de reponse doit etre pret).
			bool proposerInitial = false;
			/// Mise en scene (--mode=N) : la bascule de mode posee au lancement.
			int32 modeInitial = -1;
			/// Le RAPPORT DE TRANSPOSITION (écran 27) : modal d'overlay, ouvert par
			/// le menu Cible ou --rapport-transposition. Le mécanisme de
			/// transposition n'existe pas : le rapport le DIT (0 constat).
			bool rapportTransposition = false;
			/// Le declencheur de simulation (en-tete du panneau droit) demande
			/// l'ouverture du panneau Simulation — servi par l'overlay (main),
			/// qui seul tient la coquille (FocusPanel).
			bool ouvrirSimulation = false;
			/// LES CONSTATS DE LA DERNIERE TRANSPOSITION (« Generate Mobile ») :
			/// ce que la re-disposition n'a pas su absorber. Le rapport de
			/// transposition (ecran 27) les liste — il cesse d'etre vide. Etat de
			/// SESSION (le document ne porte que le lien `transpose_de`).
			NkVector<NkString> constatsTransposition;
			/// La vue POSEE en ligne de commande (--vue=, protocole de mesure du
			/// pan) : appliquee au premier affichage a la place du defaut.
			bool vuePosee = false;
			float32 vueX = 0.f;
			float32 vueY = 0.f;
			float32 vueZ = 1.f;
			/// Le MENU DES ROLES (écrans 5-6-7) : ouvert par l'onglet Widget,
			/// dessiné en overlay (main.cpp), il écrit la clé `role` du nœud.
			menurole::Etat menuRole;
			/// Le MENU DES FORMATS (catalogue Formats.h) : ouvert par la
			/// section CIBLE, dessiné en overlay (main.cpp) ; le choix passe
			/// par AppliquerFormat — une seule main sur le modèle.
			menuformat::Etat menuFormat;
			/// LA LANGUE ACTIVE d'apercu/edition (multilingue 01/09) : etat de
			/// VUE (Save ne l'ecrit pas), vide = la langue principale. La
			/// bascule est A CHAUD : l'hote la relit a chaque image.
			NkString langueActive;

			/// APPLIQUER UN FORMAT à une page : la cible s'écrit (« <nom> <L> x
			/// <H> [note] » — la note dpi du papier fait partie de la cible),
			/// le cadre se REDIMENSIONNE aux pixels RÉELS du format, le contenu
			/// reste où il est, et les DÉPASSEMENTS sont CONSTATÉS au rapport
			/// de transposition (écran 27) — le même contrat que la version
			/// mobile. Annulable en un pas (l'observateur fait le sien).
			void AppliquerFormat(int32 page, const char *nom, float32 w, float32 h,
								 const char *note) {
				if (!doc.IsValidIndex(page) || !nom)
					return;
				NkUINode &n = doc.nodes[(uint32)page];
				char t[96];
				if (note && *note)
					snprintf(t, sizeof(t), "%s %d x %d %s", nom, (int32)w, (int32)h, note);
				else
					snprintf(t, sizeof(t), "%s %d x %d", nom, (int32)w, (int32)h);
				n.target = NkString(t);
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = w;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = h;
				doc.MarkHumanEdit(page);
				// les dépassements, constatés — enfants directs contre la
				// nouvelle boîte (le contenu ne bouge pas, c'est le contrat).
				constatsTransposition.Clear();
				const NkVector<int32> kids = n.children;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i) {
					if (!doc.IsValidIndex(kids[i]))
						continue;
					const NkUINode &c = doc.nodes[(uint32)kids[i]];
					char b[160];
					if (c.width.mode == NkSizeMode::Fixed && c.width.value > w) {
						snprintf(b, sizeof(b),
								 "\xC2\xAB %s \xC2\xBB : largeur fixe %d > cible %d",
								 c.label.Data(), (int32)c.width.value, (int32)w);
						constatsTransposition.PushBack(NkString(b));
					}
					if (c.posX < 0.f
						|| (c.width.mode == NkSizeMode::Fixed && c.posX + c.width.value > w)
						|| (c.height.mode == NkSizeMode::Fixed && c.posY + c.height.value > h)) {
						snprintf(b, sizeof(b),
								 "\xC2\xAB %s \xC2\xBB : position posée hors bornes de la cible",
								 c.label.Data());
						constatsTransposition.PushBack(NkString(b));
					}
				}
				char msg[160];
				snprintf(msg, sizeof(msg),
						 "Format « %s » appliqué — %d constat(s) au rapport de transposition.",
						 nom, (int32)constatsTransposition.Size());
				DireAuPied(msg);
			}
			/// Mise en scene (--menu-role) : ouvrir le menu au premier passage
			/// dans la rangee Role (l'ancre vraie, pas une devinee).
			bool menuRoleInitial = false;
			/// Mise en scene (--filtre-hierarchie) : la loupe deja depliee.
			bool filtreHierarchieInitial = false;
			/// L'onglet d'Inspecteur demande au lancement (--inspecteur-onglet=,
			/// mise en scene des ecrans 4/5/6) : -1 = defaut (Design).
			int32 ongletInitial = -1;
			void PrendreEtatEnregistre() {
				etatEnregistre = NkString();
				doc.Save(etatEnregistre);
			}
			bool DocumentModifie() {
				NkString now;
				doc.Save(now);
				const char *a = now.Data() ? now.Data() : "";
				const char *b = etatEnregistre.Data() ? etatEnregistre.Data() : "";
				while (*a && *a == *b) {
					++a;
					++b;
				}
				return *a != *b;
			}
			void DireAuPied(const char *t) {
				status = NkString(t ? t : "");
				if (pied)
					pied(piedUser, status.Data());
			}

			/// La vue de toile : la SEULE traduction document <-> ecran.
			NkCanvasView view;

			/// ⚠️ LA DISPOSITION SE CALCULE EN ESPACE DOCUMENT, TOUJOURS.
			///    `surface` est ici un rectangle DOCUMENT, pas un rectangle ecran.
			///    C'est ce qui fait que zoomer ne change AUCUNE valeur du modele :
			///    le zoom vit dans la vue, jamais dans le document.
			void Recompute(const NkPaintRect &surface) {
				host.SyncTo(doc);
				NkComputeLayout(doc, surface, layout);
			}

			/// La meme disposition, traduite en pixels ECRAN. Le dessin et la
			/// saisie travaillent sur celle-ci ; le modele ne la voit jamais.
			void ProjectToScreen(NkLayoutResult &out) const {
				out.valid = layout.valid;
				out.rects.Clear();
				out.rects.Reserve(layout.rects.Size());
				for (uint32 i = 0; i < (uint32)layout.rects.Size(); ++i) {
					out.rects.PushBack(view.ToScreen(layout.rects[i]));
				}
			}

			void SaveDoc() {
				// ⚠️ CHAQUE ONGLET A SON FICHIER (multi-documents, 01/09) :
				//    Ctrl+S ecrit le chemin du document ACTIF. Un document jamais
				//    enregistre derive « <titre>.nkuidoc » a cote de l'executable
				//    courant — et le DIT.
				if (cheminActif.Empty()) {
					NkString nomF;
					const char *t = doc.title.Data() ? doc.title.Data() : "document";
					for (const char *q = t; *q; ++q) {
						const char c = *q;
						const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
										|| (c >= '0' && c <= '9') || c == '-' || c == '_';
						const char cbuf[2] = {ok ? c : '_', 0};
						nomF.Append(cbuf);
					}
					if (nomF.Empty())
						nomF.Append("document");
					nomF.Append(".nkuidoc");
					cheminActif = nomF;
				}
				NkString out;
				doc.Save(out);
				status = nkentseu::NkFile::WriteAllText(cheminActif.Data(), out.Data())
							 ? NkString("Document enregistré : ")
							 : NkString("ÉCHEC d'écriture : ");
				status.Append(cheminActif);
				PrendreEtatEnregistre();
			}

			bool LoadDoc() {
				loadFailed = false;
				if (!nkentseu::NkFile::Exists(kDocumentPath))
					return false; // absent : normal au premier lancement, rien a dire
				const NkString text = nkentseu::NkFile::ReadAllText(kDocumentPath);
				// ⚠️ UN FICHIER PRESENT MAIS LU VIDE N EST PAS UN FICHIER ABSENT.
				//    Mesure du 2026-08-28 : sur trois lancements identiques -- meme
				//    binaire, meme fichier, meme commande -- le document a ete charge
				//    DEUX fois et remplace par celui de demonstration UNE fois. Le
				//    demarrage n est pas deterministe, et la cause n est pas encore
				//    etablie (poignee encore ouverte par le processus precedent ?).
				//    **Ce qui est etabli, c est qu il faut le DIRE.**
				if (text.Empty()) {
					loadFailed = true;
					status = NkString("!! Le fichier existe mais s est lu VIDE : rien n a ete "
									  "charge. Votre document n est PAS perdu -- ne pas enregistrer.");
					return false;
				}
				uint32 unknown = 0;
				NkUIDocument loaded;
				if (!loaded.Load(text.Data(), &unknown)) {
					// ⚠️ ON NE CHARGE PAS A MOITIE. Un document dont la structure est
					//    incoherente laisse l'ancien en place et le DIT : recuperer un
					//    arbre a demi reconstruit serait pire que ne rien recuperer,
					//    parce que l'utilisateur croirait avoir retrouve son travail.
					loadFailed = true;
					status = NkString("!! Document ILLISIBLE : rien n a ete change. Votre "
									  "document n est PAS perdu -- ne pas enregistrer par-dessus.");
					return false;
				}
				doc = loaded;
				cheminActif = NkString(kDocumentPath); // l'onglet actif suit ce fichier
				SelectSingle(0);
				host.demoModels.Clear();
				host.SyncTo(doc);
				char b[192];
				snprintf(b, sizeof(b), "Document chargé : %u nœud(s), %u composant(s) inconnu(s)",
						 doc.NodeCount(), unknown);
				status = NkString(b);
				PrendreEtatEnregistre();
				// L'HISTORIQUE REPART D'ICI : on n'annule pas a travers un
				// rechargement (etat 0 = ce qui vient d'etre lu).
				histoire.Reinitialiser(etatEnregistre);
				return true;
			}

			// ── L'ANNULATION UNIFIEE (§7 ; cf. Historique.h) ────────────────
			/// Recharge le document depuis un instantane d'historique. La
			/// disposition, l'arbre et l'hote resuivent a l'image meme
			/// (Recompute -> SyncTo) ; la pastille « ● » suit par la mesure de
			/// DocumentModifie (annuler jusqu'a l'etat sauve la re-eteint) ;
			/// les tampons d'edition (Inspecteur) se resynchronisent par
			/// `editionGeneration`.
			void RechargerDepuis(const NkString &s) {
				NkUIDocument d2;
				if (!d2.Load(s.Data() ? s.Data() : ""))
					return; // un instantane illisible ne detruit rien (jamais vu :
							// il vient de Save — mais on ne charge pas a moitie)
				doc = d2;
				if (!doc.IsValidIndex(selected))
					SelectClear();
				host.demoModels.Clear();
				host.SyncTo(doc);
				++editionGeneration;
			}
			void Annuler() {
				const NkString *s = histoire.Annuler();
				if (!s) {
					DireAuPied("Rien à annuler.");
					return;
				}
				RechargerDepuis(*s);
				DireAuPied(histoire.PeutAnnuler() ? "Annulé." : "Annulé — début de l'historique.");
			}
			void Retablir() {
				const NkString *s = histoire.Retablir();
				if (!s) {
					DireAuPied("Rien à rétablir.");
					return;
				}
				RechargerDepuis(*s);
				DireAuPied("Rétabli.");
			}

			/// L'historique d'annulation du document (instantanés de
			/// sérialisation — voir Historique.h pour la règle un geste = un pas).
			NkHistorique histoire;
			/// Incrementee a chaque restauration d'historique : les tampons
			/// locaux (Inspecteur) se resynchronisent quand elle change.
			uint32 editionGeneration = 0;

			// ── LES ONGLETS DE PROJETS (§6) : N DOCUMENTS OUVERTS ────────────
			// ⚠️ MESURE DU 01/09 (5e retour de Rodolf) : les trois onglets
			//    etaient du CHROME — des libelles ecrits en dur, un clic qui ne
			//    changeait que `gOngletActif`, TOUS montraient le seul document
			//    charge. La famille « l'infobulle promettait, aucun code ne
			//    lisait » (le F jamais branche).
			//
			// Le modele : l'etat VIVANT (doc, histoire, selection, vue, chemin)
			// reste dans DesignState — les panneaux lisent `mSt->doc` et ne
			// changent PAS. Chaque onglet inactif garde une ARDOISE (NkDocOuvert)
			// de cet etat ; basculer = ranger l'actif dans son ardoise, charger
			// celle du voisin. Chaque document garde SON historique, SA pastille
			// (mesuree), SA vue, SA selection — l'annulation ne traverse pas les
			// onglets. La copie d'ardoise passe par les operateurs profonds des
			// conteneurs (la meme voie que RechargerDepuis) ; ~1,3 Mo au pire
			// (100 instantanes d'historique), au clic d'onglet seulement.
			struct NkDocOuvert {
					NkString nom;	 ///< libelle d'onglet = titre du document
					NkString chemin; ///< fichier ("" = jamais enregistre)
					NkUIDocument doc;
					NkHistorique histoire;
					NkString etatEnregistre;
					int32 selected = 0;
					float32 vueX = 0.f, vueY = 0.f, vueZ = 1.f;
					bool vueInit = false; ///< faux = vue par defaut a poser
					/// Pastille « ● » de l'onglet INACTIF : mesuree au rangement
					/// (mesurer chaque ardoise a chaque image serait une
					/// serialisation par onglet et par image — le poids invisible
					/// que la passe fluidite chasse). L'onglet ACTIF est mesure
					/// en direct par la boucle existante.
					bool modifie = false;
			};
			NkVector<NkDocOuvert> ouverts;
			uint32 ongletActif = 0;
			/// Le fichier du document ACTIF ("" = jamais enregistre). SaveDoc
			/// ecrit ICI — plus un chemin unique en dur pour tous les onglets.
			NkString cheminActif;
			/// La vue par defaut d'un document neuf : a droite de la barre
			/// d'outils flottante (2 x kOutilsMarge + kOutilsLargeur du canvas
			/// = 2 x 10 + 36 ; les constantes vivent dans CanvasPanel, declare
			/// APRES — le nombre est repris ici avec sa provenance).
			static constexpr float32 kVueDefautX = 56.f;
			static constexpr float32 kVueDefautY = 12.f;

			/// Range l'etat VIVANT dans l'ardoise de l'onglet actif.
			void RangerActif() {
				if (ongletActif >= (uint32)ouverts.Size())
					return;
				NkDocOuvert &s = ouverts[ongletActif];
				s.doc = doc;
				s.histoire = histoire;
				s.etatEnregistre = etatEnregistre;
				s.selected = selected;
				s.vueX = view.panX;
				s.vueY = view.panY;
				s.vueZ = view.zoom;
				s.vueInit = true;
				s.chemin = cheminActif;
				s.nom = doc.title;
				s.modifie = DocumentModifie();
			}

			/// Charge l'ardoise `i` dans l'etat vivant (sans ranger l'actif —
			/// c'est l'affaire de l'appelant : BasculerVers range, FermerOnglet
			/// ne range PAS l'onglet qui meurt).
			void ChargerSlot(uint32 i) {
				if (i >= (uint32)ouverts.Size())
					return;
				NkDocOuvert &s = ouverts[i];
				doc = s.doc;
				histoire = s.histoire;
				etatEnregistre = s.etatEnregistre;
				cheminActif = s.chemin;
				if (doc.IsValidIndex(s.selected))
					SelectSingle(s.selected);
				else
					SelectClear();
				if (s.vueInit) {
					view.panX = s.vueX;
					view.panY = s.vueY;
					view.zoom = s.vueZ > 0.01f ? s.vueZ : 1.f;
				} else {
					view.panX = kVueDefautX;
					view.panY = kVueDefautY;
					view.zoom = 1.f;
				}
				ongletActif = i;
				host.demoModels.Clear();
				host.SyncTo(doc);
				++editionGeneration; // les tampons d'Inspecteur se resynchronisent
				if (titre)
					titre(titreUser, DocumentModifie());
			}

			/// Cliquer un onglet : bascule le document ACTIF. Hierarchie, toile,
			/// Inspecteur, titre suivent tous — ils lisent deja `doc`.
			bool BasculerVers(uint32 i) {
				if (i >= (uint32)ouverts.Size() || i == ongletActif)
					return false;
				RangerActif();
				ChargerSlot(i);
				return true;
			}

			/// Ouvre un onglet INACTIF pour un document deja charge (les demos
			/// du demarrage). Rend l'index de l'onglet.
			int32 OuvrirOngletInactif(const NkUIDocument &d, const char *chemin) {
				NkDocOuvert s;
				s.doc = d;
				s.nom = d.title;
				s.chemin = NkString(chemin ? chemin : "");
				d.Save(s.etatEnregistre);
				s.histoire.Reinitialiser(s.etatEnregistre);
				ouverts.PushBack(s);
				return (int32)ouverts.Size() - 1;
			}

			/// « + > Vierge » et Ctrl+N : un NOUVEAU document devient un NOUVEL
			/// onglet — l'ancien reste ouvert (5e retour : plus d'ecrasement).
			void NouvelOngletVierge() {
				RangerActif();
				NkDocOuvert s;
				ouverts.PushBack(s);
				ongletActif = (uint32)ouverts.Size() - 1;
				cheminActif = NkString();
				BuildStarterDocument(); // pose doc, selection, historique, statut
				view.panX = kVueDefautX;
				view.panY = kVueDefautY;
				view.zoom = 1.f;
				++editionGeneration;
				if (titre)
					titre(titreUser, DocumentModifie());
			}

			/// LE [+] DE LA SECTION PAGES (6e retour) : cree une page VIDE sur la
			/// toile, A DROITE de la page de premier niveau la plus a droite
			/// (marge 80 — le placement des artboards neufs de Lunacy), nom
			/// « Page N » incremente jusqu'a etre unique. PAS de cible posee :
			/// la section CIBLE affiche « choisir un format… » — creation
			/// directe puis on change, le flux Lunacy (meme contrat qu'une page
			/// tracee a l'outil F : les DEUX gestes creent le meme objet).
			/// Annulable par l'observateur (gratuit). Rend l'index, -1 si refus.
			int32 CreerPage() {
				float32 x = 80.f, y = 48.f;
				bool premier = true;
				uint32 nPages = 0;
				for (uint32 i = 0; i < (uint32)doc.nodes.Size(); ++i) {
					const NkUINode &n = doc.nodes[i];
					if (n.parent < 0 || !doc.IsValidIndex(n.parent)
						|| doc.nodes[(uint32)n.parent].parent >= 0)
						continue; // seuls les enfants directs de la racine comptent
					if (!NkComponentDecl::StrEq(n.shape.Data(), "frame"))
						continue;
					++nPages;
					const float32 dr =
						n.posX
						+ (n.width.mode == NkSizeMode::Fixed ? n.width.value : 240.f);
					if (premier || dr + 80.f > x)
						x = dr + 80.f;
					if (premier || n.posY < y)
						y = n.posY;
					premier = false;
				}
				// « Page N » unique (N part du compte des pages + 1).
				char nom[32];
				for (uint32 essai = nPages + 1; essai < nPages + 100; ++essai) {
					snprintf(nom, sizeof(nom), "Page %u", essai);
					bool pris = false;
					for (uint32 i = 0; i < (uint32)doc.nodes.Size(); ++i)
						if (NkComponentDecl::StrEq(doc.nodes[i].label.Data(), nom))
							pris = true;
					if (!pris)
						break;
				}
				const int32 idx = doc.AddChild(0, "", NkAuthor::Humain);
				if (!doc.IsValidIndex(idx))
					return -1;
				NkUINode &n = doc.nodes[(uint32)idx];
				n.label = NkString(nom);
				n.shape = NkString("frame");
				n.layout.kind = NkLayoutKind::Free; // une page RECOIT des formes
				n.posX = x;
				n.posY = y;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = 390.f;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = 844.f;
				doc.MarkHumanEdit(idx);
				SelectSingle(idx);
				return idx;
			}

			/// Un document d'ardoise est-il modifie ? (serialisation comparee —
			/// la meme MESURE que la pastille, jamais un drapeau).
			bool SlotModifie(const NkDocOuvert &s) const {
				NkString now;
				s.doc.Save(now);
				const char *a = now.Data() ? now.Data() : "";
				const char *b = s.etatEnregistre.Data() ? s.etatEnregistre.Data() : "";
				while (*a && *a == *b) {
					++a;
					++b;
				}
				return *a != *b;
			}

			/// La croix : ferme l'onglet. Un document MODIFIE ne se ferme pas en
			/// silence — Ctrl+S d'abord (le dialogue « enregistrer avant de
			/// fermer ? » est un chantier nomme, pas une promesse muette).
			bool FermerOnglet(uint32 i) {
				if (i >= (uint32)ouverts.Size())
					return false;
				const bool actif = (i == ongletActif);
				const bool mod = actif ? DocumentModifie() : SlotModifie(ouverts[i]);
				if (mod) {
					DireAuPied("Onglet non enregistré — Ctrl+S d'abord (le dialogue de "
							   "confirmation arrive).");
					return false;
				}
				if ((uint32)ouverts.Size() == 1) {
					// le dernier onglet ne disparait pas : il se VIDE (un editeur
					// sans document n'existe pas — Lunacy repart d'un canvas neuf).
					cheminActif = NkString();
					BuildStarterDocument();
					ouverts[0] = NkDocOuvert();
					ouverts[0].nom = doc.title;
					view.panX = kVueDefautX;
					view.panY = kVueDefautY;
					view.zoom = 1.f;
					++editionGeneration;
					DireAuPied("Dernier onglet : document remplacé par un neuf.");
					return true;
				}
				if (actif) {
					const uint32 v = (i + 1 < (uint32)ouverts.Size()) ? i + 1 : i - 1;
					ChargerSlot(v); // sans ranger l'onglet qui meurt
				}
				ouverts.RemoveAt(i);
				if (ongletActif > i)
					--ongletActif;
				DireAuPied("Onglet fermé.");
				return true;
			}
			/// Un renommage d'arbre (Hierarchie) est en cours : les raccourcis
			/// lettres de la toile se taisent (les lettres sont une saisie).
			/// Pose par HierarchyPanel a chaque image.
			bool renommageArbre = false;
			/// ⚠️ LE MEME CANAL, POUR LA TOILE (01/09). `mEditNode` et `mForage`
			///    vivent dans PreviewPanel — ils sont a lui. Mais les COMMANDES de
			///    la coquille (Ctrl+D, Ctrl+G, Ctrl+Maj+G) tournent HORS de tout
			///    panneau, et le menu Edition aussi : ils doivent savoir « une
			///    saisie est-elle ouverte ? » et « a quel niveau de forage
			///    est-on ? ». Un DEUXIEME etat d'edition aurait diverge des le
			///    premier Echap — on PUBLIE donc l'unique, une ecriture par image,
			///    exactement comme `renommageArbre` au-dessus. (Le decalage d'une
			///    image ne se voit pas : l'etat d'edition ne change jamais sans
			///    qu'une image passe.)
			int32 editionToile = -1; ///< noeud en edition en place, -1 = aucune
			int32 forageToile = -1;	 ///< groupe fore courant, -1 = premier niveau
			/// Vrai des qu'une SAISIE est ouverte, ou que ce soit. Les gestes
			/// d'edition s'y taisent : un Ctrl+D pendant qu'on tape un nom
			/// dupliquerait un noeud a l'insu de la main qui ecrivait.
			bool SaisieOuverte() const { return renommageArbre || editionToile >= 0; }
			/// Ctrl+A : TOUT au niveau courant du forage (Lunacy) — les enfants du
			/// groupe fore, sinon le premier niveau. ⚠️ ICI et pas dans la toile :
			/// le menu Edition le declenche aussi, et deux ecritures auraient
			/// donne deux definitions de « le niveau courant ».
			uint32 ToutSelectionner() {
				const int32 ctxA = doc.IsValidIndex(forageToile) ? forageToile : 0;
				sel.Clear();
				const NkVector<int32> &kids = doc.nodes[(uint32)ctxA].children;
				for (uint32 k = 0; k < (uint32)kids.Size(); ++k)
					sel.Add(kids[k]);
				selected = sel.Primary();
				char msg[48];
				snprintf(msg, sizeof(msg), "Sélection : %u élément(s).", sel.Count());
				status = NkString(msg);
				return sel.Count();
			}
			/// Mise en scene (--annuler=N / --retablir=N) : N pas au lancement,
			/// consommes par la toile quand le document est la.
			int32 annulerInitial = 0;
			int32 retablirInitial = 0;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  LE MENU DU CLIC DROIT — DESSINÉ UNE FOIS POUR LES DEUX SURFACES
	// ═══════════════════════════════════════════════════════════════════════════
	//  ⚠️ TROISIÈME RETOUR DU COUPLE TOILE / HIÉRARCHIE sur ce chantier, après
	//     les tables de clic et l'englobant. La leçon a fini par prendre : le
	//     menu est écrit AVANT que le second chemin existe, pas après que le
	//     premier a mordu. La TABLE des entrées vit dans `MenuContexte.h` (sans
	//     NKGui, donc mesurable) ; ce qui suit n'est que le DESSIN et
	//     l'EXÉCUTION.
	//
	//  ⚠️ ET LA RANGÉE D'ICÔNES SE PEINT ICI, pas dans le kit : il n'existe
	//     aucun atlas partagé, et le kit tient déjà la place et l'état
	//     (`NkCtxMenuRangee`). Même patron que `rowOverlay` de l'arbre.

	/// Le peintre d'une icône de la rangée — le vocabulaire vectoriel local
	/// (`Costume.h`), pas un atlas inventé pour l'occasion.
	inline void NkPeindreIconeCtx(void *, NkGuiDrawList &dl, nkentseu::int32 i, const NkRect &cell,
								  const NkColor &c) {
		const float32 x = cell.x + (cell.w - 16.f) * 0.5f;
		const float32 y = cell.y + (cell.h - 16.f) * 0.5f;
		switch ((NkIconeCtx)i) {
			// ⚠️ TROIS PICTOGRAMMES ONT ÉTÉ DESSINÉS POUR DE BON APRÈS LA
			//    PREMIÈRE CAPTURE. La version d'avant reprenait « le plus proche
			//    voisin » : une FLÈCHE pour couper, une CROIX pour verrouiller.
			//    À l'écran la flèche se lit « aller à », pas « ciseaux ». Je
			//    m'étais moi-même écrit qu'un pictogramme faux est pire qu'un
			//    pictogramme approximatif — le mien était faux, pas approximatif.
			//    C'est la capture qui l'a dit, pas la relecture.
			case NkIconeCtx::Coller: costume::IcDocOnglet(dl, x, y, c); break;
			case NkIconeCtx::Dupliquer: costume::IcCarreaux(dl, x, y, c); break;
			case NkIconeCtx::Couper: costume::IcCiseaux(dl, x + 2.f, y + 2.f, c); break;
			case NkIconeCtx::Verrouiller: costume::IcCadenas(dl, x + 2.f, y + 2.f, c); break;
			case NkIconeCtx::Masquer: costume::IcOeil(dl, x, y, c); break;
			case NkIconeCtx::Supprimer: costume::IcPoubelle(dl, x, y, c); break;
			default: costume::IcComposant(dl, x + 2.f, y + 2.f, c); break;
		}
	}

	/// Le contexte lu depuis l'état — une seule lecture pour les deux surfaces.
	inline NkContexteCtx NkContexteDepuisEtat(const DesignState &st, nkentseu::int32 noeud,
											  bool surfaceListe) {
		NkContexteCtx c;
		if (!st.doc.IsValidIndex(noeud))
			return c;
		const NkUINode &n = st.doc.nodes[(nkentseu::uint32)noeud];
		c.aTexte = NkComponentDecl::StrEq(n.shape.Data(), "text") || !n.text.Empty();
		c.estCadre = NkComponentDecl::StrEq(n.shape.Data(), "frame");
		c.estTexte = NkComponentDecl::StrEq(n.shape.Data(), "text");
		c.aEnfants = !n.children.Empty();
		c.pasRacine = (noeud != 0);
		c.pressePapiersPlein = st.pressePapiersPlein;
		c.surfaceListe = surfaceListe;
		return c;
	}

	/// LE DESSIN. Rend l'action choisie, ou `NkActionCtx::NB` si rien.
	/// ⚠️ L'ACTION VOYAGE, PAS L'INDEX. Un index d'affichage change dès qu'on
	///    insère une entrée, et l'appelant exécuterait alors le geste voisin —
	///    le défaut classique des menus, et il est silencieux.
	inline NkActionCtx NkDessinerMenuCtx(NkGuiContext &ctx, editorkit::NkCtxMenu &mn,
										 const DesignState &st, nkentseu::int32 noeud,
										 bool surfaceListe, char *filtre, nkentseu::int32 filtreCap,
										 bool *filtreFocus) {
		const NkContexteCtx c = NkContexteDepuisEtat(st, noeud, surfaceListe);
		NkMenuCtx menu;
		NkConstruireMenuCtx(c, menu);
		const char *items[kMaxEntreesCtx];
		const char *raccourcis[kMaxEntreesCtx];
		bool en[kMaxEntreesCtx], sub[kMaxEntreesCtx], sep[kMaxEntreesCtx];
		for (nkentseu::uint32 i = 0; i < menu.n; ++i) {
			items[i] = menu.items[i].libelle;
			raccourcis[i] = menu.items[i].raccourci;
			en[i] = menu.items[i].agit;
			sub[i] = menu.items[i].sousMenu;
			sep[i] = menu.items[i].sepApres;
		}
		// La rangée d'icônes, dans le MÊME contexte que les entrées : deux
		// lectures d'applicabilité auraient donné une poubelle active au-dessus
		// d'un « Supprimer » grisé.
		bool iconEn[(nkentseu::uint32)NkIconeCtx::NB];
		for (nkentseu::uint32 i = 0; i < (nkentseu::uint32)NkIconeCtx::NB; ++i)
			iconEn[i] = NkIconeCtxAgit((NkIconeCtx)i, c);
		struct TipUser {
				const NkContexteCtx *ctxt;
		} tu{&c};
		editorkit::NkCtxMenuRangee rangee;
		rangee.count = (nkentseu::int32)NkIconeCtx::NB;
		rangee.enabled = iconEn;
		rangee.paint = &NkPeindreIconeCtx;
		rangee.user = &tu;
		rangee.tip = [](void *u, nkentseu::int32 i) -> const char * {
			auto *t = static_cast<TipUser *>(u);
			return NkInfobulleIconeCtx((NkIconeCtx)i, NkIconeCtxAgit((NkIconeCtx)i, *t->ctxt));
		};
		const nkentseu::int32 act = editorkit::NkCtxMenuDraw(
			ctx, mn, items, en, (nkentseu::int32)menu.n, nullptr, sub, nullptr, filtre, filtreCap,
			filtreFocus, raccourcis, sep, &rangee);
		if (rangee.clicked >= 0)
			return NkActionIconeCtx((NkIconeCtx)rangee.clicked);
		if (act >= 0 && (nkentseu::uint32)act < menu.n)
			return menu.items[(nkentseu::uint32)act].action;
		return NkActionCtx::NB;
	}

	/// LE MENU DU CLIC DROIT **DANS LE VIDE** — le menu de VUE.
	/// ⚠️ IL PASSE PAR LE MÊME `NkCtxMenuDraw` QUE L'AUTRE, et c'est tout
	///    l'intérêt d'avoir fait grossir le kit plutôt que d'écrire ici : le
	///    champ de recherche, le défilement, les traits de groupe, l'occlusion et
	///    maintenant les COCHES sont un seul code. Un second peintre de menu
	///    aurait divergé au premier ajustement — le dépôt a déjà mesuré ce prix.
	inline NkActionVide NkDessinerMenuVide(NkGuiContext &ctx, editorkit::NkCtxMenu &mn,
										   const DesignState &st, char *filtre,
										   nkentseu::int32 filtreCap, bool *filtreFocus) {
		NkContexteVide c;
		c.pressePapiersPlein = st.pressePapiersPlein;
		c.grilleVisible = st.grilleVisible;
		c.aimantCalques = st.aimantActif;
		NkMenuVide menu;
		NkConstruireMenuVide(c, menu);
		const char *items[kMaxEntreesVide];
		const char *raccourcis[kMaxEntreesVide];
		bool en[kMaxEntreesVide], sep[kMaxEntreesVide], chk[kMaxEntreesVide];
		for (nkentseu::uint32 i = 0; i < menu.n; ++i) {
			items[i] = menu.items[i].libelle;
			raccourcis[i] = menu.items[i].raccourci;
			en[i] = menu.items[i].agit;
			sep[i] = menu.items[i].sepApres;
			chk[i] = menu.items[i].coche;
		}
		const nkentseu::int32 act =
			editorkit::NkCtxMenuDraw(ctx, mn, items, en, (nkentseu::int32)menu.n, nullptr, nullptr,
									 nullptr, filtre, filtreCap, filtreFocus, raccourcis, sep,
									 nullptr, chk);
		if (act >= 0 && (nkentseu::uint32)act < menu.n)
			return menu.items[(nkentseu::uint32)act].action;
		return NkActionVide::NB;
	}

	/// L'EXÉCUTION DE CE QUI EST COMMUN AUX DEUX SURFACES.
	/// ⚠️ AUCUN GESTE N'EST RÉÉCRIT : ce sont les MÊMES méthodes que le clavier
	///    et que le menu Édition. Trois portes, une écriture — c'est la
	///    condition pour qu'une correction les atteigne toutes.
	/// Rend vrai si l'action a été traitée ici (les deux qui restent —
	/// `EditerTexte` et `Renommer` — dépendent de la surface).
	inline bool NkAppliquerActionCtx(DesignState &st, nkentseu::int32 noeud, NkActionCtx a) {
		switch (a) {
			case NkActionCtx::Copier: st.CopierSelection(); return true;
			case NkActionCtx::Couper: st.CouperSelection(); return true;
			case NkActionCtx::Coller: st.CollerPressePapiers(); return true;
			case NkActionCtx::Dupliquer: st.DupliquerSelection(); return true;
			case NkActionCtx::Grouper: st.GrouperSelection(); return true;
			case NkActionCtx::Degrouper:
				st.SelectSingle(noeud);
				st.DegrouperSelection();
				return true;
			case NkActionCtx::Supprimer: st.SupprimerSelection(); return true;
			default: return false;
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 1 — LA PALETTE
	// ═══════════════════════════════════════════════════════════════════════════
	// ⚠️ ELLE BOUCLE SUR LE REGISTRE, elle ne nomme aucun composant. Une liste
	//    ecrite en dur afficherait aujourd'hui les bons noms sans qu'aucune
	//    declaration soit lue — elle « marcherait » en ne prouvant rien, et le
	//    jour ou un second composant arrive elle ne l'afficherait pas.
	class PalettePanel : public NkEditorPanel {
		public:
			explicit PalettePanel(DesignState *st)
				: NkEditorPanel("Palette", NkEditorDockSide::NK_LEFT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				designkit::releve::Zone(ctx, "palette");
				ec.Text("Ce que la bibliothèque déclare");
				ec.Separator();
				(void)ctx;

				// ⚠️ LA PALETTE EST UN CHOIX EXCLUSIF, DONC ELLE PASSE PAR LE MEME
				//    CONTROLE QUE LES AUTRES. Elle empilait des `Selectable` a la
				//    main ; deux raisons de changer, et la seconde a ete MESUREE :
				//      - c'est bien un choix parmi N, et le dire par la forme evite
				//        qu'il se lise comme une liste ou l'on coche ;
				//      - un `Selectable` pose en largeur AUTOMATIQUE publie un
				//        rectangle de **1 px** dans `ctx.lastItemRect` (releve :
				//        `palette.composant.tree_view = 10.0 107.0 1.0 28.0`),
				//        alors que le meme widget dans une rangee explicite publie
				//        `120x28`. Un essai a la souris ne pouvait donc PAS viser
				//        la palette. Le controle segmente donne des cellules a
				//        largeur explicite, et le rectangle publie devient utile.
				//
				//    ⚠️ RECTIFICATION DU 19/08, ET ELLE M'ACCUSE : j'avais conclu de
				//       ces chiffres que `nkgui::Selectable` publiait un rectangle
				//       FAUX hors rangee explicite, et je l'ai porte au canal comme
				//       un defaut de NKGui. **C'etait faux.** Les trois releves ont
				//       ete pris pendant que le panneau n'etait PAS dessine (region
				//       de largeur 0) : ce n'est pas le widget qui mentait, c'est
				//       moi qui mesurais un panneau ferme. Panneau ouvert, le meme
				//       `Selectable` en flot publie **120x28**.
				//       *Une mesure prise dans un etat qu'on n'a pas verifie
				//       n'accuse que celui qui la publie.*
				//
				//    La forme reste la bonne pour une autre raison, et elle suffit :
				//    c'est un choix EXCLUSIF, et le controle segmente le dit.
				//
				//    L'entree 0 n'est pas un composant : c'est le CADRE, un noeud
				//    qui n'affiche rien et sert a agencer. Il est dans la palette
				//    parce qu'une composition en a besoin avant qu'un composant
				//    conteneur existe.
				const uint16 n = NkComponentRegistry::Count();
				const char *choix[16];
				uint16 nb = 0;
				choix[nb++] = "cadre";
				for (uint16 i = 0; i < n && nb < 16; ++i) {
					const NkComponentDecl *d = NkComponentRegistry::At(i);
					// ⚠️ LE NOM DECLARE, pas le libelle affiche : le libelle est
					//    destine a devenir une cle de traduction, et un essai qui
					//    viserait « Arbre » cesserait de trouver sa cible le jour
					//    du multilingue.
					choix[nb++] = d ? d->name : "?";
				}
				const int32 pick = designkit::Segmented(ctx, choix, (int32)nb, mSt->paletteChoice,
														"palette.composant");
				if (pick >= 0)
					mSt->paletteChoice = pick;

				ec.Separator();
				char b[192];
				designkit::KeyValue(ctx, "cible",
									mSt->doc.IsValidIndex(mSt->selected)
										? mSt->doc.nodes[(uint32)mSt->selected].label.Data()
										: "(aucune)");
				if (designkit::Button(ctx, "Poser dans la sélection", "palette.poser"))
					Place();

				const NkComponentDecl *d = Chosen();
				if (d) {
					// ⚠️ DEUX PHRASES DE SIX NOMBRES SONT DEVENUES SIX LIGNES
					//    ALIGNEES. « 4 parametres, 3 variantes, 10 jetons » se
					//    lisait comme une phrase — donc se coupait comme une
					//    phrase. Un chiffre par ligne, la cle a gauche : rien a
					//    tronquer, et on compare deux composants d'un coup d'oeil.
					if (designkit::Section(ctx, "Ce que ce composant déclare")) {
						snprintf(b, sizeof(b), "%u", d->paramCount);
						designkit::KeyValue(ctx, "parametres", b);
						snprintf(b, sizeof(b), "%u", d->variantCount);
						designkit::KeyValue(ctx, "variantes", b);
						snprintf(b, sizeof(b), "%u", d->tokenCount);
						designkit::KeyValue(ctx, "jetons", b);
						snprintf(b, sizeof(b), "%u", d->metricCount);
						designkit::KeyValue(ctx, "metriques", b);
						snprintf(b, sizeof(b), "%u", d->hookCount);
						designkit::KeyValue(ctx, "greffes", b);
						snprintf(b, sizeof(b), "%u", d->eventCount);
						designkit::KeyValue(ctx, "evenements", b);
					}
					// ⚠️ `TextWrapped`, PAS `Text` : un resume de deux lignes est
					//    fait pour revenir a la ligne, pas pour etre coupe.
					nkgui::TextWrapped(ctx, d->summary ? d->summary : "");
				} else {
					nkgui::TextWrapped(ctx, "Un cadre ne déclare rien : il agence ses enfants.");
				}

				// ── CE QUE LA CANONISATION A RATTRAPE ────────────────────────
				// ⚠️ SANS CETTE LIGNE, LE CORRECTIF (a) SE RETOURNERAIT CONTRE
				//    NOUS : l'ecran serait juste, les declarations resteraient
				//    fausses, et personne n'irait verifier -- « une protection qui
				//    empeche d'aller verifier ». Elle est la liste de travail de
				//    la correction a la source, pas une decoration.
				if (NkRoleAudit::RescuedCount() > 0) {
					ec.Separator();
					snprintf(b, sizeof(b), "%u", NkRoleAudit::RescuedCount());
					designkit::KeyValue(ctx, "roles PascalCase", b);
					nkgui::TextWrapped(ctx, "Rattrapes par la canonisation ; a corriger a la source "
											"(NKEditorKit). L'écran est juste, la déclaration ne "
											"l'est pas.");
				}
			}

		private:
			const NkComponentDecl *Chosen() const {
				if (mSt->paletteChoice <= 0)
					return nullptr;
				return NkComponentRegistry::At((uint16)(mSt->paletteChoice - 1));
			}
			void Place() {
				const NkComponentDecl *d = Chosen();
				const int32 created = mSt->doc.AddChild(mSt->selected, d ? d->name : "", NkAuthor::Humain);
				if (created < 0) {
					mSt->status = NkString("Pose refusée : cible invalide ou composant inconnu.");
					return;
				}
				mSt->SelectSingle(created);
				mSt->host.SyncTo(mSt->doc);
				mSt->status = NkString("Pose : ");
				mSt->status.Append(mSt->doc.nodes[(uint32)created].label);
			}
			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 2 — L'ARBRE DE COMPOSITION
	// ═══════════════════════════════════════════════════════════════════════════
	// « une interface complete est un composant qui en contient d'autres — meme
	// mecanisme aux deux echelles ». Cet arbre est la vue de ce mecanisme.
	class CompositionPanel : public NkEditorPanel {
		public:
			explicit CompositionPanel(DesignState *st)
				: NkEditorPanel("Composition", NkEditorDockSide::NK_LEFT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				designkit::releve::Zone(ctx, "composition");
				ec.Text(mSt->doc.title.Data());

				// ── L'ARBRE, DANS UNE ZONE DEFILABLE ─────────────────────────
				// ⚠️ IL ETAIT POSE A NU dans le panneau : un document de trente
				//    noeuds poussait tout le reste — operations, metriques,
				//    boutons — hors de l'ecran, sans barre pour y revenir. Un
				//    panneau dont le contenu depend de la taille du document n'est
				//    pas un panneau, c'est une liste qui a debordе.
				const NkRect arbre = ctx.NextItemRect(-1.f, 220.f);
				if (nkgui::BeginChild(ctx, "nkuidesign.arbre", arbre, true)) {
					DrawNode(ctx, 0, 0);
					nkgui::EndChild(ctx);
				}

				// ── Les operations de structure, EN FLOT ─────────────────────
				// ⚠️ TOUTES ECRIVENT UN PARENT OU UN RANG. Aucune ne deplace un
				//    rectangle : reordonner n'est pas glisser.
				// ⚠️ ET LEURS LIBELLES ONT ETE RACCOURCIS APRES MESURE, pas par
				//    gout : « Rentrer dans le voisin du dessus » s'affichait
				//    « Rentrer dans le voisin du dessu… ». Deux corrections, et il
				//    faut les deux — un libelle qui tient, ET un conteneur qui
				//    mesure (`BeginFlow` donne a chaque bouton la largeur de son
				//    texte et passe a la ligne). L'une sans l'autre retronquerait
				//    au premier panneau retreci.
				{
					designkit::Flow f(ctx);
					if (designkit::Button(ctx, "Monter", "compo.monter"))
						Reorder(-1);
					if (designkit::Button(ctx, "Descendre", "compo.descendre"))
						Reorder(+1);
					if (designkit::Button(ctx, "Imbriquer", "compo.imbriquer"))
						NestIntoPreviousSibling();
					if (designkit::Button(ctx, "Sortir", "compo.sortir"))
						Outdent();
					if (designkit::Button(ctx, "Supprimer", "compo.supprimer"))
						Remove();
				}

				// ── LES METRIQUES DU DOCUMENT ────────────────────────────────
				// Une valeur, tous les noeuds qui la nomment. C'est le benefice
				// direct de la regle « un espacement se nomme » : l'aeration de
				// l'interface entiere se regle ici, pas noeud par noeud.
				if (designkit::Section(ctx, "Métriques du document")) {
					for (uint32 i = 0; i < (uint32)mSt->doc.metrics.Size(); ++i) {
						float32 v = mSt->doc.metrics[i].value;
						if (nkgui::DragFloat(ctx, mSt->doc.metrics[i].name.Data(), v, 0.25f, 0.f, 64.f))
							mSt->doc.metrics[i].value = v;
					}
				}

				// ⚠️ L'EN-TETE PUBLIE SON RECTANGLE : une section repliee cache ses
				//    boutons, et un essai qui les viserait trouverait un rectangle
				//    perime. Publier l'en-tete permet de l'OUVRIR d'abord.
				const bool docOuvert = designkit::Section(ctx, "Document");
				designkit::releve::Cle(ctx, "compo.section_document");
				if (docOuvert) {
					// ⚠️ TROIS CHIFFRES, TROIS LIGNES « cle : valeur » — au lieu de
					//    la phrase « 5 noeud(s) — 0 pose(s) par l'IA, 0 corrige(s) »
					//    qui s'affichait « … 0 pose(s) par l'IA, 0… ». Une phrase se
					//    fait couper ; deux colonnes alignees, non.
					char b[64];
					snprintf(b, sizeof(b), "%u", mSt->doc.NodeCount());
					designkit::KeyValue(ctx, "noeuds", b);
					snprintf(b, sizeof(b), "%u", mSt->doc.CountByAuthor(NkAuthor::IA));
					designkit::KeyValue(ctx, "posés par l'IA", b);
					snprintf(b, sizeof(b), "%u", mSt->doc.CountCorrected());
					designkit::KeyValue(ctx, "corriges", b);
					{
						designkit::Flow f(ctx);
						if (designkit::Button(ctx, "Enregistrer", "doc.enregistrer"))
							mSt->SaveDoc();
						if (designkit::Button(ctx, "Recharger", "doc.recharger"))
							mSt->LoadDoc();
						if (designkit::Button(ctx, "Nouveau", "doc.nouveau"))
							mSt->BuildStarterDocument();
					}
				}

				ec.Separator();
				nkgui::TextWrapped(ctx, mSt->status.Data() ? mSt->status.Data() : "");
			}

		private:
			void DrawNode(NkGuiContext &ctx, int32 node, int32 depth) {
				if (!mSt->doc.IsValidIndex(node))
					return;
				const NkUINode &n = mSt->doc.nodes[(uint32)node];
				char line[256];
				char indent[32];
				int32 k = 0;
				for (; k < depth * 2 && k < 30; ++k)
					indent[k] = ' ';
				indent[k] = 0;
				// La provenance se lit DANS l'arbre : un document ou l'on ne voit pas
				// ce que la machine a pose est un document ou personne ne relit ce que
				// la machine a pose.
				const char *mark = n.prov.corrected				  ? " [ia/corrige]"
								   : (n.prov.author == NkAuthor::IA) ? " [ia]"
																	 : "";
				snprintf(line, sizeof(line), "%s%s%s%s", indent, n.label.Data(),
						 n.IsFrame() ? " (cadre)" : "", mark);
				// La selection de l arbre EST celle du canvas (doc 3 §11.5).
				if (Selectable(ctx, line, mSt->sel.Contains(node)))
					mSt->SelectSingle(node);
				for (uint32 i = 0; i < (uint32)n.children.Size(); ++i)
					DrawNode(ctx, n.children[i], depth + 1);
			}

			int32 RankInParent(int32 node) const {
				if (!mSt->doc.IsValidIndex(node))
					return -1;
				const int32 p = mSt->doc.nodes[(uint32)node].parent;
				if (!mSt->doc.IsValidIndex(p))
					return -1;
				const NkVector<int32> &kids = mSt->doc.nodes[(uint32)p].children;
				for (uint32 i = 0; i < (uint32)kids.Size(); ++i)
					if (kids[i] == node)
						return (int32)i;
				return -1;
			}
			void Reorder(int32 delta) {
				const int32 r = RankInParent(mSt->selected);
				if (r < 0)
					return;
				if (mSt->doc.MoveChild(mSt->selected, r + delta))
					mSt->doc.MarkHumanEdit(mSt->selected);
			}
			/// Imbriquer : le noeud entre DANS son voisin du dessus. C'est le geste
			/// « poser un composant dans un autre » sous sa forme clavier ; a la
			/// souris ce sera un glisser, et il appellera exactement le meme
			/// `Reparent`.
			void NestIntoPreviousSibling() {
				const int32 r = RankInParent(mSt->selected);
				if (r <= 0)
					return;
				const int32 p = mSt->doc.nodes[(uint32)mSt->selected].parent;
				const int32 target = mSt->doc.nodes[(uint32)p].children[(uint32)(r - 1)];
				if (mSt->doc.Reparent(mSt->selected, target)) {
					mSt->doc.MarkHumanEdit(mSt->selected);
					mSt->status = NkString("Imbrique dans : ");
					mSt->status.Append(mSt->doc.nodes[(uint32)target].label);
				}
			}
			void Outdent() {
				if (!mSt->doc.IsValidIndex(mSt->selected))
					return;
				const int32 p = mSt->doc.nodes[(uint32)mSt->selected].parent;
				if (!mSt->doc.IsValidIndex(p))
					return;
				const int32 gp = mSt->doc.nodes[(uint32)p].parent;
				if (!mSt->doc.IsValidIndex(gp))
					return;
				if (mSt->doc.Reparent(mSt->selected, gp))
					mSt->doc.MarkHumanEdit(mSt->selected);
			}
			void Remove() {
				// Le geste vit dans DesignState (SupprimerSelection) : le menu
				// contextuel de la toile l'appelle aussi — une seule reparation
				// de selection apres renumerotation, pas deux.
				(void)mSt->SupprimerSelection();
			}
			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 3 — L'APERCU
	// ═══════════════════════════════════════════════════════════════════════════
	// C'est le CONSOMMATEUR. Il n'affiche pas une image du document : il calcule
	// sa mise en page et appelle les fonctions de dessin memes que l'application
	// finale appellera.
	class PreviewPanel : public NkEditorPanel {
			/// La recette du contrat universel d'edition (--recette-edition)
			/// exerce FermerEditionTexte et HandleMouse sans fenetre — l'acces
			/// de banc, pas une seconde interface.
			friend struct RecetteEditionAcces;

		public:
			explicit PreviewPanel(DesignState *st)
				: NkEditorPanel("Aperçu", NkEditorDockSide::NK_CENTER), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				designkit::releve::Zone(ctx, "apercu");
				// ── L'INSTRUMENT DE FLUIDITE (mandat de nuit, 01/09) ─────────
				// « fluide » se MESURE, pas se ressent : le cout de CETTE image
				// de toile (ms), min/moy/max PENDANT le geste en cours (drag,
				// trace, edition, pan) — publie au releve `canvas.fluidite` =
				// [min, moy, max, n images du geste]. La moyenne glissante hors
				// geste part dans `canvas.image_ms` = [moyenne 60 img, derniere].
				// Un destructeur : la mesure couvre l'image ENTIERE de la toile,
				// quel que soit le chemin de sortie.
				struct MesureFluidite {
						PreviewPanel *p;
						nkgui::NkGuiContext *cx;
						nkentseu::NkElapsedTime t0;
						~MesureFluidite() {
							const float32 ms =
								(float32)(nkentseu::NkChrono::Now() - t0).ToMilliseconds();
							PreviewPanel &c = *p;
							// moyenne glissante (60 images) — geste ou pas
							c.mFluRoule = c.mFluRoule * 0.9833f + ms * 0.0167f;
							const bool geste = c.mDragging || c.mCreating || c.mMainPan
											   || c.mMarquee || c.mResizeEdges != 0
											   || c.mSt->doc.IsValidIndex(c.mEditNode);
							if (geste) {
								if (c.mFluN == 0) {
									c.mFluMin = c.mFluMax = ms;
									c.mFluSomme = 0.f;
								}
								if (ms < c.mFluMin)
									c.mFluMin = ms;
								if (ms > c.mFluMax)
									c.mFluMax = ms;
								c.mFluSomme += ms;
								++c.mFluN;
							} else if (c.mFluN > 0) {
								// le geste vient de finir : ses chiffres restent
								// publies jusqu'au geste suivant
								c.mFluDMin = c.mFluMin;
								c.mFluDMoy = c.mFluSomme / (float32)c.mFluN;
								c.mFluDMax = c.mFluMax;
								c.mFluDN = c.mFluN;
								c.mFluN = 0;
							}
							const bool enCours = c.mFluN > 0;
							nkgui::NkGuiNoterMesure(
								*cx, "canvas.fluidite",
								enCours ? c.mFluMin : c.mFluDMin,
								enCours ? (c.mFluSomme / (float32)c.mFluN) : c.mFluDMoy,
								enCours ? c.mFluMax : c.mFluDMax,
								(float32)(enCours ? c.mFluN : c.mFluDN));
							nkgui::NkGuiNoterMesure(*cx, "canvas.image_ms", c.mFluRoule, ms,
													0.f, 0.f);
						}
				} mesureFluidite{this, &ctx, nkentseu::NkChrono::Now()};
				// ⚠️ LES QUATRE LIGNES D'AIDE ONT ÉTÉ RETIRÉES, ET C'EST LE POINT.
				//    Elles énuméraient les gestes de la souris EN HAUT DE LA TOILE :
				//    un affichage de MISE AU POINT, utile pendant qu'on branchait le
				//    zoom et le glisser, et qui n'est nulle part dans le plan de la
				//    fenêtre. Ce qu'on montre à Rodolf ne doit pas porter les traces
				//    de la séance qui l'a construit.
				//    📌 Le geste juste, quand il existera : l'infobulle (§14quinquies)
				//       et le cluster de toile (§4), pas un paragraphe posé sur le
				//       dessin. Signalé plutôt que tu.

				// ⚠️ DIRE QUAND IL N Y A RIEN A VOIR. Le 2026-08-28 au matin, Rodolf a
				//    lance le binaire et n a vu AUCUNE toile -- parce que son document
				//    enregistre (18 aout) ne porte que des agencements `column` et
				//    `row`, donc aucun noeud pose a des coordonnees. La machinerie
				//    etait branchee et n avait rien a projeter.
				//
				//    Un outil qui ne montre rien doit DIRE pourquoi. Sans cette ligne,
				//    « je ne vois pas la toile » et « la toile est cassee » se
				//    ressemblent -- et on cherche le defaut du mauvais cote.
				bool aUneToile = false;
				for (uint32 i = 0; i < (uint32)mSt->doc.nodes.Size(); ++i)
					if (mSt->doc.nodes[i].layout.kind == NkLayoutKind::Free)
						aUneToile = true;
				if (!aUneToile)
					// ⚠️ LA PHRASE DISAIT VRAI ET SE LISAIT FAUX. « Aucune page de
					//    toile » parlait des pages a agencement `Free` -- celles ou
					//    l'on POSE des formes. Mais le document EST dessine juste
					//    en dessous (cadres `Column`/`Row` et deux composants), et
					//    l'ecran contredisait donc la phrase. Mesure : 5 nœuds, 0 en
					//    `Free`, et 5 rectangles publies par `--dump-ui`.
					//    Un diagnostic vrai qui se lit comme « rien n'est dessine »
					//    envoie chercher le defaut du mauvais cote -- exactement ce
					//    qu'il devait eviter.
					ec.Text("(ce document est agencé en colonnes et rangées : il n'a aucune page "
							"« libre » où poser des formes à la souris — Ctrl+N en ouvre une)");

				// ── LE REPLI FRANC, A L'ENDROIT OU LE MAGENTA APPARAIT ───────
				// ⚠️ C'EST LA MOITIE (b) DU CORRECTIF DU 18/08. Le magenta de
				//    `NkTheme::Get` disait « un role est faux » et rien d'autre :
				//    ni lequel, ni combien, ni dans quel composant. Il a fallu
				//    lire trois fichiers pour le savoir. Ce bandeau le DIT, juste
				//    au-dessus du dessin fautif -- et il ne s'affiche pas quand
				//    tout va bien, sinon on cesserait de le lire.
				// ⚠️ LA CONDITION LIT LES DEUX REGISTRES, comme le resume les
				//    compte. Sur le seul compte de l'application, le bandeau se
				//    serait TU des qu'on corrigeait le defaut applicatif -- en
				//    laissant vivre celui du kit, magenta a l'ecran et plus rien
				//    pour le dire.
				if (NkRoleAudit::FaultCount() + nkentseu::editorkit::NkRoleAudit::FaultCount() > 0) {
					NkString resume;
					NkRoleAudit::Summary(resume, 6);
					ec.Text("!! ROLE(S) DE THÈME NON RÉSOLU(S) -- ce qui suit est peint en magenta :");
					ec.Text(resume.Data());
				}
				// ⚠️ « CANVAS — PREND TOUT L'ESPACE RESTANT » (plan de la fenêtre).
				//    C'était `NextItemRect(-1.f, 520.f)` : une hauteur ÉCRITE EN
				//    DUR, donc une toile qui laissait un tiers de fenêtre vide en
				//    bas et qui débordait dès qu'on rapetissait la fenêtre. La
				//    hauteur se MESURE — même mesure que la Hiérarchie, même
				//    fonction.
				// ⚠️ LA MARGE SOUSTRAITE EST L'ESPACEMENT D'ITEM, PAS 4 px — mesure
				//    du 30/08 (Rodolf : « je ne vois pas l'utilité du scrollbar dans
				//    la page principale ») : `NextItemRect` avance le curseur de
				//    `itemSpacingY` (6) APRÈS la toile ; avec 4 px de marge, le
				//    contenu dépassait de 2 px — un ascenseur au pouce quasi plein,
				//    inutile et faux sur une toile INFINIE qui se navigue à la
				//    molette et au bouton du milieu. Le débordement à zéro, le
				//    cadre de défilement de la fenêtre n'a plus rien à dessiner.
				const float32 hToile = designkit::HauteurVisibleBas(ctx, "Apercu")
									   - ctx.layout.cursor.y - ctx.layout.itemSpacingY - 1.f;
				NkRect area = ctx.NextItemRect(-1.f, hToile > 120.f ? hToile : 120.f);
				if (area.w <= 0.f || area.h <= 0.f)
					return;
				// Le mode demande en ligne de commande se consomme AVANT le
				// partage Split (il decide de la geometrie de cette image).
				if (mSt->modeInitial >= 0 && mSt->modeInitial <= 3) {
					mMode = (uint32)mSt->modeInitial;
					mSt->modeInitial = -1;
				}
				// ── LE MODE SPLIT (§10, tranche minimale) : Design a GAUCHE,
				//    Behavior a DROITE — meme document, meme selection (« un seul
				//    modele de verite », doc 1 §3) ; separateur deplacable borne
				//    25–75 %. La toile Design devient simplement la moitie
				//    gauche : tout ce qui suit (viewport, clip, flottants,
				//    selection) la suit sans changer. La moitie droite et le
				//    separateur se dessinent en fin d'OnUI. ECARTS NOMMES : la
				//    paire est fixee Design|Behavior (le menu des trois paires
				//    du §10 viendra avec le modele Animation) ; les sous-barres
				//    d'outils reduites restent celles de la moitie gauche.
				const NkRect areaTotale = area;
				const bool splitActif = (mMode == 3);
				if (splitActif)
					area.w = areaTotale.w * mSplitRatio - 3.f;

				// ── LA VUE NE NAIT PAS SOUS LA BARRE D'OUTILS ────────────────
				// ⚠️ MESURE, ET ELLE DEPLACE LE DEFAUT. La barre verticale
				//    recouvrait les lignes Sol / Rocher / Caisse de l'arbre.
				//    Releve : barre a x = 266..314, nœud « Arbre » a x = 270..530.
				//    La barre est POURTANT a sa place -- 12 px du bord gauche de la
				//    toile (zone.x = 254), et §7 la veut la, « elle flotte au-dessus
				//    du canvas, le canvas passe dessous ». Ce n'est donc pas la
				//    barre qui est mal posee : c'est **la vue qui naissait collee a
				//    l'origine** (canvas.vue = zoom 1, pan 0, 0), donc le document
				//    naissait sous la barre.
				//    La correction va a la VUE, une seule fois, et elle se calcule
				//    depuis la geometrie de la barre -- pas depuis un nombre choisi
				//    a l'oeil qui se decalerait au premier changement de largeur.
				if (!mVuePosee) {
					mVuePosee = true;
					if (mSt->vuePosee) {
						// La vue demandee en ligne de commande (protocole de
						// mesure) prime sur le defaut « pas sous la barre ».
						mSt->view.panX = mSt->vueX;
						mSt->view.panY = mSt->vueY;
						mSt->view.zoom = mSt->vueZ > 0.01f ? mSt->vueZ : 1.f;
					} else {
						mSt->view.panX = kOutilsMarge * 2.f + kOutilsLargeur;
						mSt->view.panY = 12.f;
					}
				}

				// ⚠️ DEUX SURFACES, ET ELLES N ONT PAS LE MEME ESPACE.
				//    `area` est le rectangle ECRAN du panneau ; il devient le
				//    `viewport` de la vue. La disposition, elle, se calcule dans un
				//    rectangle DOCUMENT ancre a l'origine : c'est ce qui rend les
				//    coordonnees du modele independantes du zoom et de la taille de
				//    la fenetre.
				mSt->view.viewport = {area.x, area.y, area.w, area.h};
				const NkPaintRect docSurface = {0.f, 0.f, mSt->view.ToDocLength(area.w),
												mSt->view.ToDocLength(area.h)};
				mSt->Recompute(docSurface);

				// ── LA PORTE D'ACTIVITE (mandat fluidite, 01/09) ─────────────
				// ⚠️ MESURE AVANT DE CORRIGER : l'observateur d'historique
				//    SERIALISAIT LE DOCUMENT A CHAQUE IMAGE (13 Ko x 60/s), et
				//    la pastille ajoutait sa serialisation 2x/s — meme au repos
				//    complet. Le document ne peut changer que par une ENTREE
				//    (souris, clavier, molette — les leviers injectes ecrivent
				//    ctx.input) ou par une restauration (editionGeneration).
				//    L'activite arme une traine de ~45 images : la stabilite
				//    ~6 images de l'observateur tient largement dedans, puis le
				//    repos ne serialise PLUS RIEN.
				{
					bool touche = false;
					for (int32 kk = 0; kk < (int32)nkgui::NkGuiInput::KeyCount && !touche; ++kk)
						touche = ctx.input.keyInit[kk];
					const bool activite = ctx.input.mouseDown[0] || ctx.input.mouseDown[1]
										  || ctx.input.mouseDown[2] || ctx.input.mouseClicked[0]
										  || ctx.input.mouseReleased[0] || ctx.input.wheel != 0.f
										  || ctx.input.charCount > 0 || touche
										  || mSt->editionGeneration != mFluGenVue;
					mFluGenVue = mSt->editionGeneration;
					if (activite)
						mHorlogeActivite = 45;
				}

				// La pastille « modifie » du titre : une MESURE toutes les 30
				// images, SEULEMENT dans la traine d'activite (au repos l'etat
				// ne peut pas avoir change), jamais un drapeau.
				if (mSt->titre && mHorlogeActivite > 0 && (mFramePastille++ % 30u) == 0u)
					mSt->titre(mSt->titreUser, mSt->DocumentModifie());

				// ── L'OBSERVATEUR D'HISTORIQUE (annulation unifiee, §7) ──────
				// La serialisation courante, observee pendant la traine
				// d'activite ; l'instantane n'est pousse que STABLE (drag lache,
				// frappe en pause) — un geste = UN pas (Historique.h).
				if (mHorlogeActivite > 0) {
					--mHorlogeActivite;
					NkString ser;
					mSt->doc.Save(ser);
					mSt->histoire.Observer(ser);
				}
				nkgui::NkGuiNoterMesure(ctx, "canvas.histoire",
										(float32)(mSt->histoire.PeutAnnuler() ? 1 : 0),
										(float32)(mSt->histoire.PeutRetablir() ? 1 : 0),
										(float32)mSt->doc.NodeCount(), 0.f);
				// Leviers --annuler=N / --retablir=N : espaces de 10 images pour
				// que chaque restauration se voie au releve, apres que les
				// gestes injectes (--clic, jusqu'a ~la trame 60) ont ete
				// POUSSES par l'observateur (stabilite ~6 images).
				++mFrameHistorique;
				if (mSt->annulerInitial > 0 && mFrameHistorique >= 130
					&& (mFrameHistorique % 10u) == 0u) {
					mSt->Annuler();
					--mSt->annulerInitial;
				} else if (mSt->annulerInitial == 0 && mSt->retablirInitial > 0
						   && mFrameHistorique >= 170 && (mFrameHistorique % 10u) == 0u) {
					mSt->Retablir();
					--mSt->retablirInitial;
				}

				if (mSt->selectionInitiale >= 0
					&& mSt->doc.IsValidIndex(mSt->selectionInitiale)) {
					// ⚠️ CONSOMMÉE SEULEMENT QUAND ELLE PEUT S'APPLIQUER. Le
					//    démarrage n'est pas déterministe (LoadDoc : le fichier se
					//    lit parfois VIDE au premier passage — poignée encore tenue
					//    par le processus précédent) : consommer le levier sur un
					//    document pas encore là rendait des captures SANS sélection,
					//    une fois sur quelques-unes. Mesuré le 31/08 (écran 8).
					mSt->SelectSingle(mSt->selectionInitiale);
					mSt->selectionInitiale = -1;
				}
				if (mSt->editTexteInitial >= 0
					&& mSt->doc.IsValidIndex(mSt->editTexteInitial)) {
					// Mise en scene --editer-texte=N : le meme etat que le
					// double-clic pose (champ superpose + selection). Consomme
					// seulement quand le document est la (regle --selection=).
					const int32 ne = mSt->editTexteInitial;
					if (StrEq(mSt->doc.nodes[(uint32)ne].shape.Data(), "text")) {
						mEditNode = ne;
						const char *t0 =
							mSt->doc.nodes[(uint32)ne].TexteEn(mSt->langueActive.Data());
						snprintf(mEditBuf, sizeof(mEditBuf), "%s", t0 ? t0 : "");
						mSt->SelectSingle(ne);
						Dire("Édition du texte — Entrée valide, Échap annule.", "", "");
					} else
						Dire("--editer-texte : ce nœud n'est pas un texte.", "", "");
					mSt->editTexteInitial = -1;
				}
				if (mSt->modeFormeInitial >= 0
					&& mSt->doc.IsValidIndex(mSt->modeFormeInitial)) {
					// Mise en scene --mode-forme=N : l'etat exact que le
					// double-clic pose sur une forme a sommets. Consomme
					// seulement quand le document est la (regle --selection=).
					const int32 nf = mSt->modeFormeInitial;
					if (NkFormeEditable(mSt->doc.nodes[(uint32)nf])) {
						mSt->modeForme.Quitter();
						mSt->modeForme.noeud = nf;
						// ⚠️ ET UN SOMMET EST DESIGNE, sinon la capture montrerait la
						//    section « ÉDITION DE FORME » avec ses trois champs a
						//    « — » : on photographierait le panneau vide, c'est-a-dire
						//    precisement ce qu'on veut prouver qui ne l'est pas.
						if (mSt->sommetsInitiaux != 0) {
							mSt->modeForme.marques = mSt->sommetsInitiaux;
							for (uint32 b = 0; b < 64; ++b)
								if ((mSt->sommetsInitiaux & (1ull << b)) != 0ull) {
									mSt->modeForme.sommet = (int32)b;
									break;
								}
						} else
							mSt->modeForme.MarquerSeul(0);
						// Mise en scene : --courber pose une tangente MIROIR sur
						// chaque sommet marque. Meme famille que --sommets= : une
						// courbe se tire a la souris, et le harnais ne sait pas
						// viser une poignee dont on ignore la coordonnee d ecran.
						if (mSt->courberInitial) {
							NkUINode &cn2 = mSt->doc.nodes[(uint32)nf];
							NkMaterialiserSommets(cn2);
							for (uint32 k = 0; k < (uint32)cn2.sommets.Size(); ++k) {
								if (!mSt->modeForme.Marque((int32)k))
									continue;
								NkPoserLiaison(cn2.sommets[k], NkPoint2::LiaisonMiroir);
								NkPoserTangente(cn2.sommets[k], 1, 0.55f, 0.35f);
							}
							mSt->courberInitial = false;
						}
						mSt->SelectSingle(nf);
						Dire(NkRaisonDeDblClic(NkSuiteDblClic::ModePoints), "", "");
					} else
						Dire("--mode-forme : ce nœud n'a pas de sommets à éditer.", "", "");
					mSt->modeFormeInitial = -1;
				}

				if (!mAideInitiale) {
					// L'outil arme se DIT des la premiere image — une application
					// qui ne dit pas quel outil est actif fait deviner (mesure du
					// 30/08).
					// ⚠️ MAIS ELLE NE COUVRE PAS UN MODE DEJA OUVERT. Mesure du
					//    01/09, sur la capture `preuve_edit_shape_n9` : le levier
					//    `--mode-forme=` posait sa phrase, cette ligne la
					//    remplacait aussitot par « Selection — cliquer... », et le
					//    pied de fenetre annoncait un outil pendant que l'ecran
					//    montrait un mode. C'est le meme defaut que le 24f48773 a
					//    corrige dans l'autre sens : *une phrase qui contredit
					//    l'etat est un mensonge d'interface*, qu'elle survive a
					//    son etat ou qu'elle le devance.
					mAideInitiale = true;
					if (!mSt->modeForme.Actif())
						Dire("", AideOutil(mOutil), "");
				}

				NkComponentInput in;
				in.surfaceScale = 1.f; ///< ⚠️ A BRANCHER sur le DPI reel de la surface
				in.mouseX = ctx.input.mousePos.x;
				in.mouseY = ctx.input.mousePos.y;
				in.wheel = ctx.input.wheel;
				in.mouseDown = ctx.input.mouseDown[0];
				in.mousePressed = ctx.input.mouseClicked[0];
				in.mouseReleased = ctx.input.mouseReleased[0];
				in.doubleClick = ctx.input.mouseDoubleClicked[0];
				in.rightPressed = ctx.input.mouseClicked[1];
				in.ctrl = ctx.input.ctrlDown;
				in.shift = ctx.input.shiftDown;
				in.alt = ctx.input.altDown;

				// ── LA TOILE : molette = zoom, bouton du milieu = deplacement ──
				// Le curseur doit etre DANS le panneau, sinon la molette de
				// n'importe quel autre panneau zoomerait la toile.
				const bool dedans = in.mouseX >= area.x && in.mouseX < area.x + area.w
									&& in.mouseY >= area.y && in.mouseY < area.y + area.h;
				if (dedans && ctx.input.wheel != 0.f) {
					// ⚠️ ZOOM AUTOUR DU CURSEUR, jamais autour du coin : sinon le
					//    contenu fuit sous la souris. `ZoomAt` est le seul endroit
					//    qui melange les deux espaces, et il est mesure (39f/39g).
					mSt->view.ZoomAt(ctx.input.wheel > 0.f ? 1.1f : (1.f / 1.1f), in.mouseX,
									 in.mouseY);
				}
				if (ctx.input.mouseDown[2]) {
					if (mPanning) {
						mSt->view.PanBy(in.mouseX - mPanX, in.mouseY - mPanY);
					}
					mPanning = true;
					mPanX = in.mouseX;
					mPanY = in.mouseY;
				} else {
					mPanning = false;
				}

				// La disposition traduite en pixels : tout ce qui suit -- dessin,
				// saisie, rectangles publies -- travaille en ESPACE ECRAN.
				NkLayoutResult screen;
				mSt->ProjectToScreen(screen);

				// ⚠️ LA VUE SE PUBLIE, PARCE QUE PERSONNE NE POUVAIT LA MESURER.
				//    Le zoom et le deplacement vivent dans `OnUI` -- un endroit
				//    qu AUCUN banc ne peut atteindre : ils sont donc restes trois
				//    etapes sans preuve, et j ai annonce qu ils marchaient sans
				//    l avoir vu. En les publiant dans le registre, `--dump-ui`
				//    devient la mesure : si `canvas.vue` ne bouge pas quand on
				//    tourne la molette, la molette n arrive pas.
				// ⚠️ `NoterMesure`, PAS un rectangle. Ces quatre nombres — zoom,
				//    deplacement X, deplacement Y, taille de selection — n'ont
				//    jamais ete une geometrie ; l'ancien registre les faisait
				//    passer par un `NkRect` faute d'autre moyen. Le releve de
				//    NKGui, lui, JUGE les rectangles : un zoom de 1 et un
				//    deplacement nul seraient sortis « vide,hors-vue ». Une
				//    fausse alerte dans un instrument coute plus cher que pas
				//    d'alerte : elle apprend a ne plus le lire.
				nkgui::NkGuiNoterMesure(ctx, "canvas.vue", mSt->view.zoom, mSt->view.panX,
										mSt->view.panY, (float32)mSt->sel.Count());
				// LA SÉLECTION ET LE FORAGE SE PUBLIENT (preuve du forage sous
				// curseur : deux doubles-clics injectés à deux positions doivent
				// montrer DEUX sélections différentes au relevé).
				nkgui::NkGuiNoterMesure(
					ctx, "canvas.selection", (float32)mSt->selected, (float32)mForage,
					(float32)(mSt->doc.IsValidIndex(mEditNode) ? mEditNode : -1), 0.f);
				// ⚠️ ET ILS SE PUBLIENT AUSSI DANS L'ÉTAT PARTAGÉ, une écriture par
				//    image : les commandes de la coquille (Ctrl+D/G) et le menu
				//    Édition tournent hors de tout panneau et n'ont aucun autre
				//    moyen de savoir qu'une saisie est ouverte. UN SEUL sens
				//    d'écriture — la toile décide, l'état partagé rapporte.
				// LE SURVOL ET LE COMPTE DE SELECTION SE PUBLIENT : sans eux, la
				// pre-selection et la multi-selection ne se jugeraient qu'a l'oeil.
				// LE MODE POINTS SE PUBLIE : noeud edite, sommet tire, nombre de
				// sommets — sans quoi il ne se jugerait qu'a l'oeil.
				{
					float32 tmpXY[64];
					uint32 nbP = 0;
					if (mSt->modeForme.noeud >= 0 && mSt->doc.IsValidIndex(mSt->modeForme.noeud)
						&& screen.Has(mSt->modeForme.noeud))
						nbP = NkSommetsDe(mSt->doc.nodes[(uint32)mSt->modeForme.noeud],
										  screen.At(mSt->modeForme.noeud), tmpXY, 32);
					nkgui::NkGuiNoterMesure(ctx, "canvas.points", (float32)mSt->modeForme.noeud,
											(float32)mSt->modeForme.tire, (float32)nbP, 0.f);
				}
				nkgui::NkGuiNoterMesure(ctx, "canvas.survol", (float32)mSurvol,
										(float32)NkCompteSelection(mSt->doc, mSt->sel), 0.f,
										0.f);
				mSt->editionToile = mSt->doc.IsValidIndex(mEditNode) ? mEditNode : -1;
				mSt->forageToile = mForage;
				// L'AIMANT SE PUBLIE : sans ca il serait invisible a la mesure —
				// un guide qui se peint et disparait en une image ne se prouve
				// pas a la capture. Quatre nombres : aimant on/off, guide
				// vertical (coord ou -99999 si aucun), guide horizontal, ecart.
				nkgui::NkGuiNoterMesure(
					ctx, "canvas.snap", mSt->aimantActif ? 1.f : 0.f,
					mSt->snapVif.guideV.actif ? mSt->snapVif.guideV.coord : -99999.f,
					mSt->snapVif.guideH.actif ? mSt->snapVif.guideH.coord : -99999.f,
					mSt->snapVif.guideV.actif ? mSt->snapVif.guideV.ecart
											  : mSt->snapVif.guideH.ecart);
				// ⚠️ LE NOMBRE DE DISTANCES ECRITES SE PUBLIE, ET PAS SEULEMENT SE
				//    PEINT. Un badge se juge a la capture ; son EXISTENCE se
				//    mesure sans fenetre. C'est la lecon de Q42 sur les titres de
				//    section (« rendre l'Inspecteur mesurable au releve est un
				//    travail identifie »), appliquee CETTE FOIS D'ENTREE au lieu
				//    d'apres coup.
				nkgui::NkGuiNoterMesure(
					ctx, "canvas.snap.mesures", (float32)mSt->snapVif.nbMesures,
					mSt->snapVif.nbMesures > 0 ? mSt->snapVif.mesures[0].valeur : -1.f,
					mSt->snapVif.nbMesures > 1 ? mSt->snapVif.mesures[1].valeur : -1.f, 0.f);

				// ── RACCOURCIS D'OUTILS AU VRAI CLAVIER (Lunacy : V F R O L T) ──
				// ⚠️ MESURE DU 30/08 (Rodolf : « les F glisser et autres ne
				//    fonctionnent donc pas ») : la touche n'avait JAMAIS été
				//    branchée — l'infobulle la promettait, le clavier n'arrivait
				//    nulle part. Règle Lunacy : les lettres n'arment un outil que
				//    si AUCUN champ texte n'a le clavier (sinon elles sont une
				//    saisie) ; l'effet est VISIBLE (l'icône s'allume, la ligne
				//    d'aide change) — jamais de no-op muet. On lit `chars[]`
				//    (traduits par l'OS : un AZERTY donne les mêmes lettres),
				//    pas des codes de touche.
				// ⚠️ PAS pendant une edition de texte en place : les lettres sont
				//    alors une SAISIE (meme regle que les champs NKGui).
				if (ctx.inputId == NKGUI_ID_NONE && !mSt->doc.IsValidIndex(mEditNode)
					&& !mSt->renommageArbre) {
					for (int32 k = 0; k < ctx.input.charCount; ++k) {
						switch (ctx.input.chars[k]) {
							// familles Lunacy (3e retour) : V/H Déplacer, F Cadre,
							// R/O Formes, L Ligne, T Texte, M Image ; P/X/N DISENT
							// leur chantier (jamais un no-op muet).
							case 'v': case 'V': mVarMove = 0; ArmerOutil(0); break;
							case 'h': case 'H': mVarMove = 1; ArmerOutil(0); break;
							case 'f': case 'F': ArmerOutil(1); break;
							case 'r': case 'R': mVariante = 0; ArmerOutil(2); break;
							case 'o': case 'O': mVariante = 2; ArmerOutil(2); break;
							case 'l': case 'L': mVarLigne = 0; ArmerOutil(3); break;
							case 't': case 'T': ArmerOutil(5); break;
							case 'm': case 'M': ArmerOutil(7); break;
							case 'p': case 'P': DireRaisonFamille(6); break;
							case 'x': case 'X': DireRaisonFamille(4); break;
							case 'n': case 'N': DireRaisonFamille(8); break;
							default: break;
						}
					}
					// ── LES RACCOURCIS D'EDITION (Lunacy, 01/09) — même garde que
					//    les lettres : jamais pendant une saisie. Ctrl+C/X/V/A
					//    arrivent par les drapeaux want* de la coquille (les mêmes
					//    que les champs texte) ; Suppr par la touche. Ctrl+D/G
					//    passent par les commandes enregistrées (main.cpp).
					if (ctx.input.wantCopy) {
						mSt->CopierSelection();
						Dire(mSt->status.Data(), "", "");
					} else if (ctx.input.wantCut) {
						if (mSt->CouperSelection())
							Dire(mSt->status.Data(), "", "");
					} else if (ctx.input.wantPaste) {
						if (mSt->CollerPressePapiers())
							Dire(mSt->status.Data(), "", "");
						else
							Dire("Le presse-papiers est vide.", "", "");
					} else if (ctx.input.wantSelectAll) {
						// Ctrl+A : le geste vit dans l'état partagé (le menu
						// Édition l'appelle aussi) — la toile ne fait que le
						// déclencher et le DIRE.
						mSt->ToutSelectionner();
						Dire(mSt->status.Data(), "", "");
					} else if (ctx.input.KeyPressed(NkGuiKey::Delete)) {
						if (mSt->SupprimerSelection())
							Dire(mSt->status.Data(), "", "");
					} else if (ctx.input.KeyPressed(NkGuiKey::Enter)) {
						// ENTRÉE DESCEND D'UN NIVEAU (Lunacy) — le geste symétrique
						// d'ÉCHAP, qui remonte (juste en dessous). Entrer dans un
						// groupe sélectionne son PREMIER enfant et pose le forage :
						// exactement ce que fait le double-clic, sans la souris.
						// ⚠️ ET IL DIT QUAND IL NE PEUT PAS. Un nœud sans enfant
						//    n'est pas un groupe ; un no-op muet ferait croire que
						//    la touche n'arrive pas — le défaut mesuré le 30/08 sur
						//    les lettres d'outils, où l'infobulle promettait une
						//    touche que le clavier n'atteignait jamais.
						const int32 g = mSt->selected;
						if (mSt->doc.IsValidIndex(g)
							&& !mSt->doc.nodes[(uint32)g].children.Empty()) {
							mForage = g;
							mSt->SelectSingle(mSt->doc.nodes[(uint32)g].children[0]);
							Dire("Entré dans le groupe — Échap ressort.", "", "");
						} else if (mSt->doc.IsValidIndex(g)) {
							Dire("Entrer : cet élément n'a pas d'enfants.", "", "");
						}
					}
				}
				// ── LE DÉPLACEMENT AU CLAVIER (vague 1 du document de référence) ─
				// ⚠️ IL EST TESTÉ AVANT LES AUTRES TOUCHES, et la garde compte
				//    autant que le geste : on ne déplace RIEN tant qu'une saisie
				//    est ouverte. Sans elle, taper une flèche pour corriger une
				//    lettre dans un libellé déplacerait l'objet derrière le champ
				//    — un geste, deux effets, dont un invisible. C'est le défaut
				//    exact que le mode points a payé le 01/09.
				// ⚠️ ET PAS EN MODE ÉDITION DE FORME : là, les flèches devront
				//    déplacer les SOMMETS marqués, pas le nœud. Tant que ce n'est
				//    pas écrit, on ne fait rien plutôt que la mauvaise chose.
				if (!mSt->doc.IsValidIndex(mEditNode) && !mEditEtiquette
					&& !mSt->modeForme.Actif() && ctx.popupDepth == 0
					&& mSt->sel.Count() > 0) {
					const bool maj = ctx.input.shiftDown;
					float32 dx = 0.f, dy = 0.f;
					if (ctx.input.KeyPressedRepeat(NkGuiKey::Left))
						dx = -NkPasClavier(maj);
					else if (ctx.input.KeyPressedRepeat(NkGuiKey::Right))
						dx = NkPasClavier(maj);
					else if (ctx.input.KeyPressedRepeat(NkGuiKey::Up))
						dy = -NkPasClavier(maj);
					else if (ctx.input.KeyPressedRepeat(NkGuiKey::Down))
						dy = NkPasClavier(maj);
					if (dx != 0.f || dy != 0.f) {
						// ⚠️ TOUTE LA SÉLECTION BOUGE, pas seulement le principal :
						//    sinon le geste au clavier ferait autre chose que le
						//    même geste à la souris, et c'est la divergence que ce
						//    chantier ferme depuis un mois.
						uint32 bouges = 0;
						for (uint32 k = 0; k < (uint32)mSt->sel.Count(); ++k) {
							const int32 i = mSt->sel.items[k];
							if (!mSt->doc.IsValidIndex(i) || i == 0)
								continue;
							// seul un enfant placé LIBREMENT se déplace : sous un
							// agencement calculé, posX/posY sont ignorés (même
							// règle que le recadrage sur le tracé).
							if (!ParentLibre(i))
								continue;
							NkUINode &n = mSt->doc.nodes[(uint32)i];
							n.posX += dx;
							n.posY += dy;
							mSt->doc.MarkHumanEdit(i);
							++bouges;
						}
						char msg[160];
						if (bouges > 0)
							snprintf(msg, sizeof(msg), "Déplacé de %g px — Maj+flèche pour 10 px.",
									 (double)(dx != 0.f ? (dx < 0 ? -dx : dx)
													   : (dy < 0 ? -dy : dy)));
						else
							snprintf(msg, sizeof(msg),
									 "Rien à déplacer : le parent place ses enfants lui-même.");
						Dire(msg, "", "");
					}
				}
				// ÉCHAP annule le tracé en cours (Lunacy), et le DIT.
				if (mCreating && ctx.input.KeyPressed(NkGuiKey::Escape)) {
					mCreating = false;
					Dire("Tracé annulé.", "", "");
				} else if (mSt->modeForme.noeud >= 0 && !mSt->doc.IsValidIndex(mEditNode)
						   && ctx.input.KeyPressed(NkGuiKey::Escape)) {
					// ⚠️ ÉCHAP SORT D'ABORD DU MODE POINTS, ENSUITE du forage. Les
					//    deux écoutent la même touche ; sans cet ordre, Échap
					//    remonterait d'un niveau en laissant les poignées de
					//    sommets affichées sur un nœud qu'on vient de quitter.
					mSt->modeForme.Quitter();
					Dire("Mode points quitté.", "", "");
				} else if (mForage >= 0 && !mSt->doc.IsValidIndex(mEditNode)
						   && ctx.input.KeyPressed(NkGuiKey::Escape)) {
					// ÉCHAP REMONTE d'un niveau de forage (Figma/Lunacy) : on
					// sélectionne le groupe qu'on quitte, le contexte remonte —
					// jusqu'au premier niveau. (L'Échap d'une édition en place,
					// lui, ANNULE l'édition — il est consommé par le champ.)
					if (mSt->doc.IsValidIndex(mForage)) {
						mSt->SelectSingle(mForage);
						const int32 pa = mSt->doc.nodes[(uint32)mForage].parent;
						const bool paPremier =
							pa < 0 || !mSt->doc.IsValidIndex(pa)
							|| mSt->doc.nodes[(uint32)pa].parent < 0
							|| StrEq(mSt->doc.nodes[(uint32)pa].shape.Data(), "frame");
						mForage = paPremier ? -1 : pa;
					} else
						mForage = -1;
				}
				TraceEntree(ctx);

				// ── LES MODES GRAPHE (écrans 13 Behavior / 18 Animation) : la toile
				//    devient une vue blueprint. Le CHROME se dessine (fond, grille
				//    20/100, bande de portée sur la sélection RÉELLE, Entry canonique
				//    en Animation) ; le CONTENU du graphe — nœuds, câbles, console —
				//    naîtra du modèle de comportement §4.6, il ne se peint pas en
				//    démo (cadrage du 31/08). Split = Design|Behavior composés (§10).
				//    (le mode initial en ligne de commande est consommé plus haut,
				//    avant le partage Split)
				const bool modeGraphe = (mMode == 1 || mMode == 2);
				// ⚠️ MENU CONTEXTUEL OUVERT = la souris lui appartient. Le menu se
				//    dessine APRES la toile (overlay) : son occlusion « modal
				//    leger » ne protege que ce qui vient apres lui — HandleMouse
				//    tourne AVANT et verrait le clic. Un clic sur « Supprimer »
				//    aurait aussi deselectionne derriere (la classe des flottants,
				//    mesuree le 30/08).
				if (!modeGraphe && !mMenuCtx.open)
					HandleMouse(in, screen);

				// ⚠️ `NkDesignPaint`, PAS `NkGuiComponentPaint` : c'est lui qui
				//    traduit les poignees de CETTE application en dessins. Le
				//    peintre du kit peint un carre pour toute poignee non nulle —
				//    correct pour lui (il ne connait l'enumeration de personne),
				//    insuffisant pour un chevron, qui doit dire « ouvert » ou
				//    « ferme ». Il surcharge `Icon` et RIEN d'autre.
				NkDesignPaint paint(ctx, mSt->theme);

				// ── LA DÉCOUPE AUX BORNES DE LA TOILE (mesuré le 31/08, test de
				//    Rodolf : « tu ne définis pas bien le clipping ») ─────────────
				// ⚠️ LE CLIP EN VIGUEUR ICI ÉTAIT CELUI DU DOCK, PAS CELUI DE LA
				//    TOILE. Le contenu du panneau est découpé par `BeginScrollFrame`
				//    aux bornes de la feuille de dock — 11 px PLUS LARGE que `area`
				//    (le viewport de la vue) : un artboard poussé au-delà du bord
				//    gauche se peignait sur la gouttière du panneau, jusqu'au
				//    splitter. Mesure sur capture : toile x=249, artboard peint dès
				//    x=238. Tout ce que la toile dessine (fond, document, trace,
				//    zone sûre, lignes, flottants, sélection) passe sous CE clip,
				//    ouvert ici et refermé en fin d'OnUI — aucun return entre les
				//    deux (vérifié), et les clips internes (zone sûre, composants)
				//    s'y intersectent.
				ctx.DL().PushClipRect({area.x, area.y, area.w, area.h}, true);

				// ── LA TOILE DE LA PLANCHE 22.0 : fond + grille POINTILLEE ────
				// Decor d'EDITEUR, pas de document : `RenderDocument` (les essais
				// 41) n'emet aucune de ces commandes, et le fichier enregistre n'en
				// sait rien. Le pas est en espace DOCUMENT : la grille zoome avec
				// le contenu, comme sur la planche. En dessous de 6 px projetes,
				// elle se tait — des points serres deviennent du bruit.
				if (modeGraphe) {
					DessinerBlueprint(ctx, area, mMode);
				} else {
					// La toile SUIT LE THEME (test de Rodolf, 31/08 : « cette
					// couleur blanche c'est pour le theme light ; en Design il
					// faut la meme couleur de fond que pour Behavior et les
					// autres ») : `canvas_bg` vaut #0d1117 en sombre (le fond de
					// la vue Behavior) et #f5f7fb en clair (la valeur Banani V2),
					// pose par les fabriques de themes du kit. Les points
					// `canvas_dot` suivent pareil.
					paint.Fill({area.x, area.y, area.w, area.h},
							   NkDesignResolveRole("canvas_bg"), 0.f);
					const float32 pasEcran = kGrillePas * mSt->view.zoom;
					// ⚠️ `grilleVisible` EST HONORÉE ICI, au seul endroit qui peint
					//    la grille. Le menu qui la bascule serait sinon une case
					//    cochée à côté d'un dessin qui ne l'écoute pas — exactement
					//    le défaut que Q42 a trouvé sur l'aimant du menu Affichage.
					if (mSt->grilleVisible && pasEcran >= 6.f) {
						const uint16 rPoint = NkDesignResolveRole("canvas_dot");
						const float32 d0x = mSt->view.ToDocX(area.x);
						const float32 d0y = mSt->view.ToDocY(area.y);
						const int32 kx0 = (int32)(d0x / kGrillePas) - 1;
						const int32 ky0 = (int32)(d0y / kGrillePas) - 1;
						for (int32 gy = ky0;; ++gy) {
							const float32 sy = mSt->view.ToScreenY((float32)gy * kGrillePas);
							if (sy > area.y + area.h)
								break;
							if (sy < area.y)
								continue;
							for (int32 gx = kx0;; ++gx) {
								const float32 sx =
									mSt->view.ToScreenX((float32)gx * kGrillePas);
								if (sx > area.x + area.w)
									break;
								if (sx < area.x)
									continue;
								paint.Fill({sx, sy, 2.f, 2.f}, rPoint, 0.f);
							}
						}
					}
				}

				if (!modeGraphe) {
					// L'echelle du document = le zoom de la vue : le texte des
					// formes suit (corps pose x zoom), comme les rectangles qui
					// arrivent deja projetes (correction du 31/08).
					mSt->host.docScale = mSt->view.zoom;
					mSt->host.langueDoc = mSt->langueActive; // bascule a chaud
					// Le noeud en edition en place ne dessine pas son texte —
					// le champ transparent le dessine a sa place.
					mSt->host.editionNode =
						mSt->doc.IsValidIndex(mEditNode) ? mEditNode : -1;
					mSt->host.editionEtiquette = mEditEtiquette;
					NkDrawDocument(paint, in, mSt->doc, screen, mSt->host);
				}

				// ── LE TRACE ELASTIQUE d'un outil F/R en cours ────────────────
				// Peint APRES le document (il flotte au-dessus), jamais enregistre.
				if (mCreating && !modeGraphe) {
					// ⚠️ ALT — PAS CTRL, ET C'EST LA DOCUMENTATION QUI LE DIT. Rodolf
					//    écrivait « le Ctrl ou Shift » : la source Lunacy
					//    (`shortcuts/`, `tips/`) tranche pour **Maj = proportions**
					//    et **Alt = depuis le centre**. La règle de la maison sur les
					//    keymaps est de lire, pas de deviner — et accepter Ctrl « au
					//    cas où » aurait fabriqué un raccourci que Lunacy n'a pas,
					//    donc une divergence à découvrir le jour où Ctrl servira à
					//    autre chose.
					const NkPaintRect t = RectTrace(in.mouseX, in.mouseY, in.shift, in.alt);
					paint.OutlineSharp(t, NkDesignResolveRole("accent_ui"));
					// La même puce que la sélection : le tracé se lit pendant
					// qu'on dessine (Lunacy).
					PuceTaille(paint, t, mSt->view.ToDocLength(t.w),
							   mSt->view.ToDocLength(t.h));
				}

				// ⚠️ LES TROIS FLOTTANTS SONT DESSINÉS **APRÈS** LE DOCUMENT, et
				//    c'est tout leur sens : le plan les note en orange avec la
				//    mention « zones en orange = flottantes au-dessus du canvas,
				//    elles ne le redimensionnent pas ». Les poser avant, ou leur
				//    réserver de la place dans le flux, en referait des bandes —
				//    et §7 est explicite : « il n'existe pas de troisième bande
				//    d'outils, les outils flottent au-dessus du canvas, ils ne
				//    bordent pas la fenêtre ».
				// ── LA ZONE SÛRE (écrans 11/12) : décor d'ÉDITEUR sur les cadres
				//    à cible Mobile — hachures des bandes non sûres, encoche et
				//    indicateur home, pointillés + « zone sûre ». Proportions du
				//    JSX (52/476 haut, 34/476 bas, encoche 80/220 × 24/476).
				//    Jamais dans le document ni dans les essais 41.
				if (mSt->zoneSure && !modeGraphe) {
					NkDesignPaint pz(ctx, mSt->theme);
					auto &dlz = ctx.DL();
					for (uint32 i = 0; i < (uint32)mSt->doc.nodes.Size(); ++i) {
						const NkUINode &nz = mSt->doc.nodes[i];
						if (!StrEq(nz.shape.Data(), "frame") || nz.target.Empty()
							|| nz.target.Data()[0] != 'M' || !screen.Has((int32)i))
							continue;
						const NkPaintRect r = screen.At((int32)i);
						const float32 hTop = r.h * (52.f / 476.f);
						const float32 hBas2 = r.h * (34.f / 476.f);
						const NkColor hachure = {90, 120, 210, 71}; // rgba .28
						// hachures 45°, pas 6 : segments obliques bornés à la bande
						auto hachurer = [&](float32 by, float32 bh) {
							dlz.PushClipRect({r.x, by, r.w, bh}, true);
							for (float32 t = -bh; t < r.w; t += 6.f)
								dlz.AddLine({r.x + t, by + bh}, {r.x + t + bh, by}, hachure,
											1.5f);
							dlz.PopClipRect();
						};
						hachurer(r.y, hTop);
						hachurer(r.y + r.h - hBas2, hBas2);
						// l'encoche (noire, arrondie) et l'indicateur home
						const float32 nw = r.w * (80.f / 220.f);
						const float32 nh = r.h * (24.f / 476.f);
						dlz.AddRectFilled({r.x + (r.w - nw) * 0.5f, r.y, nw, nh},
										  {0, 0, 0, 255}, nh * 0.5f);
						dlz.AddRectFilled({r.x + (r.w - 60.f) * 0.5f, r.y + r.h - 10.f, 60.f,
										   4.f},
										  {255, 255, 255, 178}, 2.f);
						// les pointillés de la zone sûre (#4f8ef7, tirets 3/3)
						const NkColor pointille = {79, 142, 247, 178};
						auto tirets = [&](float32 yy) {
							for (float32 t = 0.f; t < r.w; t += 6.f) {
								const float32 fin = (t + 3.f < r.w) ? t + 3.f : r.w;
								dlz.AddLine({r.x + t, yy}, {r.x + fin, yy}, pointille, 0.8f);
							}
						};
						tirets(r.y + hTop);
						tirets(r.y + r.h - hBas2);
						costume::Texte(dlz, costume::Fontes().px9, r.x + 6.f, r.y + hTop - 13.f,
									   "zone sûre", pointille);
					}
					(void)pz;
				}
				// Les LIGNES DE MAGNÉTISME FIGÉES (levier --lignes=, mise en
				// scène) : rose `snap_line` #ff4fd8, 1 px — SOUS les flottants,
				// comme dans la maquette (zIndex des lignes < bascule).
				{
					const uint16 rose = NkDesignResolveRole("snap_line");
					NkDesignPaint p2(ctx, mSt->theme);
					if (mSt->ligneV >= 0.f && mSt->ligneV <= 1.f)
						p2.Fill({area.x + area.w * mSt->ligneV, area.y, 1.f, area.h}, rose, 0.f);
					if (mSt->ligneH >= 0.f)
						p2.Fill({area.x, area.y + mSt->ligneH, area.w, 1.f}, rose, 0.f);
				}
				// ── LES GUIDES VIVANTS DE L'AIMANT (Lunacy) ──────────────────
				// ⚠️ LE CALCUL N'EST PAS ICI : `snapVif` a ete rempli par le
				//    geste, avec le mecanisme de Snap.h. Ce bloc PEINT, et rien
				//    d'autre — c'est ce qui rend l'aimantation mesurable sans
				//    ecran (la recette exerce le calcul, la capture juge le
				//    trait).
				// 1 px, couleur `snap_line` : la ligne de Lunacy, fine et unie.
				// Les coordonnees sont en espace DOCUMENT et passent par LA vue,
				// jamais par une conversion locale.
				if (mSt->snapVif.Aimante()) {
					const uint16 rose = NkDesignResolveRole("snap_line");
					NkDesignPaint p3(ctx, mSt->theme);
					auto badge = [&](float32 cx, float32 cy, float32 val) {
						char t[16];
						snprintf(t, sizeof(t), "%d", (int32)(val + 0.5f));
						const float32 w = costume::Largeur(costume::Fontes().px9, t) + 8.f;
						p3.Fill({cx - w * 0.5f, cy - 8.f, w, 15.f}, rose, 3.f);
						costume::Texte(ctx.dl, costume::Fontes().px9, cx - w * 0.5f + 4.f,
									   cy - 4.f, t, ctx.theme.panel);
					};
					const NkSnapGuide &gv = mSt->snapVif.guideV;
					if (gv.actif) {
						const float32 x = mSt->view.ToScreenX(gv.coord);
						const float32 y0 = mSt->view.ToScreenY(gv.de);
						const float32 y1 = mSt->view.ToScreenY(gv.a);
						p3.Fill({x, y0, 1.f, (y1 - y0) > 1.f ? (y1 - y0) : 1.f}, rose, 0.f);
						if (gv.badge)
							badge(x, (y0 + y1) * 0.5f, gv.ecart);
					}
					const NkSnapGuide &gh = mSt->snapVif.guideH;
					if (gh.actif) {
						const float32 y = mSt->view.ToScreenY(gh.coord);
						const float32 x0 = mSt->view.ToScreenX(gh.de);
						const float32 x1 = mSt->view.ToScreenX(gh.a);
						p3.Fill({x0, y, (x1 - x0) > 1.f ? (x1 - x0) : 1.f, 1.f}, rose, 0.f);
						if (gh.badge)
							badge((x0 + x1) * 0.5f, y, gh.ecart);
					}
					// ── LES DISTANCES ECRITES (Rodolf : « avec écriture des
					//    distances ») — PLUSIEURS a la fois.
					// ⚠️ CHAQUE MESURE EST UN SEGMENT **PLUS** UN NOMBRE, jamais
					//    un nombre seul. Un « 60 » flottant au milieu de la toile
					//    n'apprend rien : on ne sait pas ce qui est mesuré. Le
					//    segment relie les deux bords, les embouts le bornent, et
					//    c'est ce qui rend le nombre VERIFIABLE d'un coup d'œil
					//    au lieu de demander qu'on croie l'aimant sur parole.
					for (uint32 mi = 0; mi < mSt->snapVif.nbMesures; ++mi) {
						const NkSnapMesure &m = mSt->snapVif.mesures[mi];
						const float32 sx0 = mSt->view.ToScreenX(m.x0);
						const float32 sy0 = mSt->view.ToScreenY(m.y0);
						const float32 sx1 = mSt->view.ToScreenX(m.x1);
						const float32 sy1 = mSt->view.ToScreenY(m.y1);
						if (m.horizontal) {
							p3.Fill({sx0, sy0, (sx1 - sx0) > 1.f ? (sx1 - sx0) : 1.f, 1.f}, rose,
									0.f);
							p3.Fill({sx0, sy0 - 3.f, 1.f, 7.f}, rose, 0.f);
							p3.Fill({sx1 - 1.f, sy0 - 3.f, 1.f, 7.f}, rose, 0.f);
						} else {
							p3.Fill({sx0, sy0, 1.f, (sy1 - sy0) > 1.f ? (sy1 - sy0) : 1.f}, rose,
									0.f);
							p3.Fill({sx0 - 3.f, sy0, 7.f, 1.f}, rose, 0.f);
							p3.Fill({sx0 - 3.f, sy1 - 1.f, 7.f, 1.f}, rose, 0.f);
						}
						badge((sx0 + sx1) * 0.5f, (sy0 + sy1) * 0.5f, m.valeur);
					}
				}
				DessinerFlottants(ctx, area);

				// ── LE MENU CONTEXTUEL DE LA TOILE (Rodolf, 01/09 : « double-
				//    cliquer pour l'édition est très compliqué — clic droit →
				//    Éditer ») — le NkCtxMenu du kit, 3e consommateur (NKCode en a
				//    deux). Le double-clic reste ; le clic droit est l'ALTERNATIVE.
				//    Chaque entrée qui agit est annulable : Éditer/Renommer
				//    passent par le contrat universel d'édition, Supprimer par
				//    SupprimerSelection (recette annulation, cas « suppression »).
				//    Les entrées futures (Dupliquer, ordre z…) viendront ICI —
				//    on ne livre que ce qui agit.
				if (!modeGraphe && ctx.popupDepth == 0 && in.rightPressed && dedans
					&& !DansZone(mZoneModes, in) && !DansZone(mZoneOutils, in)
					&& !DansZone(mZoneCluster, in) && !DansZone(mZoneEventail, in)
					&& !DansZone(mZoneAppareil, in)) {
					int32 vise = -1;
					// l'étiquette d'artboard désigne SA page (la poignée)
					for (uint32 fi = 0; fi < (uint32)mSt->doc.nodes.Size() && vise < 0; ++fi) {
						const NkUINode &fn = mSt->doc.nodes[fi];
						if (!StrEq(fn.shape.Data(), "frame") || !screen.Has((int32)fi))
							continue;
						const NkPaintRect fr2 = screen.At((int32)fi);
						const NkPaintRect bande = {fr2.x, fr2.y - 26.f,
												   fr2.w > 160.f ? fr2.w : 160.f, 22.f};
						if (in.mouseX >= bande.x && in.mouseX < bande.x + bande.w
							&& in.mouseY >= bande.y && in.mouseY < bande.y + bande.h)
							vise = (int32)fi;
					}
					if (vise < 0) {
						vise = NkPickDansContexte(mSt->doc, screen, in.mouseX, in.mouseY, mForage);
						if (vise == -2)
							vise = NkPickTopLevel(mSt->doc, screen, in.mouseX, in.mouseY);
					}
					if (vise >= 0) {
						// Lunacy : le clic droit sur un element NON selectionne le
						// selectionne d'abord ; sur un element de la selection, il
						// garde la selection entiere.
						if (!mSt->sel.Contains(vise))
							mSt->SelectSingle(vise);
						mMenuNode = vise;
						mMenuCtx.open = true;
						mMenuCtx.pos = {in.mouseX, in.mouseY};
					} else {
						// ── LE CLIC DROIT DANS LE VIDE (Rodolf, 01/09) ───────
						// Signale en Q43 comme un petit chantier a part : il ne
						// faisait RIEN et ne disait RIEN (mesure : `vise < 0`).
						// Il ouvre desormais le MENU DE VUE de Lunacy.
						// ⚠️ ET IL RETIENT LE POINT CLIQUE : « Coller ici » veut
						//    dire ici, pas « au centre de la page ». Sans ce
						//    couple, l'entree aurait porte un nom qui ment.
						mMenuVide.open = true;
						mMenuVide.pos = {in.mouseX, in.mouseY};
						mMenuVidePt = {in.mouseX, in.mouseY};
					}
				}
				// ── LE MENU DE VUE, PEINT ET EXECUTE ────────────────────────
				if (mMenuVide.open) {
					const NkActionVide av = NkDessinerMenuVide(ctx, mMenuVide, *mSt, mMenuVideFiltre,
															   (int32)sizeof(mMenuVideFiltre),
															   &mMenuVideFiltreFocus);
					switch (av) {
						case NkActionVide::GrillePixels:
							mSt->grilleVisible = !mSt->grilleVisible;
							Dire(mSt->grilleVisible ? "Grille de points affichée."
													: "Grille de points masquée.",
								 "", "");
							break;
						case NkActionVide::AimanterCalques:
							mSt->aimantActif = !mSt->aimantActif;
							// ⚠️ ON EFFACE LES GUIDES EN ETEIGNANT, comme le fait
							//    le bouton du cluster : un guide qui survit a
							//    l'aimant qu'on vient d'eteindre est un trait qui
							//    ment. Les deux portes du meme reglage doivent
							//    faire le meme geste, sinon l'une des deux laisse
							//    l'ecran dans un etat que l'autre ne produit pas.
							if (!mSt->aimantActif)
								mSt->snapVif = NkSnapResultat();
							Dire(mSt->aimantActif
									 ? "Aimantation aux calques activée — bords, centres, "
									   "espacements égaux et répétés."
									 : "Aimantation aux calques désactivée.",
								 "", "");
							break;
						case NkActionVide::CollerIci:
							mSt->CollerPressePapiers();
							Dire("Collé.", "", "");
							break;
						default: break;
					}
				}
				if (mMenuCtx.open && mSt->doc.IsValidIndex(mMenuNode)) {
					// ── LE MENU VIENT DU CONSTRUCTEUR PARTAGÉ (MenuContexte.h).
					// ⚠️ ÉCRIT LÀ ET PAS ICI PARCE QUE LA HIÉRARCHIE A LE MÊME
					//    MENU. C'est le troisième retour du couple TOILE /
					//    HIÉRARCHIE sur ce chantier (après les tables de clic et
					//    l'englobant) : on ne recommence pas à l'écrire deux fois.
					//    Ce qui diffère entre les deux surfaces est
					//    l'APPLICABILITÉ, jamais le jeu d'entrées — Lunacy montre
					//    le même menu depuis sa toile et depuis son panneau Layers.
					const NkUINode &nm = mSt->doc.nodes[(uint32)mMenuNode];
					const NkActionCtx a =
						NkDessinerMenuCtx(ctx, mMenuCtx, *mSt, mMenuNode, false, mMenuFiltre,
										  (int32)sizeof(mMenuFiltre), &mMenuFiltreFocus);
					if (a == NkActionCtx::EditerTexte) {
						mEditNode = mMenuNode;
						mEditEtiquette = false;
						const char *t0 = nm.TexteEn(mSt->langueActive.Data());
						snprintf(mEditBuf, sizeof(mEditBuf), "%s", t0 ? t0 : "");
						mSt->SelectSingle(mMenuNode);
						Dire("Édition du texte — Entrée valide, Échap annule.", "", "");
					} else if (a == NkActionCtx::Renommer) {
						mEditNode = mMenuNode;
						mEditEtiquette = true;
						snprintf(mEditBuf, sizeof(mEditBuf), "%s", nm.label.Data());
						mSt->SelectSingle(mMenuNode);
						Dire("Renommage de la page — Entrée valide, Échap annule.", "", "");
					} else if (NkAppliquerActionCtx(*mSt, mMenuNode, a)) {
						Dire(mSt->status.Data(), "", "");
						if (a == NkActionCtx::Couper || a == NkActionCtx::Supprimer)
							mMenuNode = -1; // le noeud visé vient de partir
					}
					if (!mMenuCtx.open)
						mMenuFiltre[0] = 0; // le filtre ne survit pas a son menu
				} else if (mMenuCtx.open) {
					mMenuCtx.open = false; // le noeud vise a disparu : rien a montrer
				}

				// ⚠️ L'APERCU PUBLIE LE RECTANGLE DE CHAQUE NOEUD. Meme principe
				//    que pour les widgets : un essai a la souris doit viser ce que
				//    l'application a REELLEMENT dispose, jamais une coordonnee
				//    recalculee dehors. Ici c'est encore plus vrai qu'ailleurs —
				//    **la position d'un noeud est un RESULTAT** (elle se calcule
				//    depuis les tailles declarees), donc la recalculer dans l'essai
				//    reviendrait a reimplementer le solveur qu'on veut eprouver, et
				//    il serait juste par construction.
				//
				//    ⚠️ Et ca ne contredit pas « l'outil n'enregistre jamais une
				//    coordonnee » : ces rectangles ne vont PAS dans le document,
				//    ils vont dans un fichier de diagnostic que seul `--dump-ui`
				//    ecrit. Le document, lui, ne contient toujours que des tailles.
				for (uint32 i = 0; i < (uint32)mSt->doc.nodes.Size(); ++i) {
					if (!mSt->layout.Has((int32)i))
						continue;
					// ⚠️ `screen`, PAS `mSt->layout`. Le registre publie ce que
					//    l'utilisateur VOIT, donc des pixels ecran. Mesure du
					//    2026-08-28 : publier la disposition document rendait
					//    `Racine = 0.0 0.0` au lieu de `293.8 92.0` -- les tailles
					//    justes, les positions amputees du decalage du panneau.
					//    Un essai a la souris aurait vise le coin de la fenetre.
					const NkPaintRect r = screen.At((int32)i);
					char clef[128];
					snprintf(clef, sizeof(clef), "apercu.nœud.%s",
							 mSt->doc.nodes[i].label.Data());
					designkit::releve::Rect(ctx, clef, {r.x, r.y, r.w, r.h});
				}

				// Le liseré de selection se peint APRES le document et n'en fait pas
				// partie : c'est du mobilier d'editeur. La sonde ne le voit pas, et
				// c'est voulu — elle mesure l'interface, pas le decor.
				// ⚠️ `screen`, PAS `mSt->layout` -- ET C'ETAIT LE DEFAUT LE PLUS
				//    VISIBLE DE LA FENETRE. `mSt->layout` est en espace DOCUMENT ;
				//    le liseré etait donc peint a l'origine de l'ECRAN. Mesure sur
				//    capture : un trait d'accent (31,111,235) de 1 px, vertical a
				//    x = 818 de y = 60 a 823, et horizontal a y = 824. Ce sont les
				//    bords DROIT et BAS d'un rectangle de 819 x 825 pose en (0,0) --
				//    exactement la taille document de « Racine » (819,1 x 825). Ses
				//    bords gauche et haut, eux, etaient hors champ : on ne voyait
				//    que deux traits qui ne correspondaient a rien.
				// ⚠️ ET L'AVERTISSEMENT ETAIT DEJA ECRIT DANS CE FICHIER, quinze
				//    lignes plus bas, pour la SOURIS : « la designer contre
				//    mSt->layout, qui est en espace DOCUMENT, donnait un pointage
				//    juste au zoom 1 et faux partout ailleurs ». Le meme piege, une
				//    fonction plus loin, du cote du DESSIN. Une lecon ecrite pour un
				//    chemin ne protege pas l'autre.
				// ⚠️ « AccentUi » ETAIT ECRIT ICI, et ce liseré etait donc MAGENTA
				//    lui aussi. La resolution canonise desormais, mais le nom
				//    canonique s'ecrit quand meme : la canonisation est un filet,
				//    pas une dispense.
				// ⚠️ PAS DE MARQUEUR POUR LA RACINE (mesure de Rodolf, 30/08 : « il
				//    part jusqu'à l'infini vers la droite ») : la racine couvre la
				//    surface RESOLUE, plus large que la fenêtre dès que la vue est
				//    décalée — son liseré filait hors champ. Lunacy ne dessine
				//    aucun marqueur de canvas pour la page : la Hiérarchie
				//    surligne, et c'est le bon endroit.
				// ── LE MODE POINTS (§8bis restreint) ─────────────────────────
				// ⚠️ LES SOMMETS VIENNENT DE `NkSommetsDe`, LA MÊME FONCTION QUE LE
				//    PEINTRE. C'est la seule façon qu'une poignée tombe sur le
				//    sommet DESSINÉ : deux tables auraient dérivé au premier
				//    ajustement, et l'utilisateur tirerait un coin qui n'est pas là.
				// ⚠️ LE MODE SE FERME TOUT SEUL si son nœud disparaît (une
				//    suppression renumérote) : un mode points sur un nœud mort
				//    peindrait des poignées sur le vide.
				if (mSt->modeForme.noeud >= 0 && !mSt->doc.IsValidIndex(mSt->modeForme.noeud))
					mSt->modeForme.Quitter();
				// 🔴 LE MODE SE DESARME DES QUE SA FORME N'EST PLUS SELECTIONNEE.
				// C'est LE correctif du bogue bloquant du 01/09 (capture
				// `probleme_vertices_141913`) : « c'est difficile ou impossible de
				// le deselectionner ».
				// ⚠️ SANS CETTE LIGNE, le bloc ci-dessous tournait sur TOUS les
				//    clics du canevas — sa garde etait `NkGuiRectContains(area,…)`,
				//    c'est-a-dire n'importe ou — et un clic a moins de 6 px du
				//    contour de la forme QUITTEE lui ajoutait un sommet en
				//    silence, pendant que `HandleMouse` traitait le meme clic. Un
				//    geste, deux effets, dont un invisible.
				// ⚠️ ET LE MESSAGE PART AVEC LE MODE. Sur sa capture, le pied de
				//    fenetre annoncait « Mode edition de forme » pendant que
				//    l'ecran montrait les carres de redimensionnement : le texte
				//    disait un mode, l'ecran un autre, l'etat reel un troisieme.
				//    Une phrase qui survit a son etat est un mensonge d'interface.
				if (mSt->modeForme.noeud >= 0 && !NkModePointsArme(mSt->modeForme.noeud, mSt->selected)) {
					mSt->modeForme.Quitter();
					if (mSt->status.Contains("Mode édition de forme"))
						mSt->status = NkString("");
				}
				if (!modeGraphe && NkModePointsArme(mSt->modeForme.noeud, mSt->selected)
					&& screen.Has(mSt->modeForme.noeud)) {
					const NkUINode &pn = mSt->doc.nodes[(uint32)mSt->modeForme.noeud];
					const NkPaintRect rp = screen.At(mSt->modeForme.noeud);
					float32 xy[64];
					const uint32 nbS = NkSommetsDe(pn, rp, xy, 32);
					const uint16 accentP = NkDesignResolveRole("accent_ui");
					// ── LA DISTINCTION VISUELLE (retour 2 de Rodolf, 01/09) ──
					// « Dans ce cas distinguer l'édition des vertices de l'édition
					//   de la boîte englobante. »
					// ⚠️ ON NE DESSINE PLUS LA BOÎTE. Avant, le mode points
					//    tracait `OutlineSharp(rp)` — c'est-à-dire EXACTEMENT le
					//    liseré de sélection — puis posait des petits CARRÉS aux
					//    sommets, qui sur un rectangle tombent pile sur les coins
					//    de la boîte. Les deux modes rendaient donc la MÊME image :
					//    sa capture le montre, et il n'y avait aucun moyen de
					//    savoir dans lequel on se trouvait.
					//    Désormais : le CONTOUR RÉEL est surligné (il épouse la
					//    forme, pas sa boîte) et les poignées sont RONDES.
					{
						float32 ct[256];
						const uint32 nbC = NkContourDe(pn, rp, ct, 128);
						for (uint32 i = 0; i + 1 <= nbC; ++i) {
							const uint32 j = (i + 1) % nbC;
							if (nbC < 2)
								break;
							paint.Line(ct[i * 2], ct[i * 2 + 1], ct[j * 2], ct[j * 2 + 1],
									   accentP, 1.5f);
						}
					}
					const float32 hs = 9.f;
					for (uint32 i = 0; i < nbS; ++i) {
						const NkPaintRect ph{xy[i * 2] - hs * 0.5f, xy[i * 2 + 1] - hs * 0.5f,
											 hs, hs};
						// ⚠️ RONDES, ET C'EST LA MOITIÉ DU RETOUR 2 : les poignées
						//    de redimensionnement sont des CARRÉS de 6 px. Deux
						//    jeux de poignées carrées, au même endroit sur un
						//    rectangle, ne se distinguent pas — c'est ce que sa
						//    capture montre. Un rond de 9 px ne se confond avec
						//    rien, et le mode se lit d'un coup d'œil.
						const float32 rd = hs * 0.5f;
						// ⚠️ UN SOMMET ARRONDI SE VOIT SUR SA POIGNÉE, pas seulement
						//    sur la forme. Sans ça, deux sommets de rayons différents
						//    auraient exactement la même poignée, et le double-clic
						//    d'arrondi n'aurait de retour visible que si la forme est
						//    assez grande pour que l'arc se distingue — c'est-à-dire
						//    pas sur les petites formes, celles qu'on manipule le
						//    plus. La poignée d'un sommet rond est RONDE.
						const float32 rr = (i < (uint32)pn.sommets.Size())
											   ? pn.sommets[i].rayon
											   : 0.f;
						// 🔴 DÉFAUT MESURÉ LE 01/09 SUR SA CAPTURE, ET IL A COÛTÉ UN
						//    RAPPORT DE BOGUE SUR UNE AUTRE FONCTIONNALITÉ. Ces
						//    lignes appelaient `Outline(ph, accentP, 0x00000000u, rd)`
						//    en croyant écrire « intérieur transparent ». Le second
						//    paramètre d'`Outline` est un **RÔLE** (`uint16`), pas une
						//    couleur : `0x00000000u` est le rôle 0, donc une couleur
						//    PLEINE — et le `FillColor` blanc juste au-dessus était
						//    repeint en NOIR. Agrandie ×5, sa capture montre des
						//    pastilles noires pleines là où le commentaire promettait
						//    des ronds blancs cerclés d'accent.
						//    *Un paramètre déclaré qui n'est pas honoré — le même
						//    défaut, pour la neuvième fois de la semaine, et cette
						//    fois c'est le TYPE qui a menti au site d'appel.*
						const uint32 accentRGBA = mSt->theme.Get(accentP);
						// le sommet TIRÉ ET le sommet SÉLECTIONNÉ se remplissent
						// d'accent, les autres sont blancs cerclés — sans ça, la
						// section « ÉDITION DE FORME » afficherait les coordonnées
						// d'un sommet que rien ne désigne à l'écran.
						// TOUT SOMMET MARQUE est plein, pas seulement le principal :
						// une multi-selection qui ne se VOIT pas est une
						// multi-selection que personne ne sait qu il a faite.
						const bool vif = (int32)i == mSt->modeForme.tire
										 || mSt->modeForme.Marque((int32)i);
						paint.OutlineColor(ph, accentRGBA, vif ? accentRGBA : 0xFFFFFFFFu, rd);
						// un sommet ARRONDI porte un second anneau, plus large
						if (rr > 0.f)
							paint.OutlineColor({ph.x - 2.f, ph.y - 2.f, ph.w + 4.f, ph.h + 4.f},
											   accentRGBA, 0x00000000u, rd + 2.f);
					}
					// ── LES POIGNÉES DE COURBE (retour de Rodolf, 01/09 nuit) ──
					// *« pour l'arrondi on doit avoir le manipulateur de courbe. »*
					// ⚠️ ELLES NE SE PEIGNENT QUE SUR LES SOMMETS SÉLECTIONNÉS, et
					//    c'est une décision : une forme à douze sommets courbes
					//    afficherait VINGT-QUATRE poignées reliées par autant de
					//    tiges, et on ne saurait plus laquelle appartient à quoi.
					//    C'est ce que font Lunacy et Figma, et c'est aussi ce qui
					//    rend la règle de priorité tenable — une poignée qu'on ne
					//    voit pas ne vole pas de clic.
					// ⚠️ LA TIGE EST DESSINÉE AVANT LA POIGNÉE : sans le trait qui
					//    relie la poignée à son sommet, deux ronds voisins ne
					//    disent pas lequel commande lequel.
					{
						const NkUINode &pt = mSt->doc.nodes[(uint32)mSt->modeForme.noeud];
						const float32 hxT = rp.w * 0.5f, hyT = rp.h * 0.5f;
						for (uint32 i = 0; i < nbS && i < (uint32)pt.sommets.Size(); ++i) {
							if (!mSt->modeForme.Marque((int32)i))
								continue;
							const NkPoint2 &sp = pt.sommets[i];
							if (sp.liaison == NkPoint2::LiaisonDroit)
								continue;
							const float32 ax = xy[i * 2], ay = xy[i * 2 + 1];
							for (uint32 cote = 0; cote < 2; ++cote) {
								const float32 tx = (cote == 0) ? sp.ex : sp.sx;
								const float32 ty = (cote == 0) ? sp.ey : sp.sy;
								if (tx == 0.f && ty == 0.f)
									continue;
								const float32 px = ax + tx * hxT, py = ay + ty * hyT;
								paint.Line(ax, ay, px, py, accentP, 1.f);
								const float32 hp = 7.f;
								const NkPaintRect ph{px - hp * 0.5f, py - hp * 0.5f, hp, hp};
								// creuse, pour ne pas se confondre avec l'ancre
								// pleine du sommet sélectionné qui la commande
								paint.OutlineColor(ph, mSt->theme.Get(accentP), 0xFFFFFFFFu,
												   hp * 0.5f);
							}
						}
					}
					// LE GESTE : prendre un sommet, le traîner, le lâcher.
					const NkVec2 ms = ctx.input.mousePos;
					const bool dansToile = ctx.popupDepth == 0 && NkGuiRectContains(area, ms);
					// ── LE DOUBLE-CLIC SUR UN SOMMET L'ARRONDIT ──────────────
					// Retour de Rodolf, 01/09 : *« si on double-clique sur une
					// poignée sombre on peut l'arrondir. »*
					// ⚠️ IL EST TESTÉ AVANT LE SIMPLE CLIC, et ce n'est pas un
					//    détail d'ordre : depuis le correctif Win32 du matin, un
					//    double-clic arrive AVEC son appui. Laissé après, il
					//    aurait d'abord armé un glisser de sommet, et l'arrondi
					//    serait parti sur un sommet qu'on vient de bouger d'un
					//    pixel. *Le geste le plus spécifique se lit en premier.*
					if (ctx.input.mouseDoubleClicked[0] && dansToile) {
						const int32 ia = NkAncreLaPlusProche(xy, nbS, ms.x, ms.y, hs + 3.f);
						if (ia >= 0) {
							NkUINode &pa = mSt->doc.nodes[(uint32)mSt->modeForme.noeud];
							const float32 rr = NkArrondirSommet(pa, (uint32)ia);
							if (rr >= 0.f) {
								mSt->doc.MarkHumanEdit(mSt->modeForme.noeud);
								mSt->modeForme.tire = -1;
								char msg[128];
								if (rr > 0.f)
									snprintf(msg, sizeof(msg),
											 "Sommet %d arrondi à %.0f px — double-clic à "
											 "nouveau pour continuer le cycle.",
											 ia + 1, rr);
								else
									snprintf(msg, sizeof(msg),
											 "Sommet %d redevenu vif — le cycle boucle.",
											 ia + 1);
								Dire(msg, "", "");
							}
						}
					}
					else if (ctx.input.mouseClicked[0] && dansToile) {
						mSt->modeForme.tire = -1;
						// ⚠️ L'ANCRE D'ABORD, LE SEGMENT ENSUITE — la priorité est
						//    dite dans `Sommets.h` et lue ici : une ancre est POSÉE
						//    SUR son segment, donc les deux répondent au même clic.
						//    Sans cette priorité, prendre un coin pour le déplacer
						//    en aurait ajouté un second par-dessus, et la forme
						//    aurait gagné un sommet à chaque tentative de la bouger.
						// ⚠️ LA TANGENTE EST INTERROGÉE AVANT L'ANCRE, et la règle
						//    vit dans `SelectionGeste.h` (`NkAQuiLAncre`) : une
						//    poignée ramenée près de son sommet — le geste normal
						//    pour aplatir une courbe — deviendrait inatteignable si
						//    l'ancre gagnait. *Le geste le plus spécifique se lit en
						//    premier*, troisième étage de la même règle.
						mSt->modeForme.tangenteSommet = -1;
						{
							const NkUINode &pt = mSt->doc.nodes[(uint32)mSt->modeForme.noeud];
							const float32 hxT = rp.w * 0.5f, hyT = rp.h * 0.5f;
							float32 meilleure = -1.f;
							for (uint32 i = 0; i < nbS && i < (uint32)pt.sommets.Size(); ++i) {
								if (!mSt->modeForme.Marque((int32)i))
									continue;
								const NkPoint2 &sp = pt.sommets[i];
								if (sp.liaison == NkPoint2::LiaisonDroit)
									continue;
								for (uint32 cote = 0; cote < 2; ++cote) {
									const float32 tx = (cote == 0) ? sp.ex : sp.sx;
									const float32 ty = (cote == 0) ? sp.ey : sp.sy;
									if (tx == 0.f && ty == 0.f)
										continue;
									const float32 px = xy[i * 2] + tx * hxT;
									const float32 py = xy[i * 2 + 1] + ty * hyT;
									const float32 d = NkLongueur2D(ms.x - px, ms.y - py);
									if (meilleure < 0.f || d < meilleure) {
										meilleure = d;
										mSt->modeForme.tangenteSommet = (int32)i;
										mSt->modeForme.tangenteCote = cote;
									}
								}
							}
							const float32 dAncre = [&]() -> float32 {
								const int32 a = NkAncreLaPlusProche(xy, nbS, ms.x, ms.y, hs + 3.f);
								if (a < 0)
									return -1.f;
								return NkLongueur2D(ms.x - xy[a * 2], ms.y - xy[a * 2 + 1]);
							}();
							if (NkAQuiLAncre(meilleure, dAncre, hs + 3.f)
								!= NkProprioAncre::Tangente)
								mSt->modeForme.tangenteSommet = -1;
						}
						if (mSt->modeForme.tangenteSommet >= 0)
							mSt->modeForme.tire = -1;
						else
							mSt->modeForme.tire =
								NkAncreLaPlusProche(xy, nbS, ms.x, ms.y, hs + 3.f);
						// ⚠️ LE SOMMET SÉLECTIONNÉ SUIT L'ANCRE SAISIE, ET IL LUI
						//    SURVIT. C'est lui que la section « ÉDITION DE FORME »
						//    de l'Inspecteur affiche : le lier à `tire` viderait les
						//    champs X/Y au relâchement, c'est-à-dire à l'instant
						//    précis où l'on veut lire le nombre qu'on vient
						//    d'obtenir.
						// ── LA MULTI-SÉLECTION DE SOMMETS (retour 2, 01/09 soir) ──
						// ⚠️ LA TABLE DÉCIDE, PAS UN `if` LOCAL. `NkGesteSommet` vit
						//    dans `SelectionGeste.h`, **à côté** de celles de la toile
						//    et de la liste — c'est la troisième surface de sélection
						//    de cette application, et l'écrire ici aurait rouvert la
						//    divergence que les deux premières viennent de fermer.
						// ⚠️ ET UN SOMMET DÉJÀ MARQUÉ NE SE RE-SÉLECTIONNE PAS SEUL AU
						//    CLIC NU : sinon prendre l'un des cinq sommets marqués pour
						//    les déplacer ensemble en désélectionnerait quatre au
						//    moment même de la prise. C'est la règle de tous les outils
						//    de dessin, et elle est nécessaire ICI : sans elle, le
						//    « déplacement simultané » qu'il demande serait
						//    inatteignable à la souris.
						if (mSt->modeForme.tire >= 0) {
							const NkGesteSel g =
								NkGesteSommet(ctx.input.ctrlDown, ctx.input.shiftDown);
							if (g == NkGesteSel::Basculer)
								mSt->modeForme.BasculerMarque(mSt->modeForme.tire);
							else if (!mSt->modeForme.Marque(mSt->modeForme.tire))
								mSt->modeForme.MarquerSeul(mSt->modeForme.tire);
							else
								mSt->modeForme.sommet = mSt->modeForme.tire;
						}
						if (mSt->modeForme.tire < 0 && nbS >= 2) {
							// ── AJOUTER UN SOMMET SUR LE CÔTÉ ────────────────
							// Rodolf : *« on peut ajouter des informations, entre
							// autres des vertices. »* Sa capture 3 le montre : le
							// sommet neuf est SUR le côté droit du rectangle.
							float32 t = 0.f, d = 0.f;
							const int32 seg = NkSegmentLePlusProche(xy, nbS, ms.x, ms.y, t, d);
							if (seg >= 0 && d <= 6.f) {
								NkUINode &pa = mSt->doc.nodes[(uint32)mSt->modeForme.noeud];
								const int32 neuf = NkInsererSommet(pa, (uint32)seg, t);
								if (neuf >= 0) {
									mSt->doc.MarkHumanEdit(mSt->modeForme.noeud);
									mSt->modeForme.tire = neuf;
									// un sommet neuf est SEUL sélectionné : il vient de
									// naître, rien ne le groupe avec les autres.
									mSt->modeForme.MarquerSeul(neuf);
									char msg[192];
									snprintf(msg, sizeof(msg),
											 "Sommet ajouté sur le côté %d — la forme n'a pas "
											 "bougé ; glisse-le, ou double-clique-le pour "
											 "l'arrondir.",
											 seg + 1);
									Dire(msg, "", "");
								}
							}
						}
					}
					// ── LE GLISSER D'UNE TANGENTE ──────────────────────────
					// ⚠️ LES MODIFICATEURS VIENNENT DE LA SOURCE : les notes de
					//    version Lunacy disent « Alt = disconnected, Ctrl =
					//    asymmetric ». La table vit dans `SelectionGeste.h`
					//    (`NkLiaisonDuModificateur`), pas dans ce `if`.
					// ⚠️ ET LA LIAISON EST POSÉE AVANT LA TANGENTE, pas après : c'est
					//    elle qui décide de ce que fait la jumelle, donc l'appliquer
					//    ensuite propagerait selon l'ANCIEN type et rangerait les
					//    poignées d'après une règle que l'utilisateur vient de
					//    quitter.
					if (mSt->modeForme.tangenteSommet >= 0 && ctx.input.mouseDown[0]) {
						NkUINode &pm = mSt->doc.nodes[(uint32)mSt->modeForme.noeud];
						const uint32 iT = (uint32)mSt->modeForme.tangenteSommet;
						if (iT < (uint32)pm.sommets.Size() && rp.w > 0.f && rp.h > 0.f) {
							const nkentseu::uint8 li = NkLiaisonDuModificateur(
								ctx.input.ctrlDown, ctx.input.altDown);
							if (li != 255u && pm.sommets[iT].liaison != li)
								NkPoserLiaison(pm.sommets[iT], li);
							const float32 ax = xy[iT * 2], ay = xy[iT * 2 + 1];
							NkPoserTangente(pm.sommets[iT], mSt->modeForme.tangenteCote,
											(ms.x - ax) / (rp.w * 0.5f),
											(ms.y - ay) / (rp.h * 0.5f));
							mSt->doc.MarkHumanEdit(mSt->modeForme.noeud);
						}
					}
					if (mSt->modeForme.tire >= 0 && ctx.input.mouseDown[0]) {
						NkUINode &pm = mSt->doc.nodes[(uint32)mSt->modeForme.noeud];
						const NkNatureSommets nat = NkNatureDe(pm.shape.Data());
						if (NkSommetsStockes(nat)) {
							// ⚠️ ON MATÉRIALISE AVANT D'ÉCRIRE : la table du
							//    polygone régulier est une CONSTANTE partagée, on
							//    n'écrit jamais dedans. Même geste que
							//    `MaterialiserFills`.
							NkMaterialiserSommets(pm);
							if (mSt->modeForme.tire < (int32)pm.sommets.Size() && rp.w > 0.f
								&& rp.h > 0.f) {
								// retour en UNITAIRE : la boîte est l'unité.
								const float32 cx = rp.x + rp.w * 0.5f;
								const float32 cy = rp.y + rp.h * 0.5f;
								const float32 nx = (ms.x - cx) / (rp.w * 0.5f);
								const float32 ny = (ms.y - cy) / (rp.h * 0.5f);
								// ── TOUS LES SOMMETS MARQUÉS SUIVENT ───────────────
								// *« pour déplacement ou transformation simultanés »*.
								// ⚠️ LE CALCUL EST DANS `Sommets.h`, PAS ICI : écrit dans
								//    la boucle de rendu, il aurait vécu là où aucun banc
								//    ne va — la facture que ce chantier a déjà payée
								//    quatre fois. Ce qu'il tient, et qu'un cas mesure :
								//    on applique un ÉCART, jamais une position absolue.
								(void)NkDeplacerSommetsMarques(pm, mSt->modeForme.tire,
															   mSt->modeForme.marques, nx, ny);
								mSt->doc.MarkHumanEdit(mSt->modeForme.noeud);
							}
						} else if (nat == NkNatureSommets::Bouts) {
							// ⚠️ UNE LIGNE N'A PAS DE LISTE DE SOMMETS, ET ELLE N'EN
							//    A PAS BESOIN : elle EST la diagonale de sa boîte.
							//    Bouger un bout, c'est bouger un coin de la boîte —
							//    exprimable avec le modèle d'aujourd'hui, sans une
							//    clé de plus. Lui inventer des sommets aurait donné
							//    deux façons de dire la même chose.
							const float32 dxd = mSt->view.ToDocLength(ms.x - rp.x);
							const float32 dyd = mSt->view.ToDocLength(ms.y - rp.y);
							const float32 lw = mSt->view.ToDocLength(rp.w);
							const float32 lh = mSt->view.ToDocLength(rp.h);
							const bool monte = StrEq(pm.shape.Data(), "line_up");
							// bout 0 = gauche, bout 1 = droite (cf. NkSommetsDe)
							if (mSt->modeForme.tire == 0) {
								pm.posX += dxd;
								pm.width.value = lw - dxd;
								if (monte) {
									pm.height.value = dyd;
								} else {
									pm.posY += dyd;
									pm.height.value = lh - dyd;
								}
							} else {
								pm.width.value = dxd;
								if (monte) {
									pm.posY += dyd;
									pm.height.value = lh - dyd;
								} else {
									pm.height.value = dyd;
								}
							}
							if (pm.width.value < 2.f)
								pm.width.value = 2.f;
							if (pm.height.value < 2.f)
								pm.height.value = 2.f;
							pm.width.mode = NkSizeMode::Fixed;
							pm.height.mode = NkSizeMode::Fixed;
							mSt->doc.MarkHumanEdit(mSt->modeForme.noeud);
						}
					}
					// ── LA BOÎTE SE RECADRE SUR LE TRACÉ, AU RELÂCHEMENT ─────
					// Retour de Rodolf, 01/09 (soir) : « lorsqu'on modifie un objet
					// par ses vertices, on doit redéfinir sa bounding box pour la
					// sélection. » Mesuré avant de toucher : le glisser n'écrivait
					// QUE `sommets[i].x/y` — `posX/posY/width/height` ne bougeaient
					// pas, donc les poignées, la puce, l'Inspecteur, l'aimant et le
					// POINTAGE lisaient tous une boîte périmée.
					//
					// ⚠️ AU RELÂCHEMENT, PAS À CHAQUE IMAGE, et c'est une décision.
					//    Recadrer pendant le glisser change le RÉFÉRENTIEL des
					//    sommets sous la main qui les tire : le geste se battrait
					//    contre sa propre unité, et l'erreur d'arrondi
					//    s'accumulerait sur des centaines d'images. Une fois par
					//    geste, l'erreur ne s'accumule pas. *Ce qu'on recalcule à
					//    chaque image, on le fait dériver.*
					if (!ctx.input.mouseDown[0]) {
						// une tangente relâchée recadre elle aussi : elle peut
						// sortir la courbe de la boîte (cf. l'englobant, qui
						// compte les points de contrôle).
						if (mSt->modeForme.tangenteSommet >= 0
							&& mSt->doc.IsValidIndex(mSt->modeForme.noeud)) {
							NkUINode &pr2 = mSt->doc.nodes[(uint32)mSt->modeForme.noeud];
							if (NkRecadrerNoeud(pr2, ParentLibre(mSt->modeForme.noeud)))
								mSt->doc.MarkHumanEdit(mSt->modeForme.noeud);
							mSt->modeForme.tangenteSommet = -1;
						}
						if (mSt->modeForme.tire >= 0
							&& mSt->doc.IsValidIndex(mSt->modeForme.noeud)) {
							NkUINode &pr = mSt->doc.nodes[(uint32)mSt->modeForme.noeud];
							if (NkRecadrerNoeud(pr, ParentLibre(mSt->modeForme.noeud)))
								mSt->doc.MarkHumanEdit(mSt->modeForme.noeud);
							else if (!ParentLibre(mSt->modeForme.noeud))
								// ⚠️ ON LE DIT PLUTÔT QUE DE FAIRE SEMBLANT : sous un
								//    agencement calculé, `posX/posY` sont ignorés — y
								//    écrire un décalage donnerait une valeur sans effet
								//    pendant que le tracé, lui, aurait bougé.
								Dire("Sommet déplacé — la boîte n'a pas été recadrée : le "
									 "parent place ses enfants lui-même.",
									 "", "");
						}
						mSt->modeForme.tire = -1;
					}
				}

				// ── LE CONTOUR DE SURVOL (pré-sélection, Lunacy) ─────────────
				// ⚠️ IL DIT CE QUE LE CLIC PRENDRAIT, pas ce qui est pris. C'est sa
				//    seule raison d'être : sans lui, sur une maquette dense, on
				//    clique pour découvrir ce qu'on aurait sélectionné — et on
				//    perd la sélection en cours pour le savoir.
				// ⚠️ IL SUIT LA MÊME TABLE QUE LE CLIC. Dessiner le survol avec un
				//    pointage et sélectionner avec un autre donnerait un contour
				//    qui MENT dès que Ctrl est tenu (il montrerait le groupe, le
				//    clic prendrait le nœud profond). C'est exactement la classe de
				//    divergence que SelectionGeste.h vient de fermer : on ne la
				//    rouvre pas ici.
				if (!modeGraphe && !mCreating && !mMoving && !mDragging && !mMarquee
					&& ctx.popupDepth == 0 && !mSt->doc.IsValidIndex(mEditNode)
					&& NkGuiRectContains(area, ctx.input.mousePos)) {
					const float32 hx = ctx.input.mousePos.x, hy = ctx.input.mousePos.y;
					const NkGesteSel gs = NkGesteToile(ctx.input.ctrlDown, ctx.input.shiftDown);
					int32 sv = (gs == NkGesteSel::Profond)
								   ? NkPickSelectable(mSt->doc, screen, hx, hy)
								   : NkPickDansContexte(mSt->doc, screen, hx, hy, mForage);
					if (sv == -2)
						sv = NkPickTopLevel(mSt->doc, screen, hx, hy);
					// Ni la racine, ni ce qui est DÉJÀ sélectionné : un contour de
					// survol par-dessus le liseré de sélection ferait un double
					// trait que personne ne sait lire.
					if (sv > 0 && screen.Has(sv) && !mSt->sel.Contains(sv)) {
						const NkPaintRect rv = screen.At(sv);
						NkDesignPaint pv(ctx, mSt->theme);
						pv.OutlineSharp(rv, NkDesignResolveRole("accent_ui"));
					}
					mSurvol = sv; // publié au relevé : le survol se mesure
				} else {
					mSurvol = -1;
				}

				// ── LA MULTI-SÉLECTION : POIGNÉES ET PUCE SUR L'ENGLOBANT ────
				// ⚠️ CHAQUE MEMBRE GARDE SON LISERÉ, MAIS LES POIGNÉES SONT SUR LA
				//    BOÎTE COMMUNE (Lunacy). Poser huit poignées par membre en
				//    donnerait vingt-quatre pour trois éléments, et aucune ne
				//    dirait ce que le geste va faire — il agit sur le GROUPE.
				// ⚠️ L'ENGLOBANT VIENT DU MÉCANISME (`NkRectSelection`), pas d'un
				//    calcul local : c'est le même que la recette mesure, et le même
				//    que la puce affichera. Trois calculs auraient donné trois
				//    boîtes.
				const uint32 nSel = NkCompteSelection(mSt->doc, mSt->sel);
				if (!modeGraphe && nSel > 1) {
					const uint16 accentM = NkDesignResolveRole("accent_ui");
					// le liseré de chaque membre
					for (uint32 k = 0; k < (uint32)mSt->sel.items.Size(); ++k) {
						const int32 mi = mSt->sel.items[k];
						if (mi > 0 && screen.Has(mi))
							paint.OutlineSharp(screen.At(mi), accentM);
					}
					NkPaintRect engDoc;
					if (NkRectSelection(mSt->doc, mSt->layout, mSt->sel, engDoc)) {
						const NkPaintRect eng = mSt->view.ToScreen(engDoc);
						paint.OutlineSharp(eng, accentM);
						const float32 hp = 6.f;
						const float32 xs[3] = {eng.x - hp * 0.5f,
											   eng.x + eng.w * 0.5f - hp * 0.5f,
											   eng.x + eng.w - hp * 0.5f};
						const float32 ys[3] = {eng.y - hp * 0.5f,
											   eng.y + eng.h * 0.5f - hp * 0.5f,
											   eng.y + eng.h - hp * 0.5f};
						for (uint32 gy = 0; gy < 3; ++gy)
							for (uint32 gx = 0; gx < 3; ++gx) {
								if (gx == 1 && gy == 1)
									continue;
								const NkPaintRect ph{xs[gx], ys[gy], hp, hp};
								paint.FillColor(ph, 0xFFFFFFFFu, 0.f);
								paint.OutlineSharp(ph, accentM);
							}
						// LA PUCE dit la taille de l'ENGLOBANT et le NOMBRE — sur
						// une multi-sélection, « 240 x 170 » seul laisserait croire
						// qu'un seul objet fait cette taille.
						PuceTailleN(paint, eng, engDoc.w, engDoc.h, nSel);
					}
				}

				// ⚠️ LE MODE POINTS PREND LA MAIN SUR LES POIGNEES DE SELECTION, ET
				//    LA CAPTURE DU 01/09 L'A MONTRE : sur une LIGNE, les deux bouts
				//    tombent EXACTEMENT sur deux coins de la boite — les poignees de
				//    redimensionnement et celles de sommets se superposaient au
				//    pixel. Le meme point de l'ecran aurait fait deux choses, et
				//    laquelle gagne n'aurait dependu que de l'ordre du code.
				//    Lunacy masque les poignees de selection en edition de points ;
				//    on fait pareil, et le geste redevient sans ambiguite.
				if (!modeGraphe
					&& NkAQuiLaPoignee(mSt->modeForme.noeud, mSt->selected)
						   == NkProprioPoignee::Redimension
					&& nSel <= 1 && screen.Has(mSt->selected)
					&& mSt->doc.IsValidIndex(mSt->selected)
					&& mSt->doc.nodes[(uint32)mSt->selected].parent >= 0) {
					const NkPaintRect rs = screen.At(mSt->selected);
					const uint16 accent = NkDesignResolveRole("accent_ui");
					paint.OutlineSharp(rs, accent);
					// LES POIGNEES DE LA PLANCHE 22.0 : huit carres — coins et
					// milieux de bords — sur l'element selectionne (le bouton
					// « Se connecter » de la planche les montre). Petits carres
					// clairs a contour accent, le vocabulaire du §8bis. Decor
					// d'editeur : jamais dans le document, jamais dans les essais
					// 41. Le glisser des bords droit/bas redimensionne deja ; les
					// poignees RENDENT VISIBLE ou tirer.
					// COSTUME BANANI EXACT (JSX ResizeHandle) : poignees 6x6 BLANCHES
					// au bord accent — le blanc est celui de la maquette, pas un
					// role d'editeur (la selection vit sur l'artboard clair).
					const float32 hp = 6.f;
					const float32 xs[3] = {rs.x - hp * 0.5f, rs.x + rs.w * 0.5f - hp * 0.5f,
										   rs.x + rs.w - hp * 0.5f};
					const float32 ys[3] = {rs.y - hp * 0.5f, rs.y + rs.h * 0.5f - hp * 0.5f,
										   rs.y + rs.h - hp * 0.5f};
					for (uint32 gy = 0; gy < 3; ++gy)
						for (uint32 gx = 0; gx < 3; ++gx) {
							if (gx == 1 && gy == 1)
								continue; // pas de poignee au centre
							const NkPaintRect ph{xs[gx], ys[gy], hp, hp};
							paint.FillColor(ph, 0xFFFFFFFFu, 0.f);
							paint.OutlineSharp(ph, accent);
						}
					// ── LES POIGNEES DE ROTATION (Lunacy, lunacy_props_11) ───────
					// Retour de Rodolf, 01/09 : « ni autour de l'objet
					// selectionne ». Relu au pixel sur sa capture : ce ne sont PAS
					// des carres de plus sur la boite, ce sont quatre zones EN
					// DEHORS, en diagonale de chaque coin.
					// ⚠️ ET CE « EN DEHORS » EST TOUT LE CONTRAT : posees SUR les
					//    coins, elles auraient vole le geste de redimensionnement,
					//    cent fois plus frequent. La regle vit dans `Transfo.h`
					//    (`NkPoigneeRotation`) et un cas de recette la tient :
					//    AUCUNE des quatre ne chevauche la boite.
					// ⚠️ ELLES NE PARAISSENT QUE SUR CE QUI PEUT TOURNER. Un groupe
					//    n'en montre pas -- montrer une poignee qui refuse le geste
					//    serait du chrome qui promet, le defaut deja paye avec la
					//    touche F et les onglets.
					{
						const NkUINode &selN = mSt->doc.nodes[(uint32)mSt->selected];
						if (NkPeutTourner(selN)) {
							const float32 tr = 9.f;
							for (uint32 k = 0; k < NkNbPoigneesRotation(); ++k) {
								const NkPaintRect pr = NkPoigneeRotation(rs, k, tr);
								const bool sv = ctx.popupDepth == 0
												&& NkGuiRectContains(
													{pr.x, pr.y, pr.w, pr.h}, ctx.input.mousePos);
								// 🔴 ELLES NE SE PEIGNENT PLUS AU REPOS, ET C'EST LE
								//    RETOUR DE RODOLF DU 01/09 AU SOIR : « ça
								//    n'épouse pas, regarde bien ». Agrandie ×5, sa
								//    capture `probleme_nepouse_pas_181556.png` montre
								//    quatre PASTILLES NOIRES PLEINES posées en
								//    diagonale HORS des coins — il les a lues comme
								//    les sommets du rectangle, et conclu qu'ils
								//    flottaient à côté de la forme. Deux fautes,
								//    empilées :
								//    1. le disque était noir parce que `Outline`
								//       prend un RÔLE et qu'on lui passait
								//       `0x00000000u` comme une couleur (rôle 0) ;
								//    2. et même corrigé, quatre ronds permanents
								//       autour d'une sélection ne sont PAS ce que
								//       montre Lunacy : sa capture du temps 1
								//       (`lunacy_2temps_selection_181741.png`)
								//       n'affiche QUE les carrés de la boîte. La
								//       zone de rotation s'y révèle au survol.
								//    On fait pareil : rien au repos, un rond d'accent
								//    au survol et pendant le geste. *Une poignée qu'on
								//    prend pour une autre est pire qu'une poignée
								//    invisible : elle fait douter du reste de l'outil.*
								if (sv || mRotDrag == (int32)k) {
									paint.Fill(pr, accent, tr * 0.5f);
									// ⚠️ ET ELLE SE DIT, puisqu'elle ne se voit plus :
									//    une capacité qu'aucune phrase ne nomme est
									//    une capacité que l'utilisateur n'a pas.
									if (sv && mRotDrag < 0)
										mSt->status = NkString(
											"Rotation — glissez pour tourner ; Maj aimante "
											"à 15°. Le coin redimensionne, son extérieur "
											"tourne.");
								}
								if (sv && ctx.input.mouseClicked[0]) {
									mRotDrag = (int32)k;
									mRotBase = selN.rotation;
									mRotAngle0 = NkAngleDeg(rs.x + rs.w * 0.5f,
															rs.y + rs.h * 0.5f,
															ctx.input.mousePos.x,
															ctx.input.mousePos.y);
								}
							}
							// LE GESTE : l'angle sous la souris moins celui du depart.
							if (mRotDrag >= 0 && ctx.input.mouseDown[0]) {
								const float32 a = NkAngleDeg(rs.x + rs.w * 0.5f,
															 rs.y + rs.h * 0.5f,
															 ctx.input.mousePos.x,
															 ctx.input.mousePos.y);
								NkUINode &m = mSt->doc.nodes[(uint32)mSt->selected];
								const float32 brut = mRotBase + (a - mRotAngle0);
								// Maj aimante a 15 degres (Lunacy), sinon au degre.
								m.rotation = NkAngleNormalise(
									NkAngleAimante(brut, ctx.input.shiftDown));
								mSt->doc.MarkHumanEdit(mSt->selected);
								char msg[128];
								snprintf(msg, sizeof(msg),
										 "Rotation %.0f° — Maj aimante aux 15°.%s", m.rotation,
										 NkPeintureSaitTourner(m)
											 ? ""
											 : " (le texte reste droit : ce peintre ne sait pas "
											   "le tourner)");
								mSt->status = NkString(msg);
							}
							if (!ctx.input.mouseDown[0])
								mRotDrag = -1;
						}
					}
					// LE BADGE DE ROLE (Banani RoleBadge) : pilule 9 px au-dessus a
					// gauche de la selection, en FRANCAIS sur la toile (la maquette
					// ecrit « Bouton » sur la toile et « Button » dans l'arbre).
					{
						const NkUINode &sel = mSt->doc.nodes[(uint32)mSt->selected];
						if (!sel.role.Empty()) {
							const char *affiche = sel.role.Data();
							if (StrEq(affiche, "Button"))
								affiche = "Bouton";
							const uint32 teinte = paint.ColorOf(accent);
							char btxt[48];
							snprintf(btxt, sizeof(btxt), "%s", affiche);
							// largeur mesurée sur la vraie police 9 px du costume
							const float32 bw = 12.f + costume::Largeur(costume::Fontes().px9, btxt);
							const NkPaintRect pb{rs.x, rs.y - 19.f, bw, 16.f};
							paint.FillColor(pb, (teinte & 0xFFFFFF00u) | 0x22u, 8.f);
							// MOBILIER d'editeur : corps de la maquette via
							// CorpsMaquette (TextHex rend le corps demande
							// EXACTEMENT depuis la correction zoom du 31/08).
							paint.TextHex({pb.x + 6.f, pb.y, pb.w, pb.h}, btxt, teinte, accent,
										  editorkit::NkTextAlign::Left,
										  costume::CorpsMaquette(9.f), 600.f);
						}
					}
					// LA PUCE DE TAILLE (Lunacy) : pendant un GESTE seulement — au
					// repos, la maquette Banani ne la montre pas (le badge de role
					// occupe la scene) ; en glisser/redimensionnement elle reste
					// l'instrument demande par Rodolf (ref_lunacy_toile).
					if ((mDragging || mResizeEdges != 0) && mSt->layout.Has(mSt->selected)) {
						const NkPaintRect rd = mSt->layout.At(mSt->selected);
						PuceTaille(paint, rs, rd.w, rd.h);
					}
					// ── LA BARRE D'APPAREIL FLOTTANTE (Banani « Generate
					//    Mobile », 31/08) : au-dessus d'une PAGE sélectionnée —
					//    le geste exact des captures banani_mobile_1..3 : pilule
					//    appareil + menu « <cible> ✓ / Générer la version
					//    mobile ». La transposition copie le contenu, lie par
					//    `transpose_de`, et CONSTATE ce qu'elle ne sait pas
					//    absorber — le rapport (écran 27) cesse d'être vide.
					{
						const NkUINode &selN = mSt->doc.nodes[(uint32)mSt->selected];
						const bool page = StrEq(selN.shape.Data(), "frame")
										  && selN.parent >= 0
										  && mSt->doc.nodes[(uint32)selN.parent].parent < 0;
						if (!page)
							mZoneAppareil = {0.f, 0.f, 0.f, 0.f};
						else {
							auto &dlp = ctx.DL();
							auto &F = costume::Fontes();
							const NkRect pille = {rs.x + rs.w * 0.5f - 22.f, rs.y - 58.f,
												  44.f, 24.f};
							dlp.AddRectFilled(pille, {22, 27, 34, 255}, 12.f);
							dlp.AddRect(pille, ctx.theme.border, 1.f, 12.f);
							dlp.AddRect({pille.x + 8.f, pille.y + 7.f, 12.f, 9.f},
										ctx.theme.textMuted, 1.2f, 2.f);
							costume::ChevronCombo7(dlp, pille.x + 26.f, pille.y + 9.f,
												   ctx.theme.textMuted);
							mZoneAppareil = pille;
							if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
								&& NkGuiRectContains(pille, ctx.input.mousePos))
								mMenuAppareil = !mMenuAppareil;
							if (mMenuAppareil) {
								const NkRect mnu = {pille.x - 80.f, pille.y + 28.f, 230.f,
													60.f};
								// la zone consommée couvre pilule + menu
								mZoneAppareil = {mnu.x, pille.y, mnu.w + 80.f,
												 28.f + mnu.h};
								dlp.AddRectFilled({mnu.x - 1.f, mnu.y + 3.f, mnu.w + 2.f,
												   mnu.h + 4.f},
												  {0, 0, 0, 60}, 10.f);
								dlp.AddRectFilled(mnu, {22, 27, 34, 255}, 8.f);
								dlp.AddRect(mnu, ctx.theme.border, 1.f, 8.f);
								// rangée 1 : la cible ACTUELLE, cochée
								const NkRect r1 = {mnu.x + 4.f, mnu.y + 4.f, mnu.w - 8.f,
												   24.f};
								const char *cible =
									selN.target.Empty() ? "Bureau" : selN.target.Data();
								costume::Texte(dlp, F.px11, r1.x + 10.f,
											   costume::CentrerY(F.px11, r1.y, r1.h), cible,
											   ctx.theme.text);
								costume::Texte(dlp, F.px11,
											   r1.x + r1.w - 10.f
												   - costume::Largeur(F.px11, "\xE2\x9C\x93"),
											   costume::CentrerY(F.px11, r1.y, r1.h),
											   "\xE2\x9C\x93", ctx.theme.textMuted);
								// rangée 2 : « Générer la version mobile » — grisée
								// avec raison si la page est DÉJÀ une cible Mobile.
								const NkRect r2 = {mnu.x + 4.f, mnu.y + 30.f, mnu.w - 8.f,
												   24.f};
								const bool dejaMobile = !selN.target.Empty()
														&& selN.target.Data()[0] == 'M';
								const bool svr2 =
									ctx.popupDepth == 0
									&& NkGuiRectContains(r2, ctx.input.mousePos);
								if (svr2 && !dejaMobile) {
									NkColor voile = ctx.theme.accent;
									voile.a = 40;
									dlp.AddRectFilled(r2, voile, 5.f);
								}
								costume::TexteGras(dlp, F.px11, r2.x + 10.f,
												   costume::CentrerY(F.px11, r2.y, r2.h),
												   "Générer la version mobile",
												   dejaMobile ? ctx.theme.textMuted
															  : ctx.theme.text,
												   0.3f);
								if (svr2 && ctx.input.mouseClicked[0]) {
									if (dejaMobile) {
										Dire("Cette page est déjà une cible Mobile — "
											 "générer depuis la version Bureau.",
											 "", "");
									} else {
										mSt->constatsTransposition.Clear();
										const int32 ni = mSt->doc.TransposerVersMobile(
											mSt->selected, mSt->constatsTransposition);
										mMenuAppareil = false;
										if (ni >= 0) {
											mSt->doc.MarkHumanEdit(ni);
											mSt->SelectSingle(ni);
											char bb[120];
											snprintf(bb, sizeof(bb),
													 "Version mobile créée — %d constat(s) "
													 "au rapport de transposition (menu "
													 "Cible).",
													 (int32)mSt->constatsTransposition
														 .Size());
											Dire(bb, "", "");
										} else
											Dire("Transposition refusée (page invalide).",
												 "", "");
									}
								}
							}
						}
					}
				}

				// ── L'EDITION EN PLACE D'UN TEXTE (31/08, 2e passe) ──────────
				// Le champ du kit, SUPERPOSE a la position ecran du noeud, FOND
				// TRANSPARENT (« on doit voir qu'on edite DANS la toile »), a
				// L'ALIGNEMENT du noeud et au CORPS pose x zoom (le meme choix
				// d'atlas que le peintre : costume::AtlasProche). Entree valide
				// (ecrit `text` + MarkHumanEdit), Echap annule, cliquer ailleurs
				// valide (HandleMouse). Dessine SOUS le clip de la toile.
				if (!modeGraphe && mSt->doc.IsValidIndex(mEditNode)) {
					if (!screen.Has(mEditNode)) {
						FermerEditionTexte(false); // le noeud a quitte la disposition
					} else if (mEditEtiquette) {
						// LE RENOMMAGE D'ETIQUETTE : champ transparent sur la
						// bande de l'etiquette, taille MOBILIER (l'etiquette ne
						// zoome pas), couleur `doc_muted` — le nom seul, le
						// suffixe de cible se re-suffixe tout seul au valider.
						const NkPaintRect re = screen.At(mEditNode);
						const float32 dpi = ctx.S(1.f) > 0.5f ? ctx.S(1.f) : 1.f;
						float32 ech = 1.f;
						const nkgui::NkGuiFont *fed =
							costume::AtlasProche(costume::CorpsMaquette(11.f) * dpi, ech);
						editorkit::NkOverlayFieldStyle sty;
						sty.fond = false;
						sty.bord = true;
						sty.align = 0;
						sty.echelle = ech;
						sty.texte = nkentseu::editorkit::NkThemeUnpack(
							mSt->theme.Get(nkentseu::editorkit::NkRole::DocMuted));
						const nkgui::NkRect rf = {re.x - 2.f, re.y - 26.f,
												  (re.w > 160.f ? re.w : 160.f), 22.f};
						mEditRect = rf; // le contrat compare le clic a CE rectangle
						nkentseu::editorkit::NkOverlayTextField(
							ctx, ctx.DL(), fed ? fed : ctx.font, rf, mEditBuf,
							(int32)sizeof(mEditBuf), true, &sty);
						if (ctx.input.KeyPressed(nkgui::NkGuiKey::Enter))
							FermerEditionTexte(true);
						else if (ctx.input.KeyPressed(nkgui::NkGuiKey::Escape))
							FermerEditionTexte(false);
					} else {
						const NkPaintRect re = screen.At(mEditNode);
						const NkUINode &ne = mSt->doc.nodes[(uint32)mEditNode];
						const float32 dpi = ctx.S(1.f) > 0.5f ? ctx.S(1.f) : 1.f;
						const float32 vise = (ne.fontPx > 0.f ? ne.fontPx : 12.f)
											 * mSt->view.zoom * dpi;
						float32 ech = 1.f;
						const nkgui::NkGuiFont *fed = costume::AtlasProche(vise, ech);
						editorkit::NkOverlayFieldStyle sty;
						sty.fond = false; // transparent : on edite DANS la toile
						sty.bord = true;  // le lisere dit ou l'on edite
						sty.align = StrEq(ne.alignText.Data(), "centre")   ? 1
									: StrEq(ne.alignText.Data(), "droite") ? 2
																		   : 0;
						sty.echelle = ech;
						sty.texte = ne.textColor.Empty()
										? nkentseu::editorkit::NkThemeUnpack(mSt->theme.Get(
											  nkentseu::editorkit::NkRole::DocText))
										: nkentseu::editorkit::NkThemeUnpack(
											  renderdetail::NkGHexRGBA(ne.textColor.Data()));
						const nkgui::NkRect rf = {re.x - 2.f, re.y - 2.f,
												  (re.w > 40.f ? re.w : 40.f) + 4.f,
												  (re.h > 18.f ? re.h : 18.f) + 4.f};
						mEditRect = rf; // le contrat compare le clic a CE rectangle
						nkentseu::editorkit::NkOverlayTextField(
							ctx, ctx.DL(), fed ? fed : ctx.font, rf, mEditBuf,
							(int32)sizeof(mEditBuf), true, &sty);
						if (ctx.input.KeyPressed(nkgui::NkGuiKey::Enter))
							FermerEditionTexte(true);
						else if (ctx.input.KeyPressed(nkgui::NkGuiKey::Escape))
							FermerEditionTexte(false);
					}
				}

				// La découpe de la toile se referme ici — le pendant du Push posé
				// avant le fond. Tout ce qui suit reverrait le clip du dock.
				ctx.DL().PopClipRect();

				// ── LA MOITIÉ DROITE DU SPLIT + LE SÉPARATEUR (§10) ──────────
				if (splitActif) {
					const float32 xSep = areaTotale.x + areaTotale.w * mSplitRatio;
					const NkRect droite = {xSep + 3.f, areaTotale.y,
										   areaTotale.x + areaTotale.w - (xSep + 3.f),
										   areaTotale.h};
					ctx.DL().PushClipRect(droite, true);
					DessinerBlueprint(ctx, droite, 1); // Behavior, même sélection
					ctx.DL().PopClipRect();
					// le séparateur : poignée fine, curseur ↔, ratio borné 25–75 %
					const NkRect sep = {xSep - 3.f, areaTotale.y, 6.f, areaTotale.h};
					const bool sv = ctx.popupDepth == 0
									&& NkGuiRectContains(sep, ctx.input.mousePos);
					ctx.DL().AddRectFilled(sep, ctx.theme.border);
					if (sv || mSplitDrag)
						ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
					if (sv && ctx.input.mouseClicked[0])
						mSplitDrag = true;
					if (mSplitDrag) {
						if (!ctx.input.mouseDown[0])
							mSplitDrag = false;
						else if (areaTotale.w > 1.f) {
							float32 rr = (ctx.input.mousePos.x - areaTotale.x) / areaTotale.w;
							mSplitRatio = rr < 0.25f ? 0.25f : rr > 0.75f ? 0.75f : rr;
						}
					}
				}
			}

		private:
			// ── LA SOURIS, TRADUITE ──────────────────────────────────────────
			// ⚠️ CE QUE CETTE FONCTION ECRIT, ET DANS QUEL ESPACE (mise a jour du
			//    2026-08-30 — l'ancienne regle « rien ici n'ecrit une position »
			//    datait d'avant la toile posee, et le mandat du jour est
			//    exactement l'inverse) :
			//    - un clic ecrit une SELECTION ;
			//    - un glisser de bord ecrit une TAILLE (`NkResizeByDrag`) ;
			//    - un glisser du CORPS d'un noeud POSE (parent `Free`) ecrit sa
			//      POSITION (`posX`/`posY`) — c'est une propriete legitime du
			//      document depuis l'etape 38, jamais une coordonnee calculee ;
			//    - un trace a l'outil F/R/T CREE un noeud (forme = frame/rect/
			//      text), pose dans le conteneur `Free` sous le curseur.
			//    Les noeuds AGENCES (Column/Row/Grid/Anchor) restent intouchables
			//    a la souris : leur position est un RESULTAT.
			/// ⚠️ `screen` EST LA DISPOSITION EN PIXELS, ET LE PARAMETRE EST LA
			///    POUR QU ON NE PUISSE PAS SE TROMPER. La souris arrive en pixels ;
			///    la designer contre `mSt->layout`, qui est en espace DOCUMENT,
			///    donnait un pointage juste au zoom 1 et faux partout ailleurs --
			///    le genre de defaut qui ne se voit pas tant que personne ne zoome.
			void HandleMouse(const NkComponentInput &in, const NkLayoutResult &screen) {
				const float32 kHandle = 6.f;

				// ── UN DOUBLE-CLIC EST UN APPUI (regle citee, pas subie) ─────────
				// ⚠️ MESURE DU 01/09, EN QUATRE MAILLONS, ET C'EST LE DEFAUT QUE
				//    RODOLF RAPPORTAIT (« double-cliquer ne me donne pas acces a la
				//    modification fine ») : la classe de fenetre porte CS_DBLCLKS,
				//    donc Windows REMPLACE le second WM_LBUTTONDOWN par
				//    WM_LBUTTONDBLCLK ; le Win32 n'en tirait aucun appui ; la
				//    coquille ne leve `mouseDown` que sur un appui ; et cette
				//    fonction enfermait sa branche double-clic dans
				//    `if (in.mousePressed)`. Le geste reel n'y entrait JAMAIS.
				//    L'injecteur, lui, posait `mouseDown` PUIS `SetDoubleClick` :
				//    toutes les mesures passaient par une autre porte que la souris.
				//    La cause racine est corrigee dans NKWindow (l'appui est emis) ;
				//    CETTE ligne est la ceinture : un double-clic ne doit pas
				//    dependre d'un detail de plomberie pour exister.
				//    `NkAppuiDeToile` porte la regle (SelectionGeste.h) parce que le
				//    meme croisement se retrouve a CINQ endroits ci-dessous.
				const bool appui = NkAppuiDeToile(in.mousePressed, in.doubleClick);

				// ── CONTRAT UNIVERSEL D'EDITION : LE CLIC AILLEURS VALIDE ────────
				// ⚠️ MESURE DU 01/09 (meme classe que la Hierarchie) : la sortie au
				//    clic se jugeait APRES le retour anticipe « hors de la toile »
				//    — un clic sur un AUTRE PANNEAU pendant une edition en place ne
				//    validait donc jamais. Et elle se jugeait contre la TOILE
				//    entiere : un clic DANS le champ superpose validait aussi, ce
				//    que le contrat ne dit pas. Le bon rectangle est celui du CHAMP
				//    (`mEditRect`, pose par le dessin de l'overlay) : hors de lui =
				//    valider PUIS laisser le clic agir ; dedans = le clic appartient
				//    au champ, rien ne se ferme.
				if (appui && mSt->doc.IsValidIndex(mEditNode)) {
					if (DansZone(mEditRect, in))
						return; // le clic appartient au champ d'edition
					FermerEditionTexte(true); // valide — puis le clic agit normalement
				}

				// ── UN CLIC HORS DE LA TOILE N'APPARTIENT PAS A LA TOILE ─────────
				// ⚠️ MESURE PAR LE PILOTE DU 30/08, et le defaut etait a l'ECRAN
				//    depuis le debut : cliquer le champ « Contenu » de l'INSPECTEUR
				//    passait aussi par ici, tombait « dans le vide » du pointage,
				//    et VIDAIT LA SELECTION — le champ disparaissait sous le clic
				//    venu l'editer. Un clic n'est un geste de toile que s'il NAIT
				//    dans la toile (un glisser en cours, lui, continue : sortir du
				//    cadre en tirant un bord ne doit pas lacher le bord).
				const NkPaintRect &vp = mSt->view.viewport;
				if (appui
					&& !(in.mouseX >= vp.x && in.mouseX < vp.x + vp.w && in.mouseY >= vp.y
						 && in.mouseY < vp.y + vp.h))
					return;

				// ── LES FLOTTANTS MANGENT LEURS CLICS ────────────────────────────
				// ⚠️ MESURE PAR LE PILOTE DU 30/08 : cliquer un bouton de la barre
				//    d'outils atteignait AUSSI la toile en dessous — le clic
				//    choisissait l'outil ET deselectionnait (ou creait) derriere.
				//    Les trois zones flottantes (bascule de mode, barre d'outils,
				//    cluster) sont posees par `DessinerFlottants` a l'image
				//    precedente : un clic dedans appartient a leurs boutons, jamais
				//    a la toile.
				if (appui
					&& (DansZone(mZoneModes, in) || DansZone(mZoneOutils, in)
						|| DansZone(mZoneCluster, in) || DansZone(mZoneEventail, in)
						|| DansZone(mZoneAppareil, in)))
					return;

				// ── LA MAIN (famille Déplacer, variante Main — Lunacy H) : le
				//    glisser DÉPLACE LA VUE, il ne touche à rien du document.
				if (mOutil == 0 && mVarMove == 1) {
					if (appui) {
						mMainPan = true;
						mMainX = in.mouseX;
						mMainY = in.mouseY;
					}
					if (mMainPan && in.mouseDown) {
						mSt->view.PanBy(in.mouseX - mMainX, in.mouseY - mMainY);
						mMainX = in.mouseX;
						mMainY = in.mouseY;
					}
					if (!in.mouseDown)
						mMainPan = false;
					return; // la Main ne sélectionne ni ne trace
				}

				// ── LES OUTILS QUI CREENT : F (cadre), R/O... (formes), L (ligne),
				//    M (image/avatar), T (texte) ──
				const bool outilTrace = (mOutil == 1 || mOutil == 2 || mOutil == 3 || mOutil == 7);
				const bool outilTexte = (mOutil == 5);
				if (appui && (outilTrace || outilTexte)) {
					const int32 parent = NkPickFreeContainer(mSt->doc, screen, in.mouseX, in.mouseY);
					if (parent < 0) {
						Dire("Aucun conteneur libre sous le curseur — rien n'est créé.", "", "");
					} else if (outilTexte) {
						// Le texte se pose d'un CLIC, a taille de depart fixe ; son
						// contenu s'edite dans l'Inspecteur (Typographie).
						CreerForme(parent, screen, in.mouseX, in.mouseY, 160.f, 28.f, "text");
					} else {
						mCreating = true;
						mCreateParent = parent;
						mCreateX = in.mouseX;
						mCreateY = in.mouseY;
					}
					return; // un outil de trace ne selectionne pas en meme temps
				}
				if (mCreating && !in.mouseDown) {
					// Relachement : le trace devient un noeud — de la nature de la
					// FAMILLE et de sa VARIANTE (Lunacy). Un trace minuscule (clic
					// sans glisser) prend une taille de depart : une forme de 0 px
					// paraitrait perdue.
						// ⚠️ LE MÊME APPEL QUE L'APERÇU, MODIFICATEURS COMPRIS. Aperçu et
						//    relâchement qui ne liraient pas les mêmes touches donneraient
						//    une forme différente de celle qu'on a vue pendant le geste —
						//    la raison d'être de `RectTrace` comme source unique.
						const NkPaintRect t = RectTrace(in.mouseX, in.mouseY, in.shift, in.alt);
						float32 w = t.w, h = t.h;
					const bool ligne = (mOutil == 3);
					if (!ligne && w < 8.f)
						w = mOutil == 1 ? 200.f : (mOutil == 7 && mVarImage == 1 ? 64.f : 120.f);
					if (!ligne && h < 8.f)
						h = mOutil == 1 ? 160.f : (mOutil == 7 && mVarImage == 1 ? 64.f : 80.f);
					if (ligne && w < 8.f && h < 8.f)
						w = 120.f;
					const char *forme = "frame";
					bool arrondi = false;
					if (mOutil == 2) {
						if (mVariante == 1) {
							forme = "rect"; // l'ARRONDI est un rect a rayon pose
							arrondi = true;
						} else if (mVariante == 2)
							forme = "ellipse";
						else if (mVariante == 3)
							forme = "triangle"; // vague Lunacy (b), 31/08 :
						else if (mVariante == 4)
							forme = "pentagone"; // natures ADDITIVES du §4.2
						else if (mVariante == 5)
							forme = "etoile";
						else
							forme = "rect";
					} else if (mOutil == 3) {
						if (mVarLigne == 1)
							forme = "fleche";
						else
							// La diagonale MONTE si les axes du geste divergent.
							forme = ((mCreateX <= in.mouseX) != (mCreateY <= in.mouseY))
										? "line_up"
										: "line";
					} else if (mOutil == 7) {
						// natures ADDITIVES image/avatar (famille Image, 3e retour)
						forme = (mVarImage == 1) ? "avatar" : "image";
					}
					const int32 cree =
						CreerForme(mCreateParent, screen, t.x, t.y, mSt->view.ToDocLength(w),
								   mSt->view.ToDocLength(h), forme);
					if (arrondi && mSt->doc.IsValidIndex(cree)) {
						mSt->doc.nodes[(uint32)cree].radius = 8.f; // le preset Lunacy
						mSt->doc.MarkHumanEdit(cree);
					}
					mCreating = false;
				}
				if (appui) {
					// (la validation « clic ailleurs » de l'edition en place est
					// jugee EN TETE de HandleMouse, contre le rectangle du champ —
					// avant les retours anticipes, sinon un clic hors toile ne
					// validait jamais ; mesure du 01/09)
					// le contexte de forage se perime avec le document
					if (mForage >= 0 && !mSt->doc.IsValidIndex(mForage))
						mForage = -1;
					// ── LE FORAGE (regle de Rodolf, 31/08) : chaque double-clic
					//    descend D'UN NIVEAU vers l'enfant SOUS LE CURSEUR (« ceci
					//    par rapport a ou la souris atterrit ») ; une feuille
					//    EDITABLE entre en edition, une feuille non editable se
					//    selectionne et le DIT ; Echap remonte d'un niveau ; le
					//    clic simple reste au niveau courant ; le vide ressort.
					// ⚠️ PAS DE FORAGE NI D'EDITION QUAND CTRL EST TENU, et c'est
					//    MESURE, pas prudentiel. Ctrl+clic est la SELECTION PROFONDE
					//    (Lunacy) : il ne descend pas, il ne renomme pas, il n'ouvre
					//    aucun champ. Releve du 01/09 : un Ctrl+clic injecte sur un
					//    noeud TEXTE ressortait avec `mEditNode = 15` -- l'editeur
					//    s'ouvrait sous la main. La cause est dans NKGui, qui DECLARE
					//    un double-clic des que deux clics tombent a moins de 0,40 s
					//    au meme endroit (NkGuiInput.h) -- le meme piege que Rodolf
					//    avait rapporte le 18/08 sur NK3DModeler, ou « en selection
					//    multiple (Ctrl+clic de carte en carte) il ouvrait un editeur
					//    sous les doigts de l'utilisateur ». Le rayon avait ete ajoute
					//    la-bas ; il ne suffit pas quand les deux clics sont AU MEME
					//    POINT, ce qui est le cas normal d'un Ctrl+clic repete.
					//    La garde est donc ici, au sens du geste : un clic modifie
					//    n'est jamais un double-clic.
					if (in.doubleClick && !in.ctrl && !in.shift) {
						// ── double-clic sur L'ÉTIQUETTE d'un artboard = RENOMMER
						//    la page (Rodolf, 31/08 : « le nom présent dans cette
						//    vue doit être le même que dans la hiérarchie » — le
						//    nom édité EST `label`, la clé que SyncPages lit).
						for (uint32 fi = 0; fi < (uint32)mSt->doc.nodes.Size(); ++fi) {
							const NkUINode &fn = mSt->doc.nodes[fi];
							if (!StrEq(fn.shape.Data(), "frame") || !screen.Has((int32)fi))
								continue;
							const NkPaintRect fr2 = screen.At((int32)fi);
							const NkPaintRect bande = {fr2.x, fr2.y - 26.f,
													   fr2.w > 160.f ? fr2.w : 160.f, 22.f};
							if (in.mouseX >= bande.x && in.mouseX < bande.x + bande.w
								&& in.mouseY >= bande.y && in.mouseY < bande.y + bande.h) {
								mEditNode = (int32)fi;
								mEditEtiquette = true;
								snprintf(mEditBuf, sizeof(mEditBuf), "%s", fn.label.Data());
								mSt->SelectSingle((int32)fi);
								Dire("Renommage de la page — Entrée valide, Échap annule.",
									 "", "");
								return;
							}
						}
						// ⚠️ LE FORAGE **D'AVANT** SE GARDE, ET C'EST LUI QUI PORTE LA
						//    PROGRESSION DU GESTE. Deux lignes plus bas il est remis à
						//    −1 (le contexte rend −2 dès que le point ne tombe sur
						//    aucun enfant) : lire `mForage` après cette remise
						//    répondrait « on n'était nulle part » alors qu'on venait
						//    justement d'entrer dans ce nœud au coup précédent. C'est
						//    exactement ce qui faisait tourner le geste en rond sur le
						//    corps d'un rectangle à enfants.
						const int32 forageAvant = mForage;
						int32 cand = NkPickDansContexte(mSt->doc, screen, in.mouseX, in.mouseY,
														mForage);
						if (cand == -2) { // hors du contexte : ressort au 1er niveau
							mForage = -1;
							cand = NkPickTopLevel(mSt->doc, screen, in.mouseX, in.mouseY);
						}
						// 🔴 LE MODE ÉDITION DE FORME GARDE SES PROPRES GESTES.
						//    Le double-clic sur un sommet (l'arrondi) est lu par le bloc
						//    de mode points, plus haut dans la même image. Si cette
						//    branche le relit, elle ré-entre dans le mode par-dessus
						//    lui-même : `sommet` retombe à −1, la section « ÉDITION DE
						//    FORME » se vide, et le message « Sommet 3 arrondi » est
						//    remplacé par celui du forage. *Un geste, deux lecteurs, et
						//    lequel gagne ne dépendait que de l'ordre du code.*
						if (NkDblClicAuModeForme(mSt->modeForme.noeud, mSt->selected, cand))
							return;
						if (cand >= 0) {
							const NkUINode &cn = mSt->doc.nodes[(uint32)cand];
							// ⚠️ L'ISSUE VIENT DE LA TABLE (SelectionGeste.h), pas
							//    d'une cascade de `if` locale. Éditer un texte et
							//    entrer en mode points sont deux issues du MÊME
							//    geste : les décider ici, chacune à côté de
							//    l'autre, aurait rouvert la divergence que les
							//    tables de clic viennent de fermer.
							// ⚠️ ET LA SUITE AUSSI VIENT D'UN MÉCANISME, parce que
							//    `Forer` en a DEUX et que la seconde était muette.
							//    Mesure du 01/09 sur le document Dashboard : sur le
							//    CORPS d'une carte, d'un panneau ou d'un artboard —
							//    c'est-à-dire partout où aucun enfant direct n'est
							//    sous le point — `NkPickDansContexte` rendait −2, la
							//    branche faisait un `SelectSingle` SANS RIEN DIRE, et
							//    n'armait même pas le forage : le double-clic suivant
							//    refaisait la même chose. Rien ne se passait, rien ne
							//    le disait, et ça ne progressait jamais.
							const NkIssueDblClic issue = NkIssueDeDblClic(cn);
							const int32 enfant =
								(issue == NkIssueDblClic::Forer)
									? NkPickDansContexte(mSt->doc, screen, in.mouseX,
														 in.mouseY, cand)
									: -1;
							// ⚠️ MESURE DU 01/09 SUR SON DOCUMENT (`--recette-document`) :
							//    sur les 14 rectangles simples de
							//    `nkuidesign_document.nkuidoc`, les 12 qui ouvraient le
							//    mode étaient les BARRES DU GRAPHIQUE. Les rectangles
							//    qu'il voit — le bouton « Se connecter », les cartes,
							//    `Panel_Nav` — portent des enfants, donc `Forer`, et
							//    n'avaient AUCUNE route vers leurs sommets. La suite
							//    reçoit donc deux faits de plus : ce nœud est-il
							//    lui-même une forme éditable, et y étions-nous DÉJÀ ?
							const bool formeEd = NkFormeEditable(cn);
							const NkSuiteDblClic suite = NkSuiteDeDblClic(
								issue, enfant >= 0, formeEd, forageAvant == cand);
							switch (suite) {
								case NkSuiteDblClic::ForerVersEnfant:
									mForage = cand;
									mSt->SelectSingle(enfant);
									Dire("", mSt->doc.nodes[(uint32)enfant].label.Data(),
										 NkRaisonDeDblClic(suite));
									break;
								case NkSuiteDblClic::ForerSansEnfant:
									// ⚠️ ON ENTRE QUAND MÊME, et c'est ce que fait
									//    Lunacy : double-cliquer le fond d'un groupe y
									//    entre. Sans l'armement du forage, le geste ne
									//    progressait pas d'un cran, quel que soit le
									//    nombre de double-clics.
									// ⚠️ ET LA PHRASE ANNONCE LA SUITE **quand il y en
									//    a une** : sur une forme éditable, le prochain
									//    double-clic au même endroit ouvre ses sommets.
									//    Sur un artboard, non — et la phrase ne le
									//    promet pas.
									mForage = cand;
									mSt->SelectSingle(cand);
									Dire("", cn.label.Data(), NkRaisonDeDblClic(suite, formeEd));
									break;
								case NkSuiteDblClic::EditerTexte: {
									mEditNode = cand;
									const char *t0 = cn.TexteEn(mSt->langueActive.Data());
									snprintf(mEditBuf, sizeof(mEditBuf), "%s", t0 ? t0 : "");
									mSt->SelectSingle(cand);
									Dire(NkRaisonDeDblClic(suite), "", "");
									break;
								}
								case NkSuiteDblClic::ModePoints:
									// ⚠️ ON ENTRE PROPRE : ni sommet tiré, ni sommet
									//    sélectionné hérité de la forme précédente —
									//    sinon l'Inspecteur ouvrirait « ÉDITION DE
									//    FORME » sur l'indice d'un point qui
									//    appartenait à un autre objet.
									mSt->modeForme.Quitter();
									mSt->modeForme.noeud = cand;
									mSt->SelectSingle(cand);
									Dire(NkRaisonDeDblClic(suite), "", "");
									break;
								case NkSuiteDblClic::CoinsSeuls:
									// ⚠️ ON LE DIT PLUTÔT QUE DE FAIRE SEMBLANT : un
									//    rectangle n'a pas de sommets à éditer, ses
									//    coins REDIMENSIONNENT. Ouvrir un « mode
									//    points » qui ne ferait que redimensionner
									//    laisserait croire à une édition vectorielle
									//    qui n'existe pas.
									mSt->SelectSingle(cand);
									Dire(NkRaisonDeDblClic(suite), "", "");
									break;
								default:
									mSt->SelectSingle(cand);
									Dire(NkRaisonDeDblClic(suite), "", "");
									break;
							}
						} else {
							// ⚠️ LE VIDE AUSSI SE DIT. Un double-clic qui ne trouve
							//    rien ressort au premier niveau et vide la sélection :
							//    sans un mot, il est indistinguable d'un geste ignoré.
							mForage = -1;
							mSt->SelectClear();
							Dire("Double-clic dans le vide — sortie au premier niveau.", "",
								 "");
						}
						return; // un double-clic ne demarre ni glisser ni rectangle
					}
					// ⚠️ ET SI ON EST ICI SANS APPUI REEL, ON S'ARRETE. `appui` vaut
					//    vrai pour un double-clic ; un double-clic MODIFIE (Ctrl/Maj)
					//    ne passe pas par la branche ci-dessus — le laisser tomber
					//    dans le corps du geste armerait un glisser ou un rectangle
					//    alors que `mouseDown` dit le bouton relache. Le clic modifie
					//    reel, lui, arrive avec son propre `mousePressed`.
					if (!in.mousePressed)
						return;
					// ── L'ÉTIQUETTE D'ARTBOARD EST LA POIGNÉE DE LA PAGE (Rodolf,
					//    01/09, comportement Figma/Lunacy) : un CLIC la SÉLECTIONNE
					//    (pas de forage — l'étiquette désigne le cadre entier), un
					//    GLISSER depuis elle DÉPLACE la page entière dans la toile
					//    infinie (posX/posY ; les enfants sont RELATIFS, ils
					//    suivent sans une écriture). Le DOUBLE-clic (renommage) est
					//    traité AVANT et sort — pas de conflit de gestes ; et
					//    pendant un renommage, le clic dans le champ appartient au
					//    champ (tête de fonction), donc pas de drag.
					//    Un déplacement = un pas d'annulation : le même MarkHumanEdit
					//    que le déplacement au corps, coalescé par l'observateur.
					for (uint32 fi = 0; fi < (uint32)mSt->doc.nodes.Size(); ++fi) {
						const NkUINode &fn = mSt->doc.nodes[fi];
						if (!StrEq(fn.shape.Data(), "frame") || !screen.Has((int32)fi))
							continue;
						const NkPaintRect fr2 = screen.At((int32)fi);
						const NkPaintRect bande = {fr2.x, fr2.y - 26.f,
												   fr2.w > 160.f ? fr2.w : 160.f, 22.f};
						if (in.mouseX >= bande.x && in.mouseX < bande.x + bande.w
							&& in.mouseY >= bande.y && in.mouseY < bande.y + bande.h) {
							mSt->SelectSingle((int32)fi);
							mForage = -1;
							mMoving = true;
							mMoveNode = (int32)fi;
							// ⚠️ LE SECOND SITE QUI ARME `mMoving`, ET IL DOIT POSER
							//    LA POSITION LIBRE COMME L'AUTRE. Oubliée ici, elle
							//    aurait gardé la valeur du geste PRÉCÉDENT : la page
							//    aurait sauté à la première image du glisser. C'est
							//    la rançon d'un état armé à deux endroits — la même
							//    classe de défaut que `mLastX` juste en dessous, qu'il
							//    avait déjà fallu poser ici pour la même raison.
							mMoveLibreX = mSt->doc.nodes[fi].posX;
							mMoveLibreY = mSt->doc.nodes[fi].posY;
							// le pas de déplacement se mesure depuis CE point — sans
							// cette pose, le premier delta sauterait depuis le
							// dernier point connu d'un autre geste.
							mLastX = in.mouseX;
							mLastY = in.mouseY;
							return;
						}
					}
					// clic simple : au NIVEAU COURANT du forage (les freres du
					// niveau ou l'on est) ; ailleurs = ressortie au 1er niveau.
					// ⚠️ CTRL ET MAJ ETAIENT INVERSES PAR RAPPORT A LUNACY, et ce
					//    n'etait pas un choix : `Ctrl`+clic basculait la multi-
					//    selection (doc 3 §11.5, ecrit AVANT qu'on prenne Lunacy
					//    pour reference d'interaction de la toile). Chez Lunacy --
					//    et chez Figma, et chez Sketch -- c'est l'inverse :
					//      • CTRL+clic = SELECTION PROFONDE (le noeud le plus
					//        profond sous le curseur, sans passer par le forage) ;
					//      • MAJ+clic  = MULTI-SELECTION (ajoute / retire).
					//    La regle de la maison dit que diverger demande une raison
					//    ecrite et que suivre n'en demande aucune : on suit.
					// LE GESTE VIENT DE LA TABLE PARTAGEE, pas d'un `if` local.
					const NkGesteSel geste = NkGesteToile(in.ctrl, in.shift);
					int32 hit = (geste == NkGesteSel::Profond)
									? NkPickSelectable(mSt->doc, screen, in.mouseX, in.mouseY)
									: NkPickDansContexte(mSt->doc, screen, in.mouseX, in.mouseY,
														 mForage);
					if (hit == -2) {
						mForage = -1;
						hit = NkPickTopLevel(mSt->doc, screen, in.mouseX, in.mouseY);
					}
					if (hit < 0) {
						// Le vide : ressort au premier niveau, vide la selection,
						// arme le rectangle.
						// ⚠️ MAJ conserve la selection : le rectangle AJOUTE au lieu
						//    de remplacer (Lunacy). C'etait `Ctrl` ici aussi.
						mForage = -1;
						if (geste != NkGesteSel::Basculer)
							mSt->SelectClear();
						mMarquee = true;
						mMarqX = in.mouseX;
						mMarqY = in.mouseY;
					}
					// ⚠️ ET LE GESTE SUIT LE DESSIN : en mode points, le clic sur ce
					//    noeud appartient aux SOMMETS. Sans cette garde, un clic sur
					//    un bout aurait arme un redimensionnement (la poignee de
					//    selection est au meme endroit) et le sommet n'aurait jamais
					//    bouge — un mode qui s'affiche et ne repond pas.
					if (hit >= 0
						&& NkAQuiLaPoignee(mSt->modeForme.noeud, hit) == NkProprioPoignee::Sommet)
						return; // la regle, citee -- pas l'ordre des `if`
					if (hit >= 0) {
						// ⚠️ L'APPLICATION PASSE PAR LE MECANISME : c'est lui qui porte la
						//    regle « la racine n'est jamais un element », une fois pour
						//    les trois surfaces. Elle avait deja ete oubliee ICI (releve
						//    du 01/09 : selection = [0, 15], principal = la racine).
						if (geste == NkGesteSel::Basculer || !mSt->sel.Contains(hit))
							NkAppliquerGeste(mSt->doc, mSt->sel, mSt->selected, hit, geste);
						// CTRL+clic a designe un noeud PROFOND : le contexte de forage
						// SUIT. Sans ca le clic suivant repartirait du premier niveau,
						// et la selection profonde n'aurait tenu qu'une image.
						if (geste == NkGesteSel::Profond && mSt->doc.IsValidIndex(hit))
							mForage = mSt->doc.nodes[(uint32)hit].parent;
						const NkPaintRect r = screen.At(hit);
						const bool nearRight = in.mouseX >= r.x + r.w - kHandle;
						const bool nearBottom = in.mouseY >= r.y + r.h - kHandle;
						const bool nearLeft = in.mouseX <= r.x + kHandle;
						const bool nearTop = in.mouseY <= r.y + kHandle;
						NkUINode &hn = mSt->doc.nodes[(uint32)hit];
						const bool pose =
							hn.parent >= 0
							&& mSt->doc.nodes[(uint32)hn.parent].layout.kind == NkLayoutKind::Free;
						// ⚠️ LES HUIT POIGNEES REDIMENSIONNENT TOUTES (Lunacy) — pour
						//    un noeud POSE en taille Fixed. Tirer GAUCHE ou HAUT
						//    deplace l'origine EN COMPENSANT la taille : le bord
						//    oppose ne bouge pas, comme dans tout outil de dessin.
						//    Un noeud AGENCE garde l'ancien geste (droite/bas via
						//    NkResizeByDrag) : sa position est un resultat.
						if (pose && (nearRight || nearBottom || nearLeft || nearTop)
							&& hn.width.mode == NkSizeMode::Fixed
							&& hn.height.mode == NkSizeMode::Fixed) {
							mDragging = true;
							mDragNode = hit;
							mResizeEdges = (uint8)((nearLeft ? 1u : 0u) | (nearRight ? 2u : 0u)
												   | (nearTop ? 4u : 0u) | (nearBottom ? 8u : 0u));
						} else if (nearRight || nearBottom) {
							mDragging = true;
							mDragHorizontal = nearRight;
							mDragNode = hit;
							mResizeEdges = 0;
						} else if (mSt->doc.nodes[(uint32)hit].parent >= 0
								   && mSt->doc.nodes[(uint32)mSt->doc.nodes[(uint32)hit].parent]
											  .layout.kind
										  == NkLayoutKind::Free) {
							// Le CORPS d'un noeud POSE : le glisser DEPLACE —
							// `posX`/`posY` sont la propriete du document, pas un
							// calcul (cf. le bloc de tete de cette fonction). Un
							// noeud AGENCE, lui, ne s'arme pas : sa position est
							// un resultat, et la souris n'a rien a y ecrire.
							mMoving = true;
							mMoveNode = hit;
							// ⚠️ LA POSITION LIBRE — CELLE DE LA SOURIS, SANS
							//    AIMANT. Sans elle, l'aimant se relirait
							//    lui-meme : le noeud collerait, la souris
							//    continuerait, et le noeud DERIVERAIT de quelques
							//    pixels a chaque accrochage. On garde donc les
							//    deux positions — celle que la main demande, et
							//    celle que l'aimant accorde.
							mMoveLibreX = mSt->doc.nodes[(uint32)hit].posX;
							mMoveLibreY = mSt->doc.nodes[(uint32)hit].posY;
						}
					}
				}
				if (mMoving && in.mouseDown) {
					const float32 dx = mSt->view.ToDocLength(in.mouseX - mLastX);
					const float32 dy = mSt->view.ToDocLength(in.mouseY - mLastY);
					if ((dx != 0.f || dy != 0.f) && mSt->doc.IsValidIndex(mMoveNode)) {
						NkUINode &n = mSt->doc.nodes[(uint32)mMoveNode];
						mMoveLibreX += dx;
						mMoveLibreY += dy;
						NkSnapResultat snap;
						if (mSt->aimantActif && mSt->layout.Has(mMoveNode)) {
							// Le rectangle QUE LA MAIN DEMANDE : celui de la
							// disposition resolue, translate de l'ecart entre la
							// position libre et celle qui est ecrite. Aucun
							// re-solveur ici — les voisins n'ont pas bouge.
							NkPaintRect rc = mSt->layout.At(mMoveNode);
							rc.x += mMoveLibreX - n.posX;
							rc.y += mMoveLibreY - n.posY;
							// ⚠️ LA TOLERANCE SE DIVISE PAR LE ZOOM. 6 px ECRAN,
							//    pas 6 unites document : sans ca l'aimant
							//    collerait quatre fois plus fort au zoom 4,
							//    c'est-a-dire quand on cherche justement a placer
							//    finement.
							snap = NkCalculerSnap(mSt->doc, mSt->layout, mMoveNode, rc,
												  mSt->view.ToDocLength(
													  DesignState::kSnapTolEcran));
						}
						n.posX = mMoveLibreX + snap.dx;
						n.posY = mMoveLibreY + snap.dy;
						mSt->snapVif = snap;
						mSt->doc.MarkHumanEdit(mMoveNode);
					}
				}
				if (mDragging && in.mouseDown && mResizeEdges != 0
					&& mSt->doc.IsValidIndex(mDragNode)) {
					// Les huit poignees d'un noeud POSE : taille ET origine, en
					// espace document. Plancher 8 px — une forme de 0 px se perd.
					const float32 dx = mSt->view.ToDocLength(in.mouseX - mLastX);
					const float32 dy = mSt->view.ToDocLength(in.mouseY - mLastY);
					NkUINode &n = mSt->doc.nodes[(uint32)mDragNode];
					if ((mResizeEdges & 2u) && n.width.value + dx >= 8.f)
						n.width.value += dx;
					if ((mResizeEdges & 8u) && n.height.value + dy >= 8.f)
						n.height.value += dy;
					if ((mResizeEdges & 1u) && n.width.value - dx >= 8.f) {
						n.posX += dx;
						n.width.value -= dx;
					}
					if ((mResizeEdges & 4u) && n.height.value - dy >= 8.f) {
						n.posY += dy;
						n.height.value -= dy;
					}
					if (dx != 0.f || dy != 0.f)
						mSt->doc.MarkHumanEdit(mDragNode);
				} else if (mDragging && in.mouseDown) {
					// ⚠️ ICI, ET NULLE PART AILLEURS : le glissement arrive en pixels
					//    ECRAN et va ecrire une TAILLE, qui est une longueur
					//    DOCUMENT. Sans `ToDocLength`, tirer un bord de 100 px
					//    ajouterait 100 unites au zoom 1 et 100 unites au zoom 4 --
					//    soit quatre fois trop. Le bord fuirait sous le curseur, et
					//    le fichier enregistrerait une taille que personne n a voulue.
					const float32 deltaEcran =
						mDragHorizontal ? in.mouseX - mLastX : in.mouseY - mLastY;
					const float32 delta = mSt->view.ToDocLength(deltaEcran);
					if (delta != 0.f)
						NkResizeByDrag(mSt->doc, mSt->layout, mDragNode, mDragHorizontal, delta);
				}
				// ── LE RECTANGLE DE SELECTION ────────────────────────────────
				// ⚠️ IL SE TRACE A L ECRAN ET TESTE EN DOCUMENT. Les deux coins
				//    sont traduits UNE fois ; `NkPickInRect` travaille ensuite dans
				//    l'espace de la disposition. Traduire N formes vers l'ecran
				//    aurait donne le meme resultat pour N conversions au lieu de
				//    deux -- autant d occasions de se tromper d espace.
				if (mMarquee && !in.mouseDown) {
					const NkPaintRect zone = NkRectFromPoints(
						mSt->view.ToDocX(mMarqX), mSt->view.ToDocY(mMarqY),
						mSt->view.ToDocX(in.mouseX), mSt->view.ToDocY(in.mouseY));
					if (zone.w > 2.f || zone.h > 2.f) {
						NkPickInRect(mSt->doc, mSt->layout, zone, mSt->sel);
						mSt->selected = mSt->sel.Primary();
					}
					mMarquee = false;
				}
				if (!in.mouseDown) {
					mDragging = false;
					mMarquee = false;
					mMoving = false;
					// Le guide meurt AVEC son geste : un trait qui survit au
					// relacher est un trait qui ment sur ce qui est aligne.
					mSt->snapVif = NkSnapResultat();
				}
				mLastX = in.mouseX;
				mLastY = in.mouseY;
			}

			/// LA CREATION D'UN NOEUD PAR UN OUTIL. `x`/`y` sont en pixels ECRAN
			/// (le point de depart du trace), `w`/`h` deja en espace DOCUMENT. La
			/// position ecrite est RELATIVE AU PARENT — `posX` d'une forme dans un
			/// artboard est mesuree depuis l'artboard, pas depuis la toile, sinon
			/// deplacer l'artboard laisserait ses formes derriere.
			int32 CreerForme(int32 parent, const NkLayoutResult &screen, float32 x, float32 y,
							 float32 w, float32 h, const char *shape) {
				if (!mSt->doc.IsValidIndex(parent))
					return -1;
				const int32 idx = mSt->doc.AddChild(parent, "", NkAuthor::Humain);
				if (!mSt->doc.IsValidIndex(idx))
					return -1;
				// Le nom : nature + numero d'ordre parmi les memes natures. « Cadre
				// 2 » se cherche dans la Hierarchie ; « noeud 17 » non.
				uint32 memes = 0;
				for (uint32 i = 0; i < (uint32)mSt->doc.nodes.Size(); ++i)
					if (StrEq(mSt->doc.nodes[i].shape.Data(), shape))
						++memes;
				// ⚠️ CHAQUE NATURE A SON NOM (defaut paye : les triangles de la
				//    vague (b) naissaient « Texte N », le repli d'en bas).
				const char *base = StrEq(shape, "frame")	   ? "Cadre"
								   : StrEq(shape, "rect")	   ? "Rectangle"
								   : StrEq(shape, "ellipse")   ? "Ellipse"
								   : StrEq(shape, "line")	   ? "Ligne"
								   : StrEq(shape, "line_up")   ? "Ligne"
								   : StrEq(shape, "triangle")  ? "Triangle"
								   : StrEq(shape, "pentagone") ? "Pentagone"
								   : StrEq(shape, "etoile")	   ? "Étoile"
								   : StrEq(shape, "fleche")	   ? "Flèche"
								   : StrEq(shape, "image")	   ? "Image"
								   : StrEq(shape, "avatar")	   ? "Avatar"
															   : "Texte";
				char nom[48];
				snprintf(nom, sizeof(nom), "%s %u", base, memes + 1u);
				NkUINode &n = mSt->doc.nodes[(uint32)idx];
				n.label = NkString(nom);
				n.shape = NkString(shape);
				if (StrEq(shape, "frame"))
					n.layout.kind = NkLayoutKind::Free; // un cadre RECOIT des formes
				if (StrEq(shape, "text"))
					n.text = NkString("Texte");
				// ⚠️ L'ORIGINE DU PARENT SE LIT DANS LA DISPOSITION DOCUMENT
				//    (`mSt->layout`), pas dans `screen` : `posX` est une longueur
				//    DOCUMENT depuis le parent. Passer par l'ecran aurait remis le
				//    zoom dans une propriete qui n'en depend pas.
				float32 px = mSt->view.ToDocX(x);
				float32 py = mSt->view.ToDocY(y);
				if (mSt->layout.Has(parent)) {
					px -= mSt->layout.At(parent).x;
					py -= mSt->layout.At(parent).y;
				}
				n.posX = px;
				n.posY = py;
				n.width.mode = NkSizeMode::Fixed;
				n.width.value = w;
				n.height.mode = NkSizeMode::Fixed;
				n.height.value = h;
				mSt->doc.MarkHumanEdit(idx);
				mSt->SelectSingle(idx);
				mSt->host.SyncTo(mSt->doc);
				// L'outil revient a la Selection : on trace UNE forme, puis on la
				// place — le geste des outils de dessin, et celui de la planche
				// (le bouton « Se connecter » y est selectionne, pas en cours de
				// trace).
				mOutil = 0;
				Dire("", mSt->doc.nodes[(uint32)idx].label.Data(), " créé — outil Sélection.");
				return idx;
			}

			static bool DansZone(const NkRect &z, const NkComponentInput &in) {
				return z.w > 0.f && in.mouseX >= z.x && in.mouseX < z.x + z.w && in.mouseY >= z.y
					   && in.mouseY < z.y + z.h;
			}

			/// LA VUE BLUEPRINT (écrans 13/18), extraite pour servir AUSSI la
			/// moitié droite du mode Split (§10) : fond #0d1117, grille 20/100,
			/// bande « Portée : » sur la sélection RÉELLE, Entry canonique en
			/// Animation, état vide dit. `mode` : 1 Behavior, 2 Animation.
			void DessinerBlueprint(NkGuiContext &ctx, const NkRect &zone, uint32 mode) {
				auto &dlg = ctx.DL();
				dlg.AddRectFilled({zone.x, zone.y, zone.w, zone.h}, {13, 17, 23, 255});
				const NkColor fine = {60, 80, 120, 64};
				const NkColor forte = {60, 80, 120, 115};
				for (float32 gx = zone.x; gx < zone.x + zone.w; gx += 20.f)
					dlg.AddLine({gx, zone.y}, {gx, zone.y + zone.h},
								(((int32)((gx - zone.x) / 20.f)) % 5 == 0) ? forte : fine,
								(((int32)((gx - zone.x) / 20.f)) % 5 == 0) ? 1.f : 0.5f);
				for (float32 gy = zone.y; gy < zone.y + zone.h; gy += 20.f)
					dlg.AddLine({zone.x, gy}, {zone.x + zone.w, gy},
								(((int32)((gy - zone.y) / 20.f)) % 5 == 0) ? forte : fine,
								(((int32)((gy - zone.y) / 20.f)) % 5 == 0) ? 1.f : 0.5f);
				auto &F = costume::Fontes();
				// ── LA BANDE DE PORTÉE : « Portée : » + la sélection RÉELLE ──
				{
					const char *nomSel = "(aucune sélection)";
					if (mSt->doc.IsValidIndex(mSt->selected))
						nomSel = mSt->doc.nodes[(uint32)mSt->selected].label.Data();
					const float32 wl = costume::Largeur(F.px10, "Portée :");
					const float32 wn = costume::Largeur(F.px11, nomSel);
					const NkRect bp = {zone.x + 14.f, zone.y + 12.f, wl + wn + 34.f, 26.f};
					dlg.AddRectFilled(bp, {26, 32, 48, 255}, 5.f);
					dlg.AddRect(bp, {42, 53, 72, 255}, 1.f, 5.f);
					costume::Texte(dlg, F.px10, bp.x + 10.f,
								   costume::CentrerY(F.px10, bp.y, bp.h), "Portée :",
								   {139, 148, 158, 255});
					costume::TexteGras(dlg, F.px11, bp.x + wl + 16.f,
									   costume::CentrerY(F.px11, bp.y, bp.h), nomSel,
									   {79, 142, 247, 255}, 0.3f);
				}
				if (mode == 2) {
					// ── L'ENTRY canonique (écran 18 : tout graphe d'états en a
					//    UN — ce n'est pas de la démo, c'est l'état de départ).
					const NkRect en = {zone.x + 80.f, zone.y + zone.h * 0.4f, 92.f, 34.f};
					dlg.AddRectFilled(en, {30, 42, 64, 255}, 17.f);
					dlg.AddRect(en, {79, 142, 247, 255}, 1.5f, 17.f);
					costume::TexteGras(dlg, F.px11,
									   en.x + (en.w - costume::Largeur(F.px11, "Entry")) * 0.5f,
									   costume::CentrerY(F.px11, en.y, en.h), "Entry",
									   {230, 237, 243, 255}, 0.3f);
				}
				// ── L'ÉTAT VIDE, DIT : le graphe attend son modèle ──────────
				{
					const char *ph = (mode == 1)
										 ? "Le graphe de comportement naîtra du modèle "
										   "(§4.6) — ses nœuds, des événements de "
										   "l'onglet Behavior."
										 : "Les états s'ajouteront ici — Entry est posé, "
										   "le graphe attend son modèle.";
					float32 tx = zone.x + (zone.w - costume::Largeur(F.px11, ph)) * 0.5f;
					if (tx < zone.x + 8.f)
						tx = zone.x + 8.f; // une moitié étroite garde le début lisible
					costume::Texte(dlg, F.px11, tx, zone.y + zone.h * 0.62f, ph,
								   {101, 109, 118, 255});
				}
			}

			DesignState *mSt;
			bool mDragging = false;
			bool mDragHorizontal = true;
			int32 mDragNode = -1;
			uint8 mResizeEdges = 0; ///< bits 1=G 2=D 4=H 8=B (noeud pose, huit poignees)
			bool mMoving = false;
			int32 mMoveNode = -1;
			/// LE GLISSER DE ROTATION : quelle poignée est tenue, l'angle du nœud
			/// au début du geste, et l'angle de la souris au début.
			/// ⚠️ LES DEUX ANGLES DE DÉPART SONT MÉMORISÉS, ET C'EST NÉCESSAIRE :
			///    prendre « l'angle sous la souris » comme rotation ferait SAUTER
			///    l'objet au premier pixel du geste, pour l'aligner sur le point
			///    saisi. On applique un ÉCART, pas une valeur absolue — l'objet
			///    part de là où il est.
			int32 mRotDrag = -1;
			float32 mRotBase = 0.f;
			float32 mRotAngle0 = 0.f;
			/// Le noeud SOUS LE CURSEUR (pre-selection), -1 si aucun. Pose a
			/// chaque image par le dessin du survol, publie au releve.
			int32 mSurvol = -1;
			/// La position que la MAIN demande, sans aimant (cf. le commentaire
			/// au site du geste). Posee a l'armement du deplacement.
			float32 mMoveLibreX = 0.f, mMoveLibreY = 0.f;
			bool mCreating = false;
			int32 mCreateParent = -1;
			float32 mCreateX = 0.f, mCreateY = 0.f;
			float32 mLastX = 0.f, mLastY = 0.f;
			/// La variante armée de la famille Formes : 0 rectangle, 1 ellipse,
			/// 2 ligne (Lunacy : R, O, L), puis la vague (b) du 31/08 —
			/// 3 triangle, 4 pentagone, 5 étoile, 6 flèche. Elle est aussi la
			/// FACE du bouton (GlypheForme la dessine).
			uint32 mVariante = 0;
			/// L'éventail ouvert : l'INDEX de la famille (-1 = aucun) — chaque
			/// famille Lunacy montre le sien (3e retour, 01/09).
			int32 mEventailFam = -1;
			/// Les variantes RETENUES par famille (la face du bouton les montre —
			/// Lunacy retient la dernière) : Déplacer (0 sélection, 1 main),
			/// Ligne (0 ligne, 1 flèche), Image (0 image, 1 avatar). La variante
			/// de FORMES reste `mVariante` (0 rect, 1 arrondi, 2 ellipse,
			/// 3 triangle, 4 pentagone, 5 étoile).
			uint32 mVarMove = 0;
			uint32 mVarLigne = 0;
			uint32 mVarImage = 0;
			/// L'outil Main en cours de glisser (pan de la vue).
			bool mMainPan = false;
			float32 mMainX = 0.f;
			float32 mMainY = 0.f;
			// ── L'instrument de fluidite (mandat 01/09) ──────────────────────
			/// Traine d'activite : > 0 = une entree recente peut avoir mute le
			/// document — l'observateur et la pastille serialisent ; a 0, repos
			/// complet, AUCUNE serialisation par image.
			int32 mHorlogeActivite = 45;
			uint32 mFluGenVue = 0;	 ///< editionGeneration vue (restaurations)
			float32 mFluRoule = 0.f; ///< moyenne glissante du cout d'image (ms)
			float32 mFluMin = 0.f, mFluMax = 0.f, mFluSomme = 0.f;
			uint32 mFluN = 0; ///< images du geste en cours
			float32 mFluDMin = 0.f, mFluDMoy = 0.f, mFluDMax = 0.f;
			uint32 mFluDN = 0; ///< les chiffres du DERNIER geste (publies)
			bool mAideInitiale = false;
			uint32 mFramePastille = 0;
			NkRect mZoneEventail = {0.f, 0.f, 0.f, 0.f};
			/// Les zones flottantes de l'image PRECEDENTE (bascule, outils,
			/// cluster) — posees par `DessinerFlottants`, lues par `HandleMouse`
			/// pour que la toile ne recoive pas leurs clics.
			NkRect mZoneModes = {0.f, 0.f, 0.f, 0.f};
			NkRect mZoneOutils = {0.f, 0.f, 0.f, 0.f};
			NkRect mZoneCluster = {0.f, 0.f, 0.f, 0.f};
			/// L'EDITION EN PLACE D'UN TEXTE (test de Rodolf, 31/08 : double-clic
			/// sur un noeud texte = champ superpose a sa position). -1 = aucune.
			int32 mEditNode = -1;
			char mEditBuf[600] = {0};
			/// Le rectangle du CHAMP superpose, pose par le dessin de l'overlay
			/// (image precedente, meme patron que mZoneOutils) : c'est LUI que le
			/// contrat universel d'edition compare au clic — pas la toile entiere.
			NkRect mEditRect = {0.f, 0.f, 0.f, 0.f};
			/// LE MENU CONTEXTUEL de la toile (clic droit — NkCtxMenu du kit) et
			/// le noeud qu'il vise. Pendant qu'il est ouvert, HandleMouse se tait.
			nkentseu::editorkit::NkCtxMenu mMenuCtx;
			int32 mMenuNode = -1;
			/// La barre de recherche du menu contextuel (Lunacy la met en tête).
			/// ⚠️ ELLE NE SURVIT PAS A SON MENU : un filtre garde ferait rouvrir
			///    le menu suivant DEJA filtre, et l'utilisateur croirait la moitie
			///    des commandes disparue.
			char mMenuFiltre[48] = {};
			bool mMenuFiltreFocus = true;
			/// LE MENU DU CLIC DROIT DANS LE VIDE — le menu de VUE.
			/// ⚠️ UN SECOND `NkCtxMenu`, ET C'EST VOULU : ce sont deux menus qui
			///    peuvent être ouverts par deux gestes différents, avec chacun son
			///    filtre de recherche. Un seul état partagé aurait fait que taper
			///    dans l'un filtrerait l'autre au prochain clic droit — le défaut
			///    exact que le commentaire ci-dessus décrit, un cran plus haut.
			nkentseu::editorkit::NkCtxMenu mMenuVide;
			char mMenuVideFiltre[48] = {};
			bool mMenuVideFiltreFocus = true;
			/// LE POINT CLIQUÉ, retenu pour « Coller ici » — qui veut dire ICI.
			NkVec2 mMenuVidePt = {0.f, 0.f};
			/// Vrai quand l'edition porte sur L'ETIQUETTE d'un artboard (le NOM
			/// de la page — LA cle que la Hierarchie lit aussi), pas sur `text`.
			bool mEditEtiquette = false;
			/// LE CONTEXTE DE FORAGE (regle du 31/08) : le groupe dans lequel les
			/// doubles-clics sont descendus. -1 = premier niveau. Echap remonte,
			/// le clic dans le vide ressort.
			int32 mForage = -1;
			/// LE MODE SPLIT (§10) : part de la moitie Design (bornee 25–75 %) et
			/// glisser du separateur en cours.
			float32 mSplitRatio = 0.5f;
			bool mSplitDrag = false;
			/// LA BARRE D'APPAREIL flottante (« Generate Mobile ») : menu ouvert,
			/// et sa zone de l'image precedente (HandleMouse n'y clique pas).
			bool mMenuAppareil = false;
			NkRect mZoneAppareil = {0.f, 0.f, 0.f, 0.f};
			/// Compteur d'images pour les leviers --annuler/--retablir.
			uint32 mFrameHistorique = 0;

			/// Ferme l'edition en place : valide (ecrit `text` — ou `label` pour
			/// une etiquette d'artboard — + MarkHumanEdit) ou annule. Les deux
			/// sorties se DISENT dans la ligne d'aide.
			void FermerEditionTexte(bool valider) {
				if (valider && mSt->doc.IsValidIndex(mEditNode)) {
					NkUINode &n = mSt->doc.nodes[(uint32)mEditNode];
					if (mEditEtiquette) {
						// le NOM seul — le suffixe « — Mobile 390 x 844 » est un
						// AFFICHAGE derive de la cible, jamais dans la cle.
						n.label = NkString(mEditBuf);
						Dire("Page renommée — la Hiérarchie lit la même clé.", "", "");
					} else {
						// multilingue (01/09) : la frappe ecrit LA LANGUE ACTIVE
						// (vide = la principale `texte`) — bascule a chaud.
						n.PoserTexte(mSt->langueActive.Data(), mEditBuf);
						Dire("Texte modifié.", "", "");
					}
					mSt->doc.MarkHumanEdit(mEditNode);
				} else if (mEditNode >= 0)
					Dire("Édition annulée — rien n'a bougé.", "", "");
				mEditNode = -1;
				mEditEtiquette = false;
				mEditRect = {0.f, 0.f, 0.f, 0.f}; // un rect perime mangerait un clic
			}

			// ═══════════════════════════════════════════════════════════════════
			//  LES TROIS FLOTTANTS DU PLAN — ils SURPLOMBENT la toile
			// ═══════════════════════════════════════════════════════════════════
			//  Le plan les dessine en orange, avec la note : « zones en orange =
			//  flottantes au-dessus du canvas, elles ne le redimensionnent pas ».
			//  Et §7 le redit contre la tentation évidente : « il n'existe pas de
			//  troisième bande d'outils — les outils flottent au-dessus du canvas,
			//  ils ne bordent pas la fenêtre. »
			//
			//  ⚠️ CE QUE ÇA IMPOSE AU CODE, et c'est la seule chose qui compte ici :
			//     ils sont posés à des RECTANGLES calculés depuis `zone`, après le
			//     dessin du document, et ils n'avancent **jamais** le curseur de
			//     mise en page. Un `NextItemRect` leur réserverait de la place —
			//     et ils redeviendraient des bandes sans que personne ne le décide.
			//
			//  ⚠️ INERTES POUR CE MORCEAU, ET ILS LE DISENT. Chacun pose son
			//     message dans le pied de fenêtre au clic. Un bouton muet se lit
			//     comme un bouton cassé ; on cherche alors le défaut là où il n'y
			//     en a pas. Ce qui se juge aujourd'hui est la PLACE.
			void DessinerFlottants(NkGuiContext &ctx, const NkRect &zone) {
				auto &dl = ctx.DL();
				const NkColor fond = ctx.theme.panel;
				const NkColor bord = ctx.theme.border;

				// ── 1. LA BASCULE DE MODE, centrée en HAUT de la toile (§7) ──
				//    ⚠️ Elle n'est PAS dans la barre d'outils, et c'est délibéré :
				//       elle choisit QUELLE barre s'affiche, elle ne peut donc pas
				//       vivre dedans.
				{
					// COSTUME BANANI (31/08) : bascule aux mesures du JSX — haut 14,
					// boutons h 26 « icône 11 + écart 4 + libellé 11 px fw500 »,
					// padding 12, fond `panel`, bord `border`, rayon 4, ombre. Les
					// clics sont pris à la main (le Button du socle repeindrait son
					// fond par-dessus le costume — mesuré le 28/08 sur les onglets).
					static const char *const kModes[4] = {"Design", "Behavior", "Animation",
														  "Split"};
					auto &F = costume::Fontes();
					const float32 h = 26.f;
					float32 lw[4], w = 0.f;
					for (uint32 i = 0; i < 4; ++i) {
						lw[i] = 12.f + 11.f + 4.f + costume::Largeur(F.px11, kModes[i]) + 12.f;
						w += lw[i];
					}
					const NkRect r = {zone.x + (zone.w - w) * 0.5f, zone.y + 14.f, w, h};
					mZoneModes = r; // HandleMouse ne doit pas voir ses clics
					dl.AddRectFilled({r.x - 1.f, r.y + 2.f, r.w + 2.f, r.h + 4.f},
									 {0, 0, 0, 50}, 8.f); // l'ombre portée, approchée
					dl.AddRectFilled(r, fond, 4.f);
					dl.AddRect(r, bord, 1.f, 4.f);
					float32 bx = r.x;
					for (uint32 i = 0; i < 4; ++i) {
						const NkRect c = {bx, r.y, lw[i], h};
						bx += lw[i];
						if (i == mMode)
							dl.AddRectFilled(c, ctx.theme.accent, i == 0 ? 4.f : 0.f);
						const NkColor ic = (i == mMode) ? ctx.theme.accent : ctx.theme.textMuted;
						const float32 iy = c.y + (h - 11.f) * 0.5f;
						if (i == 0)
							costume::ModeDesign(dl, c.x + 12.f, iy, ic);
						else if (i == 1)
							costume::ModeBehavior(dl, c.x + 12.f, iy, ic);
						else if (i == 2)
							costume::ModeAnimation(dl, c.x + 12.f, iy, ic);
						else
							costume::ModeSplit(dl, c.x + 12.f, iy, ic);
						const float32 ty = costume::CentrerY(F.px11, c.y, h);
						if (i == mMode)
							costume::TexteGras(dl, F.px11, c.x + 27.f, ty, kModes[i],
											   ctx.theme.onAccent, 0.3f);
						else
							costume::TexteGras(dl, F.px11, c.x + 27.f, ty, kModes[i],
											   ctx.theme.textMuted, 0.3f);
						if (ctx.input.mouseClicked[0] && ctx.popupDepth == 0
							&& NkGuiRectContains(c, ctx.input.mousePos)) {
							mMode = i;
							Dire("Mode ", kModes[i],
								 (i == 0)	  ? " : la toile de design."
								 : (i == 3) ? " : Design | Behavior côte à côte — le "
											  "séparateur se tire (25–75 %)."
											: " : la vue se dessine ; le graphe naîtra du modèle.");
						}
					}
				}

				if (mMode == 1 || mMode == 2)
					return; // les modes graphe n'ont ni rail d'outils ni cluster (écran 13)
				// ── 2. LA BARRE D'OUTILS VERTICALE, 48 px (§7) ───────────────
				//    ⚠️ SES DEUX COTES SONT DES CONSTANTES NOMMEES parce que la VUE
				//       s'en sert pour ne pas naitre dessous. Deux litteraux, et le
				//       jour ou la barre s'elargit, le document repasse dessous sans
				//       que rien ne le dise.
				//    « panneau flottant vertical, posé entre la Hiérarchie et le
				//    canvas, largeur 48 px, centré verticalement ». Elle flotte
				//    AU-DESSUS : la toile passe dessous.
				//    ⚠️ Un bouton porte une FAMILLE, pas un outil (§7.1) — le
				//       chevron et l'éventail viendront ; la place est prise.
				{
					// ⚠️ QUATRE FAMILLES AGISSENT (2026-08-30, chaîne du designer) :
					//    Sélection déplace, Cadre pose un artboard, Formes un
					//    rectangle, Texte un texte. Vectoriel, Média et Mesure
					//    restent à brancher et le DISENT.
					// ⚠️ DES ICÔNES, PLUS DES LETTRES (question de Rodolf, 30/08 :
					//    « est-ce que tu utilises les bonnes icônes ? » — mesuré
					//    sur la capture : non). Les glyphes sont VECTORIELS,
					//    dessinés dans le vocabulaire de la liste de dessin — même
					//    choix que le chevron de `NkDesignPaint` (Icons.h) : pas de
					//    bitmap, pas de caractère absent de l'atlas de police. Le
					//    raccourci s'apprend en INFOBULLE (l'icône montre, la
					//    lettre s'apprend — le principe des menus) ; la TOUCHE
					//    elle-même reste à brancher, comme les raccourcis grisés
					//    des menus. Les libellés `##…` ne rendent aucun texte : le
					//    glyphe est peint par-dessus le bouton.
					// ── LE RAIL AUX FAMILLES LUNACY (3e retour, 01/09 : « les onze
					//    references en entier ») — l'ordre du rail au repos
					//    (panneau 11) : Déplacer, Cadre, Formes, Ligne, Connecteur,
					//    Texte, Plume, Image, Icône. MOINS la famille Bouton —
					//    l'exception de Rodolf : « on a déjà une partie réservée
					//    aux composants à part ». Une famille qui n'agit pas
					//    encore est GRISÉE ET LE DIT au clic (jamais un no-op
					//    muet) ; chaque famille à variantes porte un CHEVRON
					//    VIVANT et retient sa dernière variante (la face change).
					static const char *const kOutilsBulles[9] = {
						"Déplacer (V · Main H)", "Cadre (F)",	 "Formes (R · O)",
						"Ligne (L)",			 "Connecteur (X)", "Texte (T)",
						"Plume (P)",			 "Image (M)",	 "Icône (N)"};
					// vrai = la famille agit ; faux = grisée-qui-le-dit
					static const bool kFamAgit[9] = {true, true,  true, true, false,
													 true, false, true, false};
					// familles à éventail (chevron vivant)
					static const bool kFamEventail[9] = {true, true,  true,  true, false,
														 false, true, true, false};
					const float32 w = kOutilsLargeur, hb = 28.f;
					const float32 filet = 5.f; // 2 + 1 + 2 (my-0.5 + border-t)
					const float32 h = 6.f + hb * 9.f + filet + 6.f;
					const NkRect r = {zone.x + kOutilsMarge, zone.y + (zone.h - h) * 0.5f, w, h};
					mZoneOutils = r; // idem
					dl.AddRectFilled({r.x - 1.f, r.y + 3.f, r.w + 2.f, r.h + 4.f},
									 {0, 0, 0, 55}, 10.f); // ombre 0 4 16 approchée
					dl.AddRectFilled(r, fond, 5.f);
					dl.AddRect(r, bord, 1.f, 5.f);
					for (uint32 i = 0; i < 9; ++i) {
						// le filet sépare les outils de POSE du texte (comme avant)
						const float32 yOff = (i >= 5) ? filet : 0.f;
						if (i == 5)
							dl.AddLine({r.x + 8.f, r.y + 6.f + hb * 5.f + 2.5f},
									   {r.x + w - 8.f, r.y + 6.f + hb * 5.f + 2.5f}, bord, 1.f);
						const NkRect c = {r.x, r.y + 6.f + hb * (float32)i + yOff, w, hb};
						const bool survol =
							ctx.popupDepth == 0 && NkGuiRectContains(c, ctx.input.mousePos);
						if (survol && ctx.input.mouseClicked[0]) {
							// Le CHEVRON est VIVANT (Lunacy) : le coin bas-droit
							// ouvre l'éventail de LA famille, le reste arme.
							const NkRect zc = {c.x + c.w - 12.f, c.y + c.h - 12.f, 12.f, 12.f};
							if (kFamEventail[i] && NkGuiRectContains(zc, ctx.input.mousePos))
								mEventailFam = (mEventailFam == (int32)i) ? -1 : (int32)i;
							else if (kFamAgit[i])
								ArmerOutil(i);
							else
								DireRaisonFamille(i);
						}
						nkentseu::editorkit::NkTooltip(ctx, survol, kOutilsBulles[i]);
						if (i == mOutil)
							dl.AddRectFilled(c, ctx.theme.accent, 3.f);
						// une famille grisée se voit : glyphe atténué de moitié
						NkColor g = (i == mOutil) ? ctx.theme.onAccent : ctx.theme.textMuted;
						if (!kFamAgit[i])
							g.a = (nkentseu::uint8)(g.a / 2);
						const float32 ix = c.x + (c.w - 13.f) * 0.5f;
						const float32 iy = c.y + (c.h - 13.f) * 0.5f;
						switch (i) {
							case 0: // la face = la variante (Sélection / Main)
								if (mVarMove == 1)
									costume::OutilMain(dl, ix, iy, g);
								else
									costume::OutilFleche(dl, ix, iy, g);
								break;
							case 1:
								costume::OutilCadre(dl, ix, iy, g);
								break;
							case 2: // la face = la variante de FORMES
								if (mVariante == 2)
									costume::OutilEllipse(dl, ix, iy, g);
								else if (mVariante >= 1)
									GlypheForme(dl, c, mVariante, g);
								else
									costume::OutilRect(dl, ix, iy, g);
								break;
							case 3:
								GlypheLigne(dl, c, mVarLigne, g);
								break;
							case 4:
								costume::OutilConnecteur(dl, ix, iy, g);
								break;
							case 5:
								costume::OutilTexte(dl, ix, iy, g);
								break;
							case 6:
								costume::OutilPlume(dl, ix, iy, g);
								break;
							case 7:
								if (mVarImage == 1)
									costume::OutilAvatar(dl, ix, iy, g);
								else
									costume::OutilImage(dl, ix, iy, g);
								break;
							default:
								costume::OutilIcone(dl, ix, iy, g);
								break;
						}
						if (kFamEventail[i])
							costume::ChevronVariante4(dl, c.x + c.w - 6.f, c.y + c.h - 6.f, g);
					}

					// ── L'ÉVENTAIL DE **LA** FAMILLE OUVERTE (§7.1, Lunacy) ────
					// Déplié « vers le canvas », À DROITE de la rangée de sa
					// famille — chaque famille montre son éventail COMPLET
					// (panneaux 3-4-6-7-8-9-10 des références) ; une variante qui
					// n'agit pas est GRISÉE et son clic DIT pourquoi.
					if (mEventailFam >= 0) {
						struct VarDecl {
								const char *id;
								const char *bulle;
								bool agit;
						};
						static const VarDecl kVarMove[4] = {
							{"##var_selection", "Sélection (V)", true},
							{"##var_main", "Main (H) — glisser déplace la vue", true},
							{"##var_echelle", "Échelle — chantier nommé", false},
							{"##var_editionpts", "Édition de points — chantier nommé", false}};
						static const VarDecl kVarCadre[3] = {
							{"##var_cadre", "Cadre (F)", true},
							{"##var_tranche", "Tranche (export) — chantier nommé", false},
							{"##var_crayoncadre", "Crayon de cadre — chantier nommé", false}};
						static const VarDecl kVarFormes[6] = {
							{"##var_rect", "Rectangle (R)", true},
							{"##var_arrondi", "Rectangle arrondi", true},
							{"##var_ellipse", "Ellipse (O)", true},
							{"##var_triangle", "Triangle", true},
							{"##var_pentagone", "Pentagone", true},
							{"##var_etoile", "Étoile", true}};
						static const VarDecl kVarLigne[2] = {{"##var_ligne", "Ligne (L)", true},
															 {"##var_fleche", "Flèche", true}};
						static const VarDecl kVarPlume[2] = {
							{"##var_plume", "Plume (P) — chemins libres, chantier nommé", false},
							{"##var_crayon", "Crayon — chantier nommé", false}};
						static const VarDecl kVarImage[2] = {
							{"##var_image", "Image (M) — cadre d'image", true},
							{"##var_avatar", "Avatar — pastille de profil", true}};
						const VarDecl *vars = nullptr;
						uint32 nVars = 0;
						switch (mEventailFam) {
							case 0: vars = kVarMove; nVars = 4; break;
							case 1: vars = kVarCadre; nVars = 3; break;
							case 2: vars = kVarFormes; nVars = 6; break;
							case 3: vars = kVarLigne; nVars = 2; break;
							case 6: vars = kVarPlume; nVars = 2; break;
							case 7: vars = kVarImage; nVars = 2; break;
							default: break;
						}
						const float32 yFam = r.y + 6.f + hb * (float32)mEventailFam
											 + ((mEventailFam >= 5) ? filet : 0.f);
						const NkRect bFam = {r.x, yFam, w, hb};
						const float32 vb = 36.f;
						const NkRect ev = {r.x + w + 6.f, bFam.y,
										   (vb + 4.f) * (float32)nVars + 12.f, vb + 8.f};
						mZoneEventail = ev;
						dl.AddRectFilled(ev, fond, 6.f);
						dl.AddRect(ev, bord, 1.f, 6.f);
						const uint32 varActive = (mEventailFam == 0)   ? mVarMove
												 : (mEventailFam == 2) ? mVariante
												 : (mEventailFam == 3) ? mVarLigne
												 : (mEventailFam == 7) ? mVarImage
																	   : 0u;
						for (uint32 v = 0; v < nVars; ++v) {
							const NkRect cv = {ev.x + 4.f + (vb + 4.f) * (float32)v, ev.y + 4.f,
											   vb, vb};
							ctx.SetNextItemRect(cv);
							if (Button(ctx, vars[v].id)) {
								if (!vars[v].agit) {
									// grisée-qui-le-dit : la raison part au pied
									Dire("", vars[v].bulle, ".");
								} else {
									if (mEventailFam == 0)
										mVarMove = v;
									else if (mEventailFam == 2)
										mVariante = v;
									else if (mEventailFam == 3)
										mVarLigne = v;
									else if (mEventailFam == 7)
										mVarImage = v;
									ArmerOutil((uint32)mEventailFam);
									mEventailFam = -1;
								}
							}
							if (ctx.IsItemHovered())
								SetTooltip(ctx, vars[v].bulle);
							if (v == varActive && vars[v].agit)
								dl.AddRectFilled(cv, ctx.theme.accent, 4.f);
							NkColor gv = ctx.theme.text;
							if (!vars[v].agit)
								gv.a = (nkentseu::uint8)(gv.a / 2);
							const float32 gx = cv.x + (cv.w - 13.f) * 0.5f;
							const float32 gy = cv.y + (cv.h - 13.f) * 0.5f;
							switch (mEventailFam) {
								case 0:
									if (v == 0)
										costume::OutilFleche(dl, gx, gy, gv);
									else if (v == 1)
										costume::OutilMain(dl, gx, gy, gv);
									else if (v == 2)
										costume::OutilCadre(dl, gx, gy, gv); // échelle : cadre fléché
									else
										costume::OutilCrayon(dl, gx, gy, gv);
									break;
								case 1:
									if (v == 0)
										costume::OutilCadre(dl, gx, gy, gv);
									else if (v == 1)
										costume::OutilTranche(dl, gx, gy, gv);
									else
										costume::OutilCrayon(dl, gx, gy, gv);
									break;
								case 2:
									GlypheForme(dl, cv, v, gv);
									break;
								case 3:
									GlypheLigne(dl, cv, v, gv);
									break;
								case 6:
									if (v == 0)
										costume::OutilPlume(dl, gx, gy, gv);
									else
										costume::OutilCrayon(dl, gx, gy, gv);
									break;
								case 7:
									if (v == 0)
										costume::OutilImage(dl, gx, gy, gv);
									else
										costume::OutilAvatar(dl, gx, gy, gv);
									break;
								default:
									break;
							}
						}
						// Un clic hors de l'éventail et hors de sa famille le
						// referme — le comportement de tout menu volant.
						if (ctx.input.mouseClicked[0]
							&& !NkGuiRectContains(ev, ctx.input.mousePos)
							&& !NkGuiRectContains(bFam, ctx.input.mousePos))
							mEventailFam = -1;
					} else {
						mZoneEventail = {0.f, 0.f, 0.f, 0.f};
					}
				}

				// ── 3. LE CLUSTER DE TOILE, en BAS À DROITE **DE LA TOILE** ──
				//    ⚠️ DE LA TOILE, PAS DE LA FENÊTRE. Le « Zoom 107 % » du pied
				//       de fenêtre est autre chose : il appartient à la coquille.
				//       Ce cluster-ci appartient au canvas et le suit — ce sont
				//       des réglages de VUE, pas des outils (§7).
				{
					char zoom[32];
					// ⚠️ PAS DE « \u25be » ICI. Mesure sur capture : le champ affichait
					//    « 100 % ? ». Le point de code U+25BE (petit triangle bas)
					//    n'est PAS dans l'atlas de la police chargee -- 1381 glyphes
					//    pour Inter, et celui-la n'y est pas. Un caractere absent ne
					//    se voit pas comme absent, il se voit comme une faute.
					//    Le chevron est donc DESSINE, quelques lignes plus bas.
					// COSTUME BANANI EXACT (JSX DesignCanvasV2) : bas 12 / droite
					// 12, padding 3-5, « 100% » 11 px + chevron TRACÉ 7x5, filet
					// vertical, grille 22x22 au bord ACCENT (la grille de points
					// EST affichée : l'état est vrai), aimant 22x22 au bord
					// `border`. Clics pris à la main — le Button du socle
					// repeindrait son fond opaque sur le costume.
					auto &F = costume::Fontes();
					snprintf(zoom, sizeof(zoom), "%d%%",
							 (int32)(mSt->view.zoom * 100.f + 0.5f));
					const float32 wz = costume::Largeur(F.px11, zoom);
					// [5][zoom][4][chevron 7][8][filet 1][4][22][4][22][5]
					const float32 w =
						5.f + wz + 4.f + 7.f + 8.f + 1.f + 4.f + 22.f + 4.f + 22.f + 5.f;
					const float32 h = 28.f;
					const NkRect r = {zone.x + zone.w - w - 12.f, zone.y + zone.h - h - 12.f, w,
									  h};
					mZoneCluster = r; // idem
					dl.AddRectFilled({r.x - 1.f, r.y + 2.f, r.w + 2.f, r.h + 4.f}, {0, 0, 0, 50},
									 8.f); // ombre 0 2 8 approchée
					dl.AddRectFilled(r, fond, 4.f);
					dl.AddRect(r, bord, 1.f, 4.f);
					float32 x = r.x + 5.f;
					costume::Texte(dl, F.px11, x, costume::CentrerY(F.px11, r.y, h), zoom,
								   ctx.theme.text);
					costume::ChevronCombo7(dl, x + wz + 4.f, r.y + (h - 5.f) * 0.5f,
										   ctx.theme.textMuted);
					const NkRect rz = {r.x, r.y, 5.f + wz + 4.f + 7.f + 8.f, h};
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(rz, ctx.input.mousePos))
						Dire("Zoom : la saisie directe n'est pas encore branchée.", "", "");
					x += wz + 4.f + 7.f + 8.f;
					dl.AddLine({x, r.y + 5.f}, {x, r.y + h - 5.f}, bord, 1.f);
					x += 1.f + 4.f;
					const NkRect rg = {x, r.y + 3.f, 22.f, 22.f};
					dl.AddRect(rg, ctx.theme.accent, 1.f, 3.f);
					costume::IcGrille(dl, rg.x + 5.f, rg.y + 5.f, ctx.theme.accent);
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(rg, ctx.input.mousePos))
						Dire("Grille : à brancher.", "", "");
					x += 22.f + 4.f;
					// L'AIMANT EST BRANCHÉ (01/09) — et il DIT son état. Le bord
					// et l'encre suivent `aimantActif` : accent quand il agit,
					// atténué quand il dort, comme la grille juste à gauche.
					// Un bouton dont l'apparence ne bouge pas est un bouton dont
					// on ne sait jamais s'il a pris le clic.
					const NkRect rm = {x, r.y + 3.f, 22.f, 22.f};
					dl.AddRect(rm, mSt->aimantActif ? ctx.theme.accent : bord, 1.f, 3.f);
					costume::IcAimant(dl, rm.x + 5.f, rm.y + 5.f,
									  mSt->aimantActif ? ctx.theme.accent : ctx.theme.textMuted);
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(rm, ctx.input.mousePos)) {
						mSt->aimantActif = !mSt->aimantActif;
						if (!mSt->aimantActif)
							mSt->snapVif = NkSnapResultat();
						Dire(mSt->aimantActif ? "Magnétisme activé — bords, centres et "
												"espacements égaux."
											  : "Magnétisme désactivé.",
							 "", "");
					}
				}
			}

			// ── LES GLYPHES DE LA BARRE D'OUTILS (planche 22.0) ──────────────
			// Vectoriels, dans le vocabulaire de la liste de dessin (triangle,
			// ligne, rectangle, cercle) — le choix d'Icons.h, jamais un bitmap ni
			// un caractère qui peut manquer à l'atlas. Ils disparaîtront au profit
			// de l'atlas NKGui le jour où il arrive, comme le chevron.
			/// Le chevron de VARIANTES en bas-droite d'une famille (planche :
			/// formes et plume en portent un). GRISÉ tant que la famille n'a
			/// qu'un outil — le signe est posé, le menu fantôme non.
			static void ChevronVariante(nkgui::NkGuiDrawList &dl, const NkRect &c,
										const nkgui::NkColor &mut) {
				const float32 bx = c.x + c.w - 7.f, by = c.y + c.h - 7.f;
				dl.AddTriangleFilled({bx - 3.f, by - 2.f}, {bx + 3.f, by - 2.f}, {bx, by + 2.f},
									 mut);
			}
			/// Le glyphe d'UNE VARIANTE de la famille Formes (face du bouton ET
			/// éventail) — l'ÉVENTAIL LUNACY COMPLET (panneau 8) : 0 rectangle,
			/// 1 arrondi, 2 ellipse, 3 triangle, 4 pentagone, 5 étoile. La ligne
			/// et la flèche sont désormais la famille LIGNE (panneau 7).
			static void GlypheForme(nkgui::NkGuiDrawList &dl, const NkRect &c, uint32 v,
									const nkgui::NkColor &enc) {
				const float32 cx = c.x + c.w * 0.5f, cy = c.y + c.h * 0.5f;
				const float32 s = 7.f;
				if (v == 1) // rectangle ARRONDI (rect à rayon posé à la création)
					dl.AddRect({cx - s, cy - s * 0.72f, s * 2.f, s * 1.44f}, enc, 1.6f, 5.f);
				else if (v == 2)
					dl.AddCircle({cx, cy}, s * 0.85f, enc, 1.6f);
				else if (v == 3) { // triangle (vague Lunacy (b), 31/08)
					const nkgui::NkVec2 p[4] = {{cx, cy - s},
												{cx + s, cy + s * 0.8f},
												{cx - s, cy + s * 0.8f},
												{cx, cy - s}};
					dl.AddPolyline(p, 4, enc, 1.6f);
				} else if (v == 4) { // pentagone
					const nkgui::NkVec2 p[6] = {{cx, cy - s},
												{cx + s * 0.95f, cy - s * 0.31f},
												{cx + s * 0.59f, cy + s * 0.81f},
												{cx - s * 0.59f, cy + s * 0.81f},
												{cx - s * 0.95f, cy - s * 0.31f},
												{cx, cy - s}};
					dl.AddPolyline(p, 6, enc, 1.6f);
				} else if (v == 5) { // étoile (5 branches)
					const nkgui::NkVec2 p[11] = {
						{cx, cy - s},
						{cx + s * 0.22f, cy - s * 0.31f},
						{cx + s * 0.95f, cy - s * 0.31f},
						{cx + s * 0.36f, cy + s * 0.12f},
						{cx + s * 0.59f, cy + s * 0.81f},
						{cx, cy + s * 0.38f},
						{cx - s * 0.59f, cy + s * 0.81f},
						{cx - s * 0.36f, cy + s * 0.12f},
						{cx - s * 0.95f, cy - s * 0.31f},
						{cx - s * 0.22f, cy - s * 0.31f},
						{cx, cy - s}};
					dl.AddPolyline(p, 11, enc, 1.4f);
				} else
					dl.AddRect({cx - s, cy - s * 0.72f, s * 2.f, s * 1.44f}, enc, 1.6f, 3.f);
			}
			/// Le glyphe d'UNE VARIANTE de la famille LIGNE (panneau 7 Lunacy) :
			/// 0 ligne, 1 flèche.
			static void GlypheLigne(nkgui::NkGuiDrawList &dl, const NkRect &c, uint32 v,
									const nkgui::NkColor &enc) {
				const float32 cx = c.x + c.w * 0.5f, cy = c.y + c.h * 0.5f;
				const float32 s = 7.f;
				if (v == 1) { // flèche (diagonale montante, pointe pleine — Lunacy)
					dl.AddLine({cx - s, cy + s * 0.7f}, {cx + s * 0.35f, cy - s * 0.25f}, enc, 2.f);
					dl.AddTriangleFilled({cx + s, cy - s * 0.7f}, {cx + s * 0.15f, cy - s * 0.6f},
										 {cx + s * 0.55f, cy + s * 0.05f}, enc);
				} else
					dl.AddLine({cx - s, cy + s * 0.7f}, {cx + s, cy - s * 0.7f}, enc, 2.f);
			}

			void GlypheOutil(nkgui::NkGuiDrawList &dl, const NkRect &c, uint32 outil,
							 const nkgui::NkColor &enc, const nkgui::NkColor &mut) {
				const float32 cx = c.x + c.w * 0.5f, cy = c.y + c.h * 0.5f;
				const float32 s = 7.f; // demi-côté du glyphe
				switch (outil) {
					case 0: { // SÉLECTION : la flèche de curseur, pointe haut-gauche
						const float32 ax = cx - s * 0.55f, ay = cy - s;
						dl.AddTriangleFilled({ax, ay}, {ax, ay + s * 1.7f},
											 {ax + s * 1.2f, ay + s * 1.15f}, enc);
						dl.AddLine({cx + s * 0.05f, cy + s * 0.15f},
								   {cx + s * 0.65f, cy + s}, enc, 2.2f);
						break;
					}
					case 1: // CADRE : le carré
						dl.AddRect({cx - s, cy - s, s * 2.f, s * 2.f}, enc, 1.6f);
						break;
					case 2: // FORMES : la face = la DERNIÈRE variante choisie (§7.1)
						GlypheForme(dl, c, mVariante, enc);
						// Le chevron est VIVANT (il ouvre l'éventail) : couleur
						// pleine, plus le gris du provisoire.
						ChevronVariante(dl, c, enc);
						break;
					case 3: { // VECTORIEL : la plume (pointe en bas), et ses variantes
						dl.AddTriangleFilled({cx - s * 0.7f, cy - s * 0.35f},
											 {cx + s * 0.7f, cy - s * 0.35f}, {cx, cy + s}, enc);
						dl.AddLine({cx - s * 0.7f, cy - s * 0.35f}, {cx, cy - s}, enc, 1.6f);
						dl.AddLine({cx + s * 0.7f, cy - s * 0.35f}, {cx, cy - s}, enc, 1.6f);
						ChevronVariante(dl, c, mut);
						break;
					}
					case 4: // TEXTE : le T, en deux traits — aucun glyphe de police
						dl.AddLine({cx - s * 0.9f, cy - s + 1.f}, {cx + s * 0.9f, cy - s + 1.f},
								   enc, 2.f);
						dl.AddLine({cx, cy - s + 1.f}, {cx, cy + s}, enc, 2.f);
						break;
					case 5: // MÉDIA : le cadre d'image, son soleil, sa montagne
						dl.AddRect({cx - s, cy - s * 0.8f, s * 2.f, s * 1.6f}, enc, 1.4f);
						dl.AddCircleFilled({cx - s * 0.35f, cy - s * 0.3f}, 1.6f, enc);
						dl.AddLine({cx - s + 1.5f, cy + s * 0.65f}, {cx + s * 0.05f, cy - s * 0.05f},
								   enc, 1.4f);
						dl.AddLine({cx + s * 0.05f, cy - s * 0.05f},
								   {cx + s - 1.5f, cy + s * 0.65f}, enc, 1.4f);
						break;
					case 6: // MESURE : la règle et ses graduations
						dl.AddRect({cx - s, cy - s * 0.45f, s * 2.f, s * 0.9f}, enc, 1.4f);
						for (uint32 g = 0; g < 3; ++g) {
							const float32 gx = cx - s * 0.5f + s * 0.5f * (float32)g;
							dl.AddLine({gx, cy - s * 0.45f}, {gx, cy}, enc, 1.2f);
						}
						break;
					default:
						break;
				}
			}

			/// L'aide contextuelle de l'outil — LE texte qui dit quoi faire
			/// (mesure du 30/08 : « F puis glisser » n'était pas compréhensible ;
			/// l'application ne disait nulle part quel outil est armé ni quoi
			/// faire). Elle part au pied de fenêtre à chaque armement.
			const char *AideOutil(uint32 i) const {
				switch (i) {
					case 0:
						return mVarMove == 1
								   ? "Main — glisser déplace la vue (pan) ; V = retour Sélection"
								   : "Sélection — cliquer ; glisser = déplacer ; bords = "
									 "redimensionner";
					// ⚠️ CHAQUE OUTIL DE TRACÉ NOMME SES DEUX MODIFICATEURS. « Alt =
					//    depuis le centre » a été ajouté le 01/09, et **rien à
					//    l'écran ne l'annoncerait** : personne ne maintient une
					//    touche au hasard pour voir. C'est la même règle que le
					//    mode édition de forme, qui énumère ses trois gestes —
					//    *une capacité qu'aucune phrase ne nomme est une capacité
					//    que l'utilisateur n'a pas.*
					case 1:
						return "Cadre — cliquez-glissez pour tracer ; Maj = carré ; "
							   "Alt = depuis le centre ; Échap = annuler";
					case 2:
						switch (mVariante) {
							case 1:
								return "Rectangle arrondi — cliquez-glissez ; Maj = carré ; "
									   "Alt = depuis le centre ; Échap = annuler";
							case 2:
								return "Ellipse — cliquez-glissez ; Maj = cercle ; "
									   "Alt = depuis le centre ; Échap = annuler";
							case 3:
								return "Triangle — cliquez-glissez ; Maj = contraint ; "
									   "Alt = depuis le centre ; Échap = annuler";
							case 4:
								return "Pentagone — cliquez-glissez ; Maj = contraint ; "
									   "Alt = depuis le centre ; Échap = annuler";
							case 5:
								return "Étoile — cliquez-glissez ; Maj = contraint ; "
									   "Alt = depuis le centre ; Échap = annuler";
							default:
								return "Rectangle — cliquez-glissez ; Maj = carré ; "
									   "Alt = depuis le centre ; Échap = annuler";
						}
					case 3:
						return mVarLigne == 1
								   ? "Flèche — cliquez-glissez ; Maj = 0°/45°/90° ; "
									 "Alt = depuis le centre ; Échap = annuler"
								   : "Ligne — cliquez-glissez ; Maj = 0°/45°/90° ; "
									 "Alt = depuis le centre ; Échap = annuler";
					case 5:
						return "Texte — cliquez pour poser ; contenu dans l'Inspecteur "
							   "(Typographie)";
					case 7:
						return mVarImage == 1
								   ? "Avatar — cliquez-glissez pour poser la pastille de profil"
								   : "Image — cliquez-glissez pour poser le cadre d'image (la "
									 "source arrive avec la bibliothèque de médias)";
					default:
						return "";
				}
			}

			/// La RAISON d'une famille qui n'agit pas encore — grisée-qui-le-dit,
			/// jamais un clic muet (la règle des menus).
			void DireRaisonFamille(uint32 i) {
				if (i == 4)
					Dire("Connecteur : le nodal vient PAR-DESSUS le modèle (règle du dépôt) "
						 "— chantier nommé, rien n'est armé.",
						 "", "");
				else if (i == 6)
					Dire("Plume : les chemins libres demandent un vocabulaire de forme à "
						 "points (§4.2) — chantier nommé, rien n'est armé.",
						 "", "");
				else
					Dire("Icône : la bibliothèque d'icônes n'existe pas encore — chantier "
						 "nommé, rien n'est armé.",
						 "", "");
			}

			/// Armer un outil : l'icône s'allume, la ligne d'aide change — jamais
			/// de no-op muet. Un tracé en cours est abandonné (changer d'outil au
			/// milieu d'un geste est un renoncement, pas une erreur).
			void ArmerOutil(uint32 i) {
				mOutil = i;
				mCreating = false;
				Dire("", AideOutil(i), "");
			}

			/// Le rectangle du tracé en cours, CONTRAINT par Maj (Lunacy : carré /
			/// cercle ; pour la ligne : horizontale, verticale ou 45°). Une seule
			/// source pour l'aperçu ET le relâchement — deux calculs auraient
			/// divergé à la première retouche.
			/// ⚠️ LE CALCUL A DÉMÉNAGÉ DANS `SelectionGeste.h` (`NkRectDeTrace`) : il
			///    vivait ici, donc hors de portée d'un banc — la facture que ce
			///    chantier a déjà payée sur le zoom (Q41) et sur l'aimantation
			///    (Q42). Cette méthode n'est plus qu'un raccord vers l'état du
			///    panneau, et c'est tout ce qu'elle doit être.
			/// Le parent place-t-il ses enfants LIBREMENT ? C est la condition pour
			/// que `posX`/`posY` aient un effet -- et donc pour qu on puisse recadrer
			/// la boite d une forme sur son trace.
			/// (`Anchor` compte comme `Free` : un enfant ancre est une forme posee
			///  dont seul le calcul de position differe -- meme regle que le peintre.)
			bool ParentLibre(int32 i) const {
				if (!mSt->doc.IsValidIndex(i))
					return false;
				const int32 pa = mSt->doc.nodes[(uint32)i].parent;
				if (!mSt->doc.IsValidIndex(pa))
					return false;
				const NkLayoutKind k = mSt->doc.nodes[(uint32)pa].layout.kind;
				return k == NkLayoutKind::Free || k == NkLayoutKind::Anchor;
			}

			NkPaintRect RectTrace(float32 mx, float32 my, bool shift, bool depuisCentre) const {
				return NkRectDeTrace(mCreateX, mCreateY, mx, my, shift, depuisCentre,
									 mOutil == 3);
			}

			/// NK_ENTREE_TRACE=1 : journal du VRAI chemin clavier (append dans
			/// nkuidesign_trace_entree.txt). Silencieux par défaut — c'est
			/// l'instrument du diagnostic collaboratif : Rodolf appuie, on lit.
			void TraceEntree(NkGuiContext &ctx) {
				static int32 actif = -1;
				if (actif < 0) {
					const char *e = getenv("NK_ENTREE_TRACE");
					actif = (e && *e == '1') ? 1 : 0;
				}
				if (!actif)
					return;
				const bool echap = ctx.input.KeyPressed(NkGuiKey::Escape);
				if (ctx.input.charCount == 0 && !echap)
					return;
				FILE *f = fopen("nkuidesign_trace_entree.txt", "ab");
				if (!f)
					return;
				char b[256];
				int32 n = snprintf(b, sizeof(b), "chars=");
				for (int32 k = 0; k < ctx.input.charCount && n < 200; ++k) {
					const uint32 cp = ctx.input.chars[k];
					n += snprintf(b + n, sizeof(b) - (size_t)n, "%c",
								  (cp >= 32 && cp < 127) ? (char)cp : '?');
				}
				n += snprintf(b + n, sizeof(b) - (size_t)n,
							  " focusChamp=%s outil=%u variante=%u trace=%d echap=%d\n",
							  ctx.inputId == NKGUI_ID_NONE ? "aucun" : "OUI", mOutil, mVariante,
							  (int32)mCreating, (int32)echap);
				fwrite(b, 1, (size_t)n, f);
				fclose(f);
			}

			/// La puce « L × H » sous un rectangle d'écran (Lunacy). `w`/`h` sont
			/// des longueurs DOCUMENT, arrondies à l'entier — le fichier n'écrira
			/// rien d'autre.
			/// La puce de taille d'une MULTI-selection : elle dit aussi COMBIEN.
			/// ⚠️ Sans le compte, « 240 x 170 » ferait croire qu'un seul objet fait
			///    cette taille — alors que c'est la boite qui les contient tous.
			void PuceTailleN(NkDesignPaint &paint, const NkPaintRect &rs, float32 w, float32 h,
							 nkentseu::uint32 n) {
				char t[64];
				snprintf(t, sizeof(t), "%d x %d  (%u)", (int32)(w + 0.5f), (int32)(h + 0.5f), n);
				const float32 lw = 16.f + costume::Largeur(costume::Fontes().px9, t);
				const NkPaintRect pb{rs.x + rs.w * 0.5f - lw * 0.5f, rs.y + rs.h + 6.f, lw, 18.f};
				const uint16 accent = NkDesignResolveRole("accent_ui");
				paint.Fill(pb, accent, 4.f);
				paint.TextHex(pb, t, 0xFFFFFFFFu, accent, editorkit::NkTextAlign::Center,
							  costume::CorpsMaquette(9.f), 600.f);
			}
			void PuceTaille(NkDesignPaint &paint, const NkPaintRect &rs, float32 w, float32 h) {
				char t[48];
				snprintf(t, sizeof(t), "%d × %d", (int32)(w + 0.5f), (int32)(h + 0.5f));
				const float32 tw = paint.TextWidth(t) + 12.f;
				const float32 th = 18.f;
				const NkPaintRect pr{rs.x + (rs.w - tw) * 0.5f, rs.y + rs.h + 6.f, tw, th};
				paint.Fill(pr, NkDesignResolveRole("accent_ui"), 4.f);
				paint.Text(pr, t, NkDesignResolveRole("text_on_accent"),
						   nkentseu::editorkit::NkTextAlign::Center);
			}

			void Dire(const char *a, const char *b, const char *c) {
				NkString t(a);
				t.Append(b);
				t.Append(c);
				// Le message va AU PIED DE FENETRE (le rail bas fusionne) — c'est
				// la ligne d'aide que Rodolf ne trouvait pas (« F puis glisser
				// n'etait pas comprehensible ») : elle vit desormais dans le seul
				// bandeau bas, a droite des pastilles.
				mSt->DireAuPied(t.Data());
			}

			/// La geometrie de la barre d'outils flottante (§7 : « largeur 48px »).
			/// ⚠️ LUE PAR DEUX ENDROITS -- la barre elle-meme, et la pose initiale de
			///    la vue, qui doit garantir qu'aucun contenu ne naisse dessous.
			/// Banani (FloatingToolRail) : 36 px — plus la barre de 48 du plan
			/// initial ; la maquette est la reference exacte (Rodolf, 30/08).
			static constexpr float32 kOutilsLargeur = 36.f;
			static constexpr float32 kOutilsMarge = 10.f; // Banani : left 10 px
			/// Le pas de la grille a points, en espace DOCUMENT — 20, la valeur
			/// exacte de la maquette Banani (radial-gradient, pas 20 px).
			static constexpr float32 kGrillePas = 20.f;

			uint32 mMode = 0;  ///< Design / Behavior / Animation / Split
			uint32 mOutil = 0; ///< famille d'outils active
			bool mVuePosee = false; ///< la vue a-t-elle recu sa position de depart ?
			bool mPanning = false;
			bool mMarquee = false;
			float32 mMarqX = 0.f, mMarqY = 0.f;
			float32 mPanX = 0.f, mPanY = 0.f;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 4 — LES PROPRIETES
	// ═══════════════════════════════════════════════════════════════════════════
	// ⚠️ AUCUN WIDGET DE COMPOSANT N'EST ECRIT EN DUR ICI. La section « reglages »
	//    BOUCLE sur les tables de la declaration : ajouter un parametre au
	//    composant le fait apparaitre SANS TOUCHER A CE FICHIER. C'est ce qui
	//    distingue « lire la declaration » de « connaitre le composant » — un
	//    panneau ecrit a la main afficherait les memes curseurs aujourd'hui et
	//    mentirait des le premier ajout.
	class PropertiesPanel : public NkEditorPanel {
		public:
			explicit PropertiesPanel(DesignState *st)
				: NkEditorPanel("Propriétés", NkEditorDockSide::NK_RIGHT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				if (!mSt->doc.IsValidIndex(mSt->selected)) {
					ec.Text("Aucun nœud sélectionné.");
					return;
				}
				NkUINode &n = mSt->doc.nodes[(uint32)mSt->selected];

				ec.Text(n.label.Data());
				ec.Separator();
				if (designkit::Section(ctx, "Provenance"))
					DrawProvenance(ec, ctx, n);
				if (designkit::Section(ctx, "Largeur"))
					DrawSizing(ec, ctx, n.width);
				if (designkit::Section(ctx, "Hauteur"))
					DrawSizing(ec, ctx, n.height);
				if (designkit::Section(ctx, "Agencement de ses enfants"))
					DrawLayout(ec, ctx, n);
				if (designkit::Section(ctx, "Réglages du composant"))
					DrawComponentSettings(ec, ctx, n);
			}

		private:
			void Edited() {
				mSt->doc.MarkHumanEdit(mSt->selected);
			}

			void DrawProvenance(NkEditorFrameContext &ec, NkGuiContext &ctx, const NkUINode &n) {
				(void)ec;
				char b[192];
				snprintf(b, sizeof(b), "%s%s%s", NkAuthorName(n.prov.author),
						 n.prov.verified ? " · rejouée" : "", n.prov.corrected ? " · corrigée" : "");
				designkit::KeyValue(ctx, "auteur", b);
				if (n.prov.origin.Length() > 0)
					designkit::KeyValue(ctx, "origine", n.prov.origin.Data());
				// ⚠️ AUCUNE CASE A COCHER ICI, ET C'EST DELIBERE. « rejouee » est un
				//    constat de mesure, « corrigee » se deduit d'une edition : les
				//    rendre cochables ferait entrer dans le corpus des tampons poses a
				//    la main, c'est-a-dire du bruit qui ressemble a du signal.
			}

			// ⚠️ LES CINQ MODES ETAIENT CINQ `Selectable` EMPILES -- c'est ce que
			//    Rodolf a vu comme « une liste de valeurs cliquables ». Cinq lignes
			//    pour UN reglage, et rien ne disait qu'elles s'excluaient. Un
			//    controle segmente le dit par sa forme : une ligne, N cases, une
			//    seule allumee.
			void DrawSizing(NkEditorFrameContext &ec, NkGuiContext &ctx, NkSizeDecl &s) {
				(void)ec;
				const char *modes[(uint32)NkSizeMode::Count];
				for (uint8 i = 0; i < (uint8)NkSizeMode::Count; ++i)
					modes[i] = NkSizeModeName((NkSizeMode)i);
				const int32 pick =
					designkit::Segmented(ctx, modes, (int32)NkSizeMode::Count, (int32)s.mode);
				if (pick >= 0) {
					s.mode = (NkSizeMode)pick;
					Edited();
				}
				if (s.mode == NkSizeMode::Fixed) {
					if (nkgui::DragFloat(ctx, "taille (px)", s.value, 1.f, 0.f, 1200.f))
						Edited();
				} else if (s.mode == NkSizeMode::Weight) {
					if (nkgui::DragFloat(ctx, "poids", s.value, 0.05f, 0.f, 8.f))
						Edited();
				}
				// ⚠️ « max (<= min : non borne) » S'AFFICHAIT « max (<= mir… ».
				//    Le libelle porte maintenant le nom, et la convention est dite
				//    UNE fois sous les deux champs -- une explication repetee dans
				//    un libelle est une explication qui sera coupee.
				static const float32 kPoids[2] = {-1.f, -1.f};
				nkgui::BeginRow(ctx, 0.f, kPoids, 2);
				if (nkgui::DragFloat(ctx, "min", s.minVal, 1.f, 0.f, 800.f))
					Edited();
				if (nkgui::DragFloat(ctx, "max", s.maxVal, 1.f, 0.f, 1600.f))
					Edited();
				nkgui::EndRow(ctx);
				nkgui::TextWrapped(ctx, "max <= min : non borne.");
			}

			void DrawLayout(NkEditorFrameContext &ec, NkGuiContext &ctx, NkUINode &n) {
				(void)ec;
				const char *kinds[(uint32)NkLayoutKind::Count];
				for (uint8 i = 0; i < (uint8)NkLayoutKind::Count; ++i)
					kinds[i] = NkLayoutKindName((NkLayoutKind)i);
				const int32 pick =
					designkit::Segmented(ctx, kinds, (int32)NkLayoutKind::Count, (int32)n.layout.kind);
				if (pick >= 0) {
					n.layout.kind = (NkLayoutKind)pick;
					Edited();
				}
				// ⚠️ NI GOUTTIERE NI MARGE EN NOMBRE ICI, ET C'EST LA REGLE DU KIT :
				//    un espacement est du STYLE, il se NOMME. Le noeud designe deux
				//    metriques ; leurs VALEURS s'editent une fois pour tout le
				//    document, dans le panneau Composition. Mettre un curseur de
				//    pixels ici ferait exister la valeur a deux endroits, et l'editeur
				//    n'en changerait qu'un.
				char nm[160];
				snprintf(nm, sizeof(nm), "%s (%0.1f px)", n.spacingName.Data(),
						 mSt->doc.Metric(n.spacingName.Data()));
				designkit::KeyValue(ctx, "espacement", nm);
				snprintf(nm, sizeof(nm), "%s (%0.1f px)", n.padName.Data(),
						 mSt->doc.Metric(n.padName.Data()));
				designkit::KeyValue(ctx, "remplissage", nm);
				if (n.layout.kind == NkLayoutKind::Grid) {
					float32 cols = (float32)n.layout.gridColumns;
					if (ec.SliderFloat("colonnes", cols, 1.f, 8.f)) {
						n.layout.gridColumns = (uint16)(cols + 0.5f);
						Edited();
					}
				}
				ec.Text("Alignement transverse");
				const char *aligns[(uint32)NkAlign::Count];
				for (uint8 i = 0; i < (uint8)NkAlign::Count; ++i)
					aligns[i] = NkAlignName((NkAlign)i);
				const int32 pickA =
					designkit::Segmented(ctx, aligns, (int32)NkAlign::Count, (int32)n.layout.crossAlign);
				if (pickA >= 0) {
					n.layout.crossAlign = (NkAlign)pickA;
					Edited();
				}
			}

			void DrawComponentSettings(NkEditorFrameContext &ec, NkGuiContext &ctx, NkUINode &n) {
				const NkComponentDecl *d = n.IsFrame() ? nullptr : n.instance.Decl();
				if (!d) {
					ec.Text("Un cadre n'a pas de réglages : il agence.");
					return;
				}
				ec.Text("Representation");
				for (uint16 i = 0; i < d->variantCount; ++i) {
					const bool cur = n.instance.HasVariant() && n.instance.Variant() == (int32)i;
					if (Selectable(ctx, d->variants[i].label, cur)) {
						n.instance.SetVariant((int32)i);
						Edited();
					}
				}

				ec.Separator();
				ec.Text("Parametres");
				for (uint16 i = 0; i < d->paramCount; ++i) {
					const NkParamDecl &pd = d->params[i];
					float32 v = n.instance.Param(pd.name);
					if (pd.kind == NkParamKind::Bool) {
						bool b = v > 0.5f;
						if (ec.Checkbox(pd.label, b)) {
							n.instance.SetParam(pd.name, b ? 1.f : 0.f);
							Edited();
						}
					} else {
						// ⚠️ LES BORNES VIENNENT DE LA DECLARATION, pas de l'editeur. Un
						//    editeur qui bornerait lui-meme creerait une seconde verite,
						//    et le jour ou la declaration change, le curseur resterait
						//    sur l'ancienne.
						const float32 lo = pd.maxVal > pd.minVal ? pd.minVal : 0.f;
						const float32 hi = pd.maxVal > pd.minVal ? pd.maxVal : 1.f;
						if (ec.SliderFloat(pd.label, v, lo, hi)) {
							n.instance.SetParam(pd.name, v);
							Edited();
						}
					}
				}

				ec.Separator();
				ec.Text("Métriques (px logiques)");
				for (uint16 i = 0; i < d->metricCount; ++i) {
					const NkMetricDecl &md = d->metrics[i];
					float32 v = n.instance.Metric(md.name);
					// Plage d'edition derivee du defaut declare : faute de bornes dans
					// `NkMetricDecl`, on ouvre autour de la valeur declaree plutot que
					// d'inventer un maximum absolu.
					if (ec.SliderFloat(md.name, v, 0.f, md.defVal * 4.f + 8.f)) {
						n.instance.SetMetric(md.name, v);
						Edited();
					}
				}

				ec.Separator();
				ec.Text("Jetons de thème");
				for (uint16 i = 0; i < d->tokenCount; ++i) {
					const NkTokenDecl &td = d->tokens[i];
					char line[192];
					snprintf(line, sizeof(line), "%s = %s%s", td.name, n.instance.TokenRole(td.name),
							 n.instance.IsTokenOverridden(td.name) ? "  (modifie)" : "");
					ec.Text(line);
				}
				// ⚠️ LA REAFFECTATION D'UN JETON N'A TOUJOURS PAS DE WIDGET, et c'est
				//    une absence NOMMEE : il faudrait un selecteur de role, qui est un
				//    composant a part entiere (il en existe deja un dans NK3DModeler).
				//    L'ecrire ici en serait une copie de plus. Le mecanisme, lui, est
				//    en place et teste.

				ec.Separator();
				ec.Text("Événements exposés (déclarés, non branchés)");
				for (uint16 i = 0; i < d->eventCount; ++i) {
					const NkEventDecl &e = d->events[i];
					char line[256];
					int32 k = snprintf(line, sizeof(line), "%s(", e.name);
					for (uint8 a = 0; a < e.argCount && k > 0 && k < (int32)sizeof(line); ++a)
						k += snprintf(line + k, sizeof(line) - (uint32)k, "%s%s: %s", a ? ", " : "",
									  e.args[a].name, NkArgTypeName(e.args[a].kind));
					if (k > 0 && k < (int32)sizeof(line))
						snprintf(line + k, sizeof(line) - (uint32)k, ")");
					ec.Text(line);
				}
				ec.Separator();
				if (ec.Button("Réglages : tout réinitialiser")) {
					n.instance.ResetAll();
					Edited();
				}
				char b[128];
				snprintf(b, sizeof(b), "%u écart(s) par rapport a la déclaration",
						 n.instance.OverrideCount());
				ec.Text(b);
			}

			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 5 — LES PREFERENCES
	// ═══════════════════════════════════════════════════════════════════════════
	//  Il ferme la chaine demandee par Rodolf : **Preferences (interface) ->
	//  fichier de config -> lu par defaut au demarrage**. Les deux derniers
	//  maillons existaient ; celui-ci est le premier.
	//
	//  ⚠️ POURQUOI UN PANNEAU ET PAS LA FENETRE « Preferences » DE LA COQUILLE.
	//     Elle existe (`NkEditorShell::OpenPreferences`, categories Polices /
	//     Theme) et ce serait sa place — mais `DrawPreferences` est **privee** et
	//     la coquille n'expose **aucun point de greffe** pour qu'une application y
	//     ajoute une categorie. La toucher demanderait de modifier NKEditorKit,
	//     que je ne tiens pas. **Manque porte au canal** ; en attendant, un
	//     panneau, qui a l'avantage d'etre testable et de ne rien casser chez
	//     personne.
	//
	//  ⚠️ CE QUI EST ECRIT, ET CE QUI NE L'EST PAS. On ecrit **`nkuidesign.cfg`**,
	//     a cote de l'executable. **Jamais `~/.nkcode_*.cfg`** : la coquille les
	//     lit inconditionnellement (`NkLoadTheme`, `NkLoadFontPrefs`), donc cette
	//     application en HERITE — mais NKCode est en pause depuis des semaines et
	//     il fonctionne. Ecrire chez lui pour regler une autre application
	//     casserait une application au repos : un cout pur, pour rien.
	class PreferencesPanel : public NkEditorPanel {
		public:
			explicit PreferencesPanel(DesignState *st)
				: NkEditorPanel("Préférences", NkEditorDockSide::NK_RIGHT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				designkit::releve::Zone(ctx, "preferences");
				ec.Text("Backend graphique");
				nkgui::TextWrapped(ctx, "Le réglage est écrit dans nkuidesign.cfg, à côté de "
										"l'exécutable. Il vaut pour tous les lancements suivants.");
				ec.Separator();

				// ── CE QUI TOURNE MAINTENANT, ET QUI L'A DECIDE ──────────────
				// ⚠️ LES DEUX LIGNES COMPTENT AUTANT. Afficher le backend sans
				//    dire QUI l'a choisi laisserait un utilisateur regler « opengl »
				//    ici, voir « dx11 » se lancer (parce qu'une variable
				//    d'environnement gagne) et n'avoir aucun moyen de comprendre.
				designkit::KeyValue(ctx, "en cours", mSt->gfxEffective.Data());
				// ⚠️ PAS UN `KeyValue` POUR LA SOURCE : « fichier de configuration
				//    nkuidesign.cfg (cle gfx) » ne tient dans aucune colonne, et
				//    s'affichait « fichier de configura… ». Une valeur longue n'est
				//    pas une valeur de tableau : elle se met a la ligne.
				ec.Text("décidé par");
				nkgui::TextWrapped(ctx, mSt->gfxSource.Data());
				ec.Separator();

				// ── LE CHOIX ─────────────────────────────────────────────────
				// Les memes noms que la ligne de commande et le fichier : un seul
				// vocabulaire pour les quatre sources, sinon l'interface enseigne
				// un mot que le fichier ne comprend pas.
				// La table vit dans `Backend.h` : le menu et ce panneau lisent la
				// même, tant que les deux coexistent.
				uint32 nApis = 0;
				const char *const *kApis = NkGfxApiNames(nApis);
				const int32 pick =
					designkit::Segmented(ctx, kApis, nApis, mSt->prefsChoice, "prefs.gfx");
				if (pick >= 0)
					mSt->prefsChoice = pick;

				if (designkit::Button(ctx, "Enregistrer dans nkuidesign.cfg", "prefs.enregistrer"))
					mSt->SavePrefs(kApis[mSt->prefsChoice < nApis ? mSt->prefsChoice : 0]);

				// ── LE REDEMARRAGE, ANNONCE ──────────────────────────────────
				// ⚠️ REGLE DE RODOLF : ce qui implique un redemarrage le DIT ; ce
				//    qui n'en a pas besoin ne le demande pas. Le backend graphique
				//    en implique un — le contexte est cree une fois, au lancement.
				//    La langue, elle, n'en impliquera pas : NKGui devra la gerer a
				//    chaud, et ce panneau ne doit pas prendre l'habitude de
				//    reclamer un redemarrage pour tout.
				if (mSt->prefsNeedsRestart) {
					ec.Separator();
					nkgui::TextWrapped(ctx, "!! Enregistre. Le backend graphique ne change qu'au "
											"PROCHAIN lancement : le contexte est créé une fois, au "
											"démarrage. Fermez et relancez pour l'appliquer.");
				}
				if (!mSt->prefsStatus.Empty()) {
					ec.Separator();
					nkgui::TextWrapped(ctx, mSt->prefsStatus.Data());
				}
			}

		private:
			DesignState *mSt;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 6 — L'IA
	// ═══════════════════════════════════════════════════════════════════════════
	// La place, pas le modele. Ce panneau ne sait rien de ce qu'il y a derriere le
	// backend, et c'est exactement ce qui permettra de le remplacer par Ilyana
	// sans toucher a une ligne d'ici.
	// ═══════════════════════════════════════════════════════════════════════════
	//  TROIS PANNEAUX DES ÉCRANS 16 / 20 / 21 — le CHROME se pose, l'état vide
	//  se DIT. Aucune donnée de démonstration peinte (cadrage du 31/08) : les
	//  compteurs de simulation, les ambiances et la liste de greffons naîtront
	//  de leurs mécanismes ; en attendant, chaque panneau nomme ce qui manque.
	// ═══════════════════════════════════════════════════════════════════════════
	class SimulationPanel : public NkEditorPanel {
		public:
			explicit SimulationPanel(DesignState *st)
				: NkEditorPanel("Simulation", NkEditorDockSide::NK_BOTTOM), mSt(st) {
				SetOpen(false);
			}
			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				{
					const NkRect r = ctx.NextItemRect(-1.f, 30.f);
					// la pastille d'etat (verte quand une simulation TOURNERA)
					dl.AddCircleFilled({r.x + 16.f, r.y + 15.f}, 4.f, ctx.theme.textMuted);
					costume::TexteGras(dl, F.px11, r.x + 28.f,
									   costume::CentrerY(F.px11, r.y, 30.f),
									   "Aucune simulation en cours", ctx.theme.text, 0.3f);
				}
				ec.Text("Le mode Simulation viendra avec le modèle de comportement (§6.4) :");
				ec.Text("il comptera les chemins ATTEINTS, JAMAIS ATTEINTS, et marquera les");
				ec.Text("DOUBLURES — les compteurs « 7 atteints · 3 jamais atteints » de la");
				ec.Text("maquette sont l'état qu'il produira, pas un décor à peindre.");
			}

		private:
			DesignState *mSt;
	};

	class AmbiancesPanel : public NkEditorPanel {
		public:
			explicit AmbiancesPanel(DesignState *st)
				: NkEditorPanel("Ambiances", NkEditorDockSide::NK_RIGHT), mSt(st) {
				SetOpen(false);
			}
			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				// La sélection RÉELLE et son rôle (l'en-tête de l'écran 20).
				const NkUINode *n = mSt->doc.IsValidIndex(mSt->selected)
										? &mSt->doc.nodes[(uint32)mSt->selected]
										: nullptr;
				{
					const NkRect r = ctx.NextItemRect(-1.f, 30.f);
					costume::TexteGras(dl, F.px11, r.x + 12.f,
									   costume::CentrerY(F.px11, r.y, 30.f),
									   n ? n->label.Data() : "(aucune sélection)",
									   ctx.theme.text, 0.4f);
				}
				if (n && !n->role.Empty()) {
					const NkRect r = ctx.NextItemRect(-1.f, 20.f);
					char b[96];
					snprintf(b, sizeof(b), "Rôle : %s", n->role.Data());
					costume::Texte(dl, F.px10, r.x + 12.f,
								   costume::CentrerY(F.px10, r.y, 20.f), b,
								   ctx.theme.textMuted);
				}
				ec.Separator();
				ec.Text("Les AMBIANCES — états animés d'un rôle, lissage, valeur de");
				ec.Text("repos, instances en phase — viendront avec le modèle");
				ec.Text("d'animation. Le mode Animation de la toile est posé (Entry) ;");
				ec.Text("ce panneau en sera la table de réglage.");
			}

		private:
			DesignState *mSt;
	};

	class GreffonsPanel : public NkEditorPanel {
		public:
			explicit GreffonsPanel(DesignState *st)
				: NkEditorPanel("Greffons", NkEditorDockSide::NK_RIGHT), mSt(st) {
				SetOpen(false);
			}
			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				{
					const NkRect r = ctx.NextItemRect(-1.f, 32.f);
					const NkRect rf = {r.x + 10.f, r.y + 4.f, (r.w > 304.f ? 304.f : r.w) - 20.f,
									   24.f};
					nkentseu::editorkit::NkOverlayTextField(ctx, dl, ctx.font, rf, mRecherche,
																(int32)sizeof(mRecherche), false);
					if (!mRecherche[0])
						costume::Texte(dl, F.px11, rf.x + 8.f,
									   costume::CentrerY(F.px11, rf.y, rf.h), "Rechercher…",
									   ctx.theme.textMuted);
				}
				{
					const NkRect r = ctx.NextItemRect(-1.f, 24.f);
					ctx.BeginDisabled();
					costume::Texte(dl, F.px11, r.x + 12.f,
								   costume::CentrerY(F.px11, r.y, 24.f),
								   "(aucun greffon installé)", ctx.theme.textMuted);
					ctx.EndDisabled();
				}
				// « + Installer… » et « Ouvrir le dossier… » : inertes, et ils le disent.
				const char *const kActes[2] = {"+ Installer un greffon…",
											   "Ouvrir le dossier des greffons"};
				for (uint32 i = 0; i < 2; ++i) {
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const bool sv = ctx.popupDepth == 0
									&& NkGuiRectContains({r.x + 8.f, r.y, 220.f, 26.f},
														 ctx.input.mousePos);
					costume::Texte(dl, F.px11, r.x + 12.f,
								   costume::CentrerY(F.px11, r.y, 26.f), kActes[i],
								   sv ? ctx.theme.text : ctx.theme.textMuted);
					if (sv && ctx.input.mouseClicked[0])
						mSt->status = NkString("Greffons : à brancher (chargement, signature, "
											   "dossier — le mécanisme §9 arrive).");
				}
				ec.Separator();
				ec.Text("L'installation (écrans 22-23, signature NON SIGNÉ), la");
				ec.Text("désinstallation (24) et le voile « greffon manquant » de la");
				ec.Text("toile (25) viendront avec le mécanisme de greffons.");
			}

		private:
			DesignState *mSt;
			char mRecherche[64] = {0};
	};

	class AIPanel : public NkEditorPanel {
		public:
			explicit AIPanel(DesignState *st)
				: NkEditorPanel("IA", NkEditorDockSide::NK_BOTTOM), mSt(st) {
				// ⚠️ REPLIÉ PAR DÉFAUT, ET LA MESURE DIT POURQUOI CE N'EST QU'UN
				//    DEMI-CORRECTIF. Le plan (§4) veut en bas un « rail de pastilles,
				//    ancré discret » ; §13 décrit le mécanisme complet (rails de
				//    28 px, pastilles à quatre états). Mesure faite dans
				//    `NkEditorShell.h` : **ce mécanisme n'existe pas** — la coquille
				//    ne porte ni rail ni pastille, seulement des « voyants » de pied
				//    de fenêtre. La réponse à « pastille laissée ouverte, ou panneau
				//    pas encore converti ? » est donc la SECONDE : rien n'a été
				//    converti, parce qu'il n'y a pas encore de rail où le poser.
				//    ⚠️ Et ce panneau n'irait de toute façon pas là : §13.1 place
				//       « Chat IA » sur le rail DROIT, le rail bas portant
				//       Console/Validation et Preview/Test.
				//    En attendant, il est FERMÉ au démarrage : la toile récupère le
				//    cinquième de fenêtre qu'il occupait, et il reste atteignable par
				//    `Affichage > Panneaux` et par `IA > Ouvrir le chat IA`. Une
				//    capacité qui se replie n'est pas une capacité perdue.
				SetOpen(false);
			}

			// ── LE COSTUME DE L'ÉCRAN 28, SUR LA PLOMBERIE Q31 ──────────────
			// Bande modèle (nom RÉEL du backend + pilule LOCAL + « Rien ne
			// quitte cette machine. »), actions rapides, carte « RELEVÉ DE
			// CHANGEMENTS » quand une proposition attend (compte réel, cases,
			// « S'appliquera en une seule opération annulable. », Rejeter /
			// Appliquer — les VRAIS gestes DiscardProposal / CommitProposal),
			// invite + envoi. Retirer/Rejeu restent en liens dessous.
			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				if (mSt->proposerInitial) { // mise en scene : une proposition prete
					mSt->proposerInitial = false;
					snprintf(mSt->promptBuf, sizeof(mSt->promptBuf),
							 "un écran de connexion : un titre, deux champs, un bouton");
					Proposer();
				}
				// ── LA BANDE MODÈLE ──────────────────────────────────────────
				{
					const NkRect r = ctx.NextItemRect(-1.f, 34.f);
					costume::IcCarreaux(dl, r.x + 10.f, r.y + 10.f, ctx.theme.textMuted);
					const bool dispo = mSt->ai.Backend() && mSt->ai.Backend()->IsAvailable();
					char b[96];
					snprintf(b, sizeof(b), "%s",
							 mSt->ai.Backend() ? mSt->ai.Backend()->Name() : "(aucun backend)");
					costume::TexteGras(dl, F.px11, r.x + 30.f,
									   costume::CentrerY(F.px11, r.y, 34.f), b, ctx.theme.text,
									   0.3f);
					float32 px = r.x + 30.f + costume::Largeur(F.px11, b) + 8.f;
					if (dispo) {
						// la pilule LOCAL, verte — vraie : le pont est local.
						const float32 wl = costume::Largeur(F.px9, "LOCAL") + 10.f;
						NkColor vert = ctx.theme.success;
						NkColor fondV = vert;
						fondV.a = 40;
						dl.AddRectFilled({px, r.y + 9.f, wl, 16.f}, fondV, 8.f);
						costume::TexteGras(dl, F.px9, px + 5.f,
										   costume::CentrerY(F.px9, r.y + 9.f, 16.f), "LOCAL",
										   vert, 0.4f);
					} else
						costume::Texte(dl, F.px10, px, costume::CentrerY(F.px10, r.y, 34.f),
									   "(indisponible)", ctx.theme.textMuted);
				}
				{
					const NkRect r = ctx.NextItemRect(-1.f, 18.f);
					costume::Texte(dl, F.px10, r.x + 10.f, costume::CentrerY(F.px10, r.y, 18.f),
								   "Rien ne quitte cette machine.", ctx.theme.textMuted);
					dl.AddLine({r.x, r.y + 17.5f}, {r.x + r.w, r.y + 17.5f}, ctx.theme.border,
							   1.f);
				}
				// ── LES ACTIONS RAPIDES (chips) ──────────────────────────────
				{
					static const char *const kChips[3] = {"Générer un écran",
														  "Modifier la sélection",
														  "Générer un comportement"};
					const NkRect r = ctx.NextItemRect(-1.f, 30.f);
					const float32 wVisC = r.w > 304.f ? 304.f : r.w;
					float32 x = r.x + 10.f;
					for (uint32 i = 0; i < 3; ++i) {
						const float32 w = costume::Largeur(F.px10, kChips[i]) + 16.f;
						if (x + w > r.x + wVisC - 4.f)
							break; // le panneau étroit coupe la 3e — elle vit au menu IA
						const NkRect c = {x, r.y + 4.f, w, 22.f};
						const bool sv = ctx.popupDepth == 0
										&& NkGuiRectContains(c, ctx.input.mousePos);
						dl.AddRectFilled(c, sv ? ctx.theme.buttonHover : ctx.theme.button, 11.f);
						costume::Texte(dl, F.px10, c.x + 8.f,
									   costume::CentrerY(F.px10, c.y, c.h), kChips[i],
									   ctx.theme.text);
						if (sv && ctx.input.mouseClicked[0])
							mSt->status = NkString("Action rapide : à brancher (elle remplira "
												   "l'invite).");
						x += w + 6.f;
					}
				}
				// ── LA CARTE « RELEVÉ DE CHANGEMENTS » ───────────────────────
				if (mSt->ai.HasProposal()) {
					const uint32 nprop = mSt->ai.Proposal().NodeCount();
					const float32 hCarte = 96.f + (float32)(nprop > 6 ? 6 : nprop) * 22.f;
					const NkRect r = ctx.NextItemRect(-1.f, hCarte + 8.f);
					// ⚠️ LARGEUR VISIBLE, PAS LARGEUR DE REGION : dans le tiroir la
					//    region de defilement est plus large que la fenetre — la
					//    carte deborderait a droite et ses boutons partiraient hors
					//    champ (mesure sur capture, 31/08).
					const float32 wVis = r.w > 304.f ? 304.f : r.w;
					const NkRect c = {r.x + 8.f, r.y + 4.f, wVis - 16.f, hCarte};
					dl.AddRectFilled(c, ctx.theme.panel, 6.f);
					dl.AddRect(c, ctx.theme.border, 1.f, 6.f);
					costume::TexteGras(dl, F.px11, c.x + 10.f, c.y + 8.f,
									   "Relevé de changements", ctx.theme.text, 0.5f);
					char b[64];
					snprintf(b, sizeof(b), "%u ajout(s)", nprop);
					costume::Texte(dl, F.px10, c.x + 10.f, c.y + 24.f, b, ctx.theme.textMuted);
					float32 y = c.y + 42.f;
					const uint32 max = nprop > 6 ? 6 : nprop;
					for (uint32 i = 0; i < max; ++i) {
						// la case cochée (l'application PARTIELLE viendra : cases
						// figées cochées, dit dans le rapport)
						dl.AddRectFilled({c.x + 10.f, y + 3.f, 12.f, 12.f}, ctx.theme.accent,
										 3.f);
						const nkgui::NkVec2 pc[3] = {{c.x + 12.5f, y + 9.f},
													 {c.x + 15.f, y + 12.f},
													 {c.x + 19.5f, y + 5.5f}};
						dl.AddPolyline(pc, 3, ctx.theme.onAccent, 1.4f);
						const char *nomN = mSt->ai.Proposal().nodes[i].label.Data();
						costume::Texte(dl, F.px11, c.x + 30.f, y + 2.f,
									   nomN && *nomN ? nomN : "(nœud)", ctx.theme.text);
						y += 22.f;
					}
					costume::Texte(dl, F.px9, c.x + 10.f, c.y + hCarte - 44.f,
								   "S'appliquera en une seule opération annulable.",
								   ctx.theme.textMuted);
					// Rejeter (bord) / Appliquer (accent)
					const float32 by = c.y + hCarte - 30.f;
					const float32 wA = costume::Largeur(F.px11, "Appliquer") + 20.f;
					const float32 wR = costume::Largeur(F.px11, "Rejeter") + 20.f;
					const NkRect ra = {c.x + c.w - wA - 10.f, by, wA, 22.f};
					const NkRect rr = {ra.x - wR - 8.f, by, wR, 22.f};
					dl.AddRect(rr, ctx.theme.border, 1.f, 4.f);
					costume::Texte(dl, F.px11, rr.x + 10.f, costume::CentrerY(F.px11, by, 22.f),
								   "Rejeter", ctx.theme.text);
					dl.AddRectFilled(ra, ctx.theme.accent, 4.f);
					costume::TexteGras(dl, F.px11, ra.x + 10.f,
									   costume::CentrerY(F.px11, by, 22.f), "Appliquer",
									   ctx.theme.onAccent, 0.3f);
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]) {
						if (NkGuiRectContains(ra, ctx.input.mousePos))
							Appliquer();
						else if (NkGuiRectContains(rr, ctx.input.mousePos)) {
							mSt->ai.DiscardProposal();
							mLast = NkString("Proposition rejetée — rien n'a changé.");
						}
					}
				}
				// ── L'INVITE + LA PORTÉE + L'ENVOI ───────────────────────────
				{
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					// la portée : « Sélection » (vraie : la greffe vise la sélection)
					const float32 wp = costume::Largeur(F.px10, "Sélection") + 14.f;
					const NkRect rp = {r.x + 10.f, r.y + 2.f, wp, 20.f};
					dl.AddRect(rp, ctx.theme.border, 1.f, 10.f);
					costume::Texte(dl, F.px10, rp.x + 7.f, costume::CentrerY(F.px10, rp.y, 20.f),
								   "Sélection", ctx.theme.textMuted);
				}
				InputText(ctx, "Demande", mSt->promptBuf, (int32)sizeof(mSt->promptBuf));
				if (ec.Button("Proposer (aperçu)"))
					Proposer();
				if (mDernierCommit.Accepted() && ec.Button("Retirer la greffe posée"))
					Retirer();
				if (ec.Button("Vérifier le document par rejeu"))
					Replay();
				ec.Separator();
				ec.Text(mLast.Data() ? mLast.Data() : "");
			}

		private:
			void Proposer() {
				const NkAIResult r = mSt->ai.Propose(mSt->promptBuf, mSt->doc);
				if (r.Accepted()) {
					mLast = NkString("Proposition validée et rejouée — en attente. "
									 "Appliquer la pose ; Rejeter la jette.");
				} else {
					char b[320];
					snprintf(b, sizeof(b), "REFUSÉE — %s. Le document n'a pas bougé.",
							 NkAIVerdictName(r.verdict));
					mLast = NkString(b);
					if (r.detail.Length() > 0) {
						mLast.Append("  ");
						mLast.Append(r.detail);
					}
				}
			}
			void Appliquer() {
				const NkAIResult r = mSt->ai.CommitProposal(mSt->doc, mSt->selected);
				char b[320];
				if (r.Accepted()) {
					snprintf(b, sizeof(b),
							 "Appliquée : %u nœud(s) posés. « Retirer la greffe posée » "
							 "l'annule en une opération.",
							 r.nodesAdded);
					mSt->host.SyncTo(mSt->doc);
					mSt->selected = r.graftedRoot;
					mDernierCommit = r;
				} else {
					snprintf(b, sizeof(b), "GREFFE REFUSÉE — %s. La proposition reste en attente.",
							 NkAIVerdictName(r.verdict));
				}
				mLast = NkString(b);
			}
			void Retirer() {
				if (NkDesignAI::Retract(mSt->doc, mDernierCommit)) {
					mSt->host.SyncTo(mSt->doc);
					mSt->SelectSingle(0);
					mLast = NkString("Greffe retirée — le document est revenu à l'état d'avant.");
				} else {
					mLast = NkString("RETRAIT REFUSÉ — le document a changé depuis la pose.");
				}
				mDernierCommit = NkAIResult();
			}
			void Ask() {
				const NkAIResult r = mSt->ai.Ask(mSt->promptBuf, mSt->doc, mSt->selected);
				char b[320];
				if (r.Accepted()) {
					snprintf(b, sizeof(b), "Acceptée : %u nœud(s) posés, rejeu conforme.", r.nodesAdded);
					mSt->host.SyncTo(mSt->doc);
					mSt->selected = r.graftedRoot;
				} else {
					snprintf(b, sizeof(b), "REFUSÉE — %s. Le document n'a pas bougé.",
							 NkAIVerdictName(r.verdict));
				}
				mLast = NkString(b);
				if (r.detail.Length() > 0) {
					mLast.Append("  ");
					mLast.Append(r.detail);
				}
			}
			void Replay() {
				const uint32 diffs = NkDesignAI::ReplayDiffs(mSt->doc, mSt->ai.replaySurface);
				char b[192];
				snprintf(b, sizeof(b), "Rejeu du document : %u divergence(s)%s", diffs,
						 diffs == 0 ? " — fidèle." : " — NON fidèle.");
				mLast = NkString(b);
				if (diffs == 0)
					mSt->doc.MarkVerified(0);
			}
			DesignState *mSt;
			NkString mLast;
			NkAIResult mDernierCommit;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 7 — LA HIÉRARCHIE (document 3 §11, planches §22.5 et 091913)
	// ═══════════════════════════════════════════════════════════════════════════
	//  ⚠️ CE N'EST PAS « Composition » RENOMMÉ. Le panneau Composition empilait
	//     des `Selectable` et six boutons d'action ; celui-ci passe par
	//     **`NkTreeViewModel` / `NkDrawTreeView` du kit**, c'est-à-dire par le
	//     composant que quatre applications partagent. Faire évoluer l'ancien
	//     aurait conservé sa structure — donc conservé les ~460 lignes qui
	//     réimplémentent ce que le kit porte déjà.
	//
	//  DEUX SECTIONS EMPILÉES (§11.6, proposition de Rodolf du 20/08) :
	//     en haut  l'arbre de la page courante  — ce que cette page CONTIENT ;
	//     en bas   les composants du projet     — ce que ce projet POSSÈDE.
	//  Deux modèles distincts, un seul composant : c'est la démonstration que le
	//  composant du kit sert deux usages sans variante nouvelle.
	//
	//  ⚠️ LA LOUPE EST UNE SEULE, EN HAUT DU PANNEAU, et elle filtre les deux
	//     sections. Le composant sait dessiner sa propre barre de recherche
	//     (`show_search`) ; l'activer deux fois donnerait deux champs pour une
	//     seule intention. Le paramètre est donc mis à 0 sur les deux instances,
	//     et l'hôte écrit dans `modele.filter` — le champ prévu pour ça.
	//
	//  ⚠️ CE QUI N'EST PAS ENCORE LÀ, ET QUI EST DIT PLUTÔT QUE SUGGÉRÉ : le
	//     badge d'avertissement (§11.2), le reparentage par glisser (§11.3), le
	//     losange d'instance (§11.4) et le compte d'instances de la section
	//     basse. Le composant porte les trois premiers ; il leur manque
	//     seulement d'être alimentés. Le CONTENU s'affine ensuite — ce qui se
	//     juge aujourd'hui est la place, le titre et les proportions.
	// ═══════════════════════════════════════════════════════════════════════════
	//  LA BIBLIOTHÈQUE DE COMPOSANTS (écran 9 Banani) — la palette par PROVENANCE
	// ═══════════════════════════════════════════════════════════════════════════
	//  Trois groupes : Projet / Importé / Système. Ce qui s'affiche est le
	//  REGISTRE RÉEL (cadrage Rodolf : les entrées de la maquette sont des
	//  données de démonstration — on ne les peint pas dans le code). Projet et
	//  Importé sont vides aujourd'hui et le DISENT ; « Importer un composant… »
	//  est inerte et le dit. S'ouvre par la pastille du rail droit (tiroir).
	class BibliothequePanel : public NkEditorPanel {
		public:
			explicit BibliothequePanel(DesignState *st)
				: NkEditorPanel("Bibliothèque", NkEditorDockSide::NK_RIGHT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				designkit::releve::Zone(ctx, "bibliotheque");
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				// ── La recherche (costume : boîte 24 px) ─────────────────────
				{
					const NkRect r = ctx.NextItemRect(-1.f, 32.f);
					const NkRect rf = {r.x + 10.f, r.y + 4.f, r.w - 20.f, 24.f};
					nkentseu::editorkit::NkOverlayTextField(ctx, dl, ctx.font, rf, mRecherche,
																(int32)sizeof(mRecherche), false);
					if (!mRecherche[0])
						costume::Texte(dl, F.px11, rf.x + 8.f,
									   costume::CentrerY(F.px11, rf.y, rf.h), "Rechercher…",
									   ctx.theme.textMuted);
				}
				Section(ctx, "Projet");
				Phrase(ctx, "(aucun composant de projet — promouvoir en crée)");
				Section(ctx, "Importé");
				Phrase(ctx, "(aucun composant importé)");
				Section(ctx, "Système");
				const uint16 n = NkComponentRegistry::Count();
				for (uint16 c = 0; c < n; ++c) {
					const NkComponentDecl *d = NkComponentRegistry::At(c);
					if (!d || !d->name)
						continue;
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					costume::IcPanneau(dl, r.x + 12.f, r.y + 7.f, ctx.theme.textMuted);
					costume::Texte(dl, F.px11, r.x + 30.f, costume::CentrerY(F.px11, r.y, 26.f),
								   d->name, ctx.theme.text);
					// le compte d'instances RÉEL dans le document (badge « ×N »)
					int32 compte = 0;
					for (uint32 i = 0; i < (uint32)mSt->doc.nodes.Size(); ++i)
						if (StrEq(mSt->doc.nodes[i].component.Data(), d->name))
							++compte;
					if (compte > 0) {
						char b[16];
						snprintf(b, sizeof(b), "\xC3\x97%d", compte);
						costume::BadgePilule(dl, F.px9,
											 r.x + 34.f + costume::Largeur(F.px11, d->name),
											 r.y + 6.f, 14.f, b, ctx.theme.accent);
					}
				}
				// ── « Importer un composant… » (pied, inerte et il le dit) ───
				{
					const NkRect r = ctx.NextItemRect(-1.f, 40.f);
					dl.AddLine({r.x, r.y + 8.f}, {r.x + r.w, r.y + 8.f}, ctx.theme.border, 1.f);
					costume::Texte(dl, F.px11, r.x + 12.f, r.y + 16.f, "Importer un composant…",
								   ctx.theme.textMuted);
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(r, ctx.input.mousePos))
						mSt->status = NkString("Importer un composant : à brancher.");
				}
			}

		private:
			void Section(NkGuiContext &ctx, const char *titre) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 24.f);
				costume::TexteGras(dl, F.px9, r.x + 12.f, r.y + 10.f, titre, ctx.theme.textMuted,
								   0.4f);
				const float32 lx = r.x + 12.f + costume::Largeur(F.px9, titre) + 8.f;
				dl.AddLine({lx, r.y + 15.f}, {r.x + r.w - 12.f, r.y + 15.f}, ctx.theme.border,
						   1.f);
			}
			void Phrase(NkGuiContext &ctx, const char *t) {
				auto &F = costume::Fontes();
				const NkRect r = ctx.NextItemRect(-1.f, 22.f);
				ctx.BeginDisabled();
				costume::Texte(ctx.DL(), F.px10, r.x + 12.f, costume::CentrerY(F.px10, r.y, 22.f),
							   t, ctx.theme.textMuted);
				ctx.EndDisabled();
			}
			DesignState *mSt;
			char mRecherche[64] = {0};
	};

	class HierarchyPanel : public NkEditorPanel {
		public:
			explicit HierarchyPanel(DesignState *st)
				: NkEditorPanel("Hiérarchie", NkEditorDockSide::NK_LEFT), mSt(st) {
				// ⚠️ LES DEUX INSTANCES SONT LIÉES À LA MÊME DÉCLARATION. Les
				//    nombres (hauteur de ligne, indentation, largeur du chevron)
				//    viennent donc de `NkTreeViewDecl()`, pas d'un littéral écrit
				//    ici : un éditeur pourra les changer, et ils ne seront jamais
				//    à deux valeurs dans le même programme.
				mInstPages.Bind(NkTreeViewDecl());
				mInstComposants.Bind(NkTreeViewDecl());
				// Une seule loupe, en haut : voir le bandeau ci-dessus.
				mInstPages.SetParam("show_search", 0.f);
				mInstComposants.SetParam("show_search", 0.f);
				// Les bandes de titre « PAGES » / « COMPOSANTS » sont dessinées par
				// l'hôte (le composant remplit sa bande mais n'y écrit aucun titre).
				mInstPages.SetParam("show_header", 0.f);
				mInstComposants.SetParam("show_header", 0.f);
				mInstComposants.SetParam("show_footer", 0.f);
				mInstPages.SetParam("show_footer", 0.f); // Banani : pas de pied compteur
				// COSTUME BANANI (31/08) : pas de filets d'indentation dans la
				// maquette — et ses mesures de rangée : hauteur 22, marge 8,
				// chevron 9 (+4 d'écart -> case 13), icône 11 (+4 -> case 15).
				// METRIQUES D'INSTANCE : la déclaration partagée (NK3DModeler)
				// garde les siennes — personne d'autre ne bouge.
				mInstPages.SetParam("indent_guides", 0.f);
				// L'œil de visibilité : la maquette ne le montre qu'AU SURVOL, à
				// DROITE (opacity-0 group-hover) — le composant ne porte que la
				// colonne permanente à gauche. Elle est donc retirée (écart nommé :
				// « œil/cadenas au survol » = chantier du composant tree_view).
				mInstPages.SetParam("show_visibility", 0.f);
				mInstComposants.SetParam("show_visibility", 0.f);
				// 24, pas 22 : « dans les tree, ajoute encore un peu d'espace
				// verticalement » (Rodolf, 01/09) — la valeur de la declaration
				// partagee du kit, qui est celle des references aerees.
				mInstPages.SetMetric("row_h", 24.f);
				mInstComposants.SetMetric("row_h", 24.f);
				mInstPages.SetMetric("row_pad", 8.f);
				mInstComposants.SetMetric("row_pad", 8.f);
				mInstPages.SetMetric("chevron_w", 13.f);
				mInstComposants.SetMetric("chevron_w", 13.f);
				mInstPages.SetMetric("icon_w", 15.f);
				mInstComposants.SetMetric("icon_w", 15.f);
			}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				designkit::releve::Zone(ctx, "hierarchie");

				SyncPages();
				SyncComposants();

				// Pendant un renommage d'arbre, les lettres sont une SAISIE : la
				// toile lit ce drapeau avant d'armer un outil (meme regle que
				// l'edition en place — 1er retour, titres desaccordes).
				mSt->renommageArbre =
					(mModelePages.renaming != 0) || (mModeleComposants.renaming != 0);

				// ── LE CONTROLE DU RELEVE (1er retour) : UNE cle, DEUX lecteurs.
				// La Hierarchie et l'etiquette de toile lisent toutes deux
				// `label` ; ce controle MESURE l'accord a chaque image (une
				// chaine a deux copies diverge toujours — famille FocusPanel).
				// hierarchie.titres = [desaccords, renommage en cours, 0, 0] —
				// desaccords DOIT rester 0 hors saisie.
				{
					int32 desaccords = 0;
					const uint32 nT = (uint32)mModelePages.nodes.Size();
					for (uint32 i = 0; i < nT; ++i) {
						const NkTreeNode &t = mModelePages.nodes[i];
						const int32 di = (int32)t.id - 1;
						if (!mSt->doc.IsValidIndex(di)) {
							++desaccords;
							continue;
						}
						const NkUINode &d = mSt->doc.nodes[(uint32)di];
						const char *attendu =
							d.label.Empty()
								? (d.component.Empty() ? "(cadre)" : d.component.Data())
								: d.label.Data();
						if (!NkComponentDecl::StrEq(t.label.Data() ? t.label.Data() : "",
													attendu))
							++desaccords;
					}
					nkgui::NkGuiNoterMesure(ctx, "hierarchie.titres", (float32)desaccords,
											(float32)(mModelePages.renaming != 0 ? 1 : 0), 0.f,
											0.f);
				}

				// ── L'EN-TÊTE BANANI : « Hiérarchie » 12 px 600 + LOUPE (34 px) ──
				// La maquette ne montre PAS de champ de filtre : la loupe le
				// DÉPLIE (le geste reste à un clic, le costume reste exact).
				{
					auto &F = costume::Fontes();
					auto &dl = ctx.DL();
					const NkRect e = ctx.NextItemRect(-1.f, 34.f);
					costume::TexteGras(dl, F.px12, e.x + 12.f,
									   costume::CentrerY(F.px12, e.y, 34.f), "Hiérarchie",
									   ctx.theme.text, 0.5f);
					const NkRect rl = {e.x + e.w - 12.f - 13.f, e.y + (34.f - 13.f) * 0.5f, 13.f,
									   13.f};
					costume::IcLoupe(dl, rl.x, rl.y, ctx.theme.textMuted);
					// L'ŒIL-BARRÉ (écran 8) : « Afficher seulement les éléments à
					// rôle » — un FILTRE vrai, pas un décor : l'arbre ne montre
					// alors que les sous-arbres qui portent un rôle.
					const NkRect rf2 = {rl.x - 20.f, rl.y + 0.5f, 12.f, 12.f};
					{
						const NkColor cf = mFiltreRoles ? ctx.theme.accent : ctx.theme.textMuted;
						costume::IcOeil(dl, rf2.x, rf2.y, cf);
						dl.AddLine({rf2.x + 1.f, rf2.y + 11.f}, {rf2.x + 11.f, rf2.y + 1.f}, cf,
								   1.2f);
					}
					nkentseu::editorkit::NkTooltip(
						ctx,
						ctx.popupDepth == 0
							&& NkGuiRectContains({rf2.x - 3.f, e.y, 18.f, 34.f}, ctx.input.mousePos),
						"Afficher seulement les éléments à rôle");
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains({rf2.x - 3.f, e.y, 18.f, 34.f}, ctx.input.mousePos))
						mFiltreRoles = !mFiltreRoles;
					dl.AddLine({e.x, e.y + 34.f - 0.5f}, {e.x + e.w, e.y + 34.f - 0.5f},
							   ctx.theme.border, 1.f);
					const NkRect zl = {rl.x - 4.f, e.y, 21.f, 34.f};
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(zl, ctx.input.mousePos))
						mFiltreVisible = !mFiltreVisible;
				}
				if (mSt->filtreHierarchieInitial) {
					mSt->filtreHierarchieInitial = false;
					mFiltreVisible = true;
				}
				if (mFiltreVisible)
					InputText(ctx, "Filtrer", mFiltre, (int32)sizeof(mFiltre));
				else
					mFiltre[0] = 0; // replié = plus de filtre actif (état visible)
				CopyFiltre(mModelePages.filter, sizeof(mModelePages.filter));
				CopyFiltre(mModeleComposants.filter, sizeof(mModeleComposants.filter));

				// ── LE PARTAGE DE LA HAUTEUR ─────────────────────────────────
				// ⚠️ MESURE QUI M'A CONTREDIT, ET C'EST POUR ÇA QU'ELLE EST ÉCRITE
				//    ICI. Ma première version prenait `ctx.layout.region` pour la
				//    zone visible du panneau. `--dump-ui` a rendu :
				//        panneau.hierarchie = 48.0 84.0 219.8 1000000.0
				//    `region.h` vaut **un million** : c'est la région de CONTENU
				//    d'une zone défilable, volontairement sans fond. La section
				//    basse est donc partie à y = 999 874 — hors de l'écran, et
				//    « la Hiérarchie n'a qu'une section » aurait été le diagnostic.
				//
				//    La zone VISIBLE est celle du cadre de défilement posé par
				//    `Begin` (`BeginScrollFrame(..., content, ...)`) : c'est
				//    `childStack[childDepth-1].area`. On la lit là, ou nulle part.
				const float32 basY = designkit::HauteurVisibleBas(ctx, "Hiérarchie");
				// ⚠️ LA BORNE OUBLIÉE, MESURÉE (précision de Rodolf, 01/09 : « on
				//    ne voit pas le bas du scrollbar des Composants ») : la pile
				//    compte CINQ items (bande, arbre, poignée, bande, arbre) et la
				//    disposition avance de `itemSpacingY` après CHACUN — 5 × 6 =
				//    30 px que l'ancien partage ne retranchait pas (hier.bornes
				//    rendait dépassement = +30.0). Conséquence double : le rail
				//    des Composants sortait du panneau par le bas, ET le panneau
				//    devenait défilable de 30 px — la molette lui volait les
				//    crans destinés aux sections.
				float32 restant = basY - ctx.layout.cursor.y - 5.f * ctx.layout.itemSpacingY;
				if (restant < 80.f)
					restant = 80.f;
				// La hauteur MEMORISEE (3e retour du 01/09) : lue UNE fois dans
				// nkuidesign.cfg (cle hier_bas), ecrite au relacher de la poignee.
				if (!mHBasLu) {
					mHBasLu = true;
					char v[32];
					const NkString cfg = NkFile::ReadAllText(NkPath(NkGfxConfigPath()));
					if (!cfg.Empty() && NkGfxConfigValue(cfg.Data(), "hier_bas", v, sizeof(v)))
						mHBasVoulu = (float32)atof(v);
				}
				// §11.6 : la section basse est SECONDAIRE ; elle prend le tiers,
				// borné, pour qu'un arbre profond garde de la place.
				float32 hBas = restant * 0.32f;
				if (hBas > 220.f)
					hBas = 220.f;
				if (hBas < 90.f)
					hBas = 90.f;
				// La POIGNÉE (écran 10) : la part choisie a la main PRIME — sur le
				// tiers par defaut ET sur le clamp « les sections se suivent »
				// ci-dessous. Bornes : aucune des deux sections ne disparait
				// (60 px chacune).
				const float32 hBasMax = restant - 60.f - 2.f * kBandeH - kPoigneeH;
				if (mHBasVoulu > 0.f) {
					hBas = mHBasVoulu;
					if (hBas > hBasMax)
						hBas = hBasMax;
					if (hBas < 60.f)
						hBas = 60.f;
				}
				float32 hHaut = restant - hBas - 2.f * kBandeH - kPoigneeH;
				// COSTUME BANANI : les sections SE SUIVENT — quand l'arbre tient,
				// « COMPOSANTS » vient juste dessous (la maquette), pas au tiers
				// bas. ⚠️ C'est un DEFAUT, pas une loi : ce clamp ne s'applique
				// QUE tant qu'aucune hauteur n'a ete choisie a la poignee —
				// mesure du 2e retour du 01/09 : applique apres le choix manuel,
				// il re-ecrasait hHaut et la poignee tiree vers le bas ne bougeait
				// pas (« pas possible de correctement modifier la hauteur »).
				const float32 hPages = (float32)mModelePages.nodes.Size()
										   * mInstPages.Metric("row_h", 24.f)
									   + 6.f; // tout déplié
				if (mHBasVoulu <= 0.f && hPages < hHaut)
					hHaut = hPages;

				BandeDeSection(ctx, "PAGES", "hier.pages.plus");
				DessinerArbre(ctx, mModelePages, mInstPages, hHaut > 60.f ? hHaut : 60.f, "pages");

				// La POIGNÉE DE REDIMENSIONNEMENT à trois points (écran 10) —
				// épaisse, entre les deux sections, et elle REDIMENSIONNE : tirer
				// déplace la FRONTIÈRE (les deux sections suivent, bornées à
				// 60 px chacune) ; la hauteur choisie est MÉMORISÉE dans
				// nkuidesign.cfg au relâcher (clé hier_bas).
				{
					const NkRect fs = ctx.NextItemRect(-1.f, kPoigneeH);
					auto &dlp = ctx.DL();
					const bool sv = ctx.popupDepth == 0
									&& NkGuiRectContains(fs, ctx.input.mousePos);
					dlp.AddLine({fs.x, fs.y + 4.f}, {fs.x + fs.w, fs.y + 4.f},
								sv ? ctx.theme.accent : ctx.theme.border, sv ? 2.f : 1.f);
					const float32 cxp = fs.x + fs.w * 0.5f;
					const NkColor cp = sv ? ctx.theme.accent : ctx.theme.textMuted;
					for (int32 i = -1; i <= 1; ++i)
						dlp.AddCircleFilled({cxp + (float32)i * 7.f, fs.y + 4.f}, 1.5f, cp);
					if (sv)
						ctx.wantCursor = nkgui::NkGuiCursor::ResizeNS;
					if (sv && ctx.input.mouseClicked[0]) {
						mPoigneeActive = true;
						mPoigneeY = ctx.input.mousePos.y;
					}
					if (mPoigneeActive) {
						if (!ctx.input.mouseDown[0]) {
							mPoigneeActive = false;
							// Le relâcher MÉMORISE (l'écriture ne tourne jamais
							// pendant le geste — un fichier par image serait le
							// suspect n.1 de fluidité déjà payé).
							if (mHBasVoulu > 0.f) {
								char v[32];
								snprintf(v, sizeof(v), "%d", (int32)(mHBasVoulu + 0.5f));
								(void)NkGfxConfigSetKey(NkGfxConfigPath(), "hier_bas", v);
							}
						} else {
							const float32 dy = ctx.input.mousePos.y - mPoigneeY;
							if (dy != 0.f) {
								mPoigneeY = ctx.input.mousePos.y;
								float32 voulu = (mHBasVoulu > 0.f ? mHBasVoulu : hBas) - dy;
								// bornage AU GESTE : la souris au-delà de la
								// butée n'accumule pas un « dette » invisible
								// qu'il faudrait re-tirer dans l'autre sens.
								if (voulu > hBasMax)
									voulu = hBasMax;
								if (voulu < 60.f)
									voulu = 60.f;
								mHBasVoulu = voulu;
							}
							ctx.wantCursor = nkgui::NkGuiCursor::ResizeNS;
						}
					}
				}
				BandeDeSection(ctx, "COMPOSANTS", "hier.composants.plus");
				DessinerArbre(ctx, mModeleComposants, mInstComposants, hBas, "composants");
				// LA GARDE DE BORNES (précision de Rodolf, 01/09 : « on ne voit
				// pas le bas du scrollbar des Composants ») : la pile des
				// sections doit finir AU-DESSUS du bas visible du panneau —
				// sinon le rail de la dernière section sort de l'écran ET le
				// panneau devient défilable, la molette lui vole les crans des
				// sections. hier.bornes = [basY, fin de pile, dépassement, 0] ;
				// dépassement DOIT rester <= 0.
				nkgui::NkGuiNoterMesure(ctx, "hier.bornes", basY, ctx.layout.cursor.y,
										ctx.layout.cursor.y - basY, 0.f);
			}

		private:
			static constexpr float32 kBandeH = 22.f;
			/// L'épaisseur de la poignée entre les deux sections. ⚠️ Elle DOIT
			/// entrer dans le partage de hauteur : l'ancien « - 8.f » pour une
			/// poignée de 9 px faisait déborder la pile d'un pixel par image.
			static constexpr float32 kPoigneeH = 9.f;

			/// La bande de titre d'une section, avec son `[+]` (planche 091913).
			/// ⚠️ Assemblage de primitives NKGui, pas un widget de plus : un titre
			///    et un bouton posés à des rectangles explicites.
			void BandeDeSection(NkGuiContext &ctx, const char *titre, const char *id) {
				// COSTUME BANANI : libellé MAJUSCULES 10 px 600 `text_muted` à
				// gauche (marge 8), « + » 14 px à droite (marge 8) — dessinés au
				// trait, le clic pris à la main (un Button repeindrait son fond).
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, kBandeH);
				// ── LA MÊME BANDE QUE L'INSPECTEUR — chemin frère, un seul
				//    peintre. Cette fonction s'appelait DÉJÀ « BandeDeSection »
				//    et ne dessinait aucune bande : le nom promettait ce que le
				//    code ne faisait pas.
				costume::BandeEnTete(dl, r.x, r.y, r.w, r.h, ctx.theme.header, ctx.theme.border);
				costume::TexteGras(dl, F.px10, r.x + 8.f, costume::CentrerY(F.px10, r.y, r.h),
								   titre, ctx.theme.textMuted, 0.4f);
				const float32 wp = costume::Largeur(F.px15, "+");
				const NkRect rp = {r.x + r.w - 8.f - wp - 6.f, r.y, wp + 6.f, r.h};
				costume::Texte(dl, F.px15, rp.x + 3.f, costume::CentrerY(F.px15, r.y, r.h), "+",
							   ctx.theme.textMuted);
				if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
					&& NkGuiRectContains(rp, ctx.input.mousePos)) {
					if (NkComponentDecl::StrEq(id, "hier.pages.plus")) {
						// LE [+] DE PAGES CREE (6e retour — il disait « à
						// brancher »). Le nom NAIT EN EDITION (Lunacy) : la
						// saisie du kit s'ouvre sur la rangee neuve, le pilote
						// ClavierRenommage la tient.
						const int32 p = mSt->CreerPage();
						if (p >= 0) {
							SyncPages(); // la rangee doit exister pour la saisie
							mModelePages.renaming = (nkentseu::nk_uint64)(p + 1);
							// le clic du [+] se mange dans le COMPOSANT (il tient
							// desormais le volet « clic ailleurs » du contrat)
							mModelePages.renameEatClick = true;
							snprintf(mModelePages.renameBuf, sizeof(mModelePages.renameBuf),
									 "%s", mSt->doc.nodes[(uint32)p].label.Data());
							mSt->DireAuPied("Page créée — son nom est en édition (Entrée "
											"valide) ; le format se choisit par la section "
											"CIBLE.");
						}
					} else {
						// COMPOSANTS : le registre dynamique est un chantier a
						// part (borne kMaxComponents) — le clic le DIT.
						mSt->DireAuPied("[+] COMPOSANTS : le registre dynamique de "
										"composants est un chantier à part — rien n'est "
										"créé.");
					}
				}
			}

			/// L'HOTE TIENT LE CLAVIER DU RENOMMAGE (contrat du kit — bloc
			/// « CONTOURNEMENT » de NkTreeViewModel.h : « l'hote, qui a le
			/// clavier, ecrit dans renameBuf et leve renameCommit/Cancel »).
			/// Contrat universel d'edition (Rodolf, 31/08) : ENTREE valide,
			/// ECHAP annule ; le CLIC AILLEURS, lui, est juge par le COMPOSANT
			/// (hors de la rangee editee = valider, puis le clic agit) — voir la
			/// mesure du 01/09 dans `NkTreeViewDraw.cpp`. Sans ce pilote, le
			/// double-clic du kit ouvrait une saisie que RIEN ne fermait : la
			/// rangee restait figee sur son tampon pendant que la toile vivait —
			/// les « titres desaccordes » du 1er retour.
			void ClavierRenommage(NkGuiContext &ctx, NkTreeViewModel &m) {
				if (m.renaming == 0)
					return;
				auto &in = ctx.input;
				int32 len = 0;
				while (m.renameBuf[len])
					++len;
				// La frappe (codepoints traduits par l'OS), encodee UTF-8 —
				// les libelles accentues du document en dependent.
				for (int32 i = 0; i < in.charCount; ++i) {
					const uint32 cp = (uint32)in.chars[i];
					if (cp < 32u)
						continue;
					char enc[4];
					int32 n = 0;
					if (cp < 0x80u)
						enc[n++] = (char)cp;
					else if (cp < 0x800u) {
						enc[n++] = (char)(0xC0u | (cp >> 6));
						enc[n++] = (char)(0x80u | (cp & 0x3Fu));
					} else if (cp < 0x10000u) {
						enc[n++] = (char)(0xE0u | (cp >> 12));
						enc[n++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
						enc[n++] = (char)(0x80u | (cp & 0x3Fu));
					} else {
						enc[n++] = (char)(0xF0u | (cp >> 18));
						enc[n++] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
						enc[n++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
						enc[n++] = (char)(0x80u | (cp & 0x3Fu));
					}
					if (len + n < (int32)sizeof(m.renameBuf) - 1)
						for (int32 k = 0; k < n; ++k)
							m.renameBuf[len++] = enc[k];
				}
				m.renameBuf[len] = 0;
				// Retour arriere : UN codepoint (pas un octet — un « é » entier).
				if (in.KeyPressedRepeat(nkgui::NkGuiKey::Backspace) && len > 0) {
					--len;
					while (len > 0 && ((unsigned char)m.renameBuf[len] & 0xC0u) == 0x80u)
						--len;
					m.renameBuf[len] = 0;
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Enter))
					m.renameCommit = true;
				else if (in.KeyPressed(nkgui::NkGuiKey::Escape))
					m.renameCancel = true;
				// ⚠️ LE CLIC AILLEURS N'EST PLUS JUGE ICI — mesure du 01/09
				//    (Rodolf : « plus possible de desactiver l'edition ») : ce
				//    pilote comparait le clic a la ZONE ENTIERE de l'arbre, donc
				//    un clic sur une AUTRE rangee — le geste naturel — ne sortait
				//    jamais de la saisie. Le rectangle juste est celui de la
				//    RANGEE editee, et seul le composant le connait :
				//    `NkDrawTreeView` tient desormais ce volet du contrat (hors de
				//    la rangee = valider, puis le clic agit), pour TOUS ses hotes.
				//    Un seul mecanisme — pas un doublon qui diverge.
			}

			void DessinerArbre(NkGuiContext &ctx, NkTreeViewModel &modele,
							   NkComponentInstance &inst, float32 hauteur, const char *cle) {
				const NkRect zone = ctx.NextItemRect(-1.f, hauteur);
				if (zone.w <= 0.f || zone.h <= 0.f)
					return;
				designkit::releve::Rect(ctx, cle, zone);
				// L'ASCENSEUR PAR SECTION (2e passe de Rodolf : « un pour la
				// Hiérarchie, un pour COMPOSANTS ») : la barre STANDARD du kit
				// (NkEditorScrollbar) — le composant arbre ne dessine pas de
				// barre par conception, il défile ; la barre du kit pilote LE
				// MÊME `modele.scroll`, donc chaque section défile seule.
				const float32 sbw = nkentseu::editorkit::NkScrollbarWidth();

				NkComponentInput in;
				in.surfaceScale = 1.f;
				in.mouseX = ctx.input.mousePos.x;
				in.mouseY = ctx.input.mousePos.y;
				in.wheel = ctx.input.wheel;
				in.mouseDown = ctx.input.mouseDown[0];
				in.mousePressed = ctx.input.mouseClicked[0];
				in.mouseReleased = ctx.input.mouseReleased[0];
				in.doubleClick = ctx.input.mouseDoubleClicked[0];
				in.rightPressed = ctx.input.mouseClicked[1];
				in.ctrl = ctx.input.ctrlDown;
				in.shift = ctx.input.shiftDown;

				NkDesignPaint paint(ctx, mSt->theme);
				// la colonne de droite est reservee a l'ascenseur de la section
				const NkPaintRect r = {zone.x, zone.y, zone.w - sbw, zone.h};
				// ── LE BADGE DE RÔLE EN PILULE (Banani §1.4) — par le point de
				//    greffe `rowOverlay` que le composant porte DÉJÀ (porte du
				//    28/08 : la couche du dessous d'abord — aucun kit à changer).
				//    `kindLabel` porte le NOM DU RÔLE (vide = pas de pilule).
				// ⚠️ UN SEUL CONTEXTE POUR TOUS LES CROCHETS, ET C'EST UN PLANTAGE
				//    PAYÉ (dump du 31/08 21:25, AV dans NkVector<NkTreeNode>::
				//    operator[] depuis rowOverlay). L'ancienne version REPOINTAIT
				//    `hooks.user` vers un second struct quand l'œil-barré était
				//    actif — mais `rowOverlay` castait toujours vers le premier :
				//    filtre allumé = pointeur sauvage, l'application tombait au
				//    premier badge de rôle dessiné. Deux crochets, deux structs,
				//    UN champ `user` : le partage était écrit nulle part. Un seul
				//    struct désormais — le compilateur n'a plus rien à confondre.
				struct Sur {
						NkGuiContext *ctx;
						NkTreeViewModel *modele;
						NkUIDocument *doc;
						DesignState *st;
						bool pages;
						// Le menu contextuel de CETTE surface, ouvert par le crochet
						// que le composant du kit emet deja.
						nkentseu::editorkit::NkCtxMenu *menu;
						int32 *menuNode;
				} sur{&ctx, &modele, &mSt->doc, mSt, &modele == &mModelePages, &mHierMenu,
					   &mHierMenuNode};
				NkTreeViewHooks hooks;
				hooks.user = &sur;
				// ── LE CLIC DROIT DE LA HIÉRARCHIE — ET LA CAPACITÉ ÉTAIT DÉJÀ EN
				//    DESSOUS (porte du 28/08, sixième occurrence de ce motif sur ce
				//    chantier, et la troisième trouvée AVANT d'écrire).
				// ⚠️ `NkTreeViewHooks::onContextMenu` existe depuis toujours dans le
				//    composant du kit, avec sa convention `index = -1` sur le fond du
				//    panneau — et il l'émet aux DEUX endroits (`NkTreeViewDraw.cpp`
				//    l. 675 et 722). NkUIDesign ne le branchait simplement pas : un
				//    clic droit dans la Hiérarchie ne faisait RIEN, sans un mot. Rien
				//    à faire grossir en dessous — il n'y avait qu'à s'en servir.
				hooks.onContextMenu = [](void *u, int32 ri, float32 mx, float32 my) {
					auto *s = static_cast<Sur *>(u);
					if (!s->pages) {
						// ON LE DIT PLUTÔT QUE D'OUVRIR UN MENU SANS OBJET : le
						// registre de composants n'est pas un document éditable.
						s->st->DireAuPied("Un composant du registre ne se modifie pas ici.");
						return;
					}
					if (ri < 0 || (uint32)ri >= (uint32)s->modele->nodes.Size()) {
						s->st->DireAuPied("Clic droit dans le vide — visez une ligne.");
						return;
					}
					const int32 di = (int32)s->modele->nodes[(uint32)ri].id - 1;
					if (!s->st->doc.IsValidIndex(di))
						return;
					// Lunacy : le clic droit sur une ligne NON sélectionnée la
					// sélectionne d'abord ; sur une ligne de la sélection, il garde
					// la sélection entière. LA MÊME règle que la toile.
					if (!s->st->sel.Contains(di))
						s->st->SelectSingle(di);
					*s->menuNode = di;
					s->menu->open = true;
					s->menu->pos = {mx, my};
				};
				// ── LE RENOMMAGE PAR LA HIERARCHIE ECRIT ENFIN LE DOCUMENT ───
				// ⚠️ MESURE DU 1er RETOUR (« les titres se desaccordent ») : le
				//    kit OUVRAIT deja la saisie au double-clic (NkTreeViewDraw
				//    l. 631), MAIS l'hote ne tenait ni le clavier ni le commit —
				//    la rangee restait figee sur son tampon pendant que la toile
				//    vivait : deux titres pour la meme page. Le contrat du kit
				//    (« l'hote, qui a le clavier, ecrit dans renameBuf ») est
				//    tenu ci-dessous (ClavierRenommage) ; ICI, le commit ecrit
				//    LA cle unique `label` — celle que SyncPages relit et que
				//    l'etiquette de toile dessine. Une cle, deux lecteurs.
				hooks.onRename = [](void *u, int32 ri, const char *, const char *,
									const char *nouveau) {
					auto *s = static_cast<Sur *>(u);
					if (!s->pages)
						return; // un composant du registre ne se renomme pas ici
					if (ri < 0 || (uint32)ri >= (uint32)s->modele->nodes.Size())
						return;
					const int32 di = (int32)s->modele->nodes[(uint32)ri].id - 1;
					if (!s->st->doc.IsValidIndex(di))
						return;
					s->st->doc.nodes[(uint32)di].label = NkString(nouveau ? nouveau : "");
					s->st->doc.MarkHumanEdit(di);
					s->st->DireAuPied("Renommé — l'étiquette de la toile lit la même clé.");
				};
				// L'ŒIL-BARRÉ (écran 8) : ne garder que les sous-arbres à rôle.
				if (mFiltreRoles && &modele == &mModelePages) {
					hooks.acceptNode = [](void *u, const NkTreeNode &n) -> bool {
						auto *s = static_cast<Sur *>(u);
						const int32 di = (int32)n.id - 1;
						if (!s->doc->IsValidIndex(di))
							return true;
						// le nœud, ou l'un de ses descendants, porte un rôle
						struct P {
								static bool Porte(const NkUIDocument &d, int32 i) {
									const NkUINode &nd = d.nodes[(nkentseu::uint32)i];
									if (!nd.role.Empty())
										return true;
									for (nkentseu::usize c = 0; c < nd.children.Size(); ++c)
										if (Porte(d, nd.children[(nkentseu::uint32)c]))
											return true;
									return false;
								}
						};
						return P::Porte(*s->doc, di);
					};
				}
				hooks.rowOverlay = [](void *u, nkentseu::editorkit::NkComponentPaint &p,
									  int32 index, float32 x, float32 y, float32, float32 h) {
					auto *s = static_cast<Sur *>(u);
					// Garde de bornes : un index de rangée hors du modèle ne doit
					// jamais déréférencer (la famille du plantage ci-dessus).
					if (index < 0 || (nkentseu::uint32)index >= (nkentseu::uint32)s->modele->nodes.Size())
						return;
					const NkTreeNode &n = s->modele->nodes[index];
					if (!n.kindLabel || !n.kindLabel[0])
						return;
					int32 depth = 0;
					for (int32 pa = n.parent; pa >= 0; pa = s->modele->nodes[pa].parent)
						++depth;
					const float32 lw = p.TextWidth(n.label.CStr());
					const float32 bx = x + 8.f + (float32)depth * 14.f + 13.f + 15.f + lw + 5.f;
					auto &F = costume::Fontes();
					// « hors page » n'est pas un role : pilule GRISE (l'accent
					// reste aux roles — 6e retour, volet B).
					const bool horsPage = NkComponentDecl::StrEq(n.kindLabel, "hors page");
					costume::BadgePilule(s->ctx->DL(), F.px9, bx, y + (h - 14.f) * 0.5f, 14.f,
										 n.kindLabel,
										 horsPage ? s->ctx->theme.textMuted
												  : s->ctx->theme.accent);
				};
				// L'hote tient le clavier de la saisie de renommage (contrat du
				// kit) — AVANT le dessin, pour que la frappe de cette image se
				// voie a cette image.
				ClavierRenommage(ctx, modele);

				const NkTreeViewResult res = nkentseu::editorkit::NkDrawTreeView(
					paint, in, r, modele, Style(inst), hooks);

				// Un composant du registre ne se renomme pas : la saisie ouverte
				// par le double-clic est refermee, ET LA RAISON SE DIT (jamais un
				// geste sans effet muet).
				if (&modele == &mModeleComposants && modele.renaming != 0) {
					modele.renameCancel = true;
					mSt->DireAuPied("Un composant du registre ne se renomme pas ici.");
				}

				// ── LE MÊME MENU QUE LA TOILE — pas une seconde écriture ───────
				// ⚠️ CE QUI DIFFÈRE ENTRE LES DEUX SURFACES EST L'APPLICABILITÉ,
				//    JAMAIS LE JEU D'ENTRÉES : `surfaceListe` fait basculer deux
				//    lignes (renommer agit ici, éditer le texte renvoie à la toile)
				//    et rien d'autre. Deux jeux auraient dérivé au premier ajout, et
				//    l'utilisateur aurait cherché dans un menu une commande qu'il
				//    venait de voir dans l'autre.
				if (mHierMenu.open && mSt->doc.IsValidIndex(mHierMenuNode)) {
					const NkActionCtx a =
						NkDessinerMenuCtx(ctx, mHierMenu, *mSt, mHierMenuNode, true, mHierFiltre,
										  (int32)sizeof(mHierFiltre), &mHierFiltreFocus);
					if (a == NkActionCtx::Renommer) {
						// LE RENOMMAGE NATIF DE L'ARBRE : le composant du kit le porte
						// déjà (`renaming`), et `onRename` écrit `label`. On l'ARME,
						// on ne le réécrit pas — le doubler serait la cinquième
						// occurrence du motif « la couche du dessous avait déjà
						// tranché », celle qu'on a déjà payée une fois.
						for (uint32 k = 0; k < (uint32)modele.nodes.Size(); ++k)
							if ((int32)modele.nodes[k].id - 1 == mHierMenuNode) {
								modele.renaming = modele.nodes[k].id;
								snprintf(modele.renameBuf, sizeof(modele.renameBuf), "%s",
										 modele.nodes[k].label.CStr());
								break;
							}
						mSt->DireAuPied("Renommage — Entrée valide, Échap annule.");
					} else if (a == NkActionCtx::EditerTexte) {
						// La saisie de texte n'a pas de point d'insertion dans une
						// liste : l'entrée est grisée et DIT où aller. Si elle arrive
						// quand même ici, on le redit plutôt que de ne rien faire.
						mSt->DireAuPied("L'édition du texte se fait dans la toile.");
					} else if (NkAppliquerActionCtx(*mSt, mHierMenuNode, a)) {
						mSt->DireAuPied(mSt->status.Data());
						if (a == NkActionCtx::Couper || a == NkActionCtx::Supprimer)
							mHierMenuNode = -1;
					}
					if (!mHierMenu.open)
						mHierFiltre[0] = 0; // le filtre ne survit pas a son menu
				} else if (mHierMenu.open) {
					mHierMenu.open = false; // la ligne visee a disparu
				}

				// L'ascenseur de CETTE section : il pilote le même `scroll` que la
				// molette du composant (une seule vérité de défilement), avec la
				// même arithmétique que le composant (visibleCount × row_h).
				{
					const float32 rowH = inst.Metric("row_h", 24.f);
					const float32 contentH = (float32)res.visibleCount * rowH;
					nkentseu::editorkit::NkVScrollbar(
						ctx, ctx.DL(), {zone.x + zone.w - sbw, zone.y, sbw, zone.h},
						modele.scroll, contentH, zone.h, ctx.GetId(cle) ^ 0x5C011Bu, rowH);
					// LA GEOMETRIE SE MESURE (2e retour du 01/09 : « le scrollbar
					// ne suit pas ») : hier.defile.<section> = [scroll, etendue,
					// fenetre, rangees emises]. scroll=0 doit montrer la premiere
					// rangee a zone.y ; scroll=max (etendue-fenetre) la derniere
					// entiere a zone.y+zone.h.
					char cleD[48];
					snprintf(cleD, sizeof(cleD), "hier.defile.%s", cle);
					nkgui::NkGuiNoterMesure(ctx, cleD, modele.scroll, contentH, zone.h,
											(float32)res.visibleCount);
				}

				// ⚠️ LA BOUCLE SE REFERME ICI, ET SON ABSENCE ETAIT UN DEFAUT REEL.
				//    `SyncPages` recopie `DesignState::selected` dans le modele a
				//    CHAQUE image. Sans la relecture ci-dessous, une selection faite
				//    dans l'arbre etait ECRASEE a l'image suivante : le clic avait
				//    l'air de ne rien faire, et l'Inspecteur affichait « Aucune
				//    selection » sur un arbre ou une ligne etait surlignee.
				//    Mesure : capture 08 -- « 5 nœud(s), 0 sélectionné(s) » cote
				//    PAGES pendant que l'Inspecteur disait « Aucune sélection ».
				// ⚠️ Et elle ne vaut QUE pour l'arbre des pages : les composants du
				//    projet ne sont pas des nœuds du document, leur index ne veut
				//    rien dire pour `DesignState::selected`.
				if (res.selectionChanged && &modele == &mModelePages) {
					// ⚠️ L'ID du modèle EST « index document + 1 » (posé par
					//    SyncPages) : c'est LUI qui traduit, plus l'index du
					//    modèle — la racine sautée a décalé les indices.
					//
					// ⚠️ ET C'EST LE COMPOSANT QUI FAIT FOI, PAS L'APPLICATION.
					//    Première version : je relisais `active` et je REDÉCIDAIS
					//    le geste avec `NkGesteListe`. Ça marchait pour Ctrl —
					//    et ça écrasait la PLAGE. Le composant du kit implémente
					//    déjà Maj+plage (`range_select`, actif par défaut, avec
					//    son `anchor`) : il avait calculé la plage entière dans
					//    `chosen`, et je n'en gardais qu'un nœud.
					//    **La règle : quand la couche du dessous a déjà décidé,
					//    on la SUIT, on ne re-décide pas.** Redécider, c'est se
					//    donner une seconde chance de diverger.
					mSt->sel.Clear();
					for (uint32 k = 0; k < (uint32)modele.chosen.Size(); ++k) {
						const int32 di = (int32)modele.chosen[k] - 1;
						if (di > 0 && mSt->doc.IsValidIndex(di))
							mSt->sel.Add(di);
					}
					if (modele.active > 0 && mSt->doc.IsValidIndex((int32)modele.active - 1)
						&& mSt->sel.Empty())
						mSt->sel.Add((int32)modele.active - 1);
					mSt->selected = mSt->sel.Empty() ? -1 : mSt->sel.Primary();
					// ⚠️ `mSt->selected` doit rester le nœud ACTIF du composant
					//    quand il est dans la sélection : c'est lui que
					//    l'Inspecteur nomme, et le composant sait lequel la main
					//    vient de toucher.
					if (modele.active > 0 && mSt->sel.Contains((int32)modele.active - 1))
						mSt->selected = (int32)modele.active - 1;
				}
			}

			/// Le style, LU DANS LA DÉCLARATION. Aucun nom de rôle n'est écrit ici :
			/// `TokenRole` rend celui que la déclaration nomme, et
			/// `NkDesignResolveRole` est la résolution UNIQUE du programme — celle
			/// que la sonde emprunte aussi.
			NkTreeViewStyle Style(const NkComponentInstance &inst) const {
				NkTreeViewStyle s;
				s.panelBg = NkDesignResolveRole(inst.TokenRole("panel_bg"));
				s.headerBg = NkDesignResolveRole(inst.TokenRole("header_bg"));
				s.border = NkDesignResolveRole(inst.TokenRole("border"));
				s.text = NkDesignResolveRole(inst.TokenRole("text"));
				s.textMuted = NkDesignResolveRole(inst.TokenRole("text_muted"));
				s.rowHover = NkDesignResolveRole(inst.TokenRole("row_hover"));
				s.activeMark = NkDesignResolveRole(inst.TokenRole("active_mark"));
				s.activeText = NkDesignResolveRole(inst.TokenRole("active_text"));
				s.chosenMark = NkDesignResolveRole(inst.TokenRole("chosen_mark"));
				s.guide = NkDesignResolveRole(inst.TokenRole("guide"));
				s.dropMark = NkDesignResolveRole(inst.TokenRole("drop_mark"));
				s.iconTint = NkDesignResolveRole(inst.TokenRole("icon_tint"));
				s.dimTint = NkDesignResolveRole(inst.TokenRole("dim_tint"));
				s.icons = NkDesignTreeIcons();
				s.values = &inst;
				return s;
			}

			void CopyFiltre(char *dst, nkentseu::usize n) const {
				nkentseu::usize i = 0;
				while (i + 1 < n && mFiltre[i]) {
					dst[i] = mFiltre[i];
					++i;
				}
				dst[i] = '\0';
			}

			// ── LE DOCUMENT VERS LE MODÈLE DE L'ARBRE ────────────────────────
			// ⚠️ ORDRE PRÉFIXE OBLIGATOIRE (`NkTreeViewModel::IsWellFormed`). Les
			//    nœuds du document sont déjà en ordre préfixe (un parent précède
			//    ses enfants) : la copie est donc directe, et la précondition est
			//    VÉRIFIÉE plutôt que supposée.
			void SyncPages() {
				mModelePages.nodes.Clear();
				const uint32 n = (uint32)mSt->doc.nodes.Size();
				// COSTUME BANANI : la RACINE ne s'affiche pas (Lunacy et la maquette
				// commencent aux PAGES ; la racine se surligne ailleurs). Les ids
				// restent i+1 — seule la parenté du modèle saute l'étage racine.
				for (uint32 i = 0; i < n; ++i) {
					const NkUINode &d = mSt->doc.nodes[i];
					if (d.parent < 0)
						continue; // la racine
					NkTreeNode t;
					t.id = (nkentseu::nk_uint64)(i + 1);
					// ⚠️ `parent` du modèle = INDEX DE MODÈLE : la racine sautée
					//    décale tout d'un cran (doc i -> modèle i-1).
					t.parent = (mSt->doc.IsValidIndex(d.parent)
								&& mSt->doc.nodes[(uint32)d.parent].parent < 0)
								   ? -1
								   : d.parent - 1;
					t.label = d.label.Empty() ? NkString(d.component.Empty() ? "(cadre)"
																			: d.component.Data())
											  : d.label;
					t.path = t.label;
					// COSTUME BANANI : `kindLabel` porte le NOM DU RÔLE du nœud —
					// c'est lui que la pilule affiche (vide = pas de pilule).
					t.kindLabel = d.role.Empty() ? "" : d.role.Data();
					// ── « HORS PAGE » (6e retour, volet B) : un element POSE A LA
					//    RACINE de la toile (permis — modele Figma/Lunacy) n'est
					//    couvert ni par la transposition ni par l'export par page.
					//    CA DOIT SE VOIR : pilule discrete « hors page » (grise,
					//    pas accent — rowOverlay la distingue d'un role).
					if (d.role.Empty() && mSt->doc.IsValidIndex(d.parent)
						&& mSt->doc.nodes[(uint32)d.parent].parent < 0
						&& !NkComponentDecl::StrEq(d.shape.Data(), "frame"))
						t.kindLabel = "hors page";
					// L'icône de NATURE (tracés du JSX) : page pour un artboard,
					// « T » pour un texte, pilule-bouton pour un élément à rôle,
					// panneau pour le reste. Teinte : accent quand le nœud porte
					// un rôle ou est un artboard ouvert — la maquette teinte les
					// deux —, `text_muted` sinon.
					const bool artboard = NkComponentDecl::StrEq(d.shape.Data(), "frame");
					const bool texte = NkComponentDecl::StrEq(d.shape.Data(), "text");
					if (!d.role.Empty())
						t.icon = NK_ICON_NATURE_BOUTON;
					else if (artboard)
						t.icon = NK_ICON_NATURE_PAGE;
					else if (texte)
						t.icon = NK_ICON_NATURE_TEXTE;
					else
						t.icon = NK_ICON_NATURE_PANNEAU;
					t.kindRole = NkDesignResolveRole(
						(!d.role.Empty() || artboard) ? "accent_ui" : "text_muted");
					mModelePages.nodes.PushBack(t);
				}
				// ⚠️ LES PLAFONDS CRIENT, ILS NE DÉBORDENT PAS EN SILENCE.
				//    `IsWellFormed` refuse au-delà de `kMaxDepth` (64) et sur un
				//    ordre non préfixe. Un modèle mal formé ne PLANTE pas le
				//    dessin — il s'affiche dans un ordre surprenant, ce qui
				//    ressemble à un défaut de l'arbre et non du document.
				if (!mModelePages.nodes.Empty() && !mModelePages.IsWellFormed() && !mCriPages) {
					mCriPages = true;
					logger.Error("[NKUIDesign] Hiérarchie : le document n'est PAS en ordre préfixe "
								 "ou dépasse kMaxDepth={0}. L'arbre s'affichera dans un ordre "
								 "surprenant -- ce n'est pas un défaut du composant.",
								 (int32)NkTreeViewModel::kMaxDepth);
				}
				// COSTUME BANANI (31/08) : les SOUS-CONTENEURS d'un artboard
				// démarrent repliés — la maquette montre des pages dépliées à
				// leurs enfants directs, pas l'arborescence entière (les douze
				// barres du graphique noieraient l'arbre). UNE fois : l'état
				// d'ouverture appartient ensuite à l'utilisateur.
				if (mPlierUneFois && !mModelePages.nodes.Empty()) {
					mPlierUneFois = false;
					const uint32 nd = (uint32)mSt->doc.nodes.Size();
					for (uint32 i = 0; i < nd; ++i) {
						const NkUINode &d = mSt->doc.nodes[i];
						if (d.children.Empty() || d.parent < 0)
							continue;
						if (mSt->doc.nodes[(uint32)d.parent].parent >= 0)
							mModelePages.SetOpen((nkentseu::nk_uint64)(i + 1), false, true);
					}
				}
				// La sélection est UNE (§11.5) : elle vit dans `DesignState`.
				// ⚠️ ET LA HIÉRARCHIE REFLÈTE DÉSORMAIS LA MULTI-SÉLECTION. Cette
				//    ligne ne poussait que le PRINCIPAL dans `chosen` : sélectionner
				//    trois éléments sur la toile n'en surlignait qu'UN dans l'arbre,
				//    et le va-et-vient restait borgne dans ce sens-là aussi.
				// ⚠️ ET `chosen` EXISTAIT DÉJÀ DANS LE MODÈLE DU KIT, avec son
				//    `anchor` de plage Maj+clic. Rien à faire grossir en dessous :
				//    la capacité était là, l'application ne s'en servait pas —
				//    troisième cas de la semaine pour la porte du 28/08 (chercher
				//    qui porte déjà la chose, en commençant par la couche du
				//    dessous).
				mModelePages.active =
					mSt->selected >= 0 ? (nkentseu::nk_uint64)(mSt->selected + 1) : 0;
				mModelePages.chosen.Clear();
				for (uint32 k = 0; k < (uint32)mSt->sel.items.Size(); ++k) {
					const int32 si = mSt->sel.items[k];
					if (si > 0 && mSt->doc.IsValidIndex(si))
						mModelePages.chosen.PushBack((nkentseu::nk_uint64)(si + 1));
				}
				if (mModelePages.chosen.Empty() && mModelePages.active)
					mModelePages.chosen.PushBack(mModelePages.active);
			}

			/// ⚠️ AUCUN CATALOGUE ÉCRIT EN DUR, ET C'EST UNE CONTRAINTE DE RODOLF :
			///    « des millions d'utilisateurs peuvent créer des composants, les
			///    commercialiser ou les partager. » La section basse boucle donc sur
			///    le REGISTRE et ne nomme rien.
			///    ⚠️ `NkComponentRegistry` est borné à `kMaxComponents` = 64. Le
			///       registre dynamique est un chantier à part, non commencé ici ;
			///       ce qui est fait aujourd'hui, c'est que la borne se DISE.
			void SyncComposants() {
				mModeleComposants.nodes.Clear();
				const uint16 n = NkComponentRegistry::Count();
				for (uint16 c = 0; c < n; ++c) {
					const NkComponentDecl *d = NkComponentRegistry::At(c);
					if (!d)
						continue;
					NkTreeNode t;
					t.id = (nkentseu::nk_uint64)(c + 1);
					t.parent = -1;
					// ⚠️ `name` ET NON `title` : `title` est une CLÉ DE TRADUCTION
					//    (« content_browser.title »), pas un libellé. L'afficher
					//    telle quelle mettrait une clé sous les yeux de
					//    l'utilisateur. Le multilingue vit dans NKGui (règle du
					//    18/08) et n'est pas branché ici.
					t.label = NkString(d->name ? d->name : "");
					t.path = NkString(d->name ? d->name : "");
					// COSTUME BANANI : pas de pilule pour un composant sans rôle
					// (`kindLabel` = le rôle, désormais) ; icône panneau, teinte
					// `text_muted` — la maquette réserve l'accent aux rôles.
					t.kindLabel = "";
					t.icon = NK_ICON_NATURE_PANNEAU;
					t.kindRole = NkDesignResolveRole("text_muted");
					mModeleComposants.nodes.PushBack(t);
				}
				if (n >= 64 && !mCriRegistre) {
					mCriRegistre = true;
					logger.Error("[NKUIDesign] le registre de composants est PLEIN ({0} entrées, "
								 "plafond kMaxComponents). Les composants suivants ne sont ni "
								 "enregistrés ni affichés -- ce n'est pas un filtre de la "
								 "Hiérarchie.",
								 (int32)n);
				}
			}

			DesignState *mSt;
			NkTreeViewModel mModelePages;
			NkTreeViewModel mModeleComposants;
			NkComponentInstance mInstPages;
			NkComponentInstance mInstComposants;
			char mFiltre[128] = {0};
			bool mFiltreVisible = false; // la loupe déplie le filtre (costume Banani)
			bool mFiltreRoles = false;
			// ── LE MENU CONTEXTUEL DE LA HIÉRARCHIE ──────────────────
			// ⚠️ UN ÉTAT PAR SURFACE, ET PAS UN ÉTAT PARTAGÉ : la toile et la
			//    Hiérarchie peuvent être ouvertes en même temps, et deux menus
			//    ouverts sur un seul état se ferment l'un l'autre. Le CONTENU est
			//    partagé (`NkConstruireMenuCtx`), la POSITION ne l'est pas.
			nkentseu::editorkit::NkCtxMenu mHierMenu;
			int32 mHierMenuNode = -1;
			char mHierFiltre[48] = {};
			bool mHierFiltreFocus = true;
			float32 mHBasVoulu = -1.f;	 // la part de COMPOSANTS choisie a la poignee (ecran 10)
			bool mHBasLu = false;		 // hier_bas deja lu dans nkuidesign.cfg ?
			bool mPoigneeActive = false;
			float32 mPoigneeY = 0.f;	 // œil-barré : seulement les éléments à rôle (écran 8)
			bool mPlierUneFois = true;	 // repli initial des sous-conteneurs (une fois)
			bool mCriPages = false;
			bool mCriRegistre = false;
	};

	// ═══════════════════════════════════════════════════════════════════════════
	//  PANNEAU 8 — L'INSPECTEUR (document 3 §12, planche 091913)
	// ═══════════════════════════════════════════════════════════════════════════
	//  ⚠️ CONTRE-ÉPREUVE, ET ELLE CONTREDIT LA CONSIGNE — MESURÉE, PAS SUPPOSÉE.
	//     La consigne disait « via `NkEditorInspector` du kit ». Lecture faite du
	//     fichier (`Engine/NKEditorKit/src/NKEditorKit/NkEditorInspector.h`,
	//     141 lignes) : il expose UNE fonction, `DrawInspector(ctx, obj, cls)`,
	//     qui énumère les propriétés d'une classe **enregistrée dans
	//     NKReflection** (`EnumerateEditableProperties`, `SetPropertyByName`).
	//     Il ne porte ni en-tête, ni onglets, ni sections nommées.
	//
	//     Or un nœud de NkUIDesign (`NkUINode`, `Document.h`) n'est PAS une
	//     classe réfléchie — il n'y a ni `NK_CLASS`, ni enregistrement. L'appeler
	//     ici demanderait d'abord de réfléchir le document, ce que le CLAUDE.md
	//     parent range explicitement de l'autre côté de la frontière tracée le
	//     18/08 (NKReflection = objets de DONNÉES à l'exécution ; la déclaration
	//     de composant = constante de compilation).
	//
	//     Donc : **`NkEditorInspector` n'est pas contourné, il ne s'applique pas
	//     encore.** Ce panneau assemble les primitives NKGui — `TabBar`,
	//     `CollapsingHeader`, `BeginRow` — exactement celles que `DrawInspector`
	//     utilise lui-même à l'intérieur. Le jour où le document porte de la
	//     réflexion, le CORPS d'une section devient un appel à `DrawInspector`
	//     sans que la charpente bouge.
	//     📌 ELLE EXISTE DEPUIS LE 2026-08-29 :
	//        `NKEditorKit/NkEditorInspectorFrame.h`. Ce panneau ne dessine plus
	//        sa charpente, il la DÉCRIT et la laisse dessiner.
	//        ⚠️ LE SIGNALEMENT AVAIT ÉTÉ ÉCRIT ICI, ET IL N'A RIEN DÉCLENCHÉ.
	//           Il disait déjà, mot pour mot, « il manque au kit une charpente
	//           d'inspecteur […] elle n'existe pas » — pendant que la charpente
	//           était écrite ici, et deux fois de plus chez Nogee. **Un
	//           signalement n'est pas un remède** : celui-là a tenu des semaines
	//           pendant que trois applications payaient. C'est une cause
	//           DIFFÉRENTE de celle de la conversion de thème (qui, elle,
	//           existait mais exigeait une coquille pour être appelée).
	//
	//  L'ORDRE DES SECTIONS EST NORMATIF (§12.2) : « un ordre laissé au hasard se
	//  met à varier d'un écran à l'autre ». Il est donc écrit UNE fois, dans une
	//  table, et la boucle le suit.
	// ═══════════════════════════════════════════════════════════════════════════
	//  L'ORDRE DES SECTIONS DE L'INSPECTEUR — UN MÉCANISME, PAS UN DESSIN
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ CETTE LISTE VIVAIT DANS `InspectorPanel::Sections`, DONC HORS DE PORTÉE
	///    D'UN BANC — la facture que ce chantier a déjà payée trois fois (le zoom
	///    en Q41, l'aimantation en Q42, le tracé ce matin). Or ce qu'elle décide
	///    est exactement ce que Rodolf a demandé à voir : **quel panneau droit
	///    s'affiche dans quel état**. Tant qu'elle était écrite dans le corps du
	///    dessin, « EDIT SHAPE remplace la géométrie » ne pouvait se vérifier
	///    qu'à l'œil, sur une capture, une fois par jour.
	///
	/// ⚠️ ET C'EST UN **REMPLACEMENT**, PAS UN AJOUT — relu au pixel sur
	///    `lunacy_2temps_edition_181745.png`. Laisser « Position 86,13 / 423,76 »
	///    à côté des coordonnées d'un SOMMET donnerait deux X et deux Y dans le
	///    même panneau, sans rien qui dise lequel parle de quoi. LAYER / FILLS /
	///    BORDERS / EFFECTS / PROTOTYPING, eux, restent : chez Lunacy comme chez
	///    nous, ils décrivent l'OBJET, pas sa géométrie.
	/// @return le nombre de sections écrites dans `out`.
	inline uint32 NkSectionsInspecteur(bool modeForme, bool cadreCible,
									   const char *const *&out) {
		static const char *const kForme[] = {
			"ÉDITION DE FORME", "REMPLISSAGES", "BORDURES",
			"APPARENCE",		"EFFETS",		"POINTS DE RUPTURE",
		};
		static const char *const kAvecCible[] = {
			"CIBLE",	  "DISPOSITION", "ANCRAGE",	 "ALIGNEMENT",
			"ESPACEMENT", "REMPLISSAGES", "BORDURES", "APPARENCE",
			"TYPOGRAPHIE", "EFFETS",	 "POINTS DE RUPTURE",
		};
		static const char *const kNormal[] = {
			"DISPOSITION", "ANCRAGE",	 "ALIGNEMENT", "ESPACEMENT",
			"REMPLISSAGES", "BORDURES",	 "APPARENCE",  "TYPOGRAPHIE",
			"EFFETS",	   "POINTS DE RUPTURE",
		};
		// ⚠️ LE MODE PRIME SUR LA CIBLE, ET L'ORDRE DES TROIS `if` EST LA RÈGLE :
		//    un artboard n'entre pas en édition de forme (il n'est pas une forme
		//    éditable), mais une forme DANS un artboard porte une cible héritée —
		//    tester la cible d'abord aurait rendu le panneau de géométrie pendant
		//    qu'on édite des sommets.
		if (modeForme) {
			out = kForme;
			return (uint32)(sizeof(kForme) / sizeof(kForme[0]));
		}
		if (cadreCible) {
			out = kAvecCible;
			return (uint32)(sizeof(kAvecCible) / sizeof(kAvecCible[0]));
		}
		out = kNormal;
		return (uint32)(sizeof(kNormal) / sizeof(kNormal[0]));
	}

	class InspectorPanel : public NkEditorPanel {
		public:
			explicit InspectorPanel(DesignState *st)
				: NkEditorPanel("Inspecteur", NkEditorDockSide::NK_RIGHT), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				designkit::releve::Zone(ctx, "inspecteur");

				// ⚠️ CE PANNEAU NE DESSINE PLUS SA CHARPENTE, IL LA DÉCRIT.
				//    En-tête, onglets et boucle de sections vivent dans
				//    `NKEditorKit/NkEditorInspectorFrame.h` ; il ne reste ici que
				//    ce qui est PROPRE à NkUIDesign — quelles sections, dans quel
				//    ordre, et ce que chacune affiche.
				//
				// ⚠️ ET L'ONGLET COURANT N'EST PLUS GARDÉ ICI. `TabBarEx` le
				//    tient, persistant par son id ; `mOnglet` ne sert plus qu'à
				//    RECEVOIR ce que la charpente a décidé, pour le lire ailleurs.
				//    Une seconde copie aurait donné deux vérités sur « quel onglet
				//    est ouvert » — c'est le doublon d'état que la charpente
				//    refuse par construction.
				// ⚠️ LES SECTIONS DEMARRENT DEPLIEES, UNE FOIS. Le plan (§12.2)
				//    les veut ouvertes par defaut ; `CollapsingHeader` demarre
				//    ferme, et son etat appartient au contexte. On le POSE donc au
				//    premier affichage -- une seule fois : l'utilisateur qui
				//    replie ensuite garde sa main, on ne rouvre pas derriere lui.
				//    (Meme pile d'id que l'appel de la charpente juste en dessous,
				//    donc memes identifiants -- c'est ce qui rend le geste sur.)
				// (Le dépliage initial par CollapsingHeader est PARTI avec le
				// costume : les sections sont non repliables — la maquette ne
				// replie que certaines sections — l'état par section vit dans `mSections`.)
				(void)mDeplierUneFois;
				editorkit::NkInspectorCharpente ch;
				ch.user = this;
				ch.entete = Nom();
				ch.enteteVide = "Aucune sélection";
				ch.onglets = Onglets();
				ch.ongletCount = 3;
				ch.idOnglets = "insp.onglets";
				ch.sectionsDe = &SectionsDe;
				ch.messageOngletVide = &MessageOngletVide;
				// COSTUME BANANI (remandat 31/08) : les trois crochets de dessin
				// opt-in de la charpente — en-tete 34 px a icone, onglets 11 px au
				// lisere de 2 px, titres de section en MAJUSCULES 9 px + filet.
				ch.dessineEntete = [](void *u, NkGuiContext &c, const char *nom) {
					static_cast<InspectorPanel *>(u)->EnteteCostume(c, nom);
				};
				ch.dessineOnglets = [](void *u, NkGuiContext &c) -> int32 {
					return static_cast<InspectorPanel *>(u)->OngletsCostume(c);
				};
				ch.dessineTitreSection = [](void *u, NkGuiContext &c, const char *t) {
					static_cast<InspectorPanel *>(u)->TitreSection(c, t);
				};
				if (mSt->ongletInitial >= 0 && mSt->ongletInitial <= 2) {
					mOnglet = mSt->ongletInitial; // mise en scene : l'onglet demande
					mSt->ongletInitial = -1;
				}
				mOnglet = editorkit::NkInspectorDessiner(ctx, ch);
				// LE RELEVE DE LA ZONE DEFILANTE (meme contrat que hier.defile.*) :
				// l'etat vit dans le magasin public du contexte, sous l'id que la
				// charpente derive de `idOnglets`. inspecteur.defile = [scroll,
				// max, 0, 0].
				{
					const nkgui::NkGuiId idDef = ctx.GetId("insp.onglets") ^ 0x5EC7104u;
					for (uint32 k = 0; k < (uint32)ctx.scrollKeys.Size(); ++k)
						if (ctx.scrollKeys[k] == idDef) {
							nkgui::NkGuiNoterMesure(ctx, "inspecteur.defile",
													ctx.scrollVals[k].y, ctx.scrollVals[k].maxY,
													0.f, 0.f);
							break;
						}
				}
			}

		private:
			// ── CE QUE LA CHARPENTE VIENT CHERCHER ICI ──────────────────────
			static const char *const *Onglets() noexcept {
				static const char *const kOnglets[3] = {"Design", "Widget", "Behavior"};
				return kOnglets;
			}

			const char *Nom() const noexcept {
				if (!mSt->doc.IsValidIndex(mSt->selected))
					return nullptr; // -> `enteteVide`
				const NkUINode &n = mSt->doc.nodes[(uint32)mSt->selected];
				return n.label.Empty() ? "(sans nom)" : n.label.Data();
			}

			/// L'ORDRE DES SECTIONS EST NORMATIF (§12.2) : « un ordre laissé au
			/// hasard se met à varier d'un écran à l'autre ». Il est écrit UNE
			/// fois, ici, et la charpente le suit.
			static const editorkit::NkInspectorSection *SectionsDe(void *user, int32 onglet,
																   int32 &count) noexcept {
				// ⚠️ UN ONGLET SANS CONTENU REND ZÉRO SECTION, il ne rend pas
				//    sept sections vides. C'est la charpente qui dira quoi
				//    afficher à la place (§12.2) — voir `MessageOngletVide`.
				// L'ONGLET WIDGET : la section RÔLE (écrans 5-6-7 — le geste
				// « promouvoir ») dès qu'un nœud est sélectionné.
				if (onglet == 1) {
					auto *self = static_cast<InspectorPanel *>(user);
					if (self->NoeudCourant()) {
						static const editorkit::NkInspectorSection kWidget[] = {
							{"RÔLE", &CorpsRoleC, false},
						};
						count = 1;
						return kWidget;
					}
				}
				// L'ONGLET BEHAVIOR (écran 4 Banani) : deux sections quand la
				// sélection porte un RÔLE — le vocabulaire d'événements du rôle,
				// et les événements ajoutés. Sans rôle : le message historique.
				if (onglet == 2) {
					auto *self = static_cast<InspectorPanel *>(user);
					const NkUINode *n = self->NoeudCourant();
					if (n && !n->role.Empty()) {
						static const editorkit::NkInspectorSection kBehavior[] = {
							{"ÉVÉNEMENTS DU RÔLE", &CorpsEvenementsRoleC, false},
							{"ÉVÉNEMENTS AJOUTÉS", &CorpsEvenementsAjoutesC, false},
						};
						count = (int32)(sizeof(kBehavior) / sizeof(kBehavior[0]));
						return kBehavior;
					}
				}
				if (onglet != 0) {
					count = 0;
					return nullptr;
				}
				// LA SECTION « CIBLE » (écran 3 Banani, sa seule nouveauté qui ne
				// contredit pas l'InspecteurV2 de l'écran 1) : PREMIÈRE, et
				// seulement quand la sélection est un ARTBOARD à cible — les
				// autres nœuds gardent la table de l'écran 1 telle quelle.
				// ⚠️ LA TABLE SUIT reference_4_185519.png A LA LETTRE (2e passe de
				//    Rodolf, 31/08 : « regarde comment les proprietes de position,
				//    taille, ancrage, alignement, espacement, apparence,
				//    typographie etc. sont definies »). CIBLE montre la cible du
				//    CADRE ENGLOBANT (la reference l'affiche sur un bouton) ;
				//    DISPOSITION reunit Position + Largeur/Hauteur ; BORDS a
				//    fusionne dans APPARENCE (Bordure/Arrondi) ; EFFETS et POINTS
				//    DE RUPTURE existent, replies, et DISENT que leur modele
				//    arrive. Toutes les sections se replient au chevron
				//    (etat dans mSections) — la maquette replie ESPACEMENT,
				//    EFFETS et POINTS DE RUPTURE par defaut.
				// ── LE MODE ÉDITION DE FORME REMPLACE LA GÉOMÉTRIE ─────────────
				// ⚠️ RELU AU PIXEL SUR `lunacy_2temps_edition_181745.png` : `EDIT SHAPE`
				//    prend la place des rangées X/Y/W/H et de la barre d'alignement,
				//    tandis que LAYER / FILLS / BORDERS / EFFECTS / PROTOTYPING restent
				//    en dessous, inchangés. On fait pareil : REMPLISSAGES, BORDURES,
				//    APPARENCE, EFFETS et POINTS DE RUPTURE survivent ; DISPOSITION,
				//    ANCRAGE, ALIGNEMENT, ESPACEMENT, TYPOGRAPHIE et CIBLE se taisent.
				// ⚠️ ET C'EST UN REMPLACEMENT, PAS UN AJOUT. Laisser « Position 86,13 /
				//    423,76 » à côté des coordonnées d'un SOMMET donnerait deux X et deux
				//    Y dans le même panneau, sans rien qui dise lequel parle de quoi.
				{
					auto *self = static_cast<InspectorPanel *>(user);
					if (self->mSt && self->mSt->modeForme.Actif()
						&& self->mSt->modeForme.noeud == self->mSt->selected) {
						// ⚠️ LES NOMS VIENNENT DE `NkSectionsInspecteur`, LITTÉRALEMENT --
						//    pas d'une copie qu'une assertion surveillerait. Le panneau
						//    n'associe qu'un CORPS à chaque nom ; la décision « quelles
						//    sections, dans quel ordre » vit dans la fonction libre, où
						//    un banc l'atteint sans fenêtre. Écrite deux fois, elle
						//    aurait été corrigée une seule au premier ajustement.
						static editorkit::NkInspectorSection kForme[6];
						static bool formePret = false;
						if (!formePret) {
							static void (*const corps[6])(void *, NkGuiContext &) = {
								&CorpsEditionFormeC, &CorpsRemplissagesC, &CorpsBorduresC,
								&CorpsApparenceC,	 &CorpsEffetsC,		  &CorpsRuptureC};
							const char *const *noms = nullptr;
							const uint32 nb = NkSectionsInspecteur(true, false, noms);
							for (uint32 i = 0; i < 6 && i < nb; ++i) {
								kForme[i].titre = noms[i];
								kForme[i].corps = corps[i];
								kForme[i].repliable = false;
							}
							formePret = true;
						}
						count = (int32)(sizeof(kForme) / sizeof(kForme[0]));
						return kForme;
					}
				}
				{
					auto *self = static_cast<InspectorPanel *>(user);
					if (self->CadreCible()) {
						static const editorkit::NkInspectorSection kAvecCible[] = {
							{"CIBLE", &CorpsCibleC, false},
							{"DISPOSITION", &CorpsDispositionC, false},
							{"ANCRAGE", &CorpsAncrageC, false},
							{"ALIGNEMENT", &CorpsAlignementC, false},
							{"ESPACEMENT", &CorpsEspacementC, false},
							{"REMPLISSAGES", &CorpsRemplissagesC, false},
							{"BORDURES", &CorpsBorduresC, false},
							{"APPARENCE", &CorpsApparenceC, false},
							{"TYPOGRAPHIE", &CorpsTypographieC, false},
							{"EFFETS", &CorpsEffetsC, false},
							{"POINTS DE RUPTURE", &CorpsRuptureC, false},
						};
						count = (int32)(sizeof(kAvecCible) / sizeof(kAvecCible[0]));
						return kAvecCible;
					}
				}
				static const editorkit::NkInspectorSection kSections[] = {
					{"DISPOSITION", &CorpsDispositionC, false},
					{"ANCRAGE", &CorpsAncrageC, false},
					{"ALIGNEMENT", &CorpsAlignementC, false},
					{"ESPACEMENT", &CorpsEspacementC, false},
					{"REMPLISSAGES", &CorpsRemplissagesC, false},
					{"BORDURES", &CorpsBorduresC, false},
					{"APPARENCE", &CorpsApparenceC, false},
					{"TYPOGRAPHIE", &CorpsTypographieC, false},
					{"EFFETS", &CorpsEffetsC, false},
					{"POINTS DE RUPTURE", &CorpsRuptureC, false},
				};
				count = (int32)(sizeof(kSections) / sizeof(kSections[0]));
				return kSections;
			}

			/// L'INDEX du cadre-page qui contient la selection (ou la selection
			/// elle-meme) : la reference affiche « Cible du cadre » meme sur un
			/// bouton — c'est la cible de l'artboard englobant qu'elle montre.
			/// Un cadre de PREMIER NIVEAU compte MEME SANS cible posee (31/08,
			/// catalogue de formats : une page fraichement tracee doit pouvoir
			/// en choisir un). -1 si la selection n'est dans aucune page.
			int32 CadreCibleIndex() const {
				int32 idx = mSt->selected;
				int32 garde = 0;
				while (mSt->doc.IsValidIndex(idx) && ++garde < 64) {
					const NkUINode &n = mSt->doc.nodes[(uint32)idx];
					if (StrEq(n.shape.Data(), "frame")) {
						if (!n.target.Empty())
							return idx;
						// premier niveau (enfant de la racine) : page sans format
						if (mSt->doc.IsValidIndex(n.parent)
							&& mSt->doc.nodes[(uint32)n.parent].parent < 0)
							return idx;
					}
					idx = n.parent;
				}
				return -1;
			}
			const NkUINode *CadreCible() const {
				const int32 i = CadreCibleIndex();
				return i >= 0 ? &mSt->doc.nodes[(uint32)i] : nullptr;
			}

			static const char *MessageOngletVide(void *, int32 onglet) noexcept {
				return onglet == 1 ? "Onglet Widget : le rôle et ses paramètres — pas encore branché."
								   : "Onglet Behavior : événements et callbacks — pas encore branché.";
			}

			// ═══════════════════════════════════════════════════════════════════
			//  LE COSTUME BANANI DE LA CHARPENTE (InspectorPanelV2, JSX)
			// ═══════════════════════════════════════════════════════════════════
			/// L'en-tête de la référence (reference_4_185519) : TUILE 28×28
			/// arrondie (voile accent 13 %) portant l'icône de nature, nom 13 px
			/// gras, sous-titre « Rôle : <rôle> » 10 px atténué quand un rôle est
			/// porté. Filet bas.
			void EnteteCostume(NkGuiContext &ctx, const char *nom) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkUINode *n = NoeudCourant();
				const bool sousTitre = n && !n->role.Empty();
				const float32 h = 48.f;
				const NkRect r = ctx.NextItemRect(-1.f, h);
				// La tuile
				const NkRect tuile = {r.x + 12.f, r.y + (h - 28.f) * 0.5f, 28.f, 28.f};
				{
					NkColor voile = ctx.theme.accent;
					voile.a = 34;
					dl.AddRectFilled(tuile, voile, 6.f);
					const float32 ix = tuile.x + (28.f - 11.f) * 0.5f;
					const float32 iy = tuile.y + (28.f - 11.f) * 0.5f;
					if (sousTitre)
						costume::IcBouton(dl, ix, iy, ctx.theme.accent);
					else if (n && StrEq(n->shape.Data(), "frame"))
						costume::IcPage(dl, ix, iy, ctx.theme.accent);
					else if (n && StrEq(n->shape.Data(), "text"))
						costume::IcTexte(dl, ix, iy, ctx.theme.accent);
					else
						costume::IcPanneau(dl, ix, iy, ctx.theme.accent);
				}
				const float32 tx = tuile.x + 28.f + 10.f;
				if (sousTitre) {
					costume::TexteGras(dl, F.px13, tx, r.y + 7.f, nom, ctx.theme.text, 0.4f);
					char st[80];
					snprintf(st, sizeof(st), "Rôle : %s", n->role.Data());
					costume::Texte(dl, F.px10, tx, r.y + 26.f, st, ctx.theme.textMuted);
				} else
					costume::TexteGras(dl, F.px13, tx, costume::CentrerY(F.px13, r.y, h), nom,
									   ctx.theme.text, 0.4f);
				// ── LE DÉCLENCHEUR DE SIMULATION (Rodolf, 31/08 : « lancer la
				//    simulation depuis la page sélectionnée, si simulation
				//    définie ») — en haut du panneau droit. AUCUN modèle de
				//    simulation n'existe encore : le triangle est GRIS, jamais
				//    absent, jamais muet — le clic DIT la raison exacte et ouvre
				//    le panneau Simulation (écran 16, l'état vide honnête). Le
				//    chantier « exécution » (§4.9 : callbacks factices
				//    journalisés) est nommé au rapport.
				{
					const NkRect rp = {r.x + r.w - 34.f, r.y + (h - 22.f) * 0.5f, 22.f, 22.f};
					dl.AddRectFilled(rp, ctx.theme.button, 4.f);
					dl.AddRect(rp, ctx.theme.border, 1.f, 4.f);
					const float32 cxp = rp.x + rp.w * 0.5f - 1.f, cyp = rp.y + rp.h * 0.5f;
					dl.AddTriangleFilled({cxp - 3.f, cyp - 5.f}, {cxp - 3.f, cyp + 5.f},
										 {cxp + 5.f, cyp}, ctx.theme.textDisabled);
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(rp, ctx.input.mousePos)) {
						mSt->status = NkString(
							"Aucune simulation définie pour cette page — le panneau "
							"Simulation dit ce que le mode comptera (exécution : §4.9).");
						mSt->ouvrirSimulation = true;
					}
				}
				dl.AddLine({r.x, r.y + h - 0.5f}, {r.x + r.w, r.y + h - 0.5f}, ctx.theme.border,
						   1.f);
			}

			/// Les onglets 28 px : libellé 11 px, actif = texte plein + liseré 2 px
			/// accent SOUS SON PROPRE onglet ; le panneau porte l'état (il a
			/// remplacé le widget qui le tenait — la vérité reste unique).
			/// Les onglets de la référence (reference_4_185519) : TROIS CELLULES
			/// ÉGALES sur toute la largeur, libellés CENTRÉS, l'actif en texte
			/// plein avec un liseré accent de 2 px sous SON libellé (largeur du
			/// texte + 16), les inactifs atténués ; filet bas pleine largeur.
			int32 OngletsCostume(NkGuiContext &ctx) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 30.f);
				static const char *const kO[3] = {"Design", "Widget", "Behavior"};
				dl.AddLine({r.x, r.y + 29.5f}, {r.x + r.w, r.y + 29.5f}, ctx.theme.border, 1.f);
				const float32 colW = r.w / 3.f;
				for (int32 i = 0; i < 3; ++i) {
					const NkRect c = {r.x + colW * (float32)i, r.y, colW, 30.f};
					const float32 tw = costume::Largeur(F.px11, kO[i]);
					const float32 tx = c.x + (colW - tw) * 0.5f;
					const float32 ty = costume::CentrerY(F.px11, r.y, 30.f);
					if (i == mOnglet) {
						costume::TexteGras(dl, F.px11, tx, ty, kO[i], ctx.theme.text, 0.5f);
						const float32 lw = tw + 16.f;
						dl.AddRectFilled({c.x + (colW - lw) * 0.5f, c.y + 28.f, lw, 2.f},
										 ctx.theme.accent);
					} else
						costume::Texte(dl, F.px11, tx, ty, kO[i], ctx.theme.textMuted);
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(c, ctx.input.mousePos))
						mOnglet = i;
				}
				return mOnglet;
			}

			/// L'état de repli PAR SECTION (reference_4_185519 : TOUTES les
			/// sections portent un chevron ; ESPACEMENT, EFFETS et POINTS DE
			/// RUPTURE démarrent repliées). Un titre inconnu de la table (RÔLE,
			/// ÉVÉNEMENTS…) garde l'ancien costume à filet, sans repli.
			// ═══════════════════════════════════════════════════════════════════
			//  LES COLONNES D'UNE RANGÉE DE LISTE — ÉCRITES UNE FOIS POUR LES TROIS
			// ═══════════════════════════════════════════════════════════════════
			// ⚠️ CE TYPE EXISTE PARCE QUE LA LEÇON A ÉTÉ ÉCRITE À CÔTÉ D'UN SEUL
			//    CHEMIN, ET QUE LE CHEMIN VOISIN L'A REFAITE. Porte du dépôt du
			//    28/08 : « une leçon écrite à côté d'un chemin ne couvre pas le
			//    chemin voisin — demander quels chemins FRÈRES partagent le même
			//    danger, et l'écrire une fois pour tous ».
			//
			//    Le danger, mesuré trois fois le 01/09 dans un panneau de 235 px :
			//    l'hexa affichait « #00000 » (six caractères sur sept), puis le
			//    « % » touchait la poubelle, puis il restait 2 px. J'avais nommé
			//    la règle — « un texte tronqué est un texte qu'on n'a pas mesuré
			//    dans sa colonne » — la veille, à côté d'un AUTRE contrôle. Elle
			//    n'a pas voyagé toute seule jusqu'à la ligne d'à côté.
			//
			//    REMPLISSAGES, BORDURES et EFFETS ont la MÊME rangée : pastille,
			//    hexa, opacité, %, poubelle, œil. Les largeurs vivent donc ICI,
			//    au-dessus du groupe, et pas dans celle des trois qui vient de
			//    mordre. Une quatrième section à liste les héritera sans les
			//    redécouvrir.
			struct ColonnesRangee {
					float32 pastille = 0.f; ///< x de la pastille de couleur (16 px)
					float32 hexX = 0.f;		///< x du champ hexa
					float32 hexW = 0.f;		///< sa largeur, ce qui RESTE une fois le reste posé
					float32 opacX = 0.f;	///< x du champ d'opacité (30 px)
					float32 poubX = 0.f;	///< x de la poubelle (14 px)
					float32 oeilX = 0.f;	///< x de l'œil (14 px)
			};
			/// Les colonnes d'une rangée de liste, pour une bande `r` déjà obtenue.
			/// ⚠️ L'HEXA PREND CE QUI RESTE, ET C'EST DÉLIBÉRÉ : les quatre autres
			///    colonnes ont une largeur DICTÉE (une icône, trois chiffres) ;
			///    seule la couleur peut s'étirer. Lui donner une largeur fixe et
			///    laisser le reste flotter, c'est reproduire la troncature à la
			///    première police un peu large.
			static ColonnesRangee ColonnesDe(const NkRect &r) {
				ColonnesRangee c;
				const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
				c.oeilX = x1 - 16.f;
				c.poubX = x1 - 36.f;
				c.opacX = x1 - 80.f; // 30 px de champ + « % » + 5 px de garde
				c.pastille = x0;
				c.hexX = x0 + 20.f;
				c.hexW = c.opacX - c.hexX - 2.f;
				if (c.hexW < 24.f)
					c.hexW = 24.f;
				return c;
			}

			struct EtatSection {
					const char *titre;
					bool ouvert;
			};
			static constexpr uint32 kNbSections = 12;
			/// Un champ de sommet a change et la boite attend son recadrage.
			/// ⚠️ UN DRAPEAU, ET IL EST JUSTIFIE : on ne peut pas recadrer dans la
			///    branche qui ecrit (le champ est un GLISSER, il ecrit a chaque
			///    image), et on ne peut pas non plus recadrer sans savoir si
			///    quelque chose a bouge (le recadrage est idempotent, mais
			///    l'appeler a vide ferait un `MarkHumanEdit` de trop a chaque
			///    image ou la souris est relachee).
			bool mARecadrer = false;
			EtatSection mSections[kNbSections] = {
				// ⚠️ OUVERTE PAR DEFAUT, et c'est la seule qui le merite : on n'entre
				//    en edition de forme que par un geste explicite. Une section qu'il
				//    faudrait deplier apres etre entre dans un mode serait une section
				//    que personne ne voit.
				{"ÉDITION DE FORME", true},
				{"CIBLE", true},		{"DISPOSITION", true},	{"ANCRAGE", true},
				{"ALIGNEMENT", true},	{"ESPACEMENT", false},	{"REMPLISSAGES", true},
				{"BORDURES", true},	{"APPARENCE", true},	{"TYPOGRAPHIE", true},
				{"EFFETS", true},	{"POINTS DE RUPTURE", false},
			};
			EtatSection *TrouverSection(const char *titre) {
				for (uint32 i = 0; i < kNbSections; ++i)
					if (StrEq(mSections[i].titre, titre))
						return &mSections[i];
				return nullptr;
			}
			bool SectionOuverte(const char *titre) {
				const EtatSection *s = TrouverSection(titre);
				return s ? s->ouvert : true;
			}

			/// Un titre de section : chevron d'état + MAJUSCULES 9 px 600
			/// `text_muted` (le costume de la référence). Cliquer la rangée
			/// replie/déplie ; les corps se taisent quand leur section est pliée.
			void TitreSection(NkGuiContext &ctx, const char *titre) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 22.f);
				// ── LA BANDE PLEINE LARGEUR (Rodolf, 01/09 ; Lunacy) ─────────
				// Le titre ne flotte plus sur le fond : il est POSÉ. Le dessin
				// vient de `costume::BandeEnTete`, partagé avec les en-têtes de
				// la HIÉRARCHIE — le même motif, une seule écriture.
				costume::BandeEnTete(ctx.DL(), r.x, r.y, r.w, r.h, ctx.theme.header,
									 ctx.theme.border);
				EtatSection *s = TrouverSection(titre);
				float32 x = r.x + 12.f;
				const float32 ty = r.y + 8.f;
				if (s) {
					if (s->ouvert)
						costume::ChevronBas9(dl, x - 1.f, ty, ctx.theme.textMuted);
					else
						costume::ChevronReplie8(dl, x, ty + 1.f, ctx.theme.textMuted);
					x += 12.f;
				}
				costume::TexteGras(dl, F.px9, x, ty, titre, ctx.theme.textMuted, 0.4f);
				// ⚠️ L'EN-TÊTE SE PUBLIE AU RELEVÉ — ET C'EST LA LIMITE QUE
				//    J'AVAIS SIGNALÉE LE 01/09 QUI TOMBE ICI. Les titres de
				//    section sont PEINTS (`costume::TexteGras`), pas enregistrés
				//    comme widgets : ils étaient donc invisibles à `--dump-ui`, et
				//    chaque section neuve ne pouvait se juger qu'À L'ŒIL, sur une
				//    capture. Trois lignes lèvent ça : `insp.section.<TITRE>`
				//    porte le rectangle, et une mesure porte l'état d'ouverture
				//    (1 = déplié). « La section est-elle dessinée ? » devient une
				//    question à laquelle un banc répond.
				{
					char cle[64];
					snprintf(cle, sizeof(cle), "insp.section.%s", titre ? titre : "?");
					designkit::releve::Rect(ctx, cle, r);
					char cleM[80];
					snprintf(cleM, sizeof(cleM), "%s.ouvert", cle);
					nkgui::NkGuiNoterMesure(ctx, cleM, (s && s->ouvert) ? 1.f : 0.f,
											s ? 1.f : 0.f, 0.f, 0.f);
				}
				// ── LE « + » DE REMPLISSAGES, DANS L'EN-TÊTE (Lunacy) ────────
				// ⚠️ IL EST ICI PARCE QUE C'EST LÀ QU'IL EST CHEZ EUX, et parce
				//    que l'application dessine déjà ses propres en-têtes. Ajouter
				//    une fente d'action à la charpente du kit pour UN SEUL
				//    consommateur aurait fait grossir le socle sans preuve de
				//    besoin ; le jour où une deuxième section en veut une, elle
				//    descendra — c'est la règle du corollaire, pas son inverse.
				bool plusPris = false;
				const bool sectionAListe =
					s && NoeudMutable()
					&& (StrEq(titre, "REMPLISSAGES") || StrEq(titre, "BORDURES")
						|| StrEq(titre, "EFFETS"));
				if (sectionAListe) {
					const NkRect rp = {r.x + r.w - 28.f, r.y + 4.f, 16.f, 16.f};
					const bool sv =
						ctx.popupDepth == 0 && NkGuiRectContains(rp, ctx.input.mousePos);
					costume::IcPlus(dl, rp.x + 3.f, rp.y + 3.f,
									sv ? ctx.theme.accent : ctx.theme.textMuted);
					if (sv && ctx.input.mouseClicked[0]) {
						plusPris = true; // le clic du « + » n'est PAS un clic de repli
						if (StrEq(titre, "BORDURES"))
							AjouterBordure();
						else if (StrEq(titre, "EFFETS"))
							AjouterEffet();
						else
							AjouterRemplissage();
					}
				}
				if (s) {
					if (!plusPris && ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(r, ctx.input.mousePos))
						s->ouvert = !s->ouvert;
				} else {
					const float32 lx = x + costume::Largeur(F.px9, titre) + 8.f;
					dl.AddLine({lx, ty + 5.f}, {r.x + r.w - 12.f, ty + 5.f}, ctx.theme.border,
							   1.f);
				}
			}

			// ── LES CHAMPS DU COSTUME (boîte 20 px, fond `InputBg` #010409) ──
			nkgui::NkColor CouleurInput() const {
				return nkentseu::editorkit::NkThemeUnpack(
					mSt->theme.Get(nkentseu::editorkit::NkRole::InputBg));
			}
			static nkgui::NkColor CouleurHex(const char *hex, nkgui::NkColor repli) {
				if (!hex || hex[0] != '#')
					return repli;
				auto nyb = [](char c) -> int32 {
					if (c >= '0' && c <= '9')
						return c - '0';
					if (c >= 'a' && c <= 'f')
						return c - 'a' + 10;
					if (c >= 'A' && c <= 'F')
						return c - 'A' + 10;
					return -1;
				};
				uint32 v = 0;
				for (int32 i = 1; i <= 6; ++i) {
					const int32 d = nyb(hex[i]);
					if (d < 0)
						return repli;
					v = (v << 4) | (uint32)d;
				}
				return {(uint8)((v >> 16) & 0xFF), (uint8)((v >> 8) & 0xFF), (uint8)(v & 0xFF),
						255};
			}

			/// La boîte statique : fond, bord, texte 11 px (10 px si `petit`).
			void BoiteChamp(NkGuiContext &ctx, const NkRect &r, const char *texte,
							bool petit = false) {
				auto &dl = ctx.DL();
				dl.AddRectFilled(r, CouleurInput(), 4.f);
				dl.AddRect(r, ctx.theme.border, 1.f, 4.f);
				auto &F = costume::Fontes();
				const nkgui::NkGuiFont &f = petit ? F.px10 : F.px11;
				dl.PushClipRect(r, true);
				costume::Texte(dl, f, r.x + 6.f, costume::CentrerY(f, r.y, r.h), texte,
							   ctx.theme.text);
				dl.PopClipRect();
			}

			/// Un champ NUMÉRIQUE au costume : boîte + glisser horizontal (la
			/// saisie clavier viendra — le DragFloat d'avant n'en avait pas non
			/// plus). Rend true si la valeur a changé.
			/// LE PENDANT NON-AXE DE `ChampAxeMulti` : « — » si mixte, écriture
			/// sur TOUS les sélectionnés. ⚠️ IL EXISTE POUR LA MÊME RAISON, et la
			/// raison vaut d'être répétée ici plutôt que renvoyée ailleurs : sans
			/// lui, chaque rangée numérique de l'Inspecteur devrait se souvenir
			/// toute seule de tester le mixte, et celle qu'on ajoutera dans trois
			/// semaines montrera la valeur du PRINCIPAL comme si c'était celle du
			/// groupe — un chiffre faux qui a l'air juste.
			template <typename Lire, typename Ecrire>
			bool ChampNombreMulti(NkGuiContext &ctx, const char *id, const NkRect &r,
								  float32 vitesse, float32 vmin, float32 vmax, Lire lire,
								  Ecrire ecrire, bool petit = false) {
				float32 commune = 0.f;
				const bool uniforme = NkValeurCommune(mSt->doc, mSt->sel, lire, commune);
				char b[32];
				if (!uniforme)
					snprintf(b, sizeof(b), "\xE2\x80\x94"); // « — » : valeurs mixtes
				else if (commune == (float32)(int32)commune)
					snprintf(b, sizeof(b), "%d", (int32)commune);
				else
					snprintf(b, sizeof(b), "%.2f", (double)commune);
				BoiteChamp(ctx, r, b, petit);
				float32 v = uniforme ? commune : 0.f;
				if (ChampDrag(ctx, id, r, v, vitesse, vmin, vmax)) {
					AppliquerATous(ecrire, v);
					return true;
				}
				return false;
			}

			bool ChampNombre(NkGuiContext &ctx, const char *id, const NkRect &r, float32 &v,
							 float32 vitesse, float32 vmin, float32 vmax, bool petit = false,
							 bool tiretSiZero = false) {
				char b[32];
				if (tiretSiZero && v == 0.f)
					snprintf(b, sizeof(b), "\xE2\x80\x94"); // « — »
				else if (v == (float32)(int32)v)
					snprintf(b, sizeof(b), "%d", (int32)v);
				else
					snprintf(b, sizeof(b), "%.2f", (double)v);
				BoiteChamp(ctx, r, b, petit);
				const nkgui::NkGuiId gid = ctx.GetId(id);
				bool change = false;
				const bool dans = ctx.popupDepth == 0
								  && NkGuiRectContains(r, ctx.input.mousePos);
				if (dans)
					ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
				if (dans && ctx.input.mouseClicked[0]) {
					mDragChamp = gid;
					mDragDernierX = ctx.input.mousePos.x;
				}
				if (mDragChamp == gid) {
					if (!ctx.input.mouseDown[0])
						mDragChamp = 0;
					else {
						const float32 dx = ctx.input.mousePos.x - mDragDernierX;
						if (dx != 0.f) {
							mDragDernierX = ctx.input.mousePos.x;
							float32 nv = v + dx * vitesse;
							if (nv < vmin)
								nv = vmin;
							if (nv > vmax)
								nv = vmax;
							if (nv != v) {
								v = nv;
								change = true;
							}
						}
					}
				}
				return change;
			}

			// ⚠️ TROIS TREMPLINS, ET C'EST LE PRIX ASSUMÉ DU JOINT. La charpente
			//    prend `void(*)(void*, NkGuiContext&)` — l'idiome du kit
			//    (`NkTreeViewHooks`, `dockHeaderFn`, `clipboardGetFn`) — et non un
			//    pointeur de méthode, qui l'aurait liée à UNE classe et l'aurait
			//    rendue inutilisable par Nogee. Trois lignes chacun, une fois.
			static void CorpsPositionC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsPosition(ctx);
			}
			static void CorpsTailleC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsTaille(ctx);
			}
			static void CorpsAncrageC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsAncrage(ctx);
			}
			static void CorpsAlignementC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsAlignement(ctx);
			}
			static void CorpsApparenceC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsApparence(ctx);
			}
			static void CorpsRemplissagesC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsRemplissages(ctx);
			}
			static void CorpsBorduresC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsBordures(ctx);
			}
			static void CorpsBordsC(void *u, NkGuiContext &ctx) {
				// Le modele porte rayon/bordure depuis le 31/08 (vocabulaire
				// d'apparence §8ter, cle `rayon`/`bordure`) : la section edite.
				static_cast<InspectorPanel *>(u)->CorpsBords(ctx);
			}
			static void CorpsEspacementC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsEspacement(ctx);
			}
			static void CorpsDispositionC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsDisposition(ctx);
			}
			static void CorpsEffetsC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsEffets(ctx);
			}
			static void CorpsRuptureC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsRupture(ctx);
			}

			static void CorpsEditionFormeC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsEditionForme(ctx);
			}

			// ═══════════════════════════════════════════════════════════════════
			//  ÉDITION DE FORME — LE PANNEAU CHANGE EN MÊME TEMPS QUE LA TOILE
			// ═══════════════════════════════════════════════════════════════════
			/// ⚠️ CETTE SECTION EXISTE PARCE QUE LA COMPARAISON EN DEUX TEMPS DE
			///    RODOLF (01/09) MONTRE **TROIS** CHANGEMENTS, PAS UN.
			///    `lunacy_2temps_selection_181741.png` : la boîte, ses poignées, le
			///    panneau droit habituel. `lunacy_2temps_edition_181745.png` : plus
			///    aucune poignée de boîte, quatre carrés SUR le tracé — **et le
			///    panneau droit change de contenu**, une section `EDIT SHAPE`
			///    remplaçant la géométrie, LAYER / FILLS / BORDERS / EFFECTS /
			///    PROTOTYPING restant en dessous.
			///
			///    Le troisième était **absent de la consigne** qui a lancé ce lot,
			///    et il n'a été vu que parce que Rodolf a envoyé la paire d'images.
			///    *Une question sur du visuel se pose avec une capture.*
			///
			/// ⚠️ CE QUE LE MODÈLE NE PORTE PAS EST **MONTRÉ GRISÉ-QUI-LE-DIT**,
			///    jamais caché ni maquillé. Un champ absent laisse croire que
			///    l'outil ne connaît pas la notion ; un champ grisé MUET montre une
			///    capacité sans dire ce qui manque (le défaut déjà payé avec la
			///    touche F et les onglets). La raison est donc ÉCRITE sous le
			///    groupe, pas cachée dans une infobulle qu'il faut deviner.
			void CorpsEditionForme(NkGuiContext &ctx) {
				if (!SectionOuverte("ÉDITION DE FORME"))
					return;
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const int32 noeud = mSt->modeForme.noeud;
				if (!mSt->doc.IsValidIndex(noeud))
					return;
				NkUINode &n = mSt->doc.nodes[(uint32)noeud];
				// 🔴 REGARDER N'ÉCRIT PAS. Ma première version appelait ici
				//    `NkMaterialiserSommets` pour lire — ce qui ajoute la liste
				//    `sommets` au nœud, change les octets du fichier et marque le
				//    document modifié ALORS QUE L'UTILISATEUR N'A FAIT QU'OUVRIR
				//    UN PANNEAU. Et le volet CONSERVATION du mode points (cas 22)
				//    ne l'aurait pas vu : il n'exécute pas l'Inspecteur. La
				//    lecture passe donc par `NkLireSommet` / `NkNbSommetsDe`, qui
				//    retombent sur la table régulière sans rien écrire ; la
				//    matérialisation reste au moment de l'ÉCRITURE, comme sur la
				//    toile.
				const uint32 nbS = NkNbSommetsDe(n);
				const int32 iSel = mSt->modeForme.sommet;
				const bool unSelectionne = iSel >= 0 && (uint32)iSel < nbS;

				// ── LA RANGÉE X / Y / RAYON DU SOMMET SÉLECTIONNÉ ─────────────
				// ⚠️ EN PIXELS DEPUIS LE COIN HAUT-GAUCHE DE LA FORME, PAS EN
				//    UNITAIRE. Le modèle stocke -1..1 (c'est ce qui rend un tracé
				//    redimensionnable), mais « 0,42 » ne veut rien dire pour la
				//    main. La conversion vit ici, à l'affichage, et le stockage
				//    n'apprend rien de l'écran.
				const NkPaintRect rb = mSt->layout.Has(noeud)
										   ? mSt->layout.At(noeud)
										   : NkPaintRect{0.f, 0.f, 0.f, 0.f};
				{
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f;
					if (!unSelectionne)
						ctx.BeginDisabled();
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   "X", ctx.theme.textMuted);
					// 🔴 LES TROIS CHAMPS SE PARTAGENT LA LARGEUR DISPONIBLE, ILS NE
					//    LA SUPPOSENT PLUS. Ma première version posait trois boîtes
					//    de 50 px à des offsets fixes : la capture
					//    `preuve_edit_shape_n9.png` montre le troisième champ COUPÉ
					//    par le bord du panneau. Une largeur décidée sans regarder
					//    la place disponible — le même défaut, dans le même
					//    fichier, que celui que `Segmented` documente vingt lignes
					//    plus haut.
					const float32 dispo = (r.x + r.w - 12.f) - (x0 + 14.f);
					const float32 lc = (dispo - 2.f * 22.f) / 3.f;
					const NkRect rx = {x0 + 14.f, r.y + 3.f, lc, 20.f};
					costume::Texte(dl, F.px10, rx.x + rx.w + 8.f,
								   costume::CentrerY(F.px10, r.y + 3.f, 20.f), "Y",
								   ctx.theme.textMuted);
					const NkRect ry = {rx.x + rx.w + 22.f, r.y + 3.f, lc, 20.f};
					// 🔴 « ⌒ » (U+2312) SORTAIT « ? » — LA FONTE NE PORTE PAS LE
					//    GLYPHE. Trouvé sur la capture, pas à la relecture : le code
					//    était juste, l'atlas n'avait pas le caractère. Lunacy peut
					//    se permettre l'icône, nous non — et un « ? » à côté d'un
					//    nombre est pire qu'une lettre. On écrit « R ».
					costume::Texte(dl, F.px10, ry.x + ry.w + 8.f,
								   costume::CentrerY(F.px10, r.y + 3.f, 20.f), "R",
								   ctx.theme.textMuted);
					const NkRect rr = {ry.x + ry.w + 22.f, r.y + 3.f, lc, 20.f};
					float32 sx = 0.f, sy = 0.f, ra = 0.f;
					if (unSelectionne && rb.w > 0.f && rb.h > 0.f
						&& NkLireSommet(n, (uint32)iSel, sx, sy, ra)) {
						float32 px = (sx + 1.f) * 0.5f * rb.w;
						float32 py = (sy + 1.f) * 0.5f * rb.h;
						// ⚠️ LA MATÉRIALISATION EST ICI, DANS LA BRANCHE QUI ÉCRIT,
						//    et pas une ligne plus haut : c'est la seule place où
						//    elle ne transforme pas un regard en modification.
						if (ChampNombre(ctx, "insp.forme.x", rx, px, 0.5f, -4096.f, 4096.f)) {
							NkMaterialiserSommets(n);
							if ((uint32)iSel < (uint32)n.sommets.Size()) {
								n.sommets[(uint32)iSel].x = px / (rb.w * 0.5f) - 1.f;
								mSt->doc.MarkHumanEdit(noeud);
								mARecadrer = true;
							}
						}
						if (ChampNombre(ctx, "insp.forme.y", ry, py, 0.5f, -4096.f, 4096.f)) {
							NkMaterialiserSommets(n);
							if ((uint32)iSel < (uint32)n.sommets.Size()) {
								n.sommets[(uint32)iSel].y = py / (rb.h * 0.5f) - 1.f;
								mSt->doc.MarkHumanEdit(noeud);
								mARecadrer = true;
							}
						}
						if (ChampNombre(ctx, "insp.forme.rayon", rr, ra, 0.5f, 0.f, 256.f)) {
							NkMaterialiserSommets(n);
							if ((uint32)iSel < (uint32)n.sommets.Size()) {
								n.sommets[(uint32)iSel].rayon = ra;
								mSt->doc.MarkHumanEdit(noeud);
							}
						}
						// ── LA BOÎTE SUIT LES CHAMPS AUSSI, AU RELÂCHEMENT ────
						// ⚠️ MÊME RAISON QU'AU GLISSER D'UN SOMMET SUR LA TOILE, et
						//    c'est le chemin frère : recadrer à chaque cran du champ
						//    changerait le RÉFÉRENTIEL des sommets sous le doigt qui
						//    tire — la valeur affichée sauterait à chaque pixel de
						//    glissement, et le champ deviendrait inutilisable.
						//    *Le chemin frère traité comme un groupe dès l'écriture,
						//    pas après l'avoir vu mordre.*
						if (mARecadrer && !ctx.input.mouseDown[0]) {
							mARecadrer = false;
							const int32 pa = n.parent;
							const bool libre =
								mSt->doc.IsValidIndex(pa)
								&& (mSt->doc.nodes[(uint32)pa].layout.kind == NkLayoutKind::Free
									|| mSt->doc.nodes[(uint32)pa].layout.kind
										   == NkLayoutKind::Anchor);
							if (NkRecadrerNoeud(n, libre))
								mSt->doc.MarkHumanEdit(noeud);
						}
					} else {
						BoiteChamp(ctx, rx, "\xE2\x80\x94");
						BoiteChamp(ctx, ry, "\xE2\x80\x94");
						BoiteChamp(ctx, rr, "\xE2\x80\x94");
					}
					if (!unSelectionne)
						ctx.EndDisabled();
				}
				// Quel sommet, sur combien : sans ce compte, « X 42 » ne dit pas
				// DE QUOI il parle quand la forme en a douze.
				{
					char b[96];
					if (unSelectionne)
						snprintf(b, sizeof(b), "Sommet %d sur %u", iSel + 1, nbS);
					else
						snprintf(b, sizeof(b),
								 "%u sommet(s) — cliquez-en un sur la toile pour lire ses "
								 "coordonnées.",
								 nbS);
					const NkRect r = ctx.NextItemRect(-1.f, 18.f);
					dl.PushClipRect(r, true);
					costume::Texte(dl, F.px9, r.x + 12.f, costume::CentrerY(F.px9, r.y, 18.f), b,
								   ctx.theme.textMuted);
					dl.PopClipRect();
				}

				// ── LES POIGNÉES DE BÉZIER : NOMMÉES, PAS ÉBAUCHÉES ───────────
				// ⚠️ SA CAPTURE 2 MONTRE UN COIN COURBÉ AVEC SES DEUX TANGENTES.
				//    Chez nous l'arrondi est un RAYON, donc un arc SYMÉTRIQUE : il
				//    couvre « on peut l'arrondir », pas la courbe libre. La
				//    différence est réelle, et on l'écrit plutôt que de la masquer.
				{
					ctx.BeginDisabled();
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f;
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   "X1", ctx.theme.textMuted);
					// Même partage de largeur que la rangée X/Y/R : deux champs et
					// deux étiquettes, jamais des offsets fixes.
					const float32 dispoB = (r.x + r.w - 12.f) - (x0 + 22.f);
					const float32 lb = (dispoB - 26.f) * 0.5f;
					const NkRect a1 = {x0 + 22.f, r.y + 3.f, lb, 20.f};
					BoiteChamp(ctx, a1, "\xE2\x80\x94");
					costume::Texte(dl, F.px10, a1.x + a1.w + 6.f,
								   costume::CentrerY(F.px10, r.y + 3.f, 20.f), "Y1",
								   ctx.theme.textMuted);
					const NkRect a2 = {a1.x + a1.w + 26.f, r.y + 3.f, lb, 20.f};
					BoiteChamp(ctx, a2, "\xE2\x80\x94");
					const NkRect r2 = ctx.NextItemRect(-1.f, 26.f);
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r2.y + 3.f, 20.f),
								   "X2", ctx.theme.textMuted);
					const NkRect b1 = {x0 + 22.f, r2.y + 3.f, lb, 20.f};
					BoiteChamp(ctx, b1, "\xE2\x80\x94");
					costume::Texte(dl, F.px10, b1.x + b1.w + 6.f,
								   costume::CentrerY(F.px10, r2.y + 3.f, 20.f), "Y2",
								   ctx.theme.textMuted);
					const NkRect b2 = {b1.x + b1.w + 26.f, r2.y + 3.f, lb, 20.f};
					BoiteChamp(ctx, b2, "\xE2\x80\x94");
					// ── LES SIX TYPES DE POINT — ET POURQUOI ILS ATTENDENT ────
					// ⚠️ ILS N'ONT DE SENS QU'AVEC LES POIGNÉES DE BÉZIER. Les
					//    poser avant, ce serait six boutons dont quatre ne
					//    changeraient rien — exactement la « case toujours cochée à
					//    côté d'un aimant qui ne fait rien » que Q42 a trouvée dans
					//    le menu Affichage.
					// 🔴 ET MA PREMIÈRE VERSION LES A POSÉS QUAND MÊME, EN `Segmented`.
					//    La capture `preuve_edit_shape_n9.png` montre le résultat : six
					//    mots ne tiennent pas dans une colonne d'Inspecteur, le contrôle
					//    bascule en FLOT (à raison — c'est sa règle anti-troncature), et
					//    on obtient **six lignes empilées dont la première est en
					//    surbrillance**. Un choix qui a l'air fait, sur un contrôle qui
					//    n'agit pas : c'est le défaut même que le commentaire ci-dessus
					//    dit éviter, et je l'avais écrit deux lignes plus bas.
					//    *Les nommer suffit ; les mimer trompe.*
					(void)designkit::Button(ctx, "Ouvrir le tracé", "insp.forme.ouvrir");
					ctx.EndDisabled();
					nkgui::TextWrapped(ctx,
									   "« Ouvrir le tracé » : nos tracés sont fermés — le "
									   "modèle ne sait pas encore exprimer un tracé ouvert.");
				}

				// ── LES TYPES DE POINT : LA RANGÉE CESSE D'ÊTRE GRISE ─────────
				// Retour de Rodolf, 01/09 (nuit) : *« le manipulateur de chaque
				// côté indépendant ou dépendant en fonction de l'utilisateur. »*
				//
				// ⚠️ LES QUATRE NOMS SONT CEUX DE LA DOCUMENTATION LUNACY
				//    (`tools/#types-of-points`), pas de mon invention — et
				//    « asymétrique » y veut dire MÊME ANGLE, longueurs
				//    différentes. Le libellé le dit sous la rangée, parce que
				//    c'est contre-intuitif et qu'un bouton dont on devine mal le
				//    sens est un bouton qu'on n'ose pas presser.
				//
				// ⚠️ ET LE CHANGEMENT S'APPLIQUE À TOUS LES SOMMETS MARQUÉS, pas
				//    au seul principal : la multi-sélection existe depuis ce
				//    soir, et un réglage qui n'obéirait qu'à un sommet sur cinq
				//    serait un réglage qu'on croit avoir appliqué.
				{
					static const char *const kTypes[4] = {"Droit", "Miroir", "Asym.",
														  "Libre"};
					int32 courant = -1;
					if (unSelectionne) {
						float32 tx = 0.f, ty = 0.f, tr = 0.f;
						(void)NkLireSommet(n, (uint32)iSel, tx, ty, tr);
						if ((uint32)iSel < (uint32)n.sommets.Size())
							courant = (int32)n.sommets[(uint32)iSel].liaison;
					}
					if (!unSelectionne)
						ctx.BeginDisabled();
					const int32 choisi =
						designkit::Segmented(ctx, kTypes, 4, courant, "insp.forme.types");
					if (!unSelectionne)
						ctx.EndDisabled();
					if (unSelectionne && choisi >= 0) {
						NkMaterialiserSommets(n);
						uint32 touches = 0;
						for (uint32 k = 0; k < (uint32)n.sommets.Size(); ++k) {
							if (!mSt->modeForme.Marque((int32)k))
								continue;
							NkPoserLiaison(n.sommets[k], (nkentseu::uint8)choisi);
							++touches;
						}
						if (touches > 0) {
							mSt->doc.MarkHumanEdit(noeud);
							mARecadrer = true;
							char b[160];
							snprintf(b, sizeof(b),
									 "%u sommet(s) passé(s) en « %s »%s", touches,
									 kTypes[choisi],
									 choisi == 0 ? " — les tangentes sont effacées." : ".");
							mSt->status = NkString(b);
						}
					}
					nkgui::TextWrapped(
						ctx, "Miroir : les deux poignées liées en direction ET longueur. "
							 "Asym. : même angle, longueurs libres. Libre : indépendantes. "
							 "Pendant le glisser, Alt = libre, Ctrl = asym.");
				}
				{
				}

				// ── TERMINER ──────────────────────────────────────────────────
				// ⚠️ CE BOUTON N'EST PAS DU DÉCOR : c'est la SORTIE EXPLICITE du
				//    mode, celle que Lunacy met en accent (`Finish`). Échap sort
				//    déjà — mais une sortie qui n'est QUE clavier est une sortie
				//    que l'utilisateur qui s'est senti coincé ne trouve pas. Sa
				//    capture du 01/09 disait exactement ça : « c'est difficile ou
				//    impossible de le désélectionner ».
				if (designkit::Button(ctx, "Terminer", "insp.forme.terminer")) {
					mSt->modeForme.Quitter();
					mSt->status = NkString("Édition de forme terminée.");
				}
			}

			/// DISPOSITION (reference_4_185519) : la rangée « Position » — deux
			/// champs à BARRE D'AXE (rouge = X, vert = Y, les rôles AxisX/AxisY)
			/// et deux petits boutons carrés (« + », « • », inertes et ils le
			/// disent) — puis « Largeur » et « Hauteur » : boîte-combo du mode
			/// (« expand », « fixed 44 »…) éditable au glisser, bornes min–max en
			/// texte atténué à droite quand elles existent (la référence les
			/// montre ainsi, pas en champs — l'édition des bornes passe par le
			/// document, écart nommé au rapport).
			void CorpsDisposition(NkGuiContext &ctx) {
				if (!SectionOuverte("DISPOSITION"))
					return;
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				NkUINode *n = NoeudMutable();
				// ── Position ────────────────────────────────────────────────
				{
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   "Position", ctx.theme.textMuted);
					const float32 champs0 = x0 + 52.f;
					const float32 wBtns = 2.f * 20.f + 4.f;
					const float32 colW = (x1 - champs0 - wBtns - 12.f) * 0.5f;
					const NkRect rx = {champs0, r.y + 3.f, colW - 3.f, 20.f};
					const NkRect ry = {champs0 + colW + 3.f, r.y + 3.f, colW - 3.f, 20.f};
					const NkColor rouge = nkentseu::editorkit::NkThemeUnpack(
						mSt->theme.Get(nkentseu::editorkit::NkRole::AxisX));
					const NkColor vert = nkentseu::editorkit::NkThemeUnpack(
						mSt->theme.Get(nkentseu::editorkit::NkRole::AxisY));
					const bool libre = n && ParentKind() == editorkit::NkLayoutKind::Free;
					bool bouge = false;
					if (libre) {
						// ⚠️ MULTI-SÉLECTION COMPRISE : « — » si les X diffèrent, et
						//    l'édition part sur TOUS. Avant, cette rangée montrait
						//    la position du PRINCIPAL comme si c'était celle du
						//    groupe.
						bouge |= ChampAxeMulti(
							ctx, "insp.dispo.x", rx, rouge,
							[](const NkUINode &q) { return q.posX; },
							[](NkUINode &q, float32 v) { q.posX = v; });
						bouge |= ChampAxeMulti(
							ctx, "insp.dispo.y", ry, vert,
							[](const NkUINode &q) { return q.posY; },
							[](NkUINode &q, float32 v) { q.posY = v; });
					} else {
						// la position CALCULÉE (jamais écrite) — boîtes statiques
						char b[32];
						const bool a = n && mSt->layout.Has(mSt->selected);
						const NkPaintRect rc = a ? mSt->layout.At(mSt->selected)
												 : NkPaintRect{0.f, 0.f, 0.f, 0.f};
						snprintf(b, sizeof(b), a ? "%.0f" : "\xE2\x80\x94", (double)rc.x);
						BoiteChampAxe(ctx, rx, b, rouge);
						snprintf(b, sizeof(b), a ? "%.0f" : "\xE2\x80\x94", (double)rc.y);
						BoiteChampAxe(ctx, ry, b, vert);
					}
					if (bouge)
						mSt->doc.MarkHumanEdit(mSt->selected);
					// les deux boutons carrés de la référence — inertes, et ils
					// le DISENT au clic (règle des menus : jamais un no-op muet).
					for (int32 i = 0; i < 2; ++i) {
						const NkRect rb = {x1 - wBtns + (float32)i * 24.f, r.y + 3.f, 20.f, 20.f};
						dl.AddRectFilled(rb, CouleurInput(), 4.f);
						dl.AddRect(rb, ctx.theme.border, 1.f, 4.f);
						if (i == 0) {
							dl.AddLine({rb.x + 10.f, rb.y + 6.f}, {rb.x + 10.f, rb.y + 14.f},
									   ctx.theme.textMuted, 1.2f);
							dl.AddLine({rb.x + 6.f, rb.y + 10.f}, {rb.x + 14.f, rb.y + 10.f},
									   ctx.theme.textMuted, 1.2f);
						} else
							dl.AddCircleFilled({rb.x + 10.f, rb.y + 10.f}, 2.f,
											   ctx.theme.textMuted);
						if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
							&& NkGuiRectContains(rb, ctx.input.mousePos))
							mSt->status = NkString(
								"Contraintes de position : à brancher (référence Banani).");
					}
					if (n && !libre)
						nkgui::TextWrapped(ctx, "calculée — jamais écrite dans le document");
				}
				if (!n)
					return;
				LigneDimension(ctx, "Largeur", n->width);
				LigneDimension(ctx, "Hauteur", n->height);
			}

			/// La boîte à BARRE D'AXE : champ du costume + barre 3 px de la
			/// couleur d'axe au bord gauche (la référence identifie X/Y par la
			/// couleur, pas par une lettre).
			void BoiteChampAxe(NkGuiContext &ctx, const NkRect &r, const char *texte,
							   const NkColor &axe) {
				auto &dl = ctx.DL();
				dl.AddRectFilled(r, CouleurInput(), 4.f);
				dl.AddRect(r, ctx.theme.border, 1.f, 4.f);
				dl.AddRectFilled({r.x + 1.f, r.y + 3.f, 3.f, r.h - 6.f}, axe, 1.5f);
				auto &F = costume::Fontes();
				dl.PushClipRect(r, true);
				costume::Texte(dl, F.px11, r.x + 9.f, costume::CentrerY(F.px11, r.y, r.h), texte,
							   ctx.theme.text);
				dl.PopClipRect();
			}
			/// LA RANGÉE D'AXE EN MULTI-SÉLECTION : « — » quand c'est MIXTE, et
			/// l'édition s'applique à TOUS les sélectionnés (Lunacy).
			/// ⚠️ CE HELPER EXISTE POUR QUE LA RÈGLE NE SE RÉÉCRIVE PAS RANGÉE PAR
			///    RANGÉE. X, Y, largeur, hauteur, opacité, rayon… chacune aurait
			///    dû se souvenir de tester le mixte ; la première ajoutée après
			///    coup l'aurait oublié, et elle aurait affiché la valeur du
			///    PRINCIPAL comme si c'était celle de tout le monde — un chiffre
			///    faux qui a l'air juste. Le mécanisme (`NkValeurCommune`) tranche,
			///    et cette rangée ne sait plus se tromper.
			/// `lire` donne la valeur d'un nœud ; `ecrire` la pose sur un nœud.
			template <typename Lire, typename Ecrire>
			bool ChampAxeMulti(NkGuiContext &ctx, const char *id, const NkRect &r,
							   const NkColor &axe, Lire lire, Ecrire ecrire) {
				float32 commune = 0.f;
				const bool uniforme = NkValeurCommune(mSt->doc, mSt->sel, lire, commune);
				if (!uniforme) {
					// MIXTE : le tiret cadratin, et un glisser qui part de zéro
					// poserait la MÊME valeur partout — c'est ce que fait Lunacy.
					BoiteChampAxe(ctx, r, "\xE2\x80\x94", axe);
					float32 v = 0.f;
					if (ChampDrag(ctx, id, r, v, 1.f, -100000.f, 100000.f)) {
						AppliquerATous(ecrire, v);
						return true;
					}
					return false;
				}
				char b[32];
				if (commune == (float32)(int32)commune)
					snprintf(b, sizeof(b), "%d", (int32)commune);
				else
					snprintf(b, sizeof(b), "%.2f", (double)commune);
				BoiteChampAxe(ctx, r, b, axe);
				float32 v = commune;
				if (ChampDrag(ctx, id, r, v, 1.f, -100000.f, 100000.f)) {
					AppliquerATous(ecrire, v);
					return true;
				}
				return false;
			}
			/// Poser une valeur sur TOUS les sélectionnés (la racine exceptée), en
			/// marquant chacun — un seul `MarkHumanEdit` sur le principal aurait
			/// laissé les autres sans provenance.
			template <typename Ecrire>
			void AppliquerATous(Ecrire ecrire, float32 v) {
				for (uint32 k = 0; k < (uint32)mSt->sel.items.Size(); ++k) {
					const int32 i = mSt->sel.items[k];
					if (i <= 0 || !mSt->doc.IsValidIndex(i))
						continue;
					ecrire(mSt->doc.nodes[(uint32)i], v);
					mSt->doc.MarkHumanEdit(i);
				}
			}

			bool ChampNombreAxe(NkGuiContext &ctx, const char *id, const NkRect &r, float32 &v,
								const NkColor &axe) {
				char b[32];
				if (v == (float32)(int32)v)
					snprintf(b, sizeof(b), "%d", (int32)v);
				else
					snprintf(b, sizeof(b), "%.2f", (double)v);
				BoiteChampAxe(ctx, r, b, axe);
				return ChampDrag(ctx, id, r, v, 1.f, -100000.f, 100000.f);
			}

			/// « Largeur : [expand v]  120–320 » — la rangée de dimension de la
			/// référence : boîte-combo du mode (glisser = éditer la valeur quand
			/// le mode en porte une), bornes en texte atténué à droite.
			void LigneDimension(NkGuiContext &ctx, const char *titre, NkSizeDecl &d) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const bool porteValeur = d.mode == NkSizeMode::Fixed
										 || d.mode == NkSizeMode::Fraction
										 || d.mode == NkSizeMode::Weight;
				const bool metrique = d.valueMetric && *d.valueMetric;
				const NkRect r = ctx.NextItemRect(-1.f, 26.f);
				const float32 x0 = r.x + 12.f;
				costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f), titre,
							   ctx.theme.textMuted);
				const NkRect rb = {x0 + 52.f, r.y + 3.f, 96.f, 20.f};
				char b[96];
				if (metrique)
					snprintf(b, sizeof(b), "« %s »", d.valueMetric);
				else if (porteValeur)
					snprintf(b, sizeof(b), "%s %d", NkSizeModeName(d.mode), (int32)d.value);
				else
					snprintf(b, sizeof(b), "%s", NkSizeModeName(d.mode));
				BoiteChamp(ctx, rb, b);
				costume::ChevronCombo7(dl, rb.x + rb.w - 12.f, rb.y + 8.f, ctx.theme.textMuted);
				// LE MODE SE CHANGE (recadrage « chaque contrôle agit ») : cliquer
				// le CHEVRON cycle fixed → content → fraction → weight → expand.
				// Le glisser sur le reste de la boîte édite la VALEUR.
				const NkRect rChevron = {rb.x + rb.w - 20.f, rb.y, 20.f, rb.h};
				if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
					&& NkGuiRectContains(rChevron, ctx.input.mousePos)) {
					d.mode = (NkSizeMode)(((int32)d.mode + 1) % (int32)NkSizeMode::Count);
					if (d.mode == NkSizeMode::Fixed && d.value <= 0.f)
						d.value = 100.f; // un fixed sans valeur serait invisible
					mSt->doc.MarkHumanEdit(mSt->selected);
				} else if (porteValeur && !metrique) {
					const float32 vitesse = d.mode == NkSizeMode::Fixed ? 1.f
											: d.mode == NkSizeMode::Fraction ? 0.01f : 0.05f;
					const float32 vmax = d.mode == NkSizeMode::Fraction ? 1.f : 4096.f;
					char id[40];
					snprintf(id, sizeof(id), "insp.dim.%s", titre);
					float32 avant = d.value;
					if (ChampDrag(ctx, id, rb, d.value, vitesse, 0.f, vmax) && d.value != avant)
						mSt->doc.MarkHumanEdit(mSt->selected);
				}
				// les bornes min / max — ÉDITABLES (fonction d'abord ; la
				// référence les résume en texte, l'édition prime — écart nommé).
				{
					const NkRect r2 = ctx.NextItemRect(-1.f, 22.f);
					const float32 mx0 = r2.x + 12.f + 52.f;
					const float32 moitie = (r2.x + r2.w - 12.f - mx0 - 8.f) * 0.5f;
					costume::Texte(dl, F.px9, mx0, costume::CentrerY(F.px9, r2.y + 2.f, 18.f),
								   "min", ctx.theme.textMuted);
					const NkRect rmin = {mx0 + 26.f, r2.y + 2.f, moitie - 26.f, 18.f};
					const float32 mx1 = mx0 + moitie + 8.f;
					costume::Texte(dl, F.px9, mx1, costume::CentrerY(F.px9, r2.y + 2.f, 18.f),
								   "max", ctx.theme.textMuted);
					const NkRect rmax = {mx1 + 26.f, r2.y + 2.f, moitie - 26.f, 18.f};
					char id[48];
					snprintf(id, sizeof(id), "insp.dmin.%s", titre);
					bool bouge = ChampNombre(ctx, id, rmin, d.minVal, 1.f, 0.f, 4096.f, true, true);
					snprintf(id, sizeof(id), "insp.dmax.%s", titre);
					bouge |= ChampNombre(ctx, id, rmax, d.maxVal, 1.f, 0.f, 4096.f, true, true);
					if (bouge)
						mSt->doc.MarkHumanEdit(mSt->selected);
				}
			}

			/// EFFETS — la section existe (référence), son modèle n'existe pas :
			/// elle le DIT, elle ne met pas en scène des ombres inventées.
			// ═══════════════════════════════════════════════════════════════════
			//  EFFETS (Lunacy « EFFECTS ») — étage 3 du mandat listes
			// ═══════════════════════════════════════════════════════════════════
			// Le gabarit de `lunacy_props_11`, sur TROIS lignes par effet :
			//   1. le TYPE (« Ombre portée ⌄ ») + œil + poubelle
			//   2. X, Y, flou, étendue
			//   3. couleur + opacité
			//
			// ⚠️ LES COLONNES VIENNENT DU GROUPE (`ColonnesDe`), pas d'ici. C'est
			//    la troisième section à liste ; les deux premières ont payé trois
			//    troncatures avant que la géométrie descende au-dessus du groupe.
			//    Celle-ci n'a rien redécouvert — c'était l'objet du geste.
			void CorpsEffets(NkGuiContext &ctx) {
				if (!SectionOuverte("EFFETS"))
					return;
				NkUINode *n = NoeudMutable();
				if (!n) {
					designkit::KeyValue(ctx, "Effets", "-");
					return;
				}
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				if (mEffetsNode != mSt->selected || mEffetsGen != mSt->editionGeneration) {
					mEffetsNode = mSt->selected;
					mEffetsGen = mSt->editionGeneration;
					for (uint32 i = 0; i < kMaxFillsUI; ++i)
						mEffetsBuf[i][0] = '\0';
					for (uint32 i = 0; i < (uint32)n->effets.Size() && i < kMaxFillsUI; ++i)
						snprintf(mEffetsBuf[i], sizeof(mEffetsBuf[i]), "%s",
								 n->effets[i].couleur.Data());
				}
				const uint32 nb = (uint32)n->effets.Size();
				// Du DERNIER au premier, comme les deux autres listes.
				for (uint32 vi = 0; vi < nb; ++vi) {
					const uint32 i = nb - 1u - vi;
					if (i >= kMaxFillsUI)
						continue;
					NkEffet &e = n->effets[i];
					// ── LIGNE 1 : le TYPE, l'œil, la poubelle
					{
						const NkRect r = ctx.NextItemRect(-1.f, 26.f);
						const ColonnesRangee col = ColonnesDe(r);
						const char *nomType = (e.type == NkEffetType::OmbreInterne)
												  ? "Ombre interne"
												  : "Ombre portée";
						const float32 tw = col.poubX - r.x - 16.f;
						const NkRect rt = {r.x + 12.f, r.y + 3.f, tw > 40.f ? tw : 40.f, 20.f};
						const bool svT =
							ctx.popupDepth == 0 && NkGuiRectContains(rt, ctx.input.mousePos);
						dl.AddRectFilled(rt, CouleurInput(), 4.f);
						dl.AddRect(rt, svT ? ctx.theme.accent : ctx.theme.border, 1.f, 4.f);
						costume::Texte(dl, F.px10, rt.x + 6.f,
									   costume::CentrerY(F.px10, rt.y, 20.f), nomType,
									   e.visible ? ctx.theme.text : ctx.theme.textDisabled);
						costume::ChevronCombo7(dl, rt.x + rt.w - 11.f, rt.y + 7.5f,
											   ctx.theme.textMuted);
						if (svT && ctx.input.mouseClicked[0]) {
							e.type = (e.type == NkEffetType::OmbrePortee)
										 ? NkEffetType::OmbreInterne
										 : NkEffetType::OmbrePortee;
							mSt->doc.MarkHumanEdit(mSt->selected);
							// ⚠️ ON LE DIT : l'ombre interne se règle et se sauve,
							//    mais son PEINTRE n'existe pas. Un réglage qui ne
							//    change rien à l'écran sans le dire ferait croire
							//    à une panne.
							mSt->status = NkString(
								e.type == NkEffetType::OmbreInterne
									? "Ombre interne : réglée et enregistrée — son peintre "
									  "arrive (elle ne se voit pas encore sur la toile)."
									: "Ombre portée.");
						}
						{
							const NkRect rp = {col.poubX, r.y + 6.f, 14.f, 14.f};
							const bool sv =
								ctx.popupDepth == 0 && NkGuiRectContains(rp, ctx.input.mousePos);
							costume::IcPoubelle(dl, rp.x + 1.f, rp.y + 1.f,
												sv ? ctx.theme.accent : ctx.theme.textMuted);
							if (sv && ctx.input.mouseClicked[0]) {
								n->effets.RemoveAt(i);
								mEffetsGen = -1;
								mSt->doc.MarkHumanEdit(mSt->selected);
								mSt->status = NkString("Effet retiré — Ctrl+Z le ramène.");
								return; // la liste a glissé : on ne lit plus `e`
							}
						}
						{
							const NkRect re = {col.oeilX, r.y + 6.f, 14.f, 14.f};
							const bool sv =
								ctx.popupDepth == 0 && NkGuiRectContains(re, ctx.input.mousePos);
							const NkColor c = sv ? ctx.theme.accent
												 : (e.visible ? ctx.theme.textMuted
															  : ctx.theme.textDisabled);
							if (e.visible)
								costume::IcOeil(dl, re.x + 1.f, re.y + 1.f, c);
							else
								costume::IcOeilBarre(dl, re.x + 1.f, re.y + 1.f, c);
							if (sv && ctx.input.mouseClicked[0]) {
								e.visible = !e.visible;
								mSt->doc.MarkHumanEdit(mSt->selected);
							}
						}
					}
					// ── LIGNE 2 : X, Y, flou, étendue — les QUATRE nombres
					{
						const NkRect r = ctx.NextItemRect(-1.f, 24.f);
						const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
						const float32 large = (x1 - x0 - 3.f * 6.f) * 0.25f;
						const char *etiq[4] = {"X", "Y", "flou", "étendue"};
						float32 *val[4] = {&e.x, &e.y, &e.flou, &e.etendue};
						for (int32 k = 0; k < 4; ++k) {
							const NkRect rc = {x0 + (large + 6.f) * (float32)k, r.y + 2.f,
											   large, 20.f};
							char id[32];
							snprintf(id, sizeof(id), "insp.effet.%d.%u", k, i);
							// ⚠️ X ET Y ACCEPTENT LE NEGATIF (une ombre peut porter
							//    vers la gauche ou vers le haut) ; le flou et
							//    l'etendue non — un rayon negatif n'a pas de sens.
							const float32 mini = (k < 2) ? -256.f : 0.f;
							if (ChampNombre(ctx, id, rc, *val[k], 0.5f, mini, 256.f))
								mSt->doc.MarkHumanEdit(mSt->selected);
							costume::Texte(dl, F.px9, rc.x + 2.f, rc.y + 21.f, etiq[k],
										   ctx.theme.textDisabled);
						}
					}
					// ── LIGNE 3 : couleur + opacité (les colonnes du GROUPE)
					{
						const NkRect r = ctx.NextItemRect(-1.f, 30.f);
						const ColonnesRangee col = ColonnesDe(r);
						const NkRect sw = {col.pastille, r.y + 9.f, 16.f, 16.f};
						if (mEffetsBuf[i][0]) {
							dl.AddRectFilled(sw, CouleurHex(mEffetsBuf[i], ctx.theme.textMuted),
											 3.f);
							dl.AddRect(sw, ctx.theme.border, 1.f, 3.f);
						} else {
							dl.AddRect(sw, ctx.theme.border, 1.f, 3.f);
							dl.AddLine({sw.x + 3.f, sw.y + 13.f}, {sw.x + 13.f, sw.y + 3.f},
									   ctx.theme.textMuted, 1.f);
						}
						char idHex[32];
						snprintf(idHex, sizeof(idHex), "##insp.effet.hex%u", i);
						ctx.SetNextItemRect({col.hexX, r.y + 7.f, col.hexW, 20.f});
						if (nkgui::InputText(ctx, idHex, mEffetsBuf[i], 10)) {
							e.couleur = NkString(mEffetsBuf[i]);
							mSt->doc.MarkHumanEdit(mSt->selected);
						}
						char idOp[32];
						snprintf(idOp, sizeof(idOp), "insp.effet.op%u", i);
						const NkRect ro = {col.opacX, r.y + 7.f, 30.f, 20.f};
						if (ChampNombre(ctx, idOp, ro, e.opacite, 1.f, 0.f, 100.f))
							mSt->doc.MarkHumanEdit(mSt->selected);
						costume::Texte(dl, F.px9, ro.x + ro.w + 3.f,
									   costume::CentrerY(F.px9, r.y + 7.f, 20.f), "%",
									   ctx.theme.textMuted);
					}
				}
				if (nb == 0) {
					const NkRect r = ctx.NextItemRect(-1.f, 20.f);
					costume::Texte(dl, F.px9, r.x + 12.f, costume::CentrerY(F.px9, r.y, 18.f),
								   "Aucun — « + » en ajoute.", ctx.theme.textDisabled);
				}
			}
			/// Le « + » d'EFFETS. Les valeurs de départ sont celles de la
			/// référence (`lunacy_props_11`) : Y 4, flou 4, noir à 25 % — une ombre
			/// qui se VOIT tout de suite, plutôt qu'un effet nul qu'il faudrait
			/// régler avant de comprendre qu'il est là.
			void AjouterEffet() {
				NkUINode *n = NoeudMutable();
				if (!n)
					return;
				if ((uint32)n->effets.Size() >= kMaxFillsUI) {
					mSt->status = NkString("Effets : la pile de l'inspecteur en montre huit au "
										   "plus (le modèle, lui, n'a pas de borne).");
					return;
				}
				NkEffet e;
				e.couleur = NkString("#000000");
				n->effets.PushBack(e);
				mEffetsGen = -1;
				mSt->doc.MarkHumanEdit(mSt->selected);
				mSt->status = NkString("Ombre portée ajoutée — Ctrl+Z la retire.");
			}
			/// POINTS DE RUPTURE — même règle d'honnêteté que EFFETS.
			void CorpsRupture(NkGuiContext &ctx) {
				if (!SectionOuverte("POINTS DE RUPTURE"))
					return;
				ctx.BeginDisabled();
				nkgui::TextWrapped(ctx, "Les points de rupture arrivent avec la transposition "
										"entre cibles (écran 27).");
				ctx.EndDisabled();
			}
			static void CorpsCibleC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsCible(ctx);
			}
			/// CIBLE (écran 3) : « Cible du cadre : [Mobile — 390 × 844 v] » —
			/// la valeur vient de la clé `cible` du document, reformatée à la
			/// graphie de la maquette (« Mobile 390 x 844 » -> « Mobile — 390
			/// × 844 »). Boîte-combo STATIQUE : le menu Cible (écran 26) est un
			/// chantier à part, la place et le costume sont pris.
			void CorpsCible(NkGuiContext &ctx) {
				if (!SectionOuverte("CIBLE"))
					return;
				// La cible du CADRE ENGLOBANT (reference_4_185519 : elle s'affiche
				// aussi quand la selection est un bouton DANS le cadre).
				const NkUINode *n = CadreCible();
				if (!n)
					return;
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 24.f);
				const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
				// « Cible » court : au corps +4, « Cible du cadre » mangeait la
				// boîte et tronquait la valeur (mesuré sur capture).
				costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 2.f, 20.f),
							   "Cible", ctx.theme.textMuted);
				const NkRect rb = {x0 + 44.f, r.y + 2.f, x1 - x0 - 44.f, 20.f};
				if (n->target.Empty()) {
					// une page fraichement tracee : le catalogue attend son choix
					BoiteChamp(ctx, rb, "\xE2\x80\x94 choisir un format\xE2\x80\xA6");
				} else {
					// « Full HD 1920 x 1080 » -> « Full HD — 1920 × 1080 » : le
					// tiret se place AVANT LE PREMIER NOMBRE (les noms a
					// plusieurs mots du catalogue cassaient l'ancienne regle
					// « apres le premier espace »).
					char aff[96];
					const char *t = n->target.Data();
					uint32 k = 0;
					bool tiret = false;
					for (const char *q = t; *q && k + 5 < sizeof(aff); ++q) {
						if (!tiret && *q == ' ' && q[1] >= '0' && q[1] <= '9') {
							tiret = true;
							aff[k++] = ' ';
							aff[k++] = '\xE2'; // « — »
							aff[k++] = '\x80';
							aff[k++] = '\x94';
							aff[k++] = ' ';
						} else if (*q == 'x' && q > t && q[-1] == ' ' && q[1] == ' ') {
							aff[k++] = '\xC3'; // « × »
							aff[k++] = '\x97';
						} else
							aff[k++] = *q;
					}
					aff[k] = 0;
					BoiteChamp(ctx, rb, aff);
				}
				costume::ChevronCombo7(dl, rb.x + rb.w - 13.f, rb.y + 8.f, ctx.theme.textMuted);
				// LE CATALOGUE S'OUVRE ICI (31/08) : cliquer la boîte ouvre le
				// menu des formats sur la PAGE englobante — il se choisit à la
				// création (page sans cible) ET se change après, même geste.
				if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
					&& NkGuiRectContains(rb, ctx.input.mousePos)) {
					mSt->menuFormat.ouvert = true;
					mSt->menuFormat.ancre = rb;
					mSt->menuFormat.vientDOuvrir = true;
					mSt->menuFormat.page = CadreCibleIndex();
				}
			}

			static void CorpsEvenementsRoleC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsEvenementsRole(ctx);
			}
			static void CorpsEvenementsAjoutesC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsEvenementsAjoutes(ctx);
			}

			/// Une rangée d'événement (écran 4) : pastille 8 px, nom 11 px 500,
			/// liaison 10 px `text_muted` à droite. ⚠️ AUCUNE LIAISON DE DÉMO :
			/// le modèle de comportement n'existe pas encore — toutes les
			/// pastilles sont grises et la colonne liaison reste vide, plutôt
			/// que de mettre en scène des `OnSubmitForm` que rien ne porte.
			void RangeeEvenement(NkGuiContext &ctx, const char *nom) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 28.f);
				const float32 x0 = r.x + 16.f;
				dl.AddCircleFilled({x0 + 4.f, r.y + 14.f}, 4.f, ctx.theme.textMuted);
				costume::TexteGras(dl, F.px11, x0 + 16.f, costume::CentrerY(F.px11, r.y, 28.f),
								   nom, ctx.theme.text, 0.3f);
			}

			/// ÉVÉNEMENTS DU RÔLE : le vocabulaire §4.6 du rôle porté. La table
			/// vit ici en attendant la taxonomie des rôles (écran 6, chantier
			/// « promouvoir ») — dit dans le rapport, pas glissé.
			void CorpsEvenementsRole(NkGuiContext &ctx) {
				static const char *const kEvtsBouton[5] = {"pressé", "relâché", "cliqué",
														   "survol entré", "survol sorti"};
				for (uint32 i = 0; i < 5; ++i)
					RangeeEvenement(ctx, kEvtsBouton[i]);
			}

			/// ÉVÉNEMENTS AJOUTÉS + « Nouvel événement » (bord TIRETÉ, écran 4).
			void CorpsEvenementsAjoutes(NkGuiContext &ctx) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				{
					const NkRect r = ctx.NextItemRect(-1.f, 24.f);
					ctx.BeginDisabled();
					costume::Texte(dl, F.px11, r.x + 16.f, costume::CentrerY(F.px11, r.y, 24.f),
								   "(aucun — le modèle arrive)",
								   ctx.theme.textMuted);
					ctx.EndDisabled();
				}
				const NkRect r = ctx.NextItemRect(-1.f, 46.f);
				const NkRect b = {r.x + 12.f, r.y + 16.f, r.w - 24.f, 30.f};
				// le bord TIRETÉ, segment par segment (la liste de dessin n'a pas
				// de trait pointillé) : 6 px de trait, 4 d'écart.
				auto tirets = [&](NkVec2 a, NkVec2 c, bool horiz) {
					const float32 lg = horiz ? (c.x - a.x) : (c.y - a.y);
					for (float32 t = 0.f; t < lg; t += 10.f) {
						const float32 fin = (t + 6.f < lg) ? t + 6.f : lg;
						if (horiz)
							dl.AddLine({a.x + t, a.y}, {a.x + fin, a.y}, ctx.theme.border, 1.5f);
						else
							dl.AddLine({a.x, a.y + t}, {a.x, a.y + fin}, ctx.theme.border, 1.5f);
					}
				};
				tirets({b.x, b.y}, {b.x + b.w, b.y}, true);
				tirets({b.x, b.y + b.h}, {b.x + b.w, b.y + b.h}, true);
				tirets({b.x, b.y}, {b.x, b.y + b.h}, false);
				tirets({b.x + b.w, b.y}, {b.x + b.w, b.y + b.h}, false);
				const char *lib = "Nouvel événement";
				const float32 wl = costume::Largeur(F.px11, lib);
				const float32 cx = b.x + (b.w - (10.f + 5.f + wl)) * 0.5f;
				const float32 cy = b.y + b.h * 0.5f;
				dl.AddLine({cx + 5.f, cy - 4.f}, {cx + 5.f, cy + 4.f}, ctx.theme.textMuted, 1.3f);
				dl.AddLine({cx + 1.f, cy}, {cx + 9.f, cy}, ctx.theme.textMuted, 1.3f);
				costume::TexteGras(dl, F.px11, cx + 15.f, costume::CentrerY(F.px11, b.y, b.h),
								   lib, ctx.theme.textMuted, 0.3f);
				if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
					&& NkGuiRectContains(b, ctx.input.mousePos))
					mSt->status = NkString(
						"Nouvel événement : à brancher (le modèle de comportement arrive).");
			}

			static void CorpsRoleC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsRole(ctx);
			}
			/// LA RANGÉE « Rôle » (onglet Widget) : la boîte-combo qui OUVRE le
			/// menu des rôles (écrans 5-6-7). La valeur est la clé `role` du
			/// document ; « aucun » sinon.
			void CorpsRole(NkGuiContext &ctx) {
				const NkUINode *n = NoeudCourant();
				if (!n)
					return;
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 24.f);
				const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
				costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 2.f, 20.f),
							   "Rôle", ctx.theme.textMuted);
				const NkRect rb = {x0 + 40.f, r.y + 2.f, x1 - x0 - 40.f, 20.f};
				BoiteChamp(ctx, rb, n->role.Empty() ? "aucun — choisir…" : n->role.Data());
				costume::ChevronCombo7(dl, rb.x + rb.w - 13.f, rb.y + 8.f, ctx.theme.textMuted);
				// (mise en scene) attendre que la mise en page soit posee : au
				// premier passage, l'ancre serait celle d'un dock pas encore cale.
				if (mSt->menuRoleInitial && rb.x > (float32)ctx.viewW * 0.5f) {
					mSt->menuRoleInitial = false;
					mSt->menuRole.ouvert = true;
					mSt->menuRole.ancre = rb;
					mSt->menuRole.vientDOuvrir = true;
				}
				if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
					&& NkGuiRectContains(rb, ctx.input.mousePos)) {
					mSt->menuRole.ouvert = true;
					mSt->menuRole.ancre = rb;
					mSt->menuRole.vientDOuvrir = true;
					mSt->menuRole.filtreFocus = true;
				}
				if (!n->role.Empty()) {
					const NkRect r2 = ctx.NextItemRect(-1.f, 22.f);
					ctx.BeginDisabled();
					costume::Texte(dl, F.px10, r2.x + 12.f,
								   costume::CentrerY(F.px10, r2.y, 22.f),
								   "Les paramètres du rôle arrivent avec la taxonomie.",
								   ctx.theme.textMuted);
					ctx.EndDisabled();
				}
			}
			static void CorpsTypographieC(void *u, NkGuiContext &ctx) {
				static_cast<InspectorPanel *>(u)->CorpsTypographie(ctx);
			}
			// ⚠️ LE MODELE PORTE DU TEXTE DEPUIS LE 2026-08-30 (chaine du
			//    designer, cle `texte`). Le plan (§12.2) : « presente seulement si
			//    l'element porte du texte » — un noeud `forme = text` en porte, et
			//    son contenu s'edite ICI. Police, graisse, taille : a venir avec
			//    le vocabulaire d'apparence.
			void CorpsTypographie(NkGuiContext &ctx) {
				if (!SectionOuverte("TYPOGRAPHIE"))
					return;
				NkUINode *n = NoeudMutable();
				if (!n || !StrEq(n->shape.Data(), "text")) {
					ctx.BeginDisabled();
					nkgui::TextWrapped(ctx, "L'élément sélectionné ne porte pas de texte.");
					ctx.EndDisabled();
					return;
				}
				// LA RÉFÉRENCE (reference_4_185519), rangée par rangée — et chaque
				// contrôle AGIT ou dit pourquoi pas : Police (une seule famille
				// embarquée — le clic le dit), Poids (CYCLE les graisses réelles
				// 400/500/600/700, clé `graisse`), Taille (clé `police_px`),
				// Hauteur ligne / Interlettrage (le modèle ne les porte pas —
				// grisés avec raison), Aligner (clé `texte_aligne`, 3 valeurs
				// réelles + « justifié » qui dit son absence), Décor (grisé),
				// Contenu (clé `texte`).
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const float32 wLib = 60.f;
				// ── Police ──
				{
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   "Police", ctx.theme.textMuted);
					const NkRect rb = {x0 + wLib, r.y + 3.f, x1 - x0 - wLib, 20.f};
					BoiteChamp(ctx, rb, "Inter");
					costume::ChevronCombo7(dl, rb.x + rb.w - 13.f, rb.y + 9.f,
										   ctx.theme.textMuted);
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(rb, ctx.input.mousePos))
						mSt->status = NkString("Police : une seule famille embarquée "
											   "aujourd'hui (Inter, OFL).");
				}
				// ── Poids : le clic CYCLE les graisses réelles ──
				{
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   "Poids", ctx.theme.textMuted);
					const NkRect rb = {x0 + wLib, r.y + 3.f, x1 - x0 - wLib, 20.f};
					const int32 fw = (int32)n->fontWeight;
					const char *nomFw = fw >= 700	? "Bold"
										: fw >= 600 ? "Semi-Bold"
										: fw >= 500 ? "Medium"
										: fw >= 400 ? "Regular"
													: "\xE2\x80\x94 (défaut)";
					BoiteChamp(ctx, rb, nomFw);
					costume::ChevronCombo7(dl, rb.x + rb.w - 13.f, rb.y + 9.f,
										   ctx.theme.textMuted);
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(rb, ctx.input.mousePos)) {
						n->fontWeight = fw >= 700 ? 0.f
										: fw >= 600 ? 700.f
										: fw >= 500 ? 600.f
										: fw >= 400 ? 500.f
													: 400.f;
						mSt->doc.MarkHumanEdit(mSt->selected);
					}
				}
				// ── Taille ──
				{
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f;
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   "Taille", ctx.theme.textMuted);
					const NkRect rt = {x0 + wLib, r.y + 3.f, 48.f, 20.f};
					if (ChampNombre(ctx, "insp.typo.px", rt, n->fontPx, 0.5f, 0.f, 256.f, false,
									true))
						mSt->doc.MarkHumanEdit(mSt->selected);
					costume::Texte(dl, F.px9, rt.x + rt.w + 4.f,
								   costume::CentrerY(F.px9, r.y + 3.f, 20.f), "px",
								   ctx.theme.textMuted);
				}
				// ── Hauteur ligne / Interlettrage : le modèle ne les porte pas ──
				for (int32 li = 0; li < 2; ++li) {
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f;
					ctx.BeginDisabled();
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   li == 0 ? "Hauteur ligne" : "Interlettrage",
								   ctx.theme.textMuted);
					const NkRect rv = {x0 + 76.f, r.y + 3.f, 40.f, 20.f};
					dl.AddRectFilled(rv, CouleurInput(), 4.f);
					dl.AddRect(rv, ctx.theme.border, 1.f, 4.f);
					costume::Texte(dl, F.px11, rv.x + 6.f, costume::CentrerY(F.px11, rv.y, 20.f),
								   "\xE2\x80\x94", ctx.theme.textMuted);
					ctx.EndDisabled();
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(rv, ctx.input.mousePos))
						mSt->status = NkString("Le modèle de texte ne porte pas encore cette "
											   "clé — chantier nommé (vocabulaire Lunacy).");
				}
				// ── Aligner : la clé `texte_aligne` (3 réelles + justifié absent) ──
				{
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   "Aligner", ctx.theme.textMuted);
					const float32 champs0 = x0 + wLib;
					const float32 btnW = (x1 - champs0 - 3.f * 4.f) / 4.f;
					const int32 courant = StrEq(n->alignText.Data(), "centre") ? 1
										  : StrEq(n->alignText.Data(), "droite") ? 2
																				 : 0;
					static const char *const kCles[3] = {"gauche", "centre", "droite"};
					for (int32 i = 0; i < 4; ++i) {
						const NkRect cb = {champs0 + (btnW + 4.f) * (float32)i, r.y + 3.f, btnW,
										   20.f};
						const bool actif = (i == courant);
						if (actif) {
							NkColor voile = ctx.theme.accent;
							voile.a = 44;
							dl.AddRectFilled(cb, voile, 4.f);
							dl.AddRect(cb, ctx.theme.accent, 1.f, 4.f);
						} else {
							dl.AddRectFilled(cb, CouleurInput(), 4.f);
							dl.AddRect(cb, ctx.theme.border, 1.f, 4.f);
						}
						// le glyphe « lignes de texte » : 3 traits, ancrés selon i
						const NkColor gc = actif ? ctx.theme.accent : ctx.theme.textMuted;
						const float32 gw = 12.f, gx0 = cb.x + (cb.w - gw) * 0.5f;
						const float32 gy = cb.y + 5.f;
						for (int32 l = 0; l < 3; ++l) {
							const float32 lw = (l == 1) ? gw : gw * 0.66f;
							float32 lx = gx0; // gauche / justifié
							if (i == 1)
								lx = gx0 + (gw - lw) * 0.5f;
							else if (i == 2)
								lx = gx0 + gw - lw;
							const float32 lw2 = (i == 3) ? gw : lw;
							dl.AddLine({lx, gy + (float32)l * 5.f},
									   {(i == 3 ? gx0 : lx) + lw2, gy + (float32)l * 5.f}, gc,
									   1.4f);
						}
						if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
							&& NkGuiRectContains(cb, ctx.input.mousePos)) {
							if (i < 3) {
								n->alignText = NkString(kCles[i]);
								mSt->doc.MarkHumanEdit(mSt->selected);
							} else
								mSt->status = NkString("Justifié : le modèle de texte ne le "
													   "porte pas encore.");
						}
					}
				}
				// ── Décor : le modèle ne le porte pas — grisé, raison au clic ──
				{
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
					ctx.BeginDisabled();
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   "Décor", ctx.theme.textMuted);
					const float32 champs0 = x0 + wLib;
					const float32 btnW = (x1 - champs0 - 2.f * 4.f) / 3.f;
					for (int32 i = 0; i < 3; ++i) {
						const NkRect cb = {champs0 + (btnW + 4.f) * (float32)i, r.y + 3.f, btnW,
										   20.f};
						dl.AddRectFilled(cb, CouleurInput(), 4.f);
						dl.AddRect(cb, ctx.theme.border, 1.f, 4.f);
						const float32 cxg = cb.x + cb.w * 0.5f;
						if (i == 0)
							dl.AddLine({cxg - 5.f, cb.y + 10.f}, {cxg + 5.f, cb.y + 10.f},
									   ctx.theme.textMuted, 1.4f);
						else if (i == 1) {
							dl.AddLine({cxg - 4.f, cb.y + 6.f}, {cxg - 4.f, cb.y + 12.f},
									   ctx.theme.textMuted, 1.2f);
							dl.AddLine({cxg + 4.f, cb.y + 6.f}, {cxg + 4.f, cb.y + 12.f},
									   ctx.theme.textMuted, 1.2f);
							dl.AddLine({cxg - 5.f, cb.y + 15.f}, {cxg + 5.f, cb.y + 15.f},
									   ctx.theme.textMuted, 1.2f);
						} else
							for (int32 d = -1; d <= 1; ++d)
								dl.AddCircleFilled({cxg + (float32)d * 4.f, cb.y + 10.f}, 1.2f,
												   ctx.theme.textMuted);
						if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
							&& NkGuiRectContains(cb, ctx.input.mousePos))
							mSt->status = NkString("Décorations de texte : le modèle ne les "
												   "porte pas encore — chantier nommé.");
					}
					ctx.EndDisabled();
				}
				// Le tampon SUIT LA SELECTION : en changer recharge le contenu —
				// sans ce garde, editer un texte ecrirait dans celui d'avant.
				// (et il suit l'HISTORIQUE : une annulation change le contenu
				// sans changer la selection — editionGeneration le dit.)
				if (mTexteNode != mSt->selected || mTexteGen != mSt->editionGeneration) {
					mTexteNode = mSt->selected;
					mTexteGen = mSt->editionGeneration;
					const char *t = n->text.Data();
					uint32 i = 0;
					for (; t && t[i] && i + 1 < sizeof(mTexteBuf); ++i)
						mTexteBuf[i] = t[i];
					mTexteBuf[i] = 0;
				}
				if (nkgui::InputText(ctx, "Contenu", mTexteBuf, (int32)sizeof(mTexteBuf))) {
					mSt->doc.nodes[(uint32)mSt->selected].text = NkString(mTexteBuf);
					mSt->doc.MarkHumanEdit(mSt->selected);
				}
			}

			const NkUINode *NoeudCourant() const noexcept {
				return mSt->doc.IsValidIndex(mSt->selected) ? &mSt->doc.nodes[(uint32)mSt->selected]
														    : nullptr;
			}

		private:
			// ⚠️ LA POSITION EST UN RÉSULTAT, PAS UNE DONNÉE (règle de Rodolf du
			//    18/08). Elle est donc LUE dans la disposition calculée et affichée
			//    en lecture seule ; l'outil n'écrira jamais une coordonnée dans le
			//    document.
			// COSTUME BANANI : UNE rangée « X [95] Y [228] » (grille 2, libellés
			// 10 px de 24, champs 20 px). Sous un parent `Free`, les champs
			// éditent posX/posY (la règle du 30/08 : X/Y posés = des champs) ;
			// sous un agencement ils MONTRENT la valeur calculée, boîtes
			// statiques, avec la phrase d'explication dessous — la règle « la
			// position est un résultat » ne bouge pas.
			void CorpsPosition(NkGuiContext &ctx) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 24.f);
				const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
				const float32 colW = (x1 - x0 - 6.f) * 0.5f;
				const float32 fy = r.y + 2.f, fh = 20.f;
				costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, fy, fh), "X",
							   ctx.theme.textMuted);
				const NkRect rx = {x0 + 28.f, fy, colW - 28.f, fh};
				const float32 xc = x0 + colW + 6.f;
				costume::Texte(dl, F.px10, xc, costume::CentrerY(F.px10, fy, fh), "Y",
							   ctx.theme.textMuted);
				const NkRect ry = {xc + 28.f, fy, colW - 28.f, fh};
				NkUINode *n = NoeudMutable();
				if (!n) {
					BoiteChamp(ctx, rx, "-");
					BoiteChamp(ctx, ry, "-");
					return;
				}
				if (ParentKind() == editorkit::NkLayoutKind::Free) {
					bool bouge = false;
					bouge |= ChampNombre(ctx, "insp.pos.x", rx, n->posX, 1.f, -100000.f,
										 100000.f);
					bouge |= ChampNombre(ctx, "insp.pos.y", ry, n->posY, 1.f, -100000.f,
										 100000.f);
					if (bouge)
						mSt->doc.MarkHumanEdit(mSt->selected);
				} else {
					char b[32];
					const bool a = mSt->layout.Has(mSt->selected);
					const NkPaintRect rc =
						a ? mSt->layout.At(mSt->selected) : NkPaintRect{0.f, 0.f, 0.f, 0.f};
					snprintf(b, sizeof(b), a ? "%.0f" : "-", (double)rc.x);
					BoiteChamp(ctx, rx, b);
					snprintf(b, sizeof(b), a ? "%.0f" : "-", (double)rc.y);
					BoiteChamp(ctx, ry, b);
					// ⚠️ PHRASE PLEINE LARGEUR, PAS UNE CELLULE (capture du 29/08 :
					//    coupée en colonne, elle n'expliquait plus).
					nkgui::TextWrapped(ctx, "calculée — jamais écrite dans le document");
				}
			}

			void CorpsTaille(NkGuiContext &ctx) {
				NkUINode *n = NoeudMutable();
				if (!n) {
					auto &F = costume::Fontes();
					const NkRect r = ctx.NextItemRect(-1.f, 24.f);
					costume::Texte(ctx.DL(), F.px10, r.x + 12.f,
								   costume::CentrerY(F.px10, r.y, 24.f), "Largeur",
								   ctx.theme.textMuted);
					BoiteChamp(ctx, {r.x + 12.f + 56.f, r.y + 2.f, r.w - 80.f, 20.f}, "-");
					const NkRect r2 = ctx.NextItemRect(-1.f, 24.f);
					costume::Texte(ctx.DL(), F.px10, r2.x + 12.f,
								   costume::CentrerY(F.px10, r2.y, 24.f), "Hauteur",
								   ctx.theme.textMuted);
					BoiteChamp(ctx, {r2.x + 12.f + 56.f, r2.y + 2.f, r2.w - 80.f, 20.f}, "-");
					return;
				}
				LigneTaille(ctx, "Largeur", n->width);
				LigneTaille(ctx, "Hauteur", n->height);
			}

			/// COSTUME BANANI (InspecteurV2) : la rangée « Largeur : [expand →] »
			/// — libellé 10 px de 52, boîte de 20 avec LE VOCABULAIRE DU FORMAT
			/// (« expand », « fixed 44 »...) et l'icône du mode à droite (flèche
			/// accent pour expand, taquets pour fixed). Puis « min / max » EN
			/// RETRAIT (52), champs de 18, « — » quand la borne n'existe pas.
			/// La VALEUR d'un mode qui en porte une s'édite au GLISSER sur la
			/// boîte (même fonction que le DragFloat d'avant, costume exact) ;
			/// une MÉTRIQUE nommée prime et se nomme dans la boîte, grisée.
			void LigneTaille(NkGuiContext &ctx, const char *titre, NkSizeDecl &d) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const bool porteValeur = d.mode == NkSizeMode::Fixed ||
										 d.mode == NkSizeMode::Fraction || d.mode == NkSizeMode::Weight;
				const bool metrique = d.valueMetric && *d.valueMetric;
				const NkRect r = ctx.NextItemRect(-1.f, 24.f);
				const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
				costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 2.f, 20.f), titre,
							   ctx.theme.textMuted);
				const NkRect rb = {x0 + 56.f, r.y + 2.f, x1 - x0 - 56.f, 20.f};
				char b[96];
				if (metrique)
					snprintf(b, sizeof(b), "métrique « %s »", d.valueMetric);
				else if (porteValeur)
					snprintf(b, sizeof(b), "%s %d", NkSizeModeName(d.mode), (int32)d.value);
				else
					snprintf(b, sizeof(b), "%s", NkSizeModeName(d.mode));
				BoiteChamp(ctx, rb, b);
				// l'icône du mode, à droite dans la boîte
				if (d.mode == NkSizeMode::Expand)
					costume::IcExpand(dl, rb.x + rb.w - 14.f, rb.y + 6.f, ctx.theme.accent);
				else if (d.mode == NkSizeMode::Fixed)
					costume::IcFixed(dl, rb.x + rb.w - 14.f, rb.y + 6.f, ctx.theme.textMuted);
				// la valeur s'édite au glisser (quand elle existe et qu'aucune
				// métrique ne prime)
				if (porteValeur && !metrique) {
					const float32 vitesse = d.mode == NkSizeMode::Fixed ? 1.f
											: d.mode == NkSizeMode::Fraction ? 0.01f : 0.05f;
					const float32 vmax = d.mode == NkSizeMode::Fraction ? 1.f : 4096.f;
					char id[40];
					snprintf(id, sizeof(id), "insp.taille.%s", titre);
					// le glisser passe par le même champ : on repose la boîte en
					// zone de drag sans la redessiner
					float32 avant = d.value;
					if (ChampDrag(ctx, id, rb, d.value, vitesse, 0.f, vmax) && d.value != avant)
						mSt->doc.MarkHumanEdit(mSt->selected);
				}
				// min / max en retrait
				const NkRect r2 = ctx.NextItemRect(-1.f, 22.f);
				const float32 mx0 = r2.x + 12.f + 52.f;
				const float32 moitie = (r2.x + r2.w - 12.f - mx0 - 8.f) * 0.5f;
				costume::Texte(dl, F.px9, mx0, costume::CentrerY(F.px9, r2.y + 2.f, 18.f), "min",
							   ctx.theme.textMuted);
				const NkRect rmin = {mx0 + 26.f, r2.y + 2.f, moitie - 26.f, 18.f};
				const float32 mx1 = mx0 + moitie + 8.f;
				costume::Texte(dl, F.px9, mx1, costume::CentrerY(F.px9, r2.y + 2.f, 18.f), "max",
							   ctx.theme.textMuted);
				const NkRect rmax = {mx1 + 26.f, r2.y + 2.f, moitie - 26.f, 18.f};
				char id[48];
				snprintf(id, sizeof(id), "insp.min.%s", titre);
				bool bouge = false;
				bouge |= ChampNombre(ctx, id, rmin, d.minVal, 1.f, 0.f, 4096.f, true, true);
				snprintf(id, sizeof(id), "insp.max.%s", titre);
				bouge |= ChampNombre(ctx, id, rmax, d.maxVal, 1.f, 0.f, 4096.f, true, true);
				if (bouge)
					mSt->doc.MarkHumanEdit(mSt->selected);
			}

			/// Le glisser SEUL (sans redessiner la boîte) — pour un champ déjà
			/// peint par BoiteChamp dont le texte n'est pas le nombre nu.
			bool ChampDrag(NkGuiContext &ctx, const char *id, const NkRect &r, float32 &v,
						   float32 vitesse, float32 vmin, float32 vmax) {
				const nkgui::NkGuiId gid = ctx.GetId(id);
				bool change = false;
				const bool dans = ctx.popupDepth == 0
								  && NkGuiRectContains(r, ctx.input.mousePos);
				if (dans)
					ctx.wantCursor = nkgui::NkGuiCursor::ResizeEW;
				if (dans && ctx.input.mouseClicked[0]) {
					mDragChamp = gid;
					mDragDernierX = ctx.input.mousePos.x;
				}
				if (mDragChamp == gid) {
					if (!ctx.input.mouseDown[0])
						mDragChamp = 0;
					else {
						const float32 dx = ctx.input.mousePos.x - mDragDernierX;
						if (dx != 0.f) {
							mDragDernierX = ctx.input.mousePos.x;
							float32 nv = v + dx * vitesse;
							if (nv < vmin)
								nv = vmin;
							if (nv > vmax)
								nv = vmax;
							if (nv != v) {
								v = nv;
								change = true;
							}
						}
					}
				}
				return change;
			}

			/// ESPACEMENT (replié par défaut, la maquette) — FONCTIONNEL : la
			/// gouttière et la marge sont des MÉTRIQUES NOMMÉES du document ;
			/// la rangée montre le nom et ÉDITE LA VALEUR (doc.SetMetric) — la
			/// toile suit à l'image même. Un conteneur qui ne nomme rien le dit.
			void CorpsEspacement(NkGuiContext &ctx) {
				if (!SectionOuverte("ESPACEMENT"))
					return;
				const NkUINode *n = NoeudCourant();
				if (!n)
					return;
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				struct L {
						const char *libelle;
						const NkString *nom;
				};
				const L lignes[2] = {{"Gouttière", &n->spacingName}, {"Marge", &n->padName}};
				for (int32 li = 0; li < 2; ++li) {
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
					if (lignes[li].nom->Empty()) {
						ctx.BeginDisabled();
						costume::Texte(dl, F.px10, x0,
									   costume::CentrerY(F.px10, r.y + 3.f, 20.f),
									   lignes[li].libelle, ctx.theme.textMuted);
						costume::Texte(dl, F.px10, x0 + 64.f,
									   costume::CentrerY(F.px10, r.y + 3.f, 20.f),
									   "\xE2\x80\x94 (ce conteneur ne nomme rien)",
									   ctx.theme.textMuted);
						ctx.EndDisabled();
						continue;
					}
					costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f),
								   lignes[li].libelle, ctx.theme.textMuted);
					// le NOM de la métrique (la valeur est UNE pour tout le
					// document — c'est le principe), puis sa valeur, éditable.
					char nm[64];
					snprintf(nm, sizeof(nm), "« %s »", lignes[li].nom->Data());
					costume::Texte(dl, F.px9, x0 + 64.f,
								   costume::CentrerY(F.px9, r.y + 3.f, 20.f), nm,
								   ctx.theme.textMuted);
					const NkRect rv = {x1 - 48.f, r.y + 3.f, 48.f, 20.f};
					float32 v = mSt->doc.Metric(lignes[li].nom->Data(), 0.f);
					char id[48];
					snprintf(id, sizeof(id), "insp.esp.%d", li);
					if (ChampNombre(ctx, id, rv, v, 0.5f, 0.f, 512.f)) {
						mSt->doc.SetMetric(lignes[li].nom->Data(), v);
						mSt->doc.MarkHumanEdit(mSt->selected);
					}
				}
			}

			/// BORDS : « R / Brd » (le modèle porte rayon et bordure depuis le
			/// 31/08 — clés additives `rayon` / `bordure`).
			void CorpsBords(NkGuiContext &ctx) {
				NkUINode *n = NoeudMutable();
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 24.f);
				const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
				const float32 colW = (x1 - x0 - 6.f) * 0.5f;
				const float32 fy = r.y + 2.f, fh = 20.f;
				costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, fy, fh), "R",
							   ctx.theme.textMuted);
				const NkRect rr = {x0 + 28.f, fy, colW - 28.f, fh};
				const float32 xc = x0 + colW + 6.f;
				costume::Texte(dl, F.px10, xc, costume::CentrerY(F.px10, fy, fh), "Brd",
							   ctx.theme.textMuted);
				const NkRect rb = {xc + 28.f, fy, colW - 28.f, fh};
				if (!n) {
					BoiteChamp(ctx, rr, "-");
					BoiteChamp(ctx, rb, "-");
					return;
				}
				bool bouge = false;
				bouge |= ChampNombre(ctx, "insp.bords.r", rr, n->radius, 0.5f, 0.f, 128.f);
				bouge |= ChampNombre(ctx, "insp.bords.b", rb, n->borderW, 0.5f, 0.f, 32.f);
				if (bouge)
					mSt->doc.MarkHumanEdit(mSt->selected);
			}

			// ── ANCRAGE : les quatre bords, quand le PARENT est en `Anchor` ──
			void CorpsAncrage(NkGuiContext &ctx) {
				if (!SectionOuverte("ANCRAGE"))
					return;
				NkUINode *n = NoeudMutable();
				if (!n) {
					designkit::KeyValue(ctx, "Ancrage", "-");
					return;
				}
				if (ParentKind() != editorkit::NkLayoutKind::Anchor) {
					// ⚠️ LA SECTION DIT POURQUOI ELLE NE S'APPLIQUE PAS, elle ne
					//    disparait pas. `anchorEdges` ne vaut que sous un parent
					//    `Anchor` (c'est ecrit sur le champ) : afficher quatre
					//    cases actives ici laisserait editer un reglage sans
					//    effet, et un reglage sans effet est pire qu'un reglage
					//    absent -- on le cherche a l'ecran.
					ctx.BeginDisabled();
					designkit::KeyValue(ctx, "Parent", NkLayoutKindName(ParentKind()));
					nkgui::TextWrapped(ctx, "Le parent n'est pas en « anchor » — l'ancrage ne s'applique pas.");
					ctx.EndDisabled();
					return;
				}
				// InspecteurV2 (Banani §1.6) : le WIDGET D'ANCRAGE graphique --
				// cadre du parent, rectangle central, quatre poignees de bord ;
				// une poignee ACTIVE est accent et RELIEE au bord par un trait,
				// une inactive est grise et detachee. Cliquer bascule le bit --
				// les memes `anchorEdges` que les anciennes cases, MarkHumanEdit
				// pareil : seul le costume change.
				WidgetAncrage(ctx, n);
			}

			// LE WIDGET D'ANCRAGE DE LA RÉFÉRENCE (reference_4_185519) : grand
			// cadre arrondi, CROIX POINTILLÉE au centre, carré central plein, et
			// un SEGMENT ACCENT de chaque bord ancré vers le centre (gauche+droite
			// ancrés = la ligne horizontale traverse, comme la référence).
			// Cliquer une moitié (gauche/droite/haut/bas) bascule le bit —
			// mêmes `anchorEdges`, même MarkHumanEdit : seul le costume grandit.
			void WidgetAncrage(NkGuiContext &ctx, NkUINode *n) {
				const NkRect bande = ctx.NextItemRect(-1.f, 152.f);
				auto &dl = ctx.DL();
				const float32 W = (bande.w - 24.f) < 200.f ? (bande.w - 24.f) : 200.f;
				const float32 H = 140.f;
				const float32 ox = bande.x + (bande.w - W) * 0.5f;
				const float32 oy = bande.y + 4.f;
				const float32 cx = ox + W * 0.5f, cy = oy + H * 0.5f;
				dl.AddRect({ox, oy, W, H}, ctx.theme.border, 1.f, 6.f);
				// la croix pointillée (tirets 3/5)
				{
					NkColor pt = ctx.theme.textMuted;
					pt.a = 90;
					for (float32 t = oy + 8.f; t < oy + H - 8.f; t += 8.f)
						dl.AddLine({cx, t}, {cx, t + 3.f}, pt, 1.f);
					for (float32 t = ox + 8.f; t < ox + W - 8.f; t += 8.f)
						dl.AddLine({t, cy}, {t + 3.f, cy}, pt, 1.f);
				}
				// les segments accent des bords ancrés — AVANT le carré, pour que
				// la ligne passe « derrière » lui comme sur la référence.
				const uint8 e = n->anchorEdges;
				if (e & editorkit::nkanchor::Left)
					dl.AddLine({ox + 3.f, cy}, {cx, cy}, ctx.theme.accent, 2.f);
				if (e & editorkit::nkanchor::Right)
					dl.AddLine({cx, cy}, {ox + W - 3.f, cy}, ctx.theme.accent, 2.f);
				if (e & editorkit::nkanchor::Top)
					dl.AddLine({cx, oy + 3.f}, {cx, cy}, ctx.theme.accent, 2.f);
				if (e & editorkit::nkanchor::Bottom)
					dl.AddLine({cx, cy}, {cx, oy + H - 3.f}, ctx.theme.accent, 2.f);
				// le carré central
				const NkRect ctr = {cx - 28.f, cy - 22.f, 56.f, 44.f};
				dl.AddRectFilled(ctr, ctx.theme.button, 3.f);
				dl.AddRect(ctr, ctx.theme.textMuted, 1.f, 3.f);
				// les quatre zones de bascule (les moitiés hors carré central)
				struct Z {
						uint8 bit;
						NkRect clic;
				};
				const Z zs[4] = {
					{editorkit::nkanchor::Left, {ox, cy - 20.f, ctr.x - ox, 40.f}},
					{editorkit::nkanchor::Right,
					 {ctr.x + ctr.w, cy - 20.f, ox + W - (ctr.x + ctr.w), 40.f}},
					{editorkit::nkanchor::Top, {cx - 20.f, oy, 40.f, ctr.y - oy}},
					{editorkit::nkanchor::Bottom,
					 {cx - 20.f, ctr.y + ctr.h, 40.f, oy + H - (ctr.y + ctr.h)}},
				};
				for (uint32 i = 0; i < 4; ++i) {
					const bool survole = ctx.popupDepth == 0
										 && NkGuiRectContains(zs[i].clic, ctx.input.mousePos);
					if (survole)
						ctx.wantCursor = nkgui::NkGuiCursor::Hand;
					if (survole && ctx.input.mouseClicked[0]) {
						if (e & zs[i].bit)
							n->anchorEdges = (uint8)(n->anchorEdges & ~zs[i].bit);
						else
							n->anchorEdges |= zs[i].bit;
						mSt->doc.MarkHumanEdit(mSt->selected);
					}
				}
			}

			// ── ALIGNEMENT : l'agencement que ce noeud impose a SES ENFANTS ──
			// ⚠️ C'EST LE SENS DU CHAMP, ET IL SE DIT : `layout` s'applique aux
			//    ENFANTS, jamais au noeud lui-meme (c'est ecrit sur le champ, et
			//    l'oublier ferait chercher l'effet au mauvais etage).
			// LA RÉFÉRENCE (reference_4_185519) : deux rangées « H » et « V » de
			// QUATRE boutons chacune (début / centre / fin / étirer) — et les
			// quatre AGISSENT : ce sont les quatre valeurs réelles de `NkAlign`
			// (Étirer comprise — l'ancienne table de 6 icônes ne savait pas la
			// montrer). H/V se traduisent en axes principal/transverse selon
			// l'agencement : Column = V principal, Row = H principal.
			void CorpsAlignement(NkGuiContext &ctx) {
				if (!SectionOuverte("ALIGNEMENT"))
					return;
				NkUINode *n = NoeudMutable();
				if (!n) {
					designkit::KeyValue(ctx, "Agencement", "-");
					return;
				}
				if (n->layout.kind == editorkit::NkLayoutKind::None) {
					ctx.BeginDisabled();
					nkgui::TextWrapped(ctx, "Feuille : ce nœud n'agence pas d'enfants.");
					ctx.EndDisabled();
					return;
				}
				const bool colonne = n->layout.kind == editorkit::NkLayoutKind::Column;
				// H : l'axe horizontal = principal d'un Row, transverse d'un Column.
				editorkit::NkAlign &alignH = colonne ? n->layout.crossAlign : n->layout.mainAlign;
				editorkit::NkAlign &alignV = colonne ? n->layout.mainAlign : n->layout.crossAlign;
				int32 c = RangeeAlignement(ctx, "H", (int32)alignH, false, "insp.align.h");
				if (c >= 0) {
					alignH = (editorkit::NkAlign)c;
					mSt->doc.MarkHumanEdit(mSt->selected);
				}
				c = RangeeAlignement(ctx, "V", (int32)alignV, true, "insp.align.v");
				if (c >= 0) {
					alignV = (editorkit::NkAlign)c;
					mSt->doc.MarkHumanEdit(mSt->selected);
				}
			}

			/// La rangée d'alignement de la RÉFÉRENCE : libellé (« H »/« V ») à
			/// gauche, QUATRE boutons larges qui remplissent la largeur (début /
			/// centre / fin / étirer), l'actif en voile accent. Rend l'indice
			/// cliqué, -1 sinon. Le glyphe : une BARRE d'ancre + deux traits qui
			/// se rangent contre elle — le dessin des inspecteurs Figma/Lunacy.
			int32 RangeeAlignement(NkGuiContext &ctx, const char *label, int32 actif,
								   bool vertical, const char *id) {
				const NkRect bande = ctx.NextItemRect(-1.f, 24.f);
				auto &dl = ctx.DL();
				auto &F = costume::Fontes();
				const float32 x0 = bande.x + 12.f, x1 = bande.x + bande.w - 12.f;
				costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, bande.y, 22.f), label,
							   ctx.theme.textMuted);
				const float32 champs0 = x0 + 24.f;
				const float32 btnW = (x1 - champs0 - 3.f * 4.f) / 4.f;
				int32 choisi = -1;
				for (int32 i = 0; i < 4; ++i) {
					const NkRect cb = {champs0 + (btnW + 4.f) * (float32)i, bande.y, btnW, 22.f};
					if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
						&& NkGuiRectContains(cb, ctx.input.mousePos))
						choisi = i;
					if (i == actif) {
						NkColor voile = ctx.theme.accent;
						voile.a = 44;
						dl.AddRectFilled(cb, voile, 4.f);
						dl.AddRect(cb, ctx.theme.accent, 1.f, 4.f);
					} else {
						dl.AddRectFilled(cb, CouleurInput(), 4.f);
						dl.AddRect(cb, ctx.theme.border, 1.f, 4.f);
					}
					// Le glyphe, en espace 14x14 centre.
					const float32 gx = cb.x + (cb.w - 14.f) * 0.5f, gy = cb.y + 4.f, g = 14.f;
					const NkColor enc = (i == actif) ? ctx.theme.accent : ctx.theme.textMuted;
					auto barre = [&](float32 t) {
						if (vertical)
							dl.AddLine({gx, gy + t}, {gx + g, gy + t}, enc, 1.5f);
						else
							dl.AddLine({gx + t, gy}, {gx + t, gy + g}, enc, 1.5f);
					};
					auto trait = [&](float32 a, float32 b, float32 pos) {
						if (vertical)
							dl.AddLine({gx + pos, gy + a}, {gx + pos, gy + b}, enc, 2.f);
						else
							dl.AddLine({gx + a, gy + pos}, {gx + b, gy + pos}, enc, 2.f);
					};
					switch (i) {
						case 0: // debut : barre au bord, traits colles a elle
							barre(0.f);
							trait(2.f, 10.f, 4.5f);
							trait(2.f, 7.f, 9.5f);
							break;
						case 1: // centre
							barre(g * 0.5f);
							trait(3.f, 11.f, 4.5f);
							trait(4.5f, 9.5f, 9.5f);
							break;
						case 2: // fin
							barre(g);
							trait(4.f, 12.f, 4.5f);
							trait(7.f, 12.f, 9.5f);
							break;
						default: // etirer : deux barres, traits pleine longueur
							barre(0.f);
							barre(g);
							trait(1.5f, 12.5f, 4.5f);
							trait(1.5f, 12.5f, 9.5f);
							break;
					}
				}
				return choisi;
			}

			/// Une rangée « Fond / Texte » du costume : libellé 10 px de 52, boîte
			/// de 20 avec PASTILLE de couleur 12×12 + hexa 11 px. Statique pour
			/// l'instant (poser une couleur au champ : la suite — le glisser d'un
			/// nombre n'a pas de sens ici, il faudra la saisie).
			void RangeeCouleur(NkGuiContext &ctx, const char *libelle, const NkString &hex) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 24.f);
				const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
				costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 2.f, 20.f),
							   libelle, ctx.theme.textMuted);
				const NkRect rb = {x0 + 56.f, r.y + 2.f, x1 - x0 - 56.f, 20.f};
				dl.AddRectFilled(rb, CouleurInput(), 4.f);
				dl.AddRect(rb, ctx.theme.border, 1.f, 4.f);
				if (hex.Empty()) {
					costume::Texte(dl, F.px11, rb.x + 6.f,
								   costume::CentrerY(F.px11, rb.y, rb.h), "\xE2\x80\x94 (thème)",
								   ctx.theme.textMuted);
					return;
				}
				const NkColor c = CouleurHex(hex.Data(), ctx.theme.textMuted);
				dl.AddRectFilled({rb.x + 6.f, rb.y + 4.f, 12.f, 12.f}, c, 2.f);
				dl.AddRect({rb.x + 6.f, rb.y + 4.f, 12.f, 12.f}, ctx.theme.border, 1.f, 2.f);
				costume::Texte(dl, F.px11, rb.x + 6.f + 12.f + 6.f,
							   costume::CentrerY(F.px11, rb.y, rb.h), hex.Data(),
							   ctx.theme.text);
			}

			/// La pastille + le champ hexa ÉDITABLE d'une couleur posée (chaque
			/// contrôle AGIT — recadrage de Rodolf) : vider = revenir au thème.
			/// Rend true si la clé a été écrite.
			bool RangeeCouleurEdit(NkGuiContext &ctx, const char *label, const char *id,
								   char *buf, NkString &cle) {
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				const NkRect r = ctx.NextItemRect(-1.f, 26.f);
				const float32 x0 = r.x + 12.f, x1 = r.x + r.w - 12.f;
				costume::Texte(dl, F.px10, x0, costume::CentrerY(F.px10, r.y + 3.f, 20.f), label,
							   ctx.theme.textMuted);
				const NkRect sw = {x0 + 52.f, r.y + 5.f, 16.f, 16.f};
				if (buf[0]) {
					dl.AddRectFilled(sw, CouleurHex(buf, ctx.theme.textMuted), 3.f);
					dl.AddRect(sw, ctx.theme.border, 1.f, 3.f);
				} else {
					// aucune couleur posée : la case « thème » (barrée)
					dl.AddRect(sw, ctx.theme.border, 1.f, 3.f);
					dl.AddLine({sw.x + 3.f, sw.y + 13.f}, {sw.x + 13.f, sw.y + 3.f},
							   ctx.theme.textMuted, 1.f);
				}
				ctx.SetNextItemRect({sw.x + 24.f, r.y + 3.f, x1 - (sw.x + 24.f), 20.f});
				if (nkgui::InputText(ctx, id, buf, 10)) {
					cle = NkString(buf);
					mSt->doc.MarkHumanEdit(mSt->selected);
					return true;
				}
				return false;
			}

			// ═══════════════════════════════════════════════════════════════════
			//  REMPLISSAGES (Lunacy « FILLS ») — étage 1 du mandat listes
			// ═══════════════════════════════════════════════════════════════════
			// La ligne exacte de `lunacy_props_12` : pastille · hexa · opacité ·
			// œil · poubelle. Et le « + » dans l'en-tête (voir TitreSection).
			//
			// ⚠️ UNE SEULE GRAMMAIRE VISUELLE, LISTE OU PAS. Quand le nœud n'a
			//    que la clé simple `fond`, la section montre UNE ligne — la même
			//    ligne, avec son opacité à 100 et son œil ouvert. On ne montre
			//    pas « l'ancienne rangée Fond » d'un côté et « la liste » de
			//    l'autre : deux présentations pour une seule notion, c'est ce qui
			//    apprend à l'utilisateur qu'il y a deux notions.
			//
			// ⚠️ ET LE PREMIER GESTE QUI A BESOIN DE PLUS QUE `fond` MATÉRIALISE.
			//    Toucher l'opacité, l'œil, ou ajouter une seconde ligne : le nœud
			//    bascule dans la forme liste, et le fichier gagne ses clés
			//    `fond_i`. Tant qu'on ne touche qu'à la couleur, RIEN NE BOUGE
			//    dans le format — c'est ce qui tient la promesse d'additivité.
			void CorpsRemplissages(NkGuiContext &ctx) {
				if (!SectionOuverte("REMPLISSAGES"))
					return;
				NkUINode *n = NoeudMutable();
				if (!n) {
					designkit::KeyValue(ctx, "Remplissages", "-");
					return;
				}
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				// Les tampons de saisie, resynchronisés comme ceux d'APPARENCE :
				// la sélection ET l'historique les rechargent (une annulation doit
				// se voir dans le champ, pas seulement dans le document).
				if (mFillsNode != mSt->selected || mFillsGen != mSt->editionGeneration) {
					mFillsNode = mSt->selected;
					mFillsGen = mSt->editionGeneration;
					for (uint32 i = 0; i < kMaxFillsUI; ++i)
						mFillsBuf[i][0] = '\0';
					if (n->fills.Empty())
						snprintf(mFillsBuf[0], sizeof(mFillsBuf[0]), "%s", n->fill.Data());
					else
						for (uint32 i = 0; i < (uint32)n->fills.Size() && i < kMaxFillsUI; ++i)
							snprintf(mFillsBuf[i], sizeof(mFillsBuf[i]), "%s",
									 n->fills[i].couleur.Data());
				}
				// ⚠️ RIEN DE POSE = AUCUNE LIGNE, ET C'EST CE QUE MONTRE LA
				//    CAPTURE QUI M'A CORRIGE. Ma premiere version dessinait
				//    toujours une ligne : sur un noeud sans fond, elle affichait
				//    une pastille barree, un champ vide et « 100 % » -- une
				//    opacite pour un remplissage qui n'existe pas. Lunacy, lui,
				//    ne montre que le « + ». Une ligne vide n'est pas neutre :
				//    elle affirme qu'il y a un remplissage.
				const bool rienDePose = n->fills.Empty() && n->fill.Empty();
				const uint32 nb = rienDePose ? 0u : (n->fills.Empty() ? 1u : (uint32)n->fills.Size());
				// ⚠️ ON DESSINE DU DERNIER AU PREMIER. Le dernier remplissage se
				//    peint PAR-DESSUS ; Lunacy le montre donc EN HAUT de la liste.
				//    Afficher dans l'ordre du modèle aurait mis « celui du dessus »
				//    tout en bas — l'écran dirait l'inverse de la toile.
				for (uint32 vi = 0; vi < nb; ++vi) {
					const uint32 i = nb - 1u - vi; // l'index MODÈLE
					if (i >= kMaxFillsUI)
						continue;
					const bool simple = n->fills.Empty();
					const NkRect r = ctx.NextItemRect(-1.f, 26.f);
					// LES COLONNES VIENNENT DU GROUPE, pas de cette section.
					const ColonnesRangee col = ColonnesDe(r);
					const bool visible = simple ? true : n->fills[i].visible;
					const NkColor encre = visible ? ctx.theme.text : ctx.theme.textDisabled;
					// 1. la PASTILLE de couleur
					const NkRect sw = {col.pastille, r.y + 5.f, 16.f, 16.f};
					if (mFillsBuf[i][0]) {
						dl.AddRectFilled(sw, CouleurHex(mFillsBuf[i], ctx.theme.textMuted), 3.f);
						dl.AddRect(sw, ctx.theme.border, 1.f, 3.f);
					} else {
						dl.AddRect(sw, ctx.theme.border, 1.f, 3.f);
						dl.AddLine({sw.x + 3.f, sw.y + 13.f}, {sw.x + 13.f, sw.y + 3.f},
								   ctx.theme.textMuted, 1.f);
					}
					// 2. l'HEXA — la seule écriture qui NE matérialise PAS : tant
					//    qu'on ne change qu'une couleur, un document d'avant garde
					//    sa clé simple et son octet près.
					char idHex[32];
					snprintf(idHex, sizeof(idHex), "##insp.fill.hex%u", i);
					ctx.SetNextItemRect({col.hexX, r.y + 3.f, col.hexW, 20.f});
					if (nkgui::InputText(ctx, idHex, mFillsBuf[i], 10)) {
						if (simple)
							n->fill = NkString(mFillsBuf[i]);
						else
							n->fills[i].couleur = NkString(mFillsBuf[i]);
						mSt->doc.MarkHumanEdit(mSt->selected);
					}
					// 3. l'OPACITÉ — la toucher MATÉRIALISE (la clé simple ne sait
					//    pas la dire).
					char idOp[32];
					snprintf(idOp, sizeof(idOp), "insp.fill.op%u", i);
					const NkRect ro = {col.opacX, r.y + 3.f, 30.f, 20.f};
					float32 op = simple ? 100.f : n->fills[i].opacite;
					if (ChampNombre(ctx, idOp, ro, op, 1.f, 0.f, 100.f)) {
						n->MaterialiserFills();
						const uint32 k = simple ? 0u : i;
						if (k < (uint32)n->fills.Size())
							n->fills[k].opacite = op;
						mSt->doc.MarkHumanEdit(mSt->selected);
					}
					costume::Texte(dl, F.px9, ro.x + ro.w + 3.f,
								   costume::CentrerY(F.px9, r.y + 3.f, 20.f), "%",
								   ctx.theme.textMuted);
					// 4. la POUBELLE — retire CE remplissage. Sur la forme simple
					//    elle vide la clé `fond` : c'est le même geste, « il n'y a
					//    plus de remplissage ».
					{
						const NkRect rp = {col.poubX, r.y + 6.f, 14.f, 14.f};
						const bool sv =
							ctx.popupDepth == 0 && NkGuiRectContains(rp, ctx.input.mousePos);
						costume::IcPoubelle(dl, rp.x + 1.f, rp.y + 1.f,
											sv ? ctx.theme.accent : ctx.theme.textMuted);
						if (sv && ctx.input.mouseClicked[0]) {
							if (simple)
								n->fill = NkString();
							else if (i < (uint32)n->fills.Size())
								n->fills.RemoveAt(i);
							mFillsGen = -1; // les tampons se rechargent : la liste a glissé
							mSt->doc.MarkHumanEdit(mSt->selected);
							mSt->status = NkString("Remplissage retiré — Ctrl+Z le ramène.");
						}
					}
					// 5. l'ŒIL — masque sans perdre la couleur. Matérialise aussi.
					{
						const NkRect re = {col.oeilX, r.y + 6.f, 14.f, 14.f};
						const bool sv =
							ctx.popupDepth == 0 && NkGuiRectContains(re, ctx.input.mousePos);
						const NkColor c = sv ? ctx.theme.accent
											 : (visible ? ctx.theme.textMuted
														: ctx.theme.textDisabled);
						if (visible)
							costume::IcOeil(dl, re.x + 1.f, re.y + 1.f, c);
						else
							costume::IcOeilBarre(dl, re.x + 1.f, re.y + 1.f, c);
						if (sv && ctx.input.mouseClicked[0]) {
							n->MaterialiserFills();
							const uint32 k = simple ? 0u : i;
							if (k < (uint32)n->fills.Size())
								n->fills[k].visible = !n->fills[k].visible;
							mSt->doc.MarkHumanEdit(mSt->selected);
						}
					}
					(void)encre;
				}
				if (rienDePose) {
					// On le DIT, plutôt qu'une ligne vide muette. Message COURT :
					// le panneau fait 235 px, et la capture du 01/09 montrait la
					// version longue coupée en plein milieu -- un texte tronqué
					// est un texte qui n'a pas été mesuré dans sa colonne.
					const NkRect r = ctx.NextItemRect(-1.f, 20.f);
					costume::Texte(dl, F.px9, r.x + 12.f,
								   costume::CentrerY(F.px9, r.y, 18.f),
								   "Aucun — « + » en ajoute.", ctx.theme.textDisabled);
				}
			}
			/// Le geste du « + » de l'en-tête : matérialise puis empile un
			/// remplissage NEUF au-dessus. Un pas d'annulation, comme tout geste.
			void AjouterRemplissage() {
				NkUINode *n = NoeudMutable();
				if (!n)
					return;
				if ((uint32)n->fills.Size() >= kMaxFillsUI) {
					mSt->status = NkString("Remplissages : la pile de l'inspecteur en montre "
										   "huit au plus (le modèle, lui, n'a pas de borne).");
					return;
				}
				// ⚠️ DEUX CLICS DONNAIENT TROIS REMPLISSAGES, ET C'EST LA CAPTURE
				//    QUI L'A MONTRÉ. Sur un nœud SANS fond, matérialiser inventait
				//    un premier remplissage (`#ffffff`, que personne n'avait
				//    demandé) et le « + » en empilait un second : le premier clic
				//    en posait DEUX. Matérialiser n'a de sens que s'il y a quelque
				//    chose À PRÉSERVER — la clé simple. Sans elle, le « + » pose
				//    simplement le premier.
				if (!n->fill.Empty())
					n->MaterialiserFills();
				NkRemplissage f;
				f.couleur = NkString("#ffffff");
				n->fills.PushBack(f);
				mFillsGen = -1; // recharger les tampons
				mSt->doc.MarkHumanEdit(mSt->selected);
				mSt->status = NkString("Remplissage ajouté — Ctrl+Z le retire.");
			}

			// ═══════════════════════════════════════════════════════════════════
			//  BORDURES (Lunacy « BORDERS ») — étage 2 du mandat listes
			// ═══════════════════════════════════════════════════════════════════
			// Les DEUX lignes de `lunacy_props_12` : la ligne de couleur (pastille,
			// hexa, opacité, œil, poubelle) et la ligne de trait (épaisseur +
			// POSITION). Même grammaire que REMPLISSAGES, volontairement : deux
			// sections qui font la même chose doivent se ressembler, sinon
			// l'utilisateur apprend deux gestes pour une seule idée.
			void CorpsBordures(NkGuiContext &ctx) {
				if (!SectionOuverte("BORDURES"))
					return;
				NkUINode *n = NoeudMutable();
				if (!n) {
					designkit::KeyValue(ctx, "Bordures", "-");
					return;
				}
				auto &F = costume::Fontes();
				auto &dl = ctx.DL();
				if (mBordsNode != mSt->selected || mBordsGen != mSt->editionGeneration) {
					mBordsNode = mSt->selected;
					mBordsGen = mSt->editionGeneration;
					for (uint32 i = 0; i < kMaxFillsUI; ++i)
						mBordsBuf[i][0] = '\0';
					if (n->borders.Empty())
						snprintf(mBordsBuf[0], sizeof(mBordsBuf[0]), "%s", n->borderColor.Data());
					else
						for (uint32 i = 0; i < (uint32)n->borders.Size() && i < kMaxFillsUI; ++i)
							snprintf(mBordsBuf[i], sizeof(mBordsBuf[i]), "%s",
									 n->borders[i].couleur.Data());
				}
				const bool rienDePose = n->borders.Empty() && n->borderColor.Empty();
				const uint32 nb =
					rienDePose ? 0u : (n->borders.Empty() ? 1u : (uint32)n->borders.Size());
				// Du DERNIER au premier, comme les remplissages : le dernier se
				// peint par-dessus, il se montre donc en haut.
				for (uint32 vi = 0; vi < nb; ++vi) {
					const uint32 i = nb - 1u - vi;
					if (i >= kMaxFillsUI)
						continue;
					const bool simple = n->borders.Empty();
					// ── LIGNE 1 : couleur, opacité, œil, poubelle
					{
						const NkRect r = ctx.NextItemRect(-1.f, 26.f);
						// LES MEMES COLONNES QUE REMPLISSAGES, par construction.
						const ColonnesRangee col = ColonnesDe(r);
						const bool visible = simple ? true : n->borders[i].visible;
						const NkRect sw = {col.pastille, r.y + 5.f, 16.f, 16.f};
						if (mBordsBuf[i][0]) {
							dl.AddRectFilled(sw, CouleurHex(mBordsBuf[i], ctx.theme.textMuted),
											 3.f);
							dl.AddRect(sw, ctx.theme.border, 1.f, 3.f);
						} else {
							dl.AddRect(sw, ctx.theme.border, 1.f, 3.f);
							dl.AddLine({sw.x + 3.f, sw.y + 13.f}, {sw.x + 13.f, sw.y + 3.f},
									   ctx.theme.textMuted, 1.f);
						}
						char idHex[32];
						snprintf(idHex, sizeof(idHex), "##insp.bord.hex%u", i);
						ctx.SetNextItemRect({col.hexX, r.y + 3.f, col.hexW, 20.f});
						if (nkgui::InputText(ctx, idHex, mBordsBuf[i], 10)) {
							if (simple)
								n->borderColor = NkString(mBordsBuf[i]);
							else
								n->borders[i].couleur = NkString(mBordsBuf[i]);
							mSt->doc.MarkHumanEdit(mSt->selected);
						}
						char idOp[32];
						snprintf(idOp, sizeof(idOp), "insp.bord.op%u", i);
						const NkRect ro = {col.opacX, r.y + 3.f, 30.f, 20.f};
						float32 op = simple ? 100.f : n->borders[i].opacite;
						if (ChampNombre(ctx, idOp, ro, op, 1.f, 0.f, 100.f)) {
							n->MaterialiserBorders();
							const uint32 k = simple ? 0u : i;
							if (k < (uint32)n->borders.Size())
								n->borders[k].opacite = op;
							mSt->doc.MarkHumanEdit(mSt->selected);
						}
						costume::Texte(dl, F.px9, ro.x + ro.w + 3.f,
									   costume::CentrerY(F.px9, r.y + 3.f, 20.f), "%",
									   ctx.theme.textMuted);
						{
							const NkRect rp = {col.poubX, r.y + 6.f, 14.f, 14.f};
							const bool sv =
								ctx.popupDepth == 0 && NkGuiRectContains(rp, ctx.input.mousePos);
							costume::IcPoubelle(dl, rp.x + 1.f, rp.y + 1.f,
												sv ? ctx.theme.accent : ctx.theme.textMuted);
							if (sv && ctx.input.mouseClicked[0]) {
								if (simple) {
									n->borderColor = NkString();
									n->borderW = 0.f;
								} else if (i < (uint32)n->borders.Size())
									n->borders.RemoveAt(i);
								mBordsGen = -1;
								mSt->doc.MarkHumanEdit(mSt->selected);
								mSt->status = NkString("Bordure retirée — Ctrl+Z la ramène.");
							}
						}
						{
							const NkRect re = {col.oeilX, r.y + 6.f, 14.f, 14.f};
							const bool sv =
								ctx.popupDepth == 0 && NkGuiRectContains(re, ctx.input.mousePos);
							const NkColor c = sv ? ctx.theme.accent
												 : (visible ? ctx.theme.textMuted
															: ctx.theme.textDisabled);
							if (visible)
								costume::IcOeil(dl, re.x + 1.f, re.y + 1.f, c);
							else
								costume::IcOeilBarre(dl, re.x + 1.f, re.y + 1.f, c);
							if (sv && ctx.input.mouseClicked[0]) {
								n->MaterialiserBorders();
								const uint32 k = simple ? 0u : i;
								if (k < (uint32)n->borders.Size())
									n->borders[k].visible = !n->borders[k].visible;
								mSt->doc.MarkHumanEdit(mSt->selected);
							}
						}
					}
					// ── LIGNE 2 : épaisseur + POSITION (la ligne « 1  Outside ⌄ »)
					{
						const NkRect r = ctx.NextItemRect(-1.f, 24.f);
						const float32 x0 = r.x + 34.f, x1 = r.x + r.w - 12.f;
						char idEp[32];
						snprintf(idEp, sizeof(idEp), "insp.bord.ep%u", i);
						const NkRect re = {x0, r.y + 2.f, 40.f, 20.f};
						float32 ep = simple ? (n->borderW > 0.f ? n->borderW : 1.f)
											: n->borders[i].epaisseur;
						if (ChampNombre(ctx, idEp, re, ep, 0.25f, 0.f, 64.f)) {
							if (simple)
								n->borderW = ep;
							else
								n->borders[i].epaisseur = ep;
							mSt->doc.MarkHumanEdit(mSt->selected);
						}
						costume::Texte(dl, F.px9, re.x + re.w + 4.f,
									   costume::CentrerY(F.px9, r.y + 2.f, 20.f), "px",
									   ctx.theme.textMuted);
						// LA POSITION — un bouton qui CYCLE, et qui DIT sa valeur.
						// ⚠️ Lunacy ouvre un menu déroulant ; on cycle, faute d'un
						//    combo assez étroit pour 235 px. Ce qui compte est que
						//    les trois valeurs soient atteignables ET honorées au
						//    dessin (elles le sont : cf. NkGCadre). Le déroulant
						//    est un habillage, pas une capacité — il est nommé.
						const NkBordurePos pos =
							simple ? NkBordurePos::Interieur : n->borders[i].position;
						const char *nomPos = (pos == NkBordurePos::Interieur) ? "intérieur"
											 : (pos == NkBordurePos::Centre)  ? "centré"
																			  : "extérieur";
						const float32 pw = costume::Largeur(F.px10, nomPos) + 18.f;
						const NkRect rp = {x1 - pw, r.y + 2.f, pw, 20.f};
						const bool sv =
							ctx.popupDepth == 0 && NkGuiRectContains(rp, ctx.input.mousePos);
						dl.AddRectFilled(rp, CouleurInput(), 4.f);
						dl.AddRect(rp, sv ? ctx.theme.accent : ctx.theme.border, 1.f, 4.f);
						costume::Texte(dl, F.px10, rp.x + 6.f,
									   costume::CentrerY(F.px10, rp.y, 20.f), nomPos,
									   ctx.theme.text);
						costume::ChevronCombo7(dl, rp.x + rp.w - 11.f, rp.y + 7.5f,
											   ctx.theme.textMuted);
						if (sv && ctx.input.mouseClicked[0]) {
							n->MaterialiserBorders();
							const uint32 k = simple ? 0u : i;
							if (k < (uint32)n->borders.Size()) {
								const NkBordurePos suivant =
									(n->borders[k].position == NkBordurePos::Interieur)
										? NkBordurePos::Centre
									: (n->borders[k].position == NkBordurePos::Centre)
										? NkBordurePos::Exterieur
										: NkBordurePos::Interieur;
								n->borders[k].position = suivant;
								mSt->status = NkString(
									suivant == NkBordurePos::Interieur   ? "Bordure : intérieure."
									: suivant == NkBordurePos::Centre    ? "Bordure : centrée."
																		 : "Bordure : extérieure.");
							}
							mSt->doc.MarkHumanEdit(mSt->selected);
						}
					}
				}
				if (rienDePose) {
					const NkRect r = ctx.NextItemRect(-1.f, 20.f);
					costume::Texte(dl, F.px9, r.x + 12.f, costume::CentrerY(F.px9, r.y, 18.f),
								   "Aucune — « + » en ajoute.", ctx.theme.textDisabled);
				}
			}
			/// Le « + » de BORDURES. Même règle que pour les remplissages : on ne
			/// matérialise que s'il y a une clé simple À PRÉSERVER.
			void AjouterBordure() {
				NkUINode *n = NoeudMutable();
				if (!n)
					return;
				if ((uint32)n->borders.Size() >= kMaxFillsUI) {
					mSt->status = NkString("Bordures : la pile de l'inspecteur en montre huit "
										   "au plus (le modèle, lui, n'a pas de borne).");
					return;
				}
				if (!n->borderColor.Empty())
					n->MaterialiserBorders();
				NkBordure b;
				b.couleur = NkString("#000000");
				n->borders.PushBack(b);
				mBordsGen = -1;
				mSt->doc.MarkHumanEdit(mSt->selected);
				mSt->status = NkString("Bordure ajoutée — Ctrl+Z la retire.");
			}

			// ── APPARENCE (reference_4_185519, FONCTIONNELLE) : [Texte],
			//    Bordure (couleur + épaisseur), Arrondi, Opacité. Chaque contrôle
			//    écrit sa clé du document (couleur_texte / couleur_bord /
			//    bordure / rayon) — la toile suit à l'image même.
			// ⚠️ « Fond » A QUITTÉ CETTE SECTION pour REMPLISSAGES : le laisser
			//    ici en plus aurait donné deux endroits pour écrire la même clé,
			//    et le second aurait ignoré la liste.
			void CorpsApparence(NkGuiContext &ctx) {
				if (!SectionOuverte("APPARENCE"))
					return;
				NkUINode *n = NoeudMutable();
				if (!n) {
					designkit::KeyValue(ctx, "Apparence", "-");
					return;
				}
				// Un nœud DESSINÉ (forme) porte l'apparence posée.
				if (!n->shape.Empty() || !n->fill.Empty() || !n->textColor.Empty()) {
					auto &F = costume::Fontes();
					auto &dl = ctx.DL();
					// tampons synchronisés sur la sélection ET l'historique (le
					// patron mTexteBuf — une annulation les recharge)
					if (mApparNode != mSt->selected || mApparGen != mSt->editionGeneration) {
						mApparNode = mSt->selected;
						mApparGen = mSt->editionGeneration;
						snprintf(mFondBuf, sizeof(mFondBuf), "%s", n->fill.Data());
						snprintf(mTexteColBuf, sizeof(mTexteColBuf), "%s", n->textColor.Data());
						snprintf(mBordColBuf, sizeof(mBordColBuf), "%s", n->borderColor.Data());
					}
					// (« Fond » vit desormais dans REMPLISSAGES — cf. son commentaire.)
					if (StrEq(n->shape.Data(), "text"))
						RangeeCouleurEdit(ctx, "Texte", "##insp.app.texte", mTexteColBuf,
										  n->textColor);
					// (« Bordure » vit desormais dans BORDURES — meme raison que
					//  « Fond » : deux endroits pour ecrire une meme cle, et le
					//  second aurait ignore la liste.)
					// Arrondi (la clé `rayon` — les rayons PAR COIN viendront avec
					// le vocabulaire Lunacy, nommés au rapport).
					{
						const NkRect r = ctx.NextItemRect(-1.f, 26.f);
						const float32 x0 = r.x + 12.f;
						costume::Texte(dl, F.px10, x0,
									   costume::CentrerY(F.px10, r.y + 3.f, 20.f), "Arrondi",
									   ctx.theme.textMuted);
						const NkRect rr = {x0 + 52.f, r.y + 3.f, 48.f, 20.f};
						// MULTI-SÉLECTION COMPRISE : « — » si les rayons diffèrent.
						ChampNombreMulti(
							ctx, "insp.app.rayon", rr, 0.5f, 0.f, 128.f,
							[](const NkUINode &q) { return q.radius; },
							[](NkUINode &q, float32 v) { q.radius = v; });
						costume::Texte(dl, F.px9, rr.x + rr.w + 4.f,
									   costume::CentrerY(F.px9, r.y + 3.f, 20.f), "px",
									   ctx.theme.textMuted);
					}
					// ── ROTATION ET MIROIRS (Lunacy, bandeau du haut) ────────
					// Retour de Rodolf, 01/09 : « dans propriétés il n'y a pas
					// miroir, rotation etc. » Trois des neuf manques de Q42.
					// ⚠️ ILS SONT DANS APPARENCE ET PAS DANS DISPOSITION, ET C'EST
					//    UNE DÉCISION : la disposition calcule des boîtes DROITES
					//    (c'est ce qui fait marcher l'aperçu depuis août), et la
					//    rotation ne la touche pas — elle agit sur le dessin et sur
					//    le clic. Les ranger dans DISPOSITION aurait laissé croire
					//    qu'un objet tourné pousse ses voisins. Il ne les pousse pas.
					{
						const bool peut = NkPeutTourner(*n);
						const NkRect r = ctx.NextItemRect(-1.f, 26.f);
						const float32 x0 = r.x + 12.f;
						if (!peut)
							ctx.BeginDisabled();
						costume::Texte(dl, F.px10, x0,
									   costume::CentrerY(F.px10, r.y + 3.f, 20.f), "Rotation",
									   ctx.theme.textMuted);
						const NkRect rr = {x0 + 52.f, r.y + 3.f, 48.f, 20.f};
						if (peut) {
							ChampNombreMulti(
								ctx, "insp.app.rotation", rr, 1.f, -360.f, 360.f,
								[](const NkUINode &q) { return q.rotation; },
								[](NkUINode &q, float32 v) { q.rotation = NkAngleNormalise(v); });
						} else {
							dl.AddRectFilled(rr, CouleurInput(), 4.f);
							dl.AddRect(rr, ctx.theme.border, 1.f, 4.f);
							costume::Texte(dl, F.px11, rr.x + 6.f,
										   costume::CentrerY(F.px11, rr.y, 20.f), "—",
										   ctx.theme.textMuted);
						}
						costume::Texte(dl, F.px9, rr.x + rr.w + 4.f,
									   costume::CentrerY(F.px9, r.y + 3.f, 20.f), "°",
									   ctx.theme.textMuted);
						if (!peut) {
							ctx.EndDisabled();
							// ⚠️ UN CHAMP GRISÉ MUET EST PIRE QU'UN CHAMP ABSENT :
							//    il montre une capacité sans dire ce qui manque.
							if (ctx.popupDepth == 0 && NkGuiRectContains(r, ctx.input.mousePos))
								mSt->status = NkString(NkRaisonPasDeRotation());
						} else if (!NkPeintureSaitTourner(*n) && n->rotation != 0.f
								   && ctx.popupDepth == 0
								   && NkGuiRectContains(r, ctx.input.mousePos)) {
							// La rotation est ENREGISTRÉE sur un texte, et le peintre
							// ne la rendra pas. On le dit plutôt que de laisser
							// croire à un bug.
							mSt->status = NkString(NkRaisonRotationTexte());
						}
					}
					// LES DEUX MIROIRS — deux bascules, pas un champ.
					// ⚠️ ELLES SONT ÉCRITES ENSEMBLE, EN UNE BOUCLE DE DEUX, parce
					//    que ce sont des chemins frères au sens strict : même
					//    dessin, même geste, même écriture, un seul axe de
					//    différence. Deux blocs recopiés auraient divergé au
					//    premier ajustement de largeur — le défaut payé trois fois
					//    sur les colonnes des listes.
					{
						const NkRect r = ctx.NextItemRect(-1.f, 26.f);
						const float32 x0 = r.x + 12.f;
						costume::Texte(dl, F.px10, x0,
									   costume::CentrerY(F.px10, r.y + 3.f, 20.f), "Miroir",
									   ctx.theme.textMuted);
						for (uint32 k = 0; k < 2; ++k) {
							const bool actif = (k == 0) ? n->miroirH : n->miroirV;
							const char *lib = (k == 0) ? "H" : "V";
							const NkRect rb = {x0 + 52.f + (float32)k * 26.f, r.y + 3.f, 22.f,
											   20.f};
							const bool sv = ctx.popupDepth == 0
											&& NkGuiRectContains(rb, ctx.input.mousePos);
							dl.AddRectFilled(rb, actif ? ctx.theme.accent : CouleurInput(), 4.f);
							dl.AddRect(rb, sv ? ctx.theme.accent : ctx.theme.border, 1.f, 4.f);
							costume::Texte(dl, F.px10,
										   rb.x + (22.f - costume::Largeur(F.px10, lib)) * 0.5f,
										   costume::CentrerY(F.px10, rb.y, 20.f), lib,
										   actif ? ctx.theme.panel : ctx.theme.text);
							if (sv && ctx.input.mouseClicked[0]) {
								if (k == 0)
									n->miroirH = !n->miroirH;
								else
									n->miroirV = !n->miroirV;
								mSt->doc.MarkHumanEdit(mSt->selected);
								char msg[112];
								snprintf(msg, sizeof(msg), "Miroir %s : %s.",
										 k == 0 ? "horizontal" : "vertical",
										 (k == 0 ? n->miroirH : n->miroirV) ? "activé"
																			: "désactivé");
								mSt->status = NkString(msg);
							}
						}
					}
					// Opacité : LE MODÈLE NE LA PORTE PAS — grisée, avec la raison.
					{
						const NkRect r = ctx.NextItemRect(-1.f, 26.f);
						const float32 x0 = r.x + 12.f;
						ctx.BeginDisabled();
						costume::Texte(dl, F.px10, x0,
									   costume::CentrerY(F.px10, r.y + 3.f, 20.f), "Opacité",
									   ctx.theme.textMuted);
						const NkRect ro = {x0 + 52.f, r.y + 3.f, 48.f, 20.f};
						dl.AddRectFilled(ro, CouleurInput(), 4.f);
						dl.AddRect(ro, ctx.theme.border, 1.f, 4.f);
						costume::Texte(dl, F.px11, ro.x + 6.f,
									   costume::CentrerY(F.px11, ro.y, 20.f), "100",
									   ctx.theme.textMuted);
						costume::Texte(dl, F.px9, ro.x + ro.w + 4.f,
									   costume::CentrerY(F.px9, r.y + 3.f, 20.f), "%",
									   ctx.theme.textMuted);
						ctx.EndDisabled();
						if (ctx.popupDepth == 0 && ctx.input.mouseClicked[0]
							&& NkGuiRectContains(ro, ctx.input.mousePos))
							mSt->status = NkString("Opacité : le modèle ne la porte pas encore "
												   "(vocabulaire d'apparence, chantier nommé).");
					}
					return;
				}
				if (n->IsFrame()) {
					ctx.BeginDisabled();
					nkgui::TextWrapped(ctx, "Un cadre n'a pas d'apparence propre — elle vient du thème.");
					ctx.EndDisabled();
					return;
				}
				const NkComponentDecl *d = NkComponentRegistry::Find(n->component.Data());
				if (!d || d->tokenCount == 0) {
					ctx.BeginDisabled();
					nkgui::TextWrapped(ctx, "Ce composant ne déclare aucun jeton d'apparence.");
					ctx.EndDisabled();
					return;
				}
				// ⚠️ LECTURE SEULE, ET CE N'EST PAS UNE PARESSE : editer un jeton,
				//    c'est le geste « donner un role » du §14ter -- « le moment
				//    decisif de tout l'outil », qui a sa ligne dediee dans
				//    l'en-tete du panneau, pas une cellule d'inspecteur. On montre
				//    ici la VERITE EFFECTIVE (jeton -> role, surcharge ou herite),
				//    celle que le peintre lit.
				char b[96];
				for (uint16 t = 0; t < d->tokenCount; ++t) {
					const char *nom = d->tokens[t].name;
					const char *role = n->instance.TokenRole(nom);
					snprintf(b, sizeof(b), "%s%s", role ? role : "?",
							 n->instance.IsTokenOverridden(nom) ? "  (surchargé)" : "");
					designkit::KeyValue(ctx, nom, b);
				}
			}

			// ── OUTILS PARTAGES DES CORPS ─────────────────────────────────────
			editorkit::NkLayoutKind ParentKind() const {
				const NkUINode *n = NoeudCourant();
				if (!n || !mSt->doc.IsValidIndex(n->parent))
					return editorkit::NkLayoutKind::None;
				return mSt->doc.nodes[(uint32)n->parent].layout.kind;
			}

			NkUINode *NoeudMutable() {
				return mSt->doc.IsValidIndex(mSt->selected) ? &mSt->doc.nodes[(uint32)mSt->selected]
															: nullptr;
			}

			static void Decrire(const NkSizeDecl &s, char *out, nkentseu::usize n) {
				const char *mode = NkSizeModeName(s.mode);
				if (s.mode == NkSizeMode::Fixed || s.mode == NkSizeMode::Fraction
					|| s.mode == NkSizeMode::Weight)
					snprintf(out, n, "%s %.0f", mode, (double)s.value);
				else
					snprintf(out, n, "%s", mode);
			}

			DesignState *mSt;
			int32 mOnglet = 0;
			bool mDeplierUneFois = true; ///< (hérité — plus de CollapsingHeader au costume)
			nkgui::NkGuiId mDragChamp = 0;	   ///< champ numérique en cours de glisser
			float32 mDragDernierX = 0.f;	   ///< dernière abscisse du glisser
			/// Le tampon d'édition du contenu texte (section Typographie) et le
			/// nœud qu'il reflète — recopié à chaque changement de sélection.
			int32 mTexteNode = -1;
			char mTexteBuf[128] = {};
			/// Les tampons hexa de la section APPARENCE (fond / couleur de
			/// texte / couleur de bord), synchronisés sur la sélection — même
			/// patron que mTexteBuf.
			int32 mApparNode = -1;
			char mFondBuf[12] = {};
			/// ── LES TAMPONS DE LA SECTION REMPLISSAGES ───────────────────────
			/// ⚠️ HUIT LIGNES AU PLUS, ET C'EST UNE BORNE DE L'INSPECTEUR, PAS DU
			///    MODELE. `NkUINode::fills` n'a aucune limite ; c'est ce tableau
			///    de tampons de saisie qui en a une. Le « + » le DIT quand il
			///    refuse -- il ne se contente pas de ne rien faire.
			static constexpr uint32 kMaxFillsUI = 8;
			char mFillsBuf[kMaxFillsUI][12] = {};
			int32 mFillsNode = -1;
			uint32 mFillsGen = 0;
			char mBordsBuf[kMaxFillsUI][12] = {};
			int32 mBordsNode = -1;
			uint32 mBordsGen = 0;
			char mEffetsBuf[kMaxFillsUI][12] = {};
			int32 mEffetsNode = -1;
			uint32 mEffetsGen = 0;
			char mTexteColBuf[12] = {};
			char mBordColBuf[12] = {};
			/// Les generations d'historique vues par les tampons (une
			/// annulation recharge sans changer la selection).
			uint32 mTexteGen = 0;
			uint32 mApparGen = 0;
	};

} // namespace nkuidesign
