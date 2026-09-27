#pragma once
// -----------------------------------------------------------------------------
// @File    NkCoquilleDocument.h
// @Brief   NKUIDesign branche `NKGui/Doc/NkGuiCoquille.h` sur les crochets de
//          `NkEditorShell` : menu d'application et barre d'état décrits par des
//          documents `.nkgui`.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ORIGINE DE CETTE COPIE — À LIRE AVANT D'Y TOUCHER
// =============================================================================
//  COPIÉ DE : `Applications/NkAnimaEditor/src/NkAnimaEditor/NkCoquilleDocument.h`
//  LE       : 2026-09-27
//
//  Rodolf, le 27/09/2026 : « c'est pour ça que la copie du code est important :
//  on copie, on modifie après, doucement doucement — pareil pour NK3DModeler,
//  NKScena, Nogee, NkAnimaEditor. »
//
//  ⚠️ L'ORIGINE ET LA DATE SONT ÉCRITES PARCE QU'UNE COPIE SE PÉRIME EN SILENCE.
//     C'est la seule chose qu'une copie doive porter en plus de son code : sans
//     elle, un correctif posé dans un exemplaire est introuvable dans les autres,
//     et personne ne sait lequel est en avance. Avec elle, un `diff` contre
//     l'origine répond en une commande. Ce dépôt a payé deux fois pour l'avoir
//     oublié — un motif de retrait levé la veille par un autre atelier, et une
//     livraison par copie testée sur le mauvais binaire.
//
//  🔴 ET LA COPIE A DÉJÀ IMPORTÉ UNE AFFIRMATION PÉRIMÉE — corrigée ici, pas
//     propagée. L'exemplaire d'origine écrit : « Le format connaît `MenuBar`,
//     `Menu`, `MenuItem` et `ContextMenu` […] **le monteur n'en monte aucun**
//     (son énumération en compte 26, et ces quatre n'y sont pas). » C'était vrai
//     le 26/09. Ce ne l'est plus : le monteur monte `MenuBar`, `Menu` et
//     `MenuItem`, et `ContextMenu` depuis ce matin (son refus a été levé par le
//     crochet `MenuContextuelOuvre` qu'il réclamait). L'énumération en compte
//     désormais plus de quarante. **L'origine porte encore la phrase fausse.**
//
// =============================================================================
//  CE QUI EST BRANCHÉ ICI, ET CE QUI NE L'EST PAS
// =============================================================================
//  ⚠️ DEUX CROCHETS SEULEMENT, ET C'EST MESURÉ. `NKUIDesign/main.cpp` pose déjà
//     `SetMenuBar(&DrawMenuBar)` — 447 lignes qui portent AUSSI la récolte de la
//     génération IA (« le seul rappel que la coquille appelle à CHAQUE image sans
//     condition ») — et `SetToolbar(&DrawProjectTabs)`, qui énumère les projets
//     ouverts à l'exécution. Les remplacer par un document échangerait du code qui
//     marche contre une rangée plate ET casserait l'attente de la génération.
//     **On ne migre pas vers moins.**
//
//     Restent libres, mesurés dans `main.cpp` :
//       - `SetStatusBarFn`  : jamais posé — donc strictement ADDITIF ;
//       - `SetAppMenu`      : posé UNIQUEMENT sous un drapeau de sonde
//                             (`--capture`, `--sonde-gel`, `--sonde-portes`,
//                             les mesures) ; libre en marche normale. Et il est
//                             **additif** : la coquille l'appelle DANS sa propre
//                             barre, après ses menus (`NkEditorShell.cpp:3497`),
//                             au contraire de `SetMenuBar` qui REMPLACE.
//
//  C'est le « doucement doucement » : deux bandes que le document gagne sans que
//  rien ne soit retiré. La suite (la barre d'outils, puis les panneaux de
//  `Panels.h` — 21 052 lignes) se fera document par document.
//
// =============================================================================
//  LA FRONTIÈRE
// =============================================================================
//  Le document porte la STRUCTURE, les RÔLES, les LIBELLÉS, les ANCRAGES, les
//  ÉTATS d'apparence et le NOM de l'action. L'application garde **ce que l'action
//  FAIT** — ici les `Cmd*` que `RegisterCommand` sert déjà.
//
//  ⚠️ L'IDENTIFIANT DU BOUTON EST LE NOM DE L'ACTION, et c'est une convention de
//     CET hôte, pas une extension du format. La grammaire n'a pas de
//     `on Pressed -> ...` ; `NkBandeDocument` dérive l'appui et cherche le nom
//     dans la table. **Condition de retrait :** le jour où le format porte un
//     événement, la table de `main.cpp` reste et la dérivation disparaît.
//
//  ⚠️ ET LE KIT NE SAIT PAS LANCER UNE COMMANDE PAR SON NOM. `NkEditorShell`
//     n'expose que `ExecuteCommand(int32 index)` — *un indice n'est pas un nom* :
//     il se décale dès qu'une commande est insérée. La table d'actions vit donc
//     dans `main.cpp`, où les `Cmd*` sont visibles, et pointe droit sur elles.
//     Toucher le kit pour lui ajouter un « lancer par nom » appartient à l'agent
//     qui le tient.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKGui/Doc/NkGuiCoquille.h" // la bande partagee (tire le monteur)

namespace nkuidesign {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;
	using nkentseu::NkString;
	using nkentseu::NkStringView;
	using nkentseu::editorkit::NkEditorFrameContext;
	using nkgui::NkActionFn;
	using nkgui::NkActionNommee;
	using nkgui::NkBandeDocument;
	using nkgui::NkZoneFn;
	using nkgui::NkZoneNommee;

	/// Les deux bandes que NKUIDesign décrit par un document, et le dossier qui les
	/// porte.
	///
	/// ⚠️ DEUX DOCUMENTS ET NON UN, ET C'EST UNE CONSÉQUENCE DU FORMAT. `Monter`
	///    monte TOUTES les sections `widgets` d'un document au curseur courant, et
	///    il n'existe aucune API pour ne monter qu'un sous-arbre nommé
	///    (`MonterCorps` est privée). Or le menu et la barre d'état sont deux
	///    RÉGIONS que la coquille pose à deux instants différents. **Il manque
	///    `Monter(ctx, doc, "nom_de_racine")`** — c'est le même manque que
	///    l'exemplaire d'origine a nommé, et il n'a pas bougé.
	class NkCoquilleDocument {
		public:
			NkBandeDocument menuApp;
			NkBandeDocument barreEtat;

			/// Le dossier qui porte les documents. Tout est relatif à lui.
			NkString dossier;

			/// Lit les deux documents. Rend faux si l'un manque — et `RefusTotal` dit
			/// LEQUEL, parce qu'un « ça n'a pas chargé » sans nom se cherche à la main.
			bool ChargerDepuisDossier(const char *dossierDocuments) noexcept {
				dossier = NkString(dossierDocuments);
				const bool a = menuApp.ChargerDepuisFichier(Joindre("menu_design.nkgui").CStr());
				const bool b = barreEtat.ChargerDepuisFichier(Joindre("barre_etat.nkgui").CStr());
				return a && b;
			}

			/// Pose les mêmes tables sur les deux bandes.
			void PoserTables(const NkActionNommee *act, uint32 nAct, const NkZoneNommee *zon,
							 uint32 nZon) noexcept {
				NkBandeDocument *b[2] = {&menuApp, &barreEtat};
				for (uint32 i = 0; i < 2u; ++i) {
					b[i]->actions = act;
					b[i]->nbActions = nAct;
					b[i]->zones = zon;
					b[i]->nbZones = nZon;
				}
			}

			// ── les crochets, de la signature exacte que le kit attend ────
			/// ⚠️ LA BARRE EST DÉJÀ OUVERTE quand la coquille appelle ce crochet : le
			///    document ne doit porter que des `Menu`, jamais un `MenuBar`.
			static void MonterMenuApp(NkEditorFrameContext &ec, void *user) noexcept {
				((NkCoquilleDocument *)user)->menuApp.Monter(ec.Ui());
			}
			static void MonterBarreEtat(NkEditorFrameContext &ec, void *user) noexcept {
				((NkCoquilleDocument *)user)->barreEtat.Monter(ec.Ui());
			}

			uint32 MontesTotal() const noexcept {
				return menuApp.rap.montes + barreEtat.rap.montes;
			}
			uint32 RefusTotal() const noexcept {
				return (menuApp.lu ? 0u : 1u) + (barreEtat.lu ? 0u : 1u);
			}
			uint32 ActionsInconnuesTotal() const noexcept {
				return menuApp.actionsInconnues + barreEtat.actionsInconnues;
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

} // namespace nkuidesign

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
