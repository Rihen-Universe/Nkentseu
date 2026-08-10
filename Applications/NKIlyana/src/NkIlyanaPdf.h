// =============================================================================
// NkIlyanaPdf — extraire le texte d'un PDF, pour qu'un livre en PDF entre dans
// la bibliothèque.
// -----------------------------------------------------------------------------
// RIEN N'EST ÉCRIT ICI DE CE QUI EST DIFFICILE. Le lecteur PDF existait déjà dans
// le dépôt (tables ET flux de références croisées, objets compressés, filtres
// Flate/ASCII85/ASCIIHex/RunLength, prédicteurs, polices, tables /ToUnicode). Il
// vivait dans l'application NKCode ; il a été descendu dans NKMedia, au niveau
// Runtime, parce que NKAI est dans le noyau et qu'un module du noyau ne peut pas
// dépendre d'une application sans renverser les couches. Ce fichier ne fait donc
// que s'en servir.
//
// COMMENT ON OBTIENT DU TEXTE. Un PDF ne contient PAS de texte : il contient des
// identifiants de glyphes posés à des coordonnées. Le rendu du dépôt, en les
// dessinant, note pour chacun sa boîte et son équivalent UTF-8 tiré de la table
// /ToUnicode de sa police. On lui fait donc parcourir chaque page, et on relit
// ces notes. C'est un détour — on rastérise pour lire — mais c'est le seul chemin
// honnête, et il est payé une fois, au dépôt du livre.
//
// CE QUI SORTIRA MAL, ET POURQUOI ON NE PEUT PAS MIEUX FAIRE :
//  - une police SANS table /ToUnicode ne dit nulle part ce que son glyphe numéro
//    17 représente. Le rendu laisse alors le caractère VIDE plutôt que d'inventer.
//    Un livre entier dans ce cas ressortira quasi muet — et il vaut mieux qu'il
//    ressorte muet que faux ;
//  - les FORMULES de mathématiques et de physique ne sont pas du texte, même
//    ici : ce sont des glyphes placés un à un, sans la structure qui dit « ceci
//    est une intégrale ». Elles ressortiront en suite de symboles. Aucun outil,
//    du commerce ou non, ne fait mieux sans reconstruction sémantique ;
//  - les colonnes se mélangent si l'ordre de dessin ne suit pas l'ordre de
//    lecture. On regroupe par LIGNE, ce qui redresse le cas courant.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_ILYANA_PDF_H
#define NK_ILYANA_PDF_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKMedia/Pdf/NkPdf.h"
#include "NKMedia/Pdf/NkPdfRaster.h"
#include "NKMedia/Pdf/NkPdfRender.h"

namespace ilyana {
	using namespace nkentseu;

	// Assemble les caractères d'une page en lignes puis en paragraphes.
	//
	// Le regroupement se fait sur la coordonnée VERTICALE : deux caractères dont
	// les bases sont proches appartiennent à la même ligne, quel que soit l'ordre
	// dans lequel ils ont été dessinés. C'est ce qui redresse les PDF où le texte
	// est émis par blocs plutôt que de gauche à droite.
	inline NkString AssemblerPage(const NkVector<nkentseu::nkcode::pdf::NkPdfRenderer::TextItem> &items,
								  float32 toleranceLigne = 3.0f) {
		NkString page;
		if (items.Size() == 0)
			return page;

		// Tri par ligne (y croissant) puis par x. Tri à bulles évité : on passe
		// par un tri par insertion sur les indices, suffisant pour quelques
		// milliers de caractères par page et sans allocation supplémentaire.
		NkVector<uint32> ordre;
		ordre.Reserve(items.Size());
		for (nk_size i = 0; i < items.Size(); ++i)
			ordre.PushBack((uint32)i);
		for (nk_size i = 1; i < ordre.Size(); ++i) {
			const uint32 cle = ordre[i];
			nk_size j = i;
			while (j > 0) {
				const nk_size a = ordre[j - 1];
				const bool memeLigne = (items[a].y > items[cle].y - toleranceLigne) &&
									   (items[a].y < items[cle].y + toleranceLigne);
				const bool apres = memeLigne ? (items[a].x > items[cle].x) : (items[a].y > items[cle].y);
				if (!apres)
					break;
				ordre[j] = ordre[j - 1];
				--j;
			}
			ordre[j] = cle;
		}

		float32 yLigne = items[ordre[0]].y;
		float32 xPrec = -1.f;
		float32 hPrec = 0.f;
		for (nk_size k = 0; k < ordre.Size(); ++k) {
			const auto &it = items[ordre[k]];
			if (it.text.Size() == 0)
				continue; // police sans /ToUnicode : on saute plutôt qu'inventer
			const bool nouvelleLigne = (it.y < yLigne - toleranceLigne) || (it.y > yLigne + toleranceLigne);
			if (nouvelleLigne) {
				// Un écart vertical franchement plus grand qu'une ligne marque un
				// changement de paragraphe : c'est ce qui donne les lignes vides
				// dont tout le reste de la chaîne Ilyana se sert pour découper.
				const float32 saut = (it.y > yLigne) ? (it.y - yLigne) : (yLigne - it.y);
				page.Append((hPrec > 0.f && saut > hPrec * 1.8f) ? "\n\n" : "\n");
				yLigne = it.y;
				xPrec = -1.f;
			} else if (xPrec >= 0.f && it.x - xPrec > it.h * 0.28f) {
				// Les espaces ne sont pas toujours dessinés : un blanc horizontal
				// notable entre deux glyphes EST une espace. Sans cette règle, une
				// page entière ressort en un seul mot collé.
				page.Append(' ');
			}
			page.Append(it.text);
			xPrec = it.x + it.w;
			hPrec = it.h;
		}
		return page;
	}

	// Point d'entrée. `nbPages` rend le nombre de pages effectivement lues,
	// `pagesMuettes` celles qui n'ont donné aucun caractère — c'est le signalement
	// qui distingue « livre lu » de « livre dont les polices ne se déclarent pas ».
	// Compteurs de DIAGNOSTIC. Une extraction vide a deux causes opposées, et les
	// confondre mène à accuser le document quand le défaut est chez soi :
	//  - AUCUN caractère récolté  → le parcours de la page n'a rien produit ;
	//  - des caractères récoltés mais TOUS VIDES → les polices n'ont pas de table
	//    /ToUnicode, et le document est réellement indéchiffrable.
	struct DiagPdf {
		int64 glyphes = 0;	   // caractères rencontrés
		int64 sansUnicode = 0; // ... dont on ignore ce qu'ils représentent
		// Compteurs du rendu lui-même : ils disent OÙ la chaîne se rompt, ce
		// qu'aucune observation du résultat ne permet de deviner.
		int64 octetsContenu = 0; // 0 = le flux de la page n'a pas été décodé
		int64 operations = 0;	 // 0 = le flux n'a pas été exécuté
		int64 opsTexte = 0;		 // 0 = la page ne contient aucun ordre de texte
		int64 glyphesDemandes = 0;
		int64 glyphesObtenus = 0; // < demandés = polices non chargées
	};

	inline NkString LirePdf(const char *chemin, int64 &nbPages, int64 &pagesMuettes, double dpi = 72.0,
							DiagPdf *diag = nullptr) {
		using namespace nkentseu::nkcode::pdf;
		nbPages = 0;
		pagesMuettes = 0;
		NkString texte;

		NkPdfDoc doc;
		if (doc.Open(chemin) != NK_PDF_OK)
			return texte;

		const int32 n = doc.PageCount();
		for (int32 p = 0; p < n; ++p) {
			NkPdfRenderer rendu;
			NkPdfCanvas canevas;
			if (!rendu.RenderPage(doc, p, dpi, canevas))
				continue;
			if (diag) {
				const auto &items = rendu.TextItems();
				diag->glyphes += (int64)items.Size();
				for (nk_size i = 0; i < items.Size(); ++i)
					if (items[i].text.Size() == 0)
						++diag->sansUnicode;
				const auto &st = rendu.GetStats();
				diag->octetsContenu += st.contentBytes;
				diag->operations += st.ops;
				diag->opsTexte += st.textOps;
				diag->glyphesDemandes += st.glyphsAsked;
				diag->glyphesObtenus += st.glyphsGot;
			}
			const NkString t = AssemblerPage(rendu.TextItems());
			++nbPages;
			if (t.Size() < 20) {
				++pagesMuettes;
				continue;
			}
			texte.Append(t);
			texte.Append("\n\n", 2);
		}
		doc.Close();
		return texte;
	}

} // namespace ilyana

#endif // NK_ILYANA_PDF_H
