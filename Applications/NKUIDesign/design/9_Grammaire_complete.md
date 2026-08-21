# Document 9 — Grammaire `.nkgui` complétée (PROPOSITION)

> Écrit le **2026-08-21**, avant d'écrire le parseur, parce qu'un parseur se
> construit sur une grammaire arrêtée et que quatre décisions de Rodolf n'étaient
> pas encore descendues dans le document 2.
>
> ⚠️ **CE DOCUMENT PROPOSE, IL NE REMPLACE RIEN.** Le
> `2_NkUIDesign_Langage_Description_NodeBlueprint.md` reste la référence et
> appartient à Rodolf. Ce qui suit est écrit en EBNF, dans le style de son §3,
> pour qu'un report se fasse par copie et non par réécriture.
>
> **Ce qui est repris de décisions déjà prises** est marqué ✅ avec sa source.
> **Ce qui reste à trancher** est marqué 🔴 et n'est **pas** implémenté dans le
> parseur — il attend un arbitrage.

---

## 0. Ce que ce document ajoute, et ce qu'il ne touche pas

| § | ajout | décision source |
|---|---|---|
| 2 | rôles `Text` et `Spacer` | ✅ doc 3 §14ter.3, tranché par Rodolf le 2026-08-21 |
| 3 | apparence portée par le widget, en **surcharges** | ✅ doc 3 §14ter.3, issue **(a)** |
| 4 | section `animation`, trois familles | ✅ doc 3 §9ter |
| 5 | déclaration de **police** | ✅ doc 3 §14quater bis |
| 6 | **quatre manques du document 2** relevés en écrivant le parseur | 🔴 arbitrage requis |

**Rien d'autre ne bouge** : le lexique (§2 du doc 2), la section `widgets`, la
section `behavior` (script et graphe) et les contrats `controller`/`callback`
restent mot pour mot ce qu'ils sont.

---

## 1. La règle d'écriture de ce document

⚠️ **Aucune des quatre décisions n'exige de nouvelle forme de VALEUR.** C'était le
premier piège : ajouter un type littéral (une liste, un pourcentage, une durée)
aurait obligé à retoucher `value`, donc le lexeur, donc tout ce qui lit déjà du
`.nkgui`.

Tout ce qui suit s'écrit avec **ce qui existe déjà** — des blocs nommés, des
`Identifier '=' value`, et l'imbrication. C'est ce qui rend cette proposition
rapportable dans le document 2 sans rien casser.

> **Un format se prolonge par ses blocs, jamais par ses littéraux.** Un bloc de
> plus, c'est une règle de plus dans le parseur ; un littéral de plus, c'est une
> ambiguïté de plus dans le lexeur, et elle se paie partout.

---

## 2. Les rôles `Text` et `Spacer` ✅

### 2.1 La grammaire ne change pas

`Kind := Identifier` (doc 2 §3) accepte déjà `Text` et `Spacer`. **Ce qui manque
n'est pas de la syntaxe, c'est une ligne dans la table §8** — et c'est cette table
qui fait la validation statique de §4 (« le compilateur rejette une propriété
absente du schéma du rôle »).

### 2.2 Ce qui s'ajoute à la table §8

| Rôle | Propriétés | Notes |
|---|---|---|
| `Text` | `text`, `wrap`, `align`, `maxLines`, `overflow`, `for` | ne porte ni état ni événement (doc 3 §14ter.3) |
| `Spacer` | `size` | déclare une **intention** de vide |

Domaines des propriétés énumérées, tels que doc 3 §14ter.3 les fixe :

```ebnf
(* valeurs admises, en Identifier — pas de nouveau type de litteral *)
text_align    := "start" | "center" | "end" | "justify"
text_overflow := "ellipsis" | "clip" | "visible"
```

`wrap` est un booléen (`true` | `false`, déjà dans `literal` du §3).
`maxLines` est un `Number` entier. `for` est un `String` : l'id d'un autre nœud.

### 2.3 Ce qui n'entre PAS

`Label` — doublon de la propriété `label` que `Button`, `MenuItem`,
`CollapsingHeader`, `TreeNode` et `Selectable` portent déjà (doc 3 §14ter.3).

### 2.4 Exemple

```nkgui
widgets {
    VBox "formulaire" {
        Text "etiquette_email" {
            text = "Adresse e-mail"
            align = start
            maxLines = 1
            overflow = ellipsis
            for = "champ_email"
        }
        InputText "champ_email" {
            bind = email
        }
        Spacer "respiration" {
            size = 24
        }
    }
}
```

---

## 3. L'apparence, portée par le widget, en SURCHARGES ✅

> Issue **(a)** de doc 3 §14ter.3 : *« un bloc de style optionnel par nœud ; le
> thème donne les défauts »*.

### 3.1 Grammaire

```ebnf
(* ---- Widgets : un membre de plus, et un seul ---- *)
node_decl     := Kind String? '{' node_member* '}'
node_member   := prop_decl | event_decl | appearance_blk | node_decl

(* ---- Apparence ---- *)
appearance_blk := "appearance" ('(' state_ref ')')? '{' appearance_member* '}'
state_ref      := Identifier
appearance_member := prop_decl | effect_blk
effect_blk     := effect_kind String? '{' prop_decl* '}'
effect_kind    := "fill" | "stroke" | "shadow" | "blur"
```

⚠️ **`appearance` est un MEMBRE du nœud, pas une section du fichier.** Le mettre
au niveau du fichier aurait demandé de désigner le nœud visé par un chemin — donc
deux endroits à tenir synchronisés pour un même objet, ce que doc 3 §14ter.3
écarte explicitement pour l'issue (c).

⚠️ **`shadow` se répète, il n'y a pas de littéral de liste.** §8ter demande une
**pile** d'ombres réordonnable ; l'ordre d'écriture des blocs `shadow` **est**
l'ordre de la pile. C'est ce qui permet d'exprimer un empilement sans toucher à
`value` (cf. §1, et le manque relevé en §6.1).

### 3.2 Les propriétés d'apparence

Reprises de doc 3 §12.2 (sections **6. Apparence**, **7. Typographie**) et §8ter
(**8. Effets**). Les noms sont en anglais `camelCase`, règle **R1** du document 7.

| bloc | propriétés |
|---|---|
| `appearance` | `opacity`, `radius`, `blend` |
| `fill` | `color`, `gradient`, `from`, `to`, `angle`, `image`, `fit` |
| `stroke` | `color`, `width`, `position`, `dash` |
| `shadow` | `offset` *(Vec2)*, `blur`, `spread`, `color`, `inner` *(Bool)* |
| `blur` | `radius`, `backdrop` *(Bool)* |
| typographie *(dans `appearance` directement)* | `font`, `weight`, `size`, `lineHeight`, `textAlign` |

🔴 **À trancher — les noms des ÉTATS.** `appearance(Hover)` a besoin d'une liste
fermée. Doc 3 §12.2 les nomme en français : *repos, survol, pressé, désactivé,
focus*. Le format est en anglais (doc 7 **R1**). La transposition évidente est
`Idle` · `Hover` · `Pressed` · `Disabled` · `Focus`, **mais personne ne l'a
écrite**, et un nom d'état est un nom que les utilisateurs apprendront. *Non
implémenté tant que ce n'est pas dit* : le parseur accepte n'importe quel
`Identifier` en `state_ref` et ne valide pas la liste.

### 3.3 ⚠️ Le garde-fou, et il fait partie de la décision

Doc 3 §14ter.3 : *« la validation compte les surcharges. Un document où presque
chaque élément surcharge son apparence a abandonné son thème sans le dire. »*

Deux classes de diagnostic s'ajoutent donc à la table §12 du document 2 :

| Code | Signification | Sévérité |
|---|---|---|
| `W-THEME-ABANDONNE` | la proportion de nœuds portant un `appearance` dépasse le seuil | Avertissement |
| `I-SURCHARGE` | ce nœud surcharge une valeur que le thème donnait déjà, à l'identique | Info |

**`I-SURCHARGE` n'est pas du zèle** : une surcharge qui recopie le défaut est
invisible tant que le thème ne change pas, et le jour où il change, elle est le
seul élément qui ne suit pas — sans que rien n'explique pourquoi.

### 3.4 Exemple

```nkgui
widgets {
    Button "valider" {
        label = "Valider"

        appearance {
            radius = 6
            opacity = 1.0
            font = "Inter"
            weight = 600
            size = 14
            fill { color = #2F6F7A }
            shadow { offset = (0, 2), blur = 6, color = #0000003A }
        }
        appearance(Hover) {
            fill { color = #3A8894 }
        }
    }
}
```

---

## 4. La section `animation` ✅

> Doc 3 §9ter : trois familles, et **une machine à états n'en couvre qu'une**.

### 4.1 Grammaire

```ebnf
section       := geometry_sec | widgets_sec | behavior_sec | contract_sec
               | animation_sec | font_sec

animation_sec := "animation" String? '{' anim_decl* '}'
anim_decl     := transition_decl | ambience_decl | continuous_decl

(* ---- Famille A : la transition, declenchee par un evenement ---- *)
transition_decl := "transition" String '{' target_ref? anim_prop* track_blk* '}'

(* ---- Famille B : l'ambiance, qui ne se declenche pas ---- *)
ambience_decl   := "ambience" String '{' target_ref? anim_prop* track_blk* '}'

(* ---- Famille C : l'effet continu, pilote par une entree ---- *)
continuous_decl := "continuous" String '{' target_ref? map_blk+ anim_prop* '}'
map_blk         := "map" '{' prop_decl* '}'

target_ref    := "target" '=' String
anim_prop     := Identifier '=' value
track_blk     := "track" String '{' key_stmt* '}'
key_stmt      := "key" Number '->' value ("," "curve" '=' Identifier)?
```

### 4.2 Ce que chaque famille porte, et pourquoi

| famille | mot-clé | propriétés | source |
|---|---|---|---|
| A. transition | `transition` | `on` *(l'événement)*, `from`, `to` *(états)*, `duration`, `curve`, `delay` | §9bis, §9ter |
| B. ambiance | `ambience` | `state` ⚠️, `duration`, `repeat`, `direction`, `delay`, `jitter` | §9ter.1 |
| C. continu | `continuous` | (voir `map`) `rest` ⚠️, `smoothing` | §9ter.2 |
| toutes | — | `reduceMotion` | §9ter.4 |

Le bloc `map` de la famille C, propriété par propriété :
`source`, `sourceFrom`, `sourceTo`, `property`, `targetFrom`, `targetTo`, `curve`.

⚠️ **`state` sur une ambiance est OBLIGATOIRE, ce n'est pas un réglage.** §9ter.1 :
*« une ambiance appartient à un ÉTAT, jamais à l'élément en général »*. Sans lui,
on obtient le bouton qui respire pendant qu'on appuie dessus, et personne ne sait
d'où vient le tremblement. **Une ambiance sans `state` doit être une erreur, pas
un défaut appliqué en silence.**

⚠️ **`rest` sur un effet continu est OBLIGATOIRE pour la même raison.** §9ter.2 :
sans valeur de repos déclarée, l'élément *« reste figé dans la dernière position
qu'il avait »* — sur un écran tactile, où il n'y a pas de pointeur, une carte
inclinée pour toujours.

`reduceMotion := "stop" | "shorten" | "keep"` — §9ter.4. Défaut proposé :
`stop` pour `ambience`, `shorten` pour `transition`, `stop` pour `continuous`
(qui reprend alors sa valeur de repos). `keep` est réservé à ce qui porte de
l'information.

### 4.3 Ce que la section n'invente pas

Les propriétés animables **ne sont pas énumérées**, et c'est délibéré : §9ter.3
dit qu'il n'y a *« pas de catalogue fermé d'effets »*. `track "opacity"`,
`track "scale"`, `track "shadow.blur"` sont des **chemins de propriété**, résolus
contre le nœud visé — pas des mots-clés du langage.

🔴 **À trancher — la forme du chemin de propriété.** `"shadow.blur"` suppose qu'on
sait désigner la propriété d'un effet empilé. Avec deux ombres, laquelle ?
`"shadow[1].blur"` demanderait un indice, donc une syntaxe de chemin. Nommer les
blocs d'effet (`shadow "portee" { … }` — le `String?` de `effect_blk` en §3.1)
évite l'indice, mais **la décision de rendre ce nom obligatoire dès qu'on anime
un effet appartient à Rodolf**. *Non implémenté.*

### 4.4 Exemple — les trois familles, sur le même bouton

```nkgui
animation "bouton_valider" {

    transition "appui" {
        target = "valider"
        on = Click
        duration = 0.12
        curve = EaseOut
        reduceMotion = shorten
        track "scale" {
            key 0.0 -> 1.0
            key 1.0 -> 0.96, curve = EaseOut
        }
    }

    ambience "respiration" {
        target = "valider"
        state = Idle
        duration = 2.4
        repeat = 0
        direction = alternate
        jitter = 0.3
        reduceMotion = stop
        track "scale" {
            key 0.0 -> 1.0
            key 1.0 -> 1.02
        }
    }

    continuous "reflet" {
        target = "valider"
        rest = 0.0
        smoothing = 0.18
        reduceMotion = stop
        map {
            source = PointerX
            sourceFrom = -1.0
            sourceTo = 1.0
            property = "fill.angle"
            targetFrom = -6.0
            targetTo = 6.0
            curve = Linear
        }
    }
}
```

---

## 5. La déclaration de police ✅

> Doc 3 §14quater bis : *« le moteur sait le faire, le format ne sait pas le
> dire »*. Et : **une police qui manque donne une MISE EN PAGE fausse**, pas une
> couleur fausse.

### 5.1 Grammaire

```ebnf
font_sec      := "fonts" '{' font_decl* '}'
font_decl     := "font" String '{' font_member* '}'
font_member   := prop_decl | source_blk | fallback_blk | metrics_blk
source_blk    := "source" '{' prop_decl* '}'
fallback_blk  := "fallback" '{' fallback_item* '}'
fallback_item := "family" '=' String
metrics_blk   := "metrics" '{' metric_stmt* '}'
metric_stmt   := "glyph" String '->' Number
               | Identifier '=' Number
```

### 5.2 Les propriétés

| bloc | propriétés | pourquoi |
|---|---|---|
| `font` | `family`, `weight`, `style`, `kind` | `kind := "text" \| "icons"` |
| `source` | `mode`, `path`, `data` | `mode := "embedded" \| "referenced"` |
| `fallback` | suite ordonnée de `family` | §14quater bis, « la chaîne de polices à essayer, dans l'ordre » |
| `metrics` | `lineHeight`, `ascent`, `descent`, `unitsPerEm`, et des `glyph "A" -> 712` | **l'empreinte** |

⚠️ **`kind` n'est pas une étiquette de confort.** §14quater bis : une police
d'icônes substituée *« ne devient pas une autre icône, elle devient un carré
vide »*. Replier une police d'icônes sur une police de texte ne produit **jamais**
rien de lisible — la chaîne de repli doit donc savoir qu'elle a affaire à des
icônes et **refuser** au lieu de replier.

⚠️ **`mode = embedded` engage une LICENCE.** §14quater bis : beaucoup de polices
commerciales interdisent la redistribution, y compris incluse dans un fichier.
L'outil doit le dire au moment où l'on coche « embarquer », pas au moment où l'on
diffuse.

⚠️ **`metrics` est la pièce que personne ne pense à mettre**, et c'est la seule qui
rend la substitution détectable : *« le document s'ouvre, il est valide, il est
faux »*. À l'ouverture, l'outil recalcule les largeurs des glyphes de référence et
compare.

Deux classes de diagnostic s'ajoutent à la table §12 :

| Code | Signification | Sévérité |
|---|---|---|
| `W-FONT-SUBSTITUEE` | l'empreinte relue ne correspond pas à l'empreinte déclarée | Avertissement |
| `E-FONT-ICONES-REPLIEE` | une police `kind = icons` a été repliée sur une police de texte | Bloquante |

### 5.3 Exemple

```nkgui
fonts {
    font "titre" {
        family = "Inter"
        weight = 600
        style = normal
        kind = text
        source {
            mode = embedded
            path = "Fonts/Inter-SemiBold.ttf"
        }
        fallback {
            family = "Noto Sans"
            family = "DejaVu Sans"
        }
        metrics {
            unitsPerEm = 2048
            lineHeight = 1.21
            ascent = 1984
            descent = -494
            glyph "A" -> 1366
            glyph "M" -> 1774
            glyph "i" -> 569
        }
    }

    font "icones" {
        family = "Rihen Icons"
        kind = icons
        source {
            mode = embedded
            path = "Fonts/RihenIcons.ttf"
        }
    }
}
```

---

## 6. 🔴 Quatre manques du document 2, relevés en écrivant le parseur

> Ce ne sont pas des propositions. Ce sont des endroits où **la grammaire écrite
> ne suffit pas à lire les exemples que le document 2 donne lui-même**. Ils sont
> listés ici parce qu'un parseur qui les comblerait sans le dire inventerait le
> format à la place de Rodolf.

### 6.1 Il n'existe aucun littéral de LISTE, et cinq rôles en demandent un

`value := String | Number | Color | Vec2 | flags | Identifier` (§3).

La table §8 donne pourtant `columns[]` à `Table`, `items[]` à `Combo`,
`tabs[]` et `enabled[]` à `TabBar`, `values[]` à `PlotLines`, `sizes[]` aux
conteneurs. **Aucun de ces cinq rôles n'est écrivable aujourd'hui.**

Trois issues, et il faut en choisir une :

| | |
|---|---|
| **a.** un littéral de liste | `items = ["Rouge", "Vert", "Bleu"]` — une ligne dans `value`, et le lexeur n'a rien à apprendre |
| **b.** des nœuds enfants | `Combo { Item "Rouge" {} Item "Vert" {} }` — cohérent avec `Item` du document 7 §3.3, et chaque entrée peut alors porter ses propres propriétés |
| **c.** une propriété répétée | `item = "Rouge"` trois fois — aucune syntaxe nouvelle, mais l'ordre devient implicite et rien n'empêche un doublon |

*Recommandation, et elle n'engage que moi : **(b)**.* Une entrée de liste
déroulante finira toujours par vouloir une icône, un état désactivé, une
infobulle — et (a) comme (c) obligeraient alors à tout réécrire. **(b) est le
seul des trois qui ne se paie pas une deuxième fois.**

⚠️ **Non implémenté.** Le parseur écrit aujourd'hui refuse `[` et le dit
(`E-PARSE`). Il ne devine pas.

### 6.2 `expr` ne peut pas lire les exemples du document 2

§6.3 écrit `node n2 Multiply { a = n1.value, b = 100 }`.
§4.1 écrit `Callback "…"(Enum.X, value)`.

Or `expr := literal | Identifier | expr op expr | '(' expr ')'` — **`n1.value` et
`Enum.X` n'y sont pas.** Le `pin_ref := Identifier '.' Identifier` existe, mais
seulement dans `wire_stmt`, pas dans `expr`.

*Ce qui a été fait dans le parseur, et il faut le savoir* : un `Identifier '.'
Identifier` est lu comme **un seul identifiant pointé**, réémis tel quel. C'est le
minimum pour que les deux exemples du document 2 soient lisibles — **ce n'est pas
une décision de format**, et la question de savoir si un chemin peut avoir trois
segments (`a.b.c`) reste ouverte.

### 6.3 `include` est déclaré, `Theme.nkgui` ne l'est pas

§7 donne `include "Theme.nkgui"`. **La grammaire d'un fichier de thème n'existe
nulle part** (relevé aussi par doc 3 §14ter.3). Un `include` qui résout vers un
fichier dont la forme n'est pas définie ne se vérifie pas.

Avec l'issue (a) de l'apparence — le thème donne les défauts — cette grammaire
devient nécessaire, et **elle n'est pas dans ce document** : c'est un cinquième
ajout, pas un des quatre demandés. `NkTheme` (NKEditorKit, 302 lignes) porte déjà
des **rôles de couleur nommés avec héritage et chargement** (doc 8 §3) — c'est de
là qu'il faut partir, pas d'une page blanche.

### 6.4 La validation par rôle n'a pas de table à interroger

§4 : *« le compilateur rejette une propriété absente du schéma du rôle »*. Le
schéma, c'est la table §8 — **et le document 7 propose précisément de la
remplacer** (`SliderFloat` → `Slider`, `Combo` → `Dropdown`, …), question 1 de son
§7, non tranchée.

**Conséquence assumée, et écrite avec le résultat** : le parseur livré fait la
validation **syntaxique** (`E-PARSE`) et **pas** la validation par rôle
(`E-TYPE`). Il accepte n'importe quel `Kind` et n'importe quelle propriété.

⚠️ **C'est ce qui lui permet de lire les dix documents du corpus**, qui sont déjà
écrits dans le vocabulaire du document 7 (`Text`, `TextField`, `Dropdown`,
`Progress`, `Item`) et qu'une validation contre la table §8 rejetterait en bloc.
*Le dire vaut mieux que de laisser croire à une validation qui n'a pas lieu.*

---

## 7. Récapitulatif — la grammaire complète proposée

Les seuls ajouts au §3 du document 2, rassemblés :

```ebnf
section       := geometry_sec | widgets_sec | behavior_sec | contract_sec
               | animation_sec | font_sec

node_decl     := Kind String? '{' node_member* '}'
node_member   := prop_decl | event_decl | appearance_blk | node_decl

appearance_blk    := "appearance" ('(' state_ref ')')? '{' appearance_member* '}'
state_ref         := Identifier
appearance_member := prop_decl | effect_blk
effect_blk        := effect_kind String? '{' prop_decl* '}'
effect_kind       := "fill" | "stroke" | "shadow" | "blur"

animation_sec   := "animation" String? '{' anim_decl* '}'
anim_decl       := transition_decl | ambience_decl | continuous_decl
transition_decl := "transition" String '{' target_ref? anim_prop* track_blk* '}'
ambience_decl   := "ambience"   String '{' target_ref? anim_prop* track_blk* '}'
continuous_decl := "continuous" String '{' target_ref? map_blk+ anim_prop* '}'
target_ref      := "target" '=' String
anim_prop       := Identifier '=' value
map_blk         := "map" '{' prop_decl* '}'
track_blk       := "track" String '{' key_stmt* '}'
key_stmt        := "key" Number '->' value ("," "curve" '=' Identifier)?

font_sec      := "fonts" '{' font_decl* '}'
font_decl     := "font" String '{' font_member* '}'
font_member   := prop_decl | source_blk | fallback_blk | metrics_blk
source_blk    := "source"   '{' prop_decl* '}'
fallback_blk  := "fallback" '{' fallback_item* '}'
fallback_item := "family" '=' String
metrics_blk   := "metrics"  '{' metric_stmt* '}'
metric_stmt   := "glyph" String '->' Number | Identifier '=' Number
```

**Mots-clés réservés ajoutés** : `appearance`, `fill`, `stroke`, `shadow`,
`blur`, `animation`, `transition`, `ambience`, `continuous`, `track`, `key`,
`map`, `target`, `fonts`, `font`, `source`, `fallback`, `metrics`, `glyph`,
`family`.

⚠️ **Réserver ces mots a un coût qu'il faut dire** : `source`, `family`, `target`,
`key` sont des noms de propriété plausibles ailleurs. Le parseur ne les traite
comme mots-clés **que là où la grammaire les attend** (au début d'un membre de
bloc), jamais dans `prop_decl` — un `source = "x"` reste une propriété ordinaire.
*Sans cette règle, le premier document qui nomme une propriété `target` cesse de
se lire.*

---

## 8. État d'implémentation — **mis à jour le 2026-08-21, format `nkgui 0.3`**

> **Rodolf a tranché le 2026-08-21.** Ses cinq décisions sont descendues dans le
> code ; le format passe de **0.2 à 0.3**. Le document 2 n'a toujours **pas** été
> touché : il lui appartient, et ce tableau dit exactement ce qu'il aurait à y
> reporter.

| § | proposé | implémenté dans le lecteur/écrivain v0.3 |
|---|---|---|
| 2 — `Text`, `Spacer` | ✅ | ✅ rôles au catalogue de validation (`NkGuiValidate.h`) |
| 3 — apparence | ✅ | ✅ `appearance` / `appearance(État)` / `fill·stroke·shadow·blur`, forme en ligne conservée |
| 4 — animation | ✅ | ✅ `transition` · `ambience` · `continuous`, avec `track` / `key` / `map` |
| 5 — polices | ✅ | ✅ `fonts` / `font` / `source` / `fallback` / `metrics` + `glyph "A" -> 1366` |
| 6.1 — littéral de liste | ✅ **tranché** | ✅ **et les dictionnaires aussi** : `[a, b]` et `{ clé = v }`, imbriqués |
| 6.2 — identifiant pointé | ✅ **tranché** | ✅ `n1.value`, `Enum.X`, `a.b.c` — en argument, en pin, en expression ; `NkGSplitPath` pour qui doit résoudre |
| 6.3 — grammaire du thème | 🔴 | ❌ **toujours ouvert** — `include` est lu, sa cible n'est pas résolue |
| 6.4 — validation par rôle | ✅ **tranché** | ✅ `NkGuiValidate.h`, **contre le vocabulaire du document 7**, alias compris |

### 8.1 Les deux choix de la validation, et ils ne sont pas neutres

**Le vocabulaire de référence est celui du document 7, pas la table §8 du
document 2.** Les dix documents du corpus sont écrits en `Text`, `TextField`,
`Dropdown`, `Item`, `Progress` ; valider contre la table §8 les rejetterait en
bloc — c'est-à-dire rejeter tout ce qui existe. Les anciens noms restent lus, en
**alias**, avec un avertissement `W-ROLE-ALIAS` qui nomme le nouveau.

**Un rôle inconnu produit une erreur nommée, jamais un rejet muet du fichier
entier.** C'est pour ça que la validation vit dans un fichier *séparé* du
lecteur : le document se lit d'abord, intégralement, et se juge ensuite. *Un
outil qui refuse d'ouvrir ce qu'il signale est celui qui empêche de le réparer.*

Codes émis : `E-ROLE-INCONNU`, `E-TYPE` (propriété hors schéma **ou** valeur d'un
type que le rôle n'attend pas), `W-ROLE-ALIAS`.

### 8.2 ⚠️ La contrainte qui prime : mettre à jour sans rien détruire

Rodolf, mot pour mot : *« rassure-toi que le système pourra facilement être mis à
jour sans tout détruire et sans bug. »* Quatre règles la tiennent, et elles sont
dans le code, pas seulement dans ce document :

| | règle | où elle vit |
|---|---|---|
| **a** | le fichier porte sa version, relue et **réémise telle quelle** | `NkGDocument::versionMajor/Minor` — un 0.2 se réécrit en 0.2 |
| **b** | un lecteur récent lit **tous** les fichiers anciens | rien de la v0.2 n'a été retiré ni resserré ; corpus 0.2 à 10/10 |
| **c** | un lecteur ancien **refuse clairement** un fichier plus récent | **majeure** supérieure → `E-VERSION-INCOMPATIBLE`, « ce fichier est trop récent pour moi » |
| **d** | ce qu'on ne comprend pas, on le **préserve tel quel** | **mineure** supérieure → section (et membre de widget) inconnus gardés en **texte brut**, réémis à l'octet près |

⚠️ **La frontière entre (c) et (d) est la MAJEURE, et ce n'est pas un détail de
numérotation.** Refuser aussi la mineure serait plus simple et plus faux : ça
transformerait le moindre ajout futur en rupture, et le format ne pourrait plus
jamais grandir sans casser les outils déjà déployés.

⚠️ **La règle (d) impose sa forme au code.** On ne peut pas préserver ce qu'on ne
sait pas modéliser en le faisant passer par le modèle : il faut garder **la
tranche de source**. C'est pour ça que le lexeur note l'offset de début et de fin
de chaque jeton et que le modèle porte un type `NkGRaw`.

⚠️ **Ce que la règle (d) exige du format, et il vaut mieux le dire maintenant** :
une construction future doit être un **bloc accoladé**. Une construction sans
bloc n'est pas délimitable sans connaître sa grammaire, donc pas préservable.
C'est une contrainte sur les versions à venir, pas une limite de cette
implémentation.

### 8.3 Ce qui a été retiré, et c'est un progrès

Les deux réglages « ligne vide après l'en-tête » et « ligne vide entre les
sections » de `NkGWriteOptions` **n'existent plus**. C'étaient des heuristiques :
on devinait l'intention de l'auteur à partir d'une seule observation. Les lignes
vides sont désormais **lues et conservées** comme les commentaires — il n'y a
plus rien à deviner, donc plus rien qui puisse se tromper.

### 8.4 La trivia, et pourquoi un découpage naïf suffit

Tout ce que le lexeur jette entre deux jetons est récupéré **verbatim**, découpé
en lignes, et rattaché soit à la fin de la ligne du jeton précédent, soit au
début de celle du suivant. Un commentaire de bloc sur plusieurs lignes est coupé
en autant de morceaux — et se recolle à l'identique à l'écriture, parce que
chaque morceau est réémis tel quel suivi d'un retour à la ligne. *Le modèle n'a
pas besoin de savoir que c'était un commentaire ; il a besoin de ne rien perdre.*

Seule l'indentation de la dernière ligne — celle qui précède immédiatement le
jeton — est délibérément abandonnée : c'est l'écrivain qui la régénère, sinon
deux règles se disputeraient la même colonne.

### Où il vit, et comment on le vérifie

| | |
|---|---|
| lecteur, écrivain, modèle | `src/NKUIDesign/NkGuiFormat.h` |
| validation rôles et types | `src/NKUIDesign/NkGuiValidate.h` |
| le banc d'aller-retour | `src/NKUIDesign/NkGuiRoundTrip.h` |
| `NKUIDesign --roundtrip=<dossier>` | l'aller-retour sur tous les `.nkgui` d'un dossier |
| `NKUIDesign --roundtrip-controles` | les témoins de bruit, contrôles positifs et négatifs |
| `NKUIDesign --valider=<dossier>` | la validation par rôle et par type |

**Mesure du 2026-08-21 (v0.3)**, sur les dix documents de
`D:/Projets/Camrail/AI/CorpusUI/sortie/nkgui` (270 à 3 015 nœuds, jusqu'à 13
niveaux d'imbrication) : **10/10 équivalents, 10/10 identiques octet pour octet**,
et **0 erreur / 0 avertissement** à la validation. Contrôles : **36/36**.

⚠️ **Le taux seul ne vaut rien sans les contrôles, et c'est pour ça qu'ils sont
livrés avec.** Une fonction de comparaison qui répondrait « égal » en toutes
circonstances donnerait exactement le même 10/10. Les contrôles 2a à 2j vérifient
qu'une valeur, un identifiant, **l'ordre des membres**, un lexème numérique
réécrit, une section en trop, l'ordre des drapeaux, **un commentaire manquant**,
**une ligne vide manquante**, un élément de liste et une clé de dictionnaire sont
bien **détectés** ; le contrôle 7 vérifie qu'une expression réémise garde sa
précédence — *une expression peut se réécrire juste et se calculer faux.*

⚠️ **Le contrôle 20b est le plus important du banc.** Un document **0.4 fictif**
contient une section inconnue *et* un membre de widget inconnu, commentaires
intérieurs compris : les deux doivent revenir à l'octet près. Le 20c vérifie
l'inverse — la même source estampillée **0.3** doit être **refusée**. Sans cette
paire, la préservation ne serait pas liée à la version : elle serait une
tolérance permanente, c'est-à-dire un trou.

⚠️ **La limite nommée le 2026-08-21 est levée** : les commentaires et les lignes
vides **survivent** à l'aller-retour, et la trivia **entre dans la comparaison
d'équivalence** — sans quoi un écrivain qui jetterait tous les commentaires
resterait « équivalent ».

### 8.5 Ce qui reste ouvert

1. **La grammaire du thème** (§6.3) — `include "Theme.nkgui"` se lit, sa cible ne
   se résout pas. `NkTheme` (NKEditorKit, 302 lignes) porte déjà des rôles de
   couleur nommés avec héritage : c'est de là qu'il faut partir.
2. **Les noms des états d'apparence** (§3.2) — le lecteur accepte n'importe quel
   identifiant en `appearance(X)`. `Idle · Hover · Pressed · Disabled · Focus`
   est la transposition évidente, mais personne ne l'a écrite.
3. **La forme du chemin de propriété animée** (§4.3) — `"shadow.blur"` suppose
   qu'on sait désigner la propriété d'un effet empilé. Nommer les blocs d'effet
   (`shadow "portee" { … }`) est **implémenté** et évite l'indice ; rendre ce nom
   obligatoire dès qu'on anime un effet reste une décision de Rodolf.
4. **`W-THEME-ABANDONNE` et `I-SURCHARGE`** (§3.3) — les deux diagnostics de
   garde-fou sur les surcharges d'apparence ne sont pas encore comptés.

---

*Voir aussi : document 2 (la référence), document 3 §9ter / §14ter.3 /
§14quater bis (les décisions), document 7 (le vocabulaire), document 8
(l'inventaire de l'existant).*
