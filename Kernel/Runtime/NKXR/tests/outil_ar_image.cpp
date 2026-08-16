// =============================================================================
// tests/outil_ar_image.cpp — passer le détecteur sur une VRAIE image.
//
// ⚠️ CE N'EST PAS UN TEST. C'est un INSTRUMENT, et il ne compte dans aucun
// total d'auto-tests. Il s'appelait `test_ar_image.cpp` jusqu'au 2026-08-17 —
// dans un dossier `tests/`, construit par `build_tests.sh`, il avait tout d'un
// test sans en être un.
//
// LA RAISON, ET ELLE EST PLUS FORTE QUE « il attend un argument » :
//     ce programme N'A AUCUN VERDICT. Il journalise ce qu'il trouve et rend 0,
//     qu'il détecte cinq marqueurs ou zéro. Il ne peut donc pas échouer.
// L'ajouter au décompte aurait créé un contrôle incapable de tomber — c'est-à-
// dire du repos acheté, pas de l'information. Tant qu'il n'a pas d'attendu, il
// reste dehors, et c'est écrit ici plutôt que déduit de son nom.
//
// Pourquoi cet instrument existe : les images de synthèse passaient toutes —
// nette, floue, avec halo — pendant que la planche réelle échouait à l'écran.
// Une simulation qui ne reproduit pas le défaut ne prouve rien ; il faut donc
// pouvoir donner au détecteur l'image EXACTE qui lui résiste, et regarder à
// quelle étape elle est rejetée.
//
// Usage : outil_ar_image <image.png> [gridBits] [minEdgePixels]
//
// CODES DE SORTIE — aucun ne signifie « détection réussie » :
//     0  l'analyse est allée au bout. Le nombre de marqueurs trouvés est dans
//        le journal, et il peut parfaitement être zéro. 0 = « j'ai tourné »,
//        PAS « j'ai réussi ».
//     1  aucun chemin d'image fourni (emploi). Ce n'est pas un échec d'analyse.
//     2  l'image n'a pas pu être lue.
//
// Pour qu'il rejoigne un jour le décompte, il lui faudrait un attendu : une
// image versionnée ET le nombre de marqueurs qu'on exige d'y trouver, avec
// échec si le compte diffère. Ni l'un ni l'autre n'existe aujourd'hui.
// =============================================================================
#include "NKXR/AR/NkArMarker.h"
#include "NKImage/NkImage.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"

using namespace nkentseu;
using namespace nkentseu::xr;

int main(int argc, char **argv) {
	if (argc < 2) {
		// Dit ce qu'il EST, pas seulement comment on l'appelle : sans ça, un
		// code 1 dans une sortie de banc se lit comme un test qui tombe.
		logger.Error("outil_ar_image : INSTRUMENT de diagnostic, pas un test — il ne compte dans "
					 "aucun total d'auto-tests (il n'a aucun verdict).\n");
		logger.Error("usage: outil_ar_image <image.png> [gridBits] [minEdgePixels]\n");
		return 1;
	}
	NkImage img;
	if (!img.LoadFromFile(argv[1])) {
		logger.Errorf("Impossible de lire %s\n", argv[1]);
		return 2;
	}
	const uint32 w = img.Width();
	const uint32 h = img.Height();
	logger.Infof("Image %ux%u, %u canaux\n", w, h, img.Channels());

	auto &allocator = memory::NkGetDefaultAllocator();
	uint8 *gray = static_cast<uint8 *>(allocator.Allocate(nk_size(w) * h, 1));
	const uint8 *px = img.Pixels();
	const uint32 ch = img.Channels();
	for (uint32 i = 0; i < w * h; ++i) {
		const uint32 r = px[i * ch + 0];
		const uint32 g = (ch > 1) ? px[i * ch + 1] : r;
		const uint32 b = (ch > 2) ? px[i * ch + 2] : r;
		gray[i] = uint8((r * 77u + g * 151u + b * 28u) >> 8);
	}

	NkArDetectorConfig cfg;
	cfg.gridBits = (argc > 2) ? uint32(atoi(argv[2])) : 4u;
	cfg.minEdgePixels = (argc > 3) ? uint32(atoi(argv[3])) : 20u;
	cfg.debugCounters = true;

	NkVector<NkArDetection> found;
	const uint32 n = NkArDetectMarkers(gray, w, h, cfg, found);
	logger.Infof("=== %u marqueur(s) detecte(s) ===\n", n);
	for (nk_size i = 0; i < found.Size(); ++i) {
		logger.Infof("  id %d, cote moyen %.1f px, coin0 (%.0f,%.0f)\n", found[i].id, found[i].edgeLength,
					 found[i].corners[0].x, found[i].corners[0].y);
	}
	allocator.Deallocate(gray);
	return 0;
}
