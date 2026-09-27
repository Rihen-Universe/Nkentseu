# Le langage `.nkgui` — référence

### Document 2 — version du format : **0.4**

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Réécrit le 27/09/2026 à la demande de Rodolf (*« réécris tous les documents si tu
> le juges nécessaire, de 1 à 21 »*). La version 0.2 de ce document décrivait un
> langage **miroir de l'API** ; le format réel s'en est éloigné (vocabulaire du
> doc 7, grammaire 0.3 du doc 9, montage par NKGui, événements, composants,
> `include`). Ce document décrit le langage **tel qu'il est**, et **tel qu'il est
> décidé** pour la refonte.
>
> **Autorité.** Pour le **sens** du langage (ce qu'un mot veut dire, ce que l'hôte
> en fait), ce document fait foi sur tous les autres. Pour la **syntaxe exacte**, la
> grammaire complète est au **§3** de ce document ; le doc 9 garde la justification
> des décisions et leur historique. Pour le **vocabulaire des rôles**, le doc 7 et le
> validateur (`NkGuiValidate.h`) font foi, et le §8 ci-dessous en est la copie
> commentée.
>
> **Numérotation.** Les sections 1 à 12 gardent le numéro qu'elles avaient en 0.2 :
> le code les cite (`NkGuiInteraction.h` cite le §4 pour le script). Les nouveautés
> sont en §13 et au-delà.

---

## 0. Lire ce document

### 0.1 Les marques d'état

Chaque construction porte une marque. Elle dit **où en est le moteur**, pas ce que
le langage permet d'écrire : tout ce qui est écrit est **lu, gardé et réécrit à
l'identique**, même ce qui n'est pas encore exécuté.

| marque | sens |
|---|---|
| ✅ | **servi** : lu, validé, et l'hôte l'exécute ou le peint |
| ◐ | **compté** : lu, validé, gardé ; l'hôte ne le sert pas encore et **le compte** (un compteur nommé le dit) |
| 🟢 | **décidé** par Rodolf (27/09/2026), à implémenter — la syntaxe et le sens sont figés ici |
| 🔴 | **ouvert** : une question reste à trancher ; la question est écrite |

📌 **La règle qui tient le tout** : *ce qui est écrit voyage*. Un document peut
employer une construction ◐ ou 🟢 : il n'est pas faux, il **dit une intention** que
le moteur servira sans réécriture du fichier. Ce qui n'aurait pas été écrit serait
perdu.

### 0.2 Qui lit un `.nkgui`

| lecteur | ce qu'il fait | fichier |
|---|---|---|
| **l'archive** | lit le texte, garde tout (commentaires, lignes vides, style), réécrit à l'octet | `NKSerialization/NkGuiArchive` |
| **le validateur** | juge le document contre le vocabulaire ; ne refuse jamais tout le fichier en silence | `NKUIDesign/NkGuiValidate.h` |
| **le résolveur d'inclusions** | remplace chaque `include` par le contenu du fichier | `NKGui/Doc/NkGuiInclusions` |
| **l'expanseur de composants** | remplace chaque instance par l'arbre du composant | `NKGui/Doc/NkGuiComposants` |
| **le monteur** | pose les widgets dans NKGui, image par image, et compte ce qu'il ne sert pas | `NKGui/Doc/NkGuiMonteur.h` |
| **l'interaction** | résout les états, évalue les `behavior`, appelle les `Callback`, tient les `bind` | `NKGui/Doc/NkGuiInteraction.h`, `NkGuiEvenements.h` |
| **la coquille** | monte un ou plusieurs documents dans une application et relaie les événements | `NKGui/Doc/NkGuiCoquille.h` |
| **NKUIDesign** | ouvre, édite, simule, teste et enregistre les documents | l'application |

---

## 1. Vue d'ensemble

Un fichier `.nkgui` commence par sa version, puis ses inclusions, puis ses sections,
dans n'importe quel ordre sauf une contrainte : **un `include` prend la place exacte
où il est écrit** (§7), donc un fichier de composants s'inclut **avant** la section
qui les emploie.

| section | contenu | ce que l'hôte en fait | état |
|---|---|---|---|
| `widgets` | l'arbre des widgets, rôle par rôle | **monté** dans NKGui | ✅ |
| `behavior` | les comportements : script **ou Blueprint** (graphe) | script **exécuté** ; graphe lu et gardé, **exécution décidée** (compilé vers la même représentation, §6.2) | ✅ script · ◐ → 🟢 graphe |
| `component` | un composant réutilisable (un arbre à racine unique) | **expansé** avant le montage | ✅ |
| `geometry` | des formes (calques) derrière les widgets | `rect`, `ellipse`, `text` **peints** ; `image`, `path`, `frame` comptés | ✅ / ◐ |
| `geometry "nom"` | une **bibliothèque de formes** nommées, jamais peinte seule, que les widgets prennent comme surface (§15) | — | 🟢 |
| `controller` / `callback` | les contrats : signatures des callbacks, sans implémentation | validés ; la liaison se fait côté C++ | ✅ |
| `animation` | transitions, ambiances, effets continus | lue, **comptée**, non jouée | ◐ |
| `fonts` | déclarations de polices, repli, empreinte | lue, comptée | ◐ |
| `theme` | les **jetons** de l'application et leurs valeurs par thème (§20) | — | 🟢 |
| `application` | la **carte** : écrans, types, point de départ (§18) | — | 🟢 |
| `test` | des scénarios de test d'interaction (§19) | — | 🟢 |

Un document **sans** `widgets` est légitime : un fichier de composants, un thème, une
carte d'application, un fichier de tests.

### 1.1 Où vivent les fichiers d'une application

```
Resources/Interface/
├── Commun/                       ← partagé par toute la famille
│   ├── theme.nkgui               ← jetons et thèmes communs (§20)
│   ├── composants.nkgui          ← composants communs (§14)
│   └── panneaux/*.nkgui          ← panneaux communs, avec leurs Host d'extension
├── <Application>/
│   ├── application.nkgui         ← la carte (§18)
│   ├── theme.nkgui               ← les jetons propres à l'application (§20)
│   ├── composants.nkgui          ← ses composants
│   ├── <écran>.nkgui             ← un fichier par écran, fenêtre, dialogue, panneau
│   └── tests/*.tests.nkgui       ← ses scénarios de test (§19)
└── Themes/                       ← thèmes livrés ou installés (§20.6, §20.5)
```

Ce rangement est une **convention** (la carte `application` désigne les fichiers par
chemin, rien n'est deviné) ; NKUIDesign la propose à la création d'un projet.

---

## 2. Lexique

```
Identifier   := [A-Za-z_][A-Za-z0-9_]*
String       := '"' ( caractère | '\"' | '\\' | '\n' | '\t' | '\u' HEX{4} )* '"'
Number       := '-'? [0-9]+ ('.' [0-9]+)?
Color        := '#' HEX{6} | '#' HEX{8}                 // RGB ou RGBA
Vec2         := '(' Number ',' Number ')'
Token        := '@' Identifier ('.' Identifier)*        // un jeton de thème (§20)  🟢
List         := '[' ( value (',' value)* ','? )? ']'
Dict         := '{' ( Identifier '=' value )* '}'
LineComment  := '//' jusqu'à la fin de la ligne
BlockComment := '/*' … '*/'                              // ne s'imbrique pas
```

- **Sensible à la casse**, sauf les noms d'**événements** et d'**états**, comparés
  sans casse par le moteur (`click` = `Click`) ; NKUIDesign **écrit** toujours la
  forme canonique (`Click`, `Hover`).
- **Encodage UTF-8.** Les identifiants sont ASCII ; les chaînes et les commentaires
  portent n'importe quel texte (le français, le ghomala' et ses diacritiques
  combinants).
- **Commentaires et lignes vides sont des données** : l'archive les garde, attachés
  à la ligne voisine, et les réécrit à l'identique. Un éditeur qui les perd détruit
  la moitié de la valeur d'un fichier de la famille — ils sont écrits pour être lus.
- **Listes et dictionnaires** s'imbriquent (`[ { a = 1 }, { a = 2 } ]`). Une virgule
  finale dans une liste est un **avertissement** (`v2` du corpus), pas une erreur.
- **Mots réservés contextuels** : un mot-clé n'est un mot-clé **qu'à la place où la
  grammaire l'attend** (en tête d'un membre de bloc). `source = "x"` reste une
  propriété ordinaire bien que `source` soit un mot de la section `fonts`. La liste
  est au doc 9 §7 et au §3.10 ci-dessous.

---

## 3. Grammaire complète (EBNF)

C'est **la** grammaire normative. Les productions marquées 🟢 sont celles de la 0.4.

### 3.1 Fichier

```ebnf
file          := header include* section*
header        := "nkgui" Number '.' Number
include       := "include" String
section       := widgets_sec | behavior_sec | component_sec | geometry_sec
               | contract_sec | animation_sec | font_sec
               | theme_sec | application_sec | test_sec          (* 🟢 les trois dernières *)
```

### 3.2 Widgets

```ebnf
widgets_sec   := "widgets" '{' node_decl* '}'
node_decl     := Kind String? '{' node_member* '}'
Kind          := Identifier                    (* un rôle du §8, ou le nom d'un composant *)
node_member   := prop_decl | appearance_blk | node_decl | event_decl
prop_decl     := Identifier '=' value
value         := String | Number | Color | Vec2 | Token | List | Dict
               | flags | Identifier
flags         := Identifier ('|' Identifier)+
event_decl    := "on" Identifier ('(' Identifier (',' Identifier)* ')')? '->' action
action        := callback_call | "Behavior" String | '{' statement* '}'
```

📌 `event_decl` (`on Click -> …`) est **réservé** : lu et gardé, **pas exécuté**.
L'écriture servie est la parenthèse sur le comportement (§5.1). Les deux diront la
même chose le jour où les événements porteront des paramètres (§9.3).

### 3.3 Apparence

```ebnf
appearance_blk := "appearance" ('(' State ')')? '{' appearance_member* '}'
State          := "Normal" | "Hover" | "Pressed" | "Focus" | "FocusVisible" | "Disabled"
appearance_member := prop_decl | effect_blk
effect_blk     := effect_kind String? '{' prop_decl* '}'
effect_kind    := "fill" | "stroke" | "shadow" | "blur" | "text"      (* "text" : 🟢 0.4 *)
```

### 3.4 Comportement — script

```ebnf
behavior_sec  := "behavior" String ('(' Event ')')? '{' statement* '}'
               | "behavior" String ('(' Event ')')? "graph"
                   ( "function" '(' param_list? ')' ('->' Type)? | "macro" )?   (* 🟢 *)
                   '{' graph_body '}'
Event         := Identifier                      (* §9 *)

statement     := assign_stmt | if_stmt | call_stmt
               | ui_stmt                                            (* 🟢 §5.4 *)
assign_stmt   := "set" target '=' expr
target        := Identifier                                          (* une variable *)
               | widget_ref '.' Identifier                           (* 🟢 une propriété d'un widget *)
if_stmt       := "if" expr '{' statement* '}'
                 ("else" ( '{' statement* '}' | if_stmt ))?          (* else if : 🟢 *)
call_stmt     := callback_call
callback_call := "Callback" String '(' arg_list? ')'
arg_list      := expr (',' expr)*

(* 🟢 0.4 — les instructions d'interface, §5.4 *)
ui_stmt       := "open" String ("as" "modal")?
               | "close" String?
               | "back"
               | ("show" | "hide" | "toggle") widget_ref
               | "enable" widget_ref
               | "disable" widget_ref ("because" expr)?
               | "focus" widget_ref
               | "message" expr expr "buttons" List ('->' Identifier '{' statement* '}')?
               | "toast" expr
               | "emit" String '(' arg_list? ')'
               | "after" Number '{' statement* '}'
               | "call" String '(' arg_list? ')'
                   ('->' Identifier '{' statement* '}')?
                   ("else" Identifier '{' statement* '}')?
               | "theme" expr                                        (* §20.4 *)

widget_ref    := Identifier | String      (* String quand l'id contient un point : "instance.bouton" *)

expr          := or_expr
or_expr       := and_expr ("||" and_expr)*
and_expr      := not_expr ("&&" not_expr)*
not_expr      := ("not" | "!") not_expr | cmp_expr                   (* 🟢 *)
cmp_expr      := add_expr (cmp_op add_expr)?
cmp_op        := "==" | "!=" | "<" | "<=" | ">" | ">="                 (* "!=" : 🟢 *)
add_expr      := mul_expr (('+' | '-') mul_expr)*
mul_expr      := unary (('*' | '/') unary)*
unary         := '-' unary | primary
primary       := literal | Identifier | path | widget_field | Token
               | func_call | '(' expr ')'
path          := Identifier ('.' Identifier)+                         (* un chemin de modèle, Enum.X *)
widget_field  := widget_ref '.' Field                                 (* §5.3 *)
Field         := "value" | "checked"                                  (* ✅ *)
               | "text" | "selected" | "visible" | "enabled"
               | "focused" | "count"                                  (* 🟢 *)
func_call     := ("empty" | "length" | "matches" | "contains") '(' arg_list ')'   (* 🟢 *)
literal       := String | Number | Color | "true" | "false"
```

📌 **Ambiguïté levée.** `a.b` sans guillemets est lu comme **champ de widget** si `b`
est un `Field` et que `a` est l'identifiant d'un widget du document ; sinon comme
**chemin de modèle** (`scene.lumiere.intensite`, `Enum.X`). Le résolveur coupe au
**dernier point** (`NkGuiExecution::Resoudre`). Un identifiant qui **contient** un
point (ids préfixés des composants, §14.3) s'écrit **entre guillemets** :
`"transport.lecture".enabled`.

### 3.5 Comportement — graphe

```ebnf
graph_body    := (node_stmt | wire_stmt)*
node_stmt     := "node" Identifier NodeType ('{' pin_init (',' pin_init)* '}')?
NodeType      := Identifier                                           (* §6.3 *)
pin_init      := Identifier '=' expr
wire_stmt     := "wire" pin_ref ('->' pin_ref)+
pin_ref       := Identifier '.' Identifier
```

### 3.6 Composants

```ebnf
component_sec := "component" String '{' widgets_sec behavior_sec* '}'
```

La section `widgets` d'un composant a **exactement une racine** (§14).

### 3.7 Géométrie

```ebnf
geometry_sec  := "geometry" String? '{' shape_decl* '}'                (* nom : 🟢 *)
shape_decl    := "shape" String '{' (prop_decl | shape_decl)* '}'     (* imbrication : 🟢, seulement sous kind = compound *)
```

### 3.8 Contrats, animation, polices

```ebnf
contract_sec  := "controller" String '{' callback_sig* '}'
               | callback_sig
callback_sig  := "callback" CallbackName '(' param_list? ')' '->' Type
CallbackName  := Identifier ('.' Identifier)*                      (* le nom de l'action : fiche.enregistrer *)
param_list    := param (',' param)*
param         := Identifier ':' Type
Type          := "Void" | "Bool" | "Int" | "Float" | "String" | "Color" | "Vec2"
               | "Enum" '[' Identifier (',' Identifier)* ']'

animation_sec := "animation" String? '{' anim_decl* '}'
anim_decl     := ("transition" | "ambience") String '{' anim_member* '}'
               | "continuous" String '{' anim_member* '}'
anim_member   := "target" '=' String | prop_decl | track_blk | map_blk
track_blk     := "track" String '{' key_stmt* '}'
key_stmt      := "key" Number '->' value (',' "curve" '=' Identifier)?
map_blk       := "map" '{' prop_decl* '}'

font_sec      := "fonts" '{' font_decl* '}'
font_decl     := "font" String '{' font_member* '}'
font_member   := prop_decl | "source" '{' prop_decl* '}'
               | "fallback" '{' ("family" '=' String)* '}'
               | "metrics" '{' ( "glyph" String '->' Number | prop_decl )* '}'
```

### 3.9 Thème, application, tests — 🟢

```ebnf
theme_sec     := "theme" '{' theme_member* '}'
theme_member  := prop_decl                                   (* nom, auteur, version, extends, defaut… *)
               | "token" Token '{' prop_decl* '}'            (* déclaration : sens, type, famille *)
               | "variant" String '{' ( Token '=' value )* '}'   (* un thème : Sombre, Clair… *)

application_sec := "application" String '{' app_member* '}'
app_member    := prop_decl
               | "ecran" String '{' prop_decl* '}'
               | "themes" '{' prop_decl* '}'                  (* §20.4 *)

test_sec      := "test" String '{' test_stmt* '}'
test_stmt     := "depart" String ('{' prop_decl* '}')?
               | "doublure" String '->' ( value | "echec" String )
               | "click" widget_ref | "doubleclick" widget_ref | "hover" widget_ref
               | "type" widget_ref expr | "select" widget_ref expr
               | "check" widget_ref | "uncheck" widget_ref
               | "key" String | "drag" widget_ref "to" widget_ref
               | "back" | "answer" String | "wait" Number
               | "platform" Identifier
               | "expect" expectation
expectation   := "not"? ( "ecran" String | "message" String | "toast" String
               | "visible" widget_ref | "hidden" widget_ref
               | "enabled" widget_ref | "disabled" widget_ref ("because" String)?
               | "focused" widget_ref
               | "called" String ('(' arg_list? ')')? ("times" Number)?
               | "emitted" String
               | expr )
```

### 3.10 Mots réservés

`nkgui include widgets behavior graph component geometry shape controller callback
animation fonts theme application test` (en tête de section) ;
`on set if else Callback Behavior node wire` (script, graphe) ; 🟢 `function macro` (graphe) ;
`appearance fill stroke shadow blur text` (apparence) ;
`transition ambience continuous track key map target` (animation) ;
`font source fallback metrics glyph family` (polices) ;
🟢 `open as modal close back show hide toggle enable disable because focus message
buttons toast emit after call not theme token variant ecran themes depart doublure
echec click doubleclick hover type select check uncheck drag to answer wait platform
expect called times emitted visible hidden enabled disabled focused`.

Tous **contextuels** (§2).

---

## 4. Section `widgets` — sémantique

> *Le code (`NkGuiInteraction.h`) cite « document 2 §4 » pour la grammaire du script :
> elle y était en 0.2. Elle est désormais au **§3.4** (syntaxe) et au **§5** (sens).*

### 4.1 Un nœud

```nkgui
Button "anim.enregistrer_cle" {
  label   = "Clé"
  tooltip = "Enregistrer une clé sur les os sélectionnés (S)"
}
```

- `Kind` est un **rôle** du vocabulaire (§8) — ou le **nom d'un composant** déclaré
  dans le document ou un fichier inclus (§14).
- La chaîne qui suit est l'**identifiant** du widget. Il est **stable** : c'est lui que
  les comportements, les tests, les liaisons et l'application désignent. Le moteur le
  hache (FNV-1a, comme `NkGuiId`).
- **Règle de la famille : l'identifiant d'un élément interactif est le nom de son
  action** (`anim.enregistrer_cle`, `commun.parcourir`). Il se lit dans les journaux,
  dans les tests et dans le registre d'actions (doc 3 §24).
- **Unicité** : deux widgets de même identifiant dans le même document →
  `W-ID-DUPLICATE`. Dans un composant, les identifiants sont préfixés par
  l'instance (§14.3), ce qui rend l'unicité automatique.
- Un nœud **sans** identifiant est permis (conteneur anonyme) ; il ne peut être ni
  désigné ni testé.

### 4.2 Les propriétés

Chaque rôle a un **schéma** (§8) : une propriété hors du schéma, ou d'un mauvais
type, est `E-TYPE`. Le validateur lit **tout** le document, **puis** juge ; il ne
refuse jamais un fichier entier sans dire où et pourquoi.

**Les propriétés universelles** (tout rôle) :

| propriété | type | sens | état |
|---|---|---|---|
| `id` | chaîne | identifiant supplémentaire (rare : l'identifiant est normalement la chaîne du nœud) | ✅ |
| `tooltip` | chaîne | l'infobulle (une **propriété**, jamais un nœud — doc 3 §14quinquies) | ✅ |
| `enabled` | booléen | `false` = désactivé ; le comportement peut le changer (§5.4) | ✅ |
| `visible` | booléen | `false` = absent du rendu **et** du flux | ✅ |
| `pos` | Vec2, px | **pose** le widget à cette position dans son parent (voir §4.3) | ✅ |
| `size` | Vec2, px | taille ; une composante `≤ 0` = axe non contraint | ✅ |
| `sizeRel` | Vec2, fraction | fraction de la taille du parent, par axe ; gagne sur `size` sur son axe | ✅ conteneurs et `Host` |
| `minSize` / `maxSize` | Vec2, px | bornes appliquées **après** `size` / `sizeRel` | ✅ conteneurs et `Host` |
| `hitShape` | booléen | le clic suit la **forme** de la surface (§15.4) au lieu de la boîte | 🟢 |
| `shortcut` | chaîne | raccourci **affiché** par le widget (infobulle, comme à la ligne de menu) — **affichage seulement**, il ne déclenche rien (proposition P8) | 🔴 doc 7 |

⚠️ `pos` **est l'interrupteur**, `size` ne l'est pas : un widget qui écrit `pos` est
**posé** ; `size` seul reste le `size` **du rôle** (`Spacer "vide" { size = 12 }` vaut
un nombre, pas un Vec2 — le schéma du rôle est consulté **avant** les universelles).

⚠️ `sizeRel`, `minSize`, `maxSize` ne s'appliquent qu'aux **conteneurs** et aux `Host`.
Une feuille dans un flux garde sa taille naturelle (limite nommée, pas un oubli).

### 4.3 Placement : le flux ou l'absolu

- Par défaut, un conteneur **range** ses enfants : une `VBox` empile, une `HBox`
  aligne, une `Grid` quadrille, un `Flow` enroule.
- Les trois conteneurs **neutres** — `Window`, `Panel`, `Group` — acceptent
  `placement = absolute` : leurs enfants **directs** portent alors `pos`. Les autres
  conteneurs **nomment** déjà leur agencement ; leur donner `placement` est `E-TYPE`
  (une `VBox` absolue se contredirait).
- La racine de `widgets` est **absolue par nature** (rien ne la contient).
- Un `pos` sur l'enfant d'un conteneur en flux est `E-PLACEMENT`.

### 4.4 `bind` : la liaison à une donnée vivante

`bind = chemin` relie la valeur du widget à une donnée de l'application
(`bind = scene.lumiere.intensite`). L'application fournit les chemins ; l'interaction
tient la liaison dans les deux sens, image par image. En simulation, les chemins sont
servis par les **variables** de la simulation (doc 3 §18).

### 4.5 Les drapeaux

`flags = Resizable | Closable` : des noms combinés par `|`. Ils reprennent les
drapeaux du moteur (`NkGuiWindowFlags`, `NkGuiButtonFlags`, `NkGuiInputFlags`,
`NkGuiTableFlags`…) ; un drapeau inconnu est `E-VALEUR`.

---

## 5. Section `behavior` — le script

### 5.1 Quand un comportement s'exécute ✅

```nkgui
behavior "anim.boucle" { … }              // CONTINU : à chaque image
behavior "anim.boucle"(Changed) { … }     // quand la valeur de anim.boucle change
behavior "anim.sauver"(Click) { … }       // au clic sur anim.sauver
```

- Le **nom** d'un comportement avec événement est l'**identifiant du widget source** :
  `behavior "x"(Click)` s'exécute quand le widget `x` est cliqué.
- **Sans parenthèse**, le comportement est **continu** : l'hôte le passe **à chaque
  image**, après le montage. Il doit donc être **idempotent** : il **tient un état**
  (« Suivant n'est actif que si la case est cochée »), il ne **bascule** rien. Un
  `Callback` qui bascule dans un comportement continu est un clignotant à 60 Hz.
- **Avec parenthèse**, le comportement **répond** à un fait daté ; il peut basculer,
  ouvrir, afficher.
- Un événement **reconnu mais sans source** (§9) est gardé et **compté**
  (`evenementsSansSource`) ; un mot inconnu est **refusé** et compté à part.

📌 **La règle de conception** : la forme **continue** pour l'**état**, la forme **au
geste** pour l'**action**. Un bouton qui ne peut pas agir est **désactivé avec sa
raison** (forme continue), pas un bouton qui ne réagit pas au clic.

### 5.2 Les instructions de base ✅

| instruction | effet |
|---|---|
| `set nom = expr` | pose une **variable** du comportement (visible des autres comportements du document) |
| `if expr { … } else { … }` | branche ; `else if` 🟢 |
| `Callback "domaine.action"(args)` | appelle le **code de l'application** (le contrat, §10) ; en simulation, appel **journalisé**, sans effet |

`NkGuiEvaluateur` implémente exactement cela. Un comportement qui contient une
instruction qu'il ne connaît pas est **gardé entier et non exécuté** (compté) —
jamais exécuté à moitié.

### 5.3 Lire un widget dans une expression

| champ | type | pour | état |
|---|---|---|---|
| `x.value` | nombre | la valeur d'un curseur, d'un champ numérique | ✅ |
| `x.checked` | booléen | une case, un interrupteur | ✅ |
| `x.text` | chaîne | le contenu d'un champ, le libellé d'un bouton | 🟢 |
| `x.selected` | chaîne ou entier | l'élément choisi d'une liste, d'un menu déroulant, d'un groupe radio, d'onglets | 🟢 |
| `x.visible` · `x.enabled` · `x.focused` | booléen | l'état d'un autre élément | 🟢 |
| `x.count` | entier | le nombre d'éléments d'une liste, d'un arbre | 🟢 |

Un champ que le widget n'a pas (`"case".text` sur une `Checkbox`) est `E-CHAMP` ; un
widget introuvable est `E-WIDGET-INCONNU` (§12). Les deux sont signalés **à
l'écriture** dans NKUIDesign, pas au lancement.

**Fonctions de test** 🟢 : `empty(x)` (chaîne vide, liste vide), `length(x)`,
`matches(x, "motif")` (expression régulière simple : classes, `*`, `+`, `?`, ancres),
`contains(x, y)`.

**Opérateurs** : `+ - * /`, comparaisons, `&&`, `||` ✅ ; `!=`, `not` / `!`, moins
unaire 🟢. `+` sur deux chaînes concatène. Priorités (du plus faible au plus fort) :
`||`, `&&`, `not`, comparaison, `+ -`, `* /`, moins unaire.

### 5.4 Les instructions d'interface 🟢 (P25, décidé le 27/09)

Ce que l'interface **fait** en réponse. Chaque instruction est **additive** : un
document qui ne les emploie pas ne change pas de conduite.

| instruction | effet | exemple |
|---|---|---|
| `open "écran"` | ouvre un écran de la **carte** (§18) | `open "reglages"` |
| `open "écran" as modal` | l'ouvre en modal : ce qui est derrière ne répond plus | `open "confirmer_suppression" as modal` |
| `close` · `close "écran"` | ferme l'écran courant · l'écran nommé | `close` |
| `back` | revient à l'écran précédent de la pile de navigation | `back` |
| `show x` · `hide x` · `toggle x` | montre / cache un élément (`visible`) | `show "panneau_details"` |
| `enable x` | active un élément | `enable "envoyer"` |
| `disable x because "raison"` | le désactive **avec sa raison**, lue au survol (doc 3 §14quater) | `disable "envoyer" because "Formulaire incomplet"` |
| `set x.prop = expr` | règle une propriété d'**un autre widget** | `set "titre".text = "Enregistré"` |
| `focus x` | donne le focus au clavier | `focus "champ_nom"` |
| `message titre texte buttons [..] -> r { … }` | boîte de message ; `r` reçoit le **libellé** du bouton choisi | voir ci-dessous |
| `toast "texte"` | notification brève | `toast "Copié"` |
| `emit "événement"(args)` | émet un événement **de l'application**, que d'autres comportements écoutent par `behavior "événement"(Emitted)` 🟢 | `emit "projet.modifie"()` |
| `after ms { … }` | exécute plus tard | `after 1500 { hide "bandeau" }` |
| `call "service"(args) -> r { … } else e { … }` | appelle un **service du système** (fichier, horloge, réseau, presse-papiers…) ; `r` = résultat, `e` = raison de l'échec | `call "fichier.ouvrir"() -> chemin { … } else annule { … }` |
| `theme "Nom"` | change de thème (§20.4) | `theme "Rihen UE5 Clair"` |

```nkgui
behavior "projet.supprimer"(Click) {
  message "Supprimer ?" "Cette action est définitive." buttons ["Supprimer", "Annuler"] -> reponse {
    if reponse == "Supprimer" {
      Callback "projet.supprimer"()
      toast "Supprimé"
      back
    }
  }
}
```

**`Callback` et `call` ne sont pas la même chose.** `Callback` appelle le **code de
l'application** (déclaré au contrat, §10) ; `call` appelle un **service du système**
dont la liste est fermée (§5.6). En simulation, `Callback` est journalisé, `call`
est servi par une **doublure** qui peut réussir **ou échouer** (doc 3 §18bis).

**Qui exécute** : dans NKUIDesign, la simulation ; dans l'application, **le même
interpréteur NKGui**, la navigation étant servie par la coquille (`NkGuiCoquille`) à
partir de la **même carte**. *Ce qu'on a testé dans NKUIDesign est ce qui tourne.*

### 5.5 Les actions conditionnelles : agir sur un autre widget *seulement si*

```nkgui
// AU GESTE : la condition est évaluée quand on agit, et on dit pourquoi si elle échoue
behavior "fiche.enregistrer"(Click) {
  if empty("nom".text) {
    set "nom_erreur".text = "Le nom est obligatoire"
    show "nom_erreur"
    focus "nom"
  } else {
    hide "nom_erreur"
    open "confirmer_enregistrement" as modal
  }
}

// EN CONTINU : l'état suit la condition — idempotent
behavior "inscription.suivant" {
  if "conditions".checked && not empty("email".text) {
    enable "inscription.suivant"
  } else {
    disable "inscription.suivant" because "Accepte les conditions et saisis ton adresse"
  }
}
```

**Pièges signalés par le validateur et par NKUIDesign** :

| piège | code |
|---|---|
| la condition lit un widget qui n'existe pas | `E-WIDGET-INCONNU` |
| la condition lit un champ que le widget n'a pas | `E-CHAMP` |
| un comportement continu désactive sans `because` | `W-RAISON-ABSENTE` |
| deux comportements continus écrivent la même propriété du même widget | `W-ECRITURE-CONCURRENTE` (l'état clignote : le dernier écrit gagne à chaque image) |
| un `open` vers un écran absent de la carte | `E-ECRAN-INCONNU` |
| un `message` dont une réponse n'est traitée par aucune branche | `I-REPONSE-IGNOREE` (souvent voulu : « Annuler » ne fait rien) |
| une branche jamais atteinte par aucun test | `I-BRANCHE-NON-TESTEE` (rapport de couverture, §19) |

### 5.6 Les services système (liste fermée) 🟢

| service | résultat | échecs possibles |
|---|---|---|
| `fichier.ouvrir(filtres?)` | chemin | `annule` |
| `fichier.enregistrer(nom?, filtres?)` | chemin | `annule`, `refuse`, `plein` |
| `dossier.choisir()` | chemin | `annule` |
| `fichier.lire(chemin)` | texte | `introuvable`, `illisible` |
| `presse_papiers.lire()` · `presse_papiers.ecrire(t)` | texte · — | `vide` |
| `horloge.maintenant()` | date ISO | — |
| `reseau.disponible()` | booléen | — |
| `reseau.requete(url, options?)` | texte | `coupe`, `delai`, `refuse` |
| `systeme.ouvrir_lien(url)` | — | `refuse` |
| `systeme.notifier(titre, texte)` | — | `refuse` |
| `systeme.locale()` | langue et disposition du clavier | — |

Un service hors liste est `E-SERVICE-INCONNU`. La liste s'étend par le doc 2, jamais
par un document. Les **doublures** de la simulation (doc 3 §18bis) servent **ces
noms-là**, avec les mêmes échecs.

### 5.7 Les garanties d'exécution

1. **Jamais à moitié** : une instruction non comprise → le comportement entier est
   gardé et non exécuté, et compté.
2. **Ordre** : dans une image, les comportements **continus** passent après le
   montage, dans l'ordre du document ; puis les comportements **à événement**, dans
   l'ordre où les faits se sont produits.
3. **Pas de récursion** : un comportement ne déclenche pas d'événement dans la même
   image (`emit` et `open` prennent effet à l'image suivante). Il n'y a pas de
   boucle générale ; seul le graphe a `ForEachChild`.
4. **Les réponses asynchrones** (`message`, `call`, `after`) reprennent le
   comportement **dans son bloc de réponse**, jamais au milieu d'un autre.
5. **Une modale** suspend les événements des écrans qu'elle couvre ; leurs
   comportements **continus** continuent (ils tiennent un état).

---

## 6. Section `behavior … graph` — le Blueprint

Le Blueprint est la **seconde écriture** du comportement, de plein droit : ce qu'un
script dit, un graphe le dit, et l'inverse tant qu'on reste dans le sous-ensemble
commun (§6.6). Dans NKUIDesign, c'est la toile **Behavior** (doc 3 §9) ; dans le
fichier, c'est une section `behavior "nom" graph`.

### 6.1 Modèle

Un graphe est un ensemble de **nœuds** reliés par des **fils**. Deux familles de
broches :

- **exécution** (`exec`, `true`, `false`, `then1`…) : l'**ordre** ; un fil
  d'exécution part d'**une** sortie et peut arriver sur une entrée déjà reliée (deux
  chemins qui se rejoignent) ;
- **données**, typées (`Bool Int Float String Color Vec2 Enum List NodeRef Any`) : les
  **valeurs**, désignées `nœud.broche`. Une entrée de donnée non reliée prend sa
  **valeur écrite** (`pin_init`) ou la valeur par défaut de son type.

Un nœud **pur** (opérateurs, lectures) n'a pas de broche d'exécution : il est évalué
**à la demande**, quand un nœud d'exécution lit sa sortie, et **une seule fois par
exécution** du graphe (sa valeur est mise en cache jusqu'à la fin de l'exécution).

**Conversions implicites** permises : `Int → Float`, tout type → `String` (pour
`Toast`, `Log`), `Enum → Int`. Toute autre connexion entre types différents est
**refusée** à la pose du fil, avec la raison (doc 3 §9.3).

### 6.2 L'exécution 🟢 (décidé le 27/09)

- Aujourd'hui ◐ : un graphe est lu, gardé, **refusé à l'exécution** (`grapheIgnore`).
- **Décidé** : le graphe est **compilé** vers la représentation intermédiaire du
  script (§6.6) et exécuté par **le même évaluateur**, avec les mêmes garanties
  (§5.7). *Un Blueprint tourne dans l'application exactement comme le script
  équivalent* — pas d'interpréteur de graphe séparé, donc pas de divergence possible.
- Un graphe qui ne compile pas (fil manquant sur une entrée obligatoire, cycle de
  nœuds purs, type incompatible) n'est **pas exécuté** et le dit :
  `E-GRAPHE` + le nœud en cause.
- Les nœuds **personnalisés** d'un projet (§6.4) doivent se compiler vers la même
  représentation, ou déclarer qu'ils ne le peuvent pas (ils rendent alors le graphe
  « lecture seule côté code », §6.6).

### 6.3 Le catalogue

Couleur du **bandeau** par famille — la même que les Blueprints de NKCraft (UI_SPEC
§10ter.3) et de Nogee, pour qu'un utilisateur de la famille lise un graphe de
NKUIDesign sans réapprendre : **Événement** rose, **Action** ambre (elle *fait*),
**Donnée / calcul** sarcelle, **Flux** gris, **Interface** bleu (elle agit sur
l'écran), **Commentaire** sans bandeau.

**Événements** (point d'entrée, une sortie `exec`)

| nœud | sorties de données | état |
|---|---|---|
| `EventClick` `EventChanged` `EventHover` | `source:NodeRef` | ✅ source (une par événement du §9) |
| `EventDoubleClick` `EventContextMenu` `EventFocus` `EventBlur` `EventKeyDown` `EventWheel` `EventSubmit` `EventDragStart` `EventDrop` | `source` (+ paramètres le jour où ils existent, §9.3) | ◐ |
| `EventEmitted` | `name`, arguments | 🟢 |
| `EventOpened` · `EventClosed` | `screen` | 🟢 |
| `EventTick` | — le comportement **continu** (`behavior` sans parenthèse), chaque image — ce n'est **pas** un nom d'événement du §9 | 🟢 |

**Flux** (gris)

| nœud | entrées | sorties |
|---|---|---|
| `Branch` | `exec`, `cond:Bool` | `true`, `false` |
| `Sequence` | `exec` | `then1…thenN` (ajoutables) |
| `Switch` 🟢 | `exec`, `value:String\|Int` | une sortie par cas écrit, `default` |
| `ForEachChild` | `exec`, `parent:NodeRef` | `loopBody` (+ `child`), `completed` |
| `ForEachItem` 🟢 | `exec`, `list:List` | `loopBody` (+ `item`, `index`), `completed` |
| `DoOnce` 🟢 | `exec`, `reset` | `exec` (une seule fois jusqu'au `reset`) |
| `Gate` 🟢 | `exec`, `open`, `close` | `exec` (passe si ouvert) |
| `Delay` 🟢 | `exec`, `ms:Int` | `exec` |
| `Call` 🟢 | (`exec`), les paramètres de la fonction | (`exec`), le retour — appelle une **fonction** de graphe (§6.4) |
| `Return` 🟢 | (`exec`), `value` | — (fin d'une fonction) |

**Données et calcul** (sarcelle ; purs sauf mention)

| nœud | entrées | sortie |
|---|---|---|
| `GetVariable` · `SetVariable` (exec) | `name` (+ `value`) | `value` |
| `GetWidgetValue` | `target`, `field:Enum[value,checked,text,selected,visible,enabled,focused,count]` | `value` |
| `Literal` | — (valeur écrite) | `value` |
| `Add` `Subtract` `Multiply` `Divide` `Min` `Max` `Clamp` `Abs` | `a`, `b` (…) | `result` |
| `Compare` | `a`, `op`, `b` | `result:Bool` |
| `And` `Or` `Not` | `a`, (`b`) | `result:Bool` |
| `Select` 🟢 | `cond`, `a`, `b` | la valeur choisie (le « ? : » du graphe) |
| `IsEmpty` `Length` `Matches` `Contains` 🟢 | selon la fonction | `result` |
| `Concat` `Format` 🟢 | chaînes ; `Format` : `"{0} éléments"` + arguments | `result:String` |
| `ToString` `ToNumber` 🟢 | `value` | `result` |
| `Token` 🟢 | `name` (`@accent`) | la valeur du jeton dans le thème actif |

**Interface** (bleu ; exécution)

| nœud | entrées | sorties |
|---|---|---|
| `SetWidgetProperty` | `exec`, `target`, `prop`, `value` | `exec` |
| `Show` `Hide` `Toggle` `Enable` `Focus` 🟢 | `exec`, `target` | `exec` |
| `Disable` 🟢 | `exec`, `target`, `because:String` | `exec` |
| `OpenScreen` 🟢 | `exec`, `screen`, `modal:Bool` | `exec` |
| `CloseScreen` · `Back` 🟢 | `exec`, (`screen`) | `exec` |
| `ShowMessage` 🟢 | `exec`, `title`, `text`, `buttons:List` | **une sortie `exec` par bouton**, nommée par son libellé |
| `Toast` 🟢 | `exec`, `text` | `exec` |
| `SetTheme` 🟢 | `exec`, `name` | `exec` |

**Actions** (ambre ; exécution)

| nœud | entrées | sorties |
|---|---|---|
| `CallCallback` | `exec`, `name`, arguments typés **selon la signature déclarée** (§10) | `exec` |
| `CallService` 🟢 | `exec`, `name` (liste du §5.6), arguments | `success` (+ `result`), `failure` (+ `reason`) |
| `Emit` 🟢 | `exec`, `name`, arguments | `exec` |
| `Log` | `exec`, `message` | `exec` |

**Organisation** (sans bandeau)

| nœud | rôle |
|---|---|
| `Comment` | une note ; en **cadre**, il englobe des nœuds et se déplace avec eux |
| `Reroute` 🟢 | un point de passage d'un fil, pour le dessin ; sans effet à l'exécution |

### 6.4 Fonctions, macros et nœuds du projet 🟢

```nkgui
behavior "valider_email" graph function(email: String) -> Bool {
    node n1 Matches { a = email, pattern = "*@*.*" }
    node n2 Return  { value = n1.result }
}
```

- Une **fonction** est un graphe nommé à **paramètres** et **retour**, appelé par un
  nœud `Call "valider_email"` ; elle est **pure** si elle n'a pas de broche
  d'exécution. Elle se compile comme une fonction du script.
- Une **macro** (`graph macro`) est un morceau de graphe **déplié** là où on l'emploie ;
  elle peut avoir plusieurs sorties d'exécution (`Valide` / `Invalide`).
- **Réduire en fonction** (doc 3 §9.4) transforme une sélection de nœuds en fonction
  et la remplace par son appel — le geste qui garde les graphes lisibles.
- Les fonctions et macros se partagent **par `include`**, comme les composants
  (`Commun/blueprints.nkgui`).
- Un **nœud personnalisé** de projet ou de greffon déclare ses broches et **sa
  compilation** ; il porte le préfixe de son greffon.

### 6.5 Exemple

```nkgui
behavior "fiche.enregistrer"(Click) graph {
    node n1 EventClick
    node n2 GetWidgetValue { target = "nom", field = text }
    node n3 IsEmpty        { value = n2.value }
    node n4 Branch         { cond = n3.result }
    node n5 SetWidgetProperty { target = "nom_erreur", prop = "text", value = "Le nom est obligatoire" }
    node n6 Show           { target = "nom_erreur" }
    node n7 Focus          { target = "nom" }
    node n8 CallCallback   { name = "fiche.enregistrer", nom = n2.value }
    node n9 Toast          { text = "Fiche enregistrée" }
    node n10 CloseScreen   { }
    wire n1.exec -> n4.exec
    wire n4.true -> n5.exec -> n6.exec -> n7.exec
    wire n4.false -> n8.exec -> n9.exec -> n10.exec
}
```

C'est **exactement** le comportement `fiche.enregistrer` de l'exemple du §23, écrit en script.

### 6.6 Équivalence Code ⇔ Nœuds

Script et graphe compilent vers **la même représentation intermédiaire** : `Assign`,
`Branch`, `Call`, `BinaryOp`, et en 0.4 `Ui` (instructions du §5.4), `Await` (réponses
de `message`, `call`, `after`), `Loop` (`ForEachChild`, `ForEachItem`), `Func`.

- **Code → Nœuds** : toujours possible ; NKUIDesign **range** le graphe produit
  (disposition gauche → droite, fils d'exécution sur une ligne).
- **Nœuds → Code** : possible tant que le graphe ne contient **ni fil d'exécution qui
  remonte** (une boucle hors des nœuds de boucle), **ni nœud personnalisé sans
  compilation**, **ni `Gate` / `DoOnce`** (état caché entre deux exécutions, sans
  équivalent lisible en script). Au-delà, le graphe est la source et la vue Code est
  une **lecture seule générée**, signalée par un bandeau.
- **La disposition** des nœuds (positions, couleurs de commentaire, fils repliés)
  n'est pas du comportement : elle va dans le fichier voisin `.nkgui.meta`
  (doc 1 §4.1), jamais dans le `.nkgui`. Un graphe sans `.meta` se range
  automatiquement à l'ouverture.

---

## 7. Modularité — `include` ✅

```nkgui
nkgui 0.4
include "../Commun/theme.nkgui"
include "../Commun/composants.nkgui"
include "composants.nkgui"

widgets { … }
```

- Le chemin est **relatif au fichier qui inclut**.
- Le contenu inclus **prend la place exacte** de l'`include` : ses composants, ses
  jetons, ses contrats existent **à partir de là**. Un `include` écrit **après** la
  section qui emploie ses composants arrive trop tard.
- Un fichier inclus deux fois (diamant) n'est lu qu'**une fois**.
- Refus nommés : `introuvable`, `illisible`, `cycle` (avec la chaîne des fichiers).
  Le monteur refuse le document et **dit lequel** ; NKUIDesign montre la même chose
  **avant** le lancement, dans le graphe des inclusions (doc 3 §23).
- La version d'un fichier inclus peut être plus ancienne que celle de l'incluant ; pas
  plus récente en majeure (§21).

---

## 8. Les rôles

La table remplace la « correspondance avec l'API » de la 0.2 : le format a son
**propre vocabulaire** (doc 7), découplé des noms du moteur, pour qu'un document
écrit en 2026 s'ouvre encore quand le moteur aura tout renommé.

### 8.1 Les 41 rôles canoniques

Types : `s` chaîne · `b` booléen · `n` nombre · `v` Vec2 · `c` couleur (ou jeton) ·
`e` énumération / identifiant · `i` drapeaux · `l` liste · `r` référence de liaison ·
`a` quelconque.
**Monté** : ✅ le monteur le pose dans NKGui · ◐ lu, validé, compté
(`rolesNonMontes`), non posé.

**Actions**

| rôle | propriétés | monté | émet |
|---|---|---|---|
| `Button` | `label:s icon:e flags:i repeat:b repeatDelay:n repeatRate:n` | ✅ | Click, Hover |
| `ImageButton` | `image:e source:e size:a tint:c` | ◐ | (Click, Hover) |
| `MenuItem` | `label:s shortcut:s checked:b` | ✅ **dans un `Menu` seulement** (`elementsMenuHorsMenu` sinon) | Click |

**Saisie**

| rôle | propriétés | monté | émet |
|---|---|---|---|
| `TextField` | `bind:r multiline:b secret:b maxChars:n wrap:b placeholder:s flags:i valueType:e` | ✅ | Changed, Hover |
| `NumberField` | `bind:r valueType:e(Int, Float) step:n min:n max:n` | ◐ | (Changed) |
| `Slider` | `bind:r valueType:e min:n max:n step:n` | ✅ | Changed, Hover |
| `Drag` | `bind:r valueType:e speed:n min:n max:n dir:e` | ◐ | (Changed) |
| `ColorField` | `bind:r alpha:b mode:e(Button, Field, Picker)` | ◐ | (Changed) |

**Sélection**

| rôle | propriétés | monté | émet |
|---|---|---|---|
| `Checkbox` | `bind:r tristate:b label:s` | ✅ | Changed, Hover |
| `Switch` | `bind:r label:s` | ◐ | (Changed) |
| `RadioGroup` | `bind:r options:l orientation:e(Horizontal, Vertical)` | ◐ | (Changed) |
| `Dropdown` | `bind:r items:l editable:b` | ✅ | Changed, Hover |
| `ListBox` | `bind:r items:l multiSelect:b` | ✅ | Changed, Hover |
| `Item` | `label:s selected:b editable:b text:s` | ✅ | Hover (Click 🟢) |
| `TreeItem` | `label:s expanded:b editable:b` | ✅ (conteneur de `TreeItem`) | Hover (Click 🟢) |

**Affichage**

| rôle | propriétés | monté | émet |
|---|---|---|---|
| `Text` | `text:s wrap:b align:e(start, center, end, justify) maxLines:n overflow:e(ellipsis, clip, visible) for:s` | ✅ | — |
| `Image` | `source:e size:a tint:c uv0:v uv1:v texId:a` | ✅ | — |
| `Progress` | `bind:r overlay:s indeterminate:b` | ✅ | — |
| `Chart` | `values:l kind:e(Line, Histogram) min:n max:n height:n` | ✅ | — |
| `Separator` | `orientation:e(Horizontal, Vertical)` | ✅ | — |
| `Spacer` | `size:a` (un nombre dans un flux, un Vec2 sinon) | ✅ | — |

**Navigation**

| rôle | propriétés | monté | émet |
|---|---|---|---|
| `TabBar` | `tabs:l bind:r editable:b closable:b` | ✅ | Changed (si `bind`) |
| `MenuBar` | — | ✅ | — |
| `Menu` | `label:s` | ✅ | — |
| `ContextMenu` | — | ◐ (se lèvera avec l'événement ContextMenu) | — |
| `Expander` | `label:s expanded:b` | ✅ | Hover |
| `DockSpace` | `overViewport:b topMargin:n` | ✅ (zones nommées, §8.4) | — |

**Conteneurs**

| rôle | propriétés | monté | émet |
|---|---|---|---|
| `Window` | `title:s flags:i modal:b placement:e(flow, absolute)` | ✅ | — |
| `Panel` | `title:s placement:e` | ✅ | — |
| `Group` | `placement:e` | ✅ | — |
| `VBox` · `HBox` · `Row` · `Column` | `gap:n align:e justify:e` | ✅ (`Row` = `HBox`, `Column` = `VBox`) | — |
| `Grid` | `columns:n gap:n sizes:l` | ✅ (`sizes` compté, non honoré) | — |
| `Flow` | `gap:n` | ✅ | — |
| `Stack` | `anchor:e` | ◐ | — |
| `Table` | `columns:l flags:i` | ◐ | — |
| `Scroll` | `axis:e(X, Y, XY) always:b` | ✅ | — |
| `Splitter` | `bind:r min:n max:n orientation:e ratio:n` | ✅ | Changed (si `bind`) |
| `Host` | `hint:s` | ✅ (rectangle rempli par l'application, §8.3) | — |

**Les neuf rôles non montés** : `Stack`, `Table`, `ImageButton`, `NumberField`, `Drag`,
`ColorField`, `Switch`, `RadioGroup`, `ContextMenu`. Un document peut les employer :
ils voyagent et seront servis sans réécriture. NKUIDesign les **dessine** et marque
leur fidélité ◐ (doc 3 §22).

⚠️ **Le validateur ne vérifie pas encore les valeurs d'énumération** (il vérifie le
type). À ajouter : `E-VALEUR` sur une valeur hors liste.

### 8.2 Les alias

Un nom ancien reste **lu**, signalé `W-ROLE-ALIAS`, et réécrit sous son nom nouveau par
NKUIDesign **seulement si l'utilisateur le demande** (« Moderniser le document ») —
jamais en silence, pour ne pas changer un fichier écrit par quelqu'un d'autre.

| ancien | nouveau | ancien | nouveau |
|---|---|---|---|
| `RepeatButton`, `ButtonEx` | `Button` (`repeat = true`) | `Combo` | `Dropdown` |
| `InputText`, `InputTextEx`, `InputTextMultiline` | `TextField` | `BeginListBox` | `ListBox` |
| `InputInt`, `InputFloat` | `NumberField` | `CollapsingHeader` | `Expander` |
| `SliderFloat` | `Slider` | `Selectable`, `SelectableEditable`, `SelectItem` | `Item` |
| `DragFloat`, `DragInt` | `Drag` | `TreeNode`, `TreeNodeEditable` | `TreeItem` |
| `ColorEdit4`, `ColorPicker4`, `ColorButton` | `ColorField` | `ProgressBar` | `Progress` |
| `CheckboxTristate`, `CheckBox3` | `Checkbox` | `PlotLines`, `PlotHistogram` | `Chart` |
| `TabBarEx`, `TabBarEditable` | `TabBar` | `DockSpaceOverViewport` | `DockSpace` |

📌 **`RepeatButton` a une double vie**, et elle est écrite ici pour ne surprendre
personne : pour le **validateur** c'est un alias de `Button{repeat=true}` ; le
**monteur** le reconnaît encore comme rôle et le monte par son propre chemin (et
peint son `fill`). NKUIDesign **écrit** `Button { repeat = true }`.

### 8.3 `Host` — la zone que l'application remplit

`Host` déclare un **rectangle** que l'application remplit elle-même : un viseur 3D,
une toile, un éditeur de code, la bande des clés d'une timeline. *Le document dit OÙ
et QUOI ; l'application garde QUAND et COMMENT.*

- Un `Host` que personne ne remplit est dessiné **hachuré avec son nom** (ou son
  `hint`) et compté dans `hotesNonRemplis`.
- Le monteur demande le remplissage par un crochet (`NkGuiMonteHooks`) ; le compteur
  `hotes` dit combien ont été servis.
- Les **hôtes d'extension** : un panneau commun (`Commun/panneaux/`) déclare des
  `Host` que **chaque application** remplit à sa façon (le panneau Détails de la
  famille, par exemple, a un `Host "details.extension"`).

### 8.4 `DockSpace` — partage des responsabilités

- Le **document** dit quelles zones existent, leur ordre et leurs proportions par
  défaut ; les zones sont désignées par l'**identifiant** du panneau, jamais par son
  libellé.
- La **coquille** garde l'arbre vivant (ce que l'utilisateur a déplacé), et ne le
  réécrit **jamais** dans le document : c'est ce qui garde le document partageable.
- Une zone nommée mais non fournie est dessinée hachurée avec son nom ; un panneau
  fourni mais non nommé n'est pas touché.
- Largeur d'une zone : sa fraction, sinon le **reste** partagé.

### 8.5 Les rôles proposés

Les documents des applications de la famille proposent des rôles que NKGui ne porte
pas encore (P2 à P23, P28 : `ToggleButton`, `Badge`, `Tile`, `VectorField`,
`KeyDiamond`, `CurveField`, `TimecodeField`, `Meter`, `ColorWheel`, `AssetField`,
`KeyCaptureField`, `SplitButton`, `PieMenu`, `HoldButton`, `Stepper`, `TokenField`,
`Drawer`…). Leur fiche complète — propriétés, événements, **doublure** (l'écriture de
repli en rôles existants) et statut — est au **doc 7 §3.10**. Tant qu'un rôle proposé
n'est pas au vocabulaire, un document **écrit sa doublure** et un commentaire
`// P4 Tile` qui permettra de la remplacer mécaniquement.

---

## 9. Les événements

### 9.1 Le catalogue

| événement | source aujourd'hui | émis par | état |
|---|---|---|---|
| `Click` | le clic relâché sur le widget survolé et actif — **là où l'hôte tire l'action** | `Button`, `RepeatButton`, `MenuItem` ✅ ; `Item`, `TreeItem` 🟢 | ✅ |
| `Changed` | une valeur **liée (`bind`)** a changé depuis l'image précédente (jamais à la première image) | tout rôle qui porte un `bind` | ✅ |
| `Hover` | le pointeur est sur le widget | tout rôle **interactif** monté (y compris lignes de liste et vignettes) | ✅ |
| `DoubleClick` | — | `Item`, `TreeItem`, `Button` | ◐ |
| `ContextMenu` | — | tout widget | ◐ |
| `Focus` · `Blur` | — | champs | ◐ |
| `KeyDown` | — | widget focalisé | ◐ |
| `Wheel` | — | `Scroll`, `Host` | ◐ |
| `Submit` | — | `TextField` (Entrée) | ◐ |
| `DragStart` · `Drop` | — | éléments de liste, `Host` | ◐ |
| `Emitted` 🟢 | un `emit` d'un comportement | l'application (nom de l'événement émis) | 🟢 |
| `Opened` · `Closed` 🟢 | la navigation (§18) | un **écran** (nom de l'écran) | 🟢 |

Les événements ◐ sont **reconnus, validés et comptés** (`evenementsSansSource`) ; un
nom hors catalogue est **refusé** et compté à part. Les deux compteurs sont séparés
exprès : l'un dit « pas encore servi », l'autre « faute de frappe ».

### 9.2 Le nom du comportement

| comportement | écouté sur |
|---|---|
| `behavior "x"(Click)` | le widget d'identifiant `x` |
| `behavior "projet.modifie"(Emitted)` 🟢 | l'événement d'application `projet.modifie` |
| `behavior "reglages"(Opened)` 🟢 | l'écran `reglages` de la carte |

### 9.3 Les paramètres

Aucun événement ne transporte encore de paramètre : un comportement **lit** ce qu'il
lui faut (`x.value`, `x.text`). Le jour où un événement en transportera (la touche de
`KeyDown`, l'objet de `Drop`), il s'écrira `behavior "x"(KeyDown key)` et le
paramètre sera une variable du bloc ; l'écriture `on …` (§3.2) dira la même chose
depuis le widget. 🔴 *À trancher avec le premier événement à paramètre.*

---

## 10. Contrats — `controller` / `callback` ✅

```nkgui
controller "Reglages" {
    callback reglages.appliquer() -> Void
    callback reglages.opacite_changee(valeur: Float) -> Void
}
callback projet.supprimer() -> Void
```

- Un `Callback "x"(…)` référencé doit être **déclaré** dans le document ou un fichier
  inclus : sinon `E-CALLBACK-UNDECLARED`. C'est différent de « déclaré mais non lié en
  C++ » (`W-CALLBACK-UNBOUND`, avertissement d'exécution : l'appel est journalisé et
  ne fait rien).
- Les types des arguments sont vérifiés contre la signature.
- **Le registre d'actions** (doc 3 §24) **génère** les contrats d'une application à
  partir de ce qu'elle sert réellement : un contrat n'est jamais saisi à la main
  quand il peut être lu.
- 📌 Dans la famille, un callback porte **le nom de l'action** (`anim.enregistrer_cle`),
  et c'est aussi l'identifiant du widget qui la déclenche. Un seul nom partout.

---

## 11. Types

| type | C++ | littéral | notes |
|---|---|---|---|
| `Bool` | `bool` | `true` · `false` | |
| `Int` | `int32` | `12` | |
| `Float` | `float32` | `0.25` | |
| `String` | `NkString` | `"texte"` | |
| `Color` | `math::NkColor` | `#RRGGBB[AA]` ou `@jeton` | un jeton se résout par le thème actif (§20) |
| `Vec2` | `math::NkVec2` | `(x, y)` | |
| `Enum[...]` | `int32` ou enum générée | `Enum.X` | labels fixés à la déclaration |
| `List` 🟢 | `NkVector<…>` | `[a, b]` | dans une signature : `List[String]` |
| `Void` | — | — | le retour réel, s'il existe, est ignoré |

---

## 12. Diagnostics

Chaque diagnostic a un **code**, une **phrase** qui nomme l'élément et dit quoi faire,
et un **chemin** (`widgets/Window "principale"/Button "x"`). Sévérités : **E** erreur
(le document ne se monte pas ou l'export est bloqué), **W** avertissement, **I**
information.

### 12.1 Implémentés ✅

| code | quand |
|---|---|
| `E-PARSE` | syntaxe (le fichier ne se lit pas) — avec ligne et colonne |
| `E-VERSION-INCOMPATIBLE` | version majeure plus récente que le lecteur |
| `E-SECTION-INCONNUE` | section hors du format, dans un fichier de **notre** version (une version mineure plus récente la garde brute, §21) |
| `E-ROLE-INCONNU` | rôle hors vocabulaire et hors composants connus |
| `E-TYPE` | propriété hors schéma, ou valeur du mauvais type |
| `E-VALEUR` | valeur illisible pour son type (couleur à 5 chiffres…) |
| `E-PLACEMENT` | `pos` dans un conteneur en flux, ou `placement` sur un conteneur qui nomme son agencement |
| `E-EFFET-INCONNU` | bloc d'effet hors `fill stroke shadow blur` (+ `text` en 0.4) |
| `E-ETAT-INCONNU` | état hors des six (§13.2) |
| `E-FORME-INCONNUE` | nature de forme hors `rect ellipse text image path frame` (+ `compound` en 0.4) |
| `W-ROLE-ALIAS` | nom ancien (§8.2) |
| `W-ETAT-DOUBLE` | `appearance {}` et `appearance(Normal) {}` sur le même widget |
| `W-FOCUS-ANNEAU` | un `stroke` dans `appearance(Focus)` — voulais-tu `FocusVisible` ? |

### 12.2 Déclarés par le format, à brancher au validateur

| code | quand |
|---|---|
| `E-CALLBACK-UNDECLARED` | `Callback` sans signature |
| `W-CALLBACK-UNBOUND` | signature jamais liée en C++ (à l'exécution) |
| `W-ID-DUPLICATE` | deux widgets de même identifiant dans le même document |
| `W-ORPHAN-GEOMETRY` | forme d'une bibliothèque jamais employée |
| `E-INCLUDE-INTROUVABLE` · `E-INCLUDE-ILLISIBLE` · `E-INCLUDE-CYCLE` | le résolveur les produit déjà ; le validateur doit les rapporter avec leur chaîne |
| `E-COMPOSANT-RACINES` · `E-COMPOSANT-RECURSION` | les compteurs `racinesMultiples` et `recursions` existent ; en faire des diagnostics |
| `E-EVENEMENT-INCONNU` · `W-EVENEMENT-SANS-SOURCE` | idem (`evenementsSansSource`) |
| `W-THEME-ABANDONNE` · `I-SURCHARGE` | doc 9 §3.3 |
| `W-FONT-SUBSTITUEE` · `E-FONT-ICONES-REPLIEE` | doc 9 §5.2 |

### 12.3 Nouveaux en 0.4 🟢

| code | quand |
|---|---|
| `E-WIDGET-INCONNU` | une expression, une instruction ou un test désigne un widget absent |
| `E-CHAMP` | champ que le widget n'a pas (§5.3) |
| `E-ECRAN-INCONNU` | `open`, `close`, `depart` ou `expect ecran` vers un écran absent de la carte |
| `E-SERVICE-INCONNU` | `call` hors de la liste du §5.6 |
| `E-FORME-INTROUVABLE` | `appearance { shape = "x" }` sans forme `x` dans une bibliothèque |
| `E-OPERATION-BOOLEENNE` | `compound` sans `op`, ou avec moins de deux opérandes |
| `E-JETON-INCONNU` | `@nom` absent de tous les thèmes chargés |
| `E-JETON-SANS-SENS` | un jeton déclaré sans `sens` |
| `W-JETON-SANS-VALEUR` | un jeton sans valeur dans un des thèmes livrés (il retombera sur le défaut) |
| `W-CONTRASTE` | paire texte / fond sous le seuil AA dans un thème |
| `W-COULEUR-EN-DUR` | `#…` là où le projet exige des jetons (règle de contrôle, doc 3 §25) |
| `W-RAISON-ABSENTE` | `disable` continu sans `because` |
| `W-ECRITURE-CONCURRENTE` | deux comportements continus écrivent la même propriété |
| `E-TEST-SANS-ATTENTE` | un `test` sans aucun `expect` (il ne peut pas échouer) |
| `E-GRAPHE` | un Blueprint qui ne compile pas : entrée obligatoire non reliée, cycle de nœuds purs, types incompatibles — avec le nœud en cause |
| `E-FIL-TYPE` | un fil entre deux broches de types incompatibles (refusé dès la pose dans l'éditeur) |
| `I-REPONSE-IGNOREE` · `I-BRANCHE-NON-TESTEE` · `I-ECRAN-INATTEIGNABLE` | couverture |
| `E-ACTION-INCONNUE` | un identifiant interactif absent du **registre d'actions** de l'application, quand le projet en a un (doc 3 §24) |

---

## 13. L'apparence

### 13.1 Le bloc

L'apparence d'un widget est une **surcharge** du thème, jamais une seconde source de
style : ce qui n'est pas écrit vient du thème.

```nkgui
Button "anim.enregistrer_cle" {
  label = "Clé"
  appearance {
    radius = 4
    fill { color = @action.ecrit }
    text { color = @texte.sur_accent }
  }
  appearance(Hover)   { fill { color = @action.ecrit.survol } }
  appearance(Disabled){ fill { color = @fond.desactive } }
}
```

| bloc | propriétés | peint aujourd'hui |
|---|---|---|
| `appearance` | `opacity radius blend` · typographie : `font weight size lineHeight textAlign` · 🟢 `shape` (§15.4) | `radius` ✅ ; le reste ◐ (`apparencesNonPeintes`) |
| `fill` | `color gradient from to angle image fit` | `color` ✅ **sur `Button`, `RepeatButton`, `Panel`, `Window`** ; ailleurs et le reste ◐ |
| `text` 🟢 | `color` | ✅ **déjà peint par le monteur** ; manque au validateur (`E-EFFET-INCONNU` aujourd'hui) — à ajouter |
| `stroke` | `color width position dash` | ◐ |
| `shadow` | `offset blur spread color inner` | ◐ (se répète : l'ordre d'écriture est l'ordre de la pile) |
| `blur` | `radius backdrop` | ◐ |

Un bloc d'effet peut être **nommé** (`shadow "portee" { … }`) : le nom sert aux chemins
d'animation (`track "shadow.portee.blur"`).

### 13.2 Les six états, et leur priorité ✅

`Disabled > Pressed > Hover > FocusVisible > Focus > Normal`

- L'ordre **est** la priorité : **seul l'état gagnant s'applique**, pas de cumul
  (le cumul produirait un rendu que personne n'a dessiné). L'accumulation pourra
  être permise plus tard sans casser un document ; l'inverse les casserait.
- `appearance {}` et `appearance(Normal) {}` sont synonymes ; les deux sur le même
  widget → `W-ETAT-DOUBLE`.
- **Actif / sélectionné n'est pas un état d'apparence** : c'est une **donnée** du
  composant (`checked`, `selected`). Il se dessine par la donnée (un `Item` sélectionné
  prend `@nkgui.selection`), pas par un septième état. `Checked`, `Selected`,
  `ReadOnly`, `Error`, `Loading` sont exclus pour cette raison ; `Dragged` sera le
  premier ajouté, avec le glisser-déposer.
- **Où les états sont appliqués** : `NkGuiResoudreEtat` résout l'état ; il est peint là
  où NKGui expose le crochet de style (bouton, case, élément de liste, cible
  d'ancrage). Ailleurs, l'état est **compté** (`etatsNonAppliques`).

### 13.3 Les couleurs : un jeton plutôt qu'une valeur

Une couleur s'écrit `@jeton` (§20) ou `#RRGGBB[AA]`. Dans un projet qui exige des
jetons (règle de contrôle, doc 3 §25), une valeur en dur est `W-COULEUR-EN-DUR` —
**sauf** si un commentaire sur la ligne précédente dit pourquoi (`// en dur : …`) :
une couleur qui porte une information et doit tenir dans tous les thèmes peut l'être,
à condition que ce soit une décision écrite.

---

## 14. Les composants ✅

### 14.1 Déclarer

```nkgui
component "BoutonTransport" {
  widgets {
    Button "bouton" {
      label = "?"
      appearance { radius = 3 }
    }
  }
}
```

- **Une racine exactement** (`racinesMultiples` sinon — le composant est refusé).
- Un composant peut contenir des **instances d'autres composants**, jamais de lui-même,
  directement ou non (`recursions` : refusé, la chaîne est nommée).
- Un composant peut porter ses propres `behavior` (§14.4).
- Un fichier de composants **ne déclare aucune vue** : il s'inclut.

### 14.2 Instancier

```nkgui
include "composants.nkgui"
widgets {
  HBox "transport" {
    BoutonTransport "anim.lecture" { label = "▶"  tooltip = "Lecture (Espace)" }
    BoutonTransport "anim.arret"   { label = "■"  tooltip = "Arrêt" }
  }
}
```

- La racine du composant **prend l'identifiant de l'instance** — donc, dans la
  famille, **le nom de l'action**.
- Les propriétés écrites sur l'instance **l'emportent** sur celles de la racine
  (ce sont les **surcharges**). Une propriété absente du schéma de la racine est
  `E-TYPE`, comme sur un widget.
- L'expansion a lieu **avant** le montage : le monteur ne voit que des rôles.

### 14.3 Les identifiants des enfants

Les enfants de la racine reçoivent l'identifiant `<instance>.<original>` :
l'enfant `"icone"` de l'instance `"anim.lecture"` devient `"anim.lecture.icone"`. Un
comportement ou un test qui le désigne l'écrit **entre guillemets** (§3.4).

🔴 **Question ouverte (famille)** : quand l'**action** est portée par un **enfant** du
composant et non par sa racine, son identifiant préfixé (`transport.lecture`) n'est
plus le nom d'action attendu (`anim.lecture`). Deux réponses possibles : (a) l'action
est **toujours** portée par la racine (règle de conception des composants) ; (b) un
enfant peut déclarer `id = "anim.lecture"` qui **échappe** au préfixe. Recommandation :
**(a)** pour un composant qui porte **une** action (un bouton de transport, un bouton de
rail) — elle ne demande rien au moteur ; **(b)** pour un composant qui en porte
**plusieurs** (une carte avec « Aller à » et « Corriger ») — (a) y est impossible. Les
deux sont compatibles : (b) est la seule extension à décider.

### 14.4 Comportements d'un composant

Un `behavior` déclaré **dans** un composant est **instancié avec lui** : son nom est
préfixé comme les identifiants, et ses références aux enfants aussi. Il ne peut
désigner que des widgets **du composant**.

### 14.5 Emplacements (P9) 🔴

Un composant qui accueille des enfants de l'instance (une carte dont le corps est
libre) demande un **emplacement** : `Slot "corps" {}` dans le composant, et les
enfants de l'instance y vont. Proposé (P9, doc 7), non décidé.

### 14.6 Le modèle d'édition

NKUIDesign édite les composants avec son modèle (déclaration, instance, surcharge,
provenance, propagation — doc 15). Le passage de ce modèle au `.nkgui` est au
**doc 15 §15.16**.

---

## 15. La géométrie et les formes

### 15.1 Les calques ✅

```nkgui
geometry {
  shape "fond_haut" { kind = rect  pos = (0, 0)  size = (1440, 56)  color = @fond.entete }
  shape "pastille"  { kind = ellipse  pos = (12, 12)  size = (8, 8)  color = @etat.ok }
  shape "titre"     { kind = text  pos = (32, 18)  text = "Nkentseu"  color = @texte.principal }
}
```

Une forme de la section **sans nom** est un **calque** peint **derrière** les widgets,
à son rectangle absolu : ni rôle, ni état, ni événement. Propriétés : `kind`, `pos`,
`size`, `color`, `radius`, `text`. `rect`, `ellipse`, `text` sont peints ; `image`,
`path`, `frame` sont comptés (`formes` / `formesPeintes`).

### 15.2 La bibliothèque de formes 🟢 (P24, décidé le 27/09)

```nkgui
geometry "formes" {
  shape "onglet_biseaute" {
    kind = compound
    op   = subtract
    shape "base" { kind = rect  pos = (0, 0)  size = (1, 1)  radius = 4 }
    shape "coin" { kind = path  closed = true
                   anchors = [ { p = (0.85, 0) }, { p = (1, 0) }, { p = (1, 0.35) } ] }
  }
}
```

- Une section `geometry` **nommée** est une **bibliothèque** : ses formes ne sont
  **jamais peintes seules** ; elles servent de **surface** aux widgets (§15.4) ou
  d'**icônes** (§15.5).
- Dans une bibliothèque, les coordonnées sont des **fractions de la boîte** (0 à 1) :
  la forme suit la taille du widget. Les **rayons** restent en **pixels** — un coin
  arrondi ne se déforme pas quand le bouton s'élargit (c'est la réponse de la famille
  au problème du « 9-slice », doc 12 §12.3(b)).
- Une forme employée par aucun widget → `W-ORPHAN-GEOMETRY`.

### 15.3 Les tracés et les formes composées 🟢

**`kind = path`** : `anchors` (liste de dictionnaires), `closed` (booléen), `fillRule`
(`nonzero` | `evenodd`, défaut `nonzero`).

| clé d'une ancre | sens |
|---|---|
| `p` | position (Vec2) |
| `in` · `out` | poignées de Bézier **relatives** à `p` (absentes = segment droit) |
| `corner` | `Hard` · `Mirrored` · `Asymmetric` · `Free` (les quatre types de sommet de l'éditeur) |
| `radius` | rayon de l'angle en px, **seulement** si `corner = Hard` |

**`kind = compound`** : les **booléens**, non destructifs — un groupe qui porte une
opération.

| propriété | sens |
|---|---|
| `op` | `union` · `subtract` · `intersect` · `exclude` |
| enfants `shape` | les opérandes, **dans l'ordre** (pour `subtract` : le premier moins les suivants) ; au moins deux ; un opérande peut être lui-même `compound` |
| `fillRule` | `nonzero` · `evenodd` |

L'imbrication de `shape` n'est permise **que** sous un `compound`
(`E-OPERATION-BOOLEENNE` sinon). Aplatir un booléen en un tracé est un geste de
l'éditeur (destructif, et il le dit), jamais une écriture implicite.

### 15.4 Une forme comme surface d'un widget 🟢

```nkgui
Button "nav.onglet_scene" {
  label = "Scène"
  hitShape = true
  appearance { shape = "onglet_biseaute"  fill { color = @fond.onglet } }
  appearance(Hover) { fill { color = @fond.onglet.survol } }
}
```

- `appearance { shape = "nom" }` : la surface du widget (son `fill`, son `stroke`, son
  ombre) prend la forme au lieu du rectangle arrondi.
- `hitShape = true` : le clic et le survol **suivent la forme** — un bouton hexagonal ne
  se déclenche pas dans les coins de sa boîte.
- La forme peut changer **par état** (`appearance(Pressed) { shape = "…_enfonce" }`).
- **Ce qu'il faut au moteur** (et qui manque) : le remplissage de **contours
  multiples** non convexes (NKGui n'a que `AddConvexPolyFilled`) et le calcul des
  booléens — le triangulateur de NKUIDesign (`NkEarcut`) **descend** dans le noyau.
  D'ici là : `apparencesNonPeintes`, et la surface reste le rectangle arrondi.

### 15.5 Les icônes maison 🟢

Une forme de bibliothèque sert d'**icône** : `Button { icon = @forme.maison }` ou
`Image { source = "formes#maison" }`. Un jeu d'icônes dessiné dans NKUIDesign n'a
**pas de licence à suivre** : c'est la voie de la famille pour remplacer les icônes
de provenance étrangère.

---

## 16. L'animation ◐

Trois familles, lues, validées, gardées, **non jouées** (le moteur d'animation du
runtime n'existe pas encore). NKUIDesign les édite et les **prévisualise** (doc 3
§9bis).

| famille | pour | propriétés | réduction du mouvement (défaut) |
|---|---|---|---|
| `transition` | passer d'un état à un autre | `on from to duration curve delay` + `track` | `shorten` |
| `ambience` | une boucle **attachée à un état** | `state` (**obligatoire**) `duration repeat direction delay jitter` + `track` | `stop` |
| `continuous` | une propriété **pilotée** par une source | `rest` (**obligatoire**) `smoothing` + `map { source sourceFrom sourceTo property targetFrom targetTo curve }` | `stop` (revient au repos) |

```nkgui
animation {
  transition "appui" {
    target = "anim.lecture"  on = Click  duration = 0.12  curve = EaseOut
    track "scale" { key 0.0 -> 1.0  key 1.0 -> 0.96, curve = EaseOut }
  }
  ambience "respiration" {
    target = "pastille_live"  state = Normal  duration = 2.4  repeat = 0  direction = alternate  jitter = 0.3
    track "opacity" { key 0.0 -> 1.0  key 1.0 -> 0.6 }
  }
  continuous "inclinaison" {
    target = "carte"  rest = 0.0  smoothing = 0.18
    map { source = PointerX sourceFrom = -1.0 sourceTo = 1.0 property = "rotation" targetFrom = -6.0 targetTo = 6.0 curve = Linear }
  }
}
```

- `state` d'une ambiance est l'un des **six états** du §13.2 (l'ancien nom `Idle` est
  `Normal`).
- `reduceMotion = stop | shorten | keep` ; `keep` seulement pour un mouvement **qui
  porte une information** (une pastille « en direct »).
- Les chemins animés sont des chemins de propriété : `"opacity"`, `"scale"`,
  `"fill.color"`, `"shadow.portee.blur"` (effets nommés).
- Codes : `E-AMBIENT-NO-STATE`, `E-DRIVEN-NO-REST`, `W-AMBIENT-COUNT` (trop
  d'ambiances sur une page), `W-AMBIENT-IN-PHASE` (N instances en phase — mettre du
  `jitter`), `W-REDUCE-KEEP`.

---

## 17. Les polices ◐

```nkgui
fonts {
  font "interface" {
    family = "Inter"  weight = 400  kind = text
    source { mode = embedded  path = "Fonts/Inter-Regular.ttf" }
    fallback { family = "Noto Sans"  family = "DejaVu Sans" }
    metrics { unitsPerEm = 2048  lineHeight = 1.21  ascent = 1984  descent = -494
              glyph "A" -> 1366  glyph "M" -> 1774  glyph "i" -> 569 }
  }
  font "icones" { family = "Rihen Icons"  kind = icons
                  source { mode = referenced  path = "Fonts/RihenIcons.ttf" } }
}
```

- `kind = icons` **refuse** de se replier sur une police de texte
  (`E-FONT-ICONES-REPLIEE`) : une icône remplacée par une lettre ment.
- L'**empreinte** (`metrics`) détecte une substitution silencieuse
  (`W-FONT-SUBSTITUEE`).
- `embedded` implique une licence : NKUIDesign **avertit** quand on le coche.
- Les caractères du ghomala' (diacritiques combinants) exigent une police capable
  de les composer : le contrôle de couverture est une règle du projet (doc 3 §25).

---

## 18. La carte de l'application 🟢 (P26, décidé le 27/09)

```nkgui
nkgui 0.4
application "NKScena" {
  plateforme = Bureau                   // Bureau | Web | Mobile — plusieurs : [Bureau, Mobile]
  depart     = "accueil"
  route_base = "/"                      // Web seulement

  ecran "accueil"    { document = "accueil.nkgui"    type = Fenetre }
  ecran "principale" { document = "principale.nkgui" type = Fenetre  route = "/projet" }
  ecran "rendu"      { document = "rendu.nkgui"      type = Fenetre }
  ecran "fin_seance" { document = "fin_seance.nkgui" type = Dialogue }
  ecran "reglages"   { document = "reglages.nkgui"   type = Feuille  plateformes = [Mobile] }

  themes { defaut = "Rihen UE5"  choix_utilisateur = true }       // §20.4
}
```

| clé | sens |
|---|---|
| `plateforme` | `Bureau`, `Web`, `Mobile`, ou une liste — l'interface de NKUIDesign **affiche** `Web` sous le libellé « Navigateur » ; la **valeur** écrite est toujours `Web` |
| `depart` | l'écran ouvert au lancement |
| `ecran "nom"` | un écran : `document` (chemin relatif à la carte), `type`, `route` (Web), `plateformes` (restreint un écran) |
| `type` | `Fenetre` (fenêtre ou page), `Dialogue` (modal par nature), `Feuille` (panneau glissant, Mobile), `Panneau` (s'ancre dans un `DockSpace`), `Superposition` (au-dessus, sans bloquer) |
| `themes` | les thèmes que l'application embarque et le choix laissé à l'utilisateur (§20.4) |

- Les instructions `open`, `close`, `back` (§5.4), les tests (`depart`,
  `expect ecran`) et la simulation désignent **ces noms**. Un nom absent →
  `E-ECRAN-INCONNU`. Un écran qu'aucun chemin ne peut ouvrir → `I-ECRAN-INATTEIGNABLE`.
- **La même carte** sert la simulation dans NKUIDesign **et** la navigation dans
  l'application (la coquille `NkGuiCoquille` monte les documents qu'elle désigne).
  Sans cela, la simulation cesserait de prouver quelque chose.
- Ce que `open`, `back`, `message` et `toast` **font** selon la plateforme (nouvelle
  fenêtre, route, pile d'écrans…) est défini au **doc 3 §18.6**.

---

## 19. Les tests d'interaction 🟢 (P27, décidé le 27/09)

Un test d'interface, c'est : **un geste** → **une réponse attendue** (un message
affiché, un autre widget qui change, un écran qui s'ouvre, un callback appelé). Les
tests vivent dans des fichiers `*.tests.nkgui` à côté de la carte.

```nkgui
nkgui 0.4
include "../application.nkgui"

test "Supprimer demande confirmation" {
  depart "principale"
  doublure "fichier.ouvrir" -> "/projets/exemple.nksc"
  click "projet.supprimer"
  expect message "Supprimer ?"
  answer "Annuler"
  expect ecran "principale"
  expect not called "projet.supprimer"
}

test "Enregistrer sans nom affiche l'erreur" {
  depart "fiche"
  type "nom" ""
  click "fiche.enregistrer"
  expect visible "nom_erreur"
  expect "nom_erreur".text == "Le nom est obligatoire"
  expect focused "nom"
  expect not ecran "confirmer_enregistrement"
}
```

| instruction | sens |
|---|---|
| `depart "écran" { var = valeur … }` | d'où part le test, avec un **état de départ** (valeurs de variables, de `bind`) |
| `platform Bureau\|Web\|Mobile` | la plateforme simulée (défaut : la première de la carte) |
| `doublure "service" -> valeur` · `-> echec "raison"` | ce que répondra un service du §5.6 |
| `click` `doubleclick` `hover` `type` `select` `check` `uncheck` `key` `drag … to …` `back` `wait` | les gestes |
| `answer "bouton"` | répondre à la boîte de message ouverte |
| `expect …` / `expect not …` | l'attendu : `ecran`, `message`, `toast`, `visible`, `hidden`, `enabled`, `disabled … because`, `focused`, `called "x"(args) times n`, `emitted`, ou une expression |

- Un test **sans** `expect` est `E-TEST-SANS-ATTENTE` : il ne peut pas échouer, donc il
  ne prouve rien.
- **Un test n'est une preuve que s'il peut échouer** : « Vérifier les tests » (doc 3
  §18ter) le relance **en retirant** l'élément attendu (le `message`, la branche du
  `if`) et exige qu'il rougisse.
- **Les mêmes tests tournent dans l'application réelle** (NKGui exécute les mêmes
  instructions ; les `Callback` y sont vrais) : ce qui passe dans NKUIDesign et casse
  dans l'application **désigne l'écart** entre les deux.
- Sans fenêtre, un test d'interface qui échoue **arrête la construction** (Jenga),
  comme le reste de la maison.

---

## 20. Les thèmes 🟢 (décidé par Rodolf le 27/09)

> *« On peut définir pour chaque application créée un système de thème : l'utilisateur
> décide lui-même de définir le thème de son application. Ils peuvent utiliser des
> thèmes existants, créer leur propre thème, et si possible faire que leur
> application intègre plusieurs thèmes au choix de leurs utilisateurs, ou intégrer
> des plugins pour le changement de thème. »*

### 20.1 Jetons et variantes

```nkgui
nkgui 0.4
theme {
  nom     = "Rihen UE5"
  auteur  = "Rihen"
  version = "1.0"
  defaut  = "Sombre"

  // ── la STRUCTURE : trois gris, une hiérarchie qui se lit sans effort ──
  token @fond.app     { type = Color  famille = structure  sens = "Fond de fenêtre : recule derrière tout" }
  token @fond.panneau { type = Color  famille = structure  sens = "Fond des panneaux : se détache du vide" }
  token @fond.entete  { type = Color  famille = structure  sens = "En-têtes, barres d'outils : ce qui structure se lit en premier" }
  token @accent       { type = Color  famille = etat       sens = "État de l'interface : actif, focus, bouton principal" }
  token @selection    { type = Color  famille = selection  sens = "Sélection dans la zone de travail (règle : bleu = interface, ambre = sélection)" }
  token @arrondi      { type = Number famille = geometrie  sens = "Arrondi des contrôles" }

  variant "Sombre" {
    @fond.app = #141414   @fond.panneau = #212121   @fond.entete = #2B2B2B
    @accent   = #1177D1   @selection    = #F2980E   @arrondi     = 2
  }
  variant "Clair" {
    @fond.app = #F5F5F5   @fond.panneau = #FFFFFF   @fond.entete = #EAEAEA
    @accent   = #0E5FA6   @selection    = #C97A08   @arrondi     = 2
  }
}
```

- Un **jeton** a un nom (`@famille.nom`), un **type** (`Color`, `Number`, `String`
  pour une police), une **famille**, et un **sens** — une phrase, **obligatoire**
  (`E-JETON-SANS-SENS`) : un jeton sans raison d'être finit en doublon.
- Une **variante** est un thème concret (Sombre, Clair, Contraste élevé…) : une
  valeur par jeton.
- Un jeton peut **renvoyer à un autre** (`@bouton.fond = @fond.entete`) ; un cycle est
  une erreur.

### 20.2 Les jetons du moteur : `@nkgui.*`

Le thème du moteur (`NkGuiTheme`) s'**énumère par nom** (`NkGuiThemeTokens`,
`NkGuiThemeColor`, `NkGuiThemeScalar`). Ses champs sont des jetons **réservés** :
`@nkgui.bgPrimary`, `@nkgui.panel`, `@nkgui.header`, `@nkgui.button`,
`@nkgui.buttonHover`, `@nkgui.buttonActive`, `@nkgui.border`, `@nkgui.text`,
`@nkgui.textDisabled`, `@nkgui.selection`, `@nkgui.accent`, `@nkgui.track`,
`@nkgui.tabBar`, `@nkgui.tab`, `@nkgui.tabHover`, `@nkgui.tabActive`,
`@nkgui.onAccent`, `@nkgui.card`, `@nkgui.rowHover`, `@nkgui.textMuted`,
`@nkgui.separator`, `@nkgui.scrollbar`, `@nkgui.scrollbarHover`, `@nkgui.scrim`,
`@nkgui.shadow`, `@nkgui.success`, `@nkgui.warning`, `@nkgui.danger`, `@nkgui.info`,
`@nkgui.rounding`, `@nkgui.roundingSmall`, `@nkgui.roundingLarge`,
`@nkgui.borderThickness`, `@nkgui.framePadX`, `@nkgui.framePadY`.

Une variante qui les pose **règle tout ce que le monteur peint par défaut** (un widget
sans `appearance`). Elle les pose en général **par renvoi** aux jetons de sens :
`@nkgui.panel = @fond.panneau`. C'est ainsi qu'un thème change toute l'application
sans toucher un seul document d'écran.

### 20.3 Hériter, composer, retomber

- `extends = "GitHub Pro"` : le thème **part** d'un autre et ne redéclare que ce qui
  change.
- Plusieurs sections `theme` (par `include`) se **fusionnent** : un fichier commun
  (`Commun/theme.nkgui`) et un fichier d'application (`NkAnima/theme.nkgui`, ses
  jetons `@origine.*`, `@info.*`).
- **Résolution d'un jeton**, dans l'ordre : la variante active → la variante par
  défaut du même thème → le thème parent (`extends`) → le thème du moteur. **Jeton par
  jeton** : un thème auquel il manque une couleur ne produit jamais une interface noir
  sur noir.
- Un jeton **inconnu** de tous les thèmes chargés : `E-JETON-INCONNU` à la validation,
  et à l'exécution la valeur du moteur pour le rôle concerné (jamais une couleur
  inventée).

### 20.4 Plusieurs thèmes au choix de l'utilisateur final

```nkgui
application "NkAnima" {
  …
  themes {
    embarques         = ["Rihen UE5", "GitHub Pro", "Contraste élevé"]
    defaut            = "Rihen UE5"
    variante          = systeme          // suit le réglage clair / sombre du système ; ou "Sombre"
    choix_utilisateur = true             // l'utilisateur final choisit dans ses préférences
    greffons          = true             // un greffon peut ajouter des thèmes (§20.5)
  }
}
```

- L'instruction `theme "Nom"` change de thème, `theme "Nom/Clair"` de thème et de
  variante, `theme "/Clair"` de variante seulement.
- Le choix de l'utilisateur est **retenu** par l'application (dans ses préférences,
  pas dans le document).
- Un changement de thème est **immédiat** : les jetons sont résolus à chaque image.

### 20.5 Les thèmes par greffon

Un greffon (`.nkgreffe`, doc 3 §20bis) peut **apporter des thèmes** à une application
qui l'autorise (`greffons = true`) :

- un thème de greffon est **un fichier de données** (`theme { … }`), jamais du code ;
  il ne demande donc aucune permission au-delà de « lecture du document » ;
- ses thèmes portent le **préfixe** du greffon (`lumen.Nuit`) ;
- il ne peut **ni retirer ni renommer** un jeton de l'application ; il ne peut que lui
  donner des valeurs ;
- désinstaller le greffon ramène au thème par défaut, sans rien casser.

### 20.6 Les thèmes livrés

| thème | variantes | pour |
|---|---|---|
| **Rihen UE5** | Sombre (défaut), Clair | **le thème des applications de la famille, NKUIDesign compris** (décision de Rodolf, 27/09 : le style Unreal Engine 5) — palette de NKCraft (UI_SPEC §10bis) |
| **GitHub Pro** | Sombre (« Dark Pro »), Clair (« Light Pro ») | l'ancien thème de NKUIDesign et de NkAntenne ; livré comme alternative |
| **Contraste élevé** | Sombre, Clair | accessibilité : contraste AAA, bordures visibles |

Les valeurs complètes sont au **doc 3 §2**.

---

## 21. Versions et compatibilité

| règle | sens |
|---|---|
| **(a)** la version est **réécrite telle que lue** | un fichier 0.2 enregistré reste 0.2 tant qu'il n'emploie rien de plus récent ; NKUIDesign monte la version **seulement** quand l'utilisateur emploie une construction plus récente, et le dit |
| **(b)** un lecteur récent lit tout fichier plus ancien | le corpus 0.2 passe 10 / 10 |
| **(c)** version **majeure** plus récente | `E-VERSION-INCOMPATIBLE` |
| **(d)** version **mineure** plus récente | les sections et les membres inconnus sont gardés **bruts** et réécrits à l'octet |
| **(e)** 🟢 un comportement contenant une instruction inconnue | gardé brut, **non exécuté**, compté — jamais exécuté à moitié |

📌 D'où la règle de conception de toute extension future : **une construction nouvelle
est un bloc délimité par des accolades**, ou une instruction dans un bloc qui l'est.
Toutes celles de la 0.4 la respectent.

**Journal des versions**

| version | ajouts |
|---|---|
| 0.2 | grammaire de base, `geometry`, `widgets`, `behavior`, contrats |
| 0.3 | vocabulaire du doc 7 ; `Text`, `Spacer` ; `appearance` et ses états ; `animation` ; `fonts` ; listes et dictionnaires ; chemins pointés ; `component` ; `Host` ; `sizeRel` / `minSize` / `maxSize` ; `placement` ; événements en parenthèse ; `include` résolu |
| **0.4** | **exécution des Blueprints** (compilés vers la représentation du script), fonctions et macros de graphe, nœuds d'interface ; `@jetons` et section `theme` ; effet `text` ; instructions d'interface ; champs `.text .selected .visible .enabled .focused .count` ; `!=`, `not`, fonctions de test, `else if` ; services système ; `geometry` nommée, `path`, `compound`, `shape`, `hitShape` ; section `application` ; section `test` ; événements `Emitted`, `Opened`, `Closed` |

---

## 22. Ce que l'hôte compte

Le monteur et la coquille tiennent un **rapport** par document. NKUIDesign affiche les
**mêmes** compteurs, **avant** le lancement (doc 3 §22). Un compteur non nul n'est pas
une erreur : c'est ce qui **arrivera**.

| compteur | compte |
|---|---|
| `rolesInconnus` | rôles hors vocabulaire |
| `rolesNonMontes` | rôles connus, non posés (§8.1) |
| `attributsNonHonores` | propriétés valides que le monteur n'applique pas (`Grid.sizes`…) |
| `apparencesNonPeintes` | blocs ou propriétés d'apparence lus, non peints |
| `etatsNonAppliques` | états d'apparence là où le crochet de style n'existe pas |
| `elementsMenuHorsMenu` | `MenuItem` hors d'un `Menu` |
| `hotes` · `hotesNonRemplis` | `Host` servis · réclamés sans réponse |
| `actionsInconnues` | identifiants interactifs que l'application ne sert pas |
| `racinesMultiples` · `recursions` | composants refusés |
| `evenementsSansSource` | événements reconnus, pas encore déclenchés |
| `taillesSansEquivalent` | modes de taille de l'éditeur sans équivalent au format (`Weight`, `Expand`…) |
| `formes` · `formesPeintes` | calques de `geometry` |
| `comportementsDeclenches` · `grapheIgnore` | comportements exécutés · graphes refusés |
| 🟢 `jetonsReplies` | jetons résolus par repli (§20.3) |
| 🟢 `instructionsInconnues` | comportements gardés bruts (§21 e) |

---

## 23. Exemple complet

```nkgui
nkgui 0.4
include "../Commun/theme.nkgui"
include "composants.nkgui"

controller "Fiche" {
  callback fiche.enregistrer(nom: String) -> Void
}

widgets {
  Window "fiche" {
    title = "Nouvelle fiche"
    size  = (420, 220)
    VBox "corps" {
      gap = 8
      Text "etiquette_nom" { text = "Nom"  for = "nom" }
      TextField "nom" { placeholder = "Ex. Kofi"  maxChars = 64 }
      Text "nom_erreur" { text = ""  visible = false
                          appearance { text { color = @info.erreur } } }
      HBox "actions" {
        gap = 8
        Spacer "pousse" { size = 1 }
        Button "fiche.annuler"     { label = "Annuler" }
        Button "fiche.enregistrer" { label = "Enregistrer"
                                     appearance { fill { color = @accent } } }
      }
    }
  }
}

behavior "fiche.enregistrer"(Click) {
  if empty("nom".text) {
    set "nom_erreur".text = "Le nom est obligatoire"
    show "nom_erreur"
    focus "nom"
  } else {
    Callback "fiche.enregistrer"("nom".text)
    toast "Fiche enregistrée"
    close
  }
}

behavior "fiche.annuler"(Click) { close }
```

---

*Documents liés : 1 (le produit), 3 (l'interface humaine de NKUIDesign), 5 (le
modèle et l'interface pour l'agent), 7 (le vocabulaire), 9 (la grammaire : décisions
et historique), 15 (le modèle des composants).*
