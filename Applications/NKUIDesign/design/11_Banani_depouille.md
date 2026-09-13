# Document 11 — Dépouille de l'export Banani du 2026-08-30

> Source : `design/banani_export_2026-08-30.txt` (540 Ko, JSON : `flow` + 40
> `designs` + 8 `sharedFiles`). Dépouillé le 30/08 pour que l'implémentation
> se fasse sans relire les 540 Ko. Tout ce qui est écrit ici a été **lu dans
> le JSX**, rien n'est reconstitué de mémoire.
>
> **Garde-fou « Registre Forestier Gabon »** : c'est le NOM DU FLUX Banani
> (`flow.name`, url `app.banani.co/flow/CSPy_Z1zdbTH`) — un flux recyclé d'un
> autre projet, jamais renommé. **Aucun des 40 écrans ne concerne le registre
> forestier** ; il n'y a rien à mettre de côté au-delà de ce nom.
>
> **Correction de prémisse — les icônes ne sont PAS des imports Lucide.**
> Zéro `import … from 'lucide-react'` dans tout l'export (mesuré). Les ~670
> icônes sont des **SVG inline dessinés dans le code**, dans le style Lucide
> (trait 1-1.3 px, bouts ronds). C'est une bonne nouvelle : les primitives de
> chaque icône sont DÉJÀ dans l'export, prêtes à être transposées dans le
> vocabulaire du kit. Aucune police à embarquer, aucune question de licence
> bloquante (pour mémoire : Lucide est sous licence ISC, permissive — mais on
> redessine de toute façon, cohérence oblige, comme pour les 7 outils).

---

## 1. Les 12 composants

Les dimensions sont celles du JSX. Rappel de la règle du dépôt : **les
maquettes donnent la géométrie, pas les couleurs** — les couleurs passent par
les rôles NkTheme (§3).

### 1.1 TitleBar (V1 — remplacée par TopHeader en V2)
- Barre 32 px : logo 18×18 (carré `primary`, 4 carreaux blancs d'opacités
  décroissantes) · menus `Fichier Édition Affichage Objet Comportement IA
  Fenêtre Aide` (12 px, `text_muted`) · nom du fichier CENTRÉ
  `● Dashboard_Admin.nkgui` (pastille `warning` = non enregistré) · 3 boutons
  fenêtre 14×14 (réduire/agrandir gris `muted`, fermer FOND ROUGE #f85149).

### 1.2 TabBar (V1 — absorbée par TopHeader en V2)
- Barre 34 px sous la TitleBar, onglets 33 px : icône document 10×10, libellé
  (` ●` si modifié), `×` de fermeture, **liseré actif 2 px `primary` en bas**,
  onglet actif `bg-panel` / inactifs `bg-background`, bouton `+` en fin.
- Convergence : `nkgui::TabBarEx` porte déjà ce dessin (fond distinct +
  liseré d'accent — cf. PORTES du 28/08).

### 1.3 TopHeader (V2) — logo-ancre + deux bandes minces
- Hauteur totale **56 px = 2 bandes de 28 px**, et un **logo carré 56×56 qui
  chevauche les deux bandes** (dégradé 135° #1a5fb4→#2f81f7→#a371f7, 4
  carreaux blancs + trait diagonal). Les deux bandes commencent à
  `left = 56 px`.
- Bande 1 (28 px) : menus (11 px) · nom de fichier centré sur TOUTE la
  fenêtre · contrôles fenêtre 13×13.
- Bande 2 (28 px) : les onglets (27 px, mêmes états que TabBar) + `+`.
- État : actif `bg-background`, inactif `bg-panel` (inversé par rapport à
  TabBar V1 — le fond de la zone document est sombre).

### 1.4 HierarchyPanel — panneau gauche 220 px
- En-tête 34 px : titre « Hiérarchie » (12 px, 600) + loupe 13×13.
- Sections à en-tête MAJUSCULES 10 px espacées (« Pages », « Composants »)
  avec `+` à droite ; séparateur `border` entre les deux.
- Rangée d'arbre 22 px : chevron 9×9 (ou espace réservé) · icône de nature
  11×11 · libellé 12 px (ellipsis) · badge de rôle (« Button », 9 px, pilule
  `primary` à 13 % de fond) · **œil + cadenas visibles au survol seulement**
  (`opacity-0 group-hover`).
- Indentation : `8 + 14·niveau` px. Sélection : rangée entière `bg-primary`,
  texte blanc. Page ouverte = icône teinte `primary`.

### 1.5 InspectorPanel (V1) — panneau droit 240 px
- En-tête 34 px : icône bouton + nom de l'élément. Onglets **Design · Widget
  · Behavior** (30 px, actif = liseré 2 px `primary` dessous).
- Sections (titre MAJUSCULES 10 px + filet) : Transformation (X/Y/W/H en
  grille 2×2, champs 22 px `input`), Apparence (pastille couleur 13×13 +
  hexa), Bords (R/Brd), Ombres (« Aucune ombre » + `+`), Texte (police Inter
  déroulante, px/fw/lh en grille 3).

### 1.6 InspectorPanelV2 — panneau droit 236 px, LE modèle déclaratif
Ce que V2 change (c'est la version qui correspond au modèle du document) :
- **Taille par MODE** : « Largeur : `expand` » (flèche `primary`), « Hauteur :
  `fixed` 44 », chacune avec sa ligne `min` / `max` en retrait (120/320,
  36/—). Le vocabulaire affiché est CELUI du format (`expand`, `fixed`).
- **Position X/Y** (95/228 — un élément posé sous parent Free).
- **Espacement** : section repliée (chevron).
- **Widget d'ANCRAGE** : rectangle central + 4 poignées de bord — gauche et
  droite ACTIVES en bleu avec traits de liaison, haut/bas grises détachées,
  point central. (≈ le widget d'ancrage type Unity.)
- **Alignement** : 6 icônes 22×22 (3 horizontales | filet | 3 verticales),
  active = fond `primary` 13 % + bord `primary`.
- Puis Apparence / Bords / Typographie comme V1 (champs 20 px, plus denses).

### 1.7 DesignToolbar (V1 — éclatée en V2 entre FloatingToolRail et flottants du canvas)
- Barre horizontale 40 px : 7 outils 26×26 (flèche pleine active sur fond
  `primary`, cadre, rectangle, ellipse, texte, image, plume) · **bascule de
  mode centrée** `Design / Behavior / Split` (26 px, actif fond `primary`) ·
  zoom `100% ▾` + **grille** (bord `primary` = enclenchée) + **aimant**
  (magnétisme).

### 1.8 DesignCanvas (V1) — toile sombre
- Fond `canvas` #1c2128 + **grille de POINTS** (`radial-gradient`, pas 20 px).
- 2 cadres page (Connexion 320 px, Dashboard 380 px), étiquette au-dessus
  (11 px `muted`), ombre portée. Formulaire de connexion peint DANS le cadre.
- **Bouton sélectionné** : contour 1.5 px `primary`, **8 poignées 7×7**
  (coins + milieux, fond `background` bord `primary`), **badge de rôle
  « Button »** au-dessus à gauche (pilule 9 px).
- **Lignes de magnétisme** roses `snap` #ff4fd8 (1 px, opacité 0.85).
- Indicateur « Canvas — 100% » en bas à gauche.

### 1.9 DesignCanvasV2 — toile CLAIRE, cadres d'appareil, flottants
Ce que V2 change :
- Fond **clair** #f5f7fb, points #d4dce8 (la toile n'est plus sombre — le
  document est clair, l'éditeur reste sombre autour).
- Cadres = **appareils nommés** : « Connexion — Mobile 390 × 844 » (240×520
  affiché) et « Dashboard — Bureau 1440 × 900 » (560×340).
- **Bascule de mode FLOTTANTE centrée en haut** : `Design / Behavior /
  Animation / Split` — **Animation apparaît en V2**.
- **Cluster de zoom FLOTTANT en bas à droite** : `100% ▾` + grille + aimant
  (les trois quittent la barre d'outils).
- Badge de rôle en français (« Bouton »), poignées 6×6 blanches bord
  `primary`, lien « Mot de passe oublié ? » dans la maquette.

### 1.10 FloatingToolRail — rail d'outils FLOTTANT vertical
- 36 px de large, fond `panel`, bord `border`, rayon 5 px, **ombre portée**
  (flotte SUR la toile, bord gauche). Boutons 36×28, actif fond `primary`.
- Outils : flèche (active) · cadre · formes (rectangle, **chevron 4×4 en bas
  à droite = menu de variantes**) · plume (chevron aussi) · — filet — ·
  texte · image · règle.

### 1.11 SideRail — rail vertical mince 28 px (par côté)
- Boutons 28×28, actif = fond `primary` à 13 %.
- Rail DROIT : grille de 4 carreaux (bibliothèque de composants) · **étoile
  violette #a371f7 (IA, active)** · œil-vague (aperçu ?).
- Rail GAUCHE : cadenas (verrous ?) · silhouette (profil/compte ?).
- (Les usages exacts ne sont pas étiquetés dans le JSX — à trancher.)

### 1.12 BottomRail — barre basse 28 px
- Pastilles : **Console** (icône `>_` + **badge d'erreurs rouge « 2 »**,
  fond `secondary` = sélectionnée) · **Aperçu** (œil) · à droite : pastille
  verte 6 px + « Prêt ».
- C'est le « rail bas » que le plan §13 attendait (Console/Validation +
  Preview/Test) — la coquille ne porte pas encore ce mécanisme (mesure du
  panneau IA, Q31).

---

## 2. Inventaire des icônes (SVG inline, primitives lues dans l'export)

Toutes en trait 1-1.3 px, bouts ronds, sur grilles 9×9 à 14×14. Couleur =
`text_muted` au repos, `accent_ui` actif (jamais en dur). Le redessin va dans
le vocabulaire du kit, comme les 7 outils existants.

| icône | dessin en primitives | où elle sert |
|---|---|---|
| chevron droite/bas/haut | 1 polyligne 2 segments (`M3 2 L6 4.5 L3 7`) | arbre, sections repliées, menus, combos |
| page | rect arrondi + 2 lignes horizontales | Hiérarchie (Pages) |
| panneau | rect arrondi + 1 ligne horizontale médiane | Hiérarchie (conteneurs) |
| bouton (rôle) | rect arrondi plat + ligne centrée | Hiérarchie, en-tête Inspecteur |
| texte | `M2 3 H9 M5.5 3 V9` (T) ; variante outil : + barre basse | Hiérarchie, outils |
| œil | 2 arcs opposés (amande) + cercle central | visibilité, Aperçu |
| œil barré | œil + trait diagonal | filtre « éléments à rôle », écran 8 |
| cadenas | rect 5×4 + anse en U | verrouillage, lecture seule |
| loupe | cercle + trait diagonal | recherche (Hiérarchie, menus rôle, Bibliothèque) |
| flèche de sélection | polygone plein 7 points (curseur) | outil S, rails |
| cadre (artboard) | 4 rects en croisillon (coins évidés) | outil F |
| rectangle | rect arrondi contour | outil R |
| ellipse | ellipse contour | outil O |
| image | rect + cercle (soleil) + polyligne (montagne) | outil image |
| plume | pentagone pointe + trait | outil plume |
| règle | rect horizontal + 3 graduations | outil règle |
| grille | 2 lignes H + 2 lignes V (#) | bascule grille |
| aimant | U épais + 3 bouts de pôles | magnétisme |
| mode Design | A (2 traits + barre) | bascule de mode |
| mode Behavior | 3 cercles reliés par 2 courbes (graphe) | bascule de mode |
| mode Animation | cercle + aiguilles (horloge) | bascule V2 |
| mode Split | 2 rects côte à côte | bascule de mode |
| console | chevron `>` + trait bas (`>_`) | BottomRail |
| étoile IA | étoile 5 branches contour (violette) | SideRail, en-tête Chat IA |
| étincelle IA | étoile 4 branches | badge « généré par IA » (Bibliothèque) |
| carreaux ×4 | 4 petits rects arrondis | bibliothèque, logo, bande modèle (Chat) |
| losange | carré tourné 45°, plein/creux | instance avec/sans écart (Hiérarchie 8/10/19) |
| pastille/anneau ambre | disque ou anneau + chiffre | écarts d'instance, compteurs |
| triangle ! | triangle + point d'exclamation | avertissements (Simulation, greffons) |
| prise/greffon | rect + 2 broches (barré quand manquant) | greffons (21-25) |
| épingle | goutte pointée | Bibliothèque (épingler) |
| case cochée | rect arrondi plein `primary` + coche blanche | Relevé de changements (Chat IA) |
| envoi | flèche pleine type avion | Chat IA |
| ampoule/info | cercle + i | justifications, rapports |
| fenêtre réduire/agrandir/fermer | trait bas / rect / croix | TitleBar, TopHeader, déco client (14/15) |
| alignements ×6 | traits + barres (g/c/d, h/m/b) | InspecteurV2 |
| ancrage | rect central + 4 poignées de bord + liaisons | InspecteurV2 |
| X rouge / Y vert | marqueurs de champ Position | écran 3 (axes = `AxisX`/`AxisY`) |

---

## 3. Jetons de thème → rôles NkTheme

Palette de l'export = **GitHub Dark** (la règle « thème GitHub partout » est
respectée par la maquette). Police **Inter**, corps 10-14 px ; rayons 4/6/8/24,
`--radius` par défaut = 4 px.

| jeton Banani | valeur | rôle NkTheme | note |
|---|---|---|---|
| `--color-background` | #0d1117 | `WindowBg` | ✅ même valeur que la coquille |
| `--color-panel` | #161b22 | `PanelBg` | ✅ |
| `--color-border` | #30363d | `Border` | ✅ |
| `--color-input` | #010409 | `InputBg` | plus sombre que le panneau (« creusé ») |
| `--color-foreground` | #e6edf3 | `Text` | ✅ |
| `--color-secondary-foreground` / `muted-foreground` | #8b949e | `TextMuted` | ✅ |
| `--color-primary` | #2f81f7 | `AccentUi` | l'accent de la maquette |
| `--color-primary-foreground` | #ffffff | `TextOnAccent` | ✅ |
| `--color-secondary` / `muted` | #21262d | `ButtonBg` | le rôle ajouté le 29/08 convient |
| `--color-canvas` | #1c2128 | **MANQUANT** | fond de toile V1 ; V2 = clair #f5f7fb → un rôle `CanvasBg` (et la grille de points avec) |
| `--color-success` | #3fb950 | **MANQUANT** (`StatusOk` ?) | « Prêt », simulation, LOCAL |
| `--color-warning` | #d29922 | proche `AccentSel` #F2980E | ambre = non-enregistré, écarts d'instance, permissions — à trancher : `AccentSel` ou un `StatusWarn` dédié |
| `--color-error` | #f85149 | **MANQUANT** (`StatusErr` ?) | badge console, rejets, fermer |
| `--color-violet` | #a371f7 | **MANQUANT** (`AccentAI` ?) | l'IA est systématiquement violette dans les maquettes |
| `--color-snap` | #ff4fd8 | **MANQUANT** (`SnapLine` ?) | lignes de magnétisme |

Rayons : `--radius-sm` 4 ≈ champs/boutons ; `md` 6 ≈ cartes ; `xl` 24 ≈
pilules de badge. À porter en métriques, pas en dur.

---

## 4. Les 28 écrans, une ligne chacun

| écran | ce qu'il montre | zone / spéc | neuf par rapport aux planches 22.x ? |
|---|---|---|---|
| 1 Main Editor V2 (Dark) | l'assemblage canonique V2 : TopHeader + Hiérarchie + FloatingToolRail + CanvasV2 + InspecteurV2 + SideRails + BottomRail | fenêtre principale | OUI — c'est LA composition de référence (V2) |
| 2 Canvas — gros plan Design | CanvasV2 + rail flottant, sélection avec poignées/badge | §4.2 toile | non (détail de 22.0), mais valide poignées + badge |
| 3 Inspecteur — gros plan Dark | InspecteurV2 détaillé : **Cible du cadre**, Position X(rouge)/Y(vert), modes `content/fraction/weight/expand`, Aligner, Décor | §4.4 | OUI — la section « Cible » et les marqueurs d'axes |
| 4 Inspecteur — onglet Behavior | événements du rôle (pressé/relâché/cliqué→`OnSubmitForm`/survols) + « événements ajoutés » + « Nouvel événement » | §4.6 | OUI — la liste liée/non liée par élément |
| 5 Inspecteur — menu Rôle ouvert | déroulant : recherche, Rôles natifs, Rôles de projet, « + Définir un rôle de projet… », Changer/Retirer | §4.3 promotion | OUI — le geste d'attribution de rôle |
| 6 Menu Rôle étendu | LA taxonomie complète : Récents · Actions · Saisie · Sélection · Navigation · Conteneurs · Affichage · Rôles de projet (~45 rôles, 50 icônes) | vocabulaire des rôles | OUI — la liste canonique des rôles avec icônes |
| 7 Menu Rôle — bas du menu | fin de la même liste (Conteneurs/Affichage/projet) + barre de défilement | idem | non (suite de 6) |
| 8 Panneau Hiérarchie | arbre complet : **losange d'écart d'instance**, badges ambre, filtre « seulement les éléments à rôle », œil-barré | §4.3 | OUI — losanges + filtre à rôle |
| 9 Bibliothèque de composants | grille/liste, groupes **Projet / Importé / Système**, badges (instances ×N, étincelle IA, mise à jour), épingle, « Importer un composant… » | §4.5 palette | OUI — la palette par PROVENANCE |
| 10 Hiérarchie + composants du projet | les 2 sections empilées avec **poignée de redimensionnement à 3 points** entre elles | §11.6 (déjà construit) | partiellement — la poignée épaisse est neuve |
| 11 Canvas — zone sûre Mobile | cadre iPhone (encoche, indicateur home), **hachures zones non sûres**, pointillés + libellé « zone sûre », annotation « ancré au bord du cadre » | cible d'appareil | OUI — tout le concept zone sûre |
| 12 Canvas — zone sûre visible | idem, les 2 annotations + rail flottant | idem | non (variante de 11) |
| 13 Canvas — Comportement Blueprint | graphe : nœud Événement `cliqué` (**atteint ×3**), Condition `champ non vide`, Actions, câbles allumés/éteints, entrée données verte, « non lié », Console de simulation | §4.6 nodal | OUI — compteurs d'atteinte sur les nœuds |
| 14 Canvas — déco fenêtre client | fenêtre Bureau décoration **Client** : barre de titre dessinée par l'app, « zone de saisie », bords de redimensionnement, curseurs | déco NKWindow | OUI — éditer la décoration client |
| 15 Canvas — zone de saisie hachurée | idem 14, la zone de saisie SURLIGNÉE en hachures bleues (découpe des boutons) | idem | non (état de 14) |
| 16 Panneau — Simulation en cours | doublures (« une doublure rend aussi l'échec »), **points de connexion 7 atteints / 3 jamais atteints**, journal, Recharger/Geler/Comparer | §4.9 test | OUI — la comptabilité des chemins |
| 17 Canvas — états de champs | 5 blocs : normal · lecture seule · **désactivé avec raison (infobulle sur la région)** · occupé · **désactivé par héritage (voile)** | états des widgets | OUI — la sémantique des états |
| 18 Canvas — graphe d'états (Animation) | State Machine : Entry→Idle, transitions étiquetées (survols/pressé avec durées), panneau Transition + **courbe Ease Out à poignées**, Dope Sheet inactif | mode Animation | OUI — tout le mode Animation |
| 19 Fenêtre principale — assemblage complet | TOUT réuni (l'écran 1 avec le contenu de 8/9/V2/3) — l'écran de référence à recopier | fenêtre principale | OUI — référence d'assemblage |
| 20 Panneau — Animations (Ambiances) | onglets Transitions/**Ambiances**/Effets continus : Respiration (échelle, 1800 ms, aller-retour), Inclinaison (pointeur→rotation, lissage 0.65, **valeur de repos**), bandeau **Mouvement réduit** (Arrêter/Raccourcir/Conserver) | animation décorative | OUI — familles d'animation + accessibilité |
| 21 Gestionnaire de greffons | liste (Actifs/Désactivés/Rejetés), coût « 16,3 ms par image », erreurs, « Ouvrir le dossier des greffons » | greffons | OUI — tout le chantier greffons |
| 22 Modale — installation de greffon | « Pont Figma » : ce qu'il ajoute, PERMISSIONS en ambre, provenance (fichier, **NON SIGNÉ**), case « J'ai lu… », « préfixées par figma. » | greffons | OUI |
| 23 Modale — installation v2 | idem 22 + avertissement proéminent « Un greffon exécute du code dans l'éditeur. » | greffons | non (v2 de 22 — la plus récente) |
| 24 Modale — désinstallation | « tout ce qui est préfixé par lumen. sera retiré » + **documents qui en dépendent** + « Désactiver au lieu de désinstaller » | greffons | OUI |
| 25 Canvas — greffon manquant | éléments hachurés + bandeau « 18 éléments ne peuvent pas être affichés. Ils sont conservés tels quels. » + **export bloqué** | greffons | OUI — la dégradation honnête |
| 26 Menu Cible — déroulant ouvert | menu : Classe › · Appareil… · Orientation (Portrait ✓/Paysage) · ✓ Afficher la zone sûre · Décoration › | cible d'appareil | OUI — le menu qui pilote 11/12/14 |
| 27 Rapport de transposition | Bureau→Mobile, « 14 constats », dépassements en px, tailles tactiles < 44 px, seuils — « Ce rapport nomme les écarts. Il n'en corrige aucun. » | multi-cibles | OUI — l'outil de constat |
| 28 Panneau Chat IA — Assistant | panneau droit 360 px : bande modèle « **Ollama · llama3 8B · LOCAL — Rien ne quitte cette machine** », carte « **Relevé de changements** » (cases cochables par changement, vignette, justification, « S'appliquera en une seule opération annulable », **Rejeter/Appliquer**), actions rapides (Générer un écran / Modifier la sélection / Générer un comportement), sélecteur de portée « Sélection » | §6 IA | OUI — l'interface exacte de l'aperçu IA |

---

## 5. Les écarts (et les convergences) avec ce qui est construit

### V1 → V2 : ce que les V2 changent (à traiter comme l'intention actuelle)
1. **TitleBar+TabBar → TopHeader** : une seule pièce, logo 56×56 qui
   chevauche deux bandes de 28 px ; l'application actuelle a une barre
   de titre de coquille classique.
2. **DesignToolbar (barre horizontale) → FloatingToolRail + flottants** :
   les outils quittent la barre pour un rail flottant à gauche de la toile ;
   la bascule de mode devient un flottant centré EN HAUT DE LA TOILE et
   gagne **Animation** ; le zoom/grille/aimant deviennent un flottant en bas
   à droite. L'application actuelle a déjà des flottants (bascule, outils,
   cluster — cf. Q31 toile) : c'est la même direction, à aligner sur ces
   géométries.
3. **DesignCanvas sombre → DesignCanvasV2 clair** (#f5f7fb, points #d4dce8) :
   la toile V2 est claire même en thème sombre ; les cadres deviennent des
   **appareils nommés avec leurs dimensions** (« Mobile 390 × 844 » — les
   mêmes 390×844 que le Ctrl+N actuel).
4. **InspectorPanel → InspectorPanelV2** : la Transformation X/Y/W/H simple
   devient **mode de taille (`expand`/`fixed`) + min/max + ancrage +
   alignement** — c'est l'inspecteur du modèle déclaratif, exactement le
   vocabulaire `NkSizeMode` du code (écran 3 : `content | fraction | weight |
   expand`).

### Convergences fortes (rien à inventer, tout à brancher)
- **Écran 28 = la spécification visuelle du chantier IA livré en Q31** :
  « Relevé de changements » + Rejeter/Appliquer = `Propose`/`DiscardProposal`/
  `CommitProposal` ; « une seule opération annulable » = `Retract` ; « Ollama
  · LOCAL » = le pont `ia_pont.py` ; le sélecteur de portée « Sélection » =
  le `targetParent`. Le panneau AIPanel actuel (texte + « Demander à l'IA »)
  doit évoluer vers cette carte.
- **Position 95/228 dans InspecteurV2** = les coordonnées `posX/posY` sous
  parent `Free` du modèle actuel (même exemple que la toile).
- **Badge de rôle sur la toile et l'arbre** : la Hiérarchie actuelle affiche
  déjà « [ia] » — le badge de rôle Banani (pilule) est le même mécanisme.
- **Les 7 outils du rail** ≈ les outils déjà redessinés par le chantier
  toile (S/F/R/T…) ; Banani ajoute règle, plume et les chevrons de variante.

### Contradictions / points à trancher (par Rodolf ou le coordinateur)
1. **Toile claire** (V2) vs toile sombre actuelle — thème du DOCUMENT ≠
   thème de l'ÉDITEUR ; demande un rôle `CanvasBg` (et le futur thème Light
   Pro y passera aussi).
2. **Grille de POINTS** (V1 et V2) vs grille pointillée actuelle en LIGNES
   (Ctrl+N du chantier toile) — à trancher, probablement points.
3. **Onglet actif** : TabBar V1 dit actif=`panel` sur fond `background`,
   TopHeader V2 dit actif=`background` sur fond `panel` — suivre V2.
4. **Rails latéraux** : la coquille n'a ni rail ni pastille (mesuré, Q31) —
   SideRail/BottomRail exigent le mécanisme §13 dans NkEditorShell, c'est un
   chantier de KIT, pas d'application.
5. **5 couleurs sans rôle** (`canvas`, `success`, `error`, `violet IA`,
   `snap`) — le vocabulaire NkTheme doit s'étendre (append-only) avant que
   ces écrans soient exprimables sans couleurs en dur.
6. **Le menu « Cible »** (écran 26) n'existe pas dans la barre de menus
   actuelle (Fichier/Édition/Affichage/Objet/Comportement/IA/Fenêtre/Aide) —
   il apparaît dans les écrans zone-sûre ; à insérer (probablement entre
   Affichage et Comportement, la maquette 26 l'ancre dans la barre).
7. **L'écran 6 fixe la taxonomie des rôles (~45)** — bien plus riche que les
   2 composants du registre actuel ; c'est la réponse maquettée à la
   « question de la palette » posée en Q31 (pas de bouton/champ déclarés).

### Ordre d'implémentation suggéré (du moins dépendant au plus dépendant)
1. Icônes + jetons/rôles manquants (§2, §3) — tout le reste les consomme.
2. TopHeader, HierarchyPanel (badges/losanges), InspecteurV2 (modes/min-max/
   ancrage) — panneaux purs, mesurables par la sonde.
3. Flottants de toile V2 (bascule+zoom) — la toile existe déjà.
4. Chat IA (écran 28) sur le pipeline Q31 — la plomberie est prouvée.
5. Zone sûre / Cible / déco client (11-15, 26) — exige le menu Cible.
6. Simulation (16), Animation (18/20), Greffons (21-25), Transposition (27)
   — des chantiers en soi, spécifiés par ces écrans.
