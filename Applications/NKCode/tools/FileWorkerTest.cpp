//
// FileWorkerTest.cpp — verifie DEUX chemins soupconnes d'une regression :
//   1. la lecture de fichiers via NkFile (texte ET binaire), apres le passage
//      aux API larges de Windows ;
//   2. le fil de rendu PDF, qui doit livrer une page sans bloquer.
//
// Un « ca ne marche plus » ne se diagnostique pas en relisant le code : il faut
// exercer le chemin exact et regarder ce qui sort.
//
#include "NKCode/Pdf/NkPdf.h"
#include "NKCode/Shell/NkPdfWorker.h"

#include "NKFileSystem/NkFile.h"

#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace nkentseu;
using namespace nkentseu::nkcode;

static void Dodo(unsigned ms) {
#if defined(_WIN32)
	::Sleep(ms);
#else
	::usleep(ms * 1000u);
#endif
}

static int gFail = 0;

static void Check(const char *quoi, bool ok, const char *detail) {
	std::printf("  [%s] %-46s %s\n", ok ? "OK   " : "ECHEC", quoi, detail ? detail : "");
	if (!ok)
		++gFail;
}

int main(int argc, char **argv) {
	std::printf("=== Lecture de fichiers (NkFile) ===\n");

	// Texte : c'est le chemin qu'emprunte l'apercu Markdown.
	if (argc > 1) {
		const NkString txt = NkFile::ReadAllText(argv[1]);
		char d[96];
		std::snprintf(d, sizeof(d), "%d octets lus", static_cast<int>(txt.Size()));
		Check("ReadAllText sur le fichier fourni", !txt.Empty(), d);
	}

	// Binaire : chemin du PDF.
	if (argc > 2) {
		NkVector<uint8> b = NkFile::ReadAllBytes(argv[2]);
		char d[96];
		std::snprintf(d, sizeof(d), "%d octets lus", static_cast<int>(b.Size()));
		Check("ReadAllBytes sur le PDF fourni", !b.Empty(), d);

		std::printf("\n=== Fil de rendu PDF ===\n");
		pdf::NkPdfDoc doc;
		const bool op = (doc.Open(argv[2]) == pdf::NK_PDF_OK);
		Check("ouverture du document", op, op ? "" : doc.StatusText());
		if (op) {
			NkPdfWorker w;
			Check("demarrage du fil", w.Open(NkString(argv[2])), "");
			Check("demande de la page 0", w.Request(0, 1.0, 96.0), "");

			// On attend au plus 20 secondes : si rien n'arrive, le fil ne livre pas.
			int32 page = -1;
			double zoom = 0.0;
			pdf::NkPdfCanvas cv;
			NkVector<pdf::NkPdfRenderer::TextItem> items;
			NkString unsup;
			bool got = false;
			for (int32 i = 0; i < 2000 && !got; ++i) {
				got = w.TakeResult(&page, &zoom, cv, items, unsup);
				if (!got)
					Dodo(10);
			}
			char d[160];
			std::snprintf(d, sizeof(d), "page %d, %dx%d, %d elements de texte", page, cv.Width(),
						  cv.Height(), static_cast<int>(items.Size()));
			Check("le fil livre une page rendue", got && cv.Valid(), d);

			// La page rendue doit contenir de l'encre : une page toute blanche
			// signalerait un rendu vide, pas un fil defaillant.
			if (got && cv.Valid()) {
				const uint8 *p = cv.Pixels();
				const usize n = static_cast<usize>(cv.Width()) * static_cast<usize>(cv.Height());
				usize ink = 0;
				for (usize i = 0; i < n; ++i)
					if (p[i * 4] < 250)
						++ink;
				char d2[96];
				std::snprintf(d2, sizeof(d2), "%.2f %% de pixels peints",
							  n ? 100.0 * static_cast<double>(ink) / static_cast<double>(n) : 0.0);
				Check("la page rendue contient de l'encre", ink * 1000u > n, d2);
			}
			w.Stop();
			Check("arret du fil", true, "");
		}
	}

	std::printf("\nechecs : %d\n", gFail);
	return gFail;
}
