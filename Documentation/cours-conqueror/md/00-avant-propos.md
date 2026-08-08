# Avant-propos

## Pourquoi cet atelier existe

Les règles de Conqueror ne sont pas connues. Plusieurs sont ouvertement
suspectes, et aucune ne se tranche par le débat. La version précédente du
document de règles s'annonçait « TOTALEMENT VERROUILLÉE » avec « 16 décisions
prises » ; plusieurs de ces décisions étaient mesurablement mauvaises, et
personne ne pouvait le savoir sans les simuler.

D'où la règle de travail du projet, qui commande tout le reste :

**`Conquerror_PREMIUM/Stage/Reference/Conqueror/REGLES_COMPLETES_v2.md:31-35`**

```
> La règle de travail de ce projet. Toute valeur numérique de ce document est
> un paramètre nommé, jamais une constante. Le moteur de règles doit exposer
> ces paramètres (§14) pour qu'ils soient modifiables sans recompilation. Une
> règle n'est pas défendue par argument : elle est réfutée ou confirmée par
> simulation de masse.
```

ConquerorLab est l'instrument qui produit cette simulation de masse. Il permet
de **tester une règle en dix minutes au lieu de trois semaines**.

## À qui s'adresse ce cours

| Vous êtes… | Lisez au minimum |
|---|---|
| **A1** — vous écrivez le moteur de règles | chapitres 0, 1, 2, 4, 5 |
| **A2** — vous écrivez l'IA adversaire | chapitres 0, 1, 3, 5 |
| **designer** — vous réglez et vous mesurez | chapitres 0, 1, 4, 5 |
| **vous reprenez le projet** | tout, dans l'ordre |

Prérequis : savoir écrire du C++ (pas du C++ avancé — les exemples de ce cours
n'utilisent ni templates, ni héritage, ni exceptions). Aucune connaissance de
Nkentseu n'est nécessaire pour les chapitres 2 et 3.

## Le kit, et sur quoi il tourne

Vous avez reçu un dossier — ou une archive `ConquerorLab-Kit.zip` — qui contient
tout : l'atelier, les en-têtes, les bibliothèques, les exemples, et ce cours en
PDF. **Il n'y a rien à installer sauf un compilateur** (voir `LISEZMOI.txt`).

```
ConquerorLab.exe          l'atelier
include/                  TOUS les en-têtes, à plat
    Conqueror/            le contrat
    NKCore/  NKMath/  NKContainers/  NKLogger/  ...
lib/                      les bibliothèques liées à vos modules
travail/rules/            <- vos moteurs de règles    (.cpp)
travail/ai/               <- vos IA                   (.cpp)
travail/boards/           <- vos grilles              (.json)
exemples/                 trois modules complets, à recopier
```

> **✅ `include/` reflète exactement ce que vous tapez**
>
> `include/NKMath/NkFunctions.h` correspond à `#include "NKMath/NkFunctions.h"`.
>
> La première version du kit recopiait l'arborescence du dépôt —
> `Kernel/Foundation/NKCore/src/NKCore/...`. `Foundation`, `System`, `src` sont
> de la plomberie de dépôt : trois niveaux qui ne veulent rien dire quand on
> écrit des règles de jeu, et qui séparent les modules de ce qu'ils incluent.
> L'atelier n'a donc qu'**un seul `-I`**.

> **⚠️ Windows uniquement, pour l'instant**
>
> Le kit livré est **Windows x64**. Ce n'est pas une limite de conception :
> l'atelier est écrit sur NKGui/NKCanvas, dont les portages Linux, Android et Web
> existent. C'est une limite de **ce qui a été construit et vérifié à ce jour** —
> et livrer un binaire non testé serait pire que ne rien livrer.
>
> Si vous travaillez sous Linux ou macOS, vous pouvez cloner le dépôt et compiler
> vous-même (voir ci-dessous). Dites-le : c'est le genre de retour qui fait
> avancer la liste.

### Compiler pour une autre plateforme

Le dépôt se compile avec `jenga`, l'outil de construction de la maison :

```sh
git clone <depot Nkentseu>
cd Nkentseu
jenga build --target ConquerorLab --config Release
```

Le binaire sort dans `Build/Bin/Release-<Plateforme>/ConquerorLab/`.

Deux choses à savoir avant de vous lancer :

- **Le compilateur de modules est PC seulement.** Sur Android et Web il n'y a pas
  de compilateur sur l'appareil : seuls les modules compilés *dans* l'application
  sont disponibles. L'atelier reste jouable et mesurable ; c'est le rechargement
  à chaud qui disparaît. Limite réelle et assumée.
- **Les chemins des bibliothèques changent.** `NkcModuleCompiler::LibDir()` vise
  `Build/Lib/<Config>-<Plateforme>` ; l'extension des modules passe de `.dll` à
  `.so` (`.dylib` sur macOS). Tout cela est déjà écrit et compilé conditionnellement
  — mais jamais **exécuté** ailleurs que sur Windows. Attendez-vous à corriger
  deux ou trois choses, et signalez-les.

### Le kit évoluera

Ce n'est pas une livraison figée. Les retours — un message d'erreur incompréhensible,
un panneau qui déborde, une fonction du contrat qui manque — donnent lieu à une
nouvelle version du kit et du cours. **Les deux nouveautés les plus récentes sont
nées exactement comme ça** : le panneau *Sortie* (§1.8) et l'ouverture de la
géométrie au C++ (chapitre 5) viennent tous deux d'une remarque, pas d'un plan.

Le fichier `VERSION.txt` du kit dit quelle version vous avez. Citez-la dans vos
retours.

## Lancer l'atelier

**`Applications/ConquerorLab/README.md — section « Lancer »`**

```
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target ConquerorLab --config Release
.\Build\Bin\Release-Windows\ConquerorLab\ConquerorLab.exe
```

Trois dossiers sont créés au premier lancement, à côté du dépôt :

```
Build/ConquerorLab/rules/    ← vos moteurs de règles   (.cpp)
Build/ConquerorLab/ai/       ← vos IA                  (.cpp)
Build/ConquerorLab/boards/   ← vos grilles             (.json)
```

Le troisième contient déjà `exemple_plateau_par_defaut.json`, écrit par
l'atelier : c'est le plateau du moteur de référence, exporté. Un format qu'on
peut lire vaut mieux qu'un format qu'on décrit.

## La première minute

À l'ouverture, **rien ne bouge**, et c'est voulu :

- le **joueur 0 est humain**, les autres sont pilotés par l'IA de référence ;
- la partie est **en pause** : c'est vous qui la lancez.

La barre du plateau vous dit ce que l'atelier attend. Elle affiche à tout
instant l'une de ces phrases :

| Ce que vous lisez | Ce que ça veut dire |
|---|---|
| *au tour du joueur humain — clique un totem* | à vous. Cliquez un de vos totems, puis une case verte |
| *en pause — Lecture ou Pas à pas* | c'est à une IA de jouer, mais la partie ne tourne pas |
| *l'IA réfléchit* | un thread calcule ; l'interface reste vivante |
| *rejeu en pause — revenir à la position vivante* | vous relisez le passé, le jeu attend |
| *partie terminée* | le décompte est affiché au centre |
| *IA introuvable ou refusée au chargement* | un vrai problème : allez au panneau Modules |

> **✅ Ce qu'il faut retenir**
>
> **Une interface immobile et muette se lit comme une panne.** C'est exactement
> ce qui s'est passé pendant le développement : le premier réglage mettait le
> joueur 0 en humain, l'atelier attendait un clic, et personne ne comprenait
> pourquoi « la simulation ne marchait pas ». La barre d'état a été ajoutée pour
> ça. **Si l'atelier semble bloqué, lisez-la avant de chercher un bug.**

## Trois gestes pour commencer

1. **Jouer un coup.** Cliquez un de vos totems (les disques bleus). Les
   destinations légales s'entourent de vert. Survolez-en une : les totems
   ennemis qu'elle **retournerait** s'entourent de rouge. C'est la lecture
   tactique centrale du jeu — « quelle surface de contact suis-je en train
   d'offrir ? ». Cliquez pour jouer.

2. **Laisser tourner.** Bouton **Lecture**. L'IA répond. Le dernier coup est
   marqué d'un anneau orange qui pulse une seconde.

3. **Passer en mode mesure.** Bouton **IA vs IA** : tous les sièges passent à
   l'IA et la partie tourne seule. C'est la configuration dans laquelle
   l'atelier répond à une question.

## Ce que ce cours ne couvre pas

- **Le jeu lui-même.** Les règles de Conqueror sont dans
  `REGLES_COMPLETES_v2.md`. Ce cours explique l'outil qui les teste.
- **NKGui, NKCanvas, le moteur.** Un module de stagiaire ne les touche pas. Si
  vous voulez modifier l'*interface* de l'atelier, c'est le cours
  *NkCanvas & NKGui* (`Documentation/cours/`) qu'il vous faut.
- **Les paliers 1 et 2** (fusion, ressources, pouvoirs, artefacts). Le moteur de
  référence livré s'arrête au palier 0. Le contrat, lui, les prévoit déjà : c'est
  pour cela que `NkcMove` porte des champs `fuseCells` et `powerId` qui restent
  à zéro aujourd'hui.
