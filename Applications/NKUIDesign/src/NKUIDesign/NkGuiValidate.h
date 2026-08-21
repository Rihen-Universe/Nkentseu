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
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKUIDESIGN_NKGUIVALIDATE_H__
#define __NKENTSEU_NKUIDESIGN_NKGUIVALIDATE_H__

#include "NkGuiFormat.h"

namespace nkuidesign {
	namespace guifmt {

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

		inline bool NkGValueMatches(const NkGValue &v, char kind) {
			switch (kind) {
				case 'a':
					return true;
				case 'n':
					return v.kind == NkGValueKind::Number;
				case 's':
					return v.kind == NkGValueKind::String;
				case 'v':
					return v.kind == NkGValueKind::Vec2;
				case 'c':
					return v.kind == NkGValueKind::Color;
				case 'l':
					return v.kind == NkGValueKind::List;
				case 'd':
					return v.kind == NkGValueKind::Dict;
				case 'i':
					return v.kind == NkGValueKind::Ident || v.kind == NkGValueKind::Flags;
				case 'e':
				case 'r':
					return v.kind == NkGValueKind::Ident || v.kind == NkGValueKind::String;
				case 'b':
					// LE BOOLEEN N'EST PAS UN TYPE DU LEXIQUE (doc 2 §2) : c'est un
					// identifiant qui vaut `true` ou `false`. On le verifie donc sur
					// le TEXTE -- sinon `wrap = Vrai` passerait pour un booleen.
					return v.kind == NkGValueKind::Ident
						   && (v.text.Compare("true") == 0 || v.text.Compare("false") == 0);
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

		inline const char *NkGValueKindName(NkGValueKind k) {
			switch (k) {
				case NkGValueKind::String:
					return "chaine";
				case NkGValueKind::Number:
					return "nombre";
				case NkGValueKind::Color:
					return "couleur";
				case NkGValueKind::Vec2:
					return "Vec2";
				case NkGValueKind::Ident:
					return "identifiant";
				case NkGValueKind::Flags:
					return "drapeaux";
				case NkGValueKind::List:
					return "liste";
				case NkGValueKind::Dict:
					return "dictionnaire";
				default:
					return "rien";
			}
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

		inline void NkGPushDiag(NkVector<NkGDiag> &out, const char *code, const NkString &msg,
								uint32 line) {
			NkGDiag d;
			d.code = NkString(code);
			d.message = msg;
			d.line = line;
			out.PushBack(d);
		}

		inline void NkGValidateNode(const NkGDocument &doc, uint32 idx, NkVector<NkGDiag> &out) {
			const NkGNode &n = doc.nodes[idx];

			const NkGSchemaRole *def = NkGFindRole(n.kind);
			if (!def) {
				const char *alias = NkGFindAlias(n.kind);
				if (alias) {
					NkString m("role '");
					m.Append(n.kind);
					m.Append("' : ancien nom, encore lu. Le vocabulaire du document 7 dit '");
					m.Append(alias);
					m.Append("'");
					NkGPushDiag(out, "W-ROLE-ALIAS", m, n.line);
					def = NkGFindRole(NkString(alias));
				} else {
					// UNE ERREUR NOMMEE, PAS UN REJET MUET DU FICHIER. Le document
					// reste lisible, modifiable et enregistrable : c'est la seule
					// facon de pouvoir CORRIGER la faute qu'on signale.
					NkString m("role inconnu : '");
					m.Append(n.kind);
					m.Append("' n'est pas dans le vocabulaire NkUI (document 7 §3)");
					NkGPushDiag(out, "E-ROLE-INCONNU", m, n.line);
				}
			}

			if (def) {
				uint32 un = 0;
				const NkGSchemaProp *uni = NkGUniversalProps(un);
				for (uint32 p = 0; p < (uint32)n.props.Size(); ++p) {
					const NkGProp &prop = n.props[p];
					const NkGSchemaProp *found = nullptr;
					for (uint32 k = 0; k < def->count && !found; ++k) {
						if (prop.name.Compare(def->props[k].name) == 0) {
							found = &def->props[k];
						}
					}
					for (uint32 k = 0; k < un && !found; ++k) {
						if (prop.name.Compare(uni[k].name) == 0) {
							found = &uni[k];
						}
					}
					if (!found) {
						NkString m("propriete '");
						m.Append(prop.name);
						m.Append("' absente du schema du role '");
						m.Append(def->role);
						m.Append("'");
						NkGPushDiag(out, "E-TYPE", m, prop.line);
						continue;
					}
					if (!NkGValueMatches(prop.value, found->kind)) {
						NkString m("propriete '");
						m.Append(prop.name);
						m.Append("' du role '");
						m.Append(def->role);
						m.Append("' : ");
						m.Append(NkGKindName(found->kind));
						m.Append(" attendu, lu ");
						m.Append(NkGValueKindName(prop.value.kind));
						NkGPushDiag(out, "E-TYPE", m, prop.line);
					}
				}
			}

			for (uint32 c = 0; c < (uint32)n.children.Size(); ++c) {
				NkGValidateNode(doc, n.children[c], out);
			}
		}

		/// Valide TOUT le document. Ne modifie rien : le modele reste exactement
		/// celui qui a ete lu, donc l'aller-retour est intact meme sur un document
		/// que la validation refuse.
		inline NkGValidateResult NkGValidate(const NkGDocument &doc, NkVector<NkGDiag> &out) {
			NkGValidateResult r;
			for (uint32 s = 0; s < (uint32)doc.sections.Size(); ++s) {
				const NkGSection &sec = doc.sections[s];
				if (sec.kind != NkGSectionKind::Widgets) {
					continue;
				}
				const NkGTopSection &ts = doc.topSections[sec.index];
				for (uint32 i = 0; i < (uint32)ts.roots.Size(); ++i) {
					NkGValidateNode(doc, ts.roots[i], out);
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
