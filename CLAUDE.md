# Règles de travail — Nkentseu (pour Rodolf, les collaborateurs et les agents IA)

Ce fichier est lu en premier par toute personne ou tout agent qui travaille dans ce dépôt.
Il renvoie aux conventions ; en cas de doute, ce sont elles qui font foi.

## 1. Les conventions à appliquer
- **Code** : `CONVENTIONS_CODE.md` (public) — une instruction par ligne, commentaires, en-tête
  de fichier, nommage, **énumérations `NK_<Type>_<Mots>`** (§5.1), arithmétique de pointeur.
- **Fichiers, formats, extensions** : `Conception/CONVENTIONS_FICHIERS.md` (dépôt privé
  Nkentseu-Conception, monté dans `Conception/`).
- **Licence** : `LICENSE` — logiciel propriétaire, tous droits réservés ; copie, reproduction,
  redistribution et usage par une IA interdits sans autorisation écrite (§12bis).

## 2. Organisation des dépôts
- Ce dépôt est **public** : bibliothèques de `Kernel` (hors `AI` et `Bare`), démonstrations,
  outils de base. Les parties commerciales sont des **dépôts privés montés en sous-modules aux
  mêmes emplacements** : `Kernel/AI`, `Kernel/Bare`, `Spark`, `Documentation`, `Conception`,
  `Engine/*`, et les applications UnkenyEditor, UnkenyPlayer, Nogee, NkAnimaEditor, NKCraft,
  NKScena, NKUIDesign, NKCode, PV3DE.
- Cloner : `git clone --recurse-submodules https://github.com/Rihen-Universe/Nkentseu.git`.
  Les 17 sous-modules privés portent `update = none` dans `.gitmodules` : ce clone les saute
  sans erreur (seuls le public et `Externals/` arrivent). **Avec accès** (Rodolf,
  collaborateurs), ajouter une fois, depuis la racine :
  `git submodule update --init --recursive --checkout` (la même commande met ensuite à jour
  tous les sous-modules, privés compris).
- Sans les dépôts privés, `Nkentseu.jenga` saute leurs `includeprive()` et retire les projets
  qui en dépendent (`retirerprojetssansdependances()`) : le public se charge et se construit
  seul. Avec eux, rien ne change. Inclure toute nouvelle partie privée par `includeprive()`.
- Une modification qui touche une partie privée se commite **dans son dépôt**, puis le
  pointeur du sous-module est avancé ici.

## 3. Construire et vérifier
- Build : Jenga (version épinglée dans `Applications/NKCode/JENGA_VERSION`), toujours depuis
  la **racine** : `jenga build --target <Cible>`.
- Lancer les applications depuis la racine. Chaque changement est prouvé par les bancs
  (`--selftest`) avec une contre-épreuve (une mutation doit faire échouer le banc).
- Jamais de pilotage de la souris ou du clavier réels : captures hors écran.

## 4. Langue
Documentation, commentaires et messages de commit en **français**. Les identifiants internes
des API de scripts de jeu sont en **anglais** ; les noms choisis par l'utilisateur sont libres.
