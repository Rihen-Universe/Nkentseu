# Nogee — l'interface, pour l'agent qui écrit les `.nkgui`

### Document 6 — Spécification d'interface côté agent

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> Traduction du **05** en tes quatre points : **arbre nommé**, **composants**,
> **actions**, **couleurs porteuses** — plus ce que tu ne sais pas encore
> reproduire, écrit quand même.
> **Si 05 et 06 se contredisent, 05 gagne** : signale l'écart.
>
> **Prérequis** : le **09** de NkAnima (notation, P1–P12, jetons, composants) et le
> **03** de NKScena (le dossier `Commun/`, P13–P15). Ce document ne les répète
> pas.
>
> **`include` est branché** (26/09, 18 h 54) : tout ce qui est commun s'écrit une
> fois.

---

## 0. Ce qui change par rapport à NkAnima et NKScena

1. **Les documents de Nogee remplacent les deux existants** :
   `Resources/Interface/Nogee/barre_etat.nkgui` et `panneau_scene.nkgui` (des
   démonstrations : « ce panneau est décrit par un fichier ») sont **réécrits** ;
   leur `Host "scene.entites"` est conservé dans la barre d'état (§5.4).
2. **Le panneau Détails de Nogee est engendré par la réflexion.** Les propriétés
   d'un composant de moteur (et celles qu'un Blueprint ou une classe C++ expose)
   ne sont **pas** écrites à la main dans un `.nkgui` : l'application les lit par
   **NKReflection** et monte, pour chacune, **le composant de ligne commun** qui
   correspond à son type (`LigneNombre`, `LigneVec3`, `LigneListe`, `LigneCase`,
   `LigneCouleur`, `LigneAsset`…). **Tu écris le cadre et les composants de ligne ;
   l'application écrit les lignes.**
   📌 C'est ce qui rend l'éditeur extensible : une propriété ajoutée en C++
   apparaît dans Détails **sans toucher à un document**.
3. **Quatre panneaux communs s'ouvrent à l'extension**, sur le modèle de
   `scene.filtres` (NKScena 03 §4.1) — **un `Host` que chaque application
   remplit**, au lieu d'une copie :
   | panneau commun | `Host` d'extension | Nogee y monte |
   |---|---|---|
   | `vue_3d.nkgui` | `vue.barre_extra` | `vue_extra.nkgui` (modes Collisions / Navigation, aimantations, statistiques) |
   | `scene.nkgui` | `scene.filtres` | `scene_filtres.nkgui` |
   | `bibliotheque.nkgui` | `biblio.filtres` (remplace la `ListBox` de catégories de NkAnima) | `bibliotheque_filtres.nkgui` |
   | `details.nkgui` | `details.entete_extra` (au-dessus de la recherche) | `details_composants.nkgui` (l'arbre des composants de l'acteur) |
   ⚠️ Pour NkAnima, cela veut dire **déplacer** ses catégories de bibliothèque dans
   `NkAnimaEditor/bibliotheque_filtres.nkgui`. Aucun comportement ne change.
4. **La brique Monde est commune** (doc 04 D4) : les outils de terrain, de
   végétation, d'eau et de ciel vont dans `Commun/monde_*.nkgui`, et **NKScena
   déplace** ses `details_dispersion` et `details_ciel` (03 §5.6) vers ces
   fichiers.
5. **Le Journal devient un panneau commun** (`Commun/journal.nkgui`) : NkAnima et
   NKScena l'appellent déjà par `fenetre.journal` sans que son arbre ait été écrit.
6. **Actions de Nogee**, par domaine : `fichier.` `edition.` `fenetre.` `outils.`
   `construire.` `selection.` `acteur.` `aide.` `mode.` `ajouter.` `jeu.` `bp.`
   `mat.` `comportement.` `maillage.` `son.` `table.` `phys.` `plateforme.`
   `reglages.` `vue.` `contenu.` `scene.` `lanceur.` `ia.` `monde.`.

---

## 1. Le vocabulaire proposé en plus

P1–P12 : NkAnima 09. P13–P15 : NKScena 03. Nogee en ajoute **trois**.

### P16 — `AssetField`

- **Propriétés** : `bind` (une référence d'asset) `type` (étiquette : Maillage,
  Matériau, Son, Texture, Blueprint, Niveau, Animation, Interface, Table…)
  `allowNone` (booléen).
- **Forme** : vignette (32 px), nom, et trois boutons : **parcourir** (liste
  filtrée par `type`), **utiliser la sélection du tiroir**, **effacer**.
- **Événements** : `Changed`, `Drop` (un asset glissé du tiroir, refusé s'il n'a
  pas le bon `type`), `DoubleClick` (ouvre l'asset).
- **Pourquoi** : c'est le champ le plus fréquent du panneau Détails d'un moteur,
  et il fait quatre choses qu'un `Dropdown` ne fait pas (vignette, glisser-déposer
  typé, prendre la sélection, ouvrir).
- **En attendant** : composant `ChampAsset` (§3.2).

### P17 — `KeyCaptureField`

- **Propriétés** : `bind` (une combinaison : touche + modificateurs, ou bouton de
  manette, ou axe).
- **Comportement** : clic → « Appuyez sur une touche… » ; la prochaine touche
  (ou bouton, ou mouvement d'axe) est capturée ; Échap annule ; un conflit avec
  une autre action est **signalé** (`@info.alerte`).
- **Pourquoi** : les **cartes d'entrées** du projet. Taper « Maj+Espace » dans un
  `TextField` est une source d'erreurs ; capturer la touche ne l'est pas.
- **En attendant** : `TextField` avec `placeholder: "Maj+Espace"`.

### P18 — `SplitButton`

- **Propriétés** : `label` `icon` (l'action principale) ; enfants `MenuItem`
  (les variantes, dans la flèche ▾).
- **Pourquoi** : **▶ Jouer ▾** — un clic joue avec le dernier réglage, la flèche
  choisit un autre réglage. C'est la signature d'UE5, et deux boutons côte à côte
  ne disent pas que la flèche règle le bouton.
- **Événements** : `Click` (principal), et chaque `MenuItem` a son action.
- **En attendant** : `HBox` d'un `Button` (action principale) et d'un `Button`
  « ▾ » qui ouvre un `ContextMenu` (non monté : attendu).

---

## 2. Les jetons propres à Nogee

À ajouter au thème « UE5 Rihen », sans rien retirer.

| jeton | Sombre | où | pourquoi |
|---|---|---|---|
| `@jeu.en_cours` | `#3FB950` | liseré de la Vue pendant le jeu ; fond du bouton ▶ au repos ; texte « EN JEU » | ce qui change en jeu est perdu à l'arrêt |
| `@jeu.pause` | `#D29922` | liseré de la Vue en pause ; « PAUSE » | idem |
| `@jeu.simulation` | `#2EC4D6` | liseré de la Vue en simulation ; « SIMULATION » | idem |
| `@jeu.arret` | `#F85149` | bouton ⏹ | arrêter perd l'état |
| `@compile.ok` · `@compile.a_faire` · `@compile.erreur` | `#3FB950` · `#D29922` · `#F85149` | `PastilleCompile` (bouton Compiler, barre d'état, vignettes de Blueprint) | ce qu'on voit n'est peut-être pas ce qui tourne |
| `@plateforme.prete` · `@plateforme.incomplete` · `@plateforme.absente` | `#3FB950` · `#D29922` · `#6E7681` | `PastillePlateforme` | ce qui manque avant de construire |
| `@pin.execution` `@pin.booleen` `@pin.entier` `@pin.reel` `@pin.texte` `@pin.vecteur` `@pin.objet` `@pin.structure` | `#FFFFFF` `#C1272D` `#1FB5A5` `#8BC34A` `#E056C3` `#E8C33A` `#3A8EE6` `#1F4E9C` | broches des graphes (peintes par l'application) ; pastille de type d'une variable dans « Mon Blueprint » | le type décide de ce qui se branche |
| `@noeud.evenement` `@noeud.fonction` `@noeud.pur` `@noeud.flux` | `#8B1E1E` `#1E4E8B` `#2E6B2E` `#4A4A4A` | en-têtes de nœuds (application) | lire un graphe par sa forme |

📌 Les jetons de broches et de nœuds sont **consommés par l'application** (le
graphe est un `Host`) ; ils sont dans le thème pour que **les trois éditeurs de
graphes** (Blueprint, Matériau, Arbre de comportement) **et** le compositing de
NKScena parlent la même langue.

---

## 3. Les composants

### 3.1 Communs — `Commun/composants.nkgui`

Tous ceux de NkAnima 09 §3, inchangés. **Plus**, parce qu'ils servent aussi les
autres applications :

#### `LigneAsset`

```
component "LigneAsset"
  Grid "ligne"              columns: 4   sizes: [0.40, 0.50, 0.05, 0.05]   gap: 4
    Text "etiquette"        text: "?"
    AssetField✚ "valeur"    bind: ?   type: ?
    Button "anim.reinitialiser_propriete"   icon: undo-2   tooltip: "Revenir à la valeur par défaut"   visible: false
    KeyDiamond✚ "cle"       bind: ?   visible: false
```
**Aujourd'hui** : `valeur` = `ChampAsset` (ci-dessous).
📌 `cle.visible: false` **par défaut** : un asset se choisit, il ne s'anime pas
(sauf une exception que NkAnima ou NKScena rendraient visible).

#### `ChampAsset` (en attendant P16)

```
component "ChampAsset"
  HBox "champ"              gap: 2   align: Center
    Image "vignette"        source: "?"   size: (32, 32)
    Text "nom"              text: "(aucun)"
    Button "contenu.parcourir"         icon: search         tooltip: "Parcourir"
    Button "contenu.utiliser_selection" icon: arrow-left    tooltip: "Utiliser la sélection du tiroir de contenu"
    Button "contenu.effacer_reference" icon: x              tooltip: "Effacer"
```

#### `LigneTouche`

```
component "LigneTouche"
  Grid "ligne"              columns: 3   sizes: [0.40, 0.50, 0.10]   gap: 4
    Text "etiquette"        text: "?"
    KeyCaptureField✚ "valeur"   bind: ?
    Button "reglages.retirer_touche"   icon: x   tooltip: "Retirer cette touche"
```

### 3.2 Propres à Nogee — `Nogee/composants_nogee.nkgui`

#### `BoutonJouer`

```
component "BoutonJouer"
  SplitButton✚ "bouton"                label: "Jouer"   icon: play
    appearance   radius: 2   fill: #3FB950 // @jeu.en_cours   text: #0A0A0A
    MenuItem "jeu.dans_vue"            label: "Dans la vue"             shortcut: "Alt+P"   checked: true
    MenuItem "jeu.nouvelle_fenetre"    label: "Nouvelle fenêtre"
    Menu "jeu.appareil_menu"           label: "Sur l'appareil"          (rempli par l'application)
    MenuItem "jeu.simuler"             label: "Simuler"                 shortcut: "Alt+S"
    Separator "sep"
    Dropdown "jeu.nb_joueurs"          bind: jeu.nb_joueurs   items: ["1 joueur","2 joueurs","3 joueurs","4 joueurs"]
    Dropdown "jeu.mode_reseau"         bind: jeu.mode_reseau  items: ["Autonome","Écouter comme serveur","Client"]
```
**Pourquoi un composant pour une seule instance** : la barre d'outils de
l'éditeur de Blueprint porte le même bouton (on y joue pour déboguer).
⚠️ Un `Dropdown` dans le menu d'un `SplitButton` : si le monteur n'admet que des
`MenuItem`, écris-les en `Menu` à sous-entrées cochables et **signale-le**.
**Aujourd'hui** : `Button "jeu.jouer"` + `Button "jeu.options"` (▾).

#### `PastilleCompile`

```
component "PastilleCompile"
  Badge✚ "pastille"   text: "●"   tone: compile.ok   tooltip: "À jour"
```
Instances : `● / compile.ok / « À jour »` · `● / compile.a_faire / « Modifié —
à compiler »` · `✕ / compile.erreur / « Erreurs — voir les résultats »`.

#### `PastillePlateforme`

```
component "PastillePlateforme"
  Badge✚ "pastille"   text: "✓"   tone: plateforme.prete   tooltip: "Prête"
```
Instances : `✓ prete` · `! incomplete` · `— absente`.

#### `BarreGraphe` — la barre commune des trois éditeurs de graphes

```
component "BarreGraphe"
  HBox "barre"                         gap: 4   align: Center
    Button "compiler"                  label: "Compiler"   icon: hammer   tooltip: "Compiler (F7)"
    PastilleCompile "etat"
    Button "fichier.enregistrer_asset" icon: save   tooltip: "Enregistrer (Ctrl+S)"
    Button "contenu.trouver"           icon: folder-search   tooltip: "Trouver dans le tiroir de contenu (Ctrl+B)"
    Separator "sep"                    orientation: Vertical
    Button "graphe.cadrer"             icon: scan   tooltip: "Cadrer le graphe (F)"
    Button "graphe.rechercher"          icon: search   tooltip: "Rechercher (Ctrl+F)"
    Spacer "pousse"
```
Les instances **surchargent l'identifiant** du bouton `compiler` :
`bp.compiler`, `mat.appliquer`, `comportement.valider` (voir §5.9–5.11).
⚠️ La surcharge d'un **enfant** (et non de la racine) n'est peut-être pas admise
par le développement des composants : si elle ne l'est pas, c'est **P9 (slot)**
qui manque — signale-le. En attendant, écris trois barres.

---

## 4. Les fichiers

### 4.1 Communs — `Resources/Interface/Commun/`

| fichier | état |
|---|---|
| `composants.nkgui` | + `LigneAsset`, `ChampAsset`, `LigneTouche` |
| `vue_3d.nkgui` | + `Host "vue.barre_extra"` |
| `scene.nkgui` | `Host "scene.filtres"` (déjà prévu par NKScena) |
| `bibliotheque.nkgui` | + `Host "biblio.filtres"` à la place de la `ListBox` de catégories |
| `details.nkgui` | + `Host "details.entete_extra"` |
| `journal.nkgui` | **nouveau** (§5.5) |
| `monde_terrain.nkgui`, `monde_vegetation.nkgui`, `monde_eau.nkgui`, `monde_ciel.nkgui` | **nouveaux** (§5.8) — NKScena y déplace ses `details_dispersion` et `details_ciel` |

### 4.2 Propres — `Resources/Interface/Nogee/`

| fichier | § | remplace |
|---|---|---|
| `composants_nogee.nkgui` | 3.2 | — |
| `lanceur.nkgui` | 5.1 | — |
| `menus.nkgui` | 5.2 | — |
| `barre_outils.nkgui` | 5.3 | — |
| `barre_etat.nkgui` | 5.4 | **l'existant** |
| `vue_extra.nkgui` | 5.6 | — |
| `scene_filtres.nkgui` | 5.6 | **`panneau_scene.nkgui`** (retiré) |
| `bibliotheque_filtres.nkgui` | 5.6 | — |
| `details_composants.nkgui` | 5.7 | — |
| `outils_mode.nkgui` | 5.8 | — |
| `palette.nkgui` | 5.8 | — |
| `bp_editeur.nkgui` | 5.9 | — |
| `mat_editeur.nkgui` | 5.10 | — |
| `comportement_editeur.nkgui` | 5.11 | — |
| `maillage_apercu.nkgui`, `anim_lecture.nkgui`, `son_compose.nkgui`, `interface_apercu.nkgui`, `table_donnees.nkgui`, `physique_profils.nkgui` | 5.12 | — |
| `reglages_projet.nkgui` | 5.13 | — |
| `plateformes.nkgui` | 5.14 | — |
| `dispositions.nkgui` | 5.15 | — |

---

## 5. Les arbres

### 5.1 `lanceur.nkgui`

```
Window "lanceur"                              title: "Nogee"
  Splitter "lanceur.partage"                  orientation: Vertical   ratio: 0.18
    VBox "lanceur.nav"                        gap: 2
      Image "lanceur.logo"                    source: "logo_rihen"
      ListBox "lanceur.section"               bind: lanceur.section   items: ["Récents","Nouveau projet","Ouvrir","Apprendre"]
    VBox "lanceur.contenu"                    gap: 8
      Scroll "lanceur.recents_defil"          visible: true
        Flow "lanceur.recents"                gap: 8     (Tile par projet : vignette, nom, dernière ouverture, plateformes)
      Splitter "lanceur.nouveau"              orientation: Vertical   ratio: 0.62   visible: false
        Scroll "lanceur.modeles_defil"
          Flow "lanceur.modeles"              gap: 8
            Tile✚ "lanceur.modele_vide"            image: "modeles/vide"            label: "Vide"                caption: "Un niveau, une lumière, rien d'autre"
            Tile✚ "lanceur.modele_troisieme"       image: "modeles/troisieme"       label: "Troisième personne"  caption: "Un personnage, une caméra à bras"
            Tile✚ "lanceur.modele_premiere"        image: "modeles/premiere"        label: "Première personne"   caption: "Des mains, une arme, une visée"
            Tile✚ "lanceur.modele_dessus"          image: "modeles/dessus"          label: "Vue du dessus"       caption: "Cliquer pour se déplacer"
            Tile✚ "lanceur.modele_plateforme"      image: "modeles/plateforme"      label: "Plateforme 3D"       caption: "Sauter, grimper, collecter"
            Tile✚ "lanceur.modele_course"          image: "modeles/course"          label: "Course"              caption: "Un véhicule, une piste"
            Tile✚ "lanceur.modele_plateau"         image: "modeles/plateau"         label: "Jeu de plateau"      caption: "Tours, pions, règles (SONGO, NKORA)"
            Tile✚ "lanceur.modele_rv"              image: "modeles/rv"              label: "Réalité virtuelle"   caption: "Casque, mains, téléportation"
        VBox "lanceur.formulaire"             gap: 6
          TextField "lanceur.nom"             bind: lanceur.nom   placeholder: "Nom du projet"
          HBox "lanceur.dossier_ligne"        gap: 4
            TextField "lanceur.dossier"       bind: lanceur.dossier
            Button "lanceur.choisir_dossier"  icon: folder-open
          Text "lanceur.plateformes_titre"    text: "Plateformes visées"
          Flow "lanceur.plateformes"          gap: 4
            Checkbox "lanceur.pf_windows"     label: "Windows"   bind: lanceur.pf.windows
            Checkbox "lanceur.pf_linux"       label: "Linux"     bind: lanceur.pf.linux
            Checkbox "lanceur.pf_android"     label: "Android"   bind: lanceur.pf.android
            Checkbox "lanceur.pf_web"         label: "Web"       bind: lanceur.pf.web
            Checkbox "lanceur.pf_macos"       label: "macOS"     bind: lanceur.pf.macos
            Checkbox "lanceur.pf_ios"         label: "iOS"       bind: lanceur.pf.ios
          RadioGroup "lanceur.qualite"        bind: lanceur.qualite   options: ["Maximale","Mobile"]   orientation: Horizontal
          Checkbox "lanceur.contenu_depart"   label: "Contenu de départ"   bind: lanceur.contenu_depart
          Button "lanceur.creer"              label: "Créer le projet"   icon: rocket
```
`lanceur.recents_defil.visible = (lanceur.section == "Récents")`,
`lanceur.nouveau.visible = (lanceur.section == "Nouveau projet")` — idempotents ✅.
Un clic sur un modèle **le sélectionne** (état `Selected` de la `Tile`, liseré
`@accent`) ; `lanceur.creer.enabled = (lanceur.modele != "" and lanceur.nom != "")`
— idempotent ✅.

### 5.2 `menus.nkgui`

```
MenuBar "menus"
  Menu "menu_fichier"                         label: "Fichier"
    MenuItem "fichier.nouveau_niveau"         label: "Nouveau niveau…"          shortcut: "Ctrl+N"
    MenuItem "fichier.ouvrir_niveau"          label: "Ouvrir un niveau…"        shortcut: "Ctrl+O"
    Menu "menu_niveaux_recents"               label: "Niveaux récents"          (rempli par l'application)
    Separator "sep_f1"
    MenuItem "fichier.enregistrer_niveau"     label: "Enregistrer le niveau"    shortcut: "Ctrl+S"
    MenuItem "fichier.enregistrer_sous"       label: "Enregistrer sous…"        shortcut: "Ctrl+Alt+S"
    MenuItem "fichier.enregistrer_tout"       label: "Enregistrer tout"         shortcut: "Ctrl+Maj+S"
    Separator "sep_f2"
    MenuItem "fichier.importer_niveau"        label: "Importer dans le niveau…"
    MenuItem "fichier.exporter_selection"     label: "Exporter la sélection…"
    Separator "sep_f3"
    MenuItem "fichier.nouveau_projet"         label: "Nouveau projet…"
    MenuItem "fichier.ouvrir_projet"          label: "Ouvrir un projet…"
    Menu "menu_projets_recents"               label: "Projets récents"
    Separator "sep_f4"
    MenuItem "fichier.quitter"                label: "Quitter"                  shortcut: "Ctrl+Q"

  Menu "menu_edition"                         label: "Édition"
    MenuItem "anim.annuler"                   label: "Annuler"                  shortcut: "Ctrl+Z"
    MenuItem "anim.refaire"                   label: "Refaire"                  shortcut: "Ctrl+Y"
    MenuItem "edition.historique"             label: "Historique"
    Separator "sep_e1"
    MenuItem "edition.couper"                 label: "Couper"                   shortcut: "Ctrl+X"
    MenuItem "anim.copier"                    label: "Copier"                   shortcut: "Ctrl+C"
    MenuItem "anim.coller"                    label: "Coller"                   shortcut: "Ctrl+V"
    MenuItem "edition.dupliquer"              label: "Dupliquer"                shortcut: "Ctrl+D"
    MenuItem "edition.supprimer"              label: "Supprimer"                shortcut: "Suppr"
    MenuItem "edition.renommer"               label: "Renommer"                 shortcut: "F2"
    Separator "sep_e2"
    MenuItem "edition.preferences"            label: "Réglages de l'éditeur…"
    MenuItem "reglages.projet"                label: "Réglages du projet…"

  Menu "menu_fenetre"                         label: "Fenêtre"
    MenuItem "fenetre.scene"                  label: "Scène"                    checked: true
    MenuItem "fenetre.details"                label: "Détails"                  checked: true
    MenuItem "biblio.ouvrir"                  label: "Tiroir de contenu"        shortcut: "Ctrl+Espace"
    MenuItem "fenetre.journal"                label: "Journal"
    MenuItem "fenetre.outils_mode"            label: "Outils du mode"
    MenuItem "fenetre.palette"                label: "Palette"
    MenuItem "fenetre.statistiques"           label: "Statistiques"
    MenuItem "fenetre.profileur"              label: "Profileur"
    MenuItem "fenetre.references"             label: "Visionneuse de références"
    MenuItem "fenetre.tailles"                label: "Carte des tailles"
    MenuItem "ia.assistant"                   label: "Assistant IA"             shortcut: "Ctrl+K"
    Separator "sep_w1"
    MenuItem "fenetre.reinitialiser"          label: "Réinitialiser la disposition"

  Menu "menu_outils"                          label: "Outils"
    MenuItem "outils.nouvelle_classe_cpp"     label: "Nouvelle classe C++…"
    MenuItem "outils.ouvrir_nkcode"           label: "Ouvrir le projet dans NKCode"
    MenuItem "outils.recharger_cpp"           label: "Recharger le C++"         shortcut: "Ctrl+Alt+F11"
    Separator "sep_o1"
    MenuItem "outils.construire_navigation"   label: "Construire la navigation"
    MenuItem "outils.construire_eclairage"    label: "Construire l'éclairage"
    Separator "sep_o2"
    MenuItem "outils.rechercher_blueprints"   label: "Rechercher dans les Blueprints…"   shortcut: "Ctrl+Maj+F"
    MenuItem "outils.audit_assets"            label: "Audit des assets"

  Menu "menu_construire"                      label: "Construire"
    MenuItem "construire.compiler"            label: "Compiler"                 shortcut: "F7"
    Menu "menu_cuire"                         label: "Cuire pour"               (rempli par l'application : plateformes)
    MenuItem "fenetre.plateformes"            label: "Plateformes…"
    Menu "menu_empaqueter"                    label: "Empaqueter pour"          (idem)
    Menu "menu_lancer"                        label: "Lancer sur"               (idem : appareils détectés)

  Menu "menu_selection"                       label: "Sélection"
    MenuItem "anim.selection_tout"            label: "Tout"                     shortcut: "Ctrl+A"
    MenuItem "anim.selection_rien"            label: "Rien"                     shortcut: "Échap"
    MenuItem "anim.selection_inverser"        label: "Inverser"                 shortcut: "Ctrl+I"
    MenuItem "selection.meme_classe"          label: "Même classe"
    MenuItem "selection.meme_materiau"        label: "Même matériau"
    MenuItem "selection.enfants"              label: "Enfants"

  Menu "menu_acteur"                          label: "Acteur"
    MenuItem "acteur.poser_sol"               label: "Poser au sol"             shortcut: "Fin"
    Menu "menu_aligner"                       label: "Aligner"
      MenuItem "acteur.aligner_min_x"         label: "Au minimum en X"
      MenuItem "acteur.aligner_centre"        label: "Au centre"
      MenuItem "acteur.repartir"              label: "Répartir régulièrement"
    MenuItem "acteur.grouper"                 label: "Grouper"                  shortcut: "Ctrl+G"
    MenuItem "acteur.degrouper"               label: "Dégrouper"                shortcut: "Maj+G"
    MenuItem "acteur.attacher"                label: "Attacher à…"
    MenuItem "acteur.detacher"                label: "Détacher"
    Separator "sep_a1"
    MenuItem "acteur.convertir_blueprint"     label: "Convertir en Blueprint…"
    MenuItem "acteur.remplacer"               label: "Remplacer par la sélection du tiroir"
    MenuItem "acteur.editer_asset"            label: "Éditer l'asset"           shortcut: "Ctrl+E"
    MenuItem "contenu.trouver"                label: "Trouver dans le tiroir"   shortcut: "Ctrl+B"
    Menu "menu_transformations"               label: "Transformations"
      MenuItem "acteur.reinitialiser_transfo" label: "Réinitialiser"
      MenuItem "acteur.miroir_x"              label: "Miroir X"
      MenuItem "acteur.miroir_y"              label: "Miroir Y"
      MenuItem "acteur.miroir_z"              label: "Miroir Z"

  Menu "menu_aide"                            label: "Aide"
    MenuItem "anim.aide_raccourcis"           label: "Raccourcis"
    MenuItem "aide.documentation"             label: "Documentation"
    MenuItem "anim.apropos"                   label: "Crédits et licences / À propos"
```
⚠️ **Écarts voulus avec NkAnima** : Tout sélectionner est **Ctrl+A** (et non A),
Tout désélectionner **Échap**, Enregistrer sous **Ctrl+Alt+S** — ce sont les
raccourcis d'UE5, et Nogee est l'éditeur « style UE5 ». Le doc 05 §7 le dit.
Si tu veux l'uniformité entre les quatre éditeurs, **signale-le à Rodolf, ne
tranche pas.**

### 5.3 `barre_outils.nkgui`

```
HBox "barre_outils"                           gap: 4   align: Center
  Button "fichier.enregistrer_tout"           icon: save   tooltip: "Enregistrer tout (Ctrl+Maj+S)"
  Separator "sep1"                            orientation: Vertical
  Dropdown "mode.choix"                       bind: mode.actif   items: ["Sélection","Monde","Peinture","Volumes"]
  Separator "sep2"                            orientation: Vertical
  Button "ajouter.menu"                       label: "Ajouter ▾"   icon: plus-square
  Button "bp.menu"                            label: "Blueprints ▾"   icon: workflow
  Button "cine.menu"                          label: "Cinématiques ▾"   icon: clapperboard
  Spacer "pousse_gauche"

  HBox "jeu"                                  gap: 2   align: Center
    BoutonJouer "jeu.jouer"
    Button "jeu.pause"                        icon: pause    tooltip: "Pause"                 visible: false
    Button "jeu.arreter"                      icon: square   tooltip: "Arrêter (Échap)"       visible: false
    Button "jeu.ejecter"                      icon: log-out  tooltip: "Éjecter (F8)"          visible: false
    Button "jeu.image_suivante"               icon: skip-forward   tooltip: "Image suivante"  visible: false
  Spacer "pousse_droite"

  Button "plateforme.menu"                    label: "Plateformes ▾"   icon: monitor-smartphone
  Button "reglages.menu"                      label: "Réglages ▾"      icon: settings
```
- `jeu.arreter` : `appearance fill #F85149 // @jeu.arret`.
- **Comportements idempotents** ✅ :
  ```
  jeu.pause.visible            = jeu.en_cours
  jeu.arreter.visible          = jeu.en_cours
  jeu.ejecter.visible          = jeu.en_cours and not jeu.simulation
  jeu.image_suivante.visible   = jeu.en_pause
  jeu.jouer.visible            = not jeu.en_cours
  ```
- Les quatre boutons « … ▾ » ouvrent des menus : ce sont des **`SplitButton` sans
  action principale** — en attendant un `ContextMenu` monté, l'application ouvre
  le menu correspondant de la barre de menus. Contenu :
  - `ajouter.menu` : `ajouter.cube` · `ajouter.sphere` · `ajouter.plan` ·
    `ajouter.lumiere_directionnelle` · `ajouter.lumiere_ponctuelle` ·
    `ajouter.lumiere_spot` · `ajouter.camera` · `ajouter.point_apparition` ·
    `ajouter.declencheur` · `ajouter.volume` · `ajouter.son` · `ajouter.effet` ·
    `fenetre.palette` (« Palette complète… ») ;
  - `bp.menu` : `bp.nouvelle_classe` · `bp.niveau` · (classes récentes) ;
  - `cine.menu` : `cine.ajouter` · `cine.ouvrir_nkscena` ;
  - `plateforme.menu` : une ligne par plateforme avec `PastillePlateforme`,
    puis `fenetre.plateformes` · `plateforme.empaqueter` · `plateforme.lancer` ;
  - `reglages.menu` : `reglages.projet` · `reglages.monde` · `edition.preferences`.

### 5.4 `barre_etat.nkgui`

```
HBox "barre_etat"                             gap: 8   align: Center
  Button "biblio.ouvrir"                      icon: library   label: "Tiroir de contenu"
  Button "fenetre.journal"                    icon: scroll-text   label: "Journal"
  TextField "commande"                        placeholder: "> commande"   bind: commande.saisie
  Spacer "pousse"
  Text "etat.jeu"                             text: ""        (« EN JEU » / « PAUSE » / « SIMULATION »)
  Host "scene.entites"                        hint: "nombre d'entités (rempli par Nogee)"
  HBox "etat.compile"                         gap: 2
    PastilleCompile "etat.compile_pastille"
    Text "etat.compile_texte"                 text: "Compilé"
  Text "etat.branche"                         text: "⎇ —"
  HBox "etat.plateforme"                      gap: 2
    Text "etat.plateforme_nom"                text: "Windows"
    PastillePlateforme "etat.plateforme_pastille"
```
**Couleur** de `etat.jeu.text` : `@jeu.en_cours` / `@jeu.pause` /
`@jeu.simulation` selon l'état — idempotent ✅.
📌 `Host "scene.entites"` **est repris** de l'ancienne barre d'état : Nogee le
sert déjà.

### 5.5 `Commun/journal.nkgui`

```
Panel "journal"                               title: "Journal"
  VBox "colonne"                              gap: 2
    HBox "journal.barre"                      gap: 4
      TextField "journal.recherche"           placeholder: "Rechercher…"   icon✚: search   bind: journal.filtre_texte
      Dropdown "journal.categorie"            bind: journal.categorie   (items : catégories vues)
      ToggleButton✚ "journal.messages"        label: "Messages"         bind: journal.voir_messages
      ToggleButton✚ "journal.avertissements"  label: "Avertissements"   bind: journal.voir_avertissements
      ToggleButton✚ "journal.erreurs"         label: "Erreurs"          bind: journal.voir_erreurs
      Spacer "pousse"
      Button "journal.copier"                 icon: copy    tooltip: "Copier"
      Button "journal.effacer"                icon: trash-2 tooltip: "Effacer"
    Host "journal.lignes"                     hint: "lignes du journal : heure, catégorie, sévérité, texte ; une erreur de Blueprint ou de C++ est cliquable"
```
Le **nombre** d'avertissements et d'erreurs figure dans le libellé des bascules
(« Erreurs (3) ») : comportement idempotent ✅. Couleur du texte des bascules
actives et non vides : `@info.alerte` / `@info.erreur`.

### 5.6 Extensions des panneaux communs

#### `vue_extra.nkgui` (dans `vue.barre_extra`)

```
HBox "vue_extra"                              gap: 2   align: Center
  Dropdown "vue.mode_debug"                   bind: vue.mode_debug
                                              items: ["—","Collisions","Navigation","Complexité de la lumière","Complexité des shaders"]
  Separator "sep1"                            orientation: Vertical
  ToggleButton✚ "vue.aimant_grille"           icon: grid-3x3   bind: vue.aimant.grille   tooltip: "Aimantation à la grille"
  Dropdown "vue.pas_grille"                   bind: vue.pas.grille   items: ["1","5","10","50","100"]
  ToggleButton✚ "vue.aimant_angle"            icon: rotate-cw  bind: vue.aimant.angle    tooltip: "Aimantation d'angle"
  Dropdown "vue.pas_angle"                    bind: vue.pas.angle    items: ["5°","10°","15°","45°","90°"]
  ToggleButton✚ "vue.aimant_echelle"          icon: maximize-2 bind: vue.aimant.echelle  tooltip: "Aimantation d'échelle"
  Dropdown "vue.pas_echelle"                  bind: vue.pas.echelle  items: ["0,1","0,25","0,5","1"]
  ToggleButton✚ "vue.jeu"                     icon: gamepad-2  bind: vue.jeu   tooltip: "Vue de jeu (G)"
```
**Liseré de jeu** : dessiné par l'application autour de la vue avec
`@jeu.en_cours` / `@jeu.pause` / `@jeu.simulation` — **pas** dans ce document
(ce n'est pas un widget).

#### `scene_filtres.nkgui`

```
HBox "scene.filtres"                          gap: 2
  ToggleButton✚ "scene.filtre_geometrie"      icon: box              bind: scene.filtre.geometrie   tooltip: "Géométrie"
  ToggleButton✚ "scene.filtre_lumiere"        icon: lightbulb        bind: scene.filtre.lumiere     tooltip: "Lumières"
  ToggleButton✚ "scene.filtre_camera"         icon: video            bind: scene.filtre.camera      tooltip: "Caméras"
  ToggleButton✚ "scene.filtre_volume"         icon: square-dashed    bind: scene.filtre.volume      tooltip: "Volumes"
  ToggleButton✚ "scene.filtre_blueprint"      icon: workflow         bind: scene.filtre.blueprint   tooltip: "Blueprints"
  ToggleButton✚ "scene.filtre_son"            icon: volume-2         bind: scene.filtre.son         tooltip: "Sons"
  Button "scene.nouveau_dossier"              icon: folder-plus      tooltip: "Nouveau dossier"
```

#### `bibliotheque_filtres.nkgui`

```
VBox "biblio.filtres"                         gap: 2
  HBox "biblio.actions"                       gap: 4
    Button "contenu.ajouter"                  label: "+ Ajouter"    icon: plus
    Button "contenu.importer"                 label: "Importer"     icon: download
    Button "fichier.enregistrer_tout"         label: "Enregistrer tout"   icon: save
  Host "biblio.chemin"                        hint: "chemin du dossier courant, cliquable"
  Flow "biblio.types"                         gap: 2
    ToggleButton✚ "contenu.filtre_maillage"   label: "Maillages"    bind: contenu.filtre.maillage
    ToggleButton✚ "contenu.filtre_materiau"   label: "Matériaux"    bind: contenu.filtre.materiau
    ToggleButton✚ "contenu.filtre_texture"    label: "Textures"     bind: contenu.filtre.texture
    ToggleButton✚ "contenu.filtre_blueprint"  label: "Blueprints"   bind: contenu.filtre.blueprint
    ToggleButton✚ "contenu.filtre_niveau"     label: "Niveaux"      bind: contenu.filtre.niveau
    ToggleButton✚ "contenu.filtre_son"        label: "Sons"         bind: contenu.filtre.son
    ToggleButton✚ "contenu.filtre_animation"  label: "Animations"   bind: contenu.filtre.animation
    ToggleButton✚ "contenu.filtre_interface"  label: "Interfaces"   bind: contenu.filtre.interface
    ToggleButton✚ "contenu.filtre_table"      label: "Tables"       bind: contenu.filtre.table
    ToggleButton✚ "contenu.filtre_effet"      label: "Effets"       bind: contenu.filtre.effet
  RadioGroup "contenu.affichage"              bind: contenu.affichage   options: ["Vignettes","Liste"]   orientation: Horizontal
```
Le **menu contextuel** d'un asset (renommer, dupliquer, supprimer, références,
tailles, « Ouvrir dans … ») :
```
ContextMenu "contenu.menu"                    (non monté : attendu)
  MenuItem "edition.renommer"                 label: "Renommer"   shortcut: "F2"
  MenuItem "edition.dupliquer"                label: "Dupliquer"
  MenuItem "contenu.supprimer"                label: "Supprimer…"   (montre les références avant de confirmer)
  Separator "s1"
  MenuItem "fenetre.references"               label: "Visionneuse de références"
  MenuItem "fenetre.tailles"                  label: "Carte des tailles"
  Separator "s2"
  MenuItem "contenu.ouvrir_nk3dmodeler"       label: "Ouvrir dans NK3DModeler"
  MenuItem "contenu.ouvrir_nkanima"           label: "Ouvrir dans NkAnima"
  MenuItem "contenu.ouvrir_nkscena"           label: "Ouvrir dans NKScena"
  MenuItem "contenu.ouvrir_nkuidesign"        label: "Ouvrir dans NKUIDesign"
  MenuItem "contenu.ouvrir_nkcode"            label: "Ouvrir dans NKCode"
```
Visibilité de chaque « Ouvrir dans » selon le type de l'asset — idempotent ✅.

### 5.7 `details_composants.nkgui` (dans `details.entete_extra`)

```
VBox "details.composants"                     gap: 2
  HBox "dc.barre"                             gap: 4
    Button "acteur.ajouter_composant"         label: "+ Ajouter"   icon: plus
    TextField "dc.recherche_composant"        placeholder: "Composant…"   bind: dc.recherche   visible: false
    Spacer "pousse"
    Button "acteur.editer_blueprint"          label: "Modifier le Blueprint ▾"   icon: workflow
  Host "dc.arbre"                             hint: "arbre des composants de l'acteur : racine, enfants, icône par type ; sélection → catégories en dessous"
```
`dc.recherche_composant.visible = dc.ajout_ouvert` — idempotent ✅.

**Le reste du panneau** (les catégories et leurs lignes) est **engendré par
l'application** depuis la réflexion (§0.2), dans `details.contenu`. Pour les
**catégories propres à l'éditeur** (pas des propriétés de composant), écris :

```
// details_acteur_editeur.nkgui — monté APRÈS les catégories engendrées
VBox "racine"
  Categorie "cat_acteur"                      label: "Acteur"   expanded: false
    LigneCase "acteur.cache_en_jeu"           etiquette: "Caché en jeu"          bind: acteur.cache_en_jeu
    LigneCase "acteur.editeur_seulement"      etiquette: "Éditeur seulement"     bind: acteur.editeur_seulement
    LigneListe "acteur.couche"                etiquette: "Couche"                bind: acteur.couche
    Host "acteur.tags"                        hint: "étiquettes de l'acteur"
  Categorie "cat_jeu"                         label: "Pendant le jeu"   visible: false
    Button "jeu.conserver_valeurs"            label: "Conserver les valeurs de simulation"   icon: pin
```
`cat_jeu.visible = jeu.en_cours` — idempotent ✅. Toutes ces lignes :
`cle.visible = false`.

### 5.8 `outils_mode.nkgui` et `palette.nkgui`

#### `outils_mode.nkgui`

```
Panel "outils_mode"                           title: "Outils du mode"
  VBox "colonne"                              gap: 4
    TabBar "monde.onglets"                    bind: monde.outil   tabs: ["Terrain","Végétation","Eau","Ciel"]   visible: false
    Host "outils_mode.contenu"                hint: "le document du mode et de l'onglet : Commun/monde_terrain, monde_vegetation, monde_eau, monde_ciel, ou Nogee/peinture, volumes"
```
`monde.onglets.visible = (mode.actif == "Monde")` — idempotent ✅.
`outils_mode` **n'est affiché que si** `mode.actif != "Sélection"` — c'est la
disposition (§5.15) qui le montre ou le cache, pas un `visible`.

#### `Commun/monde_terrain.nkgui`

```
VBox "racine"
  RadioGroup "monde.terrain_outil"            bind: monde.terrain.outil
                                              options: ["Monter","Creuser","Lisser","Aplanir","Bruit","Peindre"]   orientation: Horizontal
  LigneNombre "monde.pinceau_rayon"           etiquette: "Rayon"    bind: monde.pinceau.rayon
  LigneNombre "monde.pinceau_force"           etiquette: "Force"    bind: monde.pinceau.force   min: 0   max: 1
  LigneNombre "monde.pinceau_attenuation"     etiquette: "Atténuation"   bind: monde.pinceau.attenuation   min: 0   max: 1
  LigneAsset "monde.matiere_peinte"           etiquette: "Matière"  bind: monde.terrain.matiere   type: "Matériau"
  Categorie "cat_terrain_gestion"             label: "Gestion"   expanded: false
    Button "monde.terrain_nouveau"            label: "Nouveau terrain…"   icon: mountain
    Button "monde.terrain_importer"           label: "Importer une carte de hauteurs…"   icon: download
    LigneNombre "monde.terrain_taille"        etiquette: "Taille (m)"   bind: monde.terrain.taille
    LigneNombre "monde.terrain_morceaux"      etiquette: "Morceaux"     bind: monde.terrain.morceaux
```
`monde_vegetation`, `monde_eau`, `monde_ciel` suivent le même patron (pinceau et
palette pour la végétation — c'est le contenu de `details_dispersion` de NKScena ;
rivière/lac/océan pour l'eau ; lieu, heure, nuages, brouillard pour le ciel —
le contenu de `details_ciel` de NKScena). Toutes les lignes : `cle.visible = false`
**dans Nogee** ; dans NKScena, l'heure du ciel **s'anime** → l'application de
NKScena rend `cle` visible sur cette ligne.

#### `palette.nkgui`

```
Panel "palette"                               title: "Palette"
  VBox "colonne"                              gap: 4
    TextField "palette.recherche"             placeholder: "Rechercher une classe…"   icon✚: search   bind: palette.recherche
    Splitter "palette.partage"                orientation: Vertical   ratio: 0.35
      ListBox "palette.categorie"             bind: palette.categorie
                                              items: ["Récents","Formes","Lumières","Caméras","Volumes","Gameplay","Sons","Effets","Tous les acteurs"]
      Scroll "palette.defil"
        Flow "palette.grille"                 gap: 4     (Tile par classe : icône, nom ; glisser vers la vue)
```

### 5.9 `bp_editeur.nkgui` — l'éditeur de Blueprint

```
Window "bp_editeur"                           title: "Blueprint"
  VBox "racine"                               gap: 0
    HBox "bp.barre"                           gap: 4   align: Center
      Button "bp.compiler"                    label: "Compiler"   icon: hammer   tooltip: "Compiler (F7)"
      PastilleCompile "bp.etat"
      Button "fichier.enregistrer_asset"      icon: save   tooltip: "Enregistrer (Ctrl+S)"
      Button "contenu.trouver"                icon: folder-search   tooltip: "Trouver dans le tiroir (Ctrl+B)"
      Separator "sep1"                        orientation: Vertical
      Dropdown "bp.instance_debug"            bind: bp.instance_debug   items: ["(aucune instance)"]   tooltip: "Instance à déboguer (pendant le jeu)"
      BoutonJouer "jeu.jouer"                 (même action que la barre d'outils : jouer pour déboguer)
      Separator "sep2"                        orientation: Vertical
      Button "bp.voir_cpp"                    label: "Voir en C++"   icon: code
      Button "bp.parametres_classe"           label: "Classe"   icon: settings-2
      Button "bp.graphe_cadrer"               icon: scan    tooltip: "Cadrer le graphe (F)"
      Button "bp.rechercher"                  icon: search  tooltip: "Rechercher (Ctrl+F)"
    Splitter "bp.partage_gauche"              orientation: Vertical   ratio: 0.18
      VBox "bp.mon_blueprint"                 gap: 4
        Text "bp.mb_titre"                    text: "Mon Blueprint"
        Expander "bp.graphes"                 label: "Graphes"      expanded: true
          Host "bp.graphes_liste"             hint: "graphe d'événements, script de construction"
        HBox "bp.fonctions_tete"              gap: 2
          Text "bp.fonctions_titre"           text: "Fonctions"
          Button "bp.ajouter_fonction"        icon: plus   tooltip: "Nouvelle fonction"
        Host "bp.fonctions_liste"             hint: "fonctions de la classe"
        HBox "bp.variables_tete"              gap: 2
          Text "bp.variables_titre"           text: "Variables"
          Button "bp.ajouter_variable"        icon: plus   tooltip: "Nouvelle variable"
        Host "bp.variables_liste"             hint: "variables : pastille de type (@pin.*), nom, œil (exposée)"
        HBox "bp.repartiteurs_tete"           gap: 2
          Text "bp.repartiteurs_titre"        text: "Répartiteurs"
          Button "bp.ajouter_repartiteur"     icon: plus   tooltip: "Nouveau répartiteur d'événement"
        Host "bp.repartiteurs_liste"          hint: "répartiteurs d'événements"
      Splitter "bp.partage_droite"            orientation: Vertical   ratio: 0.78
        VBox "bp.centre"                      gap: 0
          TabBar "bp.onglets"                 bind: bp.graphe_courant   closable: true   tabs: ["Vue","Graphe d'événements","Script de construction"]
          Host "bp.graphe"                    hint: "le graphe (NKGraph) ou l'aperçu 3D des composants (onglet Vue) ; broches @pin.*, en-têtes @noeud.*, chemin d'exécution illuminé en débogage"
          Splitter "bp.bas_partage"           orientation: Horizontal   ratio: 0.8
            Spacer "bp.vide"
            TabBar "bp.bas_onglets"           bind: bp.bas   tabs: ["Résultats de compilation","Recherche"]
          Host "bp.bas_contenu"               hint: "résultats de compilation cliquables, ou résultats de recherche"
        Panel "bp.details"                    title: "Détails"
          Host "bp.details_contenu"           hint: "détails du nœud, de la variable ou de la fonction sélectionnés — engendrés comme §0.2"
```
- `bp.instance_debug.enabled = jeu.en_cours` — idempotent ✅.
- `bp.compiler` : `appearance fill` selon l'état — `@compile.a_faire` (ambre)
  quand il y a quelque chose à compiler, sinon neutre. Idempotent ✅.
- **Clic droit dans le graphe** : la palette contextuelle des nœuds est peinte
  par l'application (elle dépend de la broche tirée) — pas un document.
- **F9** sur un nœud : point d'arrêt (action `bp.point_arret`).

### 5.10 `mat_editeur.nkgui` — l'éditeur de matériau

```
Window "mat_editeur"                          title: "Matériau"
  VBox "racine"                               gap: 0
    HBox "mat.barre"                          gap: 4   align: Center
      Button "mat.appliquer"                  label: "Appliquer"   icon: check   tooltip: "Recompiler le matériau (NkSL)"
      PastilleCompile "mat.etat"
      Button "fichier.enregistrer_asset"      icon: save
      Button "contenu.trouver"                icon: folder-search
      Separator "sep1"                        orientation: Vertical
      Dropdown "mat.forme_apercu"             bind: mat.forme   items: ["Sphère","Cube","Plan","Cylindre","Maillage choisi"]
      Button "mat.graphe_cadrer"              icon: scan
      Button "mat.rechercher"                 icon: search
    Splitter "mat.partage"                    orientation: Vertical   ratio: 0.25
      VBox "mat.gauche"                       gap: 0
        Host "mat.apercu"                     hint: "aperçu 3D du matériau"
        Panel "mat.details"                   title: "Détails"
          Host "mat.details_contenu"          hint: "détails du nœud sélectionné (engendrés)"
      Splitter "mat.partage_droite"           orientation: Vertical   ratio: 0.8
        VBox "mat.centre"                     gap: 0
          Host "mat.graphe"                   hint: "graphe du matériau (NKGraph / NkMatGraph)"
          Host "mat.statistiques"             hint: "instructions, échantillonneurs utilisés / budget, avertissements"
        Panel "mat.palette"                   title: "Palette"
          VBox "mat.pal_colonne"
            TextField "mat.palette_recherche" placeholder: "Nœud…"   bind: mat.palette_recherche
            Host "mat.palette_liste"          hint: "nœuds par catégorie : constantes, textures, maths, coordonnées, paramètres"
```
**Couleur** : dans `mat.statistiques`, le compte d'échantillonneurs passe en
`@info.alerte` près du budget et `@info.erreur` au-delà (peint par l'application).

### 5.11 `comportement_editeur.nkgui` — l'arbre de comportement

```
Window "comportement_editeur"                 title: "Arbre de comportement"
  VBox "racine"                               gap: 0
    HBox "cp.barre"                           gap: 4   align: Center
      Button "comportement.valider"           label: "Valider"   icon: check-check
      PastilleCompile "cp.etat"
      Button "fichier.enregistrer_asset"      icon: save
      Button "contenu.trouver"                icon: folder-search
      Separator "sep1"                        orientation: Vertical
      Dropdown "cp.instance_debug"            bind: cp.instance_debug   items: ["(aucune instance)"]
      Button "comportement.tableau_noir"      label: "Tableau noir"   icon: clipboard-list
    Splitter "cp.partage"                     orientation: Vertical   ratio: 0.78
      Host "cp.graphe"                        hint: "arbre de haut en bas : sélecteurs, séquences, décorateurs, tâches ; chemin actif illuminé en débogage"
      VBox "cp.droite"                        gap: 0
        Panel "cp.details"                    title: "Détails"
          Host "cp.details_contenu"           hint: "détails du nœud (engendrés)"
        Panel "cp.tableau"                    title: "Tableau noir"
          Host "cp.tableau_contenu"           hint: "clés du tableau noir : nom, type, valeur en jeu"
```
`cp.instance_debug.enabled = jeu.en_cours` — idempotent ✅.

### 5.12 Les autres éditeurs d'assets (patron)

Chacun est une `Window` avec une **barre** (Enregistrer, Trouver dans le tiroir,
et ses actions propres), une **zone centrale** `Host`, et un **Détails** engendré.

| fichier | barre (actions propres) | zone centrale |
|---|---|---|
| `maillage_apercu.nkgui` | `maillage.collision_generer` · `maillage.collision_simplifier` · `maillage.lod_generer` · `maillage.ajouter_socket` · `Dropdown "maillage.lod"` | aperçu 3D, collisions, sockets |
| `anim_lecture.nkgui` | **`contenu.ouvrir_nkanima`** (bouton mis en avant, libellé « Ouvrir dans NkAnima ») · transport `lecture.*` | lecteur de clip ou de graphe d'animation |
| `son_compose.nkgui` | `son.ecouter` · `son.arreter` · `son.ajouter_noeud` | graphe du son composé |
| `interface_apercu.nkgui` | **`contenu.ouvrir_nkuidesign`** (mis en avant) · `Dropdown "interface.resolution"` | aperçu du `.nkgui` monté |
| `table_donnees.nkgui` | `table.ajouter_ligne` · `table.supprimer_ligne` · `table.dupliquer_ligne` · `LigneAsset "table.structure"` | `Table` (non monté) + `Host` de repli |
| `physique_profils.nkgui` | `phys.ajouter_canal` · `phys.ajouter_profil` · `phys.ajouter_matiere` | `Table` canaux × réponses (Ignorer / Chevaucher / Bloquer) + `Host` de repli |

📌 **Les transports `lecture.*`** sont ceux de NKScena : Nogee les sert aussi
(lecture d'un clip dans l'aperçu).

### 5.13 `reglages_projet.nkgui`

```
Window "reglages_projet"                      title: "Réglages du projet"
  Splitter "rp.partage"                       orientation: Vertical   ratio: 0.22
    VBox "rp.gauche"                          gap: 4
      TextField "rp.recherche"                placeholder: "Rechercher un réglage…"   icon✚: search   bind: rp.recherche
      ListBox "rp.categorie"                  bind: rp.categorie
                                              items: ["Description","Cartes et modes","Entrées","Physique","Collision","Rendu","Audio","Réseau","Plateformes","Langues"]
    Scroll "rp.defil"                         axis: Vertical
      Host "rp.contenu"                       hint: "catégorie choisie ; lignes engendrées par la réflexion des réglages, sauf Entrées (ci-dessous)"
```
**La catégorie Entrées** est écrite à la main (elle a une forme propre) :

```
// reglages_entrees.nkgui — monté dans rp.contenu quand rp.categorie == "Entrées"
VBox "racine"
  Categorie "cat_actions"                     label: "Actions"
    Host "entrees.actions"                    hint: "une ligne par action : nom, puis une LigneTouche par touche liée, + ajouter une touche"
    Button "reglages.ajouter_action"          label: "+ Action"
  Categorie "cat_axes"                        label: "Axes"
    Host "entrees.axes"                       hint: "une ligne par axe : nom, touches ou axe de manette avec leur échelle (+1 / −1)"
    Button "reglages.ajouter_axe"             label: "+ Axe"
```
Un **conflit de touche** (la même touche pour deux actions) : la `LigneTouche`
fautive passe son texte en `@info.alerte` — idempotent ✅.

### 5.14 `plateformes.nkgui`

```
Window "plateformes"                          title: "Plateformes"
  VBox "racine"                               gap: 6
    Host "pf.liste"                           hint: "une ligne par cible : PastillePlateforme, nom, ce qui manque (ex. « ANDROID_SDK_ROOT introuvable »), bouton Préparer"
    Separator "pf.sep1"
    HBox "pf.cible"                           gap: 6   align: Center
      Dropdown "plateforme.cible"             bind: plateforme.cible   (items : cibles)
      RadioGroup "plateforme.profil"          bind: plateforme.profil   options: ["Développement","Expédition"]   orientation: Horizontal
      Spacer "pousse"
      Button "plateforme.preparer"            label: "Préparer"   icon: wrench
      Button "plateforme.cuire"               label: "Cuire"      icon: flame
      Button "plateforme.empaqueter"          label: "Empaqueter" icon: package
      Button "plateforme.lancer"              label: "Lancer sur…"   icon: send
    Dropdown "plateforme.appareil"            bind: plateforme.appareil   (items : appareils détectés)
    Text "pf.sortie_titre"                    text: "Sortie de Jenga"
    Host "pf.sortie"                          hint: "sortie de jenga build en direct ; erreurs cliquables"
    Progress "pf.progression"                 bind: plateforme.progression   visible: false
```
- `pf.progression.visible = plateforme.en_cours` ✅ ;
- `plateforme.empaqueter.enabled = (plateforme.etat_cible == "prete")` ✅, avec la
  raison en infobulle sinon (« Il manque : … »).

### 5.15 `dispositions.nkgui`

```
layout "niveau"
  dock "vue" center · dock "scene" right-top 0.20 · dock "details" right-bottom 0.60 · dock "journal" bottom 0.20 (replié)
layout "niveau_mode"
  dock "outils_mode" left 0.18 · dock "vue" center · dock "scene" right-top 0.20 · dock "details" right-bottom 0.60
layout "blueprint"
  document "bp_editeur"
layout "materiau"
  document "mat_editeur"
layout "comportement"
  document "comportement_editeur"
```
- Le **tiroir de contenu** n'est dans aucune disposition : il monte du bas
  (`biblio.ouvrir`), ou s'amarre si l'utilisateur le demande.
- `niveau` ↔ `niveau_mode` : l'application bascule selon `mode.actif`.
- Les éditeurs d'assets sont des **documents** (onglets) : chacun garde sa
  disposition interne.

---

## 6. Les actions

Légende : ✅ servie aujourd'hui par Nogee · ✚ à créer (grisée avec sa raison).

### 6.1 Actions communes que Nogee doit servir

`anim.annuler` · `anim.refaire` · `anim.copier` · `anim.coller` ·
`anim.selection_tout` · `anim.selection_rien` · `anim.selection_inverser` ·
`anim.reinitialiser_propriete` · `anim.aide_raccourcis` · `anim.apropos` ·
`biblio.ouvrir` · `fenetre.journal` · `ia.assistant` · `ia.proposer` ·
`ia.apercu` · `ia.appliquer` · `ia.rejeter` · `lecture.*` (aperçu des clips) —
toutes ✚ **dans Nogee**.
⚠️ `anim.cle_propriete` : **non servie** dans Nogee (l'éditeur de niveau n'anime
pas). Les `KeyDiamond` des lignes engendrées y sont **masqués** ; s'il en reste un
visible, il est grisé avec la raison « Les animations se font dans NkAnima ou
NKScena ».

### 6.2 Actions propres — toutes ✚

| domaine | actions |
|---|---|
| **fichier** | `nouveau_niveau` · `ouvrir_niveau` · `enregistrer_niveau` · `enregistrer_sous` · `enregistrer_tout` · `enregistrer_asset` · `importer_niveau` · `exporter_selection` · `nouveau_projet` · `ouvrir_projet` · `quitter` |
| **edition** | `historique` · `couper` · `dupliquer` · `supprimer` · `renommer` · `preferences` |
| **fenetre** | `scene` · `details` · `outils_mode` · `palette` · `statistiques` · `profileur` · `references` · `tailles` · `plateformes` · `reinitialiser` |
| **outils** | `nouvelle_classe_cpp` · `ouvrir_nkcode` · `recharger_cpp` · `construire_navigation` · `construire_eclairage` · `rechercher_blueprints` · `audit_assets` |
| **construire** | `compiler` |
| **selection** | `meme_classe` · `meme_materiau` · `enfants` |
| **acteur** | `poser_sol` · `aligner_min_x` · `aligner_centre` · `repartir` · `grouper` · `degrouper` · `attacher` · `detacher` · `convertir_blueprint` · `remplacer` · `editer_asset` · `reinitialiser_transfo` · `miroir_x` · `miroir_y` · `miroir_z` · `ajouter_composant` · `editer_blueprint` |
| **ajouter** | `menu` · `cube` · `sphere` · `plan` · `lumiere_directionnelle` · `lumiere_ponctuelle` · `lumiere_spot` · `camera` · `point_apparition` · `declencheur` · `volume` · `son` · `effet` |
| **jeu** | `jouer` · `options` · `dans_vue` · `nouvelle_fenetre` · `simuler` · `pause` · `arreter` · `ejecter` · `image_suivante` · `conserver_valeurs` |
| **cine** | `menu` · `ajouter` · `ouvrir_nkscena` |
| **bp** | `menu` · `nouvelle_classe` · `niveau` · `compiler` · `voir_cpp` · `parametres_classe` · `graphe_cadrer` · `rechercher` · `ajouter_fonction` · `ajouter_variable` · `ajouter_repartiteur` · `point_arret` |
| **graphe** | `cadrer` · `rechercher` (barre commune des graphes, §3.2) |
| **mat** | `appliquer` · `graphe_cadrer` · `rechercher` |
| **comportement** | `valider` · `tableau_noir` |
| **maillage** | `collision_generer` · `collision_simplifier` · `lod_generer` · `ajouter_socket` |
| **son** | `ecouter` · `arreter` · `ajouter_noeud` |
| **table** | `ajouter_ligne` · `supprimer_ligne` · `dupliquer_ligne` |
| **phys** | `ajouter_canal` · `ajouter_profil` · `ajouter_matiere` |
| **plateforme** | `menu` · `preparer` · `cuire` · `empaqueter` · `lancer` |
| **reglages** | `menu` · `projet` · `monde` · `ajouter_action` · `ajouter_axe` · `retirer_touche` |
| **contenu** | `ajouter` · `importer` · `supprimer` · `trouver` · `parcourir` · `utiliser_selection` · `effacer_reference` · `ouvrir_nk3dmodeler` · `ouvrir_nkanima` · `ouvrir_nkscena` · `ouvrir_nkuidesign` · `ouvrir_nkcode` |
| **scene** | `nouveau_dossier` |
| **journal** | `copier` · `effacer` |
| **monde** | `terrain_nouveau` · `terrain_importer` |
| **lanceur** | `modele_vide` · `modele_troisieme` · `modele_premiere` · `modele_dessus` · `modele_plateforme` · `modele_course` · `modele_plateau` · `modele_rv` · `choisir_dossier` · `creer` |
| **aide** | `documentation` |

### 6.3 Raisons de grisé

| cas | raison |
|---|---|
| action ✚ non servie | « Pas encore disponible dans Nogee » |
| `jeu.*` avant P2 | « Jouer dans l'éditeur arrive au palier P2 » |
| `plateforme.empaqueter` sur une cible incomplète | « Il manque : … » (la liste) |
| `bp.*` avant P4 | « L'éditeur de Blueprint arrive au palier P4 » |
| `jeu.conserver_valeurs` hors du jeu | « Seulement pendant le jeu » |
| `bp.instance_debug` hors du jeu | « Lance le jeu pour choisir une instance » |
| `contenu.ouvrir_*` si l'application n'est pas installée | « <application> n'est pas installée » |
| `jeu.mode_reseau` ≠ Autonome avant P6 | « La réplication arrive au palier P6 » |

---

## 7. Ce que tu ne sais pas encore reproduire — écrit quand même

### 7.1 États

Ceux de NkAnima 09 §7.1 et NKScena 03 §7.1, plus :

| élément | état | rendu attendu |
|---|---|---|
| `BoutonJouer` | `Hover` / `Pressed` | vert plus clair / plus sombre |
| `BoutonJouer` | caché pendant le jeu | (c'est un `visible`, pas un état) |
| `Tile` (lanceur) | `Selected` | liseré `@accent` 2 px |
| `ToggleButton` des filtres du journal | `Active` + non vide | texte `@info.alerte` / `@info.erreur` |
| onglet de document d'asset | `Dirty` | astérisque + point `@asset.modifie` |
| `AssetField` | `DropOk` / `DropRefused` | liseré `@accent` / `@info.erreur` pendant un glisser |
| `KeyCaptureField` | `Capturing` | fond `@accent` à 30 %, texte « Appuyez sur une touche… » |

### 7.2 Animations

| élément | animation |
|---|---|
| liseré de la Vue (application) | fondu de couleur à l'entrée et à la sortie du jeu (200 ms) |
| `PastilleCompile` | pulsation une fois quand elle passe au vert |
| tiroir de contenu | monte en 150 ms, redescend en 120 ms (comme NkAnima) |
| `pf.progression` | avance douce |

### 7.3 Événements (lot b)

| élément | événement | effet | idempotent ? |
|---|---|---|---|
| `BoutonJouer` | `on Click` | jouer avec le dernier réglage | ❌ |
| `mode.choix` | `on Changed` | changer de disposition (`niveau` ↔ `niveau_mode`) | ❌ — aujourd'hui l'application compare à la valeur précédente |
| `AssetField` | `on Drop` / `on DoubleClick` | affecter / ouvrir l'asset | ❌ |
| `KeyCaptureField` | `on KeyCaptured` | lier la touche | ❌ |
| `Tile` de palette et du tiroir | `on DragStart` / `on Drop` (dans la Vue) | placer l'acteur | ❌ |
| `commande` | `on Submit` | exécuter la commande | ❌ |
| `journal.lignes` (application) | clic sur une erreur | ouvrir l'endroit fautif | ❌ |

### 7.4 Polices

Comme NkAnima et NKScena ; **`"Mono-11"`** pour le Journal, la sortie de Jenga, la
vue C++ d'un Blueprint.

---

## 8. L'ordre de livraison proposé

Aligné sur le phasage du doc 04.

| # | livrable | palier | témoin |
|---|---|---|---|
| 1 | extensions des panneaux communs (§0.3) + `Commun/journal` ; NkAnima et NKScena **repassés** sur les `Host` d'extension | — | compteurs de NkAnimaEditor **identiques** avant / après |
| 2 | `composants_nogee` + `menus` + `barre_outils` + `barre_etat` (remplace l'existant) | P1 | `hotes` : `scene.entites` toujours servi ; `actionsInconnues` = les ✚ attendues |
| 3 | `vue_extra` + `scene_filtres` + `bibliotheque_filtres` + `details_composants` + **Détails engendré par la réflexion** | P1 | une propriété ajoutée à un composant C++ apparaît dans Détails **sans toucher à un document** |
| 4 | `lanceur` | P1 | un projet créé depuis le modèle Vide s'ouvre |
| 5 | l'état de jeu dans la barre d'outils et la barre d'état | P2 | les boutons ⏸ ⏹ ⏏ visibles **seulement** en jeu, à chaque image |
| 6 | `plateformes` | P3 | la ligne Android dit exactement ce qui manque sur une machine sans SDK |
| 7 | `bp_editeur` + `mat_editeur` + `reglages_projet` (+ entrées) | P4 | |
| 8 | `outils_mode` + `palette` + `Commun/monde_*` (NKScena y déplace les siens) + `comportement_editeur` | P5 | la même `monde_terrain.nkgui` montée dans Nogee **et** dans NKScena |
| 9 | les autres éditeurs d'assets (§5.12) + `dispositions` | P5–P6 | |

📌 Comme pour les deux autres : **après l'étape 3, publie `apparencesNonPeintes`
et `etatsNonAppliques`** pour Nogee.
