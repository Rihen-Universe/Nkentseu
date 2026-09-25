# Les démos — ce qu'on peut LANCER et VOIR

> **Règle permanente (Rodolf, 25/09/2026)**
> « en branchant, si tu peux, crée des démos que je peux tester, après je serais content. »
>
> **Aucun câblage n'est déclaré fini tant que Rodolf ne peut pas le lancer.** Un banc n'éprouve que
> son maillon ; une démo qu'un humain lance éprouve **la chaîne entière**.

**Ce qu'une démo doit être :**

1. un exécutable qui s'ouvre et **montre** — sans variable d'environnement, sans argument
   obligatoire, sans fichier à préparer ;
2. **nommée par ce qu'elle montre**, jamais par un numéro (*un indice n'est pas un nom* — `--demo=2`
   s'est déjà décalé d'un cran à chaque fusion, et deux bancs ont visé le même indice le 07/09) ;
3. inscrite ici : **le nom, ce qu'on doit voir, et ce qui prouverait que c'est cassé** ;
4. elle montre le **câblage**, pas le module ;
5. ⚠️ **jamais poussée sur GitHub.**

---

## Facial — « la face bouge quand on tire les curseurs »

**Ce que ça montre, et c'est un câblage, pas un module :** trois coefficients d'*Action Unit* → le
calcul (`NKAnima/Face/NkFaceController`) → des poids de blendshapes → la déformation d'un maillage.
Le calcul vivait dans `Applications/PV3DE/` jusqu'au 25/09 : une **feuille** du graphe de
dépendances, donc personne d'autre ne pouvait en dépendre. Il est descendu dans **NKAnima**, et
cette démo le prouve depuis une application **qui n'est pas PV3DE**, sans copier une ligne de PV3DE.

### Comment la lancer

1. lancer `NK3DModeler` ;
2. **Ajouter** un objet (un cube suffit), puis **TAB** pour entrer en Édition ;
3. panneau **Propriétés** (à droite), faire défiler jusqu'au groupe **« Facial (éprouvette) »**,
   le déplier ;
4. cliquer **« Activer l'éprouvette »** ;
5. tirer les trois curseurs : **AU1 sourcil**, **AU26 mâchoire**, **AU12 sourire**.

### Ce qu'on doit voir

- la forme **se déforme** pendant qu'on tire — le haut se soulève, le bas descend, le milieu
  s'élargit ;
- la ligne **« déplacement max »** monte avec les curseurs ;
- en cliquant **« Liaison BRANCHÉE »**, le bouton devient **rouge « Liaison COUPÉE »** : la forme se
  **fige**, et **les curseurs continuent de bouger les poids**. C'est le cœur de la démo — elle
  montre à l'œil que la déformation vient de ce fil-là et de nulle part ailleurs.

### Ce qui prouverait que c'est cassé

| symptôme | ce que ça veut dire |
|---|---|
| on tire les curseurs et **rien ne bouge** | le fil est coupé quelque part entre le calcul et la déformation |
| on coupe la liaison et **la forme bouge encore** | ⚠️ **le plus grave** : un **autre** chemin déforme le maillage, et la liaison mesurée n'est pas celle qu'on croit — *une garde verte grâce au défaut* |
| « déplacement max » reste à **0** alors que la forme bouge | l'instrument ment, pas le produit |
| « déplacement max » **monte** alors que rien ne bouge à l'écran | la déformation n'est pas poussée au GPU (`UpdateVertices` non atteint, ou maillage non 1:1) |

### ⚠️ Ce que cette démo n'est PAS

**Ce n'est pas une tête.** Il n'existe **aucun maillage à blendshapes** dans le dépôt — les quatre
`.glb` et tous les `.gltf` de `Resources/` ont été vérifiés : **zéro morph target**. Les trois cibles
sont **calculées** sur le maillage ouvert, quel qu'il soit, à partir de la hauteur de ses sommets.
C'est du **matériel d'épreuve**, et le panneau le dit à l'écran.

> Le dépôt a déjà payé *une sonde qui se fait passer pour le produit* : Rodolf a photographié la
> fenêtre d'un agent en croyant voir son application. Ici **rien n'est écrit sur le disque** — il n'y
> a aucun fichier qu'on puisse prendre pour un actif.

**CONDITION DE RETRAIT :** le jour où une vraie tête à blendshapes entre dans les ressources,
l'éprouvette part et la démo charge ses cibles au lieu de les calculer. *Un contournement dit quand
le supprimer.*

**Question ouverte pour Rodolf :** importer une vraie tête à blendshapes — **laquelle, et sous quelle
licence**. C'est un choix d'actif et de droits, pas un choix technique. L'éprouvette ne le remplace
pas : elle permet seulement de ne pas l'attendre.

### Pour les bancs (sans souris)

`NK_FACE_DEMO="0.8:0.8:0.8"` arme l'éprouvette et pose les trois coefficients ; `NK_FACE_COUPE=1`
coupe la liaison. Les deux passent par **les mêmes façades que les boutons du panneau** — une sonde
qui poserait l'état directement mesurerait un chemin que Rodolf n'emprunte jamais.

Mesure de référence (cube subdivisé, 486 sommets, coefficients à 0,8) :

```
liaison=branchee : dmax=0.24   nonNuls=486/486
liaison=COUPEE   : dmax=0      nonNuls=0
```


---

## Messages en vue — « le module qui journalise ne connaît pas l'écran »

**`Build/Bin/Release-Windows/NkMessagesEnVue/NkMessagesEnVue.exe`**

**Ce que ça montre, et c'est un câblage, pas un module :** `logger.Warn()` /
`logger.Error()` → **NKLogger** → le **neuvième puits**
(`NKEditorKit/NkScreenLogSink.h`) → la boucle d'affichage → `NkEcranLogVue`.
Chaque bouton de la fenêtre appelle `logger.*` **et rien d'autre** : aucun ne
dessine, aucun ne connaît l'écran, aucun ne choisit une couleur. C'est tout le
sujet — le rendu, l'import, la physique, et **demain les scripts et le nodal**
sont déjà branchés sans une ligne à écrire chez eux.

### Ce qu'on doit voir

- **« Emettre un AVERTISSEMENT »** → une pastille **ambre** dans le cadre du
  bas, qui s'efface au bout d'une douzaine de secondes ;
- **« Emettre une ERREUR »** → une pastille **rouge**, qui reste plus longtemps
  que l'avertissement ;
- **« Douze fois la MEME erreur »** → **UNE** pastille portant **« x 12 »** ;
- **« Emettre une INFORMATION »** → **rien à l'écran**, et c'est le
  comportement attendu : le puits filtre à AVERTISSEMENT. La ligne part quand
  même dans la console et dans `logs/app.log`.

### Ce qui prouverait que c'est cassé

| symptôme | ce que ça veut dire |
|---|---|
| **après avoir pressé « DEBRANCHER le puits », un message apparaît encore** | ⚠️ **le plus grave** : un **autre** chemin le peint, et tout le reste de la démo ne prouve rien. C'est le bouton le plus important de la fenêtre. |
| une pastille **bleue** après « Emettre une INFORMATION » | le filtre de niveau du puits ne tient pas — l'écran devient la console |
| **douze** pastilles identiques au lieu d'une | la fusion des répétitions ne marche pas ; une boucle de rendu qui refuse remplira l'écran |
| l'erreur disparaît **avant** l'avertissement | la durée ne suit pas le niveau |

La démo emploie `NkEcranLogVue`, **la vue du kit** — pas la pile de bandeaux de
NK3DModeler. Elle prouve donc en même temps que l'affichage marche chez un hôte
qui n'a aucune pile à lui : ce sera le cas de **Nogee, NkAnimaEditor et
NKScena**.

---

## Dans NK3DModeler — deux gestes, pas de démo à part

| quoi faire | ce qu'on doit voir | ce qui prouverait que c'est cassé |
|---|---|---|
| **Les messages en vue.** Provoquer un refus d'import : importer un modèle **sans avoir ouvert de scène**. | Le bandeau apparaît **dans la vue**, une seule fois, en rouge, et il ne s'efface pas tout seul. | • Le refus n'apparaît que dans la console ou `logs/app.log`.<br>• Il apparaît **deux fois**, en rouge **et** en ambre → la marque « je viens du journal » ne tient plus (`NkImportNote` pose le bandeau *et* journalise le même texte).<br>• Il **disparaît** au bout de douze secondes → la durée est relue sur l'écho au lieu du bandeau existant. |
| **Le navigateur de contenu.** L'ouvrir (panneau du bas), **tirer la séparation** entre l'arbre des dossiers (gauche) et la grille des vignettes (droite). Puis **fermer l'application et la rouvrir**. | Le curseur devient une **double flèche** au survol du trait, le trait **s'éclaire**, la séparation suit la souris — et la largeur est **encore là** après réouverture. | • Le curseur ne change pas → la poignée n'est pas déclarée, ou une autre zone lui vole le clic.<br>• L'arbre **ou** la grille disparaît quand on tire à fond → une borne manque.<br>• La largeur revient à celle d'origine après réouverture → `~/.nk3dmodeler_ui.cfg` n'est pas écrit. Il l'est **au relâchement** de la poignée, jamais à la sortie du programme : une application fermée par la croix de l'OS n'écrirait rien. |

| **Les compteurs de rendu.** Menu **Fenêtre → Compteurs de rendu**. | Un petit panneau apparaît **en haut à droite de la vue 3D** : Draw, Tris, Sommets, Lots, Écartés, Lumières, Ombreurs, GPU, CPU, dt, FPS — et le nom du dorsal en titre (« Vulkan », « OpenGL »…). L'entrée de menu porte une **coche**. Refermer et rouvrir l'application les retrouve **allumés**. | • Ils sont **allumés au premier lancement** → le défaut n'est plus « éteint », et c'est ce qui polluait chaque capture.<br>• **« GPU 0.00 ms »** alors qu'aucune mesure GPU n'a encore répondu → un zéro qui n'est pas un zéro ; la vue doit écrire **« -- »**.<br>• Les chiffres restent **figés** pendant qu'on tourne la caméra → ils ne viennent plus de `NkRenderer::GetStats()`.<br>• Ils apparaissent **sur l'écran d'accueil** → la garde `!st.welcome` est tombée.<br>• Un clic dessus **tourne la caméra** → la zone n'est pas réclamée, et l'affichage laisse passer. |

⚠️ **Les compteurs n'ont pas de démo à part, et c'est délibéré :** leur sujet est
de **relayer** les chiffres d'un vrai rendu. Une démo qui les alimenterait avec
des nombres inventés montrerait un relais de rien du tout — elle aurait l'air de
marcher quel que soit l'état du câblage. NK3DModeler est le seul endroit où le
geste prouve quelque chose.

| **La barre de menus.** Cliquer **Fichier**, puis **Fenêtre**, puis **Fichier** de nouveau. | Chaque menu **se déroule et reste ouvert** ; un second clic sur un autre titre y passe ; un second clic sur le même le referme. | • Rien ne se déroule → le défaut du 25/09 est revenu : le même clic est lu deux fois dans l'image, et la seconde lecture referme ce que la première vient d'ouvrir.<br>• Le menu clignote → même cause, à une image près. |
| **L'aide aux raccourcis.** Menu **Fenêtre → Aide aux raccourcis** (cochée par défaut). | Les deux bandeaux « OBJET \| G/R/S=… » et « clic=sel Shift+clic=multi… » disparaissent **et rien d'autre** — la barre d'outils du viseur, le gizmo d'axes et les boutons latéraux restent. | • D'autres éléments disparaissent avec eux → la garde couvre plus que l'aide, et c'est le défaut de garde unique qu'on vient de défaire.<br>• Ils reviennent après redémarrage alors qu'on les avait éteints → `aide=` n'est pas relu, ou n'est pas poussé au viseur. |
| **Les panneaux.** Menu **Fenêtre → Hiérarchie / Propriétés / Navigateur de contenu / Plein écran**. | Chaque entrée replie ou rouvre son panneau, et porte une **coche** quand il est visible. | • Une entrée ne fait rien → le fil vers `st.showLeft` / `showRight` / `showBrowser` est coupé.<br>• La coche ne suit pas l'état → elle est lue ailleurs que dans l'état qui commande. |

| **L'entrée grisée.** Menu **Fenêtre**, regarder **Panneau d'outils**. | Elle est **plus grise** que les autres, **ne s'allume pas** au survol, **n'affiche aucun raccourci**, et cliquer dessus **ne fait rien** — le menu reste ouvert. Elle dit « prévu, pas disponible ». | • Elle s'allume au survol → un surlignage est une promesse de clic.<br>• Un **« T »** apparaît à sa droite → on promet un raccourci qui n'a aucun consommateur ; on douterait du clavier plutôt que de la fonction.<br>• Le menu **se referme** quand on clique dessus → le clic « a fait quelque chose », et c'est exactement ce que le gris doit nier.<br>• Elle a disparu → on aurait effacé une intention réelle au lieu de la dire. |

### Pour les bancs (sans souris, sans fenêtre)

```
NK3DModeler.exe --sonde-messages              -> 9/9   (le chemin logger -> bandeau)
NK3DModeler.exe --sonde-ui-etat <fichier>     -> 14/14 (l'aller-retour, les bornes,
                                                        et l'interrupteur des compteurs)
NKEditorKitTest.exe                           -> 226/226 (familles 27 et 28 comprises)
python Applications/NK3DModeler/tests/sonde_barre_menus.py -> 8/8  (la barre de menus, et l'entree grisee)
```

Les deux appellent **les fonctions du produit** (`NkToastDrainerJournal`,
`NkBrowserTreeW`), jamais une copie : *une sonde qui recalcule ce qu'elle mesure
ne peut voir aucun défaut de ce qu'elle mesure.*
||||||| 96c1ce562

---

| démo | ce qu'on doit voir | ce qui prouverait que c'est cassé |
|---|---|---|
| **`nkdemodepliageuv`**<br>`Build/Bin/Release-Windows/nkdemodepliageuv/`<br>*lancer depuis la racine de l'arbre* | Un objet **généré** qui tourne lentement, couvert d'un **damier** (gris clair / gris foncé, liseré orange tous les 4 carreaux). Un bandeau pétrole en haut donne l'état : nombre d'îlots, triangles par îlot, distorsion d'aire et d'angle, recouvrement, temps. **ESPACE** bascule `[BRUT]` ↔ `[DEPLIE]`. Le nom du fichier montré est affiché à droite. | • `[BRUT]` et `[DEPLIE]` donnent **la même image** → les UV calculées n'arrivent pas au GPU : le câblage est coupé entre le solveur et le rendu.<br>• Le damier est un **confetti** de carreaux minuscules sans continuité → le dépliage a éclaté le maillage en un îlot par triangle. **C'est l'état mesuré aujourd'hui** (2 115 îlots pour 2 156 triangles) : c'est attendu, et c'est le défaut à corriger.<br>• Un **REFUS** s'affiche avec son nom et son chiffre d'Euler → le maillage n'est pas dépliable tel quel. Ce n'est pas une panne de la démo.<br>• La fenêtre **fige plusieurs dizaines de secondes** à l'ouverture → elle est tombée sur le maillage TripoSR (42 s mesurées), pas sur celui de famille (267 ms). |

## 25/09 au soir — la première version ne montrait RIEN, et pourquoi

Rodolf l'a lancée : fenêtre ouverte, bandeau pétrole **peint et vide**, et
dessous un aplat uni. **Deux causes, toutes les deux de moi, et elles
expliquent chacune une moitié de l'écran.**

1. **Le bandeau vide** — je demandais `assets/font.ttf`, un fichier qui n'existe
   **nulle part** dans ce dépôt, après avoir écrit « sans fichier à préparer »
   dans l'en-tête de la démo. `LoadFont` rendait une poignée invalide et
   `DrawText` ne peignait rien. Le cadre se peignait (`NkRender2D`), le texte
   non : c'est la signature exacte d'une police absente.
   → `GetDefaultFont()`, embarquée dans le binaire.

2. **L'objet invisible** — j'appelais `r3d->Flush(cmd)` moi-même. Or c'est le
   **graphe de rendu** qui flushe la 3D, dans sa passe de géométrie
   (`NkRendererImpl.cpp:979`), quand `Present()` l'exécute. En flushant à la
   main je vidais la liste de dessin **hors de toute passe** : le travail
   partait dans un tampon sans cible et la passe du graphe ne trouvait plus rien.
   → on ne flushe plus ; `BeginScene` + `Submit`, et `Present()` fait le reste.

**Le 2D et la 3D n'empruntent pas le même chemin** : `r2d->Begin/End` se dessine
en direct, la 3D passe par le graphe. C'est pourquoi le bandeau prouvait le
dorsal graphique **et pas la scène** — un seul écran, deux chemins, et j'ai lu
le premier comme s'il parlait du second.

**Ajouté pour que ça ne se reproduise pas :**
- un **cube de repli**, construit en code, affiché et annoncé quand aucun objet
  généré n'est trouvé. Une fenêtre vide ne peut plus vouloir dire deux choses.
- la **résolution des chemins remonte jusqu'à six niveaux** de répertoires
  parents : la démo ne dépend plus de l'endroit d'où on la lance, et chaque
  essai part au journal.
- une **trace de ce que la démo croit dessiner** — fichier, police, texture,
  matériau, maillages, cadrage, les trois lignes du bandeau, puis la poignée
  effectivement soumise aux deux premières images. Elle situe un défaut en amont
  ou en aval **sans jamais regarder l'écran**.

⚠️ **Le verdict est dans `logs/app.log`, pas sur la sortie standard** — cherchez
les lignes `[demo-uv]`.

⚠️ **Lancée depuis le dossier du binaire, la démo se ferme net** (code −1) : le
compilateur de nuanceurs échoue (`non-opaque uniforms outside a block`). Ce
n'est pas propre à cette démo — c'est la résolution des ressources du moteur
relative au répertoire courant. **Lancer depuis la racine de l'arbre.**

## État honnête au 25/09/2026

Le dépliage **est branché** — pour la première fois il est appelable depuis
autre chose qu'un banc — et il **ne passe pas** les critères :

| maillage | temps | îlots | recouvrement | verdict |
|---|---|---|---|---|
| famille (`000.obj`, 2 156 tri) | 267 ms | 2 115 (**1,0 tri/îlot**) | 0 % | **ROUGE** — confetti |
| TripoSR (`table.glb`, 60 478 tri) | 42 336 ms | 1 | **151 %** | **ROUGE** — atlas replié sur lui-même |

Le solveur n'est pas en cause : sa distorsion d'angle vaut 7,04° en moyenne sur
le maillage TripoSR, dans les clous. **Ce qui manque est une stratégie de
coupe** — l'arbre couvrant du dual donne une découpe *valide* (un disque) et
*mauvaise*. C'est le prochain lot, et c'est exactement ce que `SmartUnwrap`
devait faire dans le `NkUVEditor` qui n'a jamais eu de corps.

## Démos à venir (ce lot)

| démo | ce qu'on doit voir |
|---|---|
| **l'éditeur UV** | on ouvre, on déplace un îlot, la texture suit sur l'objet. Pas encore écrit : les opérations doivent d'abord vivre dans `NKRenderer/Mesh/` sur `NkEditMesh`, et le panneau dans `NKEditorKit` pour que NK3DModeler, NkAnimaEditor et NKScena l'hébergent à l'identique. |

