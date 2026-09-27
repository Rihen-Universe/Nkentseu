# NkAntenne — l'interface, pour l'agent qui écrit les `.nkgui`

### Document 3 — Spécification d'interface côté agent

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 27/09/2026.
> Traduction du **02** en tes quatre points : **arbre nommé**, **composants**,
> **actions**, **couleurs porteuses** — plus ce que tu ne sais pas encore
> reproduire. **Si 02 et 03 se contredisent, 02 gagne.**
>
> **Prérequis** : les documents « agent » de la famille (notation, P1–P27,
> `Commun/`, actions `commun.`) — NkAnima 09, NKScena 03, Nogee 06, NKCraft 03,
> PV3DE 03, NKUIDesign 21.

---

## 0. Ce qui est propre à NkAntenne

1. **Le thème est GitHub Dark Pro / Light Pro** (doc 02 §1.2) : **mêmes noms de
   jetons** que la famille, autres valeurs. **N'écris aucune couleur de structure** ;
   les seules couleurs écrites sont les **jetons d'information** du §2.
2. **Épure** : **peu d'éléments visibles**, le reste dans des **tiroirs** (P28) et la
   **palette** (Ctrl+K). La barre de menus existe mais est **cachée** (Alt).
3. **Gestes du direct** : **grandes cibles** (`minSize: (0, 40)` au moins) pour
   Démarrer / Arrêter, Enregistrer, Marqueur, sourdines.
4. **Surfaces peintes par l'application** (`Host`) : la **vue Programme / Aperçu**
   (et ses poignées), les **VU-mètres** tant que `Meter` (P14) n'est pas monté, la
   **timeline** (brique partagée), la **lecture** du montage et de la médiathèque, la
   **forme d'onde**, les **vignettes** de fenêtres à capturer.
5. **Actions**, par domaine : `antenne.` (direct, enregistrement, marqueur, clip),
   `studio.`, `scene.`, `source.`, `mixeur.`, `dest.`, `montage.`, `comp.`,
   `media.`, `lecteur.`, `espace.`, `reglages.`, `accueil.`, `app.` + communs
   (`commun.`, `ia.`, `fenetre.`).
6. **Composants communs repris** : `LigneNombre`, `LigneListe`, `LigneCase`,
   `LigneCouleur`, `LigneAsset`, `Categorie`, `EnTetePanneau`, `CartePropo`,
   `BoutonEcrit` (Commun), et **`TrancheMixeur`** de NKScena (mixeur en vertical).

---

## 1. Le vocabulaire

**Consommés** : P2 `ToggleButton`, P3 `Badge`, P4 `Tile`, P13 `TimecodeField`, P14
`Meter`, P18 `SplitButton` (Démarrer le direct ▾), P21 `HoldButton` (micro en
appuyer-pour-parler), P25 (instructions d'interface : les tiroirs, les
confirmations).

### P28 — `Drawer` (tiroir)

- **Quoi** : un panneau qui **glisse depuis un bord** (`edge` : Left, Right, Bottom),
  par-dessus le contenu, **se referme** au clic à l'extérieur ou par Échap ;
  propriétés `edge`, `size` (fraction), `modal` (assombrit le reste), `bind`
  (ouvert / fermé).
- **Pourquoi** : l'épure de NkAntenne (doc 02 §1.1) repose sur les tiroirs ; un
  `Panel` avec `visible` n'a ni l'entrée glissée, ni la fermeture au clic extérieur,
  ni le placement contre un bord.
- **En attendant** : `Panel` en `placement: absolute` + `visible` lié ; la fermeture
  par l'application ; l'animation écrite dans `animation` (comptée).

---

## 2. Les jetons d'information

| jeton | Dark | Light | où **seulement** |
|---|---|---|---|
| `@antenne.direct` | `#F85149` | `#CF222E` | bouton « Arrêter le direct », badge EN DIRECT, liseré Programme |
| `@antenne.enregistrement` | `#F85149` | `#CF222E` | bouton « Arrêter l'enregistrement », badge ● REC (point pulsant) |
| `@antenne.pause` | `#D29922` | `#9A6700` | badge ❚❚ |
| `@antenne.pret` · `.reconnexion` · `.erreur` | `#3FB950` · `#D29922` · `#F85149` | `#1A7F37` · `#9A6700` · `#CF222E` | pastilles de destination |
| `@sante.bon` · `.moyen` · `.mauvais` | `#3FB950` · `#D29922` · `#F85149` | `#1A7F37` · `#9A6700` · `#CF222E` | barre de santé |
| `@son.ecretage` | `#F85149` | `#CF222E` | VU-mètres (commun NKScena) |
| `@son.sourdine` | `#D29922` | `#9A6700` | bouton de sourdine actif |
| `@studio.apercu` | = `@accent` | = `@accent` | liseré de l'Aperçu (mode studio) |
| `@studio.programme` | = `@antenne.direct` | = `@antenne.direct` | liseré du Programme **pendant le direct** |
| `@plateforme.youtube` · `.facebook` · `.tiktok` · `.twitch` · `.kick` · `.autre` | couleurs de marque | idem | **pastille** d'une carte de destination — jamais un fond ni un bouton |

📌 **Le rouge est réservé** : direct, enregistrement, erreur. Un contrôle qui
emploierait `@antenne.*` ailleurs est une **faute** (règle de contrôle à déclarer
dans NKUIDesign, doc 19 I).

---

## 3. Les composants — `NkAntenne/composants_antenne.nkgui`

#### `BadgeAntenne` — EN DIRECT / ● REC, avec la durée

```
component "BadgeAntenne"
  HBox "badge"                         gap: 6   align: Center   visible: false
    Badge✚ "etat"                      text: "EN DIRECT"   tone: antenne.direct
    Text "duree"                       text: "00:00:00"    font: "Mono-12"
```
Instances : `app.badge_direct` (« EN DIRECT », `antenne.direct`) ·
`app.badge_rec` (« ● REC », `antenne.enregistrement`, animation pulsante du point).
`badge.visible` lié à l'état (idempotent ✅).

#### `BoutonAntenne` — un grand geste du direct

```
component "BoutonAntenne"
  Button "bouton"                      label: "?"   icon: "?"   minSize: (0, 44)
    appearance   radius: 6
```
Instances : voir §5.4. **Deux jeux d'apparence** selon l'état (repos / actif), écrits
par les comportements (`set "id".label`, `set "id".appearance` — ou deux boutons
alternés par `visible` en attendant P25).

#### `LigneMixeur` — une source audio, en horizontal

```
component "LigneMixeur"
  Grid "ligne"                         columns: 5   sizes: [0.22, 0.46, 0.12, 0.10, 0.10]   gap: 6
    Text "nom"                         text: "?"
    VBox "niveaux"                     gap: 2
      Meter✚ "vu"                      bind: ?.niveau   peak: ?.crete   channels: 2   orientation: Horizontal
      Slider "fader"                   bind: ?.volume   min: -60   max: 12
    ToggleButton✚ "ecoute"             icon: headphones   bind: ?.ecoute     tooltip: "Écouter"
    ToggleButton✚ "sourdine"           icon: mic-off      bind: ?.sourdine   tooltip: "Couper"   minSize: (40, 40)
    Button "reglages"                  icon: settings     tooltip: "Filtres, pistes, décalage"
```
- `sourdine` actif → fond `@son.sourdine` + icône barrée.
- **Aujourd'hui** : `Meter` → `Progress` (+ `Badge` d'écrêtage).
- ⚠️ Actions portées par des **enfants** (`sourdine`, `reglages`) : la question de
  format ouverte par NKCraft 03 (l'identifiant d'une action portée par un enfant de
  composant est-il préfixé ?) **se pose ici aussi**.

#### `CarteDestination`

```
component "CarteDestination"
  Panel "carte"                        appearance: radius 6
    VBox "corps"                       gap: 6
      HBox "tete"                      gap: 6   align: Center
        Badge✚ "plateforme"            text: "?"   tone: plateforme.autre
        Text "nom"                     text: "?"
        Spacer "pousse"
        Badge✚ "etat"                  text: "●"   tone: antenne.pret
        Switch "active"                bind: ?.active   label: "Active"      (non monté : attendu)
      LigneListe "serveur"             etiquette: "Serveur"   bind: ?.serveur
      HBox "cle_ligne"                 gap: 4
        Text "cle_titre"               text: "Clé"
        TextField "cle"                bind: ?.cle   secret: true
        ToggleButton✚ "cle_voir"       icon: eye   bind: ?.cle_visible   tooltip: "Montrer la clé"
      HBox "boutons"                   gap: 4   justify: End
        Button "dest.tester"           label: "Tester"      icon: plug-zap
        Button "dest.supprimer"        label: "Supprimer"   icon: trash-2
```
- **`secret: true` est obligatoire** sur `cle` ; `cle_voir` bascule `secret` (comportement
  continu ✅) ; **la clé n'est jamais** dans un `tooltip`, un `text`, un journal.
- `etat` : `●` + `antenne.pret` / `antenne.reconnexion` / `antenne.erreur` ; en
  erreur, `tooltip` = le message de la plateforme.

#### `TuileSource` — dans le choix de source

```
component "TuileSource"
  Tile✚ "tuile"                        image: "?"   label: "?"   caption: "?"   size: (132, 104)
```

#### `LigneSante` — un indicateur de la barre de santé

```
component "LigneSante"
  HBox "ind"                           gap: 4   align: Center
    Text "valeur"                      text: "—"   font: "Mono-11"
    Badge✚ "pastille"                  text: "●"   tone: sante.bon
```

---

## 4. Les fichiers — `Resources/Interface/NkAntenne/`

| fichier | § |
|---|---|
| `composants_antenne.nkgui` | 3 |
| `application.nkgui` | 5.1 — la **carte** de l'application (P26) : espaces, dialogues, départ |
| `barre_haut.nkgui` | 5.2 |
| `menus.nkgui` | 5.3 (barre cachée, Alt) |
| `studio.nkgui` + `studio_commandes`, `studio_scenes`, `studio_sources`, `studio_mixeur`, `barre_sante` | 5.4 |
| `tiroir_destinations`, `tiroir_filtres`, `tiroir_transitions`, `tiroir_stats` | 5.5 |
| `dlg_choix_source.nkgui`, `dlg_confirmer_arret.nkgui` | 5.6 |
| `montage.nkgui` + `montage_silences` | 5.7 |
| `compression.nkgui` | 5.8 |
| `mediatheque.nkgui` + `lecteur_video`, `lecteur_audio` | 5.9 |
| `reglages.nkgui` | 5.10 |
| `assistant_demarrage.nkgui` | 5.11 |
| `NkAntenne.tests.nkgui` | 5.12 — les **tests d'interaction** (P27) |

---

## 5. Les arbres

### 5.1 `application.nkgui` — la carte (P26)

```
application "NkAntenne" {
  plateforme = Bureau
  depart     = "studio"
  ecran "assistant"          { document = "assistant_demarrage.nkgui"   type = Dialogue }
  ecran "studio"             { document = "studio.nkgui"                type = Fenetre }
  ecran "montage"            { document = "montage.nkgui"               type = Fenetre }
  ecran "compression"        { document = "compression.nkgui"           type = Fenetre }
  ecran "mediatheque"        { document = "mediatheque.nkgui"           type = Fenetre }
  ecran "lecteur_video"      { document = "lecteur_video.nkgui"         type = Fenetre }
  ecran "lecteur_audio"      { document = "lecteur_audio.nkgui"         type = Fenetre }
  ecran "choix_source"       { document = "dlg_choix_source.nkgui"      type = Dialogue }
  ecran "confirmer_arret"    { document = "dlg_confirmer_arret.nkgui"   type = Dialogue }
  ecran "reglages"           { document = "reglages.nkgui"              type = Dialogue }
}
```
📌 Les **quatre espaces** sont des écrans de **même fenêtre** : le sélecteur (§5.2)
fait `open "montage"` **en remplaçant** l'écran courant. Si P26 ne sait pas dire
« remplacer dans la même fenêtre », dis-le : c'est le cas de la plupart des
applications à onglets principaux.

### 5.2 `barre_haut.nkgui`

```
HBox "barre_haut"                              gap: 12   align: Center   minSize: (0, 56)
  Image "haut.logo"                            source: "nkantenne_logo"   size: (28, 28)
  Text "haut.nom"                              text: "NkAntenne"
  RadioGroup "espace.choix"                    bind: espace.actif   options: ["Studio","Montage","Compression","Médiathèque"]   orientation: Horizontal
  Spacer "pousse"
  BadgeAntenne "app.badge_direct"
  BadgeAntenne "app.badge_rec"
  ToggleButton✚ "app.theme"                    icon: moon   bind: app.theme_sombre   tooltip: "Thème sombre / clair"
  Button "app.reglages"                        icon: settings   tooltip: "Réglages"
  Button "app.aide"                            icon: circle-help   tooltip: "Aide"
```
- **Comportements** :
  ```
  behavior "espace.choix"(Changed) { open espace.actif }        // P25 : remplacer l'écran
  behavior "app.badge_direct" { … visible = antenne.en_direct ; duree = antenne.duree_direct }
  behavior "app.badge_rec"    { … visible = antenne.enregistre ; duree = antenne.duree_rec }
  ```
- **Les badges sont dans la barre du haut, commune aux quatre espaces** : c'est la
  règle « on voit toujours si l'on est en direct » (doc 02 §11). Ne les mets **pas**
  dans le document d'un espace.

### 5.3 `menus.nkgui` — cachée par défaut

```
MenuBar "menus"                                visible: false        (Alt la montre ; l'application tient la règle)
  Menu "menu_fichier"                          label: "Fichier"
    MenuItem "app.nouvelle_collection"         label: "Nouvelle collection de scènes"
    MenuItem "app.importer_collection"         label: "Importer une collection…"
    MenuItem "app.exporter_collection"         label: "Exporter la collection…"
    Separator "s1"
    MenuItem "media.ouvrir"                    label: "Ouvrir un média…"             shortcut: "Ctrl+O"
    MenuItem "app.dossier_enregistrements"     label: "Dossier des enregistrements"
    Separator "s2"
    MenuItem "app.quitter"                     label: "Quitter"                     shortcut: "Ctrl+Q"
  Menu "menuEdition"                          label: "Édition"
    MenuItem "commun.annuler"                  label: "Annuler"                     shortcut: "Ctrl+Z"
    MenuItem "commun.refaire"                  label: "Refaire"                     shortcut: "Ctrl+Y"
    MenuItem "app.palette"                     label: "Palette de commandes"        shortcut: "Ctrl+K"
  Menu "menu_affichage"                        label: "Affichage"
    MenuItem "espace.studio"                   label: "Studio"                      shortcut: "Ctrl+1"
    MenuItem "espace.montage"                  label: "Montage"                     shortcut: "Ctrl+2"
    MenuItem "espace.compression"              label: "Compression"                 shortcut: "Ctrl+3"
    MenuItem "espace.mediatheque"              label: "Médiathèque"                 shortcut: "Ctrl+4"
    Separator "s3"
    MenuItem "studio.mode_studio"              label: "Mode studio"                 checked: false
    MenuItem "mixeur.vertical"                 label: "Mixeur vertical"             checked: false
    MenuItem "app.theme"                       label: "Thème sombre"                checked: true
  Menu "menu_antenne"                          label: "Antenne"
    MenuItem "antenne.direct"                  label: "Démarrer le direct"
    MenuItem "antenne.enregistrer"             label: "Enregistrer"
    MenuItem "antenne.marqueur"                label: "Poser un marqueur"
    MenuItem "antenne.clip"                    label: "Enregistrer les dernières secondes"
    MenuItem "antenne.capture_image"           label: "Capture d'écran"
    Separator "s4"
    MenuItem "dest.ouvrir"                     label: "Destinations…"
    MenuItem "antenne.test_performance"        label: "Test de performance…"
  Menu "menuAide"                             label: "Aide"
    MenuItem "app.assistant"                   label: "Assistant de démarrage…"
    MenuItem "commun.aide_raccourcis"          label: "Raccourcis"
    MenuItem "commun.apropos"                  label: "À propos"
```
⚠️ **Aucun raccourci par défaut** pour démarrer ou arrêter le direct (doc 02 §10) :
l'utilisateur les **choisit** dans les Réglages.
⚠️ `commun.annuler` / `commun.refaire` / `commun.aide_raccourcis` /
`commun.apropos` : **nouveaux** identifiants communs, dans le prolongement de la
décision de NKCraft 03 §0.3 ; les applications existantes gardent leurs `anim.*`
servis — ne les renomme pas.

### 5.4 L'espace Studio

#### `studio.nkgui`

```
Window "studio"                                title: ""   flags: NoTitleBar
  VBox "racine"                                gap: 0
    Host "studio.barre_haut"                   hint: "barre_haut.nkgui (commune aux quatre espaces)"
    Splitter "studio.partage"                  orientation: Vertical   ratio: 0.82
      Host "studio.vue"                        hint: "PROGRAMME (liseré @studio.programme pendant le direct) ; en mode studio : APERÇU | Transition | PROGRAMME ; poignées de sources ; compte à rebours de démarrage"
      Host "studio.commandes_hote"             hint: "studio_commandes.nkgui"
    Splitter "studio.bas"                      orientation: Vertical   ratio: 0.22
      Host "studio.scenes_hote"                hint: "studio_scenes.nkgui"
      Splitter "studio.bas2"                   orientation: Vertical   ratio: 0.36
        Host "studio.sources_hote"             hint: "studio_sources.nkgui"
        Host "studio.mixeur_hote"              hint: "studio_mixeur.nkgui"
    Host "studio.sante_hote"                   hint: "barre_sante.nkgui"
```

#### `studio_commandes.nkgui`

```
VBox "commandes"                               gap: 10   minSize: (180, 0)
  Text "cmd.titre"                             text: "COMMANDES"
  SplitButton✚ "antenne.direct"                label: "Démarrer le direct"   icon: radio   minSize: (0, 48)
    appearance   radius: 6   fill: @accent.plein
    MenuItem "dest.ouvrir"                     label: "Choisir les destinations…"
  BoutonAntenne "antenne.arreter_direct"       label: "Arrêter le direct"   icon: square   visible: false
    appearance   fill: #F85149 // @antenne.direct
  BoutonAntenne "antenne.enregistrer"          label: "Enregistrer"   icon: circle
  HBox "cmd.rec_actif"                         gap: 6   visible: false
    BoutonAntenne "antenne.arreter_rec"        label: "Arrêter"   icon: square
      appearance   fill: #F85149 // @antenne.enregistrement
    BoutonAntenne "antenne.pause_rec"          label: "Pause"     icon: pause
  BoutonAntenne "antenne.marqueur"             label: "Marqueur"   icon: flag
  ToggleButton✚ "studio.mode_studio"           label: "Mode studio"   icon: columns-2   bind: studio.mode   minSize: (0, 40)
  BoutonAntenne "antenne.clip"                 label: "Clip 30 s"   icon: history   visible: false
```
- **Comportements continus** (idempotents ✅) :
  ```
  antenne.direct.visible         = not antenne.en_direct
  antenne.arreter_direct.visible = antenne.en_direct
  antenne.enregistrer.visible    = not antenne.enregistre
  cmd.rec_actif.visible          = antenne.enregistre
  antenne.clip.visible           = reglages.tampon_actif
  antenne.direct : disable because "Aucune destination active — ouvre Destinations" si dest.nb_actives == 0
  ```
- **Au geste** (P25) :
  ```
  behavior "antenne.arreter_direct"(Click) {
    open "confirmer_arret" as modal
  }
  ```
  **Démarrer** : l'application affiche le **compte à rebours de 3 s** annulable dans
  `studio.vue` (pas un document : c'est une surimpression de la vue).
- `@accent.plein` est un **jeton de structure** (le bouton principal), permis en
  `fill` : ce n'est pas une couleur d'information, c'est **le** bouton principal —
  signale-le si les contrôles le refusent.

#### `studio_scenes.nkgui`

```
Panel "scenes"                                 title: "Scènes"
  VBox "colonne"                               gap: 4
    HBox "sc.barre"                            gap: 4
      Dropdown "scene.collection"              bind: scene.collection   (items : collections)
      Button "scene.ajouter"                   icon: plus   tooltip: "Nouvelle scène"
    Host "sc.liste"                            hint: "scènes : nom, la scène à l'antenne marquée ● rouge pendant le direct, raccourci ; clic = à l'antenne (ou à l'aperçu en mode studio) ; clic droit : renommer, dupliquer, supprimer, raccourci"
```

#### `studio_sources.nkgui`

```
Panel "sources"                                title: "Sources"
  VBox "colonne"                               gap: 4
    HBox "so.barre"                            gap: 4
      Button "source.ajouter"                  icon: plus   label: "Source"
      Button "source.filtres"                  icon: sliders-horizontal   tooltip: "Filtres de la source"
      Button "source.proprietes"               icon: settings   tooltip: "Propriétés"
    Host "so.liste"                            hint: "sources de la scène : œil, cadenas, icône du type, nom ; glisser pour l'ordre ; double clic = propriétés"
```
`source.ajouter` → `open "choix_source" as modal`.
`source.filtres`, `source.proprietes` : `disable because "Sélectionne une source"`
sans sélection ✅.

#### `studio_mixeur.nkgui`

```
Panel "mixeur"                                 title: "Mixeur"
  VBox "colonne"                               gap: 6
    Scroll "mx.defil"                          axis: Vertical
      VBox "mx.lignes"                         gap: 6     (LigneMixeur par source audio, ajoutées par l'application)
    HBox "mx.pied"                             gap: 4
      Text "mx.lufs"                           text: "— LUFS (cible −14)"   font: "Mono-11"
      Spacer "pousse"
      ToggleButton✚ "mixeur.vertical"          icon: sliders-vertical   bind: mixeur.vertical   tooltip: "Mixeur vertical"
```
En vertical, l'application remplace `mx.lignes` par des **`TrancheMixeur`** (NKScena).

#### `barre_sante.nkgui`

```
HBox "sante"                                   gap: 16   align: Center   minSize: (0, 32)
  HBox "sa.debit"                              gap: 4
    Text "sa.debit_titre"                      text: "↑"
    LigneSante "sa.debit_val"
  HBox "sa.pertes"                             gap: 4
    Text "sa.pertes_titre"                     text: "images perdues"
    LigneSante "sa.pertes_val"
  HBox "sa.cpu"                                gap: 4
    Text "sa.cpu_titre"                        text: "processeur"
    LigneSante "sa.cpu_val"
  HBox "sa.gpu"                                gap: 4
    Text "sa.gpu_titre"                        text: "GPU"
    LigneSante "sa.gpu_val"
  Spacer "pousse"
  Host "sa.destinations"                       hint: "une pastille par destination active : plateforme, ● état ; clic = détail et Réessayer"
  Button "antenne.stats"                       icon: activity   tooltip: "Statistiques détaillées"
  Text "sa.duree"                              text: "⏱ 00:00:00"   font: "Mono-12"
```
Tonalité de chaque `LigneSante.pastille` : seuils portés par l'application
(`sante.bon` / `moyen` / `mauvais`) — comportement continu ✅.

### 5.5 Les tiroirs (P28)

#### `tiroir_destinations.nkgui`

```
Drawer✚ "tiroir_destinations"                  edge: Right   size: 0.34   modal: false   bind: dest.tiroir_ouvert
  VBox "colonne"                               gap: 8
    HBox "td.tete"                             gap: 4   align: Center
      Text "td.titre"                          text: "Destinations"
      Spacer "pousse"
      Button "dest.ajouter"                    label: "+ Destination"   icon: plus
      Button "dest.fermer"                     icon: x   tooltip: "Fermer (Échap)"
    Text "td.vide"                             text: "Aucune destination. Ajoute YouTube, Facebook, TikTok… avec leur clé de diffusion."   wrap: true   visible: false
    Scroll "td.defil"                          axis: Vertical
      VBox "td.cartes"                         gap: 8     (CarteDestination par destination)
    Text "td.note"                             text: "Les clés sont chiffrées sur cet ordinateur et ne quittent jamais NkAntenne, sauf vers leur plateforme."   wrap: true
```
`dest.ajouter` ouvre une **grille** de `Tile` plateformes (YouTube, Facebook, TikTok,
Twitch, Kick, RTMP personnalisé, SRT, Réseau local) puis le formulaire de la carte.

#### `tiroir_filtres.nkgui`, `tiroir_transitions.nkgui`, `tiroir_stats.nkgui`

Même patron (`Drawer✚`, `edge: Right`) :
- **Filtres** : titre = la source ; **deux sections** (Image, Audio), la **pile** des
  filtres (`ModuleEffet` commun : actif, préréglages, supprimer), **+ Filtre ▾** ;
  les paramètres **engendrés** (règle de Nogee 06 §0.2).
- **Transitions** : liste (Coupe, Fondu, Glissement, Volet, Stinger, `.nkgui`),
  **durée** (`LigneNombre`, ms), pour Stinger un `LigneAsset` (vidéo), pour `.nkgui`
  un `LigneAsset` (document) + « Ouvrir dans NKUIDesign ».
- **Statistiques** : par destination — débit, images perdues (réseau / encodage /
  rendu), reconnexions, durée ; `Host` de courbes.

### 5.6 Les dialogues

#### `dlg_choix_source.nkgui`

```
Window "dlg_choix_source"                      title: "Ajouter une source"   modal: true
  VBox "racine"                                gap: 10
    Flow "cs.grille"                           gap: 10
      TuileSource "source.type_ecran"          image: "sources/ecran"      label: "Écran"                caption: "Un moniteur entier"
      TuileSource "source.type_fenetre"        image: "sources/fenetre"    label: "Fenêtre"              caption: "Une application, même recouverte"
      TuileSource "source.type_zone"           image: "sources/zone"       label: "Zone"                 caption: "Un rectangle de l'écran"
      TuileSource "source.type_jeu"            image: "sources/jeu"        label: "Jeu"                  caption: "Plein écran, faible latence"
      TuileSource "source.type_camera"         image: "sources/camera"     label: "Caméra"               caption: "Webcam, carte de capture"
      TuileSource "source.type_telephone"      image: "sources/telephone"  label: "Téléphone"            caption: "Par le réseau local"
      TuileSource "source.type_micro"          image: "sources/micro"      label: "Micro"                caption: "Une entrée audio"
      TuileSource "source.type_audio_systeme"  image: "sources/bureau"     label: "Audio du système"     caption: "Ce que l'ordinateur joue"
      TuileSource "source.type_audio_appli"    image: "sources/appli"      label: "Audio d'une application" caption: "Le son d'un seul programme"
      TuileSource "source.type_media"          image: "sources/media"      label: "Média"                caption: "Une vidéo, un son"
      TuileSource "source.type_image"          image: "sources/image"      label: "Image"                caption: "Une image, un diaporama"
      TuileSource "source.type_texte"          image: "sources/texte"      label: "Texte"
      TuileSource "source.type_superposition"  image: "sources/nkgui"      label: "Superposition"        caption: "Un document de NKUIDesign"
      TuileSource "source.type_reseau"         image: "sources/reseau"     label: "Réseau"               caption: "Un autre ordinateur"
      TuileSource "source.type_chrono"         image: "sources/chrono"     label: "Chronomètre"
      TuileSource "source.type_couleur"        image: "sources/couleur"    label: "Couleur"
    HBox "cs.boutons"                          gap: 4   justify: End
      Button "cs.annuler"                      label: "Annuler"
```
Un clic sur une tuile ferme le dialogue et ouvre les **propriétés** de la nouvelle
source (engendrées par type — doc 02 §5).
`source.type_audio_appli.enabled` selon le système (raison : « Demande Windows 10
version 2004 ou plus récent »).

#### `dlg_confirmer_arret.nkgui`

```
Window "dlg_confirmer_arret"                   title: "Arrêter le direct ?"   modal: true
  VBox "racine"                                gap: 10
    Text "ca.message"                          text: "Le direct s'arrêtera sur : YouTube, Facebook."   wrap: true
    Text "ca.rec"                              text: "L'enregistrement local continue."   visible: false
    HBox "ca.boutons"                          gap: 6   justify: End
      Button "ca.annuler"                      label: "Continuer le direct"
      BoutonAntenne "antenne.confirmer_arret"  label: "Arrêter le direct"   icon: square
        appearance   fill: #F85149 // @antenne.direct
```
- `ca.message.text` : la liste **réelle** des destinations actives (comportement ✅).
- `ca.rec.visible = antenne.enregistre` ✅ — dire qu'on **ne perd pas** l'enregistrement.
- **Le bouton par défaut (Entrée) est « Continuer le direct »**, pas l'arrêt.

### 5.7 L'espace Montage

#### `montage.nkgui`

```
Window "montage"                               title: ""   flags: NoTitleBar
  VBox "racine"                                gap: 0
    Host "mo.barre_haut"                       hint: "barre_haut.nkgui"
    Splitter "mo.haut"                         orientation: Vertical   ratio: 0.20
      Panel "mo.medias"                        title: "Médias"
        VBox "mo.medias_col"                   gap: 4
          Button "montage.importer"            label: "+ Importer"   icon: plus
          Host "mo.medias_liste"               hint: "vignettes des médias du projet ; glisser vers la timeline"
      Splitter "mo.haut2"                      orientation: Vertical   ratio: 0.74
        VBox "mo.centre"                       gap: 4
          RadioGroup "montage.format"          bind: montage.format   options: ["16:9","9:16","1:1"]   orientation: Horizontal
          Host "mo.lecture"                    hint: "lecture du montage à la tête de lecture ; en 9:16, cadre de recadrage déplaçable"
          HBox "mo.transport"                  gap: 4   align: Center   justify: Center
            Button "lecteur.precedent"         icon: skip-back
            Button "lecteur.jouer"             icon: play   minSize: (40, 40)
            Button "lecteur.suivant"           icon: skip-forward
            TimecodeField✚ "mo.tc"             bind: montage.tete
        Panel "mo.proprietes"                  title: "Propriétés"
          Host "mo.proprietes_contenu"         hint: "propriétés du clip choisi, engendrées : vitesse (ou courbe), volume, couleur, LUT, recadrage, zoom sur le curseur"
    HBox "mo.outils"                           gap: 6   align: Center
      Button "montage.rasoir"                  label: "Rasoir"                  icon: scissors         tooltip: "C"
      Button "montage.rogner"                  label: "Rogner"                  icon: move-horizontal
      Button "montage.supprimer_resserrer"     label: "Supprimer et resserrer"  icon: delete
      Separator "s1"                           orientation: Vertical
      Button "montage.silences"                label: "Couper les silences"     icon: volume-x
      Button "montage.vitesse"                 label: "Vitesse"                 icon: gauge
      Button "montage.sous_titres"             label: "Sous-titres"             icon: captions
      Button "montage.titre"                   label: "Titre"                   icon: type
      Button "montage.transition"              label: "Transition"              icon: blend
      Button "montage.zoom_curseur"            label: "Zoom sur le curseur"     icon: zoom-in
      Spacer "pousse"
      SplitButton✚ "montage.exporter"          label: "Exporter"   icon: download
        MenuItem "montage.export_youtube"      label: "YouTube 1080p"
        MenuItem "montage.export_youtube_4k"   label: "YouTube 4K"
        MenuItem "montage.export_shorts"       label: "Shorts / TikTok / Reels (9:16)"
        MenuItem "montage.export_whatsapp"     label: "WhatsApp (< 16 Mo)"
        MenuItem "montage.vers_compression"    label: "Vers la compression…"
    Host "mo.timeline"                         hint: "LA TIMELINE PARTAGÉE (Editor Kit) : pistes V / A / sous-titres, marqueurs du direct ⚑, morceaux à retirer hachurés en aperçu"
```

#### `montage_silences.nkgui` (panneau d'aperçu)

```
Panel "silences"                               title: "Couper les silences"
  VBox "colonne"                               gap: 6
    LigneNombre "sil.seuil"                    etiquette: "Seuil (dB)"         bind: sil.seuil   min: -80   max: -10
    LigneNombre "sil.duree"                    etiquette: "Durée minimale (ms)"   bind: sil.duree   min: 100   max: 5000
    LigneNombre "sil.marge"                    etiquette: "Marge (ms)"         bind: sil.marge   min: 0   max: 1000
    Text "sil.resume"                          text: "— morceaux, — retirés"
    HBox "sil.boutons"                         gap: 4   justify: End
      Button "sil.annuler"                     label: "Annuler"
      BoutonEcrit "montage.appliquer_silences" label: "Retirer"   icon: scissors
```
`cle.visible = false` sur les lignes. Les morceaux à retirer sont **hachurés** dans la
timeline tant que le panneau est ouvert (`@info.apercu`, jeton de la famille).

### 5.8 `compression.nkgui`

```
Window "compression"                           title: ""   flags: NoTitleBar
  VBox "racine"                                gap: 0
    Host "co.barre_haut"                       hint: "barre_haut.nkgui"
    Splitter "co.partage"                      orientation: Vertical   ratio: 0.62
      VBox "co.file"                           gap: 6
        Text "co.titre"                        text: "File"
        Text "co.vide"                         text: "Glisse des fichiers ici, ou « + Ajouter des fichiers »."   visible: false
        Host "co.liste"                        hint: "travaux : fichier → préréglage, progression, résultat (taille obtenue, écart à l'estimation)"
        HBox "co.boutons"                      gap: 6
          Button "comp.ajouter"                label: "+ Ajouter des fichiers"   icon: plus
          Spacer "pousse"
          Text "co.estimation"                 text: "Estimation : —"
          Button "comp.lancer"                 label: "Lancer"   icon: play
          Button "comp.pause"                  label: "Pause"    icon: pause
      VBox "co.reglages"                       gap: 6
        Text "co.reglages_titre"               text: "Réglages"
        LigneListe "comp.prereglage"           etiquette: "Préréglage"   bind: comp.prereglage   (items : préréglages — des fichiers de données)
        RadioGroup "comp.mode"                 bind: comp.mode   options: ["Taille cible","Qualité"]   orientation: Horizontal
        LigneNombre "comp.taille"              etiquette: "Taille cible (Mo)"   bind: comp.taille
        LigneNombre "comp.qualite"             etiquette: "Qualité"   bind: comp.qualite   min: 0   max: 100   visible: false
        Categorie "co.video"                   label: "Vidéo"
          LigneListe "comp.codec_video"        etiquette: "Codec"        bind: comp.codec_video   items: ["H.264"]
          LigneListe "comp.definition"         etiquette: "Définition"   bind: comp.definition   items: ["Source","2160p","1440p","1080p","720p","480p"]
          LigneListe "comp.cadence"            etiquette: "Cadence"      bind: comp.cadence   items: ["Source","60","50","30","25","24"]
        Categorie "co.audio"                   label: "Audio"
          LigneListe "comp.codec_audio"        etiquette: "Codec"        bind: comp.codec_audio   items: ["AAC","Opus","MP3","FLAC","WAV"]
          LigneListe "comp.debit_audio"        etiquette: "Débit"        bind: comp.debit_audio   items: ["64 kb/s","96 kb/s","128 kb/s","192 kb/s","320 kb/s"]
          LigneCase "comp.audio_seul"          etiquette: "Extraire l'audio seul"   bind: comp.audio_seul
        Categorie "co.decoupe"                 label: "Découpe"   expanded: false
          HBox "co.intervalle"                 gap: 4
            TimecodeField✚ "comp.debut"        bind: comp.debut
            TimecodeField✚ "comp.fin"          bind: comp.fin
          LigneListe "comp.morceaux"           etiquette: "Découper en morceaux"   bind: comp.morceaux   items: ["Non","Par durée…","Par taille…"]
```
- `comp.taille.visible = (comp.mode == "Taille cible")` ; `comp.qualite.visible =
  (comp.mode == "Qualité")` ✅.
- **Codecs non encore écrits** (AAC, Opus, MP3, FLAC en encodage — doc 01 §7.2) :
  options **grisées avec leur raison** (« Encodeur pas encore disponible »). Écris-les.
- Toutes les lignes : `cle.visible = false`.

### 5.9 Médiathèque et lecteurs

#### `mediatheque.nkgui`

```
Window "mediatheque"                           title: ""   flags: NoTitleBar
  VBox "racine"                                gap: 8
    Host "me.barre_haut"                       hint: "barre_haut.nkgui"
    HBox "me.barre"                            gap: 8   align: Center
      TextField "me.recherche"                 placeholder: "Rechercher (titres et sous-titres)…"   icon✚: search   bind: media.recherche
      RadioGroup "media.filtre"                bind: media.filtre   options: ["Tout","Vidéos","Audio","Directs","Exports"]   orientation: Horizontal
      Spacer "pousse"
      RadioGroup "media.vue"                   bind: media.vue   options: ["Grille","Liste"]   orientation: Horizontal
    Text "me.vide"                             text: "Tes enregistrements, montages et exports apparaîtront ici."   visible: false
    Scroll "me.defil"                          axis: Vertical
      Flow "me.grille"                         gap: 12     (Tile par média : vignette, titre, durée, date ; double clic → lecteur)
```

#### `lecteur_video.nkgui`

```
Window "lecteur_video"                         title: "?"
  VBox "racine"                                gap: 0
    Host "lv.image"                            hint: "la vidéo (moteur de NkVideoPlayer : NkVideoReader, horloge audio maîtresse)"
    Host "lv.progression"                      hint: "barre de progression avec les MARQUEURS ⚑ et la boucle A-B"
    HBox "lv.commandes"                        gap: 8   align: Center
      Button "lecteur.jouer"                   icon: play   minSize: (40, 40)
      Button "lecteur.image_prec"              icon: chevron-left    tooltip: "Image précédente (,)"
      Button "lecteur.image_suiv"              icon: chevron-right   tooltip: "Image suivante (.)"
      TimecodeField✚ "lv.tc"                   bind: lecteur.position
      Spacer "pousse"
      Button "lecteur.boucle_ab"               label: "A-B"   tooltip: "Boucle entre deux points"
      Dropdown "lecteur.vitesse"               bind: lecteur.vitesse   items: ["0,25×","0,5×","1×","1,5×","2×","4×"]
      Dropdown "lecteur.piste_audio"           bind: lecteur.piste   (items : pistes du fichier)
      Dropdown "lecteur.sous_titres"           bind: lecteur.sous_titres   (items : Aucun + pistes)
      Button "lecteur.plein_ecran"             icon: maximize   tooltip: "Plein écran (F)"
    HBox "lv.actions"                          gap: 6   justify: End
      Button "media.ouvrir_montage"            label: "Ouvrir au montage"   icon: film
      Button "media.compresser"                label: "Compresser"          icon: package
      Button "media.montrer_dossier"           label: "Montrer dans le dossier"   icon: folder
```
📌 `lecteur.*` : **mêmes identifiants** que le futur document de NkVideoPlayer (même
moteur) — quand NkVideoPlayer aura son interface `.nkgui`, ses commandes de base
seront **un composant commun**.

#### `lecteur_audio.nkgui`

```
Window "lecteur_audio"                         title: "?"
  VBox "racine"                                gap: 8
    Host "la.onde"                             hint: "forme d'onde pleine largeur, tête de lecture, boucle A-B"
    HBox "la.commandes"                        gap: 8   align: Center   justify: Center
      Button "lecteur.precedent"               icon: skip-back
      Button "lecteur.jouer"                   icon: play   minSize: (48, 48)
      Button "lecteur.suivant"                 icon: skip-forward
      TimecodeField✚ "la.tc"                   bind: lecteur.position
      Dropdown "lecteur.vitesse"               bind: lecteur.vitesse   items: ["0,5×","0,75×","1×","1,25×","1,5×","2×"]
      ToggleButton✚ "lecteur.boucle"           icon: repeat   bind: lecteur.boucle
    Text "la.meta"                             text: "—"     (titre, artiste, format, débit)
    Expander "la.liste"                        label: "Liste de lecture"   expanded: true
      Host "la.liste_hote"                     hint: "liste de lecture : glisser pour réordonner"
```

### 5.10 `reglages.nkgui`

Même patron que PV3DE 03 §5.16 : `ListBox` des catégories à gauche (**Général,
Diffusion, Sortie, Enregistrement, Audio, Vidéo, Raccourcis, Vie privée, Avancé**),
les lignes à droite. Particularités :

- **Sortie ▸ Encodeur** : `LigneListe "reglages.encodeur"` items `["Matériel
  (recommandé)","Nkentseu (logiciel)"]` ; l'option matérielle **grisée avec sa
  raison** si aucun encodeur matériel n'est détecté.
- **Raccourcis** : `LigneTouche` (Nogee 06 §3.1, P17) pour chaque geste, avec une
  case **« global »** ; **aucune valeur par défaut** pour Démarrer / Arrêter le direct.
- **Vie privée** : liste des **fenêtres exclues** (`Host`), `LigneCase "reglages.mode_prive"`,
  et **« Effacer toutes les clés »** (`BoutonEcrit`, avec confirmation).
- Toutes les lignes : `cle.visible = false`.

### 5.11 `assistant_demarrage.nkgui`

```
Window "assistant"                             title: "Bienvenue"   modal: true
  VBox "racine"                                gap: 12
    Stepper✚ "as.etapes"                       bind: assistant.etape   steps: ["Usage","Capture","Destinations","Performance"]   free: false
    VBox "as.usage"                            gap: 10   visible: true
      Text "as.q1"                             text: "Que veux-tu faire ?"
      HBox "as.usages"                         gap: 12
        Tile✚ "accueil.usage_direct"           image: "accueil/direct"   label: "Diffuser en direct"
        Tile✚ "accueil.usage_enregistrer"      image: "accueil/rec"      label: "Enregistrer"   caption: "tutoriel, cours, jeu"
        Tile✚ "accueil.usage_les_deux"         image: "accueil/deux"     label: "Les deux"
    VBox "as.capture"                          gap: 10   visible: false
      Text "as.q2"                             text: "Que veux-tu capturer ?"
      HBox "as.captures"                       gap: 12
        Tile✚ "accueil.capture_ecran"          image: "sources/ecran"     label: "Tout l'écran"
        Tile✚ "accueil.capture_fenetre"        image: "sources/fenetre"   label: "Une fenêtre"
        Tile✚ "accueil.capture_jeu"            image: "sources/jeu"       label: "Un jeu"
      HBox "as.plus"                           gap: 12
        Checkbox "as.camera"                   label: "+ Caméra"   bind: assistant.camera
        Checkbox "as.micro"                    label: "+ Micro"    bind: assistant.micro
    VBox "as.destinations"                     gap: 10   visible: false
      Text "as.q3"                             text: "Où diffuser ?"
      Flow "as.plateformes"                    gap: 12     (Tile par plateforme + « Plus tard »)
    VBox "as.performance"                      gap: 10   visible: false
      Text "as.q4"                             text: "Test de performance (10 s)…"
      Progress "as.progression"                bind: assistant.test_progression
      Text "as.resultat"                       text: ""   wrap: true
    HBox "as.boutons"                          gap: 6   justify: End
      Button "accueil.precedent"               label: "Précédent"
      Button "accueil.suivant"                 label: "Suivant"
      Button "accueil.terminer"                label: "Commencer"   visible: false
```
Visibilité des étapes selon `assistant.etape` ✅ ; l'étape **Destinations** est
`Hidden` si l'usage est « Enregistrer ».

### 5.12 `NkAntenne.tests.nkgui` — les premiers tests (P27)

```
test "Arrêter le direct demande confirmation et garde l'enregistrement" {
  depart "studio"
  doublure "diffusion.youtube" -> "connecte"
  click "antenne.direct"
  click "antenne.enregistrer"
  click "antenne.arreter_direct"
  expect ecran "confirmer_arret"
  expect visible "ca.rec"
  answer "Continuer le direct"
  expect visible "antenne.arreter_direct"
}

test "Démarrer sans destination est impossible, et dit pourquoi" {
  depart "studio"
  expect disabled "antenne.direct" because "Aucune destination active — ouvre Destinations"
}

test "La clé de diffusion n'est jamais affichée sans le geste" {
  depart "studio"
  click "dest.ouvrir"
  expect "td.cartes.cle".secret == true
  click "td.cartes.cle_voir"
  expect "td.cartes.cle".secret == false
}

test "Le badge EN DIRECT reste visible dans les autres espaces" {
  depart "studio"
  doublure "diffusion.youtube" -> "connecte"
  click "antenne.direct"
  select "espace.choix" "Montage"
  expect visible "app.badge_direct"
}
```
⚠️ Les chemins `"td.cartes.cle"` supposent que l'on sait désigner **un enfant
d'instance** de composant — la même question de format que ci-dessus. Signale-le.

---

## 6. Les actions — toutes ✚

| domaine | actions |
|---|---|
| **app** | `badge_direct` · `badge_rec` · `theme` · `reglages` · `aide` · `palette` · `assistant` · `nouvelle_collection` · `importer_collection` · `exporter_collection` · `dossier_enregistrements` · `quitter` |
| **espace** | `studio` · `montage` · `compression` · `mediatheque` |
| **antenne** | `direct` · `arreter_direct` · `confirmer_arret` · `enregistrer` · `arreter_rec` · `pause_rec` · `marqueur` · `clip` · `capture_image` · `test_performance` · `stats` |
| **studio** | `mode_studio` |
| **scene** | `ajouter` |
| **source** | `ajouter` · `filtres` · `proprietes` · `type_ecran` · `type_fenetre` · `type_zone` · `type_jeu` · `type_camera` · `type_telephone` · `type_micro` · `type_audio_systeme` · `type_audio_appli` · `type_media` · `type_image` · `type_texte` · `type_superposition` · `type_reseau` · `type_chrono` · `type_couleur` |
| **mixeur** | `vertical` |
| **dest** | `ouvrir` · `ajouter` · `fermer` · `tester` · `supprimer` |
| **montage** | `importer` · `rasoir` · `rogner` · `supprimer_resserrer` · `silences` · `appliquer_silences` · `vitesse` · `sous_titres` · `titre` · `transition` · `zoom_curseur` · `exporter` · `export_youtube` · `export_youtube_4k` · `export_shorts` · `export_whatsapp` · `vers_compression` |
| **comp** | `ajouter` · `lancer` · `pause` |
| **media** | `ouvrir` · `ouvrir_montage` · `compresser` · `montrer_dossier` |
| **lecteur** | `jouer` · `precedent` · `suivant` · `image_prec` · `image_suiv` · `boucle_ab` · `boucle` · `plein_ecran` |
| **accueil** | `usage_direct` · `usage_enregistrer` · `usage_les_deux` · `capture_ecran` · `capture_fenetre` · `capture_jeu` · `precedent` · `suivant` · `terminer` |
| **commun** | `annuler` · `refaire` · `aide_raccourcis` · `apropos` (nouveaux identifiants communs) |
| **(dialogues)** | `cs.annuler` · `ca.annuler` · `sil.annuler` |

### Raisons de grisé

| cas | raison |
|---|---|
| action non servie | « Pas encore disponible dans NkAntenne » |
| `antenne.direct` sans destination active | « Aucune destination active — ouvre Destinations » |
| `source.type_audio_appli` sur un système trop ancien | « Demande Windows 10 version 2004 ou plus récent » (et l'équivalent par système) |
| un encodeur audio non écrit | « Encodeur pas encore disponible » |
| `reglages.encodeur` = matériel sans matériel | « Aucun encodeur matériel détecté sur cet ordinateur » |
| `source.filtres` sans sélection | « Sélectionne une source » |
| destination TikTok sans accès | « Ce compte n'a pas accès à la diffusion par clé » (message de la plateforme, repris tel quel) |

---

## 7. Ce que tu ne sais pas encore reproduire — écrit quand même

| élément | quoi | état |
|---|---|---|
| `BadgeAntenne "app.badge_rec"` | point **pulsant** | animation comptée, non jouée |
| `Drawer` | entrée glissée (160 ms), fermeture au clic extérieur | P28 |
| `Meter` | zones vert / ambre / rouge, crête, écrêtage | P14 |
| `SplitButton "antenne.direct"` | flèche des destinations | P18 |
| boutons « actif » (Arrêter) | alternance par `visible` en attendant `set "id".appearance` | P25 |
| compte à rebours de démarrage | surimpression de la vue | peint par l'application |
| polices | `"Mono-12"` pour les durées, `"Mono-11"` pour les débits et LUFS | une seule fonte aujourd'hui |

---

## 8. L'ordre de livraison — aligné sur le doc 01 §14

| # | livrable | palier | témoin |
|---|---|---|---|
| 1 | `composants_antenne` + `barre_haut` + `studio` + `studio_commandes` (enregistrer seulement) + `barre_sante` (processeur, images) | P0 | enregistrer 10 min d'écran depuis l'interface ; le badge ● REC visible **dans les quatre espaces** |
| 2 | `studio_scenes` + `studio_sources` + `studio_mixeur` + `dlg_choix_source` + `tiroir_filtres` + `assistant_demarrage` | P1 | l'assistant crée une scène « écran + caméra + micro » qui s'enregistre sans image perdue |
| 3 | direct : `studio_commandes` complet + `tiroir_destinations` + `dlg_confirmer_arret` + `tiroir_stats` + **les tests du §5.12** | P2 | les quatre tests passent, et **« Vérifier les tests »** (NKUIDesign) les fait échouer quand on retire la confirmation |
| 4 | `tiroir_transitions` + mode studio | P2–P3 | |
| 5 | `mediatheque` + `lecteur_video` + `lecteur_audio` | P4 | |
| 6 | `montage` + `montage_silences` | P4 | un direct d'une heure → un Short sous-titré en 10 minutes |
| 7 | `compression` | P5 | |
| 8 | `reglages` (complet) + `menus` | au fil de l'eau | |
