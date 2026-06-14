# Export, visibilité et inline

> Couche **Foundation** · NKPlatform · Décider **ce qui sort** d'une bibliothèque (les
> macros d'export `NKENTSEU_PLATFORM_API`, `NKENTSEU_CLASS_EXPORT`, la liaison C) et **comment
> le compilateur traite chaque fonction** (les spécificateurs d'inline et les *hints*
> d'optimisation `NKENTSEU_FORCE_INLINE`, `NKENTSEU_LIKELY`…).

Deux questions reviennent dès qu'on dépasse le simple `.exe` monolithique. La première est une
question de **frontière** : quand le moteur est compilé en **DLL** (ou `.so`, `.dylib`), il faut
dire au compilateur *quels* symboles franchissent la frontière de la bibliothèque — sous Windows
on les **exporte** côté lib et on les **importe** côté client, et la même déclaration de classe
doit porter `__declspec(dllexport)` à la compilation interne, `__declspec(dllimport)` chez le
consommateur. La seconde est une question de **performance** : sur un chemin chaud (un `Dot` de
NKMath appelé un million de fois par frame), on veut **forcer** l'inlining ; sur un gestionnaire
d'erreur, au contraire, on veut l'**interdire** pour ne pas polluer le cache d'instructions.

Le piège, c'est que la réponse exacte **dépend du compilateur et de la plateforme** : `__declspec`
sous MSVC/MinGW, `__attribute__((visibility))` sous GCC/Clang, `__forceinline` ici,
`always_inline` là. NKPlatform fait ce travail une fois pour toutes et expose des macros
**portables** : vous écrivez `NKENTSEU_PLATFORM_API` ou `NKENTSEU_FORCE_INLINE`, et la bonne
incantation est choisie selon la cible détectée. Ce n'est **pas** une couche de runtime — il n'y a
ici **aucune fonction, struct, enum ou constante** : ce sont **uniquement des macros**, résolues à
la compilation.

- **Aucun namespace** (macros du préprocesseur, donc globales).
- **Headers** : `#include "NKPlatform/NkPlatformExport.h"` (export/visibilité) et
  `#include "NKPlatform/NkPlatformInline.h"` (inline/optimisation).
- **Ordre d'include important** : les trois macros `NKENTSEU_API_*INLINE*` combinent export **et**
  inline, donc `NkPlatformExport.h` doit être inclus **avant** `NkPlatformInline.h` (ce dernier ne
  l'inclut pas lui-même).

---

## Exporter un symbole : `NKENTSEU_PLATFORM_API`

C'est **la** macro à connaître. On la pose devant chaque fonction, classe ou variable globale
qu'une bibliothèque veut rendre **publique** à travers sa frontière, et elle s'adapte toute seule
au **mode de build** courant. Trois modes existent, sélectionnés par des interrupteurs que **le
client définit avant l'include** (ce ne sont pas des macros fournies par le header) :

- En définissant `NKENTSEU_PLATFORM_BUILD_SHARED_LIB`, on est **en train de compiler la lib
  partagée** : `NKENTSEU_PLATFORM_API` devient l'export (`__declspec(dllexport)` ou
  `visibility("default")`).
- En définissant `NKENTSEU_PLATFORM_STATIC_LIB`, on est en **statique** : la macro est **vide** —
  pas de décoration, le symbole est lié normalement.
- En ne définissant **rien** (cas par défaut côté client d'une DLL) : la macro devient l'**import**
  (`__declspec(dllimport)`).

```cpp
#include "NKPlatform/NkPlatformExport.h"

NKENTSEU_PLATFORM_API void NkInitPlatform();      // fonction publique de la lib
class NKENTSEU_CLASS_EXPORT NkPlatformInfo { … };  // classe complète exportée
```

Ce n'est **pas** une macro qu'on choisit « à la main » : la beauté du système est qu'une **seule**
déclaration sert à la fois la lib (export) et le client (import), le bon comportement étant déduit
des interrupteurs de build. Le header **interdit** d'ailleurs de définir `BUILD_SHARED_LIB` **et**
`STATIC_LIB` ensemble : les deux modes sont mutuellement exclusifs, et leur cohabitation déclenche
une `#error` à la compilation. Si vous activez `NKENTSEU_EXPORT_DEBUG`, le header émet en plus des
`#pragma message` décrivant la configuration retenue — utile pour vérifier qu'un build est bien en
export et non en import par mégarde.

> **En résumé.** `NKENTSEU_PLATFORM_API` décore tout symbole public de la lib et choisit
> **export / vide / import** selon `BUILD_SHARED_LIB` / `STATIC_LIB` / rien. `NKENTSEU_CLASS_EXPORT`
> en est l'alias pour les classes. Définir `BUILD_SHARED_LIB` **uniquement** en interne lors de la
> compilation de la lib ; les deux modes sont exclusifs (`#error` sinon).

---

## Cacher un symbole et franchir la barrière C

L'inverse de l'export existe aussi. `NKENTSEU_API_LOCAL` marque un symbole comme **privé à la
bibliothèque partagée** : il n'apparaît pas dans la table d'export, ce qui réduit la surface
publique, accélère le chargement et autorise des optimisations de l'éditeur de liens. Attention à
sa portée : elle ne s'appuie que sur la *visibility* GCC/Clang (`visibility("hidden")`) — sous MSVC
pur, c'est un **no-op** (rien n'est exporté par défaut là-bas, donc « cacher » n'a pas de sens).

Reste la question du **C**. Quand un symbole C++ doit être appelé depuis du C (ou via `dlsym` /
`GetProcAddress` sans décoration de nom), il faut désactiver le *name mangling* avec
`extern "C"`. NKPlatform en donne trois formes qui se neutralisent proprement en C pur :

- `NKENTSEU_EXTERN_C` — préfixe une déclaration unique (`extern "C"` en C++, vide en C).
- `NKENTSEU_EXTERN_C_BEGIN` / `NKENTSEU_EXTERN_C_END` — encadrent **un bloc** de déclarations
  (`extern "C" {` … `}` en C++, vides en C).

```cpp
NKENTSEU_EXTERN_C_BEGIN
NKENTSEU_PLATFORM_API int  nk_platform_version(void);
NKENTSEU_PLATFORM_API void nk_platform_shutdown(void);
NKENTSEU_EXTERN_C_END
```

Le **piège** à garder en tête : `BEGIN`/`END` ne sont **pas** symétriquement neutres en C++ —
`BEGIN` **ouvre** une accolade qu'`END` doit **fermer**. On les emploie donc **toujours par paire**,
jamais l'un sans l'autre.

> **En résumé.** `NKENTSEU_API_LOCAL` cache un symbole (effectif GCC/Clang seulement, no-op MSVC).
> `NKENTSEU_EXTERN_C` (déclaration unique) et le couple `NKENTSEU_EXTERN_C_BEGIN` /
> `NKENTSEU_EXTERN_C_END` (bloc, **toujours par paire**) désactivent le *mangling* pour exposer une
> ABI C stable.

---

## Inliner — ou pas : `NKENTSEU_INLINE`, `FORCE_INLINE`, `NO_INLINE`

Le mot-clé `inline` du langage n'est qu'un **conseil** que le compilateur est libre d'ignorer, et
sa forme exacte varie (`inline` en C++, `__inline` sous MSVC en C, `__inline__` chez GCC/Clang en C
ancien). `NKENTSEU_INLINE` masque ces différences et donne un `inline` **portable** — c'est le
choix neutre, par défaut, pour les petites fonctions de header.

Quand il faut **garantir** l'inlining sur un chemin chaud, on monte d'un cran avec
`NKENTSEU_FORCE_INLINE` : `__forceinline` sous MSVC/Intel/ARMCC, `inline __attribute__((always_inline))`
sous GCC/Clang. C'est l'outil des accesseurs minuscules et des opérateurs mathématiques appelés en
boucle serrée, là où le coût d'un appel de fonction est du même ordre que le travail utile. À
l'opposé, `NKENTSEU_NO_INLINE` (`__declspec(noinline)` MSVC, `__attribute__((noinline))` GCC/Clang)
**empêche** l'inlining : on s'en sert pour **réduire la taille du code** (une fonction grosse et
rare ne devrait pas être recopiée partout), pour **garder un symbole débogable** (un point d'arrêt
qui ne se volatilise pas dans l'inliner), ou pour **casser une récursion** que le compilateur
voudrait dérouler.

```cpp
#include "NKPlatform/NkPlatformInline.h"

NKENTSEU_FORCE_INLINE float Dot(NkVec3 a, NkVec3 b) {   // chemin chaud → inline garanti
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

NKENTSEU_NO_INLINE void ReportFatal(const char* msg);    // rare → hors-ligne, débogable
```

Ce ne sont **pas** des garanties absolues : sur un compilateur non reconnu, `NKENTSEU_INLINE` peut
retomber sur du vide (avec le risque de définitions multiples) et `NKENTSEU_FORCE_INLINE` se replie
sur `NKENTSEU_INLINE`. Le sens est toujours **préservé** ; seule l'agressivité varie.

> **En résumé.** `NKENTSEU_INLINE` = `inline` portable (par défaut). `NKENTSEU_FORCE_INLINE` =
> inline **garanti** (chemins chauds, accesseurs). `NKENTSEU_NO_INLINE` = inline **interdit** (taille
> de code, débogage, récursion). Aucun n'est une garantie dure : sur compilateur inconnu ils se
> dégradent sans changer la sémantique.

---

## Guider l'optimiseur : *hints* de fonction et de branche

Au-delà de l'inlining, NKPlatform offre des **annotations purement consultatives** qui aident
l'optimiseur sans changer le résultat. Côté **branches**, `NKENTSEU_LIKELY(cond)` et
`NKENTSEU_UNLIKELY(cond)` disent au compilateur quel chemin est **probable** : sous GCC/Clang ils
se traduisent par `__builtin_expect`, qui réorganise le code généré pour que le cas fréquent reste
linéaire (bon pour la prédiction de branche et le cache d'instructions) ; ailleurs ils s'évaluent
simplement à `(cond)`. On les emploie typiquement pour signaler qu'une vérification d'erreur est
**improbable** (`if (NKENTSEU_UNLIKELY(ptr == nullptr)) …`).

Côté **fonctions entières**, plusieurs attributs renseignent l'optimiseur sur la **nature** de la
fonction. `NKENTSEU_PURE_FUNCTION` annonce une fonction **sans effet de bord** (lecture mémoire
autorisée), `NKENTSEU_CONST_FUNCTION` une fonction qui ne dépend que de ses paramètres (aucune
lecture globale, plus restrictif, autorise l'élimination de sous-expressions communes).
`NKENTSEU_HOT_FUNCTION` et `NKENTSEU_COLD_FUNCTION` indiquent qu'une fonction est respectivement
**très** ou **peu** sollicitée, pour que l'éditeur de liens regroupe le code chaud et exile le
froid. Tous ces attributs sont des **no-op hors GCC/Clang** : on ne **dépend jamais** d'eux pour la
correction, seulement pour la vitesse.

Deux annotations plus rares complètent la panoplie. `NKENTSEU_NORETURN` marque une fonction qui **ne
revient jamais** (terminaison du programme, lancement d'exception) — `[[noreturn]]` en C++11+ — et
permet à l'optimiseur d'élaguer le code situé après l'appel. `NKENTSEU_RESTRICT_RETURN` promet que
le pointeur **renvoyé** par une fonction n'a **aucun alias** (idéal pour les *factories* et
allocateurs), et `NKENTSEU_RESTRICT_PARAM(ptr)` fait la même promesse pour un **paramètre**
pointeur, ce qui débloque la vectorisation quand le compilateur sait que deux tampons ne se
chevauchent pas.

```cpp
NKENTSEU_NORETURN void NkAbort(const char* reason);

NKENTSEU_RESTRICT_RETURN void* NkAllocFast(size_t n);

void Blend(const float* NKENTSEU_RESTRICT_PARAM(src),
                 float* NKENTSEU_RESTRICT_PARAM(dst), size_t n);
```

Attention à deux **subtilités d'ordre** dans la résolution. `NKENTSEU_RESTRICT_RETURN` teste la
branche **C++11 en premier** : en C++ on obtient donc `__restrict__` (extension GCC/Clang) **même
sous MSVC C++**, où `__restrict__` n'est pas le mot-clé natif — un piège possible. Et
`NKENTSEU_RESTRICT_PARAM` n'a **pas** de branche C++ dédiée : en C++ non-MSVC, il se réduit à un
simple `ptr` (passthrough), il ne contraint l'aliasing qu'en C99+ ou sous MSVC.

> **En résumé.** `NKENTSEU_LIKELY`/`UNLIKELY` orientent la prédiction de branche (effectifs
> GCC/Clang). `PURE`/`CONST`/`HOT`/`COLD_FUNCTION` renseignent l'optimiseur (no-op ailleurs, jamais
> requis pour la correction). `NORETURN` élague l'après-appel ; `RESTRICT_RETURN`/`RESTRICT_PARAM`
> promettent le non-aliasing (gare à l'ordre : `__restrict__` en C++ même sous MSVC, passthrough
> pour le param C++ non-MSVC).

---

## Aperçu de l'API

La liste de **tous** les éléments publics (ce sont exclusivement des **macros**). Chacun est détaillé
dans la « Référence complète » qui suit. Les macros internes (préfixées « interne ») ne doivent pas
être utilisées dans le code client.

### `NkPlatformExport.h` — export & visibilité

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Build (entrées) | `NKENTSEU_PLATFORM_BUILD_SHARED_LIB` | À définir **avant** l'include : compiler la lib en partagé (mode export). |
| Build (entrées) | `NKENTSEU_PLATFORM_STATIC_LIB` | À définir **avant** l'include : mode statique (décoration vide). |
| Build (entrées) | `NKENTSEU_EXPORT_DEBUG` | Si défini : `#pragma message` décrivant la config d'export. |
| Export public | `NKENTSEU_PLATFORM_API` | **Macro principale** : décore les symboles publics (export / vide / import selon le mode). |
| Export public | `NKENTSEU_CLASS_EXPORT` | Alias de `NKENTSEU_PLATFORM_API` pour les déclarations de classe complète. |
| Export public | `NKENTSEU_API_LOCAL` | Rendre un symbole privé/non exporté (GCC/Clang ; no-op MSVC). |
| Liaison C | `NKENTSEU_EXTERN_C` | `extern "C"` (C++) / vide (C) sur une déclaration unique. |
| Liaison C | `NKENTSEU_EXTERN_C_BEGIN` / `NKENTSEU_EXTERN_C_END` | Encadrent un bloc `extern "C" { … }` (**toujours par paire**). |
| Interne | `NKENTSEU_EXPORT_HAS_DECLSPEC` | `1` si `__declspec` supporté (Windows + MSVC/MinGW). |
| Interne | `NKENTSEU_EXPORT_HAS_VISIBILITY` | `1` si `__attribute__((visibility))` supporté (GCC/Clang). |
| Interne | `NKENTSEU_PLATFORM_API_EXPORT` / `_API_IMPORT` | Brique export/import bas niveau (**ne pas utiliser directement**). |

### `NkPlatformInline.h` — inline & optimisation

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Inline | `NKENTSEU_INLINE` | `inline` portable (par défaut). |
| Inline | `NKENTSEU_FORCE_INLINE` | Inlining **forcé** (chemins chauds). |
| Inline | `NKENTSEU_NO_INLINE` | Inlining **interdit** (taille, debug, récursion). |
| Inline+export | `NKENTSEU_API_INLINE` | `NKENTSEU_PLATFORM_API` + `NKENTSEU_INLINE` (inline exporté). |
| Inline+export | `NKENTSEU_API_FORCE_INLINE` | `NKENTSEU_PLATFORM_API` + `NKENTSEU_FORCE_INLINE`. |
| Inline+export | `NKENTSEU_API_NO_INLINE` | `NKENTSEU_PLATFORM_API` + `NKENTSEU_NO_INLINE`. |
| Hint fonction | `NKENTSEU_PURE_FUNCTION` | Sans effet de bord (lecture mémoire OK) ; GCC/Clang. |
| Hint fonction | `NKENTSEU_CONST_FUNCTION` | Dépend des seuls paramètres (plus restrictif, CSE) ; GCC/Clang. |
| Hint fonction | `NKENTSEU_HOT_FUNCTION` / `NKENTSEU_COLD_FUNCTION` | Fonction très / peu sollicitée ; GCC/Clang. |
| Flux | `NKENTSEU_NORETURN` | Fonction qui ne revient pas (`[[noreturn]]`…). |
| Aliasing | `NKENTSEU_RESTRICT_RETURN` | Pointeur de retour non-aliasant (factory/alloc). |
| Aliasing | `NKENTSEU_RESTRICT_PARAM(ptr)` | Paramètre pointeur non-aliasant (vectorisation). |
| Branche | `NKENTSEU_LIKELY(cond)` / `NKENTSEU_UNLIKELY(cond)` | Hint de probabilité de branche (`__builtin_expect` GCC/Clang). |
| Interne | `NKENTSEU_INLINE_COMPILER_*` / `_COMPILER_NAME` | Détection du compilateur (exactement un défini) + nom en chaîne. |

---

## Référence complète

Chaque macro est reprise ici en détail. Les triviales sont décrites brièvement ; celles qui pèsent
sur l'**ABI** (export DLL, liaison C) ou sur la **performance des boucles chaudes** (force-inline,
*hints* de branche, aliasing) le sont **à fond**, avec leurs usages dans les différents domaines du
temps réel.

### Mécanique de détection (briques internes)

Avant toute décision, les headers détectent l'environnement. Côté export,
`NKENTSEU_EXPORT_HAS_DECLSPEC` vaut `1` sur plateforme Windows/Cygwin (`_WIN32` || `_WIN64` ||
`__CYGWIN__`) **et** compilateur MSVC ou MinGW — c'est le terrain de `__declspec(dllexport/dllimport)`.
`NKENTSEU_EXPORT_HAS_VISIBILITY` vaut `1` sous `__GNUC__` ou `__clang__` (Clang sous Windows
compris) — le terrain de `__attribute__((visibility))`. À partir de ces deux drapeaux, les briques
internes `NKENTSEU_PLATFORM_API_EXPORT` et `NKENTSEU_PLATFORM_API_IMPORT` choisissent `dllexport`/
`dllimport`, sinon `visibility("default")`, sinon vide. **On ne les utilise jamais directement** :
on passe toujours par `NKENTSEU_PLATFORM_API`.

Côté inline, le header identifie le compilateur dans un ordre **précis** — MSVC → Clang → GCC →
Intel → ARMCC → IAR → Unknown — en définissant exactement **un** token `NKENTSEU_INLINE_COMPILER_*`
plus `NKENTSEU_INLINE_COMPILER_NAME` (la chaîne `"MSVC"`, `"Clang"`, `"GCC"`…). L'ordre n'est pas
anodin : **Clang est testé avant GCC** parce qu'il définit aussi `__GNUC__` — sans ce soin, un Clang
serait faussement pris pour un GCC.

### `NKENTSEU_PLATFORM_API` et `NKENTSEU_CLASS_EXPORT` à fond

C'est le cœur de l'ABI du moteur. La macro sélectionne sa valeur **au moment de l'include**, d'après
les interrupteurs de build, et cette valeur se propage sur **toute** déclaration publique. Le même
fichier d'en-tête sert ainsi trois consommateurs sans modification : la lib partagée (qui définit
`BUILD_SHARED_LIB` → export), le client DLL (qui ne définit rien → import), le client statique (qui
définit `STATIC_LIB` → vide). `NKENTSEU_CLASS_EXPORT` est l'alias dédié aux **classes complètes**
(le compilateur exporte alors toutes les méthodes membres). Le garde-fou `#error` (les deux modes
définis ensemble) évite l'erreur classique d'un projet mal configuré.

Par domaine, la frontière d'export structure tout le moteur :

- **Rendu** — un backend graphique livré en plugin DLL (Vulkan, DX12…) n'exporte que sa **fabrique**
  et son interface ; tout le détail d'implémentation reste caché derrière `NKENTSEU_API_LOCAL`.
- **GPU / RHI** — les points d'entrée d'un module RHI chargé dynamiquement doivent être exportés pour
  que le moteur les résolve.
- **Audio / UI / IO** — chaque module compilé séparément expose sa surface publique via la macro API
  de **sa** couche (le motif est identique, seul le préfixe change).
- **ECS / gameplay** — exporter les types de composants et de systèmes partagés entre la lib moteur
  et le code de jeu lié dynamiquement.

### `NKENTSEU_API_LOCAL` et la liaison C à fond

`NKENTSEU_API_LOCAL` est l'outil de **minimisation de la surface** : sur les chaînes GCC/Clang
construites avec `-fvisibility=hidden`, on rend exportés **uniquement** les symboles marqués `…API`,
et l'on confine tout le reste avec `…API_LOCAL` (ou par défaut). Bénéfices concrets : table d'export
plus petite, chargement plus rapide, inlining inter-procédural plus libre. Sous MSVC pur, où rien
n'est exporté sans décoration, c'est un **no-op** assumé.

La liaison C (`NKENTSEU_EXTERN_C*`) sert chaque fois qu'une **ABI stable** est requise :

- **GPU / shaders** — exposer des points d'entrée chargés par `GetProcAddress`/`dlsym` (le nom doit
  rester non-décoré pour être retrouvé).
- **IO / réseau** — bibliothèques de bas niveau partagées avec du code C ou d'autres langages via FFI.
- **Plugins** — une frontière C entre l'hôte et le plugin survit aux différences d'ABI C++ entre
  compilateurs. Rappel du piège : `BEGIN`/`END` **par paire** obligatoire.

### Les spécificateurs d'inline à fond

Le trio `INLINE` / `FORCE_INLINE` / `NO_INLINE` couvre tout le spectre. Détail des replis :
`NKENTSEU_INLINE` donne `inline` en C++ et C99+, `__inline` sous MSVC en C, `__inline__` sous
GCC/Clang en C ancien, et **vide** en dernier recours (avec le risque, signalé dans le header, de
définitions multiples). `NKENTSEU_FORCE_INLINE` mappe `__forceinline` (MSVC/Intel/ARMCC),
`inline __attribute__((always_inline))` (GCC/Clang), `_Pragma("inline=forced")` (IAR), et se **replie
sur `NKENTSEU_INLINE`** si le compilateur est inconnu. `NKENTSEU_NO_INLINE` mappe
`__declspec(noinline)` (MSVC), `__attribute__((noinline))` (GCC/Clang/Intel/ARMCC),
`_Pragma("inline=never")` (IAR), vide sinon.

Par domaine :

- **Rendu / math** — `FORCE_INLINE` sur les opérateurs vectoriels et matriciels, les accesseurs de
  *swizzle*, le code SIMD : ces fonctions minuscules ne doivent jamais coûter un appel.
- **ECS** — `FORCE_INLINE` sur les accès aux composants dans les boucles de système (data-oriented).
- **Physique / collision** — `FORCE_INLINE` sur les tests de recouvrement appelés des milliers de
  fois par frame.
- **IO / outils** — `NO_INLINE` sur les chemins de chargement de gros fichiers ou de sérialisation,
  rarement exécutés, pour ne pas gonfler le binaire et garder des symboles débogables.

### Les macros combinées `NKENTSEU_API_*INLINE*` à fond

Une fonction inline **exportée** depuis une DLL est un cas subtil : il faut à la fois la décoration
d'export et le spécificateur d'inline. Les trois macros le font d'un coup —
`NKENTSEU_API_INLINE` = `NKENTSEU_PLATFORM_API NKENTSEU_INLINE`,
`NKENTSEU_API_FORCE_INLINE` = `NKENTSEU_PLATFORM_API NKENTSEU_FORCE_INLINE`,
`NKENTSEU_API_NO_INLINE` = `NKENTSEU_PLATFORM_API NKENTSEU_NO_INLINE`. Usages : `API_INLINE` pour une
fonction inline visible depuis la DLL, `API_FORCE_INLINE` pour un getter critique de l'API publique
(le header avertit que l'inlining **forcé d'un symbole exporté** peut poser des soucis de linkage
selon la chaîne — à employer en connaissance de cause), `API_NO_INLINE` pour une fonction publique
qu'on garde **hors-ligne** afin de conserver un symbole débogable côté consommateur.

**Pré-requis dur** : ces trois macros utilisent `NKENTSEU_PLATFORM_API`, et `NkPlatformInline.h` **ne
l'inclut pas**. Il faut donc inclure `NkPlatformExport.h` **avant**, sans quoi `NKENTSEU_PLATFORM_API`
est indéfini et la compilation échoue.

### Les *hints* d'optimisation à fond

Tous **consultatifs**, sans incidence sur le résultat — uniquement sur la vitesse. `PURE_FUNCTION`
(pas d'effet de bord, lecture mémoire permise) et `CONST_FUNCTION` (ne lit que ses paramètres, plus
strict, autorise l'élimination de sous-expressions communes) ne sont posés que sur des fonctions
**réellement** sans état caché — sur math/physique notamment. `HOT_FUNCTION`/`COLD_FUNCTION` guident
le placement du code : *hot* pour la boucle de rendu ou de simulation, *cold* pour les gestionnaires
d'erreur et l'initialisation rare. Tous **no-op hors GCC/Clang** : jamais requis pour la correction.

`NKENTSEU_NORETURN` (`[[noreturn]]` en C++11+, sinon `__declspec(noreturn)` / `__attribute__((noreturn))`)
marque les fonctions de **terminaison** — un `NkAbort`, le lancement d'une exception fatale — et permet
d'élaguer le code mort après l'appel. Côté aliasing, `RESTRICT_RETURN` est l'allié des **allocateurs**
et **factories** (NKMemory, *pools* d'objets, *factories* de composants ECS) : promettre que le
pointeur rendu n'a pas d'alias débride les optimisations sur le bloc fraîchement obtenu.
`RESTRICT_PARAM(ptr)` est l'allié de la **vectorisation** : sur un mélange de buffers (mixage audio,
*blit* d'images, transformation de tableaux de sommets), garantir que `src` et `dst` ne se chevauchent
pas autorise le compilateur à générer des boucles SIMD sans relecture défensive. Rappel des pièges
d'ordre : `RESTRICT_RETURN` donne `__restrict__` en C++ **même sous MSVC** (où ce n'est pas natif), et
`RESTRICT_PARAM` se réduit à un simple `ptr` en C++ non-MSVC.

### `NKENTSEU_LIKELY` / `NKENTSEU_UNLIKELY` à fond

Ces deux *hints* de branche valent `(__builtin_expect(!!(cond), 1|0))` sous GCC/Clang et `(cond)`
ailleurs — la **sémantique est toujours préservée**, seul le code généré change. On les pose là où
l'on **sait** quel chemin domine :

- **Boucles chaudes (rendu, ECS, physique)** — `UNLIKELY` sur les sorties anticipées et les
  vérifications de garde, pour que le corps fréquent reste sans saut.
- **Gestion d'erreur** — `if (NKENTSEU_UNLIKELY(result != OK))` : le chemin nominal reste linéaire,
  le traitement d'erreur est repoussé hors du flot chaud.
- **Validation d'entrée (IO / réseau / parsing)** — `LIKELY` sur le cas « donnée valide », `UNLIKELY`
  sur les cas dégradés, qui sont rares mais nombreux à coder.

---

### Exemple récapitulatif

```cpp
// ---- En-tête public d'une lib partagée ---------------------------------
#include "NKPlatform/NkPlatformExport.h"   // export D'ABORD
#include "NKPlatform/NkPlatformInline.h"   // inline ENSUITE (dépend du précédent)

// API C stable, chargeable par GetProcAddress / dlsym
NKENTSEU_EXTERN_C_BEGIN
NKENTSEU_PLATFORM_API int  nk_platform_version(void);
NKENTSEU_EXTERN_C_END

// Classe complète exportée
class NKENTSEU_CLASS_EXPORT NkPlatformInfo {
public:
    // getter critique : exporté ET inline forcé
    NKENTSEU_API_FORCE_INLINE int CoreCount() const { return mCores; }
private:
    int mCores;
};

// Terminaison : ne revient jamais
NKENTSEU_PLATFORM_API NKENTSEU_NORETURN void NkAbort(const char* reason);

// ---- Implémentation interne (non exportée) -----------------------------
// Math chaud : inline forcé, sans effet de bord
NKENTSEU_FORCE_INLINE NKENTSEU_CONST_FUNCTION
float Dot(NkVec3 a, NkVec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

// Mélange de deux buffers : non-aliasing → vectorisation
void MixAudio(const float* NKENTSEU_RESTRICT_PARAM(src),
                    float* NKENTSEU_RESTRICT_PARAM(dst), int n) {
    for (int i = 0; i < n; ++i) {
        dst[i] += src[i];
        if (NKENTSEU_UNLIKELY(dst[i] > 1.f)) dst[i] = 1.f;  // clip rare
    }
}

// Symbole caché de la lib partagée (GCC/Clang)
NKENTSEU_API_LOCAL void InternalReset();
```

---

[← Index NKPlatform](README.md) · [Récap NKPlatform](../NKPlatform.md) · [Détection plateforme →](Detection.md)
