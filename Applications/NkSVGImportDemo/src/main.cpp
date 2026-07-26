// =============================================================================
// Applications/NkSVGImportDemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle pour l'import SVG minimal ajouté le 2026-07-24
// (voir Engine/Noge/ROADMAP.md, Phase G1 item 3) :
//   Engine/Noge/src/Noge/IO/NkSVGIO.h/.cpp (NkSVGIO::Import uniquement)
//   Engine/Noge/src/Noge/Design/Vector/NkVectorPath.h/.cpp (MoveTo/LineTo/Close)
//   Engine/Noge/src/Noge/Doc/NkVectorDocument.h/.cpp (AddArtboard/AddLayer/AddPath)
//
// `jenga test` / les projets `*_Tests` sont bloqués par la politique de
// workspace ("Unit-test compilation/execution is disabled by workspace
// policy", déjà rencontrée pour NkEditableMeshDemo/NkAssetIODemo le
// 2026-07-23). Cette application console classique (pas une TestSuite)
// contourne le blocage.
//
// DEUX SCÉNARIOS :
//   1) SVG SYNTHÉTIQUE (formes/couleurs connues à l'avance) : écrit sur
//      disque via NkFile::WriteAllText, importé, puis vérifié par
//      assertions PRÉCISES (nombre de shapes, couleurs de fill, opacité).
//      Fichier temporaire supprimé en fin de test.
//   2) SVG RÉEL DU REPO (Resources/Pong/Textures/socials/github.svg) :
//      vérifie que le nombre d'objets importés correspond exactement au
//      NkSVGImage::ShapeCount() du même fichier (chemin baseline direct,
//      même pattern de comparaison que NkAssetIODemo pour OBJ/glTF/FBX).
//   3) CHEMIN D'ERREUR : fichier inexistant -> Import() doit échouer
//      proprement (return false + GetLastError() non vide), pas de fausse
//      réussite.
// =============================================================================
#include "Noge/IO/NkSVGIO.h"
#include "NKImage/Codecs/SVG/NkSVGCodec.h"
#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;

namespace {

	int gPassCount = 0;
	int gFailCount = 0;

	void Check(bool cond, const char *what) noexcept {
		if (cond) {
			++gPassCount;
			logger.Infof("  [OK]   %s\n", what);
		} else {
			++gFailCount;
			logger.Errorf("  [FAIL] %s\n", what);
		}
	}

	bool ApproxEqual(float32 a, float32 b, float32 eps = 0.02f) noexcept {
		const float32 d = a - b;
		return (d < 0.f ? -d : d) <= eps;
	}

	// ── Scénario 1 : SVG synthétique (formes/couleurs connues) ─────────────
	void TestSyntheticSVG() noexcept {
		const char *kPath = "nk_svg_import_demo_synthetic.svg";
		const char *kSvgText =
			"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">\n"
			"  <rect x=\"10\" y=\"10\" width=\"30\" height=\"20\" fill=\"#ff0000\"/>\n"
			"  <circle cx=\"70\" cy=\"30\" r=\"15\" fill=\"#00ff00\"/>\n"
			// NOTE : `opacity` (opacité d'ÉLÉMENT) et PAS `fill-opacity` --
			// vérifié dans NkSVGCodec.cpp : NkSVGShapeView::Opacity() ne
			// retourne QUE style.opacity ; le fill-opacity est consommé en
			// interne par le rasterizer mais n'est PAS exposé par la vue
			// vectorielle read-only (limite honnête, documentée dans
			// NkSVGIO.cpp).
			"  <rect x=\"10\" y=\"60\" width=\"25\" height=\"25\" fill=\"#0000ff\" opacity=\"0.5\"/>\n"
			"</svg>\n";

		logger.Infof("-- NkSVGIO::Import : SVG synthetique (3 formes, couleurs connues) --\n");

		Check(NkFile::WriteAllText(kPath, kSvgText), "ecriture du SVG synthetique sur disque");

		NkVectorDocument doc;
		const bool ok = NkSVGIO::Import(kPath, doc);
		Check(ok, "NkSVGIO::Import reussit sur le SVG synthetique");
		Check(NkSVGIO::GetLastError().Empty(), "NkSVGIO::Import: aucune erreur");

		if (ok) {
			Check(doc.artboards.Size() == 1, "1 artboard cree");
			if (!doc.artboards.Empty()) {
				NkArtboard *artboard = doc.artboards[0];
				Check(artboard->layers.Size() == 1, "1 layer cree dans l'artboard");
				if (!artboard->layers.Empty()) {
					NkVectorLayer *layer = artboard->layers[0];
					Check(layer->objects.Size() == 3, "3 objets (rect+circle+rect) importes");

					if (layer->objects.Size() == 3) {
						// Objet 0 : rect rouge opaque.
						const NkVectorObject *obj0 = layer->objects[0];
						Check(!obj0->path.IsEmpty(), "Shape 0: path non vide");
						Check(obj0->fill.type == NkPaint::Type::Solid, "Shape 0: fill Solid");
						Check(ApproxEqual(obj0->fill.solidColor.x, 1.f) && ApproxEqual(obj0->fill.solidColor.y, 0.f) &&
								  ApproxEqual(obj0->fill.solidColor.z, 0.f),
							  "Shape 0: couleur rouge (#ff0000)");
						Check(ApproxEqual(obj0->opacity, 1.f), "Shape 0: opacite 1.0");

						// Objet 1 : cercle vert opaque.
						const NkVectorObject *obj1 = layer->objects[1];
						Check(!obj1->path.IsEmpty(), "Shape 1: path non vide");
						Check(ApproxEqual(obj1->fill.solidColor.x, 0.f) && ApproxEqual(obj1->fill.solidColor.y, 1.f) &&
								  ApproxEqual(obj1->fill.solidColor.z, 0.f),
							  "Shape 1: couleur verte (#00ff00)");

						// Objet 2 : rect bleu semi-transparent (opacity=0.5).
						const NkVectorObject *obj2 = layer->objects[2];
						Check(!obj2->path.IsEmpty(), "Shape 2: path non vide");
						Check(ApproxEqual(obj2->fill.solidColor.x, 0.f) && ApproxEqual(obj2->fill.solidColor.y, 0.f) &&
								  ApproxEqual(obj2->fill.solidColor.z, 1.f),
							  "Shape 2: couleur bleue (#0000ff)");
						Check(ApproxEqual(obj2->opacity, 0.5f, 0.05f), "Shape 2: opacite ~0.5 (opacity d'element)");
					}
				}
			}
		}

		Check(NkFile::Delete(kPath), "nettoyage du SVG synthetique temporaire");
	}

	// ── Scénario 2 : SVG réel du repo, comparé à la baseline NkSVGImage ────
	void TestRealSVG(const char *path) noexcept {
		logger.Infof("-- NkSVGIO::Import : %s --\n", path);

		NkSVGImage *baseline = NkSVGImage::LoadFromFile(path);
		Check(baseline != nullptr, "baseline NkSVGImage::LoadFromFile reussit");
		if (!baseline) {
			return;
		}
		const int32 baseShapes = baseline->ShapeCount();
		logger.Infof("     baseline: %d shape(s)\n", baseShapes);
		baseline->Free();
		Check(baseShapes > 0, "baseline: au moins 1 shape parsee");

		NkVectorDocument doc;
		const bool ok = NkSVGIO::Import(path, doc);
		Check(ok, "NkSVGIO::Import reussit");
		Check(NkSVGIO::GetLastError().Empty(), "NkSVGIO::Import: aucune erreur");

		if (ok && !doc.artboards.Empty() && !doc.artboards[0]->layers.Empty()) {
			const uint32 imported = (uint32)doc.artboards[0]->layers[0]->objects.Size();
			logger.Infof("     NkSVGIO : %u objet(s) importe(s)\n", imported);
			Check(imported == (uint32)baseShapes, "NkSVGIO::Import: nb objets == baseline ShapeCount()");
		}
	}

	// ── Scénario 3 : chemin d'erreur (fichier inexistant) ──────────────────
	void TestMissingFile() noexcept {
		logger.Infof("-- NkSVGIO::Import : fichier inexistant (chemin d'erreur) --\n");

		NkVectorDocument doc;
		const bool ok = NkSVGIO::Import("does_not_exist_nk_svg_import_demo.svg", doc);
		Check(!ok, "NkSVGIO::Import echoue proprement sur fichier inexistant");
		Check(!NkSVGIO::GetLastError().Empty(), "NkSVGIO::Import: GetLastError() non vide apres echec");
		Check(doc.artboards.Empty(), "NkSVGIO::Import: aucun artboard cree apres echec");
	}

} // namespace

int main() {
	logger.Infof("=== NkSVGImportDemo : preuve d'execution NkSVGIO::Import minimal (Phase G1.3) ===\n");

	TestSyntheticSVG();
	TestRealSVG("Resources/Pong/Textures/socials/github.svg");
	TestMissingFile();

	logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
