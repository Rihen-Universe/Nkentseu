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
#include "NKEditorKit/NkEditorInspectorFrame.h" // LA charpente d inspecteur (kit)
#include "NKEditorKit/NkTheme.h"
#include "NKFileSystem/NkFile.h"
// ⚠️ LES PLAFONDS DU KIT DOIVENT CRIER (kMaxComponents = 64, kMaxDepth = 64) :
//    un panneau qui les franchit JOURNALISE. Sans NKLogger ici, il ne pourrait
//    que se taire ou refuser — et se taire est exactement ce qui est interdit.
#include "NKLogger/NkLog.h"
// ⚠️ LES PLAFONDS DU KIT DOIVENT CRIER (kMaxComponents = 64, kMaxDepth = 64) :
//    un panneau qui les franchit JOURNALISE. Sans NKLogger ici, il ne pourrait
//    que se taire ou refuser — et se taire est exactement ce qui est interdit.
#include "NKLogger/NkLog.h"

#include "Canvas.h"
#include "Selection.h"
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

				if (!LoadDoc())
					BuildStarterDocument();

				AuditDeclaredRoles();
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
				NkString out;
				doc.Save(out);
				status = nkentseu::NkFile::WriteAllText(kDocumentPath, out.Data())
							 ? NkString("Document enregistré : ")
							 : NkString("ÉCHEC d'écriture : ");
				status.Append(kDocumentPath);
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
				SelectSingle(0);
				host.demoModels.Clear();
				host.SyncTo(doc);
				char b[192];
				snprintf(b, sizeof(b), "Document chargé : %u nœud(s), %u composant(s) inconnu(s)",
						 doc.NodeCount(), unknown);
				status = NkString(b);
				return true;
			}
	};

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
				NkVector<int32> remap;
				if (!mSt->doc.RemoveSubtree(mSt->selected, &remap)) {
					mSt->status = NkString("La racine ne se supprime pas.");
					return;
				}
				// ⚠️ LA SUPPRESSION RENUMEROTE : la selection courante est perimee et
				//    doit etre reparee ICI, sinon elle designerait un autre noeud — un
				//    defaut qui ne se voit qu'a la modification suivante, donc loin de
				//    sa cause.
				mSt->selected = 0;
				mSt->host.demoModels.Clear();
				mSt->host.SyncTo(mSt->doc);
				mSt->status = NkString("Noeud supprime.");
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
		public:
			explicit PreviewPanel(DesignState *st)
				: NkEditorPanel("Aperçu", NkEditorDockSide::NK_CENTER), mSt(st) {}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				designkit::releve::Zone(ctx, "apercu");
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
				const NkRect area = ctx.NextItemRect(-1.f, hToile > 120.f ? hToile : 120.f);
				if (area.w <= 0.f || area.h <= 0.f)
					return;

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
					mSt->view.panX = kOutilsMarge * 2.f + kOutilsLargeur;
					mSt->view.panY = 12.f;
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

				if (!mAideInitiale) {
					// L'outil arme se DIT des la premiere image — une application
					// qui ne dit pas quel outil est actif fait deviner (mesure du
					// 30/08).
					mAideInitiale = true;
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
				if (ctx.inputId == NKGUI_ID_NONE) {
					for (int32 k = 0; k < ctx.input.charCount; ++k) {
						switch (ctx.input.chars[k]) {
							case 'v': case 'V': ArmerOutil(0); break;
							case 'f': case 'F': ArmerOutil(1); break;
							case 'r': case 'R': mVariante = 0; ArmerOutil(2); break;
							case 'o': case 'O': mVariante = 1; ArmerOutil(2); break;
							case 'l': case 'L': mVariante = 2; ArmerOutil(2); break;
							case 'p': case 'P': ArmerOutil(3); break;
							case 't': case 'T': ArmerOutil(4); break;
							default: break;
						}
					}
				}
				// ÉCHAP annule le tracé en cours (Lunacy), et le DIT.
				if (mCreating && ctx.input.KeyPressed(NkGuiKey::Escape)) {
					mCreating = false;
					Dire("Tracé annulé.", "", "");
				}
				TraceEntree(ctx);

				HandleMouse(in, screen);

				// ⚠️ `NkDesignPaint`, PAS `NkGuiComponentPaint` : c'est lui qui
				//    traduit les poignees de CETTE application en dessins. Le
				//    peintre du kit peint un carre pour toute poignee non nulle —
				//    correct pour lui (il ne connait l'enumeration de personne),
				//    insuffisant pour un chevron, qui doit dire « ouvert » ou
				//    « ferme ». Il surcharge `Icon` et RIEN d'autre.
				NkDesignPaint paint(ctx, mSt->theme);

				// ── LA TOILE DE LA PLANCHE 22.0 : fond + grille POINTILLEE ────
				// Decor d'EDITEUR, pas de document : `RenderDocument` (les essais
				// 41) n'emet aucune de ces commandes, et le fichier enregistre n'en
				// sait rien. Le pas est en espace DOCUMENT : la grille zoome avec
				// le contenu, comme sur la planche. En dessous de 6 px projetes,
				// elle se tait — des points serres deviennent du bruit.
				{
					// Banani V2 : la toile est CLAIRE (canvas_bg #f5f7fb) meme en
					// editeur sombre — le theme du DOCUMENT n'est pas celui de
					// l'EDITEUR — et les points sont canvas_dot (#d4dce8, pas 20).
					paint.Fill({area.x, area.y, area.w, area.h},
							   NkDesignResolveRole("canvas_bg"), 0.f);
					const float32 pasEcran = kGrillePas * mSt->view.zoom;
					if (pasEcran >= 6.f) {
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

				NkDrawDocument(paint, in, mSt->doc, screen, mSt->host);

				// ── LE TRACE ELASTIQUE d'un outil F/R en cours ────────────────
				// Peint APRES le document (il flotte au-dessus), jamais enregistre.
				if (mCreating) {
					const NkPaintRect t = RectTrace(in.mouseX, in.mouseY, in.shift);
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
				DessinerFlottants(ctx, area);

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
				if (screen.Has(mSt->selected) && mSt->doc.IsValidIndex(mSt->selected)
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
					const float32 hp = 7.f;
					const uint16 fondP = NkDesignResolveRole("window_bg");
					const float32 xs[3] = {rs.x - hp * 0.5f, rs.x + rs.w * 0.5f - hp * 0.5f,
										   rs.x + rs.w - hp * 0.5f};
					const float32 ys[3] = {rs.y - hp * 0.5f, rs.y + rs.h * 0.5f - hp * 0.5f,
										   rs.y + rs.h - hp * 0.5f};
					for (uint32 gy = 0; gy < 3; ++gy)
						for (uint32 gx = 0; gx < 3; ++gx) {
							if (gx == 1 && gy == 1)
								continue; // pas de poignee au centre
							const NkPaintRect ph{xs[gx], ys[gy], hp, hp};
							paint.Fill(ph, fondP, 0.f);
							paint.OutlineSharp(ph, accent);
						}
					// LA PUCE DE TAILLE (Lunacy : « 932 × 552 » sous l'élément,
					// fond accent) — les dimensions DOCUMENT, celles que le
					// fichier écrira, jamais des pixels d'écran.
					if (mSt->layout.Has(mSt->selected)) {
						const NkPaintRect rd = mSt->layout.At(mSt->selected);
						PuceTaille(paint, rs, rd.w, rd.h);
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

				// ── UN CLIC HORS DE LA TOILE N'APPARTIENT PAS A LA TOILE ─────────
				// ⚠️ MESURE PAR LE PILOTE DU 30/08, et le defaut etait a l'ECRAN
				//    depuis le debut : cliquer le champ « Contenu » de l'INSPECTEUR
				//    passait aussi par ici, tombait « dans le vide » du pointage,
				//    et VIDAIT LA SELECTION — le champ disparaissait sous le clic
				//    venu l'editer. Un clic n'est un geste de toile que s'il NAIT
				//    dans la toile (un glisser en cours, lui, continue : sortir du
				//    cadre en tirant un bord ne doit pas lacher le bord).
				const NkPaintRect &vp = mSt->view.viewport;
				if (in.mousePressed
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
				if (in.mousePressed
					&& (DansZone(mZoneModes, in) || DansZone(mZoneOutils, in)
						|| DansZone(mZoneCluster, in) || DansZone(mZoneEventail, in)))
					return;

				// ── LES OUTILS QUI CREENT : F (cadre), R (rectangle), T (texte) ──
				const bool outilTrace = (mOutil == 1 || mOutil == 2);
				const bool outilTexte = (mOutil == 4);
				if (in.mousePressed && (outilTrace || outilTexte)) {
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
					// VARIANTE armee (Lunacy : R rectangle, O ellipse, L ligne).
					// Un trace minuscule (clic sans glisser) prend une taille de
					// depart : une forme de 0 px paraitrait perdue.
					const NkPaintRect t = RectTrace(in.mouseX, in.mouseY, in.shift);
					float32 w = t.w, h = t.h;
					const bool ligne = (mOutil == 2 && mVariante == 2);
					if (!ligne && w < 8.f)
						w = mOutil == 1 ? 200.f : 120.f;
					if (!ligne && h < 8.f)
						h = mOutil == 1 ? 160.f : 80.f;
					if (ligne && w < 8.f && h < 8.f)
						w = 120.f;
					const char *forme = "frame";
					if (mOutil == 2) {
						if (mVariante == 1)
							forme = "ellipse";
						else if (mVariante == 2)
							// La diagonale MONTE si les axes du geste divergent.
							forme = ((mCreateX <= in.mouseX) != (mCreateY <= in.mouseY))
										? "line_up"
										: "line";
						else
							forme = "rect";
					}
					CreerForme(mCreateParent, screen, t.x, t.y, mSt->view.ToDocLength(w),
							   mSt->view.ToDocLength(h), forme);
					mCreating = false;
				}
				if (in.mousePressed) {
					// ⚠️ `NkPickSelectable`, PAS `NkPickNode` : la racine couvre toute
					//    la surface et repondrait a tous les clics. Cliquer le fond
					//    DESELECTIONNE, comme dans tout outil de dessin.
					const int32 hit = NkPickSelectable(mSt->doc, screen, in.mouseX, in.mouseY);
					if (hit < 0) {
						// Le vide : on vide, et on arme le rectangle de selection.
						if (!in.ctrl)
							mSt->SelectClear();
						mMarquee = true;
						mMarqX = in.mouseX;
						mMarqY = in.mouseY;
					}
					if (hit >= 0) {
						// `Ctrl`+clic ajoute ou retire (doc 3 §11.5).
						if (in.ctrl)
							mSt->SelectToggle(hit);
						else if (!mSt->sel.Contains(hit))
							mSt->SelectSingle(hit);
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
						}
					}
				}
				if (mMoving && in.mouseDown) {
					const float32 dx = mSt->view.ToDocLength(in.mouseX - mLastX);
					const float32 dy = mSt->view.ToDocLength(in.mouseY - mLastY);
					if ((dx != 0.f || dy != 0.f) && mSt->doc.IsValidIndex(mMoveNode)) {
						NkUINode &n = mSt->doc.nodes[(uint32)mMoveNode];
						n.posX += dx;
						n.posY += dy;
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
				}
				mLastX = in.mouseX;
				mLastY = in.mouseY;
			}

			/// LA CREATION D'UN NOEUD PAR UN OUTIL. `x`/`y` sont en pixels ECRAN
			/// (le point de depart du trace), `w`/`h` deja en espace DOCUMENT. La
			/// position ecrite est RELATIVE AU PARENT — `posX` d'une forme dans un
			/// artboard est mesuree depuis l'artboard, pas depuis la toile, sinon
			/// deplacer l'artboard laisserait ses formes derriere.
			void CreerForme(int32 parent, const NkLayoutResult &screen, float32 x, float32 y,
							float32 w, float32 h, const char *shape) {
				if (!mSt->doc.IsValidIndex(parent))
					return;
				const int32 idx = mSt->doc.AddChild(parent, "", NkAuthor::Humain);
				if (!mSt->doc.IsValidIndex(idx))
					return;
				// Le nom : nature + numero d'ordre parmi les memes natures. « Cadre
				// 2 » se cherche dans la Hierarchie ; « noeud 17 » non.
				uint32 memes = 0;
				for (uint32 i = 0; i < (uint32)mSt->doc.nodes.Size(); ++i)
					if (StrEq(mSt->doc.nodes[i].shape.Data(), shape))
						++memes;
				const char *base = StrEq(shape, "frame")	 ? "Cadre"
								   : StrEq(shape, "rect")	 ? "Rectangle"
								   : StrEq(shape, "ellipse") ? "Ellipse"
								   : StrEq(shape, "line")	 ? "Ligne"
								   : StrEq(shape, "line_up") ? "Ligne"
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
			}

			static bool DansZone(const NkRect &z, const NkComponentInput &in) {
				return z.w > 0.f && in.mouseX >= z.x && in.mouseX < z.x + z.w && in.mouseY >= z.y
					   && in.mouseY < z.y + z.h;
			}

			DesignState *mSt;
			bool mDragging = false;
			bool mDragHorizontal = true;
			int32 mDragNode = -1;
			uint8 mResizeEdges = 0; ///< bits 1=G 2=D 4=H 8=B (noeud pose, huit poignees)
			bool mMoving = false;
			int32 mMoveNode = -1;
			bool mCreating = false;
			int32 mCreateParent = -1;
			float32 mCreateX = 0.f, mCreateY = 0.f;
			float32 mLastX = 0.f, mLastY = 0.f;
			/// La variante armée de la famille Formes : 0 rectangle, 1 ellipse,
			/// 2 ligne (Lunacy : R, O, L). Elle est aussi la FACE du bouton.
			uint32 mVariante = 0;
			bool mEventailOuvert = false;
			bool mAideInitiale = false;
			NkRect mZoneEventail = {0.f, 0.f, 0.f, 0.f};
			/// Les zones flottantes de l'image PRECEDENTE (bascule, outils,
			/// cluster) — posees par `DessinerFlottants`, lues par `HandleMouse`
			/// pour que la toile ne recoive pas leurs clics.
			NkRect mZoneModes = {0.f, 0.f, 0.f, 0.f};
			NkRect mZoneOutils = {0.f, 0.f, 0.f, 0.f};
			NkRect mZoneCluster = {0.f, 0.f, 0.f, 0.f};

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
					static const char *const kModes[4] = {"Design", "Behavior", "Animation",
														  "Split"};
					const float32 lw = 92.f, h = 26.f;
					const float32 w = lw * 4.f + 8.f;
					const NkRect r = {zone.x + (zone.w - w) * 0.5f, zone.y + 10.f, w, h};
					mZoneModes = r; // HandleMouse ne doit pas voir ses clics
					dl.AddRectFilled(r, fond, 6.f);
					dl.AddRect(r, bord, 1.f, 6.f);
					for (uint32 i = 0; i < 4; ++i) {
						const NkRect c = {r.x + 4.f + lw * (float32)i, r.y + 3.f, lw, h - 6.f};
						if (i == mMode)
							dl.AddRectFilled(c, ctx.theme.accent, 4.f);
						ctx.SetNextItemRect(c);
						if (Button(ctx, kModes[i])) {
							mMode = i;
							Dire("Mode ", kModes[i], " : seul Design est branché.");
						}
					}
				}

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
					static const char *const kOutilsIds[7] = {
						"##outil_selection", "##outil_cadre", "##outil_formes",
						"##outil_vectoriel", "##outil_texte", "##outil_media",
						"##outil_mesure"};
					static const char *const kOutilsBulles[7] = {
						"Sélection (V)", "Cadre (F)", "Formes (R · O · L)", "Vectoriel (P)",
						"Texte (T)",	 "Média",	  "Mesure"};
					// Geometrie BANANI (FloatingToolRail) : rail 36 px, rayon 5,
					// boutons 36x28, FILET entre la plume et le texte.
					const float32 w = kOutilsLargeur, hb = 30.f;
					const float32 filet = 8.f;
					const float32 h = hb * 7.f + filet + 6.f;
					const NkRect r = {zone.x + kOutilsMarge, zone.y + (zone.h - h) * 0.5f, w, h};
					mZoneOutils = r; // idem
					dl.AddRectFilled(r, fond, 5.f);
					dl.AddRect(r, bord, 1.f, 5.f);
					for (uint32 i = 0; i < 7; ++i) {
						const float32 yOff = (i >= 4) ? filet : 0.f;
						if (i == 4)
							dl.AddLine({r.x + 6.f, r.y + 4.f + hb * 4.f + filet * 0.5f},
									   {r.x + w - 6.f, r.y + 4.f + hb * 4.f + filet * 0.5f}, bord,
									   1.f);
						const NkRect c = {r.x, r.y + 4.f + hb * (float32)i + yOff, w, hb - 2.f};
						ctx.SetNextItemRect(c);
						if (Button(ctx, kOutilsIds[i]))
							ArmerOutil(i);
						// Le CHEVRON de la famille Formes est VIVANT (Lunacy) : le
						// cliquer ouvre l'éventail des variantes (§7.1) — le clic
						// arme aussi la famille, comme chez Lunacy.
						if (i == 2 && ctx.input.mouseClicked[0]) {
							const NkRect zc = {c.x + c.w - 14.f, c.y + c.h - 14.f, 14.f, 14.f};
							if (NkGuiRectContains(zc, ctx.input.mousePos))
								mEventailOuvert = !mEventailOuvert;
						}
						if (ctx.IsItemHovered())
							SetTooltip(ctx, kOutilsBulles[i]);
						// ⚠️ L'ACCENT DE L'OUTIL ACTIF SE PEINT APRES LE BOUTON :
						//    `Button` pose TOUJOURS un fond opaque (mesure :
						//    NkGuiWidgets.cpp, `theme.button` au repos) — peint
						//    avant, l'accent disparaissait dessous. Le glyphe vient
						//    encore au-dessus : blanc sur accent, comme le curseur
						//    bleu de la planche.
						if (i == mOutil)
							dl.AddRectFilled(c, ctx.theme.accent, 4.f);
						GlypheOutil(dl, c, i, ctx.theme.text, ctx.theme.textMuted);
					}

					// ── L'ÉVENTAIL DES VARIANTES DE FORMES (§7.1, Lunacy) ────
					// Déplié « vers le canvas », À DROITE du bouton Formes :
					// rectangle, ellipse, ligne. Choisir rend la variante active
					// ET en fait la face du bouton (GlypheOutil la dessine).
					if (mEventailOuvert) {
						const NkRect bFormes = {r.x, r.y + 4.f + hb * 2.f, w, hb - 2.f};
						const float32 vb = 36.f;
						const NkRect ev = {r.x + w + 6.f, bFormes.y, vb * 3.f + 16.f, vb + 8.f};
						mZoneEventail = ev;
						dl.AddRectFilled(ev, fond, 6.f);
						dl.AddRect(ev, bord, 1.f, 6.f);
						static const char *const kVarIds[3] = {"##var_rect", "##var_ellipse",
															   "##var_ligne"};
						static const char *const kVarBulles[3] = {"Rectangle (R)", "Ellipse (O)",
																  "Ligne (L)"};
						for (uint32 v = 0; v < 3; ++v) {
							const NkRect cv = {ev.x + 4.f + (vb + 4.f) * (float32)v, ev.y + 4.f,
											   vb, vb};
							ctx.SetNextItemRect(cv);
							if (Button(ctx, kVarIds[v])) {
								mVariante = v;
								ArmerOutil(2);
								mEventailOuvert = false;
							}
							if (ctx.IsItemHovered())
								SetTooltip(ctx, kVarBulles[v]);
							if (v == mVariante)
								dl.AddRectFilled(cv, ctx.theme.accent, 4.f);
							GlypheForme(dl, cv, v, ctx.theme.text);
						}
						// Un clic hors de l'éventail et hors du bouton Formes le
						// referme — le comportement de tout menu volant.
						if (ctx.input.mouseClicked[0]
							&& !NkGuiRectContains(ev, ctx.input.mousePos)
							&& !NkGuiRectContains(bFormes, ctx.input.mousePos))
							mEventailOuvert = false;
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
					snprintf(zoom, sizeof(zoom), "%d %%",
							 (int32)(mSt->view.zoom * 100.f + 0.5f));
					const float32 h = 28.f, w = 168.f;
					const NkRect r = {zone.x + zone.w - w - 14.f, zone.y + zone.h - h - 14.f, w,
									  h};
					mZoneCluster = r; // idem
					dl.AddRectFilled(r, fond, 6.f);
					dl.AddRect(r, bord, 1.f, 6.f);
					const NkRect rz = {r.x + 4.f, r.y + 3.f, 92.f, h - 6.f};
					ctx.SetNextItemRect(rz);
					if (Button(ctx, zoom))
						Dire("Zoom : la saisie directe n'est pas encore branchée.", "", "");
					// Le chevron « déroulant », tracé : deux segments, aucune police.
					{
						const float32 cxz = rz.x + rz.w - 12.f, cyz = rz.y + rz.h * 0.5f;
						const float32 s2 = 3.5f;
						dl.AddLine({cxz - s2, cyz - 1.5f}, {cxz, cyz + 2.f}, ctx.theme.text, 1.4f);
						dl.AddLine({cxz, cyz + 2.f}, {cxz + s2, cyz - 1.5f}, ctx.theme.text, 1.4f);
					}
					ctx.SetNextItemRect({r.x + 100.f, r.y + 3.f, 30.f, h - 6.f});
					if (Button(ctx, "#"))
						Dire("Grille : à brancher.", "", "");
					ctx.SetNextItemRect({r.x + 132.f, r.y + 3.f, 30.f, h - 6.f});
					if (Button(ctx, "|-|"))
						Dire("Magnétisme : à brancher.", "", "");
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
			/// éventail) : 0 rectangle, 1 ellipse, 2 ligne.
			static void GlypheForme(nkgui::NkGuiDrawList &dl, const NkRect &c, uint32 v,
									const nkgui::NkColor &enc) {
				const float32 cx = c.x + c.w * 0.5f, cy = c.y + c.h * 0.5f;
				const float32 s = 7.f;
				if (v == 1)
					dl.AddCircle({cx, cy}, s * 0.85f, enc, 1.6f);
				else if (v == 2)
					dl.AddLine({cx - s, cy + s * 0.7f}, {cx + s, cy - s * 0.7f}, enc, 2.f);
				else
					dl.AddRect({cx - s, cy - s * 0.72f, s * 2.f, s * 1.44f}, enc, 1.6f, 3.f);
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
						return "Sélection — cliquer ; glisser = déplacer ; bords = "
							   "redimensionner";
					case 1:
						return "Cadre — cliquez-glissez pour tracer ; Maj = carré ; "
							   "Échap = annuler";
					case 2:
						return mVariante == 1
								   ? "Ellipse — cliquez-glissez ; Maj = cercle ; Échap = annuler"
							   : mVariante == 2
								   ? "Ligne — cliquez-glissez ; Maj = contraint ; Échap = annuler"
								   : "Rectangle — cliquez-glissez ; Maj = carré ; Échap = annuler";
					case 3:
						return "Vectoriel : à brancher (§7.2)";
					case 4:
						return "Texte — cliquez pour poser ; contenu dans l'Inspecteur "
							   "(Typographie)";
					case 5:
						return "Média : à brancher (§7.2)";
					default:
						return "Mesure : à brancher (§7.2)";
				}
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
			NkPaintRect RectTrace(float32 mx, float32 my, bool shift) const {
				NkPaintRect t = NkRectFromPoints(mCreateX, mCreateY, mx, my);
				if (!shift)
					return t;
				const bool ligne = (mOutil == 2 && mVariante == 2);
				if (ligne) {
					if (t.w > t.h * 2.f)
						t.h = 1.f;
					else if (t.h > t.w * 2.f)
						t.w = 1.f;
					else
						t.w = t.h = (t.w > t.h ? t.w : t.h);
					return t;
				}
				const float32 m = t.w > t.h ? t.w : t.h;
				// Le carré s'ancre au point de DÉPART du geste, pas au coin
				// normalisé — sinon contraindre déplace la forme sous la main.
				t.x = (mx >= mCreateX) ? mCreateX : mCreateX - m;
				t.y = (my >= mCreateY) ? mCreateY : mCreateY - m;
				t.w = t.h = m;
				return t;
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
			static constexpr float32 kOutilsMarge = 12.f;
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

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				char b[256];
				snprintf(b, sizeof(b), "Backend : %s%s",
						 mSt->ai.Backend() ? mSt->ai.Backend()->Name() : "-",
						 (mSt->ai.Backend() && mSt->ai.Backend()->IsAvailable()) ? "" : " (indisponible)");
				ec.Text(b);
				ec.Text("Décrivez l'interface voulue. Elle produit une DÉCLARATION,");
				ec.Text("posée dans l'arbre comme si vous l'aviez posée vous-même.");
				InputText(ctx, "Demande", mSt->promptBuf, (int32)sizeof(mSt->promptBuf));

				if (ec.Button("Demander à l'IA"))
					Ask();
				if (ec.Button("Vérifier le document par rejeu"))
					Replay();

				ec.Separator();
				ec.Text(mLast.Data() ? mLast.Data() : "");
				ec.Separator();
				// ⚠️ CE QUI N'EST PAS LA, DIT DANS L'INTERFACE ELLE-MEME. Un editeur
				//    muet sur ce qu'il ne fait pas se fait reprocher des absences qu'il
				//    n'a jamais promises.
				ec.Text("Aucun modèle spécialisé n'existe encore : il s'entraînera sur");
				ec.Text("les documents produits ici. Backend réseau : absent (le client");
				ec.Text("HTTP doit monter dans un module partagé, pas être recopié ici).");
			}

		private:
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
				// La planche montre les filets d'indentation ; ils sont à 0 par
				// défaut dans la déclaration.
				mInstPages.SetParam("indent_guides", 1.f);
			}

			void OnUI(NkEditorFrameContext &ec) override {
				auto &ctx = ec.Ui();
				designkit::releve::Zone(ctx, "hierarchie");

				SyncPages();
				SyncComposants();

				// ── LA LOUPE, UNE SEULE POUR LES DEUX SECTIONS ───────────────
				InputText(ctx, "Filtrer", mFiltre, (int32)sizeof(mFiltre));
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
				float32 restant = basY - ctx.layout.cursor.y;
				if (restant < 80.f)
					restant = 80.f;
				// §11.6 : la section basse est SECONDAIRE ; elle prend le tiers,
				// borné, pour qu'un arbre profond garde de la place.
				float32 hBas = restant * 0.32f;
				if (hBas > 220.f)
					hBas = 220.f;
				if (hBas < 90.f)
					hBas = 90.f;
				const float32 hHaut = restant - hBas - 2.f * kBandeH - 8.f;

				BandeDeSection(ctx, "PAGES", "hier.pages.plus");
				DessinerArbre(ctx, mModelePages, mInstPages, hHaut > 60.f ? hHaut : 60.f, "pages");

				BandeDeSection(ctx, "COMPOSANTS", "hier.composants.plus");
				DessinerArbre(ctx, mModeleComposants, mInstComposants, hBas, "composants");
			}

		private:
			static constexpr float32 kBandeH = 22.f;

			/// La bande de titre d'une section, avec son `[+]` (planche 091913).
			/// ⚠️ Assemblage de primitives NKGui, pas un widget de plus : un titre
			///    et un bouton posés à des rectangles explicites.
			void BandeDeSection(NkGuiContext &ctx, const char *titre, const char *id) {
				const NkRect r = ctx.NextItemRect(-1.f, kBandeH);
				ctx.SetNextItemRect({r.x, r.y, r.w - kBandeH - 4.f, r.h});
				Text(ctx, titre);
				ctx.SetNextItemRect({r.x + r.w - kBandeH, r.y, kBandeH, r.h});
				if (Button(ctx, "+"))
					mSt->status = NkString("[+] de la section : à brancher (le geste "
										   "de création n'existe pas encore).");
				(void)id;
			}

			void DessinerArbre(NkGuiContext &ctx, NkTreeViewModel &modele,
							   NkComponentInstance &inst, float32 hauteur, const char *cle) {
				const NkRect zone = ctx.NextItemRect(-1.f, hauteur);
				if (zone.w <= 0.f || zone.h <= 0.f)
					return;
				designkit::releve::Rect(ctx, cle, zone);

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
				const NkPaintRect r = {zone.x, zone.y, zone.w, zone.h};
				const NkTreeViewResult res = nkentseu::editorkit::NkDrawTreeView(
					paint, in, r, modele, Style(inst), NkTreeViewHooks{});

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
					const int32 i = modele.IndexOf(modele.active);
					if (i >= 0)
						mSt->SelectSingle(i);
					else
						mSt->SelectClear();
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
				for (uint32 i = 0; i < n; ++i) {
					const NkUINode &d = mSt->doc.nodes[i];
					NkTreeNode t;
					t.id = (nkentseu::nk_uint64)(i + 1);
					t.parent = d.parent;
					t.label = d.label.Empty() ? NkString(d.component.Empty() ? "(cadre)"
																			: d.component.Data())
											  : d.label;
					t.path = t.label;
					t.kindLabel = d.component.Empty() ? "cadre" : d.component.Data();
					// La pastille de rôle de la planche : la NATURE du nœud.
					t.kindRole = NkDesignResolveRole(d.component.Empty() ? "text_muted" : "accent_ui");
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
				// La sélection est UNE (§11.5) : elle vit dans `DesignState`.
				mModelePages.active =
					mSt->selected >= 0 ? (nkentseu::nk_uint64)(mSt->selected + 1) : 0;
				mModelePages.chosen.Clear();
				if (mModelePages.active)
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
					t.kindLabel = "composant";
					t.kindRole = NkDesignResolveRole("accent_sel");
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
				if (mDeplierUneFois) {
					mDeplierUneFois = false;
					static const char *const kTitres[7] = {"POSITION", "TAILLE", "ANCRAGE",
														   "ALIGNEMENT", "APPARENCE", "BORDS",
														   "TYPOGRAPHIE"};
					for (int32 i = 0; i < 7; ++i)
						ctx.SetNodeOpen(ctx.GetId(kTitres[i]), true);
				}
				editorkit::NkInspectorCharpente ch;
				ch.user = this;
				ch.entete = Nom();
				ch.enteteVide = "Aucune sélection";
				ch.onglets = Onglets();
				ch.ongletCount = 3;
				ch.idOnglets = "insp.onglets";
				ch.sectionsDe = &SectionsDe;
				ch.messageOngletVide = &MessageOngletVide;
				mOnglet = editorkit::NkInspectorDessiner(ctx, ch);
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
				(void)user;
				// ⚠️ UN ONGLET SANS CONTENU REND ZÉRO SECTION, il ne rend pas
				//    sept sections vides. C'est la charpente qui dira quoi
				//    afficher à la place (§12.2) — voir `MessageOngletVide`.
				if (onglet != 0) {
					count = 0;
					return nullptr;
				}
				// ⚠️ L'ECART AVEC LE PLAN EST CONNU ET NOMME. Le §12.2 du
				//    document 3 (l. 1682) enumere DIX sections aux noms voisins
				//    (Cible, Disposition, Espacement, Effets, Points de rupture,
				//    Layout parent...). Cette table de SEPT est celle que la
				//    consigne du 2026-08-29 fixe ; la convergence vers le plan est
				//    un chantier a part, pas un detail a glisser ici en douce.
				static const editorkit::NkInspectorSection kSections[] = {
					{"POSITION", &CorpsPositionC, true},
					{"TAILLE", &CorpsTailleC, true},
					{"ANCRAGE", &CorpsAncrageC, true},
					{"ALIGNEMENT", &CorpsAlignementC, true},
					{"APPARENCE", &CorpsApparenceC, true},
					{"BORDS", &CorpsBordsC, true},
					{"TYPOGRAPHIE", &CorpsTypographieC, true},
				};
				count = (int32)(sizeof(kSections) / sizeof(kSections[0]));
				return kSections;
			}

			static const char *MessageOngletVide(void *, int32 onglet) noexcept {
				return onglet == 1 ? "Onglet Widget : le rôle et ses paramètres — pas encore branché."
								   : "Onglet Behavior : événements et callbacks — pas encore branché.";
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
			static void CorpsBordsC(void *, NkGuiContext &ctx) {
				// ⚠️ LE MODELE NE PORTE AUCUNE PROPRIETE DE BORD PAR NOEUD --
				//    mesure : `NkUINode` n'a ni rayon, ni epaisseur, ni couleur de
				//    bord. La section le DIT au lieu de disparaitre (consigne du
				//    29/08 : meme regle que les menus grises) : une section
				//    absente ferait croire que la question ne se pose pas, alors
				//    qu'elle attend son modele.
				ctx.BeginDisabled();
				nkgui::TextWrapped(ctx, "Le modèle du nœud ne porte pas encore de bords.");
				nkgui::TextWrapped(ctx, "(rayon, épaisseur : à venir avec le vocabulaire)");
				ctx.EndDisabled();
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
				const NkUINode *n = NoeudCourant();
				if (!n || !StrEq(n->shape.Data(), "text")) {
					ctx.BeginDisabled();
					nkgui::TextWrapped(ctx, "L'élément sélectionné ne porte pas de texte.");
					ctx.EndDisabled();
					return;
				}
				// Le tampon SUIT LA SELECTION : en changer recharge le contenu —
				// sans ce garde, editer un texte ecrirait dans celui d'avant.
				if (mTexteNode != mSt->selected) {
					mTexteNode = mSt->selected;
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
			void CorpsPosition(NkGuiContext &ctx) {
				const NkUINode *n = NoeudCourant();
				if (!n) {
					designkit::KeyValue(ctx, "X", "-");
					designkit::KeyValue(ctx, "Y", "-");
					return;
				}
				char b[64];
				const bool a = mSt->layout.Has(mSt->selected);
				const NkPaintRect r = a ? mSt->layout.At(mSt->selected) : NkPaintRect{0.f, 0.f, 0.f, 0.f};
				snprintf(b, sizeof(b), a ? "%.0f" : "-", (double)r.x);
				designkit::KeyValue(ctx, "X", b);
				snprintf(b, sizeof(b), a ? "%.0f" : "-", (double)r.y);
				designkit::KeyValue(ctx, "Y", b);
				// ⚠️ SOUS UN PARENT `Free`, LA POSITION EST UNE DONNEE POSEE — et
				//    depuis le 30/08 (référence Lunacy de Rodolf : X/Y sont des
				//    champs), elle S'ÉDITE ICI. Ce n'est pas contre la règle « la
				//    position est un résultat » : la règle vaut pour les nœuds
				//    PILOTÉS PAR UN AGENCEMENT — eux restent en lecture seule avec
				//    leur explication. `posX`/`posY` sous Free est ce que le
				//    fichier porte déjà (étape 38) : l'éditer au champ est le même
				//    geste que le glisser de la toile, au clavier.
				if (ParentKind() == editorkit::NkLayoutKind::Free) {
					NkUINode *m = NoeudMutable();
					if (m) {
						bool bouge = false;
						bouge |= nkgui::DragFloat(ctx, "X posée", m->posX, 1.f, -100000.f,
												  100000.f);
						bouge |= nkgui::DragFloat(ctx, "Y posée", m->posY, 1.f, -100000.f,
												  100000.f);
						if (bouge)
							mSt->doc.MarkHumanEdit(mSt->selected);
					}
				} else {
					// ⚠️ PHRASE PLEINE LARGEUR, PAS UNE CELLULE : en colonne
					//    « valeur » elle sortait « calculée — jamais écrite da... »
					//    sur la capture du 29/08. Un texte d explication coupé
					//    n explique plus.
					nkgui::TextWrapped(ctx, "calculée — jamais écrite dans le document");
				}
			}

			void CorpsTaille(NkGuiContext &ctx) {
				NkUINode *n = NoeudMutable();
				if (!n) {
					designkit::KeyValue(ctx, "Largeur", "-");
					designkit::KeyValue(ctx, "Hauteur", "-");
					return;
				}
				LigneTaille(ctx, "Largeur", n->width);
				LigneTaille(ctx, "Hauteur", n->height);
			}

			/// Une ligne de taille : le MODE se lit, la VALEUR s'edite -- mais
			/// seulement quand le mode en porte une.
			/// ⚠️ TROIS CAS, ET CHACUN A SA RAISON :
			///    - `Fixed`/`Fraction`/`Weight` portent un nombre -> DragFloat ;
			///    - `Content`/`Expand` n'en portent pas -> la ligne se GRISE au
			///      lieu de disparaitre (§12.2 : l'oeil garde sa place) ;
			///    - une METRIQUE nommee PRIME sur la valeur (`valueMetric`) -> on
			///      grise aussi, et on NOMME la metrique : editer le nombre alors
			///      qu'un nom le remplace ecrirait une valeur que le resolveur
			///      ignore -- un reglage qui ment.
			void LigneTaille(NkGuiContext &ctx, const char *titre, NkSizeDecl &d) {
				const bool porteValeur = d.mode == NkSizeMode::Fixed ||
										 d.mode == NkSizeMode::Fraction || d.mode == NkSizeMode::Weight;
				const bool metrique = d.valueMetric && *d.valueMetric;
				designkit::KeyValue(ctx, titre, NkSizeModeName(d.mode));
				if (metrique) {
					ctx.BeginDisabled();
					char b[96];
					snprintf(b, sizeof(b), "métrique « %s » — prime sur le nombre", d.valueMetric);
					designkit::KeyValue(ctx, "  valeur", b);
					ctx.EndDisabled();
					return;
				}
				if (!porteValeur) {
					ctx.BeginDisabled();
					designkit::KeyValue(ctx, "  valeur", "(le mode n'en porte pas)");
					ctx.EndDisabled();
					LignesBornes(ctx, titre, d);
					return;
				}
				// La vitesse suit l'unite : 1 px par cran en Fixed, 0.01 pour une
				// fraction 0..1, 0.05 pour un poids.
				const float32 vitesse = d.mode == NkSizeMode::Fixed ? 1.f
										: d.mode == NkSizeMode::Fraction ? 0.01f : 0.05f;
				const float32 vmin = 0.f;
				const float32 vmax = d.mode == NkSizeMode::Fraction ? 1.f : 4096.f;
				char id[32];
				snprintf(id, sizeof(id), "%s##insp.taille", titre);
				if (nkgui::DragFloat(ctx, id, d.value, vitesse, vmin, vmax))
					mSt->doc.MarkHumanEdit(mSt->selected);
				LignesBornes(ctx, titre, d);
			}

			/// Les BORNES d'un axe (InspecteurV2, Banani doc 11 §1.6) : « min /
			/// max en retrait » sous la ligne de taille — pour TOUS les modes
			/// (la maquette borne une largeur `expand` a 120/320). 0 = sans
			/// borne, et la maquette l'ecrit « — » : le DragFloat a 0 se lit
			/// pareil.
			void LignesBornes(NkGuiContext &ctx, const char *titre, NkSizeDecl &d) {
				char id[40];
				snprintf(id, sizeof(id), "  min##insp.%s", titre);
				if (nkgui::DragFloat(ctx, id, d.minVal, 1.f, 0.f, 4096.f))
					mSt->doc.MarkHumanEdit(mSt->selected);
				snprintf(id, sizeof(id), "  max##insp.%s", titre);
				if (nkgui::DragFloat(ctx, id, d.maxVal, 1.f, 0.f, 4096.f))
					mSt->doc.MarkHumanEdit(mSt->selected);
			}

			// ── ANCRAGE : les quatre bords, quand le PARENT est en `Anchor` ──
			void CorpsAncrage(NkGuiContext &ctx) {
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
				CaseAncrage(ctx, n, "Gauche", editorkit::nkanchor::Left);
				CaseAncrage(ctx, n, "Haut", editorkit::nkanchor::Top);
				CaseAncrage(ctx, n, "Droite", editorkit::nkanchor::Right);
				CaseAncrage(ctx, n, "Bas", editorkit::nkanchor::Bottom);
			}

			void CaseAncrage(NkGuiContext &ctx, NkUINode *n, const char *titre, uint8 bit) {
				bool v = (n->anchorEdges & bit) != 0;
				if (nkgui::Checkbox(ctx, titre, v)) {
					if (v)
						n->anchorEdges |= bit;
					else
						n->anchorEdges = (uint8)(n->anchorEdges & ~bit);
					mSt->doc.MarkHumanEdit(mSt->selected);
				}
			}

			// ── ALIGNEMENT : l'agencement que ce noeud impose a SES ENFANTS ──
			// ⚠️ C'EST LE SENS DU CHAMP, ET IL SE DIT : `layout` s'applique aux
			//    ENFANTS, jamais au noeud lui-meme (c'est ecrit sur le champ, et
			//    l'oublier ferait chercher l'effet au mauvais etage).
			void CorpsAlignement(NkGuiContext &ctx) {
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
				designkit::KeyValue(ctx, "Agencement", NkLayoutKindName(n->layout.kind));
				designkit::KeyValue(ctx, "  (s'applique aux enfants)", "");
				static const char *const kAligns[4] = {"début", "centre", "fin", "étirer"};
				const int32 m = designkit::Segmented(ctx, kAligns, 4, (int32)n->layout.mainAlign,
													 "insp.align.main");
				if (m >= 0 && m != (int32)n->layout.mainAlign) {
					n->layout.mainAlign = (editorkit::NkAlign)m;
					mSt->doc.MarkHumanEdit(mSt->selected);
				}
				const int32 c = designkit::Segmented(ctx, kAligns, 4, (int32)n->layout.crossAlign,
													 "insp.align.cross");
				if (c >= 0 && c != (int32)n->layout.crossAlign) {
					n->layout.crossAlign = (editorkit::NkAlign)c;
					mSt->doc.MarkHumanEdit(mSt->selected);
				}
			}

			// ── APPARENCE : les jetons du composant, et leur role effectif ──
			void CorpsApparence(NkGuiContext &ctx) {
				const NkUINode *n = NoeudCourant();
				if (!n) {
					designkit::KeyValue(ctx, "Apparence", "-");
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
			bool mDeplierUneFois = true; ///< dépliage initial des sections (une fois)
			/// Le tampon d'édition du contenu texte (section Typographie) et le
			/// nœud qu'il reflète — recopié à chaque changement de sélection.
			int32 mTexteNode = -1;
			char mTexteBuf[128] = {};
	};

} // namespace nkuidesign
