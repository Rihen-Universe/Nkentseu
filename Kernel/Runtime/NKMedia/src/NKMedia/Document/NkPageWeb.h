// =============================================================================
// NkPageWeb — extraire l'ARTICLE d'une page web, et jeter le reste.
// -----------------------------------------------------------------------------
// LE PROBLEME N'EST PAS DE LIRE LE HTML, C'EST DE TRIER. Retirer les balises est
// l'affaire de quelques lignes, et c'est déjà fait pour l'EPUB. Mais une page web
// est faite à 80 % de ce qui n'est pas l'article : menus, bandeau de cookies,
// pied de page, « articles similaires », commentaires, mentions légales. Déposer
// tout cela dans la bibliothèque la polluerait bien plus que ça ne l'enrichirait,
// et la recherche rendrait des morceaux de navigation en guise de réponse.
//
// LE TRI, ET POURQUOI IL TIENT EN UNE OBSERVATION. Un élément de navigation est
// COURT et sans ponctuation de phrase : « Accueil », « Nos offres », « En savoir
// plus ». Un paragraphe d'article est LONG et se termine par un point. C'est une
// règle grossière, et elle suffit — elle ne se trompe que sur les légendes et les
// titres, qu'on perd sans grand dommage, et jamais sur le corps du texte.
//
// CE QUE CE TRI NE FAIT PAS. Il ne distingue pas un article d'un commentaire de
// lecteur, ni un texte fiable d'un texte douteux. La page déposée engage donc
// celui qui la dépose — comme un livre.
//
// TRACER L'ORIGINE. Une page web change et disparaît, là où un livre reste. Son
// adresse et la date du prélèvement sont donc consignées au catalogue : une
// citation qui renvoie à une page devenue introuvable doit au moins dire d'où
// elle venait, et quand.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_MEDIA_PAGEWEB_H
#define NK_MEDIA_PAGEWEB_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKMedia/Document/NkEpub.h"

namespace nkentseu {
	namespace media {

		// Un bloc a-t-il l'allure d'un vrai paragraphe ?
		inline bool BlocDArticle(const NkString &b, nk_size longueurMini) {
			if (b.Size() < longueurMini)
				return false;
			// Une phrase se termine. Un lien de menu, non.
			bool ponctue = false;
			for (nk_size i = 0; i < b.Size(); ++i) {
				const char c = b.Data()[i];
				if (c == '.' || c == '!' || c == '?') {
					ponctue = true;
					break;
				}
			}
			if (!ponctue)
				return false;
			// Une accumulation de « | » ou de « · » trahit une barre de navigation
			// qui aurait échappé au filtrage des balises.
			nk_size separateurs = 0;
			for (nk_size i = 0; i < b.Size(); ++i)
				if (b.Data()[i] == '|')
					++separateurs;
			return separateurs * 40 < b.Size();
		}

		// HTML brut -> texte de l'article, un paragraphe par bloc.
		// `gardes` et `jetes` rendent le compte, parce qu'un tri qui ne dit pas ce
		// qu'il a jeté ne se vérifie pas.
		inline NkString PageWebVersTexte(const NkString &html, int64 &gardes, int64 &jetes,
										 nk_size longueurMini = 180) {
			gardes = 0;
			jetes = 0;
			const NkString brut = XhtmlVersTexte(html);

			NkString out;
			out.Reserve(brut.Size() / 2);
			nk_size i = 0;
			while (i < brut.Size()) {
				nk_size fin = brut.Find("\n\n", i);
				if (fin == NkString::npos)
					fin = brut.Size();
				NkString bloc = brut.SubStr(i, fin - i);
				i = (fin >= brut.Size()) ? brut.Size() : fin + 2;

				// Espaces en trop : le HTML en sème partout.
				NkString net;
				net.Reserve(bloc.Size());
				bool blanc = true;
				for (nk_size k = 0; k < bloc.Size(); ++k) {
					const char c = (bloc.Data()[k] == '\n' || bloc.Data()[k] == '\t') ? ' ' : bloc.Data()[k];
					if (c == ' ') {
						if (!blanc)
							net.Append(' ');
						blanc = true;
					} else {
						net.Append(c);
						blanc = false;
					}
				}
				while (net.Size() > 0 && net.Data()[net.Size() - 1] == ' ')
					net = net.SubStr(0, net.Size() - 1);

				if (!BlocDArticle(net, longueurMini)) {
					++jetes;
					continue;
				}
				out.Append(net);
				out.Append("\n\n", 2);
				++gardes;
			}
			return out;
		}

	} // namespace media
} // namespace nkentseu

#endif // NK_MEDIA_PAGEWEB_H
