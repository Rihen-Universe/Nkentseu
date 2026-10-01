# UnkenyEditor — les greffons

### Document 04 — laisser chaque équipe construire ses propres outils

> Rihen, 01/10/2026 : *« pour notre jeu, il y a des éléments au comportement différent,
> pareil pour les décors ; il serait intéressant qu'un développeur qui touche notre moteur
> puisse concevoir ses propres outils : intégrer un système de greffons. On crée un outil
> qu'on ajoute pour gérer des aspects spécifiques de notre jeu, non implémentés par le
> moteur. Exemple : chez nous, des personnages au style particulier — comment concevoir un
> outil dans le moteur pour créer nos personnages ? »*
> Il donnera des images du style de blob qu'il a en tête.

## 1. Scripts ou greffons : la différence

| | **composant de script** (doc 01) | **greffon** (ce document) |
|---|---|---|
| sert à | le **comportement en jeu** d'une entité (la porte s'ouvre, la gelée saute) | **ajouter au moteur et à l'éditeur** ce qu'ils ne savent pas faire : des **outils** et des **modules réutilisables** |
| vit | dans une scène, sur une entité | dans le projet (ou partagé entre projets), chargé au démarrage |
| exemples | « quand le joueur touche le cœur, la gelée meurt » | un **créateur de créatures-gelées**, un inspecteur sur mesure, un nouveau type d'asset et sa page, un importeur, une commande de menu, un outil de la vue |

Les deux partagent la même mécanique : le **C++ rechargé à chaud** (DLL dans l'éditeur,
statique à la livraison, table C `NkUnkHoteV1`) et le **Blueprint** (graphe exécuté par la
VM). Un greffon ne réinvente pas cette mécanique, il l'étend.

## 2. Ce qu'un greffon peut ajouter

**À l'exécution (dans le jeu livré)** : des composants, des systèmes (une passe par
image), des types d'asset et leur chargeur (voir le système de ressources,
`Kernel/System/NKSerialization/design/01`), des nœuds Blueprint.

**Dans l'éditeur seulement** :
- une **fenêtre / un panneau** (ancré comme les autres) ;
- une **carte de Détails sur mesure** pour un composant (comme un inspecteur personnalisé
  d'Unity) ;
- un **type d'asset** avec sa **page d'édition** (double-clic dans le Content Browser),
  sa vignette et sa couleur ;
- un **importeur** (un format de fichier → des assets) ;
- des **commandes** (menus, barre d'outils, clic droit) et des **outils de la vue**
  (gizmos, poignées, pinceaux) ;
- des **acteurs** dans « Placer des acteurs ».

## 3. Comment un greffon se présente

- Un **dossier** `Greffons/<Nom>/` dans le projet (ou dans le dossier commun des greffons
  du moteur), avec un **manifeste** `<Nom>.nkgreffon` : nom, version, auteur, genre
  (exécution / éditeur / les deux), dépendances, points d'entrée, icône sombre et claire.
- Son code C++ (compilé par Jenga comme un module, rechargé à chaud) et/ou ses Blueprints.
- Une fenêtre **Greffons** (comme celle d'Unreal) : liste, activer / désactiver,
  recharger, détails, erreurs ; un greffon cassé est **désactivé avec son erreur**, il ne
  fait jamais tomber l'éditeur.
- **Annulation** : toute action d'un greffon passe par l'historique de l'éditeur (comme
  l'IA, R18), donc Ctrl+Z la défait.
- **Ouvert à l'IA** : le panneau IA voit et peut appeler les commandes des greffons.

## 4. Le premier greffon : le créateur de créatures-gelées (Le Dernier Feu)

Un panneau pour composer une créature au style du jeu, à partir des images que Rihen
fournira : forme du corps (gelée à particules), nombre et position des **cœurs**,
couleur et niveau, armure de glace, yeux et visage (toujours lisibles), comportement de
base ; un aperçu vivant dans la vue ; il **fabrique un prefab** (corps mou + cœurs +
collisions + animateur + script de comportement). Les compagnons de Bodofia (végétal,
aérien, animal) en sont des variantes. C'est l'exemple qui prouve le système, et un vrai
outil pour le jeu.

## 5. Ordre

1. Après les scripts S0–S2 (même hôte C++ et même VM) : l'hôte des greffons, le manifeste,
   la fenêtre Greffons, les points d'extension de l'éditeur (panneau, carte de Détails,
   type d'asset, commande).
2. Le créateur de créatures-gelées, avec les images de Rihen.
3. Les points d'extension restants (importeurs, outils de la vue, acteurs).
4. La même chose pour la famille (Nogee, NkAnimaEditor, NKScena…) par NKEditorKit.
