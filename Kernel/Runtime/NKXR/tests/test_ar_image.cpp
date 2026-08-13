// =============================================================================
// tests/test_ar_image.cpp — passer le détecteur sur une VRAIE image.
//
// Pourquoi cet outil existe : les images de synthèse passaient toutes — nette,
// floue, avec halo — pendant que la planche réelle échouait à l'écran. Une
// simulation qui ne reproduit pas le défaut ne prouve rien ; il faut donc
// pouvoir donner au détecteur l'image EXACTE qui lui résiste, et regarder à
// quelle étape elle est rejetée.
//
// Usage : test_ar_image <image.png> [gridBits] [minEdgePixels]
// =============================================================================
#include "NKXR/AR/NkArMarker.h"
#include "NKImage/NkImage.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"

using namespace nkentseu;
using namespace nkentseu::xr;

int main(int argc, char **argv) {
	if (argc < 2) {
		logger.Error("usage: test_ar_image <image.png> [gridBits] [minEdgePixels]\n");
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
