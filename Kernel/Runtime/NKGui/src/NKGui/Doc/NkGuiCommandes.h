#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiCommandes.h
// @Brief   LE DOCUMENT EST LA SOURCE DU LIBELLÉ ET DU RACCOURCI d'une commande ;
//          le C++ ne garde que la FONCTION.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE
// =============================================================================
//  Rodolf, 28/09, devant `RegisterCommand("Vue: Hiérarchie", &CmdVueHierarchie,
//  nullptr, "Ctrl+J")` : « je croyais qu'on passait par des fichiers .nkgui ?
//  mais peut-être je confonds les choses. »
//
//  Il ne confondait rien. Cette ligne colle TROIS choses de nature différente :
//
//    `&CmdVueHierarchie`   le pointeur de fonction   → C++ (c'est le Contrôleur)
//    `"Vue: Hiérarchie"`   le libellé affiché        → DOCUMENT (texte d'interface)
//    `"Ctrl+J"`            le raccourci              → DOCUMENT (donnée)
//
//  Les deux dernières étaient DÉJÀ écrites dans le `.nkgui` — en double, donc.
//
// =============================================================================
//  🔴 ET LES DEUX VÉRITÉS AVAIENT DÉJÀ DIVERGÉ, C'EST MESURÉ
// =============================================================================
//  Relevé du 28/09 sur NKUIDesign, avant ce fichier :
//
//    `Ctrl+L`      le document affichait « Verrouiller »   → le C++ ouvrait l'INSPECTEUR
//    `Ctrl+R`      le document n'affichait RIEN            → le C++ rechargeait le document
//    `Ctrl+Maj+Z`  le document n'affichait RIEN            → le C++ rétablissait
//
//  Un raccourci écrit à deux endroits n'est pas « redondant » : c'est une
//  promesse faite à l'utilisateur d'un côté, et un câblage de l'autre, sans
//  personne pour les tenir d'accord.
//
// =============================================================================
//  ⚠️ LA NORMALISATION `Maj+` → `Shift+` N'EST PAS UNE COQUETTERIE
// =============================================================================
//  `NkEditorShell::TryRunShortcut` construit le combo qu'il cherche : « Ctrl+ »
//  (+ « Shift+ ») + UN caractère tiré d'un nom de touche de la forme exacte
//  `NK_X`. Un `Ctrl+Maj+G` écrit en français dans un document ne peut donc
//  JAMAIS correspondre — il s'afficherait et ne partirait pas.
//
//  L'interface d'un produit francophone porte « Maj » ; le comparateur épelle
//  « Shift ». La traduction se fait ICI, en un seul site, et elle est mesurée.
//
// =============================================================================
//  ⚠️ CE QUI EST REFUSÉ, ET POURQUOI C'EST LE CŒUR
// =============================================================================
//  `NkGuiRaccourciPeutPartir` rejette tout ce que le comparateur ne pourra pas
//  reconnaître : `F1`, `Maj+1`, `Ctrl+,`, `Échap`, `Suppr`, `Ctrl+Tab`… Ces
//  entrées reçoivent une commande SANS raccourci plutôt qu'avec un raccourci
//  décoratif.
//
//  🔴 PARCE QU'UN RACCOURCI AFFICHÉ QUI NE PART PAS EST UN PARAMÈTRE DÉCLARÉ
//     QUI N'EST PAS HONORÉ, et ce dépôt l'a déjà payé : un `Ctrl+1` s'affichait
//     à côté d'une commande et ne se déclenchait jamais. Le compteur
//     `raccourcisRejetes` les NOMME au lieu de les taire — un repli muet serait
//     exactement la faute qu'on ferme.
//
// =============================================================================
//  ⚠️ `lierRaccourci = false` : LE DOCUMENT AFFICHE, QUELQU'UN D'AUTRE LIE
// =============================================================================
//  Certains gestes ont UNE SEULE porte, et ce n'est pas la table de commandes :
//  `Ctrl+D`, `Ctrl+G`, `Ctrl+Maj+G` appartiennent à la TOILE, qui seule connaît
//  le mode d'édition, le popup ouvert et le renommage en cours.
//
//  🔴 LES DÉCLARER DEUX FOIS A DÉJÀ COÛTÉ UN DÉFAUT RÉEL (NKUIDesign, 05/09) :
//     une pression donnait DEUX copies de l'objet. La clé `lierRaccourci = false`
//     dit « affiche-le, ne le lie pas » — l'entrée garde son raccourci à l'œil,
//     et la toile garde sa porte unique.
// -----------------------------------------------------------------------------

#include "NKContainers/String/NkString.h"
#include "NKGui/Doc/NkGuiCoquille.h"

namespace nkentseu {
	namespace nkgui {

		/// Une commande telle que le DOCUMENT la décrit, prête à être enregistrée.
		struct NkGuiCommandeDite {
				NkString id;		  ///< l'identifiant du widget dans le document
				NkString libelle;	  ///< le texte affiché, `@t:` déjà résolu
				NkString raccourci;	  ///< vide si aucun, ou si le comparateur ne peut le lire
				NkActionFn fn = nullptr;
				void *user = nullptr;
		};

		/// Ce que le comparateur de la coquille sait reconnaître : `Ctrl+X` ou
		/// `Ctrl+Shift+X`, X d'UN caractère. Tout le reste s'affiche et ne part pas.
		///
		/// ⚠️ ELLE LIT LA FORME **APRÈS** NORMALISATION : appelée sur « Ctrl+Maj+G »
		///    elle répondrait faux, ce qui est juste — c'est bien ce que le
		///    comparateur ferait.
		inline bool NkGuiRaccourciPeutPartir(const char *s) noexcept {
			if (!s || !s[0])
				return false;
			const char *p = s;
			auto avale = [&p](const char *mot) -> bool {
				const char *q = p;
				while (*mot && *q == *mot) {
					++q;
					++mot;
				}
				if (*mot)
					return false;
				p = q;
				return true;
			};
			if (!avale("Ctrl+"))
				return false;
			(void)avale("Shift+");
			// Il doit rester EXACTEMENT un caractère, et ce doit être une lettre :
			// `NkKeyToString` ne rend `NK_X` d'un seul caractère que pour celles-là.
			if (!p[0] || p[1])
				return false;
			return (p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z');
		}

		/// « Maj » → « Shift », en un seul site. Rend la chaîne telle quelle si elle
		/// n'en contient pas.
		inline NkString NkGuiRaccourciNormalise(const NkString &s) noexcept {
			const char *d = s.Data();
			if (!d)
				return NkString();
			NkString out;
			for (const char *p = d; *p;) {
				if (p[0] == 'M' && p[1] == 'a' && p[2] == 'j' && p[3] == '+') {
					out.Append(NkString("Shift+"));
					p += 4;
					continue;
				}
				const char c[2] = {*p, '\0'};
				out.Append(NkString(c));
				++p;
			}
			return out;
		}

		/// Ce que l'énumération a vu — pour qu'un banc puisse juger, et pour qu'un
		/// rejet ne soit jamais silencieux.
		struct NkGuiCommandesRapport {
				uint32 vues = 0u;			   ///< widgets porteurs d'un identifiant servi
				uint32 retenues = 0u;		   ///< commandes produites
				uint32 grisees = 0u;		   ///< `enabled = false` — rien à lancer
				uint32 nonServies = 0u;		   ///< aucun nom dans la table d'actions
				uint32 raccourcisLies = 0u;	   ///< raccourcis réellement posés
				uint32 raccourcisRejetes = 0u; ///< écrits mais illisibles par le comparateur
				uint32 raccourcisNonLies = 0u; ///< `lierRaccourci = false` — une autre porte
				NkString premierRejete;		   ///< pour le NOMMER, pas seulement le compter
		};

		namespace detail {

			inline void NkGuiParcourirCommandes(const NkArchive &bloc,
												const NkActionNommee *actions, uint32 nActions,
												NkGuiCommandeDite *out, uint32 max, uint32 &n,
												NkGuiCommandesRapport &rap) noexcept {
				const NkArchiveNode *corps = NkGMonteCorps(bloc);
				if (!corps)
					return;
				for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
					if (!corps->array[i].IsObject() || !corps->array[i].object)
						continue;
					const NkArchive &w = *corps->array[i].object;
					const NkStringView type = NkGuiArchive::TypeOf(w);
					// Récursif : une entrée vit dans un menu, un bouton dans une boîte.
					NkGuiParcourirCommandes(w, actions, nActions, out, max, n, rap);
					const bool estEntree = NkGMotEgal(type, "MenuItem");
					if (!estEntree)
						continue;
					const NkString id(NkGuiArchive::IdOf(w));
					if (id.Size() == 0u)
						continue;
					++rap.vues;
					// ⚠️ UNE ENTRÉE GRISÉE NE DEVIENT PAS UNE COMMANDE. Elle
					//    s'affiche (§5bis.1 : ça existe, pas encore ici), mais une
					//    commande dans la palette est une promesse d'exécution.
					bool actif = true;
					(void)w.GetBool(NkStringView("enabled"), actif);
					if (!actif) {
						++rap.grisees;
						continue;
					}
					const NkActionNommee *act = nullptr;
					for (uint32 a = 0; a < nActions; ++a)
						if (actions[a].nom && id.Compare(NkString(actions[a].nom)) == 0) {
							act = &actions[a];
							break;
						}
					if (!act || !act->fn) {
						++rap.nonServies;
						continue;
					}
					if (n >= max)
						continue;
					NkGuiCommandeDite &c = out[n];
					c.id = id;
					c.libelle = NkGLibelle(w, id);
					c.fn = act->fn;
					c.user = act->user;
					NkString brut;
					(void)w.GetString(NkStringView("shortcut"), brut);
					bool lier = true;
					(void)w.GetBool(NkStringView("lierRaccourci"), lier);
					if (brut.Size() == 0u) {
						// rien d'écrit : rien à lier, et rien à signaler
					} else if (!lier) {
						++rap.raccourcisNonLies;
					} else {
						const NkString norm = NkGuiRaccourciNormalise(brut);
						if (NkGuiRaccourciPeutPartir(norm.CStr())) {
							c.raccourci = norm;
							++rap.raccourcisLies;
						} else {
							++rap.raccourcisRejetes;
							if (rap.premierRejete.Size() == 0u) {
								rap.premierRejete = id;
								rap.premierRejete.Append(NkString(" ("));
								rap.premierRejete.Append(brut);
								rap.premierRejete.Append(NkString(")"));
							}
						}
					}
					++n;
					++rap.retenues;
				}
			}

		} // namespace detail

		/// Énumère les commandes que le DOCUMENT décrit : une par entrée de menu
		/// active dont l'identifiant est servi par la table d'actions.
		///
		/// Rend le nombre écrit dans `out` (au plus `max`). `rap` dit ce qui a été
		/// écarté et pourquoi — un écart sans motif serait un repli muet.
		///
		/// ⚠️ ELLE NE TOUCHE À AUCUNE COQUILLE. NKGui est SOUS NKEditorKit dans le
		///    graphe : c'est l'appelant qui transforme chaque ligne en commande de
		///    son hôte. La partie difficile (parcours, `@t:`, normalisation,
		///    filtres) est ici ; il reste cinq lignes à l'application.
		inline uint32 NkGuiEnumererCommandes(const NkArchive &doc, const NkActionNommee *actions,
											 uint32 nActions, NkGuiCommandeDite *out, uint32 max,
											 NkGuiCommandesRapport &rap) noexcept {
			rap = NkGuiCommandesRapport();
			uint32 n = 0u;
			if (!out || max == 0u || !actions || nActions == 0u)
				return 0u;
			const NkArchiveNode *corps = NkGMonteCorps(doc);
			if (!corps)
				return 0u;
			// Les sections de premier niveau (`widgets "menu"`, `widgets "panneau"`…)
			// puis, sous chacune, l'arbre entier.
			for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
				if (!corps->array[i].IsObject() || !corps->array[i].object)
					continue;
				const NkArchive &sec = *corps->array[i].object;
				if (!NkGMotEgal(NkGuiArchive::TypeOf(sec), "widgets"))
					continue;
				detail::NkGuiParcourirCommandes(sec, actions, nActions, out, max, n, rap);
			}
			return n;
		}

	} // namespace nkgui
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
