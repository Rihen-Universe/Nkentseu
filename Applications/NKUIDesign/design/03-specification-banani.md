# NkUIDesign — Prompts de génération d'écrans
### Document 3/3 — destiné à Banani (maquettes)

> Chaque section ci-dessous est un **prompt autonome**, à copier-coller tel quel,
> un écran à la fois. Générer d'abord le **Système de design** (§ 0), puis les
> écrans dans l'ordre. Si le contexte n'est pas conservé entre deux générations,
> **recoller le bloc « Système de design » en tête de chaque prompt**.
>
> ⚠️ **Ce que ces maquettes sont, et ne sont pas.** Elles décrivent **la cible
> visuelle**. À ce jour, **aucune fenêtre de NkUIDesign n'a jamais été ouverte et
> aucun pixel n'a été produit** : une maquette n'est donc jamais la preuve que
> l'écran existe. Chaque section porte l'état de ce qu'elle montre — ✅ le code
> existe (non vu), 🟡 partiel, 📝 à faire — et cet état ne doit pas disparaître
> quand on recopie le prompt.
>
> **Les couleurs des maquettes doivent correspondre aux jetons du thème du
> moteur** (§ 0), pour que la maquette et le rendu réel se comparent. Dans le
> code, **aucune couleur n'est écrite en dur** : tout passe par un rôle nommé.

---

## Table des écrans

| # | écran | état du code |
|---|---|---|
| 1 | Éditeur principal — vue Composition (thème Sombre) | ✅ existe, non vu |
| 2 | Éditeur principal — thème Clair | 📝 (les deux thèmes existent, la bascule non) |
| 3 | Panneau Propriétés — gros plan | ✅ existe, non vu |
| 4 | Panneau Composition + Palette — gros plan | ✅ existe, non vu |
| 5 | Panneau Aperçu — agencement et poignées | 🟡 |
| 6 | Panneau IA — proposition avant validation | ✅ la place existe |
| 7 | Palette de commandes (superposition) | 🟡 coquille |
| 8 | Panneau Comportement — Événements | 📝 |
| 9 | Panneau Comportement — Graphe de blueprint | 📝 |
| 10 | Éditeur d'icônes | 📝 |
| 11 | Paramètres — Apparence / éditeur de thème | 📝 |
| 12 | Paramètres — Langue (à chaud) | 📝 |
| 13 | Paramètres — Rendu / backend graphique (+ modale de redémarrage) | 🟡 |
| 14 | Dialogue Export + rapport de validation | 📝 |
| 15 | Aperçu interactif + console de callbacks | 📝 |
| 16 | Écran d'accueil | 📝 |
| 16bis | Aperçu — simulation d'appareil et **zone sûre** (avec/sans encoche, portrait/paysage) | 📝 |
| 17 | États et composants isolés (les cas qui se voient) | mixte |

---

## 0. Système de design (à coller en préfixe de chaque prompt)

```
Produit : NkUIDesign — outil de design d'interfaces, de bureau, professionnel.
On y compose une interface à partir de composants, on règle leurs paramètres,
on déclare leurs événements, et on la teste. Public : développeurs et concepteurs
d'outils techniques (éditeurs, tableaux de bord, panneaux d'inspection).

Style général : outil professionnel dense, façon éditeur de moteur de jeu ou IDE
moderne. Texte petit (11-13 px), pas d'espace perdu, hiérarchie visuelle nette.
Ce n'est PAS une application grand public : pas de dégradé, pas d'ombre marquée,
pas d'illustration décorative, jamais d'emoji.

Angles DROITS sur les panneaux ancrés (comme un éditeur professionnel).
Arrondis légers (4-6 px) sur les boutons, les champs et les cartes.
Icônes en trait fin (1,5 px), monochromes, jamais en couleur ni en 3D.
Typographie sans-serif neutre (type Inter / Segoe UI). Titres de section en
petites majuscules discrètes, légèrement espacées.

Deux thèmes à produire. Les valeurs ci-dessous sont celles du thème réel du
moteur — les respecter exactement.

THÈME SOMBRE :
- fond de fenêtre #010409, fond de panneau #0D1117, en-tête de panneau #161B22
- bordure #30363D, fond de champ #0D1117, colonne de libellés #161B22
- texte #FFFFFF, texte secondaire #8B949E
- accent d'interface #1F6FEB (bleu), accent de sélection #F2980E (orange)
- graphe : en-tête de nœud de données #0A545E (sarcelle), en-tête de nœud
  d'action #F2980E (ambre), corps de nœud #161B22, fil #8B949E
- dossier #E3B341, types : maillage #39C5CF, animation #DB6D28, matériau #3FB950,
  texture #DB61A2

THÈME CLAIR :
- fond de fenêtre #F5F5F5, fond de panneau #FFFFFF, en-tête de panneau #EAEAEA
- bordure gris très clair, fond de champ #FFFFFF, colonne de libellés #F0F0F0
- texte #1A1A1A, texte secondaire gris moyen
- accent d'interface #0E5FA6 (bleu assombri), accent de sélection #C97A08
  (ambre assombri — assombri exprès pour rester lisible sur fond clair)
Le thème clair ne se contente pas d'inverser les gris : il les remplace, et les
couleurs porteuses de sens y sont assombries.

Règle absolue à faire sentir dans la maquette : chaque couleur correspond à un
RÔLE nommé (fond de panneau, accent, sélection…), jamais à une décision locale.
Deux éléments de même rôle ont exactement la même couleur.

Libellés en FRANÇAIS.
```

> **Variante à connaître, non retenue par défaut** : un document antérieur du
> projet propose une palette gris-bleu propre à l'outil (fond `#1A1D24`, panneau
> `#262A34`, accent `#60A5FA`). Les deux sources divergent ; **l'arbitrage
> revient à Rodolf**. Ces prompts utilisent les jetons du moteur, parce que ce
> sont eux que le code applique aujourd'hui.

---

## 1. Éditeur principal — vue Composition (thème Sombre) — ✅ le code existe, jamais vu

```
[Coller le Système de design]

Génère la fenêtre principale de NkUIDesign, thème SOMBRE, 1440x900.

Structure verticale, de haut en bas :
- Barre de titre fine : à gauche un petit logo carré et le titre
  « NkUIDesign — mon_ecran.nkuidoc • » (le point signale une modification non
  enregistrée) ; à droite les boutons réduire / agrandir / fermer.
- Barre de menus : Fichier, Édition, Composant, Affichage, Comportement, Aide.
- Barre d'outils fine : boutons icône Nouveau, Ouvrir, Enregistrer, séparateur,
  Annuler, Rétablir, séparateur, Dupliquer, Supprimer, séparateur, un bouton
  « Aperçu interactif » avec une icône de lecture.
- Zone de travail en trois colonnes ancrées, séparées par des poignées fines.
- Barre d'état en pied de fenêtre (voir plus bas).

COLONNE GAUCHE (280 px), deux panneaux empilés :
1. « PALETTE » — liste des composants disponibles, chacun sur une ligne :
   petite icône carrée, nom du composant en gras, et sous-titre gris donnant son
   rôle. Entrées : « content_browser · liste », « tree_view · arbre »,
   « Cadre · conteneur ». Un champ de recherche en haut.
2. « COMPOSITION » — un arbre. Racine « Écran » (icône de cadre), enfants
   indentés : « barre_outils » (cadre), « navigateur » (content_browser),
   « arbre_projet » (tree_view), « pied » (cadre). La ligne « navigateur » est
   SÉLECTIONNÉE : fond bleu accent, texte clair, et une barre verticale de 2 px
   à gauche de la ligne. Chevrons de repli, et à droite de chaque ligne une
   petite pastille indiquant sa règle de taille : « fixe », « poids 1 »,
   « extensible ».

COLONNE CENTRALE (le reste, la plus large) : panneau « APERÇU ».
   En haut du panneau, une barre d'outils interne : un sélecteur de largeur de
   simulation (« 1280 x 720 ▾ »), un sélecteur de variante (« grid ▾ »), un
   sélecteur d'échelle (« 100 % ▾ »), et à droite deux bascules « Repères » et
   « Poignées ».
   Le contenu : l'interface en cours de composition, dessinée pour de vrai —
   une barre d'outils factice en haut, au centre un navigateur de contenu en
   grille de cartes (miniatures grises, nom de fichier dessous), à gauche un
   arbre de dossiers. Le nœud sélectionné (le navigateur) porte un CONTOUR
   ORANGE de 2 px avec quatre petites poignées carrées sur ses bords.
   Les autres nœuds portent un contour bleu très fin au survol.

COLONNE DROITE (320 px) : panneau « PROPRIÉTÉS ». Voir l'écran 3 pour le détail ;
   ici, le montrer rempli, avec ses sections repliables.

PANNEAU DU BAS (200 px, sur toute la largeur, sous les trois colonnes) : deux
   onglets « IA » (actif) et « Comportement ». Contenu de l'onglet IA : une
   grande zone de saisie avec le texte d'invite gris « Décrivez l'interface à
   composer… », un bouton primaire « Proposer » à droite, et sous le champ une
   ligne d'état grise « Backend : fichier · aucun modèle spécialisé ».

BARRE D'ÉTAT (pied de fenêtre, hauteur 24 px, fond d'en-tête de panneau) :
   de gauche à droite, séparés par des points médians —
   « Vulkan » (avec une petite pastille verte), « navigateur · content_browser »,
   « 5 nœuds », « Français », et à droite « Enregistré il y a 2 min ».
```

**Ce que cet écran doit rendre évident** : trois zones (ce que je peux poser · ce
que je compose · ce que je règle), la sélection visible **au même endroit dans
trois panneaux à la fois** (arbre, aperçu, propriétés), et une barre d'état qui
répond sans clic à « sur quel backend, quel nœud, quelle langue ».

---

## 2. Éditeur principal — thème Clair — 📝 la bascule n'existe pas

```
[Coller le Système de design]

Génère exactement le même écran que l'écran 1, en thème CLAIR.

Contraintes :
- La DISPOSITION est rigoureusement identique : mêmes panneaux, mêmes largeurs,
  mêmes contenus, même nœud sélectionné. Seules les couleurs changent.
- Le contour de sélection devient un ambre assombri (#C97A08) — il ne doit pas
  se perdre dans le fond clair : c'est le point à vérifier sur cette maquette.
- L'accent bleu devient #0E5FA6.
- Le texte secondaire reste lisible : gris moyen, pas gris pâle.
- Aucun élément ne doit disparaître par rapport à l'écran 1 : si quelque chose
  devient invisible en clair, c'est un défaut à montrer, pas à corriger en
  changeant sa couleur localement.

Placer les deux écrans côte à côte dans la même planche si possible, pour que la
comparaison soit immédiate.
```

---

## 3. Panneau Propriétés — gros plan — ✅ le code existe

```
[Coller le Système de design]

Génère un gros plan du panneau « PROPRIÉTÉS » de NkUIDesign, thème SOMBRE,
360 x 900, tel qu'il apparaît avec un nœud « navigateur » sélectionné.

En-tête du panneau : icône du composant, « navigateur » en gras, et dessous en
petit gris « content_browser · rôle : list ». À droite, un bouton « … ».
Sous l'en-tête, un champ de recherche « Chercher une propriété… ».

Sections repliables, chacune avec un titre en petites majuscules et un chevron :

1. IDENTITÉ — deux lignes de propriété : « Nom » (champ texte, valeur
   « navigateur »), « Composant » (liste déroulante, valeur « content_browser »).

2. TAILLE ET AGENCEMENT — c'est la section la plus importante, montrer qu'elle
   ne contient AUCUNE coordonnée :
   - « Largeur » : un groupe de boutons segmentés [ fixe | contenu | fraction |
     poids | extensible ], « poids » actif en bleu accent ; à droite un champ
     numérique « 1,0 ».
   - « Hauteur » : même groupe segmenté, « extensible » actif.
   - « Min » et « Max » : deux petits champs numériques sur une ligne.
   - « Agencement des enfants » : boutons segmentés [ aucun | ligne | colonne |
     grille | ancrage ], « grille » actif.
   - « Alignement » : boutons segmentés [ début | centre | fin | étirer ].
   - « Ancre » : deux boutons segmentés [ zone sûre | bord de l'écran ], « zone
     sûre » actif, avec une aide contextuelle en petit gris : « le texte et les
     boutons dans la zone sûre ; les fonds et les images jusqu'au bord ».
   - Sous la section, une note grise en italique :
     « La position est calculée. Elle n'est jamais enregistrée.
       La zone sûre est demandée à la plateforme, jamais fixée ici. »

3. PARAMÈTRES — des lignes de propriété engendrées depuis la déclaration du
   composant : « Colonnes » (curseur avec valeur 4, bornes visibles 1 et 12),
   « Taille de carte » (curseur, 96), « Écart » (curseur, 12), « Afficher les
   extensions » (case à cocher, cochée), « Largeur de l'arbre » (curseur, 0,18).
   Chaque curseur montre ses bornes en très petit à ses extrémités.

4. VARIANTE — trois pastilles à choisir : « grid » (active), « dense_list »,
   « columns ». Sous « columns », une petite mention grise « rendue comme
   dense_list » — un manque assumé, affiché plutôt que caché.

5. JETONS DE THÈME — la liste des rôles de couleur que ce composant consomme,
   chacun avec une pastille de couleur et son nom : « fond de panneau »,
   « bordure », « texte », « texte secondaire », « sélection ». Non modifiables
   ici (une note grise : « s'éditent dans le thème »).

6. ÉVÉNEMENTS — une liste en police à chasse fixe, petite :
     onSelect(index: Int, path: String)
     onActivate(index: Int, path: String)
     onContextMenu(at: Vec2)
     onDrop(folderIndex: Int, payloadType: String)
     onNavigate(path: String)
   Chaque ligne a à droite une pastille grise « non branché ».

7. PROVENANCE — trois lignes en petit : « Auteur : humain », « Vérifié : non »,
   « Corrigé : non ».

Style des lignes de propriété : libellé à gauche sur une colonne d'une couleur
légèrement différente du fond, contrôle à droite, hauteur de ligne compacte
(26-28 px), séparateurs très discrets.
```

**Le point à faire sentir** : ce panneau **n'a pas été écrit à la main**. Il
boucle sur les tables de la déclaration — ajouter un paramètre au composant le
fait apparaître ici **sans toucher à l'éditeur**, et les bornes des curseurs
viennent de la déclaration, pas de la maquette.

---

## 4. Palette + Composition — gros plan — ✅ le code existe

```
[Coller le Système de design]

Génère un gros plan de la colonne gauche de NkUIDesign, thème SOMBRE,
320 x 900, montrant les deux panneaux empilés et une action de glisser-déposer
en cours.

PANNEAU HAUT « PALETTE » (40 % de la hauteur) :
- champ de recherche
- liste de composants, une ligne chacun : icône carrée en trait fin, nom en gras,
  sous-titre gris avec le rôle. Entrées : « content_browser · list »,
  « tree_view · tree », « Cadre · container ».
- une ligne est en cours de glissement : elle apparaît en semi-transparence
  sous le curseur, avec une petite vignette du composant.

PANNEAU BAS « COMPOSITION » (60 %) :
- l'arbre du document, avec une LIGNE D'INSERTION horizontale bleu accent entre
  deux enfants, montrant où le composant glissé va tomber, et le parent d'accueil
  légèrement surligné.
- les lignes : chevron, icône, nom, et à droite une petite pastille de règle de
  taille (« fixe », « poids 1 », « extensible »).
- un nœud « pied » est un CADRE : son icône est un rectangle en pointillés et son
  nom est en italique gris — il arrange, il ne dessine rien.
- en bas du panneau, une ligne d'état : « 5 nœuds · 1 sélectionné ».
```

---

## 5. Panneau Aperçu — agencement et poignées — 🟡

```
[Coller le Système de design]

Génère un gros plan du panneau « APERÇU » de NkUIDesign, thème SOMBRE,
1000 x 700, pendant un redimensionnement à la souris.

- Barre d'outils interne en haut : largeur de simulation, variante, échelle,
  bascules « Repères » et « Poignées ».
- Au centre, l'interface composée. Le nœud sélectionné est entouré d'un contour
  orange de 2 px avec 8 poignées carrées (coins + milieux d'arêtes). L'une des
  poignées de droite est saisie : le curseur est une double flèche horizontale.
- PENDANT le glissement, une petite étiquette flottante suit le curseur et
  affiche CE QUI EST ÉCRIT, pas une position : « poids : 1,0 → 1,6 ».
  C'est l'élément le plus important de cette maquette.
- Quand « Repères » est actif : des guides fins bleus montrent l'agencement du
  parent — pour une grille, les lignes de la grille ; pour une colonne, les
  séparations horizontales.
- En bas à gauche du panneau, une petite mention grise :
  « Agencement : grille · 4 colonnes · résolu pour 1280 x 720 ».
```

---

## 6. Panneau IA — proposition avant validation — ✅ la place existe

```
[Coller le Système de design]

Génère le panneau « IA » de NkUIDesign en pleine hauteur (fenêtre 1440 x 900,
panneau bas étendu à 420 px), thème SOMBRE, avec une proposition affichée.

Colonne gauche du panneau (40 %) — la conversation :
- une zone de saisie multiligne, avec un texte déjà écrit : « Un panneau de
  gestion de fichiers avec une barre d'outils en haut, un arbre à gauche et une
  grille de fichiers à droite. »
- un bouton primaire « Proposer » et un bouton secondaire « Continuer le design
  existant ».
- une ligne d'état : « Backend : fichier ▾ · le modèle spécialisé n'existe pas
  encore ».

Colonne droite (60 %) — la proposition :
- un aperçu miniature de l'interface proposée, dessiné.
- au-dessus, un bandeau d'état vert pâle avec une coche :
  « Rejeu vérifié : écrit → relu → même mise en page (0 écart) ».
- sous l'aperçu, deux boutons : « Poser dans le document » (primaire) et
  « Écarter » (secondaire), plus une case « Remplacer la sélection ».
- une ligne de provenance en petit gris : « Auteur : IA · vérifié : rejeu ·
  corrigé : non ».

Générer AUSSI une seconde variante du même panneau, avec le bandeau en ROUGE
PÂLE et une croix : « Rejeu ÉCHOUÉ : 3 lignes non relues — proposition écartée ».
Dans cette variante, le bouton « Poser dans le document » est DÉSACTIVÉ.
```

**Ce que la maquette doit dire** : la proposition **passe par la même porte que
la main** (elle se pose comme un collage), et une proposition qui ne se rejoue
pas **ne se pose pas**. Le moteur est le juge, pas l'œil.

---

## 7. Palette de commandes — 🟡 la coquille la porte

```
[Coller le Système de design]

Génère la palette de commandes de NkUIDesign : une superposition modale
centrée en haut de la fenêtre, largeur 640 px, sur un fond d'éditeur assombri.

- Un grand champ de recherche en haut avec le texte tapé « doc » et un curseur.
- En dessous, une liste de résultats filtrés, la première ligne surlignée en
  accent bleu :
    Document: Enregistrer                                    Ctrl+S
    Document: Recharger                                      Ctrl+R
    Document: Nouveau                                        Ctrl+N
    Document: Exporter…                                      —
- Le raccourci est aligné à droite, dans une petite étiquette encadrée, en gris.
- Une ligne sous la liste, très discrète : « 4 sur 27 commandes ».

Le nom de commande suit toujours la forme « Domaine: Verbe complément ».
```

**Pourquoi le raccourci est affiché à droite de chaque ligne** : c'est ainsi
qu'on apprend les raccourcis sans les apprendre.

---

## 8. Panneau Comportement — Événements — 📝

```
[Coller le Système de design]

Génère le panneau « COMPORTEMENT » de NkUIDesign, onglet « Événements », en bas
de fenêtre, 1440 x 320, thème SOMBRE.

Deux colonnes :

GAUCHE (35 %) — la liste des événements du nœud sélectionné, en tableau :
| Événement       | Charge                              | Branché    |
| onSelect        | index: Int, path: String            | ● Graphe   |
| onActivate      | index: Int, path: String            | ○ —        |
| onContextMenu   | at: Vec2                            | ○ —        |
| onDrop          | folderIndex: Int, payloadType: String | ○ —      |
| onNavigate      | path: String                        | ● Graphe   |
Les non branchés ne sont PAS en rouge : c'est une information, pas une erreur.
Pastille pleine = branché, pastille creuse = non branché.

DROITE (65 %) — le bloc de contrat, en police à chasse fixe, sur un fond de
champ, avec un bouton « Copier » en haut à droite :

    controller "content_browser" {
        callback onSelect(index: Int, path: String) -> Void
        callback onActivate(index: Int, path: String) -> Void
        callback onContextMenu(at: Vec2) -> Void
        callback onDrop(folderIndex: Int, payloadType: String) -> Void
        callback onNavigate(path: String) -> Void
    }

Sous le bloc, une note grise : « Ce bloc est produit à partir de la déclaration.
Il n'est pas écrit à la main. »
```

---

## 9. Panneau Comportement — Graphe de blueprint — 📝

```
[Coller le Système de design]

Génère le panneau « COMPORTEMENT », onglet « Graphe », en PLEIN ÉCRAN
(1440 x 900), thème SOMBRE. C'est un éditeur de graphe de nœuds.

Fond : très sombre, avec une grille de points discrète.

Colonne gauche (240 px) : « ÉVÉNEMENTS » (la liste des événements du document,
groupés par nœud), puis « VARIABLES » (pastilles colorées par type : bleu
booléen, vert flottant, orange vecteur, rose chaîne), puis « FONCTIONS ».
Chaque section a un « + » à droite de son titre.

Zone centrale : un graphe de 4 nœuds reliés par des fils en courbe.
- Nœud d'ÉVÉNEMENT à gauche : en-tête AMBRE (#F2980E), titre
  « onSelect (navigateur) », deux sorties de données « index » (pastille verte)
  et « path » (pastille rose), plus une sortie d'exécution (triangle blanc).
- Nœud « Branche » : en-tête sarcelle (#0A545E), une entrée d'exécution, une
  entrée « Condition » (pastille rouge), deux sorties « Vrai » / « Faux ».
- Nœud « Définir le libellé » : en-tête sarcelle, entrées « Cible » et « Texte ».
- Nœud « Journaliser » : en-tête sarcelle, entrée « Message ».
Un nœud est SÉLECTIONNÉ : contour bleu accent de 2 px.
Les fils d'exécution sont blancs et épais ; les fils de données prennent la
couleur de leur type.

Colonne droite (300 px) : « DÉTAILS » du nœud sélectionné — nom, type,
description, valeurs par défaut, sous forme de lignes de propriété.

En bas à droite, une mini-carte du graphe dans un cadre.
En bas à gauche, une ligne d'état : « 4 nœuds · 1 événement branché · aucune
erreur ».
```

⚠️ **État réel à ne pas laisser tomber en recopiant ce prompt** : les événements
sont déclarés avec leur charge, mais **rien ne s'y branche encore** — le module
de graphe est hors du build et aucun éditeur visuel n'existe. Cette maquette est
une **cible**, pas une capture.

---

## 10. Éditeur d'icônes — 📝

```
[Coller le Système de design]

Génère l'écran « ICÔNES » de NkUIDesign, thème SOMBRE, 1440 x 900. C'est un
éditeur d'icônes VECTORIELLES (des chemins, pas des images).

Colonne gauche (260 px) : « JEU D'ICÔNES » — un champ de recherche et une grille
de vignettes d'icônes en trait fin (dossier, fichier, engrenage, loupe, flèche,
œil, cadenas, plus, croix, chevron…), 4 par ligne, la vignette sélectionnée
entourée d'un contour bleu. En bas, « 84 icônes · jeu : nkuid.base ».

Zone centrale : la GRILLE DE TRAVAIL — un grand carré avec une grille fine, des
repères de marge en pointillés, et l'icône en cours d'édition dessinée en gros,
ses points d'ancrage visibles (petits carrés) et ses poignées de courbe (petites
lignes avec un rond au bout). Barre d'outils verticale à gauche de la grille :
sélection, plume, ligne, rectangle, cercle, union, soustraction, intersection,
épaisseur de trait.

Colonne droite (320 px), trois sections :
1. APERÇUS RÉELS — l'icône rendue à 16, 20, 24 et 32 px, côte à côte, sur fond
   sombre ET sur fond clair (deux rangées). C'est le contrôle qui compte : une
   icône lisible en grand peut être illisible en petit.
2. PROPRIÉTÉS — nom de l'icône, poignée (« 0x0042 », en chasse fixe, non
   modifiable), rôle de couleur (liste déroulante : « texte », « texte
   secondaire », « accent », « sélection »).
3. EXPORT — une case « Atlas PNG + table de poignées », un champ de taille de
   rastérisation, et un bouton « Exporter le jeu ».

En bas de fenêtre, une bande d'information grise :
« Vectorielle · se recolore par jeton de thème · suit le facteur DPI ·
la rastérisation en atlas est une étape de sortie, pas la source. »
```

---

## 11. Paramètres — Apparence et éditeur de thème — 📝

```
[Coller le Système de design]

Génère la fenêtre « PARAMÈTRES » de NkUIDesign, 1100 x 760, thème SOMBRE,
section « Apparence » active.

Colonne gauche (220 px) : la navigation des sections — Apparence (active),
Langue, Rendu, Édition. Style liste verticale, section active surlignée.

Zone principale, trois parties :

1. THÈME — trois grandes cartes de prévisualisation côte à côte : « Sombre »
   (sélectionnée, contour accent), « Clair », « Personnalisé ». Chaque carte
   montre une miniature d'interface dans ses couleurs.

2. ÉDITEUR DE THÈME — un tableau à deux colonnes, scrollable, groupé par
   familles avec des titres en petites majuscules (STRUCTURE, TEXTE, ACCENTS,
   ÉTATS, TYPES, GRAPHE, VUE 3D). Chaque ligne : pastille de couleur cliquable,
   nom du rôle en clair (« fond de panneau »), clé technique en gris et en
   chasse fixe (« PanelBg »), et la valeur hexadécimale dans un petit champ.
   Une section supplémentaire en bas, séparée visuellement et intitulée
   « RÔLES DE L'APPLICATION », contenant des clés préfixées :
   « nkuid.selection_noeud », « nkuid.cadre », « nkuid.cle_manquante ».

3. Un bandeau de CONTRASTE en bas, permanent :
   « Pire paire : texte secondaire sur fond de panneau — 4,8 ✔ » avec une
   pastille verte. Générer AUSSI une variante de ce bandeau en ambre :
   « Pire paire : sélection sur fond de panneau — 2,4 ✘ sous le seuil de 3,0 ».

Boutons en pied de fenêtre : « Hériter d'un thème… », « Enregistrer sous… »,
« Recharger », et à droite « Fermer ». À côté de « Recharger », une petite
mention grise : « dernier chargement : 37 clés appliquées, 2 inconnues ».
```

**Deux points à ne pas perdre** : on **édite un rôle, jamais un élément** ; et le
compte de clés inconnues est **affiché**, parce qu'un chargement « réussi » qui
n'applique rien est le pire des deux mondes.

---

## 12. Paramètres — Langue (à chaud) — 📝

```
[Coller le Système de design]

Génère la fenêtre « PARAMÈTRES » de NkUIDesign, section « Langue » active,
1100 x 760, thème SOMBRE.

Zone principale :

1. LANGUE DE L'INTERFACE — une liste de langues, chacune sur une ligne :
   drapeau ou code, nom de la langue, et à droite une barre de progression de
   traduction avec un pourcentage.
     Français      100 %   (sélectionnée, contour accent)
     English        96 %
     Deutsch        41 %
   Sous la liste, un bandeau VERT PÂLE avec une icône d'éclair :
   « Le changement de langue est immédiat. Rien ne se ferme. »
   ⚠️ Ce bandeau doit contraster visuellement avec celui de la section Rendu
   (écran 13), qui annonce un redémarrage. La différence entre les deux est le
   message principal de ces deux écrans.

2. CLÉS MANQUANTES — un encadré : « 14 clés manquantes en Deutsch » avec un
   bouton « Voir la liste ». Sous l'encadré, un exemple de rendu d'une clé
   absente : le texte « ⟦panneau.apercu.titre⟧ » dans un cadre pointillé
   d'avertissement, avec la légende « Voici comment une clé absente s'affiche.
   Jamais un vide. »

3. POLICE ET COUVERTURE — le nom de la police, et une ligne d'état :
   « La police couvre la langue sélectionnée ✔ ». Générer aussi la variante
   d'avertissement : « La police ne couvre pas cette écriture — un atlas adapté
   est nécessaire. »

4. LANGUES D'ESSAI (section développeur, visuellement mise en retrait) :
   deux cases à cocher — « Pseudo-langue LONGUE (+35 %) » et
   « Pseudo-langue de MÊME LONGUEUR ». Légende grise :
   « Sert à vérifier que le changement de langue recalcule la mise en page,
   et pas seulement le texte. »

GÉNÉRER ENFIN, sur la même planche, une COMPARAISON EN DEUX VIGNETTES du même
panneau d'éditeur : à gauche en français, à droite en pseudo-langue longue.
À droite, les libellés sont visiblement plus longs ET les colonnes, les boutons
et les champs se sont ÉLARGIS en conséquence — rien n'est tronqué, rien ne
déborde. C'est le comportement correct, et c'est ce que la maquette doit
montrer.
```

**Le point que cette planche existe pour rendre visible** : changer de langue
**recalcule la mise en page**, pas seulement la chaîne. Une maquette où seul le
texte change, à largeurs identiques, illustrerait exactement le défaut à éviter.

---

## 13. Paramètres — Rendu / backend graphique — 🟡

```
[Coller le Système de design]

Génère la fenêtre « PARAMÈTRES » de NkUIDesign, section « Rendu » active,
1100 x 760, thème SOMBRE.

1. BACKEND GRAPHIQUE — une liste de choix, boutons radio :
     Détection automatique   (sélectionnée)   « recommandé »
     OpenGL          ✔ disponible
     Vulkan          ✔ disponible          ← actuellement utilisé, pastille verte
     DirectX 11      ✔ disponible
     DirectX 12      ✔ disponible
     Metal           ✘ indisponible sur cette plateforme   (ligne grisée)
   La ligne indisponible est grisée et porte sa RAISON en clair à droite, pas
   seulement une croix.

   Sous la liste, un bandeau AMBRE avec une icône d'avertissement :
   « Changer de backend graphique redémarre NkUIDesign. Le document, la
   disposition et la sélection sont enregistrés puis restaurés. »
   ⚠️ Ce bandeau doit être visuellement DIFFÉRENT du bandeau vert « immédiat »
   de la section Langue.

2. Un encadré d'information en chasse fixe, style journal :
     backend demandé : vulkan   (source : ligne de commande --gfx)
     backend retenu  : Vulkan
   Avec la légende : « Ce que l'application a écrit au démarrage. »

3. ÉCHELLE ET IMAGES — un curseur d'échelle DPI et un plafond d'images par
   seconde.

GÉNÉRER AUSSI, sur la même planche, DEUX MODALES :

Modale A — confirmation :
  Titre « Redémarrer NkUIDesign ? »
  Corps : « Le backend graphique passera de Vulkan à DirectX 12.
  Votre document, la disposition des panneaux et la sélection seront enregistrés
  puis restaurés. »
  Boutons : « Annuler » (secondaire), « Enregistrer et redémarrer » (primaire).

Modale B — échec, avec une icône d'erreur rouge :
  Titre « Le backend demandé a été refusé »
  Corps : « DirectX 12 : aucun pilote compatible.
  Rien n'a été remplacé en silence. NkUIDesign a rouvert avec Vulkan, le backend
  précédent. »
  Bouton : « J'ai compris ».
```

**La règle que ces deux modales incarnent** : *un backend indisponible se dit, il
ne se remplace pas en silence*. Un repli muet ferait passer une API absente pour
une API qui marche.

---

## 14. Dialogue Export + rapport de validation — 📝

```
[Coller le Système de design]

Génère la modale « EXPORTER » de NkUIDesign, 900 x 700, thème SOMBRE, sur un
fond d'éditeur assombri.

Partie haute — LES SORTIES, en cases à cocher, chacune avec une description
grise en dessous :
  ☑ Déclaration (.nkuidoc)        « le format natif — se relit sans perte »
  ☑ Bloc de contrat (callbacks)   « à recopier dans votre application »
  ☐ Atlas d'icônes + table de poignées
  ☐ Thème (fichier texte)
  ☐ Clés de traduction du document
  ☐ Image de l'interface (PNG)
Un champ « Dossier de destination » avec un bouton « Parcourir… ».

Partie basse — LE RAPPORT DE VALIDATION, dans un encadré à fond de champ,
TOUJOURS affiché (même quand tout va bien) :

  ✔  2 composants déclarés, 2 branchés au dessin
  ✔  aller-retour vérifié : 0 ligne inconnue
  ⚠  1 variante déclarée rendue par une autre (columns → dense_list)
  ⚠  4 événements déclarés sans callback branché
  ⚠  3 clés de traduction manquantes en English
  ℹ  1 nœud sans composant (cadre) — normal, il arrange

Chaque ligne a une icône de niveau (coche verte, avertissement ambre, info
grise). Aucune ligne n'est rouge : rien ici n'empêche l'export.

Pied de modale : « Annuler » (secondaire) et « Exporter » (primaire).
Sous les boutons, une mention grise en italique :
« L'export ne produit pas de code. La déclaration est la sortie. »
```

---

## 15. Aperçu interactif + console de callbacks — 📝

```
[Coller le Système de design]

Génère la FENÊTRE SECONDAIRE « APERÇU INTERACTIF » de NkUIDesign, 1200 x 800,
thème SOMBRE. Elle est visiblement distincte de la fenêtre principale : sa barre
de titre porte un bandeau coloré et le texte « APERÇU INTERACTIF — les clics
agissent sur l'interface, pas sur l'éditeur ».

Zone haute (70 %) : l'interface composée, qui TOURNE. Aucune poignée, aucun
contour de sélection, aucun repère d'agencement — rien de l'éditeur. Un bouton
de l'interface est en état survolé, un autre est enfoncé.

Barre d'outils de la fenêtre : « ▶ Rejouer », « ⏸ Pause », un sélecteur de
taille de simulation, un sélecteur de langue (pour tester la retraduction à
chaud sur l'interface produite), et un bouton « Fermer ».

Zone basse (30 %) : la CONSOLE DE CALLBACKS, en police à chasse fixe, la plus
récente en bas, horodatée :
    12:04:31  navigateur     onSelect(index: 3, path: "assets/hero.png")
    12:04:31  navigateur     onNavigate(path: "assets/")
    12:04:29  arbre_projet   onExpand(node: "assets")
    12:04:27  barre_outils   onActivate(index: 0, path: "")
Le nom du nœud est coloré, l'événement en gras, la charge en gris.
À droite de la console, un petit compteur : « 4 événements · 0 non traité ».

Une mention en pied : « Dans l'éditeur, un clic sélectionne. Ici, un clic
agit. »
```

---

## 16. Écran d'accueil — 📝

```
[Coller le Système de design]

Génère l'écran d'accueil de NkUIDesign, 1200 x 760, thème SOMBRE.

- Bandeau haut : logo NkUIDesign, nom du produit, numéro de version en gris.
- Colonne gauche (280 px) : quatre grands boutons empilés, chacun avec une icône
  en trait fin et un sous-titre gris —
    « Nouveau document »        · partir d'un écran de départ déjà rempli
    « Nouveau par l'IA »        · décrire l'interface, la corriger ensuite
    « Ouvrir un document »
    « Importer une déclaration »
  En bas de la colonne : « Paramètres » et « Documentation ».
- Zone centrale : « DOCUMENTS RÉCENTS » — une grille de 6 vignettes. Chaque
  vignette : un aperçu miniature de l'interface, le nom du document, et une
  ligne grise « modifié il y a 2 h · 12 nœuds ». Coins arrondis 6 px, léger
  surlignement au survol.

GÉNÉRER AUSSI la variante ÉTAT VIDE (première utilisation) : la grille est
remplacée par un bloc centré, sobre — une icône discrète, le titre « Aucun
document pour l'instant », une phrase expliquant l'outil en une ligne
(« Composez une interface à partir de composants, réglez-la, testez-la. »), et
les deux boutons « Nouveau document » et « Nouveau par l'IA » mis en avant.
Pas d'illustration décorative.
```

---

## 16bis. Panneau Aperçu — simulation d'appareil et **zone sûre** — 📝

```
[Coller le Système de design]

Génère une PLANCHE de quatre vignettes montrant le panneau « APERÇU » de
NkUIDesign en mode SIMULATION D'APPAREIL, thème SOMBRE, 1400 x 1000.

Barre d'outils commune, en haut de chaque vignette : un sélecteur d'appareil
(« Téléphone 6,1" ▾ »), un bouton de rotation (portrait / paysage), et une
bascule « Zones sûres » (activée).

Le contenu simulé est le même dans les quatre vignettes — une interface mobile :
  - un FOND en dégradé sombre / image de couverture, qui doit aller JUSQU'AU
    BORD de l'écran, y compris derrière l'encoche et derrière l'indicateur bas ;
  - une barre de titre avec du texte ;
  - une liste qui DÉFILE et passe visiblement sous la barre du haut ;
  - un gros bouton primaire « Continuer » en bas.

Quand la bascule « Zones sûres » est activée, les zones masquées sont dessinées
en SURIMPRESSION : hachures diagonales très discrètes + une ligne pointillée
délimitant la zone sûre, avec une petite étiquette donnant les valeurs
(« haut 44 · bas 34 »).

VIGNETTE A — « Portrait, avec encoche » :
  encoche noire en haut au centre, indicateur de geste en bas, coins arrondis.
  ✔ Le FOND passe sous l'encoche et sous l'indicateur, jusqu'aux bords.
  ✔ Le TEXTE et le BOUTON sont entièrement dans la zone sûre : le bouton
    s'arrête NETTEMENT au-dessus de l'indicateur de geste.
  ✔ La liste défile sous la barre du haut, mais son premier élément lisible
    commence sous la zone masquée.

VIGNETTE B — « Portrait, sans encoche » :
  écran rectangulaire, pas d'encoche, indicateur bas minimal.
  Même interface : le fond va toujours jusqu'aux bords, le bouton remonte
  légèrement puisque la zone sûre est plus grande. La disposition N'EST PAS
  identique à A — c'est le point de la vignette.

VIGNETTE C — « Paysage, avec encoche » :
  l'appareil est tourné ; l'encoche est maintenant sur le CÔTÉ, et les zones
  masquées sont à gauche et à droite (plus une bande basse fine).
  ✔ Le bouton et le texte s'écartent des bords LATÉRAUX.
  ✘ Montrer explicitement, par une étiquette, que les valeurs ont changé :
    « gauche 44 · droite 44 · bas 21 » — ce ne sont plus celles du portrait.

VIGNETTE D — « CE QU'IL NE FAUT PAS FAIRE » — un encadré rouge pâle avec deux
  moitiés côte à côte, chacune sous un petit titre :
    « Réglage global : tout dans la zone sûre » → le fond s'arrête à la zone
      sûre et laisse des BANDES NOIRES disgracieuses en haut et en bas.
    « Réglage global : tout jusqu'au bord » → le bouton « Continuer » passe
      SOUS l'indicateur de geste et devient INATTEIGNABLE (le montrer coupé
      par l'indicateur).
  Légende sous l'encadré : « Le choix se fait élément par élément, jamais
  globalement. »
```

**Ce que cette planche existe pour rendre visible** : la zone sûre est une
**ancre**, pas une marge — et les deux fautes de la vignette D sont commises par
le même réflexe, appliquer le même traitement à tout. Les valeurs affichées
changent entre A, B et C : **elles se demandent à la plateforme, elles ne sont
jamais codées en dur**.

---

## 17. États et composants isolés — les cas qui doivent se voir

```
[Coller le Système de design]

Génère une PLANCHE unique de composants isolés sur fond d'éditeur sombre,
1400 x 1000, disposés en grille avec un titre au-dessus de chacun. Ce sont les
états qui doivent être VISIBLES dans le produit — chacun existe pour qu'un
manque ne passe pas en silence.

1. « COMPOSANT DÉCLARÉ NON BRANCHÉ » — un cartouche rectangulaire à bordure
   pointillée occupant la place exacte que le composant occuperait, portant en
   son centre son nom (« mon_composant ») et, en dessous, en plus petit :
   « déclaré, aucune fonction de dessin ». Surtout PAS un rectangle vide.

2. « CLÉ DE TRADUCTION ABSENTE » — un libellé rendu comme
   « ⟦panneau.apercu.titre⟧ » dans un cadre d'avertissement discret.

3. « NŒUD SANS COMPOSANT (CADRE) » — un rectangle en pointillés fins, étiquette
   en italique gris « pied · cadre », avec la mention « arrange, ne dessine
   rien ». Ce n'est pas une erreur.

4. « NOTIFICATION » (trois variantes empilées, coin bas-droit) :
   - succès vert : « Document enregistré. »
   - avertissement ambre : « Thème rechargé — 2 clés inconnues ignorées. »
   - erreur rouge : « Le backend DirectX 12 a été refusé : aucun pilote
     compatible. »
   Chacune avec une icône, un texte, et une croix de fermeture.

5. « MENU CONTEXTUEL » (clic droit sur un nœud de l'arbre) :
   Renommer · Dupliquer · Supprimer · —— · Promouvoir en composant ·
   Envelopper dans un cadre · —— · Copier · Coller. Un sous-menu ouvert sur
   « Agencement ▸ » proposant ligne / colonne / grille / ancrage.

6. « LIGNE DE PROPRIÉTÉ » (les trois formes) : champ texte, curseur avec bornes
   affichées, et groupe de boutons segmentés.

7. « BARRE D'ÉTAT » en pleine largeur, avec ses cinq zones :
   backend · sélection · compte de nœuds · langue · état d'enregistrement.

8. « INDICATEUR DE MODIFICATION » : le titre de fenêtre dans ses deux états,
   « mon_ecran.nkuidoc » et « mon_ecran.nkuidoc • ».
```

---

## Notes d'usage

1. **Générer le thème Sombre d'abord**, puis reprendre chaque écran en Clair. La
   comparaison des deux est le contrôle : si un élément disparaît en Clair, c'est
   un défaut de rôle de couleur, pas une retouche locale à faire.
2. **Les libellés sont en français** dans les maquettes ; les mots techniques qui
   apparaissent dans un fichier (`fixed`, `row`, `grid`, `weight`, les noms
   d'événements) restent en anglais minuscules — c'est le vocabulaire du format,
   pas de l'interface.
3. **Ne jamais inventer de fonctionnalité** pour remplir un écran. Ces documents
   sont **incomplets par construction** : une zone non spécifiée se laisse sobre,
   elle ne se meuble pas.
4. **Sur mobile, montrer toujours les deux ancres dans la même planche.** Une
   maquette mobile qui n'affiche qu'un cas laisse croire qu'un réglage global
   suffit. Le fond va jusqu'au bord, le bouton s'arrête à la zone sûre — et les
   valeurs de zone sûre **changent** entre portrait et paysage, entre appareil
   avec et sans encoche. Ne jamais dessiner deux orientations avec les mêmes
   marges.
5. **Aucune maquette ne vaut preuve.** À ce jour, la fenêtre de NkUIDesign n'a
   jamais été ouverte. Ce que ces planches montrent est la **cible** ; l'état réel
   de chaque écran est en tête de sa section, et dans le document 1, § 3.
