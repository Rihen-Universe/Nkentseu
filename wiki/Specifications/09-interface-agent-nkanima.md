# NkAnima — l'interface, pour l'agent qui écrit les `.nkgui`

### Document 9 — Spécification d'interface côté agent

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> Ce document traduit le **08** (interface côté humain) en ce que tu as demandé :
> **l'arbre des éléments nommés**, **ce qui se répète** (composants), **ce qui
> agit** (actions), **ce qui est de la couleur et pourquoi** — plus ce que tu ne
> sais pas encore reproduire, écrit quand même.
>
> Référence du vocabulaire : *« Le vocabulaire complet de `.nkgui` »*, format
> `nkgui 0.3`, relevé le 26/09.
> **Si 08 et 09 se contredisent, 08 gagne** : signale l'écart, ne le tranche pas.

---

## 0. Comment lire ce document

### 0.1 La notation

```
Rôle "identifiant"          propriété: valeur   propriété: valeur
  Rôle "enfant"             …
```

- Les **rôles** sont ceux du vocabulaire `nkgui 0.3`. Un rôle marqué **✚** est
  une **proposition de vocabulaire** décrite au §1, avec ce qu'il faut écrire
  **aujourd'hui** à sa place.
- Un rôle en **PascalCase non standard** (`LigneNombre`, `CartePropo`…) est un
  **composant** défini au §3.
- **L'identifiant d'un élément qui déclenche est le nom de son action** (règle
  déjà en vigueur) : `Button`, `MenuItem`, `ToggleButton`, `Tile` et leurs
  composants. **Un élément qui porte une valeur** (`Slider`, `Dropdown`,
  `RadioGroup`, `TextField`, `Checkbox`…) a un identifiant **qui n'est pas une
  action** : sa vérité est son `bind:`. Ne les compte pas en `actionsInconnues`.
- `→ Host` = zone **remplie par l'application**, avec son `hint`.
- Une action marquée **✚** dans la table du §6 **n'existe pas encore** : écris-la
  quand même, elle sera grisée avec sa raison.
- Les icônes sont des noms **Lucide** (déjà la convention de NK3DModeler).

### 0.2 Les noms d'actions

- Les **42 actions `anim.*` servies aujourd'hui gardent leur nom exact**
  (`anim.annuler`, `anim.vue_solide`, `anim.selection_tout`…), même quand leur
  domaine n'est pas l'animation. Les renommer est une **décision séparée** : ne
  la prends pas ici.
- Les **nouvelles** actions sont rangées par domaine : `fichier.`, `edition.`,
  `vue.`, `espace.`, `outil.`, `squel.`, `poids.`, `phys.`, `traj.`, `fx.`,
  `ia.`, `agent.`, `capt.`, `biblio.`, `facile.`, `fenetre.`, `aide.`,
  `accueil.`.

### 0.3 Le style en une ligne

**Unreal Engine 5, net** : gris neutres très sombres, densité, angles à 2 px,
onglets à liseré, panneau Détails à catégories. **Toutes les couleurs viennent de
jetons de thème** (§2) ; aucune couleur en dur hors des jetons d'information.

---

## 1. Le vocabulaire proposé

Rodolf autorise à proposer des rôles et du vocabulaire, **à condition de les
décrire**. Voici ce qui manque pour cette application, par ordre d'importance.
Chaque entrée dit **pourquoi une composition existante ne suffit pas** et **quoi
écrire en attendant**.

### P1 — Les jetons de couleur `@nom`

- **Quoi** : partout où une couleur (`c`) est admise, accepter `@nom` — un
  jeton résolu par le thème (§2).
- **Pourquoi** : le style UE5 exige un thème Sombre **et** Clair ; une couleur
  d'information (l'orange « écrit ») doit rester lisible dans les deux. Une
  valeur en dur ne le peut pas.
- **En attendant** : écris la valeur hexadécimale du thème Sombre **et** le jeton
  en commentaire sur la même ligne : `fill { color = #F79A28 }  // @info.ecrit`.
  Le jour où P1 arrive, un remplacement mécanique suffit.

### P2 — `ToggleButton`

- **Propriétés** : `label` `icon` `bind` `group` (étiquette : les boutons d'un
  même groupe sont **exclusifs**).
- **Pourquoi** : les outils de la vue (un seul actif), la Clé auto, Facile/Expert,
  les surimpressions. Un `Checkbox` n'a pas la forme d'un bouton d'outil ; un
  `Button` n'a pas d'état.
- **Événement** : `Changed`. **États** : repos, survol, appui, **actif**
  (l'actif est peint avec `@accent`, sauf surcharge).
- **En attendant** : `Button` avec l'identifiant de l'action ; l'état actif est
  écrit en `appearance(Active)` (compté, non peint).

### P3 — `Badge`

- **Propriétés** : `text` (1 à 3 caractères, ou un nombre), `tone`
  (étiquette = nom de jeton sans `@`).
- **Pourquoi** : les pastilles d'**origine des clés** (M, P, T, C, I, A), le
  compteur de propositions en attente, l'état d'un agent. Non interactif,
  petite pilule. Un `Text` n'a pas de fond, et `fill` n'est pas peint sur `Text`.
- **En attendant** : `Text` avec `text { color }` du jeton.

### P4 — `Tile`

- **Propriétés** : `image` (vignette, éventuellement animée), `label`, `caption`,
  `size`. **Cliquable** ; l'identifiant est l'action.
- **Pourquoi** : les grilles de l'espace Facile (actions, personnages),
  l'accueil (projets, types de projet), la Bibliothèque. `ImageButton` n'a ni
  libellé ni légende, et n'est pas monté.
- **Événements** : `Click`, `Hover` (lance la vignette animée), `DragStart`
  (glisser un mouvement sur un personnage).
- **En attendant** : `Button` avec `label` et `tooltip` = légende.

### P5 — `VectorField`

- **Propriétés** : `bind` `components` (2, 3 ou 4) `speed` `min` `max`
  `axisColors` (booléen, vrai par défaut pour 3).
- **Pourquoi** : Transformation, dimensions, gravité, direction du vent — le champ
  X/Y/Z à liseré rouge/vert/bleu est **la** signature du panneau Détails UE5.
  La couleur porte l'axe : c'est de l'information.
- **En attendant** : composant `ChampVec3` (§3) fait de trois `Slider` ; les
  liserés d'axe écrits en `appearance` (comptés sur un rôle sans surface).

### P6 — `KeyDiamond`

- **Propriétés** : `bind` (chemin de la propriété animable, ex.
  `os.bras_g.rotation`).
- **États** : **vide** (non animé), **plein** (clé à l'image courante), **demi**
  (animé, pas de clé ici). Survol : `@info.ecrit` (il écrit).
- **Action** : un clic émet `anim.cle_propriete` avec le `bind` en argument.
- **Pourquoi** : chaque ligne animable du panneau Détails en porte un. Trois
  états visuels, un seul geste, un argument : ni `Button` ni `Checkbox`.
- **En attendant** : `Button` icône `diamond`, identifiant
  `anim.cle_propriete`, `tooltip` = le chemin.

### P7 — `CurveField`

- **Propriétés** : `bind` (une courbe), `min` `max` `height` `xLabel` `yLabel`.
- **Pourquoi** : la **vitesse le long d'une trajectoire**, la **part de physique
  dans le temps**, l'intensité d'un vent — une courbe éditable **en place** dans
  une ligne de Détails. `Chart` n'est pas éditable.
- **En attendant** : `Host` avec `hint`.

### P8 — `shortcut` universel

- **Quoi** : la propriété `shortcut` admise sur tout rôle interactif (elle ne
  l'est que sur `MenuItem`).
- **Pourquoi** : UE5 montre le raccourci dans l'infobulle des boutons d'outil ;
  le même raccourci doit se lire au même endroit qu'au menu.
- **Sens** : **affichage seulement**. La vérité des raccourcis reste dans
  l'application (sinon deux sources).
- **En attendant** : l'écrire dans `tooltip` : `"Déplacer (W)"`.

### P9 — `slot` dans un composant

- **Quoi** : un composant déclare `Slot "valeur"` ; l'instance fournit l'enfant
  qui prend cette place.
- **Pourquoi** : une **ligne de propriété** a toujours la même forme (étiquette,
  valeur, revenir au défaut, losange) mais sa **valeur** change de rôle (nombre,
  vecteur, liste, case, couleur, courbe). Sans emplacement, il faut un composant
  par type de valeur.
- **En attendant** : un composant par type — `LigneNombre`, `LigneVec3`,
  `LigneListe`, `LigneCase`, `LigneCouleur`, `LigneCourbe` (§3).

### P10 — La section `layout` (dispositions d'amarrage)

```
layout "animation" {
  dock "vue"         center
  dock "scene"       left   0.16
  dock "details"     right  0.22
  dock "sequenceur"  bottom 0.30
}
```

- **Pourquoi** : un espace de travail **est** une disposition. Aujourd'hui elle
  vivrait dans le C++ ; elle doit vivre dans le document, comme le reste.
- **En attendant** : écrire la section (elle voyage et se compte, comme
  `animation`), et l'application pose la disposition en dur.

### P11 — `appearance { pattern = Hatch }`

- **Quoi** : un motif hachuré sur une surface.
- **Pourquoi** : **une proposition non appliquée** (IA, physique, agent) doit se
  voir au premier coup d'œil, partout. La couleur seule ne suffit pas (daltonisme)
  et la hachure est déjà le langage du monteur pour « zone non servie » — ici,
  pour « pas encore à vous ».
- **En attendant** : écrit, compté.

### P12 — `icon` sur `TextField`

- Une loupe dans les champs de recherche, à gauche. Mineur ; en attendant, un
  `placeholder` « Rechercher… ».

### Ce que j'emploie du vocabulaire existant **non monté**

`NumberField`, `Drag`, `ColorField`, `Switch`, `RadioGroup`, `ContextMenu`,
`Table`, `Stack`, `ImageButton`. Je les écris là où ils sont le bon rôle — ils
seront comptés et non montés, **c'est voulu**. Là où un rôle monté fait
honnêtement l'affaire, je prends le rôle monté.

### Les événements que j'écris (lot b)

`on Click`, `on Changed`, `on Hover`, `on DragStart`, `on Drop`. Ils sont écrits
dans les `behavior` qui en ont besoin, **avec** une version idempotente à côté
pour aujourd'hui quand c'est possible (§7).

---

## 2. Les jetons de thème

Le thème s'appelle **« UE5 Rihen »**, en **Sombre** (défaut) et **Clair**.

### 2.1 Les jetons de structure — l'agent ne les écrit **jamais** dans un document

Ils sont le thème : fond d'application, de panneau, d'en-tête, de champ, de
survol, bord, texte, texte faible, accent. Valeurs indicatives au doc 08 §1.1.
**Aucun document d'interface n'écrit une de ces couleurs** : c'est le thème qui
les porte.

### 2.2 Les jetons d'information — les **seules** couleurs écrites

| jeton | Sombre | où | pourquoi |
|---|---|---|---|
| `@info.ecrit` | `#F79A28` | fond des boutons qui écrivent une clé ; survol des `KeyDiamond` ; bouton « Appliquer » d'une proposition | **ce bouton écrit** — empêcher un clic de trop |
| `@info.enregistrement` | `#E0303A` | `ToggleButton "anim.cle_auto"` actif | chaque geste écrit une clé : l'utilisateur doit le savoir en permanence |
| `@axe.x` `@axe.y` `@axe.z` | `#D2362E` `#5BA829` `#2F7FE0` | liserés des `VectorField` | l'axe |
| `@origine.main` | `#D8D8D8` | `Badge` d'origine « M » | d'où vient la clé |
| `@origine.physique` | `#2EC4D6` | « P » | idem |
| `@origine.trajectoire` | `#4CC26B` | « T » | idem |
| `@origine.captation` | `#E4C23A` | « C » | idem |
| `@origine.ia` | `#9B6BF2` | « I » | idem |
| `@origine.agent` | `#E05BB5` | « A » | idem |
| `@info.apercu` | `#9B6BF2` à 35 % + `Hatch` | cartes et surfaces de propositions non appliquées ; `Badge` du compteur en attente | pas encore à vous |
| `@info.simulation` | `#3FB950` | `ToggleButton "phys.simuler"` actif ; état dans la barre d'état | la vue n'est plus figée |
| `@info.alerte` | `#D29922` | somme de poids ≠ 1 ; pied qui glisse | à regarder |
| `@info.erreur` | `#F85149` | message d'erreur ; action impossible | cassé |
| `@espace.squelette` `.animation` `.physique` `.phenomenes` `.ia` `.captation` `.facile` | orange, bleu, cyan, violet clair, violet, jaune, vert | liseré de 2 px sous le nom de l'espace actif | se repérer d'un coup d'œil ; **nulle part ailleurs** |

📌 **Une couleur qui n'est pas dans ce tableau ne s'écrit pas.**

---

## 3. Les composants (ce qui se répète)

Chaque composant a **une seule racine**. Les identifiants internes seront
préfixés par l'instance.

### 3.1 `BoutonOutil` — un outil de la vue

```
component "BoutonOutil"
  ToggleButton✚ "bouton"      icon: "?"  group: outils_vue  tooltip: "?"
```
**Instances** : `outil.selection`, `outil.deplacer`, `outil.tourner`,
`outil.echelle`, `outil.trajectoire`, `outil.pinceau_poids`.
**Aujourd'hui** : `Button`.

### 3.2 `BoutonTransport` — déjà livré le 26/09

Réutilisé tel quel pour `anim.debut`, `anim.image_prec`, `anim.jouer`,
`anim.image_suiv`, `anim.fin`.

### 3.3 `BoutonEcrit` — un bouton qui écrit une clé

```
component "BoutonEcrit"
  Button "bouton"   label: "?"   icon: "?"
    appearance         radius: 2   fill: #F79A28 // @info.ecrit   text: #10222B
    appearance(Hover)  fill: #FFB04A
    appearance(Pressed) fill: #D98517
```
**Instances** : `anim.inserer`, `ia.appliquer` (dans `CartePropo`),
`agent.jouer_enregistrer`, `phys.cuire`, `traj.cuire`.
📌 Composant et non copie : **la couleur « écrit » ne doit exister qu'à un
endroit**.

### 3.4 `EnTetePanneau` — recherche en tête de panneau

```
component "EnTetePanneau"
  HBox "entete"                gap: 4  align: Center
    TextField "recherche"      placeholder: "Rechercher…"  icon✚: search  bind: ?
    Button "filtres"           icon: filter   tooltip: "Filtres"
    Button "options"           icon: settings-2   tooltip: "Options du panneau"
```
**Instances** : dans Scène, Arbre des os, Détails, Bibliothèque, Corps
physiques, Pile d'effets, Séquenceur (pistes).

### 3.5 `Categorie` — catégorie repliable du panneau Détails

```
component "Categorie"
  Expander "categorie"   label: "?"   expanded: true
    VBox "corps"         gap: 0
```
**Aujourd'hui** les lignes s'écrivent dans l'instance comme enfants de
`<instance>.corps` — si le développement ne le permet pas, c'est exactement le
besoin de **P9** : signale-le.

### 3.6 Les lignes de propriété — `LigneNombre`, `LigneVec3`, `LigneListe`, `LigneCase`, `LigneCouleur`, `LigneCourbe`

Même forme, valeur différente (voir **P9**) :

```
component "LigneNombre"
  Grid "ligne"             columns: 4   sizes: [0.40, 0.50, 0.05, 0.05]   gap: 4
    Text "etiquette"       text: "?"
    Drag "valeur"          bind: ?   speed: 0.1        // NumberField/Drag non montés : attendu
    Button "anim.reinitialiser_propriete"   icon: undo-2   tooltip: "Revenir à la valeur par défaut"   visible: false
    KeyDiamond✚ "cle"      bind: ?
```

| composant | rôle de `valeur` |
|---|---|
| `LigneNombre` | `Drag` (non monté) — aujourd'hui `Slider` |
| `LigneVec3` | `VectorField✚` — aujourd'hui `ChampVec3` |
| `LigneListe` | `Dropdown` |
| `LigneCase` | `Checkbox` |
| `LigneCouleur` | `ColorField` (non monté) |
| `LigneCourbe` | `CurveField✚` — aujourd'hui `Host` |

- `visible: false` sur la flèche de retour : elle n'apparaît que si la valeur a
  changé → **comportement** : `if valeur != defaut { reinitialiser.visible = true }`
  (idempotent, rejouable à chaque image ✅).
- `sizes` de `Grid` n'est **pas honoré** aujourd'hui : écris-le quand même.
- Les propriétés **non animables** (réglages de simulation, par exemple) : pas de
  `KeyDiamond` → `visible: false` sur `cle`.

### 3.7 `ChampVec3` — en attendant `VectorField`

```
component "ChampVec3"
  HBox "vec"            gap: 2
    Slider "x"          bind: ?.x    appearance: stroke-left #D2362E // @axe.x
    Slider "y"          bind: ?.y    appearance: stroke-left #5BA829 // @axe.y
    Slider "z"          bind: ?.z    appearance: stroke-left #2F7FE0 // @axe.z
```

### 3.8 `PastilleOrigine`

```
component "PastilleOrigine"
  Badge✚ "pastille"     text: "M"   tone: origine.main   tooltip: "Clé posée à la main"
```
**Six instances** types : M/main, P/physique, T/trajectoire, C/captation,
I/ia, A/agent.

### 3.9 `CartePropo` — une proposition (IA, physique, agent)

```
component "CartePropo"
  Panel "carte"                               appearance: fill #9B6BF2@35% // @info.apercu   pattern✚: Hatch   radius: 2
    VBox "corps"                              gap: 4
      Host "apercu"                           hint: "vignette animée de la proposition"
      Text "resume"                           text: "?"   wrap: true
      HBox "impact"                           gap: 4
        Badge✚ "nb_cles"                      text: "?"   tone: info.apercu
        Text "canaux"                         text: "?"   (« bras droit, colonne, 3 autres »)
      HBox "actions"                          gap: 4   justify: End
        Button "ia.apercu"                    label: "Aperçu"    icon: eye
        BoutonEcrit "ia.appliquer"            label: "Appliquer" icon: check
        Button "ia.rejeter"                   label: "Rejeter"   icon: x
```
Une carte **appliquée** perd le motif et le fond d'aperçu (état `Applied`,
§7).

### 3.10 `TuileAction` — une action de l'espace Facile

```
component "TuileAction"
  Tile✚ "tuile"         image: "?"   label: "?"   size: (96, 96)
```
**Aujourd'hui** : `Button` avec `label`.

### 3.11 `CurseurParlant` — un réglage de l'espace Facile

```
component "CurseurParlant"
  Grid "curseur"             columns: 3   sizes: [0.30, 0.40, 0.30]
    Text "gauche"            text: "?"   align: Right
    Slider "valeur"          bind: ?   min: 0   max: 1
    Text "droite"            text: "?"
```
Et un `Text` de titre **au-dessus**, hors composant (un composant = une racine).

### 3.12 `ModuleEffet` — un module de la Pile d'effets

```
component "ModuleEffet"
  Expander "module"          label: "?"   expanded: false
    VBox "corps"
      HBox "tete"            gap: 4
        Switch "actif"       bind: ?.actif   label: "Actif"      // non monté : attendu
        Button "fx.preregler"   label: "Préréglages ▾"
        Button "fx.supprimer_module"   icon: trash-2
```

### 3.13 `EnTeteEspace` — le liseré d'espace de travail

Sans objet dans un document aujourd'hui (le liseré se peint sous l'entrée du
sélecteur). Écris-le comme `appearance(Active) { stroke-bottom 2 @espace.* }`
sur chaque `MenuItem` du sélecteur : compté, non peint.

---

## 4. Les fichiers à produire

Dans `Resources/Interface/NkAnimaEditor/`. **Un fichier par panneau** : c'est le
grain auquel un panneau s'amarre, se ferme et se rouvre.

| fichier | remplace | contenu |
|---|---|---|
| `composants.nkgui` | — | les composants du §3 |
| `menus.nkgui` | `menu_animation.nkgui` | §5.1 |
| `barre_outils.nkgui` | idem (réécrit) | §5.2 |
| `barre_etat.nkgui` | idem (réécrit) | §5.3 |
| `vue_3d.nkgui` | — | §5.4 |
| `scene.nkgui` | — | §5.5 |
| `details.nkgui` | — | §5.6 (cadre) |
| `details_os.nkgui`, `details_objet.nkgui`, `details_corps.nkgui`, `details_visage.nkgui`, `details_trajectoire.nkgui`, `details_effet_vent.nkgui`, `details_agent.nkgui` | — | §5.6 — **l'application choisit le document selon le type de la sélection** |
| `sequenceur.nkgui` | — | §5.7 |
| `bibliotheque.nkgui` | — | §5.8 |
| `arbre_os.nkgui` | — | §5.9 |
| `poids.nkgui` | — | §5.10 |
| `corps_physiques.nkgui` | — | §5.11 |
| `pile_effets.nkgui` | — | §5.12 |
| `assistant_ia.nkgui` | — | §5.13 |
| `distribution.nkgui` | — | §5.14 |
| `texte_scene.nkgui` | — | §5.15 |
| `captation.nkgui` | — | §5.16 |
| `facile.nkgui` | — | §5.17 |
| `accueil.nkgui` | — | §5.18 |
| `dispositions.nkgui` | — | les sections `layout` (P10), §5.19 |
| `panneau_outils.nkgui` | **retiré** | son contenu est redistribué : transport → barre d'outils, clés → Séquenceur, pose → Détails de l'os, squelette → Arbre des os, physique → barre d'outils et espace Physique |

✅ **`include` est branché depuis le 26/09 à 18 h 54** (`include "composants.nkgui"`,
le contenu inclus prend la place exacte de la ligne ; à écrire **avant**
`widgets`). Les composants s'écrivent donc **une seule fois**, dans
`composants.nkgui`, et chaque document les inclut.

📌 Voir aussi le document 03 de NKScena §0 : les composants et panneaux
**communs** aux applications déménagent dans `Resources/Interface/Commun/`.

---

## 5. Les arbres

### 5.1 `menus.nkgui`

```
MenuBar "menus"
  Menu "menu_fichier"                         label: "Fichier"
    MenuItem "fichier.nouveau"                label: "Nouveau"               shortcut: "Ctrl+N"
    MenuItem "fichier.ouvrir"                 label: "Ouvrir…"               shortcut: "Ctrl+O"
    Menu "menu_recents"                       label: "Récents"               (rempli par l'application)
    Separator "sep_f1"
    MenuItem "fichier.enregistrer"            label: "Enregistrer"           shortcut: "Ctrl+S"
    MenuItem "fichier.enregistrer_sous"       label: "Enregistrer sous…"     shortcut: "Ctrl+Maj+S"
    Separator "sep_f2"
    Menu "menu_importer"                      label: "Importer"
      MenuItem "fichier.importer_maillage"    label: "Maillage (NK3DModeler, FBX, glTF)…"
      MenuItem "fichier.importer_mocap"       label: "Mocap (BVH, FBX)…"
      MenuItem "fichier.importer_video"       label: "Vidéo…"
      MenuItem "fichier.importer_audio"       label: "Audio de référence…"
    Menu "menu_exporter"                      label: "Exporter"
      MenuItem "fichier.exporter_clip"        label: "Clip d'animation…"
      MenuItem "fichier.exporter_squelette"   label: "Squelette et poids…"
      MenuItem "fichier.exporter_effet"       label: "Effet…"
      MenuItem "fichier.exporter_fbx"         label: "FBX…"
      MenuItem "fichier.exporter_gltf"        label: "glTF…"
    Separator "sep_f3"
    MenuItem "fichier.quitter"                label: "Quitter"               shortcut: "Ctrl+Q"

  Menu "menu_edition"                         label: "Édition"
    MenuItem "anim.annuler"                   label: "Annuler"               shortcut: "Ctrl+Z"
    MenuItem "anim.refaire"                   label: "Refaire"               shortcut: "Ctrl+Y"
    MenuItem "edition.historique"             label: "Historique"
    Separator "sep_e1"
    MenuItem "anim.copier"                    label: "Copier"                shortcut: "Ctrl+C"
    MenuItem "anim.coller"                    label: "Coller"                shortcut: "Ctrl+V"
    MenuItem "edition.dupliquer"              label: "Dupliquer"             shortcut: "Ctrl+D"
    MenuItem "edition.supprimer"              label: "Supprimer"             shortcut: "Suppr"
    Separator "sep_e2"
    MenuItem "edition.ajuster_derniere"       label: "Ajuster la dernière opération"   shortcut: "F9"
    MenuItem "edition.preferences"            label: "Préférences…"          shortcut: "Ctrl+,"

  Menu "menu_selection"                       label: "Sélection"
    MenuItem "anim.selection_tout"            label: "Tout"                  shortcut: "A"
    MenuItem "anim.selection_rien"            label: "Rien"                  shortcut: "Alt+A"
    MenuItem "anim.selection_inverser"        label: "Inverser"              shortcut: "Ctrl+I"
    MenuItem "selection.enfants"              label: "Os enfants"
    MenuItem "selection.symetrique"           label: "Symétrique"
    MenuItem "selection.meme_origine"         label: "Clés de même origine"

  Menu "menu_vue"                             label: "Vue"
    MenuItem "anim.vue_solide"                label: "Solide"                shortcut: "Z"
    MenuItem "anim.vue_rendu"                 label: "Éclairé"
    MenuItem "anim.vue_filaire"               label: "Filaire"
    Separator "sep_v1"
    Menu "menu_afficher"                      label: "Afficher"
      MenuItem "vue.os"                       label: "Os"                    checked: true
      MenuItem "vue.noms_os"                  label: "Noms des os"
      MenuItem "vue.controleurs"              label: "Contrôleurs"           checked: true
      MenuItem "vue.trajectoires"             label: "Trajectoires"
      MenuItem "vue.pelures"                  label: "Peaux d'oignon"
      MenuItem "vue.centre_masse"             label: "Centre de masse"
      MenuItem "vue.contacts"                 label: "Contacts au sol"
      MenuItem "vue.forces"                   label: "Forces"
      MenuItem "vue.collisions"               label: "Formes de collision"
      MenuItem "vue.carte_poids"              label: "Poids de peau"
    MenuItem "anim.compteurs"                 label: "Compteurs de rendu"

  Menu "menu_squelette"                       label: "Squelette"
    Menu "menu_gabarits"                      label: "Poser un gabarit"
      MenuItem "squel.gabarit_bipede"         label: "Bipède"
      MenuItem "squel.gabarit_quadrupede"     label: "Quadrupède"
      MenuItem "squel.gabarit_oiseau"         label: "Oiseau"
      MenuItem "squel.gabarit_poisson"        label: "Poisson"
      MenuItem "squel.gabarit_serpent"        label: "Serpent"
      MenuItem "squel.gabarit_insecte"        label: "Insecte"
      MenuItem "squel.gabarit_vide"           label: "Vide"
    MenuItem "squel.proposer_ia"              label: "Proposer par IA"
    MenuItem "squel.ajouter_os"               label: "Ajouter un os"
    Menu "menu_contraintes"                   label: "Contrainte"
      MenuItem "squel.contrainte_ik2"         label: "IK à deux os"
      MenuItem "squel.contrainte_chaine"      label: "Chaîne IK"
      MenuItem "squel.contrainte_colonne"     label: "IK de colonne"
      MenuItem "squel.contrainte_visee"       label: "Visée"
      MenuItem "squel.contrainte_copie"       label: "Copie de rotation"
    MenuItem "squel.symetriser"               label: "Symétriser"
    MenuItem "squel.lier"                     label: "Lier au maillage"
    Separator "sep_s1"
    MenuItem "poids.automatiques"             label: "Poids automatiques"
    MenuItem "poids.normaliser"               label: "Normaliser les poids"
    MenuItem "squel.tester_deformation"       label: "Tester la déformation"

  Menu "menu_animation"                       label: "Animation"
    MenuItem "anim.jouer"                     label: "Jouer / Pause"         shortcut: "Espace"
    MenuItem "anim.debut"                     label: "Aller au début"        shortcut: "Début"
    MenuItem "anim.fin"                       label: "Aller à la fin"        shortcut: "Fin"
    MenuItem "anim.image_prec"                label: "Image précédente"      shortcut: "Gauche"
    MenuItem "anim.image_suiv"                label: "Image suivante"        shortcut: "Droite"
    MenuItem "anim.cle_prec"                  label: "Clé précédente"        shortcut: "Haut"
    MenuItem "anim.cle_suiv"                  label: "Clé suivante"          shortcut: "Bas"
    MenuItem "anim.plage"                     label: "Définir la plage de lecture"
    Separator "sep_a1"
    MenuItem "anim.inserer"                   label: "Insérer une clé"       shortcut: "I"
    MenuItem "anim.supprimer"                 label: "Supprimer la clé"      shortcut: "Suppr"
    MenuItem "anim.cle_auto"                  label: "Clé auto"              checked: false
    Menu "menu_pose"                          label: "Pose"
      MenuItem "anim.pose_entrer"             label: "Éditer la pose"        shortcut: "Tab"
      MenuItem "anim.pose_enregistrer"        label: "Enregistrer la pose en clé"   shortcut: "Ctrl+Entrée"   // était Ctrl+S : Ctrl+S = Enregistrer le projet
      MenuItem "anim.pose_quitter"            label: "Quitter sans enregistrer"     shortcut: "Échap"
      MenuItem "anim.pose_ik"                 label: "Cinématique inverse (IK)"
      MenuItem "anim.pose_miroir"             label: "Miroir de la pose"
      MenuItem "anim.pose_assistee"           label: "Pose assistée"         checked: true
    MenuItem "outil.trajectoire"              label: "Trajectoire"           shortcut: "T"
    Menu "menu_locomotion"                    label: "Locomotion"
      MenuItem "anim.loco_marcher"            label: "Marcher"
      MenuItem "anim.loco_courir"             label: "Courir"
      MenuItem "anim.loco_sauter"             label: "Sauter"
      MenuItem "anim.loco_grimper"            label: "Grimper"
      MenuItem "anim.loco_nager"              label: "Nager"
      MenuItem "anim.loco_ramper"             label: "Ramper"
      MenuItem "anim.loco_custom"             label: "Depuis mes clips…"
    MenuItem "anim.retarget"                  label: "Reciblage…"
    MenuItem "anim.cuire"                     label: "Cuire l'animation"

  Menu "menu_physique"                        label: "Physique"
    MenuItem "phys.generer_corps"             label: "Générer les corps"
    MenuItem "phys.corriger"                  label: "Corriger l'animation"
    MenuItem "phys.simuler"                   label: "Simuler"               checked: false
    MenuItem "phys.cuire"                     label: "Cuire"
    MenuItem "phys.reglages_monde"            label: "Réglages du monde…"

  Menu "menu_phenomenes"                      label: "Phénomènes"
    Menu "menu_ajouter_fx"                    label: "Ajouter"
      MenuItem "fx.ajouter_vent"              label: "Vent"
      MenuItem "fx.ajouter_particules"        label: "Particules"
      MenuItem "fx.ajouter_granulaire"        label: "Sable / granulaire"
      MenuItem "fx.ajouter_destruction"       label: "Destruction"
      MenuItem "fx.ajouter_fumee"             label: "Fumée et feu"
      MenuItem "fx.ajouter_eau"               label: "Eau"
      MenuItem "fx.ajouter_ocean"             label: "Océan"
      MenuItem "fx.ajouter_tissu"             label: "Tissu et cheveux"
    MenuItem "fx.declencheur"                 label: "Ajouter un déclencheur"
    MenuItem "fx.cuire"                       label: "Cuire le cache"

  Menu "menu_ia"                              label: "IA"
    MenuItem "ia.assistant"                   label: "Assistant"             shortcut: "Ctrl+K"
    MenuItem "ia.intervalles"                 label: "Intervalles"
    MenuItem "ia.corriger"                    label: "Corriger"
    Menu "menu_style"                         label: "Style"                 (rempli par l'application)
    MenuItem "ia.texte_mouvement"             label: "Texte → mouvement"
    MenuItem "ia.voix_visage"                 label: "Voix → visage"
    Separator "sep_i1"
    MenuItem "agent.distribution"             label: "Distribution des rôles"
    MenuItem "agent.jouer_scene"              label: "Jouer la scène"

  Menu "menu_fenetre"                         label: "Fenêtre"
    Menu "menu_espaces"                       label: "Espaces de travail"
      MenuItem "espace.facile"                label: "Facile"
      MenuItem "espace.squelette"             label: "Squelette"
      MenuItem "espace.animation"             label: "Animation"
      MenuItem "espace.physique"              label: "Physique"
      MenuItem "espace.phenomenes"            label: "Phénomènes"
      MenuItem "espace.ia"                    label: "IA & Agents"
      MenuItem "espace.captation"             label: "Captation"
    Separator "sep_w1"
    MenuItem "fenetre.scene"                  label: "Scène"                 checked: true
    MenuItem "fenetre.details"                label: "Détails"               checked: true
    MenuItem "fenetre.sequenceur"             label: "Séquenceur"            checked: true
    MenuItem "biblio.ouvrir"                  label: "Bibliothèque"          shortcut: "Ctrl+Espace"
    MenuItem "fenetre.arbre_os"               label: "Arbre des os"
    MenuItem "fenetre.poids"                  label: "Poids"
    MenuItem "fenetre.corps"                  label: "Corps physiques"
    MenuItem "fenetre.pile_effets"            label: "Pile d'effets"
    MenuItem "ia.assistant"                   (déjà au menu IA ; ici case cochée)
    MenuItem "fenetre.distribution"           label: "Distribution"
    MenuItem "fenetre.captation"              label: "Captation"
    MenuItem "fenetre.journal"                label: "Journal"
    Separator "sep_w2"
    MenuItem "fenetre.reinitialiser"          label: "Réinitialiser la disposition"

  Menu "menu_aide"                            label: "Aide"
    MenuItem "anim.aide_raccourcis"           label: "Raccourcis"
    MenuItem "aide.documentation"             label: "Documentation"
    MenuItem "anim.apropos"                   label: "Crédits et licences / À propos"
```

⚠️ `ia.assistant` apparaît deux fois (menu IA et Fenêtre) avec le **même
identifiant** : si le monteur l'interdit, renomme la seconde
`fenetre.assistant_ia` et signale-le.

### 5.2 `barre_outils.nkgui`

```
HBox "barre_outils"                           gap: 4   align: Center
  Dropdown "espace.choix"                     bind: espace.actif
                                              items: ["Facile","Squelette","Animation","Physique","Phénomènes","IA & Agents","Captation"]
  Separator "sep_espace"                      orientation: Vertical

  HBox "outils"                               gap: 2
    BoutonOutil "outil.selection"             icon: mouse-pointer-2   tooltip: "Sélectionner (Q)"
    BoutonOutil "outil.deplacer"              icon: move              tooltip: "Déplacer (W)"
    BoutonOutil "outil.tourner"               icon: rotate-3d         tooltip: "Tourner (E)"
    BoutonOutil "outil.echelle"               icon: scaling           tooltip: "Mettre à l'échelle (R)"
    BoutonOutil "outil.trajectoire"           icon: spline            tooltip: "Trajectoire (T)"
    BoutonOutil "outil.pinceau_poids"         icon: brush             tooltip: "Pinceau de poids (B)"   visible: false
  Separator "sep_outils"                      orientation: Vertical

  HBox "transport"                            gap: 2
    BoutonTransport "anim.debut"              label: "|<"   icon: skip-back
    BoutonTransport "anim.image_prec"         label: "<"    icon: chevron-left
    BoutonTransport "anim.jouer"              label: "Jouer / Pause"   icon: play
    BoutonTransport "anim.image_suiv"         label: ">"    icon: chevron-right
    BoutonTransport "anim.fin"                label: ">|"   icon: skip-forward
    ToggleButton✚ "anim.boucle"               icon: repeat   bind: anim.boucle   tooltip: "Jouer en boucle"
    NumberField "image_courante"              bind: anim.image   valueType: int   (non monté : attendu)
  Separator "sep_transport"                   orientation: Vertical

  HBox "cles"                                 gap: 2
    BoutonEcrit "anim.inserer"                label: "Clé"   icon: diamond-plus   tooltip: "Insérer une clé (I)"
    Button "anim.supprimer"                   icon: diamond-minus   tooltip: "Supprimer la clé (Suppr)"
    ToggleButton✚ "anim.cle_auto"             icon: circle-dot   bind: anim.cle_auto   tooltip: "Clé auto — chaque geste écrit une clé"
  Separator "sep_cles"                        orientation: Vertical

  HBox "physique"                             gap: 4
    Text "part_titre"                         text: "Physique"
    Slider "phys.part"                        bind: phys.part_selection   min: 0   max: 100   step: 1
    ToggleButton✚ "phys.simuler"              icon: atom   bind: phys.simule   tooltip: "Simuler"
  Spacer "pousse"                             (occupe le reste)

  HBox "droite"                               gap: 4
    Button "ia.assistant"                     icon: sparkles   tooltip: "Assistant IA (Ctrl+K)"
    Badge✚ "ia.en_attente"                    text: "0"   tone: info.apercu   tooltip: "Propositions en attente"
    RadioGroup "espace.niveau"                bind: espace.niveau   options: ["Facile","Expert"]   orientation: Horizontal
```

**Couleurs** : `anim.inserer` → `@info.ecrit` (par le composant) ;
`anim.cle_auto` actif → `appearance(Active) fill @info.enregistrement` ;
`phys.simuler` actif → `appearance(Active) fill @info.simulation`.
**Comportements** :
- `outil.pinceau_poids.visible = (espace.actif == "Squelette")` — idempotent ✅.
- `ia.en_attente.visible = (ia.nb_en_attente > 0)` ; `text = ia.nb_en_attente` — idempotent ✅.

### 5.3 `barre_etat.nkgui`

```
HBox "barre_etat"                             gap: 8   align: Center
  Button "biblio.ouvrir"                      icon: library   label: "Bibliothèque"   tooltip: "Ctrl+Espace"
  Button "fenetre.journal"                    icon: scroll-text   label: "Journal"
  TextField "commande"                        placeholder: "> commande"   bind: commande.saisie
  Spacer "pousse"
  Text "etat_simulation"                      text: "Simulation : au repos"
  Text "fps"                                  text: "— ips"
  Text "os"                                   text: "— os"
  Text "memoire"                              text: "— Mo"
```
**Couleur** : `etat_simulation.text.color = @info.simulation` quand elle tourne
(comportement idempotent).
**Événement (lot b)** : `on Submit commande` → exécuter la commande tapée
(**non idempotent** : ne pas l'émuler à chaque image).

### 5.4 `vue_3d.nkgui`

```
Panel "vue"                                   title: "Vue 3D"   placement: absolute
  Host "vue.rendu"                            hint: "viseur 3D (rempli par l'application)"   sizeRel: (1, 1)
  HBox "barre_vue"                            pos: (8, 8)   gap: 2          ← surimpression, comme UE5
    Dropdown "vue.camera"                     bind: vue.camera   items: ["Perspective","Dessus","Face","Côté","Caméra de scène"]
    Dropdown "vue.mode"                       bind: vue.mode     items: ["Solide","Éclairé","Filaire"]
    Button "vue.afficher"                     label: "Afficher ▾"   (ouvre le même menu que Vue ▸ Afficher)
    ToggleButton✚ "vue.aimant"                icon: magnet   bind: vue.aimant   tooltip: "Aimantation"
    Slider "vue.vitesse_camera"               bind: vue.vitesse   min: 1   max: 8   step: 1   tooltip: "Vitesse de la caméra"
  Text "vue.hud"                              pos: (8, 36)   text: "image 0 — (rien)"   (rempli par comportement)
```
⚠️ `HBox` sous un `Panel` en `placement: absolute` : `pos` est **admis** (le
parent est absolu). Si le monteur ne superpose pas un enfant sur un `Host`,
**signale-le** : la barre de vue en surimpression est un trait UE5 voulu.

### 5.5 `scene.nkgui`

```
Panel "scene"                                 title: "Scène"
  VBox "colonne"                              gap: 4
    EnTetePanneau "scene.entete"
    HBox "scene.filtres"                      gap: 2
      ToggleButton✚ "scene.filtre_perso"      icon: person-standing   bind: scene.filtre.perso    tooltip: "Personnages"
      ToggleButton✚ "scene.filtre_objet"      icon: box               bind: scene.filtre.objet    tooltip: "Objets"
      ToggleButton✚ "scene.filtre_fx"         icon: wind              bind: scene.filtre.fx       tooltip: "Phénomènes"
      ToggleButton✚ "scene.filtre_agent"      icon: bot               bind: scene.filtre.agent    tooltip: "Agents"
      ToggleButton✚ "scene.filtre_traj"       icon: spline            bind: scene.filtre.traj     tooltip: "Trajectoires"
    Host "scene.arbre"                        hint: "arbre de la scène : œil, cadenas, nom, type, pastille d'origine (rempli par l'application)"
  ContextMenu "scene.menu"                    (non monté : attendu)
    MenuItem "edition.dupliquer"              label: "Dupliquer"
    MenuItem "edition.renommer"               label: "Renommer"           shortcut: "F2"
    MenuItem "edition.supprimer"              label: "Supprimer"
    MenuItem "scene.isoler"                   label: "Isoler"
    MenuItem "traj.attacher"                  label: "Attacher à une trajectoire…"
```
📌 **L'arbre est un `Host`** : le nombre de lignes, les colonnes œil/cadenas et
les pastilles dépendent des données. Quand `Table` sera monté, il pourra
remplacer le `Host` ; pas avant.

### 5.6 `details.nkgui` et les documents par type de sélection

**Cadre commun** (`details.nkgui`) :

```
Panel "details"                               title: "Détails"
  VBox "colonne"                              gap: 4
    HBox "details.identite"                   gap: 4   align: Center
      Image "details.icone"                   source: "?"   size: (16, 16)
      TextField "details.nom"                 bind: selection.nom
      PastilleOrigine "details.origine"       (texte et ton selon la sélection)
    EnTetePanneau "details.entete"
    Scroll "details.defilement"               axis: Vertical
      Host "details.contenu"                  hint: "le document details_<type> de la sélection (monté par l'application)"
```
L'application monte **dans** `details.contenu` le document qui correspond au type
de la sélection. Chaque document est une `VBox "racine"` de `Categorie`.

#### `details_os.nkgui`

```
VBox "racine"
  Categorie "cat_transformation"              label: "Transformation"
    LigneVec3 "os.position"                   etiquette: "Position"   bind: os.position
    LigneVec3 "os.rotation"                   etiquette: "Rotation"   bind: os.rotation
    LigneVec3 "os.echelle"                    etiquette: "Échelle"    bind: os.echelle
    LigneListe "os.espace"                    etiquette: "Espace"     bind: os.espace   items: ["Local","Monde","Parent"]
  Categorie "cat_ik"                          label: "IK / FK"
    LigneNombre "os.melange_ik"               etiquette: "Mélange IK ↔ FK"   bind: os.ik_fk   min: 0   max: 1
    LigneListe "os.contrainte_type"           etiquette: "Contrainte"   bind: os.contrainte   items: ["Aucune","IK à deux os","Chaîne IK","IK de colonne","Visée","Copie de rotation"]
    LigneListe "os.contrainte_cible"          etiquette: "Cible"        bind: os.cible   (items remplis par l'application)
    LigneNombre "os.contrainte_poids"         etiquette: "Poids"        bind: os.contrainte_poids   min: 0   max: 1
  Categorie "cat_limites"                     label: "Limites d'angle"
    LigneVec3 "os.limite_min"                 etiquette: "Minimum"      bind: os.limite_min
    LigneVec3 "os.limite_max"                 etiquette: "Maximum"      bind: os.limite_max
    Host "os.cone"                            hint: "cône de débattement (rempli par l'application)"
  Categorie "cat_physique_os"                 label: "Physique"
    LigneCourbe "os.part_physique"            etiquette: "Part de physique"   bind: os.part_physique   min: 0   max: 100
  Categorie "cat_structure"                   label: "Structure"          expanded: false
    LigneNombre "os.longueur"                 etiquette: "Longueur"     bind: os.longueur
    LigneListe "os.parent"                    etiquette: "Parent"       bind: os.parent
    LigneListe "os.controleur_forme"          etiquette: "Contrôleur"   bind: os.controleur   items: ["Aucun","Anneau","Flèche","Boîte","Sphère"]
  Categorie "cat_origine"                     label: "Origine des clés"   expanded: false
    Host "os.origines"                        hint: "pastilles d'origine par canal (lecture seule)"
```
Les lignes de `cat_structure` sont **non animables** → `cle.visible = false`.

#### `details_objet.nkgui`

`Transformation` (comme l'os) · `Physique` (Type : Aucun / Actif / Passif /
Cinématique ; Masse ; Frottement ; Rebond ; Part de physique en courbe) ·
`Destruction` (Destructible ; Morceaux ; Seuil de rupture) · `Origine des clés`.

#### `details_corps.nkgui`

```
VBox "racine"
  Categorie "cat_forme"                       label: "Forme"
    LigneListe "corps.forme"                  etiquette: "Forme"   bind: corps.forme   items: ["Capsule","Boîte","Sphère","Convexe"]
    LigneVec3 "corps.dimensions"              etiquette: "Dimensions"   bind: corps.dimensions
  Categorie "cat_comportement"                label: "Comportement"
    LigneListe "corps.type"                   etiquette: "Type"    bind: corps.type   items: ["Actif","Passif","Cinématique"]
    LigneNombre "corps.maintien"              etiquette: "Maintien de la pose"   bind: corps.maintien   min: 0   max: 1
  Categorie "cat_matiere"                     label: "Matière"
    LigneNombre "corps.masse"                 etiquette: "Masse (kg)"   bind: corps.masse
    LigneNombre "corps.frottement"            etiquette: "Frottement"   bind: corps.frottement   min: 0   max: 1
    LigneNombre "corps.rebond"                etiquette: "Rebond"       bind: corps.rebond       min: 0   max: 1
    LigneNombre "corps.amortissement"         etiquette: "Amortissement"   bind: corps.amortissement
  Categorie "cat_liaison"                     label: "Liaison"
    LigneVec3 "corps.limites"                 etiquette: "Limites (balancement, torsion)"   bind: corps.limites
    LigneNombre "corps.rupture"               etiquette: "Seuil de rupture"   bind: corps.rupture
```
`corps.maintien.visible = (corps.type == "Actif")` — idempotent ✅.

#### `details_visage.nkgui`

```
VBox "racine"
  Categorie "cat_expressions"                 label: "Expressions"
    Flow "visage.expressions"                 gap: 4
      Button "visage.joie"                    label: "Joie"
      Button "visage.colere"                  label: "Colère"
      Button "visage.peur"                    label: "Peur"
      Button "visage.surprise"                label: "Surprise"
      Button "visage.degout"                  label: "Dégoût"
      Button "visage.tristesse"               label: "Tristesse"
  Categorie "cat_unites"                      label: "Unités d'action"
    LigneNombre "visage.sourcils"             etiquette: "Sourcils"    bind: visage.au.sourcils    min: -1   max: 1
    LigneNombre "visage.paupieres"            etiquette: "Paupières"   bind: visage.au.paupieres   min: 0    max: 1
    LigneNombre "visage.joues"                etiquette: "Joues"       bind: visage.au.joues       min: 0    max: 1
    LigneNombre "visage.levres"               etiquette: "Lèvres"      bind: visage.au.levres      min: -1   max: 1
    LigneNombre "visage.machoire"             etiquette: "Mâchoire"    bind: visage.au.machoire    min: 0    max: 1
    Host "visage.autres"                      hint: "les autres unités d'action du personnage (remplies par l'application)"
  HBox "visage.fin"                           justify: End
    Button "ia.voix_visage"                   label: "Depuis la voix"   icon: audio-lines
```
Les boutons d'expression **posent une pose faciale** sans écrire de clé (la clé
se pose ensuite, ou par Clé auto) : ils ne sont **pas** orange.
Les actions `visage.*` sont toutes ✚.

#### `details_trajectoire.nkgui`

```
VBox "racine"
  Categorie "cat_elements"                    label: "Éléments attachés"
    Host "traj.liste"                         hint: "éléments attachés (rempli par l'application)"
    HBox "traj.boutons"                       gap: 4
      Button "traj.attacher"                  label: "Attacher la sélection"   icon: link
      Button "traj.detacher"                  label: "Détacher"                icon: unlink
  Categorie "cat_forme_traj"                  label: "Tracé"
    LigneCase "traj.au_sol"                   etiquette: "Coller au sol"   bind: traj.au_sol
    LigneListe "traj.orientation"             etiquette: "Orientation"   bind: traj.orientation   items: ["Suivre la tangente","Fixe","Vers une cible"]
    LigneListe "traj.cible"                   etiquette: "Cible"   bind: traj.cible
  Categorie "cat_temps"                       label: "Temps"
    LigneNombre "traj.depart"                 etiquette: "Image de départ"   bind: traj.depart
    LigneNombre "traj.duree"                  etiquette: "Durée (images)"    bind: traj.duree
    LigneCourbe "traj.vitesse"                etiquette: "Vitesse le long du tracé"   bind: traj.vitesse
    LigneNombre "traj.decalage"               etiquette: "Décalage entre éléments"    bind: traj.decalage
  Categorie "cat_locomotion"                  label: "Locomotion"
    LigneListe "traj.locomotion"              etiquette: "Allure"   bind: traj.locomotion   items: ["Aucune","Marcher","Courir","Ramper","Nager","Voler"]
  Categorie "cat_physique_traj"               label: "Physique en chemin"
    RadioGroup "traj.qui_gagne"               bind: traj.qui_gagne   options: ["La courbe gagne","La physique gagne"]
    LigneNombre "traj.raideur"                etiquette: "Rappel vers la courbe"   bind: traj.raideur   min: 0   max: 1
  HBox "traj.fin"                             justify: End
    BoutonEcrit "traj.cuire"                  label: "Cuire en clés"   icon: diamond-plus
```
`traj.cible.visible = (traj.orientation == "Vers une cible")` — idempotent ✅.

#### `details_effet_vent.nkgui` (modèle pour les autres effets)

`Général` (Actif ; Intensité — **animable** ; Direction — `LigneVec3`, animable ;
Rafales ; Turbulence) · `Portée` (Globale / Volume ; Dimensions) · `Agit sur`
(`LigneCase` : Tissu, Cheveux, Végétation, Particules, Granulaire, Fumée) ·
`Déclencheur` (Aucun / À l'image / Au contact / Au seuil ; valeur).
Les autres effets (`particules`, `granulaire`, `destruction`, `fumee`, `eau`,
`ocean`, `tissu`) suivent **le même patron** : Général · spécifique · Agit sur ·
Déclencheur · Cache. Écris-les au fil des paliers P4/P6 du doc 07.

#### `details_agent.nkgui`

`Rôle` (TextField multiligne `agent.role` ; Humeur de départ `Dropdown` ;
Objectif `TextField`) · `Voix` (`Dropdown` ; `LigneNombre` hauteur, débit) ·
`Jeu` (`LigneNombre` : expressivité, amplitude des gestes, improvisation) ·
`Pour le jeu vidéo` (`Button "agent.exporter_comportement"`).

### 5.7 `sequenceur.nkgui`

```
Panel "sequenceur"                            title: "Séquenceur"
  VBox "colonne"                              gap: 0
    HBox "seq.tete"                           gap: 4   align: Center
      TabBar "seq.onglets"                    bind: seq.vue   tabs: ["Feuille d'expo","Courbes","Couches","Audio"]
      Spacer "pousse"
      Dropdown "seq.unite"                    bind: seq.unite   items: ["Images","Secondes"]
      ToggleButton✚ "seq.normaliser"          icon: fold-vertical   bind: seq.normaliser   tooltip: "Normaliser les courbes"   visible: false
      Dropdown "seq.interpolation"            bind: seq.interpolation   items: ["Constante","Linéaire","Bézier"]   visible: false
      Button "seq.cadrer"                     icon: scan   tooltip: "Cadrer (F)"
    Splitter "seq.partage"                    bind: seq.ratio   orientation: Vertical   ratio: 0.22   min: 0.12   max: 0.45
      VBox "seq.pistes"                       gap: 2
        EnTetePanneau "seq.entete"
        Host "seq.liste_pistes"               hint: "pistes : nom, œil, cadenas, pastille d'origine (rempli par l'application)"
      Host "seq.canevas"                      hint: "règle du temps, tête de lecture, plage, clés / courbes / blocs / onde selon l'onglet (rempli par l'application)"
```
- `seq.normaliser.visible` et `seq.interpolation.visible` = `(seq.vue == "Courbes")` — idempotent ✅.
- La couleur des clés **dans** le canevas (par origine) est peinte par
  l'application, avec les jetons `@origine.*` : elle n'est pas dans le document.

### 5.8 `bibliotheque.nkgui`

```
Panel "bibliotheque"                          title: "Bibliothèque"
  Splitter "biblio.partage"                   orientation: Vertical   ratio: 0.2
    VBox "biblio.gauche"
      EnTetePanneau "biblio.entete"
      ListBox "biblio.categories"             bind: biblio.categorie
                                              items: ["Mouvements","Poses","Expressions","Effets","Squelettes","Comportements"]
      Host "biblio.dossiers"                  hint: "arborescence des dossiers"
    VBox "biblio.droite"
      HBox "biblio.barre"                     gap: 4
        Button "biblio.importer"              label: "Importer…"   icon: download
        Slider "biblio.taille"                bind: biblio.taille_vignettes   min: 48   max: 160
      Scroll "biblio.defilement"
        Flow "biblio.grille"                  gap: 6          (les Tile sont ajoutées par l'application)
```
**Événements (lot b)** : `on Hover` d'une `Tile` → jouer la vignette ;
`on DragStart` → glisser un mouvement vers la Vue ou une piste.

### 5.9 `arbre_os.nkgui`

```
Panel "arbre_os"                              title: "Arbre des os"
  VBox "colonne"                              gap: 4
    HBox "os.barre"                           gap: 2
      Button "squel.gabarit"                  label: "Gabarit ▾"   icon: person-standing   (ouvre Squelette ▸ Poser un gabarit)
      Button "squel.proposer_ia"              label: "Proposer par IA"   icon: sparkles
      Button "squel.ajouter_os"               icon: plus   tooltip: "Ajouter un os enfant"
      Button "squel.symetriser"               icon: flip-horizontal-2   tooltip: "Symétriser"
      Button "squel.pose_reference"           icon: rotate-ccw   tooltip: "Pose de référence"
      Button "squel.lier"                     label: "Lier au maillage"   icon: link
      Button "squel.tester_deformation"       icon: activity   tooltip: "Tester la déformation"
    EnTetePanneau "os.entete"
    Host "os.arbre"                           hint: "hiérarchie des os : icône, nom, œil, cadenas (rempli par l'application)"
```

### 5.10 `poids.nkgui`

```
Panel "poids"                                 title: "Poids"
  VBox "colonne"                              gap: 4
    HBox "poids.os_actif"                     gap: 4
      Text "poids.os_titre"                   text: "Os actif"
      Text "poids.os_nom"                     text: "—"
    RadioGroup "poids.mode"                   bind: poids.mode   options: ["Ajouter","Retirer","Lisser","Égaliser"]   orientation: Horizontal
    LigneNombre "poids.rayon"                 etiquette: "Rayon"   bind: poids.rayon
    LigneNombre "poids.force"                 etiquette: "Force"   bind: poids.force   min: 0   max: 1
    HBox "poids.somme"                        gap: 4
      Text "poids.somme_titre"                text: "Somme au sommet"
      Text "poids.somme_valeur"               text: "—"
    HBox "poids.boutons"                      gap: 4
      Button "poids.automatiques"             label: "Poids automatiques"   icon: wand-2
      Button "poids.normaliser"               label: "Normaliser"           icon: equal
```
**Couleur** : `poids.somme_valeur.text.color = @info.alerte` si la somme ≠ 1
— **c'est une information** (comportement idempotent ✅).
Les lignes `rayon` et `force` : `cle.visible = false` (réglages d'outil).

### 5.11 `corps_physiques.nkgui`

```
Panel "corps_physiques"                       title: "Corps physiques"
  VBox "colonne"                              gap: 4
    HBox "phys.barre"                         gap: 2
      Button "phys.generer_corps"             label: "Générer les corps"   icon: shapes
      Button "phys.corriger"                  label: "Corriger l'animation"   icon: sparkles   tooltip: "Proposition en aperçu"
      ToggleButton✚ "phys.simuler"            icon: atom   bind: phys.simule   tooltip: "Simuler"
      BoutonEcrit "phys.cuire"                label: "Cuire"   icon: diamond-plus
    Categorie "phys.monde"                    label: "Monde"   expanded: false
      LigneVec3 "phys.gravite"                etiquette: "Gravité"   bind: phys.gravite
      LigneNombre "phys.pas"                  etiquette: "Pas de temps (s)"   bind: phys.pas
      LigneNombre "phys.graine"               etiquette: "Graine"   bind: phys.graine   tooltip: "Même graine, même résultat"
    EnTetePanneau "phys.entete"
    Host "phys.liste"                         hint: "corps par os / objet : forme, actif, type (rempli par l'application)"
```
⚠️ `phys.simuler` apparaît aussi dans la barre d'outils : **même action, même
`bind`** ; si le monteur refuse deux éléments de même identifiant **dans deux
documents**, dis-le — ils ne sont pas dans le même document.
Les lignes de `phys.monde` ne sont pas animables → `cle.visible = false`.

### 5.12 `pile_effets.nkgui`

```
Panel "pile_effets"                           title: "Pile d'effets"
  VBox "colonne"                              gap: 4
    HBox "fx.barre"                           gap: 2
      Button "fx.ajouter"                     label: "Ajouter ▾"   icon: plus   (ouvre Phénomènes ▸ Ajouter)
      Button "fx.declencheur"                 icon: flag   tooltip: "Ajouter un déclencheur"
      Button "fx.cuire"                       label: "Cuire le cache"   icon: package
    EnTetePanneau "fx.entete"
    Scroll "fx.defilement"                    axis: Vertical
      VBox "fx.pile"                          gap: 2       (les ModuleEffet sont ajoutés par l'application)
```

### 5.13 `assistant_ia.nkgui`

```
Panel "assistant_ia"                          title: "Assistant IA"
  VBox "colonne"                              gap: 6
    HBox "ia.cible"                           gap: 4
      Text "ia.cible_titre"                   text: "Travaille sur :"
      Host "ia.cible_pastilles"               hint: "la sélection en pastilles"
    TextField "ia.demande"                    multiline: true   placeholder: "Décris le mouvement… (« il se lève péniblement puis court vers la porte »)"   bind: ia.demande
    HBox "ia.intentions"                      gap: 2
      Button "ia.intervalles"                 label: "Intervalles"
      Button "ia.corriger"                    label: "Corriger"
      Button "ia.style"                       label: "Style ▾"
      Button "ia.voix_visage"                 label: "Depuis la voix"
      Button "squel.proposer_ia"              label: "Squelette"
      Button "poids.automatiques"             label: "Poids"
    HBox "ia.envoyer_ligne"                   justify: End
      Button "ia.proposer"                    label: "Proposer"   icon: sparkles   shortcut✚: "Ctrl+Entrée"
    Separator "ia.sep"
    Text "ia.propositions_titre"              text: "Propositions"
    Scroll "ia.defilement"                    axis: Vertical
      VBox "ia.propositions"                  gap: 6       (les CartePropo sont ajoutées par l'application)
    Expander "ia.historique"                  label: "Historique"   expanded: false
      Host "ia.historique_liste"              hint: "propositions passées, appliquées ou rejetées"
```
**Couleur** : la seule couleur de ce panneau est celle des **cartes** (aperçu,
violet hachuré) et de leur bouton **Appliquer** (orange). Pas d'autre.

### 5.14 `distribution.nkgui`

```
Panel "distribution"                          title: "Distribution"
  VBox "colonne"                              gap: 6
    Table "agent.tableau"                     columns: ["Personnage","Rôle","Humeur","Objectif","État"]   (non monté : attendu)
    Host "agent.tableau_hote"                 hint: "le tableau des agents tant que Table n'est pas monté"
    HBox "agent.boutons"                      gap: 4
      Button "agent.ajouter"                  label: "Ajouter un rôle"   icon: user-plus
      Button "agent.repeter"                  label: "Répéter"   icon: eye   tooltip: "Joue en aperçu, rien n'est écrit"
      BoutonEcrit "agent.jouer_enregistrer"   label: "Jouer et enregistrer"   icon: clapperboard
      Button "agent.rejouer_replique"         label: "Rejouer une réplique"   icon: rotate-ccw
```
📌 `Table` **et** `Host` : le premier voyage pour le jour où il sera monté ;
le second sert aujourd'hui. Quand `Table` est monté, `agent.tableau_hote`
disparaît.
L'**état** d'un agent se peint en `Badge` : au repos (neutre), joue
(`@info.simulation`), joué (`@origine.agent`).

### 5.15 `texte_scene.nkgui`

```
Panel "texte_scene"                           title: "Texte de la scène"
  VBox "colonne"                              gap: 4
    HBox "texte.barre"                        gap: 4
      Button "agent.importer_texte"           label: "Importer…"   icon: file-text
      Dropdown "texte.personnage"             bind: texte.personnage_courant   (items remplis par l'application)
    TextField "texte.contenu"                 multiline: true   wrap: true   bind: agent.texte
                                              placeholder: "KOFI — (méfiant) Qu'est-ce que tu cherches ici ?"
```

### 5.16 `captation.nkgui`

```
Panel "captation"                             title: "Captation"
  VBox "colonne"                              gap: 6
    HBox "capt.source"                        gap: 4
      Button "fichier.importer_video"         label: "Vidéo…"   icon: film
      Button "capt.camera"                    label: "Caméra"   icon: webcam
      Button "fichier.importer_mocap"         label: "Mocap…"   icon: file-input
    Host "capt.lecteur"                       hint: "lecteur vidéo avec le squelette détecté en surimpression"
    HBox "capt.transport"                     gap: 2
      Button "capt.jouer"                     icon: play
      Slider "capt.position"                  bind: capt.position   min: 0   max: 1
    Categorie "capt.correspondance"           label: "Correspondance d'os"
      Button "capt.apparier"                  label: "Apparier automatiquement"   icon: wand-2
      Host "capt.tableau_os"                  hint: "os détecté ↔ os du personnage, corrigeable"
    Categorie "capt.reglages"                 label: "Réglages"
      LigneNombre "capt.lissage"              etiquette: "Lissage"   bind: capt.lissage   min: 0   max: 1
      LigneCase "capt.pieds"                  etiquette: "Verrouiller les pieds"   bind: capt.pieds
      LigneCase "capt.physique"               etiquette: "Correction physique"   bind: capt.physique
    HBox "capt.fin"                           justify: End
      BoutonEcrit "capt.importer"             label: "Importer en clés"   icon: diamond-plus
```
Les lignes de réglage ne sont pas animables → `cle.visible = false`.

### 5.17 `facile.nkgui`

Disposition **propre**, pas de docking visible.

```
Window "facile"                               title: "NkAnima — Facile"
  VBox "facile.racine"                        gap: 0
    HBox "facile.tete"                        gap: 8   align: Center
      Text "facile.titre"                     text: "NkAnima — Facile"
      Spacer "pousse"
      Button "facile.ouvrir_detail"           label: "Ouvrir en détail"   icon: sliders-horizontal
    Splitter "facile.partage"                 orientation: Vertical   ratio: 0.22
      VBox "facile.gauche"                    gap: 8
        Text "facile.qui_titre"               text: "Qui ?"
        Flow "facile.qui"                     gap: 6       (Tile par personnage, ajoutées par l'application)
        Button "facile.ajouter_perso"         label: "+ Ajouter"
        Separator "facile.sep_g"
        Text "facile.reglages_titre"          text: "Réglages"
        Text "t_vitesse"                      text: "Vitesse"
        CurseurParlant "facile.vitesse"       gauche: "lent"         droite: "rapide"        bind: facile.vitesse
        Text "t_energie"                      text: "Énergie"
        CurseurParlant "facile.energie"       gauche: "mou"          droite: "vif"           bind: facile.energie
        Text "t_poids"                        text: "Poids"
        CurseurParlant "facile.poids"         gauche: "léger"        droite: "lourd"         bind: facile.poids
        Text "t_humeur"                       text: "Humeur"
        CurseurParlant "facile.humeur"        gauche: "triste"       droite: "joyeux"        bind: facile.humeur
        Text "t_realisme"                     text: "Réalisme"
        CurseurParlant "facile.realisme"      gauche: "dessin animé" droite: "physique"      bind: facile.realisme
      VBox "facile.droite"                    gap: 0
        Host "facile.vue"                     hint: "vue 3D, caméra automatique qui suit le personnage"
        TabBar "facile.onglets"               bind: facile.mode_saisie   tabs: ["Actions","Écrire","Tracer un chemin"]
        Stack "facile.saisie"                 (non monté : attendu — un seul des trois visible)
          Scroll "facile.actions_defil"       axis: Horizontal   visible: true
            HBox "facile.actions"             gap: 6
              TuileAction "facile.marcher"    image: "actions/marcher"   label: "Marche"
              TuileAction "facile.courir"     image: "actions/courir"    label: "Court"
              TuileAction "facile.sauter"     image: "actions/sauter"    label: "Saute"
              TuileAction "facile.asseoir"    image: "actions/asseoir"   label: "S'assoit"
              TuileAction "facile.saluer"     image: "actions/saluer"    label: "Salue"
              TuileAction "facile.tomber"     image: "actions/tomber"    label: "Tombe"
              TuileAction "facile.grimper"    image: "actions/grimper"   label: "Grimpe"
              TuileAction "facile.nager"      image: "actions/nager"     label: "Nage"
              TuileAction "facile.danser"     image: "actions/danser"    label: "Danse"
              TuileAction "facile.parler"     image: "actions/parler"    label: "Parle"
          HBox "facile.ecrire"                gap: 4   visible: false
            TextField "facile.phrase"         placeholder: "Il se lève, regarde à gauche, puis court vers la porte."   bind: facile.phrase
            Button "facile.proposer"          label: "Proposer"   icon: sparkles
          HBox "facile.tracer"                gap: 4   visible: false
            Text "facile.tracer_aide"         text: "Dessine le chemin au sol dans la vue."
            Dropdown "facile.allure"          bind: facile.allure   items: ["en marchant","en courant","en rampant"]
        HBox "facile.sequence_tete"           gap: 4   align: Center
          Text "facile.sequence_titre"        text: "Séquence"
          Button "anim.jouer"                 icon: play   tooltip: "Jouer (Espace)"
        Host "facile.sequence"                hint: "blocs d'actions du personnage choisi (glisser, allonger, clic droit)"
```
**Comportements** (idempotents ✅, et c'est ce qui remplace `Stack` aujourd'hui) :
```
facile.actions_defil.visible = (facile.mode_saisie == "Actions")
facile.ecrire.visible        = (facile.mode_saisie == "Écrire")
facile.tracer.visible        = (facile.mode_saisie == "Tracer un chemin")
```
**Tile** d'action : **Événement (lot b)** `on Click` → ajouter le bloc à la fin
de la séquence. ⚠️ **Non idempotent** : ne l'émule pas dans un `behavior`
rejoué à chaque image — c'est l'action du bouton qui le fait.
**Couleur** : aucune dans ce document. Le mode Facile est calme exprès.

### 5.18 `accueil.nkgui`

```
Window "accueil"                              title: "NkAnima"
  Splitter "accueil.partage"                  orientation: Vertical   ratio: 0.2
    VBox "accueil.nav"                        gap: 2
      Image "accueil.logo"                    source: "logo_rihen"
      ListBox "accueil.section"               bind: accueil.section   items: ["Récents","Nouveau","Ouvrir","Apprendre"]
    VBox "accueil.contenu"                    gap: 8
      Text "accueil.titre"                    text: "Projets récents"
      Scroll "accueil.defil"
        Flow "accueil.recents"                gap: 8     (Tile par projet : vignette animée, nom, date, Film/Jeu)
      HBox "accueil.nouveau"                  gap: 12    visible: false
        Tile✚ "accueil.nouveau_film"          image: "accueil/film"     label: "Film"     caption: "Un plan, toutes les clés, physique cuite"
        Tile✚ "accueil.nouveau_jeu"           image: "accueil/jeu"      label: "Jeu"      caption: "Cycles, déplacement de racine, temps réel"
        Tile✚ "accueil.nouveau_facile"        image: "accueil/facile"   label: "Facile"   caption: "Animer sans être animateur"
```
`accueil.recents.visible = (accueil.section == "Récents")`,
`accueil.nouveau.visible = (accueil.section == "Nouveau")` — idempotents ✅.

### 5.19 `dispositions.nkgui` — les espaces de travail (P10)

```
layout "squelette"
  dock "vue" center · dock "arbre_os" left 0.18 · dock "details" right 0.22 · dock "poids" right-bottom 0.35
layout "animation"
  dock "vue" center · dock "scene" left 0.16 · dock "details" right 0.22 · dock "sequenceur" bottom 0.30
layout "physique"
  dock "vue" center · dock "corps_physiques" left 0.18 · dock "details" right 0.22 · dock "sequenceur" bottom 0.25
layout "phenomenes"
  dock "vue" center · dock "pile_effets" left 0.20 · dock "details" right 0.22 · dock "sequenceur" bottom 0.25
layout "ia"
  dock "vue" center · dock "assistant_ia" right 0.26 · dock "distribution" left 0.22 · dock "texte_scene" left-bottom 0.45 · dock "sequenceur" bottom 0.25
layout "captation"
  dock "captation" left 0.40 · dock "vue" center · dock "sequenceur" bottom 0.25
layout "facile"
  document "facile"          (pas d'amarrage : la fenêtre Facile entière)
```
Le **tiroir** Bibliothèque n'est dans aucune disposition : il monte du bas par
`biblio.ouvrir` (Ctrl+Espace) et redescend, comme UE5.

---

## 6. Les actions

Légende : ✅ servie aujourd'hui · ✚ à créer (grisée avec sa raison) ·
🟠 **écrit une clé** (couleur `@info.ecrit`).

### 6.1 Existantes — gardées telles quelles

`anim.debut` ✅ · `anim.fin` ✅ · `anim.image_prec` ✅ · `anim.image_suiv` ✅ ·
`anim.jouer` ✅ · `anim.boucle` ✅ · `anim.plage` ✅ · `anim.inserer` ✅ 🟠 ·
`anim.supprimer` ✅ · `anim.copier` ✅ · `anim.coller` ✅ 🟠 · `anim.annuler` ✅ ·
`anim.refaire` ✅ · `anim.selection_tout` ✅ · `anim.selection_rien` ✅ ·
`anim.selection_inverser` ✅ · `anim.pose_entrer` ✅ · `anim.pose_enregistrer` ✅ 🟠 ·
`anim.pose_quitter` ✅ · `anim.pose_ik` ✅ · `anim.pose_miroir` ✅ ·
`anim.vue_solide` ✅ · `anim.vue_rendu` ✅ · `anim.vue_filaire` ✅ ·
`anim.compteurs` ✅ · `anim.cuire` ✅ 🟠 · `anim.retarget` ✅ ·
`anim.aide_raccourcis` ✅ · `anim.apropos` ✅.

Servies aujourd'hui mais **non reprises** dans ce document — à garder tant
qu'elles sont appelées, à retirer sinon (signale lesquelles sont mortes) :
`anim.boucle_activee`, `anim.boucle_coupee`, `anim.calques`, `anim.com`,
`anim.com_on`, `anim.com_off`, `anim.editeur_courbes`, `anim.feuille_expo`,
`anim.physique`, `anim.physique_on`, `anim.physique_off`, `anim.vfx`.

### 6.2 À créer

| domaine | actions (toutes ✚) |
|---|---|
| **anim** | `anim.cle_prec` · `anim.cle_suiv` · `anim.cle_auto` · `anim.cle_propriete` 🟠 · `anim.reinitialiser_propriete` · `anim.pose_assistee` · `anim.loco_marcher` · `anim.loco_courir` · `anim.loco_sauter` · `anim.loco_grimper` · `anim.loco_nager` · `anim.loco_ramper` · `anim.loco_custom` |
| **fichier** | `nouveau` · `ouvrir` · `enregistrer` · `enregistrer_sous` · `importer_maillage` · `importer_mocap` · `importer_video` · `importer_audio` · `exporter_clip` · `exporter_squelette` · `exporter_effet` · `exporter_fbx` · `exporter_gltf` · `quitter` |
| **edition** | `historique` · `dupliquer` · `supprimer` · `renommer` · `ajuster_derniere` · `preferences` |
| **selection** | `enfants` · `symetrique` · `meme_origine` |
| **vue** | `os` · `noms_os` · `controleurs` · `trajectoires` · `pelures` · `centre_masse` · `contacts` · `forces` · `collisions` · `carte_poids` · `camera` · `mode` · `afficher` · `aimant` · `vitesse_camera` |
| **espace** | `choix` · `niveau` · `facile` · `squelette` · `animation` · `physique` · `phenomenes` · `ia` · `captation` |
| **outil** | `selection` · `deplacer` · `tourner` · `echelle` · `trajectoire` · `pinceau_poids` |
| **seq** | `cadrer` |
| **visage** | `joie` · `colere` · `peur` · `surprise` · `degout` · `tristesse` |
| **scene** | `filtre_perso` · `filtre_objet` · `filtre_fx` · `filtre_agent` · `filtre_traj` · `isoler` |
| **squel** | `gabarit` · `gabarit_bipede` · `gabarit_quadrupede` · `gabarit_oiseau` · `gabarit_poisson` · `gabarit_serpent` · `gabarit_insecte` · `gabarit_vide` · `proposer_ia` · `ajouter_os` · `contrainte_ik2` · `contrainte_chaine` · `contrainte_colonne` · `contrainte_visee` · `contrainte_copie` · `symetriser` · `pose_reference` · `lier` · `tester_deformation` |
| **poids** | `automatiques` · `normaliser` |
| **phys** | `part` · `simuler` · `generer_corps` · `corriger` · `cuire` 🟠 · `reglages_monde` |
| **traj** | `attacher` · `detacher` · `cuire` 🟠 |
| **fx** | `ajouter` · `ajouter_vent` · `ajouter_particules` · `ajouter_granulaire` · `ajouter_destruction` · `ajouter_fumee` · `ajouter_eau` · `ajouter_ocean` · `ajouter_tissu` · `declencheur` · `cuire` · `preregler` · `supprimer_module` |
| **ia** | `assistant` · `proposer` · `apercu` · `appliquer` 🟠 · `rejeter` · `intervalles` · `corriger` · `style` · `texte_mouvement` · `voix_visage` |
| **agent** | `distribution` · `ajouter` · `repeter` · `jouer_enregistrer` 🟠 · `rejouer_replique` · `jouer_scene` · `importer_texte` · `exporter_comportement` |
| **capt** | `camera` · `jouer` · `apparier` · `importer` 🟠 |
| **biblio** | `ouvrir` · `importer` |
| **facile** | `ouvrir_detail` · `ajouter_perso` · `proposer` · `marcher` · `courir` · `sauter` · `asseoir` · `saluer` · `tomber` · `grimper` · `nager` · `danser` · `parler` |
| **fenetre** | `scene` · `details` · `sequenceur` · `arbre_os` · `poids` · `corps` · `pile_effets` · `distribution` · `captation` · `journal` · `reinitialiser` |
| **aide** | `documentation` |
| **accueil** | `nouveau_film` · `nouveau_jeu` · `nouveau_facile` |

**Raisons de grisé** à afficher (infobulle), par défaut :

| cas | raison |
|---|---|
| action ✚ non servie | « Pas encore disponible dans NkAnima » |
| `squel.*`, `poids.*` sans maillage | « Importe d'abord un maillage » |
| `anim.pose_*`, `anim.loco_*` sans squelette lié | « Il faut un squelette lié au maillage » |
| `traj.attacher` sans sélection | « Sélectionne au moins un élément » |
| `ia.appliquer` sans proposition | « Aucune proposition à appliquer » |
| `phys.cuire` sans simulation | « Lance d'abord la simulation » |

---

## 7. Ce que tu ne sais pas encore reproduire — écrit quand même

### 7.1 États (`appearance(État)`) — comptés, non peints

| élément | état | rendu attendu |
|---|---|---|
| tout `Button` | `Hover`, `Pressed` | `@fond.survol` / un cran plus sombre |
| `BoutonEcrit` | `Hover`, `Pressed` | orange plus clair / plus sombre (§3.3) |
| `ToggleButton` | `Active` | fond `@accent` ; exceptions : `anim.cle_auto` → `@info.enregistrement`, `phys.simuler` → `@info.simulation` |
| `KeyDiamond` | `Hover` | `@info.ecrit` |
| `KeyDiamond` | `Empty` / `Full` / `Half` | losange vide / plein / demi |
| `CartePropo` | `Applied` | fond et hachures retirés, `Badge` passe à `@origine.ia` |
| `Tile` | `Hover` | liseré `@accent` 1 px, vignette animée |
| onglet de panneau | `Focused` | liseré haut 2 px `@accent` |
| sélecteur d'espace | `Active` | liseré bas 2 px `@espace.*` |
| tout élément | `Disabled` | `@texte.faible`, infobulle = raison |

### 7.2 Animations (section `animation`) — voyagent, non jouées

| élément | animation |
|---|---|
| tiroir `bibliotheque` | monte du bas en 150 ms (ease-out), redescend en 120 ms |
| `CartePropo` | apparaît en fondu + glissement de 8 px, 120 ms |
| `Badge "ia.en_attente"` | pulsation d'échelle 1 → 1,15 → 1 à chaque nouvelle proposition |
| `ToggleButton "phys.simuler"` actif | pulsation lente d'opacité du fond (1,2 s) tant que la simulation tourne |
| `Tile` | vignette : lecture en boucle au survol |

### 7.3 Événements — à écrire pour le lot (b)

| élément | événement | effet | idempotent ? |
|---|---|---|---|
| `commande` | `on Submit` | exécuter la commande | ❌ — **ne pas** émuler |
| `Tile` d'action (Facile) | `on Click` | ajouter le bloc | ❌ — porté par l'action du bouton |
| `Tile` de bibliothèque | `on Hover` / `on DragStart` / `on Drop` | jouer / glisser / appliquer | ❌ |
| `ia.demande` | `on Submit` (Ctrl+Entrée) | `ia.proposer` | ❌ |
| `espace.choix` | `on Changed` | appliquer la disposition | ❌ — aujourd'hui l'application compare la valeur à la précédente |
| `seq.onglets`, `facile.onglets` | `on Changed` | visibilités | ✅ — déjà couvert par les comportements idempotents du §5 |

### 7.4 Ombres, contours, polices

- **Polices** : une seule famille sans empattement ; `font = "UI-12"` (texte),
  `"UI-12-Gras"` (titres de catégories, onglets), `"UI-11"` (étiquettes
  secondaires), `"Mono-11"` (champ de commande, valeurs numériques du
  Séquenceur). Comptées non peintes.
- **Contours** : `stroke` 1 px `@bord` sur les champs de saisie et les cartes ;
  liserés d'axe sur `ChampVec3` ; liserés d'onglet et d'espace.
- **Ombres** : **aucune**, sauf sous les menus déroulants et le tiroir
  Bibliothèque (ombre douce 8 px, 40 %). UE5 est plat : ne pas en ajouter.

---

## 8. L'ordre de livraison proposé

Chaque étape est **montrable** et **mesurable** avec les compteurs existants.

| # | livrable | pourquoi dans cet ordre | témoin |
|---|---|---|---|
| 1 | ~~`include` traité~~ ✅ **fait le 26/09** | — | — |
| 2 | `composants.nkgui` + `menus` + `barre_outils` + `barre_etat` | la coquille, avec les actions existantes | `actionsInconnues` = nombre d'actions ✚ attendues, pas plus |
| 3 | `vue_3d` + `scene` + `details` + `details_os` + `sequenceur` | l'espace **Animation**, le plus employé | `hotes` : toutes les zones servies ; le bouton `anim.inserer` compté en `#F79A28` |
| 4 | `dispositions` (P10) et le sélecteur d'espace | changer d'espace change la disposition | 7 dispositions lues, 0 ignorée |
| 5 | `arbre_os` + `poids` + `corps_physiques` + `details_corps` + `details_trajectoire` | paliers P1–P2 du doc 07 | idem |
| 6 | `facile` + `accueil` | palier P3 | les trois visibilités de `facile.saisie` exclusives à chaque image |
| 7 | `assistant_ia` + `distribution` + `texte_scene` + `captation` + `pile_effets` + effets | paliers P3–P6 | idem |

📌 Après l'étape 3, **mesure `apparencesNonPeintes` et `etatsNonAppliques`** et
publie les nombres : ils disent exactement ce que coûtera le lot (b) pour cette
application.
