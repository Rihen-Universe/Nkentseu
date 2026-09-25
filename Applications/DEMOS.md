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

### Pour les bancs (sans souris, sans fenêtre)

```
NK3DModeler.exe --sonde-messages              -> 9/9   (le chemin logger -> bandeau)
NK3DModeler.exe --sonde-ui-etat <fichier>     -> 14/14 (l'aller-retour, les bornes,
                                                        et l'interrupteur des compteurs)
NKEditorKitTest.exe                           -> 226/226 (familles 27 et 28 comprises)
```

Les deux appellent **les fonctions du produit** (`NkToastDrainerJournal`,
`NkBrowserTreeW`), jamais une copie : *une sonde qui recalcule ce qu'elle mesure
ne peut voir aucun défaut de ce qu'elle mesure.*
