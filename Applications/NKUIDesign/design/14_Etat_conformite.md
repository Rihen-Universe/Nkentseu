# État de conformité — ce que NkUIDesign fait des outils de référence, et ce qu'il ne fait pas encore

> **Demande de Rodolf, 2026-09-02** : *« tu ne m'as pas encore dit tout ce qui est
> implémenté venant de Lunacy/Figma/Sketch/Canva et tout ce qui ne l'est pas
> encore. »*
>
> **Ce document se lit en cinq minutes.** Le détail ligne par ligne — 174 lignes
> de comportement avec leur geste, leur état et ce qui manque — reste dans
> `13_Lunacy_reference_interaction.md`. Ici, on regroupe.

**Dernière régénération : 2026-09-02.** La partie chiffrée est produite par
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
| **174** | **72** | **25** | **76** | **1** |

### Par chapitre — où l'on est fort, où l'on est faible

| chapitre | livré | partiel | absent | écarté |
|---|---|---|---|---|
| 1. Édition vectorielle | 21 | 5 | 7 | 0 |
| 2. Outils de tracé et leurs modificateurs | 5 | 7 | 8 | 0 |
| 3. Opérations booléennes et de forme | 0 | 0 | 8 | 0 |
| 4. Sélection et navigation | 8 | 0 | 5 | 0 |
| 5. Toile et vue | 3 | 4 | 9 | 0 |
| 6. Transformations | 13 | 1 | 7 | 0 |
| 7. Calques et groupes | 9 | 1 | 6 | 1 |
| 8. Propriétés | 10 | 5 | 21 | 0 |
| 9. Composants et instances | 3 | 2 | 5 | 0 |

### La liste nommée — PARTIELS

**1. Édition vectorielle**

- Entrer en édition de forme
- Sortir
- Le curseur devient la plume, et la section `Edit shape` apparaît
- Sélectionner plusieurs points
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

**6. Transformations**

- Taille exacte, verrou de proportions

**7. Calques et groupes**

- Créer un cadre

**8. Propriétés**

- Appliquer à une multi-sélection ; « contenu mixte » signalé
- Saisie HEX / RGBA / HSB / HSL
- Position intérieure / centrée / extérieure
- Ombre interne
- Alignement horizontal (4)

**9. Composants et instances**

- Poser une instance
- États d'un composant (Défaut / Survol / Pressé)

### La liste nommée — ABSENTS

**1. Édition vectorielle**

- Supprimer un point à la souris
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
- Extrémités d'un tracé ouvert (*Caps*)

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

**6. Transformations**

- Redimensionner de 1 px / 10 px
- Redimensionner un cadre sans son contenu
- Outil Échelle (met aussi bordures et effets à l'échelle)
- S'aligner sur les bords du cadre quand un seul objet est sélectionné
- Distribuer H / V
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
- Dégradés linéaire / radial / angulaire, avec étapes de couleur déplaçables
- Remplissage image et ses quatre cadrages (remplir / ajuster / étirer / mosaïque)
- Pipette pour prélever une couleur
- Règle de remplissage non-zero / pair-impair
- Mode de fusion par remplissage
- *Tints* (une teinte unique sur un groupe ou une instance)
- Extrémités (*caps*), jonctions (*folds*), têtes de flèche
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
- Le composant se propage à ses instances sauf là où une surcharge existe
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
- 🚫 **Crop, Rasterize, export d'images** (PNG/JPG, @2x/@3x) — ils supposent des
  **pixels** ; un `.nkuidoc` décrit des **nœuds**. *L'export de NkUIDesign, c'est
  le `.nkuidoc` lui-même.*
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
