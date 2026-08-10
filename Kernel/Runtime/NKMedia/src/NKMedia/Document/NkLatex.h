// =============================================================================
// NkLatex — extraire le texte d'une source LaTeX.
// -----------------------------------------------------------------------------
// POURQUOI CE LECTEUR VAUT MIEUX QUE LE LECTEUR PDF, ET DE LOIN, POUR LES
// SCIENCES. Un PDF ne contient pas de texte : il contient des glyphes posés à des
// coordonnées, et une formule y devient une poussière de symboles dont la
// structure est définitivement perdue. La SOURCE LaTeX, elle, dit exactement ce
// que la formule est — « \int_0^1 x^2 dx » se lit, se cherche, et s'apprend.
// Pour un livre de mathématiques ou de physique dont on a le .tex, il n'y a pas
// à hésiter : le .tex est la bonne source, le PDF est une image de celle-ci.
//
// CE QU'ON GARDE, ET POURQUOI :
//  - les FORMULES sont conservées telles quelles, délimiteurs compris. Les
//    dépouiller donnerait « int 0 1 x 2 dx », qui ne veut plus rien dire ;
//  - les TITRES deviennent des paragraphes ordinaires : ils portent le sujet de
//    ce qui suit, et servent donc à retrouver un passage ;
//  - la LIGNE VIDE de LaTeX marque déjà le paragraphe — c'est exactement le
//    séparateur qu'utilise le reste de la chaîne. Rien à convertir.
//
// CE QU'ON JETTE : le préambule (des réglages, aucun contenu), les commentaires,
// et les commandes de mise en page qui ne portent pas de texte.
//
// LES \input ET \include SONT SUIVIS — sans quoi un livre découpé en un fichier
// par chapitre, ce qui est l'usage courant, ne rendrait que sa page de titre.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_MEDIA_LATEX_H
#define NK_MEDIA_LATEX_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace media {

		// Commandes dont on garde l'ARGUMENT (le texte) en jetant l'habillage.
		inline bool CommandeAGarderLArgument(const char *nom, nk_size n) {
			static const char *liste[] = {"textbf",	  "textit",	  "emph",	  "texttt",	  "textsc",	 "underline",
										  "section",  "subsection", "subsubsection", "chapter", "paragraph",
										  "title",	  "author",	  "caption", "textrm", "mbox", "text"};
			for (nk_size i = 0; i < sizeof(liste) / sizeof(liste[0]); ++i)
				if (strlen(liste[i]) == n && memcmp(nom, liste[i], n) == 0)
					return true;
			return false;
		}

		// Environnements dont le CONTENU n'est pas du texte de lecture.
		inline bool EnvironnementAJeter(const char *nom, nk_size n) {
			static const char *liste[] = {"figure", "tikzpicture", "lstlisting", "verbatim", "minted", "thebibliography"};
			for (nk_size i = 0; i < sizeof(liste) / sizeof(liste[0]); ++i)
				if (strlen(liste[i]) == n && memcmp(nom, liste[i], n) == 0)
					return true;
			return false;
		}

		inline NkString LireFichierBrut(const char *chemin) {
			NkString s;
			FILE *f = fopen(chemin, "rb");
			if (!f)
				return s;
			char buf[1 << 16];
			for (;;) {
				const nk_size got = fread(buf, 1, sizeof(buf), f);
				if (got == 0)
					break;
				s.Append(buf, got);
			}
			fclose(f);
			return s;
		}

		inline NkString DossierDe(const char *chemin) {
			NkString p(chemin);
			const nk_size s1 = p.RFind("/");
			const nk_size s2 = p.RFind("\\");
			nk_size coupe = NkString::npos;
			if (s1 != NkString::npos)
				coupe = s1;
			if (s2 != NkString::npos && (coupe == NkString::npos || s2 > coupe))
				coupe = s2;
			return (coupe == NkString::npos) ? NkString(".") : p.SubStr(0, coupe);
		}

		// Convertit UNE source. `profondeur` borne la récursion des \input : un
		// fichier qui s'inclut lui-même, par erreur, boucherait sinon la mémoire.
		inline NkString LatexVersTexte(const NkString &src, const NkString &dossier, int32 profondeur = 0);

		inline NkString LireLatex(const char *chemin, int32 profondeur = 0) {
			const NkString src = LireFichierBrut(chemin);
			if (src.Size() == 0)
				return NkString();
			return LatexVersTexte(src, DossierDe(chemin), profondeur);
		}

		inline NkString LatexVersTexte(const NkString &src, const NkString &dossier, int32 profondeur) {
			NkString out;
			out.Reserve(src.Size());

			// Le préambule ne contient que des réglages. S'il y a un
			// \begin{document}, tout ce qui précède est écarté.
			nk_size i = 0;
			const nk_size doc = src.Find("\\begin{document}");
			if (doc != NkString::npos)
				i = doc + 16;

			const char *p = src.Data();
			const nk_size n = src.Size();
			while (i < n) {
				// Commentaire : du % jusqu'à la fin de ligne, sauf s'il est
				// échappé (\%), auquel cas c'est un vrai signe pour cent.
				if (p[i] == '%' && (i == 0 || p[i - 1] != '\\')) {
					while (i < n && p[i] != '\n')
						++i;
					continue;
				}
				if (p[i] != '\\') {
					out.Append(p[i]);
					++i;
					continue;
				}

				// \\ = saut de ligne
				if (i + 1 < n && p[i + 1] == '\\') {
					out.Append('\n');
					i += 2;
					continue;
				}
				// Caractères échappés : on rend le caractère lui-même.
				if (i + 1 < n && !((p[i + 1] >= 'a' && p[i + 1] <= 'z') || (p[i + 1] >= 'A' && p[i + 1] <= 'Z'))) {
					out.Append(p[i + 1]);
					i += 2;
					continue;
				}

				// Nom de la commande.
				nk_size deb = i + 1;
				nk_size fin = deb;
				while (fin < n && ((p[fin] >= 'a' && p[fin] <= 'z') || (p[fin] >= 'A' && p[fin] <= 'Z')))
					++fin;
				const char *nom = p + deb;
				const nk_size lnom = fin - deb;
				i = fin;
				while (i < n && p[i] == ' ')
					++i;

				// \begin{env} / \end{env}
				if (lnom == 5 && memcmp(nom, "begin", 5) == 0 && i < n && p[i] == '{') {
					const nk_size a = i + 1;
					nk_size b = a;
					while (b < n && p[b] != '}')
						++b;
					const nk_size lenv = b - a;
					i = (b < n) ? b + 1 : n;
					if (EnvironnementAJeter(p + a, lenv)) {
						// On saute jusqu'au \end correspondant. Pas d'imbrication
						// gérée : ces environnements ne s'imbriquent pas en pratique.
						NkString marque("\\end{");
						marque.Append(p + a, lenv);
						marque.Append('}');
						const nk_size e = src.Find(marque.CStr(), i);
						i = (e == NkString::npos) ? n : e + marque.Size();
					}
					out.Append("\n\n", 2);
					continue;
				}
				if (lnom == 3 && memcmp(nom, "end", 3) == 0 && i < n && p[i] == '{') {
					while (i < n && p[i] != '}')
						++i;
					if (i < n)
						++i;
					out.Append("\n\n", 2);
					continue;
				}

				// \input{f} / \include{f} : on suit le fichier.
				const bool inclut = (lnom == 5 && memcmp(nom, "input", 5) == 0) ||
									(lnom == 7 && memcmp(nom, "include", 7) == 0);
				if (inclut && i < n && p[i] == '{') {
					const nk_size a = i + 1;
					nk_size b = a;
					while (b < n && p[b] != '}')
						++b;
					NkString rel = src.SubStr(a, b - a);
					i = (b < n) ? b + 1 : n;
					if (profondeur < 6 && rel.Size() > 0) {
						NkString chemin = dossier;
						chemin.Append('/');
						chemin.Append(rel);
						NkString sous = LireLatex(chemin.CStr(), profondeur + 1);
						if (sous.Size() == 0) {
							// LaTeX autorise d'omettre l'extension.
							chemin.Append(".tex");
							sous = LireLatex(chemin.CStr(), profondeur + 1);
						}
						if (sous.Size() > 0) {
							out.Append("\n\n", 2);
							out.Append(sous);
							out.Append("\n\n", 2);
						}
					}
					continue;
				}

				// Commandes dont on garde l'argument : on retire seulement les
				// accolades, en laissant le texte qu'elles portent.
				if (CommandeAGarderLArgument(nom, lnom) && i < n && p[i] == '{') {
					int prof = 0;
					nk_size b = i;
					for (; b < n; ++b) {
						if (p[b] == '{')
							++prof;
						else if (p[b] == '}') {
							--prof;
							if (prof == 0)
								break;
						}
					}
					const NkString dedans = src.SubStr(i + 1, (b > i + 1) ? (b - i - 1) : 0);
					out.Append("\n\n", 2);
					out.Append(LatexVersTexte(dedans, dossier, profondeur + 1));
					out.Append("\n\n", 2);
					i = (b < n) ? b + 1 : n;
					continue;
				}

				// Toute autre commande : on la retire avec ses arguments entre
				// accolades ou crochets, qui sont des réglages et non du texte.
				while (i < n && (p[i] == '{' || p[i] == '[')) {
					const char ouvre = p[i];
					const char ferme = (ouvre == '{') ? '}' : ']';
					int prof = 0;
					for (; i < n; ++i) {
						if (p[i] == ouvre)
							++prof;
						else if (p[i] == ferme) {
							--prof;
							if (prof == 0) {
								++i;
								break;
							}
						}
					}
				}
			}
			return out;
		}

	} // namespace media
} // namespace nkentseu

#endif // NK_MEDIA_LATEX_H
