#pragma once
// -----------------------------------------------------------------------------
// @File    NkComponentRole.h
// @Brief   LE ROLE : la capacite qu'on attribue a une apparence.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (Rodolf, 2026-08-18, soir)
// =============================================================================
//  Premier de ses quatre ajouts a la forme, et le plus structurant :
//
//     « On dessine une APPARENCE, puis on lui ATTRIBUE UNE CAPACITE (bouton,
//      bascule, liste). L'apparence et le comportement sont separes -- une
//      poignee de roles sert des milliers d'apparences. »
//
//  Sans ca, chaque dessin embarquerait son comportement : mille apparences de
//  bouton, mille fois le meme code de clic, mille occasions de diverger. Avec
//  ca, un dessin est du DESSIN, et ce qu'il SAIT FAIRE se declare en un mot.
//
// =============================================================================
//  CE QU'UN ROLE PORTE, ET CE QU'IL NE PORTE PAS
// =============================================================================
//  IL PORTE      : les EVENEMENTS que la capacite doit savoir signaler (avec
//                  leur charge), et les ETATS que son apparence doit savoir
//                  peindre (survol, enfonce, coche, ouvert...).
//  IL NE PORTE PAS : une couleur, une taille, un agencement, un dessin. Rien de
//                  ce qui se voit. Si un role commence a decrire ce qu'on voit,
//                  la separation demandee par Rodolf est perdue -- et c'est le
//                  seul defaut a surveiller dans ce fichier.
//
//  PAS D'HERITAGE ENTRE ROLES, ET C'EST VOLONTAIRE. `tree` recopie les trois
//  evenements de `list` au lieu d'en deriver. Trois lignes recopiees dans une
//  table qu'on lit valent mieux qu'un mecanisme de derivation qu'il faudrait
//  comprendre avant d'ajouter un role. La table est faite pour etre lue par
//  quelqu'un qui arrive.
//
// =============================================================================
//  LE CONTRAT DE ROLE EST VERIFIE, PAS SEULEMENT ANNONCE
// =============================================================================
//  Un composant qui declare `role = "button"` sans jamais emettre `onClick` est
//  exactement « un parametre qui n'est pas honore » : l'application branche et
//  attend. `NkCheckComponent` (NkComponentCheck.h) compare donc ce que le role
//  EXIGE a ce que le composant DECLARE.
//
//  LA REGLE DE CORRESPONDANCE, ET ELLE A DEUX ETAGES -- c'est le second qui
//  compte :
//   1. par NOM : le composant declare un evenement du nom exige. Cas normal.
//   2. par FORME DE CHARGE : le composant declare un evenement de nom different
//      mais dont les TYPES d'arguments correspondent, dans l'ordre. Le fait est
//      alors porte, et la verification emet une NOTE de nommage.
//
//  Pourquoi deux etages plutot qu'un : la premiere ecriture de ce fichier
//  exigeait le nom seul, et elle rendait ROUGE le composant deja livre --
//  `content_browser` emet `onDoubleClick` la ou le role `list` nomme
//  `onActivate`. Refuser le composant aurait force a renommer un champ C++ qui
//  appartient a un autre agent ; l'accepter en silence aurait perdu
//  l'information. La note la garde, la compte, et laisse l'arbitrage a Rodolf.
//
//  LES NOMS D'ARGUMENTS NE FONT PAS PARTIE DU CONTRAT, seuls leurs TYPES. Le
//  navigateur nomme sa charge `path` la ou un arbre la nommerait `id` : c'est du
//  vocabulaire local, et il vaut mieux qu'un nom generique impose. Ce qu'un
//  blueprint doit pouvoir tenir pour acquis, c'est la FORME de la charge.
//
// =============================================================================
//  OU AJOUTER UN ROLE -- le point d'extension, nomme et visible
// =============================================================================
//  Une entree dans `kRoles`, en bas de ce fichier, avec sa table d'evenements
//  juste au-dessus d'elle. Rien d'autre a toucher, nulle part.
//
//  AVANT D'EN AJOUTER UN, la question qui evite la proliferation : **est-ce une
//  capacite, ou une apparence ?** « bouton d'icone », « bouton plat », « gros
//  bouton » ne sont pas des roles -- ce sont trois apparences du role `button`.
//  Un role nouveau se justifie par un EVENEMENT ou un ETAT que les autres n'ont
//  pas. Une poignee de roles doit servir des milliers d'apparences : si la table
//  ci-dessous depasse la vingtaine, c'est qu'on y a range des apparences.
//
// EN-TETE PUR : n'ajoute aucune dependance a `NkComponentDecl.h`.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkComponentDecl.h"

namespace nkentseu {
	namespace editorkit {

		// ── LES ETATS QU'UNE APPARENCE PEUT AVOIR A PEINDRE ─────────────────────
		// Un masque de bits, pas une enumeration : un element est simultanement
		// survole ET coche ET focalise. C'est le role qui dit LESQUELS existent pour
		// sa capacite ; c'est l'apparence qui decide a quoi ils ressemblent.
		//
		// APPEND-ONLY : ces bits finiront dans un fichier de description.
		namespace nkstate {
			constexpr uint16 Normal = 0;		 ///< aucun etat particulier
			constexpr uint16 Hover = 1u << 0;	 ///< la souris est dessus
			constexpr uint16 Pressed = 1u << 1;	 ///< bouton maintenu sur lui
			constexpr uint16 Disabled = 1u << 2; ///< inactif : il n'entend plus
			constexpr uint16 Focused = 1u << 3;	 ///< il a le focus clavier
			constexpr uint16 Checked = 1u << 4;	 ///< bascule enclenchee
			constexpr uint16 Selected = 1u << 5; ///< entree choisie dans une collection
			constexpr uint16 Open = 1u << 6;	 ///< noeud deplie (arbre, section repliable)
			constexpr uint16 Dragged = 1u << 7;	 ///< en cours de glisser
		} // namespace nkstate

		// ── UN ROLE ─────────────────────────────────────────────────────────────
		struct NkRoleDecl {
				const char *name = "";	  ///< cle STABLE : « button », « tree »
				const char *label = "";	  ///< libelle affichable dans l'editeur
				const char *purpose = ""; ///< la capacite, en une ligne

				/// Ce que la capacite DOIT savoir signaler. Reutilise `NkEventDecl`
				/// a dessein : un evenement exige par un role et un evenement declare
				/// par un composant sont la meme chose, et doivent donc s'ecrire de
				/// la meme facon. Deux formes pour un concept, c'est deux verites.
				const NkEventDecl *events = nullptr;
				uint8 eventCount = 0;

				uint16 states = 0;			  ///< masque `nkstate::*` que l'apparence doit peindre
				bool acceptsChildren = false; ///< la capacite a-t-elle un sens avec des enfants
		};

		// ── LES CHARGES PARTAGEES ───────────────────────────────────────────────
		// Ecrites une fois, referencees par plusieurs roles. `id` est une chaine et
		// pas un index : un blueprint n'a pas le modele sous la main, et une charge
		// qui n'est interpretable qu'en possedant l'objet emetteur n'est pas une
		// charge, c'est un pointeur deguise. `index` sert au code, `id` au graphe.
		namespace roledetail {
			inline const NkArgDecl *ArgsEntry() {
				static const NkArgDecl a[] = {
					{"index", NkArgKind::Int, nullptr, 0},
					{"id", NkArgKind::String, nullptr, 0},
				};
				return a;
			}
			inline const NkArgDecl *ArgsPoint() {
				static const NkArgDecl a[] = {
					{"index", NkArgKind::Int, nullptr, 0},
					{"at", NkArgKind::Vec2, nullptr, 0},
				};
				return a;
			}
			inline const NkArgDecl *ArgsOpen() {
				static const NkArgDecl a[] = {
					{"index", NkArgKind::Int, nullptr, 0},
					{"id", NkArgKind::String, nullptr, 0},
					{"open", NkArgKind::Bool, nullptr, 0},
				};
				return a;
			}
			inline const NkArgDecl *ArgsText() {
				static const NkArgDecl a[] = {
					{"text", NkArgKind::String, nullptr, 0},
				};
				return a;
			}
			inline const NkArgDecl *ArgsBool() {
				static const NkArgDecl a[] = {
					{"on", NkArgKind::Bool, nullptr, 0},
				};
				return a;
			}
			inline const NkArgDecl *ArgsFloat() {
				static const NkArgDecl a[] = {
					{"value", NkArgKind::Float, nullptr, 0},
				};
				return a;
			}
			inline const NkArgDecl *ArgsDrop() {
				static const NkArgDecl a[] = {
					{"index", NkArgKind::Int, nullptr, 0},
					{"payloadType", NkArgKind::String, nullptr, 0},
				};
				return a;
			}
		} // namespace roledetail

		// ── LE CATALOGUE ────────────────────────────────────────────────────────
		// LE POINT D'EXTENSION EST ICI, et il tient en une ligne par role.
		//
		// Il est volontairement COURT. Neuf roles couvrent aujourd'hui les huit
		// paliers de composants de `ROADMAP.md` : ce n'est pas une prouesse, c'est
		// la preuve que la separation apparence/capacite fonctionne. Le jour ou il
		// faut un dixieme role pour un dixieme composant, c'est que le role decrit
		// une apparence.
		inline const NkRoleDecl *NkRoleCatalog(uint16 &outCount) {
			// Chaque table d'evenements est locale au role qu'elle sert : on la lit
			// juste au-dessus de lui, jamais a l'autre bout du fichier.
			static const NkEventDecl kButton[] = {
				{"onClick", "Clic", "le bouton a ete active (souris ou clavier)", nullptr, 0, false},
			};
			static const NkEventDecl kToggle[] = {
				{"onToggle", "Bascule", "l'etat coche a change", roledetail::ArgsBool(), 1, true},
			};
			static const NkEventDecl kField[] = {
				{"onChanged", "Saisie", "le texte a change", roledetail::ArgsText(), 1, true},
				{"onSubmit", "Validation", "l'utilisateur a valide (Entree)", roledetail::ArgsText(), 1,
				 false},
			};
			static const NkEventDecl kSlider[] = {
				{"onChanged", "Valeur", "la valeur a change", roledetail::ArgsFloat(), 1, true},
			};
			// ⚠️ `onActivate` NOMME LE FAIT, PAS LE GESTE. Un double-clic active,
			//    mais la touche Entree aussi, et un appui long sur tactile aussi. Un
			//    role nomme ce qui EST ARRIVE ; c'est l'apparence qui sait par quel
			//    geste. C'est la raison pour laquelle `onDoubleClick` du navigateur
			//    de contenu remonte aujourd'hui en NOTE de nommage.
			static const NkEventDecl kList[] = {
				{"onSelect", "Selection", "l'entree active a change", roledetail::ArgsEntry(), 2, false},
				{"onActivate", "Activation", "une entree a ete activee (double-clic, Entree)",
				 roledetail::ArgsEntry(), 2, false},
				{"onContextMenu", "Menu contextuel", "clic droit ; `index` vaut -1 sur le fond",
				 roledetail::ArgsPoint(), 2, false},
			};
			static const NkEventDecl kTree[] = {
				{"onSelect", "Selection", "l'entree active a change", roledetail::ArgsEntry(), 2, false},
				{"onActivate", "Activation", "une entree a ete activee (double-clic, Entree)",
				 roledetail::ArgsEntry(), 2, false},
				{"onContextMenu", "Menu contextuel", "clic droit ; `index` vaut -1 sur le fond",
				 roledetail::ArgsPoint(), 2, false},
				{"onExpand", "Depliage", "un noeud s'ouvre ou se referme", roledetail::ArgsOpen(), 3,
				 true},
			};
			static const NkEventDecl kDropTarget[] = {
				{"onDrop", "Depot", "une charge glissee a ete relachee ici", roledetail::ArgsDrop(), 2,
				 false},
			};

			static const NkRoleDecl kRoles[] = {
				{"container", "Conteneur", "ne fait qu'assembler et disposer -- n'entend rien", nullptr,
				 0, nkstate::Normal, true},
				{"label", "Libelle", "affiche du texte ou une icone, sans interaction", nullptr, 0,
				 nkstate::Disabled, false},
				{"button", "Bouton", "une action ponctuelle", kButton, 1,
				 nkstate::Hover | nkstate::Pressed | nkstate::Disabled | nkstate::Focused, false},
				{"toggle", "Bascule", "un etat a deux positions que l'utilisateur retourne", kToggle, 1,
				 nkstate::Hover | nkstate::Pressed | nkstate::Disabled | nkstate::Focused |
					 nkstate::Checked,
				 false},
				{"text_field", "Champ de saisie", "l'utilisateur ecrit dedans", kField, 2,
				 nkstate::Hover | nkstate::Focused | nkstate::Disabled, false},
				{"slider", "Glissiere", "une valeur continue entre deux bornes", kSlider, 1,
				 nkstate::Hover | nkstate::Pressed | nkstate::Disabled | nkstate::Focused, false},
				{"list", "Collection", "des entrees qu'on parcourt, choisit et active", kList, 3,
				 nkstate::Hover | nkstate::Selected | nkstate::Focused | nkstate::Disabled, true},
				{"tree", "Arbre", "une collection HIERARCHIQUE : les entrees s'ouvrent et se ferment",
				 kTree, 4,
				 nkstate::Hover | nkstate::Selected | nkstate::Focused | nkstate::Open |
					 nkstate::Disabled,
				 true},
				{"drop_target", "Cible de depot", "recoit une charge glissee", kDropTarget, 1,
				 nkstate::Hover | nkstate::Dragged, false},
			};
			outCount = (uint16)(sizeof(kRoles) / sizeof(kRoles[0]));
			return kRoles;
		}

		/// Le role de ce nom, ou `nullptr`. Un role inconnu est une ERREUR de
		/// declaration, pas une extension : le catalogue est la liste complete de ce
		/// que la bibliotheque sait faire faire a une apparence.
		inline const NkRoleDecl *NkFindRole(const char *name) {
			if (!name || !*name)
				return nullptr;
			uint16 n = 0;
			const NkRoleDecl *r = NkRoleCatalog(n);
			for (uint16 i = 0; i < n; ++i)
				if (NkComponentDecl::StrEq(r[i].name, name))
					return &r[i];
			return nullptr;
		}

		/// Deux charges ont-elles la MEME FORME ? Les types, dans l'ordre ; jamais
		/// les noms (cf. l'en-tete de ce fichier).
		inline bool NkSameArgShape(const NkEventDecl &a, const NkEventDecl &b) {
			if (a.argCount != b.argCount)
				return false;
			for (uint8 i = 0; i < a.argCount; ++i)
				if (a.args[i].kind != b.args[i].kind)
					return false;
			return true;
		}

	} // namespace editorkit
} // namespace nkentseu
