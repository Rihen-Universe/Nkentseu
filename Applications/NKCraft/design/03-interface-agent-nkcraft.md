# NKCraft — l'interface, pour l'agent qui écrit les `.nkgui`

### Document 3 — Spécification d'interface côté agent

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> Traduction du **02** en tes quatre points : **arbre nommé**, **composants**,
> **actions**, **couleurs porteuses** — plus ce que tu ne sais pas encore
> reproduire, écrit quand même.
>
> **Prérequis** : NkAnima **09** (notation, P1–P12, composants), NKScena **03**
> (`Commun/`, P13–P15), Nogee **06** (lignes engendrées par la réflexion,
> `Host` d'extension, P16–P18). **Ce document ne les répète pas.**
>
> **Particularité de NKCraft** : c'est la seule application de la famille dont
> l'interface **existe déjà**, codée à la main (~34 000 lignes, `Shell/`). Tu
> n'écris pas une interface neuve : tu **décris celle qui existe** en documents, et
> tu **gardes ses identifiants d'actions** (§0.2). Le passage se fait panneau par
> panneau (doc 01 §6).

---

## 0. Ce qui est propre à NKCraft

### 0.1 La référence visuelle

`docs/UI_SPEC.md` (disposition §2.2, palette §10bis, modificateurs §7, chemins
§9bis, nœuds §10ter) **et** la maquette Banani (flow « NK3D Modeler », écran
« Mode Objet (v2, ref complète) », composant `NK3DPropertiesTabs`). Icônes
**Lucide** (les 7 manquantes ont été dessinées ; attribution CC BY 4.0 livrée le
26/09).

### 0.2 Les identifiants d'actions existants — **gardés tels quels**

L'application sert déjà ces actions (relevé du code, 26/09). **Ne les renomme
pas** : le préfixe `app.` / `objet.` / `edit.` / `sel.` / `mod.` est **celui de
NKCraft**, comme `anim.` est celui de NkAnima.

| préfixe | actions servies aujourd'hui |
|---|---|
| `app.` | `annuler` · `refaire` · `enregistrer` · `enregistrer_tout` · `palette` · `panneau_outils` · `compteurs` · `aide` · `apropos` · `selecteur_outil` · `vue.hierarchie` · `vue.proprietes` · `vue.navigateur` · `vue.plein_ecran` |
| `sel.` | `tout` · `rien` |
| `objet.` | `deplacer` · `tourner` · `echelle` · `supprimer` · `dupliquer` · `dupliquer_independant` · `mode_edition` |
| `edit.` | `mode_objet` · `sous_mode_sommet` · `sous_mode_arete` · `sous_mode_face` · `deplacer` · `tourner` · `echelle` · `extruder` · `inserer` · `biseauter` · `loop_cut` · `subdiviser` · `fusionner` · `dissoudre` · `creer_face` · `bisect` · `spheriser` · `gonfler` · `supprimer` |
| `mod.` | `panel` · `sub` (et les autres types de modificateurs par leur clé) |

⚠️ `menu.0` … `menu.6` et `menu.panel` sont des **identifiants internes de
répartiteur**, pas des actions : ne les écris pas dans un document. Les menus
d'un document portent un identifiant de **menu** (`menu_fichier`…), et leurs
entrées l'identifiant de **l'action**.

⚠️ **Le relevé du 26/09 sur les menus** (19 entrées : 7 câblées, 11 grisées, trois
motifs) **fait foi** pour l'état de chaque entrée. En particulier, `objet.dupliquer`
et `objet.supprimer` **existent** mais leur chemin produit vit **en ligne** dans le
menu contextuel de la hiérarchie : dans le document, écris-les ; l'application
les grise tant qu'ils ne sont pas extraits (motif « c »).

### 0.3 Les actions **communes** — un préfixe à fixer maintenant

Les composants communs (`LigneNombre`, `LigneVec3`…) portent aujourd'hui
`anim.reinitialiser_propriete` et `anim.cle_propriete`. **Ces deux actions ne
sont servies nulle part** (✚ dans les quatre documents) : les renommer **ne coûte
rien aujourd'hui** et évite que NKCraft, Nogee et PV3DE servent un préfixe
`anim.` qui n'est pas le leur.

> **Proposition** : les actions émises par les **composants communs** prennent le
> préfixe **`commun.`** : `commun.reinitialiser_propriete`,
> `commun.cle_propriete`, `commun.parcourir`, `commun.utiliser_selection`,
> `commun.effacer_reference`, `commun.retirer_touche`. Les actions déjà
> **servies** (`anim.annuler`, `anim.inserer`…) ne bougent pas.
> Signale-le à Rodolf ; les documents 09, 03 et 06 sont mis à jour en ce sens.

### 0.4 Les couleurs

La **palette imposée** (doc 02 §1.1) devient la référence du thème de la famille
(valeurs de `@fond.*`, `@accent`, `@selection.*`). Rappel de la règle : **bleu =
état de l'interface, ambre = sélection 3D.**

---

## 1. Le vocabulaire proposé en plus

P1–P18 : voir les trois documents précédents. NKCraft en ajoute **deux**.

### P19 — `PieMenu`

- **Quoi** : un menu **circulaire** qui s'ouvre sous le curseur ; 2 à 8
  entrées disposées en cercle ; on choisit en **glissant** vers une direction et
  en relâchant, ou en cliquant.
- **Propriétés** : `label` ; enfants `MenuItem` (dans l'ordre : droite, gauche,
  bas, haut, puis les diagonales).
- **Pourquoi** : Blender ouvre les modes (Ctrl+Tab), le pivot (`.`) et
  l'orientation (`,`) en menus circulaires, et c'est un **geste** : la direction
  se mémorise, le texte non. Un `ContextMenu` vertical perdrait le geste.
- **En attendant** : `ContextMenu` (non monté) ; l'application ouvre une liste.

### P20 — `ToggleButton` à groupe **combinable**

- **Quoi** : `group` + `combine: Shift` — dans le groupe, un clic **remplace**,
  Maj+clic **ajoute**.
- **Pourquoi** : sommet / arête / face (1 / 2 / 3) sont **combinables** dans
  NKCraft (SPECIFICATION §4.1 : « sommet / arête / face, combinables »). Un groupe
  exclusif l'interdirait ; trois cases indépendantes perdraient le clic qui
  remplace.
- **En attendant** : trois `ToggleButton` sans `group` ; l'application tient la
  règle.

---

## 2. Les jetons propres à NKCraft

À ajouter au thème, sans rien retirer.

| jeton | Sombre | où | pourquoi |
|---|---|---|---|
| `@selection.3d` | `#F2980E` | contour de sélection dans la vue ; **ligne sélectionnée** de la hiérarchie (liseré) | la sélection |
| `@selection.actif` | `#FFFFFF` | élément actif | distinct de la sélection |
| `@lumiere` / `@lumiere.active` | `#E8C33A` / `#FFF1A8` | icônes de lumières (hiérarchie, vue) | UI_SPEC §3.4 |
| `@sculpt.masque` | `#2A2A2A` à 70 % | surface masquée | on ne sculpte pas le masqué |
| `@uv.couture` | `#E0303A` | arêtes de couture | convention |
| `@uv.etirement.bas` / `.juste` / `.haut` | `#2F7FE0` / `#3FB950` / `#E0303A` | carte d'étirement des UV | un dépliage se juge à l'œil |
| `@type.maillage` `@type.materiau` `@type.texture` `@type.animation` `@type.niveau` `@type.generateur` | (valeurs déjà livrées dans le navigateur) | bande de type des cartes | UI_SPEC §2.2 |
| `@noeud.donnees` / `@noeud.donnees_survol` | `#0A545E` / `#095461` | en-tête des nœuds de données | UI_SPEC §10bis.3 |
| `@noeud.action` | `#F2980E` | en-tête des nœuds qui **émettent une commande** | UI_SPEC §10bis.2 : l'ambre pour les nœuds d'action |

---

## 3. Les composants

### 3.1 Communs

Tous ceux de `Commun/composants.nkgui`, **avec les actions renommées en
`commun.`** (§0.3).

### 3.2 Propres — `NKCraft/composants_nkcraft.nkgui`

#### `LigneTransformation`

La ligne de la maquette (ROADMAP §1) : vecteur + trois boutons.

```
component "LigneTransformation"
  Grid "ligne"                     columns: 5   sizes: [0.24, 0.58, 0.06, 0.06, 0.06]   gap: 4
    Text "etiquette"               text: "?"
    VectorField✚ "valeur"          bind: ?   components: 3
    ToggleButton✚ "verrou"         icon: lock        bind: ?.verrou   tooltip: "Verrouiller"
    Button "objet.reinitialiser"   icon: refresh-cw  tooltip: "Réinitialiser"
    Button "objet.copier_valeur"   icon: copy        tooltip: "Copier la valeur"
```
**Aujourd'hui** : `valeur` = `ChampVec3` (NkAnima 09 §3.7).
⚠️ `objet.reinitialiser` et `objet.copier_valeur` sont **les mêmes** pour Position,
Rotation, Échelle : l'application distingue par l'**instance** (`<instance>.objet.reinitialiser`).
Si le préfixage des identifiants d'enfants (règle des composants) casse la
correspondance avec l'action, **dis-le** : c'est une vraie question de format
(une action portée par un enfant de composant a-t-elle l'identifiant préfixé ou
non ?), et elle vaut pour toute la famille.

#### `Pastille` — les quatre pastilles du panneau droit

```
component "Pastille"
  ToggleButton✚ "bouton"           icon: "?"   label: "?"   group: pastilles_droite
```
Instances : `app.pastille_modele` (box, « Modèle ») · `app.pastille_rendu` (sun,
« Rendu ») · `app.pastille_scene` (layers, « Scène ») · `app.pastille_modificateur`
(sliders-horizontal, « Modificateur »).
📌 **Tout icône a un mot** (UI_SPEC §0, règle 1) : le libellé est **affiché**, pas
seulement en infobulle.

#### `CarteModificateur`

La carte d'UI_SPEC §7 ; **son corps est engendré** depuis `NkModParam`.

```
component "CarteModificateur"
  Expander "carte"                             label: "?"   expanded: true
    VBox "corps"                               gap: 2
      HBox "tete"                              gap: 4   align: Center
        ToggleButton✚ "actif"                  icon: circle-dot   bind: ?.actif   tooltip: "Activer / désactiver"
        Text "id"                              text: "id ?"
        Spacer "pousse"
        Button "mod.monter"                    icon: chevron-up     tooltip: "Monter dans la pile"
        Button "mod.descendre"                 icon: chevron-down   tooltip: "Descendre dans la pile"
        Button "mod.menu"                      icon: more-vertical  tooltip: "Plus"
        Button "mod.retirer"                   icon: x              tooltip: "Retirer"
      Host "parametres"                        hint: "paramètres engendrés depuis NkModParam : Bool → case, Int → champ, Float → curseur borné, Vec3 → trois champs"
      HBox "pied"                              gap: 4   justify: End
        Button "mod.appliquer"                 label: "Appliquer"   icon: check   tooltip: "Destructif : le modificateur est fondu dans le maillage"
        Text "avertissement"                   text: "Ce modificateur n'est pas le premier : le résultat différera de l'affichage."   visible: false
```
- **Critère d'UI_SPEC §7** : ajouter un 18ᵉ modificateur ne demande **aucune ligne
  de document**.
- `carte.label` = `NkModParam` **label**, jamais `name`.
- Un modificateur **éteint** reste visible et grisé (état `Disabled` de la carte,
  pas `visible: false`).
- `avertissement.visible = (?.index > 0)` et couleur `@info.alerte` — idempotent ✅.

#### `VignetteBrosse`

```
component "VignetteBrosse"
  Tile✚ "tuile"                    image: "?"   label: "?"   size: (56, 64)
```
Instances : `sculpt.brosse_elever` · `_creuser` · `_lisser` · `_pincer` ·
`_gonfler` · `_aplanir` · `_masque` · `_peindre` (les huit modes de
`NkSculptTypes.h`).

#### `VignetteCourbe`

```
component "VignetteCourbe"
  Tile✚ "tuile"                    image: "?"   label: "?"   size: (44, 44)
```
Instances : les **cinq** profils d'atténuation déclarés (`sculpt.courbe_*` ;
noms à reprendre de `NkSculptTypes.h`, **ne les invente pas**).

#### `CarteGeneration`

Une génération de l'IA génératrice (onglet Créer de l'Assistant).

```
component "CarteGeneration"
  Panel "carte"                               appearance: fill #9B6BF2@35% // @info.apercu   pattern✚: Hatch   radius: 2
    VBox "corps"                              gap: 4
      Host "apercu"                           hint: "aperçu tournant de la forme générée"
      Text "source"                           text: "?"   (« depuis : masque.png » ou la phrase)
      HBox "actions"                          gap: 4   justify: End
        BoutonEcrit "ia.importer_generation"  label: "Importer"   icon: download
        Button "ia.regenerer"                 label: "Autre essai"   icon: refresh-cw
        Button "ia.rejeter"                   label: "Rejeter"   icon: x
```
📌 `BoutonEcrit` ici : **importer écrit au journal** (une commande). C'est le même
sens que « écrit une clé » dans NkAnima : *ce bouton modifie votre travail*.

---

## 4. Les fichiers — `Resources/Interface/NKCraft/`

| fichier | § | remplace (code de `Shell/`) |
|---|---|---|
| `composants_nkcraft.nkgui` | 3.2 | — |
| `menus.nkgui` | 5.1 | le répartiteur des menus (`menu.0`–`menu.6`) |
| `barre_outils.nkgui` | 5.2 | barre d'outils |
| `barre_etat.nkgui` | 5.3 | barre d'état |
| `vue_extra.nkgui` | 5.4 | barre de vue (`NkModelerViewport.h`) |
| `scene_filtres.nkgui` | 5.5 | tête de la hiérarchie (`NkModelerHierarchy.h`) |
| `bibliotheque_filtres.nkgui` | 5.5 | tête du navigateur (`NkModelerBrowser.h`) |
| `panneau_droit.nkgui` | 5.6 | pastilles (`NkModelerProperties.h`) |
| `pastille_modele.nkgui`, `pastille_modificateur.nkgui`, `pastille_rendu.nkgui`, `pastille_scene.nkgui` | 5.6 | contenu des pastilles |
| `panneau_t.nkgui` + `panneau_t_objet`, `_edition`, `_sculpture`, `_peinture`, `_retopo` | 5.7 | — (à créer ; le panneau est grisé aujourd'hui : « prévu, pas disponible ») |
| `derniere_operation.nkgui` | 5.8 | — |
| `menus_circulaires.nkgui` | 5.9 | — |
| `uv_editeur.nkgui` | 5.10 | — |
| `matiere.nkgui`, `calques_texture.nkgui`, `cuisson.nkgui` | 5.11 | — |
| `generateur.nkgui`, `generateur_personnage.nkgui` | 5.12 | — |
| `assistant_extra.nkgui` | 5.13 | — (monté dans l'Assistant commun) |
| `dispositions.nkgui` | 5.14 | — |

📌 La **hiérarchie** et le **navigateur** sont les panneaux **communs** (`scene`,
`bibliotheque`) avec les `Host` d'extension de Nogee 06 §0.3. Les cartes d'assets
du navigateur de NKCraft (bande de type, deux états de sélection — ROADMAP
« Cartes d'assets », 17/08) deviennent **la** carte commune : leurs règles
valent pour les cinq applications.

---

## 5. Les arbres

### 5.1 `menus.nkgui`

Les 19 entrées du relevé du 26/09, **plus** ce que le doc 02 §8 ajoute. L'état
(câblée / grisée et son motif) est tenu par l'**application** : le document ne
porte pas `enabled`.

```
MenuBar "menus"
  Menu "menu_fichier"                         label: "Fichier"
    MenuItem "app.nouveau_projet"             label: "Nouveau projet…"          shortcut: "Ctrl+N"
    MenuItem "app.ouvrir"                     label: "Ouvrir…"                  shortcut: "Ctrl+O"
    Menu "menu_recents"                       label: "Récents"                  (rempli par l'application)
    Separator "sep_f1"
    MenuItem "app.enregistrer"                label: "Enregistrer"              shortcut: "Ctrl+S"
    MenuItem "app.enregistrer_sous"           label: "Enregistrer sous…"        shortcut: "Ctrl+Maj+S"
    MenuItem "app.enregistrer_tout"           label: "Tout enregistrer"
    Separator "sep_f2"
    Menu "menu_importer"                      label: "Importer"
      MenuItem "app.importer_obj"             label: "OBJ…"
      MenuItem "app.importer_gltf"            label: "glTF…"
      MenuItem "app.importer_fbx"             label: "FBX…"
      MenuItem "app.importer_dae"             label: "DAE…"
      MenuItem "app.importer_ply"             label: "PLY…"
      MenuItem "app.importer_stl"             label: "STL…"
      MenuItem "app.importer_usda"            label: "USDA…"
    Menu "menu_exporter"                      label: "Exporter"
      MenuItem "app.exporter_gltf"            label: "glTF…"
      MenuItem "app.exporter_fbx"             label: "FBX…"
      MenuItem "app.exporter_obj"             label: "OBJ…"
    Separator "sep_f3"
    MenuItem "app.quitter"                    label: "Quitter"                  shortcut: "Ctrl+Q"

  Menu "menu_edition"                         label: "Édition"
    MenuItem "app.annuler"                    label: "Annuler"                  shortcut: "Ctrl+Z"
    MenuItem "app.refaire"                    label: "Refaire"                  shortcut: "Ctrl+Maj+Z"
    MenuItem "app.historique"                 label: "Historique"
    Separator "sep_e1"
    MenuItem "app.repeter"                    label: "Répéter la dernière"      shortcut: "Maj+R"
    MenuItem "app.ajuster_derniere"           label: "Ajuster la dernière opération"   shortcut: "F9"
    MenuItem "app.palette"                    label: "Rechercher une commande…" shortcut: "F3"
    Separator "sep_e2"
    MenuItem "app.preferences"                label: "Préférences…"

  Menu "menu_fenetre"                         label: "Fenêtre"
    Menu "menu_espaces"                       label: "Espaces de travail"
      MenuItem "espace.modelisation"          label: "Modélisation"
      MenuItem "espace.sculpture"             label: "Sculpture"
      MenuItem "espace.uv"                    label: "UV"
      MenuItem "espace.matiere"               label: "Matière"
      MenuItem "espace.peinture"              label: "Peinture"
      MenuItem "espace.noeuds"                label: "Nœuds"
      MenuItem "espace.presentation"          label: "Présentation"
    Separator "sep_w1"
    MenuItem "app.vue.hierarchie"             label: "Hiérarchie"               checked: true
    MenuItem "app.vue.proprietes"             label: "Propriétés"               checked: true
    MenuItem "app.vue.navigateur"             label: "Navigateur de projet"     shortcut: "Ctrl+Espace"   checked: true
    MenuItem "app.panneau_outils"             label: "Panneau d'outils"         shortcut: "T"
    MenuItem "ia.assistant"                   label: "Assistant"                shortcut: "Ctrl+K"
    MenuItem "fenetre.journal"                label: "Journal"
    MenuItem "app.compteurs"                  label: "Compteurs de rendu"
    MenuItem "app.vue.plein_ecran"            label: "Plein écran"
    Separator "sep_w2"
    MenuItem "fenetre.reinitialiser"          label: "Réinitialiser la disposition"

  Menu "menu_outils"                          label: "Outils"
    Menu "menu_retopo"                        label: "Retopologie"
      MenuItem "outils.retopo_auto"           label: "Automatique…"
      MenuItem "outils.retopo_manuelle"       label: "Manuelle (mode Retopologie)"
    MenuItem "outils.decimer"                 label: "Décimer…"
    MenuItem "outils.lod"                     label: "Niveaux de détail…"
    MenuItem "outils.cuire_textures"          label: "Cuire des textures…"
    MenuItem "outils.recuire_textures"        label: "Recuire les textures"      // déjà câblée
    Menu "menu_generateurs"                   label: "Générateurs"
      MenuItem "gen.personnage"               label: "Personnage"
      MenuItem "gen.vegetation"               label: "Végétation"
      MenuItem "gen.rocher"                   label: "Rochers et falaises"
      MenuItem "gen.batiment"                 label: "Bâtiments modulaires"
    Menu "menu_extensions"                    label: "Extensions"               (rempli par l'application : add-ons)
    MenuItem "outils.recharger_extensions"    label: "Recharger les extensions"

  Menu "menu_selection"                       label: "Sélection"
    MenuItem "sel.tout"                       label: "Tout"                     shortcut: "A"
    MenuItem "sel.rien"                       label: "Rien"                     shortcut: "Alt+A"
    MenuItem "sel.inverser"                   label: "Inverser"                 shortcut: "Ctrl+I"
    Separator "sep_s1"
    MenuItem "sel.boucle"                     label: "Boucle"                   shortcut: "Alt+clic"
    MenuItem "sel.anneau"                     label: "Anneau"                   shortcut: "Ctrl+Alt+clic"
    MenuItem "sel.lies"                       label: "Liés"                     shortcut: "L"
    MenuItem "sel.meme_materiau"              label: "Même matériau"
    MenuItem "sel.agrandir"                   label: "Agrandir"                 shortcut: "Ctrl+Pavé+"
    MenuItem "sel.reduire"                    label: "Réduire"                  shortcut: "Ctrl+Pavé-"
    Menu "menu_par_type"                      label: "Par type"                 (sous-menu vide aujourd'hui : motif « b »)

  Menu "menu_objet"                           label: "Objet"          // visible en mode Objet
    MenuItem "objet.deplacer"                 label: "Déplacer"                 shortcut: "G"
    MenuItem "objet.tourner"                  label: "Tourner"                  shortcut: "R"
    MenuItem "objet.echelle"                  label: "Redimensionner"           shortcut: "S"
    Separator "sep_o1"
    MenuItem "objet.dupliquer"                label: "Dupliquer"                shortcut: "Maj+D"
    MenuItem "objet.dupliquer_independant"    label: "Dupliquer (lié)"          shortcut: "Alt+D"
    MenuItem "objet.supprimer"                label: "Supprimer"                shortcut: "X"
    Separator "sep_o2"
    Menu "menu_appliquer"                     label: "Appliquer"
      MenuItem "objet.appliquer_tout"         label: "Toutes les transformations"
      MenuItem "mod.appliquer_tout"           label: "Tous les modificateurs"
    Menu "menu_origine"                       label: "Origine"
      MenuItem "objet.origine_geometrie"      label: "Au centre de la géométrie"
      MenuItem "objet.origine_curseur"        label: "Au curseur"
    MenuItem "objet.joindre"                  label: "Joindre"                  shortcut: "Ctrl+J"
    MenuItem "objet.separer"                  label: "Séparer"                  shortcut: "P"
    Menu "menu_ombrage"                       label: "Ombrage"
      MenuItem "objet.ombrage_plat"           label: "Plat"
      MenuItem "objet.ombrage_lisse"          label: "Lisse"
      MenuItem "objet.ombrage_angle"          label: "Lisse par angle"
    MenuItem "objet.mode_edition"             label: "Mode Édition"             shortcut: "Tab"

  Menu "menu_maillage"                        label: "Maillage"       // remplace « Objet » en mode Édition
    MenuItem "edit.extruder"                  label: "Extruder"                 shortcut: "E"
    MenuItem "edit.inserer"                   label: "Insérer"                  shortcut: "I"
    MenuItem "edit.biseauter"                 label: "Biseauter"                shortcut: "Ctrl+B"
    MenuItem "edit.loop_cut"                  label: "Boucle de coupe"          shortcut: "Ctrl+R"
    MenuItem "edit.couteau"                   label: "Couteau"                  shortcut: "K"
    MenuItem "edit.bisect"                    label: "Couper en deux"
    MenuItem "edit.subdiviser"                label: "Subdiviser"
    Separator "sep_m1"
    MenuItem "edit.fusionner"                 label: "Souder"                   shortcut: "M"
    MenuItem "edit.dissoudre"                 label: "Dissoudre"
    MenuItem "edit.creer_face"                label: "Créer une face"           shortcut: "F"
    MenuItem "edit.spheriser"                 label: "Sphériser"                shortcut: "Maj+Alt+S"
    MenuItem "edit.gonfler"                   label: "Gonfler / dégonfler"      shortcut: "Alt+S"
    MenuItem "edit.symetrie"                  label: "Symétrie"
    Menu "menu_normales"                      label: "Normales"
      MenuItem "edit.normales_recalculer"     label: "Recalculer vers l'extérieur"   shortcut: "Maj+N"
      MenuItem "edit.normales_inverser"       label: "Inverser"
    Menu "menu_uv_edition"                    label: "UV"                       shortcut: "U"
      MenuItem "uv.marquer_couture"           label: "Marquer la couture"
      MenuItem "uv.effacer_couture"           label: "Effacer la couture"
      MenuItem "uv.deplier"                   label: "Déplier"
      MenuItem "uv.deplier_auto"              label: "Déplier automatiquement"
    Separator "sep_m2"
    MenuItem "edit.supprimer"                 label: "Supprimer…"               shortcut: "X"
    MenuItem "edit.mode_objet"                label: "Mode Objet"               shortcut: "Tab"

  Menu "menu_sculpture"                       label: "Sculpture"      // remplace « Objet » en modes de sculpture
    Menu "menu_masque"                        label: "Masque"
      MenuItem "sculpt.masque_inverser"       label: "Inverser"                 shortcut: "Ctrl+I"
      MenuItem "sculpt.masque_effacer"        label: "Effacer"                  shortcut: "Alt+M"
      MenuItem "sculpt.masque_flouter"        label: "Flouter"
    Menu "menu_multires"                      label: "Multirésolution"
      MenuItem "sculpt.multires_monter"       label: "Subdiviser"
      MenuItem "sculpt.multires_descendre"    label: "Niveau inférieur"
      MenuItem "sculpt.multires_appliquer"    label: "Appliquer"
    MenuItem "sculpt.remailler"               label: "Remailler"                shortcut: "Ctrl+R"
    MenuItem "sculpt.projeter_detail"         label: "Projeter le détail (2.5D → maillage)"

  Menu "menu_aide"                            label: "Aide"
    MenuItem "app.aide"                       label: "Documentation"
    MenuItem "app.raccourcis"                 label: "Raccourcis clavier"
    MenuItem "app.apropos"                    label: "À propos / Crédits et licences"
```
- `menu_objet.visible = (mode == "Objet")`, `menu_maillage.visible = (mode ==
  "Édition")`, `menu_sculpture.visible = (mode in ["Sculpture","Sculpture 2.5D","Volume"])`
  — idempotents ✅.
- ⚠️ **Ctrl+R** : boucle de coupe en Édition, **remailler** en Sculpture — même
  touche, modes différents, **comme Blender**. Le menu le montre ; `NkShortcutTable`
  porte le **contexte**.
- ⚠️ **Ctrl+I** : inverser la sélection **et** inverser le masque — idem, par mode.

### 5.2 `barre_outils.nkgui`

```
HBox "barre_outils"                           gap: 4   align: Center
  Button "app.enregistrer"                    icon: save   label: "Enregistrer"   tooltip: "Ctrl+S"
  Separator "sep1"                            orientation: Vertical
  Dropdown "espace.choix"                     bind: espace.actif
                                              items: ["Modélisation","Sculpture","UV","Matière","Peinture","Nœuds","Présentation"]
  Dropdown "app.selecteur_outil"              bind: mode.actif
                                              items: ["Objet","Édition","Sculpture","Sculpture 2.5D","Volume","Peinture de texture","Retopologie"]
  Separator "sep2"                            orientation: Vertical
  Button "app.ajouter"                        label: "Ajouter ▾"       icon: plus-circle   tooltip: "Maj+A"
  Button "mod.ajouter"                        label: "Modificateur ▾"  icon: sliders-horizontal
  Spacer "pousse"
  Button "ia.assistant"                       icon: sparkles   label: "Assistant"   tooltip: "Ctrl+K"
  Badge✚ "ia.en_attente"                      text: "0"   tone: info.apercu   tooltip: "Propositions en attente"
  Separator "sep3"                            orientation: Vertical
  Button "app.reglages"                       label: "Réglages ▾"   icon: settings
```
- `app.selecteur_outil` **existe déjà** comme action : il devient le sélecteur de
  **mode** ; si son sens actuel est autre chose (sélecteur d'outil de la vue),
  **signale-le avant d'écrire** : deux sens pour un identifiant serait pire qu'un
  renommage.
- `mod.ajouter.enabled = (mode == "Objet" and selection.nb > 0)` — idempotent ✅,
  raison sinon : « Sélectionne un objet ».
- `app.reglages` : aujourd'hui la zone est **déclarée sans `hit.Clicked`**
  (relevé du 26/09) → grisée avec « Pas encore disponible ».
- `ia.en_attente.visible = (ia.nb_en_attente > 0)` ✅.

### 5.3 `barre_etat.nkgui`

```
HBox "barre_etat"                             gap: 8   align: Center
  Button "app.vue.navigateur"                 icon: folder-open   label: "Tiroir"
  Button "fenetre.journal"                    icon: scroll-text   label: "Journal"
  TextField "commande"                        placeholder: "> commande"   bind: commande.saisie
  Spacer "pousse"
  Text "etat.mode"                            text: "Objet"
  Text "etat.compte"                          text: "S:0 A:0 F:0"          (sommets / arêtes / faces)
  Text "etat.selection"                       text: "sél. 0"
  Text "etat.actif"                           text: "—"
  Text "etat.ips"                             text: "— ips"
```
📌 **« Rien d'important n'existe seulement dans un journal »** : le mode, les
comptes, la sélection et l'actif sont **ici**, en continu, et **vrais en continu**
(UI_SPEC §1 : un compteur figé est pire qu'absent).

### 5.4 `vue_extra.nkgui` (dans `vue.barre_extra`)

```
HBox "vue_extra"                              gap: 2   align: Center
  HBox "vue.outils"                           gap: 1
    BoutonOutil "objet.selection_outil"       icon: mouse-pointer-2   tooltip: "Sélectionner (W)"
    BoutonOutil "objet.deplacer"              icon: move              tooltip: "Déplacer (G)"
    BoutonOutil "objet.tourner"               icon: rotate-3d         tooltip: "Tourner (R)"
    BoutonOutil "objet.echelle"               icon: scaling           tooltip: "Redimensionner (S)"
  Dropdown "vue.orientation"                  bind: vue.orientation   items: ["Globale","Locale","Normale"]   tooltip: "Orientation (,)"
  Dropdown "vue.pivot"                        bind: vue.pivot   items: ["Centre de la boîte","Curseur 3D","Origines individuelles","Point médian","Élément actif"]   tooltip: "Pivot (.)"
  Separator "sep1"                            orientation: Vertical
  HBox "vue.sous_modes"                       gap: 1   visible: false
    ToggleButton✚ "edit.sous_mode_sommet"     icon: dot            bind: edit.sm.sommet   combine✚: Shift   tooltip: "Sommets (1)"
    ToggleButton✚ "edit.sous_mode_arete"      icon: minus          bind: edit.sm.arete    combine✚: Shift   tooltip: "Arêtes (2)"
    ToggleButton✚ "edit.sous_mode_face"       icon: square         bind: edit.sm.face     combine✚: Shift   tooltip: "Faces (3)"
  HBox "vue.symetrie"                         gap: 1   visible: false
    Text "vue.symetrie_titre"                 text: "Symétrie"
    ToggleButton✚ "sculpt.sym_x"              label: "X"   bind: sculpt.sym.x
    ToggleButton✚ "sculpt.sym_y"              label: "Y"   bind: sculpt.sym.y
    ToggleButton✚ "sculpt.sym_z"              label: "Z"   bind: sculpt.sym.z
  Separator "sep2"                            orientation: Vertical
  ToggleButton✚ "vue.aimant"                  icon: magnet   bind: vue.aimant   tooltip: "Aimantation"
  Dropdown "vue.pas_grille"                   bind: vue.pas.grille   items: ["0,1","0,25","0,5","1","5","10"]
  Dropdown "vue.pas_angle"                    bind: vue.pas.angle    items: ["5°","10°","15°","45°","90°"]
  Dropdown "vue.pas_echelle"                  bind: vue.pas.echelle  items: ["0,1","0,25","0,5","1"]
  ToggleButton✚ "vue.edition_proportionnelle" icon: circle-dashed   bind: vue.proportionnelle   tooltip: "Édition proportionnelle (O)"
```
- `vue.sous_modes.visible = (mode == "Édition")` ; `vue.symetrie.visible = (mode in
  les trois modes de sculpture)` — idempotents ✅.
- Les sous-modes **combinables** : P20 (aujourd'hui trois bascules, la règle tenue
  par l'application).
- Les **axes** du repère et le **HUD** (« mode : Édition — 12 faces ») sont peints
  par l'application dans la vue.

### 5.5 `scene_filtres.nkgui` et `bibliotheque_filtres.nkgui`

```
HBox "scene.filtres"                          gap: 2
  ToggleButton✚ "scene.filtre_maillage"       icon: box          bind: scene.filtre.maillage   tooltip: "Maillages"
  ToggleButton✚ "scene.filtre_lumiere"        icon: lightbulb    bind: scene.filtre.lumiere    tooltip: "Lumières"
  ToggleButton✚ "scene.filtre_camera"         icon: video        bind: scene.filtre.camera     tooltip: "Caméras"
  ToggleButton✚ "scene.filtre_vide"           icon: move-3d      bind: scene.filtre.vide       tooltip: "Vides"
  Button "scene.nouveau_dossier"              icon: folder-plus  tooltip: "Nouveau dossier"
```
Colonnes de l'arbre (peintes par l'application dans `scene.arbre`) : œil,
**caméra** (visible dans les captures — ETAT_DES_LIEUX 3b), nom, **type**
(colonne calculée depuis un seul bord — commit du 25/09), cadenas. Pied : « 12
objets (2 sélectionnés) ».

```
VBox "biblio.filtres"                         gap: 2
  HBox "biblio.actions"                       gap: 4
    Button "contenu.ajouter"                  label: "+ Ajouter"          icon: plus
    Button "contenu.importer"                 label: "Importer"           icon: download
    Button "app.enregistrer_tout"             label: "Tout enregistrer"   icon: save
    Spacer "pousse"
    Button "contenu.favoris"                  icon: star   tooltip: "Favoris"
    Button "app.reglages_navigateur"          icon: settings   tooltip: "Réglages du navigateur"
  Host "biblio.chemin"                        hint: "fil d'Ariane avec icône de dossier et chevrons en glyphe"
  Flow "biblio.types"                         gap: 2
    ToggleButton✚ "contenu.filtre_maillage"   label: "Maillage"    bind: contenu.filtre.maillage
    ToggleButton✚ "contenu.filtre_animation"  label: "Animation"   bind: contenu.filtre.animation
    ToggleButton✚ "contenu.filtre_materiau"   label: "Matériau"    bind: contenu.filtre.materiau
    ToggleButton✚ "contenu.filtre_texture"    label: "Texture"     bind: contenu.filtre.texture
    ToggleButton✚ "contenu.filtre_generateur" label: "Générateur"  bind: contenu.filtre.generateur
  Slider "contenu.taille_vignettes"           bind: contenu.taille   min: 48   max: 192   tooltip: "Taille des vignettes"
  Text "contenu.compte"                       text: "0 éléments (0 sélectionné)"
```
📌 Les **écarts mesurés** entre le navigateur et sa planche (ROADMAP, tableau des
13 écarts : onglets Content Browser / Favorites / Recent, section Collections,
deux états de sélection des cartes, curseur de taille, compte `n items (m
selected)` en haut à gauche…) sont **la liste de travail** de ce document : ceux
marqués **NKGui** vont dans `Commun/` (§4 de ce document), les **spécifiques** ici.

### 5.6 Le panneau droit

#### `panneau_droit.nkgui`

```
Panel "panneau_droit"                         title: "Propriétés"
  VBox "colonne"                              gap: 0
    HBox "pd.pastilles"                       gap: 2
      Pastille "app.pastille_modele"          icon: box                  label: "Modèle"
      Pastille "app.pastille_rendu"           icon: sun                  label: "Rendu"
      Pastille "app.pastille_scene"           icon: layers               label: "Scène"
      Pastille "app.pastille_modificateur"    icon: sliders-horizontal   label: "Modificateur"
    TextField "pd.recherche"                  placeholder: "Rechercher une propriété…"   icon✚: search   bind: pd.recherche
    Scroll "pd.defil"                         axis: Vertical
      Host "pd.contenu"                       hint: "le document de la pastille active (pastille_modele / _rendu / _scene / _modificateur)"
```
**Recliquer la pastille active replie le panneau** (ROADMAP §1) : c'est une
**action** (`app.pastille_*` bascule), non idempotente → portée par l'action, pas
par un `behavior`.

#### `pastille_modele.nkgui` — l'ordre de la maquette

```
VBox "racine"
  Categorie "cat_transformation"              label: "Transformation"
    LigneTransformation "objet.position"      etiquette: "Position"   bind: objet.position
    LigneTransformation "objet.rotation"      etiquette: "Rotation"   bind: objet.rotation
    LigneTransformation "objet.echelle_val"   etiquette: "Échelle"    bind: objet.echelle
  Categorie "cat_dimensions"                  label: "Dimensions"
    LigneVec3 "objet.dimensions"              etiquette: "Dimensions"   bind: objet.dimensions
  Categorie "cat_relations"                   label: "Relations"
    LigneListe "objet.parent"                 etiquette: "Parent"   bind: objet.parent
    Host "objet.enfants"                      hint: "enfants de l'objet"
  Categorie "cat_materiaux"                   label: "Matériaux"
    Host "objet.materiaux"                    hint: "emplacements de matériaux : vignette, nom ; « Assigner à la sélection » en mode Édition"
    HBox "mat.boutons"                        gap: 4
      Button "mat.ajouter_emplacement"        icon: plus-circle    label: "Ajouter"
      Button "mat.retirer_emplacement"        icon: minus-circle   label: "Retirer"
      Button "mat.assigner"                   label: "Assigner à la sélection"   visible: false
  Categorie "cat_groupes"                     label: "Groupes de sommets"   expanded: false
    Host "objet.groupes"                      hint: "groupes de sommets"
  Categorie "cat_shapekeys"                   label: "Shape keys"   expanded: false
    Host "objet.shapekeys"                    hint: "shape keys : nom, valeur"
  Categorie "cat_uv"                          label: "Maps UV"   expanded: false
    Host "objet.uv"                           hint: "jeux d'UV, actif marqué"
  Categorie "cat_couleurs"                    label: "Attributs de couleur"   expanded: false
    Host "objet.couleurs"                     hint: "attributs de couleur"
  Categorie "cat_attributs"                   label: "Attributs"   expanded: false
    Host "objet.attributs"                    hint: "attributs du maillage"
  Categorie "cat_espace_texture"              label: "Espace texture"   expanded: false
    LigneVec3 "objet.espace_texture_loc"      etiquette: "Emplacement"   bind: objet.espace_texture.loc
    LigneVec3 "objet.espace_texture_taille"   etiquette: "Taille"        bind: objet.espace_texture.taille
  Categorie "cat_donnees"                     label: "Données géométriques"   expanded: false
    Text "objet.nb_sommets"                   text: "Sommets : —"
    Text "objet.nb_faces"                     text: "Faces : —"
    Text "objet.nb_aretes"                    text: "Arêtes : —"
```
- **Le rare est replié** (règle 2) : tout ce qui suit Matériaux est fermé par défaut.
- `mat.assigner.visible = (mode == "Édition")` ✅.
- NKCraft **n'anime pas** : toutes les lignes → `cle.visible = false` (les
  `KeyDiamond` des lignes communes).

#### `pastille_modificateur.nkgui`

```
VBox "racine"                                 gap: 4
  HBox "pm.tete"                              gap: 4
    Button "mod.ajouter"                      label: "+ Ajouter ▾"   icon: plus
    Spacer "pousse"
    Button "mod.appliquer_tout"               label: "Tout appliquer"   icon: check-check
  Text "pm.vide"                              text: "Aucun modificateur. « + Ajouter » en pose un — la pile s'évalue de haut en bas."   wrap: true   visible: false
  VBox "pm.pile"                              gap: 2     (CarteModificateur par modificateur, ajoutées par l'application, dans l'ordre d'évaluation)
  Categorie "pm.outil"                        label: "Réglages de l'outil"   expanded: false
    Host "pm.outil_contenu"                   hint: "réglages de l'outil courant (en attendant leur vraie place dans le panneau T — ROADMAP §1)"
```
- `pm.vide.visible = (mod.nb == 0)` — **les états vides parlent** (règle 3) ✅.

#### `pastille_rendu.nkgui` et `pastille_scene.nkgui`

Décrire **ce que le code peint aujourd'hui**, groupe par groupe, sans l'enrichir :
Rendu → **Ambiance** (intensité, couleur, source), **Brouillard** (actif, loi,
couleur, densité ou début/fin), **Ombres** (qualité, mise à jour…), **Ciel**,
**Capture** (source Vue / Caméra active, résolution 1920×1080 + préréglages + %,
chemin, format). Scène → unités, grille, caméra active, collections,
**webcam en incrustation** (source, forme, position), **enregistrement**.
Lis `NkModelerProperties.h` et **relève** chaque ligne ; une ligne que tu n'as pas
vue peinte **ne s'écrit pas**.

### 5.7 Le panneau T

#### `panneau_t.nkgui`

```
Panel "panneau_t"                             title: "Outils"
  VBox "colonne"                              gap: 4
    Host "pt.contenu"                         hint: "le document du mode : panneau_t_objet / _edition / _sculpture / _peinture / _retopo"
```
Refermé par défaut en Objet ; **ouvert seul** en Sculpture (UI_SPEC §9bis) — c'est
la **disposition** qui le fait, pas un `visible`.

#### `panneau_t_objet.nkgui` et `panneau_t_edition.nkgui`

```
VBox "racine"                                 gap: 2
  BoutonOutil "objet.selection_outil"         icon: mouse-pointer-2   label: "Sélectionner"
  BoutonOutil "objet.curseur"                 icon: crosshair         label: "Curseur 3D"
  BoutonOutil "objet.deplacer"                icon: move              label: "Déplacer"
  BoutonOutil "objet.tourner"                 icon: rotate-3d         label: "Tourner"
  BoutonOutil "objet.echelle"                 icon: scaling           label: "Redimensionner"
  // — panneau_t_edition ajoute : —
  Separator "sep_edit"
  BoutonOutil "edit.extruder"                 icon: arrow-up-from-square   label: "Extruder"
  BoutonOutil "edit.biseauter"                icon: bevel✚                  label: "Biseauter"
  BoutonOutil "edit.inserer"                  icon: square-dashed          label: "Insérer"
  BoutonOutil "edit.couteau"                  icon: slice                  label: "Couteau"
  BoutonOutil "edit.loop_cut"                 icon: between-horizontal-start   label: "Boucle de coupe"
  BoutonOutil "edit.glisser"                  icon: move-horizontal        label: "Glisser l'arête"
  BoutonOutil "edit.lisser"                   icon: waves                  label: "Lisser"
  BoutonOutil "edit.poinconner"               icon: stamp                  label: "Poinçonner"
  Categorie "pt.reglages"                     label: "Réglages de l'outil"
    Host "pt.reglages_contenu"                hint: "engendrés depuis NkModParam de l'outil courant"
```
**Tout icône a un mot** : `label` est affiché. ⚠️ `bevel` n'est pas une icône
Lucide : prends la plus proche et signale-le.

#### `panneau_t_sculpture.nkgui`

```
VBox "racine"                                 gap: 4
  Text "ps.brosses_titre"                     text: "Brosses"
  Grid "ps.brosses"                           columns: 4   gap: 4
    VignetteBrosse "sculpt.brosse_elever"     image: "brosses/elever"    label: "Élever"
    VignetteBrosse "sculpt.brosse_creuser"    image: "brosses/creuser"   label: "Creuser"
    VignetteBrosse "sculpt.brosse_lisser"     image: "brosses/lisser"    label: "Lisser"
    VignetteBrosse "sculpt.brosse_pincer"     image: "brosses/pincer"    label: "Pincer"
    VignetteBrosse "sculpt.brosse_gonfler"    image: "brosses/gonfler"   label: "Gonfler"
    VignetteBrosse "sculpt.brosse_aplanir"    image: "brosses/aplanir"   label: "Aplanir"
    VignetteBrosse "sculpt.brosse_masque"     image: "brosses/masque"    label: "Masque"
    VignetteBrosse "sculpt.brosse_peindre"    image: "brosses/peindre"   label: "Peindre"
  LigneNombre "sculpt.rayon"                  etiquette: "Rayon (F)"         bind: sculpt.rayon   min: 1   max: 500
  LigneNombre "sculpt.force"                  etiquette: "Force (Maj+F)"     bind: sculpt.force   min: 0   max: 1
  LigneNombre "sculpt.espacement"             etiquette: "Espacement"        bind: sculpt.espacement   min: 0.01   max: 1
  Text "ps.courbe_titre"                      text: "Atténuation"
  HBox "ps.courbes"                           gap: 4     (VignetteCourbe × 5, noms de NkSculptTypes.h)
  Categorie "ps.masque"                       label: "Masque"
    HBox "ps.masque_boutons"                  gap: 4
      Button "sculpt.masque_inverser"         label: "Inverser"
      Button "sculpt.masque_effacer"          label: "Effacer"
      Button "sculpt.masque_flouter"          label: "Flouter"
  Categorie "ps.multires"                     label: "Multirésolution"   expanded: false
    Text "ps.niveau"                          text: "Niveau : 0 / 0"
    HBox "ps.multires_boutons"                gap: 4
      Button "sculpt.multires_monter"         label: "Subdiviser"
      Button "sculpt.multires_descendre"      label: "Descendre"
      Button "sculpt.multires_appliquer"      label: "Appliquer"
```
Toutes les lignes : `cle.visible = false`.
⚠️ Les **brosses pixol** (Sculpture 2.5D) sont les mêmes huit modes : **un seul
document**, l'application sait quel moteur reçoit la commande.

#### `panneau_t_peinture.nkgui`, `panneau_t_retopo.nkgui`

Même patron : outils en `BoutonOutil` avec libellé, puis réglages de l'outil
engendrés. Peinture : Pinceau, Gomme, Pochoir, Projection, Pipette ; couleur
(`LigneCouleur`), taille, force, opacité. Retopologie : Dessiner un quad,
Étendre, Relaxer, Souder à la surface ; densité cible.

### 5.8 `derniere_operation.nkgui`

```
Panel "derniere_operation"                    title: "?"          (le nom de la dernière commande : « Extruder »)
  VBox "colonne"                              gap: 2
    Host "do.parametres"                      hint: "paramètres de la dernière commande, engendrés depuis NkModParam ; toute modification rejoue la commande"
```
Flottant en **bas à gauche de la vue**, repliable, **F9** l'ouvre en fenêtre.
`derniere_operation.visible = app.derniere_operation_existe` ✅.

### 5.9 `menus_circulaires.nkgui`

```
PieMenu✚ "pie_modes"                          label: "Mode"          // Ctrl+Tab
  MenuItem "mode.objet"                       label: "Objet"
  MenuItem "mode.edition"                     label: "Édition"
  MenuItem "mode.sculpture"                   label: "Sculpture"
  MenuItem "mode.sculpture_25d"               label: "Sculpture 2.5D"
  MenuItem "mode.volume"                      label: "Volume"
  MenuItem "mode.peinture"                    label: "Peinture de texture"
  MenuItem "mode.retopo"                      label: "Retopologie"

PieMenu✚ "pie_pivot"                          label: "Pivot"         // .
  MenuItem "vue.pivot_boite"                  label: "Centre de la boîte"
  MenuItem "vue.pivot_curseur"                label: "Curseur 3D"
  MenuItem "vue.pivot_individuel"             label: "Origines individuelles"
  MenuItem "vue.pivot_median"                 label: "Point médian"
  MenuItem "vue.pivot_actif"                  label: "Élément actif"

PieMenu✚ "pie_orientation"                    label: "Orientation"   // ,
  MenuItem "vue.orientation_globale"          label: "Globale"
  MenuItem "vue.orientation_locale"           label: "Locale"
  MenuItem "vue.orientation_normale"          label: "Normale"
```

### 5.10 `uv_editeur.nkgui`

```
Panel "uv_editeur"                            title: "UV"
  VBox "colonne"                              gap: 0
    HBox "uv.barre"                           gap: 4   align: Center
      Dropdown "uv.jeu"                       bind: uv.jeu_actif   (items : jeux d'UV)
      Button "uv.ajouter_jeu"                 icon: plus   tooltip: "Nouveau jeu d'UV"
      Separator "sep1"                        orientation: Vertical
      Button "uv.marquer_couture"             label: "Couture"          icon: scissors
      Button "uv.effacer_couture"             label: "Effacer couture"  icon: eraser
      Button "uv.deplier"                     label: "Déplier"          icon: unfold-horizontal
      Button "uv.deplier_auto"                label: "Auto"             icon: wand-2
      Button "uv.empaqueter"                  label: "Empaqueter"       icon: package
      Button "uv.redresser"                   label: "Redresser"        icon: square
      Separator "sep2"                        orientation: Vertical
      RadioGroup "uv.affichage"               bind: uv.affichage   options: ["Damier","Étirement","Texture"]   orientation: Horizontal
      LigneNombre "uv.marge"                  etiquette: "Marge"   bind: uv.marge   min: 0   max: 0.1
    Host "uv.canevas"                         hint: "l'espace UV : îlots, sélection synchronisée avec la vue 3D, damier ou carte d'étirement (@uv.etirement.*)"
```
`uv.marge` : `cle.visible = false`.

### 5.11 Matière, calques, cuisson

#### `matiere.nkgui`

**La même brique que Nogee** (`mat_editeur`, doc 06 §5.10), **montée dans un
panneau** et non dans une fenêtre de document : le graphe **en bas** de l'espace
Matière. Ne réécris pas l'éditeur : **inclus** `Commun/mat_editeur_corps.nkgui`
(à extraire de `Nogee/mat_editeur.nkgui` : barre + graphe + palette + statistiques,
sans la `Window`), et ajoute à côté :

```
Panel "bibliotheque_matieres"                 title: "Matières"
  VBox "colonne"                              gap: 4
    TextField "bm.recherche"                  placeholder: "Rechercher une matière…"   bind: bm.recherche
    ListBox "bm.categorie"                    bind: bm.categorie   items: ["Métal","Bois","Pierre","Peau","Tissus","Plastique","Verre","Émissif"]
    Scroll "bm.defil"
      Flow "bm.grille"                        gap: 4     (Tile par matière : sphère d'aperçu, nom ; glisser sur un objet ou un sous-mesh)
```
Les **tissus** comprennent d'office **wax, kente, ndop, bogolan** (doc 01 §4.4).

#### `calques_texture.nkgui`

```
Panel "calques_texture"                       title: "Calques"
  VBox "colonne"                              gap: 4
    HBox "ct.barre"                           gap: 2
      Dropdown "ct.canal"                     bind: ct.canal   items: ["Couleur","Rugosité","Métal","Normales","Émission"]
      Button "tex.calque_ajouter"             icon: plus    tooltip: "Nouveau calque"
      Button "tex.calque_supprimer"           icon: trash-2 tooltip: "Supprimer le calque"
      Button "tex.masque_ajouter"             icon: square-dashed-bottom   tooltip: "Ajouter un masque"
    Host "ct.liste"                           hint: "calques : œil, vignette, nom, masque ; glisser pour réordonner"
    LigneNombre "tex.opacite"                 etiquette: "Opacité"   bind: tex.calque.opacite   min: 0   max: 1
    LigneListe "tex.fusion"                   etiquette: "Fusion"    bind: tex.calque.fusion   items: ["Normal","Multiplier","Superposition","Ajouter","Écran"]
```

#### `cuisson.nkgui`

```
Window "cuisson"                              title: "Cuire des textures"   modal: true
  VBox "colonne"                              gap: 6
    LigneAsset "cuis.source"                  etiquette: "Source (haute définition)"   bind: cuis.source   type: "Maillage"
    LigneAsset "cuis.cible"                   etiquette: "Cible (basse définition)"    bind: cuis.cible    type: "Maillage"
    Text "cuis.cartes_titre"                  text: "Cartes à produire"
    Flow "cuis.cartes"                        gap: 4
      Checkbox "cuis.normales"                label: "Normales"     bind: cuis.carte.normales
      Checkbox "cuis.occlusion"               label: "Occlusion"    bind: cuis.carte.occlusion
      Checkbox "cuis.courbure"                label: "Courbure"     bind: cuis.carte.courbure
      Checkbox "cuis.epaisseur"               label: "Épaisseur"    bind: cuis.carte.epaisseur
      Checkbox "cuis.id"                      label: "Identifiants" bind: cuis.carte.id
    LigneListe "cuis.taille"                  etiquette: "Taille"   bind: cuis.taille   items: ["512","1024","2048","4096"]
    LigneNombre "cuis.marge"                  etiquette: "Marge (px)"   bind: cuis.marge   min: 0   max: 64
    LigneNombre "cuis.distance"               etiquette: "Distance de projection"   bind: cuis.distance
    HBox "cuis.boutons"                       gap: 4   justify: End
      Button "cuis.annuler"                   label: "Annuler"
      BoutonEcrit "outils.cuire_textures"     label: "Cuire"   icon: flame
    Progress "cuis.progression"               bind: cuis.progression   visible: false
```
`BoutonEcrit` : cuire **écrit** des textures sur disque et les lie au modèle.

### 5.12 Les générateurs

#### `generateur.nkgui` (cadre commun)

```
Panel "generateur"                            title: "?"          (« Palmier », « Rocher »…)
  VBox "colonne"                              gap: 4
    HBox "gen.barre"                          gap: 4
      Dropdown "gen.prereglage"               bind: gen.prereglage   (items : préréglages du générateur)
      Button "gen.aleatoire"                  icon: dices   label: "Au hasard"
      Button "gen.voir_graphe"                icon: workflow   label: "Voir le graphe"
    Scroll "gen.defil"                        axis: Vertical
      Host "gen.parametres"                   hint: "curseurs du générateur, ENGENDRÉS depuis les entrées exposées de son graphe, regroupés par catégorie"
    HBox "gen.fin"                            gap: 4   justify: End
      BoutonEcrit "gen.figer"                 label: "Figer en modèle"   icon: snowflake
```
- **Les curseurs sont engendrés** depuis le graphe du générateur (comme les
  modificateurs depuis `NkModParam`) : un générateur nouveau ne demande **aucun**
  document.
- `gen.figer` écrit le modèle au journal : `BoutonEcrit`.

#### `generateur_personnage.nkgui`

Le seul générateur **écrit à la main**, parce que ses catégories sont une
**décision de conception** (et pas une conséquence du graphe) :

```
VBox "racine"
  HBox "gp.barre"                             gap: 4
    Dropdown "gen.prereglage"                 bind: gen.prereglage   (items : préréglages de morphologie, TOUTES origines)
    Button "gen.aleatoire"                    icon: dices   label: "Au hasard"
    ToggleButton✚ "gp.symetrie"               icon: flip-horizontal-2   label: "Symétrie"   bind: gp.symetrie
  TabBar "gp.onglets"                         bind: gp.onglet   tabs: ["Corps","Visage","Peau","Cheveux"]
  Scroll "gp.defil"                           axis: Vertical
    Host "gp.contenu"                         hint: "curseurs de l'onglet, groupés (Visage ▸ Yeux, Nez, Bouche, Oreilles, Mâchoire, Front, Joues…), engendrés depuis le graphe"
  HBox "gp.fin"                               gap: 4   justify: End
    BoutonEcrit "gen.figer"                   label: "Figer en modèle"   icon: snowflake
```
`gp.symetrie` décoché → les curseurs du visage se dédoublent gauche / droite
(un visage réel n'est pas symétrique).

### 5.13 `assistant_extra.nkgui` (dans l'Assistant commun)

L'Assistant commun (`Commun/assistant_ia.nkgui`, NkAnima 09 §5.13) reçoit un
`Host "ia.extra"` au-dessus de la zone de saisie, où NKCraft monte :

```
TabBar "ia.mode"                              bind: ia.mode   tabs: ["Faire","Créer"]
```
et, **quand `ia.mode == "Créer"`**, remplace le champ de saisie par :

```
VBox "ia.creer"                               gap: 4   visible: false
  HBox "ia.creer_source"                      gap: 4
    RadioGroup "ia.creer_depuis"              bind: ia.creer_depuis   options: ["Une image","Une phrase"]   orientation: Horizontal
    Button "ia.choisir_image"                 label: "Choisir une image…"   icon: image
  TextField "ia.creer_phrase"                 placeholder: "Un masque fang en bois, patiné"   bind: ia.creer_phrase
  Button "ia.generer"                         label: "Générer"   icon: sparkles
  VBox "ia.generations"                       gap: 6     (CarteGeneration par génération)
```
- `ia.creer.visible = (ia.mode == "Créer")` ✅ ; `ia.choisir_image.visible =
  (ia.creer_depuis == "Une image")` ✅.
- En mode **Faire**, les cartes sont les `CartePropo` communes, avec en plus
  **« Défaire ce prompt »** (`ia.defaire_prompt`) sur les cartes **appliquées** —
  l'annulation par lot (SPECIFICATION §4.5).

### 5.14 `dispositions.nkgui`

```
layout "modelisation"
  dock "scene" left 0.16 · dock "vue" center · dock "panneau_droit" right 0.24 · dock "bibliotheque" bottom 0.26
layout "sculpture"
  dock "panneau_t" left 0.14 · dock "vue" center · dock "panneau_droit" right 0.20
layout "uv"
  dock "scene" left 0.14 · dock "vue" center · dock "uv_editeur" right 0.40 · dock "panneau_droit" right-bottom 0.40
layout "matiere"
  dock "scene" left 0.14 · dock "vue" center · dock "panneau_droit" right 0.22 · dock "matiere" bottom 0.40 · dock "bibliotheque_matieres" bottom-right 0.25
layout "peinture"
  dock "panneau_t" left 0.14 · dock "vue" center · dock "calques_texture" right 0.22
layout "noeuds"
  dock "scene" left 0.14 · dock "vue" center · dock "panneau_droit" right 0.22 · dock "graphe_modelisation" bottom 0.40
layout "presentation"
  dock "scene" left 0.16 · dock "vue" center · dock "panneau_droit" right 0.24      (pastille Rendu active)
```
- **La hiérarchie seule à gauche** (UI_SPEC §2.2, règle 1) dans Modélisation ;
  dans Sculpture et Peinture, le **panneau T** prend la gauche (il faut de la
  hauteur pour les brosses) et la hiérarchie se replie. Si Rodolf veut la règle
  « hiérarchie seule à gauche » **partout**, le panneau T se range à droite de la
  vue : **signale le choix, ne le tranche pas**.
- `derniere_operation` et les `PieMenu` ne sont dans aucune disposition (flottants).

---

## 6. Les actions

Légende : ✅ servie aujourd'hui · ✚ à créer.

### 6.1 Servies (gardées telles quelles)

Toutes celles du §0.2 ✅, et `outils.recuire_textures` ✅ (« déjà câblée »).

### 6.2 À créer — toutes ✚

| domaine | actions |
|---|---|
| **app** | `nouveau_projet` · `ouvrir` · `enregistrer_sous` · `importer_obj` · `importer_gltf` · `importer_fbx` · `importer_dae` · `importer_ply` · `importer_stl` · `importer_usda` · `exporter_gltf` · `exporter_fbx` · `exporter_obj` · `quitter` · `historique` · `repeter` · `ajuster_derniere` · `preferences` · `raccourcis` · `ajouter` · `reglages` · `reglages_navigateur` · `pastille_modele` · `pastille_rendu` · `pastille_scene` · `pastille_modificateur` |
| **espace** | `modelisation` · `sculpture` · `uv` · `matiere` · `peinture` · `noeuds` · `presentation` |
| **mode** | `objet` · `edition` · `sculpture` · `sculpture_25d` · `volume` · `peinture` · `retopo` |
| **sel** | `inverser` · `boucle` · `anneau` · `lies` · `meme_materiau` · `agrandir` · `reduire` |
| **objet** | `selection_outil` · `curseur` · `reinitialiser` · `copier_valeur` · `appliquer_tout` · `origine_geometrie` · `origine_curseur` · `joindre` · `separer` · `ombrage_plat` · `ombrage_lisse` · `ombrage_angle` |
| **edit** | `couteau` · `symetrie` · `normales_recalculer` · `normales_inverser` · `glisser` · `lisser` · `poinconner` |
| **mod** | `ajouter` · `appliquer_tout` · `monter` · `descendre` · `menu` · `retirer` · `appliquer` |
| **mat** | `ajouter_emplacement` · `retirer_emplacement` · `assigner` |
| **uv** | `marquer_couture` · `effacer_couture` · `deplier` · `deplier_auto` · `empaqueter` · `redresser` · `ajouter_jeu` |
| **tex** | `calque_ajouter` · `calque_supprimer` · `masque_ajouter` |
| **cuis** | `annuler` |
| **sculpt** | `brosse_elever` · `brosse_creuser` · `brosse_lisser` · `brosse_pincer` · `brosse_gonfler` · `brosse_aplanir` · `brosse_masque` · `brosse_peindre` · `courbe_*` (×5) · `masque_inverser` · `masque_effacer` · `masque_flouter` · `multires_monter` · `multires_descendre` · `multires_appliquer` · `remailler` · `projeter_detail` |
| **vue** | `pivot_boite` · `pivot_curseur` · `pivot_individuel` · `pivot_median` · `pivot_actif` · `orientation_globale` · `orientation_locale` · `orientation_normale` |
| **outils** | `retopo_auto` · `retopo_manuelle` · `decimer` · `lod` · `cuire_textures` · `recharger_extensions` |
| **gen** | `personnage` · `vegetation` · `rocher` · `batiment` · `aleatoire` · `voir_graphe` · `figer` |
| **ia** | `generer` · `choisir_image` · `importer_generation` · `regenerer` · `defaire_prompt` (+ `assistant` · `proposer` · `apercu` · `appliquer` · `rejeter` de l'Assistant commun) |
| **contenu** | `ajouter` · `importer` · `favoris` (+ celles de Nogee 06 déjà communes) |
| **scene** | `nouveau_dossier` |
| **fenetre** | `journal` · `reinitialiser` |
| **commun** | `reinitialiser_propriete` · `cle_propriete` (non servie ici : masquée) · `parcourir` · `utiliser_selection` · `effacer_reference` |

### 6.3 Raisons de grisé — **les trois motifs du relevé du 26/09**

| motif | raison affichée |
|---|---|
| **a — la fonction n'existe pas** | « Pas encore disponible » |
| **b — sous-menu sans contenu** | « Aucune entrée pour l'instant » |
| **c — la fonction existe mais son chemin n'est pas partageable** (`objet.dupliquer`, `objet.supprimer`, `mod.ajouter` depuis le menu) | « Disponible par le clic droit de la hiérarchie » / « Disponible par le bouton Modificateur de la barre d'outils » — **dire où elle marche**, pas seulement qu'elle ne marche pas ici |
| mode inadapté | « Passe en mode Édition (Tab) » / « Passe en mode Objet (Tab) » |
| sans sélection | « Sélectionne un objet » |

📌 Le motif **c** est le plus utile à l'utilisateur : la fonction **existe**, il
faut lui dire **où**.

---

## 7. Ce que tu ne sais pas encore reproduire — écrit quand même

### 7.1 États

Ceux des trois autres documents, plus :

| élément | état | rendu attendu |
|---|---|---|
| `Pastille` | `Active` | fond `@accent` (**bleu = état de l'interface**) |
| ligne de hiérarchie | `Selected` / `Active` | liseré `@selection.3d` / texte `@selection.actif` — **trois états** (UI_SPEC §3.4) |
| carte d'asset | `Active` / `Selected` | contour épais / contour fin ou voile (ROADMAP, écart n° 3 : **deux marques distinctes**) |
| `CarteModificateur` | `Disabled` (éteint) | grisée, **visible** |
| `VignetteBrosse` | `Active` | liseré `@accent` 2 px |
| `ToggleButton` des sous-modes | `Active` combiné | les deux ou trois allumés ensemble |

### 7.2 Animations

| élément | animation |
|---|---|
| `derniere_operation` | apparaît en fondu (120 ms) après chaque commande à paramètres |
| `PieMenu` | s'ouvre en rayon depuis le curseur (100 ms) |
| `CarteGeneration` | l'aperçu **tourne** (un tour en 6 s) |

### 7.3 Événements (lot b)

| élément | événement | effet | idempotent ? |
|---|---|---|---|
| `Pastille` | `on Click` sur l'active | replier le panneau | ❌ |
| `mode.choix` / `app.selecteur_outil` | `on Changed` | changer de mode (et de menus) | ❌ |
| paramètre de `derniere_operation` | `on Changed` | **rejouer** la commande | ❌ |
| `Tile` de matière | `on Drop` (dans la vue) | assigner à l'objet / au sous-mesh sous le curseur | ❌ |
| `PieMenu` | `on Release` | choisir l'entrée dans la direction | ❌ |

---

## 8. L'ordre de livraison

**Celui du doc 01 §6.2** (panneau par panneau, derrière `NK_CRAFT_UI=doc`, témoin
par **actions émises et valeurs liées**, puis suppression de l'ancien code) :

| # | livrable | témoin |
|---|---|---|
| 1 | `menus` | les 19 entrées : les 7 câblées émettent **la même action** que leur raccourci ; les 11 grisées portent **leur motif** |
| 2 | `barre_outils` + `barre_etat` | les comptes S/A/F **identiques** à l'ancienne barre sur 10 scènes |
| 3 | `bibliotheque_filtres` (et la carte commune) | les écarts **NKGui** du tableau des 13 écarts fermés dans `Commun/` |
| 4 | `scene_filtres` | |
| 5 | `panneau_droit` + `pastille_modele` + `pastille_modificateur` | ajouter un 18ᵉ modificateur (fictif) : **zéro ligne de document** |
| 6 | `vue_extra` + `panneau_t_*` + `derniere_operation` + `menus_circulaires` | Jalon B |
| 7 | `pastille_rendu` + `pastille_scene` | relevé ligne à ligne de `NkModelerProperties.h` |
| 8 | `uv_editeur` + `matiere` + `calques_texture` + `cuisson` + `generateur*` + `assistant_extra` + `dispositions` | P3–P6 |

📌 Après l'étape 2, publie `apparencesNonPeintes`, `etatsNonAppliques` **et le
nombre de lignes de `Shell/` supprimées** : c'est le vrai compteur de ce chantier.
