# L'énumération

> Couche **Foundation** · NKCore · Donner à un `enum class` ce qui lui manque : un **type de
> stockage explicite**, une conversion **valeur → texte** générée par macros, le test de
> **drapeau**, et des conversions implicites vers l'entier et vers l'enum — le tout via
> `NkEnumeration`.

Un `enum class` ordinaire en C++ a deux petites faiblesses qui finissent toujours par coûter du
temps. D'abord, **son type sous-jacent reste implicite** : on ne maîtrise pas toujours s'il tient
sur un octet ou sur quatre, ce qui compte dès qu'on en range des milliers dans des structures
binaires ou des composants ECS. Ensuite, **il ne sait pas se transformer en texte** : afficher la
valeur d'un enum dans un log, un inspecteur ou un fichier de sauvegarde demande à chaque fois un
fastidieux `switch` écrit à la main — du *boilerplate* qu'on recopie, qu'on oublie de mettre à jour
quand on ajoute une valeur, et qui pue le bug.

`NkEnumeration` répond à ces deux points en **enveloppant** une valeur d'enum dans un entier de
votre choix, avec des conversions implicites, des comparaisons, un test de drapeau, et un
`ToString()` virtuel surchargeable — que vous n'écrivez généralement pas à la main, car des macros
le **génèrent** à partir de la liste des valeurs.

```cpp
template <typename EnumType, typename BaseType = int>
class NkEnumeration;
```

Le `BaseType` (par défaut `int`) fixe le type entier de stockage, exposé ensuite sous l'alias
`BASE_TYPE`. C'est utile quand on veut un enum **compact** (stocké sur `uint8_t` plutôt que sur un
`int` de 32 bits, pour des structures binaires nombreuses) — ou au contraire un type assez large
pour héberger un masque de **drapeaux**.

- **Namespace** : `nkentseu`
- **Header** : `#include "NKCore/NkEnumeration.h"`

Une **attention immédiate**, à garder en tête tout au long de cette page : `NkEnumeration` est **à
la fois** le nom d'une **classe template** (ci-dessus) **et** celui d'une **macro fonction**
variadique (plus bas). Ce sont deux entités distinctes portant le même nom exact. Le préprocesseur
ne remplace que les usages avec **parenthèses et arguments** correspondant à la signature de la
macro ; un usage template `NkEnumeration<Color, uint8_t>` ne déclenche pas la macro.

> **En résumé.** `NkEnumeration<EnumType, BaseType = int>` enveloppe un enum dans l'entier
> `BaseType` (alias `BASE_TYPE`), avec conversions implicites, comparaisons, `HasFlag`, et un
> `ToString()` virtuel généré par macros. Le **même nom** désigne aussi une macro de définition
> rapide : c'est la présence d'arguments fonctionnels qui les distingue.

---

## Encapsuler un enum : la classe template

On l'emploie en **héritant** d'elle, presque toujours en récupérant ses constructeurs avec un
`using`, puis en fournissant le `ToString()` :

```cpp
enum class Color { Red, Green, Blue };

class ColorEnum : public nkentseu::NkEnumeration<Color, uint8_t> {
public:
    using NkEnumeration::NkEnumeration;             // hérite des constructeurs
    std::string ToString() const override { /* switch ou macros NK_ENUM_TO_STRING_* */ }
};
```

La classe stocke un seul membre, `value` (de type `BASE_TYPE`, **protégé** — accessible aux dérivés
et aux macros). Trois constructeurs alimentent ce `value` : le constructeur **par défaut** (`value =
0`), un constructeur depuis une **valeur d'enum** (`EnumType`) et un constructeur depuis un **entier
brut** (`BASE_TYPE`). Les deux derniers ne sont **pas** `explicit` : on passe donc librement d'une
valeur d'enum — ou d'un entier, ce qui autorise des combinaisons de drapeaux non nommées dans l'enum
— au wrapper.

Dans l'autre sens, deux **opérateurs de conversion implicites** ramènent le wrapper vers `BASE_TYPE`
(usage en expression arithmétique ou en comparaison ordonnée) et vers `EnumType` (récupérer la
valeur typée). Le header avertit que cette conversion vers l'enum peut **contourner la sécurité** du
wrapper — c'est pratique mais à manier avec discernement.

> **En résumé.** On dérive de `NkEnumeration<EnumType, BaseType>`, on hérite ses constructeurs via
> `using`, on surcharge `ToString()`. La valeur vit dans `value` (protégé) ; les conversions vers
> `BASE_TYPE` et `EnumType` sont **implicites** (et celle vers l'enum peut court-circuiter la
> sécurité de type).

---

## Tester un drapeau et comparer

Quand l'enum représente un **masque de bits**, `HasFlag(e)` teste la présence d'un bit en `O(1)`
(`(value & static_cast<BASE_TYPE>(e)) != 0`) — c'est tout ce dont on a besoin pour « cette surface
a-t-elle le flag *Transparent* ? », « cet input combine-t-il *Ctrl* et *Shift* ? ».

Côté comparaison, la classe template définit `operator==` et `operator!=`, chacun en **deux
surcharges** : contre un autre `NkEnumeration` et contre une `EnumType` nue. Voilà pourquoi
`monEnum == Color::Red` compile directement.

Un point qui surprend : **aucun** opérateur relationnel (`<`, `<=`, `>`, `>=`) n'est défini. Quand
on lit dans le code `level >= LogLevel::Warning`, **ce n'est pas** un opérateur membre qui agit :
c'est la **conversion implicite vers `BASE_TYPE`** qui ramène les deux côtés à des entiers, lesquels
se comparent nativement. C'est voulu, mais il faut le savoir pour ne pas chercher un opérateur qui
n'existe pas.

Dernier détail d'affichage : un `friend std::ostream& operator<<` libre redirige vers `ToString()`,
donc `std::cout << monEnum` imprime le texte de l'enum.

> **En résumé.** `HasFlag(e)` teste un bit en `O(1)` (n'a de sens que sur un enum bitmask).
> `==`/`!=` existent contre un wrapper **et** contre l'enum. Les comparaisons **ordonnées**
> (`<`, `>=`…) passent par la conversion implicite en `BASE_TYPE`, pas par un opérateur dédié.
> `operator<<` affiche via `ToString()`.

---

## Le piège des opérateurs bitwise

On voudrait écrire `flags = Color::Red | Color::Blue`. Avec la **classe template nue, ça ne compile
pas** : les opérateurs bitwise (`operator|`, `operator&`, `operator|=`, `operator&=`, contre
`EnumType` comme contre un autre `NkEnumeration`) sont **présents mais commentés** dans le header.
Pour les obtenir, deux voies : les **redéclarer** dans la classe dérivée, ou utiliser la **macro
`NkEnumeration(...)`** (section suivante) qui, elle, les fournit **activés**.

> **En résumé.** Les opérateurs `|`, `&`, `|=`, `&=` sont **commentés** dans la classe template :
> un `Color::Red | Color::Blue` direct échoue. Redéclarez-les dans la dérivée, ou passez par la
> macro `NkEnumeration(...)` qui les active.

---

## Générer `ToString()` sans boilerplate

Écrire à la main la correspondance valeur → chaîne est précisément ce qu'on veut éviter. Le groupe de
macros `NK_ENUM_TO_STRING_*` **produit le corps** d'un `ToString()`, à insérer entre un `BEGIN` et un
`END`. Deux modes : **valeur unique** (l'enum vaut **une** des valeurs, on **remplace** la chaîne) ou
**flags** (l'enum est un masque, on **concatène** les noms des bits présents).

```cpp
std::string ToString() const override {
    NK_ENUM_TO_STRING_BEGIN
        NK_ENUM_TO_STRING_SET_CONTENT(Color::Red)     // mode valeur unique : test ==, remplace
        NK_ENUM_TO_STRING_SET_CONTENT(Color::Green)
        NK_ENUM_TO_STRING_SET_CONTENT(Color::Blue)
    NK_ENUM_TO_STRING_END(Unknown)                    // fallback si rien ne matche : passe un identifiant nu
}
```

`NK_ENUM_TO_STRING_BEGIN` ouvre la méthode (avec `override`) et déclare une `std::string str`.
`NK_ENUM_TO_STRING_END(not_value)` la referme en renvoyant `str`, ou — si elle est restée vide — le
texte de `not_value` **stringifié** (`#not_value`) : passez donc un **identifiant nu** (`Unknown`),
pas une chaîne entre guillemets.

Entre les deux, on choisit selon le mode et selon qu'on veut un **nom complet** ou un **nom court** :
les variantes simples stringifient le token tel que vous l'écrivez (`Color::Red` →
`"Color::Red"`), tandis que les variantes suffixées `2` préfixent par un alias `Enum` interne (donc
vous passez le **nom court**, `Red`, et la chaîne produite est `"Red"`). Ces dernières exigent que la
classe expose un alias `Enum` — ce que fait justement la macro `NkEnumeration(...)`.

> **En résumé.** Entourez le corps de `ToString()` par `NK_ENUM_TO_STRING_BEGIN` …
> `NK_ENUM_TO_STRING_END(Fallback)`. À l'intérieur, `SET_CONTENT` (valeur unique, test `==`,
> remplace) ou `ADD_CONTENT` (flags, test `&`, **concatène sans séparateur**). Les variantes `…2`
> attendent un nom **court** (via un alias `Enum`). Le fallback de `END` est **stringifié** : passez
> un identifiant, pas `"..."`.

---

## Tout définir en une ligne : la macro `NkEnumeration(...)`

Quand on n'a pas besoin de la souplesse de l'héritage, la **macro** `NkEnumeration(...)` déclare
l'enum **et** son wrapper d'un seul geste :

```cpp
NkEnumeration(Status, uint8_t,
    NK_ENUM_TO_STRING_BEGIN
        NK_ENUM_TO_STRING_SET_CONTENT2(Idle)
        NK_ENUM_TO_STRING_SET_CONTENT2(Running)
    NK_ENUM_TO_STRING_END(Unknown),
    ,                                   // <-- 'methods' vide : on laisse la virgule seule
    Idle = 0, Running = 1
);
```

Ses cinq paramètres : `enum_name` (le nom de la classe wrapper), `default_type` (le type de
stockage), `tostring` (le corps de `ToString()`, souvent un bloc `NK_ENUM_TO_STRING_*`, ou rien),
`methods` (des méthodes supplémentaires injectées, ou rien — d'où la **virgule seule** ci-dessus), et
le `...` variadique qui énumère les valeurs (`Name = N, ...`). La macro génère alors l'enum
`Enum##enum_name` (ici `EnumStatus`) et la classe `enum_name`, toutes deux marquées `NKENTSEU_API`
pour l'export DLL.

La grande différence avec la classe template tient en deux mots : la classe produite par la macro est
**autonome** — elle ne dérive pas du template — et ses **opérateurs bitwise sont activés** (contre
`Enum`, contre elle-même, et contre `BASE_TYPE`). Elle expose l'alias `Enum` (requis par les
variantes `…2`), les constructeurs, l'affectation, les comparaisons (contre `enum_name`, contre
`Enum`, contre `BASE_TYPE`), les conversions implicites et `HasFlag`.

> **En résumé.** `NkEnumeration(nom, type, tostring, methods, valeurs…)` génère en une déclaration
> l'enum `Enum##nom` et la classe `nom` (autonome, **pas** dérivée du template), avec les
> **opérateurs bitwise activés** et l'alias `Enum`. Pensez à la **virgule vide** pour `methods` quand
> vous n'en avez pas.

---

## FromString : l'option expérimentale (déconseillée)

Le header propose un groupe `NK_STRING_TO_ENUM_*` pour générer un `static FromString(...)`, mais le
**marque expérimental / déprécié** — et recommande explicitement d'écrire un `FromString` **manuel**
(table de correspondance) à la place. Les macros existent (`NK_STRING_TO_ENUM_BEGIN(enum_name)`,
`…_ADD_CONTENT`, `…_SET_CONTENT`, `…_END(fallback)`) mais portent deux pièges documentés : le mode
`ADD` matche par **sous-chaîne** (`find`), donc `"Read"` reconnaît à tort `"ReadOnly"` ; et le
fallback de `END` ne s'applique **que si la chaîne est vide**, pas en l'absence de correspondance.

> **En résumé.** `NK_STRING_TO_ENUM_*` est **déprécié** : préférez un `FromString` manuel à base de
> table de lookup. Si vous y tenez, utilisez `SET_CONTENT` (comparaison exacte) et non `ADD_CONTENT`
> (sous-chaîne), et sachez que le fallback ne couvre que la chaîne vide.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Le détail (comportement, pièges, usages) est dans la
« Référence complète ».

### Classe template `NkEnumeration<EnumType, BaseType = int>`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `using BASE_TYPE = BaseType` | Alias du type de stockage. |
| Donnée | `value` (`protected`) | Valeur brute, accessible aux dérivés et aux macros. |
| Construction | `NkEnumeration()` | Par défaut, `value = 0`. |
| Construction | `NkEnumeration(EnumType e)` | Depuis une valeur d'enum (implicite). |
| Construction | `NkEnumeration(BASE_TYPE v)` | Depuis un entier brut (implicite ; flags non nommés). |
| Conversion | `operator BASE_TYPE()` | Vers l'entier (arithmétique, comparaison ordonnée). |
| Conversion | `operator EnumType()` | Vers l'enum (peut contourner la sécurité). |
| Flags | `HasFlag(EnumType e)` | Test de bit `[O(1)]`. |
| Texte | `virtual ToString()` | `std::string`, vide par défaut, surchargeable. |
| Comparaison | `operator==` / `operator!=` (vs `NkEnumeration`) | Compare `value`. |
| Comparaison | `operator==` / `operator!=` (vs `EnumType`) | Compare à une valeur d'enum. |
| Flux | `friend operator<<` | Affiche via `ToString()`. |
| (commenté) | `operator\|`, `operator&`, `operator\|=`, `operator&=` | **Désactivés** dans le template (à redéclarer). |

### Macros `ToString` — groupe `NK_ENUM_TO_STRING_*`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Ouverture | `NK_ENUM_TO_STRING_BEGIN` | Ouvre le `ToString()` (avec `override`), déclare `str`. |
| Flags | `NK_ENUM_TO_STRING_ADD_CONTENT(value_e)` | Si bit présent (`&`), **concatène** le nom complet. |
| Valeur | `NK_ENUM_TO_STRING_SET_CONTENT(value_e)` | Si égal (`==`), **remplace** par le nom complet. |
| Flags (court) | `NK_ENUM_TO_STRING_ADD_CONTENT2(value_e)` | Idem `ADD`, mais nom **court** via l'alias `Enum`. |
| Valeur (court) | `NK_ENUM_TO_STRING_SET_CONTENT2(value_e)` | Idem `SET`, mais nom **court** via l'alias `Enum`. |
| Fermeture | `NK_ENUM_TO_STRING_END(not_value)` | Renvoie `str` ou le fallback **stringifié**. |

### Macros `FromString` (expérimental / déprécié) — groupe `NK_STRING_TO_ENUM_*`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Ouverture | `NK_STRING_TO_ENUM_BEGIN(enum_name)` | Ouvre un `static enum_name FromString(...)`. |
| Flags | `NK_STRING_TO_ENUM_ADD_CONTENT(value_e)` | `find` (sous-chaîne) → `value \|=` … (faux positifs). |
| Valeur | `NK_STRING_TO_ENUM_SET_CONTENT(value_e)` | Égalité exacte → `value =` … (recommandé). |
| Fermeture | `NK_STRING_TO_ENUM_END(fallback_value)` | Fallback **seulement si chaîne vide**, puis `return`. |

### Macro de définition

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Définition | `NkEnumeration(enum_name, default_type, tostring, methods, …)` | Génère `Enum##enum_name` + classe `enum_name` autonome, bitwise **activés**, alias `Enum`. |
| Export | `NKENTSEU_API` | Appliqué à l'enum et à la classe générées (DLL). |

---

## Référence complète

### `value`, `BASE_TYPE` et les constructeurs

`value` est l'unique membre, de type `BASE_TYPE`, déclaré **protégé** : les classes dérivées y
accèdent, et c'est aussi ce que manipulent les macros `NK_ENUM_TO_STRING_*` / `NK_STRING_TO_ENUM_*`
(qui écrivent directement `value` ou `e.value`). L'alias `BASE_TYPE` republie le type de stockage,
pratique pour écrire du code générique au-dessus du wrapper.

Les trois constructeurs sont triviaux mais structurants. Le **constructeur par défaut** met `value` à
0 — c'est la valeur de départ d'un masque de drapeaux vide, ou d'un état initial. Le constructeur
depuis `EnumType`, **non `explicit`**, autorise le passage transparent `Color::Red → ColorEnum`. Le
constructeur depuis `BASE_TYPE`, lui aussi implicite, sert à **reconstruire une combinaison** que
l'enum ne nomme pas (un `0b0110` issu d'un `|`), ou à relire une valeur depuis un flux binaire.

- **IO / sérialisation** : relire un octet d'un `.nkb` et le ré-emballer en wrapper via le
  constructeur `BASE_TYPE`, sans `static_cast` éparpillés.
- **GPU / rendu** : un état de pipeline (mode de *blend*, masque d'écriture) stocké compact en
  `uint8_t`, reconstruit depuis l'entier lu.

### `operator BASE_TYPE()` et `operator EnumType()`

Ces deux conversions implicites sont le cœur de l'ergonomie — et de ses pièges. `operator
BASE_TYPE()` fait fondre le wrapper en entier dès qu'un contexte arithmétique ou de comparaison
l'attend : c'est **lui** qui rend possible `if (level >= LogLevel::Warning)` alors qu'aucun
`operator>=` n'est défini. `operator EnumType()` redonne la valeur typée pour un `switch` ou une API
qui veut l'`enum class`.

Le revers, signalé par le header : avoir **deux** conversions implicites peut créer des
**ambiguïtés** de surcharge et **contourner la sécurité de type** du wrapper. En pratique :

- **Gameplay / IA** : comparer un niveau d'alerte (`Alert::Combat`) par seuil ordonné fonctionne
  grâce à la conversion entière — pratique, mais documentez-le car l'ordre dépend des valeurs
  numériques de l'enum.
- **Threading** : un état de tâche encodé en enum se range tel quel dans un entier atomique via la
  conversion `BASE_TYPE`.
- À surveiller : un appel surchargé `f(int)` / `f(EnumType)` peut devenir **ambigu** face à un
  wrapper ; dans ce cas, convertissez explicitement.

### `HasFlag`

`HasFlag(e)` calcule `(value & static_cast<BASE_TYPE>(e)) != 0` : un test de bit en temps **constant**.
Il n'a de sens que si l'enum est un **masque** (valeurs en puissances de deux). C'est l'outil de
prédilection partout où un objet porte plusieurs propriétés simultanées :

- **Rendu / matériaux** : `material.HasFlag(MaterialFlag::Transparent)`, `HasFlag(Cull::Back)`.
- **ECS** : un masque de composants — `entity.mask.HasFlag(Component::Velocity)` — pour filtrer les
  entités traitées par un système.
- **Physique / collision** : couches et masques de collision (`layers.HasFlag(Layer::Player)`).
- **UI / 2D** : flags d'un widget (`Widget::Focusable | Widget::Visible`), états d'un bouton.
- **Audio** : drapeaux d'une voix (`Voice::Looping`, `Voice::Spatialized`).

Rappel : pour **composer** ces masques (`A | B`), il faut les opérateurs bitwise — donc soit la
classe générée par la macro, soit une dérivée qui les redéclare.

### `ToString` et les macros `NK_ENUM_TO_STRING_*`

`ToString()` est **virtuel** dans la classe template (le wrapper a donc une vtable) et renvoie `""`
par défaut. On le surcharge, à la main ou — mieux — via les macros. Le choix `SET` vs `ADD` reflète
la **nature** de l'enum :

- `SET_CONTENT` / `SET_CONTENT2` — **valeur unique** : test `==`, on **remplace** la chaîne. Pour les
  états (`Idle`, `Running`), les modes (`Fill`, `Wireframe`), les codes (`LogLevel`).
- `ADD_CONTENT` / `ADD_CONTENT2` — **flags** : test `&`, on **concatène** chaque bit présent. La
  chaîne résultante n'a **aucun séparateur** (`"ReadWriteExec"`) ; à vous d'en ajouter un si besoin.
- Les variantes **`2`** produisent des noms **courts** (`"Read"`) en s'appuyant sur l'alias `Enum`
  interne ; les variantes sans `2` stringifient le token complet (`"Permission::Read"`).
- `NK_ENUM_TO_STRING_END(x)` **stringifie** `x` : passez un identifiant nu (`Unknown`), jamais
  `"Unknown"`.

Domaines où le `ToString()` rend service — toujours côté **outils**, pas boucle chaude :

- **Logs / débogage** : tracer une transition d'état FSM, un niveau de sévérité, un mode de rendu
  courant.
- **Éditeur / UI** : peupler un menu déroulant d'inspecteur avec les noms lisibles d'un enum.
- **IO** : écrire un enum en clair dans un fichier de config texte (au lieu d'un nombre opaque).

Honnêteté STL : `ToString()` renvoie un `std::string` et le header tire `<sstream>` — c'est l'une des
rares surfaces où NKCore **n'est pas zéro-STL**. Réservez-le au debug et aux outils ; sur un chemin
critique, évitez de l'appeler.

### Comparaisons et `operator<<`

`operator==` / `operator!=` existent en deux saveurs (vs un autre `NkEnumeration`, vs une `EnumType`),
ce qui couvre `a == b` et `a == Color::Red`. **Aucun** opérateur d'ordre n'est membre : `<`, `<=`,
`>`, `>=` reposent sur la conversion en `BASE_TYPE`. Le `friend operator<<` redirige vers
`ToString()`, ce qui donne un affichage lisible « gratuit » dès qu'on imprime un wrapper dans un flux.

- **Gameplay** : `if (phase == Phase::Boss)`, comparaison directe à l'enum.
- **Logs** : `std::cout << currentState;` imprime le nom plutôt que l'entier.

### Les opérateurs bitwise (template : désactivés)

Dans la classe template, `operator|`, `operator&`, `operator|=`, `operator&=` (contre `EnumType` et
contre un autre `NkEnumeration`) sont **commentés**, donc indisponibles : `Color::Red | Color::Blue`
**ne compile pas** sur le template nu. Deux remèdes : les **redéclarer** dans la dérivée (idiome du
header pour un type de flags), ou passer par la **macro** `NkEnumeration(...)` qui les fournit
**activés**. C'est le seul vrai accroc ergonomique de la classe template, et il revient sans cesse
dès qu'on manipule des masques (rendu, ECS, collision, UI).

### La macro `NkEnumeration(...)`

La macro condense enum + wrapper en une déclaration. Elle génère :

1. `enum class NKENTSEU_API Enum##enum_name : default_type { … };` — l'enum nommée `Enum<nom>`.
2. `class NKENTSEU_API enum_name { … };` — le wrapper **autonome**, qui **ne dérive pas** du template.

Le wrapper généré expose : `value` (protégé), `BASE_TYPE`, l'alias `Enum = Enum##enum_name` (celui
qu'attendent les variantes `…2`), les constructeurs `noexcept` (défaut, depuis `Enum`, depuis
`default_type`, copie), le bloc `tostring` puis le bloc `methods` injectés tels quels, les
comparaisons `==`/`!=` (contre `enum_name`, `Enum`, `BASE_TYPE`), l'affectation, les **opérateurs
bitwise activés** (`|`, `&`, `|=`, `&=` contre `Enum`, contre `enum_name`, contre `BASE_TYPE`), les
conversions implicites (`BASE_TYPE`, `Enum`) et `HasFlag(Enum e)`.

Différences clés à mémoriser :

- **Bitwise d'office** — c'est l'argument décisif pour préférer la macro dès qu'on fait des flags.
- **Pas de `friend operator<<`**, pas d'héritage du template : la classe est isolée.
- **Vigilance** : la classe macro n'a **pas de base**, alors que `NK_ENUM_TO_STRING_BEGIN` injecte un
  `override`. C'est techniquement incohérent (un `override` sans base à surcharger) ; les exemples du
  header l'emploient malgré tout ainsi — point à vérifier au build selon votre compilateur.
- `methods` vide se note par une **virgule seule** ; `tostring` vide est admis aussi.

Usages : tout enum « jetable » d'un sous-système (modes d'un blend GPU, types de message réseau,
canaux audio, états d'un widget) qu'on veut **compact, comparable, imprimable et combinable** sans
écrire de classe à la main.

### Les macros `NK_STRING_TO_ENUM_*` (déprécié)

Elles génèrent un `static FromString` mais sont **marquées expérimentales** ; le header recommande un
`FromString` **manuel** par table de lookup. `BEGIN(enum_name)` ouvre la fonction, `ADD_CONTENT`
utilise `find` (**sous-chaîne** → faux positifs : `"Read"` matche `"ReadOnly"`), `SET_CONTENT` fait
une **égalité exacte** (préférable), `END(fallback)` ne pose le fallback **que si la chaîne est
vide**. Elles écrivent `e.value` directement, d'où leur usage depuis une méthode statique de la classe
elle-même. À éviter pour du parsing fiable (config, IO, réseau) : écrivez la table à la main.

---

### Exemple récapitulatif

```cpp
#include "NKCore/NkEnumeration.h"
using namespace nkentseu;

// 1) Héritage du template : un enum d'état, ToString() généré.
enum class Status { Idle, Running, Done };

class StatusEnum : public NkEnumeration<Status, uint8_t> {
public:
    using NkEnumeration::NkEnumeration;
    std::string ToString() const override {
        NK_ENUM_TO_STRING_BEGIN
            NK_ENUM_TO_STRING_SET_CONTENT(Status::Idle)
            NK_ENUM_TO_STRING_SET_CONTENT(Status::Running)
            NK_ENUM_TO_STRING_SET_CONTENT(Status::Done)
        NK_ENUM_TO_STRING_END(Unknown)
    }
};

StatusEnum s = Status::Running;
std::cout << s;                 // "Status::Running" (via operator<< -> ToString)
if (s == Status::Running) { /* comparaison directe à l'enum */ }

// 2) La MACRO : enum compact + wrapper autonome, bitwise ACTIVÉS d'office.
NkEnumeration(RenderFlag, uint32_t,
    NK_ENUM_TO_STRING_BEGIN
        NK_ENUM_TO_STRING_ADD_CONTENT2(Transparent)
        NK_ENUM_TO_STRING_ADD_CONTENT2(CastShadow)
    NK_ENUM_TO_STRING_END(None),
    ,                                            // methods vide : virgule seule
    Transparent = 1, CastShadow = 2, TwoSided = 4
);

RenderFlag flags = RenderFlag(RenderFlag::Transparent | RenderFlag::CastShadow);  // | activé par la macro
if (flags.HasFlag(RenderFlag::CastShadow)) { /* ... */ }
std::cout << flags.ToString();  // "TransparentCastShadow" (flags concaténés, sans séparateur)
```

---

[← Bits & limites](Bits-Limits.md) · [Index NKCore](README.md) · [Récap NKCore](../NKCore.md) · [Plateforme & configuration →](Platform-Config.md)
