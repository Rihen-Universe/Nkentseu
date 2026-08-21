#pragma once
// -----------------------------------------------------------------------------
// @File    NkComponentCheck.h
// @Brief   LES VERIFICATIONS DE LA FORME -- une seule porte d'entree.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI UN FICHIER RIEN QUE POUR CA
// =============================================================================
//  Une forme de declaration a plusieurs mains n'a pas besoin de regles : elle a
//  besoin de regles VERIFIABLES. Les blocs d'avertissement en tete des autres
//  fichiers disent ce qu'il ne faut pas faire ; celui-ci le MESURE. C'est la
//  difference entre « un parent doit apparaitre plus haut » et « un parent
//  apparait plus haut, verifie sur les deux composants declares ».
//
//  Et il y a une raison d'organisation, qui vaut pour quelqu'un qui arrive :
//  **on sait ou chercher.** Une verification eparpillee dans cinq en-tetes ne se
//  trouve pas ; une porte unique se trouve. Si vous ajoutez une regle a la
//  forme, sa verification s'ecrit ICI, dans `NkCheckComponent`, et nulle part
//  ailleurs.
//
// =============================================================================
//  DEUX SEVERITES, ET LA DISTINCTION EST LE COEUR DU FICHIER
// =============================================================================
//  ERREUR : la declaration est FAUSSE. Un parent qui n'existe pas, une metrique
//           nommee qui n'est declaree nulle part, un role annonce et non
//           honore. Le composant ne peut pas se resoudre correctement, et le
//           taire produirait le pire des defauts : celui qui compile.
//
//  NOTE   : la declaration est valide mais DIVERGE d'une convention. Le seul cas
//           reel aujourd'hui : un evenement qui porte le bon fait sous un autre
//           nom que celui du role. Une note ne rougit rien -- elle SE COMPTE, et
//           un compte de notes qui monte est le signal qu'une convention est en
//           train de se perdre.
//
//  ⚠️ POURQUOI PAS TOUT EN ERREUR : parce que la premiere ecriture du controle de
//     role rendait rouge le seul composant deja livre, et que le rendre vert
//     aurait exige de renommer un champ C++ appartenant a un autre agent. Une
//     verification qui force a modifier le code d'autrui pour passer n'est pas
//     une verification, c'est un blocage. La note garde l'information, la compte,
//     et laisse l'arbitrage a qui de droit.
//
// =============================================================================
//  CE QUI N'EST PAS VERIFIE ICI, ET POURQUOI -- a lire avant de s'y fier
// =============================================================================
//  1. **Les references a un AUTRE COMPOSANT** (`NkElementDecl::component`).
//     Elles se resolvent dans le REGISTRE, qui est un objet d'EXECUTION defini
//     dans un `.cpp`. Les inclure ici obligerait a lier ce `.cpp` pour verifier
//     une declaration -- exactement la propriete que toute cette forme existe
//     pour eviter. Elles ont donc leur propre fonction,
//     `NkCheckComponentRefs`, appelee par qui a deja le registre sous la main.
//  2. **Qu'un `x` ou un `y` ne se soit pas glisse dans une declaration.** Aucun
//     champ ne peut le porter aujourd'hui ; le jour ou quelqu'un en ajoute un,
//     c'est la revue qui le voit, pas ce fichier. La regle est ecrite en tete de
//     `NkComponentLayout.h`, la ou elle se lit.
//  3. **Que le dessin honore ce qui est declare.** Aucune verification statique
//     ne peut le dire -- c'est le banc de `NKUIDesign --probe` qui compare les
//     commandes de dessin avec et sans ecrasement.
//
// EN-TETE PUR, zero allocation : l'appelant fournit le tableau d'anomalies, et
//   tous les textes rendus sont des litteraux ou des pointeurs de la
//   declaration elle-meme -- rien n'est construit, donc rien ne se libere.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentDecl.h"
#include "NKEditorKit/Components/NkComponentRole.h"

namespace nkentseu {
	namespace editorkit {

		enum class NkIssueLevel : uint8 { Error = 0, Note };

		struct NkFormIssue {
				NkIssueLevel level = NkIssueLevel::Error;
				/// Cle STABLE et courte, pensee pour etre cherchee dans ce fichier
				/// quand elle apparait dans un rapport : « metrique_inconnue ».
				const char *code = "";
				/// Le nom concerne, tel qu'il est ecrit dans la declaration. C'est un
				/// pointeur DANS la declaration : aucune copie, aucune duree de vie a
				/// gerer -- une declaration est statique.
				const char *subject = "";
				const char *detail = "";
		};

		struct NkCheckReport {
				uint16 errors = 0;
				uint16 notes = 0;
				uint16 written = 0;	  ///< anomalies effectivement ecrites dans le tampon
				bool truncated = false; ///< le tampon etait trop petit : il en manque
		};

		namespace checkdetail {
			inline bool Empty(const char *s) {
				return !s || !*s;
			}
			/// Le nom resout-il vers un NOMBRE NOMME du composant -- metrique ou
			/// parametre (cf. `NkComponentDecl::Number`) ? C'est la verification qui
			/// donne son sens a « une gouttiere se nomme au lieu de s'ecrire » : un
			/// nom qui ne resout nulle part vaudrait **0 en silence**, et
			/// l'agencement s'ecraserait sans que rien ne le dise.
			inline bool HasNumber(const NkComponentDecl &d, const char *n) {
				return Empty(n) || d.FindMetric(n) != nullptr || d.FindParam(n) != nullptr;
			}
		} // namespace checkdetail

		// ── LA PORTE UNIQUE ─────────────────────────────────────────────────────
		// Rend un rapport ; ecrit le detail dans `out` tant qu'il y a la place. Un
		// tampon trop petit est SIGNALE (`truncated`) au lieu d'etre subi : un
		// rapport tronque qu'on croit complet est pire qu'un rapport absent.
		inline NkCheckReport NkCheckComponent(const NkComponentDecl &d, NkFormIssue *out = nullptr,
											  uint16 cap = 0) {
			NkCheckReport rep;
			auto add = [&](NkIssueLevel lvl, const char *code, const char *subject,
						   const char *detail) {
				if (lvl == NkIssueLevel::Error)
					++rep.errors;
				else
					++rep.notes;
				if (out && rep.written < cap)
					out[rep.written++] = NkFormIssue{lvl, code, subject ? subject : "", detail};
				else if (out)
					rep.truncated = true;
			};

			// ── L'IDENTITE ──────────────────────────────────────────────────────
			if (checkdetail::Empty(d.name))
				add(NkIssueLevel::Error, "nom_absent", "",
					"un composant sans cle stable ne peut ni s'enregistrer ni s'ecrire dans un fichier");

			// ── LES LIBELLES SONT-ILS DES CLES ? (regle du 2026-08-18) ──────────
			// ⚠️ EN NOTE, JAMAIS EN ERREUR, ET C'EST RAISONNE -- pas une mollesse.
			//    Le jour ou cette regle est ecrite, DEUX composants portent des
			//    libelles en clair (« Grille », « Taille des vignettes »), dont un
			//    ecrit par une autre main qui compile en ce moment sur ce fichier.
			//    Une erreur rougirait le travail de quelqu'un qui n'a rien casse, et
			//    pour une regle plus jeune que son code. La note COMPTE les libelles
			//    non migres : le compte descend a mesure que les mains migrent, et il
			//    dit a tout moment ce qui reste monolingue.
			//
			// ⚠️ ET UN LIBELLE VIDE N'EST PAS SIGNALE : « pas de libelle » est un
			//    choix legitime (un noeud decoratif n'a rien a traduire). Ne se
			//    signale que ce qui PRETEND etre un libelle sans etre une cle.
			auto keyNote = [&](const char *v, const char *what) {
				if (!checkdetail::Empty(v) && !NkIsLabelKey(v))
					add(NkIssueLevel::Note, "libelle_non_cle", v,
						"ce libelle est du texte en clair, pas une cle de traduction : l'interface "
						"produite serait monolingue. Il s'affichera tel quel (le repli est voulu "
						"visible), mais il ne se traduira pas");
				(void)what;
			};
			keyNote(d.title, "title");
			for (uint16 i = 0; i < d.paramCount; ++i)
				keyNote(d.params[i].label, "param");
			for (uint16 i = 0; i < d.variantCount; ++i)
				keyNote(d.variants[i].label, "variant");
			for (uint16 i = 0; i < d.eventCount; ++i)
				keyNote(d.events[i].label, "event");

			// ── LE ROLE DU COMPOSANT (ajout 1 de Rodolf) ────────────────────────
			if (!checkdetail::Empty(d.role)) {
				const NkRoleDecl *r = NkFindRole(d.role);
				if (!r) {
					add(NkIssueLevel::Error, "role_inconnu", d.role,
						"le catalogue de NkComponentRole.h est la liste COMPLETE des capacites ; un "
						"role hors catalogue n'est pas une extension, c'est une faute de frappe");
				} else {
					for (uint8 i = 0; i < r->eventCount; ++i) {
						const NkEventDecl &req = r->events[i];
						const NkEventDecl *byName = d.FindEvent(req.name);
						if (byName) {
							if (!NkSameArgShape(req, *byName))
								add(NkIssueLevel::Error, "charge_incompatible", req.name,
									"l'evenement porte le nom exige par le role mais pas la meme forme "
									"de charge : un blueprint branche sur ce fait recevrait autre chose "
									"que ce qu'il attend");
							continue;
						}
						// Second etage : le fait est-il porte sous un autre nom ?
						//
						// ⚠️ ON ECARTE D'ABORD LES EVENEMENTS DEJA RECLAMES PAR UN
						//    AUTRE FAIT DU ROLE. Sans ce filtre, la recherche par
						//    forme designait le PREMIER evenement de meme charge --
						//    et `onSelect(Int, String)` a exactement la charge de
						//    `onActivate(Int, String)`. La note aurait alors annonce
						//    « onSelect porte le fait onActivate », ce qui est faux,
						//    et faux d'une facon credible : un rapport qui se trompe
						//    en restant plausible est pire qu'un rapport muet.
						const NkEventDecl *byShape = nullptr;
						for (uint16 e = 0; e < d.eventCount && !byShape; ++e) {
							bool claimed = false;
							for (uint8 q = 0; q < r->eventCount && !claimed; ++q)
								claimed = NkComponentDecl::StrEq(r->events[q].name, d.events[e].name);
							if (!claimed && NkSameArgShape(req, d.events[e]))
								byShape = &d.events[e];
						}
						if (byShape)
							add(NkIssueLevel::Note, "role_nom_divergent", byShape->name,
								"porte le fait exige par le role sous un autre nom -- la convergence "
								"vers .nkgui gagnerait au nom canonique du catalogue");
						else
							add(NkIssueLevel::Error, "role_non_honore", req.name,
								"le role est annonce mais ce fait n'est signale par aucun evenement : "
								"l'application branchera un ecouteur que rien n'appellera");
					}
				}
			}

			// ── LES PARAMETRES, JETONS, METRIQUES, VARIANTES ────────────────────
			for (uint16 i = 0; i < d.paramCount; ++i) {
				const NkParamDecl &p = d.params[i];
				if (checkdetail::Empty(p.name))
					add(NkIssueLevel::Error, "param_sans_nom", d.name, "une cle vide ne se sauve pas");
				if (p.kind == NkParamKind::Enum && (p.enumNames == nullptr || p.enumCount == 0))
					add(NkIssueLevel::Error, "enum_sans_libelles", p.name,
						"un parametre Enum sans libelles n'affiche rien dans l'editeur et ne se relit "
						"pas depuis un fichier");
			}
			// Un meme nom dans les deux tables : la disposition les lit dans un seul
			// espace de noms (`NkComponentDecl::Number`), donc l'ordre de lecture
			// trancherait en silence. On refuse plutot que d'avoir a l'expliquer.
			for (uint16 i = 0; i < d.metricCount; ++i)
				if (d.FindParam(d.metrics[i].name) != nullptr)
					add(NkIssueLevel::Error, "nom_ambigu", d.metrics[i].name,
						"ce nom est declare A LA FOIS en metrique et en parametre : la disposition ne "
						"saurait pas lequel elle lit");
			for (uint16 i = 0; i < d.tokenCount; ++i)
				if (checkdetail::Empty(d.tokens[i].defaultRole))
					add(NkIssueLevel::Error, "jeton_sans_role", d.tokens[i].name,
						"un jeton sans role par defaut ne se resout en aucune couleur");
			for (uint16 i = 0; i < d.variantCount; ++i)
				if (checkdetail::Empty(d.variants[i].name))
					add(NkIssueLevel::Error, "variante_sans_nom", d.name,
						"une variante se choisit par son nom dans un fichier d'ecarts");

			// ── LES EVENEMENTS ET LEURS CHARGES ─────────────────────────────────
			for (uint16 i = 0; i < d.eventCount; ++i) {
				const NkEventDecl &e = d.events[i];
				if (checkdetail::Empty(e.name))
					add(NkIssueLevel::Error, "evenement_sans_nom", d.name, "un blueprint branche un nom");
				for (uint8 a = 0; a < e.argCount; ++a) {
					const NkArgDecl &arg = e.args[a];
					if (arg.kind == NkArgKind::Void || arg.kind >= NkArgKind::Count)
						add(NkIssueLevel::Error, "charge_sans_type", e.name,
							"`Void` est un type de RETOUR, pas un type d'argument : une charge sans "
							"type ne se branche pas");
					if (arg.kind == NkArgKind::Enum && (arg.enumNames == nullptr || arg.enumCount == 0))
						add(NkIssueLevel::Error, "charge_enum_vide", e.name,
							"un Enum sans libelles ne s'ecrit pas dans le bloc controller de .nkgui");
				}
			}

			// ── L'ARBRE (ajout 2), LA TAILLE ET L'AGENCEMENT (ajout 3) ──────────
			if (d.elementCount > 0) {
				uint16 roots = 0;
				for (uint16 i = 0; i < d.elementCount; ++i) {
					const NkElementDecl &e = d.elements[i];

					if (checkdetail::Empty(e.name)) {
						add(NkIssueLevel::Error, "element_sans_nom", d.name,
							"un sous-element se designe par son nom, y compris comme parent");
						continue;
					}
					// Unicite : deux elements du meme nom rendent `parent` ambigu, et
					// l'arbre se reconstruit faux sans que rien ne le signale.
					for (uint16 j = 0; j < i; ++j)
						if (NkComponentDecl::StrEq(d.elements[j].name, e.name))
							add(NkIssueLevel::Error, "element_duplique", e.name,
								"deux sous-elements du meme nom : toute reference par `parent` devient "
								"ambigue");

					if (checkdetail::Empty(e.parent)) {
						++roots;
					} else {
						const int32 pi = d.ElementIndex(e.parent);
						if (pi < 0)
							add(NkIssueLevel::Error, "parent_inconnu", e.parent,
								"aucun sous-element de ce nom : le noeud serait orphelin, donc jamais "
								"dispose ni dessine");
						else if ((uint16)pi >= i)
							add(NkIssueLevel::Error, "parent_apres_enfant", e.name,
								"un parent doit apparaitre PLUS HAUT dans la table -- c'est cette "
								"contrainte, et elle seule, qui rend un cycle impossible a ecrire et "
								"le resolveur non recursif");
					}

					if (!checkdetail::Empty(e.role) && !NkFindRole(e.role))
						add(NkIssueLevel::Error, "role_inconnu", e.role,
							"role hors catalogue sur un sous-element");

					// Les NOMS de metrique doivent resoudre. C'est la verification
					// qui donne son sens a « une gouttiere se nomme au lieu de
					// s'ecrire » : un nom qui ne resout pas vaut 0 en silence.
					if (!checkdetail::HasNumber(d, e.width.valueMetric))
						add(NkIssueLevel::Error, "metrique_inconnue", e.width.valueMetric,
							"largeur nommee mais declaree ni en metrique ni en parametre");
					if (!checkdetail::HasNumber(d, e.height.valueMetric))
						add(NkIssueLevel::Error, "metrique_inconnue", e.height.valueMetric,
							"hauteur nommee mais declaree ni en metrique ni en parametre");
					if (!checkdetail::HasNumber(d, e.layout.spacingMetric))
						add(NkIssueLevel::Error, "metrique_inconnue", e.layout.spacingMetric,
							"gouttiere nommee mais non declaree");
					if (!checkdetail::HasNumber(d, e.layout.padMetric))
						add(NkIssueLevel::Error, "metrique_inconnue", e.layout.padMetric,
							"marge interne nommee mais non declaree");
					if (!checkdetail::HasNumber(d, e.layout.gridCellMetric))
						add(NkIssueLevel::Error, "metrique_inconnue", e.layout.gridCellMetric,
							"largeur de cellule nommee mais non declaree");

					// Un noeud qui a des enfants et aucun agencement : le resolveur
					// ne poserait AUCUN de ses enfants, et ils disparaitraient sans
					// message. Defaut muet, donc erreur.
					const bool hasChildren = d.NextChildOf(e.name, 0) >= 0;
					if (hasChildren && e.layout.kind == NkLayoutKind::None)
						add(NkIssueLevel::Error, "agencement_absent", e.name,
							"ce noeud a des enfants mais ne declare aucun agencement : ils ne seraient "
							"jamais disposes, et rien ne le dirait");
					// ⚠️ CETTE NOTE EST LA FRONTIERE, PAS UN RESIDU. Un agencement
					//    declare sur une feuille decrit presque toujours des enfants
					//    REPETES PAR LA DONNEE -- les cartes d'un navigateur, les
					//    lignes d'un arbre. Leur nombre vient du modele, leur
					//    profondeur aussi : l'arbre STATIQUE ne sait pas les dire, et
					//    `NkSolveLayout` ne les instancie pas.
					//    Elle est comptee plutot que tue, parce que c'est elle qui
					//    mesure combien de composants attendent un gabarit repete.
					//    Deux, au 19/08 : `content_browser.grid` et `tree_view.rows`,
					//    ecrits par deux mains differentes -- c'est ce qui rend
					//    l'ajout futur GENERAL plutot que taille pour les arbres.
					if (!hasChildren && e.layout.kind != NkLayoutKind::None)
						add(NkIssueLevel::Note, "agencement_sans_enfant", e.name,
							"agencement declare sans enfant statique : la structure repetee par la "
							"donnee n'est pas exprimable dans l'arbre d'aujourd'hui, et c'est le "
							"dessin qui l'instancie a partir des metriques declarees");
					if (e.layout.kind == NkLayoutKind::Grid && e.layout.gridColumns == 0 &&
						checkdetail::Empty(e.layout.gridCellMetric))
						add(NkIssueLevel::Error, "grille_indeterminee", e.name,
							"une grille sans nombre de colonnes ET sans largeur de cellule ne peut pas "
							"choisir son nombre de colonnes");

					// Un ancrage n'a de sens que si le PARENT ancre. Sinon il est
					// ignore -- silencieusement, donc on le dit.
					if (e.anchorEdges != 0) {
						const NkElementDecl *par = d.FindElement(e.parent);
						if (!par || par->layout.kind != NkLayoutKind::Anchor)
							add(NkIssueLevel::Note, "ancrage_ignore", e.name,
								"des bords d'ancrage sont declares mais le parent n'est pas en "
								"agencement Anchor : ils ne servent a rien");
					}

					// ── LE GABARIT REPETE (reparation du §10.4) ────────────────
					if (e.repeat != NkRepeatKind::Once) {
						// La RACINE ne se repete pas : rien, dans un composant, ne
						// repete le composant entier. Ce serait une instanciation, et
						// c'est le travail de l'hote, pas de la declaration.
						if (checkdetail::Empty(e.parent))
							add(NkIssueLevel::Error, "repetition_sur_racine", e.name,
								"la racine d'un composant ne peut pas etre repetee par la donnee : "
								"un composant se repete en etant INSTANCIE par son hote, pas en se "
								"declarant repete lui-meme");

						// Deux freres repetes sous le meme parent : LEQUEL suit la
						// donnee ? La declaration ne le dit pas, et le dessin
						// trancherait tout seul -- chacun a sa facon. Note plutot
						// qu'erreur : le cas peut etre voulu (deux listes soeurs).
						for (int32 j = d.NextChildOf(e.parent, 0); j >= 0;
							 j = d.NextChildOf(e.parent, (uint16)(j + 1))) {
							const NkElementDecl &sib = d.elements[j];
							if (&sib != &e && sib.repeat != NkRepeatKind::Once) {
								add(NkIssueLevel::Note, "repetition_freres", e.name,
									"deux freres se declarent repetes par la donnee sous le meme "
									"parent : la declaration ne dit pas lequel suit quelle donnee, "
									"et deux dessins trancheraient differemment");
								break;
							}
						}
					}
				}
				if (roots == 0)
					add(NkIssueLevel::Error, "racine_absente", d.name,
						"tout sous-element a un parent : il n'y a pas d'arbre, seulement un cycle ou un "
						"ensemble d'orphelins");
				else if (roots > 1)
					add(NkIssueLevel::Error, "racine_multiple", d.name,
						"plusieurs sous-elements sans parent : le resolveur n'en poserait qu'un, et les "
						"autres seraient perdus sans message");
			}

			return rep;
		}

		// ── LES REFERENCES ENTRE COMPOSANTS -- porte SEPAREE, et pour une raison ─
		// « Une interface complete est un composant qui en contient d'autres. » La
		// verification de ces renvois a besoin du REGISTRE, donc de lier
		// `NkComponentRegistry.cpp`. La garder ici aurait rendu impossible de
		// verifier une declaration sans rien lier -- la propriete meme que la
		// frontiere avec `NKReflection` revendique. Elle est donc dehors, et son
		// appelant est celui qui a deja le registre : l'editeur, ou un banc qui
		// enregistre ce qu'il veut verifier.
		inline NkCheckReport NkCheckComponentRefs(const NkComponentDecl &d, NkFormIssue *out = nullptr,
												  uint16 cap = 0) {
			NkCheckReport rep;
			for (uint16 i = 0; i < d.elementCount; ++i) {
				const NkElementDecl &e = d.elements[i];
				if (checkdetail::Empty(e.component))
					continue;
				if (NkComponentRegistry::Find(e.component) == nullptr) {
					++rep.errors;
					if (out && rep.written < cap)
						out[rep.written++] =
							NkFormIssue{NkIssueLevel::Error, "composant_inconnu", e.component,
										"ce sous-element delegue a un composant qui n'est pas enregistre "
										"-- le renvoi ne se resoudra jamais"};
					else
						rep.truncated = (out != nullptr);
				}
			}
			return rep;
		}

	} // namespace editorkit
} // namespace nkentseu
