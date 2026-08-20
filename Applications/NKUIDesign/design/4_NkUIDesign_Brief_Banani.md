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
   - une ligne "Largeur" avec un sélecteur affichant "expand", et à l'extrémité
     droite de cette MÊME ligne un PETIT MARQUEUR GRIS en lecture seule
     "120–320" ;
   - une ligne "Hauteur" avec un sélecteur affichant "fixed 44", et AUCUN
     marqueur à sa droite — aucune borne n'est posée sur la hauteur.
     AUCUNE ligne supplémentaire sous les lignes de taille : une dimension =
     une seule ligne.

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

## 22. Planches validées — repartir de là, ne pas réécrire

> **Archivées le 2026-08-20.** Toutes ont produit un résultat jugé conforme par
> Rodolf. §0bis s'applique : **on repart de ce texte, on ne le réécrit pas pour
> l'améliorer.**

⚠️ **Elles sont consignées TELLES QU'ELLES ONT ÉTÉ LANCÉES**, y compris quand il a
fallu deux passes : le prompt de base, puis le prompt de correction. **Je n'ai pas
fusionné les deux en un prompt unique** — une version consolidée n'aurait jamais
été exécutée, et l'archiver comme validée en ferait une promesse que rien ne
soutient. *La suite a marché ; sa fusion est une hypothèse.*

Le prompt du **canvas Design** est en §1bis, celui de l'**inspecteur** en §1ter.
Chaque planche ci-dessous se lance après avoir collé le Système de design (§0).

---

### 22.1 Inspecteur — onglet Design (consolidé après trois passes)

**Base : §1ter.** Puis trois passes de correction, dans cet ordre.

**Passe 1** — ce que Banani avait ajouté de trop ou d'incohérent :

```
1. SUPPRIME entièrement le liseré rouge-orangé qui court le long du bord gauche du
   panneau. Le bord gauche du panneau est neutre, de la même couleur que le fond.
2. SUPPRIME la bordure rouge du champ "X 24" et la bordure verte du champ
   "Y 512". Ces deux champs ont exactement la même apparence que tous les autres
   champs du panneau : fond sombre, fin contour gris. Aucune couleur.
3. Sur la ligne "Cible du cadre", le sélecteur "Mobile — 390 x 844" est en LECTURE
   SEULE : texte gris terne, pas de texte coloré, pas de chevron de menu.
4. Dans la section TYPOGRAPHIE, SUPPRIME la ligne "Style : Regular".
5. Dans la section APPARENCE, SUPPRIME la ligne "Ombre" et son bouton "+ Ajouter",
   la ligne "Type : Arrondi" et la ligne "Coins". Il ne reste que quatre lignes :
   "Fond #4C6FFF", "Bordure 1 px", "Arrondi 8", "Opacité 100 %".
6. Sur la ligne des bornes de la LARGEUR, SUPPRIME le champ "default". Sur celle
   de la HAUTEUR, SUPPRIME le champ "current". Les deux lignes portent alors
   STRICTEMENT les mêmes éléments : "min" puis un champ, "max" puis un champ.
```

**Passe 2** — le bas du panneau et l'alignement :

```
1. Les champs "X 24" et "Y 512" restent strictement neutres. Ne réintroduis AUCUNE
   bordure colorée sur aucun champ du panneau.
2. Dans ALIGNEMENT, une SEULE icône est active par rangée : rangée "H", seule la
   DEUXIÈME (centrer) a le fond bleu ; rangée "V", seule la DEUXIÈME (milieu).
3. LE BAS DU PANNEAU. Supprime "CONTENU" et "INTERACTIONS", qui n'existent pas.
   Les deux dernières sections sont "EFFETS" (repliée, badge rond gris "2") et
   "POINTS DE RUPTURE" (repliée, badge "1" plus un BADGE AMBRÉ "1 jamais
   atteinte"). APPARENCE et TYPOGRAPHIE ne portent AUCUN badge.
4. LA SECTION DISPOSITION EST REFAITE : trois lignes seulement, aucune ligne
   subordonnée de bornes. "Position" (X 24, Y 512) ; "Largeur : expand" avec à
   l'extrémité droite de la MÊME ligne un petit marqueur gris "120–320" ;
   "Hauteur : fixed 44" avec RIEN à sa droite.
   PAR-DESSUS, un POPOVER ouvert ancré sous la ligne "Largeur" : titre gris
   "Largeur", cinq options empilées (fixed, content, fraction, weight, expand —
   expand sur fond bleu avec une coche), un trait de séparation, puis deux champs
   côte à côte "min 120" et "max 320".
```

**Réserves relevées, non corrigées** : la ligne « Cible du cadre » reperd son grisé
de lecture seule d'une passe à l'autre ; le marqueur `120–320` est masqué tant que
le popover est ouvert (normal).

---

### 22.2 Inspecteur — onglet Behavior

```
Génère UNIQUEMENT le panneau Inspecteur d'un éditeur de design d'interface, en
gros plan vertical, thème DARK, occupant toute la hauteur de l'image. Même style
exactement que le panneau précédent.

EN-TÊTE identique : petite icône de bouton, "Bouton_Connexion" en gras, et en
dessous en gris "Rôle : bouton".

LES TROIS ONGLETS : "Design" | "Widget" | "Behavior". CETTE FOIS C'EST "Behavior"
QUI EST ACTIF — texte clair et fin liseré bleu en dessous.

LE CORPS CONTIENT DEUX GROUPES SÉPARÉS, ET RIEN D'AUTRE.

PREMIER GROUPE, titre en petites capitales grises : "ÉVÉNEMENTS DU RÔLE".
Cinq lignes. Chaque ligne porte DE GAUCHE À DROITE : une PASTILLE RONDE de
couleur, le nom de l'événement, puis à l'extrémité droite le nom de la liaison en
gris plus petit.

  - pastille BLEUE — "pressé" — à droite : "OnPressStart"
  - pastille GRISE — "relâché" — à droite : rien, la ligne s'arrête
  - pastille BLEUE — "cliqué" — à droite : "OnSubmitForm"
  - pastille ORANGE — "survol entré" — à droite : "OnHoverIn" et un minuscule
    triangle d'avertissement ambré
  - la cinquième ligne est DIFFÉRENTE : "survol sorti" est BARRÉ et en gris très
    pâle, sa pastille est un cercle en contour non rempli, et à l'extrémité droite
    se trouve une petite icône de flèche circulaire de restauration.

UN ESPACE VERTICAL BIEN VISIBLE sépare les deux groupes, avec un fin trait de
séparation en son milieu.

SECOND GROUPE, titre "ÉVÉNEMENTS AJOUTÉS" :
  - pastille BLEUE — "envoi validé" — à droite : "OnFormValidated"
  - pastille GRISE — "double appui" — à droite : rien

Sous ce second groupe uniquement, un bouton en trait pointillé sur toute la
largeur : "+ Nouvel événement".

Le bas du panneau est vide : pas de section EFFETS, pas de POINTS DE RUPTURE.
```

**Réserve** : l'icône de restauration de la ligne barrée est presque invisible, à
grossir.

---

### 22.3 Menu des rôles — partie haute

```
Génère UNIQUEMENT le HAUT d'un panneau Inspecteur, en gros plan, thème DARK. On ne
voit que l'en-tête, les trois onglets, et un grand menu déroulant ouvert
par-dessus. Le menu dépasse le bas de l'image et porte une BARRE DE DÉFILEMENT
verticale fine le long de son bord droit.

EN-TÊTE : une petite icône carrée de bouton ; à sa droite "Bouton_Connexion" en
gras ; JUSTE EN DESSOUS DU NOM, la ligne de rôle, qui est un CONTRÔLE CLIQUABLE et
non du texte : "Rôle : bouton" suivi d'un petit chevron, sur un fond plus clair
que l'en-tête, coins arrondis, entourée d'un fin contour bleu parce que son menu
est ouvert.

SOUS L'EN-TÊTE, les trois onglets "Design" | "Widget" | "Behavior", "Design"
actif, légèrement assombris car le menu est ouvert par-dessus.

LE MENU DÉROULANT, ancré sous la ligne de rôle, large, coins arrondis, ombre
portée nette :

1. Un champ de recherche avec loupe et texte gris "Rechercher un rôle…", entouré
   d'un CONTOUR BLEU — il est pré-focalisé.
2. Titre "RÉCENTS" en petites capitales grises. Trois lignes avec icône et nom :
   "bouton" (fond bleu, coche à droite), "champ de saisie", "carte cliquable".
3. Un fin trait de séparation.
4. Titre "RÔLES NATIFS" en petites capitales grises. En dessous, SIX SOUS-GROUPES.
   Le nom de chaque sous-groupe est en gris, plus petit, EN RETRAIT vers la droite
   par rapport au titre "RÔLES NATIFS" :

   "Actions" — bouton · bouton à répétition · bouton image · bouton de couleur ·
               élément de menu
   "Saisie" — champ de saisie · champ multiligne · champ entier · champ décimal ·
              curseur · glisseur · sélecteur de couleur
   "Sélection" — case à cocher · case à trois états · liste déroulante · liste ·
                 élément sélectionnable · nœud d'arbre
   "Navigation" — barre d'onglets · barre de menu · menu · menu contextuel ·
                  en-tête repliable · espace d'ancrage
   "Conteneurs" — fenêtre · panneau · groupe · boîte verticale · boîte
                  horizontale · grille · tableau · zone défilante
   "Affichage" — texte · image · barre de progression · courbe · séparateur ·
                 infobulle

   Chaque ligne porte une petite icône à gauche puis son nom. AUCUNE ligne
   "interrupteur", "bouton radio", "accordéon" ou "fenêtre modale" ici.
```

⚠️ **La dernière consigne est normative, pas décorative** : ces quatre rôles ne
sont pas portés par NKGui (§14ter.4 du document humain). Les montrer dans le groupe
natif documenterait une promesse sans code derrière.

**Réserve** : `bouton` apparaît coché dans RÉCENTS et non coché dans Actions. Le
rôle courant doit être marqué **partout où il apparaît**.

---

### 22.4 Menu des rôles — partie basse

```
Génère UNIQUEMENT la PARTIE BASSE d'un menu déroulant de sélection de rôle, comme
si on avait fait défiler ce menu jusqu'en bas. Le menu occupe toute l'image. Thème
DARK, même style que les panneaux précédents. Une BARRE DE DÉFILEMENT verticale
fine longe le bord droit, son curseur POSITIONNÉ TOUT EN BAS de sa course.

1. TOUT EN HAUT DE L'IMAGE, coupées par le bord supérieur, les deux dernières
   lignes du groupe précédent, à moitié visibles : "en-tête repliable" et "espace
   d'ancrage". Elles doivent donner l'impression que le menu continue au-dessus.
2. Un nom de sous-groupe en gris, petit et EN RETRAIT : "Conteneurs". En dessous,
   huit lignes avec icône : "fenêtre", "panneau", "groupe", "boîte verticale",
   "boîte horizontale", "grille", "tableau", "zone défilante".
3. Un nom de sous-groupe en gris, petit et en retrait : "Affichage". En dessous,
   six lignes : "texte", "image", "barre de progression", "courbe", "séparateur",
   "infobulle".
4. Un fin trait de séparation horizontal sur toute la largeur.
5. Un TITRE DE GROUPE en petites capitales grises, PAS en retrait : "RÔLES DE
   PROJET".
6. HUIT lignes. Chacune porte une petite icône, le nom en texte clair, puis À
   L'EXTRÉMITÉ DROITE une petite étiquette en gris plus terne :
   - "interrupteur"      — "dérive de case à cocher"
   - "bouton à bascule"  — "dérive de case à cocher"
   - "accordéon"         — "composition"
   - "carte cliquable"   — "dérive de bouton"
   - "champ recherche"   — "dérive de champ de saisie"
   - "fil d'Ariane"      — "composition"
   - "badge"             — "composition"
   - "carte produit"     — "composition"
7. Un fin trait de séparation.
8. Une dernière ligne, en BLEU, avec un "+" devant : "+ Définir un rôle de
   projet…".

AUCUNE ligne de cette image n'est sélectionnée : pas de fond bleu, pas de coche.
Le rôle courant se trouve plus haut dans le menu, hors de l'image.
```

**Réserve** : environ 130 px de vide sous la dernière ligne — le menu doit
s'arrêter après elle.

---

### 22.5 Hiérarchie — arbre seul

```
Génère UNIQUEMENT le panneau Hiérarchie d'un éditeur de design d'interface, en
gros plan vertical, thème DARK, occupant toute la hauteur de l'image.

EN-TÊTE : le titre "Hiérarchie" à gauche, et à droite deux petites icônes — un
chevron de repliement du panneau, et une icône d'œil barré servant de filtre.

SOUS L'EN-TÊTE : un champ de recherche avec loupe et texte gris "Filtrer…", puis
un petit interrupteur avec le libellé "Afficher seulement les éléments à rôle" —
il est ÉTEINT.

LE CORPS est un ARBRE. Chaque ligne porte DE GAUCHE À DROITE : un chevron de
pliage (si la ligne a des enfants), une petite icône de rôle, le nom, et tout à
droite les icônes d'état. L'indentation marque la profondeur.

  ▾ Page — Connexion                                    [badge gris "6"]
      Fond_Degrade                                      [œil barré affiché]
      Titre_Connexion
    ▾ Groupe_Formulaire
        Champ_Email
        Champ_MotDePasse                                [cadenas fermé affiché]
        Bouton_Connexion    <- SÉLECTIONNÉE : fond bleu sur toute la largeur, et à
                               droite un DISQUE AMBRÉ PLEIN, uni, sans chiffre.
        Lien_MotDePasseOublie

  ▸ Page — Dashboard                                    [badge gris "12"]
        <- REPLIÉE. À droite de son nom, un CERCLE AMBRÉ CREUX — un anneau vide au
           centre duquel est écrit "3" en ambré : l'erreur est PLUS BAS, dans la
           branche repliée.

  ▾ Page — Paramètres                                   [badge gris "4"]
    ▾ Groupe_Onglets
        Onglet_General
        Onglet_Avance
      ◇ Carte_Profil        <- un VRAI LOSANGE en contour (un carré posé sur la
                               pointe, vide au centre) devant son icône : instance
                               d'un composant, avec des surcharges.

TROIS CHOSES DOIVENT ÊTRE IMMÉDIATEMENT LISIBLES :
- le disque ambré PLEIN (l'erreur est ICI) contre l'anneau ambré CREUX portant "3"
  (l'erreur est PLUS BAS) — on doit les distinguer d'un seul coup d'œil ;
- le losange creux de "Carte_Profil" ;
- l'œil barré sur "Fond_Degrade" et le cadenas sur "Champ_MotDePasse", visibles en
  permanence parce qu'ils sont actifs, alors que les autres lignes n'affichent
  aucune de ces deux icônes.

EN BAS DU PANNEAU, une ligne de boutons d'icônes : ajouter une page, dupliquer,
supprimer.
```

---

### 22.6 Hiérarchie — la section basse des composants du projet

À lancer en correction de 22.5.

```
Reprends EXACTEMENT le panneau Hiérarchie précédent. Conserve tout. AJOUTE UNE
SECONDE SECTION EN BAS DU PANNEAU, séparée de l'arbre par une POIGNÉE
HORIZONTALE : un trait épais avec trois petits points au centre, montrant qu'on
peut la tirer pour redimensionner.

Sous cette poignée, un bandeau de section : un chevron pointant vers le bas, le
titre "COMPOSANTS DU PROJET" en petites capitales grises, et le compteur "(7)".

En dessous, SEPT LIGNES portant DE GAUCHE À DROITE : une petite miniature carrée
grise du composant, son nom, puis à l'extrémité droite un petit badge gris rond
avec son nombre d'instances. Certaines lignes portent en plus, juste avant le
badge, un PETIT POINT BLEU signifiant que le composant est utilisé dans une autre
page que la page courante :

   - Bouton_Primaire      [point bleu]  [12]
   - Carte_Produit        [point bleu]  [5]
   - Champ_Recherche                    [3]
   - Bandeau_Alerte                     [1]
   - Entete_Page          [point bleu]  [8]
   - Pied_Page            [point bleu]  [8]
   - Vignette_Utilisateur               [2]

Cette section occupe environ le tiers inférieur du panneau. L'arbre au-dessus est
raccourci d'autant, et sa dernière ligne visible peut être coupée par la poignée —
on doit sentir que l'arbre continue derrière.
```

**Réserves** : l'arbre ne semble pas continuer derrière la poignée (trop de vide
au-dessus) ; et la barre d'actions du bas devient ambiguë — elle agit sur l'arbre
ou sur les composants ? Chaque section doit porter les siennes.

---

### 22.7 Bibliothèque de composants

```
Génère UNIQUEMENT le panneau "Bibliothèque de composants", en gros plan vertical,
thème DARK, occupant toute la hauteur de l'image.

EN-TÊTE : le titre "Bibliothèque" à gauche, une petite icône d'épingle et un
chevron de fermeture à droite.

SOUS L'EN-TÊTE : un champ de recherche avec loupe et texte gris "Rechercher…",
puis une rangée de trois petits onglets-filtres "Tous" (actif) | "Grille" |
"Liste", et une icône de tri.

LE CORPS contient TROIS GROUPES, chacun introduit par un titre en petites
capitales grises suivi d'un compteur entre parenthèses. Chaque groupe présente ses
composants en GRILLE DE VIGNETTES : deux par rangée, chaque vignette est un
rectangle à coins arrondis montrant un aperçu simplifié en gris du composant, avec
son nom en dessous et un petit badge de comptage d'instances en bas à droite.

1. "PROJET (4)" — "Bouton_Primaire" (badge "12"), "Carte_Produit" (badge "5"),
   "Champ_Recherche" (badge "3"), "Bandeau_Alerte" (badge "1").
   La vignette "Carte_Produit" porte en HAUT À GAUCHE une PETITE ICÔNE ÉTINCELLE
   bleue — générée par l'IA, pas encore relue.

2. "IMPORTÉ (3)" — "Menu_Lateral" (badge "2"), "Selecteur_Date" (badge "4"),
   "Grille_Tarifs" (badge "1"). Chaque vignette porte SOUS SON NOM une petite
   ligne grise indiquant sa source et sa version : "studio-lumen · v2.1",
   "openui-kit · v1.4", "studio-lumen · v2.1".
   La vignette "Selecteur_Date" porte EN HAUT À DROITE une PASTILLE AMBRÉE avec
   une petite flèche vers le haut : une mise à jour est disponible. Elle est la
   seule à en porter une.

3. "SYSTÈME (2)" — visuellement plus sobres, avec un léger contour au lieu d'un
   fond plein : "Barre_Outils_Standard" (badge "1") et "Dialogue_Confirmation"
   (badge "6"). Sous chaque nom, une petite ligne grise "NKGui · moteur 0.9".

EN BAS, un bouton en trait pointillé sur toute la largeur : "+ Importer un
composant…".

CE QUI DOIT SAUTER AUX YEUX : on distingue immédiatement un composant de PROJET
(fond plein, pas de ligne de source) d'un composant IMPORTÉ (ligne de source et
version) et d'un composant SYSTÈME (contour au lieu de fond plein).
```

⚠️ **Cette planche est en retard sur la spécification** : §14bis.1 porte désormais
**quatre** provenances — Projet, **Partagé**, Importé, Système — et les composants
de **Projet** ont migré vers la Hiérarchie (§11.6). À relancer avec les groupes
« PARTAGÉ · IMPORTÉ · SYSTÈME » quand on y reviendra.

**Réserve** : le groupe SYSTÈME n'est pas assez sobre visuellement par rapport aux
deux autres.

---

### 22.8 Cible mobile — zone sûre et orientation

**Base :**

```
Génère UNIQUEMENT une zone de canvas d'éditeur d'interface, en gros plan, occupant
toute l'image. Fond gris très pâle, presque blanc, avec un semis de points de
grille discret. Clair, PAS sombre. Un seul cadre au centre, grand, occupant la
majeure partie de la hauteur.

LE CADRE est un rectangle blanc étroit et haut, aux proportions d'un téléphone,
ombre portée légère, COINS FRANCHEMENT ARRONDIS. Étiquette en gris juste au-dessus :
"Connexion — Mobile 390 x 844 · Portrait".

DANS CE CADRE, LA ZONE SÛRE EST DESSINÉE — c'est le sujet de l'image :
- EN HAUT, une bande horizontale sur toute la largeur, en HACHURES DIAGONALES
  bleu-gris translucides, assez haute pour contenir une ENCOCHE : au milieu, une
  forme noire arrondie en pilule qui descend depuis le bord supérieur. La bande
  hachurée l'englobe.
- EN BAS, une bande plus fine, également hachurée, au milieu de laquelle est
  dessiné un petit TRAIT NOIR HORIZONTAL ARRONDI centré.
- SUR LES CÔTÉS, aucune bande.
- Entre les deux bandes, une FINE LIGNE POINTILLÉE bleue délimite la zone sûre,
  avec une minuscule étiquette "zone sûre" posée dessus, en bleu, à gauche.

LE CONTENU, qui doit montrer la différence entre les deux ancrages :
- UNE IMAGE DE FOND : un grand rectangle en dégradé doux, violet vers bleu, qui
  occupe la TOTALITÉ du cadre, D'UN BORD À L'AUTRE — il PASSE SOUS LES HACHURES,
  sous l'encoche et sous l'indicateur d'accueil.
- PAR-DESSUS, à l'intérieur de la zone sûre uniquement : un titre "Connexion" en
  blanc, deux champs de saisie blancs arrondis, et un bouton plein bleu
  "Se connecter", nettement AU-DESSUS de la bande hachurée du bas.
- Deux ÉTIQUETTES FLOTTANTES à l'extérieur du cadre, reliées par un fin trait :
  "ancré au bord du cadre" vers le fond dégradé, "ancré à la zone sûre" vers le
  bouton.

FLOTTANT EN HAUT AU CENTRE, un petit panneau sombre à coins arrondis contenant :
un sélecteur "Mobile ▾" ; un séparateur ; DEUX BOUTONS SEGMENTÉS avec icônes de
téléphone — un téléphone DEBOUT portant "Portrait" (ACTIF, fond bleu) et un
téléphone COUCHÉ portant "Paysage" (terne) ; un séparateur ; un sélecteur
"iPhone 14 Pro ▾".

CE QUI DOIT SAUTER AUX YEUX : le dégradé va vraiment d'un bord à l'autre et passe
sous les hachures, tandis que tout le contenu lisible reste strictement à
l'intérieur de la ligne pointillée bleue.
```

**Correction — les hachures ne sortaient pas :**

```
Reprends EXACTEMENT le canvas précédent. Conserve tout. Ne change que deux choses.

1. LES DEUX BANDES DE ZONE SÛRE DOIVENT DEVENIR TRÈS VISIBLES. Chacune est remplie
   de HACHURES DIAGONALES ÉPAISSES ET BIEN ESPACÉES — traits obliques nets à 45
   degrés, bleu vif, semi-transparents, espacés d'environ 10 pixels. On doit voir
   chaque trait individuellement, comme sur un plan d'architecte.
   - LA BANDE DU HAUT part du bord supérieur du cadre jusqu'à la ligne pointillée,
     sur TOUTE LA LARGEUR. L'encoche noire est POSÉE PAR-DESSUS ces hachures.
   - LA BANDE DU BAS part de la ligne pointillée inférieure jusqu'au bord
     inférieur, sur TOUTE LA LARGEUR. L'indicateur d'accueil est POSÉ PAR-DESSUS.
   - Le dégradé reste visible SOUS les hachures : on le voit à travers, rayé par
     les traits obliques.
2. RAPPROCHE LES DEUX ÉTIQUETTES D'ANNOTATION DU CENTRE pour qu'elles soient
   ENTIÈREMENT VISIBLES, sans être coupées par les bords.
```

**Réserve** : l'étiquette de gauche reste coupée par le bord.

---

### 22.9 Cible bureau — fenêtre en décoration Client

**Base :**

```
Génère UNIQUEMENT une zone de canvas d'éditeur d'interface, en gros plan, occupant
toute l'image. Fond gris très pâle avec un semis de points de grille. Un seul
cadre, LARGE, aux proportions d'un écran d'ordinateur, centré.

ÉTIQUETTE au-dessus : "Editeur — Bureau 1440 x 900 · décoration Client".

LE CADRE est un rectangle blanc à COINS ARRONDIS avec une ombre portée nette :
1. UNE BARRE DE TITRE dessinée par l'application, gris foncé, sur toute la largeur
   du haut : à gauche une petite icône carrée et le texte "Mon Application", à
   droite TROIS BOUTONS DE FENÊTRE dessinés à la main — un trait (réduire), un
   carré (agrandir), une croix (fermer).
2. SOUS ELLE, le contenu : une barre latérale grise à gauche avec quatre lignes de
   menu, et à droite une grande zone claire avec trois cartes rectangulaires et un
   rectangle de graphique.

LES RÉGIONS DE FENÊTRE SONT DESSINÉES EN SURIMPRESSION :
- LES HUIT BORDS DE REDIMENSIONNEMENT : des BANDES VERTES TRANSLUCIDES étroites le
  long des quatre côtés, plus quatre PETITS CARRÉS VERTS aux coins. Sur chaque
  bande, une minuscule icône de curseur de redimensionnement pointant dans la
  bonne direction.

TROIS ÉTIQUETTES FLOTTANTES à l'extérieur du cadre, reliées par un fin trait :
vers la barre de titre : "zone de saisie — sans elle, la fenêtre ne peut pas être
déplacée" ; vers une bande verte : "bord de redimensionnement" ; vers les trois
boutons : "dessinés par l'application, pas par le système".

FLOTTANT EN HAUT AU CENTRE : un sélecteur "Bureau ▾", un séparateur, puis DEUX
BOUTONS SEGMENTÉS "Native" (terne) et "Client" (ACTIF, fond bleu).

EN BAS À GAUCHE, FLOTTANT, un petit panneau sombre intitulé "Curseur" : une flèche
avec le mot "Flèche" et une coche, une main avec "Main", et une ligne
"Personnalisé…" en bleu. Sous un fin trait, deux petits champs côte à côte
étiquetés "point chaud x" et "point chaud y", tous deux GRISÉS.
```

**Correction — la zone de saisie n'était qu'une étiquette :**

```
Reprends EXACTEMENT le canvas précédent. Conserve tout. Ne change QU'UNE chose.

LA ZONE DE SAISIE DOIT DEVENIR UNE SURFACE, PAS UNE ÉTIQUETTE.

Pose un APLAT BLEU VIF TRANSLUCIDE par-dessus la barre de titre. Cet aplat :
- couvre TOUTE LA HAUTEUR de la barre de titre et TOUTE SA LARGEUR, du bord gauche
  du cadre jusqu'aux boutons de fenêtre ;
- S'ARRÊTE NET AVANT LES TROIS BOUTONS, qui restent entièrement découverts, nets,
  sans voile par-dessus ;
- laisse voir au travers l'icône et le texte "Mon Application", qui restent
  lisibles sous le bleu ;
- porte une BORDURE BLEUE CONTINUE de 2 pixels tout autour de son contour, y
  compris le long de la limite verticale où il s'arrête avant les boutons ;
- est traversé de HACHURES DIAGONALES bleues plus soutenues, traits obliques à 45
  degrés espacés d'environ 12 pixels, pour qu'on lise une ZONE et non une teinte.

La petite étiquette "zone de saisie" se pose DANS cet aplat, à gauche, juste après
le texte "Mon Application".

CE QUI DOIT SAUTER AUX YEUX : la barre de titre est presque entièrement recouverte
d'une aire bleue hachurée, et les trois boutons forment un trou net dans cette aire.
```

---

### 22.10 Console de simulation et système simulé

```
Génère UNIQUEMENT un panneau de simulation d'éditeur d'interface, en gros plan,
occupant toute l'image, plus large que haut. Thème DARK, police à chasse fixe pour
les journaux.

EN-TÊTE sur toute la largeur : à gauche un petit CARRÉ VERT et le titre
"Simulation en cours" ; au centre le texte gris "Connexion — Mobile 390 x 844" ; à
droite trois petits boutons : "⟲ Recharger", "⏸ Geler", "⧉ Comparer".

LE CORPS EST DIVISÉ EN DEUX COLONNES par un fin trait vertical.

=== COLONNE DE GAUCHE, environ un tiers ===
Titre "SYSTÈME SIMULÉ". Sept lignes, chacune avec le nom du service puis à
l'extrémité droite un petit sélecteur montrant ce que la doublure rend. DEUX
lignes sont en MODE ÉCHEC : leur sélecteur est AMBRÉ et une minuscule icône
d'avertissement le précède.

  - "Dialogue d'ouverture"      ->  "/projets/rapport.pdf"
  - "Dialogue d'enregistrement" ->  "ANNULÉ"                [ambré]
  - "Système de fichiers"       ->  "lecture OK"
  - "Horloge"                   ->  "2026-08-20 09:00 (figée)"
  - "Réseau"                    ->  "COUPÉ"                 [ambré]
  - "Presse-papiers"            ->  "(vide)"
  - "Locale et clavier"         ->  "fr-FR · AZERTY"

Sous un fin trait, une phrase en gris clair sur deux lignes : "Une doublure rend
aussi l'échec. Le chemin d'annulation est celui que personne ne câble."

=== COLONNE DE DROITE, les deux tiers ===
Titre "POINTS DE CONNEXION" avec à sa droite un badge VERT "7 atteints" et un
badge GRIS "3 jamais atteints".

DEUX SOUS-COLONNES CÔTE À CÔTE, DE MÊME LARGEUR ET DE MÊME POIDS VISUEL :

  Sous-colonne gauche, titre "ATTEINTS" — sept lignes avec PASTILLE VERTE PLEINE,
  nom, et nombre de passages à droite :
     cliqué ×3 · champ non vide ×3 · OnSubmitForm ×3 · survol entré ×8 ·
     survol sorti ×8 · texte modifié ×14 · page ouverte ×1

  Sous-colonne droite, titre "JAMAIS ATTEINTS" — trois lignes avec CERCLE GRIS
  CREUX, en texte plus terne : OnCancel · réseau indisponible · page fermée.
  Elle est ENTOURÉE D'UN FIN LISERÉ et porte sous ses lignes : "Un point jamais
  atteint n'est pas une erreur — il n'a pas été essayé."

=== BANDEAU DE JOURNAL, EN BAS, sur toute la largeur ===
Titre "JOURNAL" à gauche. Cinq lignes à chasse fixe avec horodatage. TROIS
commencent par une petite ÉTIQUETTE AMBRÉE portant le mot "doublure" :

  09:00:02  [doublure] dialogue d'ouverture -> /projets/rapport.pdf
  09:00:02  fichier chargé          -> OnFileLoaded    (atteint ×1)
  09:00:07  [doublure] réseau       -> COUPÉ
  09:00:07  envoi                   -> échec réseau, branche non câblée
  09:00:11  [doublure] enregistrement -> ANNULÉ

CE QUI DOIT SAUTER AUX YEUX : les deux sous-colonnes ont EXACTEMENT le même poids
visuel et la même largeur — "JAMAIS ATTEINTS" n'est pas une note de bas de page,
c'est la moitié du résultat.
```

⚠️ **La dernière consigne a été renforcée après coup** : au premier lancement,
« JAMAIS ATTEINTS » est sortie en petite boîte à droite pendant qu'« ATTEINTS »
occupait une large surface — exactement ce que §18bis.2 interdit. La formulation
ci-dessus intègre la correction ; **elle n'a pas encore été relancée telle quelle.**

---

### 22.11 États d'indisponibilité

```
Génère UNIQUEMENT une zone de canvas d'éditeur d'interface, en gros plan, occupant
toute l'image. Fond gris très pâle avec un semis de points. Un seul cadre, blanc,
aux proportions d'un écran d'ordinateur, centré. Étiquette au-dessus :
"Paramètres — Bureau 1440 x 900".

DANS LE CADRE, un formulaire en une colonne, CINQ BLOCS empilés montrant chacun un
état différent. Chaque bloc porte à sa GAUCHE et À L'EXTÉRIEUR du cadre une
étiquette flottante reliée par un fin trait.

BLOC 1 — NORMAL
"Nom du projet" en gris foncé, et sous lui un champ blanc à bordure nette
contenant le texte noir "Nkentseu". Net et contrasté.
Étiquette : "actif".

BLOC 2 — LECTURE SEULE
"Identifiant" et sous lui un champ dont le FOND EST LÉGÈREMENT GRIS mais dont le
TEXTE "NK-2026-0084" RESTE NOIR ET NET. UNE PARTIE DE CE TEXTE EST SURLIGNÉE EN
BLEU, comme sélectionnée à la souris, et une petite icône de cadenas est posée à
droite dans le champ.
Étiquette : "lecture seule — sélectionnable et copiable".

BLOC 3 — DÉSACTIVÉ AVEC RAISON
"Clé de licence" en gris moyen, et sous lui un champ au fond gris avec le texte
"—". Le champ est TERNE mais son libellé reste PARFAITEMENT LISIBLE.
À DROITE du champ, une INFOBULLE SOMBRE à coins arrondis, avec une petite flèche
pointant vers le champ, portant : "Disponible après activation du compte". Autour
du champ ET de sa zone, un FIN LISERÉ POINTILLÉ délimite une région nettement plus
grande que le champ.
Étiquette : "désactivé — la raison est portée par la région enveloppante".

BLOC 4 — OCCUPÉ
"Synchronisation" et sous lui un rectangle gris clair contenant à gauche un PETIT
CERCLE DE CHARGEMENT (un anneau dont un quart est plus foncé) et à droite le texte
gris "Vérification en cours…".
Étiquette : "occupé — indisponible, mais ça revient".

BLOC 5 — PANNEAU DÉSACTIVÉ PAR HÉRITAGE
Un grand rectangle encadré en bas du cadre, titre "Options avancées" en haut à
gauche. TOUT SON CONTENU EST TERNE : deux cases à cocher grises, un curseur gris,
un bouton gris "Réinitialiser". LE PANNEAU EST RECOUVERT D'UN VOILE GRIS TRÈS
LÉGER, et son coin supérieur droit porte un PETIT BADGE GRIS avec une icône de
chaîne et le texte "hérité". CHACUN de ses quatre enfants porte, dans son propre
coin supérieur droit, une MINUSCULE ICÔNE DE CHAÎNE grise.
Étiquette : "désactivé par héritage — aucun enfant ne peut se réactiver".

CE QUI DOIT SAUTER AUX YEUX :
- le texte du bloc LECTURE SEULE est aussi noir et net que celui du bloc NORMAL,
  et il porte une sélection bleue ;
- les libellés des blocs DÉSACTIVÉ et OCCUPÉ restent PARFAITEMENT LISIBLES — gris,
  mais jamais effacés ni pâlis au point de se deviner ;
- les quatre enfants du panneau hérité portent tous la même petite chaîne.
```

⚠️ **La consigne de lisibilité a été renforcée après coup.** Au premier lancement,
« Clé de licence » et « Réinitialiser » sont sortis à la limite du lisible : la
planche censée illustrer `W-DISABLED-CONTRAST` le violait elle-même. **Preuve que
la règle n'est pas du zèle — même en y pensant, on pâlit trop.**

---

### 22.12 Canvas Behavior — graphe en cours d'exécution

```
Génère UNIQUEMENT le canvas de comportement d'un éditeur d'interface, en gros
plan, occupant toute l'image. Thème DARK. Le fond est SOMBRE et QUADRILLÉ — une
grille fine de carrés, façon Blueprint d'Unreal.

FLOTTANT AU-DESSUS DU CANVAS :
- EN HAUT À GAUCHE, un petit sélecteur sombre : "Portée : Bouton_Connexion ▾".
- EN HAUT À DROITE, deux petits boutons d'icônes collés : un "⟨⟩" pour basculer en
  vue Code, et une icône de loupe.
- CONTRE LE BORD GAUCHE, à mi-hauteur, une barre d'outils verticale étroite de
  48px, coins arrondis, ombre portée, cinq icônes empilées : flèche de sélection
  (active, fond bleu), nœud, câble, commentaire, recadrage.

LE GRAPHE, de GAUCHE À DROITE, en nœuds rectangulaires à coins arrondis. Chaque
nœud a un EN-TÊTE COLORÉ portant son titre, et un corps sombre listant ses entrées
à gauche et ses sorties à droite, chacune précédée d'une petite pastille ronde.

1. À GAUCHE, EN-TÊTE ROSE : "Événement — cliqué". Une seule sortie.
2. AU CENTRE, EN-TÊTE AMBRÉE : "Condition — champ non vide". Une entrée de flux à
   gauche, une entrée de donnée en dessous, DEUX sorties "vrai" et "faux".
3. EN HAUT À DROITE, EN-TÊTE BLEUE : "Action — appeler OnSubmitForm".
4. EN BAS À DROITE, à l'écart, EN-TÊTE BLEUE plus terne : "Action — afficher
   l'erreur".
5. EN BAS À GAUCHE, décalé, EN-TÊTE GRISE : "Valeur — texte du champ".

LES CÂBLES :
- Événement -> Condition, puis "vrai" -> OnSubmitForm : CÂBLES BLANCS ÉPAIS en
  courbe douce, LUMINEUX, portant un HALO BLEUTÉ comme s'ils pulsaient.
- "faux" -> afficher l'erreur : câble BLANC MAIS TERNE, GRISÂTRE, sans halo.
- Valeur -> entrée de donnée de Condition : câble PLUS FIN ET VERT.

L'EXÉCUTION EN COURS est le sujet :
- les trois nœuds du chemin parcouru sont ÉCLAIRÉS, contour bleu lumineux, corps
  légèrement plus clair que le fond ;
- "afficher l'erreur" est TERNE, presque fondu dans le fond ;
- compteur discret dans le coin supérieur droit de chaque nœud : "×3", "×3", "×3",
  et "×0" EN ROUGE PÂLE sur "afficher l'erreur".

EN BAS, sur toute la largeur, un BANDEAU DE CONSOLE séparé par un fin trait, titre
"Console de simulation" à gauche et trois lignes à chasse fixe :
   12:04:31  cliqué          -> OnSubmitForm      (atteint ×3)
   12:04:31  champ non vide  -> vrai
   12:04:29  survol entré    -> non lié
La dernière ligne est en gris plus terne.

CE QUI DOIT SAUTER AUX YEUX : la différence entre le chemin lumineux qui pulse et
la branche "faux" éteinte à "×0".
```

**Réserve** : le bouton `⟨⟩` et la loupe en haut à droite n'ont pas été rendus.

---

### 22.13 Canvas Animation — machine à états

```
Génère UNIQUEMENT le canvas d'animation d'un éditeur d'interface, en gros plan,
occupant toute l'image. Thème DARK. Le fond est sombre avec une GRILLE DE POINTS
espacés — PAS un quadrillage de lignes, pour qu'on le distingue au premier coup
d'œil du canvas de comportement.

FLOTTANT EN HAUT, une barre d'outils sombre à coins arrondis : un sélecteur
"Portée : Ce widget ▾", un séparateur, un bouton "+ Ajouter un état", un
séparateur, DEUX BOUTONS SEGMENTÉS "State Machine" (ACTIF, fond bleu) et "Dope
Sheet" (terne), un séparateur, un bouton "▶ Prévisualiser" avec une icône de
lecture VERTE.

LE GRAPHE D'ÉTATS. Chaque état est une BOÎTE à coins arrondis contenant une petite
VIGNETTE carrée montrant l'apparence du bouton dans cet état, puis le nom en
dessous. Les états sont reliés par des FLÈCHES COURBES.

  - EN HAUT À GAUCHE, un petit ovale plein vert foncé portant seulement "Entry",
    sans vignette.
  - AU CENTRE GAUCHE, "Idle" : vignette d'un bouton bleu ordinaire. Une flèche
    part de "Entry" vers lui.
  - AU CENTRE HAUT, "Hover" : vignette d'un bouton bleu plus clair.
  - AU CENTRE DROITE, "Pressed" : vignette d'un bouton bleu foncé, plus petit.
  - EN BAS À DROITE, "Disabled" : vignette d'un bouton GRIS. CETTE BOÎTE EST
    DIFFÉRENTE : contour en TRAIT POINTILLÉ, et coin supérieur droit portant une
    PETITE ICÔNE DE CADENAS. Sous son nom, une ligne de texte gris CLAIR ET BIEN
    LISIBLE : "piloté par la disponibilité".

LES TRANSITIONS, flèches courbes portant chacune une petite ÉTIQUETTE en leur
milieu, nom de l'événement puis durée :
  Idle -> Hover      "survol entré · 120 ms"
  Hover -> Idle      "survol sorti · 120 ms"
  Hover -> Pressed   "pressé · 60 ms"
  Pressed -> Hover   "relâché · 90 ms"

UNE TRANSITION EST SÉLECTIONNÉE : "Hover -> Pressed" est BLEU VIF et plus épaisse,
son étiquette porte un contour bleu.

DES FLÈCHES VERS "Disabled" partent de "Idle", "Hover" et "Pressed", TOUTES EN
POINTILLÉS GRIS ET TERNES, et SANS AUCUNE étiquette d'événement.

EN BAS À DROITE, FLOTTANT, un petit panneau sombre "TRANSITION SÉLECTIONNÉE" :
   Événement   ->  "pressé"     (sélecteur)
   Durée       ->  "60"  "ms"
   Courbe      ->  "Ease Out"   (sélecteur)
   puis une VIGNETTE DE COURBE : un carré sombre où une courbe blanche part du coin
   inférieur gauche, MONTE VITE, et S'APLATIT NETTEMENT vers le coin supérieur
   droit — un Ease Out, pas une courbe en S. Deux POIGNÉES DE TANGENTE rondes
   reliées par de fins segments.

CE QUI DOIT SAUTER AUX YEUX : l'état "Disabled" ne ressemble pas aux autres —
contour pointillé, cadenas — et ses flèches entrantes sont grises et sans
étiquette, parce que rien dans le widget ne les déclenche.
```

⚠️ **Deux consignes renforcées après coup** : la lisibilité de « piloté par la
disponibilité » (sorti presque illisible) et la forme de la courbe (sortie en S au
lieu d'un Ease Out). **Ces deux ajouts n'ont pas été relancés.**

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
