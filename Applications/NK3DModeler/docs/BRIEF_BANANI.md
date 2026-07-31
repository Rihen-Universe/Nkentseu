# NK3DModeler — Prompts Banani (esquisses d'écrans)

> Compagnon de `UI_SPEC.md`. Ce document ne décrit pas le comportement (déjà fait
> dans la spécification d'interface) — il traduit chaque écran en **prompt image**
> détaillé, prêt à copier-coller dans Banani, pour obtenir des maquettes visuelles
> fidèles avant de passer au design réel.

## Comment lire ces prompts

Chaque prompt décrit une image **région par région**, avec des proportions
explicites (`occupe les 16 % gauche de l'écran`, `une bande de 6 % de hauteur en
bas`) plutôt que des descriptions vagues (« un panneau à gauche ») — c'est ce qui
donne à Banani assez de contrainte spatiale pour ne pas improviser une disposition
différente à chaque essai. Chaque élément visuel est relié explicitement à sa
fonction (pas juste « une icône » mais « une icône de cube représentant un
maillage ») pour que le résultat reste lisible comme un logiciel professionnel
plutôt qu'une composition abstraite.

Les prompts sont **en français**, comme les libellés de l'interface. Seules les
valeurs qui n'ont pas de traduction restent telles quelles : les codes couleur
hexadécimaux, les proportions chiffrées et les noms de familles typographiques.

**Format d'usage :** coller le bloc *Guide de style* en tête de chaque prompt (ou
l'utiliser comme image de référence une fois un premier écran réussi, cf. notes en
fin de document), puis coller le prompt de l'écran visé.

---

## Ce que le produit est — contexte pour toute la série

**NK3DModeler** est une application de **modélisation 3D** pour ordinateur.
L'utilisateur y crée et modifie des objets en trois dimensions, et empile des
« modificateurs » qui les transforment sans les détruire. Référence d'apparence :
**Unreal Engine 5** — sombre, neutre, dense, professionnel.

> ⚠️ **Contrainte qui prime sur la ressemblance.** Unreal Engine 5 n'est **pas**
> simple à prendre en main. Notre produit doit l'être. Quand la ressemblance à UE5
> et la lisibilité se contredisent, **c'est la lisibilité qui gagne**. En pratique,
> pour les maquettes : **toute icône est accompagnée d'un mot**, sauf pour trois ou
> quatre actions évidentes ; les zones avancées sont **repliées** ; un panneau vide
> affiche une **phrase qui dit quoi faire**, jamais du vide.

---

## Guide de style (à réutiliser dans chaque prompt)

Deux couleurs portent un sens **différent** et ne doivent jamais être confondues :
le **bleu** signale un **état de l'interface** (outil actif, ligne sélectionnée
dans une liste, filtre actif) ; l'**orange** signale la **sélection dans la scène
3D** (contour de l'objet sélectionné, éléments sélectionnés). Le **blanc** est
réservé à l'élément **actif** — le dernier cliqué, celui qui sert de référence.

```
Maquette d'interface d'un logiciel professionnel de modélisation 3D, thème
sombre, dans l'esprit d'Unreal Engine 5 ou d'Autodesk Maya, mais nettement plus
épurée et moins chargée. Application de bureau, composition paysage 16:9, rendue
en illustration d'interface à plat (pas une photo, pas de chrome 3D brillant,
aucun effet de matière).

Palette de couleurs (stricte, utiliser exactement celles-ci) :
- Fond de fenêtre principal : gris neutre presque noir, #1A1A1A
- Fond des panneaux : gris neutre sombre, #242424
- Fond des en-têtes de panneau et des barres d'outils : #2A2A2A
- Fond de la colonne des libellés dans une ligne de propriété, légèrement plus
  sombre que la colonne des valeurs : #1E1E1E
- Bordures et séparateurs : traits fins de 1 pixel, blanc à faible opacité
  (blanc à 8 %). Aucune ombre portée, aucune lueur.
- Accent « ÉTAT DE L'INTERFACE » (bouton d'outil actif, ligne sélectionnée dans
  une liste, puce de filtre active, champ ayant le focus) : bleu #1177D1
- Accent « SÉLECTION 3D » (contour de l'objet sélectionné dans la vue,
  faces/arêtes/sommets sélectionnés) : orange #FF8C0D
- Élément ACTIF (le dernier cliqué, qui sert de référence) : blanc pur #FFFFFF
- Couleurs d'axes, utilisées comme fins bandeaux verticaux de 2 pixels sur le
  BORD GAUCHE des champs numériques : X = rouge atténué #C7404A,
  Y = vert atténué #5A9E3C, Z = bleu atténué #3A6FB0. Le fond du champ
  lui-même reste neutre #1E1E1E — ne jamais teinter le champ entier.
- Couleurs de type d'élément (puces de filtre du navigateur de projet et fine
  barre colorée au bas de chaque vignette) : Maillage = cyan #22B8CF,
  Animation = orange #F08C00, Matériau = vert #37B24D, Texture = magenta #E64980
- Texte : gris clair #E6E6E6 pour le texte principal, blanc à 55 % d'opacité
  pour le texte secondaire et pour la colonne « Type » des arborescences

Typographie : sans-serif technique et compacte (genre Inter ou Roboto), petites
tailles, graisse normale pour les valeurs et demi-grasse pour les titres de
section. Hauteur de ligne serrée (outil professionnel dense, environ 22 pixels
par ligne). Là où les mots exacts ne sont pas précisés ci-dessous, écrire de
courts libellés français plausibles (1 à 3 mots) plutôt que du faux latin ou
des barres grises vides.

Icônes linéaires plates et minimales, traits de 1,5 pixel, très faible rayon
d'arrondi (2 pixels sur les panneaux et les champs, 3 pixels sur les boutons et
les puces). Aucun dégradé, aucun reflet brillant, aucune forme en gélule sauf
pour les petites puces de filtre. Ambiance générale : précise, technique, calme,
non encombrée — un outil professionnel qu'un débutant pourrait néanmoins lire.

IMPORTANT : chaque icône de barre d'outils est accompagnée d'un court libellé
texte à côté d'elle. Les panneaux ne sont pas surchargés : privilégier moins de
contrôles, clairement nommés.
```

---

## A. Calibrage

### 1. Panneau de calibrage du style (à générer en premier)

Génère d'abord ce panneau neutre pour valider les couleurs et surtout la
**distinction bleu / orange** avant de lancer les écrans complets.

```
Un unique panneau d'interface sombre et vide, sans contenu applicatif, servant
uniquement à démontrer les règles de couleur et d'arrondi du guide de style
ci-dessus. Fond presque noir #1A1A1A. Au centre, une carte #242424 avec une
bordure fine de 1 pixel en blanc à 8 % et des coins arrondis de 2 pixels,
contenant, empilés verticalement avec un espacement généreux :

- une rangée de deux petites puces arrondies côte à côte : la première remplie
  en bleu #1177D1 portant le libellé « État interface », la seconde remplie en
  orange #FF8C0D portant le libellé « Sélection 3D » ;
- en dessous, une seule ligne de propriété démontrant la mise en page à deux
  colonnes : à gauche une colonne de libellé au fond #1E1E1E contenant le texte
  « Position », à droite trois petits champs numériques côte à côte, chacun
  portant un bandeau vertical de couleur de 2 pixels sur son SEUL BORD GAUCHE
  (le premier en rouge atténué #C7404A, le deuxième en vert atténué #5A9E3C, le
  troisième en bleu atténué #3A6FB0), chaque champ affichant un nombre court
  comme « 0,00 », et une petite icône de flèche de réinitialisation à
  l'extrême droite de la ligne ;
- en dessous, une rangée de quatre petites puces de filtre arrondies portant les
  libellés « Maillage », « Animation », « Matériau », « Texture », chacune
  précédée d'un petit point plein dans sa couleur de type (cyan #22B8CF,
  orange #F08C00, vert #37B24D, magenta #E64980) ;
- en dessous, un contrôle segmenté fait de trois segments accolés portant les
  libellés « Statique », « Stationnaire », « Mobile », celui du milieu étant
  actif et rempli en bleu #1177D1.
```

---

## B. Écrans principaux

### 2. Écran A — mode objet *(écran de référence, à générer en second)*

C'est la maquette maîtresse : toutes les autres en dérivent. Les **adjacences**
sont imposées ; les proportions données sont indicatives à ±3 %.

```
Une fenêtre complète d'application de modélisation 3D pour ordinateur, paysage
16:9, thème sombre, organisée en bandes horizontales et en colonnes comme suit.

BANDE SUPÉRIEURE, 4 % de la hauteur de la fenêtre, fond #2A2A2A : tout à gauche
un petit logo hexagonal, puis une barre de menus horizontale avec les entrées
« Fichier  Édition  Fenêtre  Outils  Sélection  Objet  Aide » en gris clair
#E6E6E6. À l'extrême droite de cette bande, le nom du projet « MonProjet » en
blanc à 55 % d'opacité.

DEUXIÈME BANDE, 3 % de hauteur, fond #242424 : un seul onglet de document à
gauche portant le texte « Scène_01 » avec une petite croix de fermeture,
l'onglet paraissant visuellement rattaché à la bande située en dessous.

TROISIÈME BANDE (barre d'outils principale), 5 % de hauteur, fond #2A2A2A,
séparée de la bande du dessus par un trait fin de 1 pixel en blanc à 8 % : de
gauche à droite, des groupes de boutons « icône + libellé » séparés par de
courts traits verticaux — une icône de disquette avec le libellé
« Enregistrer », puis une icône de curseur avec le libellé déroulant « Mode de
sélection », puis une icône de plus avec le libellé « Ajouter », puis une icône
de calques empilés avec le libellé « Modificateur ». À l'extrême droite de cette
bande, une icône d'engrenage avec le libellé « Réglages ».

ZONE PRINCIPALE, occupant l'espace vertical entre la barre d'outils et les
bandes du bas, découpée en trois colonnes :

COLONNE DE GAUCHE, 16 % de la largeur de la fenêtre, fond #242424 — le panneau
de hiérarchie. En haut, un onglet de panneau portant le texte « Hiérarchie »
avec une petite croix de fermeture. En dessous, un champ de recherche avec une
icône de loupe et le texte indicatif « Rechercher ». En dessous encore, une
liste à deux colonnes avec la ligne d'en-tête « Nom » et « Type » (l'en-tête
Type et ses valeurs en blanc à 55 %, alignés à droite). La liste montre une
arborescence indentée avec de petits triangles de dépliage : une ligne racine
« Scène », puis en retrait « Cube » (type « Maillage »), « Sphère » (type
« Maillage »), puis une ligne avec une petite icône de dossier JAUNE portant le
texte « Groupe » (type « Dossier »), et en retrait sous ce dossier « Roue » et
« Axe ». Une SEULE ligne, « Cube », est surlignée par un fond bleu plein
#1177D1. Tout en bas de cette colonne, une fine bande de pied de liste avec le
petit texte « 12 objets (2 sélectionnés) » en blanc à 55 %.

COLONNE CENTRALE, 55 % de la largeur de la fenêtre — la vue 3D. Elle montre une
scène 3D simple sur un fond en dégradé gris sombre neutre avec un léger sol
quadrillé en perspective : un cube gris au centre, une sphère grise à sa droite,
tous deux à facettes et sans texture. Le CUBE porte un contour ORANGE #FF8C0D
net autour de sa silhouette, indiquant qu'il est sélectionné, et un petit gizmo
de déplacement en son centre fait de trois flèches : rouge vers la droite, verte
vers le haut, bleue vers le fond. Dans le COIN INFÉRIEUR GAUCHE de la vue, un
petit repère à trois axes avec les lettres X (rouge), Y (verte), Z (bleue).
EN SURIMPRESSION sur le HAUT de cette vue, flottant au-dessus de la scène 3D,
une barre d'outils de vue compacte en deux groupes : à GAUCHE un groupe arrondi
contenant une icône de menu à trois traits, puis « Perspective » avec un petit
chevron déroulant, puis « Éclairé » avec un chevron, puis « Affichage » avec un
chevron ; à DROITE un groupe de petits boutons carrés à icône — un curseur, une
croix de déplacement, un cercle de rotation, un coin de mise à l'échelle, un
globe — où le bouton CROIX DE DÉPLACEMENT est rempli en bleu plein #1177D1 pour
montrer qu'il est l'outil actif, suivi de trois petites puces d'aimantation
portant les textes « 0,5 », « 15° » et « 0,25 ».

COLONNE DE DROITE, 29 % de la largeur de la fenêtre, partagée verticalement en
deux panneaux empilés :

  PANNEAU SUPÉRIEUR DROIT, environ 45 % de la hauteur de la colonne —
  « Propriétés ». Onglet de panneau portant « Propriétés » avec une croix de
  fermeture. En dessous un champ de recherche, puis une seule rangée de cinq
  petites puces de filtre arrondies portant « Général », « Objet », « Rendu »,
  « Physique », « Tout », où « Tout » est remplie en bleu plein #1177D1 et les
  autres sont seulement contournées. En dessous, une section repliable avec un
  petit triangle vers le bas et le titre demi-gras « Transformation », contenant
  trois lignes de propriété en stricte mise en page à deux colonnes : colonne de
  libellé à gauche au fond #1E1E1E contenant « Position », « Rotation »,
  « Échelle », et à droite de chaque libellé, trois petits champs numériques
  côte à côte, chacun portant un bandeau vertical de couleur de 2 pixels sur son
  SEUL BORD GAUCHE (rouge, vert, bleu dans cet ordre) et une valeur numérique
  courte comme « 0,00 » — le fond des champs restant neutre. La ligne
  « Position » montre une petite icône de flèche de réinitialisation à son
  extrême droite ; la ligne « Échelle » montre un petit cadenas fermé à son
  extrême droite.

  PANNEAU INFÉRIEUR DROIT, environ 55 % de la hauteur de la colonne —
  « Détails ». Onglet de panneau portant « Détails (Cube) ». Il contient trois
  sections repliables empilées, chacune avec un petit triangle vers le bas et un
  titre demi-gras : « Maillage », puis « Modificateurs » qui porte un petit
  bouton plus aligné à DROITE de son titre, puis « Matériau ». La section
  « Maillage » montre deux lignes de propriété simples dans la même mise en page
  à deux colonnes, avec les libellés « Sommets » et « Faces » et les valeurs en
  lecture seule « 8 » et « 6 ». Les deux autres sections sont repliées sur leur
  seule ligne de titre.

PANNEAU DU BAS (navigateur de projet), une bande horizontale occupant TOUTE la
largeur de la fenêtre, environ 22 % de sa hauteur, fond #242424, séparée par un
trait fin en blanc à 8 %. En haut, un onglet de panneau portant « Navigateur de
projet » avec une croix de fermeture, puis une rangée de boutons « icône +
libellé » : une icône de plus avec « Ajouter », une icône de flèche descendante
avec « Importer », une icône de disquette avec « Tout enregistrer », puis un
séparateur vertical, puis de petites flèches de retour et d'avance, puis un fil
d'Ariane portant « Tout  ›  Contenu  ›  Perso ». Sous cette rangée, le panneau
est partagé en deux parties : à GAUCHE, 18 % de la largeur du panneau, une
petite arborescence de dossiers avec la racine « MonProjet » et les enfants
« Maillages », « Animations », « Matériaux », « Textures », la ligne
« Maillages » étant surlignée en bleu #1177D1 ; à DROITE, la zone des éléments,
qui commence par un champ de recherche et une rangée de quatre petites puces de
filtre portant « Maillage », « Animation », « Matériau », « Texture », chacune
précédée d'un petit point plein dans sa couleur de type, et en dessous une
rangée horizontale de cinq vignettes carrées. Chaque vignette montre un aperçu
de forme 3D grise simple sur un fond légèrement plus clair, le nom de l'élément
en dessous en petit texte (« Cube », « Tête », « Bois », « Marche », « Roche »),
et — point essentiel — une FINE BARRE HORIZONTALE COLORÉE tout au bas de la
vignette indiquant son type : cyan #22B8CF pour les vignettes de maillage, vert
#37B24D pour celle de matériau. À l'extrême droite du pied du panneau, le petit
texte « 5 éléments » en blanc à 55 %.

BARRE D'ÉTAT INFÉRIEURE, 3 % de hauteur, fond #2A2A2A, sur toute la largeur : à
gauche, une petite icône de tiroir avec le libellé « Tiroir », puis « Journal »,
puis un champ de saisie de console avec le texte indicatif estompé « Entrer une
commande ». À droite, le petit texte de statistiques « Sommets 8 · Arêtes 12 ·
Faces 6 · sél. 4 · actif : Cube · 60 ips » en blanc à 55 % d'opacité.
```

---

### 3. Écran B — mode édition

Même disposition que l'écran A. Ne change que **l'intérieur de la vue 3D** et
**deux détails** de l'interface. Génère-le en réutilisant l'écran A comme image
de référence.

```
LA MÊME disposition d'application de modélisation 3D que l'écran précédent, avec
les mêmes panneaux aux mêmes emplacements et les mêmes couleurs, mais cette fois
en mode ÉDITION de maillage. Différences par rapport à l'écran précédent, tout
le reste étant identique :

- Dans la VUE 3D centrale, le cube est maintenant affiché comme un maillage
  éditable : une fine cage en fil de fer sombre sur sa surface, avec de petits
  carrés aux coins. Trois de ses faces supérieures sont remplies d'un voile
  ORANGE #FF8C0D translucide et leurs arêtes de contour sont tracées en orange
  plein. Exactement UN carré de coin est en BLANC pur #FFFFFF (l'élément actif),
  tandis que les autres coins sélectionnés sont ORANGE et les non sélectionnés
  presque noirs.
- Dans la barre d'outils en surimpression de la vue, un groupe SUPPLÉMENTAIRE de
  trois petits boutons carrés apparaît à gauche des outils de transformation,
  montrant une icône de point (sommet), une icône de segment (arête) et une
  icône de carré plein (face), où l'icône de FACE est remplie en bleu plein
  #1177D1 pour indiquer le sous-mode actif.
- Dans le panneau de hiérarchie à gauche, la ligne « Cube » est maintenant en
  italique et précédée d'une petite icône de crayon, indiquant qu'elle est en
  cours d'édition.
- Dans la barre d'état du bas, le texte de statistiques devient « Sommets 8 ·
  Arêtes 12 · Faces 6 · sél. 3 faces · actif : face 4 · 60 ips ».
```

---

### 4. Écran C — panneau Modificateurs, empilé

Vue rapprochée du panneau de droite. C'est le panneau le plus important du
produit : il doit avoir l'air **générique**, parce qu'il l'est — son contenu est
produit automatiquement à partir d'une liste de paramètres, jamais dessiné pour
un modificateur en particulier.

```
Gros plan sur un unique panneau d'interface vertical issu d'une application de
modélisation 3D sombre, composition plutôt portrait 3:4, fond de panneau
#242424, occupant toute l'image.

En haut, un onglet de panneau portant « Détails (Cube) » avec une petite croix
de fermeture. En dessous, une ligne de titre de section repliée, avec un
triangle pointant vers la droite, portant « Maillage ».

Puis le contenu principal : une ligne d'en-tête de section avec un triangle vers
le bas et le titre demi-gras « Modificateurs », et aligné à DROITE de ce titre un
petit bouton arrondi montrant une icône de plus, le court libellé « Ajouter » et
un chevron déroulant.

Sous cet en-tête, DEUX cartes de modificateur empilées, chacune avec une bordure
fine de 1 pixel en blanc à 8 %, des coins de 2 pixels, et un fond légèrement plus
clair que le panneau :

PREMIÈRE CARTE — sa ligne d'en-tête contient, de gauche à droite : un petit
triangle vers le bas, un petit cercle plein bleu #1177D1 servant d'interrupteur
(signifiant « activé »), le titre demi-gras « Subdivision de surface », puis
repoussés vers la droite le petit texte gris « n° 1 », une icône de menu à trois
points verticaux, et une petite croix de fermeture. Sous cet en-tête, deux lignes
de propriété en stricte mise en page à deux colonnes — colonne de libellé au fond
#1E1E1E — portant « Niveaux » avec la valeur numérique « 2 », et « Simple
(linéaire) » avec une case à cocher décochée.

SECONDE CARTE — même structure, en-tête montrant un triangle vers le bas, un
cercle plein bleu, le titre « Chanfrein », le texte gris « n° 2 », une icône à
trois points et une croix. En dessous, deux lignes de propriété : « Largeur »
avec la valeur « 0,050 » suivie d'une fine glissière horizontale partiellement
remplie en bleu #1177D1, et « Segments » avec la valeur « 1 ».

Sous ces deux cartes, une TROISIÈME carte dans un état visuellement DÉSACTIVÉ :
son cercle d'interrupteur est creux et gris au lieu d'être bleu, son titre
« Miroir » et tout son texte sont estompés à 35 % d'opacité, mais la carte reste
entièrement visible et n'est PAS masquée.

Tout en bas du panneau, une ligne de titre de section repliée, avec un triangle
pointant vers la droite, portant « Matériau ».

L'impression d'ensemble doit être que ces cartes sont ENGENDRÉES à partir d'une
liste de paramètres nommés : structure de ligne identique partout, aucun contrôle
sur mesure propre à tel ou tel modificateur.
```

---

### 5. Écran D — état vide et menu d'ajout *(optionnel mais utile)*

Montre la règle « les états vides parlent » et la liste des modificateurs.

```
Gros plan sur le MÊME panneau sombre que l'écran précédent, mais dans un état
VIDE, avec un menu déroulant ouvert par-dessus.

Le panneau montre l'en-tête de section « Modificateurs » avec son bouton plus
libellé « Ajouter » à droite. En dessous, au lieu des cartes de modificateur, un
message d'état vide centré sur deux lignes : une petite icône estompée de calques
empilés, puis le texte gris en italique « Aucun modificateur » sur la première
ligne et, plus petit et plus estompé en dessous, « Cliquez sur Ajouter pour en
empiler un ».

Chevauchant le panneau, ancré sous le bouton « Ajouter », un menu déroulant au
fond #2A2A2A, avec une bordure fine en blanc à 8 % et des coins de 2 pixels,
contenant en haut un champ de recherche au texte indicatif « Rechercher un
modificateur », puis trois groupes d'entrées séparés par de fins traits, avec de
petits en-têtes de groupe en gris et en majuscules portant « GÉNÉRER »,
« DÉFORMER » et « NORMALES ». Sous « GÉNÉRER » : « Miroir », « Tableau »,
« Subdivision de surface », « Solidifier », « Chanfrein », « Souder »,
« Trianguler ». Sous « DÉFORMER » : « Projeter », « Déformation simple »,
« Lisser », « Onde ». Sous « NORMALES » : « Ombrage par angle ». Chaque entrée
porte une petite icône linéaire monochrome à sa gauche. L'entrée « Subdivision de
surface » est surlignée par un fond bleu plein #1177D1, comme survolée.
```

---

## Notes d'usage

**Ordre de génération.** Faire l'écran **1 (calibrage)** d'abord : il valide la
palette, et surtout que le bleu et l'orange sont bien distincts. Puis l'écran
**2 (mode objet)**, qui sert ensuite d'**image de référence** pour les écrans 3,
4 et 5 — cela évite que Banani ré-improvise une disposition à chaque essai.

**Si un écran dérive.** Le symptôme le plus probable est une disposition
réinventée. Dans ce cas, re-coller le guide de style **et** la phrase de
contrainte d'adjacence : *panneau de hiérarchie à GAUCHE, propriétés au-dessus des
détails à DROITE, navigateur de projet EN BAS sur toute la largeur*.

**Ce qu'il ne faut pas accepter dans un rendu.** Une maquette est à refaire si :
le bleu et l'orange sont intervertis ou mélangés · les champs X/Y/Z sont teintés
en fond au lieu d'être bordés à gauche · les barres d'outils sont en icônes seules
sans texte · le panneau de modificateurs a des contrôles dessinés sur mesure par
type · des fenêtres flottantes apparaissent alors qu'aucune n'est demandée · le
panneau vide montre du vide au lieu d'une phrase.

**Ce qui reste libre.** Typographie exacte, dessin des icônes, densité fine,
traitement des séparateurs, et la déclinaison **claire** du thème — l'application
aura plusieurs thèmes, dont un clair. Les libellés seront traduits : ne pas caler
les boutons au caractère près, certaines langues rallongent le texte de 30 %.

**Une maquette qui contredit ce document est une question à poser, pas une faute.**
Chaque règle ci-dessus répond à un problème constaté ; si l'une gêne visiblement,
c'est utile de le dire.
