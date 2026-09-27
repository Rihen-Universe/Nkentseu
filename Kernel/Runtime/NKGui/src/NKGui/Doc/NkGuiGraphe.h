#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiGraphe.h
// @Brief   Le Blueprint NODAL : `behavior "x" graph { node … wire … }`.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  IL NE S'INTERPRÈTE PAS, IL SE COMPILE — ET C'EST LA DÉCISION DE RODOLF
// =============================================================================
//  Document 2 §6.2, décidé le 27/09 : « le graphe est COMPILÉ vers la
//  représentation intermédiaire du script et exécuté par LE MÊME évaluateur,
//  avec les mêmes garanties. *Un Blueprint tourne dans l'application exactement
//  comme le script équivalent* — pas d'interpréteur de graphe séparé, donc pas
//  de divergence possible. »
//
//  ⚠️ CETTE PHRASE EST LA RAISON D'ÊTRE DE CE FICHIER. Un second interpréteur
//     aurait eu ses propres priorités d'opérateurs, sa propre idée de « vide »,
//     sa propre façon de comparer deux jetons — et le jour où les deux auraient
//     divergé, personne n'aurait su lequel avait raison. Ici, le graphe produit
//     du TEXTE de script, que l'évaluateur existant exécute. Il n'y a qu'un
//     moteur, donc qu'une vérité.
//
// =============================================================================
//  POURQUOI CE FICHIER LIT UNE TRANCHE BRUTE
// =============================================================================
//  🔴 `behavior "x" graph { … }` N'ARRIVE JAMAIS COMME UN BLOC. Le mot `graph`
//     s'intercale entre l'identifiant et l'accolade ; le lecteur d'archive,
//     purement syntaxique, ne reconnaît pas cette forme et la garde en SOURCE
//     VERBATIM (règle T11). C'est la même situation que `include "x"` et que
//     `dock "scene" left 0.16`, et c'est la même lecture.
//
//     `NkGuiExecution::ExecuterSection` portait déjà une garde pour ce cas —
//     « un graphe évalué à moitié est pire que pas évalué du tout » — et
//     comptait `grapheIgnore`. Ce fichier la remplace par une compilation.
//
// =============================================================================
//  CE QUI EST COMPILÉ, ET CE QUI NE L'EST PAS
// =============================================================================
//  ⚠️ UN GRAPHE QUI NE COMPILE PAS N'EST PAS EXÉCUTÉ, ET IL LE DIT (§6.2). Le
//     refus porte sur le graphe ENTIER, pas sur le nœud fautif : exécuter la
//     moitié d'un comportement donnerait un état que personne n'a décrit.
//     Chaque refus nomme son nœud et sa cause (`E-GRAPHE`).
//
//  Le catalogue du §6.3 est vaste ; ce qui est compilé ici est le sous-ensemble
//  qui a un ÉQUIVALENT EXACT dans le script — c'est la condition posée par
//  §6.6. Le reste est refusé avec son nom, jamais approximé.
// =============================================================================
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nkgui {

		/// Ce qu'une compilation de graphe a produit, et ce qu'elle a refusé.
		struct NkGuiRapportGraphe {
				uint32 graphes = 0;	  ///< sections `behavior … graph` rencontrées
				uint32 noeuds = 0;	  ///< `node` lus
				uint32 fils = 0;	  ///< liaisons `->` lues
				uint32 compiles = 0;  ///< graphes traduits en script
				uint32 refuses = 0;	  ///< graphes NON exécutés
				/// Chaque refus, avec son nœud et sa cause. ⚠️ Un compte sans nom
				/// envoie chercher dans tout le fichier : c'est la leçon que ce
				/// dépôt a payée sur les débordements de région.
				NkVector<NkString> causes;

				bool Propre() const noexcept {
					return refuses == 0u;
				}
		};

		/// Compile une section `behavior "nom" graph { … }` en TEXTE DE SCRIPT.
		///
		/// `slice` est la tranche verbatim entière, `behavior` compris. Rend faux
		/// si ce n'est pas un graphe, ou s'il ne compile pas — et dans le second
		/// cas `rap.causes` dit pourquoi.
		///
		/// ⚠️ LE NOM DU COMPORTEMENT SORT AUSSI, parce que l'appelant en a besoin
		///    pour savoir QUEL événement déclenche ce graphe. Le relire ailleurs
		///    ferait deux analyses de la même ligne.
		bool NkGuiCompilerGraphe(NkStringView slice, NkString &nomOut, NkString &evenementOut,
								 NkString &scriptOut, NkGuiRapportGraphe &rap) noexcept;

	} // namespace nkgui
} // namespace nkentseu

#include "NKGui/Doc/NkGuiGraphe.inl"
