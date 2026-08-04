# NK3DModeler — Feuille de route

> Document **suivi par git** (contrairement à `CARNET.private.md`, qui garde le
> journal détaillé et les idées écartées). Il dit **ce qui reste à faire** et
> **dans quel ordre**, avec assez de détail pour reprendre sans rien redécouvrir.
> À mettre à jour **à chaque palier franchi**, pas en fin de session.

L'application est **un seul DCC** (pas de séparation modeleur / animation).
Langue de travail : **français**. Toute décision d'interface vient de Rihen et
est consignée avec sa raison.

---

## Ordre décidé par Rihen (révisé le 2026-08-02)

Cet ordre prime sur toute autre priorisation. **La modélisation est passée en
tête** : c'est le cœur de l'outil, tout le reste s'y raccroche. La sauvegarde
« viendra avec le temps » — elle n'est volontairement **pas** en tête.

| # | Chantier | État |
|---|---|---|
| 1 | **Modélisation** — et d'abord la refonte du **panneau droit** (maquette Banani) | 🔄 en cours |
| 2 | **Caméras** : plusieurs caméras, bascule, vue caméra | 🔄 socle livré (v15) |
| 3 | **Éclairage** : terminer la phase lumières | ⬜ |
| 4 | **Import de modèles** | ⬜ |
| 5 | **Terminer les éléments du combo Ajouter** | ⬜ |
| 6 | **Mode Édition** (sommets / arêtes / faces, sculpt) | ⬜ |
| 7 | **Sortir les 96 objets de la démo** — *uniquement quand toute la modélisation est terminée* | ⬜ |

### 1. Modélisation — refonte du panneau droit 🔄

Maquette de référence : Banani, flow **NK3D Modeler**, écran « Mode Objet (v2,
ref complète) », composant `NK3DPropertiesTabs`. Icônes **Lucide**.

**Livré**
- Les pastilles sont les **quatre** de la maquette — Modèle (`box`), Rendu
  (`sun`), Scène (`layers`), Modificateur (`sliders-horizontal`) — et **une
  seule est active à la fois** ; recliquer l'active replie le panneau.
- Les sept icônes Lucide manquantes ont été dessinées : `sun`,
  `sliders-horizontal`, `link-2`, `square-check`, `plus-circle`,
  `minus-circle`, `tag`.
- Les réglages de l'outil, dont la pastille disparaît dans la maquette, sont
  hébergés sous « Modificateur » en attendant leur vraie place.

**Reste à faire — contenu de la pastille Modèle**, dans l'ordre de la maquette :
Transformation (Position / Rotation / Échelle) · Dimensions · Relations
(Parent / Enfant) · Matériaux · Groupes de Vertex · Shape Keys · Maps UV ·
Attributs de Couleur · Attributs · Espace Texture · Données Géométrie
(Vertices / Faces / Edges).

Chaque ligne de transformation porte ses trois boutons `lock`, `refresh-cw`,
`link-2` ; chaque élément de liste porte sa rangée de quatre boutons
`square-check`, `square`, `plus-circle`, `minus-circle`, et chaque section de
liste finit par un bouton « + Ajouter ».

**Règle absolue** : aucune donnée inventée. Les sections dont le modèle de
données n'existe pas encore (groupes de vertex, shape keys, maps UV, attributs)
affichent un état vide honnête et se remplissent de ce que l'utilisateur crée —
jamais d'entrées de démonstration.

Puis viennent les trois autres pastilles, à retravailler avec Rihen.

---

## 1. Caméras 🔄

**Livré (v15)** : sélecteur de vue en haut-gauche du viewport listant « Vue 3D »
et **toutes les caméras du document actif** ; la vue caméra montre exactement ce
que voit la caméra (position monde, regard **-Z local** comme le filaire, focale
du nœud) ; retour à la vue 3D avec **restitution de la pose libre** mémorisée à
l'entrée ; la vue caméra force la perspective (une caméra réelle n'est pas
orthographique) ; sortie automatique si la caméra disparaît ou change de document.

**Reste à faire :**
- Raccourci clavier (façon pavé numérique 0) et « caméra active » de la scène.
- **Piloter la caméra depuis la vue caméra** : naviguer déplace la caméra elle-même
  (verrou façon UE5 « pilot »), au lieu de subir la pose.
- Cadre de sécurité / rapport d'aspect du rendu, surbalayage, grille de composition.
- Profondeur de champ, exposition, et les autres propriétés ciné.
- « Aligner la caméra sur la vue » et « aligner la vue sur la caméra ».

## 2. Éclairage ⬜

- **Mélange de deux textures** de lumière en **pré-composition CPU** (décidé avec
  Rihen ; le nodal viendra avec NKGraphe).
- **Textures de lumière par fichier** : remplacer le numéro d'atlas (8 slots) par
  une vraie image importée.
- Ombres par lumière, portée/atténuation affinées, IES éventuellement.
- Widget de sélection dans la vue pour les lumières utilisateur.

## 3. Modélisation complète ⬜

- **Mode Édition** : sommets / arêtes / faces, sélection, extrusion, biseau,
  boucles, subdivision.
- **Sculpt** (2.5D puis réel), retopologie, décimation.
- Géométrie manquante du menu Ajouter : **texte, courbes, surfaces, metaballs**.
- Modificateurs réels (la liste existe, elle est décorative).

## 4. Import de modèles ⬜

- **Glisser-déposer depuis Windows** (`WM_DROPFILES`) + bouton « Importer ».
- Boîte de **propriétés d'import** (échelle, axe haut, unités, matériaux).
- Formats : mesh (via NKAssimp), **texture**, **image de référence**.
  Le format matériau et le format dataset JSONL existent déjà.
- Conversion vers **nos propres formats** au moment de l'import.

## 5. Terminer le combo Ajouter ⬜

Les natures créées mais **sans géométrie réelle** : texte, courbe, surface,
metaball. Chacune doit avoir son panneau « Ajuster la création » comme les
primitives paramétriques (sphère, icosphère, tore, cylindre, cône, capsule…).

## 6. Sortie de la démo ⬜

Les nœuds 0-95 appartiennent au portage de `--demo=2` (scène 0 du document).
Ne les retirer **qu'une fois toute la modélisation terminée** — ils servent
aujourd'hui de banc d'essai permanent pour le rendu, la parenté et le gizmo.

---

## Chantiers de fond (hors ordre ci-dessus)

### NKGraphe — l'éditeur nodal
« Blueprint » s'appelle désormais **Graphe**. Natures prévues :
**modélisation procédurale**, **texturing procédural**, **matériau**, et
**motion** (plus tard). Un graphe est un asset du navigateur (nature 0 +
sous-type). L'éditeur de **texture** doit permettre de **peindre au pinceau**,
de construire en **nœuds procéduraux**, ou de **mélanger les deux**.

### Fenêtres flottantes dockables (façon UE5)
Le double-clic sur un asset doit ouvrir une **fenêtre modale flottante**
**dockable** dans la barre d'onglets ; une fois dockée, elle remplace tout
l'espace de scène. Demande un gestionnaire de fenêtres + zones d'accueil +
aperçu de dépôt + détacher/rattacher. Aujourd'hui le double-clic ouvre
directement un onglet docké (état final du geste, sans l'étape flottante).

### Sauvegarde et format projet
Scènes, models, arborescence du navigateur, propriétés par scène, matériaux,
dimensions. « Viendra avec le temps » (Rihen), mais **rien n'est persistant**
tant que ce n'est pas fait.

### Annuler / Refaire
Aucun historique aujourd'hui. À concevoir avant que les outils d'édition se
multiplient (chaque opération devra être une commande réversible).

### Divers
- Sous-onglets spécialisés **par document** (modélisation procédurale, etc.)
  à côté de « Modeler ».
- Table de raccourcis **par fenêtre**, configurable.
- Restreindre l'ajout dans un Model : ✅ fait (maillages = sous-meshes ;
  lumières/caméras/empties = cosmétiques).

---

## Règles d'interface acquises (ne pas les redécouvrir)

- **Un document par onglet** : un nœud appartient à **une** scène ou **un**
  éditeur ; ailleurs il n'est ni rendu, ni listé, ni sélectionnable.
- **Scène et Model** seuls ont l'interface complète (hiérarchie, propriétés,
  barre d'outils, viewport). Matériau, texture, graphe et dataset restent
  **vides** tant que leur design n'est pas défini.
- Dans un Model, la **racine de la hiérarchie s'appelle « Model »**, le panneau
  garde le nom « Hiérarchie ».
- **Chaque scène a ses propres propriétés** ; on peut les copier vers une scène
  précise ou vers toutes.
- **Pliage sur la flèche seulement** ; le **clic droit** vaut sur toute la
  largeur d'une ligne ou d'une carte.
- Les menus prennent la **largeur de leur entrée la plus longue** et s'ouvrent
  **vers le haut** quand le bas manque.
- **Aucune référence inventée** dans l'interface : chaque libellé décrit ce qui
  existe vraiment.

## Pièges techniques déjà payés

- **Registre de zones** (`NkHitRegistry`) : capacité 1024 depuis v15. À 256 il
  **saturait en silence** et tout ce qui était déclaré tard (les chevrons de
  pliage) devenait mort. Si une interaction cesse de répondre sans raison,
  vérifier d'abord la saturation.
- La **dernière zone déclarée gagne** le survol : une zone large déclarée après
  une petite lui vole ses clics.
- Les **événements clavier ne livrent pas les lettres** au shell : les raccourcis
  passent par **polling NkInput** dans l'hôte.
- Les **combos différés** exigent une adresse d'état **stable** (jamais une
  variable locale recalculée).
- `SetGridFlags` ne doit **jamais** réactiver `g.showAxes` (axe Y du shader
  erroné = « seconde ligne verte »).
- Les scripts de patch doivent vérifier les **tabulations exactes** des ancres
  (`cat -A`) avant d'écrire, et rester **réentrants**.

---

# PASSATION — état au 2026-08-04 (session Rihen + agent)

> Cette section est le **point d'entrée d'un agent qui reprend le modeleur**.
> Elle dit ce qui vient d'être livré, ce qui reste, et dans quel ordre Rihen
> veut l'aborder. Les règles de travail (jamais la STL, français, valider après
> chaque correctif, build Debug ET Release, app fermée avant de compiler,
> relance vérifiée) sont dans `CLAUDE.md` à la racine.

## Ce qui a été livré et validé pendant cette session

- **Ombres** : bande de garde des faces omni (le carré au sol sous une point
  light), plan lointain à 2× la portée (l'ombre ne se tranche plus à la
  limite), aucune limite propre à l'ombre — elle meurt avec l'atténuation ;
  fondu de bord pour la directionnelle.
- **Caméra** : cadenas d'orbite (rotation seule autour du point visé), boule de
  navigation qui suit la caméra, palette qui ne chevauche plus.
- **Espaces** : 7 onglets (Objet, Édition, Sculpture 2.5D, Sculpture,
  Texturing, Patron, Texture painting) = les modes ; chacun a **sa pastille**
  dans le panneau Propriétés. Séparateurs entre onglets.
- **Matériau** : pastille dédiée (bibliothèque, groupes repliables, aperçu 5
  formes, combo système + Ajouter/Nouveau), règles de vie Blender (tout maillage
  naît avec un matériau, le dernier ne se supprime pas, suppression = les
  porteurs convergent), texture de couleur.
- **Aimantation** : cibles géométriques (sommet, arête, face, centres) avec la
  base « Closest » de Blender, aimant-bascule + panneau (cibles ET pas).
- **Divers** : multi-sélection réparée au commit, presse-papiers câblé (il ne
  l'avait jamais été), champs à 259 caractères, Maj+D ne duplique plus deux
  fois, dimensions honnêtes (cube 1 m comme Unreal), pivot dans la barre.

# CAMÉRA — plan d'ensemble (NkCamLink)

> Piloté depuis le modeleur, mais **destiné à servir d'autres applications**.
> Le code devra donc naître dans `Kernel/Runtime/`, à côté de NKCamera, et non
> dans `Applications/NK3DModeler/` : une extraction ultérieure coûte toujours
> plus cher que la bonne place au départ. Le modeleur en sera le premier client.

## Les trois usages, décidés avec Rihen

1. **Caméra réelle en incrustation** — la webcam devient une source de sortie
   comme une autre, posée sur l'image finale à la forme voulue : montrer la
   personne qui réalise le travail de modélisation.
2. **Caméra réelle en vidéo** — la même source, pendant un enregistrement.
3. **Téléphone pilotant une caméra virtuelle** — le téléphone ne transmet
   **que la pose** (position, orientation, focale) : quelques dizaines d'octets
   par image. C'est le principe posé par Rihen, et il est juste — transférer
   des images dans ce sens n'apporterait rien. Le retour visuel (voir ce que
   voit la caméra, depuis le téléphone) est un **second étage**, séparable.

## Ce qui existe déjà — et qu'il ne faut donc pas réécrire

| Module | État | Ce qu'on en prend |
|---|---|---|
| **NKCamera** | Livré | Énumération des périphériques, `StartStreaming`, `GetLastFrame`, `ConvertToRGBA8`. Backends Media Foundation (Windows), V4L2, Camera2, AVFoundation, getUserMedia. Mapping caméra physique → caméra virtuelle par **IMU**, livré. |
| **NKNetwork** | Livré | `NkDiscovery` (broadcast LAN : le téléphone trouve le PC sans qu'on tape une adresse), `NkReliableUDP` (ACK sélectif, retransmission sur RTT), `NkBitStream` avec `WriteQuatf` / `WriteVec3fQ` — la quantification pour laquelle cette couche a été écrite. `NkRPC` pour les commandes ponctuelles. |
| **NKMedia** | Livré | `NkImageSequenceWriter` (séquence PNG/JPEG/BMP/TGA/QOI, « workflow Blender »), `NkVideoWriter` (RAW, MJPEG, MPEG-1, **H.264 baseline bit-exact vs ffmpeg**), conteneurs AVI/MOV/MP4/WebM, **mux audio+vidéo** (`AddAudioSamples`, sync 0 ms). |
| **NKAudio** | Livré sauf capture | 256 voix, DSP, WAV/MP3/OGG/FLAC/Opus. |
| **NKCanvas** | Livré | Suffit à l'application mobile : au premier étage le téléphone n'affiche aucune 3D — il lit son IMU, envoie une pose, montre des repères. Au second, il affiche le retour comme une simple texture. |
| **NKImage** | Livré | Déjà exploité par la sortie (formats, `Resize` bicubique). |

## Ce qui manque, et où

| Manque | Où | Pourquoi c'est nécessaire |
|---|---|---|
| **Capture micro** | NKAudio | Sans elle, pas de **voix off** sur un tutoriel. Rihen : « on doit l'intégrer à tout prix, même si ce n'est pas pour maintenant ». Le reste de la chaîne est prêt : mixage, encodage Opus, mux A/V. |
| **Retour visuel vers le téléphone** | NkCamLink + NKMedia | Voir depuis le téléphone ce que voit la caméra. Second étage, explicitement voulu. Flux basse résolution : MJPEG suffit et NKMedia l'encode déjà. |
| **Protocole NkCamLink** | Kernel/Runtime | Découverte, appairage, pose quantifiée, RPC de commande. S'assemble depuis NKNetwork — rien à inventer côté transport. |
| **Application mobile** | Applications/ | NKCanvas + NKCamera (IMU) + NKNetwork. |
| **Tracking sans IMU** | — | Le mapping de NKCamera repose sur l'IMU, **absent des webcams Windows desktop**. Piloter la caméra virtuelle depuis une webcam demanderait du tracking visuel : projet à part entière, écarté pour l'instant. |

## Ce que ce travail apporte aux modules

**NKNetwork y gagne le plus.** Il est aujourd'hui validé par 67 checks et un
bout-en-bout en **loopback 127.0.0.1**. NkCamLink serait son premier usage réel
sur un vrai réseau — Wi-Fi, mobile vers PC, avec latence, pertes et
reconnexions. C'est cela qui éprouve un RUDP, pas un loopback. Deux TODO de sa
roadmap en bénéficieraient directement : les **stats runtime** (RTT, perte),
aujourd'hui partielles, et la **compression des snapshots** si le retour visuel
arrive.

**NKAudio** y gagne sa capture micro, qui manque à tout usage d'enregistrement.

**NKCamera** y gagne un usage réel de son mapping IMU, aujourd'hui livré mais
jamais employé par une application.

## Ordre proposé

1. **Webcam en incrustation** — presque du branchement : `ConvertToRGBA8` rend
   exactement le tampon RGBA que `NkInsetCompose` sait déjà poser, avec les
   formes et le liseré existants. Une source de plus dans la liste.
2. **Vidéo de sortie** — `NkImageSequenceWriter` d'abord (utile tout de suite,
   n'importe quel monteur assemble une séquence), puis `NkVideoWriter`.
3. **Capture micro** dans NKAudio, puis voix off sur la vidéo.
4. **NkCamLink, étage 1** — le téléphone comme manette : découverte, pose.
5. **NkCamLink, étage 2** — le retour visuel.

---

## OUTPUT — livré pendant la pause du 4 août (à valider)

> Release et Debug à 28/28, app relancée et fermée sans erreur au journal.
> **Non poussé** : Rihen valide d'abord.

**Une pastille dédiée**, `Output` (index 6 ; la pastille du mode passe en 7).
Elle est ajoutée en fin de liste et non après `Rendu` où sa place serait plus
logique : les indices de section sont mémorisés dans `st.propOpen` et câblés en
dur ailleurs (`kSelOnly`, la pastille du mode) — les décaler casserait ces
règles en silence.

**Sortie principale** — source (vue 3D ou n'importe quelle caméra), résolution
avec 8 presets (HD, FHD, 2K, 4K, carré, vertical, ciné, web), pourcentage
d'échelle, et la taille réellement produite affichée en toutes lettres.
La résolution est **indépendante de la fenêtre** : un rendu 4K depuis une
fenêtre 1600×900 fait bien 4K. Sans cela le champ n'aurait été qu'une
décoration.

**Destination** — dossier, nom, format, et le chemin du dernier fichier écrit.

**Incrustations** — jusqu'à 8 cibles secondaires posées sur la principale
(« une principale et les autres en miniature, rectangle, carré, cercle etc. »).
Chacune : source, forme (rectangle, carré, cercle, ovale, rectangle arrondi,
losange), position, taille, liseré avec sa couleur, opacité. Position et taille
sont des **fractions** de la principale, donc changer la résolution ne déplace
rien.

**Aperçu dans la vue** — les incrustations se dessinent dans le cadre caméra, à
leur place et à leur forme, numérotées et nommées. Sans cela il faudrait lancer
un rendu pour savoir où elles tombent.

**Le rapport de la caméra suit la sortie** — c'était annoncé dans le code
(« Full HD en v1 ; la pastille Output le pilotera ») : le cadre dessiné, le
voile et le rendu dérivent tous d'une seule fonction. Régler la sortie en carré
ou en vertical se voit immédiatement dans la vue caméra.

### Comment c'est fait, et pourquoi

Redimensionner la cible hors écran et lire ses pixels ne peuvent pas avoir lieu
dans la même image : le GPU doit avoir rendu entre les deux. Le rendu s'étale
donc en étapes — la principale, puis chaque incrustation — chacune en trois
images (poser, laisser rendre, lire). Neuf cibles font vingt-sept images, moins
d'une demi-seconde, et l'interface ne se bloque pas.

Le cadre caméra est forcé en **plein cadre** pendant la sortie. Comme c'est le
point de passage unique qui pilote l'élargissement du champ, le passe-partout
et le recadrage de la capture, le neutraliser à un seul endroit suffit à ce que
l'image de la caméra occupe toute la cible.

Les formes vivent dans `NkOutCompose.h`, à part : c'est du calcul pur, donc
**vérifiable hors de l'application**. Un test isolé génère une planche des six
formes et vérifie 9 propriétés (couverture au centre et aux coins de chaque
forme, liseré sur les quatre bords, opacité, cadres carrés forcés) — toutes
passent.

### Ce qui reste à faire sur Output

1. **Le rendu GPU n'a pas pu être testé** : il se déclenche par un bouton, et
   je ne pilote pas la souris. Tout le reste est vérifié (compilation, démarrage,
   formes en test isolé). C'est le premier point à essayer.
2. **F12** — c'est le raccourci de rendu chez Blender, mais il est déjà pris ici
   par l'opacité du plan de grille (un vestige de la démo). Je n'ai pas
   réquisitionné le raccourci sans ton accord : à trancher.
3. **Fond transparent** — le champ existe dans l'état mais n'est ni affiché ni
   implémenté (il faudrait ne pas peindre le ciel). Aucune commande factice
   n'est affichée pour autant.
4. Formats autres que PNG ; séquence d'images ; rendu depuis plusieurs caméras
   vers plusieurs fichiers plutôt qu'en incrustation.

## À VÉRIFIER PAR RIHEN À SON RETOUR (compilé, non relancé)

> Pause du 2026-08-04 à 11 h 35. Le modeleur est **fermé** pour rendre le GPU à
> BulkGen (corpus IA : 87 549 / 100 000 paires, cadence divisée par ~15 quand
> les deux tournent). Release et Debug sont à 28/28 ; **rien n'a été poussé** —
> la règle est de valider d'abord.

Quatre points à regarder, dans cet ordre :

1. **Édition proportionnelle en rotation et en échelle** (mode Objet). Elle ne
   propageait que la **translation** ; c'était une limitation que je m'étais
   donnée sans raison, relevée par Rihen. Les trois composantes se propagent
   désormais, atténuées, **autour du pivot figé au début du geste**. Test :
   sélectionner un objet au milieu d'un groupe, rayon large, tourner puis
   agrandir — la rangée doit s'**incurver** et s'**évaser**, pas pivoter sur
   place.
2. **Le NaN — cause trouvée et corrigée dans NKMath.** `NkQuatT::SLerp`
   rendait un quaternion NaN pour **deux quaternions identiques** :
   `NkQuatEpsilon` vaut 1e-12, or en float32 `1.0f - 1e-12f` arrondit
   exactement à `1.0f`, donc le repli NLerp ne se déclenchait jamais et on
   divisait par `sin(acos(1)) = 0`. Interpoler vers une rotation **nulle** —
   le cas le plus banal — contaminait toute la scène. Corrigé par un seuil
   exprimé dans la précision du calcul **plus** une barrière sur `sin θ` juste
   avant la division. Vérifié isolément hors application (`P' = (3.5;0;4)`
   exact, interpolation à 40 % d'une rotation de 30° = 12°). C'est un bug de
   **NKMath**, pas seulement du modeleur : tout code qui SLerp vers une
   rotation identique en souffrait silencieusement.
3. **Garde-fou conservé** : le commit de l'édition proportionnelle refuse
   d'écrire une position ou un quaternion non finis et trace dans le journal
   (`[PropEdit] terme degenere`). La cause est corrigée, mais l'état d'une
   scène ne doit jamais pouvoir être empoisonné sans laisser de trace.
4. **Pastille translucide derrière le gizmo de navigation** (bas à gauche),
   plus dense au survol du corps — elle signale au passage qu'il est
   saisissable. A obligé à ajouter `NkModelerPainter::RingColor` : le trou des
   demi-axes négatifs était rempli avec la couleur **opaque** du fond de vue et
   aurait percé des ronds pleins dans la pastille.

Également livré et non validé : l'**icône aimant** en fer à cheval (le
quadrillage disait « grille », alors que la bascule aimante aussi sur sommets,
arêtes et faces) et l'**icône d'édition proportionnelle** (point plein + anneaux
qui s'affinent : l'influence décroissante est dite par le trait). Chaque bascule
garde son chevron **à côté d'elle**, et le panneau du proportional (rayon + 8
atténuations) suit le patron de celui de l'aimantation : bloquant, ancré à son
chevron, fermé au clic extérieur. Les mêmes réglages sont répétés dans la
pastille **Outil**.

## EN COURS — à reprendre en premier (non validé)

**Orientations Local vs Global.** Rihen constate qu'elles restent identiques.
Deux causes ont déjà été corrigées : l'échelle s'appliquait toujours en axes
monde (corrigée par conjugaison avec la base du geste dans `NkGizmo3D::Apply`),
et le **commit** recomposait la transformation à sa façon (corrigé : il
décompose désormais `ComposedOf(i)`, la matrice réellement affichée).
**Le dernier retour de Rihen dit qu'il reste des erreurs — c'est le point de
départ.** Pistes à vérifier dans l'ordre :
1. le repère est-il bien poussé au gizmo qui bouge (`emptyGizmo` pour les
   nœuds utilisateur — ce gizmo a DÉJÀ été oublié deux fois dans des fan-out) ;
2. `HostDecompose` rend-il des angles cohérents avec la convention d'euler
   utilisée à la soumission (Z*Y*X) ;
3. la rotation et la translation (pas seulement l'échelle) respectent-elles le
   repère au commit.
Rappel de Rihen : **Local et Global ne coïncident que si l'objet est aligné au
monde** ; sur un objet tourné, les deux doivent visiblement différer, pendant
le glissement **et** après le relâchement.

## Reste à faire, dans l'ordre décidé par Rihen

1. ~~**Proportional editing**~~ — livré (sommets ET objets, les 8 atténuations,
   les trois transformations). **En attente de validation**, voir plus haut.
2. **Aimantation, compléments** : cibles *Volume* et *Arête perpendiculaire*
   (affichées « à venir », elles laissent le geste libre) ; base d'aimantation
   (Closest / Center / Median / Active) ; « Aligner la rotation sur la cible ».
3. ~~**Pastille Output**~~ — livrée (source, résolution, presets, pourcentage,
   destination, incrustations, aperçu). **En attente de validation**, voir plus
   haut. Reste la colonne caméra de la hiérarchie.
4. **Matériaux** : textures des autres canaux (rugosité, métallique, normale),
   choix du **type de surface** (`NkMaterialType` : PBR, Toon, Unlit…),
   sauvegarde `.nkasset` via `NkMaterialAsset`/`NkMaterialLibrary` (le format
   existe déjà, ne pas en inventer un), puis l'**éditeur nodal** pour le mixage.
5. **Contenu des espaces** : Édition (ses catégories sont amorcées), puis
   Sculpture 2.5D, Sculpture, Patron (dépliage UV), Texture painting — chacun
   remplit **sa** pastille.
6. **Plus tard** : profondeur de champ, Shift X/Y caméra, presets de nuages
   supplémentaires, matériaux par mesh/dynamiques/de déformation (après les
   fonctions d'édition), suppression de la démo **d'un bloc**.
7. **Icosphère** : elle ignore encore ses subdivisions (TODO moteur dans
   `NkMeshSystem::BuildIcosphereData`) — le curseur est donc sans effet.

## Principe à respecter (décision de Rihen)

Une fonctionnalité — onglet, espace, entrée de menu — **ne naît qu'avec ses
outils** : « les ajouter quand leurs outils naissent leur donnera un contenu
réel dès le premier jour ». Voir `PRINCIPES_CONCEPTION.private.md` à la racine.
Les stubs assumés s'affichent grisés et disent « à venir » — jamais une entrée
qui fait semblant d'agir.
