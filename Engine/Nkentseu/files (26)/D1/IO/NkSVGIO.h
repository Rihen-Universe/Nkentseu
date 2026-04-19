#pragma once
// =============================================================================
// Nkentseu/Design/IO/NkSVGIO.h
// =============================================================================
// Import/Export SVG 1.1 pour les applications de design vectoriel.
//
// IMPORT (SVG → NkVectorDocument) :
//   • Formes : rect, circle, ellipse, line, polyline, polygon, path
//   • Transforms : translate, rotate, scale, matrix, skewX/Y
//   • Styles : fill, stroke, stroke-width, opacity, stroke-dasharray
//   • Dégradés : linearGradient, radialGradient
//   • Texte : text, tspan (basique)
//   • Groupes : g, use (instances), defs, symbol
//   • Clip-paths et masques
//
// EXPORT (NkVectorDocument → SVG) :
//   • Tout ce qui a été importé
//   • + NkVectorPath avec toutes les commandes (M, L, C, Q, A, Z)
//   • Optimisation : merge des paths, simplification des transforms
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKContainers/String/NkString.h"
#include "Nkentseu/Design/Vector/NkVectorDocument.h"

namespace nkentseu {

    struct NkSVGImportOptions {
        bool   importGroups      = true;
        bool   importDefs        = true;
        bool   importText        = true;
        bool   flattenGroups     = false;  ///< Fusionne tous les groupes en une couche
        float32 scaleFactor      = 1.f;   ///< Facteur d'échelle global
        bool   preserveViewBox   = true;
        bool   importClipPaths   = true;
    };

    struct NkSVGExportOptions {
        bool   prettyPrint       = true;   ///< Indentation lisible
        bool   minify            = false;  ///< Minification (taille minimale)
        bool   embedFonts        = false;  ///< Intègre les polices en base64
        bool   useAbsCoords      = false;  ///< Utilise coordonnées absolues
        bool   optimizePaths     = true;   ///< Simplifie les paths
        float32 precision        = 2.f;    ///< Décimales de précision
        bool   exportMetadata    = true;
    };

    class NkSVGIO {
    public:
        /**
         * @brief Importe un fichier SVG vers un NkVectorDocument.
         */
        [[nodiscard]] static bool Import(
            const char* path,
            NkVectorDocument& doc,
            const NkSVGImportOptions& opts = {}) noexcept;

        /**
         * @brief Importe depuis une chaîne SVG en mémoire.
         */
        [[nodiscard]] static bool ImportFromString(
            const char* svgText,
            NkVectorDocument& doc,
            const NkSVGImportOptions& opts = {}) noexcept;

        /**
         * @brief Exporte un NkVectorDocument vers un fichier SVG.
         * @param artboardIdx Artboard à exporter (0 = premier).
         */
        [[nodiscard]] static bool Export(
            const NkVectorDocument& doc,
            const char* path,
            uint32 artboardIdx = 0,
            const NkSVGExportOptions& opts = {}) noexcept;

        /**
         * @brief Exporte un NkVectorPath unique vers SVG (utilité).
         */
        [[nodiscard]] static NkString PathToSVG(
            const NkVectorPath& path,
            uint32 precision = 2) noexcept;

        /**
         * @brief Parse un attribut "d" de path SVG vers NkVectorPath.
         */
        [[nodiscard]] static NkVectorPath SVGToPath(const char* d) noexcept;

        [[nodiscard]] static const NkString& GetLastError() noexcept;
    };

} // namespace nkentseu
