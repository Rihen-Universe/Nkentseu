#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorShell.h
// @Brief   Coquille d'application d'editeur : fenetre + docking + panneaux.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// NkEditorShell est la base reutilisable des editeurs Nkentseu (NKCode = IDE,
// et plus tard Nogee = editeur de moteur). Elle POSSEDE :
//   - la fenetre (NKWindow) + la cible de rendu (NKCanvas/NkRenderWindow),
//   - le contexte NKGui (qui contient fenetres/dock/layout/police/draw lists),
//   - la barre de menus (Fichier/Affichage/Fenetre + menu applicatif optionnel),
//   - la palette de commandes (Ctrl+P),
//   - la boucle principale (events -> frame -> docking -> panneaux -> rendu).
//
// L'application N'A QU'A : creer le shell, enregistrer ses panneaux/commandes,
// appeler Run(). Pipeline NKGui 2D pur (NKCanvas), independant du moteur 3D.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkEditorExport.h"
#include "NKEditorKit/NkEditorContext.h"
#include "NKEditorKit/NkEditorPanel.h"
#include "NKEditorKit/NkEditorCommand.h"
// ⚠️ La coquille porte LE point de synchronisation des deux objets theme
//    (`ApplyTheme`) : elle a donc besoin des ROLES de l editeur, en plus du
//    `NkGuiTheme` que `NKGui/NKGui.h` lui apporte plus bas.
#include "NKEditorKit/NkTheme.h"

#include "NKWindow/NKWindow.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkClock.h"
#include "NKGui/NKGui.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKEditorKit/NkFontPrefs.h"
#include "NKEditorKit/NkIEditorRenderer.h" // backend de rendu pluggable
#include "NKEditorKit/NkEditorContextMenu.h" // NkCtxMenu : sous-menu du menu contextuel shell

namespace nkentseu {
	namespace editorkit {

		// Parametres de creation du shell.
		struct NKEDITORKIT_API NkEditorShellConfig {
				const char *title = "Nkentseu Editor";
				uint32 width = 1280;
				uint32 height = 720;
				bool resizable = true;
				NkEditorGfxApi graphicsApi = NkEditorGfxApi::Auto;
				// Backend de rendu INJECTE (optionnel). nullptr => impl NKCanvas par
				// defaut (IDE). Une app 2D/3D (anim/moteur) fournit ici une impl NKRHI/
				// NKRenderer. Le shell NE POSSEDE PAS un renderer injecte (l'app le gere).
				NkIEditorRenderer *renderer = nullptr;
		};

		// Menu applicatif optionnel : appele a l'interieur de la barre de menus,
		// apres « Fenetre », pour que l'app ajoute ses propres menus.
		using NkEditorAppMenuFn = void (*)(NkEditorFrameContext &ec, void *user);

		class NKEDITORKIT_API NkEditorShell {
			public:
				static constexpr int32 MAX_PANELS = 64;
				static constexpr int32 MAX_COMMANDS = 128;

				NkEditorShell() = default;
				~NkEditorShell();

				NkEditorShell(const NkEditorShell &) = delete;
				NkEditorShell &operator=(const NkEditorShell &) = delete;

				// ── Cycle de vie ────────────────────────────────────────────────────
				bool Init(const NkEditorShellConfig &config) noexcept;
				int Run() noexcept; ///< boucle bloquante ; retourne le code de sortie

				void RequestClose() noexcept {
					mRunning = false;
				}

				/// Le backend de rendu ACTIF — injecté par l'app, ou NKCanvas par
				/// défaut. Jamais nul après un Init() réussi. Exposé pour que l'app
				/// puisse armer une capture (`CaptureNext`) sans posséder le backend
				/// qu'elle n'a pas injecté.
				NkIEditorRenderer *Renderer() noexcept {
					return mRenderer;
				}

				// ── Fermeture EXPLICITE de la fenetre (croix de la barre de titre) ──
				// Declenche uniquement par NkWindowCloseEvent, JAMAIS par RequestClose()
				// ci-dessus. La distinction est necessaire a l'application : fermer une
				// fenetre (« je n'ai plus besoin de ce workspace ») et quitter par Ctrl+Q
				// (« je ferme l'application, je reprendrai ou j'en etais ») n'ont pas le
				// meme sens pour la restauration de session au lancement suivant.
				// Retourne true = fermer maintenant ; false = ANNULER la fermeture. Le
				// false rend la main a l'application, qui peut afficher une confirmation
				// (« enregistrer / fermer sans enregistrer / annuler ») puis appeler
				// RequestClose() elle-meme une fois l'utilisateur decide.
				using NkOnWindowClosed = bool (*)(void *user);

				void SetOnWindowClosed(NkOnWindowClosed cb, void *user) noexcept {
					mOnWindowClosed = cb;
					mOnWindowClosedUser = user;
				}

				// ── Demande de fermeture : PASSAGE OBLIGE ───────────────────────────
				// Croix dessinee, menu Quitter, Ctrl+Q, croix de la barre de titre : tout
				// passe ici, pour laisser l'application poser une question. Les boutons
				// dessines mettaient autrefois mRunning a faux directement : toute
				// confirmation etait alors contournee sans qu'on s'en apercoive.
				// `windowClose` distingue « je ferme CETTE fenetre » (croix, Fermer la
				// fenetre) de « je quitte l'application » (Quitter, Ctrl+Q). NKCode s'en
				// sert pour la restauration au lancement suivant : une fenetre fermee
				// explicitement ne revient pas, une session quittee si.
				void RequestQuit(bool windowClose = true) noexcept {
					mQuitIsWindowClose = windowClose;
					if (mOnWindowClosed && !mOnWindowClosed(mOnWindowClosedUser))
						return;
					mRunning = false;
				}

				bool QuitIsWindowClose() const noexcept {
					return mQuitIsWindowClose;
				}

				// ── Enregistrement (le shell NE POSSEDE PAS les panneaux) ───────────
				bool AddPanel(NkEditorPanel *panel) noexcept;
				// Ouvre (si ferme) puis met au premier plan le panneau nomme (onglet de dock).
				bool FocusPanel(const char *title) noexcept;
				bool IsPanelOpen(const char *title) noexcept; ///< panneau nomme ouvert ?
				void ClosePanel(const char *title) noexcept;  ///< ferme le panneau nomme
				int32 PanelDockNode(const char *title) noexcept; ///< feuille de dock (-1 = non ancre)
				void DetachPanel(const char *title) noexcept; ///< retire de sa feuille (collapse si vide)
				// Agrandir / replier la REGION de dock d'un panneau (ajuste le ratio du split
				// parent) : maximiser = la region prend presque toute la place ; replier = juste
				// les onglets. Rebascule a l'etat normal si deja dans cet etat.
				void ToggleMaximizePanel(const char *title) noexcept;
				void ToggleCollapsePanel(const char *title) noexcept;
				int32 PanelRegionMode() const noexcept { return mDockRegionState; } ///< 0 normal,1 replie,2 max

				// ── GESTIONNAIRE DE MENU CONTEXTUEL (réutilisable, shell-level) ──────
				// Un panneau APPELLE OpenContextMenu(pos, items, enabled, count) ; le shell
				// dessine le menu APRÈS tous les panneaux (input réel, occlusion propre via
				// le mécanisme modal). Le panneau récupère l'item choisi via
				// TakeContextMenuChoice() (-1 si aucun). Thémé (NkGuiTheme) -> personnalisable.
				void OpenContextMenu(const nkgui::NkVec2 &pos, const char *const *items, const bool *enabled,
									 int32 count) noexcept;
				// SOUS-MENU (façon menus natifs) : l'item `item` du menu COURANT ouvre au
				// SURVOL (sans clic, flèche ▸) une liste déroulante scrollable V/H
				// (NkCtxMenuDraw). À appeler juste APRÈS OpenContextMenu. Le choix arrive via
				// TakeContextMenuChoice() == item, puis TakeContextMenuSubChoice() = index
				// dans `subItems`. Un seul item à sous-menu par menu.
				// `subIcons` est FACULTATIF : un sous-menu de projets se parcourt alors
				// exactement comme le combo de la barre d'outils (meme icone de Kind),
				// au lieu d'une liste de texte nu qui ne dit pas de quoi il s'agit.
				void SetContextSubmenu(int32 item, const char *const *subItems, int32 count,
									   const uint32 *subIcons = nullptr) noexcept {
					mCtxSubItem = item;
					mCtxSubItems.Clear();
					mCtxSubIcons.Clear();
					for (int32 i = 0; i < count; ++i) {
						mCtxSubItems.PushBack(nkentseu::NkString(subItems[i] ? subItems[i] : ""));
						mCtxSubIcons.PushBack(subIcons ? subIcons[i] : 0u);
					}
					mCtxSub = NkCtxMenu{};
					mCtxSubChoice = -1;
				}
				int32 TakeContextMenuChoice() noexcept {
					const int32 c = mCtxChoice;
					mCtxChoice = -1;
					return c;
				}
				int32 TakeContextMenuSubChoice() noexcept {
					const int32 c = mCtxSubChoice;
					mCtxSubChoice = -1;
					return c;
				}
				bool IsContextMenuOpen() const noexcept {
					return mCtxOpen;
				}

				// ── SÉLECTEUR de FICHIER/DOSSIER GÉNÉRIQUE (modal, réutilisable) ────

				// ── Géométrie de fenêtre (launcher) : fichier global taille/pos/maximisé ──
				void MaximizeWindow() noexcept;
				void SaveWindowGeom(const char *path) noexcept;	 ///< écrit win=/maximized= (position écran)
				bool LoadWindowGeom(const char *path) noexcept;	 ///< applique si le fichier existe ; false sinon

				// BARRES D'ACTIVITE (bandes verticales d'icones, facon VSCode) : presentes
				// par defaut, parce que l'IDE en vit. Une application qui n'a PAS de
				// « vues » a basculer — un atelier, un visualiseur — doit pouvoir les
				// retirer : sinon elle herite du chrome de NKCode et lui ressemble, alors
				// qu'elle ne fait pas le meme metier. Le dock reprend alors la largeur
				// liberee.
				// ── GEOMETRIE DE L EN-TETE (optionnelle) ────────────────────────
				// Par defaut la coquille calcule sa barre de titre (`ItemHeight()+10`)
				// et sa barre d outils (46). Une application dont la maquette impose
				// des cotes exactes les pose ici.
				//
				// ⚠️ ADDITIF : 0 = comportement historique, inchange. Aucune
				//    application existante ne bouge.
				//
				// ⚠️ ET CES VALEURS NE PASSENT PAS PAR `S()`. Ce sont des PIXELS,
				//    pas des unites a mettre a l echelle : quand une maquette dit 28,
				//    la mesure sur la capture doit rendre 28. Les faire passer par le
				//    facteur DPI donnerait 30 sur un ecran a 107 % -- « la valeur est
				//    ecrite » sans etre « la valeur est honoree ».
				//
				// `logo` : bloc CARRE colle au coin haut-gauche, qui CHEVAUCHE les
				// deux bandes. Les deux bandes commencent alors a x = logo.
				void SetHeaderLayout(float32 titleH, float32 bandH, float32 logo) noexcept {
					mHeaderTitleH = titleH;
					mHeaderBandH = bandH;
					mHeaderLogo = logo;
				}

				// ═══════════════════════════════════════════════════════════════
				//  LE POINT DE SYNCHRONISATION DES DEUX OBJETS THEME
				// ═══════════════════════════════════════════════════════════════
				//  ⚠️ IL Y A **DEUX** OBJETS THEME DANS CE DEPOT, ET RIEN NE LES
				//     SYNCHRONISAIT :
				//       - `editorkit::NkTheme` / `NkThemeLibrary` : les ROLES, les
				//         jetons nommes, l audit de contraste, les fichiers de
				//         theme. C est **l autorite** ;
				//       - `nkgui::NkGuiTheme` (porte par `NkGuiContext`) : ce que le
				//         DESSIN lit, a chaque primitive.
				//
				//  DEFAUT MESURE (agent NKCraft, chantier voisin) : bascule de
				//  l editeur en Clair, le levier repond « accepte (courant : Clair) »,
				//  les vignettes du navigateur changent — et le menu contextuel ne
				//  bouge pas d un pixel, parce que `NkCtxMenuDraw` lit `ctx.theme`.
				//  **Deux objets, chacun cru par une partie du dessin.** C est le
				//  motif des deux registres homonymes de roles, applique aux themes.
				//
				//  ⚠️ DEUX OBJETS N EST PAS LE PROBLEME — ils sont dans deux couches,
				//     NKGui dessous, NKEditorKit dessus, ce que la regle « UN SEUL
				//     SOCLE D INTERFACE DANS NKGui, SPECIALISE PAR EDITEUR » (Rodolf,
				//     2026-08-17) prescrit. **Deux AUTORITES, si.** D ou : une seule
				//     direction, la bibliotheque POUSSE vers le theme du dessin, et
				//     elle le fait ICI, en un seul endroit qu on peut citer.
				//
				//  ⚠️ CE QUE LA CONVERSION NE PEUT PAS FAIRE, et il faut le savoir
				//     pour ne pas s y fier a tort :
				//       - `NkGuiTheme` porte des ETATS (survol, actif) que `NkTheme`
				//         ne porte pas en roles. Ils sont **derives par une regle
				//         nommee** — melange vers l accent — pas inventes couleur par
				//         couleur ;
				//       - `success` / `warning` / `danger` / `info` n ont AUCUN role
				//         equivalent cote editeur : ils sont **laisses tels quels**,
				//         et c est dit plutot que tu ;
				//       - `scrim` et `shadow` sont des voiles noirs a alpha : ils ne
				//         dependent pas du theme.
				void ApplyTheme(const NkTheme &t) noexcept;

				// ⚠️ L INDICATEUR « Zoom NNN% » DU PIED N EST PAS UN ZOOM DE VUE :
				//    il rend `ActiveCodeSize() / kDefaultCodeFontSize`, c est-a-dire
				//    le zoom de la POLICE DE CODE (Ctrl+= / Ctrl+0), une notion de
				//    NKCode. Une application sans editeur de code l affiche sans
				//    rien derriere -- et si elle a par ailleurs un zoom de TOILE,
				//    l ecran porte deux nombres sous le meme mot, qui ne disent pas
				//    la meme chose. Mesure NkUIDesign : « 100 % » dans le cluster de
				//    toile et « Zoom 107 % » au pied, au meme instant.
				//    ⚠️ ADDITIF : `true` = comportement historique, NKCode ne bouge
				//       pas.
				/// DEBRANCHER LA BARRE D'ETAT ENTIERE, PAS LA DETRUIRE (meme regle
				/// que SetActivityBars) — ajoute le 2026-08-30 : le plan de
				/// NkUIDesign (§4/§13) ne prevoit qu'UN bandeau bas, le rail de
				/// pastilles ; la barre d'etat VSCode en faisait un second. A
				/// faux, la bande basse n'est plus ni reservee ni dessinee (sauf
				/// si un SetStatusBarFn est pose : le hook garde sa bande), et
				/// SetFooter route son texte vers le RAIL BAS — une seule verite
				/// d'affichage, aucun message perdu.
				void SetStatusBarVisible(bool v) noexcept {
					mStatusBarVisible = v;
				}
				/// DEBRANCHER LA GRANDE BARRE DE DEFILEMENT DES PANNEAUX ANCRES
				/// (meme motif que SetStatusBarVisible — additif, 01/09, retour de
				/// Rodolf : « la scrollbar la plus grande doit etre supprimee »).
				/// A faux, les fenetres de panneaux passent NoScrollbar a NKGui :
				/// la MOLETTE et le bornage restent (le contenu long demeure
				/// atteignable), seule la barre externe ne se dessine plus — pour
				/// les applications dont les panneaux portent leurs propres
				/// ascenseurs par section. Defaut : vrai (NKCode et les autres
				/// consommateurs ne bougent pas).
				void SetDockScrollbarVisible(bool v) noexcept {
					mDockScrollbars = v;
				}
				/// Le TEXTE du rail bas, a droite des pastilles (l'aide
				/// contextuelle de l'outil arme, l'etat de l'application).
				void SetRailFooterText(const char *t) noexcept {
					// copie bornee inline : `CopyStr` vit dans le .cpp
					uint32 i = 0;
					for (; t && t[i] && i + 1 < (uint32)sizeof(mRailFooterText); ++i)
						mRailFooterText[i] = t[i];
					mRailFooterText[i] = 0;
				}

				void SetFooterZoomIndicator(bool visible) noexcept {
					mFooterZoom = visible;
				}

				// ═══════════════════════════════════════════════════════════════
				//  RAILS DE PASTILLES (document 3 §13) — ETATS 1 ET 2
				// ═══════════════════════════════════════════════════════════════
				//  Trois rails FINS (28 px) aux bords du corps, EN DEHORS des
				//  panneaux fixes. Chacun porte des pastilles de 28x28, une par
				//  panneau SECONDAIRE.
				//
				//  ETAT 1 — repliee : l icone seule, infobulle au survol.
				//  ETAT 2 — depliee : le panneau glisse EN OVERLAY par-dessus le
				//           canvas, voile semi-transparent dessous, ferme par un
				//           clic ailleurs ou un second clic sur la pastille.
				//
				//  ⚠️ L ETAT 2 NE REDIMENSIONNE PAS LE CANVAS, et c est EXACTEMENT
				//     ce qui le distingue de l etat 3 (ancre). Le rectangle du dock
				//     ne retranche que les 28 px des RAILS, jamais la largeur du
				//     tiroir. Si un jour le dock se met a bouger quand on deplie,
				//     l etat 2 est devenu l etat 3 sans que personne l ait decide.
				//
				//  ⚠️ UNE SEULE PASTILLE DEPLIEE PAR RAIL (§13.3) : deplier la
				//     seconde referme la premiere. La regle vit dans `mRailOuvert`,
				//     UN entier par rail — un ensemble d ouverts aurait rendu la
				//     regle facultative.
				//
				//  ⚠️ CE N EST PAS LA BARRE D ACTIVITE, ET LA MESURE LE DIT. Avant
				//     d ecrire ceci, la question « qui porte deja un rail lateral ? »
				//     a ete posee (porte du 28/08). Reponse : `DrawActivityBar` en
				//     porte un — mais ses huit icones sont un `switch (idx)` code en
				//     dur (Explorateur, Recherche, Controle de source...), sans
				//     libelle, sans infobulle, et sans tiroir. Elle bascule des vues
				//     ANCREES, c est-a-dire l etat 3. Ce qui a ete REPRIS, en
				//     revanche : `NkTooltip` (l infobulle des voyants du pied),
				//     `NkEditorPanel` (le tiroir dessine un panneau existant, il
				//     n en invente pas un second) et le voile de `theme.scrim`.
				struct NkEditorRailItem {
						/// Titre du panneau a deplier. ⚠️ C est une CLE : elle doit
						/// s ecrire a l identique ici et dans `NkEditorPanel(...)`.
						const char *panel = "";
						const char *tooltip = ""; ///< infobulle de l etat 1
						const char *glyphe = "";  ///< 1 a 2 lettres, faute d atlas
						// ── ADDITIF (costume Banani, 2026-08-31) — les défauts rendent
						//    le comportement historique, aucun consommateur ne bouge. ──
						/// Dessin de la pastille PAR L'APPLICATION (à la place du
						/// glyphe) : icône vectorielle, libellé, badge — l'app dessine
						/// tout le CONTENU dans `r` avec ses propres polices ; la
						/// coquille garde le fond d'état, l'infobulle et le clic.
						/// `actif` = tiroir déplié, `survol` = souris dessus.
						void (*icone)(nkgui::NkGuiContext &ui, const nkgui::NkRect &r, bool actif,
									  bool survol, void *user) = nullptr;
						void *iconeUser = nullptr;
						/// Largeur de la pastille (rail bas : une PILULE « icône +
						/// libellé » est plus large que 28). 0 = les 28 historiques.
						float32 largeur = 0.f;
				};
				static const int32 kRailMax = 8;

				/// Pose les pastilles d un rail. `side` : NK_LEFT, NK_RIGHT ou
				/// NK_BOTTOM. Un rail sans pastille n est pas dessine du tout — une
				/// bande vide de 28 px serait du chrome, exactement ce qu on vient
				/// de retirer.
				/// ⚠️ AU-DELA DE `kRailMax`, LE RAIL CRIE ET REFUSE le surplus : il
				///    ne le laisse pas tomber en silence.
				void SetRail(NkEditorDockSide side, const NkEditorRailItem *items, int32 count) noexcept;

				/// Ouvre (ou ferme, index -1) le TIROIR d'une pastille de rail par
				/// programme — mise en scène et raccourcis. Additif (2026-08-31) :
				/// même règle qu'un clic sur la pastille, une seule par rail.
				void OuvrirTiroir(NkEditorDockSide side, int32 index) noexcept {
					const int32 slot = side == NkEditorDockSide::NK_LEFT	? 0
									   : side == NkEditorDockSide::NK_RIGHT ? 1
									   : side == NkEditorDockSide::NK_BOTTOM ? 2
																			 : -1;
					if (slot >= 0 && index < mRailCount[slot])
						mRailOuvert[slot] = index;
				}

				void SetActivityBars(bool left, bool right) noexcept {
					mActivityBarLeft = left;
					mActivityBarRight = right;
				}

				// MASQUAGE DE L'INPUT DU CORPS quand un popup NKGui est sous la souris.
				// Vrai par defaut : c'est ce qui empeche les clics destines a un menu de
				// la BARRE DE TITRE de traverser vers l'editeur.
				//
				// A COUPER si vos PANNEAUX ouvrent eux-memes des popups (BeginCombo,
				// BeginMenu) : ceux-la sont dessines PENDANT les panneaux, donc avec
				// l'input deja masque -- le combo s'ouvre puis n'accepte plus aucun clic
				// et refuse de se refermer. NKGui resout deja l'occlusion de ses propres
				// popups ; en echange, gardez vos hit-tests « bruts » sous garde
				// `ctx.popupDepth == 0`.
				void SetMaskBodyOnPopup(bool mask) noexcept {
					mMaskBodyOnPopup = mask;
				}

				// Clic sur l'activity bar : l'app recoit l'index (0..6 = vues gauche, 100..102 = IA
				// droite, 999 = reglages) et decide (sidebar exclusive facon VSCode).
				void SetActivityHandler(void (*fn)(void *, int32), void *user) noexcept {
					mActivityFn = fn;
					mActivityUser = user;
				}

				// Icônes MARQUÉES des activity bars (l'app pousse chaque frame l'état RÉEL
				// des panneaux : disposition restaurée comprise). -1 = aucune.
				void SetActivityActive(int32 leftIdx, int32 rightIdx) noexcept {
					mActivityIndex = leftIdx;
					mActivityIndexRight = rightIdx;
				}

				// Drop de FICHIERS depuis l'OS : l'app reçoit chemins + position client.
				void SetDropFilesHandler(void (*fn)(void *, const NkVector<NkString> &, int32, int32),
										 void *user) noexcept {
					mDropFn = fn;
					mDropUser = user;
				}

				// Icônes TEXTURE des activity bars (teintées au rendu ; 0 = le dessin au
				// trait par défaut reste). left[0..6] = vues gauche, gear = réglages,
				// right[0..2] = IA droite.
				void SetActivityIcons(const uint32 *left, int32 nLeft, uint32 gear, const uint32 *right,
									  int32 nRight) noexcept {
					for (int32 i = 0; i < 8 && i < nLeft; ++i)
						mActTexL[i] = left[i];
					mActTexGear = gear;
					for (int32 i = 0; i < 4 && i < nRight; ++i)
						mActTexR[i] = right[i];
				}

				bool RegisterCommand(const char *name, NkEditorCommandFn fn, void *user = nullptr,
									 const char *shortcut = "") noexcept;

				void SetAppMenu(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mAppMenuFn = fn;
					mAppMenuUser = user;
				}

				// REMPLACE ENTIEREMENT les menus par defaut du shell (Fichier/Affichage/
				// Fenetre/Preferences) : l'app dessine TOUTE la barre (BeginMenu/MenuItem)
				// dans ce hook — appele entre BeginMenuBar et EndMenuBar, apres mAppMenuFn
				// (flags launcher). Si non pose, comportement historique conserve.
				void SetMenuBar(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mMenuBarFn = fn;
					mMenuBarUser = user;
				}

				// Items « un par panneau enregistre » (ouvrir/fermer via FocusPanel) — la
				// boucle du menu Affichage historique, exposee pour le menu app (Open View).
				void DrawPanelsMenuItems() noexcept;

				// Items injectes DANS le menu « Fichier » (avant Palette/Quitter) : l'app
				// y met Nouveau fichier/projet/workspace, Enregistrer(/sous/tout), Deploiement.
				void SetFileMenu(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mFileMenuFn = fn;
					mFileMenuUser = user;
				}

				// Barre d'outils horizontale (sous la barre de titre, facon Visual Studio) :
				// l'app y dessine config/plateforme cible + bouton Build/Run + emulateur.
				void SetToolbar(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mToolbarFn = fn;
					mToolbarUser = user;
				}

				// Overlay applicatif (dessine APRES les panneaux, sur dlOverlay) : l'app y
				// rend ses dialogues modaux (creation de projet, proprietes...). Quand
				// ctx.appModal est leve, le shell masque l'input du corps. L'input du popup
				// est restaure avant l'appel (comme la fenetre Preferences).
				void SetOverlay(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mOverlayFn = fn;
					mOverlayUser = user;
				}

				// Ecran de demarrage (launcher) : dessine TOUT le corps quand
				// ctx.appFullScreen est leve (remplace barre d'outils + panneaux).
				void SetStartScreen(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mStartScreenFn = fn;
					mStartScreenUser = user;
				}

				// Barre d'etat ENTIEREMENT dessinee par l'app (« a sa maniere ») :
				// REMPLACE le footer VSCode du shell — voyants (SetFooterLights),
				// textes gauche/droite (SetFooter) et indicateur de zoom ne sont PAS
				// dessines quand ce hook est pose. Le shell reserve toujours la bande
				// basse (jamais en plein-ecran launcher) et initialise la region de
				// layout sur son rect avant l'appel, comme pour la toolbar : widgets
				// + SameLine() utilisables, rect complet dans ctx.layout.region.
				// Couleurs : passer par mUI.theme, jamais de 0xRRGGBB (regle du kit).
				// Si non pose, comportement historique conserve. Patron SetMenuBar.
				void SetStatusBarFn(NkEditorAppMenuFn fn, void *user = nullptr) noexcept {
					mStatusBarFn = fn;
					mStatusBarUser = user;
				}

				// Maximise la fenetre (ex. au lancement, pour l'ecran de demarrage).
				void Maximize() noexcept {
					mWindow.Maximize();
				}

				// Donne une taille/position de fenetre (non maximisee).
				void Resize(uint32 w, uint32 h) noexcept {
					mWindow.SetSize(w, h);
				}

				float32 DpiScale() const noexcept {
					const float32 s = mUI.S(1.f);
					return s > 0.5f ? s : 1.f;
				} ///< echelle DPI (pour uploader les icones a la taille ecran)

				// Upload une image RGBA8 comme texture backend ; renvoie un texId stable
				// (0 si echec). Pour les logos/icones de l'app (dessines via AddImage).
				uint32 UploadRGBA(const uint8 *pixels, int32 w, int32 h) noexcept {
					if (!mRenderer || !pixels || w <= 0 || h <= 0)
						return 0;
					const uint32 id = mNextTexId++;
					return mRenderer->UploadImageRGBA(id, pixels, w, h) ? id : 0;
				}

				// Re-uploade des pixels RGBA8 dans une texture DEJA allouee (meme texId).
				// Pour le contenu qui change chaque frame (video : NkVideoReader -> onglet)
				// sans allouer une nouvelle texture a chaque image. false si texId==0.
				bool UpdateRGBA(uint32 texId, const uint8 *pixels, int32 w, int32 h) noexcept {
					if (!mRenderer || !texId || !pixels || w <= 0 || h <= 0)
						return false;
					return mRenderer->UploadImageRGBA(texId, pixels, w, h);
				}

				// Ouvre la palette de commandes (equivalent Ctrl+P) par programme
				// (ex. clic sur la barre de recherche de la toolbar NKCode).
				void OpenCommandPalette() noexcept {
					mPaletteOpen = true;
					mPaletteSel = 0;
				}

				// Logo dessine a gauche de la barre de titre (texId via UploadRGBA).
				// aspect > 0 : LOGO COMPLET (wordmark icone+nom) dessine en preservant son
				// ratio largeur/hauteur ; "nkcode" n'est alors PAS re-ecrit a cote (deja dans
				// l'image). aspect == 0 : logo carre (icone seule).
				void SetTitleLogo(uint32 texId, float32 aspect = 0.f) noexcept {
					mTitleLogoTex = texId;
					mTitleLogoAspect = aspect;
				}

				// ═══════════════════════════════════════════════════════════════
				//  COSTUME EXACT (remandat Banani, 2026-08-31) — TOUT EST ADDITIF
				// ═══════════════════════════════════════════════════════════════
				//  Chaque crochet a un défaut = comportement historique : NKCode et
				//  les autres consommateurs ne bougent pas d'un pixel tant qu'ils
				//  n'optent pas.

				/// Le bloc logo carré (SetHeaderLayout) dessiné PAR L'APPLICATION.
				/// Prime sur SetTitleLogo et sur le « O » Rihen par défaut — c'est le
				/// même patron que SetMenuBar : l'app fournit le dessin, la coquille
				/// fournit la place.
				void SetHeaderLogoFn(void (*fn)(nkgui::NkGuiContext &, const nkgui::NkRect &, void *),
									 void *user = nullptr) noexcept {
					mHeaderLogoFn = fn;
					mHeaderLogoUser = user;
				}

				/// Police dédiée à la BARRE DE TITRE (menus + nom de fichier). Posée,
				/// elle remplace ctx.font le temps de DrawTitleBar — les menus d'une
				/// maquette à 11 px cessent d'hériter du 12-16 px de l'interface.
				void SetTitleBarFont(nkgui::NkGuiFont *f) noexcept {
					mTitleBarFont = f;
				}

				/// Contrôles de fenêtre COMPACTS (Banani TopHeader) : réduire /
				/// agrandir sur fond `theme.button`, fermer sur FOND ROUGE permanent.
				/// faux = les trois zones larges historiques. `sizePx` : le côté du
				/// bouton (13 = la maquette ; Rodolf 31/08 : « trop petits » —
				/// l'application choisit, glyphes et zone cliquable suivent).
				void SetWindowControlsCompact(bool v, float32 sizePx = 13.f) noexcept {
					mWinControlsCompact = v;
					mWinControlsSize = (sizePx >= 8.f && sizePx <= 28.f) ? sizePx : 13.f;
				}

				/// Rail bas, à DROITE : pastille d'état colorée + texte (« Prêt »).
				/// Dessinés seulement si le texte est non vide.
				void SetRailFooterStatus(const char *texte, nkgui::NkColor pastille) noexcept {
					uint32 i = 0;
					for (; texte && texte[i] && i + 1 < (uint32)sizeof(mRailStatusText); ++i)
						mRailStatusText[i] = texte[i];
					mRailStatusText[i] = 0;
					mRailStatusColor = pastille;
				}

				/// Masque la barre d'onglets des panneaux LATÉRAUX même à plusieurs
				/// (les panneaux dessinent alors leur propre en-tête, patron Banani
				/// « Hiérarchie » / « Bouton_Connexion »). Le panneau central garde sa
				/// règle historique.
				void SetSideTabsVisible(bool v) noexcept {
					mSideTabsVisible = v;
				}

				/// Charge et téléverse une police D'APPLICATION (taille fixe d'une
				/// maquette). Les texIds mFont+16..+23 sont réservés à ces huit
				/// emplacements — distincts du code (+1/+8..+15) et du terminal (+2).
				/// La police doit déjà être chargée (LoadEmbedded/LoadFromFile) ;
				/// ce point d'entrée pose le texId et téléverse l'atlas.
				bool UploadAppFont(nkgui::NkGuiFont &font, uint32 slot) noexcept {
					if (slot >= 8u || !mRenderer || !font.Valid())
						return false;
					font.texId = mFont.TexId() + 16u + slot;
					return mRenderer->UploadFontGray8(font.TexId(), font.pixels, font.atlasW, font.atlasH);
				}

				/// Force la taille de la police d'interface (px logiques) SANS la
				/// persister : les réglages utilisateur sur disque ne bougent pas,
				/// et le choix ne survit pas à l'application qui ne le redemande pas.
				void ForceUiFontSize(float32 px) noexcept {
					if (px < 8.f || px > 40.f)
						return;
					mFontPrefs.uiSize = px;
					LoadUiFont();
				}

				// ── Layout ──────────────────────────────────────────────────────────
				void ResetLayout() noexcept {
					mDockBootstrap = true;
				}

				// Persistance de disposition : à recâbler sur la sérialisation d'arbre NKGui
				// (TODO) ; stubs no-op en attendant pour conserver l'API.
				void SaveLayout(const char *path) noexcept {
					(void)path;
				}

				bool LoadLayout(const char *path) noexcept {
					(void)path;
					return false;
				}

				// Etat d'interface PAR PROJET (lu/ecrit dans un fichier de config du
				// workspace, ex. <ws>/.nkcode/ui.cfg) : fenetre maximisee + panneaux
				// ouverts. LoadUiState applique l'etat ; no-op si le fichier est absent.
				void LoadUiState(const char *path) noexcept;
				void SaveUiState(const char *path) noexcept;

				// ── Barre d'etat (footer VSCode) : texte gauche/droite mis par l'app ─
				void SetFooter(const char *left, const char *right = "") noexcept;
				// VOYANTS du footer (pastilles colorees SANS texte, tout a gauche) : l'app
				// pousse chaque frame l'etat courant (ex. sante code/compilation/link).
				// `tips[i]` (optionnel) = libelle montre en INFOBULLE au survol seulement.
				void SetFooterLights(const nkgui::NkColor *colors, const char *const *tips, int32 count) noexcept {
					if (count > 8)
						count = 8;
					mFooterLightCount = count < 0 ? 0 : count;
					for (int32 i = 0; i < mFooterLightCount; ++i) {
						mFooterLights[i] = colors[i];
						mFooterLightTips[i] = (tips && tips[i]) ? nkentseu::NkString(tips[i]) : nkentseu::NkString();
					}
				}
				// ── Zoom de l'editeur (Ctrl+molette / Ctrl+± ) : ajuste la taille de la
				//    police du code (+ terminal), reconstruit l'atlas, persiste. ──────────
				void NudgeCodeFontSize(float32 delta) noexcept;
				void ResetCodeFontSize() noexcept;	   ///< remet la police du code à la taille par défaut (Ctrl+0)
				float32 CodeFontSize() const noexcept; ///< taille GLOBALE par défaut (prefs)
				static constexpr float32 kDefaultCodeFontSize = 15.f; ///< défaut (= NkFontPrefs::codeSize)
				static constexpr float32 kCodeReloadDebounce =
					0.12f; ///< délai (s) avant rebuild de l'atlas code après le dernier cran de zoom

				// ── Zoom PAR ONGLET (per-file) : l'app (qui possède les onglets) enregistre un
				//    handler ; Nudge/Reset le routent vers lui (delta, ou reset=true) au lieu
				//    d'agir sur la taille globale. L'app pilote ensuite l'atlas via RequestCodeSize.
				using NkZoomFn = void (*)(void *user, float32 delta, bool reset);

				void SetZoomHandler(NkZoomFn fn, void *user) noexcept {
					mZoomFn = fn;
					mZoomUser = user;
				}

				// CACHE d'atlas de code PAR TAILLE : l'editeur appelle EnsureCodeSize (arme la
				// rasterisation d'une taille) + CodeFontForSize (police a utiliser MAINTENANT, non
				// bloquant). Une taille deja rasterisee reste en cache -> revenir sur un onglet zoome
				// = atlas DEJA pret (aucun rebuild, aucun « saut »).
				void EnsureCodeSize(float32 logicalPx, bool immediate = false) noexcept;
				nkgui::NkGuiFont *CodeFontForSize(float32 logicalPx) noexcept;
				float32 ActiveCodeSize() const noexcept; ///< taille de code actuellement affichée (indicateur)
				// Police du TERMINAL : atlas PROPRE a taille GLOBALE fixe, decouple du zoom
				// par-onglet de l'editeur (le terminal ne bouge pas quand on zoome un fichier).
				nkgui::NkGuiFont *TermCodeFont() noexcept;
				void RequestTermSize(float32 logicalPx) noexcept; ///< zoom du TERMINAL (survol) : taille voulue (0 =
																  ///< globale), rebuild debounce
				float32 TermSize() const noexcept; ///< taille actuelle du terminal (pour calculer le zoom au survol)
				// ── Infos centrees dans la barre de titre (ex. fichier actif) ────────
				void SetTitleInfo(const char *center) noexcept;

				// ── Acces (pour besoins avances) ────────────────────────────────────
				nkgui::NkGuiContext &Ui() noexcept {
					return mUI;
				}

				// ── Preferences (menu dedie : Polices, Theme, ... extensible) ───────
				void OpenPreferences(int32 tab = 0) noexcept {
					mShowPrefs = true;
					mPrefsTab = tab;
					mPrefsJustOpened = true;
				}

			private:
				void LoadFontsFromPrefs() noexcept;	   ///< (re)charge mFont + mCodeFont
				void LoadUiFont() noexcept;			   ///< (re)charge SEULE la police d'interface
				void LoadCodeFont() noexcept;		   ///< (re)charge le repli mCodeFont (taille globale)
				void BuildCodeSlot(int32 px) noexcept; ///< rasterise une taille dans le cache d'atlas (LRU)
				void LoadTermFont() noexcept;		   ///< (re)charge la police du TERMINAL (taille globale fixe)
				void DrawPreferences(NkEditorFrameContext &ec) noexcept; ///< fenetre Preferences (categories)
				void BuildMenuBar(NkEditorFrameContext &ec, const nkgui::NkRect &rect) noexcept;
				void DrawHeaderLogo(NkEditorFrameContext &ec, const nkgui::NkRect &r) noexcept;
				void DrawTitleBar(NkEditorFrameContext &ec, const nkgui::NkRect &bar) noexcept;
				void DrawToolbar(NkEditorFrameContext &ec, const nkgui::NkRect &rect) noexcept;
				void HandleEdgeResize(float32 W, float32 H) noexcept;
				void DrawPanels(NkEditorFrameContext &ec) noexcept;
				void BootstrapDocking() noexcept;
				void DrawCommandPalette(NkEditorFrameContext &ec) noexcept;
				void ExecuteCommand(int32 index) noexcept;
				void HookEvents() noexcept;
				void MapEditKey(NkKey k, bool down) noexcept;
				void TryRunShortcut(NkKey k, bool shift) noexcept;
				void DrawActivityBar(const nkgui::NkRect &bar) noexcept;
				void DrawActivityBarRight(const nkgui::NkRect &bar) noexcept; ///< barre IA (cote droit)
				void DrawStatusBar(float32 footerH) noexcept;

				// === Redimensionnement / deplacement : hand-off NATIF (BeginResize/BeginDragMove).
				// Le hand-off bloque (boucle modale OS) -> on le DIFFERE en fin de boucle Run()
				// (jamais en plein milieu d'une frame, pour eviter la re-entrance de RenderFrame). ===
				void RenderFrame() noexcept; // rend UNE frame (appele par Run + le callback size/move)
				static void
				SizeMoveFrameThunk(void *user) noexcept; // callback OS : redessine pendant le resize (anti-stretch)
				int32 mPendingResizeEdge = -1;			 // -1 aucun ; sinon NkResizeEdge a declencher
				bool mPendingDragMove = false;			 // deplacement de barre de titre a declencher

				// === Fenetre / rendu ===
				NkWindow mWindow;
				NkIEditorRenderer *mRenderer = nullptr; // backend pluggable (NKCanvas par defaut / NKRHI injecte)
				uint32 mNextTexId = 0x4E4B0100u;		// ids de textures app (logos/icones) — distincts des polices
				uint32 mTitleLogoTex = 0;				// logo dans la barre de titre
				float32 mTitleLogoAspect = 0.f;			// >0 => wordmark (ratio l/h), pas de texte "nkcode"
				bool mOwnsRenderer = false;				// true => cree par le shell (a detruire)

				// === NKGui (contexte + police possedee) ===
				nkgui::NkGuiContext mUI;
				nkgui::NkGuiFont mFont;		   ///< police d'interface (Inter/Karla)
				nkgui::NkGuiFont mCodeFont;	   ///< police monospace de l'EDITEUR (taille = onglet actif, zoom)
				nkgui::NkGuiFont mTermFont;	   ///< police monospace du TERMINAL (taille GLOBALE fixe, non zoomee)
				float32 mTermLoadedSize = 0.f; ///< taille logique a laquelle mTermFont est construit
				float32 mTermTargetSize = 0.f; ///< taille logique voulue du terminal (0 = globale), zoom au survol
				float32 mTermReloadCountdown = -1.f; ///< debounce du rebuild de l'atlas terminal (zoom)
				bool mFontOk = false;
				bool mFontReloadPending = false; ///< reload des DEUX polices differe au debut de frame (anti-crash)
				float32 mCodeReloadCountdown =
					-1.f; ///< zoom : debounce (s). >=0 => rasterise la taille en attente quand il atteint 0
				float32 mCodeTargetSize = 0.f; ///< (init) taille logique de mCodeFont (repli global)
				float32 mCodeLoadedSize = 0.f; ///< taille logique a laquelle mCodeFont (repli) est construit
				float32 mCodeActiveLogical =
					0.f; ///< derniere taille demandee par l'editeur (indicateur de zoom ; 0 = globale)
				// Cache d'atlas de code par taille (px) : chaque taille rasterisee 1 seule fois.
				static constexpr int32 kCodeCacheN = 8;

				struct CodeSlot {
						nkgui::NkGuiFont *font = nullptr;
						int32 px = 0;
						uint32 lru = 0;
				};

				CodeSlot mCodeSlots[kCodeCacheN] = {};
				uint32 mCodeClock = 0;		///< horloge LRU du cache
				int32 mCodePendingPx = 0;	///< px a rasteriser quand le debounce expire
				NkZoomFn mZoomFn = nullptr; ///< handler zoom per-onglet (app)
				void *mZoomUser = nullptr;
				NkFontPrefs mFontPrefs;			   ///< reglages de polices (persistes)
				nkgui::NkGuiTheme mDefaultTheme;   ///< theme par defaut (vit dans l'app) -> Reinitialiser
				nkgui::NkGuiSyntax mDefaultSyntax; ///< couleurs langages par defaut -> Reinitialiser
				bool mShowPrefs = false;		   ///< fenetre Preferences ouverte ?
				bool mPrefsJustOpened = false;	   ///< grace 1 frame (anti auto-fermeture)
				int32 mPrefsTab = 0;			   ///< categorie active (0=Polices, 1=Theme)
				int32 mThemeSel = 6;			   ///< element de theme selectionne (defaut Accent)
				int32 mSynSel = 1;				   ///< token de langage selectionne (defaut Mot-cle)

				// === Panneaux / commandes ===
				NkEditorPanel *mPanels[MAX_PANELS] = {};
				int32 mNumPanels = 0;
				float32 mDockSavedRatio = -1.f; // ratio normal sauvegarde (restauration)
				int32 mDockRegionState = 0;		// 0 normal, 1 replie, 2 maximise (region du bas)
				void SetRegionMode(const char *title, int32 mode) noexcept;
				// Boutons maximiser/replier dessines par le SHELL sur la barre d'onglets d'une
				// region HAUT|BAS -> dispo pour TOUS les onglets (Terminal/Sortie/Problemes...).
				bool IsBottomRegionTab(nkgui::NkGuiContext &c, nkgui::NkGuiId win) noexcept;
				void DrawRegionButtons(nkgui::NkGuiContext &c, const nkgui::NkRect &area, nkgui::NkGuiId win) noexcept;
				NkEditorCommand mCommands[MAX_COMMANDS] = {};
				int32 mNumCommands = 0;
				NkEditorAppMenuFn mAppMenuFn = nullptr;
				void *mAppMenuUser = nullptr;
				float32 mHeaderTitleH = 0.f; // 0 = calcul historique
				float32 mHeaderBandH = 0.f;  // 0 = 46 historique
				float32 mHeaderLogo = 0.f;   // 0 = logo dans la barre, pas de bloc carre
				NkEditorAppMenuFn mMenuBarFn = nullptr; // barre COMPLETE fournie par l'app (SetMenuBar)
				void *mMenuBarUser = nullptr;
				NkEditorAppMenuFn mFileMenuFn = nullptr;
				void *mFileMenuUser = nullptr;
				NkEditorAppMenuFn mToolbarFn = nullptr;
				void *mToolbarUser = nullptr;
				NkEditorAppMenuFn mOverlayFn = nullptr;
				void *mOverlayUser = nullptr;
				NkEditorAppMenuFn mStartScreenFn = nullptr;
				void *mStartScreenUser = nullptr;
				NkEditorAppMenuFn mStatusBarFn = nullptr; // barre d'etat COMPLETE fournie par l'app (SetStatusBarFn)
				void *mStatusBarUser = nullptr;

				// === Palette de commandes ===
				bool mPaletteOpen = false;
				int32 mPaletteSel = 0;

				// === Barre de titre custom + footer + activity bar ===
				char mTitle[160] = {};
				char mTitleCenter[200] = {};
				bool mStatusBarVisible = true;
				/// Barre de defilement externe des panneaux ancres (voir
				/// SetDockScrollbarVisible) — vrai par defaut.
				bool mDockScrollbars = true;
				char mRailFooterText[256] = {};
				char mFooterLeft[256] = {};
				// Voyants du footer (cf. SetFooterLights)
				nkgui::NkColor mFooterLights[8] = {};
				nkentseu::NkString mFooterLightTips[8];
				int32 mFooterLightCount = 0;
				char mFooterRight[128] = {};
				int32 mActivityIndex = 0;					  // icone selectionnee dans l'activity bar
				int32 mActivityIndexRight = -1;				  // icone marquee de la barre DROITE (IA)
				// Rails de pastilles : 0 = gauche, 1 = droite, 2 = bas.
				NkEditorRailItem mRailItems[3][kRailMax] = {};
				int32 mRailCount[3] = {0, 0, 0};
				/// Index de la pastille DEPLIEE, -1 si aucune. Un entier, pas un
				/// ensemble : c est ce qui rend « une seule par rail » structurel.
				int32 mRailOuvert[3] = {-1, -1, -1};
				void DrawRail(int32 slot, const nkgui::NkRect &bar, bool vertical) noexcept;
				void DrawRailDrawers(NkEditorFrameContext &ec, const nkgui::NkRect &corps) noexcept;
				NkEditorPanel *TrouverPanneau(const char *titre) noexcept;

				// ── Costume exact (Banani 2026-08-31), cf. bloc public « COSTUME EXACT » ──
				void (*mHeaderLogoFn)(nkgui::NkGuiContext &, const nkgui::NkRect &, void *) = nullptr;
				void *mHeaderLogoUser = nullptr;
				nkgui::NkGuiFont *mTitleBarFont = nullptr; // police dédiée barre de titre (menus 11 px)
				bool mWinControlsCompact = false;		   // contrôles compacts, fermer rouge permanent
				float32 mWinControlsSize = 13.f;		   // côté du bouton compact (l'app choisit)
				bool mSideTabsVisible = true;			   // barres d'onglets des panneaux latéraux
				char mRailStatusText[64] = {};			   // « Prêt » à droite du rail bas
				nkgui::NkColor mRailStatusColor = {63, 185, 80, 255};

				bool mFooterZoom = true;					  // cf. SetFooterZoomIndicator
				bool mActivityBarLeft = true;				  // cf. SetActivityBars
				bool mActivityBarRight = true;
				bool mMaskBodyOnPopup = true;				  // cf. SetMaskBodyOnPopup
				uint32 mActTexL[8] = {};					  // textures vues gauche (0 = trait)
				uint32 mActTexR[4] = {};					  // textures IA droite
				uint32 mActTexGear = 0;						  // texture reglages
				// Géométrie fenêtre cachée chaque frame (valide à la sauvegarde post-Run).
				bool mGeomValid = false, mGeomMax = false;
				int32 mGeomX = 0, mGeomY = 0, mGeomW = 1440, mGeomH = 900;
				// Gestionnaire de menu contextuel (shell-level).
				bool mCtxOpen = false;
				nkgui::NkVec2 mCtxPos{0.f, 0.f};
				nkentseu::NkVector<nkentseu::NkString> mCtxItems;
				nkentseu::NkVector<nkentseu::uint8> mCtxEnabled;
				int32 mCtxChoice = -1;
				float32 mCtxSy = 0.f; // défilement vertical (listes longues)
				// ── Sous-menu (un seul item à sous-menu par menu) ──
				int32 mCtxSubItem = -1;	  ///< item parent (▸) ; -1 = pas de sous-menu
				int32 mCtxSubChoice = -1; ///< index choisi dans le sous-menu (avec mCtxChoice)
				nkentseu::NkVector<nkentseu::NkString> mCtxSubItems;
				nkentseu::NkVector<uint32> mCtxSubIcons; // icones du sous-menu (0 = aucune)
				char mCtxSubFilter[64] = {0};            // barre de recherche ancree du sous-menu
				bool mCtxSubFilterFocus = false;
				NkCtxMenu mCtxSub; ///< état du sous-menu (position/scroll, NkCtxMenuDraw)
				void DrawContextMenu() noexcept;
				// Sélecteur fichier/dossier générique (modal).
				void (*mActivityFn)(void *, int32) = nullptr; // handler app du clic activity bar
				void *mActivityUser = nullptr;
				void (*mDropFn)(void *, const NkVector<NkString> &, int32, int32) = nullptr; // drop OS
				void *mDropUser = nullptr;

				// === Etat boucle ===
				NkClock mClock;
				bool mRunning = true;
				// Rappel de fermeture explicite (croix) — cf. SetOnWindowClosed.
				bool mQuitIsWindowClose = true; ///< cf. RequestQuit/QuitIsWindowClose
				NkOnWindowClosed mOnWindowClosed = nullptr;
				void *mOnWindowClosedUser = nullptr;
				bool mDockBootstrap = true;
				bool mPopupMasked = false;	  ///< input du corps masque (souris sur un popup) - cf. dockHeaderFn
				nkgui::NkGuiInput mRealInput; ///< input REEL pendant le masquage (barres d onglets)
				NkEditorGfxApi mGraphicsApi = NkEditorGfxApi::Auto;
				uint32 mLastWidth = 0;
				uint32 mLastHeight = 0;
				// Deplacement de fenetre par la barre de titre : arme au press, ne demarre
				// qu'apres un vrai glissement (seuil) -> un simple clic ne deplace jamais.
				bool mTitleDragArmed = false;
				float32 mDragStartX = 0.f, mDragStartY = 0.f;
		};

	} // namespace editorkit
} // namespace nkentseu
