# Cours — ConquerorLab

Comprendre et utiliser l'atelier de test de Conqueror : écrire un moteur de
règles, écrire une IA, fabriquer une grille, et faire parler les mesures.

Cours en français, pour les stagiaires **A1** (moteur de règles) et **A2** (IA
adversaire), et pour toute personne qui reprend le projet.

## Contenu

| | Chapitre | Sujet |
|---|---|---|
| 0 | [Avant-propos](md/00-avant-propos.md) | à qui s'adresse ce cours, lancer l'atelier, la première minute |
| 1 | [Le principe](md/01-le-principe.md) | trois dossiers, un contrat, qui parle à qui — et pourquoi c'est fait ainsi |
| 2 | [Écrire des règles en trois fonctions](md/02b-regles-en-trois-fonctions.md) | **la voie normale** : `Construire`, `CoupsPossibles`, `Appliquer` — un module complet en une centaine de lignes |
| 3 | [Écrire une IA](md/03-ecrire-une-ia.md) | `NkcAIVTable`, le thread worker, le budget |
| 4 | [Fabriquer une grille en JSON](md/04-les-grilles.md) | le plateau comme donnée : le format, les pièges |
| 5 | [Définir sa grille en C++](md/05-grille-en-cpp.md) | forme, voisinage et **projection écran** décidés par le module (ABI 3) |
| 6 | [Se servir de l'atelier](md/06-se-servir-de-l-atelier.md) | les panneaux, mesurer, lire un résultat sans se tromper |
| 7 | [Exemples complets](md/08-exemples-complets.md) | l'échelle : aléatoire → glouton → **négamax alpha-bêta**, règles en 3 fonctions, plateaux JSON et C++ |
| 8 | [Quand l'échafaudage ne suffit plus](md/02-ecrire-des-regles.md) | le **contrat nu** : les vingt-quatre entrées écrites à la main, le jour où `Partie` ne suffit plus |

> **L'ordre compte, et il a changé le 2026-08-16.** Le contrat nu était le
> chapitre 2 : le stagiaire apprenait vingt-quatre fonctions avant de découvrir
> qu'il n'en avait besoin que de trois. L'échafaudage passe devant ; le contrat
> nu devient le chapitre de référence qu'on lit **quand on en a besoin**.
>
> ⚠️ Le chapitre **« Regarder et habiller »** (`md/07-regarder-et-habiller.md` —
> zoom, multijoueur, totems en images, thème et langue) **n'est pas dans le
> PDF** : son code (`NkcBoardView.h`, `NkcTotemLibrary.h`, `NkcLang.h`) n'est pas
> sur cette branche. Mesure du 16/08 : 0 définition pour ces quatre symboles,
> 0 plateau multijoueur, pas de dossier `totems/`. Le livrer décrirait au
> stagiaire un atelier qu'il n'a pas. Il se réintègre avec son code.

## Fiches de travail, une par stagiaire

Le cours explique **comment** écrire un module. Ces fiches expliquent **quoi
mesurer** et **comment défendre** le résultat — c'est ce sur quoi le stagiaire
est évalué.

| Fiche | Pour qui | Ce qu'elle contient |
|---|---|---|
| [`stagiaires/A1_moteur_de_regles.md`](stagiaires/A1_moteur_de_regles.md) | A1 — moteur de règles | 11 indicateurs, protocole de mesure, comment prouver qu'une règle est bonne pour le joueur, ce qui fait fuir un joueur |
| [`stagiaires/A2_intelligence_artificielle.md`](stagiaires/A2_intelligence_artificielle.md) | A2 — IA adversaire | 11 indicateurs, paliers de difficulté réellement distincts, temps de réponse, erreurs plausibles |

Les deux insistent sur le même point : **l'atelier sait réfuter, il ne sait pas
confirmer que c'est amusant.** Ce qui relève du plaisir se vérifie en regardant
quelqu'un jouer, et les fiches imposent une section « ce que je n'ai pas pu
vérifier ».

## Formats

- **PDF** : `Cours_ConquerorLab.pdf` — la version composée, à lire.
- **Markdown** : `md/` — la même matière, lisible sur GitHub et modifiable.
- **Source** : `tex/` — LaTeX (XeLaTeX), un fichier par chapitre dans
  `tex/chapitres/`.

### Recompiler le PDF

```powershell
cd Documentation/cours-conqueror
./build.ps1          # compilation complète (2 passes XeLaTeX)
./build.ps1 -Quick   # une passe, pour un aperçu
./build.ps1 -Clean   # nettoyage
```

MiKTeX est requis (`xelatex` dans le PATH). Les polices utilisées — Segoe UI et
Consolas — sont livrées avec Windows.

## Le code dont ce cours parle

Sept exemples **complets et compilables**, du plus court au plus complet
(chapitre 8) :

| Fichier | Ce qu'il montre |
|---|---|
| `exemples/ai/IAMinimale.cpp` | la forme d'un module d'IA — tire au hasard |
| `exemples/ai/IAFacile.cpp` | **évaluer et choisir**, avec `ConquerorFacile.h` |
| `exemples/ai/IAGloutonne.cpp` | le même algorithme, au contrat nu |
| `exemples/ai/IANegamax.cpp` | négamax, alpha-bêta, approfondissement itératif, budget |
| `exemples/rules/RegleFacile.cpp` | **des règles en trois fonctions** (`ConquerorRegleFacile.h`) |
| `exemples/rules/RegleContratNu.cpp` | le même jeu, au contrat nu |
| `exemples/rules/GrilleLibre.cpp` | un plateau **circulaire** défini en C++, projection comprise |

Côté plateaux : `boards/mini_3x3.json` (9 cases, pour déboguer à l'œil) jusqu'à
`boards/hexagone_30x30_grand.json` (900 cases, pour éprouver le zoom).

Les deux modules de **référence**, plus riches, servent de dernier palier :

- `Applications/ConquerorLab/modules/rules/ConquerorRulesV2.cpp`
- `Applications/ConquerorLab/modules/ai/ConquerorAIRef.cpp`

### État vérifié des exemples — 2026-08-07

Les exemples compilent par la chaîne de l'atelier, et passent le banc d'essai en
jouant l'un contre l'autre :

```
=== Banc d'essai du contrat Conqueror ===
       regles : RegleContratNu 1.0.0 (palier 0)
       IA     : IAMinimale 1.0.0
       plateau : 25 cases, 2 joueurs
  OK   l'IA n'a jamais produit de coup illegal
  OK   la partie se termine d'elle-meme          -> 22 coups, vainqueur 0, 13/11
  OK   le journal se rejoue integralement
  OK   rejeu deterministe : empreinte identique  -> 5911215428123534347
  OK   « aucun coup legal » equivaut a « joueur bloque »
  OK   une valeur hors plage est bornee, pas acceptee telle quelle
  OK   le garde-fou max_tours coupe bien la partie
=== 14 verifications OK, 2 echecs ===
```

**Les deux échecs sont attendus** : le banc d'essai vérifie en dur le plateau du
moteur de *référence* — « hexagone 6×7 = 42 cases » et « 2 totems par joueur au
départ ». `RegleContratNu` joue sur un carré 5×5 avec un totem chacun. Les
quatorze autres vérifications, celles qui portent sur le **contrat** et non sur
la forme du plateau, passent toutes.

C'est aussi une leçon de méthode : un test qui code en dur les valeurs d'une
implémentation particulière n'est pas un test du contrat. Si vous reprenez ce
banc d'essai pour votre module, ces deux lignes-là sont à adapter.

## Documents de référence

Ce cours explique **l'outil**. Il ne remplace pas la spécification du jeu :

- `Conquerror_PREMIUM/Stage/Reference/Conqueror/REGLES_COMPLETES_v2.md` — les
  règles, source unique de vérité ;
- `Applications/ConquerorLab/include/Conqueror/ConquerorRulesABI.h` et
  `ConquerorAIABI.h` — les deux contrats, commentés ;
- `Applications/ConquerorLab/README.md` — l'état réel de l'atelier.

## Règles d'édition

Reprises du cours *NkCanvas & NKGui* :

- chaque bloc de code porte sa **provenance** (`chemin:ligne` du dépôt) ; un
  exemple sans provenance ne peut pas être vérifié, il n'entre pas ;
- ce que l'atelier **ne fait pas** se dit noir sur blanc ; aucun exemple qui
  promet ;
- les chapitres se numérotent depuis **zéro**.
