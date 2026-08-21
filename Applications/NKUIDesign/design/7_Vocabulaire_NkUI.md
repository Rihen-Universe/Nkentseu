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

⚠️ **`modal` est une propriété de `Window`, pas un rôle** (§14ter.4). Et `Scroll`
devient un conteneur explicite au lieu d'un drapeau : *une zone défilante est un
objet dans une maquette, pas une option cachée d'un autre.*

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
