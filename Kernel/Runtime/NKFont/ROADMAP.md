# NKFont V2 — Feuille de route et formats supportés

## État actuel des formats

| Format | État | Notes |
|--------|------|-------|
| TTF (TrueType) | ✅ Complet | — |
| OTF avec glyf | ✅ Complet | — |
| TTC | ✅ via faceIndex | — |
| **OTF/CFF** | 🔶 Implémenté, limitation connue | Interpréteur Type 2 charstrings intégré directement dans `NkFontParser.cpp` (pas de fichiers séparés `NkFontCFF.h/cpp`). Voir « CFF — état réel » ci-dessous. |
| **WOFF** | ✅ Complet | Décompresseur zlib/deflate (tinfl) intégré dans `NkFontParser.cpp`, branché dans `NkInitFontFace()`. |
| WOFF2 | ❌ Non supporté | Nécessite Brotli, dépendance externe |
| Type 1 | ❌ Non supporté | Abandon |
| SVG fonts | ❌ Non supporté | Abandon |

---

## CFF — état réel de l'implémentation

Le support OTF/CFF **est branché et fonctionnel dans le chemin normal** :

```
NkGetGlyphShape()                                   [NkFontParser.cpp:1607]
  → if (info->isCFF && info->cff) DecodeGlyphCFF()   [NkFontParser.cpp:1513]
      → parse Name/TopDICT/String/GlobalSubr INDEX
      → charstrings INDEX (une entrée par glyphe)
      → InterpretType2()                              [NkFontParser.cpp:598]
```

`NkGetGlyphShape` est lui-même appelé depuis le pipeline réel de construction
d'atlas (`NkFontAtlas.cpp:887`) et le rastériseur (`NkFontRasterizer.cpp:641`),
qui sont atteints depuis `NkFontAtlas::AddFont*()` → `NkInitFontFace()`
(`NkFontAtlas.cpp:360`). Ce n'est donc pas du code mort : un OTF/CFF chargé par
l'API publique passe bien par cet interpréteur et produit des contours
(`NK_FONT_VERTEX_CUBIC`).

**Limitation connue (bug fonctionnel, pas juste un TODO cosmétique)** :
`InterpretType2()` reçoit toujours `gsubrs = nullptr` et `lsubrs = nullptr`
(voir l'appel dans `DecodeGlyphCFF`, `NkFontParser.cpp:1601`). Résultat :

- Les opérateurs `callsubr` (10) et `callgsubr` (29) ont leur corps gardé par
  `if (ctx.stackTop > 0 && lsubrs)` / `&& gsubrs` — toujours faux — donc **ils
  ne font rien** : ni saut dans une subroutine, ni même dépilement de l'index
  (`NkFontParser.cpp:883-891`).
- Le Private DICT n'est jamais localisé (l'opérateur Top DICT 18 `Private`
  n'est pas traité dans la boucle de parsing du Top DICT), donc les INDEX de
  **subroutines locales** ne sont jamais lus, et `nominalWidthX`/
  `defaultWidthX` — bien que lus dans la mauvaise boucle (Top DICT au lieu de
  Private DICT) — n'ont en pratique aucune valeur correcte.
- `seac` (accents composés) n'est pas géré.

**Conséquence pratique** : les glyphes de polices OTF/CFF qui n'utilisent que
des opérateurs de contour directs (rlineto, rrcurveto, hvcurveto, etc., sans
`callsubr`/`callgsubr`) se décodent correctement. Les polices de production
qui compressent leurs charstrings via des subroutines locales/globales
(très répandu) produiront des contours **incomplets ou corrompus** pour les
glyphes concernés, sans erreur signalée (retour `true` silencieux).

**Reste à faire pour un support CFF complet :**
1. Parser le Private DICT (opérateur Top DICT 18) pour localiser et charger
   l'INDEX des Local Subrs.
2. Passer les vraies `gsubrs`/`lsubrs` (+ tailles) à `InterpretType2`, avec le
   bias standard CFF (`bias = 107` si count < 1240, `1131` si < 33900, sinon
   `32768`), et faire réellement l'appel récursif dans les branches
   `callsubr`/`callgsubr`.
3. Lire `nominalWidthX`/`defaultWidthX` depuis le Private DICT (pas le Top
   DICT) et gérer l'argument de largeur optionnel en tête de charstring.
4. `seac`/accents composés (`endchar` avec 4 arguments, forme historique).

---

## WOFF — état réel de l'implémentation

Le WOFF est décompressé automatiquement et **branché dans le chemin normal** :

```
NkInitFontFace()                                     [NkFontParser.cpp:926]
  → détecte la signature 'wOFF' (0x774F4646)          [NkFontParser.cpp:932]
  → DecompressWOFF()                                  [NkFontParser.cpp:313]
      → reconstruit un buffer sfnt (TTF/OTF) standard en mémoire
      → InflateDeflate() (tinfl maison) par table compressée
  → parsing TTF/OTF classique sur le buffer reconstruit
```

Le buffer reconstruit est alloué via `nkentseu::memory::NkAlloc` et stocké
dans `info->woffBuffer` ; il est libéré par `NkFreeFontFace()`
(`NkFontParser.cpp:1063`). Pas de limitation fonctionnelle connue au-delà de
WOFF2 (non supporté, nécessiterait Brotli).

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

**Pour une qualité parfaite à toute taille → SDF (voir ci-dessous, déjà implémenté)**

---

## SDF — Signed Distance Fields

**Déjà implémenté et branché**, pas seulement planifié : `NkMakeSDFFromBitmap()`
(`NkFontParser.cpp:1948`) génère la texture SDF à partir d'un bitmap alpha, et
est appelée depuis la construction d'atlas (`NkFontAtlas.cpp:535`).

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

**Reste à faire :** MSDF (Multi-channel SDF, pour des coins plus nets) n'est
pas confirmé branché par cet audit — à vérifier séparément si nécessaire.

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
