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

# CAMÉRA — plan d'ensemble

> Piloté depuis le modeleur, mais **destiné à servir d'autres applications**.
>
> **Aucun nouveau module.** J'avais d'abord proposé une bibliothèque dédiée ;
> Rihen a demandé pourquoi ne pas l'intégrer à NKCamera, et la question a
> montré que ma proposition était mauvaise. Le découpage juste :
>
> - **Capture (webcam en incrustation, en vidéo)** → NKCamera, tel quel. Rien à
>   créer : un adaptateur dans le modeleur suffit.
> - **Lien téléphone (découverte, pose, commandes)** → **NKNetwork**, à côté de
>   `NkLobby` et `NkDiscovery` qui font déjà de la découverte et de
>   l'appairage. Pas dans NKCamera : celui-ci **ne dépend pas de NKNetwork**
>   (vérifié dans son `.jenga`), et l'y mettre ferait tirer toute la pile réseau
>   à `NkCameraDemos`, qui veut seulement lire une webcam. Dans l'autre sens,
>   recevoir une pose de téléphone n'a aucune raison d'exiger Media Foundation.
> - **Application mobile** → une application, pas une bibliothèque.

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
| **Lien de pose** | NKNetwork | Découverte, appairage, pose quantifiée, RPC de commande. S'assemble depuis les couches existantes — rien à inventer côté transport. |
| **Application mobile** | Applications/ | NKCanvas + NKCamera (IMU) + NKNetwork. |
| **Tracking sans IMU** | — | Le mapping de NKCamera repose sur l'IMU, **absent des webcams Windows desktop**. Piloter la caméra virtuelle depuis une webcam demanderait du tracking visuel : projet à part entière, écarté pour l'instant. |
| **Baromètre** | NKCamera | Variations verticales ~10–20 cm. Retenu comme complément, pas comme solution (aucun déplacement horizontal). |
| **Module `NKXR` (OpenXR)** | Kernel/Runtime | Le vrai 6DoF : casque **et contrôleurs**. Rien n'existe aujourd'hui dans le dépôt. Sert aussi la VR/AR du moteur, au-delà de la caméra du modeleur. |

## Position et hauteur : mesurées ou déclarées ?

Question de Rihen. Réponse honnête : **la hauteur exacte n'est pas mesurable
par l'IMU**, et ce n'est pas une limite d'implémentation mais de physique.
`NkCameraOrientation` expose `yaw`, `pitch`, `roll` et l'accéléromètre brut —
aucune position. En tirer une position demanderait d'intégrer deux fois
l'accélération : l'erreur croît quadratiquement, la dérive atteint des mètres
en quelques secondes.

| Voie | Ce qu'elle donne | Coût |
|---|---|---|
| **Déclaration** (config) | hauteur exacte, choisie | nul — **retenu pour l'étage 1** |
| **Baromètre** | variations verticales ~10–20 cm après remise à zéro ; rien d'absolu (la météo décale) | moyen, non exposé par NKCamera |
| **ARCore / ARKit** | vraie pose 6DoF (position + orientation) | élevé, SDK propriétaires, chantier à part |

**Étage 1 : position déclarée, orientation mesurée.** Un trépied virtuel dont
on règle la hauteur, et le téléphone dit où l'on vise. **Rihen a raison de
noter la limite** : cela convient aux plans fixes — vue de dessus, de dessous,
panoramique depuis un point — mais pas au mouvement. Pour monter, descendre,
courir, tourner, il faut du vrai 6DoF.

**Le baromètre est à retenir** (Rihen : « on doit y penser ») : il donne les
variations verticales à ~10–20 cm après remise à zéro. Il ne suffit pas seul —
pas de déplacement horizontal — mais il rend crédible un mouvement vertical.

## Se déplacer VRAIMENT dans la scène — le 6DoF

Objectif explicite de Rihen. Trois voies, comparées honnêtement :

| Voie | À écrire | Qualité | Coût |
|---|---|---|---|
| **Casque + contrôleurs VR (OpenXR)** | un backend OpenXR | excellente | **moyen — retenu** |
| ARCore / ARKit | deux intégrations propriétaires, par plateforme | bonne | élevé |
| Notre propre SLAM visuel-inertiel | tout | incertaine | très élevé (années-homme) |

**Pourquoi le casque gagne.** Un casque 6DoF *fait déjà* son tracking
(inside-out, caméras intégrées) : il ne livre pas des mesures à intégrer mais
une **pose position + orientation** déjà calculée, des centaines de fois par
seconde, au millimètre. Et l'accès passe par **OpenXR**, standard **ouvert** de
Khronos — comme Vulkan — et non par un SDK propriétaire.

**Les contrôleurs comptent autant que le casque** : eux aussi suivis en 6DoF,
ce sont deux caméras qu'on tient à la main. Monter, descendre, courir,
tourner : c'est ainsi que se font les mouvements de caméra virtuelle en
production. C'est la réponse directe aux « acrobaties » demandées.

**Rien d'XR n'existe dans le dépôt aujourd'hui** (vérifié : aucun module, aucune
mention d'OpenXR). C'est donc à créer — un module `NKXR` au niveau Runtime,
qui servirait aussi la VR et l'AR déjà envisagées pour le moteur, pas seulement
la caméra du modeleur.

Sur « recréer notre propre système AR » : légitime à terme, mais un SLAM
visuel-inertiel de qualité représente plusieurs années-homme. Le faire **après**
un backend OpenXR qui fonctionne est un choix ; le faire **avant** priverait
longtemps le projet de ce qu'il veut maintenant.

Pour une **webcam desktop**, la question ne se pose même pas : le tableau des
backends de NKCamera donne l'IMU absent sur Windows. Tout en configuration.

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

### Découper la vue — DEUX fonctionnalités distinctes (idées de Rihen)

Elles partagent l'apparence — une vue coupée en morceaux — mais **pas du tout la
sémantique**. Les confondre mènerait à une implémentation qui ne sert bien ni
l'une ni l'autre.

| | **Multi-vue** | **Séparateur univue** |
|---|---|---|
| Sert à | modéliser | comparer, expliquer |
| Caméras | **une par vue** (face, côté, dessus, perspective) | **une seule** |
| Tourner la vue | ne bouge que celle qu'on manipule | **bouge tout**, il n'y en a qu'une |
| Ce qui diffère | le point de vue | le **mode de rendu** (ou les réglages) |
| Séparateur | une cloison entre panneaux | un **trait de coupe** dans une image |
| Précédent connu | Blender, Maya | comparateur avant/après |

**Multi-vue** — la disposition classique de modélisation : quatre quadrants,
face / côté / dessus / perspective, chacun avec sa caméra et son mode. C'est de
la **mise en page de panneaux**, proche de ce que fait déjà le système de
séparateurs de l'interface.

**Séparateur univue** — décrit ci-dessous. C'est celui auquel Rihen tient le
plus, et le moins courant des deux.

#### Séparateur univue — comparer deux rendus sur la MÊME image

Diviser la vue 3D en deux — ou en N — **non pas pour montrer deux vues
différentes**, mais pour montrer **le même point de vue rendu de deux façons** :
fil de fer contre solide, solide contre rendu, ou deux réglages de rendu
distincts. Un séparateur déplaçable fait glisser la frontière ; plus on le
bouge, plus la découpe est inégale.

**Ce qui fait tout l'intérêt, et qui doit guider l'implémentation :** ce n'est
pas un écran partagé. C'est **une seule image, une seule caméra**. Tourner la
vue fait tourner les deux côtés ensemble, parce qu'il n'y en a qu'une. L'illusion
recherchée est celle d'une image qu'on **découpe** : à gauche elle montre une
chose, à droite une autre, et le séparateur est le trait de coupe.

Conséquences techniques à prévoir :

- **Une seule caméra, un seul état de scène.** Les deux côtés partagent tout sauf
  le mode de rendu (et, à terme, un jeu de réglages). Toute tentation de tenir
  deux caméras est à écarter : elle briserait la promesse.
- **Deux passes de rendu, un seul assemblage**, avec un masque de découpe — c'est
  exactement ce que fait déjà `NkInsetCompose` pour les incrustations, à ceci
  près que la forme est ici un demi-plan mobile. La brique de composition existe.
- **Ça doit sortir en image.** Une comparaison qui ne se capture pas ne sert
  qu'à l'écran ; la pastille Output doit pouvoir la produire, séparateur compris.
- **N côtés, pas seulement deux** — prévoir la généralisation dès la structure de
  données, même si l'interface commence à deux.

Voisin utile : le même mécanisme permettrait un « avant / après » sur un réglage
qu'on modifie, ce qui est le meilleur outil pédagogique pour un tutoriel.

**Les deux peuvent coexister** : une multi-vue dont l'un des quadrants porte lui-
même un séparateur univue. C'est une raison de plus pour ne pas les bâtir sur le
même mécanisme — l'un découpe des **panneaux**, l'autre découpe une **image**.

### Récepteur d'ombre (*shadow catcher*) — demandé par Rihen

Un sol qui **ne se peint pas** mais **reçoit les ombres** : c'est ce qui permet
de détourer un objet sur fond transparent sans qu'il paraisse flotter. Sans lui,
couper le sol emporte l'ombre avec, puisqu'elle est projetée *sur* lui.

**Le sol est un mesh plan ordinaire** avec un matériau standard, rendu par le
PBR — pas de shader dédié. Deux voies, et elles n'ont pas le même coût :

**A. Par différence de rendus** — utilise la machine multi-passes existante,
aucun shader touché :

1. scène **avec** sol, ombres actives → `A`
2. scène **avec** sol, ombres coupées → `B`
3. scène **sans** sol → `C` (déjà produit par le fond transparent)

L'ombre vaut `1 − A/B` là où le sol est visible ; `C` fournit les objets et
leur alpha. On compose l'ombre dessous, les objets dessus. Chaque étape étant
elle-même doublée par la reconstruction d'alpha, cela fait **cinq à six rendus**
pour une image — acceptable pour une image fixe, exclu pour la vidéo.

**B. Par matériau dédié** — un shader de sol qui écrit `couleur = noir` et
`alpha = 1 − visibilité de l'ombre`. Un seul rendu, résultat exact, et
utilisable en vidéo. Mais il faut que le PBR expose la visibilité d'ombre à un
matériau, ce qui n'existe pas aujourd'hui.

**Recommandation** : commencer par **A**, qui donne le résultat tout de suite
sans toucher au moteur, et garder **B** pour quand le chantier « alpha porté par
la chaîne » (voir plus haut) sera engagé — les deux ont besoin de la même
chose : que le rendu sache transporter une couverture.

### Ce qui reste à faire sur Output

Le chantier est **terminé** au 5 août : fond transparent (alpha porté par la
chaîne, plus de double passe), récepteur d'ombre, sept formats d'image, cinq
sorties vidéo dont MP4/H.264, enregistrement de la vue **et** du tutoriel.
Restent :

1. **F12** — c'est le raccourci de rendu chez Blender, mais il est déjà pris ici
   par l'opacité du plan de grille (un vestige de la démo). Je n'ai pas
   réquisitionné le raccourci sans ton accord : à trancher.
2. **Rendu depuis plusieurs caméras vers plusieurs fichiers** plutôt qu'en
   incrustation — un fichier par caméra en une seule commande.
3. **Rendu d'animation** sur la plage d'images : elle est réglable mais sans
   effet tant qu'il n'y a pas de timeline. Le champ reste, il attend sa
   fonction (annoncé comme tel dans le panneau).

## CHANTIER A — l'alpha porté par la chaîne (5 août, compilé, non relancé)

Le fond transparent, le récepteur d'ombre et la vidéo transparente butaient
**au même endroit** : `PP_FXAA/NkSL/pp_fxaa.frag.nksl` et
`PP_Tonemap/NkSL/pp_tonemap.frag.nksl` terminaient par `vec4(rgb, 1.)`. Les
deux corrigés, les trois se débloquent — et la double passe (rendre noir puis
blanc, reconstruire l'alpha) se **désactive d'elle-même** par détection.

- Mesuré : fond alpha **0**, géométrie alpha **255 exact** (245 par
  reconstruction), une seule passe, tous backends.
- FXAA prend l'alpha du pixel **central** : mélanger les alphas des voisins
  étalerait le bord au lieu de le lisser.
- **Récepteur d'ombre** (`NkMaterial::SetShadowCatcher`) : le matériau ne rend
  que la **couverture** de l'ombre, en alpha. L'ambiante entre des deux côtés
  du rapport, sinon l'ombre sort noire et opaque. Vérifié : alpha moyen **206**
  sans ciel, **77** avec le ciel ajouté — c'est la scène qui décide.
- Corrigé au passage dans le moteur : `NkRendererImpl` effaçait la passe
  `DeferredLight` avec une couleur **écrite en dur** — `SetBackgroundColor`
  était sans effet dès que le différé tournait, pour **toute** application.

### Enregistrement — deux prises en parallèle

La vue et le tutoriel partagent la même mécanique (`HostRecStartOn/StopOn/
Enqueue/WaitSlot/EncodeLoopOn`) sur deux instances de `NkVpRec`, et peuvent
tourner **en même temps** : deux points de vue d'une même session, deux noms de
base distincts pour que les fichiers ne s'écrasent pas.

- **Tutoriel en vidéo** : la fenêtre entière, capturée après `EndFrame()` —
  seul instant où elle affiche l'image de *cette* frame.
- **MP4/H.264** ajouté au choix de sortie (l'encodeur muxe lui-même) ; qualité
  1–100 convertie en QP borné 12–48.
- **Qualité vidéo séparée** de la qualité image : elles étaient partagées, si
  bien que soigner un JPEG alourdissait toutes les prises. Le curseur disparaît
  pour la suite d'images PNG, qui est sans perte.
- **Barre d'enregistrement dans le pied de page** : visible seulement pendant
  une prise, elle dit le type, le temps écoulé, les images sautées, et n'offre
  que les trois décisions réelles — Pause / Arrêter / Abandon (en rouge : il
  efface). Trois **icônes**, pas trois libellés : le transport a un dessin
  universel, et `media-pause/play/stop/record` ont été dessinés pour le projet.

### Mise à l'épreuve du 5 août — ce qu'elle a corrigé

- **Le MP4 sortait onze fois trop rapide.** Le journal chiffrait la cause : 86
  images en 39 s pour le `.mp4` (2,2 i/s) contre 765 en 36 s pour le `.avi`
  (21 i/s). L'encodeur H.264 est trop lent pour le temps réel, la file saturait,
  et le temps sauté **disparaissait** au lieu d'être comblé.
  → **Encodage différé** (choix de Rihen) : on filme en images — PNG si la
  transparence est demandée, JPEG sinon — et le MP4 est encodé à l'arrêt, hors
  temps réel, sur son fil. La barre affiche `encodage 312/765` en ambre. Le
  dossier temporaire n'est effacé qu'après un encodage **complet**.
- **Conteneur et codec séparés**, comme Blender : Suite d'images / AVI /
  QuickTime / MPEG-1 / MP4, la liste des codecs suivant le conteneur. Expose au
  passage l'**AVI non compressé**, que NKMedia savait écrire sans que personne
  puisse le choisir — et corrige la suite d'images, **figée en PNG** malgré son
  combo.
- **Deux prises portaient le rang 006** : `mp4` manquait dans la recherche du
  premier rang libre, et le compte du tableau était écrit en dur à `3`.
- **Le curseur est dessiné dans la vidéo de tutoriel** : `PrintWindow` ne
  capture pas le pointeur, d'où des menus qui s'ouvraient tout seuls. Flèche
  blanche bordée de noir + trace des 24 dernières positions, échantillonnée à
  chaque image (à la cadence de capture, elle sauterait). Case dédiée, active
  par défaut, sans effet sur les images fixes.

### Reste sur la vidéo

- **Intervalle d'image clé** (GOP) : réglable dans `NkH264Encoder`, aujourd'hui
  codé en dur à une seconde. À sortir dans le panneau, comme le *Keyframe
  Interval* de Blender.
- **Profondeur 10 bits, vitesse d'encodage, B-frames, piste audio** : Blender
  les propose, notre encodeur ne les gère pas. À ne pas afficher tant qu'ils
  n'existent pas.
- **Accélérer H.264** reste le seul chemin vers un vrai MP4 en temps réel.

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
