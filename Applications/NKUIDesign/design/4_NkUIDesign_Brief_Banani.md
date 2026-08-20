# NkUIDesign — Brief de génération visuelle
### Document 4 — Destiné à Banani

> Chaque section est un prompt autonome à coller tel quel. Colle d'abord la section
> 0 (système de design), identique dans l'esprit à `03-specification-banani.md`
> (Aetherion Engine) pour garder la cohérence visuelle de l'écosystème Nkentseu,
> avec les ajouts propres à un outil de design d'interface (canvas clair, guides
> roses de snapping façon Figma).

---

## 0. Système de design (à coller en préfixe de chaque prompt)

```
Style général : éditeur de design d'interface professionnel, croisement entre
Figma/Lunacy (canvas de design) et Unreal Blueprint (canvas de comportement à
nœuds). Densité élevée, texte petit (11-13px), look outil pro.

Deux thèmes à produire, Light et Dark, façon GitHub / GitHub Dark Pro :

THÈME LIGHT :
- fond principal #ffffff, fond panneaux #f6f8fa, fond champs #eaeef2
- bordures #d0d7de, texte principal #1f2328, texte secondaire #656d76
- accent bleu #0969da, succès #1a7f37, alerte #9a6700, erreur #d1242f, violet #8250df

THÈME DARK :
- fond principal #0d1117, fond panneaux #161b22, fond champs #010409
- bordures #30363d, texte principal #e6edf3, texte secondaire #8b949e
- accent bleu #2f81f7, succès #3fb950, alerte #d29922, erreur #f85149, violet #a371f7

Typographie : sans-serif système, texte petit, labels de section discrets.
Icônes : trait fin (outline 1.5px), monochromes, jamais d'emoji.
Coins arrondis légers (4-6px) sur boutons/cartes, angles droits sur panneaux dockables.
Boutons primaires = fond accent, texte clair. Boutons secondaires = contour fin.

Spécificités de cet outil :
- le canvas de DESIGN garde un fond clair avec un semis de points de grille très
  discret, même en thème Dark (pas de damier de transparence, pas de fond noir).
- le canvas de BEHAVIOR (nœuds) garde un fond sombre quadrillé façon Blueprint,
  câbles Bézier colorés par type (rose=événement, blanc épais=flux, coloré fin=donnée).
- guides de snapping/alignement sur le canvas Design : lignes fines ROSE VIF #ff4fd8,
  volontairement hors de la palette GitHub pour rester visibles sur n'importe quel fond.
- badge "rôle" sur un élément déjà promu en widget : petit tag pilule avec fond
  bleu clair et texte bleu, coin supérieur gauche de la sélection.
```

---

## 0bis. Méthode — une planche par zone, jamais la fenêtre entière d'un coup

⚠️ **Constat de la session du 2026-08-20, à ne pas réapprendre.** Un prompt
décrivant la fenêtre complète a produit un canvas vide, sombre, sans bascule de
mode ni cluster de vue — trois fois de suite. Les mêmes éléments, demandés seuls
dans une planche dédiée, sont sortis justes du premier coup.

**Deux enseignements, du plus solide au plus incertain :**

1. **Les corrections exprimées en valeurs et étiquettes concrètes passent ; celles
   exprimées en description de scène ne passent pas.** `min 120 · max 320`,
   `"Se connecter"`, `48px`, `100% ▾` ont été rendus exactement. « le canvas est
   clair et contient deux cadres » a été ignoré quatre fois. **Nommer les objets et
   leur texte, pas l'ambiance.**
2. *Hypothèse non vérifiée* : sur une liste de six corrections, seules les deux
   **dernières** ont été appliquées. Cela peut venir de la longueur du prompt comme
   d'autre chose — c'est une piste, pas une règle. Dans le doute, **une correction
   à la fois, et la plus importante en dernier**.

**Procédure retenue :**

| étape | ce qu'on demande |
|---|---|
| 1 | une planche **par zone** (canvas seul, inspecteur seul, hiérarchie seule), en gros plan, occupant toute l'image |
| 2 | on itère sur chaque zone jusqu'à validation |
| 3 | la fenêtre complète se demande **en dernier**, en fournissant les planches validées comme images de référence, plutôt qu'en redécrivant tout |

⚠️ **Ne pas corriger une planche en redécrivant la fenêtre entière.** C'est
exactement ce qui a échoué.

---

## 1. Fenêtre principale — vue d'ensemble, mode Design

```
[Coller le Système de design]

Génère la fenêtre principale complète de l'éditeur, thème DARK, résolution 1920x1080.

EN HAUT, SEULEMENT DEUX BANDES HORIZONTALES FINES (il n'y a PAS de troisième bande d'outils) :
1. Un GRAND LOGO CARRÉ de 56x56 pixels, collé au coin supérieur gauche, qui CHEVAUCHE LES DEUX BANDES : il commence en haut de la première et se termine en bas de la seconde. Il est nettement plus grand que dans un éditeur ordinaire — c'est l'ancre visuelle de la fenêtre. Les deux bandes commencent donc à droite de ce logo, jamais au bord gauche.
2. Barre de menu très fine (28px), à droite du logo : menu horizontal en texte simple "Fichier  Édition  Affichage  Objet  Comportement  IA  Fenêtre  Aide" collé au bord droit du logo. Au centre exact de la fenêtre (par rapport à toute la largeur), le texte "Dashboard_Admin.nkgui" avec un petit point devant. Tout à droite, trois boutons de fenêtre.
3. Bande d'onglets très fine (28px) juste en dessous, également à droite du logo : 3 onglets "Dashboard_Admin ●" (actif, fond légèrement plus clair, petit liseré bleu en bas), "Landing_Page", "HUD_Jeu", chacun avec une icône miniature et un × au survol. Un bouton "+" à la fin.

Corps principal :
- Colonne gauche fine "Hiérarchie" : arbre avec 3 racines "Page — Connexion", "Page — Dashboard" (ouverte, développée avec des enfants indentés : "Panel_Header", "Bouton_Connexion" surligné bleu = sélectionné), "Page — Paramètres". Icônes de rôle devant chaque widget.
- JUSTE À DROITE DE LA HIÉRARCHIE ET PAR-DESSUS LE BORD GAUCHE DU CANVAS : une BARRE D'OUTILS FLOTTANTE VERTICALE, étroite (48px de large), coins arrondis, ombre portée nette, centrée verticalement. Elle contient une colonne d'icônes d'outils empilées : flèche de sélection (active, surlignée bleu), cadre, puis une icône de forme portant un PETIT CHEVRON en bas à droite (famille de formes : rectangle, rectangle arrondi, cercle, triangle, polygone), une plume vectorielle avec chevron elle aussi, un T de texte, une icône d'image, une règle. Les icônes à chevron indiquent des familles dépliables.
- Zone centrale, le CANVAS DE DESIGN, fond clair avec un très léger semis de points de grille. EN HAUT AU CENTRE DU CANVAS, FLOTTANT PAR-DESSUS LUI : un groupe de 4 boutons segmentés bien visibles "Design" (actif, surligné bleu) / "Behavior" / "Animation" / "Split", avec petites icônes (crayon / nœuds / chronomètre / colonnes). EN BAS À DROITE DU CANVAS, FLOTTANT AUSSI : un petit groupe compact "100% ▾" plus deux icônes bascule (grille, aimant).
  Sur le canvas : deux grands rectangles blancs avec ombre légère, côte à côte, représentant deux cadres. Le premier est ÉTROIT ET HAUT (format téléphone) étiqueté "Connexion — Mobile 390x844" ; le second est LARGE (format bureau) étiqueté "Dashboard — Bureau 1440x900". Dans le cadre mobile, un mini formulaire stylisé (deux champs, un bouton bleu "Se connecter" sélectionné, avec un petit badge pilule bleu clair "Bouton" au-dessus et des poignées de redimensionnement carrées autour).
- Colonne droite fine "Inspecteur" : en-tête "Bouton_Connexion", trois onglets "Design / Widget / Behavior" (Design actif), puis des champs Position X/Y, une ligne "Largeur : expand" et "Hauteur : fixed 44", un PETIT CARRÉ D'ANCRAGE (un rectangle central avec quatre petits traits autour, dont deux surlignés bleu pour indiquer un étirement horizontal), une rangée d'icônes d'alignement (gauche/centre/droite, haut/milieu/bas), une pastille de couleur de fond, un champ de coin arrondi.

En dehors de la colonne Inspecteur, tout contre le bord droit de la fenêtre, un rail fin vertical (28px) avec 2-3 petites pastilles rondes empilées (icônes : palette de composants, étoile/étincelle pour l'IA, bulle de callback). Rail identique fin en bas de fenêtre avec 2 pastilles (console, œil de preview).
```

---

## 1bis. Canvas seul — planche **validée** (2026-08-20)

⚠️ **Ce prompt a produit un résultat conforme. Ne pas le réécrire pour l'améliorer
— le repartir de là.** C'est la seule formulation du canvas qui ait fonctionné.

```
[Coller le Système de design]

Génère UNIQUEMENT la zone centrale de canvas d'un éditeur de design d'interface,
en gros plan, occupant toute l'image. Pas de panneaux latéraux, pas de barre de
menu — seulement le canvas et ce qui flotte au-dessus de lui.

LE FOND : gris très pâle, presque blanc, avec un semis de points de grille
discret. Clair, PAS sombre — c'est un canvas de design.

DEUX CADRES posés côte à côte sur ce fond, chacun rectangulaire, blanc, avec une
ombre portée légère et une petite étiquette de nom en gris juste au-dessus :

- À GAUCHE, un cadre étroit et haut, proportions d'un téléphone (environ deux
  fois plus haut que large). Étiquette : "Connexion — Mobile 390 x 844".
  Contenu : un titre en haut, deux champs de saisie rectangulaires vides
  empilés, et en dessous un BOUTON BLEU plein portant le texte "Se connecter".
  CE BOUTON EST SÉLECTIONNÉ : contour bleu vif de 2px, quatre petites poignées
  carrées blanches à ses quatre coins, et un petit badge pilule bleu clair
  portant le mot "Bouton" collé juste au-dessus de son coin supérieur gauche.

- À DROITE, un cadre large, proportions d'un écran d'ordinateur. Étiquette :
  "Dashboard — Bureau 1440 x 900". Contenu : une barre latérale grise verticale
  à gauche, trois cartes rectangulaires claires alignées en haut, et un grand
  rectangle clair en dessous représentant un graphique.

TROIS ÉLÉMENTS FLOTTANT AU-DESSUS DU CANVAS, en panneaux sombres à coins
arrondis avec ombre portée :

1. EN HAUT AU CENTRE : quatre boutons segmentés collés les uns aux autres,
   "Design" | "Behavior" | "Animation" | "Split". Chacun a une petite icône
   (crayon, nœuds reliés, chronomètre, deux colonnes). "Design" est actif :
   fond bleu accent, texte clair. Les trois autres sont ternes.

2. EN BAS À DROITE : un petit groupe compact contenant "100% ▾", puis une icône
   de grille, puis une icône d'aimant.

3. CONTRE LE BORD GAUCHE, à mi-hauteur : une barre verticale étroite de 48px de
   large, coins arrondis, ombre portée, contenant sept icônes d'outils empilées.
   La première (une flèche de sélection) est active, fond bleu. Deux des icônes
   portent un tout petit chevron en bas à droite.
```

**Réserves relevées sur le rendu, à corriger seulement lors de la composition
finale** — aucune ne remet la planche en cause :

- deux repères d'alignement magenta traversent tout le canvas alors que rien n'est
  déplacé ; les repères intelligents n'apparaissent **que pendant un glisser**, et
  se rapportent à l'élément sélectionné, jamais au centre du viewport ;
- l'étiquette du cadre mobile flotte trop haut au-dessus de son cadre — elle doit
  être **collée** au bord supérieur, à quelques pixels.

---

## 1ter. Inspecteur seul — planche à produire (zone suivante)

⚠️ **Même méthode que §1bis : l'inspecteur seul, en gros plan, occupant toute
l'image.** Les sections sont énumérées dans l'ordre normatif du doc 3 §12.2 — cet
ordre n'est pas indicatif, c'est le contrat.

```
[Coller le Système de design]

Génère UNIQUEMENT le panneau Inspecteur d'un éditeur de design d'interface, en
gros plan vertical, occupant toute la hauteur de l'image. Thème DARK. Pas de
canvas, pas de hiérarchie, pas de barre de menu — seulement ce panneau.

EN-TÊTE, tout en haut, toujours visible :
une petite icône de bouton, puis le nom "Bouton_Connexion" en gras, puis en
dessous en plus petit et en gris le mot "Rôle : bouton".

JUSTE SOUS L'EN-TÊTE, trois onglets côte à côte : "Design" | "Widget" |
"Behavior". "Design" est actif — texte clair et fin liseré bleu en dessous ;
les deux autres sont ternes.

ENSUITE, LES SECTIONS EMPILÉES DANS CET ORDRE EXACT. Chaque section a un titre
en LETTRES CAPITALES petites et grises, précédé d'un chevron (pointant vers le
bas si dépliée, vers la droite si repliée) :

1. "CIBLE" — dépliée. Une seule ligne : libellé "Cible du cadre" à gauche, et à
   droite un sélecteur grisé affichant "Mobile — 390 x 844".

2. "DISPOSITION" — dépliée. C'EST LA SECTION LA PLUS IMPORTANTE DE L'IMAGE.
   - une ligne "Position" avec deux champs côte à côte étiquetés "X 24" et
     "Y 512" ;
   - une ligne "Largeur" avec un sélecteur affichant "expand", et JUSTE EN
     DESSOUS une ligne subordonnée, en retrait et en texte plus petit, portant
     deux champs : "min 120" et "max 320" ;
   - une ligne "Hauteur" avec un sélecteur affichant "fixed 44", et JUSTE EN
     DESSOUS la même ligne subordonnée en retrait : "min 36" et "max —".
     LE TIRET EST VOLONTAIRE : il signifie "aucune borne". Ne pas mettre 0, ne
     pas laisser le champ vide.
     Comme la hauteur est en "fixed", ces deux champs de bornes sont GRISÉS
     mais bien présents et lisibles.

3. "ANCRAGE" — dépliée. Au centre de la section, un CARRÉ D'ANCRAGE : un
   rectangle gris au milieu représentant l'élément, et quatre traits autour de
   lui. Les traits GAUCHE et DROIT sont BLEU VIF et TOUCHENT le rectangle —
   l'élément s'étire horizontalement. Les traits HAUT et BAS sont gris pâle et
   nettement DÉTACHÉS du rectangle, séparés par un vide visible.

4. "ALIGNEMENT" — dépliée. Deux rangées de quatre petites icônes carrées.
   Rangée du haut (horizontale) : aligner à gauche, centrer, aligner à droite,
   étirer — "centrer" est actif, fond bleu. Rangée du bas (verticale) : haut,
   milieu, bas, étirer — "milieu" est actif.

5. "ESPACEMENT" — REPLIÉE. Seulement le titre et son chevron pointant à droite.

6. "APPARENCE" — dépliée. Une ligne "Fond" avec une pastille de couleur bleue et
   le code "#4C6FFF" ; une ligne "Bordure" avec une pastille grise et "1 px" ;
   une ligne "Rayon" avec le champ "8" ; une ligne "Opacité" avec "100 %".

7. "TYPOGRAPHIE" — dépliée. Une ligne "Police" avec "Inter · Semi-Bold", une
   ligne avec deux champs "Taille 15" et "Interligne 1.4", et une rangée de
   trois icônes d'alignement de texte dont celle du centre est active.

8. "EFFETS" — REPLIÉE. Titre, chevron à droite, et un petit badge rond gris
   portant le chiffre "2" à droite du titre.

9. "POINTS DE RUPTURE" — REPLIÉE. Titre, chevron à droite, un badge rond gris
   "1", et à côté un PETIT BADGE AMBRÉ portant le texte "1 jamais atteinte".

Le panneau est étroit et dense, chaque ligne fait la même hauteur, les libellés
sont alignés à gauche en gris et les valeurs alignées à droite en clair.
```

⚠️ **Ce qui ne doit PAS apparaître sur cette planche** : la section "LAYOUT
PARENT" (l'élément n'est pas dans un conteneur live dans cet exemple), et le
bouton "Attribuer un rôle" (l'élément porte déjà un rôle — le bouton n'est là que
pour les éléments qui n'en ont pas).

---

## 2. Inspecteur — vue "Objets de la scène" (rien sélectionné)

```
[Coller le Système de design]

Génère uniquement le panneau Inspecteur en gros plan, thème LIGHT, occupant toute l'image, dans l'état "rien n'est sélectionné".

En-tête du panneau : titre "Objets de la scène" (pas de nom d'élément puisque rien n'est sélectionné).
Corps : une grille de 3-4 vignettes verticales, chacune représentant une page du projet — miniature rectangulaire claire montrant un aperçu simplifié de la mise en page (quelques rectangles gris représentant les zones), nom en dessous ("Connexion", "Dashboard", "Paramètres"), légère ombre portée, coins arrondis. Une des vignettes est survolée (légère bordure bleue).
En bas de la grille, un bouton en pointillés "+ Nouvelle page".
```

---

## 3. Bascule vers le mode Split — Design + Behavior côte à côte

```
[Coller le Système de design]

Génère la même fenêtre principale que le prompt 1, thème DARK, mais avec le canvas central maintenant divisé en deux par un diviseur vertical fin draggable.

Moitié gauche : canvas de Design (fond clair semis de points) avec le cadre "Page — Connexion" et son bouton "Bouton_Connexion" sélectionné (contour bleu, poignées).

Moitié droite : canvas de Behavior, fond sombre quadrillé façon Blueprint. En haut de cette moitié, un petit dropdown contextuel "Portée : Ce composant ▾". Le graphe montre : un nœud rose "EventClick" relié par un câble blanc épais à un nœud bleu "CallCallback" nommé "OnLoginClicked", avec un troisième nœud fin "GetWidgetValue" (données, câble fin coloré) alimentant un paramètre du CallCallback.

Le bouton segmenté "Design/Behavior/Split" de la toolbar du haut montre "Split" actif (surligné), avec une petite icône supplémentaire à côté indiquant l'orientation verticale du split (deux colonnes).
```

---

## 4. Rail de pastilles — état replié / overlay déployé

```
[Coller le Système de design]

Génère deux images côte à côte (ou deux variantes successives) montrant le comportement du rail de pastilles, thème DARK.

Variante A "Replié" : bord droit du canvas avec un rail fin vertical (28px) contenant 3 pastilles rondes empilées avec des icônes (grille/composants, étincelle IA, bulle callback), aucune n'est active, juste les icônes visibles sur fond légèrement différent du canvas.

Variante B "Overlay déployé" : la pastille du milieu (étincelle IA) est maintenant active (légèrement surlignée), et un panneau de largeur ~320px a glissé par-dessus le canvas depuis le bord droit (le canvas en dessous reste visible mais légèrement assombri/estompé sous le panneau, façon tiroir superposé avec une ombre portée marquée sur son bord gauche). Le panneau affiche un en-tête "Assistant IA" avec deux petites icônes en haut à droite : une icône "épingle" (pour ancrer) et une icône "détacher" (fenêtre flottante). Corps du panneau : un historique de chat avec une bulle utilisateur "Ajoute un champ mot de passe" et une bulle assistant contenant une petite carte d'aperçu miniature avec deux boutons "Appliquer" (vert) et "Rejeter" (gris), puis un champ de saisie en bas avec 3 boutons rapides "Générer un écran / Modifier la sélection / Générer un comportement".
```

---

## 5. Chat IA — fenêtre flottante détachée

```
[Coller le Système de design]

Génère une petite fenêtre flottante indépendante, thème DARK, flottant au-dessus d'un fond de canvas assombri/flouté en arrière-plan (juste suggéré, pas détaillé).

La fenêtre a sa propre mini barre de titre "Assistant IA" avec une icône étincelle et un bouton fermer, redimensionnable (poignée en coin inférieur droit visible), contenu identique au panneau overlay du prompt précédent (historique de chat + carte d'aperçu Appliquer/Rejeter + champ de saisie + 3 boutons rapides), mais dans un cadre de fenêtre avec une ombre portée large indiquant qu'elle flotte librement au-dessus de tout le reste de l'interface.
```

---

## 6. Bouton contextuel IA sur une sélection

```
[Coller le Système de design]

Génère un gros plan du canvas de Design, thème LIGHT, avec un panneau de type "carte produit" sélectionné (contour bleu, poignées de redimensionnement carrées aux coins et milieux de bords).

Juste au-dessus du coin supérieur droit de la sélection, un petit bouton flottant rond avec une icône étincelle blanche sur fond dégradé bleu-violet, légèrement en surbrillance comme s'il venait d'apparaître au survol. À côté, suggérer (esquisser en transparence, comme une infobulle) un petit popover qui commence à s'ouvrir avec juste un champ de texte "Décrivez la modification..." — ne pas développer complètement le popover, montrer juste son amorce pour indiquer l'interaction.
```

---

## 7. Palette de composants (pastille dépliée)

```
[Coller le Système de design]

Génère le panneau "Palette de composants" en gros plan, thème DARK, tel qu'il apparaît déplié depuis une pastille.

En-tête "Composants" avec une barre de recherche. Corps organisé en sections repliables avec leur titre en majuscules discrètes : "CONTENEURS" (ouverte : icônes+labels "VBox", "HBox", "Grid", "Panel"), "ENTRÉES" (ouverte : "Button", "SliderFloat", "InputText", "Checkbox", "ColorPicker4"), "AFFICHAGE" (fermée, juste l'en-tête avec chevron), "NAVIGATION" (fermée). Chaque icône de composant est une petite représentation schématique simple du widget (ex. Button = petit rectangle avec un contour, SliderFloat = ligne avec un curseur rond).
```

---

## 8. Gestionnaire de callbacks / contrôleurs

```
[Coller le Système de design]

Génère le panneau "Gestionnaire de callbacks", thème LIGHT, plein écran type tableau.

Colonnes : "Nom", "Contrôleur", "Signature", "Utilisé par", "Statut".
Lignes :
- "OnPositionChanged" | "TransformInspector" | "(axis: Enum[X,Y,Z], value: Float) → Void" | "SliderFloat_X, SliderFloat_Y (liens cliquables en bleu)" | pastille verte "Lié en test"
- "OnResetClicked" | "TransformInspector" | "() → Void" | "Bouton_Reset" | pastille verte "Lié en test"
- "WarnHighValue" | "(global)" | "() → Void" | "— aucun —" en gris italique | pastille grise "Jamais lié"
- "OnSubmitForm" | "LoginController" | "(email: String, password: String) → Void" | "Bouton_Connexion" | pastille rouge "Callback non déclaré" (ligne entière avec un léger fond rosé d'alerte)

En haut : bouton "+ Nouveau contrôleur", bouton "+ Nouveau callback", et deux filtres toggle "Callbacks orphelins" / "Appels invalides".
```

---

## 9. Inspecteur — onglet Behavior avec statuts d'événements

```
[Coller le Système de design]

Génère le panneau Inspecteur en gros plan, thème DARK, onglet "Behavior" actif parmi "Design / Widget / Behavior".

En-tête : "Bouton_Connexion" avec icône bouton.
Liste d'événements disponibles pour ce rôle, chacun sur une ligne avec le nom de l'événement à gauche et une pastille de statut colorée à droite :
- "Click" — pastille bleue pleine "Lié" 
- "Hover" — pastille grise creuse "Non lié"
- "DoubleClick" — pastille rouge "Invalide" avec un petit triangle d'avertissement à côté

Chaque ligne a une légère flèche ">" à l'extrême droite suggérant qu'elle est cliquable pour ouvrir le graphe correspondant.
```

---

## 10. Fenêtre de simulation du système

```
[Coller le Système de design]

Génère une VÉRITABLE FENÊTRE D'APPLICATION SÉPARÉE (avec sa propre barre de titre et ses propres bords, PAS un panneau intégré à l'éditeur), thème DARK, posée en flottant au-dessus d'un éditeur flou en arrière-plan.

En haut de cette fenêtre, une mini barre de simulation : à gauche l'étiquette de cible "Mobile — 390 x 844" et la taille actuelle "412 x 900", à droite trois boutons compacts "⟲ Recharger", "⏸ Geler", "⧉ Comparer".

Corps : le rendu réel de l'interface simulée — un vrai formulaire de connexion propre et fini, deux champs de saisie et un bouton bleu "Se connecter" en état SURVOLÉ (légèrement plus clair, curseur visible dessus) pour montrer que les interactions sont réelles. Pas un wireframe : un rendu final.

La fenêtre a des poignées de redimensionnement visibles sur son bord droit, avec une petite bulle d'aide "Redimensionner pour voir le responsive".

En dessous, un panneau plus petit "Console de simulation", fond très sombre, texte monospace, lignes horodatées : "[12:03:41] Bouton_Connexion — événement 'cliqué' — callback OnSubmitForm (factice, aucun effet réel)".
```

---

## 11. Dialogue Export / Validation

```
[Coller le Système de design]

Génère une fenêtre modale "Exporter le projet", thème LIGHT, overlay sombre semi-transparent autour.

En haut : résumé "3 pages · 24 widgets · 6 callbacks déclarés · 5 liés · 1 orphelin".
Corps : liste de validation avec icônes de sévérité — une ligne rouge avec icône croix "E-CALLBACK-UNDECLARED : 'OnSubmitForm' référencé mais non déclaré (Bouton_Connexion)", une ligne jaune avec icône triangle "W-CALLBACK-UNBOUND : 'WarnHighValue' jamais lié", une ligne grise avec icône info "W-ORPHAN-GEOMETRY : forme 'Rectangle_12' non promue". Chaque ligne cliquable, soulignée au survol.
En dessous, une section "Découpage en fichiers" avec un petit arbre éditable montrant "Theme.nkgui", "Widgets/Inspecteur.nkgui" avec des pages glissées dedans.
Bouton "Exporter" en bas à droite, grisé/désactivé (à cause de l'erreur bloquante), bouton "Annuler" à côté.
```

---

## 12. Launcher — Nouveau projet (3 branches)

```
[Coller le Système de design]

Génère l'écran "Nouveau projet" du Launcher, thème DARK, dans le style déjà établi pour le Launcher Aetherion (sidebar gauche + contenu), mais avec un choix plus direct à 3 grandes cartes plutôt qu'un stepper complet.

Trois grandes cartes côte à côte : "Vierge" (icône page blanche, description courte "Un canvas infini vide"), "Gabarit" (icône grille, description "Choisir parmi des mises en page prêtes"), "Via IA" (icône étincelle, contour légèrement dégradé bleu-violet pour la distinguer, description "Décrivez votre interface, l'IA génère un premier jet éditable"). La carte "Via IA" a en dessous un champ de texte déjà visible "Ex: un dashboard d'administration avec une barre latérale..." suggérant que le clic ouvre directement la saisie.
```

---

---

## 13. Édition vectorielle au sommet — Pen tool

```
[Coller le Système de design]

Génère un gros plan du canvas de Design, thème LIGHT, en mode édition de points sur une forme.

Une forme organique (silhouette d'icône stylisée, ni rectangle ni cercle) dessinée par un tracé fermé. Le contour du tracé est visible en trait fin bleu. Sur le contour, plusieurs petits carrés blancs à bordure bleue = points d'ancrage, régulièrement espacés. Sur l'un des points sélectionné (carré plein bleu), deux petites poignées symétriques dépassent de part et d'autre le long d'une tangente, terminées par de petits ronds creux bleus, reliées au point par des segments fins — représentant une poignée de courbe de Bézier "Miroir". Un autre point plus loin sur le tracé est un simple carré blanc sans poignées (coin dur).

Barre d'outils flottante au-dessus de la sélection : icônes "Ajouter un point", "Supprimer un point", "Couper le tracé", et trois icônes de type de coin (dur/miroir/asymétrique) dont "Miroir" est actif/surligné.

En bas à droite du canvas, un petit panneau flottant "Opérations" avec 4 boutons "Union / Soustraction / Intersection / Exclusion" sur deux formes se chevauchant légèrement en fond (un cercle et un carré, prévisualisation schématique).
```

---

## 14. Panneau Effets

```
[Coller le Système de design]

Génère le panneau Inspecteur en gros plan, thème DARK, onglet "Design" actif, faisant défiler jusqu'à la section Effets.

Sections repliables empilées, toutes ouvertes :
- "REMPLISSAGE" : une rampe de dégradé horizontale allant du bleu au violet, avec deux petits curseurs ronds de couleur dessus (aux extrémités), un dropdown "Linéaire ▾" au-dessus, un champ d'angle "45°" à côté.
- "CONTOUR" : swatch de couleur, champ "2px", dropdown "Centre ▾", un aperçu de motif de tirets en dessous (petite ligne pointillée éditable).
- "OMBRES" : deux lignes empilées "Ombre portée : X 0 Y 4 Flou 12 Étendue 0" et "Ombre interne : X 0 Y 2 Flou 4", chacune avec une petite swatch de couleur sombre semi-transparente et une poignée de glisser à gauche pour réordonner, bouton "+ Ajouter une ombre" en dessous.
- "FLOU" : deux sliders "Flou de calque" (à 0) et "Flou d'arrière-plan" (à 8, actif, effet "verre dépoli" suggéré par un léger halo dans l'icône du slider).
- Tout en bas, hors section repliable : dropdown "Fusion : Normal ▾" et un slider "Opacité : 100%".
```

---

## 15. Bibliothèque de composants — instance avec override

```
[Coller le Système de design]

Génère deux éléments juxtaposés, thème LIGHT.

À gauche, le panneau "Bibliothèque de composants" déplié depuis une pastille : grille de 4 miniatures de composants ("Bouton_Primaire", "Carte_Produit", "Champ_Recherche", "Badge_Statut"), chacune avec un petit badge en coin indiquant le nombre d'instances ("×12", "×4"...), une icône de provenance discrète (maison = local, flèche de téléchargement = importé).

À droite, l'Inspecteur d'une instance sélectionnée du composant "Bouton_Primaire" : en-tête avec le nom, un bandeau discret "Instance de Bouton_Primaire" avec un lien cliquable vers le maître, et un bouton "Détacher l'instance" en contour rouge léger. En dessous, la liste de propriétés Design habituelle, mais une des lignes (couleur de fond) a une petite pastille violette pleine dans sa marge gauche indiquant un override local, avec une icône de réinitialisation (flèche circulaire) au survol de cette ligne.
```

---

## 16. Animation de widget — State Machine

```
[Coller le Système de design]

Génère le canvas "Animation" pour un widget, thème DARK, plein écran, même style que le graphe de State Machine d'Aetherion.

En haut : dropdown "Portée : Ce widget (Bouton_Connexion) ▾".

Graphe : état "Entry" (rond plein) relié à "Idle" (état actif, bordure bleu accent, mini-aperçu montrant le bouton dans son état normal). "Idle" a deux flèches de transition : une vers "Hover" (étiquette "Événement : Hover", petite icône curseur de souris) et une vers "Pressed" (étiquette "Événement : Click"). "Hover" a une flèche retour vers "Idle" et une vers "Pressed". Chaque état affiche une mini-vignette avec une légère variation visuelle du bouton (couleur plus claire pour Hover, légèrement enfoncé/assombri pour Pressed).

Barre d'outils : toggle "State Machine / Dope Sheet" (State Machine actif), bouton "+ Ajouter un état", bouton "▶ Prévisualiser l'animation" en vert.
```

---

## 17. Animation de widget — Dope Sheet / Curve Editor de transition

```
[Coller le Système de design]

Génère le même canvas Animation, thème DARK, mais basculé en vue "Curve Editor" pour la transition "Idle → Hover" sélectionnée.

Colonne gauche : liste de propriétés animées avec cases à cocher — "Couleur de fond" (cochée), "Échelle" (cochée), "Rayon de coin" (décochée, grisée).
Zone principale : un graphe de courbe montrant une accélération de type "Ease Out" (montée rapide puis ralentissement), avec deux points de keyframe (0% et 100%) reliés par la courbe, poignée de tangente visible sur le point de départ.
En haut à droite du graphe, une rangée de presets rapides sous forme de petites icônes de courbe schématiques : "Linéaire", "Ease In", "Ease Out" (actif/surligné), "Ease In-Out", "Bounce", "Élastique".
En bas : champ "Durée : 180ms".
```

---

## 18. Mode Split — Design + Animation

```
[Coller le Système de design]

Génère la fenêtre principale, thème DARK, canvas divisé verticalement.

Moitié gauche : canvas de Design avec un bouton sélectionné, actuellement affiché dans un état visuellement "Hover" (légèrement plus clair, ombre portée plus marquée) pour illustrer la prévisualisation en direct.
Moitié droite : canvas Animation en vue State Machine (comme le prompt 16), avec l'état "Hover" actuellement surligné en vert pour indiquer qu'il est celui prévisualisé à gauche.

Dans la toolbar du haut, le bouton segmenté montre "Split" actif avec un petit menu déroulant ouvert à côté listant "Design + Behavior / Design + Animation (coché) / Behavior + Animation".
```

---

## 19. Attribuer un rôle à une forme dessinée

```
[Coller le Système de design]

Génère un gros plan sur le panneau Inspecteur, thème DARK, au moment où l'utilisateur donne un rôle à une forme.

À gauche du cadrage, un bout de canvas avec un rectangle arrondi bleu sélectionné (poignées carrées visibles).

À droite, l'Inspecteur, et au premier plan une LISTE DÉROULANTE OUVERTE intitulée "Attribuer un rôle", avec des entrées à icônes : "Bouton" (survolé, surligné bleu), "Case à cocher", "Interrupteur", "Champ de saisie", "Curseur", "Liste", "Onglets", "Arbre".

En dessous de la liste, un aperçu de ce que le rôle apporte, en trois petits blocs empilés avec des titres : "États : repos · survol · pressé · désactivé · focus" (cinq petites pastilles côte à côte), "Événements : pressé · relâché · cliqué · survol entré · survol sorti" (cinq puces avec une icône d'éclair), et une ligne d'action discrète en bas "+ Ajouter mon propre événement".

Une petite note d'avertissement discrète en bas du panneau, fond légèrement ambré : "Un rôle donne un point de départ complet, jamais une cage — vous pouvez retirer ou ajouter des événements."
```

---

## 20. Cibles, responsive et carré d'ancrage

```
[Coller le Système de design]

Génère un gros plan sur la section "Disposition" du panneau Inspecteur, thème DARK.

En haut, une ligne "Cible du cadre" avec un sélecteur affichant "Mobile — 390 x 844 ▾".

En dessous, deux lignes de taille avec un sélecteur chacune : "Largeur : expand ▾" et "Hauteur : fixed 44 ▾". Le menu de la largeur est entrouvert et montre les options empilées : fixed, content, fraction, weight, expand (expand surligné bleu).

Sous CHACUNE de ces deux lignes, une ligne subordonnée en retrait et en texte plus petit, portant les bornes : sous la largeur "min 120" et "max 320" ; sous la hauteur "min 36" et "max —". Le tiret signifie "aucune borne" — ni 0, ni champ vide. Les bornes de la hauteur sont GRISÉES parce que la hauteur est en "fixed", mais restent visibles et à leur place.

Au centre, l'élément le plus visible du cadrage : un CARRÉ D'ANCRAGE — un rectangle central gris représentant l'élément, et quatre petits traits autour de lui (haut, bas, gauche, droite) représentant les ancres vers le parent. Les traits gauche ET droit sont surlignés en bleu vif et reliés au rectangle, ce qui signifie que l'élément s'étire horizontalement ; les traits haut et bas sont gris pâle et non reliés.

À droite du carré d'ancrage, deux petites rangées d'icônes d'alignement : une rangée horizontale (aligner à gauche, centrer, aligner à droite, étirer) et une rangée verticale (haut, milieu, bas, étirer), avec "centrer" actif dans la rangée horizontale.

Sous le carré d'ancrage, une section "Espacement" avec trois lignes à quatre champs chacune : "Remplissage" (16, 16, 16, 16 avec une petite icône de cadenas de liaison active), "Marge" (0, 8, 0, 8), et "Espacement enfants" avec deux champs seulement (H : 8, V : 12).

Tout en bas, une ligne repliée "Points de rupture (1)" avec un chevron, et à côté un petit badge d'avertissement ambré "1 règle jamais atteinte".
```

---

## 21. Canvas Behavior seul — graphe en cours d'exécution

```
[Coller le Système de design]

Génère le canvas de comportement occupant toute l'image, thème DARK, fond sombre quadrillé façon Blueprint.

En haut à gauche, FLOTTANT par-dessus le canvas, un petit sélecteur "Portée : Ce composant ▾". En haut à droite, un petit bouton d'icône "⟨⟩" pour basculer en vue Code. Contre le bord gauche, la barre d'outils flottante verticale étroite (48px, coins arrondis, ombre) avec des icônes de nœud et de liaison.

Le graphe, de gauche à droite : un nœud à EN-TÊTE ROSE "Événement — cliqué" ; un câble BLANC ÉPAIS partant de sa sortie vers un nœud à EN-TÊTE AMBRÉE "Condition — champ non vide" qui a deux sorties "vrai" et "faux" ; depuis "vrai", un câble blanc épais vers un nœud à EN-TÊTE BLEUE "Action — appeler OnSubmitForm". En dessous et à l'écart, un nœud à EN-TÊTE GRISE "Calcul — valeur du champ", relié par un câble FIN ET COLORÉ (vert) à une entrée du nœud Condition.

TRÈS IMPORTANT — le graphe est en cours d'exécution : les câbles blancs du chemin Événement → Condition → Action SONT LUMINEUX ET PULSENT (halo bleuté), les nœuds traversés sont éclairés, tandis que la branche "faux" et son câble restent TERNES ET GRISÂTRES. Sur chaque nœud traversé, un petit compteur discret en coin ("×3", "×3", "×3") ; sur la branche non prise, "×0" en rouge pâle.
```

---

## Notes d'usage pour Banani

- Générer les écrans 1 à 3 en premier (ils partagent la même coquille), puis 4-6
  (système de pastilles et IA, le plus spécifique à cette demande), puis 7-12,
  puis 13-18 (édition vectorielle, effets, composants, animation de widget — les
  ajouts les plus récents).
- Toujours coller les codes hexadécimaux exacts de la section 0, y compris le rose
  de snapping `#ff4fd8` qui est une exception volontaire à la palette GitHub.
- Vérifier que le canvas de Design ne devient jamais un damier de transparence
  (réflexe fréquent des IA de génération pour un "canvas de design") — c'est le
  canvas Behavior qui a un fond sombre, pas le Design.
