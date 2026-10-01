//
// NkBpExpression.h
// =============================================================================
// Description :
//   LE LANGAGE DES NOEUDS DE CODE d'un Blueprint (demande de Rihen, 01/10 : la
//   reference principale montre « Evaluate » avec `$round((testsTotal -
//   testsFailed) / testsTotal * 100, 1)` et « If » avec `runtime > -1` ; la
//   charte ne les dessine pas, § 13.1). Trois noeuds s'en servent :
//     - « Expression » (pur, filet petrole) : une expression -> « résultat » ;
//     - « Si (expression) » (instruction) : une condition -> Vrai / Faux ;
//     - « Code » (instruction) : des INSTRUCTIONS, une par ligne ou separees
//       par « ; » : `ouverte = vrai`, `total = total + a`, `afficher("...")`,
//       `OuvrirPorte(hauteur)`.
//
// Le langage, petit et sur (il compile vers la MEME machine que les noeuds) :
//   nombres (2, 1.5), textes ("..."), vrai / faux, soi ; + - * / ; < <= > >=
//   == != ; && || ! (et / ou / non) ; parentheses ; v.x / v.y ; appels de
//   fonction : celles du langage (vec2, abs, min, max, plancher, arrondi,
//   racine, sin, cos, puissance, borner, interpoler, longueur, normaliser,
//   entier, reel), les NATIFS par leur nom court (position, teleporter,
//   afficher, par_nom...), les FONCTIONS du Blueprint ; le « $ » devant un
//   nom de fonction (la reference) est accepte. Les noms : les ENTREES du
//   noeud, puis les VARIABLES du Blueprint.
//
//   Ce fichier : le LEXEUR (il sert aussi a COLORER le code dans le noeud) et
//   l'ANALYSEUR. La generation de code est dans NkBpCompilateur.cpp, qui
//   connait les types, les entrees et les variables.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKBPEXPRESSION_H__
#define __NKENTSEU_UNKENYEDITOR_NKBPEXPRESSION_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editeur {

		/// Les cles des trois noeuds de code.
		constexpr const char *NK_BP_EXPRESSION = "bp.expr";
		constexpr const char *NK_BP_SI_EXPRESSION = "bp.si.expr";
		constexpr const char *NK_BP_CODE = "bp.code";

		enum class NkGenreJetonExpr : uint8 { NK_NOMBRE = 0, NK_IDENT, NK_TEXTE, NK_OP, NK_FIN, NK_ERREUR };

		struct NkJetonExpr {
				NkGenreJetonExpr genre = NkGenreJetonExpr::NK_FIN;
				NkString texte;
				uint32 debut = 0u, fin = 0u; ///< octets dans le code
		};

		/// Decoupe `code` en jetons (le dernier est NK_FIN). Un caractere inconnu
		/// donne un jeton NK_ERREUR (le reste continue : la coloration en a besoin).
		void NkBpLexerExpr(const char *code, NkVector<NkJetonExpr> &sortie);

		enum class NkGenreNoeudExpr : uint8 {
			NK_NOMBRE = 0,
			NK_BOOLEEN,
			NK_TEXTE,
			NK_SOI,
			NK_NOM,		///< une entree du noeud, une variable
			NK_APPEL,	///< nom(args...)
			NK_UNAIRE,	///< op a
			NK_BINAIRE, ///< a op b
			NK_MEMBRE	///< a.x, a.y
		};

		struct NkNoeudExpr {
				NkGenreNoeudExpr genre = NkGenreNoeudExpr::NK_NOMBRE;
				NkString nom; ///< nom, operateur (« + », « && »), membre
				float64 nombre = 0.0;
				bool entier = false; ///< un nombre ecrit sans point
				int32 a = -1, b = -1;
				NkVector<int32> args;
				uint32 colonne = 0u; ///< 1 = le premier caractere
				uint32 ligne = 1u;
		};

		/// Une instruction : `cible = valeur`, ou une valeur seule (un appel).
		struct NkInstructionExpr {
				NkString cible; ///< vide : pas d'affectation
				uint32 colonneCible = 0u;
				int32 valeur = -1;
		};

		struct NkArbreExpr {
				NkVector<NkNoeudExpr> noeuds;
				NkVector<NkInstructionExpr> instructions;
				NkString erreur;		///< vide : l'analyse a reussi
				uint32 colonne = 0u;	///< ou (1 = le premier caractere)
				uint32 ligne = 0u;		///< (1 = la premiere)
		};

		/// Analyse une EXPRESSION seule (`instructions` faux) ou une suite
		/// d'INSTRUCTIONS (lignes ou « ; »). false : `a.erreur` dit quoi et ou.
		bool NkBpAnalyserExpr(const char *code, bool instructions, NkArbreExpr &a);

		/// Le code d'un noeud est garde dans sa propriete « code » sur UNE ligne
		/// (le .nkgraph lit le reste d'une ligne) : les sauts y sont « \n ».
		NkString NkBpCodeVersPropriete(const char *code);
		NkString NkBpCodeDePropriete(const char *propriete);

		/// Les « Snippets » proposes dans le noeud.
		const char *const *NkBpSnippets(uint32 &nombre, bool instructions);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKBPEXPRESSION_H__
