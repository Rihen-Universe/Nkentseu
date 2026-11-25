---
titre: Ton premier programme avec NKPlatform
resume: En cinq étapes, tu déclares un programme console dans Jenga, tu lis l'ordre des octets de ta machine, tu passes un entier à l'ordre du réseau, puis tu lis une variable d'environnement.
ordre: 10
cles: tutoriel premiers pas jenga console octets environnement
---
<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

Tout le code de ce tutoriel est réel : ce sont deux programmes de `Docs/exemples/`, compilés et exécutés à chaque construction de cette documentation. Ce que tu lis sous « Sortie » est ce qu'ils ont affiché.

## Étape 1 : déclarer le programme

Un programme console de Nkentseu se déclare dans un fichier `.jenga`. Il dit quel module il utilise : ici NKPlatform, plus NKContainers pour afficher du texte avec `NkPrintln`.

```jenga
with project("MonPremierProgramme"):
    consoleapp()
    language("C++")
    cppdialect("C++20")
    files(["main.cpp"])
    nkentseudependson(["NKPlatform", "NKContainers", "NKMemory", "NKCore"])
```

C'est l'essentiel. Le fichier complet qui construit les exemples de cette page, avec le choix de la chaîne de compilation par système, est `Docs/exemples/NkDocExemples.jenga`.

## Étape 2 : connaître le sens de ta machine

Une machine range les octets d'un entier dans un sens : du plus petit au plus grand (petit-boutiste, presque tous les processeurs actuels), ou l'inverse. [[NkIsLittleEndian]] te le dit, sans rien mesurer à l'exécution :

{{extrait:Foundation/NKPlatform/ordre_des_octets#sens}}

## Étape 3 : inverser les octets

[[NkByteSwap32]] inverse les quatre octets d'un entier de 32 bits. C'est l'outil de base pour lire un fichier écrit par une machine de l'autre sens.

{{extrait:Foundation/NKPlatform/ordre_des_octets#echange}}

## Étape 4 : parler au réseau

Sur le réseau, l'ordre est gros-boutiste. Avant d'envoyer, [[NkHostToNetwork32]] ; en recevant, [[NkNetworkToHost32]]. Sur une machine gros-boutiste, ces deux fonctions ne font rien : ton code reste le même partout.

{{extrait:Foundation/NKPlatform/ordre_des_octets#reseau}}

Le programme complet, avec ce qu'il affiche :

{{exemple:Foundation/NKPlatform/ordre_des_octets}}

## Étape 5 : l'environnement du processus

Un programme reçoit des variables d'environnement de celui qui le lance. [[GetEnvVar]] en lit une ; une variable absente rend `nullptr`, pas une chaîne vide :

{{exemple:Foundation/NKPlatform/variables_environnement}}

> [!attention] Teste le pointeur
> Lire un `nullptr` plante ton programme. La sortie ci-dessus montre les deux cas : une variable présente, une absente.

## Et maintenant

- Lis [NKPlatform : savoir sur quoi tu tournes](@/manuel/foundation/nkplatform) pour les pièges de la détection.
- Cherche une fonction dans la [référence de NKPlatform](@/reference/foundation/nkplatform) (raccourci : `Ctrl` `K`).
