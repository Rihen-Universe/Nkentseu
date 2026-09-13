# NKGraph — Roadmap (substrat de graphe de nodes UNIQUE)

> Décision d'architecture validée par Rihen le 2026-07-09. **Un seul système de
> graphe de nodes** pour tout l'écosystème : matériaux (NKRenderer Phase T.2),
> VFX (Noge), Blueprint/Scratch (NKCode), modélisation procédurale (Kernel/AI),
> anim graphs / state machines (NkAnima M2), futur rig graph.
>
> **Le cœur (couche 1) est livré et prouvé** depuis le 31/07/2026 : modèle,
> évaluation, sous-graphes, sérialisation, annuler/refaire. Il ne dépend ni de
> NKCanvas ni de NKRenderer — c'est sa raison d'être. Restent le widget canevas
> (couche 2, NKEditorKit) et les consommateurs.

## Architecture en 3 couches (la séparation est la règle n°1)

```
Couche 3 — SÉMANTIQUE MÉTIER (chez chaque consommateur)
   bibliothèques de nodes + backends d'exécution :
   compilateur NkSL (matériaux) · plan d'exécution particules (VFX) ·
   VM/codegen script (Blueprint) · commandes NkMeshEditCommand (procédural)
        ▲
Couche 2 — ÉDITION (Engine/NKEditorKit)
   widget canvas node-based : pan/zoom, fils, sélection, recherche de nodes,
   groupes/commentaires, preview — UX apprise UNE fois, partagée partout
        ▲
Couche 1 — CŒUR (ce module : Kernel/Runtime/NKGraph)
   modèle de données pur : NkGraph, NkGraphNode, sockets TYPÉS, connexions,
   validation de types, sous-graphes, tri topologique / ordre d'évaluation,
   sérialisation .nkgraph, undo/redo (commandes inversibles)
```

**On unifie l'AUTORAT (modèle + sérialisation + UI), jamais l'EXÉCUTION.** Les
besoins d'exécution sont incompatibles : les matériaux se **compilent** vers NkSL
à l'édition (au runtime il n'y a plus de graphe), le VFX **évalue par frame** un
plan aplati (boucle chaude, jamais d'interprétation naïve), le Blueprint
**interprète** (VM) ou génère du C++. Le cœur fournit « l'ordre d'évaluation et
les types » ; chaque domaine fournit « ses nodes et ce qu'il en fait ».

## Garde-fous (là où ce genre de décision échoue)

1. **Cœur 100% agnostique** : aucun type métier (« texture », « bone », « son »)
   dans NKGraph. Les types de sockets sont enregistrés par les consommateurs via
   des type-ids (NKReflection). Un `if (nodeType == ...)` métier dans le cœur =
   architecture morte.
2. **Règle des deux consommateurs** : le cœur se construit AVEC son premier
   client réel, et c'est le **deuxième** client qui force la généralisation de
   l'API. Pas d'abstraction dans le vide. Premier client = **celui qui démarre
   en premier** : la Phase 4 « Graph » de NKCode est marquée « PROCHAIN » chez
   l'agent NKCode (note de coordination posée dans son ROADMAP le 2026-07-09) —
   sinon le graphe de matériaux (périmètre fermé, backend NkSL existant).
3. **Précédents internes** : même mouvement que NkGizmo3D (extrait de Demo3D →
   NKRenderer/Core) et NKEditorKit (extrait pour NKCode → partagé). Précédents
   externes : Unreal (EdGraph unique pour Blueprint/matériaux/anim), Blender
   (un système de nodes : shader/geometry/compositor) ; contre-exemple Godot
   (implémentations séparées, VisualScript abandonné).

## Synthèse

| Brique | Statut | Notes |
|--------|--------|-------|
| P1 — Modèle de données (nodes/sockets typés/connexions/validation) | ✅ | `src/NKGraph/NkNodeGraph.h/.inl`, **en-tête pur** (testable sans lier de cible, comme `NkShortcutTable`), zero-STL. **Nommé `NkNodeGraph` et non `NkGraph` : `nkentseu::NkGraph<V,Alloc>` existe déjà dans NKContainers** (graphe pondéré générique, DFS/BFS, 877 l.). Conversions implicites **dirigées** et déclarées par le consommateur, jamais devinées. Une entrée n'accepte qu'une source (la 2ᵉ remplace, comme Blender/Unreal). Supprimer un nœud emporte ses liens. Identifiants **jamais recyclés**. |
| P2 — Évaluation (tri topologique, sous-graphes, plan aplati) | ✅ | tri topologique + **refus du cycle à la connexion** (avec sa raison) ; **sous-graphes** et **plan aplati** dans `NkGraphDocument.h/.inl`. Un sous-graphe est une brique **nommée**, définie une fois et **instanciée** N fois (groupes de nœuds de Blender, graphes repliés d'Unreal) — corriger le groupe corrige les N instances. Après aplatissement, les nœuds d'instance et de frontière **ont disparu** et chaque entrée pointe sur l'étape réelle, même à trois groupes de distance. Récursion directe **et indirecte** refusée en la nommant. |
| P3 — Sérialisation `.nkgraph` + undo/redo | ✅ | `NkNodeGraphIO.inl`. Format **texte**, une directive par ligne : un graphe se relit, se compare avec `git diff` et se répare à la main ; le binaire ferait gagner des octets sur des fichiers de quelques Ko. **Écart assumé** : annuler/refaire par **instantanés sérialisés**, pas par commandes inversibles — l'inverse de « supprimer un nœud » doit restaurer le nœud, tous ses liens **et** leurs identifiants, et c'est le genre d'inverse qu'on écrit presque juste, dont l'erreur ne se voit que trois manipulations plus tard. L'instantané est correct par construction. Coût : mémoire ∝ taille × profondeur ; négligeable à cette échelle, et l'API publique ne changera pas si un jour il faut basculer. |
| P4 — Widget canvas (NKEditorKit) | ❌ | pan/zoom, fils, recherche, groupes, preview |
| P5 — 1er consommateur : NKCode Phase 4 (Blueprint) OU matériaux T.2 | ⏳ | **Démarré le 2026-08-21 : c'est le graphe de MATÉRIAUX.** Couche 3 dans `Kernel/Runtime/NKRenderer/src/NKRenderer/Materials/Graph/NkMatGraphTypes.h` (en-tête pur, comme le cœur), preuve dans `Applications/NkMatGraphCheck` — **application console**, 12 cas, **7 mutations vérifiées rouges**. Livré à ce stade : les 4 types de prises (réel/vecteur/couleur/shader) avec conversions **dirigées** (réel→couleur oui, couleur→réel non — luminance ? moyenne ? canal rouge ? trois réponses plausibles, donc aucune par défaut), les prototypes Principled/Diffuse/Emission/Mix Shader/Material Output, et la validation **de domaine** (zéro sortie, sorties multiples, sortie non reliée) — que le cœur ne peut pas faire, et c'est voulu. Reste : le compilateur → NkSL. |
| P6 — 2e consommateur (l'autre des deux, ou VFX) | ❌ | force la généralisation de l'API |

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Abandonné

## ✅ Les deux dettes du cœur sont PAYéES (2026-08-22) — valeurs + validation à la relecture

Trouvées par le premier consommateur réel le 21/08, arbitrées dans la nuit,
livrées le 22/08. Preuve : `Applications/NkMatGraphCheck`, **26 cas**, **11
mutations vérifiées rouges** ; les **169 cas** de `NKEditMeshHarness` (dont ses
19 cas `graphe/`) sont **inchangés**, et son `io-aller-retour` pèse toujours
**246 octets** — preuve directe qu'une valeur jamais renseignée n'écrit **rien**.

### 1. Un nœud porte des valeurs — et il y en a **deux sortes**

| | où | ce que c'est |
|---|---|---|
| `NkSocket::defaultValue` | sur la **prise** | la valeur d'une entrée **non connectée** — Base Color, Roughness. Le compilateur en a besoin exactement quand un lien manque |
| `NkNode::props` | sur le **nœud** | ce qui n'est **pas** une entrée — l'opération d'un Math, les arrêts d'un ColorRamp, le chemin d'une image |

**Pourquoi deux et pas un seul sac** : un défaut de prise rangé dans un sac de
nœud ne se retrouve plus que par **convention de nommage**
(« defaut_base_color »), et une convention de nommage finit toujours par être
violée. Ici le lien entre la prise et sa valeur est **structurel**. Blender fait
exactement cette séparation.

**Le garde-fou n°1 tient** : une valeur est un triplet `(nom, NkTypeId, charge
utile)` où le `NkTypeId` vient **du registre de types de prises** que les
consommateurs enregistrent déjà. La charge utile est volontairement pauvre — N
réels et un texte — pour que le cœur puisse lire, écrire, comparer et sérialiser
**sans jamais savoir ce que la valeur signifie**.

⚠️ **« Jamais renseigné » et « renseigné à vide » sont deux états distincts**, et
c'est le piège que l'agent NkUIDesign a payé la même nuit : un écrivain qui
réémet une forme mémorisée écrit du vide quand la valeur n'a jamais été
renseignée, **et sans erreur**. Ici « jamais renseigné » a **une seule**
représentation (`type == NK_TYPE_INVALID`) et **n'écrit aucune ligne**.

⚠️ **Les réels des valeurs s'écrivent en `%.9g`, pas en `%.6f`.** `%.6f` rend
0.333333 pour un tiers — un **autre** flottant : l'égalité exacte échouerait sur
un aller-retour pourtant correct. Les positions `x, y` gardent `%.6f` : dette
antérieure, sans conséquence mesurée (un pixel de canevas ne se compare pas au
bit près).

### 2. La validation était une précondition d'un côté et **rien du tout** de l'autre

`Connect()` refusait cycle, type et sens — l'invalide était **impossible à
construire par l'API**, ce dont un éditeur a pourtant besoin. `Deserialize()`
faisait `mLinks.PushBack` **sans aucun contrôle** — l'invalide entrait librement
par le **fichier**, et l'échec ne sortait qu'à `TopoSort`, plus tard et ailleurs.

`NkNodeGraph::Validate()` est désormais une **passe** qui rend un **diagnostic
désigné** (nœud, lien, nom de prise) et les liste **tous** — réparer un fichier
en le rechargeant dix fois pour découvrir dix défauts n'est pas un format, c'est
un supplice. Neuf genres : lien vers un nœud absent · index de prise hors bornes
· sens invalide · type incompatible · **deux sources sur une même entrée** · cycle
· type de prise inconnu · **défaut dont le type diffère de sa prise** · propriété
au type inconnu.

**Le sens visé est tenu : l'invalide est REPRÉSENTABLE *et* DÉTECTÉ.** Un fichier
cyclique se charge **avec ses deux liens** — un `Deserialize` qui « réparerait »
en refusant le second perdrait silencieusement une donnée de l'utilisateur.

⚠️ **Deux décisions de lecture qui ne se devinent pas**, chacune avec son cas :
- un `def` dont l'index de prise est **hors bornes est ABANDONNÉ, jamais rabattu**
  sur la dernière prise ni sur zéro. Rabattre poserait la valeur sur une prise
  **qui n'est pas la sienne** : le graphe paraîtrait sain et calculerait autre
  chose. Une valeur perdue finit par se voir ; une valeur **déplacée**, non.
- le **compte de réels est borné à la lecture** (4096), pas après. Il vient du
  fichier : un fichier corrompu annonçant quatre milliards de réels tuerait le
  processus **avant** que la validation n'ait la parole.

### 3. ⚠️ `.nkgraph` est un format de plus — la question se posera, elle n'est pas traitée

L'agent NkUIDesign refait en ce moment la fondation de sérialisation sur
**NKReflection + NKSerialization**, sur consigne de Rodolf. `.nkgraph` reste
délibérément à côté pour l'instant — coupler les deux chantiers maintenant
coûterait plus qu'il ne rapporte. **Mais la question devra être posée**, et c'est
écrit ici pour que personne ne découvre le troisième format dans trois mois. Ce
qui plaide pour le maintien du texte : un graphe se relit, se compare avec
`git diff` et se répare à la main. Ce qui plaide pour la bascule : un seul
lecteur à maintenir, et les valeurs typées que NKReflection sait déjà décrire.

### 4. Ce que le premier consommateur a aussi appris, et qui reste vrai

**Un seul harnais ne suffit pas à prouver un cœur.** Les deux défauts ci-dessus
vivaient sous 19 cas verts. Ils sont sortis au **premier usage réel**, pas à la
relecture. Le corollaire vaut pour les consommateurs suivants (VFX, Blueprint,
rig) : attendez-vous à en trouver, et **rapportez plutôt que de contourner**.

## ⚠️ Les deux dettes, telles qu'elles ont été trouvées le 2026-08-21 (archive)

Le cœur était prouvé par un seul harnais. Le premier usage véritable en a sorti
deux choses qu'un harnais unique ne pouvait pas révéler. Elles sont **posées ici,
pas corrigées** : elles touchent la couche 1, et l'arbitrage est demandé dans
`echanges/nkrenderer.questions.md` (Q14).

**1. Un nœud n'a AUCUN endroit où ranger un paramètre, et un socket aucune valeur
par défaut.** `NkNode` porte `id / type / label / subgraph / x / y / sockets /
alive` ; `NkSocket` porte `name / type / dir` ; et `.nkgraph` n'a que les
directives `type`, `conv`, `noeud`, `sock`, `lien`. Or c'est **la moitié d'un
nœud de matériau** : Base Color = blanc et Roughness = 0,5 sur les entrées non
connectées, les arrêts d'un ColorRamp, l'opération d'un nœud Math, le chemin
d'une Image Texture. Le raisonnement qui tranche est déjà écrit dans
`NkNodeGraph.h`, à propos de la position `x, y` : *« si le modèle ne la porte
pas, elle finira dans un fichier à côté — donc désynchronisée. »* Mot pour mot le
cas des paramètres. Un sac de propriétés générique n'est **pas** un type métier :
c'est de la grammaire de graphe, au même titre que `x`, `y` et `label`.

**2. La validation est une précondition d'un côté et rien du tout de l'autre.**
`Connect()` refuse le cycle, le type incompatible et le mauvais sens — donc
l'état invalide est **impossible à construire par l'API**. Mais `Deserialize()`
fait `mLinks.PushBack(l)` **directement** (`NkNodeGraphIO.inl`, branche `lien`) :
aucun contrôle. Un `.nkgraph` retouché à la main ou corrompu charge donc un
graphe cyclique sans une plainte, et l'échec ne sort qu'à `TopoSort`, plus tard
et ailleurs. **Aucun des 16 cas `graphe/` n'exerce ce chemin** : ils testent
l'aller-retour d'un graphe *valide*. Ce n'est pas forcément un défaut — un
éditeur a besoin que l'invalide soit représentable — mais les deux portes ne
disent pas la même chose, et personne ne l'a écrit ni testé.

**Preuve (31/07/2026)** — 17 cas dans `Applications/NKEditMeshHarness` (141 cas au
total, les 124 antérieurs inchangés). Ils sont choisis pour qu'une implantation
fausse **échoue**, pas pour confirmer ce qui marche :

| cas | pourquoi il discrimine | résultat |
|---|---|---|
| `topo-losange-insere-inverse` | les nœuds sont créés **D,C,B,A**, l'inverse de l'ordre attendu — un tri qui renverrait l'ordre d'insertion échouerait | `A>C>B>D` |
| `cycle-refuse` | vérifie aussi que le lien **n'a pas été posé** et que le graphe reste triable — un refus qui laisserait le lien donnerait le même code d'erreur | `cycle`, liens=2, triable |
| `conversion-dirigee` | testée dans les **deux sens** : une table symétrique par erreur passerait un test à sens unique | `réel>vect=ok`, `vect>réel=refusé` |
| `suppression-milieu` | porte sur le nœud **du milieu** : une extrémité ne montrerait pas l'oubli d'un sens | 2 liens → 0 |
| `entree-source-unique` | vérifie que la source restante est la **nouvelle** — garder l'ancienne donnerait le même compte | `y-la-nouvelle` |
| `sens-et-sockets` | « ce socket n'existe pas » ≠ « vous branchez une entrée sur une entrée » : l'interface doit pouvoir l'expliquer | 4 codes distincts |
| `identifiants-stables` | un id recyclé ferait pointer silencieusement une sauvegarde sur un autre nœud | pas de recyclage |
| `io-aller-retour` | compare les **textes caractère pour caractère**, pas des comptes : des libellés ou des conversions perdus laisseraient les comptes intacts | 246 o identiques |
| `io-identifiants-non-recycles` | **le piège du format** : sans la ligne `compteurs`, un aller-retour par ailleurs correct réattribuerait l'id du nœud supprimé | supprimé=2, nouveau=4 |
| `io-semantique-survit` | le graphe rechargé doit encore **accepter et refuser** comme avant — preuve que `conv` et les types ont été relus, pas seulement réécrits | `réel>vect=ok`, `réel>maillage=refusé` |
| `undo-restaure-les-liens` | **le piège de l'annulation** : la suppression du nœud du milieu tue 2 liens ; une annulation qui ne ressusciterait que le nœud laisserait un graphe coupé, d'apparence saine | nœud revenu, 2 liens, **mêmes ids** |
| `undo-branche-abandonnee` | remonter toute la pile doit redonner l'état initial **au texte près** — les comptes laisseraient passer une dérive de position ou de libellé | retour exact, refaisable 2→0 |
| `aplati-frontiere-disparait` | ne compte pas les étapes : vérifie que l'entrée du puits pointe sur **B**, de l'autre côté d'une frontière — c'est ce raccordement qui prouve l'aplatissement | `[src>A>B>dst]`, `dst<-B` |
| `aplati-deux-instances` | **le cas qui tranche** : une implantation qui mémoïserait par *graphe* n'émettrait le groupe qu'une fois et les deux branches liraient la même valeur | 8 étapes, `A` émis **2×**, sources distinctes |
| `aplati-recursion-refusee` | l'**indirecte** est le vrai cas : une garde qui ne comparerait qu'au graphe courant laisserait passer `G→H→G` et ferait déborder la pile | directe, indirecte et sous-graphe inconnu, tous nommés |
| `aplati-entree-libre` | **le piège de la sentinelle** : `0` est l'index d'une étape valide ; une sentinelle à zéro ferait lire la sortie de la 1ʳᵉ étape à chaque entrée libre | `sans-source` |
| `doc-aller-retour` | le champ `subgraph` est ce qui se perd le plus facilement ; on compare les textes **puis** on vérifie que le plan **reconstruit** est identique — une donnée peut survivre à l'écriture sans plus rien piloter | texte identique, même plan |

### Interface d'un groupe instancié — **corrigé le 31/07/2026**

Diagnostic initial faux : j'avais présenté le problème comme « les identifiants
de type sont propres à chaque graphe » et annoncé un registre partagé. Le
registre partagé n'aurait rien réglé — il aurait rendu la comparaison moins
coûteuse, sans rien **refuser**. Ce qui manquait était le **contrôle**.

`BuildPlan` valide désormais l'interface de chaque instance avant de la
développer : mêmes noms, mêmes types, ni socket manquant ni socket en trop. La
comparaison porte sur les **noms de type**, jamais sur leurs identifiants —
chaque graphe tenant son propre registre, le numéro 1 peut désigner « flux » ici
et « maillage » là.

| cas | pourquoi il discrimine | résultat |
|---|---|---|
| conforme | sans lui, un contrôle qui refuserait **tout** passerait les trois autres et ne prouverait rien | `ok` |
| nom absent | le groupe attend `e`, l'instance offre `entree` | refusé, socket nommé |
| **même identifiant, autre nom** | **le cas qui tranche** : `maillage` est enregistré en premier dans la racine, il porte donc l'id 1 — exactement celui de `flux` dans le groupe. Les numéros coïncident, les noms non. Un contrôle par identifiant les croirait d'accord | `type different sur e : groupe=flux instance=maillage` |
| socket en trop | un fil branché sur rien ; sans ce contrôle une entrée se retrouve **silencieusement débranchée** après modification du groupe, et le calcul continue avec une valeur manquante | refusé, socket nommé |

Le message porte le **chemin d'instanciation** (`racine/inst : …`) : un code
d'erreur seul obligerait à chercher dans un document qui peut compter des
dizaines de groupes.

### Instanciation multiple vs récursion — ce qui est permis, ce qui ne l'est pas

- **Instancier N fois le même groupe** : ✅ permis, prouvé (`aplati-deux-instances`).
- **Un groupe qui se contient lui-même** (`G→G` ou `G→H→G`) : ❌ refusé.

Ce n'est pas une limitation arbitraire : un graphe de **flux de données** est
aplati en un plan **fini** avant évaluation, et un groupe qui se contient
n'a pas de développement fini. Blender, Houdini et les graphes de matériaux
d'Unreal refusent tous le même cas.

La **récursion de fonction** est autre chose : elle suppose un graphe
d'**exécution** avec une pile d'appels à l'exécution (les fonctions Blueprint
d'Unreal la permettent, pour cette raison). Si le blueprint de modélisation en a
besoin, la réponse sera un **nœud d'appel** distinct du nœud d'instance — pas un
assouplissement de l'aplatissement. À décider avec le premier consommateur.

## ✅ Les trois décisions des 23-24/08 — et ce qu'elles FERMENT

> Elles viennent de Rodolf, elles se tiennent, et elles se lisent ensemble : la
> deuxième est la raison technique de la première, et la troisième dit vers quoi
> tout ça compile. Les deux premières sont **codées et mesurées** ; la troisième
> est **écrite et rien d'autre** — c'est délibéré, et dit plus bas pourquoi.

### 1. ✅ L'acyclicité est **UNIVERSELLE** — pas d'exception pour l'exécution

> **Un cycle vit à l'intérieur d'un nœud, jamais dans le graphe.**

⚠️ **Ce module a porté le contraire pendant une journée**, et la trace reste
écrite dans le code plutôt qu'effacée. Le 23/08 au matin, la spécification du
chantier design proposait que `WouldCreateCycle` et `TopoSort` ne voient plus que
les liens de famille `Data`, pour rendre un rebouclage d'exécution traçable. Ça a
été codé. ❌ **Retiré le 23/08 au soir** : ça affaiblissait une règle **générale**
pour un cas **particulier**.

**Trois raisons, et la première est celle qui décide :**

1. ⚠️ **Une règle sans exception est une règle que les outils n'ont pas à
   interroger.** Le jour où l'acyclicité dépend de la famille du lien, **tout** ce
   qui parcourt un graphe doit savoir dans quelle famille il se trouve — et ce
   chantier venait de passer trois jours sur des défauts causés par des choses qui
   **ne savaient pas à quelle famille elles appartenaient** ;
2. **le tri topologique reste valide sur le graphe ENTIER** : autoriser un cycle
   *quelque part*, c'est perdre l'ordre défini *partout* ;
3. **tous les cas connus sont couverts sans fil qui revienne.** Le `For Loop`
   d'Unreal a une sortie *corps* et une sortie *terminé*, et le corps ne revient
   **jamais** au nœud par un fil : le nœud itère lui-même.

| ce que ça change dans le code | où |
|---|---|
| `WouldCreateCycle` suit **tous** les liens vivants | `NkNodeGraph.inl` |
| `Connect` appelle la garde **sans condition de famille** | `NkNodeGraph.inl` |
| `TopoSort` compte **tous** les liens vivants | `NkNodeGraph.inl` |
| `NkSocketFamily` ne commande plus que **trois** comportements (compatibilité, arité d'entrée, arité de sortie) | `NkNodeGraph.h` |
| `LinkFamily()` n'a plus **aucun appelant dans le cœur** — elle sert la **vue**, qui ne dessine pas un fil d'exécution comme un fil de donnée | `NkNodeGraph.h/.inl` |

📌 **Et un gain qu'on n'attendait pas.** L'exemption s'écrivait à **deux**
endroits — la condition dans `Connect` **et** le filtre dans le parcours. Deux
encodages de la même règle, donc **invisibles à une mutation à un seul défaut** :
M36 survivait, et il avait fallu un **couple** (M38) pour mesurer ce que chacun
achetait. Les deux sont partis ensemble ; **M36 et M38seul rougissent désormais
seules**. Une règle sans exception se mesure aussi plus simplement.

**Ce que ça coûte, et il faut l'écrire :** `Portail (aller à)` — un saut **en
arrière** — n'existe pas sous cette décision. Les trois usages qu'on en attendait
sont couverts autrement (nœud de boucle · relais/`reroute` · fil d'exécution
ordinaire pour un saut **en avant**).

### 2. ✅ La machine à états est un **NŒUD** — la voie d'Unreal, pas celle d'Unity

**Et l'argument décisif est technique**, pas esthétique : ses cycles vivent **à
l'intérieur** du nœud, dans la liste de transitions que le runtime porte déjà
(`AddTransition(from, to, paramName, kind, threshold, fadeDur)`). **Le graphe
extérieur ne voit jamais de cycle, donc `WouldCycle` n'est pas touché.** C'est le
seul choix qui n'affaiblit pas une règle générale pour un cas particulier — et
c'est ce qui rend la décision 1 tenable.

🔴 **Le cœur n'a besoin de RIEN de neuf pour la porter**, et c'est une mesure,
pas une opinion : le cas `etats/la-machine-a-etats-est-un-noeud` de
`NkMatGraphCheck` n'appelle **aucune API nouvelle**. Le § 19.3 de la
spécification concluait « le mode états est **impossible** sur le cœur
d'aujourd'hui » ; **c'était faux**.

- la machine est **DÉSIGNÉE** par une **propriété de nœud** — le mécanisme qui
  porte déjà l'opération d'un Math et le chemin d'une image ;
- ⚠️ **pas dans `NkNode::subgraph`**, et la tentation est forte parce que le champ
  existe et porte déjà une référence par nom. Mais `subgraph` veut dire *« un
  `NkNodeGraph` de ce document »*, et **une machine à états n'en est pas un** :
  c'est une liste d'états et de transitions, avec son propre format. Deux sens
  pour un champ obligeraient tout lecteur à savoir lequel s'applique — exactement
  le défaut que la famille de prise vient de corriger ailleurs. Le cas vérifie que
  `subgraph` reste **vide** sur ce nœud ;
- **contre-épreuve**, sans laquelle le cas serait une tautologie : les **mêmes
  états câblés dehors** — `Marche → Saut → Marche` — tombent sur `WouldCycle` dès
  la transition de retour.

### 3. 📝 La forme **non nodale** d'un blueprint sera un **MODULE DE BYTECODE** — décidé le 24/08, **rien à construire maintenant**

C'est la réponse à *« vers quoi compile un graphe d'exécution ? »*, et elle ferme
la question au lieu de la laisser ouverte sous chaque chantier qui la croisera.
**À la manière d'Unreal**, dont le Blueprint compile vers un bytecode que la VM
du moteur exécute.

📌 **Pourquoi c'est cohérent avec tout le reste :** *le nodal vient **par-dessus**
le non nodal.* Le graphe **compile vers** la représentation non nodale, il ne la
remplace pas. Le graphe de matériaux l'applique déjà — c'est pour ça qu'il a
**refusé** les types composés : `NkMaterial` n'a nulle part où les écrire, et
*« ce n'est pas le graphe qui manque de types, c'est ce vers quoi il COMPILE »*.
La machine à états l'applique aussi (§ 2 ci-dessus) : le nœud **désigne** un
modèle non nodal qui existe déjà. Le blueprint suit la même loi.

🔴 **LA CONTRAINTE QUI DÉFINIT LA DÉCISION, et c'est elle qu'il faudra tenir :**

> **Le bytecode doit pouvoir s'écrire, se lire et se sérialiser SANS QU'AUCUN
> GRAPHE N'EXISTE.**

C'est le même critère que celui appliqué partout ici : si la forme non nodale
dépend du graphe pour exister, ce n'est pas une forme non nodale, c'est un cache
du graphe — et le jour où quelqu'un voudra produire un module autrement (import,
génération, écriture à la main, un autre langage de surface), il découvrira qu'il
ne peut pas.

⚠️ **RIEN N'EST À CONSTRUIRE AUJOURD'HUI, et c'est délibéré** : ni Noge, ni
NKScena, ni NKAnee ne sont fonctionnels. Écrire le bytecode maintenant, ce serait
l'écrire **sans consommateur** — exactement ce que le garde-fou n°2 de ce module
interdit (« le cœur se construit AVEC son premier client réel »). La décision est
donc **écrite ici et nulle part ailleurs**, pour qu'elle n'ait pas à être reprise
depuis zéro à chaque fois que la question remonte.

## Consommateurs prévus (état de leur côté)

- **NKRenderer Phase T.2** — graphe de matériaux (extension des templates
  existants, compat ascendante `.nkasset`). Cf. `Kernel/Runtime/NKRenderer/ROADMAP.md`.
- **Noge VFX** — graphe d'effets (émetteurs/forces/rendu). Cf. `Engine/Noge/ROADMAP.md`.
  ⚠️ `Engine/Noge/src/Noge/ECS/VisualScript/NkBlueprint.h` (header de spec 696
  lignes, aucun .cpp) devra être **réaligné sur NKGraph** au moment de son
  implémentation — ne pas l'implémenter en silo. ⚠️ Son
  `NkPinPrimitiveType { Exec, Float, … }` mélange **la famille et le type** :
  c'est le piège que le cœur a écarté (`NkSocketFamily` est un **axe séparé**),
  et il est mesuré par `exec/le-piege-du-type-exec`. À réaligner, pas à recopier.
  ⚠️ Sa forme **non nodale** est décidée : un **module de bytecode** (section
  ci-dessus) — **rien à construire aujourd'hui**.
- **NKCode** — Blueprint + Scratch (`src/NKCode/{Graph,Blueprint,Blocks}/` =
  README seulement à ce jour) : NKCode devient un **client** de NKGraph comme les
  autres. ⚠️ NKCode est tenu par un autre agent — coordination nécessaire avant
  d'y implémenter quoi que ce soit.
- **Kernel/AI** — graphe de modélisation procédurale (pilotable par prompt).
  Cf. `Kernel/AI/ROADMAP.md`.
- **NkAnima M2** — anim graph / state machine (HFSM) éditables. Cf.
  `Kernel/Runtime/NKAnima/ROADMAP_PRODUIT.md`. ✅ **Le mur annoncé n'existe pas** : la
  machine à états est **un nœud** du graphe d'animation (décision du 23/08,
  section ci-dessus), ses cycles vivent dans son modèle interne, et **le cœur n'a
  besoin de rien de neuf** pour la porter — mesuré par
  `etats/la-machine-a-etats-est-un-noeud`.

## Dépendances

- Cœur : NKCore, NKMemory, NKContainers, NKSerialization, NKReflection (type-ids
  de sockets). **Rien d'autre** — pas de NKRHI, pas de NKRenderer, pas d'UI.
- Widget (couche 2) : NKEditorKit (qui a ses propres deps UI).
- Pas de `.jenga` tant que P1 n'est pas démarré (module doc-only pour l'instant).
