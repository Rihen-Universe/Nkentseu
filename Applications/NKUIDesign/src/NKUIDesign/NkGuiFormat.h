//
// NkGuiFormat.h
// =============================================================================
// Description :
//   Le LECTEUR et l'ECRIVAIN du format `.nkgui` **v0.3** -- lexeur, analyseur
//   descendant recursif, modele de document en memoire, et serialiseur.
//   La reference est le document 2
//   (`2_NkUIDesign_Langage_Description_NodeBlueprint.md`) pour le socle v0.2, et
//   `design/9_Grammaire_complete.md` pour ce que la v0.3 ajoute.
//
// Ce que la v0.3 ajoute au socle v0.2 (decisions de Rodolf, 2026-08-21) :
//   - LISTES `[a, b, c]` et DICTIONNAIRES `{ cle = valeur, ... }` comme valeurs.
//     Sans elles, `columns[]`, `items[]`, `tabs[]`, `values[]`, `sizes[]` ne
//     s'ecrivaient pas : cinq roles etaient inecrivables (doc 9 §6.1) ;
//   - l'IDENTIFIANT POINTE (`n1.value`, `Enum.X`, `a.b.c`) dans `expr`, que le
//     document 2 emploie dans ses propres exemples (doc 9 §6.2) ;
//   - les COMMENTAIRES et LIGNES VIDES survivent a l'aller-retour ;
//   - les blocs `appearance` portes par le widget, la section `animation` et la
//     section `fonts` (doc 9 §3, §4, §5) ;
//   - la VALIDATION par role, dans `NkGuiValidate.h` -- separee volontairement :
//     lire n'est pas juger, et un document invalide doit rester LISIBLE.
//
// =============================================================================
//  ATTENTION -- LA COMPATIBILITE ASCENDANTE PRIME SUR TOUT LE RESTE
// =============================================================================
//  Rodolf, mot pour mot : « rassure-toi que le systeme pourra facilement etre
//  mis a jour sans tout detruire et sans bug. » Quatre regles la tiennent, et
//  elles sont dans le code, pas seulement dans ce commentaire :
//
//   (a) LE FICHIER PORTE SA VERSION. `nkgui <majeure>.<mineure>`, relue et
//       reemise telle quelle. Un document ecrit en 0.2 se reecrit en 0.2 : un
//       outil qui reestampille silencieusement les fichiers qu'il touche rend
//       tout diagnostic de version impossible.
//
//   (b) UN LECTEUR RECENT LIT TOUS LES FICHIERS ANCIENS. Rien de la v0.2 n'a
//       ete retire ni resserre. Le corpus 0.2 reste a 10/10.
//
//   (c) UN LECTEUR ANCIEN REFUSE CLAIREMENT UN FICHIER PLUS RECENT. Une
//       MAJEURE superieure = rupture : `E-VERSION-INCOMPATIBLE`, et on ne lit
//       pas. « ce fichier est trop recent pour moi » vaut mieux que l'ouvrir en
//       perdant la moitie.
//
//   (d) CE QU'ON NE COMPREND PAS, ON LE PRESERVE TEL QUEL. Une MINEURE
//       superieure = ajout compatible : le fichier se lit, et toute section
//       (ou tout membre de widget) inconnu est garde **en TEXTE BRUT**
//       (`NkGRaw`) puis reemis a l'octet pres.
//
//       C'est la regle qui fait le travail, et elle impose sa forme au code :
//       **on ne peut pas preserver ce qu'on ne sait pas modeliser en le faisant
//       passer par le modele.** Il faut garder la tranche de source. Sans ca,
//       la version 0.4 ajouterait une section, et le premier outil 0.3 qui
//       ouvrirait puis enregistrerait un document 0.4 la supprimerait -- sans
//       message, sans trace, et sans que personne ne s'en apercoive avant que
//       le document ne serve.
//
// Caracteristiques :
//   - zero-STL : `NkString` / `NkVector` (NKContainers), allocateurs NKMemory ;
//   - ARENES PLATES : noeuds, valeurs, expressions et instructions vivent dans
//     des `NkVector` du document, references par INDICE. Aucun pointeur vers
//     l'interieur d'un conteneur qui peut se reallouer ;
//   - ORDRE D'ECRITURE PRESERVE : un noeud garde la suite exacte de ses membres
//     (propriete / evenement / apparence / enfant), et le fichier garde la suite
//     exacte de ses sections ;
//   - LEXEMES CONSERVES : un nombre, une couleur, un vecteur et un identifiant
//     sont reemis TELS QU'ILS ONT ETE LUS. Le document 2 ne definit aucune forme
//     canonique pour eux ; normaliser reecrirait le document de l'auteur ;
//   - TRIVIA CONSERVEE : chaque element porte les lignes de commentaire et les
//     lignes vides qui le precedent, **verbatim**, plus le commentaire de fin de
//     sa propre ligne. C'est ce qui rend l'aller-retour sur un fichier ecrit a
//     la main non destructeur.
//
// Algorithmes implementes :
//   - analyse descendante recursive a un seul jeton d'avance (LL(1)) ;
//   - analyse des expressions par PRECEDENCE GRIMPANTE (precedence climbing) ;
//   - rattachement de la trivia par BALAYAGE DES INTERVALLES entre jetons : le
//     lexeur note l'offset de debut et de fin de chaque jeton, et une passe
//     unique decoupe le texte laisse entre deux jetons en « commentaire de fin
//     de ligne du precedent » + « lignes qui precedent le suivant ».
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

		/// La version que CE lecteur comprend. Regle (c) : au-dela de la majeure,
		/// on refuse ; au-dela de la mineure, on lit et on preserve (regle (d)).
		static const int32 kFormatMajor = 0;
		static const int32 kFormatMinor = 3;

		// =====================================================================
		//  LA TRIVIA  (commentaires et lignes vides)
		// =====================================================================

		/// LA TRIVIA EST STOCKEE VERBATIM, INDENTATION COMPRISE, et ce n'est pas
		/// de la paresse : une ligne de commentaire n'a pas de « profondeur »
		/// dans le modele -- elle peut commenter le bloc, le membre suivant, ou
		/// rien du tout. La reindenter, c'est decider a la place de l'auteur ;
		/// la recopier, c'est ne rien decider. Un commentaire de bloc sur
		/// plusieurs lignes est simplement decoupe en autant d'entrees, et se
		/// recolle a l'identique a l'ecriture.
		struct NkGTrivia {
				NkVector<NkString> lead;  ///< lignes completes AVANT l'element ("" = ligne vide)
				NkString trail;			  ///< le reste de la ligne APRES l'element, verbatim

				bool Empty() const {
					return lead.Size() == 0 && trail.Empty();
				}
		};

		/// Ce qu'un BLOC porte en plus : le commentaire colle a son accolade
		/// ouvrante, et les lignes qui precedent sa fermante. Sans ces deux-la,
		/// un commentaire en fin de bloc disparait.
		struct NkGBlockTrivia {
				NkString openTrail;	 ///< apres l'accolade ouvrante, verbatim
				NkGTrivia tail;		 ///< lignes avant l'accolade fermante

				bool Empty() const {
					return openTrail.Empty() && tail.lead.Size() == 0;
				}
		};

		inline bool NkGEqualTrivia(const NkGTrivia &a, const NkGTrivia &b) {
			if (a.lead.Size() != b.lead.Size() || a.trail.Compare(b.trail) != 0) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)a.lead.Size(); ++i) {
				if (a.lead[i].Compare(b.lead[i]) != 0) {
					return false;
				}
			}
			return true;
		}

		inline bool NkGEqualBlockTrivia(const NkGBlockTrivia &a, const NkGBlockTrivia &b) {
			return a.openTrail.Compare(b.openTrail) == 0 && NkGEqualTrivia(a.tail, b.tail);
		}

		// =====================================================================
		//  LES VALEURS  (doc 2 §3, etendu v0.3)
		//    value := String | Number | Color | Vec2 | flags | Identifier
		//           | '[' (value (',' value)*)? ']'                     (v0.3)
		//           | '{' (key '=' value (',' ...)*)? '}'               (v0.3)
		// =====================================================================

		enum class NkGValueKind : uint8 {
			None = 0,
			String,	 ///< contenu entre guillemets -- `text` porte le contenu DECODE
			Number,	 ///< -?[0-9]+(\.[0-9]+)?
			Color,	 ///< #RRGGBB ou #RRGGBBAA
			Vec2,	 ///< (x, y)
			Ident,	 ///< un identifiant seul, eventuellement pointe (`Enum.X`)
			Flags,	 ///< A | B | C
			List,	 ///< [ ... ]         -- v0.3
			Dict	 ///< { cle = ... }   -- v0.3
		};

		/// DEUX FORMES COHABITENT, ET C'EST VOULU : `text` est la forme LUE
		/// (contenu decode d'une chaine), `raw` est la forme ECRITE (le lexeme
		/// source, guillemets compris pour une chaine).
		///
		/// Le serialiseur reemet `raw` pour tout sauf les chaines, qu'il
		/// re-encode depuis `text`, et sauf les listes/dictionnaires, qu'il
		/// reconstruit depuis leurs elements. Raison : le document 2 ne definit
		/// aucune forme canonique pour un nombre (`0.20` vaut `0.2`), une
		/// couleur (`#ff0000` vaut `#FF0000`) ni un vecteur (`(1,2)` vaut
		/// `(1, 2)`). **Normaliser reecrirait silencieusement le fichier de
		/// l'auteur** -- un diff de trois cents lignes le lendemain d'un simple
		/// enregistrement, sans qu'aucune valeur n'ait change.
		struct NkGValue {
				NkGValueKind kind = NkGValueKind::None;
				NkString text;	 ///< String : contenu decode. Ident/Flags : le texte.
				NkString raw;	 ///< le lexeme source, tel quel
				float64 num = 0.0;	  ///< Number
				uint32 color = 0;	  ///< Color, en RGBA8
				float64 vx = 0.0;	  ///< Vec2
				float64 vy = 0.0;	  ///< Vec2

				/// List : les elements. Dict : les VALEURS, alignees sur `keys`.
				/// Indices dans `NkGDocument::values` -- une valeur ne peut pas se
				/// contenir elle-meme par valeur, et une arene evite d'inventer un
				/// type recursif que `NkVector` ne saurait pas instancier.
				NkVector<uint32> items;
				NkVector<NkString> keys;	 ///< Dict : les cles, DECODEES
				NkVector<uint8> keyQuoted;	 ///< Dict : 1 si la cle etait une chaine
		};

		struct NkGProp {
				NkString name;
				NkGValue value;
				NkGTrivia tv;
				/// La LIGNE SOURCE, pour le diagnostic seul. Un message de validation
				/// sans ligne oblige a chercher dans un fichier de trois mille noeuds ;
				/// elle ne participe ni a l'egalite ni a l'ecriture.
				uint32 line = 0;
		};

		// =====================================================================
		//  LES EXPRESSIONS  (doc 2 §3, section behavior)
		// =====================================================================

		enum class NkGExprKind : uint8 {
			Literal = 0,  ///< String | Number | Color | true | false
			Ident,		  ///< un identifiant, POINTE ou non (`n1.value`, `Enum.X`, `a.b.c`)
			Binary,		  ///< lhs op rhs
			Paren		  ///< ( inner ) -- conserve pour reemettre le groupement
		};

		struct NkGExpr {
				NkGExprKind kind = NkGExprKind::Literal;
				NkGValue literal;	 ///< Literal
				NkString ident;		 ///< Ident : le chemin COMPLET, points compris
				NkString op;		 ///< Binary
				uint32 lhs = kNoIndex;
				uint32 rhs = kNoIndex;	 ///< Paren : reutilise `lhs`
		};

		/// Decoupe `a.b.c` en ses segments. Le modele garde le chemin ENTIER dans
		/// `ident` -- c'est lui qui est reemis, donc c'est lui qui fait foi. Cette
		/// fonction est un service pour qui CONSOMME le document (resolution de
		/// pin, resolution d'enum) ; elle ne participe pas a l'aller-retour.
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
		//  LES INSTRUCTIONS  (doc 2 §5)
		// =====================================================================

		enum class NkGStmtKind : uint8 {
			Assign = 0,	 ///< set Identifier = expr
			If,			 ///< if expr { ... } else { ... }
			Call		 ///< Callback "X"(args)
		};

		struct NkGStmt {
				NkGStmtKind kind = NkGStmtKind::Assign;
				NkString name;			  ///< Assign : la variable. Call : le nom du callback.
				uint32 expr = kNoIndex;	  ///< Assign : la valeur. If : la condition.
				NkVector<uint32> args;	  ///< Call
				NkVector<uint32> thenStmts;
				NkVector<uint32> elseStmts;
				bool hasElse = false;
				NkGTrivia tv;
				NkGBlockTrivia blkThen;
				NkGBlockTrivia blkElse;
		};

		// =====================================================================
		//  LES EVENEMENTS  (doc 2 §3 : event_decl)
		// =====================================================================

		enum class NkGActionKind : uint8 {
			Callback = 0,  ///< Callback "X"(args)
			Behavior,	   ///< Behavior "X"
			Inline		   ///< { statement* }
		};

		struct NkGEvent {
				NkString name;				 ///< l'identifiant apres `on`
				NkVector<NkString> params;	 ///< (value, text)
				bool hasParams = false;		 ///< distingue `on Click` de `on Click()`
				NkGActionKind action = NkGActionKind::Callback;
				NkString target;		 ///< nom du callback ou du behavior
				NkVector<uint32> args;	 ///< Callback
				NkVector<uint32> stmts;	 ///< Inline
				NkGTrivia tv;
				NkGBlockTrivia blk;	 ///< Inline
		};

		// =====================================================================
		//  L'APPARENCE  (doc 9 §3 -- v0.3)
		//    appearance_blk    := "appearance" ('(' Identifier ')')? '{' m* '}'
		//    appearance_member := prop_decl | effect_blk
		//    effect_blk        := ("fill"|"stroke"|"shadow"|"blur") String? '{' p* '}'
		// =====================================================================

		/// `oneLine` N'EST PAS DE LA COSMETIQUE. Le document 9 ecrit ses effets sur
		/// une seule ligne (`fill { color = #2F6F7A }`) ; les reemettre eclates
		/// ferait diverger le fichier de son auteur des le premier enregistrement.
		/// On note donc la forme lue et on la rend.
		struct NkGEffect {
				NkString kind;	 ///< fill | stroke | shadow | blur
				NkString name;	 ///< le String? optionnel (doc 9 §4.3 : nommer une ombre)
				bool hasName = false;
				NkVector<NkGProp> props;
				bool oneLine = false;	 ///< le bloc tenait sur une ligne
				bool commas = false;	 ///< ses proprietes etaient separees par des virgules
				NkGTrivia tv;
				NkGBlockTrivia blk;
		};

		enum class NkGAppMemberKind : uint8 { Prop = 0, Effect };

		struct NkGAppMember {
				NkGAppMemberKind kind = NkGAppMemberKind::Prop;
				uint32 index = kNoIndex;
		};

		struct NkGAppearance {
				NkString state;	 ///< appearance(Hover) -- doc 9 §3.2 : liste non fermee
				bool hasState = false;
				NkVector<NkGProp> props;
				NkVector<NkGEffect> effects;
				NkVector<NkGAppMember> members;	 ///< l'ordre du fichier
				NkGTrivia tv;
				NkGBlockTrivia blk;
		};

		// =====================================================================
		//  LES NOEUDS DE WIDGETS  (doc 2 §3 : node_decl, etendu doc 9 §3.1)
		// =====================================================================

		/// CE TYPE EXISTE POUR UNE SEULE RAISON, ET ELLE EST L'ALLER-RETOUR.
		/// `node_decl` autorise ses membres dans N'IMPORTE QUEL ORDRE. Les ranger
		/// dans des listes separees et les reemettre « proprietes d'abord » suffit
		/// a rendre le fichier different de l'original -- et un outil qui reordonne
		/// un document a chaque enregistrement produit des diffs que personne ne
		/// peut relire.
		///
		/// `Raw` est le membre de la regle (d) : un membre qu'un lecteur plus
		/// ancien ne comprend pas, garde en texte brut et reemis tel quel.
		enum class NkGMemberKind : uint8 { Prop = 0, Event, Child, Appearance, Raw };

		struct NkGMember {
				NkGMemberKind kind = NkGMemberKind::Prop;
				uint32 index = kNoIndex;  ///< indice dans props / events / children / ...
		};

		struct NkGNode {
				NkString kind;	 ///< Kind -- n'importe quel identifiant (validation : NkGuiValidate.h)
				NkString id;	 ///< le String? optionnel
				bool hasId = false;
				NkVector<NkGProp> props;
				NkVector<NkGEvent> events;
				NkVector<uint32> children;	 ///< indices dans NkGDocument::nodes
				NkVector<NkGAppearance> appearances;
				NkVector<uint32> raws;		  ///< indices dans NkGDocument::raws
				NkVector<NkGMember> members;  ///< l'ordre du fichier
				NkGTrivia tv;
				NkGBlockTrivia blk;
				uint32 line = 0;  ///< pour le diagnostic seul (cf. NkGProp::line)
		};

		// =====================================================================
		//  LA GEOMETRIE  (doc 2 §3 : geometry_sec)
		// =====================================================================

		struct NkGShape {
				NkString name;
				NkVector<NkGProp> props;
				NkGTrivia tv;
				NkGBlockTrivia blk;
		};

		// =====================================================================
		//  LE COMPORTEMENT  (doc 2 §5 et §6)
		// =====================================================================

		struct NkGGraphNode {
				NkString name;	 ///< node <name> <type>
				NkString type;
				NkVector<NkGProp> pins;		 ///< pin_init, valeurs = expressions
				NkVector<uint32> pinExprs;	 ///< indices dans NkGDocument::exprs
				bool hasPins = false;		 ///< distingue `node n1 X` de `node n1 X { }`
				NkGTrivia tv;
		};

		struct NkGWire {
				NkVector<NkString> refs;  ///< a.b -> c.d -> e.f, en texte tel que lu
				NkGTrivia tv;
		};

		struct NkGBehavior {
				NkString name;
				bool isGraph = false;
				NkVector<uint32> stmts;	 ///< script
				NkVector<NkGGraphNode> gnodes;
				NkVector<NkGWire> wires;
				/// L'ordre du fichier pour un graphe : Prop = noeud, Event = fil.
				/// (on reutilise NkGMember plutot que d'inventer un enum de plus)
				NkVector<NkGMember> gorder;
				NkGBlockTrivia blk;
		};

		// =====================================================================
		//  L'ANIMATION  (doc 9 §4 -- v0.3)
		// =====================================================================

		struct NkGAnimKey {
				NkString at;	 ///< le lexeme du temps, tel que lu
				NkGValue value;
				NkString curve;
				bool hasCurve = false;
				NkGTrivia tv;
		};

		struct NkGAnimTrack {
				NkString name;	 ///< un CHEMIN de propriete ("scale", "shadow.blur")
				NkVector<NkGAnimKey> keys;
				NkGTrivia tv;
				NkGBlockTrivia blk;
		};

		struct NkGAnimMap {
				NkVector<NkGProp> props;
				NkGTrivia tv;
				NkGBlockTrivia blk;
		};

		enum class NkGAnimFamily : uint8 { Transition = 0, Ambience, Continuous };
		enum class NkGAnimMemberKind : uint8 { Prop = 0, Track, Map };

		struct NkGAnimMember {
				NkGAnimMemberKind kind = NkGAnimMemberKind::Prop;
				uint32 index = kNoIndex;
		};

		struct NkGAnimDecl {
				NkGAnimFamily family = NkGAnimFamily::Transition;
				NkString name;
				NkVector<NkGProp> props;
				NkVector<NkGAnimTrack> tracks;
				NkVector<NkGAnimMap> maps;
				NkVector<NkGAnimMember> members;
				NkGTrivia tv;
				NkGBlockTrivia blk;
		};

		struct NkGAnimation {
				NkString name;
				bool hasName = false;
				NkVector<NkGAnimDecl> decls;
				NkGBlockTrivia blk;
		};

		// =====================================================================
		//  LES POLICES  (doc 9 §5 -- v0.3)
		// =====================================================================

		struct NkGFontMetric {
				bool isGlyph = false;  ///< glyph "A" -> 712
				NkString glyph;		   ///< le glyphe, DECODE
				NkString name;		   ///< sinon : unitsPerEm, lineHeight, ...
				NkGValue value;
				NkGTrivia tv;
		};

		enum class NkGFontBlockKind : uint8 { Source = 0, Fallback, Metrics };

		struct NkGFontBlock {
				NkGFontBlockKind kind = NkGFontBlockKind::Source;
				NkVector<NkGProp> props;		  ///< Source et Fallback
				NkVector<NkGFontMetric> metrics;  ///< Metrics
				NkGTrivia tv;
				NkGBlockTrivia blk;
		};

		enum class NkGFontMemberKind : uint8 { Prop = 0, Block };

		struct NkGFontMember {
				NkGFontMemberKind kind = NkGFontMemberKind::Prop;
				uint32 index = kNoIndex;
		};

		struct NkGFont {
				NkString name;
				NkVector<NkGProp> props;
				NkVector<NkGFontBlock> blocks;
				NkVector<NkGFontMember> members;
				NkGTrivia tv;
				NkGBlockTrivia blk;
		};

		struct NkGFonts {
				NkVector<NkGFont> fonts;
				NkGBlockTrivia blk;
		};

		// =====================================================================
		//  LE TEXTE BRUT PRESERVE  --  LA REGLE (d)
		// =====================================================================

		/// Une tranche de source qu'on ne sait pas modeliser et qu'on garde donc
		/// **telle quelle**. C'est le seul moyen honnete de survivre a un fichier
		/// ecrit par une version plus recente du format : on ne peut pas preserver
		/// ce qu'on ne sait pas representer en le faisant passer par le modele.
		struct NkGRaw {
				NkString text;	 ///< la tranche EXACTE du fichier source
				NkString what;	 ///< le mot-cle qui l'ouvrait, pour le diagnostic
				NkGTrivia tv;
		};

		// =====================================================================
		//  LE DOCUMENT
		// =====================================================================

		enum class NkGSectionKind : uint8 {
			Geometry = 0,
			Widgets,
			Behavior,
			Controller,
			Callback,
			Animation,
			Fonts,
			Raw
		};

		struct NkGSection {
				NkGSectionKind kind = NkGSectionKind::Widgets;
				uint32 index = kNoIndex;  ///< indice dans l'arene correspondante
				NkGTrivia tv;
		};

		/// Une section geometry ou widgets : ses membres de premier niveau.
		struct NkGTopSection {
				NkVector<uint32> shapes;  ///< Geometry : indices dans shapes
				NkVector<uint32> roots;	  ///< Widgets : indices dans nodes
				NkGBlockTrivia blk;
		};

		struct NkGInclude {
				NkString path;
				NkGTrivia tv;
		};

		// =====================================================================
		//  LES CONTRATS  (doc 2 §10)
		// =====================================================================

		struct NkGParam {
				NkString name;
				NkString type;					///< Void|Bool|... tel que lu
				NkVector<NkString> enumLabels;	///< Enum[X,Y,Z]
				bool isEnum = false;
		};

		struct NkGCallbackSig {
				NkString name;
				NkVector<NkGParam> params;
				NkString ret;
				NkVector<NkString> retEnumLabels;
				bool retIsEnum = false;
				NkGTrivia tv;
		};

		struct NkGController {
				NkString name;
				NkVector<uint32> callbacks;	 ///< indices dans NkGDocument::callbacks
				NkGBlockTrivia blk;
		};

		struct NkGDocument {
				NkString versionMajor = NkString("0");	///< lexeme, pas un nombre
				NkString versionMinor = NkString("3");
				/// Vrai quand la MINEURE du fichier depasse celle du lecteur.
				/// Regle (d) : dans ce mode, ce qu'on ne comprend pas est PRESERVE
				/// au lieu d'etre refuse.
				bool futureMinor = false;

				NkVector<NkGInclude> includes;

				// -- Les arenes ------------------------------------------------
				NkVector<NkGNode> nodes;
				NkVector<NkGShape> shapes;
				NkVector<NkGValue> values;	 ///< elements de listes / dictionnaires
				NkVector<NkGExpr> exprs;
				NkVector<NkGStmt> stmts;
				NkVector<NkGBehavior> behaviors;
				NkVector<NkGCallbackSig> callbacks;
				NkVector<NkGController> controllers;
				NkVector<NkGTopSection> topSections;
				NkVector<NkGAnimation> animations;
				NkVector<NkGFonts> fontSections;
				NkVector<NkGRaw> raws;

				/// L'ORDRE DES SECTIONS DU FICHIER. Meme raison que NkGNode::members.
				NkVector<NkGSection> sections;

				NkGTrivia headTv;  ///< avant / apres la ligne `nkgui X.Y`
				NkGTrivia tailTv;  ///< les lignes de la fin du fichier

				void Clear() {
					versionMajor = NkString("0");
					versionMinor = NkString("3");
					futureMinor = false;
					includes.Clear();
					nodes.Clear();
					shapes.Clear();
					values.Clear();
					exprs.Clear();
					stmts.Clear();
					behaviors.Clear();
					callbacks.Clear();
					controllers.Clear();
					topSections.Clear();
					animations.Clear();
					fontSections.Clear();
					raws.Clear();
					sections.Clear();
					headTv = NkGTrivia();
					tailTv = NkGTrivia();
				}
		};

		// =====================================================================
		//  LES DIAGNOSTICS  (doc 2 §12, etendu)
		// =====================================================================

		struct NkGDiag {
				NkString code;	 ///< E-PARSE, E-VERSION-INCOMPATIBLE, E-ROLE-INCONNU, ...
				NkString message;
				uint32 line = 0;
				uint32 column = 0;
		};

		// =====================================================================
		//  LE LEXEUR  (doc 2 §2)
		// =====================================================================

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
			LBracket,  ///< v0.3 : ouvre un litteral de liste
			RBracket,
			Equal,
			Comma,
			Pipe,
			Arrow,	///< ->
			Colon,
			Op	///< + - * / > < >= <= == && ||
		};

		struct NkGToken {
				NkGTok kind = NkGTok::End;
				NkString text;	 ///< Ident/Op : le texte. String : contenu DECODE.
				NkString raw;	 ///< le lexeme source exact
				uint32 line = 1;
				uint32 column = 1;
				/// Les OFFSETS, et ils ne servent pas au confort : sans eux, ni la
				/// trivia ni la preservation en texte brut de la regle (d) ne sont
				/// possibles. Les deux ont besoin de retrouver la SOURCE, pas le
				/// jeton.
				uint32 begin = 0;
				uint32 end = 0;
				/// Rempli par NkGAttachTrivia, jamais par le lexeur lui-meme.
				NkVector<NkString> lead;
				NkString trail;
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
		/// ca reste quelques centaines de milliers de jetons -- et un analyseur qui
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
							t.begin = mLen;
							t.end = mLen;
							out.PushBack(t);
							return true;
						}
						NkGToken tok;
						tok.begin = mPos;
						if (!Next(tok, out, err)) {
							return false;
						}
						tok.end = mPos;
						out.PushBack(tok);
					}
				}

				/// L'offset du premier octet significatif (apres BOM). La passe de
				/// trivia en a besoin pour ne pas prendre le BOM pour un commentaire.
				uint32 FirstOffset() const {
					if (mLen >= 3 && (uint8)mSrc[0] == 0xEF && (uint8)mSrc[1] == 0xBB
						&& (uint8)mSrc[2] == 0xBF) {
						return 3;
					}
					return 0;
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
								// Un commentaire de bloc non ferme avale la fin du fichier
								// en silence si on ne le dit pas : le document se lit
								// « valide » avec la moitie de son contenu disparue.
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

				/// Le tiret est ambigu : debut d'un nombre negatif, ou soustraction. On
				/// tranche sur le jeton PRECEDENT -- s'il peut terminer un operande,
				/// c'est une soustraction. C'est la seule information disponible sans
				/// remonter a la grammaire, et elle suffit.
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
						// L'IDENTIFIANT POINTE (`n1.value`, `Enum.X`, `a.b.c`). Le
						// document 2 §3 ne l'a pas dans `expr`, mais ses propres
						// exemples §4.1 et §6.3 l'utilisent -- c'est la decision 2 de
						// Rodolf (doc 9 §6.2). Il est lu comme UN identifiant : le
						// chemin complet est ce qui se reemet, `NkGSplitPath` sert a
						// qui doit le resoudre.
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
						if (mPos + 1 < mLen && mSrc[mPos] == kTwo[i][0]
							&& mSrc[mPos + 1] == kTwo[i][1]) {
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
							// TROIS ECHAPPEMENTS, PAS QUATRE. Le document 2 §2 en
							// definit exactement trois. En accepter d'autres en les
							// recopiant tels quels ferait perdre l'aller-retour : le
							// re-encodage doublerait la contre-oblique. Un echappement
							// inconnu est donc une ERREUR nommee, pas une tolerance.
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
					Advance();	// le diese
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

		// =====================================================================
		//  LE RATTACHEMENT DE LA TRIVIA
		// =====================================================================
		//
		//  DECISION 3 DE RODOLF : « les commentaires et les lignes vides doivent
		//  survivre a l'aller-retour ». Aujourd'hui un .nkgui ecrit a la main puis
		//  enregistre perdait ses commentaires -- « ce n'est pas normal ».
		//
		//  LE PRINCIPE, ET IL TIENT EN UNE PHRASE : tout ce que le lexeur a jete
		//  entre deux jetons est recupere VERBATIM, decoupe en lignes, et rattache
		//  soit a la fin de la ligne du jeton precedent, soit au debut de celle du
		//  jeton suivant.
		//
		//  POURQUOI UN DECOUPAGE NAIF PAR RETOUR A LA LIGNE SUFFIT, y compris pour
		//  un commentaire de bloc sur plusieurs lignes : chaque morceau est reemis
		//  tel quel, suivi d'un retour a la ligne. Recoller les morceaux redonne
		//  donc exactement la source -- meme quand la coupure tombe au milieu d'un
		//  /* ... */. Le modele n'a pas besoin de savoir que c'etait un commentaire ;
		//  il a besoin de ne rien perdre.
		//
		//  CE QUI EST DELIBEREMENT ABANDONNE : l'indentation de la derniere ligne,
		//  celle qui precede immediatement le jeton. C'est l'ecrivain qui la
		//  regenere, sinon deux regles se disputeraient la meme colonne.
		inline void NkGAttachTrivia(const char *src, uint32 len, uint32 firstOffset,
									NkVector<NkGToken> &toks) {
			uint32 prevEnd = firstOffset;
			for (uint32 i = 0; i < (uint32)toks.Size(); ++i) {
				const uint32 b = toks[i].begin;
				if (b < prevEnd) {
					prevEnd = b;
				}
				uint32 p = prevEnd;

				// -- Le morceau qui reste sur la ligne du jeton PRECEDENT --------
				uint32 q = p;
				while (q < b && src[q] != '\n') {
					++q;
				}
				if (i > 0) {
					uint32 e = q;
					if (e > p && src[e - 1] == '\r') {
						--e;
					}
					bool hasCode = false;
					for (uint32 k = p; k < e; ++k) {
						if (!NkGIsSpace(src[k])) {
							hasCode = true;
							break;
						}
					}
					if (hasCode) {
						toks[i - 1].trail = NkString(src + p, e - p);
					}
				}

				// -- Les lignes qui precedent le jeton COURANT -------------------
				if (q < b) {
					uint32 lineStart = q + 1;
					while (lineStart <= b) {
						uint32 lineEnd = lineStart;
						while (lineEnd < b && src[lineEnd] != '\n') {
							++lineEnd;
						}
						if (lineEnd < b) {
							uint32 e = lineEnd;
							if (e > lineStart && src[e - 1] == '\r') {
								--e;
							}
							toks[i].lead.PushBack(e > lineStart ? NkString(src + lineStart, e - lineStart)
																: NkString());
							lineStart = lineEnd + 1;
							continue;
						}
						// Dernier morceau : l'indentation du jeton. On ne la garde
						// que si elle porte autre chose que des blancs -- cas rare
						// d'un /* ... */ colle devant le jeton, qu'il vaut mieux
						// remonter d'une ligne que perdre.
						bool hasCode = false;
						for (uint32 k = lineStart; k < b; ++k) {
							if (!NkGIsSpace(src[k])) {
								hasCode = true;
								break;
							}
						}
						if (hasCode) {
							toks[i].lead.PushBack(NkString(src + lineStart, b - lineStart));
						}
						break;
					}
				}
				prevEnd = toks[i].end;
			}
			(void)len;
		}

		/// Decodage d'un lexeme numerique. Maison, parce que `strtod` depend de la
		/// locale : sous une locale francaise il lit `3,14` et rejette `3.14` -- et
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
			// Un #RRGGBB vaut #RRGGBBFF : sans cette ligne, une couleur opaque se
			// comparerait comme totalement transparente.
			if (n == 6) {
				v = (v << 8) | 0xFFu;
			}
			return v;
		}

		/// Ecriture d'un entier sans `printf`. Le depot journalise par NKLogger ;
		/// un banc qui formate a la main garde la meme discipline jusqu'au bout.
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

		// =====================================================================
		//  L'ANALYSEUR
		// =====================================================================

		class NkGParser {
			public:
				NkGParser(const NkVector<NkGToken> &toks, NkGDocument &doc, const char *src)
					: mToks(&toks), mDoc(&doc), mSrc(src) {}

				bool ParseFile(NkGDiag &err) {
					// file := "nkgui" version_lit include* section*
					mDoc->headTv.lead = (*mToks)[0].lead;
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
					mDoc->headTv.trail = (*mToks)[mPos - 1].trail;
					uint32 dot = 0;
					while (dot < (uint32)ver.Size() && ver[dot] != '.') {
						++dot;
					}
					if (dot >= (uint32)ver.Size()) {
						return Fail(err, "version attendue sous la forme <majeure>.<mineure>");
					}
					mDoc->versionMajor = ver.SubStr(0, dot);
					mDoc->versionMinor = ver.SubStr(dot + 1);

					if (!CheckVersion(err)) {
						return false;
					}

					while (Peek().kind == NkGTok::Ident && Peek().text.Compare("include") == 0) {
						const uint32 startTok = mPos;
						Take();
						if (Peek().kind != NkGTok::String) {
							return Fail(err, "chemin attendu apres 'include'");
						}
						NkGInclude inc;
						inc.path = Peek().text;
						Take();
						CaptureTv(inc.tv, startTok);
						mDoc->includes.PushBack(inc);
					}

					while (Peek().kind != NkGTok::End) {
						if (!ParseSection(err)) {
							return false;
						}
					}
					mDoc->tailTv.lead = Peek().lead;
					return true;
				}

			private:
				const NkVector<NkGToken> *mToks = nullptr;
				NkGDocument *mDoc = nullptr;
				const char *mSrc = nullptr;
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

				/// La trivia d'un element : les lignes qui precedaient son PREMIER
				/// jeton, le reste de la ligne apres son DERNIER. Un seul point de
				/// capture pour tous les elements -- sans ca, chaque construction
				/// aurait sa propre facon d'oublier un commentaire.
				void CaptureTv(NkGTrivia &tv, uint32 startTok) const {
					tv.lead = (*mToks)[startTok].lead;
					const uint32 last = (mPos > 0) ? mPos - 1 : 0;
					tv.trail = (*mToks)[last].trail;
				}

				bool OpenBlock(NkGBlockTrivia &blk, NkGDiag &err) {
					if (!Expect(NkGTok::LBrace, "'{'", err)) {
						return false;
					}
					blk.openTrail = (*mToks)[mPos - 1].trail;
					return true;
				}
				/// L'appelant garantit que le jeton courant est bien l'accolade
				/// fermante ; les boucles de membres testent RBrace pour sortir.
				void CloseBlock(NkGBlockTrivia &blk) {
					blk.tail.lead = Peek().lead;
					Take();
				}

				bool Fail(NkGDiag &err, const char *msg) const {
					err.code = NkString("E-PARSE");
					err.message = NkString(msg);
					// LE LEXEME FAUTIF EST DANS LE MESSAGE. « symbole inattendu ligne
					// 412 » oblige a ouvrir le fichier ; « symbole inattendu '[' ligne
					// 412 » dit tout de suite ce qui manque au format.
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

				bool FailCode(NkGDiag &err, const char *code, const NkString &msg) const {
					err.code = NkString(code);
					err.message = msg;
					const NkGToken &t = Peek();
					err.line = t.line;
					err.column = t.column;
					return false;
				}

				// -- LA VERSION, ET LES REGLES (b) (c) (d) ----------------------
				//
				//  Deux comportements, et la frontiere est la MAJEURE :
				//    majeure superieure  -> rupture annoncee : on REFUSE, en le
				//                           disant. C'est la regle (c) ;
				//    mineure superieure  -> ajout compatible : on LIT, et tout ce
				//                           qu'on ne comprend pas est garde en texte
				//                           brut. C'est la regle (d).
				//
				//  Refuser aussi la mineure serait plus simple et plus faux : ca
				//  transformerait le moindre ajout futur en rupture, et le format ne
				//  pourrait plus jamais grandir sans casser les outils deployes.
				bool CheckVersion(NkGDiag &err) {
					const int32 maj = (int32)NkGParseNumber(mDoc->versionMajor);
					const int32 min = (int32)NkGParseNumber(mDoc->versionMinor);
					if (maj > kFormatMajor) {
						NkString m("ce fichier est trop recent pour moi : nkgui ");
						m.Append(mDoc->versionMajor);
						m.Append('.');
						m.Append(mDoc->versionMinor);
						m.Append(" ; ce lecteur comprend jusqu'a ");
						m.Append(NkGU32((uint32)kFormatMajor));
						m.Append('.');
						m.Append(NkGU32((uint32)kFormatMinor));
						m.Append(" (majeure superieure = rupture annoncee, on ne devine pas)");
						return FailCode(err, "E-VERSION-INCOMPATIBLE", m);
					}
					mDoc->futureMinor = (maj == kFormatMajor && min > kFormatMinor);
					return true;
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

				// -- Les valeurs -----------------------------------------------
				uint32 AddValue(const NkGValue &v) {
					mDoc->values.PushBack(v);
					return (uint32)mDoc->values.Size() - 1;
				}

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
						case NkGTok::LBracket:
							return ParseList(v, err);
						case NkGTok::LBrace:
							return ParseDict(v, err);
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
						default:
							return Fail(err, "valeur attendue");
					}
				}

				/// LA LISTE -- decision 1 de Rodolf. Cinq roles l'exigeaient
				/// (`columns[]`, `items[]`, `tabs[]`, `values[]`, `sizes[]`) et
				/// aucun n'etait ecrivable : ni tableau, ni liste deroulante, ni
				/// barre d'onglets. Les elements sont des VALEURS, donc une liste
				/// peut contenir une liste ou un dictionnaire.
				bool ParseList(NkGValue &v, NkGDiag &err) {
					Take();	 // '['
					v.kind = NkGValueKind::List;
					if (Peek().kind == NkGTok::RBracket) {
						Take();
						return true;
					}
					while (true) {
						NkGValue item;
						if (!ParseValue(item, err)) {
							return false;
						}
						v.items.PushBack(AddValue(item));
						if (Peek().kind == NkGTok::Comma) {
							Take();
							// Pas de virgule finale : la tolerer obligerait a decider
							// si on la reemet, et une decision de mise en forme prise
							// en silence est exactement ce qui rend les diffs
							// illisibles.
							continue;
						}
						break;
					}
					return Expect(NkGTok::RBracket, "']'", err);
				}

				/// LE DICTIONNAIRE -- decision 1 de Rodolf, deuxieme moitie. La cle
				/// est un identifiant ou une chaine : une cle libre (« Mon libelle »)
				/// n'est pas toujours un identifiant valide, et l'imposer forcerait a
				/// inventer un encodage.
				bool ParseDict(NkGValue &v, NkGDiag &err) {
					Take();	 // '{'
					v.kind = NkGValueKind::Dict;
					if (Peek().kind == NkGTok::RBrace) {
						Take();
						return true;
					}
					while (true) {
						if (Peek().kind == NkGTok::Ident) {
							v.keys.PushBack(Peek().text);
							v.keyQuoted.PushBack(0);
						} else if (Peek().kind == NkGTok::String) {
							v.keys.PushBack(Peek().text);
							v.keyQuoted.PushBack(1);
						} else {
							return Fail(err, "cle attendue dans un dictionnaire "
											 "(identifiant ou chaine)");
						}
						Take();
						if (!Expect(NkGTok::Equal, "'='", err)) {
							return false;
						}
						NkGValue item;
						if (!ParseValue(item, err)) {
							return false;
						}
						v.items.PushBack(AddValue(item));
						if (Peek().kind == NkGTok::Comma) {
							Take();
							continue;
						}
						break;
					}
					return Expect(NkGTok::RBrace, "'}'", err);
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

				// -- Les expressions, par precedence grimpante ------------------
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
						|| t.kind == NkGTok::Color || t.kind == NkGTok::LBracket
						|| t.kind == NkGTok::LBrace) {
						e.kind = NkGExprKind::Literal;
						if (!ParseValue(e.literal, err)) {
							return false;
						}
						out = AddExpr(e);
						return true;
					}
					if (t.kind == NkGTok::Ident) {
						// `n1.value` et `Enum.X` arrivent ici en UN seul jeton : c'est
						// le lexeur qui a recolle le chemin. Le document 2 les emploie
						// dans ses propres exemples (§4.1, §6.3) -- decision 2.
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

				// -- Les instructions ------------------------------------------
				uint32 AddStmt(const NkGStmt &s) {
					mDoc->stmts.PushBack(s);
					return (uint32)mDoc->stmts.Size() - 1;
				}

				bool ParseStmtBlock(NkVector<uint32> &out, NkGBlockTrivia &blk, NkGDiag &err) {
					if (!OpenBlock(blk, err)) {
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
					CloseBlock(blk);
					return true;
				}

				bool ParseStmt(uint32 &out, NkGDiag &err) {
					const uint32 startTok = mPos;
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
						CaptureTv(s.tv, startTok);
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
						if (!ParseStmtBlock(s.thenStmts, s.blkThen, err)) {
							return false;
						}
						if (Peek().kind == NkGTok::Ident && Peek().text.Compare("else") == 0) {
							Take();
							s.hasElse = true;
							if (!ParseStmtBlock(s.elseStmts, s.blkElse, err)) {
								return false;
							}
						}
						CaptureTv(s.tv, startTok);
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
						CaptureTv(s.tv, startTok);
						out = AddStmt(s);
						return true;
					}
					return Fail(err, "instruction attendue ('set', 'if' ou 'Callback')");
				}

				// -- Les evenements --------------------------------------------
				bool ParseEvent(NkGEvent &ev, NkGDiag &err) {
					const uint32 startTok = mPos;
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
						if (!ParseStmtBlock(ev.stmts, ev.blk, err)) {
							return false;
						}
						CaptureTv(ev.tv, startTok);
						return true;
					}
					if (a.kind == NkGTok::Ident && a.text.Compare("Callback") == 0) {
						Take();
						ev.action = NkGActionKind::Callback;
						if (Peek().kind != NkGTok::String) {
							return Fail(err, "nom de callback attendu (chaine)");
						}
						ev.target = Peek().text;
						Take();
						if (!ParseArgList(ev.args, err)) {
							return false;
						}
						CaptureTv(ev.tv, startTok);
						return true;
					}
					if (a.kind == NkGTok::Ident && a.text.Compare("Behavior") == 0) {
						Take();
						ev.action = NkGActionKind::Behavior;
						if (Peek().kind != NkGTok::String) {
							return Fail(err, "nom de comportement attendu (chaine)");
						}
						ev.target = Peek().text;
						Take();
						CaptureTv(ev.tv, startTok);
						return true;
					}
					return Fail(err, "action attendue ('Callback', 'Behavior' ou un bloc)");
				}

				// -- L'apparence (doc 9 §3) ------------------------------------
				static bool IsEffectKind(const NkString &s) {
					return s.Compare("fill") == 0 || s.Compare("stroke") == 0
						   || s.Compare("shadow") == 0 || s.Compare("blur") == 0;
				}

				bool ParseEffect(NkGEffect &ef, NkGDiag &err) {
					const uint32 startTok = mPos;
					ef.kind = Peek().text;
					Take();
					if (Peek().kind == NkGTok::String) {
						ef.hasName = true;
						ef.name = Peek().text;
						Take();
					}
					const uint32 lineOpen = Peek().line;
					if (!OpenBlock(ef.blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "nom de propriete attendu");
						}
						const uint32 pStart = mPos;
						NkGProp p;
						p.line = Peek().line;
						p.name = Peek().text;
						Take();
						if (!Expect(NkGTok::Equal, "'='", err)) {
							return false;
						}
						if (!ParseValue(p.value, err)) {
							return false;
						}
						CaptureTv(p.tv, pStart);
						ef.props.PushBack(p);
						if (Peek().kind == NkGTok::Comma) {
							ef.commas = true;
							Take();
						}
					}
					const uint32 lineClose = Peek().line;
					CloseBlock(ef.blk);
					ef.oneLine = (lineOpen == lineClose);
					CaptureTv(ef.tv, startTok);
					return true;
				}

				bool ParseAppearance(NkGAppearance &ap, NkGDiag &err) {
					const uint32 startTok = mPos;
					Take();	 // 'appearance'
					if (Peek().kind == NkGTok::LParen) {
						Take();
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "nom d'etat attendu dans appearance(...)");
						}
						ap.hasState = true;
						ap.state = Peek().text;
						Take();
						if (!Expect(NkGTok::RParen, "')'", err)) {
							return false;
						}
					}
					if (!OpenBlock(ap.blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "propriete ou effet attendu dans 'appearance'");
						}
						if (IsEffectKind(Peek().text)
							&& (Peek(1).kind == NkGTok::LBrace || Peek(1).kind == NkGTok::String)) {
							NkGEffect ef;
							if (!ParseEffect(ef, err)) {
								return false;
							}
							NkGAppMember m;
							m.kind = NkGAppMemberKind::Effect;
							m.index = (uint32)ap.effects.Size();
							ap.effects.PushBack(ef);
							ap.members.PushBack(m);
							continue;
						}
						const uint32 pStart = mPos;
						NkGProp p;
						p.line = Peek().line;
						p.name = Peek().text;
						Take();
						if (!Expect(NkGTok::Equal, "'='", err)) {
							return false;
						}
						if (!ParseValue(p.value, err)) {
							return false;
						}
						CaptureTv(p.tv, pStart);
						NkGAppMember m;
						m.kind = NkGAppMemberKind::Prop;
						m.index = (uint32)ap.props.Size();
						ap.props.PushBack(p);
						ap.members.PushBack(m);
					}
					CloseBlock(ap.blk);
					CaptureTv(ap.tv, startTok);
					return true;
				}

				// -- LA PRESERVATION EN TEXTE BRUT -- regle (d) ------------------
				//
				//  On avance jusqu'a la premiere accolade, puis on equilibre. La
				//  tranche gardee va du PREMIER octet du mot-cle au DERNIER de
				//  l'accolade fermante : commentaires interieurs compris, puisqu'on
				//  ne repasse pas par le modele.
				//
				//  CE QUE CETTE REGLE EXIGE DU FORMAT, ET IL FAUT LE DIRE : une
				//  construction future doit etre un BLOC accolade. Une construction
				//  sans bloc ne serait pas delimitable sans connaitre sa grammaire,
				//  donc pas preservable. C'est une contrainte sur les versions a
				//  venir, pas une limite de cette implementation.
				bool ParseRawBlock(uint32 &out, NkGDiag &err) {
					const uint32 startTok = mPos;
					NkGRaw r;
					r.what = Peek().text;
					while (Peek().kind != NkGTok::LBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "construction inconnue sans bloc '{ }' : elle n'est "
											 "pas preservable, la grammaire future doit "
											 "l'encadrer");
						}
						Take();
					}
					uint32 depth = 0;
					while (true) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "bloc jamais ferme dans une construction inconnue");
						}
						if (Peek().kind == NkGTok::LBrace) {
							++depth;
						} else if (Peek().kind == NkGTok::RBrace) {
							--depth;
							if (depth == 0) {
								Take();
								break;
							}
						}
						Take();
					}
					const uint32 endTok = (mPos > 0) ? mPos - 1 : 0;
					const uint32 b = (*mToks)[startTok].begin;
					const uint32 e = (*mToks)[endTok].end;
					r.text = NkString(mSrc + b, e - b);
					CaptureTv(r.tv, startTok);
					mDoc->raws.PushBack(r);
					out = (uint32)mDoc->raws.Size() - 1;
					return true;
				}

				// -- Les noeuds de widgets --------------------------------------
				/// L'INDICE EST RESERVE AVANT L'ANALYSE DES ENFANTS, et c'est
				/// obligatoire : `mDoc->nodes` grandit pendant la recursion. Copier
				/// le noeud en fin d'analyse ecraserait ce que les enfants y ont
				/// ajoute -- et remplir un noeud par reference le ferait pointer dans
				/// un tampon qui a demenage.
				bool ParseNode(uint32 &out, NkGDiag &err) {
					const uint32 startTok = mPos;
					NkGNode node;
					node.line = Peek().line;
					node.kind = Peek().text;
					Take();
					if (Peek().kind == NkGTok::String) {
						node.hasId = true;
						node.id = Peek().text;
						Take();
					}
					NkGBlockTrivia blk;
					if (!OpenBlock(blk, err)) {
						return false;
					}
					node.blk = blk;

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
						if (Peek().text.Compare("appearance") == 0
							&& (Peek(1).kind == NkGTok::LBrace || Peek(1).kind == NkGTok::LParen)) {
							NkGAppearance ap;
							if (!ParseAppearance(ap, err)) {
								return false;
							}
							NkGMember m;
							m.kind = NkGMemberKind::Appearance;
							m.index = (uint32)mDoc->nodes[self].appearances.Size();
							mDoc->nodes[self].appearances.PushBack(ap);
							mDoc->nodes[self].members.PushBack(m);
							continue;
						}
						if (Peek(1).kind == NkGTok::Equal) {
							const uint32 pStart = mPos;
							NkGProp p;
							p.line = Peek().line;
							p.name = Peek().text;
							Take();
							Take();	 // '='
							if (!ParseValue(p.value, err)) {
								return false;
							}
							CaptureTv(p.tv, pStart);
							NkGMember m;
							m.kind = NkGMemberKind::Prop;
							m.index = (uint32)mDoc->nodes[self].props.Size();
							mDoc->nodes[self].props.PushBack(p);
							mDoc->nodes[self].members.PushBack(m);
							continue;
						}
						// Regle (d), au niveau du widget : un membre en bloc qu'on ne
						// sait pas lire, dans un fichier plus recent, est GARDE.
						if (mDoc->futureMinor && Peek(1).kind != NkGTok::String
							&& Peek(1).kind != NkGTok::LBrace) {
							uint32 rawIdx = kNoIndex;
							if (!ParseRawBlock(rawIdx, err)) {
								return false;
							}
							NkGMember m;
							m.kind = NkGMemberKind::Raw;
							m.index = (uint32)mDoc->nodes[self].raws.Size();
							mDoc->nodes[self].raws.PushBack(rawIdx);
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
					mDoc->nodes[self].blk.tail.lead = Peek().lead;
					Take();	 // '}'
					CaptureTv(mDoc->nodes[self].tv, startTok);
					out = self;
					return true;
				}

				// -- Les sections ----------------------------------------------
				bool ParseSection(NkGDiag &err) {
					// L'indice du premier jeton de la section : c'est lui qui porte
					// les lignes de commentaire et les lignes vides qui la precedent.
					mSectionStart = mPos;
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
					if (t.text.Compare("animation") == 0) {
						return ParseAnimation(err);
					}
					if (t.text.Compare("fonts") == 0) {
						return ParseFonts(err);
					}
					// REGLE (d). Dans un fichier de MINEURE plus recente, une section
					// inconnue est un AJOUT du format, pas une faute de l'auteur : on
					// la garde telle quelle. Dans un fichier de notre version ou plus
					// ancien, c'est une faute, et on le dit.
					if (mDoc->futureMinor) {
						const uint32 startTok = mPos;
						uint32 rawIdx = kNoIndex;
						if (!ParseRawBlock(rawIdx, err)) {
							return false;
						}
						NkGSection ref;
						ref.kind = NkGSectionKind::Raw;
						ref.index = rawIdx;
						ref.tv = mDoc->raws[rawIdx].tv;
						mDoc->raws[rawIdx].tv = NkGTrivia();
						(void)startTok;
						mDoc->sections.PushBack(ref);
						return true;
					}
					return Fail(err,
								"section inconnue ('geometry', 'widgets', 'behavior', "
								"'controller', 'callback', 'animation' ou 'fonts' attendus)");
				}

				bool ParsePropBlock(NkVector<NkGProp> &props, NkGBlockTrivia &blk, bool allowCommas,
									NkGDiag &err) {
					if (!OpenBlock(blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "nom de propriete attendu");
						}
						const uint32 pStart = mPos;
						NkGProp p;
						p.line = Peek().line;
						p.name = Peek().text;
						Take();
						if (!Expect(NkGTok::Equal, "'='", err)) {
							return false;
						}
						if (!ParseValue(p.value, err)) {
							return false;
						}
						CaptureTv(p.tv, pStart);
						props.PushBack(p);
						if (allowCommas && Peek().kind == NkGTok::Comma) {
							Take();
						}
					}
					CloseBlock(blk);
					return true;
				}

				bool ParseGeometry(NkGDiag &err) {
					Take();	 // 'geometry'
					NkGTopSection sec;
					if (!OpenBlock(sec.blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						const uint32 startTok = mPos;
						if (!ExpectIdentText("shape", err)) {
							return false;
						}
						NkGShape sh;
						if (Peek().kind != NkGTok::String) {
							return Fail(err, "nom de forme attendu (chaine)");
						}
						sh.name = Peek().text;
						Take();
						if (!ParsePropBlock(sh.props, sh.blk, false, err)) {
							return false;
						}
						CaptureTv(sh.tv, startTok);
						mDoc->shapes.PushBack(sh);
						sec.shapes.PushBack((uint32)mDoc->shapes.Size() - 1);
					}
					CloseBlock(sec.blk);
					mDoc->topSections.PushBack(sec);
					NkGSection ref;
					ref.kind = NkGSectionKind::Geometry;
					ref.index = (uint32)mDoc->topSections.Size() - 1;
					FinishSection(ref);
					return true;
				}

				/// La trivia d'une section se capture APRES coup, sur les jetons
				/// bornes : `mPos` est deja passe a la suite, et le premier jeton de
				/// la section est celui du mot-cle. On memorise donc l'indice de
				/// depart avant chaque section.
				void FinishSection(NkGSection &ref) {
					ref.tv.lead = (*mToks)[mSectionStart].lead;
					const uint32 last = (mPos > 0) ? mPos - 1 : 0;
					ref.tv.trail = (*mToks)[last].trail;
					mDoc->sections.PushBack(ref);
				}

				bool ParseWidgets(NkGDiag &err) {
					Take();	 // 'widgets'
					NkGTopSection sec;
					if (!OpenBlock(sec.blk, err)) {
						return false;
					}
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
					CloseBlock(sec.blk);
					mDoc->topSections.PushBack(sec);
					NkGSection ref;
					ref.kind = NkGSectionKind::Widgets;
					ref.index = (uint32)mDoc->topSections.Size() - 1;
					FinishSection(ref);
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
					if (!OpenBlock(b.blk, err)) {
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
					mDoc->behaviors[self].blk.tail.lead = Peek().lead;
					Take();
					NkGSection ref;
					ref.kind = NkGSectionKind::Behavior;
					ref.index = self;
					FinishSection(ref);
					return true;
				}

				bool ParseGraphItem(uint32 self, NkGDiag &err) {
					const uint32 startTok = mPos;
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
									p.line = Peek().line;
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
						CaptureTv(gn.tv, startTok);
						NkGMember m;
						m.kind = NkGMemberKind::Prop;  // Prop = un noeud de graphe
						m.index = (uint32)mDoc->behaviors[self].gnodes.Size();
						mDoc->behaviors[self].gnodes.PushBack(gn);
						mDoc->behaviors[self].gorder.PushBack(m);
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
						CaptureTv(w.tv, startTok);
						NkGMember m;
						m.kind = NkGMemberKind::Event;	// Event = un fil
						m.index = (uint32)mDoc->behaviors[self].wires.Size();
						mDoc->behaviors[self].wires.PushBack(w);
						mDoc->behaviors[self].gorder.PushBack(m);
						return true;
					}
					return Fail(err, "'node' ou 'wire' attendu");
				}

				// -- L'animation (doc 9 §4) -------------------------------------
				bool ParseAnimTrack(NkGAnimTrack &tr, NkGDiag &err) {
					const uint32 startTok = mPos;
					Take();	 // 'track'
					if (Peek().kind != NkGTok::String) {
						return Fail(err, "chemin de propriete attendu apres 'track' (chaine)");
					}
					tr.name = Peek().text;
					Take();
					if (!OpenBlock(tr.blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						const uint32 kStart = mPos;
						if (!ExpectIdentText("key", err)) {
							return false;
						}
						NkGAnimKey k;
						if (Peek().kind != NkGTok::Number) {
							return Fail(err, "temps attendu apres 'key' (nombre)");
						}
						k.at = Peek().raw;
						Take();
						if (!Expect(NkGTok::Arrow, "'->'", err)) {
							return false;
						}
						if (!ParseValue(k.value, err)) {
							return false;
						}
						if (Peek().kind == NkGTok::Comma) {
							Take();
							if (!ExpectIdentText("curve", err)) {
								return false;
							}
							if (!Expect(NkGTok::Equal, "'='", err)) {
								return false;
							}
							if (Peek().kind != NkGTok::Ident) {
								return Fail(err, "nom de courbe attendu");
							}
							k.hasCurve = true;
							k.curve = Peek().text;
							Take();
						}
						CaptureTv(k.tv, kStart);
						tr.keys.PushBack(k);
					}
					CloseBlock(tr.blk);
					CaptureTv(tr.tv, startTok);
					return true;
				}

				bool ParseAnimDecl(NkGAnimDecl &d, NkGDiag &err) {
					const uint32 startTok = mPos;
					const NkString fam = Peek().text;
					if (fam.Compare("transition") == 0) {
						d.family = NkGAnimFamily::Transition;
					} else if (fam.Compare("ambience") == 0) {
						d.family = NkGAnimFamily::Ambience;
					} else {
						d.family = NkGAnimFamily::Continuous;
					}
					Take();
					if (Peek().kind != NkGTok::String) {
						return Fail(err, "nom attendu apres la famille d'animation (chaine)");
					}
					d.name = Peek().text;
					Take();
					if (!OpenBlock(d.blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "propriete, 'track' ou 'map' attendu");
						}
						if (Peek().text.Compare("track") == 0 && Peek(1).kind == NkGTok::String) {
							NkGAnimTrack tr;
							if (!ParseAnimTrack(tr, err)) {
								return false;
							}
							NkGAnimMember m;
							m.kind = NkGAnimMemberKind::Track;
							m.index = (uint32)d.tracks.Size();
							d.tracks.PushBack(tr);
							d.members.PushBack(m);
							continue;
						}
						if (Peek().text.Compare("map") == 0 && Peek(1).kind == NkGTok::LBrace) {
							const uint32 mStart = mPos;
							Take();	 // 'map'
							NkGAnimMap mp;
							if (!ParsePropBlock(mp.props, mp.blk, true, err)) {
								return false;
							}
							CaptureTv(mp.tv, mStart);
							NkGAnimMember m;
							m.kind = NkGAnimMemberKind::Map;
							m.index = (uint32)d.maps.Size();
							d.maps.PushBack(mp);
							d.members.PushBack(m);
							continue;
						}
						const uint32 pStart = mPos;
						NkGProp p;
						p.line = Peek().line;
						p.name = Peek().text;
						Take();
						if (!Expect(NkGTok::Equal, "'='", err)) {
							return false;
						}
						if (!ParseValue(p.value, err)) {
							return false;
						}
						CaptureTv(p.tv, pStart);
						NkGAnimMember m;
						m.kind = NkGAnimMemberKind::Prop;
						m.index = (uint32)d.props.Size();
						d.props.PushBack(p);
						d.members.PushBack(m);
					}
					CloseBlock(d.blk);
					CaptureTv(d.tv, startTok);
					return true;
				}

				bool ParseAnimation(NkGDiag &err) {
					Take();	 // 'animation'
					NkGAnimation an;
					if (Peek().kind == NkGTok::String) {
						an.hasName = true;
						an.name = Peek().text;
						Take();
					}
					if (!OpenBlock(an.blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident
							|| (Peek().text.Compare("transition") != 0
								&& Peek().text.Compare("ambience") != 0
								&& Peek().text.Compare("continuous") != 0)) {
							return Fail(err, "'transition', 'ambience' ou 'continuous' attendu");
						}
						NkGAnimDecl d;
						if (!ParseAnimDecl(d, err)) {
							return false;
						}
						an.decls.PushBack(d);
					}
					CloseBlock(an.blk);
					mDoc->animations.PushBack(an);
					NkGSection ref;
					ref.kind = NkGSectionKind::Animation;
					ref.index = (uint32)mDoc->animations.Size() - 1;
					FinishSection(ref);
					return true;
				}

				// -- Les polices (doc 9 §5) -------------------------------------
				bool ParseFontMetrics(NkGFontBlock &fb, NkGDiag &err) {
					if (!OpenBlock(fb.blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "metrique attendue ('glyph' ou un nom)");
						}
						const uint32 mStart = mPos;
						NkGFontMetric mt;
						if (Peek().text.Compare("glyph") == 0 && Peek(1).kind == NkGTok::String) {
							Take();
							mt.isGlyph = true;
							mt.glyph = Peek().text;
							Take();
							if (!Expect(NkGTok::Arrow, "'->'", err)) {
								return false;
							}
						} else {
							mt.name = Peek().text;
							Take();
							if (!Expect(NkGTok::Equal, "'='", err)) {
								return false;
							}
						}
						if (!ParseValue(mt.value, err)) {
							return false;
						}
						CaptureTv(mt.tv, mStart);
						fb.metrics.PushBack(mt);
					}
					CloseBlock(fb.blk);
					return true;
				}

				bool ParseFont(NkGFont &f, NkGDiag &err) {
					const uint32 startTok = mPos;
					Take();	 // 'font'
					if (Peek().kind != NkGTok::String) {
						return Fail(err, "nom de police attendu (chaine)");
					}
					f.name = Peek().text;
					Take();
					if (!OpenBlock(f.blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident) {
							return Fail(err, "propriete ou bloc attendu dans 'font'");
						}
						const NkString w = Peek().text;
						const bool isBlock = Peek(1).kind == NkGTok::LBrace
											 && (w.Compare("source") == 0
												 || w.Compare("fallback") == 0
												 || w.Compare("metrics") == 0);
						if (isBlock) {
							const uint32 bStart = mPos;
							NkGFontBlock fb;
							Take();
							if (w.Compare("metrics") == 0) {
								fb.kind = NkGFontBlockKind::Metrics;
								if (!ParseFontMetrics(fb, err)) {
									return false;
								}
							} else {
								fb.kind = (w.Compare("source") == 0) ? NkGFontBlockKind::Source
																	 : NkGFontBlockKind::Fallback;
								if (!ParsePropBlock(fb.props, fb.blk, true, err)) {
									return false;
								}
							}
							CaptureTv(fb.tv, bStart);
							NkGFontMember m;
							m.kind = NkGFontMemberKind::Block;
							m.index = (uint32)f.blocks.Size();
							f.blocks.PushBack(fb);
							f.members.PushBack(m);
							continue;
						}
						const uint32 pStart = mPos;
						NkGProp p;
						p.line = Peek().line;
						p.name = Peek().text;
						Take();
						if (!Expect(NkGTok::Equal, "'='", err)) {
							return false;
						}
						if (!ParseValue(p.value, err)) {
							return false;
						}
						CaptureTv(p.tv, pStart);
						NkGFontMember m;
						m.kind = NkGFontMemberKind::Prop;
						m.index = (uint32)f.props.Size();
						f.props.PushBack(p);
						f.members.PushBack(m);
					}
					CloseBlock(f.blk);
					CaptureTv(f.tv, startTok);
					return true;
				}

				bool ParseFonts(NkGDiag &err) {
					Take();	 // 'fonts'
					NkGFonts fs;
					if (!OpenBlock(fs.blk, err)) {
						return false;
					}
					while (Peek().kind != NkGTok::RBrace) {
						if (Peek().kind == NkGTok::End) {
							return Fail(err, "'}' attendu");
						}
						if (Peek().kind != NkGTok::Ident || Peek().text.Compare("font") != 0) {
							return Fail(err, "'font' attendu dans la section 'fonts'");
						}
						NkGFont f;
						if (!ParseFont(f, err)) {
							return false;
						}
						fs.fonts.PushBack(f);
					}
					CloseBlock(fs.blk);
					mDoc->fontSections.PushBack(fs);
					NkGSection ref;
					ref.kind = NkGSectionKind::Fonts;
					ref.index = (uint32)mDoc->fontSections.Size() - 1;
					FinishSection(ref);
					return true;
				}

				// -- Les contrats ----------------------------------------------
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
					const uint32 startTok = mPos;
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
					if (!ParseType(sig.ret, sig.retEnumLabels, sig.retIsEnum, err)) {
						return false;
					}
					CaptureTv(sig.tv, startTok);
					return true;
				}

				bool ParseController(NkGDiag &err) {
					Take();	 // 'controller'
					NkGController c;
					if (Peek().kind != NkGTok::String) {
						return Fail(err, "nom de controleur attendu (chaine)");
					}
					c.name = Peek().text;
					Take();
					if (!OpenBlock(c.blk, err)) {
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
					CloseBlock(c.blk);
					mDoc->controllers.PushBack(c);
					NkGSection ref;
					ref.kind = NkGSectionKind::Controller;
					ref.index = (uint32)mDoc->controllers.Size() - 1;
					FinishSection(ref);
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
					FinishSection(ref);
					return true;
				}

			private:
				uint32 mSectionStart = 0;
		};

		// =====================================================================
		//  LE SERIALISEUR
		// =====================================================================

		/// CE SONT DES OPTIONS DE MISE EN FORME, PAS DE SEMANTIQUE. Deux fichiers
		/// qui ne different que par elles decrivent la meme interface. Elles
		/// existent parce que le document 2 n'impose pas de style d'ecriture : ses
		/// exemples indentent de quatre espaces, le convertisseur du corpus Camrail
		/// en met deux, et il faut pouvoir reecrire l'un comme l'autre sans
		/// reformater le fichier de quelqu'un d'autre.
		///
		/// CE QUI A DISPARU EN v0.3, ET C'EST UN PROGRES : les deux reglages
		/// « ligne vide apres l'en-tete » et « ligne vide entre les sections ».
		/// C'etaient des HEURISTIQUES -- on devinait l'intention de l'auteur a
		/// partir d'une seule observation. Les lignes vides sont desormais LUES et
		/// conservees comme le reste de la trivia : il n'y a plus rien a deviner.
		struct NkGWriteOptions {
				uint32 indent = 4;
				bool crlf = false;
				bool inlineEmptyBlock = true;  ///< `Button "x" { }` sur une ligne
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
					WriteLead(mDoc->headTv);
					mOut.Append("nkgui ");
					mOut.Append(mDoc->versionMajor);
					mOut.Append('.');
					mOut.Append(mDoc->versionMinor);
					EndLine(mDoc->headTv);
					for (uint32 i = 0; i < (uint32)mDoc->includes.Size(); ++i) {
						const NkGInclude &inc = mDoc->includes[i];
						WriteLead(inc.tv);
						mOut.Append("include ");
						WriteQuoted(inc.path);
						EndLine(inc.tv);
					}
					for (uint32 i = 0; i < (uint32)mDoc->sections.Size(); ++i) {
						WriteSection(mDoc->sections[i]);
					}
					// Les lignes de la fin du fichier : commentaire de pied de page,
					// ligne vide finale. Sans elles, chaque enregistrement rognerait
					// un peu plus la fin du document.
					WriteLead(mDoc->tailTv);
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

				/// Les lignes de trivia sont reemises TELLES QUELLES, sans indenter :
				/// elles portent deja la leur. Voir la note sur `NkGTrivia`.
				void WriteLead(const NkGTrivia &tv) {
					for (uint32 i = 0; i < (uint32)tv.lead.Size(); ++i) {
						mOut.Append(tv.lead[i]);
						NewLine();
					}
				}
				void EndLine(const NkGTrivia &tv) {
					if (!tv.trail.Empty()) {
						mOut.Append(tv.trail);
					}
					NewLine();
				}
				void OpenBrace(const NkGBlockTrivia &blk) {
					mOut.Append(" {");
					if (!blk.openTrail.Empty()) {
						mOut.Append(blk.openTrail);
					}
					NewLine();
				}

				/// Le seul endroit du serialiseur qui REGENERE une chaine au lieu de
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
					if (v.kind == NkGValueKind::List) {
						mOut.Append('[');
						for (uint32 i = 0; i < (uint32)v.items.Size(); ++i) {
							if (i > 0) {
								mOut.Append(", ");
							}
							WriteValue(mDoc->values[v.items[i]]);
						}
						mOut.Append(']');
						return;
					}
					if (v.kind == NkGValueKind::Dict) {
						if (v.items.Size() == 0) {
							mOut.Append("{ }");
							return;
						}
						mOut.Append("{ ");
						for (uint32 i = 0; i < (uint32)v.items.Size(); ++i) {
							if (i > 0) {
								mOut.Append(", ");
							}
							if (i < (uint32)v.keyQuoted.Size() && v.keyQuoted[i]) {
								WriteQuoted(v.keys[i]);
							} else {
								mOut.Append(v.keys[i]);
							}
							mOut.Append(" = ");
							WriteValue(mDoc->values[v.items[i]]);
						}
						mOut.Append(" }");
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
					WriteLead(s.tv);
					Indent(depth);
					if (s.kind == NkGStmtKind::Assign) {
						mOut.Append("set ");
						mOut.Append(s.name);
						mOut.Append(" = ");
						WriteExpr(s.expr);
						EndLine(s.tv);
						return;
					}
					if (s.kind == NkGStmtKind::Call) {
						mOut.Append("Callback ");
						WriteQuoted(s.name);
						WriteArgs(s.args);
						EndLine(s.tv);
						return;
					}
					mOut.Append("if ");
					WriteExpr(s.expr);
					OpenBrace(s.blkThen);
					for (uint32 i = 0; i < (uint32)s.thenStmts.Size(); ++i) {
						WriteStmt(s.thenStmts[i], depth + 1);
					}
					WriteLead(s.blkThen.tail);
					Indent(depth);
					mOut.Append('}');
					if (s.hasElse) {
						mOut.Append(" else");
						OpenBrace(s.blkElse);
						for (uint32 i = 0; i < (uint32)s.elseStmts.Size(); ++i) {
							WriteStmt(s.elseStmts[i], depth + 1);
						}
						WriteLead(s.blkElse.tail);
						Indent(depth);
						mOut.Append('}');
					}
					EndLine(s.tv);
				}

				void WriteEvent(const NkGEvent &ev, uint32 depth) {
					WriteLead(ev.tv);
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
						EndLine(ev.tv);
						return;
					}
					if (ev.action == NkGActionKind::Behavior) {
						mOut.Append("Behavior ");
						WriteQuoted(ev.target);
						EndLine(ev.tv);
						return;
					}
					mOut.Append('{');
					if (!ev.blk.openTrail.Empty()) {
						mOut.Append(ev.blk.openTrail);
					}
					NewLine();
					for (uint32 i = 0; i < (uint32)ev.stmts.Size(); ++i) {
						WriteStmt(ev.stmts[i], depth + 1);
					}
					WriteLead(ev.blk.tail);
					Indent(depth);
					mOut.Append('}');
					EndLine(ev.tv);
				}

				void WriteProp(const NkGProp &p, uint32 depth) {
					WriteLead(p.tv);
					Indent(depth);
					mOut.Append(p.name);
					mOut.Append(" = ");
					WriteValue(p.value);
					EndLine(p.tv);
				}

				void WriteEffect(const NkGEffect &ef, uint32 depth) {
					WriteLead(ef.tv);
					Indent(depth);
					mOut.Append(ef.kind);
					if (ef.hasName) {
						mOut.Append(' ');
						WriteQuoted(ef.name);
					}
					if (ef.props.Size() == 0) {
						mOut.Append(" { }");
						EndLine(ef.tv);
						return;
					}
					if (ef.oneLine) {
						// La forme LUE est rendue : le document 9 ecrit ses effets sur
						// une ligne, et un outil qui les eclaterait ferait un diff de
						// tout le fichier au premier enregistrement.
						mOut.Append(" { ");
						for (uint32 i = 0; i < (uint32)ef.props.Size(); ++i) {
							if (i > 0) {
								mOut.Append(ef.commas ? ", " : " ");
							}
							mOut.Append(ef.props[i].name);
							mOut.Append(" = ");
							WriteValue(ef.props[i].value);
						}
						mOut.Append(" }");
						EndLine(ef.tv);
						return;
					}
					OpenBrace(ef.blk);
					for (uint32 i = 0; i < (uint32)ef.props.Size(); ++i) {
						WriteProp(ef.props[i], depth + 1);
					}
					WriteLead(ef.blk.tail);
					Indent(depth);
					mOut.Append('}');
					EndLine(ef.tv);
				}

				void WriteAppearance(const NkGAppearance &ap, uint32 depth) {
					WriteLead(ap.tv);
					Indent(depth);
					mOut.Append("appearance");
					if (ap.hasState) {
						mOut.Append('(');
						mOut.Append(ap.state);
						mOut.Append(')');
					}
					if (ap.members.Size() == 0 && ap.blk.Empty() && mOpt.inlineEmptyBlock) {
						mOut.Append(" { }");
						EndLine(ap.tv);
						return;
					}
					OpenBrace(ap.blk);
					for (uint32 i = 0; i < (uint32)ap.members.Size(); ++i) {
						const NkGAppMember &m = ap.members[i];
						if (m.kind == NkGAppMemberKind::Prop) {
							WriteProp(ap.props[m.index], depth + 1);
						} else {
							WriteEffect(ap.effects[m.index], depth + 1);
						}
					}
					WriteLead(ap.blk.tail);
					Indent(depth);
					mOut.Append('}');
					EndLine(ap.tv);
				}

				/// LA REEMISSION DU TEXTE BRUT -- regle (d). On recopie la tranche
				/// source telle quelle : c'est tout l'interet. Seule l'indentation de
				/// sa PREMIERE ligne est regeneree, parce que c'est nous qui la
				/// posons ; les suivantes sont deja dans la tranche.
				void WriteRaw(uint32 rawIdx, uint32 depth, const NkGTrivia &tv) {
					const NkGRaw &r = mDoc->raws[rawIdx];
					WriteLead(tv);
					Indent(depth);
					mOut.Append(r.text);
					EndLine(tv);
				}

				void WriteNode(uint32 idx, uint32 depth) {
					const NkGNode &n = mDoc->nodes[idx];
					WriteLead(n.tv);
					Indent(depth);
					mOut.Append(n.kind);
					if (n.hasId) {
						mOut.Append(' ');
						WriteQuoted(n.id);
					}
					if (n.members.Size() == 0 && n.blk.Empty() && mOpt.inlineEmptyBlock) {
						mOut.Append(" { }");
						EndLine(n.tv);
						return;
					}
					OpenBrace(n.blk);
					for (uint32 i = 0; i < (uint32)n.members.Size(); ++i) {
						const NkGMember &m = n.members[i];
						if (m.kind == NkGMemberKind::Prop) {
							WriteProp(n.props[m.index], depth + 1);
						} else if (m.kind == NkGMemberKind::Event) {
							WriteEvent(n.events[m.index], depth + 1);
						} else if (m.kind == NkGMemberKind::Appearance) {
							WriteAppearance(n.appearances[m.index], depth + 1);
						} else if (m.kind == NkGMemberKind::Raw) {
							const uint32 ri = n.raws[m.index];
							WriteRaw(ri, depth + 1, mDoc->raws[ri].tv);
						} else {
							WriteNode(n.children[m.index], depth + 1);
						}
					}
					WriteLead(n.blk.tail);
					Indent(depth);
					mOut.Append('}');
					EndLine(n.tv);
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
					WriteLead(sig.tv);
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
					EndLine(sig.tv);
				}

				void WriteAnimTrack(const NkGAnimTrack &tr, uint32 depth) {
					WriteLead(tr.tv);
					Indent(depth);
					mOut.Append("track ");
					WriteQuoted(tr.name);
					OpenBrace(tr.blk);
					for (uint32 i = 0; i < (uint32)tr.keys.Size(); ++i) {
						const NkGAnimKey &k = tr.keys[i];
						WriteLead(k.tv);
						Indent(depth + 1);
						mOut.Append("key ");
						mOut.Append(k.at);
						mOut.Append(" -> ");
						WriteValue(k.value);
						if (k.hasCurve) {
							mOut.Append(", curve = ");
							mOut.Append(k.curve);
						}
						EndLine(k.tv);
					}
					WriteLead(tr.blk.tail);
					Indent(depth);
					mOut.Append('}');
					EndLine(tr.tv);
				}

				void WriteAnimDecl(const NkGAnimDecl &d, uint32 depth) {
					WriteLead(d.tv);
					Indent(depth);
					if (d.family == NkGAnimFamily::Transition) {
						mOut.Append("transition ");
					} else if (d.family == NkGAnimFamily::Ambience) {
						mOut.Append("ambience ");
					} else {
						mOut.Append("continuous ");
					}
					WriteQuoted(d.name);
					OpenBrace(d.blk);
					for (uint32 i = 0; i < (uint32)d.members.Size(); ++i) {
						const NkGAnimMember &m = d.members[i];
						if (m.kind == NkGAnimMemberKind::Prop) {
							WriteProp(d.props[m.index], depth + 1);
						} else if (m.kind == NkGAnimMemberKind::Track) {
							WriteAnimTrack(d.tracks[m.index], depth + 1);
						} else {
							const NkGAnimMap &mp = d.maps[m.index];
							WriteLead(mp.tv);
							Indent(depth + 1);
							mOut.Append("map");
							OpenBrace(mp.blk);
							for (uint32 p = 0; p < (uint32)mp.props.Size(); ++p) {
								WriteProp(mp.props[p], depth + 2);
							}
							WriteLead(mp.blk.tail);
							Indent(depth + 1);
							mOut.Append('}');
							EndLine(mp.tv);
						}
					}
					WriteLead(d.blk.tail);
					Indent(depth);
					mOut.Append('}');
					EndLine(d.tv);
				}

				void WriteFont(const NkGFont &f, uint32 depth) {
					WriteLead(f.tv);
					Indent(depth);
					mOut.Append("font ");
					WriteQuoted(f.name);
					OpenBrace(f.blk);
					for (uint32 i = 0; i < (uint32)f.members.Size(); ++i) {
						const NkGFontMember &m = f.members[i];
						if (m.kind == NkGFontMemberKind::Prop) {
							WriteProp(f.props[m.index], depth + 1);
							continue;
						}
						const NkGFontBlock &fb = f.blocks[m.index];
						WriteLead(fb.tv);
						Indent(depth + 1);
						if (fb.kind == NkGFontBlockKind::Source) {
							mOut.Append("source");
						} else if (fb.kind == NkGFontBlockKind::Fallback) {
							mOut.Append("fallback");
						} else {
							mOut.Append("metrics");
						}
						OpenBrace(fb.blk);
						if (fb.kind == NkGFontBlockKind::Metrics) {
							for (uint32 k = 0; k < (uint32)fb.metrics.Size(); ++k) {
								const NkGFontMetric &mt = fb.metrics[k];
								WriteLead(mt.tv);
								Indent(depth + 2);
								if (mt.isGlyph) {
									mOut.Append("glyph ");
									WriteQuoted(mt.glyph);
									mOut.Append(" -> ");
								} else {
									mOut.Append(mt.name);
									mOut.Append(" = ");
								}
								WriteValue(mt.value);
								EndLine(mt.tv);
							}
						} else {
							for (uint32 k = 0; k < (uint32)fb.props.Size(); ++k) {
								WriteProp(fb.props[k], depth + 2);
							}
						}
						WriteLead(fb.blk.tail);
						Indent(depth + 1);
						mOut.Append('}');
						EndLine(fb.tv);
					}
					WriteLead(f.blk.tail);
					Indent(depth);
					mOut.Append('}');
					EndLine(f.tv);
				}

				void WriteSection(const NkGSection &ref) {
					// LE CALLBACK DE PREMIER NIVEAU EST LE SEUL CAS OU LA SECTION ET
					// SON CONTENU PARTAGENT LE MEME PREMIER JETON. Ecrire la trivia
					// de la section PUIS celle de la signature la doublerait -- et un
					// commentaire qui se duplique a chaque enregistrement finit par
					// remplir le fichier.
					if (ref.kind == NkGSectionKind::Callback) {
						WriteCallbackSig(mDoc->callbacks[ref.index], 0);
						return;
					}
					WriteLead(ref.tv);
					if (ref.kind == NkGSectionKind::Raw) {
						mOut.Append(mDoc->raws[ref.index].text);
						EndLine(ref.tv);
						return;
					}
					if (ref.kind == NkGSectionKind::Widgets) {
						const NkGTopSection &sec = mDoc->topSections[ref.index];
						mOut.Append("widgets");
						OpenBrace(sec.blk);
						for (uint32 i = 0; i < (uint32)sec.roots.Size(); ++i) {
							WriteNode(sec.roots[i], 1);
						}
						WriteLead(sec.blk.tail);
						mOut.Append('}');
						EndLine(ref.tv);
						return;
					}
					if (ref.kind == NkGSectionKind::Geometry) {
						const NkGTopSection &sec = mDoc->topSections[ref.index];
						mOut.Append("geometry");
						OpenBrace(sec.blk);
						for (uint32 i = 0; i < (uint32)sec.shapes.Size(); ++i) {
							const NkGShape &sh = mDoc->shapes[sec.shapes[i]];
							WriteLead(sh.tv);
							Indent(1);
							mOut.Append("shape ");
							WriteQuoted(sh.name);
							if (sh.props.Size() == 0 && sh.blk.Empty() && mOpt.inlineEmptyBlock) {
								mOut.Append(" { }");
								EndLine(sh.tv);
								continue;
							}
							OpenBrace(sh.blk);
							for (uint32 p = 0; p < (uint32)sh.props.Size(); ++p) {
								WriteProp(sh.props[p], 2);
							}
							WriteLead(sh.blk.tail);
							Indent(1);
							mOut.Append('}');
							EndLine(sh.tv);
						}
						WriteLead(sec.blk.tail);
						mOut.Append('}');
						EndLine(ref.tv);
						return;
					}
					if (ref.kind == NkGSectionKind::Behavior) {
						const NkGBehavior &b = mDoc->behaviors[ref.index];
						mOut.Append("behavior ");
						WriteQuoted(b.name);
						if (b.isGraph) {
							mOut.Append(" graph");
						}
						OpenBrace(b.blk);
						if (b.isGraph) {
							for (uint32 i = 0; i < (uint32)b.gorder.Size(); ++i) {
								const NkGMember &m = b.gorder[i];
								if (m.kind == NkGMemberKind::Prop) {
									const NkGGraphNode &gn = b.gnodes[m.index];
									WriteLead(gn.tv);
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
									EndLine(gn.tv);
									continue;
								}
								const NkGWire &w = b.wires[m.index];
								WriteLead(w.tv);
								Indent(1);
								mOut.Append("wire ");
								for (uint32 r = 0; r < (uint32)w.refs.Size(); ++r) {
									if (r > 0) {
										mOut.Append(" -> ");
									}
									mOut.Append(w.refs[r]);
								}
								EndLine(w.tv);
							}
						} else {
							for (uint32 i = 0; i < (uint32)b.stmts.Size(); ++i) {
								WriteStmt(b.stmts[i], 1);
							}
						}
						WriteLead(b.blk.tail);
						mOut.Append('}');
						EndLine(ref.tv);
						return;
					}
					if (ref.kind == NkGSectionKind::Animation) {
						const NkGAnimation &an = mDoc->animations[ref.index];
						mOut.Append("animation");
						if (an.hasName) {
							mOut.Append(' ');
							WriteQuoted(an.name);
						}
						OpenBrace(an.blk);
						for (uint32 i = 0; i < (uint32)an.decls.Size(); ++i) {
							WriteAnimDecl(an.decls[i], 1);
						}
						WriteLead(an.blk.tail);
						mOut.Append('}');
						EndLine(ref.tv);
						return;
					}
					if (ref.kind == NkGSectionKind::Fonts) {
						const NkGFonts &fs = mDoc->fontSections[ref.index];
						mOut.Append("fonts");
						OpenBrace(fs.blk);
						for (uint32 i = 0; i < (uint32)fs.fonts.Size(); ++i) {
							WriteFont(fs.fonts[i], 1);
						}
						WriteLead(fs.blk.tail);
						mOut.Append('}');
						EndLine(ref.tv);
						return;
					}
					// Controller
					const NkGController &c = mDoc->controllers[ref.index];
					mOut.Append("controller ");
					WriteQuoted(c.name);
					OpenBrace(c.blk);
					for (uint32 i = 0; i < (uint32)c.callbacks.Size(); ++i) {
						WriteCallbackSig(mDoc->callbacks[c.callbacks[i]], 1);
					}
					WriteLead(c.blk.tail);
					mOut.Append('}');
					EndLine(ref.tv);
				}
		};

		// =====================================================================
		//  L'API PUBLIQUE
		// =====================================================================

		/// Analyse `text` dans `doc`. En cas d'echec, `err` porte le code, le
		/// message, la ligne et la colonne -- et `doc` est laisse VIDE.
		///
		/// VIDE, PAS A MOITIE REMPLI. Un document partiel apres une erreur est le
		/// pire des etats : l'appelant qui oublie de tester le retour affiche une
		/// interface amputee au lieu de dire que le fichier est casse.
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
			NkGAttachTrivia(text, length, lex.FirstOffset(), toks);
			NkGParser parser(toks, doc, text);
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

		// =====================================================================
		//  LA COMPARAISON -- c'est elle qui rend l'aller-retour verifiable
		// =====================================================================

		/// CETTE COMPARAISON EST LE CRITERE D'ACCEPTATION, elle merite donc d'etre
		/// lue avec autant d'attention que le parseur. Elle compare la SEMANTIQUE
		/// (roles, identifiants, valeurs, ordre des membres) ET LA TRIVIA, **pas**
		/// la mise en forme. Deux documents egaux au sens de cette fonction
		/// decrivent la meme interface et portent les memes commentaires, meme si
		/// l'un indente de deux espaces et l'autre de quatre.
		///
		/// LA TRIVIA EST DANS LA COMPARAISON DEPUIS LA v0.3, et c'est le seul moyen
		/// de rendre la decision 3 de Rodolf verifiable : sans elle, un ecrivain
		/// qui jetterait tous les commentaires resterait « equivalent ».
		bool NkGEqualNode(const NkGDocument &a, uint32 ia, const NkGDocument &b, uint32 ib);
		bool NkGEqualValue(const NkGDocument &a, const NkGValue &x, const NkGDocument &b,
						   const NkGValue &y);

		inline bool NkGEqualValue(const NkGDocument &a, const NkGValue &x, const NkGDocument &b,
								  const NkGValue &y) {
			if (x.kind != y.kind) {
				return false;
			}
			if (x.kind == NkGValueKind::String) {
				return x.text.Compare(y.text) == 0;
			}
			if (x.kind == NkGValueKind::List || x.kind == NkGValueKind::Dict) {
				if (x.items.Size() != y.items.Size() || x.keys.Size() != y.keys.Size()) {
					return false;
				}
				for (uint32 i = 0; i < (uint32)x.keys.Size(); ++i) {
					if (x.keys[i].Compare(y.keys[i]) != 0) {
						return false;
					}
					if (x.keyQuoted[i] != y.keyQuoted[i]) {
						return false;
					}
				}
				for (uint32 i = 0; i < (uint32)x.items.Size(); ++i) {
					if (!NkGEqualValue(a, a.values[x.items[i]], b, b.values[y.items[i]])) {
						return false;
					}
				}
				return true;
			}
			// Pour tout le reste on compare le LEXEME : c'est ce que le serialiseur
			// reemet, donc c'est ce qui doit survivre. Comparer la valeur decodee
			// laisserait passer un `0.20` devenu `0.2`.
			return x.raw.Compare(y.raw) == 0;
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
					return NkGEqualValue(a, x.literal, b, y.literal);
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

		inline bool NkGEqualPropList(const NkGDocument &a, const NkVector<NkGProp> &xa,
									 const NkGDocument &b, const NkVector<NkGProp> &xb) {
			if (xa.Size() != xb.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)xa.Size(); ++i) {
				if (xa[i].name.Compare(xb[i].name) != 0) {
					return false;
				}
				if (!NkGEqualValue(a, xa[i].value, b, xb[i].value)) {
					return false;
				}
				if (!NkGEqualTrivia(xa[i].tv, xb[i].tv)) {
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
			if (!NkGEqualTrivia(x.tv, y.tv) || !NkGEqualBlockTrivia(x.blkThen, y.blkThen)
				|| !NkGEqualBlockTrivia(x.blkElse, y.blkElse)) {
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
			if (!NkGEqualTrivia(x.tv, y.tv) || !NkGEqualBlockTrivia(x.blk, y.blk)) {
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

		inline bool NkGEqualEffect(const NkGDocument &a, const NkGEffect &x, const NkGDocument &b,
								   const NkGEffect &y) {
			if (x.kind.Compare(y.kind) != 0 || x.hasName != y.hasName
				|| x.name.Compare(y.name) != 0 || x.oneLine != y.oneLine || x.commas != y.commas) {
				return false;
			}
			if (!NkGEqualTrivia(x.tv, y.tv) || !NkGEqualBlockTrivia(x.blk, y.blk)) {
				return false;
			}
			return NkGEqualPropList(a, x.props, b, y.props);
		}

		inline bool NkGEqualAppearance(const NkGDocument &a, const NkGAppearance &x,
									   const NkGDocument &b, const NkGAppearance &y) {
			if (x.hasState != y.hasState || x.state.Compare(y.state) != 0) {
				return false;
			}
			if (!NkGEqualTrivia(x.tv, y.tv) || !NkGEqualBlockTrivia(x.blk, y.blk)) {
				return false;
			}
			if (x.members.Size() != y.members.Size() || x.effects.Size() != y.effects.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)x.members.Size(); ++i) {
				if (x.members[i].kind != y.members[i].kind
					|| x.members[i].index != y.members[i].index) {
					return false;
				}
			}
			for (uint32 i = 0; i < (uint32)x.effects.Size(); ++i) {
				if (!NkGEqualEffect(a, x.effects[i], b, y.effects[i])) {
					return false;
				}
			}
			return NkGEqualPropList(a, x.props, b, y.props);
		}

		inline bool NkGEqualRaw(const NkGRaw &x, const NkGRaw &y) {
			return x.text.Compare(y.text) == 0 && NkGEqualTrivia(x.tv, y.tv);
		}

		inline bool NkGEqualNode(const NkGDocument &a, uint32 ia, const NkGDocument &b, uint32 ib) {
			const NkGNode &x = a.nodes[ia];
			const NkGNode &y = b.nodes[ib];
			if (x.kind.Compare(y.kind) != 0 || x.hasId != y.hasId || x.id.Compare(y.id) != 0) {
				return false;
			}
			if (!NkGEqualTrivia(x.tv, y.tv) || !NkGEqualBlockTrivia(x.blk, y.blk)) {
				return false;
			}
			if (x.members.Size() != y.members.Size()) {
				return false;
			}
			if (!NkGEqualPropList(a, x.props, b, y.props)) {
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
			if (x.appearances.Size() != y.appearances.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)x.appearances.Size(); ++i) {
				if (!NkGEqualAppearance(a, x.appearances[i], b, y.appearances[i])) {
					return false;
				}
			}
			if (x.raws.Size() != y.raws.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)x.raws.Size(); ++i) {
				if (!NkGEqualRaw(a.raws[x.raws[i]], b.raws[y.raws[i]])) {
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
			if (!NkGEqualTrivia(x.tv, y.tv)) {
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

		inline bool NkGEqualAnimation(const NkGDocument &a, const NkGAnimation &x,
									  const NkGDocument &b, const NkGAnimation &y) {
			if (x.hasName != y.hasName || x.name.Compare(y.name) != 0
				|| x.decls.Size() != y.decls.Size() || !NkGEqualBlockTrivia(x.blk, y.blk)) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)x.decls.Size(); ++i) {
				const NkGAnimDecl &dx = x.decls[i];
				const NkGAnimDecl &dy = y.decls[i];
				if (dx.family != dy.family || dx.name.Compare(dy.name) != 0
					|| dx.members.Size() != dy.members.Size()
					|| dx.tracks.Size() != dy.tracks.Size() || dx.maps.Size() != dy.maps.Size()) {
					return false;
				}
				if (!NkGEqualTrivia(dx.tv, dy.tv) || !NkGEqualBlockTrivia(dx.blk, dy.blk)) {
					return false;
				}
				for (uint32 m = 0; m < (uint32)dx.members.Size(); ++m) {
					if (dx.members[m].kind != dy.members[m].kind
						|| dx.members[m].index != dy.members[m].index) {
						return false;
					}
				}
				if (!NkGEqualPropList(a, dx.props, b, dy.props)) {
					return false;
				}
				for (uint32 t = 0; t < (uint32)dx.tracks.Size(); ++t) {
					const NkGAnimTrack &tx = dx.tracks[t];
					const NkGAnimTrack &ty = dy.tracks[t];
					if (tx.name.Compare(ty.name) != 0 || tx.keys.Size() != ty.keys.Size()) {
						return false;
					}
					if (!NkGEqualTrivia(tx.tv, ty.tv) || !NkGEqualBlockTrivia(tx.blk, ty.blk)) {
						return false;
					}
					for (uint32 k = 0; k < (uint32)tx.keys.Size(); ++k) {
						if (tx.keys[k].at.Compare(ty.keys[k].at) != 0
							|| tx.keys[k].hasCurve != ty.keys[k].hasCurve
							|| tx.keys[k].curve.Compare(ty.keys[k].curve) != 0) {
							return false;
						}
						if (!NkGEqualValue(a, tx.keys[k].value, b, ty.keys[k].value)) {
							return false;
						}
						if (!NkGEqualTrivia(tx.keys[k].tv, ty.keys[k].tv)) {
							return false;
						}
					}
				}
				for (uint32 mp = 0; mp < (uint32)dx.maps.Size(); ++mp) {
					if (!NkGEqualPropList(a, dx.maps[mp].props, b, dy.maps[mp].props)) {
						return false;
					}
					if (!NkGEqualTrivia(dx.maps[mp].tv, dy.maps[mp].tv)
						|| !NkGEqualBlockTrivia(dx.maps[mp].blk, dy.maps[mp].blk)) {
						return false;
					}
				}
			}
			return true;
		}

		inline bool NkGEqualFonts(const NkGDocument &a, const NkGFonts &x, const NkGDocument &b,
								  const NkGFonts &y) {
			if (x.fonts.Size() != y.fonts.Size() || !NkGEqualBlockTrivia(x.blk, y.blk)) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)x.fonts.Size(); ++i) {
				const NkGFont &fx = x.fonts[i];
				const NkGFont &fy = y.fonts[i];
				if (fx.name.Compare(fy.name) != 0 || fx.members.Size() != fy.members.Size()
					|| fx.blocks.Size() != fy.blocks.Size()) {
					return false;
				}
				if (!NkGEqualTrivia(fx.tv, fy.tv) || !NkGEqualBlockTrivia(fx.blk, fy.blk)) {
					return false;
				}
				for (uint32 m = 0; m < (uint32)fx.members.Size(); ++m) {
					if (fx.members[m].kind != fy.members[m].kind
						|| fx.members[m].index != fy.members[m].index) {
						return false;
					}
				}
				if (!NkGEqualPropList(a, fx.props, b, fy.props)) {
					return false;
				}
				for (uint32 k = 0; k < (uint32)fx.blocks.Size(); ++k) {
					const NkGFontBlock &bx = fx.blocks[k];
					const NkGFontBlock &by = fy.blocks[k];
					if (bx.kind != by.kind || bx.metrics.Size() != by.metrics.Size()) {
						return false;
					}
					if (!NkGEqualTrivia(bx.tv, by.tv) || !NkGEqualBlockTrivia(bx.blk, by.blk)) {
						return false;
					}
					if (!NkGEqualPropList(a, bx.props, b, by.props)) {
						return false;
					}
					for (uint32 t = 0; t < (uint32)bx.metrics.Size(); ++t) {
						if (bx.metrics[t].isGlyph != by.metrics[t].isGlyph
							|| bx.metrics[t].glyph.Compare(by.metrics[t].glyph) != 0
							|| bx.metrics[t].name.Compare(by.metrics[t].name) != 0) {
							return false;
						}
						if (!NkGEqualValue(a, bx.metrics[t].value, b, by.metrics[t].value)) {
							return false;
						}
						if (!NkGEqualTrivia(bx.metrics[t].tv, by.metrics[t].tv)) {
							return false;
						}
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
			if (!NkGEqualTrivia(a.headTv, b.headTv) || !NkGEqualTrivia(a.tailTv, b.tailTv)) {
				return false;
			}
			if (a.includes.Size() != b.includes.Size()) {
				return false;
			}
			for (uint32 i = 0; i < (uint32)a.includes.Size(); ++i) {
				if (a.includes[i].path.Compare(b.includes[i].path) != 0
					|| !NkGEqualTrivia(a.includes[i].tv, b.includes[i].tv)) {
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
				if (!NkGEqualTrivia(ra.tv, rb.tv)) {
					return false;
				}
				if (ra.kind == NkGSectionKind::Widgets) {
					const NkGTopSection &xa = a.topSections[ra.index];
					const NkGTopSection &xb = b.topSections[rb.index];
					if (xa.roots.Size() != xb.roots.Size()
						|| !NkGEqualBlockTrivia(xa.blk, xb.blk)) {
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
					if (xa.shapes.Size() != xb.shapes.Size()
						|| !NkGEqualBlockTrivia(xa.blk, xb.blk)) {
						return false;
					}
					for (uint32 i = 0; i < (uint32)xa.shapes.Size(); ++i) {
						const NkGShape &sa = a.shapes[xa.shapes[i]];
						const NkGShape &sb = b.shapes[xb.shapes[i]];
						if (sa.name.Compare(sb.name) != 0
							|| !NkGEqualPropList(a, sa.props, b, sb.props)
							|| !NkGEqualTrivia(sa.tv, sb.tv)
							|| !NkGEqualBlockTrivia(sa.blk, sb.blk)) {
							return false;
						}
					}
					continue;
				}
				if (ra.kind == NkGSectionKind::Behavior) {
					const NkGBehavior &ba = a.behaviors[ra.index];
					const NkGBehavior &bb = b.behaviors[rb.index];
					if (ba.name.Compare(bb.name) != 0 || ba.isGraph != bb.isGraph
						|| !NkGEqualBlockTrivia(ba.blk, bb.blk)) {
						return false;
					}
					if (!NkGEqualStmtList(a, ba.stmts, b, bb.stmts)) {
						return false;
					}
					if (ba.gnodes.Size() != bb.gnodes.Size() || ba.wires.Size() != bb.wires.Size()
						|| ba.gorder.Size() != bb.gorder.Size()) {
						return false;
					}
					for (uint32 i = 0; i < (uint32)ba.gorder.Size(); ++i) {
						if (ba.gorder[i].kind != bb.gorder[i].kind
							|| ba.gorder[i].index != bb.gorder[i].index) {
							return false;
						}
					}
					for (uint32 i = 0; i < (uint32)ba.gnodes.Size(); ++i) {
						if (ba.gnodes[i].name.Compare(bb.gnodes[i].name) != 0
							|| ba.gnodes[i].type.Compare(bb.gnodes[i].type) != 0
							|| ba.gnodes[i].hasPins != bb.gnodes[i].hasPins
							|| !NkGEqualTrivia(ba.gnodes[i].tv, bb.gnodes[i].tv)) {
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
						if (ba.wires[i].refs.Size() != bb.wires[i].refs.Size()
							|| !NkGEqualTrivia(ba.wires[i].tv, bb.wires[i].tv)) {
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
						|| ca.callbacks.Size() != cb.callbacks.Size()
						|| !NkGEqualBlockTrivia(ca.blk, cb.blk)) {
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
				if (ra.kind == NkGSectionKind::Animation) {
					if (!NkGEqualAnimation(a, a.animations[ra.index], b,
										   b.animations[rb.index])) {
						return false;
					}
					continue;
				}
				if (ra.kind == NkGSectionKind::Fonts) {
					if (!NkGEqualFonts(a, a.fontSections[ra.index], b,
									   b.fontSections[rb.index])) {
						return false;
					}
					continue;
				}
				if (ra.kind == NkGSectionKind::Raw) {
					// LE TEMOIN DE LA REGLE (d) : la tranche brute doit revenir a
					// l'IDENTIQUE. Si elle bouge d'un octet, une version future du
					// format perd du contenu en passant par un outil ancien.
					if (a.raws[ra.index].text.Compare(b.raws[rb.index].text) != 0) {
						return false;
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
