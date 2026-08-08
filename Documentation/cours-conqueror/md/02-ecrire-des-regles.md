# Écrire un moteur de règles

Ce chapitre construit un moteur complet, de la première ligne à la dernière. Le
fichier existe, il compile, et l'atelier le charge :

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp`**

Ouvrez-le à côté de ce texte. Nous le parcourons dans l'ordre, en expliquant à
chaque étape *pourquoi* elle est là et *ce qui casse* si on l'oublie.

## 2.0 Ce que ce moteur fait

```
plateau         carre 5x5, 4 voisins  (pas d'hexagone : une chose a la fois)
action          DUPLIQUER uniquement, portee 1
transformation  les ennemis voisins de la case OU L'ON VIENT DE POSER
                changent de camp -- et eux seuls
fin             des qu'un joueur ne peut plus dupliquer
decompte        le plus de totems gagne
```

C'est déjà un jeu jouable, et c'est assez pour que l'atelier fasse **tout** ce
qu'il sait faire : partie humaine, IA, journal, rejeu, campagne de mesure.

## 2.1 Les vingt-et-une fonctions, par famille

`NkcRulesVTable` compte vingt-et-une entrées. Elles se rangent en quatre
familles, et cette carte suffit à ne jamais se perdre :

| Famille | Fonctions | Ce qu'elles font |
|---|---|---|
| **cycle de vie** | `Create`, `Destroy`, `CreateState`, `DestroyState`, `CloneState`, `Setup` | fabriquer et détruire un moteur, puis des parties |
| **réglages** | `GetParamsSchemaJson`, `SetParam`, `GetParam`, `LoadBoardJson`, `GetBoardJson` | ce que le designer peut changer sans recompiler |
| **jeu** | `GetView`, `GenerateLegalMoves`, `IsLegalMove`, `ApplyMove`, `IsFinished`, `GetWinner`, `IsPlayerBlocked` | les règles proprement dites |
| **sérialisation** | `SerializeState`, `DeserializeState`, `HashState` | rejeu, transfert vers le thread d'IA, diagnostic |

> **✅ Ce qu'il faut retenir**
>
> Convention de retour, partout : **`0` = échec, `1` = succès**. Une fonction qui
> échoue ne doit **rien** avoir modifié.

## 2.2 La mémoire : jamais de `new` ni de `delete` bruts

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — section 1`**

```cpp
NkcAllocFn gAlloc = nullptr;
NkcFreeFn  gFree  = nullptr;

void *RawAlloc(usize n) { return gAlloc ? gAlloc(n) : std::malloc(n); }
void  RawFree(void *p)  { if (!p) return; if (gFree) gFree(p); else std::free(p); }
```

L'atelier peut vous injecter ses propres allocateurs (NKMemory) par
`nkc_rules_set_allocator`. Les deux chemins doivent exister : avec injection, et
sans.

> **⚠️ Ne jamais mélanger deux tas**
>
> C'est une règle générale du dépôt : allouer avec l'allocateur du moteur et
> libérer avec `std::free` (ou l'inverse) corrompt le tas — sous Windows, cela
> donne un `c0000374` sans pile utile, souvent très loin du vrai coupable. Le
> couple `RawAlloc` / `RawFree` ci-dessus garantit la symétrie.

## 2.3 Les paramètres : aucune constante en dur

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — section 2`**

```cpp
enum ParamId : int32 {
    P_PORTEE = 0,     ///< distance maximale de duplication
    P_MAX_TOURS,      ///< garde-fou de simulation, jamais une regle de jeu
    P_COUNT
};

struct ParamDesc { const char *key, *group; int32 def, lo, hi; };

const ParamDesc kParams[P_COUNT] = {
    {"portee_duplication", "Duplication",   1,  1,   3},
    {"max_tours",          "Fin de partie", 200, 10, 100000},
};
```

Une seule table décrit tout : la clé, le groupe d'affichage, la valeur par
défaut, et les bornes. **Ajouter un réglage = ajouter une ligne.**

Et voici pourquoi cela suffit à faire apparaître un champ à l'écran :

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — V_GetParamsSchemaJson`**

```cpp
k = std::snprintf(w, left,
                  "%s{\"key\":\"%s\",\"group\":\"%s\",\"type\":\"int\","
                  "\"min\":%d,\"max\":%d,\"def\":%d,\"val\":%d}",
                  i ? "," : "", kParams[i].key, kParams[i].group,
                  kParams[i].lo, kParams[i].hi, kParams[i].def, R->params[i]);
```

Le panneau *Règles* de l'atelier est **entièrement auto-généré** à partir de
cette chaîne. Il n'y a pas une ligne d'interface par paramètre : ajoutez une
ligne dans `kParams`, le réglage apparaît, groupé, borné, avec son infobulle.

Types reconnus par l'atelier :

| `"type"` | Widget | Champs supplémentaires |
|---|---|---|
| `"int"` | champ numérique (glisser, ou boutons quand la plage est étroite) | `min`, `max` |
| `"bool"` | case à cocher | — |
| `"enum"` | liste déroulante | `"values":["A","B",…]` |

Deux champs optionnels partout : `"label"` (sinon dérivé de la clé) et `"help"`
(affiché en infobulle).

### Borner, ne pas refuser

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — V_SetParam`**

```cpp
int32 v = static_cast<int32>(value < 0 ? value - 0.5 : value + 0.5);
if (v < kParams[i].lo) v = kParams[i].lo;
if (v > kParams[i].hi) v = kParams[i].hi;
R->params[i] = v;
return 1;
```

> **✅ Le banc d'essai vérifie ce comportement**
>
> **`Applications/ConquerorLab/tests/NkcAbiHarness.cpp:121-124`**
>
> ```cpp
> // Hors plage : doit etre borne au minimum du parametre (10), pas accepte.
> R.SetParam(ri, "max_tours", 3);
> CHECK(R.GetParam(ri, "max_tours") == 10.0,
>       "une valeur hors plage est bornee, pas acceptee telle quelle");
> ```
>
> Le panneau *Règles* relit le schéma **à chaque image** et n'a aucun cache : si
> vous bornez, l'utilisateur voit tout de suite la valeur bornée. Un cache local
> côté interface mentirait.

Notez aussi que `SetParam` prend un `float64` — c'est le **seul** flottant du
contrat, et il ne traverse pas la logique : c'est un canal de réglage, converti
en entier dès la première ligne.

## 2.4 L'état : votre représentation, leur vue

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — section 4`**

```cpp
struct State {
        NkcCellView cells[kCells];
        uint8       playerCount = 2;
        uint8       current     = 0;
        uint8       finished    = 0;
        int8        winner      = -2;
        uint32      turn        = 0;
        int32       energy[kMaxPlayers]         = {};
        int32       conquestTenths[kMaxPlayers] = {};
        int32       totemCount[kMaxPlayers]     = {};
        uint64      rng = 0x9E3779B97F4A7C15ull;   ///< PRNG PORTE PAR L'ETAT
};
```

Le contrat n'impose rien de cette structure. Une seule contrainte, mais elle est
dure :

> **⚠️ L'état doit se copier tel quel**
>
> `CloneState` est le **chemin chaud de l'IA** : appelé des milliers de fois par
> seconde. Le contrat dit : « doit être rapide et ne jamais allouer ». D'où :
>
> **`RegleMinimale.cpp — V_CloneState`**
>
> ```cpp
> void V_CloneState(NkcRules, NkcState dst, const NkcState src) {
>     if (dst && src) *static_cast<State *>(dst) = *static_cast<const State *>(src);
> }
> ```
>
> Une seule affectation, parce que `State` est un POD. Si vous y mettez un
> conteneur qui alloue, cette ligne alloue aussi, et votre IA perd un ordre de
> grandeur.

La même contrainte explique `SerializeState` :

**`RegleMinimale.cpp — V_SerializeState`**

```cpp
uint32 V_SerializeState(NkcRules, const NkcState st, void *buf, uint32 cap) {
    const uint32 need = static_cast<uint32>(sizeof(State));
    if (!buf || cap < need) return need;   // renvoie la taille NECESSAIRE
    std::memcpy(buf, st, need);
    return need;
}
```

> **⚠️ Le protocole en deux temps**
>
> Appelée avec `buf == nullptr`, la fonction doit renvoyer la **taille
> nécessaire** sans rien écrire. C'est ainsi que l'atelier dimensionne son
> tampon avant de transférer l'état vers le thread d'IA. Renvoyer `0` dans ce cas
> fait échouer la réflexion — et l'atelier affichera alors « SerializeState
> annonce une taille nulle » dans la barre du plateau.

## 2.5 Générer les coups

C'est la fonction la plus importante du fichier. Trois exigences s'y croisent.

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — GenMoves`**

```cpp
for (int32 i = 0; i < kCells; ++i) {
    if (s->cells[i].owner != static_cast<int8>(me)) continue;
    const NkcCoord src = R->coords[i];

    for (int32 k = 0; k < nbr; ++k) {
        const NkcCoord dst = Neighbor(NkcTopology::Square4, src, k);
        const int32    di  = IndexOf(dst);
        if (di < 0) continue;
        if (s->cells[di].owner != kCellEmpty) continue;
        if (Distance(NkcTopology::Square4, src, dst) > range) continue;

        if (n < cap) {
            NkcMove m;
            std::memset(&m, 0, sizeof(m));
            m.kind        = NkcMoveKind::Duplicate;
            m.player      = me;
            m.from        = src;
            m.to          = dst;
            m.targetLevel = -1;
            m.powerId     = -1;
            out[n]        = m;
        }
        ++n;
    }
}
```

**Exigence 1 — l'ordre est normatif.** Cases par index croissant, puis voisins
dans l'ordre de `Neighbor`. Deux exécutions produisent la même liste, sur toute
plateforme.

**Exigence 2 — la mise à zéro est obligatoire.**

> **⚠️ `memset` avant de remplir, toujours**
>
> **`ConquerorRulesABI.h:112-113`**
>
> ```
> Coups (REGLES §6.2). POD comparable octet a octet APRES mise a zero :
> le module DOIT remplir integralement les champs inutilises avec zero.
> ```
>
> `NkcMove` contient `NkcCoord fuseCells[12]`, inutilisés au palier 0. Sans
> `memset`, ils contiennent ce que la pile y a laissé. Or `IsLegalMove` compare
> les coups **octet à octet** : deux coups identiques deviendraient différents, et
> l'atelier rejetterait le coup de votre propre IA. Le symptôme est spectaculaire
> — « l'IA a proposé un coup ILLÉGAL » — et la cause est trois lignes plus haut.

**Exigence 3 — `cap` peut être trop petit.** La fonction écrit **au plus** `cap`
coups mais renvoie le nombre **total**. Remarquez que `++n` est en dehors du
`if (n < cap)` : l'appelant apprend ainsi qu'il doit agrandir son tampon.

### Le cas de PASSER

```cpp
// PASSER n'est legal que si RIEN d'autre ne l'est (REGLES §6.2).
if (n == 0) {
    if (cap > 0) { /* ... un coup Pass ... */ }
    return 1;
}
```

Ce n'est pas un détail de confort : c'est ce qui rend vraie l'équivalence
suivante, que le banc d'essai vérifie automatiquement.

## 2.6 L'équivalence qui teste tout le reste

**`Applications/ConquerorLab/tests/NkcAbiHarness.cpp:107-111`**

```cpp
const uint32 n = R.GenerateLegalMoves(ri, probe, buf, 512);
const bool onlyPass = (n == 1 && buf[0].kind == NkcMoveKind::Pass);
const bool blocked  = R.IsPlayerBlocked(ri, probe, pv.current) != 0;
if (onlyPass != blocked) { equiv = false; break; }
```

Autrement dit : **« aucun coup légal » doit être exactement équivalent à « joueur
bloqué »**. Les règles le disent en toutes lettres :

**`REGLES_COMPLETES_v2.md:467`**

```
| Aucune action légale mais joueur non bloqué au sens §12.1 | Impossible par
construction. Le moteur doit asserter cette équivalence en test : c'est le
meilleur test d'intégrité du générateur de coups. |
```

> **✅ Pourquoi ce test attrape presque tout**
>
> `GenerateLegalMoves` et `IsPlayerBlocked` sont deux chemins de code
> indépendants qui répondent à la même question. S'ils divergent, l'un des deux
> a tort — et c'est presque toujours le générateur, sur un cas de bord (case hors
> plateau, case bloquée, portée mal comparée). Dans `RegleMinimale.cpp` les deux
> partagent délibérément la même primitive, `CanPlay`, ce qui rend l'équivalence
> vraie par construction.

## 2.7 Appliquer un coup, et la cascade

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — DoApply`**

```cpp
// --- placement : la source RESTE INTACTE (REGLES §7.2) ---------------
s->cells[di].owner = static_cast<int8>(mv->player);
s->cells[di].level = 0;
Emit(sink, user, NkcEventKind::TotemDuplicated, mv->player, mv->from, mv->to, 0);
s->conquestTenths[mv->player] += 10;   // +1,0 PC, en DIXIEMES entiers
```

Puis la transformation, et **c'est ici qu'on se trompe** :

```cpp
const int32 nbr     = NeighborCount(NkcTopology::Square4);
int32       flipped = 0;
for (int32 k = 0; k < nbr; ++k) {
    const int32 ni = IndexOf(Neighbor(NkcTopology::Square4, mv->to, k));
    if (ni < 0) continue;
    const int8 o = s->cells[ni].owner;
    if (o < 0 || o == static_cast<int8>(mv->player)) continue;
    /* ... retourner ce totem ... */
    ++flipped;
}
if (flipped >= 2)
    Emit(sink, user, NkcEventKind::Cascade, mv->player, mv->to, mv->to, flipped);
```

> **⚠️ Une transformation ne déclenche pas de transformation**
>
> **`REGLES_COMPLETES_v2.md:246-249`**
>
> ```
> Une transformation ne déclenche pas de nouvelle transformation. Seul le
> totem placé est un déclencheur. Les cascades naissent de la multiplicité des
> voisins, pas d'une propagation. C'est le point le plus facile à implémenter
> de travers.
> ```
>
> La boucle ci-dessus parcourt **une seule fois** les voisins de `mv->to`. Si
> vous la rappelez sur chaque case retournée, tout le plateau bascule au premier
> coup et le jeu n'existe plus. La « cascade » que l'atelier affiche en gros au
> centre de l'écran, c'est `flipped >= 2` — plusieurs voisins d'un coup, pas une
> réaction en chaîne.

### Les événements : décrire, pas animer

`Emit` pousse un `NkcEvent` vers un collecteur fourni par l'appelant. Le moteur
ne sait rien de l'animation : il décrit ce qui s'est produit, la présentation
décide comment le montrer.

Ce collecteur peut être `nullptr` — et il l'est presque toujours, parce qu'une IA
ne veut pas payer le coût des événements pendant ses simulations. **Testez-le** :
`Emit` commence par `if (!sink) return;`.

> **✅ Les événements servent aussi à l'aperçu**
>
> Quand vous survolez une destination dans l'atelier, les totems qui seraient
> retournés s'entourent de rouge. L'atelier ne le **déduit** pas des règles : il
> clone l'état, joue le coup pour de faux avec un collecteur, et lit les
> `TotemTransformed`. Conséquence : **votre aperçu reste juste même quand vous
> changez la règle de transformation**, sans qu'on touche à l'interface.

## 2.8 Fin de partie et garde-fou

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — CheckEnd`**

```cpp
// Garde-fou de SIMULATION : sans lui, une partie sur dix mille ne finit
// jamais et le batch tourne pour toujours (REGLES §12.3).
if (!over && static_cast<int32>(s->turn) >=
                 R->params[P_MAX_TOURS] * static_cast<int32>(s->playerCount))
    over = true;
```

> **⚠️ `max_tours` n'est PAS une règle de jeu**
>
> **`REGLES_COMPLETES_v2.md:451-454`**
>
> ```
> max_tours n'est pas une règle de jeu : c'est une sécurité pour que 10 000
> parties IA-vs-IA se terminent toujours. Le rapport de batch doit indiquer
> combien de parties ont fini par ce garde-fou. Un taux non nul signale une
> pathologie des règles, pas un réglage à monter.
> ```
>
> Le panneau *Métriques* affiche ce taux dans une tuile à part, **rouge dès qu'il
> dépasse zéro**. Si vous le voyez monter, ne montez pas `max_tours` : cherchez
> pourquoi vos parties ne se terminent pas.

## 2.9 Les deux symboles exportés

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — fin de fichier`**

```cpp
NKC_MODULE_EXPORT void nkc_rules_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_EXPORT void nkc_rules_get_factory(NkcRulesFactory *out) { FillFactory(out); }
```

> **⚠️ L'erreur du premier jour**
>
> Les oublier donne, dans le panneau *Modules* :
>
> **`Applications/ConquerorLab/src/ConquerorLab/NkcModuleHost.h — BuildAndLoad`**
>
> ```cpp
> e.log  = "Symbole introuvable : ";
> e.log += symFactory;
> e.log += " — as-tu bien termine ton fichier par la macro d'export ?";
> ```
>
> Le message a été écrit pour cette erreur précise, parce qu'elle arrive à tout
> le monde une fois.

Et la version d'ABI, dans `FillFactory` :

```cpp
out->info.abiVersion = kRulesAbiVersion;
```

L'atelier **refuse** un module dont l'`abiVersion` diffère, avec un message qui
donne les deux numéros. Si vous lisez « ABI 1, attendue 2 », vous avez compilé
contre de vieux en-têtes.

## 2.10 Votre premier module, en quatre gestes

1. Copiez `exemples/rules/RegleMinimale.cpp` vers
   `Build/ConquerorLab/rules/mes_regles.cpp`.
2. Changez le nom dans `FillFactory` — sinon vous aurez deux entrées identiques
   dans le menu et vous ne saurez pas laquelle vous testez.
3. Sauvegardez. Attendez une seconde. Le panneau *Modules* affiche votre module.
4. Sélectionnez-le, puis **Nouvelle partie**.

À partir de là, modifiez **une chose à la fois** et mesurez. C'est tout le
métier.

## Exercices

> **✏️ 1 — La portée**
>
> `portee_duplication` est réglable de 1 à 3, mais `GenMoves` n'énumère que les
> **voisins immédiats** : passer la portée à 2 ne change donc rien. Corrigez-le.
> Indice : `Neighbor` ne donne que la distance 1 ; il vous faut parcourir les
> cases et filtrer par `Distance(...) <= range`. Vérifiez ensuite que le plateau
> réagit bien au changement.

> **✏️ 2 — Casser l'équivalence exprès**
>
> Dans `V_IsPlayerBlocked`, remplacez `CanPlay(R, s, player)` par `true`.
> Recompilez, lancez une partie IA contre IA. Que se passe-t-il ? Reproduisez
> ensuite le test du banc d'essai à la main : générez les coups, appelez
> `IsPlayerBlocked`, comparez. Vous venez d'écrire votre premier test
> d'intégrité.

> **✏️ 3 — Le `memset` manquant**
>
> Retirez le `std::memset(&m, 0, sizeof(m))` de `GenMoves`, recompilez, et faites
> jouer une IA. Décrivez ce que vous observez et **expliquez-le** par le
> fonctionnement de `IsLegalMove`. Remettez ensuite la ligne.

> **✏️ 4 — Le troisième joueur**
>
> `V_Setup` force `playerCount` à 2. Faites-en un moteur à 3 joueurs : placez un
> troisième totem de départ, faites tourner l'ordre de jeu, et vérifiez le
> décompte en cas d'égalité. Attention : que doit-il se passer quand un joueur
> n'a plus aucun totem ?
