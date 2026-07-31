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

**Explicitement écarté** : reprendre l'apparence de NKCode. NKCode est un IDE,
son identité visuelle lui appartient. Le **mécanisme** de thème et de langue est
partagé ; **l'apparence** ne l'est pas.

> ⚠️ **Réserve honnête sur les références.** Les trois liens fournis par Rihen
> sont des pages de texte ; leurs captures d'écran ne sont pas lisibles par
> l'outil de récupération dont je dispose. Cette spécification part donc de ma
> connaissance de la disposition d'UE5, pas d'une lecture de ces pages précises.
> **À corriger sur maquette** plutôt qu'à supposer acquis.

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

Contraintes de zone, pas d'esthétique. Les proportions sont indicatives ; les
**adjacences** ne le sont pas.

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│ A  MENUS      Fichier   Édition   Fenêtre   Outils   Aide                        │
├─────────────────────────────────────────────────────────────────────────────────┤
│ B  BARRE D'OUTILS   [💾] [Mode ▾] [Ajouter ▾] │ [↖][✥][⟳][⤢] │ [Aimant ▾] […]   │
├──────────────┬──────────────────────────────────────┬───────────────────────────┤
│ C HIÉRARCHIE │  D  VUE 3D                           │ E  PROPRIÉTÉS             │
│              │  ┌ barre de vue ──────────────────┐  │  ▾ Transform              │
│  ▾ Scène     │  │ Persp │ Éclairé ▾ │ Afficher ▾ │  │    Position   X  Y  Z     │
│    ▸ Cube    │  └────────────────────────────────┘  │    Rotation   X  Y  Z     │
│    ▸ Sphère  │                                      │    Échelle    X  Y  Z     │
│    ▸ Soleil  │                                      ├───────────────────────────┤
│    ▾ Groupe  │                                      │ F  DÉTAILS  (objet sél.)  │
│      ▸ Roue  │                                      │  ▾ Maillage               │
│      ▸ Axe   │                                      │  ▾ Modificateurs      [+] │
│              │                                      │  ▾ Matériau               │
├──────────────┴──────────────────────────────────────┴───────────────────────────┤
│ G  NAVIGATEUR DE PROJET                                                         │
│  ▾ MonProjet    │  Maillages │ Animations │ Matériaux │ Textures                │
│    ▸ Maillages  │   ▣ Cube      ▣ Perso      ▣ Décor                            │
│    ▸ Animations │                                                               │
├─────────────────────────────────────────────────────────────────────────────────┤
│ H  BARRE D'ÉTAT   S:8  A:12  F:6 · sél. 4 · actif : Cube · 60 ips · [message]    │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Règles de disposition, et leur raison

1. **C (hiérarchie) est à GAUCHE.** C'est la nuance demandée par Rihen : UE5 place
   son *Outliner* en haut à droite. Ici il est à gauche, donc **seul** de ce côté —
   la colonne gauche n'accueille rien d'autre, sinon la hiérarchie perd sa
   lisibilité de sommaire.
2. **E et F sont à DROITE, l'un au-dessus de l'autre**, et leur partage est une
   règle, pas un rangement :
   - **E — Propriétés** : ce qui existe pour **tout** objet (transform, visibilité,
     nom). Contenu **fixe**, donc toujours au même endroit à l'œil.
   - **F — Détails** : ce qui est propre à **cet** objet-là. **Cette zone
     s'allonge** : ajouter un modificateur, un matériau, une propriété, c'est
     ajouter une section ici. C'est la demande de Rihen, mot pour mot — « si on
     ajoute une propriété, elle est ajoutée sur la zone de détail de l'objet
     sélectionné ».
   La raison du partage : si tout était mélangé, la position d'un objet
   descendrait de plus en plus bas à mesure qu'on lui ajoute des choses.
3. **G (navigateur de projet) est en BAS, sur toute la largeur.** Équivalent du
   *Content Browser* d'UE5. Il structure le projet en dossiers, et cette structure
   est **enregistrée dans le fichier de projet** (§ 4).
4. **D (vue 3D) porte sa propre barre**, dans la vue : mode de caméra, mode
   d'affichage, options d'overlay. Ce qui concerne la vue reste dans la vue.
5. **H est sur toute la largeur** et porte les compteurs. Ils sont aujourd'hui
   absents ou morts : c'est un défaut à corriger, pas un détail.
6. **Tout est dockable**, façon UE5 : onglets, glisser-déposer, dispositions
   enregistrables. ⚠️ NKEditorKit a un **défaut connu sur le split droit** — à
   corriger **avant** de promettre des panneaux déplaçables, sinon la première
   démonstration échouera là-dessus.

---

## 3. Aspect des widgets — UE5, pas NKCode

Demande explicite de Rihen. Les différences qui comptent, parce qu'elles se
voient immédiatement :

| élément | NKCode (à ne PAS reprendre) | NK3DModeler (UE5) |
|---|---|---|
| **arrondi** | 5 px, doux | **2 px**, quasi net |
| **densité** | confortable, IDE | **dense** : hauteur de ligne réduite, marges serrées |
| **boutons** | remplis, colorés | **plats**, bordure discrète, remplissage au survol |
| **accent** | bleu clair | **bleu-gris froid**, et **orange** pour la sélection d'objet |
| **en-têtes de section** | onglets | **triangle de repli** `▾` + titre, sections empilées |
| **propriété** | libellé au-dessus du champ | **deux colonnes** : libellé à gauche, valeur à droite, alignées |
| **vecteur X/Y/Z** | trois champs neutres | **trois champs teintés** rouge / vert / bleu |
| **barre d'outils** | texte | **icône + libellé**, groupes séparés par un trait |

**La ligne de propriété à deux colonnes est la brique la plus structurante** : le
panneau Détails en est presque entièrement fait, et c'est elle qui donne à UE5
sa lisibilité en balayage vertical.

### Grammaire de couleurs — déjà établie dans le viewport, à ne pas réinventer

| notion | couleur |
|---|---|
| axe X / Y / Z | rouge / vert / bleu |
| **non sélectionné** | noir |
| **sélectionné** | orange `(1 · 0,55 · 0,05)` |
| **actif** | blanc |
| lumière | jaune, teinte claire si active |

**Trois états, pas deux.** La distinction sélectionné / actif existe partout dans
le viewport ; les panneaux et la hiérarchie doivent la respecter — dans une liste,
la ligne active se distingue des lignes seulement sélectionnées.

---

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

## 11. Ce que ce document ne fixe pas encore

Assumé, à trancher quand la question se posera :
- **format du fichier de projet** : un fichier ou un dossier (§ 4) ;
- **jeu d'icônes** : dessinées ou une police d'icônes ;
- **accessibilité** (contraste minimal, taille de police) : à instruire, pas à
  improviser ;
- **disposition par défaut à la première ouverture** : celle de la section 2, mais
  les proportions restent à régler à l'usage.
