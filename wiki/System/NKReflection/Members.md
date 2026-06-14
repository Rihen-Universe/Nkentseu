# Propriétés et méthodes réflexives

> Couche **System** · NKReflection · Décrire à l'exécution les **membres** d'un type : ses
> champs avec `NkProperty`, ses fonctions avec `NkMethod`, le tout lisible *et*
> modifiable/invocable sans connaître la classe à la compilation.

Un moteur arrive vite au moment où il faut **manipuler un objet sans savoir lequel**. L'éditeur
affiche un inspecteur : il doit lister les champs d'un composant et écrire dedans alors que le code
de l'inspecteur, lui, ne connaît aucun composant en particulier. Le sérialiseur doit parcourir tous
les champs d'une scène pour les écrire en JSON. Un binding de script veut appeler une méthode dont
le nom n'est qu'une chaîne. Dans tous ces cas, on a besoin d'un **descripteur runtime** : un petit
objet qui dit « ce type possède un champ nommé `health`, de tel type, à tel offset » ou « ce type a
une méthode nommée `Update` qui prend ces paramètres ». C'est exactement ce que sont `NkProperty` et
`NkMethod` — la brique « membre » de la réflexion.

L'idée centrale, et ce qui distingue NKReflection d'une réflexion « magique », c'est que **rien
n'est copié et rien n'est devine**. Une propriété ne stocke qu'un *pointeur* vers le nom, une
*référence* vers le `NkType`, et un *offset* en octets ; la méthode, en plus, un pointeur vers un
tableau de types de paramètres. C'est volontairement minimaliste et **zero-STL** : les callables
type-effacés passent par `NkFunction` (SBO + heap automatique, via NKContainers), pas par
`std::function`. Le revers, qu'il faut intégrer dès maintenant : **les durées de vie sont à votre
charge**. Le nom, le type et le tableau de paramètres doivent survivre au descripteur.

- **Namespace** : `nkentseu::reflection` (enums, classes) ; `nkentseu::reflection::MakeThreadEntry`
  (fonctions libres)
- **Headers** : `#include "NKReflection/NkProperty.h"` · `#include "NKReflection/NkMethod.h"`
- **Dépendances internes** : `NKReflection/NkType.h`, `NKContainers/Functional/NkFunction.h`,
  `NKContainers/Functional/NkBind.h` (pour `NkMethod`), `NKCore/Assert/NkAssert.h`

---

## Décrire un champ : `NkProperty`

Une `NkProperty` est la fiche d'identité d'un **champ** d'un type. Au minimum elle retient quatre
choses : un **nom** (`const nk_char*`), un **type** (`const NkType&`), un **offset** en octets
depuis le début de l'instance, et des **flags**. Avec ces quatre éléments, le code générique sait
déjà tout faire : pour lire le champ d'une instance, il ajoute l'offset au pointeur d'instance et
réinterprète ; pour l'écrire, il fait l'inverse. C'est le **mode d'accès direct**, le plus rapide
(`O(1)`, une simple arithmétique de pointeur).

```cpp
struct Enemy { float32 health; nk_uint32 level; };

// Champ 'health' décrit par offset (accès direct).
NkProperty health = NkProperty::MakeFromMember(
    (Enemy*)nullptr, &Enemy::health, "health", typeFloat);

Enemy e{ 100.f, 3 };
float32 hp = health.GetValue<float32>(&e);   // 100.f — lecture par offset
health.SetValue<float32>(&e, 75.f);          // écrit dans e.health
```

Ce n'est **pas** une copie de la valeur ni un conteneur : `NkProperty` ne possède rien, elle
*décrit*. Le constructeur `NkProperty(name, type, offset, flags)` stocke le pointeur de nom et
l'adresse du type tels quels — si la chaîne ou le `NkType` meurent avant la propriété, vous tenez
des références pendantes. En pratique, on construit ces descripteurs à partir de littéraux et de
`NkType` statiques, qui vivent tout le programme.

> **En résumé.** `NkProperty` = nom + type + offset + flags, un descripteur **non-ownant** d'un
> champ. Le mode par défaut est l'**accès direct par offset** (`O(1)`). Garantissez la durée de vie
> du nom et du `NkType` : ils ne sont **pas** copiés.

---

## Accès direct ou accès indirect : getter/setter

L'accès par offset ne marche que pour un **vrai champ** rangé linéairement dans l'objet. Mais
souvent une « propriété » n'est pas un champ brut : c'est une valeur **calculée**, ou un champ
protégé derrière un getter/setter qui valide, notifie, ou convertit. Pour ces cas, `NkProperty`
offre un **mode indirect** : on lui attache deux callables type-effacés, `GetterFn` (signature
`void*(void*)` — reçoit l'instance, renvoie un pointeur vers la valeur) et `SetterFn` (signature
`void(void*, void*)` — instance + valeur).

```cpp
NkProperty p("speed", typeFloat, 0);
p.SetGetter([](void* inst) -> void* { return &((Mover*)inst)->ComputeSpeed(); });
p.SetSetter([](void* inst, void* v) { ((Mover*)inst)->SetSpeed(*(float32*)v); });

float32 s = p.GetValue<float32>(&mover);   // passe par le getter
```

Le point important : `GetValue<T>` et `SetValue<T>` **choisissent automatiquement** le chemin. Si un
getter est attaché (`HasGetter()`), ils l'appellent ; sinon ils retombent sur l'accès direct par
offset. C'est ce qui rend l'API uniforme côté appelant — l'inspecteur écrit toujours
`prop.SetValue(...)`, sans savoir si c'est un champ ou un setter.

Attention, ce n'est **pas** un système type-sûr : `GetValue<T>` ne vérifie jamais que `T`
correspond au `NkType` réel. Un mauvais `T` est un cast erroné, donc un comportement indéfini. La
sécurité de type est à la charge de l'appelant (typiquement en croisant `GetType()` avant le cast).

> **En résumé.** Deux modes : **direct** (offset) par défaut, **indirect** (getter/setter
> type-effacés via `NkFunction`) si on appelle `SetGetter`/`SetSetter`. `GetValue<T>`/`SetValue<T>`
> basculent automatiquement de l'un à l'autre. Aucune vérification `T` vs `NkType` : mauvais cast =
> UB.

---

## Les flags : qualifier une propriété

`NkPropertyFlags` est un `enum class : nk_uint32` codé **bit-à-bit** (puissances de 2), pensé pour
être combiné par OU et testé par ET. Les six qualificatifs disent comment le moteur doit *traiter*
le champ : `NK_READ_ONLY` (l'éditeur grise le champ), `NK_TRANSIENT` (le sérialiseur l'ignore),
`NK_PDEPRECATED` (l'inspecteur avertit), `NK_STATIC` (membre de classe, offset 0), etc.

Subtilité **zero-STL** : il n'y a *pas* d'opérateurs surchargés sur cet `enum class`. Pour combiner
ou tester un flag, il faut passer par `static_cast<nk_uint32>(...)` — l'idiome utilisé partout dans
la classe :

```cpp
nk_uint32 flags = static_cast<nk_uint32>(NkPropertyFlags::NK_READ_ONLY)
                | static_cast<nk_uint32>(NkPropertyFlags::NK_TRANSIENT);
NkProperty p("id", typeU32, offsetof(Foo, id), flags);
if (p.IsReadOnly()) { /* griser dans l'inspecteur */ }
```

Plutôt que de tester les bits à la main, on dispose de six prédicats nommés (`IsReadOnly`,
`IsWriteOnly`, `IsStatic`, `IsConst`, `IsTransient`, `IsDeprecated`) qui font le `static_cast` et le
ET en interne. `SetValue` consulte d'ailleurs `IsReadOnly()` : il déclenche un
`NKENTSEU_ASSERT(!IsReadOnly())` avant d'écrire — protection **en debug seulement**, désactivable en
release selon la configuration des asserts.

> **En résumé.** Flags bit-à-bit sur `enum class` **sans opérateurs** → toujours
> `static_cast<nk_uint32>`. Six prédicats nommés (`IsReadOnly`…) pour lire. `SetValue` ne protège le
> read-only que via `NKENTSEU_ASSERT` (debug).

---

## Décrire une méthode : `NkMethod`

`NkMethod` est le pendant de `NkProperty` pour les **fonctions membres**. Elle retient un nom, un
**type de retour** (`const NkType&`), des **flags** (`NkMethodFlags`), une **liste de types de
paramètres** et un **invokeur** type-effacé. L'invocation passe par `InvokeFn`, de signature
`void*(void*, void**)` : on lui donne l'instance et un tableau de pointeurs vers les arguments, elle
renvoie un `void*` vers le résultat (`nullptr` pour `void`).

```cpp
NkMethod m("TakeDamage", typeVoid);
m.SetInvoke([](void* inst, void** args) -> void* {
    ((Enemy*)inst)->TakeDamage(*(float32*)args[0]);
    return nullptr;
});
float32 dmg = 25.f;
void* argv[] = { &dmg };
m.Invoke(&enemy, argv);     // appelle Enemy::TakeDamage(25.f)
```

Comme pour les paramètres : `SetParameters(types, count)` stocke **le pointeur tel quel**, sans la
moindre copie. Le tableau de `const NkType*` doit donc survivre au `NkMethod` — l'idiome est un
tableau `static` :

```cpp
static const NkType* params[] = { &typeFloat };
m.SetParameters(params, 1);
```

Attention à un détail qui surprend : `NkMethodFlags::NK_STATIC` vaut **1** (`1 << 0`), alors que
`NkPropertyFlags::NK_STATIC` vaut **4** (`1 << 2`). Les deux enums ont des dispositions de bits
différentes — ne réutilisez jamais une valeur de l'un pour l'autre.

> **En résumé.** `NkMethod` = nom + type de retour + flags + types de paramètres (**non copiés**) +
> invokeur `void*(void*, void**)`. `SetParameters` ne possède pas le tableau (gardez-le `static`).
> Le cast du retour de `Invoke` est à votre charge. `NK_STATIC` n'a **pas** la même valeur que dans
> `NkPropertyFlags`.

---

## Pièges des fabriques automatiques

Les fabriques `MakeFromAccessors` (propriété) et `MakeFromMember` (méthode) sont des raccourcis
séduisants, mais leur implémentation cache deux pièges **réels** qu'il faut connaître avant de s'y
fier en production.

D'abord, le **stockage `static`**. Le getter généré par `MakeFromAccessors` et l'invokeur généré par
`MakeFromMember` utilisent une variable `static` locale (`static ValueType result;` /
`static ReturnType resultStorage;`) pour héberger la valeur de retour. C'est donc une mémoire
**partagée entre tous les appels** : non réentrant, écrasé en cas d'appels concurrents (multithread)
ou imbriqués. Pour du code chaud ou multithread, écrivez un wrapper manuel via `MakeFromCallable` /
`SetInvoke`.

Ensuite, la **limitation aux méthodes sans argument** : `NkMethod::MakeFromMember` n'invoque
réellement la méthode que si `sizeof...(Args) == 0` (un `if constexpr`). Dès qu'il y a un paramètre,
les `args` ne sont **pas** extraits du tableau — la méthode n'est pas appelée correctement. Pour une
méthode à arguments, écrivez l'invokeur à la main (c'est là que `void**` prend tout son sens).

> **En résumé.** `MakeFromAccessors`/`MakeFromMember` automatisent les cas simples mais utilisent un
> **stockage `static`** (non thread-safe) ; `NkMethod::MakeFromMember` ne gère que les méthodes
> **sans argument**. Au-delà → `MakeFromCallable` + `SetInvoke` + wrapper manuel.

---

## Lancer un thread depuis une méthode : `MakeThreadEntry`

Dernier outil de la famille, deux fonctions libres `MakeThreadEntry` adaptent une **méthode membre**
en *entry point* de thread. Elles prennent une instance et un pointeur sur méthode de signature
`void(void*)` (avec une surcharge pour la version `const`), et renvoient un `NkFunction<void(void*)>`
prêt à passer à `NkThread::Start`. En interne, le lien instance↔méthode est fait via
`NkBindThreadFunc`.

```cpp
auto entry = nkentseu::reflection::MakeThreadEntry(&worker, &Worker::Run);
thread.Start(entry, userData);   // worker.Run(userData) tourne dans le thread
```

> **En résumé.** `MakeThreadEntry(instance, &Class::Method)` transforme une méthode membre
> (`void(void*)`, surcharge `const`) en `NkFunction<void(void*)>` pour `NkThread::Start`.

---

## Aperçu de l'API

Tous les éléments publics des deux headers. Complexités entre crochets quand utile.

### `enum class NkPropertyFlags : nk_uint32` — qualificatifs de champ

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Valeur | `NK_NONE` (0) | Aucun flag. |
| Accès | `NK_READ_ONLY` (1) · `NK_WRITE_ONLY` (2) | Lecture seule / écriture seule. |
| Nature | `NK_STATIC` (4) · `NK_PCONST` (8) | Membre statique / champ const. |
| Sérialisation | `NK_TRANSIENT` (16) | À ne pas sérialiser. |
| Cycle de vie | `NK_PDEPRECATED` (32) | Champ déprécié. |
| Idiome | `static_cast<nk_uint32>(flag)` | Combiner/tester (pas d'opérateurs surchargés). |

### `class NkProperty` — descripteur de champ

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Typedefs | `GetterFn = NkFunction<void*(void*)>` · `SetterFn = NkFunction<void(void*,void*)>` | Callables d'accès indirect. |
| Construction | `NkProperty(name, type, offset, flags=0)` | Stocke pointeur/référence (non copiés). |
| Métadonnées | `GetName` · `GetType` · `GetOffset` · `GetFlags` `[O(1)]` | Nom / type / offset / flags. |
| Prédicats | `IsReadOnly` · `IsWriteOnly` · `IsStatic` · `IsConst` · `IsTransient` · `IsDeprecated` `[O(1)]` | Test des flags. |
| Accesseurs | `SetGetter` · `SetSetter` · `HasGetter` · `HasSetter` `[O(1)]` | Attacher/tester getter/setter. |
| Valeurs | `GetValue<T>(inst)` · `SetValue<T>(inst, v)` `[O(1)]` | Lire/écrire (indirect si attaché, sinon offset). |
| Valeurs | `GetValuePtr(inst)` `[O(1)]` | Pointeur brut par offset (**contourne** getter/setter). |
| Fabriques | `MakeWithAccessors(...)` · `MakeFromMember(...)` · `MakeFromAccessors(...)` | Construire avec/sans accesseurs ; depuis pointeur-membre. |

### `enum class NkMethodFlags : nk_uint32` — qualificatifs de méthode

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Valeur | `NK_NONE` (0) | Aucun flag. |
| Nature | `NK_STATIC` (1) · `NK_MCONST` (2) | Méthode statique / const (valeurs **≠** `NkPropertyFlags`). |
| Polymorphisme | `NK_VIRTUAL` (4) · `NK_ABSTRACT` (8) · `NK_MFINAL` (16) | Virtuelle / abstraite / finale. |
| Cycle de vie | `NK_MDEPRECATED` (32) | Méthode dépréciée. |

### `class NkMethod` — descripteur de méthode

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Typedef | `InvokeFn = NkFunction<void*(void*,void**)>` | Invokeur type-effacé. |
| Construction | `NkMethod(name, returnType, flags=0)` | Stocke pointeur/référence (non copiés). |
| Signature | `GetName` · `GetReturnType` · `GetFlags` `[O(1)]` | Nom / type de retour / flags. |
| Prédicats | `IsStatic` · `IsConst` · `IsVirtual` · `IsFinal` · `IsAbstract` · `IsDeprecated` `[O(1)]` | Test des flags. |
| Paramètres | `SetParameters(types, count)` · `GetParameterCount` · `GetParameterType(i)` `[O(1)]` | Liste **non-ownée** des types de paramètres. |
| Invocation | `SetInvoke` · `Invoke(inst, args)` · `HasInvoke` · `GetInvoke` `[O(1)]` | Configurer/appeler l'invokeur. |
| Fabriques | `MakeFromCallable(...)` · `MakeFromMember(...)` | Depuis callable conforme ; depuis pointeur-membre (sans arg). |

### Fonctions libres — `nkentseu::reflection`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Threading | `MakeThreadEntry(inst, &C::M)` (+ surcharge `const`) | Adapte une méthode `void(void*)` en `NkFunction<void(void*)>`. |

---

## Référence complète

Chaque élément repris en détail : comportement, complexité, et usages par domaine. Les éléments
triviaux (accesseurs O(1)) sont brefs ; les pièges et les mécanismes d'accès sont à fond.

### `NkPropertyFlags` — les six qualificatifs

L'enum code six états d'un champ sur des bits séparés (`NK_READ_ONLY`=1, `NK_WRITE_ONLY`=2,
`NK_STATIC`=4, `NK_PCONST`=8, `NK_TRANSIENT`=16, `NK_PDEPRECATED`=32), combinables par OU. Chacun
oriente le traitement générique :

- **Outils / éditeur** — `NK_READ_ONLY` grise un champ dans l'inspecteur ; `NK_PDEPRECATED` y
  affiche un avertissement ou masque le champ obsolète.
- **IO / réseau** — `NK_TRANSIENT` exclut le champ de la sérialisation (caches, handles GPU, états
  recalculables) : le sérialiseur de scène saute ces champs au lieu d'écrire des octets inutiles ou
  invalides à la relecture.
- **ECS** — `NK_STATIC` marque un membre de classe (offset 0) partagé par tous les composants, à
  traiter différemment d'un champ d'instance.

Rappel d'usage : pas d'opérateurs bitwise sur l'`enum class`. On combine avec
`static_cast<nk_uint32>(...)` et on lit via les prédicats nommés. `O(1)` partout.

### `NkProperty` — construction et métadonnées

Le constructeur `NkProperty(name, type, offset, flags=0)` est inline et **ne copie ni le nom ni le
type** : il garde `mName` (le `const nk_char*` reçu) et `mType` (l'adresse du `NkType`). Les
accesseurs `GetName`, `GetType`, `GetOffset`, `GetFlags` ne font que relire ces membres, en `O(1)`.
Les six prédicats (`IsReadOnly`…) appliquent un `static_cast` + ET sur `mFlags`, eux aussi `O(1)`.

- **Outils / éditeur** — l'inspecteur itère les propriétés d'un type, affiche `GetName()` comme
  label et choisit le widget d'après `GetType()`.
- **IO / réseau** — le sérialiseur boucle sur les propriétés, saute celles marquées `IsTransient()`,
  écrit nom + valeur des autres.

### `GetValue` / `SetValue` — lire et écrire une valeur

Ce sont les deux opérations centrales. `GetValue<T>(instance)` : si un getter est attaché
(`mGetter.IsValid()`), elle l'appelle et déréférence (`*static_cast<T*>(mGetter(instance))`) ; sinon
elle calcule l'adresse par offset (`*reinterpret_cast<T*>((nk_char*)instance + mOffset)`). Elle
**renvoie une référence**. `SetValue<T>(instance, value)` déclenche d'abord
`NKENTSEU_ASSERT(!IsReadOnly())`, puis emprunte le setter si présent, sinon écrit par offset. Coût :
`O(1)` (plus le coût de `NkFunction` en mode indirect).

Le point dur, valable dans tous les domaines : **aucune vérification de type**. `T` est cru sur
parole ; un mismatch avec le `NkType` réel est un cast invalide donc un UB. La parade est toujours
de croiser `GetType()` avant de choisir `T`.

- **Outils / éditeur** — la boucle d'inspecteur lit `GetValue<float32>` pour alimenter un slider,
  puis `SetValue<float32>` au déplacement ; le test `IsReadOnly()` désactive l'écriture.
- **IO / réseau** — au chargement, le désérialiseur fait `SetValue` champ par champ ; à la sauvegarde,
  `GetValue` pour chaque champ non transient.
- **Gameplay / IA** — un système de tweaks ou de console de debug modifie un paramètre par son nom
  (`"speed"`, `"aggro"`) sans dépendance compile-time sur la classe.
- **Animation** — un *property animator* écrit une valeur interpolée dans un champ ciblé par son
  descripteur, frame après frame.

### `GetValuePtr` — l'adresse brute

`GetValuePtr(instance)` renvoie `(nk_char*)instance + mOffset` sans aucun cast et **en contournant
totalement getter/setter** (et la gestion du cas statique). C'est l'échappatoire bas niveau, `O(1)`,
quand on veut l'adresse mémoire elle-même plutôt que la valeur typée.

- **GPU / rendu** — récupérer l'adresse d'un champ pour le copier directement dans un buffer uniforme
  ou un layout serialisé binaire, sans passer par une copie typée.
- **IO / réseau** — `memcpy` direct d'un bloc de champ vers/depuis un flux binaire compact.

À n'utiliser qu'en connaissant le mode d'accès réel : sur une propriété calculée (getter sans champ
sous-jacent), l'adresse retournée n'a pas de sens.

### `SetGetter` / `SetSetter` / `HasGetter` / `HasSetter` — le mode indirect

`SetGetter(GetterFn)` et `SetSetter(SetterFn)` prennent le callable **par valeur puis le déplacent**
(`traits::NkMove`), ce qui évite toute fuite (la mémoire du callable est gérée par `NkFunction`,
SBO + heap). `HasGetter`/`HasSetter` ne font que `mGetter.IsValid()` / `mSetter.IsValid()`, `O(1)`.
Ce sont eux qui font basculer `GetValue`/`SetValue` en mode indirect.

- **Gameplay** — exposer une propriété **calculée** (vitesse dérivée, score formaté) sans champ réel.
- **Outils / éditeur** — router l'écriture vers un setter qui valide (clamp, notification de
  changement) au lieu d'écrabouiller le champ brut.

### `NkProperty::MakeWithAccessors` — fabrique flexible

Template `MakeWithAccessors<GetterType, SetterType>(name, type, offset, getter, setter, flags)`.
Elle construit la propriété puis, via `if constexpr (!NkIsSame<GetterType,void>::value)`, attache le
getter (idem setter) — les types par défaut `void` signifient « pas d'accesseur ». C'est la fabrique
de référence quand on veut un mélange souple : champ par offset *plus* éventuellement un setter de
validation. `O(1)`.

### `NkProperty::MakeFromMember` — depuis un pointeur sur membre

Template `MakeFromMember(instance, ClassType::*memberPtr, name, typeMeta, flags)`. L'argument
`instance` est **ignoré** (`NKENTSEU_UNUSED`) ; l'offset est calculé par l'idiome
`reinterpret_cast<nk_usize>(&(((ClassType*)0)->*memberPtr))` (un *offsetof* manuel via pointeur nul —
UB technique mais idiome interne assumé). Elle produit une propriété en **accès direct**, sans
getter/setter. C'est la fabrique la plus courante pour les vrais champs.

- **ECS / sérialisation** — enregistrer en masse les champs d'un composant
  (`MakeFromMember(nullptr, &Transform::position, "position", typeVec3)`), prêts pour l'inspecteur et
  le JSON.

### `NkProperty::MakeFromAccessors` — depuis getter/setter membres

Template `MakeFromAccessors(instance, getterPtr, setterPtr, name, typeMeta, flags)`. Elle fabrique
des lambdas capturant `instance` et les pointeurs-membres. **Deux pièges réels** :

- Le getter wrapper stocke le résultat dans un `static ValueType result;` — **non thread-safe**,
  valeur partagée et écrasée entre appels concurrents/imbriqués.
- La propriété est créée avec `flags | NK_READ_ONLY` ; **si** `setterPtr != nullptr`, le setter est
  attaché *et* `NK_READ_ONLY` est retiré (`mFlags &= ~NK_READ_ONLY`). De plus le callable est lié à
  l'**instance capturée**, pas au `void*` passé à `GetValue`/`SetValue`.

À réserver aux cas mono-thread simples ; sinon, wrapper manuel.

### `NkMethodFlags` — les six qualificatifs de méthode

Mêmes mécaniques bit-à-bit que pour les propriétés, mais **disposition différente** :
`NK_STATIC`=1, `NK_MCONST`=2, `NK_VIRTUAL`=4, `NK_ABSTRACT`=8, `NK_MFINAL`=16, `NK_MDEPRECATED`=32.
Le piège à retenir : `NK_STATIC` vaut **1** ici contre **4** chez `NkPropertyFlags`.

- **Outils / scripting** — exposer ou non une méthode selon `IsStatic()`/`IsConst()` (appel sur la
  classe vs sur l'instance ; méthode const = sans effet de bord, appelable en lecture seule).
- **Éditeur** — afficher un bouton « appeler » uniquement pour les méthodes non `IsAbstract()`.

### `NkMethod` — construction, signature, paramètres

Le constructeur `NkMethod(name, returnType, flags=0)` initialise nom, type de retour (stocké en
pointeur), flags, un invokeur vide et `mParameterTypes = nullptr`. Les accesseurs `GetName`,
`GetReturnType`, `GetFlags` et les six prédicats sont `O(1)`.

Côté paramètres, `SetParameters(types, count)` **stocke le pointeur tel quel** — *aucune copie*. Le
tableau de `const NkType*` doit donc rester valide toute la vie du `NkMethod` (idiome :
`static const NkType* params[]`). `GetParameterCount()` renvoie le compte ; `GetParameterType(i)`
fait `NKENTSEU_ASSERT(i < count)` puis déréférence. Tout en `O(1)`.

- **Scripting / binding** — décrire la signature pour valider les arguments d'un appel script avant
  de l'exécuter (arité via `GetParameterCount`, types via `GetParameterType`).
- **Outils / éditeur** — générer une UI d'appel de méthode (un widget par paramètre selon son type).

### `NkMethod::SetInvoke` / `Invoke` / `HasInvoke` / `GetInvoke` — appeler

`SetInvoke(InvokeFn)` prend par valeur puis déplace l'invokeur. `Invoke(instance, args)` renvoie
`nullptr` si aucun invokeur n'est configuré, sinon appelle `mInvoke(instance, args)` et renvoie son
`void*` — **le cast du retour est à votre charge**. `HasInvoke()` teste la validité ;
`GetInvoke()` donne une référence const sur l'invokeur (inspection ou copie). `O(1)` plus le coût du
callable.

- **Gameplay / IA** — déclencher un comportement nommé (`"Attack"`, `"Flee"`) depuis une table de
  données ou un graphe, sans dépendance compile-time.
- **IO / réseau** — un RPC reçoit un nom de méthode et un buffer d'arguments, construit le `void**`,
  appelle `Invoke`, renvoie le résultat sérialisé.
- **UI / 2D** — câbler le clic d'un bouton sur une méthode décrite par réflexion.

### `NkMethod::MakeFromCallable` / `MakeFromMember` — fabriques

`MakeFromCallable(name, returnType, callable, flags)` enveloppe un callable **déjà conforme** à
`void*(void*, void**)` dans l'invokeur — elle ne configure **pas** les paramètres (à faire à part).
C'est la voie robuste, y compris pour les méthodes à arguments (l'extraction du `void**` est dans
votre callable).

`MakeFromMember(instance, methodPtr, name, returnTypeMeta, flags)` génère un wrapper depuis un
pointeur-membre, mais avec **deux limites fortes** : elle n'invoque la méthode que si
`sizeof...(Args) == 0` (sinon les `args` ne sont pas extraits), et elle utilise un
`static ReturnType resultStorage;` (non thread-safe, partagé). Pas de surcharge pour méthode const.
À réserver aux méthodes sans argument, mono-thread.

### `MakeThreadEntry` — méthode membre vers entry de thread

Les deux surcharges (`void(void*)` et sa version `const`) renvoient un `NkFunction<void(void*)>` via
`NkBindThreadFunc(instance, method)`, directement consommable par `NkThread::Start`.

- **Threading** — lancer le `Run` d'un worker, le `Tick` d'un sous-système ou la boucle d'un job sur
  un thread dédié, sans écrire de trampoline C à la main.

### Idiomes transverses et pièges

- **Non-ownership** — nom, type/returnType et tableau de paramètres ne sont **jamais copiés** :
  garantissez leur survie (littéraux, `NkType` statiques, tableaux `static`).
- **Type-safety appelant** — `GetValue<T>`/`SetValue<T>`/`Invoke` ne valident pas `T`/le cast :
  croisez `GetType()`/`GetReturnType()` avant.
- **Thread-safety** — les wrappers `MakeFromAccessors`/`MakeFromMember` stockent leur résultat en
  `static` local : non réentrants.
- **Read-only** — `SetValue` ne protège que via `NKENTSEU_ASSERT` (désactivable en release).
- **Flags** — pas d'opérateurs sur les `enum class` → toujours `static_cast<nk_uint32>` ; et
  `NK_STATIC` n'a pas la même valeur entre propriétés (4) et méthodes (1).
- **Mémoire des callables** — `NkFunction` (SBO + heap auto) gère tout : pas de `new`/`delete`
  manuel, pas de fuite à la copie/déplacement.

---

### Exemple récapitulatif

```cpp
#include "NKReflection/NkProperty.h"
#include "NKReflection/NkMethod.h"
using namespace nkentseu::reflection;

struct Enemy {
    float32   health;
    nk_uint32 level;
    void TakeDamage(float32 d) { health -= d; }
};

// 1. Décrire un champ par offset (accès direct) — base de l'inspecteur et du JSON.
NkProperty pHealth = NkProperty::MakeFromMember(
    (Enemy*)nullptr, &Enemy::health, "health", typeFloat);

Enemy e{ 100.f, 3 };
float32 hp = pHealth.GetValue<float32>(&e);   // 100.f
pHealth.SetValue<float32>(&e, 75.f);          // écrit dans e.health

// 2. Marquer un champ non sérialisable (flags bit-à-bit, sans opérateurs).
nk_uint32 f = static_cast<nk_uint32>(NkPropertyFlags::NK_TRANSIENT);
NkProperty pCache("cacheId", typeU32, offsetof(Enemy, level), f);
if (!pCache.IsTransient()) { /* sérialiser */ }

// 3. Décrire et invoquer une méthode à argument (invokeur manuel, robuste).
NkMethod mDamage("TakeDamage", typeVoid);
mDamage.SetInvoke([](void* inst, void** args) -> void* {
    ((Enemy*)inst)->TakeDamage(*(float32*)args[0]);
    return nullptr;
});
static const NkType* params[] = { &typeFloat };   // tableau STATIC : non copié
mDamage.SetParameters(params, 1);

float32 dmg = 25.f;
void* argv[] = { &dmg };
mDamage.Invoke(&e, argv);                      // e.health -> 50.f

// 4. Lancer une méthode membre sur un thread.
auto entry = MakeThreadEntry(&worker, &Worker::Run);
thread.Start(entry, userData);
```

---

[← Index NKReflection](README.md) · [Récap NKReflection](../NKReflection.md) · [Couche System](../README.md)
