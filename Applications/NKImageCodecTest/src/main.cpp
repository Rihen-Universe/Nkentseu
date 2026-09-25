// =============================================================================
// NKImageCodecTest — round-trip des codecs image NKImage, VÉRIFIÉ via ffmpeg.
//   Génère un motif synthétique (dégradé, damier fin, barres saturées, « texte »),
//   l'encode dans chaque format, écrit les fichiers + la référence brute RGBA.
//   Un script ffmpeg décode chaque sortie et calcule le PSNR (lossless attendu
//   BIT-EXACT pour PNG/BMP/TGA/QOI/PPM).
// =============================================================================
#include "NKImage/Codecs/WEBP/NkWebPCodec.h" // (25/09) webp/lossy-refuse : on interroge LE CODEC
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
	// ═══ webp/lossy-refuse — UN WEBP LOSSY EST REFUSE, PAS INVENTE ═══════════
	//
	// 🔴 CE QUE CE CRITERE EMPECHE DE REVENIR (mesure du 25/09) : `DecodeVP8`
	//    rendait une image VALIDE entierement remplie de gris 128. Un WebP lossy ne
	//    ratait donc pas, il REUSSISSAIT avec un contenu invente -- la bonne taille,
	//    aucun refus, et un rectangle gris a la place de la photo.
	//    *Un repli qui reste plausible est pire qu'un refus.*
	//
	// ⚠️ A RETOURNER LE JOUR OU LE LOSSY SERA DECODE : il exige aujourd'hui un
	//    REFUS ; quand `DecodeVP8` rendra de vrais pixels, il devra exiger une IMAGE
	//    JUSTE. La condition est ecrite ici ET dans `NkWebPCodec.cpp`, aux deux
	//    bouts, pour qu'on ne l'oublie pas d'un cote.
	{
		// Un RIFF/WEBP minimal portant UN chunk `VP8 ` : exactement la forme du
		// fichier de Rodolf (ni VP8L, ni alpha, ni animation). Fabrique ici plutot
		// que lu sur le disque -- un banc qui depend d'un fichier du poste de
		// quelqu'un n'est pas un banc.
		nkentseu::uint8 buf[32] = {};
		buf[0] = 'R'; buf[1] = 'I'; buf[2] = 'F'; buf[3] = 'F';
		buf[4] = 24;
		buf[8] = 'W'; buf[9] = 'E'; buf[10] = 'B'; buf[11] = 'P';
		buf[12] = 'V'; buf[13] = 'P'; buf[14] = '8'; buf[15] = ' ';
		buf[16] = 12;
		// En-tete de key frame VP8 : bit 0 a 0, signature 9D 01 2A, puis 16 x 16.
		// C'est ce que l'ancienne version lisait pour fabriquer son gris.
		buf[23] = 0x9D; buf[24] = 0x01; buf[25] = 0x2A;
		buf[26] = 16; buf[28] = 16;
		// ⚠️ ON APPELLE LE CODEC DIRECTEMENT, ET LA PREMIERE VERSION NE LE
		//    FAISAIT PAS. Elle passait par `NkImage::LoadFromMemory` -- et elle est
		//    restee VERTE sous mutation : avec le gris 128 REMIS, le critere ne
		//    rougissait pas. Le refus venait d'ailleurs (le repartiteur generique),
		//    pas de `DecodeVP8`. *Un critere qui ne refute pas sa propre mutation
		//    est une decoration.* On interroge donc le codec lui-meme.
		const nkentseu::NkImage lossy = nkentseu::NkWebPCodec::Decode(buf, sizeof(buf));
		const bool refuse = !lossy.IsValid();
		printf("  webp/lossy-refuse : %s (un WebP lossy doit etre REFUSE tant que "
			   "DecodeVP8 n'existe pas ; il rendait un gris 128 de la bonne taille)\n",
			   refuse ? "VERT" : "ROUGE - le codec INVENTE une image");
		if (!refuse)
			return 1;
	}

	printf("[CODECTEST] reference ic_ref.rgb (%dx%d rgb24). Verifier avec ffmpeg (PSNR).\n", W, H);
	return 0;
}
