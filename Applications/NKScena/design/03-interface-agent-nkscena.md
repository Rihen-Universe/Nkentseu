# NKScena — l'interface, pour l'agent qui écrit les `.nkgui`

### Document 3 — Spécification d'interface côté agent

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> Traduction du **02** en tes quatre points : **arbre nommé**, **composants**,
> **actions**, **couleurs porteuses** — plus ce que tu ne sais pas encore
> reproduire, écrit quand même.
> **Si 02 et 03 se contredisent, 02 gagne** : signale l'écart.
>
> **Prérequis : le document 09 de NkAnima.** Ce document **ne répète pas** ses
> règles de lecture (§0), ses propositions de vocabulaire P1 à P12 (§1), ses
> jetons (§2) ni ses composants (§3) : il les **emploie**. Ce qui est propre à
> NKScena est ici.

---

## 0. Ce qui change par rapport à NkAnima

1. **Les composants communs déménagent** dans
   `Resources/Interface/Commun/composants.nkgui`, et NkAnima **et** NKScena
   l'incluent. Deux applications qui recopient les mêmes composants
   divergeraient — c'est la même raison qu'entre deux timelines. ✅ `include`
   est branché depuis le 26/09 : écris `include "../Commun/composants.nkgui"`
   (si le chemin relatif avec `..` est refusé, dis-le : c'est le seul besoin
   nouveau).
2. **Les panneaux partagés sont partagés** : `vue_3d`, `scene`, `details`,
   `bibliotheque`, `sequenceur`, `assistant_ia` sont écrits **une fois** dans
   `Resources/Interface/Commun/`, et chaque application les monte. Là où NKScena
   a besoin d'un élément de plus, elle l'écrit **à côté** (un document
   supplémentaire monté dans un `Host`), **jamais** en recopiant le panneau.
3. **Les actions de NKScena** sont rangées par domaine : `lecture.`, `film.`,
   `plan.`, `cam.`, `lum.`, `decor.`, `prep.`, `montage.`, `son.`, `etal.`,
   `comp.`, `titre.`, `rendu.`, et les domaines communs `fichier.`, `edition.`,
   `selection.`, `vue.`, `espace.`, `outil.`, `ia.`, `biblio.`, `fenetre.`,
   `aide.`, `accueil.`.
   📌 Le transport de NKScena est `lecture.*` (celui de NkAnima reste `anim.*`) :
   les deux applications servent leurs propres actions.
4. **Les panneaux partagés nomment des actions** : quand `sequenceur.nkgui`
   commun contient `anim.inserer`, NKScena doit **servir** `anim.inserer` (poser
   une clé sur une caméra ou une lumière). Ne renomme pas ; **liste** les actions
   `anim.*` que NKScena doit servir (§6.1).

---

## 1. Le vocabulaire proposé en plus

P1 à P12 : voir doc 09 de NkAnima. NKScena en ajoute **trois**.

### P13 — `TimecodeField`

- **Propriétés** : `bind` (une image, entier) `fps` (référence ou nombre)
  `format` (`Timecode` HH:MM:SS:II · `Images` · `Secondes`).
- **Pourquoi** : le timecode est la langue du montage ; il s'affiche **et** se
  saisit (« +1:00 » avance d'une seconde, « 3-040 » va au plan). Un
  `NumberField` affiche un entier, pas un timecode, et ne comprend pas ces
  saisies.
- **Événement** : `Changed`, `Submit`.
- **En attendant** : `TextField` avec `bind` et un `placeholder: "00:00:00:00"` ;
  l'application formate.

### P14 — `Meter`

- **Propriétés** : `bind` (niveau, dB) `peak` (niveau crête) `min` (−60) `max`
  (+6) `channels` (1, 2, 6) `orientation`.
- **Zones** : vert jusqu'à −12 dB, ambre jusqu'à −3 dB, **rouge au-delà de 0 dB
  (`@son.ecretage`)**, trait de crête qui retombe lentement, **voyant
  d'écrêtage** qui reste allumé jusqu'au clic.
- **Pourquoi** : `Progress` n'a ni échelle logarithmique, ni zones, ni crête, ni
  voyant ; et l'écrêtage est **de l'information** (on ne l'entend pas toujours).
- **En attendant** : `Progress` avec `bind` ; le voyant en `Badge` (P3).

### P15 — `ColorWheel`

- **Propriétés** : `bind` (une couleur de décalage, RVB) `luminance` (bind d'un
  curseur associé) `label` (« Lift », « Gamma », « Gain »).
- **Pourquoi** : les roues d'étalonnage — un point qu'on déplace dans un disque de
  teintes, et une luminance à côté. `ColorField` choisit **une** couleur ; une roue
  **décale** une couleur autour du neutre, et se remet au centre d'un double clic.
- **Événement** : `Changed`, `DoubleClick` (retour au neutre).
- **En attendant** : `Host` avec `hint` + un `Slider` de luminance.

---

## 2. Les jetons propres à NKScena

À ajouter au thème « UE5 Rihen » (doc 09 de NkAnima §2), **sans rien retirer**.

| jeton | Sombre | où | pourquoi |
|---|---|---|---|
| `@info.antenne` | `#E5243B` | liseré de la Vue caméra active ; clip sous la tête de lecture dans la timeline de montage | ce qui passe à l'image |
| `@etat.a_faire` | `#8B949E` | `PastilleEtat` | avancement du plan |
| `@etat.en_cours` | `#2F81F7` | idem | idem |
| `@etat.a_revoir` | `#D29922` | idem | idem |
| `@etat.valide` | `#3FB950` | idem | idem |
| `@info.perime` | `#D29922` + `Hatch` (P11) | clip de montage, vignette de prise, travail de rendu | le rendu ne montre plus la scène |
| `@rendu.temps_reel` | `#2EC4D6` | `PastilleRendu` « TR » | mode de rendu du plan |
| `@rendu.precalcule` | `#7A4FD0` | `PastilleRendu` « PC » | idem |
| `@info.budget_ok` | `#3FB950` | texte de l'estimation | dans le budget |
| `@info.budget_depasse` | `#F85149` | texte de l'estimation | hors budget |
| `@son.ecretage` | `#F85149` | zone haute et voyant des `Meter` | le son sature |
| `@marqueur.1` … `@marqueur.8` | 8 teintes | marqueurs de montage | **choisis par l'utilisateur** |

📌 Les couleurs d'**état** et de **mode de rendu** portent aussi un **signe**
(○ ◐ ! ✓ ; TR / PC) : c'est dans le composant, pas à la charge de l'instance.

---

## 3. Les composants

### 3.1 Communs (dans `Commun/composants.nkgui`)

Ceux du doc 09 de NkAnima, **inchangés** : `BoutonOutil`, `BoutonTransport`,
`BoutonEcrit`, `EnTetePanneau`, `Categorie`, `LigneNombre`, `LigneVec3`,
`LigneListe`, `LigneCase`, `LigneCouleur`, `LigneCourbe`, `ChampVec3`,
`PastilleOrigine`, `CartePropo`, `TuileAction`, `CurseurParlant`,
`ModuleEffet`.

### 3.2 Propres à NKScena (dans `NKScena/composants_scena.nkgui`)

#### `PastilleEtat`

```
component "PastilleEtat"
  Badge✚ "pastille"       text: "○"   tone: etat.a_faire   tooltip: "À faire"
```
Instances types : `○ / etat.a_faire / « À faire »`, `◐ / etat.en_cours / « En
cours »`, `! / etat.a_revoir / « À revoir »`, `✓ / etat.valide / « Validé »`.

#### `PastilleRendu`

```
component "PastilleRendu"
  Button "film.mode_rendu_plan"   label: "TR"   tooltip: "Rendu temps réel — cliquer pour passer en précalculé"
    appearance  radius: 2   fill: #2EC4D6 // @rendu.temps_reel   text: #0A0A0A
```
Cliquable : **bascule** le mode du plan. Instance « PC » : `fill #7A4FD0 //
@rendu.precalcule`, `text #FFFFFF`.
⚠️ **Bascule** = non idempotent : c'est l'**action** du bouton qui bascule,
jamais un `behavior` rejoué à chaque image.

#### `FilAriane`

```
component "FilAriane"
  HBox "fil"                           gap: 2   align: Center
    Dropdown "film.sequence_courante"  bind: film.sequence   (items remplis par l'application)
    Text "sep1"                        text: "▸"
    Dropdown "film.plan_courant"       bind: film.plan
    Text "sep2"                        text: "▸"
    Dropdown "film.prise_courante"     bind: film.prise
    PastilleEtat "etat"
    PastilleRendu "mode"
```
Employé **une fois** aujourd'hui (barre d'outils), mais c'est un composant parce
que le moniteur source du montage montrera le même fil.

#### `TrancheMixeur`

```
component "TrancheMixeur"
  VBox "tranche"                       gap: 4   minSize: (64, 0)
    Text "nom"                         text: "?"   align: Center
    Host "inserts"                     hint: "effets insérés (égaliseur, compresseur…)"
    Slider "pan"                       bind: ?.pan   min: -1   max: 1   tooltip: "Panoramique"
    HBox "boutons"                     gap: 2
      ToggleButton✚ "sourdine"         label: "M"   bind: ?.sourdine   tooltip: "Sourdine"
      ToggleButton✚ "solo"             label: "S"   bind: ?.solo       tooltip: "Solo"
      ToggleButton✚ "armer"            label: "●"   bind: ?.armee      tooltip: "Armer pour l'enregistrement"
    HBox "niveau"                      gap: 2
      Slider "fader"                   bind: ?.volume   min: -60   max: 12   orientation: Vertical
      Meter✚ "vu"                      bind: ?.niveau   peak: ?.crete   channels: 2   orientation: Vertical
    KeyDiamond✚ "cle"                  bind: ?.volume
```
`armer` actif : `@info.enregistrement`. `sourdine` actif : `@info.alerte`.
`solo` actif : `@accent`.
⚠️ `orientation` n'existe pas sur `Slider` dans `nkgui 0.3` : écris-le, il sera
compté comme non honoré — signale-le, un fader vertical est indispensable.

#### `RoueEtalonnage`

```
component "RoueEtalonnage"
  VBox "roue"                          gap: 2   align: Center
    Text "titre"                       text: "?"
    ColorWheel✚ "disque"               bind: ?.couleur
    Slider "luminance"                 bind: ?.luminance   min: -1   max: 1
    Button "etal.neutre"               icon: rotate-ccw   tooltip: "Revenir au neutre"
```
Instances : `etal.lift`, `etal.gamma`, `etal.gain`, `etal.decalage`.

#### `VignettePrise`

```
component "VignettePrise"
  Tile✚ "tuile"                        image: "?"   label: "?"   caption: "?"   size: (128, 72)
```
Plus, **en surimpression** (dessinées par l'application dans la vignette tant que
`Stack` n'est pas monté) : `PastilleEtat`, `PastilleRendu`, et les hachures
`@info.perime`.

---

## 4. Les fichiers

### 4.1 Communs — `Resources/Interface/Commun/`

| fichier | origine |
|---|---|
| `composants.nkgui` | déplacé depuis NkAnima |
| `vue_3d.nkgui`, `scene.nkgui`, `details.nkgui`, `bibliotheque.nkgui`, `sequenceur.nkgui`, `assistant_ia.nkgui` | écrits pour NkAnima (doc 09 §5.4–5.8, §5.13), **déplacés** ici |

⚠️ Les filtres de `scene.nkgui` (personnages, objets, phénomènes, agents,
trajectoires) sont ceux de NkAnima. NKScena a besoin de filtres de plus (décor,
lumières, caméras). **Solution** : les filtres sortent de `scene.nkgui` dans un
`Host "scene.filtres"`, et chaque application monte son document de filtres
(`NkAnimaEditor/scene_filtres.nkgui`, `NKScena/scene_filtres.nkgui`).

### 4.2 Propres — `Resources/Interface/NKScena/`

| fichier | § |
|---|---|
| `composants_scena.nkgui` | §3.2 |
| `menus.nkgui` | §5.1 |
| `barre_outils.nkgui` | §5.2 |
| `barre_etat.nkgui` | §5.3 |
| `vue_camera.nkgui` | §5.4 |
| `scene_filtres.nkgui` | §5.5 |
| `details_camera.nkgui`, `details_lumiere.nkgui`, `details_ciel.nkgui`, `details_acteur.nkgui`, `details_dispersion.nkgui`, `details_plan.nkgui`, `details_clip_montage.nkgui` | §5.6 |
| `texte.nkgui`, `decoupage.nkgui`, `storyboard.nkgui` | §5.7 |
| `liste_plans.nkgui` | §5.8 |
| `liens_lumiere.nkgui` | §5.9 |
| `montage.nkgui`, `moniteurs.nkgui`, `chutier.nkgui` | §5.10 |
| `mixeur.nkgui` | §5.11 |
| `etalonnage.nkgui` | §5.12 |
| `compositing.nkgui` | §5.13 |
| `rendu.nkgui` | §5.14 |
| `accueil.nkgui` | §5.15 |
| `dispositions.nkgui` | §5.16 |

---

## 5. Les arbres

Notation, rôles ✚ et composants : comme le doc 09 de NkAnima.

### 5.1 `menus.nkgui`

```
MenuBar "menus"
  Menu "menu_fichier"                         label: "Fichier"
    MenuItem "fichier.nouveau"                label: "Nouveau film"           shortcut: "Ctrl+N"
    MenuItem "fichier.ouvrir"                 label: "Ouvrir…"                shortcut: "Ctrl+O"
    Menu "menu_recents"                       label: "Récents"                (rempli par l'application)
    Separator "sep_f1"
    MenuItem "fichier.enregistrer"            label: "Enregistrer"            shortcut: "Ctrl+S"
    MenuItem "fichier.enregistrer_sous"       label: "Enregistrer sous…"      shortcut: "Ctrl+Maj+S"
    Separator "sep_f2"
    Menu "menu_importer"                      label: "Importer"
      MenuItem "fichier.importer_scene"       label: "Scène Noge…"
      MenuItem "fichier.importer_decor"       label: "Décor (NKCraft, FBX, glTF)…"
      MenuItem "fichier.importer_acteur"      label: "Acteur NkAnima…"
      MenuItem "fichier.importer_audio"       label: "Audio…"
      MenuItem "fichier.importer_video"       label: "Vidéo de référence…"
      MenuItem "fichier.importer_texte"       label: "Texte / scénario…"
      MenuItem "fichier.importer_lut"         label: "LUT (.cube)…"
      MenuItem "fichier.importer_soustitres"  label: "Sous-titres (SRT)…"
    Menu "menu_exporter"                      label: "Exporter"
      MenuItem "fichier.exporter_film"        label: "Film…"
      MenuItem "fichier.exporter_sequence"    label: "Séquence…"
      MenuItem "fichier.exporter_plan"        label: "Plan…"
      MenuItem "fichier.exporter_audio"       label: "Audio (WAV)…"
      MenuItem "fichier.exporter_soustitres"  label: "Sous-titres (SRT)…"
      MenuItem "fichier.exporter_cinematique" label: "Cinématique Noge…"
    Separator "sep_f3"
    MenuItem "fichier.quitter"                label: "Quitter"                shortcut: "Ctrl+Q"

  Menu "menu_edition"                         label: "Édition"
    MenuItem "anim.annuler"                   label: "Annuler"                shortcut: "Ctrl+Z"
    MenuItem "anim.refaire"                   label: "Refaire"                shortcut: "Ctrl+Y"
    MenuItem "edition.historique"             label: "Historique"
    Separator "sep_e1"
    MenuItem "anim.copier"                    label: "Copier"                 shortcut: "Ctrl+C"
    MenuItem "anim.coller"                    label: "Coller"                 shortcut: "Ctrl+V"
    MenuItem "edition.dupliquer"              label: "Dupliquer"              shortcut: "Ctrl+D"
    MenuItem "edition.supprimer"              label: "Supprimer"              shortcut: "Suppr"
    MenuItem "edition.renommer"               label: "Renommer"               shortcut: "F2"
    Separator "sep_e2"
    MenuItem "edition.preferences"            label: "Préférences…"           shortcut: "Ctrl+,"

  Menu "menu_film"                            label: "Film"
    MenuItem "film.nouvelle_sequence"         label: "Nouvelle séquence"
    MenuItem "film.nouveau_plan"              label: "Nouveau plan"
    MenuItem "film.nouvelle_prise"            label: "Nouvelle prise"
    Menu "menu_etat"                          label: "État du plan"
      MenuItem "film.etat_a_faire"            label: "○ À faire"
      MenuItem "film.etat_en_cours"           label: "◐ En cours"
      MenuItem "film.etat_a_revoir"           label: "! À revoir"
      MenuItem "film.etat_valide"             label: "✓ Validé"
    Menu "menu_mode_rendu"                    label: "Mode de rendu du plan"
      MenuItem "film.mode_temps_reel"         label: "Temps réel"
      MenuItem "film.mode_precalcule"         label: "Précalculé"
      MenuItem "film.mode_projet"             label: "Comme le projet"
    MenuItem "film.reglages"                  label: "Réglages du film…"

  Menu "menu_plan"                            label: "Plan"
    MenuItem "plan.ajouter_camera"            label: "Ajouter une caméra"
    MenuItem "plan.ajouter_acteur"            label: "Ajouter un acteur…"
    MenuItem "outil.marque"                   label: "Marque au sol"          shortcut: "M"
    MenuItem "plan.ouvrir_nkanima"            label: "Ouvrir dans NkAnima"
    MenuItem "plan.duree"                     label: "Rallonger / raccourcir…"
    Separator "sep_p1"
    MenuItem "plan.precedent"                 label: "Plan précédent"         shortcut: "Page préc."
    MenuItem "plan.suivant"                   label: "Plan suivant"           shortcut: "Page suiv."

  Menu "menu_camera"                          label: "Caméra"
    MenuItem "cam.vue_camera"                 label: "Vue caméra"             shortcut: "0"
    MenuItem "cam.depuis_vue"                 label: "Caméra depuis la vue"   shortcut: "Ctrl+0"
    Menu "menu_map"                           label: "Mise au point"
      MenuItem "cam.map_point"                label: "Sur le point cliqué"    shortcut: "Maj+F"
      MenuItem "cam.map_suivre"               label: "Suivre la sélection"
      MenuItem "cam.map_bascule"              label: "Bascule vers la sélection…"
    Menu "menu_rig"                           label: "Rig"
      MenuItem "cam.rig_aucun"                label: "Aucun"
      MenuItem "cam.rig_grue"                 label: "Grue"
      MenuItem "cam.rig_rail"                 label: "Travelling sur rail"
      MenuItem "cam.rig_steadicam"            label: "Steadicam"
      MenuItem "cam.rig_epaule"               label: "Épaule"
      MenuItem "cam.rig_drone"                label: "Drone"
      MenuItem "cam.rig_orbite"               label: "Orbite"
    Menu "menu_format"                        label: "Format"
      MenuItem "cam.format_185"               label: "1,85"
      MenuItem "cam.format_239"               label: "2,39"
      MenuItem "cam.format_169"               label: "16/9"
      MenuItem "cam.format_43"                label: "4/3"
    Menu "menu_guides"                        label: "Guides"
      MenuItem "cam.guide_tiers"              label: "Tiers"                  checked: true
      MenuItem "cam.guide_centre"             label: "Centre"
      MenuItem "cam.guide_securite"           label: "Zone de sécurité titres"
      MenuItem "cam.guide_horizon"            label: "Horizon"
      MenuItem "cam.guide_storyboard"         label: "Planche de storyboard"  checked: true
    MenuItem "cam.virtuelle"                  label: "Caméra virtuelle…"

  Menu "menu_lumiere"                         label: "Lumière"
    Menu "menu_ajouter_lum"                   label: "Ajouter"
      MenuItem "lum.ajouter_soleil"           label: "Soleil"
      MenuItem "lum.ajouter_ponctuelle"       label: "Ponctuelle"
      MenuItem "lum.ajouter_spot"             label: "Spot"
      MenuItem "lum.ajouter_surface"          label: "Surfacique"
      MenuItem "lum.ajouter_ciel"             label: "Ciel (IBL)"
    Menu "menu_prereglages_lum"               label: "Préréglages"
      MenuItem "lum.pre_trois_points"         label: "Trois points"
      MenuItem "lum.pre_contre_jour"          label: "Contre-jour"
      MenuItem "lum.pre_heure_doree"          label: "Heure dorée"
      MenuItem "lum.pre_nuit_bleue"           label: "Nuit bleue"
      MenuItem "lum.pre_tungstene"            label: "Intérieur tungstène"
    MenuItem "fenetre.liens_lumiere"          label: "Liens de lumière"
    MenuItem "lum.solo"                       label: "Solo de la lumière sélectionnée"
    MenuItem "decor.ciel"                     label: "Ciel et atmosphère"

  Menu "menu_montage"                         label: "Montage"
    MenuItem "montage.inserer"                label: "Insérer"                shortcut: ","
    MenuItem "montage.ecraser"                label: "Écraser"                shortcut: "."
    MenuItem "montage.rasoir"                 label: "Rasoir"                 shortcut: "C"
    Menu "menu_transition"                    label: "Transition"
      MenuItem "montage.fondu"                label: "Fondu enchaîné"
      MenuItem "montage.fondu_noir"           label: "Fondu au noir"
      MenuItem "montage.volet"                label: "Volet"
    MenuItem "montage.vitesse"                label: "Vitesse…"
    MenuItem "montage.marqueur"               label: "Marqueur"               shortcut: "M"
    Menu "menu_versions"                      label: "Versions"               (rempli par l'application)
    MenuItem "montage.nouvelle_version"       label: "Nouvelle version"
    MenuItem "montage.comparer"               label: "Comparer deux versions…"
    MenuItem "montage.rendre_perimes"         label: "Rendre à nouveau les plans périmés"

  Menu "menu_son"                             label: "Son"
    MenuItem "son.ajouter_piste"              label: "Ajouter une piste"
    MenuItem "son.enregistrer"                label: "Enregistrer"
    Menu "menu_bus"                           label: "Bus"
      MenuItem "son.bus_ajouter"              label: "Ajouter un bus"
    Menu "menu_effets_son"                    label: "Effets"
      MenuItem "son.fx_egaliseur"             label: "Égaliseur"
      MenuItem "son.fx_compresseur"           label: "Compresseur"
      MenuItem "son.fx_reverb"                label: "Réverbération"
    MenuItem "son.spatialiser"                label: "Attacher à la sélection"
    MenuItem "son.normaliser"                 label: "Normaliser"

  Menu "menu_image"                           label: "Image"
    Menu "menu_etal"                          label: "Étalonnage"
      MenuItem "etal.roues"                   label: "Roues"
      MenuItem "etal.courbes"                 label: "Courbes"
      MenuItem "etal.lut"                     label: "LUT…"
      MenuItem "etal.correspondance"          label: "Correspondance de plans"
    Menu "menu_scopes"                        label: "Scopes"
      MenuItem "etal.scope_onde"              label: "Forme d'onde"
      MenuItem "etal.scope_vecteur"           label: "Vecteurscope"
      MenuItem "etal.scope_histo"             label: "Histogramme"
    Menu "menu_effets_image"                  label: "Effets image"
      MenuItem "etal.fx_bloom"                label: "Lueur (bloom)"
      MenuItem "etal.fx_grain"                label: "Grain de film"
      MenuItem "etal.fx_vignettage"           label: "Vignettage"
      MenuItem "etal.fx_aberration"           label: "Aberration chromatique"
      MenuItem "etal.fx_reflets"              label: "Reflets d'objectif"
    Menu "menu_titres"                        label: "Titres"
      MenuItem "titre.carton"                 label: "Carton"
      MenuItem "titre.anime"                  label: "Titre animé"
      MenuItem "titre.generique"              label: "Générique défilant"
      MenuItem "titre.filigrane"              label: "Filigrane de version"
    Menu "menu_soustitres"                    label: "Sous-titres"
      MenuItem "titre.soustitres_piste"       label: "Ajouter une piste de sous-titres"
    MenuItem "espace.compositing"             label: "Compositing"

  Menu "menu_rendu"                           label: "Rendu"
    MenuItem "rendu.plan"                     label: "Rendre le plan"         shortcut: "Ctrl+R"
    Menu "menu_file"                          label: "Ajouter à la file"
      MenuItem "rendu.file_plan"              label: "Le plan"
      MenuItem "rendu.file_sequence"          label: "La séquence"
      MenuItem "rendu.file_film"              label: "Le film (version courante)"
    MenuItem "rendu.estimer"                  label: "Estimer"
    MenuItem "rendu.budget"                   label: "Budget…"
    MenuItem "espace.rendu"                   label: "File de rendu"          shortcut: "Ctrl+Maj+R"
    MenuItem "rendu.historique"               label: "Historique des rendus"

  Menu "menu_ia"                              label: "IA"
    MenuItem "ia.assistant"                   label: "Assistant"              shortcut: "Ctrl+K"
    MenuItem "ia.decouper"                    label: "Découper le texte"
    MenuItem "ia.storyboard"                  label: "Storyboard du plan"
    MenuItem "ia.cadrer"                      label: "Cadrer…"
    MenuItem "ia.eclairer"                    label: "Éclairer…"
    MenuItem "ia.premier_montage"             label: "Premier montage"
    MenuItem "ia.etalonner_reference"         label: "Étalonner d'après une image…"
    MenuItem "ia.voix_provisoire"             label: "Voix provisoire"
    MenuItem "ia.budget_rendu"                label: "Budget de rendu"

  Menu "menu_fenetre"                         label: "Fenêtre"
    Menu "menu_espaces"                       label: "Espaces de travail"
      MenuItem "espace.preparation"           label: "Préparation"
      MenuItem "espace.decor"                 label: "Décor"
      MenuItem "espace.lumiere"               label: "Lumière"
      MenuItem "espace.plan"                  label: "Plan"
      MenuItem "espace.montage"               label: "Montage"
      MenuItem "espace.son"                   label: "Son"
      MenuItem "espace.etalonnage"            label: "Étalonnage"
      MenuItem "espace.compositing"           label: "Compositing"
      MenuItem "espace.rendu"                 label: "Rendu"
    Separator "sep_w1"
    MenuItem "fenetre.scene"                  label: "Scène"
    MenuItem "fenetre.details"                label: "Détails"
    MenuItem "fenetre.sequenceur"             label: "Séquenceur du plan"
    MenuItem "biblio.ouvrir"                  label: "Bibliothèque"           shortcut: "Ctrl+Espace"
    MenuItem "fenetre.liste_plans"            label: "Liste des plans"
    MenuItem "fenetre.liens_lumiere"          label: "Liens de lumière"
    MenuItem "fenetre.mixeur"                 label: "Mixeur"
    MenuItem "fenetre.scopes"                 label: "Scopes"
    MenuItem "fenetre.journal"                label: "Journal"
    Separator "sep_w2"
    MenuItem "fenetre.reinitialiser"          label: "Réinitialiser la disposition"

  Menu "menu_aide"                            label: "Aide"
    MenuItem "anim.aide_raccourcis"           label: "Raccourcis"
    MenuItem "aide.documentation"             label: "Documentation"
    MenuItem "anim.apropos"                   label: "Crédits et licences / À propos"
```
⚠️ **Identifiants employés deux fois dans ce menu** : `espace.compositing`
(Image et Fenêtre), `espace.rendu` (Rendu et Fenêtre), `fenetre.liens_lumiere`
(Lumière et Fenêtre), et `outil.marque` / `montage.marqueur` partagent le
raccourci **M** (contexte différent, voir doc 02 §6). Si le monteur refuse deux
identifiants égaux dans un document, suffixe la seconde occurrence (`…_menu`) et
signale-le.

### 5.2 `barre_outils.nkgui`

```
HBox "barre_outils"                           gap: 4   align: Center
  Dropdown "espace.choix"                     bind: espace.actif
                                              items: ["Préparation","Décor","Lumière","Plan","Montage","Son","Étalonnage","Compositing","Rendu"]
  Separator "sep_espace"                      orientation: Vertical
  FilAriane "film.fil"
  Separator "sep_fil"                         orientation: Vertical

  HBox "outils"                               gap: 2
    BoutonOutil "outil.selection"             icon: mouse-pointer-2   tooltip: "Sélectionner (Q)"
    BoutonOutil "outil.deplacer"              icon: move              tooltip: "Déplacer (W)"
    BoutonOutil "outil.tourner"               icon: rotate-3d         tooltip: "Tourner (E)"
    BoutonOutil "outil.echelle"               icon: scaling           tooltip: "Échelle (R)"
    BoutonOutil "outil.marque"                icon: map-pin           tooltip: "Marque au sol (M)"
    BoutonOutil "outil.mise_au_point"         icon: focus             tooltip: "Mise au point (Maj+F)"
    BoutonOutil "outil.dispersion"            icon: sprout            tooltip: "Pinceau de dispersion"   visible: false
  Separator "sep_outils"                      orientation: Vertical

  HBox "transport"                            gap: 2
    BoutonTransport "lecture.debut"           label: "|<"   icon: skip-back
    BoutonTransport "lecture.image_prec"      label: "<"    icon: chevron-left
    BoutonTransport "lecture.jouer"           label: "Jouer / Pause"   icon: play
    BoutonTransport "lecture.image_suiv"      label: ">"    icon: chevron-right
    BoutonTransport "lecture.fin"             label: ">|"   icon: skip-forward
    ToggleButton✚ "lecture.boucle"            icon: repeat   bind: lecture.boucle   tooltip: "Jouer en boucle"
    TimecodeField✚ "lecture.timecode"         bind: lecture.image   fps: film.ips   format: Timecode
  Separator "sep_transport"                   orientation: Vertical

  HBox "rendu"                                gap: 4
    Button "rendu.plan"                       icon: clapperboard   tooltip: "Rendre le plan (Ctrl+R)"
    Button "espace.rendu"                     icon: timer   label: "⏱ —"   tooltip: "Estimation du rendu — ouvrir la file"
  Spacer "pousse"

  HBox "droite"                               gap: 4
    ToggleButton✚ "cam.vue_camera"            icon: video   bind: cam.vue_camera   tooltip: "Vue caméra (0)"
    Button "ia.assistant"                     icon: sparkles   tooltip: "Assistant IA (Ctrl+K)"
    Badge✚ "ia.en_attente"                    text: "0"   tone: info.apercu   tooltip: "Propositions en attente"
```
**Couleurs** : `espace.rendu` (label) → `text @info.budget_ok` ou
`@info.budget_depasse` selon `rendu.dans_budget` ; `cam.vue_camera` actif →
`@info.antenne`.
**Comportements idempotents** ✅ :
```
outil.dispersion.visible = (espace.actif == "Décor")
espace.rendu.label       = "⏱ " + rendu.estimation_texte
ia.en_attente.visible    = (ia.nb_en_attente > 0)
```
📌 `espace.rendu` est un **bouton qui change d'espace**, et son libellé porte
l'estimation : l'identifiant reste celui de l'action.

### 5.3 `barre_etat.nkgui`

```
HBox "barre_etat"                             gap: 8   align: Center
  Button "biblio.ouvrir"                      icon: library   label: "Bibliothèque"
  Button "fenetre.journal"                    icon: scroll-text   label: "Journal"
  TextField "commande"                        placeholder: "> commande"   bind: commande.saisie
  Spacer "pousse"
  Text "etat.ips"                             text: "24 ips"
  Text "etat.plan"                            text: "—"
  Text "etat.rendu"                           text: "Rendu : au repos"
  Progress "etat.progression"                 bind: rendu.progression   visible: false
```
`etat.progression.visible = rendu.en_cours` — idempotent ✅.

### 5.4 `vue_camera.nkgui`

Monté **dans** la Vue commune (`vue_3d.nkgui`) quand `cam.vue_camera` est vrai.

```
Panel "vue_camera"                            placement: absolute
  Host "cam.cadre"                            hint: "cadre du format, bandes assombries, guides, planche de storyboard, plan net, liseré antenne (dessinés par l'application)"   sizeRel: (1, 1)
  HBox "cam.barre"                            pos: (8, 8)   gap: 2
    Dropdown "cam.active"                     bind: plan.camera_active   (items : les caméras du plan)
    Dropdown "cam.focale_rapide"              bind: cam.focale   items: ["18 mm","24 mm","35 mm","50 mm","85 mm","135 mm"]
    Dropdown "cam.format"                     bind: cam.format   items: ["1,85","2,39","16/9","4/3"]
    Dropdown "cam.apercu"                     bind: vue.apercu   items: ["Éditeur","Temps réel final","Précalculé progressif"]
    Slider "cam.opacite_planche"              bind: cam.opacite_planche   min: 0   max: 1   tooltip: "Opacité de la planche"
```
`Précalculé progressif` : **grisé** avec sa raison (« Disponible au palier P4 »)
tant que le précalculé n'existe pas → `enabled` porté par l'application.

### 5.5 `scene_filtres.nkgui`

```
HBox "scene.filtres"                          gap: 2
  ToggleButton✚ "scene.filtre_decor"          icon: mountain        bind: scene.filtre.decor     tooltip: "Décor"
  ToggleButton✚ "scene.filtre_lumiere"        icon: lightbulb       bind: scene.filtre.lumiere   tooltip: "Lumières"
  ToggleButton✚ "scene.filtre_camera"         icon: video           bind: scene.filtre.camera    tooltip: "Caméras"
  ToggleButton✚ "scene.filtre_acteur"         icon: person-standing bind: scene.filtre.acteur    tooltip: "Acteurs"
  ToggleButton✚ "scene.filtre_fx"             icon: wind            bind: scene.filtre.fx        tooltip: "Effets"
  ToggleButton✚ "scene.filtre_son"            icon: volume-2        bind: scene.filtre.son       tooltip: "Sons"
```

### 5.6 Détails par type de sélection

Montés dans `details.contenu` (commun). Chaque document : `VBox "racine"` de
`Categorie`.

#### `details_camera.nkgui`

```
VBox "racine"
  Categorie "cat_transformation"              label: "Transformation"
    LigneVec3 "cam.position"                  etiquette: "Position"   bind: cam.position
    LigneVec3 "cam.rotation"                  etiquette: "Rotation"   bind: cam.rotation
  Categorie "cat_objectif"                    label: "Objectif"
    LigneNombre "cam.focale"                  etiquette: "Focale (mm)"   bind: cam.focale   min: 8   max: 300
    LigneListe "cam.capteur"                  etiquette: "Capteur"   bind: cam.capteur   items: ["Super 35","Plein format","Imax","Personnalisé"]
    LigneListe "cam.format_img"               etiquette: "Format"    bind: cam.format    items: ["1,85","2,39","16/9","4/3"]
  Categorie "cat_map"                         label: "Mise au point"
    LigneNombre "cam.distance_map"            etiquette: "Distance (m)"   bind: cam.distance_map
    LigneListe "cam.cible_map"                etiquette: "Suivre"    bind: cam.cible_map   (items : éléments de la scène)
    LigneNombre "cam.ouverture"               etiquette: "Ouverture (f/)"   bind: cam.ouverture   min: 1.4   max: 22
    LigneCase "cam.pdc"                       etiquette: "Profondeur de champ"   bind: cam.pdc
  Categorie "cat_obturateur"                  label: "Obturateur"
    LigneNombre "cam.angle_obturateur"        etiquette: "Angle (°)"   bind: cam.angle_obturateur   min: 0   max: 360
    LigneCase "cam.flou_mouvement"            etiquette: "Flou de mouvement"   bind: cam.flou_mouvement
  Categorie "cat_rig"                         label: "Rig"
    LigneListe "cam.rig"                      etiquette: "Type"   bind: cam.rig   items: ["Aucun","Grue","Rail","Steadicam","Épaule","Drone","Orbite"]
    LigneNombre "cam.tremblement"             etiquette: "Tremblement"   bind: cam.tremblement   min: 0   max: 1
    LigneNombre "cam.amortissement"           etiquette: "Amortissement"   bind: cam.amortissement   min: 0   max: 1
  Categorie "cat_visee"                       label: "Visée"
    LigneListe "cam.cible"                    etiquette: "Regarder"   bind: cam.cible
    LigneVec3 "cam.decalage"                  etiquette: "Décalage"   bind: cam.decalage
```
`cam.tremblement.visible = (cam.rig == "Épaule" or cam.rig == "Steadicam")`,
`cam.decalage.visible = (cam.cible != "")` — idempotents ✅.
`cam.capteur`, `cam.format_img`, `cam.rig` ne s'animent pas → `cle.visible = false`.

#### `details_lumiere.nkgui`

`Transformation` · `Lumière` (Type ; **Température (K)** `LigneNombre` 1 000–12 000 ;
Couleur `LigneCouleur` ; Intensité ; Unité `LigneListe` [Lux, Lumens, Artistique] ;
Taille) · `Ombres` (Actives ; Douceur ; Résolution) · `Volumétrie` (Active ;
Densité) · `Liens` (`Button "fenetre.liens_lumiere"` « Modifier les liens… »).
Intensité, température, couleur, taille : **animables**.

#### `details_ciel.nkgui`

`Lieu et heure` (Latitude ; Longitude ; Date ; **Heure** — animable) · `Ciel`
(Modèle `LigneListe` [Hosek-Wilkie, Prague] ; Turbidité ; Nuages ; Couverture) ·
`Atmosphère` (Brouillard ; Densité ; Brume hauteur ; Volumétrie).

#### `details_acteur.nkgui`

```
VBox "racine"
  Categorie "cat_acteur"                      label: "Acteur"
    LigneListe "acteur.personnage"            etiquette: "Personnage"   bind: acteur.personnage
    Host "acteur.clips"                       hint: "clips assignés sur la piste de l'acteur"
    HBox "acteur.boutons"                     gap: 4
      Button "plan.ouvrir_nkanima"            label: "Ouvrir dans NkAnima"   icon: external-link
      Button "acteur.recaler_marque"          label: "Recaler sur la marque"   icon: map-pin
  Categorie "cat_marques"                     label: "Marques au sol"
    Host "acteur.marques"                     hint: "marques de départ et d'arrivée"
  Categorie "cat_lien"                        label: "Lien avec NkAnima"
    Text "acteur.lien_etat"                   text: "À jour"
```
`acteur.lien_etat.text` = « À jour » / « Ouvert dans NkAnima » / « Modifié —
recharger » ; **couleur** `@info.alerte` si ouvert ou modifié — idempotent ✅.

#### `details_dispersion.nkgui`

`Palette` (Host : éléments de la palette ; `Button "decor.palette_ajouter"`) ·
`Pinceau` (Rayon ; Densité ; Échelle min / max ; Orientation aléatoire) ·
`Exclusion` (Distance aux chemins ; Pente maximale) ·
`Button "decor.gomme"`. Aucune ligne animable → `cle.visible = false` partout.

#### `details_plan.nkgui` (le plan lui-même, sélectionné dans la Liste des plans)

```
VBox "racine"
  Categorie "cat_plan"                        label: "Plan"
    TextField "plan.numero"                   bind: plan.numero
    LigneListe "plan.etat"                    etiquette: "État"   bind: plan.etat   items: ["À faire","En cours","À revoir","Validé"]
    LigneListe "plan.mode_rendu"              etiquette: "Rendu"  bind: plan.mode_rendu   items: ["Comme le projet","Temps réel","Précalculé"]
    LigneNombre "plan.duree"                  etiquette: "Durée (images)"   bind: plan.duree
  Categorie "cat_decoupage"                   label: "Découpage"
    LigneListe "plan.valeur"                  etiquette: "Valeur de cadre"   bind: plan.valeur   items: ["Très gros plan","Gros plan","Plan rapproché","Plan américain","Plan moyen","Plan d'ensemble","Plan général"]
    LigneListe "plan.angle"                   etiquette: "Angle"   bind: plan.angle   items: ["Normal","Plongée","Contre-plongée","Vue du dessus","Débullé"]
    LigneListe "plan.mouvement"               etiquette: "Mouvement"   bind: plan.mouvement   items: ["Fixe","Panoramique","Travelling","Grue","Épaule","Zoom"]
    TextField "plan.note"                     multiline: true   bind: plan.note   placeholder: "Note de réalisation…"
  Categorie "cat_prises"                      label: "Prises"
    Host "plan.prises"                        hint: "prises du plan, la prise retenue marquée"
    Button "film.nouvelle_prise"              label: "Nouvelle prise"   icon: copy-plus
```
Aucune ligne animable → `cle.visible = false`.

#### `details_clip_montage.nkgui` (un clip sélectionné dans la timeline de montage)

`Clip` (Prise `LigneListe` ; Entrée / Sortie `TimecodeField✚` ; Vitesse
`LigneNombre` en % ; `Button "montage.ouvrir_plan"` « Ouvrir le plan ») ·
`Rendu` (état : à jour / **périmé** ; `Button "montage.rendre_clip"` « Rendre à
nouveau ») · `Transition` (entrée, sortie ; durée).
**Couleur** : le texte « périmé » en `@info.perime` — idempotent ✅.

### 5.7 Préparation

#### `texte.nkgui`

```
Panel "texte"                                 title: "Texte"
  VBox "colonne"                              gap: 4
    HBox "texte.barre"                        gap: 4
      Dropdown "texte.type_ligne"             bind: texte.type   items: ["Intitulé","Action","Personnage","Dialogue","Didascalie","Transition"]
      Button "fichier.importer_texte"         label: "Importer…"   icon: file-text
      Button "ia.decouper"                    label: "Découper par IA"   icon: sparkles
    Host "texte.editeur"                      hint: "éditeur de scénario : types de lignes, personnages et lieux reconnus (rempli par l'application)"
```
📌 **L'éditeur de scénario est un `Host`**, pas un `TextField` multiligne : les
lignes ont un **type** qui change leur mise en page et leur comportement au
clavier (Tab / Entrée). Ce n'est pas un texte brut.

#### `decoupage.nkgui`

```
Panel "decoupage"                             title: "Découpage"
  VBox "colonne"                              gap: 4
    EnTetePanneau "dec.entete"
    HBox "dec.barre"                          gap: 4
      Button "film.nouveau_plan"              label: "Nouveau plan"   icon: plus
      Button "ia.decouper"                    label: "Découper par IA"   icon: sparkles
    Table "dec.tableau"                       columns: ["N°","Séq.","État","Valeur","Angle","Mouvement","Durée","Personnages","Lieu","Note"]   (non monté : attendu)
    Host "dec.tableau_hote"                   hint: "le tableau des plans tant que Table n'est pas monté"
```

#### `storyboard.nkgui`

```
Panel "storyboard"                            title: "Storyboard"
  VBox "colonne"                              gap: 4
    HBox "sb.barre"                           gap: 4
      Button "prep.planche_nouvelle"          label: "Nouvelle planche"   icon: plus
      Button "prep.planche_capturer"          label: "Capturer la vue 3D"   icon: camera
      Button "prep.planche_importer"          label: "Importer une image…"   icon: image
      Button "ia.storyboard"                  label: "Proposer par IA"   icon: sparkles
      Spacer "pousse"
      Button "prep.animatique"                label: "Générer l'animatique"   icon: film
      Button "prep.planche_vers_plan"         label: "Planche → Plan 3D"   icon: box
    Scroll "sb.defil"                         axis: Vertical
      Flow "sb.grille"                        gap: 8     (VignettePrise par planche, ajoutées par l'application)
    Host "sb.dessin"                          hint: "planche en grand : pinceau, gomme, calques, flèches de mouvement"   visible: false
```
`sb.dessin.visible = prep.planche_ouverte` et `sb.defil.visible = not
prep.planche_ouverte` — idempotents ✅.

### 5.8 `liste_plans.nkgui`

```
Panel "liste_plans"                           title: "Plans"
  VBox "colonne"                              gap: 4
    HBox "lp.barre"                           gap: 2
      Button "film.nouveau_plan"              icon: plus   tooltip: "Nouveau plan"
      Button "film.nouvelle_prise"            icon: copy-plus   tooltip: "Nouvelle prise"
      Button "plan.precedent"                 icon: chevron-left   tooltip: "Plan précédent (Page préc.)"
      Button "plan.suivant"                   icon: chevron-right  tooltip: "Plan suivant (Page suiv.)"
    Scroll "lp.defil"                         axis: Horizontal
      HBox "lp.bande"                         gap: 4     (VignettePrise par plan : état, TR/PC, périmé)
```

### 5.9 `liens_lumiere.nkgui`

```
Panel "liens_lumiere"                         title: "Liens de lumière"
  VBox "colonne"                              gap: 4
    EnTetePanneau "ll.entete"
    Table "ll.tableau"                        columns: ["Élément", "…une colonne par lumière"]   (non monté : attendu)
    Host "ll.tableau_hote"                    hint: "lumières × éléments, cases à cocher"
```

### 5.10 Montage

#### `moniteurs.nkgui`

```
Splitter "moniteurs"                          orientation: Vertical   ratio: 0.5
  Panel "mon.source"                          title: "Source"
    VBox "src.colonne"                        gap: 2
      Host "src.image"                        hint: "la prise choisie dans le chutier"
      HBox "src.barre"                        gap: 2   align: Center
        Button "montage.src_jouer"            icon: play
        TimecodeField✚ "montage.src_tc"       bind: montage.src_image
        Button "montage.entree"               label: "Entrée"   icon: bracket-left    tooltip: "I"
        Button "montage.sortie"               label: "Sortie"   icon: bracket-right   tooltip: "O"
        Button "montage.inserer"              label: "Insérer"  tooltip: ","
        Button "montage.ecraser"              label: "Écraser"  tooltip: "."
  Panel "mon.programme"                       title: "Programme"
    VBox "prg.colonne"                        gap: 2
      Host "prg.image"                        hint: "le film à la tête de lecture ; liseré antenne"
      HBox "prg.barre"                        gap: 2   align: Center
        BoutonTransport "lecture.jouer"       label: "Jouer / Pause"   icon: play
        TimecodeField✚ "montage.prg_tc"       bind: lecture.image
```
⚠️ `bracket-left` / `bracket-right` ne sont peut-être pas des icônes Lucide
existantes : prends l'icône la plus proche et signale-le.

#### `chutier.nkgui`

```
Panel "chutier"                               title: "Chutier"
  VBox "colonne"                              gap: 4
    EnTetePanneau "ch.entete"
    Host "ch.arbre"                           hint: "Séquence › Plan › Prise"
    Scroll "ch.defil"                         axis: Vertical
      Flow "ch.grille"                        gap: 6     (VignettePrise par prise)
```

#### `montage.nkgui` (la timeline)

```
Panel "montage"                               title: "Montage"
  VBox "colonne"                              gap: 0
    HBox "mt.barre"                           gap: 4   align: Center
      Dropdown "montage.version"              bind: montage.version_courante   (items : versions)
      Button "montage.nouvelle_version"       icon: git-branch-plus   tooltip: "Nouvelle version"
      Separator "mt.sep1"                     orientation: Vertical
      BoutonOutil "outil.mt_selection"        icon: mouse-pointer-2   tooltip: "Sélection (V)"    group: outils_montage
      BoutonOutil "outil.mt_rasoir"           icon: scissors          tooltip: "Rasoir (C)"       group: outils_montage
      BoutonOutil "outil.mt_trim"             icon: move-horizontal   tooltip: "Trim (T)"         group: outils_montage
      BoutonOutil "outil.mt_glisser"          icon: arrow-left-right  tooltip: "Glisser (Y)"      group: outils_montage
      Separator "mt.sep2"                     orientation: Vertical
      ToggleButton✚ "montage.aimant"          icon: magnet   bind: montage.aimant   tooltip: "Aimantation"
      Button "montage.marqueur"               icon: bookmark   tooltip: "Marqueur (M)"
      Spacer "pousse"
      Button "montage.rendre_perimes"         label: "Rendre les périmés"   icon: refresh-cw
      Button "montage.comparer"               icon: columns-2   tooltip: "Comparer deux versions"
    Splitter "mt.partage"                     orientation: Vertical   ratio: 0.14
      Host "mt.pistes"                        hint: "pistes V1, V2, A1…, sous-titres : nom, sourdine, verrou"
      Host "mt.canevas"                       hint: "règle en timecode, clips avec vignettes, état périmé hachuré, marqueurs, clip sous la tête en antenne"
```
📌 `BoutonOutil` porte `group: outils_vue` dans le composant : ici l'instance
**surcharge** `group` en `outils_montage`. Si la surcharge d'un attribut de la
racine ne descend pas jusqu'au `ToggleButton` interne, dis-le.

### 5.11 `mixeur.nkgui`

```
Panel "mixeur"                                title: "Mixeur"
  HBox "mx.barre"                             gap: 4
    Button "son.ajouter_piste"                label: "Piste"   icon: plus
    Button "son.bus_ajouter"                  label: "Bus"     icon: git-merge
    ToggleButton✚ "son.enregistrer"           icon: circle   bind: son.enregistre   tooltip: "Enregistrer sur les pistes armées"
  Scroll "mx.defil"                           axis: Horizontal
    HBox "mx.tranches"                        gap: 2     (TrancheMixeur par piste, puis par bus ; ajoutées par l'application)
  Separator "mx.sep"                          orientation: Vertical
  TrancheMixeur "son.master"                  nom: "Master"
```
`son.enregistrer` actif → `@info.enregistrement`.

### 5.12 `etalonnage.nkgui`

```
Panel "etalonnage"                            title: "Étalonnage"
  VBox "colonne"                              gap: 6
    HBox "et.niveau"                          gap: 4
      RadioGroup "etal.niveau"                bind: etal.niveau   options: ["Séquence","Plan"]   orientation: Horizontal
      ToggleButton✚ "etal.avant_apres"        icon: split-square-horizontal   bind: etal.avant_apres   tooltip: "Avant / après"
      Button "etal.neutre_tout"               label: "Tout remettre à zéro"   icon: rotate-ccw
    HBox "et.roues"                           gap: 12   justify: Center
      RoueEtalonnage "etal.lift"              titre: "Lift"
      RoueEtalonnage "etal.gamma"             titre: "Gamma"
      RoueEtalonnage "etal.gain"              titre: "Gain"
      RoueEtalonnage "etal.decalage"          titre: "Décalage"
    Categorie "et.base"                       label: "Réglages de base"
      LigneNombre "etal.exposition"           etiquette: "Exposition"    bind: etal.exposition   min: -4   max: 4
      LigneNombre "etal.contraste"            etiquette: "Contraste"     bind: etal.contraste    min: 0    max: 2
      LigneNombre "etal.saturation"           etiquette: "Saturation"    bind: etal.saturation   min: 0    max: 2
      LigneNombre "etal.temperature"          etiquette: "Température"   bind: etal.temperature  min: -1   max: 1
      LigneNombre "etal.teinte"               etiquette: "Teinte"        bind: etal.teinte       min: -1   max: 1
    Categorie "et.courbes"                    label: "Courbes"   expanded: false
      Host "etal.courbes_hote"                hint: "courbes RVB et teinte/saturation"
    Categorie "et.lut"                        label: "LUT"
      LigneListe "etal.lut_choix"             etiquette: "LUT"   bind: etal.lut   (items : LUT du projet)
      LigneNombre "etal.lut_force"            etiquette: "Force"   bind: etal.lut_force   min: 0   max: 1
      Button "fichier.importer_lut"           label: "Importer…"   icon: download
    Categorie "et.looks"                      label: "Looks"
      Scroll "etal.looks_defil"               axis: Horizontal
        HBox "etal.looks"                     gap: 4     (Tile par look)
      HBox "etal.looks_boutons"               gap: 4
        Button "etal.look_enregistrer"        label: "Enregistrer le look"   icon: save
        Button "etal.correspondance"          label: "Correspondance"   icon: pipette
        Button "ia.etalonner_reference"       label: "D'après une image…"   icon: sparkles
```
Et deux panneaux voisins dans la disposition (§5.16) :

```
Panel "scopes"                                title: "Scopes"
  VBox "sc.colonne"
    TabBar "etal.scope"                       bind: etal.scope   tabs: ["Forme d'onde","Vecteurscope","Histogramme"]
    Host "sc.image"                           hint: "le scope choisi"

Panel "bande_plans"                           title: "Plans"
  Scroll "bp.defil"                           axis: Horizontal
    HBox "bp.bande"                           gap: 4     (VignettePrise par plan du montage ; sélection multiple)
```
Ces deux panneaux vont dans `etalonnage.nkgui` avec lui (un fichier = une
**page** ici, parce qu'ils n'existent pas hors de l'espace Étalonnage).

### 5.13 `compositing.nkgui`

```
Panel "compositing"                           title: "Compositing"
  Splitter "cp.partage"                       orientation: Horizontal   ratio: 0.45
    Host "comp.visionneuse"                   hint: "sortie du nœud sélectionné"
    VBox "cp.bas"                             gap: 0
      HBox "cp.barre"                         gap: 4
        Button "comp.ajouter_noeud"           label: "Ajouter ▾"   icon: plus
        Button "comp.cadrer"                  icon: scan   tooltip: "Cadrer le graphe (F)"
        Dropdown "comp.niveau"                bind: comp.niveau   items: ["Film","Séquence","Plan"]
      Host "comp.graphe"                      hint: "éditeur de nœuds NKGraph"

Panel "passes"                                title: "Passes"
  VBox "ps.colonne"                           gap: 2
    LigneCase "comp.passe_beaute"             etiquette: "Beauté"         bind: comp.passes.beaute
    LigneCase "comp.passe_diffus"             etiquette: "Diffus"         bind: comp.passes.diffus
    LigneCase "comp.passe_speculaire"         etiquette: "Spéculaire"     bind: comp.passes.speculaire
    LigneCase "comp.passe_emission"           etiquette: "Émission"       bind: comp.passes.emission
    LigneCase "comp.passe_profondeur"         etiquette: "Profondeur"     bind: comp.passes.profondeur
    LigneCase "comp.passe_normales"           etiquette: "Normales"       bind: comp.passes.normales
    LigneCase "comp.passe_vitesse"            etiquette: "Vitesse"        bind: comp.passes.vitesse
    LigneCase "comp.passe_id_objet"           etiquette: "ID objets"      bind: comp.passes.id_objet
    LigneCase "comp.passe_id_matiere"         etiquette: "ID matières"    bind: comp.passes.id_matiere
    LigneCase "comp.passe_occlusion"          etiquette: "Occlusion"      bind: comp.passes.occlusion
```
Les passes ne s'animent pas → `cle.visible = false`.
📌 **Le graphe est un `Host`** tant que NKGraph n'a pas de rôle dans `.nkgui` :
c'est le même cas que le graphe de comportement de NKUIDesign.

### 5.14 `rendu.nkgui`

```
Panel "rendu"                                 title: "Rendu"
  VBox "colonne"                              gap: 6
    Splitter "rd.haut"                        orientation: Vertical   ratio: 0.6
      VBox "rd.file"                          gap: 4
        Text "rd.file_titre"                  text: "File de rendu"
        Host "rendu.file"                     hint: "travaux : case, nom, TR/PC, estimation, progression, état ; glisser pour la priorité"
        HBox "rd.boutons"                     gap: 4
          Button "rendu.estimer"              label: "Estimer"   icon: timer
          Button "rendu.lancer"               label: "Lancer"    icon: play
          Button "rendu.pause"                label: "Pause"     icon: pause
          Button "rendu.reprendre"            label: "Reprendre" icon: step-forward
          Button "rendu.retirer"              icon: trash-2   tooltip: "Retirer de la file"
          Dropdown "rendu.debut"              bind: rendu.heure_debut   items: ["Maintenant","Ce soir 22:00","Demain 06:00","À une heure précise…"]
      VBox "rd.reglages"                      gap: 2
        Text "rd.reglages_titre"              text: "Réglages du travail"
        RadioGroup "rendu.mode"               bind: rendu.mode   options: ["Temps réel","Précalculé"]   orientation: Horizontal
        LigneListe "rendu.prereglage"         etiquette: "Préréglage"   bind: rendu.prereglage   items: ["Brouillon","Revue","Temps réel final","Précalculé moyen","Précalculé maximal"]
        LigneListe "rendu.resolution"         etiquette: "Résolution"   bind: rendu.resolution   items: ["720p","1080p","2K","4K"]
        LigneNombre "rendu.ips"               etiquette: "Images / s"   bind: rendu.ips
        LigneListe "rendu.plage"              etiquette: "Plage"        bind: rendu.plage   items: ["Tout","Plage de lecture","Personnalisée"]
        LigneListe "rendu.format"             etiquette: "Sortie"       bind: rendu.format   items: ["MP4 (H.264)","MOV","WebM","Suite PNG","Suite EXR","Suite JPEG"]
        HBox "rd.dossier"                     gap: 4
          TextField "rendu.dossier"           bind: rendu.dossier
          Button "rendu.choisir_dossier"      icon: folder-open
        Categorie "rd.qualite"                label: "Qualité (précalculé)"   expanded: false
          LigneNombre "rendu.echantillons"    etiquette: "Échantillons / pixel"   bind: rendu.echantillons   min: 1   max: 4096
          LigneCase "rendu.pdc_vraie"         etiquette: "Profondeur de champ par accumulation"   bind: rendu.pdc_vraie
          LigneCase "rendu.flou_vrai"         etiquette: "Flou de mouvement par accumulation"   bind: rendu.flou_vrai
          LigneListe "rendu.ombres"           etiquette: "Ombres"   bind: rendu.ombres   items: ["Moyennes","Hautes","Maximales"]
          LigneCase "rendu.debruiteur"        etiquette: "Débruiteur"   bind: rendu.debruiteur
    Separator "rd.sep"
    HBox "rd.budget"                          gap: 6   align: Center
      Text "rd.estimation_titre"              text: "Estimation"
      Spacer "pousse"
      Text "rd.budget_titre"                  text: "Budget"
      TextField "rendu.budget_duree"          bind: rendu.budget   placeholder: "8 h 00"
      Button "ia.budget_rendu"                label: "Proposer des réglages"   icon: sparkles
    Host "rendu.estimation_tableau"           hint: "plan · images · s/image · total · marge · écart du dernier rendu ; ligne TOTAL"
    Text "rendu.total"                        text: "TOTAL —"
    Expander "rd.historique"                  label: "Historique"   expanded: false
      Host "rendu.historique_liste"           hint: "rendus passés : estimé, réel, écart"
    VBox "rd.propositions"                    gap: 6     (CartePropo : réglages proposés pour tenir le budget)
```
- `rd.qualite.visible = (rendu.mode == "Précalculé")` — idempotent ✅.
- `rendu.total.text.color = @info.budget_ok` ou `@info.budget_depasse` —
  idempotent ✅.
- **Tant que le précalculé n'existe pas (P1–P3)** : l'option « Précalculé » de
  `rendu.mode` et les préréglages « Précalculé … » sont **grisés** avec la raison
  « Disponible au palier P4 ». Écris-les quand même.
- Les lignes de réglages de rendu ne s'animent pas → `cle.visible = false`.

### 5.15 `accueil.nkgui`

Comme celui de NkAnima (doc 09 §5.18), avec **Nouveau** remplacé par un
formulaire :

```
VBox "accueil.nouveau"                        gap: 6   visible: false
  TextField "accueil.nom"                     bind: accueil.nom   placeholder: "Nom du film"
  HBox "accueil.dossier_ligne"                gap: 4
    TextField "accueil.dossier"               bind: accueil.dossier
    Button "accueil.choisir_dossier"          icon: folder-open
  LigneListe "accueil.ips"                    etiquette: "Images / s"   bind: accueil.ips   items: ["24","25","30","48","60"]
  LigneListe "accueil.format"                 etiquette: "Format"       bind: accueil.format   items: ["1,85","2,39","16/9"]
  LigneListe "accueil.resolution"             etiquette: "Résolution"   bind: accueil.resolution   items: ["1080p","2K","4K"]
  LigneListe "accueil.mode"                   etiquette: "Rendu par défaut"   bind: accueil.mode   items: ["Temps réel","Précalculé"]
  Button "accueil.creer"                      label: "Créer le film"   icon: clapperboard
```
Toutes les lignes : `cle.visible = false`.

### 5.16 `dispositions.nkgui`

```
layout "preparation"
  dock "texte" left 0.30 · dock "storyboard" center · dock "decoupage" right 0.30 · dock "montage" bottom 0.28
layout "decor"
  dock "vue" center · dock "scene" left 0.16 · dock "details" right 0.22 · dock "bibliotheque" bottom 0.28
layout "lumiere"
  dock "vue" center · dock "scene" left 0.16 · dock "details" right 0.22 · dock "liens_lumiere" bottom 0.25
layout "plan"
  dock "vue" center · dock "liste_plans" top 0.12 · dock "scene" left 0.16 · dock "details" right 0.22 · dock "sequenceur" bottom 0.28
layout "montage"
  dock "moniteurs" top 0.45 · dock "chutier" right 0.22 · dock "montage" bottom 0.55
layout "son"
  dock "montage" top 0.50 · dock "mixeur" bottom 0.50 · dock "details" right 0.20
layout "etalonnage"
  dock "vue" center · dock "bande_plans" top 0.12 · dock "etalonnage" bottom 0.40 · dock "scopes" right 0.24
layout "compositing"
  dock "compositing" center · dock "passes" right 0.18 · dock "details" right-bottom 0.50
layout "rendu"
  dock "rendu" center · dock "vue" right 0.35
```
Dans `etalonnage`, `vue` est en **Vue caméra, aperçu Temps réel final** — c'est la
visionneuse.

---

## 6. Les actions

Légende : ✅ servie aujourd'hui · ✚ à créer (grisée avec sa raison) ·
🟠 écrit une clé (`@info.ecrit`).

### 6.1 Actions `anim.*` que NKScena doit servir (panneaux communs)

`anim.annuler` · `anim.refaire` · `anim.copier` · `anim.coller` 🟠 ·
`anim.inserer` 🟠 · `anim.supprimer` · `anim.cle_prec` · `anim.cle_suiv` ·
`anim.cle_auto` · `commun.cle_propriete` 🟠 · `commun.reinitialiser_propriete` ·
`anim.aide_raccourcis` · `anim.apropos` — toutes ✚ **dans NKScena** (servies par
NkAnimaEditor, pas encore par NKScena).

### 6.2 Actions propres — toutes ✚

| domaine | actions |
|---|---|
| **lecture** | `debut` · `image_prec` · `jouer` · `image_suiv` · `fin` · `boucle` |
| **fichier** | `nouveau` · `ouvrir` · `enregistrer` · `enregistrer_sous` · `importer_scene` · `importer_decor` · `importer_acteur` · `importer_audio` · `importer_video` · `importer_texte` · `importer_lut` · `importer_soustitres` · `exporter_film` · `exporter_sequence` · `exporter_plan` · `exporter_audio` · `exporter_soustitres` · `exporter_cinematique` · `quitter` |
| **edition** | `historique` · `dupliquer` · `supprimer` · `renommer` · `preferences` |
| **film** | `nouvelle_sequence` · `nouveau_plan` · `nouvelle_prise` · `etat_a_faire` · `etat_en_cours` · `etat_a_revoir` · `etat_valide` · `mode_temps_reel` · `mode_precalcule` · `mode_projet` · `mode_rendu_plan` · `reglages` |
| **plan** | `ajouter_camera` · `ajouter_acteur` · `ouvrir_nkanima` · `duree` · `precedent` · `suivant` |
| **acteur** | `recaler_marque` |
| **cam** | `vue_camera` · `depuis_vue` · `map_point` · `map_suivre` · `map_bascule` · `rig_aucun` · `rig_grue` · `rig_rail` · `rig_steadicam` · `rig_epaule` · `rig_drone` · `rig_orbite` · `format_185` · `format_239` · `format_169` · `format_43` · `guide_tiers` · `guide_centre` · `guide_securite` · `guide_horizon` · `guide_storyboard` · `virtuelle` |
| **lum** | `ajouter_soleil` · `ajouter_ponctuelle` · `ajouter_spot` · `ajouter_surface` · `ajouter_ciel` · `pre_trois_points` · `pre_contre_jour` · `pre_heure_doree` · `pre_nuit_bleue` · `pre_tungstene` · `solo` |
| **decor** | `ciel` · `palette_ajouter` · `gomme` |
| **prep** | `planche_nouvelle` · `planche_capturer` · `planche_importer` · `animatique` · `planche_vers_plan` |
| **montage** | `inserer` · `ecraser` · `rasoir` · `fondu` · `fondu_noir` · `volet` · `vitesse` · `marqueur` · `nouvelle_version` · `aimant` · `comparer` · `rendre_perimes` · `rendre_clip` · `ouvrir_plan` · `entree` · `sortie` · `src_jouer` |
| **son** | `ajouter_piste` · `enregistrer` · `bus_ajouter` · `fx_egaliseur` · `fx_compresseur` · `fx_reverb` · `spatialiser` · `normaliser` |
| **etal** | `roues` · `courbes` · `lut` · `correspondance` · `scope_onde` · `scope_vecteur` · `scope_histo` · `fx_bloom` · `fx_grain` · `fx_vignettage` · `fx_aberration` · `fx_reflets` · `neutre` · `neutre_tout` · `avant_apres` · `look_enregistrer` |
| **titre** | `carton` · `anime` · `generique` · `filigrane` · `soustitres_piste` |
| **comp** | `ajouter_noeud` · `cadrer` |
| **rendu** | `plan` · `file_plan` · `file_sequence` · `file_film` · `estimer` · `budget` · `historique` · `lancer` · `pause` · `reprendre` · `retirer` · `choisir_dossier` |
| **ia** | `assistant` · `decouper` · `storyboard` · `cadrer` · `eclairer` · `premier_montage` · `etalonner_reference` · `voix_provisoire` · `budget_rendu` (+ `proposer` · `apercu` · `appliquer` 🟠 · `rejeter` du panneau commun) |
| **outil** | `selection` · `deplacer` · `tourner` · `echelle` · `marque` · `mise_au_point` · `dispersion` · `mt_selection` · `mt_rasoir` · `mt_trim` · `mt_glisser` |
| **espace** | `preparation` · `decor` · `lumiere` · `plan` · `montage` · `son` · `etalonnage` · `compositing` · `rendu` |
| **scene** | `filtre_decor` · `filtre_lumiere` · `filtre_camera` · `filtre_acteur` · `filtre_fx` · `filtre_son` |
| **biblio** | `ouvrir` |
| **fenetre** | `scene` · `details` · `sequenceur` · `liste_plans` · `liens_lumiere` · `mixeur` · `scopes` · `journal` · `reinitialiser` |
| **aide** | `documentation` |
| **accueil** | `choisir_dossier` · `creer` |

### 6.3 Raisons de grisé

| cas | raison |
|---|---|
| action ✚ non servie | « Pas encore disponible dans NKScena » |
| tout ce qui est **précalculé** (P1–P3) | « Disponible au palier P4 — pour un début, temps réel » |
| `cam.virtuelle`, lancer de rayons | « Disponible au palier P6 » |
| `plan.*`, `cam.*` sans plan ouvert | « Ouvre d'abord un plan » |
| `montage.rendre_perimes` sans plan périmé | « Aucun plan périmé » |
| `rendu.reprendre` sans rendu interrompu | « Aucun rendu interrompu » |
| `plan.ouvrir_nkanima` si NkAnima est introuvable | « NkAnima n'est pas installé » |

---

## 7. Ce que tu ne sais pas encore reproduire — écrit quand même

### 7.1 États

Tous ceux du doc 09 de NkAnima §7.1, plus :

| élément | état | rendu attendu |
|---|---|---|
| `ToggleButton "cam.vue_camera"` | `Active` | `@info.antenne` |
| `ToggleButton "sourdine"` (mixeur) | `Active` | `@info.alerte` |
| `ToggleButton "armer"`, `"son.enregistrer"` | `Active` | `@info.enregistrement` |
| `VignettePrise` | `Stale` (périmé) | hachures `@info.perime` |
| `VignettePrise` | `OnAir` | liseré `@info.antenne` |
| `Meter` | `Clipping` | voyant `@son.ecretage` allumé |

### 7.2 Animations

| élément | animation |
|---|---|
| `Progress "etat.progression"` | avance douce |
| `Text "rendu.total"` | fondu de couleur quand il passe de dans à hors budget |
| `Meter` | retombée de la crête (1,5 s) |
| `VignettePrise` | lecture de la prise au survol |

### 7.3 Événements (lot b)

| élément | événement | effet | idempotent ? |
|---|---|---|---|
| `lecture.timecode` | `on Submit` | aller à l'image / au plan saisi | ❌ |
| `PastilleRendu` | `on Click` | basculer TR/PC | ❌ — porté par l'action |
| `VignettePrise` | `on Click` / `on DoubleClick` / `on DragStart` | choisir / ouvrir le plan / glisser vers la timeline | ❌ |
| `RoueEtalonnage.disque` | `on DoubleClick` | retour au neutre | ❌ |
| `rendu.budget_duree` | `on Submit` | recalculer « dans le budget » | ✅ possible en idempotent (comparaison rejouable) |
| `espace.choix` | `on Changed` | appliquer la disposition | ❌ |

### 7.4 Polices, contours, ombres

Comme NkAnima (doc 09 §7.4), plus **`"Mono-12"`** pour tous les **timecodes**
(les chiffres ne doivent pas danser pendant la lecture).

---

## 8. L'ordre de livraison proposé

Aligné sur le phasage du document 01 (**pour un début, temps réel**).

| # | livrable | palier | témoin |
|---|---|---|---|
| 1 | `Commun/` créé (`include` est déjà branché) ; NkAnima repassé sur `Commun/` **sans régression** | — | les compteurs de NkAnimaEditor **identiques** avant / après le déménagement |
| 2 | `composants_scena` + `menus` + `barre_outils` + `barre_etat` | P1 | `actionsInconnues` = exactement les ✚ attendues |
| 3 | `vue_camera` + `scene_filtres` + `details_camera` + `details_acteur` + `details_plan` + `liste_plans` + séquenceur commun | P1 | un plan s'ouvre, sa caméra se règle, le séquenceur montre ses pistes |
| 4 | `montage` + `moniteurs` + `chutier` + `details_clip_montage` + `mixeur` | P2 | un clip périmé se peint hachuré (témoin par couleur nommée `@info.perime`) |
| 5 | `details_lumiere` + `details_ciel` + `details_dispersion` + `liens_lumiere` + `etalonnage` | P3 | les quatre `RoueEtalonnage` comptées, 4 instances, 0 copie |
| 6 | `rendu` (le mode Précalculé grisé) | P1 → P4 | l'estimation affichée ; en P4 l'écart publié |
| 7 | `compositing` | P4 | |
| 8 | `texte` + `decoupage` + `storyboard` + `accueil` | P5 | |
| 9 | `dispositions` complet | au fil de l'eau | 9 dispositions lues, 0 ignorée |

📌 Comme pour NkAnima : **après l'étape 3, publie `apparencesNonPeintes` et
`etatsNonAppliques`** pour NKScena.
