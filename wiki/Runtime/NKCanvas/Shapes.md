# Les formes dessinables

> Couche **Runtime** · NKCanvas · Dessiner des **formes 2D** prêtes à l'emploi — le rectangle
> `NkRectangleShape`, le cercle `NkCircleShape`, le polygone convexe `NkConvexShape`, le segment
> épais `NkLineShape`, tous bâtis sur la base abstraite `NkShape`.

Dès qu'on veut **poser une forme à l'écran** — une raquette de Pong, un cercle de portée autour
d'une unité, une barre de vie, un polygone d'éditeur — on ne veut pas tracer des triangles à la
main. On veut un **objet** qu'on positionne, qu'on colore, qu'on tourne, et qu'on dessine d'un seul
appel. C'est exactement le rôle des *formes* de NKCanvas, calquées sur `sf::Shape` de SFML : chaque
forme connaît sa **géométrie locale** (une suite de sommets), hérite d'un **transform** (position,
rotation, échelle, origine) et sait se **dessiner** sur une cible de rendu. Le moteur s'occupe du
reste : trianguler l'intérieur, tracer le contour, plaquer une texture.

L'idée maîtresse tient en une phrase : **une forme = des points locaux + un transform + un style de
remplissage/contour/texture**. Toute la famille dérive d'une seule base, `NkShape`, qui factorise le
style et le rendu ; les sous-classes ne fournissent que leurs **points**. Le compromis à garder en
tête dès maintenant : le remplissage se fait par **triangulation en éventail** (*fan*), qui ne rend
correctement que les formes **convexes** — un détail qui décide quel type choisir.

- **Namespace** : `nkentseu::renderer`
- **Header de base** : `#include "NKCanvas/Renderer/Shapes/NkShape.h"`
- **Headers concrets** : `NkRectangleShape.h`, `NkCircleShape.h`, `NkConvexShape.h`, `NkLineShape.h`
  (tous sous `NKCanvas/Renderer/Shapes/`)

---

## La base de toute forme : `NkShape`

`NkShape` est la **classe abstraite** dont héritent toutes les formes. Elle n'existe jamais seule —
on l'instancie via une sous-classe — mais c'est elle qui porte tout ce qui est commun. Son héritage
est **multiple** et révélateur : `public NkTransformable, public NkDrawable`. De la première, elle
récupère un transform complet (position, rotation, échelle, origine) ; de la seconde, le contrat
`Draw(target, states)` qui fait d'elle quelque chose qu'on peut passer à `target.Draw(...)`.

Le rôle de `NkShape` est de transformer une **liste de points locaux** — que la sous-classe fournit
via deux virtuels purs — en pixels. Le remplissage est obtenu par **triangulation en éventail** :
on relie le premier sommet à tous les autres pour former des triangles. C'est rapide et sans
allocation, mais cela suppose la forme **convexe** ; une forme concave s'auto-intersecte et rend
faux. Le contour, lui, est tracé en `NK_LINES` paire-à-paire, d'une épaisseur réglable. Une texture
optionnelle peut être plaquée, avec un *textureRect* pour choisir la portion d'image utilisée.

```cpp
class MaForme : public NkShape {
public:
    uint32  GetPointCount() const override          { return 3; }
    NkVec2f GetPoint(uint32 i) const override        { return mSommets[i]; }
private:
    NkVec2f mSommets[3];
};
```

Le **style** est entièrement réglable sur la base : `SetFillColor` (défaut blanc), `SetOutlineColor`
(défaut noir) et `SetOutlineThickness` (défaut `0.f`, donc pas de contour tant qu'on ne l'active
pas). Ces accesseurs sont tous `inline noexcept` : ils ne font qu'écrire un membre.

Côté **texture**, le piège central est l'**ownership** : `SetTexture(const NkTexture*)` stocke un
pointeur **brut, non-owning**. La forme ne possède pas la texture, ne la crée pas, ne la détruit
pas — c'est à l'appelant de garantir qu'elle vit assez longtemps. Et `SetTextureRect` est
**collant** : l'appeler bascule définitivement `mHasTextureRect = true` (mode UV explicite), et il
n'existe aucune méthode pour revenir au mapping automatique.

Le calcul des **UV** passe par `GetPointUV(index, localBounds)`, un virtuel **surchargeable**. Par
défaut : si un textureRect est actif, on interpole le point local dans `localBounds` puis on le
remappe vers les bornes du rect (`left + tx*width`, `top + ty*height`) ; sinon, auto-mapping
classique = point local normalisé sur la bounding box locale, dans `[0..1]²`. Des garde-fous
mettent `invW`/`invH` à `0.f` si une dimension est nulle ou négative, ce qui évite une division par
zéro. Une sous-classe peut surcharger pour un mapping custom (polaire, cylindrique…).

Enfin, deux **bounding boxes** : `GetLocalBounds()` parcourt tous les points (`O(n)`) pour la boîte
*avant* transform — elle renvoie `NkRect2f(0,0,0,0)` si la forme n'a aucun point ; et
`GetGlobalBounds()` applique le transform à cette boîte (`GetTransform().TransformRect(...)`) pour
la boîte *monde*, celle qu'on utilise pour le picking, le culling ou la collision large-phase.

Ce n'est **pas** un conteneur de triangles : une forme ne stocke pas sa triangulation, elle la
recalcule à chaque `Draw` à partir de ses points. Et ce n'est **pas** un objet GPU : aucune
ressource n'est allouée par la base (sauf `NkConvexShape`, qui stocke ses points) — le rendu réel
vit dans `NkShape::Draw`, seule méthode non-inline, implémentée dans le `.cpp`.

> **En résumé.** `NkShape` factorise tout le commun : transform hérité, style
> (fill/outline/texture), UV, bounding boxes locale/monde, et le `Draw`. Les sous-classes ne
> fournissent que `GetPointCount()` + `GetPoint(i)` en **coords locales**. Remplissage par
> triangulation **fan** → convexes uniquement ; texture **non-owning** ; textureRect **collant**.

---

## Le rectangle : `NkRectangleShape`

C'est la forme la plus simple et la plus courante : quatre coins, une taille. On la dimensionne par
`SetSize({largeur, hauteur})`, et ses points locaux sont les quatre coins partant de l'origine
`(0,0)`. L'ordre est fixe et important pour la triangulation : `0` = haut-gauche `{0,0}`, `1` =
haut-droit `{size.x, 0}`, `2` = bas-droit `{size.x, size.y}`, `3` = bas-gauche `{0, size.y}`.
`GetPointCount()` renvoie toujours **4** et `GetPoint(i)` est `O(1)` via un `switch`.

Comme tout part de `(0,0)`, la position finale à l'écran est donnée par le transform hérité
(`SetPosition`) combiné à l'origine (`SetOrigin`). Pour pivoter un rectangle autour de son centre,
on pose `SetOrigin({size.x/2, size.y/2})`. Le type est **header-only** et n'alloue rien (juste un
`NkVec2f mSize`).

```cpp
NkRectangleShape paddle({ 20.f, 100.f });
paddle.SetFillColor(NkColor2D::White);
paddle.SetPosition({ 40.f, 300.f });
target.Draw(paddle);
```

> **En résumé.** `NkRectangleShape` = quatre coins depuis `(0,0)`, dimensionné par `SetSize`,
> toujours 4 points, `GetPoint` en `O(1)`. Convexe par construction, donc rend toujours bien.
> Header-only, sans allocation.

---

## Le cercle : `NkCircleShape`

Un cercle parfait n'existe pas en rastérisation : on l'**approxime** par un polygone régulier. C'est
exactement ce que fait `NkCircleShape` — il génère `segments` sommets répartis sur un cercle de
rayon `radius`. Plus on met de segments, plus c'est lisse (et plus il y a de triangles) : le header
conseille 30+ pour une taille moyenne, 64 pour un grand cercle. Le défaut est 32.

`GetPoint(i)` est `O(1)` : il calcule l'angle `2π·index/segments` et renvoie
`{radius + radius·cos, radius + radius·sin}` via `math::NkCos`/`math::NkSin`/`math::NK_PI_F` (et
`{0,0}` si `segments == 0`). La **convention de centrage** mérite attention : le centre est placé en
`(radius, radius)`, pas en `(0,0)`, pour que la bounding box locale parte de `(0,0)` — cohérent avec
le rectangle. Conséquence pratique : pour positionner ou faire tourner le cercle **depuis son
centre**, on pose `SetOrigin({radius, radius})`.

```cpp
NkCircleShape halo(48.f, 64);          // rayon 48, 64 segments → bien lisse
halo.SetOrigin({ 48.f, 48.f });         // origine au centre
halo.SetFillColor(NkColor2D::Yellow);
halo.SetPosition(unitWorldPos);
target.Draw(halo);
```

Ce n'est **pas** un disque analytique : à fort grossissement, on voit les facettes du polygone.
Header-only, sans allocation (deux scalaires : `mRadius`, `mSegments`).

> **En résumé.** `NkCircleShape` = polygone régulier de `segments` sommets sur un rayon `radius`,
> `GetPoint` en `O(1)` par trigonométrie. Centre en `(radius, radius)` → `SetOrigin({r, r})` pour
> pivoter/positionner depuis le centre. Plus de segments = plus lisse.

---

## Le polygone convexe libre : `NkConvexShape`

Quand la forme n'est ni un rectangle ni un cercle — un triangle de visée, un losange, un pentagone
d'éditeur — on fournit ses sommets soi-même via `NkConvexShape`. On fixe le nombre de points
(`SetPointCount(n)` ou le constructeur `NkConvexShape(pointCount)`), puis on place chacun par
`SetPoint(index, p)`. Les sommets sont stockés dans un `NkVector<NkVec2f>`, le conteneur maison —
donc **l'allocation passe par NKMemory**.

C'est la raison d'une particularité : contrairement aux autres formes, **les méthodes qui
redimensionnent ne sont pas `noexcept`** (le ctor `NkConvexShape(uint32)`, `SetPointCount`,
`SetPoint`) car un `Resize` peut allouer. `SetPoint` est tolérant : il ignore silencieusement un
index hors bornes, et `GetPoint` est *bounds-checké* (`{0,0}` si l'index dépasse la taille).
`GetPointCount()` renvoie la taille du vecteur.

```cpp
NkConvexShape tri(3);
tri.SetPoint(0, { 0.f,   0.f });
tri.SetPoint(1, { 50.f, 100.f });
tri.SetPoint(2, { 100.f,  0.f });
tri.SetFillColor(NkColor2D::Cyan);
target.Draw(tri);
```

Le nom est un **contrat** : la triangulation fan de `NkShape` n'est correcte que pour un polygone
**convexe**. Si vous fournissez des sommets concaves, le rendu s'auto-intersecte et devient faux.
Pour une forme concave, il faut une vraie triangulation (*ear-clipping*, Delaunay) effectuée côté
application, puis dessiner via un `NkVertexArray`.

> **En résumé.** `NkConvexShape` = polygone à sommets arbitraires (3+) fournis par l'utilisateur,
> stockés dans un `NkVector` (alloc NKMemory → ctor/Set non-`noexcept`). `SetPoint` ignore les index
> hors bornes, `GetPoint` est borné. Doit rester **convexe**, sinon rendu incorrect.

---

## Le segment épais : `NkLineShape`

Une « ligne » visible à l'écran n'a pas qu'une longueur, elle a aussi une **épaisseur**.
`NkLineShape` représente un segment AB d'épaisseur donnée comme un **quad** (4 coins) dont l'axe
long suit AB. On le construit par ses extrémités et son épaisseur (`NkLineShape(a, b, thickness)`,
épaisseur 1 par défaut), ou on les règle après coup avec `SetA`, `SetB`, `SetEndpoints`,
`SetThickness` — chacun déclenchant un recalcul interne.

Ce recalcul, `Recompute()` (privé, `noexcept`), construit les 4 coins : il prend `dir = B − A`,
gère le cas **dégénéré** `A == B` (`len2 <= 0`) en plaçant les 4 coins sur A pour éviter un NaN,
puis calcule la **normale unitaire** (rotation 90° : `nx = -dir.y·invLen`, `ny = dir.x·invLen`,
via `math::NkSqrt`) et un offset `normale·(thickness/2)`. Les coins sont ordonnés
`[0]=A-off, [1]=B-off, [2]=B+off, [3]=A+off`. `GetPointCount()` renvoie toujours **4** et
`GetPoint(i)` lit `mCorners[i]` (`{0,0}` si `i >= 4`). Le transform hérité s'applique **en plus**,
donc on peut faire tourner ou mettre à l'échelle le segment après création.

```cpp
NkLineShape ray({ 100.f, 100.f }, { 400.f, 250.f }, 4.f);
ray.SetFillColor(NkColor2D::Red);
target.Draw(ray);
```

Pour un **trait fin de 1 pixel**, le header recommande `NkRenderer2D::DrawLine` (un seul segment
`LINE`), plus efficient que ce quad triangulé. `NkLineShape` est header-only, sans allocation
(quelques scalaires + un tableau fixe `mCorners[4]`).

> **En résumé.** `NkLineShape` = segment AB épais matérialisé en quad 4 coins, recalculé à chaque
> mutation. Gère le cas dégénéré A==B sans NaN, le transform s'applique en plus. Toujours 4 points.
> Pour 1 px fin, préférer `NkRenderer2D::DrawLine`.

---

## Aperçu de l'API

Tous les types vivent dans `nkentseu::renderer`. Couleur = `NkColor2D`, vecteur = `NkVec2f`,
rectangle = `NkRect2f`. Les sous-classes héritent en plus de **tout** `NkShape` (style, texture,
UV, bounds, `Draw`) et de `NkTransformable` (position/rotation/échelle/origine).

### `NkShape` — base abstraite

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Héritage | `NkTransformable`, `NkDrawable` | Transform (pos/rot/scale/origine) + contrat `Draw`. |
| Ctor / dtor | `NkShape()`, `~NkShape()` (virtuel) | Construction / destruction polymorphe. |
| Remplissage | `SetFillColor` / `GetFillColor` | Couleur intérieure (défaut blanc). |
| Contour | `SetOutlineColor` / `GetOutlineColor` | Couleur du contour (défaut noir). |
| Contour | `SetOutlineThickness` / `GetOutlineThickness` | Épaisseur du contour (défaut `0.f` = aucun). |
| Texture | `SetTexture` / `GetTexture` | Texture **non-owning** (`const NkTexture*`, défaut `nullptr`). |
| Texture | `SetTextureRect` / `GetTextureRect` | Rect UV ; **active** le mode UV explicite (collant). |
| Points (pur) | `GetPointCount()` | Nombre de sommets (≥3 pour une aire, 2 pour un segment). |
| Points (pur) | `GetPoint(index)` | Position du i-ème sommet en **coords locales**. |
| UV | `GetPointUV(index, localBounds)` | UV `[0..1]²` du point (surchargeable). |
| Bounds | `GetLocalBounds()` | Boîte avant transform (`O(n)`). |
| Bounds | `GetGlobalBounds()` | Boîte monde après transform. |
| Rendu | `Draw(target, states)` | Override `NkDrawable` : fan + outline + texture (`.cpp`). |

### `NkRectangleShape` — rectangle

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkRectangleShape()`, `NkRectangleShape(size)` | Vide / dimensionné. |
| Taille | `SetSize` / `GetSize` | Largeur × hauteur. |
| Points | `GetPointCount()` (= 4), `GetPoint(index)` | 4 coins depuis `(0,0)`, ordre TL→TR→BR→BL, `O(1)`. |

### `NkCircleShape` — cercle (polygone régulier)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkCircleShape()`, `NkCircleShape(radius, segments=32)` | Vide / rayon + segments. |
| Rayon | `SetRadius` / `GetRadius` | Rayon du cercle. |
| Finesse | `SetPointCount(n)` | Nombre de segments (30+ moyen, 64 grand). |
| Points | `GetPointCount()` (= segments), `GetPoint(index)` | Sommet par trigonométrie, `O(1)`, centre en `(r,r)`. |

### `NkConvexShape` — polygone convexe libre

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkConvexShape()`, `NkConvexShape(pointCount)` | Vide / nombre de points (alloue, non-`noexcept`). |
| Sommets | `SetPointCount(n)` | Redimensionne (`NkVector::Resize`, alloue). |
| Sommets | `SetPoint(index, p)` | Place un sommet (ignoré si index hors bornes). |
| Points | `GetPointCount()`, `GetPoint(index)` | Taille du vecteur ; lecture bornée (`{0,0}` hors bornes). |

### `NkLineShape` — segment épais (quad)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkLineShape()`, `NkLineShape(a, b, thickness=1)` | Vide / extrémités + épaisseur. |
| Extrémités | `SetA` / `GetA`, `SetB` / `GetB`, `SetEndpoints` | Points A et B (recalcul à chaque set). |
| Épaisseur | `SetThickness` / `GetThickness` | Largeur du segment. |
| Points | `GetPointCount()` (= 4), `GetPoint(index)` | 4 coins du quad (`{0,0}` si index ≥ 4). |

---

## Référence complète

Chaque élément est repris en détail ci-dessous, avec ses usages dans les différents domaines du
temps réel — rendu, ECS, physique, animation, gameplay/IA, UI/2D, outils/éditeur.

### Héritage : `NkTransformable` + `NkDrawable`

Toute forme **est** un objet transformable **et** un objet dessinable. De `NkTransformable` viennent
position, rotation, échelle et origine, ainsi que `GetTransform()` qui produit la matrice utilisée
pour passer du local au monde. De `NkDrawable` vient la signature `Draw(target, states)`, qui rend
les formes interopérables avec `NkRenderTarget::Draw(...)`. C'est cette double identité qui rend le
modèle si maniable :

- **Rendu / 2D** — on positionne une forme une fois (`SetPosition`), on la redessine chaque frame ;
  le transform sépare proprement la **géométrie** (fixe, locale) du **placement** (variable).
- **Gameplay** — l'origine (`SetOrigin`) sert de **point de pivot** : centrer l'origine d'une
  raquette ou d'un projectile pour qu'il tourne autour de lui-même.
- **Outils / éditeur** — `GetGlobalBounds()` (boîte monde) sert au *picking* (clic sur la forme),
  au *gizmo* de sélection, et au *snapping*.

### Couleurs et contour

`SetFillColor` / `SetOutlineColor` / `SetOutlineThickness` (et leurs lecteurs) décrivent l'apparence.
Le remplissage par défaut est blanc, le contour noir, l'épaisseur de contour `0.f` — autrement dit,
**aucun contour n'apparaît tant qu'on ne donne pas une épaisseur > 0**. Ces trois réglages sont
`inline noexcept`, donc gratuits à appeler. Usages :

- **UI / 2D** — boutons, barres de progression, panneaux : couleur de fond + liseré.
- **Gameplay / IA** — coder une information par la couleur (ennemi rouge, allié vert), souligner un
  état par un contour (unité sélectionnée).
- **Animation** — interpoler `NkColor2D` dans le temps (clignotement, fondu d'alerte) en réécrivant
  `SetFillColor` chaque frame.
- **Outils / éditeur** — contour fin pour matérialiser un *bounding volume* ou une zone de
  déclenchement sans masquer ce qu'il y a derrière.

Le contour est tracé en `NK_LINES` paire-à-paire, **sans joints ni *caps* stylisés** (pas de
round/bevel) : pour un trait épais avec coins propres, il faut construire la géométrie soi-même.

### Texture et textureRect

`SetTexture(const NkTexture*)` plaque une image sur la forme. Le point crucial est l'**ownership** :
le pointeur est **non-owning**, stocké tel quel ; la forme ne crée ni ne détruit la texture. C'est à
l'appelant de garantir que la texture reste valide aussi longtemps que la forme l'utilise — il n'y a
ici **aucun couple Create/Destroy**. `SetTextureRect(const NkRect2f&)` choisit la portion d'image à
utiliser et **bascule définitivement** en mode UV explicite (`mHasTextureRect = true`) : il n'existe
pas de méthode pour revenir au mapping automatique. Usages :

- **Rendu / 2D** — habiller un rectangle d'une texture (sprite simple), choisir une sous-région
  (atlas) via le textureRect.
- **UI** — icônes, vignettes, fond texturé d'un panneau.
- **Outils / éditeur** — aperçu d'un asset image posé sur une forme rectangulaire.

> Pour un véritable système de sprites (animation par frames, flip, ancrage), la couche NKCanvas
> propose des types dédiés ; `NkShape::SetTexture` reste l'option simple « une forme, une image ».

### `GetPointCount` / `GetPoint` — le contrat des sous-classes

Ces deux **virtuels purs** sont tout ce qu'une sous-classe doit fournir : combien de sommets, et où
se trouve chacun en **coordonnées locales** (avant transform). Le contrat est `>= 3` pour une forme
qui a une aire, `2` pour un simple segment. C'est sur cette liste que `NkShape` construit la
triangulation et le contour. Comme les points sont locaux, ils sont indépendants de la position à
l'écran — c'est le transform hérité (origine, rotation, échelle) qui les place :

- **Rendu** — la base itère ces points pour générer les triangles de remplissage à chaque `Draw`.
- **Physique / collision** — la même liste de points peut alimenter un test de collision polygonal
  (SAT) côté application : la géométrie de rendu et de collision partagent la définition.
- **Outils / éditeur** — surligner ou éditer les sommets un par un.

### `GetPointUV` — le mapping de texture

`GetPointUV(index, localBounds)` retourne l'UV normalisé `[0..1]²` d'un sommet. Par défaut, deux
comportements : si un textureRect est actif, on interpole le point dans `localBounds` puis on le
remappe vers les bornes du rect (`left + tx*width`, `top + ty*height`) ; sinon, on normalise
simplement le point sur la bounding box locale. Des garde-fous mettent l'inverse de largeur/hauteur
à `0.f` si la dimension est `<= 0`, évitant toute division par zéro. Étant **virtuel
surchargeable**, il ouvre la porte aux mappings spéciaux :

- **Rendu** — surcharger pour un mapping **polaire** (texture qui tourne avec un cercle) ou
  **cylindrique**.
- **Outils** — projeter une grille ou un damier de calibration sur une forme arbitraire.

### `GetLocalBounds` / `GetGlobalBounds` — les boîtes englobantes

`GetLocalBounds()` calcule la boîte englobante **avant transform** en parcourant tous les points
(`O(n)`) ; elle renvoie une boîte nulle `(0,0,0,0)` si la forme n'a aucun sommet. La boucle interne
utilise un `if / else if` (un point n'est comparé qu'à une borne par itération), ce qui reste
correct parce que le premier point initialise min **et** max. `GetGlobalBounds()` applique le
transform à cette boîte (`GetTransform().TransformRect(...)`) pour obtenir la boîte **monde**.
Usages :

- **Rendu** — *culling* : ne dessiner que les formes dont la boîte monde intersecte la vue.
- **Gameplay / IA** — collision *broad-phase* approchée (deux boîtes se chevauchent-elles ?),
  déclenchement de zones.
- **Outils / éditeur** — *picking* au clic, rectangle de sélection, *snapping*, cadrage automatique
  d'une vue sur une forme.

### `Draw` — le rendu

`Draw(NkRenderTarget&, const NkRenderStates&)` est l'**override** de `NkDrawable` et la **seule**
méthode non-inline de `NkShape` (implémentée dans le `.cpp`). Elle réalise tout le travail : la
triangulation **fan** colorée par `fillColor`, le contour en `NK_LINES` coloré par `outlineColor`,
et l'application texture/UV via `GetPointUV`. On ne l'appelle généralement pas directement : on écrit
`target.Draw(shape)`, et la cible appelle `Draw` pour nous.

- **Rendu / 2D** — point d'entrée unique pour mettre une forme à l'écran, frame après frame.
- **Threading** — aucune thread-safety déclarée ; le dessin appartient au thread de rendu.

### `NkRectangleShape` à fond

Le rectangle a quatre coins fixes partant de `(0,0)`, dimensionnés par `SetSize`. L'ordre TL→TR→BR→BL
garantit une triangulation fan correcte, et `GetPoint` est `O(1)` (un `switch`). Étant convexe par
construction, il rend **toujours** bien. C'est le cheval de trait du 2D :

- **Gameplay / 2D** — raquettes, plateformes, murs, projectiles carrés, barres de vie.
- **UI** — panneaux, boutons, séparateurs, fonds colorés.
- **Outils / éditeur** — cadres, zones de *drop*, surbrillances rectangulaires.

Header-only, un seul membre (`mSize`), aucune allocation : on peut en créer des milliers sans coût
mémoire dynamique.

### `NkCircleShape` à fond

Le cercle est un **polygone régulier** : `segments` sommets sur un rayon `radius`. Le nombre de
segments est le curseur lissé/coût — 30+ pour une taille moyenne, 64 pour un grand cercle ; `GetPoint`
calcule chaque sommet en `O(1)` par trigonométrie. Le centre placé en `(radius, radius)` (et non
`(0,0)`) aligne la bounding box sur `(0,0)`, ce qui rend le cercle cohérent avec le rectangle —
mais impose `SetOrigin({radius, radius})` pour pivoter/positionner depuis le centre. Usages :

- **Gameplay / IA** — cercles de **portée** (aggro, soin, explosion), indicateurs de cible, balle de
  Pong, particules rondes.
- **UI** — boutons radio, jauges circulaires, points de menu.
- **Physique** — visualiser un *collider* circulaire (le rendu approxime, le test reste analytique
  côté physique).
- **Outils / éditeur** — *handles* de gizmo, marqueurs de points.

Header-only (deux scalaires), aucune allocation.

### `NkConvexShape` à fond

C'est la seule forme dont la géométrie est **fournie par l'utilisateur** : on déclare un nombre de
sommets et on les place un à un. Le stockage est un `NkVector<NkVec2f>`, donc **l'allocation passe
par NKMemory** — et c'est pourquoi son constructeur paramétré, `SetPointCount` et `SetPoint` ne sont
**pas `noexcept`** (à la différence des autres formes, intégralement header-only sans allocation).
`SetPoint` ignore en silence un index hors bornes, et `GetPoint` est borné (`{0,0}` si dépassement),
ce qui évite tout accès invalide. Le **contrat de convexité** est strict : la triangulation fan ne
rend juste que pour un polygone convexe ; un polygone concave s'auto-intersecte. Usages :

- **Rendu / 2D** — triangles, losanges, flèches, pentagones, formes d'éditeur arbitraires (tant
  qu'elles restent convexes).
- **Gameplay** — zones de déclenchement polygonales, champs de vision approximés en éventail.
- **Outils / éditeur** — primitives convexes dessinées à la souris ; pour une forme **concave**,
  trianguler côté app (*ear-clipping*) et dessiner via `NkVertexArray`.

### `NkLineShape` à fond

`NkLineShape` matérialise un **segment épais** : un quad de 4 coins dont l'axe long suit AB. Tout
*setter* (`SetA`, `SetB`, `SetEndpoints`, `SetThickness`) relance `Recompute()`, qui calcule la
normale unitaire (rotation 90° de la direction, via `math::NkSqrt`), l'offset `thickness/2`, et
ordonne les coins `A-off, B-off, B+off, A+off`. Le cas dégénéré `A == B` est géré : les 4 coins
tombent sur A, sans NaN. Le transform hérité s'applique **en plus** du calcul AB, donc on peut faire
tourner ou mettre à l'échelle le segment après création. Usages :

- **Rendu / debug** — tracer des **rayons**, des vecteurs (vitesse, force, normale), des connexions
  entre nœuds, des trajectoires.
- **Gameplay / IA** — lignes de visée, liens de *graph* de navigation, câbles, faisceaux laser.
- **Outils / éditeur** — règles, guides, axes, segments de mesure.

Pour un trait **fin de 1 px**, préférer `NkRenderer2D::DrawLine` (un segment `LINE`), plus efficient
que ce quad triangulé. Header-only, sans allocation (scalaires + `mCorners[4]`).

### Idiomes et pièges transverses

- **Le flux SFML** — créer la forme → régler style/transform → `target.Draw(shape)`. `Draw` est
  l'interface `NkDrawable` ; on n'appelle jamais la triangulation à la main.
- **Local vs monde** — `GetPoint` est **local** ; position/rotation/échelle/origine viennent de
  `NkTransformable`. Pour la boîte transformée, utiliser `GetGlobalBounds()`.
- **Texture non-owning** — `SetTexture` stocke un `const NkTexture*` brut ; l'appelant garantit la
  durée de vie. Pas de Create/Destroy ici.
- **textureRect collant** — appeler `SetTextureRect` bascule définitivement en mode UV explicite ;
  aucune méthode pour le désactiver.
- **Triangulation fan = convexes seulement** — Rectangle/Circle/Line sont convexes par
  construction ; `NkConvexShape` ne rend juste que s'il reste convexe. Concave → `NkVertexArray`.
- **`NkConvexShape` alloue** — via `NkVector`/NKMemory : ses ctor/`SetPointCount`/`SetPoint` ne sont
  pas `noexcept`. Les autres formes sont header-only sans allocation (tableaux fixes / scalaires).
- **Centrage du cercle** — centre en `(r, r)` : poser `SetOrigin({r, r})` pour pivoter/positionner
  depuis le centre.
- **Contour brut** — `NK_LINES` sans joints ni *caps* (pas de round/bevel), pas d'antialiasing
  dédié (dépend du MSAA du contexte).
- **Threading / GPU** — aucune thread-safety déclarée ; le rendu GPU vit dans `NkShape::Draw`
  (impl `.cpp`).

---

### Exemple

```cpp
#include "NKCanvas/Renderer/Shapes/NkRectangleShape.h"
#include "NKCanvas/Renderer/Shapes/NkCircleShape.h"
#include "NKCanvas/Renderer/Shapes/NkConvexShape.h"
#include "NKCanvas/Renderer/Shapes/NkLineShape.h"
using namespace nkentseu::renderer;

// Rectangle : une raquette de Pong, pivot au centre.
NkRectangleShape paddle({ 20.f, 100.f });
paddle.SetFillColor(NkColor2D::White);
paddle.SetOrigin({ 10.f, 50.f });
paddle.SetPosition({ 40.f, 360.f });

// Cercle : un halo de portée, origine au centre pour le placer pile.
NkCircleShape range(48.f, 64);
range.SetOrigin({ 48.f, 48.f });
range.SetFillColor(NkColor2D::Yellow);
range.SetOutlineColor(NkColor2D::Black);
range.SetOutlineThickness(2.f);

// Polygone convexe : un triangle de visée (alloue via NKMemory).
NkConvexShape arrow(3);
arrow.SetPoint(0, { 0.f,  0.f });
arrow.SetPoint(1, { 40.f, 20.f });
arrow.SetPoint(2, { 0.f, 40.f });
arrow.SetFillColor(NkColor2D::Cyan);

// Segment épais : un rayon de debug.
NkLineShape ray({ 100.f, 100.f }, { 400.f, 250.f }, 4.f);
ray.SetFillColor(NkColor2D::Red);

// Rendu : un appel par forme.
target.Draw(paddle);
target.Draw(range);
target.Draw(arrow);
target.Draw(ray);
```

---

[← Index NKCanvas](README.md) · [Récap NKCanvas](../NKCanvas.md) · [Couche Runtime](../README.md)
