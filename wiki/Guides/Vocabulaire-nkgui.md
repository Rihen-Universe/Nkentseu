# Le vocabulaire complet de `.nkgui`

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
**Relevé dans le code le 26/09/2026. Version du format : `nkgui 0.3`.**

> ⚠️ **Deux vocabulaires, et ils ne coïncident pas.** Le **format** (ce que le
> validateur accepte) est plus large que le **monteur** (ce qui se peint
> réellement). Chaque tableau dit les deux, parce que la différence est la seule
> chose qui coûte cher : *un document que le format accepte et que le monteur ne
> monte pas ne se présente pas comme une erreur — il se présente comme un
> document plus pauvre.*

**Sources** : `Applications/NKUIDesign/src/NKUIDesign/NkGuiValidate.h` (le format) ·
`Kernel/Runtime/NKGui/src/NKGui/Doc/NkGuiMonteur.h` (le montage).

---

## 1. La forme d'un fichier

```
nkgui 0.3

// un commentaire

component "MonComposant" {      // 0..n
  widgets { ... }
}

widgets {                        // les vues
  Panel "racine" {
    Text "titre" { text = "Bonjour" }
  }
}

behavior "titre" { ... }         // 0..n
```

La première ligne déclare la version. Tout le reste est fait de **blocs** :
`Type "identifiant" { propriétés et blocs }`.

---

## 2. Les neuf sections

| section | rôle | **montée ?** |
|---|---|---|
| `widgets` | la vue : l'arbre des éléments | ✅ **montée** |
| `behavior` | ce qui s'exécute | 🟡 comptée ici, **exécutée par l'hôte** |
| `animation` | les animations | ❌ **comptée, jamais jouée** |
| `component` | un patron nommé, instanciable *(26/09)* | ✅ **développé avant le montage** |
| `geometry` | géométrie | ❌ ignorée |
| `controller` | le contrôleur (le C de MVC) | ❌ ignoré |
| `callback` | signatures de rappel | ❌ ignoré |
| `fonts` | les polices | ❌ ignorée |
| `include` | inclure un autre document | ❌ **déclaré, traité par personne** |

⚠️ **`include` est le cas le plus frappant** : il est au vocabulaire depuis
toujours et **rien ne le lit**. C'est le fil le plus court vers la composition
par fichiers, et il n'est pas tiré.

---

## 3. Les 41 rôles du format

**Légende** : ✅ monté et peint · ❌ accepté par le format, **non monté**

### Conteneurs

| rôle | propriétés | monté |
|---|---|---|
| `Window` | `title` `flags` `modal` `placement` | ✅ |
| `Panel` | `title` `placement` | ✅ |
| `Group` | `placement` | ✅ |
| `VBox` `HBox` | `gap` `align` `justify` | ✅ |
| `Row` `Column` | `gap` `align` `justify` | ✅ *(alias de HBox / VBox)* |
| `Grid` | `columns` `gap` `sizes` | ✅ ⚠️ `sizes` **non honoré** |
| `Flow` | `gap` | ✅ |
| `Scroll` | `axis` `always` | ✅ |
| `Splitter` | `bind` `min` `max` `orientation` `ratio` | ✅ |
| `DockSpace` | `overViewport` `topMargin` | ✅ |
| `Expander` | `label` `expanded` | ✅ |
| `Stack` | `anchor` | ❌ |
| `Table` | `columns` `flags` | ❌ |

### Commandes

| rôle | propriétés | monté |
|---|---|---|
| `Button` | `label` `icon` `flags` `repeat` `repeatDelay` `repeatRate` | ✅ |
| `RepeatButton` | *(hérite de Button)* | ⚠️ **monté, absent du format** |
| `ImageButton` | `image` `source` `size` `tint` | ❌ |

### Saisie

| rôle | propriétés | monté |
|---|---|---|
| `TextField` | `bind` `multiline` `secret` `maxChars` `wrap` `placeholder` `flags` `valueType` | ✅ |
| `Checkbox` | `bind` `tristate` `label` | ✅ |
| `Slider` | `bind` `valueType` `min` `max` `step` | ✅ |
| `Dropdown` | `bind` `items` `editable` | ✅ |
| `NumberField` | `bind` `valueType` `step` `min` `max` | ❌ |
| `Drag` | `bind` `valueType` `speed` `min` `max` `dir` | ❌ |
| `ColorField` | `bind` `alpha` `mode` | ❌ |
| `Switch` | `bind` `label` | ❌ |
| `RadioGroup` | `bind` `options` `orientation` | ❌ |

### Listes

| rôle | propriétés | monté |
|---|---|---|
| `ListBox` | `bind` `items` `multiSelect` | ✅ |
| `Item` | `label` `selected` `editable` `text` | ✅ |
| `TreeItem` | `label` `expanded` `editable` | ✅ |
| `TabBar` | `tabs` `bind` `editable` `closable` | ✅ |

### Menus

| rôle | propriétés | monté |
|---|---|---|
| `MenuBar` | *(aucune)* | ✅ |
| `Menu` | `label` | ✅ |
| `MenuItem` | `label` `shortcut` `checked` | ✅ |
| `ContextMenu` | *(aucune)* | ❌ |

⚠️ **`MenuItem` hors d'un menu est refusé et compté** (`elementsMenuHorsMenu`).
NKGui ne sait dessiner une entrée que dans un popup ouvert ; la poser dans le
flux en ferait un faux bouton.

### Affichage

| rôle | propriétés | monté |
|---|---|---|
| `Text` | `text` `wrap` `align` `maxLines` `overflow` `for` | ✅ |
| `Image` | `source` `size` `tint` `uv0` `uv1` `texId` | ✅ |
| `Progress` | `bind` `overlay` `indeterminate` | ✅ |
| `Chart` | `values` `kind` `min` `max` `height` | ✅ |
| `Separator` | `orientation` | ✅ |
| `Spacer` | `size` | ✅ |

### La zone hôte

| rôle | propriétés | monté |
|---|---|---|
| `Host` | `hint` | ✅ |

`Host` **n'est pas un widget**, et c'est tout son intérêt : il déclare un
**rectangle que l'application remplit** — un viseur 3D, une toile, une bande de
clés. Sans lui, un document ne peut décrire qu'un écran de démonstration.

⚠️ **Une zone que personne ne sert se couvre de hachures avec son nom dedans.**
Elle ne doit pas se faire prendre pour un fond.

---

## 4. Les 9 propriétés universelles

Admises sur **tout** rôle. Elles n'entrent pas au catalogue par rôle, sinon il
faudrait `ButtonWithTooltip`, `FieldWithTooltip`, *« et le catalogue doublerait
à chaque capacité ajoutée »*.

| propriété | type | sens |
|---|---|---|
| `tooltip` | chaîne | l'infobulle |
| `enabled` | booléen | `false` = grisé, visible et inactif |
| `visible` | booléen | |
| `id` | chaîne | |
| `pos` | Vec2 | **pixels absolus** — c'est l'interrupteur du placement |
| `size` | Vec2 | pixels absolus |
| `sizeRel` | Vec2 | fraction du parent : `0.16` = 16 % |
| `minSize` `maxSize` | Vec2 | bornes en pixels |

⚠️ **`pos` est l'interrupteur, `size` ne l'est pas.** Un widget qui écrit `pos`
est **posé** ; `size` seul reste le `size` **du rôle** — `Spacer { size = 12 }`
vaut un nombre, pas un Vec2. Le schéma du rôle est consulté **avant** les
universelles.

⚠️ **`pos` n'est lu que si le parent est absolu.** Sous un conteneur en flux, le
**validateur les refuse** (`E-PLACEMENT`) — elles ne sont pas ignorées en
silence.

📌 **`placement = absolute` n'existe que sur `Window`, `Panel` et `Group`.** Les
autres **nomment déjà** leur agencement : une `VBox` empile, une `Grid`
quadrille. Leur donner `placement` leur ferait dire le contraire de leur nom.

---

## 5. Les types de valeur

| code | type | exemple |
|---|---|---|
| `s` | chaîne | `label = "Jouer"` |
| `n` | nombre | `gap = 6` |
| `b` | booléen | `enabled = false` |
| `v` | Vec2 | `pos = (12, 40)` |
| `c` | couleur | `tint = #F79A28` |
| `l` | liste | `sizes = [90, 90]` |
| `e` | étiquette | `align = Center` *(identifiant ou chaîne)* |
| `r` | référence | `bind = anim.boucle` |
| `i` | drapeaux | `flags = ...` |
| `a` | n'importe quoi | |

---

## 6. Le design : `appearance`

```
Button "valider" {
  label = "Valider"
  appearance {
    radius = 4
    fill { color = #F79A28 }
    text { color = #10222B }
  }
}
```

| déclaration | peint ? |
|---|---|
| `fill { color }` | ✅ **sur `Button`, `RepeatButton`, `Panel`, `Window` uniquement** |
| `text { color }` | ✅ |
| `radius` | ✅ |
| `font = "..."` | ❌ **compté** — le monteur n'a qu'une seule fonte |
| `shadow { }` | ❌ **compté** |
| `stroke { }` | ❌ **compté** |
| `appearance(Hover)` et tout état hors repos | ❌ **compté `etatsNonAppliques`** |

⚠️ **Un `fill` sur un `Text`, un `Group` ou une `HBox` est compté, pas tenu** —
seuls quatre rôles ont une surface à remplir.

⚠️ **N'écris une couleur que si elle porte une information.** La couleur par
défaut vient du **thème** (GitHub Dark Pro / Light Pro). Une couleur en dur
reste **sombre en thème clair** : l'utilisateur verrait un bloc noir dans une
application blanche.

---

## 7. Le comportement : `behavior`

```
behavior "boucle" {
  if boucle.checked {
    Callback "anim.boucle_activee"()
  } else {
    Callback "anim.boucle_coupee"()
  }
}
```

L'identifiant du bloc est celui du **widget** auquel il se rattache. Ses
instructions sont conservées en **tranche de source verbatim** : le fichier fait
l'aller-retour à l'octet, mais le *sens* de ces constructions n'est pas modélisé
dans l'archive.

🔴 **LA LIMITE À CONNAÎTRE** : le format n'a **pas encore d'événement** — ni
`on Changed(...)`, ni `on Click(...)`. L'hôte passe **tous les `behavior` une
fois par image**, après le montage.

> **Conséquence : chaque appel doit être IDEMPOTENT.** Un `Callback` qui
> *bascule* un état serait un clignotant à 60 Hz.

📌 Et **la case devient la seule vérité** : tant que le document repose la valeur
à chaque image, un autre chemin qui la changerait serait écrasé au tour suivant.

---

## 8. Les composants *(26/09)*

```
component "BoutonTransport" {
  widgets {
    Button "bouton" {
      label = "?"
      appearance { radius = 3 }
    }
  }
  behavior "bouton" { ... }
}

widgets {
  HBox "barre" {
    BoutonTransport "anim.debut" { label = "|<" }
    BoutonTransport "anim.jouer" { label = "Jouer" }
  }
}
```

**Les règles, et chacune est refusée plutôt que devinée :**

- **Une seule racine**, sinon **refusé et compté**. Avec plusieurs racines, on ne
  saurait pas à laquelle donner l'identifiant de l'instance.
- **Les identifiants sont préfixés** : la racine prend celui de l'instance, ses
  descendants deviennent `<instance>.<original>`. Sans ça, deux instances
  partageraient la clé d'état du monteur.
- **Les attributs de l'instance l'emportent.** Sans surcharge, toutes les
  instances seraient identiques et on aurait renommé la copie-colle.
  L'identité (`$type`/`$id`) ne se surcharge pas.
- **Les `behavior` suivent**, leurs références `<id>.` réécrites par instance.
- **La récursion est refusée, pas limitée.** Une profondeur maximale ferait un
  document à moitié développé — un défaut silencieux.
- **Les définitions sortent du document développé** : un `widgets` de définition
  est un patron, pas une vue.

---

## 9. Ce que le format ne sait pas encore dire

| | |
|---|---|
| **les états** | écrits, comptés, **jamais peints** |
| **les animations** | la section voyage, **jamais jouée** |
| **les événements** | aucun `on Changed(...)` |
| **les transformations** | rotation, miroir, perspective, opacité, fusion : **aucun vocabulaire** |
| **`include`** | déclaré, **traité par personne** |

⚠️ **Écris-les quand même.** Ils voyagent dans le fichier et se comptent — donc
le jour où ils seront branchés, ton document s'anime **sans être réécrit**. Ce
qui n'aurait pas été écrit, lui, serait perdu.

---

## 10. Comment savoir ce qui n'a pas été tenu

Les compteurs, visibles dans la sonde de chaque application :

| compteur | ce qu'il dit |
|---|---|
| `rolesInconnus` | un nom hors vocabulaire — **le document est refusé** |
| `attributsNonHonores` | le monteur n'a pas d'équivalent (`Grid.sizes`) |
| `apparencesNonPeintes` | `font`, `shadow`, `stroke`, `fill` hors des 4 rôles |
| `etatsNonAppliques` | `appearance(Hover)` et consorts |
| `elementsMenuHorsMenu` | un `MenuItem` posé hors d'un menu |
| `hotes` | zones réclamées / zones servies |
| `actionsInconnues` | un bouton nomme une action que l'application ne sert pas |
| `racinesMultiples` `recursions` | composants refusés |

📌 **Rien n'est jeté en silence.** Un document rendu **autrement** que demandé
n'a pas l'air cassé — il a l'air normal. C'est le seul défaut qui ne se voit
jamais, et tous ces compteurs existent pour ça.
