//
// PdfRenderProbe.cpp — banc de RENDU : rend la 1re page de chaque PDF d'un
// corpus et mesure le resultat.
//
// Comme pour l'analyseur, on ne juge pas « a l'oeil » et on n'affiche AUCUN
// contenu : on mesure des grandeurs qui distinguent une page rendue d'une page
// blanche — proportion de pixels peints, nombre de couleurs distinctes — et on
// releve les fonctionnalites rencontrees mais non rendues.
//
// Une page blanche est le mode d'echec le plus vicieux d'un lecteur PDF :
// aucune erreur, aucun plantage, et rien a l'ecran. C'est precisement ce que
// ce banc detecte.
//
// Usage :  NkPdfRenderProbe <dossier|fichier> [--dpi N] [--png <dossier>]
//
#include "NKMedia/Pdf/NkPdf.h"
#include "NKMedia/Pdf/NkPdfFont.h"
#include "NKMedia/Pdf/NkPdfRender.h"

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
		const NkString p = ents[i].FullPath.ToString();
		if (ents[i].IsDirectory)
			Collect(p, out, depth + 1);
		else if (EndsWithI(p.CStr(), ".pdf"))
			out.PushBack(p);
	}
}

// Proportion de pixels NON blancs, et nombre de teintes distinctes (echantillon
// grossier sur 4 bits par canal : on cherche « y a-t-il du contenu », pas une
// analyse colorimetrique).
static void Measure(const NkPdfCanvas &c, double *inkPct, int32 *tones) {
	const uint8 *p = c.Pixels();
	const usize n = static_cast<usize>(c.Width()) * static_cast<usize>(c.Height());
	usize ink = 0;
	static uint8 seen[4096];
	for (int32 i = 0; i < 4096; ++i)
		seen[i] = 0;
	int32 distinct = 0;
	for (usize i = 0; i < n; ++i) {
		const uint8 r = p[i * 4], g = p[i * 4 + 1], b = p[i * 4 + 2];
		if (r < 250 || g < 250 || b < 250)
			++ink;
		const int32 key = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
		if (!seen[key]) {
			seen[key] = 1;
			++distinct;
		}
	}
	*inkPct = n ? (100.0 * static_cast<double>(ink) / static_cast<double>(n)) : 0.0;
	*tones = distinct;
}

// Ecrit un PPM (format trivial, pas de dependance codec) pour inspection
// visuelle facultative. Ne sert QU'AU debogage local, jamais au corpus perso.
static void WritePpm(const NkPdfCanvas &c, const NkString &path) {
	NkVector<uint8> out;
	char hdr[64];
	const int32 n = std::snprintf(hdr, sizeof(hdr), "P6\n%d %d\n255\n", c.Width(), c.Height());
	for (int32 i = 0; i < n; ++i)
		out.PushBack(static_cast<uint8>(hdr[i]));
	const uint8 *p = c.Pixels();
	const usize np = static_cast<usize>(c.Width()) * static_cast<usize>(c.Height());
	for (usize i = 0; i < np; ++i) {
		out.PushBack(p[i * 4]);
		out.PushBack(p[i * 4 + 1]);
		out.PushBack(p[i * 4 + 2]);
	}
	NkFile::WriteAllBytes(path.CStr(), out);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		std::printf("usage : NkPdfRenderProbe <dossier|fichier.pdf> [--dpi N] [--ppm <dossier>]\n");
		return 2;
	}

	double dpi = 72.0;
	int32 pageIdx = 0;
	NkString ppmDir;
	NkVector<NkString> files;
	for (int i = 1; i < argc; ++i) {
		const NkString a(argv[i]);
		if (a == "--dpi" && i + 1 < argc) {
			dpi = 0.0;
			for (const char *s = argv[++i]; *s >= '0' && *s <= '9'; ++s)
				dpi = dpi * 10 + (*s - '0');
			if (dpi <= 0)
				dpi = 72.0;
			continue;
		}
		if (a == "--page" && i + 1 < argc) {
			pageIdx = 0;
			for (const char *s2 = argv[++i]; *s2 >= '0' && *s2 <= '9'; ++s2)
				pageIdx = pageIdx * 10 + (*s2 - '0');
			continue;
		}
		if (a == "--ppm" && i + 1 < argc) {
			ppmDir = argv[++i];
			continue;
		}
		Collect(a, files, 0);
	}
	if (files.Empty()) {
		std::printf("aucun PDF trouve.\n");
		return 2;
	}

	int32 rendus = 0, blanches = 0, echecs = 0, chiffres = 0;
	std::printf("=== Rendu de la 1re page, %d ppp ===\n\n", static_cast<int>(dpi));

	for (usize i = 0; i < files.Size(); ++i) {
		NkPdfDoc doc;
		if (doc.Open(files[i].CStr()) != NK_PDF_OK) {
			if (doc.StatusCode() == NK_PDF_ERR_ENCRYPTED)
				++chiffres;
			else
				++echecs;
			continue;
		}
		NkPdfRenderer rend;
		NkPdfCanvas cv;
		const int32 pg = (pageIdx < doc.PageCount()) ? pageIdx : 0;
		if (!rend.RenderPage(doc, pg, dpi, cv) || !cv.Valid()) {
			++echecs;
			std::printf("  [%3d] ECHEC de rendu\n", static_cast<int>(i + 1));
			continue;
		}
		double ink = 0.0;
		int32 tones = 0;
		Measure(cv, &ink, &tones);
		// Une page rendue a forcement de l'encre. Sous 0,05 % on considere
		// qu'elle est blanche — c'est le mode d'echec silencieux qu'on traque.
		const bool blanche = ink < 0.05;
		if (blanche)
			++blanches;
		else
			++rendus;

		std::printf("  [%3d] %4dx%-4d  encre %6.2f %%  teintes %4d  %s", static_cast<int>(i + 1),
					cv.Width(), cv.Height(), ink, tones, blanche ? "PAGE BLANCHE" : "");
		// Texte selectionnable : on compte les caracteres RECUPERABLES, jamais
		// leur contenu. Un ratio faible signale des polices sans /ToUnicode.
		{
			const NkVector<NkPdfRenderer::TextItem> &ti = rend.TextItems();
			int32 avecTexte = 0;
			for (usize k = 0; k < ti.Size(); ++k)
				if (!ti[k].text.Empty())
					++avecTexte;
			if (!ti.Empty())
				std::printf("  texte %d/%d", avecTexte, static_cast<int>(ti.Size()));
		}
		if (!rend.Unsupported().Empty())
			std::printf("  [non rendu : %s]", rend.Unsupported().CStr());
		std::printf("\n");

		if (blanche) {
			// Une page blanche ne se diagnostique pas sans savoir OU la chaine
			// s'est rompue : contenu vide ? operateurs non executes ? glyphes
			// sans contour ? peinture qui tombe hors du cadre ?
			const NkPdfRenderer::Stats &st = rend.GetStats();
			std::printf("        contenu %d o | %d operateurs | %d remplissages | %d contours\n"
						"        %d ops texte | %d glyphes demandes -> %d obtenus | %d images | %d formulaires\n",
						st.contentBytes, st.ops, st.fills, st.strokes, st.textOps, st.glyphsAsked,
						st.glyphsGot, st.images, st.forms);
			if (NkPdfFont *lf = rend.LastFont())
				std::printf("        police \"%s\" : programme %d o, contours sur %d des 64 premiers glyphes\n",
							lf->BaseFont().CStr(), static_cast<int>(lf->ProgramSize()),
							lf->DiagGlyphsWithShape(64));
			{
				const NkPdfRenderer::Stats &s2 = rend.GetStats();
				std::printf("        %d octets de chaine ; premiers codes demandes :", s2.strBytes);
				for (int32 k = 0; k < s2.nFirstCodes; ++k)
					std::printf(" %u", static_cast<unsigned>(s2.firstCodes[k]));
				std::printf("\n");
			}
		}

		if (!ppmDir.Empty()) {
			char nom[64];
			std::snprintf(nom, sizeof(nom), "/page_%03d.ppm", static_cast<int>(i + 1));
			WritePpm(cv, ppmDir + nom);
		}
	}

	std::printf("\n================ RECAPITULATIF ================\n");
	std::printf("  fichiers          : %d\n", static_cast<int>(files.Size()));
	std::printf("  pages rendues     : %d\n", rendus);
	std::printf("  PAGES BLANCHES    : %d\n", blanches);
	std::printf("  echecs de rendu   : %d\n", echecs);
	std::printf("  chiffres (ignores): %d\n", chiffres);
	return blanches + echecs;
}
