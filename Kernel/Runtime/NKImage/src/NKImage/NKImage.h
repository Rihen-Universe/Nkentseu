#pragma once
/**
 * @File    NKImage.h
 * @Brief   Include unique — bibliothèque NKImage complète.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - All Rights Reserved (see LICENSE)
 *
 * @Formats
 *  Lecture  : PNG, JPEG, BMP, TGA, HDR, PPM/PGM/PBM, QOI, GIF, ICO, WebP, SVG
 *  Écriture : PNG, JPEG, BMP, TGA, HDR, PPM, QOI, GIF, WebP, SVG
 *
 * @SVG pipeline complet
 *  1. NkXMLParser    — tokenizer XML + DOM complet
 *  2. NkSVGDOM       — arbre SVG avec CSS cascade, defs/use/symbol/g,
 *                       gradients linéaires + radiaux, clipPath
 *  3. NkSVGDOMBuilder— construction depuis XML ou fichier
 *  4. NkSVGRenderer  — rasterisation RGBA32 avec AA, gradients, dash
 *
 * @Usage SVG
 * @code
 *   #include "NKImage/NKImage.h"
 *   using namespace nkentseu;
 *
 *   // Rastérisation d'un SVG : NkImage est un TYPE VALEUR, rien à libérer.
 *   NkImage img = NkSVGCodec::DecodeFromFile("logo.svg", 512, 512);
 *   if (!img.IsValid())
 *       return false;              // échec = image invalide, plus de nullptr
 *   // ... img.Pixels(), img.Width(), img.Height() ...
 *   // img libère ses pixels toute seule en sortant de la portée.
 * @endcode
 *
 * @note Pour garder la représentation VECTORIELLE et la rastériser plusieurs
 *       fois à des tailles différentes, voir `NkSVGImage` dans
 *       `NKImage/Codecs/SVG/NkSVGCodec.h`. ⚠️ `NkSVGImage` est un type
 *       DIFFÉRENT de `NkImage` : il reste un pointeur possédé et garde son
 *       propre `Free()`. Seule l'image rastérisée est une valeur.
 */

// Core
#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/SVG/NkXMLParser.h"

// Codecs image
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKImage/Codecs/BMP/NkBMPCodec.h"
#include "NKImage/Codecs/TGA/NkTGACodec.h"
#include "NKImage/Codecs/HDR/NkHDRCodec.h"
#include "NKImage/Codecs/EXR/NkEXRCodec.h"
#include "NKImage/Codecs/PPM/NkPPMCodec.h"
#include "NKImage/Codecs/QOI/NkQOICodec.h"
#include "NKImage/Codecs/GIF/NkGIFCodec.h"
#include "NKImage/Codecs/ICO/NkICOCodec.h"
#include "NKImage/Codecs/WEBP/NkWebPCodec.h"

// SVG : NkSVGDOM/Renderer ont ete remplaces par NkSVGCodec (codec unique).
// Voir [[nkimage_svg_support]] en memoire.
#include "NKImage/Codecs/SVG/NkSVGCodec.h"