#pragma once
// -----------------------------------------------------------------------------
// @File    NkCoquilleDocument.h
// @Brief   LE FIL MANQUANT : une application construit sa coquille depuis des
//          documents `.nkgui` posés sur le disque, et non depuis son code.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QUI EXISTAIT, ET CE QUE CE FICHIER N'ÉCRIT DONC PAS
// =============================================================================
//  Mesure du 2026-09-25, par OUVERTURE des fichiers et non par comptage :
//
//   - NKUIDesign ÉCRIT un `.nkgui` (`NkGuiEcrire.h`, aller-retour prouvé) ;
//   - le format se LIT (`NkGuiArchive::Read`, purement syntaxique) ;
//   - le document se MONTE en widgets réels (`NKGui/Doc/NkGuiMonteur.h`,
//     2 242 lignes, 26 rôles, zone hôte comprise) ;
//   - ce qui s'EXÉCUTE existe aussi (`NKGui/Doc/NkGuiInteraction.h`,
//     1 385 lignes) : états d'apparence, `behavior` évalué, `Callback` rendu à
//     l'application, `bind` vers une donnée vivante.
//
//  Et pourtant, le 25/09 au matin, **aucune application ne construisait sa
//  coquille depuis un document**. Les seuls appelants du monteur étaient deux
//  bancs (`NKGuiMonteTest`, `NKGuiInteractTest`) et l'écrivain de NKUIDesign.
//  Ce fichier est ce fil-là, et rien d'autre. Il n'ajoute **aucun widget**,
//  **aucune règle de format**, **aucune ligne à NKGui ni à NKEditorKit**.
//
// =============================================================================
//  OÙ IL SE BRANCHE — LES COUTURES EXISTAIENT DÉJÀ
// =============================================================================
//  `NkEditorShell` expose des crochets de bande, tous de la même signature
//  `void (*)(NkEditorFrameContext &, void *)` :
//      SetMenuBar / SetToolbar / SetStatusBarFn
//  et la coquille POSE la région de mise en page avant de les appeler
//  (`NkEditorShell.cpp` : `mUI.layout.region = rect` puis le curseur, l. 2913
//  pour la barre d'outils, l. 1602 pour la barre d'état). C'est exactement ce
//  que `NkGuiMonteur::Monter` demande de son appelant (« l'appelant a déjà fait
//  `ctx.BeginLayout(region)` »). **Rien à ajouter au kit.**
//
//  ⚠️ ET C'EST POURQUOI ON NE DÉMÉNAGE RIEN. La première idée était de faire
//     descendre la coquille de NK3DModeler (30 fichiers, 33 857 lignes) dans
//     `NKEditorKit`. Elle aurait figé dans du code ce que Rodolf veut pouvoir
//     redessiner — et un autre agent travaille dans ce shell.
//
// =============================================================================
//  LA FRONTIÈRE, MESURÉE ET NON DÉCRÉTÉE
// =============================================================================
//  Le document porte : la STRUCTURE, les RÔLES, les LIBELLÉS, les ANCRAGES, les
//  ÉTATS d'apparence, le COMPORTEMENT (`behavior`), et le NOM de l'action.
//  L'application garde : **ce que l'action FAIT**.
//
//  ⚠️ IL MANQUE UN MAILLON DANS LE FORMAT, ET IL FAUT LE NOMMER PLUTÔT QUE DE
//     L'INVENTER. Le monteur écrit `(void)Button(ctx, s.CStr())`
//     (`NkGuiMonteur.h`, cas `Button`) : **l'appui est jeté**. Et la grammaire
//     du document n'a pas de `on Pressed -> Behavior "..."` — l'en-tête de
//     `NkGuiInteraction.h` le dit lui-même, limite (4) et (5) : « aucun fichier
//     du corpus n'écrit un seul `on` », donc « l'HÔTE tranche », et il tranche
//     en passant TOUS les `behavior` une fois par image.
//
//     Conséquence directe, et elle se voit : un `behavior` ne peut réagir qu'à
//     une VALEUR (celle d'une case ou d'un curseur, via `bind`), jamais à un
//     APPUI. Un bouton, lui, n'a pas de valeur.
//
//     On ne bricole donc pas un `on` dans le format — **c'est une décision de
//     conception qui appartient à Rodolf**. On emploie à la place la convention
//     déjà posée par la zone hôte, et on l'emploie à l'identique :
//
//         `Host "viseur3d"`   -> l'hôte répond au NOM, ou la zone se signale
//         `Button "anim.jouer"` -> l'hôte répond au NOM, ou le bouton se signale
//
//     L'identifiant du bouton EST le nom de l'action. C'est une convention de
//     CET hôte, pas une extension du format : aucun fichier du format n'est
//     touché, et le jour où `on Pressed` existe, cette classe-ci change, et
//     elle seule.
//
//  ⚠️ ET LA DÉTECTION DE L'APPUI EST UNE DÉRIVATION. Elle refait la condition
//     de `ButtonBehavior` (« vrai au relâchement dans le rect ») :
//         survolé au moment du relâchement, et non grisé.
//     Elle est ÉCRITE ici parce qu'une dérivation qu'on ne nomme pas devient un
//     doublon qu'on croit être une source. **Condition de retrait :** le jour où
//     le format porte un événement, ou le jour où le monteur rend l'appui,
//     `Apres()` ci-dessous se supprime.
//
//  ⚠️ UN NOM QUE PERSONNE NE SERT NE DOIT PAS SE TAIRE. Un bouton dont l'action
//     n'est connue d'aucune table est COMPTÉ (`actionsInconnues`) — même motif
//     que `hotesNonRemplis` : un bouton muet se fait prendre pour un bouton qui
//     marche.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Doc/NkGuiInteraction.h" // tire NkGuiMonteur + NkGuiArchive

namespace nkanima {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;
	using nkentseu::NkString;
	using nkentseu::NkStringView;
	// NkVector reste QUALIFIE : `Panels.h` ouvre deja `nkentseu::nkgui` dans CE
	// namespace, et une using-declaration de plus entre en conflit avec le nom
	// deja visible. L'ordre des inclusions deciderait sinon de la compilation.
	// NkFile reste QUALIFIE : un `using` dessus entre en conflit avec un nom deja
	// en portee via NKEditorKit. Qualifier coute trois mots, un conflit coute une heure.
	using nkentseu::editorkit::NkEditorFrameContext;
	using nkentseu::editorkit::NkEditorPanel;
	using nkentseu::editorkit::NkEditorDockSide;
	using namespace nkentseu::nkgui;

	// =========================================================================
	//  CE QUE L'APPLICATION FOURNIT : des actions et des zones, par NOM
	// =========================================================================
	using NkActionFn = void (*)(void *);

	struct NkActionNommee {
			const char *nom = nullptr; ///< l'identifiant du bouton dans le document
			NkActionFn fn = nullptr;
			void *user = nullptr;
	};

	/// Ce que l'hôte sait peindre dans une zone `Host "nom"`.
	using NkZoneFn = bool (*)(NkGuiContext &, const NkRect &, void *);

	struct NkZoneNommee {
			const char *nom = nullptr;
			NkZoneFn fn = nullptr;
			void *user = nullptr;
	};

	// =========================================================================
	//  UNE BANDE : un fichier, son archive, son état, son exécution
	// =========================================================================
	/**
	 * ⚠️ UNE EXÉCUTION PAR BANDE, ET CE N'EST PAS DU CONFORT. `NkGuiInfosDoc`
	 *    est indexée par identifiant de widget, et `NkGuiMonteEtat` par clé de
	 *    `bind`. Deux documents partageant une exécution partageraient leurs
	 *    identifiants : deux boutons « fermer » dans deux bandes deviendraient
	 *    le même. Le format ne garantit l'unicité qu'à l'intérieur d'un fichier.
	 */
	class NkBandeDocument : public NkGuiExecution {
		public:
			// ── l'état de chargement, et il se dit ────────────────────────
			bool lu = false;		 ///< l'archive a été lue
			NkString chemin;		 ///< d'où elle vient
			NkString refus;			 ///< pourquoi elle n'a pas été lue (jamais vide si !lu)
			nkentseu::NkArchive doc; ///< le document
			NkGuiMonteEtat etat;	 ///< le magasin des valeurs éditées
			NkGuiMonteRapport rap;	 ///< le relevé du dernier montage

			// ── ce que ce montage a fait des actions ──────────────────────
			uint32 boutons = 0;			  ///< boutons montés à la dernière image
			uint32 actionsTirees = 0;	  ///< actions réellement appelées (cumul)
			uint32 actionsInconnues = 0;  ///< appuis sur un nom que personne ne sert (cumul)
			uint32 zonesRemplies = 0;	  ///< zones hôte servies à la dernière image

			// ── les tables que l'application pose ─────────────────────────
			const NkActionNommee *actions = nullptr;
			uint32 nbActions = 0;
			const NkZoneNommee *zones = nullptr;
			uint32 nbZones = 0;

			// ═════════════════════════════════════════════════════════════
			//  LA FAÇADE PREND UN ARBRE, JAMAIS UN CHEMIN
			// ═════════════════════════════════════════════════════════════
			/**
			 * Décision de conception de Rodolf, 25/09 : *« je ne veux rien perdre. »*
			 * Lire le document au lancement (on redessine sans recompiler) et
			 * l'embarquer à la compilation (rien à livrer, rien à lire) **ne sont pas
			 * deux produits** : ce sont deux CONFIGURATIONS DE CONSTRUCTION du même
			 * document.
			 *
			 * ⚠️ ET LE PIÈGE EST NOMMÉ : si l'embarquement émettait du C++ qui appelle
			 *    les widgets, il existerait **deux implémentations de ce que signifie
			 *    un document**, et elles divergeraient. Ce dépôt a déjà payé ce motif
			 *    (*deux compteurs sans code commun*, *une dérivation en double : l'un
			 *    publie, l'autre lit*). La parade n'est pas la vigilance, c'est la
			 *    forme : **le monteur reste la seule implémentation de la sémantique**,
			 *    et l'embarquement ne fait qu'économiser la lecture du texte.
			 *
			 * C'est pourquoi `Adopter` prend un **ARBRE DÉJÀ ANALYSÉ** et que la
			 * lecture du disque n'est qu'**un appelant parmi d'autres**. Le jour où un
			 * générateur émettra l'arbre en mémoire, il appellera `Adopter` et **rien
			 * d'autre ne changera ici**.
			 */
			bool Adopter(const nkentseu::NkArchive &arbre) noexcept {
				lu = false;
				refus.Clear();
				doc = arbre;
				NkGuiMonteur::Preparer(doc, etat);
				infos.Lire(doc);
				NkGuiExecution::etat = &etat;
				eval.rappel = &NkBandeDocument::SurCallback;
				eval.rappelUser = this;
				lu = true;
				return true;
			}

			/// UN appelant de `Adopter` : celui qui lit le texte sur le disque.
			/// Rend faux AVEC un refus nommé — jamais une bande vide.
			///
			/// ⚠️ `ReadAllBytes`, JAMAIS `ReadAllText`. Ce dépôt a payé : `WriteAllText`
			///    écrit en CRLF et `ReadAllText` renormalise — l'écart serait masqué des
			///    deux côtés à la fois. Un document est une suite d'octets.
			bool ChargerDepuisFichier(const char *ch) noexcept {
				lu = false;
				refus.Clear();
				chemin = NkString(ch);
				const nkentseu::NkVector<nkentseu::nk_uint8> octets = nkentseu::NkFile::ReadAllBytes(ch);
				if (octets.Size() == 0u) {
					refus = NkString("fichier introuvable ou vide");
					return false;
				}
				nkentseu::NkArchive arbre;
				nkentseu::NkGuiDiag err;
				if (!NkGuiArchive::Read((const char *)octets.Data(), (uint32)octets.Size(), arbre, err)) {
					// LE REFUS EST NOMMÉ, et il porte la ligne. Une fenêtre vide n'est
					// pas un échec acceptable : ce soir même, une démo a été déclarée
					// livrée parce qu'elle « s'ouvre et tient », et elle s'ouvrait vide.
					refus = NkPrefixeRefus(err);
					return false;
				}
				const NkString garde = chemin;
				const bool ok = Adopter(arbre);
				chemin = garde; // `Adopter` ne connaît aucun chemin : c'est le sujet
				return ok;
			}

			/// Monte la bande dans la région que la coquille vient de poser.
			void Monter(NkGuiContext &ctx) noexcept {
				if (!lu) {
					PeindreRefus(ctx);
					return;
				}
				rap = NkGuiMonteRapport();
				boutons = 0;
				zonesRemplies = 0;
				ReinitialiserCompteurs();
				Brancher(ctx);
				NkGuiMonteur::Monter(ctx, doc, etat, rap, this);
				// Les comportements APRÈS le montage — la limite (5) de
				// NkGuiInteraction.h : `n1.value` doit être la valeur que le widget
				// vient d'avoir, pas celle de l'image d'avant.
				ExecuterComportements(doc);
				Debrancher(ctx);
			}

			// ── crochet du monteur : la zone hôte, par NOM ────────────────
			bool RemplirHote(NkGuiContext &ctx, const char *nom, const NkRect &zone) noexcept override {
				for (uint32 i = 0; i < nbZones; ++i) {
					if (zones[i].nom && zones[i].fn && NkGMotEgal(NkStringView(nom), zones[i].nom)) {
						if (zones[i].fn(ctx, zone, zones[i].user)) {
							++zonesRemplies;
							return true;
						}
						return false; // il a dit non : le monteur peindra ses hachures
					}
				}
				return false;
			}

			// ── crochet du monteur : l'appui, dérivé (voir l'en-tête) ─────
			void Apres(NkGuiContext &ctx, const nkentseu::NkArchive &w, NkStringView role,
					   NkGuiMonteEtat::Entree *e) noexcept override {
				// L'exécution garde ses propres devoirs (anneau de focus, donnée
				// vivante, EndDisabled) : on l'appelle AVANT d'ajouter les nôtres.
				NkGuiExecution::Apres(ctx, w, role, e);
				// ⚠️ `MenuItem` EST DE LA MEME FAMILLE, ET CE N'EST PAS UNE SUPPOSITION :
				//    il passe par le MEME `ctx.ButtonBehavior(...)` que `Button`
				//    (`NkGuiWidgets.cpp:5303`), donc il pose `lastItemHovered` de la meme
				//    facon, donc la meme condition le detecte. Une entree de menu qui
				//    n'agirait pas serait un menu purement decoratif.
				if (!NkGMotEgal(role, "Button") && !NkGMotEgal(role, "RepeatButton")
					&& !NkGMotEgal(role, "MenuItem"))
					return;
				++boutons;
				// La condition de `ButtonBehavior` : relâchement, survolé, non grisé.
				if (!ctx.IsItemHovered() || !ctx.input.mouseReleased[0] || ctx.IsDisabled())
					return;
				Declencher(NkGuiArchive::IdOf(w));
			}

			/// Le nom de l'action est l'identifiant du bouton.
			void Declencher(NkStringView nom) noexcept {
				for (uint32 i = 0; i < nbActions; ++i) {
					if (actions[i].nom && actions[i].fn && NkGMotEgal(nom, actions[i].nom)) {
						actions[i].fn(actions[i].user);
						++actionsTirees;
						return;
					}
				}
				// PERSONNE NE SERT CE NOM. On le compte plutôt que de le taire :
				// même motif que `hotesNonRemplis`.
				++actionsInconnues;
			}

		private:
			/// `Callback "nom"(...)` émis par un `behavior` : même table que les boutons.
			static void SurCallback(const NkGuiAppelCallback &a, void *user) noexcept {
				auto *b = (NkBandeDocument *)user;
				if (b)
					b->Declencher(NkStringView(a.nom.Data(), (nkentseu::usize)a.nom.Size()));
			}

			static NkString NkPrefixeRefus(const nkentseu::NkGuiDiag &err) noexcept {
				// Le diagnostic du lecteur, tel quel : il porte déjà la ligne et la
				// colonne. Le reformuler ferait perdre ce qu'il sait.
				return NkString(err.message);
			}

			/// Un document qu'on n'a pas pu lire ne laisse pas une bande vide : il
			/// ÉCRIT pourquoi, là où il aurait dû se monter.
			void PeindreRefus(NkGuiContext &ctx) noexcept {
				if (!ctx.font || !ctx.font->Valid())
					return;
				const NkRect r = ctx.layout.region;
				NkString m = NkString("Document d'interface refusé : ");
				m += chemin;
				m += NkString("  —  ");
				m += refus.Size() ? refus : NkString("raison inconnue");
				ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
								 {r.x + 8.f, r.y + 4.f}, m.CStr(), NkColor{232, 96, 80, 255});
			}
	};

	// =========================================================================
	//  LA COQUILLE : les bandes, et les crochets que la coquille appelle
	// =========================================================================
	/**
	 * ⚠️ PLUSIEURS FICHIERS, ET C'EST UNE CONSÉQUENCE DU FORMAT, PAS UN CHOIX DE
	 *    CONFORT. `NkGuiMonteur::Monter` monte TOUTES les sections `widgets` d'un
	 *    document, dans l'ordre du fichier, au curseur courant. Il n'existe
	 *    **aucune API pour ne monter qu'un sous-arbre nommé** (`MonterCorps` est
	 *    privée). Or la barre de menu, la barre d'outils et la barre d'état sont
	 *    trois RÉGIONS que la coquille pose à trois instants différents. Un seul
	 *    document ne peut donc pas les servir toutes les trois.
	 *
	 *    C'est un vrai manque du maillon, et il est nommé dans la réponse plutôt
	 *    que contourné en silence : **il manque `Monter(ctx, doc, "nom_de_racine")`.**
	 *
	 * ⚠️ LA BARRE DE MENU N'EST PAS ICI, ET C'EST UNE MESURE, PAS UN OUBLI. Le
	 *    format connaît `MenuBar`, `Menu`, `MenuItem` et `ContextMenu`
	 *    (`NkGuiValidate.h`, table des 41 rôles) ; **le monteur n'en monte
	 *    aucun** (son énumération en compte 26, et ces quatre n'y sont pas). Un
	 *    document de barre de menu tomberait donc dans le repli « rôle inconnu »
	 *    et se monterait à plat, sans menu déroulant.
	 *
	 *    Et le brancher coûterait plus que ça : `SetMenuBar` REMPLACE
	 *    entièrement les menus de la coquille (`NkEditorShell.cpp`, l. 3459 :
	 *    « Barre COMPLETE fournie par l'app […] remplace entierement les menus
	 *    par defaut »). On échangerait des menus qui marchent contre une rangée
	 *    plate. **On ne le fait pas, et on le dit.**
	 */
	class NkCoquilleDocument {
		public:
			/// LE MENU D'APPLICATION. Monté par `SetAppMenu`, qui est **additif** :
			/// la coquille l'appelle DANS sa propre barre, après ses quatre menus
			/// (`NkEditorShell.cpp:3497`). On n'emploie surtout pas `SetMenuBar`, qui
			/// la remplacerait : son menu « Affichage » est construit par
			/// `DrawPanelsMenuItems()`, qui énumère les panneaux à l'exécution, et
			/// **le format ne sait pas exprimer une liste engendrée**. On ne migre
			/// pas vers moins.
			NkBandeDocument menuApp;
			NkBandeDocument barreOutils;
			NkBandeDocument barreEtat;
			NkBandeDocument panneau;

			/// Le dossier qui porte les documents. Tout est relatif à lui : le jour
			/// où une VARIANTE arrive (Nogee/ECS, NKScena/séquence), c'est ce
			/// dossier-là qui change, et rien d'autre.
			NkString dossier;

			/// Lit les trois documents du dossier. C'est LA CONFIGURATION DE
			/// DÉVELOPPEMENT ; celle de livraison appellera `Adopter` bande par bande.
			bool ChargerDepuisDossier(const char *dossierDocuments) noexcept {
				dossier = NkString(dossierDocuments);
				const bool a = menuApp.ChargerDepuisFichier(Joindre("menu_animation.nkgui").CStr());
				const bool b = barreOutils.ChargerDepuisFichier(Joindre("barre_outils.nkgui").CStr());
				const bool c = barreEtat.ChargerDepuisFichier(Joindre("barre_etat.nkgui").CStr());
				const bool d = panneau.ChargerDepuisFichier(Joindre("panneau_outils.nkgui").CStr());
				return a && b && c && d;
			}

			/// Pose les mêmes tables sur les quatre bandes.
			void PoserTables(const NkActionNommee *act, uint32 nAct, const NkZoneNommee *zon,
							 uint32 nZon) noexcept {
				NkBandeDocument *b[4] = {&menuApp, &barreOutils, &barreEtat, &panneau};
				for (uint32 i = 0; i < 4u; ++i) {
					b[i]->actions = act;
					b[i]->nbActions = nAct;
					b[i]->zones = zon;
					b[i]->nbZones = nZon;
				}
			}

			// ── les crochets, de la signature exacte que le kit attend ────
			/// ⚠️ LA BARRE EST DEJA OUVERTE quand la coquille appelle ce crochet : le
			///    document ne doit donc porter que des `Menu`, jamais un `MenuBar`.
			static void MonterMenuApp(NkEditorFrameContext &ec, void *user) noexcept {
				((NkCoquilleDocument *)user)->menuApp.Monter(ec.Ui());
			}
			static void MonterBarreOutils(NkEditorFrameContext &ec, void *user) noexcept {
				((NkCoquilleDocument *)user)->barreOutils.Monter(ec.Ui());
			}
			static void MonterBarreEtat(NkEditorFrameContext &ec, void *user) noexcept {
				((NkCoquilleDocument *)user)->barreEtat.Monter(ec.Ui());
			}

			/// Combien de widgets les trois bandes ont monté à la dernière image.
			uint32 MontesTotal() const noexcept {
				return menuApp.rap.montes + barreOutils.rap.montes + barreEtat.rap.montes
					+ panneau.rap.montes;
			}
			uint32 RefusTotal() const noexcept {
				return (menuApp.lu ? 0u : 1u) + (barreOutils.lu ? 0u : 1u)
					+ (barreEtat.lu ? 0u : 1u) + (panneau.lu ? 0u : 1u);
			}

			NkString Joindre(const char *nom) const noexcept {
				NkString s = dossier;
				if (s.Size() > 0u) {
					const char d = s.Data()[s.Size() - 1u];
					if (d != '/' && d != '\\')
						s += NkString("/");
				}
				s += NkString(nom);
				return s;
			}
	};

	// =========================================================================
	//  LE PANNEAU DONT LE CONTENU EST UN DOCUMENT
	// =========================================================================
	/// Le CONTENU du panneau vient du document ; **son TITRE, non**, et il faut le
	/// dire plutôt que de le laisser croire.
	///
	/// 🔴 J'AVAIS ÉCRIT ICI QUE LE TITRE VENAIT DU DOCUMENT. C'était faux : il est
	///    le littéral `"Outils"` ci-dessous. `NkEditorPanel` garde son titre dans un
	///    `char mTitle[64]` posé à la construction et **n'expose aucun `SetTitle`** ;
	///    or le panneau se construit AVANT que le document ne soit lu. Le rendre
	///    dynamique demanderait de toucher `NKEditorKit`, qui appartient à un autre
	///    agent. *Déclarer n'est pas livrer* : la ligne est corrigée, pas le code.
	///
	/// **CONDITION DE RETRAIT :** le jour où `NkEditorPanel` accepte un titre après
	/// coup, ce constructeur lit `title` sur le bloc racine du document et cette
	/// note disparaît.
	class PanneauDocument : public NkEditorPanel {
		public:
			explicit PanneauDocument(NkBandeDocument &bande) noexcept
				: NkEditorPanel("panneau_document", "Outils", NkEditorDockSide::NK_LEFT), mBande(bande) {
			}

			void OnUI(NkEditorFrameContext &ec) override {
				mBande.Monter(ec.Ui());
			}

		private:
			NkBandeDocument &mBande;
	};

} // namespace nkanima
