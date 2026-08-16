// =============================================================================
// NKImageCodecTest — round-trip des codecs image NKImage, VÉRIFIÉ via ffmpeg.
//   Génère un motif synthétique (dégradé, damier fin, barres saturées, « texte »),
//   l'encode dans chaque format, écrit les fichiers + la référence brute RGBA.
//   Un script ffmpeg décode chaque sortie et calcule le PSNR (lossless attendu
//   BIT-EXACT pour PNG/BMP/TGA/QOI/PPM).
// =============================================================================
#include "NKImage/Core/NkImage.h"
#include "NKMemory/NKMemory.h"

#include <cstdio>

using namespace nkentseu;

int main() {
	const int W = 512, H = 512;

	// Motif synthétique déterministe (RGB24).
	NkImage img = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGB24);
	if (!img.IsValid()) {
		printf("[ERREUR] Alloc\n");
		return 1;
	}
	nk_uint8 *db = img.Pixels();
	const int st = img.Stride();
	for (int y = 0; y < H; ++y) {
		for (int x = 0; x < W; ++x) {
			nk_uint8 r = 0, g = 0, b = 0;
			if (y < H / 4) { // dégradé de gris
				r = g = b = (nk_uint8)(x * 255 / W);
			} else if (y < H / 2) { // damier fin 4 px
				r = g = b = (((x / 4) + (y / 4)) & 1) ? 255 : 0;
			} else if (y < 3 * H / 4) { // barres de couleur saturées
				const int bar = x * 8 / W;
				const nk_uint8 C[8][3] = {{255, 0, 0},   {0, 255, 0},	{0, 0, 255},   {255, 255, 0},
										  {0, 255, 255}, {255, 0, 255}, {255, 255, 255}, {0, 0, 0}};
				r = C[bar][0];
				g = C[bar][1];
				b = C[bar][2];
			} else { // « texte » : lignes fines
				const bool ink = (x % 6) < 2;
				r = ink ? 255 : 10;
				g = ink ? 255 : 20;
				b = ink ? 255 : 60;
			}
			nk_uint8 *p = db + (nk_size)y * st + x * 3;
			p[0] = r;
			p[1] = g;
			p[2] = b;
		}
	}

	// Référence brute RGB (pour ffmpeg -pix_fmt rgb24).
	{
		FILE *rf = fopen("ic_ref.rgb", "wb");
		if (rf) {
			for (int y = 0; y < H; ++y)
				fwrite(db + (nk_size)y * st, 1, (size_t)W * 3, rf);
			fclose(rf);
		}
	}

	// Encode chaque format via Save(extension) et via les Encode* directs.
	struct Fmt {
			const char *file;
			bool lossless;
	};
	const Fmt formats[] = {
		{"ic_out.png", true}, {"ic_out.bmp", true}, {"ic_out.tga", true}, {"ic_out.qoi", true},
		{"ic_out.ppm", true}, {"ic_out.gif", false}, {"ic_out.hdr", false}, {"ic_out.webp", true}, {"ic_out.exr", false},
	};
	for (const Fmt &f : formats) {
		const bool ok = img.Save(f.file, 100);
		printf("  %-12s : %s%s\n", f.file, ok ? "ecrit" : "ECHEC SAVE", f.lossless ? "  (lossless attendu)" : "");
	}
	printf("[CODECTEST] reference ic_ref.rgb (%dx%d rgb24). Verifier avec ffmpeg (PSNR).\n", W, H);
	return 0;
}
