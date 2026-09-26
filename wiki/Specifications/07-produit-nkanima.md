# NkAnima — ce qu'on attend de l'application

### Document 7 — Spécification produit

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 26/09/2026 à partir de la discussion avec Rodolf du même jour.
> Langue de travail : français.

> **Ce document remplace**, pour le **périmètre** et les **priorités**, les
> documents `design/04`, `05`, `06` (« Aetherion Animate & FX », pensés pour un
> autre cadre, orientés Unreal et jeu seul) et `important/interface.md` /
> `f1.md` (orientés film, mais débordant sur la mise en scène). Ces documents ne
> sont **pas jetés** : ils restent un **réservoir d'idées** de détail. En cas de
> désaccord, **ce document gagne**.
>
> Documents compagnons :
> - **08** — l'interface, expliquée à un humain (quoi, où, pourquoi) ;
> - **09** — l'interface, écrite pour l'agent qui produit les `.nkgui`.

---

## 0. En une phrase

**NkAnima est l'atelier d'animation 3D de Rihen, pour le film et pour le jeu :
tout ce qui bouge — un corps, une créature, un objet qui se brise, le vent, le
sable, l'océan — s'anime à la main, par la physique, par une trajectoire, par une
vidéo, par la parole ou par un agent, et le résultat est toujours le même : des
clés et des courbes que l'artiste peut reprendre.**

---

## 1. Positionnement

NkAnima prend le meilleur de trois familles, sans en copier aucune :

| famille | ce qu'on lui prend |
|---|---|
| **Cascadeur** | la pose assistée (bouger peu de points, le corps suit), la **correction physique** d'une animation à la main (équilibre, trajectoires balistiques), le mouvement secondaire automatique |
| **iClone** | la **vitesse de production** : bibliothèque de mouvements, mocap et vidéo vers mouvement, facial et synchronisation labiale, mélange de clips sur une piste |
| **Blender** | la **précision** : feuille d'exposition, éditeur de courbes, couches non linéaires, contrôle total de chaque canal |

Et ce qu'aucune des trois ne fait ensemble :

- **l'animation par trajectoire** : on trace une courbe dans la vue, les éléments
  sélectionnés la suivent, et peuvent **réagir à la physique** en chemin ;
- **la physique au-delà du corps** : destruction, effondrement, collision de
  véhicules, vent, sable, eau — dans le **même** atelier que le personnage ;
- **l'IA maison** (NKAI), qui produit des **commandes éditables**, jamais un
  résultat figé ;
- **les agents comportementaux** : on donne un rôle et un texte à un agent, il
  joue la scène, et son jeu devient des clés.

**Public** : l'animateur de film, l'animateur de jeu, et l'artiste qui n'est pas
animateur (voir le **mode Facile**, §7).

---

## 2. Frontières — ce qui est dans NkAnima, et ce qui n'y est pas

Décision de Rodolf : **trois applications distinctes qui communiquent par
fichiers**.

```
NK3DModeler ──(maillages, matériaux)──▶ NkAnima ──(personnages animés, clips,
 modélisation, sculpture                squelette, poids,     caches VFX)──▶ NKScena
                                        animation, physique,             mise en scène :
                                        VFX, IA, agents                  caméras, plans,
                                                                         montage, audio,
                                                                         rendu final
                                             │
                                             └──(clips, graphes d'animation)──▶ Nogee / Noge
                                                                                 (jeu)
```

| dans NkAnima | hors de NkAnima |
|---|---|
| **squelette** : création, édition, gabarits | modélisation, sculpture, UV → **NK3DModeler** |
| **poids de peau** (skinning) : automatiques et peints | caméras de plan, découpage, montage → **NKScena** |
| **animation** : toutes les méthodes du §5 | audio de production, mixage → **NKScena** |
| **facial** : expressions, synchronisation labiale | rendu final, compositing → **NKScena** |
| **physique** : corps, objets, destruction, véhicules, tissu | logique de gameplay → **Nogee** |
| **VFX et phénomènes** : particules, vent, sable, fumée, feu, eau, océan | |
| **IA** et **agents comportementaux** | |
| **prévisualisation** (vue en temps réel, lecture, capture d'aperçu) | |

📌 **NkAnima a une vue, pas un moteur de rendu de production.** L'aperçu doit
être fidèle, mais l'image finale d'un film sort de NKScena.

📌 **Une piste audio de référence** (un dialogue à synchroniser, une musique pour
caler un rythme) **est** dans NkAnima : c'est un outil d'animation, pas de
production sonore.

---

## 3. Ce que l'on anime — « la totale »

Tout sujet appartient à une des six familles. Chaque famille a ses outils, mais
**toutes aboutissent à la même timeline**.

| famille | exemples | ce qui la pilote |
|---|---|---|
| **A. Corps à squelette** | humain, enfant, quadrupède, oiseau, poisson, serpent, insecte, créature à membres multiples | os, IK/FK, contraintes, facial |
| **B. Objets rigides et mécaniques** | caisse, porte, arme, véhicule, machine articulée | transformations, charnières, moteurs, suspension |
| **C. Destructibles** | mur qui se fissure, bâtiment qui s'effondre, verre, rocher, montagne qui s'écroule | fracture (pré-découpée ou à l'impact), liaisons qui cèdent |
| **D. Déformables** | tissu, vêtement, cheveux, fourrure, chair, corde | simulation masse-ressort / contraintes |
| **E. Phénomènes** | vent, sable, poussière, fumée, feu, pluie, eau, océan, vagues | champs de forces, particules, fluides |
| **F. Foules et agents** | passants, armée, troupeau, banc de poissons, personnage autonome | comportements, IA |

Les familles **interagissent** : le vent soulève le sable **et** fait flotter le
vêtement ; la voiture percute le mur **et** le mur se brise **et** la poussière
se lève ; le personnage marche dans l'eau **et** l'eau réagit.

---

## 4. Squelette et poids — faits dans NkAnima

### 4.1 Le squelette

- **Gabarits** prêts : bipède humain, quadrupède, oiseau, poisson, serpent,
  insecte, et **vide**. Un gabarit se **pose** sur le maillage importé puis
  s'ajuste.
- **Placement assisté** : l'IA propose l'emplacement des articulations à partir
  du maillage (proposition éditable, jamais imposée — §6).
- **Édition** : ajouter, supprimer, renommer, reparenter un os ; symétrie
  gauche/droite ; orientation des axes.
- **Contraintes** : IK à deux os, chaîne IK, IK de colonne (spline), visée,
  copie de rotation, limites d'angle.
- **Contrôleurs** : formes de manipulation posées sur les os (anneaux, flèches),
  pour que l'animateur manipule des **contrôles**, pas des os nus.
- **Facial** : unités d'action (existant : `NkActionUnit`, `NkFaceController`)
  et formes de mélange (blendshapes) venant du maillage.

### 4.2 Les poids de peau

- **Automatiques** par défaut, calculés dès que le squelette est lié.
- **Peinture** : pinceau ajouter / retirer / lisser / égaliser, avec
  **carte de chaleur** (bleu = 0, rouge = 1) de l'os actif.
- **Contrôle** : la somme des poids d'un sommet est affichée ; une somme
  différente de 1 est **signalée**.
- **Épreuve** : bouton « tester la déformation », qui joue une amplitude de
  mouvements sur les os et révèle les zones qui s'écrasent.

---

## 5. Les façons d'animer

**Principe fondateur : toutes les méthodes écrivent dans le même modèle —
des clés sur des canaux, et des courbes entre les clés.** Une animation faite
par la physique, par une vidéo ou par un agent **s'ouvre dans l'éditeur de
courbes comme une animation faite à la main.** Rien n'est une boîte noire.

Chaque canal (un os, une propriété) sait **d'où viennent ses clés** : main,
physique, trajectoire, vidéo, IA, agent. L'interface le montre (document 08).

### M1 — Clés à la main

Pose à pose, ou à la volée. Insérer, supprimer, copier, coller, décaler des clés.
Feuille d'exposition (vue des clés), éditeur de courbes (tangentes, interpolation
constante / linéaire / Bézier), peaux d'oignon.

### M2 — Pose assistée (façon Cascadeur)

L'animateur déplace **quelques points de contrôle** (mains, pieds, bassin, tête) ;
le reste du corps **suit de façon naturelle** en respectant les limites des
articulations et l'équilibre. Existant : `NkAutoPose`, `NkPoseBalancer`,
`NkPoseMass`. Le résultat est une pose ordinaire, clé par clé.

### M3 — Animation par trajectoire ⭐

La signature de NkAnima.

1. L'artiste **trace une courbe** dans la vue (à main levée ou point par point).
2. Il sélectionne un ou plusieurs éléments et les **attache** à la courbe.
3. Les éléments **la suivent** : position le long de la courbe, **orientation**
   (tangente, fixe, vers une cible), **vitesse** réglable le long du tracé
   (courbe de vitesse), **décalage** entre éléments (le troupeau ne se superpose
   pas, les wagons se suivent).
4. En chemin, chaque élément peut **réagir à la physique** : un obstacle le
   dévie, la gravité l'infléchit, un choc le décroche de la courbe. On choisit
   **qui gagne** : la courbe (réaction légère, retour sur le tracé) ou la
   physique (la courbe n'est qu'une intention).
5. Pour un corps à squelette, la trajectoire pilote le **déplacement**, et la
   locomotion (M5) produit les pas qui vont avec.

Existant : `NkMotionPath` (NKAnima).

### M4 — Physique

Trois usages distincts, qu'il ne faut pas confondre :

| usage | ce qu'il fait | exemple |
|---|---|---|
| **Correction** | reprend une animation à la main et la rend physiquement plausible, en touchant le moins possible | un saut dont la trajectoire était fausse devient balistique ; un personnage qui penchait retrouve son équilibre |
| **Simulation active** | le corps est simulé, mais **cherche** à tenir une pose ou à suivre l'animation, avec une force réglable | un personnage frappé encaisse le coup puis se rattrape |
| **Simulation passive** | tout est laissé à la physique | ragdoll, chute, effondrement, collision de véhicules |

**Le dosage se règle** : par os, par objet, et **dans le temps** (une courbe
« part de physique » de 0 à 100 %). Le résultat est **cuit** en clés.

📌 **Déterminisme** : pour une même scène et les mêmes réglages, la simulation
cuite donne **le même résultat**. Sans ça, un plan de film validé ne se refait
pas.

Existant : `NKPhysics` — corps rigides, liaisons, ragdoll, tissu, vêtement,
véhicule, mannequin ; `NkBalance`, `NkContactDetector`, l'IK des pieds qui reçoit
le monde physique.

### M5 — Actions et locomotion procédurales

Générateurs paramétrés, conscients du terrain :

**marcher, courir, sauter, grimper, nager, ramper, voler, tomber, se relever**,
et **custom** (l'artiste enregistre un générateur à partir de ses propres clips).

Chaque générateur a des **paramètres lisibles** (vitesse, amplitude, poids du
personnage, fatigue, humeur) et **s'adapte** : pente, escalier, prise de
grimpe, profondeur de l'eau. Il se pose sur une trajectoire (M3), ou en
place. Les pieds et les mains **tiennent** au contact (existant : l'IK des pieds
et la pose de référence, en cours).

### M6 — Depuis une vidéo, et depuis la mocap

- **Vidéo → mouvement** : une vidéo (fichier ou caméra, existant : `NKCamera`)
  donne un mouvement de corps, de mains et de visage. Le résultat passe par la
  **correction physique** (M4) pour retirer les glissements de pieds et les
  tremblements.
- **Import de mocap** : formats standards (au minimum **BVH** et **FBX**), avec
  nettoyage (filtrage, verrouillage des pieds, réduction de clés).

### M7 — Texte → mouvement (IA)

« Il se lève péniblement, regarde à gauche, puis court vers la porte. » L'IA
propose une animation, **en aperçu**, que l'artiste accepte, rejette ou retouche
(§6).

### M8 — Bibliothèque, couches, reciblage

- **Bibliothèque** de mouvements, d'expressions et d'effets, avec recherche et
  aperçu animé.
- **Couches non linéaires** : un clip de base + des couches additives
  (respiration, correction de main), mélange et transitions.
- **Reciblage** d'un squelette vers un autre (existant : `NkAnimRetarget`),
  y compris entre proportions différentes.

### M9 — Agents comportementaux

On donne à un agent **un rôle** (« un vieux pêcheur méfiant ») et **un texte**
(dialogue, didascalies), et il **joue la scène** : déplacements, gestes,
regards, expressions, synchronisation labiale. Plusieurs agents peuvent jouer
**ensemble**. Le jeu est enregistré puis **cuit en clés** : on le reprend comme
n'importe quelle animation.

Pour le jeu vidéo, le même agent peut être **exporté comme comportement** et
jouer en temps réel dans Noge (voir §9).

### M10 — Animation facile

Voir §7. Ce n'est pas une méthode de plus : c'est une **façon de présenter**
toutes les autres à quelqu'un qui n'est pas animateur.

---

## 6. L'IA dans NkAnima — les règles

Mêmes règles que dans NK3DModeler, parce que ce sont les mêmes raisons.

| règle | pourquoi |
|---|---|
| **L'IA produit des commandes**, jamais un résultat figé : des clés, des poses, des contraintes, des réglages | une commande est annulable, rejouable, éditable ; un résultat figé ne l'est pas |
| **L'IA n'emploie que des commandes qui existent** pour l'humain | sinon l'IA, le rejeu et l'humain divergent en silence |
| **Aperçu avant application** : Aperçu / Appliquer / Rejeter | sinon chaque essai est une annulation |
| **Ce que l'IA a fait reste marqué** (origine des clés, §5) | l'artiste sait ce qu'il a validé et ce qu'il n'a pas regardé |
| **Retouche dans les deux sens** : l'humain retouche l'IA, l'IA reprend à partir de la retouche | c'est la demande de Rodolf : « l'homme peut modifier le travail de l'IA et vice versa » |

### 6.1 Ce que l'IA sait faire

| capacité | entrée | sortie |
|---|---|---|
| placement du squelette | un maillage | un squelette proposé |
| poids automatiques | maillage + squelette | des poids |
| intervalles | deux poses ou plus | les poses entre (façon Cascadeur) |
| correction | une animation | la même, plausible |
| texte → mouvement | une phrase | un clip |
| vidéo → mouvement | une vidéo | un clip |
| voix → visage | un audio de dialogue | synchronisation labiale et expression |
| style | un clip + « plus lourd », « plus nerveux », « comme un enfant » | le clip modifié |
| agent | rôle + texte | une scène jouée |

### 6.2 Nos propres modèles

Les modèles sont ceux de **NKAI** (framework IA maison), exécutés **en local**.
Deux voies, choisies **au cas par cas** :

- **Entraîner à partir de modèles existants** quand leur licence le permet ;
- **Entraîner depuis zéro** sur des données que Rihen a le droit d'employer.

⚠️ **La licence voyage avec les poids.** Beaucoup de bases de mocap publiques sont
réservées à la recherche. Comme pour GenIA 3D, chaque modèle porte sa
**frontière de licence** (R&D interne / produit diffusé) dans son manifeste, et
un modèle entraîné sur des données « recherche seulement » **ne sort pas** dans le
produit.

📌 **Les données d'entraînement viendront en partie de NkAnima lui-même** : la
correction physique (M4) et les générateurs (M5) produisent du mouvement
plausible **en volume**. Ce n'est pas un détail : c'est ce qui rend l'entraînement
depuis zéro réaliste.

---

## 7. Le mode Facile

Pour l'artiste qui n'est pas animateur, pour l'étudiant, pour aller vite.

**Principe : on ne montre pas moins de puissance, on montre moins de
vocabulaire.** Le mode Facile commande **les mêmes méthodes** (§5) ; il cache les
canaux, les tangentes et les réglages fins, et les remplace par des gestes et
des phrases.

### 7.1 Ce que voit l'artiste

1. **Choisir un personnage** (ou un objet) dans la scène — une grande carte.
2. **Dire ce qu'il fait**, de trois façons au choix :
   - **cliquer une action** dans une grille illustrée (marcher, courir, sauter,
     s'asseoir, saluer, tomber…) ;
   - **écrire une phrase** (M7) ;
   - **tracer un chemin** dans la vue (M3) — le personnage le suit en marchant.
3. **Régler avec des curseurs parlants** : vitesse, énergie, poids, humeur
   (triste ↔ joyeux), réalisme (cartoon ↔ physique).
4. **Voir le résultat** tout de suite, en boucle.
5. **Enchaîner** : les actions se posent l'une après l'autre sur une **bande de
   séquence** en gros blocs (pas de clés visibles), qu'on déplace et
   raccourcit à la souris.

### 7.2 Ce que le mode Facile fait à la place de l'artiste

- choisit les transitions entre deux actions ;
- tient les pieds au sol, évite que le personnage traverse les objets ;
- applique la correction physique si « réalisme » est au-dessus de la moitié ;
- ajoute le mouvement secondaire (respiration, balancement).

### 7.3 Le passage vers le mode Expert

Un bouton **« Ouvrir en détail »** bascule en mode Expert **sur la même
animation** : les blocs deviennent des clips, les clips s'ouvrent en clés et en
courbes. **Aucune perte**, et **retour possible** vers le mode Facile tant qu'on
n'a pas modifié les clés à la main (au-delà, les blocs concernés sont marqués
« retouchés à la main » et le mode Facile les respecte sans les régénérer).

---

## 8. VFX et phénomènes

| phénomène | approche | film | jeu |
|---|---|---|---|
| **particules** (étincelles, pluie, poussière, magie) | émetteurs + forces + collisions | cuit en cache | temps réel |
| **vent** | **champ de forces** qui agit sur **tout** : tissu, cheveux, végétation, sable, particules, fumée | ✅ | ✅ |
| **sable, gravier, neige** | granulaire (particules à friction) | cuit | approximé |
| **fumée, feu** | fluide sur grille | cuit | approximé (sprites / volumes simples) |
| **eau, océan, vagues** | océan procédural (spectre de vagues) + fluide local pour les éclaboussures | cuit | océan procédural temps réel |
| **destruction** | fracture pré-découpée ou à l'impact, liaisons qui cèdent, débris | cuit | pré-découpée |
| **effondrement** (bâtiment, montagne) | destruction + granulaire + poussière, enchaînés | cuit | cuit puis rejoué |
| **collision de véhicules** | `NkVehicle` + déformation + destruction | cuit | temps réel |

**Tout phénomène se déclenche et se règle dans la timeline**, comme une
animation : le mur cède à l'image 120, le vent forcit de l'image 200 à 260.

⚠️ **État réel** : le module `NKVFX` ne contient qu'un générateur de maillage
d'eau et un témoin (≈ 400 lignes), **mais le socle VFX vit dans NKRenderer** :
`NkVFXSystem`, `NkParticleStore` / `NkParticleStoreGPU` (particules CPU et GPU),
`NkSPHSolver` (fluide à particules), `NkFluidGrid` + `NkFluidGridRaymarch`
(fluide sur grille, rendu volumétrique), `NkForceField` (champs de forces — la
base du vent), `NkSplashEmitter`, `NkTerrainSable`. Ce qui manque est surtout
l'**édition** (pile d'effets, déclencheurs, cuisson en cache) et la
**destruction**. Le phasage (§10) en tient compte.

---

## 9. Film et jeu — ce que NkAnima livre

| | film (→ NKScena) | jeu (→ Nogee / Noge) |
|---|---|---|
| **animation** | clips denses, toutes les clés, pour **un** plan | clips **en boucle**, déplacement de la racine (root motion), compressés |
| **physique** | cuite, figée, déterministe | partie cuite, partie **temps réel** (ragdoll, destruction à l'impact) |
| **VFX** | caches cuits | émetteurs et réglages temps réel |
| **facial** | clés complètes | unités d'action + synchronisation labiale en temps réel |
| **agents** | jeu cuit en clés | **comportement exporté**, joué en temps réel |
| **graphes d'animation** | — | machines à états et espaces de mélange (voir décision ouverte D2) |

**Formats** : les extensions sont à fixer (décision ouverte D1). Proposition :
`.nkrig` (squelette + poids + contrôleurs), `.nkanim` (clip), `.nkfx` (effet),
`.nkanimproj` (projet). Import et export **FBX** et **glTF** au minimum, **BVH**
en import.

---

## 10. Phasage

**Une tranche qui marche > un scaffold de plus.** Chaque palier se termine par
un **critère mesurable**, pas par « c'est codé ».

| palier | contenu | critère de fin |
|---|---|---|
| **P0 — aujourd'hui** | coquille pilotée par `.nkgui`, lecture de clips, IK des pieds, ragdoll, compteurs | (acquis) |
| **P1 — l'atelier de base** | import d'un maillage NK3DModeler ; squelette par gabarit ; poids automatiques + peinture ; clés, feuille d'exposition, courbes ; pose assistée (M2) ; export `.nkanim` / FBX | un personnage importé est rigué, animé à la main sur 5 secondes, exporté, et relu identique |
| **P2 — la physique et la trajectoire** | correction physique (M4), simulation active et passive, dosage dans le temps ; trajectoire (M3) avec réaction physique ; objets rigides et véhicules | un saut à la main devient balistique sans bouger les appuis ; trois objets suivent une courbe et un obstacle en dévie un |
| **P3 — la production rapide** | locomotion et actions (M5) ; mocap BVH/FBX ; vidéo → mouvement (M6) ; bibliothèque, couches, reciblage (M8) ; facial et synchronisation labiale ; **mode Facile** (§7) | un non-animateur produit une scène de 20 s (marcher, s'asseoir, saluer) en mode Facile, puis l'ouvre en détail sans perte |
| **P4 — les phénomènes** | vent, particules, destruction, effondrement, tissu et cheveux dans la timeline | une voiture percute un mur qui se brise, la poussière se lève et le vent l'emporte — cuit, déterministe |
| **P5 — l'IA générative** | texte → mouvement (M7), intervalles IA, style, voix → visage | une phrase produit un clip en aperçu, accepté, retouché, et l'IA reprend à partir de la retouche |
| **P6 — fluides et agents** | fumée, feu, eau, océan ; agents comportementaux (M9) et foules | deux agents jouent un dialogue de 30 s à partir de leurs rôles et du texte ; le jeu est cuit et retouchable |

📌 La **2D** vient **après** P6 (décision de Rodolf : « principalement orienté 3D,
on verra la 2D plus tard »).

---

## 11. Règles transverses

1. **Toute action est une commande** : annulable, rejouable, scriptable,
   sérialisable. L'interface, le script, le rejeu et l'IA passent **par la même
   porte**.
2. **Rien n'est une boîte noire** : tout résultat (physique, IA, agent, vidéo)
   devient des clés que l'on peut ouvrir.
3. **L'origine des clés est tenue** et visible.
4. **La simulation cuite est déterministe.**
5. **Fermer un onglet ne supprime jamais rien** (règle de Rodolf pour le
   modeleur, valable ici).
6. **Zéro-STL, mémoire par NKMemory**, conventions du dépôt.
7. **L'interface vient des documents `.nkgui`** : structure, comportement et
   design (décision du 26/09).
8. **Le style est celui de l'éditeur Unreal Engine 5, net** : gris neutres,
   densité, panneau Détails à catégories, couleur réservée à l'information
   (document 08 §1). Thème Sombre par défaut, Clair disponible.

---

## 12. Décisions ouvertes

| # | question | proposition |
|---|---|---|
| D1 | extensions de fichiers | `.nkrig`, `.nkanim`, `.nkfx`, `.nkanimproj` |
| D2 | les **graphes d'animation de jeu** (machines à états, espaces de mélange) se créent-ils dans NkAnima ou dans Nogee ? | dans NkAnima (c'est de l'animation), **joués** par Noge |
| D3 | les **foules** : NkAnima ou NKScena ? | les comportements dans NkAnima ; leur **placement** dans un plan dans NKScena |
| D4 | la **végétation** soumise au vent : animée dans NkAnima ou décor de NKScena ? | la réponse au vent se règle dans NkAnima, le décor se place dans NKScena |
| D5 | premier modèle IA à entraîner | les **intervalles** (M2/IA) : données produites en volume par NkAnima lui-même, gain immédiat pour l'animateur |
