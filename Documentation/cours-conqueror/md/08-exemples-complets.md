# Exemples complets — l'échelle, barreau par barreau

Les chapitres précédents expliquaient le contrat. Celui-ci donne **du code qui
tourne**, du plus court au plus complet, pour chacune des trois choses qu'on
écrit : des règles, une IA, un plateau.

> **Ne sautez pas de barreau.** Chaque exemple existe parce que le suivant serait
> incompréhensible sans lui. `IANegamax.cpp` fait cinq cents lignes ; il en fait
> cent pour qui a d'abord lu `IAGloutonne.cpp`, et mille pour qui ne l'a pas lu.

## 8.1 Les trois échelles

| Écrire… | Le plus court | L'intermédiaire | Le complet |
|---|---|---|---|
| **une IA** | `ai/IAMinimale.cpp`<br>≈ 120 l. — tire au hasard | `ai/IAFacile.cpp`<br>≈ 140 l. — un coup d'avance,<br>couche confortable | `ai/IANegamax.cpp`<br>≈ 500 l. — négamax αβ |
| **des règles** | `rules/RegleFacile.cpp`<br>≈ 110 l. — trois fonctions | `rules/RegleContratNu.cpp`<br>≈ 560 l. — contrat nu | `modules/rules/ConquerorRulesV2.cpp`<br>le moteur de référence |
| **un plateau** | `boards/mini_3x3.json`<br>9 cases | `boards/hexagone_6x7.json`<br>42 cases | `rules/GrilleLibre.cpp`<br>en C++, projection comprise |

Tous sont dans `exemples/`, sauf le moteur de référence. Tous compilent, et les
duels ci-dessous ont été **joués**, pas supposés.

---

## 8.2 D'abord : la couche confortable

Le contrat est fait pour être **stable** — structures plates, pointeurs bruts,
aucune allocation. C'est ce qu'il faut pour une frontière binaire, et c'est
pénible à lire.

Compter « les ennemis autour de mes totems » avec le contrat nu :

```cpp
NkcStateView v;
std::memset(&v, 0, sizeof(v));
rules->GetView(inst, st, &v);
for (uint32 i = 0; i < v.cellCount; ++i) {
    if (v.cells[i].owner != (int8)moi) continue;
    NkcCoord nb[16];
    const uint32 n = rules->GetNeighbors(inst, v.coords[i], nb, 16);
    for (uint32 k = 0; k < n; ++k)
        for (uint32 j = 0; j < v.cellCount; ++j)      // recherche a la main
            if (v.coords[j].q == nb[k].q && v.coords[j].r == nb[k].r) {
                if (v.cells[j].owner >= 0 && v.cells[j].owner != (int8)moi) ++c;
                break;
            }
}
```

Avec `Conqueror/ConquerorFacile.h` :

```cpp
Plateau p(*rules, inst, st);
for (Case c : p)
    if (c.AMoi(moi))
        contact += p.CompteVoisins(c.ou, Voisin::Ennemi, moi);
```

### Ce qu'elle apporte

| Classe | À quoi ça sert |
|---|---|
| `Plateau` | lire l'état : `QuiJoue()`, `Totems(j)`, `Avantage(moi)`, `A(coord)`, `Voisins()`, `CompteVoisins()` — et `for (Case c : p)` |
| `Case` | une case **avec sa coordonnée** : `Vide()`, `Bloquee()`, `AMoi(j)`, `Ennemie(j)` |
| `Coups<N>` | les coups légaux, itérables : `for (const NkcMove &m : liste)` |
| `Essai` | jouer un coup **pour de faux** : clone, applique, libère tout seul |

### Trois promesses

> **✅ Rien de nouveau**
>
> Chaque fonction appelle le contrat, et rien d'autre. Tout ce qu'on fait avec ce
> fichier, on peut le faire sans — en plus long. Il n'y a donc **rien à
> désapprendre** le jour où vous passerez au contrat nu.

> **✅ Tout est inline, aucune allocation**
>
> Aucun symbole à lier, rien qu'un compilateur en `-O2` ne supprime. Un `Plateau`
> tient en quelques pointeurs : le construire dans une boucle ne coûte rien.

> **⚠️ Un piège quand même**
>
> Créez l'`Essai` **hors** de la boucle des coups candidats. En créer un par
> candidat coûte une allocation d'état par candidat — et c'est exactement la
> boucle chaude d'une IA.

### La preuve : la même IA, deux fois

`exemples/ai/IAFacile.cpp` est **le même algorithme** que `IAGloutonne.cpp`,
écrit avec la couche.

```
IAGloutonne.cpp   contrat nu           ~240 lignes
IAFacile.cpp      ConquerorFacile.h    ~140 lignes
```

Vérifié en les faisant jouer l'une contre l'autre, 40 parties, côtés inversés :

```
IAFacile   vs  IAGloutonne     20 - 20     50 %     <- comportement identique
IAFacile   vs  IAMinimale      40 -  0    100 %
```

Le 20–20 est le résultat **attendu** de deux joueurs identiques. C'est ce qui
prouve que la couche ne change rien au jeu — seulement à ce qu'on écrit.

> **Par où commencer.** Lisez `IAFacile.cpp` d'abord : sa partie qui *décide*
> tient en quinze lignes. Lisez `IAGloutonne.cpp` ensuite, pour voir ce que la
> couche vous épargne, et pour savoir quoi faire le jour où elle ne suffira plus.

---

## 8.3 Une IA, en trois temps

### Temps 1 — tirer au hasard (`IAMinimale.cpp`)

Elle ne réfléchit pas. Son seul mérite est de montrer **la forme d'un module
d'IA** : neuf fonctions, dont la plupart ne font rien.

**`exemples/ai/IAMinimale.cpp — le cœur`**

```cpp
const uint32 total = rules->GenerateLegalMoves(inst, st, a->moves, kMaxMoves);
const uint32 n     = total < kMaxMoves ? total : kMaxMoves;
if (n == 0) return 0;              // aucun coup : l'atelier jouera PASSER
out->move = a->moves[Rand(a) % n];
return 1;
```

Retenez surtout ce qu'elle **ne fait pas** : elle ne construit pas de coup. Elle
**demande** les coups légaux au moteur et en choisit un. Une IA qui fabrique ses
propres coups produit tôt ou tard un coup illégal, et l'atelier le signale.

### Temps 2 — regarder un coup (`IAGloutonne.cpp`)

Le vrai premier pas. L'algorithme tient en quatre lignes :

```
pour chaque coup légal :
    cloner l'état, jouer le coup pour de faux
    donner une note à la position obtenue
garder le coup dont la note est la meilleure
```

**`exemples/ai/IAGloutonne.cpp — jouer pour de faux`**

```cpp
for (uint32 i = 0; i < n; ++i) {
    // JOUER POUR DE FAUX : on clone, on applique, on note. L'etat recu
    // n'est JAMAIS modifie -- le contrat l'interdit, et l'atelier compte
    // dessus pour faire tourner plusieurs parties en parallele.
    rules->CloneState(inst, a->essai, st);
    if (!rules->ApplyMove(inst, a->essai, &a->moves[i], nullptr, nullptr))
        continue;   // le moteur a refuse : il a toujours le dernier mot

    const int32 note = Noter(rules, inst, a->essai, moi, a->poidsTotems);

    // `>` et non `>=` : a note egale on garde le PREMIER. L'ordre des
    // coups est fixe par le moteur, donc ce choix est REPRODUCTIBLE --
    // c'est ce qui permet de rejouer une partie a l'identique.
    if (premier || note > meilleurNote) { meilleurNote = note; meilleur = i; premier = false; }
}
```

L'évaluation est volontairement pauvre — un seul critère, le nombre de totems :

**`exemples/ai/IAGloutonne.cpp — Noter`**

```cpp
int32 miens = 0, siens = 0;
for (uint8 p = 0; p < v.playerCount; ++p) {
    if (p == moi) miens += v.totemCount[p];
    else          siens += v.totemCount[p];
}
return (miens - siens) * poids;
```

> **✅ Ce qu'elle ne sait pas faire, et c'est le point**
>
> Elle ne voit pas la **réponse** de l'adversaire. Un coup qui gagne trois totems
> et en offre cinq au coup suivant lui paraît excellent.
>
> C'est exactement le manque que négamax comble. Le meilleur moyen de comprendre
> pourquoi négamax existe est d'avoir d'abord vu jouer celle-ci.

### Temps 3 — regarder loin (`IANegamax.cpp`)

Quatre pièces, dans l'ordre où elles comptent : **évaluer**, **négamax**,
**alpha-bêta**, **approfondissement itératif**.

#### Pourquoi négamax et pas minimax

Minimax écrit deux fonctions, une qui maximise et une qui minimise. Négamax n'en
écrit qu'une, en observant que :

```
min(a, b) == -max(-a, -b)
```

La valeur d'une position pour le joueur au trait est l'opposé de sa valeur pour
l'autre. On évalue **toujours du point de vue du joueur au trait**, et on renvoie
`-Negamax(...)` à l'appelant.

**`exemples/ai/IANegamax.cpp — la boucle centrale`**

```cpp
// L'enfant rend SA valeur, du point de vue de SON joueur au trait. On
// la ramene au point de vue de `mover` : identique si c'est le meme
// joueur qui rejoue (coup gratuit, tour multiple), opposee sinon.
if (after.current == mover) {
    score = Negamax(ai, rules, inst, ai->work[ply], depth - 1, alpha, beta, ply + 1);
} else {
    score = -Negamax(ai, rules, inst, ai->work[ply], depth - 1, -beta, -alpha, ply + 1);
}

if (ai->outOfTime) return 0;
if (score > best) best = score;
if (best > alpha) alpha = best;
if (alpha >= beta) break;    // COUPURE BETA
```

> **⚠️ L'erreur de signe — elle s'est produite ici, en écrivant ce fichier**
>
> La première version évaluait toujours du point de vue du joueur **racine**,
> tout en négligeant à chaque changement de trait. Les deux conventions se
> contredisaient.
>
> **Rien ne plantait.** Aucun test ne tombait. L'IA cherchait simplement, très
> efficacement, le **pire** coup — et perdait **16 à 20 contre un joueur
> aléatoire**.
>
> Le remède n'a pas été de corriger un signe, mais de **supprimer la
> possibilité de l'erreur** : le paramètre `me` a disparu de `Negamax`. Tant
> qu'une fonction porte à la fois « le joueur racine » et « le joueur au trait »,
> il existe un endroit où l'on peut confondre les deux, et il suffit d'un.

#### Alpha-bêta

`alpha` est le meilleur score que le joueur au trait s'est déjà garanti ; `beta`
celui que l'adversaire s'est garanti ailleurs. Dès que `score >= beta`, on
arrête : l'adversaire ne laissera jamais la partie arriver ici.

Le résultat est **exactement** celui du négamax nu ; seul le nombre de nœuds
change. Et cette économie dépend entièrement de l'**ordre** des coups — d'où le
tri, qui est un paramètre pour qu'on puisse mesurer ce qu'il rapporte.

#### Approfondissement itératif

On cherche à profondeur 1, on garde le meilleur coup. Puis 2. Puis 3.

**`exemples/ai/IANegamax.cpp — Search`**

```cpp
// LE POINT QUI FAIT TOUT : on ne garde le resultat d'un niveau que
// s'il a ete PARCOURU EN ENTIER. Un niveau interrompu a examine les
// premiers coups seulement, et le « meilleur » qu'il propose est le
// meilleur d'un echantillon arbitraire.
if (ai->outOfTime) break;
best      = levelBestMove;
bestScore = levelBest;
reached   = depth;
```

Cela paraît du gaspillage — on refait le travail à chaque fois. Ce n'en est pas :
le dernier niveau coûte plus que tous les précédents réunis. Et c'est **la seule
façon de respecter un budget de temps** : sans lui, il faudrait deviner la
profondeur atteignable, et se tromper signifie soit figer l'interface, soit ne
rien chercher.

#### La limite assumée

`IANegamax` détecte les parties à plus de deux joueurs et **le dit** :

```cpp
NKC_LOG_WARN("plus de 2 joueurs : negamax ne s'applique pas, "
             "recherche limitee a 1 coup");
```

L'identité `min(a,b) == -max(-a,-b)` suppose un jeu à somme nulle **à deux**. À
trois, « le gain de l'un est la perte de l'autre » est faux : il y a deux autres.
Traiter le multijoueur demande max^n ou une recherche paranoïaque, et c'est un
autre sujet.

### Ce que ça donne, mesuré

Banc de duel, 40 parties, côtés inversés une sur deux, budget 60 ms par coup,
moteur `ConquerorRulesV2` :

```
Gloutonne  vs  IAMinimale (aleatoire)     40 - 0     100 %
Negamax    vs  Gloutonne                  40 - 0     100 %
Negamax    vs  IAMinimale                 40 - 0     100 %

controles :
Gloutonne  vs  Gloutonne                  20 - 20     50 %
IAMinimale vs  IAMinimale                 20 - 20     50 %
```

> **✅ Pourquoi les deux dernières lignes comptent plus que les trois premières**
>
> Un banc qui donne toujours 40–0 est un banc cassé. Les contrôles à 20–20
> prouvent qu'il n'y a **ni biais de siège, ni bug de comptage** — et que les
> 100 % au-dessus mesurent bien les IA.
>
> C'est le premier réflexe à prendre : avant de croire un chiffre, faire tourner
> le cas où l'on connaît déjà la réponse.

---

## 8.4 Des règles : trois fonctions, ou vingt et une

`NkcRulesVTable` compte **vingt et une** fonctions. Sur ces vingt et une, **trois**
contiennent votre jeu :

| Fonction | Ce qu'elle dit |
|---|---|
| `Construire` | à quoi la partie ressemble au départ |
| `CoupsPossibles` | ce qu'on a le droit de faire |
| `Appliquer` | ce qui se passe quand on le fait |

Les dix-huit autres sont les mêmes pour tout le monde. `ConquerorRegleFacile.h`
les écrit une fois :

```
RegleContratNu.cpp   contrat nu                 ~560 lignes
RegleFacile.cpp     ConquerorRegleFacile.h     ~110 lignes
```

**`exemples/rules/RegleFacile.cpp — le jeu entier`**

```cpp
struct RegleFacile {
    void Construire(Grille &g) {
        g.topologie = NkcTopology::Square4;
        g.nbJoueurs = 2;
        g.AjouterRectangle(5, 5);
        g.PoserAuxCoins(2);       // symétrie §4.2
    }

    void CoupsPossibles(const Partie &p, ListeCoups &out) {
        NkcCoord voisins[8];
        for (int32 i = 0; i < p.nbCases; ++i) {
            if (p.cases[i].owner != (int8)p.joueur) continue;
            const int32 n = p.Voisins(p.ou[i], voisins, 8);
            for (int32 k = 0; k < n; ++k)
                if (p.Vide(voisins[k]))
                    out.Dupliquer(p.joueur, p.ou[i], voisins[k]);
        }
    }

    void Appliquer(Partie &p, const NkcMove &m, Evenements &ev) { /* ... */ }
};
NKC_REGLES(RegleFacile, "RegleFacile", "1.0.0", "Cours ConquerorLab")
```

### Pourquoi les dix-huit autres deviennent gratuites

Parce que **l'état cesse d'être libre**. Le contrat dit « le module choisit sa
représentation interne » — c'est sa force, et c'est ce qui coûte cher : tant que
la structure est inconnue, personne ne peut écrire sa sérialisation ni son
empreinte à votre place.

`Partie` est une structure **fixe, plate, sans pointeur**. Du coup :

| Fonction | Devient |
|---|---|
| `SerializeState` | un `memcpy` |
| `CloneState` | une affectation |
| `HashState` | FNV-1a sur les octets — donc identique sur toute plateforme |
| `IsLegalMove` | énumérer et comparer |

Et ces quatre-là sont correctes **par construction**, pas par relecture.

> **✅ Deux pièges classiques disparaissent**
>
> `ListeCoups::Dupliquer` fait le `memset` du coup avant de le remplir — l'erreur
> numéro un des premiers modules, puisque le contrat compare les coups **octet
> par octet**.
>
> Et `PASSER` est ajouté automatiquement quand la liste reste vide, jamais
> autrement : c'est exactement ce qu'exige `REGLES §13`.

> **⚠️ Ce qu'on perd, et quand il faudra partir**
>
> Le jour où votre moteur a besoin d'un état que `Partie` ne sait pas porter —
> une pile de pouvoirs, un historique, un cache d'évaluation — l'échafaudage ne
> suffit plus. Vous reprendrez le contrat nu, et vous saurez exactement quelles
> dix-huit fonctions écrire, **parce que vous les aurez vues à l'œuvre**.
>
> C'est un échafaudage, pas une cage. Rien n'est caché : tout est inline et
> lisible dans le header.

### Ce que le banc d'essai en dit

```
=== 13 verifications OK, 3 echecs ===
```

Les trois échecs sont ceux **codés en dur sur le moteur de référence** : le banc
cherche `portee_duplication`, « 42 cases » et « 2 totems par joueur ».
`RegleFacile` joue un 5×5 avec un totem chacun. Même profil que `RegleContratNu`
(14/16) et `GrilleLibre` (13/16).

Tout ce qui porte sur le **contrat** passe — y compris le rejeu déterministe, le
bornage des paramètres et le garde-fou `max_tours`.

> **Une correction que ce banc a provoquée.** À la première version, l'échafaudage
> n'exposait **aucun paramètre** et n'avait **aucun garde-fou** : un jeu de règles
> pathologique pouvait boucler indéfiniment, et la campagne ne rendait jamais la
> main. Le banc l'a signalé en deux lignes. `max_tours` est désormais fourni
> d'office, et il n'est pas optionnel.

## 8.5 Des règles, sans échafaudage : `RegleContratNu.cpp`

Vingt et une fonctions dans la vtable, dont la moitié tient en trois lignes. Le
chapitre 2 les parcourt une à une ; voici seulement ce qui structure le fichier.

**`exemples/rules/RegleContratNu.cpp — le plateau, en C++`**

```cpp
for (int32 y = 0; y < kSide; ++y)
    for (int32 x = 0; x < kSide; ++x) {
        R->coords[y * kSide + x].q = static_cast<int16>(x);
        R->coords[y * kSide + x].r = static_cast<int16>(y);
    }
```

**`exemples/rules/RegleContratNu.cpp — un coup DUPLIQUER`**

```cpp
s->cells[di].owner = static_cast<int8>(mv->player);
s->cells[di].level = 0;
Emit(sink, user, NkcEventKind::TotemDuplicated, mv->player, mv->from, mv->to, 0);
```

> **⚠️ Le `memset` sur chaque coup**
>
> ```cpp
> NkcMove m;
> std::memset(&m, 0, sizeof(m));   // comparaison octet a octet
> ```
>
> Le contrat compare les coups **octet par octet**. Un champ non initialisé —
> même inutilisé — rend le coup différent de lui-même, et `IsLegalMove` le
> refuse. C'est l'erreur numéro un des premiers modules.

Le moteur complet, `modules/rules/ConquerorRulesV2.cpp`, ajoute le chargement de
plateau JSON, les paramètres, la sérialisation et l'empreinte. Lisez-le **après**
avoir écrit le vôtre, pas avant.

---

## 8.6 Un plateau : les deux voies

### En JSON, le plus petit possible

**`exemples/boards/mini_3x3.json`**

```json
{
  "topology": "SQUARE_4",
  "cells": [[0,0],[1,0],[2,0],
            [0,1],[1,1],[2,1],
            [0,2],[1,2],[2,2]],
  "blocked": [[1,1]],
  "starts": [
    {"player": 0, "q": 0, "r": 0, "level": 0},
    {"player": 1, "q": 2, "r": 2, "level": 0}
  ],
  "min_players": 2, "max_players": 2
}
```

Neuf cases, une bloquée au centre, deux totems opposés. Une partie dure quinze
secondes et **on suit chaque coup à l'œil**.

> **✅ C'est le plateau sur lequel on débogue**
>
> Chercher pourquoi un moteur se trompe sur 42 cases est un travail
> d'archéologue. Sur neuf, on voit.
>
> Réflexe à prendre : dès qu'un comportement paraît étrange, **rechargez
> `mini_3x3.json`** avant de lire une ligne de code.

### En C++, quand la forme est une formule

`exemples/rules/GrilleLibre.cpp` — trois anneaux concentriques, 19 cellules.
Ni hexagone, ni carré. Le chapitre 5 le détaille ; retenez les deux entrées qui
rendent la chose possible :

```cpp
int32  (*GetCellCenter)(NkcRules self, NkcCoord c, float32 *outXY);
uint32 (*GetCellShape)(NkcRules self, NkcCoord c, float32 *outXY, uint32 capPoints);
```

et celle qui rend la grille **utilisable par une IA** :

```cpp
uint32 (*GetNeighbors)(NkcRules self, NkcCoord c, NkcCoord *out, uint32 cap);
```

Sans elle, une IA devine l'adjacence à partir de `topology` — et devine faux, en
silence. `IANegamax` s'en sert pour son terme de menace, et **s'abstient** de ce
terme quand elle n'est pas déclarée : une évaluation qui devine faux est pire
qu'une évaluation qui ignore le critère.

---

## 8.7 Le chemin conseillé

1. **Copiez `IAMinimale.cpp`** dans `travail/ai/`, changez son nom dans
   `FillFactory`, sauvegardez. Vérifiez qu'elle apparaît dans le panneau
   *Joueurs*. Vous savez maintenant faire tourner du code à vous.
2. **Lisez `IAFacile.cpp`** — quinze lignes décident, le reste est de la
   plomberie qu'on recopie.
3. **Écrivez votre évaluation** : partez d'`IAFacile.cpp`, changez `Noter`, et
   rien d'autre. C'est là qu'est le vrai travail d'une IA de jeu.
4. **Faites-la jouer contre l'aléatoire**, 40 parties. Vous devez gagner
   largement. Sinon, c'est un signe — pas une malchance.
5. **Puis seulement**, ouvrez `IANegamax.cpp`.

---

## Exercices

> **✏️ 1 — Le contrôle avant la mesure**
>
> Faites jouer `IAGloutonne` contre elle-même, 100 parties. Attendu : environ
> 50 %. Si vous obtenez autre chose, cherchez pourquoi **avant** toute autre
> mesure — vous venez de trouver un biais de siège ou un bug de comptage.

> **✏️ 2 — Casser le signe, exprès**
>
> Dans `IANegamax.cpp`, remplacez `-Negamax(...)` par `Negamax(...)` dans la
> branche du changement de joueur. Recompilez, faites-la jouer contre
> l'aléatoire. Que se passe-t-il ? Combien de temps auriez-vous mis à trouver
> sans connaître la réponse ?

> **✏️ 3 — Ce que rapporte le tri**
>
> `IANegamax` expose `tri_des_coups`. Coupez-le, relancez une campagne à
> profondeur fixe, comparez le nombre de nœuds. De combien l'alpha-bêta est-il
> moins efficace sans tri ? Le chiffre vous appartient — le facteur théorique
> est connu, mais ce jeu-ci n'est pas la théorie.

> **✏️ 4 — Un troisième critère**
>
> Ajoutez à `IAGloutonne` un terme de mobilité (nombre de coups disponibles) et
> mesurez. Le gain est-il réel, ou dans le bruit ? Rappel du chapitre 6 :
> 40 parties ne suffisent pas à trancher un écart de cinq points.

> **✏️ 5 — Le budget**
>
> Passez le budget de `IANegamax` de 60 ms à 5 ms. Quelle profondeur atteint-elle
> encore ? Gagne-t-elle toujours contre l'aléatoire ? Vous mesurez là le rapport
> entre **temps de réflexion et force**, qui est la seule chose que règle
> vraiment un palier de difficulté.
