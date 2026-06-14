# Les types réflexifs

> Couche **System** · NKReflection · Décrire un type à l'**exécution** : ses
> métadonnées (`NkType`), celles d'une classe (`NkClass`), et les **registres** qui les
> retrouvent par leur nom (`NkRegistry`, `NkTypeRegistry`).

Le compilateur connaît tout des types — leur taille, leurs champs, leurs méthodes — puis
**oublie tout** une fois le binaire produit. Or un moteur a sans cesse besoin de
re-questionner les types *pendant qu'il tourne* : un inspecteur d'éditeur qui veut lister les
champs d'un composant pour les afficher, un sérialiseur qui doit savoir quoi écrire dans un
`.nkscene`, un système ECS qui crée une entité à partir du **nom** d'une classe lu dans un
fichier. C'est exactement le trou que comble la **réflexion** : conserver, à côté du code, une
**description manipulable du type** que l'on peut interroger à l'exécution.

NKReflection est volontairement **minimaliste et zéro-allocation** : pas de conteneur STL, pas
de `new` pour les métadonnées. Tout est rangé dans des **tableaux statiques de capacité fixe**
(512 types, 512 classes, 64 propriétés/méthodes par classe), et les objets de métadonnées
eux-mêmes vivent comme **static locals** — créés une fois, jamais détruits, jamais déplacés.
Cette page couvre la famille **Types** : la brique `NkType`, la classe `NkClass`, et les deux
registres qui les indexent.

- **Namespace** : `nkentseu::reflection` (sauf `NkFunction`, qui est `nkentseu::NkFunction`)
- **Headers** (à inclure individuellement, pas de parapluie) :
  `#include "NKReflection/NkType.h"`, `#include "NKReflection/NkClass.h"`,
  `#include "NKReflection/NkRegistry.h"`

---

## La brique de base : `NkType`

Tout part de `NkType`. C'est l'objet de métadonnées le plus simple du module : il décrit **un**
type par son **nom**, sa **taille**, son **alignement** et sa **catégorie** (un `enum` qui dit
« primitif entier », « pointeur », « classe »…). Rien de plus — un `NkType` ne connaît ni les
champs ni les méthodes ; pour ça il faut un `NkClass`, qu'on peut justement **rattacher** à un
`NkType` (`SetClass`).

On ne construit presque **jamais** un `NkType` à la main. La fabrique canonique est la fonction
libre `NkTypeOf<T>()`, qui renvoie une référence vers un `static NkType` **unique par `T`** :

```cpp
const NkType& t = NkTypeOf<NkVec3>();
t.GetName();        // typeid(NkVec3).name() — manglé, mais stable
t.GetSize();        // sizeof(NkVec3)
t.GetAlignment();   // alignof(NkVec3)
t.GetCategory();    // NK_CLASS (catégorie par défaut des types non-primitifs)
```

Cette unicité est le point crucial : deux appels `NkTypeOf<NkVec3>()` renvoient **la même
instance** en mémoire. C'est ce qui rend `operator==` fiable — car il compare les **pointeurs**
du nom (`mName`), pas leur contenu caractère par caractère. Ce n'est donc **pas** une comparaison
de chaînes : deux `NkType` que vous construiriez vous-même avec deux littéraux distincts mais
identiques (`"int"` et un autre `"int"`) pourraient comparer *différents*. Passez toujours par
`NkTypeOf<T>()` et l'égalité « marche » comme attendu.

La catégorie est calculée par `DetermineCategory<T>()` (`constexpr`), qui reconnaît les types
primitifs (`bool`, les entiers, les flottants), les pointeurs, les références et les tableaux —
et range **tout le reste** dans `NK_CLASS`. C'est un détail à connaître : `DetermineCategory`
ne produit **jamais** `NK_ENUM`, `NK_STRUCT`, `NK_STRING`, `NK_VECTOR` ni `NK_UNKNOWN`, même si
ces valeurs existent dans l'énumération. Une `enum` réfléchie sera donc étiquetée `NK_CLASS`
sauf si vous fixez sa catégorie autrement.

> **En résumé.** `NkType` = nom + taille + alignement + catégorie, métadonnée immuable d'un
> type. Fabriquez-le **toujours** via `NkTypeOf<T>()` (instance unique par `T`, identité stable).
> `operator==` compare les **pointeurs** de nom — fiable uniquement grâce à cette unicité.
> `DetermineCategory` classe les non-primitifs en `NK_CLASS`, jamais en `NK_ENUM`/`NK_UNKNOWN`.

---

## La classe réfléchie : `NkClass`

Un `NkType` dit *quel* type ; un `NkClass` dit *de quoi il est fait*. C'est l'objet riche du
module : il porte le nom et la taille de la classe, son `NkType` associé, sa **classe de base**
(héritage simple), une collection de **propriétés** (les champs), une collection de
**méthodes**, et deux **callbacks** ctor/dtor pour fabriquer et détruire des instances *par
réflexion*.

Les collections sont des **tableaux fixes de 64 entrées** chacun. Conséquence directe à garder
en tête : au-delà de 64 propriétés (ou 64 méthodes), `AddProperty`/`AddMethod` **abandonnent
silencieusement** l'ajout — aucune erreur, aucun débordement, l'élément est simplement perdu.
Il n'y a pas non plus de détection de doublon : ajouter deux fois le même champ le stocke deux
fois.

```cpp
NkClass meta = NkClass::MakeFromClassType<Player>("Player");
// ... on lui ajoute des propriétés/méthodes (voir plus bas) ...
meta.GetSize();                 // sizeof(Player)
meta.GetPropertyCount();        // champs déclarés sur Player (hors héritage)
meta.GetTotalPropertyCount();   // + ceux des classes de base
```

`GetProperty(name)` et `GetMethod(name)` cherchent **linéairement par `strcmp`** dans la classe,
puis **remontent récursivement** dans les bases — un champ hérité est donc trouvé. À l'inverse,
`GetPropertyAt(i)` / `GetMethodAt(i)` n'accèdent qu'à la classe **courante** (pas d'héritage).
Notez que `GetMethod` ne gère **pas la surcharge** : il renvoie la *première* méthode portant ce
nom. Ce n'est donc pas un résolveur d'overloads.

L'héritage se déclare avec `SetBaseClass`, et s'interroge avec `IsSubclassOf` (remonte la chaîne
des bases, **et s'inclut lui-même**) et `IsSuperclassOf` (l'inverse, faux si l'argument est
`nullptr`). C'est la base d'un test de type « est-ce un `Enemy` ou une de ses sous-classes ? » à
l'exécution.

Enfin, le **cycle de vie réflexif** : `SetConstructor`/`SetDestructor` enregistrent des callables
(`NkFunction`), et `CreateInstance()` / `DestroyInstance(ptr)` les appellent. Attention à la
politique mémoire : le `DestructorFn` est censé **libérer les ressources** de l'objet, **pas
forcément désallouer sa mémoire** — c'est laissé à l'appelant (ou au lambda lui-même).

> **En résumé.** `NkClass` = nom + taille + `NkType` + base + 64 propriétés + 64 méthodes +
> callbacks ctor/dtor. Les recherches par nom remontent l'héritage (`strcmp`, pas de surcharge) ;
> les accès indexés restent sur la classe courante. Capacités **fixes et silencieuses** (64),
> pas de dédoublonnage. `CreateInstance`/`DestroyInstance` pilotent la vie d'une instance par
> réflexion — la **mémoire** de l'objet reste de la responsabilité de l'appelant.

---

## Retrouver un type par son nom : les registres

Décrire un type ne sert à rien si on ne peut pas le **retrouver** quand on n'a en main qu'une
chaîne — typiquement le nom d'une classe lu dans un fichier de scène. C'est le rôle des
**registres** : des annuaires globaux (singletons) qui indexent les `NkType` et `NkClass` par
leur nom.

Le module en expose **deux**, et c'est une source de confusion à dissiper tout de suite :

- **`NkRegistry`** (dans `NkRegistry.h`) est le **registre central recommandé**. Il indexe **à
  la fois** les types **et** les classes, propose un callback d'enregistrement, et toutes ses
  méthodes sont **inline**. C'est lui que les macros de réflexion utilisent.
- **`NkTypeRegistry`** (dans `NkType.h`) est un registre **plus ancien et limité** aux `NkType`
  seuls, dont certaines méthodes (`Get`, `RegisterType`, `FindType`) sont seulement **déclarées**
  dans le header et **définies dans un `.cpp`**.

Dans les deux cas, on enregistre avec `RegisterType` / `RegisterClass` et on retrouve avec
`FindType` / `FindClass` (recherche linéaire `strcmp`, `O(n)`) :

```cpp
NkRegistry& reg = NkRegistry::Get();
const NkClass* c = reg.FindClass("Player");   // nullptr si jamais enregistrée
if (c) {
    void* obj = c->CreateInstance();          // fabrique une instance par son nom
}
```

Deux pièges à connaître, parce qu'ils trahissent un écart entre la documentation interne et le
code réel. **Premièrement**, malgré le `OnRegisterCallback`, les méthodes `RegisterType` et
`RegisterClass` **n'appellent pas** ce callback : il est stocké, jamais invoqué. **Deuxièmement**,
ces registres ne sont **pas thread-safe en écriture** : seule l'initialisation du singleton (à la
Meyer) l'est. Tout `RegisterType`/`RegisterClass`/`SetOnRegisterCallback` concurrent doit être
synchronisé par vos soins.

> **En résumé.** `NkRegistry` (types **+** classes, inline, recommandé) et `NkTypeRegistry`
> (types seuls, méthodes en `.cpp`) sont des **singletons** indexant par nom (`O(n)`,
> dédup par pointeur **ou** `strcmp`). Capacité **512**. Écriture **non thread-safe** ; le
> `OnRegisterCallback` est stocké mais **jamais déclenché**.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé dans la
« Référence complète » qui suit. Complexités/notes entre crochets.

### `NkTypeCategory` (enum) et `NkType`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Énumération | `NkTypeCategory` | Catégorie d'un type : `NK_BOOL`…`NK_FLOAT64` (primitifs consécutifs), `NK_nk_char`, `NK_STRING`, `NK_POINTER`, `NK_REFERENCE`, `NK_ARRAY`, `NK_VECTOR`, `NK_CLASS`, `NK_STRUCT`, `NK_ENUM`, `NK_UNION`, `NK_FUNCTION`, `NK_VOID`, `NK_UNKNOWN`. |
| Construction | `NkType(name, size, alignment, category)` | Construit une métadonnée (`mClass = nullptr`). |
| Construction | `~NkType()` (`virtual`) | Destructeur virtuel (héritage possible). |
| Lecture | `GetName` `GetSize` `GetAlignment` `GetCategory` `[O(1)]` | Accesseurs des champs. |
| Tests | `IsClass` `IsPointer` `IsReference` `IsArray` `IsEnum` `[O(1)]` | Compare la catégorie. |
| Tests | `IsPrimitive` `[O(1)]` | Vrai si `NK_BOOL ≤ cat ≤ NK_FLOAT64` (test de plage). |
| Classe liée | `GetClass` / `SetClass` | Lit / rattache un `NkClass`. |
| Comparaison | `operator==` `operator!=` | Égalité par adresse **ou** (pointeur `mName` + `mSize`). |

### Fonctions libres (`NkType.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Fabrique | `NkTypeOf<T>()` | **Instance unique** par `T` (la fabrique canonique). |
| Fabrique | `NkTypeOf(const T&)` | Surcharge instance (ignore l'argument, délègue). |
| Catégorisation | `DetermineCategory<T>()` `[constexpr]` | Déduit la catégorie ; défaut → `NK_CLASS`. |

### `NkTypeRegistry` (`NkType.h`, singleton types seuls)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Accès | `static Get()` | Singleton (défini hors header). |
| Écriture | `RegisterType(type)` `[O(n)]` | Enregistre (doublons ignorés ; **non thread-safe**). |
| Lecture | `FindType(name)` `[O(n)]` | Recherche par nom. |
| Lecture | `GetType<T>()` | `FindType(typeid(T).name())` (inline). |

### `NkClass` (`NkClass.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Types membres | `ConstructorFn` `DestructorFn` | Callables ctor (`void*()`) / dtor (`void(void*)`). |
| Construction | `NkClass(name, size, const NkType&)` | Construit (base `nullptr`, compteurs à 0). |
| Infos | `GetName` `GetSize` `GetType` `[O(1)]` | Identité de la classe. |
| Héritage | `GetBaseClass` / `SetBaseClass` | Lit / fixe le parent. |
| Héritage | `IsSubclassOf` `IsSuperclassOf` `[O(profondeur)]` | Tests de filiation. |
| Propriétés | `AddProperty` `[cap 64]` | Ajoute (ignore null ; **pas de dédup**). |
| Propriétés | `GetProperty(name)` `[O(n·prof.)]` | Recherche `strcmp` + héritage. |
| Propriétés | `GetPropertyAt(i)` `[O(1)]` | Indexé, classe courante. |
| Propriétés | `GetPropertyCount` `[O(1)]` / `GetTotalPropertyCount` `[O(prof.)]` | Direct / récursif avec bases. |
| Méthodes | `AddMethod` / `GetMethod(name)` / `GetMethodAt(i)` | Symétriques aux propriétés (pas de surcharge). |
| Méthodes | `GetMethodCount` / `GetTotalMethodCount` | Direct / récursif. |
| Cycle de vie | `SetConstructor` `SetDestructor` | Enregistre les callbacks. |
| Cycle de vie | `CreateInstance` / `DestroyInstance(ptr)` | Fabrique / détruit par réflexion. |
| Cycle de vie | `HasConstructor` `HasDestructor` | Callback présent ? |
| Fabriques statiques | `MakeFromClassType<T>(name)` | Construit un `NkClass` (**par valeur**). |
| Fabriques statiques | `RegisterMemberProperty<C,V>(meta, ptr, name, flags=0)` | Crée + ajoute une `NkProperty` (offset auto). |
| Fabriques statiques | `RegisterMemberMethod<C,R,Args...>(meta, ptr, name, flags=0)` | Crée + ajoute une `NkMethod` (**sans invokeur**). |

### `NkRegistry` (`NkRegistry.h`, singleton central types + classes)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Accès | `static Get()` | Singleton inline. |
| Types | `RegisterType(type)` `[O(n)]` | Enregistre (dédup pointeur/`strcmp`, cap 512). |
| Types | `FindType(name)` `[O(n)]` / `GetType<T>()` | Recherche ; `GetType<T>` **auto-crée** si absent. |
| Classes | `RegisterClass(info)` `[O(n)]` | Enregistre (cap 512). |
| Classes | `FindClass(name)` `[O(n)]` / `GetClass<T>()` | Recherche ; `GetClass<T>` renvoie `nullptr` si absent. |
| Comptes | `GetTypeCount` `GetClassCount` `[O(1)]` | Nombres enregistrés. |
| Accès indexé | `GetTypeAt(i)` `GetClassAt(i)` | `nullptr` hors borne (ordre non garanti). |
| Callback | `OnRegisterCallback` (type) | `NkFunction<void(const nk_char*, nk_bool)>`. |
| Callback | `SetOnRegisterCallback` / `GetOnRegisterCallback` | Stocke / lit (**jamais invoqué** par Register*). |

### Macros (`NkRegistry.h`, portée globale)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Déclaration | `NKENTSEU_REFLECT_CLASS(Name)` | Injecte `SelfType`, `GetStaticClass()`, `GetClass()` (**virtual**). |
| Déclaration | `NKENTSEU_REFLECT_PROPERTY(Name)` | Injecte `Get##Name##Property()` (requiert `SelfType`). |
| Déclaration | `NKENTSEU_PROPERTY(Type, Name)` | Membre + sa réflexion en une ligne. |
| Annotation | `NKENTSEU_REFLECT` | Attribut `[[nkentseu::reflect]]` (informatif). |
| Enregistrement | `NKENTSEU_REGISTER_CLASS(Name)` | Auto-enregistre la classe avant `main()` (dans **un** `.cpp`). |

---

## Référence complète

Chaque élément est repris ici en détail — comportement, complexité, et usages dans les domaines
du moteur. Les éléments triviaux sont décrits brièvement ; les pièges et les mécanismes
importants, à fond.

### `NkTypeCategory` — la classification des types

L'énumération range chaque type dans une **catégorie**, et son ordre n'est pas anodin : les
primitifs `NK_BOOL`(0) jusqu'à `NK_FLOAT64`(10) sont **consécutifs**, ce qui permet à
`IsPrimitive()` de tester d'un seul coup `NK_BOOL ≤ cat ≤ NK_FLOAT64` (`O(1)`, sans `switch`).
Viennent ensuite `NK_nk_char` (oui, orthographié littéralement ainsi, **pas** `NK_CHAR`),
`NK_STRING`, les indirections (`NK_POINTER`, `NK_REFERENCE`), les agrégats (`NK_ARRAY`,
`NK_VECTOR`, `NK_CLASS`, `NK_STRUCT`, `NK_ENUM`, `NK_UNION`), `NK_FUNCTION`, puis `NK_VOID` et
`NK_UNKNOWN`. À l'usage, c'est le discriminant qui pilote la sérialisation et l'affichage :

- **Outils / éditeur** — un inspecteur choisit le *widget* d'édition selon la catégorie : un
  champ texte pour `NK_STRING`, une case à cocher pour `NK_BOOL`, un sélecteur pour `NK_ENUM`,
  un sous-panneau pliable pour `NK_CLASS`.
- **IO / réseau** — un sérialiseur écrit un nombre brut pour un primitif, suit le pointeur pour
  `NK_POINTER`, descend récursivement dans une `NK_CLASS`.

À nuancer : la catégorie n'est fiable que si elle a été **renseignée correctement** à la
construction. Comme `DetermineCategory` (voir plus bas) rabat tout le non-primitif sur
`NK_CLASS`, les valeurs `NK_ENUM`/`NK_STRUCT`/`NK_UNION`/`NK_STRING`/`NK_VECTOR` existent surtout
pour un usage **manuel** ou un outillage futur — elles ne sortent pas automatiquement.

### `NkType` — la métadonnée d'un type

C'est un objet **léger et immuable** (en intention) : nom, taille, alignement, catégorie, plus un
pointeur optionnel vers un `NkClass`. Les accesseurs (`GetName`, `GetSize`, `GetAlignment`,
`GetCategory`) et les tests (`IsClass`, `IsPointer`, `IsReference`, `IsArray`, `IsEnum`,
`IsPrimitive`) sont tous `O(1)` et triviaux. Le destructeur est **virtuel**, ce qui autorise
d'éventuelles spécialisations dérivées.

Deux points méritent l'attention. D'abord `SetClass` : il **mute** l'objet (méthode non-`const`)
pour lui rattacher un `NkClass`, ce qui entame l'immuabilité annoncée — en pratique on l'appelle
une fois, au moment de relier le type à sa classe. Ensuite, et c'est **le** piège du module,
`operator==` :

- Il renvoie vrai si les deux objets sont à la **même adresse**, *ou* si leurs pointeurs `mName`
  sont **égaux** (comparaison de **pointeurs**, pas `strcmp`) **et** leurs tailles égales.
- C'est fiable **uniquement** parce que `NkTypeOf<T>()` partage une instance unique : le même
  `T` donne le même `mName`. Deux `NkType` bâtis à la main avec des littéraux distincts —
  même identiques — peuvent comparer **faux**.

Usages, par domaine :
- **ECS** — étiqueter une colonne de composants par son `NkType` et comparer rapidement « cette
  archétype contient-il ce composant ? » via l'identité de pointeur.
- **Sérialisation / IO** — écrire la taille et le nom du type dans un en-tête binaire `.nkb`,
  relire et vérifier la concordance au chargement.
- **GPU / rendu** — connaître `GetSize()`/`GetAlignment()` d'un type de sommet pour calculer un
  *stride* et un offset d'attribut sans coder la taille en dur.
- **Threading** — comparer des types par identité (pointeur) est sans verrou et sûr en lecture,
  pratique pour router un message typé vers le bon handler.

### `NkTypeOf<T>()` et la surcharge instance — la fabrique canonique

`NkTypeOf<T>()` est **l'entrée principale** du module. Elle renvoie une référence vers un
`static NkType` **local à la fonction**, donc :

- **une seule instance par `T`** pour toute la durée du programme,
- **initialisation thread-safe** (static local, garantie C++11+),
- construite comme `NkType(typeid(T).name(), sizeof(T), alignof(T), DetermineCategory<T>())`.

C'est sur elle que reposent `MakeFromClassType`, `RegisterMember*`, `GetType<T>` et les macros.
La surcharge `NkTypeOf(const T&)` **ignore** son argument et délègue à `NkTypeOf<T>()` — pratique
quand on a une valeur sous la main plutôt qu'un type explicite (`NkTypeOf(maVariable)`). À retenir :
parce que l'instance est partagée, c'est elle qui fait « marcher » `operator==` ; ne reconstruisez
jamais un `NkType` à la main si vous comptez le comparer.

### `DetermineCategory<T>()` — déduire la catégorie

Fonction `constexpr` qui cascade des traits `traits::NkIsSame<T, …>` : `void` → `NK_VOID`,
`nk_bool` → `NK_BOOL`, les entiers signés/non signés → leurs `NK_INT*`/`NK_UINT*`, les flottants
→ `NK_FLOAT32`/`NK_FLOAT64` ; puis `NkIsPointer` → `NK_POINTER`, `NkIsReference` → `NK_REFERENCE`,
`NkIsArray` → `NK_ARRAY` ; et **tout le reste** → `NK_CLASS`.

Le comportement important est ce **défaut `NK_CLASS`** : une `enum` réfléchie ressort `NK_CLASS`
(jamais `NK_ENUM`), une chaîne maison ressort `NK_CLASS` (jamais `NK_STRING`), et `NK_nk_char`
n'est **jamais** produit automatiquement. Si votre outillage a besoin de distinguer ces cas, il
faut fixer la catégorie autrement (construction manuelle de `NkType` ou correction après coup).
C'est typiquement le genre de subtilité qui mord un sérialiseur générique : croire qu'une `enum`
sera marquée `NK_ENUM` est une erreur.

### `NkTypeRegistry` — le registre de types historique

Singleton (à la Meyer) qui n'indexe que des `NkType`, dans un tableau statique de **512**
entrées. C'est l'ancêtre de `NkRegistry`, plus limité :

- `Get()`, `RegisterType(type)` (doublons ignorés, `O(n)`) et `FindType(name)` (`O(n)`) sont
  seulement **déclarés** dans le header — leurs corps vivent dans un `.cpp` du module. Si vous
  lisez le header en cherchant l'implémentation, vous ne la trouverez pas : c'est normal.
- `GetType<T>()` est, lui, **inline** : c'est un raccourci `FindType(typeid(T).name())`.
- Il est **non thread-safe** en écriture (seule l'init du singleton l'est).

En pratique, préférez `NkRegistry` (ci-dessous) qui fait tout ce que fait `NkTypeRegistry` *et*
gère les classes. `NkTypeRegistry` reste utile si vous n'avez besoin d'indexer **que** des types
bruts (par exemple un cache de types de sommets côté **rendu**, ou un annuaire de types de
messages côté **réseau**), mais la duplication des deux registres est surtout une dette à
connaître pour ne pas chercher un type au mauvais endroit.

### `NkClass` — identité, taille, type associé

Les accesseurs `GetName`/`GetSize` sont `O(1)` ; `GetType()` **déréférence** le `NkType*` associé
et renvoie une référence. Le constructeur `NkClass(name, size, const NkType&)` initialise base à
`nullptr` et compteurs à zéro. Le `name` passé est le nom **lisible** (ex. `"Player"`) — on le
fournit explicitement parce que `typeid(T).name()` serait manglé et illisible dans un éditeur ou
un fichier.

### `NkClass` — l'héritage simple

`SetBaseClass(base)` établit le parent (héritage **simple** uniquement). Les deux tests forment
la base de tout `is-a` à l'exécution :

- `IsSubclassOf(other)` remonte la chaîne `mBaseClass` — **en s'incluant lui-même** — jusqu'à
  trouver `other` ou atteindre `nullptr`. Coût `O(profondeur)`.
- `IsSuperclassOf(other)` est l'inverse : `other && other->IsSubclassOf(this)`. Il est **faux**
  si `other == nullptr`.

Usages, par domaine :
- **Gameplay / IA** — « cette entité est-elle un `Enemy` ou une sous-classe ? » pour décider d'un
  comportement, d'un filtre de ciblage, d'un effet de zone.
- **Outils / éditeur** — peupler une liste déroulante « créer un objet de type… » en ne montrant
  que les sous-classes d'une base donnée.
- **Sérialisation** — choisir le bon désérialiseur en remontant la hiérarchie jusqu'à un type
  connu.

### `NkClass` — propriétés et méthodes

Les propriétés (`NkProperty`, défini ailleurs) et méthodes (`NkMethod`) sont stockées dans deux
tableaux fixes de **64** entrées. Les opérations se répondent en miroir :

- `AddProperty`/`AddMethod` ignorent un pointeur `nullptr`, ajoutent **si** le compteur est sous
  64 (sinon **perte silencieuse**), et **ne détectent pas les doublons**.
- `GetProperty(name)`/`GetMethod(name)` cherchent linéairement par `strcmp` dans la classe **puis
  récursivement dans les bases** — un membre hérité est donc retrouvé. Coût `O(n × profondeur)`.
  `GetMethod` renvoie la **première** correspondance par nom : **pas de résolution de surcharge**.
- `GetPropertyAt(i)`/`GetMethodAt(i)` sont indexés sur la **classe courante** seulement (`O(1)`,
  `nullptr` hors borne).
- `GetPropertyCount`/`GetMethodCount` comptent la classe seule (`O(1)`) ;
  `GetTotalPropertyCount`/`GetTotalMethodCount` ajoutent récursivement les bases (`O(profondeur)`).

Usages, par domaine :
- **Outils / éditeur** — un inspecteur itère `GetPropertyAt(0..GetTotalPropertyCount())` pour
  afficher un panneau d'édition de chaque champ.
- **ECS** — découvrir les champs d'un composant pour les exposer dans un *data table* ou un
  *blueprint*.
- **IO / réseau** — sérialiser automatiquement chaque propriété d'un objet, et envoyer/recevoir
  un appel de méthode nommée (RPC) en retrouvant la `NkMethod` par son nom.

### `NkClass` — le cycle de vie réflexif

Quatre méthodes pilotent la création/destruction d'instances *sans connaître le type statique* :
`SetConstructor(ConstructorFn)` et `SetDestructor(DestructorFn)` enregistrent des `NkFunction`
(`void*()` et `void(void*)`), `CreateInstance()` appelle le ctor s'il est valide (sinon
`nullptr`), `DestroyInstance(ptr)` appelle le dtor seulement si le callback est valide **et** le
pointeur non nul. `HasConstructor`/`HasDestructor` interrogent leur présence.

Le point délicat est la **politique mémoire**. Le `DestructorFn` est censé **libérer les
ressources** de l'objet, mais **pas nécessairement désallouer** la mémoire de l'objet lui-même —
c'est à l'appelant de la rendre. Dans les exemples du module, les lambdas ctor/dtor font
`new`/`delete` bruts ; sachez que cela **entre en tension** avec la règle dure NKMemory (tout ce
qui suit NKMemory doit allouer/libérer via NKMemory). Si vous câblez ces callbacks dans du code
de production, faites passer l'allocation par `memory::NkAlloc`/`NkFree` plutôt que par le heap
CRT, sous peine de corruption de tas (`c0000374` sous Windows).

Usages, par domaine :
- **ECS / gameplay** — instancier une entité à partir du **nom** de sa classe lu dans un
  `.nkscene` (factory par réflexion).
- **Outils / éditeur** — bouton « ajouter un composant » qui crée l'objet par `CreateInstance()`.
- **IO / réseau** — reconstruire un objet reçu sérialisé sans `switch` géant sur son type.

### `MakeFromClassType<T>(name)` — fabriquer un `NkClass`

Helper statique qui construit `NkClass(name, sizeof(T), NkTypeOf<T>())`. **Attention** : il
renvoie **par valeur**, pas une référence vers un static. Les écritures du style
`auto& meta = NkClass::MakeFromClassType<T>("…");` lient donc une référence const à un
**temporaire** (durée de vie prolongée par extension, mais fragile et trompeur). Préférez une
variable **nommée par valeur** : `NkClass meta = NkClass::MakeFromClassType<T>("…");`.

### `RegisterMemberProperty` / `RegisterMemberMethod` — peupler une classe

`RegisterMemberProperty<C, V>(meta, &C::champ, name, flags)` crée un `static NkProperty` (un par
instanciation du template), calcule son **offset** via le pointeur-sur-membre, l'ajoute à `meta`
et renvoie la référence (chaînage). `RegisterMemberMethod<C, R, Args...>(meta, &C::methode, name,
flags)` crée un `static NkMethod(name, NkTypeOf<R>(), flags)` et l'ajoute.

Deux pièges importants :
- Le `static` local signifie **une seule** entité par instanciation `<C, V, name>` : réutiliser
  la même paire avec un `meta` **différent** ré-ajoute **le même pointeur** dans les deux classes.
- `RegisterMemberMethod` **ne configure pas l'invokeur** : le pointeur-de-méthode n'est pas
  réellement stocké, et les types de paramètres ne sont pas enregistrés. Pour rendre la méthode
  *appelable*, l'appelant doit lui-même faire `method.SetInvoke(...)`. C'est une enregistreuse de
  **signature**, pas un pont d'appel complet.

### `NkRegistry` — le registre central

Singleton **recommandé**, entièrement **inline**, indexant types **et** classes (512 chacun,
copie/affectation `= delete`). `Get()` renvoie l'instance Meyer.

- `RegisterType(type)` rejette null et nom null, **déduplique** par égalité de pointeur **ou**
  `strcmp` des noms, et ajoute sous la capacité. `RegisterClass(info)` est symétrique. **Note** :
  malgré le `OnRegisterCallback`, ces deux méthodes **n'invoquent pas** le callback (écart
  spec/impl à connaître).
- `FindType(name)`/`FindClass(name)` rejettent null/vide et cherchent linéairement (`strcmp`,
  `O(n)`).
- `GetType<T>()` cherche par `typeid(T).name()` et, **s'il est absent, crée et renvoie** un
  `static NkType` local (auto-création, init thread-safe) — mais **sans l'enregistrer** dans le
  tableau. `GetClass<T>()`, lui, **ne crée pas** : il renvoie `nullptr` si la classe est absente.
  Cette asymétrie est délibérée (une classe doit être explicitement enregistrée) et utile à
  retenir.
- `GetTypeCount`/`GetClassCount` (`O(1)`) et `GetTypeAt(i)`/`GetClassAt(i)` (`nullptr` hors
  borne, **ordre non garanti**) permettent de parcourir l'annuaire.

Usages, par domaine :
- **ECS / outils** — annuaire global de tous les composants/objets réfléchis, parcouru pour
  remplir un menu de création.
- **IO / réseau** — table de dispatch « nom de type → métadonnée → instance » pour la
  désérialisation et les RPC.

### `NkRegistry` — le callback d'enregistrement

Le type `OnRegisterCallback = NkFunction<void(const nk_char*, nk_bool)>` (nom, *est-ce une
classe*) est destiné à notifier chaque enregistrement. `SetOnRegisterCallback` le stocke (via
`traits::NkMove`), `GetOnRegisterCallback` y donne accès. **Mais** — c'est l'écart spec/impl déjà
signalé — `RegisterType`/`RegisterClass` **ne l'appellent jamais** dans le code réel. Ne comptez
donc pas dessus pour déclencher une réaction automatique (rafraîchir un panneau d'éditeur, par
exemple) tant que ce n'est pas câblé : il faudra notifier manuellement.

### Les macros — automatiser la réflexion

Ces macros (portée globale, hors namespace) génèrent le code réflexif standard. Elles dépendent
de `NK_CONCAT` (NKCore) et des symboles `::nkentseu::reflection::*`.

- **`NKENTSEU_REFLECT_CLASS(Name)`** — à placer **en section publique** d'une classe. Elle injecte
  `using SelfType = Name;`, une `static const NkClass& GetStaticClass()` (renvoyant un
  `static NkClass(#Name, sizeof(Name), NkTypeOf<Name>())`, init thread-safe) et une
  `virtual const NkClass& GetClass() const`. **Avertissement** : ce `virtual` **ajoute une
  vtable** à la classe — à éviter sur un type qui doit rester *POD*, et inadapté tel quel à
  l'héritage multiple.
- **`NKENTSEU_REFLECT_PROPERTY(Name)`** — injecte (en public) une
  `static const NkProperty& Get##Name##Property()` bâtie sur `offsetof(SelfType, Name)`. Elle
  **requiert** `SelfType`, donc `NKENTSEU_REFLECT_CLASS` au préalable. À placer **après** la
  déclaration du membre. Note : elle **ne s'ajoute pas** toute seule au `NkClass` — le câblage
  reste manuel.
- **`NKENTSEU_PROPERTY(Type, Name)`** — déclare `Type Name;` **et** sa réflexion en une seule
  ligne (combine le membre et `NKENTSEU_REFLECT_PROPERTY`). En section publique.
- **`NKENTSEU_REFLECT`** — l'attribut C++20 `[[nkentseu::reflect]]`, purement **informatif** :
  aucun effet à l'exécution, destiné à un outillage d'analyse statique.
- **`NKENTSEU_REGISTER_CLASS(Name)`** — à mettre dans **un seul** `.cpp`. Elle crée, dans un
  namespace anonyme, une struct `Name_Registrar` dont le constructeur appelle
  `NkRegistry::Get().RegisterClass(&Name::GetStaticClass())`, instanciée en static global
  `g_Name_registrar` → **auto-enregistrement avant `main()`**. **Pièges** : l'ordre d'init entre
  unités de traduction n'est pas garanti ; ne **jamais** la placer dans un header ; et la
  stringification de noms qualifiés (`ns::Type`) ne se concatène pas proprement via `NK_CONCAT`
  (identifiant possiblement invalide).

Usages, par domaine :
- **ECS / gameplay** — décorer un composant avec `NKENTSEU_REFLECT_CLASS` + `NKENTSEU_PROPERTY`,
  puis `NKENTSEU_REGISTER_CLASS` dans son `.cpp` pour qu'il soit visible dans tout l'annuaire.
- **Outils / éditeur** — l'inspecteur exploite `GetStaticClass()` pour lister les champs sans
  écrire de code spécifique par type.

---

### Exemple récapitulatif

```cpp
#include "NKReflection/NkType.h"
#include "NKReflection/NkClass.h"
#include "NKReflection/NkRegistry.h"
using namespace nkentseu::reflection;

// 1) Un type : la fabrique canonique, instance unique par T.
const NkType& tVec = NkTypeOf<NkVec3>();          // == NkTypeOf<NkVec3>() partout
bool same = (tVec == NkTypeOf<NkVec3>());          // true (même adresse de nom)
bool prim = NkTypeOf<float>().IsPrimitive();       // true (NK_BOOL..NK_FLOAT64)

// 2) Une classe : on la fabrique PAR VALEUR (pas auto&), on la peuple.
NkClass meta = NkClass::MakeFromClassType<Player>("Player");
NkClass::RegisterMemberProperty(meta, &Player::health, "health");
meta.SetConstructor([] -> void* {
    // En production : passer par memory::NkAlloc, pas new (règle NKMemory).
    return memory::NkAlloc<Player>();
});

// 3) Le registre central : retrouver une classe par son NOM, instancier.
NkRegistry& reg = NkRegistry::Get();
reg.RegisterClass(&meta);
if (const NkClass* c = reg.FindClass("Player")) {
    void* obj = c->CreateInstance();               // factory par réflexion
    c->DestroyInstance(obj);                        // libère ressources (mémoire à gérer)
}

// 4) En une classe annotée (header) :
//   class Enemy { public: NKENTSEU_REFLECT_CLASS(Enemy)
//                         NKENTSEU_PROPERTY(int, hp) };
//   et, dans Enemy.cpp : NKENTSEU_REGISTER_CLASS(Enemy)
```

---

[← Index NKReflection](README.md) · [Récap NKReflection](../NKReflection.md) · [Couche System](../README.md)
