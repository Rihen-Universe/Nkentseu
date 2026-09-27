# NKUIDesign — spécification pour l'agent

### Document 5 — Destiné à Claude (agent d'implémentation)

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> **Réécrit le 27/09/2026.** La version d'août décrivait une pile **TypeScript /
> React** (`.tsx`, `useNkGuiDocumentStore`, variables CSS) et un format 0.2 : elle est
> **retirée**. NKUIDesign est une application C++ du moteur Nkentseu, sur NKGui ; son
> interface va être **écrite en `.nkgui`** et son document **est** le `.nkgui`.
> Ce document absorbe l'ancienne spécification agent de la refonte (doc 21).
>
> **Ce que tu y trouves**, dans l'ordre où tu en as besoin :
> 1. **le modèle** que l'éditeur manipule (§1, §1ter à §1decies — ces sections d'août,
>    en types `Nk*`, sont **gardées** : elles sont justes et le code les cite) ;
> 2. **le fil `.nkgui`** : ouvrir, éditer, enregistrer, compiler les comportements (§2,
>    §3) ;
> 3. **l'interface de NKUIDesign écrite en `.nkgui`** : vocabulaire, jetons,
>    composants, fichiers, **arbres**, **actions**, ce que tu ne sais pas encore
>    reproduire, ordre de livraison (§4 à §11).
>
> **Autorité** : **le doc 3 gagne** sur ce document pour tout ce qui apparaît à l'écran ;
> **le doc 2** pour le langage ; **le doc 1** pour le produit. Un écart est un défaut
> **d'ici**, à corriger ici.
>
> **Prérequis** : la notation des arbres et les conventions des documents « agent » de
> la famille (NkAnima 09 : notation, P1–P12 ; NKScena 03 : `Commun/` ; Nogee 06 :
> lignes engendrées, `Host` d'extension ; NKCraft 03 : actions `commun.`). Rappel de
> la notation : `Rôle "identifiant"   propriété: valeur`, indentée ; `Rôle✚` = rôle
> proposé (doc 7 §3.10), écris sa **doublure** ; `propriété✚` = propriété proposée ;
> `// +` = ajout de la refonte.

---

## 0. Ce qui est propre à NKUIDesign

### 0.1 Ce qui devient document, et ce qui reste `Host`

Tout n'est pas un document. **La chrome l'est ; les surfaces de travail ne le sont
pas.**

| devient un document `.nkgui` | reste un `Host` (peint par l'application) |
|---|---|
| menus, barre d'onglets (cadre), bascule de mode, barres d'outils flottantes, grappe de la toile, cadres des panneaux, en-têtes, **inspecteur** (catégories et lignes), barre d'état, rails de pastilles, dialogues, fenêtres Thème / Actions / Contrôles / Fidélité / Tests / Carte, accueil, préférences | **la toile Design** et ses cadres, les poignées, les guides ; **la toile Behavior** (les nœuds et les fils du Blueprint) ; **la frise d'animation** ; **l'arbre de la Structure** et de l'**Explorateur** ; **l'éditeur de source** ; le **sélecteur de couleur** (radial, arrêts, pipette) ; le **graphe des inclusions** ; la **carte** de l'application ; les **cadres de plateforme** de la simulation ; les **aperçus** |

📌 **La règle** : si une surface a **ses propres gestes** (glisser une poignée, tirer un
fil, tracer), c'est un `Host`. Si elle ne fait que **présenter des commandes et des
valeurs**, c'est un document.

### 0.2 Le code existant, et comment le remplacer

L'interface est aujourd'hui codée à la main : `Panels.h` (~21 000 lignes), `main.cpp`
(~10 500, dont la barre de menus l. 8418–8800), `MenuContexte.h`, avec le kit
`designkit::` et `costume::`. **Même méthode que NKCraft** : panneau par panneau,
derrière un interrupteur (`NK_UID_UI=doc`), **témoin par actions émises et valeurs
liées**, puis suppression du code remplacé. **Jamais** de suppression avant que le
témoin passe.

### 0.3 Les identifiants d'actions

Les menus actuels n'ont **pas d'identifiants textuels** : ils appellent des fonctions,
et le menu contextuel une énumération (`NkActionCtx`). **Tu en crées**, par cette
règle :

- **domaine = le menu** où l'entrée vit (`fichier.`, `edition.`, `affichage.`,
  `objet.`, `cible.`, `comportement.`, `ia.`, `projet.`, `fenetre.`, `aide.`) ;
  `outil.`, `toile.`, `insp.`, `mode.`, `bp.` (Blueprint), `sim.`, `tests.`, `carte.`,
  `th.` (thème) pour ce qui n'est pas dans un menu ;
- **nom = le libellé en `snake_case` sans accents** ; pour une valeur de `NkActionCtx`,
  **son nom** en `snake_case` (`ExtraireComposant` → `objet.extraire_composant`) —
  **une** action par valeur de l'énumération, pour que le menu contextuel et la barre de
  menus émettent **la même** action ;
- ⚠️ **les clés d'état existantes** (`insp.pos.x`, `insp.onglets`, `canvas.snap`…) **ne
  sont pas des actions** : ce sont des clés d'état du kit. Garde-les comme **`bind`**.

### 0.4 L'état des actions

**Toute entrée de la barre de menus actuelle est ✅ si la sonde la voit câblée, ✚
sinon.** Les entrées affichées « (à brancher) » aujourd'hui (Aligner, Répartir) sont ✚.
Les nouvelles entrées de la refonte sont ✚. **Le document ne porte pas l'état** : c'est
le registre (doc 3 §24) qui le tient, et la simulation grise une action ✚ avec sa
raison (§9.3).

### 0.5 Les décisions qui te concernent

| décision | effet ici |
|---|---|
| **le `.nkgui` est le document** (doc 1 §4.1, D1) | aucun document de l'interface de NKUIDesign avant que l'outil sache **ouvrir** un `.nkgui` (R1) — sinon il ne pourrait pas éditer sa propre interface |
| **format 0.4 décidé** (D8, 27/09) | les panneaux qui éditent booléens, instructions, carte et tests **s'écrivent et se livrent** ; plus d'interrupteur d'attente |
| **Blueprints exécutés** (D11) | le panneau des comportements et la barre d'outils Behavior sont des documents ; la toile est un `Host` |
| **thème Rihen UE5** (D2), **thème par application** (D10) | NKUIDesign **porte** son thème dans `NKUIDesign/theme.nkgui` (§4) ; aucune couleur en dur dans ses documents |
| **l'action portée par un enfant de composant** (D12, 🔴) | un composant à **une** action la porte par sa **racine** (a) ; un composant à **plusieurs** actions (`EnTeteIdentite`, `CarteControle`, `LigneJeton`) donne à chaque enfant actif `id: "<action>"` (b) **et** un commentaire `// D12` — si Rodolf tranche autrement, la réécriture est mécanique |

---

## 1. Le modèle d'édition — construit depuis l'archive

⚠️ **Le changement de fond de la réécriture.** Le modèle d'édition (`NkUIDocument`,
`Document.h`, ~5 300 lignes) **ne possède plus rien que le fichier ne porte pas** :
il est **construit depuis** l'archive `.nkgui` (`NkGuiArchive`) et **écrit vers** elle.
Ce que l'éditeur seul retient va dans le `.nkgui.meta`.

### 1.1 Les trois couches

| couche | type | possède | source de vérité pour |
|---|---|---|---|
| **l'archive** | `NkGuiArchive` (NKSerialization) | le texte : nœuds, lexèmes, commentaires, lignes vides, style | **l'octet** — ce qui sera réécrit |
| **le modèle** | `NkUIDocument` (NKUIDesign) | une vue **éditable** : arbre des widgets et des formes, composants, comportements (script et graphe), thèmes, carte, tests ; la **sélection** ; l'**historique** | **le sens** — ce que l'utilisateur modifie |
| **la méta** | `NkUIDocumentMeta` | positions des cadres sur la toile, zoom, positions des nœuds de Blueprint, contenu fictif des `Host`, planches d'essai, notes | ce qui **n'est pas** l'interface |

**Invariants** (chacun a son témoin, §11) :

1. **Ouvrir puis enregistrer sans modification = le même octet.** L'écriture passe par
   l'archive, qui garde les lexèmes et la trivia (`NkGuiRoundTrip` le prouve déjà sur le
   corpus : équivalence **et** identité).
2. **Une modification ne touche que ses lignes.** Le modèle écrit **dans** l'archive par
   des opérations localisées (poser une propriété, insérer un nœud, retirer un bloc),
   jamais en réécrivant le fichier depuis le modèle.
3. **Un nœud du modèle garde la référence de son nœud d'archive** (et de sa position de
   lexème) : c'est ce qui permet (2), la synchronisation avec le mode Source et le
   positionnement des diagnostics.
4. **Ce qui est inconnu est gardé** : une section, un membre, une instruction que le
   modèle ne comprend pas reste **brut** dans l'archive et **visible** (marqué ◐) dans
   la Structure — jamais perdu, jamais réécrit.
5. **Le `.nkgui.meta` est facultatif** : un `.nkgui` sans lui s'ouvre (cadres rangés
   automatiquement, graphes rangés gauche → droite).

### 1.2 Ce que le modèle représente

| dans le fichier (doc 2) | dans le modèle | éditeur |
|---|---|---|
| `widgets` : nœuds de rôle | `NkUINode` (rôle, identifiant, propriétés typées selon le schéma du validateur, enfants) | toile Design, Structure, inspecteur |
| `appearance(État)` et ses effets | pile d'apparence **indexée par état** (doc 15 §15.7) | inspecteur Design, sélecteur d'état |
| `component` / instances | `NkComponentDecl` + instance (`instanceDe`, `ecarts`) — **doc 15**, et son passage au `.nkgui` au **§15.16** | bibliothèque, inspecteur |
| `geometry` (calques) et `geometry "nom"` (bibliothèque) | formes : `rect`, `ellipse`, `text`, `image`, `path` (ancres), `compound` (opération + opérandes) | toile, vectoriel, booléens |
| `behavior` script | `NkBehaviorProgram { representation = Code, source, ir }` | vue Code |
| `behavior … graph` | `NkBehaviorProgram { representation = Graph, graph, ir }` ; positions des nœuds dans la **méta** | toile Behavior |
| `controller` / `callback` | contrats | gestionnaire de callbacks, registre |
| `animation` | `NkTransitionAnim`, `NkAmbientAnim`, `NkDrivenEffect` (§1sexies) | toile Animation |
| `fonts` | déclarations de police | inspecteur, préférences |
| `theme` | thèmes, jetons (nom, type, famille, sens), variantes, renvois | éditeur de thème |
| `application` | carte : écrans, types, routes, départ, livraison des thèmes | carte, simulation |
| `test` | scénarios | panneau Tests, simulation |

⚠️ **Deux vocabulaires de taille** : le modèle connaît `fixed · content · fraction ·
weight · expand` et les bornes (§1ter.3) ; le format connaît `size`, `sizeRel`,
`minSize`, `maxSize`. La traduction est **à écrire et à dire** : `fraction` → `sizeRel`,
bornes → `minSize` / `maxSize`, `fixed` → `size` ; `weight`, `expand`, `content` n'ont
**pas d'équivalent** au format → compteur `taillesSansEquivalent`, marque ◐, **et** la
valeur est gardée dans la **méta** pour ne pas la perdre à l'aller-retour.

### 1.3 L'historique

- **Une seule pile** pour la toile, le Blueprint, le thème, la carte, les tests :
  annuler une modification de jeton s'annule comme un déplacement.
- **Une proposition de l'IA = une étape** (Propose / Commit ; `Retract` pour le retrait
  complet).
- **Éditer une déclaration de composant** propage aux instances (doc 15 §15.6, R1′) :
  la propagation **et** la modification forment **une** étape (piège n°1 du doc 12
  §12.4).
- **Une modification dans le mode Source** est une étape par validation du texte
  (perte du focus ou 500 ms d'inactivité), pas par caractère.

### 1.4 La migration `.nkuidoc`

`NkUIDocument::Load` lit encore le `.nkuidoc`. **Convertir** (Launcher, Fichier) :
1. lit le `.nkuidoc` ;
2. produit un `.nkgui` (via `NkGuiEcrire.h`, branché le 26/09) et un `.nkgui.meta` ;
3. **rapporte** ce qui est converti, ce qui ne l'est pas (un mode de taille sans
   équivalent, un style de calque sans jeton…) et **pourquoi** ;
4. laisse le `.nkuidoc` **intact** (renommé `<nom>.avant-nkgui`) ;
5. ne s'exécute **jamais** sans le geste de l'utilisateur.

📌 Les sections **§1ter à §1decies** qui suivent sont celles d'août : types `Nk*`,
codes de diagnostic, invariants. Elles sont **gardées telles quelles**. Là où elles
parlent de « l'export » ou du « document », lis « le `.nkgui` ». La **§1bis** d'août
(extensions de langage proposées) est **retirée** : ces extensions sont au doc 2.


## 1ter. Cible, ancrage, responsive, alignement — additions au modèle

> Découle de `3_NkUIDesign_Interface_Humaine.md` §8quater. Ces quatre notions
> entrent dans le **document**, pas dans l'éditeur : elles doivent survivre à la
> sérialisation et être relues à l'identique.

### 1ter.1 Cible portée par le cadre

```
NkFrameTarget {
    kind      : Desktop | Mobile | Tablet | Free
    width     : uint32        // px logiques
    height    : uint32
    density   : float32       // px physiques par px logique, défaut 1.0
    safeArea  : { top, right, bottom, left : uint32 }
}
```

⚠️ **La cible n'est pas cosmétique** : elle fournit la zone sûre (règle du corpus
« la zone sûre est une ancre, pas une marge »), la densité de référence, et la
taille de cible tactile minimale que la validation vérifie. Un cadre sans cible
explicite vaut `Free` **et le déclare** — il ne prend pas silencieusement les
valeurs du bureau.

### 1ter.2 Ancrage — quatre arêtes, relatives au PARENT

```
NkAnchorEdge { mode : None | Fixed | Proportional ; value : float32 }
NkAnchors    { left, right, top, bottom : NkAnchorEdge }
```

Règles de résolution, dans cet ordre :

1. deux arêtes opposées `Fixed` ou `Proportional` → l'élément **s'étire** sur cet
   axe, sa taille devient dépendante du parent ;
2. une seule arête active → distance conservée à ce bord, taille libre ;
3. aucune → **centré** sur cet axe.

⚠️ **Relatives au parent, jamais au cadre.** Un ancrage résolu contre le cadre
casserait dès qu'un élément change de parent — et le déplacement de parent est un
geste courant dans l'arbre de composition.

### 1ter.3 Taille — réutiliser le vocabulaire existant

`fixed <n>` · `content` · `fraction <n>` · `weight <n>` · `expand`

⚠️ **Ne pas introduire un second vocabulaire.** L'aperçu écrit déjà `fixed 260` →
`fixed 340` quand on tire un bord, et la sonde le vérifie. Deux vocabulaires pour
une même chose = deux sources de vérité, et c'est le défaut qui a laissé vivre le
magenta.

**Bornes.** Chaque dimension porte en plus un couple de bornes optionnelles :

```
NkSizeBounds {
    minPx : Optional<uint32>     // absent = pas de borne basse
    maxPx : Optional<uint32>     // absent = pas de borne haute
}
```

⚠️ **`Optional`, pas une sentinelle.** `0` pour « pas de minimum » et `UINT32_MAX`
pour « pas de maximum » sont des valeurs légitimes qu'un concepteur peut vouloir
saisir ; les employer comme sentinelles rend « borne à zéro » et « aucune borne »
indistinguables dans le modèle, donc dans le fichier, donc dans le diff.
L'interface les rend par un tiret `—` et non par un champ vide (doc 3 §8quater.2).

**Ordre d'application, normatif et non négociable :**

```
1. mode      -> taille candidate (fixed | content | fraction | weight | expand)
2. bounds    -> clamp(candidate, minPx, maxPx)
3. remplissage du parent  -> retranché de l'espace offert AVANT l'étape 1
```

⚠️ **Le clamp intervient après le mode, jamais avant.** Un `expand` borné à 320
prend l'espace restant *puis* est ramené à 320 ; l'espace libéré retourne à la
répartition entre frères. Clamper d'abord donnerait une répartition calculée sur
une taille que personne n'occupe.

**Validation :**

| code | condition |
|---|---|
| `E-BOUNDS-INVERTED` | `minPx > maxPx` — refusé à la saisie, pas arbitré |
| `W-BOUNDS-UNREACHABLE` | le mode ne peut jamais atteindre l'intervalle (ex. `fraction 1` avec `maxPx` très inférieur à la dimension du parent) |
| `W-BOUNDS-INERT` | bornes posées sur un `fixed` — sans effet, signalées, non supprimées |

⚠️ **`W-BOUNDS-INERT` avertit, il ne nettoie pas.** Une borne inerte aujourd'hui
redevient active dès que le mode change ; l'effacer d'office détruirait une
intention au moment précis où elle est invisible.

**Pas de troisième champ de taille.** Le modèle porte `mode` (avec son paramètre)
et `bounds`. Aucune `defaultSize`. Sur `fixed`, un tel champ dupliquerait le
paramètre du mode ; sur `expand`/`content`, il désignerait une valeur que
personne ne calcule.

```
NkSizeSpec {
    mode   : Fixed(u32) | Content | Fraction(f32) | Weight(f32) | Expand
    bounds : NkSizeBounds
}
```

**Mémoire d'édition — hors modèle, hors fichier :**

```
// Etat d'EDITEUR uniquement. Jamais serialise dans .nkgui.
NkSizeModeMemory {
    lastFixed    : Optional<uint32>
    lastFraction : Optional<float>
    lastWeight   : Optional<float>
}
```

⚠️ **Cette mémoire ne va pas dans le document.** La sérialiser la ferait
apparaître dans les diffs, les revues et les fusions, pour une valeur sans aucun
effet sur le rendu — du bruit que les relecteurs apprendraient à ignorer, ce
qu'ils finissent toujours par faire aussi pour le reste.

**Rendu de la ligne (doc 3 §8quater.2) :** une ligne par dimension ; marqueur
lecture seule `min–max` affiché **si et seulement si** `bounds` n'est pas vide
(`…` du côté absent) ; clic = popover `mode + min + max`.

⚠️ **Le marqueur n'est pas décoratif, il est la condition du popover.** Masquer
les champs sans afficher l'existence de la borne échange un encombrement mesuré
contre une classe de bug non mesurable.

### 1ter.3bis Fenêtre et curseur (cible Bureau)

```
NkWindowDecoration { Native | Client }

NkWindowRegion {
    kind   : Drag | ResizeN | ResizeS | ResizeE | ResizeW
           | ResizeNE | ResizeNW | ResizeSE | ResizeSW
    bounds : rect relatif au cadre
}

NkWindowSpec {
    decoration     : NkWindowDecoration
    regions        : liste de NkWindowRegion    // vide si Native
    maximizedStyle : { corners : float32, shadow : bool }
}

NkCursorSpec {
    kind : System(NkSystemCursor) | Custom {
        image    : NkAssetRef
        hotspot  : { x, y : uint32 }     // OBLIGATOIRE
        fallback : NkSystemCursor        // OBLIGATOIRE
    }
}
```

**Validation :**

| code | condition |
|---|---|
| `E-WINDOW-NO-DRAG-REGION` | `decoration == Client` et aucune région `Drag` — **bloque l'export** |
| `W-WINDOW-NO-RESIZE-EDGES` | `Client` sans régions de redimensionnement |
| `W-WINDOW-NO-CLOSE` | `Client` sans affordance de fermeture |
| `W-WINDOW-MAXIMIZED-CHROME` | `maximizedStyle.corners > 0` ou `shadow == true` |
| `E-CURSOR-NO-HOTSPOT` | `Custom` sans point chaud |
| `E-CURSOR-NO-FALLBACK` | `Custom` sans doublure système |
| `W-WINDOW-IGNORED-ON-TARGET` | `NkWindowSpec` non vide sur une cible Mobile ou Web |

⚠️ **`E-WINDOW-NO-DRAG-REGION` bloque l'export, il n'avertit pas.** Une fenêtre en
décoration Client sans zone de saisie **ne peut pas être déplacée**, et rien dans
l'éditeur ne le montre : elle se dessine, elle s'exporte, et l'utilisateur final
reçoit une fenêtre clouée à son écran. Un avertissement se lit et s'oublie ; ici la
conséquence est inutilisable, donc le refus est proportionné.

⚠️ **`hotspot` et `fallback` sont des champs requis du variant `Custom`, pas des
`Optional` avec une valeur par défaut.** Un point chaud par défaut à (0,0) fait
cliquer à côté de la cible en permanence sans que personne n'en identifie la cause ;
une doublure absente laisse l'utilisateur **sans curseur du tout** quand le
chargement échoue — il ne peut alors ni viser, ni comprendre, ni le signaler.

⚠️ **La taille du curseur personnalisé se résout à l'exécution**, en combinant la
densité de l'écran et le **réglage système de taille de curseur**. Fixer la taille
à la conception ignore un réglage d'accessibilité — et l'ignore exactement pour les
personnes qui l'ont activé.

⚠️ **Changer de cible ne supprime jamais `NkWindowSpec`.** Il est conservé, marqué
inactif, et signalé par `W-WINDOW-IGNORED-ON-TARGET`. Un travail effacé par un
changement de cible ne se redécouvre qu'au retour, quand il n'est plus là.

### 1ter.4 Points de rupture

```
NkBreakpoint {
    axis       : Width | Height
    comparator : Below | AtLeast
    threshold  : uint32
    overrides  : liste de (propriété, valeur)   // ancrage, taille, visibilité
}
```

Portés par l'élément, évalués **du plus général au plus spécifique**, la valeur de
base restant toujours présente et lisible.

⚠️ **Un point de rupture inatteignable doit se signaler** — seuil de 2000px sur un
cadre mobile de 390px. La validation le compte et le nomme ; elle ne le supprime
pas. *Une règle morte qui ne se dit pas est du travail perdu qu'on croit fait.*

### 1ter.5 Marge, remplissage, espacement

```
NkEdgeUnit { Px | PercentParent | PercentSelf }

NkEdgeValue { value : float32; unit : NkEdgeUnit }

NkEdges { top, right, bottom, left : NkEdgeValue }

NkSpacing {
    padding : NkEdges      // bord -> contenu, vers l'interieur
    margin  : NkEdges      // bord -> voisinage, vers l'exterieur
    gap     : { h, v : NkEdgeValue }   // entre enfants
}
```

⚠️ **`PercentSelf` est refusé si le mode de la dimension correspondante est
`Expand` ou `Weight`** — code `E-SPACING-CYCLE`. Sur ces deux modes la taille
dépend de l'espace restant, l'espace restant dépend de la marge, la marge
dépendrait de la taille. **Refuser à la saisie, pas casser le cycle au calcul :**
un cycle brisé par une règle interne produit une disposition correcte au pixel
près et inexplicable à la lecture.

⚠️ **`PercentSelf` se résout après le clamp des bornes**, sur la taille finale de
l'élément — sinon un `content` borné à `max 320` donnerait une marge calculée sur
une largeur que l'élément n'occupe pas.

⚠️ **La sérialisation écrit l'unité, jamais la valeur résolue.** Écrire les pixels
calculés ferait qu'un document relu dans un parent de taille différente ne
reproduirait pas la disposition enregistrée — le même défaut que `gap` simulé par
des marges, une ligne plus bas.

**Ligne `Rapport` du popover de taille (doc 3 §8quater.2) :** pur sucre d'édition.
L'interface résout `W = k·g` et `W + 2g = P` en `g = P/(k+2)`, puis écrit deux
`NkAnchorEdge` proportionnelles. **`k` n'est stocké nulle part.** Le stocker
ferait du modèle un système de contraintes ; ce n'est pas ce produit.

⚠️ **La règle de composition avec l'ancrage, à implémenter une seule fois et à
documenter dans le code** :

> **Le `padding` du parent définit sa zone de contenu. Le `margin` de l'enfant
> définit sa boîte de marge. L'ancre se mesure entre les deux.**

Donc `distance visible = padding(parent) + ancre + margin(enfant)`. **Les trois
s'additionnent ; aucun ne remplace l'autre.** Toute autre convention produira des
écarts de quelques pixels que personne ne saura expliquer — et qu'on attribuera au
moteur de rendu.

⚠️ **`gap` n'est pas simulable par des marges** et ne doit pas être implémenté
ainsi : des marges de `n/2` sur chaque enfant donnent bien `n` entre eux, **mais
aussi `n/2` contre les bords du parent**. La sérialisation doit distinguer les deux
cas, sinon relire un document produit une disposition différente de celle
enregistrée.

---

### 1ter.6 Alignement

```
NkAlign      { Start | Center | End | Stretch }
NkDistribute { None | SpaceBetween | SpaceAround | SpaceEvenly }

NkLayoutAlign {
    self     : { h : NkAlign, v : NkAlign }        // dans le parent
    content  : { h : NkAlign, v : NkAlign, d : NkDistribute }  // des enfants
}
NkTextAlign {
    horizontal : Start | Center | End | Justify
    vertical   : Start | Center | End
    baseline   : bool     // aligner sur la ligne de base plutôt que sur la boîte
}
```

⚠️ `baseline` n'est pas un raffinement typographique : deux textes de corps
différents posés côte à côte paraissent bancals sans lui, **et personne ne sait
dire pourquoi**. C'est exactement le genre de défaut qui se ressent avant de se
voir.

---

## 1quater. Rôle d'un composant — le contrat

> Découle de `3_NkUIDesign_Interface_Humaine.md` §14ter.

```
NkRoleOrigin { Native | Project }

NkRoleContract {
    id             : NkString              // "button", "checkbox", …
    origin         : NkRoleOrigin
    derivesFrom    : NkRoleContract*       // Project : natif etendu, ou nul
    states         : liste de NkStateId    // repos, survol, pressé, désactivé, focus
    events         : liste de NkEventDecl  // nom + charge utile
    properties     : liste de NkPropDecl
    expectedChildren : liste de NkChildSlot // libellé requis, icône optionnelle…
    a11y           : { minTargetPx : uint32, minContrast : float32 }
}

NkRoleBinding {                 // ce que porte l'élément
    contract     : NkRoleContract*   // nul si aucun rôle
    userEvents   : liste de NkEventDecl   // ajoutés par l'utilisateur
    mutedEvents  : liste de NkEventId     // du contrat, désactivés
    stateOverrides : ...                  // apparence redéfinie par état
}
```

**Trois invariants, à faire respecter par le code :**

1. **`userEvents` survit au retrait du rôle.** Détacher un contrat vide
   `contract`, `mutedEvents` et `stateOverrides` — **jamais** `userEvents`. On perd
   le contrat, on ne perd pas le travail.
2. **Un événement du contrat n'est jamais supprimé, seulement `muted`.** Sinon
   réattribuer le rôle plus tard ne le restaure pas.
3. **`expectedChildren` non satisfait n'est pas une erreur** — c'est un
   **avertissement de validation nommé**. Un bouton sans libellé est peut-être un
   bouton-icône délibéré.

4. **Changer de contrat convertit en `userEvents` tout événement de l'ancien
   contrat qui portait une liaison** — il n'est jamais supprimé. La liaison désigne
   un callback qui existe dans le code ; la perdre casse la compilation **loin du
   geste qui l'a causée**, et personne ne remonte de l'erreur au changement de
   rôle.
5. **`origin == Project` exige que `derivesFrom` soit explicitement rempli ou
   explicitement nul.** Pas de défaut implicite. Un rôle de projet sans dérivation
   n'a d'interaction que celle de ses enfants, et l'éditeur doit le dire à la
   création — sinon il promet un comportement que rien n'implémente, et l'écart ne
   se découvre qu'à l'exécution.

**Le catalogue natif est énuméré en doc 3 §14ter.3**, relevé dans
`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h`. — `interrupteur` et
`bouton radio` **n'y figurent pas** : NKGui n'a ni `Switch` ni `RadioButton`. Ne
pas les déclarer `Native` tant que le moteur ne les porte pas.

⚠️ **`BeginDragSource` / `BeginDropTarget` sont des capacités, pas des rôles** —
un drapeau sur `NkRoleBinding`, jamais une entrée de catalogue. Sinon chaque
capacité transversale double le nombre de contrats.

⚠️ **La modalité non plus n'est pas un rôle.** NKGui la porte en état de contexte
(`modalDepth`, `appModal`, couche 100) ; c'est un **drapeau de `fenêtre`**. Mêmes
états, mêmes événements, mêmes propriétés qu'une fenêtre ordinaire.

**Inventaire complet des manques : doc 3 §14ter.4.** Trois statuts — natif,
dérivable (zéro moteur), code.

⚠️ **Une seule primitive manquante débloque quatre rôles** : `bouton radio`,
`groupe de boutons segmentés`, `accordéon exclusif` et `groupe exclusif` butent
tous sur **une seule sélection vivante parmi des frères**. C'est un chantier, pas
quatre — et c'est la frontière exacte entre ce qui se compose et ce qui se code :
la composition cesse de suffire dès qu'un élément doit en éteindre un autre.

⚠️ **Un contrat `origin == Native` dont l'API NKGui n'existe pas ne doit pas
être enregistré, même désactivé** — code `E-ROLE-UNBACKED`. Un catalogue qui
annonce ce qu'il n'a pas se fait apprendre par cœur comme étant faux.

⚠️ **`origin == Native` n'est pas créable depuis l'éditeur.** Un rôle natif est
adossé à un widget NKGui ; en fabriquer un depuis l'interface donnerait un `id`
que le runtime ne sait pas instancier. La liste des rôles natifs est **fermée et
fournie par le moteur**.

⚠️ **La composition est autorisée** : un élément porte un rôle **et** peut contenir
des enfants qui portent les leurs. Aucune règle d'exclusion.

---

## 1quinquies. Disponibilité d'un élément

> Doc 3 §14quater.

```
NkAvailability { Enabled | Disabled | ReadOnly | Busy }

NkAvailabilitySource {
    Constant                       // pose par l'auteur
  | Bound(NkExprRef)               // liee a une condition
  | Inherited                      // calculee : un ancetre est Disabled/Busy
}

NkAvailabilityState {
    value  : NkAvailability
    source : NkAvailabilitySource
    reason : Optional<NkString>    // affichee au survol de la REGION enveloppante
}
```

⚠️ **`Inherited` est calculé, jamais stocké.** Le sérialiser produirait deux
vérités sur le même fait : celle de l'ancêtre et la copie chez l'enfant — et elles
divergeraient au premier déplacement dans l'arbre.

⚠️ **Un descendant ne peut pas remonter à `Enabled` sous un ancêtre `Disabled`.** La
résolution prend le **minimum** le long du chemin depuis la racine. Autoriser la
réactivation locale viderait « ce panneau est désactivé » de son sens : il faudrait
parcourir tout le sous-arbre pour savoir ce qui reste cliquable.

**Conséquences à implémenter, pas seulement à peindre :**

| état | test de pointage | ordre de tabulation | sélection du texte |
|---|---|---|---|
| `Enabled` | oui | oui | oui |
| `Disabled` | **non** | **exclu** | non |
| `ReadOnly` | **oui** | **oui** | **oui** |
| `Busy` | non | exclu | non |

⚠️ **`Disabled` retire du test de pointage.** Repeindre sans bloquer donne un
contrôle qui a l'air mort et agit vivant ; le défaut apparaît dans le callback,
loin de sa cause.

⚠️ **Si l'élément focalisé devient `Disabled` ou `Busy`, le focus se déplace au
suivant focalisable.** Le laisser en place immobilise la navigation au clavier sur
un élément muet, sans rien à l'écran pour l'indiquer.

**Validation :**

| code | condition |
|---|---|
| `E-DISABLED-REENABLE` | un descendant tente `Enabled` sous un ancêtre `Disabled` |
| `W-DISABLED-CONTRAST` | l'état `désactivé` viole `a11y.minContrast` |
| `W-REASON-UNREACHABLE` | une `reason` posée sans région enveloppante active |
| `W-READONLY-AS-DISABLED` | un champ de saisie `Disabled` dont la valeur n'est jamais modifiée par le programme — `ReadOnly` était probablement voulu |

⚠️ **`W-DISABLED-CONTRAST` n'est pas du zèle.** C'est l'état qu'on oublie de
tester, parce qu'on le regarde rarement et qu'on lui accorde le droit d'être pâle —
alors qu'il faut pouvoir **lire** un contrôle pour comprendre pourquoi il n'est pas
disponible.

⚠️ **`W-REASON-UNREACHABLE` attrape un piège coûteux** : une infobulle attachée à
l'élément désactivé lui-même est écrite, enregistrée, exportée — et ne s'affiche
jamais, puisque l'élément ne reçoit plus le survol.

**Lien avec la couverture de simulation (doc 3 §18bis.2)** : un point de connexion
non atteint dont l'élément est resté `Disabled` toute la session est reporté
`jamais atteint : ancetre desactive`. **C'est la seule cause de non-atteinte que
l'outil puisse affirmer** ; les autres demandent un jugement humain.

---

## 1sexies. Les trois familles d'animation

> Doc 3 §9ter.

```
NkReduceMotion { Stop | Shorten | Keep }

// A -- transition entre etats, declenchee par un evenement (doc 3 9bis)
NkTransitionAnim {
    fromState, toState : NkStateId
    event    : NkEventId
    duration : uint32          // ms
    curve    : NkCurve
    reduce   : NkReduceMotion  // defaut Shorten
}

// B -- ambiance : tourne tant que l'etat dure
NkAmbientAnim {
    state     : NkStateId      // OBLIGATOIRE : une ambiance appartient a un etat
    track     : NkPropertyTrack
    duration  : uint32
    repeat    : Infinite | Times(uint32)
    direction : Forward | PingPong
    delay     : uint32
    jitter    : uint32         // decalage aleatoire max, en ms
    reduce    : NkReduceMotion // defaut Stop
}

// C -- effet continu : une liaison, pas une timeline
NkDrivenEffect {
    source   : PointerX | PointerY | PointerDistance | Time | Scroll
             | PropertyValue(NkPropRef)
    target   : NkPropRef            // y compris un parametre de la pile d'effets
    inRange  : { min, max : float32 }
    outRange : { min, max : float32 }
    curve    : NkCurve
    smoothing: float32              // 0 = suit au pixel, 1 = tres amorti
    atRest   : float32              // OBLIGATOIRE : valeur quand la source manque
    reduce   : NkReduceMotion       // defaut Stop
}
```

⚠️ **`NkAmbientAnim.state` est requis, pas optionnel.** Une ambiance attachée à
l'élément plutôt qu'à un état continue de tourner pendant `Pressed` et pendant
`Disabled` — un bouton gris qui respire, et rien dans le document pour expliquer
d'où vient le tremblement.

⚠️ **`NkDrivenEffect.atRest` est requis, pas `Optional`.** Sans pointeur — écran
tactile, navigation au clavier — l'effet n'a pas de source ; sans valeur de repos
déclarée, l'élément **conserve la dernière valeur reçue**. Une carte inclinée de
travers en permanence, sur l'appareil de quelqu'un d'autre, sans cause lisible.

⚠️ **`reduce` n'a pas le même défaut selon la famille.** Les ambiances et les effets
continus s'**arrêtent** ; les transitions se **raccourcissent**. Supprimer les
transitions ferait perdre le retour visuel qui dit qu'un clic a été pris en compte
— le réglage vise le mouvement continu, pas l'accusé de réception.

**Validation :**

| code | condition |
|---|---|
| `E-AMBIENT-NO-STATE` | `NkAmbientAnim` sans `state` |
| `E-DRIVEN-NO-REST` | `NkDrivenEffect` sans `atRest` |
| `W-AMBIENT-COUNT` | plus de N ambiances actives simultanément sur une page |
| `W-AMBIENT-IN-PHASE` | même ambiance sur plusieurs instances avec `jitter == 0` |
| `W-REDUCE-KEEP` | `reduce == Keep` sur une animation purement décorative |

⚠️ **`W-AMBIENT-COUNT` ne protège pas que la batterie.** Une page qui n'atteint
jamais l'état de repos rend une capture automatique non reproductible, empêche un
test visuel de conclure, et fait annoncer des changements en boucle par un lecteur
d'écran. **L'état de repos est une condition technique, pas un confort.**

---

## 1septies. Greffons — le contrat d'extension

> Doc 3 §20bis. ⚠️ **Contrat d'écosystème, pas de NkUIDesign** : à déplacer dans le
> document partagé dès qu'une deuxième application l'utilise.

```
NkGreffonId = NkString          // "lumen" -- prefixe de TOUTES ses contributions

NkGreffonPermission {
    ReadDocument | WriteDocument | FileRead | FileWrite
  | Network | Clipboard | SpawnProcess
}

NkGreffonManifest {
    id          : NkGreffonId
    name        : NkString
    publisher   : NkString
    version     : NkVersion
    hostRange   : { min, max : NkVersion }
    permissions : NkFlags<NkGreffonPermission>
    contributions : liste de NkContributionDecl
    migrations  : liste de { fromVersion, oldKey, newKey }
    signature   : Optional<NkSignature>
}

NkGreffonState { Active | Disabled | Rejected }

NkGreffonRecord {
    manifest    : NkGreffonManifest
    state       : NkGreffonState
    rejectCause : Optional<NkString>   // phrase lisible, pas un code
    frameCostUs : uint32               // mesure, mise a jour en continu
}
```

### 1septies.1 Le rejet est une DONNÉE, jamais une exception

⚠️ **`hostRange` se vérifie avant de charger la moindre ligne de code du greffon.**
Un greffon hors intervalle devient `Rejected` avec sa `rejectCause`, et l'hôte
démarre normalement.

Charger d'abord et rattraper l'exception ensuite ne suffit pas : le code a déjà
tourné, il a pu enregistrer des contributions et modifier de l'état global. **Et
si le plantage survient au démarrage, l'utilisateur ne peut plus ouvrir l'outil
pour retirer le greffon fautif** — la seule sortie devient la ligne de commande.

### 1septies.2 Le préfixe est imposé à l'enregistrement, pas par convention

Toute clé de contribution vaut `id + "." + nomLocal`. **L'hôte la compose
lui-même** ; le greffon ne fournit que `nomLocal`.

⚠️ **Laisser le greffon écrire sa clé complète revient à lui faire confiance pour
respecter une convention** — et il suffit d'un greffon distrait pour écraser les
contributions d'un autre. Le déchargement, lui, retire **exactement** les clés
portant le préfixe : c'est le préfixe imposé qui rend cette opération sûre.

| code | condition |
|---|---|
| `E-GREFFON-HOST-RANGE` | version d'hôte hors `hostRange` |
| `E-GREFFON-KEY-COLLISION` | deux greffons revendiquent la même clé complète |
| `E-GREFFON-NATIVE-ROLE` | un greffon déclare un rôle `origin == Native` |
| `W-GREFFON-UNSIGNED` | `signature` absente |
| `W-GREFFON-SLOW` | `frameCostUs` au-dessus du seuil |

### 1septies.3 Élargir les permissions redemande l'accord

À la mise à jour, l'hôte compare l'ancien et le nouveau `permissions`. **Tout bit
ajouté suspend l'activation et redemande le consentement**, en ne montrant que les
bits nouveaux.

⚠️ **Comparer les ensembles, pas les versions.** Une mise à jour « mineure » qui
gagne `Network` est exactement le schéma par lequel des écosystèmes entiers se
sont fait prendre — le numéro de version ne dit rien de ce que le code fait.

### 1septies.4 Isolation et coût

Tout appel dans un greffon est encadré : une erreur le fait passer `Disabled` avec
sa cause, **sans se propager à l'hôte**. Le temps passé est cumulé par greffon et
par image dans `frameCostUs`.

⚠️ **Sans cette mesure par greffon, un greffon lent devient « l'application est
lente »**, et l'attribution est impossible : on optimise du code d'éditeur pendant
que la cause est une extension installée trois semaines plus tôt.

### 1septies.5 Dépendances du document — et la règle qui les rend tenables

```
NkDocumentDependency {
    greffonId : NkGreffonId
    version   : NkVersion
    usedKeys  : liste de NkString
}
```

⚠️ **Le sérialiseur doit conserver la représentation BRUTE des éléments dont le
greffon est absent, et la réécrire à l'identique.** C'est la contrainte
d'implémentation qui découle de « ouvrir sans greffon ne supprime rien » : on ne
peut pas préserver ce qu'on ne sait pas modéliser en le passant par le modèle. Il
faut garder le texte tel quel.

Sans cette règle, ouvrir puis enregistrer **détruit du travail en silence** — et la
perte n'apparaît qu'à la prochaine ouverture sur la machine qui, elle, avait le
greffon.

| code | condition |
|---|---|
| `E-DEP-MISSING` | greffon absent — éléments préservés, **export bloqué** |
| `E-DEP-KEY-GONE` | greffon présent, clé disparue et **aucune migration** ne la couvre |

⚠️ **`E-DEP-KEY-GONE` nomme la clé exacte.** Échouer en nommant ce qui a disparu
se corrige ; afficher un élément vide ne se remarque même pas.

---

## 1octies. Console et backend graphique

> Doc 3 §20ter.

```
NkConsoleTab { Validation | Simulation | Greffons | Systeme }

NkSystemInfo {
    hostVersion   : NkVersion
    backend       : NkRhiBackend      // UNIQUE : editeur ET simulation
    backendAsked  : NkRhiBackend      // ce qui etait demande
    driver        : NkString
    adapter       : NkString
    vramMo        : uint32
    dpiScale      : float32
    fps           : float32
    ambientCount  : uint32            // doc 3 9ter.4
    greffons      : liste de NkGreffonRecord
}
```

⚠️ **`backend` est un champ unique, partagé par l'éditeur et la simulation.** Deux
champs distincts rendraient possible — donc inévitable — qu'ils divergent, et la
promesse de §18 (« le même moteur que l'application finale ») tomberait sans que
rien ne le signale.

⚠️ **`backendAsked` existe pour rendre le repli VISIBLE.** Si l'initialisation
échoue et que l'hôte retombe sur un autre backend, `backend != backendAsked` et la
console le dit. Un repli silencieux est le pire cas : les couleurs changent, les
performances changent, **et rien dans l'interface n'explique pourquoi**.
*(L'historique du magenta et l'état de NkSL — cinq backends propres sur six —
rendent ce cas concret.)*

| code | condition |
|---|---|
| `W-BACKEND-FALLBACK` | `backend != backendAsked` |
| `W-BACKEND-NOT-TARGET` | la cible déclare une plateforme dont le backend n'est pas celui-ci |

⚠️ **`W-BACKEND-NOT-TARGET` n'est pas une erreur à corriger, c'est un écart à
nommer.** Travailler en Vulkan pour livrer en DirectX 12 est légitime ; **croire
qu'on a vérifié le rendu de la cible ne l'est pas.** Un seul backend supprime
l'écart éditeur/simulation ; il ne supprime pas l'écart simulation/livraison, il
le déplace.

`NkSystemInfo` expose **une représentation texte copiable en une action** : c'est
ce qu'on demande à quelqu'un qui signale un défaut.

---

## 1nonies. Menus — un registre unique de commandes

> Doc 3 §5bis.

```
NkCommandId = NkString

NkCommand {
    id          : NkCommandId
    label       : NkString
    kind        : Action | Submenu | Toggle | Separator
    shortcut    : Optional<NkChord>
    enabledWhen : NkPredicate
    checkedWhen : Optional<NkPredicate>
    opensDialog : bool          // rend le suffixe "..." -- doc 3 5bis.1
}
```

### 1nonies.1 Un seul registre, deux présentations

⚠️ **La barre de menu et les menus contextuels se construisent à partir du MÊME
registre**, filtré par `enabledWhen` et par le contexte. Jamais deux listes.

Deux listes, et la même commande finit par se comporter différemment selon qu'on
l'atteint par la barre ou par le clic droit — un écart que personne ne pense à
tester, parce qu'on suppose que c'est « le même bouton ».

### 1nonies.2 L'unicité des raccourcis se vérifie au démarrage

⚠️ **Le registre refuse deux commandes portant le même `NkChord`** — code
`E-SHORTCUT-DUPLICATE`, **vérifié à la construction du registre, pas à la frappe.**

C'est la forme implémentable d'une leçon du 2026-08-20 : `F1` et `F2` étaient
attribués deux fois chacun, **et ces collisions n'existaient nulle part tant que
les raccourcis n'étaient pas rassemblés dans une seule table.** Éparpillés, ils se
contredisaient sans que rien ne le montre — on ne l'aurait découvert qu'en
appuyant sur la touche.

*Règle d'arbitrage retenue : **c'est le nouveau venu qui cède, jamais l'habitude.***
`F1` reste à l'aide, `F2` au renommage ; les modes de canvas sont passés sur
`Ctrl+1..5` (`Ctrl+5` pour Source, le 27/09).

### 1nonies.3 Bascules et libellés

⚠️ **Une commande `Toggle` porte `checkedWhen` et un libellé INVARIANT.** Un
libellé qui s'inverse — « Afficher la grille » / « Masquer la grille » — oblige à
déduire l'état courant de l'action proposée. On se trompe une fois sur deux, et le
modèle ne peut même pas exprimer l'état.

### 1nonies.4 Grisage et retrait

- **barre de menu** : une entrée de premier niveau ne disparaît jamais, elle se
  grise. Seuls des blocs entiers sans objet se retirent ;
- **menu contextuel** : même règle, mêmes motifs — on retire des blocs, on ne
  déplace pas les premières lignes ;
- **sélection multiple** : une commande n'apparaît que si `enabledWhen` est vraie
  pour **tous** les éléments sélectionnés.

⚠️ **La position d'une entrée est apprise par la main.** Un menu dont la forme
change à chaque contexte détruit ce que l'utilisateur a mémorisé, et le coût se
paie à chaque ouverture sans que personne sache le nommer.

---

## 1decies. Infobulles

> Doc 3 §14quinquies.

```
NkTooltipSide { Auto | Top | Bottom | Left | Right }

NkTooltip {
    content     : NkNodeRef          // texte OU sous-arbre
    side        : NkTooltipSide
    showDelayMs : uint32
    hideDelayMs : uint32
    groupMs     : uint32             // fenetre pendant laquelle le voisin s'affiche sans delai
    onFocus     : bool               // defaut true
}
```

⚠️ **`NkTooltip` est un CHAMP d'un élément, jamais un nœud de l'arbre.** Frère, il
n'a pas d'ancre ; enfant, il entre dans le calcul de disposition du parent et le
déforme. Son `content` vit dans un sous-arbre **hors flux**.

⚠️ **`groupMs` est le réglage qui décide de l'utilisabilité.** Sans lui, parcourir
dix icônes coûte dix fois `showDelayMs`, et l'utilisateur renonce à chercher.

**À implémenter, pas seulement à peindre :**

| propriété | valeur |
|---|---|
| test de pointage | **désactivé** — l'infobulle est transparente aux clics |
| couche | au-dessus de tout sauf des modales |
| basculement | obligatoire si elle sortirait de l'écran **ou du cadre simulé** |
| recouvrement de l'ancre | interdit |
| `Échap` | ferme sans déplacer le focus |

⚠️ **Le test de pointage désactivé n'est pas un détail.** Une infobulle qui avale
le clic rend inaccessible ce qu'elle décrit — au moment précis où l'utilisateur,
ayant lu, veut agir.

**Validation :**

| code | condition |
|---|---|
| `W-TOOLTIP-REDUNDANT` | le texte reproduit le libellé visible de l'élément |
| `W-TOOLTIP-MISSING` | élément interactif **sans libellé visible** et sans infobulle |
| `E-TOOLTIP-ONLY-SOURCE` | une information marquée requise n'existe QUE dans une infobulle |
| `W-TOOLTIP-ON-DISABLED` | infobulle posée directement sur un élément `Disabled` — elle ne s'affichera jamais (doc 3 §14quater.5) |

⚠️ **`E-TOOLTIP-ONLY-SOURCE` est une erreur, pas un avertissement.** Sur écran
tactile il n'y a pas de survol ; au clavier, l'infobulle ne s'affiche que si
`onFocus` ; sous lecteur d'écran elle peut n'être jamais annoncée. **Une
information qui n'existe que là n'existe pas pour une partie des utilisateurs** —
et le concepteur ne peut pas le voir, puisque lui la voit.

---

---

## 2. Le fil `.nkgui` — ce qu'il faut construire

### 2.1 Ce qui existe (mesuré)

| brique | fichier | état |
|---|---|---|
| lecture / écriture à l'octet | `NKSerialization/NkGuiArchive` | ✅ corpus 10 / 10 équivalents et identiques ; 36 contrôles |
| validation contre le vocabulaire | `NKUIDesign/NkGuiValidate.h` | ✅ 41 rôles, alias, apparence, états, formes, sections |
| aller-retour et détection du style | `NKUIDesign/NkGuiRoundTrip.h` | ✅ |
| écriture depuis le modèle (export) | `NKUIDesign/NkGuiEcrire.h` | ✅ branché le 26/09, avec `sizeRel` |
| inclusions, composants | `NKGui/Doc/NkGuiInclusions`, `NkGuiComposants` | ✅ |
| montage, interaction, événements, coquille | `NKGui/Doc/NkGuiMonteur.h`, `NkGuiInteraction.h`, `NkGuiEvenements.h`, `NkGuiCoquille.h` | ✅ |
| **ouvrir un `.nkgui` dans le modèle** | — | ✚ **le premier chantier (R1)** |

### 2.2 À construire, dans l'ordre

1. **`NkUIDocument::FromArchive(const NkGuiArchive&)`** : construit le modèle, chaque
   nœud gardant sa référence d'archive (§1.1, invariant 3). Les sections et membres
   inconnus restent bruts et sont listés.
2. **Les opérations d'écriture localisées** sur l'archive : `PoserPropriete(noeud,
   nom, lexeme)`, `RetirerPropriete`, `InsererNoeud(parent, index, texte)`,
   `RetirerNoeud`, `DeplacerNoeud`, `RenommerIdentifiant` (qui met à jour les références
   dans les comportements et les tests **du même document**, et **liste** celles des
   autres documents avant de les toucher). Chaque opération **écrit dans le style
   détecté** du fichier (indentation, espaces autour de `=`, un membre par ligne ou
   bloc en ligne).
3. **`NkUIDocumentMeta`** : lecture / écriture du `.nkgui.meta` (format `.nkgui` lui-même,
   section `meta` inconnue du runtime — un seul lecteur).
4. **Le projet** : un dossier ; index des documents, des `include` (graphe), des
   applications (cartes), des thèmes, des tests ; surveillance de fichier
   (`NkFileWatcher`) — un document modifié hors de l'éditeur est **relu**, et si
   l'éditeur a des modifications non enregistrées sur ce document, il **demande**.
5. **Le mode Source** : l'éditeur de texte (`Host`) lit **l'archive** ; une édition du
   texte relit le document (validation incrémentale), la sélection est partagée par les
   références d'archive.
6. **La validation continue** : le validateur tourne à chaque modification (sur le
   document ouvert), les diagnostics portent la position d'archive.

### 2.3 Le témoin de R1

Les six documents existants (`Resources/Interface/NkAnimaEditor/*.nkgui`,
`Resources/Interface/Nogee/*.nkgui`) : ouverts, **une** propriété modifiée à la souris
(le libellé d'un bouton), enregistrés — `git diff` montre **une ligne**. Puis **100
modifications aléatoires** par la toile, chacune suivie de : relire le fichier, le
valider, comparer le modèle relu au modèle en mémoire → **identiques**.

---

## 3. Le compilateur de comportement

### 3.1 La représentation intermédiaire

Une seule, pour le script et le Blueprint (doc 2 §6.6) :

```
NkBehaviorIR
  instructions : liste de
    Assign   { cible (variable | widget.propriété), expr }
    Branch   { cond, alors[], sinon[] }
    Switch   { valeur, cas[] (valeur, bloc), defaut[] }
    Loop     { kind (ForEachChild | ForEachItem), source, variable, corps[] }
    Call     { callback, args[] }                  // Callback : le code de l'application
    Ui       { op (Open | Close | Back | Show | Hide | Toggle | Enable | Disable
                   | Focus | Toast | Emit | SetTheme), args[] }
    Await    { kind (Message | Service | After), args[], reponse (nom, bloc),
               echec (nom, bloc) }                 // message, call, after
    Func     { nom, params[], corps[] }            // fonctions de graphe
  expressions : Literal | Var | WidgetField | Token | BinOp | Not | FuncCall
```

- **Script → IR** : l'évaluateur actuel (`NkGuiEvaluateur`) lit déjà `set` / `if` /
  `Callback` ; il s'étend aux instructions 0.4.
- **Graphe → IR** : parcours des fils d'exécution depuis l'événement ; les nœuds purs
  deviennent des expressions (évaluées **une fois** par exécution, doc 2 §6.1) ; un
  nœud d'exécution = une instruction. Refus nommés : `E-GRAPHE` (entrée obligatoire non
  reliée, cycle de purs, type), avec le nœud.
- **IR → script** (vue Code) et **IR → graphe** (conversion) : doc 2 §6.6.
- **Le même IR est exécuté** dans NKUIDesign (simulation) et dans l'application
  (runtime NKGui) : **aucun** interpréteur de graphe séparé.

### 3.2 L'exécution

- Les garanties du doc 2 §5.7 (jamais à moitié ; ordre ; pas de récursion dans une
  image ; réponses asynchrones dans leur bloc ; modale).
- **Débogage** : chaque instruction porte la référence de son nœud (graphe) ou de sa
  ligne (script) ; l'exécuteur publie « atteint » ; points d'arrêt et pas à pas
  suspendent **entre deux instructions**, jamais au milieu d'une.
- **Couverture** : l'exécuteur compte, par comportement, chaque branche (`vrai` /
  `faux`, chaque cas d'un `Switch`, chaque réponse d'un `message`, succès / échec d'un
  `call`).

---

## 4. Le thème de NKUIDesign — `NKUIDesign/theme.nkgui`

NKUIDesign **porte** son thème comme toute application de la famille (décision D10) :
il **complète** le thème commun (les sections `theme` incluses se fusionnent, doc 2 §20.3) et ajoute ses jetons d'information (valeurs : doc 3 §2.3).

```nkgui
nkgui 0.4
include "../Commun/theme.nkgui"

theme {
  nom = "Rihen UE5"   defaut = "Sombre"      // fusionné avec la section du thème commun inclus (doc 2 §20.3)

  token @guide.alignement     { type = Color  famille = information  sens = "Guides de magnétisme de la toile, pendant un glisser" }
  token @fidelite.peint       { type = Color  famille = information  sens = "Le monteur peint cet élément tel qu'écrit" }
  token @fidelite.compte      { type = Color  famille = information  sens = "Écrit et gardé, pas encore peint par le monteur" }
  token @fidelite.refuse      { type = Color  famille = information  sens = "Refusé par le monteur (rôle inconnu, menu hors menu…)" }
  token @propose              { type = Color  famille = information  sens = "Rôle proposé, affiché par sa doublure" }
  token @controle.erreur      { type = Color  famille = information  sens = "Contrôle de conception : erreur" }
  token @controle.alerte      { type = Color  famille = information  sens = "Contrôle de conception : alerte" }
  token @controle.info        { type = Color  famille = information  sens = "Contrôle de conception : information" }
  token @provenance.theme     { type = Color  famille = provenance   sens = "Valeur venue du thème (T)" }
  token @provenance.composant { type = Color  famille = provenance   sens = "Valeur venue de la déclaration du composant (C)" }
  token @provenance.surcharge { type = Color  famille = provenance   sens = "Surcharge de cette instance (S)" }
  token @provenance.local     { type = Color  famille = provenance   sens = "Valeur locale (L)" }
  token @action.servie        { type = Color  famille = information  sens = "L'application sert cette action" }
  token @action.a_creer       { type = Color  famille = information  sens = "Action déclarée, pas encore servie" }
  token @action.inconnue      { type = Color  famille = information  sens = "Action absente du registre : faute de frappe probable" }
  token @test.reussi          { type = Color  famille = information  sens = "Test réussi" }
  token @test.echoue          { type = Color  famille = information  sens = "Test échoué" }
  token @test.jamais          { type = Color  famille = information  sens = "Test jamais lancé" }
  token @simulation.enregistre { type = Color famille = information  sens = "Un test est en cours d'enregistrement" }
  token @doublure             { type = Color  famille = information  sens = "Ligne de journal servie par une doublure" }
  token @hote                 { type = Color  famille = information  sens = "Zone Host non remplie : hachures et nom" }
  token @chemin.execute       { type = Color  famille = information  sens = "Chemin parcouru dans un Blueprint pendant la simulation" }
  token @toile.fond           { type = Color  famille = structure    sens = "Fond de table de la toile Design, neutre et clair dans les deux variantes" }
  token @graphe.fond          { type = Color  famille = structure    sens = "Fond de la toile Behavior" }

  variant "Sombre" {
    @guide.alignement = #FF4FD8   @fidelite.peint = #3FB950   @fidelite.compte = #D29922   @fidelite.refuse = #F85149
    @propose = #9B6BF2   @controle.erreur = #F85149   @controle.alerte = #D29922   @controle.info = #1177D1
    @provenance.theme = #6E7681   @provenance.composant = #9B6BF2   @provenance.surcharge = #F2980E   @provenance.local = #E6E6E6
    @action.servie = #3FB950   @action.a_creer = #D29922   @action.inconnue = #F85149
    @test.reussi = #3FB950   @test.echoue = #F85149   @test.jamais = #6E7681
    @simulation.enregistre = #F85149   @doublure = #E3B341   @toile.fond = #EDEDED   @graphe.fond = #1A1A1A
    @hote = #6E7681   @chemin.execute = #FFFFFF
  }
  variant "Clair" {
    @guide.alignement = #D10FB0   @fidelite.peint = #1A7F37   @fidelite.compte = #9A6700   @fidelite.refuse = #D1242F
    @propose = #8250DF   @controle.erreur = #D1242F   @controle.alerte = #9A6700   @controle.info = #0E5FA6
    @provenance.theme = #6E7781   @provenance.composant = #8250DF   @provenance.surcharge = #C97A08   @provenance.local = #1A1A1A
    @action.servie = #1A7F37   @action.a_creer = #9A6700   @action.inconnue = #D1242F
    @test.reussi = #1A7F37   @test.echoue = #D1242F   @test.jamais = #6E7781
    @simulation.enregistre = #D1242F   @doublure = #9A6700   @toile.fond = #EDEDED   @graphe.fond = #1E1E1E
    @hote = #6E7781   @chemin.execute = #FFFFFF        // sur @graphe.fond, sombre dans les deux variantes
  }
}
```

⚠️ **Pourquoi `@provenance.*` et non `@origine.*`** : `@origine.*` existe déjà dans
NkAnima (origine des **clés** : main, physique, IA…). **Ne réutilise jamais un nom de
jeton de la famille avec un autre sens.**

⚠️ **Tant que la section `theme` n'est pas implémentée** (🟢, doc 2 §20) : écris ce
fichier quand même (il voyage, il sera compté), et dans les autres documents écris la
valeur **Sombre** avec le jeton en commentaire sur la même ligne —
`fill { color = #F85149 }  // @controle.erreur` — pour un remplacement mécanique.

---

## 5. Le vocabulaire proposé et consommé

- **P23 — `TokenField`** (proposé par NKUIDesign) : un champ de couleur qui choisit **un
  jeton** ; `bind`, `families` (liste de familles autorisées), `allowFree` ; forme :
  pastille + nom du jeton + flèche, la liste montrant pastille, nom **et sens**.
  **En attendant** : `Dropdown` des noms de jetons + `ColorField` (non monté) en repli.
- **Les extensions du format 0.4** (booléens et formes, instructions, conditions,
  carte, tests, thèmes, Blueprints exécutés) sont **décidées** : NKUIDesign les
  **édite** ; ses **propres** documents d'interface n'en emploient que ce qui leur sert
  (thème, instructions de navigation entre ses fenêtres).
- **Consommés, et donc prioritaires pour le monteur** : **P2** `ToggleButton` (outils,
  bascules), **P3** `Badge` (compteurs, provenance), **P9** `Slot` (lignes de
  l'inspecteur), **P10** `layout` (dispositions), **P18** `SplitButton` (familles
  d'outils : le bouton montre l'outil courant, la flèche la famille), **P23**
  `TokenField`, **P28** `Drawer` (pastilles en superposition). 📌 Quand NKUIDesign passe
  en documents, ces rôles cessent d'être « pour plus tard ».

---

## 6. Les composants

### 6.1 Un ajout aux lignes **communes** (à proposer à Rodolf)

Les composants de ligne communs (`LigneNombre`, `LigneVec3`, `LigneListe`,
`LigneCase`, `LigneCouleur`, `LigneCourbe`, `LigneAsset`) gagnent une **première
colonne facultative** :

```
    Badge✚ "provenance"     text: "T"   tone: provenance.theme   visible: false
```

- **Invisible par défaut** : aucune application existante ne change d'aspect.
- **NKUIDesign la rend visible** sur toutes les lignes de l'inspecteur.
- **Pourquoi dans les communs** : sinon NKUIDesign recopie les sept lignes pour une
  colonne — exactement la copie que les composants servent à éviter.
⚠️ Cela modifie `Commun/composants.nkgui`, donc **les six applications** : fais-le
**après** que le chantier « Commun/ » est stable, et **vérifie par les compteurs**
que les autres applications n'ont pas bougé.

### 6.2 Propres — `NKUIDesign/composants_nkuidesign.nkgui`

#### `EnTeteIdentite` — le haut de l'inspecteur

```
component "EnTeteIdentite"
  VBox "entete"                          gap: 4
    HBox "ligne1"                        gap: 6   align: Center
      Dropdown "role"                    bind: selection.role   (items : rôles natifs, puis proposés)
      TextField "identifiant"            bind: selection.id
      Badge✚ "etat_action"               text: "?"   tone: action.inconnue   visible: false
    HBox "ligne2"                        gap: 4   visible: false
      Text "composant_titre"             text: "Composant :"
      Text "composant_nom"               text: "—"
      Text "composant_biblio"            text: "—"
      Button "voir"                      id: "objet.voir_composant"   icon: external-link   tooltip: "Ouvrir la déclaration"   // D12 (b)
```
- `etat_action.visible = selection.interactive` ; `text` et `tone` selon le
  registre (✅ `action.servie` · ✚ `action.a_creer` · ? `action.inconnue`) —
  idempotent ✅.
- `ligne2.visible = selection.est_instance` ✅.
- `identifiant` : quand l'élément est interactif, l'application **propose la
  complétion depuis le registre** (P23 n'est pas nécessaire ici : c'est un
  `TextField` + une liste de complétion de l'application).

#### `BoutonRail` — un bouton de rail avec compteur

```
component "BoutonRail"
  Button "bouton"                        icon: "?"   label: "?"
```
Instances : Console, **Contrôles**, **Fidélité**, **Tests**, Aperçu (barre d'état) ;
rails latéraux (doc 3 §13).

- **L'action est portée par la racine** (décision D12, recommandation (a) : la racine
  prend l'identifiant de l'instance, donc le nom de l'action). C'est pourquoi le
  compteur **n'est pas** un enfant `Badge` : un enfant cliquable aurait un identifiant
  préfixé (`projet.controles.compteur`) qui ne serait plus l'action.
- Le **compteur** est dans le libellé, tenu par un comportement continu :
  ```nkgui
  behavior "projet.controles" {
    if controles.nb > 0 { set "projet.controles".label = "Contrôles ⚠" + controles.nb }
    else                { set "projet.controles".label = "Contrôles" }
  }
  ```
  idempotent ✅ (0.4 : `set "id".prop`, concaténation).
- Le jour où `Badge` (P3) et les emplacements (P9) existent, le compteur deviendra une
  pastille **non interactive** dans un emplacement — non cliquable, donc sans action, et
  la règle (a) tient toujours.

#### `GroupeOutils` — une famille d'outils (doc 3 §7.1)

```
component "GroupeOutils"
  SplitButton✚ "groupe"                  icon: "?"   label: "?"
```
Le bouton montre **l'outil courant de la famille** ; la flèche liste la famille
(`MenuItem`, un par outil). **En attendant P18** : `ToggleButton` de l'outil
courant + un `Button` « ▾ » (identifiant `outil.famille_<nom>`) qui ouvre la famille.

#### `LigneJeton` — une ligne de la fenêtre Thème

```
component "LigneJeton"
  Grid "ligne"                           columns: 6   sizes: [0.20, 0.13, 0.13, 0.36, 0.08, 0.10]   gap: 6
    Text "nom"                           text: "@?"          font: "Mono-11"
    ColorField "sombre"                  bind: ?.sombre      (non monté : attendu)
    ColorField "clair"                   bind: ?.clair
    TextField "sens"                     bind: ?.sens        placeholder: "Ce que ce jeton signifie (obligatoire)"
    Text "contraste"                     text: "—"
    Button "usages"                      id: "projet.usages_jeton"   label: "—"   tooltip: "Où ce jeton sert"   // D12 (b)
```

#### `CarteControle` — un manquement

```
component "CarteControle"
  HBox "carte"                           gap: 6   align: Center
    Badge✚ "severite"                    text: "!"   tone: controle.alerte
    VBox "texte"                         gap: 0
      Text "regle"                       text: "?"
      Text "ou"                          text: "?"     (« NkAnimaEditor › barre_outils › anim.inserer »)
    Spacer "pousse"
    Button "aller"                       id: "projet.aller_a"     label: "Aller à"                    // D12 (b)
    Button "corriger"                    id: "projet.corriger"    label: "Corriger"   visible: false  // D12 (b)
```
`corriger.visible = controle.correction_mecanique` ✅.

---

## 7. Les fichiers — `Resources/Interface/NKUIDesign/`

| fichier | § | remplace (code) |
|---|---|---|
| `composants_nkuidesign.nkgui` | 6.2 | — |
| `menus.nkgui` | 8.1 | `main.cpp` l. 8418–8800 |
| `onglets.nkgui` | 8.2 | barre d'onglets de projets |
| `bascule_mode.nkgui` | 8.3 | bascule flottante |
| `outils_design.nkgui`, `outils_comportement.nkgui`, `outils_animation.nkgui` | 8.4 | barres d'outils flottantes (doc 3 §7) |
| `grappe_toile.nkgui` | 8.5 | cluster zoom / grille / aimant |
| `colonne_gauche.nkgui` | 8.6 | Hiérarchie (Pages / Composants) |
| `inspecteur.nkgui` + `insp_design`, `insp_widget`, `insp_comportement`, `insp_toile` | 8.7 | inspecteur (`Panels.h`) |
| `barre_etat.nkgui` | 8.8 | barre d'état |
| `fen_theme.nkgui`, `fen_actions.nkgui`, `pan_controles.nkgui`, `pan_fidelite.nkgui`, `fen_inclusions.nkgui`, `dlg_coller_arbre.nkgui` | 8.9 | — (nouveaux) |
| `accueil.nkgui` | 8.10 | écran d'accueil |
| `barre_booleens.nkgui` | 8.12 | — (nouveau) |
| `barre_simulation.nkgui`, `dlg_simuler_depuis.nkgui`, `pan_doublures.nkgui`, `pan_variables.nkgui`, `fen_carte.nkgui` | 8.13 | barre de la fenêtre de simulation (doc 3 §18) |
| `pan_tests.nkgui`, `carte_test.nkgui` | 8.14 | — (nouveaux) |
| `dispositions.nkgui` | 8.11 | dispositions (Fenêtre ▸ Disposition) |
| `theme.nkgui` | 4 | couleurs en dur de `Costume.h` / `NkTheme` |
| `outils_comportement.nkgui`, `pan_comportements.nkgui`, `pop_recherche_noeuds.nkgui` | 8.15 | barre du mode Behavior ; — (nouveaux) |
| `menus_contextuels.nkgui` | 8.16 | `MenuContexte.h` (quand `ContextMenu` sera monté) |
| `dlg_preferences.nkgui` | 8.17 | boîte de préférences |

📌 **Chaque document inclut** `theme.nkgui` (§4), `../Commun/composants.nkgui` puis
`composants_nkuidesign.nkgui`, **dans cet ordre** (le contenu inclus prend la place
exacte du `include` ; écrit après `widgets`, il arriverait trop tard).

---

## 8. Les arbres

### 8.1 `menus.nkgui`

**Toutes les entrées actuelles sont reprises, dans leur ordre et avec leurs
raccourcis** (relevé du 27/09) ; les **ajouts** de la refonte sont marqués `// +`.

```
MenuBar "menus"
  Menu "menu_fichier"                             label: "Fichier"
    Menu "menu_nouveau"                           label: "Nouveau"                                               // + (était « Nouveau projet… »)
      MenuItem "fichier.nouveau_projet"           label: "Projet…"
      MenuItem "fichier.nouveau_document"         label: "Document…"                    shortcut: "Ctrl+N"       // +
      MenuItem "fichier.nouveau_depuis_gabarit"   label: "Depuis un gabarit…"                                    // +
      MenuItem "fichier.nouveau_depuis_spec"      label: "Depuis une spécification…"                             // +
      MenuItem "fichier.nouveau_via_ia"           label: "Via IA…"                                               // +
    MenuItem "fichier.ouvrir_projet"              label: "Ouvrir un projet…"            shortcut: "Ctrl+Alt+O"   // +
    MenuItem "fichier.ouvrir"                     label: "Ouvrir un document…"          shortcut: "Ctrl+O"
    Menu "menu_recents"                           label: "Ouvrir récent"                (rempli par l'application : projets, puis documents)
    MenuItem "fichier.convertir_nkuidoc"          label: "Convertir un .nkuidoc…"                                // +
    MenuItem "fichier.fermer_document"            label: "Fermer le document"           shortcut: "Ctrl+W"       // libellé : un onglet = un document
    Separator "sep_f1"
    MenuItem "fichier.enregistrer"                label: "Enregistrer"                  shortcut: "Ctrl+S"
    MenuItem "fichier.enregistrer_sous"           label: "Enregistrer sous…"            shortcut: "Ctrl+Maj+S"
    MenuItem "fichier.enregistrer_tout"           label: "Enregistrer tout"             shortcut: "Ctrl+Alt+S"
    MenuItem "fichier.revenir"                    label: "Revenir à la version enregistrée"
    Separator "sep_f2"
    Menu "menu_importer"                          label: "Importer"
      MenuItem "fichier.importer_composant"       label: "Composant…"
      MenuItem "fichier.importer_document"        label: "Document…"
      MenuItem "fichier.importer_svg"             label: "SVG…"                                                  // +
      MenuItem "fichier.importer_ressources"      label: "Ressources…"
    Menu "menu_exporter"                          label: "Exporter"                                              // + (était une entrée)
      MenuItem "fichier.exporter"                 label: "Image (PNG, SVG)…"            shortcut: "Ctrl+E"
      MenuItem "fichier.exporter_code"            label: "Code…"                                                 // + (grisé avec sa raison tant qu'absent)
      MenuItem "fichier.decouper"                 label: "Découper en fichiers…"                                 // +
      MenuItem "fichier.exporter_nkgui"           label: "Document .nkgui (copie)"      // gardé : l'enregistrement EST l'export ; ceci écrit une copie ailleurs
    MenuItem "fichier.valider"                    label: "Valider le document"          shortcut: "F7"          // raccourci changé (doc 3 §29)
    MenuItem "fichier.voir_dans_application"      label: "Voir dans l'application"      shortcut: "Ctrl+Maj+Entrée"   // +
    Separator "sep_f3"
    Menu "menu_backend"                           label: "Backend graphique"            (rempli par l'application)
    MenuItem "fichier.preferences"                label: "Préférences…"                 shortcut: "Ctrl+,"
    MenuItem "fichier.quitter"                    label: "Quitter"                      shortcut: "Ctrl+Q"

  Menu "menu_edition"                             label: "Édition"
    MenuItem "edition.annuler"                    label: "Annuler"                      shortcut: "Ctrl+Z"
    MenuItem "edition.retablir"                   label: "Rétablir"                     shortcut: "Ctrl+Y"
    Separator "sep_e1"
    MenuItem "edition.couper"                     label: "Couper"                       shortcut: "Ctrl+X"
    MenuItem "edition.copier"                     label: "Copier"                       shortcut: "Ctrl+C"
    MenuItem "edition.coller"                     label: "Coller"                       shortcut: "Ctrl+V"
    MenuItem "edition.coller_meme_place"          label: "Coller à la même place"       shortcut: "Ctrl+Maj+V"
    MenuItem "edition.coller_style"               label: "Coller le style seul"         shortcut: "Ctrl+Alt+V"
    MenuItem "edition.coller_arbre"               label: "Coller comme arbre"           shortcut: "Ctrl+Maj+T"   // +
    MenuItem "edition.dupliquer"                  label: "Dupliquer"                    shortcut: "Ctrl+D"
    MenuItem "edition.supprimer"                  label: "Supprimer"                    shortcut: "Suppr"
    Separator "sep_e2"
    MenuItem "edition.tout_selectionner"          label: "Tout sélectionner"            shortcut: "Ctrl+A"
    MenuItem "edition.selectionner_meme_role"     label: "Sélectionner tous les éléments du même rôle"
    MenuItem "edition.deselectionner"             label: "Désélectionner"               shortcut: "Échap"
    Separator "sep_e3"
    MenuItem "edition.rechercher"                 label: "Rechercher…"                  shortcut: "Ctrl+F"
    MenuItem "edition.rechercher_projet"          label: "Rechercher dans le projet…"   shortcut: "Ctrl+Maj+F"   // +
    MenuItem "edition.remplacer_propriete"        label: "Remplacer une propriété…"     shortcut: "Ctrl+H"
    MenuItem "edition.renommer"                   label: "Renommer"                     shortcut: "F2"

  Menu "menu_affichage"                           label: "Affichage"
    MenuItem "affichage.zoom_avant"               label: "Zoom avant"                   shortcut: "Ctrl++"
    MenuItem "affichage.zoom_arriere"             label: "Zoom arrière"                 shortcut: "Ctrl+-"
    MenuItem "affichage.zoom_100"                 label: "Zoom 100 %"                   shortcut: "Ctrl+0"
    MenuItem "affichage.ajuster_selection"        label: "Ajuster à la sélection"       shortcut: "Maj+2"
    MenuItem "affichage.ajuster_page"             label: "Ajuster à la page"            shortcut: "Maj+1"
    Separator "sep_a1"
    MenuItem "affichage.grille"                   label: "Grille"                       shortcut: "Ctrl+'"
    MenuItem "affichage.magnetisme"               label: "Magnétisme"                   shortcut: "Ctrl+;"
    MenuItem "affichage.regles"                   label: "Règles"
    MenuItem "affichage.reperes"                  label: "Repères intelligents"
    MenuItem "affichage.marges"                   label: "Marges et remplissage"
    MenuItem "affichage.regions_fenetre"          label: "Régions de fenêtre"
    MenuItem "affichage.desactives_heritage"      label: "Éléments désactivés par héritage"
    Separator "sep_a2"
    MenuItem "affichage.fidelite"                 label: "Fidélité (rendu du monteur)"  shortcut: "Maj+F"     // +
    Menu "menu_etat"                              label: "État"                                               // + — les SIX états du format (doc 2 §13.2) ; « actif » n'en est pas un
      MenuItem "affichage.etat_repos"             label: "Repos"                        checked: true         // +
      MenuItem "affichage.etat_survol"            label: "Survol"                                             // +
      MenuItem "affichage.etat_appui"             label: "Appui"                                              // +
      MenuItem "affichage.etat_focus"             label: "Focus"                                              // +
      MenuItem "affichage.etat_focus_visible"     label: "Focus visible"                                      // +
      MenuItem "affichage.etat_desactive"         label: "Désactivé"                                          // +
    Menu "menu_theme_document"                    label: "Thème du document"            (rempli par l'application : thèmes et variantes de l'application du document)   // +
      MenuItem "affichage.doc_les_deux"           label: "Deux variantes côte à côte"                         // +
    MenuItem "affichage.tailles_multiples"        label: "Tailles multiples"                                  // +
    Separator "sep_a3"
    Menu "menu_mode"                              label: "Mode"
      MenuItem "mode.design"                      label: "Design"                       shortcut: "Ctrl+1"
      MenuItem "mode.comportement"                label: "Behavior"                     shortcut: "Ctrl+2"   // libellé gardé (planches validées, doc 3 §7)
      MenuItem "mode.animation"                   label: "Animation"                    shortcut: "Ctrl+3"
      MenuItem "mode.split"                       label: "Split"                        shortcut: "Ctrl+4"
      MenuItem "mode.source"                      label: "Source"                       shortcut: "Ctrl+5"   // +
    Menu "menu_theme_outil"                       label: "Thème de l'outil"             // renommé (était « Thème ») : on ne le confond plus avec celui du document
      MenuItem "affichage.theme_rihen_ue5"        label: "Rihen UE5"                    checked: true         // + (D2)
      MenuItem "affichage.theme_github_pro"       label: "GitHub Pro"                                         // +
      MenuItem "affichage.theme_contraste"        label: "Contraste élevé"                                    // +
      Menu "menu_themes_installes"                label: "Installés"                    (rempli par l'application : thèmes de greffons)   // +
      Separator "sep_t1"
      MenuItem "affichage.theme_systeme"          label: "Variante : Système"
      MenuItem "affichage.theme_sombre"           label: "Variante : Sombre"
      MenuItem "affichage.theme_clair"            label: "Variante : Clair"
    Menu "menu_panneaux"                          label: "Panneaux"                     (rempli par l'application)
    MenuItem "affichage.plein_ecran"              label: "Plein écran"                  shortcut: "F11"

  Menu "menu_objet"                               label: "Objet"
    MenuItem "objet.attribuer_role"               label: "Attribuer un rôle…"
    MenuItem "objet.retirer_role"                 label: "Retirer le rôle"
    MenuItem "objet.choisir_action"               label: "Choisir l'action…"            shortcut: "Ctrl+Maj+A"   // +
    MenuItem "objet.choisir_jeton"                label: "Choisir un jeton pour la couleur…"   shortcut: "Ctrl+T"   // +
    Separator "sep_o1"
    MenuItem "objet.grouper"                      label: "Grouper"                      shortcut: "Ctrl+G"
    MenuItem "objet.degrouper"                    label: "Dégrouper"                    shortcut: "Ctrl+Maj+G"
    MenuItem "objet.extraire_composant"           label: "Convertir en composant"       shortcut: "Ctrl+K"
    MenuItem "objet.detacher_composant"           label: "Détacher l'instance"          shortcut: "Ctrl+Alt+D"
    MenuItem "objet.appliquer_au_composant"       label: "Appliquer au composant"       shortcut: "Ctrl+Alt+M"
    MenuItem "objet.promouvoir_partage"           label: "Promouvoir en composant partagé"
    Separator "sep_o2"
    MenuItem "objet.poser_hote"                   label: "Poser une zone hôte"                                 // +
    Menu "menu_booleens"                          label: "Booléens"                                            // +
      MenuItem "objet.booleen_union"              label: "Union"                        shortcut: "Ctrl+Maj+U"   // +
      MenuItem "objet.booleen_soustraire"         label: "Soustraire"                   shortcut: "Ctrl+Maj+P"   // +
      MenuItem "objet.booleen_intersecter"        label: "Intersecter"                  shortcut: "Ctrl+Maj+I"   // +
      MenuItem "objet.booleen_exclure"            label: "Exclure"                      shortcut: "Ctrl+Maj+X"   // +
      Separator "sep_b1"
      MenuItem "objet.aplatir"                    label: "Aplatir…"                                            // +
    MenuItem "objet.vectoriser_contour"           label: "Vectoriser le contour"        shortcut: "Ctrl+Maj+O"   // +
    MenuItem "objet.masque"                       label: "Masque"                       shortcut: "Ctrl+M"       // + (doc 3 §29 : Ctrl+M libéré)
    MenuItem "objet.forme_du_widget"              label: "Utiliser comme forme du widget"                      // +
    MenuItem "objet.enregistrer_icone"            label: "Enregistrer comme icône…"                            // +
    Menu "menu_proposes"                          label: "Poser un rôle proposé"         (rempli par l'application : P2–P23, P28)   // +
    Separator "sep_o3"
    Menu "menu_aligner"                           label: "Aligner"                      (✚ — « à brancher » aujourd'hui)
    Menu "menu_repartir"                          label: "Répartir"                     (✚)
    Menu "menu_ordre"                             label: "Ordre"
      MenuItem "objet.premier_plan"               label: "Premier plan"                 shortcut: "Ctrl+Maj+]"
      MenuItem "objet.avancer"                    label: "Avancer"                      shortcut: "Ctrl+]"
      MenuItem "objet.reculer"                    label: "Reculer"                      shortcut: "Ctrl+["
      MenuItem "objet.arriere_plan"               label: "Arrière-plan"                 shortcut: "Ctrl+Maj+["
    Menu "menu_miroir"                            label: "Miroir"                       // + (actions existantes, NkActionCtx)
      MenuItem "objet.miroir_h"                   label: "Horizontal"                   shortcut: "Maj+H"
      MenuItem "objet.miroir_v"                   label: "Vertical"                     shortcut: "Maj+V"
    MenuItem "objet.verrouiller"                  label: "Verrouiller"                  shortcut: "Ctrl+L"
    MenuItem "objet.masquer_editeur"              label: "Masquer dans l'éditeur"       shortcut: "Ctrl+Maj+H"
    Menu "menu_disponibilite"                     label: "Disponibilité"
      MenuItem "objet.dispo_actif"                label: "Actif"
      MenuItem "objet.dispo_desactive"            label: "Désactivé"
      MenuItem "objet.dispo_lecture_seule"        label: "Lecture seule"
      MenuItem "objet.dispo_occupe"               label: "Occupé"

  Menu "menu_cible"                               label: "Cible"
    Menu "menu_classe"                            label: "Classe"
      MenuItem "cible.bureau"                     label: "Bureau"
      MenuItem "cible.mobile"                     label: "Mobile"
      MenuItem "cible.web"                        label: "Web"
      MenuItem "cible.appareil"                   label: "Appareil…"
    Menu "menu_orientation"                       label: "Orientation"
      MenuItem "cible.portrait"                   label: "Portrait"
      MenuItem "cible.paysage"                    label: "Paysage"
    MenuItem "cible.zone_sure"                    label: "Afficher la zone sûre"
    Menu "menu_decoration"                        label: "Décoration"
      MenuItem "cible.decoration_native"          label: "Native"
      MenuItem "cible.decoration_client"          label: "Client"
    MenuItem "cible.curseur"                      label: "Curseur…"
    Menu "menu_langue"                            label: "Langue du document"           (rempli par l'application)
    MenuItem "cible.points_rupture"               label: "Points de rupture…"
    MenuItem "cible.apercu_multi"                 label: "Aperçu multi-cibles"
    MenuItem "cible.rapport_transposition"        label: "Rapport de transposition…"

  Menu "menu_comportement"                        label: "Comportement"
    MenuItem "comportement.ouvrir_graphe"         label: "Ouvrir le graphe"
    MenuItem "comportement.vue_code"              label: "Vue Code"                     shortcut: "Ctrl+²"
    MenuItem "comportement.ajouter_evenement"     label: "Ajouter un événement…"
    MenuItem "comportement.lier_callback"         label: "Lier à un callback…"
    MenuItem "comportement.delier"                label: "Délier"
    MenuItem "comportement.gestionnaire"          label: "Gestionnaire de callbacks…"
    MenuItem "comportement.nouveau_blueprint"     label: "Nouveau Blueprint"                                     // +
    MenuItem "comportement.nouvelle_fonction"     label: "Nouvelle fonction"                                     // +
    MenuItem "comportement.nouvelle_macro"        label: "Nouvelle macro"                                        // +
    MenuItem "comportement.point_arret"           label: "Point d'arrêt sur le nœud"    shortcut: "F9"           // +
    Separator "sep_c1"
    MenuItem "comportement.simuler"               label: "Simuler"                      shortcut: "F5"
    MenuItem "comportement.simuler_depuis"        label: "Simuler à partir de…"         shortcut: "Ctrl+F5"      // +
    MenuItem "comportement.geler"                 label: "Geler la simulation"          shortcut: "F6"
    MenuItem "comportement.recharger"             label: "Recharger la simulation"      shortcut: "Maj+F5"
    MenuItem "comportement.pas_a_pas"             label: "Pas à pas"                    shortcut: "F10"          // +
    MenuItem "comportement.systeme_simule"        label: "Système simulé…"
    MenuItem "comportement.couverture"            label: "Rapport de couverture…"
    Separator "sep_c2"                                                                                         // +
    MenuItem "comportement.carte"                 label: "Carte de l'application…"                             // +
    MenuItem "comportement.test_enregistrer"      label: "Enregistrer un test"          shortcut: "Ctrl+Maj+R"   // + (en simulation)

  Menu "menu_ia"                                  label: "IA"
    Menu "menu_generer"                           label: "Générer"
      MenuItem "ia.generer_composant"             label: "un composant…"
      MenuItem "ia.generer_comportement"          label: "un comportement…"
      MenuItem "ia.generer_animation"             label: "une animation…"
    MenuItem "ia.proposer_role"                   label: "Proposer un rôle pour la sélection"
    MenuItem "ia.chercher_bibliotheque"           label: "Chercher dans la bibliothèque avant de générer"
    MenuItem "ia.assistant"                       label: "Ouvrir le chat IA"             shortcut: "Ctrl+Maj+K"   // raccourci ajouté (doc 3 §29)
    MenuItem "ia.reglages_modele"                 label: "Réglages du modèle…"

  Menu "menu_projet"                              label: "Projet"                                              // + (menu entier)
    MenuItem "projet.explorateur"                 label: "Explorateur"
    MenuItem "projet.inclusions"                  label: "Graphe des inclusions…"
    Separator "sep_p1"
    MenuItem "projet.theme"                       label: "Thème et jetons…"
    MenuItem "projet.actions"                     label: "Actions…"
    Separator "sep_p2"
    MenuItem "projet.controles"                   label: "Contrôles"
    MenuItem "projet.regles"                      label: "Règles de contrôle…"
    MenuItem "projet.actualiser_actions"          label: "Actualiser les actions (sonde)"
    Separator "sep_p3"
    Menu "menu_tests"                             label: "Tests"                                               // +
      MenuItem "projet.tests_tous"                label: "Lancer tous"                                         // +
      MenuItem "projet.tests_echoues"             label: "Lancer les échoués"                                  // +
      MenuItem "projet.tests_verifier"            label: "Vérifier les tests"                                  // +
      MenuItem "projet.tests_couverture"          label: "Rapport de couverture…"                              // +
      MenuItem "projet.tests_panneau"             label: "Panneau des tests"                                   // +
    Separator "sep_p4"
    MenuItem "projet.moderniser"                  label: "Moderniser les documents…"                           // + (sur demande seulement)

  Menu "menu_fenetre"                             label: "Fenêtre"
    MenuItem "fenetre.nouvelle"                   label: "Nouvelle fenêtre"
    MenuItem "fenetre.detacher_onglet"            label: "Détacher l'onglet dans une fenêtre"
    Menu "menu_disposition"                       label: "Disposition"
      MenuItem "fenetre.dispo_defaut"             label: "Par défaut"
      MenuItem "fenetre.dispo_design"             label: "Design"
      MenuItem "fenetre.dispo_comportement"       label: "Comportement"
      MenuItem "fenetre.dispo_source"             label: "Source"                                              // +
      MenuItem "fenetre.dispo_enregistrer"        label: "Enregistrer la disposition…"
      MenuItem "fenetre.reinitialiser"            label: "Réinitialiser"
    Separator "sep_w1"
    MenuItem "fenetre.onglet_suivant"             label: "Onglet suivant"               shortcut: "Ctrl+Tab"
    MenuItem "fenetre.onglet_precedent"           label: "Onglet précédent"             shortcut: "Ctrl+Maj+Tab"
    Separator "sep_w2"
    // ici, les fenêtres ouvertes, insérées par l'application (la courante cochée)

  Menu "menu_aide"                                label: "Aide"
    MenuItem "aide.documentation"                 label: "Documentation"                shortcut: "F1"
    MenuItem "aide.raccourcis"                    label: "Raccourcis clavier…"
    MenuItem "aide.glossaire"                     label: "Glossaire des composants"
    MenuItem "aide.greffons"                      label: "Gestionnaire de greffons…"
    MenuItem "aide.console"                       label: "Console…"
    MenuItem "aide.infos_systeme"                 label: "Informations système — copier"
    Separator "sep_h1"
    MenuItem "aide.mises_a_jour"                  label: "Rechercher les mises à jour"   // +
    MenuItem "aide.a_propos"                      label: "À propos"                      // +
```

⚠️ **Changements de l'existant**, à ne pas faire en silence — **chacun est une
décision du doc 3 (§5bis, §29)** :
1. « Valider le document » passe de **Ctrl+Maj+V** (en conflit avec « Coller à la
   même place ») à **F7** ;
2. « Thème » devient **« Thème de l'outil »**, avec les thèmes livrés ;
3. « Nouveau projet… » devient le sous-menu **Nouveau** ; `Ctrl+N` crée un **document** ;
4. « Fermer le projet » devient **« Fermer le document »** (un onglet = un document) ;
5. **`Ctrl+M`** est retiré de la commande de coquille « Vue : Préférences » (doublon de
   `Ctrl+,`) et donné au masque ; **`Ctrl+P`** et **`Ctrl+K`** sont retirés des
   commandes de coquille « Vue : Propriétés » / « Vue : Composition ».
Le mode garde le libellé **« Behavior »** (planches validées).

### 8.2 `onglets.nkgui`

```
HBox "onglets"                                    gap: 0   align: Center
  Host "onglets.liste"                            hint: "onglets de documents : « Application › document », point si modifié, croix ; glisser pour réordonner, détacher"
  Button "fichier.nouveau_document"               icon: plus   tooltip: "Nouveau document"
```
📌 Le **logo carré 56 px** qui tient les deux bandes (doc 3 §4) n'est **pas** dans ce
document : il appartient à la fenêtre (`Window` de la disposition), à gauche des
deux bandes. S'il ne peut pas s'y poser, dis-le — c'est une décision d'identité
visuelle, pas un détail.

### 8.3 `bascule_mode.nkgui`

```
HBox "bascule"                                    gap: 0   align: Center
  RadioGroup "mode.choix"                         bind: mode.actif   options: ["Design","Behavior","Animation","Split","Source"]   orientation: Horizontal
  Separator "sep"                                 orientation: Vertical
  ToggleButton✚ "affichage.fidelite"              icon: eye   label: "Fidélité"   bind: affichage.fidelite   tooltip: "Rendu du monteur (Maj+F)"
```
- Flottant, **centré en haut de la toile** (placement absolu dans la disposition).
- `mode.choix` : l'option active en `@accent` (c'est l'**état de l'interface**, bleu).
- `affichage.fidelite` actif : fond `@fidelite.compte` (ambre) — on regarde **une
  autre vérité** que le design, il faut le voir.

### 8.4 Les barres d'outils flottantes

**Lis le doc 3 §7.2 à §7.4** pour la **liste exacte** des outils de chaque mode et
de leurs familles ; **ne l'invente pas**. La forme est :

```
VBox "outils_design"                              gap: 2
  GroupeOutils "outil.selection"                  icon: mouse-pointer-2   label: "Sélection"   tooltip: "V"
    MenuItem "outil.selection"                    label: "Sélection"     shortcut: "V"
    MenuItem "outil.main"                         label: "Main"          shortcut: "H"
  GroupeOutils "outil.cadre"                      icon: frame   label: "Cadre"   tooltip: "F"
    …                                             (la famille du doc 3)
  GroupeOutils "outil.forme"                      icon: square   label: "Formes"
    …
  ToggleButton✚ "outil.texte"                     icon: type   label: "Texte"   tooltip: "T"
  …
```
- **Outils visibles sur la capture du 02/09** (pour contrôle, pas comme liste) :
  sélection, cadre, rectangle, ligne, courbe / plume, texte, crayon, image,
  composants.
- `group: outils_<mode>` pour que **un seul** outil soit actif.
- Chaque bouton porte **`label`** (affiché en infobulle ici : la barre est
  verticale et étroite — c'est l'une des **cinq exceptions** « l'icône suffit » de
  la règle de NKCraft (UI_SPEC §0), à signaler dans les contrôles comme exception **voulue**).

### 8.5 `grappe_toile.nkgui`

```
HBox "grappe"                                     gap: 2   align: Center
  Dropdown "affichage.etat"                       bind: toile.etat   items: ["Repos","Survol","Appui","Focus","Focus visible","Désactivé"]   tooltip: "État affiché"
  Dropdown "affichage.theme_doc"                  bind: toile.theme_doc   tooltip: "Thème et variante du document"   (items : « Application · thème · variante », + « Deux variantes côte à côte »)
  Dropdown "affichage.tailles"                    bind: toile.tailles     tooltip: "Tailles du cadre"   (items : les largeurs de la cible, + « Trois tailles »)
  Separator "sep1"                                orientation: Vertical
  Dropdown "affichage.zoom"                       bind: toile.zoom   items: ["25 %","50 %","100 %","200 %","Ajuster"]
  ToggleButton✚ "affichage.grille"                icon: hash       bind: canvas.grille     tooltip: "Grille (Ctrl+')"
  ToggleButton✚ "affichage.magnetisme"            icon: magnet     bind: canvas.snap       tooltip: "Magnétisme (Ctrl+;)"
  Separator "sep2"                                orientation: Vertical
  Button "comportement.simuler"                   icon: play   label: "Simuler"   tooltip: "Simuler (F5) — au point de départ de la carte"   appearance: fill #3FB950 // @info.ok — le seul bouton vert : il lance
```
- `affichage.etat` ≠ Repos **et** l'élément sélectionné sans crochet de style →
  le libellé du `Dropdown` en `@fidelite.compte` avec l'infobulle « cet état n'est pas
  peint par le monteur sur ce rôle » — comportement continu, idempotent ✅.
- `affichage.theme_doc` porte toujours le **nom** de l'application et du thème : c'est
  ce qui empêche de confondre le thème du document avec celui de l'outil (doc 3 §2.1).
- `canvas.grille`, `canvas.snap` : **clés d'état existantes** (§0.3), reprises en
  `bind`.

### 8.6 `colonne_gauche.nkgui`

```
VBox "colonne_gauche"                             gap: 0
  Expander "cg.projet"                            label: "PROJET"   expanded: true
    HBox "cg.projet_barre"                        gap: 2
      TextField "cg.projet_recherche"             placeholder: "Rechercher…"   icon✚: search   bind: projet.recherche
      Button "projet.inclusions"                  icon: git-fork   tooltip: "Graphe des inclusions"
      Button "projet.actualiser_actions"          icon: refresh-cw tooltip: "Actualiser les actions (sonde)"
    Host "cg.projet_arbre"                        hint: "applications › documents, Commun en tête ; un badge par document (le plus grave) ; nombre d'instances par composant"
  Splitter "cg.partage"                           orientation: Horizontal   ratio: 0.55
    Expander "cg.structure"                       label: "STRUCTURE"   expanded: true
      HBox "cg.structure_barre"                   gap: 2
        TextField "cg.structure_recherche"        placeholder: "Filtrer…"   bind: structure.filtre
        Button "hier.pages.plus"                  icon: plus   tooltip: "Nouvelle page"
      Host "cg.structure_arbre"                   hint: "arbre du document : rôle, nom, badge de composant (ancré à droite, ne se coupe jamais — E2), marque de fidélité, badge de contrôle"
    Expander "cg.composants"                      label: "COMPOSANTS"   expanded: true
      HBox "cg.composants_barre"                  gap: 2
        Dropdown "cg.composants_source"           bind: composants.source   items: ["Ce document","Commun","Application","Tous"]
        Button "hier.composants.plus"             icon: plus   tooltip: "Convertir la sélection en composant (Ctrl+K)"
      Text "cg.composants_vide"                   text: "Aucun composant dans ce document. Ctrl+K sur un élément en crée un."   wrap: true   visible: false
      Host "cg.composants_liste"                  hint: "composants : aperçu, nom, instances ↗ ; les rôles proposés en pointillés"
```
- `hier.pages.plus`, `hier.composants.plus` : **identifiants existants** — gardés
  comme **actions** (ce sont des boutons qui déclenchent).
- `cg.composants_vide.visible = (composants.nb == 0)` ✅ (le texte vient de la
  capture du 02/09 : **les états vides parlent**).

### 8.7 L'inspecteur

#### `inspecteur.nkgui` (cadre)

```
Panel "inspecteur"                                title: "Inspecteur"
  VBox "colonne"                                  gap: 0
    EnTeteIdentite "insp.identite"
    TabBar "insp.onglets"                         bind: insp.onglets   tabs: ["Design","Widget","Comportement"]
    Scroll "insp.defil"                           axis: Vertical
      Host "insp.contenu"                         hint: "insp_design / insp_widget / insp_comportement selon l'onglet ; insp_toile si rien n'est sélectionné"
```
- `insp.onglets` : **clé d'état existante**.
- Rien de sélectionné → `insp.contenu` monte `insp_toile` (la vue « Objets de la
  scène » du doc 3 §12.1 et les réglages de la toile — clés `insp.canvas.*`).

#### `insp_design.nkgui`

**Les catégories de la capture du 02/09**, dans leur ordre, **refaites selon le
doc 3 §12.3** : lignes de 30 px, gouttières de 8 px, **sections vides masquées**,
aides en ⓘ.

```
VBox "racine"                                     gap: 16
  Categorie "cat_disposition"                     label: "Disposition"
    LigneVec3 "insp.pos"                          etiquette: "Position"   bind: insp.pos   components✚: 2
    LigneListe "insp.largeur_mode"                etiquette: "Largeur"    bind: insp.largeur.mode   items: ["fixe","fraction","poids","expand","contenu"]
    HBox "insp.largeur_bornes"                    gap: 4
      LigneNombre "insp.largeur_min"              etiquette: "min"   bind: insp.largeur.min
      LigneNombre "insp.largeur_max"              etiquette: "max"   bind: insp.largeur.max
    LigneListe "insp.hauteur_mode"                etiquette: "Hauteur"    bind: insp.hauteur.mode   items: ["fixe","fraction","poids","expand","contenu"]
    HBox "insp.hauteur_bornes"                    gap: 4
      LigneNombre "insp.hauteur_min"              etiquette: "min"   bind: insp.hauteur.min
      LigneNombre "insp.hauteur_max"              etiquette: "max"   bind: insp.hauteur.max
  Categorie "cat_ancrage"                         label: "Ancrage"
    Host "insp.ancrage"                           hint: "le widget d'ancrage (doc 3 §8quater.3) ; DÉSACTIVÉ avec sa raison si le parent n'est pas en anchor"
  Categorie "cat_alignement"                      label: "Alignement"
    HBox "insp.align_h"                           gap: 2
      Text "insp.align_h_titre"                   text: "H"
      RadioGroup "insp.align.h"                   bind: insp.align.h   options: ["Début","Centre","Fin","Étirer"]   orientation: Horizontal
    HBox "insp.align_v"                           gap: 2
      Text "insp.align_v_titre"                   text: "V"
      RadioGroup "insp.align.v"                   bind: insp.align.v   options: ["Haut","Centre","Bas","Étirer"]   orientation: Horizontal
  Categorie "cat_espacement"                      label: "Espacement"   expanded: false
    LigneNombre "insp.gap"                        etiquette: "Espacement"   bind: insp.gap
    Host "insp.marges"                            hint: "marges et remplissage (quatre côtés, liés ou non)"
  VBox "insp.proprietes"                          gap: 16     (catégories présentes seulement si l'élément les porte : Remplissages, Bordures, Apparence, Typographie, Effets, Points de rupture — engendrées)
  Button "insp.ajouter_propriete"                 label: "+ Propriété ▾"   icon: plus
```
- **Les catégories qui disaient « Aucun — « + » en ajoute »** ne sont plus écrites :
  elles apparaissent dans `insp.proprietes` **quand l'élément en porte une**, et
  `insp.ajouter_propriete` liste ce qu'on peut ajouter.
- `components✚: 2` sur `LigneVec3` : une position 2D. Si `VectorField` (P5) n'admet
  pas 2 composantes, écris un `ChampVec3` sans `z` et **signale-le**.
- **Les couleurs** dans Remplissages / Bordures / Typographie sont des
  **`TokenField`** (P23) — en attendant, `Dropdown` de jetons + `ColorField`.
- **Les lignes des champs de valeur calculée** (« calculée — jamais écrite dans le
  document ») : `enabled: false` + ⓘ dont l'infobulle porte la phrase.
- Toutes les lignes : `provenance.visible = true` (§6.1), `cle.visible = false`
  sauf en mode **Animation** (où l'application les rend visibles : on anime les
  propriétés d'un widget — doc 3 §9bis).

#### `insp_widget.nkgui` et `insp_comportement.nkgui`

- **Widget** : les propriétés **du rôle** (`label`, `bind`, `min`, `max`,
  `items`…) — **engendrées depuis le catalogue du format** (`NkGuiValidate.h` sait,
  pour chaque rôle, ses propriétés et leurs types : c'est **la même règle** que les
  lignes engendrées de Nogee depuis la réflexion). **Tu n'écris que le cadre** :
  ```
  VBox "racine"                   gap: 16
    Host "insp.widget_proprietes"   hint: "propriétés du rôle, engendrées depuis le schéma du format ; une propriété non montée porte la marque ◐"
    Categorie "cat_etats"           label: "États"
      Host "insp.etats"             hint: "la pile des six états du format (insp.etat.*) : Normal, Survol, Appui, Focus, Focus visible, Désactivé — chacun avec son appearance"
  ```
- **Comportement** : la liste des **événements du rôle** (doc 2 §9) avec leur
  statut (non lié · lié et compile · invalide · ◐ pas encore déclenché), les
  **événements ajoutés à la main**, et **« Ouvrir dans le Blueprint »**
  (`comportement.ouvrir_graphe`). `Click`, `Changed`, `Hover` sont **déclenchés** ;
  les neuf autres portent ◐ « reconnu, compté, pas encore déclenché ».
  ```
  VBox "racine"                   gap: 16
    Categorie "cat_evenements_role"   label: "Événements du rôle"
      Host "insp.evenements_role"     hint: "un événement par ligne : pastille d'état, nom, comportement qui l'écoute (script ou Blueprint), ⟶ ouvrir"
    Categorie "cat_evenements_ajoutes" label: "Événements ajoutés"
      Host "insp.evenements_ajoutes"  hint: "événements créés à la main ; un événement du rôle retiré reste barré, avec « Rétablir »"
    Button "comportement.ajouter_evenement"   label: "+ Nouvel événement"   icon: plus
  ```

### 8.8 `barre_etat.nkgui`

```
HBox "barre_etat"                                 gap: 8   align: Center
  BoutonRail "aide.console"                       icon: terminal       label: "Console"
  BoutonRail "projet.controles"                   icon: shield-check   label: "Contrôles"
  BoutonRail "affichage.fidelite_panneau"         icon: eye            label: "Fidélité"
  BoutonRail "projet.tests_panneau"               icon: flask-conical  label: "Tests"
  BoutonRail "comportement.apercu"                icon: play           label: "Aperçu"
  Separator "sep1"                                orientation: Vertical
  Text "etat.aide"                                text: "Sélection — cliquer ; glisser = déplacer ; bords = redimensionner"
  Spacer "pousse"
  Text "etat.selection"                           text: ""
  Text "etat.pret"                                text: "● Prêt"
```
- `projet.controles` : compteur `@controle.*` selon la sévérité la plus haute.
- `affichage.fidelite_panneau` : compteur = `apparencesNonPeintes +
  etatsNonAppliques + …` du document courant ; `tone: fidelite.compte`, ou
  `fidelite.refuse` s'il y a des refus.
- `etat.aide` : **le texte de la capture du 02/09**, qui change avec l'outil (il
  existe : c'est une bonne idée à garder).
- `etat.pret` : « ● Prêt » en `@fidelite.peint` ; « ● Enregistrement… » ;
  « ● Non enregistré » en `@info.alerte`.

### 8.9 Les nouvelles fenêtres et panneaux

#### `fen_theme.nkgui`

```
Window "fen_theme"                                title: "Thème et jetons"
  VBox "racine"                                   gap: 6
    HBox "th.barre"                               gap: 4   align: Center
      Dropdown "th.theme"                         bind: theme.courant   (items : thèmes de l'application, du projet, livrés, de greffons)
      Text "th.herite"                            text: "étend : —"    (« étend : Rihen UE5 (Commun) »)
      Button "th.bibliotheque"                    label: "Bibliothèque…"   icon: library   tooltip: "Partir d'un thème existant : hériter ou dupliquer"
      Spacer "pousse"
      Button "projet.jeton_ajouter"               label: "+ Jeton"      icon: plus
      Button "th.variante_ajouter"                label: "+ Variante"   icon: plus
      Dropdown "th.apercu"                        bind: theme.apercu   (items : les variantes, + « Deux côte à côte »)
    TabBar "th.onglets"                           bind: theme.onglet   tabs: ["Jetons","Livraison"]
    VBox "th.page_jetons"                         gap: 4
      TabBar "th.familles"                        bind: theme.famille
                                                  tabs: ["Tous","Structure","Sélection","Information","Axes","Origines","États","Moniteur","Grille","Provenance","Moteur"]
      Host "th.entete"                            hint: "en-tête : jeton · une colonne PAR VARIANTE (nombre variable) · sens · AA · usages · T/L"
      Scroll "th.defil"                           axis: Vertical
        VBox "th.lignes"                          gap: 0     (LigneJeton par jeton, ajoutées par l'application)
    VBox "th.page_livraison"                      gap: 6   visible: false
      Text "th.liv_aide"                          text: "Ce que l'application embarque et ce que son utilisateur peut choisir. C'est la section themes { } de sa carte."   wrap: true
      LigneListe "th.liv_embarques"               etiquette: "Thèmes embarqués"     bind: carte.themes.embarques   (liste à cocher)
      LigneListe "th.liv_defaut"                  etiquette: "Thème par défaut"     bind: carte.themes.defaut
      LigneListe "th.liv_variante"                etiquette: "Variante"             bind: carte.themes.variante   items: ["Suit le système","Sombre","Clair"]
      LigneCase "th.liv_choix"                    etiquette: "L'utilisateur choisit" bind: carte.themes.choix_utilisateur
      LigneCase "th.liv_greffons"                 etiquette: "Thèmes par greffon"    bind: carte.themes.greffons
```
- `th.page_jetons.visible = (theme.onglet == "Jetons")`, `th.page_livraison.visible =
  (theme.onglet == "Livraison")` — comportement continu ✅.
- Le **sens vide** d'un jeton : le `TextField` passe en `@controle.erreur` et
  l'enregistrement est **refusé avec sa raison** (doc 3 §26).
- **Contraste** : un symbole par variante (« ✓ ✓ » / « ✓ ⚠ »), `@controle.*` selon le
  résultat.
- `LigneJeton` a **une colonne par variante** : l'application ajoute les `TokenField✚`
  (en attendant, `ColorField`) selon les variantes du thème ; le composant du §6.2 en
  montre deux (Sombre, Clair), le cas courant.
- `th.page_livraison` est **grisée avec sa raison** si le document n'appartient à aucune
  carte : « Ce thème n'est livré par aucune application — ouvre la carte d'abord ».

#### `fen_actions.nkgui`

```
Window "fen_actions"                              title: "Actions"
  VBox "racine"                                   gap: 6
    HBox "ac.barre"                               gap: 4   align: Center
      Dropdown "ac.application"                   bind: actions.application   (items : applications du projet)
      TextField "ac.recherche"                    placeholder: "Rechercher…"   icon✚: search   bind: actions.recherche
      RadioGroup "ac.filtre"                      bind: actions.filtre   options: ["Toutes","✅","✚","?"]   orientation: Horizontal
      Spacer "pousse"
      Button "projet.actualiser_actions"          label: "Actualiser"   icon: refresh-cw
    Table "ac.tableau"                            columns: ["identifiant","libellé","raccourci","état","raison de grisé","utilisée par"]   (non monté : attendu)
    Host "ac.tableau_hote"                        hint: "le registre tant que Table n'est pas monté ; clic sur « utilisée par » → la liste des endroits"
    Text "ac.sources"                             text: "Sources : sonde de l'application (servies) · tables §6 des spécifications (à créer, raisons). Jamais saisi à la main."   wrap: true
```

#### `pan_controles.nkgui`

```
Panel "pan_controles"                             title: "Contrôles"
  VBox "colonne"                                  gap: 4
    HBox "ct.barre"                               gap: 4
      Dropdown "ct.portee"                        bind: controles.portee   items: ["Document","Application","Projet"]
      RadioGroup "ct.severite"                    bind: controles.severite   options: ["Tout","Erreurs","Alertes","Infos"]   orientation: Horizontal
      Spacer "pousse"
      Button "projet.regles"                      label: "Règles…"   icon: list-checks
    Text "ct.vide"                                text: "Aucun manquement aux règles du projet."   visible: false
    Scroll "ct.defil"                             axis: Vertical
      VBox "ct.liste"                             gap: 2     (CarteControle par manquement, groupées par règle)
```

#### `pan_fidelite.nkgui`

```
Panel "pan_fidelite"                              title: "Fidélité"
  VBox "colonne"                                  gap: 4
    Text "fi.explication"                         text: "Ce que le monteur NKGui fera de ce document, aujourd'hui."   wrap: true
    Grid "fi.compteurs"                           columns: 2   sizes: [0.7, 0.3]   gap: 2
      Text "fi.l_roles"                           text: "Rôles inconnus (refusé)"
      Text "fi.v_roles"                           text: "0"
      Text "fi.l_attributs"                       text: "Attributs non honorés"
      Text "fi.v_attributs"                       text: "0"
      Text "fi.l_apparences"                      text: "Apparences non peintes"
      Text "fi.v_apparences"                      text: "0"
      Text "fi.l_etats"                           text: "États non appliqués"
      Text "fi.v_etats"                           text: "0"
      Text "fi.l_menus"                           text: "Entrées de menu hors menu"
      Text "fi.v_menus"                           text: "0"
      Text "fi.l_actions"                         text: "Actions inconnues"
      Text "fi.v_actions"                         text: "0"
      Text "fi.l_hotes"                           text: "Hôtes (servis / réclamés)"
      Text "fi.v_hotes"                           text: "— / —"
      Text "fi.l_tailles"                         text: "Tailles sans équivalent"
      Text "fi.v_tailles"                         text: "0"
    Host "fi.details"                             hint: "pour le compteur choisi : la liste des éléments, clic → sélection sur la toile"
```
- **Les libellés reprennent les noms des compteurs du monteur** (doc 2 §22) :
  si le monteur en ajoute un, **ajoute une ligne** — n'en invente pas.
- Chaque `fi.v_*` : `@fidelite.peint` à 0, `@fidelite.compte` sinon ;
  `fi.v_roles` et `fi.v_menus` en `@fidelite.refuse` s'ils sont non nuls ✅.

#### `fen_inclusions.nkgui`

```
Window "fen_inclusions"                           title: "Graphe des inclusions"
  VBox "racine"                                   gap: 4
    HBox "gi.barre"                               gap: 4
      Dropdown "gi.application"                   bind: inclusions.application   items: ["Toutes"]   (et les applications)
      Button "gi.cadrer"                          icon: scan   tooltip: "Cadrer (F)"
    Host "gi.graphe"                              hint: "documents en boîtes, include en flèches ; cycle et introuvable en rouge (@controle.erreur)"
```

#### `dlg_coller_arbre.nkgui`

```
Window "dlg_coller_arbre"                         title: "Depuis une spécification"   modal: true
  VBox "racine"                                   gap: 6
    Text "ca.aide"                                text: "Collez un arbre écrit dans la notation des spécifications : Rôle \"identifiant\"   propriété: valeur, indenté."   wrap: true
    Splitter "ca.partage"                         orientation: Vertical   ratio: 0.5
      TextField "ca.texte"                        multiline: true   bind: coller.texte   font: "Mono-11"
      VBox "ca.droite"                            gap: 4
        Host "ca.apercu"                          hint: "aperçu du document produit"
        Text "ca.problemes_titre"                 text: "Non compris"
        Host "ca.problemes"                       hint: "lignes non comprises, avec la raison — rien n'est deviné"
    HBox "ca.options"                             gap: 6
      Dropdown "ca.application"                   bind: coller.application   (items : applications du projet)
      Checkbox "ca.inclure_commun"                label: "Inclure les composants communs"   bind: coller.inclure_commun
    HBox "ca.boutons"                             gap: 4   justify: End
      Button "ca.annuler"                         label: "Annuler"
      Button "fichier.creer_depuis_spec"          label: "Créer le document"   icon: file-plus
```
`fichier.creer_depuis_spec.enabled = (coller.nb_problemes == 0)` ✅ ; raison :
« Des lignes ne sont pas comprises ».
⚠️ `font` sur un `TextField` : compté non peint (une seule fonte) — écris-le.

### 8.10 `accueil.nkgui`

```
Window "accueil"                                  title: "NKUIDesign"
  Splitter "ac.partage"                           orientation: Vertical   ratio: 0.2
    VBox "ac.nav"                                 gap: 2
      Image "ac.logo"                             source: "logo_rihen"
      ListBox "ac.section"                        bind: accueil.section   items: ["Projets","Récents","Nouveau","Apprendre"]
    VBox "ac.contenu"                             gap: 8
      HBox "ac.actions"                           gap: 8
        Button "fichier.ouvrir_projet"            label: "Ouvrir un projet d'interfaces…"   icon: folder-open
        Button "fichier.ouvrir"                   label: "Ouvrir un document…"               icon: file
        Button "fichier.nouveau_depuis_spec"      label: "Depuis une spécification…"         icon: clipboard-paste
      Scroll "ac.defil"                           axis: Vertical
        Flow "ac.cartes"                          gap: 8     (Tile par projet ou document récent : aperçu, nom, application, date)
      Text "ac.nkuidoc"                           text: "Des fichiers .nkuidoc ont été trouvés. Les convertir en .nkgui ?"   visible: false
      Button "fichier.convertir_nkuidoc"          label: "Convertir…"   visible: false
```
`ac.nkuidoc.visible = fichier.convertir_nkuidoc.visible = accueil.nkuidoc_trouves` ✅.

### 8.11 `dispositions.nkgui`

```
layout "defaut"
  dock "colonne_gauche" left 0.17 · dock "toile" center · dock "inspecteur" right 0.20 · rail bottom (barre_etat)
layout "design"
  idem "defaut"
layout "comportement"
  dock "colonne_gauche" left 0.14 · dock "toile" center (mode Comportement) · dock "inspecteur" right 0.22
layout "source"
  dock "colonne_gauche" left 0.14 · dock "toile" center (mode Split Design | Source) · dock "inspecteur" right 0.20 · dock "pan_fidelite" bottom 0.20
```
- **La toile** est un `Host` de la disposition ; la **bascule de mode**, la **barre
  d'outils flottante** et la **grappe** sont posées **par-dessus** (placement absolu)
  — c'est le dessin du doc 3 §4 : **elles flottent, elles ne sont pas amarrées**.
- Les **rails de pastilles** (doc 3 §13) restent peints par l'application tant que
  `layout` ne sait pas dire « rail ».

### 8.12 `barre_booleens.nkgui` — la barre contextuelle

```
HBox "barre_booleens"                             gap: 2   align: Center
  Button "objet.booleen_union"                    icon: squares-unite       label: "Union"         tooltip: "Ctrl+Maj+U"
  Button "objet.booleen_soustraire"               icon: squares-subtract    label: "Soustraire"    tooltip: "Ctrl+Maj+P"
  Button "objet.booleen_intersecter"              icon: squares-intersect   label: "Intersecter"   tooltip: "Ctrl+Maj+I"
  Button "objet.booleen_exclure"                  icon: squares-exclude     label: "Exclure"       tooltip: "Ctrl+Maj+X"
  Separator "sep"                                 orientation: Vertical
  Button "objet.aplatir"                          icon: layers-2            label: "Aplatir"       tooltip: "Destructif — les formes sources sont perdues"
```
- **Posée par l'application au-dessus de la sélection** (placement absolu calculé)
  quand **au moins deux formes** sont sélectionnées ; cachée sinon.
- Les quatre icônes Lucide `squares-*` existent ; **vérifie-les** avant de les
  écrire, et signale celles qui manquent.

Dans **`insp_design.nkgui`**, pour un **groupe booléen**, une catégorie en tête :
```
  Categorie "cat_booleen"                         label: "Booléen"
    LigneListe "insp.booleen_operation"           etiquette: "Opération"     bind: selection.booleen.operation   items: ["Union","Soustraction","Intersection","Différence"]
    LigneListe "insp.booleen_regle"               etiquette: "Remplissage"   bind: selection.booleen.regle       items: ["Non nul","Pair-impair"]
```
Et, pour un **widget à forme** (P24), dans la catégorie **Apparence** :
```
    Grid "insp.forme_widget"                      columns: 3   sizes: [0.40, 0.45, 0.15]   gap: 4
      Text "insp.forme_titre"                     text: "Forme"
      Host "insp.forme_apercu"                    hint: "aperçu de la géométrie + son nom"
      Button "objet.forme_modifier"               icon: pen-tool   tooltip: "Modifier la forme"
    LigneCase "insp.forme_clic"                   etiquette: "Zone de clic = forme"   bind: selection.hitShape
```
`cle.visible = false` sur ces lignes ; `provenance.visible = true` (règle de
l'inspecteur).

### 8.13 La simulation

#### `barre_simulation.nkgui` — en haut de la fenêtre de simulation

```
HBox "barre_simulation"                           gap: 6   align: Center
  Dropdown "sim.plateforme"                       bind: sim.plateforme   items: ["Bureau","Navigateur","Mobile"]
  Dropdown "sim.theme"                            bind: sim.theme        tooltip: "Thème et variante (ceux que l'application embarque)"
  Dropdown "sim.appareil"                         bind: sim.appareil     visible: false   (items : appareils — tailles réelles)
  ToggleButton✚ "sim.orientation"                 icon: smartphone   bind: sim.paysage   tooltip: "Portrait / paysage"   visible: false
  Separator "sep1"                                orientation: Vertical
  ToggleButton✚ "comportement.geler"              icon: pause         label: "Geler"   bind: sim.gelee   tooltip: "F6"
  Button "comportement.recharger"                 icon: rotate-ccw    tooltip: "Recharger (Maj+F5)"
  Separator "sep2"                                orientation: Vertical
  Host "sim.fil"                                  hint: "pile de navigation : accueil › principale › [rendu] — clic = y revenir"
  Spacer "pousse"
  Button "sim.carte"                              icon: map            label: "Carte"
  Button "sim.doublures"                          icon: drama          label: "Doublures"
  Button "sim.variables"                          icon: variable       label: "Variables"
  ToggleButton✚ "sim.comparer"                    icon: columns-2      label: "Comparer"   bind: sim.comparer   tooltip: "Le design à côté du rendu"
  ToggleButton✚ "comportement.test_enregistrer"   icon: circle-dot     label: "Enregistrer un test"   bind: sim.enregistre   tooltip: "Ctrl+Maj+R"
  Button "sim.attendu"                            icon: check-check    label: "＋ Attendu"   visible: false
```
- `sim.appareil.visible = sim.orientation.visible = (sim.plateforme == "Mobile")` ✅.
- `sim.attendu.visible = sim.enregistre` ✅.
- « Navigateur » est le **libellé** ; la **valeur** liée est `Web`, celle du format (doc 2 §18) — de même dans `dlg_simuler_depuis`, `fen_carte` et `pan_tests`.
- `comportement.test_enregistrer` actif → fond `@simulation.enregistre` **et** toute la
  barre prend un liseré bas de la même couleur (on doit savoir qu'on enregistre).
- **Le cadre de la plateforme** (fenêtre système, cadre de navigateur avec barre
  d'adresse et ◀ ▶ ⟳, cadre d'appareil avec barre d'état et geste retour) est
  **peint par l'application** : ce sont des `Host`, pas des documents — ils imitent
  un système, ils ne sont pas l'interface de NKUIDesign.

#### `dlg_simuler_depuis.nkgui`

```
Window "dlg_simuler_depuis"                       title: "Simuler à partir de…"   modal: true
  VBox "racine"                                   gap: 6
    LigneListe "sim.ecran_depart"                 etiquette: "Écran"        bind: sim.depart   (items : écrans de la carte ; par défaut, celui qu'on édite)
    LigneListe "sim.plateforme_depart"            etiquette: "Plateforme"   bind: sim.plateforme   items: ["Bureau","Navigateur","Mobile"]
    Categorie "sim.etat"                          label: "État de départ"   expanded: false
      Host "sim.etat_variables"                   hint: "variables de l'application : nom, valeur (modifiable)"
      Host "sim.etat_doublures"                   hint: "réponses des services au départ"
      Host "sim.etat_historique"                  hint: "écrans à mettre dans la pile AVANT celui de départ (pour que « retour » ait un sens)"
    Checkbox "sim.memoriser"                      label: "Mémoriser ce départ pour ce document"   bind: sim.memoriser
    HBox "boutons"                                gap: 4   justify: End
      Button "sim.annuler"                        label: "Annuler"
      Button "sim.lancer"                         label: "Simuler"   icon: play
```
Toutes les lignes : `cle.visible = false`.

#### `pan_doublures.nkgui`

```
Panel "pan_doublures"                             title: "Doublures"
  VBox "colonne"                                  gap: 4
    Text "db.aide"                                text: "Ce que répondent les services pendant la simulation. Chaque réponse est marquée [DOUBLURE] dans le journal."   wrap: true
    Host "db.liste"                               hint: "un service par ligne : nom, dernière réponse, SUCCÈS / ÉCHEC (les deux boutons aussi visibles l'un que l'autre), valeur rendue, nombre d'appels"
    Button "sim.doublure_ajouter"                 label: "+ Doublure"   icon: plus
```

#### `pan_variables.nkgui`

```
Panel "pan_variables"                             title: "Variables"
  VBox "colonne"                                  gap: 4
    TextField "var.recherche"                     placeholder: "Rechercher…"   bind: sim.var_recherche
    Host "var.liste"                              hint: "variables de l'application : nom, type, valeur modifiable en direct, dernier écrivain (quel comportement)"
```

#### `fen_carte.nkgui`

```
Window "fen_carte"                                title: "Carte de l'application"
  VBox "racine"                                   gap: 4
    HBox "ca.barre"                               gap: 4   align: Center
      Dropdown "carte.application"                bind: carte.application   (items : applications du projet)
      LigneListe "carte.depart"                   etiquette: "Point de départ"   bind: carte.depart
      LigneListe "carte.plateforme"               etiquette: "Plateforme"        bind: carte.plateforme   items: ["Bureau","Navigateur","Mobile"]
      Spacer "pousse"
      Button "carte.ajouter_ecran"                label: "+ Écran"   icon: plus
      ToggleButton✚ "carte.couverture"            icon: route   label: "Parcours"   bind: carte.voir_couverture   tooltip: "Transitions parcourues (couleur) / jamais parcourues (gris)"
    Host "carte.graphe"                           hint: "écrans en boîtes (type : Fenetre, Dialogue, Feuille, Panneau, Superposition — doc 2 §18), transitions = les `open` / `back` des comportements, en flèches ; écran de départ marqué ; un `open` vers un écran absent en rouge"
```
📌 **Les transitions ne se dessinent pas à la main** : elles sont **lues dans les
comportements** (`open "x"`). La carte est une **vue** du code, jamais une seconde
source de vérité.

### 8.14 Les tests

#### `pan_tests.nkgui`

```
Panel "pan_tests"                                 title: "Tests"
  VBox "colonne"                                  gap: 4
    HBox "te.barre"                               gap: 4   align: Center
      Button "projet.tests_tous"                  label: "Tout"        icon: play
      Button "projet.tests_echoues"               label: "Échoués"     icon: rotate-ccw
      Checkbox "te.sans_fenetre"                  label: "Sans fenêtre"   bind: tests.sans_fenetre
      Dropdown "te.plateforme"                    bind: tests.plateforme   items: ["Bureau","Navigateur","Mobile","Les trois"]
      Spacer "pousse"
      Button "projet.tests_verifier"              label: "Vérifier les tests"   icon: shield-question   tooltip: "Relance chaque test SANS ce qu'il attend : il doit échouer"
    Table "te.tableau"                            columns: ["test","état","durée",""]   (non monté : attendu)
    Host "te.tableau_hote"                        hint: "un test par ligne : ✓ réussi / ✕ échoué / ○ jamais lancé / ⚠ inerte ; ▶ rejouer à l'écran ; ✎ ouvrir le texte ; 🖼 attendu / obtenu pour un échec"
    Host "te.couverture"                          hint: "Couverture : écrans n/N · transitions · BRANCHES (vraie / fausse par condition) · callbacks · services — clic → ce qui n'est jamais atteint"
```
Couleurs des états (peintes dans le `Host`, jetons existants) : réussi
`@test.reussi`, échoué `@test.echoue`, inerte `@controle.alerte`, jamais lancé
`@test.jamais` (§4).

#### `carte_test.nkgui` — le test qu'on vient d'enregistrer

```
Panel "carte_test"                                appearance: fill #9B6BF2@35% // @info.ia   pattern✚: Hatch   radius: 2
  VBox "corps"                                    gap: 4
    TextField "ct.nom"                            bind: test.nom   placeholder: "Nom du test (ce qu'il prouve)"
    TextField "ct.texte"                          multiline: true   bind: test.texte   font: "Mono-11"
    Text "ct.avertissement"                       text: "Ce test n'attend rien : il ne peut pas échouer."   visible: false
    HBox "ct.boutons"                             gap: 4   justify: End
      Button "tests.jeter"                        label: "Jeter"
      BoutonEcrit "tests.enregistrer"             label: "Enregistrer le test"   icon: save
```
- `ct.avertissement.visible = (test.nb_expect == 0)`, en `@controle.alerte` ✅.
- `BoutonEcrit` : enregistrer **écrit** dans le fichier de tests (sens de la famille).
- La carte est **violette hachurée** (proposition non encore appliquée) : même
  langage que les propositions de l'IA.

---

### 8.15 Le Blueprint — ce qui entoure la toile Behavior

La toile (nœuds, fils, mini-carte) est un **`Host`** : elle a ses propres gestes. Autour
d'elle, trois documents.

#### `outils_comportement.nkgui` — la barre flottante du mode Behavior

```
VBox "outils_comportement"                        gap: 2
  GroupeOutils "outil.bp_selection"               icon: mouse-pointer-2   label: "Sélection"   tooltip: "V"
    MenuItem "outil.bp_selection"                 label: "Sélection"   shortcut: "V"
    MenuItem "outil.main"                         label: "Main"        shortcut: "H"
  GroupeOutils "outil.bp_noeud"                   icon: box   label: "Nœud"   tooltip: "N — ouvre la recherche filtrée sur la famille"
    MenuItem "outil.bp_evenement"                 label: "Événement"
    MenuItem "outil.bp_flux"                      label: "Flux"
    MenuItem "outil.bp_donnee"                    label: "Donnée"
    MenuItem "outil.bp_interface"                 label: "Interface"
    MenuItem "outil.bp_action"                    label: "Action"
    MenuItem "outil.bp_fonction"                  label: "Fonction"
  GroupeOutils "outil.bp_liaison"                 icon: spline   label: "Liaison"   tooltip: "L"
    MenuItem "outil.bp_liaison"                   label: "Liaison"
    MenuItem "outil.bp_coupe"                     label: "Coupe-liaison"
    MenuItem "outil.bp_renvoi"                    label: "Renvoi"
  GroupeOutils "outil.bp_commentaire"             icon: sticky-note   label: "Commentaire"   tooltip: "C"
    MenuItem "outil.bp_note"                      label: "Note"
    MenuItem "outil.bp_cadre"                     label: "Cadre de groupe"
  Separator "sep"                                 orientation: Horizontal
  ToggleButton✚ "comportement.point_arret"        icon: circle-dot   label: "Point d'arrêt"   tooltip: "F9"
  Button "comportement.pas_a_pas"                 icon: step-forward label: "Pas à pas"       tooltip: "F10"
```
- `group: outils_comportement` : un seul outil actif.
- `comportement.point_arret.enabled` et `comportement.pas_a_pas.enabled` = **simulation en cours**, sinon
  désactivés **avec leur raison** (« Lance d'abord la simulation (F5) ») — comportement
  continu ✅.

#### `pan_comportements.nkgui` — la liste à gauche de la toile

```
Panel "pan_comportements"                         title: "Comportements"
  VBox "colonne"                                  gap: 4
    HBox "bc.barre"                               gap: 4   align: Center
      Dropdown "bc.portee"                        bind: behavior.portee   items: ["Ce widget","Ce composant","Cette page","Cette application"]   tooltip: "Portée"
      Spacer "pousse"
      Button "comportement.nouveau_blueprint"     icon: plus   tooltip: "Nouveau Blueprint"
      Button "comportement.nouvelle_fonction"     icon: function-square   tooltip: "Nouvelle fonction"
    TextField "bc.recherche"                      placeholder: "Rechercher un comportement…"   bind: behavior.recherche
    Host "bc.liste"                               hint: "un comportement par ligne : widget source + événement (ou nom de fonction / macro), forme (graphe ⬡ | script ⟨⟩), état (✓ compile · ✕ E-GRAPHE avec le nœud), compteur d'exécutions en simulation ; fonctions de Commun/ en tête, grisées (lecture seule ici)"
    HBox "bc.pied"                                gap: 4   align: Center
      Button "bp.ranger"                          label: "Ranger"    icon: layout-grid   tooltip: "Disposition automatique (Maj+L)"
      Button "comportement.vue_code"              label: "Code"      icon: code          tooltip: "Vue Code (Ctrl+²)"
      Text "bc.conversion"                        text: ""           (« Code en lecture seule : nœud Porte "g3" » quand la conversion est impossible)
```
- `bc.conversion.visible = behavior.code_lecture_seule` ; le texte **nomme** le nœud
  responsable (doc 3 §9.5) ✅.
- Ce panneau s'ancre à gauche de la toile **en mode Behavior seulement** (disposition
  `comportement`, §8.11).

#### `pop_recherche_noeuds.nkgui` — la recherche de nœuds

```
Window "pop_recherche_noeuds"                     title: ""   flags: NoTitleBar   modal: false
  VBox "racine"                                   gap: 2
    TextField "rn.saisie"                         placeholder: "Chercher un nœud, un widget, un callback…"   bind: bp.recherche
    Text "rn.filtre"                              text: ""    (« Type attendu : Texte » quand on vient d'une broche)
    Host "rn.resultats"                           hint: "résultats groupés par famille (bandeau coloré) : nœuds du catalogue, widgets du document (Lire… / Régler… / Événement…), callbacks du projet, fonctions ; Entrée pose, flèches naviguent"
```
- Ouverte par `Tab`, le clic droit dans le vide, ou un fil lâché dans le vide ; posée
  **sous le curseur** par l'application (placement absolu).
- `rn.filtre.visible = bp.recherche_depuis_broche` ✅.

### 8.16 `menus_contextuels.nkgui` — les clics droits

Un `ContextMenu` **par contexte** (doc 3 §5bis.11). `ContextMenu` est **non monté** :
écris-les quand même — ils voyagent, et l'application garde son menu contextuel codé
(`MenuContexte.h`) tant que le monteur ne les pose pas. **Les actions sont celles de la
barre de menus** (§0.3) : un même identifiant, jamais un doublon.

```
ContextMenu "ctx.toile_vide"
  MenuItem "edition.coller"                       label: "Coller"
  MenuItem "edition.coller_meme_place"            label: "Coller à la même place"
  Separator "s1"
  MenuItem "objet.ajouter_cadre"                  label: "Ajouter un cadre…"
  MenuItem "ia.generer_ici"                       label: "Générer avec l'IA…"
  Separator "s2"
  MenuItem "affichage.ajuster_page"               label: "Ajuster à la page"
  MenuItem "affichage.grille"                     label: "Grille"
  MenuItem "affichage.magnetisme"                 label: "Magnétisme"

ContextMenu "ctx.element"
  MenuItem "objet.attribuer_role"                 label: "Devenir…"
  MenuItem "objet.retirer_role"                   label: "Retirer le rôle"
  MenuItem "objet.choisir_action"                 label: "Choisir l'action…"
  Separator "s1"
  MenuItem "edition.couper"  · "edition.copier" · "edition.dupliquer" · "edition.supprimer"      (quatre MenuItem)
  Separator "s2"
  MenuItem "objet.grouper"                        label: "Grouper"
  MenuItem "objet.extraire_composant"             label: "Convertir en composant"
  Separator "s3"
  Menu "ctx.ordre"      label: "Ordre"      (les quatre de objet.*)
  Menu "ctx.aligner"    label: "Aligner"
  Separator "s4"
  MenuItem "objet.verrouiller"                    label: "Verrouiller"
  MenuItem "objet.masquer_editeur"                label: "Masquer dans l'éditeur"
  Menu "ctx.dispo"      label: "Disponibilité"  (les quatre de objet.dispo_*)
  Separator "s5"
  MenuItem "comportement.definir_evenements"      label: "Définir des événements…"

ContextMenu "ctx.page"            (Renommer · Dupliquer la page · Supprimer la page · Cible ▸ · Orientation ▸ · Centrer · Rapport de transposition…)
ContextMenu "ctx.instance"        (Éditer le maître · Réinitialiser au maître · Détacher · Sélectionner toutes les instances · Promouvoir en partagé)
ContextMenu "ctx.composant"       (Renommer · Éditer · Dupliquer · Supprimer · Promouvoir en partagé · Sélectionner ses instances)
ContextMenu "ctx.evenement"       (Lier… · Délier · Ouvrir dans le Blueprint · Retirer du rôle · Rétablir)
ContextMenu "ctx.noeud"           (Couper · Copier · Supprimer · Désactiver · Point d'arrêt · Commentaire · Réduire en fonction · Réduire en macro · Aller à la définition · Aller au widget)
ContextMenu "ctx.fil"             (Poser un renvoi · Couper · Insérer un nœud de conversion)
ContextMenu "ctx.booleen"         (Opération ▸ · Aplatir… · Utiliser comme forme du widget · Enregistrer comme icône…)
ContextMenu "ctx.hote"            (Contenu fictif… · Retirer le contenu fictif · Qui la remplit ?)
ContextMenu "ctx.greffon"         (Activer · Désactiver · Voir les permissions… · Voir les contributions… · Désinstaller…)
```
Les contextes écrits entre parenthèses se déploient **comme `ctx.element`** : un
`MenuItem` par entrée, identifiant = l'action de la barre de menus quand elle existe,
sinon une action du domaine du contexte (`ctx.page` → `page.renommer`…). **Règle du
doc 3** : un bloc entier sans objet se retire, une entrée isolée se grise ; en sélection
multiple, seules les entrées valables pour **tous**.

### 8.17 `dlg_preferences.nkgui`

```
Window "dlg_preferences"                          title: "Préférences"   modal: true
  Splitter "pr.partage"                           orientation: Vertical   ratio: 0.25
    ListBox "pr.categorie"                        bind: prefs.categorie
                                                  items: ["Apparence","Langue","Toile","Fichiers","Simulation","Raccourcis","Intelligence artificielle","Greffons"]
    Scroll "pr.defil"                             axis: Vertical
      VBox "pr.apparence"                         gap: 8
        LigneListe "pr.theme_outil"               etiquette: "Thème de l'outil"   bind: prefs.theme_outil     (items : Rihen UE5, GitHub Pro, Contraste élevé, installés)
        LigneListe "pr.variante_outil"            etiquette: "Variante"           bind: prefs.variante_outil  items: ["Système","Sombre","Clair"]
        Text "pr.theme_note"                      text: "Le thème de l'outil ne change jamais celui des documents."   wrap: true
        LigneNombre "pr.taille_texte"             etiquette: "Taille du texte"    bind: prefs.taille_texte
        LigneListe "pr.grille_behavior"           etiquette: "Grille du Blueprint" bind: prefs.grille_bp     items: ["Points","Lignes"]
      VBox "pr.autres"                            gap: 8   visible: false
        Host "pr.page"                            hint: "la page de la catégorie choisie (doc 3 §20) ; Raccourcis = la table unique (doc 3 §29) avec couches défaut / greffon / utilisateur et conflits affichés"
    HBox "pr.boutons"                             gap: 4   justify: End
      Button "pr.reinitialiser"                   label: "Réinitialiser la catégorie"
      Button "pr.fermer"                          label: "Fermer"
```
`pr.apparence.visible = (prefs.categorie == "Apparence")`, `pr.autres.visible =`
l'inverse ✅. La clé de l'IA **n'est jamais** un `bind` lisible par un document : la page
IA est un `Host`.


---

## 9. Les actions

### 9.1 Reprises de l'existant (✅ si la sonde les voit câblées)

Toutes celles du §8.1 **sans** `// +`, plus : `hier.pages.plus` ·
`hier.composants.plus` · `insp.forme.ouvrir` · `insp.forme.terminer` ·
`insp.compo.detacher` · les valeurs de `NkActionCtx` (`objet.editer_texte`,
`objet.vectoriser`, `ia.contexte`, `edition.coller_par_dessus`,
`edition.copier_comme`, `objet.miroir_h`, `objet.miroir_v`,
`objet.cadrer_selection`, `objet.ajouter_agencement`, `objet.voir_composant`).

### 9.2 Nouvelles — toutes ✚

| domaine | actions |
|---|---|
| **fichier** | `nouveau_document` · `nouveau_depuis_spec` · `nouveau_depuis_gabarit` · `ouvrir_projet` · `convertir_nkuidoc` · `voir_dans_application` · `creer_depuis_spec` |
| **edition** | `coller_arbre` · `rechercher_projet` |
| **affichage** | `fidelite` · `fidelite_panneau` · `etat_repos` · `etat_survol` · `etat_appui` · `etat_desactive` · `etat_focus` · `doc_sombre` · `doc_clair` · `doc_les_deux` · `tailles_multiples` · `theme_sombre` · `theme_clair` |
| **mode** | `source` |
| **objet** | `choisir_action` · `choisir_jeton` · `poser_hote` |
| **projet** | `explorateur` · `inclusions` · `theme` · `actions` · `actualiser_actions` · `controles` · `regles` · `jeton_ajouter` · `usages_jeton` · `aller_a` · `corriger` · `tests_tous` · `tests_echoues` · `tests_verifier` · `tests_couverture` · `tests_panneau` |
| **insp** | `ajouter_propriete` |
| **objet** (booléens, formes) | `booleen_union` · `booleen_soustraire` · `booleen_intersecter` · `booleen_exclure` · `aplatir` · `vectoriser_contour` · `masque` · `forme_du_widget` · `forme_modifier` · `enregistrer_icone` |
| **comportement** (simulation) | `simuler_depuis` · `carte` · `test_enregistrer` · `point_arret` · `pas_a_pas` |
| **sim** | `orientation` · `carte` · `doublures` · `variables` · `attendu` · `annuler` · `lancer` · `doublure_ajouter` |
| **carte** | `ajouter_ecran` · `couverture` |
| **tests** | `jeter` · `enregistrer` |
| **fenetre** | `dispo_source` |
| **comportement** | `apercu` |
| **gi** | `cadrer` |
| **ca** | `annuler` |
| **outil** (Design — les familles du doc 3 §7.2, **existantes** : ✅ si la sonde les voit) | `selection` · `selection_directe` · `main` · `cadre` · `section` · `groupe` · `forme` · `rectangle` · `rect_arrondi` · `ellipse` · `triangle` · `polygone` · `etoile` · `ligne` · `fleche` · `plume` · `crayon` · `courbe` · `ciseaux` · `texte` · `texte_zone` · `texte_trace` · `image` · `icone` · `masque` · `regle` · `cotation` · `pipette` · `famille_<nom>` |
| **outil** (Animation, doc 3 §7.4) | `anim_selection` · `cle_poser` · `cle_supprimer` · `cle_copier` · `courbe_lineaire` · `courbe_acceleree` · `courbe_ralentie` · `courbe_perso` · `declencheur_chargement` · `declencheur_survol` · `declencheur_clic` · `declencheur_evenement` |
| **fichier** (suite) | `nouveau_via_ia` · `fermer_document` · `importer_svg` · `exporter_code` · `decouper` |
| **affichage** (suite) | `etat_focus_visible` · `theme_rihen_ue5` · `theme_github_pro` · `theme_contraste` · `theme_doc` · `tailles` |
| **comportement** (Blueprint) | `nouveau_blueprint` · `nouvelle_fonction` · `nouvelle_macro` · `definir_evenements` |
| **bp** | `ranger` · `reduire_fonction` · `reduire_macro` · `renvoi` · `conversion` |
| **outil** (Blueprint) | `bp_selection` · `bp_noeud` · `bp_evenement` · `bp_flux` · `bp_donnee` · `bp_interface` · `bp_action` · `bp_fonction` · `bp_liaison` · `bp_coupe` · `bp_renvoi` · `bp_commentaire` · `bp_note` · `bp_cadre` |
| **th** | `bibliotheque` · `variante_ajouter` · `supprimer_jeton` · `heriter` · `dupliquer` |
| **projet** (suite) | `moderniser` |
| **aide** | `mises_a_jour` · `a_propos` |
| **objet** (suite) | `ajouter_cadre` · `importer_svg` |
| **ia** | `generer_ici` |
| **sim** (suite) | `theme` · `comparer` |
| **pr** | `reinitialiser` · `fermer` |
| **page**, **instance**, **composant**, **evenement**, **noeud**, **fil**, **booleen**, **hote**, **greffon** | les entrées des menus contextuels (§8.16) qui n'ont pas d'action de la barre de menus |

### 9.3 Raisons de grisé

| cas | raison |
|---|---|
| nouvelle action non servie | « Pas encore disponible dans NKUIDesign » |
| `fichier.voir_dans_application` sans application liée au document | « Ce document n'appartient à aucune application du projet » |
| `projet.actualiser_actions` si l'application n'est pas construite | « Construis d'abord <application> : la sonde lit son binaire » |
| `affichage.fidelite` si le monteur n'est pas chargé | « Le monteur NKGui n'est pas disponible » |
| `menu_aligner`, `menu_repartir` | « À brancher » (le libellé actuel, gardé comme raison) |
| `fichier.creer_depuis_spec` avec des lignes non comprises | « Des lignes ne sont pas comprises » |
| `objet.booleen_*` avec moins de deux formes | « Sélectionne au moins deux formes » |
| `objet.forme_du_widget` sans widget dans la sélection | « Sélectionne une forme ET le widget qui la prendra » |
| `comportement.simuler_depuis`, `comportement.carte` sans carte d'application | « Ce document n'appartient à aucune carte d'application — la simulation l'ouvrira seul » |
| `comportement.test_enregistrer` hors simulation | « Lance d'abord la simulation (F5) » |
| une action d'une extension 0.4 que le runtime ne sert pas encore (instructions, carte, tests, thèmes) | « Décidé (format 0.4), pas encore servi par NKGui » |
| `comportement.point_arret`, `comportement.pas_a_pas` hors simulation | « Lance d'abord la simulation (F5) » |
| `th.supprimer_jeton` d'un jeton employé | « Ce jeton sert N fois — remplace-le d'abord » |

---

## 10. Ce que tu ne sais pas encore reproduire — écrit quand même

### 10.1 États

| élément | état | rendu |
|---|---|---|
| `RadioGroup "mode.choix"` | option active | fond `@accent` |
| `ToggleButton "affichage.fidelite"` | coché (`checked = true` : une donnée, pas un état d'apparence — doc 2 §13.2) | fond `@fidelite.compte` |
| `GroupeOutils` | outil courant | fond `@accent` |
| `BoutonRail` | panneau ouvert | liseré haut `@accent` |
| onglet de document | `Dirty` | point blanc |

### 10.2 Animations

| élément | animation |
|---|---|
| `BoutonRail` compteur | pulsation une fois quand il augmente |
| bascule vers la vue Fidélité | fondu croisé (150 ms) entre les deux rendus |
| `CarteControle` corrigée | glisse et disparaît (120 ms) |

### 10.3 Événements

| élément | événement | effet | idempotent ? |
|---|---|---|---|
| `mode.choix` | `(Changed)` | changer de mode (et de barre d'outils) | ❌ |
| `insp.identite.identifiant` | `(Changed)` | re-vérifier l'action dans le registre | ✅ (vérification rejouable) |
| `ca.texte` | `(Changed)` | relire l'arbre, rafraîchir l'aperçu | ✅ mais coûteux : **à ne pas** rejouer à chaque image sur un long texte — signale-le |
| `LigneJeton.sens` | `(Changed)` | lever / baisser le refus d'enregistrement | ✅ |
| `sim.plateforme` | `(Changed)` | rouvrir l'écran courant dans l'autre cadre | ❌ |
| `bc.portee` | `(Changed)` | filtrer la toile Behavior | ✅ |
| `th.onglets` | continu | montrer la page Jetons ou Livraison | ✅ |

📌 **Ce que les événements sont devenus le 26/09** : `(Click)`, `(Changed)`,
`(Hover)` sont **déclenchés** (`NkGuiEvenements.h`) ; `DoubleClick`, `ContextMenu`,
`Focus`, `Blur`, `KeyDown`, `Wheel`, `Submit`, `DragStart`, `Drop` sont **reconnus et
comptés** (`evenementsSansSource`). Écris la forme **`behavior "id"(Événement)`**,
qui est celle que le runtime lit ; `on … -> Behavior` (doc 2 §5) n'est **pas**
implémentée.

---

## 11. L'ordre de livraison

Aligné sur les paliers du doc 1 §9 (R1 à R6).

| # | livrable | palier | témoin |
|---|---|---|---|
| 1 | **Aucun document de NKUIDesign avant R1** : l'outil doit d'abord savoir **ouvrir** un `.nkgui` (doc 1 §4.1), sinon il ne pourrait pas éditer sa propre interface | R1 | les 6 documents de la famille ouverts, modifiés, enregistrés : diff minimal |
| 2 | `composants_nkuidesign` + `menus` + `barre_etat` | R2 | les ~110 entrées de menu émettent **la même action** que l'ancien menu (témoin d'actions émises) |
| 3 | `fen_theme` + `fen_actions` + `pan_controles` + `pan_fidelite` | R2–R3 | les fenêtres **nouvelles** d'abord : elles n'ont pas d'ancien code à égaler |
| 4 | `bascule_mode` + `grappe_toile` + `outils_*` + `onglets` | R3 | |
| 5 | `colonne_gauche` + `accueil` + `dlg_coller_arbre` + `fen_inclusions` | R4 | |
| 6 | `inspecteur` + `insp_*` (le plus gros : `Panels.h`) | R5 | E1 fermé à l'œil **et** par mesure (doc 16 : une seule valeur de gouttière) |
| 7 | `dispositions` | R5 | |
| 8 | `barre_booleens` + les catégories Booléen / Forme de l'inspecteur | R2 (dès que les booléens le sont (doc 1 §9)) | le bouton à coin coupé : composé, appliqué au widget, **monté dans NkAnimaEditor** avec sa forme |
| 9bis | `theme` (NKUIDesign) — **avant** tout autre document de NKUIDesign qui emploie une couleur | R2 | les documents de NKUIDesign ne contiennent **aucune** couleur hors jetons (contrôle §25 du doc 3 à zéro) |
| 9ter | `outils_comportement` + `pan_comportements` + `pop_recherche_noeuds` | R3 (avec l'exécution des Blueprints) | le même comportement écrit en Blueprint et en script donne **la même trace** d'exécution en simulation |
| 9quater | `menus_contextuels` + `dlg_preferences` | R5 | les clics droits émettent les **mêmes** actions que la barre de menus |
| 9 | `barre_simulation` + `dlg_simuler_depuis` + `pan_doublures` + `pan_variables` + `fen_carte` + `pan_tests` + `carte_test` | R3 | NKScena simulé en Bureau puis en Mobile ; un test enregistré passe, et **« Vérifier les tests »** le fait échouer quand on retire son `message` |

📌 **Le témoin final de R5** : NKUIDesign ouvre `NKUIDesign/barre_etat.nkgui`,
**ajoute un bouton à sa propre barre d'état**, enregistre — et le bouton apparaît
**dans la fenêtre de NKUIDesign** sans recompiler. L'outil se modifie avec lui-même.

---

## Annexe A — Contrats de comportement des surfaces (août 2026)

> **Gardée pour ses règles, pas pour sa pile.** Ces contrats ont été écrits pour une
> implémentation TypeScript / React qui n'aura pas lieu ; les **règles de
> comportement** qu'ils portent (machine à états des pastilles, ordre des sections de
> l'inspecteur, sélection multiple, statuts d'événements, aperçu de l'IA, boîtes des
> greffons, console) restent **justes** et s'appliquent à l'implémentation C++ et aux
> documents `.nkgui` ci-dessus. Lis les types comme des **descriptions** ; là où un
> contrat contredit le doc 3 réécrit (trois modes au lieu de cinq, trois provenances au
> lieu de quatre, trois statuts d'événement au lieu de quatre), **le doc 3 gagne**.

### A.0 Contrats de composants

### A.1 `<ProjectTabStrip>`
```ts
interface ProjectTab { id: string; name: string; isDirty: boolean; thumbnailUrl?: string; }
interface ProjectTabStripProps {
  tabs: ProjectTab[];
  activeTabId: string;
  onSelect: (id: string) => void;
  onClose: (id: string) => void;
  onReorder: (fromIdx: number, toIdx: number) => void;
  onDetachToNewWindow: (id: string) => void;   // drag hors de la bande
  onNewProject: () => void;                     // bouton "+", ouvre Launcher modal
}
```

### A.2 `<CanvasModeSwitch>`
```ts
type CanvasMode = "design" | "behavior" | "split";
type SplitOrientation = "vertical" | "horizontal";
interface CanvasModeSwitchProps {
  mode: CanvasMode;
  splitOrientation: SplitOrientation;
  onModeChange: (m: CanvasMode) => void;
  onSplitOrientationChange: (o: SplitOrientation) => void;
}
```

### A.3 `<DockRailPill>` — state machine
```ts
type PillState = "collapsed" | "overlay" | "docked" | "floating";
interface DockRailPillProps {
  id: string; icon: string; label: string;
  state: PillState;
  onStateChange: (s: PillState) => void;
  badgeCount?: number;               // ex. Chat IA en attente de réponse
  renderPanelContent: () => ReactNode;
}
```
Règles de transition à implémenter dans `useDockRailState.ts` (pas dans le
composant, pour rester testable indépendamment de React) :
- `collapsed → overlay` : clic simple sur la pastille.
- `overlay → collapsed` : clic sur la pastille à nouveau, clic hors panneau,
  ou `Échap`.
- `overlay → docked` : clic sur l'icône "épingle" dans l'en-tête du panneau
  overlay ; insère le panneau dans le `DockManager` (réutilise le docking de la
  coquille, doc 2 §8.4) à un emplacement par défaut adjacent au rail d'origine.
- `docked → overlay` ou `docked → floating` : clic sur l'icône "détacher".
- **Contrainte de rail** (doc 3 §13.3) : au moment où une pillule passe à
  `overlay`, toute autre pastille du **même rail** actuellement en `overlay`
  repasse à `collapsed` automatiquement — sauf les pastilles en `docked` ou
  `floating`, non affectées par cette règle. Implémenter via un registre par
  rail (`railId → activeOverlayPillId | null`), pas par un simple `useState`
  local à chaque pastille (elles doivent se coordonner).
- Cette règle est désactivable globalement depuis les Préférences (doc 3
  §20) : prévoir `enforceSingleOverlayPerRail: boolean` dans les settings
  globaux, lu par `useDockRailState`.

### A.4 `<InfiniteDesignCanvas>`
```ts
interface InfiniteDesignCanvasProps {
  pages: WidgetNode[];              // isPage === true, avec pagePosition
  shapes: ShapeNode[];
  selection: string[];
  onSelectionChange: (ids: string[], additive: boolean) => void;
  viewport: { x: number; y: number; zoom: number };
  onViewportChange: (v: InfiniteDesignCanvasProps["viewport"]) => void;
  showGrid: boolean; showRulers: boolean; snapEnabled: boolean;
  onFrameToPage: (pageId: string) => void;   // double-clic depuis Hiérarchie/SceneObjectsGrid
  onPromote: (shapeId: string, role: string) => void;
  onDemote: (widgetId: string) => void;
}
```
Rendu recommandé : un seul système de coordonnées monde partagé entre toutes
les pages (elles ne sont que des `WidgetNode` racines positionnées par
`pagePosition`, pas des documents séparés) — le "zoom to fit" sur une page
n'est qu'un calcul de viewport centré sur son `transform`, pas un changement
de contexte de rendu.

### A.5 `<SceneObjectsGrid>` (Inspecteur, aucune sélection)
```ts
interface SceneObjectsGridProps {
  pages: { id: string; name: string; thumbnailUrl?: string }[];
  onSelectPage: (id: string) => void;   // sélectionne + appelle onFrameToPage du canvas
  onCreatePage: () => void;
}
```

### A.5bis `<InspectorPanel>` — ordre des sections et sélection multiple

```ts
type InspectorSection =
  | "target" | "layout" | "anchors" | "align" | "spacing"
  | "appearance" | "typography" | "effects" | "breakpoints" | "parentLayout";

// Ordre d'affichage NORMATIF (doc 3 §12.2). Une section absente disparaît,
// elle ne se rend jamais vide.
const SECTION_ORDER: InspectorSection[] = [
  "target", "layout", "anchors", "align", "spacing",
  "appearance", "typography", "effects", "breakpoints", "parentLayout",
];

type FieldValue<T> = { kind: "same"; value: T } | { kind: "mixed" };

interface InspectorPanelProps {
  selection: ElementId[];                        // 0 = SceneObjectsGrid (§A.5)
  visibleSections: InspectorSection[];           // intersection sur toute la sélection
  collapsed: Record<InspectorSection, boolean>;  // mémorisé PAR TYPE d'élément
  activeTab: "design" | "widget" | "behavior";
}
```

⚠️ **`SECTION_ORDER` est une constante, jamais un tri calculé.** Un ordre dérivé
des données varie d'un élément à l'autre et oblige l'œil à relire le panneau à
chaque sélection ; c'est le genre de coût qu'on ne mesure pas et qui se paie à
chaque clic.

⚠️ **`FieldValue` a un état `mixed` explicite.** En sélection multiple, afficher la
valeur du premier élément à la place de `mixed` est un mensonge silencieux :
l'utilisateur croit constater une uniformité, et sa première édition la crée
au lieu de la préserver. Écrire dans un champ `mixed` applique à toute la
sélection — c'est l'opération, pas un effet de bord.

⚠️ **`collapsed` est indexé par type d'élément.** Un état replié global fait
réapparaître Typographie dépliée sur un panneau qui ne porte pas de texte.

⚠️ **Un champ inapplicable se grise ; une section inapplicable disparaît.** Les
deux règles sont différentes exprès : garder la ligne préserve la position que
l'œil a mémorisée, garder la section ferait croire à une propriété manquante.

### A.6 `<InspectorBehaviorTab>`
```ts
interface EventRow {
  eventName: string;
  status: "unbound" | "bound" | "invalid";
  boundProgramId?: string;
}
interface InspectorBehaviorTabProps {
  targetKind: "widget" | "page";
  roleEvents:   EventRow[];   // issus du contrat de rôle (doc 2 §9), ou portée "page"
  customEvents: EventRow[];   // ajoutés à la main par le concepteur (doc 3 §14ter)
  onSelectEvent: (eventName: string) => void; // bascule CanvasModeSwitch → "behavior", filtre le graphe sur cet event
  onAddCustomEvent: () => void;
  onWithdrawRoleEvent: (eventName: string) => void;
  onRestoreRoleEvent:  (eventName: string) => void;
}
```
Couleur de la pastille de statut = `--status-unbound` / `--status-bound` /
`--status-invalid` (§4).

⚠️ **`roleEvents` et `customEvents` sont deux listes distinctes dans le modèle, pas
une liste avec un drapeau d'origine.** Retirer le rôle emporte la première et doit
laisser la seconde intacte (doc 3 §14ter) ; une liste unique rendrait cette
opération dépendante d'un filtrage correct, c'est-à-dire réversible par un bug.

⚠️ **Un événement de rôle `withdrawn: true` reste dans `roleEvents`, barré et
restaurable.** Le supprimer rendrait « écarté volontairement » et « jamais vu »
indistinguables.

### A.7 `<BehaviorScopeSelector>`
```ts
type BehaviorScope = "component" | "page" | "global";
interface BehaviorScopeSelectorProps {
  scope: BehaviorScope;
  currentTargetName: string;         // nom du composant/page/"Projet" affiché
  onScopeChange: (s: BehaviorScope) => void;
}
```

### A.8 `<AiChatPanel>` / `<AIContextButton>` / `<AIPreviewCard>`
```ts
interface AiChatMessage {
  id: string; role: "user" | "assistant";
  text: string;
  previewCard?: { summary: string; thumbnailUrl?: string; diffKind: "add"|"modify"|"behavior" };
}
interface AiChatPanelProps {
  messages: AiChatMessage[];
  onSend: (prompt: string, quickAction?: "generateScreen"|"editSelection"|"generateBehavior") => void;
  onApplyPreview: (messageId: string) => void;
  onRejectPreview: (messageId: string) => void;
  isFloating: boolean;               // reflète l'état "floating" du DockRailPill parent
}
interface AIContextButtonProps {
  anchorSelectionBounds: { x:number;y:number;w:number;h:number };
  onOpenPopover: () => void;         // ouvre un mini-panel, PAS le AiChatPanel complet
}
```
Toutes les générations, quel que soit le point d'entrée (§17 doc humain),
passent par `aiGenerationService.generate(context) → Promise<DocumentPatch>`
— un `DocumentPatch` est un diff partiel du `NkGuiDocument`, jamais appliqué
tant que `onApplyPreview` n'est pas appelé. Un seul service, pas un par point
d'entrée, pour garantir la garantie « aperçu avant application » (doc 1 §6.2)
de façon centralisée plutôt que reproduite N fois.

### A.9 `<VectorPathEditor>` (édition au sommet, doc 3 §8bis)
```ts
interface VectorPathEditorProps {
  path: VectorPath;
  onAnchorMove: (anchorId: string, pos: {x:number;y:number}) => void;
  onHandleMove: (anchorId: string, handle: "in"|"out", pos: {x:number;y:number}) => void;
  onSetCornerType: (anchorId: string, type: AnchorCornerType) => void;
  onSetCornerRadius: (anchorId: string, radius: number) => void;
  onAddAnchorOnSegment: (afterAnchorId: string, t: number) => void;
  onDeleteAnchor: (anchorId: string) => void;
  onBooleanOp: (op: VectorPath["booleanOp"], shapeIds: string[]) => void;
  onDetachComponents: (shapeId: string) => void;
}
```
Overlay du `InfiniteDesignCanvas`, pas un canvas séparé — actif uniquement
en mode édition de points (double-clic d'une forme, ou outil Tracé actif).

### A.10 `<EffectsStackPanel>` (doc 3 §8ter)
```ts
interface EffectsStackPanelProps {
  effects: EffectStack;
  onChangeFill: (index: number, fill: FillEffect) => void;
  onAddShadow: () => void;
  onReorderShadow: (fromIdx: number, toIdx: number) => void;
  onChangeShadow: (index: number, shadow: ShadowEffect) => void;
  onChangeBlur: (kind: "layer"|"background", value: number | undefined) => void;
  onChangeBlendMode: (mode: EffectStack["blendMode"]) => void;
  onChangeOpacity: (value: number) => void;
}
```
Le sélecteur de dégradé (`FillEffect` avec `stops`) réutilise un composant
`<GradientRamp>` générique (curseurs de couleur draggables sur une bande),
nouveau composant `ui-kit`, pas de dépendance externe requise pour un
premier jet.

### A.11 `<ComponentLibraryPanel>` (doc 3 §14bis)
```ts
interface ComponentSummary {
  id: string; name: string; thumbnailUrl?: string; instanceCount: number;
  source: "local" | "imported" | "marketplace";
}
interface ComponentLibraryPanelProps {
  components: ComponentSummary[];
  onDragToCanvas: (componentId: string) => void;    // crée une instance
  onImportFromFile: () => void;
  onEditMaster: (componentId: string) => void;       // ouvre l'onglet canvas isolé
  searchQuery: string; onSearchChange: (q: string) => void;
}
interface InstanceOverrideBadgeProps {
  isOverridden: boolean;
  onResetToMaster: () => void;
}
```
`InstanceOverrideBadge` s'intègre à `<PropertyRow>` (doc 2 §4.1) exactement
comme `isOverridden` y est déjà prévu pour Aetherion — même prop, même
rendu, réutilisation directe sans nouveau composant visuel.

### A.12 `<WidgetAnimationCanvas>` (doc 3 §9bis)
Composant composite réutilisant **directement** les contrats déjà définis
dans `05-specification-claude-animation-vfx.md` (§4.6 `StateMachineCanvas`,
§4.4 `DopeSheet`, §4.5 `CurveEditorCanvas`) — importés depuis le package
partagé plutôt que redéfinis :
```ts
interface WidgetAnimationCanvasProps {
  program: WidgetAnimationProgram;
  viewMode: "stateMachine" | "dopeSheet" | "curveEditor";
  onViewModeChange: (m: WidgetAnimationCanvasProps["viewMode"]) => void;
  scope: BehaviorScope;                          // réutilise §A.7, même sélecteur de portée
  onScopeChange: (s: BehaviorScope) => void;
  onAddState: () => void;
  onPreviewTransition: (transitionId: string) => void; // joue dans InfiniteDesignCanvas en incrustation
  easingPresets: Record<string, CurveChannel>;    // Linéaire/EaseIn/EaseOut/EaseInOut/Bounce/Élastique
}
```
Implémentation recommandée : extraire `StateMachineCanvas`, `DopeSheet` et
`CurveEditorCanvas` dans un **package partagé** entre Aetherion Animate & FX
et NkUIDesign (ex. `@nkentseu/anim-editors`) plutôt que de les dupliquer dans
les deux applications — la divergence entre les deux outils sur ces trois
composants serait un bug, exactement comme pour le design system de base.

---

### A.13 `<GreffonManager>`
```ts
interface GreffonManagerProps {
  records: NkGreffonRecord[];
  filter: "all" | "active" | "disabled" | "rejected";
  onToggle: (id: string, next: boolean) => void;
  onUninstall: (id: string) => void;          // ouvre <GreffonUninstallDialog>
  onInstall: () => void;                      // ouvre <GreffonInstallDialog>
}
```
Une carte `Rejected` rend `rejectCause` **en toutes lettres**, pas le code. Un
`frameCostUs` au-dessus du seuil se rend en `--status-invalid`, pas en gris.

### A.14 `<GreffonInstallDialog>`
```ts
interface GreffonInstallDialogProps {
  manifest: NkGreffonManifest;
  newPermissions?: NkGreffonPermission[];  // mise a jour : seulement les AJOUTS
  acknowledged: boolean;                   // la case a cocher
  onAcknowledge: (v: boolean) => void;
  onConfirm: () => void;                   // desactive tant que !acknowledged
}
```
⚠️ **Chaque permission se rend avec sa phrase en langage ordinaire**, pas son
identifiant. `Network` ne dit rien ; « il pourra envoyer et recevoir des données
sur Internet » dit ce qui se passe.

⚠️ **La phrase « un greffon exécute du code dans l'éditeur » se place au-dessus des
boutons, à la taille des libellés** (doc 3 §20bis.5). Le composant ne doit pas
l'exposer comme un texte secondaire : c'est l'énoncé du coût.

### A.15 `<GreffonUninstallDialog>`
```ts
interface DependentDoc { path: string; elementCount: number }
interface GreffonUninstallDialogProps {
  record: NkGreffonRecord;
  dependents: DependentDoc[];      // CALCULE, jamais suppose vide
  acknowledged: boolean;
  onDisableInstead: () => void;    // chemin reversible, presente comme un vrai choix
  onConfirm: () => void;
}
```
⚠️ **`dependents` se calcule avant d'ouvrir la boîte.** Une liste vide par défaut,
remplie ensuite, laisserait une fenêtre de temps où l'on confirme sans voir la
conséquence.

⚠️ **`onDisableInstead` n'est pas un lien secondaire** : c'est l'option sûre et
réversible, rendue avec son propre bouton.

### A.16 `<ConsolePanel>`
```ts
interface ConsolePanelProps {
  tab: NkConsoleTab;
  validation: ValidationEntry[];   // clic = selectionne l'element fautif
  simulation: SimLogEntry[];       // doublures marquees, doc 3 18bis.2
  greffons: NkGreffonRecord[];
  system: NkSystemInfo;
  onCopySystemInfo: () => void;    // une seule action
}
```
⚠️ **`system.backend` est unique.** Si `backend != backendAsked`, le panneau
affiche le repli **avant** le reste : c'est l'information qui explique tout le
reste de la session.

---

