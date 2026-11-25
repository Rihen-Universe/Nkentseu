---
titre: NKPlatform : savoir sur quoi tu tournes
resume: Le module le plus bas du moteur. Il dit, dès la compilation, pour quel système, quel processeur et quel compilateur tu construis ; à l'exécution, ce que le processeur sait faire et ce que contient l'environnement.
ordre: 10
cles: plateforme système processeur compilateur octets endianness environnement macro détection
---
<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

## À quoi il sert

NKPlatform est la fondation de toutes les autres couches : il ne dépend de rien, et presque tout le moteur dépend de lui. Il répond à trois questions.

- **Pour quoi est-ce que je compile ?** Des macros te disent le système (`NKENTSEU_PLATFORM_WINDOWS`, `NKENTSEU_PLATFORM_LINUX`, `NKENTSEU_PLATFORM_ANDROID`...), l'architecture ([[NKENTSEU_ARCH_64BIT]]) et le compilateur (`NKENTSEU_COMPILER_CLANG`, `NKENTSEU_COMPILER_MSVC`...). Elles sont fixées par le préprocesseur : elles décrivent la machine **cible**, pas celle qui lance le programme.
- **Comment ranger les octets ?** [[NkIsLittleEndian]] te dit le sens de la machine, [[NkByteSwap32]] inverse les octets d'un entier, [[NkHostToNetwork32]] et [[NkNetworkToHost32]] passent à l'ordre du réseau et en reviennent.
- **Qu'est-ce que la machine sait faire, ici et maintenant ?** [[NkGetCPUFeatures]] lit le processeur à l'exécution (SSE2, AVX, NEON, taille des lignes de cache, nombre de cœurs). [[GetEnvVar]] lit une variable d'environnement du processus.

## Quand l'utiliser

- Tu écris du code qui doit se comporter différemment selon le système : teste une macro `NKENTSEU_PLATFORM_*`, jamais `_WIN32` ou `__linux__` directement. Le jour où une plateforme s'ajoute, une seule détection change.
- Tu écris ou lis un format binaire (fichier, réseau) : fixe l'ordre des octets du format, et convertis avec les fonctions de `NkEndianness.h`.
- Tu veux choisir à l'exécution le chemin le plus rapide (une boucle vectorisée, par exemple) : demande à [[NkGetCPUFeatures]], pas au compilateur.
- Tu lis un réglage passé par l'environnement (un chemin, un mode de banc) : `NkEnv.h`.
- Tu écris un module de Foundation, plus bas que NKLogger : les macros `NK_FOUNDATION_LOG_*` journalisent, et [[NkFoundationSetLogSink]] envoie les messages où tu veux.

Pour le lier, une ligne dans ton `.jenga` :

```jenga
nkentseudependson(["NKPlatform"])
```

`#include "NKPlatform/NKPlatform.h"` donne d'un coup la détection, l'ordre des octets, les macros d'export et d'inlining. `NkEnv.h` et `NkCPUFeatures.h` s'incluent à part.

## Un exemple complet

{{exemple:Foundation/NKPlatform/ordre_des_octets}}

## Compilation, puis exécution

Les macros répondent à la compilation :

{{exemple:Foundation/NKPlatform/detection}}

Le processeur répond à l'exécution :

{{exemple:Foundation/NKPlatform/processeur}}

## Les pièges

> [!attention] Compilation contre exécution
> Les macros de détection sont figées au moment de la compilation. Un programme compilé pour x86_64 et lancé sous émulation sur un processeur ARM dit toujours x86_64. Pour savoir ce que le processeur sait faire vraiment, c'est [[NkGetCPUFeatures]], à l'exécution.

> [!attention] Une variable absente rend nullptr
> [[GetEnvVar]] rend un pointeur vers la valeur, ou `nullptr` si la variable n'existe pas. Teste toujours le pointeur avant de le lire.

{{si-version:0.1}}
> [!attention] Problème connu dans 0.1.0-beta.11 : NkSet, NkGet, NkExists et NkUnset ne se lient pas
> Ces quatre fonctions sont déclarées `inline` dans `NkEnv.h`, mais leur corps est dans `NkEnv.cpp`. Un programme hors du module qui les appelle reçoit l'avertissement « inline function is not defined », puis l'édition des liens échoue (constaté en construisant l'exemple de cette page). Dans cette version, lis l'environnement avec [[GetEnvVar]]. Corrigé après cette version (voir la documentation de travail).

> [!attention] Problème connu dans 0.1.0-beta.11 : CPUFeatures::ToString ne se lie pas
> Ses deux formes sont définies `inline` dans `NkCPUFeatures.cpp` et n'entrent pas dans la bibliothèque : un programme hors du module qui les appelle ne se lie pas. Lis les champs de la structure, ou les fonctions `NkHas...` et `NkGet...`. Corrigé après cette version.
{{fin-si}}

{{si-version:dev}}
> [!attention] Une variable d'environnement vit dans le processus
> [[NkSet]] modifie l'environnement de TON processus (et des processus qu'il lancera ensuite). Il ne change rien pour la session de l'utilisateur, et tout disparaît quand ton programme s'arrête.

> [!attention] Une variable absente et une variable vide se ressemblent
> [[NkGet]] rend une chaîne vide dans les deux cas. Si la différence compte, demande d'abord à [[NkExists]].

{{exemple:Foundation/NKPlatform/environnement_ecrire}}

Le résumé lisible du processeur, pour un journal :

{{exemple:Foundation/NKPlatform/processeur_resume}}
{{fin-si}}

> [!attention] Les macros de X11
> Les en-têtes de X11 définissent des macros aux noms très courants (`None`, `Bool`, `Status`...). Si tu inclus X11 avant un en-tête de Nkentseu qui utilise ces noms, le préprocesseur les remplace. `NkX11Clean.h` les retire : inclus-le juste après X11.

> [!pourquoi] Pourquoi une couche rien que pour ça
> Tout le reste du moteur en dépend : si chaque module détectait lui-même le système, il y aurait autant de façons de se tromper que de modules. Une seule détection, en bas de la pile, et tout le monde la lit.

## Pour aller plus loin

- Le tutoriel [Ton premier programme avec NKPlatform](@/apprendre/nkplatform-premiers-pas) construit et lance deux petits programmes.
- La [référence de NKPlatform](@/reference/foundation/nkplatform) liste chaque fonction, chaque type et chaque macro, avec sa signature.
