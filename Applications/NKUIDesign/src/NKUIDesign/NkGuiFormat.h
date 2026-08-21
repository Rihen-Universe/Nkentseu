//
// NkGuiFormat.h
// =============================================================================
// Description :
//   Le LECTEUR et l'ECRIVAIN du format `.nkgui` v0.2, tel que le document 2
//   (`2_NkUIDesign_Langage_Description_NodeBlueprint.md`) le definit -- lexeur,
//   analyseur descendant recursif, modele de document en memoire, et
//   serialiseur. C'est ce qui manquait pour que NkUIDesign puisse ENREGISTRER
//   et RELIRE ses documents (inventaire, document 8 §5 : « chemin critique »).
//
// Caracteristiques :
//   - zero-STL : `NkString` / `NkVector` (NKContainers), allocateurs NKMemory ;
//   - ARENES PLATES : tous les noeuds, expressions et instructions vivent dans
//     des `NkVector` du document, references par INDICE. Aucun pointeur vers
//     l'interieur d'un conteneur qui peut se reallouer ;
//   - ORDRE D'ECRITURE PRESERVE : un noeud garde la suite exacte de ses membres
//     (propriete / evenement / enfant), et le fichier garde la suite exacte de
//     ses sections. Sans ca, l'aller-retour reordonne le document ;
//   - LEXEMES CONSERVES : un nombre, une couleur, un vecteur et un identifiant
//     sont reemis TELS QU'ILS ONT ETE LUS. Le document 2 ne definit aucune forme
//     canonique pour eux ; normaliser reecrirait le document de l'auteur.
//
// Algorithmes implementes :
//   - analyse descendante recursive a un seul jeton d'avance (LL(1)) ;
//   - analyse des expressions par PRECEDENCE GRIMPANTE (precedence climbing),
//     sur les operateurs du document 2 §3.
//
// ⚠️ PORTEE, et elle est etroite -- a lire avant de s'en servir :
//    ce fichier fait la validation SYNTAXIQUE (`E-PARSE`) et RIEN d'autre. La
//    validation par role (`E-TYPE`, doc 2 §4) exige la table §8, que le
//    document 7 propose justement de remplacer : la coder aujourd'hui, ce serait
//    trancher a la place de Rodolf. Detail et manques releves :
//    `design/9_Grammaire_complete.md` §6.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKUIDESIGN_NKGUIFORMAT_H__
#define __NKENTSEU_NKUIDESIGN_NKGUIFORMAT_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkuidesign {
	namespace guifmt {

		using nkentseu::float64;
		using nkentseu::int32;
		using nkentseu::NkString;
		using nkentseu::NkVector;
		using nkentseu::uint32;
		using nkentseu::uint8;

		/// Indice « aucun » des arenes. `uint32` plutot qu'un pointeur : les arenes
		/// se reallouent, un pointeur vers leur interieur ne survivrait pas au
		/// premier `PushBack`.
		static const uint32 kNoIndex = 0xFFFFFFFFu;

		// ═══════════════════════════════════════════════════════════════════════
		//  LES VALEURS  (doc 2 §3 : value := String | Number | Color | Vec2
		//                                  | flags | Identifier)
		// ═══════════════════════════════════════════════════════════════════════

		enum class NkGValueKind : uint8 {
			None = 0,
			String,	 ///< `"..."` — `text` porte le contenu DECODE
			Number,	 ///< `-?[0-9]+(\.[0-9]+)?`
			Color,	 ///< `#RRGGBB` ou `#RRGGBBAA`
			Vec2,	 ///< `(x, y)`
			Ident,	 ///< un identifiant seul (`true`, `false`, `EaseOut`, `Enum.X`)
			Flags	 ///< `A | B | C`
		};

		/// ⚠️ DEUX FORMES COHABITENT, ET C'EST VOULU : `text` est la forme LUE
		///    (contenu decode d'une chaine), `raw` est la forme ECRITE (le lexeme
		///    source, guillemets compris pour une chaine).
		///
		///    Le serialiseur reemet `raw` pour tout sauf les chaines, qu'il
		///    re-encode depuis `text`. Raison : le document 2 ne definit aucune
		///    forme canonique pour un nombre (`0.20` vaut `0.2`), une couleur
		///    (`#ff0000` vaut `#FF0000`) ni un vecteur (`(1,2)` vaut `(1, 2)`).
		///    **Normaliser reecrirait silencieusement le fichier de l'auteur** —
		///    un diff de trois cents lignes le lendemain d'un simple
		///    enregistrement, sans qu'aucune valeur n'ait change.
		struct NkGValue {
				NkGValueKind kind = NkGValueKind::None;
				NkString text;	 ///< String : contenu decode. Ident/Flags : le texte.
				NkString raw;	 ///< le lexeme source, tel quel
				float64 num = 0.0;	  ///< Number
				uint32 color = 0;	  ///< Color, en RGBA8
				float64 vx = 0.0;	  ///< Vec2
				float64 vy = 0.0;	  ///< Vec2
		};

		struct NkGProp {
				NkString name;
				NkGValue value;
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LES EXPRESSIONS  (doc 2 §3, section behavior)
		// ═══════════════════════════════════════════════════════════════════════

		enum class NkGExprKind : uint8 {
			Literal = 0,  ///< String | Number | Color | true | false
			Ident,		  ///< un identifiant, pointe ou non (cf. §6.2 du doc 9)
			Binary,		  ///< `lhs op rhs`
			Paren		  ///< `( inner )` — conserve pour reemettre le groupement
		};

		struct NkGExpr {
				NkGExprKind kind = NkGExprKind::Literal;
				NkGValue literal;	 ///< Literal
				NkString ident;		 ///< Ident
				NkString op;		 ///< Binary
				uint32 lhs = kNoIndex;
				uint32 rhs = kNoIndex;	 ///< Paren : reutilise `lhs`
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LES INSTRUCTIONS  (doc 2 §5)
		// ═══════════════════════════════════════════════════════════════════════

		enum class NkGStmtKind : uint8 {
			Assign = 0,	 ///< `set Identifier = expr`
			If,			 ///< `if expr { ... } else { ... }`
			Call		 ///< `Callback "X"(args)`
		};

		struct NkGStmt {
				NkGStmtKind kind = NkGStmtKind::Assign;
				NkString name;			  ///< Assign : la variable. Call : le nom du callback.
				uint32 expr = kNoIndex;	  ///< Assign : la valeur. If : la condition.
				NkVector<uint32> args;	  ///< Call
				NkVector<uint32> thenStmts;
				NkVector<uint32> elseStmts;
				bool hasElse = false;
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LES EVENEMENTS  (doc 2 §3 : event_decl)
		// ═══════════════════════════════════════════════════════════════════════

		enum class NkGActionKind : uint8 {
			Callback = 0,  ///< `Callback "X"(args)`
			Behavior,	   ///< `Behavior "X"`
			Inline		   ///< `{ statement* }`
		};

		struct NkGEvent {
				NkString name;				 ///< l'identifiant apres `on`
				NkVector<NkString> params;	 ///< `(value, text)`
				bool hasParams = false;		 ///< distingue `on Click` de `on Click()`
				NkGActionKind action = NkGActionKind::Callback;
				NkString target;		 ///< nom du callback ou du behavior
				NkVector<uint32> args;	 ///< Callback
				NkVector<uint32> stmts;	 ///< Inline
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LES NOEUDS DE WIDGETS  (doc 2 §3 : node_decl)
		// ═══════════════════════════════════════════════════════════════════════

		/// ⚠️ CE TYPE EXISTE POUR UNE SEULE RAISON, ET ELLE EST L'ALLER-RETOUR.
		///    `node_decl := Kind String? '{' (prop_decl | event_decl | node_decl)* '}'`
		///    autorise les trois membres dans N'IMPORTE QUEL ORDRE. Les ranger dans
		///    trois listes separees et les reemettre « proprietes d'abord » suffit
		///    a rendre le fichier different de l'original — et un outil qui
		///    reordonne un document a chaque enregistrement produit des diffs que
		///    personne ne peut relire.
		enum class NkGMemberKind : uint8 { Prop = 0, Event, Child };

		struct NkGMember {
				NkGMemberKind kind = NkGMemberKind::Prop;
				uint32 index = kNoIndex;  ///< indice dans props / events / children
		};

		struct NkGNode {
				NkString kind;	 ///< `Kind` — n'importe quel identifiant (cf. doc 9 §6.4)
				NkString id;	 ///< le `String?` optionnel
				bool hasId = false;
				NkVector<NkGProp> props;
				NkVector<NkGEvent> events;
				NkVector<uint32> children;	 ///< indices dans `NkGDocument::nodes`
				NkVector<NkGMember> members;	 ///< l'ordre du fichier
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LA GEOMETRIE  (doc 2 §3 : geometry_sec)
		// ═══════════════════════════════════════════════════════════════════════

		struct NkGShape {
				NkString name;
				NkVector<NkGProp> props;
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LE COMPORTEMENT  (doc 2 §5 et §6)
		// ═══════════════════════════════════════════════════════════════════════

		struct NkGGraphNode {
				NkString name;	 ///< `node <name> <type>`
				NkString type;
				NkVector<NkGProp> pins;	 ///< pin_init, valeurs = expressions
				NkVector<uint32> pinExprs;	 ///< indices dans `NkGDocument::exprs`
				bool hasPins = false;		 ///< distingue `node n1 X` de `node n1 X { }`
		};

		struct NkGWire {
				NkVector<NkString> refs;  ///< `a.b -> c.d -> e.f`, en texte tel que lu
		};

		struct NkGBehavior {
				NkString name;
				bool isGraph = false;
				NkVector<uint32> stmts;	 ///< script
				NkVector<NkGGraphNode> gnodes;
				NkVector<NkGWire> wires;
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LES CONTRATS  (doc 2 §10)
		// ═══════════════════════════════════════════════════════════════════════

		struct NkGParam {
				NkString name;
				NkString type;					///< `Void`|`Bool`|... tel que lu
				NkVector<NkString> enumLabels;	///< `Enum[X,Y,Z]`
				bool isEnum = false;
		};

		struct NkGCallbackSig {
				NkString name;
				NkVector<NkGParam> params;
				NkString ret;
				NkVector<NkString> retEnumLabels;
				bool retIsEnum = false;
		};

		struct NkGController {
				NkString name;
				NkVector<uint32> callbacks;	 ///< indices dans `NkGDocument::callbacks`
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LE DOCUMENT
		// ═══════════════════════════════════════════════════════════════════════

		enum class NkGSectionKind : uint8 { Geometry = 0, Widgets, Behavior, Controller, Callback };

		struct NkGSection {
				NkGSectionKind kind = NkGSectionKind::Widgets;
				uint32 index = kNoIndex;  ///< indice dans l'arene correspondante
		};

		/// Une section `geometry` ou `widgets` : ses membres de premier niveau.
		struct NkGTopSection {
				NkVector<uint32> shapes;	///< Geometry : indices dans `shapes`
				NkVector<uint32> roots;		///< Widgets : indices dans `nodes`
		};

		struct NkGDocument {
				NkString versionMajor = NkString("0");  ///< lexeme, pas un nombre
				NkString versionMinor = NkString("2");
				NkVector<NkString> includes;

				// ── Les arenes ────────────────────────────────────────────────
				NkVector<NkGNode> nodes;
				NkVector<NkGShape> shapes;
				NkVector<NkGExpr> exprs;
				NkVector<NkGStmt> stmts;
				NkVector<NkGBehavior> behaviors;
				NkVector<NkGCallbackSig> callbacks;
				NkVector<NkGController> controllers;
				NkVector<NkGTopSection> topSections;

				/// L'ORDRE DES SECTIONS DU FICHIER. Meme raison que `NkGNode::members`.
				NkVector<NkGSection> sections;

				void Clear() {
					versionMajor = NkString("0");
					versionMinor = NkString("2");
					includes.Clear();
					nodes.Clear();
					shapes.Clear();
					exprs.Clear();
					stmts.Clear();
					behaviors.Clear();
					callbacks.Clear();
					controllers.Clear();
					topSections.Clear();
					sections.Clear();
				}
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LES DIAGNOSTICS  (doc 2 §12)
		// ═══════════════════════════════════════════════════════════════════════

		struct NkGDiag {
				NkString code;	 ///< `E-PARSE`, ...
				NkString message;
				uint32 line = 0;
				uint32 column = 0;
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LE LEXEUR  (doc 2 §2)
		// ═══════════════════════════════════════════════════════════════════════

		enum class NkGTok : uint8 {
			End = 0,
			Ident,
			String,
			Number,
			Color,
			LBrace,
			RBrace,
			LParen,
			RParen,
			LBracket,  ///< jamais produit par la grammaire v0.2 — refuse (doc 9 §6.1)
			RBracket,
			Equal,
			Comma,
			Pipe,
			Arrow,	///< `->`
			Colon,
			Op	///< `+ - * / > < >= <= == && ||`
		};

		struct NkGToken {
				NkGTok kind = NkGTok::End;
				NkString text;	 ///< Ident/Op : le texte. String : contenu DECODE.
				NkString raw;	 ///< le lexeme source exact
				uint32 line = 1;
				uint32 column = 1;
		};

		inline bool NkGIsSpace(char c) {
			return c == ' ' || c == '\t' || c == '\r' || c == '\n';
		}
		inline bool NkGIsDigit(char c) {
			return c >= '0' && c <= '9';
		}
		inline bool NkGIsAlpha(char c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
		}
		inline bool NkGIsHex(char c) {
			return NkGIsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		}
		inline int32 NkGHexVal(char c) {
			if (c >= '0' && c <= '9') {
				return c - '0';
			}
			if (c >= 'a' && c <= 'f') {
				return c - 'a' + 10;
			}
			return c - 'A' + 10;
		}

		/// Le lexeur produit la SUITE COMPLETE des jetons avant que l'analyseur ne
		/// commence. Sur les plus gros documents du corpus (250 Ko, ~3 000 noeuds)
		/// ca reste quelques centaines de milliers de jetons — et un analyseur qui
		/// peut regarder en arriere sans relire le texte est bien plus simple.
		class NkGLexer {
			public:
				NkGLexer(const char *src, uint32 length) : mSrc(src), mLen(length) {}

				bool Run(NkVector<NkGToken> &out, NkGDiag &err) {
					// Le BOM UTF-8 se saute en silence : il ne porte aucune information
					// de contenu, et le refuser ferait echouer tout fichier passe par un
					// editeur Windows.
					if (mLen >= 3 && (uint8)mSrc[0] == 0xEF && (uint8)mSrc[1] == 0xBB
						&& (uint8)mSrc[2] == 0xBF) {
						mPos = 3;
					}
					while (true) {
						if (!SkipTrivia(err)) {
							return false;
						}
						if (mPos >= mLen) {
							NkGToken t;
							t.kind = NkGTok::End;
							t.line = mLine;
							t.column = mCol;
							out.PushBack(t);
							return true;
						}
						NkGToken tok;
						if (!Next(tok, out, err)) {
							return false;
						}
						out.PushBack(tok);
					}
				}

			private:
				const char *mSrc = nullptr;
				uint32 mLen = 0;
				uint32 mPos = 0;
				uint32 mLine = 1;
				uint32 mCol = 1;

				void Advance() {
					if (mSrc[mPos] == '\n') {
						++mLine;
						mCol = 1;
					} else {
						++mCol;
					}
					++mPos;
				}

				bool SkipTrivia(NkGDiag &err) {
					while (mPos < mLen) {
						const char c = mSrc[mPos];
						if (NkGIsSpace(c)) {
							Advance();
							continue;
						}
						if (c == '/' && mPos + 1 < mLen && mSrc[mPos + 1] == '/') {
							while (mPos < mLen && mSrc[mPos] != '\n') {
								Advance();
							}
							continue;
						}
						if (c == '/' && mPos + 1 < mLen && mSrc[mPos + 1] == '*') {
							const uint32 openLine = mLine;
							const uint32 openCol = mCol;
							Advance();
							Advance();
							bool closed = false;
							while (mPos < mLen) {
								if (mSrc[mPos] == '*' && mPos + 1 < mLen && mSrc[mPos + 1] == '/') {
									Advance();
									Advance();
									closed = true;
									break;
								}
								Advance();
							}
							if (!closed) {
								// ⚠️ Un commentaire de bloc non ferme avale la fin du fichier
								//    en silence si on ne le dit pas : le document se lit
								//    « valide » avec la moitie de son contenu disparue.
								err.code = NkString("E-PARSE");
								err.message = NkString("commentaire de bloc jamais ferme");
								err.line = openLine;
								err.column = openCol;
								return false;
							}
							continue;
						}
						break;
					}
					return true;
				}

				/// `-` est ambigu : debut d'un nombre negatif, ou soustraction. On
				/// tranche sur le jeton PRECEDENT — s'il peut terminer un operande,
				/// c'est une soustraction. C'est la seule information disponible sans
				/// remonter a la grammaire, et elle suffit pour la v0.2.
				static bool CanEndOperand(const NkVector<NkGToken> &out) {
					if (out.Size() == 0) {
						return false;
					}
					const NkGTok k = out[out.Size() - 1].kind;
					return k == NkGTok::Ident || k == NkGTok::Number || k == NkGTok::String
						   || k == NkGTok::Color || k == NkGTok::RParen || k == NkGTok::RBracket;
				}

				bool Next(NkGToken &tok, const NkVector<NkGToken> &out, NkGDiag &err) {
					tok.line = mLine;
					tok.column = mCol;
					const uint32 begin = mPos;
					const char c = mSrc[mPos];

					if (c == '"') {
						return LexString(tok, err);
					}
					if (c == '#') {
						return LexColor(tok, err);
					}
					if (NkGIsDigit(c) || (c == '-' && mPos + 1 < mLen && NkGIsDigit(mSrc[mPos + 1])
										  && !CanEndOperand(out))) {
						return LexNumber(tok, err);
					}
					if (NkGIsAlpha(c)) {
						while (mPos < mLen && (NkGIsAlpha(mSrc[mPos]) || NkGIsDigit(mSrc[mPos]))) {
							Advance();
						}
						// L'identifiant POINTE (`n1.value`, `Enum.X`) : le document 2 §3
						// ne l'a pas dans `expr`, mais ses propres exemples §4.1 et §6.3
						// l'utilisent. Il est lu comme UN identifiant, et le manque est
						// porte tel quel dans `design/9_Grammaire_complete.md` §6.2.
						while (mPos + 1 < mLen && mSrc[mPos] == '.' && NkGIsAlpha(mSrc[mPos + 1])) {
							Advance();
							while (mPos < mLen
								   && (NkGIsAlpha(mSrc[mPos]) || NkGIsDigit(mSrc[mPos]))) {
								Advance();
							}
						}
						tok.kind = NkGTok::Ident;
						tok.text = NkString(mSrc + begin, mPos - begin);
						tok.raw = tok.text;
						return true;
					}

					switch (c) {
						case '{':
							tok.kind = NkGTok::LBrace;
							break;
						case '}':
							tok.kind = NkGTok::RBrace;
							break;
						case '(':
							tok.kind = NkGTok::LParen;
							break;
						case ')':
							tok.kind = NkGTok::RParen;
							break;
						case '[':
							tok.kind = NkGTok::LBracket;
							break;
						case ']':
							tok.kind = NkGTok::RBracket;
							break;
						case ',':
							tok.kind = NkGTok::Comma;
							break;
						case ':':
							tok.kind = NkGTok::Colon;
							break;
						default:
							tok.kind = NkGTok::Op;
							break;
					}

					if (tok.kind != NkGTok::Op) {
						Advance();
						tok.text = NkString(mSrc + begin, 1);
						tok.raw = tok.text;
						return true;
					}

					// Les operateurs, du plus long au plus court.
					static const char *kTwo[] = {"->", ">=", "<=", "==", "&&", "||"};
					for (uint32 i = 0; i < 6; ++i) {
						if (mPos + 1 < mLen && mSrc[mPos] == kTwo[i][0] && mSrc[mPos + 1] == kTwo[i][1]) {
							Advance();
							Advance();
							tok.kind = (i == 0) ? NkGTok::Arrow : NkGTok::Op;
							tok.text = NkString(kTwo[i]);
							tok.raw = tok.text;
							return true;
						}
					}
					if (c == '=') {
						Advance();
						tok.kind = NkGTok::Equal;
						tok.text = NkString("=");
						tok.raw = tok.text;
						return true;
					}
					if (c == '|') {
						Advance();
						tok.kind = NkGTok::Pipe;
						tok.text = NkString("|");
						tok.raw = tok.text;
						return true;
					}
					if (c == '+' || c == '-' || c == '*' || c == '/' || c == '>' || c == '<') {
						Advance();
						tok.kind = NkGTok::Op;
						tok.text = NkString(mSrc + begin, 1);
						tok.raw = tok.text;
						return true;
					}

					err.code = NkString("E-PARSE");
					err.message = NkString("caractere inattendu");
					err.message.Append(" '");
					err.message.Append(c);
					err.message.Append('\'');
					err.line = mLine;
					err.column = mCol;
					return false;
				}

				bool LexString(NkGToken &tok, NkGDiag &err) {
					const uint32 begin = mPos;
					const uint32 openLine = mLine;
					const uint32 openCol = mCol;
					Advance();	// le guillemet ouvrant
					NkString decoded;
					while (mPos < mLen && mSrc[mPos] != '"') {
						const char c = mSrc[mPos];
						if (c == '\\') {
							if (mPos + 1 >= mLen) {
								break;
							}
							const char e = mSrc[mPos + 1];
							// ⚠️ TROIS ECHAPPEMENTS, PAS QUATRE. Le document 2 §2 en
							//    definit exactement trois. En accepter d'autres en les
							//    recopiant tels quels ferait perdre l'aller-retour : le
							//    re-encodage doublerait la contre-oblique. Un echappement
							//    inconnu est donc une ERREUR nommee, pas une tolerance.
							if (e == '"') {
								decoded.Append('"');
							} else if (e == '\\') {
								decoded.Append('\\');
							} else if (e == 'n') {
								decoded.Append('\n');
							} else {
								err.code = NkString("E-PARSE");
								err.message = NkString("echappement inconnu dans une chaine : \\");
								err.message.Append(e);
								err.message.Append(" (le document 2 §2 n'en definit que trois : "
												   "\\\" \\\\ \\n)");
								err.line = mLine;
								err.column = mCol;
								return false;
							}
							Advance();
							Advance();
							continue;
						}
						decoded.Append(c);
						Advance();
					}
					if (mPos >= mLen || mSrc[mPos] != '"') {
						err.code = NkString("E-PARSE");
						err.message = NkString("chaine jamais fermee");
						err.line = openLine;
						err.column = openCol;
						return false;
					}
					Advance();	// le guillemet fermant
					tok.kind = NkGTok::String;
					tok.text = decoded;
					tok.raw = NkString(mSrc + begin, mPos - begin);
					tok.line = openLine;
					tok.column = openCol;
					return true;
				}

				bool LexColor(NkGToken &tok, NkGDiag &err) {
					const uint32 begin = mPos;
					const uint32 openLine = mLine;
					const uint32 openCol = mCol;
					Advance();	// le dièse
					uint32 digits = 0;
					while (mPos < mLen && NkGIsHex(mSrc[mPos])) {
						Advance();
						++digits;
					}
					if (digits != 6 && digits != 8) {
						err.code = NkString("E-PARSE");
						err.message = NkString("couleur : 6 ou 8 chiffres hexadecimaux attendus");
						err.line = openLine;
						err.column = openCol;
						return false;
					}
					tok.kind = NkGTok::Color;
					tok.raw = NkString(mSrc + begin, mPos - begin);
					tok.text = tok.raw;
					tok.line = openLine;
					tok.column = openCol;
					return true;
				}

				bool LexNumber(NkGToken &tok, NkGDiag &err) {
					const uint32 begin = mPos;
					const uint32 openLine = mLine;
					const uint32 openCol = mCol;
					if (mSrc[mPos] == '-') {
						Advance();
					}
					while (mPos < mLen && NkGIsDigit(mSrc[mPos])) {
						Advance();
					}
					if (mPos + 1 < mLen && mSrc[mPos] == '.' && NkGIsDigit(mSrc[mPos + 1])) {
						Advance();
						while (mPos < mLen && NkGIsDigit(mSrc[mPos])) {
							Advance();
						}
					}
					(void)err;
					tok.kind = NkGTok::Number;
					tok.raw = NkString(mSrc + begin, mPos - begin);
					tok.text = tok.raw;
					tok.line = openLine;
					tok.column = openCol;
					return true;
				}
		};

		/// Decodage d'un lexeme numerique. Maison, parce que `strtod` depend de la
		/// locale : sous une locale francaise il lit `3,14` et rejette `3.14` — et
		/// le format, lui, ne connait que le point.
		inline float64 NkGParseNumber(const NkString &raw) {
			const char *p = raw.Data();
			if (!p) {
				return 0.0;
			}
			bool neg = false;
			if (*p == '-') {
				neg = true;
				++p;
			}
			float64 whole = 0.0;
			while (*p && NkGIsDigit(*p)) {
				whole = whole * 10.0 + (float64)(*p - '0');
				++p;
			}
			if (*p == '.') {
				++p;
				float64 scale = 0.1;
				while (*p && NkGIsDigit(*p)) {
					whole += (float64)(*p - '0') * scale;
					scale *= 0.1;
					++p;
				}
			}
			return neg ? -whole : whole;
		}

		inline uint32 NkGParseColor(const NkString &raw) {
			const char *p = raw.Data();
			if (!p || *p != '#') {
				return 0;
			}
			++p;
			uint32 v = 0;
			uint32 n = 0;
			while (*p && NkGIsHex(*p)) {
				v = (v << 4) | (uint32)NkGHexVal(*p);
				++p;
				++n;
			}
			// Un `#RRGGBB` vaut `#RRGGBBFF` : sans cette ligne, une couleur opaque
			// se comparerait comme totalement transparente.
			if (n == 6) {
				v = (v << 8) | 0xFFu;
			}
			return v;
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  L'ANALYSEUR
		// ═══════════════════════════════════════════════════════════════════════

		class NkGParser {
			public:
				NkGParser(const NkVector<NkGToken> &toks, NkGDocument &doc)
					: mToks(&toks), mDoc(&doc) {}

				bool ParseFile(NkGDiag &err) {
					// `file := "nkgui" version_lit include* section*`
					if (!ExpectIdentText("nkgui", err)) {
						return false;
					}
					if (Peek().kind != NkGTok::Number) {
						return Fail(err, "numero de version attendu apres 'nkgui'");
					}
					// Le lexeur rend `0.2` en un seul nombre : `version_lit` du
					// document 2 est `Number '.' Number`, ce qui decrit exactement ce
					// lexeme. On le rescinde ici plutot que de compliquer le lexeur.
					const NkString ver = Peek().raw;
					Take();
					uint32 dot = 0;
					while (dot < (uint32)ver.Size() && ver[dot] != '.') {
						++dot;
					}
					if (dot >= (uint32)ver.Size()) {
						return Fail(err, "version attendue sous la forme <majeure>.<mineure>");
					}
					mDoc->versionMajor = ver.SubStr(0, dot);
					mDoc->versionMinor = ver.SubStr(dot + 1);

					while (Peek().kind == NkGTok::Ident && Peek().text.Compare("include") == 0) {
						Take();
						if (Peek().kind != NkGTok::String) {
							return Fail(err, "chemin attendu apres 'include'");
						}
						mDoc->includes.PushBack(Peek().text);
						Take();
					}

					while (Peek().kind != NkGTok::End) {
						if (!ParseSection(err)) {
							return false;
						}
					}
					return true;
				}

			private:
				const NkVector<NkGToken> *mToks = nullptr;
				NkGDocument *mDoc = nullptr;
				uint32 mPos = 0;

				const NkGToken &Peek(uint32 ahead = 0) const {
					const uint32 i = mPos + ahead;
					const uint32 last = (uint32)mToks->Size() - 1;
					return (*mToks)[i < last ? i : last];
				}
				const NkGToken &Take() {
					const NkGToken &t = Peek();
					if (mPos + 1 < (uint32)mToks->Size()) {
						++mPos;
					}
					return t;
				}

				bool Fail(NkGDiag &err, const char *msg) const {
					err.code = NkString("E-PARSE");
					err.message = NkString(msg);
					// ⚠️ LE LEXEME FAUTIF EST DANS LE MESSAGE. « symbole inattendu ligne
					//    412 » oblige a ouvrir le fichier ; « symbole inattendu '[' ligne
					//    412 » dit tout de suite ce qui manque au format.
					const NkGToken &t = Peek();
					if (!t.raw.Empty()) {
						err.message.Append(" (lu : '");
						err.message.Append(t.raw);
						err.message.Append("')");
					}
					err.line = t.line;
					err.column = t.column;
					return false;
				}

				bool ExpectIdentText(const char *what, NkGDiag &err) {
					if (Peek().kind != NkGTok::Ident || Peek().text.Compare(what) != 0) {
						NkString m("'");
						m.Append(what);
						m.Append("' attendu");
						return Fail(err, m.Data());
					}
					Take();
					return true;
				}
				bool Expect(NkGTok k, const char *what, NkGDiag &err) {
					if (Peek().kind != k) {
						NkString m(what);
						m.Append(" attendu");
						return Fail(err, m.Data());
					}
					Take();
					return true;
				}

				// ── Les valeurs ───────────────────────────────────────────────
				bool ParseValue(NkGValue &v, NkGDiag &err) {
					const NkGToken &t = Peek();
					switch (t.kind) {
						case NkGTok::String:
							v.kind = NkGValueKind::String;
							v.text = t.text;
							v.raw = t.raw;
							Take();
							return true;
						case NkGTok::Number:
							v.kind = NkGValueKind::Number;
							v.raw = t.raw;
							v.num = NkGParseNumber(t.raw);
							Take();
							return true;
						case NkGTok::Color:
							v.kind = NkGValueKind::Color;
							v.raw = t.raw;
							v.color = NkGParseColor(t.raw);
							Take();
							return true;
						case NkGTok::LParen:
							return ParseVec2(v, err);
						case NkGTok::Ident: {
							NkString joined(t.text);
							NkString raw(t.raw);
							Take();
							bool isFlags = false;
							while (Peek().kind == NkGTok::Pipe) {
								isFlags = true;
								Take();
								if (Peek().kind != NkGTok::Ident) {
									return Fail(err, "identifiant attendu apres '|'");
								}
								joined.Append(" | ");
								joined.Append(Peek().text);
								raw.Append(" | ");
								raw.Append(Peek().raw);
								Take();
							}
							v.kind = isFlags ? NkGValueKind::Flags : NkGValueKind::Ident;
							v.text = joined;
							v.raw = raw;
							return true;
						}
						case NkGTok::LBracket:
							// Voir `design/9_Grammaire_complete.md` §6.1 : le document 2
							// n'a AUCUN litteral de liste, alors que sa table §8 en
							// demande un pour cinq roles. On refuse en le disant plutot
							// que d'en inventer un.
							return Fail(err,
										"litteral de liste : le document 2 v0.2 n'en definit "
										"aucun (cf. doc 9 §6.1, decision en attente)");
						default:
							return Fail(err, "valeur attendue");
					}
				}

				bool ParseVec2(NkGValue &v, NkGDiag &err) {
					NkString raw("(");
					Take();	 // '('
					if (Peek().kind != NkGTok::Number) {
						return Fail(err, "nombre attendu dans un Vec2");
					}
					const NkString xs = Peek().raw;
					raw.Append(xs);
					Take();
					if (!Expect(NkGTok::Comma, "','", err)) {
						return false;
					}
					raw.Append(", ");
					if (Peek().kind != NkGTok::Number) {
						return Fail(err, "nombre attendu dans un Vec2");
					}
					const NkString ys = Peek().raw;
					raw.Append(ys);
					Take();
					if (!Expect(NkGTok::RParen, "')'", err)) {
						return false;
					}
					raw.Append(')');
					v.kind = NkGValueKind::Vec2;
					v.raw = raw;
					v.vx = NkGParseNumber(xs);
					v.vy = NkGParseNumber(ys);
					return true;
				}

				// ── Les expressions, par precedence grimpante ─────────────────
				static int32 Precedence(const NkString &op) {
					if (op.Compare("||") == 0) {
						return 1;
					}
					if (op.Compare("&&") == 0) {
						return 2;
					}
					if (op.Compare("==") == 0) {
						return 3;
					}
					if (op.Compare(">") == 0 || op.Compare("<") == 0 || op.Compare(">=") == 0
						|| op.Compare("<=") == 0) {
						return 4;
					}
					if (op.Compare("+") == 0 || op.Compare("-") == 0) {
						return 5;
					}
					if (op.Compare("*") == 0 || op.Compare("/") == 0) {
						return 6;
					}
					return 0;
				}

				uint32 AddExpr(const NkGExpr &e) {
					mDoc->exprs.PushBack(e);
					return (uint32)mDoc->exprs.Size() - 1;
				}

				bool ParsePrimary(uint32 &out, NkGDiag &err) {
					const NkGToken &t = Peek();
					NkGExpr e;
					if (t.kind == NkGTok::LParen) {
						Take();
						uint32 inner = kNoIndex;
						if (!ParseExpr(inner, 0, err)) {
							return false;
						}
						if (!Expect(NkGTok::RParen, "')'", err)) {
							return false;
						}
						e.kind = NkGExprKind::Paren;
						e.lhs = inner;
						out = AddExpr(e);
						return true;
					}
					if (t.kind == NkGTok::String || t.kind == NkGTok::Number
						|| t.kind == NkGTok::Color) {
						e.kind = NkGExprKind::Literal;
						if (!ParseValue(e.literal, err)) {
							return false;
						}
						out = AddExpr(e);
						return true;
					}
					if (t.kind == NkGTok::Ident) {
						e.kind = NkGExprKind::Ident;
						e.ident = t.text;
						Take();
						out = AddExpr(e);
						return true;
					}
					return Fail(err, "expression attendue");
				}

				bool ParseExpr(uint32 &out, int32 minPrec, NkGDiag &err) {
					uint32 lhs = kNoIndex;
					if (!ParsePrimary(lhs, err)) {
						return false;
					}
					while (Peek().kind == NkGTok::Op) {
						const NkString op = Peek().text;
						const int32 prec = Precedence(op);
						if (prec == 0 || prec < minPrec) {
							break;
						}
						Take();
						uint32 rhs = kNoIndex;
						if (!ParseExpr(rhs, prec + 1, err)) {
							return false;
						}
						NkGExpr e;
						e.kind = NkGExprKind::Binary;
						e.op = op;
						e.lhs = lhs;
						e.rhs = rhs;
						lhs = AddExpr(e);
					}
					out = lhs;
					return true;
				}

				bool ParseArgList(NkVector<uint32> &args, NkGDiag &err) {
					if (!Expect(NkGTok::LParen, "'('", err)) {
						return false;
					}
					if (Peek().kind == NkGTok::RParen) {
						Take();
						return true;
					}
					while (true) {
						uint32 a = kNoIndex;
						if (!ParseExpr(a, 0, err)) {
							return false;
						}
						args.PushBack(a);
						if (Peek().kind == NkGTok::Comma) {
							Take();
							continue;
						}
						break;
					}
					return Expect(NkGTok::RParen, "')'", err);
				}

				// ── Les instructions ──────────────────────────────────────────
				uint32 AddStmt(const NkGStmt &s) {
					mDoc->stmts.PushBack(s);
					return (uint32)mDoc->stmts.Size() - 1;
				}

				bool ParseStmtBlock(NkVector<uint32> &out, NkGDiag &err) {
					if (!Expect(NkGTok::LBrace, "'{'", err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						uint32 s = kNoIndex;
						if (!ParseStmt(s, err)) {
							return false;
						}
						out.PushBack(s);
					}
					Take();
					return true;
				}

				bool ParseStmt(uint32 &out, NkGDiag &err) {
					const NkGToken &t = Peek();
					if (t.kind != NkGTok::Ident) {
						return Fail(err, "instruction attendue");
					}
					if (t.text.Compare("set") == 0) {
						Take();
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "nom de variable attendu apres 'set'");
						}
						NkGStmt s;
						s.kind = NkGStmtKind::Assign;
						s.name = Peek().text;
						Take();
						if (!Expect(NkGTok::Equal, "'='", err)) {
							return false;
						}
						if (!ParseExpr(s.expr, 0, err)) {
							return false;
						}
						out = AddStmt(s);
						return true;
					}
					if (t.text.Compare("if") == 0) {
						Take();
						NkGStmt s;
						s.kind = NkGStmtKind::If;
						if (!ParseExpr(s.expr, 0, err)) {
							return false;
						}
						if (!ParseStmtBlock(s.thenStmts, err)) {
							return false;
						}
						if (Peek().kind == NkGTok::Ident && Peek().text.Compare("else") == 0) {
							Take();
							s.hasElse = true;
							if (!ParseStmtBlock(s.elseStmts, err)) {
								return false;
							}
						}
						out = AddStmt(s);
						return true;
					}
					if (t.text.Compare("Callback") == 0) {
						Take();
						NkGStmt s;
						s.kind = NkGStmtKind::Call;
						if (Peek().kind != NkGTok::String) {
							return Fail(err, "nom de callback attendu (chaine)");
						}
						s.name = Peek().text;
						Take();
						if (!ParseArgList(s.args, err)) {
							return false;
						}
						out = AddStmt(s);
						return true;
					}
					return Fail(err, "instruction attendue ('set', 'if' ou 'Callback')");
				}

				// ── Les evenements ────────────────────────────────────────────
				bool ParseEvent(NkGEvent &ev, NkGDiag &err) {
					Take();	 // 'on'
					if (Peek().kind != NkGTok::Ident) {
						return Fail(err, "nom d'evenement attendu apres 'on'");
					}
					ev.name = Peek().text;
					Take();
					if (Peek().kind == NkGTok::LParen) {
						ev.hasParams = true;
						Take();
						if (Peek().kind != NkGTok::RParen) {
							while (true) {
								if (Peek().kind != NkGTok::Ident) {
									return Fail(err, "nom de parametre attendu");
								}
								ev.params.PushBack(Peek().text);
								Take();
								if (Peek().kind == NkGTok::Comma) {
									Take();
									continue;
								}
								break;
							}
						}
						if (!Expect(NkGTok::RParen, "')'", err)) {
							return false;
						}
					}
					if (!Expect(NkGTok::Arrow, "'->'", err)) {
						return false;
					}
					const NkGToken &a = Peek();
					if (a.kind == NkGTok::LBrace) {
						ev.action = NkGActionKind::Inline;
						return ParseStmtBlock(ev.stmts, err);
					}
					if (a.kind == NkGTok::Ident && a.text.Compare("Callback") == 0) {
						Take();
						ev.action = NkGActionKind::Callback;
						if (Peek().kind != NkGTok::String) {
							return Fail(err, "nom de callback attendu (chaine)");
						}
						ev.target = Peek().text;
						Take();
						return ParseArgList(ev.args, err);
					}
					if (a.kind == NkGTok::Ident && a.text.Compare("Behavior") == 0) {
						Take();
						ev.action = NkGActionKind::Behavior;
						if (Peek().kind != NkGTok::String) {
							return Fail(err, "nom de comportement attendu (chaine)");
						}
						ev.target = Peek().text;
						Take();
						return true;
					}
					return Fail(err, "action attendue ('Callback', 'Behavior' ou un bloc)");
				}

				// ── Les noeuds de widgets ─────────────────────────────────────
				/// ⚠️ L'INDICE EST RESERVE AVANT L'ANALYSE DES ENFANTS, et c'est
				///    obligatoire : `mDoc->nodes` grandit pendant la recursion. Copier
				///    le noeud en fin d'analyse ecraserait ce que les enfants y ont
				///    ajoute — et remplir un noeud par reference le ferait pointer dans
				///    un tampon qui a demenage.
				bool ParseNode(uint32 &out, NkGDiag &err) {
					NkGNode node;
					node.kind = Peek().text;
					Take();
					if (Peek().kind == NkGTok::String) {
						node.hasId = true;
						node.id = Peek().text;
						Take();
					}
					if (!Expect(NkGTok::LBrace, "'{'", err)) {
						return false;
					}

					mDoc->nodes.PushBack(node);
					const uint32 self = (uint32)mDoc->nodes.Size() - 1;

					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "propriete, evenement ou noeud attendu");
						}
						if (Peek().text.Compare("on") == 0 && Peek(1).kind == NkGTok::Ident) {
							NkGEvent ev;
							if (!ParseEvent(ev, err)) {
								return false;
							}
							NkGMember m;
							m.kind = NkGMemberKind::Event;
							m.index = (uint32)mDoc->nodes[self].events.Size();
							mDoc->nodes[self].events.PushBack(ev);
							mDoc->nodes[self].members.PushBack(m);
							continue;
						}
						if (Peek(1).kind == NkGTok::Equal) {
							NkGProp p;
							p.name = Peek().text;
							Take();
							Take();	 // '='
							if (!ParseValue(p.value, err)) {
								return false;
							}
							NkGMember m;
							m.kind = NkGMemberKind::Prop;
							m.index = (uint32)mDoc->nodes[self].props.Size();
							mDoc->nodes[self].props.PushBack(p);
							mDoc->nodes[self].members.PushBack(m);
							continue;
						}
						uint32 child = kNoIndex;
						if (!ParseNode(child, err)) {
							return false;
						}
						NkGMember m;
						m.kind = NkGMemberKind::Child;
						m.index = (uint32)mDoc->nodes[self].children.Size();
						mDoc->nodes[self].children.PushBack(child);
						mDoc->nodes[self].members.PushBack(m);
					}
					Take();	 // '}'
					out = self;
					return true;
				}

				// ── Les sections ──────────────────────────────────────────────
				bool ParseSection(NkGDiag &err) {
					const NkGToken &t = Peek();
					if (t.kind != NkGTok::Ident) {
						return Fail(err, "section attendue");
					}
					if (t.text.Compare("geometry") == 0) {
						return ParseGeometry(err);
					}
					if (t.text.Compare("widgets") == 0) {
						return ParseWidgets(err);
					}
					if (t.text.Compare("behavior") == 0) {
						return ParseBehavior(err);
					}
					if (t.text.Compare("controller") == 0) {
						return ParseController(err);
					}
					if (t.text.Compare("callback") == 0) {
						return ParseTopCallback(err);
					}
					return Fail(err,
								"section inconnue ('geometry', 'widgets', 'behavior', "
								"'controller' ou 'callback' attendus)");
				}

				bool ParsePropBlock(NkVector<NkGProp> &props, NkGDiag &err) {
					if (!Expect(NkGTok::LBrace, "'{'", err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "nom de propriete attendu");
						}
						NkGProp p;
						p.name = Peek().text;
						Take();
						if (!Expect(NkGTok::Equal, "'='", err)) {
							return false;
						}
						if (!ParseValue(p.value, err)) {
							return false;
						}
						props.PushBack(p);
					}
					Take();
					return true;
				}

				bool ParseGeometry(NkGDiag &err) {
					Take();	 // 'geometry'
					if (!Expect(NkGTok::LBrace, "'{'", err)) {
						return false;
					}
					NkGTopSection sec;
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (!ExpectIdentText("shape", err)) {
							return false;
						}
						NkGShape sh;
						if (Peek().kind != NkGTok::String) {
							return Fail(err, "nom de forme attendu (chaine)");
						}
						sh.name = Peek().text;
						Take();
						if (!ParsePropBlock(sh.props, err)) {
							return false;
						}
						mDoc->shapes.PushBack(sh);
						sec.shapes.PushBack((uint32)mDoc->shapes.Size() - 1);
					}
					Take();
					mDoc->topSections.PushBack(sec);
					NkGSection ref;
					ref.kind = NkGSectionKind::Geometry;
					ref.index = (uint32)mDoc->topSections.Size() - 1;
					mDoc->sections.PushBack(ref);
					return true;
				}

				bool ParseWidgets(NkGDiag &err) {
					Take();	 // 'widgets'
					if (!Expect(NkGTok::LBrace, "'{'", err)) {
						return false;
					}
					NkGTopSection sec;
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "role de widget attendu");
						}
						uint32 root = kNoIndex;
						if (!ParseNode(root, err)) {
							return false;
						}
						sec.roots.PushBack(root);
					}
					Take();
					mDoc->topSections.PushBack(sec);
					NkGSection ref;
					ref.kind = NkGSectionKind::Widgets;
					ref.index = (uint32)mDoc->topSections.Size() - 1;
					mDoc->sections.PushBack(ref);
					return true;
				}

				bool ParseBehavior(NkGDiag &err) {
					Take();	 // 'behavior'
					NkGBehavior b;
					if (Peek().kind != NkGTok::String) {
						return Fail(err, "nom de comportement attendu (chaine)");
					}
					b.name = Peek().text;
					Take();
					if (Peek().kind == NkGTok::Ident && Peek().text.Compare("graph") == 0) {
						b.isGraph = true;
						Take();
					}
					if (!Expect(NkGTok::LBrace, "'{'", err)) {
						return false;
					}
					mDoc->behaviors.PushBack(b);
					const uint32 self = (uint32)mDoc->behaviors.Size() - 1;

					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (mDoc->behaviors[self].isGraph) {
							if (!ParseGraphItem(self, err)) {
								return false;
							}
							continue;
						}
						uint32 s = kNoIndex;
						if (!ParseStmt(s, err)) {
							return false;
						}
						mDoc->behaviors[self].stmts.PushBack(s);
					}
					Take();
					NkGSection ref;
					ref.kind = NkGSectionKind::Behavior;
					ref.index = self;
					mDoc->sections.PushBack(ref);
					return true;
				}

				bool ParseGraphItem(uint32 self, NkGDiag &err) {
					if (Peek().kind != NkGTok::Ident) {
						return Fail(err, "'node' ou 'wire' attendu");
					}
					if (Peek().text.Compare("node") == 0) {
						Take();
						NkGGraphNode gn;
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "nom de noeud attendu");
						}
						gn.name = Peek().text;
						Take();
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "type de noeud attendu");
						}
						gn.type = Peek().text;
						Take();
						if (Peek().kind == NkGTok::LBrace) {
							gn.hasPins = true;
							Take();
							if (Peek().kind != NkGTok::RBrace) {
								while (true) {
									if (Peek().kind != NkGTok::Ident) {
										return Fail(err, "nom de pin attendu");
									}
									NkGProp p;
									p.name = Peek().text;
									Take();
									if (!Expect(NkGTok::Equal, "'='", err)) {
										return false;
									}
									uint32 e = kNoIndex;
									if (!ParseExpr(e, 0, err)) {
										return false;
									}
									gn.pins.PushBack(p);
									gn.pinExprs.PushBack(e);
									if (Peek().kind == NkGTok::Comma) {
										Take();
										continue;
									}
									break;
								}
							}
							if (!Expect(NkGTok::RBrace, "'}'", err)) {
								return false;
							}
						}
						mDoc->behaviors[self].gnodes.PushBack(gn);
						return true;
					}
					if (Peek().text.Compare("wire") == 0) {
						Take();
						NkGWire w;
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "reference de pin attendue");
						}
						w.refs.PushBack(Peek().text);
						Take();
						while (Peek().kind == NkGTok::Arrow) {
							Take();
							if (Peek().kind != NkGTok::Ident) {
								return Fail(err, "reference de pin attendue apres '->'");
							}
							w.refs.PushBack(Peek().text);
							Take();
						}
						if (w.refs.Size() < 2) {
							return Fail(err, "un fil relie au moins deux pins");
						}
						mDoc->behaviors[self].wires.PushBack(w);
						return true;
					}
					return Fail(err, "'node' ou 'wire' attendu");
				}

				bool ParseType(NkString &type, NkVector<NkString> &labels, bool &isEnum,
							   NkGDiag &err) {
					if (Peek().kind != NkGTok::Ident) {
						return Fail(err, "type attendu");
					}
					type = Peek().text;
					Take();
					isEnum = false;
					if (type.Compare("Enum") == 0 && Peek().kind == NkGTok::LBracket) {
						isEnum = true;
						Take();
						while (true) {
							if (Peek().kind != NkGTok::Ident) {
								return Fail(err, "libelle d'enumeration attendu");
							}
							labels.PushBack(Peek().text);
							Take();
							if (Peek().kind == NkGTok::Comma) {
								Take();
								continue;
							}
							break;
						}
						return Expect(NkGTok::RBracket, "']'", err);
					}
					return true;
				}

				bool ParseCallbackSig(NkGCallbackSig &sig, NkGDiag &err) {
					if (!ExpectIdentText("callback", err)) {
						return false;
					}
					if (Peek().kind != NkGTok::Ident) {
						return Fail(err, "nom de callback attendu");
					}
					sig.name = Peek().text;
					Take();
					if (!Expect(NkGTok::LParen, "'('", err)) {
						return false;
					}
					if (Peek().kind != NkGTok::RParen) {
						while (true) {
							NkGParam p;
							if (Peek().kind != NkGTok::Ident) {
								return Fail(err, "nom de parametre attendu");
							}
							p.name = Peek().text;
							Take();
							if (!Expect(NkGTok::Colon, "':'", err)) {
								return false;
							}
							if (!ParseType(p.type, p.enumLabels, p.isEnum, err)) {
								return false;
							}
							sig.params.PushBack(p);
							if (Peek().kind == NkGTok::Comma) {
								Take();
								continue;
							}
							break;
						}
					}
					if (!Expect(NkGTok::RParen, "')'", err)) {
						return false;
					}
					if (!Expect(NkGTok::Arrow, "'->'", err)) {
						return false;
					}
					return ParseType(sig.ret, sig.retEnumLabels, sig.retIsEnum, err);
				}

				bool ParseController(NkGDiag &err) {
					Take();	 // 'controller'
					NkGController c;
					if (Peek().kind != NkGTok::String) {
						return Fail(err, "nom de controleur attendu (chaine)");
					}
					c.name = Peek().text;
					Take();
					if (!Expect(NkGTok::LBrace, "'{'", err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						NkGCallbackSig sig;
						if (!ParseCallbackSig(sig, err)) {
							return false;
						}
						mDoc->callbacks.PushBack(sig);
						c.callbacks.PushBack((uint32)mDoc->callbacks.Size() - 1);
					}
					Take();
					mDoc->controllers.PushBack(c);
					NkGSection ref;
					ref.kind = NkGSectionKind::Controller;
					ref.index = (uint32)mDoc->controllers.Size() - 1;
					mDoc->sections.PushBack(ref);
					return true;
				}

				bool ParseTopCallback(NkGDiag &err) {
					NkGCallbackSig sig;
					if (!ParseCallbackSig(sig, err)) {
						return false;
					}
					mDoc->callbacks.PushBack(sig);
					NkGSection ref;
					ref.kind = NkGSectionKind::Callback;
					ref.index = (uint32)mDoc->callbacks.Size() - 1;
					mDoc->sections.PushBack(ref);
					return true;
				}
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  LE SERIALISEUR
		// ═══════════════════════════════════════════════════════════════════════

		/// ⚠️ CE SONT DES OPTIONS DE MISE EN FORME, PAS DE SEMANTIQUE. Deux fichiers
		///    qui ne different que par elles decrivent la meme interface. Elles
		///    existent parce que le document 2 n'impose pas de style d'ecriture :
		///    ses exemples indentent de quatre espaces, le convertisseur du corpus
		///    Camrail en met deux, et il faut pouvoir reecrire l'un comme l'autre
		///    sans reformater le fichier de quelqu'un d'autre.
		struct NkGWriteOptions {
				uint32 indent = 4;
				bool crlf = false;
				bool inlineEmptyBlock = true;	///< `Button "x" { }` sur une ligne
				bool blankLineAfterHeader = true;
				bool blankLineBetweenSections = true;
		};

		class NkGWriter {
			public:
				NkGWriter(const NkGDocument &doc, const NkGWriteOptions &opt)
					: mDoc(&doc), mOpt(opt) {}

				NkString Run() {
					mOut.Clear();
					// Une reserve grossiere evite quelques dizaines de reallocations
					// sur les gros documents ; elle n'a pas besoin d'etre juste.
					mOut.Reserve(4096);
					mOut.Append("nkgui ");
					mOut.Append(mDoc->versionMajor);
					mOut.Append('.');
					mOut.Append(mDoc->versionMinor);
					NewLine();
					for (uint32 i = 0; i < (uint32)mDoc->includes.Size(); ++i) {
						mOut.Append("include ");
						WriteQuoted(mDoc->includes[i]);
						NewLine();
					}
					if (mOpt.blankLineAfterHeader && mDoc->sections.Size() > 0) {
						NewLine();
					}
					for (uint32 i = 0; i < (uint32)mDoc->sections.Size(); ++i) {
						if (i > 0 && mOpt.blankLineBetweenSections) {
							NewLine();
						}
						WriteSection(mDoc->sections[i]);
					}
					return mOut;
				}

			private:
				const NkGDocument *mDoc = nullptr;
				NkGWriteOptions mOpt;
				NkString mOut;

				void NewLine() {
					if (mOpt.crlf) {
						mOut.Append('\r');
					}
					mOut.Append('\n');
				}
				void Indent(uint32 depth) {
					const uint32 n = depth * mOpt.indent;
					for (uint32 i = 0; i < n; ++i) {
						mOut.Append(' ');
					}
				}

				/// Le seul endroit du serialiseur qui REGENERE une valeur au lieu de
				/// recopier son lexeme. Il doit donc etre l'inverse exact de
				/// `NkGLexer::LexString`, sinon l'aller-retour ne tient pas.
				void WriteQuoted(const NkString &s) {
					mOut.Append('"');
					const char *p = s.Data();
					const uint32 n = (uint32)s.Size();
					for (uint32 i = 0; i < n; ++i) {
						const char c = p[i];
						if (c == '"') {
							mOut.Append("\\\"");
						} else if (c == '\\') {
							mOut.Append("\\\\");
						} else if (c == '\n') {
							mOut.Append("\\n");
						} else {
							mOut.Append(c);
						}
					}
					mOut.Append('"');
				}

				void WriteValue(const NkGValue &v) {
					if (v.kind == NkGValueKind::String) {
						WriteQuoted(v.text);
						return;
					}
					mOut.Append(v.raw);
				}

				void WriteExpr(uint32 idx) {
					if (idx == kNoIndex) {
						return;
					}
					const NkGExpr &e = mDoc->exprs[idx];
					switch (e.kind) {
						case NkGExprKind::Literal:
							WriteValue(e.literal);
							return;
						case NkGExprKind::Ident:
							mOut.Append(e.ident);
							return;
						case NkGExprKind::Paren:
							mOut.Append('(');
							WriteExpr(e.lhs);
							mOut.Append(')');
							return;
						case NkGExprKind::Binary:
						default:
							WriteExpr(e.lhs);
							mOut.Append(' ');
							mOut.Append(e.op);
							mOut.Append(' ');
							WriteExpr(e.rhs);
							return;
					}
				}

				void WriteArgs(const NkVector<uint32> &args) {
					mOut.Append('(');
					for (uint32 i = 0; i < (uint32)args.Size(); ++i) {
						if (i > 0) {
							mOut.Append(", ");
						}
						WriteExpr(args[i]);
					}
					mOut.Append(')');
				}

				void WriteStmt(uint32 idx, uint32 depth) {
					const NkGStmt &s = mDoc->stmts[idx];
					Indent(depth);
					if (s.kind == NkGStmtKind::Assign) {
						mOut.Append("set ");
						mOut.Append(s.name);
						mOut.Append(" = ");
						WriteExpr(s.expr);
						NewLine();
						return;
					}
					if (s.kind == NkGStmtKind::Call) {
						mOut.Append("Callback ");
						WriteQuoted(s.name);
						WriteArgs(s.args);
						NewLine();
						return;
					}
					mOut.Append("if ");
					WriteExpr(s.expr);
					mOut.Append(" {");
					NewLine();
					for (uint32 i = 0; i < (uint32)s.thenStmts.Size(); ++i) {
						WriteStmt(s.thenStmts[i], depth + 1);
					}
					Indent(depth);
					mOut.Append('}');
					if (s.hasElse) {
						mOut.Append(" else {");
						NewLine();
						for (uint32 i = 0; i < (uint32)s.elseStmts.Size(); ++i) {
							WriteStmt(s.elseStmts[i], depth + 1);
						}
						Indent(depth);
						mOut.Append('}');
					}
					NewLine();
				}

				void WriteEvent(const NkGEvent &ev, uint32 depth) {
					Indent(depth);
					mOut.Append("on ");
					mOut.Append(ev.name);
					if (ev.hasParams) {
						mOut.Append('(');
						for (uint32 i = 0; i < (uint32)ev.params.Size(); ++i) {
							if (i > 0) {
								mOut.Append(", ");
							}
							mOut.Append(ev.params[i]);
						}
						mOut.Append(')');
					}
					mOut.Append(" -> ");
					if (ev.action == NkGActionKind::Callback) {
						mOut.Append("Callback ");
						WriteQuoted(ev.target);
						WriteArgs(ev.args);
						NewLine();
						return;
					}
					if (ev.action == NkGActionKind::Behavior) {
						mOut.Append("Behavior ");
						WriteQuoted(ev.target);
						NewLine();
						return;
					}
					mOut.Append('{');
					NewLine();
					for (uint32 i = 0; i < (uint32)ev.stmts.Size(); ++i) {
						WriteStmt(ev.stmts[i], depth + 1);
					}
					Indent(depth);
					mOut.Append('}');
					NewLine();
				}

				void WriteProp(const NkGProp &p, uint32 depth) {
					Indent(depth);
					mOut.Append(p.name);
					mOut.Append(" = ");
					WriteValue(p.value);
					NewLine();
				}

				void WriteNode(uint32 idx, uint32 depth) {
					const NkGNode &n = mDoc->nodes[idx];
					Indent(depth);
					mOut.Append(n.kind);
					if (n.hasId) {
						mOut.Append(' ');
						WriteQuoted(n.id);
					}
					if (n.members.Size() == 0 && mOpt.inlineEmptyBlock) {
						mOut.Append(" { }");
						NewLine();
						return;
					}
					mOut.Append(" {");
					NewLine();
					for (uint32 i = 0; i < (uint32)n.members.Size(); ++i) {
						const NkGMember &m = n.members[i];
						if (m.kind == NkGMemberKind::Prop) {
							WriteProp(n.props[m.index], depth + 1);
						} else if (m.kind == NkGMemberKind::Event) {
							WriteEvent(n.events[m.index], depth + 1);
						} else {
							WriteNode(n.children[m.index], depth + 1);
						}
					}
					Indent(depth);
					mOut.Append('}');
					NewLine();
				}

				void WriteTypeRef(const NkString &type, const NkVector<NkString> &labels,
								  bool isEnum) {
					mOut.Append(type);
					if (!isEnum) {
						return;
					}
					mOut.Append('[');
					for (uint32 i = 0; i < (uint32)labels.Size(); ++i) {
						if (i > 0) {
							mOut.Append(',');
						}
						mOut.Append(labels[i]);
					}
					mOut.Append(']');
				}

				void WriteCallbackSig(const NkGCallbackSig &sig, uint32 depth) {
					Indent(depth);
					mOut.Append("callback ");
					mOut.Append(sig.name);
					mOut.Append('(');
					for (uint32 i = 0; i < (uint32)sig.params.Size(); ++i) {
						if (i > 0) {
							mOut.Append(", ");
						}
						mOut.Append(sig.params[i].name);
						mOut.Append(": ");
						WriteTypeRef(sig.params[i].type, sig.params[i].enumLabels,
									 sig.params[i].isEnum);
					}
					mOut.Append(") -> ");
					WriteTypeRef(sig.ret, sig.retEnumLabels, sig.retIsEnum);
					NewLine();
				}

				void WriteSection(const NkGSection &ref) {
					if (ref.kind == NkGSectionKind::Widgets) {
						const NkGTopSection &sec = mDoc->topSections[ref.index];
						mOut.Append("widgets {");
						NewLine();
						for (uint32 i = 0; i < (uint32)sec.roots.Size(); ++i) {
							WriteNode(sec.roots[i], 1);
						}
						mOut.Append('}');
						NewLine();
						return;
					}
					if (ref.kind == NkGSectionKind::Geometry) {
						const NkGTopSection &sec = mDoc->topSections[ref.index];
						mOut.Append("geometry {");
						NewLine();
						for (uint32 i = 0; i < (uint32)sec.shapes.Size(); ++i) {
							const NkGShape &sh = mDoc->shapes[sec.shapes[i]];
							Indent(1);
							mOut.Append("shape ");
							WriteQuoted(sh.name);
							if (sh.props.Size() == 0 && mOpt.inlineEmptyBlock) {
								mOut.Append(" { }");
								NewLine();
								continue;
							}
							mOut.Append(" {");
							NewLine();
							for (uint32 p = 0; p < (uint32)sh.props.Size(); ++p) {
								WriteProp(sh.props[p], 2);
							}
							Indent(1);
							mOut.Append('}');
							NewLine();
						}
						mOut.Append('}');
						NewLine();
						return;
					}
					if (ref.kind == NkGSectionKind::Behavior) {
						const NkGBehavior &b = mDoc->behaviors[ref.index];
						mOut.Append("behavior ");
						WriteQuoted(b.name);
						if (b.isGraph) {
							mOut.Append(" graph");
						}
						mOut.Append(" {");
						NewLine();
						if (b.isGraph) {
							for (uint32 i = 0; i < (uint32)b.gnodes.Size(); ++i) {
								const NkGGraphNode &gn = b.gnodes[i];
								Indent(1);
								mOut.Append("node ");
								mOut.Append(gn.name);
								mOut.Append(' ');
								mOut.Append(gn.type);
								if (gn.hasPins) {
									mOut.Append(" { ");
									for (uint32 p = 0; p < (uint32)gn.pins.Size(); ++p) {
										if (p > 0) {
											mOut.Append(", ");
										}
										mOut.Append(gn.pins[p].name);
										mOut.Append(" = ");
										WriteExpr(gn.pinExprs[p]);
									}
									mOut.Append(" }");
								}
								NewLine();
							}
							for (uint32 i = 0; i < (uint32)b.wires.Size(); ++i) {
								Indent(1);
								mOut.Append("wire ");
								for (uint32 r = 0; r < (uint32)b.wires[i].refs.Size(); ++r) {
									if (r > 0) {
										mOut.Append(" -> ");
									}
									mOut.Append(b.wires[i].refs[r]);
								}
								NewLine();
							}
						} else {
							for (uint32 i = 0; i < (uint32)b.stmts.Size(); ++i) {
								WriteStmt(b.stmts[i], 1);
							}
						}
						mOut.Append('}');
						NewLine();
						return;
					}
					if (ref.kind == NkGSectionKind::Controller) {
						const NkGController &c = mDoc->controllers[ref.index];
						mOut.Append("controller ");
						WriteQuoted(c.name);
						mOut.Append(" {");
						NewLine();
						for (uint32 i = 0; i < (uint32)c.callbacks.Size(); ++i) {
							WriteCallbackSig(mDoc->callbacks[c.callbacks[i]], 1);
						}
						mOut.Append('}');
						NewLine();
						return;
					}
					WriteCallbackSig(mDoc->callbacks[ref.index], 0);
				}
		};

		// ═══════════════════════════════════════════════════════════════════════
		//  L'API PUBLIQUE
		// ═══════════════════════════════════════════════════════════════════════

		/// Analyse `text` dans `doc`. En cas d'echec, `err` porte le code, le
		/// message, la ligne et la colonne — et `doc` est laisse VIDE.
		///
		/// ⚠️ VIDE, PAS A MOITIE REMPLI. Un document partiel apres une erreur est le
		///    pire des etats : l'appelant qui oublie de tester le retour affiche une
		///    interface amputee au lieu de dire que le fichier est casse.
		inline bool NkGParse(const char *text, uint32 length, NkGDocument &doc, NkGDiag &err) {
			doc.Clear();
			if (!text) {
				err.code = NkString("E-PARSE");
				err.message = NkString("aucun contenu a analyser");
				return false;
			}
			NkVector<NkGToken> toks;
			NkGLexer lex(text, length);
			if (!lex.Run(toks, err)) {
				doc.Clear();
				return false;
			}
			NkGParser parser(toks, doc);
			if (!parser.ParseFile(err)) {
				doc.Clear();
				return false;
			}
			return true;
		}

		inline bool NkGParse(const NkString &text, NkGDocument &doc, NkGDiag &err) {
			return NkGParse(text.Data(), (uint32)text.Size(), doc, err);
		}

		inline NkString NkGWrite(const NkGDocument &doc, const NkGWriteOptions &opt) {
			NkGWriter w(doc, opt);
			return w.Run();
		}

		inline NkString NkGWrite(const NkGDocument &doc) {
			return NkGWrite(doc, NkGWriteOptions());
		}

		// ═══════════════════════════════════════════════════════════════════════
		//  LA COMPARAISON — c'est elle qui rend l'aller-retour verifiable
		// ═══════════════════════════════════════════════════════════════════════

		/// ⚠️ CETTE COMPARAISON EST LE CRITERE D'ACCEPTATION, elle merite donc d'etre
		///    lue avec autant d'attention que le parseur. Elle compare la SEMANTIQUE
		///    (roles, identifiants, valeurs, ordre des membres), **pas** la mise en
		///    forme. Deux documents egaux au sens de cette fonction decrivent la meme
		///    interface, meme si l'un indente de deux espaces et l'autre de quatre.
		bool NkGEqualNode(const NkGDocument &a, uint32 ia, const NkGDocument &b, uint32 ib);

		inline bool NkGEqualValue(const NkGValue &a, const NkGValue &b) {
			if (a.kind != b.kind) {
				return false;
			}
			if (a.kind == NkGValueKind::String) {
				return a.text.Compare(b.text) == 0;
			}
			// Pour tout le reste on compare le LEXEME : c'est ce que le serialiseur
			// reemet, donc c'est ce qui doit survivre. Comparer la valeur decodee
			// laisserait passer un `0.20` devenu `0.2`.
			return a.raw.Compare(b.raw) == 0;
		}

		inline bool NkGEqualExpr(const NkGDocument &a, uint32 ia, const NkGDocument &b, uint32 ib) {
			if (ia == kNoIndex || ib == kNoIndex) {
				return ia == ib;
			}
			const NkGExpr &x = a.exprs[ia];
			const NkGExpr &y = b.exprs[ib];
			if (x.kind != y.kind) {
				return false;
			}
			switch (x.kind) {
				case NkGExprKind::Literal:
					return NkGEqualValue(x.literal, y.literal);
				case NkGExprKind::Ident:
					return x.ident.Compare(y.ident) == 0;
				case NkGExprKind::Paren:
					return NkGEqualExpr(a, x.lhs, b, y.lhs);
				default:
					break;
			}
			if (x.op.Compare(y.op) != 0) {
				return false;
			}
			return NkGEqualExpr(a, x.lhs, b, y.lhs) && NkGEqualExpr(a, x.rhs, b, y.rhs);
		}

		inline bool NkGEqualExprList(const NkGDocument &a, const NkVector<uint32> &xa,
									 const NkGDocument &b, const NkVector<uint32> &xb) {
			if (xa.Size() != xb.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)xa.Size(); ++i) {
				if (!NkGEqualExpr(a, xa[i], b, xb[i])) {
					return false;
				}
			}
			return true;
		}

		inline bool NkGEqualPropList(const NkVector<NkGProp> &xa, const NkVector<NkGProp> &xb) {
			if (xa.Size() != xb.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)xa.Size(); ++i) {
				if (xa[i].name.Compare(xb[i].name) != 0) {
					return false;
				}
				if (!NkGEqualValue(xa[i].value, xb[i].value)) {
					return false;
				}
			}
			return true;
		}

		bool NkGEqualStmt(const NkGDocument &a, uint32 ia, const NkGDocument &b, uint32 ib);

		inline bool NkGEqualStmtList(const NkGDocument &a, const NkVector<uint32> &xa,
									 const NkGDocument &b, const NkVector<uint32> &xb) {
			if (xa.Size() != xb.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)xa.Size(); ++i) {
				if (!NkGEqualStmt(a, xa[i], b, xb[i])) {
					return false;
				}
			}
			return true;
		}

		inline bool NkGEqualStmt(const NkGDocument &a, uint32 ia, const NkGDocument &b, uint32 ib) {
			if (ia == kNoIndex || ib == kNoIndex) {
				return ia == ib;
			}
			const NkGStmt &x = a.stmts[ia];
			const NkGStmt &y = b.stmts[ib];
			if (x.kind != y.kind || x.hasElse != y.hasElse) {
				return false;
			}
			if (x.name.Compare(y.name) != 0) {
				return false;
			}
			if (!NkGEqualExpr(a, x.expr, b, y.expr)) {
				return false;
			}
			if (!NkGEqualExprList(a, x.args, b, y.args)) {
				return false;
			}
			return NkGEqualStmtList(a, x.thenStmts, b, y.thenStmts)
				   && NkGEqualStmtList(a, x.elseStmts, b, y.elseStmts);
		}

		inline bool NkGEqualEvent(const NkGDocument &a, const NkGEvent &x, const NkGDocument &b,
								  const NkGEvent &y) {
			if (x.name.Compare(y.name) != 0 || x.hasParams != y.hasParams || x.action != y.action) {
				return false;
			}
			if (x.target.Compare(y.target) != 0) {
				return false;
			}
			if (x.params.Size() != y.params.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)x.params.Size(); ++i) {
				if (x.params[i].Compare(y.params[i]) != 0) {
					return false;
				}
			}
			return NkGEqualExprList(a, x.args, b, y.args)
				   && NkGEqualStmtList(a, x.stmts, b, y.stmts);
		}

		inline bool NkGEqualNode(const NkGDocument &a, uint32 ia, const NkGDocument &b, uint32 ib) {
			const NkGNode &x = a.nodes[ia];
			const NkGNode &y = b.nodes[ib];
			if (x.kind.Compare(y.kind) != 0 || x.hasId != y.hasId
				|| x.id.Compare(y.id) != 0) {
				return false;
			}
			if (x.members.Size() != y.members.Size()) {
				return false;
			}
			if (!NkGEqualPropList(x.props, y.props)) {
				return false;
			}
			if (x.events.Size() != y.events.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)x.events.Size(); ++i) {
				if (!NkGEqualEvent(a, x.events[i], b, y.events[i])) {
					return false;
				}
			}
			if (x.children.Size() != y.children.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)x.members.Size(); ++i) {
				if (x.members[i].kind != y.members[i].kind
					|| x.members[i].index != y.members[i].index) {
					return false;
				}
			}
			for (uint32 i = 0; i < (uint32)x.children.Size(); ++i) {
				if (!NkGEqualNode(a, x.children[i], b, y.children[i])) {
					return false;
				}
			}
			return true;
		}

		inline bool NkGEqualCallback(const NkGCallbackSig &x, const NkGCallbackSig &y) {
			if (x.name.Compare(y.name) != 0 || x.ret.Compare(y.ret) != 0
				|| x.retIsEnum != y.retIsEnum) {
				return false;
			}
			if (x.params.Size() != y.params.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)x.params.Size(); ++i) {
				if (x.params[i].name.Compare(y.params[i].name) != 0
					|| x.params[i].type.Compare(y.params[i].type) != 0
					|| x.params[i].isEnum != y.params[i].isEnum) {
					return false;
				}
				if (x.params[i].enumLabels.Size() != y.params[i].enumLabels.Size()) {
					return false;
				}
				for (uint32 j = 0; j < (uint32)x.params[i].enumLabels.Size(); ++j) {
					if (x.params[i].enumLabels[j].Compare(y.params[i].enumLabels[j]) != 0) {
						return false;
					}
				}
			}
			return true;
		}

		inline bool NkGEqual(const NkGDocument &a, const NkGDocument &b) {
			if (a.versionMajor.Compare(b.versionMajor) != 0
				|| a.versionMinor.Compare(b.versionMinor) != 0) {
				return false;
			}
			if (a.includes.Size() != b.includes.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)a.includes.Size(); ++i) {
				if (a.includes[i].Compare(b.includes[i]) != 0) {
					return false;
				}
			}
			if (a.sections.Size() != b.sections.Size()) {
				return false;
			}
			for (uint32 s = 0; s < (uint32)a.sections.Size(); ++s) {
				const NkGSection &ra = a.sections[s];
				const NkGSection &rb = b.sections[s];
				if (ra.kind != rb.kind) {
					return false;
				}
				if (ra.kind == NkGSectionKind::Widgets) {
					const NkGTopSection &xa = a.topSections[ra.index];
					const NkGTopSection &xb = b.topSections[rb.index];
					if (xa.roots.Size() != xb.roots.Size()) {
						return false;
					}
					for (uint32 i = 0; i < (uint32)xa.roots.Size(); ++i) {
						if (!NkGEqualNode(a, xa.roots[i], b, xb.roots[i])) {
							return false;
						}
					}
					continue;
				}
				if (ra.kind == NkGSectionKind::Geometry) {
					const NkGTopSection &xa = a.topSections[ra.index];
					const NkGTopSection &xb = b.topSections[rb.index];
					if (xa.shapes.Size() != xb.shapes.Size()) {
						return false;
					}
					for (uint32 i = 0; i < (uint32)xa.shapes.Size(); ++i) {
						const NkGShape &sa = a.shapes[xa.shapes[i]];
						const NkGShape &sb = b.shapes[xb.shapes[i]];
						if (sa.name.Compare(sb.name) != 0 || !NkGEqualPropList(sa.props, sb.props)) {
							return false;
						}
					}
					continue;
				}
				if (ra.kind == NkGSectionKind::Behavior) {
					const NkGBehavior &ba = a.behaviors[ra.index];
					const NkGBehavior &bb = b.behaviors[rb.index];
					if (ba.name.Compare(bb.name) != 0 || ba.isGraph != bb.isGraph) {
						return false;
					}
					if (!NkGEqualStmtList(a, ba.stmts, b, bb.stmts)) {
						return false;
					}
					if (ba.gnodes.Size() != bb.gnodes.Size()
						|| ba.wires.Size() != bb.wires.Size()) {
						return false;
					}
					for (uint32 i = 0; i < (uint32)ba.gnodes.Size(); ++i) {
						if (ba.gnodes[i].name.Compare(bb.gnodes[i].name) != 0
							|| ba.gnodes[i].type.Compare(bb.gnodes[i].type) != 0
							|| ba.gnodes[i].hasPins != bb.gnodes[i].hasPins) {
							return false;
						}
						if (ba.gnodes[i].pins.Size() != bb.gnodes[i].pins.Size()) {
							return false;
						}
						for (uint32 p = 0; p < (uint32)ba.gnodes[i].pins.Size(); ++p) {
							if (ba.gnodes[i].pins[p].name.Compare(bb.gnodes[i].pins[p].name) != 0) {
								return false;
							}
						}
						if (!NkGEqualExprList(a, ba.gnodes[i].pinExprs, b,
											  bb.gnodes[i].pinExprs)) {
							return false;
						}
					}
					for (uint32 i = 0; i < (uint32)ba.wires.Size(); ++i) {
						if (ba.wires[i].refs.Size() != bb.wires[i].refs.Size()) {
							return false;
						}
						for (uint32 r = 0; r < (uint32)ba.wires[i].refs.Size(); ++r) {
							if (ba.wires[i].refs[r].Compare(bb.wires[i].refs[r]) != 0) {
								return false;
							}
						}
					}
					continue;
				}
				if (ra.kind == NkGSectionKind::Controller) {
					const NkGController &ca = a.controllers[ra.index];
					const NkGController &cb = b.controllers[rb.index];
					if (ca.name.Compare(cb.name) != 0
						|| ca.callbacks.Size() != cb.callbacks.Size()) {
						return false;
					}
					for (uint32 i = 0; i < (uint32)ca.callbacks.Size(); ++i) {
						if (!NkGEqualCallback(a.callbacks[ca.callbacks[i]],
											  b.callbacks[cb.callbacks[i]])) {
							return false;
						}
					}
					continue;
				}
				if (!NkGEqualCallback(a.callbacks[ra.index], b.callbacks[rb.index])) {
					return false;
				}
			}
			return true;
		}

	} // namespace guifmt
} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_NKGUIFORMAT_H__
