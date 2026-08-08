# NK3DModeler — spécification produit et cahier des charges

> **Statut : SPÉCIFICATION.** Le dossier ne contient pas encore de code.
> Rédigé le **2026-07-31**. Auteur du besoin : Rihen.
>
> **Ce document tranche.** Il n'a d'utilité que s'il ferme des questions : ce qui
> est dans le produit, ce qui n'y est pas, et à quoi l'on reconnaît qu'une étape
> est finie. Un document qui paraphraserait Blender serait pire qu'aucun document
> — il donnerait l'illusion d'une décision en laissant tout ouvert.

---

## 1. Ce qu'est NK3DModeler

Une **application de modélisation 3D** autonome, bâtie sur le socle
`Engine/NKEditorKit`, et partageant son composant viewport avec Nogee (éditeur de
jeu), NkAnima (animation) et PV3DE.

**Le problème qu'elle résout, et c'est le seul qui justifie de l'écrire :** tout
le travail de modélisation vit aujourd'hui dans `Applications/Sandbox/src/Demo/Demo3D.cpp`
— dix mille lignes pilotées au clavier, dont les retours n'existent que dans un
fichier de log. 17 modificateurs, 36 paramètres, une pile réordonnable, un mode
édition complet : **rien de tout cela n'est atteignable pour un utilisateur**.
La preuve la plus nette est qu'il a fallu inventer des variables d'environnement
(`NK_MOD_ADD`, `NK_LIGHT_PICK_AT`, `NK_EDIT_MODE`) pour vérifier ces fonctions
soi-même.

Le moteur de modélisation existe et il est protégé (harnais `NKEditMeshHarness`,
111 cas). **Ce qui manque, c'est la surface par laquelle on l'utilise.**

---

## 2. À qui il s'adresse

| utilisateur | ce qu'il en attend | conséquence sur le produit |
|---|---|---|
| **Rihen** (premier et principal) | modéliser les assets de ses propres projets sans quitter son écosystème | les raccourcis doivent être ceux de Blender : réapprendre des gestes acquis serait un recul |
| **L'IA (NKGen)** | appeler les mêmes opérations qu'un humain, pas une variante dégradée | toute opération exposée à l'interface doit exister comme **commande donnée**, rejouable et sérialisable |
| **Le lecteur / l'étudiant** | comprendre comment un modeleur est construit | le code reste lisible et commenté ; aucune dépendance tierce |

**Conséquence directe** : l'interface n'est pas une couche cosmétique posée
au-dessus. Elle consomme le **même** chemin de commandes que le rejeu et que
l'IA — sinon trois comportements divergeront en silence.

---

## 3. Ce que « parité Blender » veut dire, et ce que ça ne veut pas dire

**Cela veut dire** :
- les **mêmes raccourcis** (G/R/S, TAB, E, I, K, Ctrl+R, Alt+clic, B/C, `.` et `,`) ;
- le **même vocabulaire** (« pivot », « orientation », « modificateur »,
  « appliquer », « aimantation ») ;
- le **même modèle mental** : mode objet ↔ mode édition, pile non destructive,
  élément actif distinct de la sélection.

**Cela ne veut pas dire** :
- le même **nombre** de fonctions. Blender a trente ans et des centaines de
  contributeurs ;
- les mêmes **algorithmes** internes ;
- reproduire ses défauts par mimétisme.

**Règle d'arbitrage, à appliquer chaque fois que la question se pose :** quand
notre comportement diffère de Blender, ce doit être une **décision écrite et
motivée**, pas un accident. Le cas déjà tranché fait jurisprudence — j'avais
refusé de déplacer une lumière directionnelle au motif que ça n'a pas d'effet sur
l'image ; c'était une erreur, parce que dans Blender une lampe **est** un objet
qu'on range. Correction : le geste est autorisé, l'absence d'effet est **dite**.

---

## 4. Périmètre

### 4.1 Dans le produit (v1)

| domaine | contenu |
|---|---|
| **Modes** | objet ↔ édition (sommet / arête / face, combinables) |
| **Navigation** | orbite / panoramique / zoom, vues numpad, caméra éditeur |
| **Sélection** | clic, multi, zone (rectangle / lasso / cercle), boucles, tout/rien, **élément actif distinct** |
| **Transformation** | déplacer / tourner / redimensionner, verrou d'axe, pivot (5 modes), orientation (global/local/normal), **aimantation** |
| **Édition de maillage** | extruder, insérer, chanfreiner, subdiviser, loop cut, couteau, bisect, souder, dissoudre, spin, to sphere, shrink/fatten, edge split, proportional editing, symétrie |
| **Modificateurs** | les **17 types** livrés, pile réordonnable, activer / dupliquer / retirer / **appliquer**, paramètres générés depuis la table publiée |
| **Ombrage** | flat / smooth, par angle |
| **Affichage** | modes viewport (fil de fer / solide / matériau / rendu + normales / occlusion), matcaps, grille, gizmos |
| **Fichier** | nouveau / ouvrir / enregistrer une scène, importer les 7 formats de maillage déjà lus, journal de commandes rejouable |
| **Annulation** | historique complet, y compris sur les opérations de pile |

### 4.2 Hors produit (v1) — et où ça vit

| ce qui n'y est pas | pourquoi | où |
|---|---|---|
| Édition UV, dépliage | dépend du dépliage automatique, encore à écrire | v2 (`NKGen`, roadmap IA) |
| Nœuds de matériaux | substrat NKGraph déjà décidé, consommateur à part | Nogee |
| Animation, rigging | module dédié | NkAnima |
| Rendu final, compositing | politique de rendu | Nogee / PV3DE |
| Simulation (eau, fumée) | module `NKSimulation`, encore en spécification | NKSimulation |
| Modificateurs à dépendance externe | groupes de sommets, textures, autre objet, simulation, poils — les systèmes n'existent pas | quand leur dépendance existera |

**Cette colonne « pourquoi » est le cœur du document.** Sans elle, chaque absence
ressemble à un oubli et sera re-proposée à chaque session.

### 4.3 Le SCULPT est dans le produit *(décision de Rihen, 31/07)*

> Il figurait d'abord hors périmètre. **C'était une erreur** : le sculpt fait
> partie de ce qu'on attend d'un modeleur, et surtout il impose une contrainte
> d'architecture qu'il vaut bien mieux poser **avant** d'écrire l'application.

> ⚠️ **CORRECTION DE MA CORRECTION (31/07).** Rihen : *« on a même déjà commencé à
> implémenter les algorithmes de sculpting, tu peux vérifier. »* Vérifié — et il a
> raison. **`NKRenderer/Tools/PixolSculpt/` existe** : 1 234 lignes, 8 fichiers,
> deux shaders compute, un `DESIGN.md` complet. J'avais écrit un plan S1-S5 en
> supposant qu'il n'y avait rien. C'était faux, **et faux d'une manière
> intéressante** : l'approche déjà choisie n'est pas celle que j'esquissais.

**L'approche existante — pixol / 2.5D, façon ZBrush.** Elle ne sculpte **pas** des
sommets : elle maintient un G-buffer « pixol » en **espace-écran** (profondeur +
normale + matériau + masque + couleur) et le mute par des **dispatchs compute
bornés à la tuile sous la brosse**. Conséquence décisive : *le coût est borné par
la résolution d'écran, pas par le nombre de triangles.*

**Ce que ça change par rapport à ce que j'avais écrit :**

| ma proposition (mesh) | l'approche déjà en place (pixol) |
|---|---|
| S1 mise à jour partielle du maillage GPU | **sans objet** : rien n'est envoyé au GPU, la brosse écrit directement dans des images de stockage |
| S2 requête spatiale sur les sommets | **sans objet** : le voisinage est le voisinage de PIXELS, gratuit |
| S3 trait, S4 brosses | **déjà spécifiés** : `NkSculptStroke` (dabs interpolés + rectangle sale borné), 8 modes de brosse et 5 courbes d'atténuation déjà déclarés |
| S5 dyntopo ou multirésolution | `NkSculptTileStore.h` est posé comme la voie « vraie géométrie » — **à trancher plus tard**, comme prévu |

**État réel, tel que le dit son propre `DESIGN.md` : « squelette ».** Les structures,
les enums et la façade sont là ; l'implantation ne l'est pas. Restent à écrire :
création des images de stockage, chargement des pipelines compute, le kernel de
brosse `RAISE`/`LOWER`, le rectangle sale, la passe de resolve vers le G-buffer, et
le branchement dans `NkRendererImpl` (le diff est écrit, non appliqué).

**Deux pièges y sont déjà consignés**, et ils valent d'être relus avant de reprendre :
le backend OpenGL passe un format **codé en dur** `GL_RGBA32F` à `glBindImageTexture`
(à étendre si l'on utilise `r32f`/`rgba16f`) ; et la profondeur du G-buffer est
souvent non-storage, donc son resolve peut exiger une petite passe graphique plutôt
qu'un `imageStore`.

**Ce qui reste vrai de mon analyse**, et qui sert l'autre voie (celle de
`NkSculptTileStore`, sculpt sur vraie géométrie) :

| brique existante | ce qu'elle apporte au sculpt |
|---|---|
| **proportional editing** (6 courbes d'atténuation) | c'est **exactement** la mécanique de retombée d'une brosse. Une brosse est un déplacement pondéré par une courbe autour d'un point — le calcul existe et il est testé. |
| **symétrie 1-3 axes** | sculpter symétriquement, sans code neuf |
| **Catmull-Clark** | la densité de maillage que le sculpt exige |
| **cycles radial et disque** (BMesh étape 2) | le voisinage d'un sommet en O(k), nécessaire au lissage et à la dilatation |
| **harnais, 116 cas** | un filet déjà en place pour une opération qui déforme massivement |

**⚠️ La contrainte de mise à jour PARTIELLE reste valable**, mais pour l'**autre**
voie. Aujourd'hui, toute opération d'édition reconstruit et resynchronise le
maillage entier (`Demo3D_SyncFromHE`) : sans conséquence sur un cube de 24 sommets,
rédhibitoire pour du sculpt **sur vraie géométrie**. Le chemin pixol y échappe par
construction — c'est même son argument principal.

**Position honnête** : le sculpt est dans le produit, l'architecture est choisie et
documentée, et il reste à l'**implanter**. Le chantier s'ouvre quand l'interface
existe : une brosse sans interface pour régler son rayon, sa force et son mode ne
se teste pas.

### 4.4 MODÉLISATION et MATÉRIAUX par GRAPHE DE NŒUDS *(décision Rihen, 31/07)*

Demande : pouvoir faire de la **modélisation par blueprint / visuel**, et de même
pour les **matériaux**.

**Cette demande tombe sur une décision déjà prise et documentée.**
`Kernel/Runtime/NKGraph/ROADMAP.md` (validé le 2026-07-09) définit **un substrat de
graphe UNIQUE** pour tout l'écosystème, et **cite déjà nommément** les deux
consommateurs demandés : *« matériaux (NKRenderer Phase T.2) »* et *« modélisation
procédurale »*. Rien à réinventer : il faut l'implanter.

**Architecture, telle qu'elle est déjà arbitrée** — la séparation est la règle :

| couche | où | rôle |
|---|---|---|
| **3 — sémantique métier** | chez chaque consommateur | bibliothèques de nœuds + exécution : compilateur NkSL pour les matériaux, **commandes `NkMeshEditCommand` pour la modélisation** |
| **2 — édition** | `Engine/NKEditorKit` | le canevas de nœuds : panoramique, zoom, fils, sélection, recherche, groupes — **appris une fois, partagé partout** |
| **1 — cœur** | `Kernel/Runtime/NKGraph` | modèle pur : nœuds, sockets **typés**, connexions, validation, tri topologique, sérialisation, annulation |

> **La phrase à retenir de cette architecture** : *« on unifie l'AUTORAT — modèle,
> sérialisation, interface — jamais l'EXÉCUTION. »* Les matériaux se **compilent**
> vers NkSL à l'édition ; la modélisation **produit des commandes** rejouables. Un
> `if (nodeType == …)` métier dans le cœur signerait sa mort.

**Ce qui rend la modélisation par nœuds réaliste chez nous, et c'est décisif** :
le journal de commandes existe **déjà** (`NkMeshEditRecorder`, sérialisé,
rejouable) et l'aller-retour est prouvé au bit près. Un graphe de modélisation
n'est donc pas un nouveau moteur — c'est une **autre façon d'écrire la même liste
de commandes**. La preuve que les deux chemins coïncident est déjà outillée.

**État réel** : `Kernel/Runtime/NKGraph/` ne contient **que sa roadmap**, aucun
code, aucune cible de build — comme NKSimulation. Le module naîtra avec son premier
consommateur.

**Ordre proposé** : cœur NKGraph (couche 1) → canevas (couche 2) → **matériaux**
d'abord *(périmètre fermé, backend NkSL existant)* → **modélisation** ensuite, qui
bénéficiera d'une API déjà éprouvée par un second usage. La roadmap NKGraph appelle
cela la « règle des deux consommateurs » : *c'est le deuxième client qui force la
généralisation de l'API.*

### 4.5 MODÉLISATION PAR PROMPT, en CYCLE avec l'édition manuelle *(Rihen, 31/07)*

Demande : *« modéliser avec l'IA dans l'interface par prompt, tout en acceptant de
modifier manuellement le résultat obtenu et continuer si possible de manière
cyclique entre prompt et modification manuelle. »*

**C'est la demande la plus exigeante du document, et la pièce qui la rend possible
existe déjà.**

#### Pourquoi le cycle est le point dur

La partie facile est « un prompt produit un objet ». La partie qui casse
d'habitude, c'est le **deuxième tour** : l'utilisateur retouche le résultat à la
main, redemande quelque chose à l'IA, et **ses retouches disparaissent**. C'est le
comportement par défaut de tout système où l'IA rend un **maillage fini** : le
second prompt régénère depuis zéro et écrase le travail manuel.

#### Ce qui l'évite, et nous l'avons déjà

> **L'IA ne produit pas un maillage. Elle produit des COMMANDES.**

`NkMeshEditRecorder` existe : chaque opération d'édition est une **commande donnée**,
sérialisée, **rejouable**, et l'aller-retour est prouvé au bit près
(`NK_EDIT_IDENTITY` → 0 sur tous les attributs). Le journal est déjà le format
d'échange dont ce cycle a besoin.

Conséquence : **prompt et main écrivent dans le MÊME journal.**

```
   prompt ──▶ [commandes IA]  ─┐
                               ├──▶  journal unique ──▶ maillage
   main   ──▶ [commandes UI]  ─┘        (rejouable, annulable)
                               ▲
   prompt suivant ────────────┘   il AJOUTE, il ne régénère pas
```

Les retouches manuelles ne sont pas un obstacle au prompt suivant : ce sont des
entrées du même journal, que l'IA voit comme contexte. **Le cycle se ferme tout
seul** — non par une astuce, mais parce que les deux chemins parlent la même langue.

C'est aussi la raison pour laquelle le § 2 exige que *toute opération de l'interface
soit une commande*. Cette exigence, écrite avant cette demande, la sert exactement.

#### Ce que ça impose

| exigence | pourquoi |
|---|---|
| **L'IA n'émet que des commandes existantes** | une commande inventée ne serait ni rejouable, ni annulable, ni éditable ensuite |
| **Sortie validée par schéma**, jamais du texte libre exécuté | même règle que `NkRoleContextSchema` dans NkAnima : une sortie malformée est **rejetée avec un message**, jamais interprétée au mieux |
| **Aperçu avant application** | la proposition de l'IA est un lot de commandes qu'on **voit** avant d'accepter. Sinon chaque essai est une annulation |
| **Annulation par lot** | « défaire ce que le prompt vient de faire » doit être **un** geste, pas trente |
| **Le prompt et sa réponse sont journalisés** | savoir quel prompt a produit quelle partie du modèle — sans quoi le cycle devient illisible au bout de dix tours |
| **Repli explicite** | modèle indisponible ou réponse invalide → on le **dit**, on ne devine pas |

#### Interface (détaillée dans `UI_SPEC.md`)

Un panneau **« Assistant »** dans la colonne de droite : champ de prompt,
historique des échanges, et pour chaque réponse la **liste des commandes proposées**
avec `Aperçu` / `Appliquer` / `Rejeter`. L'utilisateur voit **ce que l'IA va faire**
avant que ce soit fait — c'est ce qui rend la collaboration tenable plutôt
qu'inquiétante.

#### État des dépendances — honnêtement

| brique | état |
|---|---|
| journal de commandes rejouable | ✅ livré et prouvé |
| annulation / rétablissement | ✅ livré |
| 17 modificateurs + ~40 opérations d'édition adressables | ✅ livrés |
| paramètres adressables par nom | ✅ livré (`NkModParam`) |
| **modèle de langue local** | 🟡 NkGPT existe et génère du texte ; **le pont n'est pas câblé**, et le Palier 4 est interrompu |
| **traduction prompt → commandes** | ❌ non commencé — c'est le vrai travail |

**Position honnête** : l'ossature est là et elle est bonne. Ce qui manque est la
pièce du milieu — transformer une phrase en une suite de commandes valides — et
elle ne doit pas être bricolée : sans validation par schéma, elle produira des
suites plausibles et fausses, exactement comme le corpus l'a montré sur les faits
historiques.

### 4.6 SCRIPTS et EXTENSIONS en C++ *(Rihen, 31/07)*

Demande : *« écrire des scripts comme sur Blender, créer des add-ons »*, en C++.

**Le mécanisme existe déjà, et il est prouvé par exécution.** `Engine/Noge` porte
le premier des cinq piliers de scripting :

| brique | où | état |
|---|---|---|
| ABI C **autonome** (aucun lien avec le moteur) | `Noge/ECS/Scripting/NkScriptABI.h` | ✅ |
| chargement de DLL, **copie fantôme**, injection des hooks NKMemory | `NkScriptBridge.cpp` | ✅ |
| **rechargement à chaud** : détection de date → sérialisation de l'état → déchargement → rechargement → restauration | `NkScriptLoader::HotReload` | ✅ |
| **compilation à l'exécution** (appel clang++ direct, même chaîne que jenga) | `NkHotReloadDemo` | ✅ |
| surveillance de fichiers | `NKFileSystem/NkFileWatcher.h` | ✅ |

Preuve : `Applications/NkHotReloadDemo`, **16 assertions**. Un compteur passe de
`+1/tick` à `+2/tick` par recompilation d'un `.cpp` **pendant l'exécution**, et
**son état survit** au rechargement — la valeur reste à 5 pendant le remplacement,
puis suit le nouveau comportement. C'est exactement ce qu'un add-on demande.

**Ce qui manque n'est donc pas la machinerie, c'est la SURFACE** : ce qu'un add-on
a le droit d'appeler.

#### La règle, et c'est la troisième fois qu'elle s'impose

> **Un add-on n'appelle pas les fonctions internes. Il émet des COMMANDES.**

S'il touchait directement au maillage, il court-circuiterait l'annulation, le
rejeu et l'IA — et le premier add-on écrit ainsi rendrait ces trois systèmes faux
sans qu'on s'en aperçoive. En émettant des commandes, il hérite de tout
gratuitement.

C'est le **troisième consommateur** de la même règle, après l'assistant par prompt
(§ 4.5) et le graphe de nœuds (§ 4.4). Trois besoins indépendants qui convergent
sur la même contrainte : c'est le signe que l'architecture est juste.

#### La surface est déjà à moitié conçue

Parce que tout a été rendu **adressable par données**, un add-on dispose déjà de :

| ce qu'il peut faire | grâce à quoi | état |
|---|---|---|
| exécuter n'importe quelle opération d'édition | `NkMeshEditCommand` sérialisable | ✅ |
| lire et régler n'importe quel paramètre de modificateur | `NkModifierParams` (clé stable + type + bornes) | ✅ |
| déclarer ses propres raccourcis, et voir les conflits | `NkShortcutTable` | ✅ |
| analyser un maillage | `NkMeshAnalysis` | ✅ |
| **déclarer une commande nommée** | `NkEditorCommand` (NKEditorKit) | ✅ |
| **ajouter un panneau** | `NkEditorPanel` (NKEditorKit) | ✅ |
| **ajouter un type de modificateur** | ❌ — la table `NkModifierType` est une énumération **fermée** |
| **s'enregistrer / se décharger proprement** | ❌ — pas de cycle de vie d'add-on |

**Deux briques à écrire, donc**, et une seule est délicate : ouvrir l'enregistrement
des modificateurs à des types venus de l'extérieur. L'énumération est aujourd'hui
**sérialisée** — un type d'add-on ne peut pas y prendre un numéro sans risquer de
collisionner avec un futur type interne. Il lui faudra un identifiant **textuel**
(`monaddon.plisser`), exactement comme les clés de paramètres.

#### Réserves honnêtes sur le C++ comme langage de script

Elles ne remettent pas le choix en cause — elles disent ce qu'il coûte :

1. **Un plantage dans un add-on tue l'application.** L'ABI C et la copie fantôme
   isolent le *chargement*, pas l'*exécution*. Blender a choisi Python en partie
   pour cela. Atténuation réaliste : un add-on qui plante est **désactivé au
   redémarrage suivant** et signalé, plutôt que de replanter en boucle.
2. **Il faut un compilateur.** Acceptable ici — la chaîne clang++ est déjà exigée
   par le projet, et `NkHotReloadDemo` s'en sert déjà à l'exécution.
3. **L'ABI doit rester stable.** C'est déjà la discipline de `NkScriptABI.h`
   (header autonome, aucun lien) : la tenir est un engagement, pas une évidence.

#### L'échelle complète, du plus simple au plus puissant

| niveau | pour qui | état |
|---|---|---|
| **graphe de nœuds** (§ 4.4) | ne pas programmer du tout | ❌ à écrire (NKGraph) |
| **assistant par prompt** (§ 4.5) | décrire au lieu de faire | 🟡 ossature prête |
| **add-on C++ rechargeable** | tout faire | ✅ mécanisme prêt, surface à finir |
| **journal de commandes** | le socle commun aux trois | ✅ livré |

Les bridges **C#** et **Python** existent comme specs dans Noge
(`Scripting/CSharp/`, `Scripting/Python/`) — hors périmètre v1, mais la même
surface de commandes les servira sans rien redéfinir.

### 4.7 Verrou partagé identifié

Les **groupes de sommets** bloquent simultanément : Mask par groupe, les trois
modificateurs Vertex Weight, et le skinning côté animation. C'est le prérequis au
meilleur rapport valeur/coût du périmètre v2 — à traiter comme une brique du
noyau, pas comme une fonction du modeleur.

---

## 5. Contraintes non négociables

Elles viennent du projet, pas du produit, et aucune n'est discutable ici :

1. **Zéro STL.** `NKMemory`, `NKMath`, `NKContainers`. Ce qui manque s'ajoute au
   module concerné.
2. **Allocation uniquement via NKMemory** ; tout `Create` a son `Destroy`.
3. **From scratch** : aucun code tiers, même reformaté. Sont légitimes : un
   oracle en boîte noire, un texte de spécification, des tables extraites par
   script depuis un standard.
4. **Cinq backends** (Vulkan, DX11, DX12, OpenGL, logiciel) : aucune fonction ne
   peut dépendre d'un seul.
5. **Debug ET Release** doivent compiler et se comporter pareil.
6. **Toute évolution du maillage passe par le harnais** : cas ajoutés **en fin**,
   les lignes précédentes gardant leur numéro.
7. **Aucun raccourci en dur** : ils passent tous par la table de raccourcis de
   NKEditorKit. C'est ce qui rend possible à la fois leur affichage dans
   l'interface et leur reconfiguration.

---

## 6. Critères d'acceptation

Une étape est finie quand ces phrases sont **vraies et vérifiables**, pas quand
le code compile.

### Jalon A — l'outil devient utilisable *(cible immédiate)*
- [ ] L'application démarre, ouvre une scène, affiche un viewport navigable.
- [ ] Le **panneau de modificateurs** liste les 17 types, permet d'en ajouter un,
      de le déplacer dans la pile, de le désactiver, de le dupliquer, de le
      retirer, de l'appliquer — **à la souris, sans toucher au clavier**.
- [ ] Les paramètres affichés sont **générés depuis `NkModifierParams`** : ajouter
      un 18ᵉ modificateur ne demande **aucune ligne d'interface**.
- [ ] Un **en-tête** montre en permanence : mode, aimantation (état + pas + grille),
      pivot, orientation, objet actif. Aucune de ces informations ne doit plus
      exiger de lire un fichier de log.
- [ ] La **table de raccourcis** est affichable, et modifier une entrée change
      effectivement le comportement.

### Jalon B — parité de geste
- [ ] Un utilisateur de Blender modélise une forme simple **sans consulter d'aide**.
- [ ] Les 40 raccourcis existants sont dans la table, aucun en dur.
- [ ] Écarts de comportement avec Blender : **liste écrite**, chacun motivé.

### Jalon C — l'IA passe par la même porte
- [ ] Toute opération de l'interface produit une **commande** rejouable.
- [ ] Rejouer le journal d'une session reproduit le maillage **au bit près**
      (même critère que `NK_EDIT_IDENTITY`).

---

## 7. Ce qui pourrait faire échouer ce projet

Écrit d'avance, pour être surveillé :

1. **Recommencer le viewport au lieu de l'extraire.** Demo3D contient dix mille
   lignes éprouvées (pick, cage, opérations modales, journal, rejeu). Les
   réécrire « proprement » perdrait des mois et des correctifs subtils. Le cap est
   l'**extraction**, prouvée par déplacement — la méthode a déjà servi et est
   documentée (`66a489ba` : 0 ligne supprimée sur la comparaison ligne à ligne).
2. **Une interface qui double la logique.** Si un bouton fait autre chose que la
   commande correspondante, l'IA et le rejeu divergeront de l'humain, en silence.
3. **Ajouter des fonctions avant que les précédentes soient atteignables.** C'est
   l'erreur déjà commise : 17 modificateurs qu'on ne peut pas cliquer.
4. **Des raccourcis en dur.** Ils rendent la table décorative et l'affichage faux.

---

## 8. Ordre de travail décidé

Il découle du cap déjà acté dans `Engine/Noge/HANDOFF.md` :

1. **Table de raccourcis** dans NKEditorKit (configurable, aucun raccourci en dur).
2. **Extraction du câblage viewport** de Demo3D vers NKEditorKit, par déplacement
   prouvé, à comportement identique.
3. **Application NK3DModeler** : fenêtre, disposition, panneaux.
4. **Panneau modificateurs** piloté par la table de paramètres.
5. **En-tête et barre d'état**.
6. Le reste, par ordre d'utilité mesurée à l'usage.

Chaque étape doit laisser l'outil **au moins aussi utilisable** qu'avant. Aucune
étape n'a le droit de casser Demo3D tant que l'extraction n'est pas terminée :
c'est aujourd'hui le seul endroit où la modélisation fonctionne.
