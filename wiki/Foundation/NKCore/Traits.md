# Les traits de types

> Couche **Foundation** · NKCore · Poser des questions sur un **type à la compilation** et en
> **fabriquer** d'autres : l'équivalent maison, zéro-STL, de `<type_traits>`, plus `NkInvoke`
> (appeler n'importe quel invocable) et les utilitaires de types (`NkTypeUtils.h`).

Quand on écrit du code générique, on bute vite sur une question gênante : *le code que je veux
écrire dépend de la **nature** de `T`*. Un conteneur voudrait copier un bloc de `int` d'un coup
(`memcpy`) mais appeler le constructeur de copie pour des objets non triviaux ; un sérialiseur
voudrait traiter un `enum` comme son entier sous-jacent mais une classe comme une agrégation de
champs ; une fabrique d'événements voudrait n'accepter que les types qui dérivent de `NkEvent`. Les
*traits* répondent à ces questions **à la compilation**, sans le moindre coût à l'exécution : tout
est décidé par le compilateur, et le binaire final ne contient que la branche retenue.

Comme le reste de Nkentseu, NKCore en fournit une bibliothèque **maison** — pas un `#include
<type_traits>`. La convention suit celle de la STL : un trait qui pose une **question** expose une
constante `::value` (un booléen compile-time), doublée d'une variable `NkXxx_v` ; un trait qui
**transforme** un type expose un `::type`, doublé d'un alias `NkXxx_t`. Ce **n'est pas** de la
réflexion runtime (voir NKReflection pour cela) : un trait ne connaît rien des *noms* de champs ni
ne lit la mémoire — il raisonne uniquement sur la **forme statique** du type.

- **Namespaces** : `nkentseu::traits` (traits), `nkentseu` (`NkInvoke`, helpers de cast),
  `nkentseu::literals` (littéraux UDL). Alias pratique : `nkentseu::traits_alias`.
- **Headers** : `#include "NKCore/NkTraits.h"`, `#include "NKCore/NkInvoke.h"`,
  `#include "NKCore/NkTypeUtils.h"`.

---

## Interroger un type

La famille la plus utilisée répond à « de quelle nature est ce type ? ». `NkIsIntegral<T>`,
`NkIsFloatingPoint<T>`, `NkIsArithmetic<T>` (entier *ou* flottant), `NkIsPointer<T>`,
`NkIsReference<T>`, `NkIsArray<T>`, `NkIsEnum<T>`, `NkIsClass<T>`, `NkIsUnion<T>`,
`NkIsFunction<T>`… chacun vaut `true` ou `false` selon la catégorie de `T`. D'autres interrogent ses
**propriétés** : `NkIsConst<T>`, `NkIsVolatile<T>`, `NkIsSigned<T>`/`NkIsUnsigned<T>`, ou encore les
traits qui décident des optimisations de copie — `NkIsTrivial<T>`, `NkIsTriviallyCopyable<T>`,
`NkIsPOD<T>`, `NkIsStandardLayout<T>`.

Une autre famille interroge les **relations** entre types : `NkIsSame<A, B>` (sont-ils
identiques ?), `NkIsBaseOf<Base, Derived>` (y a-t-il héritage ?), `NkIsAnyOf<T, Ts...>` (`T`
figure-t-il dans la liste ?). On y trouve aussi de quoi tester la constructibilité —
`NkIsDefaultConstructible`, `NkIsCopyConstructible`, `NkIsMoveConstructible`, `NkIsAssignable<T, U>`.

Chaque question existe en deux formes : le **trait-struct** (`NkIsIntegral<T>::value`) et la
**variable** (`NkIsIntegral_v<T>`), plus lisible. Méfiez-vous d'une nuance : `NkIsPointer_v<T>`
applique `NkRemoveCV_t` avant de tester, alors que la struct `NkIsPointer<T>` non — pour un pointeur
const, préférez toujours le `_v`.

> **En résumé.** Les `NkIs…` posent une question booléenne sur la forme statique d'un type :
> sa **catégorie** (`NkIsIntegral`, `NkIsPointer`, `NkIsEnum`…), ses **propriétés** (`NkIsConst`,
> `NkIsSigned`, `NkIsTrivial`…) ou ses **relations** (`NkIsSame`, `NkIsBaseOf`, `NkIsAnyOf`).
> Utilisez la variante `_v` plutôt que `::value`.

---

## Transformer un type

L'autre versant des traits ne pose pas de question mais **fabrique** un type. `NkRemoveReference<T>`
retire `&`/`&&`, `NkRemoveCV<T>` retire `const`/`volatile`, `NkDecay<T>` fait le grand nettoyage
(références + qualificatifs retirés, tableau → pointeur, fonction → pointeur de fonction),
`NkAddPointer<T>` ajoute une `*`, `NkConditional<B, T, F>` choisit `T` ou `F` selon une condition
compile-time. Tous exposent un alias `_t` (`NkDecay_t<T>`) qu'on emploie en pratique.

Le plus emblématique est `NkEnableIf<B, T>`, le pivot du **SFINAE** : il n'expose un `::type` que si
`B` vaut `true`, ce qui permet d'activer ou de désactiver une surcharge de fonction selon une
condition.

```cpp
// Cette surcharge n'existe que pour les types entiers.
template <typename T, typename = NkEnableIf_t<NkIsIntegral_v<T>>>
void PrintHex(T v) { /* ... */ }
```

Si on appelle `PrintHex` avec un flottant, cette surcharge disparaît purement et simplement de
l'ensemble des candidats — le compilateur ne la voit même pas. Ce **n'est pas** une erreur : c'est
la *substitution failure* du SFINAE, silencieuse et voulue. Pour les cas courants, NKCore offre des
macros raccourcies : `NK_ENABLE_IF_T(cond)`, `NK_IS_BASE_OF_V(Base, Derived)`, `NK_IS_CONST_V(T)`.

Pour combiner plusieurs conditions, `NkConjunction<...>` (un ET logique, court-circuité) et
`NkDisjunction<...>` (un OU) s'enchaînent, et `NkNegation<B>` inverse. Tout repose sur une base
commune, `NkIntegralConstant<T, V>`, qui encapsule une valeur constante dans un type — `NkTrueType`
et `NkFalseType` en sont les deux instances booléennes.

> **En résumé.** Les traits de transformation rendent un nouveau type via `::type` / alias `_t` :
> `NkRemoveReference`, `NkRemoveCV`, `NkDecay`, `NkAddPointer`, `NkConditional`. `NkEnableIf`
> (SFINAE) active/désactive des surcharges ; `NkConjunction`/`NkDisjunction`/`NkNegation` combinent
> des conditions. Le socle est `NkIntegralConstant` (et `NkTrueType`/`NkFalseType`).

---

## Forwarder et déplacer : NkForward, NkMove, NkSwap

Écrire du code générique correct, c'est aussi **transmettre des arguments sans perdre leur nature**
(lvalue ou rvalue). `NkForward<T>(x)` est le *perfect forwarding* (équivalent `std::forward`) :
employé sur un paramètre `T&&`, il préserve la catégorie de valeur d'origine. `NkMove(x)` (équivalent
`std::move`) signale au contraire qu'on **abandonne** `x` pour transférer son contenu — c'est lui qui
déclenche les constructeurs et affectations par **déplacement** des conteneurs NKContainers. `NkSwap(a,
b)` échange deux valeurs par déplacement, avec un `noexcept` conditionné à la nature *move* des types.

`NkDeclval<T>()` (non défini, à n'utiliser que dans un `decltype`) fabrique une valeur fictive d'un
type pour interroger une expression sans l'instancier ; `NkDeclVal<T>` est l'alias-type associé
(attention à la casse : **`NkDeclval`** la fonction, **`NkDeclVal`** l'alias).

> **En résumé.** `NkForward` préserve lvalue/rvalue (forwarding), `NkMove` transfère le contenu
> (déplacement), `NkSwap` échange par move. `NkDeclval<T>()` sert dans un `decltype` pour sonder une
> expression sans l'évaluer.

---

## Appeler n'importe quoi : NkInvoke

`NkInvoke.h` apporte une touche fonctionnelle : `NkInvoke` appelle de façon **uniforme** n'importe
quel invocable — une fonction libre, une lambda, un foncteur, un **pointeur de méthode**, un
**pointeur de membre données** — avec la même syntaxe. C'est l'équivalent de `std::invoke`, et la
brique sur laquelle s'appuient les mécanismes de dispatch génériques du moteur (le `Visit` d'un
variant, l'appel d'un *callback* stocké, l'invocation d'un système ECS).

```cpp
NkInvoke(callable, args...);                 // appel générique
using R = NkInvokeResult_t<F, Args...>;      // type de retour exact (réfs/cv préservés)
// NkIsInvocable_v<F, Args...> -> peut-on appeler F avec ces arguments ?
```

L'astuce des pointeurs de membre : `NkInvoke(&T::method, obj, args...)` accepte `obj` comme
**référence ou pointeur** (normalisé en interne), et `NkInvoke(&T::field, obj)` rend une **référence**
au champ, lisible ou modifiable. Ce **n'est pas** un appel vérifié : l'objet doit être non-null,
sinon comportement indéfini. `NkIsInvocable` ne teste que la validité **syntaxique** (pas `noexcept`).

> **En résumé.** `NkInvoke` unifie l'appel des trois familles d'invocables (callable libre, pointeur
> de fonction membre, pointeur de membre données). `NkInvokeResult_t` en donne le type de retour
> exact ; `NkIsInvocable_v` dit si l'appel compile.

---

## Macros et littéraux : NkTypeUtils

`NkTypeUtils.h` complète les traits par des **macros utilitaires** (bits, alignement, offset),
des **casts sécurisés**, des **littéraux typés** et une batterie de `static_assert` qui verrouille
les tailles des types primitifs au build. C'est l'outillage bas-niveau de manipulation de types et
de bits, distinct de la métaprogrammation des deux autres headers.

> **En résumé.** `NkTypeUtils.h` = macros de bits/alignement (`NK_BIT`, `NK_ALIGN_UP`,
> `NK_SET_BIT`…), casts sûrs (`NK_SAFE_CAST_TO_U32`, `NK_DYNAMIC_CAST`…), littéraux (`NK_LITERAL`,
> UDL `_u32`/`_f32`/`_c8`…) et validation compile-time des types primitifs.

---

## Aperçu de l'API

La liste de **tous** les éléments publics. Chaque entrée est détaillée dans la « Référence complète »
qui suit. Les traits-struct ont (sauf mention) un alias `_t` (types) ou une variable `_v` (booléens).

### `NkTraits.h` — métaprogrammation de types (`nkentseu::traits`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Base | `NkIntegralConstant<T, V>` | Constante intégrale compile-time (`value`/`value_type`/`type`, opérateurs). |
| Base | `NkBoolConstant<V>`, `NkTrueType`, `NkFalseType` (`NkTrueType_v`, `NkFalseType_v`) | Constantes booléennes de type. |
| Base | `NkVoidT<...>` | Helper SFINAE (= `void`). |
| Base | `NkTypeIdentity<T>` / `_t` | Contexte non-déduit. |
| Conditionnel | `NkEnableIf<B, T>` / `_t` | SFINAE : `::type` seulement si `B`. |
| Conditionnel | `NkConditional<B, T, F>` / `_t` | Choisit `T` (vrai) ou `F` (faux). |
| Comparaison | `NkIsSame<T, U>` / `_v` | Types identiques (cv compris) ? |
| Modif. type | `NkRemoveConst` / `NkRemoveVolatile` / `NkRemoveCV` (`_t`) | Retire `const` / `volatile` / les deux. |
| Modif. type | `NkRemoveReference` / `_t` | Retire `&` et `&&`. |
| Modif. type | `NkAddLValueReference` / `NkAddRValueReference` (`_t`) | Ajoute `&` / `&&` (void inchangé). |
| Modif. type | `NkRemovePointer` / `_t` | Retire `*` (toutes formes cv). |
| Modif. type | `NkRemoveExtent` / `_t` | `T[N]`/`T[]` → `T`. |
| Classif. | `NkIsVoid` / `NkIsNullPointer` (`_v`) | `void` ? / `decltype(nullptr)` ? |
| Classif. | `NkIsConst` / `NkIsVolatile` (`_v`) | Qualifié `const` / `volatile` ? |
| Classif. | `NkIsLValueReference` / `NkIsRValueReference` / `NkIsReference` (`_v`) | Référence (l/r/quelconque) ? |
| Classif. | `NkIsPointer` (`_v` applique `RemoveCV`) | Pointeur ? |
| Classif. | `NkIsArray` (`_v`) | Tableau `T[]`/`T[N]` ? |
| Arithm. | `NkIsIntegral` / `NkIsFloatingPoint` / `NkIsArithmetic` (`_v`) | Entier / flottant / l'un des deux ? |
| Arithm. | `NkIsSigned` / `NkIsUnsigned` (`_v`) | Signé / non signé ? |
| Composition | `NkIsEnum` / `NkIsClass` / `NkIsUnion` / `NkIsFunction` (`_v`) | `enum` / classe / union / fonction ? |
| Transform. | `NkAddPointer` / `_t` | `NkRemoveReference_t<T>*`. |
| Logique | `NkConjunction<...>` / `NkDisjunction<...>` / `NkNegation<B>` | ET / OU (court-circuit) / NON. |
| Logique | `NkIsAnyOf<T, Ts...>` / `_v` | `T` figure dans la liste ? |
| Decay | `NkDecay<T>` / `_t` (`NkDecayT` legacy) | Nettoyage complet (réf/cv/array/func). |
| Déduction | `NkDeclval<T>()` (fn) / `NkDeclVal<T>` (alias) | Valeur fictive pour `decltype`. |
| Déduction | `NkHasSize<T>` | Détecte `.Size()` (SFINAE). |
| Forwarding | `NkForward<T>` / `NkMove<T>` / `NkSwap(a, b)` | Forward / move / échange. |
| Macros | `NK_DECL_TYPE(...)` (`NkDeclType` legacy) | `decltype(...)`. |
| Macros | `NK_ENABLE_IF_T(cond)` / `NK_IS_BASE_OF_V(B, D)` / `NK_IS_CONST_V(T)` | Raccourcis SFINAE. |
| NKCore | `NkIsCharacterType` / `NkIsValidCharType` (`_v`) | Type caractère ? |
| NKCore | `NkIsFundamental` / `NkIsScalar` / `NkIsObject` / `NkIsCompound` (`_v`) | Catégories larges. |
| NKCore | `NkIsBaseOf<Base, Derived>` / `_v` | Héritage ? |
| NKCore | `NkNounType<T>` / `_t` | `RemoveCV(RemoveReference(T))`. |
| NKCore | `NkIsPlatformSupported` / `_v` | Type supporté (int128 selon plateforme). |
| Constructibilité | `NkIsTriviallyCopyable` (`_v`) | Approx. maison (arith \|\| ptr). |
| Constructibilité | `NkIsCopyConstructible` / `NkIsMoveConstructible` / `NkIsTriviallyMoveConstructible` (`_v`) | Copie/déplacement constructible. |
| Constructibilité | `NkIsTriviallyDestructible<T>` (**variable**) | Destructible trivialement. |
| Constructibilité | `NkIsTriviallyConstructible_v<T>` (**variable**) | Constructible trivialement. |
| Extensions | `NkIsDefaultConstructible` / `NkIsAssignable<T, U>` (`_v`) | Défaut-constructible / assignable. |
| Extensions | `NkIsTrivial` / `NkIsStandardLayout` / `NkIsPOD` (`_v`) | Trivial / standard-layout / POD. |
| Extensions | `NkIsEmpty` / `NkIsPolymorphic` (`_v`) | Vide / polymorphe ? |
| Extensions | `NkBitSize<T>` / `_v` | Taille en **bits** (`sizeof*8`). |

### `NkInvoke.h` — appel uniforme (`nkentseu`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Invocation | `NkInvoke(f, args...)` | Callable libre / pointeur de méthode / pointeur de membre données. |
| Trait | `NkInvokeResult_t<F, Args...>` | Type de retour exact de l'appel. |
| Trait | `NkIsInvocable<F, Args...>` / `NkIsInvocable_v` | L'appel compile-t-il ? |

### `NkTypeUtils.h` — macros, casts, littéraux (global · `nkentseu` · `nkentseu::literals`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Bits/align | `NK_BIT(x)`, `NK_ALIGN_UP(x,a)`, `NK_ALIGN_DOWN(x,a)`, `NK_IS_ALIGNED(n,a)` | Masque de bit / arrondi d'alignement / test. |
| Bits | `NK_SET_BIT` / `NK_CLEAR_BIT` / `NK_TEST_BIT` / `NK_TOGGLE_BIT` | Manipulation d'un bit dans un drapeau. |
| Tableaux | `NK_ARRAY_SIZE(a)`, `NK_STR_BOOL(b)` | Taille tableau statique / `"True"`/`"False"`. |
| Offset | `NK_OFFSET_OF(type, m)`, `NK_CONTAINER_OF(ptr, type, m)` | Offset de membre / remonter au conteneur. |
| Constantes | `NKENTSEU_BIT_MASK_8` / `_32` / `NKENTSEU_BIT_SHIFT` (`nkentseu`) | Masques et décalage. |
| Casts sûrs | `NK_SAFE_CAST_TO_U8/16/32/64`, `…_I8/16/32/64` | Cast clampé aux bornes du type cible. |
| Casts | `NK_STATIC_CAST` / `NK_REINTERPRET_CAST` / `NK_CONST_CAST` / `NK_C_CAST` / `NK_DYNAMIC_CAST` | Casts C++ macro-isés (dynamic → static sans RTTI). |
| Littéraux | `NK_LITERAL(CharT, str)`, `NK_CONST_CHAR(type, str)`, `NK_CONST_CHAR8/16/32`, `NK_CONST_WCHAR`, `NK_CONST_NK_CHAR` | Préfixe de chaîne choisi selon le type caractère. |
| UDL (`literals`) | `_u8/_u16/_u32/_u64`, `_i8/_i16/_i32/_i64`, `_f32/_f64/_f80`, `_b32`, `_cb/_c8/_c16/_c32`, `_cw`, `_u128/_i128`, `_b` | Littéraux typés (entiers/flottants/bool/caractères/byte). |
| Handles (`nkentseu`) | `NkToHandle<T>(ptr)`, `NkFromHandle<T>(h)`, `NkSafeCast<To, From>(v)` | Conversions ptr↔`NkHandle` et cast typé. |
| Validation | `NK_STATIC_ASSERT(name, cond, msg)` | `static_assert` nommé + vérifs de tailles des primitifs. |

---

## Référence complète

Voici le vrai cours. Les éléments triviaux sont expédiés en une phrase ; les pivots — `NkEnableIf`,
`NkInvoke`, les traits de copie — sont traités à fond, avec leurs usages dans les différents domaines
du moteur.

### `NkIntegralConstant` et les constantes de type — le socle

`NkIntegralConstant<T, V>` emballe une valeur connue à la compilation **dans un type** : elle expose
`static constexpr T value = V`, l'alias `value_type`, un `type` qui se renvoie lui-même, et deux
opérateurs (`operator value_type()` et `operator()()`) qui restituent `value`. Tous les traits
booléens en dérivent via `NkBoolConstant<V>`, dont `NkTrueType` et `NkFalseType` sont les deux
incarnations (avec les variables `NkTrueType_v == true`, `NkFalseType_v == false`). C'est ce qui
permet à un trait d'**être** un type (utilisable en argument template, en spécialisation) tout en
**portant** un booléen. `NkVoidT<...>` (toujours `void`) et `NkTypeIdentity<T>` (un type tel quel,
en contexte non-déduit) complètent la trousse SFINAE.

- **Partout** : ce sont les briques internes — vous les croisez rarement directement, mais toute
  spécialisation de trait maison (dans ECS, NKContainers, NKReflection) hérite de l'un d'eux.

### `NkEnableIf` et `NkConditional` — décider à la compilation

`NkConditional<B, T, F>` est un `if` de types : `NkConditional_t<B, T, F>` vaut `T` si `B`, sinon
`F`. On l'emploie pour **choisir une représentation** selon le type — un index `nk_uint16` ou
`nk_uint32` selon la taille d'un pool, un stockage *inline* ou *heap* selon `sizeof(T)`.

`NkEnableIf<B, T>` est le **pivot du SFINAE** : `NkEnableIf_t<B>` n'existe (`= T`, défaut `void`) que
si `B` est vrai ; sinon la substitution échoue et la surcharge **disparaît silencieusement**. C'est
le mécanisme par lequel on écrit une fonction générique qui ne s'active que pour certains types.

- **Rendu / GPU** : une fonction d'upload n'accepte que les `NkIsTriviallyCopyable` (qu'on peut
  `memcpy` vers un buffer GPU) ; un type complexe prend une autre surcharge.
- **ECS** : `RegisterComponent<T>` ne s'ouvre qu'aux `NkIsStandardLayout` (composants POD bien
  alignés dans l'archétype) ; les `NkConditional` choisissent la stratégie de stockage par taille.
- **Sérialisation / IO** : une surcharge d'écriture binaire pour `NkIsArithmetic`, une autre pour les
  classes — sélectionnées sans `if` runtime.
- **UI / 2D, audio, threading** : `NkEnableIf` filtre les *handlers* d'événements selon leur
  signature, les formats d'échantillon selon `NkIsFloatingPoint`, les jobs selon leur invocabilité.

### `NkIsSame`, `NkIsBaseOf`, `NkIsAnyOf` — les relations

`NkIsSame<T, U>` teste l'identité stricte (cv compris). `NkIsBaseOf<Base, Derived>` détecte
l'héritage (via l'intrinsèque compilateur). `NkIsAnyOf<T, Ts...>` (= `NkDisjunction<NkIsSame<T,
Ts>...>`) teste l'appartenance à une liste.

- **Gameplay / événements** : un `EventBus` n'accepte un abonné que si `NkIsBaseOf_v<NkEvent, E>` ;
  un dispatcheur de messages réagit à `NkIsAnyOf<Msg, Spawn, Despawn, Damage>`.
- **Rendu / ressources** : une fabrique d'assets route selon `NkIsBaseOf<NkResource, T>` ; un cache
  typé compare `NkIsSame` pour éviter un re-cast.
- **Physique, animation** : filtrer les *colliders* ou les pistes d'anim par hiérarchie de classes
  sans `dynamic_cast` runtime.

### Modification et nettoyage de type — `NkRemove*`, `NkDecay`, `NkAddPointer`

`NkRemoveReference`, `NkRemoveConst`, `NkRemoveVolatile`, `NkRemoveCV` retirent un qualificatif ;
`NkRemovePointer` retire une indirection (toutes formes cv) ; `NkRemoveExtent` déballe une dimension
de tableau ; `NkAddLValueReference`/`NkAddRValueReference` ajoutent `&`/`&&` (laissant `void`
intact) ; `NkAddPointer` rend `NkRemoveReference_t<T>*`. Le plus puissant est `NkDecay<T>` : il
applique d'un coup le traitement qu'un paramètre par valeur subirait — réf et cv retirés, tableau →
pointeur, fonction → pointeur de fonction. `NkNounType<T>` est sa version « légère » (réf + cv
seulement), idéale pour obtenir le **type nominal** d'un argument.

- **Conteneurs / ECS** : stocker des éléments par `NkDecay_t<T>` garantit qu'on range une valeur
  propre (pas une référence ni un `const`), quelle que soit la forme passée.
- **Forwarding générique** : `NkNounType_t<T>` sert à comparer ou indexer par type « brut » dans un
  variant, une *type map*, un système de composants.

### Catégories et propriétés — `NkIs*`

Les classifieurs (`NkIsVoid`, `NkIsPointer`, `NkIsArray`, `NkIsReference`, `NkIsEnum`, `NkIsClass`,
`NkIsUnion`, `NkIsFunction`) et les arithmétiques (`NkIsIntegral`, `NkIsFloatingPoint`,
`NkIsArithmetic`, `NkIsSigned`, `NkIsUnsigned`) répondent en `O(1)` compilateur. Deux pièges
documentés : `NkIsPointer_v` applique `NkRemoveCV` (la struct non) — pour un `T* const`, utilisez le
`_v` ; et `NkIsCharacterType`/`NkIsValidCharType` (caractères : `nk_char`, `nk_wchar`, `nk_char8/16/32`,
`signed/unsigned char`) sert à valider les types de chaînes.

- **Audio** : `NkIsFloatingPoint` sépare un *mix bus* en `float32` d'un buffer entier PCM ;
  `NkIsSigned` choisit la normalisation d'un échantillon.
- **Sérialisation / IO** : `NkIsEnum` route vers l'écriture de l'entier sous-jacent, `NkIsArithmetic`
  vers un write direct, `NkIsClass` vers une visite de champs.
- **Texte / 2D** : `NkIsCharacterType` valide le type d'élément d'un `NkString`/`NkStringView` avant
  de choisir le préfixe de littéral.

### Logique de traits — `NkConjunction`, `NkDisjunction`, `NkNegation`

`NkConjunction<...>` (ET, court-circuité) et `NkDisjunction<...>` (OU) composent des conditions ; une
conjonction vide est vraie, une disjonction vide est fausse. `NkNegation<B>` inverse. On les combine
dans des `NkEnableIf` complexes : « `T` est arithmétique **et** non `const` », « pointeur **ou**
référence ». `NkIsAnyOf` n'est qu'une `NkDisjunction` de `NkIsSame`.

- **ECS / rendu** : contraindre une surcharge à « trivialement copiable **et** standard-layout » pour
  autoriser un chemin `memcpy`/upload GPU.

### Forwarding, mouvement, échange — `NkForward`, `NkMove`, `NkSwap`, `NkDeclval`

`NkForward<T>(x)` préserve la catégorie de valeur (lvalue reste lvalue, rvalue reste rvalue) : c'est
le *perfect forwarding* des fabriques (`EmplaceBack`, constructeurs in-place). `NkMove(x)` requalifie
inconditionnellement en rvalue pour **transférer** un contenu — il déclenche les move-ctors/assign,
clé de la sémantique `O(1)` des conteneurs. `NkSwap(a, b)` échange par trois moves, avec `noexcept`
conditionné à la nature *move-noexcept* de `T`. `NkDeclval<T>()` (jamais défini) ne s'utilise que
sous `decltype` pour sonder une expression ; son alias-type est `NkDeclVal<T>` (casse différente).

- **Conteneurs** : `NkMove`/`NkSwap` sous-tendent tout réagencement de `NkVector`, tout transfert de
  nœud de liste, toute rotation de tampon.
- **Gameplay / animation** : déplacer un état lourd (squelette, snapshot) plutôt que le copier ;
  `NkSwap` pour le *double buffering* d'un état de simulation.
- **Threading** : `NkMove` transfère un job ou un *promise* vers une file sans copie.

### Traits de constructibilité et de copie — `NkIsTrivial*`, `NkIs*Constructible`

Cette famille décide des **optimisations mémoire**. `NkIsTriviallyCopyable<T>` est ici une
**approximation maison** (`true` seulement pour arithmétiques et pointeurs), pas l'intrinsèque STL —
à connaître si vous attendiez un POD agrégé comme `true`. `NkIsCopyConstructible`,
`NkIsMoveConstructible` (alias du précédent), `NkIsDefaultConstructible`, `NkIsAssignable<T, U>`
testent la constructibilité/affectation. `NkIsTrivial`, `NkIsStandardLayout`, `NkIsPOD` qualifient la
disposition mémoire. **Piège majeur** : `NkIsTriviallyDestructible<T>` et
`NkIsTriviallyConstructible_v<T>` sont des **variables** templates, pas des structs — pas de
`::value`, on écrit directement `NkIsTriviallyDestructible<MonType>`.

- **Conteneurs / mémoire** : choisir `memcpy` en masse vs construction/destruction élément par
  élément ; sauter le destructeur pour un type `NkIsTriviallyDestructible` lors d'un `Clear()`.
- **GPU / rendu** : n'uploader directement que les structures `NkIsStandardLayout` (layout C garanti,
  même offsets côté shader) ; refuser à la compilation un vertex non-POD.
- **Réseau / sérialisation** : `NkIsPOD` autorise un dump binaire brut ; sinon, sérialisation champ
  par champ.

### Traits NKCore élargis et plateforme

`NkIsFundamental` (arith \|\| void \|\| nullptr), `NkIsScalar` (+ enum/pointeur), `NkIsObject`
(ni référence, ni void, ni fonction), `NkIsCompound` (`!fundamental`) donnent des catégories larges
pour des branchements grossiers. `NkBitSize<T>` rend la taille en **bits** (`sizeof*8`), pratique pour
dimensionner des masques. `NkIsPlatformSupported<T>` vaut `false` pour `nk_int128`/`nk_uint128` quand
la plateforme ne les a pas — un garde à mettre avant d'instancier un type sur cible restreinte
(certains backends mobiles / consoles).

### `NkInvoke` — l'appel universel

`NkInvoke` choisit, par SFINAE, l'une de trois surcharges `constexpr` : **(1)** callable libre →
`f(args...)` (fonction, pointeur de fonction, lambda, foncteur) ; **(2)** pointeur de fonction membre
→ `(obj.*pmf)(args...)`, où `obj` peut être référence **ou** pointeur (normalisé en interne) ; **(3)**
pointeur de membre données → `obj.*pmd`, rendu par **référence** (donc lisible et modifiable). Le
`noexcept` de l'appelé est propagé, le type de retour est exact (`decltype(auto)`). Aucun contrôle
runtime : l'objet doit être non-null. `NkInvokeResult_t<F, Args...>` donne le type de retour, et
`NkIsInvocable_v<F, Args...>` dit si l'appel **compile** (validité purement syntaxique, `noexcept`
non testé ; échoue sur types incomplets).

- **Gameplay / événements** : un `NkDelegate` stocke n'importe quel invocable et le rejoue via
  `NkInvoke` — fonction libre, méthode liée à une instance, lambda capturante, tout passe.
- **ECS** : exécuter un système exprimé comme méthode (`&System::Update`) ou comme lambda avec la
  même boucle de dispatch.
- **Variant / UI** : le `Visit` d'un type-somme appelle l'alternative active par `NkInvoke` ; un
  callback UI (`onClick`) est invoqué uniformément.
- **Threading** : une file de jobs garde des invocables hétérogènes et les lance par `NkInvoke`,
  `NkInvokeResult_t` servant à typer la valeur de retour (*future*).

### `NkTypeUtils` — manipulation de bits, casts, littéraux

Côté **bits** : `NK_BIT(x)` produit `1ULL << x`, `NK_SET_BIT`/`NK_CLEAR_BIT`/`NK_TEST_BIT`/
`NK_TOGGLE_BIT` manipulent un drapeau, `NK_ALIGN_UP`/`NK_ALIGN_DOWN`/`NK_IS_ALIGNED` gèrent
l'alignement — l'outillage quotidien d'un allocateur, d'un masque de couches de rendu, d'un masque de
collision. `NK_ARRAY_SIZE(a)` compte les éléments d'un tableau **statique** (pas un pointeur).
`NK_OFFSET_OF`/`NK_CONTAINER_OF` calculent l'offset d'un membre et remontent du membre au conteneur
— le motif des listes intrusives.

Côté **casts** : `NK_STATIC_CAST`/`NK_REINTERPRET_CAST`/`NK_CONST_CAST`/`NK_C_CAST`, plus
`NK_DYNAMIC_CAST` qui dégrade vers `static_cast` quand le RTTI est absent (`NKENTSEU_HAS_RTTI`). Les
`NK_SAFE_CAST_TO_U32`/`_I16`/… **clampent** vers les bornes du type cible au lieu de déborder
silencieusement — précieux pour passer d'un index `nk_usize` à un `nk_uint16` de format GPU.
`NkToHandle`/`NkFromHandle` convertissent un pointeur en `core::NkHandle` opaque (et retour),
`NkSafeCast<To, From>` est un `static_cast` typé.

Côté **littéraux** : `NK_LITERAL(CharT, "txt")` choisit le bon préfixe (`u8`/`u`/`U`/`L`) selon le
type caractère, et les UDL de `nkentseu::literals` produisent des valeurs **typées** —
`42_u32`, `3.14_f32`, `1_b32` (booléen typé : `val != 0`), `65_c16` (les suffixes caractère prennent
un entier), `0_b` (`Byte`). Distinct d'un cast : le suffixe
fixe le type à la source, sans conversion implicite ambiguë.

Enfin la **validation compile-time** : `NK_STATIC_ASSERT(name, cond, msg)` est un `static_assert`
nommé, et le header en enchaîne une batterie qui verrouille tailles et signedness de tous les types
primitifs (`nk_int8..nk_int64`, `nk_uint*`, `nk_float32/64/80`, `nk_char*`, `nk_bool32`, `nk_ptr`,
`nk_usize`…) — si une cible casse une hypothèse de taille, le build échoue tout de suite.

---

### Exemple récapitulatif

```cpp
#include "NKCore/NkTraits.h"
#include "NKCore/NkInvoke.h"
#include "NKCore/NkTypeUtils.h"
using namespace nkentseu;
using namespace nkentseu::traits;
using namespace nkentseu::literals;

// 1) SFINAE : surcharge réservée aux types arithmétiques.
template <typename T, typename = NkEnableIf_t<NkIsArithmetic_v<T>>>
void Log(T v) { /* ... */ }

// 2) Choix de stockage à la compilation selon la taille.
using Index = NkConditional_t<(sizeof(void*) == 8), nk_uint64, nk_uint32>;

// 3) Optimiser une copie : memcpy si trivial, élément par élément sinon.
template <typename T>
void CopyAll(T* dst, const T* src, nk_usize n) {
    if constexpr (NkIsTriviallyCopyable_v<T>) { /* memcpy */ }
    else for (nk_usize i = 0; i < n; ++i) dst[i] = src[i];
}

// 4) Appel uniforme : fonction libre, méthode, lambda — même syntaxe.
struct Player { void Hit(int dmg) {} };
Player p;
NkInvoke(&Player::Hit, p, 10);                 // pointeur de méthode
NkInvoke([](int x) { return x * 2; }, 21);     // lambda
using R = NkInvokeResult_t<decltype(&Player::Hit), Player&, int>;
static_assert(NkIsInvocable_v<decltype(&Player::Hit), Player&, int>);

// 5) Bits et littéraux typés.
nk_uint32 layers = 0;
NK_SET_BIT(layers, 3);                          // active le bit 3
auto speed = 9.81_f32;                          // littéral typé float32
```

---

[← Atomiques](Atomics.md) · [Index NKCore](README.md) · [Récap NKCore](../NKCore.md) · [Types-vocabulaire →](Vocabulary-Types.md)
