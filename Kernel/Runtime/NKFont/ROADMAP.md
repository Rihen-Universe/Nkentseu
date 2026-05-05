# NKFont V2 — Feuille de route et formats non supportés

## État actuel des formats

| Format | État | Effort | Priorité |
|--------|------|--------|----------|
| TTF (TrueType) | ✅ Complet | — | — |
| OTF avec glyf | ✅ Complet | — | — |
| TTC | ✅ via faceIndex | — | — |
| **OTF/CFF** | 🔶 Détecté, non rendu | ~3000 lignes | Haute |
| **WOFF** | ❌ Non supporté | ~200 lignes | Moyenne |
| WOFF2 | ❌ Non supporté | ~500 lignes + Brotli | Basse |
| Type 1 | ❌ Non supporté | ~5000 lignes | Abandon |
| SVG fonts | ❌ Non supporté | ~2000 lignes | Abandon |

---

## Comment implémenter OTF/CFF (priorité haute)

OTF/CFF utilise des **charstrings Type 2** — un bytecode de 60 opérateurs
qui décrit les contours avec des Bézier cubiques.

### Fichiers à créer

```
NKFont/Core/NkFontCFF.h    — Parser de la table CFF
NKFont/Core/NkFontCFF.cpp  — Interpréteur Type 2 charstrings
```

### Algorithme

```
1. Dans NkFontParser.cpp, GetGlyphShape() :
   if (info->isCFF) return GetGlyphShapeCFF(info, glyph, buf);

2. NkFontCFF — lire la table CFF :
   - INDEX des charstrings (un par glyphe)
   - Private DICT (defaultWidthX, nominalWidthX)
   - INDEX des subrs (sous-routines)

3. Interpréteur Type 2 :
   - Stack d'arguments (max 48 valeurs)
   - Opérateurs : rmoveto, hmoveto, vmoveto
                  rlineto, hlineto, vlineto
                  rrcurveto, hhcurveto, hvcurveto
                  rcurveline, rlinecurve, vhcurveto, vvcurveto
                  callsubr, callgsubr, return
                  endchar, seac (accent)
   - Tous les opérateurs produisent des NK_FONT_VERTEX_CUBIC
```

### Estimations

- Interpréteur minimal (opérateurs essentiels) : ~800 lignes, 1 semaine
- Interpréteur complet (hints, accents, seac) : ~3000 lignes, 3 semaines

---

## Comment implémenter WOFF (priorité moyenne)

WOFF est un TTF/OTF avec un header de 44 octets et des tables compressées
individuellement en zlib. La décompression tinfl intégrée dans NkFontEmbedded.cpp
peut être réutilisée.

### Fichiers à créer/modifier

```
NKFont/Core/NkFontParser.cpp — InitFontFace() : détecter et décompresser WOFF
```

### Algorithme

```cpp
// Dans InitFontFace(), avant la détection TTF/OTF :
if (ReadU32(data) == 0x774F4646u) { // 'wOFF'
    // 1. Lire le header WOFF (44 octets)
    // 2. Pour chaque table WOFF :
    //    - Si compLength < origLength → décompresser avec tinfl
    //    - Sinon → copier tel quel
    // 3. Reconstruire un buffer TTF standard en mémoire
    // 4. Appeler InitFontFace() sur le buffer reconstruit
}
```

Estimations : ~300 lignes, 3 jours.

---

## Scale dynamique sans rebuild (approche actuelle)

La démo utilise **fontRef** (rastérisée à 64px) avec un scale GPU :

```
targetSize = 18px → scale = 18/64 = 0.28
                  → quads × 0.28 → rendu à ~18px écran
```

**Qualité :**
- Réduction (scale < 1) : **très propre**, le filtrage bilinéaire aide
- Agrandissement (scale > 1) : **flou**, limité à ~2× sans artefacts

**Pour une qualité parfaite à toute taille → SDF (futur)**

---

## SDF — Signed Distance Fields (futur)

Au lieu de stocker des pixels alpha, on stocke la **distance au contour** :
- Blanc (1.0) = intérieur du glyphe
- Noir (0.0) = extérieur
- Gris (0.5) = exactement sur le contour

Le shader GPU reconstruit le contour à n'importe quelle taille :

```glsl
// Fragment shader SDF
float dist = texture(uAtlas, vUV).r;
float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, dist);
// smoothing = 0.5/fontSize → adaptatif selon la taille

// Bonus : outline gratuit
float outline = smoothstep(0.4, 0.5, dist) * (1.0 - smoothstep(0.5, 0.6, dist));
```

**Avantages :**
- Qualité parfaite de 6px à 200px avec un seul atlas
- Outline, ombre portée, glow — tout en shader, sans CPU
- Atlas plus petit (une seule taille de référence)

**Fichiers à créer :**
```
NKFont/Core/NkFontSDF.h/cpp    — Générateur SDF
NKFont/Demo/kFontFragSDF       — Shader fragment SDF
```

Estimations : ~400 lignes pour le générateur, 2 semaines.

---

## Polices embarquées — comment ajouter

```bash
# 1. Téléchargez le TTF (ex: DroidSans, Apache 2.0)
# 2. Générez le header C
python3 Tools/EmbedFont.py Resources/Fonts/DroidSans.ttf DroidSans

# 3. Incluez dans NkFontEmbedded.cpp
#include "DroidSans_data.h"

# 4. Ajoutez dans sRegistry[]
{ "DroidSans", sDroidSansCompressedData, sDroidSansCompressedSize,
  sDroidSansOriginalSize, NkFontKind::Vector, 0.f, "Apache 2.0" },

# 5. Décommentez dans NkEmbeddedFontId (NkFontEmbedded.h)
DroidSans = 2,
```

**Polices recommandées (open source, petites) :**
- DroidSans.ttf — 190 Ko → ~50 Ko compressé (Apache 2.0)
- Cousine-Regular.ttf — 200 Ko → ~55 Ko compressé (Apache 2.0)  
- Karla-Regular.ttf — 180 Ko → ~48 Ko compressé (OFL 1.1)
- ProggyClean.ttf — 41 Ko → ~9 Ko compressé (MIT)
