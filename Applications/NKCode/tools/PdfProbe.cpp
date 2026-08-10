//
// PdfProbe.cpp — banc de test de l'analyseur PDF, en CONSOLE.
//
// Confronte NkPdfDoc a un corpus de fichiers REELS et n'affiche que des
// donnees STRUCTURELLES : nombre de pages, dimensions, rotation, etat.
// AUCUN contenu de document n'est lu, affiche ni enregistre — le corpus de
// validation est constitue de fichiers personnels.
//
// Usage :  NkPdfProbe <dossier|fichier> [...]
//
// Sortie : une ligne par fichier + un recapitulatif. Code de retour = nombre
// de fichiers en echec, pour pouvoir l'enchainer dans un script.
//
#include "NKMedia/Pdf/NkPdf.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::nkcode::pdf;

static bool EndsWithI(const char *s, const char *suf) {
	usize ls = 0, lf = 0;
	while (s[ls])
		++ls;
	while (suf[lf])
		++lf;
	if (lf > ls)
		return false;
	for (usize i = 0; i < lf; ++i) {
		char a = s[ls - lf + i], b = suf[i];
		if (a >= 'A' && a <= 'Z')
			a = static_cast<char>(a + 32);
		if (b >= 'A' && b <= 'Z')
			b = static_cast<char>(b + 32);
		if (a != b)
			return false;
	}
	return true;
}

// Affiche les octets non ASCII d'un chemin. Sert a distinguer un chemin UTF-8
// (« E accent aigu » = C3 89) d'un chemin en page de code ANSI (= C9 seul) :
// c'est le seul moyen de savoir QUI se trompe entre l'enumeration du dossier
// et l'ouverture du fichier.
static void DumpNonAscii(const char *s) {
	bool any = false;
	for (usize k = 0; s[k]; ++k) {
		const uint8 c = static_cast<uint8>(s[k]);
		if (c > 127) {
			if (!any)
				std::printf("        octets non-ASCII :");
			any = true;
			std::printf(" %02X", static_cast<unsigned>(c));
		}
	}
	if (any)
		std::printf("\n");
}

static void Collect(const NkString &root, NkVector<NkString> &out, int32 depth) {
	if (depth > 6)
		return;
	if (NkFile::Exists(root.CStr())) {
		if (EndsWithI(root.CStr(), ".pdf"))
			out.PushBack(root);
		return;
	}
	if (!NkDirectory::Exists(root.CStr()))
		return;
	NkVector<NkDirectoryEntry> ents =
		NkDirectory::GetEntries(root.CStr(), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
	for (usize i = 0; i < ents.Size(); ++i) {
		// FullPath fourni par l'API plutot qu'une concatenation maison.
		const NkString p = ents[i].FullPath.ToString();
		if (ents[i].IsDirectory)
			Collect(p, out, depth + 1);
		else if (EndsWithI(p.CStr(), ".pdf"))
			out.PushBack(p);
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		std::printf("usage : NkPdfProbe <dossier|fichier.pdf> [...]\n");
		return 2;
	}

	NkVector<NkString> files;
	for (int i = 1; i < argc; ++i)
		Collect(NkString(argv[i]), files, 0);

	if (files.Empty()) {
		std::printf("aucun PDF trouve.\n");
		return 2;
	}

	int32 ok = 0, ko = 0, pagesTotal = 0, chiffres = 0;
	int32 sansBoite = 0, tournees = 0;

	for (usize i = 0; i < files.Size(); ++i) {
		NkPdfDoc doc;
		const NkPdfStatus st = doc.Open(files[i].CStr());
		const nk_int64 sz = NkFile::GetFileSize(files[i].CStr());

		if (st != NK_PDF_OK) {
			++ko;
			if (st == NK_PDF_ERR_ENCRYPTED)
				++chiffres;
			// Chemin affiche UNIQUEMENT sur echec : « fichier illisible » ne se
			// diagnostique pas sans savoir lequel. Aucun CONTENU n'est jamais lu.
			std::printf("  [%3d] %8lld o  ECHEC  %s\n        chemin : %s\n", static_cast<int>(i + 1),
						static_cast<long long>(sz), doc.StatusText(), files[i].CStr());
			DumpNonAscii(files[i].CStr());
			continue;
		}
		++ok;
		const int32 n = doc.PageCount();
		pagesTotal += n;

		double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
		const bool box = doc.PageMediaBox(0, &x0, &y0, &x1, &y1);
		if (!box)
			++sansBoite;
		const int32 rot = doc.PageRotate(0);
		if (rot != 0)
			++tournees;

		std::printf("  [%3d] %8lld o  %4d page(s)  %s", static_cast<int>(i + 1),
					static_cast<long long>(sz), n, box ? "" : "MediaBox absente");
		if (box)
			std::printf("%.0f x %.0f pt", x1 - x0, y1 - y0);
		if (rot)
			std::printf("  rot %d", rot);
		if (!doc.UnsupportedFilter().Empty())
			std::printf("  [filtre non gere : %s]", doc.UnsupportedFilter().CStr());
		std::printf("\n");
	}

	std::printf("\n================ RECAPITULATIF ================\n");
	std::printf("  fichiers analyses : %d\n", static_cast<int>(files.Size()));
	std::printf("  succes            : %d\n", ok);
	std::printf("  echecs            : %d  (dont %d chiffres, hors perimetre v1)\n", ko, chiffres);
	std::printf("  pages au total    : %d\n", pagesTotal);
	std::printf("  sans MediaBox     : %d\n", sansBoite);
	std::printf("  pages tournees    : %d\n", tournees);
	// Les documents chiffres ne sont pas des defauts : ils sont hors perimetre
	// ASSUME et signales comme tels a l'utilisateur.
	return ko - chiffres;
}
