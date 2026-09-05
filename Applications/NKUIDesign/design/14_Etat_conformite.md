// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# État de conformité — ce que NkUIDesign fait des outils de référence, et ce qu'il ne fait pas encore

> **Demande de Rodolf, 2026-09-02** : *« tu ne m'as pas encore dit tout ce qui est
> implémenté venant de Lunacy/Figma/Sketch/Canva et tout ce qui ne l'est pas
> encore. »*
>
> **Ce document se lit en cinq minutes.** Le détail ligne par ligne — 174 lignes
> de comportement avec leur geste, leur état et ce qui manque — reste dans
> `13_Lunacy_reference_interaction.md`. Ici, on regroupe.

**Dernière régénération : 2026-09-05** (compte identique au 02/09 : le lot « variable dans l'interface » et le rail Variables laissent les deux lignes concernées **partielles**, ce qui est vrai — voir doc 13 §8.2 / §8.4). La partie chiffrée est produite par
`python compte_etat.py --document` ; **elle ne s'écrit pas à la main.** Le reste
— les familles, les trois groupes, les coûts — est du **jugement**, et le script
n'y touche jamais.

---

## 0. ⚠️ CE QUE CE DOCUMENT COUVRE VRAIMENT — à lire avant les chiffres

**Il couvre LUNACY**, dépouillé page par page le 01/09 et re-vérifié le 02/09.
C'est le seul des quatre outils dont la documentation a été lue en entier.

⚠️ **Figma, Sketch et Canva n'ont PAS été dépouillés.** Je ne vais pas laisser
croire le contraire : ce serait exactement le genre de vert qui ment que ce
chantier chasse. Ce qui est vrai, et qui est déjà beaucoup, est en **§5**.

---

## 1. LES CHIFFRES — comptés, jamais saisis

*Tout ce qui suit jusqu'au marqueur de fin est **régénéré** : les nombres et les
libellés sont lus dans la colonne d'état du document 13. **Ne rien y écrire à la
main** — la prochaine régénération l'effacerait.*

<!-- AUTO:DEBUT — regenere par `compte_etat.py --document`. NE RIEN ECRIRE ENTRE LES DEUX MARQUEURS. -->

### Le compte global

| lignes de comportement | livré | partiel | absent | écarté |
|---|---|---|---|---|
| **191** | **86** | **32** | **72** | **1** |

### Par chapitre — où l'on est fort, où l'on est faible

| chapitre | livré | partiel | absent | écarté |
|---|---|---|---|---|
| 1. Édition vectorielle | 7 | 4 | 1 | 0 |
| 🔑 « MIROIR » DÉSIGNE **TROIS CHOSES DIFFÉRENTES** | 14 | 1 | 6 | 0 |
| 2. Outils de tracé et leurs modificateurs | 7 | 7 | 7 | 0 |
| 3. Opérations booléennes et de forme | 0 | 0 | 8 | 0 |
| 4. Sélection et navigation | 8 | 0 | 5 | 0 |
| 5. Toile et vue | 9 | 5 | 12 | 0 |
| 6. Transformations | 16 | 1 | 5 | 0 |
| 7. Calques et groupes | 9 | 1 | 6 | 1 |
| 8. Propriétés | 11 | 12 | 18 | 0 |
| 9. Composants et instances | 5 | 1 | 4 | 0 |

### La liste nommée — PARTIELS

**1. Édition vectorielle**

- Entrer en édition de forme
- Sortir
- Le curseur devient la plume, et la section `Edit shape` apparaît
- Sélectionner plusieurs points

**🔑 « MIROIR » DÉSIGNE **TROIS CHOSES DIFFÉRENTES****

- Escamoter une poignée d'un point déconnecté

**2. Outils de tracé et leurs modificateurs**

- Rectangle
- Rectangle arrondi
- Ovale / Triangle / Polygone / Étoile
- Ligne / Flèche
- Sélection
- Cadre (frame)
- Arrondir un seul coin

**5. Toile et vue**

- Défiler / déplacer la vue
- Règles
- Grille carrée
- Mode présentation
- Exporter en SVG (page, sélection, images embarquées)

**6. Transformations**

- Taille exacte, verrou de proportions

**7. Calques et groupes**

- Créer un cadre

**8. Propriétés**

- Une ligne = pastille (aperçu réel) · nom · œil · poubelle ; le détail dans le popover
- Appliquer à une multi-sélection ; « contenu mixte » signalé
- Dégradés linéaire / radial / angulaire, avec étapes de couleur déplaçables, poignées sur la toile
- Remplissage image et ses cinq cadrages (`Fill` · `Fit` · `Stretch` · `Tile` · `Crop`)
- Saisie HEX / RGBA / HSB / HSL
- Position intérieure / centrée / extérieure
- Extrémités (*caps*), jonctions (*folds*), têtes de flèche
- Ombre interne
- Mode de fusion PAR REMPLISSAGE (18 modes : Normal, Darken, Multiply, Plus Darker, Color Burn, Lighten, Screen, Plus Lighter, Color Dodge, Overlay, Soft Light, Hard Light, Difference, Exclusion, Hue, Saturation, Color, Luminosity)
- Styles de couleur nommés (la ligne de remplissage porte « Dark Primary ») et `Styles` / `Variables` au rail de gauche
- Opacité + mode de fusion du CALQUE (section `LAYER`)
- Alignement horizontal (4)

**9. Composants et instances**

- États d'un composant (Défaut / Survol / Pressé)

### La liste nommée — ABSENTS

**1. Édition vectorielle**

- Supprimer un point à la souris

**🔑 « MIROIR » DÉSIGNE **TROIS CHOSES DIFFÉRENTES****

- `OnlyFrom` / `OnlyTo`
- La rangée d'icônes montre des icônes
- Fermer un tracé
- Ouvrir un tracé fermé
- Union / Soustraction / Intersection / Différence
- Vectoriser un texte

**2. Outils de tracé et leurs modificateurs**

- Plume (tracé point par point)
- Crayon (main levée)
- Éditeur d'arc d'un ovale (*Sweep*, *Ratio*, *Start*)
- Nombre de branches d'une étoile / d'un polygone (*Count*)
- Ratio d'une étoile
- Arrondi sur la toile des formes prédéfinies
- Coins lisses type iOS

**3. Opérations booléennes et de forme**

- Union
- Soustraction
- Intersection
- Différence
- Le résultat reste non destructif — les formes restent modifiables individuellement
- Aplatir (*Flatten*) — fusionne définitivement
- Vectoriser une bordure / un texte (*Outline stroke*)
- Masque (*Toggle mask*)

**4. Sélection et navigation**

- Sélectionner tous les cadres
- Rendre la sélection profonde permanente pour un groupe (case à cocher)
- Sélectionner un calque verrouillé par la liste
- Chercher un calque par son nom
- Sélectionner tout ce qui utilise une couleur / une police

**5. Toile et vue**

- Zoom fin
- Outil Zoom (`Z`, `Alt+Z`)
- Guides (créer, déplacer, supprimer)
- Désactiver l'aimant pendant un glisser
- Grille de mise en page
- Grille de pixels au-delà de 500 %
- Pixels au zoom (rendu réel au-delà de 100 %)
- Couleur de la toile
- Cadre précédent / suivant, page précédente / suivante
- Exporter en PDF
- Exporter en code (HTML / CSS / JS, React, Next)
- Tranches d'export (*export slices*, presets par calque, JPEG / WebP)

**6. Transformations**

- Redimensionner de 1 px / 10 px
- Redimensionner un cadre sans son contenu
- Outil Échelle (met aussi bordures et effets à l'échelle)
- Ranger en grille (*Tidy up*)
- Mesurer la distance à un autre calque

**7. Calques et groupes**

- Duplication répétée qui rejoue le dernier décalage
- Copier / coller le style
- Grille répétée (*Repeat grid*)
- Replier toute la liste
- Liste intelligente (n'affiche que le visible)
- Déplacer vers une autre page

**8. Propriétés**

- Réordonner (l'ordre change le rendu)
- Pipette pour prélever une couleur (capture 04/09 : en bas à gauche du popover)
- Règle de remplissage non-zero / pair-impair
- Mode de fusion par remplissage
- *Tints* (une teinte unique sur un groupe ou une instance)
- Pointillés (tiret / écart)
- Un tracé ouvert n'accepte que la position « centrée »
- Flou gaussien
- Flou d'arrière-plan
- Gras / italique / souligné
- Interligne, interlettrage, espacement de paragraphe
- Position verticale dans la boîte (3)
- Ajustement automatique : largeur libre / hauteur libre / taille fixe
- Casse (Titre / MAJUSCULES / minuscules)
- Décorations (souligné, barré), listes, exposant/indice
- Texte sur un chemin
- Troncature par points de suspension + nombre de lignes max
- Texte généré (contenu factice)

**9. Composants et instances**

- Réinitialiser les surcharges
- Aller au composant principal / revenir
- Échanger le composant d'une instance
- Supprimer un composant → ses instances deviennent des cadres

### La liste nommée — ÉCARTÉS

**7. Calques et groupes**

- Z-index automatique (le petit passe au-dessus du grand)

<!-- AUTO:FIN -->

---

## 2. CE QUI EST IMPLÉMENTÉ — par famille

*Une phrase par famille plutôt que la liste brute des livrés — le compte exact est
en §1, et **on ne le recopie pas ici** : un nombre écrit à deux endroits est un
nombre qui finira par se contredire. Chaque famille est adossée à des cas de
recette : « livré » se mesure, il ne se déclare pas.*

- **Le mode édition de forme** — on y entre au double-clic et on en sort
  (`Échap`, clic dans le vide, bouton « Terminer ») ; le panneau droit change en
  même temps que la toile.
- **Les sommets** — déplacer un sommet ou **plusieurs ensemble** (d'un écart, pas
  d'une position absolue), en ajouter un sur le tracé au simple clic (**droit**)
  ou au double-clic (**courbe, Miroir, poignées posées**), arrondir un coin au
  double-clic ou par le champ « R ».
- **Les courbes de Bézier** — les **quatre types de point** de la source
  (Droit / Miroir / Asym. / Libre), les poignées qui se voient, se saisissent et
  se tirent, `Alt`/`Ctrl` qui cassent la liaison pendant le geste, les champs
  X1/Y1/X2/Y2, et l'**amorce automatique** des tangentes quand un sommet devient
  courbe.
- **Le remplissage** — le peintre remplit les formes **concaves** (triangulateur
  descendu sous le peintre, sans allocation dans la boucle de dessin).
- **La sélection** — clic, multi-sélection, élastique, sélection profonde,
  descente dans un groupe, tout sélectionner, désélectionner ; la toile et la
  Hiérarchie s'accordent.
- **Les transformations** — déplacer (souris, flèches 1 px / 10 px, champs X/Y),
  **contrainte d'axe** au `Maj`, redimensionner par les **huit** poignées avec
  **`Maj` (proportions)** et **`Alt` (depuis le centre)**, rotation (souris,
  champ, aimant 15°), miroirs H/V, les six alignements, et **`Alt`+glisser pour
  dupliquer**.
- **Les calques** — grouper / dégrouper, copier / couper / coller / dupliquer
  (souris **et** raccourcis), réordonner dans la liste, **ordre de profondeur**
  (les quatre gestes, **par le menu et au clavier**), renommer, supprimer,
  recherche dans le menu contextuel.
- **Les propriétés** — remplissages, bordures et effets en **listes empilables**
  (ajouter, masquer sans supprimer, supprimer), couleur unie, opacité par
  remplissage, épaisseur, ombre portée, police / graisse / taille / couleur.
- **La toile** — zoom, panoramique, aimantation aux objets et aux guides **avec
  les distances affichées**.
- **Aligner la sélection** (05/09) — six alignements et deux répartitions, la référence
  dite avant le clic (la **sélection**, le **dernier sélectionné**, ou **la page** quand un
  seul objet est choisi) ; ce qui ne peut pas bouger — un nœud dont le parent agence ses
  enfants — est laissé, compté et **dit**. ⚠️ **À ne pas confondre avec la section
  ALIGNEMENT**, qui règle l'alignement des **enfants** d'un cadre : c'est elle qui portait,
  jusqu'au 05/09, un ✅ « Aligner » dans le document 13. Sonde 93.
- **L'export** (05/09) — une page ou la sélection en **PNG** à ×1 / ×2 / ×3 par le
  **même peintre que la toile** rendu sans GPU (`NkGuiDrawListRaster`, NKGui), le
  texte rastérisé à la taille exacte ; en **SVG** par un lecteur du document ;
  la destination par le sélecteur de fichier du kit, le pied dit le chemin écrit
  ou l'échec. Sondes 81-82 (pixels ; SVG re-rastérisé par `NkSVGCodec`).
- **L'aperçu pendant le tracé** (05/09) — la **forme réelle** se peint pendant le
  glisser de création (ellipse, triangle, étoile, ligne, flèche, arrondi…), avec le
  costume qu'elle aura ; la boîte en pointillé et la pastille de taille restent
  par-dessus. **Une seule table de genre** est lue par l'aperçu et par le relâchement
  — sans quoi la forme créée pourrait différer de celle montrée. Sonde 94.
- **L'opacité a sa propre ligne** (05/09) — dans le sélecteur de couleur, une barre
  sur damier qui va de la couleur **transparente** à la couleur **pleine**, un
  curseur, et le champ numérique qui reste : deux commandes, **un seul modèle**
  (le curseur n'a aucune mémoire propre). Sonde 95.
- **Un seul dialogue d'export** (05/09) — le menu, **Ctrl+E** et le **clic droit**
  ouvrent la **même** porte, qui demande le format (PNG / SVG ; PDF et Code
  **nommés et grisés**), l'échelle, l'étendue (page / sélection), la sortie (une
  image / **un fichier par objet**) et le nom. Le nom proposé est celui de
  **l'objet** — page, groupe, graphique — assaini pour le système de fichiers
  (les accents restent), avec `@2x` quand l'échelle change. **Tout s'exporte** :
  un nœud, un groupe **avec ses enfants**, plusieurs objets en une image ou un
  fichier chacun (un doublon devient ` (2)`, jamais un écrasement silencieux).
  ⚠️ **Ctrl+E n'est déclaré qu'une fois** — la leçon de Ctrl+D. Sonde 96.
- **Le sélecteur de fichiers à deux volets** (05/09) — rail de dossiers à gauche
  (disques, Accueil, Bureau, Documents, Téléchargements, Images, favoris, puis la
  chaîne du dossier courant), **vignettes** à droite, fil d'Ariane, recherche, tri,
  filtre d'extension, champ de nom. Il **ne contient aucun navigateur** : le volet
  droit **est** `NkDrawContentBrowser` du kit. L'ancien sélecteur — une colonne
  unique — **reste** et sert encore à « Choisir une image… » ; le nouvel état
  dérive du même `NkFilePickerState`, donc le même contrat de confirmation.
  ⚠️ **Mesure qui contredit la prémisse** : NKCode n'avait pas de sélecteur à lui,
  il héritait déjà de celui du kit. Sonde 97.
- **Le sélecteur de fichiers, après les six retours du soir** (05/09) —
  ① il est **plein dès la première image** : `pickerPath` est le **seul** endroit
  où vit le chemin, et la liste le suit ; personne n'a plus rien à « armer »
  (sonde 98). ② le rail prend **la forme de l'explorateur** : trois sections
  titrées et repliables — « Récents », « Accès rapide », « Ce PC »,
  « Dossier courant » — les dossiers usuels **lus du système**
  (`NkDirectory::GetUserFolder`, nouveau dans NKFileSystem) et les volumes
  **montés** avec leur étiquette (sonde 99). ③ les **dossiers récents**, deux
  listes : la **session** (mémoire vive) et le **document** (écrit dans le
  `.nkuidoc`, il voyage avec lui) — l'aller-retour est mesuré (sonde 100).
  ④ le dialogue **ne montre que ce que son mode exige** : ni bande « Contenu »,
  ni « Créer / Importer / Tout enregistrer », ni « Tout sélectionner » quand un
  seul objet peut être choisi (sonde 101). ⑤ le **résultat exporté se voit** :
  un bandeau qui **reste**, avec la **vignette du fichier relu du disque**, ses
  dimensions, et deux portes vers le système (`NKPlatform/NkShell`, nouveau) —
  qui **rendent faux** plutôt que de mentir, le chemin restant copiable
  (sonde 102). ⑥ il est **l'outil par défaut du kit** : `NkDrawSelecteur` en
  **une ligne**, les **quatre modes** (ouvrir un fichier, ouvrir un dossier,
  **créer un dossier**, enregistrer sous), NkUIDesign a basculé ses deux sites
  et l'ancien reste appelable (sonde 103).
  ⚠️ **Aucune autre application ne bascule toute seule** — chacune possède son
  appel de dessin. Deux lignes pour NK3DModeler (`main.cpp:1781` et le type de
  son état) ; NKCode demande d'abord de décider ce que devient son panneau
  supplémentaire.
- **Le sélecteur, après la deuxième passe de Rodolf** (05/09, nuit — « c'est mal
  conçu, pourtant le design est joli »). Sept corrections, et **cinq défauts sur
  sept étaient les miens, pas ceux du kit** :
  ① les trois sections **étaient bien peintes** (mesuré — sonde 105) ; c'est la
  **chaîne d'ancêtres** de la section « Dossier courant » qui les repoussait hors
  du champ et jetait les sous-dossiers à la profondeur 6, où il reste **dix
  pixels** pour un nom : d'où les « … ». Elle quitte le rail — le fil d'Ariane la
  porte déjà, et il est cliquable (profondeur max : 2).
  ② la largeur du rail est **en pixels** (minimum 180, poignée de
  redimensionnement) au lieu d'une **fraction** du volet, et la troncature garde
  **le début ET la fin** (`Nkentseu-…-noge`) — deux dossiers frères se
  distinguent par leur fin (sonde 108).
  ④ la carte **sélectionnée s'effaçait elle-même** : son marquage appelait
  `Outline`, dont le contrat est « plein **puis** creusement » (sonde 106).
  ⑥ la section « ALIGNER LA SÉLECTION » était listée **deux fois** dans une table
  et **absente** de l'autre ; et `TexteTronque` cassait l'UTF-8 — ellipsis
  **mojibake** de quatre octets dont on n'en copiait que trois, et **terminateurs
  écrits en espaces** (sonde 107).
  ⑤ « Nouveau dossier » devient un **bouton discret** en haut (sonde 109).
  ⑦ **un seul lanceur système** : `NkLauncher` (NKWindow) existait déjà et couvre
  **sept plateformes** ; `NKPlatform/NkShell`, écrit la veille, est **supprimé**,
  sa seule vraie valeur (sélectionner le fichier dans l'explorateur) portée dans
  `NkLauncher::RevealFile`.
  ⚠️ **Trois de mes sondes étaient vertes pendant que Rodolf voyait le défaut** :
  la 92 ne mesurait qu'une **largeur** (pas la chaîne tronquée), la 99 ne
  mesurait que **le modèle** (pas le dessin), et la première version de la 105
  comptait des rangées que `NkRecordingPaint` enregistre **même hors champ**.
- **Où vit le sélecteur de fichiers, et pourquoi** (question de Rodolf, 05/09) —
  il est dans **NKEditorKit** (`Engine/NKEditorKit/NkFilePickerNav.h`), **pas**
  dans NKGui, et c'est le bon endroit. NKGui est la bibliothèque d'**interface
  immédiate** — boutons, fenêtres, listes, dessin ; NKEditorKit est la couche
  d'**outils d'éditeur** posée au-dessus, que **les neuf** applications
  consomment déjà. Le descendre dans NKGui y ferait descendre avec lui le
  **système de fichiers**, les **vignettes** et le **cache d'images** — trois
  choses qu'une bibliothèque de dessin ne doit rien savoir. Une application
  non-éditeur qui en voudrait consomme NKEditorKit, exactement comme les neuf.

- **Le sélecteur après la troisième passe de Rodolf** (05/09, nuit — sept
  retours) : ④ les boutons **débordaient de quatre pixels** (trois hauteurs
  posées à la main dont la somme dépassait la réserve) ; la géométrie est
  désormais une **fonction nommée** que le dessin et la sonde appellent
  (sonde 110). ① la grille laissait une **bande morte à droite** — le reste de
  la division se répartit sur les colonnes (sonde 111). ②③ les dossiers ont une
  **silhouette** et chaque type son **icône dessinée** — onze natures, onze
  empreintes distinctes, **jamais un glyphe de police** ; un `.png` sans vignette
  ne ressemble plus à un dossier (sonde 112). ⑥ une **API de filtres** nommés,
  choisis dans un combo, valable dans les quatre modes, **les dossiers toujours
  visibles** (sonde 113). ⑤ le tri devient un **combo** : nom, date, taille,
  type, dans les deux sens — date et taille étaient « nommés, non faits », le
  système de fichiers les donnait déjà (sonde 114). ⑥ le rail montre un **nom**
  et non un chemin, et son **infobulle** porte le chemin complet — le champ que
  j'avais déclaré impossible est **additif**, défaut vide (sonde 115). ⑦ un seul
  lanceur système : `NkLauncher`.
- **Ce que Lunacy n'a pas et que nous avons** (§11.3 du document 13) :
  l'**agencement** calculé, l'**ancrage**, les **cibles** et **points de
  rupture**, les **rôles de thème** et les **langues**. *Aucun n'est sacrifié
  pour ressembler à Lunacy.*

---

## 3. CE QUI N'EST PAS IMPLÉMENTÉ — en trois groupes, pour décider

*Le classement n'est pas par chapitre mais par **ce qu'il faut payer**. C'est ce
qui permet de choisir la prochaine vague.*

### (a) 🔑 LE SOCLE DES TOUCHES — ✅ **FAIT le 02/09**, et le diagnostic était faux

**Ce groupe est vidé.** L'alphabet de `NkGuiKey` est complet (A..Z, ajout additif
en fin d'énumération, accord de Rodolf), la traduction OS→NkGuiKey de
`NkEditorShell` est complétée, et ce que ça débloquait est branché.

🔴 **MAIS TROIS DES CINQ TOUCHES QUE J'AVAIS DÉCLARÉES MANQUANTES NE MANQUAIENT
PAS.** Je l'écris ici parce que c'est ce document que Rodolf lit :

| ce que j'avais annoncé | la mesure |
|---|---|
| `[` et `]` manquent | **faux** — présentes **et** traduites depuis le lot NKCode. Le seul manque était dans **notre** table `NkActionDuRaccourci` |
| `Z` manque | **faux** — présente et traduite |
| `1..6` manquent | **faux dans l'enum** — elles y étaient ; c'est la **traduction** qui s'arrêtait à `NK_NUM2` |
| `Ctrl+A` est bloqué | **faux** — il ne passe même pas par `NkGuiKey`, il arrive par l'intention `wantSelectAll`, et il **marchait déjà** |
| `R` manque | **vrai** — la seule exacte |

> *J'avais accusé la couche du dessous sans aller regarder. La vérification a
> coûté un `grep`.* La porte du dépôt demande de **chercher en bas avant
> d'écrire** ; ce cas ajoute son symétrique : **chercher en bas avant
> d'ACCUSER.**

🔴 **ET LE VRAI DÉFAUT ÉTAIT D'UNE AUTRE NATURE.** `NkGuiTypes.h` portait **deux**
signalements disant que `Ctrl+1..6` et `Ctrl+,` manquaient « faute de code de
touche ». Les valeurs étaient là depuis le lot du launcher : c'est
`NkEditorShell` qui ne les **émettait** pas. *Une valeur d'énumération que
personne n'émet est aussi morte qu'une valeur absente — et elle est **pire**,
parce qu'elle a l'air présente.* On la lit dans l'enum, on conclut que le socle
sait la recevoir, et on cherche le défaut chez l'appelant. Deux signalements
successifs ont cherché du mauvais côté.

**Ce qui reste de ce groupe** : les **zooms** `Ctrl+0..4` et l'**outil Zoom**
(`Z`) — désormais de simples branchements côté application (groupe **c**), plus
aucun blocage moteur.

### (b) 🏗️ VRAI CHANTIER DE MODÈLE — nommés avec leur coût relatif

| chantier | ce qu'il ouvre | coût |
|---|---|---|
| ~~Verrouiller / masquer~~ | ✅ **FAIT le 02/09** — pointage, dessin, héritage, format additif, **et les deux icônes** | — **la vague 2 est close** |
| **Tracés ouverts** (`fermé` par nœud) | « Close path », « Open path », extrémités (*caps*), **et la plume** | **moyen** — le peintre et le round-trip doivent l'honorer |
| **Dégradés + remplissage image** | 3 lignes de §8, très visibles en maquette | **moyen** |
| **Texte : ajustement auto, troncature, décorations** | ~8 lignes de §8 | **moyen**, découpable en petits lots |
| **Opérations booléennes** (tracé à **contours multiples**) | **les 8 lignes de §3** + la vectorisation du texte | **gros** — le maillon du remplissage est fait, il reste le modèle |
| **Alignement VERTICAL du texte dans son cadre** (haut / milieu / bas) et **justifié** | les deux lignes de §8.5 que TYPOGRAPHIE ne peut pas cocher | **petit à moyen** — le modèle ne porte pas la clé (`texte_aligne` ne dit que l'horizontal) et le peintre **centre toujours** verticalement : il faut une clé additive, un peintre qui l'honore, et un témoin à l'enregistreur. Le **justifié**, lui, exige le retour à la ligne — il n'est pas de la même famille, et il attend `NkGuiDrawList` côté mesure de mots |
| **Export PDF** | la forme qu'un client ouvre sans rien installer (`ROADMAP_PRODUITS.md` §1) | **moyen** — **le même arbre que le SVG** (`ExportSVG.h` parcourt déjà le document nœud par nœud) traduit en objets PDF : un flux de contenu par page (`re`, `f`, `S`, `cm` pour la matrice, `sh` pour les dégradés — ou des bandes comme le peintre), les images en `XObject` (le PNG re-encodé en `FlateDecode`, NKImage a le déflate), **les polices à embarquer** (Inter en `FontFile2`, la sous-table des glyphes utilisés — c'est le seul morceau sans base dans le dépôt), la table `xref` ; témoin : le PDF relu par un lecteur tiers **et** un parseur minimal maison qui compte les objets |
| **Export code — HTML / CSS / JS, React, Next** | *futur proche* (Rodolf, 02/09) : la contrepartie web du même arbre | **moyen à gros** — **un lecteur de plus du format, jamais un second modèle** : les **composants de base** (`ComposantsBase.h`, bouton / champ / case / interrupteur / progression / étiquette / séparateur / carte) deviennent des éléments ou des composants React nommés, les **styles de calque et de texte** (§15.15) des classes CSS **nommées**, les **variables de couleur** des `--custom-properties`, l'agencement (colonne / ligne / grille / libre / ancrage) du `flex` / `grid` / `position:absolute`, les états d'apparence des pseudo-classes ; Next = React + un fichier de page. Ce qu'il exige d'abord : **une table de correspondance écrite** (nœud → balise, rôle de thème → variable CSS) et un témoin qui **relit le HTML produit** (compte des éléments, des classes) — la maison n'a pas de moteur HTML pour un témoin en pixels |
| **Composants de document** | ✅ **socle POSÉ le 02/09** (étapes 1-3 du `15_…`) : modèle + format additif + identité d'auteur + **porte de fork**, extraire/détacher **ensemble** avec l'aller-retour neutre, et le retour visuel. **Le GESTE existe** (menu + `Ctrl+Alt+K` + dispatcher), donc Rodolf peut créer un composant à la main. Le chapitre 9 passe de **0/10** à **3 livrés + 1 partiel** | **reste moyen** — puis les **états** (⚠️ à réconcilier avec le mécanisme voisin des composants de code **avant** d'en écrire un troisième) |

### (c) 🔧 DU ROBINET — le mécanisme existe, il n'est pas branché

*Le meilleur rapport après (a). Chacun est un branchement, pas une conception.*

- ✅ ~~`Suppr` sur un sommet~~ — **fait le 02/09** (cas 51) ;
- **élastique sur les sommets** — l'élastique existe sur les objets ; il faut
  distinguer clic et glisser au relâchement, sans rouvrir la porte de sortie du
  mode ;
- **distribuer H / V** et **s'aligner sur les bords du cadre** — la rangée
  d'alignement est là ; il manque le cas « un seul objet » ;
- **mesurer la distance à un autre calque** — l'aimant **calcule déjà** ces
  distances pendant un glisser ; il s'agit de les montrer à froid ;
- **copier / coller le style** — remplissages, bordures et effets sont **déjà**
  des listes séparées ;
- **duplication répétée qui rejoue le dernier décalage** — mémoriser un décalage ;
- **redimensionner au clavier** (`Ctrl`+flèches) — les flèches et le
  redimensionnement existent séparément ;
- **chercher un calque par son nom** — le champ de recherche existe déjà dans le
  menu contextuel ;
- ✅ ~~les deux icônes verrou / œil dans la Hiérarchie~~ — **faites le 02/09** ;
- ✅ ~~les zooms `Ctrl+0..4`~~ — **faits le 02/09** (cas 52) ; **l'outil Zoom
  (`Z`) reste à poser** — c'est un outil, pas un raccourci de vue.

---

## 4. LES ÉCARTÉS — et pourquoi

⚠️ **Une seule ligne de la colonne d'état est marquée « écarté ».** Les autres
refus vivent en **§11 du document 13** comme prose, parce qu'ils portent sur des
familles entières et non sur un geste :

- 🚫 **Z-index automatique** (le petit passe au-dessus du grand) — *un
  réordonnancement que l'utilisateur n'a pas demandé est exactement ce que ce
  chantier refuse. Un outil qui range tout seul est un outil dont on ne prévoit
  pas le résultat.*
- 🚫 **Crop, Rasterize** — ils supposent des **pixels dans le document** ; un
  `.nkuidoc` décrit des **nœuds**. ⚠️ **L'export d'images n'est PLUS ici** : il a
  été écarté à tort jusqu'au 02/09 (Rodolf, `ROADMAP_PRODUITS.md` §1) et **livré le
  05/09** en PNG, partiel en SVG, absent en PDF et en code — chapitre 5 du
  document 13, §3(b) ci-dessus pour ce que PDF et code exigeront.
- 🚫 **Trois endroits où copier la source serait une régression** : la **pipette**
  prélèvera un **rôle de thème** et non une valeur (sinon elle contourne le
  thème) ; leur **texte de remplissage** est du lorem ipsum quand notre **pont
  IA** génère du contenu *à propos* ; leur **Z-index automatique** (ci-dessus).

---

## 5. ⚠️ FIGMA, SKETCH, CANVA — ce qui est couvert, et ce qui ne l'est pas

**Réponse honnête à la question élargie de Rodolf.**

### 5.1 Ce qui EST couvert, par recoupement

**Le socle de l'édition vectorielle est le même dans les trois outils
professionnels**, et le document 13 le couvre donc de fait : types de points,
poignées de Bézier, sélection, transformations, alignement, calques et groupes,
remplissages / bordures / effets empilables. Ce n'est pas une supposition — c'est
**mesuré sur un cas** : en re-vérifiant « Asym. » le 02/09, les trois outils ont
été lus côte à côte, et **Sketch (« Mirror angle ») comme Figma (« Mirror
angle ») décrivent le même type que Lunacy**, aux noms près :

| Lunacy | Sketch (depuis *Dublin*) | Figma |
|---|---|---|
| Mirrored | Mirror angle and length | Mirror angle and length |
| Asymmetric | **Mirror angle** | **Mirror angle** |
| Disconnected | Independent | No mirroring |

*Trois vocabulaires, un seul modèle.* **Suivre Lunacy sur ce socle, c'est suivre
les trois.**

### 5.2 Ce qui n'est PAS couvert — et je ne prétends pas le contraire

**Aucune documentation de Figma, Sketch ou Canva n'a été dépouillée page par
page.** Ce que je sais qu'ils ont **en propre**, et qui n'est **pas** inventorié
ici :

| outil | ce qu'il ajoute de spécifique | pertinence pour nous |
|---|---|---|
| **Figma** | **auto-layout**, **variants** et propriétés de composant, **prototypage** (flux, transitions), *vector networks* (arêtes multiples par point), **variables et modes**, collaboration temps réel | ⭐ **La plus haute des trois.** Et **deux de ces notions sont déjà les nôtres** : l'auto-layout, c'est notre **agencement** ; les variables et modes, ce sont nos **rôles de thème** et nos **cibles**. Un dépouillement Figma vaudrait surtout pour les **variants** et le **prototypage** |
| **Sketch** | symboles et *smart layout*, bibliothèques partagées, *Data* (contenu factice) | moyenne — largement recouvert par Lunacy, qui s'en inspire directement (même format de fichier à l'origine) |
| **Canva** | modèles, banque d'images et de polices, retouche photo, publication et impression, marque | ⚠️ **faible — voir 5.3** |

### 5.3 🔴 MON AVIS SUR CANVA, puisque Rodolf le demande

**Canva n'est pas une référence pertinente pour NkUIDesign, et je le dis
franchement plutôt que de faire semblant de l'inventorier.**

C'est un outil de **composition graphique grand public** : son objet est de
produire une **image finie** (affiche, post, présentation) à partir de
**modèles**, pour quelqu'un qui ne veut pas apprendre un outil de design. Le
nôtre produit un **arbre de composants déclarés** qu'un **moteur relit** — avec
agencement calculé, ancrage, cibles, rôles de thème et langues. **Les deux
n'ont ni le même objet, ni le même utilisateur, ni la même sortie.**

📌 **Ce qu'il y aurait quand même à y prendre, et c'est réel** : sa **prise en
main immédiate** — la bibliothèque de **modèles de départ** (nos mises en scène
en sont l'embryon) et le fait qu'on obtienne quelque chose de présentable **sans
rien connaître**. Si Rodolf veut viser un public non-designer, **c'est ça** qu'il
faut regarder chez Canva, pas ses fonctions.

⚠️ **Et le risque à ne pas prendre** : Canva **range à ta place**. C'est
exactement la famille de comportements que §11 écarte (leur Z-index automatique).
*Un outil qui décide sans qu'on le lui demande est agréable cinq minutes et
ingérable ensuite.*

### 5.4 Ce que je propose

**Un dépouillement séparé de Figma** — pas des trois — **quand la vague 3 sera
finie**, et ciblé sur ce que Lunacy n'a pas : **variants** et **prototypage**.
Sketch n'apporterait presque rien de plus ; Canva relève d'une autre discussion
(le public visé), pas d'un inventaire de fonctions. **Rodolf tranche.**
