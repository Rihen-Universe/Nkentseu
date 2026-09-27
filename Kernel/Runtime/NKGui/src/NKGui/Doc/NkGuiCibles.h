#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiCibles.h
// @Brief   P27 — LA CIBLE : un même document porte le design de plusieurs plateformes.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA DEMANDE
// =============================================================================
//  Rodolf, le 27/09/2026 : « le système normalement doit pouvoir avoir des design
//  spécifiques par plateforme — par exemple on peut conditionnellement intégrer des
//  designs pour web, mobile et PC, vu que Nkentseu est multiplateforme […] et comme
//  en plus le responsive est défini, ça pourrait encore apporter plus au système. »
//
// =============================================================================
//  🔴 CE QUI EST FAIT AUTREMENT QUE DEMANDÉ, ET POURQUOI
// =============================================================================
//  La demande dit « le système compile conditionnellement en fonction de la
//  plateforme ». Un `.nkgui` se lit à l'EXÉCUTION : il n'y a rien à compiler. Et
//  c'est une chance, pas une limite.
//
//  ⚠️ UNE SÉLECTION À LA COMPILATION AURAIT RENDU L'APERÇU IMPOSSIBLE. NKUIDesign
//     doit pouvoir MONTRER le design mobile depuis un PC — sinon il faut recompiler
//     pour voir ce qu'on dessine, et un éditeur d'interface qui ne montre pas
//     l'interface n'est pas un éditeur. En choisissant à l'exécution on obtient tout
//     ce que la demande veut, PLUS l'aperçu, et le coût est nul : la branche qui ne
//     correspond pas n'est simplement pas montée.
//
//  D'où la cible COURANTE, qui a deux sources et dans cet ordre : la plateforme
//  compilée (le défaut, juste sans que personne n'ait rien à dire) et l'hôte, qui
//  peut la remplacer (l'aperçu). Le défaut vient de `NkPlatform.h`.
//
// =============================================================================
//  ET LA TAXONOMIE N'EST PAS INVENTÉE ICI
// =============================================================================
//  ⚠️ `NkPlatform.h` NOMME DÉJÀ LES CINQ FAMILLES : `NkIsDesktop`, `NkIsMobile`,
//     `NkIsConsole`, `NkIsEmbedded`, `NkIsWeb`, chacune adossée à une macro
//     (`NKENTSEU_PLATFORM_DESKTOP`…). En écrire une sixième liste aurait fait deux
//     vérités sur « qu'est-ce qu'une plateforme », et le jour où elles divergent
//     personne ne sait laquelle le moteur respecte. Les noms du FORMAT sont les
//     seules choses que ce fichier ajoute, parce qu'un document s'écrit en mots.
//
// =============================================================================
//  UNE SEULE PORTE : L'ATTRIBUT `platform`
// =============================================================================
//  Le format n'apprend pas un rôle, ni une section, ni un qualificateur de bloc : il
//  apprend UN ATTRIBUT TRANSVERSE, admis partout où un attribut est déjà lu.
//
//      Panel "tiroir"  { platform = Mobile        ... }   // mobile seulement
//      HBox  "barre"   { platform = [Bureau, Web] ... }   // deux cibles
//      Panel "detail"  { ... }                            // partout (le défaut)
//
//      layout "defaut" { platform = Mobile
//        dock "outils" bottom 0.3
//      }
//
//  ⚠️ POURQUOI PAS UN QUALIFICATEUR `Panel(Mobile) "tiroir"`, sur le modèle
//     d'`appearance(Hover)`. Parce qu'un état d'apparence QUALIFIE un bloc de style
//     d'un même widget, alors qu'une cible DÉCIDE SI LE WIDGET EXISTE. Le premier se
//     résout en choisissant des couleurs, le second en montant ou pas un sous-arbre
//     entier. Leur donner la même graphie aurait fait croire qu'ils se combinent de
//     la même façon — et `appearance(Hover)` sur un widget écarté par la plateforme
//     n'a aucun sens à écrire.
//
//  ⚠️ ET UN ATTRIBUT, C'EST CE QUI LE FAIT MARCHER SUR LES CONTENEURS SANS RIEN
//     AJOUTER. `platform = Mobile` sur une `VBox` écarte la boîte ET sa descendance,
//     parce que le monteur ne descend pas dans ce qu'il ne monte pas. Un
//     qualificateur de rôle aurait demandé d'écrire la règle pour chaque rôle.
//
// =============================================================================
//  LE SILENCE EST COMPTÉ — ET ICI IL LE FAUT PLUS QU'AILLEURS
// =============================================================================
//  ⚠️ UN DOCUMENT PEUT DEVENIR VIDE SANS ÊTRE FAUX. Écrire `platform = Mobile`
//     partout donne, sur un PC, une fenêtre parfaitement vide — et aucun message
//     d'erreur, parce que rien n'est invalide. C'est le pire des silences : un
//     document juste qui n'affiche rien. `ecartesParPlateforme` est donc COMPTÉ dans
//     le rapport et doit apparaître dans le verdict des sondes, à côté de
//     `invisibles` : sans ce chiffre, « mon interface a disparu » se cherche à la
//     main.
//
//  ⚠️ ET UN NOM DE CIBLE INCONNU N'ÉCARTE PAS. `platform = Mobil` (faute de frappe)
//     ne doit pas faire disparaître un panneau en silence : le lexème inconnu est
//     COMPTÉ (`ciblesInconnues`) et le widget est MONTÉ. Écarter sur une faute de
//     frappe aurait donné un document qui perd un panneau sans que rien ne le dise —
//     c'est la politique inverse de celle d'un RÔLE inconnu, qui fait refuser le
//     document entier, et la différence se justifie : un rôle inconnu rend le
//     document inmontable, une cible inconnue laisse un document parfaitement
//     montable dont une intention est perdue.
//
//  Le responsif reste ce qu'il est : `sizeRel`, `minSize`, `maxSize` adaptent la
//  TAILLE à la fenêtre ; `platform` choisit la STRUCTURE. Les deux se composent —
//  une `VBox` mobile dont les enfants font `sizeRel = (1, 0)`.
// -----------------------------------------------------------------------------

#ifndef NK_GUI_DOC_NKGUICIBLES_H
#define NK_GUI_DOC_NKGUICIBLES_H

#include "NKContainers/String/NkString.h"
#include "NKCore/NkPlatform.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nkgui {

		/// Les familles de plateformes que le FORMAT nomme. Elles sont le miroir des
		/// prédicats de `NkPlatform.h`, jamais une seconde liste.
		enum class NkGuiCible : uint8 {
			Toutes = 0, ///< aucun `platform` écrit : le document vaut partout
			Bureau,		///< `NkIsDesktop` — Windows, Linux, macOS, BSD
			Mobile,		///< `NkIsMobile` — Android, iOS
			Web,		///< `NkIsWeb` — Emscripten
			Console,	///< `NkIsConsole`
			Embarque	///< `NkIsEmbedded`
		};

		/// Le nom d'une cible, pour les traces et les relevés.
		///
		/// ⚠️ JAMAIS POUR LA LECTURE. La lecture se fait par comparaison explicite
		///    (`NkGuiCibleDepuisNom`), pas en parcourant cette table à l'envers : une
		///    table employée dans les deux sens finit par accepter ce qu'elle ne
		///    devrait pas, et l'erreur ne se voit que sur une faute de frappe.
		inline const char *NkGuiNomCible(NkGuiCible c) noexcept {
			switch (c) {
				case NkGuiCible::Bureau: return "Bureau";
				case NkGuiCible::Mobile: return "Mobile";
				case NkGuiCible::Web: return "Web";
				case NkGuiCible::Console: return "Console";
				case NkGuiCible::Embarque: return "Embarque";
				default: return "Toutes";
			}
		}

		/// Lit un nom de cible. Rend faux sur un lexème inconnu — et l'appelant le
		/// COMPTE au lieu d'écarter le widget.
		///
		/// ⚠️ DEUX GRAPHIES POUR LE BUREAU (`Bureau` et `Desktop`) ET C'EST DÉLIBÉRÉ.
		///    Le dépôt écrit ses interfaces en français, et ses macros en anglais
		///    (`NKENTSEU_PLATFORM_DESKTOP`). Refuser l'une des deux aurait fait échouer
		///    un document sur un mot que le moteur emploie lui-même. `PC` est admis
		///    pour la même raison : c'est le mot de la demande.
		inline bool NkGuiCibleDepuisNom(const char *n, uint32 taille, NkGuiCible &out) noexcept {
			if (!n || taille == 0u)
				return false;
			struct Paire {
					const char *mot;
					NkGuiCible cible;
			};
			static const Paire kTable[] = {
				{"Toutes", NkGuiCible::Toutes},	  {"All", NkGuiCible::Toutes},
				{"Bureau", NkGuiCible::Bureau},	  {"Desktop", NkGuiCible::Bureau},
				{"PC", NkGuiCible::Bureau},		  {"Mobile", NkGuiCible::Mobile},
				{"Web", NkGuiCible::Web},		  {"Console", NkGuiCible::Console},
				{"Embarque", NkGuiCible::Embarque}, {"Embedded", NkGuiCible::Embarque},
			};
			for (uint32 k = 0; k < (uint32)(sizeof(kTable) / sizeof(kTable[0])); ++k) {
				const char *m = kTable[k].mot;
				uint32 i = 0;
				for (; i < taille; ++i) {
					if (m[i] == '\0' || m[i] != n[i])
						break;
				}
				if (i == taille && m[i] == '\0') {
					out = kTable[k].cible;
					return true;
				}
			}
			return false;
		}

		/// La cible que la COMPILATION donne. C'est le défaut, et il est juste sans
		/// que personne n'ait rien à déclarer.
		inline NkGuiCible NkGuiCibleCompilee() noexcept {
			if (NkIsWeb())
				return NkGuiCible::Web;
			if (NkIsMobile())
				return NkGuiCible::Mobile;
			if (NkIsConsole())
				return NkGuiCible::Console;
			if (NkIsEmbedded())
				return NkGuiCible::Embarque;
			if (NkIsDesktop())
				return NkGuiCible::Bureau;
			// ⚠️ PAS DE REPLI SILENCIEUX SUR `Bureau`. Une plateforme que
			//    `NkPlatform.h` ne classe dans aucune famille ne doit pas se faire
			//    passer pour un PC : `Toutes` laisse TOUT monter, ce qui est le
			//    comportement d'avant ce fichier, donc sans régression.
			return NkGuiCible::Toutes;
		}

		/// La cible COURANTE, et sa surcharge par l'hôte (l'aperçu).
		///
		/// ⚠️ MÊME FORME QUE LE REGISTRE DES JETONS, ET POUR LA MÊME RAISON : la cible
		///    se lit au fond du monteur, dans un `case` qui ne reçoit qu'un bloc
		///    d'archive. La faire descendre en paramètre aurait changé la signature de
		///    tout le monteur pour une valeur globale à l'image.
		NkGuiCible NkGuiCibleCourante() noexcept;

		/// Remplace la cible courante — c'est l'aperçu de NKUIDesign. `Toutes` remet
		/// le défaut compilé.
		void NkGuiPoserCible(NkGuiCible c) noexcept;

	} // namespace nkgui
} // namespace nkentseu

#include "NKGui/Doc/NkGuiCibles.inl"

#endif // NK_GUI_DOC_NKGUICIBLES_H

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
