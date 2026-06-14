# Les assertions

> Couche **Foundation** · NKCore · Affirmer des **invariants** que le code tient pour vrais, et
> décider — de façon configurable — ce qui se passe quand l'un d'eux est violé : s'arrêter dans le
> débogueur, abandonner, journaliser, continuer.

Une assertion est une affirmation que le code tient pour vraie à un point précis : « ce pointeur
n'est jamais nul ici », « cet indice est forcément dans les bornes », « cette taille est positive ».
Tant que l'affirmation tient, elle ne fait **rien** — pas de coût, pas de bruit. Mais si elle est
violée, c'est le signe d'un **bug dans la logique du programme**, et l'assertion le signale
**immédiatement**, à l'endroit exact, au lieu de laisser le programme dériver dans un état
incohérent et planter cent lignes plus loin avec une pile d'appels qui ne pointe vers rien
d'utile. C'est tout l'intérêt : transformer un crash diffus et lointain en un arrêt net, sur la
bonne ligne, avec le bon message.

NKCore fournit un système d'assertions **configurable** de bout en bout. On ne se contente pas de
crasher : un *handler* central reçoit une fiche décrivant l'échec et **décide** de la suite —
breakpoint, `abort`, télémétrie, journal, dialogue. Et comme tout le reste de la couche Foundation,
ce système est **zero-STL** : pas de `<cassert>`, pas d'exception ; juste des macros, un pointeur de
fonction global, et un point d'arrêt portable.

Un point de **nomenclature** à fixer tout de suite, car c'est le piège n°1 : toutes les macros
réelles portent le préfixe **`NKENTSEU_`** — `NKENTSEU_ASSERT`, `NKENTSEU_ASSERT_MSG`,
`NKENTSEU_VERIFY`, `NKENTSEU_STATIC_ASSERT`, `NKENTSEU_DEBUGBREAK`. Les formes `NK_ASSERT` /
`NK_VERIFY` que l'on croise dans les blocs de commentaires des headers sont des **exemples
pédagogiques** locaux, **pas** l'API. N'écrivez jamais `NK_ASSERT` dans votre code : ça ne compile
pas.

- **Namespace** : `nkentseu`
- **Headers** : `#include "NKCore/Assert/NkAssert.h"` (les macros) ·
  `#include "NKCore/Assert/NkAssertHandler.h"` (le handler) ·
  `NkAssertion.h` (les types) et `NkDebugBreak.h` (le break) sont tirés transitivement.

---

## Affirmer un invariant

La macro de base existe en deux formes. `NKENTSEU_ASSERT(condition)` prend simplement l'expression
qui **doit** être vraie ; `NKENTSEU_ASSERT_MSG(condition, message)` y ajoute un message qui explique
*pourquoi* l'invariant tient — c'est ce message que vous lirez six mois plus tard quand l'assertion
sautera.

```cpp
NKENTSEU_ASSERT(ptr != nullptr);
NKENTSEU_ASSERT_MSG(size > 0, "Buffer size must be positive");
```

Si l'expression est fausse, le système se déclenche : il construit une fiche `NkAssertionInfo`
(l'expression stringifiée, le message, le fichier, la ligne, la fonction), la passe au handler
courant, et **exécute la décision** que celui-ci renvoie — typiquement un breakpoint qui fige le
programme pile sur cette ligne. En configuration *release* (`NKENTSEU_ENABLE_ASSERTS` non défini),
`NKENTSEU_ASSERT` est **désactivée** : elle se réduit à `((void)0)` et **n'évalue même pas** son
expression, pour ne pas payer le coût des vérifications en production.

Cette désactivation pose un piège classique : que se passe-t-il si l'expression a un **effet de bord
nécessaire** ? Écrire `NKENTSEU_ASSERT(InitializeHardware() == HW_SUCCESS)` semble raisonnable — mais
en release, `InitializeHardware()` ne serait **jamais appelé**, le matériel resterait non
initialisé, et le bug serait invisible en debug. C'est exactement le rôle de
`NKENTSEU_VERIFY(expression)` (et `NKENTSEU_VERIFY_MSG`) : en debug elle est **identique** à
`NKENTSEU_ASSERT` (même fiche, même handler, même break), mais en release elle se réduit à
`((void)(expression))` — l'expression **est évaluée**, son résultat simplement ignoré. La règle est
donc nette :

```cpp
NKENTSEU_VERIFY(InitializeHardware() == HW_SUCCESS);   // l'appel a TOUJOURS lieu
```

> **En résumé.** `NKENTSEU_ASSERT` / `NKENTSEU_ASSERT_MSG` pour les invariants internes : actives en
> debug, **muettes et non évaluées** en release. `NKENTSEU_VERIFY` / `NKENTSEU_VERIFY_MSG` quand
> l'expression doit s'exécuter **même en release** (effet de bord). Ne jamais mettre d'effet de bord
> dans un `ASSERT`.

---

## Vérifier à la compilation

Tout ce qui précède s'évalue à l'**exécution**. Pour les invariants qu'on peut prouver à la
**compilation** — la taille d'un type, une propriété de trait, un alignement — il existe
`NKENTSEU_STATIC_ASSERT`. Attention à sa signature, qui est le **deuxième piège** du module : elle
prend **trois** paramètres `(name, condition, message)`, pas deux (le commentaire d'en-tête du
header dit le contraire — c'est une incohérence connue). Le premier argument est un **nom** qui
identifie la vérification ; il est intégré au message d'erreur du compilateur.

```cpp
NKENTSEU_STATIC_ASSERT(TraitCheck,
                       std::is_trivially_copyable<T>::value,
                       "must be trivially copyable");
```

Elle se résout vers `static_assert` en C++11+, `_Static_assert` en C11+, ou un *tableau de taille
négative* en repli sur les compilateurs anciens. Elle est **toujours active**, y compris en release :
une condition fausse est une **erreur de compilation**, pas un crash. Un dernier piège sur le repli :
il dérive son identifiant unique de `__LINE__`, donc deux `NKENTSEU_STATIC_ASSERT` sur la **même
ligne** entrent en collision — gardez-en un par ligne.

> **En résumé.** `NKENTSEU_STATIC_ASSERT(name, condition, message)` — **3 arguments** — vérifie un
> invariant à la **compilation** (erreur de build, jamais désactivée). Un seul par ligne sur les
> compilateurs anciens.

---

## Personnaliser le comportement

Ce qui se passe au moment d'un échec n'est **pas figé**. Le système repose sur trois pièces nettes.
`NkAssertionInfo` est la **fiche** de l'échec : l'expression stringifiée, le message (éventuellement
nul), et surtout le **fichier / ligne / fonction** d'origine — tous ses champs sont des pointeurs
vers des littéraux qu'elle ne possède pas (rien à libérer). `NkAssertAction` est la **décision** :
`NK_CONTINUE`, `NK_BREAK`, `NK_ABORT`, `NK_IGNORE`, `NK_IGNORE_ALL`. Et `NkAssertHandler` est le
**gestionnaire** central qu'on peut remplacer par le sien.

On installe son propre handler au démarrage avec `NkAssertHandler::SetCallback`. Le callback reçoit
la fiche en lecture seule et renvoie l'action voulue :

```cpp
NkAssertAction MyAssertHandler(const NkAssertionInfo& info) {
    NK_LOG_ERROR("Assertion '%s' violée : %s  (%s:%d, %s)",
                 info.expression, info.message ? info.message : "",
                 info.file, info.line, info.function);
    Telemetry::Report(info);                 // remonter une télémétrie
    return NkAssertAction::NK_BREAK;          // puis figer le débogueur
}

// au démarrage, sur le thread principal :
NkAssertHandler::SetCallback(&MyAssertHandler);
```

Pour restaurer le comportement d'origine, il suffit de `SetCallback(nullptr)`. Et pour **chaîner**
votre logique au comportement standard, terminez votre callback par
`return NkAssertHandler::DefaultCallback(info);` — la politique par défaut est *adaptative* :
`NK_BREAK` en debug, `NK_ABORT` en release.

La dernière brique est `NkDebugBreak.h`, qui fournit le **point d'arrêt débogueur portable**
(`__debugbreak()` sous MSVC, `int3` sur x86, `__builtin_trap()` sur ARM, `raise(SIGTRAP)` en repli).
C'est lui que la macro déclenche sur `NK_BREAK`, et que vous pouvez invoquer directement —
`NKENTSEU_DEBUGBREAK_IF(condition)` pose un breakpoint conditionnel **sans coût en release**.

> **En résumé.** Trois pièces : `NkAssertionInfo` (la fiche : expression, message, fichier, ligne,
> fonction), `NkAssertAction` (la décision), `NkAssertHandler` (le gestionnaire remplaçable via
> `SetCallback`). Installez votre handler au démarrage **mono-thread** ; chaînez avec
> `DefaultCallback(info)`. `NkDebugBreak.h` fournit le break portable.

---

## Le bon usage

Les assertions servent à vérifier des **invariants internes** — des choses qui « ne devraient jamais
arriver » si le code est correct. Ce n'est **pas** de la validation d'entrées : une saisie
utilisateur invalide, un fichier corrompu, un paquet réseau mal formé ne sont **pas** des bugs, ce
sont des cas **normaux** à gérer par du code ordinaire (un test, un retour d'erreur, un repli) — et
**surtout pas** par une assertion qui planterait en production. La distinction est tranchée :
**assertion pour les erreurs de *programmeur*, code de gestion pour les erreurs d'*utilisateur***.

> **En résumé.** Assertions = bugs du programmeur (invariants internes). Entrées externes (saisie,
> fichiers, réseau) = code de gestion normal, jamais une assertion.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Chacun est détaillé dans la « Référence complète ».

### Macros d'assertion — `NkAssert.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Runtime | `NKENTSEU_ASSERT(condition)` | Affirme `condition` ; **désactivée et non évaluée** en release. |
| Runtime | `NKENTSEU_ASSERT_MSG(condition, message)` | Idem avec un message explicatif. |
| Runtime | `NKENTSEU_VERIFY(expression)` | Comme `ASSERT` en debug ; en release **évalue** `expression`, ignore l'échec. |
| Runtime | `NKENTSEU_VERIFY_MSG(expression, message)` | Version avec message de `VERIFY`. |
| Interne | `NKENTSEU_ASSERT_IMPL(condition, message, file, line, func)` | Expansion commune (`@internal`) — ne pas appeler directement. |
| Compile-time | `NKENTSEU_STATIC_ASSERT(name, condition, message)` | Vérif **à la compilation** (3 args) ; toujours active. |

### Points d'arrêt — `NkDebugBreak.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Break | `NKENTSEU_DEBUGBREAK()` | Point d'arrêt débogueur portable, **toujours actif** (même en release). |
| Break | `NKENTSEU_DEBUG_BREAK()` | Alias de `NKENTSEU_DEBUGBREAK()` (`@deprecated`) ; forme appelée par la macro. |
| Break | `NKENTSEU_DEBUGBREAK_IF(condition)` | Break conditionnel **debug-only** ; en release `((void)0)`, condition non évaluée. |
| Break | `NKENTSEU_DEBUG_BREAK_IF(condition)` | Alias de `NKENTSEU_DEBUGBREAK_IF` (`@deprecated`). |

### Types — `NkAssertion.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Donnée | `struct NkAssertionInfo` | Fiche d'un échec (agrégat POD). |
| Champ | `.expression` `const nk_char*` | Condition échouée stringifiée. |
| Champ | `.message` `const nk_char*` | Message optionnel (peut être `nullptr`). |
| Champ | `.file` `const nk_char*` | Fichier source. |
| Champ | `.line` `nk_int32` | Numéro de ligne. |
| Champ | `.function` `const nk_char*` | Nom de la fonction. |
| Énum | `enum class NkAssertAction : nk_int32` | Décision renvoyée par le handler. |
| Valeur | `NK_CONTINUE` (0) | Continuer ; la macro ne fait rien. |
| Valeur | `NK_BREAK` (1) | Déclenche `NKENTSEU_DEBUG_BREAK()`. |
| Valeur | `NK_ABORT` (2) | Appelle `::abort()`. |
| Valeur | `NK_IGNORE` (3) | Ignorer cette assertion (sémantique handler ; macro inerte). |
| Valeur | `NK_IGNORE_ALL` (4) | Ignorer toutes (sémantique handler ; macro inerte). |

### Gestionnaire — `NkAssertHandler.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `using NkAssertCallback = NkAssertAction (*)(const NkAssertionInfo&)` | Pointeur de fonction handler. |
| Méthode | `static NkAssertCallback GetCallback()` | Callback courant (pour sauver/restaurer). |
| Méthode | `static void SetCallback(NkAssertCallback)` | Installe un handler ; `nullptr` restaure le défaut. **Non thread-safe.** |
| Méthode | `static NkAssertAction DefaultCallback(const NkAssertionInfo&)` | Politique par défaut : `NK_BREAK` en debug, `NK_ABORT` en release. |
| Méthode | `static NkAssertAction HandleAssertion(const NkAssertionInfo&)` | Point d'entrée des macros ; délègue au callback, fallback `DefaultCallback`. **Thread-safe.** |

---

## Référence complète

### `NKENTSEU_ASSERT` et `NKENTSEU_ASSERT_MSG`

Le cœur du système. `NKENTSEU_ASSERT(condition)` est exactement
`NKENTSEU_ASSERT_MSG(condition, "")` — message vide. Toutes deux passent par
`NKENTSEU_ASSERT_IMPL`, qui en debug exécute le motif `do { if (!(condition)) { … } } while(0)` :
construction d'un `NkAssertionInfo{ #condition, message, NKENTSEU_FILE_NAME, NKENTSEU_LINE_NUMBER,
NKENTSEU_FUNCTION_NAME }`, appel à `NkAssertHandler::HandleAssertion(info)`, puis exécution de
l'action renvoyée — `NK_BREAK` → `NKENTSEU_DEBUG_BREAK()`, `NK_ABORT` → `::abort()`. Détail crucial :
la macro ne traite **que** ces deux actions ; `NK_CONTINUE`, `NK_IGNORE`, `NK_IGNORE_ALL` ne
déclenchent **aucune** action côté macro — l'exécution poursuit. La distinction *ignorer une fois*
vs *ignorer toutes* est donc à la charge du **handler** (c'est lui qui doit mémoriser l'état).

Coût : en debug, une comparaison plus un appel quand la condition tient déjà (à peu près gratuit dans
le cas nominal grâce à la prédiction de branche). En release, **zéro** — la macro disparaît et
l'expression n'est pas évaluée. C'est cette gratuité qui autorise à en parsemer les chemins chauds.

Cas d'usage par domaine :
- **Rendu / GPU** — vérifier qu'un handle de ressource est valide avant un *bind*, qu'un format de
  vertex correspond au shader, qu'un *render target* a la bonne taille avant un blit.
- **ECS** — l'entité existe-t-elle encore ? le composant demandé est-il bien présent dans
  l'archétype ? l'indice d'archétype est-il dans les bornes ?
- **Physique / collision** — une normale est-elle unitaire ? un *timestep* est-il strictement
  positif ? une masse est-elle non nulle avant une division ?
- **Animation** — l'os ciblé est-il dans le squelette ? le poids de *blend* est-il dans `[0, 1]` ?
- **Gameplay / IA** — l'état de la machine à états est-il dans l'énumération attendue ? le nœud du
  *behavior tree* a-t-il bien un parent ?
- **Audio** — la fréquence d'échantillonnage est-elle supportée ? l'indice de voix est-il valide
  avant d'écrire dans le bus ?
- **UI / 2D** — un layout produit-il des dimensions positives ? l'index de calque est-il valide ?

### `NKENTSEU_VERIFY` et `NKENTSEU_VERIFY_MSG`

Identiques à `ASSERT` / `ASSERT_MSG` **en debug**. La seule différence est en **release** : là où
`ASSERT` s'efface complètement, `VERIFY` conserve `((void)(expression))` — l'expression est
**évaluée**, l'échec simplement non signalé. À réserver aux expressions dont **l'effet de bord est
nécessaire au fonctionnement**, pas seulement à la vérification.

- **IO / fichiers** — `NKENTSEU_VERIFY(file.Open(path));` : en release on veut que `Open` soit
  appelé, même si on ne fait plus échouer le programme dessus.
- **GPU** — `NKENTSEU_VERIFY(CreatePipeline(&pso) == OK);` : la création doit avoir lieu en release.
- **Threading** — `NKENTSEU_VERIFY(mutex.TryLock());` quand le verrouillage doit être tenté en toute
  config.
- **Audio / réseau** — toute initialisation matérielle dont l'appel ne doit jamais être élidé.

La règle inverse vaut tout autant : ne mettez **jamais** d'effet de bord dans `NKENTSEU_ASSERT`, sous
peine de comportement divergent entre debug et release.

### `NKENTSEU_STATIC_ASSERT`

Vérification **à la compilation**, à **trois** arguments `(name, condition, message)`. Selon le
standard disponible : `static_assert(condition, #name ": " message)` en C++11+,
`_Static_assert(...)` en C11+, sinon un `typedef` de tableau de taille `-1` (donc invalide) qui force
l'erreur. **Toujours active**, quelle que soit la config — c'est une erreur de *build*, jamais un
crash. Sur le repli ancien, l'unicité de l'identifiant repose sur `__LINE__` (via
`NKENTSEU_STATIC_ASSERT_JOIN`) : deux assertions statiques sur la même ligne **collisionnent**.

- **NKContainers / NKMemory** — garantir qu'un type stocké est *trivially copyable*, qu'un POD a
  la taille/l'alignement attendus, qu'une union ne dépasse pas un budget.
- **NKMath** — verrouiller `sizeof(NkVec4) == 16`, l'alignement SIMD d'une matrice.
- **GPU / sérialisation** — figer le layout binaire d'une structure d'uniform ou d'un en-tête de
  fichier `.nkb` pour qu'il ne dérive jamais silencieusement.

### `NKENTSEU_DEBUGBREAK` et variantes

`NKENTSEU_DEBUGBREAK()` est le point d'arrêt débogueur **portable**, choisi automatiquement selon le
compilateur et l'architecture (via `NkPlatformDetect.h` / `NkCompilerDetect.h`) : `__debugbreak()`
sous MSVC, `int $0x03` sur x86/x86_64 GCC/Clang, `__builtin_trap()` sur ARM et autres,
`raise(SIGTRAP)` en repli POSIX. Point capital : il est **toujours actif, même en release** — un
break laissé par mégarde dans du code de prod **figera** l'application chez l'utilisateur. C'est
d'ailleurs `NKENTSEU_DEBUG_BREAK()` (l'alias `@deprecated` avec underscore) que la macro
d'assertion appelle réellement.

Pour un break **sûr en production**, préférez `NKENTSEU_DEBUGBREAK_IF(condition)` : *debug-only*, il
se réduit à `((void)0)` en release (la condition n'est **pas** évaluée), zéro overhead. Usages : se
poser sur une frame précise d'un bug de rendu, sur l'instant où un compteur d'objets ECS dépasse un
seuil, sur la `n`-ième itération d'une boucle de simulation — autant de breakpoints conditionnels
qu'on laisse en place sans crainte qu'ils survivent au build release.

### `NkAssertionInfo`

Agrégat POD purement descriptif, construit par initialisation `{ expression, message, file, line,
function }` (l'ordre des champs **compte**). Aucune méthode, aucun constructeur : c'est un simple
sac de pointeurs vers des littéraux que la structure **ne possède pas** — on ne libère donc **rien**.
Le handler le reçoit en `const&` et y lit tout le contexte : `message` peut être `nullptr` (ou `""`)
pour un `NKENTSEU_ASSERT` sans message, donc **testez-le** avant de l'imprimer. C'est cette fiche
qu'un handler de télémétrie sérialise, qu'un handler de log formate, qu'un handler d'éditeur affiche
dans une boîte de dialogue.

### `NkAssertAction`

L'énumération de décision (`: nk_int32`) : `NK_CONTINUE` (0) poursuit sans rien faire, `NK_BREAK` (1)
déclenche le breakpoint, `NK_ABORT` (2) appelle `::abort()`, `NK_IGNORE` (3) et `NK_IGNORE_ALL` (4)
sont des intentions « ne plus signaler ». Rappel du comportement réel : **la macro n'agit que sur
`NK_BREAK` et `NK_ABORT`** ; toutes les autres valeurs laissent l'exécution continuer. Si votre
handler renvoie `NK_IGNORE_ALL`, c'est à **lui** de retenir l'état et de répondre `NK_CONTINUE` aux
prochains appels — la macro, elle, ne mémorise rien. Ce contrat permet par exemple un handler
d'éditeur « Ignorer / Ignorer tout / Casser » sans modifier le moteur.

### `NkAssertCallback` et `NkAssertHandler`

`NkAssertCallback` est le type `NkAssertAction (*)(const NkAssertionInfo&)`. Le contrat du callback :
il **lit** l'info, **renvoie** une action, doit être **thread-safe** et **ne pas lever d'exception**.

`NkAssertHandler` est une classe purement **statique** (jamais instanciée), exportée DLL via
`NKENTSEU_CORE_API` :

- `SetCallback(cb)` — installe `cb` ; `SetCallback(nullptr)` restaure le défaut. **Non thread-safe** :
  à appeler une seule fois, à l'initialisation **mono-thread**, avant que d'autres threads ne tournent.
- `GetCallback()` — renvoie le callback courant. Idiome de test : `auto old = GetCallback();
  SetCallback(monStub); … ; SetCallback(old);` pour vérifier qu'une fonction déclenche bien
  l'assertion attendue sans casser le débogueur.
- `DefaultCallback(info)` — la politique par défaut, **sans état** donc **thread-safe** : `NK_BREAK`
  en debug, `NK_ABORT` en release. Appelable directement, ce qui sert au **chaînage** (un handler
  custom termine par `return NkAssertHandler::DefaultCallback(info);`).
- `HandleAssertion(info)` — le point d'entrée invoqué par les macros : il délègue au callback
  enregistré, retombe sur `DefaultCallback` si aucun n'est installé, et renvoie l'action que la macro
  exécutera. **Thread-safe** (l'accès au callback global est protégé).

Cas d'usage par domaine :
- **Outils / éditeur (Nogee)** — handler qui ouvre une boîte de dialogue « Casser / Ignorer / Ignorer
  tout » et mappe le choix sur `NkAssertAction`, pour ne pas tuer une session d'édition sur une
  assertion non critique.
- **Build serveur / CI** — handler qui journalise l'`NkAssertionInfo` complète puis renvoie
  `NK_ABORT` : tout échec d'invariant fait échouer le job, trace à l'appui.
- **Télémétrie / jeu livré** — handler qui agrège les échecs (fichier:ligne) et les remonte au backend
  avant de retomber sur `DefaultCallback`.
- **Tests** — sauvegarde/restauration du callback pour asserter qu'un code *déclenche* bien une
  assertion, sans interrompre la suite de tests.

---

### Exemple récapitulatif

```cpp
#include "NKCore/Assert/NkAssert.h"
#include "NKCore/Assert/NkAssertHandler.h"
using namespace nkentseu;

// 1) Invariant interne, désactivé en release.
NKENTSEU_ASSERT(ptr != nullptr);
NKENTSEU_ASSERT_MSG(size > 0, "Buffer size must be positive");

// 2) Effet de bord nécessaire : VERIFY (l'appel a lieu même en release).
NKENTSEU_VERIFY(InitializeHardware() == HW_SUCCESS);

// 3) Invariant à la compilation (3 arguments : name, condition, message).
NKENTSEU_STATIC_ASSERT(VertexLayout, sizeof(NkVertex) == 32,
                       "vertex layout doit rester compact");

// 4) Handler custom installé au démarrage (mono-thread), avec chaînage.
NkAssertAction MyHandler(const NkAssertionInfo& info) {
    NK_LOG_ERROR("[ASSERT] %s | %s (%s:%d, %s)",
                 info.expression, info.message ? info.message : "",
                 info.file, info.line, info.function);
    return NkAssertHandler::DefaultCallback(info);   // puis comportement standard
}

void BootDiagnostics() {
    NkAssertHandler::SetCallback(&MyHandler);
}

// 5) Breakpoint conditionnel sûr en release (no-op si asserts désactivés).
NKENTSEU_DEBUGBREAK_IF(frameIndex == 1234);
```

---

[← Types-vocabulaire](Vocabulary-Types.md) · [Index NKCore](README.md) · [Récap NKCore](../NKCore.md) · [Bits & limites →](Bits-Limits.md)
