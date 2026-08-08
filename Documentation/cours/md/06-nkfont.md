# NKFont : polices, atlas et métriques

Afficher du texte est l'opération la plus banale d'une interface et la plus mal
comprise. On croit qu'il suffit d'« écrire une chaîne » ; en réalité, il faut
convertir des octets UTF-8 en *codepoints*, trouver le dessin de chaque
caractère, le rastériser une fois pour toutes dans une grande image, puis
émettre deux triangles texturés par lettre en faisant avancer un curseur.

`NKFont` fait tout cela, et **rien d'autre**. C'est un module
remarquablement honnête sur son périmètre : il produit un bitmap en mémoire
vive, et vous rend pour chaque caractère un rectangle à l'écran et un rectangle
dans le bitmap. Ce qu'on en fait ensuite ne le regarde pas.

## Le module ne connaît ni image, ni GPU, ni fenêtre

**`Kernel/Runtime/NKFont/NKFont.jenga:35-39`**

```cpp
    nkentseudependson(
        ["NKPlatform", "NKCore", "NKMemory", "NKMath", "NKContainers", "NKThreading", "NKLogger"],
        selfexport="NKFont",
        extra_includes=["src"],
    )
```

Ni `NKImage`, ni `NKCanvas`, ni `NKWindow`. NKFont est même
plus autonome que NKImage : il ne dépend pas du système de fichiers pour
fonctionner, puisqu'il embarque ses propres polices — nous y venons.

**`Kernel/Runtime/NKFont/ — arborescence`**

```
NKFont/
  NKFont.jenga
  src/NKFont/
    NkFont.h            ( 366 l.)  <- EN-TETE PUBLIC PRINCIPAL (NkFontAtlas + NkFont)
    NkFontAtlas.cpp     (~ 700 l.) <- packing + rasterisation
    NkFontMesh.cpp      (~ 400 l.) <- extrusion 3D des glyphes
    Core/
      NkFontTypes.h                <- alias nkft_*, NkFontCodepoint, NkGlyphId
      NkFontParser.h/.cpp          <- parser TTF/OTF/CFF/WOFF + rasteriseur + SDF
      NkFontRasterizer.cpp
      NkFontDetect.h/.cpp          <- NkFontProfile / NkFontDetector
      NkFontSizeCache.h/.cpp       <- cache multi-tailles
    Embedded/
      NkFontEmbedded.h/.cpp        <- 10 polices compressees DANS le binaire
```

> **⚠️ La description du `.jenga` promet six fonctionnalités inexistantes**
>
> `Kernel/Runtime/NKFont/NKFont.jenga:7-23` annonce fièrement le support de
> BDF, de Type 1 (PFB/PFA), de WOFF2 avec Brotli, d'une machine virtuelle de
> *hinting* TrueType, de GSUB pour les ligatures, et de l'algorithme
> bidirectionnel UAX#9.
>
> Une recherche exhaustive sur `src/` donne **zéro occurrence** de
> `BDF`, `Type1`, `PFB`, `Brotli`, `Bidi`,
> `GSUB` et `Hinting`. La seule mention de `WOFF2` est un
> avertissement disant qu'il n'est pas supporté. `GPOS` apparaît une fois,
> pour lire un décalage de table qui n'est ensuite jamais utilisé
> (`Core/NkFontParser.cpp:993`).
>
> La liste honnête est celle que le parser affiche lui-même :
>
> **`Kernel/Runtime/NKFont/src/NKFont/Core/NkFontParser.h:5-14`**
>
> ```cpp
> // Formats supportés :
> //   OK  TTF SimpleGlyph + CompositeGlyph
> //   OK  TTC (TrueType Collection) via faceIndex
> //   OK  OTF avec table glyf (contours TrueType)
> //   OK  OTF/CFF (Type 2 charstrings — interpréteur intégré)
> //   OK  WOFF (décompresseur zlib intégré)
> //   /!\ WOFF2 (Brotli — non supporté, dépendance externe requise)
> //   OK  cmap format 4 (BMP) et format 12 (full Unicode)
> //   OK  kern table format 0
> //   OK  SDF generation (Signed Distance Field)
> ```
>
> C'est déjà beaucoup — un interpréteur CFF Type 2 écrit à la main n'est pas rien.
> Mais c'est *ça*, et pas ce que promet le fichier de build.

## Les quatre objets à connaître

### `NkFontAtlas` — le propriétaire de tout

L'atlas est la seule chose que vous créez vous-même. Il possède la texture, les
configurations, et les polices.

**`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:102-133 (champs publics)`**

```cpp
            void *texID = nullptr;
            nkft_int32 texWidth = 0, texHeight = 0;
            nkft_uint8 *texPixels = nullptr;      // ALPHA8, 1 octet/pixel
            bool texReady = false;
            nkft_int32 texDesiredWidth = 0;
            nkft_int32 texGlyphPadding = 2;       ///< Padding recommandé >= 2 pour éviter le bleeding.
            bool sdfMode = false;
            nkft_int32 sdfSpread = 6;             ///< Rayon SDF en pixels (4-8 typique).
            NkFont *fonts[NK_FONT_ATLAS_MAX_FONTS] = {};
            NkFontConfig configs[NK_FONT_ATLAS_MAX_FONTS];
            nkft_uint32 fontCount = 0u;
```

Ce sont des champs publics, sans accesseurs : on les lit et on les écrit
directement. C'est délibéré et cohérent avec le reste du moteur.

**`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:134-157`**

```cpp
NkFont* AddFontFromFile(const char* path, nkft_float32 sizePixels, const NkFontConfig* cfg = nullptr);
NkFont* AddFontFromMemory(const nkft_uint8* data, nkft_size dataSize,
                          nkft_float32 sizePixels, const NkFontConfig* cfg = nullptr);
NkFont* AddFontFromMemoryOwned(nkft_uint8* data, nkft_size dataSize,
                               nkft_float32 sizePixels, const NkFontConfig* cfg = nullptr);

static const nkft_uint32* GetGlyphRangesDefault();
static const nkft_uint32* GetGlyphRangesLatinExtA();
static const nkft_uint32* GetGlyphRangesCyrillic();
static const nkft_uint32* GetGlyphRangesGreek();
static const nkft_uint32* GetGlyphRangesChineseFull();

bool Build();                                            // <- rasterise TOUT

void GetTexDataAsAlpha8 (nkft_uint8** op, nkft_int32* ow, nkft_int32* oh, nkft_int32* ob = nullptr);
void GetTexDataAsRGBA32 (nkft_uint8** op, nkft_int32* ow, nkft_int32* oh, nkft_int32* ob = nullptr);

void ClearTexData();
void Clear();
bool IsBuilt() const;
```

Comme `NkImage`, l'atlas n'est pas copiable :
`NkFontAtlas(const NkFontAtlas&) = delete;` (`NkFont.h:129`).

### `NkFont` — une face rastérisée à une taille

Attention au nom : dans ce module, `NkFont` n'est pas « une police ».
C'est **une police à une taille donnée**, déjà rastérisée. Charger Inter en
18 et en 32 pixels donne *deux* `NkFont`.

**`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:211-219`**

```cpp
            NkFontAtlas *containerAtlas = nullptr;    // l'atlas POSSEDE cette NkFont
            NkFontConfig config;
            nkft_float32 fontSize = 0, ascent = 0, descent = 0, lineAdvance = 0, scale = 1;
            NkFontGlyph glyphs[NK_FONT_MAX_GLYPHS];
            nkft_uint32 glyphCount = 0u;
            const NkFontGlyph *fallbackGlyph = nullptr;
            nkft_float32 fallbackAdvanceX = 0;
```

Le commentaire d'en-tête est catégorique sur la propriété :

**`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:88-90`**

```
 NkFont (le struct produit) n'est pas auto-portant : il est detenu par
 son `containerAtlas` qui parse le TTF, fournit les glyphes et la
 texture. NkFont seul ne « se charge » pas.
```

> **✅ Ce qu'il faut retenir**
>
> Les `NkFont*` que vous recevez appartiennent à l'atlas. **Ne les
> détruisez jamais vous-même**, et ne les gardez pas au-delà de la vie de l'atlas.
> Détruire l'atlas détruit toutes ses faces.

### `NkFontGlyph` — la structure qui fait tout le travail

**`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:32-38`**

```cpp
    struct NkFontGlyph {
            NkFontCodepoint codepoint = 0;
            nkft_float32 advanceX = 0.f;
            nkft_float32 x0 = 0, y0 = 0, x1 = 0, y1 = 0; ///< Position quad (pixels écran, relative au curseur).
            nkft_float32 u0 = 0, v0 = 0, u1 = 0, v1 = 0; ///< UV dans l'atlas [0..1].
            bool visible = true;
    };
```

Lisez-la attentivement : c'est la structure la plus importante du chapitre.

- `x0, y0, x1, y1` sont les coins du rectangle à l'écran,
  **relatifs au curseur** — pas absolus. Vous ajoutez la position du
  curseur, et c'est tout ;
- `u0, v0, u1, v1` sont les coordonnées dans l'atlas, déjà
  normalisées entre 0 et 1 ;
- `advanceX` est de combien avancer le curseur après ce caractère ;
- `visible` vaut `false` pour l'espace et les caractères qui
  n'ont pas de dessin : on saute l'émission du quad mais on avance quand
  même le curseur.

Il n'y a **rien d'autre à calculer**. Pas de métriques à recomposer, pas de
conversion d'unités de police en pixels : `Build()` l'a déjà fait.

### `NkFontConfig` — les réglages, posés avant l'ajout

**`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:44-62`**

```cpp
    struct NkFontConfig {
            const nkft_uint8 *fontData = nullptr;
            nkft_size fontDataSize = 0u;
            bool fontDataOwned = false;
            nkft_int32 fontIndex = 0;
            nkft_float32 sizePixels = 13.f;
            nkft_int32 oversampleH = 2;
            nkft_int32 oversampleV = 1;
            bool pixelSnapH = false;
            math::NkVec2f glyphOffset = {0.f, 0.f};
            nkft_float32 glyphMinAdvanceX = 0.f;
            nkft_float32 glyphMaxAdvanceX = 1e9f;
            nkft_float32 glyphExtraSpacing = 0.f;
            const nkft_uint32 *glyphRanges = nullptr;
            NkFontCodepoint glyphFallback = 0x003Fu;
            bool mergeMode = false;
            nkft_float32 rasterizerMultiply = 1.f;
            char name[40] = {};
    };
```

Dix-sept champs, dont deux comptent vraiment au début.

`glyphRanges` est un tableau de *paires* `{début, fin}`
terminé par un zéro. Si vous le laissez à `nullptr`, vous obtenez la
plage par défaut. Si vous voulez le latin étendu plus les caractères de
dessin de boîtes, vous écrivez :

**`Exemple écrit pour le cours (API : NkFont.h:56, 134)`**

```cpp
static const uint32 kRanges[] = { 0x0020, 0x00FF, 0x2500, 0x257F, 0 };  // Latin-1 + box-drawing
NkFontConfig cfg;
cfg.glyphRanges = kRanges;
NkFont *f = atlas.AddFontFromFile("Resources/Fonts/Inter-Regular.ttf", 20.f, &cfg);
```

`mergeMode` est le mécanisme de *repli* : en ajoutant une seconde
police avec `mergeMode = true`, ses glyphes viennent combler ceux qui
manquent dans la première, dans la même `NkFont`. C'est ainsi qu'une
police latine peut afficher des idéogrammes ou des émojis. Nous verrons le code
réel plus loin.

## Les dix polices embarquées : écrire du texte sans aucun fichier

C'est la meilleure porte d'entrée du module, et sans doute la fonctionnalité la
plus sous-estimée du moteur.

**`Kernel/Runtime/NKFont/src/NKFont/Embedded/NkFontEmbedded.h:49-74`**

```cpp
enum class NkEmbeddedFontId : nkft_uint32 {
    ProggyClean = 0, ProggyTiny = 1, DroidSans = 2, Karla = 3, Roboto = 4,
    Cousine = 5, SourceCodePro = 6, DroidSerif = 7, DejaVuSansMono = 8, Inter = 9,
    Count = 10, Default = ProggyClean,
};
```

Ces dix polices sont *réellement* présentes : le fichier
`Embedded/NkFontEmbedded.cpp` pèse 2,1 Mo de données compressées, et le
registre (`NkFontEmbedded.cpp:20265-20370`) fait pointer chaque entrée sur
un tableau non vide. Aucune n'est un espace réservé.

**`Kernel/Runtime/NKFont/src/NKFont/Embedded/NkFontEmbedded.h:120-158`**

```cpp
static bool IsAvailable(NkEmbeddedFontId id);
static const NkEmbeddedFontData* GetData(NkEmbeddedFontId id);
static NkFont* AddToAtlas(NkFontAtlas& atlas, NkEmbeddedFontId id,
                          nkft_float32 sizePx = 0.f, const NkFontConfig* cfg = nullptr);
static NkFont* AddDefaultFont(NkFontAtlas& atlas, const NkFontConfig* cfg = nullptr);
static const char* GetName(NkEmbeddedFontId id);
static const NkEmbeddedFontData* GetAll(nkft_int32* outCount);
static nkft_uint8* DecompressData(const NkEmbeddedFontData& data, nkft_uint32* outSize = nullptr);
static void FreeDecompressedData(nkft_uint8* ptr);
```

Les licences sont déclarées dans le registre : ProggyClean et ProggyTiny sont en
MIT ; DroidSans, Roboto, Cousine et DroidSerif en Apache 2.0 ; Karla,
SourceCodePro et Inter en OFL 1.1 ; DejaVuSansMono en Bitstream Vera / domaine
public. Ce n'est pas un détail décoratif : cela veut dire que vous pouvez
distribuer votre exécutable sans vous poser de question.

> **✅ Ce qu'il faut retenir**
>
> **Votre première application affichant du texte n'a besoin d'aucun fichier
> de police, d'aucun dossier de ressources, d'aucun chemin relatif.** Une ligne
> suffit :
> `NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::Inter, 18.f)`.
> Vous n'aurez jamais l'erreur « police introuvable ».

Le dépôt s'en sert d'ailleurs avec un repli en cascade, parce que rien n'oblige
une police donnée à être compilée dans un binaire particulier :

**`Applications/Pong/src/Pong/Render/FontAtlas.cpp:52-58`**

```cpp
            NkEmbeddedFontId fontId = NkEmbeddedFontId::ProggyClean;
            if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::Karla)) {
                fontId = NkEmbeddedFontId::Karla;
            } else if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::DroidSans)) {
                fontId = NkEmbeddedFontId::DroidSans;
            } else if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::Roboto)) {
                fontId = NkEmbeddedFontId::Roboto;
            }
```

## La séquence canonique en quatre temps

Tout usage de NKFont suit le même ordre, sans exception :

1. `atlas.Clear()` — si l'atlas a déjà servi ;
2. `Add*()` — une ou plusieurs fois, une par police et par taille ;
3. `atlas.Build()` — **c'est ici que les pixels apparaissent** ;
4. `atlas.GetTexDataAsAlpha8(...)` — on récupère le bitmap.

Le pont officiel entre NKFont et NKGui l'applique littéralement :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiFont.cpp:118-133`**

```cpp
        bool NkGuiFont::LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback) noexcept {
            atlas.Clear();
            face = nullptr;
            pixels = nullptr; // rechargeable
            NkFontConfig cfg;
            cfg.glyphRanges = NkGlyphRanges();
            face = NkFontEmbedded::AddToAtlas(atlas, id, sizePx, &cfg);
            if (!face)
                return false;
            NkMergeFallback(atlas, sizePx, extFallback); // repli pour les glyphes manquants
            if (!atlas.Build())
                return false;
            int32 bpp = 0;
            atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
            dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
            return dirty;
        }
```

> **⚠️ L'ordre n'est pas négociable, et l'échec est silencieux**
>
> `Build()` refuse de travailler sur un atlas vide et le journalise :
>
> **`Kernel/Runtime/NKFont/src/NKFont/NkFontAtlas.cpp:330-333`**
>
> ```cpp
>         if (fontCount == 0) {
>             logger.Info("[NkFontAtlas] Build():aucune fonte\n");
>             return false;
>         }
> ```
>
> Mais `GetTexDataAsAlpha8`, appelé avant `Build()`, ne journalise
> *rien* : il pose simplement `nullptr`
> (`NkFontAtlas.cpp:640-642` : `if (!texReady || !texPixels) { *op = nullptr; ... }`). Vous uploadez alors un pointeur nul, le backend ne fait rien,
> et vous cherchez pendant une heure pourquoi tous vos textes sont invisibles.
>
> **Testez toujours le pointeur récupéré**, comme le fait `NkGuiFont`
> avec son champ `dirty`.

### Le mécanisme de repli en action

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiFont.cpp:98-113`**

```cpp
        static void NkMergeFallback(NkFontAtlas &atlas, float32 sizePx, bool ext) noexcept {
            if (!ext)
                return;
            auto add = [&](const char *path, const uint32 *ranges) {
                if (!NkFileExists(path))
                    return;
                NkFontConfig fb;
                fb.glyphRanges = ranges;
                fb.mergeMode = true;
                atlas.AddFontFromFile(path, sizePx > 0.f ? sizePx : 16.f, &fb);
            };
            add(gFbBroad, NkBroadRanges());
            add(gFbCjk, NkCjkRanges());
            add(gFbEmoji, NkEmojiRanges());
        }
```

Trois polices de repli : une couverture large, une pour le chinois-japonais-
coréen, une pour les émojis. Chacune n'est ajoutée que si le fichier existe —
d'où le `NkFileExists`. Les chemins, eux, sont posés par l'application :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiFont.h:76-78`**

```cpp
 A poser par l'APPLICATION (ex. NKCode, depuis son dossier data/fonts) AVANT
 de charger les polices. Un chemin nullptr/vide = role desactive.
void NkSetFallbackFontPaths(const char* broad, const char* cjk, const char* emoji) noexcept;
```

*Avant* de charger — pas après. Une fois l'atlas construit, il est trop
tard.

## Dessiner une chaîne de caractères

### Le patron universel

Voici la boucle que vous écrirez, sous une forme ou une autre, chaque fois que
vous rendrez du texte vous-même. C'est celle de NKGui :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:245-266`**

```cpp
            while (p < end) {
                const NkFontCodepoint cp = NkFontDecodeUTF8(&p, end);
                if (cp == 0u)
                    break;
                const NkFontGlyph *g = face->FindGlyph(cp);
                if (!g)
                    continue;
                if (g->visible) {
                    const float32 x0 = NkGuiPixelSnap(x + g->x0), y0 = NkGuiPixelSnap(y + g->y0);
                    const float32 x1 = x0 + (g->x1 - g->x0), y1 = y0 + (g->y1 - g->y0);
                    if (x1 > xEnd)
                        break; // troncature simple
                    const uint32 i0 = Vtx({x0 + sTop, y0}, {g->u0, g->v0}, c);
                    const uint32 i1 = Vtx({x1 + sTop, y0}, {g->u1, g->v0}, c);
                    const uint32 i2 = Vtx({x1 + sBot, y1}, {g->u1, g->v1}, c);
                    const uint32 i3 = Vtx({x0 + sBot, y1}, {g->u0, g->v1}, c);
                    Tri(i0, i1, i2, texId);
                    Tri(i0, i2, i3, texId);
                }
                x += g->advanceX;
            }
```

Décortiquons.

1. `NkFontDecodeUTF8(&p, end)` avance le pointeur `p` et
   rend le *codepoint*. C'est une fonction libre du module
   (`NkFont.h:313`), doublée d'une méthode statique
   `NkFont::DecodeUTF8` (`NkFont.h:235`) ;
2. `FindGlyph(cp)` cherche le glyphe **avec repli** : si le
   caractère n'existe pas dans la police, on obtient le glyphe de
   remplacement plutôt que `nullptr`. La variante
   `FindGlyphNoFallback` existe si vous voulez savoir la vérité ;
3. si `visible`, on émet quatre sommets et deux triangles. Les
   positions sont `x + g->x0` et `y + g->y0` : le curseur plus
   le décalage du glyphe ;
4. **en dehors du `if`**, on avance : `x += g->advanceX`.
   L'espace n'a pas de dessin mais fait avancer le curseur.

### Le pixel-snap, et l'invariant subtil

Regardez à nouveau : les positions passent par `NkGuiPixelSnap`. Le
commentaire explique pourquoi :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:225-231`**

```
 Le curseur de texte accumule des avances FRACTIONNAIRES. Sans arrondi,
 seul le PREMIER glyphe d'une chaîne tombe sur un pixel entier ; tous les
 suivants dérivent et échantillonnent l'atlas ENTRE deux texels, ce qui
 rend le texte uniformément flou.
```

Mais l'arrondi est appliqué avec une précaution qu'on ne devine pas :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiDrawList.cpp:257-266`**

```
 On arrondit la POSITION du quad, jamais l'AVANCE : `x` continue
 d'accumuler la valeur exacte, donc la largeur totale de la chaîne
 est inchangée et MeasureWidth reste d'accord avec le rendu.
```

> **✅ Arrondir l'affichage, pas le calcul**
>
> Si vous arrondissiez `advanceX`, l'erreur s'accumulerait caractère après
> caractère et la largeur mesurée ne correspondrait plus à la largeur dessinée —
> vos alignements, vos centrages et vos troncatures deviendraient tous faux. C'est
> une leçon générale : **on arrondit à l'affichage, jamais dans
> l'accumulateur**.

### La ligne de base, pas le haut du texte

`y` dans la boucle ci-dessus n'est pas le haut du texte : c'est la
**ligne de base** — la ligne sur laquelle « posent » les lettres, et sous
laquelle descendent les jambages du `p` et du `g`. Les
`g->y0` sont donc négatifs pour la plupart des caractères.

Les métriques nécessaires sont dans la `NkFont`, calculées par
`Build()` :

- `ascent` — hauteur au-dessus de la ligne de base ;
- `descent` — profondeur en dessous ;
- `lineAdvance` — de combien descendre pour la ligne suivante ;
- `fontSize`, `scale` — la taille demandée et le facteur
  appliqué.

Donc, pour dessiner à partir d'un coin haut-gauche :

**`Exemple écrit pour le cours (API : NkFont.h:213)`**

```cpp
float x = posX;
float y = posY + font->ascent;      // y = LIGNE DE BASE, pas le haut !
// ... boucle de glyphes ...
y += font->lineAdvance;             // ligne suivante
```

> **⚠️ La constante magique de Pong**
>
> Le jeu `Pong` approxime la montée par un facteur empirique :
>
> **`Applications/Pong/src/Pong/Render/FontAtlas.cpp:206-208`**
>
> ```cpp
>             const float ascent = fontPx * 0.82f;
> ```
>
> Cela fonctionne pour *cette* police, à *ces* tailles. Changez de
> police et tout se décale verticalement. La valeur exacte existe pourtant :
> c'est `font->ascent`, calculée par `Build()`
> (`NkFontAtlas.cpp:377`). Utilisez-la.

### Mesurer avant de dessiner

**`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:229-235`**

```cpp
            nkft_float32 GetCharAdvance(NkFontCodepoint c) const {
                const NkFontGlyph *g = FindGlyph(c);
                return g ? g->advanceX : fallbackAdvanceX;
            }

            nkft_float32 CalcTextSizeX(const char *text, const char *textEnd = nullptr) const;
            static NkFontCodepoint DecodeUTF8(const char **text, const char *textEnd);
```

`CalcTextSizeX` parcourt la chaîne et somme les avances. C'est ce qu'il
faut pour centrer un libellé, dimensionner un bouton, ou décider d'une
troncature. Notez qu'il n'existe pas de `CalcTextSizeY` : la hauteur d'une
ligne, c'est `lineAdvance`, point.

## De l'atlas à l'écran

### L'atlas est en alpha 8 bits

`GetTexDataAsAlpha8` rend un octet par pixel : la *couverture* du
glyphe, de 0 (rien) à 255 (plein). Ce n'est pas une image en niveaux de gris à
afficher telle quelle — c'est un masque.

Or les rasteriseurs 2D du moteur échantillonnent des textures RGBA. Il faut donc
convertir, et le dépôt explique exactement pourquoi la recette est celle-là :

**`Applications/Pong/src/Pong/Render/FontAtlas.cpp:92-104`**

```cpp
            // L'atlas NKFont est alpha8 (1 canal = couverture du glyphe). Le
            // shader 2D de NKCanvas echantillonne la texture en RGBA puis la
            // multiplie par la couleur du vertex. On convertit donc l'atlas en
            // RGBA8 "blanc + alpha=couverture" : texture(1,1,1,cov) * color =
            // (color.rgb, color.a*cov) => exactement le rendu de texte voulu.
            const usize pixCount = static_cast<usize>(w) * static_cast<usize>(h);
            NkVector<uint8> rgba;
            rgba.Resize(pixCount * 4u);
            for (usize i = 0; i < pixCount; ++i) {
                rgba[i * 4 + 0] = 255;
                rgba[i * 4 + 1] = 255;
                rgba[i * 4 + 2] = 255;
                rgba[i * 4 + 3] = pixels[i]; // couverture du glyphe
            }
```

Blanc partout, alpha = couverture. Multiplié par la couleur du sommet, cela
donne exactement la couleur voulue avec la bonne transparence. Vous n'avez pas à
écrire cette boucle si vous ne le voulez pas : `GetTexDataAsRGBA32`
(`NkFontAtlas.cpp:662-680`) fait rigoureusement la même chose, en allouant
un second tampon à la demande.

### Le piège d'upload

**`Applications/Pong/src/Pong/Render/FontAtlas.cpp:107-109`**

```cpp
            // NB : surtout PAS Create()+Update() : Update exige gTextureBackend.Update
            // qui n'est pas cable sur tous les backends -> echec silencieux.
```

Ce commentaire vaut avertissement général : dans NkCanvas, préférez
`LoadFromImage` en un seul appel plutôt que la paire
`Create` + `Update`, tant que tous les backends ne câblent pas
`Update`.

### Le chemin NKGui, celui que vous emploierez

Dans une interface NKGui, tout ce qui précède est encapsulé dans un petit
*wrapper* :

**`Kernel/Runtime/NKGui/src/NKGui/Core/NkGuiFont.h:19-69 (abrégé)`**

```cpp
        struct NkGuiFont {
                NkFontAtlas atlas;            ///< possède la texture + les glyphes
                NkFont *face = nullptr;        ///< face produite (détenue par l'atlas)
                uint32 texId = 0x4E4B4654u; ///< 'NKFT' — id stable pour le backend
                uint8 *pixels = nullptr;    ///< atlas alpha8 (détenu par l'atlas)
                int32 atlasW = 0;
                int32 atlasH = 0;
                bool dirty = false; ///< à (ré)uploader côté backend
                bool LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback = true) noexcept;
                bool LoadFromFile(const char *path, float32 sizePx, bool extFallback = true) noexcept;
                /* Face() TexId() Valid() Ascent() Descent() LineHeight() MeasureWidth() */
        };
```

et son branchement tient en six lignes :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:281-288`**

```cpp
    auto fontPtr = memory::NkMakeUnique<NkGuiFont>();
    if (!fontPtr->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f)) {
        fontPtr->LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f);
    }
    ctx.font = fontPtr.Get();
    if (fontPtr->Valid()) {
        backend.UploadFontGray8(fontPtr->TexId(), fontPtr->pixels, fontPtr->atlasW, fontPtr->atlasH);
    }
```

Notez `UploadFontGray8` et non `UploadImageRGBA` : le backend sait
qu'un atlas de police est en un seul canal et fait la conversion lui-même
(`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:41`).
Après quoi `ctx.font` pointe la face, et tous les widgets de NKGui
dessinent leur texte tout seuls.

> **✅ Trois lignes, trois responsabilités**
>
> 1. `fontPtr->LoadEmbedded(id, taille)` — NKFont rastérise ;
> 2. `backend.UploadFontGray8(...)` — le backend crée la texture GPU ;
> 3. `ctx.font = fontPtr.Get()` — NKGui sait quelle face utiliser.
>
> Oubliez la deuxième : tous les textes sont invisibles, sans message d'erreur.
> Oubliez la troisième : les widgets n'affichent aucun libellé, sans message
> d'erreur non plus.

### Changer d'échelle : recharger et ré-uploader

Sur un écran à 150 %, il ne suffit pas d'agrandir les quads : l'atlas doit être
rastérisé plus gros, sinon le texte est flou. La démo le fait à la touche F9 :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:461-466`**

```cpp
        if (pendingScale > 0.f) { // F9 : appliquer la nouvelle echelle DPI
            SetUiScale(ctx, pendingScale);
            if (fontPtr->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f * pendingScale))
                backend.UploadFontGray8(fontPtr->TexId(), fontPtr->pixels, fontPtr->atlasW, fontPtr->atlasH);
            pendingScale = 0.f;
        }
```

Le ré-upload est obligatoire, et le module le dit :

**`Kernel/Runtime/NKFont/src/NKFont/Core/NkFontSizeCache.h:62-63`**

```
 @note Le rebuild de l'atlas invalide toutes les textures GPU.
       L'application doit être notifiée pour re-uploader la texture.
```

C'est la raison d'être du drapeau `dirty` de `NkGuiFont` et des
méthodes `NeedsGpuUpload()` / `ClearGpuUploadFlag()` du cache de
tailles.

## Le mode SDF : du texte net à toute taille

Un atlas classique est rastérisé à une taille précise ; l'agrandir le rend flou.
Le mode *Signed Distance Field* stocke, pour chaque texel, la distance au
bord du glyphe plutôt que sa couverture. Un *shader* reconstitue alors un
bord net à n'importe quelle échelle.

**`Exemple écrit pour le cours (API : NkFont.h:112-113, 149)`**

```cpp
NkFontAtlas atlas;
atlas.sdfMode   = true;      // AVANT Build()
atlas.sdfSpread = 6;         // 4-8 pixels
NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::Inter, 48.f);
atlas.Build();
// Utiliser le shader fourni : nkentseu::kNkFontFragSDF (NkFont.h:343)
// avec uSmoothing ~ 0.1 / fontSize.
```

Ce mode est réellement branché : `NkFontAtlas.cpp:496-535` alloue un
tampon temporaire et appelle `nkfont::NkMakeSDFFromBitmap(...)`.

Et le module fournit les deux *shaders* correspondants, en constantes
compilées :

**`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:324-336`**

```cpp
    static constexpr const char *kNkFontFragNormal = R"GLSL(
#version 460 core
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;
layout(binding=0) uniform sampler2D uAtlas;
void main() {
    float alpha = texture(uAtlas, vUV).a;
    if (alpha < 0.01) discard;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)GLSL";
```

`kNkFontFragSDF` (`NkFont.h:343`) est son pendant pour le mode SDF,
avec un uniforme `uSmoothing`. Vous n'en aurez pas besoin dans une
interface NKGui — le backend a déjà les siens — mais ils sont là si vous écrivez
votre propre rendu.

## Les limites dures, et l'invariant du `Build()`

Voici la partie que le lecteur pressé saute et regrette.

### Des tableaux de taille fixe

**`Kernel/Runtime/NKFont/src/NKFont/NkFont.h:99-100, 169-170`**

```cpp
static constexpr nkft_uint32 NK_FONT_ATLAS_MAX_FONTS = 16;
static constexpr nkft_uint32 NK_FONT_ATLAS_MAX_CUSTOM_RECTS = 64;
static constexpr nkft_uint32 NK_FONT_MAX_GLYPHS = 4096u;
static constexpr nkft_uint32 NK_FONT_INDEX_SIZE = 256u;
```

`NkFont::glyphs` est un tableau *membre* de 4096 entrées, pas un
conteneur dynamique. Conséquences concrètes :

- une face ne peut pas dépasser 4096 glyphes. La plage
  `GetGlyphRangesChineseFull()` en dépasse largement : elle ne
  rentrera pas ;
- un atlas ne peut pas contenir plus de 16 entrées — et une entrée, c'est
  *une police à une taille*. Cinq tailles de deux polices, plus trois
  replis, et vous êtes à treize ;
- chaque `NkFont` est un objet volumineux. Ne les copiez pas ;
  manipulez des `NkFont*`, ce que l'API impose de toute façon.

### `Build()` n'est ni réentrant ni parallélisable

**`Kernel/Runtime/NKFont/src/NKFont/NkFontAtlas.cpp:336-337`**

```cpp
        static nkfont::NkFontFaceInfo faceInfos[NK_FONT_ATLAS_MAX_FONTS];
        static nkft_float32 scales[NK_FONT_ATLAS_MAX_FONTS];
```

Ces tableaux sont `static` — partagés par toutes les instances. Et pire,
chaque face en garde l'adresse :

**`Kernel/Runtime/NKFont/src/NKFont/NkFontAtlas.cpp:381-382`**

```cpp
                // Stockage du faceInfo pour l'extraction des contours
                font->m_FaceInfo = &faceInfos[i];
```

Les tableaux de *packing* sont eux aussi statiques
(`NkFontAtlas.cpp:381-386`).

> **⚠️ Un seul atlas, un seul `Build()`, un seul fil**
>
> Trois conséquences, toutes silencieuses :
>
> 1. deux `Build()` **en parallèle** sur deux fils écrivent dans
>    les mêmes tableaux : corruption, sans message ;
> 2. un `Build()` sur un atlas B **invalide** les
>    `m_FaceInfo` des faces de l'atlas A. Les fonctions qui s'en
>    servent — `GetGlyphOutlinePoints`, l'extrusion 3D — deviennent
>    fausses. L'atlas A continue pourtant d'afficher du texte normalement :
>    seule l'extraction de contours ment ;
> 3. `Build()` n'est pas réentrant : ne l'appelez pas depuis un
>    *callback* déclenché par lui-même.
>
> **La règle à suivre : un seul `NkFontAtlas` par application, sur le
> fil principal.** C'est ce que fait NKGui, et c'est ce que vous ferez.

### Le `mergeMode` duplique les pointeurs

**`Kernel/Runtime/NKFont/src/NKFont/NkFontAtlas.cpp:185-189`**

```
 Les polices FUSIONNÉES (mergeMode) partagent le MÊME NkFont* que leur police de
 base (cf. AddFontFromMemoryOwned) : fonts[] contient donc des pointeurs DUPLIQUÉS.
 Il faut ne détruire chaque pointeur unique QU'UNE fois — sinon double-free / tas
 corrompu au rechargement (crash zoom).
```

Vous n'aurez pas à gérer cela vous-même — l'atlas s'en charge — mais retenez le
symptôme : « crash au zoom » signifie « rechargement d'atlas avec fusion ». Si
vous écrivez votre propre wrapper et que vous parcourez `atlas.fonts[]`
pour faire quelque chose sur chaque face, **vous visiterez plusieurs fois
le même pointeur**.

### Le cache de tailles rend `nullptr` la première fois

Si vous utilisez `NkFontSizeCache` (`Core/NkFontSizeCache.h:85`),
lisez ce contrat avant :

**`Kernel/Runtime/NKFont/src/NKFont/Core/NkFontSizeCache.h:114-117`**

```
 Si la taille n'est pas dans le cache :
   1. Ajoute la fonte à l'atlas.
   2. Marque l'atlas comme nécessitant un rebuild.
   3. Retourne nullptr jusqu'au prochain appel après BuildAtlas().
```

Un `GetFont(24.f)` qui renvoie `nullptr` n'est donc pas forcément
une erreur : c'est peut-être « revenez à la frame suivante ». Le cache plafonne à
`MAX_CACHED_SIZES = 16` tailles (`NkFontSizeCache.h:87`).

## Ce que le module ne fait pas

Pour être complet, et pour vous éviter de chercher :

| **Fonctionnalité** | **État réel** |
|---|---|
| WOFF2 / Brotli | absent (`Core/NkFontParser.h:11`) |
| GSUB, ligatures | absent |
| Bidi (UAX#9) | absent |
| *Hinting* TrueType | absent |
| BDF, Type 1 (PFB/PFA) | absent |
| GPOS | offset lu, jamais exploité |
| Kerning par paire | existe au parser, non remonté |
| Faux-gras | non — charger une fonte grasse |
| `NkFontSdf.h` | annoncé « futur », n'existe pas |

Deux précisions.

Le **kerning** est un vrai cas curieux : le parser sait lire la table
`kern` format 0 et expose `NkGetGlyphKernAdvance`
(`Core/NkFontParser.h:171`), mais l'information ne remonte pas jusqu'à
`NkFontGlyph`. Le wrapper NkCanvas l'écrit franchement :
« Le module NKFont n'expose pas le kerning par paire → 0 »
(`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkFont.h:87`).
Un « AV » restera donc plus espacé que dans un traitement de texte.

Le **gras** n'est pas synthétisé : « bold ignoré (le module ne fait pas de
faux-gras ; charger une fonte bold dédiée si nécessaire) »
(`.../Resources/NkFont.h:84-85`). Si vous voulez du gras, ajoutez une
seconde entrée dans l'atlas — en gardant à l'esprit la limite de seize.

## L'autre wrapper : `renderer::NkFont`

Il existe une seconde façade, côté NkCanvas, d'inspiration SFML :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkFont.h:63-115 (abrégé)`**

```cpp
        class NkFont {
            public:
                bool LoadFromFile(NkIRenderer2D &renderer, const char *path);
                bool LoadFromMemory(NkIRenderer2D &renderer, const void *data, usize sizeBytes);
                const NkGlyph &GetGlyph(uint32 codepoint, uint32 characterSize, bool bold = false) const;
                float32 GetKerning(uint32 first, uint32 second, uint32 characterSize) const;
                float32 GetLineHeight(uint32 characterSize) const;
                const NkTexture *GetAtlasTexture(uint32 characterSize) const;
                bool IsValid() const;
                void Destroy();
            private:
                struct Page {                       // une page = une TAILLE de caractère
                        uint32 characterSize = 0;
                        ::nkentseu::NkFontAtlas *atlas = nullptr;
                        ::nkentseu::NkFont *moduleFont = nullptr;
                        NkTexture texture;
                };
        };
```

Attention à l'homonymie : `renderer::NkFont` n'est pas
`nkentseu::NkFont`. Le premier est un gestionnaire de pages qui délègue au
second :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkFont.h:55-62`**

```
 Depuis le refactoring 2026-05-28, NKCanvas ne réimplémente plus la
 rasterisation (ex-FreeType supprimé). renderer::NkFont délègue au module
 NKFont (nkentseu::NkFontAtlas + nkentseu::NkFont) : une "page" (atlas
 rasterisé + texture GPU) par taille de caractère demandée.
```

Une page par taille demandée : chaque nouvelle `characterSize` construit
un nouvel atlas. Pratique, mais rappelez-vous l'invariant du `Build()`
statique — c'est un point à surveiller si vous mélangez ce wrapper avec un atlas
à vous.

## Récapitulatif des trois chemins

| **Chemin** | **Quand** | **Ce que vous écrivez** |
|---|---|---|
| `NkGuiFont` | interface NKGui | 3 lignes, rien de plus |
| `renderer::NkFont` + `NkText` | scène NkCanvas | chargement + dessin |
| Atlas manuel | rendu maison | atlas, conversion, quads |

Le premier est celui de ce cours. Le troisième est celui qui *explique*, et
c'est pourquoi nous l'avons détaillé : le jour où votre texte est flou, décalé
ou invisible, c'est dans ces quatre étapes que le problème se trouve.

## Exercices

> **✏️ 1 — Le plus court chemin vers un texte à l'écran**
>
> Dans une application NKGui minimale, remplacez le chargement de police par une
> boucle sur les dix polices embarquées : pour chaque
> `NkEmbeddedFontId` de 0 à 9, testez `IsAvailable`, récupérez son
> nom avec `GetName`, et affichez la liste dans un panneau.
>
> Puis ajoutez un bouton qui passe à la police suivante. Vous devrez recharger
> l'atlas *et* le ré-uploader dans le backend — l'exercice est là. Vérifiez ce
> qui se passe si vous oubliez le ré-upload.

> **✏️ 2 — Voir l'atlas**
>
> Récupérez le bitmap alpha8 avec `GetTexDataAsAlpha8`, enveloppez-le dans
> une `NkImage` — le chapitre 5 vous a donné tout ce qu'il faut — et
> sauvegardez-le en PNG.
>
> Ouvrez le fichier. Vous verrez les glyphes empilés en étagères, avec deux pixels
> de marge (`texGlyphPadding`). Comptez-les et comparez avec
> `face->glyphCount`. Puis recommencez avec une plage de glyphes plus large
> et observez l'atlas grandir. Enfin, mettez `sdfMode = true` et
> regardez à quoi ressemble un champ de distance.

> **✏️ 3 — Écrire son propre rendu de texte**
>
> Sans utiliser NKGui, écrivez une fonction qui prend une `NkFont*`, une
> chaîne UTF-8, une position et une couleur, et remplit deux tableaux de sommets
> et d'indices — exactement comme `NkGuiDrawList::AddText`. Testez-la sans
> GPU : affichez pour chaque caractère son *codepoint*, ses quatre coins et
> ses UV.
>
> Vérifiez ensuite que la somme des `advanceX` est bien égale à
> `face->CalcTextSizeX(texte)`. Puis arrondissez volontairement
> `advanceX` à l'entier et observez l'écart se creuser avec la longueur de
> la chaîne : vous venez de reproduire le bug contre lequel le commentaire de
> `NkGuiDrawList.cpp:257-266` met en garde.

> **✏️ 4 — Toucher les plafonds**
>
> Écrivez un programme console qui ajoute des polices à un atlas dans une boucle,
> en incrémentant la taille d'un pixel à chaque tour, et qui s'arrête quand
> `AddToAtlas` renvoie `nullptr`. Combien d'itérations ?
>
> Ensuite, tentez d'ajouter une police avec
> `GetGlyphRangesChineseFull()` et regardez ce que vaut
> `face->glyphCount` après `Build()`. Comparez avec
> `NK_FONT_MAX_GLYPHS`. Que se passe-t-il pour les caractères au-delà ?
> Cherchez la réponse dans le comportement observé, puis dans
> `NkFontAtlas.cpp` — et notez si le module vous a prévenu ou non.
