# Le principe : trois dossiers, un contrat

Ce chapitre est la carte. Tout le reste en découle, et si vous ne deviez lire
qu'un chapitre, ce serait celui-ci.

## 1.1 Trois dossiers, trois natures de contribution

Vous ne manipulez **jamais** de DLL et ne lancez **jamais** de commande de build.
Vous déposez un fichier dans un dossier, et l'atelier fait le reste.

| Je veux ajouter… | Je dépose… | Ce qui se passe |
|---|---|---|
| un **moteur de règles** | `Build/ConquerorLab/rules/mes_regles.cpp` | détecté → **compilé** → chargé → apparaît dans le panneau *Modules* |
| une **IA** | `Build/ConquerorLab/ai/mon_ia.cpp` | idem → apparaît dans la liste des pilotes, panneau *Joueurs* |
| une **grille** | `Build/ConquerorLab/boards/diamant.json` | **aucune compilation** → apparaît dans le panneau *Règles* |

La différence entre les deux premiers et le troisième n'est pas une commodité,
c'est une décision de conception :

> **✅ Une règle est du code, un plateau est une donnée**
>
> **`REGLES_COMPLETES_v2.md:95-96`**
>
> ```
> Le plateau n'est pas une constante du code : c'est un descripteur
> sérialisable, éditable à la souris dans le moteur de test.
> ```
>
> Une règle décrit un *comportement* : elle se programme. Un plateau décrit une
> *forme* : il se décrit. Confondre les deux vous condamne à recompiler pour
> essayer un hexagone au lieu d'un carré — et donc à ne jamais l'essayer.

## 1.2 Le cycle de travail

```
1.  vous ecrivez   Build/ConquerorLab/rules/mes_regles.cpp
2.  vous sauvegardez
3.  l'atelier detecte le changement (il scrute une fois par seconde)
4.  il COMPILE  ->  il charge  ->  votre module apparait dans le menu
5.  vous rejouez une partie, sans avoir quitte l'application
```

En cas d'erreur, **la sortie complète du compilateur** s'affiche dans le panneau
*Modules*. C'est le seul retour que vous aurez : pas de terminal, pas de chaîne
de build à apprendre.

> **⚠️ La compilation fige l'atelier une à deux secondes**
>
> C'est assumé, et documenté :
>
> **`Applications/ConquerorLab/src/ConquerorLab/NkcSession.h — PollModules`**
>
> ```cpp
> /// La compilation est SYNCHRONE : quand le stagiaire sauvegarde,
> /// l'atelier se fige une seconde ou deux, puis son module apparait.
> /// C'est assume — un compilateur pilote depuis un thread annexe
> /// demanderait une file de travaux et un verrou sur le catalogue,
> /// pour gagner deux secondes toutes les cinq minutes.
> ```
>
> Si l'atelier se fige au moment où vous sauvegardez, il ne plante pas : il vous
> compile.

## 1.3 Le contrat : deux tables de pointeurs

Un module ne dérive d'aucune classe et n'exporte aucun symbole C++. Il remplit
une **structure de pointeurs de fonction** et l'expose par deux fonctions `C`.

**`Applications/ConquerorLab/include/Conqueror/ConquerorRulesABI.h:255-273 (extrait)`**

```cpp
struct NkcRulesFactory {
        NkcRulesInfo   info;
        NkcRulesVTable vtable;
};

// Les DEUX symboles que tout module de regles doit exporter.
using NkcRulesGetFactoryFn   = void (*)(NkcRulesFactory *out);
using NkcRulesSetAllocatorFn = void (*)(NkcAllocFn a, NkcFreeFn f);
```

```cpp
#define NKC_RULES_SYM_GET_FACTORY "nkc_rules_get_factory"
#define NKC_RULES_SYM_SET_ALLOC   "nkc_rules_set_allocator"
```

Pourquoi une table de pointeurs plutôt qu'une classe abstraite ? Parce qu'une
classe abstraite impose une **ABI C++** — disposition de vtable, décoration des
noms, modèle d'exception — qui change d'un compilateur à l'autre et parfois d'une
version à l'autre. Une structure de pointeurs de fonction, non.

## 1.4 Qui parle à qui

```
                    +---------------------------+
                    |        ConquerorLab       |
                    | (l'atelier - six panneaux)|
                    +-------------+-------------+
                                  | NE CONNAIT AUCUNE REGLE
                    +-------------+-------------+
                    |                           |
          NkcRulesVTable                 NkcAIVTable
                    |                           |
       +------------v-----------+   +-----------v------------+
       | votre moteur de regles |<--|        votre IA        |
       |          (A1)          |   |          (A2)          |
       +------------------------+   +------------------------+
                  ^                              |
                  +------------------------------+
                     l'IA TRAVERSE les regles :
                     elle recoit la vtable + un handle opaque
```

Trois propriétés découlent de ce schéma, et ce sont elles qui font tenir le
stage :

1. **A2 démarre en semaine 1**, sur un module bouchon, sans attendre A1.
2. **Quand A1 change son code interne, l'IA continue de fonctionner** — elle n'a
   jamais vu ce code.
3. **Les deux ne peuvent pas diverger sur les règles** : il n'en existe qu'une
   implémentation, et l'IA la traverse.

## 1.5 L'état est opaque, la vue est en lecture seule

Vous choisissez librement la représentation interne de votre partie. Ni l'atelier
ni l'IA ne la voient. Ce qu'ils voient est une **vue** :

**`Applications/ConquerorLab/include/Conqueror/ConquerorRulesABI.h:92-109 (abrégé)`**

```cpp
struct NkcStateView {
        const NkcCellView *cells     = nullptr;  ///< cellCount entrees
        const NkcCoord    *coords    = nullptr;  ///< meme indexation
        uint32             cellCount = 0;

        NkcTopology topology    = NkcTopology::HexPointy;
        uint8       playerCount = 2;
        uint8       current     = 0;   ///< joueur au trait
        uint8       finished    = 0;
        int8        winner      = -2;  ///< -1 = nul, -2 = en cours
        uint32      turn        = 0;

        const int32 *energy         = nullptr;
        const int32 *conquestTenths = nullptr;   ///< en DIXIEMES
        const int32 *totemCount     = nullptr;
};
```

> **⚠️ Les pointeurs de la vue appartiennent au module**
>
> Ils restent valides **jusqu'au prochain appel mutant** sur cet état
> (`ApplyMove`, `Setup`, `DeserializeState`, `CloneState`). L'appelant ne libère
> jamais rien. En pratique : après avoir appliqué un coup, **redemandez la vue**.
> Les champs scalaires (`current`, `turn`, `finished`) sont des *copies* faites
> au moment de `GetView` — ils ne se mettent pas à jour tout seuls.

## 1.6 Les quatre exigences non négociables

Elles ne sont pas des préférences de style. Elles conditionnent l'existence même
du sujet A2 et de toute campagne de mesure.

**`REGLES_COMPLETES_v2.md:583-590`**

```
1. Zéro flottant dans la logique de règles. Entiers partout, PC en dixièmes.
2. PRNG porté par l'état de partie, sérialisé avec lui. Jamais de générateur
   global.
3. Rejeu bit-à-bit : seed + liste de coups -> état final identique, sur
   Windows et sur Linux, quel que soit le compilateur.
4. Aucun ordre d'itération de conteneur ne doit être observable dans le
   résultat. Toute résolution ambiguë est arbitrée par un critère explicite
   (index de joueur, coordonnée), jamais par l'ordre de parcours d'une table.
```

Chacune a une raison très concrète.

**1 — Zéro flottant.** Les Points de Conquête valent `1 + niveau × 0,5 +
ennemis × 0,2 + alliés × 0,1`. En virgule flottante, **l'ordre d'accumulation
change le dernier bit** selon le compilateur et la plateforme, et le rejeu
bit-à-bit tombe. Le moteur stocke donc les PC en **dixièmes entiers** : base
`10`, niveau `× 5`, ennemi `× 2`, allié `× 1`. La division par 10 est un fait
d'*affichage* — elle n'existe nulle part dans la logique.

**2 — PRNG porté par l'état.** L'IA clone une position des milliers de fois par
seconde. Avec un générateur global, deux clones piochent dans le même flux et le
rejeu meurt. Au palier 0 il n'y a aucun aléatoire dans Conqueror ; le champ
existe quand même, parce que l'ajouter plus tard obligerait à re-sérialiser tous
les états déjà enregistrés.

**3 — Rejeu bit-à-bit.** C'est ce qui permet de coller un journal dans un rapport
de bug et de le rejouer à l'identique ailleurs. Le panneau *Journal* affiche
l'empreinte après **chaque** coup : quand deux rejeux divergent, elle dit *où*,
pas seulement *qu'ils diffèrent*.

**4 — Aucun ordre observable.** Si votre générateur de coups parcourt une table
de hachage, l'ordre des coups change entre deux exécutions. L'IA choisit alors un
coup différent, la partie diverge, et vos dix mille parties ne mesurent plus rien.

> **✅ Ce qu'il faut retenir**
>
> `GenerateLegalMoves` **doit** produire les coups dans un ordre stable et
> reproductible. Dans les deux exemples de ce cours : cases par index croissant,
> puis voisins dans l'ordre de `Neighbor`. Ne le changez jamais sans raison — et
> si vous le changez, sachez que toutes les mesures antérieures deviennent
> incomparables.

## 1.7 Ce qu'un module a le droit d'inclure

La frontière est **NKCanvas / NKGui** : tout ce qui est en dessous vous est
ouvert, rien de ce qui est au-dessus ne l'est.

**`Applications/ConquerorLab/src/ConquerorLab/NkcModuleCompiler.h — StackIncludes`**

```cpp
/// L'atelier ne donnait au depart que trois -I et aucun lien : un
/// module compilait sans rien lier du moteur. C'etait elegant, et
/// trop etroit — un moteur de regles a besoin d'une chaine, d'un
/// tableau dynamique, d'un formatage, d'un journal. Reecrire tout
/// cela a la main dans chaque module est du temps vole au jeu.
```

Vous avez donc droit à :

| Ce que vous incluez | Ce que ça vous donne |
|---|---|
| `Conqueror/ConquerorRulesABI.h`, `ConquerorAIABI.h` | le contrat |
| `Conqueror/ConquerorGeometry.h` | voisinage et distance, inline et entiers |
| `NKCore/NkTypes.h` | `uint32`, `int8`, `usize`, `float64` |
| `NKContainers/String/NkString.h` | `NkString`, `NkFormat` |
| `NKContainers/Sequential/NkVector.h` | `NkVector`, `NkList`, `NkDeque` |
| `NKContainers/Associative/NkMap.h` | `NkMap`, `NkSet` |
| `NKMath/NkFunctions.h` | `math::NkSqrt`, `NkAbs`, `NkClamp`… |
| `NKLogger/NkLog.h` | `logger.Infof(...)` |
| `NKFileSystem/NkFile.h` | `NkFile`, `NkDirectory` |
| `NKThreading/NkThread.h` | `NkThread`, `NkMutex`, `NkAtomic` |
| la bibliothèque standard C | `<cstring>`, `<cstdio>`, `<cstdlib>`, `<new>` |

Vous n'avez **pas** droit à NKCanvas, NKGui, NKWindow, NKRenderer — un moteur de
règles ne dessine pas. C'est l'atelier qui affiche, à partir de la vue que vous
exposez. **Cette frontière est le cœur du contrat**, pas une restriction
administrative : c'est elle qui permet de changer entièrement l'affichage sans
toucher une règle, et inversement.

## 1.8 Quand le contrat change : majeure et mineure

Le contrat va évoluer pendant vos huit semaines. La question qui compte est :
**faut-il tout recompiler à chaque fois ?**

**`Applications/ConquerorLab/include/Conqueror/ConquerorRulesABI.h`**

```cpp
inline constexpr uint32 kRulesAbiMajor = 3;
inline constexpr uint32 kRulesAbiMinor = 1;
```

- **MAJEURE** — un champ change de type ou de place, une signature change. Votre
  module est **refusé** : il faut recompiler, et le panneau Modules le dit.
- **MINEURE** — une fonction est ajoutée *à la fin* de la vtable. Votre module
  **continue de tourner**. L'atelier l'accepte et note : « module en ABI 3.0,
  atelier en 3.1 — il fonctionne, les fonctions ajoutées depuis ne sont pas
  disponibles. »

Pourquoi cette distinction existe : sans elle, le jour où j'ai ajouté trois
entrées à la vtable, **tous** les modules devenaient « ABI 2, attendue 3 » —
refusés alors qu'ils tournaient parfaitement. Vous faire recompiler tout votre
travail parce que *j'ai* ajouté une fonction est exactement la friction qu'un
contrat doit supprimer.

> **⚠️ Ce que ça a coûté d'apprendre**
>
> La règle « on ajoute à la fin » est plus étroite qu'elle n'en a l'air : on
> ajoute à la fin de la **dernière structure de la chaîne**.
>
> `NkcRulesInfo` précède `NkcRulesVTable` dans la fabrique. Grossir `info`
> **décale** la vtable : un module d'époque écrit sa table à l'ancien décalage,
> l'atelier la lit au nouveau, et on appelle des adresses au hasard. Premier
> essai : **segfault**. Les champs sont donc à la fin de `NkcRulesFactory`,
> après la vtable.
>
> `tests/NkcAbiCompat.cpp` monte désormais la garde : il compile un module contre
> des en-têtes **figés** et le fait jouer avec l'atelier d'aujourd'hui.
> **11 vérifications, 0 échec.**

## 1.9 Afficher un état pour comprendre — le panneau Sortie

Votre module a **sa propre copie de la pile** : l'édition de liens est statique.
Conséquence directe, et surprenante la première fois : un `logger.Infof()` depuis
votre code n'atteint pas la console de l'atelier. Vous écrivez dans *votre*
journal, pas dans le sien.

Ce n'est pas un oubli — c'est la conséquence de l'isolation qui fait tout
l'intérêt du système. Mais déboguer un moteur de règles sans pouvoir afficher un
état, c'est déboguer à l'aveugle. L'atelier **injecte donc un puits** dans chaque
module au chargement, et le branchement tient en une ligne :

**`Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp — fin de fichier`**

```cpp
NKC_MODULE_EXPORT void nkc_rules_set_allocator(NkcAllocFn a, NkcFreeFn f) { gAlloc = a; gFree = f; }
NKC_MODULE_LOGGING(rules)   // <- une ligne, et NKC_LOG_* va dans le panneau « Sortie »
```

`NKC_MODULE_LOGGING(rules)` dans un module de règles, `NKC_MODULE_LOGGING(ai)`
dans une IA. Il faut inclure `Conqueror/ConquerorLog.h`. À partir de là :

```cpp
NKC_LOG_INFO("nouvelle partie : %d cases, %d joueurs, graine %llu",
             kCells, (int32)s->playerCount, (unsigned long long)seed);
NKC_LOG_WARN("coup rejete : la destination n'est pas voisine");
```

s'affichent dans le panneau **Sortie**, avec le nom de votre module et le niveau.

> **✅ `logger.Infof` marche aussi**
>
> Si vous incluez NKLogger, la macro branche en plus un `NkISink` sur le logger
> *privé* de votre module, qui repousse tout vers l'atelier. Le réflexe de
> quelqu'un qui connaît déjà Nkentseu fonctionne donc sans qu'il ait à
> l'apprendre.
>
> Une seule contrainte : incluez `ConquerorLog.h` **après** NKLogger — la
> détection se fait sur son garde d'inclusion.

> **⚠️ Sans atelier, tout part sur stderr**
>
> Banc d'essai, test en ligne de commande : personne n'injecte de puits, et vos
> traces sortent sur `stderr` sous la forme `[INFO] module: ...`. Elles ne
> disparaissent jamais en silence.

> **⚠️ Ne journalisez pas dans une boucle chaude**
>
> `NKC_LOG_*` formate **avant** de savoir si quelqu'un écoute. Dans un rollout
> MCTS, c'est un désastre — et le tampon de l'atelier est borné à 4096 lignes,
> donc vous perdriez de toute façon ce que vous cherchez. Une trace par coup, pas
> par nœud. Pour mesurer, il y a `NkcAIResult` et `GetDebugJson`.

L'isolation explique aussi `nkc_rules_set_allocator` : sans injection, les deux
côtés allouent dans des tas distincts.

Les trois exemples de ce cours n'utilisent volontairement que `<cstring>` et
`<cstdio>` : ils doivent rester lisibles par quelqu'un qui ne connaît pas encore
Nkentseu. Rien ne vous oblige à cette austérité.

> **⚠️ Pourquoi la projection écran n'est pas dans la géométrie**
>
> **`Applications/ConquerorLab/include/Conqueror/ConquerorGeometry.h:10-13`**
>
> ```
> Aucune dependance a NKMath : tout y est inline et entier, donc un module
> compile par l'atelier n'a AUCUN symbole a lier. La projection ecran
> (cos/sin/arrondi) vit ailleurs — c'est de la presentation, pas de la regle.
> ```
>
> `NkRound` de NKMath est un symbole *lié*. L'inclure dans le contrat ferait
> échouer l'édition de liens de **tous** les modules. C'est aussi la bonne
> frontière : une règle de jeu ne connaît pas les pixels.

## 1.8 La géométrie, en trois fonctions

C'est tout ce dont vous avez besoin pour raisonner sur un plateau.

**`Applications/ConquerorLab/include/Conqueror/ConquerorGeometry.h:26-88 (signatures)`**

```cpp
int32    NeighborCount(NkcTopology t) noexcept;
NkcCoord Neighbor(NkcTopology t, NkcCoord c, int32 i) noexcept;
int32    Distance(NkcTopology t, NkcCoord a, NkcCoord b) noexcept;
```

Quatre topologies, et le voisinage est une propriété de la topologie — **jamais
codé en dur ailleurs** :

| Topologie | Voisins | Coordonnées |
|---|---|---|
| `HexPointy` | 6 | axiales `(q, r)`, pointes haut/bas |
| `HexFlat` | 6 | axiales `(q, r)`, pointes gauche/droite |
| `Square4` | 4 | `(x, y)`, orthogonaux |
| `Square8` | 8 | `(x, y)`, + diagonales |

Un seul type de coordonnée pour les quatre : c'est la topologie qui décide de
l'interprétation.

> **⚠️ L'ordre des voisins est NORMATIF**
>
> **`ConquerorGeometry.h:36-37`**
>
> ```cpp
> /// i-eme voisin de `c`. L'ORDRE EST NORMATIF : il fixe l'ordre de
> /// generation des coups, donc la reproductibilite. Ne jamais le changer.
> ```

## Exercices

> **✏️ 1 — Lire le contrat**
>
> Ouvrez `ConquerorRulesABI.h` et faites la liste des fonctions de
> `NkcRulesVTable` **sans regarder le chapitre 2**. Classez-les en quatre
> familles : cycle de vie, réglages, jeu, sérialisation. Combien y en a-t-il ?
> Laquelle vous paraît la plus difficile à implémenter, et pourquoi ?

> **✏️ 2 — La cascade**
>
> Lisez `REGLES_COMPLETES_v2.md` §7.3. Une duplication retourne les ennemis
> voisins de la case où l'on vient de poser. Si un totem retourné a lui-même des
> voisins ennemis, sont-ils retournés à leur tour ? Trouvez la phrase qui
> tranche, et expliquez pourquoi le document dit que c'est « le point le plus
> facile à implémenter de travers ».

> **✏️ 3 — Le flottant qui tue**
>
> Écrivez un petit programme qui additionne `0.1f` cent fois, puis fait la même
> chose en dixièmes entiers, et compare au résultat exact `10.0`. Quel écart
> obtenez-vous ? Multipliez-le mentalement par dix mille parties et deux
> plateformes : vous tenez la raison de l'exigence n° 1.
