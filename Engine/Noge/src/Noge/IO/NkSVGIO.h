#pragma once
// =============================================================================
// Nkentseu/IO/NkSVGIO.h
// =============================================================================
// Import/Export SVG 1.1 pour les applications de design vectoriel.
//
// [AUDIT 2026-07-23] NkSVGIO n'était PAS adapté : l'API cible
// `NkVectorDocument` dépendait de `NkVectorPath`/`NkLayerStack`/
// `NkColorManager`, 4 fichiers sans AUCUN `.cpp` (specs à 0 % d'implém.), et
// les includes eux-mêmes étaient cassés (chemins qui ne résolvaient jamais).
//
// [INCRÉMENT 2026-07-24 — IMPORT MINIMAL AJOUTÉ A POSTERIORI] : plutôt que de
// construire tout le sous-système Design/Doc (Illustrator-like complet,
// hors scope), seul le SOUS-ENSEMBLE strictement nécessaire à un import SVG
// basique a été implémenté :
//   - Includes cassés de NkVectorDocument.h/NkLayerStack.h/NkRasterCanvas.h
//     corrigés (voir commentaires [FIX 2026-07-24] dans ces fichiers), plus
//     3 types géométriques manquants (NkAABB2f/NkIRect/NkIVec2, jamais
//     déclarés nulle part) aliasés sur des types NKMath réels dans le
//     nouveau Noge/Design/NkDesignGeomTypes.h.
//   - `NkVectorPath::MoveTo/LineTo/Close` implémentés (push simple dans
//     mCmds — PAS de tessellation, PAS d'opérations booléennes, PAS de
//     ToSVGPath/FromSVGPath : ces méthodes restent déclarées sans corps,
//     stubs honnêtes non appelés par ce chemin).
//   - `NkVectorDocument::AddArtboard`, `NkArtboard::AddLayer`,
//     `NkVectorLayer::AddPath` implémentés (construction minimale de la
//     hiérarchie document->artboard->calque->objet). `SaveToFile`/
//     `LoadFromFile`/`ExportSVG`/`ExportPDF`/`ExportPNG`/guides/grilles/
//     symboles/undo réel/presse-papier : NON implémentés, restent déclarés
//     sans corps (hors scope, non appelés par Import()).
//   - `NkSVGIO::Import` implémenté ci-dessous (NkSVGIO.cpp, nouveau) :
//     parse via `NkSVGImage::LoadFromFile` (codec réel, raster + vue
//     vectorielle read-only), puis pour chaque `NkSVGShapeView` itère les
//     contours (`ContourXs`/`ContourYs`, déjà aplatis en polylignes côté
//     codec — PAS de vraies courbes de Bézier préservées) via
//     MoveTo/LineTo/Close, applique `NkPaint::Solid(FillColor)` et ajoute au
//     document via AddArtboard->AddLayer->AddPath. `ImportFromString`,
//     `Export`, `PathToSVG`, `SVGToPath` restent déclarés sans corps (hors
//     scope de cet incrément, non implémentés) : pas de dégradés/texte/
//     groupes/clip-paths (non exposés par NkSVGShapeView), pas d'export.
//     Voir Engine/Noge/ROADMAP.md pour le suivi.
//
// IMPORT (SVG → NkVectorDocument) — AMBITION ORIGINALE DE LA SPEC, DONT SEUL
// UN SOUS-ENSEMBLE EST RÉALISÉ (formes aplaties en polylignes + fill uni) :
//   • Formes : rect, circle, ellipse, line, polyline, polygon, path
//   • Transforms : translate, rotate, scale, matrix, skewX/Y
//   • Styles : fill, stroke, stroke-width, opacity, stroke-dasharray
//   • Dégradés : linearGradient, radialGradient
//   • Texte : text, tspan (basique)
//   • Groupes : g, use (instances), defs, symbol
//   • Clip-paths et masques
//
// EXPORT (NkVectorDocument → SVG) — NON IMPLÉMENTÉ DANS CET INCRÉMENT :
//   • Tout ce qui a été importé
//   • + NkVectorPath avec toutes les commandes (M, L, C, Q, A, Z)
//   • Optimisation : merge des paths, simplification des transforms
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKContainers/String/NkString.h"
// [FIX 2026-07-24] Include cassé : préfixe "Noge/" manquant (includedirs
// Noge.jenga = "src", fichier réel sous "src/Noge/Doc/...").
#include "Noge/Doc/NkVectorDocument.h"

namespace nkentseu {

	struct NkSVGImportOptions {
			// [2026-07-24] Import() n'honore réellement QUE `scaleFactor` (post-
			// traitement trivial sur les contours déjà parsés). Les 5 autres
			// options sont des no-ops honnêtes documentés dans NkSVGIO.cpp :
			// `NkSVGShapeView` (codec réel) n'expose ni groupes, ni defs, ni
			// texte, ni clip-paths -- il n'y a rien à (ne pas) importer pour ces
			// options, elles ne peuvent donc pas changer le résultat.
			bool importGroups = true;
			bool importDefs = true;
			bool importText = true;
			bool flattenGroups = false; ///< Fusionne tous les groupes en une couche
			float32 scaleFactor = 1.f;	///< Facteur d'échelle global -- seule option réellement honorée
			bool preserveViewBox = true;
			bool importClipPaths = true;
	};

	struct NkSVGExportOptions {
			bool prettyPrint = true;   ///< Indentation lisible
			bool minify = false;	   ///< Minification (taille minimale)
			bool embedFonts = false;   ///< Intègre les polices en base64
			bool useAbsCoords = false; ///< Utilise coordonnées absolues
			bool optimizePaths = true; ///< Simplifie les paths
			float32 precision = 2.f;   ///< Décimales de précision
			bool exportMetadata = true;
	};

	class NkSVGIO {
		public:
			/**
			 * @brief Importe un fichier SVG vers un NkVectorDocument.
			 * @note IMPLÉMENTÉ (2026-07-24, NkSVGIO.cpp) : voir le commentaire
			 *       d'en-tête du fichier pour le détail (contours polylignes,
			 *       fill uni, pas de dégradés/texte/groupes/clip-paths).
			 */
			[[nodiscard]] static bool Import(const char *path, NkVectorDocument &doc,
											 const NkSVGImportOptions &opts = {}) noexcept;

			/**
			 * @brief Importe depuis une chaîne SVG en mémoire.
			 * @note NON IMPLÉMENTÉ dans cet incrément (hors scope : seul
			 *       Import(path) a été demandé/réalisé). Déclaration
			 *       conservée pour compatibilité amont, aucun corps -- ne pas
			 *       appeler (pas de définition -> erreur de lien).
			 */
			[[nodiscard]] static bool ImportFromString(const char *svgText, NkVectorDocument &doc,
													   const NkSVGImportOptions &opts = {}) noexcept;

			/**
			 * @brief Exporte un NkVectorDocument vers un fichier SVG.
			 * @param artboardIdx Artboard à exporter (0 = premier).
			 * @note NON IMPLÉMENTÉ dans cet incrément (Export SVG hors scope,
			 *       cf. NkOBJIO::Export/NkGLTFIO Export non plus implémentés
			 *       le même jour). Déclaration conservée, aucun corps.
			 */
			[[nodiscard]] static bool Export(const NkVectorDocument &doc, const char *path, uint32 artboardIdx = 0,
											 const NkSVGExportOptions &opts = {}) noexcept;

			/**
			 * @brief Exporte un NkVectorPath unique vers SVG (utilité).
			 * @note NON IMPLÉMENTÉ dans cet incrément (sérialisation SVG hors
			 *       scope, cf. NkVectorPath::ToSVGPath non plus implémentée).
			 */
			[[nodiscard]] static NkString PathToSVG(const NkVectorPath &path, uint32 precision = 2) noexcept;

			/**
			 * @brief Parse un attribut "d" de path SVG vers NkVectorPath.
			 * @note NON IMPLÉMENTÉ dans cet incrément (hors scope : Import()
			 *       ne passe pas par cette fonction -- il consomme les
			 *       contours déjà parsés/aplatis par NkSVGCodec).
			 */
			[[nodiscard]] static NkVectorPath SVGToPath(const char *d) noexcept;

			/// @note IMPLÉMENTÉ (2026-07-24) : reflète la dernière erreur de Import().
			[[nodiscard]] static const NkString &GetLastError() noexcept;
	};

} // namespace nkentseu
