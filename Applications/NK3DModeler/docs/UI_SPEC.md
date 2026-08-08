# NK3DModeler — spécification d'interface

> Rédigé le **2026-07-31**, révisé le même jour après arbitrage de Rihen.
> Compagnon de `SPECIFICATION.md`, qui définit le périmètre ; celui-ci définit
> **la surface**.
>
> **Trois lecteurs, un seul document** — c'est délibéré :
> - **Rihen**, pour valider ou refuser un choix avant qu'il ne soit codé ;
> - **l'agent**, comme référence d'implémentation ;
> - **Banani**, pour produire des maquettes — d'où des contraintes de disposition
>   plutôt que des descriptions littéraires.
>
> Une maquette qui contredirait ce document est une **question à poser**, pas une
> licence artistique : chaque règle ci-dessous répond à un problème constaté.

---

## 0. La décision qui gouverne tout le reste

> **Apparence d'Unreal Engine 5. Gestes de Blender.**

Ce n'est pas un compromis mou, c'est un choix, et il faut l'énoncer parce qu'il
est inhabituel : quelqu'un voudra un jour « corriger » l'un pour le faire coller
à l'autre.

| ce qui vient d'**Unreal Engine 5** | ce qui vient de **Blender** |
|---|---|
| disposition, panneaux dockables, navigateur de projet | raccourcis clavier (G/R/S, TAB, E, I, K, Ctrl+R…) |
| aspect des widgets : boutons, listes, en-têtes repliables | vocabulaire (pivot, orientation, modificateur, appliquer) |
| densité, barre d'outils à icônes, arborescence | modèle mental objet ↔ édition, pile non destructive |

**La raison** : les gestes sont ce qu'on a dans les doigts, la disposition est ce
qu'on lit. Rihen modélise avec les réflexes de Blender ; il veut lire une
interface d'UE5. Les deux ne se contredisent pas — ils touchent des couches
différentes.

> ⚠️ **REGLE D'ARBITRAGE, prioritaire sur tout le reste (Rihen, 31/07)** :
> l'apparence d'UE5 est **PUREMENT COSMETIQUE**. Unreal n'est pas un logiciel
> simple a prendre en main — il est reconnu pour l'inverse. **Notre produit doit
> etre facile a apprendre, comme Blender l'est pour qui le decouvre.** Chaque fois
> que la ressemblance a UE5 et la facilite d'usage se contredisent, **c'est la
> facilite qui gagne.**
>
> « Facile » est traduit en cinq regles verifiables, pour ne pas rester un voeu :
> 1. **tout icone a un mot** — icone seule reservee aux 5 actions evidentes ;
> 2. **le rare est replie** — l'avance est ferme par defaut ;
> 3. **les etats vides parlent** — ils disent quoi faire, ils ne montrent pas du vide ;
> 4. **cinq actions sans menu** — ajouter un objet, changer de mode, ajouter un
>    modificateur, enregistrer, changer l'affichage ;
> 5. **une information, un endroit** — sinon on se demande laquelle fait foi.
> Ces cinq regles sont dans le brief de conception (BRIEF_BANANI.md).

**Explicitement écarté** : reprendre l'apparence de NKCode. NKCode est un IDE,
son identité visuelle lui appartient. Le **mécanisme** de thème et de langue est
partagé ; **l'apparence** ne l'est pas.

> ✅ **Références VÉRIFIÉES le 31/07.** Rihen a fourni les captures dans
> `C:\Users\Rihen\Downloads\ue`. Les sections 2 et 3 sont désormais décrites
> d'après ce que ces images montrent RÉELLEMENT, pas d'après mon souvenir d'UE5 —
> c'est une différence de nature, et elle a corrigé plusieurs détails que j'avais
> supposés (les champs X/Y/Z ne sont pas teintés en fond mais bordés à gauche ;
> le panneau Détails porte une rangée de filtres que je n'avais pas prévue).

---

## 1. Principe directeur

> **Rien d'important ne doit exister uniquement dans un journal.**

C'est ce qui a fait naître ce projet. Aujourd'hui, pour savoir quel modificateur
est actif, quel est le pas d'aimantation ou combien de sommets sont sélectionnés,
il faut **ouvrir `logs/app.log`**.

Corollaire : une information affichée doit être **vraie en continu**. Un compteur
figé est pire qu'absent — un `Draw:0 Tris:0` mort dans le HUD m'a déjà fait
conclure à tort à une scène vide.

---

## 2. Disposition

Relevée sur la capture de référence d'UE5 (zones numérotées), puis adaptée à la
nuance demandée. Les proportions sont indicatives ; les **adjacences** ne le sont
pas.

### 2.1 Ce que montre UE5 (référence)

| zone | contenu |
|---|---|
| **1** | barre de menus + bandeau d'onglets de document (nom du niveau, croix de fermeture) |
| **2** | barre d'outils principale : enregistrer · mode de sélection ▾ · ajouter ▾ · blueprints ▾ · cinématique ▾ · **lecture** ▶ ⏸ ⏹ · plateformes ▾ · réglages ▾ |
| **3** | barre de la VUE : à gauche un groupe en pilule (☰, Perspective ▾, Éclairé ▾, Afficher ▾) ; à droite les outils de transformation, le repère monde/local, et les **aimantations** (grille 10, angle 10°, échelle 0,25, vitesse caméra) |
| **4** | vue 3D |
| **5** | **Outliner**, en haut à droite |
| **6** | **Détails**, sous l'Outliner |
| **7** | tiroir de contenu, en bas à gauche |
| **8** | barre d'état : journal, console, état d'enregistrement, gestion de version |

### 2.2 Notre disposition

**La seule divergence assumée** : l'Outliner passe **à gauche**, demande de
Rihen. Tout le reste suit UE5.

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│ 1  ⬢  Fichier  Édition  Fenêtre  Outils  Sélection  Objet  Aide      MonProjet   │
│    ┌ Scene_01  ✕ ┐                                                              │
├─────────────────────────────────────────────────────────────────────────────────┤
│ 2  [💾] │ [Mode de sélection ▾] │ [＋Ajouter ▾] [Modificateur ▾] │  [⚙ Réglages ▾]│
├──────────────┬──────────────────────────────────────┬───────────────────────────┤
│ 5 HIÉRARCHIE │ 3 ┌ ☰ │ Perspective ▾ │ Éclairé ▾ │  │ 6  Propriétés         ✕   │
│ [▾][🔍     ] │   └ Afficher ▾ ┘   [↖][✥][⟳][⤢][🌐] │  [🔍            ] [⚙][🔒] │
│ Nom    │Type │                    [⊞ 0,5][∠15°][↗] │  Général Objet Rendu  Tout│
│ ▾ Scène│Monde│                                      │  ▾ Transform              │
│  ▸Cube │Maill│ 4         VUE 3D                     │    Position │▌X │▌Y │▌Z │↩│
│  ▸Sphèr│Maill│                                      │    Rotation │▌X │▌Y │▌Z │ │
│  ▾📁Grp│Doss.│                                      │    Échelle  │▌X │▌Y │▌Z │🔒│
│    ▸Roue│Mail│                                      ├───────────────────────────┤
│         │    │                                      │ 6b DÉTAILS  (objet sél.)  │
│         │    │                                      │  ▾ Maillage               │
│ 12 objets (2 │                                      │  ▾ Modificateurs      [＋]│
│  sélectionnés)                                      │  ▾ Matériau               │
├──────────────┴──────────────────────────────────────┴───────────────────────────┤
│ 7  NAVIGATEUR DE PROJET                                                   ✕     │
│  [＋Ajouter][⤓Importer][💾Tout enregistrer] │ ← → 📁 Tout ▸ Contenu ▸ Perso  [⚙]│
│  ┌ Favoris ─────┬──────────────────────────────────────────────────────────────┐│
│  │ ▾ MonProjet  │ [▾][🔍          ]  ●Maillage ●Animation ●Matériau ●Texture   ││
│  │   ▸ Maillages│  ┌────┐ ┌────┐ ┌────┐                                        ││
│  │   ▸ Animation│  │ ▣  │ │ ▣  │ │ ▣  │                                        ││
│  │   ▸ Matériaux│  │Cube│ │Tête│ │Bois│                                        ││
│  │ ▸ Collections│  └▬▬▬▬┘ └▬▬▬▬┘ └▬▬▬▬┘   ← barre de couleur = TYPE            ││
│  └──────────────┴──────────────────────────────────────────────────────────────┘│
│                                                                       3 éléments│
├─────────────────────────────────────────────────────────────────────────────────┤
│ 8  [📂 Tiroir] [Journal] [Cmd ▾ …]      S:8 A:12 F:6 · sél.4 · Cube · 60 ips     │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Règles, et leur raison

1. **La hiérarchie est à GAUCHE et SEULE de ce côté.** Divergence assumée avec
   UE5. La colonne gauche n'accueille rien d'autre : un sommaire perd sa
   lisibilité dès qu'on l'empile avec autre chose.
2. **Propriétés (6) au-dessus de Détails (6b)**, à droite, et leur partage est
   une **règle**, pas un rangement :
   - **Propriétés** : ce qui existe pour **tout** objet — transform, nom,
     visibilité. Contenu **fixe**, donc toujours au même endroit à l'œil.
   - **Détails** : ce qui est propre à **cet** objet. **Cette zone s'allonge** :
     ajouter un modificateur, un matériau, une propriété, c'est ajouter une
     section ici. Demande de Rihen, mot pour mot.
   Sans ce partage, la position d'un objet descendrait de plus en plus bas à
   mesure qu'on lui ajoute des choses.
3. **La barre de vue (3) est DANS la vue**, en surimpression, avec le groupe de
   navigation à gauche et les outils + aimantations à droite — exactement la
   disposition d'UE5. Les réglages d'aimantation y sont **visibles en
   permanence** : c'est là que se lit le pas de la grille, ce qui n'existait
   nulle part chez nous.
4. **Le navigateur (7) est en bas, sur toute la largeur**, avec l'arborescence de
   source à gauche et la grille d'éléments à droite.
5. **La barre d'état (8) porte les compteurs** — aujourd'hui absents ou morts.
6. **Tout est dockable** : onglets avec croix, glisser-déposer, dispositions
   enregistrables. ⚠️ NKEditorKit a un **défaut connu sur le split droit** : à
   corriger **avant** de promettre des panneaux déplaçables, sinon la première
   démonstration échouera là-dessus.

## 3. Aspect des widgets — relevé sur les captures

Demande explicite de Rihen : **comme UE5, pas comme NKCode**. Ce qui suit est
relevé sur les images, pas supposé.

### 3.1 Ce que les captures montrent

| élément | ce qu'on voit dans UE5 |
|---|---|
| **fond** | très sombre et NEUTRE (gris presque noir) ; les panneaux à peine plus clairs que le fond ; les séparations sont des traits de 1 px, pas des ombres |
| **arrondi** | quasi nul sur les panneaux ; léger sur les puces et les boutons de la barre de vue |
| **densité** | rangées serrées (~22 px), typographie petite, beaucoup d'information par écran |
| **ligne de propriété** | **deux colonnes** : libellé à gauche sur fond légèrement plus sombre, valeur à droite |
| **champs X/Y/Z** | **bordés d'un trait de couleur À GAUCHE** (rouge / vert / bleu) — le fond du champ reste neutre. *J'avais supposé des champs teintés : c'est faux.* |
| **propriété modifiée** | flèche de **réinitialisation** `↩` à l'extrême droite de la rangée |
| **choix exclusif** | **segmenté** (ex. Statique │ Stationnaire │ Mobile), l'actif en surbrillance |
| **filtres** | rangée de **puces** au-dessus des sections (Général · Objet · Rendu · … · **Tout**), l'active en **bleu plein**. *Non prévu dans ma première version.* |
| **sections** | triangle `▾` + titre en gras, empilées, sans cadre |
| **outils de vue** | **puces carrées** groupées ; l'outil actif est **bleu plein** |
| **sélection dans une liste** | rangée surlignée en **bleu**, texte inchangé |
| **onglet de panneau** | icône + nom + **croix de fermeture** |
| **état vide** | phrase en italique, centrée (« Sélectionnez un objet pour voir ses détails. ») |
| **navigateur** | filtres de type en **puces COLORÉES**, et chaque vignette porte une **barre de couleur** indiquant son type |
| **arborescence** | dossiers en **jaune**, colonne « Type » atténuée à droite, pied de liste « 63 objets (2 sélectionnés) » |

### 3.2 Ce que ça donne comme règles

- **La ligne à deux colonnes est la brique la plus structurante.** Le panneau
  Détails en est presque entièrement fait ; c'est elle qui donne à UE5 sa
  lisibilité en balayage vertical.
- **Le bleu est la couleur de l'ÉTAT** (actif, sélectionné dans une liste).
  **L'orange est la couleur de la SÉLECTION 3D** — c'est déjà notre convention
  dans le viewport, et UE5 fait exactement pareil. Les deux ne se confondent pas.
- **La couleur porte du sens dans le navigateur** : un type = une couleur, sur la
  puce de filtre ET sur la vignette. C'est ce qui permet de distinguer d'un coup
  d'œil maillages, animations et matériaux — précisément ce que Rihen demande.
- **La flèche de réinitialisation n'est pas un détail** : elle dit qu'une valeur
  a été modifiée par rapport au défaut. Sans elle, on ne sait pas ce qu'on a
  touché.

### 3.3 Ce qu'on ne reprend PAS de NKCode

| | NKCode | NK3DModeler |
|---|---|---|
| arrondi | 5 px | **2 px** |
| densité | confortable (IDE) | **dense** |
| boutons | remplis, colorés | **plats**, bordure discrète, remplissage au survol |
| libellé de champ | au-dessus | **à gauche, deux colonnes** |

### 3.4 Grammaire de couleurs métier — déjà établie, à ne pas réinventer

| notion | couleur |
|---|---|
| axe X / Y / Z | rouge / vert / bleu |
| **non sélectionné** | noir |
| **sélectionné** | ambre `#F2980E` (cf. § 10bis.2) |
| **actif** | blanc |
| lumière | jaune, teinte claire si active |

**Trois états, pas deux.** La distinction sélectionné / actif existe partout dans
le viewport ; la hiérarchie et les panneaux doivent la respecter.

## 4. Navigateur de projet et fichier de projet

Demande de Rihen : *« la partie docker va permettre de structurer un projet en
dossiers qui seront sauvegardés dans le fichier de projet de modèle 3D, et on
pourra distinguer les maillages, des animations, des matériaux, etc. »*

**Ce que ça implique, et c'est un vrai morceau** : NK3DModeler a besoin d'un
**format de projet** qui porte, en plus des données, une **arborescence choisie
par l'utilisateur**.

- **Catégories fixes** : Maillages · Animations · Matériaux · Textures ·
  Collections. Elles sont **imposées** — c'est ce qui permet à l'outil de savoir
  ce qu'il manipule.
- **Dossiers libres à l'intérieur** : l'utilisateur organise comme il veut, et
  cette organisation est **enregistrée**. Sans cela, il la refait à chaque
  ouverture.
- **Un asset a un chemin stable** (`Maillages/Perso/Tete`) : c'est lui que
  référencent la scène et l'animation. Renommer un dossier doit mettre à jour les
  références, ou le projet casse en silence — **piège classique, à traiter dès la
  conception du format, pas après**.
- **Le fichier de projet n'est pas la scène.** Une scène décrit ce qui est posé
  dans l'espace ; le projet décrit ce qui est *disponible*. Les confondre
  empêcherait d'avoir deux scènes partageant les mêmes maillages.

> Décision à prendre avant d'écrire le format : **un seul fichier** (archive) ou
> **un dossier** (fichiers séparés + index) ? Un dossier facilite le suivi de
> version et le travail à plusieurs ; un fichier unique se déplace mieux. Non
> tranché ici — ce n'est pas une question d'interface.

---

## 5. Système de thèmes

Demande : plusieurs thèmes livrés, **et l'utilisateur peut créer les siens**.

**Ce qui existe déjà** : `NkGuiTheme` dans `NKGui/Core/NkGuiContext.h` — 20
couleurs et 3 mesures (arrondi, marges). C'est la bonne base, mais telle quelle
elle ne suffit pas :

| manque | pourquoi c'est bloquant |
|---|---|
| **pas d'identité** | un thème n'a ni nom ni auteur : impossible d'en proposer une liste |
| **pas de chargement/enregistrement** | « créer son thème » suppose un fichier |
| **mesures trop pauvres** | UE5 a besoin d'une hauteur de ligne, d'une largeur de colonne de libellé, d'une épaisseur de trait |
| **couleurs métier absentes** | les axes X/Y/Z et les trois états de sélection doivent être **dans** le thème, sinon ils resteront en dur et un thème clair les rendra illisibles |

**Décisions**
1. Le système vit dans **NKGui** — partagé avec NKCode, qui en bénéficie.
2. Chaque application livre son **thème par défaut** : NKCode garde le sien,
   NK3DModeler démarre sur un thème « UE5 sombre ». Le mécanisme est commun,
   l'apparence ne l'est pas.
3. Thèmes fournis : **UE5 sombre** (défaut), **UE5 clair**, **contraste élevé**.
4. Un thème est un **fichier lisible** que l'utilisateur peut copier et modifier.
   Un thème inconnu ou incomplet **retombe sur le défaut champ par champ** — un
   thème auquel il manque une couleur ne doit pas produire une interface noire
   sur noir.

---

## 6. Multilangue

**Ce qui existe déjà** : la roadmap de NKCode porte l'i18n — *« 5 langues majeures
+ ghomala' (bamiléké) »*, avec un sélecteur dans Paramètres › Général.
NK3DModeler **utilise le même mécanisme**, pas un second.

**La règle, et elle est déjà la nôtre à deux autres endroits** :

> **clé stable + libellé traduisible.**

| système | clé stable (jamais renommée) | partie traduisible |
|---|---|---|
| paramètres de modificateur | `NkModParam::name` (`array_count`) | `label` |
| raccourcis | `NkShortcutBinding::command` (`mesh.extrude`) | `label` |
| **interface** | identifiant de texte | traduction |

Cette cohérence n'est pas cosmétique : elle veut dire qu'un fichier enregistré,
une courbe d'animation et une configuration de raccourcis **survivent** à un
changement de langue. Un libellé traduit n'est jamais une identité.

⚠️ **Conséquence à assumer dès le premier bouton** : aucun texte affiché n'est
écrit en dur. L'ajouter après coup obligerait à repasser sur chaque libellé.
Le ghomala' impose en plus une exigence technique : **diacritiques combinants**,
donc normalisation Unicode explicite et police capable de les composer — voir
`docs/LANGUES_LOCALES_CAMEROUN.md`.

---

## 7. Le panneau Modificateurs — spécification détaillée

Premier panneau livré, et le plus contraint. Il vit dans **Détails (F)** et
consomme directement `NkModifierParams(type, count)`.

```
▾ Modificateurs                                              [+ Ajouter ▾]
  ┌────────────────────────────────────────────────────────────────────┐
  │ ▾  ◉  Subdivision de surface                        id 1     ⋮  ✕  │
  │      Niveaux                    │  2                              │
  │      Simple (linéaire)          │  ☐                              │
  ├────────────────────────────────────────────────────────────────────┤
  │ ▾  ◉  Chanfrein                                     id 2     ⋮  ✕  │
  │      Largeur                    │  0,050  ───────────             │
  │      Segments                   │  1                              │
  └────────────────────────────────────────────────────────────────────┘
```

### Règles dures

- **Aucun widget écrit à la main par type de modificateur.** Les contrôles sont
  déduits de `NkModParam::type` : `Bool` → case · `Int` → champ entier ·
  `Float` → curseur borné par `minV`/`maxV` · `Vec3` → trois champs X/Y/Z teintés.
  **Critère d'acceptation** : ajouter un 18ᵉ modificateur ne doit demander
  **aucune ligne d'interface**. Sinon le panneau est mal écrit.
- **`label` s'affiche, `name` jamais.** `name` est une clé d'animation et de
  fichier ; l'exposer inviterait à la renommer.
- **L'ordre de la pile est l'ordre d'évaluation**, de haut en bas. Glisser-déposer
  souhaité ; ↑↓ obligatoire (accessible et scriptable).
- **`◉` = activé.** Un modificateur éteint reste visible et grisé — le masquer
  ferait croire qu'il a été supprimé.
- **« Appliquer » est destructif et doit le dire.** Si le modificateur n'est pas
  le premier, afficher l'avertissement que le moteur remonte déjà
  (`outWarnNotFirst`) : le résultat ne correspondra pas à l'affichage.
- **`id` est visible.** C'est la clé qu'une courbe d'animation utilisera.
- **`⋮`** ouvre : marquer un paramètre pour l'animation *(v2)*, copier, épingler.

---

## 8. Règles d'interaction

1. **Un clic, un destinataire.** Le clic est arbitré une seule fois : poignée de
   gizmo, puis lumière (distance écran), puis objet/élément. Déjà implanté, et un
   clic dans un panneau ne doit jamais atteindre la vue.
2. **Rien n'est refusé en silence.** Si une action n'a pas d'effet, on l'exécute et
   on **dit** pourquoi. Jurisprudence : déplacer une lumière directionnelle est
   autorisé, et un message explique que l'éclairage ne bougera pas.
3. **Toute opération destructive pose un point d'annulation avant.**
4. **Aperçu temps réel des opérations modales** : souris = paramètre continu,
   molette = paramètre entier, clic gauche = confirmer, Échap = annuler. Rien
   n'entre dans l'historique tant que ce n'est pas confirmé.
5. **Les panneaux ne dupliquent jamais la logique.** Un bouton émet la **même
   commande** que le raccourci — sinon l'humain, le rejeu et l'IA divergeront.

---

## 9. Raccourcis

**Aucun raccourci en dur.** Tous passent par `NkShortcutTable` (NKEditorKit,
livrée le 31/07) : clé de commande, combinaison, contexte. Trois conséquences,
toutes recherchées : l'interface **affiche** le raccourci ; l'utilisateur peut le
**changer** ; les **conflits** sont détectés.

**Inventaire porté depuis Demo3D** : 32 liaisons, 0 conflit — le jeu actuel est
propre. Reste à porter les touches de vue (pavé numérique) et de session (F1-F12).

**Écart connu** : `F7`-`F10`, `[`, `]`, `\` ne sont pas des raccourcis Blender —
ils datent d'une époque sans panneau. Une fois le panneau là, la plupart n'ont
plus lieu d'être ; les garder en second rôle est acceptable, en inventer d'autres
ne l'est pas.

---

## 9bis. Chemins d'accès aux commandes *(Rihen, 31/07)*

**Le défaut qu'il corrige.** Ce document affirmait « simple à prendre en main »
tout en ne décrivant, pour l'extrusion et ses semblables, qu'un accès **au
clavier**. Les deux sont incompatibles : un modeleur qui exige de mémoriser des
raccourcis avant de pouvoir extruder une face échoue au critère posé en § 1.

### La règle

> **Toute commande atteignable au clavier doit l'être aussi à la souris, et
> partout où elle apparaît, son raccourci est affiché à côté d'elle.**

La seconde moitié fait le travail : le menu n'est pas un pis-aller pour ceux qui
ne connaissent pas les raccourcis, c'est **ce qui les enseigne**. Un menu muet
laisserait l'utilisateur débutant le rester.

### Les quatre chemins

| chemin | rôle | état |
|---|---|---|
| **menus de commandes** dans la barre flottante de la vue — `Ajouter/Objet/Sélection` en mode objet, `Ajouter/Maillage/Sommet/Arête/Face` en mode édition | découverte : on parcourt ce qui existe | ❌ à maquetter — **modifie** les écrans A et B |
| **menu contextuel** au clic droit, filtré par le mode de sélection courant | usage courant : ce qu'on peut faire **ici**, sans traverser l'écran | ❌ nouvel écran G |
| **palette de recherche** : on tape « extru », on obtient la commande, son chemin de menu et son raccourci | filet de sécurité — permet à l'interface de rester dépouillée sans rien rendre introuvable | ❌ nouvel écran H |
| **panneau de dernière opération**, flottant en bas à gauche de la vue | on extrude d'abord, on **règle ensuite** au chiffre près | ❌ à maquetter — **modifie** l'écran B |
| **panneau T**, vertical à gauche de la vue, repliable (touche `T`) | la liste des outils et les réglages de l'outil courant — **indispensable au sculpt** | ❌ à maquetter — **modifie** l'écran B, plus un écran sculpt |

### Le panneau T — barre d'outils verticale *(Rihen, 31/07 — révision)*

J'avais écarté ce cinquième chemin en jugeant qu'il ferait doublon avec le groupe
de boutons carrés de la barre flottante. **C'était une erreur d'analyse**, et la
raison est le sculpt : une barre horizontale peut tenir cinq outils de
transformation, elle ne peut pas tenir une **liste de brosses avec leurs
réglages**. Or le sculpt est au périmètre.

Les deux ne font donc pas doublon, ils ne portent pas la même chose :

| | contenu | pourquoi là |
|---|---|---|
| **barre flottante** (haut de la vue) | l'outil **actif** et le mode de vue | changement rapide, toujours visible, coût vertical nul |
| **panneau T** (gauche de la vue, repliable) | la **liste** des outils et les **réglages de l'outil courant** | il faut de la hauteur : brosses, force, rayon, courbe d'atténuation, symétrie |

**Contenu par mode :**

- **objet** — sélection, curseur, déplacement, rotation, échelle, transformer ;
- **édition** — les précédents plus extruder, biseauter, insérer, découper,
  boucle de coupe, glisser une arête, lisser, poinçonner ;
- **sculpt** — la liste des brosses (élever, creuser, lisser, pincer, gonfler,
  aplanir, masque, peindre — les huit modes déjà déclarés dans `NkSculptTypes.h`),
  et sous elle les réglages de la brosse courante : **rayon**, **force**, **courbe
  d'atténuation** (les cinq profils déjà déclarés), **symétrie X/Y/Z**.

**Repliable, et refermé par défaut en mode objet.** Un débutant qui ouvre le
logiciel doit voir la scène, pas trois panneaux. En mode sculpt il s'ouvre seul :
sans lui, le mode est inutilisable.

**Raccourci `T`**, comme Blender — c'est le geste que tout utilisateur venant de
Blender essaiera en premier, et le refuser n'apporterait rien.

Comme les quatre autres chemins, il se peuple depuis `NkShortcutTable` ; ses
réglages de brosse depuis `NkModParam`. Toujours aucune liste écrite deux fois.

### Ce que ça coûte en code : rien de nouveau

Les quatre chemins lisent la **même** `NkShortcutTable` déjà livrée — clé de
commande stable, libellé traduisible, combinaison, contexte. Un menu est une
**vue** sur cette table filtrée par contexte ; la palette est la même table
filtrée par texte. Aucune liste de commandes n'est écrite deux fois, donc aucune
ne peut diverger.

Le panneau de dernière opération, lui, se branche sur `NkModParam` : mêmes
paramètres nommés, mêmes bornes, même rendu générique que le panneau
Modificateurs (§ 7). C'est le troisième consommateur de ce mécanisme.

**Conséquence sur la génération de maquettes** : les écrans A et B sont à
**refaire** (barre flottante, panneau de dernière opération, et **panneau T
ouvert** sur B) ; G, H et **I (mode sculpt)** sont **nouveaux**. Les autres
écrans ne bougent pas.

---

## 10. Pour Banani — ce qu'on attend d'une maquette

**À produire :** la disposition de la section 2 en trois états —
1. mode objet, un objet sélectionné ;
2. mode édition, sélection de faces ;
3. panneau Détails avec deux modificateurs empilés.

**Contraintes à respecter** : les adjacences de la section 2 (hiérarchie à gauche,
Propriétés puis Détails à droite, navigateur en bas) · l'aspect UE5 de la
section 3, **pas** celui de NKCode · la grammaire de couleurs (trois états :
noir / orange / blanc) · les compteurs de la barre d'état toujours visibles ·
la ligne de propriété à **deux colonnes**.

**Libre** : typographie, icônes, densité exacte, traitement des ombres, déclinaison
claire du thème.

**À éviter — ce sont des erreurs déjà payées** : fenêtres flottantes par défaut ·
information disponible seulement au survol · un panneau modificateurs dessiné
avec des contrôles spécifiques par type, alors qu'il est **générique** · des
libellés qui laisseraient croire qu'ils sont écrits en dur dans le code.

---

## 10bis. Palette imposée et rôles

Rihen a fourni six couleurs à intégrer aux thèmes principaux (sombre **et** clair) :
`#F2980E` `#0A545E` `#095461` `#141414` `#2B2B2B` `#212121`.

### 10bis.1 Les trois gris — la structure du thème sombre

| couleur | rôle | pourquoi celui-là |
|---|---|---|
| `#141414` | **fond de fenêtre** | le plus sombre : il doit reculer derrière tout le reste |
| `#212121` | **fond des panneaux** | un cran au-dessus, pour que le panneau se détache du vide |
| `#2B2B2B` | **en-têtes de panneau, barres d'outils, en-têtes de section** | le plus clair des trois : ce qui structure doit se lire en premier |

Trois valeurs suffisent, et c'est une bonne chose : une hiérarchie à trois niveaux
se lit sans effort. En ajouter un quatrième rendrait les écarts indistincts.

### 10bis.2 L'ambre `#F2980E` — et une décision à prendre

`#F2980E` est **très proche** de l'orange de sélection 3D que j'avais posé
(`#FF8C0D`) : 13 points d'écart sur le rouge, 12 sur le vert. **Côte à côte, on ne
les distinguerait pas.** Garder les deux créerait une différence que personne ne
peut voir mais que tout le monde devrait maintenir.

> **Décision : `#F2980E` devient l'orange UNIQUE du produit.** Il sert à la fois de
> couleur de **sélection 3D** et d'accent **ambre** pour les nœuds d'action et les
> avertissements. `#FF8C0D` est retiré de la spécification.

Cela ne remet pas en cause la règle qui compte : **le bleu reste l'état de
l'interface, l'ambre reste la sélection 3D.** Ce sont deux familles, pas deux
nuances.

### 10bis.3 Les deux sarcelles `#0A545E` et `#095461` — écart imperceptible, usage précis

Elles diffèrent de **1 à 3 points par canal**. Aucun œil ne les sépare si elles
sont voisines. Les affecter à deux rôles distincts et **simultanément visibles**
serait une erreur : l'utilisateur croirait à une seule couleur et se demanderait
pourquoi elle « bave ».

> **Décision : les affecter à des états qui ne coexistent JAMAIS.**
> `#0A545E` = en-tête de nœud de **données / évaluation** au repos.
> `#095461` = le **même** en-tête, survolé ou sélectionné.
> Un écart minime est exactement ce qu'il faut pour un changement d'état : assez
> pour être ressenti au survol, trop peu pour créer une seconde famille de couleur.

### 10bis.4 Déclinaison CLAIRE

Le thème clair n'inverse pas les gris — il les **remplace** :
fond `#F5F5F5` · panneaux `#FFFFFF` · en-têtes `#EAEAEA` · texte `#1A1A1A`.

Les couleurs **porteuses de sens** sont conservées mais **assombries** pour rester
lisibles sur fond clair : l'ambre passe de `#F2980E` à `#C97A08`, le bleu d'état de
`#1177D1` à `#0E5FA6`, les sarcelles restent inchangées (elles sont déjà sombres).

> ⚠️ **C'est précisément pour cela que les couleurs métier doivent vivre DANS le
> thème** — axes X/Y/Z et trois états de sélection compris. Laissées en dur dans le
> code, elles rendraient le thème clair illisible, et personne ne s'en apercevrait
> avant de l'essayer.

---

## 10ter. Éditeur de nœuds — modélisation et matériaux

Rihen : *« on doit avoir aussi la possibilité de faire de la modélisation par
blueprint ou visuel, pareil pour les matériaux. »* Le périmètre et l'architecture
sont traités dans `SPECIFICATION.md` § 4.4 (le substrat `NKGraph` est déjà arbitré).
Ici : **à quoi ça ressemble**.

### 10ter.1 Deux références, deux emprunts distincts

| référence | ce qu'on en prend |
|---|---|
| **capture 1** (éditeur sombre à nœuds arrondis) | **le style général** : cartes sombres à coins arrondis, bandeau de titre coloré, ports typés, fils courbes colorés par type, fond quadrillé de points |
| **capture 2** (Blueprint UE5) | **l'en-tête seulement**, et un détail précis : **les broches d'exécution sont DANS le bandeau de titre** — entrée à gauche, sortie à droite — tandis que les broches de **données** sont dans le corps |

**Pourquoi ce détail compte** : il sépare visuellement le **flux** (l'ordre des
opérations) de la **donnée** (ce qui circule). On lit la chaîne d'exécution en
suivant une seule ligne horizontale, sans la chercher parmi les valeurs.

### 10ter.2 Anatomie d'un nœud

```
        ┌──────────────────────────────────────────────┐
   ▶────┤  Extruder                              ────▶ │  ← bandeau : titre +
        │                                              │    broches d'EXÉCUTION
        ├──────────────────────────────────────────────┤
   ●────┤  Maillage                                    │  ← corps : broches de
   ●────┤  Distance          │  0,25                   │    DONNÉES + valeurs
        │                              Maillage   ────● │
        └──────────────────────────────────────────────┘
```

- **Bandeau** : hauteur ~24 px, coloré **par famille** (§ 10ter.3), titre en
  demi-gras clair, broches d'exécution en **triangle** aux deux extrémités.
- **Corps** : fond `#212121`, lignes en **deux colonnes** — exactement le même
  composant que le panneau Détails. Une broche non connectée affiche son **champ
  de saisie** ; connectée, le champ disparaît.
- **Broches de données** : petits **cercles**, colorés par **type**.
- **Coins** : 6 px sur les nœuds (plus généreux que les 2 px des panneaux — un nœud
  est un objet flottant, pas une zone d'interface).

### 10ter.3 Couleur du bandeau = famille de nœud

| famille | couleur | exemples |
|---|---|---|
| **Action / opération** | ambre `#F2980E` | Extruder, Chanfreiner, Subdiviser |
| **Donnée / évaluation** | sarcelle `#0A545E` (survol `#095461`) | Nombre, Vecteur, Expression |
| **Contrôle de flux** | gris `#2B2B2B` | Si, Répéter, Séquence |
| **Entrée / sortie** | bleu `#1177D1` | Maillage d'entrée, Résultat |

### 10ter.4 Fils

Courbes de Bézier, épaisseur 2 px, **couleur du TYPE transporté** — pas de la
famille du nœud. Le fil d'exécution est **blanc et plus épais** (3 px) : c'est le
squelette du graphe, il doit se distinguer d'un coup d'œil de tout ce qui est
donnée.

### 10ter.5 Fond

`#141414` avec une **grille de points** discrets (1 px, blanc à 6 %, pas de 24 px).
Des points plutôt qu'un quadrillage : ils donnent le repère de position et
d'aimantation sans ajouter de lignes qui entreraient en concurrence avec les fils.

### 10ter.6 Règles

1. **Un type = une couleur**, la même sur la broche et sur le fil. Sans cela, on ne
   peut pas voir d'un regard ce qui est connectable à quoi.
2. **Une connexion invalide est refusée à la prise** et le dit — pas acceptée puis
   signalée en erreur plus tard.
3. **Un nœud repliable** sur son seul bandeau : un graphe de modélisation devient
   vite dense.
4. **Le panneau Détails montre le nœud sélectionné**, avec les mêmes lignes à deux
   colonnes. Une seule grammaire de propriété dans toute l'application.

---

## 11. Socle partagé — NK3DModeler, Nogee, NkAnima

Décision de Rihen (31/07) : **le même principe pour les trois applications**,
chacune dans son domaine.

| application | domaine | ce qui remplace la vue 3D et les panneaux |
|---|---|---|
| **NK3DModeler** | modélisation | vue 3D · hiérarchie · propriétés/détails · navigateur de projet |
| **Nogee** | moteur de jeu | vue de niveau · hiérarchie d'acteurs · détails · navigateur d'assets · **lecture ▶** |
| **NkAnima** | animation 3D | vue 3D + **ligne de temps / feuille d'exposition / éditeur de courbes** · hiérarchie d'os · détails · bibliothèque de mouvements |

**Ce qui est COMMUN et doit vivre dans NKEditorKit / NKGui — donc écrit une fois :**
- la coquille : menus, bandeau d'onglets, barre d'outils, docking, barre d'état ;
- le **système de thèmes** (§ 5) et le **multilangue** (§ 6) ;
- la **table de raccourcis** (§ 9), déjà livrée ;
- les **widgets d'UE5** de la § 3 : ligne à deux colonnes, champ vectoriel bordé,
  segmenté, puces de filtre, section repliable, arborescence, grille de vignettes ;
- le **panneau générique piloté par une table de paramètres** (§ 7) — sa valeur
  dépasse les modificateurs : il vaut pour tout objet décrit par des paramètres
  nommés.

**Ce qui est PROPRE à chaque application** : le contenu de la vue, les commandes
métier, les panneaux spécifiques (ligne de temps pour NkAnima, lecture pour
Nogee).

> ⚠️ **Conflit à résoudre, découvert le 31/07** :
> `Applications/NkAnimaEditor/important/interface.md` existe déjà — **1 655
> lignes**, 23 sections, et il décrit une interface **façon Cascadeur + Blender**.
> Il **précède** la décision UE5 et la contredit sur la disposition et l'aspect.
> Il reste précieux pour son **contenu métier** (ligne de temps, éditeur de
> courbes, bibliothèque de mouvements, directeur IA) — c'est la partie qu'aucune
> capture d'UE5 ne donnera. À faire : **réconcilier**, pas jeter — garder ses
> chapitres métier, remplacer ses choix de disposition et d'aspect par ceux-ci.
> Ne pas le laisser diverger en silence : c'est exactement ainsi qu'on se
> retrouve avec deux éditeurs qui ne se ressemblent pas.

## 12. Ce que ce document ne fixe pas encore

Assumé, à trancher quand la question se posera :
- **format du fichier de projet** : un fichier ou un dossier (§ 4) ;
- **jeu d'icônes** : dessinées ou une police d'icônes ;
- **accessibilité** (contraste minimal, taille de police) : à instruire, pas à
  improviser ;
- **disposition par défaut à la première ouverture** : celle de la section 2, mais
  les proportions restent à régler à l'usage.
