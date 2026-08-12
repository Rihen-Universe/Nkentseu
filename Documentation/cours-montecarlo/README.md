# Cours — Monte-Carlo

**Calculer ce qu'on ne sait pas calculer, en lançant les dés.**
Du jeu de cailloux au moteur de rendu et à l'IA de jeu.

`Cours_MonteCarlo.pdf` — 27 pages, 8 chapitres.

## Ce que couvre le cours

| ch. | sujet | ce qu'on en retient |
|---|---|---|
| 0 | avant-propos | le contrat : on échange l'exactitude contre la possibilité de calculer |
| 1 | compter sans compter | la flaque, π avec de la pluie — **aucune formule** |
| 2 | la règle générale | l'estimateur, et **pourquoi on divise par la densité** |
| 3 | la précision | la loi du 1/√N, et pourquoi la dimension ne compte pas |
| 4 | viser juste | échantillonnage d'importance, stratification, roulette russe |
| 5 | le rendu | path tracing complet, pas à pas, avec le code |
| 6 | l'arbre de jeu | MCTS + UCB1, code complet — pour un jeu qu'on ne sait pas évaluer |
| 7 | les pièges | générateur, biais silencieux, accumulation, parallélisation |

Le chapitre 1 se lit **sans une seule formule** ; elles n'arrivent qu'au
chapitre 2, une fois l'intuition en place, et chacune est expliquée en français
avant d'être écrite en symboles.

## Compiler

```powershell
./build.ps1            # deux passes XeLaTeX (table des matières à jour)
./build.ps1 -Quick     # une passe (aperçu rapide)
./build.ps1 -Clean     # efface les fichiers intermédiaires
```

Nécessite **MiKTeX** (XeLaTeX) et les polices Windows Segoe UI / Consolas —
mêmes prérequis que les cours *ConquerorLab* et *NkCanvas & NKGui*.

## Structure

```
tex/main.tex          assemblage seul : aucun contenu
tex/preambule.tex     charte Rihen + amsmath (ce cours-ci porte des formules)
tex/garde.tex         page de garde
tex/chapitres/*.tex   le contenu
assets/               le logo
```

Contrairement au cours ConquerorLab, il n'y a **pas de dossier `md/`** : la
source unique est le LaTeX. Deux copies d'un même texte divergent toujours, et
c'est le PDF qui est lu.

## Choix de fond

- **Deux mises en œuvre complètes**, pas une : le rendu (chapitre 5) et l'IA de
  jeu (chapitre 6). Le même principe résout deux problèmes qui n'ont rien à
  voir — c'est ce qui fait comprendre qu'il s'agit d'une méthode générale et
  non d'une recette de rendu.
- **Le code suit les conventions Nkentseu** (`NkPascalCase`, `float32`,
  `math::NkRand`, zero-STL) : il est copiable tel quel dans le moteur.
- **Chaque piège est décrit par son symptôme**, pas seulement par sa cause. Un
  bug Monte-Carlo ne plante pas — il rend un nombre plausible. Savoir à quoi
  ressemble l'erreur vaut mieux que savoir qu'elle existe.
