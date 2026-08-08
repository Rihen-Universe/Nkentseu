# Définir sa grille en C++

Le chapitre précédent décrivait un plateau dans un fichier JSON. Ce chapitre
montre l'autre voie — et corrige au passage un malentendu répandu.

## 5.1 Le malentendu : « je suis obligé de passer par du JSON »

Non. **Vous ne l'avez jamais été.** Le contrat ne vous impose ni format de
plateau, ni fichier, ni même l'existence d'un fichier. Regardez ce que
`LoadBoardJson` vaut dans l'exemple minimal :

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — V_LoadBoardJson`**

```cpp
/// Plateau fige : on REFUSE tout chargement. L'atelier l'affiche proprement
/// (« le moteur a REFUSE ce plateau ») au lieu de faire semblant.
int32 V_LoadBoardJson(NkcRules, const char *) { return 0; }
```

Ce module refuse tout JSON, et il fonctionne parfaitement dans l'atelier. Son
plateau est construit en C++, dans `Create` :

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — V_Create`**

```cpp
for (int32 y = 0; y < kSide; ++y)
    for (int32 x = 0; x < kSide; ++x) {
        R->coords[y * kSide + x].q = static_cast<int16>(x);
        R->coords[y * kSide + x].r = static_cast<int16>(y);
    }
```

> **✅ Trois choses ont TOUJOURS été à vous**
>
> - **La forme du plateau** — quelles coordonnées existent : c'est le tableau
>   `coords` que vous exposez dans `GetView`. Une coordonnée absente est du
>   hors-plateau, point.
> - **Le voisinage** — qui touche qui : c'est *votre* code dans
>   `GenerateLegalMoves`. `ConquerorGeometry.h` n'est qu'une **commodité**, pas
>   une obligation — vous pouvez ne pas l'inclure.
> - **Les cases bloquées** — c'est `kCellBlocked` dans le champ `owner` de la
>   vue. Rien d'autre.
>
> Le JSON n'existe que pour qu'un *designer* — quelqu'un qui ne compile pas —
> puisse essayer vingt formes dans l'après-midi.

## 5.2 Ce qui vous échappait vraiment : la projection écran

Une seule chose ne vous appartenait pas : **où l'atelier dessine vos cellules, et
quelle forme il leur donne**. Il le déduisait de `topology`, donc de quatre
valeurs possibles — deux hexagones, deux carrés.

Conséquence : un plateau que vos règles savaient parfaitement jouer pouvait être
**impossible à afficher**. Un plateau circulaire, en triangles, en octogones, à
cellules de tailles inégales : le moteur tournait, l'écran mentait.

L'ABI 3 rend la main au module.

**`Applications/ConquerorLab/include/Conqueror/ConquerorRulesABI.h — bloc géométrie`**

```cpp
// TOUT CE BLOC EST OPTIONNEL : laisser a nullptr fait retomber l'atelier
// sur la projection standard de `topology`.

/// Centre de la cellule `c`, en unites de cellule (1.0 = un « pas » de
/// grille). L'atelier cadre et met a l'echelle ensuite.
int32 (*GetCellCenter)(NkcRules self, NkcCoord c, float32 *outXY) = nullptr;

/// Contour de la cellule `c` : `capPoints` paires (x, y) au plus, en unites
/// de cellule, RELATIVES au centre. Renvoie le nombre de points ecrits.
uint32 (*GetCellShape)(NkcRules self, NkcCoord c, float32 *outXY,
                       uint32 capPoints) = nullptr;
```

> **⚠️ Ces flottants ne violent pas « zéro flottant »**
>
> L'exigence §17.1 porte sur la **logique de règles** : ce qui décide d'un coup,
> alimente un état, entre dans une empreinte. Ces deux fonctions ne font que de
> la **présentation** — l'atelier les appelle pour dessiner, jamais pour jouer.
>
> Le repère est simple : **si votre `HashState` ne les voit pas, elles sont du
> bon côté de la frontière.**

## 5.3 Un plateau circulaire, entièrement en C++

Le fichier : `Applications/ConquerorLab/exemples/rules/GrilleLibre.cpp`. Trois
anneaux concentriques, 19 cellules en secteurs. Ni hexagone, ni carré.

**`exemples/rules/GrilleLibre.cpp — la forme, en entiers`**

```cpp
constexpr int32 kRings            = 3;
constexpr int32 kRingSize[kRings] = {1, 6, 12};
constexpr int32 kCells            = 1 + 6 + 12;   // 19
constexpr int32 kRingStart[kRings] = {0, 1, 7};   // index du 1er de chaque anneau

int32 IndexOf(NkcCoord c) {
    if (c.q < 0 || c.q >= kRings) return -1;
    if (c.r < 0 || c.r >= kRingSize[c.q]) return -1;
    return kRingStart[c.q] + c.r;
}
```

Une coordonnée vaut ici `(q = anneau, r = index dans l'anneau)`. C'est *notre*
convention : le contrat ne dit rien du sens de `q` et `r` hors topologie
standard, et c'est exactement ce qui rend la chose possible.

### Le voisinage, défini par nous

**`exemples/rules/GrilleLibre.cpp — Neighbors`**

```cpp
uint32 Neighbors(NkcCoord c, NkcCoord *out, uint32 cap) {
    uint32 n = 0;
    auto   push = [&](int32 ring, int32 idx) {
          if (ring < 0 || ring >= kRings) return;
          const int32 sz = kRingSize[ring];
          const int32 r  = ((idx % sz) + sz) % sz;   // modulo circulaire
          if (n < cap) { out[n].q = (int16)ring; out[n].r = (int16)r; }
          ++n;
    };

    if (c.q == 0) {                     // le centre touche tout l'anneau 1
        for (int32 i = 0; i < kRingSize[1]; ++i) push(1, i);
        return n;
    }
    push(c.q, c.r - 1);                 // voisin arriere sur l'anneau
    push(c.q, c.r + 1);                 // voisin avant  sur l'anneau
    if (c.q == 1) {
        push(0, 0);                     // vers le centre
        push(2, c.r * 2);               // vers l'exterieur : DEUX cellules
        push(2, c.r * 2 + 1);
    } else {                            // anneau 2
        push(1, c.r / 2);               // vers l'interieur : UNE cellule
    }
    return n;
}
```

C'est le cœur de la démonstration : `ConquerorGeometry.h` n'est même pas inclus
dans ce fichier. Aucune topologie du contrat ne sait exprimer « le centre touche
tout l'anneau 1 », ni « une cellule de l'anneau 1 touche deux cellules de
l'anneau 2 ». Ici, si — et le tout reste **entièrement entier**, donc le rejeu
bit-à-bit tient.

> **✅ L'ordre reste normatif**
>
> `push` est appelé dans un ordre fixe, et c'est lui qui fixe l'ordre de
> génération des coups. Ne le réordonnez pas.

### Où dessiner : `GetCellCenter`

**`exemples/rules/GrilleLibre.cpp — V_GetCellCenter`**

```cpp
int32 V_GetCellCenter(NkcRules, NkcCoord c, float32 *outXY) {
    if (!outXY) return 0;
    if (c.q < 0 || c.q >= kRings) return 0;
    if (c.q == 0) { outXY[0] = 0.f; outXY[1] = 0.f; return 1; }
    const float32 a = CellAngle(c);
    const float32 R = static_cast<float32>(c.q);   // anneau 1 -> rayon 1
    outXY[0] = R * std::cos(a);
    outXY[1] = R * std::sin(a);
    return 1;
}
```

Des coordonnées polaires, en **unités de cellule**. Vous ne vous souciez ni du
zoom, ni des pixels, ni de la taille de la fenêtre : l'atelier calcule la boîte
englobante réelle de vos centres et met tout à l'échelle.

Renvoyer `0` signifie « je ne sais pas placer celle-ci » — l'atelier retombe alors
sur la topologie **pour cette cellule seulement**. Vous pouvez donc ne
personnaliser qu'une partie du plateau.

### Quelle forme : `GetCellShape`

**`exemples/rules/GrilleLibre.cpp — V_GetCellShape, secteur d'anneau`**

```cpp
// 4 points sur l'arc exterieur, puis 4 sur l'interieur en sens inverse.
for (uint32 i = 0; i < seg; ++i) {
    const float32 t = static_cast<float32>(i) / static_cast<float32>(seg - 1);
    const float32 a = a0 - half + 2.f * half * t;
    outXY[n * 2]     = (R + 0.46f) * std::cos(a) - cx;
    outXY[n * 2 + 1] = (R + 0.46f) * std::sin(a) - cy;
    ++n;
}
for (uint32 i = 0; i < seg; ++i) {
    const float32 t = static_cast<float32>(seg - 1 - i) / static_cast<float32>(seg - 1);
    const float32 a = a0 - half + 2.f * half * t;
    outXY[n * 2]     = (R - 0.46f) * std::cos(a) - cx;
    outXY[n * 2 + 1] = (R - 0.46f) * std::sin(a) - cy;
    ++n;
}
```

Un polygone **relatif au centre**, en unités de cellule. Huit points suffisent à
faire un secteur d'anneau convaincant ; `kMaxCellPoints` vaut 12.

> **⚠️ Le polygone doit rester convexe**
>
> L'atelier remplit vos cellules par un éventail de triangles depuis le premier
> sommet — c'est la primitive dont dispose `NkGuiDrawList`, qui n'a pas de
> remplissage de polygone quelconque. Un contour concave se remplira de travers.
>
> Un secteur d'anneau assez fin passe très bien ; une cellule en U, non.

### Le déclarer aux autres : `GetNeighbors`

Votre voisinage est écrit, il marche, vos coups sont bons. Il reste un problème :
**personne d'autre ne le connaît**.

- une IA qui évalue une position (« combien d'ennemis touche cette case ? »)
  n'avait que `view.topology` pour le deviner. Sur une grille libre, elle
  devinait **faux, en silence** ;
- l'atelier ne pouvait pas montrer le voisinage à l'écran, donc un stagiaire qui
  se trompait d'adjacence n'avait aucun moyen de le **voir**.

D'où une troisième entrée, et celle-ci est **entièrement entière** — c'est de la
règle, pas du dessin :

**`exemples/rules/GrilleLibre.cpp — V_GetNeighbors`**

```cpp
/// Le voisinage, DECLARE. Sans cette entree, une IA qui evalue une position
/// sur ce plateau devinerait l'adjacence a partir de `topology` — et
/// devinerait faux, en silence.
uint32 V_GetNeighbors(NkcRules, NkcCoord c, NkcCoord *out, uint32 cap) {
    return Neighbors(c, out, cap);
}
```

Une ligne, puisque la fonction existait déjà. Vérifié :

```
GetNeighbors(0,0) -> 6 voisins : (1,0) (1,1) (1,2) (1,3) (1,4) (1,5)
```

Le centre touche tout l'anneau 1 — aucune topologie du contrat ne sait exprimer
cela.

> **✅ Le bouton « Voisinage » du panneau Plateau**
>
> Il trace, au survol, les liens de la case vers ses voisins. C'est le **seul
> moyen de vérifier une adjacence à l'œil**, et il lit `GetNeighbors`. Sans lui,
> une erreur d'adjacence se découvre par un coup légal inexplicable, trois heures
> plus tard.

## 5.4 Comment l'atelier s'y adapte

Tout passe par un objet minuscule, reconstruit à chaque image :

**`Applications/ConquerorLab/src/ConquerorLab/NkcBoardRender.h — le projecteur`**

```cpp
struct NkcProjector {
        const NkcRulesVTable *vt       = nullptr;
        NkcRules              inst     = nullptr;
        NkcTopology           topology = NkcTopology::HexPointy;
        bool                  custom   = false;  // le module declare sa geometrie
};
```

> **✅ Pourquoi il n'a aucun état persistant**
>
> Le rendre persistant obligerait à l'invalider quand le module change — et c'est
> exactement le genre d'invalidation qu'on oublie. Le reconstruire coûte deux
> affectations et une comparaison de pointeur.

Une conséquence à connaître, côté picking :

**`Applications/ConquerorLab/src/ConquerorLab/NkcBoardRender.h — ProjPickCell`**

```cpp
// Deux chemins, et c'est assume :
//   - geometrie standard : inversion analytique en O(1) (arrondi cube),
//     exacte jusqu'aux aretes ;
//   - geometrie du module : aucune inverse n'existe, on cherche le centre
//     le plus proche parmi les cases. O(n) sur quelques dizaines de cases
//     ne se mesure pas, et c'est la seule methode qui marche pour une
//     grille quelconque.
```

Le clic sur une grille libre désigne donc la cellule dont le *centre* est le plus
proche, à condition d'être à portée. Sur des cellules très allongées, le picking
peut sembler légèrement décalé près des bords : c'est le prix d'une géométrie
arbitraire, et il est assumé.

## 5.5 JSON ou C++ : lequel choisir

| Prenez le **JSON** quand… | Prenez le **C++** quand… |
|---|---|
| la forme se décrit par une liste de coordonnées | la forme se décrit par une *formule* (anneaux, spirale, fractale) |
| un designer doit pouvoir l'essayer sans compiler | le voisinage n'est pas géométrique (portails, tore, graphe quelconque) |
| vous voulez vingt variantes de la même idée | les cellules n'ont ni la même taille ni la même forme |

Rien n'interdit les deux : votre `LoadBoardJson` peut lire la liste des cases,
pendant que `GetCellCenter` en calcule la position. C'est même souvent le bon
partage — la *forme* en donnée, la *projection* en code.

## 5.6 État vérifié

**`Banc d'essai, 2026-08-07 — GrilleLibre contre IAMinimale`**

```
       regles : GrilleLibre (3 anneaux) 1.0.0 (palier 0)
       IA     : IAMinimale 1.0.0
       plateau : 19 cases, 2 joueurs
  OK   l'IA n'a jamais produit de coup illegal
  OK   la partie se termine d'elle-meme        -> 17 coups, vainqueur 0, 10/9
  OK   le journal se rejoue integralement
  OK   rejeu deterministe : empreinte identique -> 15623799979863270834
  OK   « aucun coup legal » equivaut a « joueur bloque »
=== 13 verifications OK, 3 echecs ===
```

Les trois échecs sont **attendus** : le banc d'essai vérifie en dur le plateau du
moteur de *référence* (« 42 cases », « 2 totems par joueur », et la présence du
paramètre `portee_duplication`). Un plateau de 19 cellules en anneaux n'a aucune
de ces propriétés. Tout ce qui porte sur le **contrat** passe.

> **✅ Une leçon de méthode, au passage**
>
> Un test qui code en dur les valeurs d'une implémentation particulière n'est pas
> un test du contrat. Si vous reprenez `tests/NkcAbiHarness.cpp` pour votre
> module, ces trois vérifications-là sont à adapter — les treize autres, non.

## Exercices

> **✏️ 1 — Le tore**
>
> Partez de `RegleMinimale.cpp` et rendez son carré 5×5 *torique* : la colonne 4
> touche la colonne 0, la ligne 4 touche la ligne 0. Vous n'avez à toucher que la
> génération des coups. Le plateau reste affiché à plat — que manque-t-il pour
> *voir* le repliement ? Est-ce grave ?

> **✏️ 2 — Le triangle**
>
> Écrivez un plateau en cellules **triangulaires** : un grand triangle subdivisé,
> où chaque petite cellule touche ses trois voisines de côté. Vous aurez besoin
> des deux orientations (pointe en haut, pointe en bas) — c'est le vrai exercice.
> `GetCellShape` doit renvoyer trois points.

> **✏️ 3 — La question de fond**
>
> `GrilleLibre` déclare `supportsHex = 0` et `supportsSquare = 0`. Cherchez dans
> l'atelier qui lit ces deux champs. Que faudrait-il en faire ? Proposez une
> réponse en trois lignes — et vérifiez si le code actuel la respecte.
