# Document 6 — L'IA de design : cadrage et corpus

> Ouvert le **2026-08-20** au soir, à la demande de Rodolf : *« commence à
> travailler sur le corpus de l'IA de design et sur l'IA de design. »*
>
> ⚠️ **Ce document cadre, il ne décide pas.** Chaque section se termine par ce qui
> reste à trancher. Rien n'est lancé sans l'accord de Rodolf.

---

## 1. Ce que ce modèle doit produire — et pourquoi ça change tout

§16bis.1 l'a déjà tranché pour l'assistant : **il produit dans le vocabulaire de
l'outil, jamais dans le sien.** Un modèle de design pour NkUIDesign ne sort donc
**pas une image**. Il sort un **document structuré** : une hiérarchie d'éléments
portant des rôles, des modes de taille, des ancres, des bornes, de l'espacement,
des états et des animations.

⚠️ **C'est la contrainte qui commande tout le reste du document, et elle est
contre-intuitive** : on croit qu'un modèle de design s'entraîne sur des captures
d'interfaces. **Un corpus de captures n'apprend que l'apparence.** Il n'apprend ni
la structure qui la produit, ni les rôles, ni ce qui se passe quand la fenêtre
change de taille.

Il faut donc des **paires** : `(intention ou rendu) → document`.

**Reste à trancher** : rien ici. C'est une conséquence de §16bis.1, déjà décidé.

---

## 2. D'où viennent les paires — inventaire honnête

| source | ce qu'elle donne | ce qui coince |
|---|---|---|
| **Web — HTML/CSS + capture** | des millions de paires (structure, rendu), publiques, collectables | le CSS n'est pas le vocabulaire de NkUIDesign : il faut convertir |
| **Dispositions Android — XML** | très proche du modèle de widgets, **avec les rôles** | extraction lourde, licences à vérifier |
| **Trousses de design publiques** | qualité de conception élevée | formats propriétaires, licences floues |
| **Génération synthétique** | paires parfaites, illimitées, couvrant **tout** le vocabulaire | n'enseigne que le goût du générateur |
| **Documents de Rihen** | exactement le bon vocabulaire | beaucoup trop peu nombreux |

### 2.1 ⚠️ Le convertisseur EST le corpus

Quelle que soit la source, il faut un convertisseur de son langage de disposition
vers celui de NkUIDesign. **Écrire ce convertisseur, c'est l'essentiel du travail
de corpus.**

Et il ne se jette pas après : **c'est exactement la fonction d'import** que §14bis
prévoit déjà, et que le greffon d'exemple « Pont Figma » illustre.

### 2.1bis ⚠️ Correction du 2026-08-20, 23h : « la même chose » était trop fort

J'ai écrit plus haut, ce soir, que le travail de corpus et la fonction d'import
étaient **la même chose**. **Vérification faite dans l'écosystème, c'est à moitié
vrai, et la moitié fausse coûte cher.**

Le convertisseur a **deux consommateurs aux contraintes opposees** :

| | corpus | import dans le produit |
|---|---|---|
| quand | hors ligne, par lots | dans l'éditeur, en direct |
| où | poste de travail | dans le moteur |
| langage | libre — Python convient | **C++ zero-STL, sans dépendance tierce** |
| tolérance à l'échec | on jette la page et on passe | il faut un message à l'utilisateur |

**Ce qui est réellement partagé, c'est la SPÉCIFICATION DE LA CORRESPONDANCE** —
quelle construction HTML devient quelle construction NkUIDesign. Écrite une fois,
implémentée deux fois.

⚠️ **Et le côté produit est bien plus cher que je ne l'ai laissé entendre.**
Relevé le 2026-08-20 dans `Kernel/` : cinq modules Foundation, vingt-deux Runtime,
huit System — **aucun ne lit du balisage**. Ni HTML, ni CSS, ni XML. Un analyseur
HTML5 conforme est un chantier notoire ; la cascade CSS et sa disposition en sont
un autre. **En zero-STL et sans bibliothèque tierce, ce n'est pas une brique, c'est
un module.**

Conclusion à retenir pour la décision : **le corpus n'attend pas le produit.** La
chaîne hors ligne peut démarrer immédiatement ; l'import dans l'éditeur est un
chantier séparé, à chiffrer à part, et probablement à viser sur des formats
**déjà structurés** (JSON de Figma, XML Android) plutôt que sur du HTML brut.

### 2.1ter ⚠️ Ne pas écrire de moteur CSS : conduire un navigateur

Convertir fidèlement du HTML demande de **résoudre la disposition** — c'est le CSS
qui produit les positions, et on ne peut pas déduire une boîte d'une feuille de
style sans l'appliquer.

**Donc on n'écrit pas de moteur : on en conduit un.** Edge en mode sans tête est
**déjà utilisé dans ce projet** pour rendre les SVG en PNG. La même commande peut :

1. charger la page ;
2. rendre la **capture** ;
3. extraire, pour chaque élément, sa **géométrie calculée**, ses **styles
   calculés**, sa **balise** et ses **attributs ARIA**.

> **On cartographie à partir des valeurs CALCULÉES, jamais du CSS source.**

C'est ce qui fait passer le coût de « écrire un navigateur » à « piloter celui qui
est déjà installé ». Et cela rend la paire exacte par construction : la capture et
la géométrie viennent du **même rendu**, au même instant.

⚠️ **Le piège de cette voie** : les valeurs calculées donnent des positions
absolues, pas des **intentions**. Un élément à `x=240` ne dit pas s'il est ancré à
gauche, centré, ou en `fraction`. **Il faut redimensionner la fenêtre et observer
comment la géométrie bouge pour déduire l'ancrage.** Deux ou trois largeurs
suffisent à distinguer fixe, proportionnel et étiré — mais il faut le faire, sinon
le corpus n'enseigne que des positions gelées, et le modèle produira des
dispositions qui ne survivent pas au premier redimensionnement.

> C'est le vrai contenu technique de la chaîne : **rendre à plusieurs largeurs et
> déduire l'intention du mouvement.** Le reste est de la plomberie.

### 2.2 ⚠️ Le web enseigne précisément la partie la plus difficile

§14ter appelle l'attribution de rôle *« le moment décisif de tout l'outil »*. Or le
HTML sémantique **porte les rôles presque directement** :

| HTML | rôle NkUIDesign |
|---|---|
| `<button>` | bouton |
| `<input type=text>` | champ de saisie |
| `<input type=checkbox>` | case à cocher |
| `<select>` | liste déroulante |
| `<input type=range>` | curseur |
| `<a>` | lien (rôle de projet dérivant de bouton) |
| `role="tablist"` | barre d'onglets |

Les attributs ARIA en donnent davantage, et **gratuitement** : ils sont écrits par
des humains pour décrire l'intention d'un élément.

⚠️ **Une page sans HTML sémantique — tout en `<div>` — n'apprend rien de bon.** Le
filtre de collecte doit privilégier les pages qui utilisent les balises et les
attributs ARIA, quitte à réduire fortement le volume. *Un corpus deux fois plus
petit et correctement étiqueté vaut mieux qu'un corpus dix fois plus grand où le
modèle apprend que tout est un `div`.*

### 2.3 Ce que le web n'enseigne pas

Il n'a **pas** d'ancres au sens de §8quater.3, pas de bornes `min/max` attachées à
un mode de taille, pas de zone sûre, pas les trois familles d'animation de §9ter.

**C'est là que la génération synthétique sert** : produire des documents qui
couvrent délibérément ce que le web ne montre jamais, les rendre, et obtenir des
paires parfaites.

⚠️ **Mais un modèle entraîné uniquement sur du synthétique apprend notre goût, pas
le design.** Le synthétique couvre la **grammaire** ; le web apporte le **réel**.
Ni l'un ni l'autre seul.

**Reste à trancher** : la proportion des deux, et si l'on ouvre Android en plus.

---

## 3. Comment on saura que c'est bon

⚠️ **Pas par la perte.** Le projet interdit déjà de comparer des pertes entre
corpus différents, et ici la perte ne dirait rien de la qualité d'un design.

Trois mesures, de la plus objective à la plus faible :

### 3.1 Le validateur de l'outil est un juge gratuit et objectif

Un document produit par le modèle passe **la validation de NkUIDesign**. Tout code
`E-` est une **faute par construction**, sans jugement humain :

`E-BOUNDS-INVERTED` · `E-DISABLED-REENABLE` · `E-AMBIENT-NO-STATE` ·
`E-DRIVEN-NO-REST` · `E-SPACING-CYCLE` · `E-TOOLTIP-ONLY-SOURCE` ·
`E-WINDOW-NO-DRAG-REGION`

> **Le taux de documents produits sans aucune erreur de validation est la première
> métrique, et elle ne demande aucun annotateur.**

⚠️ **C'est un critère de correction, pas de qualité.** Un document parfaitement
valide peut être laid et inutilisable. Il élimine le n'importe quoi ; il ne
distingue pas le bon du médiocre.

### 3.2 Aller-retour

À partir d'un rendu, le modèle produit un document ; on rend ce document ; on
mesure l'écart entre les deux images. Objectif, automatisable.

⚠️ **Mais l'aller-retour récompense la copie, pas la conception.** Un modèle qui
reproduit fidèlement ne sait pas nécessairement créer. À utiliser comme garde-fou,
jamais comme but.

### 3.3 Exactitude des rôles

Sur un corpus de validation issu de pages à HTML sémantique, comparer le rôle
attribué au rôle attendu. **C'est la mesure la plus proche de ce que l'outil
promet.**

**Reste à trancher** : les seuils. Aucun ne doit être fixé après avoir vu les
premiers résultats — **§ à pré-enregistrer avant la première mesure.**

---

## 4. Taille et coût — ce que les mesures d'Ilyana permettent de dire

Le vocabulaire de sortie est **minuscule** devant la langue naturelle : quelques
dizaines de types d'éléments, une trentaine de propriétés, cinq modes de taille.
Ce modèle n'a pas besoin de la taille d'Ilyana.

Ordre de grandeur attendu : **quelques dizaines de millions de paramètres**, comme
le modèle de maillage de [[plan-trois-chantiers]] ③ — des **heures**
d'entraînement, pas des semaines.

⚠️ **Chiffre à ne pas oublier** : la campagne actuelle tourne à **12,85 s/pas
mesurés** sur cette carte, et le plafond d'accélération des chantiers GPU engagés
est de **×3,95** (Amdahl sur parts mesurées). Toute planification qui suppose mieux
est fausse.

⚠️ **Et la carte n'est pas libre.** Ilyana l'occupe jusqu'à la fin de l'horizon
6000. Rien ne commence côté entraînement avant.

**Reste à trancher** : rien pour l'instant — la question ne se pose qu'après le
premier lot de corpus.

---

## 5. Le premier pas, et sa condition d'arrêt

**Étape 1 — écrire le convertisseur HTML/CSS → `.nkgui`.** Pas un prototype
jetable : la brique d'import de §14bis.

**Étape 2 — convertir un premier lot de pages, et MESURER une seule chose :**

> **Quelle fraction des pages se convertit sans perte d'information ?**

Une page « convertie sans perte » est une page dont tous les éléments ont trouvé un
rôle ou un conteneur, et dont le rendu du document produit est visuellement proche
de la capture d'origine.

### ⚠️ À pré-enregistrer AVANT de mesurer

- **Ce que ce chiffre décide** : sous un certain seuil, la voie web est trop
  coûteuse et il faut basculer vers Android ou vers davantage de synthétique.
- **Ce qu'il ne prouvera pas** : ni que le modèle apprendra, ni que les designs
  produits seront bons. **Il ne mesure que la faisabilité de la collecte.**
- **Ce qui n'est PAS un signal** : une belle conversion sur des pages choisies à la
  main. L'échantillon doit être tiré au hasard dans la liste de collecte, **avant**
  de regarder les résultats.

**Reste à trancher, et c'est la première chose à demander à Rodolf** : le seuil,
écrit avant la mesure.

---

## 4bis. Les quatre catégories — et la découverte qui change le plan

> **Cadrage de Rodolf, 2026-08-21, 2h50** : quatre familles de documents — **apps
> web**, **apps mobiles**, **UI de jeux** (PC, web, mobile), **apps de bureau** —
> **1 000 sources tentées par famille**, et **au moins 25 % converties**.
>
> Objectif énoncé : *« assez de marge pour que notre système sache concevoir des
> applications et des jeux le plus facilement possible »*.

⚠️ **1 000 par famille est la taille de la SONDE, pas celle du corpus.** À 25 %,
cela donne 250 documents par famille — très en dessous de ce qu'un entraînement
demande. Ce que ces 4 000 tentatives mesurent, c'est **si la voie tient** ; si oui,
on passe à l'échelle sur le même outillage.

### 4bis.1 ⚠️ Trois familles sur quatre ne passent PAS par le navigateur

C'est le point qui change le plan de §2.1ter. J'avais construit toute la chaîne
autour du rendu sans tête, parce que je raisonnais sur le web. **Les trois autres
familles vivent dans des formats déjà structurés** — du texte, pas des pixels.

| famille | source | format | coût |
|---|---|---|---|
| **web** | pages publiques | HTML + CSS | **rendu à 3 largeurs** (§2.1ter) |
| **mobile** | applications libres (F-Droid) | **XML de disposition Android** | lecture directe |
| **jeux** | jeux libres faits avec Godot | **`.tscn`** | lecture directe |
| **bureau** | applications libres Qt / GTK | **`.ui` Qt**, **Glade**, **XAML** | lecture directe |

> **Le navigateur ne sert que pour le web.** Pour les trois autres, on lit un arbre
> déjà écrit — c'est plus simple, plus fiable, et sans étape de rendu.

### 4bis.2 ⚠️ Ces formats donnent ce que le web ne donne PAS

§2.3 affirmait que le web n'enseigne ni ancres, ni bornes, ni rôles complets, et
que **le synthétique devrait combler ces trous**. **C'est en grande partie faux
maintenant** :

| ce qui manquait au web | qui le porte, nativement |
|---|---|
| **ancres aux quatre bords** | **Godot** — `anchor_left/right/top/bottom` par nœud `Control` |
| **bornes min/max liées à un mode** | **Qt** — `minimumSize` / `maximumSize` |
| **modes de taille** | **Qt** — `sizePolicy` : `Fixed`, `Minimum`, `Maximum`, `Preferred`, `Expanding` |
| **conteneurs explicites** | Godot `VBoxContainer` / `HBoxContainer` / `GridContainer` ; Qt `QVBoxLayout` / `QHBoxLayout` / `QGridLayout` |
| **rôles** | Android (`Button`, `EditText`, `Switch`, `CheckBox`…), Qt, Godot |

⚠️ **La `sizePolicy` de Qt et le vocabulaire de §8quater.2 se recouvrent presque
mot pour mot**, et les ancres de Godot sont **exactement** §8quater.3. Ce n'est pas
une coïncidence : ces systèmes résolvent le même problème, et NkUIDesign a
redécouvert leurs réponses.

**Conséquence pour le plan** : la génération synthétique **descend en priorité**.
Elle reste utile pour ce que personne ne porte — zone sûre, les trois familles
d'animation de §9ter — mais elle ne doit plus enseigner la grammaire de base.

### 4bis.3 ⚠️ Kenney donne des pièces, pas des interfaces

Rodolf propose Kenney pour l'UI de jeux. **Kenney publie des jeux d'assets** —
panneaux, boutons, cadres, icônes, en images. **Il n'y a aucune structure à en
extraire** : une planche de sprites ne dit pas comment les éléments étaient
disposés, ni quel élément était un bouton.

Ce qui n'en fait pas une source à jeter, **mais pour un autre usage** :

- **apparence** — matière pour habiller des documents synthétiques ;
- **licence** — CC0 sur l'essentiel du catalogue, donc utilisable sans question.

**Pour de vraies dispositions de jeu**, la source est **Godot** : `.tscn` est un
format texte lisible, les nœuds `Control` portent leurs ancres, et il existe
beaucoup de jeux libres. Unity (UXML) et LOVE sont des sources secondaires.

### 4bis.4 Le seuil de 25 % ne veut pas dire la même chose partout

⚠️ **25 % est un chiffre de WEB.** Convertir une page demande de résoudre une
disposition, de deviner des intentions, et d'accepter que beaucoup de pages soient
inexploitables. **Pour les trois formats structurés, 25 % serait un mauvais
résultat, pas un succès** : l'arbre est déjà écrit, les rôles sont nommés, il n'y a
pas de rendu à interpréter.

**Attente honnête, à écrire avant de mesurer :**

| famille | plancher | ce qu'un résultat sous le plancher signifierait |
|---|---|---|
| web | **25 %** | la voie web coûte trop cher, on s'appuie sur les trois autres |
| mobile / jeux / bureau | **60 %** | **notre convertisseur est mauvais**, pas la source |

> C'est la différence qui compte : sur le web, un échec accuse **le corpus** ; sur
> un format structuré, un échec accuse **notre code**.

### 4bis.5 Ce qui reste à vérifier avant de collecter

- **licences** : F-Droid et Godot sont libres, mais chaque projet a la sienne —
  à relever automatiquement, pas à supposer ;
- **le web n'a pas de licence** — c'est le point le plus flou des quatre, et il
  doit être tranché par Rodolf avant toute collecte à l'échelle ;
- **biais de complexité** (§5) : mesuré par famille, pas globalement.

---

## 5bis. La spécification de la correspondance

> **C'est l'artefact partagé de §2.1bis** : écrit une fois, implémenté deux fois —
> hors ligne pour le corpus, en moteur pour l'import.
>
> ⚠️ **Tout se lit dans les valeurs CALCULÉES** rendues par le navigateur, jamais
> dans le CSS source (§2.1ter).

### 5bis.1 Rôles — depuis la balise et l'ARIA

| source | rôle NkUIDesign | note |
|---|---|---|
| `<button>`, `role="button"` | bouton | |
| `<input type=text\|email\|password>` | champ de saisie | `password` -> drapeau de masquage 🔴 (§14ter.4) |
| `<textarea>` | champ multiligne | |
| `<input type=number>` | champ entier ou décimal | selon `step` |
| `<input type=checkbox>` | case à cocher | |
| `<input type=radio>` | **bouton radio** 🔴 | pas de natif — à jeter ou à marquer |
| `<input type=range>` | curseur | |
| `<input type=color>` | sélecteur de couleur | |
| `<select>` | liste déroulante | |
| `<a href>` | lien | rôle de projet dérivant de bouton |
| `<img>`, `<svg>` | image | |
| `<p>`, `<span>`, `<h1..h6>`, texte nu | texte | |
| `<hr>` | séparateur | |
| `<progress>` | barre de progression | |
| `role="tablist"` | barre d'onglets | |
| `role="tree"` | nœud d'arbre + conteneur | |
| `role="menu"`, `<menu>` | menu | |
| `role="dialog"` | fenêtre + drapeau modal | |
| `title=`, `aria-label` sur une icône seule | **infobulle** | c'est une propriété, pas un rôle (§14quinquies.1) |
| `<div>`, `<section>`, `<nav>`… | **conteneur**, sans rôle | voir 5bis.2 |

⚠️ **`aria-disabled` et l'attribut `disabled` ne donnent pas le même état.**
`readonly` -> `ReadOnly`, `disabled` -> `Disabled`, `aria-busy` -> `Busy`
(§14quater.1). Les confondre effacerait précisément la distinction que la
spécification a pris la peine d'écrire.

### 5bis.2 Conteneurs — depuis `display` calculé

| `display` calculé | conteneur |
|---|---|
| `flex` + `row` | HBox |
| `flex` + `column` | VBox |
| `grid` | Grille |
| `block` avec plusieurs enfants en flux | VBox |
| `inline`, `inline-block` en série | Flux |
| `position: absolute` dans un parent positionné | Pile |
| `table` | Tableau |

⚠️ **Un conteneur à un seul enfant se supprime.** Le HTML réel empile des `div`
sans rôle structurel ; les recopier produirait des hiérarchies à douze niveaux
qu'aucun humain n'aurait dessinées — et le modèle apprendrait à les produire.
**On aplatit tant que l'aplatissement ne change ni la géométrie ni l'espacement.**

### 5bis.3 Taille — le point délicat

| observé | mode |
|---|---|
| largeur identique à toutes les largeurs de fenêtre | `fixed n` |
| largeur = fraction constante du parent | `fraction f` |
| `flex-grow > 0`, partage avec des frères | `weight g` |
| prend tout le reste, seul à grandir | `expand` |
| largeur suit le contenu, change avec le texte | `content` |
| `min-width` / `max-width` calculés | `NkSizeBounds` |

⚠️ **Le mode ne se lit pas sur un seul rendu.** `x=240` ne dit rien : il faut
**rendre à trois largeurs** et regarder bouger. C'est le cœur de la chaîne, pas un
raffinement (§2.1ter).

### 5bis.4 Ancrage — déduit du mouvement

Sur trois largeurs de fenêtre, pour chaque élément, on observe ses distances aux
bords du parent :

| observation | ancrage |
|---|---|
| distance gauche constante, droite variable | ancre gauche fixe |
| distance droite constante, gauche variable | ancre droite fixe |
| **les deux constantes** | **deux ancres actives : l'élément s'étire** |
| les deux variables, rapport constant | ancres proportionnelles |
| les deux variables, égales entre elles | centré, aucune ancre |

⚠️ **Trois largeurs, pas deux.** Deux points ne distinguent pas une relation
constante d'une relation proportionnelle qui passerait par là par hasard. Le
troisième point est ce qui transforme une coïncidence en règle.

### 5bis.5 Espacement, alignement, apparence

`padding` -> remplissage · `margin` -> marge · `gap` -> espacement enfants
(§8quater.5, et la distinction y est déjà écrite : **ne jamais convertir un `gap`
en marges**).

`justify-content` / `align-items` -> alignement (§8quater.6), `stretch` -> étirer.

`background-color`, `border`, `border-radius`, `opacity` -> Apparence.
`font-family/size/weight/line-height/letter-spacing`, `text-align` -> Typographie.
`box-shadow` -> ombre · `filter: blur` -> flou · `linear-gradient` -> dégradé ·
`mix-blend-mode` -> fusion (§8ter).

`@media (max-width:…)` -> `NkBreakpoint` sur l'axe largeur, **mais seulement pour
les règles qui changent effectivement la géométrie observée** entre deux rendus.
Une règle média qui ne déplace rien est du bruit.

### 5bis.6 ⚠️ Ce qui n'a PAS d'équivalent se journalise, jamais ne se jette en silence

Sans correspondance : `float`, `position: sticky`, `transform` 3D composé,
`clip-path`, animations CSS, pseudo-éléments `::before/::after` porteurs de
contenu, `<canvas>`, `<video>`, `<iframe>`, tout script.

**Chaque élément non convertible est compté et nommé dans un journal de perte.**

> C'est ce journal qui produit la mesure de §5 — la fraction de pages converties
> sans perte. **Sans lui, le chiffre serait inventé.**

⚠️ **Et il faut se méfier du silence des pseudo-éléments** : `::before` porte
souvent une icône ou un chevron **visible à l'écran**. Ignoré, le document produit
paraît complet et l'aller-retour visuel échoue sans qu'on sache pourquoi.

---

## 6. Ce que je n'ai PAS fait ce soir

- aucun corpus n'a été collecté ;
- aucun convertisseur n'a été écrit ;
- aucun entraînement n'a été lancé ;
- rien n'a été décidé sur les licences des sources.

⚠️ **Et je n'écris pas dans le code.** Ce document cadre le chantier pour que
Rodolf tranche, et pour qu'un agent puisse ensuite travailler sur une cible écrite
plutôt que devinée.

Voir [[plan-trois-chantiers]] · `3_NkUIDesign_Interface_Humaine.md` §16bis, §14ter,
§9ter · `5_NkUIDesign_Specification_Claude.md` §1quinquies à §1decies.
