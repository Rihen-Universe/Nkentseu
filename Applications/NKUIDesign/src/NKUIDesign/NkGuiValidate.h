//
// NkGuiValidate.h
// =============================================================================
// Description :
//   La VALIDATION PAR ROLE ET PAR TYPE d'un document `.nkgui` -- decision 4 de
//   Rodolf (2026-08-21). Elle repond a ce que le document 2 §4 promet (« le
//   compilateur rejette une propriete absente du schema du role ») et que le
//   lecteur livre le 2026-08-21 ne faisait pas.
//
// =============================================================================
//  DEUX CHOIX QUI PORTENT TOUT LE FICHIER, ET RODOLF LES A TRANCHES
// =============================================================================
//
//  1. LE VOCABULAIRE DE REFERENCE EST CELUI DU DOCUMENT 7, PAS LA TABLE §8 DU
//     DOCUMENT 2. Les dix documents du corpus sont ecrits en `Text`,
//     `TextField`, `Dropdown`, `Item`, `Progress` -- la table §8, elle, connait
//     `InputText`, `Combo`, `Selectable`, `ProgressBar` et ignore `Text`. Valider
//     contre §8 rejetterait le corpus en bloc, c'est-a-dire tout ce qui existe.
//     Les anciens noms restent acceptes en ALIAS (doc 7 §5) avec un
//     avertissement qui nomme le nouveau : « un rôle renomme garde son ancien
//     nom, sinon personne ne renomme jamais rien ».
//
//  2. UN ROLE INCONNU PRODUIT UNE ERREUR NOMMEE, JAMAIS UN REJET MUET DU FICHIER
//     ENTIER. C'est pour ca que la validation est SEPAREE de la lecture : le
//     document se lit d'abord, integralement, et se juge ensuite. Un editeur doit
//     pouvoir OUVRIR un document qui contient une faute -- sinon la faute est
//     incorrigeable, et l'outil qui la signale est celui qui empeche de la
//     reparer.
//
//     C'est aussi ce qui garde l'aller-retour intact : la validation ne touche
//     pas au modele, elle ne fait qu'ajouter des diagnostics.
//
// Codes emis (doc 2 §12, etendu) :
//   E-ROLE-INCONNU  role absent du vocabulaire du document 7        Bloquant
//   E-TYPE          propriete absente du schema du role, ou valeur
//                   d'un type que le role n'attend pas             Bloquant
//   W-ROLE-ALIAS    ancien nom, encore lu, a remplacer             Avertissement
//   W-ID-DUPLIQUE   deux widgets partagent le meme id (doc 2 §12)  Avertissement
//   E-EFFET-INCONNU bloc inconnu dans un `appearance` (doc 9 §3.1) Bloquant
//   E-ETAT-INCONNU  `appearance(X)` ou X n'est pas de la liste
//                   fermee des etats (doc 9 §3.2bis)               Bloquant
//   W-ETAT-DOUBLE   le meme etat declare deux fois sur un widget  Avertissement
//
//
// =============================================================================
//  BASCULE DU 2026-08-22 : CE FICHIER JUGE UNE `NkArchive`, PLUS UN `NkGDocument`
// =============================================================================
//  `NkGuiFormat.h` a ete retire. La lecture passe desormais par
//  `NKSerialization/NkGui/NkGuiArchive.h`, une couche PUREMENT SYNTAXIQUE qui ne
//  connait aucun mot-cle du langage. Deux consequences, et il vaut mieux les
//  ecrire que de les decouvrir :
//
//   1. QUATRE REFUS ONT CHANGE DE DOMICILE. L'ancien lecteur refusait `#12345`,
//      une liste a virgule finale, une cle de dictionnaire qui n'en est pas une,
//      et une section inconnue dans un fichier de NOTRE version. Un lecteur
//      syntaxique les lit tres bien -- ce sont des jetons nus. Ils sont donc
//      devenus des FAUTES DE VALIDATION, ici : `E-VALEUR` et `E-SECTION-INCONNUE`.
//      Ce n'est pas un relachement, c'est ce que ce fichier annonce depuis le
//      debut -- **lire n'est pas juger, et un document invalide doit rester
//      LISIBLE**. Un editeur qui refuse d'ouvrir le fichier dont il signale la
//      faute rend cette faute incorrigible.
//
//   2. LES DIAGNOSTICS PORTENT LA LIGNE **ET** LE CHEMIN.
//      `widgets / VBox "v" / Button "ok" . label`, ligne 47. Les deux, parce
//      qu'ils repondent a deux questions : **le chemin dit QUOI, la ligne dit OU
//      ALLER**. Celui qui corrige un `.nkgui` l'a ouvert dans un editeur de
//      texte ; lui donner le chemin seul, c'est le laisser chercher.
//
//      ⚠️ ET J'AVAIS D'ABORD LIVRE LE CHEMIN SEUL, en le presentant comme un
//         « echange ». C'en etait un, et il n'avait pas lieu d'etre :
//         l'information n'etait pas PERDUE, elle n'etait pas TRANSPORTEE -- le
//         lecteur connait la ligne au moment ou il analyse. Chiffrage fait avant
//         d'ecrire, et MESURE ensuite : le champ tient dans le rembourrage que la
//         trivia portait deja -- `sizeof` 200 avec, 200 sans. **Zero octet**, zero
//         allocation nouvelle, aucun changement de forme de l'archive. Quand une
//         information utile ne coute rien, « c'est un echange » est une facon de
//         ne pas la porter.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKUIDESIGN_NKGUIVALIDATE_H__
#define __NKENTSEU_NKUIDESIGN_NKGUIVALIDATE_H__

#include "NKSerialization/NkGui/NkGuiArchive.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkuidesign {
	namespace guifmt {

		using nkentseu::NkArchive;
		using nkentseu::NkArchiveNode;
		using nkentseu::NkGuiArchive;
		using nkentseu::NkGuiValueKind;
		using nkentseu::NkString;
		using nkentseu::NkStringView;
		using nkentseu::NkVector;
		using nkentseu::nk_int32;
		using nkentseu::uint32;

		// =====================================================================
		//  DEUX SERVICES QUI SURVIVENT A LA BASCULE
		// =====================================================================
		//  Ils vivaient dans `NkGuiFormat.h` et ne dependaient d'AUCUN modele :
		//  l'un formate un entier, l'autre decoupe un chemin pointe. Les perdre
		//  avec le fichier aurait ete les reecrire ailleurs.

		inline NkString NkGU32(uint32 v) {
			char buf[16];
			uint32 n = 0;
			if (v == 0) {
				buf[n++] = '0';
			}
			while (v > 0 && n < 15) {
				buf[n++] = (char)('0' + (v % 10));
				v /= 10;
			}
			NkString out;
			for (uint32 i = 0; i < n; ++i) {
				out.Append(buf[n - 1 - i]);
			}
			return out;
		}

		/// Decoupe `a.b.c` en ses segments. Le document garde le chemin ENTIER --
		/// c'est lui qui est reemis, donc c'est lui qui fait foi. Ce service est
		/// pour qui CONSOMME le document (resolution de pin, resolution d'enum) ;
		/// il ne participe pas a l'aller-retour.
		inline NkVector<NkString> NkGSplitPath(const NkString &ident) {
			NkVector<NkString> out;
			const char *p = ident.Data();
			if (!p) {
				return out;
			}
			const uint32 n = (uint32)ident.Size();
			uint32 begin = 0;
			for (uint32 i = 0; i <= n; ++i) {
				if (i == n || p[i] == '.') {
					out.PushBack(NkString(p + begin, i - begin));
					begin = i + 1;
				}
			}
			return out;
		}

		// =====================================================================
		//  LE SCHEMA
		// =====================================================================
		//
		//  Chaque propriete porte une lettre de TYPE ATTENDU. Le code tient en une
		//  lettre parce que la table doit rester relisible : une table de schema
		//  qu'on ne peut plus lire d'un coup d'oeil cesse d'etre corrigee, et une
		//  validation qui ment est pire que pas de validation.
		//
		//    a  n'importe quoi          n  nombre            s  chaine
		//    b  booleen (true|false)    v  Vec2              c  couleur
		//    l  liste                   d  dictionnaire      i  identifiant/drapeaux
		//    e  etiquette : identifiant OU chaine (les deux se lisent)
		//    r  reference de donnee : identifiant OU chaine (`bind`)
		struct NkGSchemaProp {
				const char *name;
				char kind;
		};

		struct NkGSchemaRole {
				const char *role;
				const NkGSchemaProp *props;
				uint32 count;
		};

		/// LES PROPRIETES TRANSVERSALES, et elles ne sont pas une commodite.
		/// Document 7 §4 : une capacite transversale n'entre jamais au catalogue,
		/// « sinon il faudrait `ButtonWithTooltip`, `FieldWithTooltip`, et le
		/// catalogue doublerait a chaque capacite ajoutee ». `tooltip` et
		/// `enabled` sont donc admis sur TOUT role -- et le corpus les emploie
		/// exactement comme ca (116 `tooltip`, 6 `enabled`).
		inline const NkGSchemaProp *NkGUniversalProps(uint32 &count) {
			static const NkGSchemaProp kUniversal[] = {
				{"tooltip", 's'}, {"enabled", 'b'}, {"visible", 'b'}, {"id", 's'},
			};
			count = 4;
			return kUniversal;
		}

		/// LE VOCABULAIRE, tel que le document 7 §3 l'ecrit, plus `Text` et
		/// `Spacer` (doc 9 §2, tranches par Rodolf le 2026-08-21).
		inline const NkGSchemaRole *NkGRoleTable(uint32 &count) {
			static const NkGSchemaProp pButton[] = {{"label", 's'},	 {"icon", 'e'},
													{"flags", 'i'},	 {"repeat", 'b'},
													{"repeatDelay", 'n'}, {"repeatRate", 'n'}};
			static const NkGSchemaProp pImageButton[] = {
				{"image", 'e'}, {"source", 'e'}, {"size", 'a'}, {"tint", 'c'}};
			static const NkGSchemaProp pMenuItem[] = {
				{"label", 's'}, {"shortcut", 's'}, {"checked", 'b'}};
			static const NkGSchemaProp pTextField[] = {
				{"bind", 'r'},	   {"multiline", 'b'}, {"secret", 'b'}, {"maxChars", 'n'},
				{"wrap", 'b'},	   {"placeholder", 's'}, {"flags", 'i'}, {"valueType", 'e'}};
			static const NkGSchemaProp pNumberField[] = {{"bind", 'r'},	 {"valueType", 'e'},
														 {"step", 'n'},	 {"min", 'n'},
														 {"max", 'n'}};
			static const NkGSchemaProp pSlider[] = {{"bind", 'r'}, {"valueType", 'e'},
													{"min", 'n'},  {"max", 'n'},
													{"step", 'n'}};
			static const NkGSchemaProp pDrag[] = {{"bind", 'r'},  {"valueType", 'e'},
												  {"speed", 'n'}, {"min", 'n'},
												  {"max", 'n'},	  {"dir", 'e'}};
			static const NkGSchemaProp pColorField[] = {
				{"bind", 'r'}, {"alpha", 'b'}, {"mode", 'e'}};
			static const NkGSchemaProp pCheckbox[] = {
				{"bind", 'r'}, {"tristate", 'b'}, {"label", 's'}};
			static const NkGSchemaProp pSwitch[] = {{"bind", 'r'}, {"label", 's'}};
			static const NkGSchemaProp pRadioGroup[] = {
				{"bind", 'r'}, {"options", 'l'}, {"orientation", 'e'}};
			static const NkGSchemaProp pDropdown[] = {
				{"bind", 'r'}, {"items", 'l'}, {"editable", 'b'}};
			static const NkGSchemaProp pListBox[] = {
				{"bind", 'r'}, {"items", 'l'}, {"multiSelect", 'b'}};
			static const NkGSchemaProp pItem[] = {
				{"label", 's'}, {"selected", 'b'}, {"editable", 'b'}, {"text", 's'}};
			static const NkGSchemaProp pTreeItem[] = {
				{"label", 's'}, {"expanded", 'b'}, {"editable", 'b'}};
			static const NkGSchemaProp pText[] = {{"text", 's'},		{"wrap", 'b'},
												  {"align", 'e'},	{"maxLines", 'n'},
												  {"overflow", 'e'}, {"for", 's'}};
			static const NkGSchemaProp pImage[] = {{"source", 'e'}, {"size", 'a'},
												   {"tint", 'c'},  {"uv0", 'v'},
												   {"uv1", 'v'},   {"texId", 'a'}};
			static const NkGSchemaProp pProgress[] = {
				{"bind", 'r'}, {"overlay", 's'}, {"indeterminate", 'b'}};
			static const NkGSchemaProp pChart[] = {
				{"values", 'l'}, {"kind", 'e'}, {"min", 'n'}, {"max", 'n'}, {"height", 'n'}};
			static const NkGSchemaProp pSeparator[] = {{"orientation", 'e'}};
			static const NkGSchemaProp pSpacer[] = {{"size", 'a'}};
			static const NkGSchemaProp pTabBar[] = {
				{"tabs", 'l'}, {"bind", 'r'}, {"editable", 'b'}, {"closable", 'b'}};
			static const NkGSchemaProp pMenu[] = {{"label", 's'}};
			static const NkGSchemaProp pExpander[] = {{"label", 's'}, {"expanded", 'b'}};
			static const NkGSchemaProp pDockSpace[] = {
				{"overViewport", 'b'}, {"topMargin", 'n'}};
			static const NkGSchemaProp pWindow[] = {{"title", 's'}, {"pos", 'v'},
													{"size", 'v'},	{"flags", 'i'},
													{"modal", 'b'}};
			static const NkGSchemaProp pPanel[] = {{"title", 's'}};
			static const NkGSchemaProp pBox[] = {
				{"gap", 'n'}, {"align", 'e'}, {"justify", 'e'}};
			static const NkGSchemaProp pGrid[] = {
				{"columns", 'n'}, {"gap", 'n'}, {"sizes", 'l'}};
			static const NkGSchemaProp pFlow[] = {{"gap", 'n'}};
			static const NkGSchemaProp pStack[] = {{"anchor", 'e'}};
			static const NkGSchemaProp pTable[] = {{"columns", 'l'}, {"flags", 'i'}};
			static const NkGSchemaProp pScroll[] = {{"axis", 'e'}, {"always", 'b'}};
			static const NkGSchemaProp pSplitter[] = {
				{"bind", 'r'}, {"min", 'n'}, {"max", 'n'}, {"orientation", 'e'}};
			static const NkGSchemaProp pNone[] = {{"", 'a'}};

			static const NkGSchemaRole kTable[] = {
				{"Button", pButton, 6},
				{"ImageButton", pImageButton, 4},
				{"MenuItem", pMenuItem, 3},
				{"TextField", pTextField, 8},
				{"NumberField", pNumberField, 5},
				{"Slider", pSlider, 5},
				{"Drag", pDrag, 6},
				{"ColorField", pColorField, 3},
				{"Checkbox", pCheckbox, 3},
				{"Switch", pSwitch, 2},
				{"RadioGroup", pRadioGroup, 3},
				{"Dropdown", pDropdown, 3},
				{"ListBox", pListBox, 3},
				{"Item", pItem, 4},
				{"TreeItem", pTreeItem, 3},
				{"Text", pText, 6},
				{"Image", pImage, 6},
				{"Progress", pProgress, 3},
				{"Chart", pChart, 5},
				{"Separator", pSeparator, 1},
				{"Spacer", pSpacer, 1},
				{"TabBar", pTabBar, 4},
				{"MenuBar", pNone, 0},
				{"Menu", pMenu, 1},
				{"ContextMenu", pNone, 0},
				{"Expander", pExpander, 2},
				{"DockSpace", pDockSpace, 2},
				{"Window", pWindow, 5},
				{"Panel", pPanel, 1},
				{"Group", pNone, 0},
				{"VBox", pBox, 3},
				{"HBox", pBox, 3},
				{"Row", pBox, 3},
				{"Column", pBox, 3},
				{"Grid", pGrid, 3},
				{"Flow", pFlow, 1},
				{"Stack", pStack, 1},
				{"Table", pTable, 2},
				{"Scroll", pScroll, 2},
				{"Splitter", pSplitter, 4},
			};
			count = sizeof(kTable) / sizeof(NkGSchemaRole);
			return kTable;
		}

		struct NkGRoleAlias {
				const char *oldName;
				const char *newName;
		};

		/// LES ALIAS -- document 7 §5, et c'est la regle qui rend le vocabulaire
		/// corrigeable : « sans alias, une renommee casse tous les documents
		/// existants, et personne ne renomme jamais rien ».
		inline const NkGRoleAlias *NkGRoleAliases(uint32 &count) {
			static const NkGRoleAlias kAliases[] = {
				{"RepeatButton", "Button"},
				{"ButtonEx", "Button"},
				{"InputText", "TextField"},
				{"InputTextEx", "TextField"},
				{"InputTextMultiline", "TextField"},
				{"InputInt", "NumberField"},
				{"InputFloat", "NumberField"},
				{"SliderFloat", "Slider"},
				{"DragFloat", "Drag"},
				{"DragInt", "Drag"},
				{"ColorEdit4", "ColorField"},
				{"ColorPicker4", "ColorField"},
				{"ColorButton", "ColorField"},
				{"CheckboxTristate", "Checkbox"},
				{"CheckBox3", "Checkbox"},
				{"Combo", "Dropdown"},
				{"BeginListBox", "ListBox"},
				{"CollapsingHeader", "Expander"},
				{"Selectable", "Item"},
				{"SelectableEditable", "Item"},
				{"SelectItem", "Item"},
				{"TreeNode", "TreeItem"},
				{"TreeNodeEditable", "TreeItem"},
				{"ProgressBar", "Progress"},
				{"PlotLines", "Chart"},
				{"PlotHistogram", "Chart"},
				{"TabBarEx", "TabBar"},
				{"TabBarEditable", "TabBar"},
				{"DockSpaceOverViewport", "DockSpace"},
			};
			count = sizeof(kAliases) / sizeof(NkGRoleAlias);
			return kAliases;
		}

		// =====================================================================
		//  LA VERIFICATION D'UNE VALEUR
		// =====================================================================
		//
		//  La lettre du schema est confrontee au KIND SYNTAXIQUE rendu par
		//  `NkGuiArchive::KindOf`. C'est exactement ce que l'ancienne version
		//  faisait avec `NkGValue::kind` -- la seule difference est que le kind est
		//  desormais CALCULE depuis le lexeme au lieu d'etre produit par
		//  l'analyseur. La mesure faite avant la bascule dit pourquoi : rien dans
		//  `NKUIDesign` ne lit les ELEMENTS d'une liste, seulement son type.

		/// NOTE -- UNE VALEUR `Invalid` N'ARRIVE JAMAIS ICI, ET C'EST UNE MUTATION
		/// SURVIVANTE QUI L'A ETABLI. Cette fonction a porte un garde
		/// `if (k == Invalid) return false;` pendant exactement une heure. La
		/// mutation qui le retirait restait VERTE a 40/40 : le seul appelant juge
		/// deja la bonne formation AVANT de chercher la propriete dans le schema,
		/// donc le garde n'etait jamais atteint.
		///
		/// >>> Deux mecanismes pour une seule question, dont un mort. Il est parti.
		///     C'est la deuxieme fois de ce chantier qu'une mutation fait RETIRER du
		///     code au lieu d'en ajouter -- et c'est toujours le meme signe : un
		///     garde qu'on ecrit « au cas ou » est un garde que personne ne mesure.
		inline bool NkGValueMatches(const NkArchiveNode &node, char kind) {
			const NkGuiValueKind k = NkGuiArchive::KindOf(node);
			switch (kind) {
				case 'a':
					return true;
				case 'n':
					return k == NkGuiValueKind::Number;
				case 's':
					return k == NkGuiValueKind::String;
				case 'v':
					return k == NkGuiValueKind::Vec2;
				case 'c':
					return k == NkGuiValueKind::Color;
				case 'l':
					return k == NkGuiValueKind::List;
				case 'd':
					return k == NkGuiValueKind::Dict;
				case 'i':
					return k == NkGuiValueKind::Ident || k == NkGuiValueKind::Flags;
				case 'e':
				case 'r':
					return k == NkGuiValueKind::Ident || k == NkGuiValueKind::String;
				case 'b': {
					// LE BOOLEEN N'EST PAS UN TYPE DU LEXIQUE (doc 2 §2) : c'est un
					// identifiant qui vaut `true` ou `false`. On le verifie donc sur
					// le TEXTE -- sinon `wrap = Vrai` passerait pour un booleen.
					if (k != NkGuiValueKind::Ident) {
						return false;
					}
					const NkStringView t = node.Lexeme();
					const NkString v(t);
					return v.Compare("true") == 0 || v.Compare("false") == 0;
				}
				default:
					return true;
			}
		}

		inline const char *NkGKindName(char kind) {
			switch (kind) {
				case 'n':
					return "un nombre";
				case 's':
					return "une chaine";
				case 'v':
					return "un Vec2";
				case 'c':
					return "une couleur";
				case 'l':
					return "une liste";
				case 'd':
					return "un dictionnaire";
				case 'i':
					return "un identifiant ou des drapeaux";
				case 'e':
					return "une etiquette (identifiant ou chaine)";
				case 'r':
					return "une reference (identifiant ou chaine)";
				case 'b':
					return "true ou false";
				default:
					return "n'importe quelle valeur";
			}
		}

		// =====================================================================
		//  LES SECTIONS CONNUES
		// =====================================================================
		//
		//  ⚠️ CETTE TABLE EXISTE PARCE QU'UN REFUS A CHANGE DE DOMICILE. L'ancien
		//     lecteur connaissait les sections et refusait `inconnue { }` dans un
		//     fichier de NOTRE version. Le lecteur syntaxique ne connait aucune
		//     section -- c'est tout son interet -- donc c'est ici que la faute
		//     redevient visible.
		//
		//  ⚠️ ET ELLE NE S'APPLIQUE PAS A UN FICHIER PLUS RECENT. Une MINEURE
		//     superieure a la notre = regle (d) : ce qu'on ne comprend pas, on le
		//     PRESERVE, on ne le signale pas comme une faute. Sans cette condition,
		//     la validation transformerait la compatibilite ascendante en erreur.
		inline bool NkGSectionConnue(const NkString &nom) {
			static const char *kSections[] = {"geometry", "widgets",	"behavior", "controller",
											  "callback", "animation", "fonts",	   "include"};
			for (uint32 i = 0; i < 8; ++i) {
				if (nom.Compare(kSections[i]) == 0) {
					return true;
				}
			}
			return false;
		}

		// =====================================================================
		//  LA VALIDATION
		// =====================================================================

		struct NkGValidateResult {
				uint32 errors = 0;
				uint32 warnings = 0;
		};

		inline const NkGSchemaRole *NkGFindRole(const NkString &role) {
			uint32 n = 0;
			const NkGSchemaRole *t = NkGRoleTable(n);
			for (uint32 i = 0; i < n; ++i) {
				if (role.Compare(t[i].role) == 0) {
					return &t[i];
				}
			}
			return nullptr;
		}

		inline const char *NkGFindAlias(const NkString &role) {
			uint32 n = 0;
			const NkGRoleAlias *t = NkGRoleAliases(n);
			for (uint32 i = 0; i < n; ++i) {
				if (role.Compare(t[i].oldName) == 0) {
					return t[i].newName;
				}
			}
			return nullptr;
		}

		inline void NkGPushDiag(NkVector<nkentseu::NkGuiDiag> &out, const char *code,
								const NkString &msg, nk_int32 line) {
			nkentseu::NkGuiDiag d;
			d.code = NkString(code);
			d.message = msg;
			// -1 = le noeud ne vient pas d'un fichier (document fabrique par le
			// code). On rend 0, que le rapport affiche comme « pas de ligne ».
			d.line = (line > 0) ? (nkentseu::nk_uint32)line : 0u;
			out.PushBack(d);
		}

		/// Le CHEMIN d'un bloc, tel qu'il s'ecrit dans un diagnostic :
		/// `widgets / VBox "v" / Button "ok"`.
		inline NkString NkGCheminEnfant(const NkString &parent, const NkArchive &bloc) {
			NkString p(parent);
			if (!p.Empty()) {
				p.Append(" / ");
			}
			p.Append(NkString(NkGuiArchive::TypeOf(bloc)));
			// L'etat fait partie du chemin : sans lui, trois `appearance` sur un
			// meme widget rendent trois diagnostics au chemin IDENTIQUE, et il faut
			// compter les lignes pour savoir lequel est en cause.
			const NkStringView st = NkGuiArchive::StateOf(bloc);
			if (st.Size() > 0) {
				p.Append('(');
				p.Append(NkString(st));
				p.Append(')');
			}
			const NkStringView id = NkGuiArchive::IdOf(bloc);
			if (id.Size() > 0) {
				p.Append(" \"");
				p.Append(NkString(id));
				p.Append('"');
			}
			return p;
		}

		/// Le `$body` d'un bloc, ou nullptr.
		inline const NkArchiveNode *NkGCorps(const NkArchive &bloc) {
			const NkArchiveNode *b = bloc.FindNode(NkStringView(NkGuiArchive::KeyBody()));
			return (b && b->IsArray()) ? b : nullptr;
		}

		// =====================================================================
		//  LE VOCABULAIRE D'APPARENCE -- DOCUMENT 9 §3, ET CE N'EST PAS UN ROLE
		// =====================================================================
		//  ⚠️ CE BLOC EST LE CORRECTIF DE TROIS FAUX POSITIFS, ET LE DEFAUT ETAIT
		//     DANS LA VALIDATION, PAS DANS LE LECTEUR. `appearance`, `fill` et
		//     `shadow` sont des constructions du document 9 §3 ; la validation les
		//     traversait comme des roles de widget et rendait `E-ROLE-INCONNU` sur
		//     chacun. L'aller-retour, lui, etait deja octet pour octet.
		//
		//  >>> CE QUE LE FICHIER `limites/01_apparence_faux_positifs.nkgui` A MONTRE,
		//      ET C'EST PLUS INTERESSANT QUE LE DEFAUT LUI-MEME : `appearance(Hover)`
		//      -- l'en-tete a parentheses, la LIMITE DECLAREE -- ne produisait AUCUNE
		//      erreur, parce qu'il reste une tranche verbatim que la validation ne
		//      regarde pas. **LA PARTIE MODELISEE CRIE, LA PARTIE NON MODELISEE SE
		//      TAIT.** Une limite qui protege d'un faux positif ne protege de rien :
		//      elle cache que c'est le faux positif qu'il fallait traiter.
		//
		//  ✅ CETTE LIMITE EST FERMEE DEPUIS LE 2026-08-27, et il a fallu DEUX
		//     choses, pas une : la liste fermee des etats (tranchee par Rodolf),
		//     et le fait que le LECTEUR modelise enfin `appearance(Etat)` au lieu
		//     de le garder en tranche verbatim. La validation n'est plus
		//     asymetrique : la meme faute est vue des deux cotes, avec le meme
		//     code, la meme ligne et le meme chemin.
		//
		//  ⚠️ ET LE MOT `Normal` A OUVERT UNE QUESTION QUE LA LISTE IMPLICITE
		//     EVITAIT : `appearance { }` et `appearance(Normal) { }` designent-ils
		//     la meme chose ? **Oui -- synonymes**, et le releve des huit outils le
		//     dit sans ambiguite : chez tous ceux qui NOMMENT le repos (Unity,
		//     Godot, WPF, Figma), **le repos nomme EST le socle** -- aucun n'a a la
		//     fois un socle et un etat de repos distincts.
		//
		//     La graphie, elle, appartient au fichier : `$state` est ABSENTE quand
		//     le fichier n'ecrit pas de parentheses. Le modele ne canonise pas,
		//     donc l'aller-retour tient pour LES DEUX graphies -- sans quoi
		//     l'ecrivain devrait en regenerer une et casserait tous les documents
		//     qui emploient l'autre.
		//
		//     Consequence directe, et elle avait besoin de son diagnostic :
		//     `appearance { }` ET `appearance(Normal) { }` sur le MEME widget sont
		//     le meme etat declare deux fois -> `W-ETAT-DOUBLE`.
		//
		//  ⚠️ LES TYPES NE SONT POSES QUE LA OU LE DOCUMENT LES POSE. Le document 9
		//     §3.2 n'annote que `offset` (Vec2), `inner` (Bool) et `backdrop` (Bool).
		//     Le reste est deduit du nom et de l'exemple §3.4 -- sauf `from` et `to`,
		//     laisses a `a` : un degrade peut aller d'une couleur a une couleur comme
		//     d'un point a un point, le document ne le dit pas, et **inventer une
		//     contrainte ici recreerait exactement le faux positif qu'on corrige**.

		/// Les propriedes portees par `appearance` LUI-MEME : celles du document 9
		/// §3.2 ligne `appearance`, plus la TYPOGRAPHIE, qui s'y pose directement.
		inline const NkGSchemaProp *NkGApparenceProps(uint32 &count) {
			static const NkGSchemaProp kProps[] = {
				{"opacity", 'n'},	 {"radius", 'n'},	  {"blend", 'e'},
				{"font", 'e'},		 {"weight", 'n'},	  {"size", 'n'},
				{"lineHeight", 'n'}, {"textAlign", 'e'},
			};
			count = 8;
			return kProps;
		}

		/// LES QUATRE EFFETS, et la liste est FERMEE (document 9 §3.1 :
		/// `effect_kind := "fill" | "stroke" | "shadow" | "blur"`). C'est ce qui
		/// permet de refuser `glow` sans le confondre avec un role de widget.
		inline const NkGSchemaRole *NkGEffetTable(uint32 &count) {
			static const NkGSchemaProp pFill[] = {{"color", 'c'}, {"gradient", 'e'},
												  {"from", 'a'},  {"to", 'a'},
												  {"angle", 'n'}, {"image", 'e'},
												  {"fit", 'e'}};
			static const NkGSchemaProp pStroke[] = {
				{"color", 'c'}, {"width", 'n'}, {"position", 'e'}, {"dash", 'l'}};
			static const NkGSchemaProp pShadow[] = {{"offset", 'v'}, {"blur", 'n'},
													{"spread", 'n'}, {"color", 'c'},
													{"inner", 'b'}};
			static const NkGSchemaProp pBlur[] = {{"radius", 'n'}, {"backdrop", 'b'}};

			static const NkGSchemaRole kTable[] = {
				{"fill", pFill, 7},
				{"stroke", pStroke, 4},
				{"shadow", pShadow, 5},
				{"blur", pBlur, 2},
			};
			count = sizeof(kTable) / sizeof(NkGSchemaRole);
			return kTable;
		}

		inline const NkGSchemaRole *NkGFindEffet(const NkString &nom) {
			uint32 n = 0;
			const NkGSchemaRole *t = NkGEffetTable(n);
			for (uint32 i = 0; i < n; ++i) {
				if (nom.Compare(t[i].role) == 0) {
					return &t[i];
				}
			}
			return nullptr;
		}

		/// Vrai si ce bloc ouvre une APPARENCE et non un widget. Un seul endroit
		/// decide, parce que l'aiguillage doit se lire d'un coup d'oeil.
		inline bool NkGEstApparence(const NkString &nom) {
			return nom.Compare("appearance") == 0;
		}

		// =====================================================================
		//  LA LISTE FERMEE DES ETATS -- doc 9 §3.2bis, tranchee le 2026-08-27
		// =====================================================================
		//  Relevee sur huit outils avant d'etre figee, parce qu'un nom d'etat est
		//  un nom que les utilisateurs apprendront, et qu'ils arrivent ici en
		//  connaissant deja un autre outil. Ce qui est DEHORS compte autant que ce
		//  qui est dedans :
		//
		//   - `Idle` : n'existe dans AUCUN des huit. Ecarte.
		//   - `Active` : **quatre sens documentes et incompatibles** (CSS = presse,
		//     Qt = la fenetre est active, eBay = destination courante, SwiftUI =
		//     la fenetre). `Pressed` dit le sens utile sans le piege.
		//   - `Default` : repos chez Figma/Spectrum, mais **le bouton par defaut
		//     d'un dialogue** en CSS et en Qt.
		//   - `Checked`, `Selected`, `ReadOnly`, `Error` : ce sont des **DONNEES**
		//     du composant, pas des etats d'interaction. CSS les separe
		//     normativement (*input* contre *user action*), Material ne leur donne
		//     pas de *state layer*, et le document les declare DEJA (`tristate`,
		//     `enabled`, `bind`). Qt, qui ne trace pas cette ligne, en a ~45 --
		//     dont des POSITIONS dans une structure (`:first`, `:only-one`).
		//
		//  ⚠️ `Disabled` EST UNE ENTORSE ASSUMEE A CETTE REGLE, ET ELLE RESTE
		//     ECRITE. Par la ligne ci-dessus il devrait etre dehors : MDN le range
		//     avec `:checked`, Material ne lui accorde pas de *state layer*. Il est
		//     dedans parce qu'Unity, Godot, Qt, Figma, Flutter, WPF et Spectrum le
		//     stylent tous, et que tout le monde cherchera `appearance(Disabled)`
		//     en premier. **Le jour ou quelqu'un demandera pourquoi `Checked` n'y
		//     est pas, la reponse est ici** -- et elle doit rester lisible.
		//
		//  ⚠️ CE QUE CETTE LISTE NE REGLE PAS : la COMBINAISON (`Hover` ET
		//     `Pressed`) et le FOCUS CLAVIER (`:focus-visible`). Material n'a
		//     jamais tranche la premiere ; Godot a paye `hover_pressed`. Les deux
		//     attendent une decision, et **rien ici ne les prejuge**.
		inline bool NkGEtatConnu(const NkString &etat) {
			static const char *kEtats[] = {"Normal", "Hover", "Pressed", "Focus",
										   "Disabled"};
			for (uint32 i = 0; i < 5; ++i) {
				if (etat.Compare(kEtats[i]) == 0) {
					return true;
				}
			}
			return false;
		}

		/// L'etat d'un bloc d'apparence, **repos compris**. Une apparence sans
		/// parentheses EST `Normal` : c'est ce qui fait des deux graphies des
		/// synonymes, et c'est le seul endroit qui le dit.
		inline NkString NkGEtatDe(const NkArchive &bloc) {
			const NkStringView e = NkGuiArchive::StateOf(bloc);
			return e.Size() > 0 ? NkString(e) : NkString("Normal");
		}

		// =====================================================================
		//  LES PROPRIETES D'UN BLOC, JUGEES CONTRE UNE TABLE -- UN SEUL ENDROIT
		// =====================================================================
		//  ⚠️ CE SERVICE EXISTE PARCE QUE LE FORMAT A **DEUX** VOCABULAIRES, PAS UN.
		//     Les widgets (document 7 §3) et l'apparence (document 9 §3) sont deux
		//     domaines distincts, mais la facon de juger une propriete est la meme
		//     dans les deux : la valeur mal formee d'abord, le nom ensuite, le type
		//     enfin.
		//
		//     Le RECOPIER pour l'apparence aurait ete tomber exactement dans le piege
		//     que la mutation V6 a deja puni le 2026-08-23 : **un mecanisme present a
		//     plusieurs endroits doit etre mesure a chacun**, et deux copies se
		//     desynchronisent en silence. Une seule copie, deux appelants -- et les
		//     controles 22 et 22c, qui franchissent les DEUX portes du diagnostic de
		//     propriete, valent desormais pour l'apparence aussi.
		//
		//  `universels` vaut nullptr pour l'apparence : `tooltip` et `enabled` sont
		//  des capacites transversales du WIDGET (document 7 §4), elles n'ont aucun
		//  sens dans un bloc `fill`. Les admettre partout « pour simplifier » aurait
		//  rendu la table d'apparence plus permissive que le document.
		inline void NkGValidateProps(const NkArchive &noeud, const NkString &chemin,
									 const NkGSchemaProp *props, uint32 count,
									 const NkGSchemaProp *universels, uint32 un,
									 const NkString &nomDuRole,
									 NkVector<nkentseu::NkGuiDiag> &out) {
			const NkGSchemaProp *uni = universels;
			const NkVector<nkentseu::NkArchiveEntry> &ents = noeud.Entries();
			for (uint32 p = 0; p < (uint32)ents.Size(); ++p) {
				if (NkGuiArchive::IsReservedKey(NkStringView(ents[p].key))) {
					continue;  // `$type`, `$id`, `$body`, `$layout` : de la syntaxe
				}
				const NkString &nom = ents[p].key;

				// ⚠️ UNE VALEUR MAL FORMEE EST JUGEE AVANT TOUTE QUESTION DE
				//    SCHEMA, ET C'EST UNE MESURE QUI L'A IMPOSE. La premiere
				//    version cherchait d'abord la propriete dans le schema et
				//    passait a la suivante si elle n'y etait pas -- si bien que
				//    `color = #12345` sur un role qui n'a pas de `color` ne
				//    produisait QUE « propriete hors schema » : la couleur a cinq
				//    chiffres n'etait plus signalee nulle part. Le controle 3b l'a
				//    vu, et il avait ete ecrit exactement pour ca.
				//
				//    Une valeur qui ne correspond a AUCUNE forme du format est une
				//    faute LEXICALE. Elle ne depend d'aucun role, donc elle ne doit
				//    dependre d'aucune recherche de role.
				if (NkGuiArchive::KindOf(ents[p].node) == NkGuiValueKind::Invalid) {
					NkString m(chemin);
					m.Append(" . ");
					m.Append(nom);
					m.Append(" : valeur mal formee -- '");
					m.Append(NkString(ents[p].node.Lexeme()));
					m.Append("' n'est aucune des formes de valeur du format");
					NkGPushDiag(out, "E-VALEUR", m, ents[p].node.SourceLine());
					continue;
				}

				const NkGSchemaProp *found = nullptr;
				for (uint32 k = 0; k < count && !found; ++k) {
					if (nom.Compare(props[k].name) == 0) {
						found = &props[k];
					}
				}
				for (uint32 k = 0; k < un && !found; ++k) {
					if (nom.Compare(uni[k].name) == 0) {
						found = &uni[k];
					}
				}
				if (!found) {
					NkString m(chemin);
					m.Append(" . ");
					m.Append(nom);
					m.Append(" : propriete absente du schema du role '");
					m.Append(nomDuRole);
					m.Append("'");
					NkGPushDiag(out, "E-TYPE", m, ents[p].node.SourceLine());
					continue;
				}
				if (!NkGValueMatches(ents[p].node, found->kind)) {
					const NkGuiValueKind k = NkGuiArchive::KindOf(ents[p].node);
					NkString m(chemin);
					m.Append(" . ");
					m.Append(nom);
					m.Append(" : ");
					m.Append(NkGKindName(found->kind));
					m.Append(" attendu, lu ");
					m.Append(NkGuiArchive::KindName(k));
					// `E-VALEUR` quand la valeur n'est bien formee pour AUCUN type
					// -- c'est un des quatre refus qui ont change de domicile ;
					// `E-TYPE` quand elle est bien formee mais du mauvais type.
					NkGPushDiag(out, k == NkGuiValueKind::Invalid ? "E-VALEUR" : "E-TYPE", m,
								ents[p].node.SourceLine());
				}
			}
		}

		/// Valide un bloc `appearance` et les effets qu'il contient.
		///
		/// ⚠️ IL NE RAPPELLE PAS `NkGValidateNode`, ET C'EST LA GRAMMAIRE QUI LE DIT :
		///    `appearance_member := prop_decl | effect_blk` (document 9 §3.1). Un
		///    widget ne peut pas vivre dans une apparence, un effet ne contient que
		///    des proprietes. Aiguiller vers la validation de widget « au cas ou »
		///    aurait rendu `appearance { Button "x" { } }` legal, ce que le document
		///    ne dit nulle part.
		inline void NkGValidateApparence(const NkArchive &bloc, const NkString &parent,
										 nk_int32 ligne, NkVector<nkentseu::NkGuiDiag> &out) {
			const NkString chemin = NkGCheminEnfant(parent, bloc);

			// L'ETAT, contre la liste fermee. Il est juge AVANT le contenu : un
			// etat inconnu ne rend pas le bloc illisible, et son contenu reste
			// verifiable -- on doit pouvoir corriger la faute qu'on signale.
			const NkStringView brut = NkGuiArchive::StateOf(bloc);
			if (brut.Size() > 0 && !NkGEtatConnu(NkString(brut))) {
				NkString m(chemin);
				m.Append(" : etat inconnu '");
				m.Append(NkString(brut));
				m.Append("'. Les etats sont Normal, Hover, Pressed, Focus, Disabled");
				NkGPushDiag(out, "E-ETAT-INCONNU", m, ligne);
			}

			uint32 an = 0;
			const NkGSchemaProp *ap = NkGApparenceProps(an);
			NkGValidateProps(bloc, chemin, ap, an, nullptr, 0, NkString("appearance"), out);

			const NkArchiveNode *corps = NkGCorps(bloc);
			if (!corps) {
				return;
			}
			for (uint32 c = 0; c < (uint32)corps->array.Size(); ++c) {
				if (!corps->array[c].IsObject() || !corps->array[c].object) {
					continue;  // une tranche brute : passee, comme partout ailleurs
				}
				const NkArchive &eff = *corps->array[c].object;
				const NkString nom(NkGuiArchive::TypeOf(eff));
				const NkString cheminEff = NkGCheminEnfant(chemin, eff);
				const NkGSchemaRole *def = NkGFindEffet(nom);
				if (!def) {
					// ⚠️ UN CODE A LUI, PAS `E-ROLE-INCONNU`. Dire « role inconnu »
					//    dans une apparence serait refaire, en plus discret, l'erreur
					//    qu'on corrige : ce n'est pas le vocabulaire des roles qu'on
					//    consulte ici, donc ce n'est pas de lui qu'il faut se plaindre.
					NkString m(cheminEff);
					m.Append(" : effet inconnu -- '");
					m.Append(nom);
					m.Append("' n'est pas un effet d'apparence (document 9 §3.1 : "
							 "fill, stroke, shadow, blur)");
					NkGPushDiag(out, "E-EFFET-INCONNU", m, corps->array[c].SourceLine());
					continue;
				}
				NkGValidateProps(eff, cheminEff, def->props, def->count, nullptr, 0, nom, out);

				// Un effet ne contient QUE des proprietes. Un bloc a l'interieur est
				// une faute, et la taire rendrait `fill { shadow { } }` legal.
				const NkArchiveNode *dedans = NkGCorps(eff);
				if (!dedans) {
					continue;
				}
				for (uint32 k = 0; k < (uint32)dedans->array.Size(); ++k) {
					if (!dedans->array[k].IsObject() || !dedans->array[k].object) {
						continue;
					}
					NkString m(NkGCheminEnfant(cheminEff, *dedans->array[k].object));
					m.Append(" : un effet d'apparence ne contient que des proprietes "
							 "(document 9 §3.1)");
					NkGPushDiag(out, "E-EFFET-INCONNU", m, dedans->array[k].SourceLine());
				}
			}
		}

		/// ⚠️ LA LIGNE D'UN BLOC VIT SUR LE NOEUD QUI LE PORTE, pas dans l'archive
		///    du bloc. Un bloc est un element du `$body` de son parent : c'est cet
		///    element qui a une trivia, donc une ligne. L'archive interieure, elle,
		///    n'est qu'une table de membres. Le parametre `ligne` existe pour ca --
		///    l'oublier redonnerait des diagnostics a la ligne 0 sans que rien ne
		///    tombe, puisque le message reste juste.
		inline void NkGValidateNode(const NkArchive &noeud, const NkString &parent,
									nk_int32 ligne, NkVector<nkentseu::NkGuiDiag> &out) {
			const NkString role(NkGuiArchive::TypeOf(noeud));
			const NkString chemin = NkGCheminEnfant(parent, noeud);

			const NkGSchemaRole *def = NkGFindRole(role);
			if (!def) {
				const char *alias = NkGFindAlias(role);
				if (alias) {
					NkString m(chemin);
					m.Append(" : role '");
					m.Append(role);
					m.Append("' -- ancien nom, encore lu. Le vocabulaire du document 7 dit '");
					m.Append(alias);
					m.Append("'");
					NkGPushDiag(out, "W-ROLE-ALIAS", m, ligne);
					def = NkGFindRole(NkString(alias));
				} else {
					// UNE ERREUR NOMMEE, PAS UN REJET MUET DU FICHIER. Le document
					// reste lisible, modifiable et enregistrable : c'est la seule
					// facon de pouvoir CORRIGER la faute qu'on signale.
					NkString m(chemin);
					m.Append(" : role inconnu -- '");
					m.Append(role);
					m.Append("' n'est pas dans le vocabulaire NkUI (document 7 §3)");
					NkGPushDiag(out, "E-ROLE-INCONNU", m, ligne);
				}
			}

			if (def) {
				uint32 un = 0;
				const NkGSchemaProp *uni = NkGUniversalProps(un);
				NkGValidateProps(noeud, chemin, def->props, def->count, uni, un,
								 NkString(def->role), out);
			}

			// Les enfants. Une TRANCHE BRUTE (element scalaire du corps) n'est pas
			// un noeud : elle n'est pas jugee, elle est passee.
			const NkArchiveNode *corps = NkGCorps(noeud);
			if (!corps) {
				return;
			}
			// Les etats deja rencontres SUR CE WIDGET. Locale a la boucle : un
			// widget enfant repart d'une liste vide, ses etats sont les siens.
			NkVector<NkString> etatsVus;
			for (uint32 c = 0; c < (uint32)corps->array.Size(); ++c) {
				if (!corps->array[c].IsObject() || !corps->array[c].object) {
					continue;
				}
				const NkArchive &enfant = *corps->array[c].object;
				// ⚠️ `appearance` N'EST PAS UN ROLE, C'EST UN MEMBRE DU NOEUD
				//    (document 9 §3.1). Sans cet aiguillage, il partait dans la
				//    validation de widget et en ressortait avec `E-ROLE-INCONNU`,
				//    lui et chacun de ses effets -- trois faux positifs sur un
				//    fichier parfaitement legal.
				if (NkGEstApparence(NkString(NkGuiArchive::TypeOf(enfant)))) {
					// ⚠️ LE MEME ETAT DECLARE DEUX FOIS. Consequence directe du fait
					//    que `appearance { }` et `appearance(Normal) { }` sont
					//    SYNONYMES : sans ce controle, les deux graphies cohabitent
					//    sur un widget sans que rien ne le dise. Mesure du
					//    2026-08-27 : un fichier qui les cumule passait a 0 erreur.
					//
					//    AVERTISSEMENT et non erreur, deliberement : dire laquelle
					//    des deux gagne serait trancher la question de la
					//    COMBINAISON des etats, qui ne nous appartient pas. On
					//    signale la redondance sans prejuger de la precedence.
					const NkString etat = NkGEtatDe(enfant);
					bool deja = false;
					for (uint32 v = 0; v < (uint32)etatsVus.Size(); ++v) {
						if (etatsVus[v].Compare(etat) == 0) {
							deja = true;
							break;
						}
					}
					if (deja) {
						NkString m(chemin);
						m.Append(" : l'etat '");
						m.Append(etat);
						m.Append("' est declare deux fois");
						if (NkGuiArchive::StateOf(enfant).Size() == 0
							|| etat.Compare("Normal") == 0) {
							m.Append(" (`appearance` et `appearance(Normal)` sont le "
									 "meme etat)");
						}
						NkGPushDiag(out, "W-ETAT-DOUBLE", m, corps->array[c].SourceLine());
					} else {
						etatsVus.PushBack(etat);
					}
					NkGValidateApparence(enfant, chemin, corps->array[c].SourceLine(), out);
					continue;
				}
				NkGValidateNode(enfant, chemin, corps->array[c].SourceLine(), out);
			}
		}

		/// Valide TOUT le document. Ne modifie rien : l'archive reste exactement
		/// celle qui a ete lue, donc l'aller-retour est intact meme sur un document
		/// que la validation refuse.
		inline NkGValidateResult NkGValidate(const NkArchive &doc,
											 NkVector<nkentseu::NkGuiDiag> &out) {
			NkGValidateResult r;
			// Regle (d) : un fichier d'une MINEURE plus recente peut porter des
			// sections que nous ne connaissons pas. Les signaler serait transformer
			// la compatibilite ascendante en erreur.
			const nkentseu::NkSchemaVersion v = NkGuiArchive::VersionOf(doc);
			const bool plusRecent = (v.major > (nkentseu::nk_uint16)NkGuiArchive::kMajor)
									|| (v.major == (nkentseu::nk_uint16)NkGuiArchive::kMajor
										&& v.minor > (nkentseu::nk_uint16)NkGuiArchive::kMinor);

			const NkArchiveNode *corps = NkGCorps(doc);
			if (corps) {
				for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
					if (!corps->array[i].IsObject() || !corps->array[i].object) {
						continue;  // une construction non modelisee : passee telle quelle
					}
					const NkArchive &sec = *corps->array[i].object;
					const NkString nom(NkGuiArchive::TypeOf(sec));
					if (!NkGSectionConnue(nom) && !plusRecent) {
						NkString m("section inconnue : '");
						m.Append(nom);
						m.Append("' n'est pas une section du format, et ce fichier n'est pas "
								 "d'une version plus recente que la mienne");
						NkGPushDiag(out, "E-SECTION-INCONNUE", m,
									corps->array[i].SourceLine());
						continue;
					}
					if (nom.Compare("widgets") != 0) {
						continue;
					}
					const NkArchiveNode *racines = NkGCorps(sec);
					if (!racines) {
						continue;
					}
					for (uint32 k = 0; k < (uint32)racines->array.Size(); ++k) {
						if (racines->array[k].IsObject() && racines->array[k].object) {
							NkGValidateNode(*racines->array[k].object, NkString("widgets"),
											racines->array[k].SourceLine(), out);
						}
					}
				}
			}
			for (uint32 i = 0; i < (uint32)out.Size(); ++i) {
				if (out[i].code.StartsWith("E-")) {
					++r.errors;
				} else {
					++r.warnings;
				}
			}
			return r;
		}

	}  // namespace guifmt
}  // namespace nkuidesign

#endif	// __NKENTSEU_NKUIDESIGN_NKGUIVALIDATE_H__
