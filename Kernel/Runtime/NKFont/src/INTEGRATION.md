# Intégration NkFontRasterizer.cpp

## Ce qu'il faut faire dans NkFontParser.cpp

Dans NkFontParser.cpp, supprimer ou commenter la fonction MakeGlyphBitmap() existante
et sa fonction helper Rasterize() / BuildEdgeList() / AddEdgeCoverage() etc.

NkFontRasterizer.cpp fournit à la place :
- MakeGlyphBitmap()       → remplace l'ancienne, redirige vers MakeGlyphBitmapExact
- MakeGlyphBitmapExact()  → nouvel algorithme correct

## Ajouter dans NkFontParser.h

```cpp
// Dans namespace nkfont :
// Rastériseur exact (NkFontRasterizer.cpp)
void MakeGlyphBitmapExact(const NkFontFaceInfo* info,
                           nkft_uint8* output,
                           nkft_int32 outW, nkft_int32 outH, nkft_int32 outStride,
                           nkft_float32 scaleX, nkft_float32 scaleY,
                           nkft_float32 shiftX, nkft_float32 shiftY,
                           NkGlyphId glyph);
```

## Ce qu'il faut retirer de NkFontParser.cpp

Supprimer tout ce qui est dans le namespace anonymous suivant (à la fin de NkFontParser.cpp):
- struct ActiveEdge
- struct Edge
- static nkft_float32 gCovBuf[]
- FlattenQuad()
- FlattenCubic()
- BuildEdgeList()
- SortEdges()
- Rasterize()
- AddEdgeCoverage()  (ou équivalent)

Et supprimer les fonctions publiques :
- GetGlyphBitmapBox() : GARDER (pas changée)
- MakeGlyphBitmap()   : SUPPRIMER (maintenant dans NkFontRasterizer.cpp)

## CMakeLists.txt

Ajouter NkFontRasterizer.cpp à la liste des sources :
```cmake
target_sources(NKFont PRIVATE
    NKFont/NkFontAtlas.cpp
    NKFont/Core/NkFontParser.cpp
    NKFont/Core/NkFontRasterizer.cpp   # <-- AJOUTER
    NKFont/Core/NkFontDetect.cpp
    NKFont/Core/NkFontSizeCache.cpp
    NKFont/Embedded/NkFontEmbedded.cpp
)
```
