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
| 2 | [Écrire un moteur de règles](md/02-ecrire-des-regles.md) | `NkcRulesVTable` de bout en bout, sur un exemple qui compile |
| 3 | [Écrire une IA](md/03-ecrire-une-ia.md) | `NkcAIVTable`, le thread worker, le budget |
| 4 | [Fabriquer une grille en JSON](md/04-les-grilles.md) | le plateau comme donnée : le format, les pièges |
| 5 | [Définir sa grille en C++](md/05-grille-en-cpp.md) | forme, voisinage et **projection écran** décidés par le module (ABI 3) |
| 6 | [Se servir de l'atelier](md/06-se-servir-de-l-atelier.md) | les six panneaux, mesurer, lire un résultat sans se tromper |

## Formats

- **PDF** : `Cours_ConquerorLab.pdf` (41 pages) — la version composée, à lire.
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

Trois exemples **complets et compilables**, écrits pour ce cours :

- `Applications/ConquerorLab/exemples/rules/RegleMinimale.cpp` — le plus petit
  moteur qui joue vraiment (carré 5×5) ;
- `Applications/ConquerorLab/exemples/rules/GrilleLibre.cpp` — un plateau
  **circulaire** défini entièrement en C++, sans JSON ;
- `Applications/ConquerorLab/exemples/ai/IAMinimale.cpp` — la plus petite IA qui
  joue vraiment.

Les deux modules de **référence**, plus riches, servent de second palier de
lecture :

- `Applications/ConquerorLab/modules/rules/ConquerorRulesV2.cpp`
- `Applications/ConquerorLab/modules/ai/ConquerorAIRef.cpp`

### État vérifié des exemples — 2026-08-07

Les exemples compilent par la chaîne de l'atelier, et passent le banc d'essai en
jouant l'un contre l'autre :

```
=== Banc d'essai du contrat Conqueror ===
       regles : RegleMinimale 1.0.0 (palier 0)
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
départ ». `RegleMinimale` joue sur un carré 5×5 avec un totem chacun. Les
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
