# Regarder le plateau, et l'habiller

Ce chapitre couvre ce qui n'est ni règle ni IA : **voir** ce qui se passe, et
**donner un visage** aux totems. C'est du ressort du designer et de l'artiste, et
rien de ce qui suit ne change un seul résultat de mesure.

## 7.1 Les grands plateaux : zoom et déplacement

Le cadrage automatique remplit le panneau avec le plateau entier. Parfait pour 42
cases, intenable à 900 : les cellules tombent à quelques pixels et on ne joue
plus, on regarde une mosaïque.

| Geste | Effet |
|---|---|
| **molette** | zoom, **ancré sous le curseur** |
| **bouton du milieu** | déplacement |
| **Recadrer** | retour au cadrage automatique (le bouton n'apparaît que si la vue a bougé) |

> **✅ Le zoom est un facteur, pas une taille de cellule**
>
> Il s'applique *par-dessus* le cadrage automatique. Conséquence : il survit au
> redimensionnement de la fenêtre et au changement de plateau. Stocker « cellule
> = 18 px » obligerait à recalculer ce nombre à chaque fois ; un rapport, non.
>
> À `zoom 1`, l'affichage est **exactement** celui d'avant. On n'a pas remplacé
> un comportement qui marchait : on lui a ajouté une sortie de secours.

L'ancrage sous le curseur n'est pas un détail de confort. On vise une case, on
tourne la molette, et cette case reste sous le pointeur. Un zoom centré sur le
panneau oblige à rattraper le plateau après chaque cran.

**`Applications/ConquerorLab/src/ConquerorLab/NkcBoardView.h — NkcCellVisible`**

```cpp
/// Faut-il dessiner cette cellule ? Rejette ce qui ne touche pas `area`.
///
/// Sans ce test, un plateau de 900 cases produit 900 polygones par image
/// dont l'immense majorite est hors ecran. Avec, le cout suit ce qu'on
/// VOIT, pas ce qui existe.
```

Sur un plateau de plus de 200 cases, la barre d'état change de contenu : elle
affiche `N/900 cases affichées, zoom xN`. Le culling doit être **visible**, pas
magique.

Plateau d'épreuve livré : `hexagone_30x30_grand.json`.

> **⚠️ La molette est consommée, et il le fallait**
>
> NkGui décide d'afficher une barre de défilement ainsi :
> `contentH = cursor.y - contentTop` puis `barV = (contentH - viewH) > 0`. Or
> `NextItemRect` avance le curseur de `h + itemSpacingY`.
>
> Réserver toute la hauteur visible donnait donc un contenu qui dépassait **de
> l'espacement** — d'un cheveu, mais assez pour faire surgir une barre. Elle
> volait la molette et faisait glisser l'entête.
>
> La barre d'actions et le bandeau de score doivent rester **fixes** : seule la
> zone de simulation bouge, et elle bouge par le zoom.

## 7.2 Trois et quatre joueurs

L'atelier laissait choisir jusqu'à quatre joueurs, alors qu'**aucun plateau livré
ne plaçait plus de deux totems**. Les joueurs 2 et 3 démarraient sans rien, donc
bloqués au premier tour.

Le sélecteur est désormais **borné par le plateau chargé** — il lit
`max_players`. Une option qui ne peut produire qu'une partie dégénérée n'est pas
une option, c'est un piège.

Trois plateaux multijoueurs sont livrés : `hexagone_7x7_3j`, `hexagone_8x8_4j`,
`carre_9x9_4j`. Pour en écrire d'autres, il faut **et** `max_players`, **et** un
`starts` par joueur.

> **⚠️ L'inversion des côtés ne vaut qu'à deux joueurs**
>
> Rappel du chapitre 6 : à 3 ou 4 joueurs, les winrates par camp mélangent effet
> de stratégie et effet de siège. Le multijoueur sert à observer des dynamiques —
> alliances de fait, cible désignée — pas à trancher un équilibrage.

## 7.3 Habiller les totems

Un totem est une **donnée**, comme un plateau. Un artiste doit pouvoir le changer
sans compiler, sans ouvrir l'atelier, sans demander à personne.

```
travail/totems/
    Guerrier/
        n0.png          niveau 0, image fixe
        n1.png          niveau 1
        n2.png  n3.png  n4.png
    Esprit/
        n0_000.png      niveau 0, ANIMATION : suffixe à trois chiffres
        n0_001.png
        n0_002.png
        n1.png          niveau 1, fixe — on peut mélanger
    Pierre/
        base.png        AUCUN niveau : cette image sert à tous
```

**Un dossier = un totem**, et le nom du dossier est celui qui apparaît dans le
panneau *Joueurs*. Trois règles de nommage, et rien d'autre :

| Nom | Sens |
|---|---|
| `nN.png` | image du niveau N (0 à 4) |
| `nN_KKK.png` | K-ième image de l'animation du niveau N |
| `base.png`, `base_KKK.png` | s'applique à **tous** les niveaux |

> **✅ Une image manquante ne fait jamais disparaître un totem**
>
> Niveau absent → `base`. `base` absent → **le disque coloré**. Le jeu reste
> jouable pendant qu'on dessine, et on peut comparer une silhouette en cours à la
> forme de référence.
>
> Ce n'est pas un pis-aller : c'est ce qui permet de travailler.

Le niveau reste lisible **même avec une image** : un anneau se superpose, comme
avant. Un artiste peut donc dessiner cinq niveaux identiques sans rendre le
plateau illisible.

### Format et taille

PNG **avec canal alpha** — sans alpha, votre totem est un carré posé sur la
cellule au lieu d'une silhouette.

La taille est celle qui vous arrange. L'atelier ajuste à la cellule **par
contenance** (l'image entre entièrement), proportions conservées :

**`Applications/ConquerorLab/src/ConquerorLab/NkcBoardPanel.h — DrawTotemImage`**

```cpp
// Ajustement PAR CONTENANCE : l'image entre entierement dans le carre de
// la cellule. Un ajustement par couverture rognerait, et un totem rogné
// n'est plus le totem qu'on a dessine.
const float32 box = rad * 2.f;
const float32 k   = (iw > ih) ? (box / iw) : (box / ih);
```

La taille d'une cellule dépend du plateau, de la fenêtre et du zoom : trois
choses qu'aucun fichier PNG ne peut connaître. C'est donc l'atelier qui adapte.

> **⚠️ Le rechargement n'est pas automatique**
>
> Panneau *Joueurs* → **Recharger les totems**. Décoder des PNG soixante fois par
> seconde figerait l'atelier — et vous savez quand vous venez de modifier un
> fichier.

> **✅ Pas d'atlas, délibérément**
>
> Un atlas demande un outil de packing, un format de métadonnées et une étape de
> build : exactement les trois choses qu'on veut épargner à quelqu'un dont le
> métier est de dessiner. Une texture par image coûte plus d'appels de dessin ;
> sur quelques dizaines de totems à l'écran, cela ne se mesure pas.

### Ce que le choix de totem ne change pas

**Rien de mesurable.** Deux campagnes menées avec des totems différents donnent
le même journal et la même empreinte. C'est pourquoi ce réglage **n'entre pas**
dans la signature de configuration (chapitre 6) : y mettre un choix décoratif
ferait croire à une différence là où il n'y en a aucune.

## 7.4 Le menu, le thème, la langue

Deux menus, et c'est tout.

| Menu | Contenu |
|---|---|
| **Fichier** | Thème (Sombre / Clair), Langue (Français / Anglais), Réinitialiser la disposition, Quitter |
| **Conqueror** | Nouvelle partie, Lecture/Pause, Un coup, Position vivante, Rechercher les modules |

Ont été retirés : *Affichage* (les panneaux se ferment par leur onglet), la
*palette de commandes* (sept panneaux ne justifient pas un lanceur), les réglages
de police, et *Fenêtre*. Un menu qui contient des entrées qui ne mènent nulle part
apprend à l'utilisateur à ne pas lire les menus.

> **✅ Le thème change le décor, pas le sens**
>
> Le chrome — fonds, boutons, bordures, texte — suit le thème. **Les couleurs de
> jeu ne bougent pas** : camps, coups légaux, menace, dernier coup.
>
> Les faire varier reviendrait à changer la lecture tactique en changeant
> l'humeur, et une capture d'écran de partie ne voudrait plus dire la même chose
> selon le réglage de qui l'a prise.

Pour la langue, la clé de traduction **est le texte français** : `T("Nouvelle
partie")`, pas un identifiant abstrait. Le code reste lisible, et une chaîne non
encore traduite s'affiche en français plutôt qu'en marqueur d'erreur.

Ne sont **pas** traduits, volontairement : les noms de modules et de fichiers
(ils viennent de vous), la sortie du compilateur (elle vient de clang), et les
traces de vos modules. Traduire ce qu'on n'a pas écrit, c'est mentir sur la
source.

## Exercices

> **✏️ 1 — Le totem qui manque**
>
> Créez `travail/totems/Test/` avec une seule image `n2.png`. Rechargez,
> affectez-la à un joueur, lancez une partie. Que voit-on aux niveaux 0, 1, 3 ?
> Pourquoi ce comportement est-il préférable à un totem invisible ?

> **✏️ 2 — Lire à petite échelle**
>
> Chargez `hexagone_30x30_grand.json`. Sans zoomer, pouvez-vous dire qui mène ?
> Zoomez jusqu'à ce que ce soit possible : à quel facteur ? Qu'est-ce qui devient
> illisible en premier — le camp, le niveau, ou le dernier coup ? Ce que vous
> venez de trouver est une contrainte de **lisibilité**, et elle vaut aussi pour
> le jeu final.

> **✏️ 3 — Le totem et la mesure**
>
> Jouez deux parties avec la même graine et deux totems différents. Comparez les
> empreintes du journal. Expliquez pourquoi elles sont identiques, et pourquoi ce
> serait un **bug grave** si elles ne l'étaient pas.

> **✏️ 4 — Trois joueurs**
>
> Lancez 300 parties sur `hexagone_7x7_3j`, même IA aux trois sièges. Un siège
> gagne-t-il plus souvent ? Pouvez-vous conclure quelque chose sur les règles à
> partir de ce chiffre ? Justifiez — la bonne réponse tient en une phrase.
