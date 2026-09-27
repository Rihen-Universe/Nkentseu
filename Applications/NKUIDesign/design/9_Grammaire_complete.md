# Document 9 — Grammaire `.nkgui` : les décisions et leur histoire

> **Statut au 27/09/2026 — mis à jour lors de la réécriture des documents 1 à 21.**
> Ce document **n'est plus une proposition** : ses décisions sont implémentées (format
> 0.3, §8) ou décidées (format 0.4, §9). La **grammaire normative complète** est
> désormais **au document 2 §3**, réécrit le 27/09 — une seule source, pour qu'elle ne
> diverge pas. Ce document garde **pourquoi** chaque construction est comme elle est,
> les options écartées, et l'état d'implémentation. Les numéros de section sont
> inchangés (le code cite §3, §3.1, §3.2bis, §4.2, §6.1, §7).
>
> Corrections apportées le 27/09 : les exemples emploient le vocabulaire du doc 7
> (`TextField` au lieu de `InputText`) et le nom d'état décidé (`Normal` au lieu de
> `Idle`) ; §6.3 et §8.5 disent que `include` est résolu et que la grammaire du thème
> est décidée ; §9 est ajouté.

---

*En-tête d'origine (21/08/2026), conservé comme trace :*

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
| 6 | **quatre manques du document 2** relevés en écrivant le parseur | ✅ fermés le 27/09 (format 0.4) |

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
        TextField "champ_email" {
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

### 3.2bis ✅ Les noms des ÉTATS — **tranché le 2026-08-27**

> # `Disabled` > `Pressed` > `Hover` > `FocusVisible` > `Focus` > `Normal`
>
> **Six noms, et l'ordre EST la priorité.** Quand plusieurs états sont vrais en
> même temps, celui de gauche gagne et **lui seul s'applique**.
>
> **Décision de Rodolf**, prise sur le relevé ci-dessous. Elle retient les quatre
> noms proposés, **ajoute `Normal` explicite** (le repos porte un nom au lieu
> d'être implicite, pour la lisibilité) et **ajoute `FocusVisible`**. `Idle` reste
> écarté pour la raison mesurée — **aucun des huit outils ne l'emploie**.

#### ⚠️ Pourquoi une PRIORITÉ et non un cumul

> **Le cumul permet de produire un rendu que PERSONNE n'a dessiné** — la somme
> accidentelle de deux règles, que l'utilisateur final voit sans qu'aucun
> concepteur ne l'ait validée. **Avec la priorité fixe, ce qui s'affiche est
> toujours quelque chose que quelqu'un a choisi.**

Et le choix reste ouvert **dans un seul sens**, ce qui est le point : la priorité
fixe est un **sous-ensemble strict** du cumul. Passer au cumul plus tard, avec
l'ordre de cascade égal à cet ordre-ci, laisse les documents existants rendre
**à l'identique**. L'inverse — restreindre un cumul en priorité — casserait des
documents. **On peut élargir plus tard ; on ne pourra pas restreindre.**

⚠️ **L'ordre est tenu par un contrôle, pas par ce paragraphe** (26h). Une table
qu'on réordonnerait « pour ranger » changerait ce qui s'affiche à l'écran, en
silence et sans qu'aucun fichier ne bouge — mesuré par la mutation F1 : **77 / 78,
et 26h est le seul contrôle rouge.**

#### ⚠️ Pourquoi DEUX noms de focus, alors qu'un seul serait plus simple

> **CSS a vécu des années avec un seul `:focus` et a dû ajouter
> `:focus-visible`.** Un écosystème mature a essayé un seul nom et n'a pas pu s'y
> tenir.

Les deux besoins sont réels et distincts : un **champ de texte** doit se voir
focalisé **quelle que soit l'origine** ; un **bouton** ne doit montrer son anneau
**qu'au clavier**. « Clavier uniquement » rendrait le champ inhabillable ;
« toutes origines » sacrifierait l'accessibilité.

Le coût de la subtilité est réel — six noms dont deux voisins — et il est payé par
**`W-FOCUS-ANNEAU`** : un `stroke` posé sur `Focus` déclenche un avertissement qui
suggère `FocusVisible`. **La distinction cesse d'être un piège de documentation
pour devenir une question posée au bon moment.**

> 📌 **Le premier document que cet avertissement a attrapé était le nôtre** :
> `valides/10_etats_apparence.nkgui`, écrit deux heures plus tôt, posait l'anneau
> de son *bouton* sur `Focus`. Le fichier a été corrigé, et un champ de saisie y a
> été ajouté pour porter le cas légitime de `Focus`.

Seul `stroke` déclenche l'avertissement. Un anneau peut aussi se faire au `shadow`
(la technique du `box-shadow`), mais un `shadow` sur le focus est tout aussi
plausible comme simple mise en avant : **élargir ici fabriquerait un faux
positif**, exactement ce que le correctif d'apparence du 23/08 a dû défaire.

**Implémenté et mesuré** : le lecteur modélise `appearance(État)`, la validation
refuse ce qui n'est pas dans la liste (`E-ÉTAT-INCONNU`), et le contenu des blocs
à état est jugé comme celui des blocs nus. Contrôles 23e et 26a–26l.

⚠️ **Passer de cinq à six noms est un ÉLARGISSEMENT, donc la direction sûre** :
la liste accepte plus, jamais moins, et aucun document existant ne peut cesser
d'être lu. Mesuré plutôt que supposé — la famille 20, qui défend la compatibilité
ascendante et qui avait déjà rattrapé une erreur de conception ce jour-là, repasse
intacte.

#### ⚠️ `appearance { }` et `appearance(Normal) { }` — **synonymes**

`Normal` explicite a ouvert une question que la liste implicite évitait. La
réponse tient au relevé, pas au goût :

> **Chez tous les outils qui NOMMENT le repos — Unity (`normalColor`), Godot
> (`normal`), WPF (`Normal` dans `CommonStates`), Figma (`Default`) — le repos
> nommé EST le socle.** Aucun n'a *à la fois* un socle et un état de repos
> distincts. CSS n'a même pas de `:normal` : le sélecteur nu est la base.

Un « socle » séparé de « l'état de repos » serait donc une invention sans
précédent dans les huit outils — et toute la méthode de cette liste a été de ne
rien inventer.

**Mais la GRAPHIE appartient au fichier.** `$state` est *absente* de l'archive
quand le fichier n'écrit pas de parenthèses : le modèle **ne canonise pas**, et
l'écrivain réémet le lexème tel quel — espaces intérieurs compris
(`appearance(  Normal  )` revient à l'octet).

> ⚠️ **C'est la promesse centrale qui a décidé seule, exactement comme annoncé :**
> *un document lu et réécrit sans modification rend les mêmes octets.* Si les deux
> formes existaient et que l'écrivain n'en régénérait qu'une, l'aller-retour
> casserait pour **tous** les documents employant l'autre. Même raisonnement que
> `0.50` contre `0.5` : la valeur est une, la forme écrite appartient au fichier.
> Mesuré par la mutation N4 — l'écrivain qui régénère l'état au lieu de réémettre
> son lexème : **72 / 73, et le seul contrôle rouge est 26f.**

**Conséquence, et elle avait besoin de son diagnostic** : les cumuler sur un même
widget déclare deux fois le même état → **`W-ÉTAT-DOUBLE`**. Avertissement et non
erreur, délibérément : dire *laquelle des deux gagne* reviendrait à trancher la
question de la **combinaison**, qui reste ouverte.

#### ⚠️ La restriction que la fermeture a failli coûter

La première version acceptait `Ident ( Ident ) {` **partout**. Le contrôle 20 l'a
attrapée immédiatement : `futurMembre(x) { }` — le membre inconnu d'un fichier
0.4 — cessait d'être conservé verbatim. **On aurait inventé une structure pour une
construction qu'on ne connaît pas**, puis jugé son contenu contre un schéma qu'on
n'a pas : la règle (d) sacrifiée pour fermer une limite. Le §7 est net —
`appearance_blk := "appearance" ('(' state_ref ')')? …` — l'en-tête parenthésé
appartient à `appearance` et à lui seul. Contrôle 26g.

---

#### Le relevé qui a fondé la décision

`appearance(Hover)` a besoin d'une liste fermée. Doc 3 §12.2 les nomme en
français — *repos, survol, pressé, désactivé, focus* — et le format est en
anglais (doc 7 **R1**). La transposition évidente était
`Idle · Hover · Pressed · Disabled · Focus`. **Elle a été confrontée à ce qui se
fait ailleurs avant d'être figée**, parce qu'un nom d'état est un nom que les
utilisateurs apprendront et qu'ils arriveront ici en connaissant déjà un autre
outil.

Relevé sur huit outils (CSS/MDN, Figma, Unity uGUI **et** UI Toolkit, Qt QStyle
**et** feuilles de style, Flutter, SwiftUI, Blender, Godot) et quatre systèmes de
design (Material 3, Adobe Spectrum, eBay Playbook, SAP Fiori).

*Statut : **tranché et implémenté**. Le lecteur modélise l'état, la validation
le juge contre la liste fermée.*

#### Ce qui revient partout — le noyau dur

| concept | où, et sous quel nom |
|---|---|
| **survol** | `:hover` CSS, Qt, Unity UI Toolkit · `State_MouseOver` Qt · `Highlighted` Unity uGUI · `hovered` Flutter · `hover` Godot, Figma, Material, Spectrum · `MouseOver` WPF |
| **pressé** | `:active` CSS · `:pressed`/`State_Sunken` Qt · `Pressed` Unity uGUI, Godot, WPF, Figma · `pressed` Flutter, Material · `isPressed` SwiftUI · `UI_SELECT` Blender |
| **désactivé** | `:disabled` CSS, Qt, Unity UIT · `Disabled` Unity uGUI, Godot, WPF, Figma, Spectrum · `disabled` Flutter, Material |
| **focus** | `:focus` CSS, Qt, Unity UIT · `focus` Godot, Spectrum · `focused` Flutter, WPF · **absent** de Unity uGUI et du thème Blender |
| **repos** | `Normal` Unity uGUI, Godot, WPF · `Default` Figma, Spectrum · `Enabled` Material, eBay · **rien du tout** Flutter (`Set` vide) et CSS (sélecteur nu) |

#### La proposition d'alors : **quatre noms, et `Idle` disparaît**

> **`Hover` · `Pressed` · `Focus` · `Disabled`**
>
> *Rodolf a retenu les quatre et **ajouté `Normal`** — voir en tête de section.
> Le relevé lui-même désignait ce mot : « si un nom explicite est voulu malgré
> tout, `Normal` est le seul défendable ».*

#### Pourquoi chacun y est

- **`Hover`** — le nom le plus attesté du relevé, et de loin. `Highlighted`
  (Unity uGUI) est écarté : le même mot sert chez Blender au surlignage
  **clavier**, il mélange deux choses.
- **`Pressed`** — attesté chez Unity, Godot, WPF, Figma, Flutter, Material, Qt.
  Il dit sans ambiguïté ce que CSS appelle `:active`, et **c'est précisément
  pour éviter ce mot-là** (voir plus bas).
- **`Focus`** — sans `-ed` : `focus` (CSS, Qt, Unity UIT, Godot, Spectrum, eBay)
  l'emporte nettement sur `focused` (Flutter, WPF).
- **`Disabled`** — et il faut dire **pourquoi c'est le membre inconfortable de la
  liste** : ce n'est pas un état d'interaction. MDN le range avec `:checked` et
  `:invalid` dans les *« input pseudo-classes »*, pas avec `:hover`/`:active`
  dans les *« user action pseudo-classes »*. Material ne lui accorde **pas** de
  *state layer*. Il est ici quand même, parce que Unity, Godot, Qt, Figma,
  Flutter, WPF et Spectrum le stylent tous, et que **tout le monde cherchera
  `appearance(Disabled)` en premier**. C'est un choix d'usage assumé contre la
  taxinomie, pas un oubli.

#### Pourquoi `Idle` n'y est pas — et c'est le changement le plus net

1. **Le mot n'apparaît dans aucun des huit outils.** Pas un. C'est un mot de
   machine à états ou d'animation, pas de vocabulaire d'interface. Les seuls
   noms attestés pour le repos sont `Normal` (Unity, Godot, WPF) et `Default`
   (Figma, Spectrum) — et `Default` est un piège (voir plus bas).
2. **Le format sait déjà le dire.** `appearance { … }` sans argument **est** le
   repos. C'est exactement le choix de Flutter (le repos est l'ensemble vide) et
   de CSS (le sélecteur nu). Ajouter `Idle` créerait **deux façons d'écrire la
   même chose** — et deux façons d'écrire une chose divergent toujours : un
   fichier écrirait `appearance`, un autre `appearance(Idle)`, et il faudrait un
   jour décider laquelle gagne quand les deux sont présentes.

Si un nom explicite est voulu malgré tout, **`Normal`** est le seul défendable :
trois des outils que les utilisateurs de NkUI connaissent déjà l'emploient.

#### Pourquoi les autres n'y sont pas — les exclusions se justifient

**Écartés parce que le mot est ambigu :**

| mot | pourquoi |
|---|---|
| **`Active`** | **le mot le plus toxique du domaine — quatre sens documentés et incompatibles** : CSS `:active` = en cours de pression ; Qt `:active`/`State_Active` = *la fenêtre est active* ; eBay `Active` = la destination courante ; SwiftUI `ControlActiveState.active` = encore l'état de fenêtre. La page de Figma sur les états **se contredit elle-même** : elle dit « Active » dans le texte et sérialise `State=Pressed` dans le tableau. `Pressed` couvre le sens utile sans le piège. |
| **`Default`** | repos chez Figma/Spectrum/Material, mais **le bouton par défaut d'un dialogue** en CSS (`:default`), en Qt (`:default`) et chez Blender (`UI_BUT_ACTIVE_DEFAULT`). |
| **`Highlighted`** | survol chez Unity uGUI, surlignage clavier chez Blender. |

**Écartés parce que ce sont des DONNÉES du composant, pas des états d'interaction
— et c'est la vraie ligne de partage :**

`Checked` · `Selected` · `Indeterminate` · `ReadOnly` · `Error` · `Loading`

> ⚠️ **C'est ce mélange qui rend ces listes ingouvernables ailleurs.** Trois
> sources le disent, chacune à sa façon :
>
> - **CSS**, normativement : deux familles nommées différemment dans le même
>   standard — *user action* (`:hover`, `:active`, `:focus`) contre *input*
>   (`:checked`, `:disabled`, `:read-only`, `:indeterminate`, `:invalid`).
> - **Material 3**, par son mécanisme : la *state layer* n'existe que pour
>   quatre états — `hover` (0.08), `focus` (0.12), `pressed` (0.12),
>   `dragged` (0.16). `selected`, `activated`, `error` passent par autre chose.
> - **Blender**, structurellement : `ThemeWidgetStateColors` désigne
>   **exclusivement** l'état de la **donnée** (animée, sur une keyframe, pilotée
>   par un driver, surchargée, modifiée, en erreur). L'interaction n'y figure
>   pas du tout.
>
> **Et Qt montre le prix à payer quand on ne trace pas la ligne : ~45
> pseudo-états**, dont `:first`, `:middle`, `:only-one`, `:adjoins-item`,
> `:has-children` — qui ne sont pas des états mais des **positions dans une
> structure**.

Un état transitoire est **produit par le système d'entrée**, dure de zéro à
quelques centaines de millisecondes, et ne se sérialise pas. `Checked`,
`Selected`, `ReadOnly` sont **des propriétés que le programme fixe**, qui
persistent, et **que le document déclare déjà** (`tristate`, `enabled`, `bind`).
Les remettre dans `appearance(X)` dupliquerait une information qui vit ailleurs.

**Écartés parce que trop spécifiques à un outil ou à un domaine :**

`Visited` · `Link` · `Target` (CSS seul, propres à l'hypertexte) ·
`ScrolledUnder` (Flutter seul, c'est une relation de position) ·
`Inactive` (Unity UI Toolkit seul) · `Pending` (Spectrum seul).

**Le seul écarté qui reviendra frapper : `Dragged`.** Flutter le porte, Material
lui accorde une *state layer* à part entière (0.16), Spectrum et eBay l'ont
aussi. Il est absent de CSS, Qt, Unity et Godot. Il n'est pas dans la liste parce
que **NkUI n'a pas encore de glisser-déposer** — mais c'est **le premier à
ajouter** le jour où il en aura un, et il est bien un état d'interaction, pas une
donnée.

#### ⚠️ La question que cette liste NE règle PAS : la combinaison

Une liste fermée sans opérateur de combinaison force à inventer des noms
composés. **Godot en est la démonstration** : il a dû ajouter `hover_pressed`,
puis `font_hover_pressed_color`, puis les variantes `_mirrored` — onze StyleBox
et sept couleurs pour un seul bouton — et documenter une priorité *ad hoc*
(« disabled, hover et pressed priment sur focus »).

Quatre stratégies existent, et il faudra en choisir une :

| stratégie | qui | ce que ça coûte |
|---|---|---|
| chaînage + priorité | CSS (cascade), Qt (**ET logique explicite**, plus une négation `!` : `QPushButton:hover:!pressed`), Unity UI Toolkit | un moteur de priorité |
| noms composés | Godot (`hover_pressed`) | explosion combinatoire |
| une seule couche à la fois | Material 3 | simple, **et jamais tranché** — voir ci-dessous |
| groupes orthogonaux | WPF : `CommonStates` {Normal, MouseOver, Pressed, Disabled} **et** `FocusStates` {Focused, Unfocused}, un état actif dans chaque | il faut décider quelles dimensions sont orthogonales |

> ⚠️ **Material n'a jamais tranché.** Sa spécification dit à un endroit « When
> multiple states occur at once, such as selection and hover, **both** state
> indicators should be displayed » et à un autre « **only one** state layer is
> applied at a time ». Le ticket qui met les deux passages face à face
> (material-components-android #2003) a été **fermé sans réponse des
> mainteneurs**. Le système de design le plus documenté du monde laisse la
> question ouverte : `.nkgui` doit la trancher **explicitement**, pas en hériter.

**Piste** : le relevé penche vers les **groupes orthogonaux**. Godot dessine
`focus` **par-dessus** l'apparence de base — c'est-à-dire qu'il traite déjà le
focus comme orthogonal, mais par le rendu au lieu du vocabulaire. WPF le fait par
le vocabulaire. Les deux arrivent au même endroit.

#### Une seconde limite à nommer tout de suite : le focus clavier

CSS distingue `:focus`, `:focus-visible` (le focus doit être **rendu visible**,
typiquement au clavier et non à la souris) et `:focus-within`. Qt fait la même
distinction avec `State_KeyboardFocusChange`, Spectrum avec `focus` **et**
`keyboard-focus`. Un `Focus` unique fait perdre la distinction souris/clavier —
qui est aujourd'hui **une exigence d'accessibilité**, pas un raffinement.
`FocusVisible` est donc le deuxième nom probable, après `Dragged`.

#### Ce que ça a débloqué — fait le 2026-08-27

Fermer cette liste était **le seul verrou** devant `appearance(Hover)`. Il est
levé : l'en-tête parenthésé n'est plus une tranche verbatim, son contenu est jugé,
et **la validation n'est plus asymétrique**.

Le contrôle 23e, qui *figeait* l'asymétrie (« 1 diagnostic, pas 2 »), a été
**retourné** : il exige désormais **2** diagnostics, chacun avec sa ligne **et son
état dans le chemin**. Même fichier, même faute, chiffre inverse — c'est la mesure
de la fermeture, pas une nouvelle affirmation.

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
        state = Normal        // « Idle » dans la première version : le nom décidé est Normal (§3.2bis)
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

## 6. ✅ Quatre manques du document 2, relevés en écrivant le parseur — fermés en 0.4

> Ce ne sont pas des propositions. Ce sont des endroits où **la grammaire écrite
> ne suffit pas à lire les exemples que le document 2 donne lui-même**. Ils sont
> listés ici parce qu'un parseur qui les comblerait sans le dire inventerait le
> format à la place de Rodolf.

### 6.1 Il n'existe aucun littéral de LISTE, et cinq rôles en demandent un

> ✅ **Fermé le 27/09.** Le format 0.4 porte un **littéral de liste** (issue **(a)**)
> et un littéral de dictionnaire : `List` et `Dict` au lexique du doc 2 §2, dans `value`
> au doc 2 §3.2. Le texte qui suit est l'analyse d'origine.

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

> ✅ **Fermé le 27/09.** `expr` lit les chemins pointés (`path`) et les champs de
> widget (`widget_field`), et un identifiant qui contient un point s'écrit entre
> guillemets : doc 2 §3.4 et son « ambiguïté levée ». Le texte qui suit est l'analyse
> d'origine.

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

> ✅ **Fermé.** `include` est **résolu** depuis le 26/09 (`NkGuiInclusions` : chemin
> relatif, place exacte, refus nommés `introuvable` / `illisible` / `cycle`). La
> grammaire du thème est **décidée** le 27/09 (Rodolf : un système de thème par
> application, thèmes existants, thèmes propres, plusieurs thèmes au choix de
> l'utilisateur final, thèmes par greffon) : section `theme`, jetons `@nom`,
> variantes — doc 2 §20, et la justification au §9.1 ci-dessous. Le texte qui suit
> est l'analyse d'origine.

§7 donne `include "Theme.nkgui"`. **La grammaire d'un fichier de thème n'existe
nulle part** (relevé aussi par doc 3 §14ter.3). Un `include` qui résout vers un
fichier dont la forme n'est pas définie ne se vérifie pas.

Avec l'issue (a) de l'apparence — le thème donne les défauts — cette grammaire
devient nécessaire, et **elle n'est pas dans ce document** : c'est un cinquième
ajout, pas un des quatre demandés. `NkTheme` (NKEditorKit, 302 lignes) porte déjà
des **rôles de couleur nommés avec héritage et chargement** (doc 8 §3) — c'est de
là qu'il faut partir, pas d'une page blanche.

### 6.4 La validation par rôle n'a pas de table à interroger

> ✅ **Fermé le 27/09.** Le vocabulaire du doc 7 est adopté (question 1 de son §7) et
> `NkGuiValidate.h` valide contre lui depuis la 0.3 (41 rôles, `E-TYPE`) — doc 7, doc 2
> §8. Le texte qui suit est l'analyse d'origine.

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

> **Depuis le 27/09, la grammaire complète normative est au document 2 §3** (0.4
> comprise). Le récapitulatif ci-dessous est celui de la **0.3**, gardé tel quel parce
> qu'il est cité (`NkGuiArchive.cpp`, `NKSerialization/ROADMAP.md`). **La règle des
> mots réservés contextuels** qui le suit reste valable et s'étend aux mots de la 0.4
> (liste complète : doc 2 §3.10).

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
| 6.3 — grammaire du thème | ✅ **décidé le 27/09** | `include` ✅ **résolu** (26/09, `NkGuiInclusions`) ; section `theme` 🟢 à implémenter (doc 2 §20) |
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

1. ✅ **La grammaire du thème** (§6.3) — **décidée le 27/09** (§9.1, doc 2 §20) ;
   `include` est résolu depuis le 26/09.
2. ✅ **Les noms des états d'apparence** (§3.2bis) — **clos le 2026-08-27**, les
   trois décisions comprises :
   - la liste : `Disabled > Pressed > Hover > FocusVisible > Focus > Normal` ;
   - la **combinaison** : **priorité fixe**, pas cumul — *le cumul permet un
     rendu que personne n'a dessiné*. Élargissable au cumul plus tard sans
     casser un document ; l'inverse ne l'aurait pas été ;
   - le **focus clavier** : `Focus` **et** `FocusVisible`, parce que *CSS a
     essayé un seul nom et n'a pas pu s'y tenir*. Le coût est payé par
     `W-FOCUS-ANNEAU`.

   La limite `appearance(Hover)` est fermée avec.

   ⚠️ **Ce qui reste ouvert ici** : `Dragged` (Flutter, Material *state layer*
   0.16, Spectrum, eBay le portent ; NkUI n'a pas encore de glisser-déposer).
   C'est **le premier à ajouter** le jour où il en aura un — et l'ajouter sera
   un élargissement, donc sans risque pour les documents existants.
3. **La forme du chemin de propriété animée** (§4.3) — `"shadow.blur"` suppose
   qu'on sait désigner la propriété d'un effet empilé. Nommer les blocs d'effet
   (`shadow "portee" { … }`) est **implémenté** et évite l'indice ; rendre ce nom
   obligatoire dès qu'on anime un effet reste une décision de Rodolf.
4. **`W-THEME-ABANDONNE` et `I-SURCHARGE`** (§3.3) — les deux diagnostics de
   garde-fou sur les surcharges d'apparence ne sont pas encore comptés.

---


## 9. La version 0.4 — décidée le 27/09/2026, et pourquoi

> Rodolf, 27/09 : les extensions du format proposées pour la refonte de NKUIDesign
> sont **« décidées »** — booléens et `geometry` / `shape`, instructions d'interface,
> carte d'application, tests, conditions sur d'autres widgets ; puis, le même matin,
> le **système de thème par application**. La syntaxe est au doc 2 ; ici, les
> raisons et les options écartées. Rien n'est encore implémenté (🟢).

### 9.1 Les thèmes : une section, des jetons, des variantes

| choix | retenu | écarté, et pourquoi |
|---|---|---|
| où vit un thème | une **section `theme`** dans un `.nkgui`, incluse comme le reste | un format à part (`.nktheme`) : un second lecteur, une seconde validation, et le thème ne pourrait pas voyager avec `include` |
| la valeur d'une couleur | un **jeton** `@famille.nom` | une couleur nommée sans famille : 426 couleurs en dur mesurées chez les consommateurs NKGui (18/08) venaient justement de l'absence de rôles |
| la sémantique | **sens obligatoire** par jeton | un jeton sans raison d'être finit en doublon d'un autre — c'est ce qui est arrivé aux deux sarcelles de la palette avant l'UI_SPEC §10bis.3 |
| Sombre / Clair | des **variantes** d'un même thème | deux thèmes séparés : on ne saurait plus que « Rihen UE5 Clair » et « Rihen UE5 Sombre » partagent leurs jetons, et le contrôle « un jeton sans valeur dans une variante » deviendrait impossible |
| le lien au moteur | les champs de `NkGuiTheme` **sont** des jetons réservés `@nkgui.*` | une table de correspondance à maintenir : `NkGuiThemeTokens` énumère déjà le thème par nom, depuis le 18/08, exactement pour cela |
| le repli | **jeton par jeton**, jusqu'au thème du moteur | le repli par thème entier : un thème incomplet donnerait une interface noir sur noir (UI_SPEC §5, décision 4) |
| le choix de l'utilisateur final | déclaré par l'**application** (`themes { … }` de la carte) | déclaré par chaque document : un écran oublié resterait dans l'ancien thème |
| les greffons | **données seulement**, préfixées, sans pouvoir retirer un jeton | un greffon de code : une permission de plus pour changer des couleurs, et un risque qui n'apporte rien |

⚠️ **Deux homonymes à ne pas confondre** : `NkTheme` (NKEditorKit, thème des outils
d'édition) et `NkGuiTheme` (NKGui, thème du runtime). Les jetons `@nkgui.*` sont ceux
du **second**. Le premier devra, à terme, être **décrit** par la même section — c'est
le chantier M du doc 1 (NKUIDesign se décrit lui-même).

### 9.2 Les formes : une bibliothèque nommée, des fractions, des rayons en pixels

- **Pourquoi une section `geometry` nommée** plutôt qu'une section nouvelle : la
  section `geometry` existe, elle est montée, et une forme y a déjà sa syntaxe. La
  nommer est le plus petit ajout qui distingue « calque peint » de « forme de
  référence ». Le doc 3 avait déjà écarté par écrit l'idée d'une section à côté.
- **Pourquoi des fractions de la boîte** : une forme de surface suit la taille du
  widget. **Pourquoi les rayons en pixels** : un coin arrondi qui grossit quand le
  bouton s'élargit est exactement le défaut du « 9-slice » relevé au doc 12 §12.3(b).
  Fractions pour la silhouette, pixels pour les angles : la moitié du problème du
  9-slice est réglée sans découper l'image en neuf.
- **Pourquoi l'imbrication de `shape` seulement sous `compound`** : la garde du doc 9
  d'origine (« une forme n'a que des propriétés », `NkGuiValidate.h`) existait pour
  empêcher un second vocabulaire d'apparence. Un opérande de booléen n'est pas de
  l'apparence : c'est de la géométrie. La garde reste vraie partout ailleurs.
- **Pourquoi non destructif** : c'est la décision du doc 13 (« un groupe qui porte une
  opération est le modèle requis ») et celle de Figma, Lunacy et Sketch. Aplatir reste
  un geste, jamais une écriture implicite.

### 9.3 Les instructions d'interface : dans le script, pas dans une section

- **Pourquoi dans `behavior`** : l'évaluateur existe et implémente `set` / `if` /
  `Callback` entièrement ; les instructions nouvelles sont des `statement` de plus. Une
  section `navigation` séparée aurait coupé la condition de son effet.
- **Pourquoi `call` à côté de `Callback`** : l'un vise le **code de l'application**
  (contrat déclaré, lié en C++), l'autre un **service du système** (liste fermée, servie
  par une doublure en simulation). Les confondre aurait rendu impossible de simuler un
  dialogue « Ouvrir » **annulé** sans écrire de C++.
- **Pourquoi une liste fermée de services** : une doublure doit connaître d'avance les
  échecs possibles (`annule`, `coupe`…) pour les proposer. Un service libre n'aurait pas
  de doublure honnête.
- **Pourquoi `disable … because`** : le doc 3 §14quater exige qu'un élément désactivé
  dise pourquoi. Le mettre **dans l'instruction** rend l'oubli visible à la lecture et
  détectable par le validateur (`W-RAISON-ABSENTE`).
- **Pourquoi `emit` et `open` prennent effet à l'image suivante** : un comportement qui
  en déclenche un autre dans la même image ouvrirait la porte à la récursion, et la
  garantie « jamais à moitié » ne tiendrait plus.

### 9.4 Les conditions sur les autres widgets

- **Le point de départ est mesuré** : `NkGuiExecution::Resoudre` coupe au dernier point
  et ne connaît que `.value` et `.checked` — *« un champ que je ne connais pas ne se
  devine pas »*. Les six champs ajoutés sont ceux qu'une interface réelle interroge.
- **`!=` et `not`** : absents du doc 2 d'origine, et l'évaluateur a **choisi** de ne pas
  les ajouter en silence. Ils sont maintenant écrits d'abord — la règle de la maison.
- **Les guillemets pour les identifiants pointés** : les composants préfixent les
  identifiants (`instance.original`) ; sans guillemets, `transport.lecture.enabled`
  serait coupé au mauvais point.

### 9.5 La carte de l'application

- **Pourquoi un document** plutôt qu'un réglage de NKUIDesign : la **même** carte doit
  servir la simulation **et** l'application réelle (`NkGuiCoquille` monte déjà plusieurs
  documents). Une carte qui vivrait dans l'éditeur ne prouverait rien sur l'application.
- **Pourquoi des types d'écran** (`Fenetre`, `Dialogue`, `Feuille`, `Panneau`,
  `Superposition`) : `open` ne fait pas la même chose d'une plateforme à l'autre (doc 3
  §18.6), et c'est le type qui dit ce qu'il doit faire.

### 9.6 Les tests

- **Pourquoi une section du format** et non un script externe : les tests désignent les
  mêmes identifiants, écrans et services que le document ; le validateur peut donc
  vérifier qu'un test ne vise rien d'inexistant **avant** de le lancer.
- **Pourquoi `E-TEST-SANS-ATTENTE`** : un test qui ne peut pas échouer ne prouve rien —
  la même discipline que les recettes de la maison (« lancé une fois **sans** l'élément
  attendu, il doit rougir »).

### 9.6bis Les Blueprints exécutés

> Rodolf, 27/09 : *« n'oublie surtout pas les blueprints. »*

- **Pourquoi compiler plutôt qu'interpréter le graphe** : `NkGuiEvaluateur` implémente
  déjà entièrement le script ; un second interpréteur (de graphe) aurait sa propre
  sémantique des nœuds purs, des erreurs, de l'ordre — et le jour où les deux
  divergeraient, un comportement ne ferait pas la même chose selon la vue dans laquelle
  on l'a écrit. Compiler vers **la même représentation intermédiaire** rend cette
  divergence impossible par construction.
- **Pourquoi les nœuds purs sont évalués une fois par exécution** : c'est la règle des
  Blueprints d'Unreal, et c'est la seule qui rend un graphe prévisible (deux lectures
  d'un `GetWidgetValue` dans la même exécution rendent la même valeur).
- **Pourquoi `Gate` et `DoOnce` rendent la vue Code en lecture seule** : ils portent un
  état **caché** entre deux exécutions ; le traduire en script demanderait des variables
  inventées que l'utilisateur n'a pas écrites.
- **Pourquoi la disposition va au `.nkgui.meta`** : la position d'un nœud n'est pas du
  comportement ; l'écrire dans le `.nkgui` rendrait chaque déplacement de nœud visible
  dans le diff comme un changement d'interface.

### 9.7 Ce qui reste ouvert après la 0.4

1. Les **paramètres d'événement** (doc 2 §9.3) — avec le premier événement qui en porte.
2. L'**action portée par un enfant de composant** (doc 2 §14.3) — recommandation : (a) pour un composant à **une** action, (b) pour un composant à **plusieurs** (D12).
3. Les **emplacements** de composant (P9, doc 2 §14.5).
4. Le **raccourci propre à un widget** (P8, doc 2 §4.2).
5. `Dragged` (§3.2bis) — avec le glisser-déposer.
6. La forme du chemin d'effet animé (§4.3, point 3 du §8.5).

---

*Voir aussi : document 2 (la référence, grammaire complète §3), document 3 (les
décisions d'interface), document 7 (le vocabulaire), document 8 (l'inventaire de
l'existant), document 15 (le modèle des composants).*
