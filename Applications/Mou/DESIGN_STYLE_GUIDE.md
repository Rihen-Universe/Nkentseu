# Mou — Charte graphique « enfant-friendly »

> Plateforme de jeux éducatifs pour la maternelle (3-5 ans).
> Équivalent de Nkoung, mais avec une direction artistique pensée pour les tout-petits.
> Date : 2026-06-15.

---

## 1. Le principe directeur

Tout l'opposé du style « tech/IA » de Nkoung (fond noir, néon, dégradés).
On vise le **cartoon flat chaleureux** : clair, rond, souriant.

| Règle | Détail |
|-------|--------|
| **Fond clair et chaud** | Crème `#FFF6E5` ou ciel `#AEE3FF`. Jamais de noir. |
| **Contours épais encrés** | Trait brun `#4A3728` de 6-8 px (pas noir pur — plus doux). |
| **Aplats, zéro dégradé** | Une forme = une couleur unie. |
| **Coins très arrondis** | Cartes `rx=30+`, boutons ronds, formes douces. |
| **Un visage sur tout** | Étoile, mascotte, formes : tout sourit. |
| **Une couleur héros + accents** | Le jaune soleil domine, les autres ponctuent. |
| **Couleurs vives mais pas néon** | Sago Mini ≠ flash épileptique. Équilibre. |

Sources : [RetroStyle 2D art](https://retrostylegames.com/blog/best-2d-art-styles-for-games/) ·
[Kids palette](https://www.media.io/color-palette/kids-color-palette.html) ·
[Sago Mini style](https://screenwiseapp.com/guides/sago-mini)

---

## 2. Palette officielle Mou

| Rôle | Nom | Hex | RGB |
|------|-----|-----|-----|
| Fond principal | Crème | `#FFF6E5` | 255,246,229 |
| Fond alternatif | Ciel | `#AEE3FF` | 174,227,255 |
| Carte / surface | Blanc | `#FFFFFF` | 255,255,255 |
| **Héros** | Jaune soleil | `#FFC93C` | 255,201,60 |
| Accent 1 | Corail | `#FF6B6B` | 255,107,107 |
| Accent 2 | Vert pousse | `#51CF66` | 81,207,102 |
| Accent 3 | Bleu ciel | `#4DABF7` | 77,171,247 |
| Accent 4 | Raisin | `#B197FC` | 177,151,252 |
| Accent 5 | Orange | `#FF922B` | 255,146,43 |
| Joues / mignon | Rose | `#FF8FA3` | 255,143,163 |
| Étoile / récompense | Or | `#FFD43B` | 255,212,59 |
| **Contour (encre)** | Brun doux | `#4A3728` | 74,55,40 |
| Texte sur clair | Brun doux | `#4A3728` | 74,55,40 |
| Texte sur couleur | Blanc | `#FFFFFF` | 255,255,255 |

Implémentée dans `src/Mou/UI/MouUIColor.h` (même structure que `NkoungUIColor.h`, drop-in).

---

## 3. Accessibilité — règle des couleurs daltoniennes

8 % des garçons sont daltoniens. Pour le jeu des couleurs, **chaque couleur a AUSSI
un symbole** (jamais la couleur seule comme indice) :

| Couleur | Symbole associé |
|---------|-----------------|
| Rouge / corail | ❤ Cœur |
| Bleu | ● Rond |
| Jaune | ★ Étoile |
| Vert | 🍃 Feuille |

Le panier ET l'objet à trier portent le même symbole → un enfant daltonien réussit
quand même par la forme.

---

## 4. Pack d'assets (dossier `assets/svg/`)

Format **SVG** (source de vérité) : vectoriel, net sur tous les écrans
(Android/iOS/Harmony, multi-DPI), recolorable, léger.

| Fichier | Usage |
|---------|-------|
| `mascot_nana.svg` | Mascotte « Nana » — guide vocal, félicite, explique |
| `star_reward.svg` | Étoile de récompense (fin de manche) |
| `button_play.svg` | Gros bouton jouer |
| `card_frame.svg` | Cadre de tuile (menu de sélection de jeu) |
| `bin_red.svg` / `bin_blue.svg` / `bin_yellow.svg` / `bin_green.svg` | Paniers de tri (jeu Couleurs) |
| `balloon_red.svg` / `balloon_blue.svg` / `balloon_yellow.svg` / `balloon_green.svg` | Objets à trier (ballons) |
| `apple.svg` | Objet à compter / calculer (jeux Compter & Calcul) |
| `duck.svg` | Objet à compter (jeu Compter) |

**Pack local camerounais/africain** (pilote : [docs/04-elements-culturels.md](docs/04-elements-culturels.md)) :

| Fichier | Usage |
|---------|-------|
| `basket_{red,yellow,green,blue,orange,purple}.svg` | Paniers tressés de tri (jeu Couleurs) — version locale des bins, + symbole daltonien |
| `fruit_tomate.svg` (rouge) · `fruit_mangue.svg` (jaune) · `fruit_avocat.svg` (vert) · `fruit_safou.svg` (bleu-violet) · `fruit_papaye.svg` (orange) · `fruit_plantain.svg` (jaune) | Objets à trier/compter, couvrent les 6 couleurs |
| `margouillat.svg` | Lézard agame (tête orange + corps bleu) — bonus interaction/couleurs |

---

## 5. Chargement SVG — NATIF dans NKImage (pas de conversion PNG !)

NKImage embarque un codec SVG (`NkImage/Codecs/SVG/NkSVGCodec.h`) qui **rasterise
le SVG à la résolution voulue, sans perte**. Aucune conversion PNG au build :
on charge le `.svg` directement et on rasterise à la taille de l'écran.

```cpp
#include "NKImage/Codecs/SVG/NkSVGCodec.h"
using namespace nkentseu;

// Option A — rasterisation directe en NkImage RGBA32 (taille cible)
NkImage* img = NkSVGCodec::DecodeFromFile("assets/svg/mascot_nana.svg", 512, 512);
// ... upload en NkTexture (NKCanvas) ...
img->Free();

// Option B — garder la version vectorielle et re-rasteriser par DPI
NkSVGImage* svg = NkSVGImage::LoadFromFile("assets/svg/mascot_nana.svg");
NkImage* hi = svg->Rasterize(0, 0);          // 0,0 = taille naturelle (viewBox)
NkImage* big = svg->Rasterize(1024, 1024);   // hi-DPI tablette
hi->Free(); big->Free(); svg->Free();
```

**Avantage multi-écrans (Android/iOS/Harmony)** : on rasterise à la densité réelle
de l'appareil → toujours net, jamais flou. C'est exactement ce qu'il faut pour Mú.

### Compatibilité de mes assets avec le codec
Le codec supporte : `<svg> <g> <path> <rect> <circle> <ellipse> <line> <polyline>
<polygon> <linearGradient> <radialGradient> <stop>` + `fill, stroke, stroke-width,
opacity, fill-opacity, stroke-opacity, fill-rule, transform, stroke-linecap,
stroke-linejoin, stroke-miterlimit` + couleurs `#RGB/#RRGGBB/rgb()/rgba()/noms CSS`.
**Mes assets sont 100 % compatibles.**

> ✅ **stroke-linecap / stroke-linejoin sont maintenant gérés** (ajoutés au codec le
> 2026-06-15) : les bouts de traits arrondis (bras de la mascotte, sourires) sont
> rendus correctement en `round`.
>
> ✅ **Gradients linear/radial gérés** (objectBoundingBox + userSpaceOnUse, stops,
> spreadMethod, gradientTransform). On peut donc enrichir le style plus tard (ciel
> dégradé, boutons brillants) sans quitter le SVG. Non géré : `<text>` (différé —
> le texte se rend en direct via NKFont), `<defs><style>` classes CSS, clipPath,
> patterns, filters.

### Option avancée — rendu vectoriel direct (sans texture)
`NkSVGImage::TriangulateAll(...)` produit un mesh triangulé (vertices + indices +
couleurs) prêt pour le GPU. On peut donc dessiner les assets **comme géométrie
vectorielle** dans NKCanvas, zoom infini sans pixellisation. À considérer plus tard.

---

## 6. Mascotte « Nana »

Petite créature ronde jaune soleil (style boule de soleil) : grands yeux, joues
roses, sourire, pieds orange. Elle **parle** (voix FR), donne les consignes,
encourage et félicite. C'est le fil rouge émotionnel de toute la plateforme
(modèle Khan Academy Kids : un ami qui guide).
