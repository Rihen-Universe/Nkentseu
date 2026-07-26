// =============================================================================
// Nkentseu/IO/NkSVGIO.cpp
// =============================================================================
// [AJOUT 2026-07-24] Implémentation MINIMALE de NkSVGIO::Import, pont entre
// le codec SVG réel (NkSVGCodec.h -- NkSVGImage/NkSVGShapeView) et le
// sous-ensemble minimal de NkVectorDocument (voir NkVectorPath.cpp /
// NkVectorDocument.cpp, ajoutés le même jour). Voir NkSVGIO.h pour le détail
// complet des limites honnêtes documentées.
//
// LIMITES (rappel, cf. NkSVGIO.h) :
//   - Contours DÉJÀ aplatis en polylignes par le codec (ContourXs/ContourYs)
//     -- pas de vraies courbes de Bézier préservées (le codec les a déjà
//     subdivisées lors du parsing).
//   - Un seul NkPaint::Solid(FillColor) par shape -- pas de dégradés (le
//     codec en calcule un fill résolu par shape, pas la définition du
//     gradient elle-même côté NkSVGShapeView).
//   - Pas de texte/groupes/clip-paths : NkSVGShapeView n'expose qu'une liste
//     plate de shapes, sans hiérarchie de groupes ni éléments <text>.
//   - `fill-opacity` NON reprise (vérifié dans NkSVGCodec.cpp) :
//     NkSVGShapeView::Opacity() ne retourne QUE style.opacity (opacité
//     d'élément) ; le fill-opacity est consommé en interne par le rasterizer
//     du codec mais n'est pas exposé par la vue vectorielle read-only, et
//     FillColor() retourne l'alpha brut du fill sans pré-multiplication.
//   - Seule `scaleFactor` de NkSVGImportOptions est honorée (post-traitement
//     trivial sur les contours déjà parsés). importGroups/importDefs/
//     importText/importClipPaths/flattenGroups sont des no-ops honnêtes :
//     rien à (ne pas) importer pour ces catégories côté NkSVGShapeView.
//   - ImportFromString/Export/PathToSVG/SVGToPath : NON implémentés (hors
//     scope), restent déclarés sans corps dans NkSVGIO.h.
// =============================================================================
#include "NkSVGIO.h"
#include "NKImage/Codecs/SVG/NkSVGCodec.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

	namespace {
		NkString &LastErrorStorage() noexcept {
			static NkString sLastError;
			return sLastError;
		}
	} // namespace

	bool NkSVGIO::Import(const char *path, NkVectorDocument &doc, const NkSVGImportOptions &opts) noexcept {
		LastErrorStorage().Clear();

		if (!path || !path[0]) {
			LastErrorStorage() = "NkSVGIO::Import: chemin vide";
			logger.Errorf("[NkSVGIO] Import: chemin vide\n");
			return false;
		}

		if (!opts.importGroups || !opts.importDefs || !opts.importText || !opts.importClipPaths ||
			opts.flattenGroups) {
			logger.Warnf("[NkSVGIO] Import('%s'): importGroups/importDefs/importText/importClipPaths/"
						 "flattenGroups ignorés -- NkSVGShapeView (codec réel) n'expose ni groupes, ni defs, "
						 "ni texte, ni clip-paths\n",
						 path);
		}

		NkSVGImage *svg = NkSVGImage::LoadFromFile(path);
		if (!svg) {
			LastErrorStorage() = "NkSVGIO::Import: NkSVGImage::LoadFromFile a echoue (fichier introuvable ou XML "
								 "invalide)";
			logger.Errorf("[NkSVGIO] Import('%s'): NkSVGImage::LoadFromFile a echoue\n", path);
			return false;
		}

		const int32 shapeCount = svg->ShapeCount();
		if (shapeCount <= 0) {
			LastErrorStorage() = "NkSVGIO::Import: 0 shape parsee (SVG vide ou sans forme supportee)";
			logger.Warnf("[NkSVGIO] Import('%s'): 0 shape parsee\n", path);
			svg->Free();
			return false;
		}

		NkArtboard &artboard =
			doc.AddArtboard(path, static_cast<float32>(svg->NaturalWidth()), static_cast<float32>(svg->NaturalHeight()));
		NkVectorLayer &layer = artboard.AddLayer("SVG Import");

		const float32 scale = opts.scaleFactor;
		int32 importedShapes = 0;

		for (int32 i = 0; i < shapeCount; ++i) {
			const NkSVGShapeView shape = svg->GetShape(i);
			if (!shape.IsValid()) {
				continue;
			}

			NkVectorPath vpath;
			const int32 contourCount = shape.ContourCount();
			for (int32 c = 0; c < contourCount; ++c) {
				const int32 ptCount = shape.ContourPointCount(c);
				if (ptCount <= 0) {
					continue;
				}
				const float32 *xs = shape.ContourXs(c);
				const float32 *ys = shape.ContourYs(c);
				if (!xs || !ys) {
					continue;
				}

				vpath.MoveTo(xs[0] * scale, ys[0] * scale);
				for (int32 p = 1; p < ptCount; ++p) {
					vpath.LineTo(xs[p] * scale, ys[p] * scale);
				}
				vpath.Close();
			}

			const NkSVGColor svgColor = shape.FillColor();
			const NkVec4f fillColor = {svgColor.r / 255.f, svgColor.g / 255.f, svgColor.b / 255.f,
									   svgColor.none ? 0.f : svgColor.a / 255.f};

			const NkString objName = NkString::Format("Shape %d", i);
			NkVectorObject &obj = layer.AddPath(vpath, NkPaint::Solid(fillColor), objName.CStr());
			obj.opacity = shape.Opacity();
			obj.fillRule = shape.FillEvenOdd() ? NkFillRule::EvenOdd : NkFillRule::NonZero;

			++importedShapes;
		}

		svg->Free();

		if (importedShapes == 0) {
			LastErrorStorage() = "NkSVGIO::Import: aucune shape valide (toutes IsValid()==false)";
			logger.Warnf("[NkSVGIO] Import('%s'): 0 shape valide sur %d\n", path, shapeCount);
			return false;
		}

		logger.Infof("[NkSVGIO] Import('%s'): %d/%d shape(s) importee(s)\n", path, importedShapes, shapeCount);
		return true;
	}

	const NkString &NkSVGIO::GetLastError() noexcept {
		return LastErrorStorage();
	}

} // namespace nkentseu
