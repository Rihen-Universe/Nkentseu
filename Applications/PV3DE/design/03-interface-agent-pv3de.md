# PV3DE — l'interface, pour l'agent qui écrit les `.nkgui`

### Document 3 — Spécification d'interface côté agent

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> Traduction du **02** en tes quatre points : **arbre nommé**, **composants**,
> **actions**, **couleurs porteuses** — plus ce que tu ne sais pas encore
> reproduire, écrit quand même. **Si 02 et 03 se contredisent, 02 gagne.**
>
> **Prérequis** : NkAnima 09, NKScena 03, Nogee 06, NKCraft 03 (notation, P1–P20,
> `Commun/`, lignes engendrées, actions `commun.`).

---

## 0. Ce qui est propre à PV3DE

1. **Deux visages** : la **salle** (apprenant) et l'**atelier** (formateur). La
   salle **n'est pas un éditeur** : pas de `DockSpace` visible, des panneaux qui
   apparaissent selon l'étape. L'atelier suit la famille (panneaux amarrables,
   Détails à catégories).
2. **La règle de la salle** : *le patient n'est jamais caché.* Dans les
   dispositions (§5.16), la vue du patient ne partage l'écran qu'avec des
   panneaux **latéraux** ou **bas**, jamais superposés au centre.
3. **La règle pédagogique** : *rien ne donne la réponse pendant la séance.* Les
   jetons `@grille.*` n'apparaissent **que** dans `debriefing.nkgui` ; le corrigé
   (diagnostic différentiel) **que** dans `debriefing.nkgui`, `edcas_corrige.nkgui`
   et le mode démonstration. **Si un document de la salle référence un de ces
   jetons ou une de ces données, c'est une faute** — refuse-la et signale-la.
4. **L'existant** : PV3DE a une interface NKGui codée (`UI/PvGui.h`,
   `Layers/MedicalUILayer`, `Panels/` : `SymptomInput`, `DiagnosticPanel`,
   `PatientStatePanel`, `ReportPanel`, portés NKUI → NKGui le 18/08). **Aucun
   document `.nkgui`.** Ces quatre panneaux deviennent des outils **du formateur**
   (décision D3 du doc 01) : `SymptomInput` et `DiagnosticPanel` → éditeur de cas
   (§5.13) ; `PatientStatePanel` → mode démonstration ; `ReportPanel` → export du
   débriefing.
5. **Actions de PV3DE**, par domaine : `accueil.` `profil.` `cas.` `seance.`
   `etape.` `parole.` `comm.` `examen.` `installer.` `decouvrir.` `consent.`
   `dossier.` `bilan.` `hypo.` `presc.` `synthese.` `bloc.` `debrief.` `edcas.`
   `suivi.` `demo.` `reglages.` + communs (`ia.`, `commun.`, `fenetre.`, `aide.`).
6. **Deux langues** : **tous les libellés** sont écrits en français dans les
   documents et passent par la **table de traduction** de l'application (le
   mécanisme de langue commun de la famille — UI_SPEC de NKCraft §6). Ne duplique
   pas un document par langue.

---

## 1. Le vocabulaire proposé en plus

### P21 — `HoldButton`

- **Quoi** : un bouton qui agit **tant qu'on le maintient** ; événements
  `Press` et `Release` ; propriété `key` (une touche qui fait la même chose
  maintenue, ex. `Espace`).
- **Pourquoi** : **« Maintenir pour parler »**. Un `Button` n'a qu'un clic ; un
  `ToggleButton` obligerait à cliquer deux fois et laisserait le micro ouvert si
  l'on oublie.
- **États** : repos · **enregistrement** (fond `@info.enregistrement`, pulsation) ·
  **traitement** (la transcription se fait).
- **En attendant** : `ToggleButton` ; l'application coupe le micro après un
  silence.

### P22 — `Stepper`

- **Quoi** : une suite d'étapes numérotées, horizontale ou verticale ;
  propriétés `steps` (liste), `bind` (étape courante), `states` (liste :
  `Done`, `Current`, `Todo`, `Hidden`), `free` (booléen : cliquer une étape y va).
- **Pourquoi** : la **barre des étapes** de la séance (libre) et le **fil des étapes**
  d'une intervention au bloc (vertical, avec des étapes **cachées** en Examen). Un
  `TabBar` n'a ni états « faite / à venir », ni étapes masquées.
- **En attendant** : `TabBar` (`bind`, `tabs`) ; les états en `appearance(État)`.

---

## 2. Les jetons propres à PV3DE

| jeton | Sombre | où **seulement** | pourquoi |
|---|---|---|---|
| `@moniteur.fc` | `#3FB950` | valeurs et courbe du moniteur | convention des moniteurs |
| `@moniteur.spo2` | `#2EC4D6` | idem | idem |
| `@moniteur.pa` | `#F85149` | idem | idem |
| `@moniteur.fr` | `#E8C33A` | idem | idem |
| `@moniteur.temp` | `#E6E6E6` | idem | idem |
| `@alarme.haute` / `@alarme.moyenne` | `#F85149` / `#E8C33A` | cadre d'une valeur en alarme | l'apprenant doit réagir |
| `@etape.faite` · `@etape.courante` · `@etape.a_venir` | `#3FB950` à 60 % · `@accent` · `#6E7681` | `Stepper` | se repérer ; « faite » = **touchée**, pas réussie |
| `@grille.fait` · `@grille.partiel` · `@grille.manque` · `@grille.critique` | `#3FB950` · `#D29922` · `#6E7681` · `#F85149` | **`debriefing.nkgui` uniquement** | évaluation |
| `@cas.valide` · `@cas.relecture` · `@cas.brouillon` | `#3FB950` · `#D29922` · `#6E7681` | cartes de cas, éditeur | un cas non validé ne sort pas en Examen |
| `@consent.manque` | `#D29922` | carte de consentement | l'oubli se voit — c'est un moment d'apprentissage, pas une faute cachée |

---

## 3. Les composants — `PV3DE/composants_pv3de.nkgui`

Plus les communs (`Commun/Composants.nkgui`).

#### `CarteCas`

```
component "CarteCas"
  Tile✚ "tuile"                         image: "?"   label: "?"   caption: "?"   size: (220, 132)
```
- `label` = **le motif de consultation** (« Douleur thoracique »), **jamais le
  diagnostic**.
- `caption` = « Homme, 54 ans · Urgences · 2ᵉ cycle · 30 min ».
- Pastille de statut (`Badge` : ✓ `cas.valide` / ◐ `cas.relecture` / — `cas.brouillon`)
  dessinée par l'application dans la vignette (`Stack` non monté). **Côté
  apprenant**, seuls les cas **validés** (ou marqués « entraînement » par le
  formateur) sont listés.

#### `BoutonInstrument`

```
component "BoutonInstrument"
  ToggleButton✚ "bouton"                icon: "?"   label: "?"   group: trousse   tooltip: "?"
```
Un seul instrument en main à la fois (groupe exclusif) ; **Échap** le repose.

#### `ValeurMoniteur`

```
component "ValeurMoniteur"
  VBox "valeur"                         gap: 0   minSize: (96, 0)
    Text "nom"                          text: "?"          (« FC », « SpO₂ »…)
    Text "chiffre"                      text: "—"          font: "Mono-24"
    Text "unite"                        text: "?"          (« /min », « % », « mmHg »…)
```
Chaque instance écrit sa couleur : `text { color = #3FB950 } // @moniteur.fc` sur
`chiffre` (compté aujourd'hui : `text.color` **est** peint ✅). Le cadre d'alarme :
`appearance(Alarm) stroke 2 @alarme.haute` (compté, non peint).

#### `CarteConsentement`

```
component "CarteConsentement"
  Panel "carte"                         appearance: fill #2B2B2B   stroke-left 3 #D29922 // @consent.manque   radius: 2
    VBox "corps"                        gap: 4
      Text "message"                    text: "?"   wrap: true   (« Vous n'avez ni expliqué ni demandé — le patient hésite. »)
      HBox "actions"                    gap: 4   justify: End
        Button "consent.demander"       label: "Expliquer et demander"   icon: message-circle
        Button "consent.passer_outre"   label: "Continuer sans"          icon: arrow-right
```
📌 `consent.passer_outre` **est permis** : l'apprenant peut se tromper, c'est le
débriefing qui le lui dira. L'interface ne l'en empêche pas.

#### `SectionObservation`

```
component "SectionObservation"
  VBox "section"                        gap: 2
    Text "titre"                        text: "?"
    TextField "texte"                   multiline: true   wrap: true   bind: ?   placeholder: "?"
```

#### `BlocCompetence` (débriefing)

```
component "BlocCompetence"
  Grid "bloc"                           columns: 3   sizes: [0.45, 0.35, 0.20]   gap: 4
    Text "nom"                          text: "?"
    Progress "barre"                    bind: ?   
    Text "pourcent"                     text: "—"
```

#### `ItemGrille` (débriefing)

```
component "ItemGrille"
  HBox "item"                           gap: 6   align: Center
    Badge✚ "symbole"                    text: "?"   tone: grille.fait     (✓ fait · ◐ partiel · — manqué · ✕ critique)
    Text "libelle"                      text: "?"   wrap: true
    Spacer "pousse"
    Text "points"                       text: "?"
    Button "debrief.aller_au_moment"    icon: play   tooltip: "Revoir ce moment"
```

---

## 4. Les fichiers — `Resources/Interface/PV3DE/`

| fichier | § | public |
|---|---|---|
| `composants_pv3de.nkgui` | 3 | — |
| `accueil.nkgui` | 5.1 | tous |
| `accueil_apprenant.nkgui` | 5.2 | apprenant |
| `accueil_formateur.nkgui` | 5.3 | formateur |
| `salle.nkgui` | 5.4 | apprenant |
| `salle_parole.nkgui` | 5.5 | apprenant |
| `salle_trousse.nkgui` | 5.6 | apprenant |
| `moniteur.nkgui` | 5.7 | apprenant, formateur (démo) |
| `dossier.nkgui` + `dossier_conversation`, `_examen`, `_examens`, `_hypotheses`, `_prescriptions`, `_notes`, `_synthese` | 5.8 | apprenant |
| `bloc.nkgui` | 5.9 | apprenant |
| `fin_seance.nkgui` | 5.10 | apprenant |
| `debriefing.nkgui` | 5.11 | apprenant, formateur |
| `editeur_cas.nkgui` + `edcas_identite`, `_personnalite`, `_histoire`, `_examen`, `_bilans`, `_evolution`, `_bloc`, `_corrige`, `_grille`, `_validation` | 5.12–5.13 | formateur |
| `suivi.nkgui` | 5.14 | formateur |
| `demonstration.nkgui` | 5.15 | formateur |
| `reglages.nkgui` | 5.16 | tous |
| `dispositions.nkgui` | 5.17 | — |

---

## 5. Les arbres

### 5.1 `accueil.nkgui`

```
Window "accueil"                              title: "PV3DE"
  VBox "racine"                               gap: 12   align: Center
    HBox "acc.tete"                           gap: 8   align: Center
      Image "acc.logo"                        source: "logo_rihen"
      Spacer "pousse"
      RadioGroup "reglages.langue"            bind: reglages.langue   options: ["Français","English"]   orientation: Horizontal
    Text "acc.titre"                          text: "Patient virtuel 3D"
    HBox "acc.roles"                          gap: 16   justify: Center
      Tile✚ "accueil.apprenant"               image: "accueil/apprenant"   label: "Apprenant"   caption: "Soigner un patient virtuel"
      Tile✚ "accueil.formateur"               image: "accueil/formateur"   label: "Formateur"   caption: "Écrire des cas, suivre une promotion"
    HBox "acc.profil"                         gap: 6   align: Center
      Dropdown "profil.choix"                 bind: profil.courant   (items : profils locaux)
      Button "profil.nouveau"                 label: "Nouveau profil"   icon: user-plus
      TextField "profil.code"                 placeholder: "Code (facultatif)"   secret: true   bind: profil.code
    Text "acc.mention"                        text: "Simulation à but pédagogique — ne constitue pas un avis médical."   align: Center
```
`accueil.apprenant.enabled = accueil.formateur.enabled = (profil.courant != "")`
— idempotent ✅, raison : « Choisis ou crée un profil ».

### 5.2 `accueil_apprenant.nkgui`

```
Window "accueil_apprenant"                    title: "PV3DE — Apprenant"
  Splitter "aa.partage"                       orientation: Vertical   ratio: 0.2
    VBox "aa.nav"                             gap: 2
      ListBox "aa.section"                    bind: aa.section   items: ["Continuer","À faire","Bibliothèque","Mes progrès"]
      Spacer "pousse"
      Button "fenetre.reglages"               icon: settings   label: "Réglages"
      Button "accueil.changer_profil"         icon: log-out    label: "Changer de profil"
    VBox "aa.contenu"                         gap: 8
      HBox "aa.filtres"                       gap: 4   visible: false
        TextField "aa.recherche"              placeholder: "Rechercher un motif…"   icon✚: search   bind: aa.recherche
        Dropdown "aa.specialite"              bind: aa.specialite   (items : spécialités)
        Dropdown "aa.niveau"                  bind: aa.niveau   items: ["Tous","1ᵉʳ cycle","2ᵉ cycle","Internat"]
      Scroll "aa.defil"                       axis: Vertical
        Flow "aa.cas"                         gap: 8     (CarteCas par cas, ajoutées par l'application)
      Host "aa.progres"                       hint: "courbes par compétence, séances passées (cliquables → débriefing)"   visible: false
      HBox "aa.demarrer"                      gap: 8   align: Center   visible: false
        Text "aa.cas_choisi"                  text: "—"
        RadioGroup "seance.mode"              bind: seance.mode   options: ["Entraînement","Examen"]   orientation: Horizontal
        Button "seance.entrer"                label: "Entrer dans la salle"   icon: door-open
```
- `aa.filtres.visible = (aa.section == "Bibliothèque")` ; `aa.progres.visible =
  (aa.section == "Mes progrès")` ; `aa.defil.visible = (aa.section in ["Continuer","À faire","Bibliothèque"])` ;
  `aa.demarrer.visible = (cas.choisi != "")` — idempotents ✅.
- `seance.mode` : l'option **Examen** est grisée si le cas n'est pas **validé**
  (raison : « Ce cas n'est pas encore validé par un relecteur médical »).

### 5.3 `accueil_formateur.nkgui`

```
Window "accueil_formateur"                    title: "PV3DE — Formateur"
  Splitter "af.partage"                       orientation: Vertical   ratio: 0.2
    VBox "af.nav"                             gap: 2
      ListBox "af.section"                    bind: af.section   items: ["Mes cas","Relectures en attente","Promotions","Démonstration"]
      Spacer "pousse"
      Button "fenetre.reglages"               icon: settings   label: "Réglages"
      Button "accueil.changer_profil"         icon: log-out    label: "Changer de profil"
    VBox "af.contenu"                         gap: 8
      HBox "af.barre"                         gap: 4
        Button "cas.nouveau"                  label: "Nouveau cas"   icon: file-plus
        Button "cas.importer"                 label: "Importer…"     icon: download
        Button "cas.exporter"                 label: "Exporter…"     icon: upload
      Scroll "af.defil"                       axis: Vertical
        Flow "af.cas"                         gap: 8     (CarteCas, statut visible)
```

### 5.4 `salle.nkgui`

```
Window "salle"                                title: ""   flags: NoTitleBar
  VBox "racine"                               gap: 0
    HBox "salle.tete"                         gap: 8   align: Center
      Button "seance.quitter"                 icon: arrow-left   label: "Quitter"
      Stepper✚ "etape.choix"                  bind: seance.etape   free: true
                                              steps: ["Accueil","Interrogatoire","Examen","Examens","Hypothèses","Prise en charge","Bloc","Synthèse"]
      Spacer "pousse"
      Text "seance.temps"                     text: "00:00"      font: "Mono-12"
      Text "seance.temps_simule"              text: ""          font: "Mono-12"
      Button "seance.pause"                   icon: pause           tooltip: "Pause (P)"
      Button "seance.accelerer"               icon: fast-forward    tooltip: "Accélérer le temps"
      Button "seance.indice"                  icon: lightbulb   label: "Indice"   tooltip: "Compté au débriefing (H)"
      Button "seance.terminer"                label: "Terminer la séance"   icon: flag
    Splitter "salle.partage"                  orientation: Vertical   ratio: 0.78   min: 0.60
      VBox "salle.centre"                     gap: 0
        Host "salle.patient"                  hint: "LE PATIENT : vue 3D, sous-titres, bulles de résultats d'examen, contour de la région visée"
        Host "salle.bas"                      hint: "salle_parole ou salle_trousse selon l'étape"
      Host "salle.droite"                     hint: "moniteur (branché) ou dossier (ouvert)"
```
- **Comportements idempotents** ✅ :
  ```
  seance.pause.visible        = (seance.mode == "Entraînement")
  seance.accelerer.visible    = (seance.mode == "Entraînement")
  seance.indice.visible       = (seance.mode == "Entraînement")
  seance.temps_simule.visible = (seance.temps_simule != seance.temps)
  etape.choix : état « Bloc » = Hidden tant que le cas n'a pas de bloc
  ```
- En **Examen**, `seance.temps` est un **compte à rebours** et passe en
  `@info.alerte` sous 2 minutes.
- `salle.bas` : l'application monte `salle_trousse` quand `seance.etape ==
  "Examen"`, sinon `salle_parole`.
- ⚠️ **Rien ici ne dépend du corrigé.** L'état « faite » d'une étape = **touchée**.

### 5.5 `salle_parole.nkgui`

```
VBox "parole"                                 gap: 4
  HBox "parole.ligne"                         gap: 6   align: Center
    HoldButton✚ "parole.parler"               label: "Maintenir pour parler"   icon: mic   key: "Espace"
    TextField "parole.texte"                  placeholder: "Écrire une question au patient…"   bind: parole.saisie
    Button "parole.envoyer"                   icon: send   tooltip: "Envoyer (Entrée)"
    Button "dossier.ouvrir"                   icon: clipboard-list   label: "Dossier"   tooltip: "D"
  Flow "comm.actions"                         gap: 4
    Button "comm.se_presenter"                label: "Se présenter"
    Button "comm.expliquer"                   label: "Expliquer ce que je vais faire"
    Button "comm.consentement"                label: "Demander le consentement"
    Button "comm.rassurer"                    label: "Rassurer"
    Button "comm.reformuler"                  label: "Reformuler"
    Button "comm.annoncer"                    label: "Annoncer…"
```
- Les actions de communication **visibles** varient par étape (ex. « Annoncer »
  à partir de Hypothèses) : `comm.annoncer.visible = (seance.etape in [...])` ✅.
- ⚠️ Ces boutons **ne donnent pas de phrase toute faite** : ils **ouvrent** la saisie
  avec l'intention notée (« Expliquer : … ») — c'est l'apprenant qui formule. Le
  bouton sert à **déclarer l'intention** pour la grille ; la formulation reste la
  sienne.

### 5.6 `salle_trousse.nkgui`

```
VBox "trousse"                                gap: 4
  HBox "tr.instruments"                       gap: 4
    BoutonInstrument "examen.stethoscope"     icon: stethoscope   label: "Stéthoscope"
    BoutonInstrument "examen.palper"          icon: hand          label: "Palper"
    BoutonInstrument "examen.percuter"        icon: pointer       label: "Percuter"
    BoutonInstrument "examen.lampe"           icon: flashlight    label: "Lampe"
    BoutonInstrument "examen.marteau"         icon: hammer        label: "Marteau à réflexes"
    BoutonInstrument "examen.thermometre"     icon: thermometer   label: "Thermomètre"
    BoutonInstrument "examen.tensiometre"     icon: gauge         label: "Tensiomètre"
    BoutonInstrument "examen.oxymetre"        icon: activity      label: "Oxymètre"
    BoutonInstrument "examen.glycemie"        icon: droplet       label: "Glycémie"
    BoutonInstrument "examen.otoscope"        icon: ear           label: "Otoscope"
    BoutonInstrument "examen.abaisse_langue"  icon: minus         label: "Abaisse-langue"
    BoutonInstrument "examen.regarder"        icon: eye           label: "Inspecter"
    Button "moniteur.brancher"                icon: monitor       label: "Brancher le moniteur"
  HBox "tr.installation"                      gap: 4   align: Center
    Text "tr.installer_titre"                 text: "Installer :"
    Button "installer.allonge"                label: "Allongé"
    Button "installer.assis"                  label: "Assis"
    Button "installer.debout"                 label: "Debout"
    Button "installer.cote"                   label: "Sur le côté"
    Separator "tr.sep"                        orientation: Vertical
    Text "tr.decouvrir_titre"                 text: "Découvrir :"
    Dropdown "decouvrir.region"               bind: decouvrir.region   items: ["—","Thorax","Abdomen","Membres supérieurs","Membres inférieurs","Dos","Région génitale"]
    Button "decouvrir.recouvrir"              label: "Recouvrir"   icon: shirt
    Separator "tr.sep2"                       orientation: Vertical
    Button "examen.demander_respirer"         label: "« Respirez fort »"
    Button "examen.demander_tousser"          label: "« Toussez »"
  Host "tr.consentement"                      hint: "CarteConsentement quand un geste l'exige et qu'il n'a pas été demandé"
  HBox "tr.parole"                            gap: 4
    HoldButton✚ "parole.parler"               label: "Parler"   icon: mic   key: "Espace"
    Button "dossier.ouvrir"                   icon: clipboard-list   label: "Dossier"
```
- **On parle aussi pendant l'examen** (« Est-ce que ça fait mal ici ? ») : la
  parole reste accessible.
- `installer.debout.enabled` dépend de l'état du patient (**raison** : « Le
  patient ne tient pas debout ») — portée par l'application.
- `decouvrir.region = "Région génitale"` exige un consentement **explicite** et
  propose un **chaperon** (action `consent.chaperon`) : si non fait, la carte de
  consentement le dit.
- Les **régions du corps** (survol, contour, clic) sont dans `salle.patient` : le
  corps est un **Host**, pas un document.

### 5.7 `moniteur.nkgui`

```
Panel "moniteur"                              title: "Moniteur"
  VBox "colonne"                              gap: 4
    Text "mon.debranche"                      text: "Patient non branché."   visible: true
    VBox "mon.valeurs"                        gap: 6   visible: false
      ValeurMoniteur "mon.fc"                 nom: "FC"     unite: "/min"   (chiffre en #3FB950 // @moniteur.fc)
      Host "mon.courbe_ecg"                   hint: "tracé ECG défilant"
      ValeurMoniteur "mon.spo2"               nom: "SpO₂"   unite: "%"      (#2EC4D6 // @moniteur.spo2)
      Host "mon.courbe_pleth"                 hint: "courbe de pléthysmographie"
      ValeurMoniteur "mon.pa"                 nom: "PA"     unite: "mmHg"   (#F85149 // @moniteur.pa)
      ValeurMoniteur "mon.fr"                 nom: "FR"     unite: "/min"   (#E8C33A // @moniteur.fr)
      ValeurMoniteur "mon.temp"               nom: "T°"     unite: "°C"     (#E6E6E6 // @moniteur.temp)
    HBox "mon.boutons"                        gap: 4   visible: false
      Button "moniteur.silence_alarme"        icon: bell-off   label: "Silence 2 min"
      Button "moniteur.debrancher"            icon: unplug     label: "Débrancher"
```
- `mon.debranche.visible = not moniteur.branche` ; `mon.valeurs.visible =
  mon.boutons.visible = moniteur.branche` ✅.
- Les **chiffres** : `text` lié aux constantes (idempotent ✅). L'**alarme** :
  état `Alarm` sur la `ValeurMoniteur` (compté, non peint aujourd'hui) **et** un son
  (application).
- ⚠️ Le moniteur affiche **ce que mesure la machine**, **pas** d'interprétation
  (« tachycardie ») : ce serait donner la réponse.

### 5.8 `dossier.nkgui` et ses onglets

```
Panel "dossier"                               title: "Dossier"
  VBox "colonne"                              gap: 0
    HBox "dos.tete"                           gap: 4
      TabBar "dossier.onglet"                 bind: dossier.onglet
                                              tabs: ["Conversation","Examen","Examens","Hypothèses","Prescriptions","Notes","Synthèse"]
      Button "dossier.fermer"                 icon: x   tooltip: "Fermer (D)"
    Scroll "dos.defil"                        axis: Vertical
      Host "dos.contenu"                      hint: "le document de l'onglet"
```

#### `dossier_conversation.nkgui`

```
VBox "racine"
  Host "conv.lignes"                          hint: "transcription horodatée : Vous / Patient ; les actions de communication en italique"
```

#### `dossier_examen.nkgui`

```
VBox "racine"
  Host "exa.resultats"                        hint: "résultats des gestes, groupés par région, horodatés ; clic → la caméra montre la région"
```

#### `dossier_examens.nkgui` (examens complémentaires)

```
VBox "racine"                                 gap: 6
  Categorie "bil.prescrire"                   label: "Prescrire"
    TextField "bil.recherche"                 placeholder: "Rechercher un examen (NFS, CRP, radio thorax, ECG…)"   icon✚: search   bind: bil.recherche
    Host "bil.catalogue"                      hint: "résultats de recherche ; clic → ajouter à la demande"
    Host "bil.demande"                        hint: "examens choisis, pas encore demandés"
    Button "bilan.demander"                   label: "Demander"   icon: send
  Categorie "bil.attente"                     label: "En attente"
    Host "bil.attente_liste"                  hint: "examens demandés : nom, heure de demande, sablier (temps simulé restant)"
  Categorie "bil.recus"                       label: "Résultats"
    Host "bil.recus_liste"                    hint: "résultats reçus : valeurs, normes du laboratoire, images (radio, ECG) — clic → en grand"
```
- Les **normes** du laboratoire s'affichent (comme sur un vrai bulletin) ; les
  valeurs hors normes sont marquées **comme sur un vrai bulletin** (astérisque ou
  H / B), **sans commentaire** interprétatif.

#### `dossier_hypotheses.nkgui`

```
VBox "racine"                                 gap: 6
  HBox "hyp.ajout"                            gap: 4
    TextField "hyp.saisie"                    placeholder: "Une hypothèse…"   bind: hyp.saisie
    Button "hypo.ajouter"                     icon: plus   label: "Ajouter"
  Host "hyp.liste"                            hint: "hypothèses de l'apprenant, glisser pour classer ; pour chacune un curseur de certitude"
  Separator "hyp.sep"
  HBox "hyp.retenu"                           gap: 4
    Text "hyp.retenu_titre"                   text: "Diagnostic retenu :"
    TextField "hyp.retenu_saisie"             bind: hypo.retenu   placeholder: "—"
```
⚠️ **Aucune liste de pathologies proposée** ici (pas de complétion depuis la base
des maladies) : ce serait souffler les hypothèses. L'apprenant **écrit**. La
reconnaissance (pour la grille) se fait au débriefing.

#### `dossier_prescriptions.nkgui`

```
VBox "racine"                                 gap: 6
  Categorie "pr.medicament"                   label: "Médicament"
    TextField "pr.med_recherche"              placeholder: "Médicament (DCI)…"   icon✚: search   bind: presc.med
    LigneNombre "pr.dose"                     etiquette: "Dose"        bind: presc.dose
    LigneListe "pr.unite"                     etiquette: "Unité"       bind: presc.unite   items: ["mg","g","µg","UI","mL","mg/kg"]
    LigneListe "pr.voie"                      etiquette: "Voie"        bind: presc.voie    items: ["Orale","IV","IM","SC","Inhalée","Rectale","Sublinguale"]
    LigneListe "pr.frequence"                 etiquette: "Fréquence"   bind: presc.frequence   items: ["Une fois","Toutes les 4 h","Toutes les 6 h","Toutes les 8 h","Toutes les 12 h","Par jour","En continu"]
    BoutonEcrit "presc.prescrire"             label: "Prescrire"   icon: pill
  Categorie "pr.gestes"                       label: "Gestes"
    Flow "pr.gestes_liste"                    gap: 4
      Button "presc.voie_veineuse"            label: "Voie veineuse"
      Button "presc.oxygene"                  label: "Oxygène…"
      Button "presc.remplissage"              label: "Remplissage…"
      Button "presc.sonde_urinaire"           label: "Sonde urinaire"
      Button "presc.sonde_gastrique"          label: "Sonde gastrique"
      Button "presc.pansement"                label: "Pansement"
      Button "presc.immobilisation"           label: "Immobilisation"
  Categorie "pr.orientation"                  label: "Orientation"
    Flow "pr.orientation_liste"               gap: 4
      Button "presc.hospitaliser"             label: "Hospitaliser…"
      Button "presc.bloc"                     label: "Bloc opératoire"
      Button "presc.transferer"               label: "Transférer…"
      Button "presc.sortie"                   label: "Sortie avec consignes…"
  Categorie "pr.historique"                   label: "Déjà prescrit"
    Host "pr.historique_liste"                hint: "prescriptions et gestes horodatés"
```
- `BoutonEcrit` sur **Prescrire** : l'acte **agit sur le patient** (c'est le sens
  « ce bouton modifie » de la famille).
- Toutes les lignes : `cle.visible = false`.
- ⚠️ **Aucune alerte d'allergie ou d'interaction** n'est affichée en séance (le
  patient **réagit**, et le débriefing explique) — sauf si le formateur l'active
  pour un cas d'entraînement (réglage du cas).

#### `dossier_notes.nkgui`

```
TextField "notes.texte"                       multiline: true   wrap: true   bind: seance.notes   placeholder: "Notes libres…"
```

#### `dossier_synthese.nkgui`

```
VBox "racine"                                 gap: 6
  SectionObservation "syn.identite"           titre: "Identité"                 bind: syn.identite
  SectionObservation "syn.motif"              titre: "Motif de consultation"    bind: syn.motif
  SectionObservation "syn.histoire"           titre: "Histoire de la maladie"   bind: syn.histoire
  SectionObservation "syn.antecedents"        titre: "Antécédents"              bind: syn.antecedents
  SectionObservation "syn.examen"             titre: "Examen clinique"          bind: syn.examen
  SectionObservation "syn.synthese"           titre: "Synthèse"                 bind: syn.synthese
  SectionObservation "syn.hypotheses"         titre: "Hypothèses"               bind: syn.hypotheses
  SectionObservation "syn.cat"                titre: "Conduite à tenir"         bind: syn.cat
```
Les sections sont **celles du cas** (l'application peut en ajouter ou en retirer
selon la structure imposée par le formateur) ; ce document donne la structure par
défaut.

### 5.9 `bloc.nkgui`

```
Window "bloc"                                 title: ""   flags: NoTitleBar
  VBox "racine"                               gap: 0
    HBox "bloc.tete"                          gap: 8   align: Center
      Text "bloc.titre"                       text: "Bloc opératoire"
      Spacer "pousse"
      Text "seance.temps"                     text: "00:00"   font: "Mono-12"
    Splitter "bloc.partage"                   orientation: Vertical   ratio: 0.18
      VBox "bloc.gauche"                      gap: 6
        Categorie "bloc.checklist"            label: "Check-list de sécurité"
          Checkbox "bloc.ck_identite"         label: "Identité du patient confirmée"          bind: bloc.ck.identite
          Checkbox "bloc.ck_site"             label: "Site opératoire marqué"                 bind: bloc.ck.site
          Checkbox "bloc.ck_consentement"     label: "Consentement éclairé"                   bind: bloc.ck.consentement
          Checkbox "bloc.ck_anesthesie"       label: "Contrôle de sécurité anesthésique"       bind: bloc.ck.anesthesie
          Checkbox "bloc.ck_allergies"        label: "Allergies connues vérifiées"            bind: bloc.ck.allergies
          Checkbox "bloc.ck_saignement"       label: "Risque hémorragique évalué"             bind: bloc.ck.saignement
          Checkbox "bloc.ck_antibio"          label: "Antibioprophylaxie"                     bind: bloc.ck.antibio
          Checkbox "bloc.ck_equipe"           label: "Équipe présentée, rôles connus"         bind: bloc.ck.equipe
        Categorie "bloc.anesthesie"           label: "Anesthésie"
          LigneListe "bloc.type_anesthesie"   etiquette: "Type"   bind: bloc.anesthesie   items: ["—","Générale","Rachianesthésie","Locorégionale","Locale"]
          Button "bloc.induire"               label: "Induire"   icon: syringe
        Stepper✚ "bloc.etape"                 bind: bloc.etape   orientation: Vertical   free: false
                                              steps: (les étapes de l'intervention, venues du cas)
      Splitter "bloc.partage_droit"           orientation: Vertical   ratio: 0.78
        VBox "bloc.centre"                    gap: 0
          Host "bloc.champ"                   hint: "champ opératoire 3D, anatomie par couches, instruments en main, tracé des sutures"
          HBox "bloc.plateau"                 gap: 4
            BoutonInstrument "bloc.bistouri"      icon: slice          label: "Bistouri"
            BoutonInstrument "bloc.pince"         icon: grip           label: "Pince"
            BoutonInstrument "bloc.ciseaux"       icon: scissors       label: "Ciseaux"
            BoutonInstrument "bloc.ecarteur"      icon: move-horizontal label: "Écarteur"
            BoutonInstrument "bloc.porte_aiguille" icon: pen-tool      label: "Porte-aiguille"
            BoutonInstrument "bloc.aspiration"    icon: wind           label: "Aspiration"
            BoutonInstrument "bloc.compresse"     icon: square         label: "Compresse"
            BoutonInstrument "bloc.bistouri_elec" icon: zap            label: "Bistouri électrique"
          HBox "bloc.parole"                  gap: 4
            HoldButton✚ "parole.parler"       label: "Parler à l'équipe"   icon: mic   key: "Espace"
        Host "bloc.moniteur"                  hint: "moniteur.nkgui (toujours branché au bloc)"
```
- `bloc.induire.enabled = (bloc.anesthesie != "—")` ✅.
- **L'incision** avant la check-list complète : **permise** (l'erreur est notée),
  sauf si le formateur l'a interdite pour le cas.
- En **Examen**, les étapes **à venir** du `Stepper` sont `Hidden`.
- Les **instruments** : la famille `trousse` et `bloc` sont deux groupes
  distincts (`group: trousse` surchargé en `group: bloc` par l'instance).

### 5.10 `fin_seance.nkgui`

```
Window "fin_seance"                           title: "Terminer la séance ?"   modal: true
  VBox "racine"                               gap: 6
    Host "fin.manques"                        hint: "ce qui n'a pas été fait (« Synthèse non rédigée », « Aucune hypothèse notée ») — SANS dire ce qui manque cliniquement"
    HBox "fin.boutons"                        gap: 4   justify: End
      Button "fin.continuer"                  label: "Continuer la séance"
      Button "seance.valider_fin"             label: "Terminer et voir le débriefing"   icon: flag
```
⚠️ `fin.manques` ne liste que des **parties du dossier vides**, jamais « vous n'avez
pas ausculté » : ce serait un indice.

### 5.11 `debriefing.nkgui`

```
Window "debriefing"                           title: "Débriefing"
  VBox "racine"                               gap: 8
    HBox "deb.tete"                           gap: 8   align: Center
      Text "deb.titre"                        text: "?"       (motif — âge — mode — durée)
      Spacer "pousse"
      Button "debrief.rejouer"                label: "Rejouer la séance"   icon: play
      Button "debrief.exporter_pdf"           label: "PDF"                 icon: file-down
      Button "debrief.exporter_trace"         label: "Trace (FHIR)"        icon: file-json
      Button "debrief.fermer"                 label: "Fermer"
    Splitter "deb.haut"                       orientation: Vertical   ratio: 0.35
      VBox "deb.scores"                       gap: 4
        Text "deb.score_total"                text: "Score : —"
        BlocCompetence "deb.c_interro"        nom: "Interrogatoire"     bind: debrief.score.interro
        BlocCompetence "deb.c_examen"         nom: "Examen"             bind: debrief.score.examen
        BlocCompetence "deb.c_raison"         nom: "Raisonnement"       bind: debrief.score.raisonnement
        BlocCompetence "deb.c_pec"            nom: "Prise en charge"    bind: debrief.score.pec
        BlocCompetence "deb.c_comm"           nom: "Communication"      bind: debrief.score.comm
        BlocCompetence "deb.c_bloc"           nom: "Bloc"               bind: debrief.score.bloc   visible: false
      VBox "deb.retours"                      gap: 6
        Text "deb.critiques_titre"            text: "Erreurs critiques"   (text.color #F85149 // @grille.critique)
        VBox "deb.critiques"                  gap: 2     (ItemGrille ✕, ajoutés par l'application)
        Text "deb.bien_titre"                 text: "Ce que vous avez bien fait"
        VBox "deb.bien"                       gap: 2     (ItemGrille ✓)
        Text "deb.manque_titre"               text: "Ce que vous avez manqué"
        VBox "deb.manque"                     gap: 2     (ItemGrille — et ◐)
        Expander "deb.grille"                 label: "Grille complète"   expanded: false
          VBox "deb.grille_items"             gap: 2     (ItemGrille pour tous les items)
    Host "deb.frise"                          hint: "frise des actes horodatés ; clic → relecture à ce moment"
    TabBar "deb.onglet"                       bind: deb.onglet   tabs: ["Raisonnement","Parcours","Ressenti du patient","Observation"]
    Host "deb.onglet_contenu"                 hint: "Raisonnement : vos hypothèses ↔ diagnostic différentiel du cas ; Parcours : vous ↔ expert par étape ; Ressenti : phrases du patient ; Observation : la vôtre ↔ le modèle"
```
- `deb.c_bloc.visible = cas.a_bloc` ✅ ; `deb.critiques_titre.visible = (nb_critiques > 0)` ✅.
- **C'est le seul document où `@grille.*` et le corrigé apparaissent** (§0.3).

### 5.12 `editeur_cas.nkgui` (cadre)

```
Window "editeur_cas"                          title: "Cas"
  VBox "racine"                               gap: 0
    HBox "ec.barre"                           gap: 4   align: Center
      Button "edcas.enregistrer"              icon: save   label: "Enregistrer"   tooltip: "Ctrl+S"
      Button "edcas.essayer"                  icon: play   label: "Essayer le cas"
      Button "ia.assistant"                   icon: sparkles   label: "Assistant"
      Spacer "pousse"
      Badge✚ "ec.statut"                      text: "Brouillon"   tone: cas.brouillon
      Dropdown "ec.langue"                    bind: edcas.langue_edition   items: ["Français","English"]
      Button "edcas.soumettre"                label: "Soumettre à relecture"   icon: send
    Splitter "ec.partage"                     orientation: Vertical   ratio: 0.18
      ListBox "edcas.onglet"                  bind: edcas.onglet
                                              items: ["Identité et apparence","Personnalité","Histoire","Examen","Examens complémentaires","Évolution","Bloc","Corrigé","Grille","Validation"]
      Scroll "ec.defil"                       axis: Vertical
        Host "ec.contenu"                     hint: "le document de l'onglet (edcas_*)"
```
- `ec.langue` : on écrit le cas **dans une langue à la fois** ; les champs non
  traduits de l'autre langue sont signalés (ambre) dans chaque onglet.
- `edcas.soumettre.enabled = (statut == "Brouillon")` ✅.

### 5.13 Les onglets de l'éditeur de cas

#### `edcas_identite.nkgui`

```
VBox "racine"
  Categorie "cat_identite"                    label: "Identité (fictive)"
    TextField "edcas.nom"                     bind: cas.patient.nom   placeholder: "Nom fictif"
    LigneNombre "edcas.age"                   etiquette: "Âge"      bind: cas.patient.age   min: 0   max: 110
    LigneListe "edcas.sexe"                   etiquette: "Sexe"     bind: cas.patient.sexe   items: ["Femme","Homme"]
    LigneListe "edcas.langue_patient"         etiquette: "Langue parlée"   bind: cas.patient.langue   items: ["Français","English"]
    TextField "edcas.metier"                  bind: cas.patient.metier   placeholder: "Métier, contexte social"
  Categorie "cat_apparence"                   label: "Apparence"
    LigneAsset "edcas.personnage"             etiquette: "Personnage"   bind: cas.patient.personnage   type: "Personnage"
    Button "edcas.generer_personnage"         label: "Créer avec le générateur (NKCraft)"   icon: user-round-cog
    Host "edcas.apercu"                       hint: "aperçu 3D du patient"
  Categorie "cat_situation"                   label: "Situation de départ"
    TextField "edcas.situation"               multiline: true   bind: cas.situation   placeholder: "Ce que lit l'apprenant en entrant (« Homme de 54 ans, amené aux urgences pour… »)"
    LigneListe "edcas.lieu"                   etiquette: "Lieu"   bind: cas.lieu   items: ["Urgences","Consultation","Chambre","Domicile","Bloc"]
```
Toutes les lignes : `cle.visible = false` (vaut pour tout l'éditeur de cas).

#### `edcas_personnalite.nkgui`

```
VBox "racine"
  Categorie "cat_traits"                      label: "Traits (modèle à cinq facteurs)"
    CurseurParlant "edcas.ouverture"          gauche: "fermé"       droite: "curieux"        bind: cas.perso.openness
    CurseurParlant "edcas.rigueur"            gauche: "désinvolte"  droite: "stoïque"        bind: cas.perso.conscientiousness
    CurseurParlant "edcas.extraversion"       gauche: "réservé"     droite: "expansif"       bind: cas.perso.extraversion
    CurseurParlant "edcas.agreabilite"        gauche: "méfiant"     droite: "coopératif"     bind: cas.perso.agreeableness
    CurseurParlant "edcas.nevrosisme"         gauche: "calme"       droite: "anxieux"        bind: cas.perso.neuroticism
  Categorie "cat_expression"                  label: "Expression"
    CurseurParlant "edcas.tolerance"          gauche: "sensible"    droite: "dur au mal"     bind: cas.perso.painTolerance
    CurseurParlant "edcas.expressivite"       gauche: "retenu"      droite: "très expressif" bind: cas.perso.expressionScale
  Categorie "cat_capacites"                   label: "Ce que le patient peut faire"
    Flow "edcas.capacites"                    gap: 4
      Checkbox "edcas.cap_pleurer"            label: "Pleurer"                     bind: cas.perso.canCry
      Checkbox "edcas.cap_refuser"            label: "Refuser de répondre"         bind: cas.perso.canRefuseToAnswer
      Checkbox "edcas.cap_minimiser"          label: "Minimiser sa douleur"        bind: cas.perso.canLieAboutPain
      Checkbox "edcas.cap_initier"            label: "Parler sans qu'on l'interroge" bind: cas.perso.canInitiateConversation
      Checkbox "edcas.cap_paniquer"           label: "Paniquer"                    bind: cas.perso.canPanic
      Checkbox "edcas.cap_colere"             label: "Se fâcher"                   bind: cas.perso.canShowAnger
      Checkbox "edcas.cap_montrer"            label: "Montrer où il a mal"         bind: cas.perso.canReachForPainSite
      Checkbox "edcas.cap_proteger"           label: "Se protéger avec le bras"    bind: cas.perso.canGuardWithArm
  Categorie "cat_role"                        label: "Le rôle, en mots"
    TextField "edcas.role"                    multiline: true   bind: cas.perso.roleDescription   placeholder: "Un vieux pêcheur de Kribi, méfiant envers l'hôpital, qui a peur de perdre son travail…"
  Button "edcas.essayer_personnalite"         label: "Parler au patient ainsi réglé"   icon: message-circle
```
📌 Les `bind` reprennent **les noms de champs de `NkPersonality`** (existant).

#### `edcas_histoire.nkgui`

```
VBox "racine"
  TabBar "edcas.theme_histoire"               bind: edcas.theme   tabs: ["Motif","Histoire","Antécédents","Traitements","Allergies","Mode de vie","Contexte"]
  Host "edcas.faits"                          hint: "faits du thème : pour chacun — le fait (clinique), comment le patient le dit, spontané / si on demande, question(s) qui le révèlent (FR/EN)"
  Button "edcas.ajouter_fait"                 label: "+ Fait"   icon: plus
```

#### `edcas_examen.nkgui`

```
Splitter "racine"                             orientation: Vertical   ratio: 0.45
  Host "edcas.carte_corps"                    hint: "patient 3D : régions cliquables ; les régions renseignées sont marquées (ici, c'est l'atelier : le marquage est permis)"
  VBox "edcas.region"                         gap: 4
    Text "edcas.region_nom"                   text: "—"
    Host "edcas.gestes_region"                hint: "pour la région choisie : un résultat par geste (inspection, palpation, percussion, auscultation…) — texte, image, son, réaction du patient ; « normal » par défaut"
```

#### `edcas_bilans.nkgui`, `edcas_evolution.nkgui`, `edcas_bloc.nkgui`

- **Bilans** : catalogue (recherche) ; pour chaque examen retenu : résultat
  (valeurs, image), **délai**. `Host` + boutons `edcas.bilan_ajouter`,
  `edcas.bilan_retirer`.
- **Évolution** : `Host "edcas.graphe_evolution"` — **l'éditeur de nœuds commun**
  (substrat NKGraph) : états, transitions (temps, actes), constantes par état,
  effets des médicaments ; à côté, `Button "edcas.simuler"` (« Simuler 2 h ») et
  un `Host` qui trace les constantes simulées.
- **Bloc** : `Host "edcas.graphe_bloc"` (même éditeur) : étapes, instruments
  attendus, erreurs possibles et leurs conséquences.

#### `edcas_corrige.nkgui`

```
VBox "racine"
  Categorie "cat_diagnostic"                  label: "Diagnostic"
    TextField "edcas.diagnostic"              bind: cas.corrige.diagnostic
    LigneListe "edcas.cim10"                  etiquette: "CIM-10"   bind: cas.corrige.cim10
  Categorie "cat_differentiel"                label: "Diagnostic différentiel"
    Button "edcas.proposer_differentiel"      label: "Proposer depuis la base"   icon: list-ordered   tooltip: "NkDiagnosticEngine classe les pathologies d'après les faits du cas ; vous ajustez"
    Host "edcas.differentiel"                 hint: "pathologies classées, avec les arguments pour / contre ; glisser pour reclasser"
  Categorie "cat_parcours"                    label: "Parcours d'expert"
    Host "edcas.parcours"                     hint: "étapes attendues, dans l'ordre, avec les délais"
  Categorie "cat_enseignement"                label: "Points d'enseignement"
    TextField "edcas.points"                  multiline: true   bind: cas.corrige.points
```
📌 C'est **ici** que vit `DiagnosticPanel` (et `SymptomInput`) — décision D3.

#### `edcas_grille.nkgui`

```
VBox "racine"
  HBox "gr.barre"                             gap: 4
    Button "edcas.item_ajouter"               label: "+ Item"   icon: plus
    Dropdown "edcas.item_competence"          bind: edcas.competence   items: ["Interrogatoire","Examen","Raisonnement","Prise en charge","Communication","Bloc"]
  Host "gr.items"                             hint: "items : libellé, compétence, points, CRITIQUE (case), formulations reconnues FR / EN (pour les questions), acte attendu (pour les gestes)"
```

#### `edcas_validation.nkgui`

```
VBox "racine"
  LigneListe "edcas.statut"                   etiquette: "Statut"   bind: cas.statut   items: ["Brouillon","En relecture","Validé"]
  TextField "edcas.relecteur"                 bind: cas.relecteur   placeholder: "Nom du relecteur médical"
  Text "edcas.date_validation"                text: "—"
  Host "edcas.commentaires"                   hint: "commentaires de relecture, par onglet"
  Categorie "cat_sources"                     label: "Sources"
    TextField "edcas.sources"                 multiline: true   bind: cas.sources   placeholder: "Recommandations, ouvrages, articles…"
  HBox "val.boutons"                          gap: 4   justify: End
    Button "edcas.demander_corrections"       label: "Demander des corrections"
    BoutonEcrit "edcas.valider"               label: "Valider le cas"   icon: badge-check
```
- `edcas.statut` n'est **pas** modifiable à la main vers « Validé » : seule
  l'action `edcas.valider` le fait, **par un profil relecteur** (raison sinon :
  « Seul un relecteur médical peut valider »).
- Un cas **modifié après validation** repasse en **En relecture** (application).

### 5.14 `suivi.nkgui`

```
Window "suivi"                                title: "Promotions"
  VBox "racine"                               gap: 6
    HBox "sv.barre"                           gap: 4
      Dropdown "suivi.promotion"              bind: suivi.promotion   (items : promotions)
      Button "suivi.nouvelle_promotion"       icon: users   label: "Nouvelle promotion"
      Dropdown "suivi.competence"             bind: suivi.competence   items: ["Toutes","Interrogatoire","Examen","Raisonnement","Prise en charge","Communication","Bloc"]
      Spacer "pousse"
      Button "suivi.proposer_cas"             label: "Proposer un cas…"   icon: send
      Button "suivi.exporter"                 label: "Exporter"   icon: download
    Table "sv.tableau"                        columns: ["Apprenant","Cas","Score","Critiques","Date"]   (non monté : attendu)
    Host "sv.tableau_hote"                    hint: "apprenants × cas ; clic → le débriefing de l'apprenant"
```

### 5.15 `demonstration.nkgui` (surimpression sur la salle)

```
HBox "demo.barre"                             gap: 4   align: Center
  Text "demo.titre"                           text: "Démonstration"
  Button "demo.pause"                         icon: pause        label: "Pause"
  Button "demo.dessiner"                      icon: pencil       label: "Dessiner"
  Button "demo.effacer_dessin"                icon: eraser       tooltip: "Effacer le dessin"
  Button "demo.montrer_corrige"               icon: eye          label: "Montrer le corrigé"
  Button "demo.etat_patient"                  icon: activity     label: "État du patient"     (le PatientStatePanel actuel)
  Button "demo.terminer"                      icon: x            label: "Terminer"
```
Monté **au-dessus de `salle.tete`**, seulement en mode Démonstration.

### 5.16 `reglages.nkgui`

```
Window "reglages"                             title: "Réglages"
  Splitter "rg.partage"                       orientation: Vertical   ratio: 0.22
    ListBox "rg.categorie"                    bind: rg.categorie   items: ["Langue","Son et micro","Affichage","Intelligence artificielle","Profils","Mentions"]
    Scroll "rg.defil"
      VBox "rg.contenu"                       gap: 6
        RadioGroup "reglages.langue"          bind: reglages.langue   options: ["Français","English"]   visible: false
        VBox "rg.son"                         gap: 4   visible: false
          LigneListe "reglages.micro"         etiquette: "Micro"   bind: reglages.micro   (items : périphériques)
          Button "reglages.tester_micro"      label: "Tester le micro"   icon: mic
          Meter✚ "rg.niveau_micro"            bind: reglages.niveau_micro   channels: 1
          LigneNombre "reglages.sensibilite"  etiquette: "Sensibilité"   bind: reglages.sensibilite   min: 0   max: 1
          Button "reglages.tester_casque"     label: "Tester le casque (sons d'auscultation)"   icon: headphones
          LigneCase "reglages.sous_titres"    etiquette: "Sous-titres"   bind: reglages.sous_titres
        VBox "rg.affichage"                   gap: 4   visible: false
          LigneListe "reglages.qualite"       etiquette: "Qualité graphique"   bind: reglages.qualite   items: ["Économe","Standard","Maximale"]
          LigneListe "reglages.theme"         etiquette: "Thème"   bind: reglages.theme   items: ["Sombre","Clair"]
        VBox "rg.ia"                          gap: 4   visible: false
          LigneListe "reglages.modele"        etiquette: "Modèle du patient"   bind: reglages.modele   items: ["Règles (partout)","Petit modèle local","Grand modèle local"]
          Text "rg.ia_machine"                text: "—"   wrap: true   (ce que la machine permet, mesuré à l'installation)
          Text "rg.ia_hors_ligne"             text: "Tout fonctionne sans Internet."
        Host "rg.profils"                     hint: "profils locaux : ajouter, renommer, supprimer (avec confirmation), rôle (apprenant / formateur / relecteur)"   visible: false
        Text "rg.mentions"                    text: "Simulation à but pédagogique — ne constitue pas un avis médical. Crédits et licences…"   wrap: true   visible: false
```
Visibilités par catégorie — idempotentes ✅. Toutes les lignes : `cle.visible = false`.
`reglages.modele` : l'option « Grand modèle local » est **grisée** si la machine
ne le permet pas (raison : « Mémoire graphique insuffisante : il faut … »).

### 5.17 `dispositions.nkgui`

```
layout "salle"
  document "salle"             (salle.bas = salle_parole | salle_trousse ; salle.droite = moniteur | dossier)
layout "bloc"
  document "bloc"
layout "debriefing"
  document "debriefing"
layout "atelier"
  dock "editeur_cas" center · dock "assistant_ia" right 0.26
layout "suivi"
  document "suivi"
```
**La salle et le bloc n'ont pas d'amarrage** : c'est la règle « le patient n'est
jamais caché » — un utilisateur ne peut pas y amarrer un panneau par-dessus.

---

## 6. Les actions — toutes ✚ (aucun document `.nkgui` n'existe pour PV3DE)

| domaine | actions |
|---|---|
| **accueil** | `apprenant` · `formateur` · `changer_profil` |
| **profil** | `nouveau` |
| **cas** | `nouveau` · `importer` · `exporter` |
| **seance** | `entrer` · `quitter` · `pause` · `accelerer` · `indice` · `terminer` · `valider_fin` |
| **etape** | (le `Stepper` : `etape.choix` porte une valeur, ce n'est pas une action) |
| **parole** | `parler` · `envoyer` |
| **comm** | `se_presenter` · `expliquer` · `consentement` · `rassurer` · `reformuler` · `annoncer` |
| **examen** | `stethoscope` · `palper` · `percuter` · `lampe` · `marteau` · `thermometre` · `tensiometre` · `oxymetre` · `glycemie` · `otoscope` · `abaisse_langue` · `regarder` · `demander_respirer` · `demander_tousser` |
| **installer** | `allonge` · `assis` · `debout` · `cote` |
| **decouvrir** | `recouvrir` |
| **consent** | `demander` · `passer_outre` · `chaperon` |
| **moniteur** | `brancher` · `debrancher` · `silence_alarme` |
| **dossier** | `ouvrir` · `fermer` |
| **bilan** | `demander` |
| **hypo** | `ajouter` |
| **presc** | `prescrire` · `voie_veineuse` · `oxygene` · `remplissage` · `sonde_urinaire` · `sonde_gastrique` · `pansement` · `immobilisation` · `hospitaliser` · `bloc` · `transferer` · `sortie` |
| **bloc** | `induire` · `bistouri` · `pince` · `ciseaux` · `ecarteur` · `porte_aiguille` · `aspiration` · `compresse` · `bistouri_elec` |
| **fin** | `continuer` |
| **debrief** | `rejouer` · `exporter_pdf` · `exporter_trace` · `fermer` · `aller_au_moment` |
| **edcas** | `enregistrer` · `essayer` · `soumettre` · `generer_personnage` · `essayer_personnalite` · `ajouter_fait` · `bilan_ajouter` · `bilan_retirer` · `simuler` · `proposer_differentiel` · `item_ajouter` · `demander_corrections` · `valider` |
| **suivi** | `nouvelle_promotion` · `proposer_cas` · `exporter` |
| **demo** | `pause` · `dessiner` · `effacer_dessin` · `montrer_corrige` · `etat_patient` · `terminer` |
| **reglages** | `tester_micro` · `tester_casque` |
| **fenetre** | `reglages` |
| **ia** | `assistant` (+ celles de l'Assistant commun, **dans l'atelier seulement**) |

### Raisons de grisé

| cas | raison |
|---|---|
| action non servie | « Pas encore disponible » |
| Examen sur un cas non validé | « Ce cas n'est pas encore validé par un relecteur médical » |
| `seance.pause`, `seance.accelerer`, `seance.indice` en Examen | (masqués, pas grisés : **rien ne doit suggérer qu'une aide existe**) |
| `installer.debout` | « Le patient ne tient pas debout » (selon l'état) |
| `edcas.valider` sans profil relecteur | « Seul un relecteur médical peut valider » |
| `reglages.modele` = grand sur petite machine | « Mémoire graphique insuffisante : il faut … » |
| bloc sans indication | l'étape Bloc est **masquée** (`Hidden`) si le cas n'en a pas |

---

## 7. Ce que tu ne sais pas encore reproduire — écrit quand même

### 7.1 États

| élément | état | rendu |
|---|---|---|
| `HoldButton "parole.parler"` | `Recording` / `Processing` | fond `@info.enregistrement` pulsant / sablier |
| `ValeurMoniteur` | `Alarm` (haute / moyenne) | cadre `@alarme.*` clignotant (1 Hz) |
| `Stepper` | `Done` / `Current` / `Todo` / `Hidden` | `@etape.*` |
| `BoutonInstrument` | `Active` | fond `@accent` ; le curseur devient l'instrument |
| `CarteCas` | statut | pastille `@cas.*` |

### 7.2 Animations

| élément | animation |
|---|---|
| `HoldButton` en enregistrement | pulsation d'échelle douce |
| `ValeurMoniteur` en alarme | clignotement 1 Hz **et** son (application) |
| sous-titres | apparaissent mot à mot, synchronisés à la voix |
| `CarteConsentement` | glisse depuis le bas (120 ms) |

### 7.3 Événements (lot b)

| élément | événement | effet | idempotent ? |
|---|---|---|---|
| `HoldButton` | `on Press` / `on Release` | ouvrir / fermer le micro, transcrire, envoyer | ❌ |
| `parole.texte` | `on Submit` | envoyer la question | ❌ |
| `etape.choix` | `on Changed` | changer d'étape (et de barre du bas) | ❌ |
| régions du corps (application) | clic avec un instrument | faire le geste | ❌ |
| `ItemGrille` | `on Click` sur ▶ | relecture à ce moment | ❌ |

### 7.4 Polices

Comme la famille ; **`"Mono-24"`** pour les chiffres du moniteur, **`"Mono-12"`**
pour les temps ; sous-titres en **`"UI-16"`** (lisibles à distance, et pour la
démonstration projetée).

---

## 8. L'ordre de livraison

Aligné sur le phasage du doc 01.

| # | livrable | palier | témoin |
|---|---|---|---|
| 1 | `composants_pv3de` + `accueil` + `accueil_apprenant` + `salle` + `salle_parole` + `dossier` (Conversation, Notes) | P0–P1 | un interrogatoire au clavier ; **aucun jeton `@grille.*` dans les documents de la salle** (vérification par script) |
| 2 | `debriefing` (sans les onglets Raisonnement et Parcours) + `fin_seance` + `reglages` | P1 | la frise rejoue une question à la seconde près |
| 3 | `salle_trousse` + `moniteur` + `dossier_examen` + `CarteConsentement` | P2 | la palpation de la zone douloureuse déclenche la grimace **et** écrit le résultat au dossier |
| 4 | `dossier_examens` + `dossier_hypotheses` + `dossier_prescriptions` + `dossier_synthese` + débriefing complet | P3 | un sepsis : la prescription d'antibiotique change l'évolution, la grille le note |
| 5 | `accueil_formateur` + `editeur_cas` + onglets + `suivi` + `demonstration` | P4 | un cas écrit, soumis, **validé par un profil relecteur**, passé en Examen |
| 6 | `bloc` | P5 | l'incision avant la check-list est notée en erreur |
| 7 | `dispositions` | au fil de l'eau | la salle refuse l'amarrage d'un panneau |

📌 Après l'étape 1, publie `apparencesNonPeintes`, `etatsNonAppliques`, **et le
résultat du script qui vérifie qu'aucun document de la salle ne référence
`@grille.*` ni le corrigé.** C'est le témoin de la règle la plus importante du
produit.
