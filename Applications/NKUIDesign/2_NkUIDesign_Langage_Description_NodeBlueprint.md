# Langage de description `.nkgui` et système de Node Blueprint

Version 0.2 — spécification autonome, à charge du parseur/compilateur partagé
entre NkUIDesign (auteur) et le runtime NKGui (exécution).

---

## 1. Vue d'ensemble

Un fichier `.nkgui` contient jusqu'à quatre sections indépendantes :

| Section | Contenu | Optionnelle |
|---|---|---|
| `geometry` | Formes visuelles brutes (calques du canvas) | oui |
| `widgets` | Arbre sémantique de widgets NKGui | non (sauf fichier purement `geometry`) |
| `behavior` | Programmes de comportement (script **ou** node graph) | oui |
| `controller` / `callback` | Contrats de callbacks (signatures, sans implémentation) | oui |

Design (formes), Structure (widgets) et Comportement compilent vers un même
document en mémoire ; le langage n'impose pas de les séparer en fichiers,
mais `include` (§7) permet de le faire pour les gros projets.

---

## 2. Lexique

```
Identifier   := [A-Za-z_][A-Za-z0-9_]*
String       := '"' (caractère | '\"' | '\\' | '\n')* '"'
Number       := '-'? [0-9]+ ('.' [0-9]+)?
Color        := '#' HEX{6}  |  '#' HEX{8}      // RGB ou RGBA
Vec2         := '(' Number ',' Number ')'
LineComment  := '//' jusqu'à fin de ligne
BlockComment := '/*' ... '*/'
```

Sensible à la casse. Les identifiants de rôle/type suivent `NkPascalCase`
(cohérence avec le moteur). Les mots-clés (`widgets`, `behavior`, `on`,
`set`, `if`, `else`, `node`, `wire`, `callback`, `controller`, `include`)
sont réservés.

---

## 3. Grammaire (EBNF)

```ebnf
file          := "nkgui" version_lit include* section*
version_lit   := Number '.' Number
include       := "include" String

section       := geometry_sec | widgets_sec | behavior_sec | contract_sec

(* ---- Géométrie ---- *)
geometry_sec  := "geometry" '{' shape* '}'
shape         := "shape" String '{' shape_prop* '}'
shape_prop    := Identifier '=' value

(* ---- Widgets ---- *)
widgets_sec   := "widgets" '{' node_decl* '}'
node_decl     := Kind String? '{' (prop_decl | event_decl | node_decl)* '}'
Kind          := Identifier               (* "Window","Button","SliderFloat",... *)
prop_decl     := Identifier '=' value
value         := String | Number | Color | Vec2 | flags | Identifier
flags         := Identifier ('|' Identifier)*

(* ---- Événements & actions ---- *)
event_decl    := "on" Identifier ('(' param_ref (',' param_ref)* ')')? '->' action
param_ref     := Identifier
action        := callback_call | behavior_ref | inline_block
callback_call := "Callback" String '(' arg_list? ')'
arg_list      := expr (',' expr)*
behavior_ref  := "Behavior" String
inline_block  := '{' statement* '}'

(* ---- Behavior : script ---- *)
behavior_sec  := "behavior" String ("graph")? '{' body '}'
body          := statement* | graph_body
statement     := assign_stmt | if_stmt | call_stmt
assign_stmt   := "set" Identifier '=' expr
if_stmt       := "if" expr '{' statement* '}' ("else" '{' statement* '}')?
call_stmt     := callback_call
expr          := literal | Identifier | expr op expr | '(' expr ')'
op            := '+' | '-' | '*' | '/' | '>' | '<' | '>=' | '<=' | '==' | '&&' | '||'
literal       := String | Number | Color | "true" | "false"

(* ---- Behavior : node graph ---- *)
graph_body    := node_stmt* wire_stmt*
node_stmt     := "node" Identifier NodeType ('{' pin_init (',' pin_init)* '}')?
pin_init      := Identifier '=' expr
wire_stmt     := "wire" wire_chain
wire_chain    := pin_ref ('->' pin_ref)+
pin_ref       := Identifier '.' Identifier

(* ---- Contrats ---- *)
contract_sec  := controller_decl | "callback" callback_sig
controller_decl := "controller" String '{' callback_sig* '}'
callback_sig  := "callback" Identifier '(' param_list? ')' '->' Type
param_list    := param (',' param)*
param         := Identifier ':' Type
Type          := "Void" | "Bool" | "Int" | "Float" | "String" | "Color" | "Vec2"
               | "Enum" '[' Identifier (',' Identifier)* ']'
```

---

## 4. Section `widgets` — sémantique

- Chaque `node_decl` correspond à un rôle du catalogue widgets NKGui (table
  complète en §8). Le second argument (`String`) est l'**id/label**
  (identité stable, hashée FNV-1a côté moteur — cohérent avec `NkGuiId`).
- Les propriétés (`prop_decl`) sont typées **par rôle** : le compilateur
  rejette une propriété absente du schéma du rôle (validation statique).
- Les `flags` reprennent littéralement les valeurs des enums moteur
  (`NkGuiButtonFlags`, `NkGuiTableFlags`, `NkGuiWindowFlags`,
  `NkGuiColorFlags`, `NkGuiInputFlags`, `NkGuiDragDir`, `NkGuiCheck`),
  combinables par `|`.
- L'imbrication de `node_decl` reproduit l'imbrication Begin/End du moteur
  (un `Panel` contenant un `Button` ⇔ `BeginPanel(...) ... Button(...) ...
  EndPanel()`).

### 4.1 Exemple

```nkgui
nkgui 0.2

widgets {
    Window "Inspecteur" {
        pos = (40, 40)
        size = (320, 480)
        flags = Resizable | Closable

        Panel "Transform" {
            SliderFloat "X" {
                bind = x
                min = -100
                max = 100
                on Commit(value) -> Callback "TransformInspector.OnPositionChanged"(Enum.X, value)
            }
            Button "Reset" {
                on Click -> Callback "TransformInspector.OnResetClicked"()
            }
        }
    }
}
```

---

## 5. Section `behavior` — script

Sous-ensemble minimal, typé dynamiquement au niveau `expr`, sans boucle
générale (les itérations restent du ressort du C++ lié) — volontairement
limité à de la logique locale simple :

```nkgui
behavior "PreviewOpacity" {
    set opacityPreview = value * 100
    if value > 0.8 {
        Callback "WarnHighValue"()
    } else {
        set opacityPreview = value * 50
    }
}
```

`value` et toute autre variable non déclarée par `set` fait référence à un
**paramètre d'événement** (cf. `event_decl` : `on Changed(value) -> Behavior
"PreviewOpacity"` transmet `value` dans le contexte du programme).

---

## 6. Section `behavior ... graph` — Node Blueprint

### 6.1 Modèle

Un graphe est un ensemble de **nœuds** (`node_stmt`) reliés par des **fils**
(`wire_stmt`). Deux familles de pins :

- **Pins d'exécution** (`exec`, `true`, `false`, `then`...) : déterminent
  l'ordre d'exécution (flux, façon Blueprint — flèche blanche).
- **Pins de données** (typés `Bool|Int|Float|String|Color|Vec2|Enum`) :
  transportent une valeur, référencée par `nomNoeud.nomPin`.

### 6.2 Catalogue de nœuds standard

| Catégorie | Nœud | Pins entrée | Pins sortie |
|---|---|---|---|
| Événement | `EventClick`, `EventChanged`, `EventCommit`, `EventHover`, `EventSelect`, `EventOpen`, `EventClose`, … (1 par événement du catalogue §9) | — | `exec`, + données de l'événement (`value`, `text`, `color`…) |
| Flux | `Branch` | `exec`, `cond:Bool` | `true`, `false` |
| Flux | `Sequence` | `exec` | `then1..thenN` (ordonnés) |
| Flux | `ForEachChild` | `exec`, `parent:NodeRef` | `loopBody`, `completed` |
| Donnée | `GetVariable` | `name:String` | `value` |
| Donnée | `SetVariable` | `exec`, `name:String`, `value` | `exec` |
| Donnée | `GetWidgetValue` | `target:NodeRef` | `value` |
| Donnée | `SetWidgetProperty` | `exec`, `target:NodeRef`, `prop:String`, `value` | `exec` |
| Opérateur | `Add`, `Subtract`, `Multiply`, `Divide` | `a`, `b` | `result` |
| Opérateur | `Compare` | `a`, `op:String`, `b` | `result:Bool` |
| Opérateur | `And`, `Or`, `Not` | `a`,(`b`) | `result:Bool` |
| Action | `CallCallback` | `exec`, `name:String`, `args...` | `exec` |
| Action | `Log` | `exec`, `message:String` | `exec` |
| Commentaire | `Comment` | — (cosmétique éditeur uniquement) | — |

Ce catalogue est **extensible** (l'éditeur peut proposer des nœuds
personnalisés packagés par le projet), mais ces nœuds forment le socle
minimal que le compilateur/interpréteur doit toujours reconnaître.

### 6.3 Exemple

```nkgui
behavior "PreviewOpacity" graph {
    node n1 EventChanged
    node n2 Multiply     { a = n1.value, b = 100 }
    node n3 SetVariable  { name = "opacityPreview", value = n2.result }
    node n4 Compare      { a = n1.value, op = ">", b = 0.8 }
    node n5 Branch       { cond = n4.result }
    node n6 CallCallback { name = "WarnHighValue" }

    wire n1.exec -> n3.exec -> n5.exec
    wire n5.true -> n6.exec
}
```

### 6.4 Équivalence Code ⇔ Node Graph

Les deux fronts syntaxiques (§5 et §6) compilent vers la **même
représentation intermédiaire** (liste d'instructions typées : `Assign`,
`Branch`, `Call`, `BinaryOp`). L'éditeur peut donc convertir dans les deux
sens tant que le programme reste dans :
- pas de boucle arbitraire non bornée (seul `ForEachChild` est supporté) ;
- pas d'expression au-delà des opérateurs du §3.

Au-delà, la vue Node Graph reste la source de vérité (plus expressive) et la
vue Code devient une **lecture seule générée** (pretty-print), signalée comme
telle dans l'éditeur.

---

## 7. Modularité — `include`

```nkgui
nkgui 0.2
include "Theme.nkgui"
include "Widgets/Inspecteur.nkgui"
```

Fusionne les sections des fichiers inclus dans le document courant au
parsing (résolution de chemins relative au fichier courant, détection de
cycle = erreur).

---

## 8. Table de correspondance rôle `.nkgui` → API NKGui

| Rôle | Fonction(s) moteur rejouée(s) | Propriétés principales |
|---|---|---|
| `Window` | `SetNextWindowPos/Size`, `Begin`, `EndWindow` | `pos`, `size`, `flags` (`NkGuiWindowFlags`) |
| `Panel` | `BeginPanel`, `EndPanel` | `title` |
| `Button` | `Button` / `ButtonEx` | `label`, `flags` (`NkGuiButtonFlags`), `repeatDelay`, `repeatRate` |
| `Checkbox` / `CheckboxTristate` / `CheckBox3` | idem | `bind` |
| `SliderFloat` | `SliderFloat` | `bind`, `min`, `max` |
| `DragFloat` / `DragInt` | idem | `bind`, `speed`, `min`, `max`, `dir` (`NkGuiDragDir`) |
| `InputFloat` / `InputInt` | idem | `bind`, `step`, `min`, `max` |
| `InputText` / `InputTextEx` / `InputTextMultiline` | idem | `bind`, `flags` (`NkGuiInputFlags`), `maxChars`, `wrap` |
| `ColorButton` / `ColorPicker4` / `ColorEdit4` | idem | `bind`, `flags` (`NkGuiColorFlags`) |
| `Image` / `ImageButton` | idem | `texId`, `size`, `tint`, `uv0`, `uv1` |
| `ProgressBar` | `ProgressBar` | `bind`, `overlay` |
| `PlotLines` / `PlotHistogram` | idem | `values`, `min`, `max`, `height` |
| `Table` | `BeginTable`/`TableSetupColumn`/`TableHeadersRow`/`TableNextRow`/`TableNextColumn`/`EndTable` | `columns[]`, `flags` (`NkGuiTableFlags`) |
| `Combo` | `BeginCombo`/`Selectable`/`EndCombo` | `items[]`, `bind` |
| `Menu` / `MenuItem` | `BeginMenuBar`/`BeginMenu`/`MenuItem`/`EndMenu` | `label`, `shortcut` |
| `TreeNode` / `TreeNodeEditable` | idem | `label`, `editable` |
| `Selectable` / `SelectableEditable` / `SelectItem` | idem | `label`, `selected`, `editable` |
| `TabBar` / `TabBarEx` / `TabBarEditable` | idem | `tabs[]`, `enabled[]`, `editable` |
| `DockSpace` / `DockSpaceOverViewport` | idem | `topMargin` |
| `VBox` / `HBox` / `Grid` / `Flow` / `Row` / `Column` / `Stack` | conteneurs de layout correspondants | `gap`, `columns`, `sizes[]` |
| `Splitter` | `Splitter` | `bind`, `min`, `max`, `vertical` |
| `Separator` | `Separator` | — |
| `CollapsingHeader` | `CollapsingHeader` | `label` |
| `Tooltip` (modificateur, pas un nœud) | `SetTooltip` | `text` |

---

## 9. Catalogue des événements par rôle

Voir Document 1, tableau A.8 — repris tel quel comme référence du champ
`Identifier` autorisé après `on` dans `event_decl` et pour les nœuds
`EventXxx` du §6.2.

---

## 10. Contrats — `controller` / `callback`

```nkgui
controller "TransformInspector" {
    callback OnPositionChanged(axis: Enum[X,Y,Z], value: Float) -> Void
    callback OnResetClicked() -> Void
    callback OnTintChanged(color: Color) -> Void
}

callback WarnHighValue() -> Void
```

Règles :
- Une `callback_call` référencée dans `widgets`/`behavior` **doit** exister
  comme `callback_sig` déclarée quelque part dans le document (ou un fichier
  inclus) — sinon erreur de compilation « callback non déclaré » (à ne pas
  confondre avec « non lié en C++ », qui n'est qu'un avertissement runtime).
- Les types d'arguments passés dans `callback_call` doivent correspondre à la
  signature déclarée (vérification statique).

---

## 11. Système de typage (résumé)

| Type `.nkgui` | Équivalent C++ côté binding | Notes |
|---|---|---|
| `Bool` | `bool` | |
| `Int` | `int32` | |
| `Float` | `float32` | |
| `String` | `const char*` (NKMemory) | |
| `Color` | `math::NkColor` | littéral `#RRGGBB[AA]` |
| `Vec2` | `math::NkVec2` | littéral `(x, y)` |
| `Enum[...]` | `int32` (index) ou enum générée | liste de labels fixée à la déclaration |
| `Void` | — | callback sans valeur de retour utile au design (le retour réel, s'il existe, est ignoré côté design) |

---

## 12. Erreurs & validation (classes)

| Code | Signification | Sévérité |
|---|---|---|
| `E-PARSE` | Erreur de syntaxe | Bloquante (fichier invalide) |
| `E-TYPE` | Type de propriété/argument invalide pour le rôle | Bloquante |
| `E-CALLBACK-UNDECLARED` | `Callback "X"` référencé sans `callback_sig` correspondante | Bloquante |
| `W-CALLBACK-UNBOUND` | Callback déclaré mais jamais lié en C++ au chargement | Avertissement (no-op journalisé) |
| `W-ID-DUPLICATE` | Deux widgets partagent le même id/label dans le même scope | Avertissement |
| `W-ORPHAN-GEOMETRY` | Forme non promue, sans usage dans `widgets` | Info |

---

*Voir aussi : Document 1 (spécification de l'application), Document 3
(interface humaine), Document 4 (brief Banani).*
