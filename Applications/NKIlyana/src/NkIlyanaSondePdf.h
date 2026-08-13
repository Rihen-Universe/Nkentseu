// =============================================================================
// NkIlyanaSondePdf — SONDAGE d'un corpus de PDF, avant d'écrire quoi que ce soit.
// -----------------------------------------------------------------------------
// POURQUOI CE FICHIER EXISTE. Le périmètre du lecteur PDF a été fixé par une
// MESURE sur 95 documents réels (cf. l'en-tête de `NkPdf.h`), et non d'après la
// spécification : on implémente ce que le corpus contient vraiment. Ajouter les
// métadonnées, les signets, les liens ou l'arbre de structure mérite la même
// discipline — sans quoi on écrirait des milliers de lignes pour des clés que
// personne ne possède. Deux fois cette semaine, une mesure a évité d'écrire un
// module entier inutile.
//
// Ce mode ne produit AUCUNE donnée pour la bibliothèque : il compte, il affiche,
// il ne dépose rien.
//
// CE QU'IL SAIT VOIR, ET CE QU'IL NE SAIT PAS. Les clés de document sont lues
// via le modèle d'objets (`NkPdfDoc`), donc y compris à travers les flux
// d'objets compressés. Les FILTRES, eux, sont cherchés dans les octets bruts du
// fichier : c'est fiable ici, parce que la spécification interdit qu'un flux
// vive dans un flux d'objets — un dictionnaire de flux est donc toujours en
// clair. Sans ce raisonnement, il aurait fallu parcourir toute l'arène.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_ILYANA_SONDE_PDF_H
#define NK_ILYANA_SONDE_PDF_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKMedia/Pdf/NkPdf.h"

#include <cstdio>

namespace ilyana {
	using namespace nkentseu;

	// Ce qu'un document déclare. Tout est compté « présent ET non vide » : une
	// clé présente mais vide ne vaut rien pour l'indexation, et la compter
	// gonflerait le score d'une fonctionnalité qui ne rendrait rien.
	struct SondePdf {
			bool ouvert = false;
			bool chiffre = false;

			bool info = false;		// /Info du trailer, avec au moins un champ non vide
			int32 infoChamps = 0;	// combien de ses 8 champs sont renseignés
			bool titre = false;		// /Title non vide — le champ le plus utile
			bool metadata = false;	// /Metadata (flux XMP) non vide

			int32 signets = 0;		// entrées de /Outlines effectivement parcourues
			int32 annots = 0;		// annotations, toutes natures
			int32 liens = 0;		// dont /Subtype /Link
			int32 notes = 0;		// dont /Text (commentaires)
			int32 surlignages = 0;	// dont /Highlight

			int32 champsForm = 0;	// /AcroForm /Fields (premier niveau)
			bool structTree = false; // /StructTreeRoot
			bool dests = false;		// /Dests (catalogue) ou /Names /Dests
			bool embarques = false;	// /Names /EmbeddedFiles
			bool lang = false;		// /Lang

			// Filtres relevés dans les octets bruts.
			bool lzw = false, ccitt = false, jbig2 = false, jpx = false, dct = false;
	};

	// Cherche une suite d'octets dans un tampon. Rendue ici plutôt qu'empruntée
	// à NkString : on travaille sur des octets bruts, pas sur du texte, et un
	// PDF contient des zéros au milieu.
	inline bool ContientOctets(const uint8 *tampon, usize taille, const char *motif) {
		usize n = 0;
		while (motif[n])
			++n;
		if (n == 0 || taille < n)
			return false;
		for (usize i = 0; i + n <= taille; ++i) {
			usize k = 0;
			while (k < n && tampon[i + k] == (uint8)motif[k])
				++k;
			if (k == n)
				return true;
		}
		return false;
	}

	// Compte les entrées d'un arbre de signets. Le parcours suit /First puis
	// /Next, et descend par /First.
	//
	// ⚠️ DEUX gardes, et non une. La profondeur seule ne suffit pas : un /Next
	// qui pointe vers un frère déjà vu boucle SANS jamais descendre. C'est la
	// leçon de la chaîne /Prev du 11 août — une borne arbitraire basse rejetait
	// un document légitime à 53 maillons, alors qu'une liste d'objets visités
	// arrête un cycle sans jamais limiter un document sain.
	inline int32 CompterSignets(const media::pdf::NkPdfDoc &doc, const media::pdf::NkPdfVal &premier,
								NkVector<int32> &vus, int32 profondeur) {
		using namespace nkentseu::media::pdf;
		if (profondeur > 32 || vus.Size() > 20000u)
			return 0;
		int32 total = 0;
		NkPdfVal noeud = premier;
		while (noeud.IsDictLike()) {
			// Identité de l'objet = (kind, a) : `a` indexe sa première entrée
			// dans l'arène, et il est unique par objet.
			bool deja = false;
			for (nk_size i = 0; i < vus.Size(); ++i)
				if (vus[i] == noeud.a) {
					deja = true;
					break;
				}
			if (deja)
				break;
			vus.PushBack(noeud.a);
			++total;

			const NkPdfVal fils = doc.DictGet(noeud, "First");
			if (fils.IsDictLike())
				total += CompterSignets(doc, fils, vus, profondeur + 1);

			noeud = doc.DictGet(noeud, "Next");
		}
		return total;
	}

	// Sonde UN document. Ne dépose rien, ne rend rien à la bibliothèque.
	inline SondePdf SonderPdf(const char *chemin) {
		using namespace nkentseu::media::pdf;
		SondePdf s;

		// ── Filtres : sur les octets bruts (voir l'en-tête du fichier) ──
		{
			FILE *f = fopen(chemin, "rb");
			if (f) {
				fseek(f, 0, SEEK_END);
				const long taille = ftell(f);
				fseek(f, 0, SEEK_SET);
				if (taille > 0) {
					NkVector<uint8> brut;
					brut.Resize((nk_size)taille);
					const nk_size lu = (nk_size)fread(brut.Data(), 1, (nk_size)taille, f);
					s.lzw = ContientOctets(brut.Data(), lu, "LZWDecode");
					s.ccitt = ContientOctets(brut.Data(), lu, "CCITTFaxDecode");
					s.jbig2 = ContientOctets(brut.Data(), lu, "JBIG2Decode");
					s.jpx = ContientOctets(brut.Data(), lu, "JPXDecode");
					s.dct = ContientOctets(brut.Data(), lu, "DCTDecode");
				}
				fclose(f);
			}
		}

		NkPdfDoc doc;
		const NkPdfStatus st = doc.Open(chemin);
		if (st != NK_PDF_OK) {
			s.chiffre = (st == NK_PDF_ERR_ENCRYPTED);
			return s;
		}
		s.ouvert = true;

		// ── /Info : dans le TRAILER, pas dans le catalogue ──
		{
			const NkPdfVal info = doc.DictGet(doc.Trailer(), "Info");
			if (info.IsDictLike()) {
				static const char *kChamps[] = {"Title",	"Author",		"Subject",	  "Keywords",
												"Creator", "Producer", "CreationDate", "ModDate"};
				for (int32 i = 0; i < 8; ++i) {
					const NkPdfVal v = doc.DictGet(info, kChamps[i]);
					int32 len = 0;
					if (v.kind == NK_PDF_STRING) {
						doc.Text(v, &len);
						if (len > 0) {
							++s.infoChamps;
							if (i == 0)
								s.titre = true;
						}
					}
				}
				s.info = (s.infoChamps > 0);
			}
		}

		const NkPdfVal cat = doc.Catalog();

		// ── /Metadata : un flux XMP. On ne le décode pas ici (le sondage ne
		// doit pas coûter le prix d'une décompression par document) ; sa
		// présence en tant que FLUX suffit à décider s'il vaut la peine. ──
		{
			const NkPdfVal meta = doc.DictGet(cat, "Metadata");
			s.metadata = (meta.kind == NK_PDF_STREAM && meta.rawLen > 0);
		}

		// ── /Outlines ──
		{
			const NkPdfVal out = doc.DictGet(cat, "Outlines");
			const NkPdfVal premier = doc.DictGet(out, "First");
			if (premier.IsDictLike()) {
				NkVector<int32> vus;
				s.signets = CompterSignets(doc, premier, vus, 0);
			}
		}

		// ── /Annots, page par page ──
		for (int32 p = 0; p < doc.PageCount(); ++p) {
			const NkPdfVal annots = doc.DictGet(doc.Page(p), "Annots");
			const int32 n = doc.ArraySize(annots);
			for (int32 i = 0; i < n; ++i) {
				const NkPdfVal a = doc.ArrayAt(annots, i);
				if (!a.IsDictLike())
					continue;
				++s.annots;
				const NkPdfVal sub = doc.DictGet(a, "Subtype");
				if (doc.NameIs(sub, "Link"))
					++s.liens;
				else if (doc.NameIs(sub, "Text"))
					++s.notes;
				else if (doc.NameIs(sub, "Highlight"))
					++s.surlignages;
			}
		}

		// ── /AcroForm, /StructTreeRoot, /Dests, /EmbeddedFiles, /Lang ──
		{
			const NkPdfVal acro = doc.DictGet(cat, "AcroForm");
			s.champsForm = doc.ArraySize(doc.DictGet(acro, "Fields"));

			s.structTree = doc.DictGet(cat, "StructTreeRoot").IsDictLike();

			const NkPdfVal noms = doc.DictGet(cat, "Names");
			s.embarques = doc.DictGet(noms, "EmbeddedFiles").IsDictLike();
			s.dests = doc.DictGet(cat, "Dests").IsDictLike() ||
					  doc.DictGet(noms, "Dests").IsDictLike();

			const NkPdfVal lg = doc.DictGet(cat, "Lang");
			int32 len = 0;
			if (lg.kind == NK_PDF_STRING) {
				doc.Text(lg, &len);
				s.lang = (len > 0);
			}
		}

		doc.Close();
		return s;
	}

} // namespace ilyana

#endif // NK_ILYANA_SONDE_PDF_H
