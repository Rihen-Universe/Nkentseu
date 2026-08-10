// =============================================================================
// NkIlyanaEpub — lire un livre au format EPUB.
// -----------------------------------------------------------------------------
// UN EPUB EST UNE ARCHIVE ZIP contenant des pages XHTML. Tout le travail tient
// donc en trois gestes : ouvrir le ZIP, décompresser les pages, retirer le
// balisage. Aucun des trois n'exige de bibliothèque extérieure.
//
// POURQUOI CE FICHIER DÉPEND DE NKImage — un voisinage qui surprend. La
// décompression DEFLATE du dépôt vit dans `NkDeflate`, à l'intérieur de NKImage,
// parce qu'elle y a été écrite pour le PNG. La réécrire ici donnerait deux
// implémentations du même algorithme, dont l'une serait moins éprouvée : c'est
// exactement ce qu'il faut éviter. La place JUSTE de `NkDeflate` serait NKStream
// (c'est un flux, pas une image) ; le déplacer touche un module partagé et
// attendra un moment plus calme.
//
// CE QUI EST DÉLIBÉRÉMENT SIMPLE, ET CE QUE ÇA COÛTE. L'ordre des chapitres
// devrait se lire dans le `content.opf` (la « spine »). On prend ici l'ordre du
// catalogue de l'archive, qui le suit presque toujours parce que les outils de
// publication écrivent les pages dans l'ordre. Un livre dont les chapitres
// sortiraient mélangés resterait parfaitement utilisable pour l'INDEX — un
// passage se retrouve quel que soit son rang — et gênerait surtout
// l'entraînement, où la continuité d'un raisonnement a du sens.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_MEDIA_EPUB_H
#define NK_MEDIA_EPUB_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKImage/Core/NkImage.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace media {


	struct EntreeZip {
		NkString nom;
		uint32 methode = 0;	 // 0 = stocké tel quel, 8 = deflate
		uint64 offsetLocal = 0;
		uint64 tailleComp = 0;
		uint64 tailleDecomp = 0;
	};

	inline uint16 Lire16(const uint8 *p) { return (uint16)(p[0] | (p[1] << 8)); }
	inline uint32 Lire32(const uint8 *p) {
		return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
	}

	// -------------------------------------------------------------------------
	// Catalogue de l'archive. On part de la FIN : un ZIP se lit à l'envers, par
	// son « end of central directory », qu'on cherche à reculons parce qu'un
	// commentaire de longueur libre peut le suivre.
	// -------------------------------------------------------------------------
	inline bool LireCatalogueZip(const uint8 *zip, nk_size taille, NkVector<EntreeZip> &out) {
		out.Clear();
		if (taille < 22)
			return false;
		nk_size eocd = 0;
		bool trouve = false;
		const nk_size mini = (taille > 66000) ? (taille - 66000) : 0;
		for (nk_size i = taille - 22 + 1; i-- > mini;) {
			if (Lire32(zip + i) == 0x06054b50u) {
				eocd = i;
				trouve = true;
				break;
			}
		}
		if (!trouve)
			return false;

		const uint16 nbEntrees = Lire16(zip + eocd + 10);
		const uint32 offsetCat = Lire32(zip + eocd + 16);
		if (offsetCat >= taille)
			return false;

		nk_size p = offsetCat;
		for (uint16 e = 0; e < nbEntrees; ++e) {
			if (p + 46 > taille || Lire32(zip + p) != 0x02014b50u)
				break;
			EntreeZip z;
			z.methode = Lire16(zip + p + 10);
			z.tailleComp = Lire32(zip + p + 20);
			z.tailleDecomp = Lire32(zip + p + 24);
			const uint16 lenNom = Lire16(zip + p + 28);
			const uint16 lenExtra = Lire16(zip + p + 30);
			const uint16 lenComm = Lire16(zip + p + 32);
			z.offsetLocal = Lire32(zip + p + 42);
			if (p + 46 + lenNom > taille)
				break;
			z.nom.Append((const char *)(zip + p + 46), lenNom);
			out.PushBack(z);
			p += 46 + lenNom + lenExtra + lenComm;
		}
		return out.Size() > 0;
	}

	// Extrait une entrée. Rend une chaîne vide en cas d'échec — un chapitre
	// manquant ne doit pas faire perdre le livre entier.
	inline NkString ExtraireEntree(const uint8 *zip, nk_size taille, const EntreeZip &z) {
		NkString out;
		if (z.offsetLocal + 30 > taille || Lire32(zip + z.offsetLocal) != 0x04034b50u)
			return out;
		const uint16 lenNom = Lire16(zip + z.offsetLocal + 26);
		const uint16 lenExtra = Lire16(zip + z.offsetLocal + 28);
		const nk_size debut = (nk_size)z.offsetLocal + 30 + lenNom + lenExtra;
		if (debut + (nk_size)z.tailleComp > taille)
			return out;

		if (z.methode == 0) {
			out.Append((const char *)(zip + debut), (nk_size)z.tailleComp);
			return out;
		}
		if (z.methode != 8)
			return out; // ni stocké ni deflate : format exotique, on passe

		// La taille décompressée annoncée peut valoir 0 quand elle n'est donnée
		// que dans le descripteur qui SUIT les données. On prévoit alors large
		// plutôt que d'échouer : une page de livre dépasse rarement quelques
		// centaines de kilo-octets.
		nk_size cap = (nk_size)z.tailleDecomp;
		if (cap == 0)
			cap = (nk_size)z.tailleComp * 20 + (1u << 16);
		uint8 *buf = (uint8 *)malloc(cap);
		if (!buf)
			return out;
		usize ecrit = 0;
		const bool ok = NkDeflate::DecompressRaw(zip + debut, (usize)z.tailleComp, buf, (usize)cap, ecrit);
		if (ok && ecrit > 0)
			out.Append((const char *)buf, (nk_size)ecrit);
		free(buf);
		return out;
	}

	// -------------------------------------------------------------------------
	// XHTML vers texte.
	//
	// On ne cherche pas à comprendre le HTML : on retire les balises, en gardant
	// les frontières qui portent du sens. Les balises de bloc (paragraphe, titre,
	// division, saut de ligne) deviennent des LIGNES VIDES, parce que c'est ce que
	// tout le reste de la chaîne Ilyana utilise comme séparateur de passage — un
	// livre dont les paragraphes ne seraient pas marqués deviendrait un bloc
	// unique, incitable et mal classé.
	// -------------------------------------------------------------------------
	inline bool MemeMot(const char *p, nk_size reste, const char *mot) {
		const nk_size n = strlen(mot);
		if (reste < n)
			return false;
		for (nk_size i = 0; i < n; ++i) {
			char c = p[i];
			if (c >= 'A' && c <= 'Z')
				c = (char)(c - 'A' + 'a');
			if (c != mot[i])
				return false;
		}
		return true;
	}

	inline NkString XhtmlVersTexte(const NkString &html) {
		NkString out;
		out.Reserve(html.Size() / 2 + 64);
		const char *p = html.Data();
		const nk_size n = html.Size();
		nk_size i = 0;
		while (i < n) {
			if (p[i] == '<') {
				// Le contenu des scripts et des styles n'est pas du texte de
				// livre : le garder polluerait le corpus de code.
				if (MemeMot(p + i, n - i, "<script") || MemeMot(p + i, n - i, "<style")) {
					const char *fin = MemeMot(p + i, n - i, "<script") ? "</script" : "</style";
					nk_size j = i + 1;
					while (j < n && !MemeMot(p + j, n - j, fin))
						++j;
					i = j;
					while (i < n && p[i] != '>')
						++i;
					if (i < n)
						++i;
					continue;
				}
				const bool bloc = MemeMot(p + i, n - i, "<p") || MemeMot(p + i, n - i, "</p") ||
								  MemeMot(p + i, n - i, "<br") || MemeMot(p + i, n - i, "<div") ||
								  MemeMot(p + i, n - i, "</div") || MemeMot(p + i, n - i, "<h1") ||
								  MemeMot(p + i, n - i, "<h2") || MemeMot(p + i, n - i, "<h3") ||
								  MemeMot(p + i, n - i, "<h4") || MemeMot(p + i, n - i, "</h1") ||
								  MemeMot(p + i, n - i, "</h2") || MemeMot(p + i, n - i, "</h3") ||
								  MemeMot(p + i, n - i, "</h4") || MemeMot(p + i, n - i, "<li") ||
								  MemeMot(p + i, n - i, "</li");
				while (i < n && p[i] != '>')
					++i;
				if (i < n)
					++i;
				if (bloc)
					out.Append("\n\n", 2);
				continue;
			}
			if (p[i] == '&') {
				// Entités les plus fréquentes. Les autres sont laissées telles
				// quelles : mieux vaut un « &oelig; » visible qu'un caractère
				// inventé au hasard.
				struct {
					const char *e;
					const char *r;
				} tbl[] = {{"&amp;", "&"},	 {"&lt;", "<"},		{"&gt;", ">"},	  {"&quot;", "\""},
						   {"&apos;", "'"},	 {"&#39;", "'"},	{"&nbsp;", " "},  {"&#160;", " "},
						   {"&eacute;", "é"}, {"&egrave;", "è"}, {"&agrave;", "à"}, {"&ccedil;", "ç"},
						   {"&rsquo;", "'"},  {"&laquo;", "«"},	 {"&raquo;", "»"}};
				bool remplace = false;
				for (nk_size t = 0; t < sizeof(tbl) / sizeof(tbl[0]); ++t) {
					const nk_size le = strlen(tbl[t].e);
					if (n - i >= le && memcmp(p + i, tbl[t].e, le) == 0) {
						out.Append(tbl[t].r);
						i += le;
						remplace = true;
						break;
					}
				}
				if (remplace)
					continue;
			}
			// Les retours à la ligne du source HTML ne sont que de la mise en
			// forme : dans un XHTML, seule la balise fait le paragraphe.
			out.Append((p[i] == '\n' || p[i] == '\r' || p[i] == '\t') ? ' ' : p[i]);
			++i;
		}
		return out;
	}

	inline bool EstPageLisible(const NkString &nom) {
		const nk_size pt = nom.RFind(".");
		if (pt == NkString::npos)
			return false;
		NkString ext = nom.SubStr(pt);
		for (nk_size i = 0; i < ext.Size(); ++i) {
			char *d = (char *)ext.Data();
			if (d[i] >= 'A' && d[i] <= 'Z')
				d[i] = (char)(d[i] - 'A' + 'a');
		}
		return ext == ".xhtml" || ext == ".html" || ext == ".htm";
	}

	// -------------------------------------------------------------------------
	// Point d'entrée : un chemin d'EPUB, le texte du livre.
	// `nbPages` rend le nombre de chapitres lus — un EPUB qui n'en rendrait
	// aucun est presque toujours un fichier protégé (DRM) et doit le dire.
	// -------------------------------------------------------------------------
	inline NkString LireEpub(const char *chemin, int64 &nbPages) {
		nbPages = 0;
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
		if (LireCatalogueZip(zip, lu, entrees)) {
			for (nk_size i = 0; i < entrees.Size(); ++i) {
				if (!EstPageLisible(entrees[i].nom))
					continue;
				const NkString html = ExtraireEntree(zip, lu, entrees[i]);
				if (html.Size() == 0)
					continue;
				const NkString t = XhtmlVersTexte(html);
				if (t.Size() < 40)
					continue; // page de garde, table des matières vide, etc.
				texte.Append(t);
				texte.Append("\n\n", 2);
				++nbPages;
			}
		}
		free(zip);
		return texte;
	}

	} // namespace media
} // namespace nkentseu

#endif // NK_MEDIA_EPUB_H
