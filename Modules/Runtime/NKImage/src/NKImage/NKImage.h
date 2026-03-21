#pragma once
/**
 * @File    NKImage.h
 * @Brief   Include unique — bibliothèque NKImage complète.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
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
 *   // Rendu rapide
 *   NkImage* img = NkSVGRenderer::RenderFromFile("logo.svg", 512, 512);
 *
 *   // Accès au DOM
 *   NkSVGDOM dom; dom.Init();
 *   NkSVGDOMBuilder::BuildFromFile("logo.svg", dom, 512, 512);
 *   NkXMLNode* node = dom.xmlDoc.GetElementById("my-path");
 *   NkImage* img2 = NkSVGRenderer::Render(dom);
 *   dom.Destroy(); img->Free(); img2->Free();
 * @endcode
 */

// Core
#include "NKImage/Core/NkImage.h"
#include "NKImage/Core/NkXMLParser.h"

// Codecs image
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKImage/Codecs/BMP/NkBMPCodec.h"
#include "NKImage/Codecs/TGA/NkTGACodec.h"
#include "NKImage/Codecs/HDR/NkHDRCodec.h"
#include "NKImage/Codecs/PPM/NkPPMCodec.h"
#include "NKImage/Codecs/QOI/NkQOICodec.h"
#include "NKImage/Codecs/GIF/NkGIFCodec.h"
#include "NKImage/Codecs/ICO/NkICOCodec.h"
#include "NKImage/Codecs/WEBP/NkWebPCodec.h"

#include "NKImage/Codecs/SVG/NkSVGDOM.h"
#include "NKImage/Codecs/SVG/NkSVGRenderer.h"