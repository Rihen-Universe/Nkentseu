// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
# Document 7 — Le vocabulaire NkUI

> Ouvert le **2026-08-21** à la demande de Rodolf : *« définis un vocabulaire pour
> que, quand on exporte, on traduise directement dans ce vocabulaire qui nous est
> propre. »*
>
> ⚠️ **Ce document PROPOSE.** Là où il s'écarte du
> `2_NkUIDesign_Langage_Description_NodeBlueprint.md`, c'est signalé. Rien n'est
> changé dans le document 2 sans l'accord de Rodolf.

---

## 1. Pourquoi un vocabulaire à nous, et pas les noms du moteur

Le document 2 §8 fait correspondre chaque rôle `.nkgui` à **une fonction de
l'API NKGui** : `Button` → `Button()`, `Combo` → `BeginCombo()`,
`ColorEdit4` → `ColorEdit4()`.

⚠️ **C'est un miroir, et un miroir couple le format au code.** Le jour où une
fonction du moteur est renommée, **tous les documents déjà écrits deviennent
faux** — alors qu'ils ne décrivent pas du code, ils décrivent une interface.

> **Un `.nkgui` doit s'ouvrir en 2030 même si le moteur a tout renommé en 2028.**
> Le vocabulaire est un **contrat**, pas un reflet.

Et cela ne vaut pas que pour la durée. Trois autres raisons :

- **l'export** — traduire vers une autre cible demande un point de départ stable ;
  partir des noms d'API, c'est traduire depuis un dialecte mouvant ;
- **l'IA de design** (document 6) apprend ce vocabulaire. S'il bouge, le corpus
  vieillit ;
- **NKGui lui-même le réclame** : sa raison d'être écrite est *« noms 100 %
  Nkentseu, zéro lien ImGui »*. Un format qui recopie ses signatures C++ ramène
  par la fenêtre ce que la réécriture a sorti par la porte.

---

## 2. Les cinq règles de nommage

**R1 — PascalCase anglais.** C'est déjà la convention du document 2 et de tout
l'écosystème. ⚠️ **On ne francise pas le format.** Le français est la langue de
l'**interface** (§14ter.3) ; l'anglais est celle du **fichier**. Deux registres,
une table de correspondance, aucune ambiguïté.

**R2 — Aucun `Begin`/`End` dans un nom.** L'imbrication s'exprime par
l'imbrication. `BeginCombo` → `Combo`. *(Déjà respecté par le document 2.)*

**R3 — ⚠️ Aucun type ni arité dans un nom.** `SliderFloat`, `DragInt`,
`ColorEdit4`, `CheckBox3` portent la signature C++ dans leur nom. **Un document
n'a pas de types C++** — il a des propriétés.

**R4 — ⚠️ Aucun suffixe de commodité.** `ButtonEx`, `InputTextEx`, `TabBarEx`,
`TreeNodeEditable`, `SelectableEditable` distinguent des surcharges d'API. Dans un
document, **une variante est une propriété**, pas un rôle.

**R5 — Un nom dit ce que la chose EST, pas ce que le moteur en fait.**
`DockSpaceOverViewport` décrit un appel ; `DockSpace` décrit un objet.

> **R3 et R4 sont les deux qui font vraiment le travail.** Elles font tomber le
> catalogue d'une trentaine de noms à une vingtaine, et **chaque nom retiré est un
> nom que personne n'aura à apprendre.**

---

## 3. Le vocabulaire

### 3.1 Actions

| vocabulaire | remplace | propriétés |
|---|---|---|
| `Button` | `Button`, `ButtonEx`, `RepeatButton` | `label`, `icon`, `flags`, **`repeat`** *(bool)*, `repeatDelay`, `repeatRate` |
| `ImageButton` | `ImageButton` | `image`, `size`, `tint` |
| `MenuItem` | `MenuItem` | `label`, `shortcut`, `checked` |

⚠️ `RepeatButton` disparaît : c'est `Button { repeat = true }`. Le document 2 le
comptait déjà comme une propriété (`repeatDelay`, `repeatRate`) **tout en gardant
un rôle séparé** — les deux ne peuvent pas être vrais.

### 3.2 Saisie

| vocabulaire | remplace | propriétés |
|---|---|---|
| `TextField` | `InputText`, `InputTextEx`, `InputTextMultiline` | `bind`, **`multiline`**, **`secret`**, `maxChars`, `wrap`, `placeholder`, `flags` |
| `NumberField` | `InputInt`, `InputFloat` | `bind`, **`valueType`** *(Int/Float)*, `step`, `min`, `max` |
| `Slider` | `SliderFloat` | `bind`, **`valueType`**, `min`, `max`, `step` |
| `Drag` | `DragFloat`, `DragInt` | `bind`, **`valueType`**, `speed`, `min`, `max`, `dir` |
| `ColorField` | `ColorEdit4`, `ColorPicker4`, `ColorButton` | `bind`, **`alpha`** *(bool)*, **`mode`** *(Button/Field/Picker)* |

⚠️ **`secret` sur `TextField` répond au manque relevé en §14ter.4** : NKGui n'a
pas de champ de mot de passe. Le vocabulaire, lui, sait le dire — et c'est au
moteur de suivre, pas au format de s'amputer. *Un format qui ne peut exprimer que
ce qui existe déjà ne peut jamais rien demander.*

### 3.3 Booléens et sélection

| vocabulaire | remplace | propriétés |
|---|---|---|
| `Checkbox` | `Checkbox`, `CheckboxTristate`, `CheckBox3` | `bind`, **`tristate`** *(bool)*, `label` |
| `Switch` | *(rien — n'existe pas dans NKGui)* | `bind`, `label` |
| `RadioGroup` | *(rien)* | `bind`, `options[]`, `orientation` |
| `Dropdown` | `Combo` | `bind`, `items[]`, `editable` |
| `ListBox` | `BeginListBox` | `bind`, `items[]`, **`multiSelect`** |
| `Item` | `Selectable`, `SelectableEditable`, `SelectItem` | `label`, `selected`, **`editable`** |
| `TreeItem` | `TreeNode`, `TreeNodeEditable` | `label`, `expanded`, **`editable`** |

⚠️ **`Switch` et `RadioGroup` sont dans le vocabulaire alors que le moteur ne les
porte pas** (§14ter.4). C'est délibéré, et c'est la conséquence directe de la
règle du §3.2 : **le vocabulaire décrit l'interface, pas l'état d'avancement du
moteur.** Un `RadioGroup` exporté vers un moteur qui n'a pas l'exclusivité doit
**échouer bruyamment**, pas être traduit en cases à cocher indépendantes.

### 3.4 Texte et affichage

| vocabulaire | remplace | propriétés |
|---|---|---|
| **`Text`** | *(absent du document 2 — §14ter.3)* | `text`, `wrap`, `align`, `maxLines`, `overflow`, `for` |
| `Image` | `Image` | `source`, `size`, `tint`, `uv0`, `uv1` |
| `Progress` | `ProgressBar` | `bind`, `overlay`, **`indeterminate`** |
| `Chart` | `PlotLines`, `PlotHistogram` | `values`, **`kind`** *(Line/Histogram)*, `min`, `max` |
| `Separator` | `Separator` | `orientation` |
| **`Spacer`** | *(absent — §14ter.3)* | `size` |

### 3.5 Navigation

| vocabulaire | remplace | propriétés |
|---|---|---|
| `TabBar` | `TabBar`, `TabBarEx`, `TabBarEditable` | `tabs[]`, `bind`, **`editable`**, **`closable`** |
| `MenuBar` | `BeginMenuBar` | — |
| `Menu` | `BeginMenu` | `label` |
| `ContextMenu` | `BeginPopupMenu` | — |
| `Expander` | `CollapsingHeader` | `label`, `expanded` |
| `DockSpace` | `DockSpace`, `DockSpaceOverViewport` | **`overViewport`** *(bool)*, `topMargin` |

### 3.6 Conteneurs

| vocabulaire | remplace | propriétés |
|---|---|---|
| `Window` | `Window` | `title`, `pos`, `size`, `flags`, **`modal`** |
| `Panel` | `Panel` | `title` |
| `Group` | `BeginGroup` | — |
| `VBox` · `HBox` | idem | `gap`, `align`, `justify` |
| `Grid` | `Grid` | `columns`, `gap`, `sizes[]` |
| `Flow` | `Flow` | `gap` |
| `Stack` | `Stack` | `anchor` |
| `Table` | `Table` | `columns[]`, `flags` |
| `Scroll` | *(drapeaux de `BeginChild`)* | `axis`, `always` |
| `Splitter` | `Splitter` | `bind`, `min`, `max`, `orientation` |

### 3.7 La zone hôte

| vocabulaire | remplace | propriétés |
|---|---|---|
| `Host` | *(rien : le concept manquait)* | `hint` |

⚠️ **`Host` n'est pas un widget, et c'est tout son intérêt.** Il déclare un **rectangle
que l'application remplit** : le viseur 3D de NK3DModeler, la toile de NKUIDesign,
l'éditeur de texte de NKCode. C'est la **frontière du format**, et elle se dit en une
phrase : *le document décrit OÙ et QUOI ; l'application garde QUAND et COMMENT.*

Sans ce rôle, un document ne peut décrire qu'un **écran de démonstration**, jamais une
application réelle — c'est ce que l'inventaire du 17/09 a mesuré.

⚠️ **Ce n'est PAS `Callback`.** Ce mot appartient déjà deux fois au format : `callback`
est l'une des huit sections (document 9), et `Callback "nom"(...)` est un **appel de
comportement** — `valides/05_animation_comportement.nkgui:29` l'emploie ainsi et valide à
0 erreur. Le monteur avait inscrit, de son côté, un rôle de widget du même nom : aucun
document ne l'employait, le validateur le refusait, et il a été **retiré** plutôt que
légalisé. *Un rôle qui porte le nom d'une section n'est pas un rôle, c'est un homonyme.*

⚠️ **Le NOM est revisable, et c'est le seul point de ce lot qui demande ton arbitrage.**
`Host` a été choisi parce que le dépôt dit déjà « l'application hôte », « les panneaux
hôtes ». Le renommer coûtera un alias (§5), jamais une réécriture.

**Une zone que personne ne remplit ne disparaît pas en silence** : le monteur y peint des
hachures et le nom de la zone, et la compte (`hotesNonRemplis`). Un manque muet se fait
prendre pour un fond ; un manque qui se voit se répare.

⚠️ **`modal` est une propriété de `Window`, pas un rôle** (§14ter.4). Et `Scroll`
devient un conteneur explicite au lieu d'un drapeau : *une zone défilante est un
objet dans une maquette, pas une option cachée d'un autre.*

---

### 3.8 Les tailles relatives — trois propriétés UNIVERSELLES

| propriété | type | sens |
|---|---|---|
| `sizeRel` | vecteur | fraction de la région du **parent**, par axe (`0.16` = 16 %) |
| `minSize` | vecteur | plancher, en pixels |
| `maxSize` | vecteur | plafond, en pixels |

⚠️ **Sans elles, un document ne décrit qu'une seule taille de fenêtre.** `pos` et `size`
sont en **pixels absolus** ; or la disposition réelle de NK3DModeler est écrite en
fractions — `NkLayout::Compute(W, H, fLeft = 0.16f, fRight = 0.29f)`. Un `.nkgui` qui
décrirait le modeleur avec `size` le **figerait**. C'est ce qui sépare un écran de
démonstration d'une **fenêtre d'éditeur**.

**Elles sont strictement ADDITIVES**, et c'est mesuré, pas espéré :
- `size` garde exactement son sens — il n'agit qu'avec `pos`, en pixels ;
- une composante `<= 0` veut dire « **cet axe n'est pas contraint** » : on peut ne dire que
  la largeur, `sizeRel = (0.16, 0)` ;
- si `sizeRel` est présent pour un axe, il gagne sur cet axe, puis `minSize`/`maxSize`
  bornent le résultat.

Preuve, au pixel, sur le **même** document (`valides/13_tailles_relatives.nkgui`) :

    a 1200 x 800 :  gauche 192 px (16 %)      droite 348 px (29 %)
    a  800 x 600 :  gauche 180 px (le PLANCHER mord : 128 -> 180)   droite 232 px
    negatif      :  un `pos`+`size` rend 200 px dans LES DEUX fenetres

⚠️ **Ce que ces propriétés ne font pas encore** : elles s'appliquent aux **conteneurs** et à
`Host`. Une **feuille en flux** garde sa taille naturelle, parce que `pos` reste
l'interrupteur du placement et que le changer ne serait plus additif. C'est une limite
nommée, pas un oubli.

### 3.9 L'ancrage — `DockSpace`

| rôle | ce qu'il dit | ce qu'il NE dit PAS |
|---|---|---|
| `DockSpace` | **quelles zones** existent, dans quel ordre, et leurs **proportions par défaut** | l'**arbre** d'ancrage, les onglets, ce que l'utilisateur a tiré |

⚠️ **La frontière, et elle vient d'une mesure, pas d'un goût.** La coquille écrit **déjà**
un arbre complet — `dockroot=`, `node=k|kind|vertical|ratio|c0|c1|activeTab`, `nwin=`,
`float=` — et elle le **relit** (`NkEditorShell::LoadUiState`). Si le document décrivait
lui aussi un arbre, il dirait une **seconde vérité**, et les deux divergeraient au premier
séparateur tiré. Donc :

- le **document** dit *quelles zones, leurs proportions par défaut* ;
- la **coquille** garde *l'arbre que l'utilisateur a obtenu*.

*Le document dit le défaut, le geste vit à côté* — la même frontière que pour le
séparateur, un cran plus haut.

⚠️ **Une zone se nomme par l'IDENTIFIANT du panneau**, jamais par son libellé affiché :
`hierarchie`, pas `Hiérarchie`. Un libellé se traduit, un identifiant non ; la mesure du
17/09 montre que renommer perdait la disposition en silence tant que la coquille adressait
les panneaux par leur titre.

**Le monteur ne résout aucun panneau** : il **demande** à l'hôte
(`NkGuiMonteHooks::ZoneAncree(nom, rect)`). NKGui ne sait pas quels panneaux une
application fournit, et une table ici en ferait une seconde autorité.

**Les deux cas dissymétriques, mesurés des deux côtés** :

| cas | réponse | où c'est mesuré |
|---|---|---|
| le document nomme une zone que **personne ne fournit** | elle **se signale** : cadre, hachures, **et son nom écrit** (c'est le nom que l'application doit servir) | `NKGuiMonteTest` (m12), mutation `NK_DOCK_MUTATION=muet` |
| l'application fournit un panneau que le **document ne nomme pas** | il reste **INTOUCHÉ** : le crochet n'est jamais appelé pour lui — le document n'a aucun pouvoir dessus | `NKGuiMonteTest` (m12) : 3 crochets pour 3 zones |
| la **disposition enregistrée** ne nomme pas un panneau | il est **FERMÉ** — `LoadUiState` ferme tout dès qu'une ligne `panel=` existe, puis rouvre les seuls nommés | `NKUIDesign --recette-identite`, section 6 |

⚠️ Les deux derniers sont des politiques **opposées** sur la même question, dans deux
formats différents. C'est écrit pour que personne ne transporte la réponse de l'un vers
l'autre.

**La largeur d'une zone** : sa fraction si elle en déclare une, sinon **le reste** partagé
entre celles qui n'en déclarent pas — pas « une part égale ». La règle « part égale »
laissait une **bande orpheline** de 197 px que onze critères verts n'avaient pas vue et que
la capture a montrée. Un dock qui laisse un trou n'est pas un dock.

    zones : hierarchie 180 px (0,16 clampe par minSize 180) | apercu 530 px (le RESTE) | zone_absente 290 px (0,29)
    couverture : 1000 / 1000

---

---

## 4. Ce qui n'entre PAS au vocabulaire

| | pourquoi |
|---|---|
| `Tooltip` | **propriété** de n'importe quel élément (§14quinquies.1) |
| glisser-déposer | **capacité** cochable (§14ter.4) |
| modalité | **propriété** de `Window` |
| dialogue de fichier natif | **appel système**, pas un widget (§14ter.6) |

> **Trois fois le même classement corrigé.** Une capacité transversale n'entre
> jamais au catalogue : sinon il faudrait `ButtonWithTooltip`, `FieldWithTooltip`,
> et le catalogue doublerait à chaque capacité ajoutée.

---

## 5. Versionnage — un nom ne disparaît jamais en silence

Le fichier porte déjà sa version (`nkgui 0.2`). On y ajoute une règle :

⚠️ **Un rôle renommé garde son ancien nom comme alias, avec la version où il a
changé.** Le lecteur accepte les deux ; le validateur signale l'ancien ; l'écrivain
n'émet que le nouveau.

**Sans alias, une renommée casse tous les documents existants — et personne ne
renomme jamais rien.** Un vocabulaire qu'on ne peut plus corriger se fige avec ses
erreurs.

Les alias de cette version : `RepeatButton` → `Button{repeat}` ·
`InputText`/`InputTextMultiline` → `TextField` · `SliderFloat` → `Slider` ·
`ColorEdit4`/`ColorPicker4`/`ColorButton` → `ColorField` · `Combo` → `Dropdown` ·
`CollapsingHeader` → `Expander` · `Selectable` → `Item` · `TreeNode` → `TreeItem` ·
`ProgressBar` → `Progress` · `PlotLines`/`PlotHistogram` → `Chart`.

---

## 6. Ce que l'export fait de ce vocabulaire

> **Le vocabulaire est le pivot. On n'exporte jamais depuis les noms du moteur.**

```
document .nkgui  ──►  vocabulaire NkUI  ──►  cible
                          (pivot)            NKGui / autre
```

⚠️ **Un pivot n'a d'intérêt que s'il est le SEUL chemin.** Si une cible peut
court-circuiter et lire directement les noms du moteur, elle finira par le faire —
et le pivot deviendra une couche que l'on contourne, donc une couche qui ment.

**Et chaque traduction déclare ce qu'elle ne sait pas rendre.** `RadioGroup` vers
un moteur sans exclusivité : **erreur nommée**, jamais une approximation
silencieuse. *Traduire en perdant sans le dire, c'est livrer une interface qui a
l'air juste et ne l'est pas.*

---

## 7. Ce qui reste à trancher

1. **Adopter ce vocabulaire dans le document 2 §8**, ou le garder comme couche
   au-dessus ? *(Ma recommandation : l'adopter — deux tables valent une
   divergence.)*
2. Les noms proposés — `TextField`, `Dropdown`, `Expander`, `Item`, `TreeItem`,
   `Chart` — conviennent-ils ? **Ce sont des noms que les utilisateurs
   apprendront ; les changer plus tard coûtera un alias chacun.**
3. `Switch` et `RadioGroup` entrent-ils **maintenant** au vocabulaire alors que le
   moteur ne les porte pas encore ?
