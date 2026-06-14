# Les types-vocabulaire

> Couche **Foundation** · NKCore · Exprimer deux *situations* récurrentes plutôt que des données :
> « une valeur **peut-être absente** » avec `NkOptional<T>`, et « une valeur de **l'un parmi
> plusieurs** types » avec `NkVariant<Ts...>`.

Certains types ne représentent pas une donnée précise — un entier, un vecteur — mais une *situation*
qui revient sans cesse en programmation : « une valeur qui pourrait ne pas être là », ou « une valeur
qui est de l'un parmi plusieurs types possibles ». On les appelle des *types-vocabulaire* parce
qu'ils enrichissent le vocabulaire avec lequel on exprime ses intentions dans une signature. Au lieu
de tordre un type existant — renvoyer `-1` pour « pas trouvé », un pointeur nul, un `tag` entier à
côté d'une union brute — on met l'intention **dans le type lui-même**, et le compilateur la fait
respecter. NKCore en fournit deux, équivalents *maison* et zéro-STL de `std::optional` et
`std::variant`.

Les deux sont **header-only**, sans allocation dynamique (stockage *inline* aligné sur la valeur), et
**non thread-safe** : un même objet partagé entre threads doit être protégé à l'extérieur. Ils
suivent la convention NKCore — `HasValue`, `Reset`, `Emplace`, `Swap` — et leur philosophie commune
est de séparer **l'accès non vérifié** (rapide, comportement indéfini si vous vous trompez) de
**l'accès sûr** (`GetIf`/`ValueOr`, qui ne plantent jamais).

- **Namespace** : `nkentseu`
- **Headers** : `#include "NKCore/NkOptional.h"` · `#include "NKCore/NkVariant.h"`

---

## `NkOptional<T>` — peut-être une valeur

Comment une fonction de recherche signale-t-elle « je n'ai rien trouvé » ? Renvoyer `-1`, un pointeur
nul, ou un booléen posé à côté de la vraie valeur… autant de conventions fragiles que l'appelant peut
oublier de vérifier, et que rien ne l'oblige à respecter. `NkOptional<T>` rend l'absence **explicite
dans le type** : un `NkOptional<nk_int32>` contient soit un entier, soit rien — et la signature le
dit.

```cpp
NkOptional<nk_int32> Find(const char* key);   // peut ne rien trouver

if (auto r = Find("hp")) {     // teste la présence, naturellement
    nk_int32 v = *r;           // ici, on SAIT qu'il y a une valeur
}
```

Tout tient dans ce schéma. On teste l'optionnel — il se convertit en booléen (conversion **explicite**
vers `HasValue()`, donc utilisable dans un `if` mais pas dans une conversion implicite hasardeuse) —
et **seulement si** la présence est confirmée, on accède à la valeur par `*`, `->`, ou l'alias plus
lisible `Value()`. On crée un optionnel vide par défaut (`NkOptional<T> o;`) ou explicitement avec le
marqueur global `NkNullOpt`, et on le vide à tout moment avec `Reset()` ou `o = NkNullOpt;`.

Ce n'est **pas** un pointeur : il n'y a pas d'indirection, pas d'allocation — la valeur vit *à
l'intérieur* de l'optionnel, dans un buffer aligné. Et ce n'est **pas** une garantie de sûreté
magique : `*r`, `r->`, `Value()` ne vérifient **rien**. Accéder à un optionnel **vide** par ces voies
est un comportement indéfini — ils sont marqués `noexcept`, mais ce `noexcept` ne les rend pas sûrs,
il dit juste qu'ils ne lèveront pas d'exception. Le `if` de présence n'est donc pas une formalité,
c'est le contrat. Quand vous ne voulez pas l'écrire, deux accès **sûrs** existent : `GetIf()` (renvoie
un pointeur, ou `nullptr` si vide) et `ValueOr(defaut)` (renvoie une copie de la valeur, ou un
repli).

> **En résumé.** `NkOptional<T>` rend l'absence visible dans la signature. Testez avec `if (opt)` /
> `HasValue()`, puis accédez par `*`/`Value()` (rapides, **non vérifiés** → UB si vide) ou par les
> voies sûres `GetIf()` / `ValueOr(defaut)`. Videz avec `Reset()` ou `= NkNullOpt`. Préférez `Emplace`
> pour construire en place sans copie.

---

## `NkVariant<Ts...>` — l'un parmi plusieurs

Là où l'optionnel exprime « zéro ou une valeur », `NkVariant<Ts...>` exprime « **exactement** une
valeur, mais de l'un parmi ces types ». C'est une union *discriminée* et *type-safe* : à tout instant
elle sait quel type elle contient (son `Index()`), et elle empêche d'y accéder comme à un autre. Là où
une `union` C brute vous laisse lire les octets sous le mauvais type sans broncher, le variant garde la
trace du type actif et vous le fait vérifier.

```cpp
NkVariant<nk_int32, float32, const char*> v = 3.14f;   // sélectionne float32

if (v.HoldsAlternative<float32>()) {
    float32 f = v.Get<float32>();
}
```

Le pack `Ts...` doit contenir **au moins un type** (`static_assert`), et la construction directe depuis
une valeur ne compile que si son type décayé appartient au pack (SFINAE) — `NkVariant<bool, int>
c(42);` choisit `int`, et réaffecter `c = true;` change le type actif pour `bool`. Pour interroger
l'état, `HoldsAlternative<T>()` répond « est-ce un `T` en ce moment ? », `Index()` donne l'index
0-based du type actif (ou `kNpos` si vide), et `HasValue()` / `ValuelessByException()` disent
simplement s'il y a une valeur.

Pour y accéder, trois niveaux de garantie. `Get<T>()` est direct mais **non vérifié** (comportement
indéfini si le type actif n'est pas `T`). `GetChecked<T>()` ajoute une assertion `NK_ASSERT` en debug
(et redevient un `Get<T>()` à coût nul en release). `GetIf<T>()` est le plus sûr : il renvoie un
pointeur sur la valeur, ou `nullptr` si le type courant n'est pas celui demandé (ou si le variant est
vide) — jamais d'UB.

```cpp
if (auto* s = v.GetIf<const char*>()) {   // nullptr si v ne contient pas une chaîne
    UseString(*s);
}
```

Mais la façon la plus idiomatique de traiter **tous** les cas est `Visit`. On lui passe un visiteur
générique, et il l'applique à la valeur active quel que soit son type — le dispatch se fait via
`NkInvoke` (vu au chapitre des traits). Le visiteur doit savoir traiter *chacun* des types du pack,
sinon le code ne compile pas ; l'idiome est un lambda générique avec `if constexpr` :

```cpp
v.Visit([](auto&& value) {
    Process(value);   // appelé avec le type RÉELLEMENT contenu
});
```

> **En résumé.** `NkVariant<...>` est une union type-safe : `HoldsAlternative<T>()` / `Index()` pour
> savoir ce qu'elle contient, `GetIf<T>()` (ou `GetChecked<T>()` en debug) pour accéder prudemment,
> `Get<T>()` seulement si vous êtes certain du type. `Visit` traite tous les cas d'un coup. Le ctor par
> défaut n'emplace le premier type que s'il est défaut-constructible — sinon le variant est vide.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé (comportement,
complexité, usages) dans la « Référence complète » qui suit.

### `NkOptional<T>` — valeur peut-être absente

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Sentinelle | `NkNullOpt_t`, `NkNullOpt` | Type tag + instance globale `constexpr` de l'état vide. |
| Construction | `NkOptional()`, `NkOptional(NkNullOpt_t)`, `NkOptional(const T&)`, `NkOptional(T&&)`, copie, déplacement | Vide / vide explicite / copie valeur / déplace valeur / copie profonde / transfert (vide la source). |
| Affectation | `operator=(NkNullOpt_t)`, `operator=(const NkOptional&)`, `operator=(NkOptional&&)`, `operator=(const T&)`, `operator=(T&&)` | Réinitialise à vide / copie / déplace / affecte une valeur. |
| Modification | `Emplace(args…)`, `Reset()` | Construit en place (parfait *forwarding*) / vide (idempotent). |
| Observateurs | `HasValue()`, `Empty()`, `explicit operator nk_bool()` | Présence / absence / test en condition `if (opt)`. |
| Accès non vérifié | `operator*`, `operator->`, `Value()` | Référence/pointeur sur la valeur — **UB si vide**. |
| Accès sûr | `GetIf()`, `ValueOr(fallback)`, `ValueOrRef(fallback)` | Pointeur ou `nullptr` / copie ou repli / référence ou repli. |
| Utilitaire | `Swap(other)` | Échange les contenus (gère l'auto-swap). |

### `NkVariant<Ts...>` — somme de types

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constantes | `kTypeCount`, `kNpos` | Nombre de types / sentinelle d'index invalide. |
| Construction | `NkVariant()`, copie, déplacement, `NkVariant(T&& value)` | Défaut (emplace `T0` si possible) / copie / déplacement / depuis une valeur du pack (SFINAE). |
| Affectation | `operator=(const NkVariant&)`, `operator=(NkVariant&&)`, `operator=(T&& value)` | Copie / déplacement / affecte une valeur du pack (SFINAE). |
| État | `HasValue()`, `ValuelessByException()`, `Index()` | A une valeur ? / est vide ? / index du type actif (`kNpos` si vide). |
| Cycle de vie | `Reset()`, `Emplace<T>(args…)` | Vide / construit en place le type `T` (SFINAE, parfait *forwarding*). |
| Accès | `HoldsAlternative<T>()`, `Get<T>()`, `GetChecked<T>()`, `GetIf<T>()` | Test de type / accès non vérifié / accès assert-debug / accès sûr (pointeur ou `nullptr`). |
| Visiteur | `Visit(visitor)` (+ const) | Applique un visiteur à la valeur active (no-op si vide). |
| Utilitaire | `Swap(other)` | Échange les contenus (gère l'auto-swap). |
| Libre | `NkHoldsAlternative<T>(v)`, `NkGetIf<T>(&v)`, `NkVisit(visitor, v)` | Versions fonctions libres (`NkGetIf` prend un **pointeur** ; `NkVisit` = visiteur **en premier**). |

---

## Référence complète

Chaque élément est repris ici en détail. Les éléments triviaux sont décrits brièvement ; les accès et
le dispatch — là où se jouent la sûreté et les pièges — le sont **à fond**, avec leurs usages dans les
différents domaines du temps réel.

### `NkNullOpt_t` et `NkNullOpt` — le marqueur du vide

`NkNullOpt_t` est un type sentinelle minuscule dont l'unique constructeur est `explicit` et prend un
argument fictif (ignoré) — cette explicité empêche qu'un `0` ou un `{}` ne se transforme par accident
en « optionnel vide ». `NkNullOpt` en est l'instance globale `constexpr`. On s'en sert pour deux
choses : **créer** un optionnel vide de façon parlante (`NkOptional<T> o = NkNullOpt;`) et le **vider**
ensuite (`o = NkNullOpt;`, équivalent de `Reset()`). Ces deux usages passent par des conversions
**non-`explicit`**, donc le `= NkNullOpt` lit bien. C'est l'analogue exact de `std::nullopt`.

### `NkOptional` — construction, affectation, cycle de vie

Un optionnel se construit vide (ctor par défaut, `constexpr`), à partir d'une valeur (par copie
`const T&` ou par déplacement `T&&`), ou à partir d'un autre optionnel. Le ctor et l'opérateur de
**déplacement** sont `noexcept` et **vident la source** (`other.Reset()`) après transfert : ne
supposez pas que l'optionnel d'origine contient encore quelque chose après un `NkMove`. Les
affectations couvrent les mêmes cas, gèrent l'auto-affectation, et acceptent aussi `= NkNullOpt`
(réinitialise) ou `= valeur` (remplace).

Deux outils dominent ce cycle de vie :

- `Emplace(args…)` construit la valeur **en place** par *placement-new* avec parfait *forwarding*,
  après avoir détruit l'éventuelle valeur courante. C'est la voie sans copie ni *move* d'un temporaire
  — `opt.Emplace(10, 'x')` construit un `T(10, 'x')` directement dans le buffer. Retourne une référence
  sur la valeur fraîchement construite.
- `Reset()` détruit la valeur si elle existe (`~T()`) et marque l'optionnel vide. `noexcept`,
  **idempotent** : l'appeler sur un optionnel déjà vide ne fait rien. C'est aussi ce que le destructeur
  appelle (RAII — pas de fuite si `T` détient une ressource).

Usages, par domaine :

- **ECS / gameplay** — la valeur de retour d'un *lookup* de composant ou d'entité (`FindComponent<T>()`
  renvoie `NkOptional<T&>`-like) : présence testée avant usage, jamais de pointeur nul à gérer à la
  main.
- **IA** — la cible courante d'un agent, le prochain point de patrouille : « peut-être aucune » est
  l'état naturel, et `ValueOr` fournit un repli (rester sur place) sans `if`.
- **Animation** — un *override* facultatif sur un os (une rotation imposée par l'IK) : présent ou non,
  sans valeur sentinelle bricolée.
- **IO / parsing** — le résultat d'une conversion qui peut échouer (`ParseInt` → `NkOptional<nk_int32>`)
  : l'échec est dans le type, pas dans un code d'erreur séparé.
- **UI / 2D** — l'élément survolé ou sélectionné (`NkOptional<WidgetId>`) : « rien sous le curseur » est
  un état de première classe.

### `HasValue`, `Empty`, `operator nk_bool` — interroger la présence

`HasValue()` (`[[nodiscard]] noexcept`) répond `true` si une valeur est présente ; `Empty()` est son
inverse exact. La **conversion booléenne est `explicit`** : elle s'active dans un contexte de condition
(`if (opt)`, `while (opt)`, `!opt`) mais **pas** dans une conversion implicite (`bool b = opt;` ne
compile pas — c'est voulu, pour éviter qu'un `NkOptional<bool>` ne se confonde avec sa propre valeur).
Ces trois observateurs sont `O(1)` et ne touchent pas à la valeur.

### `operator*`, `operator->`, `Value` — l'accès non vérifié

Ce sont les voies **directes** vers la valeur : `*opt` et `Value()` donnent une référence (mutable ou
`const`), `opt->m` accède à un membre. Elles ne coûtent rien — un `reinterpret_cast` sur le buffer —
mais **ne vérifient pas** la présence : les utiliser sur un optionnel vide est un **comportement
indéfini**. Le `noexcept` qui les décore ne change rien à ce fait : il promet l'absence d'exception,
pas l'absence de catastrophe. La règle est simple : ne déréférencez qu'après avoir confirmé la présence
(`if (opt)`), ou utilisez les accès sûrs ci-dessous. `Value()` n'est qu'un **alias lisible** de
`operator*`, sans contrôle ajouté — ne vous fiez pas à son nom rassurant.

### `GetIf`, `ValueOr`, `ValueOrRef` — l'accès sûr

Ce trio ne plante **jamais**, quel que soit l'état de l'optionnel :

- `GetIf()` renvoie un pointeur sur la valeur si présente, sinon `nullptr`. C'est l'idiome
  *test-and-use* en une ligne : `if (auto* p = opt.GetIf()) { … }`. Parfait quand l'absence est un cas
  normal qu'on traite sur place.
- `ValueOr(fallback)` renvoie une **copie** de la valeur si présente, sinon le repli. Le `fallback` est
  passé **par valeur** (donc copié) ; idéal pour un défaut scalaire — `nk_float32 vol =
  settings.volume.ValueOr(1.0f);`. Ne modifie pas l'optionnel.
- `ValueOrRef(fallback)` renvoie une **référence** : sur la valeur si présente, sinon sur le `fallback`
  fourni. Il évite la copie quand `T` est lourd — mais attention au piège de durée de vie : ne détruisez
  ni ne modifiez le `fallback` tant que la référence retournée vit (vous pointeriez dans le vide).

Usages, par domaine :

- **Rendu / GPU** — un *override* de matériau facultatif : `mat.ValueOr(defaultMaterial)` donne toujours
  un matériau valide à dessiner, présent ou non.
- **Audio** — le bus de sortie d'une voix (`voice.bus.ValueOr(masterBus)`) : pas de branchement, un repli
  propre.
- **Threading** — la valeur dépilée d'une file non bloquante : `if (auto* job = queue.TryPop().GetIf())`
  enchaîne dépilement et test sans valeur sentinelle.
- **Physique** — le résultat d'un *raycast* facultatif : `GetIf()` distingue « touché » de « rien »
  proprement.

### `Emplace`, `Reset`, `Swap` (NkOptional) — récap utilitaires

`Emplace` et `Reset` ont été décrits plus haut. `Swap(other)` échange les contenus de deux optionnels en
gérant les trois cas (deux valeurs → échange par *moves* temporaires ; une seule → *move* + reset de la
source ; aucune → no-op) ainsi que l'auto-swap. Il **n'est pas** marqué `noexcept`. Utile pour le
*double-buffering* d'un état facultatif, ou pour réordonner sans recopier.

### `NkVariant` — constantes et construction

Deux constantes statiques renseignent le variant : `kTypeCount` = nombre de types du pack, et `kNpos` =
`(nk_size)-1`, l'index « invalide » que `Index()` renvoie quand le variant est vide (calqué sur
`std::variant::npos`).

Le **ctor par défaut** est subtil : il tente d'emplacer le **premier** type `T0` *seulement si* `T0` est
défaut-constructible (`if constexpr (__is_constructible(T0))`) ; sinon le variant reste **vide**
(`ValuelessByException()` vaut `true`). Ne supposez donc pas une valeur après un ctor par défaut sans le
vérifier. Le ctor depuis une **valeur** (`NkVariant(T&& value)`) n'est activé (SFINAE) que si le type
décayé de l'argument appartient au pack ; il choisit le type correspondant et fait un `Emplace`. Copie
et déplacement font une copie/transfert profond du type actif (le déplacement est `noexcept` ; la source
reste valide mais dans un état indéterminé). Les affectations couvrent les mêmes cas, gèrent
l'auto-affectation, et `= valeur` (type dans le pack) change le type actif.

### `Emplace`, `Reset` (NkVariant) — changer de type actif

`Emplace<T>(args…)` est la voie canonique pour **changer** le type contenu : il `Reset()` d'abord, puis
construit `T(args…)` en place (*placement-new*, parfait *forwarding*), met à jour l'index interne et
marque le variant plein. SFINAE garantit que `T` appartient au pack. Garantie de sûreté : si la
construction lève, le variant **reste vide** (état cohérent, jamais à demi-construit). `Reset()` détruit
la valeur active, met l'index à `kNpos` et le variant à vide ; `noexcept`, idempotent, appelé par le
destructeur.

### `HoldsAlternative`, `Index`, `HasValue`, `ValuelessByException` — interroger l'état

`HoldsAlternative<T>()` répond `true` si le type **actif** est exactement `T` — `O(1)` (simple
comparaison d'index), `false` si le variant est vide. `Index()` donne l'index 0-based du type actif (ou
`kNpos` si vide), utile pour un *dispatch* par table ou une sérialisation du discriminant.
`HasValue()` et `ValuelessByException()` sont inverses l'un de l'autre et disent juste s'il y a une
valeur. Tous `[[nodiscard]] noexcept`, `O(1)`.

### `Get`, `GetChecked`, `GetIf` — les trois niveaux d'accès

C'est le cœur de la sûreté du variant, gradué en trois paliers :

- `Get<T>()` (`noexcept`) lit directement la valeur comme un `T`. **Non vérifié** : si le type actif
  n'est pas `T`, c'est un comportement indéfini. Le plus rapide, à réserver aux endroits où vous *venez*
  de tester le type juste avant.
- `GetChecked<T>()` insère un `NK_ASSERT(HoldsAlternative<T>())` avant le `Get<T>()`. En debug, un
  mauvais type **claque** une assertion lisible plutôt qu'une corruption silencieuse ; en release,
  l'assert s'évapore et c'est un `Get<T>()` à coût nul. C'est le défaut raisonnable quand vous êtes
  *presque* sûr du type. (La macro est bien `NK_ASSERT`, pas `NkAssert` ni `NKENTSEU_ASSERT`.)
- `GetIf<T>()` (`noexcept`) renvoie un pointeur sur la valeur si `HoldsAlternative<T>()`, sinon
  `nullptr` (et `nullptr` aussi si vide). **Jamais d'UB** : c'est l'idiome `if (auto* p = v.GetIf<T>())
  { … }`, le bon réflexe quand l'identité du type est *incertaine*.

Usages, par domaine :

- **Gameplay / IA** — une machine à états où chaque état porte ses propres données
  (`NkVariant<Idle, Chase, Flee>`) : `GetIf<Chase>()` accède aux données de poursuite seulement si l'état
  l'est.
- **UI / 2D** — un événement d'entrée hétérogène (`NkVariant<MouseMove, KeyDown, TextInput>`) :
  `HoldsAlternative` route vers le bon traitement.
- **IO / sérialisation** — une valeur JSON-like (`NkVariant<nk_bool, nk_float64, nk_string, …>`) :
  `Index()` sert de discriminant à écrire/relire.
- **Rendu** — un paramètre de matériau typé (`NkVariant<float32, NkVec3, NkTextureHandle>`) :
  `GetIf<NkVec3>()` lit la couleur si c'en est une.

### `Visit` — traiter tous les cas d'un coup

`Visit(visitor)` (et sa surcharge `const`) applique un visiteur à la valeur **active**, quel que soit
son type, par dispatch récursif compile-time relayé à `NkInvoke`. Le visiteur doit être invocable avec
**chacun** des types du pack — sinon le code ne compile pas, ce qui est exactement la garantie
recherchée : impossible d'oublier un cas. L'idiome est un lambda générique avec `if constexpr` interne
pour spécialiser le traitement par type. Le retour est `void`, et `Visit` est un **no-op** si le variant
est vide. C'est la forme la plus expressive : un seul appel exhaustif au lieu d'une cascade de
`if (HoldsAlternative<...>)`.

- **IA / gameplay** — exécuter le comportement de l'état courant d'une FSM sans cascade de tests.
- **Sérialisation** — écrire la valeur active dans un flux, le visiteur ayant une branche par type.
- **UI** — rendre un widget hétérogène en parcourant une liste de `NkVariant` et en visitant chacun.

### Fonctions libres `NkHoldsAlternative`, `NkGetIf`, `NkVisit`

Trois alias-fonctions libres reprennent les méthodes membres avec déduction du pack, pour un style plus
fonctionnel ou générique :

- `NkHoldsAlternative<T>(v)` ≡ `v.HoldsAlternative<T>()`.
- `NkGetIf<T>(&v)` ≡ `v.GetIf<T>()` — **mais prend un pointeur** (`&v`), et renvoie `nullptr` si ce
  pointeur est nul *ou* si le type ne correspond pas. Piège à connaître : la méthode membre `GetIf` ne
  prend rien, la fonction libre `NkGetIf` veut l'**adresse**.
- `NkVisit(visitor, v)` ≡ `v.Visit(visitor)` — **mais le visiteur vient en premier**, le variant en
  second. Cet ordre est l'inverse de l'appel membre (et calqué sur `std::visit`) ; ne l'inversez pas.

### `Swap` (NkVariant)

`Swap(other)` échange les contenus de deux variants via un *move* temporaire (`tmp(NkMove(other)); other
= NkMove(*this); *this = NkMove(tmp)`), en gérant l'auto-swap. **Non** marqué `noexcept`. Utile pour
permuter deux états sans recopie profonde.

---

### Exemple récapitulatif

```cpp
#include "NKCore/NkOptional.h"
#include "NKCore/NkVariant.h"
using namespace nkentseu;

// NkOptional : un lookup qui peut échouer, traité sans pointeur nul ni sentinelle.
NkOptional<nk_int32> hp = Find("hp");
if (auto* p = hp.GetIf()) {            // accès SÛR : nullptr si absent
    ApplyDamage(*p);
}
nk_float32 vol = settings.volume.ValueOr(1.0f);   // repli propre si absent
hp.Reset();                                        // vide à nouveau

// NkVariant : un événement d'entrée hétérogène, traité exhaustivement par Visit.
NkVariant<MouseMove, KeyDown, TextInput> ev = KeyDown{ Key::Space };

if (ev.HoldsAlternative<KeyDown>()) {              // test O(1) du type actif
    Handle(ev.Get<KeyDown>());                     // sûr ici : on vient de tester
}

ev.Visit([](auto&& e) {                            // une branche par type, sans en oublier
    Dispatch(e);
});

if (auto* k = NkGetIf<KeyDown>(&ev)) {             // fonction libre : prend l'ADRESSE
    LogKey(*k);
}
```

---

[← Les traits](Traits.md) · [Index NKCore](README.md) · [Récap NKCore](../NKCore.md) · [Les assertions →](Assertions.md)
