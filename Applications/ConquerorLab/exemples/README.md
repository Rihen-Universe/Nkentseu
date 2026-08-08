# Exemples — les plus petits modules qui jouent vraiment

Ces trois fichiers sont le point de départ des stagiaires, et le support du
[cours ConquerorLab](../../../Documentation/cours-conqueror/README.md).

| Fichier | Ce qu'il fait | Chapitre du cours |
|---|---|---|
| `rules/RegleMinimale.cpp` | moteur de règles complet : carré 5×5, DUPLIQUER, transformation par adjacence, fin par blocage | 2 |
| `ai/IAMinimale.cpp` | IA complète : demande les coups légaux, en prend un au hasard | 3 |
| `rules/GrilleLibre.cpp` | plateau **circulaire** (3 anneaux, 19 cellules) : forme, voisinage **et projection écran** définis en C++, sans JSON | 5 |

## S'en servir

```
copier  rules/RegleMinimale.cpp  ->  Build/ConquerorLab/rules/mes_regles.cpp
copier  rules/GrilleLibre.cpp    ->  Build/ConquerorLab/rules/ma_grille.cpp
copier  ai/IAMinimale.cpp        ->  Build/ConquerorLab/ai/mon_ia.cpp
```

Changez le nom dans `FillFactory` avant de sauvegarder, sinon vous aurez deux
entrées identiques dans le menu et vous ne saurez pas laquelle vous testez.

L'atelier détecte, compile et charge en une seconde. En cas d'erreur, la sortie
du compilateur s'affiche dans le panneau **Modules**.

## Ce que vous avez le droit d'utiliser

Toute la pile Nkentseu **sous** l'affichage, sans rien configurer :

```cpp
#include "NKContainers/String/NkString.h"       // NkString, NkFormat
#include "NKContainers/Sequential/NkVector.h"   // NkVector, NkList, NkDeque
#include "NKContainers/Associative/NkMap.h"     // NkMap, NkSet
#include "NKMath/NkFunctions.h"                 // math::NkSqrt, NkAbs, NkClamp
#include "NKLogger/NkLog.h"                     // logger.Infof(...)
#include "NKFileSystem/NkFile.h"                // NkFile, NkDirectory
#include "NKThreading/NkThread.h"               // NkThread, NkMutex, NkAtomic
```

Rien de ce qui est **au-dessus** de NKCanvas / NKGui : un moteur de règles ne
dessine pas. C'est l'atelier qui affiche, à partir de la vue que vous exposez.

> **Afficher un état pour comprendre.** Votre module a **sa propre copie** de la
> pile (édition de liens statique), donc un `logger.Infof()` n'atteint pas la
> console de l'atelier. Une ligne suffit à le brancher :
>
> ```cpp
> #include "Conqueror/ConquerorLog.h"
> NKC_MODULE_LOGGING(rules)      // ou (ai) dans un module d'IA
> ```
>
> Ensuite `NKC_LOG_INFO("...")` — et `logger.Infof(...)` si vous incluez
> NKLogger — s'affichent dans le panneau **Sortie**. Sans atelier (banc d'essai),
> tout part sur `stderr` : rien ne disparaît en silence.

Ces trois exemples n'utilisent volontairement que `<cstring>` et `<cstdio>` :
ils doivent rester lisibles par quelqu'un qui ne connaît pas encore Nkentseu.

## Les compiler à la main

C'est exactement ce que fait l'atelier :

```sh
CLANG="C:/msys64/ucrt64/bin/clang++.exe"
R="d:/Projets/2026/Nkentseu/Nkentseu"

INC=""
for p in Applications/ConquerorLab/include \
         Kernel/Foundation/NKCore/src      Kernel/Foundation/NKPlatform/src \
         Kernel/Foundation/NKMemory/src    Kernel/Foundation/NKContainers/src \
         Kernel/Foundation/NKMath/src      Kernel/System/NKLogger/src \
         Kernel/System/NKTime/src          Kernel/System/NKStream/src \
         Kernel/System/NKFileSystem/src    Kernel/System/NKThreading/src \
         Kernel/System/NKSerialization/src Kernel/System/NKReflection/src; do
    INC="$INC -I$R/$p"
done

LIB="-L$R/Build/Lib/Release-Windows -Wl,--start-group \
     -lNKSerialization -lNKReflection -lNKLogger -lNKFileSystem -lNKStream \
     -lNKThreading -lNKTime -lNKContainers -lNKMath -lNKMemory -lNKPlatform \
     -lNKCore -Wl,--end-group"

for m in rules/RegleMinimale rules/GrilleLibre ai/IAMinimale; do
    $CLANG -shared -std=c++17 -O2 -fPIC -static $INC \
           -o "$(basename $m).dll" "$R/Applications/ConquerorLab/exemples/$m.cpp" $LIB
done
```

`-static` produit un binaire qui ne dépend que de Windows — sans lui, le module
réclame `libstdc++-6.dll` et l'atelier ne peut pas le charger. En contrepartie,
l'éditeur de liens émet 33 avertissements `duplicate section` : ils sont
constants et sans action possible, et l'atelier les retire du journal **quand la
compilation réussit** (jamais quand elle échoue).

## État vérifié — 2026-08-07

Les trois modules compilent, se chargent, et jouent une partie complète sous le
banc d'essai (`tests/NkcAbiHarness.cpp`) :

```
       plateau : 25 cases, 2 joueurs
  OK   l'IA n'a jamais produit de coup illegal
  OK   la partie se termine d'elle-meme          -> 22 coups, vainqueur 0, 13/11
  OK   rejeu deterministe : empreinte identique  -> 5911215428123534347
  OK   « aucun coup legal » equivaut a « joueur bloque »
=== 14 verifications OK, 2 echecs ===
```

**Les deux échecs sont attendus** : le banc d'essai vérifie en dur le plateau du
moteur de *référence* (hexagone 6×7, 42 cases, 2 totems par joueur). Ces exemples
jouent sur un carré 5×5 avec un totem chacun. Tout ce qui porte sur le
**contrat** passe.

`GrilleLibre` joue lui aussi une partie complète (17 coups, empreinte
`15623799979863270834`, rejeu identique) : **13/16**, les trois échecs étant les
mêmes vérifications codées en dur, plus `portee_duplication` qu'il n'expose pas.

## Ce que ces exemples ne font pas, volontairement

- **`RegleMinimale` refuse tout chargement de plateau** (`LoadBoardJson` renvoie
  `0`) : sa grille est figée dans le code. C'est précisément ce que
  `REGLES_COMPLETES_v2.md` §4 interdit à un vrai moteur — voir
  `modules/rules/ConquerorRulesV2.cpp` pour une implémentation complète. Renoncement
  assumé pour un exemple ; l'atelier le signale proprement au lieu de planter.
- Ni fusion, ni pouvoirs, ni artefacts : c'est le palier 0, et même pas en
  entier.
- `IAMinimale` ne distingue aucun palier de difficulté et ignore son budget —
  elle répond instantanément, la question ne se pose pas encore.
