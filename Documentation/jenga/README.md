# Jenga — Décrire, construire, livrer

Le livre complet sur **Jenga**, le système de construction de Nkentseu.
Version couverte : **2.4.0**.

```
Jenga_le_livre.pdf        le livre
tex/                      les sources LaTeX
tex/chapitres/            un fichier par chapitre
captures/                 les sorties de terminal REELLES citees dans le livre
assets/                   le logo
build.ps1                 la compilation (XeLaTeX, deux passes)
```

## Compiler

```powershell
.\build.ps1            # deux passes -> table des matieres a jour
.\build.ps1 -Quick     # une passe, pour une relecture rapide
```

⚠️ **`-Quick` est UNE passe.** La table des matieres y est celle du tour
precedent. Un PDF qui *maigrit* apres un ajout est le signe qu'on a lu une
compilation partielle, pas une regression.

## Ce que le livre garantit, et comment

**Tout ce qu'il affirme a ete mesure sur la version 2.4.0.** Les sorties de
terminal qu'il cite ne sont pas recopiees : elles sont dans `captures/`, chacune
avec la commande qui l'a produite et son code de sortie en tete de fichier.

Quand une chose n'a pas pu etre verifiee ici — une cible qui exige macOS, un SDK
de console sous accord — **le livre le dit** au lieu de la decrire d'apres son
nom. Il emploie trois mots, et les trois comptent :

| mot | ce qu'il signifie |
|---|---|
| *declaree* | des lignes existent ; rien ne dit qu'elles fonctionnent |
| *construite* | un artefact sort ; rien ne dit qu'il se lance |
| *eprouvee* | l'artefact a tourne sur la cible |

## La demonstration a refaire soi-meme

Le chapitre 9 montre, **captures a l'appui**, qu'une source modifiee dont la date
est ancienne est declaree a jour par Jenga — et que l'ancien binaire tourne, avec
un `SUCCESS` a l'ecran :

```
1. source = VERSION UN, construite normalement       -> VERSION UN
2. source = VERSION DEUX, date NEUVE                 -> VERSION DEUX
3. source = VERSION TROIS, date REMISE A L'ANCIENNE  -> VERSION DEUX
4. la meme source, apres `touch`                     -> VERSION TROIS
```

Le temoin n'est pas la sortie de `jenga` — une ligne « deja a jour » serait deja
une reponse — mais **ce que le programme affiche**. Une compilation qui n'a pas
eu lieu et une compilation sans effet se ressemblent dans un journal ; elles ne
se ressemblent pas a l'ecran.

## Structure

| partie | chapitres | ce qu'elle donne |
|---|---|---|
| I. Prendre Jenga en main | 1–3 | installer, trouver, construire de A a Z |
| II. Ecrire un `.jenga` | 4–7 | sources, sorties, filtres, dependances |
| III. Ce que Jenga en fait | 8–9 | chargement, ordre, fraicheur, parallelisme |
| IV. Les cibles | 10–12 | bureau, mobile, Web — et leur etat reel |
| V. Au-dela de la construction | 13–15 | lancer, livrer, inspecter |
| VI. Diagnostiquer | 16–17 | lire un echec, et la reference |

Chaque chapitre se termine par un bilan et quatre familles d'exercices :
questions de cours, exercice pratique, exercice de **demonstration** (on prouve
par une mesure, on ne constate pas) et exercice de logique.

## Refaire les captures

Elles ont ete produites en enchainant reellement les commandes sur un projet
cree pour l'occasion. Pour les regenerer, refaites les cinq commandes du
chapitre 3 puis les manipulations du chapitre 9 — le livre les donne toutes.

⚠️ **Les sequences ANSI se retirent.** `NO_COLOR=1` ne suffit pas : la banniere
de Jenga les emet quand meme, et une sortie coloree collee dans un livre devient
une bouillie de `[36m`.
