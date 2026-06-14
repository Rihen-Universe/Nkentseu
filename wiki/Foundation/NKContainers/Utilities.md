# Les utilitaires : présence, succès, alternance

> Couche **Foundation** · NKContainers · Trois petits conteneurs qui modélisent non pas
> *plusieurs* valeurs mais **l'incertitude sur une seule** : `NkOptional` (valeur peut-être
> absente), `NkResult` (succès **ou** erreur), `NkVariant` (une valeur parmi plusieurs types).

Les conteneurs séquentiels répondent à « j'ai *beaucoup* de choses, dans quel ordre les ranger ».
Cette page répond à une question plus subtile, qui ne porte que sur **une** valeur : *et si elle
n'était pas là ? et si l'opération avait échoué ? et si je ne savais pas encore de quel type elle
serait ?* Plutôt que de bricoler avec des pointeurs nuls, des codes d'erreur globaux et des `union`
à la main — toutes des sources classiques de bugs et de comportements indéfinis — Nkentseu offre
trois types **valeur**, sans allocation dynamique, qui rendent l'incertitude **explicite dans le
type lui-même**. Le compilateur vous force alors à traiter le cas « absent » ou « erreur » avant de
toucher la valeur.

Les trois sont **zéro-STL**, stockent leur contenu **inline** (un buffer aligné dans l'objet, pas
de `new`) et exposent une API uniquement `PascalCase` — pas de doublons STL minuscules ici,
contrairement aux conteneurs séquentiels. Détail d'architecture : dans NKContainers, `NkOptional`
et `NkVariant` sont des *forwarding headers* purs vers NKCore (zéro duplication), tandis que
`NkResult` est implémenté entièrement dans NKContainers. Pour l'utilisateur, tout vit dans le même
namespace.

- **Namespace** : `nkentseu`
- **Header parapluie** : `#include "NKContainers/NKContainers.h"`

---

## La valeur peut-être absente : `NkOptional<T>`

Très souvent, une fonction « renvoie un `T`… ou rien ». Chercher une entité par son nom (trouvée ou
pas), lire le premier élément d'un conteneur (qui peut être vide), parser un champ facultatif. La
mauvaise réponse historique est de renvoyer un pointeur (`T*`) où `nullptr` signifie « absent » :
on confond alors *propriété*, *absence* et *erreur*, et on s'expose au déréférencement nul.
`NkOptional<T>` modélise proprement ce « peut-être un `T` » : soit il **contient** une valeur, soit
il est **vide**, et il le dit franchement via `HasValue()`.

En interne, c'est un buffer aligné `mStorage` de la taille exacte de `T` plus un drapeau
`mHasValue` : **aucune allocation dynamique**, la valeur vit dans l'optional. Toutes les opérations
sont `O(1)` (au coût de copie/déplacement de `T` près). Pour l'état vide, on utilise la constante
sentinelle `NkNullOpt` :

```cpp
NkOptional<Entity> found = scene.Find("player");
if (found.HasValue())                 // ou simplement : if (found)
    found->Update(dt);                // operator-> : accès direct au T

Entity e = found.ValueOr(defaultEntity);   // valeur, ou repli si vide
```

Pour **mettre** une valeur, le réflexe idiomatique est `Emplace(args...)` : il détruit le contenu
courant et construit `T` *sur place* par *forwarding* parfait, sans temporaire. `Reset()` revide
l'optional. Ce n'est **pas** un pointeur : il n'y a pas d'indirection, pas de propriété partagée,
la valeur est *dans* l'objet.

Le piège majeur tient en une règle : `operator*`, `operator->` et `Value()` **ne vérifient rien**
— les utiliser sur un optional vide est un comportement indéfini (le header l'avertit
explicitement). Pour un accès *sûr*, préférez `GetIf()` (renvoie un pointeur, ou `nullptr` si
vide), `ValueOr(fallback)` (copie de repli) ou `ValueOrRef(fallback)` (référence de repli, sans
copie).

> **En résumé.** `NkOptional<T>` = « un `T`, ou rien », stocké inline sans allocation. Testez avec
> `HasValue()` / `if (opt)`, posez avec `Emplace(...)`, videz avec `Reset()`. Accès sûr via
> `GetIf()` / `ValueOr()` ; `operator*` / `Value()` sur un optional vide = UB. Ce n'est **pas** un
> pointeur nullable.

---

## Le succès ou l'erreur : `NkResult<T, E>`

Une fonction qui peut échouer doit dire **pourquoi**. Les codes d'erreur globaux (`errno`), les
booléens « ça a marché ? » accompagnés d'un paramètre de sortie, ou les exceptions, ont chacun
leurs maux. `NkResult<T, E>` adopte l'approche de Rust (`Result`), de C++23 (`std::expected`) et de
Haskell (`Either`) : un type **somme** qui contient *soit* une valeur de succès `T`, *soit* une
erreur `E` — jamais les deux, jamais ni l'un ni l'autre. Le type de retour porte lui-même la
possibilité d'échec, et le compilateur ne vous laissera pas l'ignorer.

En interne, un `bool mHasValue` plus une `union` qui superpose le stockage de `T` et de `E` :
inline, sans allocation, `O(1)`. On construit un Result via les fabriques **`NkOk(valeur)`** et
**`NkErr(erreur)`**, qui déduisent les types :

```cpp
NkResult<Texture, NkSimpleError> LoadTexture(const char* path) {
    if (!Exists(path))
        return NkErr(NkSimpleError("file not found", 404));
    return NkOk(DecodeImage(path));
}

auto r = LoadTexture("hero.png");
if (r.IsOk())  Use(r.Value());
else           Log(r.GetError().message);
```

La force du type, ce sont ses **opérations monadiques**, qui chaînent des étapes sans empiler les
`if` : `Map(f)` transforme la valeur si Ok (et propage l'erreur sinon), `MapError(f)` transforme
l'erreur, `AndThen(f)` enchaîne une autre opération qui renvoie elle-même un `NkResult`, et
`OrElse(f)` tente une récupération en cas d'erreur. On écrit ainsi un *pipeline* lisible :

```cpp
auto pixels = LoadTexture(path)
                  .Map([](Texture t){ return t.ToPixels(); })
                  .AndThen([](Pixels p){ return Validate(p); })
                  .MapError([](NkSimpleError e){ return Wrap(e); });
```

Ce n'est **pas** un `NkOptional` : un optional dit *présent / absent*, un Result dit *succès /
échec **avec un motif***. Pour la consommation finale, `Unwrap()` extrait la valeur (et **panique**
via assert si on est en erreur), `Expect("message")` fait de même avec un message custom, et leurs
miroirs `UnwrapError()` / `ExpectError(...)` extraient l'erreur. Ces quatre-là sont **rvalue-only**
(ils *consomment* le Result). Pour ne jamais paniquer, `UnwrapOr(fallback)` /
`UnwrapOrElse(callable)` donnent une valeur de repli.

Attention : `Value()`, `GetError()`, `operator*` et `operator->` **assertent** si on les appelle
sur le mauvais état — vérifiez `IsOk()` / `IsErr()` d'abord. Pour les cas simples, le type d'erreur
prêt à l'emploi `NkSimpleError` (un `message` + un `code`) et l'alias
`NkSimpleResult<T> = NkResult<T, NkSimpleError>` évitent de définir son propre `E`. Enfin, les
macros `NKENTSEU_CONTAINERS_TRY(expr)` / `NKENTSEU_CONTAINERS_TRY_RET(expr)` reproduisent
l'opérateur `?` de Rust : elles propagent automatiquement l'erreur d'un sous-appel.

> **En résumé.** `NkResult<T, E>` = « succès `T` **ou** erreur `E` ». Retournez `NkOk(...)` /
> `NkErr(...)`, testez `IsOk()` / `IsErr()`, chaînez avec `Map` / `AndThen` / `MapError` / `OrElse`.
> `Unwrap` / `Expect` consomment et paniquent si mauvais état ; `Value()` / `GetError()` assertent.
> `NkSimpleError` + `NkSimpleResult<T>` pour aller vite. Ce n'est **pas** un optional : il porte le
> *motif* de l'échec.

---

## La valeur parmi plusieurs types : `NkVariant<Ts...>`

Parfois une valeur peut être de **plusieurs types différents**, exclusifs : un message d'événement
qui est *soit* un clic, *soit* une touche, *soit* un redimensionnement ; un nœud d'arbre qui est un
nombre, une chaîne ou un booléen ; une propriété matériau qui est une couleur ou une texture. La
solution C historique est l'`union` brute plus une étiquette `enum` qu'on gère à la main — fragile,
sans appel de destructeur, source de bugs. `NkVariant<Ts...>` est l'**union discriminée type-safe**
(comme `std::variant`) : elle stocke exactement **une** valeur parmi `Ts...`, retient lequel des
types est actif, et appelle les bons constructeurs/destructeurs automatiquement.

En interne : un buffer brut dimensionné et aligné automatiquement sur le plus grand/exigeant des
`Ts...`, plus un index `mIndex` du type actif. Le *dispatch* (destruction, copie, déplacement,
visite) se résout à la **compilation** par récursion `if constexpr` sur l'index. `HoldsAlternative`
et `Index` sont `O(1)` ; copie/déplacement/`Visit` font un dispatch linéaire *compile-time* sur le
nombre de types.

```cpp
NkVariant<int, float, NkString> value;
value = 3.14f;                            // construit le float, mIndex pointe dessus
value.Emplace<NkString>("hello");         // détruit le float, construit la chaîne

if (value.HoldsAlternative<NkString>())
    Print(value.Get<NkString>());         // Get<T> non vérifié — sûr ici car on a testé
```

La façon idiomatique de *poser* une valeur est `Emplace<T>(args...)` (construit `T` sur place,
parfait pour les types non copiables). Pour *lire*, plusieurs niveaux de sûreté : `GetIf<T>()`
renvoie un pointeur ou `nullptr` (jamais d'UB), `HoldsAlternative<T>()` teste le type actif,
`GetChecked<T>()` assert avant l'accès (zéro coût en release), tandis que `Get<T>()` est **non
vérifié** (UB si le type actif n'est pas `T`).

Le vrai pouvoir du variant, c'est le **pattern visiteur** : `Visit(visitor)` applique une fonction
à la valeur active, quel qu'en soit le type. On écrit en général un visiteur générique `auto&&` qui
se spécialise par `if constexpr`, ce qui force à traiter **tous** les cas — exactement comme un
`match` :

```cpp
event.Visit([](auto&& e) {
    using T = NkRemoveCV_t<NkRemoveReference_t<decltype(e)>>;
    if constexpr (NkIsSame_v<T, ClickEvent>)  HandleClick(e);
    else if constexpr (NkIsSame_v<T, KeyEvent>) HandleKey(e);
});
```

Quelques subtilités utiles : le constructeur par défaut *emplace* le **premier type** s'il est
constructible par défaut, sinon le variant reste **vide** — état signalé par `ValuelessByException()`
(compatibilité `std::variant`). La fonction libre `NkVisit` prend le **visiteur d'abord**, le
variant ensuite (`NkVisit(visitor, variant)`), et `NkGetIf` prend un **pointeur** sur le variant
(`NkGetIf<T>(&variant)`), null-safe.

> **En résumé.** `NkVariant<Ts...>` = « une valeur parmi `Ts...` », type-safe, inline, avec
> destruction automatique. Posez avec `Emplace<T>(...)` (ou affectation directe), lisez en sûreté
> via `GetIf<T>()` / `HoldsAlternative<T>()` / `GetChecked<T>()`, dispatchez avec `Visit(...)`.
> `Get<T>()` non vérifié = UB. Ce n'est **pas** une `union` brute : les ctors/dtors sont appelés.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé dans la « Référence
complète » qui suit. Sauf mention, tout est `O(1)` (au coût de copie/déplacement du `T` près).

### `NkOptional<T>` — valeur peut-être absente

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Sentinelle | `NkNullOpt_t`, `NkNullOpt` | Tag et constante d'**absence**. |
| Construction | `NkOptional()`, `NkOptional(NkNullOpt)`, `NkOptional(const T&)`, `NkOptional(T&&)`, copie, déplacement | Vide / vide explicite / copie / déplacement / … (la source d'un move devient vide). |
| Affectation | `operator=(NkNullOpt)`, `operator=(const T&)`, `operator=(T&&)`, copie, déplacement | Réinitialise / pose la valeur / transfère. |
| Modification | `Emplace(args...)`, `Reset()` | Construit **sur place** (idiomatique) / vide (idempotent). |
| Observateurs | `HasValue()`, `Empty()`, `operator bool` | Présente ? / vide ? / test en condition. |
| Accès non vérifié | `operator->`, `operator*`, `Value()` | Accès direct — **UB si vide**. |
| Accès sûr | `GetIf()`, `ValueOr(fallback)`, `ValueOrRef(fallback)` | Pointeur ou `nullptr` / copie de repli / référence de repli. |
| Utilitaire | `Swap(other)` | Échange deux optionals. |

### `NkResult<T, E>` — succès ou erreur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias membres | `ValueType`, `ErrorType`, `ThisType` | `T` / `E` / le type lui-même. |
| Fabriques | `NkOk(value)`, `NkErr(error)`, `NkSuccess<T>`, `NkError<E>` | Construire un Ok / un Err (avec déduction). |
| Construction | depuis `NkSuccess`/`NkError`, copie, déplacement | État Ok ou Err (pas de ctor par défaut). |
| Affectation | `operator=` copie / déplacement | Reconstruit l'état actif. |
| Observateurs | `IsOk()`, `IsErr()`, `operator bool` | Succès ? / erreur ? / `true` = Ok. |
| Accès valeur | `Value()`, `operator*`, `operator->`, `ValueOr(def)`, `ValueOrElse(f)` | Valeur (**assert si Err**) / replis. |
| Accès erreur | `GetError()`, `ErrorOr(def)` | Erreur (**assert si Ok**) / repli. |
| Monadique | `Map(f)`, `MapError(f)`, `AndThen(f)`, `OrElse(f)` | Transformer valeur / erreur ; chaîner ; récupérer. |
| Consommation `&&` | `Unwrap()`, `Expect(msg)`, `UnwrapError()`, `ExpectError(msg)` | Extraire (**panique** si mauvais état). |
| Consommation sûre | `UnwrapOr(fb)`, `UnwrapOrElse(f)` | Valeur ou repli, sans panique. |
| Erreur simple | `NkSimpleError`, `NkSimpleResult<T>` | `message`+`code` (`==` sur le code) ; alias prêt. |
| Macros | `NKENTSEU_CONTAINERS_TRY(e)`, `NKENTSEU_CONTAINERS_TRY_RET(e)` | Propagation auto de l'erreur (style `?`). |

### `NkVariant<Ts...>` — valeur parmi plusieurs types

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constantes | `kTypeCount`, `kNpos` | Nombre de types / index « aucun ». |
| Construction | `NkVariant()`, `NkVariant(T&&)`, copie, déplacement | Emplace le 1ᵉʳ type (si default-constructible) / depuis une valeur du pack. |
| Affectation | `operator=` copie / déplacement / `operator=(T&&)` | Transfère / emplace la valeur. |
| État | `HasValue()`, `ValuelessByException()`, `Index()` | A une valeur ? / vide ? / index actif (ou `kNpos`). |
| Cycle de vie | `Emplace<T>(args...)`, `Reset()` | Construit **sur place** / vide (idempotent). |
| Accès | `HoldsAlternative<T>()` `[O(1)]`, `Get<T>()`, `GetChecked<T>()`, `GetIf<T>()` | Test type / accès **non vérifié** / accès asserté / pointeur ou `nullptr`. |
| Visite | `Visit(visitor)` | Applique le visiteur à la valeur active. |
| Utilitaire | `Swap(other)` | Échange deux variants. |
| Fonctions libres | `NkHoldsAlternative<T>(v)`, `NkGetIf<T>(&v)`, `NkVisit(visitor, v)` | Versions libres (`NkGetIf` prend un **pointeur**, `NkVisit` le **visiteur d'abord**). |

---

## Référence complète

Chaque élément est repris à fond, avec ses usages dans les différents domaines du moteur — rendu,
ECS, physique, animation, gameplay/IA, audio, UI/2D, IO, GPU. Les éléments triviaux (construction,
affectation) sont décrits brièvement ; les opérations importantes le sont en détail.

### `NkOptional` — construction, accès, modification

Un optional naît vide (`NkOptional()` ou `NkOptional(NkNullOpt)`), ou plein à partir d'une valeur
(`NkOptional(value)` copie, `NkOptional(NkMove(value))` déplace). La construction par déplacement
**vide la source**. La modification idiomatique est `Emplace(args...)`, qui détruit le contenu
courant puis construit `T` *in-place* par forwarding parfait — pas de temporaire, idéal pour les
types coûteux ou non copiables. `Reset()` revide, et il est *idempotent* (rappelable sans danger).

Pour tester, `HasValue()` / `Empty()` / `if (opt)` (l'`operator bool` est `explicit`). Pour lire,
deux familles à bien distinguer :

- **Non vérifiée et rapide** — `operator*`, `operator->`, `Value()` (alias de `operator*`) : zéro
  contrôle, **UB sur un optional vide**. À réserver après un `HasValue()`.
- **Sûre** — `GetIf()` renvoie un pointeur (ou `nullptr` si vide) ; `ValueOr(fallback)` renvoie la
  valeur ou une **copie** du repli (le fallback est pris *par valeur*) ; `ValueOrRef(fallback)`
  renvoie une **référence** vers la valeur ou vers le repli (sans copie — mais ne conservez pas
  cette référence si le fallback est un temporaire).

Cas d'usage, par domaine :
- **ECS / scène** — `Find(name)` qui peut ne rien trouver : `NkOptional<EntityHandle>` plutôt qu'un
  handle « invalide » magique ; le composant facultatif d'une entité.
- **Rendu / GPU** — la texture *override* d'un matériau (présente ou héritée), la cible de rendu
  secondaire d'une passe, une feature optionnelle interrogée sur le device.
- **Animation** — la valeur d'une courbe à un instant qui peut tomber hors de sa plage de clés ;
  l'os parent (la racine n'en a pas).
- **Gameplay / IA** — la cible courante d'un agent (peut ne pas en avoir), le résultat d'un
  *raycast* qui peut ne rien toucher.
- **UI / 2D** — le widget survolé, l'élément focalisé, la sélection courante.
- **IO** — un champ facultatif d'un fichier de config ; le résultat d'un parse qui peut échouer
  *silencieusement* (sans motif — sinon préférez `NkResult`).

### `NkResult` — fabriques, observateurs, accès

On ne construit jamais un Result « vide » : il est **toujours** Ok ou Err (pas de ctor par défaut).
On le produit par les fabriques `NkOk(value)` et `NkErr(error)` (qui déduisent les types via les
tags `NkSuccess<T>` / `NkError<E>`), typiquement en `return` d'une fonction `NkResult<T, E>`. Les
alias membres `ValueType` / `ErrorType` / `ThisType` servent à la métaprogrammation.

Pour distinguer les deux états : `IsOk()`, `IsErr()`, ou l'`operator bool` (`true` = Ok). **Avant**
tout accès, vérifiez : `Value()` / `operator*` / `operator->` **assertent** sur un Err
(« Value() called on Error state »), et `GetError()` **assert** sur un Ok. Les variantes
tolérantes ne paniquent jamais : `ValueOr(def)` (valeur ou repli casté vers `T`),
`ValueOrElse(callable)` (valeur ou résultat d'un *callable*), `ErrorOr(def)` côté erreur.

Cas d'usage, par domaine :
- **IO / chargement d'assets** — `LoadTexture`, `ParseScene`, `OpenFile` : succès = la ressource,
  erreur = le motif (fichier manquant, format invalide, version incompatible). Le terrain de
  prédilection du Result.
- **GPU** — création d'un pipeline, compilation d'un shader, allocation d'un buffer : l'échec porte
  un message exploitable (log, fallback) plutôt qu'un crash.
- **Réseau / sérialisation** — décodage d'un paquet ou d'un JSON : Ok = l'objet, Err = l'erreur de
  protocole/parse.
- **Gameplay** — une action qui peut être refusée (pas assez de ressources, hors de portée) :
  `NkResult<void-ish, ReasonCode>` dit *pourquoi* elle a échoué.
- **Outils / éditeur** — validation d'une opération avant de l'appliquer ; le motif d'erreur
  remonte directement à l'UI.

### `NkResult` — opérations monadiques

C'est ici que le Result dépasse le simple « code retour ». Les quatre opérations se déclinent en
surcharges `&` / `const &` / `&&` et permettent d'enchaîner sans empiler les `if` :

- **`Map(f)`** — applique `f` à la **valeur** si Ok (le type de retour devient
  `NkResult<decltype(f(value)), E>`), et **propage l'erreur** telle quelle sinon. Pour transformer
  un succès en un autre succès : un `Texture` en `Pixels`, un octet brut en structure décodée.
- **`MapError(f)`** — symétrique : transforme l'**erreur** si Err (vers
  `NkResult<T, decltype(f(error))>`), propage la valeur sinon. Pour *traduire* une erreur bas niveau
  en erreur métier (envelopper un code OS dans une `NkSimpleError` parlante).
- **`AndThen(f)`** — chaîne une étape qui peut **elle aussi échouer** : `f` doit renvoyer un
  `NkResult`, exécuté seulement si Ok, l'erreur étant propagée sinon. C'est le maillon des
  *pipelines* : charger **puis** valider **puis** uploader, chacun pouvant casser.
- **`OrElse(f)`** — la **récupération** : si Err, exécute `f` (qui renvoie un `NkResult`) pour tenter
  une alternative ; propage la valeur si déjà Ok. Charger depuis le cache, *ou sinon* depuis le
  disque, *ou sinon* générer un défaut.

Domaines : pipelines d'assets (rendu/IO/GPU), décodage réseau en plusieurs étapes, validation
d'entrées en cascade, recovery de configuration. Le chaînage
`NkOk(x).Map(...).AndThen(...).MapError(...).OrElse(...)` reste lisible là où des `if`/`else`
imbriqués deviendraient illisibles.

### `NkResult` — consommation et erreur simple

Pour *sortir* la valeur d'un Result, les fonctions de consommation sont **rvalue-only** (elles
consomment le Result) : `Unwrap()` extrait la valeur et **panique** (assert) si Err ; `Expect(msg)`
fait pareil avec un message custom (utile pour documenter l'invariant : « ce fichier embarqué
existe toujours ») ; `UnwrapError()` / `ExpectError(msg)` extraient l'erreur et paniquent si Ok.
Quand la panique n'est pas acceptable, `UnwrapOr(fallback)` et `UnwrapOrElse(callable)` donnent
toujours une valeur.

Pour ne pas avoir à définir son propre `E` à chaque fois, `NkSimpleError` regroupe un
`const char* message` et un `nk_int32 code` (par défaut `"Unknown error"`, `-1`). Subtilité :
son `operator==` compare **uniquement le code**, pas le message — pratique pour matcher une
catégorie d'erreur. L'alias `NkSimpleResult<T> = NkResult<T, NkSimpleError>` couvre 90 % des
besoins courants.

Enfin, les macros `NKENTSEU_CONTAINERS_TRY(expr)` et `NKENTSEU_CONTAINERS_TRY_RET(expr)` imitent
l'opérateur `?` de Rust à l'intérieur d'une fonction renvoyant un `NkResult` : si `expr` est Err,
elles `return` l'erreur immédiatement ; sinon elles *bindent* la valeur (`TRY`) ou la renvoient
en `NkOk` (`TRY_RET`). Idéal pour enchaîner des appels faillibles sans bruit syntaxique dans un
chargeur d'assets ou un décodeur.

### `NkVariant` — construction, état, modification

Le constructeur par défaut *emplace* le **premier type** `T0` s'il est constructible par défaut ;
sinon le variant reste **vide**. On peut aussi le construire directement depuis une valeur dont le
type décayé appartient au pack (`NkVariant<int, NkString> v = NkString("x");`). La modification
idiomatique est `Emplace<T>(args...)` : il `Reset()` l'état courant puis construit `T` sur place
(parfait pour les types non copiables), met à jour l'index actif et renvoie la valeur. `Reset()`
détruit la valeur active (dispatch par index) et revide, de façon idempotente.

Pour l'état : `HasValue()`, son inverse `ValuelessByException()` (compatibilité `std::variant`,
l'état vide), et `Index()` qui renvoie l'index 0-based du type actif ou `kNpos` si vide. Les
constantes `kTypeCount` (`sizeof...(Ts)`) et `kNpos` complètent l'introspection.

### `NkVariant` — accès et visite

Quatre niveaux d'accès, du plus sûr au plus rapide :
- `HoldsAlternative<T>()` — `O(1)`, teste si `T` est le type actif (`false` si vide).
- `GetIf<T>()` — renvoie un **pointeur** vers la valeur, ou `nullptr` si le type actif n'est pas `T`
  (ou si vide) : **jamais d'UB**, c'est l'accès sûr par défaut.
- `GetChecked<T>()` — `NK_ASSERT(HoldsAlternative<T>())` puis accès : sûr en debug, **zéro coût en
  release**.
- `Get<T>()` — accès **non vérifié**, **UB** si le type actif n'est pas `T`. À réserver après un
  test.

Mais l'outil reine est `Visit(visitor)` : il applique le visiteur à la valeur active (via
`NkInvoke`), quel que soit son type, et ne fait **rien** sur un variant vide. On écrit en général un
visiteur générique `auto&&` discriminé par `if constexpr`, ce qui **force** à couvrir tous les cas
— l'équivalent d'un `match` exhaustif.

Cas d'usage, par domaine :
- **Événements / UI** — un `NkVariant<ClickEvent, KeyEvent, ResizeEvent, ...>` dispatché par
  `Visit` : un seul point de traitement, type-safe, sans `dynamic_cast`.
- **Sérialisation / config** — un nœud de document qui est nombre, chaîne, booléen, liste ou objet :
  le variant modélise un JSON/arbre de valeurs hétérogènes.
- **Rendu / matériaux** — une propriété qui est `Color` *ou* `TextureHandle` *ou* `float` ; un
  paramètre d'uniform de type variable.
- **Animation** — une piste dont la valeur est `float`, `NkVec3` ou `NkQuat` selon le canal animé.
- **Gameplay / IA** — la valeur d'un *blackboard* (entier, vecteur, entité…) ; le paramètre d'une
  commande/action.
- **Audio** — un paramètre de DSP qui peut être un scalaire, une enveloppe ou une courbe.

### `NkVariant` — fonctions libres

Trois helpers libres complètent l'API membre, avec des conventions d'appel à retenir :
- `NkHoldsAlternative<T>(variant)` — version libre de `HoldsAlternative`.
- `NkGetIf<T>(&variant)` — prend un **pointeur** sur le variant (null-safe : renvoie `nullptr` si le
  pointeur est nul), équivalent libre de `GetIf`.
- `NkVisit(visitor, variant)` — applique le visiteur ; **signature inversée** par rapport à la
  méthode : le **visiteur d'abord**, le variant ensuite (calque `std::visit`).

Les helpers internes (`NkMaxSizeOf`, `NkMaxAlignOf`, `NkTypeIndex`, `NkTypeAt`, `NkContainsType_v`,
`NkAlwaysFalse`) vivent dans `nkentseu::detail` : exposés mais d'**usage interne**, leur API peut
changer — ne vous y appuyez pas.

### Le socle commun

- **Stockage inline, zéro allocation.** Les trois types logent leur contenu dans un buffer aligné de
  l'objet : pas de `new`, pas d'indirection, compatibles avec les boucles chaudes et le
  *data-oriented design*. Voir [NKMemory](../NKMemory.md) pour la philosophie d'allocation.
- **API `PascalCase` uniquement.** Contrairement aux conteneurs séquentiels, ces trois-là n'ont
  **pas** de doublons STL minuscules — ce ne sont pas des conteneurs itérables.
- **Sémantique de déplacement.** Construction/affectation par déplacement transfèrent le contenu et
  vident la source (pour `NkOptional` notamment), d'où l'intérêt de `NkMove`.
- **Politique d'erreur.** Les accès incorrects (optional vide, mauvais état d'un Result, mauvais
  type d'un variant) déclenchent une **assertion** (debug) ou un **UB documenté** selon la méthode
  — d'où l'importance des accesseurs sûrs `GetIf` / `ValueOr` / `HoldsAlternative`.

---

### Exemple récapitulatif

```cpp
#include "NKContainers/NKContainers.h"
using namespace nkentseu;

// Optional : une recherche qui peut ne rien trouver.
NkOptional<Entity> hero = scene.Find("hero");
if (hero) hero->Update(dt);
Entity e = hero.ValueOr(fallbackEntity);          // accès sûr

// Result : un chargement faillible, chaîné en pipeline.
NkSimpleResult<Texture> LoadAndPrepare(const char* path) {
    return LoadTexture(path)                       // NkResult<Texture, NkSimpleError>
        .Map([](Texture t){ return t.GenerateMips(); })
        .AndThen([](Texture t){ return Validate(t); });
}
auto r = LoadAndPrepare("hero.png");
if (r.IsOk()) Use(r.Value());
else          Log(r.GetError().message);

// Variant : un événement dispatché par visite exhaustive.
NkVariant<ClickEvent, KeyEvent> ev = KeyEvent{ Key::Space };
ev.Visit([](auto&& e) {
    using T = NkRemoveCV_t<NkRemoveReference_t<decltype(e)>>;
    if constexpr (NkIsSame_v<T, ClickEvent>) HandleClick(e);
    else if constexpr (NkIsSame_v<T, KeyEvent>) HandleKey(e);
});
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [Associatifs →](Associative.md)
