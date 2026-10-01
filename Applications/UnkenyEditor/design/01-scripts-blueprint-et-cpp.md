# Les scripts d'Unkeny — un Blueprint et du C++

### Document 01 d'UnkenyEditor — conception

> Rédigé le 29/09/2026 à la demande de Rihen : *« on doit avoir deux langages de
> script, un Blueprint et l'autre Python ou C# ou C++ interprété, à toi de voir ;
> pour Blueprint il y a déjà des systèmes de graphe conçus pour ça. »*
> Complément du même jour : *les corps mous doivent pouvoir être des personnages
> ou des parties de personnage, pilotés à la souris, au clavier, à la manette ou
> au tactile.*
>
> **Statut : CONCEPTION.** Aucune ligne de code n'accompagne ce document, et rien
> n'a été compilé ni exécuté pour l'écrire : c'est une lecture du dépôt (branche
> `physic2d-editeur-ue5`, 29/09 au soir). Chaque fait porte son chemin. Ce qui
> n'a pas été vérifié est marqué **⚪ NON VÉRIFIÉ** et récapitulé au § 12.
>
> **Compagnons** : `Kernel/Runtime/NKGraph/ROADMAP.md` (le substrat de graphe et
> ses décisions), `Kernel/Runtime/NKGraph/CATALOGUE_NOEUDS.md` (les nœuds de
> blueprint visés), `Applications/Nogee/design/04-produit-noge-nogee.md` § 3.3 et
> D1 (la même décision, côté 3D), `Engine/Noge/ROADMAP.md` « Quintette de
> scripting » (le C++ à chaud prouvé).
>
> ⚠️ `CLAUDE.md`, qui porte les blocs de décision du dépôt (dont « substrat de
> graphe unique NKGraph »), est **absent** de ce dossier et des copies voisines
> (`.gitignore`, cf. `CONVENTIONS_CODE.md`). Les règles citées ici viennent de
> leurs échos suivis par git (`NKGraph/ROADMAP.md`, `NkNodeGraph.h`,
> `Engine/Unkeny/README.md`). Si `CLAUDE.md` dit autre chose, **il gagne**.

---

## 0. En une page

**La décision.** Deux langages, **une seule API de jeu** derrière eux :

| | **Blueprint** | **C++** |
|---|---|---|
| pour qui | tout le monde ; la voie par défaut | le programmeur ; ce qui est lourd ou pointu |
| s'écrit | à la souris, dans UnkenyEditor | dans un éditeur de texte (NKCode) |
| s'exécute | un **module de bytecode** interprété par une **VM d'Unkeny** | du code natif |
| en éditeur | recompilé à chaque modification, même pendant JEU | **DLL / SO rechargée à chaud** (Windows, Linux ; macOS ensuite) |
| en livraison | les **mêmes octets** sur les 7 plateformes | les **mêmes sources liées en statique** (Android, iOS, Web, HarmonyOS) |
| appelle le moteur par | la table C `NkUnkHoteV1` | la même table C `NkUnkHoteV1` |

Python et C# sont **écartés**, Lua est le **plan B nommé** (§ 1). C'est la
décision que Nogee a déjà prise pour le moteur 3D (`04-produit-noge-nogee.md`,
D1 : scripts C# et Python pour le jeu **gelés**, « squelettes morts ; C++ à
chaud + Blueprints suffisent ») : Unkeny ne fait pas exception.

**Le graphe retenu : `Kernel/Runtime/NKGraph`**, et lui seul. Il porte déjà les
prises d'**exécution** comme un axe distinct du type (`NkSocketFamily::Exec`,
`NkNodeGraph.h:204`) avec les arités de Blueprint. Il ne porte **pas**
l'exécution — c'est voulu (« on unifie l'AUTORAT, jamais l'EXÉCUTION ») : la VM
est la **couche 3**, et Unkeny en est le **premier consommateur réel**, celui
que la décision du 24/08 attendait pour écrire le bytecode. **Il manque le
canevas** (couche 2, à 0 %) : c'est le plus gros morceau d'interface du chantier.

**Les risques principaux** : (1) un module C++ tourne dans le processus de
l'éditeur, et clang-mingw ne sait pas rattraper une faute (`__try` absent) —
Blueprint ne peut pas tuer l'éditeur, le C++ **si** ; on garantit qu'on ne perd
rien et qu'on sait qui, et un mode « Jouer isolé » pour la garantie stricte
(§ 8) ; (2) `sharedlib()` de jenga n'a **jamais** été exercé dans ce dépôt
(§ 5.3) ; (3) le canevas nodal est à écrire de zéro, sans primitive de Bézier
dans `NkGuiDrawList` ; (4) les corps mous n'ont ni « au sol », ni poussée qui
garde la forme, ni saisie par corps, ni événements de contact (§ 3.5).

**Les paliers** (§ 10) : S0 socle et table C (C++ statique) → S1 bytecode et VM
→ S2 compilateur et premier lot de nœuds → S3 corps mous pilotables (en
parallèle) → S4 canevas → S5 C++ à chaud → S6 livraison statique → S7 latents,
fonctions, isolation.

---

## 1. La décision, et ce qui l'a faite

### 1.1 Ce qui a été mesuré dans le dépôt

| piste | ce que le dépôt en a | ce que ça a déjà coûté, ou ce qui manque |
|---|---|---|
| **Python** | `Externals/Libs/PythonEmbed` (CPython 3.12.7) + `pybind11`, **servant NKCode seulement** (embarquer Jenga). Côté moteur : `Engine/Noge/.../Scripting/Python/NkScriptPython.h` (646 l.), **squelette mort** — `NKECS_PYTHON_AVAILABLE` n'est défini nulle part (`Engine/Noge/ROADMAP.md:328`). | NKCode, **sur le bureau seulement** : la `python312.lib` officielle est au format MSVC, il a fallu régénérer `libpython312.a` par `objdump` + `dlltool` (`Applications/NKCode/ROADMAP.md:316-318`) ; `_DEBUG` bascule sur l'ABI de débogage absente du runtime et doit être neutralisé (`NkEmbeddedJenga.cpp:18-24`) ; le runtime n'était pas posé à côté du binaire → **0xC0000135** au lancement, corrigé le **25/09** pour une décision du **21/07** (`NKCode.jenga:403-420`) ; Linux demande une **autre** distribution (`python-build-standalone`, `NKCode.jenga:24-40`). Android, iOS, Web : **jamais tentés**. |
| **C#** | `Scripting/CSharp/NkScriptCSharp.h` (631 l.), **squelette mort** : tout sous `#ifdef NKECS_MONO_AVAILABLE`, jamais défini, **aucun `.cs`** dans le dépôt (`Engine/Noge/ROADMAP.md:327`). | Un runtime Mono ou CoreCLR entier à embarquer par plateforme. Rien d'amorcé. |
| **C++ à chaud** | `Engine/Noge/src/Noge/ECS/Scripting/NkScriptABI.h` (ABI C **autonome**, libc seule) + `NkScriptBridge.cpp` (`LoadLibraryA`/`dlopen`, copie fantôme `Foo.dll.hotN`, rechargement qui garde l'ancienne version si la nouvelle échoue). Prouvé par `Applications/NkHotReloadDemo` : **16 assertions, compteur gardé 5 → 11** à travers v1 → v2 (`Engine/Noge/ROADMAP.md:1910-1971`, 24/07). **Second précédent** : `Applications/ConquerorLab` (`NkcModuleHost.h`, `NkcModuleCompiler.h`) compile un `.cpp` à la sauvegarde, le charge, vérifie une ABI **majeure/mineure**, et sur Android/Web ne garde que les modules **compilés dans l'application**. | ⚪ Ni l'un ni l'autre n'a été ré-exécuté ici (aucun binaire `NkHotReloadDemo` dans `Build/Bin`). Chemin POSIX écrit, **jamais exécuté** (ROADMAP de Noge). Aucun des deux n'utilise `sharedlib()` de jenga : ils appellent le compilateur directement (§ 5.3). |
| **Blueprint** | `Kernel/Runtime/NKGraph` : cœur livré (≈ 4 000 l., en-tête pur), prises d'exécution, `.nkgraph` v5, annuler/refaire. Décision du **24/08** : *« la forme non nodale d'un blueprint sera un MODULE DE BYTECODE »*, qui doit *« s'écrire, se lire et se sérialiser SANS QU'AUCUN GRAPHE N'EXISTE »* (`NKGraph/ROADMAP.md:327-358`). | **Aucune VM d'exécution de graphe** ; **aucun canevas** (§ 2). |
| **Lua** | **Rien** (`Externals/Libs` : ImGui, NKAssimp, NKGLSlang, NKGlad, NKMbedTLS, NKOpenXR, NKSPIRVCross, NKShaderc, PythonEmbed, Vulkan-Headers, pybind11). | Une bibliothèque C tierce à vendoriser. |

### 1.2 Pourquoi le C++, et pas un interprété de plus

1. **Le besoin « interprété, partout » est déjà couvert — par le Blueprint.** Sa
   VM est du C++ d'Unkeny, sans JIT (interdit sur iOS), sans fil (le Web
   d'Unkeny tourne sans, `NkUnkenyScene.h:80-95`), sans bibliothèque tierce. Un
   second langage interprété ferait doublon sur ce besoin-là et apporterait son
   runtime.
2. **Un runtime tiers se paie par plateforme, et le dépôt vient de le payer.**
   Pour CPython dans NKCode, sur le **seul bureau** : une bibliothèque de lien
   à régénérer, une ABI de débogage à neutraliser, une distribution Linux à
   part, et un runtime posé à côté du binaire Windows le 25/09 pour une
   décision du 21/07 (§ 1.1). Unkeny vise sept plateformes, dont quatre
   (Android, iOS, Web, HarmonyOS) où un runtime tiers n'a jamais été tenté.
3. **Le C++ est la langue du moteur** : la table C que le Blueprint appelle
   (§ 4.3) est **exactement** celle qu'appelle un module C++. Pas de *binding*
   à écrire en plus, pas de marshaling, une seule sémantique.
4. **« C++ interprété » n'existe pas dans le dépôt** et n'y viendrait qu'avec un
   LLVM embarqué (façon Cling) — un poids hors de proportion. Le C++ **compilé
   et rechargé** donne le même confort d'itération sur PC, et le statique donne
   la vitesse partout ailleurs.

**Le prix, dit tout de suite** : du code natif dans le processus peut planter
l'éditeur. Le § 8 dit ce qu'on garantit et ce qu'on ne peut pas garantir.

### 1.3 Ce qui renverserait la décision

- **Le chargement de DLL ne tient pas sous clang-mingw avec l'ABI C (S5
  échoue).** → On garde le C++ **statique** (recompiler et relancer l'éditeur,
  le fonctionnement d'UE5 sans *Live Coding*). Ce n'est **pas** une raison de
  revenir à Python : le C++ statique marche déjà partout.
- **Rihen veut des scripts modifiables PAR LE JOUEUR sur mobile ou Web (mods).**
  → Le C++ est exclu là (pas de compilateur sur l'appareil ; iOS n'accepte pas
  de code chargé hors du paquet — ⚪ règle d'Apple, non documentée dans le
  dépôt). Deux réponses, dans l'ordre : (a) un **langage textuel qui compile
  vers le MÊME bytecode** — la décision du 24/08 prévoit explicitement « un
  autre langage de surface » ; (b) **Lua**, plan B tiers, si (a) est trop long.
- **La VM Blueprint s'avère trop lente** pour un jeu réel (mesure du banc x10,
  § 9). → Compiler le module en C++ (la seconde forme d'exécution que le ROADMAP
  de NKGraph cite : « le Blueprint interprète (VM) ou génère du C++ »), pas
  changer de langage.

---

## 2. Les systèmes de graphe du dépôt, et celui qu'on réutilise

### 2.1 Inventaire

| système | où | ce qu'il fournit | maturité | exécution ? |
|---|---|---|---|---|
| **NKGraph** (couche 1) | `Kernel/Runtime/NKGraph/src/NKGraph/` : `NkNodeGraph.h/.inl` (655/1 022), `NkNodeGraphIO.inl` (851), `NkGraphDocument.h/.inl` (137/469), `NkGraphGroup.h/.inl` (282/599) | **Modèle** : nœuds, prises typées (types enregistrés par le consommateur, composites avec empreinte), conversions **dirigées**, identifiants stables jamais recyclés, liens qualifiés. **Sérialisation** `.nkgraph` v5 (texte, une directive par ligne, prises désignées par NOM depuis v2). **Annuler/refaire** (`NkGraphHistory`, instantanés, 128). **Sous-graphes** et plan de données aplati (`BuildPlan`). **Validation** qui rend des diagnostics désignés. | En-tête pur, **sans cible jenga**, absent de `Nkentseu.jenga` (inclusion à la main, ex. `NkMatGraphCheck.jenga:54`). Éprouvé par `NkMatGraphCheck` (~148 cas dont `exec/*`, `etats/*`) et `NKEditMeshHarness` (19 cas). Dernier commit `9c3fad332` (13/09). | **Modèle oui, exécution non** : `NkSocketFamily { Data, Exec }` (`NkNodeGraph.h:204`) ; une sortie exec n'a **qu'une** suite (`ExecOutputAlreadyBound`), une entrée exec en accepte plusieurs ; acyclicité **universelle** (`WouldCycle`, « un cycle vit à l'intérieur d'un nœud »). Aucun marcheur d'exécution, par décision. |
| **NkMatGraph** (couche 3, matériaux) | `Kernel/Runtime/NKRenderer/src/NKRenderer/Materials/Graph/NkMatGraphTypes.h` (2 091), `NkMatGraphCompile.h` (3 541) | Types et conversions du domaine, **prototypes de nœuds** (`NkMatNodeProto`, `NkMatRegistreProtos`), **« nœuds compatibles avec cette prise »** (`NkMatNoeudsPourPrise`), validation de domaine, compilateur vers NkSL, évaluateur CPU. | Mûr pour son domaine. `NkMatGraphDemo` (1 513 l.) est une preuve de rendu **sans interface**. | Non (données seules, par nature). **Le patron à copier** pour la couche 3 Blueprint. |
| **Blueprint de NKGui** | `Kernel/Runtime/NKGui/src/NKGui/Doc/NkGuiGraphe.h/.inl` (88/744), évaluateur dans `NkGuiInteraction.h` (2 259) | `behavior "x" graph { node … wire … }` **compilé en texte de script NKGui**, interprété par `NkGuiEvaluateur`. Catalogue fermé : 5 événements d'interface, `Branch`, `Sequence`, actions de widget, maths pures. | Fonctionne : `NKGuiInteractTest` (b8, b9), commit `5d7e38a6d` (27/09). **Aucun éditeur visuel** (`NKUIDesign/Panels.h:8575 DessinerBlueprint` = grille et texte d'attente). | Oui, mais **un seul événement par graphe**, ni boucle, ni latent, **aucun contrôle de type** (valeurs en texte). **Modèle à part, pas NKGraph.** |
| **NkBlueprint de Noge** | `Engine/Noge/src/Noge/ECS/VisualScript/NkBlueprint.h` (909), `NkValidGraph.h` (125), `NkBlueprintHotReload.h` (170) | Classes façon Unreal : `NkBlueprintNode::Execute`, registre, graphe à connexions par **indice**. | **Spécification** : zéro consommateur (`Engine/Noge/ROADMAP.md:329`) ; exécuteur cassé à la lecture (`ResolveInputs` vide, toutes les sorties exec suivies). Dépend de `NkGameObject` et de la physique de Noge, qui vit sur NKRenderer — **exclusif de NKCanvas**, donc d'Unkeny. | Oui sur le papier, mais **`Exec` y est mêlé aux types** — le piège que NKGraph a nommé (`NkNodeGraph.h:187-200`, cas `exec/le-piege-du-type-exec`). Au mieux une liste d'idées de nœuds. |
| **VM de NKSL** | `Kernel/Runtime/NKSL/src/NKSL/VM/` (`NkSLVM.cpp` 570, `NkSLByteCode.h` 236, `NkSLByteCodeIO.cpp` 271) | Vraie VM à registres, bytecode sérialisé. | Réelle. | Valeurs = vecteurs de flottants (`NkSLValue{float v[16]}`), orientée shader. **L'expérience se reprend, pas le code** (`Engine/Noge/ROADMAP.md:2024-2034`). |
| **Spécifications d'éditeur nodal** | `NKGraph/SPECIFICATION_VISUELLE.md` (3 824 l. ; § 7.7 nœud d'exécution, § 12 canevas), `CATALOGUE_NOEUDS.md` (451), `DESIGN_EDITEUR_NODAL.md`, planches `references/planche_0*` (dont `planche_08_execution`) ; `NK3DModeler/docs/UI_SPEC.md` § 10ter (couleurs par famille, fil exec blanc 3 px) ; `NKUIDesign/2_NkUIDesign_Langage_Description_NodeBlueprint.md` (1 726 l.) | Le **dessin** et le vocabulaire. | Documents. | — |
| **NKEditorKit** | `Engine/NKEditorKit/src/NKEditorKit/` | Arbre, navigateur de contenu, onglets, champ de saisie, barre de défilement, thème. **Aucun composant de graphe.** | — | — |
| NkAnima, NKUI | `NKAnima/Clip/NkAnimation.h` (HFSM et arbres de mélange, sans éditeur) ; `NKUI/ROADMAP.md:174` (`NkUINodeGraph` en projet) | — | — | — |

### 2.2 Ce qu'on retient

- **Couche 1 : NKGraph**, sans discussion — c'est la règle du dépôt, répétée
  dans `Engine/Unkeny/README.md:52` (« graphe de nœuds → `Kernel/Runtime/NKGraph` »)
  et dans l'en-tête de `NkNodeGraph.h:8-27`. Il a déjà ce qu'un graphe
  d'exécution demande : deux familles de prises aux arités inversées, le refus
  nommé du fil croisé, l'acyclicité qui oblige boucles et machines à états à
  être des **nœuds**.
- **Couche 3 : écrite pour Unkeny**, sur le **patron de NkMatGraph** (registre
  de prototypes, requête « nœuds pour cette prise », validation de domaine,
  passe de compilation) et avec le **précédent de NKGui** (un graphe d'exécution
  compilé vers une forme non nodale, un seul évaluateur).
- **Couche 2 : à écrire**, dans `Engine/NKEditorKit` (règle des trois couches),
  dessinée avec `nkgui::NkGuiDrawList` puisque c'est ce que peint UnkenyEditor
  (NKCanvas + NKGui, `UnkenyEditor.jenga:74-82` ; ni NKUI ni NKRenderer). Le
  dessin suit `SPECIFICATION_VISUELLE.md` et les planches ; **ne pas le
  réinventer**.
- **Pas de troisième modèle de graphe.** Le blueprint de NKGui reste un outil
  d'interface ; celui de Noge sera réaligné sur NKGraph le jour où Noge s'en
  servira (et pourra alors reprendre la VM d'Unkeny, § 4.8).

### 2.3 Ce qui manque à NKGraph pour un graphe d'exécution

Trouvé **à la lecture** ; aucun cas existant ne le couvre (⚪ rien n'a été exécuté).

| # | manque | où | bloque ? |
|---|---|---|---|
| G1 | `Validate()` **ignore la famille** : il signale `LinkDuplicateTarget` sur une entrée exec à plusieurs sources — que `Connect` autorise expressément — et applique `Accepts()` (contrôle de **type**) aux liens exec. Un graphe Blueprint valide **se relirait avec de faux diagnostics**. | `NkNodeGraph.inl:993-1003` | **Oui, dès S2.** À corriger dans le cœur, avec son cas dans `NkMatGraphCheck` (`exec/validate-connait-la-famille`), et sa contre-épreuve. |
| G2 | Grouper / dégrouper **perd la famille** : `AddSocket` y est appelé sans elle, donc `Data` par défaut. Une fonction Blueprint faite par regroupement changerait ses prises exec en prises de données **sans rien dire**. | `NkGraphGroup.h:250`, `NkGraphGroup.inl:130, 345, 350, 405, 407` | Non avant S7 (fonctions). |
| G3 | `IncomingOf(node, socket)` ne rend que le **premier** lien entrant. | `NkNodeGraph.h:535` | Non : le compilateur marche **en avant** le long des sorties exec (une suite chacune) ; il n'a jamais à remonter une entrée exec. |
| G4 | **Aucun canevas**, et `NkGuiDrawList` n'a **pas de Bézier** (lignes, polylignes, rectangles arrondis, cercles, triangles, polygones convexes, texte). Les fils seront échantillonnés en `AddPolyline`. | `NKGraph/ROADMAP.md:63` (P4 ❌) | Oui pour S4. |
| G5 | Pas de copier/coller dans le cœur (faisable sur `Serialize` ou sur l'aide de copie de `NkGraphGroup`). | — | Non (confort, S4). |
| G6 | NKGraph n'a pas de cible jenga. | `NKGraph/ROADMAP.md` « Dépendances » | Non : Unkeny et l'éditeur ajoutent le chemin d'inclusion comme `NkMatGraphCheck.jenga:54`. Le ROADMAP dit que la cible deviendra légitime « quand un consommateur réel la liera » — **ce jour arrive avec S2**. |

---

## 3. L'API de jeu qu'appelleront les scripts (ce qui existe ce soir)

Unkeny est jeune (≈ 7 100 lignes, `Engine/Unkeny/src`) et **n'a aucun script**.
Tout ce qui suit existe et est éprouvé par son banc (`NkUnkenyBanc.cpp` : t, s,
a, j) ou par celui de l'éditeur (e1–e12) — sauf mention.

### 3.1 Entités et composants

- `NkScene::Creer(nom, position)`, `Detruire(id)` (détruit aussi le corps
  physique et la matière, `NkUnkenyScene.h:132-137`), `Monde()` (le `NkWorld`
  de NKECS en accès direct), `Entites(out)`, `TeleporterEntite`.
- Composants de **données pures** (`NkUnkenyComposants.h`) : `NkTransform2D`,
  `NkSprite2D`, `NkCollisionneur2D` (couche, masque, `declencheur`), `NkCorps2D`
  (porte un **identifiant** de corps, jamais la position), `NkCorpsMou2D`
  (idem), `NkEtiquette` (`char nom[32]`), `NkVitesse2D` ; ailleurs
  `NkAnimSprite2D`, `NkSource2D`.
- **La règle du fichier** (`NkUnkenyComposants.h:31`) : « un comportement →
  un système, jamais un composant ». Le composant de script (§ 4.2) la respecte :
  il ne porte que des **données** (quel script, ses variables) ; ce qui
  s'exécute est un **système**.
- ⚠️ **Les identifiants d'entité ne sont PAS stables** : `Restaurer` les refait
  (`NkUnkenyScene.h:341`), le chargement aussi (`NkUnkenySauvegarde.h:34`). Une
  variable de script de type « entité » ne peut donc **pas** être sauvée telle
  quelle (manque M3).

### 3.2 Systèmes et phases

- `NkScene::AjouterSysteme(nom, phase, fonction, donnees, ordre)` avec
  `NkFonctionSysteme = void (*)(NkScene &, float32 dt, void *donnees)`
  (`NkUnkenyScene.h:100`) — un **pointeur de fonction C et un `void *`** : la
  forme qu'il faut pour brancher un hôte de scripts, sans `std::function`.
- Deux phases : `NK_PAS_FIXE` (à chaque pas fixe, **avant** la physique) et
  `NK_TRAME` (une fois par `Pas`, après physique, synchronisation et
  animations). Planificateur **séquentiel**, sans allocation par trame, qui
  survit à un système qui se retire pendant le parcours (banc j3).
- Les systèmes **survivent** à `Init` (`Liberer` ne vide pas `mSystemes`,
  `NkUnkenyScene.cpp:120-141`) — donc à `NkChargerScene`, qui ré-initialise la
  scène : l'hôte s'enregistre **une fois**.
- ⚠️ `ApplyForce` s'accumule et **`Step` remet la force à zéro**
  (`NkIntegrator.cpp:30-31`). Une force posée en `NK_TRAME` ne s'applique qu'au
  premier pas fixe de la trame **suivante** — et à un seul des deux pas quand le
  jeu tourne à 30 i/s. Une force continue ne se pose donc **qu'en pas fixe**
  (conséquence au § 7 : le nœud « Appliquer une force » n'est valide que sous
  l'événement « Pas fixe »).

### 3.3 Contacts et zones

- `NkScene::Contacts()` (`NkUnkenyScene.h:235`) : les chocs et entrées/sorties
  de zone du **dernier** `Pas` (tous ses pas fixes réunis), **en entités** —
  `NkContact2D { a, b, phase NK_DEBUT|NK_FIN, declencheur }`, `a` étant la zone
  quand `declencheur` est vrai. Bancs j4, j4n, j5.
- ⚠️ Une entité détruite pendant le pas peut y figurer : tester `IsAlive`.
- ⚠️ **Les corps mous n'y sont pas** : `Relever()` ne lit que les quatre listes
  du monde rigide (`NkUnkenyScene.cpp:479-517`). Manque M5e.

### 3.4 Physique rigide

- Existant : `AjouterCorps`, `ActualiserCorps`, `RetirerCorps`, `PoserVitesse`,
  `Vitesse`, et `MondePhysique()` → `NkRigidBody::ApplyForce`,
  `ApplyImpulse`, `ApplyForceAtPoint` (`NkRigidBody.h:71-86`, en `NkVec3f`),
  `Raycast`, `OverlapShape` (`NkPhysicsWorld.h:89-94`, en 3D).
- **Manque M1** : `NkScene::AppliquerForce / AppliquerImpulsion` en 2D. Sans
  elles, chaque appelant fait la conversion z = 0 et le passage
  entité → `corpsId` → `NkRigidBody`, alors que l'en-tête des composants exige
  que le pont se fasse « en UN seul endroit ».

### 3.5 Corps mous — personnages et parties de personnage (demande de Rihen)

Tout passe par `physics::NkParticules2D` (`Kernel/Runtime/NKPhysics/src/NKPhysics/NkParticules2D.h`),
que la scène tient (`NkScene::Particules()`) ; l'entité porte `NkCorpsMou2D::corpsId`,
**identifiant stable** du corps (`NkCorpsP2D::id`, `.h:140`), et
`NkScene::EntiteDuCorpsMou(corpsId)` fait le chemin inverse.

**Ce qui existe et que le premier lot expose tel quel :**

| fonction | ligne | ce qu'elle fait vraiment |
|---|---|---|
| `CentreCorps(ci)`, `VitesseCorps(ci)`, `BoiteCorps(ci, mn, mx)` | `.h:241-243` | moyennes / boîte des particules du corps |
| `AppliquerVitesse(ci, v)` | `.cpp:1567-1574` | ⚠️ **REMPLACE** la vitesse de chaque particule libre par `v` |
| `EpinglerCorps(ci, bool)` | `.cpp:1586-1594` | fige toutes les particules **dans le monde** (masse inverse nulle) |
| `Translater(ci, delta)` | `.cpp:1559-1565` | déplace sans vitesse |
| `SaisirDebut(p, rayon)` / `SaisirVers(p)` / `SaisirFin()` | `.cpp:1606-1644` | saisie « à la souris » |
| `Explosion`, `Aimant`, `Couper`, `Gommer` | `.h:252-257` | effets de zone |

⚠️ **Toutes prennent un INDEX de corps (`ci`), pas l'identifiant** : l'index
bouge à la compaction (`NkParticule2D::corps`, `.h:112`). L'hôte convertit à
chaque appel par `IndexCorps(id)` (recherche linéaire, `.cpp:1385-1395`) ; un
script ne voit **jamais** un index.

**Ce qui manque pour qu'un corps mou soit un personnage** (manques M5, tous dans
NKPhysics sauf M5e qui touche aussi Unkeny) :

| # | manque | mesuré à la lecture |
|---|---|---|
| **M5a** | **« au sol »** — un personnage ne saute que s'il touche le sol. | Chaque particule porte `contact` et `normale` (`.h:106, 116`), mais **`contact` est remis à faux à la fin de chaque sous-pas** (`MajVitesses`, `.cpp:1137` et `:1159`). Après `Pas`, il n'en reste rien. Il faut un **résumé par corps**, accumulé pendant le pas avant la remise à zéro : nombre de particules en contact, normale moyenne, avec quoi (sol de la boîte, statique, rigide). |
| **M5b** | **pousser sans figer la forme**. | `AppliquerVitesse` **écrase** la vitesse de chaque particule : appelée à chaque pas pour marcher, elle efface toute déformation — le blob glisse comme un bloc. Il faut `AjouterVitesse(ci, dv)` et `AppliquerForce(ci, f, dt)` (accélération répartie selon la masse), et un **couple** (rouler). |
| **M5c** | **contrôle par PARTIE** (un bras, le bas d'un anneau). | Aucune notion de sous-ensemble : l'unité est le corps. Les indices de particules changent à la compaction, et `Gommer` peut retirer des particules libres une à une d'un blob. Deux réponses possibles, **à trancher** (Q6) : (a) une *partie* = un ensemble **nommé** d'indices **relatifs** au corps, déclaré à la fabrication (tient pour grilles et anneaux, pas pour ce qui se gomme) ; (b) un personnage = **plusieurs corps reliés** — ⚪ `Relier(a, b, repos)` (`.cpp:247`) accepte deux particules quelconques, mais la tenue d'un lien **entre deux corps** à la compaction n'a pas été vérifiée. |
| **M5d** | **saisie par corps, et plusieurs à la fois**. | La saisie est **unique pour tout le monde** (`mSaisis` ; `SaisirDebut` commence par `SaisirFin`) et prend **toutes** les particules de **tous** les corps dans le rayon. Deux doigts, deux joueurs, ou « saisir MON personnage sans prendre le voisin » sont impossibles. Il faut `SaisirCorps(ci, point, rayon, pointeur)` et une saisie par pointeur. |
| **M5e** | **les contacts d'un corps mou**. | Aucun événement mou ↔ rigide, mou ↔ zone, mou ↔ mou : le contact avec un rigide est résolu particule par particule (`.cpp:1025-1037`, compteur `stats.contactsRigides`) sans dire **quel** rigide. Il faut une liste d'événements par paire (corps mou, corps rigide) DÉBUT/FIN, que `NkScene::Relever` traduise en `NkContact2D` comme le reste. |
| **M5f** | **attacher à un rigide** (un ventre mou sur un squelette rigide, une arme rigide dans une main molle). | Seul `epingle` existe, et il fige **dans le monde**. Il faut une **attache particule ↔ corps rigide** (ancre locale au rigide), résolue avec les contacts rigides (`ResoudreRigides`), dont les impulses repartent vers le rigide. |

Et côté entrée, pour la souris et le doigt : **manque M2b**, un état de
**pointeurs** (identifiant, position **monde**, enfoncé, vient d'être pressé) —
une action (`NkAction`, un axe) ne porte pas de position.

### 3.6 Entrée : des ACTIONS, jamais des touches

- `NkActions` (`Entree/NkUnkenyActions.h`) : 32 actions (`:45`), chacune un
  **axe** −1..1, trois états `Enfoncee` / `VientDEtrePressee` /
  `VientDEtreRelachee` (`:109`), `Direction(axeX, axeY)` normalisée (`:120`),
  `NouvelleTrame()` à appeler avant d'écrire (`:76`). Le jeu définit ses
  indices.
- **La consigne de Rihen, appliquée** : l'« Événement Touche » demandé devient
  **« Événement Action »**. Aucun nœud, aucune fonction de la table ne lit une
  touche. C'est ce qui rend un script jouable au clavier, à la manette et au
  doigt sans être réécrit — et **éprouvable sans fenêtre** (l'en-tête de
  `NkUnkenyActions.h` le dit).
- ⚠️ **Mesuré** : `NkActions` n'est utilisé **nulle part** hors de son en-tête
  (`Applications/`, `Engine/`). L'éditeur n'en a pas. **Manque M2** : un
  contexte de jeu qui porte les actions, leurs **noms** (« Sauter » → 3, pour
  qu'un nœud désigne une action par son nom), et une table de
  **correspondances** entrée → action (clavier, manette, doigt) dont l'éditeur
  et chaque jeu ont besoin — deux consommateurs, la règle d'Unkeny est
  satisfaite.

### 3.7 Son

- `NkSons2D` (`Son/NkUnkenySon.h`) : `Charger(chemin)` (même chemin, même id,
  `:91`), `Trouver(nom)` (`:94`), `Jouer(son, volume, pitch, pan, boucle, bus)`
  (`:103`), `JouerA(son, position, camera, volume, portee)` (`:106`),
  `Couper(voix)`. Composant `NkSource2D` avec `demande` / `arret` (un script peut
  aussi passer par lui). Sans carte son, tout reste appelable (banc a3).
- ⚠️ **Un son se désigne par son NOM dans un script**, jamais par son
  identifiant : l'id dépend de l'ordre de chargement de la session.
- ⚠️ **Constat en passant (⚪ non éprouvé par exécution)** : `NkSource2D` est
  sauvé par `PhotographierAussi<NkSource2D>("NkSource2D")`
  (`NkUnkenyScene.cpp:76`), donc en **octets** — `son` compris (`NkUnkenySon.h:52`).
  Après réouverture dans une autre session, l'id sauvé peut désigner un autre
  son, ou aucun. Le banc a6 vérifie Photographier/Restaurer **en mémoire**, pas
  l'aller-retour par fichier entre deux sessions. **Manque M4** (hors scripts,
  mais les scripts en hériteraient).

### 3.8 Animation

- `NkAnimSprite2D::Jouer(clip)` (`Anim/NkUnkenySpriteAnim.h:91`, ne redémarre
  pas le clip courant — voulu), 8 clips au plus, `evenementAtteint` (`:85`) :
  **signal d'une trame que le jeu doit remettre à faux**. L'hôte en fera un
  événement « Image d'animation » (S7) et le remettra lui-même.

### 3.9 Temps

- Le `dt` vient des systèmes (`NK_PAS_FIXE` : le pas fixe ; `NK_TRAME` : la
  trame). La scène ne tient **pas** de temps total : l'hôte l'accumulera pour le
  nœud « Temps écoulé ».

### 3.10 Sauvegarde `.nkscene`

- JSON par NKSerialization (`NkUnkenySauvegarde.h`), `"format": "unkeny.scene"`,
  `"version": 1`, entités écrites **champ par champ** ; les composants déclarés
  par `PhotographierAussi<T>("Nom")` sont écrits en **octets hexadécimaux** dans
  `"jeu"`, dépendants de la plateforme (`:43`) ; une lecture accepte toute
  version ≤ la sienne et laisse la scène **intacte** en cas d'échec (banc s3).
- Le chargement **passe par la photo** (`NkUnkenySauvegarde.cpp:1-8`) : un seul
  chemin de reconstruction.
- **Ce que le composant de script y ajoute** est au § 6.3.

### 3.11 Les manques du moteur, en une liste

| # | manque | palier |
|---|---|---|
| M1 | `NkScene::AppliquerForce` / `AppliquerImpulsion` en 2D | S0 |
| M2 | contexte de jeu : actions + noms d'actions + correspondances d'entrée ; M2b : pointeurs (souris, doigts) en coordonnées monde | S0 |
| M3 | identité **stable** d'entité (sauvée), pour les références entre entités | S7 |
| M4 | `NkSource2D::son` sauvé en octets (⚪ à confirmer par un banc) | S0 (même chantier que le son par nom) |
| M5a–f | corps mous : au sol, poussée, parties, saisie par corps, contacts, attache | S3 |
| M6 | `evenementAtteint` rendu en événement par l'hôte | S7 |
| M7 | requêtes 2D (rayon, recouvrement) au niveau de la scène | S7 |

---

## 4. L'architecture

### 4.1 Vue d'ensemble

```
 UnkenyEditor (Applications/)                     Unkeny (Engine/) — tourne partout
 ───────────────────────────                      ─────────────────────────────────
 canevas nodal (NKEditorKit, couche 2)            NkHoteScripts2D  (2 systèmes de la scène)
   │ édite                                          │  Début · Pas fixe · Tick · Contact · Zone · Action
   ▼                                                │  état d'exécution par entité (hors composant)
 NkNodeGraph (NKGraph, couche 1)                    │
   │ compile (catalogue + compilateur Blueprint)    ├──► NkMachineBp ── lit ──► NkModuleBp (octets)
   ▼                                                │         │
 .nkbp = GRAPHE (autorat) + MODULE (exécution) ─────┘         │ appelle
                                                              ▼
 NKCode / éditeur de texte                          ┌── NkUnkHoteV1 : UNE table C ──┐
   │ Joueur.cpp                                     │  entités · rigide · corps mou │
   ▼                                                │  actions · pointeurs · son    │
 compilateur (clang++ / jenga)                      │  animation · journal · temps  │
   │                                                └───────────────▲───────────────┘
   ▼                                                                │ appelle
 Scripts.dll (éditeur, rechargée)  ── ou ──  lié en statique (livraison) ─┘
```

**Le point qui tient tout** : Blueprint et C++ appellent **les mêmes
fonctions**. La VM n'a pas d'accès privilégié au moteur : ses natifs sont les
entrées de la table. Un nœud « Appliquer une impulsion » et
`hote->AppliquerImpulsion(...)` sont **le même code**, donc le même
comportement, éprouvé une fois (banc c1 ≡ b1, § 9).

### 4.2 Le composant et le registre des scripts

```cpp
// Engine/Unkeny/src/Unkeny/Script/NkUnkenyScript.h   (PROPOSITION)
static const int32 NK_UNKENY_SCRIPT_VARS_MAX = 16;

/// Une valeur de variable : reel, entier, booleen, vec2, son, action.
/// 8 octets, et le TYPE vient du module, jamais de la valeur elle-meme.
struct NkValeurScript {
		uint8 octets[8] = {};
};

/// Le composant. DONNEES SEULEMENT, copiable bit a bit.
struct NkScript2D {
		/// Identifiant NkScripts2D (0 = aucun). C'est le NOM qui est sauve.
		uint32 script = 0;
		bool actif = true;
		uint8 nbVars = 0;
		NkValeurScript vars[NK_UNKENY_SCRIPT_VARS_MAX];
};
```

- **Copiable bit à bit** (`static_assert`) : la photo de Jouer/Arrêter le porte
  sans code nouveau, et **Arrêter rend les variables d'avant Jouer** —
  exactement ce qu'attend un utilisateur d'UE5.
- **Un script par entité** (Q1) : la forme d'un Blueprint d'acteur. Plusieurs
  comportements se composent par plusieurs entités, ou dans un même graphe.
- **Ce qui n'y est PAS, et pourquoi** : « déjà démarré », « en faute », les
  instances C++, les attentes (S7). Ce sont des états **d'exécution** : l'hôte
  les tient dans sa propre table, **indexée par `NkEntityId::Pack()`** et
  purgée par `IsAlive`. S'ils étaient dans le composant, la photo les
  recopierait : après Arrêter puis Jouer, « démarré » vaudrait vrai et
  **Début ne passerait plus jamais**. Comme les identifiants changent à
  `Restaurer`, une entité restaurée est **nouvelle** pour l'hôte, et reçoit son
  Début.
- `NkScripts2D` : le registre des définitions chargées, **nom ↔ identifiant**,
  sur le modèle de `NkTextures2D` (« même nom, même identifiant »). Un nom est
  soit un chemin relatif au projet (`Scripts/Joueur.nkbp`), soit une classe C++
  (`cpp:Joueur`).

### 4.3 La table C : `NkUnkHoteV1`

Un en-tête **autonome** (`<stdint.h>` seul), dans la ligne de `NkScriptABI.h`
de Noge : un module C++ l'inclut et **ne lie aucune bibliothèque du moteur**.

```cpp
// Engine/Unkeny/src/Unkeny/Script/NkUnkenyScriptABI.h   (PROPOSITION, extrait)
extern "C" {
	typedef struct NkUnkEntite {
			uint64_t pack; ///< NkEntityId::Pack() ; 0 = aucune
	} NkUnkEntite;

	typedef struct NkUnkHoteV1 {
			uint32_t taille;     ///< sizeof de l'HOTE : ce qui depasse n'existe pas
			uint16_t abiMajeure; ///< differente = module refuse
			uint16_t abiMineure; ///< ajouts EN FIN de structure seulement
			void *ctx;           ///< le contexte de jeu, opaque

			// Chaque fonction rend 1 (fait) ou 0 (refuse : entite morte, composant
			// absent, valeur non finie). Les resultats sortent par pointeur.
			int32_t (*Vivante)(void *ctx, NkUnkEntite e);
			int32_t (*Detruire)(void *ctx, NkUnkEntite e);
			int32_t (*CreerActeur)(void *ctx, uint32_t acteur, float x, float y, NkUnkEntite *sortie);
			int32_t (*Position)(void *ctx, NkUnkEntite e, float *x, float *y);
			int32_t (*AppliquerImpulsion)(void *ctx, NkUnkEntite e, float ix, float iy);
			int32_t (*MouAjouterVitesse)(void *ctx, NkUnkEntite e, float dvx, float dvy);
			int32_t (*ActionParNom)(void *ctx, const char *nom, int32_t *sortie);
			float (*ValeurAction)(void *ctx, int32_t action);
			int32_t (*JouerSon)(void *ctx, uint32_t son, NkUnkEntite ou, float volume);
			void (*Afficher)(void *ctx, NkUnkEntite soi, const char *texte);
			// ... la liste complete suit le premier lot (§ 7)
	} NkUnkHoteV1;
}
```

Les règles, et d'où elles viennent :

1. **Aucun pointeur vers le moteur ne traverse** : ni `NkWorld *`, ni pointeur
   de composant (il bougerait au premier changement d'archétype), ni
   `NkVector`/`NkString`. Des **valeurs** et des **poignées vérifiées**. C'est
   la première protection contre le plantage (§ 8), et la seule qui marche
   aussi pour le C++.
2. **Pourquoi une table C et pas l'API C++ d'Unkeny** — mesuré : NKECS attribue
   les identifiants de composant par des **variables `inline` d'en-tête**
   (`gNextComponentId`, `ComponentIdStorage<T>::value`, `NkTypeRegistry.h:192-198`)
   et un singleton statique de fonction (`Global()`, `:213-216`). Une DLL qui
   lie NKECS en a **sa propre copie** sous Windows : son `Get<NkTransform2D>`
   calculerait un autre identifiant que l'éditeur et lirait **la mauvaise
   colonne**, sans erreur. Même chose pour l'allocateur (`gDefaultAllocator`,
   `NkAllocator.cpp:1235-1241`, que `NkVector` et `NkString` mémorisent) et le
   journal (`NkLog::Instance()`). ⚪ Duplication non éprouvée dans le dépôt —
   c'est le comportement connu du format PE, et ConquerorLab la subit déjà
   (bruit « duplicate section … has different size » filtré,
   `NkcModuleCompiler.h:132-164`).
3. **Pas de struct rendue par valeur** à travers `extern "C"` : la règle de
   Noge (`NkScriptABI.h`, `nkecs_get_factory` remplit un paramètre de sortie).
4. **Version MAJEURE / MINEURE**, ajouts **en fin de la dernière structure
   seulement** : la leçon payée par ConquerorLab (`ConquerorRulesABI.h:45-75`,
   07/08 — un seul numéro refusait des modules qui tournaient parfaitement).
5. **Un seul point d'entrée d'événement par classe**, `Evenement(instance,
   const NkUnkEvenementV1 *)`, plutôt qu'une vtable d'un pointeur par événement
   (celle de Noge en a seize) : un événement de plus ne change pas la vtable,
   seulement un champ `genre`.

### 4.4 L'hôte : `NkHoteScripts2D`

- Il s'enregistre **deux fois** dans la scène : `"scripts.pas_fixe"`
  (`NK_PAS_FIXE`, ordre −100 : avant les systèmes du jeu) et `"scripts.trame"`
  (`NK_TRAME`).
- Il reçoit un **contexte de jeu** (M2) :

```cpp
// PROPOSITION
struct NkContexteJeu2D {
		NkScene *scene = nullptr;
		NkActions *actions = nullptr;
		const NkNomsActions *nomsActions = nullptr; ///< « Sauter » -> 3
		NkPointeurs2D *pointeurs = nullptr;         ///< souris, doigts (M2b)
		NkSons2D *sons = nullptr;                   ///< peut etre inactif : tout reste appelable
		NkTextures2D *textures = nullptr;
		NkScripts2D *scripts = nullptr;
		NkJournalScripts *journal = nullptr;         ///< « Afficher » et les fautes
};
```

- **À chaque passage**, il **photographie la liste** des entités qui portent un
  `NkScript2D` actif (un tableau membre réutilisé : aucune allocation par
  trame), **puis** appelle les scripts **hors de toute requête NKECS**. Un script
  peut donc **créer et détruire immédiatement** — l'entité créée est utilisable
  dans le même événement, comme un *SpawnActor* — sans invalider un parcours en
  cours. Chaque appel est précédé d'un `IsAlive` : une entité détruite par un
  script précédent n'est plus appelée.
- **Début passe avant tout autre événement de l'entité, quelle que soit la
  phase** : si le premier passage de la trame est un pas fixe, c'est lui qui
  distribue Début. Une entité créée par un script reçoit le sien au passage
  suivant, avant son premier Tick.
- **Ordre entre entités** : celui de la liste (l'ordre de la scène), **pas un
  contrat**. Un script qui dépend de l'ordre d'un autre est un défaut à
  signaler, pas à garantir.
- **Arrêter** (éditeur) : l'hôte distribue « Fin » (S7), détruit les instances
  C++ et vide sa table d'exécution, **puis** l'éditeur appelle `Restaurer`.

### 4.5 L'ordre d'une trame, scripts compris

```
1. le jeu (ou l'éditeur en JEU) : actions.NouvelleTrame() ; entrées -> actions, pointeurs
2. scene.Pas(dt)
     tant que le retard le permet (pas fixe) :
        systeme "scripts.pas_fixe"  -> Debut des nouveaux, puis « Pas fixe » (FORCES)
        systemes du jeu (NK_PAS_FIXE)
        particules, puis rigides (Step), puis Relever (contacts)
     synchronisation physique -> transforms ; vitesses manuelles ; animations
     systeme "scripts.trame"        -> Debut des nouveaux, Contact / Zone, Action, « Tick »
     systemes du jeu (NK_TRAME)
3. sons.Avancer(scene)
4. dessin (lit, ne modifie rien)
```

⚠️ **Une conséquence à écrire dans l'aide du nœud** : une **impulsion** posée
sur « Action Sauter pressée » (en `NK_TRAME`) est intégrée au pas fixe suivant —
au plus un pas de retard (16 ms à 60 Hz). Une **force continue** se pose dans
« Pas fixe » en lisant l'axe d'action, pas dans un événement.

### 4.6 La VM Blueprint : `NkModuleBp` et `NkMachineBp`

- **Le module** (§ 6.2) s'écrit, se lit et se vérifie **sans NKGraph** : c'est
  la contrainte du 24/08, et elle se prouve par le banc b1 (un module écrit à
  la main). Le runtime d'un jeu livré n'inclut ni NKGraph ni le compilateur.
- **La machine** : un interpréteur à **registres typés** (le modèle de la VM de
  NKSL, sans son code), un cadre par appel d'événement, des variables d'instance
  lues et écrites **dans le composant**. Jeu d'instructions du premier lot :
  constantes, copie, lecture/écriture de variable, arithmétique (réel, entier,
  vec2), comparaisons, logique, sauts **en avant** (Si, Séquence) et **en
  arrière réservés aux nœuds de boucle** (S7), appel de natif, fin.
- **Les natifs sont les fonctions de `NkUnkHoteV1`**, décrites une fois dans
  une table (`nom qualifié, types des paramètres, types des résultats,
  événements autorisés`) que lisent **à la fois** le vérificateur de la VM et le
  compilateur de l'éditeur. Le module les désigne **par NOM**
  (`"unkeny.corps.impulsion"`), résolus au chargement : la leçon de NKGraph sur
  l'index contre le nom (`NkNodeGraph.h:261-303`), appliquée d'emblée.
- **Sans fil, sans JIT, sans allocation par événement** : Web et iOS
  l'acceptent tels quels.

### 4.7 Côté éditeur

| brique | où (proposé) | rôle |
|---|---|---|
| **canevas nodal** | `Engine/NKEditorKit/src/NKEditorKit/Components/` (couche 2) | pan/zoom, nœuds, prises, fils, sélection, recherche de nœud **depuis une prise tirée**, commentaires ; annuler/refaire par `NkGraphHistory`. Générique : les matériaux s'en serviront aussi. |
| **catalogue Blueprint** | `Applications/UnkenyEditor/src/Script/NkBpCatalogue.*` | prototypes des nœuds du § 7, types enregistrés dans NKGraph, patron `NkMatRegistreProtos` |
| **compilateur** | `Applications/UnkenyEditor/src/Script/NkBpCompilateur.*` | graphe → module : marche en avant depuis chaque événement le long des fils exec, évalue les nœuds purs à la demande (tri topologique de NKGraph), vérifie la validité par événement (« force hors pas fixe »), écrit la table de lignes pc → nœud |
| **panneau du graphe** | `Applications/UnkenyEditor/src/Editeur/NkEditeurGraphe.cpp` | un onglet par `.nkbp` ouvert, à côté du viseur (disposition UE5 déjà en place, `NkEditeurInterface.h`) |
| **Détails** | `NkEditeurDetails.cpp` (existant) | « Ajouter un composant > Script » (Blueprint ou classe C++), **variables exposées** éditables, bouton « Ouvrir le graphe » / « Ouvrir dans NKCode » |
| **Journal** | `NkEditeurTiroir.cpp`, onglet Journal (existant, `:299`) | « Afficher », erreurs de compilation, fautes ; une ligne qui désigne un nœud l'ouvre **surligné** |
| **scripts C++** | `Applications/UnkenyEditor/src/Script/NkEditeurModulesCpp.*` | compiler, charger, recharger (§ 5) |

⚠️ **Pourquoi le compilateur est dans l'éditeur et pas dans Unkeny** : le jeu
livré n'en a pas besoin, et le moteur éprouve sa VM avec des modules écrits à la
main. Le jour où un second outil (NKCode, une ligne de commande de cuisson)
voudra compiler un `.nkbp`, il descendra — la règle des deux consommateurs.

⚠️ **Pendant JEU, le viseur capture le clavier** et le traduit en actions (M2) ;
**Échap** rend la main à l'éditeur. Sans cela, « Espace » sauterait ET
déclencherait un raccourci de l'éditeur.

### 4.8 Où vivra la VM à terme

La VM ne dépend d'**aucun type d'Unkeny** : ses natifs lui sont donnés par une
table. Elle vit **d'abord dans Unkeny** (premier consommateur, règle n° 2 de
NKGraph) et **descendra dans le Kernel** quand Noge sera le second — Noge dont le
`NkBlueprint` doit de toute façon être « réaligné, pas recopié »
(`NKGraph/ROADMAP.md:365-372`). Écrite ainsi, la descente est un déplacement de
fichiers, pas une réécriture.

---

## 5. Le C++ rechargé à chaud, avec CE dépôt

### 5.1 Ce qui est déjà prouvé — et ce qui ne l'est pas

- **Prouvé (ROADMAP de Noge, 24/07)** : chargement réel, copie fantôme
  Windows, capture/restauration d'état, ancienne version gardée si la nouvelle
  échoue, hooks d'allocation injectés (`nkecs_set_allocator`). Windows,
  clang-mingw, Release.
- **Pas prouvé** : le chemin POSIX (`dlopen`, écrit, jamais exécuté) ; l'accès
  aux composants depuis la DLL (le monde y est **opaque** — `NkScriptABI.h`
  n'a **aucune table de fonctions hôte** : un script Noge ne peut rien faire au
  monde). **`NkUnkHoteV1` est la moitié qui manque à Noge.**
- ⚪ Rien de cela n'a été ré-exécuté pour ce document.

### 5.2 Ce qu'on reprend, et d'où

| besoin | reprise | source |
|---|---|---|
| ouvrir / symbole / fermer, Windows + POSIX | tel quel | `NkScriptBridge.cpp:49-82` |
| **copie fantôme** (Windows verrouille une DLL chargée) | tel quel, **généralisé** : un nom par génération (`Scripts.gen7.dll`) sur **toutes** les plateformes | `NkScriptBridge.cpp:119-130` |
| surveillance par date de modification | tel quel | `NkScriptBridge.cpp:275-288` |
| charger la nouvelle **avant** de décharger l'ancienne | tel quel | `NkScriptBridge.cpp:290-311` |
| état : capture → destruction → déchargement → rechargement → restauration | adapté (§ 5.5) | `NkScriptBridge.cpp:313-370` |
| ABI majeure/mineure, queue de table nulle | tel quel | `ConquerorRulesABI.h:45-75` |
| statique sur Android/Web, mêmes sources | adapté (iOS et HarmonyOS en plus) | `NkcModuleHost.h:43-51`, `ConquerorLab.jenga:66-69` |

⚠️ **Pourquoi un nom par génération même sous Linux** : réécrire en place un
`.so` encore projeté en mémoire peut faire tomber le processus, et `dlopen` d'un
chemin déjà ouvert rend l'ancien module. ⚪ Comportements connus de la
plateforme, **non éprouvés dans le dépôt**.

📌 **C'est le troisième chargeur du dépôt** (Noge, ConquerorLab, Unkeny) — et
il n'existe aucune classe commune (aucun `NkDynamicLibrary` / `NkModuleLoader`
dans le Kernel). La règle d'Unkeny (« commencer par la couche du dessous »)
demande de l'**extraire** à cette occasion : `NkModuleDynamique` (ouvrir,
symbole, fermer, copie par génération, date) dans `Kernel/System`, premier
consommateur Unkeny ; Noge et ConquerorLab y migreront **à leur rythme** — ce
document ne touche pas à leur code.

### 5.3 Construire le module

**Ce que jenga sait faire** (`C:\Users\rihen\Documents\Projects\Jenga`, version
2.8.6, installée en mode éditable) :

- `sharedlib()` existe (`Jenga/Core/Api.py:1581-1582`) ; sous clang-mingw le
  lien est `clang++ -o X.dll -shared …` (`Core/Builders/Windows.py:550-574`),
  **sans** bibliothèque d'import (`--out-implib`), sans `-fPIC`, sans
  visibilité ; sous Linux `.so` avec `-shared` et `-fPIC` (`Linux.py:147-148,
  245-247`), `rpath $ORIGIN` ; macOS `.dylib`, `-dynamiclib`.
- **Le genre se change par plateforme** : `with filter("system:Web"):
  staticlib()` est pris en compte (`Api.py:1563-1570`, appliqué dans
  `Builder._ApplyProjectFilters`) — UnkenyEditor le fait déjà pour
  `consoleapp()` sur le Web.
- Ligne de commande : `jenga build --target X --config Debug --platform Windows`
  (`Jenga/Commands/Build.py:525-571`), sorties sous
  `Build/Bin/<Config>-<Système>/<Projet>/` et `Build/Lib/<Config>-<Système>/`.

**Ce qui n'a jamais servi, et donc bloque en l'état :**

- 🔴 **Aucun projet du dépôt n'a jamais été construit en `sharedlib()`** (les
  seules mentions sont des commentaires, `config/modules.jenga:40, 321`,
  `Nkentseu.jenga:304` ; le genre global est `STATIC_LIB`).
- La chaîne `nk-windows-clang-mingw` impose `-static -Wl,-Bstatic`
  (`config/toolchain.jenga:39-64`), qui s'appliquerait aussi à la DLL (⚪ effet
  non mesuré).
- `modules.jenga` n'émet jamais `*_USE_SHARED_LIB` : un consommateur d'une DLL
  du moteur n'aurait pas de `dllimport`. **Sans objet ici** — le module ne lie
  pas le moteur — mais c'est une raison de plus de ne pas faire de DLL du moteur.

**Recommandation, fondée sur ce qui a déjà marché deux fois :**

1. **S5a — appel direct du compilateur**, comme `NkHotReloadDemo`
   (`main.cpp:147-171` : `clang++ -shared -std=c++17 -O1 [-static] -I… -o X.dll X.cpp`)
   et ConquerorLab. Le risque habituel d'un second chemin de compilation — des
   options qui divergent — est **ici minime** : le module n'inclut que
   l'en-tête autonome et ne lie **rien** ; sa commande tient en une ligne et ne
   dépend d'aucune configuration du moteur.
2. **S5b — mesurer `sharedlib()` de jenga** sur le même module (un projet
   généré `Scripts.jenga`), et basculer s'il lie proprement **et** si le temps
   d'une reconstruction reste court (⚪ temps de démarrage de jenga non mesuré).
3. **La livraison passe par jenga dans tous les cas** : les sources du module
   entrent dans le projet du jeu en `staticlib()` (§ 5.6).

**Comment l'éditeur lance la compilation** : il n'existe **pas** de module
partagé de processus. Les briques sont dans NKCode (`Project/NkProcess.h`,
`_popen` sur un fil, sans pouvoir tuer ; `CreateProcessW` + tuyaux + délai dans
`NkLsp.cpp:327-367` ; `Shell/NkShell.h`) et dans
`NKConverse/NkConverseModeles.h:105-164`. À extraire en `Kernel/System`
(`NkProcessus`) au même titre que le chargeur : l'éditeur a besoin de lancer,
lire la sortie **et** arrêter (un compilateur qui ne rend pas la main).

**Où vivent les sources** : dans un **projet de jeu**, que l'éditeur n'a pas
encore — il travaille sur un fichier de scène (`NkEditeurModele::chemin`). Le C++
exige un dossier (`MonJeu/Scripts/*.cpp`). **Prérequis de S5**, à trancher (Q4).

### 5.4 Le verrou Windows

Réglé par la copie avant chargement (§ 5.2). Deux précisions :

- les copies des générations précédentes sont **effacées au déchargement** et,
  par précaution, **au démarrage** (une session tuée en laisse) ;
- le fichier `.pdb` / les symboles de débogage suivent la même copie, sinon le
  débogueur verrouille à son tour — ⚪ comportement de GDB/LLDB avec la chaîne
  mingw non vérifié.

### 5.5 L'état à préserver au rechargement

| état | où il vit | au rechargement |
|---|---|---|
| **propriétés exposées** d'une classe C++ (celles du panneau Détails) | dans `NkScript2D::vars`, **comme les variables d'un Blueprint** | **gardées sans rien faire** : elles ne sont pas dans le module |
| **état privé** (membres ordinaires de la classe) | dans l'instance, côté module | gardé **si** la classe fournit `Sauver` / `Relire` (octets, à la Noge — son tampon fait 4 096 octets) ; sinon perdu, et **dit** au journal |
| **l'instance elle-même** | l'hôte | détruite par l'**ancien** module (son destructeur vit dans son code), recréée par le nouveau, qui reçoit **« Rechargé »** — pas « Début » |
| une classe **disparue** du nouveau module | — | l'instance est désactivée et le dit ; le composant **garde** sa référence et ses valeurs : rien n'est perdu si la classe revient |

### 5.6 Le chemin statique (Android, iOS, Web, HarmonyOS, et toute livraison)

- Les mêmes `.cpp`, compilés **dans** l'application du jeu, avec
  `NK_UNKENY_SCRIPTS_STATIQUES`.
- La macro `NK_UNKENY_MODULE(MonJeu)` rend alors une fonction **nommée**
  `NkUnkModule_MonJeu` au lieu du symbole exporté fixe `nk_unkeny_module_v1`,
  et un **registre généré** l'appelle explicitement au démarrage.
- ⚠️ **Pas d'auto-enregistrement par objet statique** : dans une bibliothèque
  statique, un fichier que rien ne référence est **jeté par l'éditeur de
  liens**, avec son constructeur. ConquerorLab déclare ses fabriques par leur
  nom pour cette raison (`NkcModuleHost.h:53-57`).
- **Web** : pas de module secondaire (`SIDE_MODULE` existe dans jenga,
  `Emscripten.py:163-182`, mais ⚪ les bibliothèques statiques n'y sont pas
  compilées en PIC, ce qu'un `MAIN_MODULE` demande) — le statique évite la
  question. **iOS** : statique, par principe (⚪ règle d'Apple non documentée ici ;
  et `Unkeny.jenga` n'a **aucun filtre iOS** — à ajouter). **Android /
  HarmonyOS** : les `.so` seraient possibles (jenga les met dans l'APK/HAP),
  le statique est plus simple et c'est le même chemin que le Web.

---

## 6. Les formats

### 6.1 `.nkbp` — graphe ET module

L'extension existe déjà : `NkAssetType::Blueprint = 9` → `.nkbp`, « graphe de
script visuel » (`CONVENTIONS_FICHIERS.md` § 2, `NkAssetMetadata.h:216, 355`).
Le fichier suit le format commun : `NkAssetFileHeader` (40 o) + `NkAssetMetadata`
+ **charge utile**, écrit par `NkAssetIO::Write` (qui calcule les CRC,
`NkAssetMetadata.h:566-640`).

```
charge utile :
  "NKBP"  u32 version = 1  u32 nbSections
  section GRAF : le texte .nkgraph v5 du graphe (NkNodeGraph::Serialize)  <- l'AUTORAT, retirable
  section MODL : le module compilé (§ 6.2)                               <- seul lu par le jeu
```

- **Un seul fichier pour les deux**, par la règle de `CONVENTIONS_FICHIERS.md` :
  « l'extension dit ce que le fichier PRODUIT » — un graphe et son module
  produisent **la même chose** au même endroit. Un module sans graphe (livraison
  dépouillée, ou écrit à la main) reste un `.nkbp` valide ; un graphe sans module
  (jamais compilé, ou en erreur) aussi, et le jeu le **refuse en le disant**.
- **Le module est stocké, pas recompilé au chargement** : le jeu livré n'a ni
  NKGraph ni compilateur, et une erreur de compilation doit se voir **dans
  l'éditeur**, pas chez le joueur.
- **Empreinte** : le module porte l'empreinte du texte GRAF qui l'a produit. Un
  graphe modifié à la main se signale « module périmé » au lieu de jouer un
  vieux comportement en silence.

### 6.2 Le module (section MODL)

```
en-tête    "NKBC"  u16 format  u16 abiHote(majeure, mineure) minimale  u64 empreinte du graphe source
imports    nom qualifié du natif + signature (types)      -> résolus PAR NOM au chargement
variables  nom, type, valeur par défaut, exposée ?        -> celles de NkScript2D::vars
constantes réels, entiers, vec2, booléens, textes ; sons et actions PAR NOM, résolus au chargement
entrées    (événement, paramètre : nom d'action, phase…) -> fonction
fonctions  nombre de registres + type de chaque registre + code (mots de 32 bits)
lignes     pc -> NkNodeId                                 -> facultatif (journal qui désigne le nœud)
```

- Octets écrits **explicitement en petit-boutiste**, champ par champ : un module
  voyage entre plateformes, contrairement aux octets hexadécimaux du bloc
  `"jeu"` des scènes.
- **Vérifié au chargement, avant toute exécution** : opcodes connus, registres
  dans le cadre, constantes et imports dans leurs tables, sauts dans la
  fonction, types des opérandes conformes aux registres et aux signatures des
  natifs, natifs autorisés pour l'événement. Un module refusé **dit pourquoi**
  (`NkGraphIssue` en est le modèle : un code **et** ce qu'il désigne).

### 6.3 Le composant de script dans `.nkscene`

```json
{
  "format": "unkeny.scene", "version": 2,
  "entites": [
    { "nom": "Blob joueur", "transform": "0 1 0 1 1", "mou": { "corpsId": 3 },
      "script": { "ref": "Scripts/Joueur.nkbp", "actif": true,
                  "vars": { "poussee": "40", "sautsMax": "2", "cap": "1 0" } } }
  ]
}
```

- **La référence par NOM**, comme les textures (« on écrit le NOM de la
  texture et on la retrouve au chargement », `NkUnkenySauvegarde.h`).
- **Les variables par NOM**, valeurs en texte `"%.9g"` comme le reste du
  fichier. Une variable absente du module est **ignorée et dite** ; une
  variable nouvelle prend sa valeur par défaut. C'est la leçon de NKGraph
  (v1 → v2 : liens par index, puis par nom) appliquée avant d'avoir à la payer.
- **Écrit champ par champ** comme les composants d'Unkeny — pas par
  `PhotographierAussi` en octets, qui ne sait ni les noms ni les types.
- **`"version": 2`** : un moteur de version 1 **refuse** alors un fichier qui
  porte des scripts (« fichier plus récent que ce moteur ? ») au lieu de les
  perdre en silence. Les fichiers de version 1 restent lus.
- `NkSauverScene` / `NkChargerScene` gagnent un paramètre facultatif
  `NkScripts2D *`, sur le modèle exact du paramètre `NkTextures2D *` existant.

### 6.4 Le projet C++

```
MonJeu/
├── MonJeu.<ext du projet>      (Q4 — aucun nom gravé avant le premier octet)
├── Scenes/Niveau1.nkscene
├── Scripts/Joueur.nkbp
└── Scripts/Cpp/Joueur.cpp      + Scripts.jenga généré (S5b) pour la livraison
```

Chemins **relatifs au projet**, obligatoirement (`CONVENTIONS_FICHIERS.md` § 5).

---

## 7. Le premier lot de nœuds

Types de prises enregistrés dans NKGraph par la couche 3 : `booleen`, `entier`,
`reel`, `vec2`, `entite`, `son`, `action`, `texte` (constante seulement). Les
conversions sont **dirigées** et déclarées (entier → réel oui ; réel → entier
non, trois arrondis plausibles — même raisonnement que les matériaux).

Colonne « existe ? » : **✅** = appelle une fonction qui existe ce soir ;
**M…** = attend le manque du § 3.11.

### 7.1 Événements (sortie d'exécution seule)

| nœud | sorties | quand | existe ? |
|---|---|---|---|
| **Début** | exec | une fois, avant tout autre événement de l'entité | ✅ (hôte) |
| **Tick** | exec, `dt` | chaque trame, `NK_TRAME` | ✅ |
| **Pas fixe** | exec, `dt` | chaque pas fixe, **avant** la physique — le seul lieu des forces | ✅ |
| **Contact : début / fin** | exec, `autre` | choc entre corps solides ; reçu par **les deux** entités | ✅ rigides ; M5e corps mous |
| **Zone : entrée / sortie** | exec, `autre`, `soiEstLaZone` | déclencheur traversé ; reçu par la zone ET par l'entrant | ✅ rigides ; M5e |
| **Action pressée / relâchée** (propriété : l'action, par nom) | exec, `valeur` | `VientDEtrePressee` / `VientDEtreRelachee` — **jamais une touche** | M2 |

### 7.2 Flot

| nœud | prises | existe ? |
|---|---|---|
| **Si** | exec, `condition` → **Vrai**, **Faux** | ✅ (VM) |
| **Séquence** (n sorties, 2 par défaut) | exec → **Alors 0**, **Alors 1**… exécutées dans l'ordre | ✅ (VM) |

(Boucles, Faire une fois, Délai : S7 — une boucle est un **nœud**, jamais un fil
qui revient, et le portail est retiré, `CATALOGUE_NOEUDS.md` § 4.)

### 7.3 Variables

| nœud | prises |
|---|---|
| **Lire** `nom` (pur) | → valeur |
| **Écrire** `nom` | exec, valeur → exec, valeur |

Déclarées dans le Blueprint (nom, type, défaut, **exposée** au panneau Détails),
16 au plus par script au premier lot (`NK_UNKENY_SCRIPT_VARS_MAX`).

### 7.4 Entités

| nœud | prises | appelle | existe ? |
|---|---|---|---|
| **Soi** (pur) | → entité | — | ✅ |
| **Est valide** (pur) | entité → booléen | `IsAlive` | ✅ |
| **Position** (pur) | entité → vec2 | `NkTransform2D` | ✅ |
| **Téléporter** | exec, entité, position → exec | `TeleporterEntite` | ✅ |
| **Détruire** | exec, entité → exec | `NkScene::Detruire` | ✅ |
| **Créer un acteur** (propriété : l'acteur du catalogue) | exec, position → exec, entité | `NkPoserActeurSim` (`Simulation/NkUnkenyActeurs.h:89`) | ✅ |

(« Créer depuis un modèle » attend `.nkprefab` : S7.)

### 7.5 Corps rigide

| nœud | prises | appelle | existe ? |
|---|---|---|---|
| **Appliquer une force** — **valide sous « Pas fixe » seulement** | exec, entité, force → exec | `ApplyForce` | M1 (pont 2D) |
| **Appliquer une impulsion** | exec, entité, impulsion → exec | `ApplyImpulse` | M1 |
| **Poser la vitesse** | exec, entité, vitesse → exec | `PoserVitesse` | ✅ |
| **Vitesse** (pur) | entité → vec2 | `Vitesse` | ✅ |

### 7.6 Corps mou (demande de Rihen)

| nœud | prises | appelle | existe ? |
|---|---|---|---|
| **Centre du corps mou** (pur) | entité → vec2 | `CentreCorps` | ✅ |
| **Vitesse du corps mou** (pur) | entité → vec2 | `VitesseCorps` | ✅ |
| **Poser la vitesse du corps mou** — *fige la forme ce pas-là* | exec, entité, vitesse → exec | `AppliquerVitesse` | ✅ (sémantique de **remplacement**, dite dans l'aide) |
| **Pousser le corps mou** | exec, entité, Δv → exec | `AjouterVitesse` | M5b |
| **Force sur le corps mou** — *Pas fixe seulement* | exec, entité, force → exec | `AppliquerForce(ci, f, dt)` | M5b |
| **Épingler** | exec, entité, épinglé → exec | `EpinglerCorps` | ✅ |
| **Saisir au point : début / vers / fin** | exec, point, rayon → exec, saisi | `SaisirDebut/Vers/Fin` | ✅ (**une** saisie pour tout le monde, tous corps confondus — dit dans l'aide) |
| **Saisir ce corps** (par pointeur) | exec, entité, pointeur, point → exec | `SaisirCorps` | M5d |
| **Au sol** (pur) | entité → booléen, normale | résumé de contact par corps | M5a |
| **Attacher à un rigide** | exec, entité, partie, rigide, ancre → exec | attache particule ↔ rigide | M5f, M5c |

### 7.7 Entrée

| nœud | prises | existe ? |
|---|---|---|
| **Valeur d'action** (pur) | action → réel −1..1 | M2 (le nom → indice) ; `NkActions::Valeur` ✅ |
| **Action enfoncée / pressée / relâchée** (pur) | action → booléen | idem |
| **Direction** (pur) | action X, action Y → vec2 **normalisée** | `NkActions::Direction` ✅ |
| **Pointeur** (pur) | indice → position monde, enfoncé, vient d'être pressé | M2b |

### 7.8 Son, animation, débogage, calcul

| nœud | prises | appelle | existe ? |
|---|---|---|---|
| **Jouer un son** (son par nom) | exec, son, volume → exec, voix | `NkSons2D::Jouer` | ✅ |
| **Jouer un son ici** | exec, son, position → exec, voix | `JouerA` + caméra de la scène | ✅ |
| **Jouer un clip** | exec, entité, clip → exec | `NkAnimSprite2D::Jouer` | ✅ |
| **Afficher** | exec, texte, valeur (facultative) → exec | journal de l'éditeur + `logger` ; en livraison `logger` seul | ✅ (hôte) |
| **Temps écoulé** (pur) | → réel | cumul tenu par l'hôte | ✅ (hôte) |
| **Calcul** (purs) : + − × ÷ (réel, vec2), vec2 × réel, comparer, et/ou/non, construire/décomposer vec2, longueur, normaliser | — | VM | ✅ |

**Un même nœud ne se dessine qu'une fois** : `Math` de Blueprint et de matériau
ont la même apparence (`CATALOGUE_NOEUDS.md` § 4, règle 1).

---

## 8. Sécurité : un script qui plante ne doit pas tuer l'éditeur

### 8.1 Blueprint — la promesse est tenable, et on la tient

Le Blueprint n'exécute **que** ce que la VM exécute. Les garde-fous :

1. **Vérificateur au chargement** (§ 6.2) : un module mal formé ne s'exécute
   jamais — qu'il vienne du compilateur, d'une édition à la main ou d'un fichier
   abîmé.
2. **Budget d'instructions** par événement (et par trame pour la scène) : une
   boucle sans fin est **coupée**, l'instance passe **en faute**, la scène
   continue. Le journal dit le script, l'événement et **le nœud** (table de
   lignes).
3. **Aucun pointeur dans le bytecode** : une entité est une poignée
   (index + génération) vérifiée à chaque usage par l'hôte.
4. **Natifs qui refusent au lieu de casser** : entité morte, composant absent,
   valeur non finie (une force `NaN` empoisonnerait le solveur rigide ; les
   particules ont leur propre garde-fou, `NkParticules2D.cpp:1152-1157`, les
   rigides ⚪ non vérifié) → refus, compté, dit une fois.
5. **Une instance en faute est désactivée seule** : pas la scène, pas les autres
   entités. « Arrêter » puis « Jouer » la relance.

### 8.2 C++ — ce qu'on peut garantir, et ce qu'on ne peut pas

🔴 **Mesuré** : le dépôt n'a **aucun** attrapeur de faute (pas de `__try`,
`SetUnhandledExceptionFilter`, `AddVectoredExceptionHandler`, `sigaction`,
`setjmp` dans le code), et **clang-mingw ne sait pas faire `__try/__except`**
(`NkDX12Context.cpp:204-205`, 30/05). Un module C++ qui déréférence un pointeur
invalide **dans le processus** tue l'éditeur. C'est aussi le cas d'UE5 avec
*Live Coding*. La promesse tient donc par niveaux :

| niveau | garde-fou | palier | garantit |
|---|---|---|---|
| **N0** | API par **valeurs et poignées vérifiées** (§ 4.3) — la cause la plus fréquente de plantage, le pointeur vers un composant qui a bougé, **n'existe pas** | S0 | beaucoup moins de fautes |
| **N0** | **exceptions attrapées à chaque passage de frontière** : les exceptions sont **actives** dans le dépôt (aucun `-fno-exceptions`), les thunks de la macro font `try { … } catch (...)` et mettent l'instance en faute. ⚠️ Les crochets de Noge sont `noexcept` : une exception y appelle `std::terminate` — **à ne pas reprendre** | S0 | une exception ne tue rien |
| **N0** | module qui ne compile pas, ou d'ABI majeure différente → **refusé, l'ancien reste actif** | S5 | l'itération ne casse pas la session |
| **N1** | **copie de secours** : à « Jouer », la photo (déjà prise en mémoire, `NkEditeurJouer`) est **aussi écrite** en `<scène>.secours.nkscene` ; au démarrage suivant, l'éditeur la propose, et le module fautif n'est pas rechargé d'office | S5 | **on ne perd rien** |
| **N2** | **attrapeur de dernier recours** (⚪ best-effort) : `AddVectoredExceptionHandler` sous Windows (API Win32 ordinaire, disponible sous mingw), `sigaction(SIGSEGV, SIGBUS, SIGFPE)` sous POSIX — il écrit **qui** (module, classe, entité, événement) et vide le journal, **puis laisse mourir**. Reprendre l'exécution (saut hors du gestionnaire) n'est **pas** promis : l'état mémoire peut être corrompu | S5 | **on sait qui** |
| **N2** | **chien de garde** (bureau) : un appel de script de plus de 2 s est signalé ; on propose de tuer | S5 | la boucle sans fin se voit |
| **N3** | **« Jouer isolé »** : la partie tourne dans un **processus fils** (`UnkenyEditor --jouer <scène> --module <dll>`) ; l'éditeur survit à tout | S7 | la garantie **stricte** |

Si Rihen veut la garantie stricte **par défaut** dès qu'un module C++ est
chargé, N3 remonte avant S5 (Q5).

---

## 9. Les bancs, écrits avant le premier chiffre

Dans l'esprit des bancs existants (`NkUnkenyBanc.cpp`, `NkEditeurBanc.cpp`) :
témoins **nommés et pré-enregistrés**, une **contre-épreuve** (`n`) pour chaque
garantie — sans elle, un témoin vert peut n'être qu'une tautologie — et un
passage par `--selftest` sans fenêtre ni GPU. Un banc qui a besoin d'un
compilateur absent rend **INDÉTERMINÉ**, jamais vert (la règle de `main.cpp`
d'UnkenyEditor : « un banc vide est pire qu'un banc absent »). Préfixes libres
aujourd'hui : **x** (hôte), **b** (Blueprint), **c** (C++), **m** (corps mous) ;
l'éditeur continue en **e13…**.

### 9.1 Moteur — l'hôte (`NkUnkenyBancScripts.cpp`, lancé par `NkUnkenyLancerBanc`)

- **(x1)** Début : une fois par entité, avant tout autre événement, **même si le
  premier passage est un pas fixe** ; (x1n) une entité sans `NkScript2D` ne
  reçoit rien.
- **(x2)** À 30 i/s pour un pas fixe de 1/60 : « Pas fixe » deux fois (dt 1/60),
  « Tick » une fois (dt 1/30) — j2 rejoué à travers l'hôte.
- **(x3)** Balle lâchée sur le sol : « Contact début » reçu par la balle
  (`autre` = sol) **et** par le sol (`autre` = balle) ; (x3n) dans le vide :
  rien.
- **(x4)** Zone traversée : entrée puis sortie, `soiEstLaZone` vrai pour la zone
  seulement.
- **(x5)** `Sauter` enfoncée trois trames : « Action pressée » **une** fois ;
  relâchée : « Action relâchée » une fois ; (x5n) sans `NouvelleTrame`, rien ne
  part — et l'hôte le **dit** (le piège écrit dans `NkUnkenyActions.h:73-75`).
- **(x6)** Un script se **détruit** dans Tick : aucun appel ensuite, corps rigide
  parti, les autres scripts de la trame tournent ; un script **crée** un acteur :
  utilisable dans le même événement, son Début avant son premier Tick.
- **(x7)** Photographier / Restaurer : variables rendues aux valeurs d'avant,
  Début rejoué pour chaque entité.
- **(x8)** `.nkscene` v2 : aller-retour de la référence et des variables **par
  nom** ; (x8n) une variable disparue du module est ignorée **et dite** ; une
  version future refusée, scène intacte (le patron de s3).
- **(x9)** Entité morte passée à **chaque** fonction de la table : refus, rien ne
  bouge ; force `NaN` refusée, solveur fini.
- **(x10)** Coût : 1 000 entités, Tick qui lit une position et écrit une
  variable — **temps par trame publié**, sans seuil avant la première mesure.

### 9.2 Moteur — le module et la VM

- **(b1)** Un module **écrit à la main, sans graphe** (la contrainte du 24/08) :
  s'écrit, se relit **octet pour octet**, et fait sauter une balle
  (Début → Impulsion (0, 5)) — hauteur atteinte à 1 % de v²/2g.
- **(b2)** Vérificateur : saut hors fonction, registre hors cadre, import
  inconnu, type discordant, constante hors table → **refus nommé** chacun ;
  (b2n) le module de b1 passe.
- **(b3)** Boucle de 10⁶ tours : coupée au budget, instance en faute, journal qui
  désigne le pc et le nœud, les autres entités avancent ; (b3n) sous le budget,
  la même boucle finit.
- **(b4)** Si et Séquence dans l'ordre : trace des « Afficher » = `A, B1, B2`.
- **(b5)** Imports **permutés** dans la table : même exécution ; natif absent de
  l'hôte : refus **au chargement**, pas à l'appel.
- **(b6)** Force en « Pas fixe » : vitesse = F/m·t à 1 % ; (b6n) la même
  instruction sous « Tick » : refusée par le vérificateur.

### 9.3 Moteur — C++

- **(c1)** Module **statique** lié au banc : classe trouvée par nom, Début /
  Pas fixe / Tick appelés ; **même trajectoire que b1 au millimètre** — les deux
  langages sur la même table.
- **(c2)** Exception levée dans Tick : rattrapée à la frontière, instance en
  faute, scène qui continue ; (c2n) sans le `try` du thunk, le banc sait qu'il
  tomberait (mutation vérifiée rouge, hors `--selftest`).
- **(c3)** Module d'ABI **majeure** différente : refusé ; d'ABI **mineure** plus
  ancienne : accepté, queue de table nulle (le patron de
  `ConquerorLab/tests/abi_fige_3_0`).
- **(c4)** *(bureau ; INDÉTERMINÉ sans compilateur)* compiler v1 → charger
  (copie par génération) → Jouer → recompiler v2 **par-dessus** → recharger :
  propriété exposée gardée, état privé gardé par `Sauver/Relire`, nouveau
  comportement actif ; (c4n) v3 qui ne compile pas : **v2 reste active**,
  l'erreur du compilateur au journal.
- **(c5)** Arrêter puis décharger : aucune instance vivante, l'original
  réinscriptible (Windows), les copies effacées.

### 9.4 Moteur — corps mous pilotés (S3)

- **(m1)** « Poser la vitesse du corps mou » : le centre avance de v·t à 2 %.
- **(m2)** « Pousser » (M5b) : le corps **se déforme encore** (écart-type des
  distances au centre au-dessus d'un seuil mesuré) ; (m2n) « Poser la vitesse »
  appelée à chaque pas le **fige** — la raison d'être de M5b, prouvée.
- **(m3)** Blob posé : « Au sol » vrai, normale ≈ (0, 1) ; lancé : faux ;
  (m3n) contre un mur seul : normale ≈ (±1, 0), « Au sol » faux.
- **(m4)** Deux saisies simultanées de deux corps (deux pointeurs) : aucune
  particule volée ; (m4n) la saisie actuelle, au même point, prend les deux.
- **(m5)** Blob sur une caisse : « Contact début » sur les **deux** entités.
- **(m6)** Particule attachée à une caisse qui tombe : écart ≤ 1 cm.

### 9.5 Éditeur (`NkEditeurBanc.cpp`, suite)

- **(e13)** « Ajouter un composant > Script » : Jouer, le script agit ; Arrêter :
  photo et variables reviennent.
- **(e14)** Graphe construit **par code** (API NKGraph) « Début → Afficher
  "bonjour" », compilé : le journal de l'éditeur porte `bonjour` au premier Pas.
- **(e15)** Graphe invalide (sortie exec liée deux fois, fil croisé, force hors
  pas fixe) : erreur **nommée** (le nœud) ; Jouer reste possible, l'instance est
  désactivée et le dit.
- **(e16)** Graphe modifié pendant JEU : rechargement, variables gardées **par
  nom**.
- **(e17)** Enregistrer / Ouvrir : composant, référence et valeurs éditées dans
  Détails reviennent.
- **(e18)** *(S4)* Canevas : poser un nœud, tirer un fil exec, le refus d'un fil
  croisé **montré avec sa raison** (`NkLinkErrorName`) — par l'injection
  d'événements de `NKEditorKit/NkEditorScriptEvenements.h` (⚪ adéquation non
  vérifiée).

**Et le correctif G1 de NKGraph** a son cas dans `NkMatGraphCheck`, avec sa
mutation rouge : une entrée exec à deux sources **ne produit plus** de
`LinkDuplicateTarget`, une entrée de données à deux sources **en produit
toujours un**.

---

## 10. Les paliers

Chaque palier se termine sur **ce qu'on peut montrer**, et ses bancs verts.

| palier | contenu | démontrable | bancs |
|---|---|---|---|
| **S0 — le socle, sans langage** | `NkContexteJeu2D` (M2 : actions + noms + correspondances ; M2b : pointeurs) ; `NkScript2D`, `NkScripts2D` ; `NkHoteScripts2D` (Début, Pas fixe, Tick, Contact, Zone, Action ; table d'exécution hors composant) ; **`NkUnkHoteV1`** couvrant le premier lot sur ce qui existe ; M1 ; `.nkscene` v2 ; M4 ; **comportements C++ statiques** compilés dans l'application (premier client de la table) | Dans l'éditeur : une balle qui saute sur **Espace** *ou* le bouton A *ou* un toucher, par un comportement C++ statique — Arrêter rend tout. `--selftest` vert. | x1–x10, c1–c3 |
| **S1 — bytecode et VM** (moteur, sans graphe) | `NkModuleBp` (écrire, lire, vérifier), `NkMachineBp` (budget, fautes désignées), natifs = table S0 | Un `.nkbp` **écrit à la main** (section MODL seule) pilote une entité dans l'éditeur. | b1–b6 |
| **S2 — compilateur et premier lot** (éditeur, sans canevas) | types Blueprint dans NKGraph ; catalogue (§ 7, ce qui existe) ; compilateur ; `.nkbp` GRAF + MODL ; **correctif G1** dans NKGraph ; NKGraph obtient sa cible jenga (premier consommateur réel) | Un `.nkbp` produit par le banc depuis un graphe **construit par code** s'ouvre et joue. | e13–e17 |
| **S3 — corps mous pilotables** (NKPhysics + Unkeny, **en parallèle** de S1/S2) | M5a–M5f, pointeurs multiples, leurs fonctions de table et leurs nœuds | Un **blob-personnage** : marche sur l'axe X sans perdre sa mollesse, ne saute **qu'au sol**, se saisit au doigt sans prendre le voisin — au clavier, à la manette, au tactile ; dans l'éditeur **et** sur Android. | m1–m6 |
| **S4 — l'éditeur de graphe** | canevas (NKEditorKit, couche 2, selon `SPECIFICATION_VISUELLE.md`) ; onglet graphe ; Détails (Script, variables exposées) ; Journal → nœud surligné | **Rihen construit à la souris** : « Action Sauter pressée → Si Au sol → Impulsion (0, 6) → Jouer un son "saut" », joue, modifie pendant JEU. | e18 + e13–e17 depuis l'interface |
| **S5 — le C++ à chaud** (Windows, puis Linux) | projet de jeu (Q4) ; `NkModuleDynamique` et `NkProcessus` extraits en Kernel ; compilation directe (S5a) puis mesure de `sharedlib()` (S5b) ; rechargement avec état ; N0–N2, secours | Modifier `Joueur.cpp` pendant JEU : le comportement change **sans Arrêter** ; casser la compilation : l'ancienne version tourne, l'erreur au Journal ; tuer volontairement : au redémarrage, la scène de secours est proposée. | c4, c5 |
| **S6 — la livraison statique** | une application de jeu Unkeny (pas l'éditeur) : modules liés en statique par registre généré ; `.nkbp` embarqués (GRAF retirable) ; filtre iOS dans `Unkeny.jenga` | La **même scène** (un Blueprint + une classe C++) jouée sous Windows (DLL) et sous **Web** et **Android** (statique), trajectoire identique au banc. iOS **déclaré, pas éprouvé** — comme `UnkenyEditor.jenga:206-208` le dit déjà. | c1 et b1 sur chaque cible |
| **S7 — au-delà du premier lot** | latents (Délai, coroutines de la VM) ; boucles ; fonctions Blueprint (**G2**) ; événements personnalisés ; « Image d'animation » (M6) ; identité stable (M3) et variables « entité » sauvées ; `.nkprefab` ; requêtes 2D (M7) ; **Jouer isolé** (N3) ; débogueur (points d'arrêt, chemin surligné) | Une porte à levier (référence d'entité **sauvée**), une attente de 2 s, un point d'arrêt qui s'arrête. | à écrire avec le palier |

---

## 11. Les risques

| # | risque | parade |
|---|---|---|
| R1 | `sharedlib()` de jenga jamais exercé ; `-static` de la chaîne appliqué à la DLL | le module ne lie rien → compilation directe, déjà réussie deux fois (S5a) ; jenga mesuré ensuite (S5b) |
| R2 | un module C++ tue l'éditeur (pas de SEH sous clang-mingw) | N0–N2 : on ne perd rien, on sait qui ; N3 pour la garantie stricte |
| R3 | faux diagnostics de `Validate()` sur les graphes exec (G1) | correctif dans le cœur en S2, avec cas et mutation ; **coordination** avec qui tient NKGraph et NkMatGraphCheck |
| R4 | le canevas est gros, et la tentation de l'écrire **dans** UnkenyEditor | il va dans NKEditorKit (couche 2) : les matériaux le réutiliseront ; UnkenyEditor n'en est que le premier client |
| R5 | les identifiants d'entité changent (Restaurer, chargement) | pas de variable « entité » sauvée avant M3 ; au premier lot, les entités viennent des événements (`autre`, `Soi`) |
| R6 | corps mous : index contre identifiant, vitesse **remplacée** | l'hôte convertit toujours ; l'aide du nœud le dit ; M5b en S3 |
| R7 | un troisième modèle de graphe s'installe (NKGui, Noge, Unkeny) | Unkeny est un **client de NKGraph** ; la VM est écrite pour descendre dans le Kernel (§ 4.8) |
| R8 | la VM est trop lente pour un vrai jeu | mesure x10 avant d'écrire une optimisation ; sortie de secours : générer du C++ depuis le module |
| R9 | un troisième chargeur de DLL, un Nième lanceur de processus | extraction en Kernel à S5, Noge et ConquerorLab migrent à leur rythme |
| R10 | UnkenyEditor est modifié en parallèle par un autre agent | ce document ne touche aucun fichier ; S0 ajoute des fichiers dans `Engine/Unkeny/src/Unkeny/Script/` et ne modifie qu'aux points nommés (`NkUnkenySauvegarde`, `NkEditeurModele`, `NkEditeurDetails`) — à annoncer avant d'y entrer |

---

## 12. Ce qui n'est pas vérifié

Rien n'a été compilé ni exécuté pour ce document. En particulier :

1. `NkHotReloadDemo` et ConquerorLab tournent **encore** (aucun binaire du
   premier dans `Build/Bin/Debug-Windows` ; preuve du ROADMAP de Noge datée du
   24/07, en Release).
2. Le chemin POSIX `dlopen` (jamais exécuté selon le ROADMAP de Noge) ; le
   rechargement sous Linux et macOS en général.
3. Qu'un `sharedlib()` jenga lie sous clang-mingw (pas de bibliothèque d'import,
   `-static` de la chaîne) ; le temps d'une reconstruction par jenga ; si msys
   clang lie par ld.bfd ou lld.
4. La duplication des singletons d'en-tête (NKECS, NKMemory, NKLogger) dans une
   DLL : comportement connu du format PE, **non éprouvé** dans le dépôt.
5. Les règles d'Apple sur le code chargé, et l'embarquement / la signature de
   `.dylib` par jenga sur iOS.
6. `MAIN_MODULE` Emscripten avec des bibliothèques statiques non PIC.
7. Le comportement de GDB/LLDB (fichiers de symboles verrouillés) avec la copie
   par génération.
8. Que `AddVectoredExceptionHandler` / `sigaction` permettent d'écrire la copie
   de secours de façon fiable depuis le gestionnaire (N2).
9. Les manques G1–G3 de NKGraph et M5a–M5f des corps mous : trouvés **à la
   lecture**, aucun cas ne les montre encore.
10. M4 (`NkSource2D::son` sauvé en octets) : déduit de la lecture, non éprouvé
    entre deux sessions.
11. Le comportement de `Relier` entre deux corps de particules à la compaction
    (M5c, voie b).
12. Le garde-fou `NaN` du solveur rigide.
13. Le contenu de `CLAUDE.md` (absent).
14. L'adéquation de `NkEditorScriptEvenements.h` pour éprouver le canevas (e18).
15. Les ordres de grandeur externes cités pour Python et C# sur mobile et Web.

---

## 13. À trancher par Rihen

| # | question | proposition |
|---|---|---|
| Q1 | Un script par entité (Blueprint d'acteur, UE5) ou plusieurs (Unity) ? | **Un.** Plus simple à sauver, à inspecter, à expliquer ; la composition passe par les entités. |
| Q2 | La VM vit dans Unkeny d'abord, et descend en Kernel quand Noge la prend ? | **Oui** (règle des deux consommateurs). |
| Q3 | Graphe et module dans le même `.nkbp` ? | **Oui** ; l'empaquetage retire le graphe. Pas de nouvelle extension. |
| Q4 | UnkenyEditor gagne un **projet** (dossier + fichier) — prérequis du C++. Quelle extension ? | À nommer au premier octet écrit (règle de `CONVENTIONS_FICHIERS.md`), « identifiable sans ouvrir » comme `.nk3dm`. |
| Q5 | « Jouer isolé » par défaut dès qu'un module C++ est chargé ? | **Non** au début (itération plus rapide, comme UE5) ; N1–N2 dès S5 ; N3 en S7, ou avant si Rihen le veut par défaut. |
| Q6 | Un personnage mou : des **parties nommées** d'un corps, ou **plusieurs corps reliés** ? | **Parties nommées** pour grilles et anneaux (stables) ; plusieurs corps seulement après vérification de `Relier` entre corps (§ 12, point 11). |

### Décisions de Rihen (29/09/2026)

| # | décision | ce que ça change |
|---|---|---|
| Q1 | **Plusieurs scripts par entité** (façon Unity), et non un seul comme la proposition. | Le composant script porte une **liste ordonnée** de scripts ; l'ordre d'exécution est celui de la liste (affiché et réordonnable dans Détails) ; chaque script a ses propres variables sauvées sous son nom ; un événement (Début, Tick, Contact…) est livré à chaque script de la liste, dans l'ordre. À reporter dans § 5 (composant) et dans les bancs b*/c* (deux scripts sur une même entité, ordre respecté, variables séparées). |
| Q2 | Proposition adoptée (VM dans Unkeny d'abord). | — |
| Q3 | Proposition adoptée (graphe et module dans le même `.nkbp`). | — |
| Q4 | Projet UnkenyEditor : extension **`.nkunk`**. | Premier octet écrit selon `CONVENTIONS_FICHIERS.md`. |
| Q5 | **Non** au début : pas de « Jouer isolé » par défaut. | N1–N2 dès S5, N3 en S7. |
| Q6 | **Les deux** : parties nommées d'abord, corps reliés ensuite quand `Relier` entre corps est prouvé. | Transmis au chantier `comble/physique-2d-jeu`. |

---

## 14. Ce qui est fait — S0, S1, S2, et le C++ à chaud (01/10/2026)

> Branche `unkeny/scripts-s0-s2`. Tout ce qui suit est **construit et éprouvé**
> (`UnkenyEditor --selftest` : 18 bancs REUSSI ; `UnkenyPlayer --selftest`),
> chaque garantie avec sa contre-épreuve de mutation (citée dans les commits).
> Les choix que le document laissait ouverts sont tranchés ici, « le plus simple
> qui ne ferme aucune porte ».

### 14.1 Où est quoi

| brique | fichier | rôle |
|---|---|---|
| table C | `Engine/Unkeny/src/Unkeny/Script/NkUnkenyScriptABI.h` | `NkUnkHoteV1` (journal, temps, entités, transform, corps, sprite, animation, **effets**, actions, son, variables), `NkUnkEvenementV1`, classes et module C++, `nkunk::Script`, `NK_UNKENY_CLASSE`, `NK_UNKENY_CLASSE_VARIABLES`, `NK_UNKENY_GARDER` |
| composant | `Script/NkUnkenyScript.h` | `NkScript2D` : **4 scripts ordonnés** par entité (Q1), 16 variables par entité, chacune à son script |
| registre + hôte | `Script/NkUnkenyScripts.h` | `NkScripts2D` (nom → Blueprint ou `cpp:Classe`), `NkHoteScripts2D` (2 systèmes, ordre des événements, fautes isolées, migration à chaud) |
| module + VM | `Script/NkUnkenyBpModule.h`, `Script/NkUnkenyBpMachine.h` | bytecode (écrire, lire, **vérifier**), `.nkbp` GRAF + MODL, machine à registres typés, budget, natifs = la table, assembleur |
| DLL à chaud | `Script/NkUnkenyModulesCpp.h` | copie par génération, nouvelle avant ancienne, migration, `Symbole()` (greffons R36) |
| M1 | `NkScene::AppliquerForce / AppliquerImpulsion` | pont 2D, NaN refusé |
| canevas | `Engine/NKEditorKit/src/NKEditorKit/Components/NkCanevasNoeuds.h` | **générique** (couche 2) : réutilisable par les matériaux et l'Animateur |
| catalogue + compilateur | `Applications/UnkenyEditor/src/Script/NkBpCatalogue.h` | types, nœuds (un par natif), graphe → module vérifié, erreur → nœud |
| éditeur | `Script/NkEditeurScripts.h`, `NkEditeurGraphe.h`, `NkEditeurScriptsUi.h` | compilation clang++, rechargement, Journal ; page du graphe ; bloc « Scripts » des Détails ; « + Ajouter » ; double-clic |
| exemple | `Script/NkEditeurExemplePortes.h` | `--exemple=portes` |
| livraison | `Unkeny/Livraison/NkUnkenyLivraison.h`, `Livraison/NkEditeurConstruire.cpp`, `UnkenyPlayer.jenga` | Blueprints cuits sans graphe ; C++ lié en statique |
| bancs | `Banc/NkUnkenyBancScripts.cpp` (37), `Script/NkEditeurBancScripts.cpp` (20) | x1–x11, c1–c3, r1, b1–b6, p1–p2 ; e13–e18, c4, c5 |
| G1 | `Kernel/Runtime/NKGraph/src/NKGraph/NkNodeGraph.inl` | `Validate` connaît la famille ; cas `exec/validate-connait-la-famille` (NkMatGraphCheck, 144 cas) |

### 14.2 Les choix tranchés

1. **Variables : deux réels** (`NkVec2f`) par valeur — réel, entier (exact
   jusqu'à 2²⁴), booléen, vec2. Une valeur plus large (entité, M3) prendra un
   genre de plus sans changer le fichier.
2. **Le composant est un composant DÉCRIT** (`PhotographierAussi`, déclaré dans
   `NkScene::Init`), écrit sous `"jeu" > "NkScript2D"` champ par champ (tableaux
   de textes ajoutés à la sauvegarde décrite : une ligne par élément). **Pas de
   version 3 du fichier** : un moteur ancien ignore le composant inconnu (la
   règle de tous les composants décrits), au lieu de refuser la scène — choix
   plus simple que le § 6.3, réversible.
3. **Le projet** est le dossier qui porte `Contenu/` (l'existant :
   `NkEditeurDossierProjet`, réglages `.nkprojet`). Les sources C++ : tout
   `Contenu/**/*.cpp` ; les produits : `Intermediaire/Scripts/` (registre
   généré, `Scripts.dll`, copies `Scripts.genN.dll`). ⚠️ **L'extension `.nkunk`
   (Q4) n'est pas encore écrite** : le fichier de réglages `.nkprojet`, posé par
   le chantier de livraison le 01/10, en tient lieu ; le renommer est une
   décision à confirmer (deux fichiers de projet seraient pire qu'un).
4. **Compilation C++ = appel direct de clang++** (S5a) : `-shared -std=c++17
   -O1 --target=x86_64-w64-windows-gnu -static…`, ≈ 1 s pour l'exemple, la DLL
   ne dépend que du système. Le registre des classes est **généré** en lisant
   les `NK_UNKENY_CLASSE(...)` des sources (pas d'auto-enregistrement statique).
5. **État au rechargement** : variables exposées dans le composant (rien à
   faire) ; état privé par `NK_UNKENY_GARDER(membre)` (copie bit à bit, 4 Ko au
   plus) ; l'ancienne instance est détruite par **l'ancien** code, la nouvelle
   reçoit `Recharge()`. Mesuré : sans cette migration avant le déchargement,
   l'éditeur tombe.
6. **Les clés des prises n'ont pas d'espace** (`soiEstLaZone`, `alors0`) : le
   `.nkgraph` lit les prises par jetons ; un nom avec espace perdait ses fils à
   la relecture (mesuré par e16).
7. **Une entrée « entité » libre vaut Soi** ; une valeur saisie sur une entrée
   libre est sa valeur par défaut (UE5).
8. **Un corps STATIQUE téléporté est refait** (`TeleporterEntite`) : le solveur
   ne resynchronise jamais un statique, la porte « ouverte » bloquait encore le
   Joueur (mesuré, e13).
9. **Livraison** : chaque `.nkbp` que la scène nomme est cuit **sans GRAF**
   (`assets/scripts/`, sommaire `"scripts"`, facultatif) ; les `.cpp` du projet
   et le registre (`NK_UNKENY_SCRIPTS_STATIQUES`) entrent dans le workspace
   généré (`UNKENY_JEU["scripts"]`, lu par `UnkenyPlayer.jenga`).
   `UnkenyPlayer --verifier` nomme un script absent ; `--essai-scripts` joue
   sans fenêtre.

### 14.3 Ce que n'a PAS ce premier lot

Créer un acteur depuis un script ; les nœuds latents (Délai), boucles,
fonctions, événements personnalisés (S7) ; corps mous pilotables (S3) ; les
commentaires, copier/coller et la recherche de nœud depuis une prise tirée du
canevas (S4) ; Linux/macOS (chemin `dlopen` écrit, non éprouvé) ; Web,
Android, iOS (le statique est écrit, non construit) ; « Jouer isolé » (N3) et
l'attrapeur de dernier recours (N2).
