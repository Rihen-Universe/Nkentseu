// =============================================================================
// NkArchive — extraire le code source et le texte d'une archive ZIP.
// -----------------------------------------------------------------------------
// RIEN DE NEUF N'EST ECRIT ICI. Un EPUB est deja une archive ZIP, donc la lecture
// du catalogue, la decompression et le parcours des entrees existent (NkEpub.h).
// Ce fichier ne fait que choisir d'AUTRES fichiers dedans : du code source et du
// texte au lieu de pages XHTML.
//
// POURQUOI GARDER LE CHEMIN DE CHAQUE FICHIER. Les sources d'une archive sont
// concatenees, et sans marqueur on ne saurait plus ou finit un fichier et ou
// commence le suivant — deux fonctions de deux projets differents se retrouveraient
// collees comme si elles se suivaient. Chaque fichier est donc precede de son
// chemin, ce qui sert trois fois : le modele apprend qu'un fichier a un nom et une
// fin, la recherche peut citer le fichier, et un humain s'y retrouve.
//
// CE QU'ON EXCLUT, ET POURQUOI. Les binaires, images et fichiers compiles n'ont
// aucun texte a offrir ; les inclure remplirait le corpus d'octets aleatoires que
// le tokenizer decouperait en miettes, au detriment de tout le reste. Les fichiers
// enormes (donnees generees, journaux) sont ecartes pour la meme raison : ils
// pesent lourd et n'apprennent rien.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_MEDIA_ARCHIVE_H
#define NK_MEDIA_ARCHIVE_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKMedia/Document/NkEpub.h"

namespace nkentseu {
	namespace media {

		// Extensions dont le contenu est du texte utile. Volontairement large sur
		// les langages : un fonds d'apprentissage melange souvent C++, shaders,
		// scripts de construction et notes.
		inline bool ExtensionDeSource(const NkString &nom) {
			static const char *kExt[] = {".h",	 ".hpp", ".hh",	  ".hxx", ".c",	   ".cpp",	".cc",	 ".cxx",
										 ".cs",	 ".java", ".py",  ".js",  ".ts",   ".rs",	".go",	 ".lua",
										 ".glsl", ".hlsl", ".vert", ".frag", ".comp", ".geom", ".tesc", ".tese",
										 ".metal", ".md",  ".txt",	".rst", ".ini",	 ".cfg",  ".yml",  ".yaml",
										 ".json", ".xml",  ".cmake", ".mk",	 ".sh",	 ".bat",  ".jenga"};
			const nk_size n = sizeof(kExt) / sizeof(kExt[0]);
			// Comparaison en minuscules : une archive melange souvent .CPP et .cpp.
			NkString bas = nom;
			for (nk_size i = 0; i < bas.Size(); ++i) {
				char *d = (char *)bas.Data();
				if (d[i] >= 'A' && d[i] <= 'Z')
					d[i] = (char)(d[i] - 'A' + 'a');
			}
			for (nk_size i = 0; i < n; ++i) {
				const NkString e(kExt[i]);
				if (bas.Size() > e.Size() && bas.SubStr(bas.Size() - e.Size()) == e)
					return true;
			}
			return false;
		}

		// Un fichier qui n'est pas du texte se reconnait a ses octets nuls : aucun
		// texte n'en contient, et toutes les donnees binaires en sont pleines.
		// Ce controle rattrape les fichiers dont l'extension ment.
		inline bool RessembleADuTexte(const NkString &s) {
			const nk_size n = (s.Size() < 4096) ? s.Size() : 4096;
			for (nk_size i = 0; i < n; ++i)
				if (s.Data()[i] == '\0')
					return false;
			return n > 0;
		}

		// Lit une archive. `fichiers` rend le nombre de fichiers retenus,
		// `ignores` ceux qui ont ete ecartes — sans quoi on ne saurait pas si une
		// archive a ete lue ou seulement ouverte.
		// Pourquoi une entree a ete ecartee. Sans cette ventilation, « 6 ecartes »
		// ne dit pas si l'archive contient autre chose que du code, ou si la
		// decompression echoue — deux causes opposees qu'on ne peut pas distinguer
		// en regardant le resultat.
		struct DiagArchive {
			int64 catalogueIllisible = 0; // l'archive elle-meme n'a pas pu etre ouverte
			int64 dossiers = 0;
			int64 mauvaiseExtension = 0;
			int64 tropGros = 0;
			int64 decompressionRatee = 0;
			int64 pasDuTexte = 0;
		};

		inline NkString LireArchive(const char *chemin, int64 &fichiers, int64 &ignores,
									nk_size tailleMax = 2u << 20, DiagArchive *diag = nullptr) {
			fichiers = 0;
			ignores = 0;
			NkString texte;

			FILE *f = fopen(chemin, "rb");
			if (!f)
				return texte;
			fseek(f, 0, SEEK_END);
			const nk_size taille = (nk_size)ftell(f);
			fseek(f, 0, SEEK_SET);
			uint8 *zip = (uint8 *)malloc(taille);
			if (!zip) {
				fclose(f);
				return texte;
			}
			const nk_size lu = fread(zip, 1, taille, f);
			fclose(f);

			NkVector<EntreeZip> entrees;
			if (!LireCatalogueZip(zip, lu, entrees)) {
				if (diag)
					++diag->catalogueIllisible;
			} else {
				for (nk_size i = 0; i < entrees.Size(); ++i) {
					// Un dossier est une entree de taille nulle finissant par « / ».
					if (entrees[i].nom.Size() == 0 ||
						entrees[i].nom.Data()[entrees[i].nom.Size() - 1] == '/') {
						if (diag)
							++diag->dossiers;
						continue;
					}
					if (!ExtensionDeSource(entrees[i].nom)) {
						++ignores;
						if (diag)
							++diag->mauvaiseExtension;
						continue;
					}
					if (entrees[i].tailleDecomp > tailleMax) {
						++ignores;
						if (diag)
							++diag->tropGros;
						continue;
					}
					const NkString contenu = ExtraireEntree(zip, lu, entrees[i]);
					if (contenu.Size() == 0) {
						++ignores;
						if (diag)
							++diag->decompressionRatee;
						continue;
					}
					if (!RessembleADuTexte(contenu)) {
						++ignores;
						if (diag)
							++diag->pasDuTexte;
						continue;
					}
					// Le chemin precede le contenu : sans lui, deux fichiers colles
					// passeraient pour un seul.
					texte.Append("[fichier] ");
					texte.Append(entrees[i].nom);
					texte.Append("\n\n", 2);
					texte.Append(contenu);
					texte.Append("\n\n", 2);
					++fichiers;
				}
			}
			free(zip);
			return texte;
		}

	} // namespace media
} // namespace nkentseu

#endif // NK_MEDIA_ARCHIVE_H
