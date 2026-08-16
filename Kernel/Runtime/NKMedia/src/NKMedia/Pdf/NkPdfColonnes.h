//
// NkPdfColonnes.h — DETECTION GEOMETRIQUE des colonnes, sans arbre de structure.
//
// POURQUOI CE FICHIER EXISTE. L'ordre de lecture d'un document a deux colonnes
// est le pire defaut d'une extraction de texte : l'assemblage visuel ENTRELACE
// les colonnes, et le resultat reste grammaticalement plausible. Pour un
// systeme qui cite ses sources, une citation credible et inexacte est le mode
// d'echec le plus dangereux.
//
// L'arbre /StructTreeRoot resout le probleme quand il existe — mais il n'existe
// que dans 55 % du corpus, et rien ne dit que les documents a colonnes en font
// partie. La geometrie, elle, est TOUJOURS disponible : `TextItems` porte les
// positions.
//
// PRINCIPE. Une page a deux colonnes se reconnait a sa GOUTTIERE : une bande
// verticale d'abscisses ou la couverture de texte est quasi nulle, sur la
// majeure partie de la hauteur. Un titre ou une figure pleine largeur la
// traverse legitimement — c'est pourquoi on exige « la majorite de la hauteur »
// et non « toute la hauteur ».
//
// DOUBLE USAGE, ET C'EST VOULU : ce detecteur sert d'abord a MESURER combien de
// documents du fonds sont reellement a colonnes (et si la phase 3 les touche),
// puis a les CORRIGER. Un instrument de mesure qui se transforme en correctif
// coute une fois ce qu'un simple sondage aurait coute pour rien.
//
#pragma once

#include "NKMedia/Pdf/NkPdf.h"
#include "NKMedia/Pdf/NkPdfRender.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			// Ce qu'une page revele de sa mise en page.
			struct NkPdfPageColonnes {
					bool multiColonnes = false;
					// Abscisse du milieu de la gouttiere, en pixels du canevas.
					// Sert ensuite a SEPARER le texte en colonnes.
					float32 gouttiereX = 0.f;
					float32 gouttiereLargeur = 0.f;
			};

			// Verdict pour un document entier.
			struct NkPdfDocColonnes {
					int32 pagesAnalysees = 0;	// pages portant assez de texte pour juger
					int32 pagesMultiColonnes = 0;
					bool multiColonnes = false; // verdict global (voir le seuil)
					float32 hauteurMediane = 0.f; // hauteur de caractere mediane, unite de reference
			};

			// Analyse UNE page. `items` vient de `NkPdfRenderer::TextItems()`.
			//
			// Les seuils sont exprimes en multiples de la HAUTEUR DE CARACTERE
			// mediane, jamais en points absolus : un document A4 a 72 ppp et le
			// meme a 300 ppp doivent donner le meme verdict. Une mesure absolue
			// ferait dependre le resultat de la resolution de rendu — exactement
			// le genre de seuil qui marche sur le corpus de test et nulle part
			// ailleurs.
			NkPdfPageColonnes NkPdfAnalyserColonnesPage(const NkVector<NkPdfRenderer::TextItem> &items);

			// Part des pages multi-colonnes au-dela de laquelle le DOCUMENT est
			// declare multi-colonnes. 30 % : un article scientifique a deux
			// colonnes le reste sur presque toutes ses pages, tandis qu'un livre
			// a une colonne peut porter une ou deux pages d'index ou de tableau
			// qui ressemblent a des colonnes. Le seuil separe les deux sans
			// exiger l'unanimite, qu'une seule page de garde suffirait a briser.
			static const double kNkPdfSeuilPagesColonnes = 0.30;

			// Analyse un document entier. Rend les pages une a une.
			NkPdfDocColonnes NkPdfDetecterColonnes(NkPdfDoc &doc);

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
