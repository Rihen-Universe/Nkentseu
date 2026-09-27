# NkNgoma — l'interface, pour l'agent qui écrit les `.nkgui`

### Document 3 — Spécification d'interface côté agent

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 27/09/2026.
> Traduction du **02** en tes quatre points : **arbre nommé**, **composants**,
> **actions**, **couleurs porteuses** — plus ce que tu ne sais pas encore reproduire.
> **Si 02 et 03 se contredisent, 02 gagne.**
>
> **Le langage** est le `.nkgui` **0.4** (NKUIDesign doc 2, réécrit le 27/09) : thèmes et
> jetons `@nom`, instructions d'interface (`open`, `disable … because`, `set "id".prop`,
> `message`, `call`…), conditions sur d'autres widgets, carte `application`, section
> `test`, Blueprints. **Le vocabulaire** : NKUIDesign doc 7 (41 rôles, rôles proposés
> P2–P28). **La notation** des arbres : celle des documents « agent » de la famille —
> `Rôle "identifiant"   propriété: valeur`, indentée ; `Rôle✚` = rôle proposé (écris sa
> doublure) ; `propriété✚` = propriété proposée.

---

## 0. Ce qui est propre à NkNgoma

1. **Thème Rihen UE5**, jetons de la famille. **N'écris aucune couleur de structure** ;
   les seules couleurs écrites sont les **jetons d'information** du §2, et ils vivent
   dans `NkNgoma/theme.nkgui`.
2. **Surfaces peintes par l'application** (`Host`) — elles ont leurs propres gestes :
   - la **playlist** (timeline de la famille), la **rangée de pas** de chaque canal, le
     **rouleau de piano**, les **formes d'onde**, les **courbes d'automation** ;
   - la **vidéo de référence** et la **timeline** du son à l'image ;
   - la **grille de zones** de l'échantillonneur, la **courbe d'accordage** ;
   - le **graphe des états** et la **grille des couches** de l'espace Jeu ;
   - les **mesures** (sonie, spectre, corrélation) tant que `Meter` (P14) n'est pas monté.
3. **Actions**, par domaine : `app.` · `transport.` · `espace.` · `composer.` ·
   `rack.` · `canal.` · `pas.` · `rouleau.` · `playlist.` · `pattern.` · `accordage.` ·
   `instr.` · `effet.` · `son.` · `session.` · `design.` · `adr.` · `mix.` · `voie.` ·
   `jeu.` · `export.` · `biblio.` · `reglages.` · `accueil.` + communs (`commun.`,
   `ia.`, `fenetre.`, `edition.`).
4. **Composants communs repris** (`Commun/composants.nkgui`) : `LigneNombre`,
   `LigneListe`, `LigneCase`, `Categorie`, `EnTetePanneau`, `CartePropo`, `BoutonEcrit`.
5. **Règle D12 de la famille** (NKUIDesign doc 2 §14.3, recommandation) : un composant
   à **une** action la porte par sa **racine** ; un composant à **plusieurs** actions
   donne à chaque enfant actif `id: "<action>"` et `// D12`.
6. **NKScena n'est pas modifié.** Le composant `TrancheMixeur` de NKScena est **recopié**
   ici (§3.1) à l'identique, avec la proposition de le déplacer dans `Commun/` plus tard.

---

## 1. Le vocabulaire

**Consommés** : P2 `ToggleButton` (M / S / ●, outils), P3 `Badge` (propositions),
P4 `Tile` (cartes de projets), P11 `pattern = Hatch` (propositions), P13
`TimecodeField` (position, timecode), P14 `Meter` (mesures), P18 `SplitButton`
(Nouveau ▾, Métronome ▾, + Effet ▾), P22 `Stepper` (assistant de démarrage).
**Non employés** : P23 `TokenField` — les couleurs de canal sont des **données**, pas
des jetons (§2) ; P28 `Drawer`.

**Proposés par NkNgoma** — à ajouter au doc 7 §3.10 de NKUIDesign quand il sera repris :

### P29 — `Knob` (potentiomètre)

- **Propriétés** : `bind` `min` `max` `default` `bipolar` (booléen : panoramique centré)
  `curve` (`Linear` · `Log` · `Exp`) `unit` (`dB` · `Hz` · `%` · `ms`) `fine`
  (sensibilité avec Maj).
- **Gestes** : glisser vertical ; **Maj** = fin ; **double-clic** = valeur par défaut ;
  molette ; **Alt+clic** = saisir la valeur.
- **Pourquoi** : les synthés, les départs, le volume et le panoramique du rack tiennent
  en 24 px de large ; un `Slider` horizontal ne tient pas, et ne dit pas la position
  « au centre ». Le 14ter.4 de NKUIDesign listait le potentiomètre circulaire en 🔴.
- **Événement** : `Changed`. **En attendant** : `Slider` étroit, `tooltip` = valeur.

### P30 — `Fader`

- **Propriétés** : `bind` `min` `max` (dB) `default` `scale` (`dB` : graduation non
  linéaire, 0 dB aux deux tiers) `orientation` (`Vertical` par défaut).
- **Pourquoi** : un fader de mixeur est **vertical** et **gradué en dB** ; `Slider` n'a ni
  orientation (NKScena 03 le signalait) ni échelle.
- **En attendant** : `Slider` (`orientation✚: Vertical`, compté non honoré).

### P31 — `Pad`

- **Propriétés** : `label` `color` (donnée utilisateur) `note` (hauteur MIDI).
- **Événements** : `Press` (avec **vélocité** : position verticale du clic, ou vélocité
  MIDI), `Release`.
- **Pourquoi** : la boîte à rythmes se joue à la souris **avec une force** ; un `Button` n'a
  qu'un clic sans intensité. Proche de P21 `HoldButton`, plus la vélocité.
- **En attendant** : `Button` (vélocité fixe).

---

## 2. Les jetons d'information — `NkNgoma/theme.nkgui`

```nkgui
nkgui 0.4
include "../Commun/theme.nkgui"

theme {
  nom = "Rihen UE5"   defaut = "Sombre"
  token @tete.lecture          { type = Color  famille = information  sens = "Position de lecture dans toutes les timelines et le séquenceur" }
  token @info.enregistrement   { type = Color  famille = information  sens = "On enregistre : bouton, pistes armées, liseré du transport" }
  token @son.ecretage          { type = Color  famille = information  sens = "Le son sature" }
  token @son.sonie_ok          { type = Color  famille = information  sens = "Sonie dans la cible choisie" }
  token @son.sonie_hors        { type = Color  famille = information  sens = "Sonie hors de la cible choisie" }
  token @origine.main          { type = Color  famille = origines     sens = "Note ou clip posé à la main" }
  token @origine.ia            { type = Color  famille = origines     sens = "Note ou clip produit par l'IA" }
  token @origine.transcription { type = Color  famille = origines     sens = "Notes transcrites depuis un audio" }
  token @info.apercu           { type = Color  famille = information  sens = "Proposition de l'IA non appliquée (avec hachures)" }
  token @echelle.hors          { type = Color  famille = information  sens = "Rangée hors de la gamme choisie" }
  token @accordage.ecart       { type = Color  famille = information  sens = "L'instrument n'est pas au tempérament du projet" }
  token @repere.image          { type = Color  famille = information  sens = "Repère venu de l'image : coupe, événement d'animation" }
  token @jeu.etat_actif        { type = Color  famille = etats        sens = "État de jeu joué en simulation" }
  variant "Sombre" {
    @tete.lecture = #F2F2F2   @info.enregistrement = #F85149   @son.ecretage = #F85149
    @son.sonie_ok = #3FB950   @son.sonie_hors = #D29922
    @origine.main = #E6E6E6   @origine.ia = #A371F7   @origine.transcription = #2EC4D6
    @info.apercu = #A371F759  @echelle.hors = #FFFFFF14   @accordage.ecart = #D29922
    @repere.image = #0A545E   @jeu.etat_actif = #3FB950
  }
  variant "Clair" {
    @tete.lecture = #1A1A1A   @info.enregistrement = #D1242F   @son.ecretage = #D1242F
    @son.sonie_ok = #1A7F37   @son.sonie_hors = #9A6700
    @origine.main = #1A1A1A   @origine.ia = #8250DF   @origine.transcription = #1B7C8A
    @info.apercu = #8250DF40  @echelle.hors = #0000000F   @accordage.ecart = #9A6700
    @repere.image = #0A545E   @jeu.etat_actif = #1A7F37
  }
}
```

⚠️ **`@origine.*` a le même sens que dans NkAnima** (d'où vient un contenu : main, IA,
transcription) — c'est **voulu** : même mot, même sens. N'y mets jamais un autre sens.

⚠️ **Les couleurs de canal, de pattern et de clip ne sont pas des jetons** : ce sont des
**données** du projet (choisies dans une palette par l'utilisateur), peintes par les
`Host`. N'écris **aucune** d'elles dans un document.

⚠️ Tant que la section `theme` n'est pas servie : écris la valeur Sombre **et** le jeton
en commentaire (`fill #F85149 // @info.enregistrement`).

---

## 3. Les composants — `NkNgoma/composants_ngoma.nkgui`

### 3.1 `TrancheMixeur` — copie conforme de NKScena 03 §3

```
component "TrancheMixeur"
  VBox "tranche"                       gap: 4   minSize: (64, 0)
    Text "nom"                         text: "?"   align: Center
    Host "inserts"                     hint: "effets insérés (égaliseur, compresseur…)"
    Knob✚ "pan"                        bind: ?.pan   min: -1   max: 1   bipolar: true   tooltip: "Panoramique"
    HBox "boutons"                     gap: 2
      ToggleButton✚ "sourdine"         label: "M"   bind: ?.sourdine   tooltip: "Sourdine"
      ToggleButton✚ "solo"             label: "S"   bind: ?.solo       tooltip: "Solo"
      ToggleButton✚ "armer"            label: "●"   bind: ?.armee      tooltip: "Armer pour l'enregistrement"
    HBox "niveau"                      gap: 2
      Fader✚ "fader"                   bind: ?.volume   min: -60   max: 12   scale: dB
      Meter✚ "vu"                      bind: ?.niveau   peak: ?.crete   channels: 2   orientation: Vertical
    KeyDiamond✚ "cle"                  bind: ?.volume
```
- **Deux écarts avec NKScena**, à reporter **là-bas le jour où le composant va dans
  `Commun/`** (et pas avant — consigne : ne pas modifier NKScena) : `pan` en `Knob✚`
  (P29), `fader` en `Fader✚` (P30).
- `armer` actif : `@info.enregistrement` ; `sourdine` actif : `@info.alerte` ; `solo`
  actif : `@accent`.
- `KeyDiamond` : ici, il **écrit un point d'automation** (même geste que la clé
  d'animation : même rôle, même sens — « enregistrer la valeur à cet instant »).
- Les boutons M / S / ● sont des **bascules d'état** (`bind`), pas des actions : pas d'`id`
  d'action à leur donner.

### 3.2 Propres

#### `LigneCanal` — une ligne du rack

```
component "LigneCanal"
  HBox "ligne"                          gap: 4   align: Center   minSize: (0, 28)
    Host "activite"                     hint: "témoin d'activité (clignote à chaque note)"
    ToggleButton✚ "sourdine"            label: "●"   bind: ?.actif   tooltip: "Activer / couper le canal"
    Knob✚ "volume"                      bind: ?.volume   min: -60   max: 6   unit: dB   tooltip: "Volume"
    Knob✚ "pan"                         bind: ?.pan      min: -1    max: 1   bipolar: true   tooltip: "Panoramique"
    Button "nom"                        id: "canal.ouvrir"   label: "?"   tooltip: "Ouvrir l'instrument (double-clic)"   // D12 (b)
    Host "pas"                          hint: "rangée de pas, groupés par temps selon la signature ; ou aperçu des notes pour un canal mélodique"
```
- `nom` porte la **couleur du canal** (donnée) : peinte par l'application, pas écrite.
- `canal.ouvrir` au **clic** ouvre l'instrument (§5.6) ; clic droit : le menu du canal
  (§5.3).

#### `CarteProjet` — une carte de l'accueil

```
component "CarteProjet"
  Tile✚ "carte"                         image: "?"   label: "?"   caption: "?"
```
- La racine porte l'action (`accueil.ouvrir_projet`) ; `image` = forme d'onde du rendu ;
  `Hover` → écoute (P4 le prévoit).
- **En attendant P4** : `Button` avec `label` et `tooltip` = légende.

#### `LigneCouche` — une couche de l'espace Jeu

```
component "LigneCouche"
  Grid "ligne"                          columns: 4   sizes: [0.20, 0.45, 0.25, 0.10]   gap: 6
    Text "nom"                          text: "?"
    Host "segments"                     hint: "segments de la couche, en blocs"
    TextField "regle"                   bind: ?.regle   placeholder: "intensité > 0,3"
    Button "fondu"                      id: "jeu.couche_fondu"   icon: blend   tooltip: "Fondu d'entrée et de sortie"   // D12 (b)
```
- `regle` invalide (paramètre inconnu) : le champ passe en `@controle.erreur` avec la
  raison en infobulle — comportement continu ✅.

#### `CarteFiche` — la fiche de provenance d'un élément de bibliothèque

```
component "CarteFiche"
  VBox "fiche"                          gap: 2
    Text "titre"                        text: "?"
    Grid "champs"                       columns: 2   sizes: [0.35, 0.65]   gap: 2
      Text "l_instrument" text: "Instrument"   Text "v_instrument" text: "—"
      Text "l_region"     text: "Région"       Text "v_region"     text: "—"
      Text "l_musicien"   text: "Musicien"     Text "v_musicien"   text: "—"
      Text "l_lieu"       text: "Lieu, date"   Text "v_lieu"       text: "—"
      Text "l_accordage"  text: "Accordage"    Text "v_accordage"  text: "—"
      Text "l_licence"    text: "Licence"      Text "v_licence"    text: "—"
```
(six paires `Text`, une par ligne dans le fichier réel.)

---

## 4. Les fichiers — `Resources/Interface/NkNgoma/`

| fichier | § | contenu |
|---|---|---|
| `application.nkgui` | 5.1 | la carte |
| `theme.nkgui` | 2 | les jetons d'information |
| `composants_ngoma.nkgui` | 3 | les composants propres |
| `menus.nkgui` | 5.2 | la barre de menus |
| `barre_transport.nkgui` | 5.3 | transport, position, tempo, espaces, CPU |
| `accueil.nkgui` | 5.4 | l'accueil |
| `principale.nkgui` | 5.5 | la fenêtre : bibliothèque, zone d'espace, détails, barre d'état |
| `composer.nkgui`, `rack.nkgui`, `rouleau.nkgui`, `playlist.nkgui` | 5.6 | l'espace Composer |
| `accordage.nkgui` | 5.7 | le panneau d'accordage |
| `instrument_echantillonneur.nkgui`, `instrument_synthe.nkgui`, `chaine_effets.nkgui` | 5.8 | instruments et effets |
| `son_image.nkgui`, `couches.nkgui`, `dlg_adr.nkgui` | 5.9 | l'espace Son à l'image |
| `mixer.nkgui` | 5.10 | l'espace Mixer |
| `jeu.nkgui` | 5.11 | l'espace Jeu |
| `dlg_exporter.nkgui` | 5.12 | l'export |
| `bibliotheque.nkgui` | 5.13 | la bibliothèque |
| `reglages_audio.nkgui`, `assistant_demarrage.nkgui` | 5.14 | réglages, premier lancement |
| `tests/NkNgoma.tests.nkgui` | 5.15 | les premiers tests |

📌 **Chaque document inclut** `theme.nkgui`, `../Commun/composants.nkgui` puis
`composants_ngoma.nkgui`, **dans cet ordre**, avant sa section `widgets`.

---

## 5. Les arbres

### 5.1 `application.nkgui` — la carte

```nkgui
nkgui 0.4
application "NkNgoma" {
  plateforme = Bureau
  depart     = "accueil"
  ecran "accueil"            { document = "accueil.nkgui"                type = Fenetre }
  ecran "principale"         { document = "principale.nkgui"             type = Fenetre }
  ecran "accordage"          { document = "accordage.nkgui"              type = Feuille }
  ecran "echantillonneur"    { document = "instrument_echantillonneur.nkgui" type = Panneau }
  ecran "synthe"             { document = "instrument_synthe.nkgui"      type = Panneau }
  ecran "couches"            { document = "couches.nkgui"                type = Panneau }
  ecran "adr"                { document = "dlg_adr.nkgui"                type = Dialogue }
  ecran "exporter"           { document = "dlg_exporter.nkgui"           type = Dialogue }
  ecran "reglages_audio"     { document = "reglages_audio.nkgui"         type = Dialogue }
  ecran "assistant"          { document = "assistant_demarrage.nkgui"    type = Dialogue }
  themes { embarques = ["Rihen UE5", "GitHub Pro", "Contraste élevé"]  defaut = "Rihen UE5"
           variante = systeme  choix_utilisateur = true  greffons = true }
}
```
📌 Les **quatre espaces** sont des **documents montés dans la zone d'espace** de
`principale` (un `Host "principale.zone"` qui monte `composer`, `son_image`, `mixer` ou
`jeu`), pas des écrans de la carte : changer d'espace ne change pas de fenêtre.

### 5.2 `menus.nkgui`

```
MenuBar "menus"
  Menu "menu_fichier"                          label: "Fichier"
    Menu "menu_nouveau"                        label: "Nouveau"
      MenuItem "app.nouveau_chanson"           label: "Chanson"                      shortcut: "Ctrl+N"
      MenuItem "app.nouveau_bo"                label: "Bande originale…"
      MenuItem "app.nouveau_sequence"          label: "Son de séquence (session son)…"
      MenuItem "app.nouveau_jeu"               label: "Musique de jeu"
      MenuItem "app.nouveau_podcast"           label: "Podcast / voix"
      MenuItem "app.nouveau_gabarit"           label: "Depuis un gabarit…"
    MenuItem "app.ouvrir"                      label: "Ouvrir…"                      shortcut: "Ctrl+O"
    Menu "menu_recents"                        label: "Ouvrir récent"                (rempli par l'application)
    Separator "s1"
    MenuItem "app.enregistrer"                 label: "Enregistrer"                  shortcut: "Ctrl+S"
    MenuItem "app.enregistrer_sous"            label: "Enregistrer sous…"            shortcut: "Ctrl+Maj+S"
    MenuItem "app.archiver"                    label: "Archiver le projet…"
    Separator "s2"
    Menu "menu_importer"                       label: "Importer"
      MenuItem "app.importer_audio"            label: "Audio…"
      MenuItem "app.importer_midi"             label: "MIDI…"
      MenuItem "session.importer"              label: "Session son…"
      MenuItem "session.importer_video"        label: "Vidéo de référence…"
      MenuItem "session.importer_reperes"      label: "Liste de repères…"
    MenuItem "export.ouvrir"                   label: "Exporter…"                    shortcut: "Ctrl+R"
    MenuItem "session.exporter"                label: "Exporter la session son…"
    Separator "s3"
    MenuItem "reglages.ouvrir"                 label: "Réglages audio et MIDI…"      shortcut: "Ctrl+,"
    MenuItem "app.quitter"                     label: "Quitter"                      shortcut: "Ctrl+Q"
  Menu "menu_edition"                          label: "Édition"
    MenuItem "edition.annuler"                 label: "Annuler"                      shortcut: "Ctrl+Z"
    MenuItem "edition.retablir"                label: "Rétablir"                     shortcut: "Ctrl+Y"
    Separator "s4"
    MenuItem "edition.couper"                  label: "Couper"                       shortcut: "Ctrl+X"
    MenuItem "edition.copier"                  label: "Copier"                       shortcut: "Ctrl+C"
    MenuItem "edition.coller"                  label: "Coller"                       shortcut: "Ctrl+V"
    MenuItem "edition.dupliquer"               label: "Dupliquer"                    shortcut: "Ctrl+D"
    MenuItem "edition.supprimer"               label: "Supprimer"                    shortcut: "Suppr"
    MenuItem "edition.tout_selectionner"       label: "Tout sélectionner"            shortcut: "Ctrl+A"
    MenuItem "edition.renommer"                label: "Renommer"                     shortcut: "F2"
  Menu "menu_ajouter"                          label: "Ajouter"
    MenuItem "canal.ajouter_echantillonneur"   label: "Échantillonneur"
    MenuItem "canal.ajouter_rythmes"           label: "Boîte à rythmes"
    MenuItem "canal.ajouter_synthe"            label: "Synthé soustractif"
    MenuItem "canal.ajouter_fm"                label: "Synthé FM"
    MenuItem "canal.ajouter_table"             label: "Synthé à table d'ondes"
    MenuItem "canal.ajouter_808"               label: "Basse 808 / log drum"
    Menu "menu_ajouter_afrique"                label: "Instrument africain"          (rempli par l'application : les banques)
    Menu "menu_ajouter_greffon"                label: "Greffon"                      (rempli par l'application)
    Separator "s5"
    MenuItem "design.ajouter_procedural"       label: "Effet procédural…"
    MenuItem "playlist.ajouter_piste"          label: "Piste"
    MenuItem "mix.ajouter_bus"                 label: "Bus"
  Menu "menu_composer"                         label: "Composer"
    MenuItem "espace.playlist"                 label: "Playlist"                     shortcut: "F5"
    MenuItem "espace.rack"                     label: "Rack de canaux"               shortcut: "F6"
    MenuItem "espace.rouleau"                  label: "Rouleau de piano"             shortcut: "F7"
    Separator "s6"
    MenuItem "pattern.nouveau"                 label: "Nouveau pattern"
    MenuItem "pattern.cloner"                  label: "Cloner le pattern"
    MenuItem "rouleau.quantifier"              label: "Quantifier"                   shortcut: "Q"
    MenuItem "rouleau.quantifier_reglages"     label: "Quantifier…"                  shortcut: "Alt+Q"
    MenuItem "rouleau.humaniser"               label: "Humaniser…"
    Menu "menu_gamme"                          label: "Gamme"                        (rempli par l'application)
    MenuItem "accordage.ouvrir"                label: "Accordage de l'instrument…"
    MenuItem "playlist.figer"                  label: "Figer en audio"
    MenuItem "playlist.defiger"                label: "Défiger"
    MenuItem "transport.mode_pattern"          label: "Jouer le pattern / le morceau"   shortcut: "L"
  Menu "menu_son"                              label: "Son"
    MenuItem "espace.son"                      label: "Son à l'image"                shortcut: "F8"
    MenuItem "session.maj_reperes"             label: "Mettre à jour les repères…"
    MenuItem "adr.ouvrir"                      label: "Boucle ADR…"
    MenuItem "design.couches"                  label: "Couches du bruitage…"
    MenuItem "design.variations"               label: "Variations…"
  Menu "menu_mixer"                            label: "Mixer"
    MenuItem "espace.mixer"                    label: "Mixer"                        shortcut: "F9"
    MenuItem "mix.instantane_a"                label: "Instantané A"
    MenuItem "mix.instantane_b"                label: "Instantané B"
    MenuItem "mix.comparer"                    label: "Comparer à sonie égale"
    Menu "menu_format"                         label: "Format"
      MenuItem "mix.format_stereo"             label: "Stéréo"
      MenuItem "mix.format_51"                 label: "5.1"
      MenuItem "mix.format_71"                 label: "7.1"
      MenuItem "mix.format_binaural"           label: "Binaural"
  Menu "menu_jeu"                              label: "Jeu"
    MenuItem "espace.jeu"                      label: "Jeu"                          shortcut: "F10"
    MenuItem "jeu.ajouter_etat"                label: "Ajouter un état"
    MenuItem "jeu.ajouter_parametre"           label: "Ajouter un paramètre…"
    MenuItem "jeu.ajouter_stinger"             label: "Ajouter un stinger…"
    MenuItem "jeu.simuler"                     label: "Simuler"
    MenuItem "jeu.exporter_nogee"              label: "Exporter vers Nogee…"
  Menu "menu_outils"                           label: "Outils"
    MenuItem "reglages.mesurer_latence"        label: "Mesurer la latence…"
    MenuItem "reglages.midi_apprendre"         label: "Apprendre une commande MIDI"
    MenuItem "biblio.analyser"                 label: "Analyser la bibliothèque"
  Menu "menu_ia"                               label: "IA"
    MenuItem "ia.assistant"                    label: "Assistant"                    shortcut: "Ctrl+Maj+K"
    MenuItem "ia.transcrire"                   label: "Transcrire en notes"
    MenuItem "ia.separer"                      label: "Séparer les sources"
    MenuItem "ia.conseils_mix"                 label: "Conseils de mix"
  Menu "menu_fenetre"                          label: "Fenêtre"
    MenuItem "fenetre.bibliotheque"            label: "Bibliothèque"
    MenuItem "fenetre.details"                 label: "Détails"
    MenuItem "fenetre.console"                 label: "Console"
    MenuItem "fenetre.reinitialiser"           label: "Réinitialiser la disposition"
  Menu "menu_aide"                             label: "Aide"
    MenuItem "commun.aide"                     label: "Documentation"                shortcut: "F1"
    MenuItem "commun.aide_raccourcis"          label: "Raccourcis clavier…"
    MenuItem "commun.apropos"                  label: "À propos"
```

### 5.3 `barre_transport.nkgui`

```
HBox "transport"                               gap: 8   align: Center   minSize: (0, 40)
  HBox "tr.boutons"                            gap: 2
    Button "transport.arret"                   icon: square       tooltip: "Arrêt"
    ToggleButton✚ "transport.lecture"          icon: play         bind: transport.joue      tooltip: "Lecture (Espace)"
    ToggleButton✚ "transport.enregistrer"      icon: circle       bind: transport.enregistre tooltip: "Enregistrer (R)"
    ToggleButton✚ "transport.boucle"           icon: repeat       bind: transport.boucle    tooltip: "Boucle"
  TimecodeField✚ "tr.position"                 bind: transport.position   format: Mesures   tooltip: "Position — clic : mesures / timecode"
  TimecodeField✚ "tr.timecode"                 bind: transport.position   format: Timecode
  NumberField "tr.tempo"                       bind: projet.tempo   valueType: Float   step: 0.01   min: 20   max: 400   tooltip: "Tempo — clic maintenu : taper"
  TextField "tr.signature"                     bind: projet.signature   placeholder: "12/8"   maxChars: 5
  SplitButton✚ "transport.metronome"           icon: metronome   label: ""   tooltip: "Métronome"
    MenuItem "transport.precompte_1"           label: "Pré-décompte : 1 mesure"
    MenuItem "transport.precompte_2"           label: "Pré-décompte : 2 mesures"
  Separator "tr.s1"                            orientation: Vertical
  RadioGroup "espace.choix"                    bind: espace.actif   options: ["Composer","Son","Mixer","Jeu"]   orientation: Horizontal
  Spacer "tr.pousse"
  Button "reglages.ouvrir"                     id: "reglages.ouvrir"   label: "CPU 0 % · 0,0 ms"   tooltip: "Latence mesurée — ouvrir les réglages audio"
```
- **Comportements** :
  ```nkgui
  behavior "espace.choix"(Changed) { set "principale.zone".hint = espace.actif }   // l'application monte le document de l'espace
  behavior "transport.enregistrer" {                                               // continu : le liseré
    if transport.enregistre { show "tr.liseret_rec" } else { hide "tr.liseret_rec" }
  }
  behavior "reglages.ouvrir" {
    if audio.latence_mesuree == 0 { set "reglages.ouvrir".label = "CPU " + audio.cpu + " % · latence non mesurée" }
    else { set "reglages.ouvrir".label = "CPU " + audio.cpu + " % · " + audio.latence_mesuree + " ms" }
  }
  ```
  (et un `Separator "tr.liseret_rec"` en bas de la barre, `appearance: fill #F85149 // @info.enregistrement`, `visible: false`.)
- `transport.enregistrer` actif : fond `@info.enregistrement`.
- **La latence affichée est la valeur mesurée** (01 §5.3) ; tant qu'aucune mesure n'a été
  faite, le libellé dit « latence non mesurée » en `@info.alerte`.
- `tr.tempo` : `NumberField` non monté — attendu ; en attendant, `TextField`.

### 5.4 `accueil.nkgui`

```
Window "accueil"                               title: "NkNgoma"
  Splitter "ac.partage"                        orientation: Vertical   ratio: 0.18
    VBox "ac.nav"                              gap: 2
      Image "ac.logo"                          source: "nkngoma_logo"   size: (48, 48)
      ListBox "ac.section"                     bind: accueil.section   items: ["Projets","Gabarits","Bibliothèque","Apprendre","Réglages audio"]
    VBox "ac.contenu"                          gap: 8
      HBox "ac.actions"                        gap: 8
        SplitButton✚ "app.nouveau"             label: "+ Nouveau"   icon: plus
          MenuItem "app.nouveau_chanson"       label: "Chanson"
          MenuItem "app.nouveau_bo"            label: "Bande originale…"
          MenuItem "app.nouveau_sequence"      label: "Son de séquence…"
          MenuItem "app.nouveau_jeu"           label: "Musique de jeu"
          MenuItem "app.nouveau_podcast"       label: "Podcast / voix"
        Button "app.ouvrir"                    label: "Ouvrir…"   icon: folder-open
      Scroll "ac.defil"                        axis: Y
        Flow "ac.cartes"                       gap: 8     (CarteProjet par projet, ajoutées par l'application)
      Button "reglages.ouvrir_depuis_accueil"  label: "● Audio : —"   tooltip: "Réglages audio"
```
- `reglages.ouvrir_depuis_accueil.label` = « ● Audio : pilote · fréquence · bloc · latence
  mesurée » — comportement continu ✅.

### 5.5 `principale.nkgui`

```
Window "principale"                            title: "NkNgoma"
  VBox "pr.colonne"                            gap: 0
    Host "pr.menus"                            hint: "menus.nkgui"
    Host "pr.transport"                        hint: "barre_transport.nkgui"
    DockSpace "pr.dock"
      Panel "bibliotheque"                     title: "Bibliothèque"     (bibliotheque.nkgui)
      Host "principale.zone"                   hint: "le document de l'espace actif : composer, son_image, mixer, jeu"
      Panel "details"                          title: "Détails"
        Host "details.contenu"                 hint: "panneau Détails de la famille, engendré depuis la réflexion : canal, note, clip, effet, voie, état"
    HBox "barre_etat"                          gap: 8   align: Center
      Button "fenetre.console"                 icon: terminal   label: "Console"
      Button "ia.assistant"                    icon: sparkles   label: "IA"
      Badge✚ "be.propositions"                 text: "0"   tone: info.apercu   visible: false
      Text "be.sonie"                          text: "Sonie — LUFS"
      Meter✚ "be.sortie"                       bind: master.niveau   channels: 2   orientation: Horizontal
      Spacer "be.pousse"
      Text "be.etat"                           text: "Prêt"
```
- `be.propositions.visible = (ia.en_attente > 0)` ✅ ; `be.sonie` en `@son.sonie_ok` /
  `@son.sonie_hors` selon la cible ✅.
- Dispositions (P10, écrit, compté) : `bibliotheque` gauche 0,16 · zone centre · `details`
  droite 0,22.

### 5.6 L'espace Composer

#### `composer.nkgui` (cadre)

```
DockSpace "composer"
  Panel "playlist"                             title: "Playlist"          (playlist.nkgui)
  Panel "rack"                                 title: "Rack de canaux"    (rack.nkgui)
  Panel "rouleau"                              title: "Rouleau de piano"  (rouleau.nkgui)
```
Dispositions : `playlist` haut 0,55 · `rack` bas-gauche 0,40 · `rouleau` bas-droite.
`F5` / `F6` / `F7` donnent le focus au panneau et l'**ouvrent** s'il est fermé.

#### `rack.nkgui`

```
VBox "rack"                                    gap: 2
  HBox "rk.barre"                              gap: 4   align: Center
    Dropdown "pattern.courant"                 bind: pattern.courant   tooltip: "Pattern édité"
    Button "pattern.nouveau"                   icon: plus   tooltip: "Nouveau pattern"
    NumberField "rk.pas"                       bind: pattern.nb_pas   valueType: Int   min: 1   max: 64   tooltip: "Pas"
    Text "rk.signature"                        text: "12/8 · 4 × 3"
    Dropdown "rk.groove"                       bind: pattern.groove   tooltip: "Groove"   (items : gabarits — bikutsi, makossa…)
    Slider "rk.groove_force"                   bind: pattern.groove_force   min: 0   max: 100   tooltip: "Force du groove (%)"
  Scroll "rk.defil"                            axis: Y
    VBox "rk.canaux"                           gap: 1     (LigneCanal par canal, ajoutées par l'application)
  SplitButton✚ "canal.ajouter"                 label: "+ Canal"   icon: plus
    MenuItem "canal.ajouter_echantillonneur"   label: "Échantillonneur"
    MenuItem "canal.ajouter_rythmes"           label: "Boîte à rythmes"
    MenuItem "canal.ajouter_synthe"            label: "Synthé"
```
- Menu du canal (clic droit, `ContextMenu` non monté — écris-le) : `canal.ouvrir`,
  `canal.renommer`, `canal.couleur`, `canal.dupliquer`, `canal.supprimer`,
  `canal.vers_rouleau`, `canal.remplir_2`, `canal.remplir_4`, `canal.decaler`.
- Menu d'un pas : `pas.velocite`, `pas.hauteur`, `pas.decalage`, `pas.probabilite`,
  `pas.repetition`.

#### `rouleau.nkgui`

```
VBox "rouleau"                                 gap: 2
  HBox "rl.barre"                              gap: 2   align: Center
    RadioGroup "rouleau.outil"                 bind: rouleau.outil   options: ["Crayon","Pinceau","Sélection","Coupe","Glissando","Sourdine","Zoom"]   orientation: Horizontal
    Separator "rl.s1"                          orientation: Vertical
    SplitButton✚ "rouleau.accord"              label: "Accord"
    SplitButton✚ "rouleau.arpege"              label: "Arpège"
    Button "rouleau.quantifier"                label: "Quantifier"   tooltip: "Q"
    Button "rouleau.humaniser"                 label: "Humaniser"
    Dropdown "rl.gamme"                        bind: rouleau.gamme   tooltip: "Gamme aimantée"
    Button "accordage.ouvrir"                  label: "Accordage : —"   tooltip: "Accordage de l'instrument"
    Spacer "rl.pousse"
    ToggleButton✚ "rouleau.fantomes"           icon: ghost   bind: rouleau.fantomes   tooltip: "Pistes fantômes"
  HBox "rl.propo"                              gap: 4   visible: false
    Text "rl.propo_txt"                        text: "L'IA propose 16 notes."
    Button "ia.appliquer"                      label: "Appliquer"
    Button "ia.rejeter"                        label: "Rejeter"
  Host "rl.notes"                              hint: "rouleau : rangées = touches OU lames de l'instrument (accordage) ; hors gamme en @echelle.hors ; notes colorées par vélocité, liseré d'origine ; tête de lecture"
  Host "rl.sous_notes"                         hint: "vélocité, articulation, automation choisie"
```
- `accordage.ouvrir.label` = « Accordage : <nom> » ; fond `@accordage.ecart` si l'instrument
  n'est pas au tempérament du projet ✅.
- `rl.propo.visible = (rouleau.propositions > 0)` ✅ ; `rl.propo` en
  `appearance: fill #A371F759 // @info.apercu  pattern✚: Hatch`.

#### `playlist.nkgui`

```
VBox "playlist"                                gap: 2
  HBox "pl.barre"                              gap: 2   align: Center
    RadioGroup "playlist.outil"                bind: playlist.outil   options: ["Crayon","Pinceau","Coupe","Sourdine","Étirer"]   orientation: Horizontal
    Dropdown "pl.grille"                       bind: playlist.grille   items: ["Mesure","Temps","1/2","1/4","1/8","1/16","Libre"]
    Dropdown "pl.motif"                        bind: playlist.motif_pose   tooltip: "Pattern à poser"
    Spacer "pl.pousse"
    Button "playlist.ajouter_piste"            icon: plus   tooltip: "Piste"
  Host "pl.marqueurs"                          hint: "marqueurs de section ; tempo et signature variables"
  Host "pl.pistes"                             hint: "timeline de la famille : clips de pattern, d'audio, d'automation ; couleurs = données ; tête de lecture"
```

### 5.7 `accordage.nkgui`

```
Panel "accordage"                              title: "Accordage"
  VBox "ag.colonne"                            gap: 6
    Text "ag.nom"                              text: "—"      (« Balafon Ewondo — mesuré le 12/10 »)
    Host "ag.courbe"                           hint: "une barre par lame : écart en cents au tempérament égal ; référence du projet"
    Grid "ag.choix"                            columns: 2   sizes: [0.4, 0.6]   gap: 4
      Text "ag.l_ref"                          text: "Référence du projet"
      Dropdown "ag.reference"                  bind: projet.accordage   (items : Égal 440 Hz, Égal 432 Hz, accordages de la bibliothèque)
    HBox "ag.boutons"                          gap: 4   justify: End
      Button "accordage.garder"                label: "Garder l'accordage de l'instrument"
      Button "accordage.reaccorder"            label: "Réaccorder vers la référence"
      Button "accordage.importer"              label: "Importer (.scl)…"
    Text "ag.note"                             text: "Aucun accordage n'est changé sans ce choix."   wrap: true
```

### 5.8 Instruments et effets

#### `instrument_echantillonneur.nkgui`

```
Panel "echantillonneur"                        title: "Échantillonneur"
  VBox "ec.colonne"                            gap: 6
    HBox "ec.presets"                          gap: 4
      Dropdown "instr.preset"                  bind: instr.preset
      Button "instr.preset_enregistrer"        icon: save   tooltip: "Enregistrer le préréglage"
      ToggleButton✚ "instr.ab"                 label: "A/B"   bind: instr.compare
    TabBar "ec.onglets"                        bind: instr.onglet   tabs: ["Zones","Enveloppe","Filtre","Articulations","Accordage","Fiche"]
    Host "ec.zones"                            hint: "grille clavier × vélocité, zones glissables ; forme d'onde de la zone choisie ; alternances"
    Categorie "ec.cat_env"                     label: "Enveloppe"
      HBox "ec.adsr"                           gap: 6
        Knob✚ "ec.a"                           bind: instr.env.a   min: 0   max: 5000   unit: ms   tooltip: "Attaque"
        Knob✚ "ec.d"                           bind: instr.env.d   min: 0   max: 5000   unit: ms   tooltip: "Déclin"
        Knob✚ "ec.s"                           bind: instr.env.s   min: 0   max: 1                  tooltip: "Maintien"
        Knob✚ "ec.r"                           bind: instr.env.r   min: 0   max: 10000  unit: ms   tooltip: "Relâchement"
    CarteFiche "ec.fiche"
```
- Une seule page visible selon `ec.onglets` (comportement continu ✅).

#### `instrument_synthe.nkgui`

Même cadre ; onglets `Oscillateurs · Filtre · Enveloppes · LFO · Modulation · Effets` ;
chaque réglage est un `Knob✚` dans une `Categorie` ; la **matrice de modulation** est un
`Host`.

#### `chaine_effets.nkgui`

```
VBox "chaine"                                  gap: 2
  Host "ch.inserts"                            hint: "inserts réordonnables par glisser ; activation ; latence de l'effet si non nulle ; dépli en place"
  SplitButton✚ "effet.ajouter"                 label: "+ Effet"   icon: plus
    Menu "ch.dynamique"   label: "Dynamique"   (rempli par l'application)
    Menu "ch.egalisation" label: "Égalisation"
    Menu "ch.espace"      label: "Espace"
    Menu "ch.modulation"  label: "Modulation"
    Menu "ch.distorsion"  label: "Distorsion"
    Menu "ch.hauteur"     label: "Hauteur et temps"
    Menu "ch.greffons"    label: "Greffons"
```

### 5.9 L'espace Son à l'image

#### `son_image.nkgui`

```
VBox "son_image"                               gap: 0
  Splitter "si.partage"                        orientation: Horizontal   ratio: 0.42
    HBox "si.haut"                             gap: 8
      Host "si.video"                          hint: "vidéo de référence, image courante ; détachable"
      VBox "si.infos"                          gap: 4
        TimecodeField✚ "si.tc"                 bind: transport.position   format: Timecode
        Text "si.cadence"                      text: "24 i/s"
        SplitButton✚ "session.menu"            label: "Session"
          MenuItem "session.importer"          label: "Importer une session son…"
          MenuItem "session.exporter"          label: "Exporter la session son…"
          MenuItem "session.importer_video"    label: "Importer une vidéo…"
          MenuItem "session.importer_reperes"  label: "Importer des repères…"
        Button "session.maj_reperes"           label: "⟲ Mettre à jour les repères"
    Host "si.timeline"                         hint: "timeline de la famille : IMAGE (vignettes), REPÈRES (@repere.image), DIAL, FX, AMB, MUS ; bruitages calés portent une ancre"
```
- `session.maj_reperes` n'ouvre pas d'écran : c'est un `message` qui liste ce qui a
  bougé, puis applique **sur réponse** :
  ```nkgui
  behavior "session.maj_reperes"(Click) {
    call "fichier.ouvrir"() -> chemin {
      Callback "session.comparer_reperes"(chemin)
      message "Mettre à jour les repères ?" session.resume_changements buttons ["Appliquer", "Annuler"] -> r {
        if r == "Appliquer" { Callback "session.appliquer_reperes"() }
      }
    } else annule { }
  }
  ```

#### `couches.nkgui`

```
Panel "couches"                                title: "Couches du bruitage"
  VBox "cc.colonne"                            gap: 4
    Host "cc.pile"                             hint: "une couche par ligne : son, décalage, volume, égalisation ; forme d'onde composée en bas"
    Categorie "cc.cat_var"                     label: "Variations"
      LigneNombre "cc.nb"                      etiquette: "Nombre"             bind: design.variations
      LigneNombre "cc.hauteur"                 etiquette: "Dispersion hauteur" bind: design.dispersion_hauteur
      LigneNombre "cc.timbre"                  etiquette: "Dispersion timbre"  bind: design.dispersion_timbre
    HBox "cc.boutons"                          gap: 4   justify: End
      Button "design.exporter_un"              label: "Exporter en un son"
      Button "design.exporter_couches"         label: "Exporter en couches"
```

#### `dlg_adr.nkgui`

```
Window "adr"                                   title: "Boucle ADR"   modal: true
  VBox "ad.colonne"                            gap: 6
    Text "ad.replique"                         text: "—"
    LigneListe "ad.bips"                       etiquette: "Décompte"   bind: adr.bips   items: ["Aucun","3 bips","Visuel"]
    LigneCase "ad.guide"                       etiquette: "Réplique originale au casque"   bind: adr.guide
    HBox "ad.boutons"                          gap: 4   justify: End
      Button "adr.annuler"                     label: "Fermer"
      BoutonEcrit "adr.enregistrer"            label: "● Enregistrer les prises"
```

### 5.10 `mixer.nkgui`

```
VBox "mixer"                                   gap: 4
  HBox "mx.barre"                              gap: 4   align: Center
    Dropdown "mx.format"                       bind: mix.format   items: ["Stéréo","5.1","7.1","Binaural"]
    Spacer "mx.pousse"
    RadioGroup "mx.instantane"                 bind: mix.instantane   options: ["A","B","C","D"]   orientation: Horizontal
    Button "mix.comparer"                      label: "Comparer à sonie égale"
    Dropdown "mx.cible"                        bind: mix.cible_sonie   items: ["R128 (−23)","Streaming (−14)","Perso"]
  Splitter "mx.partage"                        orientation: Vertical   ratio: 0.82
    Scroll "mx.defil"                          axis: X
      HBox "mx.voies"                          gap: 2     (TrancheMixeur par voie, puis bus, puis master — ajoutées par l'application)
    VBox "mx.mesures"                          gap: 4
      Meter✚ "mx.sonie"                        bind: master.sonie   tooltip: "Sonie intégrée / court terme / momentanée"
      Text "mx.truepeak"                       text: "True peak : — dBTP"
      Host "mx.correlation"                    hint: "corrélation de phase"
      Host "mx.spectre"                        hint: "spectre"
```
- `mx.sonie` et `be.sonie` : `@son.sonie_ok` / `@son.sonie_hors` ✅.
- En 5.1 / 7.1, le `Knob✚ "pan"` des tranches est remplacé par un `Host "joystick"` (panoramique
  surround) — l'application choisit selon `mix.format`.

### 5.11 `jeu.nkgui`

```
VBox "jeu"                                     gap: 4
  HBox "jx.barre"                              gap: 4   align: Center
    Button "jeu.ajouter_etat"                  label: "+ État"
    Button "jeu.ajouter_parametre"             label: "+ Paramètre"
    Button "jeu.ajouter_stinger"               label: "+ Stinger"
    Spacer "jx.pousse"
    Button "jeu.exporter_nogee"                label: "Exporter vers Nogee…"   icon: gamepad-2
  Splitter "jx.partage"                        orientation: Vertical   ratio: 0.45
    Host "jx.graphe"                           hint: "états en boîtes, transitions étiquetées (quand, comment) ; état joué en @jeu.etat_actif"
    VBox "jx.couches"                          gap: 2
      Text "jx.titre"                          text: "Couches — état : —"
      Scroll "jx.defil"                        axis: Y
        VBox "jx.lignes"                       gap: 1   (LigneCouche par couche)
      Host "jx.stingers"                       hint: "événement → motif → calage"
  HBox "jx.simulation"                         gap: 8   align: Center
    Host "jx.parametres"                       hint: "un curseur par paramètre, un bouton ⚡ par événement"
    ToggleButton✚ "jeu.simuler"                label: "▶ Jouer"   bind: jeu.simulation
    ToggleButton✚ "jeu.enregistrer_parcours"   label: "● Enregistrer un parcours"   bind: jeu.enregistre_parcours
    Text "jx.etat"                             text: "état : —"
```
- `jeu.exporter_nogee` désactivé **avec sa raison** s'il n'y a aucun état :
  ```nkgui
  behavior "jeu.exporter_nogee" {
    if jeu.nb_etats == 0 { disable "jeu.exporter_nogee" because "Ajoute au moins un état" }
    else { enable "jeu.exporter_nogee" }
  }
  ```

### 5.12 `dlg_exporter.nkgui`

```
Window "exporter"                              title: "Exporter"   modal: true
  VBox "ex.colonne"                            gap: 8
    Categorie "ex.quoi"                        label: "Quoi"
      LigneListe "ex.portee"                   etiquette: "Portée"   bind: export.portee
                                               items: ["Morceau","Sélection","Boucle","Stems","Pistes séparées","Musique interactive","Banque de sons"]
    Categorie "ex.format"                      label: "Format"
      LigneListe "ex.codec"                    etiquette: "Format"     bind: export.codec   items: ["WAV","FLAC","OGG","Opus","MP3"]
      LigneListe "ex.bits"                     etiquette: "Bits"       bind: export.bits    items: ["16","24","32 flottant"]
      LigneListe "ex.frequence"                etiquette: "Fréquence"  bind: export.frequence items: ["44,1 kHz","48 kHz","96 kHz","192 kHz"]
      LigneListe "ex.canaux"                   etiquette: "Canaux"     bind: export.canaux  items: ["Stéréo","5.1","7.1","Binaural"]
    Categorie "ex.sonie"                       label: "Sonie"
      LigneListe "ex.cible"                    etiquette: "Cible"      bind: export.cible   items: ["Aucune","R128 (−23)","Streaming (−14)","Perso"]
      LigneCase "ex.ajuster"                   etiquette: "Ajuster à la cible (gain seul)"   bind: export.ajuster
    Categorie "ex.vers"                        label: "Vers"
      LigneListe "ex.destination"              etiquette: "Destination"   bind: export.destination   items: ["Fichier","NKScena","Nogee","NkAntenne","NkAnima"]
    Text "ex.resume"                           text: "—"   wrap: true
    HBox "ex.boutons"                          gap: 4   justify: End
      Button "export.annuler"                  label: "Annuler"
      BoutonEcrit "export.lancer"              label: "Exporter"
```
- `ex.resume` : la phrase « ce qui sera écrit » (02 §9) — comportement continu ✅.
- Encodeurs non écrits (tout sauf WAV aujourd'hui) : l'option est **grisée avec sa raison**
  « Encodeur pas encore disponible » — l'application filtre les `items` ; si elle ne le
  peut pas, `export.lancer` est désactivé avec cette raison :
  ```nkgui
  behavior "export.lancer" {
    if export.codec != "WAV" && not export.encodeur_disponible {
      disable "export.lancer" because "Encodeur pas encore disponible — choisis WAV"
    } else { enable "export.lancer" }
  }
  ```

### 5.13 `bibliotheque.nkgui`

```
VBox "bibliotheque"                            gap: 4
  TextField "bi.recherche"                     bind: biblio.recherche   placeholder: "Rechercher…"   icon✚: search
  HBox "bi.filtres"                            gap: 2
    ToggleButton✚ "biblio.tempo_compatible"    label: "Tempo"      bind: biblio.f_tempo
    ToggleButton✚ "biblio.tonalite_compatible" label: "Tonalité"   bind: biblio.f_tonalite
    Dropdown "bi.licence"                      bind: biblio.f_licence   items: ["Toutes","Redistribuables","CC0"]
  Host "bi.arbre"                              hint: "Instruments · Afrique (en tête) · Électronique · Orchestral · Échantillons · Boucles · Motifs · Bruitages (UCS) · RI · Accordages · Préréglages · Projet · Greffons ; aperçu au clic, glisser vers le rack / la playlist / la timeline ; cadenas si non redistribuable"
  CarteFiche "bi.fiche"
```

### 5.14 `reglages_audio.nkgui` et `assistant_demarrage.nkgui`

```
Window "reglages_audio"                        title: "Réglages audio et MIDI"   modal: true
  Splitter "ra.partage"                        orientation: Vertical   ratio: 0.25
    ListBox "ra.rubrique"                      bind: reglages.rubrique   items: ["Sortie / Entrée","Latence","MIDI","Métronome","Performance","Bibliothèque","IA"]
    VBox "ra.page"                             gap: 6
      LigneListe "ra.pilote"                   etiquette: "Pilote"          bind: audio.pilote   items: ["WASAPI partagé","WASAPI exclusif","CoreAudio","ALSA","JACK / PipeWire","ASIO"]
      LigneListe "ra.sortie"                   etiquette: "Sortie"          bind: audio.sortie
      LigneListe "ra.entree"                   etiquette: "Entrée"          bind: audio.entree
      LigneListe "ra.frequence"                etiquette: "Fréquence"       bind: audio.frequence   items: ["44,1 kHz","48 kHz","96 kHz"]
      LigneListe "ra.bloc"                     etiquette: "Taille de bloc"  bind: audio.bloc        items: ["64","128","256","512","1024"]
      HBox "ra.latence"                        gap: 6   align: Center
        Text "ra.latence_val"                  text: "Latence : non mesurée"
        Button "reglages.mesurer_latence"      label: "Mesurer…"
      Host "ra.autres"                         hint: "rubriques MIDI, métronome, performance, bibliothèque, IA"
```
- `ASIO` : option **désactivée avec sa raison** « Licence ASIO à obtenir (décision D4) » —
  l'application filtre ; ⚠️ désactiver **une option** d'un `Dropdown` n'existe pas au
  format : signale-le.
- `ra.latence_val` : « Latence : 6,8 ms (mesurée le … ) » ; `@info.alerte` si non mesurée ✅.

`assistant_demarrage.nkgui` : trois pages (`ListBox` des étapes ou `Stepper✚` P22) —
Sortie · Entrée et **mesure de latence** · Clavier MIDI (facultatif) ; boutons
`accueil.precedent`, `accueil.suivant`, `accueil.terminer`.

### 5.15 `tests/NkNgoma.tests.nkgui` — les premiers tests

```nkgui
nkgui 0.4
include "../application.nkgui"

test "Exporter en FLAC sans encodeur est impossible, et dit pourquoi" {
  depart "exporter"
  select "ex.codec" "FLAC"
  expect disabled "export.lancer" because "Encodeur pas encore disponible — choisis WAV"
  select "ex.codec" "WAV"
  expect enabled "export.lancer"
}

test "Exporter vers Nogee demande au moins un état" {
  depart "principale"
  select "espace.choix" "Jeu"
  expect disabled "jeu.exporter_nogee" because "Ajoute au moins un état"
  click "jeu.ajouter_etat"
  expect enabled "jeu.exporter_nogee"
}

test "Mettre à jour les repères montre les changements avant d'appliquer" {
  depart "principale"
  select "espace.choix" "Son"
  doublure "fichier.ouvrir" -> "/sessions/sequence_12_v2.nkson"
  click "session.maj_reperes"
  expect message "Mettre à jour les repères ?"
  answer "Annuler"
  expect not called "session.appliquer_reperes"
}

test "La latence non mesurée se voit dans la barre de transport" {
  depart "principale" { audio.latence_mesuree = 0 }
  expect contains("reglages.ouvrir".text, "latence non mesurée")
  expect not visible "tr.liseret_rec"
}
```

---

## 6. Les actions — toutes ✚

| domaine | actions |
|---|---|
| **app** | `nouveau` · `nouveau_chanson` · `nouveau_bo` · `nouveau_sequence` · `nouveau_jeu` · `nouveau_podcast` · `nouveau_gabarit` · `ouvrir` · `enregistrer` · `enregistrer_sous` · `archiver` · `importer_audio` · `importer_midi` · `quitter` |
| **transport** | `arret` · `lecture` · `enregistrer` · `boucle` · `metronome` · `precompte_1` · `precompte_2` · `mode_pattern` |
| **espace** | `playlist` · `rack` · `rouleau` · `son` · `mixer` · `jeu` |
| **pattern** | `nouveau` · `cloner` |
| **canal** | `ajouter` · `ajouter_echantillonneur` · `ajouter_rythmes` · `ajouter_synthe` · `ajouter_fm` · `ajouter_table` · `ajouter_808` · `ouvrir` · `renommer` · `couleur` · `dupliquer` · `supprimer` · `vers_rouleau` · `remplir_2` · `remplir_4` · `decaler` |
| **pas** | `velocite` · `hauteur` · `decalage` · `probabilite` · `repetition` |
| **rouleau** | `accord` · `arpege` · `quantifier` · `quantifier_reglages` · `humaniser` · `fantomes` |
| **playlist** | `ajouter_piste` · `figer` · `defiger` |
| **accordage** | `ouvrir` · `garder` · `reaccorder` · `importer` |
| **instr** | `preset_enregistrer` · `ab` |
| **effet** | `ajouter` |
| **session** | `importer` · `exporter` · `importer_video` · `importer_reperes` · `maj_reperes` · `menu` |
| **design** | `ajouter_procedural` · `couches` · `variations` · `exporter_un` · `exporter_couches` |
| **adr** | `ouvrir` · `annuler` · `enregistrer` |
| **mix** | `ajouter_bus` · `instantane_a` · `instantane_b` · `comparer` · `format_stereo` · `format_51` · `format_71` · `format_binaural` |
| **jeu** | `ajouter_etat` · `ajouter_parametre` · `ajouter_stinger` · `simuler` · `enregistrer_parcours` · `exporter_nogee` · `couche_fondu` |
| **export** | `ouvrir` · `annuler` · `lancer` |
| **biblio** | `analyser` · `tempo_compatible` · `tonalite_compatible` |
| **reglages** | `ouvrir` · `ouvrir_depuis_accueil` · `mesurer_latence` · `midi_apprendre` |
| **accueil** | `ouvrir_projet` · `precedent` · `suivant` · `terminer` |
| **ia** | `assistant` · `transcrire` · `separer` · `conseils_mix` · `appliquer` · `rejeter` |
| **fenetre** | `bibliotheque` · `details` · `console` · `reinitialiser` |
| **edition** | `annuler` · `retablir` · `couper` · `copier` · `coller` · `dupliquer` · `supprimer` · `tout_selectionner` · `renommer` |
| **commun** | `aide` · `aide_raccourcis` · `apropos` |

`ToggleButton✚` à `bind` (sourdine, solo, armer, fantômes, filtres) : **états**, pas
actions — sauf ceux qui portent un identifiant de domaine ci-dessus (transport, jeu,
biblio), dont l'application sert **aussi** l'action.

### Raisons de grisé

| cas | raison |
|---|---|
| action non servie | « Pas encore disponible dans NkNgoma » |
| un encodeur non écrit | « Encodeur pas encore disponible — choisis WAV » |
| `ASIO` | « Licence ASIO à obtenir (décision D4) » |
| `jeu.exporter_nogee` sans état | « Ajoute au moins un état » |
| `session.maj_reperes` sans session | « Importe d'abord une session son ou une liste de repères » |
| `ia.*` sans modèle installé | « Aucun modèle installé — Réglages ▸ IA » |
| `accordage.reaccorder` sur un instrument sans accordage mesuré | « L'accordage de cet instrument n'est pas mesuré » |
| `transport.enregistrer` sans entrée | « Aucune entrée audio choisie — Réglages audio » |
| un élément non redistribuable dans un export de banque | « Licence : redistribution non permise » |

---

## 7. Ce que tu ne sais pas encore reproduire — écrit quand même

| élément | quoi | état |
|---|---|---|
| `Knob` · `Fader` · `Pad` | potentiomètre, fader gradué en dB, pad à vélocité | P29, P30, P31 — proposés |
| `Meter` | zones, crête, écrêtage, sonie | P14 |
| `TimecodeField` | position en mesures et en timecode | P13 |
| `SplitButton` | Nouveau ▾, Métronome ▾, + Effet ▾ | P18 |
| `Slider orientation` | fader vertical | compté non honoré |
| option désactivée **dans** un `Dropdown` | ASIO, encodeurs | pas au format — l'application filtre la liste |
| `ContextMenu` | menus du canal, du pas, du clip | non monté |
| témoin d'activité qui clignote | à chaque note | peint par l'application |
| hachures des propositions | `pattern✚: Hatch` | P11 |
| polices | `"Mono-12"` pour la position et le timecode | une seule fonte aujourd'hui |

---

## 8. L'ordre de livraison — aligné sur le doc 01 §14

| # | livrable | palier | témoin |
|---|---|---|---|
| 1 | `theme` + `composants_ngoma` + `barre_transport` + `principale` (cadre) + `rouleau` + `dlg_exporter` (WAV) | P0 | une mélodie jouée au clavier de l'ordinateur, entendue sans décalage mesurable, exportée en WAV ; la latence **mesurée** s'affiche dans la barre |
| 2 | `composer` + `rack` + `playlist` + `menus` + `accueil` + `reglages_audio` + `assistant_demarrage` | P1 | un morceau en 12/8 composé au séquenceur et au clavier MIDI |
| 3 | `accordage` + `instrument_echantillonneur` + `bibliotheque` (Afrique en tête, fiches) | P2 | un bikutsi au balafon dans son accordage mesuré ; la grille du rouleau suit ses lames |
| 4 | `mixer` + `chaine_effets` + `instrument_synthe` | P3 | une voix mixée et masterisée à −14 LUFS mesurés |
| 5 | `son_image` + `couches` + `dlg_adr` | P4 | le son d'une séquence NkAnima de 30 s, calé sur ses repères |
| 6 | `jeu` + **les tests du §5.15** | P5 | trois états qui passent à la mesure ; les tests passent, et **« Vérifier les tests »** (NKUIDesign) les fait échouer quand on retire les `disable … because` |
| 7 | l'IA dans `rouleau`, `playlist`, `mixer` | P6 | un pattern proposé hachuré, appliqué en une opération |
