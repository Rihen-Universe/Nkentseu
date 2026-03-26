#pragma once
/**
 * @File    NKFont.h
 * @Brief   Include unique — bibliothèque NKFont complète.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Formats supportés
 *  TTF/OTF  — TrueType + OpenType (tables head/hhea/maxp/hmtx/loca/cmap/glyf/kern/OS2)
 *  CFF/OTF  — PostScript/CFF avec charstrings Type 2 (OTF moderne)
 *  Type 1   — PFB + PFA, eexec déchiffrement, seac (accents composés), blend MM
 *  BDF      — Bitmap Distribution Format (X11)
 *  PCF      — Portable Compiled Format (X11 binaire)
 *  WOFF1    — Web Open Font Format v1 (zlib compressé)
 *  WOFF2    — Web Open Font Format v2 (Brotli from scratch, RFC 7932)
 *  TTC      — TrueType Collection (multiples faces dans un seul fichier)
 *
 * @Fonctionnalités
 *  Hinting  — VM TrueType complète (fpgm/prep/glyph, MDAP/MIAP/MDRP/MIRP/IUP/DELTA)
 *  AA       — Scanline 4×4 super-sampling (16 sous-pixels, qualité FreeType niveau 2)
 *  GSUB     — Ligatures ff/fi/fl, substitutions contextuelles (types 1/3/4/6)
 *  GPOS     — Kerning OpenType PairPos format 1 et 2 (classes de glyphes)
 *  Kern     — Table kern format 0 (TrueType classique, fallback)
 *  Bidi     — UAX#9 complet (arabe, hébreu, bidirectionnel)
 *  Unicode  — UTF-8/UTF-16, cmap formats 4 et 12 (BMP + SMP complet)
 *  Cache    — Atlas texture LRU, packing shelf algorithm
 *
 * @Usage basique
 * @code
 *   #include "NKFont/NKFont.h"
 *   using namespace nkentseu;
 *
 *   NkFontLibrary lib;
 *   lib.Init();
 *
 *   // Charger un TTF/OTF/WOFF/WOFF2
 *   NkFontFace* face = lib.LoadFont(ttfData, ttfSize, 16);
 *
 *   // Glyphe individuel
 *   NkGlyph glyph;
 *   face->GetGlyph(U'A', glyph);
 *   // glyph.bitmap : pixels Gray8 · glyph.uv : atlas coords
 *
 *   // Run de texte avec shaping
 *   NkGlyphRun run;
 *   face->ShapeText("Hello, fi!", run); // "fi" → ligature si la fonte la supporte
 *
 *   lib.Destroy();
 * @endcode
 *
 * @Usage TTC (collection)
 * @code
 *   int32 count = NkFontLibrary::GetTTCFaceCount(data, size);
 *   for (int32 i = 0; i < count; ++i) {
 *       NkFontFace* face = lib.LoadTTC(data, size, i, 16);
 *   }
 * @endcode
 *
 * @Usage WOFF2
 * @code
 *   // Identique à LoadFont — la détection est automatique
 *   NkFontFace* face = lib.LoadFont(woff2Data, woff2Size, 16);
 * @endcode
 */

// Phase 1 — Fondations
#include "NKFont/NkFontExport.h"
#include "NKFont/NkFixed26_6.h"
#include "NKFont/NkMemArena.h"
#include "NKFont/NkBitmap.h"
#include "NKFont/NkBezier.h"

// Phase 2 — Parsers (TTF, CFF, BDF, Type1, WOFF1/WOFF2, TTC)
#include "NKFont/NkStreamReader.h"
#include "NKFont/NkTTFParser.h"
#include "NKFont/NkCFFParser.h"
#include "NKFont/NkBDFParser.h"
#include "NKFont/NkType1Parser.h"
#include "NKFont/NkWOFFParser.h"   // contient aussi NkTTCParser

// Phase 3 — Rasterizer scanline AA 4×4
#include "NKFont/NkOutline.h"
#include "NKFont/NkScanline.h"
#include "NKFont/NkHinter.h"

// Phase 4 — Shaping GSUB/GPOS, cache atlas, API publique
#include "NKFont/NkUnicode.h"
#include "NKFont/NkShaper.h"
#include "NKFont/NkGlyphCache.h"
#include "NKFont/NkFontFace.h"
#include "NKFont/NkFTFontFace.h"
